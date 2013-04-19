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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 - 1307  USA
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

#include <mach/boardid.h>
#include "cam_util.h"
#include "cam_dbg.h"
#include "k3_isp.h"
#include "k3_ispv1.h"
#include "k3_ispv1_afae.h"
#include "k3_isp_io.h"

#define DEBUG_DEBUG 0
#define LOG_TAG "K3_ISPV1_TUNE_OPS"
#include "cam_log.h"

k3_isp_data *this_ispdata;
static bool camera_ajustments_flag;
extern ispv1_afae_ctrl *afae_ctrl;
u32 max_level;
static int scene_target_y_low = DEFAULT_TARGET_Y_LOW;
static int scene_target_y_high = DEFAULT_TARGET_Y_HIGH;
bool fps_lock;

static int ispv1_change_frame_rate(camera_auto_frame_rate direction, camera_sensor *sensor);
static int ispv1_set_frame_rate(camera_frame_rate_mode mode, camera_sensor *sensor);

/*
 * For anti-shaking, y36721 todo
 * ouput_width-2*blocksize
 * ouput_height-2*blocksize
*/
int ispv1_set_anti_shaking_block(int blocksize)
{
	camera_rect_s out, stat;
	u8 reg1;

	print_debug("enter %s", __func__);

	/* bit4: 0-before yuv downscale;1-after yuv dcw, but before yuv upscale */
	reg1 = GETREG8(REG_ISP_SCALE6_SELECT);
	reg1 |= (1 << 4);
	SETREG8(REG_ISP_SCALE6_SELECT, reg1);

	/* y36721 todo */
	out.left = 0;
	out.top = 0;

	out.width = blocksize;
	out.height = blocksize;

	k3_isp_antishaking_rect_out2stat(&out, &stat);

	SETREG8(REG_ISP_ANTI_SHAKING_BLOCK_SIZE, (stat.width & 0xff00) >> 8);
	SETREG8(REG_ISP_ANTI_SHAKING_BLOCK_SIZE + 1, stat.width & 0xff);

	/*
	 * y36721 todo
	 * should set REG_ISP_ANTI_SHAKING_ABSSTART_POSITION_H and V
	 * REG_ISP_ANTI_SHAKING_ABSSTART_POSITION_H
	 * REG_ISP_ANTI_SHAKING_ABSSTART_POSITION_V
	 */

	return 0;
}

int ispv1_set_anti_shaking(camera_anti_shaking flag)
{
	print_debug("enter %s", __func__);

	if (flag)
		SETREG8(REG_ISP_ANTI_SHAKING_ENABLE, 1);
	else
		SETREG8(REG_ISP_ANTI_SHAKING_ENABLE, 0);

	return 0;
}

/* read out anti-shaking coordinate */
int ispv1_get_anti_shaking_coordinate(coordinate_s *coordinate)
{
	u8 reg0;
	camera_rect_s out, stat;

	print_debug("enter %s", __func__);

	reg0 = GETREG8(REG_ISP_ANTI_SHAKING_ENABLE);

	if ((reg0 & 0x1) != 1) {
		print_error("anti_shaking not working!");
		return -1;
	}

	GETREG16(REG_ISP_ANTI_SHAKING_WIN_LEFT, stat.left);
	GETREG16(REG_ISP_ANTI_SHAKING_WIN_TOP, stat.top);
	stat.width = 0;
	stat.height = 0;

	k3_isp_antishaking_rect_stat2out(&out, &stat);

	coordinate->x = out.left;
	coordinate->y = out.top;

	return 0;

}

/* Added for ISO, target Y will not change with ISO */
int ispv1_set_iso(camera_iso iso)
{
	int max_iso, min_iso;
	int max_gain, min_gain;
	int retvalue = 0;
	camera_sensor *sensor;
	if (NULL == this_ispdata) {
		print_info("this_ispdata is NULL");
		return -1;
	}
	sensor = this_ispdata->sensor;

	print_debug("enter %s", __func__);

	/* ISO is to change sensor gain, but is not same */
	switch (iso) {
	case CAMERA_ISO_AUTO:
		/*
		 * max iso should be ISO800
		 * min iso should be ISO100
		 */
		max_iso = 775;	/*max is 775 for ov8830*/
		min_iso = 100;
		break;

	case CAMERA_ISO_100:
		max_iso = (100 + (100 / 8) * 2);
		min_iso = 100;
		break;

	case CAMERA_ISO_200:
		/* max and min iso should be fixed ISO200 */
		max_iso = (200 + 200 / 8);
		min_iso = (200 - 200 / 8);
		break;

	case CAMERA_ISO_400:
		/* max and min iso should be fixed ISO400 */
		max_iso = (400 + 400 / 8);
		min_iso = (400 - 400 / 8);
		break;

	case CAMERA_ISO_800:
		/* max and min iso should be fixed ISO800 */
		max_iso = 775;
		min_iso = 650;
		break;

	default:
		retvalue = -1;
		goto out;
		break;
	}

	if (sensor->sensor_iso_to_gain) {
		max_gain = sensor->sensor_iso_to_gain(max_iso);
		min_gain = sensor->sensor_iso_to_gain(min_iso);
	} else {
		print_error("sensor_iso_to_gain not defined!");
		retvalue = -1;
		goto out;
	}

	if ((max_gain <= 0) || (min_gain <= 0)) {
		retvalue = -1;
		goto out;
	} else {
		/* set to ISP registers */
		SETREG8(REG_ISP_MAX_GAIN, (max_gain & 0xff00) >> 8);
		SETREG8(REG_ISP_MAX_GAIN + 1, max_gain & 0xff);
		SETREG8(REG_ISP_MIN_GAIN, (min_gain & 0xff00) >> 8);
		SETREG8(REG_ISP_MIN_GAIN + 1, min_gain & 0xff);

		sensor->min_gain = min_gain;
		sensor->max_gain = max_gain;
	}

out:
	return retvalue;
}

/*
 * only useful for ISO auto mode, pWriteBackGain[0x1c7a4-0x1c7a5] is real gain.
 * but there is a real gain register 0x66c00-0x66c01, maybe they are same
 */
int ispv1_get_actual_iso(void)
{
	int iso, gain;
	int index;
	camera_sensor *sensor = this_ispdata->sensor;

	gain = get_writeback_gain();
	iso = gain * 100 / 0x10;

	if (isp_hw_data.cur_state == STATE_PREVIEW)
		index = sensor->preview_frmsize_index;
	else
		index = sensor->capture_frmsize_index;

	if (sensor->frmsize_list[index].binning == false)
		iso /= 2;

	iso = (iso + 5) / 10 * 10;
	return iso;
}

/* real exposure time is pWriteBackExpo[0x1c79c-0x1c79f] */
int ispv1_get_exposure_time(void)
{
	u32 exposure;
	u32 fps;
	u32 vts;
	int index;
	int denominator_exposure_time;
	camera_sensor *sensor = this_ispdata->sensor;

	if (isp_hw_data.cur_state == STATE_PREVIEW)
		index = sensor->preview_frmsize_index;
	else
		index = sensor->capture_frmsize_index;

	exposure = get_writeback_expo() >> 4;
	fps = sensor->frmsize_list[index].fps;
	vts = sensor->frmsize_list[index].vts;
	denominator_exposure_time = fps * vts / exposure;

	return denominator_exposure_time;
}


u32 ispv1_get_awb_gain(int withShift)
{
	u16 b_gain, r_gain;
	u32 return_val;
	if (withShift) {
		GETREG16(REG_ISP_AWB_GAIN_B, b_gain);
		GETREG16(REG_ISP_AWB_GAIN_R, r_gain);
	} else {
		GETREG16(REG_ISP_AWB_ORI_GAIN_B, b_gain);
		GETREG16(REG_ISP_AWB_ORI_GAIN_R, r_gain);
	}
	return_val = (b_gain << 16) | r_gain;
	return return_val;
}


u32 ispv1_get_expo_line(void)
{
	u32 ret;

	ret = get_writeback_expo();
	return ret;
}

u32 ispv1_get_sensor_vts(void)
{
	camera_sensor *sensor;
	u32 frame_index;
	u32 full_fps, fps;
	u32 basic_vts;
	u32 vts;

	if (NULL == this_ispdata) {
		print_info("this_ispdata is NULL");
		return 0;
	}
	sensor = this_ispdata->sensor;

	if (isp_hw_data.cur_state == STATE_PREVIEW)
		frame_index = sensor->preview_frmsize_index;
	else
		frame_index = sensor->capture_frmsize_index;

	full_fps = sensor->frmsize_list[frame_index].fps;
	basic_vts = sensor->frmsize_list[frame_index].vts;
	fps = sensor->fps;
	vts = basic_vts * full_fps / fps;

	return vts;
}

/* Added for EV: exposure compensation.
 * Just set as this: ev+2 add 40%, ev+1 add 20% to current target Y
 * should config targetY and step
 */
