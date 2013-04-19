/*
 *  Hisilicon K3 soc camera ISP afae driver header file
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

#ifndef __K3_ISPV1_AFAE_H__
#define __K3_ISPV1_AFAE_H__

/* Added for focus module */
#define REG_ISP_SCALE6_SELECT		(0x6502a)

#define REG_ISP_FOCUS_MIN_STAT_HF_NUM		(0x66111)
#define REG_ISP_FOCUS_STAT_THRESHOLD		(0x66112)

#define ISP_FOCUS_BINNING			(1<<2)
#define ISP_FOCUS_NONBINNING		(0<<2)
#define ISP_FOCUS_5X5_WINDOW		(1<<1)
#define ISP_FOCUS_SINGLE_WINDOW	(0<<1)
#define REG_ISP_FOCUS_AFCCTRL0		(0x66100)
#define REG_ISP_FOCUS_AFCCTRL2		(0x66102)
#define REG_ISP_FOCUS_AFCCTRL3		(0x66103)

/*windows definition registers*/

#define ISP_FOCUS_WINMODE_SINGLE	0
#define ISP_FOCUS_WINMODE_5X5		1
#define REG_ISP_FOCUS_WINMODE		(0x1c5b0)

#define ISP_FOCUS_IS_AUTO			1
#define ISP_FOCUS_ISNOT_AUTO		0
#define REG_ISP_FOCUS_IFAUTO		(0x1c5b1)

#define ISP_FOCUS_IS_WEIGHTED           1
#define ISP_FOCUS_ISNOT_WEIGHTED        0
#define REG_ISP_FOCUS_IFWEIGHTED        (0x1c5b2)
#define REG_ISP_FOCUS_ACTIVE_WINID      (0x1c5b3)

/*windows zone definition, include x0,y0,width,height,w1,h1*/
#define REG_ISP_FOCUS_WIN_X0			(0x66104)
#define REG_ISP_FOCUS_WIN_Y0			(0x66106)

#define REG_ISP_FOCUS_WIN_W0			(0x66108)
#define REG_ISP_FOCUS_WIN_H0			(0x6610a)

#define REG_ISP_FOCUS_WIN_W1			(0x6610c)
#define REG_ISP_FOCUS_WIN_H1			(0x6610e)

/*winid is 0-24*/
#define REG_ISP_FOCUS_WEIGHT_LIST(winid)		(0x1c5b4 + (winid) * 2)

/*AF control registers*/
#define REG_ISP_FOCUS_CONTRAST(n)		(0x1ca30 + (n) * 2)

#define REG_ISP_FOCUS_ACTIVE			(0x1cd0a)

#define ISP_FOCUS_VIDEO_MODE		1
#define ISP_FOCUS_SNAPSHOT_MODE	0
#define REG_ISP_FOCUS_MODE			(0x1cd0b)

#define REG_ISP_FOCUS_LOCK_AE			(0x1cd10)

/*motor registers*/
#define REG_ISP_FOCUS_MOTOR_OFFSETINIT			(0x1cca4)
#define REG_ISP_FOCUS_MOTOR_FULLRANGE			(0x1cca6)
#define REG_ISP_FOCUS_MOTOR_MINISTEP			(0x1cca8)
#define REG_ISP_FOCUS_MOTOR_COARSESTEP			(0x1ccaa)
#define REG_ISP_FOCUS_MOTOR_FINESTEP			(0x1ccac)


#define REG_ISP_FOCUS_1STEP_COMEBACK			(0x1ccb1)

#define REG_ISP_FOCUS_MOTOR_FRAMERATE			(0x1ccae)
#define REG_ISP_FOCUS_MOTOR_WAITFRAME			(0x1ccb0)
#define REG_ISP_FOCUS_MOTOR_RESTIME			(0x1ccb2)

#define REG_ISP_FOCUS_MOTOR_CURR				(0x1ccec)
#define REG_ISP_FOCUS_MOTOR_STARTPARTITION			(0x1cdd7)
#define REG_ISP_FOCUS_MOTOR_DRIVEMODE				(0x1cddc)
#define REG_ISP_FOCUS_MOTOR_CALIBRATE				(0x1cddd)

