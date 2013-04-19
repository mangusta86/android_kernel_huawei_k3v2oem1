/*
 *  Hisilicon K3 soc camera ISP driver source file
 *
 *  CopyRight (C) Hisilicon Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/vmalloc.h>
#include <linux/fb.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <asm/io.h>
#include <asm/bug.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/android_pmem.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/time.h>

#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#include <linux/workqueue.h>

#include "cam_util.h"
#include "cam_dbg.h"
#include "k3_isp.h"
#include "k3_ispv1.h"
#include "k3_ispv1_afae.h"
#include "k3_isp_io.h"

#include "mach/irqs.h"

#define LOG_TAG "K3_ISPV1_AFAE"
#include "cam_log.h"

#define ISP_MAX_FOCUSED_WIN		4
#define ISP_MAX_FOCUS_WIN		25

/* same as metering default CWA area */
#define DEFAULT_AF_WIDTH_PERCENT	20
#define DEFAULT_AF_HEIGHT_PERCENT	25

/* with stabilizer, max size is larger than 1080P. */
#define MAX_PREVIEW_WIDTH	2112
#define MAX_PREVIEW_HEIGHT	1200

#define AF_SINGLE_WINMODE_ONLY

typedef struct _ispv1_focus_win_info_s{
	u32 left;
	u32 top;
	u32 width;
	u32 height;

	u32 width1;
	u32 height1;

	u32 weight[ISP_MAX_FOCUS_WIN];
} ispv1_focus_win_info_s;

typedef struct _ispv1_focused_result_s {
	u32 focused_win_num;
	u32 focused_win[ISP_MAX_FOCUSED_WIN];
} ispv1_focused_result_s;

/* point to global isp_data */
extern k3_isp_data *this_ispdata;

ispv1_afae_ctrl *afae_ctrl;
static camera_metering this_metering = CAMERA_METERING_CWA;
struct semaphore sem_af_schedule;

axis_triple mXYZ;

#define AF_TIME_PRINT
#ifdef AF_TIME_PRINT
	static struct timeval tv_start, tv_end;
#endif

#define ispv1_setreg_focus_forcestart(enable) \
	do { \
		SETREG8(REG_ISP_FOCUS_FORCE_START, (enable)); \
	} while (0)

#define ispv1_setreg_1step_comeback(enable) \
	do { \
		SETREG8(REG_ISP_FOCUS_1STEP_COMEBACK, (enable)); \
	} while (0)

static void __attribute__((unused)) ispv1_wait_caf_done(void);
static int __attribute__((unused)) ispv1_get_merged_rect(focus_area_s *area,
				camera_rect_s *yuv_rect, u32 preview_width, u32 preview_height);
static int __attribute__((unused)) ispv1_get_focus_win_info(camera_rect_s *raw_rect,
				ispv1_focus_win_info_s *win_info, int *binning);
static int __attribute__((unused)) ispv1_get_map_table(focus_area_s *area,
				ispv1_focus_win_info_s *win_info, int *map_table);

struct workqueue_struct *af_start_work_queue;
struct work_struct af_start_work;
static void ispv1_af_start_work_func(struct work_struct *work);

struct workqueue_struct *af_hold_work_queue;
struct work_struct af_hold_work;
static void ispv1_af_hold_work_func(struct work_struct *work);

static int ispv1_setreg_vcm_code(u32 vcm_code);
static void ispv1_setreg_focus_init(vcm_info_s *vcm_info);
static void ispv1_setreg_focus_framerate(int framerate);
static void ispv1_setreg_vcm(vcm_info_s *vcm_info);
static void ispv1_setreg_focus_win(ispv1_focus_win_info_s *win_info);
static int ispv1_setreg_af_mode(camera_af_mode mode);
static int ispv1_getreg_is_focused(void);
static int ispv1_getreg_focus_result(ispv1_focused_result_s *result, int multi_win);

static int ispv1_set_focus_mode_done(camera_focus focus_mode);
static int ispv1_set_focus_area_done(focus_area_s *area);

static int ispv1_set_metering_area_done(metering_area_s *area);

static void ispv1_assistant_af(bool action);
static int ispv1_focus_need_flash(u8 current_y);

static void ispv1_focus_status_reset(void);

void ispv1_k3focus_run(void);

static inline FOCUS_STATUS get_videocaf_status(void)
{
	return afae_ctrl->video_caf_status;
}

static inline void set_videocaf_status(FOCUS_STATUS status)
{
	afae_ctrl->video_caf_status = status;
}

static inline void save_videocaf_code(int code)
{
	afae_ctrl->video_caf_code = code;
}

static inline int get_videocaf_code(void)
{
	return afae_ctrl->video_caf_code;
}

static inline bool get_videocaf_forcestart(void)
{
	return afae_ctrl->force_start;
}

static inline void set_videocaf_forcestart(bool forcestart)
{
	if (forcestart == true)
		afae_ctrl->force_start |= 0xff00;
}

static inline focus_state_e get_focus_state(void)
{
	return afae_ctrl->focus_state;
}

static inline void set_focus_state(focus_state_e state)
{
	afae_ctrl->focus_state = state;
}

static inline camera_focus get_focus_mode(void)
{
	return afae_ctrl->mode;
}

static inline void set_focus_mode(camera_focus mode)
{
	afae_ctrl->mode = mode;
}

u32 ispv1_get_focus_code(void)
{
	u32 code;
	focus_result_s result;
	int ret;

	if (get_focus_mode() == CAMERA_FOCUS_CONTINUOUS_VIDEO)
		code = get_videocaf_code();
	else
		GETREG16(REG_ISP_FOCUS_MOTOR_CURR, code);

	ret = ispv1_get_focus_result(&result);

	code &= 0xfffc;
	code |= result.status;

	return code;
}

static void ispv1_setreg_focus_sensitivity(camera_focus mode)
{
	if (mode == CAMERA_FOCUS_CONTINUOUS_VIDEO) {
		SETREG16(REG_ISP_FOCUS_nT_compare_1x, 0x0003); /* more small, CAF more easy to stop */
		SETREG16(REG_ISP_FOCUS_nT_compare_16x, 0x0003);
		SETREG16(REG_ISP_FOCUS_nT_sad_1x, 0x0000); /* 1->0, only object keep still completely will trigger CAF */
		SETREG16(REG_ISP_FOCUS_nT_sad_16x, 0x0000);

		/*
		 * current contrast compare with last focus success contrast to trigger CAF
		 * if set very large params, switch to trigger by software judgement.
		 */
		SETREG16(REG_ISP_FOCUS_nT_presad_1x, 0x7fff);
		SETREG16(REG_ISP_FOCUS_nT_presad_16x, 0x7fff);
		SETREG16(REG_ISP_FOCUS_nT_hist_1x, 0x0002);
		SETREG16(REG_ISP_FOCUS_nT_hist_16x, 0x0002);

		SETREG16(REG_ISP_FOCUS_nT_contrast_diff, 0x0004);
		SETREG8(0x1cddb, 0x14); /* default is 50 */
		SETREG8(0x1cd09, 0x10); /* skip frames after AF done */
		SETREG8(0x1cd0d, 0x3c); /* default is 0x36, more larger, CAF more easy to stop */
		SETREG16(REG_ISP_FOCUS_MOTOR_RESTIME, 0x40); /* Recording CAF will be slow */
		SETREG8(REG_ISP_FOCUS_PREMOVE, 0); /* no premove */
		SETREG8(0x1cdd6, 1); /* low precision mode */
		SETREG8(0x1ccc3, 0); /* close new CAF method */
	} else {
		SETREG16(REG_ISP_FOCUS_nT_compare_1x, 0x0008);
		SETREG16(REG_ISP_FOCUS_nT_compare_16x, 0x0008);
		SETREG16(REG_ISP_FOCUS_nT_sad_1x, 0x0001);
		SETREG16(REG_ISP_FOCUS_nT_sad_16x, 0x0002);
		SETREG16(REG_ISP_FOCUS_nT_presad_1x, 0x0006); /* this value come from ov */
		SETREG16(REG_ISP_FOCUS_nT_presad_16x, 0x0004); /* this value come from ov */
		SETREG16(REG_ISP_FOCUS_nT_hist_1x, 0x0002);
		SETREG16(REG_ISP_FOCUS_nT_hist_16x, 0x0001);

		SETREG16(REG_ISP_FOCUS_nT_contrast_diff, 0x0008); /* default is 8 */
		SETREG8(0x1cddb, 0x32); /* default is 50 */
		SETREG8(0x1cd09, 0x10); /* skip frames after AF done */
		SETREG8(0x1cd0d, 0x3c); /* default is 0x36, more larger, CAF more easy to stop */
		SETREG16(REG_ISP_FOCUS_MOTOR_RESTIME, 0x20);
		SETREG8(REG_ISP_FOCUS_PREMOVE, 1); /* premove mode */
		SETREG8(0x1cdd6, 0); /* high precision mode */
		SETREG8(0x1ccc3, 1); /* open new CAF method */
		SETREG8(0x1ccc4, 8); /* 8 close steps use no premove */
	}
}

/* set focus performance registers */
static void ispv1_setreg_focus_init(vcm_info_s *vcm_info)
{
	u8 reg1;

	print_debug("enter %s", __func__);

	SETREG8(REG_ISP_FOCUS_I2COPTION, vcm_info->vcm_bits);
	SETREG8(REG_ISP_FOCUS_DEVICEID, vcm_info->vcm_id);

	SETREG16(REG_ISP_FOCUS_MOVELENS_ADDR0, vcm_info->moveLensAddr[0]);
	SETREG16(REG_ISP_FOCUS_MOVELENS_ADDR1, vcm_info->moveLensAddr[1]);
	SETREG8(REG_ISP_FOCUS_MOTOR_DRIVEMODE, vcm_info->MotorDriveMode);
	SETREG8(REG_ISP_FOCUS_MOTOR_CALIBRATE, vcm_info->IfMotorCalibrate);
	SETREG16(REG_ISP_FOCUS_MOTOR_RESTIME, vcm_info->motorResTime);

	/*
	 * init some default register
	 * REG_ISP_SCALE6_SELECT bit5: 0-ISP raw;1-after ISP crop&dcw raw
	 */
	reg1 = GETREG8(REG_ISP_SCALE6_SELECT);
	reg1 |= (1 << 5);
	SETREG8(REG_ISP_SCALE6_SELECT, reg1);

	/* Statistic registers, large STAT_THRESHOLD value maybe helpful to focus PC Keyboard. */
	SETREG8(REG_ISP_FOCUS_MIN_STAT_HF_NUM, 0x04);
	SETREG8(REG_ISP_FOCUS_STAT_THRESHOLD, 0xff);

	/* modified for isp 2012-06-21 version*/
	SETREG8(REG_ISP_FOCUS_LOCK_AE, 1);

	ispv1_setreg_focus_sensitivity(CAMERA_FOCUS_AUTO);

	SETREG8(0x1cdcc, 0x0f);
	SETREG8(0x1ccef, 0x03);
	SETREG8(0x1cdd8, 0x10);
	SETREG8(0x1cdda, 0x02);
	SETREG8(0x1cd08, 0x00);
	SETREG8(0x1cdde, 0x20);
	SETREG8(REG_ISP_FOCUS_FAILURE_POS, 0);
	SETREG8(0x1ccc8, 0x20); /* judge focus failure, more larger, more easy to success. */

	/* init vcm ad5823 */
	if ((vcm_info->moveLensAddr[0] == 0x4) && (vcm_info->moveLensAddr[1] == 0x5)) {
		ispv1_write_vcm(vcm_info->vcm_id, 0x02, 0x01, I2C_8BIT, SCCB_BUS_MUTEX_WAIT);
		ispv1_write_vcm(vcm_info->vcm_id, 0x03, 0x80, I2C_8BIT, SCCB_BUS_MUTEX_WAIT);
	}
}

static void ispv1_cal_vcm_range(vcm_info_s *vcm)
{
	u16 otp_start = 0;
	u16 otp_end = 0;
	int otp_range;

	int normal_range;
	int normal_end;

	int video_range;
	int video_end;

	if (vcm->get_vcm_otp != NULL) {
		vcm->get_vcm_otp(&otp_start, &otp_end);
		print_info("get 10bit otp start 0x%x, end 0x%x***************", otp_start, otp_end);

		if (otp_start > 0xc0)
			otp_start = 0xc0;

		if (otp_end != 0) {
			normal_range = vcm->normalDistanceEnd - vcm->infiniteDistance;
			video_range = vcm->videoDistanceEnd - vcm->infiniteDistance;
			otp_range = (otp_end & 0x3f0) - (otp_start & 0x3f0);

			normal_range = (normal_range < otp_range) ? normal_range:otp_range;
			normal_range -= (normal_range % vcm->normalStep);

			video_range = (video_range < otp_range) ? video_range:otp_range;
			video_range -= (video_range % vcm->videoStep);

			vcm->infiniteDistance = (otp_start & 0x3f0);

			normal_end = vcm->infiniteDistance + normal_range;
			if (normal_end >= 0x400) {
				print_error("normalDistanceEnd 0x%x too large!", vcm->normalDistanceEnd);
				normal_range = (0x3f0 - vcm->infiniteDistance);
				normal_range -= (normal_range % vcm->normalStep);
				normal_end = vcm->infiniteDistance + normal_range;
			} else if (normal_end < vcm->normalDistanceEnd) {
				normal_end = vcm->normalDistanceEnd;
			}
			vcm->normalDistanceEnd = normal_end;

			video_end = vcm->infiniteDistance + video_range;
			if (vcm->videoDistanceEnd >= 0x400) {
				print_error("videoDistanceEnd 0x%x too large!", vcm->videoDistanceEnd);
				video_range = (0x3f0 - vcm->infiniteDistance);
				video_range -= (normal_range % vcm->videoStep);
				video_end = vcm->infiniteDistance + video_range;
			} else if (video_end < vcm->videoDistanceEnd) {
				video_end = vcm->videoDistanceEnd;
			}
			vcm->videoDistanceEnd = video_end;
		}
	}

	print_info("focus infiniteDistance 0x%x, normalDistanceEnd 0x%x, videoDistanceEnd 0x%x***************",
		vcm->infiniteDistance, vcm->normalDistanceEnd, vcm->videoDistanceEnd);
}

