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
#include <mach/boardid.h>

#include "k3_fb.h"
#include "k3_fb_def.h"
#include "mipi_dsi.h"
#include "mipi_reg.h"
#include <linux/lcd_tuning.h>
#include <hsad/config_interface.h>

#include <hsad/config_interface.h>

#define PWM_LEVEL 100

static bool g_display_on=false;
extern u8 g_k3fb_in_suspend;
extern u8 g_k3fb_in_resume;

/* LCD init step */
enum {
	CMI_LCD_INIT_POWER_ON = 0,
	CMI_LCD_INIT_SEND_SEQUENCE,
};

/*PWM IOMUX*/
#define IOMUX_PWM_NAME	"block_pwm"

static struct iomux_block **pwm_block;
static struct block_config **pwm_block_config;

static struct iomux_desc cmi_pwm_iomux_init_cmds[] = {
	{DTYPE_IOMUX_GET, IOMUX_PWM_NAME,
		(struct iomux_block **)&pwm_block, (struct block_config **)&pwm_block_config, 0},
};

static struct iomux_desc cmi_pwm_iomux_normal_cmds[] = {
	{DTYPE_IOMUX_SET, IOMUX_PWM_NAME,
		(struct iomux_block **)&pwm_block, (struct block_config **)&pwm_block_config, NORMAL},
};

static struct iomux_desc cmi_pwm_iomux_lowpower_cmds[] = {
	{DTYPE_IOMUX_SET, IOMUX_PWM_NAME,
		(struct iomux_block **)&pwm_block, (struct block_config **)&pwm_block_config, LOWPOWER},
};

/*LCD IOMUX*/
#define IOMUX_LCD_NAME	"block_lcd"

static struct iomux_block **lcd_block;
static struct block_config **lcd_block_config;

static struct iomux_desc cmi_lcd_iomux_init_cmds[] = {
	{DTYPE_IOMUX_GET, IOMUX_LCD_NAME,
		(struct iomux_block **)&lcd_block, (struct block_config **)&lcd_block_config, 0},
};

static struct iomux_desc cmi_lcd_iomux_normal_cmds[] = {
	{DTYPE_IOMUX_SET, IOMUX_LCD_NAME,
		(struct iomux_block **)&lcd_block, (struct block_config **)&lcd_block_config, NORMAL},
};

static struct iomux_desc cmi_lcd_iomux_lowpower_cmds[] = {
	{DTYPE_IOMUX_SET, IOMUX_LCD_NAME,
		(struct iomux_block **)&lcd_block, (struct block_config **)&lcd_block_config, LOWPOWER},
};

/*PWM GPIO*/
#define GPIO_LCD_PWM0_NAME	"gpio_pwm0"
#define GPIO_LCD_PWM1_NAME	"gpio_pwm1"

#define GPIO_LCD_PWM0	(142)
#define GPIO_LCD_PWM1	(143)

static struct gpio_desc cmi_pwm_gpio_request_cmds[] = {
	{DTYPE_GPIO_REQUEST, WAIT_TYPE_MS, 0,
		GPIO_LCD_PWM0_NAME, GPIO_LCD_PWM0, 0},
	{DTYPE_GPIO_REQUEST, WAIT_TYPE_MS, 0,
		GPIO_LCD_PWM1_NAME, GPIO_LCD_PWM1, 0},
};

static struct gpio_desc cmi_pwm_gpio_free_cmds[] = {
	{DTYPE_GPIO_FREE, WAIT_TYPE_MS, 0,
		GPIO_LCD_PWM0_NAME, GPIO_LCD_PWM0, 0},
	{DTYPE_GPIO_FREE, WAIT_TYPE_MS, 0,
		GPIO_LCD_PWM1_NAME, GPIO_LCD_PWM1, 0},
};

static struct gpio_desc cmi_pwm1_gpio_normal_cmds[] = {
	{DTYPE_GPIO_INPUT, WAIT_TYPE_MS, 1,
		GPIO_LCD_PWM1_NAME, GPIO_LCD_PWM1, 0},
};

static struct gpio_desc cmi_pwm2_gpio_normal_cmds[] = {
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 1,
		GPIO_LCD_PWM0_NAME, GPIO_LCD_PWM0, 0},
	{DTYPE_GPIO_INPUT, WAIT_TYPE_MS, 1,
		GPIO_LCD_PWM1_NAME, GPIO_LCD_PWM1, 0},
};

/*LCD GPIO*/
#define GPIO_LCD_RESET_NAME		"gpio_lcd_reset"
#define GPIO_LCD_POWER_NAME	"gpio_lcd_power"
#define GPIO_LCD_ID0_NAME	"gpio_lcd_id0"
#define GPIO_LCD_ID1_NAME	"gpio_lcd_id1"

#define GPIO_LCD_RESET  (003)
#define GPIO_LCD_POWER  (171)
#define GPIO_LCD_ID0	(151)
#define GPIO_LCD_ID1	(152)

static struct gpio_desc cmi_lcd_gpio_request_cmds[] = {
	{DTYPE_GPIO_REQUEST, WAIT_TYPE_MS, 0,
		GPIO_LCD_RESET_NAME, GPIO_LCD_RESET, 0},
	{DTYPE_GPIO_REQUEST, WAIT_TYPE_MS, 0,
		GPIO_LCD_POWER_NAME, GPIO_LCD_POWER, 0},
	{DTYPE_GPIO_REQUEST, WAIT_TYPE_MS, 0,
		GPIO_LCD_ID0_NAME, GPIO_LCD_ID0, 0},
	{DTYPE_GPIO_REQUEST, WAIT_TYPE_MS, 0,
		GPIO_LCD_ID1_NAME, GPIO_LCD_ID1, 0},
};

static struct gpio_desc cmi_lcd_gpio_free_cmds[] = {
	{DTYPE_GPIO_FREE, WAIT_TYPE_MS, 0,
		GPIO_LCD_RESET_NAME, GPIO_LCD_RESET, 0},
	{DTYPE_GPIO_FREE, WAIT_TYPE_MS, 0,
		GPIO_LCD_POWER_NAME, GPIO_LCD_POWER, 0},
	{DTYPE_GPIO_FREE, WAIT_TYPE_MS, 0,
		GPIO_LCD_ID0_NAME, GPIO_LCD_ID0, 0},
	{DTYPE_GPIO_FREE, WAIT_TYPE_MS, 0,
		GPIO_LCD_ID1_NAME, GPIO_LCD_ID1, 0},
};

static struct gpio_desc cmi_lcd0_gpio_normal_cmds[] = {
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 10,
		GPIO_LCD_RESET_NAME, GPIO_LCD_RESET, 1},
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 10,
		GPIO_LCD_POWER_NAME, GPIO_LCD_POWER, 1},
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 10,
		GPIO_LCD_RESET_NAME, GPIO_LCD_RESET, 0},
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 20,
		GPIO_LCD_RESET_NAME, GPIO_LCD_RESET, 1},
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 5,
		GPIO_LCD_RESET_NAME, GPIO_LCD_RESET, 0},
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 20,
		GPIO_LCD_RESET_NAME, GPIO_LCD_RESET, 1},
};

static struct gpio_desc cmi_lcd1_gpio_normal_cmds[] = {
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 5,
		GPIO_LCD_RESET_NAME, GPIO_LCD_RESET, 1},
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 10,
		GPIO_LCD_POWER_NAME, GPIO_LCD_POWER, 1},
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 10,
		GPIO_LCD_RESET_NAME, GPIO_LCD_RESET, 0},
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 20,
		GPIO_LCD_RESET_NAME, GPIO_LCD_RESET, 1},
};

static struct gpio_desc cmi_lcd2_gpio_normal_cmds[] = {
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 1,
		GPIO_LCD_POWER_NAME, GPIO_LCD_POWER, 0},
	{DTYPE_GPIO_OUTPUT, WAIT_TYPE_MS, 1,
		GPIO_LCD_RESET_NAME, GPIO_LCD_RESET, 0},
};

/*LCD VCC*/
#define VCC_LCDIO_NAME		"lcdio-vcc"

static struct regulator *vcc_lcdio;
static bool vcc_lcdio_status=false;

static struct vcc_desc cmi_lcd_vcc_init_cmds[] = {
	/* vcc get */
	{DTYPE_VCC_GET, VCC_LCDIO_NAME, &vcc_lcdio, 0, 0},
	/* vcc set voltage */
	{DTYPE_VCC_SET_VOLTAGE, VCC_LCDIO_NAME, &vcc_lcdio, 1800000, 1800000},
};

static struct vcc_desc cmi_lcd_vcc_setvoltage0_cmds[] = {
	/* vcc set voltage */
	{DTYPE_VCC_SET_VOLTAGE, VCC_LCDIO_NAME, &vcc_lcdio, 0, 1800000},
};

static struct vcc_desc cmi_lcd_vcc_put_cmds[] = {
	/* vcc put */
	{DTYPE_VCC_PUT, VCC_LCDIO_NAME, &vcc_lcdio, 0, 0},
};