void ispv1_calc_ev(u8 *target_low, u8 *target_high, int ev)
{
	int target_y_low = DEFAULT_TARGET_Y_LOW;
	int target_y_high = DEFAULT_TARGET_Y_HIGH;

	switch (ev) {
	case -2:
		target_y_low = DEFAULT_TARGET_Y_LOW - 0x10;
		target_y_low *= (EV_RATIO_NUMERATOR * EV_RATIO_NUMERATOR);
		target_y_low /= (EV_RATIO_DENOMINATOR * EV_RATIO_DENOMINATOR);
		target_y_high = target_y_low + 2;
		break;

	case -1:
		target_y_low = DEFAULT_TARGET_Y_LOW - 0x06;
		target_y_low *= EV_RATIO_NUMERATOR;
		target_y_low /= EV_RATIO_DENOMINATOR;
		target_y_high = target_y_low + 2;
		break;

	case 0:
		break;

	case 1:
		target_y_low = DEFAULT_TARGET_Y_LOW + 0x18;
		target_y_low *= EV_RATIO_DENOMINATOR;
		target_y_low /= EV_RATIO_NUMERATOR;
		target_y_high = target_y_low + 2;
		break;

	case 2:
		target_y_low = DEFAULT_TARGET_Y_LOW + 0x0e;
		target_y_low *= (EV_RATIO_DENOMINATOR * EV_RATIO_DENOMINATOR);
		target_y_low /= (EV_RATIO_NUMERATOR * EV_RATIO_NUMERATOR);
		target_y_high = target_y_low + 2;
		break;

	default:
		print_error("ev invalid");
		break;
	}

	*target_low = target_y_low;
	*target_high = target_y_high;
}

/* Added for EV: exposure compensation.
 * Just set as this: ev+2 add 40%, ev+1 add 20% to current target Y
 * should config targetY and step
 */
int ispv1_set_ev(int ev)
{
#ifdef OVISP_DEBUG_MODE
	return 0;
#endif
	u8 target_y_low, target_y_high;
	int ret = 0;

	print_debug("enter %s", __func__);

	/* ev is target exposure compensation value, decided by exposure time and sensor gain */
	ispv1_calc_ev(&target_y_low, &target_y_high, ev);

	if ((ev == 0) && (this_ispdata->ev != 0)) {
		SETREG8(REG_ISP_TARGET_Y_LOW, target_y_low);
		SETREG8(REG_ISP_TARGET_Y_HIGH, target_y_low);
		msleep(100);
		SETREG8(REG_ISP_TARGET_Y_HIGH, target_y_high);
	} else {
		SETREG8(REG_ISP_TARGET_Y_LOW, target_y_low);
		SETREG8(REG_ISP_TARGET_Y_HIGH, target_y_high);
	}

	scene_target_y_low = target_y_low;
	scene_target_y_high = target_y_high;

	return ret;
}

static void ispv1_init_sensor_config(camera_sensor *sensor)
{
	/* Enable AECAGC */
	SETREG8(REG_ISP_AECAGC_MANUAL_ENABLE, AUTO_AECAGC);
#if 1				/* new firmware support isp write all sensors's AEC/AGC registers. */
	SETREG8(REG_ISP_AECAGC_WRITESENSOR_ENABLE, ISP_WRITESENSOR_ENABLE);
#else
	/* y36721 2012-03-28 added temporarily */
	if (sensor->sensor_type == SENSOR_OV)
		SETREG8(REG_ISP_AECAGC_WRITESENSOR_ENABLE, ISP_WRITESENSOR_ENABLE);
	else
		SETREG8(REG_ISP_AECAGC_WRITESENSOR_ENABLE, ISP_WRITESENSOR_DISABLE);
#endif

	if (sensor->sensor_type == SENSOR_OV)
		SETREG8(REG_ISP_TOP6, SENSOR_BGGR);
	else if (sensor->sensor_type == SENSOR_SONY)
		SETREG8(REG_ISP_TOP6, SENSOR_RGGB);
	else if (sensor->sensor_type == SENSOR_SAMSUNG)
		SETREG8(REG_ISP_TOP6, SENSOR_GRBG);
	else
		SETREG8(REG_ISP_TOP6, SENSOR_BGGR);	/* default set same as OV */

	SETREG16(REG_ISP_AEC_ADDR0, sensor->aec_addr[0]);
	SETREG16(REG_ISP_AEC_ADDR1, sensor->aec_addr[1]);
	SETREG16(REG_ISP_AEC_ADDR2, sensor->aec_addr[2]);

	SETREG16(REG_ISP_AGC_ADDR0, sensor->agc_addr[0]);
	SETREG16(REG_ISP_AGC_ADDR1, sensor->agc_addr[1]);

	if (0 == sensor->aec_addr[0])
		SETREG8(REG_ISP_AEC_MASK_0, 0x00);
	else
		SETREG8(REG_ISP_AEC_MASK_0, 0xff);

	if (0 == sensor->aec_addr[1])
		SETREG8(REG_ISP_AEC_MASK_1, 0x00);
	else
		SETREG8(REG_ISP_AEC_MASK_1, 0xff);

	if (0 == sensor->aec_addr[2])
		SETREG8(REG_ISP_AEC_MASK_2, 0x00);
	else
		SETREG8(REG_ISP_AEC_MASK_2, 0xff);

	if (0 == sensor->agc_addr[0])
		SETREG8(REG_ISP_AGC_MASK_H, 0x00);
	else
		SETREG8(REG_ISP_AGC_MASK_H, 0xff);

	if (0 == sensor->agc_addr[1])
		SETREG8(REG_ISP_AGC_MASK_L, 0x00);
	else
		SETREG8(REG_ISP_AGC_MASK_L, 0xff);

	SETREG8(REG_ISP_AGC_SENSOR_TYPE, sensor->sensor_type);
}

/* Added for anti_banding. y36721 todo */
int ispv1_set_anti_banding(camera_anti_banding banding)
{
	u32 op = 0;

	print_debug("enter %s", __func__);

	switch (banding) {
	case CAMERA_ANTI_BANDING_OFF:
		op = 0;
		SETREG8(REG_ISP_BANDFILTER_SHORT_EN, 0x1);
		break;

	case CAMERA_ANTI_BANDING_50Hz:
		op = 2;
		SETREG8(REG_ISP_BANDFILTER_SHORT_EN, 0x0);
		break;

	case CAMERA_ANTI_BANDING_60Hz:
		op = 1;
		SETREG8(REG_ISP_BANDFILTER_SHORT_EN, 0x0);
		break;

	case CAMERA_ANTI_BANDING_AUTO:
		/* y36721 todo */
		break;

	default:
		return -1;
	}

	SETREG8(REG_ISP_BANDFILTER_EN, 0x1);
	SETREG8(REG_ISP_BANDFILTER_FLAG, op);

	return 0;
}

int ispv1_get_anti_banding(void)
{
	u32 op = 0;
	camera_anti_banding banding;

	print_debug("enter %s", __func__);

	op = GETREG8(REG_ISP_BANDFILTER_FLAG);

	switch (op) {
	case 0:
		banding = CAMERA_ANTI_BANDING_OFF;
		break;

	case 1:
		banding = CAMERA_ANTI_BANDING_60Hz;
		break;

	case 2:
		banding = CAMERA_ANTI_BANDING_50Hz;
		break;
	default:
		return -1;
	}

	return banding;
}

/* blue,green,red gains */
u16 isp_mwb_gain[CAMERA_WHITEBALANCE_MAX][3] = {
	{0x0000, 0x0000, 0x0000}, /* AWB not care about it */
	{0x012c, 0x0080, 0x0089}, /* INCANDESCENT 2800K */
	{0x00f2, 0x0080, 0x00b9}, /* FLUORESCENT 4200K */
	{0x00a0, 0x00a0, 0x00a0}, /* WARM_FLUORESCENT, y36721 todo */
	{0x00d1, 0x0080, 0x00d2}, /* DAYLIGHT 5000K */
	{0x00b0, 0x0080, 0x00ec}, /* CLOUDY_DAYLIGHT 6500K*/
	{0x00a0, 0x00a0, 0x00a0}, /* TWILIGHT, y36721 todo */
	{0x0180, 0x0080, 0x0080}, /* CANDLELIGHT, 2300K */
};

#if 1
/* Added for awb */
int ispv1_set_awb(camera_white_balance awb_mode)
{
	print_debug("enter %s", __func__);
	/* default is auto, ...... */

#ifdef OVISP_DEBUG_MODE
	return 0;
#endif

	switch (awb_mode) {
	case CAMERA_WHITEBALANCE_AUTO:
		SETREG8(REG_ISP_AWB_MANUAL_ENABLE, 0x0);
		/*  Awb mode, should set CT-based AWB */
		SETREG8(REG_ISP_AWB_METHOD_TYPE, 0x1);
		break;

	case CAMERA_WHITEBALANCE_INCANDESCENT:
	case CAMERA_WHITEBALANCE_FLUORESCENT:
	case CAMERA_WHITEBALANCE_WARM_FLUORESCENT:
	case CAMERA_WHITEBALANCE_DAYLIGHT:
	case CAMERA_WHITEBALANCE_CLOUDY_DAYLIGHT:
	case CAMERA_WHITEBALANCE_TWILIGHT:
	case CAMERA_WHITEBALANCE_CANDLELIGHT:
		SETREG8(REG_ISP_AWB_MANUAL_ENABLE, 0x0);	/* y36721 fix it */
		SETREG8(REG_ISP_AWB_METHOD_TYPE, awb_mode + 2);
		SETREG16(REG_ISP_AWB_MANUAL_GAIN_BLUE(awb_mode - 1),
			 isp_mwb_gain[awb_mode][0]);
		SETREG16(REG_ISP_AWB_MANUAL_GAIN_GREEN(awb_mode - 1),
			 isp_mwb_gain[awb_mode][1]);
		SETREG16(REG_ISP_AWB_MANUAL_GAIN_RED(awb_mode - 1),
			 isp_mwb_gain[awb_mode][2]);
		break;

	default:
		print_error("unknow awb mode\n");
		return -1;
		break;
	}

	return 0;
}
#else
int ispv1_set_awb(camera_white_balance awb_mode)
{
	print_info("enter %s, awb mode", __func__, awb_mode);
	/* default is auto, ...... */

	switch (awb_mode) {
	case CAMERA_WHITEBALANCE_AUTO:
		SETREG8(REG_ISP_AWB_MANUAL_ENABLE, 0x0);
		/*  Awb mode, should set CT-based AWB */
		SETREG8(REG_ISP_AWB_METHOD_TYPE, 0x3);
		break;

	case CAMERA_WHITEBALANCE_INCANDESCENT:
		SETREG8(REG_ISP_AWB_MANUAL_ENABLE, 0x0);
		/*  Awb mode, should set CT-based AWB */
		SETREG8(REG_ISP_AWB_METHOD_TYPE, 0x4);
		break;
	case CAMERA_WHITEBALANCE_DAYLIGHT:
		SETREG8(REG_ISP_AWB_MANUAL_ENABLE, 0x0);
		/*  Awb mode, should set CT-based AWB */
		SETREG8(REG_ISP_AWB_METHOD_TYPE, 0x5);
		break;

	case CAMERA_WHITEBALANCE_FLUORESCENT:
		SETREG8(REG_ISP_AWB_MANUAL_ENABLE, 0x0);
		/*  Awb mode, should set CT-based AWB */
		SETREG8(REG_ISP_AWB_METHOD_TYPE, 0x6);
		break;

	case CAMERA_WHITEBALANCE_CLOUDY_DAYLIGHT:
		SETREG8(REG_ISP_AWB_MANUAL_ENABLE, 0x0);
		/*  Awb mode, should set CT-based AWB */
		SETREG8(REG_ISP_AWB_METHOD_TYPE, 0x7);
		break;

	default:
		print_error("unknow awb mode\n");
		return -1;
		break;
	}

	return 0;
}
#endif

