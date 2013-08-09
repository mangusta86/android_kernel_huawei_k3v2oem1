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
#include <linux/err.h>

#include "k3_fb.h"
#include "ldi_reg.h"
#include "mipi_reg.h"

static int ldi_init(struct k3_fb_data_type *k3fd)
{
	u32 edc_base = 0;

	BUG_ON(k3fd == NULL);

	edc_base = k3fd->edc_base;

	set_LDI_HRZ_CTRL0_hfp(edc_base, k3fd->panel_info.ldi.h_front_porch);
	set_LDI_HRZ_CTRL0_hbp(edc_base, k3fd->panel_info.ldi.h_back_porch);
	set_LDI_HRZ_CTRL1_hsw(edc_base, k3fd->panel_info.ldi.h_pulse_width);
	set_LDI_VRT_CTRL0_vfp(edc_base, k3fd->panel_info.ldi.v_front_porch);
	set_LDI_VRT_CTRL0_vbp(edc_base, k3fd->panel_info.ldi.v_back_porch);
	if (k3fd->panel_info.ldi.v_pulse_width > 15)
		k3fd->panel_info.ldi.v_pulse_width = 15;
	set_LDI_VRT_CTRL1_vsw(edc_base, k3fd->panel_info.ldi.v_pulse_width);
	set_LDI_PLR_CTRL_hsync(edc_base, k3fd->panel_info.ldi.hsync_plr);
	set_LDI_PLR_CTRL_vsync(edc_base, k3fd->panel_info.ldi.vsync_plr);
	set_LDI_PLR_CTRL_pixel_clk(edc_base, k3fd->panel_info.ldi.pixelclk_plr);
	set_LDI_PLR_CTRL_data_en(edc_base, k3fd->panel_info.ldi.data_en_plr);

	set_LDI_DSP_SIZE_hsize(edc_base, k3fd->panel_info.xres);
	set_LDI_DSP_SIZE_vsize(edc_base, k3fd->panel_info.yres);

	set_LDI_WORK_MODE_work_mode(edc_base, LDI_WORK);
	set_LDI_WORK_MODE_colorbar_en(edc_base, K3_DISABLE);
	set_LDI_CTRL_bgr(edc_base, k3fd->panel_info.bgr_fmt);
	set_LDI_CTRL_bpp(edc_base, k3fd->panel_info.bpp);
	set_LDI_CTRL_disp_mode(edc_base, k3fd->panel_info.ldi.disp_mode);
	set_LDI_CTRL_corlorbar_width(edc_base, 0x3C);
	set_LDI_CTRL_ldi_en(edc_base, K3_ENABLE);
	set_LDI_INT_CLR(edc_base, 0xFFFFFFFF);
	if (!(k3fd->panel_info.bl_set_type & BL_SET_BY_PWM)) {
		set_reg(edc_base + LDI_DE_SPACE_LOW, 0x1, 1, 1);
	}

	return 0;
}

static int ldi_on(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	/* ldi clock enable */
	ret = clk_enable(k3fd->ldi_clk);
	if (ret != 0) {
		dev_err(&pdev->dev, "k3fb, %s: failed to enable ldi_clk!\n", __func__);
		return ret;
	}

	/* ldi init */
	ldi_init(k3fd);

	ret = panel_next_on(pdev);

	return ret;
}

static int ldi_off(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;
	u32 edc_base = 0;

	BUG_ON(pdev == NULL);

	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);
	edc_base = k3fd->edc_base;

	ret = panel_next_off(pdev);

	/* dsi pixel off */
	set_reg(edc_base + LDI_HDMI_DSI_GT, 0x1, 1, 0);
	/* disable ldi */
	set_reg(edc_base + LDI_CTRL_OFFSET, 0x0, 1, 0);
	/* ldi clock gating */
	clk_disable(k3fd->ldi_clk);

	return ret;
}

