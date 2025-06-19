// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include "vpu_comm.h"
#include "vpu_version.h"

#ifdef CONFIG_SUPPORT_TCC_JPU
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/uaccess.h>

#include <linux/io.h>
#include <asm/div64.h>

#include "vpu_devices.h"
#include "jpu_mgr.h"


#ifdef CONFIG_OF
static const struct of_device_id jmgr_of_match[] = {
	{.compatible = "telechips,jpu_dev_mgr"},	//JMGR_NAME
	{}
};
MODULE_DEVICE_TABLE(of, jmgr_of_match);
#endif

static struct platform_driver jmgr_driver = {
	.probe = jmgr_probe,
	.remove = jmgr_remove,
#if defined(CONFIG_PM)
	.suspend = jmgr_suspend,
	.resume = jmgr_resume,
#endif
	.driver = {
		   .name = JMGR_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(jmgr_of_match),
#endif
	},
};

static void __exit jmgr_cleanup(void)
{
	platform_driver_unregister(&jmgr_driver);
}

static int jmgr_init(void)
{
	int ret = 0;

	(void)pr_info("============> JPU device drivers initializing!!  Start (%s) -------\n", VPU_DRIVER_VERSION);

	(void)platform_driver_register(&jmgr_driver);

	(void)pr_info("Done!!\n");
	return ret;
}

module_init(jmgr_init);
module_exit(jmgr_cleanup);
#endif

MODULE_VERSION(VPU_DRIVER_VERSION);
MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC jpu devices driver");
MODULE_LICENSE("GPL");