/* Added for sharpness, y36721 todo */
int ispv1_set_sharpness(camera_sharpness sharpness)
{
	print_debug("enter %s", __func__);

	return 0;
}

/* Added for saturation, y36721 todo */
int ispv1_set_saturation(camera_saturation saturation)
{
	print_debug("enter %s, %d", __func__, saturation);
	this_ispdata->saturation = saturation;

	return 0;
}

int ispv1_set_saturation_done(camera_saturation saturation)
{
	print_debug("enter %s, %d", __func__, saturation);

	switch (saturation) {
	case CAMERA_SATURATION_L2:
		SETREG8(REG_ISP_TOP2, GETREG8(REG_ISP_TOP2) | ISP_SDE_ENABLE);
		SETREG8(REG_ISP_SDE_CTRL, GETREG8(REG_ISP_SDE_CTRL) | ISP_SATURATION_ENABLE);
		SETREG8(REG_ISP_SDE_U_SATURATION, 0x10);
		SETREG8(REG_ISP_SDE_V_SATURATION, 0x10);
		break;

	case CAMERA_SATURATION_L1:
		SETREG8(REG_ISP_TOP2, GETREG8(REG_ISP_TOP2) | ISP_SDE_ENABLE);
		SETREG8(REG_ISP_SDE_CTRL, GETREG8(REG_ISP_SDE_CTRL) | ISP_SATURATION_ENABLE);
		SETREG8(REG_ISP_SDE_U_SATURATION, 0x28);
		SETREG8(REG_ISP_SDE_V_SATURATION, 0x28);
		break;

	case CAMERA_SATURATION_H0:
		SETREG8(REG_ISP_TOP2, GETREG8(REG_ISP_TOP2) | ISP_SDE_ENABLE);
		SETREG8(REG_ISP_SDE_CTRL, GETREG8(REG_ISP_SDE_CTRL) | ISP_SATURATION_ENABLE);
		SETREG8(REG_ISP_SDE_U_SATURATION, 0x40);
		SETREG8(REG_ISP_SDE_V_SATURATION, 0x40);
		break;

	case CAMERA_SATURATION_H1:
		SETREG8(REG_ISP_TOP2, GETREG8(REG_ISP_TOP2) | ISP_SDE_ENABLE);
		SETREG8(REG_ISP_SDE_CTRL, GETREG8(REG_ISP_SDE_CTRL) | ISP_SATURATION_ENABLE);
		SETREG8(REG_ISP_SDE_U_SATURATION, 0x58);
		SETREG8(REG_ISP_SDE_V_SATURATION, 0x58);
		break;

	case CAMERA_SATURATION_H2:
		SETREG8(REG_ISP_TOP2, GETREG8(REG_ISP_TOP2) | ISP_SDE_ENABLE);
		SETREG8(REG_ISP_SDE_CTRL, GETREG8(REG_ISP_SDE_CTRL) | ISP_SATURATION_ENABLE);
		SETREG8(REG_ISP_SDE_U_SATURATION, 0x70);
		SETREG8(REG_ISP_SDE_V_SATURATION, 0x70);
		break;

	default:
		SETREG8(REG_ISP_TOP2, GETREG8(REG_ISP_TOP2) & (~ISP_SDE_ENABLE));
		break;
	}

	return 0;
}

int ispv1_set_contrast(camera_contrast contrast)
{
	print_debug("enter %s, %d", __func__, contrast);
	this_ispdata->contrast = contrast;

	return 0;
}

int ispv1_set_contrast_done(camera_contrast contrast)
{
	print_debug("enter %s, %d", __func__, contrast);

	SETREG8(REG_ISP_TOP2, GETREG8(REG_ISP_TOP2) | ISP_SDE_ENABLE);
	SETREG8(REG_ISP_SDE_CTRL, GETREG8(REG_ISP_SDE_CTRL) | ISP_CONTRAST_ENABLE);

	ispv1_switch_contrast(STATE_PREVIEW, contrast);

	return 0;
}

int ispv1_switch_contrast(camera_state state, camera_contrast contrast)
{
	if (state == STATE_PREVIEW) {
		switch (contrast) {
		case CAMERA_CONTRAST_L2:
			SETREG8(REG_ISP_SDE_CONTRAST, SDE_CONTRAST_PREVIEW_L2);
			SETREG8(REG_ISP_TOP5, GETREG8(REG_ISP_TOP5) & (~SDE_MANUAL_OFFSET_ENABLE));
			break;

		case CAMERA_CONTRAST_L1:
			SETREG8(REG_ISP_SDE_CONTRAST, SDE_CONTRAST_PREVIEW_L1);
			SETREG8(REG_ISP_TOP5, GETREG8(REG_ISP_TOP5) & (~SDE_MANUAL_OFFSET_ENABLE));
			break;

		case CAMERA_CONTRAST_H0:
			SETREG8(REG_ISP_SDE_CONTRAST, SDE_CONTRAST_PREVIEW_H0);
			SETREG8(REG_ISP_TOP5, (GETREG8(REG_ISP_TOP5) | SDE_MANUAL_OFFSET_ENABLE));
			break;

		case CAMERA_CONTRAST_H1:
			SETREG8(REG_ISP_SDE_CONTRAST, SDE_CONTRAST_PREVIEW_H1);
			SETREG8(REG_ISP_TOP5, GETREG8(REG_ISP_TOP5) & (~SDE_MANUAL_OFFSET_ENABLE));
			break;

		case CAMERA_CONTRAST_H2:
			SETREG8(REG_ISP_SDE_CONTRAST, SDE_CONTRAST_PREVIEW_H2);
			SETREG8(REG_ISP_TOP5, GETREG8(REG_ISP_TOP5) & (~SDE_MANUAL_OFFSET_ENABLE));
			break;

		default:
			print_error("%s, not supported contrast %d", __func__, contrast);
			break;
		}
	} else if (state == STATE_CAPTURE) {
		switch (contrast) {
		case CAMERA_CONTRAST_L2:
			SETREG8(REG_ISP_SDE_CONTRAST, SDE_CONTRAST_CAPTURE_L2);
			SETREG8(REG_ISP_TOP5, GETREG8(REG_ISP_TOP5) & (~SDE_MANUAL_OFFSET_ENABLE));
			break;

		case CAMERA_CONTRAST_L1:
			SETREG8(REG_ISP_SDE_CONTRAST, SDE_CONTRAST_CAPTURE_L1);
			SETREG8(REG_ISP_TOP5, GETREG8(REG_ISP_TOP5) & (~SDE_MANUAL_OFFSET_ENABLE));
			break;

		case CAMERA_CONTRAST_H0:
			SETREG8(REG_ISP_SDE_CONTRAST, SDE_CONTRAST_CAPTURE_H0);
			SETREG8(REG_ISP_TOP5, GETREG8(REG_ISP_TOP5) & (~SDE_MANUAL_OFFSET_ENABLE));
			break;

		case CAMERA_CONTRAST_H1:
			SETREG8(REG_ISP_SDE_CONTRAST, SDE_CONTRAST_CAPTURE_H1);
			SETREG8(REG_ISP_TOP5, GETREG8(REG_ISP_TOP5) & (~SDE_MANUAL_OFFSET_ENABLE));
			break;

		case CAMERA_CONTRAST_H2:
			SETREG8(REG_ISP_SDE_CONTRAST, SDE_CONTRAST_CAPTURE_H2);
			SETREG8(REG_ISP_TOP5, GETREG8(REG_ISP_TOP5) & (~SDE_MANUAL_OFFSET_ENABLE));
			break;

		default:
			print_error("%s, not supported contrast %d", __func__, contrast);
			break;
		}
	}

	return 0;
}

void ispv1_set_fps_lock(int lock)
{
	print_debug("enter %s", __func__);
	fps_lock = lock;
}