/*
 * set frame rate information to isp register
 */
void ispv1_setreg_focus_framerate(int framerate)
{
	print_debug("enter %s", __func__);

	SETREG16(REG_ISP_FOCUS_MOTOR_FRAMERATE, framerate);
}

/*
 * set vcm information to isp register
 */
static void ispv1_setreg_vcm(vcm_info_s *vcm_info)
{
	print_debug("enter %s", __func__);

	assert(vcm_info);

	SETREG16(REG_ISP_FOCUS_MOTOR_OFFSETINIT, vcm_info->offsetInit);
	SETREG16(REG_ISP_FOCUS_MOTOR_FULLRANGE, vcm_info->fullRange);
	SETREG16(REG_ISP_FOCUS_MOTOR_COARSESTEP, vcm_info->coarseStep);
	SETREG16(REG_ISP_FOCUS_MOTOR_FINESTEP, vcm_info->fineStep);
	SETREG16(REG_ISP_FOCUS_MOTOR_MINISTEP, vcm_info->fineStep);
	SETREG16(REG_ISP_FOCUS_MOTOR_FRAMERATE, vcm_info->frameRate);
}

/*
 **************************************************************************
 * FunctionName: ispv1_setreg_vcm_code;
 * Description : set vcm position to vcm_code;
 * Input       : NA;
 * Output      : NA;
 * ReturnValue : NA;
 * Other       : NA;
 **************************************************************************
 */
static int ispv1_setreg_vcm_code(u32 vcm_code)
{
	int ret = 0;
	vcm_info_s *vcm = this_ispdata->sensor->vcm;
#if 0
	SETREG16(0x1c5da, vcm_code & 0xffff);
	SETREG8(0x1cddd, 0x01);
#else
	if ((vcm->moveLensAddr[0] == 0x4) && (vcm->moveLensAddr[1] == 0x5)) {
		ret |= ispv1_write_vcm(vcm->vcm_id, vcm->moveLensAddr[0], ((vcm_code >> 8) | 0x04), I2C_8BIT, SCCB_BUS_MUTEX_NOWAIT);
		ret |= ispv1_write_vcm(vcm->vcm_id, vcm->moveLensAddr[1], (vcm_code & 0xFF), I2C_8BIT, SCCB_BUS_MUTEX_NOWAIT);
	} else if ((vcm->moveLensAddr[0] == 0x0) && (vcm->moveLensAddr[1] == 0x0)) {
		/*in dw9714 case, ad value must set to bit4 ~ bit13*/
		ret |= ispv1_write_vcm(vcm->vcm_id, vcm->moveLensAddr[0], vcm_code << 4, I2C_16BIT, SCCB_BUS_MUTEX_NOWAIT);
	} else {
		print_error("%s: unsupported vcm",  __func__);
	}
#endif
	return ret;
}

/* Set focus windows(suitable for ISP) to registers. */
static void ispv1_setreg_focus_win(ispv1_focus_win_info_s *win_info)
{
	u32 loopi;

	print_debug("enter %s", __func__);

	assert(win_info);

	SETREG16(REG_ISP_FOCUS_WIN_X0, win_info->left);
	SETREG16(REG_ISP_FOCUS_WIN_Y0, win_info->top);
	SETREG16(REG_ISP_FOCUS_WIN_W0, win_info->width);
	SETREG16(REG_ISP_FOCUS_WIN_H0, win_info->height);

	if ((win_info->width1 == 0) && (win_info->height1 == 0)) {
		print_debug("%s: single win mode", __func__);
		return;
	}

	SETREG16(REG_ISP_FOCUS_WIN_W1, win_info->width1);
	SETREG16(REG_ISP_FOCUS_WIN_H1, win_info->height1);

	for (loopi = 0; loopi < ISP_MAX_FOCUS_WIN; loopi++) {
		SETREG16(REG_ISP_FOCUS_WEIGHT_LIST(loopi), win_info->weight[loopi]);
	}
}

/*
 **************************************************************************
 * FunctionName: ispv1_setreg_af_mode;
 * Description : set auto focus mode;
 * Input       : mode; 0-snapshot auto mode;
 *                             1-snapshot single mode;
 *                             2-snapshot weighted mode;
 *                             3-video servo mode.
 * Output      : NA;
 * ReturnValue : ture or false;
 * Other       : NA;
 **************************************************************************
 */
static int ispv1_setreg_af_mode(camera_af_mode mode)
{
	u8 reg1 = 0;

	print_debug("enter %s, af mode %d", __func__, mode);

	SETREG16(REG_ISP_FOCUS_AFCCTRL2, 0x10);

	/*
	 * 0x66100: [2] bIfBinning: (1:binning; 0:non-binning); [1] nMode: (1:5x5;0:single)
	 * ISP_VIDEO_SERVO_MODE/ISP_SINGLE_SNAPSHOT_MODE is 0x4;
	 * ISP_AUTO_SNAPSHOT_MODE/ISP_WEIGHTED_SNAPSHOT_MODE is 0x2;
	 *
	 * REG_ISP_FOCUS_MODE; video 1; snapshot 0
	 * REG_ISP_FOCUS_WINMODE; single 0; 5x5 1;
	 * REG_ISP_FOCUS_bIFAUTO; auto 1;
	 * REG_ISP_FOCUS_bIFWEIGHTED; weighted 1; only works when nWinMode=1;
	 */
#ifdef AF_SINGLE_WINMODE_ONLY
	reg1 = ISP_FOCUS_NONBINNING;
#else
	if (afae_ctrl->binning)
		reg1 = ISP_FOCUS_BINNING;
	else
		reg1 = ISP_FOCUS_NONBINNING;
#endif
	/* set register REG_ISP_FOCUS_AFCCTRL0's bit0 to 1  */
	reg1 |= 0x01;
	switch (mode) {
	case CAMERA_VIDEO_SERVO_MODE:
		SETREG8(REG_ISP_FOCUS_AFCCTRL0, reg1 | ISP_FOCUS_SINGLE_WINDOW);
		SETREG8(REG_ISP_FOCUS_MODE, ISP_FOCUS_VIDEO_MODE);
		SETREG8(REG_ISP_FOCUS_WINMODE, ISP_FOCUS_WINMODE_SINGLE);
		/*
		 * No need to set REG_ISP_FOCUS_bIFAUTO
		 * No need to set REG_ISP_FOCUS_bIFWEIGHTED
		 */
		break;

	case CAMERA_SINGLE_SNAPSHOT_MODE:
		SETREG8(REG_ISP_FOCUS_AFCCTRL0, reg1 | ISP_FOCUS_SINGLE_WINDOW);
		SETREG8(REG_ISP_FOCUS_MODE, ISP_FOCUS_SNAPSHOT_MODE);
		SETREG8(REG_ISP_FOCUS_WINMODE, ISP_FOCUS_WINMODE_SINGLE);
		/*
		 * No need to set REG_ISP_FOCUS_bIFAUTO
		 * No need to set REG_ISP_FOCUS_bIFWEIGHTED
		 */
		break;

	case CAMERA_AUTO_SNAPSHOT_MODE:
		SETREG8(REG_ISP_FOCUS_AFCCTRL0, reg1 | ISP_FOCUS_5X5_WINDOW);
		SETREG8(REG_ISP_FOCUS_MODE, ISP_FOCUS_SNAPSHOT_MODE);
		SETREG8(REG_ISP_FOCUS_WINMODE, ISP_FOCUS_WINMODE_5X5);
		SETREG8(REG_ISP_FOCUS_IFAUTO, ISP_FOCUS_IS_AUTO);
		SETREG8(REG_ISP_FOCUS_IFWEIGHTED, ISP_FOCUS_ISNOT_WEIGHTED);
		break;

	case CAMERA_WEIGHTED_SNAPSHOT_MODE:
		SETREG8(REG_ISP_FOCUS_AFCCTRL0, reg1 | ISP_FOCUS_5X5_WINDOW);
		SETREG8(REG_ISP_FOCUS_MODE, ISP_FOCUS_SNAPSHOT_MODE);
		SETREG8(REG_ISP_FOCUS_WINMODE, ISP_FOCUS_WINMODE_5X5);
		SETREG8(REG_ISP_FOCUS_IFAUTO, ISP_FOCUS_ISNOT_AUTO);
		SETREG8(REG_ISP_FOCUS_IFWEIGHTED, ISP_FOCUS_IS_WEIGHTED);
		break;

	default:
		print_error("ispv1_set_focus_mode param error!\n");
		return -1;
	}

	return 0;
}

/*
 **************************************************************************
 * FunctionName: ispv1_is_focused;
 * Description : get focused status;
 * Input       : NA.
 * Output      : NA;
 * ReturnValue : 0-still focusing; 1-focused; 2-out of focus/failure; -1:error.
 * Other       : NA;
 **************************************************************************
 */
static int ispv1_getreg_is_focused(void)
{
	int focus_active;
	int focus_status;
	print_debug("enter %s", __func__);

	/* If focus mode is snap-shot mode, first should check FOCUS_ACTIVE reg. */
	if ((get_focus_mode() != CAMERA_FOCUS_CONTINUOUS_VIDEO) &&
	    (get_focus_mode() != CAMERA_FOCUS_CONTINUOUS_PICTURE)) {
		focus_active = GETREG8(REG_ISP_FOCUS_ACTIVE);
		print_debug("%s, focus_active %d", __func__, focus_active);
		if (focus_active)
			return STATUS_FOCUSING;
	}

	focus_status = GETREG8(REG_ISP_FOCUS_STATUS);
	/* get bit[1:0] */
	focus_status &= 0x3;

	print_debug("%s, focus_status %d", __func__, focus_status);

	if ((focus_status != STATUS_FOCUSING) &&
	    (focus_status != STATUS_FOCUSED) &&
	    (focus_status != STATUS_OUT_FOCUS)) {
		print_error("%s,line %d: focus status %d error!", __func__, __LINE__, focus_status);
		return -1;
	} else
		return focus_status;
}

/*
 **************************************************************************
 * FunctionName: ispv1_getreg_focus_result;
 * Description : get focus result from isp
 * Input       : NA;
 * Output      : NA;
 * ReturnValue : STATUS_FOCUSING, STATUS_FOCUSED, STATUS_OUT_FOCUS
 * Other       : NA;
 **************************************************************************
 */
static int ispv1_getreg_focus_result(ispv1_focused_result_s *result, int multi_win)
{
#if 1
	u32 loopi;
	int ret;
	vcm_info_s *vcm = this_ispdata->sensor->vcm;
	int val_vcm = 0;
	int val_lower_limit = 0;
	int val_uper_limit = 0;

	assert(result);
	print_debug("enter %s", __func__);

	/*
	 * first should check focus state is safe state.
	 * because Auto_focus execute is asynchronous:
	 * Auto_focus activate work queue.
	 * so in CAF_PREPARING/AF_PREPARING, should return STATUS_FOCUSING.
	 */
	if ((get_focus_state() == FOCUS_STATE_CAF_PREPARING) ||
	    (get_focus_state() == FOCUS_STATE_AF_PREPARING))
		return STATUS_FOCUSING;

	if (get_focus_mode() == CAMERA_FOCUS_CONTINUOUS_VIDEO) {
		result->focused_win_num = 1;
		result->focused_win[0] = 0;
		return get_videocaf_status();
	}

	ret = ispv1_getreg_is_focused();

	/* according to focusing time, calculate available focus range */
	if ((STATUS_FOCUSED == ret) && (NULL != vcm) && (RANGE_NORMAL == vcm->moveRange)
		&& (get_focus_mode() == CAMERA_FOCUS_AUTO)) {
		GETREG16(REG_ISP_FOCUS_MOTOR_CURR, val_vcm);
		val_lower_limit = vcm->offsetInit + vcm->coarseStep * (afae_ctrl->focus_frame_count / 2 - 7);
		val_uper_limit = vcm->offsetInit + vcm->coarseStep * (afae_ctrl->focus_frame_count / 2 + 4);
		if (((afae_ctrl->focus_frame_count > 18) && (val_vcm < val_lower_limit)) || (val_vcm > val_uper_limit)) {
			print_warn("!!!!!!focus failure caused by AP judgement!!!!!!");
			ret = STATUS_OUT_FOCUS;
		}
	}

	if (ret != STATUS_FOCUSED) {
		print_debug("%s,line %d: focus status %d!", __func__, __LINE__, ret);
		return ret;
	}

	if (!multi_win) {
		print_debug("%s, line %d, just one area\n", __func__, __LINE__);
		result->focused_win_num = 1;
		result->focused_win[0] = 0;
		return STATUS_FOCUSED;
	}

	result->focused_win_num = GETREG8(REG_ISP_FOCUS_WIN_NUM);
	if ((result->focused_win_num > 0) && (result->focused_win_num <= 4)) {
		for (loopi = 0; loopi < result->focused_win_num; loopi++) {
			/* get focused windows ID */
			result->focused_win[loopi] = GETREG8(REG_ISP_FOCUS_WIN_LIST(loopi));
		}
	} else {
		print_error("multi_win mode:is focused, but ISP win id number is error %d", result->focused_win_num);
		return -1;
	}

	return STATUS_FOCUSED;
#else
	result->focused_win_num = 1;
	result->focused_win[0] = 0;
	if (af_result == STATUS_FOCUSING) {
		return STATUS_FOCUSING;
	} else if (af_result == STATUS_FOCUSED) {
		return STATUS_FOCUSED;
	} else {
		return STATUS_OUT_FOCUS;
	}
#endif
}

