// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include "vpu_comm.h"
#include "vpu_version.h"

#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC

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
//#include <asm/io.h>
#include <asm/div64.h>

#include "vpu_comm.h"
#include "vpu_devices.h"
#include "hevc_mgr.h"


#ifdef CONFIG_OF
static const struct of_device_id hmgr_of_match[] = {
	{.compatible = "telechips,hevc_dev_mgr"},	//HMGR_NAME
	{}
};
MODULE_DEVICE_TABLE(of, hmgr_of_match);
#endif

static struct platform_driver hmgr_driver = {
	.probe = hmgr_probe,
	.remove = hmgr_remove,
#if defined(CONFIG_PM)
	.suspend = hmgr_suspend,
	.resume = hmgr_resume,
#endif
	.driver = {
		   .name = HMGR_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(hmgr_of_match),
#endif
	},
};

static void __exit hmgr_cleanup(void)
{
	platform_driver_unregister(&hmgr_driver);
}

static int hmgr_init(void)
{
	(void)pr_info("============> HEVC Devices drivers initializing!!  Start (%s) -------\n", VPU_DRIVER_VERSION);

	(void)platform_driver_register(&hmgr_driver);

	(void)pr_info("Done!!\n");
	return 0;
}

module_init(hmgr_init);
module_exit(hmgr_cleanup);
#endif


MODULE_VERSION(VPU_DRIVER_VERSION);
MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC hevc devices driver");
MODULE_LICENSE("GPL");
