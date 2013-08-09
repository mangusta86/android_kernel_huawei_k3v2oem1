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

#ifndef K3_FB_PANEL_H
#define K3_FB_PANEL_H

#include <linux/regulator/consumer.h>
#include <linux/mux.h>
#include "clock.h"

#include "k3_fb_def.h"

/* panel type list */
#define NO_PANEL                       0xffff  /* No Panel */
#define LDI_PANEL                      1       /* internal LCDC type */
#define HDMI_PANEL                     2       /* HDMI TV */
#define PANEL_MIPI_VIDEO	3	/* MIPI */
#define PANEL_MIPI_CMD	4	/* MIPI */
#define DP_PANEL                       5       /* Display Port */


/* LCD init step */
enum {
	LCD_INIT_POWER_ON = 0,
	LCD_INIT_SEND_SEQUENCE,
};

typedef struct panel_id_s {
	u32 id;
	u32 type;
} panel_id_type;

struct ldi_panel_info {
	u32 h_back_porch;
	u32 h_front_porch;
	u32 h_pulse_width;
	u32 v_back_porch;
	u32 v_front_porch;
	u32 v_pulse_width;

	u8 hsync_plr;
	u8 vsync_plr;
	u8 pixelclk_plr;
	u8 data_en_plr;
	u8 disp_mode;
};

/* DSI PHY configuration */
struct mipi_dsi_phy_ctrl {
	u32 burst_mode;
	u32 lane_byte_clk;
	u32 clk_division;
	u32 lp2hs_time;
	u32 hs2lp_time;
	u32 hsfreqrange;
	u32 cp_current;
	u32 lpf_ctrl;
	u32 n_pll;
	u32 m_pll_1;
	u32 m_pll_2;
	u32 factors_effective;
};

struct mipi_panel_info {
	u8 vc;
	u8 lane_nums;
	u8 color_mode;
	u32 dsi_bit_clk;
	/*u32 dsi_pclk_rate;*/
};

struct sbl_panel_info{
	u32 bl_max;
	u32 cal_a;
	u32 str_limit;
};

struct k3_panel_info {
	u32 type;
	u32 xres;
	u32 yres;
	u8 bpp;
	u8 orientation;
	u8 s3d_frm;
	u8 bgr_fmt;
	u8 frame_rate;
	u8 bl_set_type;
	u32 clk_rate;
	volatile bool display_on;
	u8 lcd_init_step;

	u8 sbl_enable;
	u8 frc_enable;
	u8 esd_enable;

	struct ldi_panel_info ldi;
	struct mipi_panel_info mipi;
	struct sbl_panel_info sbl;

	u32 gpio_reset;
	u32 gpio_power;
	u32 gpio_lcd_id0;
	u32 gpio_lcd_id1;
	u32 gpio_lcd_te;
	struct regulator *lcdio_vcc;
	struct regulator *lcdanalog_vcc;
	struct iomux_block *lcd_block;
	struct block_config *lcd_block_config;

	u32 bl_max;
	u32 bl_min;
	u32 gpio_pwm0;
	u32 gpio_pwm1;
	u32 pwm_base;
	struct clk *pwm_clk;
	struct iomux_block *pwm_block;
	struct block_config *pwm_block_config;
};

struct k3_fb_data_type;

struct k3_fb_panel_data {
	struct k3_panel_info *panel_info;

	/* function entry chain */
	int (*on) (struct platform_device *pdev);
	int (*off) (struct platform_device *pdev);
	int (*remove) (struct platform_device *pdev);
	int (*set_backlight) (struct platform_device *pdev);
	int (*set_timing) (struct platform_device *pdev);
	int (*set_playvideo) (struct platform_device *pdev, int gamma);
	int (*set_fastboot) (struct platform_device *pdev);
	int (*set_frc) (struct platform_device *pdev, int target_fps);
	int (*set_cabc) (struct platform_device *pdev, int value);
	int (*check_esd) (struct platform_device *pdev);
	struct platform_device *next;
};


/*******************************************************************************
** FUNCTIONS PROTOTYPES
*/

struct platform_device *k3_fb_device_alloc(struct k3_fb_panel_data *pdata,
	u32 type, u32 id, u32 *graphic_ch);
void k3_fb_device_free(struct platform_device *pdev);

int panel_next_on(struct platform_device *pdev);
int panel_next_off(struct platform_device *pdev);
int panel_next_remove(struct platform_device *pdev);
int panel_next_set_backlight(struct platform_device *pdev);
int panel_next_set_timing(struct platform_device *pdev);
int panel_next_set_playvideo(struct platform_device *pdev, int gamma);
int panel_next_set_frc(struct platform_device *pdev, int target_fps);
int panel_next_check_esd(struct platform_device *pdev);
int pwm_set_backlight(int bl_lvl, struct k3_panel_info *pinfo);
u32 square_point_six(u32);