/*Just set some fixed parameters. And get some resource. */
int ispv1_focus_init(void)
{
	vcm_info_s *vcm = this_ispdata->sensor->vcm;
	u32 size;

	afae_ctrl = kmalloc(sizeof(ispv1_afae_ctrl), GFP_KERNEL);
	if (!afae_ctrl) {
		print_error("malloc is failed in %s function at line#%d\n", __func__, __LINE__);
		return -ENOMEM;
	}
	memset(afae_ctrl, 0, sizeof(ispv1_afae_ctrl));

	afae_ctrl->map_table = kmalloc(MAX_FOCUS_RECT * ISP_MAX_FOCUS_WIN * sizeof(int), GFP_KERNEL);
	if (!afae_ctrl->map_table) {
		print_error("malloc is failed in %s function at line#%d\n", __func__, __LINE__);
		kfree(afae_ctrl);
		afae_ctrl = NULL;
		return -ENOMEM;
	}
	memset(afae_ctrl->map_table, 0, MAX_FOCUS_RECT * ISP_MAX_FOCUS_WIN * sizeof(int));

	/* request stat data for copy and calculate contrast extend threshold. */
	size = MAX_PREVIEW_WIDTH * MAX_PREVIEW_HEIGHT *
		DEFAULT_AF_WIDTH_PERCENT * DEFAULT_AF_HEIGHT_PERCENT / 100 / 100;
	afae_ctrl->stat_data = vmalloc(size);
	if (afae_ctrl->stat_data == NULL) {
		print_error("malloc is failed in %s function at line#%d\n", __func__, __LINE__);
		kfree(afae_ctrl->map_table);
		kfree(afae_ctrl);
		afae_ctrl->map_table = NULL;
		afae_ctrl = NULL;
		return -ENOMEM;
	}

	set_focus_mode(CAMERA_FOCUS_CONTINUOUS_PICTURE);
	set_focus_state(FOCUS_STATE_STOPPED);
	afae_ctrl->binning = 0;
	afae_ctrl->focus_failed = 0;

	/* init af start work queue. */
	af_start_work_queue = create_singlethread_workqueue("af_start_wq");
	if (!af_start_work_queue) {
		print_error("create workqueue is failed in %s function at line#%d\n", __func__, __LINE__);
		kfree(afae_ctrl);
		afae_ctrl = NULL;
		return -1;
	}
	INIT_WORK(&af_start_work, ispv1_af_start_work_func);
	/* init af hold work queue. */
	af_hold_work_queue = create_singlethread_workqueue("af_hold_wq");
	if (!af_hold_work_queue) {
		print_error("create workqueue is failed in %s function at line#%d\n", __func__, __LINE__);
		kfree(afae_ctrl);
		afae_ctrl = NULL;
		return -1;
	}
	INIT_WORK(&af_hold_work, ispv1_af_hold_work_func);


	ispv1_cal_vcm_range(vcm);

	ispv1_setreg_focus_init(vcm);

	/* start focus when enter cold boot preview.
	 * added by y36721 2012-02-21
	 */
	ispv1_setreg_focus_forcestart(1);

	return 0;
}
 /*
  *************************************************************************
  * FunctionName: ispv1_focus_vcm_go_infinite;
  * Description : using for vcm go to infinite position
  * Input       : delay_flag : 0 meens needless hold time;
  *			       1 meens need hold time
  * Output      : NA;
  * ReturnValue : NA;
  * Other       : in delay_flag equals 1 case, using for exit camera ;
  **************************************************************************
  */
static void ispv1_focus_vcm_go_infinite(u8 delay_flag)
{
	vcm_info_s *vcm = this_ispdata->sensor->vcm;
	int val_vcm = 0;
	int step;

	GETREG16(REG_ISP_FOCUS_MOTOR_CURR, val_vcm);

	print_debug("%s, val_vcm 0x%x, infiniteDistance 0x%x, vcm_id 0x%x, Addr 0x%x, 0x%x",
		__func__, val_vcm, vcm->infiniteDistance, vcm->vcm_id,
		vcm->moveLensAddr[0], vcm->moveLensAddr[1]);

	ispv1_auto_focus(FOCUS_STOP);	/*Stop auto focus mode */

	/*
	 * in power off camera case,
	 * if current position bigger than infinite distance(safe position),
	 * then must return to infinite position by slow speed.
	 * after arriving infinite position, holding power few micro seconds,
	 * then power off camera.
	 * but, in change mode case,
	 * set current position to infinite distance directly.
	 */
	if (val_vcm <= vcm->infiniteDistance)
		return;

	if ((vcm->moveLensAddr[0] == 0x4) && (vcm->moveLensAddr[1] == 0x5)) {

		ispv1_write_vcm(vcm->vcm_id, 0x02, 0x02, I2C_8BIT, SCCB_BUS_MUTEX_WAIT);


		while (val_vcm  > vcm->infiniteDistance) {
			if (1 == delay_flag)
				step = ((val_vcm - vcm->infiniteDistance) >= 0x40) ? 0x40 : (val_vcm - vcm->infiniteDistance);
			else
				step = val_vcm - vcm->infiniteDistance;
			val_vcm -= step; /* calculate next position of vcm */

			ispv1_write_vcm(vcm->vcm_id, vcm->moveLensAddr[0], ((val_vcm >> 8) | 0x04), I2C_8BIT, SCCB_BUS_MUTEX_WAIT);
			ispv1_write_vcm(vcm->vcm_id, vcm->moveLensAddr[1], (val_vcm & 0x00FF), I2C_8BIT, SCCB_BUS_MUTEX_WAIT);

			if (1 == delay_flag)
				msleep(6); /* holding time equals infiniteDistance div 10 */
		}
	} else if ((vcm->moveLensAddr[0] == 0x0) && (vcm->moveLensAddr[1] == 0x0)) {
		while (val_vcm  > vcm->infiniteDistance) {
			if (1 == delay_flag)
				step = ((val_vcm - vcm->infiniteDistance) >= 0x40) ? 0x40 : (val_vcm - vcm->infiniteDistance);
			else
				step = val_vcm - vcm->infiniteDistance;
			val_vcm -= step; /* calculate next position of vcm */

			ispv1_write_vcm(vcm->vcm_id, vcm->moveLensAddr[0], val_vcm << 4, I2C_16BIT, SCCB_BUS_MUTEX_WAIT);

			if (1 == delay_flag)
				msleep(6);	/* holding time equals infiniteDistance div 10 */
		}
	} else {
		print_error("%s: unsupported vcm",  __func__);
	}

	if (1 == delay_flag) {
		msleep(50);	/*holding time */
	}
}

/*called by exit */
int ispv1_focus_exit(void)
{
	ispv1_focus_vcm_go_infinite(1);
	if (afae_ctrl->map_table) {
		kfree(afae_ctrl->map_table);
		afae_ctrl->map_table = NULL;
	}

	if (afae_ctrl->stat_data) {
		vfree(afae_ctrl->stat_data);
		afae_ctrl->stat_data = NULL;
	}

	destroy_workqueue(af_start_work_queue);
	destroy_workqueue(af_hold_work_queue);

	if (afae_ctrl) {
		kfree(afae_ctrl);
		afae_ctrl = NULL;
	}

	return 0;
}

/* called by start_preview */
int ispv1_focus_prepare(void)
{
	print_debug("enter %s", __func__);

	set_focus_mode(CAMERA_FOCUS_CONTINUOUS_PICTURE);
	set_focus_state(FOCUS_STATE_STOPPED);
	afae_ctrl->af_area.focus_rect_num = 1;
	afae_ctrl->focus_failed = 0;
	afae_ctrl->video_caf_status = STATUS_FOCUSED;
	afae_ctrl->k3focus_running = false;

	return 0;
}

/* called by stop_preview */
int ispv1_focus_withdraw(void)
{
	/* ispv1_wait_caf_done(); */
	ispv1_auto_focus(FOCUS_STOP);
	set_focus_state(FOCUS_STATE_STOPPED);
	afae_ctrl->focus_failed = 0;
	afae_ctrl->af_area.focus_rect_num = 0;
	return 0;
}

/*
 **************************************************************************
 * FunctionName: ispv1_auto_focus;
 * Description : set focus start or stop; 1-start; 0-cancel or stop
 * Input       : flag;
 * Output      : NA;
 * ReturnValue : 0:ture ; -1:false;
 * Other       : NA;
 **************************************************************************
 */
int ispv1_auto_focus(int flag)
{
	u8 framerate = this_ispdata->sensor->fps;

	print_info("enter %s: %d", __func__, flag);

	if (flag == FOCUS_START) {
	#ifdef AF_TIME_PRINT
		do_gettimeofday(&tv_start);
	#endif
		if (get_focus_mode() == CAMERA_FOCUS_CONTINUOUS_VIDEO) {
			if (get_focus_state() == FOCUS_STATE_STOPPED) {
				set_focus_state(FOCUS_STATE_CAF_PREPARING);
			} else {
				print_error("line %d: not valid status %d", __LINE__, get_focus_state());
				return -1;
			}
		} else if (get_focus_mode() == CAMERA_FOCUS_CONTINUOUS_PICTURE) {
			if ((get_focus_state() == FOCUS_STATE_STOPPED) ||
			    (get_focus_state() == FOCUS_STATE_CAF_HOLDING)) {
				/* if last time focus is failed, should set force start bit. */
				if (afae_ctrl->focus_failed == 1)
					ispv1_setreg_focus_forcestart(1);
				set_focus_state(FOCUS_STATE_CAF_PREPARING);
			} else if (get_focus_state() == FOCUS_STATE_AF_HOLDING) {
				set_focus_state(FOCUS_STATE_CAF_HOLDING);
				print_info("in fact CAF should be hold");
				queue_work(af_hold_work_queue, &af_hold_work);
				return 0;
			} else {
				print_error("line %d: not valid status %d", __LINE__, get_focus_state());
				return -1;
			}
		} else {
			/* if this time is snapshot focus,
			 * should goto AF_PREPARING state,
			 * force start bit should be clear.
			 */
			set_focus_state(FOCUS_STATE_AF_PREPARING);
			ispv1_setreg_focus_forcestart(0);
		}

		if (get_focus_mode() == CAMERA_FOCUS_CONTINUOUS_VIDEO)
			ispv1_setreg_1step_comeback(1);
		else
			ispv1_setreg_1step_comeback(0);

		queue_work(af_start_work_queue, &af_start_work);
	} else if (flag == FOCUS_STOP && get_focus_state() != FOCUS_STATE_STOPPED) {
	#ifdef AF_TIME_PRINT
		do_gettimeofday(&tv_end);
		print_info("*****focus TIME: %.4dms******",
			(int)((tv_end.tv_sec - tv_start.tv_sec)*1000 + (tv_end.tv_usec - tv_start.tv_usec) / 1000));
	#endif

		/* if focus succeeded and is not continuous video mode. */
		if ((afae_ctrl->focus_failed == 0) &&
			(get_focus_mode() != CAMERA_FOCUS_CONTINUOUS_VIDEO) &&
			(get_focus_mode() != CAMERA_FOCUS_CONTINUOUS_PICTURE)) {
			/* should goto AF_HOLDING state */
			set_focus_state(FOCUS_STATE_AF_HOLDING);
			ispv1_focus_status_reset();
		} else
			set_focus_state(FOCUS_STATE_STOPPED);

		if (true == this_ispdata->assistant_af_flash) {
			ispv1_assistant_af(false);
			this_ispdata->assistant_af_flash = false;
			msleep((1000 / framerate) * 3);
		}

		/* deleted aec lock register control for isp 2012-06-21 version */
		SETREG8(REG_ISP_FOCUS_ACTIVE, FOCUS_STOP);
		if (afae_ctrl->k3focus_running == true)
			ispv1_wakeup_focus_schedule(true);
	} else {
		print_error("ispv1_auto_focus param error!");
		return -1;
	}
	return 0;
}

static void ispv1_af_start_work_func(struct work_struct *work)
{
	u8 framerate = this_ispdata->sensor->fps;
	u8 cur_lum;
	u8 frame_count = 0;

	/* if focus state has changed, should return immediately. */
	if ((get_focus_state() != FOCUS_STATE_CAF_PREPARING) &&
		(get_focus_state() != FOCUS_STATE_AF_PREPARING))
		return;

	ispv1_set_metering_area_done(&afae_ctrl->ae_area);

	ispv1_set_focus_mode_done(get_focus_mode());
	ispv1_set_focus_area_done(&afae_ctrl->af_area);
	if (get_focus_state() == FOCUS_STATE_AF_PREPARING) {
		cur_lum = get_current_y();
		if (((CAMERA_FLASH_AUTO ==  this_ispdata->flash_mode) || (CAMERA_FLASH_ON == this_ispdata->flash_mode))
			&& (0 == ispv1_focus_need_flash(cur_lum))) {
			ispv1_assistant_af(true);
			this_ispdata->assistant_af_flash = true;
			msleep((1000 / framerate) * 2);
			cur_lum = get_current_y();
			while ((frame_count++ < FLASH_TEST_MAX_COUNT)
				&& (cur_lum > FLASH_TEST_OVER_EXPO)) {
				msleep(1000 / framerate);
				cur_lum = get_current_y();
			}
		} else {
			this_ispdata->assistant_af_flash = false;
		}
	}

	if (get_focus_mode() == CAMERA_FOCUS_CONTINUOUS_PICTURE) {
		set_focus_state(FOCUS_STATE_CAF_RUNNING);
		SETREG8(REG_ISP_FOCUS_ACTIVE, FOCUS_START);
		if (afae_ctrl->k3focus_running == false)
			ispv1_k3focus_run();
		else
			print_error("fatal error in %s line %d: state error!", __func__, __LINE__);
	} else if (get_focus_mode() == CAMERA_FOCUS_CONTINUOUS_VIDEO) {
		set_videocaf_forcestart(true);
		set_focus_state(FOCUS_STATE_CAF_DETECTING);
		if (afae_ctrl->k3focus_running == false)
			ispv1_k3focus_run();
		else
			print_error("fatal error in %s line %d: state error!", __func__, __LINE__);
	} else {
		set_focus_state(FOCUS_STATE_AF_RUNNING);
		afae_ctrl->focus_frame_count = 0;
		SETREG8(REG_ISP_FOCUS_ACTIVE, FOCUS_START);
	}
}

static void ispv1_af_hold_work_func(struct work_struct *work)
{
	if (afae_ctrl->k3focus_running == false)
		ispv1_k3focus_run();
	else
		print_error("fatal error in %s line %d: state error!", __func__, __LINE__);
}

