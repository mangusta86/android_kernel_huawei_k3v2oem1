/* Copyright (c) 2008-2010, Hisilicon Tech. Co., Ltd. All rights reserved.
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
#include <linux/mutex.h>
#include <linux/leds.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/pwm.h>
#include <linux/clk.h>
#include <mach/hardware.h>

#include "k3_fb.h"
#include "k3_fb_def.h"


#define WAIT_NUMBER			(1000)
#define WAIT_TIMERLEN		(5)

#define BACKLIGHT_ENABLE	(1)
#define BACKLIGHT_DISABLE	(0)

#define PWM_DIV_OFFSET		(0x8)
#define PWM_OUT_OFFSET		(0x10)

#define PWM_MAX_DIV			(0xE0)


static DEFINE_MUTEX(k3led_backlight_lock);

static int bklclk_cnt;

int pwm_set_backlight(int bl_lvl, struct k3_panel_info *pinfo)
{
	int ret = 0;
	int i = 0;
	u32 brightness = 0;
	volatile u32 base = IO_ADDRESS(pinfo->pwm_base);

	BUG_ON(pinfo == NULL);

	mutex_lock(&k3led_backlight_lock);

	brightness = (bl_lvl * PWM_MAX_DIV) / pinfo->bl_max;
	if (!bklclk_cnt++) {
		ret = clk_enable(pinfo->pwm_clk);
		if (ret != 0) {
			pr_err("k3fb, %s: backlight failed to enable pwm_clk! \n", __func__);
			mutex_unlock(&k3led_backlight_lock);
			return ret;
		}
	}

	if (brightness == 0) {
		outp32(base + PWM_OUT_OFFSET, brightness);
		outp32(base, BACKLIGHT_DISABLE);
		clk_disable(pinfo->pwm_clk);
		bklclk_cnt = 0;
	} else {
		/*wait for display on*/
		for (i = 0; i < WAIT_NUMBER; i++) {
			if (pinfo->display_on  == false) {
				msleep(WAIT_TIMERLEN);
				continue;
			} else {
				break;
			}
		}

		if (i >= WAIT_NUMBER) {
			pr_err("k3fb, %s, backlight is time out!\n", __func__);
		}

		outp32(base, BACKLIGHT_ENABLE);
		outp32(base + PWM_DIV_OFFSET, PWM_MAX_DIV);
		outp32(base + PWM_OUT_OFFSET, brightness);
	}

	mutex_unlock(&k3led_backlight_lock);

	return 0;
}
