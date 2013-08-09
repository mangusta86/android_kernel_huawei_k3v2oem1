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
#include <linux/board_sensors.h>
#include <hsad/config_interface.h>

static char *sensor_binder_input[SENSOR_MAX] = {NULL};
static char *sensor_chip_info[SENSOR_MAX] = {NULL};
static char *gyro_selfTest_result;
static char *akm_selfTest_result;
int set_sensor_chip_info(enum input_name name, const char *chip_info)
{
	if (name >= SENSOR_MAX || chip_info == NULL) {
		pr_err("set_sensor name =%d chip_info = %s\n",
				name, chip_info);
		return -EINVAL;
	}
	sensor_chip_info[name] = (char *)chip_info;
	return 0;
}
EXPORT_SYMBOL(set_sensor_chip_info);

int set_sensor_input(enum input_name name, const char *input_num)
{
	if (name >= SENSOR_MAX || input_num == NULL) {
		pr_err("set_sensor_input name =%d input_num = %s\n",
				name, input_num);
		return -EINVAL;
	}
	sensor_binder_input[name] = (char *)input_num;

	return 0;
}
EXPORT_SYMBOL(set_sensor_input);

int set_gyro_selfTest_result(enum input_name name, const char *result)
{
	if (name >= SENSOR_MAX || result == NULL) {
		pr_err("gyro self test result = %s\n", result);
		return -EINVAL;
	}
	gyro_selfTest_result = (char *)result;
	return 0;
}
EXPORT_SYMBOL(set_gyro_selfTest_result);
int set_compass_selfTest_result(enum input_name name, const char *result)
{
	if (name >= SENSOR_MAX || result == NULL) {
		pr_err("compass self test result = %s\n", result);
		return -EINVAL;
	}
	akm_selfTest_result = (char *)result;
	return 0;
}
EXPORT_SYMBOL(set_compass_selfTest_result);
static struct platform_device sensor_input_info = {
	.name = "huawei_sensor",
	.id = -1,
};
static ssize_t show_acc_chip_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (dev == NULL) {
		pr_err("acc_chip info is null\n");
		return -EINVAL;
	}
	return sprintf(buf, "%s\n", sensor_chip_info[ACC]);
}
static DEVICE_ATTR(acc_info, S_IRUGO,
				   show_acc_chip_info, NULL);
static ssize_t show_akm_chip_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (dev == NULL) {
		pr_err("akm_chip info is null\n");
		return -EINVAL;
	}
	return sprintf(buf, "%s\n", sensor_chip_info[AKM]);
}
static DEVICE_ATTR(akm_info, S_IRUGO,
				   show_akm_chip_info, NULL);
static ssize_t show_gyro_chip_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (dev == NULL) {
		pr_err("gyro_chip info is null\n");
		return -EINVAL;
	}
	return sprintf(buf, "%s\n", sensor_chip_info[GYRO]);
}
static DEVICE_ATTR(gyro_info, S_IRUGO,
				   show_gyro_chip_info, NULL);
static ssize_t show_ps_chip_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (dev == NULL) {
		pr_err("ps_chip info is null\n");
		return -EINVAL;
	}
	return sprintf(buf, "%s\n", sensor_chip_info[PS]);
}
static DEVICE_ATTR(ps_info, S_IRUGO,
				   show_ps_chip_info, NULL);
static ssize_t show_als_chip_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (dev == NULL) {
		pr_err("als_chip info is null\n");
		return -EINVAL;
	}
	return sprintf(buf, "%s\n", sensor_chip_info[ALS]);
}
static DEVICE_ATTR(als_info, S_IRUGO,
				   show_als_chip_info, NULL);

static ssize_t sensor_show_akm_input(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (dev == NULL) {
		pr_err("sensor_show_akm_input dev is null\n");
		return -EINVAL;
	}
	return sprintf(buf, "%s\n", sensor_binder_input[AKM]);

}
static DEVICE_ATTR(akm_input, S_IRUGO,
				   sensor_show_akm_input, NULL);



static ssize_t sensor_show_acc_input(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (dev == NULL) {
		pr_err("sensor_show_acc_input dev is null\n");
		return -EINVAL;
	}
	return sprintf(buf, "%s\n", sensor_binder_input[ACC]);
}
static DEVICE_ATTR(acc_input, S_IRUGO,
				   sensor_show_acc_input, NULL);