void ispv1_change_fps(camera_frame_rate_mode mode)
{
	print_debug("enter :%s, mode :%d", __func__, mode);
	if (fps_lock == true) {
		print_error("fps is locked");
		return;
	}
	if (mode >= CAMERA_FRAME_RATE_MAX)
		print_error("inviable camera_frame_rate_mode: %d", mode);

	if (CAMERA_FRAME_RATE_FIX_MAX == mode) {
		this_ispdata->sensor->fps_min = isp_hw_data.fps_max;
		this_ispdata->sensor->fps_max = isp_hw_data.fps_max;
		ispv1_set_frame_rate(CAMERA_FRAME_RATE_FIX_MAX, this_ispdata->sensor);
	} else if (CAMERA_FRAME_RATE_FIX_MIN == mode) {
		this_ispdata->sensor->fps_min = isp_hw_data.fps_min;
		this_ispdata->sensor->fps_max = isp_hw_data.fps_min;
		ispv1_set_frame_rate(CAMERA_FRAME_RATE_FIX_MIN, this_ispdata->sensor);
	} else if (CAMERA_FRAME_RATE_AUTO == mode) {
		this_ispdata->sensor->fps_min = isp_hw_data.fps_min;
		this_ispdata->sensor->fps_max = isp_hw_data.fps_max;
	}

	this_ispdata->fps_mode = mode;
}

void ispv1_change_max_exposure(camera_sensor *sensor, camera_max_exposrure mode)
{
	u8 frame_index, fps;
	u32 vts;
	u16 max_exposure;

	frame_index = sensor->preview_frmsize_index;
	fps = sensor->frmsize_list[frame_index].fps;
	vts = sensor->frmsize_list[frame_index].vts;

	if (CAMERA_MAX_EXPOSURE_LIMIT == mode)
		max_exposure = (fps * vts / 100 - 14);
	else
		max_exposure = ((fps * vts / sensor->fps) - 14);

	SETREG16(REG_ISP_MAX_EXPOSURE, max_exposure);
	SETREG16(REG_ISP_MAX_EXPOSURE_SHORT, max_exposure);
}

int ispv1_set_scene(camera_scene scene)
{
	u8 target_y_low = scene_target_y_low;
	u8 target_y_high = scene_target_y_high;

#ifdef OVISP_DEBUG_MODE
	return 0;
#endif

	print_debug("enter %s, scene:%d", __func__, scene);

	switch (scene) {
	case CAMERA_SCENE_AUTO:
		print_info("case CAMERA_SCENE_AUTO ");
		ispv1_change_fps(CAMERA_FRAME_RATE_AUTO);
		ispv1_change_max_exposure(this_ispdata->sensor, CAMERA_MAX_EXPOSURE_RESUME);
		SETREG8(REG_ISP_TARGET_Y_LOW, target_y_low);
		SETREG8(REG_ISP_TARGET_Y_HIGH, target_y_high);
		this_ispdata->flash_mode = isp_hw_data.flash_mode;
		SETREG8(REG_ISP_UV_ADJUST, UV_ADJUST_ENABLE);
		SETREG8(REG_ISP_UV_SATURATION, 0x80);
		ispv1_set_focus_range(CAMERA_FOCUS_AUTO);
		ispv1_set_awb(this_ispdata->awb_mode);
		break;

	case CAMERA_SCENE_ACTION:
		print_info("case CAMERA_SCENE_ACTION ");
		/*
		 * Reduce max exposure time 1/100s
		 * Pre-focus recommended. (optional)
		 */
		ispv1_change_fps(CAMERA_FRAME_RATE_FIX_MAX);
		ispv1_change_max_exposure(this_ispdata->sensor, CAMERA_MAX_EXPOSURE_LIMIT);
		SETREG8(REG_ISP_TARGET_Y_LOW, target_y_low);
		SETREG8(REG_ISP_TARGET_Y_HIGH, target_y_high);
		this_ispdata->flash_mode = CAMERA_FLASH_OFF;
		SETREG8(REG_ISP_UV_ADJUST, UV_ADJUST_ENABLE);
		SETREG8(REG_ISP_UV_SATURATION, 0x80);
		ispv1_set_focus_range(CAMERA_FOCUS_AUTO);
		ispv1_set_awb(CAMERA_WHITEBALANCE_AUTO);
		break;

	case CAMERA_SCENE_PORTRAIT:
		print_info("case CAMERA_SCENE_PORTRAIT ");
		/*
		 * Pre-tuned color matrix for better skin tone recommended. (optional)
		 */
		ispv1_change_fps(CAMERA_FRAME_RATE_AUTO);
		ispv1_change_max_exposure(this_ispdata->sensor, CAMERA_MAX_EXPOSURE_RESUME);
		SETREG8(REG_ISP_TARGET_Y_LOW, target_y_low);
		SETREG8(REG_ISP_TARGET_Y_HIGH, target_y_high);
		this_ispdata->flash_mode = isp_hw_data.flash_mode;
		SETREG8(REG_ISP_UV_ADJUST, UV_ADJUST_ENABLE);
		SETREG8(REG_ISP_UV_SATURATION, 0x90);
		ispv1_set_focus_range(CAMERA_FOCUS_AUTO);
		ispv1_set_awb(CAMERA_WHITEBALANCE_AUTO);
		break;

	case CAMERA_SCENE_LANDSPACE:
		print_info("case CAMERA_SCENE_LANDSPACE ");
		/*
		 * Focus mode set to infinity
		 * Manual AWB to daylight (optional)
		 */
		ispv1_change_fps(CAMERA_FRAME_RATE_AUTO);
		ispv1_change_max_exposure(this_ispdata->sensor, CAMERA_MAX_EXPOSURE_RESUME);
		SETREG8(REG_ISP_TARGET_Y_LOW, target_y_low);
		SETREG8(REG_ISP_TARGET_Y_HIGH, target_y_high);
		this_ispdata->flash_mode = isp_hw_data.flash_mode;
		SETREG8(REG_ISP_UV_ADJUST, UV_ADJUST_ENABLE);
		SETREG8(REG_ISP_UV_SATURATION, 0x98);
		ispv1_set_focus_range(CAMERA_FOCUS_AUTO);
		ispv1_set_awb(CAMERA_WHITEBALANCE_AUTO);
		break;

	case CAMERA_SCENE_NIGHT:
		print_info("case CAMERA_SCENE_NIGHT ");
		/*
		 * Increase max exposure time£¬Ö¡ÂÊ½µµÍµ½5fps×óÓÒ
		 * Turn off the flash
		 * Turn off the UV adjust
		 * Focus mode set to infinity
		 */
		ispv1_change_fps(CAMERA_FRAME_RATE_FIX_MIN);
		ispv1_change_max_exposure(this_ispdata->sensor, CAMERA_MAX_EXPOSURE_RESUME);
		SETREG8(REG_ISP_TARGET_Y_LOW, target_y_low);
		SETREG8(REG_ISP_TARGET_Y_HIGH, target_y_high);
		this_ispdata->flash_mode = CAMERA_FLASH_OFF;
		SETREG8(REG_ISP_UV_ADJUST, UV_ADJUST_DISABLE);
		SETREG8(REG_ISP_UV_SATURATION, 0x80);
		ispv1_set_focus_range(CAMERA_FOCUS_AUTO);
		ispv1_set_awb(CAMERA_WHITEBALANCE_AUTO);
		break;

	case CAMERA_SCENE_NIGHT_PORTRAIT:
		print_info("case CAMERA_SCENE_NIGHT_PORTRAIT ");
		/*
		 * Increase max exposure time
		 * Turn off UV adjust
		 * Turn on the flash (optional)
		 */
		ispv1_change_fps(CAMERA_FRAME_RATE_FIX_MIN);
		ispv1_change_max_exposure(this_ispdata->sensor, CAMERA_MAX_EXPOSURE_RESUME);
		SETREG8(REG_ISP_TARGET_Y_LOW, target_y_low);
		SETREG8(REG_ISP_TARGET_Y_HIGH, target_y_high);
		this_ispdata->flash_mode = CAMERA_FLASH_ON;
		SETREG8(REG_ISP_UV_ADJUST, UV_ADJUST_DISABLE);
		SETREG8(REG_ISP_UV_SATURATION, 0x90);
		ispv1_set_focus_range(CAMERA_FOCUS_AUTO);
		ispv1_set_awb(CAMERA_WHITEBALANCE_AUTO);
		break;

	case CAMERA_SCENE_THEATRE:
		print_info("case CAMERA_SCENE_THEATRE ");
		/*
		 * Increase max exposure time£¬Ö¡ÂÊ½µµÍµ½5fps×óÓÒ
		 * Turn off the flash
		 * Turn off UV adjust
		 */
		ispv1_change_fps(CAMERA_FRAME_RATE_FIX_MIN);
		ispv1_change_max_exposure(this_ispdata->sensor, CAMERA_MAX_EXPOSURE_RESUME);
		SETREG8(REG_ISP_TARGET_Y_LOW, target_y_low);
		SETREG8(REG_ISP_TARGET_Y_HIGH, target_y_high);
		this_ispdata->flash_mode = CAMERA_FLASH_OFF;
		SETREG8(REG_ISP_UV_ADJUST, UV_ADJUST_DISABLE);
		SETREG8(REG_ISP_UV_SATURATION, 0x70);
		ispv1_set_focus_range(CAMERA_FOCUS_AUTO);
		ispv1_set_awb(CAMERA_WHITEBALANCE_AUTO);
		break;

	case CAMERA_SCENE_BEACH:
		print_info("case CAMERA_SCENE_BEACH ");
		/*
		 * Reduce AE target
		 * Manual AWB set to daylight (optional)
		 */
		ispv1_change_fps(CAMERA_FRAME_RATE_AUTO);
		ispv1_change_max_exposure(this_ispdata->sensor, CAMERA_MAX_EXPOSURE_RESUME);
		ispv1_calc_ev(&target_y_low, &target_y_high, -2);
		SETREG8(REG_ISP_TARGET_Y_LOW, target_y_low);
		SETREG8(REG_ISP_TARGET_Y_HIGH, target_y_high);
		this_ispdata->flash_mode = isp_hw_data.flash_mode;
		SETREG8(REG_ISP_UV_ADJUST, UV_ADJUST_ENABLE);
		SETREG8(REG_ISP_UV_SATURATION, 0x80);
		ispv1_set_focus_range(CAMERA_FOCUS_AUTO);
		ispv1_set_awb(CAMERA_WHITEBALANCE_AUTO);
		break;

	case CAMERA_SCENE_SNOW:
		print_info("case CAMERA_SCENE_SNOW ");
		/*
		 * Increase AE target
		 * Enable EDR mode
		 * Manual AWB set to daylight (optional)
		 */
		ispv1_change_fps(CAMERA_FRAME_RATE_AUTO);
		ispv1_change_max_exposure(this_ispdata->sensor, CAMERA_MAX_EXPOSURE_RESUME);
		ispv1_calc_ev(&target_y_low, &target_y_high, 2);
		SETREG8(REG_ISP_TARGET_Y_LOW, target_y_low);
		SETREG8(REG_ISP_TARGET_Y_HIGH, target_y_high);
		print_info("flash_mode:%d", isp_hw_data.flash_mode);
		this_ispdata->flash_mode = isp_hw_data.flash_mode;
		SETREG8(REG_ISP_UV_ADJUST, UV_ADJUST_ENABLE);
		SETREG8(REG_ISP_UV_SATURATION, 0x80);
		ispv1_set_focus_range(CAMERA_FOCUS_AUTO);
		ispv1_set_awb(CAMERA_WHITEBALANCE_AUTO);
		break;

	case CAMERA_SCENE_FIREWORKS:
		print_info("case CAMERA_SCENE_FIREWORKS ");
		/*
		 * Increase max exposure time£¬Ö¡ÂÊ½µµÍµ½5fps×óÓÒ
		 * Focus mode set to infinity
		 * Turn off the flash
		 */
		ispv1_change_fps(CAMERA_FRAME_RATE_FIX_MIN);
		ispv1_change_max_exposure(this_ispdata->sensor, CAMERA_MAX_EXPOSURE_RESUME);
		SETREG8(REG_ISP_TARGET_Y_LOW, target_y_low);
		SETREG8(REG_ISP_TARGET_Y_HIGH, target_y_high);
		this_ispdata->flash_mode = CAMERA_FLASH_OFF;
		SETREG8(REG_ISP_UV_ADJUST, UV_ADJUST_ENABLE);
		SETREG8(REG_ISP_UV_SATURATION, 0x80);
		ispv1_set_focus_range(CAMERA_FOCUS_AUTO);
		ispv1_set_awb(CAMERA_WHITEBALANCE_AUTO);
		break;

	case CAMERA_SCENE_CANDLELIGHT:
		print_info("case CAMERA_SCENE_CANDLELIGHT ");
		/*
		 * Increase max exposure time£¬Ö¡ÂÊ½µµÍµ½5fps×óÓÒ
		 * Turn off the flash
		 * Turn off UV adjust
		 * AWB set to manual with fixed AWB gain with yellow/warm tone
		 */
		ispv1_change_fps(CAMERA_FRAME_RATE_FIX_MIN);
		ispv1_change_max_exposure(this_ispdata->sensor, CAMERA_MAX_EXPOSURE_RESUME);
		SETREG8(REG_ISP_TARGET_Y_LOW, target_y_low);
		SETREG8(REG_ISP_TARGET_Y_HIGH, target_y_high);
		this_ispdata->flash_mode = CAMERA_FLASH_OFF;
		SETREG8(REG_ISP_UV_ADJUST, UV_ADJUST_DISABLE);
		SETREG8(REG_ISP_UV_SATURATION, 0x80);
		ispv1_set_focus_range(CAMERA_FOCUS_AUTO);
		ispv1_set_awb(CAMERA_WHITEBALANCE_CANDLELIGHT);
		break;

	case CAMERA_SCENE_FLOWERS:
		print_info("case CAMERA_SCENE_FLOWER ");

		ispv1_change_fps(CAMERA_FRAME_RATE_AUTO);
		ispv1_change_max_exposure(this_ispdata->sensor, CAMERA_MAX_EXPOSURE_RESUME);
		SETREG8(REG_ISP_TARGET_Y_LOW, target_y_low);
		SETREG8(REG_ISP_TARGET_Y_HIGH, target_y_high);
		SETREG8(REG_ISP_UV_ADJUST, UV_ADJUST_ENABLE);
		SETREG8(REG_ISP_UV_SATURATION, 0x80);
		this_ispdata->flash_mode = CAMERA_FLASH_OFF;
		ispv1_set_focus_range(CAMERA_FOCUS_MACRO);
		ispv1_set_awb(CAMERA_WHITEBALANCE_AUTO);
		break;

	case CAMERA_SCENE_SUNSET:
	case CAMERA_SCENE_STEADYPHOTO:
	case CAMERA_SCENE_SPORTS:
	case CAMERA_SCENE_BARCODE:
	default:
		print_info("This scene not supported yet. ");
		ispv1_change_fps(CAMERA_FRAME_RATE_AUTO);
		ispv1_change_max_exposure(this_ispdata->sensor, CAMERA_MAX_EXPOSURE_RESUME);
		SETREG8(REG_ISP_TARGET_Y_LOW, target_y_low);
		SETREG8(REG_ISP_TARGET_Y_HIGH, target_y_high);
		this_ispdata->flash_mode = isp_hw_data.flash_mode;
		this_ispdata->scene = CAMERA_SCENE_AUTO;
		SETREG8(REG_ISP_UV_ADJUST, UV_ADJUST_ENABLE);
		SETREG8(REG_ISP_UV_SATURATION, 0x80);
		ispv1_set_focus_range(CAMERA_FOCUS_AUTO);
		ispv1_set_awb(CAMERA_WHITEBALANCE_AUTO);
		goto out;
	}

	this_ispdata->scene = scene;

out:
	return 0;
}

