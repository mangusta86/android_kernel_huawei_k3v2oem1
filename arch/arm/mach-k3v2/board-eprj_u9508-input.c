/*
 * arch/arm/mach-k3v2/board-eprj_u9508-input.c
 *
 * Board file for Huawei U9508 machine -- Input devices
 *
 * Copyright (c) 2014, EternityProject Development. All rights reserved.
 * Copyright (c) 2014, Angelo G. Del Regno - EternityProject TeaM.
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
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/rmi.h>
#include <linux/input.h>

#include <mach/gpio.h>

/* Synaptics RMI4 */
#define TOUCH_INT_PIN	GPIO_19_5  /*GPIO_157*/
#define TOUCH_RESET_PIN	GPIO_19_4
#define ENABLE_GPIO	GPIO_7_5 /*GPIO_61*/
#define SYNA_NAME	"rmi_i2c"

static char	buf_virtualkey[500];
static ssize_t  buf_vkey_size = 0;

static ssize_t synaptics_virtual_keys_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
		memcpy( buf, buf_virtualkey, buf_vkey_size );
		return buf_vkey_size;
}

static struct kobj_attribute synaptics_virtual_keys_attr = {
	.attr = {
		.name = "virtualkeys.synaptics",
		.mode = S_IRUGO,
	},
	.show = &synaptics_virtual_keys_show,
};

static struct attribute *synaptics_properties_attrs[] = {
	&synaptics_virtual_keys_attr.attr,
	NULL
};

static struct attribute_group synaptics_properties_attr_group = {
	.attrs = synaptics_properties_attrs,
};

static void __init synaptics_virtual_keys_init(void)
{
	struct kobject *properties_kobj;
	int ret = 0;

	buf_vkey_size = sprintf(buf_virtualkey,
		    __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":120:1380:160:150"
		":" __stringify(EV_KEY) ":" __stringify(KEY_HOMEPAGE) ":365:1380:160:150"
		":" __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":605:1380:160:150"
		"\n");

	properties_kobj = kobject_create_and_add("board_properties", NULL);
	if (properties_kobj)
		ret = sysfs_create_group(properties_kobj,
			&synaptics_properties_attr_group);
	if (!properties_kobj || ret)
		pr_err("%s: failed to create board_properties!\n", __func__);
}

#ifdef CONFIG_TOUCHSCREEN_RMI4_SYNAPTICS_GENERIC
struct syna_gpio_data {
	u16 attn_gpio_number;
	u16 reset_gpio_number;
	char* attn_gpio_name;
	char* reset_gpio_name;
};

static int synaptics_touchpad_gpio_setup(void *gpio_data, bool configure)
{
	int retval = 0;
	struct syna_gpio_data *data = gpio_data;

	if (configure) {
		retval = gpio_request(data->attn_gpio_number, "rmi4_attn");
		if (retval) {
			pr_err("%s: Failed to get attn gpio %d. Code: %d.",
				   __func__, data->attn_gpio_number, retval);
			return retval;
		}
		retval = gpio_direction_input(data->attn_gpio_number);
		if (retval) {
			pr_err("%s: Failed to setup attn gpio %d. Code: %d.",
				   __func__, data->attn_gpio_number, retval);
			gpio_free(data->attn_gpio_number);
		}
		retval = gpio_request(data->reset_gpio_number, "rmi4_reset");
		if (retval) {
			pr_err("%s: Failed to get reset gpio %d. Code: %d.",
				   __func__, data->reset_gpio_number, retval);
			return retval;
		}
		retval = gpio_direction_output(data->reset_gpio_number,0);
		if (retval) {
			pr_err("%s: Failed to setup reset gpio %d. Code: %d.",
				   __func__, data->reset_gpio_number, retval);
			gpio_free(data->reset_gpio_number);
		}
		msleep (10);
		retval = gpio_direction_output(data->reset_gpio_number,1);
		if (retval) {
			pr_err("%s: Failed to setup reset gpio %d. Code: %d.",
				   __func__, data->reset_gpio_number, retval);
			gpio_free(data->reset_gpio_number);
		}
		msleep(10);
		gpio_free(data->reset_gpio_number);
	} else {
		pr_warn("%s: No way to deconfigure gpio %d and %d.",
			   __func__, data->attn_gpio_number,data->reset_gpio_number);
	}
	return retval;
};

static struct syna_gpio_data rmi4_gpiodata = {
	.attn_gpio_number = TOUCH_INT_PIN,
	.reset_gpio_number = TOUCH_RESET_PIN,
	.attn_gpio_name = "rmi4_attn",
	.reset_gpio_name = "rmi4_reset",
};

static struct rmi_device_platform_data syna_platformdata ={
	.driver_name = "rmi_generic",
	.sensor_name = "RMI4",
	.attn_gpio = TOUCH_INT_PIN,
	.enable_gpio = ENABLE_GPIO,
	.attn_polarity = 0,
	.gpio_data = &rmi4_gpiodata,
	.gpio_config = synaptics_touchpad_gpio_setup,
	.reset_delay_ms = 100,
	.axis_align = {
	.flip_x = false,
	.flip_y = false,},
};
#endif

static struct i2c_board_info u9508_input_i2c_bus2_devs[]= {
	/* Synaptics RMI4 TS */
	[0] = {
		.type		= SYNA_NAME,
		.addr		= 0x70,
		.flags 		= true,
		.platform_data 	= &syna_platformdata,
	},
};

void __init eprj_u9508_input_init(void)
{
	int ret = 0;

	ret = i2c_register_board_info(2, u9508_input_i2c_bus2_devs,
					ARRAY_SIZE(u9508_input_i2c_bus2_devs));
	if (ret)
		printk(KERN_ERR "ERROR: u9508-input devices failed to register!!\n");

	synaptics_virtual_keys_init();
}
	