static struct vcc_desc cmi_lcd_vcc_enable_cmds[] = {
	/* vcc set voltage */
	{DTYPE_VCC_SET_VOLTAGE, VCC_LCDIO_NAME, &vcc_lcdio, 1800000, 1800000},
	/* vcc enable */
	{DTYPE_VCC_ENABLE, VCC_LCDIO_NAME, &vcc_lcdio, 0, 0},
};

static struct vcc_desc cmi_lcd_vcc_disable_cmds[] = {
	/* vcc disable */
	{DTYPE_VCC_DISABLE, VCC_LCDIO_NAME, &vcc_lcdio, 0, 0},
};

/*PWM CLK*/
#define CLK_PWM0_NAME	"clk_pwm0"
#define DEFAULT_PWM_CLK_RATE	(13 * 1000000)

static struct clk *cmi_pwm_clk;


/*PWM RESOURCE*/
#define REG_BASE_PWM0_NAME  "reg_base_pwm0"
/*----------------Power ON Sequence(sleep mode to Normal mode)---------------------*/

static char bl_level_0[] = {
	0x51,
	0x00,
};

static char bl_level[] = {
	0x51,
	0x00,
};

static char te_enable[] = {
	0x35,
	0x00,
};


static char exit_sleep[] = {
	0x11,
};

static char normal_display_on[] = {
	0x13,
};

static char all_pixels_off[] = {
	0x22,
};

static char display_on[] = {
	0x29,
};

static char display_off[] = {
	0x28,
};

static char enter_sleep[] = {
	0x10,
};

/*enable orise mode*/
static char enable_orise_mode1[] = {
	0xFF,
	0x12, 0x82,0x01,
};

static char enable_orise_mode2[] = {
	0x00,
	0x80, 
};

static char enable_orise_mode3[] = {
	0xFF,
	0x12, 0x82, 
};

static char enable_orise_mode4[] = {
	0x00,
	0x80, 
};

/*Disable per-charge*/
static char disable_per_charge [] = {
	0xA5,
	0x0C, 0x04, 0x01, 
};


/* Set VGL*/
static char set_vgl1[] = {
	0x00,
	0xB0, 
};

static char set_vgl2[] = {
	0xC5,
	0x92, 0xD6,0xAF,0xAF,0x82,0x88,0x44,0x44,0x40,0x88,
};

/* Delay TE*/
static char Delay_TE[] = {
	0x44,
	0x00, 0x80,
};


static char  P_DRV_M1[] = {
	0x00,
	0xB3,
};

static char P_DRV_M2[] = {
	0xC0,
	0x33,
};

static char bl_PWM_CTRL1[] = {
	0x00,
	0xB0,
};

static char bl_PWM_CTRL2[] = {
	0xCA,
	0x02,0x02,0x5F,0x50,
};

static char bl_enable_noDimming[] = {
	0x53,
	0x24,
};

static char bl_PWM_CTRL3[] = {
	0xCA,
	0xE3,0xE3,0x5F,0x50,
};

static char orise_shift_0xb4[] = {
    0x00,
    0xb4,
};

static char display_address[] = {
    0xc0,
    0x10,
};

//#define RGV_INV_ENABLE

#ifdef RGV_INV_ENABLE
//RGB ·­×ªÉèÖÃ
static char Gen_write_RGB_INV_ON1[] = {
	0xff,
	0x12,
	0x82,
	0x01,
};

static char Gen_write_RGB_INV_ON2[] = {
	0x00,
	0x80,
};

static char Gen_write_RGB_INV_ON3[] = {
	0xff,
	0x12,
	0x82,
};

static char RGB_INV0[] = {
	0x00,
	0xB3,
};

static char RGB_INV1[] = {
	0xC0,
	0x66,
};

static char Gen_write_RGB_INV_OFF1[] = {
	0x00,
	0x80,
};

static char Gen_write_RGB_INV_OFF2[] = {
	0xff,
	0x00,
	0x00,
};

static char Gen_write_RGB_INV_OFF3[] = {
	0x00,
	0x00,
};

static char Gen_write_RGB_INV_OFF4[] = {
	0xff,
	0x00,
	0x00,
	0x00,
};


#endif

/*----------------CABC Base Sequence---------------------*/
static char enable_orise_mode5[] = {
	0x00,
	0x00,
};

/*----------------CABC UI Sequence---------------------*/
/*----------------CABC STILL Sequence---------------------*/
/*----------------CABC VID Sequence---------------------*/
/*----------------CE Sequence---------------------*/
static char CE_medium_on[] = {
	0x55,
	0x90,
};

static char enable_orise_mode14[] = {
	0x00, 0x00,
};

static char GVDD_setting[] = {
	0xd8,
	0x38, 0x38,
};

static char gamma22_Rp_setting[] = {
	0xe1,
	0x01,0x24,0x2d,0x3b,0x45,
	0x4c,0x5a,0x6c,0x78,0x88,
	0x93,0x9a,0x60,0x5b,0x56,
	0x4c,0x3c,0x2f,0x25,0x22,
	0x19,0x16,0x13,0x0F,
};

static char gamma22_Rn_setting[] = {
	0xe2,
	0x01,0x24,0x2d,0x3b,0x45,
	0x4c,0x5a,0x6c,0x78,0x88,
	0x93,0x9a,0x60,0x5b,0x56,
	0x4c,0x3c,0x2f,0x25,0x22,
	0x19,0x16,0x13,0x0F,
};

static char gamma22_Gp_setting[] = {
	0xe3,
	0x01,0x24,0x2d,0x3b,0x45,
	0x4c,0x5a,0x6c,0x78,0x88,
	0x93,0x9a,0x60,0x5b,0x56,
	0x4c,0x3c,0x2f,0x25,0x22,
	0x19,0x16,0x13,0x0F,
};

static char gamma22_Gn_setting[] = {
	0xe4,
	0x01,0x24,0x2d,0x3b,0x45,
	0x4c,0x5a,0x6c,0x78,0x88,
	0x93,0x9a,0x60,0x5b,0x56,
	0x4c,0x3c,0x2f,0x25,0x22,
	0x19,0x16,0x13,0x0F,
};

static char gamma22_Bp_setting[] = {
	0xe5,
	0x01,0x24,0x2d,0x3b,0x45,
	0x4c,0x5a,0x6c,0x78,0x88,
	0x93,0x9a,0x60,0x5b,0x56,
	0x4c,0x3c,0x2f,0x25,0x22,
	0x19,0x16,0x13,0x0F,
};

static char gamma22_Bn_setting[] = {
	0xe6,
	0x01,0x24,0x2d,0x3b,0x45,
	0x4c,0x5a,0x6c,0x78,0x88,
	0x93,0x9a,0x60,0x5b,0x56,
	0x4c,0x3c,0x2f,0x25,0x22,
	0x19,0x16,0x13,0x0F,
};

static char gamma23_Rp_setting[] = {
	0xe1,
    0x78,0x79,0x7b,0x7d,0x7f,
    0x81,0x86,0x8f,0x92,0x9a,
    0x9f,0xa3,0x5a,0x56,0x53,
    0x4a,0x3c,0x2f,0x25,0x22,
    0x19,0x16,0x13,0x0F,
};

static char gamma23_Rn_setting[] = {
	0xe2,
    0x78,0x79,0x7b,0x7d,0x7f,
    0x81,0x86,0x8f,0x92,0x9a,
    0x9f,0xa3,0x5a,0x56,0x53,
    0x4a,0x3c,0x2f,0x25,0x22,
    0x19,0x16,0x13,0x0F,    
};

static char gamma23_Gp_setting[] = {
	0xe3,
    0x64,0x66,0x68,0x6a,0x6f,
    0x74,0x7a,0x85,0x8a,0x95,
    0x9c,0xa1,0x5c,0x59,0x56,
    0x4c,0x3c,0x33,0x2a,0x25,
    0x22,0x16,0x13,0x0F,
};

static char gamma23_Gn_setting[] = {
	0xe4,
    0x64,0x66,0x68,0x6a,0x6f,
    0x74,0x7a,0x85,0x8a,0x95,
    0x9c,0xa1,0x5c,0x59,0x56,
    0x4c,0x3c,0x33,0x2a,0x25,
    0x22,0x16,0x13,0x0F,
};

static char gamma23_Bp_setting[] = {
	0xe5,
    0x01,0x24,0x2d,0x3b,0x45,
    0x4c,0x5a,0x6c,0x78,0x88,
    0x93,0x9a,0x60,0x5b,0x56,
    0x4c,0x3c,0x2f,0x25,0x22,
    0x19,0x16,0x13,0x0F,
};

static char gamma23_Bn_setting[] = {
	0xe6,
    0x01,0x24,0x2d,0x3b,0x45,
    0x4c,0x5a,0x6c,0x78,0x88,
    0x93,0x9a,0x60,0x5b,0x56,
    0x4c,0x3c,0x2f,0x25,0x22,
    0x19,0x16,0x13,0x0F,
};