#define REG_ISP_FOCUS_PREMOVE				(0x1cd0e)

#define REG_ISP_FOCUS_IGNORE_AEC_STABLE	(0x1cd04)

/* Reg focus failure position choise: 0-max position(default); 1-infinity. */
#define REG_ISP_FOCUS_FAILURE_POS			(0x1cd05)
#define REG_ISP_FOCUS_FORCE_START			(0x1cd06)

/*VCM registers*/
#define REG_ISP_FOCUS_I2COPTION		(0x1cdc6)
#define REG_ISP_FOCUS_DEVICEID		(0x1cdc7)
#define REG_ISP_FOCUS_MOVELENS_ADDR0		(0x1cdc8)
#define REG_ISP_FOCUS_MOVELENS_ADDR1	(0x1cdca)

#define REG_ISP_FOCUS_BUSY_ADDR		(0x1cdcc)

#define REG_ISP_FOCUS_WIN_NUM		(0x1cdce)
/*num 0-3*/
#define REG_ISP_FOCUS_WIN_LIST(num)		(0x1cdcf+(num))

#define REG_ISP_FOCUS_STATUS		(0x1cdd4)
#define REG_ISP_FOCUS_REDRAW		(0x1cdd5)

/*here these register also can get focus result*/
#define REG_ISP_FOCUS_FRAME_WIDTH		(0x1c774)
#define REG_ISP_FOCUS_FRAME_HEIGHT		(0x1c776)
/*num 0-3*/
#define REG_ISP_FOCUS_FRAME_POSX(num)		(0x1c778+4*(num))
#define REG_ISP_FOCUS_FRAME_POSY(num)		(0x1c77a+4*(num))

/*performance related*/
#define REG_ISP_FOCUS_nT_compare_1x		(0x1cc92)
#define REG_ISP_FOCUS_nT_compare_16x	(0x1cc94)

#define REG_ISP_FOCUS_nT_sad_1x			(0x1cc96)
#define REG_ISP_FOCUS_nT_sad_16x			(0x1cc98)

#define REG_ISP_FOCUS_nT_presad_1x		(0x1cc9a)
#define REG_ISP_FOCUS_nT_presad_16x		(0x1cc9c)

#define REG_ISP_FOCUS_nT_hist_1x			(0x1cc9e)
#define REG_ISP_FOCUS_nT_hist_16x			(0x1cca0)

#define REG_ISP_FOCUS_nT_contrast_diff		(0x1cca2)

/* target Y definitions */
#define DEFAULT_TARGET_Y_LOW			0x2c
#define DEFAULT_TARGET_Y_HIGH			0x4c
#define DEFAULT_TARGET_Y_FLASH			0x30

#define METERING_CWA_WIDTH_PERCENT		75
#define METERING_CWA_HEIGHT_PERCENT	75
#define METERING_SPOT_WIDTH_PERCENT	75	/* y36721 temp */
#define METERING_SPOT_HEIGHT_PERCENT	75

/* AE registers */
#define REG_ISP_CURRENT_Y				(0x1c75e)

#define REG_ISP_TARGET_Y				(0x1c5aa)
#define REG_ISP_TARGET_Y_LOW			(0x1c146)
#define REG_ISP_TARGET_Y_LOW_SHORT		(0x1c147)

#define REG_ISP_TARGET_Y_HIGH			(0x1c5a0)

#define REG_ISP_SLOW_RANGE			(0x1c148)
#define REG_ISP_SLOW_RANGE_SHORT	(0x1c149)
#define REG_ISP_STABLE_RANGE		(0x1c14a)
#define REG_ISP_STABLE_RANGE_SHORT	(0x1c14b)
#define REG_ISP_FAST_STEP			(0x1c14c)
#define REG_ISP_FAST_STEP_SHORT		(0x1c14d)
#define REG_ISP_SLOW_STEP			(0x1c14e)
#define REG_ISP_SLOW_STEP_SHORT		(0x1c14f)

#define REG_ISP_AECAGC_STATWIN_LEFT			(0x66401)
#define REG_ISP_AECAGC_STATWIN_TOP			(0x66403)
#define REG_ISP_AECAGC_STATWIN_RIGHT		(0x66405)
#define REG_ISP_AECAGC_STATWIN_BOTTOM		(0x66407)

