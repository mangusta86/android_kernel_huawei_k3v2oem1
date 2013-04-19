/*
 * Copyright (C) 2009 Texas	Instruments
 *
 * Author: Pradeep Gurumath	<pradeepgurumath@ti.com>
 * This	program	is free	software; you can redistribute it and/or modify
 * it under	the	terms of the GNU General Public	License	version	2 as
 * published by	the	Free Software Foundation.
 */

/* linux/arch/arm/mach-k3v2/k3v2_wifi_power.c
 */

/*=========================================================================
 *
 * histoty
 *
 *=========================================================================
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/err.h>
#include <linux/random.h>
#include <linux/skbuff.h>
#include <generated/mach-types.h>
#include <linux/wifi_tiwlan.h>
#include <asm/mach-types.h>
#include <linux/mux.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/clk.h>
#include <linux/mtd/nve_interface.h>
#include <linux/ctype.h>


#include "k3v2_wifi_power.h"

struct wifi_host_s {
	struct regulator *vdd;
	struct clk *clk;
	struct iomux_block *block;
	struct block_config *config;
	bool bEnable;
};

struct wifi_host_s *wifi_host;

#define WLAN_MAC_LEN            6
#define NV_WLAN_NUM             193
#define NV_WLAN_VALID_SIZE      12
#define MAC_ADDRESS_FILE  "/data/misc/wifi/macaddr"
unsigned char g_wifimac[WLAN_MAC_LEN] = {0};


static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];
typedef struct wifi_mem_prealloc_struct {
	void *mem_ptr;
	unsigned long size;
} wifi_mem_prealloc_t;

static wifi_mem_prealloc_t wifi_mem_array[PREALLOC_WLAN_NUMBER_OF_SECTIONS] = {
	{ NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER) }
};

/*kmalloc memory for wifi*/
void *wifi_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_NUMBER_OF_SECTIONS)
		return wlan_static_skb;
	if ((section < 0) || (section > PREALLOC_WLAN_NUMBER_OF_SECTIONS)) {
		pr_err("%s: is error(section:%d).\n", __func__, section);
		return NULL;
	}
	if (wifi_mem_array[section].size < size) {
		pr_err("%s: is error(size:%lu).\n", __func__, size);
		return NULL;
	}
	return wifi_mem_array[section].mem_ptr;
}

#ifndef	CONFIG_WIFI_CONTROL_FUNC
EXPORT_SYMBOL(wifi_mem_prealloc);
#endif

/*init wifi buf*/
int init_wifi_mem(void)
{
	int i = 0;

	pr_info("init_wifi_mem.\n");
	for (i = 0; i < WLAN_SKB_BUF_NUM; i++) {
		if (i < (WLAN_SKB_BUF_NUM / 2))
			wlan_static_skb[i] = dev_alloc_skb(WLAN_SKB_BUF_MIN);
		else
			wlan_static_skb[i] = dev_alloc_skb(WLAN_SKB_BUF_MAX);
		if (wlan_static_skb[i] == NULL) {
			pr_err("%s: dev_alloc_skb is error(%d).\n", __func__, i);
			return -ENOMEM;
		}
	}

	for	(i = 0; i < PREALLOC_WLAN_NUMBER_OF_SECTIONS; i++) {
		wifi_mem_array[i].mem_ptr = kzalloc(wifi_mem_array[i].size,
			GFP_KERNEL);
		if (wifi_mem_array[i].mem_ptr == NULL) {
			pr_err("%s: alloc mem_ptr is error(%d).\n", __func__, i);
			return -ENOMEM;
		}
	}
	return 0;
}

/*deinit wifi buf*/
int deinit_wifi_mem(void)
{
	int i = 0;

	pr_info("deinit_wifi_mem.\n");
	for (i = 0; i < WLAN_SKB_BUF_NUM; i++) {
		if (wlan_static_skb[i] != NULL) {
			dev_kfree_skb(wlan_static_skb[i]);
			wlan_static_skb[i] = NULL;
		} else
			break;
	}
	for	(i = 0;	i < PREALLOC_WLAN_NUMBER_OF_SECTIONS; i++) {
		if (wifi_mem_array[i].mem_ptr != NULL) {
			kfree(wifi_mem_array[i].mem_ptr);
			wifi_mem_array[i].mem_ptr = NULL;
		} else
			break;
	}
	return 0;
}