static struct dsi_cmd_desc write_gamma23_cmds[] = {
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode14), enable_orise_mode14},		
	 {DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(gamma23_Rp_setting), gamma23_Rp_setting},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode14), enable_orise_mode14},		
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(gamma23_Rn_setting), gamma23_Rn_setting},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode14), enable_orise_mode14},		
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(gamma23_Gp_setting), gamma23_Gp_setting},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode14), enable_orise_mode14},		
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(gamma23_Gn_setting), gamma23_Gn_setting},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode14), enable_orise_mode14},		
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(gamma23_Bp_setting), gamma23_Bp_setting},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode14), enable_orise_mode14},		
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(gamma23_Bn_setting), gamma23_Bn_setting},
	
};

static char enable_orise_mode16[] = {
	0x00,
	0x00,
};

static char enable_orise_mode17[] = {
	0x00,
	0x04,
};

static struct dsi_cmd_desc read_IC_version_cmds1[] = {
       {DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode1), enable_orise_mode1},	
	{DTYPE_GEN_WRITE2, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode2), enable_orise_mode2},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode3), enable_orise_mode3},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode16), enable_orise_mode16},	
};

static struct dsi_cmd_desc set_scan_mode[]= {
    {DTYPE_GEN_WRITE2, 0, 100, WAIT_TYPE_US,
        sizeof(orise_shift_0xb4), orise_shift_0xb4},
    {DTYPE_GEN_WRITE2, 0, 100, WAIT_TYPE_US,
        sizeof(display_address), display_address},
};

static struct dsi_cmd_desc read_IC_version_cmds2[] = {
	{DTYPE_GEN_WRITE2, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode17), enable_orise_mode17},	
};

static char ce_init_param1[] = {
    0xD4,
    0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40,
    0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 
    0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40,
    0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 
    0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 
    0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40,
    0x00, 0x40, 0x00, 0x40, 0x00, 0x41, 0x00, 0x40, 0x00, 0x42, 
    0x00, 0x40, 0x00, 0x43, 0x00, 0x40, 0x00, 0x44, 0x00, 0x40, 
    0x00, 0x45, 0x00, 0x40, 0x00, 0x45, 0x00, 0x40, 0x00, 0x46,
    0x00, 0x40, 0x00, 0x47, 0x00, 0x40, 0x00, 0x48, 0x00, 0x40, 
    0x00, 0x49, 0x00, 0x40, 0x00, 0x4a, 0x00, 0x40, 0x00, 0x4b, 
    0x00, 0x40, 0x00, 0x4b, 0x00, 0x40, 0x00, 0x4c, 0x00, 0x40,
    0x00, 0x4d, 0x00, 0x40, 0x00, 0x4e, 0x00, 0x40, 0x00, 0x4e, 
    0x00, 0x40, 0x00, 0x4f, 0x00, 0x40, 0x00, 0x50, 0x00, 0x40, 
    0x00, 0x50, 0x00, 0x40, 0x00, 0x51, 0x00, 0x40, 0x00, 0x52,
    0x00, 0x40, 0x00, 0x52, 0x00, 0x40, 0x00, 0x53, 0x00, 0x40, 
    0x00, 0x54, 0x00, 0x40, 0x00, 0x54, 0x00, 0x40, 0x00, 0x55, 
    0x00, 0x40, 0x00, 0x55, 0x00, 0x40, 0x00, 0x56, 0x00, 0x40,
};

static char ce_init_param2[] = {
    0xD4, 
    0x00, 0x57, 0x00, 0x40, 0x00, 0x57, 0x00, 0x40, 0x00, 0x58, 
    0x00, 0x40, 0x00, 0x59, 0x00, 0x40, 0x00, 0x59, 0x00, 0x40,
    0x00, 0x5a, 0x00, 0x40, 0x00, 0x5b, 0x00, 0x40, 0x00, 0x5b, 
    0x00, 0x40, 0x00, 0x5c, 0x00, 0x40, 0x00, 0x5c, 0x00, 0x40, 
    0x00, 0x5d, 0x00, 0x40, 0x00, 0x5e, 0x00, 0x40, 0x00, 0x5e, 
    0x00, 0x40, 0x00, 0x5f, 0x00, 0x40, 0x00, 0x60, 0x00, 0x40, 
    0x00, 0x60, 0x00, 0x40, 0x00, 0x5f, 0x00, 0x40, 0x00, 0x5f, 
    0x00, 0x40, 0x00, 0x5e, 0x00, 0x40, 0x00, 0x5d, 0x00, 0x40, 
    0x00, 0x5d, 0x00, 0x40, 0x00, 0x5c, 0x00, 0x40, 0x00, 0x5c,
    0x00, 0x40, 0x00, 0x5b, 0x00, 0x40, 0x00, 0x5a, 0x00, 0x40, 
    0x00, 0x5a, 0x00, 0x40, 0x00, 0x59, 0x00, 0x40, 0x00, 0x58, 
    0x00, 0x40, 0x00, 0x58, 0x00, 0x40, 0x00, 0x57, 0x00, 0x40,
    0x00, 0x56, 0x00, 0x40, 0x00, 0x56, 0x00, 0x40, 0x00, 0x55, 
    0x00, 0x40, 0x00, 0x54, 0x00, 0x40, 0x00, 0x53, 0x00, 0x40, 
    0x00, 0x52, 0x00, 0x40, 0x00, 0x51, 0x00, 0x40, 0x00, 0x50,
    0x00, 0x40, 0x00, 0x4f, 0x00, 0x40, 0x00, 0x4f, 0x00, 0x40, 
    0x00, 0x4e, 0x00, 0x40, 0x00, 0x4d, 0x00, 0x40, 0x00, 0x4c, 
    0x00, 0x40, 0x00, 0x4b, 0x00, 0x40, 0x00, 0x4a, 0x00, 0x40,
};

static char ce_init_param3[] = {
    0xD4,
    0x00, 0x4a, 0x00, 0x40, 0x00, 0x4b, 0x00, 0x40, 0x00, 0x4c, 
    0x00, 0x40, 0x00, 0x4c, 0x00, 0x40, 0x00, 0x4d, 0x00, 0x40,
    0x00, 0x4e, 0x00, 0x40, 0x00, 0x4f, 0x00, 0x40, 0x00, 0x50,
    0x00, 0x40, 0x00, 0x51, 0x00, 0x40, 0x00, 0x52, 0x00, 0x40,
    0x00, 0x53, 0x00, 0x40, 0x00, 0x53, 0x00, 0x40, 0x00, 0x54,
    0x00, 0x40, 0x00, 0x55, 0x00, 0x40, 0x00, 0x56, 0x00, 0x40,
    0x00, 0x57, 0x00, 0x40, 0x00, 0x57, 0x00, 0x40, 0x00, 0x58,
    0x00, 0x40, 0x00, 0x59, 0x00, 0x40, 0x00, 0x59, 0x00, 0x40,
    0x00, 0x5a, 0x00, 0x40, 0x00, 0x5b, 0x00, 0x40, 0x00, 0x5b,
    0x00, 0x40, 0x00, 0x5c, 0x00, 0x40, 0x00, 0x5c, 0x00, 0x40,
    0x00, 0x5d, 0x00, 0x40, 0x00, 0x5e, 0x00, 0x40, 0x00, 0x5e,
    0x00, 0x40, 0x00, 0x5f, 0x00, 0x40, 0x00, 0x60, 0x00, 0x40,
    0x00, 0x60, 0x00, 0x40, 0x00, 0x5f, 0x00, 0x40, 0x00, 0x5f,
    0x00, 0x40, 0x00, 0x5e, 0x00, 0x40, 0x00, 0x5d, 0x00, 0x40,
    0x00, 0x5d, 0x00, 0x40, 0x00, 0x5c, 0x00, 0x40, 0x00, 0x5c,
    0x00, 0x40, 0x00, 0x5b, 0x00, 0x40, 0x00, 0x5a, 0x00, 0x40,
    0x00, 0x5a, 0x00, 0x40, 0x00, 0x59, 0x00, 0x40, 0x00, 0x58,
    0x00, 0x40, 0x00, 0x58, 0x00, 0x40, 0x00, 0x57, 0x00, 0x40,
};