#define DEFAULT_AECAGC_STATWIN_XOFFSET		0x20
#define DEFAULT_AECAGC_STATWIN_YOFFSET		0x20

#define REG_ISP_AECAGC_CENTER_LEFT			(0x66409)
#define REG_ISP_AECAGC_CENTER_LEFT_SHORT	(0x6640b)
#define REG_ISP_AECAGC_CENTER_TOP			(0x6640d)
#define REG_ISP_AECAGC_CENTER_TOP_SHORT	(0x6640f)
#define REG_ISP_AECAGC_CENTER_WIDTH		(0x66411)
#define REG_ISP_AECAGC_CENTER_WIDTH_SHORT	(0x66413)
#define REG_ISP_AECAGC_CENTER_HEIGHT		(0x66415)
#define REG_ISP_AECAGC_CENTER_HEIGHT_SHORT	(0x66417)

/* win:0-12 */
#define REG_ISP_AECAGC_WIN_WEIGHT(num)			(0x6642eL+(num))
#define REG_ISP_AECAGC_WIN_WEIGHT_SHORT(num)	(0x6643bL+(num))

#define REG_ISP_AECAGC_WIN_WEIGHT_SHIFT		(0x6644e)

/* ROI definition */
#define REG_ISP_AECAGC_ROI_LEFT			(0x66419)
#define REG_ISP_AECAGC_ROI_LEFT_SHORT	(0x6641b)
#define REG_ISP_AECAGC_ROI_TOP			(0x6641d)
#define REG_ISP_AECAGC_ROI_TOP_SHORT	(0x6641f)
#define REG_ISP_AECAGC_ROI_RIGHT		(0x66421)
#define REG_ISP_AECAGC_ROI_RIGHT_SHORT	(0x66423)
#define REG_ISP_AECAGC_ROI_BOTTOM		(0x66425)
#define REG_ISP_AECAGC_ROI_BOTTOM_SHORT	(0x66427)

#define REG_ISP_AECAGC_ROI_SHIFT	(0x66429)

#define REG_ISP_AECAGC_ROI_WEIGHT_IN			(0x6642a)
#define REG_ISP_AECAGC_ROI_WEIGHT_OUT			(0x6642b)
#define REG_ISP_AECAGC_ROI_WEIGHT_IN_SHORT		(0x6642c)
#define REG_ISP_AECAGC_ROI_WEIGHT_OUT_SHORT	(0x6642d)

#define REG_ISP_AECAGC_MANUAL_ENABLE			(0x1c174)

#define ISP_WRITESENSOR_ENABLE					(1)
#define ISP_WRITESENSOR_DISABLE					(0)
#define REG_ISP_AECAGC_WRITESENSOR_ENABLE		(0x1c139)

/* 0x1cddf for aec stable status. 1 for stable, 0 for unstable. */
#define REG_ISP_AECAGC_STABLE	(0x1cddf)

#define CAF_STAT_COMPARE_START_FRAME	2
#define CAF_STAT_COMPARE_END_FRAME	5
#define CAF_STAT_COMPARE_FRAMES \
	(CAF_STAT_COMPARE_END_FRAME - CAF_STAT_COMPARE_START_FRAME + 1)

#define CAF_STAT_SKIP_FRAME		15
#define CAF_STAT_FRAME			10

#define FLASH_TEST_MAX_COUNT	20
#define FLASH_TEST_OVER_EXPO	0x80

/* struct definition */
typedef enum {
	FOCUS_STATE_STOPPED = 0,
	FOCUS_STATE_CAF_HOLDING,
	FOCUS_STATE_CAF_PREPARING,
	FOCUS_STATE_CAF_DETECTING,
	FOCUS_STATE_CAF_RUNNING,
	FOCUS_STATE_AF_PREPARING,
	FOCUS_STATE_AF_RUNNING,
	FOCUS_STATE_AF_HOLDING,
	FOCUS_STATE_MAX,
} focus_state_e;
typedef enum {
	CAMERA_AUTO_SNAPSHOT_MODE = 0,
	CAMERA_SINGLE_SNAPSHOT_MODE,
	CAMERA_WEIGHTED_SNAPSHOT_MODE,
	CAMERA_VIDEO_SERVO_MODE,
} camera_af_mode;