int k3v2_wifi_power(int on)
{
	int ret = 0;
	pr_info("%s: on:%d\n", __func__, on);
#if defined CONFIG_MACH_K3V2OEM1
	if (NULL == wifi_host) {
		pr_err("%s: wifi_host is null\n", __func__);
		return -1;
	}
#endif

	if (on) {
		/*hi_sdio_set_power(on);*/
#if defined CONFIG_MACH_K3V2OEM1
		if (wifi_host->bEnable) {
			pr_err("%s: wifi had power on.\n", __func__);
			return ret;
		}
		ret = clk_enable(wifi_host->clk);
		if (ret < 0) {
			pr_err("%s: clk_enable failed, ret:%d\n", __func__, ret);
			return ret;
		}
		ret = blockmux_set(wifi_host->block, wifi_host->config, NORMAL);
		if (ret < 0) {
			pr_err("%s: blockmux_set failed, ret:%d\n", __func__, ret);
			clk_disable(wifi_host->clk);
			return ret;
		}
#endif

#if defined CONFIG_MACH_K3V2OEM1
		ret = regulator_enable(wifi_host->vdd);
		if (ret < 0) {
			pr_err("%s: regulator_enable failed, ret:%d\n", __func__, ret);
			clk_disable(wifi_host->clk);
			ret = blockmux_set(wifi_host->block, wifi_host->config, LOWPOWER);
			if (ret < 0)
				pr_err("%s: blockmux_set failed, ret:%d\n", __func__, ret);
			return ret;
	}
#endif
		gpio_set_value(K3V2_WIFI_POWER_GPIO, 0);
		msleep(100);
		gpio_set_value(K3V2_WIFI_POWER_GPIO, 1);
		msleep(200);
#if defined CONFIG_MACH_K3V2OEM1
		wifi_host->bEnable = true;
#endif
		hi_sdio_set_power(on);
	} else {
		hi_sdio_set_power(on);
		gpio_set_value(K3V2_WIFI_POWER_GPIO, 0);
		msleep(20);

#if defined CONFIG_MACH_K3V2OEM1
		if (!wifi_host->bEnable) {
			pr_err("%s: wifi had power off\n", __func__);
			return ret;
		}
		ret = regulator_disable(wifi_host->vdd);
		if (ret < 0)
			pr_err("%s: regulator_enable failed, ret:%d\n", __func__, ret);
#endif

#if defined CONFIG_MACH_K3V2OEM1
		ret = blockmux_set(wifi_host->block, wifi_host->config, LOWPOWER);
		if (ret < 0)
			pr_err("%s: blockmux_set failed, ret:%d\n", __func__, ret);

		clk_disable(wifi_host->clk);
		wifi_host->bEnable = false;
		/*hi_sdio_set_power(on);*/
#endif
	}

	return ret;
}

#ifndef CONFIG_WIFI_CONTROL_FUNC
EXPORT_SYMBOL(k3v2_wifi_power);
#endif

int k3v2_wifi_reset(int on)
{
	pr_info("%s: on:%d.\n", __func__, on);
	if (on)
		gpio_set_value(K3V2_WIFI_POWER_GPIO, 1);
	else
		gpio_set_value(K3V2_WIFI_POWER_GPIO, 0);

	return 0;
}

#ifndef CONFIG_WIFI_CONTROL_FUNC
EXPORT_SYMBOL(k3v2_wifi_reset);
#endif

static int char2byte( char* strori, char* outbuf )
{
    int i = 0;
    int temp = 0;
    int sum = 0;
    for( i = 0; i < 12; i++ )
    {
         switch (strori[i]) {
             case '0' ... '9':
                 temp = strori[i] - '0';
                 break;
             case 'a' ... 'f':
                 temp = strori[i] - 'a' + 10;
                 break;
             case 'A' ... 'F':
                 temp = strori[i] - 'A' + 10;
                 break;
             default:
                 break;
        }
        sum += temp;
        if( i % 2 == 0 ){
            outbuf[i/2] |= temp << 4;
        }
        else{
            outbuf[i/2] |= temp;
        }
    }
    return sum;
}

static void read_from_global_buf(unsigned char * buf)
{
    memcpy(buf,g_wifimac,WLAN_MAC_LEN);
    printk("get MAC from g_wifimac: mac=%02x:%02x:%02x:%02x:%02x:%02x\n",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5]);
    return;
}