static char ce_init_param4[] = {
    0xD4,
    0x00, 0x56, 0x00, 0x40, 0x00, 0x56, 0x00, 0x40, 0x00, 0x55,
    0x00, 0x40, 0x00, 0x54, 0x00, 0x40, 0x00, 0x53, 0x00, 0x40,
    0x00, 0x52, 0x00, 0x40, 0x00, 0x51, 0x00, 0x40, 0x00, 0x50,
    0x00, 0x40, 0x00, 0x4f, 0x00, 0x40, 0x00, 0x4f, 0x00, 0x40,
    0x00, 0x4e, 0x00, 0x40, 0x00, 0x4d, 0x00, 0x40, 0x00, 0x4c,
    0x00, 0x40, 0x00, 0x4b, 0x00, 0x40, 0x00, 0x4a, 0x00, 0x40,
    0x00, 0x4a, 0x00, 0x40, 0x00, 0x4a, 0x00, 0x40, 0x00, 0x4b,
    0x00, 0x40, 0x00, 0x4c, 0x00, 0x40, 0x00, 0x4c, 0x00, 0x40,
    0x00, 0x4d, 0x00, 0x40, 0x00, 0x4e, 0x00, 0x40, 0x00, 0x4e,
    0x00, 0x40, 0x00, 0x4f, 0x00, 0x40, 0x00, 0x50, 0x00, 0x40,
    0x00, 0x50, 0x00, 0x40, 0x00, 0x51, 0x00, 0x40, 0x00, 0x52,
    0x00, 0x40, 0x00, 0x52, 0x00, 0x40, 0x00, 0x53, 0x00, 0x40,
    0x00, 0x53, 0x00, 0x40, 0x00, 0x52, 0x00, 0x40, 0x00, 0x51,
    0x00, 0x40, 0x00, 0x4f, 0x00, 0x40, 0x00, 0x4e, 0x00, 0x40,
    0x00, 0x4d, 0x00, 0x40, 0x00, 0x4b, 0x00, 0x40, 0x00, 0x4a,
    0x00, 0x40, 0x00, 0x49, 0x00, 0x40, 0x00, 0x47, 0x00, 0x40,
    0x00, 0x46, 0x00, 0x40, 0x00, 0x45, 0x00, 0x40, 0x00, 0x44,
    0x00, 0x40, 0x00, 0x42, 0x00, 0x40, 0x00, 0x41, 0x00, 0x40,
};

static char ce_init_param5[] = {
    0xD5,
    0x00, 0x55, 0x00, 0x4b, 0x00, 0x54, 0x00, 0x4b, 0x00, 0x52,
    0x00, 0x4a, 0x00, 0x51, 0x00, 0x4a, 0x00, 0x4f, 0x00, 0x4a,
    0x00, 0x4e, 0x00, 0x49, 0x00, 0x4c, 0x00, 0x49, 0x00, 0x4b,
    0x00, 0x49, 0x00, 0x4a, 0x00, 0x49, 0x00, 0x48, 0x00, 0x48,
    0x00, 0x47, 0x00, 0x48, 0x00, 0x45, 0x00, 0x48, 0x00, 0x44,
    0x00, 0x47, 0x00, 0x43, 0x00, 0x47, 0x00, 0x41, 0x00, 0x47,
    0x00, 0x40, 0x00, 0x47, 0x00, 0x41, 0x00, 0x46, 0x00, 0x41,
    0x00, 0x46, 0x00, 0x42, 0x00, 0x46, 0x00, 0x43, 0x00, 0x46,
    0x00, 0x43, 0x00, 0x46, 0x00, 0x44, 0x00, 0x46, 0x00, 0x44,
    0x00, 0x46, 0x00, 0x45, 0x00, 0x45, 0x00, 0x45, 0x00, 0x45,
    0x00, 0x46, 0x00, 0x45, 0x00, 0x47, 0x00, 0x45, 0x00, 0x47,
    0x00, 0x45, 0x00, 0x48, 0x00, 0x45, 0x00, 0x48, 0x00, 0x44,
    0x00, 0x49, 0x00, 0x44, 0x00, 0x49, 0x00, 0x45, 0x00, 0x4a,
    0x00, 0x45, 0x00, 0x4a, 0x00, 0x45, 0x00, 0x4a, 0x00, 0x45,
    0x00, 0x4b, 0x00, 0x46, 0x00, 0x4b, 0x00, 0x46, 0x00, 0x4c,
    0x00, 0x46, 0x00, 0x4c, 0x00, 0x46, 0x00, 0x4d, 0x00, 0x46,
    0x00, 0x4d, 0x00, 0x47, 0x00, 0x4e, 0x00, 0x47, 0x00, 0x4e,
    0x00, 0x47, 0x00, 0x4e, 0x00, 0x47, 0x00, 0x4f, 0x00, 0x47,
};

static char ce_init_param6[] = {
    0xD5,
    0x00, 0x4f, 0x00, 0x48, 0x00, 0x50, 0x00, 0x48, 0x00, 0x50,
    0x00, 0x48, 0x00, 0x50, 0x00, 0x48, 0x00, 0x51, 0x00, 0x49,
    0x00, 0x51, 0x00, 0x49, 0x00, 0x52, 0x00, 0x49, 0x00, 0x52,
    0x00, 0x49, 0x00, 0x53, 0x00, 0x49, 0x00, 0x53, 0x00, 0x4a,
    0x00, 0x53, 0x00, 0x4a, 0x00, 0x54, 0x00, 0x4a, 0x00, 0x54,
    0x00, 0x4a, 0x00, 0x55, 0x00, 0x4a, 0x00, 0x55, 0x00, 0x4b,
    0x00, 0x55, 0x00, 0x4b, 0x00, 0x55, 0x00, 0x4b, 0x00, 0x54,
    0x00, 0x4a, 0x00, 0x54, 0x00, 0x4a, 0x00, 0x54, 0x00, 0x4a,
    0x00, 0x53, 0x00, 0x4a, 0x00, 0x53, 0x00, 0x4a, 0x00, 0x52,
    0x00, 0x49, 0x00, 0x52, 0x00, 0x49, 0x00, 0x52, 0x00, 0x49,
    0x00, 0x51, 0x00, 0x49, 0x00, 0x51, 0x00, 0x48, 0x00, 0x50,
    0x00, 0x48, 0x00, 0x50, 0x00, 0x48, 0x00, 0x4f, 0x00, 0x48,
    0x00, 0x4f, 0x00, 0x48, 0x00, 0x4e, 0x00, 0x47, 0x00, 0x4e,
    0x00, 0x47, 0x00, 0x4d, 0x00, 0x47, 0x00, 0x4d, 0x00, 0x46,
    0x00, 0x4c, 0x00, 0x46, 0x00, 0x4c, 0x00, 0x46, 0x00, 0x4b,
    0x00, 0x46, 0x00, 0x4a, 0x00, 0x45, 0x00, 0x4a, 0x00, 0x45,
    0x00, 0x49, 0x00, 0x45, 0x00, 0x49, 0x00, 0x44, 0x00, 0x48,
    0x00, 0x44, 0x00, 0x48, 0x00, 0x44, 0x00, 0x47, 0x00, 0x44,
};

static char ce_init_param7[] = {
    0xD5,
    0x00, 0x47, 0x00, 0x43, 0x00, 0x47, 0x00, 0x44, 0x00, 0x48,
    0x00, 0x44, 0x00, 0x48, 0x00, 0x44, 0x00, 0x49, 0x00, 0x45,
    0x00, 0x4a, 0x00, 0x45, 0x00, 0x4a, 0x00, 0x45, 0x00, 0x4b,
    0x00, 0x45, 0x00, 0x4b, 0x00, 0x46, 0x00, 0x4c, 0x00, 0x46,
    0x00, 0x4c, 0x00, 0x46, 0x00, 0x4d, 0x00, 0x47, 0x00, 0x4e,
    0x00, 0x47, 0x00, 0x4e, 0x00, 0x47, 0x00, 0x4f, 0x00, 0x47,
    0x00, 0x4f, 0x00, 0x48, 0x00, 0x50, 0x00, 0x48, 0x00, 0x50,
    0x00, 0x48, 0x00, 0x50, 0x00, 0x48, 0x00, 0x51, 0x00, 0x49,
    0x00, 0x51, 0x00, 0x49, 0x00, 0x52, 0x00, 0x49, 0x00, 0x52,
    0x00, 0x49, 0x00, 0x53, 0x00, 0x49, 0x00, 0x53, 0x00, 0x4a,
    0x00, 0x53, 0x00, 0x4a, 0x00, 0x54, 0x00, 0x4a, 0x00, 0x54,
    0x00, 0x4a, 0x00, 0x55, 0x00, 0x4a, 0x00, 0x55, 0x00, 0x4b,
    0x00, 0x55, 0x00, 0x4b, 0x00, 0x55, 0x00, 0x4b, 0x00, 0x54,
    0x00, 0x4a, 0x00, 0x54, 0x00, 0x4a, 0x00, 0x54, 0x00, 0x4a,
    0x00, 0x53, 0x00, 0x4a, 0x00, 0x53, 0x00, 0x4a, 0x00, 0x52,
    0x00, 0x49, 0x00, 0x52, 0x00, 0x49, 0x00, 0x52, 0x00, 0x49,
    0x00, 0x51, 0x00, 0x49, 0x00, 0x51, 0x00, 0x48, 0x00, 0x50,
    0x00, 0x48, 0x00, 0x50, 0x00, 0x48, 0x00, 0x4f, 0x00, 0x48,
};