typedef enum {
	FOCUS_SCHEDULE_CASE_AF_MOVE = 0,
	FOCUS_SCHEDULE_CASE_CAF_PICTURE_DETECT,
	FOCUS_SCHEDULE_CASE_CAF_PICTURE_MONITOR,
	FOCUS_SCHEDULE_CASE_CAF_PICTURE_MOVE,

	FOCUS_SCHEDULE_CASE_CAF_VIDEO_DETECT,
	FOCUS_SCHEDULE_CASE_CAF_VIDEO_MONITOR,
	FOCUS_SCHEDULE_CASE_CAF_VIDEO_MOVE,

	FOCUS_SCHEDULE_CASE_AF_HOLDING,
} focus_schedule_case;

/* vcm focus position information */
typedef struct _pos_info {
	u32	contrast;
	u32	code;
} pos_info;

/* frame's statistic data */
typedef struct _focus_frame_stat_s {
	u32 contrast;
	u32 ae;
	u32 lum;
	u32 rbratio;
	axis_triple xyz;

	u32 contrast_var;
	axis_triple xyz_var;
	u32 fps;
} focus_frame_stat_s;

typedef struct _ispv1_afae_ctrl_s {
	metering_area_s ae_area;

	focus_area_s af_area;
	camera_focus mode;

	camera_rect_s cur_rect;

	int binning;
	int multi_win;

	/* save current focus state. */
	focus_state_e focus_state;

	/* previous focus action is failed or not. */
	int focus_failed;

	/* save statistic frames count */
	int focus_stat_frames;

	/* force CAF start flag */
	u16 force_start;

	FOCUS_STATUS video_caf_status;
	int video_caf_code;

	bool k3focus_running;

	/* stat data is copy from preview yuv, used to calculate contrast_ext_threshold */
	u8 *stat_data;

	/*
	 * save previous single snapshot's or CAF success contrast/gain/expo/lum
	 * when focus succeeded and stop focus,
	 * if swith to continuous video af mode,
	 * we can not switch to continuous video af mode really
	 * this value can be compare with current contrast,
	 * used to judge scene change.
	 * If compare_contrast and current contrast differ too much,
	 * then we can switch to continuous video af mode really
	 */
	focus_frame_stat_s compare_data;

	/* using for calculate focus's uper limit and lower limit */
	u16 focus_frame_count;

	/*
	 * save several frame's statistic data,
	 * used to judge scene change.
	 */
	focus_frame_stat_s frame_stat[CAF_STAT_FRAME];

	/*
	 * Define each focus rect's win ID, maybe several win IDs.
	 * If focus rect 0 is include win ID 0, 1, 2,
	 * then table[0][0]/table[0][1]/table[0][2] is 1, other is 0.
	 * After ispv1_exit_focus is called, all table value will be 0.
	 */
	int *map_table;
} ispv1_afae_ctrl;

/* For auto focus, public API, temporarily we just support one point focus.
 * y36721 todo
 */
int ispv1_auto_focus(int flag);
int ispv1_set_focus_mode(camera_focus focus_mode);
int ispv1_set_focus_area(focus_area_s *area);
int ispv1_get_focus_result(focus_result_s *result);

/* For auto focus, following are private functions for ispv1*/

/* called by ispv1_tune_ops_init() */
int ispv1_focus_init(void);
/* called by ispv1_tune_ops_init() */
int ispv1_focus_exit(void);

/* Called by start_preview */
int ispv1_focus_prepare(void);
/* Called by stop_preview */
int ispv1_focus_withdraw(void);

/*
 * set metering mode
 */
int ispv1_set_metering_mode(camera_metering mode);
/*
 * set metering area
 */
int ispv1_set_metering_area(metering_area_s *area);
int ispv1_set_focus_range(camera_focus focus_mode);
int ispv1_get_focus_distance(void);

int ispv1_set_gsensor_stat(axis_triple *xyz);
int ispv1_set_ae_statwin(pic_attr_t *pic_attr);
void ispv1_wakeup_focus_schedule(bool force_flag);
#endif /*__K3_ISPV1_AFAE_H__ */
/********************************* END ****************************************/
