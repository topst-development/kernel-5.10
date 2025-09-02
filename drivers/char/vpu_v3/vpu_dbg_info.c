/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#include "vpu_dbg_info.h"

int gs_iDebugTimeMeasure = INITIAL_ZERO;
int gs_iDebugCqCount = INITIAL_ZERO;
int gs_iDebugLogInfo = INITIAL_ZERO;

#define KERN_LOCAL_LOG_LEVEL KERN_WARNING

static struct kobject *vpudebug_kobj = INITIAL_NULL;
static atomic_t atomicVpuTimeMeasure;
static atomic_t atomicVpuCqCount;
static atomic_t atomicVpuLogInfo;

// TIME MEASURE
//  usage: echo {value} > /sys/kernel/vpudebug/vpu_time_measure //{value}: 0,1,2
// VPU CQ Count
//  usage: echo {value} > /sys/kernel/vpudebug/vpu_cq_count //{value}: 0,1,2
// VPU Log Info
//  usage: echo {value} > /sys/kernel/vpudebug/vpu_log_info //{value}:  0 : 0
//																		1 : 1
//																		2 : 1X
//																		4 : 1XX
//																		8 : 1XXX
//																		16: 1XXXX for vp9_superframe

//-------------------------------------------------------------------------------
static void vpudebug_attr_time_measure_set(int value)
{
	printk(KERN_LOCAL_LOG_LEVEL "%s : time measure = %d \n", __func__, value);
	atomic_set(&atomicVpuTimeMeasure, value);
	//vpudebug_set_attr_vpu_4kd2_time_measure(value); //vpu_4kd2_mgr
	gs_iDebugTimeMeasure = value;
}
static void vpudebug_attr_cq_count_set(int value)
{
	printk(KERN_LOCAL_LOG_LEVEL "%s : cq_count = %d \n", __func__, value);
	atomic_set(&atomicVpuCqCount, value);
	//vpudebug_set_attr_vpu_4kd2_cq_count(value); //vpu_4kd2_mgr
	gs_iDebugCqCount = value;
}
static void vpudebug_attr_log_info_set(int value)
{
	printk(KERN_LOCAL_LOG_LEVEL "%s : log_info = %d \n", __func__, value);
	atomic_set(&atomicVpuLogInfo, value);
	//vpudebug_set_attr_vpu_dec_log_info(value);  //vpu_dec
	//vpudebug_set_attr_vpu_4kd2_log_info(value); //vpu_4kd2_mgr
	gs_iDebugLogInfo = value;
}
//-------------------------------------------------------------------------------
static ssize_t vpudebug_attr_time_measure_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&atomicVpuTimeMeasure));
}
static ssize_t vpudebug_attr_cq_count_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&atomicVpuCqCount));
}
static ssize_t vpudebug_attr_log_info_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&atomicVpuLogInfo));
}

//-------------------------------------------------------------------------------
static ssize_t vpudebug_attr_time_measure_store(
	struct device *dev, struct device_attribute *attr, const char *buf,
	size_t count)
{
	unsigned long data = 0;

	data = simple_strtoul(buf, NULL, 0);
	printk(KERN_LOCAL_LOG_LEVEL "%s : data = %lu (0x%lx) \n", __func__, data, data);
	vpudebug_attr_time_measure_set(data);
	atomic_set(&atomicVpuTimeMeasure, (int)data);
	return count;
}
static ssize_t vpudebug_attr_cq_count_store(
	struct device *dev, struct device_attribute *attr, const char *buf,
	size_t count)
{
	unsigned long data = 0;

	data = simple_strtoul(buf, NULL, 0);
	printk(KERN_LOCAL_LOG_LEVEL "%s : data = %lu (0x%lx) \n", __func__, data, data);
	vpudebug_attr_cq_count_set(data);
	atomic_set(&atomicVpuCqCount, (int)data);
	return count;
}
static ssize_t vpudebug_attr_log_info_store(
	struct device *dev, struct device_attribute *attr, const char *buf,
	size_t count)
{
	unsigned long data = 0;

	data = simple_strtoul(buf, NULL, 0);
	printk(KERN_LOCAL_LOG_LEVEL "%s : data = %lu (0x%lx) \n", __func__, data, data);
	vpudebug_attr_log_info_set(data);
	atomic_set(&atomicVpuLogInfo, (int)data);
	return count;
}

//-------------------------------------------------------------------------------
static DEVICE_ATTR(vpu_time_measure, S_IRUGO | S_IWUSR | S_IWGRP,
	vpudebug_attr_time_measure_show, vpudebug_attr_time_measure_store);
static DEVICE_ATTR(vpu_cq_count, S_IRUGO | S_IWUSR | S_IWGRP,
	vpudebug_attr_cq_count_show, vpudebug_attr_cq_count_store);
static DEVICE_ATTR(vpu_log_info, S_IRUGO | S_IWUSR | S_IWGRP,
	vpudebug_attr_log_info_show, vpudebug_attr_log_info_store);

//-------------------------------------------------------------------------------
static struct attribute *vpudebug_attributes[] = {
	&dev_attr_vpu_time_measure.attr,
	&dev_attr_vpu_cq_count.attr,
	&dev_attr_vpu_log_info.attr,
	NULL
};
static struct attribute_group vpudebug_attribute_group = {
	.attrs = vpudebug_attributes
};

//-------------------------------------------------------------------------------
void vpudebug_attr_init(void)
{
	if (vpudebug_kobj == NULL) {
		int err = 0;
		vpudebug_kobj = kobject_create_and_add("vpudebug", kernel_kobj);
		if (!vpudebug_kobj) {
			printk(KERN_EMERG "\x1b[47m \x1b[31m %s kobject_create_and_add failed err %d \x1b[0m\n", __func__, err);
			return;
		}

		vpudebug_attr_time_measure_set(0); // default
		//printk(KERN_LOCAL_LOG_LEVEL "%s %d \n", __func__, __LINE__);
		vpudebug_attr_cq_count_set(0);	   // default
		//printk(KERN_LOCAL_LOG_LEVEL "%s %d \n", __func__, __LINE__);
		vpudebug_attr_log_info_set(0);	   // default
		//printk(KERN_LOCAL_LOG_LEVEL "%s %d \n", __func__, __LINE__);

		err = sysfs_create_group(
			vpudebug_kobj, &vpudebug_attribute_group);
		if (err) {
			kobject_put(vpudebug_kobj);
		}

		if (err < 0) {
			printk(KERN_EMERG "\x1b[47m \x1b[31m %s sysfs_create_group failed err %d \x1b[0m\n", __func__, err);
		}
	}
}

void vpudebug_attr_deinit(void)
{
	if (vpudebug_kobj != NULL) {
		sysfs_remove_group(vpudebug_kobj, &vpudebug_attribute_group);

		kobject_del(vpudebug_kobj);
		kobject_put(vpudebug_kobj);
		vpudebug_kobj = NULL;
	}
}