static char ce_init_param8[] = {
    0xD5,
    0x00, 0x4f, 0x00, 0x48, 0x00, 0x4e, 0x00, 0x47, 0x00, 0x4e,
    0x00, 0x47, 0x00, 0x4d, 0x00, 0x47, 0x00, 0x4d, 0x00, 0x46,
    0x00, 0x4c, 0x00, 0x46, 0x00, 0x4c, 0x00, 0x46, 0x00, 0x4b,
    0x00, 0x46, 0x00, 0x4a, 0x00, 0x45, 0x00, 0x4a, 0x00, 0x45,
    0x00, 0x49, 0x00, 0x45, 0x00, 0x49, 0x00, 0x44, 0x00, 0x48,
    0x00, 0x44, 0x00, 0x48, 0x00, 0x44, 0x00, 0x47, 0x00, 0x44,
    0x00, 0x47, 0x00, 0x43, 0x00, 0x47, 0x00, 0x44, 0x00, 0x48,
    0x00, 0x44, 0x00, 0x48, 0x00, 0x44, 0x00, 0x48, 0x00, 0x44,
    0x00, 0x49, 0x00, 0x45, 0x00, 0x49, 0x00, 0x45, 0x00, 0x4a,
    0x00, 0x45, 0x00, 0x4a, 0x00, 0x45, 0x00, 0x4a, 0x00, 0x45,
    0x00, 0x4b, 0x00, 0x46, 0x00, 0x4b, 0x00, 0x46, 0x00, 0x4c,
    0x00, 0x46, 0x00, 0x4c, 0x00, 0x46, 0x00, 0x4d, 0x00, 0x46,
    0x00, 0x4d, 0x00, 0x47, 0x00, 0x4e, 0x00, 0x47, 0x00, 0x4e,
    0x00, 0x47, 0x00, 0x4f, 0x00, 0x48, 0x00, 0x4f, 0x00, 0x48,
    0x00, 0x50, 0x00, 0x48, 0x00, 0x50, 0x00, 0x48, 0x00, 0x51,
    0x00, 0x49, 0x00, 0x52, 0x00, 0x49, 0x00, 0x52, 0x00, 0x49,
    0x00, 0x53, 0x00, 0x4a, 0x00, 0x53, 0x00, 0x4a, 0x00, 0x54,
    0x00, 0x4a, 0x00, 0x54, 0x00, 0x4a, 0x00, 0x55, 0x00, 0x4b,
};

static struct dsi_cmd_desc jdi_video_on_v3_cmds[] = {
	{DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
		sizeof(bl_level), bl_level},

	{DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
		sizeof(bl_enable_noDimming), bl_enable_noDimming},

	{DTYPE_DCS_WRITE, 0, 100, WAIT_TYPE_US,
		sizeof(all_pixels_off), all_pixels_off},

	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(te_enable), te_enable},

	{DTYPE_DCS_WRITE, 0, 10, WAIT_TYPE_MS,
		sizeof(normal_display_on), normal_display_on},

       {DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode1), enable_orise_mode1},
	{DTYPE_GEN_WRITE2, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode2), enable_orise_mode2},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode3), enable_orise_mode3},

	{DTYPE_GEN_WRITE2, 0, 100, WAIT_TYPE_US,
		sizeof(enable_orise_mode5), enable_orise_mode5},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(ce_init_param1), ce_init_param1},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(ce_init_param2), ce_init_param2},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(ce_init_param3), ce_init_param3},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(ce_init_param4), ce_init_param4},
	{DTYPE_GEN_WRITE2, 0, 100, WAIT_TYPE_US,
		sizeof(enable_orise_mode5), enable_orise_mode5},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(ce_init_param5), ce_init_param5},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(ce_init_param6), ce_init_param6},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(ce_init_param7), ce_init_param7},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(ce_init_param8), ce_init_param8},
	{DTYPE_GEN_WRITE2, 0, 100, WAIT_TYPE_US,
		sizeof(enable_orise_mode5), enable_orise_mode5},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(CE_medium_on), CE_medium_on},

	{DTYPE_GEN_WRITE2, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode4), enable_orise_mode4},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(disable_per_charge), disable_per_charge},
	{DTYPE_GEN_WRITE2, 0, 200, WAIT_TYPE_US,
		sizeof(set_vgl1), set_vgl1},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(set_vgl2), set_vgl2},

	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(P_DRV_M1), P_DRV_M1},

	{DTYPE_DCS_LWRITE, 0, 100, WAIT_TYPE_US,
		sizeof(P_DRV_M2), P_DRV_M2},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(bl_PWM_CTRL1), bl_PWM_CTRL1},
	{DTYPE_DCS_LWRITE, 0, 100, WAIT_TYPE_US,
		sizeof(bl_PWM_CTRL3), bl_PWM_CTRL3},

	{DTYPE_DCS_WRITE, 0, 120, WAIT_TYPE_MS,
		sizeof(exit_sleep), exit_sleep},

	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode14), enable_orise_mode14},
	 {DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(GVDD_setting), GVDD_setting},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode14), enable_orise_mode14},		
	 {DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(gamma22_Rp_setting), gamma22_Rp_setting},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode14), enable_orise_mode14},		
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(gamma22_Rn_setting), gamma22_Rn_setting},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode14), enable_orise_mode14},		
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(gamma22_Gp_setting), gamma22_Gp_setting},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode14), enable_orise_mode14},		
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(gamma22_Gn_setting), gamma22_Gn_setting},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode14), enable_orise_mode14},		
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(gamma22_Bp_setting), gamma22_Bp_setting},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode14), enable_orise_mode14},		
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(gamma22_Bn_setting), gamma22_Bn_setting},		
       {DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(Delay_TE), Delay_TE},

	{DTYPE_DCS_WRITE, 0, 10, WAIT_TYPE_MS,
		sizeof(display_on), display_on},
};

static struct dsi_cmd_desc jdi_video_on_cmds[] = {
	{DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
		sizeof(bl_level), bl_level},

	{DTYPE_DCS_WRITE, 0, 100, WAIT_TYPE_US,
		sizeof(all_pixels_off), all_pixels_off},	

	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(te_enable), te_enable},

	{DTYPE_DCS_WRITE, 0, 10, WAIT_TYPE_MS,
		sizeof(normal_display_on), normal_display_on},	

       {DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode1), enable_orise_mode1},	
	{DTYPE_GEN_WRITE2, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode2), enable_orise_mode2},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode3), enable_orise_mode3},
	{DTYPE_GEN_WRITE2, 0, 100, WAIT_TYPE_US,
		sizeof(enable_orise_mode5), enable_orise_mode5},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(ce_init_param1), ce_init_param1},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(ce_init_param2), ce_init_param2},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(ce_init_param3), ce_init_param3},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(ce_init_param4), ce_init_param4},
	{DTYPE_GEN_WRITE2, 0, 100, WAIT_TYPE_US,
		sizeof(enable_orise_mode5), enable_orise_mode5},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(ce_init_param5), ce_init_param5},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(ce_init_param6), ce_init_param6},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(ce_init_param7), ce_init_param7},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(ce_init_param8), ce_init_param8},
	{DTYPE_GEN_WRITE2, 0, 100, WAIT_TYPE_US,
		sizeof(enable_orise_mode5), enable_orise_mode5},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(CE_medium_on), CE_medium_on},
	{DTYPE_GEN_WRITE2, 0, 200, WAIT_TYPE_US,
		sizeof(enable_orise_mode4), enable_orise_mode4},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(disable_per_charge), disable_per_charge},	
	{DTYPE_GEN_WRITE2, 0, 200, WAIT_TYPE_US,
		sizeof(set_vgl1), set_vgl1},
	{DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(set_vgl2), set_vgl2},

	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(P_DRV_M1), P_DRV_M1},
	{DTYPE_DCS_LWRITE, 0, 100, WAIT_TYPE_US,
		sizeof(P_DRV_M2), P_DRV_M2},
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(bl_PWM_CTRL1), bl_PWM_CTRL1},
	{DTYPE_DCS_LWRITE, 0, 100, WAIT_TYPE_US,
		sizeof(bl_PWM_CTRL2), bl_PWM_CTRL2},

	{DTYPE_DCS_WRITE, 0, 120, WAIT_TYPE_MS,
		sizeof(exit_sleep), exit_sleep},

#ifdef RGV_INV_ENABLE

	{DTYPE_DCS_WRITE, 0, 100, WAIT_TYPE_US,
		sizeof(Gen_write_RGB_INV_ON1), Gen_write_RGB_INV_ON1},	
	{DTYPE_DCS_WRITE, 0, 100, WAIT_TYPE_US,
		sizeof(Gen_write_RGB_INV_ON2), Gen_write_RGB_INV_ON2},		
	{DTYPE_DCS_WRITE, 0, 100, WAIT_TYPE_US,
		sizeof(Gen_write_RGB_INV_ON3), Gen_write_RGB_INV_ON3},	

	{DTYPE_DCS_WRITE, 0, 100, WAIT_TYPE_US,
		sizeof(RGB_INV0), RGB_INV0},	
		
	{DTYPE_DCS_WRITE, 0, 100, WAIT_TYPE_US,
		sizeof(RGB_INV1), RGB_INV1},	
	
	{DTYPE_DCS_WRITE, 0, 100, WAIT_TYPE_US,
		sizeof(Gen_write_RGB_INV_OFF1), Gen_write_RGB_INV_OFF1},

	{DTYPE_DCS_WRITE, 0, 100, WAIT_TYPE_US,
		sizeof(Gen_write_RGB_INV_OFF2), Gen_write_RGB_INV_OFF2},	

	{DTYPE_DCS_WRITE, 0, 100, WAIT_TYPE_US,
		sizeof(Gen_write_RGB_INV_OFF3), Gen_write_RGB_INV_OFF3},

	{DTYPE_DCS_WRITE, 0, 100, WAIT_TYPE_US,
		sizeof(Gen_write_RGB_INV_OFF4), Gen_write_RGB_INV_OFF4},	

