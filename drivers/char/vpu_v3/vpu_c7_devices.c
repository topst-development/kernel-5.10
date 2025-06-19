// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_comm.h"

#ifdef ENABLE_VPU_DRV_VPU_C7
#include "vpu_devices.h"
#include "vpu_c7_mgr.h"

#ifdef CONFIG_OF
static const struct of_device_id vmgr_c7_of_match[] = {
	{.compatible = "telechips,vpu_dev_mgr"},	//MGR_NAME
	{}
};
MODULE_DEVICE_TABLE(of, vmgr_c7_of_match);
#endif

static struct platform_driver vmgr_c7_driver = {
	.probe = vmgr_c7_probe,
	.remove = vmgr_c7_remove,
#if defined(CONFIG_PM)
	.suspend = vmgr_c7_suspend,
	.resume = vmgr_c7_resume,
#endif
	.driver = {
		   .name = VPU_C7_MGR_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vmgr_c7_of_match),
#endif
	},
};

static void __exit vpu_c7_cleanup(void)
{
	platform_driver_unregister(&vmgr_c7_driver);
}

static int vpu_c7_init(void)
{
	(void)pr_info("VPU C7 device drivers initializing!! %s\n", VPU_V3_DRIVER_VERSION);

	platform_driver_register(&vmgr_c7_driver);

	(void)pr_info("VPU C7 device drivers initialize Done!!\n");
	return 0;
}

module_init(vpu_c7_init);
module_exit(vpu_c7_cleanup);
#endif //ENABLE_VPU_DRV_VPU_C7

MODULE_VERSION(VPU_V3_DRIVER_VERSION);
MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC VPU c7 devices driver");
MODULE_LICENSE("GPL");


