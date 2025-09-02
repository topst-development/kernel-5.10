/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#include "vpu_comm.h"
#include "vpu_devices.h"
#include "vpu_dbg.h"

#if DEFINED_CONFIG_VENC
#include "vpu_enc.h"
#endif

#if DEFINED_CONFIG_VDEC
#include "vpu_dec.h"
#endif

#if DEFINED_CONFIG_VDEC
static void vdec_device_release(struct device *dev)
{

}

static struct platform_device vdec_device = {
	.name = DEC_NAME,
	.dev = {
		.release = vdec_device_release,
	},
	.id = (int)VPU_DEC,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_vdec_of_match[] = {
	{.compatible = "telechips,vpu_vdec"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_vdec_of_match);
#endif

static struct platform_driver vdec_driver = {
	.probe = vdec_probe,
	.remove = vdec_remove,
	.driver = {
		   .name = DEC_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_vdec_of_match),
#endif
	},
};
#endif //#if DEFINED_CONFIG_VDEC

#if DEFINED_CONFIG_VENC
static void venc_device_release(struct device *dev)
{

}

static struct platform_device venc_device = {
	.name = ENC_NAME,
	.dev = {
		.release = venc_device_release,
	},
	.id = (int)VPU_ENC,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_venc_of_match[] = {
	{.compatible = "telechips,vpu_venc"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_venc_of_match);
#endif

static struct platform_driver venc_driver = {
	.probe = venc_probe,
	.remove = venc_remove,
	.driver = {
		   .name = ENC_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_of_match),
#endif
	},
};
#endif //#if DEFINED_CONFIG_VENC

static void __exit vpu_drv_dev_cleanup(void)
{
#if DEFINED_CONFIG_VENC
	platform_driver_unregister(&venc_driver);
	platform_device_unregister(&venc_device);
#endif

#if DEFINED_CONFIG_VDEC
	platform_driver_unregister(&vdec_driver);
	platform_device_unregister(&vdec_device);
#endif
}

static int vpu_drv_dev_init(void)
{
	(void)pr_info("VPU Encode/Decode drivers initializing!! %s\n", VPU_V3_DRIVER_VERSION);

#if DEFINED_CONFIG_VDEC
	V_DBG(VPU_DBG_INFO, "vpu vdec register");
	// register decoder driver...
	(void)platform_device_register(&vdec_device);
	(void)platform_driver_register(&vdec_driver);
	V_DBG(VPU_DBG_DEV_REGED, "vdec_device registered");
#endif

#if DEFINED_CONFIG_VENC
	V_DBG(VPU_DBG_INFO, "vpu venc register");
	// register encoder driver...
	(void)platform_device_register(&venc_device);
	(void)platform_driver_register(&venc_driver);
	V_DBG(VPU_DBG_DEV_REGED, "venc_device registered");
#endif

	(void)pr_info("VPU Encode/Decode drivers initialize Done!!\n");
	return 0;
}

module_init(vpu_drv_dev_init);
module_exit(vpu_drv_dev_cleanup);

MODULE_VERSION(VPU_V3_DRIVER_VERSION);
MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vpu devices driver");
MODULE_LICENSE("Dual BSD/GPL"); 