#endif
       {DTYPE_GEN_LWRITE, 0, 200, WAIT_TYPE_US,
		sizeof(Delay_TE), Delay_TE},
	
	{DTYPE_DCS_WRITE, 0, 10, WAIT_TYPE_MS,
		sizeof(display_on), display_on},

	{DTYPE_DCS_WRITE1, 0, 10, WAIT_TYPE_US,
		sizeof(bl_enable_noDimming), bl_enable_noDimming},

};

static struct dsi_cmd_desc jdi_display_off_cmds[] = {
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(bl_level_0), bl_level_0},		
	{DTYPE_DCS_WRITE, 0, 100, WAIT_TYPE_US,
		sizeof(all_pixels_off), all_pixels_off},	
	{DTYPE_DCS_WRITE, 0, 30, WAIT_TYPE_MS,
		sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 0, 80, WAIT_TYPE_MS,
		sizeof(enter_sleep), enter_sleep}
};

static struct k3_fb_panel_data jdi_panel_data;

/******************************************************************************/

int cmi_pwm_clk_get(void)
{
	cmi_pwm_clk = clk_get(NULL, CLK_PWM0_NAME);
	if (IS_ERR(cmi_pwm_clk)) {
		k3fb_loge("failed to get pwm0 clk!\n");
		return -1;
	}
	
	if (clk_set_rate(cmi_pwm_clk, DEFAULT_PWM_CLK_RATE) != 0) {
		k3fb_loge("failed to set pwm clk rate!\n");
	}

	return 0;
}

void cmi_pwm_clk_put(void)
{
	if (!IS_ERR(cmi_pwm_clk)) {
		clk_put(cmi_pwm_clk);
	}
}

static struct lcd_tuning_dev *p_tuning_dev = NULL;

static ssize_t jdi_lcd_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct k3_panel_info *pinfo = NULL;

	pinfo = jdi_panel_data.panel_info;

	sprintf(buf, "Chimei 6.0' HD TFT %d x %d\n",
		pinfo->xres, pinfo->yres);

	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(lcd_info, S_IRUGO, jdi_lcd_info_show, NULL);

extern bool sbl_low_power_mode;

static ssize_t sbl_low_power_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	if(sbl_low_power_mode)
	{
		sprintf(buf, "sbl low power mode is enable.\n");
	}
	else
	{
		sprintf(buf, "sbl low power mode is disable.\n");
	}
	ret = strlen(buf) + 1;
	return ret;
}

static ssize_t sbl_low_power_mode_store(struct device *dev,
			     struct device_attribute *devattr,
			     const char *buf, size_t count)
{
	long m_sbl_low_power_mode = simple_strtol(buf, NULL, 10) != 0;
	sbl_low_power_mode =(bool)m_sbl_low_power_mode;
	return count;
}

static DEVICE_ATTR(sbl_low_power_mode, 0664,
	sbl_low_power_mode_show, sbl_low_power_mode_store);

static struct attribute *jdi_attrs[] = {
	&dev_attr_lcd_info,
	&dev_attr_sbl_low_power_mode,
	NULL,
};

static struct attribute_group jdi_attr_group = {
	.attrs = jdi_attrs,
};

static int jdi_sysfs_init(struct platform_device *pdev)
{
	int ret;
	ret = sysfs_create_group(&pdev->dev.kobj, &jdi_attr_group);
	if (ret) {
		k3fb_loge("create sysfs file failed!\n");
		return ret;
	}
	return 0;
}

static void jdi_sysfs_deinit(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &jdi_attr_group);
}

static int jdi_set_gamma(struct lcd_tuning_dev *ltd, enum lcd_gamma gamma)
{
	int ret = 0;
	struct platform_device *pdev = NULL;
	struct k3_fb_data_type *k3fd = NULL;
	u32 edc_base = 0;

	BUG_ON(ltd == NULL);
	pdev = (struct platform_device *)(ltd->data);
	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	edc_base = k3fd->edc_base;


	return ret;
}

static int jdi_set_cabc(struct lcd_tuning_dev *ltd, enum  tft_cabc cabc)
{
	int ret = 0;
	struct platform_device *pdev = NULL;
	struct k3_fb_data_type *k3fd = NULL;
	u32 edc_base = 0;

	BUG_ON(ltd == NULL);
	pdev = (struct platform_device *)(ltd->data);
	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	edc_base = k3fd->edc_base;
	/*switch (cabc) 
	{
		case CABC_UI:
			
			mipi_dsi_cmds_tx(jdi_cabc_ui_on_cmds, \
				ARRAY_SIZE(jdi_cabc_ui_on_cmds), edc_base);
			break;
		case CABC_VID:
		
			mipi_dsi_cmds_tx(jdi_cabc_vid_on_cmds, \
				ARRAY_SIZE(jdi_cabc_vid_on_cmds), edc_base);
			break;
		case CABC_OFF:
			break;
		default:
			ret = -1;
	}*/

	return ret;
}

static unsigned int g_csc_value[9];
static unsigned int g_is_csc_set;
static struct semaphore ct_sem;

static void jdi_store_ct_cscValue(unsigned int csc_value[])
{
    down(&ct_sem);
    g_csc_value [0] = csc_value[0];
    g_csc_value [1] = csc_value[1];
    g_csc_value [2] = csc_value[2];
    g_csc_value [3] = csc_value[3];
    g_csc_value [4] = csc_value[4];
    g_csc_value [5] = csc_value[5];
    g_csc_value [6] = csc_value[6];
    g_csc_value [7] = csc_value[7];
    g_csc_value [8] = csc_value[8];
    g_is_csc_set = 1;
    up(&ct_sem);
    
    return;
}

static int jdi_set_ct_cscValue(struct k3_fb_data_type *k3fd)
{
     u32 edc_base = 0;
    edc_base = k3fd->edc_base;
    down(&ct_sem);
    if(1 == g_is_csc_set)
    {
        set_reg(edc_base + 0x400, 0x1, 1, 27);

        set_reg(edc_base + 0x408, g_csc_value[0], 13, 0);
        set_reg(edc_base + 0x408, g_csc_value[1], 13, 16);
        set_reg(edc_base + 0x40C, g_csc_value[2], 13, 0);
        set_reg(edc_base + 0x40C, g_csc_value[3], 13, 16);
        set_reg(edc_base + 0x410, g_csc_value[4], 13, 0);
        set_reg(edc_base + 0x410, g_csc_value[5], 13, 16);
        set_reg(edc_base + 0x414, g_csc_value[6], 13, 0);
        set_reg(edc_base + 0x414, g_csc_value[7], 13, 16);
        set_reg(edc_base + 0x418, g_csc_value[8], 13, 0);
    }
    up(&ct_sem);


     return 0;
}

static int jdi_set_color_temperature(struct lcd_tuning_dev *ltd, unsigned int csc_value[])
{
    int flag = 0;
    struct platform_device *pdev;
    struct k3_fb_data_type *k3fd;

    if (ltd == NULL)
    {
        return -1;
    }
    pdev  = (struct platform_device *)(ltd->data);
    k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);

    if (k3fd == NULL)
    {
        return -1;
    }

    jdi_store_ct_cscValue(csc_value);
    flag = jdi_set_ct_cscValue(k3fd);
    return flag;
}

static struct lcd_tuning_ops sp_tuning_ops = {
	.set_gamma = jdi_set_gamma,
	.set_cabc = jdi_set_cabc,
	.set_color_temperature = jdi_set_color_temperature,
};


/*******************************************************************************/

static int jdi_pwm_on(struct k3_fb_data_type *k3fd)
{
        volatile bool disp_on=true;
	BUG_ON(k3fd == NULL);

	/* backlight on */
	/* pwm iomux normal */
	iomux_cmds_tx(cmi_pwm_iomux_normal_cmds, \
		ARRAY_SIZE(cmi_pwm_iomux_normal_cmds));
	/* pwm gpio request */
	gpio_cmds_tx(cmi_pwm_gpio_request_cmds, \
		ARRAY_SIZE(cmi_pwm_gpio_request_cmds));
	/* pwm gpio normal */
	gpio_cmds_tx(cmi_pwm1_gpio_normal_cmds, \
		ARRAY_SIZE(cmi_pwm1_gpio_normal_cmds));
	pwm_set_backlight(k3fd, &disp_on);

	return 0;
}