/******************************************************************************/

#define CLK_PWM0_NAME	"clk_pwm0"
#define CLK_MIPI_DPHY0_NAME	"clk_dphy0"
#define CLK_MIPI_DPHY1_NAME	"clk_dphy2"
#define CLK_LDI0_NAME	"clk_ldi0"
#define CLK_LDI1_NAME	"clk_ldi1"
#define CLK_EDC0_NAME	"clk_edc0"
#define CLK_EDC0_RST_NAME	"clk_edc0_rst"
#define CLK_EDC1_NAME	"clk_edc1"
#define CLK_G2D_NAME	"clk_g2d"

#define IRQ_LDI0_NAME	"irq_ldi0"
#define IRQ_LDI1_NAME	"irq_ldi1"
#define IRQ_EDC0_NAME	"irq_edc0"
#define IRQ_EDC1_NAME	"irq_edc1"

#define REG_BASE_PWM0_NAME	"reg_base_pwm0"
#define REG_BASE_EDC0_NAME	"reg_base_edc0"
#define REG_BASE_EDC1_NAME	"reg_base_edc1"

#define VCC_EDC1_NAME		"vcc_edc1"
#define VCC_LCDIO_NAME		"lcdio-vcc"
#define VCC_LCDANALOG_NAME	"lcdanalog-vcc"

#define GPIO_LCD_RESET_NAME	"gpio_lcd_reset"
#define GPIO_LCD_POWER_NAME	"gpio_lcd_power"
#define GPIO_LCD_PWM0_NAME	"gpio_pwm0"
#define GPIO_LCD_PWM1_NAME	"gpio_pwm1"
#define GPIO_LCD_ID0_NAME	"gpio_lcd_id0"
#define GPIO_LCD_ID1_NAME	"gpio_lcd_id1"
#define GPIO_LCD_TE_NAME	"gpio_lcd_te"

#define IOMUX_LCD_NAME	"block_lcd"
#define IOMUX_PWM_NAME	"block_pwm"

#define DEFAULT_PWM_CLK_RATE	(13 * 1000000)


#define LCD_VCC_GET(pdev, pinfo) \
	(pinfo)->lcdio_vcc = regulator_get(&(pdev)->dev, VCC_LCDIO_NAME); \
	if (IS_ERR((pinfo)->lcdio_vcc)) { \
		pr_err("k3fb, %s: failed to get lcdio-vcc regulator\n", __func__); \
		return PTR_ERR((pinfo)->lcdio_vcc); \
	} \
	(pinfo)->lcdanalog_vcc = regulator_get(&(pdev)->dev, VCC_LCDANALOG_NAME); \
	if (IS_ERR((pinfo)->lcdanalog_vcc)) { \
		pr_err("k3fb, %s: failed to get lcdanalog-vcc regulator\n", __func__); \
		return PTR_ERR((pinfo)->lcdanalog_vcc); \
	}

#define LCDIO_SET_VOLTAGE(pinfo, min_uV, max_uV) \
	if (regulator_set_voltage((pinfo)->lcdio_vcc, (min_uV), (max_uV)) != 0) { \
		pr_err("k3fb, %s: failed to set lcdio vcc.\n", __func__); \
	} \

#define LCD_VCC_PUT(pinfo) \
	if (!IS_ERR((pinfo)->lcdio_vcc)) { \
		regulator_put((pinfo)->lcdio_vcc); \
	} \
	if (!IS_ERR((pinfo)->lcdanalog_vcc)) { \
		regulator_put((pinfo)->lcdanalog_vcc); \
	}

#define LCD_VCC_ENABLE(pinfo)   \
	if (!IS_ERR((pinfo)->lcdio_vcc)) { \
		if (regulator_enable((pinfo)->lcdio_vcc) != 0) { \
			pr_err("k3fb, %s: failed to enable lcdio-vcc regulator.\n", __func__); \
		} \
	} \
	if (!IS_ERR((pinfo)->lcdanalog_vcc)) { \
		if (regulator_enable((pinfo)->lcdanalog_vcc) != 0) { \
			pr_err("k3fb, %s: failed to enable lcdanalog-vcc regulator.\n", __func__); \
		} \
	}

#define LCD_VCC_DISABLE(pinfo)  \
	if (!IS_ERR((pinfo)->lcdanalog_vcc)) { \
			if (regulator_disable((pinfo)->lcdanalog_vcc) != 0) { \
				pr_err("k3fb, %s: failed to disable lcdanalog-vcc regulator.\n", __func__); \
			} \
	} \
	if (!IS_ERR((pinfo)->lcdio_vcc)) { \
		if (regulator_disable((pinfo)->lcdio_vcc) != 0) { \
			pr_err("k3fb, %s: failed to disable lcdio-vcc regulator.\n", __func__); \
		} \
	}