int ispv1_set_brightness(camera_brightness brightness)
{
	print_debug("enter %s, %d", __func__, brightness);
	this_ispdata->brightness = brightness;

	return 0;
}

int ispv1_set_brightness_done(camera_brightness brightness)
{
	print_debug("enter %s, %d", __func__, brightness);

	switch (brightness) {
	case CAMERA_BRIGHTNESS_L2:
		SETREG8(REG_ISP_TOP2, GETREG8(REG_ISP_TOP2) | ISP_SDE_ENABLE);
		SETREG8(REG_ISP_SDE_CTRL, GETREG8(REG_ISP_SDE_CTRL) | ISP_BRIGHTNESS_ENABLE);
		SETREG8(REG_ISP_SDE_SIGN_SET, GETREG8(REG_ISP_SDE_SIGN_SET) | ISP_BRIGHTNESS_SIGN_NEGATIVE);
		break;

	case CAMERA_BRIGHTNESS_L1:
		SETREG8(REG_ISP_TOP2, GETREG8(REG_ISP_TOP2) | ISP_SDE_ENABLE);
		SETREG8(REG_ISP_SDE_CTRL, GETREG8(REG_ISP_SDE_CTRL) | ISP_BRIGHTNESS_ENABLE);
		SETREG8(REG_ISP_SDE_SIGN_SET, GETREG8(REG_ISP_SDE_SIGN_SET) | ISP_BRIGHTNESS_SIGN_NEGATIVE);
		break;

	case CAMERA_BRIGHTNESS_H0:
		SETREG8(REG_ISP_TOP2, GETREG8(REG_ISP_TOP2) | ISP_SDE_ENABLE);
		SETREG8(REG_ISP_SDE_CTRL, GETREG8(REG_ISP_SDE_CTRL) | ISP_BRIGHTNESS_ENABLE);
		SETREG8(REG_ISP_SDE_SIGN_SET, GETREG8(REG_ISP_SDE_SIGN_SET) & (~ISP_BRIGHTNESS_SIGN_NEGATIVE));
		break;

	case CAMERA_BRIGHTNESS_H1:
		SETREG8(REG_ISP_TOP2, GETREG8(REG_ISP_TOP2) | ISP_SDE_ENABLE);
		SETREG8(REG_ISP_SDE_CTRL, GETREG8(REG_ISP_SDE_CTRL) | ISP_BRIGHTNESS_ENABLE);
		SETREG8(REG_ISP_SDE_SIGN_SET, GETREG8(REG_ISP_SDE_SIGN_SET) & (~ISP_BRIGHTNESS_SIGN_NEGATIVE));
		break;

	case CAMERA_BRIGHTNESS_H2:
		SETREG8(REG_ISP_TOP2, GETREG8(REG_ISP_TOP2) | ISP_SDE_ENABLE);
		SETREG8(REG_ISP_SDE_CTRL, GETREG8(REG_ISP_SDE_CTRL) | ISP_BRIGHTNESS_ENABLE);
		SETREG8(REG_ISP_SDE_SIGN_SET, GETREG8(REG_ISP_SDE_SIGN_SET) & (~ISP_BRIGHTNESS_SIGN_NEGATIVE));
		break;

	default:
		print_error("%s, not supported brightness %d", __func__, brightness);
		break;
	}

	ispv1_switch_brightness(STATE_PREVIEW, brightness);

	return 0;
}