static int jdi_pwm_off(struct k3_fb_data_type *k3fd)
{
        volatile bool disp_on=false;
	BUG_ON(k3fd == NULL);

	pwm_set_backlight(k3fd, &disp_on);
	
	gpio_cmds_tx(cmi_pwm2_gpio_normal_cmds, \
		ARRAY_SIZE(cmi_pwm2_gpio_normal_cmds));
	
	/* pwm gpio free */
	gpio_cmds_tx(cmi_pwm_gpio_free_cmds, \
		ARRAY_SIZE(cmi_pwm_gpio_free_cmds));
	
	/* pwm iomux lower */
	iomux_cmds_tx(cmi_pwm_iomux_lowpower_cmds, \
		ARRAY_SIZE(cmi_pwm_iomux_lowpower_cmds));

	return 0;
}

static void jdi_disp_on(struct k3_fb_data_type *k3fd)
{
	u32 edc_base = 0;
	struct k3_panel_info *pinfo = NULL;
	int read_loop, cnt = 0;
	u32 scan_mode, temp =99;

	BUG_ON(k3fd == NULL);
	edc_base = k3fd->edc_base;
	pinfo = &(k3fd->panel_info);

	jdi_set_ct_cscValue(k3fd);

	iomux_cmds_tx(cmi_lcd_iomux_normal_cmds, \
		ARRAY_SIZE(cmi_lcd_iomux_normal_cmds));

	gpio_cmds_tx(cmi_lcd_gpio_request_cmds, \
		ARRAY_SIZE(cmi_lcd_gpio_request_cmds));

	if (gpio_request(GPIO_5_3, "bl_ctrl") != 0)
		k3fb_loge("failed to request gpio bl_ctrl!\n");
	if (gpio_direction_output(GPIO_5_3, 1) != 0)
		k3fb_loge("failed to request gpio bl_ctrl!\n");
        mdelay(10);

	if(1 == get_mate_new_lcd_type())
	{
		gpio_cmds_tx(cmi_lcd0_gpio_normal_cmds, \
			ARRAY_SIZE(cmi_lcd0_gpio_normal_cmds));

		mipi_dsi_cmds_tx(read_IC_version_cmds1, \
			ARRAY_SIZE(read_IC_version_cmds1), edc_base);

		outp32(edc_base+ MIPIDSI_GEN_HDR_OFFSET, (0xd1) << 8 | 0x14);
		mdelay(1);
		temp = inp32(edc_base + MIPIDSI_GEN_PLD_DATA_OFFSET);
		printk("ID2 =%d\n",temp);

		if(temp == 0x00)
		{
			mipi_dsi_cmds_tx(read_IC_version_cmds2, \
				ARRAY_SIZE(read_IC_version_cmds2), edc_base);

			outp32(edc_base+ MIPIDSI_GEN_HDR_OFFSET, (0xf8) << 8 | 0x14);
			mdelay(1);

			temp = inp32(edc_base + MIPIDSI_GEN_PLD_DATA_OFFSET);
			printk("ic =%d\n",temp);
			if(temp == 0x01)
			{
				mipi_dsi_cmds_tx(write_gamma23_cmds, \
					ARRAY_SIZE(write_gamma23_cmds), edc_base);
			}
		}
        
		mipi_dsi_cmds_tx(jdi_video_on_cmds, \
			ARRAY_SIZE(jdi_video_on_cmds), edc_base);

		for (read_loop = 0; read_loop < 3; read_loop++) {
			outp32(edc_base+ MIPIDSI_GEN_HDR_OFFSET, 0xb40023);
			udelay(200);
			outp32(edc_base+ MIPIDSI_GEN_HDR_OFFSET, (0xc0) << 8 | 0x06);
			udelay(200);
			scan_mode = inp32(edc_base + MIPIDSI_GEN_PLD_DATA_OFFSET);
			udelay(200);
			printk("\n\n-----scan_mode =0x%2x-----\n\n",scan_mode);
			if (scan_mode == 0x10)
				break;

			mipi_dsi_cmds_tx(set_scan_mode, \
				ARRAY_SIZE(set_scan_mode), edc_base);
		}
		if (read_loop >= 3)
			outp32(edc_base+ MIPIDSI_GEN_HDR_OFFSET, 0x005315);
	}
	else
	{
		while ( cnt <2 )
		{
			gpio_cmds_tx(cmi_lcd1_gpio_normal_cmds, \
				ARRAY_SIZE(cmi_lcd1_gpio_normal_cmds));
			mipi_dsi_cmds_tx(jdi_video_on_v3_cmds, \
				ARRAY_SIZE(jdi_video_on_v3_cmds), edc_base);
			cnt++;
		}
	}

/*	mipi_dsi_cmds_tx(jdi_cabc_cmds, \
		ARRAY_SIZE(jdi_cabc_cmds), edc_base);*/

	set_MIPIDSI_PCKHDL_CFG_en_eotp_tx(edc_base, 0x00000001);
	printk("\ndisplay on\n\n");
}

static void jdi_disp_off(struct k3_fb_data_type *k3fd)
{
	u32 edc_base = 0;
	struct k3_panel_info *pinfo = NULL;

	BUG_ON(k3fd == NULL);
	edc_base = k3fd->edc_base;
	pinfo = &(k3fd->panel_info);

	mipi_dsi_cmds_tx(jdi_display_off_cmds,
		ARRAY_SIZE(jdi_display_off_cmds), edc_base);
	gpio_cmds_tx(cmi_lcd2_gpio_normal_cmds, \
		ARRAY_SIZE(cmi_lcd2_gpio_normal_cmds));

	if (gpio_direction_output(GPIO_5_3, 0) != 0)
		k3fb_loge("failed to request gpio bl_ctrl!\n");
	mdelay(1);

	if (gpio_is_valid(GPIO_5_3)) {
		gpio_free(GPIO_5_3);
	}

	gpio_cmds_tx(cmi_lcd_gpio_free_cmds, \
		ARRAY_SIZE(cmi_lcd_gpio_free_cmds));
	iomux_cmds_tx(cmi_lcd_iomux_lowpower_cmds, \
		ARRAY_SIZE(cmi_lcd_iomux_lowpower_cmds));
	vcc_cmds_tx(k3fd->pdev, cmi_lcd_vcc_setvoltage0_cmds, \
        	ARRAY_SIZE(cmi_lcd_vcc_setvoltage0_cmds));
	if (true == vcc_lcdio_status){
		vcc_cmds_tx(k3fd->pdev, cmi_lcd_vcc_disable_cmds, \
			ARRAY_SIZE(cmi_lcd_vcc_disable_cmds));
		vcc_lcdio_status = false;
	}

	printk("\ndisplay off\n\n");
}

static int skip_esd_once = true;
static int backlight_log_once = true;

static int mipi_jdi_panel_on(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;
	struct k3_panel_info *pinfo = NULL;
	
	BUG_ON(pdev == NULL);
	
	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	pinfo = &(k3fd->panel_info);
	if (pinfo->lcd_init_step == LCD_INIT_POWER_ON) {
		if (false == vcc_lcdio_status){
			vcc_cmds_tx(pdev, cmi_lcd_vcc_enable_cmds, \
        			ARRAY_SIZE(cmi_lcd_vcc_enable_cmds));
			vcc_lcdio_status = true;
		}
		pinfo->lcd_init_step = LCD_INIT_MIPI_LP_SEND_SEQUENCE;

	}else if (pinfo->lcd_init_step == LCD_INIT_MIPI_LP_SEND_SEQUENCE){
		if (!g_display_on) {
			/* lcd display on */
			jdi_disp_on(k3fd);
	        	skip_esd_once = true;

			if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM) {
				/* backlight on */
				jdi_pwm_on(k3fd);
			}
		}
		pinfo->lcd_init_step = LCD_INIT_MIPI_HS_SEND_SEQUENCE;
	}else if (pinfo->lcd_init_step == LCD_INIT_MIPI_HS_SEND_SEQUENCE){
		g_display_on = true;
		backlight_log_once = true;
	}else {
		k3fb_loge("%s failed to init lcd!\n", __func__);
	}

	printk("%s end\n", __func__);
	return 0;
}


static int mipi_jdi_panel_off(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	if (g_display_on) {
		g_display_on = false;
		if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM) {
			/* backlight off */
			jdi_pwm_off(k3fd);
		}
		/* lcd display off */
		jdi_disp_off(k3fd);
		skip_esd_once = false;
	}

	return 0;
}

static int mipi_jdi_panel_remove(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	/*BUG_ON(k3fd == NULL);*/
	if (!k3fd) {
		return 0;
	}

	if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM) {
		cmi_pwm_clk_put();
	}
	vcc_cmds_tx(pdev, cmi_lcd_vcc_put_cmds, \
        	ARRAY_SIZE(cmi_lcd_vcc_put_cmds));
    
    jdi_sysfs_deinit(pdev);

	return 0;
}

