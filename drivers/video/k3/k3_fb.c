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
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/console.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/leds.h>
#include <linux/regulator/consumer.h>
#include <mach/boardid.h>

#include "k3_fb.h"
#include "edc_reg.h"
#include "ldi_reg.h"
#include "mipi_reg.h"
#include "sbl_reg.h"

#include "hdmi/k3_hdmi.h"

#define K3_FB_MAX_DEV_LIST 32
#define MAX_FBI_LIST 32

static int k3_fb_resource_initialized;
static struct platform_device *pdev_list[K3_FB_MAX_DEV_LIST] = {0};

static int pdev_list_cnt;
static struct fb_info *fbi_list[MAX_FBI_LIST] = {0};
static int fbi_list_index;

static struct k3_fb_data_type *k3fd_list[MAX_FBI_LIST] = {0};
static int k3fd_list_index;

u32 k3fd_reg_base_edc0;
u32 k3fd_reg_base_edc1;
static u32 k3fd_irq_edc0;
static u32 k3fd_irq_edc1;
static u32 k3fd_irq_ldi0;
static u32 k3fd_irq_ldi1;
static bool hdmi_is_connected = false;

static u32 k3_fb_pseudo_palette[16] = {
	0x00000000, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
};

#define MAX_BACKLIGHT_BRIGHTNESS 255
static int lcd_backlight_registered;

#if K3_FB_OVERLAY_USE_BUF
#define MAX_OVERLAY_BUF_NUM 3
typedef struct overlay_video_data {
	struct overlay_data data;
	bool is_set;
	u32 count;
} overlay_video_data;

typedef struct overlay_video_buf_ {
	overlay_video_data video_data_list[MAX_OVERLAY_BUF_NUM];
	struct work_struct play_work;
	u32 write_addr;
	u32 play_addr;
	u32 last_addr;
	bool exit_work;    
	struct workqueue_struct *play_wq;
	struct mutex overlay_mutex;
	bool is_init;
	bool is_video;
} overlay_video_buf;

static overlay_video_buf video_buf;
#endif

DEFINE_SEMAPHORE(k3_fb_overlay_sem);
DEFINE_SEMAPHORE(k3_fb_backlight_sem);
DEFINE_SEMAPHORE(k3_fb_blank_sem);

/******************************************************************************/
static int k3fd_g2d_clk_rate_off;

static void k3_fb_set_g2d_clk_rate(int level)
{
	struct k3_fb_data_type *k3fd = NULL;
	int ret = 0;
	u32 rate = G2D_CLK_480MHz;

	k3fd = k3fd_list[0];
	BUG_ON(k3fd == NULL);

	if (!k3fd->panel_power_on) {
		return;
	}

	switch (level) {
	case 1:
		rate = G2D_CLK_60MHz;
		break;
	case 2:
		rate = G2D_CLK_120MHz;
		break;
	case 3:
		rate = G2D_CLK_240MHz;
		break;
	case 4:
		rate = G2D_CLK_360MHz;
		break;
	case 5:
		rate = G2D_CLK_480MHz;
		break;
	default:
		rate = G2D_CLK_480MHz;
		break;
	}

	if (k3fd->g2d_clk) {
		ret = clk_set_rate(k3fd->g2d_clk,  rate);
		if (ret != 0) {
			pr_err("k3fb, %s: %d error = %d\n", __func__, rate, ret);
		} else {
			pr_info("k3fb, %s: %d\n", __func__, rate);
		}
	}
}

/*Create sysfs node start*/
static int frc_state;
static ssize_t k3_fb_frc_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	k3fd = k3fd_list[0];
	BUG_ON(k3fd == NULL);

	if (k3fd->panel_info.frc_enable) {
	sprintf(buf, "frc_state = 0x%x; frc_flag = %d\n", frc_state, k3fd->frc_flag);
		ret = strlen(buf);
	}

	return ret;
}

static ssize_t k3_fb_frc_state_store(struct device *dev,
		struct device_attribute *attr, char *buf, size_t size)
{
	/*  Different input values mean different application scenes as follow:
	  *  1) 1:1 or 1:0 mean Video is playing or stop playing;
	  *  2) 2:1 or 2:0 mean Game is playing or stop playing;
	  *  3) 3:1 or 3:0 mean Benchmark is runing or stop running.
	  *  4) 4:1 or 4:0 mean Webkit is running or stop running;
	  *  5) 5:1 or 5:0 mean Special Game(angryBird and templeRun) is palying or stop playing;
          *  6) ......
	  */
	switch (buf[0]-'0') {
	case 1:
	case 2:
	case 3:
	case 4:
        case 5:
		{
			if (buf[2] == '0') {
				frc_state &= ~(1 << (buf[0] - '0'));
			} else {
				frc_state |= (1 << (buf[0] - '0'));
			}
		}
		break;
	case 9:
		{
			if (frc_state == K3_FB_FRC_BENCHMARK_PLAYING) {
				k3_fb_set_g2d_clk_rate(5);
				break;
			}

			if (buf[2] == '0') {
				if (k3fd_g2d_clk_rate_off == 0) {
					k3fd_g2d_clk_rate_off = 1;
					k3_fb_set_g2d_clk_rate(5);
				} else {
					k3fd_g2d_clk_rate_off = 0;
				}
			}

			if (k3fd_g2d_clk_rate_off == 0)
				k3_fb_set_g2d_clk_rate(buf[2]-'0');
		}
		break;
	default:
		break;
	}

	return size;
}

static ssize_t k3_fb_smartbl_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct k3_fb_data_type *k3fd = NULL;
	int ret = 0;

	k3fd = k3fd_list[0];
	BUG_ON(k3fd == NULL);

	if (k3fd->panel_info.sbl_enable) {
		sprintf(buf, "sbl_enable = 0x%x , sbl_lsensor_value = 0x%x\n",
			k3fd->sbl_enable, k3fd->sbl_lsensor_value);
		ret = strlen(buf);
	}

	return ret;
}

static ssize_t k3_fb_smartbl_store(struct device *dev,
	struct device_attribute *attr, char *buf, size_t size)
{
	unsigned long val = 0;
	struct k3_fb_data_type *k3fd = NULL;

	k3fd = k3fd_list[0];
	BUG_ON(k3fd == NULL);

	down(&k3_fb_blank_sem);
		val = simple_strtoul(buf, NULL, 10);
	if (k3fd->panel_info.sbl_enable) {
		k3fd->sbl_lsensor_value =  val & 0xffff;
		k3fd->sbl_enable = (val >> 16) & 0xf;

		if (!k3fd->panel_power_on) {
			up(&k3_fb_blank_sem);
			pr_notice("k3fb, %s: panel power is not on.\n", __func__);
			return 0;
		}

		if (get_chipid() != CS_CHIP_ID) {
			up(&k3_fb_blank_sem);
			return 0;
		}

		smartbl_ctrl_set(k3fd);
	}
	up(&k3_fb_blank_sem);
	return size;
}

static DEVICE_ATTR(frc_state, (S_IRUGO | S_IWUSR | S_IWGRP),
	k3_fb_frc_state_show, k3_fb_frc_state_store);

static DEVICE_ATTR(sbl_lsensor_state, (S_IRUGO | S_IWUSR | S_IWGRP),
	k3_fb_smartbl_show, k3_fb_smartbl_store);

static struct attribute *k3_fb_attrs[] = {
	&dev_attr_frc_state.attr,
	&dev_attr_sbl_lsensor_state.attr,
	NULL
};

static struct attribute_group k3_fb_attr_group = {
	.attrs = k3_fb_attrs,
};
static int k3_fb_sysfs_create(struct platform_device *pdev)
{
	int ret;
	ret = sysfs_create_group(&pdev->dev.kobj, &k3_fb_attr_group);
	if (ret) {
		pr_err("k3fb, %s: create sysfs file failed!\n", __func__);
		return ret;
	}
	return 0;
}

static void k3_fb_sysfs_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &k3_fb_attr_group);
}
/*Create sysfs node end*/

#if K3_FB_OVERLAY_USE_BUF
static void reset_video_buf(void)
{
	memset(&video_buf.video_data_list, 0, sizeof(video_buf.video_data_list));
	video_buf.write_addr = 0;
	video_buf.play_addr  = 0;
    video_buf.last_addr  = 0;
}

static void overlay_play_work_queue(struct work_struct *ws)
{
	struct fb_info *info = fbi_list[1];
	struct k3_fb_data_type *k3fd = NULL;
	overlay_video_data *play_data = NULL;
	u32 play_index = 0;
	int i = 0;
	int free_count = 0;
	int min_count  = 0;
	int null_count = 0;
	int timing_code = 0;

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	while (!video_buf.exit_work) {
		if (video_buf.is_video && hdmi_is_connected) {	
			if (!wait_event_interruptible_timeout(k3fd->edc_wait_queque, k3fd->edc_wait_flash, HZ)) {
				k3fd->edc_wait_flash = 0;
				continue;
			} else {
				if (signal_pending(current)) {
					pr_warn("k3fb%d, %s: wait event signal pending !!\n", k3fd->index, __func__);
					k3fd->edc_wait_flash = 0;
					continue;
				}
			}
			k3fd->edc_wait_flash = 0;
		} else {
			msleep(80);
			continue;
		}
		
		mutex_lock(&video_buf.overlay_mutex);

		free_count = 0;
		min_count = 0;

		video_buf.play_addr = video_buf.write_addr;

		if (video_buf.is_video) {
			for (i = 0; i < MAX_OVERLAY_BUF_NUM; i++) {
				play_data = &video_buf.video_data_list[i];
				if (play_data->is_set) {
					if (0 == min_count || play_data->count < min_count) {
						min_count = play_data->count;
						play_index = i;
					}
				} else {
					free_count++;
				}
			}
		
			if (MAX_OVERLAY_BUF_NUM == free_count) {
				null_count++;
				timing_code = info->var.reserved[3];
				if (null_count < 20 && (timing_code == 32 || timing_code == 33 || timing_code == 34)) {
					pr_warn("k3fb video buff null 00000\n");
				}
			} else {
				null_count = 0;
				play_data = &video_buf.video_data_list[play_index];

				if ((play_data->count == 1) && (free_count != 0)) {
					//pr_info("k3fb:free_count:%d count:%d\n", free_count, play_data->count);
                } else {
					//pr_info("k3fb:play index:%d count:%d\n", play_index, play_data->count);
    				video_buf.write_addr = play_data->data.src.phy_addr;
    				play_data->is_set = false;
				}
				edc_overlay_play(k3fd->fbi, &play_data->data);
			}
		}
		mutex_unlock(&video_buf.overlay_mutex);
	}
	return ;
}

static int overlay_play_work(struct k3_fb_data_type *k3fd)
{
	memset(&video_buf, 0, sizeof(video_buf));

	video_buf.play_wq = create_singlethread_workqueue("overlay_play_work"); 
	if (!(video_buf.play_wq)) {
		pr_err("%s : workqueue create failed !", __FUNCTION__);
		return -1;
	}

	mutex_init(&video_buf.overlay_mutex);
	INIT_WORK(&video_buf.play_work, overlay_play_work_queue);

	video_buf.is_init = true;

	return 0;
}
#endif

void set_reg(u32 addr, u32 val, u8 bw, u8 bs)
{
	u32 mask = (1 << bw) - 1;
	u32 tmp = inp32(addr);
	tmp &= ~(mask << bs);
	outp32(addr, tmp | ((val & mask) << bs));
}

