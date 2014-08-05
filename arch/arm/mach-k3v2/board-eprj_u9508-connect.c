/*
 * arch/arm/mach-k3v2/board-eprj_u9508-connect.c
 *
 * Board file for Huawei U9508 machine -- Connectivity Spec.
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
#include <linux/device.h>
#include <linux/platform_device.h>

#include <mach/gpio.h>

/* EternityProject: Broadcom (A)GPS defines */
#define GPIO_GPS_BCM_EN		(GPIO_18_7)
#define GPIO_GPS_BCM_RET	(GPIO_19_0)
#define GPIO_GPS_BCM_REFCLK	(GPIO_19_1)		/* GPIO_153 */
#define GPIO_GPS_BCM_EN_NAME	"gpio_gps_bcm_enable"
#define GPIO_GPS_BCM_RET_NAME	"gpio_gps_bcm_rest"
#define GPIO_GPS_BCM_REFCLK_NAME "gpio_gps_bcm_refclk"

/* EternityProject: Bluetooth and BCMSDIO */
#define	GPIO_BT_EN		(GPIO_21_1)
#define	GPIO_BT_RST		(GPIO_21_0)
#define	GPIO_HOST_WAKEUP	(GPIO_20_6)
#define	GPIO_DEV_WAKEUP		(GPIO_20_7)

#define	REGULATOR_DEV_BLUETOOTH_NAME	"bt-io"


static struct resource k3_gps_bcm_resources[] = {
	[0] = {
		.name  = GPIO_GPS_BCM_EN_NAME,
		.start = GPIO_GPS_BCM_EN,
		.end   = GPIO_GPS_BCM_EN,
		.flags = IORESOURCE_IO,
	},
	[1] = {
		.name  = GPIO_GPS_BCM_RET_NAME,
		.start = GPIO_GPS_BCM_RET,
		.end   = GPIO_GPS_BCM_RET,
		.flags = IORESOURCE_IO,
	},
	[2] = {
		.name  = GPIO_GPS_BCM_REFCLK_NAME,
		.start = GPIO_GPS_BCM_REFCLK,
		.end   = GPIO_GPS_BCM_REFCLK,
		.flags = IORESOURCE_IO,
	},
};

static struct platform_device k3_gps_bcm_device = {
	.name	= "k3_gps_bcm_47511",
	.id	= 1,
	.dev	= {
		  .init_name = "gps_bcm_47511",
	},
	.num_resources = ARRAY_SIZE(k3_gps_bcm_resources),
	.resource = k3_gps_bcm_resources,
};

/* Bluetooth */
static struct resource bluepower_resources[] = {
	{
		.name	= "bt_gpio_enable",
		.start	= GPIO_BT_EN,
		.end	= GPIO_BT_EN,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "bt_gpio_rst",
		.start	= GPIO_BT_RST,
		.end	= GPIO_BT_RST,
		.flags	= IORESOURCE_IO,
	},
};

static struct platform_device btbcm_device = {
	.name	=	"bt_power",
	.dev	= {
		  .platform_data = NULL,
		  .init_name = REGULATOR_DEV_BLUETOOTH_NAME,
	},
	.id	= -1,
	.num_resources	= ARRAY_SIZE(bluepower_resources),
	.resource	= bluepower_resources,

};

static struct resource bluesleep_resources[] = {
	{
		.name	= "gpio_host_wake",
		.start	= GPIO_HOST_WAKEUP,
		.end	= GPIO_HOST_WAKEUP,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "gpio_ext_wake",
		.start	= GPIO_DEV_WAKEUP,
		.end	= GPIO_DEV_WAKEUP,
		.flags	= IORESOURCE_IO,
	},
};

static struct platform_device bcm_bluesleep_device = {
	.name		= "bluesleep",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(bluesleep_resources),
	.resource	= bluesleep_resources,
};


static struct platform_device *u9508_connect_devs[] __initdata = {
	&btbcm_device,
	&bcm_bluesleep_device,
	&k3_gps_bcm_device,
};

void __init eprj_u9508_connect_init(void)
{
	int ret = 0;

	ret = platform_add_devices(u9508_connect_devs, ARRAY_SIZE(u9508_connect_devs));
	if (ret)
		printk(KERN_ERR "ERROR: u9508-connect devices failed to register!!\n");
}
