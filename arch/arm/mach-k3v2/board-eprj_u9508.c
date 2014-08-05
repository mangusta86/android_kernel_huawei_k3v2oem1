/*
 * arch/arm/mach-k3v2/board-eprj_u9508.c
 *
 * Board file for Huawei U9508 machine -- Main
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
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/serial_8250.h>
#include <linux/clk.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/dma-mapping.h>
#include <linux/pda_power.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/sysdev.h>
#include <linux/amba/bus.h>
#include <linux/io.h>

#include <linux/i2c.h>
#include <linux/input.h>

#include <linux/memblock.h>

#include <linux/mhl/mhl.h>

#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/system.h>
#include <asm/mach/arch.h>
#include <mach/hardware.h>
#include <mach/system.h>
#include <mach/irqs.h>
#include <mach/io.h>
#include <mach/gpio.h>
#include <mach/early-debug.h>
#include <mach/hisi_mem.h>
#include <mach/k3_keypad.h>
#include <mach/boardid.h>
#include <mach/tps61310.h>

#include "board.h"
#include "board-eprj_u9508.h"
#include "clock.h"
#include "k3v2_clocks_init_data.h"
#include <mach/sound/tpa2028_spk_l.h>
#include <mach/sound/tpa2028_spk_r.h>
#include <mach/sound/tpa6132.h>
#include <mach/sound/spk_5vboost.h>
#include <mach/sound/modemctl.h>

#include <linux/switch_usb.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/irq.h>
#include <hsad/config_debugfs.h>
#include <hsad/config_interface.h>
#include <linux/uart_output.h>
#include <linux/sc16is750_i2c.h>
#ifdef CONFIG_LEDS_K3_6421
#include <linux/led/k3-leds.h>
#endif

#ifdef CONFIG_ANDROID_K3_VIBRATOR
#include <linux/vibrator/k3_vibrator.h>
#endif

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif
#include <mach/ak6921af.h>

#include <linux/skbuff.h>

#ifdef CONFIG_EXTRAL_DYNAMIC_DCDC
#include <mach/extral_dynamic_dcdc.h>
#endif

/* for framebuffer */
#define PLATFORM_DEVICE_LCD_NAME "mipi_cmi_OTM1280A"

/* USB Definitions */
#define USB_SWITCH_CONTROL_GPIO		53
#define USB_SWITCH_EN_GPIO			52
#define USB_SWITCH_INTERRUPT_GPIO	99

/*pengchao add begin*/
atomic_t touch_is_pressed;
EXPORT_SYMBOL(touch_is_pressed);
u32 time_finger_up = 0;
EXPORT_SYMBOL(time_finger_up);
/*pengchao add end*/

#define SECRAM_RESET_ADDR	IO_ADDRESS(REG_BASE_PMUSPI + (0x87 << 2))
#define SECRAM_REST_FLAG_LEN	(0x8)
#define SECRAM_REST_INFO_LEN	(0x20)

#define RESET_COLD_FLAG		"coldboot"
#define RESET_WARM_FLAG		"warmboot"
#define SCTRL_SCSYSSTAT		0x004
#define SCTRL_SCPERRSTEN0	0x080
#define PMU_RST_CTRL		(0x035<<2)

#define GPIO_HIGH 1
#define GPIO_LOW  0

static struct platform_device hisik3_device_hwmon = {
	.name	= "k3-hwmon",
	.id		= -1,
};

#ifdef CONFIG_LEDS_K3_6421
/*k3_led begin*/
static struct k3_led_platform_data hi6421_leds = {
	.leds_size = K3_LEDS_MAX,
	.leds = {
		[0] = {
			.name = "red",
			.brightness = LED_OFF,
			.delay_on = 0,
			.delay_off = 0,
			.default_trigger = "timer",
		},
		[1] = {
			.name = "green",
			.brightness = LED_OFF,
			.delay_on = 0,
			.delay_off = 0,
			.default_trigger = "timer",
		},
		[2] {
			.name = "blue",
			.brightness = LED_OFF,
			.delay_on = 0,
			.delay_off = 0,
			.default_trigger = "timer",
		},
	},
};