static void k3_fb_set_backlight_cmd_mode(struct k3_fb_data_type *k3fd, u32 bkl_lvl)
{
	struct k3_fb_panel_data *pdata = NULL;
	int ret = 0;

	BUG_ON(k3fd == NULL);

	if (!k3fd->panel_power_on) {
		return;
	}

	pdata = (struct k3_fb_panel_data *)k3fd->pdev->dev.platform_data;
	if ((!pdata) || (!pdata->set_backlight)) {
		k3fb_loge("no panel operation detected!\n");
		return;
	}

	if (k3fd->cmd_mode_refresh)
		return;

	k3fd->bl_level = bkl_lvl;
	ret = pdata->set_backlight(k3fd->pdev);
	if (ret == 0) {
		k3fd->bl_level_old = k3fd->bl_level;
	} else {
		k3fb_loge("failed to set backlight.\n");
	}
}

void k3_fb_set_backlight(struct k3_fb_data_type *k3fd, u32 bkl_lvl)
{
	struct k3_fb_panel_data *pdata = NULL;
	int ret = 0;
	BUG_ON(k3fd == NULL);

	if (!k3fd->panel_power_on) {
		return;
	}

	pdata = (struct k3_fb_panel_data *)k3fd->pdev->dev.platform_data;
	if ((!pdata) || (!pdata->set_backlight)) {
		k3fb_loge("no panel operation detected!\n");
		return;
	}

	if (k3fd->panel_info.type == PANEL_MIPI_CMD) {
		if (k3fd->cmd_mode_refresh)
			return;
	}

	down(&k3_fb_backlight_sem);

	if (k3fd->panel_info.type == PANEL_MIPI_CMD) {
	#if CLK_SWITCH
		/*Enable edc0 clk*/
		clk_enable(k3fd->edc_clk);
		clk_enable(k3fd->ldi_clk);
	#endif
	}

	k3fd->bl_level = bkl_lvl;
	ret = pdata->set_backlight(k3fd->pdev);
	if (ret == 0) {
		k3fd->bl_level_old = k3fd->bl_level;
	} else {
		k3fb_loge("failed to set backlight.\n");
	}

	if (k3fd->panel_info.type == PANEL_MIPI_CMD) {
#if CLK_SWITCH
	/*Disable edc0 clk*/
	clk_disable(k3fd->ldi_clk);
	clk_disable(k3fd->edc_clk);
#endif
	}

	up(&k3_fb_backlight_sem);
}

static int k3_fb_blank_sub(int blank_mode, struct fb_info *info)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;
	struct k3_fb_panel_data *pdata = NULL;

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	pdata = (struct k3_fb_panel_data *)k3fd->pdev->dev.platform_data;
	if ((!pdata) || (!pdata->on) || (!pdata->off)) {
		pr_err("k3fb, %s: no panel operation detected!\n", __func__);
		return -ENODEV;
	}

	down(&k3_fb_blank_sem);

	switch (blank_mode) {
	case FB_BLANK_UNBLANK:
		if (!k3fd->panel_power_on) {

			edc_fb_resume(info);
			ret = pdata->on(k3fd->pdev);
			if (ret != 0) {
				pr_err("k3fb, %s: failed to turn on sub devices!\n", __func__);
			} else {
				if (k3fd->panel_info.sbl_enable)
					smartbl_ctrl_resume(k3fd);

				k3fd->panel_power_on = true;
				if (k3fd->panel_info.type != PANEL_MIPI_CMD)
					k3_fb_set_backlight(k3fd, k3fd->bl_level);
				
				if (k3fd->panel_info.type == PANEL_MIPI_CMD) {
					set_EDC_INTE(k3fd->edc_base, 0xFFFFFFFF);
					set_EDC_INTS(k3fd->edc_base, 0x0);
					k3fd->is_first_frame_end = false;
				}

				/* enable edc irq */
				if (k3fd->edc_irq)
					enable_irq(k3fd->edc_irq);
				/* enable ldi irq */
				if (k3fd->ldi_irq)
					enable_irq(k3fd->ldi_irq);

				if (k3fd->panel_info.type == PANEL_MIPI_CMD) {
					set_EDC_INTE(k3fd->edc_base, 0xFFFFFF3F); 
				#if CLK_SWITCH
					/*Disable ldi clk*/
					clk_disable(k3fd->ldi_clk);
					/*Disable edc0 clk*/
					clk_disable(k3fd->edc_clk);
				#endif
				}
			}
		}
		break;

	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_NORMAL:
	case FB_BLANK_POWERDOWN:
	default:
		if (k3fd->panel_power_on) {
			bool curr_pwr_state = false;

			curr_pwr_state = k3fd->panel_power_on;
			k3fd->panel_power_on = false;

			/* disable edc irq */
			if (k3fd->edc_irq)
				disable_irq(k3fd->edc_irq);
			/* disable ldi irq*/
			if (k3fd->ldi_irq)
				disable_irq(k3fd->ldi_irq);

			if (k3fd->panel_info.type == PANEL_MIPI_CMD) {
				if (k3fd->frame_count == 1) {
				#if CLK_SWITCH
					/* enable edc clk */
					clk_enable(k3fd->edc_clk);
					/*enable ldi clk*/
					clk_enable(k3fd->ldi_clk);
				#endif
				}

				k3fd->frame_count = 0;
			}

			ret = pdata->off(k3fd->pdev);
			k3fd->bl_level_old = 0;
			if (ret != 0) {
				k3fd->panel_power_on = curr_pwr_state;
				pr_err("k3fb, %s: failed to turn off sub devices!\n", __func__);
			} else {
				edc_fb_suspend(info);
			}
		}
		break;
	}

	up(&k3_fb_blank_sem);

	return ret;
}

u32 k3_fb_line_length(u32 fb_index, u32 xres, int bpp)
{
#ifdef CONFIG_FB_64BYTES_ODD_ALIGN
	u32 stride = ALIGN_UP(xres * bpp, 64);
	return (((stride / 64) % 2 == 0) ? (stride + 64) : stride);
#else
	return xres * bpp;
#endif
}


/******************************************************************************/
static int k3_fb_open(struct fb_info *info, int user)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	pr_info("k3fb, %s: enter!\n", __func__);

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

#ifdef CONFIG_FASTBOOT_ENABLE
	if (k3fd->index == 0) {
		if (k3fd->ref_cnt == 1) {
			memset(info->screen_base, 0x0, info->fix.smem_len);
		}
	}
#endif

	if (!k3fd->ref_cnt) {
		ret = k3_fb_blank_sub(FB_BLANK_UNBLANK, info);
		if (ret != 0) {
			pr_err("k3fb, %s: can't turn on display!\n", __func__);
			return ret;
		}
	}

	k3fd->ref_cnt++;

	pr_info("k3fb, %s: exit!\n", __func__);

	return ret;
}

static int k3_fb_release(struct fb_info *info, int user)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	pr_info("k3fb, %s: enter!\n", __func__);

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	if (!k3fd->ref_cnt) {
		pr_err("k3fb, %s: try to close unopened fb %d!\n", __func__, k3fd->index);
		return -EINVAL;
	}

	k3fd->ref_cnt--;

	if (!k3fd->ref_cnt) {
		ret = k3_fb_blank_sub(FB_BLANK_POWERDOWN, info);
		if (ret != 0) {
			pr_err("k3fb, %s: can't turn off display!\n", __func__);
			return ret;
		}
	}

	pr_info("k3fb, %s: exit!\n", __func__);

	return ret;
}

static int k3_fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;
	/*u32 max_xres = 0;
	u32 max_yres = 0;*/

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	if (var->rotate != FB_ROTATE_UR) {
		pr_err("k3fb, %s: rotate %d!\n", __func__, var->rotate);
		return -EINVAL;
	}

	if (var->grayscale != info->var.grayscale) {
		pr_err("k3fb, %s: grayscale %d!\n", __func__, var->grayscale);
		return -EINVAL;
	}

	switch (var->bits_per_pixel) {
	/* RGB565 RGBA5551 RGBX5551*/
	case 16:
		{
			if (var->blue.offset == 0) {
				if (var->red.offset == 10) {
					if ((var->transp.offset != 15) ||
						(var->green.length != 5) ||
						((var->transp.length != 1) && (var->transp.length != 0))) {
						pr_err("k3fb, %s: not match  RGBA5551 and RGBX5551!\n", __func__);
						return -EINVAL;
					}
				} else if (var->red.offset == 11) {
					if ((var->transp.offset != 0) ||
						(var->green.length != 6) ||
						(var->transp.length != 0)) {
						pr_err("k3fb, %s: not match  RGB565!\n", __func__);
						return -EINVAL;
					}
				} else {
					pr_err("k3fb, %s: 1 not match  RGB565, RGBA5551 or RGBX5551!\n", __func__);
					return -EINVAL;
				}
			} else if (var->blue.offset == 10) {
				if ((var->red.offset != 0) ||
					(var->transp.offset != 15) ||
					(var->green.length != 5) ||
					((var->transp.length != 1) && (var->transp.length != 0))) {
					pr_err("k3fb, %s: not match  BGRA5551 and BGRX5551!\n", __func__);
					return -EINVAL;
				}
			} else if (var->blue.offset == 11) {
				if ((var->red.offset != 0) ||
					(var->transp.offset != 0) ||
					(var->green.length != 6) ||
					(var->transp.length != 0)) {
					pr_err("k3fb, %s: not match  BGR565!\n", __func__);
					return -EINVAL;
				}
			} else {
				pr_err("k3fb, %s: 2 not match  RGB565, RGBA5551 or RGBX5551!\n", __func__);
				return -EINVAL;
			}

			/* Check the common values for RGB565, RGBA5551 and RGBX5551 */
			if ((var->green.offset != 5) ||
				(var->blue.length != 5) ||
				(var->red.length != 5) ||
				(var->blue.msb_right != 0) ||
				(var->green.msb_right != 0) ||
				(var->red.msb_right != 0) ||
				(var->transp.msb_right != 0)) {
				pr_err("k3fb, %s: 3 not match  RGB565, RGBA5551 or RGBX5551!\n", __func__);
				return -EINVAL;
			}
		}
		break;
	/* RGBA8888 RGBX8888*/
	case 32:
		{
			if (var->blue.offset == 0) {
				if (var->red.offset != 16) {
					pr_err("k3fb, %s: not match EDC_RGB, bpp=32!\n", __func__);
					return -EINVAL;
				}
			} else if (var->blue.offset == 16) {
				if (var->red.offset != 0) {
					pr_err("k3fb, %s: not match EDC_BGR, bpp=32!\n", __func__);
					return -EINVAL;
				}
			} else {
				pr_err("k3fb, %s: 1 not match RGBA8888 or RGBX8888!\n", __func__);
				return -EINVAL;
			}

			/* Check the common values for RGBA8888 and RGBX8888 */
			if ((var->green.offset != 8) ||
				(var->transp.offset != 24) ||
				(var->blue.length != 8) ||
				(var->green.length != 8) ||
				(var->red.length != 8) ||
				((var->transp.length != 8) && (var->transp.length != 0)) ||
				(var->blue.msb_right != 0) ||
				(var->green.msb_right != 0) ||
				(var->red.msb_right != 0) ||
				(var->transp.msb_right != 0)) {
				pr_err("k3fb, %s: 2 not match RGBA8888 or RGBX8888!\n", __func__);
				return -EINVAL;
			}
		}
		break;
	default:
		{
			pr_err("k3fb, %s: bits_per_pixel=%d not supported!\n", __func__, var->bits_per_pixel);
			return -EINVAL;
		}
		break;
	}

	if ((var->xres_virtual < K3_FB_MIN_WIDTH) || (var->yres_virtual < K3_FB_MIN_HEIGHT)) {
		pr_err("k3fb, %s: xres_virtual=%d yres_virtual=%d out of range!", __func__, var->xres_virtual, var->yres_virtual);
		return -EINVAL;
	}

	/*max_xres = MIN(var->xres_virtual, K3_FB_MAX_WIDTH);
	max_yres = MIN(var->yres_virtual, K3_FB_MAX_HEIGHT);
	if ((var->xres < K3_FB_MIN_WIDTH) ||
		(var->yres < K3_FB_MIN_HEIGHT) ||
		(var->xres > max_xres) ||
		(var->yres > max_yres)) {
		pr_err("k3fb, %s: xres=%d yres=%d out of range!", __func__, var->xres, var->yres);
		return -EINVAL;
	}*/

	if (var->xoffset > (var->xres_virtual - var->xres)) {
		pr_err("k3fb, %s: xoffset=%d out of range!", __func__, var->xoffset);
		return -EINVAL;
	}

	if (var->yoffset > (var->yres_virtual - var->yres)) {
		pr_err("k3fb, %s: yoffset=%d out of range!", __func__, var->yoffset);
		return -EINVAL;
	}

	if (info->fix.smem_len < (k3_fb_line_length(k3fd->index, var->xres, (var->bits_per_pixel >> 3))  * var->yres)) {
		pr_err("k3fb, %s: smem_len=%d out of range!", __func__, info->fix.smem_len);
		return -EINVAL;
	}

	return ret;
}