static int ispv1_get_merged_rect(focus_area_s *area, camera_rect_s *yuv_rect,
				u32 preview_width, u32 preview_height)
{
	u32 rect_index;
	camera_rect_s *cur_rect;

	yuv_rect->left = preview_width;
	yuv_rect->top = preview_height;
	yuv_rect->width = 0;
	yuv_rect->height = 0;

	for (rect_index = 0; rect_index < area->focus_rect_num; rect_index++) {
		cur_rect = &area->rect[rect_index];
		if (((cur_rect->left + cur_rect->width) > preview_width) ||
		    ((cur_rect->top + cur_rect->height) > preview_height)) {
			return -1;
		}

		if (cur_rect->left < yuv_rect->left)
			yuv_rect->left = cur_rect->left;
		if (cur_rect->top < yuv_rect->top)
			yuv_rect->top = cur_rect->top;

		if ((cur_rect->left + cur_rect->width) > (yuv_rect->left + yuv_rect->width))
			yuv_rect->width = (cur_rect->left + cur_rect->width) - yuv_rect->left;

		if ((cur_rect->top + cur_rect->height) > (yuv_rect->left + yuv_rect->height))
			yuv_rect->height = (cur_rect->top + cur_rect->height) - yuv_rect->top;
	}

	return 0;
}

static int ispv1_get_focus_win_info(camera_rect_s *raw_rect,
				ispv1_focus_win_info_s *win_info, int *binning)
{
	u32 width, height;

	width = raw_rect->width / 5;
	height = raw_rect->height / 5;

	if (width <= 126) {
		if (height <= 126) {
			*binning = 1;
		} else {
			height = 126;
			*binning = 1;
		}
	} else if (width <= 252) {
		if (height <= 126) {
			width = 126;
			*binning = 1;
		} else if (height <= 252) {
			*binning = 0;
		} else {
			height = 252;
			*binning = 0;
		}
	} else {
		if (height <= 126) {
			width = 126;
			*binning = 1;
		} else if (height <= 252) {
			width = 252;
			*binning = 0;
		} else {
			width = 252;
			height = 252;
			*binning = 0;
		}
	}

	win_info->left = raw_rect->left;
	win_info->top = raw_rect->top;
	win_info->width = width;
	win_info->height = height;
	win_info->width1 = raw_rect->width - width;
	win_info->height1 = raw_rect->height - height;

	return 0;
}

static void ispv1_get_raw_win(int index, ispv1_focus_win_info_s *win_info,
			      camera_rect_s *raw_rect)
{
	raw_rect->left = win_info->left + (index % 5) * (win_info->width1 / 4);
	raw_rect->top = win_info->top + (index / 5) * (win_info->height1 / 4);
	raw_rect->width = win_info->width;
	raw_rect->height = win_info->height;
}

/*
 * 判断两个矩形的中心坐标的水平和垂直距离，
 * 只要这两个值满足某种条件就可以相交。
 * 矩形A的宽 Wa = Xa2-Xa1 高 Ha = Ya2-Ya1
 * 矩形B的宽 Wb = Xb2-Xb1 高 Hb = Yb2-Yb1
 * 矩形A的中心坐标 (Xa3,Ya3) = （ (Xa2+Xa1)/2 ，(Ya2+Ya1)/2 ）
 * 矩形B的中心坐标 (Xb3,Yb3) = （ (Xb2+Xb1)/2 ，(Yb2+Yb1)/2 ）
 * 所以只要同时满足下面两个式子，就可以说明两个矩形相交。
 * 1） | Xb3-Xa3 | <= Wa/2 + Wb/2
 * 2） | Yb3-Ya3 | <= Ha/2 + Hb/2
 * 即：
 * | Xb2+Xb1-Xa2-Xa1 | <= Xa2-Xa1 + Xb2-Xb1
 * | Yb2+Yb1-Ya2-Ya1 | <=Y a2-Ya1 + Yb2-Yb1
 *
 * return value: 0--intersection; -1--no intersection.
 */
static int ispv1_check_rect_intersection(camera_rect_s *rect1, camera_rect_s *rect2)
{
	coordinate_s center1, center2;

	center1.x = rect1->left + rect1->width / 2;
	center1.y = rect1->top + rect1->height / 2;

	center2.x = rect2->left + rect2->width / 2;
	center2.y = rect2->top + rect2->height / 2;

	if ((abs(center2.x-center1.x) <= (rect1->width / 2 + rect2->width / 2)) &&
	   (abs(center2.y-center1.y) <= (rect1->height / 2 + rect2->height / 2)))
		return 0;
	else
		return -1;
}

/*
 * Check two rects if there are differ in position or size.
 * return value: 0--have a lot differ; -1--no differ or differ a little.
 */
static int ispv1_check_rect_differ(camera_rect_s *rect1, camera_rect_s *rect2,
				   u32 preview_width, u32 preview_height)
{
	coordinate_s center1, center2;
	u32 width_diff, height_diff;

	center1.x = rect1->left + rect1->width / 2;
	center1.y = rect1->top + rect1->height / 2;

	center2.x = rect2->left + rect2->width / 2;
	center2.y = rect2->top + rect2->height / 2;

	/* if center pointer differ a lot, then it is differ. */
	if ((abs(center2.x - center1.x) > (preview_width / 6)) || (abs(center2.y - center1.y) > (preview_height / 4)))
		return 0;

	/* if size is differ a lot, then it is differ. */
	width_diff = abs(rect1->width - rect2->width);
	height_diff = abs(rect1->height - rect2->height);
	if ((width_diff > rect1->width / 2) || (width_diff > rect2->width / 2)
	    || (height_diff > rect1->height / 2) || (height_diff > rect2->height / 2))
		return 0;

	return -1;
}

static int ispv1_get_map_table(focus_area_s *area,
				ispv1_focus_win_info_s *win_info, int *map_table)
{
	u32 area_idx;
	u32 win_idx;
	camera_rect_s user_rect;
	camera_rect_s isp_rect;
	int ret;

	/* check every area,
	 * it is in which yuv win, maybe in several wins, maybe none
	 */
	for (area_idx = 0; area_idx < area->focus_rect_num; area_idx++) {
		print_debug("map table for region %d:", area_idx);

		/* get each yuv win */
		ret = k3_isp_yuvrect_to_rawrect(&area->rect[area_idx], &user_rect);
		if (ret) {
			print_error("%s:line %d error", __func__, __LINE__);
			return ret;
		}

		for (win_idx = 0; win_idx < ISP_MAX_FOCUS_WIN; win_idx++) {
			/* get each raw win */
			ispv1_get_raw_win(win_idx, win_info, &isp_rect);
			print_debug("isp_rect %d: %d,%d,%d,%d", win_idx,
				isp_rect.left, isp_rect.top, isp_rect.width, isp_rect.height);

			ret = ispv1_check_rect_intersection(&user_rect, &isp_rect);
			if (ret == 0) {
				*(map_table + area_idx * ISP_MAX_FOCUS_WIN + win_idx) = 1;
				win_info->weight[win_idx] = 1;
			} else
				*(map_table + area_idx * ISP_MAX_FOCUS_WIN + win_idx) = 0;
		}
	}

	return 0;
}

/* changed 2012-03-15 for zero size rect*/
static int ispv1_focus_get_default_yuvrect(camera_rect_s *rectin, u32 preview_width, u32 preview_height)
{
	rectin->left = preview_width * (100 - DEFAULT_AF_WIDTH_PERCENT) / 200;
	rectin->top = preview_height * (100 - DEFAULT_AF_HEIGHT_PERCENT) / 200;
	rectin->width = preview_width * DEFAULT_AF_WIDTH_PERCENT / 100;
	rectin->height = preview_height * DEFAULT_AF_HEIGHT_PERCENT / 100;

	return 0;
}
static int ispv1_focus_adjust_yuvrect(camera_rect_s *yuv)
{
	u32 height = yuv->height;
	u32 width = yuv->height;

	print_debug("before center_top:%d, center_left:%d, yuv->height:%d; yuv->width:%d", yuv->top + yuv->height / 2,
		yuv->left + yuv->width / 2, yuv->height, yuv->width);
	if ((yuv->height >= 180) || (yuv->width >= 180)) {
		height = yuv->height * 4 / 5;
		width = yuv->width * 4 / 5;
		yuv->top = yuv->top + (yuv->height - height) / 2;
		yuv->left = yuv->left + (yuv->width - width) / 2;
		yuv->height = height;
		yuv->width = width;
	}
	print_debug("after center_top:%d, center_left:%d, yuv->height:%d; yuv->width:%d", yuv->top + yuv->height / 2,
		yuv->left + yuv->width / 2, yuv->height, yuv->width);
	return 0;
}

static int ispv1_focus_adjust_rawwin(ispv1_focus_win_info_s *win_info, u32 raw_width, u32 raw_height)
{
	if (win_info->left < 24) {
		win_info->left = 24;
		if (win_info->width > 72) {
			win_info->width -= 24;
		} else {
			win_info->width = 48;
		}
	}

	if (win_info->top < 24) {
		win_info->top = 24;
		if (win_info->height > 72) {
			win_info->height -= 24;
		} else {
			win_info->height = 48;
		}
	}

	if ((win_info->top + win_info->height) > raw_height)
		win_info->top -= (win_info->top + win_info->height) - raw_height;

	if ((win_info->left + win_info->width) > raw_width)
		win_info->left -= (win_info->left + win_info->width) - raw_width;

	/* win startx+starty should be odd. */
	if ((win_info->left + win_info->top) % 2 == 0) {
		win_info->left += 1;
	}

	/* width and height should be mutiple of 6 */
	win_info->width -= (win_info->width % 6);
	win_info->height -= (win_info->height % 6);

	return 0;
}

int ispv1_set_vcm_parameters(camera_focus focus_mode)
{
	int ret = 0;
	vcm_info_s *vcm = this_ispdata->sensor->vcm;

	print_debug("enter %s, focus_mode:%d", __func__, focus_mode);

	/*
	 * should update following params, such as:
	 * offsetInit/fullRange/coarseStep/fineStep/frameRate
	 */
	switch (focus_mode) {
	case CAMERA_FOCUS_AUTO:
	case CAMERA_FOCUS_MACRO:
	case CAMERA_FOCUS_CONTINUOUS_PICTURE:
		vcm->offsetInit = vcm->infiniteDistance;
		vcm->fullRange = vcm->normalDistanceEnd;
		vcm->moveRange = RANGE_NORMAL;
		break;

	case CAMERA_FOCUS_INFINITY:
		vcm->offsetInit = vcm->infiniteDistance;
		vcm->fullRange = vcm->infiniteDistance + vcm->normalStep * 2;
		vcm->moveRange = RANGE_INFINITY;
		break;

	case CAMERA_FOCUS_CONTINUOUS_VIDEO:
		vcm->offsetInit = vcm->infiniteDistance;
		vcm->fullRange = vcm->videoDistanceEnd;
		vcm->moveRange = RANGE_NORMAL;
		break;
	case CAMERA_FOCUS_FIXED:
		break;
	case CAMERA_FOCUS_EDOF:
		ret = -1;
		print_error("focus mode not supported: %d", focus_mode);
		break;
	default:
		ret = -1;
		print_error("focus range mode unknow: %d", focus_mode);
		break;
	}

	if (ret == -1)
		return ret;

	if (CAMERA_FOCUS_CONTINUOUS_VIDEO == focus_mode) {
		vcm->coarseStep = vcm->videoStep;
	} else if ((RANGE_NORMAL == vcm->moveRange) || (RANGE_MACRO == vcm->moveRange)) {
		vcm->coarseStep = vcm->normalStep;
	} else if (RANGE_INFINITY == vcm->moveRange) {
		vcm->coarseStep = vcm->normalStep;
	}

	vcm->fineStep = vcm->coarseStep / 2;

	return ret;
}


int ispv1_set_focus_mode(camera_focus focus_mode)
{
	print_info("Enter %s, scene_type %d, focus mode %d", __func__, this_ispdata->scene, focus_mode);

	if (CAMERA_SCENE_AUTO == this_ispdata->scene) {
		/*
		 * When focus scene type is auto, and need to change focus range
		 * then excute this part
		 */
		ispv1_set_focus_range(focus_mode);
	}
	set_focus_mode(focus_mode);
	return 0;
}

/*
 **************************************************************************
 * FunctionName: ispv1_set_focus_mode_done;
 * Description : called by ispv1_auto_focus();
 * Input       : NA;
 * Output      : NA;
 * ReturnValue : NA;
 * Other       : NA;
 **************************************************************************
 */
static int ispv1_set_focus_mode_done(camera_focus focus_mode)
{
	int ret = 0;
	camera_sensor *sensor = this_ispdata->sensor;
	vcm_info_s *vcm = this_ispdata->sensor->vcm;

	print_info("enter %s, focus mode %d", __func__, focus_mode);

	if (!sensor->af_enable) {
		print_error("This sensor not support AF!");
		return -1;
	}
	ret = ispv1_set_vcm_parameters(focus_mode);

	if (ret == -1)
		return ret;

	/* set them to registers */
	ispv1_setreg_vcm(vcm);

	if ((focus_mode == CAMERA_FOCUS_CONTINUOUS_VIDEO) ||
	    (focus_mode == CAMERA_FOCUS_CONTINUOUS_PICTURE))
		ret = ispv1_setreg_af_mode(CAMERA_VIDEO_SERVO_MODE);
	else if (afae_ctrl->multi_win)
		ret = ispv1_setreg_af_mode(CAMERA_WEIGHTED_SNAPSHOT_MODE);
	else
		ret = ispv1_setreg_af_mode(CAMERA_SINGLE_SNAPSHOT_MODE);

	ispv1_setreg_focus_sensitivity(focus_mode);

	if (ret) {
		print_error("%s:line %d error", __func__, __LINE__);
		return -EINVAL;
	}

	return ret;
}

/*
 * ispv1_set_focus_area just save focus area
 */
