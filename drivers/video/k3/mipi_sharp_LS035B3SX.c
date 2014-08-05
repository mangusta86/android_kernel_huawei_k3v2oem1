/* Copyright (c) 2008-2011, Hisilicon Tech. Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *	 * Redistributions of source code must retain the above copyright
 *	   notice, this list of conditions and the following disclaimer.
 *	 * Redistributions in binary form must reproduce the above
 *	   copyright notice, this list of conditions and the following
 *	   disclaimer in the documentation and/or other materials provided
 *	   with the distribution.
 *	 * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *	   contributors may be used to endorse or promote products derived
 *	   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/pwm.h>
#include <mach/platform.h>
#include <mach/gpio.h>

#include "k3_fb.h"
#include "k3_fb_def.h"
#include "mipi_dsi.h"
#include "mipi_sharp_LS035B3SX.h"


#define PWM_LEVEL 100

static struct dsi_cmd_desc sharp_video_on_cmds[] = {
	{DTYPE_GEN_WRITE1, 0, 120, WAIT_TYPE_MS,
		sizeof(exit_sleep), exit_sleep},

	{DTYPE_GEN_WRITE2, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData1), powerOnData1},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData2), powerOnData2},
	{DTYPE_GEN_WRITE2, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData3), powerOnData3},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData4), powerOnData4},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData5), powerOnData5},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData6), powerOnData6},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData7), powerOnData7},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData8), powerOnData8},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData9), powerOnData9},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData10), powerOnData10},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData11), powerOnData11},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData12), powerOnData12},
	{DTYPE_GEN_WRITE2, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData13), powerOnData13},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData14), powerOnData14},
	{DTYPE_GEN_WRITE2, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData15), powerOnData15},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData16), powerOnData16},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData17), powerOnData17},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData18), powerOnData18},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData19), powerOnData19},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData20), powerOnData20},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData21), powerOnData21},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData22), powerOnData22},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData23), powerOnData23},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData24), powerOnData24},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData25), powerOnData25},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData26), powerOnData26},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData27), powerOnData27},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData28), powerOnData28},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData29), powerOnData29},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData30), powerOnData30},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData31), powerOnData31},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData32), powerOnData32},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData33), powerOnData33},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData34), powerOnData34},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData35), powerOnData35},
	{DTYPE_GEN_LWRITE, 0, 1, WAIT_TYPE_MS,
		sizeof(powerOnData36), powerOnData36},

	{DTYPE_GEN_WRITE1, 0, 1, WAIT_TYPE_MS,
		sizeof(display_on), display_on},
};

static struct dsi_cmd_desc sharp_display_off_cmds[] = {
	{DTYPE_GEN_WRITE1, 0, 1, WAIT_TYPE_MS,
		sizeof(display_off), display_off},
	{DTYPE_GEN_WRITE1, 0, 120, WAIT_TYPE_MS,
		sizeof(enter_sleep), enter_sleep}
};

static struct k3_fb_panel_data sharp_panel_data;

static int sharp_pwm_on(struct k3_fb_data_type *k3fd)
{
	BUG_ON(k3fd == NULL);

	/* backlight on */
	PWM_IOMUX_SET(&(k3fd->panel_info), NORMAL);
	PWM_GPIO_REQUEST(&(k3fd->panel_info));
	gpio_direction_input(k3fd->panel_info.gpio_pwm1);
	mdelay(1);
	pwm_set_backlight(k3fd->bl_level, &(k3fd->panel_info));

	return 0;
}

static int sharp_pwm_off(struct k3_fb_data_type *k3fd)
{
	BUG_ON(k3fd == NULL);

	/* backlight off */
	pwm_set_backlight(0, &(k3fd->panel_info));
	gpio_direction_output(k3fd->panel_info.gpio_pwm0, 0);
	mdelay(1);
	gpio_direction_input(k3fd->panel_info.gpio_pwm1);
	mdelay(1);
	PWM_GPIO_FREE(&(k3fd->panel_info));
	PWM_IOMUX_SET(&(k3fd->panel_info), LOWPOWER);

	return 0;
}

static void sharp_disp_on(struct k3_fb_data_type *k3fd)
{
	u32 edc_base = 0;
	struct k3_panel_info *pinfo = NULL;

	BUG_ON(k3fd == NULL);
	edc_base = k3fd->edc_base;
	pinfo = &(k3fd->panel_info);

	LCD_VCC_ENABLE(pinfo);
	LCD_IOMUX_SET(pinfo, NORMAL);
	LCD_GPIO_REQUEST(pinfo);
	gpio_direction_input(pinfo->gpio_lcd_id0);
	mdelay(1);
	gpio_direction_input(pinfo->gpio_lcd_id1);
	mdelay(1);
	gpio_direction_output(pinfo->gpio_reset, 1);
	mdelay(1);
	gpio_direction_output(pinfo->gpio_power, 1);
	mdelay(1);
	gpio_direction_output(pinfo->gpio_reset, 0);
	mdelay(1);
	gpio_direction_output(pinfo->gpio_reset, 1);
	mdelay(1);

	switch (k3fd->panel_info.bpp) {
	case EDC_OUT_RGB_565:
		memcpy(sharp_video_on_cmds[35].payload, color_mode_16bits, sizeof(color_mode_16bits));
		break;
	case EDC_OUT_RGB_666:
		memcpy(sharp_video_on_cmds[35].payload, color_mode_18bits, sizeof(color_mode_18bits));
		break;
	case EDC_OUT_RGB_888:
		break;
	default:
		pr_err("k3fb, %s: Unsupport this color mode!", __func__);
		break;
	}

	mipi_dsi_cmds_tx(sharp_video_on_cmds, ARRAY_SIZE(sharp_video_on_cmds), edc_base);
}