static int k3_fb_set_par(struct fb_info *info)
{
	struct k3_fb_data_type *k3fd = NULL;
	struct fb_var_screeninfo *var = NULL;

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	var = &info->var;
	switch (var->bits_per_pixel) {
	case 16:
		{
			if (var->blue.offset == 0) {
				k3fd->fb_bgrFmt = EDC_RGB;
				if (var->red.offset == 11)
					k3fd->fb_imgType = EDC_RGB_565;
				else
					k3fd->fb_imgType = EDC_ARGB_1555;
			} else {
				k3fd->fb_bgrFmt = EDC_BGR;
				if (var->blue.offset == 11)
					k3fd->fb_imgType = EDC_RGB_565;
				else
					k3fd->fb_imgType = EDC_ARGB_1555;
			}
		}
		break;
	case 32:
		{
			if (var->blue.offset == 0) {
				k3fd->fb_bgrFmt = EDC_RGB;
			} else {
				k3fd->fb_bgrFmt = EDC_BGR;
			}

			if (var->transp.length == 8)
				k3fd->fb_imgType = EDC_ARGB_8888;
			else
				k3fd->fb_imgType = EDC_XRGB_8888;
		}
		break;
	default:
		pr_err("k3fb, %s: bits_per_pixel=%d not supported!", __func__, var->bits_per_pixel);
		return -EINVAL;
	}

	k3fd->fbi->fix.line_length = k3_fb_line_length(k3fd->index, var->xres, var->bits_per_pixel >> 3);

	if (info->fix.xpanstep)
		info->var.xoffset = (var->xoffset / info->fix.xpanstep) * info->fix.xpanstep;

	if (info->fix.ypanstep)
		info->var.yoffset = (var->yoffset / info->fix.ypanstep) * info->fix.ypanstep;

	return 0;
}

static int k3fb_frc_get_target_fps(struct k3_fb_data_type *k3fd, int frc_state)
{
	int target_fps = 0;

	switch (frc_state) {
	case K3_FB_FRC_VIDEO_PLAYING:
		target_fps = K3_FB_FRC_VIDEO_FPS;
		break;
	case K3_FB_FRC_GAME_PLAYING:
		target_fps = K3_FB_FRC_GAME_FPS;
		break;
	case K3_FB_FRC_VIDEO_IN_GAME:
		target_fps = K3_FB_FRC_VIDEO_FPS;
		break;
	case K3_FB_FRC_BENCHMARK_PLAYING:
		target_fps = hdmi_is_connected ? K3_FB_FRC_BENCHMARK_FPS : K3_FB_FRC_BENCHMARK_FPS;
		break;
	case K3_FB_FRC_WEBKIT_PLAYING:
		target_fps = K3_FB_FRC_WEBKIT_FPS;
		break;
        case K3_FB_FRC_SPECIAL_GAME_PLAYING:
                target_fps = K3_FB_FRC_SPECIAL_GAME_FPS;
                break;
	case K3_FB_FRC_NONE_PLAYING:
	default:
		target_fps = (k3fd->frc_flag == FB_FRC_FLAG_DECREASE) ?
			K3_FB_FRC_IDLE_FPS : K3_FB_FRC_NORMAL_FPS;
		break;
	}

	return target_fps;

}

static bool k3fb_frc_prepare(struct k3_fb_data_type *k3fd)
{
	static u32 addr_old;
	u32 addr_new;
	int target_fps = K3_FB_FRC_NORMAL_FPS;

	if (time_after((k3fd->frc_timestamp  + HZ * 3), jiffies)) {
		return false;
	}

	/* When HDMI is connected*/
	if (k3fd->index == 0 && hdmi_is_connected) {
		switch (frc_state) {
		case K3_FB_FRC_GAME_PLAYING:
		case K3_FB_FRC_VIDEO_PLAYING:
		case K3_FB_FRC_BENCHMARK_PLAYING:
		case K3_FB_FRC_WEBKIT_PLAYING:
                case K3_FB_FRC_SPECIAL_GAME_PLAYING:
			{
				if (k3fd->frc_flag == FB_FRC_FLAG_IDLE) {
					target_fps = k3fb_frc_get_target_fps(k3fd, frc_state);
					if (target_fps != k3fd->panel_info.frame_rate) {
						k3fd->frc_flag = FB_FRC_FLAG_DECREASE;
						return true;
					}
					return false;
				}
				k3fd->frc_flag = FB_FRC_FLAG_DECREASE;
				return true;
			}
			break;
		case K3_FB_FRC_NONE_PLAYING:
		default:
			{
				if (k3fd->frc_flag == FB_FRC_FLAG_IDLE) {
					k3fd->frc_flag = FB_FRC_FLAG_INCREASE;
					return true;
				} else if (k3fd->frc_flag == FB_FRC_FLAG_BUSY) {
					return false;
				}
			}
			break;
		}

	}

	addr_new = inp32(k3fd->edc_base + EDC_CH2L_ADDR_OFFSET);

	/* Video, Game or Benchmark is playing. */
	if (frc_state != K3_FB_FRC_NONE_PLAYING) {
		if (k3fd->frc_flag == FB_FRC_FLAG_IDLE) {
			target_fps = k3fb_frc_get_target_fps(k3fd, frc_state);
			if (target_fps != k3fd->panel_info.frame_rate) {
				k3fd->frc_flag =  FB_FRC_FLAG_DECREASE;
				return true;
			}

			return false;
		}

		k3fd->frc_flag = FB_FRC_FLAG_DECREASE;
		k3fd->frc_frame_count = 0;
		addr_old = addr_new;
		return true;
	}

	if (addr_new == addr_old) {
		/* Already in IDLE */
		if (k3fd->frc_flag == FB_FRC_FLAG_IDLE) {
			k3fd->frc_frame_count = 0;
			return false;
		}
		k3fd->frc_frame_count++;
		if (k3fd->frc_frame_count >= K3_FB_FRC_THRESHOLD) {
			k3fd->frc_flag = FB_FRC_FLAG_DECREASE;
			k3fd->frc_frame_count = 0;
		} else {
			return false;
		}
	} else {
		k3fd->frc_frame_count = 0;
		if (k3fd->frc_flag == FB_FRC_FLAG_IDLE) {
			k3fd->frc_flag = FB_FRC_FLAG_INCREASE;
		} else if (k3fd->frc_flag == FB_FRC_FLAG_BUSY) {
			addr_old = addr_new;
			return false;
		}
	}
	addr_old = addr_new;

	return true;
}

static int k3fb_frc_set(struct k3_fb_data_type *k3fd)
{
	int delay_count = 0;
	bool is_timeout = false;
	int tmp = 0;
	struct k3_fb_panel_data *pdata = NULL;

	if ((k3fd->frc_flag == FB_FRC_FLAG_IDLE)  || (k3fd->frc_flag == FB_FRC_FLAG_BUSY)) {
		/* Should't enter this branch*/
		pr_err("k3fb, %s: ERROR! Already in IDLE or BUSY, need't adjust FPS\n", __func__);

		if (get_chipid() == DI_CHIP_ID) {
			/* disable vfp_last_int */
			set_reg(k3fd->edc_base + LDI_INT_EN_OFFSET, 0x0, 1, 5);
		} else {
			/* disable vfrontporch_end_int */
			set_reg(k3fd->edc_base + LDI_INT_EN_OFFSET, 0x0, 1, 10);
		}

		/* Enable LDI*/
		set_reg(k3fd->edc_base + LDI_CTRL_OFFSET, 0x1, 1, 0);
		return -1;
	}

	if ((k3fd->frc_flag == FB_FRC_FLAG_DECREASE)  || (k3fd->frc_flag == FB_FRC_FLAG_INCREASE)) {
		/* Wait DSI Lane Stop for 100us*/
		while (1) {
			tmp = inp32(k3fd->edc_base + MIPIDSI_PHY_STATUS_OFFSET);
			if (((tmp & 0xA90) == 0xA90) || delay_count > 100) {
				is_timeout = (delay_count > 100) ? true : false;
				delay_count = 0;
				break;
			} else {
				udelay(1);
				++delay_count;
			}
		}

		if (is_timeout) {
			pr_err("k3fb, %s: wait DSI Lane Stop timeout\n", __func__);
			return -1;
		}

		/* target fps depends on different scenes
			Adjust frame rate */
		pdata = (struct k3_fb_panel_data *)k3fd->pdev->dev.platform_data;
		if ((!pdata) || (!pdata->set_frc)) {
			pr_err("k3fb, %s: no panel operation detected!\n", __func__);
			return -ENODEV;
		}

		if (pdata->set_frc(k3fd->pdev,  k3fb_frc_get_target_fps(k3fd, frc_state)) != 0) {
			pr_err("k3fb, %s: set frc failed!\n", __func__);
			return -1;
		}
	}

	if (k3fd->frc_flag == FB_FRC_FLAG_DECREASE)
		k3fd->frc_flag = FB_FRC_FLAG_IDLE;
	else if (k3fd->frc_flag == FB_FRC_FLAG_INCREASE)
		k3fd->frc_flag = FB_FRC_FLAG_BUSY;

	return 0;
}

static void sbl_workqueue(struct work_struct *ws)
{
	static unsigned int ALold;
	unsigned int res;
	int m = 3;
	u32 lsensor_h = 0;
	u32 lsensor_l = 0;
	u32 tmp_sbl = 0;
	struct k3_fb_data_type *k3fd = NULL;

	k3fd = container_of(ws, struct k3_fb_data_type, sbl_work);
	BUG_ON(k3fd == NULL);

	if (get_chipid() == DI_CHIP_ID)
		return;

	down(&k3_fb_blank_sem);
	if (!k3fd->panel_power_on) {
		up(&k3_fb_blank_sem);
		return;
	}

	tmp_sbl = inp32(k3fd->edc_base + EDC_DISP_DPD_OFFSET);
	if ((tmp_sbl & REG_SBL_EN) == REG_SBL_EN) {
		res = ((ALold << m) + ((int)k3fd->sbl_lsensor_value << 14) - ALold) >> m;
		if (res != ALold) {
			ALold = res;
			lsensor_h = ((res >> 14) >> 8) & 0xff;
			lsensor_l = (res >> 14) & 0xff;
			set_SBL_AMBIENT_LIGHT_L_ambient_light_l(k3fd->edc_base, lsensor_l);
			set_SBL_AMBIENT_LIGHT_H_ambient_light_h(k3fd->edc_base, lsensor_h);
		}
	}
	up(&k3_fb_blank_sem);

}

