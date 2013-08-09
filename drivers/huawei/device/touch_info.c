/*
 * Copyright (C) huawei company
 *
 * This	program	is free	software; you can redistribute it and/or modify
 * it under	the	terms of the GNU General Public	License	version	2 as
 * published by	the	Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/rmi.h>

static char *touch_chip_info = NULL;
int set_touch_chip_info(const char *chip_info)
{

	if(chip_info == NULL){
		pr_err("touch_chip_info = %s\n", chip_info);
		return -EINVAL;
	}
	touch_chip_info = (char *)chip_info;
	return 0;
}
EXPORT_SYMBOL(set_touch_chip_info);

static struct platform_device touch_input_info = {
	.name = "huawei_touch",
	.id = -1,
};

static ssize_t show_touch_chip_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (dev == NULL) {
		pr_err("touch_chip info is null\n");
		return -EINVAL;
	}

	return sprintf(buf, "%s\n", touch_chip_info);
}
static DEVICE_ATTR(touch_chip_info, 0664,
				   show_touch_chip_info, NULL);

static struct attribute *touch_input_attributes[] = {
	&dev_attr_touch_chip_info.attr,
	NULL
};
static const struct attribute_group touch_input = {
	.attrs = touch_input_attributes,
};
static int __init touch_input_info_init(void)
{
	int ret = 0;
	printk(KERN_INFO"[%s] ++", __func__);

	ret = platform_device_register(&touch_input_info);
	if (ret) {
		pr_err("%s: platform_device_register failed, ret:%d.\n",
				__func__, ret);
		goto REGISTER_ERR;
	}

	ret = sysfs_create_group(&touch_input_info.dev.kobj, &touch_input);
	if (ret) {
		pr_err("touch_input_info_init sysfs_create_group error ret =%d", ret);
		goto SYSFS_CREATE_CGOUP_ERR;
	}
	printk(KERN_INFO"[%s] --", __func__);

	return 0;
SYSFS_CREATE_CGOUP_ERR:
	platform_device_unregister(&touch_input_info);
REGISTER_ERR:
	return ret;

}

device_initcall(touch_input_info_init);
MODULE_DESCRIPTION("touch input info");
MODULE_AUTHOR("huawei driver group of k3v2");
MODULE_LICENSE("GPL");