static int ldi_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	pr_info("k3fb, %s: enter!\n", __func__);

	BUG_ON(pdev == NULL);

	k3fd = platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	if (!IS_ERR(k3fd->ldi_clk)) {
		clk_put(k3fd->ldi_clk);
	}

	ret = panel_next_remove(pdev);
	
	pr_info("k3fb, %s: exit!\n", __func__);
	
	return ret;
}

static int ldi_set_backlight(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;
	u32 tmp = 0;

	BUG_ON(pdev == NULL);

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	if (k3fd->panel_info.bl_set_type & BL_SET_BY_MIPI_ECO) {
		if (k3fd->bl_enable_mipi_eco == 1) {
			/*check last_line interrupt*/
			tmp = inp32(k3fd->edc_base + LDI_ORG_INT_OFFSET);
			if ((tmp & 0x20) != 0x20) {
				return IRQ_HANDLED;
			}

			/* disable ldi */
			/* set_reg(k3fd->edc_base + LDI_CTRL_OFFSET, 0x0, 1, 0);*/

			ret = panel_next_set_backlight(pdev);

			/* clear vfp_last_int */
			set_reg(k3fd->edc_base + LDI_INT_CLR_OFFSET, 0x1, 1, 5);
			/* mask vfp_last_line_int */
			set_reg(k3fd->edc_base + LDI_INT_EN_OFFSET, 0x0, 1, 5);

			/* enable ldi */
			set_reg(k3fd->edc_base + LDI_ORG_INT_OFFSET, 0x1, 1, 0);

			set_reg(k3fd->edc_base + LDI_DE_SPACE_LOW, 0x1, 1, 1);

			k3fd->bl_enable_mipi_eco = 0;
		}
	} else {
		ret = panel_next_set_backlight(pdev);
	}

	return ret;
}

static int ldi_set_timing(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;
	u32 edc_base = 0;

	BUG_ON(pdev == NULL);

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	edc_base = k3fd->edc_base;

	set_LDI_HRZ_CTRL0_hfp(edc_base, k3fd->panel_info.ldi.h_front_porch);
	set_LDI_HRZ_CTRL0_hbp(edc_base, k3fd->panel_info.ldi.h_back_porch);
	set_LDI_HRZ_CTRL1_hsw(edc_base, k3fd->panel_info.ldi.h_pulse_width);
	set_LDI_VRT_CTRL0_vfp(edc_base, k3fd->panel_info.ldi.v_front_porch);
	set_LDI_VRT_CTRL0_vbp(edc_base, k3fd->panel_info.ldi.v_back_porch);
	if (k3fd->panel_info.ldi.v_pulse_width > 15)
		k3fd->panel_info.ldi.v_pulse_width = 15;
	set_LDI_VRT_CTRL1_vsw(edc_base, k3fd->panel_info.ldi.v_pulse_width);

	set_LDI_DSP_SIZE_hsize(edc_base, k3fd->panel_info.xres);
	set_LDI_DSP_SIZE_vsize(edc_base, k3fd->panel_info.yres);

	set_LDI_PLR_CTRL_hsync(edc_base, k3fd->panel_info.ldi.hsync_plr);
	set_LDI_PLR_CTRL_vsync(edc_base, k3fd->panel_info.ldi.vsync_plr);

	if (clk_set_rate(k3fd->ldi_clk, k3fd->panel_info.clk_rate) != 0) {
		pr_err("k3fb, %s: failed to set ldi clk rate(%d).\n", __func__, k3fd->panel_info.clk_rate);
	}

	ret = panel_next_set_timing(pdev);

	return ret;
}

static int ldi_set_playvideo(struct platform_device *pdev, int gamma)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;
	u32 edc_base = 0;

	BUG_ON(pdev == NULL);
	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	edc_base = k3fd->edc_base;

	ret = panel_next_set_playvideo(pdev, gamma);

	return ret;
}

static int ldi_set_frc(struct platform_device *pdev, int target_fps)
{
	int ret = 0;
	BUG_ON(pdev == NULL);

	ret = panel_next_set_frc(pdev, target_fps);

	return 0;
}
static int ldi_check_esd(struct platform_device *pdev)
{
	BUG_ON(pdev == NULL);

	return panel_next_check_esd(pdev);
}

