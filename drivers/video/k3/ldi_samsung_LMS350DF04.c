/* Copyright (c) 2008-2011, Hisilicon Tech. Co., Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/pwm.h>
#include <linux/gpio.h>
#include <linux/leds.h>

#include <mach/gpio.h>
#include <mach/platform.h>
#include <mach/hisi_spi2.h>

#include "k3_fb.h"
#include "k3_fb_def.h"
#include "ldi_samsung_LMS350DF04.h"


#define PWM_LEVEL 100
#define TIME_PER_FRAME	16


#ifdef CONFIG_MACH_TC45MSU3
extern int CspiDataExchange(PCSPI_XCH_PKT_T pXchPkt);
static bool spiSendData(u8 *data, u32 length, u8 bitcount)
{
	CSPI_XCH_PKT_T xferpack;
	CSPI_BUSCONFIG_T cspiPara;

	cspiPara.bitcount = bitcount - 1;
	cspiPara.scr = 9;
	cspiPara.cpsdvsr = 2;
	cspiPara.burst_num = 3;
	cspiPara.spo = 1;
	cspiPara.sph = 1;

	xferpack.pBusCnfg = &cspiPara;
	xferpack.pTxBuf = data;
	xferpack.pRxBuf = NULL;
	xferpack.xchCnt = length;
	xferpack.xchEvent = NULL;

	xferpack.spiDevice  = CS1;  /* use LCD cs as test */

	if (CspiDataExchange(&xferpack) == -1) {
		pr_err("k3fb, %s-%d: CspiDataExchange failed!", __func__, __LINE__);
		return false;
	}

	return true;
}
#else
static bool spiSendData(u8 *data, u32 length, u8 bitcount)
{
	return true;
}
#endif

static void set_spi_cs(void)
{
	outp32(0xFCA09000, 0xFFFF003C);
}

static struct k3_fb_panel_data samsung_panel_data;

static int samsung_pwm_on(struct k3_fb_data_type *k3fd)
{
	BUG_ON(k3fd == NULL);

	/* backlight on */
	PWM_IOMUX_SET(&(k3fd->panel_info), NORMAL);
	PWM_GPIO_REQUEST(&(k3fd->panel_info));
	gpio_direction_input(k3fd->panel_info.gpio_pwm1);
	mdelay(10);
	pwm_set_backlight(k3fd->bl_level, &(k3fd->panel_info));

	return 0;
}

static int samsung_pwm_off(struct k3_fb_data_type *k3fd)
{
	BUG_ON(k3fd == NULL);

	/* backlight off */
	pwm_set_backlight(0, &(k3fd->panel_info));
	gpio_direction_output(k3fd->panel_info.gpio_pwm0, 0);
	mdelay(10);
	gpio_direction_input(k3fd->panel_info.gpio_pwm1);
	mdelay(10);
	PWM_GPIO_FREE(&(k3fd->panel_info));
	PWM_IOMUX_SET(&(k3fd->panel_info), LOWPOWER);

	return 0;
}

static void samsung_disp_on(struct k3_fb_data_type *k3fd)
{
	struct k3_panel_info *pinfo = NULL;

	BUG_ON(k3fd == NULL);
	pinfo = &(k3fd->panel_info);

	LCD_VCC_ENABLE(pinfo);
	LCD_IOMUX_SET(pinfo, NORMAL);
	LCD_GPIO_REQUEST(pinfo);
	gpio_direction_input(pinfo->gpio_lcd_id0);
	mdelay(10);
	gpio_direction_input(pinfo->gpio_lcd_id1);
	mdelay(10);
	gpio_direction_output(pinfo->gpio_reset, 1);
	mdelay(10);
	gpio_direction_output(pinfo->gpio_power, 1);
	mdelay(10);
	gpio_direction_output(pinfo->gpio_reset, 0);
	mdelay(10);
	gpio_direction_output(pinfo->gpio_reset, 1);
	mdelay(10);

	set_spi_cs();

	spiSendData(powerOnData1, sizeof(powerOnData1), 8);
	mdelay(10);
	spiSendData(powerOnData2, sizeof(powerOnData2), 8);
	mdelay(6 * TIME_PER_FRAME);
	spiSendData(powerOnData3, sizeof(powerOnData3), 8);
	mdelay(5 * TIME_PER_FRAME);
	spiSendData(powerOnData4, sizeof(powerOnData4), 8);
	mdelay(10);
	spiSendData(powerOnData5, sizeof(powerOnData5), 8);
	mdelay(2 * TIME_PER_FRAME);
	spiSendData(powerOnData6, sizeof(powerOnData6), 8);
	mdelay(2 * TIME_PER_FRAME);
	spiSendData(powerOnData7, sizeof(powerOnData7), 8);
}

static void samsung_disp_off(struct k3_fb_data_type *k3fd)
{
	struct k3_panel_info *pinfo = NULL;

	BUG_ON(k3fd == NULL);
	pinfo = &(k3fd->panel_info);

	spiSendData(powerOffData1, sizeof(powerOffData1), 8);
	mdelay(2 * TIME_PER_FRAME);
	spiSendData(powerOffData2, sizeof(powerOffData2), 8);
	mdelay(2 * TIME_PER_FRAME);
	spiSendData(powerOffData3, sizeof(powerOffData3), 8);
	mdelay(10);
	spiSendData(powerOffData4, sizeof(powerOffData4), 8);
	mdelay(10);

	gpio_direction_input(pinfo->gpio_lcd_id0);
	mdelay(10);
	gpio_direction_input(pinfo->gpio_lcd_id1);
	mdelay(10);
	gpio_direction_output(pinfo->gpio_reset, 0);
	mdelay(10);
	gpio_direction_output(pinfo->gpio_power, 0);
	mdelay(10);
	LCD_GPIO_FREE(pinfo);
	LCD_IOMUX_SET(pinfo, LOWPOWER);
	LCD_VCC_DISABLE(pinfo);
}