int ispv1_switch_brightness(camera_state state, camera_brightness brightness)
{
	if (state == STATE_PREVIEW) {
		switch (brightness) {
		case CAMERA_BRIGHTNESS_L2:
			SETREG8(REG_ISP_SDE_BRIGHTNESS, SDE_BRIGHTNESS_PREVIEW_L2);
			break;

		case CAMERA_BRIGHTNESS_L1:
			SETREG8(REG_ISP_SDE_BRIGHTNESS, SDE_BRIGHTNESS_PREVIEW_L1);
			break;

		case CAMERA_BRIGHTNESS_H0:
			SETREG8(REG_ISP_SDE_BRIGHTNESS, SDE_BRIGHTNESS_PREVIEW_H0);
			break;

		case CAMERA_BRIGHTNESS_H1:
			SETREG8(REG_ISP_SDE_BRIGHTNESS, SDE_BRIGHTNESS_PREVIEW_H1);
			break;

		case CAMERA_BRIGHTNESS_H2:
			SETREG8(REG_ISP_SDE_BRIGHTNESS, SDE_BRIGHTNESS_PREVIEW_H2);
			break;

		default:
			break;
		}
	} else if (state == STATE_CAPTURE) {
		switch (brightness) {
		case CAMERA_BRIGHTNESS_L2:
			SETREG8(REG_ISP_SDE_BRIGHTNESS, SDE_BRIGHTNESS_CAPTURE_L2);
			break;

		case CAMERA_BRIGHTNESS_L1:
			SETREG8(REG_ISP_SDE_BRIGHTNESS, SDE_BRIGHTNESS_CAPTURE_L1);
			break;

		case CAMERA_BRIGHTNESS_H0:
			SETREG8(REG_ISP_SDE_BRIGHTNESS, SDE_BRIGHTNESS_CAPTURE_H0);
			break;

		case CAMERA_BRIGHTNESS_H1:
			SETREG8(REG_ISP_SDE_BRIGHTNESS, SDE_BRIGHTNESS_CAPTURE_H1);
			break;

		case CAMERA_BRIGHTNESS_H2:
			SETREG8(REG_ISP_SDE_BRIGHTNESS, SDE_BRIGHTNESS_CAPTURE_H2);
			break;

		default:
			break;
		}
	}

	return 0;
}

int ispv1_set_effect(camera_effects effect)
{
	print_debug("enter %s, %d", __func__, effect);
	this_ispdata->effect = effect;

	return 0;
}

int ispv1_set_effect_done(camera_effects effect)
{
	print_debug("enter %s, %d", __func__, effect);

	switch (effect) {
	case CAMERA_EFFECT_NONE:
		SETREG8(REG_ISP_TOP2, GETREG8(REG_ISP_TOP2) | ISP_SDE_ENABLE);
		SETREG8(REG_ISP_SDE_CTRL, ISP_CONTRAST_ENABLE | ISP_BRIGHTNESS_ENABLE | ISP_SATURATION_ENABLE);
		break;

	case CAMERA_EFFECT_MONO:
		SETREG8(REG_ISP_TOP2, GETREG8(REG_ISP_TOP2) | ISP_SDE_ENABLE);
		SETREG8(REG_ISP_SDE_CTRL, ISP_MONO_EFFECT_ENABLE);
		break;

	case CAMERA_EFFECT_NEGATIVE:
		SETREG8(REG_ISP_TOP2, GETREG8(REG_ISP_TOP2) | ISP_SDE_ENABLE);
		SETREG8(REG_ISP_SDE_CTRL, ISP_NEGATIVE_EFFECT_ENABLE);
		break;

	case CAMERA_EFFECT_SEPIA:
		SETREG8(REG_ISP_TOP2, GETREG8(REG_ISP_TOP2) | ISP_SDE_ENABLE);
		SETREG8(REG_ISP_SDE_CTRL, ISP_FIX_U_ENABLE | ISP_FIX_V_ENABLE);
		SETREG8(REG_ISP_SDE_U_REG, 0x30);
		SETREG8(REG_ISP_SDE_V_REG, 0xb0);
		break;

	default:
		SETREG8(REG_ISP_TOP2, GETREG8(REG_ISP_TOP2) & (~ISP_SDE_ENABLE));
		break;
	}

	return 0;
}

int ispv1_set_effect_saturation_done(camera_effects effect, camera_saturation saturation)
{
	if(effect == CAMERA_EFFECT_SEPIA) {
		ispv1_set_effect_done(effect);
	} else {
		ispv1_set_effect_done(effect);
		ispv1_set_saturation_done(saturation);
	}
	return 0;
}

/*
 * Added for hue, y36721 todo
 * flag: if 1 is on, 0 is off, default is off
 */
int ispv1_set_hue(int flag)
{
	print_debug("enter %s", __func__);

	return 0;
}

/*
 * before start_preview or start_capture, it should be called to update size information
 * please see ISP manual or software manual.
 */
int ispv1_update_LENC_scale(u32 inwidth, u32 inheight)
{
	u32 scale;

	print_debug("enter %s", __func__);

	scale = (0x100000 * 3) / inwidth;
	SETREG16(REG_ISP_LENC_BRHSCALE, scale);

	scale = (0x100000 * 3) / inheight;
	SETREG16(REG_ISP_LENC_BRVSCALE, scale);

	scale = (0x100000 * 4) / inwidth;
	SETREG16(REG_ISP_LENC_GHSCALE, scale);

	scale = (0x80000 * 4) / inheight;
	SETREG16(REG_ISP_LENC_GVSCALE, scale);

	return 0;

}

/*
 * Related to sensor.
 */
int ispv1_init_LENC(u8 *lensc_param)
{
	u32 loopi;
	u8 *param;

	print_debug("enter %s", __func__);

	assert(lensc_param);

	/* set long exposure */
	param = lensc_param;
	for (loopi = 0; loopi < LENS_CP_ARRAY_BYTES; loopi++)
		SETREG8(REG_ISP_LENS_CP_ARRAY_LONG + loopi, *param++);

	/* set short exposure, just set same with long exposure, y36721 todo */
	param = lensc_param;
	for (loopi = 0; loopi < LENS_CP_ARRAY_BYTES; loopi++)
		SETREG8(REG_ISP_LENS_CP_ARRAY_SHORT + loopi, *param++);

	return 0;
}

/*
 * Related to sensor.
 */
int ispv1_init_CCM(u16 *ccm_param)
{
	u32 loopi;
	u16 *param;

	print_debug("enter %s ", __func__);

	assert(ccm_param);

	param = ccm_param;
	for (loopi = 0; loopi < CCM_MATRIX_ARRAY_SIZE16; loopi++) {
		SETREG8(REG_ISP_CCM_MATRIX + loopi * 2, (*param & 0xff00) >> 8);
		SETREG8(REG_ISP_CCM_MATRIX + loopi * 2 + 1, *param & 0xff);
		param++;
	}

	return 0;
}

/*
 * Related to sensor.
 */
int ispv1_init_AWB(u8 *awb_param)
{
	u32 loopi;
	u8 *param;

	print_debug("enter %s", __func__);

	assert(awb_param);

	param = awb_param;
	for (loopi = 0; loopi < AWB_CTRL_ARRAY_BYTES; loopi++)
		SETREG8(REG_ISP_AWB_CTRL + loopi, *param++);

	SETREG8(REG_ISP_CCM_CENTERCT_THRESHOLDS, *param++);
	SETREG8(REG_ISP_CCM_CENTERCT_THRESHOLDS + 1, *param++);
	SETREG8(REG_ISP_CCM_LEFTCT_THRESHOLDS, *param++);
	SETREG8(REG_ISP_CCM_LEFTCT_THRESHOLDS + 1, *param++);
	SETREG8(REG_ISP_CCM_RIGHTCT_THRESHOLDS, *param++);
	SETREG8(REG_ISP_CCM_RIGHTCT_THRESHOLDS + 1, *param++);

	for (loopi = 0; loopi < LENS_CT_THRESHOLDS_SIZE16; loopi++) {
		SETREG8(REG_ISP_LENS_CT_THRESHOLDS + loopi * 2, *param++);
		SETREG8(REG_ISP_LENS_CT_THRESHOLDS + loopi * 2 + 1, *param++);
	}

	return 0;
}

/* Added for binning correction, y36721 todo */
int ispv1_init_BC(int binningMode, int mirror, int filp)
{
	print_debug("enter %s", __func__);

	return 0;
}

/* Added for defect_pixel_correction, y36721 todo */
int ispv1_init_DPC(int bWhitePixel, int bBlackPixel)
{
	print_debug("enter %s", __func__);

	return 0;
}

/*
 * Added for raw DNS, y36721 todo
 * flag: if 1 is on, 0 is off, default is on
 */
int ispv1_init_rawDNS(int flag)
{
	print_debug("enter %s", __func__);

	return 0;
}

/*
 * Added for uv DNS, y36721 todo
 * flag: if 1 is on, 0 is off, default is on
 */
int ispv1_init_uvDNS(int flag)
{
	print_debug("enter %s", __func__);

	return 0;
}

/*
 * Added for GbGr DNS, y36721 todo
 * level: if >0 is on, 0 is off, default is 6
 * 16: keep full Gb/Gr difference as resolution;
 * 8: remove half Gb/Gr difference;
 * 0: remove all Gb/Gr difference;
 */
int ispv1_init_GbGrDNS(int level)
{
	print_debug("enter %s", __func__);

	return 0;
}

/*
 * Added for GRB Gamma, y36721 todo
 * flag: if 1 is on, 0 is off, default is on, 0x65004 bit[5]
 */
int ispv1_init_RGBGamma(int flag)
{
	print_debug("enter %s", __func__);

	return 0;
}