static int init_sbl_workqueue(struct k3_fb_data_type *k3fd)
{
	BUG_ON(k3fd == NULL);

	k3fd->sbl_wq = create_singlethread_workqueue("sbl_workqueue");
	if (!k3fd->sbl_wq) {
		pr_err("k3fb, %s : workqueue create failed !", __FUNCTION__);
		return -1;
	}

	INIT_WORK(&k3fd->sbl_work, sbl_workqueue);

	return 0;
}

static bool k3fb_esd_prepare(struct k3_fb_data_type *k3fd)
{
	if (time_after((k3fd->esd_timestamp  + HZ * 3), jiffies)) {
		return false;
	}

	if (++k3fd->esd_frame_count >= K3_FB_ESD_THRESHOLD) {
		k3fd->esd_frame_count = 0;
		return true;
	} else {
		return false;
	}
}

static int k3fb_esd_set(struct k3_fb_data_type *k3fd)
{
	bool is_timeout = true;
	int delay_count = 0;
	u32 tmp = 0;

	/* check payload write empty */
	while (1) {
		tmp = inp32(k3fd->edc_base + MIPIDSI_CMD_PKT_STATUS_OFFSET);
		if (((tmp & 0x00000005) == 0x00000005) || delay_count > 100) {
			is_timeout = (delay_count > 100) ? true : false;
			delay_count = 0;
			break;
		} else {
			udelay(1);
			++delay_count;
		}
	}

	if (is_timeout) {
		pr_err("k3fb, %s: ESD check payload write empty timeout\n", __func__);
		return -EBUSY;
	}

	/* check dsi stop state */
	while (1) {
		tmp = inp32(k3fd->edc_base + MIPIDSI_PHY_STATUS_OFFSET);
		if (((tmp & 0xA90) == 0xA90) || delay_count > 100) {
			is_timeout = (delay_count > 100) ? true : false;
			delay_count = 0;
			break;
		} else {
			udelay(1);
			++delay_count;
		}
	}

	if (is_timeout) {
		pr_err("k3fb, %s: ESD check dsi stop state timeout\n", __func__);
		return -EBUSY;
	}

	/* reset DSI */
	set_reg(k3fd->edc_base + MIPIDSI_PWR_UP_OFFSET, 0x0, 1, 0);

	/* check clock lane stop state */
	while (1) {
		tmp = inp32(k3fd->edc_base + MIPIDSI_PHY_STATUS_OFFSET);
		if (((tmp & 0x04) == 0x04) || delay_count > 500) {
			is_timeout = (delay_count > 500) ? true : false;
			delay_count = 0;
			break;
		} else {
			udelay(1);
			++delay_count;
		}
	};

	if (is_timeout) {
		pr_err("k3fb, %s: ESD check clock lane stop state timeout\n", __func__);
		/* Should't return. MUST power on DSI */
	}

	/* power on DSI */
	set_reg(k3fd->edc_base + MIPIDSI_PWR_UP_OFFSET, 0x1, 1, 0);

	return 0;
}

static enum hrtimer_restart k3fb_cmd_esd(struct hrtimer *timer)
{
	struct k3_fb_data_type *k3fd = NULL;

	k3fd  = container_of(timer, struct k3_fb_data_type, esd_hrtimer);
	BUG_ON(k3fd == NULL);