static int read_from_file(unsigned char * buf)
{
    struct file* filp = NULL;
    mm_segment_t old_fs;
    int result = 0;
    filp = filp_open(MAC_ADDRESS_FILE, O_CREAT|O_RDWR, 0666);
    if(IS_ERR(filp))
    {
        printk("open mac address file error\n");
        return -1;
    }

    old_fs = get_fs();
    set_fs(KERNEL_DS);
    filp->f_pos = 0;
    result = filp->f_op->read(filp,buf,WLAN_MAC_LEN,&filp->f_pos);
    if(WLAN_MAC_LEN == result)
    {
        printk("get MAC from the file!\n");
        memcpy(g_wifimac,buf,WLAN_MAC_LEN);
        set_fs(old_fs);
        filp_close(filp,NULL);
        return 0;
    }
    //random mac
    get_random_bytes(buf,WLAN_MAC_LEN);
    buf[0] = 0x0;
    printk("get MAC from Random: mac=%02x:%02x:%02x:%02x:%02x:%02x\n",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5]);
    memcpy(g_wifimac,buf,WLAN_MAC_LEN);

    //update mac -file
    filp->f_pos = 0;
    result = filp->f_op->write(filp,buf,WLAN_MAC_LEN,&filp->f_pos);
    if(WLAN_MAC_LEN != result )
    {
        printk("update NV mac to file error\n");
        set_fs(old_fs);
        filp_close(filp,NULL);
        return -1;
    }
    set_fs(old_fs);
    filp_close(filp,NULL);
    return 0;
}

int k3v2_wifi_get_mac_addr(unsigned char *buf)
{
    struct nve_info_user  info;
    int ret = -1;
    int sum = 0;

    if (NULL == buf) {
        pr_err("%s: k3v2_wifi_get_mac_addr failed\n", __func__);
        return -1;
    }

    memset(buf, 0, WLAN_MAC_LEN );

    memset(&info, 0, sizeof(info));
    info.nv_number  = NV_WLAN_NUM;   //nve item
    strcpy( info.nv_name, "MACWLAN" );
    info.valid_size = NV_WLAN_VALID_SIZE;
    info.nv_operation = NV_READ;

    if (0 != g_wifimac[0] || 0 != g_wifimac[1] || 0 != g_wifimac[2] || 0 != g_wifimac[3]
       || 0 != g_wifimac[4] || 0 != g_wifimac[5])
    {
        read_from_global_buf(buf);
        return 0;
    }

    ret = nve_direct_access( &info );

    if (!ret)
    {
        sum = char2byte(info.nv_data, buf );
        if (0 != sum)
        {
            printk("get MAC from NV: mac=%02x:%02x:%02x:%02x:%02x:%02x\n",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5]);
            memcpy(g_wifimac,buf,WLAN_MAC_LEN);
            return 0;
        }
    }
    //read buf from mac-file or random mac
    return read_from_file(buf);
}

#ifndef CONFIG_WIFI_CONTROL_FUNC
EXPORT_SYMBOL(k3v2_wifi_get_mac_addr);
#endif
struct wifi_platform_data k3v2_wifi_control = {
	.set_power = k3v2_wifi_power,
	.set_reset = k3v2_wifi_reset,
	.set_carddetect = hi_sdio_detectcard_to_core,
	.get_mac_addr = k3v2_wifi_get_mac_addr,
	.mem_prealloc = wifi_mem_prealloc,
};

#ifdef CONFIG_WIFI_CONTROL_FUNC
static struct resource k3v2_wifi_resources[] = {
	[0] = {
	.name  = "bcm4329_wlan_irq",
	.start = IRQ_GPIO(K3V2_WIFI_IRQ_GPIO),
	.end   = IRQ_GPIO(K3V2_WIFI_IRQ_GPIO),
	.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL
			| IRQF_NO_SUSPEND,/* IORESOURCE_IRQ_HIGHEDGE */
	},
};

static struct platform_device k3v2_wifi_device = {
	.name = "bcm4329_wlan",
	.id = 1,
	.num_resources = ARRAY_SIZE(k3v2_wifi_resources),
	.resource = k3v2_wifi_resources,
	.dev = {
		.platform_data = &k3v2_wifi_control,
	},
};
#endif