#define LCD_IOMUX_GET(pinfo) \
	(pinfo)->lcd_block = iomux_get_block(IOMUX_LCD_NAME); \
	if (!(pinfo)->lcd_block) { \
		pr_err("k3fb, %s: failed to get iomux_lcd\n", __func__); \
		return PTR_ERR((pinfo)->lcd_block); \
	} \
	(pinfo)->lcd_block_config = iomux_get_blockconfig(IOMUX_LCD_NAME); \
	if (!(pinfo)->lcd_block_config) { \
		pr_err("k3fb, %s: failed to get iomux_lcd config\n", __func__); \
		return PTR_ERR((pinfo)->lcd_block_config); \
	}

#define LCD_IOMUX_SET(pinfo, mode) \
	if (blockmux_set((pinfo)->lcd_block, (pinfo)->lcd_block_config, (mode)) != 0) { \
		pr_err("k3fb, %s: failed to set iomux_lcd normal mode.\n", __func__); \
	}

#define LCD_RESOURCE(pdev, pinfo, res) \
	(res) = platform_get_resource_byname((pdev), IORESOURCE_IO, GPIO_LCD_RESET_NAME); \
	if (!(res)) { \
		pr_err("k3fb, %s: failed to get gpio reset resource.\n", __func__); \
		return -ENXIO; \
	} \
	(pinfo)->gpio_reset = (res)->start; \
	if (!gpio_is_valid((pinfo)->gpio_reset)) { \
		pr_err("k3fb, %s: failed to get gpio reset resource.\n", __func__); \
		return -ENXIO; \
	} \
	(res) = platform_get_resource_byname((pdev), IORESOURCE_IO, GPIO_LCD_POWER_NAME); \
	if (!(res)) { \
		pr_err("k3fb, %s: failed to get gpio power resource.\n", __func__); \
		return -ENXIO;\
	} \
	(pinfo)->gpio_power = (res)->start; \
	if (!gpio_is_valid((pinfo)->gpio_power)) { \
		pr_err("k3fb, %s: failed to get gpio power resource.\n", __func__); \
		return -ENXIO; \
	} \
	\
	(res) = platform_get_resource_byname((pdev), IORESOURCE_IO, GPIO_LCD_ID0_NAME); \
	if (!(res)) { \
		pr_err("k3fb, %s: failed to get gpio_lcd_id0 resource.\n", __func__); \
		return -ENXIO; \
	} \
	(pinfo)->gpio_lcd_id0 = (res)->start; \
	if (!(gpio_is_valid((pinfo)->gpio_lcd_id0))) { \
		pr_err("k3fb, %s: gpio_lcd_id0 is invalid.\n", __func__); \
		return -ENXIO;\
	} \
	(res) = platform_get_resource_byname((pdev), IORESOURCE_IO, GPIO_LCD_ID1_NAME); \
	if (!(res)) { \
		pr_err("k3fb, %s: failed to get gpio_lcd_id1 resource.\n", __func__); \
		return -ENXIO; \
	} \
	(pinfo)->gpio_lcd_id1 = (res)->start;\
	if (!(gpio_is_valid((pinfo)->gpio_lcd_id1))) { \
		pr_err("k3fb, %s: gpio_lcd_id1 is invalid.\n", __func__); \
		return -ENXIO; \
	}
#define LCD_GPIO_REQUEST(pinfo) \
	if (gpio_request((pinfo)->gpio_power, GPIO_LCD_POWER_NAME) != 0) { \
		pr_err("k3fb, %s: failed to request gpio power.\n", __func__); \
	} \
	if (gpio_request((pinfo)->gpio_reset, GPIO_LCD_RESET_NAME) != 0) { \
		pr_err("k3fb, %s:  failed to request gpio reset.\n", __func__); \
	} \
	if (gpio_request((pinfo)->gpio_lcd_id0, GPIO_LCD_ID0_NAME) != 0) { \
		pr_err("k3fb, %s: failed to request gpio_lcd_id0.\n", __func__); \
	} \
	if (gpio_request((pinfo)->gpio_lcd_id1, GPIO_LCD_ID1_NAME) != 0) { \
		pr_err("k3fb, %s: failed to request gpio_lcd_id1.\n", __func__); \
	}