static void sharp_disp_off(struct k3_fb_data_type *k3fd)
{
	u32 edc_base = 0;
	struct k3_panel_info *pinfo = NULL;

	BUG_ON(k3fd == NULL);
	edc_base = k3fd->edc_base;
	pinfo = &(k3fd->panel_info);

	mipi_dsi_cmds_tx(sharp_display_off_cmds,
		ARRAY_SIZE(sharp_display_off_cmds), edc_base);

	gpio_direction_input(pinfo->gpio_lcd_id0);
	mdelay(1);
	gpio_direction_input(pinfo->gpio_lcd_id1);
	mdelay(1);
	gpio_direction_output(pinfo->gpio_reset, 0);
	mdelay(1);
	gpio_direction_output(pinfo->gpio_power, 0);
	mdelay(1);
	LCD_GPIO_FREE(pinfo);
	LCD_IOMUX_SET(pinfo, LOWPOWER);
	LCD_VCC_DISABLE(pinfo);
}

static int mipi_sharp_panel_on(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	if (!k3fd->panel_info.display_on) {
		/* lcd display on */
		sharp_disp_on(k3fd);
		k3fd->panel_info.display_on = true;
		if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM) {
			/* backlight on */
			sharp_pwm_on(k3fd);
		}
	}
	return 0;
}

static int mipi_sharp_panel_off(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	if (k3fd->panel_info.display_on) {
		k3fd->panel_info.display_on = false;
		if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM) {
			/* backlight off */
			sharp_pwm_off(k3fd);
		}
		/* lcd display off */
		sharp_disp_off(k3fd);
	}

	return 0;
}

static int mipi_sharp_panel_set_backlight(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM) {
		return pwm_set_backlight(k3fd->bl_level, &(k3fd->panel_info));
	} else {
		return 0;
	}
}

static int mipi_sharp_panel_set_fastboot(struct platform_device *pdev)
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

static struct k3_panel_info sharp_panel_info = {0};
static struct k3_fb_panel_data sharp_panel_data = {
	.panel_info = &sharp_panel_info,
	.on = mipi_sharp_panel_on,
	.off = mipi_sharp_panel_off,
	.set_backlight = mipi_sharp_panel_set_backlight,
	.set_fastboot = mipi_sharp_panel_set_fastboot,
};

static int __devinit sharp_probe(struct platform_device *pdev)
{
	struct k3_panel_info *pinfo = NULL;
	struct resource *res = NULL;

	pinfo = sharp_panel_data.panel_info;
	/* init lcd panel info */
	pinfo->display_on = false;
	pinfo->xres = 640;
	pinfo->yres = 960;
	pinfo->type = MIPI_VIDEO_PANEL;
	pinfo->orientation = LCD_PORTRAIT;
	pinfo->bpp = EDC_OUT_RGB_888;
	pinfo->s3d_frm = EDC_FRM_FMT_2D;
	pinfo->bgr_fmt = EDC_RGB;
	pinfo->bl_set_type = BL_SET_BY_PWM;
	pinfo->bl_max = PWM_LEVEL;
	pinfo->bl_min = 1;

	pinfo->ldi.h_back_porch = 4;
	pinfo->ldi.h_front_porch = 18;
	pinfo->ldi.h_pulse_width = 4;
	pinfo->ldi.v_back_porch = 4;
	pinfo->ldi.v_front_porch = 6;
	pinfo->ldi.v_pulse_width = 2;
	pinfo->ldi.hsync_plr = 1;
	pinfo->ldi.vsync_plr = 1;
	pinfo->ldi.pixelclk_plr = 0;
	pinfo->ldi.data_en_plr = 0;
	pinfo->ldi.disp_mode = LDI_DISP_MODE_NOT_3D_FBF;

	/* Note: must init here */
	pinfo->frame_rate = 60;
#ifdef CONFIG_MACH_TC45MSU3
	pinfo->clk_rate = 20000000;
#else
	pinfo->clk_rate = LCD_GET_CLK_RATE(pinfo);
#endif

	pinfo->mipi.lane_nums = DSI_2_LANES;
	pinfo->mipi.color_mode = DSI_24BITS_3;
	pinfo->mipi.vc = 0;
	pinfo->mipi.dsi_bit_clk = 300;  /* clock lane(p/n) */

	/* lcd vcc */
	LCD_VCC_GET(pdev, pinfo);
	LCDIO_SET_VOLTAGE(pinfo, 1800000, 1800000);
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
	if (platform_device_add_data(pdev, &sharp_panel_data,
		sizeof(struct k3_fb_panel_data))) {
		pr_err("k3fb, %s: platform_device_add_data failed!\n", __func__);
		platform_device_put(pdev);
		return -ENOMEM;
	}

	k3_fb_add_device(pdev);

	return 0;
}

static int sharp_remove(struct platform_device *pdev)
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

static void sharp_shutdown(struct platform_device *pdev)
{
	if (sharp_remove(pdev) != 0) {
		pr_err("k3fb, %s: failed to shutdown!\n", __func__);
	}
}

static struct platform_driver this_driver = {
	.probe = sharp_probe,
	.remove = sharp_remove,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = sharp_shutdown,
	.driver = {
		.name = "mipi_sharp_LS035B3SX",
	},
};

static int __init mipi_sharp_panel_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&this_driver);
	if (ret) {
		pr_err("k3fb, %s not able to register the driver\n", __func__);
		return ret;
	}

	return ret;
}

module_init(mipi_sharp_panel_init);
