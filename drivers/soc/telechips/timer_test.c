// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/types.h>
#include <linux/sysfs.h>
#include <soc/telechips/timer_reg.h>
#include <soc/telechips/timer_api.h>

static struct kobject *tcc_timer_test;

static int tcc_timer_idx;
static int tcc_timer_register_arr[6];
static int tcc_timer_enable_arr[6];

static struct tcc_timer *test_timer[6];

static irqreturn_t tcc_timer_handler(int irq, void *data)
{
    irqreturn_t ret = IRQ_NONE;

    pr_err("Called for timer\n");

    ret = IRQ_HANDLED;

    return ret;
}

static ssize_t tcc_timer_index_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", tcc_timer_idx);
}

static ssize_t tcc_timer_index_store(struct kobject *kobj, struct kobj_attribute *attr, char *buf, size_t count)
{
    int ret;

    ret = kstrtoint(buf, 6, &tcc_timer_idx);
    if (ret < 0)
    {
        pr_err("Failed to convert string to int\n");
        return ret;
    }

    pr_err("%d\n", tcc_timer_idx);

    return count;
}

static struct kobj_attribute tcc_timer_index_attr = __ATTR(tcc_timer_index, 0660, tcc_timer_index_show, (void *)tcc_timer_index_store);

static ssize_t tcc_timer_register_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", tcc_timer_register_arr[tcc_timer_idx]);
}

static ssize_t tcc_timer_register_store(struct kobject *kobj, struct kobj_attribute *attr, char *buf, size_t count)
{
    int period;
    int ret;

    ret = kstrtoint(buf, 6, &period);
    if (ret < 0)
    {
        pr_err("Failed to convert string to int\n");
        return ret;
    }

    if ((period > 0) && (period <= 5))
    {
        test_timer[tcc_timer_idx] = tcc_register_timer(NULL, period * 1000 * 1000, tcc_timer_handler);
        tcc_timer_register_arr[tcc_timer_idx] = period;
    }

    pr_err("Success to register timer%d\n", tcc_timer_idx);

    return count;
}

static struct kobj_attribute tcc_timer_register_attr = __ATTR(tcc_timer_register, 0660, tcc_timer_register_show, (void *)tcc_timer_register_store);

static ssize_t tcc_timer_unregister_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    if (tcc_timer_register_arr[tcc_timer_idx] != 0)
    {
        return sprintf(buf, "0\n");
    }
    else
    {
        return sprintf(buf, "1\n");
    }
}

static ssize_t tcc_timer_unregister_store(struct kobject *kobj, struct kobj_attribute *attr, char *buf, size_t count)
{
    int unreg;
    int ret;

    ret = kstrtoint(buf, 2, &unreg);
    if (ret < 0)
    {
        pr_err("Failed to convert string to int\n");
        return ret;
    }

    if (unreg == 1)
    {
        tcc_unregister_timer(test_timer[tcc_timer_idx]);
        tcc_timer_register_arr[tcc_timer_idx] = 0;
    }

    pr_err("Success to unregister timer%d\n", tcc_timer_idx);

    return count;
}

static struct kobj_attribute tcc_timer_unregister_attr = __ATTR(tcc_timer_unregister, 0660, tcc_timer_unregister_show, (void *)tcc_timer_unregister_store);

static ssize_t tcc_timer_enable_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", tcc_timer_enable_arr[tcc_timer_idx]);
}

static ssize_t tcc_timer_enable_store(struct kobject *kobj, struct kobj_attribute *attr, char *buf, size_t count)
{
    int enable;
    int ret;

    ret = kstrtoint(buf, 2, &enable);
    if (ret < 0)
    {
        pr_err("Failed to convert string to int\n");
        return ret;
    }

    if (enable == 1)
    {
        tcc_timer_enable(test_timer[tcc_timer_idx]);
        tcc_timer_enable_arr[tcc_timer_idx] = 1;
    }

    pr_err("Success to enable timer%d\n", tcc_timer_idx);

    return count;
}

static struct kobj_attribute tcc_timer_enable_attr = __ATTR(tcc_timer_enable, 0660, tcc_timer_enable_show, (void *)tcc_timer_enable_store);

static ssize_t tcc_timer_disable_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    if (tcc_timer_enable_arr[tcc_timer_idx])
    {
        return sprintf(buf, "0\n");
    }
    else
    {
        return sprintf(buf, "1\n");
    }
}

static ssize_t tcc_timer_disable_store(struct kobject *kobj, struct kobj_attribute *attr, char *buf, size_t count)
{
    int disable;
    int ret;

    ret = kstrtoint(buf, 2, &disable);
    if (ret < 0)
    {
        pr_err("Failed to convert string to int\n");
        return ret;
    }

    if (disable == 1)
    {
        tcc_timer_disable(test_timer[tcc_timer_idx]);
        tcc_timer_enable_arr[tcc_timer_idx] = 0;
    }

    pr_err("Success to disable timer%d\n", tcc_timer_idx);

    return count;
}

static struct kobj_attribute tcc_timer_disable_attr = __ATTR(tcc_timer_disable, 0660, tcc_timer_disable_show, (void *)tcc_timer_disable_store);

static int __init tcc_timer_test_init(void)
{
    int ret;

    pr_emerg("%s\n", __func__);

    tcc_timer_test = kobject_create_and_add("tcc_timer_mod", kernel_kobj);
    if (!tcc_timer_test)
    {
        pr_err("Failed to add kobject for timer\n");
        return -ENOMEM;
    }

    ret = sysfs_create_file(tcc_timer_test, &tcc_timer_index_attr.attr);
    if (ret)
    {
        pr_err("Failed to create file for timer\n");
        return -ENOMEM;
    }

    ret = sysfs_create_file(tcc_timer_test, &tcc_timer_register_attr.attr);
    if (ret)
    {
        pr_err("Failed to create file for timer\n");
        return -ENOMEM;
    }

    ret = sysfs_create_file(tcc_timer_test, &tcc_timer_unregister_attr.attr);
    if (ret)
    {
        pr_err("Failed to create file for timer\n");
        return -ENOMEM;
    }

    ret = sysfs_create_file(tcc_timer_test, &tcc_timer_enable_attr.attr);
    if (ret)
    {
        pr_err("Failed to create file for timer\n");
        return -ENOMEM;
    }

    ret = sysfs_create_file(tcc_timer_test, &tcc_timer_disable_attr.attr);
    if (ret)
    {
        pr_err("Failed to create file for timer\n");
        return -ENOMEM;
    }

    return 0;
}

static void __exit tcc_timer_test_exit(void)
{
    pr_emerg("%s\n", __func__);

	return;
}

module_init(tcc_timer_test_init);
module_exit(tcc_timer_test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Telechips.com, Inc.");
MODULE_DESCRIPTION("Timer test driver");