static int __init k3v2_wifi_init(void)
{
	int ret = 0;

	ret = init_wifi_mem();
	if (ret) {
		pr_err("%s: init_wifi_mem failed.\n", __func__);
		goto err_malloc_wifi_host;
	}

	wifi_host = kzalloc(sizeof(struct wifi_host_s), GFP_KERNEL);
	if (!wifi_host)	{
		pr_err("%s: malloc wifi_host failed.\n", __func__);
		ret = -ENOMEM;
		goto err_malloc_wifi_host;
	}

#if defined CONFIG_MACH_K3V2OEM1
	wifi_host->bEnable = false;

	/* get 32kb clock */
	wifi_host->clk = clk_get(NULL, "clk_pmu32kb");
	if (IS_ERR(wifi_host->clk)) {
		pr_err("%s: clk_get failed\n", __func__);
		ret = -ENXIO;
		goto err_clk_get;
	}

	/* get wifiio vdd */
	wifi_host->vdd = regulator_get(NULL, "wifiio-vcc");
	if (IS_ERR(wifi_host->vdd)) {
		pr_err("%s: regulator_get failed.\n", __func__);
		ret = -ENXIO;
		goto err_regulator_get;
	}

	ret = regulator_set_voltage(wifi_host->vdd,
		K3V2_WIFI_VDD_VOLTAGE, K3V2_WIFI_VDD_VOLTAGE);
	if (ret < 0) {
		pr_err("%s: regulator_set_voltage failed, ret:%d.\n",
			__func__, ret);
		ret = -ENXIO;
		goto err_regulator_set_voltage;
	}

	/* set io mux*/
	wifi_host->block = iomux_get_block("block_wifi");
	if (!wifi_host->block) {
		pr_err("%s: iomux_lookup_block failed.\n", __func__);
		ret = -ENXIO;
		goto err_iomux_get_block;
	}

	wifi_host->config = iomux_get_blockconfig("block_wifi");
	if (!wifi_host->config) {
		pr_err("%s: iomux_get_blockconfig failed.\n", __func__);
		ret = -ENXIO;
		goto err_iomux_get_blockconfig;
	}

	ret	= blockmux_set(wifi_host->block, wifi_host->config, LOWPOWER);
	if (ret < 0) {
		pr_err("%s: blockmux_set failed, ret.\n", __func__);
		goto err_blockmux_set;
	}
#else
	/* fpga VDD open forver,if can not request other driver maybe has open*/
	ret = gpio_request(K3V2_WIFI_VDD_GPIO, NULL);
	if (ret < 0) {
		pr_err("%s: gpio_request failed, ret:%d.\n", __func__,
			K3V2_WIFI_VDD_GPIO);
	} else
		gpio_direction_output(K3V2_WIFI_VDD_GPIO, 1);
#endif
	/* set power gpio */
	ret = gpio_request(K3V2_WIFI_POWER_GPIO, NULL);
	if (ret < 0) {
		pr_err("%s: gpio_request failed, ret:%d.\n", __func__,
			K3V2_WIFI_POWER_GPIO);
		goto err_power_gpio_request;
	}
	gpio_direction_output(K3V2_WIFI_POWER_GPIO, 0);
	/* set apwake gpio */
	ret = gpio_request(K3V2_WIFI_IRQ_GPIO, NULL);
	if (ret < 0) {
		pr_err("%s: gpio_request failed, ret:%d.\n", __func__,
			K3V2_WIFI_IRQ_GPIO);
		goto err_irq_gpio_request;
	}
	gpio_direction_input(K3V2_WIFI_IRQ_GPIO);

#ifdef CONFIG_WIFI_CONTROL_FUNC
	ret = platform_device_register(&k3v2_wifi_device);
	if (ret) {
		pr_err("%s: platform_device_register failed, ret:%d.\n",
			__func__, ret);
		goto err_platform_device_register;
	}
#endif

	return 0;

err_platform_device_register:
	gpio_free(K3V2_WIFI_IRQ_GPIO);
err_irq_gpio_request:
	gpio_free(K3V2_WIFI_POWER_GPIO);
err_power_gpio_request:

#if defined(CONFIG_MACH_K3V2OEM1)
err_blockmux_set:
err_iomux_get_blockconfig:
err_iomux_get_block:
err_regulator_set_voltage:
	regulator_put(wifi_host->vdd);
err_regulator_get:
	clk_put(wifi_host->clk);
err_clk_get:
	kfree(wifi_host);
	wifi_host = NULL;
#else
	gpio_free(K3V2_WIFI_VDD_GPIO);
#endif
err_malloc_wifi_host:
	deinit_wifi_mem();
	return ret;
}

device_initcall(k3v2_wifi_init);