	k3fd->esd_hrtimer_enable = true;
	hrtimer_start(&k3fd->esd_hrtimer, ktime_set(0, NSEC_PER_SEC), HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

static void frame_end_workqueue(struct work_struct *work)
{
	struct k3_fb_data_type *k3fd = NULL;
	struct k3_fb_panel_data *pdata = NULL;
	struct fb_info *info = NULL;
	static int err_count = 0;
	int ret = 0;
	u32 level;
	bool is_timeout = true;
	int delay_count = 0;
	u32 tmp = 0;

	k3fd = container_of(work, struct k3_fb_data_type, frame_end_work);
	BUG_ON(k3fd == NULL);
	pdata = (struct k3_fb_panel_data *)k3fd->pdev->dev.platform_data;

	info =  fbi_list[0];
	BUG_ON(info == NULL);

	down(&k3_fb_blank_sem);
	if (!k3fd->panel_power_on) {
		goto error;
	}

	udelay(120);
	if (k3fd->bl_level != k3fd->bl_level_old)
		k3_fb_set_backlight_cmd_mode(k3fd, k3fd->bl_level);

	if (k3fd->esd_hrtimer_enable) {
		/* check dsi stop state */
		while (1) {
			tmp = inp32(k3fd->edc_base + MIPIDSI_PHY_STATUS_OFFSET);
			if (((tmp & 0xA90) == 0xA90) || delay_count > 100) {
				is_timeout = (delay_count > 100) ? true : false;
				delay_count = 0;
				break;
			} else {
				udelay(1);
				++delay_count;
			}
		}

		if (is_timeout) {
			pr_err("k3fb, %s: ESD check dsi stop state timeout\n", __func__);
			goto error;
		}

		/* disable generate High Speed clock */
		set_reg(k3fd->edc_base + MIPIDSI_PHY_IF_CTRL_OFFSET, 0x0, 1, 0);
		/* check panel power status*/
		ret = pdata->check_esd(k3fd->pdev);
		/* enable generate High Speed clock */
		set_reg(k3fd->edc_base + MIPIDSI_PHY_IF_CTRL_OFFSET, 0x1, 1, 0);

		k3fd->esd_hrtimer_enable = false;
	}

error:
#if CLK_SWITCH
	/*Disable ldi clk*/
	clk_disable(k3fd->ldi_clk);
	/*Disable edc0 clk*/
	clk_disable(k3fd->edc_clk);
	#endif

	k3fd->frame_count--;
	up(&k3_fb_blank_sem);
}

static int init_frame_end_workqueue(struct k3_fb_data_type *k3fd)
{
	BUG_ON(k3fd == NULL);

	k3fd->frame_end_wq = create_singlethread_workqueue("frame_end_workqueue");
	if (!k3fd->frame_end_wq) {
		pr_err("k3fb, %s : workqueue create failed !", __FUNCTION__);
		return -1;
	}

	INIT_WORK(&k3fd->frame_end_work, frame_end_workqueue);

	return 0;
}

static irqreturn_t edc_isr_video_mode(struct k3_fb_data_type *k3fd, u32 ints)
{
	/*
	** check interrupt
	** 0x80 for bas_stat_int
	** 0x40 for bas_end_int
	*/
	if ((ints & 0x40) == 0x40) {
		//if (k3fd->index == 0) {
		//	if (k3fd->vsync_event && k3fd->vsync_wq)
		//		queue_work(k3fd->vsync_wq, &k3fd->vsync_work);
		//}
	} else if ((ints & 0x80) == 0x80) {
	k3fd->edc_wait_flash = 1;
	wake_up_interruptible(&k3fd->edc_wait_queque);

		if (k3fd->index == 0) {
			if (k3fd->panel_info.sbl_enable && k3fd->sbl_wq)
				queue_work(k3fd->sbl_wq, &k3fd->sbl_work);

			if (k3fd->panel_info.frc_enable) {
				if (k3fb_frc_prepare(k3fd)) {
					k3fd->ldi_int_type |= FB_LDI_INT_TYPE_FRC;
				}
			}

			if (k3fd->panel_info.esd_enable) {
				if (k3fb_esd_prepare(k3fd)) {
					k3fd->ldi_int_type |= FB_LDI_INT_TYPE_ESD;
				}
			}

			if (k3fd->ldi_int_type != FB_LDI_INT_TYPE_NONE) {
				/* clear ldi interrupt */
				outp32(k3fd->edc_base + LDI_INT_CLR_OFFSET, 0xFFFFFFFF);

				/* enable vfrontporch_end_int */
				set_reg(k3fd->edc_base + LDI_INT_EN_OFFSET, 0x1, 1, 10);
				/* disable ldi */
				set_reg(k3fd->edc_base + LDI_CTRL_OFFSET, 0x0, 1, 0);
			}
		}
	} else {
		k3fb_loge("interrupt(0x%x) is not used!\n", ints);
	}

	return IRQ_HANDLED;
}

static int edc_isr_cmd_mode(struct k3_fb_data_type *k3fd, u32 ints)
{
	if ((ints & 0xC0) == 0xC0) {
		/* handle frame_end int */
		k3fd->cmd_mode_refresh = false;

		if (k3fd->frame_count >= 2) {
			k3fd->is_first_frame_end = true;
			if (k3fd->frame_end_wq)
				queue_work(k3fd->frame_end_wq, &k3fd->frame_end_work);
		}

		/* handle frame_start int */
		k3fd->cmd_mode_refresh = true;

		if (k3fd->frame_count >= 1) {
			k3fd->frame_count++;


			set_MIPIDSI_TE_CTRL_te_mask_en(k3fd->edc_base, 1);
			set_LDI_CTRL_ldi_en(k3fd->edc_base, K3_DISABLE);
			/*set_EDC_DISP_CTL_edc_en(k3fd->edc_base, K3_DISABLE);*/

			k3fd->edc_wait_flash = 1;
			wake_up_interruptible(&k3fd->edc_wait_queque);
		}
	} else 	if ((ints & 0x40) == 0x40) {
		k3fd->cmd_mode_refresh = false;

		if (k3fd->frame_count >= 2) {
			k3fd->is_first_frame_end = true;
			queue_work(k3fd->frame_end_wq, &k3fd->frame_end_work);
		}
	} else if ((ints & 0x80) == 0x80) {
		/*Frame start int*/
		k3fd->cmd_mode_refresh = true;

		if (k3fd->frame_count >= 1) {
			k3fd->frame_count++;
			set_MIPIDSI_TE_CTRL_te_mask_en(k3fd->edc_base, 1);
			set_LDI_CTRL_ldi_en(k3fd->edc_base, K3_DISABLE);

			k3fd->edc_wait_flash = 1;
			wake_up_interruptible(&k3fd->edc_wait_queque);
		}
	}

	return IRQ_HANDLED;
}

static irqreturn_t edc_isr(int irq, void *data)
{
	struct k3_fb_data_type *k3fd = NULL;
	u32 tmp = 0;
	int ret = IRQ_HANDLED;

	k3fd = (struct k3_fb_data_type *)data;
	BUG_ON(k3fd == NULL);

	/* check edc_afifo_underflow_int interrupt */
	tmp = inp32(k3fd->edc_base + LDI_ORG_INT_OFFSET);
	if ((tmp & 0x4) == 0x4) {
		set_reg(k3fd->edc_base + LDI_INT_CLR_OFFSET, 0x1, 1, 2);
		pr_err("k3fb, %s: edc_afifo_underflow_int !!!", __func__);
	}

	tmp = inp32(k3fd->edc_base + EDC_INTS_OFFSET);
	outp32(k3fd->edc_base + EDC_INTS_OFFSET, 0x0);

	if (k3fd->panel_info.type == PANEL_MIPI_CMD)
		ret = edc_isr_cmd_mode(k3fd, tmp);
	else
		ret = edc_isr_video_mode(k3fd, tmp);

	return ret;
}

static irqreturn_t ldi_isr(int irq, void *data)
{
	struct k3_fb_data_type *k3fd = NULL;
	struct k3_fb_panel_data *pdata = NULL;
	int ret = 0;
	u32 ldi_ints = 0;

	k3fd = (struct k3_fb_data_type *)data;
	BUG_ON(k3fd == NULL);
	pdata = (struct k3_fb_panel_data *)k3fd->pdev->dev.platform_data;
	BUG_ON(pdata == NULL);

	if (k3fd->index == 1) {
		pr_err("k3fb, %s: error fb1 do not use ldi interrupt.\n", __func__);
		return IRQ_HANDLED;
	}

	ldi_ints = inp32(k3fd->edc_base + LDI_ORG_INT_OFFSET);

	/* clear ldi interrupt */
	outp32(k3fd->edc_base + LDI_INT_CLR_OFFSET, 0xFFFFFFFF);

	/* check vfrontporch_end_int interrupt*/
	if ((ldi_ints & 0x400) == 0x400) {
		if (k3fd->panel_info.frc_enable &&
			(k3fd->ldi_int_type & FB_LDI_INT_TYPE_FRC)) {
			ret = k3fb_frc_set(k3fd);
			if (ret < 0) {
				k3fb_loge("failed to set frc.\n");
			}
		} else if (k3fd->panel_info.esd_enable &&
			(k3fd->ldi_int_type & FB_LDI_INT_TYPE_ESD)) {
			ret = k3fb_esd_set(k3fd);
			if (ret < 0) {
				k3fb_loge("failed to set esd.\n");
			}
		}
	} else {
		pr_err("interrupt(0x%x) is not used!\n", ldi_ints);
	}

	/* disable vfrontporch_end_int */
	set_reg(k3fd->edc_base + LDI_INT_EN_OFFSET, 0x0, 1, 10);
	/* enable ldi */
	set_reg(k3fd->edc_base + LDI_CTRL_OFFSET, 0x1, 1, 0);

	k3fd->ldi_int_type = FB_LDI_INT_TYPE_NONE;

	return IRQ_HANDLED;
}

static int k3_fb_pan_display_cmd(struct fb_var_screeninfo *var, struct fb_info *info,
	struct k3_fb_data_type *k3fd)
{
	int ret = 0;

#if CLK_SWITCH
	/* enable edc clk */
	clk_enable(k3fd->edc_clk);
	/* enable ldi clk */
	clk_enable(k3fd->ldi_clk);
#endif

	ret = edc_fb_pan_display(var, info, k3fd->graphic_ch);
	if (ret != 0) {
		pr_err("edc_fb_pan_display err!\n");
	}

	/*set_EDC_DISP_CTL_edc_en(k3fd->edc_base, K3_ENABLE);*/
	set_LDI_CTRL_ldi_en(k3fd->edc_base, K3_ENABLE);
	set_MIPIDSI_TE_CTRL_te_mask_en(k3fd->edc_base, 0);

	if (k3fd->frame_count == 0)
		k3fd->frame_count = 1;
	if (wait_event_interruptible_timeout(k3fd->edc_wait_queque, k3fd->edc_wait_flash, HZ / 10) <= 0) {
		pr_err("k3fb, %s: wait_event_interruptible_timeout !\n", __func__);
		k3_fb_blank_sub(FB_BLANK_POWERDOWN, info);
		k3_fb_blank_sub(FB_BLANK_UNBLANK, info);
		k3fd->edc_wait_flash = 0;
		return -ETIME;
	}
	k3fd->edc_wait_flash = 0;

	return ret;
}

static int k3_fb_pan_display_video(struct fb_var_screeninfo *var, struct fb_info *info,
	struct k3_fb_data_type *k3fd)
{
	int ret = 0;

#if K3_FB_OVERLAY_USE_BUF 
	if (!hdmi_is_connected || video_buf.is_video) {
#else
	if (!hdmi_is_connected) {
#endif
		/*fixme: wait how much time*/
		if (!wait_event_interruptible_timeout(k3fd->edc_wait_queque, k3fd->edc_wait_flash, HZ / 10)) {
			pr_warn("k3fb, %s: wait_event_interruptible_timeout !!\n", __func__);
			k3fd->edc_wait_flash = 0;
			return -ETIME;
		} else {
			if (signal_pending(current)) {
				pr_warn("k3fb, %s: wait event signal pending !!\n", __func__);
				k3fd->edc_wait_flash = 0;
				return -ERESTARTSYS;
			}
		}
		k3fd->edc_wait_flash = 0;
	}

	ret = edc_fb_pan_display(var, info, k3fd->graphic_ch);
	if (ret != 0) {
		pr_warn("k3fb, %s: edc_fb_pan_display err!", __func__);
	}

	return ret;
}

static int k3_fb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	if (!k3fd->panel_power_on) {
		return -EPERM;
	}

	if (k3fd->panel_info.type == PANEL_MIPI_CMD)
		ret = k3_fb_pan_display_cmd(var, info, k3fd);
	else
		ret = k3_fb_pan_display_video(var, info, k3fd);

	return ret;
}


static int k3fb_overlay_get(struct fb_info *info, void __user *p)
{
	int ret = 0;
	struct overlay_info req;

	BUG_ON(info == NULL);

	if (copy_from_user(&req, p, sizeof(req))) {
		pr_err("k3fb, %s: copy from user failed \n", __func__);
		return -EFAULT;
	}

	ret = edc_overlay_get(info, &req);
	if (ret) {
		pr_err("k3fb, %s: edc_overlay_get ioctl failed \n", __func__);
		return ret;
	}

	if (copy_to_user(p, &req, sizeof(req))) {
		pr_err("k3fb, %s: copy2user failed \n", __func__);
		return -EFAULT;
	}

	return ret;
}

static int k3fb_overlay_set(struct fb_info *info, void __user *p)
{
	int ret = 0;
	struct overlay_info req;

	BUG_ON(info == NULL);

	if (copy_from_user(&req, p, sizeof(req))) {
		pr_err("k3fb, %s: copy from user failed \n", __func__);
		return -EFAULT;
	}

	ret = edc_overlay_set(info, &req);
	if (ret) {
		pr_err("k3fb, %s: k3fb_overlay_set ioctl failed, rc=%d\n", __func__, ret);
		return ret;
	}

	return ret;
}

static int k3fb_overlay_unset(struct fb_info *info, unsigned long *argp)
{
	int ret = 0;
	int ndx = 0;

	BUG_ON(info == NULL);

	ret = copy_from_user(&ndx, argp, sizeof(ndx));
	if (ret) {
		pr_err("k3fb, %s: copy from user failed \n", __func__);
		return ret;
	}

#if K3_FB_OVERLAY_USE_BUF
	if (video_buf.is_init && video_buf.is_video) {
		video_buf.is_video = false;
		mutex_lock(&video_buf.overlay_mutex);
		reset_video_buf();
		mutex_unlock(&video_buf.overlay_mutex);
	}
#endif
	return edc_overlay_unset(info, ndx);
}

static int k3fb_overlay_play(struct fb_info *info, unsigned long *argp)
{
	int ret = 0;
	struct overlay_data req;
	struct k3_fb_data_type *k3fd = NULL;

#if K3_FB_OVERLAY_USE_BUF
	static int count = 0;
	overlay_video_data *video_data = NULL;
	u32 video_index = 0;
	int i = 0;
	int min_count = 0;
	int set_count = 0;
#endif

	BUG_ON(info == NULL);

	ret = copy_from_user(&req, argp, sizeof(req));
	if (ret) {
		pr_err("k3fb, %s: copy from user failed \n", __func__);
		return ret;
	}

	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

#if K3_FB_OVERLAY_USE_BUF
	if ((k3fd->index == 1) && hdmi_is_connected && (req.src.is_video == 1)) {

		mutex_lock(&video_buf.overlay_mutex);
		if (!video_buf.is_video) {
			pr_info("k3fb: begin play video\n");
		}
		video_buf.is_video = true;
		count ++;

		if (video_buf.last_addr == req.src.phy_addr) {
			mutex_unlock(&video_buf.overlay_mutex);
			pr_info("k3fb same buf phys:0x%x\n", req.src.phy_addr);
			return 0;
		}

		for (i = 0; i < MAX_OVERLAY_BUF_NUM; i++) {
			video_data = &video_buf.video_data_list[i];
			if (video_data->is_set) {
				set_count++;
			}

			if (0 == i || video_data->count < min_count) {
				min_count = video_data->count;
				video_index = i;
			}
		}
		
		if (MAX_OVERLAY_BUF_NUM == set_count) {
			pr_warn("k3fb video buff full @@@@@\n");
		}

		video_data = &video_buf.video_data_list[video_index];
		memcpy(&video_data->data, &req, sizeof(req));
		video_data->is_set = true;
		video_data->count = count;

		video_buf.last_addr = req.src.phy_addr;
		//pr_info("k3fb: buf index:%d count:%d\n", video_index, count);
		mutex_unlock(&video_buf.overlay_mutex);
		return 0;
	} else if((k3fd->index == 1) && video_buf.is_video){ 
		pr_info("k3fb: exit play video req.is_video:%d buf.is_video:%d is_connected:%d\n",req.src.is_video, video_buf.is_video, hdmi_is_connected);
		video_buf.is_video = false;
		mutex_lock(&video_buf.overlay_mutex);
		count = 0;
		reset_video_buf();
		mutex_unlock(&video_buf.overlay_mutex);
	} 
#endif

	if (k3fd->index == 1 && hdmi_is_connected && (!req.src.is_video)) {
		if (!wait_event_interruptible_timeout(k3fd->edc_wait_queque, k3fd->edc_wait_flash, HZ / 10)) {
			pr_warn("k3fb, %s: wait_event_interruptible_timeout !!\n", __func__);
			k3fd->edc_wait_flash = 0;
			return -ETIME;
		} else {
			if (signal_pending(current)) {
				pr_warn("k3fb, %s: wait event signal pending !!\n", __func__);
				k3fd->edc_wait_flash = 0;
				return -ERESTARTSYS;
			}
		}
		k3fd->edc_wait_flash = 0;
	}

	return edc_overlay_play(info, &req);
}

static int k3fb_set_timing(struct fb_info *info, unsigned long *argp)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;
	struct k3_fb_panel_data *pdata = NULL;
	struct fb_var_screeninfo var;
	u32 edc_base = 0;

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	edc_base = k3fd->edc_base;

	pdata = (struct k3_fb_panel_data *)k3fd->pdev->dev.platform_data;
	if ((!pdata) || (!pdata->set_timing)) {
		pr_err("k3fb, %s: no panel operation detected!\n", __func__);
		return -ENODEV;
	}

	if (!k3fd->panel_power_on) {
		pr_err("k3fb, %s: panel power off!\n", __func__);
		return -EPERM;
	}

	ret = copy_from_user(&var, argp, sizeof(var));
	if (ret) {
		pr_err("k3fb, %s: copy from user failed \n", __func__);
		return ret;
	}

	memcpy(&info->var, &var, sizeof(var));
	k3fd->panel_info.xres = var.xres;
	k3fd->panel_info.yres = var.yres;

	k3fd->panel_info.clk_rate = (var.pixclock == 0) ? k3fd->panel_info.clk_rate : var.pixclock;
	k3fd->panel_info.ldi.h_front_porch = var.right_margin;
	k3fd->panel_info.ldi.h_back_porch = var.left_margin;
	k3fd->panel_info.ldi.h_pulse_width = var.hsync_len;
	k3fd->panel_info.ldi.v_front_porch = var.lower_margin;
	k3fd->panel_info.ldi.v_back_porch = var.upper_margin;
	k3fd->panel_info.ldi.v_pulse_width = var.vsync_len;
	k3fd->panel_info.ldi.vsync_plr = hdmi_get_vsync_bycode(var.reserved[3]);
	k3fd->panel_info.ldi.hsync_plr = hdmi_get_hsync_bycode(var.reserved[3]);

	/* Note: call clk_set_rate after clk_set_rate, set edc clock rate to normal value */
	if (clk_set_rate(k3fd->edc_clk, k3fd->panel_info.clk_rate * 12 / 10) != 0) {
		pr_err("k3fb, %s: failed to set edc clk rate(%d).\n", __func__, k3fd->panel_info.clk_rate * 12 / 10);
	}

	set_EDC_DISP_SIZE(edc_base, k3fd->panel_info.xres, k3fd->panel_info.yres);

	if (pdata->set_timing(k3fd->pdev) != 0) {
		pr_err("k3fb, %s: set timing failed!", __func__);
	}

	return 0;
}

static int k3fb_set_playvideo(struct fb_info *info, unsigned long *argp)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;
	struct k3_fb_panel_data *pdata = NULL;
	int gamma = 0;

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	pdata = (struct k3_fb_panel_data *)k3fd->pdev->dev.platform_data;
	if ((!pdata) || (!pdata->set_playvideo)) {
		pr_err("k3fb, %s: no panel operation detected!\n", __func__);
		return -ENODEV;
	}