static int mipi_jdi_panel_set_backlight(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;
	u32 edc_base = 0;
	u32 level = 0;

	char bl_level_adjust[2] = {
		0x51,
		0x00,
	};

       struct dsi_cmd_desc  jdi_bl_level_adjust[] = {
	{DTYPE_DCS_WRITE1, 0, 100, WAIT_TYPE_US,
		sizeof(bl_level_adjust), bl_level_adjust},		
	};

	BUG_ON(pdev == NULL);
	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);
	edc_base = k3fd->edc_base;

	/*Our eyes are more sensitive to small brightness.
	So we adjust the brightness of lcd following iphone4 */
	level = k3fd->bl_level;

	if (level > 248)
	{
		level = 248;
	}


	//backlight may turn off when bl_level is below 6.
	if (level < 6 && level != 0)
	{
		level = 6;
	}

	if (level >= 28 && level <= 34)
	{
		level = 35;
	}

	bl_level_adjust[1] = level;

	if (backlight_log_once) {
		backlight_log_once = false;
		k3fb_loge("----k3fd->bl_level=%d,set backlight to level = %d\n",k3fd->bl_level, level);
	}

	mipi_dsi_cmds_tx(jdi_bl_level_adjust, \
		ARRAY_SIZE(jdi_bl_level_adjust), edc_base);

	return 0;
}



static int mipi_jdi_panel_set_fastboot(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);

	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	if (false == vcc_lcdio_status){
		vcc_cmds_tx(pdev, cmi_lcd_vcc_enable_cmds, \
        		ARRAY_SIZE(cmi_lcd_vcc_enable_cmds));
		vcc_lcdio_status = true;
	}
	iomux_cmds_tx(cmi_lcd_iomux_normal_cmds, \
		ARRAY_SIZE(cmi_lcd_iomux_normal_cmds));
	
	gpio_cmds_tx(cmi_lcd_gpio_request_cmds, \
		ARRAY_SIZE(cmi_lcd_gpio_request_cmds));

	if (gpio_request(GPIO_5_3, "bl_ctrl") != 0)
		k3fb_loge("failed to request gpio bl_ctrl!\n");

	if (k3fd->panel_info.bl_set_type & BL_SET_BY_PWM) {
		/* pwm iomux normal */
		iomux_cmds_tx(cmi_pwm_iomux_normal_cmds, \
			ARRAY_SIZE(cmi_pwm_iomux_normal_cmds));
		/* pwm gpio request */
		gpio_cmds_tx(cmi_pwm_gpio_request_cmds, \
			ARRAY_SIZE(cmi_pwm_gpio_request_cmds));
	}

	g_display_on = true;

	return 0;
}

static int mipi_jdi_panel_set_cabc(struct platform_device *pdev, int value)
{
	u32 edc_base = 0;
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);
	edc_base = k3fd->edc_base;

#if 0
	if (value) {
		outp32(edc_base + MIPIDSI_GEN_HDR_OFFSET, 0x0dbb23);
	} else {
		outp32(edc_base + MIPIDSI_GEN_HDR_OFFSET, 0x0cbb23);
	}
#endif

	return 0;
}

static int mipi_jdi_panel_check_esd(struct platform_device *pdev)
{
	struct k3_fb_data_type *k3fd = NULL;

	BUG_ON(pdev == NULL);
	k3fd = (struct k3_fb_data_type *)platform_get_drvdata(pdev);
	BUG_ON(k3fd == NULL);

	/* read pwm */
    if (skip_esd_once) {
        skip_esd_once = false;
        outp32(k3fd->edc_base + MIPIDSI_GEN_HDR_OFFSET, 0xAC << 8 | 0x06);
        inp32(k3fd->edc_base + MIPIDSI_GEN_PLD_DATA_OFFSET);
        return 0;
    }

    outp32(k3fd->edc_base + MIPIDSI_GEN_HDR_OFFSET, 0xAC << 8 | 0x06);
	return  inp32(k3fd->edc_base + MIPIDSI_GEN_PLD_DATA_OFFSET);
}

static struct k3_panel_info jdi_panel_info = {0};
static struct k3_fb_panel_data jdi_panel_data = {
	.panel_info = &jdi_panel_info,
	.on = mipi_jdi_panel_on,
	.off = mipi_jdi_panel_off,
	.remove = mipi_jdi_panel_remove,
	.set_backlight = mipi_jdi_panel_set_backlight,
	.set_fastboot = mipi_jdi_panel_set_fastboot,
	.set_cabc = mipi_jdi_panel_set_cabc,
	.check_esd = mipi_jdi_panel_check_esd,
};

static int __devinit jdi_probe(struct platform_device *pdev)
{
    struct k3_panel_info *pinfo = NULL;
    //struct resource *res = NULL;
    struct platform_device *reg_pdev;
    struct lcd_tuning_dev *ltd;
    struct lcd_properities lcd_props;

    pinfo = jdi_panel_data.panel_info;
    /* init lcd panel info */
    g_display_on = false;
    pinfo->xres = 720;
    pinfo->yres = 1280;
    pinfo->width =58;
    pinfo->height =103;
    pinfo->type = PANEL_MIPI_CMD;
    pinfo->orientation = LCD_PORTRAIT;
    pinfo->bpp = EDC_OUT_RGB_888;
    pinfo->s3d_frm = EDC_FRM_FMT_2D;
    pinfo->bgr_fmt = EDC_RGB;
    pinfo->bl_set_type = BL_SET_BY_MIPI;
    pinfo->bl_max = PWM_LEVEL;
    pinfo->bl_min = 1;

    pinfo->frc_enable = 1;
    //if it is factorymode disable ESD,else if it is normalmode enable ESD
    if(NULL != strstr(saved_command_line, "androidboot.swtype=factory"))
        pinfo->esd_enable = 0;
    else
        pinfo->esd_enable = 1;

    printk("esd_enable =%d\n", pinfo->esd_enable);
    pinfo->sbl_enable = 1;
    pinfo->sbl.bl_max = 0xff;
    pinfo->sbl.cal_a = 0x08;
    pinfo->sbl.cal_b = 0xd8;
    pinfo->sbl.str_limit = 0x40;

    pinfo->ldi.h_back_porch = 43;
    pinfo->ldi.h_front_porch = 97;
    pinfo->ldi.h_pulse_width = 57;
    pinfo->ldi.v_back_porch = 12;
    pinfo->ldi.v_front_porch = 14;
    pinfo->ldi.v_pulse_width = 2;

    pinfo->ldi.hsync_plr = 1;
    pinfo->ldi.vsync_plr = 0;
    pinfo->ldi.pixelclk_plr = 1;
    pinfo->ldi.data_en_plr = 0;

    pinfo->ldi.disp_mode = LDI_DISP_MODE_NOT_3D_FBF;

    /* Note: must init here */
    pinfo->frame_rate = 60;
    pinfo->clk_rate = 76000000;

    pinfo->mipi.lane_nums = DSI_4_LANES;
    pinfo->mipi.color_mode = DSI_24BITS_1;
    pinfo->mipi.vc = 0;
    pinfo->mipi.dsi_bit_clk = 241;

    /* lcd vcc init */
    vcc_cmds_tx(pdev, cmi_lcd_vcc_init_cmds, \
        ARRAY_SIZE(cmi_lcd_vcc_init_cmds));
    /* lcd iomux init */
    iomux_cmds_tx(cmi_lcd_iomux_init_cmds, \
        ARRAY_SIZE(cmi_lcd_iomux_init_cmds));

    if (pinfo->bl_set_type & BL_SET_BY_PWM) {
        /* pwm clock*/
        cmi_pwm_clk_get();
        /* pwm iomux init */
	iomux_cmds_tx(cmi_pwm_iomux_init_cmds, \
		ARRAY_SIZE(cmi_pwm_iomux_init_cmds));
    }
    /* alloc panel device data */
    if (platform_device_add_data(pdev, &jdi_panel_data,
        sizeof(struct k3_fb_panel_data))) {
        k3fb_loge("platform_device_add_data failed!\n");
        platform_device_put(pdev);
        return -ENOMEM;
    }
    reg_pdev = k3_fb_add_device(pdev);

    sema_init(&ct_sem, 1);
    g_csc_value[0] = 0;
    g_csc_value[1] = 0;
    g_csc_value[2] = 0;
    g_csc_value[3] = 0;
    g_csc_value[4] = 0;
    g_csc_value[5] = 0;
    g_csc_value[6] = 0;
    g_csc_value[7] = 0;
    g_csc_value[8] = 0;
    g_is_csc_set = 0;
    /* for cabc */
    lcd_props.type = TFT;
    lcd_props.default_gamma = GAMMA25;
    ltd = lcd_tuning_dev_register(&lcd_props, &sp_tuning_ops, (void *)reg_pdev);
    p_tuning_dev=ltd;
    if (IS_ERR(ltd)) {
        k3fb_loge("lcd_tuning_dev_register failed!\n");
        return -1;
    }
    jdi_sysfs_init(pdev);
    return 0;
}

static struct platform_driver this_driver = {
	.probe = jdi_probe,
	.remove = NULL,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.driver = {
		.name = "mipi_cmi_OTM1282B",
	},
};

static int __init mipi_jdi_panel_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&this_driver);
	if (ret) {
		k3fb_loge("not able to register the driver\n");
		return ret;
	}

	return ret;
}

module_init(mipi_jdi_panel_init);
