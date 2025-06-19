// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_comm.h"

#ifdef ENABLE_VPU_DRV_4K_D2
#include "vpu_devices.h"
#include "vpu_4k_d2_mgr.h"

#ifdef CONFIG_OF
static const struct of_device_id vpu_4k_d2_of_match[] = {
	{.compatible = "telechips,vpu_4k_d2_dev_mgr"},	//VPU_4K_D2_MGR_NAME
	{}
};
MODULE_DEVICE_TABLE(of, vpu_4k_d2_of_match);
#endif

static struct platform_driver vpu_4k_d2_driver = {
	.probe = vmgr_4k_d2_probe,
	.remove = vmgr_4k_d2_remove,
#if defined(CONFIG_PM)
	.suspend = vmgr_4k_d2_suspend,
	.resume = vmgr_4k_d2_resume,
#endif
	.driver = {
		   .name = VPU_4K_D2_MGR_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_4k_d2_of_match),
#endif
	},
};

static void __exit vpu_4k_d2_cleanup(void)
{
	platform_driver_unregister(&vpu_4k_d2_driver);
}

static int vpu_4k_d2_init(void)
{
	(void)pr_info("4K-D2(VP9/HEVC) device drivers initializing!! %s\n", VPU_V3_DRIVER_VERSION);

	(void)platform_driver_register(&vpu_4k_d2_driver);

	(void)pr_info("4K-D2(VP9/HEVC) device drivers initialize Done!!\n");
	return 0;
}

module_init(vpu_4k_d2_init);
module_exit(vpu_4k_d2_cleanup);
#endif //ENABLE_VPU_DRV_4K_D2

MODULE_VERSION(VPU_V3_DRIVER_VERSION);
MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC 4K-D2 VP9/HEVC devices driver");
MODULE_LICENSE("GPL");