	if (!k3fd->panel_power_on) {
		pr_err("k3fb, %s: panel power off!\n", __func__);
		return -EPERM;
	}

	ret = copy_from_user(&gamma, argp, sizeof(gamma));
	if (ret) {
		pr_err("k3fb, %s: copy from user failed \n", __func__);
		return ret;
	}

	if (pdata->set_playvideo(k3fd->pdev, gamma) != 0) {
		pr_err("k3fb, %s: set timing failed!", __func__);
	}

	return 0;
}

static int k3_fb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = 0;

	BUG_ON(info == NULL);

	down(&k3_fb_overlay_sem);
	switch (cmd) {
	case K3FB_OVERLAY_GET:
		ret = k3fb_overlay_get(info, argp);
		break;
	case K3FB_OVERLAY_SET:
		ret = k3fb_overlay_set(info, argp);
		break;
	case K3FB_OVERLAY_UNSET:
		ret = k3fb_overlay_unset(info, argp);
		break;
	case K3FB_OVERLAY_PLAY:
		ret = k3fb_overlay_play(info, argp);
		break;
	case K3FB_TIMING_SET:
		ret = k3fb_set_timing(info, argp);
		break;
	case K3FB_PLAYVIDEO_SET:
		ret = k3fb_set_playvideo(info, argp);
		break;
	default:
		pr_err("k3fb, %s: unknown ioctl (cmd=%d) received!\n", __func__, cmd);
		ret = -EINVAL;
		break;
	}

	up(&k3_fb_overlay_sem);
	return ret;
}

static int k3_fb_blank(int blank_mode, struct fb_info *info)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	pr_info("k3fb, %s: enter!\n", __func__);

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	ret = k3_fb_blank_sub(blank_mode, info);
	if (ret != 0) {
		pr_err("k3fb, %s: blank mode %d failed!\n", __func__, blank_mode);
	}

	pr_info("k3fb, %s: exit!\n", __func__);

	return ret;
}

int k3_fb1_blank(int blank_mode)
{
	int ret = 0;
	struct fb_info *info = fbi_list[1];
	struct k3_fb_data_type *k3fd = NULL;

	pr_info("k3fb, %s: enter!\n", __func__);

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	ret = k3_fb_blank_sub(blank_mode, info);
	if (ret != 0) {
		pr_err("k3fb, %s: blank mode %d failed!\n", __func__, blank_mode);
	}

	if (blank_mode == FB_BLANK_UNBLANK) {
		k3_fb_set_backlight(k3fd, k3fd->bl_level);
	}

	pr_info("k3fb, %s: exit!\n", __func__);

	return ret;
}

int k3fb_buf_isfree(int phys)
{
	int i = 0;
	int ret = 1;

#if K3_FB_OVERLAY_USE_BUF
	struct fb_info *info = fbi_list[1];
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(info == NULL);
	k3fd = (struct k3_fb_data_type *)info->par;
	BUG_ON(k3fd == NULL);

	if (!k3fd->panel_power_on || phys == 0 || !video_buf.is_init || !hdmi_is_connected) {
		return ret;
	}

	mutex_lock(&video_buf.overlay_mutex);

	if (video_buf.write_addr == phys || video_buf.play_addr == phys) {
		//pr_info("k3fb, %s: addr:0x%x is playing!", __func__, phys);
		ret = 0;
		goto done;
	}
	for (i = 0; i < MAX_OVERLAY_BUF_NUM; i++) {
		if (video_buf.video_data_list[i].is_set 
		   && (video_buf.video_data_list[i].data.src.phy_addr == phys)) {
			//pr_info("k3fb, %s: addr:0x%x in buf!", __func__, phys);
			ret = 0;
			goto done;
		}
	}
	
done:
	mutex_unlock(&video_buf.overlay_mutex);
#endif
	return ret;
}

void k3fb_set_hdmi_state(bool is_connected)
{
	if (hdmi_is_connected == is_connected) {
		return;
	}
    pr_info("k3fb: hdmi_is_connected: %d is_connected: %d\n",hdmi_is_connected , is_connected);
	hdmi_is_connected = is_connected;
#if K3_FB_OVERLAY_USE_BUF
	if (video_buf.is_init) {
		mutex_lock(&video_buf.overlay_mutex);
		reset_video_buf();	
		mutex_unlock(&video_buf.overlay_mutex);

        if (hdmi_is_connected) {
            video_buf.exit_work = 0;
            queue_work(video_buf.play_wq, &video_buf.play_work);
        } else {
            video_buf.exit_work = 1;
        }
	}
#endif
	return;
}

struct fb_var_screeninfo * k3fb_get_fb_var(int index)
{
	struct fb_info *info = fbi_list[index];

	BUG_ON(info == NULL);

	return &info->var;
}

EXPORT_SYMBOL(k3_fb1_blank);
EXPORT_SYMBOL(k3fb_buf_isfree);
EXPORT_SYMBOL(k3fb_set_hdmi_state);
EXPORT_SYMBOL(k3fb_get_fb_var);


/******************************************************************************/

#ifdef CONFIG_HAS_EARLYSUSPEND
static void k3fb_early_suspend(struct early_suspend *h)
{
	struct k3_fb_data_type *k3fd = container_of(h, struct k3_fb_data_type, early_suspend);

	BUG_ON(k3fd == NULL);

	pr_info("k3fb, %s: enter!\n", __func__);

	if (k3fd->index == 0) {
		if (k3_fb_blank_sub(FB_BLANK_POWERDOWN, k3fd->fbi) != 0) {
			pr_err("k3fb, %s: failed to early suspend!\n", __func__);
		}
	} else if (k3fd->panel_power_on) {
		edc_fb_disable(k3fd->fbi);
	}

	pr_info("k3fb, %s: exit!\n", __func__);
}

static void k3fb_late_resume(struct early_suspend *h)
{
	struct k3_fb_data_type *k3fd = container_of(h, struct k3_fb_data_type, early_suspend);

	BUG_ON(k3fd == NULL);

	pr_info("k3fb, %s: enter!\n", __func__);

	if (k3fd->index == 0) {
		if (k3_fb_blank_sub(FB_BLANK_UNBLANK, k3fd->fbi) != 0) {
			pr_err("k3fb, %s: failed to late resume!\n", __func__);
		}
	} else if (k3fd->panel_power_on) {
		edc_fb_enable(k3fd->fbi);
	}

	pr_info("k3fb, %s: exit!\n", __func__);
}
#endif


/******************************************************************************/
static struct fb_ops k3_fb_ops = {
	.owner = THIS_MODULE,
	.fb_open = k3_fb_open,
	.fb_release = k3_fb_release,
	.fb_read = NULL,
	.fb_write = NULL,
	.fb_cursor = NULL,
	.fb_check_var = k3_fb_check_var,  /* vinfo check */
	.fb_set_par = k3_fb_set_par,  /* set the video mode according to info->var */
	.fb_setcolreg = NULL,  /* set color register */
	.fb_blank = k3_fb_blank, /*blank display */
	.fb_pan_display = k3_fb_pan_display,  /* pan display */
	.fb_fillrect = NULL,  /* Draws a rectangle */
	.fb_copyarea = NULL,  /* Copy data from area to another */
	.fb_imageblit = NULL,  /* Draws a image to the display */
	.fb_rotate = NULL,
	.fb_sync = NULL,  /* wait for blit idle, optional */
	.fb_ioctl = k3_fb_ioctl,  /* perform fb specific ioctl (optional) */
	.fb_mmap = NULL,
};

static void k3_fb_set_bl_brightness(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	struct k3_fb_data_type *k3fd = dev_get_drvdata(led_cdev->dev->parent);
	int bl_lvl;

	if (value > MAX_BACKLIGHT_BRIGHTNESS)
		value = MAX_BACKLIGHT_BRIGHTNESS;

	bl_lvl = value;
	k3fd->bl_level = value;
	k3fd->bl_level_sbl = value;

	if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM) {
		/*
		** This maps android backlight level 0 to 255 into
		** driver backlight level 0 to bl_max with rounding
		** bl_lvl = (2 * value * k3fd->panel_info.bl_max + MAX_BACKLIGHT_BRIGHTNESS)
		** (2 * MAX_BACKLIGHT_BRIGHTNESS);
		*/
		bl_lvl = (value * k3fd->panel_info.bl_max) / MAX_BACKLIGHT_BRIGHTNESS;
		bl_lvl &= 0xFF;

		if (!bl_lvl && value)
			bl_lvl = 1;

		k3fd->bl_level = bl_lvl;
		k3fd->bl_level_sbl = bl_lvl;
	}

	if ((k3fd->panel_info.type == PANEL_MIPI_CMD)
		&& (!k3fd->is_first_frame_end))
			return;

		if (k3fd->panel_info.sbl_enable)
			set_sbl_bkl(k3fd, value);
		k3_fb_set_backlight(k3fd, k3fd->bl_level);
}

static struct led_classdev backlight_led = {
	.name = "lcd_backlight0",
	.brightness = MAX_BACKLIGHT_BRIGHTNESS,
	.brightness_set = k3_fb_set_bl_brightness,
};