int ispv1_set_focus_area(focus_area_s *area)
{
	u32 preview_width = this_ispdata->pic_attr[STATE_PREVIEW].out_width;
	u32 preview_height = this_ispdata->pic_attr[STATE_PREVIEW].out_height;
	camera_rect_s *current_rect;
	camera_rect_s *previous_rect;
	int ret = 0;

	print_debug("enter %s", __func__);

	/* y36721 0229 add */
	if (get_focus_state() == FOCUS_STATE_CAF_RUNNING ||
		get_focus_state() == FOCUS_STATE_CAF_DETECTING) {
		/*
		 * If caf is running and CAF area changed,
		 * such as face is detected,
		 * should change focus area and force start.
		 */

		/*If it is CAF and new area is differ a little with previous,
		 * should ommit it
		 */
#ifndef AF_SINGLE_WINMODE_ONLY
		current_rect = &area->rect[0];
		previous_rect = &afae_ctrl->af_area.rect[0];
#else
		current_rect = &area->rect[area->focus_rect_num - 1];
			previous_rect = &afae_ctrl->af_area.rect[area->focus_rect_num - 1];
#endif

		ret = ispv1_check_rect_differ(current_rect, previous_rect, preview_width, preview_height);
		if (ret == -1) {
			/* if differ a little, just return. */
			print_info("CAF rect change a little, ommit it");
			return 0;
		}

		ispv1_set_focus_area_done(area);
		ispv1_setreg_focus_forcestart(1);
	}

	memcpy(&afae_ctrl->af_area, area, sizeof(focus_area_s));

	return 0;
}

/*
 **************************************************************************
 * FunctionName: ispv1_set_focus_area_done;
 * Description : set focus area
 * Input       : area: area information.
 * Output      : NA;
 * ReturnValue : 0 success, -1 failed
 * Other       : NA;
 **************************************************************************
 */
static int ispv1_set_focus_area_done(focus_area_s *area)
{
	u32 preview_width = this_ispdata->pic_attr[STATE_PREVIEW].out_width;
	u32 preview_height = this_ispdata->pic_attr[STATE_PREVIEW].out_height;
	u32 raw_width = this_ispdata->pic_attr[STATE_PREVIEW].in_width;
	u32 raw_height = this_ispdata->pic_attr[STATE_PREVIEW].in_height;
	camera_rect_s cur_rect, raw_rect;
	/*camera_rect_s merged_yuv_rect;*/
	ispv1_focus_win_info_s win_info;
	int binning = 0;
	int multi_win = 0;
	int index;
	int ret;

	/*int *map_table = afae_ctrl->map_table;*/

	print_debug("enter %s", __func__);

	print_debug("focus_area: focus_area_num %d", area->focus_rect_num);
	for (index = 0; index < area->focus_rect_num; index++) {
		print_info("focus rect %d:%d,%d,%d,%d,%d", index,
					area->rect[index].left,
					area->rect[index].top,
					area->rect[index].width,
					area->rect[index].height,
					area->rect[index].weight);
	}

	memset(&win_info, 0, sizeof(ispv1_focus_win_info_s));

	/*y36721 changed for supporting one windows only. */
#ifndef AF_SINGLE_WINMODE_ONLY
	multi_win = (area->focus_rect_num > 1) ? 1 : 0;
#else
	multi_win = 0;
#endif

	print_debug("%s, line %d: multi_win %d", __func__, __LINE__, multi_win);

	if (multi_win == 0) {
		/*y36721 changed for supporting one windows only.*/
		#ifndef AF_SINGLE_WINMODE_ONLY
			memcpy(&cur_rect, &area->rect[0], sizeof(camera_rect_s));
		#else
			memcpy(&cur_rect, &area->rect[area->focus_rect_num - 1], sizeof(camera_rect_s));
		#endif

		/* check width and height are valid, then adjust it. */
		if ((cur_rect.width == 0) || (cur_rect.height == 0)) {
			ispv1_focus_get_default_yuvrect(&cur_rect, preview_width, preview_height);
			print_debug("default yuv rect:%d,%d,%d,%d",
			    cur_rect.left, cur_rect.top, cur_rect.width, cur_rect.height);
		}

		/* most case is just one focus rect. */
		if (((cur_rect.left + cur_rect.width) > preview_width) ||
			((cur_rect.top + cur_rect.height) > preview_height)) {
			print_error("%s, line %d: rect area error!", __func__, __LINE__);
			return -1;
		}

		/* convert yuv rect to raw rect. */
		//ispv1_focus_adjust_yuvrect(&cur_rect);
		ret = k3_isp_yuvrect_to_rawrect(&cur_rect, &raw_rect);
		if (ret) {
			print_error("%s, line %d: error", __func__, __LINE__);
			return ret;
		}

		win_info.left = raw_rect.left;
		win_info.top = raw_rect.top;
		win_info.width = raw_rect.width;
		win_info.height = raw_rect.height;
		win_info.width1 = 0;
		win_info.height1 = 0;

		print_debug("win_info before %d:%d:%d:%d", win_info.left, win_info.top, win_info.width, win_info.height);

		goto setreg_out;
	}
#ifndef AF_SINGLE_WINMODE_ONLY
	/*
	 * Get a YUV area include all of user defined rects
	 * If there is any rect is out of range, then return false.
	 */
	ret = ispv1_get_merged_rect(area, &merged_yuv_rect, preview_width, preview_height);
	if (ret) {
		print_error("%s:line %d error", __func__, __LINE__);
		return ret;
	}

	//ispv1_focus_adjust_yuvrect(&merged_yuv_rect);
	/* convert yuv rect to raw rect. */
	ret = k3_isp_yuvrect_to_rawrect(&merged_yuv_rect, &raw_rect);
	if (ret) {
		print_error("%s:line %d error", __func__, __LINE__);
		return ret;
	}

	/* caculate ISP focus windows information, include binning flag */
	ret = ispv1_get_focus_win_info(&raw_rect, &win_info, &binning);
	if (ret) {
		print_error("%s:line %d error", __func__, __LINE__);
		return ret;
	}

	/* Map user defined rects to ISP defined rects */
	ret = ispv1_get_map_table(area, &win_info, map_table);
	if (ret) {
		print_error("%s:line %d error", __func__, __LINE__);
		return ret;
	}
#endif

setreg_out:
	afae_ctrl->binning = binning;
	afae_ctrl->multi_win = multi_win;

	/*Added by y36721 for adjust focus windows 2012-02-16.*/
	ispv1_focus_adjust_rawwin(&win_info, raw_width, raw_height);
	print_debug("win_info after %d:%d:%d:%d", win_info.left, win_info.top, win_info.width, win_info.height);

	/* Set ISP defined rects to ISP register */
	ispv1_setreg_focus_win(&win_info);
	memcpy(&afae_ctrl->cur_rect, &cur_rect, sizeof(camera_rect_s));

	return 0;
}

/*
 **************************************************************************
 * FunctionName: ispv1_get_focus_result;
 * Description : get focus result
 * Input       : NA;
 * Output      : NA;
 * ReturnValue : STATUS_FOCUSING, STATUS_FOCUSED, STATUS_OUT_FOCUS
 * Other       : NA;
 **************************************************************************
 */
int ispv1_get_focus_result(focus_result_s *result)
{
	u32 win_idx = 0;
	u32 rect_idx = 0;
	int status;
	ispv1_focused_result_s isp_result;
	int ret = 0;

	focus_area_s *area = &afae_ctrl->af_area;
	int *map_table = afae_ctrl->map_table;

	print_debug("enter %s", __func__);

	assert(result);

	memset(result, 0, sizeof(focus_result_s));
	afae_ctrl->focus_failed = 0;

	status = ispv1_getreg_focus_result(&isp_result, afae_ctrl->multi_win);
	if (status != STATUS_FOCUSED) {
		result->status = status;

		/* added reason: if last time out focus, then maybe also not run in continus mode */
		if ((result->status == STATUS_OUT_FOCUS) && (get_focus_mode() == CAMERA_FOCUS_AUTO)) {
			print_info("focus failed");
			afae_ctrl->focus_failed = 1;
		}

		ret = -1;
		goto out;
	}

	/* If it is focused and single win mode, then just set status and each_status[0]. */
	if (afae_ctrl->multi_win == 0) {
		result->status = STATUS_FOCUSED;

		/*y36721 changed for supporting one windows only.*/
		#ifndef AF_SINGLE_WINMODE_ONLY
			result->each_status[0] = STATUS_FOCUSED;
		#else
			for (win_idx = 0; win_idx < isp_result.focused_win_num; win_idx++) {
				result->each_status[win_idx] = STATUS_FOCUSED;
			}
		#endif
	} else {
		/*
		 * If it is focused and multi win mode,
		 * then search map table and set result.
		 * use ISP defined win id to get user defined rect id.
		 */
		result->status = STATUS_FOCUSED;
		for (win_idx = 0; win_idx < isp_result.focused_win_num; win_idx++) {
			for (rect_idx = 0; rect_idx < area->focus_rect_num; rect_idx++) {
				if (*(map_table + rect_idx * ISP_MAX_FOCUS_WIN + isp_result.focused_win[win_idx]) == 1) {
					result->each_status[rect_idx] = STATUS_FOCUSED;
				}
			}
		}
	}

out:
	return ret;
}

/*
 * Default metering rect is like that:
 * SPOT is rect of quarter width and quarter height
 * CWA is  rect of half width and half height
 * AVERAGE is all of preview region;
 */
static int ispv1_get_default_metering_rect(camera_metering metering,
				camera_rect_s *yuv, u32 out_width, u32 out_height)
{
	int retvalue = 0;

	print_debug("enter %s", __func__);

	switch (metering) {
	case CAMERA_METERING_SPOT:
		yuv->width = out_width * METERING_SPOT_WIDTH_PERCENT / 100;
		yuv->height = out_height * METERING_SPOT_HEIGHT_PERCENT / 100;
		yuv->left = out_width * (100 - METERING_SPOT_WIDTH_PERCENT) / 200;
		yuv->top = out_height * (100 - METERING_SPOT_HEIGHT_PERCENT) / 200;
		break;

	case CAMERA_METERING_CWA:
		yuv->width = out_width * METERING_CWA_WIDTH_PERCENT / 100;
		yuv->height = out_height * METERING_CWA_HEIGHT_PERCENT / 100;
		yuv->left = out_width * (100 - METERING_CWA_WIDTH_PERCENT) / 200;
		yuv->top = out_height * (100 - METERING_CWA_HEIGHT_PERCENT) / 200;
		break;

	case CAMERA_METERING_AVERAGE:
		yuv->width = out_width;
		yuv->height = out_height;
		yuv->left = 0;
		yuv->top = 0;
		break;

	default:
		retvalue = -1;
		print_error("metering mode invalid!");
		goto out;
		break;
	}

out:
	return retvalue;

}

/*
 * Called by ispv1_set_metering_area
 */
int ispv1_setreg_metering_area(camera_rect_s *raw, u32 raw_width, u32 raw_height, int roi)
{
	print_debug("enter %s", __func__);

	/*
	 * Just care about center 3x3 windows. 3x3 weight is all 1.
	 * Long&short exposure are same
	 * set raw rect to ISP registers
	 */
	if (raw->height < 80)
		raw->height = 80;

	if (raw->width < 80)
		raw->width = 80;

	if ((raw->top + raw->height) > raw_height)
		raw->top -= (raw->top + raw->height) - raw_height;

	if ((raw->left + raw->width) > raw_width)
		raw->left -= (raw->left + raw->width) - raw_width;

	if (roi == 0) {
		SETREG16(REG_ISP_AECAGC_CENTER_LEFT, raw->left);
		SETREG16(REG_ISP_AECAGC_CENTER_LEFT_SHORT, raw->left);
		SETREG16(REG_ISP_AECAGC_CENTER_TOP, raw->top);
		SETREG16(REG_ISP_AECAGC_CENTER_TOP_SHORT, raw->top);

		SETREG16(REG_ISP_AECAGC_CENTER_WIDTH, raw->width);
		SETREG16(REG_ISP_AECAGC_CENTER_WIDTH_SHORT, raw->width);

		SETREG16(REG_ISP_AECAGC_CENTER_HEIGHT, raw->height);
		SETREG16(REG_ISP_AECAGC_CENTER_HEIGHT_SHORT, raw->height);

		SETREG8(REG_ISP_AECAGC_ROI_WEIGHT_IN, 1);
		SETREG8(REG_ISP_AECAGC_ROI_WEIGHT_IN_SHORT, 1);
		SETREG8(REG_ISP_AECAGC_ROI_WEIGHT_OUT, 1);
		SETREG8(REG_ISP_AECAGC_ROI_WEIGHT_OUT_SHORT, 1);

		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(0), 1);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(1), 1);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(2), 1);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(3), 1);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(4), 2);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(5), 2);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(6), 2);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(7), 2);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(8), 4);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(9), 2);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(10), 2);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(11), 2);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(12), 2);

		//weight shift
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT_SHIFT, 3);
	} else {
	#if 0 //roi mode
		SETREG16(REG_ISP_AECAGC_ROI_LEFT, raw->left);
		SETREG16(REG_ISP_AECAGC_ROI_LEFT_SHORT, raw->left);
		SETREG16(REG_ISP_AECAGC_ROI_TOP, raw->top);
		SETREG16(REG_ISP_AECAGC_ROI_TOP_SHORT, raw->top);

		SETREG16(REG_ISP_AECAGC_ROI_RIGHT, (raw_width - (raw->left + raw->width)));
		SETREG16(REG_ISP_AECAGC_ROI_RIGHT_SHORT, (raw_width - (raw->left + raw->width)));
		SETREG16(REG_ISP_AECAGC_ROI_BOTTOM, (raw_height - (raw->top + raw->height)));
		SETREG16(REG_ISP_AECAGC_ROI_BOTTOM_SHORT, (raw_height - (raw->top + raw->height)));

		SETREG8(REG_ISP_AECAGC_ROI_WEIGHT_IN, 1);
		SETREG8(REG_ISP_AECAGC_ROI_WEIGHT_IN_SHORT, 1);
		SETREG8(REG_ISP_AECAGC_ROI_WEIGHT_OUT, 0);
		SETREG8(REG_ISP_AECAGC_ROI_WEIGHT_OUT_SHORT, 0);
	#else //enhanced 3x3 win mode
		SETREG16(REG_ISP_AECAGC_CENTER_LEFT, raw->left);
		SETREG16(REG_ISP_AECAGC_CENTER_LEFT_SHORT, raw->left);
		SETREG16(REG_ISP_AECAGC_CENTER_TOP, raw->top);
		SETREG16(REG_ISP_AECAGC_CENTER_TOP_SHORT, raw->top);

		SETREG16(REG_ISP_AECAGC_CENTER_WIDTH, raw->width);
		SETREG16(REG_ISP_AECAGC_CENTER_WIDTH_SHORT, raw->width);

		SETREG16(REG_ISP_AECAGC_CENTER_HEIGHT, raw->height);
		SETREG16(REG_ISP_AECAGC_CENTER_HEIGHT_SHORT, raw->height);

		SETREG8(REG_ISP_AECAGC_ROI_WEIGHT_IN, 1);
		SETREG8(REG_ISP_AECAGC_ROI_WEIGHT_IN_SHORT, 1);
		SETREG8(REG_ISP_AECAGC_ROI_WEIGHT_OUT, 1);
		SETREG8(REG_ISP_AECAGC_ROI_WEIGHT_OUT_SHORT, 1);

		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(0), 1);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(1), 1);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(2), 1);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(3), 1);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(4), 0xff);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(5), 0xff);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(6), 0xff);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(7), 0xff);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(8), 0xff);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(9), 0xff);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(10), 0xff);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(11), 0xff);
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT(12), 0xff);

		//weight shift
		SETREG8(REG_ISP_AECAGC_WIN_WEIGHT_SHIFT, 8);
	#endif
	}

	return 0;
}