static ssize_t sensor_show_gyro_input(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (dev == NULL) {
		pr_err("sensor_show_gyro_input dev is null\n");
		return -EINVAL;
	}
	return sprintf(buf, "%s\n", sensor_binder_input[GYRO]);
}
static DEVICE_ATTR(gyro_input, S_IRUGO,
				   sensor_show_gyro_input, NULL);

static ssize_t sensor_show_ps_input(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (dev == NULL) {
		pr_err("sensor_show_ps_input dev is null\n");
		return -EINVAL;
	}
	return sprintf(buf, "%s\n", sensor_binder_input[PS]);
}
static DEVICE_ATTR(ps_input, S_IRUGO,
				   sensor_show_ps_input, NULL);

static ssize_t sensor_show_als_input(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (dev == NULL) {
		pr_err("sensor_show_als_input dev is null\n");
		return -EINVAL;
	}
	return sprintf(buf, "%s\n", sensor_binder_input[ALS]);
}
static DEVICE_ATTR(als_input, S_IRUGO,
				   sensor_show_als_input, NULL);

static ssize_t sensor_show_ori_input(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (dev == NULL) {
		pr_err("sensor_show_als_input dev is null\n");
		return -EINVAL;
	}
	return sprintf(buf, "%s\n", sensor_binder_input[ORI]);
}
static DEVICE_ATTR(ori_input, S_IRUGO,
				   sensor_show_ori_input, NULL);
static ssize_t show_gyro_selfTest_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (dev == NULL) {
		pr_err("gyro self tset result is null\n");
		return -EINVAL;
	}
	return sprintf(buf, "%s\n", gyro_selfTest_result);
}
static DEVICE_ATTR(gyro_selfTest, S_IRUGO,
				   show_gyro_selfTest_result, NULL);
static ssize_t show_akm_selfTest_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	if (dev == NULL) {
		pr_err("akm self tset result is null\n");
		return -EINVAL;
	}
	return sprintf(buf, "%s\n", akm_selfTest_result);
}
static ssize_t write_akm_selfTest_result(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
       unsigned long val;
	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;
	if(val) akm_selfTest_result = "1";
	else akm_selfTest_result = "0";
}
static DEVICE_ATTR(akm_selfTest, 0664,
				   show_akm_selfTest_result, write_akm_selfTest_result);
static ssize_t show_gyro_use_or_not(struct device *dev,
				struct device_attribute *attr, char *buf)
{
       char *gyro_use_or_not;
	if (dev == NULL) {
		pr_err("show_gyro_use_or_not dev is null\n");
		return -EINVAL;
	}
    gyro_use_or_not = "0";
	return sprintf(buf, "%s\n", gyro_use_or_not);
}
static DEVICE_ATTR(has_gyro, 0644,
				   show_gyro_use_or_not, NULL);
static struct attribute *apds990x_input_attributes[] = {
	&dev_attr_ps_input.attr,
	&dev_attr_als_input.attr,
	&dev_attr_acc_input.attr,
	&dev_attr_akm_input.attr,
	&dev_attr_gyro_input.attr,
	&dev_attr_ori_input.attr,
	&dev_attr_acc_info.attr,
	&dev_attr_akm_info.attr,
	&dev_attr_gyro_info.attr,
	&dev_attr_ps_info.attr,
	&dev_attr_als_info.attr,
	&dev_attr_gyro_selfTest.attr,
	&dev_attr_akm_selfTest.attr,
	&dev_attr_has_gyro.attr,
	NULL
};
static const struct attribute_group sensor_input = {
	.attrs = apds990x_input_attributes,
};
static int __init sensor_input_info_init(void)
{
	int ret = 0;
	printk(KERN_INFO"[%s] ++", __func__);

	ret = platform_device_register(&sensor_input_info);
	if (ret) {
		pr_err("%s: platform_device_register failed, ret:%d.\n",
				__func__, ret);
		goto REGISTER_ERR;
	}

	ret = sysfs_create_group(&sensor_input_info.dev.kobj, &sensor_input);
	if (ret) {
		pr_err("sensor_input_info_init sysfs_create_group error ret =%d", ret);
		goto SYSFS_CREATE_CGOUP_ERR;
	}
	printk(KERN_INFO"[%s] --", __func__);

	return 0;
SYSFS_CREATE_CGOUP_ERR:
	platform_device_unregister(&sensor_input_info);
REGISTER_ERR:
	return ret;

}

device_initcall(sensor_input_info_init);
MODULE_DESCRIPTION("sensor input info");
MODULE_AUTHOR("huawei driver group of k3v2");
MODULE_LICENSE("GPL");