static int k3_fb_init_par(struct k3_fb_data_type *k3fd, int pixel_fmt)
{
	struct fb_info *fbi = NULL;
	struct fb_var_screeninfo *var = NULL;
	struct fb_fix_screeninfo *fix = NULL;
	int bpp = 0;

	BUG_ON(k3fd == NULL);

	fbi = k3fd->fbi;
	fix = &fbi->fix;
	var = &fbi->var;

	fix->type_aux = 0;	/* if type == FB_TYPE_INTERLEAVED_PLANES */
	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->visual = FB_VISUAL_TRUECOLOR;	/* True Color */
	fix->xpanstep = 1;
	fix->ypanstep = 1;
	fix->ywrapstep = 0;  /* No support */
	fix->mmio_start = 0;  /* No MMIO Address */
	fix->mmio_len = 0;	/* No MMIO Address */
	fix->accel = FB_ACCEL_NONE;  /* No hardware accelerator */

	var->xoffset = 0;  /* Offset from virtual to visible */
	var->yoffset = 0;  /* resolution */
	var->grayscale = 0;  /* No graylevels */
	var->nonstd = 0;  /* standard pixel format */
	/*var->activate = FB_ACTIVATE_NOW;*/
	var->activate = FB_ACTIVATE_VBL;  /* activate it at vsync */
	var->height = -1;  /* height of picture in mm */
	var->width = -1;  /* width of picture in mm */
	var->accel_flags = 0;  /* acceleration flags */
	var->sync = 0;	 /* see FB_SYNC_* */
	var->rotate = 0;   /* angle we rotate counter clockwise */
	var->vmode = FB_VMODE_NONINTERLACED;

	switch (pixel_fmt) {
	case IMG_PIXEL_FORMAT_RGBX_5551:
		var->blue.offset = 0;
		var->green.offset = 5;
		var->red.offset = 10;
		var->transp.offset = 15;
		var->blue.length = 5;
		var->green.length = 5;
		var->red.length = 5;
		var->transp.length = 0;
		var->blue.msb_right = 0;
		var->green.msb_right = 0;
		var->red.msb_right = 0;
		var->transp.msb_right = 0;
		bpp = 2;
		k3fd->fb_imgType = EDC_ARGB_1555;
		break;
	case IMG_PIXEL_FORMAT_RGBA_5551:
		var->blue.offset = 0;
		var->green.offset = 5;
		var->red.offset = 10;
		var->transp.offset = 15;
		var->blue.length = 5;
		var->green.length = 5;
		var->red.length = 5;
		var->transp.length = 1;
		var->blue.msb_right = 0;
		var->green.msb_right = 0;
		var->red.msb_right = 0;
		var->transp.msb_right = 0;
		bpp = 2;
		k3fd->fb_imgType = EDC_ARGB_1555;
		break;
	case IMG_PIXEL_FORMAT_RGB_565:
		var->blue.offset = 0;
		var->green.offset = 5;
		var->red.offset = 11;
		var->transp.offset = 0;
		var->blue.length = 5;
		var->green.length = 6;
		var->red.length = 5;
		var->transp.length = 0;
		var->blue.msb_right = 0;
		var->green.msb_right = 0;
		var->red.msb_right = 0;
		var->transp.msb_right = 0;
		bpp = 2;
		k3fd->fb_imgType = EDC_RGB_565;
		break;
	case IMG_PIXEL_FORMAT_RGBX_8888:
		var->blue.offset = 0;
		var->green.offset = 8;
		var->red.offset = 16;
		var->transp.offset = 24;
		var->blue.length = 8;
		var->green.length = 8;
		var->red.length = 8;
		var->transp.length = 0;
		var->blue.msb_right = 0;
		var->green.msb_right = 0;
		var->red.msb_right = 0;
		var->transp.msb_right = 0;
		bpp = 4;
		k3fd->fb_imgType = EDC_XRGB_8888;
		break;
	case IMG_PIXEL_FORMAT_RGBA_8888:
		var->blue.offset = 0;
		var->green.offset = 8;
		var->red.offset = 16;
		var->transp.offset = 24;
		var->blue.length = 8;
		var->green.length = 8;
		var->red.length = 8;
		var->transp.length = 8;
		var->blue.msb_right = 0;
		var->green.msb_right = 0;
		var->red.msb_right = 0;
		var->transp.msb_right = 0;
		bpp = 4;
		k3fd->fb_imgType = EDC_ARGB_8888;
		break;
	default:
		pr_err("k3fb, %s: fb %d unkown image type!\n", __func__, k3fd->index);
		return -EINVAL;
	}

	var->xres = k3fd->panel_info.xres;
	var->yres = k3fd->panel_info.yres;
	var->xres_virtual = k3fd->panel_info.xres;
	var->yres_virtual = k3fd->panel_info.yres * K3_NUM_FRAMEBUFFERS;
	var->bits_per_pixel = bpp * 8;

	fix->line_length = k3_fb_line_length(k3fd->index, var->xres_virtual, bpp);
	fix->smem_len = roundup(fix->line_length * var->yres_virtual, PAGE_SIZE);

	/* id field for fb app */
	snprintf(fix->id, sizeof(fix->id), "k3fb%d", k3fd->index);

	fbi->fbops = &k3_fb_ops;
	fbi->flags = FBINFO_FLAG_DEFAULT; /* FBINFO_HWACCEL_YPAN | FBINFO_HWACCEL_XPAN */
	fbi->pseudo_palette = k3_fb_pseudo_palette;

	if (k3fd->index == 0) {
		fbi->fix.smem_start = (int32_t)K3_FB_PA;
		fbi->screen_size = fix->smem_len;
		fbi->screen_base = ioremap(fbi->fix.smem_start, fbi->fix.smem_len);
		/*memset(fbi->screen_base, 0x0, fbi->fix.smem_len);*/
	} else {
		fbi->fix.smem_start = fbi_list[0]->fix.smem_start;
		fbi->screen_size = fbi_list[0]->screen_size;
		fbi->screen_base = fbi_list[0]->screen_base;
	}

	return 0;
}

static int k3_fb_register(struct k3_fb_data_type *k3fd)
{
	int ret = 0;
#ifdef CONFIG_FASTBOOT_ENABLE
	struct k3_fb_panel_data *pdata = NULL;
#endif
	struct fb_info *fbi = NULL;
	struct fb_fix_screeninfo *fix = NULL;

	BUG_ON(k3fd == NULL);

	/* fb info initialization */
	fbi = k3fd->fbi;
	fix = &fbi->fix;

	ret = k3_fb_init_par(k3fd, IMG_PIXEL_FORMAT_RGBA_8888);
	if (ret != 0) {
		pr_err("k3fb, %s: fb %d k3_fb_init_par failed!\n", __func__, k3fd->index);
		return ret;
	}

	/* init edc overlay, only intialize one time */
	edc_overlay_init(&k3fd->ctrl);

	k3fd->fb_bgrFmt = K3FB_DEFAULT_BGR_FORMAT;
	k3fd->ref_cnt = 0;
	k3fd->panel_power_on = false;
	sema_init(&k3fd->sem, 1);
	k3fd->bl_enable_mipi_eco = 0;
	if (!(k3fd->panel_info.bl_set_type & BL_SET_BY_PWM)) {
		k3fd->bl_level = 102;
	} else {
		k3fd->bl_level = 40;
	}
	k3fd->ldi_int_type = FB_LDI_INT_TYPE_NONE;	
	k3fd->cmd_mode_refresh = false;

	/* register framebuffer */
	if (register_framebuffer(fbi) < 0) {
		pr_err("k3fb, %s: not able to register framebuffer %d!\n", __func__, k3fd->index);
		return -EPERM;
	}

	/* request edc irq */
	k3fd->edc_wait_flash = 0;
	init_waitqueue_head(&k3fd->edc_wait_queque);
	snprintf(k3fd->edc_irq_name, sizeof(k3fd->edc_irq_name), "%s_edc", fix->id);
	ret = request_irq(k3fd->edc_irq, edc_isr, IRQF_SHARED, k3fd->edc_irq_name, (void *)k3fd);
	if (ret != 0) {
		pr_err("k3fb, %s:  fb %d unable to request edc irq\n", __func__, k3fd->index);
	}

		disable_irq(k3fd->edc_irq);

	/* register edc_irq to core 1 */
	k3v2_irqaffinity_register(k3fd->edc_irq, 1);

	/* request ldi irq */
	snprintf(k3fd->ldi_irq_name, sizeof(k3fd->ldi_irq_name), "%s_ldi", fix->id);
	ret = request_irq(k3fd->ldi_irq, ldi_isr, IRQF_SHARED, k3fd->ldi_irq_name, (void *)k3fd);
	if (ret != 0) {
		pr_err("k3fb, %s:  fb %d unable to request ldi irq\n", __func__, k3fd->index);
	}

	disable_irq(k3fd->ldi_irq);

	/* register ldi_irq to core 1 */
	k3v2_irqaffinity_register(k3fd->ldi_irq, 1);

	if (k3fd->index == 0) {
		if (k3fd->panel_info.frc_enable) {
			k3fd->frc_frame_count = 0;
			k3fd->frc_flag = K3_FB_FRC_NONE_PLAYING;
			k3fd->frc_timestamp = jiffies;
		}

		if (k3fd->panel_info.esd_enable) {
			k3fd->esd_timestamp = jiffies;
			k3fd->esd_frame_count = 0;
		}

		if (k3fd->panel_info.sbl_enable) {
			init_sbl_workqueue(k3fd);
		}

		init_frame_end_workqueue(k3fd);
	} else if (k3fd->index == 1) {
	#if K3_FB_OVERLAY_USE_BUF
		overlay_play_work(k3fd);
	#endif
	} else {
		k3fb_loge("fb%d not support now!\n", k3fd->index);
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	k3fd->early_suspend.suspend = k3fb_early_suspend;
	k3fd->early_suspend.resume = k3fb_late_resume;
	k3fd->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 2;
	register_early_suspend(&k3fd->early_suspend);
	hrtimer_init(&k3fd->esd_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	k3fd->esd_hrtimer.function = k3fb_cmd_esd;
	hrtimer_start(&k3fd->esd_hrtimer, ktime_set(0, NSEC_PER_SEC), HRTIMER_MODE_REL);
#endif

#ifdef CONFIG_FASTBOOT_ENABLE
	if (k3fd->index == 0) {
		pdata = (struct k3_fb_panel_data *)k3fd->pdev->dev.platform_data;

		k3fd->ref_cnt++;
		k3fd->panel_power_on = true;
		if (pdata && pdata->set_fastboot) {
			pdata->set_fastboot(k3fd->pdev);
		}

		set_EDC_INTE(k3fd->edc_base, 0xFFFFFFFF);
		set_EDC_INTS(k3fd->edc_base, 0x0);

		/* enable edc irq */
		if (k3fd->edc_irq)
			enable_irq(k3fd->edc_irq);
		/* enable ldi irq */
		if (k3fd->ldi_irq)
			enable_irq(k3fd->ldi_irq);

		if (k3fd->panel_info.type == PANEL_MIPI_CMD) {
		#if CLK_SWITCH
			/* disable ldi clk */
			clk_disable(k3fd->ldi_clk);
			/* disable edc0 clk */
			clk_disable(k3fd->edc_clk);
		#endif
		}

		set_EDC_INTE(k3fd->edc_base, 0xFFFFFF3F);
	}
#endif

	return ret;
}

/******************************************************************************/
static int k3_fb_probe(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;
	struct resource *res = 0;
	int ret = 0;

	if ((pdev->id == 0) && (pdev->num_resources > 0)) {
		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, IRQ_EDC0_NAME);
		if (!res) {
			dev_err(&pdev->dev, "k3fb, %s: failed to get irq_edc0 resource\n", __func__);
			return -ENXIO;
		}
		k3fd_irq_edc0 = res->start;

		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, IRQ_EDC1_NAME);
		if (!res) {
			dev_err(&pdev->dev, "k3fb, %s: failed to get irq_edc1 resource\n", __func__);
			return -ENXIO;
		}
		k3fd_irq_edc1 = res->start;

		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, IRQ_LDI0_NAME);
		if (!res) {
			dev_err(&pdev->dev, "k3fb, %s: failed to get irq_ldi0 resource\n", __func__);
			return -ENXIO;
		}
		k3fd_irq_ldi0 = res->start;

		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, IRQ_LDI1_NAME);
		if (!res) {
			dev_err(&pdev->dev, "k3fb, %s: failed to get irq_ldi1 resource\n", __func__);
			return -ENXIO;
		}
		k3fd_irq_ldi1 = res->start;

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, REG_BASE_EDC0_NAME);
		if (!res) {
			dev_err(&pdev->dev, "k3fb, %s: failed to get reg_base_edc0 resource\n", __func__);
			return -ENXIO;
		}
		k3fd_reg_base_edc0 = IO_ADDRESS(res->start);

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, REG_BASE_EDC1_NAME);
		if (!res) {
			dev_err(&pdev->dev, "k3fb, %s: failed to get reg_base_edc1 resource\n", __func__);
			return -ENXIO;
		}
		k3fd_reg_base_edc1 = IO_ADDRESS(res->start);

		k3_fb_resource_initialized = 1;
		return 0;
	}

	if (!k3_fb_resource_initialized) {
		pr_err("k3fb, %s: fb resource not initialized!\n", __func__);
		return -EPERM;
	}

	if (pdev_list_cnt >= K3_FB_MAX_DEV_LIST) {
		pr_err("k3fb, %s: too many fb devices!\n", __func__);
		return -ENOMEM;
	}

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	/* edc clock */
	if (k3fd->edc_base == k3fd_reg_base_edc0) {
		k3fd->edc_clk = clk_get(NULL, CLK_EDC0_NAME);
	} else {
		k3fd->edc_clk = clk_get(NULL, CLK_EDC1_NAME);
	}
	k3fd->edc_clk_rst= clk_get(NULL, CLK_EDC0_RST_NAME);

	if (IS_ERR(k3fd->edc_clk)) {
		dev_err(&k3fd->pdev->dev, "k3fb, %s: failed to get edc_clk!\n", __func__);
		return PTR_ERR(k3fd->edc_clk);
	}

	ret = clk_set_rate(k3fd->edc_clk, k3fd->panel_info.clk_rate * 12 / 10);
	if (ret != 0) {
		pr_err("k3fb, %s: failed to set edc clk rate(%d).\n", __func__, k3fd->panel_info.clk_rate * 12 / 10);
	#ifndef CONFIG_MACH_TC45MSU3
		return ret;
	#endif
	}