void ispv1_setreg_ae_statwin(u32 x_offset, u32 y_offset)
{
	SETREG16(REG_ISP_AECAGC_STATWIN_LEFT, x_offset);
	SETREG16(REG_ISP_AECAGC_STATWIN_TOP, y_offset);
	SETREG16(REG_ISP_AECAGC_STATWIN_RIGHT, x_offset);
	SETREG16(REG_ISP_AECAGC_STATWIN_BOTTOM, y_offset);
}

int ispv1_set_ae_statwin(pic_attr_t *pic_attr)
{
	camera_rect_s yuv;
	camera_rect_s raw;
	u32 x_offset, y_offset;

	yuv.left = 0;
	yuv.top = 0;
	yuv.width = pic_attr->out_width;
	yuv.height = pic_attr->out_height;
	if (k3_isp_yuvrect_to_rawrect(&yuv, &raw)) {
		print_error("%s:line %d error", __func__, __LINE__);
		return -1;
	}

	x_offset = (pic_attr->in_width - raw.width) / 2 + DEFAULT_AECAGC_STATWIN_XOFFSET;
	y_offset = (pic_attr->in_height - raw.height) / 2 + DEFAULT_AECAGC_STATWIN_YOFFSET;

	ispv1_setreg_ae_statwin(x_offset, y_offset);
	return 0;
}

int ispv1_set_gsensor_stat(axis_triple *xyz)
{
	memcpy(&mXYZ, xyz, sizeof(axis_triple));
	return 0;
}

/*
 * Added for metering mode
 * spot, CWA, average
 */
int ispv1_set_metering_mode(camera_metering mode)
{
	print_debug("enter %s, mode %d", __func__, mode);

	this_metering = mode;
	return 0;
}

int ispv1_set_metering_area(metering_area_s *area)
{
	metering_area_s *metering_area = &afae_ctrl->ae_area;

	print_debug("enter %s", __func__);
	memcpy(metering_area, area, sizeof(metering_area_s));

	if (get_focus_state() != FOCUS_STATE_AF_HOLDING &&
	    get_focus_state() != FOCUS_STATE_CAF_HOLDING) {
		ispv1_set_metering_area_done(area);
	}

	return 0;
}

static int ispv1_set_metering_area_done(metering_area_s *area)
{
	u32 loopi;
	u32 preview_width = this_ispdata->pic_attr[STATE_PREVIEW].out_width;
	u32 preview_height = this_ispdata->pic_attr[STATE_PREVIEW].out_height;
	u32 in_width = this_ispdata->pic_attr[STATE_PREVIEW].in_width;
	u32 in_height = this_ispdata->pic_attr[STATE_PREVIEW].in_height;

	camera_rect_s yuv;
	camera_rect_s raw;
	int roi_flag = 0;

	print_debug("enter %s", __func__);

	print_debug("metering_rect_num %d", area->metering_rect_num);
	for (loopi = 0; loopi < area->metering_rect_num; loopi++) {
		print_info("metering rect %d: %d,%d,%d,%d,weight %d",
			   loopi,
			   area->rect[loopi].left, area->rect[loopi].top,
			   area->rect[loopi].width, area->rect[loopi].height,
			   area->rect[loopi].weight);
	}

	/* y36721 2012-03-28
	 * set ROI metering before take picture when auto focus not selected.
	 */
	memcpy(&yuv, &area->rect[0], sizeof(camera_rect_s));

	/* check width and height are valid, then adjust it. */
	if ((yuv.width == 0) || (yuv.height == 0)) {
		ispv1_get_default_metering_rect(this_metering, &yuv, preview_width, preview_height);
		roi_flag = 0;
	} else {
		roi_flag = 1;
	}

	if (k3_isp_yuvrect_to_rawrect(&yuv, &raw)) {
		print_error("%s:line %d error", __func__, __LINE__);
		return -1;
	}
	ispv1_setreg_metering_area(&raw, in_width, in_height, roi_flag);

	return 0;
}
static void ispv1_wait_caf_done(void)
{
	ispv1_focused_result_s isp_result;
	int status = STATUS_FOCUSING;
	int loop_count = 0;

	print_debug("Enter %s, afae_ctrl->focus_state:%d", __func__, get_focus_state());

	if (get_focus_mode() == CAMERA_FOCUS_CONTINUOUS_PICTURE &&
		get_focus_state() != FOCUS_STATE_STOPPED) {
		status = ispv1_getreg_focus_result(&isp_result, afae_ctrl->multi_win);

		while ((STATUS_FOCUSING == status) && (loop_count < 50)) {
			msleep(10);
			loop_count++;
			status = ispv1_getreg_focus_result(&isp_result, afae_ctrl->multi_win);
		}

		if (loop_count != 0)
			print_info("%s, loop_count:%d", __func__, loop_count);
	}
}


int ispv1_set_focus_range(camera_focus focus_mode)
{
	int ret = 0;
	int val_vcm = 0;
	vcm_info_s *vcm = this_ispdata->sensor->vcm;

	print_debug("enter %s, focus_mode:%d", __func__, focus_mode);
	ret = ispv1_set_vcm_parameters(focus_mode);

	if (ret == -1)
		return ret;

	GETREG16(REG_ISP_FOCUS_MOTOR_CURR, val_vcm);
	if ((val_vcm < vcm->offsetInit) || (val_vcm > vcm->fullRange) \
		|| (focus_mode == CAMERA_FOCUS_CONTINUOUS_VIDEO)) {
		/* fixbug: change scene maybe cause CAF not stop */
		print_info("excute %s", __func__);
		ispv1_setreg_vcm_code(vcm->offsetInit);
		msleep(30);
		ispv1_setreg_focus_forcestart(1);
	}
	/* set vcm params to registers */
	ispv1_setreg_vcm(vcm);
	return ret;
}

/*
 **************************************************************************
 * FunctionName: ispv1_get_focus_distance;
 * Description : get focus_distance value ( cm )
 * Input       : NA;
 * Output      : NA;
 * ReturnValue : NA;
 * Other       : NA;
 **************************************************************************
 */
int ispv1_get_focus_distance(void)
{
	/* just reserve interface */
	return 0;
}
static int ispv1_focus_need_flash(u8 current_y)
{
	int ret;
	if (current_y < 0x14)
		ret = 0;
	else
		ret = -1;
	return ret;
}


static void ispv1_assistant_af(bool action)
{
	camera_flashlight *flashlight = get_camera_flash();
	print_debug("enter %s,action:%d", __func__, action);
	if (true == action) {
		flashlight->turn_on(TORCH_MODE, LUM_LEVEL2);
	} else {
		flashlight->turn_off();
	}
}

int ispv1_focus_status_collect(focus_frame_stat_s *curr_data, focus_frame_stat_s *mean_data)
{
	int loopi;
	u32 variance[4] = {0, 0, 0, 0};
	u32 contrast_diff = 0;
	axis_triple xyz_diff = {0, 0, 0};

	afae_ctrl->focus_stat_frames++;

	/* skip first frame, and get mean of next 4 frames contrast/gain/expo/lum value as compare data. */
	if ((afae_ctrl->focus_stat_frames >= CAF_STAT_COMPARE_START_FRAME)
		&& (afae_ctrl->focus_stat_frames <= CAF_STAT_COMPARE_END_FRAME)) {
		afae_ctrl->compare_data.contrast += curr_data->contrast;
		afae_ctrl->compare_data.ae += curr_data->ae;
		afae_ctrl->compare_data.lum += curr_data->lum;
		afae_ctrl->compare_data.rbratio += curr_data->rbratio;
		afae_ctrl->compare_data.xyz.x += curr_data->xyz.x;
		afae_ctrl->compare_data.xyz.y += curr_data->xyz.y;
		afae_ctrl->compare_data.xyz.z += curr_data->xyz.z;

		if (afae_ctrl->focus_stat_frames == CAF_STAT_COMPARE_END_FRAME) {
			afae_ctrl->compare_data.contrast /= CAF_STAT_COMPARE_FRAMES;
			afae_ctrl->compare_data.ae /= CAF_STAT_COMPARE_FRAMES;
			afae_ctrl->compare_data.lum /= CAF_STAT_COMPARE_FRAMES;
			afae_ctrl->compare_data.rbratio /= CAF_STAT_COMPARE_FRAMES;
			afae_ctrl->compare_data.xyz.x /= CAF_STAT_COMPARE_FRAMES;
			afae_ctrl->compare_data.xyz.y /= CAF_STAT_COMPARE_FRAMES;
			afae_ctrl->compare_data.xyz.z /= CAF_STAT_COMPARE_FRAMES;

			/*
			 * init all frame's contrast/gain/expo/lum array as compare data.
			 * used in preview tasklet to judge scene switch.
			 */
			for (loopi = 0; loopi < CAF_STAT_FRAME; loopi++) {
				memcpy(&afae_ctrl->frame_stat[loopi], &afae_ctrl->compare_data,
					sizeof(focus_frame_stat_s));
			}
		}
	}

	if (afae_ctrl->focus_stat_frames <= CAF_STAT_SKIP_FRAME)
		return -1;

	/* update new contrast/gain/expo/lum value array */
	memcpy(&afae_ctrl->frame_stat[0], &afae_ctrl->frame_stat[1],
		(CAF_STAT_FRAME - 1) * sizeof(focus_frame_stat_s));

	memcpy(&afae_ctrl->frame_stat[CAF_STAT_FRAME - 1], curr_data,
		sizeof(focus_frame_stat_s));

	/* calculate current mean contrast/gain/expo/lum */
	memset(mean_data, 0, sizeof(focus_frame_stat_s));
	for (loopi = 0; loopi < CAF_STAT_FRAME; loopi++) {
		mean_data->contrast += afae_ctrl->frame_stat[loopi].contrast;
		mean_data->ae += afae_ctrl->frame_stat[loopi].ae;
		mean_data->lum += afae_ctrl->frame_stat[loopi].lum;
		mean_data->rbratio += afae_ctrl->frame_stat[loopi].rbratio;
		mean_data->xyz.x += afae_ctrl->frame_stat[loopi].xyz.x;
		mean_data->xyz.y += afae_ctrl->frame_stat[loopi].xyz.y;
		mean_data->xyz.z += afae_ctrl->frame_stat[loopi].xyz.z;
	}

	mean_data->contrast /= CAF_STAT_FRAME;
	mean_data->ae /= CAF_STAT_FRAME;
	mean_data->lum /= CAF_STAT_FRAME;
	mean_data->rbratio /= CAF_STAT_FRAME;
	mean_data->xyz.x /= CAF_STAT_FRAME;
	mean_data->xyz.y /= CAF_STAT_FRAME;
	mean_data->xyz.z /= CAF_STAT_FRAME;

	for (loopi = 0; loopi < CAF_STAT_FRAME; loopi++) {
		variance[0] = abs(afae_ctrl->frame_stat[loopi].contrast - mean_data->contrast);
		variance[1] = abs(afae_ctrl->frame_stat[loopi].xyz.x - mean_data->xyz.x);
		variance[2] = abs(afae_ctrl->frame_stat[loopi].xyz.y - mean_data->xyz.y);
		variance[3] = abs(afae_ctrl->frame_stat[loopi].xyz.z - mean_data->xyz.z);

		variance[0] *= variance[0];
		variance[1] *= variance[1];
		variance[2] *= variance[2];
		variance[3] *= variance[3];

		contrast_diff += variance[0];
		xyz_diff.x += variance[1];
		xyz_diff.y += variance[2];
		xyz_diff.z += variance[3];
	}
	contrast_diff /= CAF_STAT_FRAME;
	xyz_diff.x /= CAF_STAT_FRAME;
	xyz_diff.y /= CAF_STAT_FRAME;
	xyz_diff.z /= CAF_STAT_FRAME;

	mean_data->contrast_var = contrast_diff;
	mean_data->xyz_var.x = xyz_diff.x;
	mean_data->xyz_var.y = xyz_diff.y;
	mean_data->xyz_var.z = xyz_diff.z;

	return 0;
}

static void ispv1_focus_status_reset(void)
{
	afae_ctrl->focus_stat_frames = 0;
	afae_ctrl->force_start = 0;
	memset(&afae_ctrl->compare_data, 0, sizeof(focus_frame_stat_s));
}

