/*
 * arch/arm/mach-tegra/eprj_generic/eprj_tegra_fan.c
 *
 * EternityProject FANs Interface
 * An interface to register FANs running on Tegra
 * based devices.
 *
 * Copyright (c) 2014, EternityProject Developers
 *
 * Angelo G. Del Regno <kholk11@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/thermal.h>
#include <linux/gpio.h>

#include <mach/gpio-tegra.h>

#include "../gpio-names.h"

/* Thermal constants (millicelsius) */
#define TJ_HIGH		55000
#define TJ_LOW		38000		/* Previous: 45000 */

/* FAN GPIOs */
#define OUYA_FAN_GPIO	TEGRA_GPIO_PJ2;

void eprj_fan_control(long temp)
{
	int fan_gpio = OUYA_FAN_GPIO;
#if 0
	if (machine_is_ouya())
		fan_gpio = OUYA_FAN_GPIO;
#endif

	if (temp > TJ_HIGH) {
		gpio_set_value(fan_gpio, 1);
		return;
	}
	if (temp < TJ_LOW) {
		gpio_set_value(fan_gpio, 0);
		return;
	}
}

/*
 * ToDo: This is the variable speed FAN implementation.
 *       Locate the regulator managing the power line
 *	 on the FAN and complete the code.
 *
 * Remember: Actually, we've badly hacked nct1008 driver
 *	     to get a basic FAN functionality for OUYA.
 *	     After we get this to work, remember to remove
 *	     the current hack!
 */
#if 0
static DEFINE_MUTEX(eprjfan_lock);
static int num_fans;
static struct eprj_fan_table fantable;

/*
 * Powers up and down the FAN.
 *
 * \return Returns 0 if no error.
 */
static int eprj_fans_set_power(bool ison)
{
}

/*
 * Get the current state of the FAN interface
 * and put it into the cur_state IN variable.
 *
 * \return Returns 0. No error handling required.
 */
static int
eprj_fans_get_cur_state(struct thermal_cooling_device *cdev,
			unsigned long *cur_state)
{
	struct balanced_throttle *bthrot = cdev->devdata;

	*cur_state = bthrot->cur_state;

	return 0;
}

/*
 * Get the maximum FAN interface state and
 * put it into the cur_state IN variable.
 *
 * Example: How much speeds we support?
 *
 * \return Returns 0. No error handling required.
 */
static int
eprj_fans_get_max_state(struct thermal_cooling_device *cdev,
			unsigned long *cur_state)
{
	struct balanced_throttle *bthrot = cdev->devdata;

	*max_state = bthrot->throt_tab_size;

	return 0;
}

static int
eprj_fans_set_cur_state(struct thermal_cooling_device *cdev,
			unsigned long *cur_state)
{
	struct balanced_throttle *bthrot = cdev->devdata;
	int index, direction;

	direction = bthrot->cur_state >= cur_state;
	bthrot->cur_state = cur_state;

	if (cur_state == 0)
		eprj_fans_set_power(false);
	else
		eprj_fans_set_power(true);

	return 0;
}
#endif

MODULE_AUTHOR("Angelo G. Del Regno - EternityProject Dev");
MODULE_DESCRIPTION("EternityProject FANs Interface");
MODULE_LICENSE("GPL");

