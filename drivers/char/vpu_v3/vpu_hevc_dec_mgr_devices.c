/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#include "vpu_comm.h"

#ifdef ENABLE_VPU_DRV_HEVCDEC

#include "vpu_devices.h"
#include "vpu_hevc_dec_mgr.h"


#ifdef CONFIG_OF
static const struct of_device_id vpu_hevc_dec_of_match[] = {
	{.compatible = "telechips,hevc_dev_mgr"},	//HMGR_NAME
	{}
};

MODULE_DEVICE_TABLE(of, vpu_hevc_dec_of_match);
#endif

static struct platform_driver vpu_hevc_dec_driver = {
	.probe = vmgr_hevc_dec_probe,
	.remove = vmgr_hevc_dec_remove,
#if defined(CONFIG_PM)
	.suspend = vmgr_hevc_dec_suspend,
	.resume = vmgr_hevc_dec_resume,
#endif
	.driver = {
		   .name = HMGR_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_hevc_dec_of_match),
#endif
	},
};

static void __exit vpu_hevc_dec_cleanup(void)
{
	platform_driver_unregister(&vpu_hevc_dec_driver);
}

static int vpu_hevc_dec_init(void)
{
	(void)pr_info("HEVC Devices drivers initializing!! %s\n", VPU_V3_DRIVER_VERSION);

	(void)platform_driver_register(&vpu_hevc_dec_driver);

	(void)pr_info("HEVC Devices drivers initialize Done!!\n");
	return 0;
}

module_init(vpu_hevc_dec_init);
module_exit(vpu_hevc_dec_cleanup);
#endif //ENABLE_VPU_DRV_HEVCDEC

MODULE_VERSION(VPU_V3_DRIVER_VERSION);
MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC hevc devices driver");
MODULE_LICENSE("Dual BSD/GPL"); 