bool ispv1_check_caf_need_trigger(focus_frame_stat_s *compare_data, focus_frame_stat_s *mean_data)
{
	focus_frame_stat_s diff;

	u8 unpeace = 0;
	u16 force_start = afae_ctrl->force_start;

	/* if current is too dark, should not trigger CAF */
	if (mean_data->lum <= 0x08)
		return false;

	if ((force_start && 0xff00) == 0) {
		/* if diff too much, should force focus start. */
		diff.contrast = abs(mean_data->contrast - compare_data->contrast);
		diff.ae = abs(mean_data->ae - compare_data->ae);
		diff.lum = abs(mean_data->lum - compare_data->lum);
		diff.rbratio= abs(mean_data->rbratio - compare_data->rbratio);

		diff.xyz.x = abs(mean_data->xyz.x - compare_data->xyz.x);
		diff.xyz.y = abs(mean_data->xyz.y - compare_data->xyz.y);
		diff.xyz.z = abs(mean_data->xyz.z - compare_data->xyz.z);

		if (diff.contrast >= (compare_data->contrast / 3))
			force_start |= 0x01;
		if (diff.ae >= (compare_data->ae / 4))
			force_start |= 0x02;
		if (diff.lum >= (compare_data->lum / 4))
			force_start |= 0x04;
		if (diff.rbratio >= (compare_data->rbratio / 10))
			force_start |= 0x08;

		if (diff.xyz.x >= 0x20)
			force_start |= 0x10;
		if (diff.xyz.y >= 0x20)
			force_start |= 0x20;
		if (diff.xyz.z >= 0x20)
			force_start |= 0x40;
	}

	if (mean_data->contrast_var > (mean_data->contrast * mean_data->contrast / 25))
		unpeace |= 0x01;
	if (mean_data->xyz_var.x > 256)
		unpeace |= 0x10;
	if (mean_data->xyz_var.y > 256)
		unpeace |= 0x20;
	if (mean_data->xyz_var.z > 256)
		unpeace |= 0x40;

	if (force_start && unpeace == 0) {
		print_info("CAF scene changed reason: 0x%.4x", force_start);
		return true;
	} else {
		if (force_start)
			print_info("force_start 0x%x, but unpeace 0x%x", force_start, unpeace);
		return false;
	}
}

/*
 **************************************************************************
 * FunctionName: ispv1_check_caf_need_restart
 * Description : decide whether restart caf
 * Input       : NA;
 * Output      : NA;
 * ReturnValue : ret: -1 do not restart caf, 0 restart caf.
 * Other       : NA;
 **************************************************************************
 */
bool ispv1_check_caf_need_restart(focus_frame_stat_s *start_data, focus_frame_stat_s *end_data)
{
	u32 diff_ae;
	u32 diff_lum;
	u32 diff_rbratio;
	u16 force_start = 0;

	/* if current is too dark, should not trigger CAF */
	if (end_data->lum <= 0x08)
		return false;

	/* when changed frame rate, care nothing */
	if (start_data->fps != end_data->fps)
		return false;

	/* if diff too much, should force focus restart. */
	diff_ae = abs(start_data->ae - end_data->ae);
	diff_lum = abs(start_data->lum - end_data->lum);
	diff_rbratio= abs(start_data->rbratio - end_data->rbratio);

	if (diff_ae >= start_data->ae / 4)
		force_start |= 0x0400;
	if (diff_ae >= end_data->ae / 4)
		force_start |= 0x0800;
	if (diff_lum >= (start_data->lum / 4))
		force_start |= 0x1000;
	if (diff_lum >= (end_data->lum / 4))
		force_start |= 0x2000;
	if (diff_rbratio >= (start_data->rbratio / 8))
		force_start |= 0x4000;
	if (diff_rbratio >= (end_data->rbratio / 8))
		force_start |= 0x8000;

	if (force_start) {
		print_info("CAF need restart reason: 0x%.4x", force_start);
		return true;
	} else {
		print_debug("CAF check_caf_need_restart: 0x%.4x", force_start);
		return false;
	}
}

/*
 **************************************************************************
 * FunctionName: ispv1_focus_get_curr_data
 * Description : using for save current envionment variables
 * Input       : NA;
 * Output      : NA;
 * ReturnValue : NA;
 * Other       : NA;
 **************************************************************************
 */
void ispv1_focus_get_curr_data(focus_frame_stat_s *curr_data)
{
	awb_gain_t awb_gain;
	u32 gain, expo;

	GETREG16(REG_ISP_FOCUS_CONTRAST(0), curr_data->contrast);
	gain = get_writeback_gain();
	expo = get_writeback_expo();

	curr_data->ae = gain * expo;
	curr_data->lum = get_current_y();
	curr_data->fps = this_ispdata->sensor->fps;

	ispv1_get_wb_value(&awb_gain);
	curr_data->rbratio = 0x100 * awb_gain.b_gain / awb_gain.r_gain;
	memcpy(&curr_data->xyz, &mXYZ, sizeof(axis_triple));

	print_debug("current xyz: 0x%4x,  0x%4x, 0x%4x",
		curr_data->xyz.x, curr_data->xyz.y, curr_data->xyz.z);
}

void ispv1_get_default_focusrect(camera_rect_s *rect)
{
	u32 preview_width = this_ispdata->pic_attr[STATE_PREVIEW].out_width;
	u32 preview_height = this_ispdata->pic_attr[STATE_PREVIEW].out_height;

	ispv1_focus_get_default_yuvrect(rect, preview_width, preview_height);
}

u32 ispv1_get_focus_rect(camera_rect_s *rect)
{
	memcpy(rect, &afae_ctrl->af_area.rect[0], sizeof(camera_rect_s));
	if (rect->width == 0 || rect->height == 0)
		ispv1_get_default_focusrect(rect);

	//ispv1_focus_adjust_yuvrect(rect);
	return 0;
}

/*
 **************************************************************************
 * FunctionName: ispv1_focus_caf_process_monitor
 * Description : save caf start and end envionment variables, then compare them.
 * Input       : NA;
 * Output      : NA;
 * ReturnValue : ret: -1 do not restart caf, 0 restart caf.
 * Other       : NA;
 **************************************************************************
 */
#define CAF_START_COMPARE_COUNT			6
#define CAF_END_COMPARE_COUNT_VIDEO		8
#define CAF_END_COMPARE_COUNT_PICTURE	1
bool ispv1_focus_caf_process_monitor(focus_result_s *result, camera_focus focus_mode)
{
	static int start_cnt = 0;
	static int end_cnt = 0;
	int compare_count = -1;
	static focus_frame_stat_s start_data;
	focus_frame_stat_s end_data;
	bool restart = false;
	int curr_contrast;
	u16 threshold_ext;
	u8 afcctrl0;

	if (focus_mode == CAMERA_FOCUS_CONTINUOUS_VIDEO) {
		/* in video mode, should be delay 8 frame after caf to get current envionment params */
		compare_count = CAF_END_COMPARE_COUNT_VIDEO;
	} else {
		compare_count = CAF_END_COMPARE_COUNT_PICTURE;
	}

	if (result->status == STATUS_FOCUSING) {
		GETREG16(REG_ISP_FOCUS_CONTRAST(0), curr_contrast);
		GETREG16(REG_ISP_FOCUS_AFCCTRL2, threshold_ext);
		afcctrl0 = GETREG8(REG_ISP_FOCUS_AFCCTRL0);
		print_debug("focusing contrast: 0x%.3x, threshold_ext 0x%.2x, afcctrl0 %d...@@@@@@@@@@",
			curr_contrast, threshold_ext, afcctrl0);
		start_cnt ++;
	#ifdef AF_TIME_PRINT
		if (start_cnt == 1)
			do_gettimeofday(&tv_start);
	#endif
		if (start_cnt == CAF_START_COMPARE_COUNT)
			ispv1_focus_get_curr_data(&start_data);
	} else if ((result->status == STATUS_FOCUSED) || (result->status == STATUS_OUT_FOCUS)) {
		if (start_cnt >= CAF_START_COMPARE_COUNT) {
			if (++end_cnt == compare_count) {
				ispv1_focus_get_curr_data(&end_data);
			}
		#ifdef AF_TIME_PRINT
			if (end_cnt == 1) {
				do_gettimeofday(&tv_end);
				print_info("*****focus TIME: %.4dms******",
					(int)((tv_end.tv_sec - tv_start.tv_sec)*1000 + (tv_end.tv_usec - tv_start.tv_usec) / 1000));
			}
		#endif
		}
	}

	/* if CAF process complete, should check if restart needed. */
	if ((start_cnt >= CAF_START_COMPARE_COUNT) && (end_cnt == compare_count)) {
		restart = ispv1_check_caf_need_restart(&start_data, &end_data);
		start_cnt = 0;
		end_cnt = 0;
	}
	return restart;
}