/*
 * For not OVT sensors,  MCU won't write exposure and gain back to sensor directly.
 * In this case, exposure and gain need Host to handle.
 */
void ispv1_cmd_id_do_ecgc(void)
{
	u16 gain;
	u32 exposure;

	print_debug("enter %s, cmd id 0x%x", __func__, isp_hw_data.aec_cmd_id);

	if (CMD_WRITEBACK_EXPO_GAIN == isp_hw_data.aec_cmd_id) {
		exposure = get_writeback_expo();
		if (this_ispdata->sensor->set_exposure)
			this_ispdata->sensor->set_exposure(exposure);

		gain = get_writeback_gain();
		if (this_ispdata->sensor->set_gain)
			this_ispdata->sensor->set_gain(gain);

		print_info("expo 0x%x, gain 0x%x, currentY 0x%x", exposure, gain, get_current_y());
	} else if (CMD_WRITEBACK_EXPO == isp_hw_data.aec_cmd_id) {
		exposure = get_writeback_expo();
		if (this_ispdata->sensor->set_exposure)
			this_ispdata->sensor->set_exposure(exposure);

		print_info("expo 0x%x, currentY 0x%x", exposure, get_current_y());
	} else if (CMD_WRITEBACK_GAIN == isp_hw_data.aec_cmd_id) {
		gain = get_writeback_gain();
		if (this_ispdata->sensor->set_gain)
			this_ispdata->sensor->set_gain(gain);

		print_info("gain 0x%x, currentY 0x%x", gain, get_current_y());
	} else {
		print_error("%s:unknow cmd id", __func__);
	}
}

static int ispv1_set_frame_rate(camera_frame_rate_mode mode, camera_sensor *sensor)
{
	int frame_rate_level = 0;
	u16 vts, fullfps, fps, step;
	u32 max_fps, min_fps;

	fullfps = sensor->frmsize_list[sensor->preview_frmsize_index].fps;
	step = 1;
	if (CAMERA_FRAME_RATE_FIX_MAX == mode) {
		max_fps = (isp_hw_data.fps_max > fullfps) ? fullfps : isp_hw_data.fps_max;
		min_fps = max_fps;
		max_level = (max_fps - min_fps) / step;
		frame_rate_level = 0;
	} else if (CAMERA_FRAME_RATE_FIX_MIN == mode) {
		min_fps = (isp_hw_data.fps_min < fullfps) ? isp_hw_data.fps_min : fullfps;
		max_fps = (isp_hw_data.fps_max > fullfps) ? fullfps : isp_hw_data.fps_max;
		max_level = (max_fps - min_fps) / step;
		frame_rate_level = max_level;
	}

	fps = fullfps - step * frame_rate_level;

	sensor->fps = fps;

	/* rules: vts1*fps1 = vts2*fps2 */
	vts = sensor->frmsize_list[sensor->preview_frmsize_index].vts;
	vts = vts * fullfps / fps;

	if (sensor->set_vts) {
		sensor->set_vts(vts);
	} else {
		print_error("set_vts null");
		goto error;
	}

	SETREG16(REG_ISP_MAX_EXPOSURE, (vts - 14));
	camera_set_frame_rate_level(frame_rate_level);
	return 0;
error:
	return -1;
}

static int ispv1_change_frame_rate(camera_auto_frame_rate direction, camera_sensor *sensor)
{
	int frame_rate_level = camera_get_frame_rate_level();
	u16 vts, fullfps, fps, step;
	bool level_changed = false;
	u32 max_fps, min_fps;

	fullfps = sensor->frmsize_list[sensor->preview_frmsize_index].fps;
	step = 1;
	max_fps = (isp_hw_data.fps_max > fullfps) ? fullfps : isp_hw_data.fps_max;
	min_fps = (isp_hw_data.fps_min < fullfps) ? isp_hw_data.fps_min : fullfps;
	if (min_fps > max_fps)
		min_fps = max_fps;
	max_level = (max_fps - min_fps) / step;

	if (CAMERA_AUTO_FRAME_RATE_DOWN == direction) {
		if (max_level <= frame_rate_level) {
			print_debug("Has arrival max frame_rate level");
			return 0;
		} else {
			frame_rate_level++;
			level_changed = true;
		}
	} else if (CAMERA_AUTO_FRAME_RATE_UP == direction) {
		if (0 == frame_rate_level) {
			print_debug("Has arrival min frame_rate level");
			return 0;
		} else {
			frame_rate_level--;
			level_changed = true;
		}
	}

	fps = fullfps - step * frame_rate_level;
	if ((fps > sensor->fps_max) || (fps < sensor->fps_min)) {
		print_info("auto fps:%d", fps);
		return 0;
	}

	if (true == level_changed) {
		if ((sensor->fps_min <= fps) && (sensor->fps_max >= fps)) {
			sensor->fps = fps;
		} else if (sensor->fps_min > fps) {
			frame_rate_level = 0;
			sensor->fps = fullfps;
			fps = fullfps;
		} else {
			print_error("can't do auto fps, level:%d, cur_fps:%d, tar_fps:%d, ori_fps:%d, max_fps:%d, min_fps:%d",
				frame_rate_level, sensor->fps, fps, fullfps, sensor->fps_max, sensor->fps_min);
			goto error;
		}

		/* rules: vts1*fps1 = vts2*fps2 */
		vts = sensor->frmsize_list[sensor->preview_frmsize_index].vts;
		vts = vts * fullfps / fps;

		/* y36721 2012-04-23 add begin to avoid preview flicker when frame rate up */
		if ((vts - 14) < (get_writeback_expo() / 0x10)) {
			SETREG16(REG_ISP_MAX_EXPOSURE, (vts - 14));
			print_warn("current expo too large");
			return -1;
		}
		/* y36721 2012-04-23 add end */

		if (sensor->set_vts) {
			sensor->set_vts(vts);
		} else {
			print_error("set_vts null");
			goto error;
		}

		SETREG16(REG_ISP_MAX_EXPOSURE, (vts - 14));
		camera_set_frame_rate_level(frame_rate_level);
	}
	return 0;
error:
	return -1;
}

/* #define PLATFORM_TYPE_PAD_S10 */
#define BOARD_ID_CS_U9510		0x67
#define BOARD_ID_CS_U9510E		0x66
#define BOARD_ID_CS_T9510E		0x06
awb_gain_t flash_platform_awb[FLASH_PLATFORM_MAX] = 
{
	{0xc8, 0x80, 0x80, 0x104}, /* U9510 */
	{0xd6, 0x80, 0x80, 0x104}, /* U9510E/T9510E, recording AWB is 0xd0,0xfc, change a little */
	{0xd0, 0x80, 0x80, 0x100}, /* s10 */
};

static void ispv1_cal_capture_awb(
	awb_gain_t *preview_awb, awb_gain_t *flash_awb, awb_gain_t *capture_awb, 
	u16 preview_lum, u16 capflash_lum)
{
	u16 weight_flash = 0x100;
	u16 weight_preview = 0x100;

	weight_flash = weight_flash * (capflash_lum - preview_lum) / capflash_lum;
	weight_preview = weight_preview * preview_lum / capflash_lum;

	print_info("weight_flash 0x%x, weight_preview 0x%x", weight_flash, weight_preview);

	capture_awb->b_gain = (weight_flash * flash_awb->b_gain + weight_preview * preview_awb->b_gain) / 0x100;
	capture_awb->gb_gain = (weight_flash * flash_awb->gb_gain + weight_preview * preview_awb->gb_gain) / 0x100;
	capture_awb->gr_gain = (weight_flash * flash_awb->gr_gain + weight_preview * preview_awb->gr_gain) / 0x100;
	capture_awb->r_gain = (weight_flash * flash_awb->r_gain + weight_preview * preview_awb->r_gain) / 0x100;
}

void ispv1_get_wb_value(awb_gain_t *awb)
{
	GETREG16(MANUAL_AWB_GAIN_B, awb->b_gain);
	GETREG16(MANUAL_AWB_GAIN_GB, awb->gb_gain);
	GETREG16(MANUAL_AWB_GAIN_GR, awb->gr_gain);
	GETREG16(MANUAL_AWB_GAIN_R, awb->r_gain);
}

void ispv1_set_wb_value(awb_gain_t *awb)
{
	SETREG16(MANUAL_AWB_GAIN_B, awb->b_gain);
	SETREG16(MANUAL_AWB_GAIN_GB, awb->gb_gain);
	SETREG16(MANUAL_AWB_GAIN_GR, awb->gr_gain);
	SETREG16(MANUAL_AWB_GAIN_R, awb->r_gain);
}

static int ispv1_cal_capflash_lum(aec_data_t *preview_ae, aec_data_t *preflash_ae, u32 *preview_ratio_lum)
{
	u32 capflash_lum;
	u32 ratio_lum;
	u32 ratio = 0x100;

	ratio_lum = preview_ae->luminance;

	print_info("preview:0x%x,0x%x,0x%x; preflash:0x%x,0x%x,0x%x ",
		preview_ae->gain, preview_ae->expo, preview_ae->luminance,
		preflash_ae->gain, preflash_ae->expo, preflash_ae->luminance);

	ratio = ratio * (preflash_ae->gain * preflash_ae->expo) / (preview_ae->gain * preview_ae->expo);
	if (ratio != 0)
		ratio_lum = preview_ae->luminance * ratio / 0x100;

	if (preflash_ae->luminance > ratio_lum)
		capflash_lum = FLASH_CAP2PRE_RATIO * (preflash_ae->luminance - ratio_lum) + ratio_lum;
	else
		capflash_lum = ratio_lum;

	/* if calculated capture flash lum is zero, set it as 1 */
	if (capflash_lum == 0)
		capflash_lum = 1;

	*preview_ratio_lum = ratio_lum;
	return capflash_lum;
}