static int ldi_samsung_panel_on(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	if (!k3fd->panel_info.display_on) {
		/* lcd display on */
		samsung_disp_on(k3fd);
		k3fd->panel_info.display_on = true;
		if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM) {
			/* backlight on */
			samsung_pwm_on(k3fd);
		}
	}
	return 0;
}

static int ldi_samsung_panel_off(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	if (k3fd->panel_info.display_on) {
		k3fd->panel_info.display_on = false;
		if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM) {
			/* backlight off */
			samsung_pwm_off(k3fd);
		}
		/* lcd display off */
		samsung_disp_off(k3fd);
	}

	return 0;
}

static int ldi_samsung_panel_set_backlight(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	return pwm_set_backlight(k3fd->bl_level, &(k3fd->panel_info));
}

static int ldi_samsung_panel_set_fastboot(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	LCD_VCC_ENABLE(&(k3fd->panel_info));
	LCD_IOMUX_SET(&(k3fd->panel_info), NORMAL);
	LCD_GPIO_REQUEST(&(k3fd->panel_info));

	if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM) {
		PWM_IOMUX_SET(&(k3fd->panel_info), NORMAL);
		PWM_GPIO_REQUEST(&(k3fd->panel_info));
	}

	k3fd->panel_info.display_on = true;

	return 0;
}

static struct k3_panel_info samsung_panel_info = {0};
static struct k3_fb_panel_data samsung_panel_data = {
	.panel_info = &samsung_panel_info,
	.on = ldi_samsung_panel_on,
	.off = ldi_samsung_panel_off,
	.set_backlight = ldi_samsung_panel_set_backlight,
	.set_fastboot = ldi_samsung_panel_set_fastboot,
};

static int __devinit samsung_probe(struct platform_device *pdev)
{
	struct k3_panel_info *pinfo = NULL;
	struct resource *res = NULL;

	pinfo = samsung_panel_data.panel_info;
	/* init lcd info */
	pinfo->display_on = false;
	pinfo->xres = 320;
	pinfo->yres = 480;
	pinfo->type = LDI_PANEL;
	pinfo->orientation = LCD_PORTRAIT;
	pinfo->bpp = EDC_OUT_RGB_888; /* 24 */
	pinfo->s3d_frm = EDC_FRM_FMT_2D;
	pinfo->bgr_fmt = EDC_RGB;
	pinfo->bl_set_type = BL_SET_BY_PWM;
	pinfo->bl_max = PWM_LEVEL;
	pinfo->bl_min = 1;

	pinfo->ldi.h_back_porch = 11;
	pinfo->ldi.h_front_porch = 4;
	pinfo->ldi.h_pulse_width = 4;
	pinfo->ldi.v_back_porch = 10;  /* 8 */
	pinfo->ldi.v_front_porch = 4;
	pinfo->ldi.v_pulse_width = 2;
	pinfo->ldi.hsync_plr = 1;
	pinfo->ldi.vsync_plr = 1;
	pinfo->ldi.pixelclk_plr = 0;
	pinfo->ldi.data_en_plr = 1;
	pinfo->ldi.disp_mode = LDI_DISP_MODE_NOT_3D_FBF;

	/* Note: must init here */
	pinfo->frame_rate = 60;
	pinfo->clk_rate = LCD_GET_CLK_RATE(pinfo);

	/* lcd vcc */
	LCD_VCC_GET(pdev, pinfo);
	/* lcd iomux */
	LCD_IOMUX_GET(pinfo);
	/* lcd resource */
	LCD_RESOURCE(pdev, pinfo, res);

	if (pinfo->bl_set_type & BL_SET_BY_PWM) {
		/* pwm clock*/
		PWM_CLK_GET(pinfo);
		/* pwm iomux */
		PWM_IOMUX_GET(pinfo);
		/* pwm resource */
		PWM_RESOUTCE(pdev, pinfo, res);
	}

	/* alloc panel device data */
	if (platform_device_add_data(pdev, &samsung_panel_data,
		sizeof(struct k3_fb_panel_data))) {
		pr_err("k3fb, %s: platform_device_add_data failed!\n", __func__);
		platform_device_put(pdev);
		return -ENOMEM;
	}

	k3_fb_add_device(pdev);

	return 0;
}

static int samsung_remove(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	/*BUG_ON(k3fd == NULL);*/
	if (!k3fd) {
		return 0;
	}

	if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM) {
		PWM_CLK_PUT(&(k3fd->panel_info));
	}
	LCD_VCC_PUT(&(k3fd->panel_info));

	return 0;
}

static void samsung_shutdown(struct platform_device *pdev)
{
	if (samsung_remove(pdev) != 0) {
		pr_err("k3fb, %s: failed to shutdown!\n", __func__);
	}
}

static struct platform_driver this_driver = {
	.probe = samsung_probe,
	.remove = samsung_remove,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = samsung_shutdown,
	.driver = {
		.name = "ldi_samsung_LMS350DF04",
	},
};

static int __init ldi_samsung_panel_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&this_driver);
	if (ret) {
		pr_err("k3fb, %s not able to register the driver\n", __func__);
		return ret;
	}

	return ret;
}

module_init(ldi_samsung_panel_init);