static struct k3_led_platform_data hi6421_leds_phone = {
	.leds_size = K3_LEDS_MAX,
	.leds = {
		[0] = {
			.name = "green",
			.brightness = LED_OFF,
			.delay_on = 0,
			.delay_off = 0,
			.default_trigger = "timer",
		},
		[1] = {
			.name = "red",
			.brightness = LED_OFF,
			.delay_on = 0,
			.delay_off = 0,
			.default_trigger = "timer",
		},
		[2] {
			.name = "blue",
			.brightness = LED_OFF,
			.delay_on = 0,
			.delay_off = 0,
			.default_trigger = "timer",
		},
	},
};

static struct resource hi6421_led_resources = {
	.start		= REG_BASE_PMUSPI,
	.end		= REG_BASE_PMUSPI + REG_PMUSPI_IOSIZE - 1,
	.flags		= IORESOURCE_MEM,

};
static struct platform_device hi6421_led_device = {
	.name		= K3_LEDS,
	.id			= 0,
	.dev = {
		.platform_data = &hi6421_leds,
		.init_name = "hkled",
	},
	.num_resources		= 1,
	.resource       =  &hi6421_led_resources,
};
/*k3_led end*/
#endif


#ifdef CONFIG_ANDROID_K3_VIBRATOR
static struct k3_vibrator_platform_data hi6421_vibrator = {
	.low_freq  = PERIOD,
	.low_power = ISET_POWER,
	.mode  = SET_MODE,
	.high_freq = PERIOD_QUICK,
	.high_power = ISET_POWERSTRONG,
};

/*vibrator  begin*/
static struct resource hi6421_vibrator_resources = {
	.start		= REG_BASE_PMUSPI,
	.end		= REG_BASE_PMUSPI + REG_PMUSPI_IOSIZE - 1,
	.flags		= IORESOURCE_MEM,

};
static struct platform_device hi6421_vibrator_device = {
	.name		= K3_VIBRATOR,
	.id			= 0,
	.dev = {
		.platform_data = &hi6421_vibrator,
		.init_name = "hkvibrator",
	},
	.num_resources		= 1,
	.resource       =  &hi6421_vibrator_resources,
};
#endif

/*vibrator end*/

