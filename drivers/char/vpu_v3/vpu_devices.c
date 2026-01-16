/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#include "vpu_comm.h"
#include "vpu_devices.h"
#include "vpu_mem.h"
#include "vpu_rm.h"

#if DEFINED_CONFIG_VENC
#include "vpu_enc.h"
#endif

#if DEFINED_CONFIG_VDEC
#include "vpu_dec.h"
#endif

//DBG_INFO
#include "vpu_dbg_info.h"

static void vmem_device_release(struct device *dev)
{

}

static struct platform_device vmem_device = {
	.name = MEM_NAME,
	.dev = {
		.release = vmem_device_release,
	},
	.id = 0,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_vmem_of_match[] = {
	{.compatible = "telechips,vpu_vmem"},	//MEM_NAME
	{}
};
MODULE_DEVICE_TABLE(of, vpu_vmem_of_match);
#endif

static struct platform_driver vmem_driver = {
	.probe = vmem_probe,
	.remove = vmem_remove,
	.driver = {
		   .name = MEM_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_vmem_of_match),
#endif
	},
};

static void vrm_device_release(struct device *dev)
{

}

static struct platform_device vrm_device = {
	.name = VRM_NAME,
	.dev = {
		.release = vrm_device_release,
	},
	.id = 0,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_vrm_of_match[] = {
	{.compatible = "telechips,vpu_rm"},	//VRM_NAME
	{}
};
MODULE_DEVICE_TABLE(of, vpu_vrm_of_match);

static struct platform_driver vrm_driver = {
	.probe = vrm_probe,
	.remove = vrm_remove,
	.driver = {
		   .name = VRM_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_vrm_of_match),
#endif
	},
};
#endif

static void __exit vdev_cleanup(void)
{
	(void)platform_device_unregister(&vmem_device);
	(void)platform_driver_unregister(&vmem_driver);

	(void)platform_device_unregister(&vrm_device);
	(void)platform_driver_unregister(&vrm_driver);

	//DBG_INFO
	vpudebug_attr_deinit();
}

static int vdev_init(void)
{
	(void)pr_info("VPU Device drivers initializing!! %s\n", VPU_V3_DRIVER_VERSION);

	(void)platform_device_register(&vmem_device);
	(void)platform_driver_register(&vmem_driver);

	(void)platform_device_register(&vrm_device);
	(void)platform_driver_register(&vrm_driver);

	//DBG_INFO
	vpudebug_attr_init();

	(void)pr_info("VPU Device drivers initialize Done!!\n");
	return 0;
}

module_init(vdev_init);
module_exit(vdev_cleanup);

MODULE_VERSION(VPU_V3_DRIVER_VERSION);
MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vpu devices driver");
MODULE_LICENSE("Dual BSD/GPL"); 