static int ldi_probe(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;
	struct fb_info *fbi = NULL;
	struct platform_device *k3_fb_dev = NULL;
	struct k3_fb_panel_data *pdata = NULL;
	int ret = 0;

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	/* ldi clock */
	if (k3fd->edc_base == k3fd_reg_base_edc0) {
		k3fd->ldi_clk = clk_get(NULL, CLK_LDI0_NAME);
	} else {
		k3fd->ldi_clk = clk_get(NULL, CLK_LDI1_NAME);
	}

	if (IS_ERR(k3fd->ldi_clk)) {
		dev_err(&pdev->dev, "k3fb, %s: failed to get ldi_clk!\n", __func__);
		return PTR_ERR(k3fd->ldi_clk);
	}

	/* set ldi clock rate */
	ret = clk_set_rate(k3fd->ldi_clk, k3fd->panel_info.clk_rate);
	if (ret != 0) {
		pr_err("k3fb, %s: failed to set ldi clk rate(%d).\n", __func__, k3fd->panel_info.clk_rate);
	#ifndef CONFIG_MACH_TC45MSU3
		return ret;
	#endif
	}

	/* k3_fb device */
	k3_fb_dev = platform_device_alloc("k3_fb", pdev->id);
	if (!k3_fb_dev) {
		pr_err("k3fb, %s: k3_fb platform_device_alloc failed!\n", __func__);
		return -ENOMEM;
	}

	/* link to the latest pdev */
	k3fd->pdev = k3_fb_dev;

	/* alloc panel device data */
	if (platform_device_add_data(k3_fb_dev, pdev->dev.platform_data,
		sizeof(struct k3_fb_panel_data))) {
		pr_err("k3fb, %s: platform_device_add_data failed!\n", __func__);
		platform_device_put(k3_fb_dev);
		return -ENOMEM;
	}

	/* data chain */
	pdata = (struct k3_fb_panel_data *)k3_fb_dev->dev.platform_data;
	pdata->on = ldi_on;
	pdata->off = ldi_off;
	pdata->remove = ldi_remove;
	pdata->set_backlight = ldi_set_backlight;
	pdata->set_timing = ldi_set_timing;
	pdata->set_playvideo = ldi_set_playvideo;
	pdata->check_esd = ldi_check_esd;
	pdata->set_frc = ldi_set_frc;
	pdata->next = pdev;

	/* get/set panel info */
	memcpy(&k3fd->panel_info, pdata->panel_info, sizeof(struct k3_panel_info));

	fbi = k3fd->fbi;
	/*fbi->var.pixclock = clk_round_rate(pixel_edc_clk, k3fd->panel_info.clk_rate);*/
	fbi->var.pixclock = k3fd->panel_info.clk_rate;
	fbi->var.left_margin = k3fd->panel_info.ldi.h_back_porch;
	fbi->var.right_margin = k3fd->panel_info.ldi.h_front_porch;
	fbi->var.upper_margin = k3fd->panel_info.ldi.v_back_porch;
	fbi->var.lower_margin = k3fd->panel_info.ldi.v_front_porch;
	fbi->var.hsync_len = k3fd->panel_info.ldi.h_pulse_width;
	fbi->var.vsync_len = k3fd->panel_info.ldi.v_pulse_width;

	/* set driver data */
	platform_set_drvdata(k3_fb_dev, k3fd);
	/* register in k3_fb driver */
	ret = platform_device_add(k3_fb_dev);
	if (ret) {
		platform_device_put(k3_fb_dev);
		pr_err("k3fb, %s: platform_device_add failed!\n", __func__);
		return ret;
	}

	return ret;
}

static struct platform_driver this_driver = {
	.probe = ldi_probe,
	.remove = NULL,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.driver = {
		.name = "ldi",
		},
};

static int __init ldi_driver_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&this_driver);
	if (ret) {
		pr_err("k3fb, %s not able to register the driver\n", __func__);
		return ret;
	}

	return ret;
}

module_init(ldi_driver_init);