static void ispv1_poll_flash_lum(void)
{
	static u8 frame_count;
	static u8 frozen_frame;

	u32 volatile cur_luminance;
	awb_gain_t capture_awb;
	awb_gain_t *flash_awb;

	int boardid = 0;

	/* skip first 2 frames */
	if (++frame_count <= 2)
		return;

	cur_luminance = get_current_y();
	print_info("%s: flash off preview_y 0x%x, current_y 0x%x",
		__func__, isp_hw_data.preview_ae.luminance, cur_luminance);

	/* if frame_count larger than 12 and in FLASH_TESTING, should goto FLASH_FROZEN. */
	if ((this_ispdata->flash_flow == FLASH_TESTING)
		&& ((cur_luminance <= FLASH_TEST_OVER_EXPO) || (frame_count >= FLASH_TEST_MAX_COUNT))) {
		this_ispdata->flash_flow = FLASH_FROZEN;
		ispv1_set_aecagc_mode(MANUAL_AECAGC);
		ispv1_set_awb_mode(MANUAL_AWB);
	} else if (this_ispdata->flash_flow == FLASH_FROZEN) {
		if (++frozen_frame > 1) {
			isp_hw_data.preflash_ae.gain = get_writeback_gain();
			isp_hw_data.preflash_ae.expo = get_writeback_expo();
			isp_hw_data.preflash_ae.luminance = cur_luminance;
			isp_hw_data.capflash_luminance =
				ispv1_cal_capflash_lum(&isp_hw_data.preview_ae, &isp_hw_data.preflash_ae, &isp_hw_data.preview_ratio_lum);

			#ifdef PLATFORM_TYPE_PAD_S10
				flash_awb = &flash_platform_awb[FLASH_PLATFORM_S10];
			#else
				boardid = get_boardid();
				print_info("%s : boardid=0x%x.", __func__, boardid);


				/* if unknow board ID, should use flash params as X9510E */

				if ((boardid == BOARD_ID_CS_U9510E) || (boardid == BOARD_ID_CS_T9510E))
					flash_awb = &flash_platform_awb[FLASH_PLATFORM_9510E];
				else if (boardid == BOARD_ID_CS_U9510)
					flash_awb = &flash_platform_awb[FLASH_PLATFORM_U9510];
				else
					flash_awb = &flash_platform_awb[FLASH_PLATFORM_9510E];
			#endif

			ispv1_cal_capture_awb(&isp_hw_data.preview_awb, flash_awb, &capture_awb,
				isp_hw_data.preview_ratio_lum, isp_hw_data.capflash_luminance);

			ispv1_set_wb_value(&capture_awb);
			this_ispdata->flash_flow = FLASH_DONE;
			frozen_frame = 0;
			frame_count = 0;
		}
	}
}

void ispv1_preview_done_do_tune(void)
{
	camera_sensor *sensor;
	static u32 count;
	camera_auto_frame_rate direction;
	static k3_last_state last_state = {CAMERA_SATURATION_MAX, CAMERA_CONTRAST_MAX, CAMERA_BRIGHTNESS_MAX, CAMERA_EFFECT_MAX};
	u16 gain;
	int ret;

	if (NULL == this_ispdata) {
		return;
	}

	print_debug("preview_done, gain 0x%x, expo 0x%x, current_y 0x%x, flash_on %d",
		get_writeback_gain(), get_writeback_expo(), get_current_y(), this_ispdata->flash_on);

	if (false == this_ispdata->flash_on) {
		isp_hw_data.preview_ae.gain = get_writeback_gain();
		isp_hw_data.preview_ae.expo = get_writeback_expo();
		isp_hw_data.preview_ae.luminance = get_current_y();
		ispv1_get_wb_value(&isp_hw_data.preview_awb);
	}

	sensor = this_ispdata->sensor;
	if (CAMERA_USE_SENSORISP == sensor->isp_location) {
		print_debug("auto frame_rate only effect at k3 isp");
		return;
	}

	if ((FLASH_TESTING == this_ispdata->flash_flow) || (FLASH_FROZEN == this_ispdata->flash_flow)) {
		if ((FOCUS_STATE_CAF_RUNNING == afae_ctrl->focus_state) ||
		    (FOCUS_STATE_CAF_DETECTING == afae_ctrl->focus_state) ||
		    (FOCUS_STATE_AF_RUNNING == afae_ctrl->focus_state)) {
			print_debug("enter %s, must stop focus, before turn on preflash. ", __func__);
			ispv1_auto_focus(FOCUS_STOP);
		}
		ispv1_poll_flash_lum();
	} else if ((afae_ctrl->focus_state == FOCUS_STATE_AF_PREPARING) ||
		(afae_ctrl->focus_state == FOCUS_STATE_AF_RUNNING)) {
		print_debug("focusing metering, should not change frame rate.");
	} else {
		if (CAMERA_ISO_AUTO != this_ispdata->iso) {
			if (camera_get_frame_rate_level() != 0)
				ispv1_change_frame_rate(CAMERA_AUTO_FRAME_RATE_UP, sensor);
		} else {
			gain = get_writeback_gain();

			/*get gain from sensor */
			if (AUTO_FRAME_RATE_MAX_GAIN < gain) {
				direction = CAMERA_AUTO_FRAME_RATE_DOWN;
				count++;
			} else if (AUTO_FRAME_RATE_MIN_GAIN > gain) {
				direction = CAMERA_AUTO_FRAME_RATE_UP;
				count++;
			} else {
				direction = CAMERA_AUTO_FRAME_RATE_UNCHANGE;
				count = 0;
			}

			if (count >= AUTO_FRAME_RATE_TRIGER_COUNT) {

				if (GETREG8(REG_ISP_AECAGC_STABLE)) {
					ret = ispv1_change_frame_rate(direction, sensor);
					if (ret == 0)
						count = 0;
				}
			}
		}
	}

	if (true == camera_ajustments_flag) {
		last_state.saturation = CAMERA_SATURATION_MAX;
		last_state.contrast = CAMERA_CONTRAST_MAX;
		last_state.brightness = CAMERA_BRIGHTNESS_MAX;
		last_state.effect = CAMERA_EFFECT_MAX;
		camera_ajustments_flag = false;
	}

	/*camera_effect_saturation_done*/
	if ((this_ispdata->effect != last_state.effect)||(this_ispdata->saturation != last_state.saturation)) {
		ispv1_set_effect_saturation_done(this_ispdata->effect, this_ispdata->saturation);
		last_state.effect = this_ispdata->effect;
		last_state.saturation = this_ispdata->saturation;
	}

	/*contrast_done*/
	if (this_ispdata->contrast != last_state.contrast) {
		ispv1_set_contrast_done(this_ispdata->contrast);
		last_state.contrast = this_ispdata->contrast;
	}

	/*brightness_done*/
	if (this_ispdata->brightness != last_state.brightness) {
		ispv1_set_brightness_done(this_ispdata->brightness);
		last_state.brightness = this_ispdata->brightness;
	}

	ispv1_wakeup_focus_schedule(false);
}

/*
 * Used for tune ops and AF functions to get isp_data handler
 */
void ispv1_tune_ops_init(k3_isp_data *ispdata)
{
	this_ispdata = ispdata;

	if (this_ispdata->sensor->isp_location != CAMERA_USE_K3ISP)
		return;

	/* maybe some fixed configurations, such as focus, ccm, lensc... */
	ispv1_focus_init();
	ispv1_init_DPC(1, 1);

	ispv1_init_rawDNS(1);
	ispv1_init_uvDNS(1);
	ispv1_init_GbGrDNS(1);
	ispv1_init_RGBGamma(1);
	camera_ajustments_flag = true;

	/*y36721 2012-02-08 delete them for performance tunning.
	 *ispv1_init_CCM(ispdata->sensor->image_setting.ccm_param);
	 *ispv1_init_LENC(ispdata->sensor->image_setting.lensc_param);
	 *ispv1_init_AWB(ispdata->sensor->image_setting.awb_param);*/

	ispv1_init_sensor_config(ispdata->sensor);
}

/*
 * something need to do after camera exit
 */
void ispv1_tune_ops_exit(void)
{
	if (this_ispdata->sensor->af_enable == 1)
		ispv1_focus_exit();
}

/*
 * something need to do before start_preview and start_capture
 */
void ispv1_tune_ops_prepare(camera_state state)
{
	camera_sensor *sensor = this_ispdata->sensor;

	if (STATE_PREVIEW == state) {

		/* For AF update */
		if (this_ispdata->sensor->af_enable)
			ispv1_focus_prepare();

		if ((CAMERA_USE_SENSORISP == sensor->isp_location) &&
			(STATE_PREVIEW == state)) {
			ispv1_set_ae_statwin(&this_ispdata->pic_attr[state]);
		}

		/* need to check whether there is binning or not */
		ispv1_init_BC(1, 0, 0);

		this_ispdata->flash_flow = FLASH_DONE;
	} else if (STATE_CAPTURE == state) {
		/* we can add some other things to do before capture */
	}

	/* update lens correction scale size */
	ispv1_update_LENC_scale(this_ispdata->pic_attr[state].in_width,
				this_ispdata->pic_attr[state].in_height);
}

/*
 * something need to do before stop_preview and stop_capture
 */
void ispv1_tune_ops_withdraw(camera_state state)
{
	if (this_ispdata->sensor->af_enable)
		ispv1_focus_withdraw();
}
