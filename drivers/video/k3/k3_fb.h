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

#ifndef K3_FB_H
#define K3_FB_H

#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/fb.h>
#include <linux/spinlock.h>
#include <mach/platform.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <mach/hisi_mem.h>
#include "k3_fb_def.h"
#include "k3_fb_panel.h"
#include "edc_overlay.h"

/*G2D clk rate*/
#define G2D_NORMAL_FREQ	(480000000)
#define G2D_SLOW_FREQ	(60000000)
#define G2D_CLK_60MHz	(60 * 1000 * 1000)
#define G2D_CLK_120MHz	(120 * 1000 * 1000)
#define G2D_CLK_240MHz	(240 * 1000 * 1000)
#define G2D_CLK_360MHz	(360 * 1000 * 1000)
#define G2D_CLK_480MHz	(480 * 1000 * 1000)

#define K3_FB_OVERLAY_USE_BUF 1
/*FRC Frame definition*/
#define K3_FB_FRC_BENCHMARK_FPS	67
#define K3_FB_FRC_NORMAL_FPS	60
#define K3_FB_FRC_VIDEO_FPS	60
#define K3_FB_FRC_WEBKIT_FPS	30
#define K3_FB_FRC_GAME_FPS	30
#define K3_FB_FRC_IDLE_FPS	30
#define K3_FB_FRC_SPECIAL_GAME_FPS    60
#define K3_FB_FRC_THRESHOLD	6

/*ESD definition*/
#define K3_FB_ESD_THRESHOLD	45/*90*/

/* display resolution limited */
#define K3_FB_MIN_WIDTH		32
#define K3_FB_MIN_HEIGHT	32
#define K3_FB_MAX_WIDTH		1200
#define K3_FB_MAX_HEIGHT	1920

/* EDC buffer width and height Align limited */
#define EDC_BUF_WIDTH_ALIGN		8
#define EDC_BUF_HEIGHT_ALIGN	2

#define EDC_CH_SECU_LINE	11

/* frame buffer physical addr */
#define K3_FB_PA			HISI_FRAME_BUFFER_BASE
#define K3_NUM_FRAMEBUFFERS	4
extern u32 k3fd_reg_base_edc0;
extern u32 k3fd_reg_base_edc1;

#define SBL_BKL_STEP 5
#define SBL_REDUCE_VALUE(x)  ((x) * 70/100)

#ifdef CONFIG_LCD_TOSHIBA_MDW70
#define CLK_SWITCH	0
#elif defined(CONFIG_LCD_CMI_OTM1280A)
#define CLK_SWITCH	1
#endif

enum {
	LCD_LANDSCAPE = 0,
	LCD_PORTRAIT,
};

/* set backlight by pwm or mipi ... */
enum {
	BL_SET_BY_NONE = 0,
	BL_SET_BY_PWM = 0x1,
	BL_SET_BY_MIPI = 0x2,
	BL_SET_BY_MIPI_ECO = 0x4,
};

enum {
	IMG_PIXEL_FORMAT_RGB_565 = 1,
	IMG_PIXEL_FORMAT_RGBA_5551,
	IMG_PIXEL_FORMAT_RGBX_5551,
	IMG_PIXEL_FORMAT_RGBA_8888,
	IMG_PIXEL_FORMAT_RGBX_8888,
};

enum {
	FB_64BYTES_ODD_ALIGN_NONE = 0,
	FB_64BYTES_ODD_ALIGN_UI,
	FB_64BYTES_ODD_ALIGN_VIDEO,
};

/*FRC State definition*/
enum {
	K3_FB_FRC_NONE_PLAYING = 0x0,
	K3_FB_FRC_VIDEO_PLAYING = 0x2,
	K3_FB_FRC_GAME_PLAYING = 0x4,
	K3_FB_FRC_VIDEO_IN_GAME = 0x6,
	K3_FB_FRC_BENCHMARK_PLAYING = 0x8,
	K3_FB_FRC_WEBKIT_PLAYING =0x10,
	K3_FB_FRC_SPECIAL_GAME_PLAYING = 0x20,
};

enum {
	FB_FRC_FLAG_IDLE = 0,
	FB_FRC_FLAG_BUSY,
	FB_FRC_FLAG_DECREASE,
	FB_FRC_FLAG_INCREASE,
};

enum {
	FB_LDI_INT_TYPE_NONE = 0,
	FB_LDI_INT_TYPE_FRC = 1,
	FB_LDI_INT_TYPE_ESD = 2,
};

#define K3FB_DEFAULT_BGR_FORMAT	EDC_RGB
#define CONFIG_FASTBOOT_ENABLE	1


struct k3_fb_data_type {
	u32 index;
	u32 ref_cnt;
	u32 bl_level;
	u32 bl_level_sbl;
	u32 bl_level_old;
	u32 fb_imgType;
	u32 fb_bgrFmt;
	u32 edc_base;
	u32 edc_irq;
	u32 ldi_irq;
	u32 bl_enable_mipi_eco;

	char edc_irq_name[64];
	char ldi_irq_name[64];

	panel_id_type panel;
	struct k3_panel_info panel_info;
	bool panel_power_on;

	struct fb_info *fbi;
	u32 graphic_ch;
	wait_queue_head_t edc_wait_queque;
	s32 edc_wait_flash;
	struct work_struct frame_end_work;
	struct workqueue_struct *frame_end_wq;

	bool cmd_mode_refresh;
	u32 frame_count;
	bool is_first_frame_end;

	struct edc_overlay_ctrl ctrl;
	struct platform_device *pdev;
	struct semaphore sem;
	struct clk *edc_clk;
	struct clk *edc_clk_rst;
	struct clk *ldi_clk;
	struct clk *mipi_dphy_clk;
	struct clk *g2d_clk;
	struct regulator *edc_vcc;

	/* for vsync event */

	/* for sbl */
	u32 sbl_enable;
	u32 sbl_lsensor_value;
	struct work_struct sbl_work;
	struct workqueue_struct *sbl_wq;

	/* for frc */
	int frc_flag;
	int frc_frame_count;
	unsigned long frc_timestamp;

	/* for esd */
	unsigned long esd_timestamp;
	int esd_frame_count;
    struct hrtimer esd_hrtimer;
    bool esd_hrtimer_enable;

	int ldi_int_type;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};


/******************************************************************************
** FUNCTIONS PROTOTYPES
*/

void set_reg(u32 addr, u32 val, u8 bw, u8 bs);
u32 k3_fb_line_length(u32 fb_index, u32 xres, int bpp);

void k3_fb_set_backlight(struct k3_fb_data_type *k3fd, u32 bkl_lvl);
struct platform_device *k3_fb_add_device(struct platform_device *pdev);
int k3_fb1_blank(int blank_mode);
int k3fb_buf_isfree(int phys);
void k3fb_set_hdmi_state(bool is_connected);
struct fb_var_screeninfo * k3fb_get_fb_var(int index);

#endif /* K3_FB_H */