static int ispv1_focus_calc_variance(camera_rect_s *rect)
{
	u32 size = 0;
	u8 *pdata = afae_ctrl->stat_data;
	int index;
	u32 average_y = 0;
	u32 variance_y = 0;

	print_debug("enter %s", __func__);

	/* calculate average */
	size = rect->width * rect->height;
	if (pdata == NULL) {
		print_error("pdata NULL in %s function at line %d\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ispv1_copy_preview_data(pdata, rect);

	for (index = 0; index < size; index++)
		average_y += pdata[index];
	average_y /= size;

	for (index = 0; index < size; index++) {
		variance_y += (pdata[index] - average_y) * (pdata[index] - average_y);
	}

	/* variance of mean */
	variance_y /= size;
	return variance_y;
}

#define FOCUS_VAR_LOW_LIMIT		0x100
#define FOCUS_VAR_HIGH_LIMIT		0x400

#define FOCUS_VAR_LOW_COEFF		0x18
#define FOCUS_VAR_HIGH_COEFF		0x10

void ispv1_focus_update_threshold_ext(void)
{
	u32 curr_y;
	u32 curr_gain;
	u32 threshold_ext;
	u32 variance_y = 0;
	u32 coeff = 0;
	camera_rect_s 	*rect;

	rect = &afae_ctrl->cur_rect;
	variance_y = ispv1_focus_calc_variance(rect);
	if (variance_y <= FOCUS_VAR_LOW_LIMIT) {
		coeff = FOCUS_VAR_LOW_COEFF;
	} else if ((variance_y > FOCUS_VAR_LOW_LIMIT) && (variance_y <= FOCUS_VAR_HIGH_LIMIT)) {
		coeff =  (FOCUS_VAR_HIGH_LIMIT - variance_y);
		coeff *=  (FOCUS_VAR_LOW_COEFF - FOCUS_VAR_HIGH_COEFF);
		coeff /= (FOCUS_VAR_HIGH_LIMIT - FOCUS_VAR_LOW_LIMIT);
		coeff += FOCUS_VAR_HIGH_COEFF;
	} else {
		coeff = FOCUS_VAR_HIGH_COEFF;
	}

	curr_y = get_current_y();
	curr_gain = get_writeback_gain();
	//curr_gain = (curr_gain > 0x70) ? 0x70 : (curr_gain & 0xf8);
	curr_gain = (curr_gain > 0x40) ? 0x40 : (curr_gain & 0xf8); //revised by y00215412

	threshold_ext = (0xd0 - curr_gain) / 0xc;
	if (curr_y < 0x10) {
		threshold_ext = 0;
	} else if ((curr_y >= 0x10) && (curr_y < 0x20)) {
		threshold_ext = (curr_y - 0x10) * threshold_ext / 0x10;
	}

	threshold_ext = threshold_ext * coeff / FOCUS_VAR_HIGH_COEFF;

	SETREG16(REG_ISP_FOCUS_AFCCTRL2, threshold_ext);
	print_info("y:0x%.2x, y_var 0x%x, gain: 0x%.2x, threshold_ext: 0x%.2x, coeff 0x%x",
		curr_y, variance_y, curr_gain, threshold_ext, coeff);
}

int ispv1_focus_need_schedule(void)
{
	int schedule_case = -1;

	if (get_focus_mode() == CAMERA_FOCUS_CONTINUOUS_PICTURE &&
	    get_focus_state() == FOCUS_STATE_CAF_RUNNING) {
		schedule_case = FOCUS_SCHEDULE_CASE_CAF_PICTURE_MONITOR;
	} else if (get_focus_mode() == CAMERA_FOCUS_CONTINUOUS_VIDEO &&
	    get_focus_state() == FOCUS_STATE_CAF_RUNNING) {
		schedule_case = FOCUS_SCHEDULE_CASE_CAF_VIDEO_MOVE;
	} else if (get_focus_mode() == CAMERA_FOCUS_CONTINUOUS_VIDEO &&
	    get_focus_state() == FOCUS_STATE_CAF_DETECTING) {
		schedule_case = FOCUS_SCHEDULE_CASE_CAF_VIDEO_DETECT;
	} else if (get_focus_state() == FOCUS_STATE_AF_HOLDING ||
	    get_focus_state() == FOCUS_STATE_CAF_HOLDING) {
		schedule_case = FOCUS_SCHEDULE_CASE_AF_HOLDING;
	}

	return schedule_case;
}

void ispv1_wakeup_focus_schedule(bool force_flag)
{
	u8 framerate = this_ispdata->sensor->fps;
	vcm_info_s *vcm = this_ispdata->sensor->vcm;
	u16 curr_contrast;
	static int frame_count;

	if (force_flag == true) {
		up(&sem_af_schedule);
		return;
	}

	if ((frame_count++ % 50) == 0) {
		GETREG16(REG_ISP_FOCUS_CONTRAST(0), curr_contrast);
		print_debug("framerate %d, expo 0x%x, gain 0x%x, current y 0x%x, focus contrast 0x%x, result %d",
			framerate, get_writeback_expo(), get_writeback_gain(), get_current_y(),
			curr_contrast, ispv1_getreg_is_focused());
	}

	/* check frame rate and set it to focus register. */
	vcm->frameRate = framerate;
	ispv1_setreg_focus_framerate(framerate);

	/* add up AF frame count */
	if (get_focus_state() == FOCUS_STATE_AF_RUNNING) {
		afae_ctrl->focus_frame_count++;
	}

	if ((ispv1_focus_need_schedule() != -1) && (afae_ctrl->k3focus_running == true))
		up(&sem_af_schedule);
}

static int ispv1_wait_focus_schedule_timeout(int time_out)
{
	long jiffies = 0;

	jiffies = msecs_to_jiffies(time_out);
	if (down_timeout(&sem_af_schedule, jiffies)) {
		print_error("wait focus schedule timeout\n");
		return -ETIME;
	} else {
		print_debug("focus schedule waking up###########");
		return 0;
	}
}

#define CAF_RUN_STAGE_PREPARE		0
#define CAF_RUN_STAGE_STARTUP		1
#define CAF_RUN_STAGE_TRY			2
#define CAF_RUN_STAGE_TRYREWIND	3
#define CAF_RUN_STAGE_FORWARD		4
#define CAF_RUN_STAGE_REWIND		5

#define CAF_RUN_DIRECTION_BACKWARD		-1
#define CAF_RUN_DIRECTION_FORWARD		1

/* (this value / 0x20) *100% * compare_contrast */
#define CAF_RUN_JUDGE_THESHOLD			1
#define CAF_RUN_RETRY_COUNT_MAX		100

static int get_next_vcmcode(vcm_info_s *vcm, int curr, int direction, int steps)
{
	int stride;
	int loop;
	int curr_code = curr;
	int next_code = 0;
	int strideSeparator;

	for (loop = 0; loop < steps; loop++) {
		if (curr_code <= vcm->offsetInit && direction == CAF_RUN_DIRECTION_BACKWARD)
			return 0;

		if (curr_code >= vcm->fullRange && direction == CAF_RUN_DIRECTION_FORWARD)
			return 0;

		strideSeparator = vcm->offsetInit + vcm->videoStrideOffset;
		strideSeparator += ((vcm->fullRange - strideSeparator) % (vcm->coarseStep * 2));

		if ((curr_code == strideSeparator && direction == CAF_RUN_DIRECTION_FORWARD) ||
			(curr_code > strideSeparator))
			stride = vcm->coarseStep * 2;
		else
			stride = vcm->coarseStep;

		next_code = curr_code + direction * stride;

		if (next_code > vcm->fullRange || curr_code < vcm->offsetInit)
			return 0;
	}

	return next_code;
}

static inline bool check_vcmcode_is_edge(vcm_info_s *vcm, int code)
{
	if (code <= vcm->offsetInit ||code >= vcm->fullRange)
		return true;
	else
		return false;
}

void ispv1_k3focus_run(void)
{
	vcm_info_s *vcm = this_ispdata->sensor->vcm;
	int schedule_case = -1;
	focus_result_s result;
	int ret;
	bool start_flag = false;
	focus_frame_stat_s curr_data;
	focus_frame_stat_s mean_data;

	pos_info curr, top, histtop;
	int next_code;
	int tryrewind_code;

	int caf_run_stage = CAF_RUN_STAGE_PREPARE;
	int direction = CAF_RUN_DIRECTION_FORWARD;

	int caf_frame_count = 0;
	int caf_retry_count = 0;
	int aec_stable;

	/* init params */
	sema_init(&sem_af_schedule, 0);

	/*set k3focus_running flag is true */
	afae_ctrl->k3focus_running = true;

	curr.code = vcm->offsetInit;
	curr.contrast = 0;
	top.code = curr.code;
	top.contrast = 0;
	histtop.code = curr.code;
	histtop.contrast = 0;
	tryrewind_code = vcm->offsetInit;

	schedule_case = ispv1_focus_need_schedule();
	print_info("enter %s, case %d ###########", __func__, schedule_case);

	while ((schedule_case = ispv1_focus_need_schedule()) != -1) {
		ispv1_wait_focus_schedule_timeout(100);
		/* recheck again. */
		schedule_case = ispv1_focus_need_schedule();
		print_debug("af schedule waked up case %d ###########", schedule_case);

		switch (schedule_case) {
		case FOCUS_SCHEDULE_CASE_AF_HOLDING:
			ispv1_focus_get_curr_data(&curr_data);
			ret = ispv1_focus_status_collect(&curr_data, &mean_data);
			if (ret != 0)
				break;

			/* if diff too much, should start a new focus process. */
			start_flag = ispv1_check_caf_need_trigger(&afae_ctrl->compare_data, &mean_data);
			if (start_flag == true)
				ispv1_setreg_focus_forcestart(1);
			else
				break;

			if (get_focus_state() == FOCUS_STATE_AF_HOLDING) {
				/*
				 * y36721 2012-03-28 fix bug:
				 * when auto focus not selected in hwcamera app menu,
				 * metering area do not go to center
				 * after touch focus and scene changed.
				 */
				ispv1_set_metering_area_done(&afae_ctrl->ae_area);
				set_focus_state(FOCUS_STATE_STOPPED);
			} else if (get_focus_state() == FOCUS_STATE_CAF_HOLDING) {
				/*
				 * if continuous auto focus has be called,
				 * then should start it
				 */
				ispv1_auto_focus(FOCUS_START);
			}
			goto run_out;
		break;

		case FOCUS_SCHEDULE_CASE_CAF_PICTURE_MONITOR:
			ret = ispv1_get_focus_result(&result);
			start_flag = ispv1_focus_caf_process_monitor(&result, CAMERA_FOCUS_CONTINUOUS_PICTURE);
			/* if CAF process start and end diff too much, should force focus restart. */
			if (start_flag == true) {
				ispv1_setreg_focus_forcestart(1);
				ispv1_focus_status_reset();
			}
		break;

		case FOCUS_SCHEDULE_CASE_CAF_VIDEO_DETECT:
			save_videocaf_code(curr.code);
			ret = ispv1_get_focus_result(&result);
			if ((result.status == STATUS_FOCUSED) || (result.status == STATUS_OUT_FOCUS)) {
				ispv1_focus_get_curr_data(&curr_data);
				ret = ispv1_focus_status_collect(&curr_data, &mean_data);
				if (ret != 0)
					break;

				/* if diff too much and quiet, should force focus start. */
				start_flag = ispv1_check_caf_need_trigger(&afae_ctrl->compare_data, &mean_data);
				if (start_flag == true) {
					set_focus_state(FOCUS_STATE_CAF_RUNNING);
					ispv1_focus_status_reset();
				}
			}
			break;

		case FOCUS_SCHEDULE_CASE_CAF_VIDEO_MOVE:
			save_videocaf_code(curr.code);

			if (caf_run_stage != CAF_RUN_STAGE_REWIND && caf_run_stage != CAF_RUN_STAGE_TRYREWIND) {
				/* odd frame should skip */
				if (caf_frame_count++ % 2 != 0)
					break;
			}

			GETREG16(REG_ISP_FOCUS_CONTRAST(0), curr.contrast);
			print_info("enter CAF video move.... stage %d, code 0x%.3x, contrast 0x%.3x###########",
				caf_run_stage, curr.code, curr.contrast);

			switch (caf_run_stage) {
			case CAF_RUN_STAGE_PREPARE:
				caf_retry_count = 0;
				aec_stable = GETREG8(REG_ISP_AECAGC_STABLE);
				if (aec_stable == 0) {
					break;
				}
				ispv1_focus_update_threshold_ext();
				caf_run_stage = CAF_RUN_STAGE_STARTUP;

				/* set this frame is first frame, next frame need skip for stable */
				caf_frame_count = 1;
				break;

			case CAF_RUN_STAGE_STARTUP:
				if (curr.code >= vcm->fullRange)
					direction = CAF_RUN_DIRECTION_BACKWARD;
				else
					direction = CAF_RUN_DIRECTION_FORWARD;
				top.code = curr.code;
				top.contrast = curr.contrast;
				histtop.code = curr.code;
				histtop.contrast = curr.contrast;

				set_videocaf_status(STATUS_FOCUSING);
				curr.code = get_next_vcmcode(vcm, curr.code, direction, 1);
				ispv1_setreg_vcm_code(curr.code); /* need not judge this code */
				caf_run_stage = CAF_RUN_STAGE_TRY;
				break;

			case CAF_RUN_STAGE_TRY:
				if ((curr.contrast <= top.contrast  * (0x10 - CAF_RUN_JUDGE_THESHOLD) / 0x10) ||
					(curr.contrast <= top.contrast  && check_vcmcode_is_edge(vcm, curr.code) == true) ||
					(caf_retry_count >= CAF_RUN_RETRY_COUNT_MAX)) {
					/* turn back right direction, one step to top code. */
					direction = 0 - direction;

					if ((top.contrast < histtop.contrast) ||
						(top.contrast == histtop.contrast && top.code > histtop.code)) {
						top.contrast = histtop.contrast;
						top.code = histtop.code;
					}

					tryrewind_code = get_next_vcmcode(vcm, top.code, direction, 1);
					if (tryrewind_code != 0) {
						next_code = get_next_vcmcode(vcm, curr.code, direction, 1);
						curr.code = next_code;
						caf_run_stage = CAF_RUN_STAGE_TRYREWIND;
					} else {
						print_error("line %d stage %d:next vcmcode will out of range!!!!!!!", __LINE__, caf_run_stage);
						/* Top code is just top, set vcm to top and goto REWIND stage */
						curr.code = top.code;
						caf_run_stage = CAF_RUN_STAGE_REWIND;
					}
				} else if ((curr.contrast <= top.contrast  * (0x10 + CAF_RUN_JUDGE_THESHOLD) / 0x10) &&
					check_vcmcode_is_edge(vcm, curr.code) == false) {
					/* retry because diff is too small */
					caf_run_stage = CAF_RUN_STAGE_TRY;

					if (curr.contrast >= top.contrast && curr.contrast >= histtop.contrast) {
						histtop.contrast = curr.contrast;
						histtop.code = curr.code;
					}

					/* need not judge this code */
					curr.code = get_next_vcmcode(vcm, curr.code, direction, 1);
					caf_retry_count ++;
				} else {
					/* update top info, set as stage FORWARD and goto next position. */
					top.contrast = curr.contrast;
					top.code = curr.code;

					next_code = get_next_vcmcode(vcm, curr.code, direction, 1);
					if (next_code != 0) {
						caf_run_stage = CAF_RUN_STAGE_FORWARD;
						curr.code = next_code;
					} else {
						print_error("line %d stage %d:next vcmcode will out of range!!!!!!!", __LINE__, caf_run_stage);
						/* Top code is this, goto REWIND stage */
						caf_run_stage = CAF_RUN_STAGE_REWIND;
						break;
					}
				}

				ispv1_setreg_vcm_code(curr.code);
				break;

			case CAF_RUN_STAGE_TRYREWIND:
				if (curr.code != tryrewind_code) {
					curr.code = get_next_vcmcode(vcm, curr.code, direction, 1);
					ispv1_setreg_vcm_code(curr.code);
				} else {
					caf_run_stage = CAF_RUN_STAGE_FORWARD;
				}
				break;

			case CAF_RUN_STAGE_FORWARD:
				if ((curr.contrast <= top.contrast  * (0x10 - CAF_RUN_JUDGE_THESHOLD) / 0x10) ||
					(curr.contrast <= top.contrast  && check_vcmcode_is_edge(vcm, curr.code) == true)) {
					/* Top code found, goto REWIND, in this case, should not go back by 1step. */

					if ((top.contrast < histtop.contrast) ||
						(top.contrast == histtop.contrast && top.code > histtop.code)) {
						top.contrast = histtop.contrast;
						top.code = histtop.code;
					}

					/* prepare next code for rewind. */
					if (abs(top.code - curr.code) >= (vcm->coarseStep * 2)) {
						if (top.code > curr.code)
							direction = CAF_RUN_DIRECTION_FORWARD;
						else
							direction = CAF_RUN_DIRECTION_BACKWARD;

						curr.code = get_next_vcmcode(vcm, curr.code, direction, 1);
					} else
						curr.code = top.code;

					print_info("top code 0x%x found!!!!!!!", top.code);

					/* rewind to next code. */
					ispv1_setreg_vcm_code(curr.code);
					caf_run_stage = CAF_RUN_STAGE_REWIND;
				} else if (check_vcmcode_is_edge(vcm, curr.code) == true) {
					/* reach edge code, stay here */
					caf_run_stage = CAF_RUN_STAGE_REWIND;
					top.code = curr.code;
				} else if (curr.contrast <= top.contrast) {
					/* retry and goto next position, because diff is too small. */
					next_code = get_next_vcmcode(vcm, curr.code, direction, 1);
					if (next_code != 0) {
						curr.code = next_code;
						ispv1_setreg_vcm_code(curr.code);
						if (curr.contrast >= top.contrast && curr.contrast >= histtop.contrast) {
							histtop.contrast = curr.contrast;
							histtop.code = curr.code;
						}
					} else {
						print_error("line %d stage %d:next vcmcode will out of range!!!!!!!", __LINE__, caf_run_stage);
						/* Top code found, goto REWIND, in this case, should not go back by 1step. */

						/* prepare next code for rewind. */
						if (abs(top.code - curr.code) >= (vcm->coarseStep * 2)) {
							if (top.code > curr.code)
								direction = CAF_RUN_DIRECTION_FORWARD;
							else
								direction = CAF_RUN_DIRECTION_BACKWARD;

							curr.code = get_next_vcmcode(vcm, curr.code, direction, 1);
						} else
							curr.code = top.code;

						/* rewind to next code. */
						ispv1_setreg_vcm_code(curr.code);
						caf_run_stage = CAF_RUN_STAGE_REWIND;
					}
				} else {
					/* update top info and goto next position. */
					top.contrast = curr.contrast;
					top.code = curr.code;

					next_code = get_next_vcmcode(vcm, curr.code, direction, 1);
					if (next_code != 0) {
						curr.code = next_code;
						ispv1_setreg_vcm_code(curr.code);
					} else {
						print_error("line %d stage %d:next vcmcode will out of range!!!!!!!", __LINE__, caf_run_stage);
						/* Top code is this, goto REWIND stage */
						caf_run_stage = CAF_RUN_STAGE_REWIND;
					}
				}
				break;

			case CAF_RUN_STAGE_REWIND:
				if (curr.code != top.code) {
					curr.code = get_next_vcmcode(vcm, curr.code, direction, 1);
					ispv1_setreg_vcm_code(curr.code);
				} else {
					top.contrast = curr.contrast;
					caf_run_stage = CAF_RUN_STAGE_PREPARE;
					set_focus_state(FOCUS_STATE_CAF_DETECTING);
					set_videocaf_status(STATUS_FOCUSED);
				}
				break;

			default:
				print_info("error:unknow caf_run_state %d ###########", caf_run_stage);
				break;
			}
			break;

		default:
			print_info("error:unknow af schedule waked up case %d ###########", schedule_case);
			goto run_out;
		}
	}
run_out:
	afae_ctrl->k3focus_running = false;
	print_info("exit %s, ###########", __func__);
	return;
}
/********************************* END ***********************************************/