static struct resource hi6421_irq_resources[] = {
	{
		.start		= REG_BASE_PMUSPI,
		.end		= REG_BASE_PMUSPI + REG_PMUSPI_IOSIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= IRQ_GPIO159,
		.end		= IRQ_GPIO159,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct platform_device hisik3_hi6421_irq_device = {
	.name		= "hi6421-irq",
	.id			= 0,
	.dev.platform_data	= NULL,
	.num_resources		= ARRAY_SIZE(hi6421_irq_resources),
	.resource       =  hi6421_irq_resources,
};

static struct platform_device k3_lcd_device = {
	.name = PLATFORM_DEVICE_LCD_NAME,
	.id	= 1,
	.dev = {
		.init_name = "k3_dev_lcd",
	},
};

/* USB Start */
static struct usb_switch_platform_data usw_plat_data = {
	.name		= "usbsw",
	.usw_ctrl_gpio	= USB_SWITCH_CONTROL_GPIO,
	.usw_en_gpio	= USB_SWITCH_EN_GPIO,
	.usw_int_gpio	= USB_SWITCH_INTERRUPT_GPIO,
	.irq_flags	= IRQ_TYPE_EDGE_RISING,
};

static struct platform_device usb_switch_device = {
	.name   = "switch-usb",
	.dev    = {
		.init_name = "switch-usb",
		.platform_data = &usw_plat_data,
	},
};
/* USB End */

/* Camera */
static struct resource hisik3_camera_resources[] = {
	{
		.name		= "isp_base",
		.start		= REG_BASE_ISP,
		.end		= REG_BASE_ISP + REG_ISP_IOSIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.name		= "isp_irq",
		.start		= IRQ_ISP,
		.end		= IRQ_ISP,
		.flags		= IORESOURCE_IRQ,
	},
	{
		.name		= "csi0_irq",
		.start		= IRQ_MIPICSI0,
		.end		= IRQ_MIPICSI0,
		.flags		= IORESOURCE_IRQ,
	},
	{
		.name		= "csi1_irq",
		.start		= IRQ_MIPICSI1,
		.end		= IRQ_MIPICSI1,
		.flags		= IORESOURCE_IRQ,
	}
};

static struct platform_device hisik3_camera_device = {
	.id	= 0,
	.name	= "k3-camera-v4l2",
	.dev = {
		.init_name = "camera",
	},
	.resource	= hisik3_camera_resources,
	.num_resources	= ARRAY_SIZE(hisik3_camera_resources),
};

static struct platform_device hisik3_fake_camera_device = {
	.id	= 1,
	.name	= "k3-fake-camera-v4l2",
	.resource	= 0,
	.num_resources	= 0,
	/*
	.dev = {
		.release = camera_platform_release,
	}
	,*/
};

/* Keypad device and platform data start, use KPC realizing keypad. */
/* That should be useless */
static const uint32_t default_keymap[] = {
	/*row, col, key*/
	/* used for debug only.*/
	KEY(0, 0, KEY_MENU),
	KEY(0, 1, KEY_BACK),

	KEY(1, 0, KEY_LEFT),
	KEY(1, 1, KEY_RIGHT),

	KEY(2, 0, KEY_UP),

	KEY(2, 1, KEY_DOWN),
	KEY(2, 2, DPAD_CENTER),

	KEY(0, 2, KEY_CAMERA_FOCUS),
	KEY(1, 2, KEY_CAMERA),

	/* TODO: add your keys below*/

	/*Used for software function, not physical connection!*/

};

static struct matrix_keymap_data hisik3_keymap_data = {
	.keymap = default_keymap,
	.keymap_size = ARRAY_SIZE(default_keymap),
};
static uint16_t long_func_key1[] = {KEY_BACK};
static uint16_t long_func_key2[] = {DPAD_CENTER, KEY_VOLUMEDOWN};

static struct keypad_remap_item remap_items[] = {
	{KEY_HOME, 1, 1000/*ms*/, long_func_key1},
	/*{KEY_A, 2, 500, long_func_key2},*/
	/*TODO: add your remap_item here*/
};

static struct keypad_remap keypad_long_remap = {
	.count = ARRAY_SIZE(remap_items),
	.items = remap_items,
};

static struct resource hisik3_keypad_resources[] = {
	[0] = {
		.start = REG_BASE_KPC,
		.end = REG_BASE_KPC + REG_KPC_IOSIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_KPC,
		.end = IRQ_KPC,
		.flags = IORESOURCE_IRQ,
	},
};

static struct k3v2_keypad_platdata hisik3_keypad_platdata = {
	.keymap_data = &hisik3_keymap_data,
	.keypad_remap = &keypad_long_remap,
	.rows = 8,
	.cols = 8,
	.row_shift = 3,
};

static struct platform_device hisik3_keypad_device = {
	.name = "k3_keypad",
	.id = -1,
	.num_resources = ARRAY_SIZE(hisik3_keypad_resources),
	.resource = hisik3_keypad_resources,
	.dev.platform_data = &hisik3_keypad_platdata,
};

/* Keypad device and platform data start, use GPIO realizing keypad. */
/* GPIO-Keys: Volume Rocker */
static struct resource hisik3_gpio_keypad_resources[] = {
	[0] = {
		.start = REG_BASE_GPIO18,
		.end = REG_BASE_GPIO18 + REG_GPIO18_IOSIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_GPIO(GPIO_17_1),
		.end = IRQ_GPIO(GPIO_17_1),
		.flags = IORESOURCE_IRQ,
	},
	[2] = {
		.start = IRQ_GPIO(GPIO_17_2),
		.end = IRQ_GPIO(GPIO_17_2),
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device hisik3_gpio_keypad_device = {
	.name = "k3v2_gpio_key",
	.id = -1,
	.num_resources = ARRAY_SIZE(hisik3_gpio_keypad_resources),
	.resource = hisik3_gpio_keypad_resources,
};

/* IRQ-Managed PowerKey */
static struct resource hisik3_power_key_resources[] = {
	[0] = {
		.start = REG_BASE_PMUSPI,
		.end = REG_BASE_PMUSPI + REG_PMUSPI_IOSIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_POWER_KEY_PRESS,
		.end = IRQ_POWER_KEY_PRESS,
		.flags = IORESOURCE_IRQ,
	},
	[2] = {
		.start = IRQ_POWER_KEY_RELEASE,
		.end = IRQ_POWER_KEY_RELEASE,
		.flags = IORESOURCE_IRQ,
	},
	[3] = {
		.start = IRQ_POWER_KEY_LONG_PRESS_1S,
		.end = IRQ_POWER_KEY_LONG_PRESS_1S,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device hisik3_power_key_device = {
	.name = "k3v2_power_key",
	.id = -1,
	.num_resources = ARRAY_SIZE(hisik3_power_key_resources),
	.resource = hisik3_power_key_resources,
};

/* K3 Watchdog */
static struct resource  hisik3_watchdog_resources[] = {

         [0] = {
                 .start = REG_BASE_WD,
                 .end = REG_BASE_WD + REG_WD_IOSIZE -1,
                 .flags = IORESOURCE_MEM,
         },

         [1] = {
                 .start = IRQ_WDOG,
                 .end   = IRQ_WDOG,
                 .flags = IORESOURCE_IRQ,
         },
 };

 static struct platform_device  hisik3_watchdog_device = {
         .name = "k3v2_watchdog",
         .id = -1,
         .num_resources = ARRAY_SIZE(hisik3_watchdog_resources),
         .resource = hisik3_watchdog_resources,
 };


/* Keypad backlight -- Should not be TS BL -- useless */
static struct platform_device hisik3_keypad_backlight_device = {
	.name = "keyboard-backlight",
};

/* TPA2028_SPK_L */
static struct tpa2028_l_platform_data tpa2028_l_pdata = {
	.gpio_tpa2028_en    = GPIO_14_5,/* 117 */
};

/* TPA2028_SPK_R */
static struct tpa2028_r_platform_data tpa2028_r_pdata = {
    //.gpio_tpa2028_en    = GPIO_14_5,/* 117 */
};

/* TPA6132 */
static struct tpa6132_platform_data tpa6132_pdata = {
	.gpio_tpa6132_en    = GPIO_14_6,/* 118 */
};

static struct platform_device tpa6132_device = {
	.name    = TPA6132_NAME,
	.id      = 0,
	.dev     = {
		.platform_data = &tpa6132_pdata,
	},
};

static struct tps61310_platform_data tps61310_platform_data =
{
	.reset_pin			= GPIO_9_4,
	.strobe0			= GPIO_8_1,
	.strobe1			= GPIO_8_2,
};

static struct platform_device boardid_dev ={
    .name    = "boardid_dev",
    .id      = 0,
};

//modem audio path switch control
static struct modemctl_platform_data modem_switch_pdata = {
	.gpio_modemctl_en    = GPIO_13_2,/* 106 */
};
static struct platform_device modem_switch_device = {
	.name    = MODEMCTL_NAME,
	.id      = 0,
	.dev     = {
		.platform_data = &modem_switch_pdata,
	},
};

#ifdef MHL_SII9244
static struct mhl_platform_data k3_mhl_data =
{
	.gpio_reset 	= MHL_GPIO_RESET,
	.gpio_wake_up	= MHL_GPIO_WAKE_UP,
	.gpio_int		= MHL_GPIO_INT,
};
#endif

/* please add i2c bus 0 devices here */
static struct i2c_board_info hisik3_i2c_bus0_devs[]= {
	/* camera tps61310 light */
#ifdef CONFIG_HIK3_CAMERA_FLASH
	[0]	=	{
		.type			= K3_FLASH_NAME,
		.addr			= K3_FLASH_I2C_ADDR,
		.platform_data		= &tps61310_platform_data,
	},
#endif
};

static struct i2c_board_info hisik3_i2c_bus0_tpa2028_l[]= {
	[0] =   {
		.type			= TPA2028_L_NAME,
		.addr			= TPA2028_I2C_ADDR,
		.flags 			= true,
		.platform_data 	= &tpa2028_l_pdata,
	},

	/*TODO: add your device here*/
};

static struct i2c_board_info hisik3_i2c_bus1_tpa2028_r[]= {
	
	[0] =   {
		.type			= TPA2028_R_NAME,
		.addr			= TPA2028_I2C_ADDR,
		.flags 			= true,
		.platform_data 	= &tpa2028_r_pdata,
	},
	/*TODO: add your device here*/
};

/* please add i2c bus 1 devices here */
static struct i2c_board_info hisik3_i2c_bus1_devs[]= {
#ifdef MHL_SII8240
        [0]	=	{
        	.type			= "Sil-8240",
        	.addr			= 0x3B,
        	.flags                     = I2C_CLIENT_WAKE,
	},

#elif defined(MHL_SII9244)
	[0]	=	{
		.type			= "mhl_Sii9244_page0",
		.addr			= MHL_SII9244_PAGE0_ADDR,
		.platform_data 		= &k3_mhl_data,
	},
	[1]	=	{
		.type			= "mhl_Sii9244_page1",
		.addr			= MHL_SII9244_PAGE1_ADDR,
		.platform_data 		= NULL,
	},
	[2]	=	{
		.type			= "mhl_Sii9244_page2",
		.addr			= MHL_SII9244_PAGE2_ADDR,
		.platform_data 		= NULL,
	},
	[3]	=	{
		.type			= "mhl_Sii9244_cbus",
		.addr			= MHL_SII9244_CBUS_ADDR,
		.platform_data 		= NULL,
	},
#else
/*TODO: add your device here*/
#endif
};

#ifdef CONFIG_EXTRAL_DYNAMIC_DCDC                                        
static struct extral_dynamic_dcdc_platform_data tps6236x_platform_date ={
    .enable_gpio    = EXTRAL_DYNAMIC_DCDC_EN,                            
    .regulator_data = &extral_dynamic_dcdc_regulator,                    
};                                                                       
#endif

#ifdef CONFIG_HUAWEI_HW_DEV_DCT
static struct platform_device huawei_device_detect = {
	.name = "hw-dev-detect",
	.id   =-1,
};
#endif

/* please add platform device in the struct.*/
static struct platform_device *k3v2oem1_public_dev[] __initdata = {
	&hisik3_hi6421_irq_device,
#ifdef CONFIG_LEDS_K3_6421
	&hi6421_led_device,
#endif

#ifdef CONFIG_ANDROID_K3_VIBRATOR
	&hi6421_vibrator_device,
#endif
	&hisik3_camera_device,
	&hisik3_fake_camera_device,
	&hisik3_device_hwmon,
	&hisik3_keypad_device,
	&hisik3_keypad_backlight_device,
	&k3_lcd_device, 
	&hisik3_power_key_device,
	&tpa6132_device,
	&usb_switch_device,
	&boardid_dev,
         &hisik3_watchdog_device,
#ifdef CONFIG_HUAWEI_HW_DEV_DCT
    &huawei_device_detect,
#endif
};

static void k3v2_i2c_devices_init(void)
{
	/* Register devices on I2C Bus0 and Bus1*/
	i2c_register_board_info(0, hisik3_i2c_bus0_devs,
					ARRAY_SIZE(hisik3_i2c_bus0_devs));
	i2c_register_board_info(1, hisik3_i2c_bus1_devs,
					ARRAY_SIZE(hisik3_i2c_bus1_devs));

	/* EternityProject: TPA2028 Audio Chip spec. */
	i2c_register_board_info(0, hisik3_i2c_bus0_tpa2028_l,
					ARRAY_SIZE(hisik3_i2c_bus0_tpa2028_l));
	i2c_register_board_info(1, hisik3_i2c_bus1_tpa2028_r,
					ARRAY_SIZE(hisik3_i2c_bus1_tpa2028_r));
}


/* EternityProject: Implement ram_console */
static struct resource ram_console_resources[] = {
	{
		.flags = IORESOURCE_MEM,
	},
};

static struct platform_device ram_console_device = {
	.name 		= "ram_console",
	.id 		= -1,
	.num_resources	= ARRAY_SIZE(ram_console_resources),
	.resource	= ram_console_resources,
};

static void __init k3v2oem1_init(void)
{
	unsigned int  index = 0;
	int err;

	edb_trace(1);
	k3v2_common_init();

	/* EternityProject: Init LED device and keypad for volume rocker */
	hi6421_led_device.dev.platform_data = &hi6421_leds_phone;
	for (index = 0; index < ARRAY_SIZE(k3v2oem1_public_dev); index++)
		if ((struct platform_device *)(&hisik3_keypad_device) == (struct platform_device *)(k3v2oem1_public_dev[index])) {
			k3v2oem1_public_dev[index] = &hisik3_gpio_keypad_device;
		break;
	}

	platform_add_devices(k3v2oem1_public_dev, ARRAY_SIZE(k3v2oem1_public_dev));

	eprj_u9508_power_init();
	eprj_u9508_connect_init();

	k3v2_i2c_devices_init();

	eprj_u9508_input_init();

#ifdef CONFIG_DEBUG_FS
	config_debugfs_init();
	uart_output_debugfs_init();
#endif

	printk(KERN_INFO "Initializing ramconsole\n");
	err = platform_device_register(&ram_console_device);
	if (err) {
		printk(KERN_ERR "Unable to register ram-console device.\n");
	}
}

static void __init k3v2_early_init(void)
{
	int chip_id = 0;
	k3v2_init_clock();
	chip_id = get_chipid();
	if (chip_id == CS_CHIP_ID) {
#ifdef CONFIG_SUPPORT_B3750000_BITRATE
	    if(get_if_use_3p75M_uart_clock())
		{
		printk(KERN_INFO"k3v2_early_init oem2, use 60M periperal clock\n");
		k3v2_clk_init_from_table(common_clk_init_table_cs_60M);
		}
		else
#endif
		k3v2_clk_init_from_table(common_clk_init_table_cs);
	} else if (chip_id == DI_CHIP_ID) {
		k3v2_clk_init_from_table(common_clk_init_table_es);
	}
}

#if 0
static void k3v2_mem_setup(void)
{
	unsigned long reserved_size;

	printk(KERN_INFO "k3v2_mem_setup\n");

	/*
	   Memory reserved for Graphic/ Dcode/EnCode
	*/
	reserved_size = hisi_get_reserve_mem_size();

	/*
	 * Memory configuration with SPARSEMEM enabled on  (see
	 * asm/mach/memory.h for more information).
	 */
	arm_add_memory(PLAT_PHYS_OFFSET, (HISI_BASE_MEMORY_SIZE - reserved_size));

	return;
}

/*
 * k3v2_mem=size1@start1[,size2@start2][,...]
 * size means memory size which larger than 512M
 */
static int __init early_k3v2_mem(char *p)
{
	unsigned long size;
	phys_addr_t start;
	char *endp = NULL;
	char *ep = NULL;

	k3v2_mem_setup();

	printk(KERN_INFO "k3v2_mem = %s\n", p);

	start = PLAT_PHYS_OFFSET + HISI_BASE_MEMORY_SIZE;
	while (*p != '\0') {
		size  = memparse(p, &endp);
		if (*endp == '@')
			start = memparse(endp + 1, &ep);

		/* oem ec1 1G memory based */
		if ((start == SZ_512M)) {
			if (size < SZ_512M)
				size = 0;
			else
				size -= SZ_512M;
		}

		arm_add_memory(start, size);

		printk(KERN_INFO "early_k3v2_mem start 0x%x size 0x%lx\n", start, size);

		if (*ep == ',')
			p = ep + 1;
		else
			break;

		printk(KERN_INFO "k3v2_mem = %s\n", p);
	}

	return 0;
}
early_param("k3v2_mem", early_k3v2_mem);
#endif

static void __init k3v2_map_io(void)
{
	printk("k3v2oem2 map io\n");
	k3v2_map_common_io();
}


void __init ram_console_reserve(unsigned long ram_console_size)
{
	struct resource *res;
	long ret;

	res = platform_get_resource(&ram_console_device, IORESOURCE_MEM, 0);
	if (!res)
		goto fail;

	res->start = memblock_end_of_DRAM() - ram_console_size;
	res->end = res->start + ram_console_size - 1;
	ret = memblock_remove(res->start, ram_console_size);
	if (ret) goto fail;

fail:
	printk(KERN_ERR "ram-console: Cannot allocate memory. FAIL.\n");
}

static void __init eprj_reserve_memory(void)
{
	ram_console_reserve(SZ_1M);
}

MACHINE_START(K3V2OEM2, "k3v2oem1")
	.boot_params	= PLAT_PHYS_OFFSET + 0x00000100,
	.init_irq       = k3v2_gic_init_irq,
	.init_machine   = k3v2oem1_init,
	.map_io         = k3v2_map_io,
	.timer          = &k3v2_timer,
	.reserve	= &eprj_reserve_memory,
	.init_early 	= k3v2_early_init,
MACHINE_END
