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

#include "k3_fb_panel.h"
#include "edc_overlay.h"


int panel_next_on(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_panel_data *pdata = NULL;
	struct k3_fb_panel_data *next_pdata = NULL;
	struct platform_device *next_pdev = NULL;

	BUG_ON(pdev == NULL);

	pdata = (struct k3_fb_panel_data *)pdev->dev.platform_data;
	if (pdata) {
		next_pdev = pdata->next;
		if (next_pdev) {
			next_pdata = (struct k3_fb_panel_data *)next_pdev->dev.platform_data;
			if ((next_pdata) && (next_pdata->on))
				ret = next_pdata->on(next_pdev);
		}
	}

	return ret;
}

int panel_next_off(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_panel_data *pdata = NULL;
	struct k3_fb_panel_data *next_pdata = NULL;
	struct platform_device *next_pdev = NULL;

	BUG_ON(pdev == NULL);

	pdata = (struct k3_fb_panel_data *)pdev->dev.platform_data;
	if (pdata) {
		next_pdev = pdata->next;
		if (next_pdev) {
			next_pdata = (struct k3_fb_panel_data *)next_pdev->dev.platform_data;
			if ((next_pdata) && (next_pdata->off))
				ret = next_pdata->off(next_pdev);
		}
	}

	return ret;
}

int panel_next_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_panel_data *pdata = NULL;
	struct k3_fb_panel_data *next_pdata = NULL;
	struct platform_device *next_pdev = NULL;

	BUG_ON(pdev == NULL);

	pdata = (struct k3_fb_panel_data *)pdev->dev.platform_data;
	if (pdata) {
		next_pdev = pdata->next;
		if (next_pdev) {
			next_pdata = (struct k3_fb_panel_data *)next_pdev->dev.platform_data;
			if ((next_pdata) && (next_pdata->remove))
				ret = next_pdata->remove(next_pdev);
		}
	}

	return ret;
}

int panel_next_set_backlight(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_panel_data *pdata = NULL;
	struct k3_fb_panel_data *next_pdata = NULL;
	struct platform_device *next_pdev = NULL;

	BUG_ON(pdev == NULL);

	pdata = (struct k3_fb_panel_data *)pdev->dev.platform_data;
	if (pdata) {
		next_pdev = pdata->next;
		if (next_pdev) {
			next_pdata = (struct k3_fb_panel_data *)next_pdev->dev.platform_data;
			if ((next_pdata) && (next_pdata->set_backlight))
				ret = next_pdata->set_backlight(next_pdev);
		}
	}

	return ret;
}

int panel_next_set_timing(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_panel_data *pdata = NULL;
	struct k3_fb_panel_data *next_pdata = NULL;
	struct platform_device *next_pdev = NULL;

	BUG_ON(pdev == NULL);

	pdata = (struct k3_fb_panel_data *)pdev->dev.platform_data;
	if (pdata) {
		next_pdev = pdata->next;
		if (next_pdev) {
			next_pdata = (struct k3_fb_panel_data *)next_pdev->dev.platform_data;
			if ((next_pdata) && (next_pdata->set_timing))
				ret = next_pdata->set_timing(next_pdev);
		}
	}

	return ret;
}

int panel_next_set_playvideo(struct platform_device *pdev, int gamma)
{
	int ret = 0;
	struct k3_fb_panel_data *pdata = NULL;
	struct k3_fb_panel_data *next_pdata = NULL;
	struct platform_device *next_pdev = NULL;

	BUG_ON(pdev == NULL);

	pdata = (struct k3_fb_panel_data *)pdev->dev.platform_data;
	if (pdata) {
		next_pdev = pdata->next;
		if (next_pdev) {
			next_pdata = (struct k3_fb_panel_data *)next_pdev->dev.platform_data;
			if ((next_pdata) && (next_pdata->set_playvideo))
				ret = next_pdata->set_playvideo(next_pdev, gamma);
		}
	}

	return ret;
}

int panel_next_set_frc(struct platform_device *pdev, int target_fps)
{
	int ret = 0;
	struct k3_fb_panel_data *pdata = NULL;
	struct k3_fb_panel_data *next_pdata = NULL;
	struct platform_device *next_pdev = NULL;

	BUG_ON(pdev == NULL);

	pdata = (struct k3_fb_panel_data *)pdev->dev.platform_data;
	if (pdata) {
		next_pdev = pdata->next;
		if (next_pdev) {
			next_pdata = (struct k3_fb_panel_data *)next_pdev->dev.platform_data;
			if ((next_pdata) && (next_pdata->set_frc))
				ret = next_pdata->set_frc(next_pdev, target_fps);
		}
	}

	return ret;
}

int panel_next_check_esd(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_panel_data *pdata = NULL;
	struct k3_fb_panel_data *next_pdata = NULL;
	struct platform_device *next_pdev = NULL;

	BUG_ON(pdev == NULL);

	pdata = (struct k3_fb_panel_data *)pdev->dev.platform_data;
	if (pdata) {
		next_pdev = pdata->next;
		if (next_pdev) {
			next_pdata = (struct k3_fb_panel_data *)next_pdev->dev.platform_data;
			if ((next_pdata) && (next_pdata->check_esd))
				ret = next_pdata->check_esd(next_pdev);
		}
	}

	return ret;
}

struct platform_device *k3_fb_device_alloc(struct k3_fb_panel_data *pdata,
	u32 type, u32 id, u32 *graphic_ch)
{
	struct platform_device *this_dev = NULL;
	char dev_name[16] = {0};

	BUG_ON(pdata == NULL);

	switch (type) {
	case HDMI_PANEL:
		snprintf(dev_name, sizeof(dev_name), "ldi");
		*graphic_ch = OVERLAY_PIPE_EDC1_CH1;
		break;
	case DP_PANEL:
		snprintf(dev_name, sizeof(dev_name), "displayport");
		*graphic_ch = OVERLAY_PIPE_EDC1_CH1;
		break;
	case LDI_PANEL:
		snprintf(dev_name, sizeof(dev_name), "ldi");
		*graphic_ch = OVERLAY_PIPE_EDC0_CH2;
		break;
	case PANEL_MIPI_VIDEO:
	case PANEL_MIPI_CMD:
		snprintf(dev_name, sizeof(dev_name), "mipi_dsi");
		*graphic_ch = OVERLAY_PIPE_EDC0_CH2;
		break;
	default:
		pr_err("k3fb, %s: invalid panel type!\n", __func__);
		return NULL;
	}

	pdata->next = NULL;

	this_dev = platform_device_alloc(dev_name, ((u32) type << 16) | (u32) id);
	if (this_dev) {
		if (platform_device_add_data(this_dev, pdata, sizeof(struct k3_fb_panel_data))) {
			pr_err("k3fb, %s: platform_device_add_data failed!\n", __func__);
			platform_device_put(this_dev);
			return NULL;
		}
	}

	return this_dev;
}

void  k3_fb_device_free(struct platform_device *pdev)
{
    BUG_ON(pdev == NULL);
    platform_device_put(pdev);
}

/*y=pow(x,0.6),x=[0,255]*/
u32 square_point_six(u32 x)
{
	unsigned long t = x * x * x;
	int i = 0, j = 255, k = 0;
	unsigned long t0 = 0;
	while (j - i > 1) {
		k = (i + j) / 2;
			t0 = k * k * k * k * k;
		if(t0 < t)
			i = k;
		else if (t0 > t)
			j = k;
		else
			return k;
	}
	return k;
}