#define LCD_GPIO_FREE(pinfo) \
	if (gpio_is_valid((pinfo)->gpio_reset)) { \
		gpio_free((pinfo)->gpio_reset); \
	} \
	if (gpio_is_valid((pinfo)->gpio_power)) { \
		gpio_free((pinfo)->gpio_power); \
	} \
	if (gpio_is_valid((pinfo)->gpio_lcd_id0)) { \
		gpio_free((pinfo)->gpio_lcd_id0); \
	} \
	if (gpio_is_valid((pinfo)->gpio_lcd_id1)) { \
		gpio_free((pinfo)->gpio_lcd_id1); \
	}

#define PWM_CLK_GET(pinfo) \
	(pinfo)->pwm_clk = clk_get(NULL, CLK_PWM0_NAME); \
	if (IS_ERR((pinfo)->pwm_clk)) { \
		pr_err("k3fb, %s: failed to get pwm0 clk.\n", __func__); \
		return PTR_ERR((pinfo)->pwm_clk); \
	} \
	if (clk_set_rate((pinfo)->pwm_clk, DEFAULT_PWM_CLK_RATE) != 0) { \
		pr_err("k3fb, %s: failed to set pwm clk rate.\n", __func__); \
	}


#define PWM_CLK_PUT(pinfo) \
	if (!IS_ERR((pinfo)->pwm_clk)) { \
		clk_put((pinfo)->pwm_clk); \
	}

#define PWM_IOMUX_GET(pinfo) \
	(pinfo)->pwm_block = iomux_get_block(IOMUX_PWM_NAME); \
	if (!(pinfo)->pwm_block) { \
		pr_err("k3fb, %s: failed to get iomux_pwm\n", __func__); \
		return PTR_ERR((pinfo)->pwm_block); \
	} \
	(pinfo)->pwm_block_config = iomux_get_blockconfig(IOMUX_PWM_NAME); \
	if (!(pinfo)->pwm_block_config) { \
		pr_err("k3fb, %s: failed to get iomux_pwm config\n", __func__); \
		return PTR_ERR((pinfo)->pwm_block_config); \
	}

#define PWM_IOMUX_SET(pinfo, mode) \
	if (blockmux_set((pinfo)->pwm_block, (pinfo)->pwm_block_config, (mode)) != 0) { \
		pr_err("k3fb, %s: failed to set iomux pwm normal mode.\n", __func__); \
	}

#define PWM_RESOUTCE(pdev, pinfo, res) \
	(res) = platform_get_resource_byname((pdev), IORESOURCE_MEM,  REG_BASE_PWM0_NAME); \
	if (!(res)) { \
		pr_err("k3fb, %s: failed to get pwm0 resource.\n", __func__); \
		return -ENXIO; \
	} \
	(pinfo)->pwm_base = (res)->start; \
	(res) = platform_get_resource_byname((pdev), IORESOURCE_IO, GPIO_LCD_PWM0_NAME); \
	if (!(res)) { \
	   pr_err("k3fb, %s: failed to get gpio pwm0 resource.\n", __func__); \
	   return -ENXIO; \
	} \
	(pinfo)->gpio_pwm0 = (res)->start; \
	if (!(gpio_is_valid((pinfo)->gpio_pwm0))) { \
	   pr_err("k3fb, %s: gpio pwm0 is invalid.\n", __func__); \
	   return -ENXIO; \
	} \
	(res) = platform_get_resource_byname((pdev), IORESOURCE_IO, GPIO_LCD_PWM1_NAME); \
	if (!(res)) { \
	   pr_err("k3fb, %s: failed to get gpio pwm1 resource.\n", __func__); \
	   return -ENXIO; \
	} \
	(pinfo)->gpio_pwm1 = (res)->start; \
	if (!(gpio_is_valid((pinfo)->gpio_pwm1))) { \
	   pr_err("k3fb, %s: gpio pwm1 is invalid.\n", __func__); \
	   return -ENXIO; \
	}

#define PWM_GPIO_REQUEST(pinfo) \
	if (gpio_request((pinfo)->gpio_pwm0, GPIO_LCD_PWM0_NAME) != 0) { \
		pr_err("k3fb, %s: failed to request gpio pwm0.\n", __func__); \
	} \
	if (gpio_request((pinfo)->gpio_pwm1, GPIO_LCD_PWM1_NAME) != 0) { \
		pr_err("k3fb, %s: failed to request gpio pwm1.\n", __func__); \
	}

#define PWM_GPIO_FREE(pinfo) \
	if (gpio_is_valid((pinfo)->gpio_pwm0)) { \
		gpio_free((pinfo)->gpio_pwm0); \
	} \
	if (gpio_is_valid((pinfo)->gpio_pwm1)) { \
		gpio_free((pinfo)->gpio_pwm1); \
	}

#define LCD_GET_CLK_RATE(pinfo)  0


#endif /* K3_FB_PANEL_H */