#ifdef CONFIG_G2D
	/*G2D clock*/
	k3fd->g2d_clk = clk_get(NULL, CLK_G2D_NAME);
	if (IS_ERR(k3fd->g2d_clk)) {
		dev_err(&k3fd->pdev->dev, "k3fb, %s: failed to get g2d_clk!\n", __func__);
		return PTR_ERR(k3fd->g2d_clk);
	}
#endif

	/* edc1 vcc */
	if (k3fd->index == 1) {
		k3fd->edc_vcc = regulator_get(NULL,  VCC_EDC1_NAME);
		if (IS_ERR(k3fd->edc_vcc)) {
			pr_err("k3fb, %s: failed to get edc1-vcc regulator\n", __func__);
			return PTR_ERR(k3fd->edc_vcc);
		}
	}

	if (k3fd->index == 0) {
		k3_fb_sysfs_create(pdev);
	}

	k3fd->ldi_int_type = FB_LDI_INT_TYPE_NONE;

	/* fb register */
	ret = k3_fb_register(k3fd);
	if (ret != 0) {
		pr_err("k3fb, %s: fb register failed!\n", __func__);
		return ret;
	}

	/* android supports only one lcd-backlight/lcd for now */
	if (!lcd_backlight_registered) {
		if (led_classdev_register(&pdev->dev, &backlight_led))
			pr_err("k3fb, %s: led_classdev_register failed\n", __func__);
		else
			lcd_backlight_registered = 1;
	}

	pdev_list[pdev_list_cnt++] = pdev;

	return 0;
}

static int k3_fb_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;
	struct k3_fb_panel_data *pdata = NULL;

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	if (!k3fd) {
		return 0;
	}

	pr_info("k3fb, %s: enter!\n", __func__);
	
	pdata = (struct k3_fb_panel_data *)k3fd->pdev->dev.platform_data;	
	if (!pdata) {
		pr_err("k3fb, %s: k3_fb_panel_data is null!\n", __func__);
		return -ENODEV;
	}

	ret = k3_fb_blank_sub(FB_BLANK_POWERDOWN, k3fd->fbi);
	if (ret != 0) {
		pr_err("k3fb, %s: can't stop the device %d\n", __func__, k3fd->index);
	}

	if (pdata->remove) {
		ret = pdata->remove(k3fd->pdev);
		if (ret != 0) {
			pr_err("k3fb, %s: no panel operation remove detected!\n", __func__);
		}
	}

	/* put edc clock */
	if (!IS_ERR(k3fd->edc_clk)) {
		clk_put(k3fd->edc_clk);
	}

	/* put g2d clock*/
	if (!IS_ERR(k3fd->g2d_clk)) {
		clk_put(k3fd->g2d_clk);
	}

	/* put edc vcc */
	if (k3fd->index == 1) {
#if K3_FB_OVERLAY_USE_BUF
		if(video_buf.play_wq) {
			video_buf.exit_work = 1;
			destroy_workqueue(video_buf.play_wq);
		}
#endif
		if (!IS_ERR(k3fd->edc_vcc)) {
			regulator_put(k3fd->edc_vcc);
		}
	}

	if (k3fd->index == 0 && k3fd->panel_info.frc_enable) {
		k3_fb_sysfs_remove(pdev);
	}

	if (k3fd->index == 0) {
		if (k3fd->panel_info.sbl_enable && k3fd->sbl_wq) {
			destroy_workqueue(k3fd->sbl_wq);
			k3fd->sbl_wq = NULL;
		}
	}

	if (lcd_backlight_registered) {
		lcd_backlight_registered = 0;
		led_classdev_unregister(&backlight_led);
	}

	/* remove /dev/fb* */
	ret = unregister_framebuffer(k3fd->fbi);
	if (ret != 0) {
		pr_err("k3fb, %s: can't unregister framebuffer %d\n", __func__, k3fd->index);
	}

	pr_info("k3fb, %s: exit!\n", __func__);

	return ret;
}

static void k3_fb_shutdown(struct platform_device *pdev)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	/* BUG_ON(k3fd == NULL); */
	if (!k3fd) {
		return;
	}

	pr_info("k3fb, %s: enter!\n", __func__);

	ret = k3_fb_blank_sub(FB_BLANK_POWERDOWN, k3fd->fbi);
	if (ret != 0) {
		pr_err("k3fb, %s: can't stop the device %d\n", __func__, k3fd->index);
	}
	
	pr_info("k3fb, %s: exit!\n", __func__);
}

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
static int k3_fb_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	/* BUG_ON(k3fd == NULL); */
	if (!k3fd) {
		return 0;
	}

	pr_info("k3fb, %s: enter!\n", __func__);

	ret = k3_fb_blank_sub(FB_BLANK_POWERDOWN, k3fd->fbi);
	if (ret != 0) {
		pr_err("k3fb, %s: failed to suspend! %d.\n", __func__, ret);
		fb_set_suspend(k3fd->fbi, FBINFO_STATE_RUNNING);
	} else {
		pdev->dev.power.power_state = state;
	}

	pr_info("k3fb, %s: exit!\n", __func__);

	return ret;
}

static int k3_fb_resume(struct platform_device *pdev)
{
	/* This resume function is called when interrupt is enabled. */
	int ret = 0;
	struct k3_fb_data_type *k3fd = NULL;

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	/* BUG_ON(k3fd == NULL); */
	if (!k3fd) {
		return 0;
	}

	pr_info("k3fb, %s: enter!\n", __func__);

	ret = k3_fb_blank_sub(FB_BLANK_UNBLANK, k3fd->fbi);
	if (ret != 0) {
		pr_err("k3fb, %s: failed to resume! %d\n", __func__, ret);
	}
	pdev->dev.power.power_state = PMSG_ON;
	fb_set_suspend(k3fd->fbi, FBINFO_STATE_RUNNING);

	pr_info("k3fb, %s: exit!\n", __func__);

	return ret;
}
#else
#define k3_fb_suspend NULL
#define k3_fb_resume NULL
#endif


/******************************************************************************/

static struct platform_driver k3_fb_driver = {
	.probe = k3_fb_probe,
	.remove = k3_fb_remove,
	.suspend = k3_fb_suspend,
	.resume = k3_fb_resume,
	.shutdown = k3_fb_shutdown,
	.driver = {
		/* Driver name must match the device name added in platform.c. */
		.name = "k3_fb",
		},
};

struct platform_device *k3_fb_add_device(struct platform_device *pdev)
{
	struct k3_fb_panel_data *pdata = NULL;
	struct platform_device *this_dev = NULL;
	struct fb_info *fbi = NULL;
	struct k3_fb_data_type *k3fd = NULL;
	u32 type = 0, id = 0, graphic_ch = 0;

	BUG_ON(pdev == NULL);

	pdata = pdev->dev.platform_data;
	BUG_ON(pdata == NULL);

	if (fbi_list_index >= MAX_FBI_LIST) {
		pr_err("k3fb, %s: no more framebuffer info list!\n", __func__);
		return NULL;
	}

	/* alloc panel device data */
	id = pdev->id;
	type = pdata->panel_info->type;
	this_dev = k3_fb_device_alloc(pdata, type, id, &graphic_ch);
	if (!this_dev) {
		pr_err("k3fb, %s: k3_fb_device_alloc failed!\n", __func__);
		return NULL;
	}

	/* alloc framebuffer info + par data */
	fbi = framebuffer_alloc(sizeof(struct k3_fb_data_type), NULL);
	if (fbi == NULL) {
		pr_err("k3fb, %s: can't alloca framebuffer info data!\n", __func__);
		/*platform_device_put(this_dev);*/
		k3_fb_device_free(this_dev);
		return NULL;
	}

	k3fd = (struct k3_fb_data_type *)fbi->par;
	k3fd->fbi = fbi;
	k3fd->panel.type = type;
	k3fd->panel.id = id;
	k3fd->graphic_ch = graphic_ch;
	k3fd->index = fbi_list_index;
	if (k3fd->index == 0) {
		k3fd->edc_base = k3fd_reg_base_edc0;
		k3fd->edc_irq = k3fd_irq_edc0;
		k3fd->ldi_irq = k3fd_irq_ldi0;
	} else {
		k3fd->edc_base = k3fd_reg_base_edc1;
		k3fd->edc_irq = k3fd_irq_edc1;
		k3fd->ldi_irq = k3fd_irq_ldi1;
	}

	/* link to the latest pdev */
	k3fd->pdev = this_dev;

	k3fd_list[k3fd_list_index++] = k3fd;
	fbi_list[fbi_list_index++] = fbi;

	 /* get/set panel info */
	memcpy(&k3fd->panel_info, pdata->panel_info, sizeof(struct k3_panel_info));

	/* set driver data */
	platform_set_drvdata(this_dev, k3fd);

	if (platform_device_add(this_dev)) {
		pr_err("k3fb, %s: platform_device_add failed!\n", __func__);
		/*platform_device_put(this_dev);*/
		framebuffer_release(fbi);
		k3_fb_device_free(this_dev);
		fbi_list_index--;
		return NULL;
	}

	return this_dev;
}
EXPORT_SYMBOL(k3_fb_add_device);

int __init k3_fb_init(void)
{
	int ret = -ENODEV;

	ret = platform_driver_register(&k3_fb_driver);
	if (ret) {
		pr_err("k3fb, %s not able to register the driver\n", __func__);
		return ret;
	}

	return ret;
}

module_init(k3_fb_init);
