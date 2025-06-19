// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

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
#include <linux/module.h>
#include <linux/moduleparam.h>
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
#include "vpu_version.h"
#include "vpu_devices.h"
#include "vpu_mem.h"
#include "vpu_mgr.h"
#include "vpu_rm.h"

#if DEFINED_CONFIG_VENC_CNT_1to16
#include "vpu_enc.h"
#endif
#if DEFINED_CONFIG_VDEC_CNT_12345
#include "vpu_dec.h"
#endif

#ifdef CONFIG_SUPPORT_TCC_VPU
#ifdef CONFIG_OF
static const struct of_device_id vpu_mgr_of_match[] = {
	{.compatible = "telechips,vpu_dev_mgr"},	//MGR_NAME
	{}
};
MODULE_DEVICE_TABLE(of, vpu_mgr_of_match);
#endif

static struct platform_driver vmgr_driver = {
	.probe = vmgr_probe,
	.remove = vmgr_remove,
#if defined(CONFIG_PM)
	.suspend = vmgr_suspend,
	.resume = vmgr_resume,
#endif
	.driver = {
		   .name = MGR_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_mgr_of_match),
#endif
	},
};
#endif

static struct platform_device vmem_device = {
	.name = MEM_NAME,
	.dev = {},
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

static struct platform_device vrm_device = {
	.name = VRM_NAME,
	.dev = {},
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

#if DEFINED_CONFIG_VDEC_CNT_12345
static struct platform_device vdec_device = {
	.name = DEC_NAME,
	.dev = {},
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
#endif

#if DEFINED_CONFIG_VDEC_CNT_2345
static struct platform_device vdec_ext_device = {
	.name = DEC_EXT_NAME,
	.dev = {},
	.id = (int)VPU_DEC_EXT,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_vdec_ext_of_match[] = {
	{.compatible = "telechips,vpu_vdec_ext"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_vdec_ext_of_match);
#endif

static struct platform_driver vdec_ext_driver = {
	.probe = vdec_probe,
	.remove = vdec_remove,
	.driver = {
		   .name = DEC_EXT_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_vdec_ext_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VDEC_CNT_345
static struct platform_device vdec_ext2_device = {
	.name = DEC_EXT2_NAME,
	.dev = {},
	.id = (int)VPU_DEC_EXT2,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_vdec_ext2_of_match[] = {
	{.compatible = "telechips,vpu_vdec_ext2"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_vdec_ext2_of_match);
#endif

static struct platform_driver vdec_ext2_driver = {
	.probe = vdec_probe,
	.remove = vdec_remove,
	.driver = {
		   .name = DEC_EXT2_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_vdec_ext2_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VDEC_CNT_45
static struct platform_device vdec_ext3_device = {
	.name = DEC_EXT3_NAME,
	.dev = {},
	.id = (int)VPU_DEC_EXT3,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_vdec_ext3_of_match[] = {
	{.compatible = "telechips,vpu_vdec_ext3"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_vdec_ext3_of_match);
#endif

static struct platform_driver vdec_ext3_driver = {
	.probe = vdec_probe,
	.remove = vdec_remove,
	.driver = {
		   .name = DEC_EXT3_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_vdec_ext3_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VDEC_CNT_5
static struct platform_device vdec_ext4_device = {
	.name = DEC_EXT4_NAME,
	.dev = {},
	.id = (int)VPU_DEC_EXT4,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_vdec_ext4_of_match[] = {
	{.compatible = "telechips,vpu_vdec_ext4"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_vdec_ext4_of_match);
#endif

static struct platform_driver vdec_ext4_driver = {
	.probe = vdec_probe,
	.remove = vdec_remove,
	.driver = {
		   .name = DEC_EXT4_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_vdec_ext4_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VENC_CNT_1to16
static struct platform_device venc_device = {
	.name = VENC_NAME,
	.dev = {},
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
		   .name = VENC_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VENC_CNT_2to16
static struct platform_device venc_ext_device = {
	.name = VENC_EXT_NAME,
	.dev = {},
	.id = (int)VPU_ENC_EXT,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_venc_ext_of_match[] = {
	{.compatible = "telechips,vpu_venc_ext"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_venc_ext_of_match);
#endif

static struct platform_driver venc_ext_driver = {
	.probe = venc_probe,
	.remove = venc_remove,
	.driver = {
		   .name = VENC_EXT_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_ext_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VENC_CNT_3to16
static struct platform_device venc_ext2_device = {
	.name = VENC_EXT2_NAME,
	.dev = {},
	.id = (int)VPU_ENC_EXT2,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_venc_ext2_of_match[] = {
	{.compatible = "telechips,vpu_venc_ext2"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_venc_ext2_of_match);
#endif

static struct platform_driver venc_ext2_driver = {
	.probe = venc_probe,
	.remove = venc_remove,
	.driver = {
		   .name = VENC_EXT2_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_ext2_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VENC_CNT_4to16
static struct platform_device venc_ext3_device = {
	.name = VENC_EXT3_NAME,
	.dev = {},
	.id = (int)VPU_ENC_EXT3,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_venc_ext3_of_match[] = {
	{.compatible = "telechips,vpu_venc_ext3"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_venc_ext3_of_match);
#endif

static struct platform_driver venc_ext3_driver = {
	.probe = venc_probe,
	.remove = venc_remove,
	.driver = {
		   .name = VENC_EXT3_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_ext3_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VENC_CNT_5to16
static struct platform_device venc_ext4_device = {
	.name = VENC_EXT4_NAME,
	.dev = {},
	.id = (int)VPU_ENC_EXT4,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_venc_ext4_of_match[] = {
	{.compatible = "telechips,vpu_venc_ext4"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_venc_ext4_of_match);
#endif

static struct platform_driver venc_ext4_driver = {
	.probe = venc_probe,
	.remove = venc_remove,
	.driver = {
		   .name = VENC_EXT4_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_ext4_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VENC_CNT_6to16
static struct platform_device venc_ext5_device = {
	.name = VENC_EXT5_NAME,
	.dev = {},
	.id = (int)VPU_ENC_EXT5,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_venc_ext5_of_match[] = {
	{.compatible = "telechips,vpu_venc_ext5"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_venc_ext5_of_match);
#endif

static struct platform_driver venc_ext5_driver = {
	.probe = venc_probe,
	.remove = venc_remove,
	.driver = {
		   .name = VENC_EXT5_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_ext5_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VENC_CNT_7to16
static struct platform_device venc_ext6_device = {
	.name = VENC_EXT6_NAME,
	.dev = {},
	.id = (int)VPU_ENC_EXT6,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_venc_ext6_of_match[] = {
	{.compatible = "telechips,vpu_venc_ext6"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_venc_ext6_of_match);
#endif

static struct platform_driver venc_ext6_driver = {
	.probe = venc_probe,
	.remove = venc_remove,
	.driver = {
		   .name = VENC_EXT6_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_ext6_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VENC_CNT_8to16
static struct platform_device venc_ext7_device = {
	.name = VENC_EXT7_NAME,
	.dev = {},
	.id = (int)VPU_ENC_EXT7,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_venc_ext7_of_match[] = {
	{.compatible = "telechips,vpu_venc_ext7"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_venc_ext7_of_match);
#endif

static struct platform_driver venc_ext7_driver = {
	.probe = venc_probe,
	.remove = venc_remove,
	.driver = {
		   .name = VENC_EXT7_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_ext7_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VENC_CNT_9to16
static struct platform_device venc_ext8_device = {
	.name = VENC_EXT8_NAME,
	.dev = {},
	.id = (int)VPU_ENC_EXT8,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_venc_ext8_of_match[] = {
	{.compatible = "telechips,vpu_venc_ext8"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_venc_ext8_of_match);
#endif

static struct platform_driver venc_ext8_driver = {
	.probe = venc_probe,
	.remove = venc_remove,
	.driver = {
		   .name = VENC_EXT8_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_ext8_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VENC_CNT_10to16
static struct platform_device venc_ext9_device = {
	.name = VENC_EXT9_NAME,
	.dev = {},
	.id = (int)VPU_ENC_EXT9,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_venc_ext9_of_match[] = {
	{.compatible = "telechips,vpu_venc_ext9"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_venc_ext9_of_match);
#endif

static struct platform_driver venc_ext9_driver = {
	.probe = venc_probe,
	.remove = venc_remove,
	.driver = {
		   .name = VENC_EXT9_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_ext9_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VENC_CNT_11to16
static struct platform_device venc_ext10_device = {
	.name = VENC_EXT10_NAME,
	.dev = {},
	.id = (int)VPU_ENC_EXT10,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_venc_ext10_of_match[] = {
	{.compatible = "telechips,vpu_venc_ext10"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_venc_ext10_of_match);
#endif

static struct platform_driver venc_ext10_driver = {
	.probe = venc_probe,
	.remove = venc_remove,
	.driver = {
		   .name = VENC_EXT10_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_ext10_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VENC_CNT_12to16
static struct platform_device venc_ext11_device = {
	.name = VENC_EXT11_NAME,
	.dev = {},
	.id = (int)VPU_ENC_EXT11,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_venc_ext11_of_match[] = {
	{.compatible = "telechips,vpu_venc_ext11"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_venc_ext11_of_match);
#endif

static struct platform_driver venc_ext11_driver = {
	.probe = venc_probe,
	.remove = venc_remove,
	.driver = {
		   .name = VENC_EXT11_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_ext11_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VENC_CNT_13to16
static struct platform_device venc_ext12_device = {
	.name = VENC_EXT12_NAME,
	.dev = {},
	.id = (int)VPU_ENC_EXT12,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_venc_ext12_of_match[] = {
	{.compatible = "telechips,vpu_venc_ext12"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_venc_ext12_of_match);
#endif

static struct platform_driver venc_ext12_driver = {
	.probe = venc_probe,
	.remove = venc_remove,
	.driver = {
		   .name = VENC_EXT12_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_ext12_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VENC_CNT_14to16
static struct platform_device venc_ext13_device = {
	.name = VENC_EXT13_NAME,
	.dev = {},
	.id = (int)VPU_ENC_EXT13,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_venc_ext13_of_match[] = {
	{.compatible = "telechips,vpu_venc_ext13"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_venc_ext13_of_match);
#endif

static struct platform_driver venc_ext13_driver = {
	.probe = venc_probe,
	.remove = venc_remove,
	.driver = {
		   .name = VENC_EXT13_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_ext13_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VENC_CNT_15to16
static struct platform_device venc_ext14_device = {
	.name = VENC_EXT14_NAME,
	.dev = {},
	.id = (int)VPU_ENC_EXT14,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_venc_ext14_of_match[] = {
	{.compatible = "telechips,vpu_venc_ext14"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_venc_ext14_of_match);
#endif

static struct platform_driver venc_ext14_driver = {
	.probe = venc_probe,
	.remove = venc_remove,
	.driver = {
		   .name = VENC_EXT14_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_ext14_of_match),
#endif
	},
};
#endif

#if DEFINED_CONFIG_VENC_CNT_16
static struct platform_device venc_ext15_device = {
	.name = VENC_EXT15_NAME,
	.dev = {},
	.id = (int)VPU_ENC_EXT15,
};

#ifdef CONFIG_OF
static const struct of_device_id vpu_venc_ext15_of_match[] = {
	{.compatible = "telechips,vpu_venc_ext15"},
	{}
};
MODULE_DEVICE_TABLE(of, vpu_venc_ext15_of_match);
#endif

static struct platform_driver venc_ext15_driver = {
	.probe = venc_probe,
	.remove = venc_remove,
	.driver = {
		   .name = VENC_EXT15_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(vpu_venc_ext15_of_match),
#endif
	},
};
#endif

static void __exit vdev_cleanup(void)
{
#if DEFINED_CONFIG_VENC_CNT_16
	platform_driver_unregister(&venc_ext15_driver);
	platform_device_unregister(&venc_ext15_device);
#endif

#if DEFINED_CONFIG_VENC_CNT_15to16
	platform_driver_unregister(&venc_ext14_driver);
	platform_device_unregister(&venc_ext14_device);
#endif

#if DEFINED_CONFIG_VENC_CNT_14to16
	platform_driver_unregister(&venc_ext13_driver);
	platform_device_unregister(&venc_ext13_device);
#endif

#if DEFINED_CONFIG_VENC_CNT_13to16
	platform_driver_unregister(&venc_ext12_driver);
	platform_device_unregister(&venc_ext12_device);
#endif

#if DEFINED_CONFIG_VENC_CNT_12to16
	platform_driver_unregister(&venc_ext11_driver);
	platform_device_unregister(&venc_ext11_device);
#endif

#if DEFINED_CONFIG_VENC_CNT_11to16
	platform_driver_unregister(&venc_ext10_driver);
	platform_device_unregister(&venc_ext10_device);
#endif

#if DEFINED_CONFIG_VENC_CNT_10to16
	platform_driver_unregister(&venc_ext9_driver);
	platform_device_unregister(&venc_ext9_device);
#endif

#if DEFINED_CONFIG_VENC_CNT_9to16
	platform_driver_unregister(&venc_ext8_driver);
	platform_device_unregister(&venc_ext8_device);
#endif

#if DEFINED_CONFIG_VENC_CNT_8to16
	platform_driver_unregister(&venc_ext7_driver);
	platform_device_unregister(&venc_ext7_device);
#endif

#if DEFINED_CONFIG_VENC_CNT_7to16
	platform_driver_unregister(&venc_ext6_driver);
	platform_device_unregister(&venc_ext6_device);
#endif

#if DEFINED_CONFIG_VENC_CNT_6to16
	platform_driver_unregister(&venc_ext5_driver);
	platform_device_unregister(&venc_ext5_device);
#endif

#if DEFINED_CONFIG_VENC_CNT_5to16
	platform_driver_unregister(&venc_ext4_driver);
	platform_device_unregister(&venc_ext4_device);
#endif

#if DEFINED_CONFIG_VENC_CNT_4to16
	platform_driver_unregister(&venc_ext3_driver);
	platform_device_unregister(&venc_ext3_device);
#endif

#if DEFINED_CONFIG_VENC_CNT_3to16
	platform_driver_unregister(&venc_ext2_driver);
	platform_device_unregister(&venc_ext2_device);
#endif

#if DEFINED_CONFIG_VENC_CNT_2to16
	platform_driver_unregister(&venc_ext_driver);
	platform_device_unregister(&venc_ext_device);
#endif

#if DEFINED_CONFIG_VENC_CNT_1to16
	platform_driver_unregister(&venc_driver);
	platform_device_unregister(&venc_device);
#endif

#if DEFINED_CONFIG_VDEC_CNT_5
	platform_driver_unregister(&vdec_ext4_driver);
	platform_device_unregister(&vdec_ext4_device);
#endif

#if DEFINED_CONFIG_VDEC_CNT_45
	platform_driver_unregister(&vdec_ext3_driver);
	platform_device_unregister(&vdec_ext3_device);
#endif

#if DEFINED_CONFIG_VDEC_CNT_345
	platform_driver_unregister(&vdec_ext2_driver);
	platform_device_unregister(&vdec_ext2_device);
#endif

#if DEFINED_CONFIG_VDEC_CNT_2345
	platform_driver_unregister(&vdec_ext_driver);
	platform_device_unregister(&vdec_ext_device);
#endif

#if DEFINED_CONFIG_VDEC_CNT_12345
	platform_driver_unregister(&vdec_driver);
	platform_device_unregister(&vdec_device);
#endif

	platform_driver_unregister(&vrm_driver);
	platform_device_unregister(&vrm_device);

	platform_driver_unregister(&vmem_driver);
	platform_device_unregister(&vmem_device);

#ifdef CONFIG_SUPPORT_TCC_VPU
	platform_driver_unregister(&vmgr_driver);
#endif
}

static int vdev_init(void)
{
	(void)pr_info("=======> VPU Device drivers initializing!!  Start (%s) -------\n", VPU_DRIVER_VERSION);

#ifdef CONFIG_SUPPORT_TCC_VPU
	(void)platform_driver_register(&vmgr_driver);
#endif

	(void)platform_device_register(&vmem_device);
	(void)platform_driver_register(&vmem_driver);

	(void)platform_device_register(&vrm_device);
	(void)platform_driver_register(&vrm_driver);

#if DEFINED_CONFIG_VDEC_CNT_12345
	// register decoder driver...
	(void)platform_device_register(&vdec_device);
	(void)platform_driver_register(&vdec_driver);
	V_DBG(VPU_DBG_DEV_REGED, "vdec_device registered");
#endif

#if DEFINED_CONFIG_VDEC_CNT_2345
	// register decoder_ext driver...
	(void)platform_device_register(&vdec_ext_device);
	(void)platform_driver_register(&vdec_ext_driver);
	V_DBG(VPU_DBG_DEV_REGED, "vdec_ext_device registered");
#endif

#if DEFINED_CONFIG_VDEC_CNT_345
	// register decoder_ext2 driver...
	(void)platform_device_register(&vdec_ext2_device);
	(void)platform_driver_register(&vdec_ext2_driver);
	V_DBG(VPU_DBG_DEV_REGED, "vdec_ext2_device registered");
#endif

#if DEFINED_CONFIG_VDEC_CNT_45
	// register decoder_ext3 driver...
	(void)platform_device_register(&vdec_ext3_device);
	(void)platform_driver_register(&vdec_ext3_driver);
	V_DBG(VPU_DBG_DEV_REGED, "vdec_ext3_device registered");
#endif

#if DEFINED_CONFIG_VDEC_CNT_5
	// register decoder_ext4 driver...
	(void)platform_device_register(&vdec_ext4_device);
	(void)platform_driver_register(&vdec_ext4_driver);
	V_DBG(VPU_DBG_DEV_REGED, "vdec_ext4_device registered");
#endif

#if DEFINED_CONFIG_VENC_CNT_1to16
	// register encoder driver...
	(void)platform_device_register(&venc_device);
	(void)platform_driver_register(&venc_driver);
	V_DBG(VPU_DBG_DEV_REGED, "venc_device registered");
#endif

#if DEFINED_CONFIG_VENC_CNT_2to16
	// register encoder_ext driver...
	(void)platform_device_register(&venc_ext_device);
	(void)platform_driver_register(&venc_ext_driver);
	V_DBG(VPU_DBG_DEV_REGED, "venc_ext_device registered");
#endif

#if DEFINED_CONFIG_VENC_CNT_3to16
	// register encoder_ext2 driver...
	(void)platform_device_register(&venc_ext2_device);
	(void)platform_driver_register(&venc_ext2_driver);
	V_DBG(VPU_DBG_DEV_REGED, "venc_ext2_device registered");
#endif

#if DEFINED_CONFIG_VENC_CNT_4to16
	// register encoder_ext3 driver...
	(void)platform_device_register(&venc_ext3_device);
	(void)platform_driver_register(&venc_ext3_driver);
	V_DBG(VPU_DBG_DEV_REGED, "venc_ext3_device registered");
#endif

#if DEFINED_CONFIG_VENC_CNT_5to16
	// register encoder_ext4 driver...
	(void)platform_device_register(&venc_ext4_device);
	(void)platform_driver_register(&venc_ext4_driver);
	V_DBG(VPU_DBG_DEV_REGED, "venc_ext4_device registered");
#endif

#if DEFINED_CONFIG_VENC_CNT_6to16
	// register encoder_ext5 driver...
	(void)platform_device_register(&venc_ext5_device);
	(void)platform_driver_register(&venc_ext5_driver);
	V_DBG(VPU_DBG_DEV_REGED, "venc_ext5_device registered");
#endif

#if DEFINED_CONFIG_VENC_CNT_7to16
	// register encoder_ext6 driver...
	(void)platform_device_register(&venc_ext6_device);
	(void)platform_driver_register(&venc_ext6_driver);
	V_DBG(VPU_DBG_DEV_REGED, "venc_ext6_device registered");
#endif

#if DEFINED_CONFIG_VENC_CNT_8to16
	// register encoder_ext7 driver...
	(void)platform_device_register(&venc_ext7_device);
	(void)platform_driver_register(&venc_ext7_driver);
	V_DBG(VPU_DBG_DEV_REGED, "venc_ext7_device registered");
#endif

#if DEFINED_CONFIG_VENC_CNT_9to16
	// register encoder_ext8 driver...
	(void)platform_device_register(&venc_ext8_device);
	(void)platform_driver_register(&venc_ext8_driver);
	V_DBG(VPU_DBG_DEV_REGED, "venc_ext8_device registered");
#endif

#if DEFINED_CONFIG_VENC_CNT_10to16
	// register encoder_ext9 driver...
	(void)platform_device_register(&venc_ext9_device);
	(void)platform_driver_register(&venc_ext9_driver);
	V_DBG(VPU_DBG_DEV_REGED, "venc_ext9_device registered");
#endif

#if DEFINED_CONFIG_VENC_CNT_11to16
	// register encoder_ext10 driver...
	(void)platform_device_register(&venc_ext10_device);
	(void)platform_driver_register(&venc_ext10_driver);
	V_DBG(VPU_DBG_DEV_REGED, "venc_ext10_device registered");
#endif

#if DEFINED_CONFIG_VENC_CNT_12to16
	// register encoder_ext11 driver...
	(void)platform_device_register(&venc_ext11_device);
	(void)platform_driver_register(&venc_ext11_driver);
	V_DBG(VPU_DBG_DEV_REGED, "venc_ext11_device registered");
#endif

#if DEFINED_CONFIG_VENC_CNT_13to16
	// register encoder_ext12 driver...
	(void)platform_device_register(&venc_ext12_device);
	(void)platform_driver_register(&venc_ext12_driver);
	V_DBG(VPU_DBG_DEV_REGED, "venc_ext12_device registered");
#endif

#if DEFINED_CONFIG_VENC_CNT_14to16
	// register encoder_ext13 driver...
	(void)platform_device_register(&venc_ext13_device);
	(void)platform_driver_register(&venc_ext13_driver);
	V_DBG(VPU_DBG_DEV_REGED, "venc_ext13_device registered");
#endif

#if DEFINED_CONFIG_VENC_CNT_15to16
	// register encoder_ext14 driver...
	(void)platform_device_register(&venc_ext14_device);
	(void)platform_driver_register(&venc_ext14_driver);
	V_DBG(VPU_DBG_DEV_REGED, "venc_ext14_device registered");
#endif

#if DEFINED_CONFIG_VENC_CNT_16
	// register encoder_ext15 driver...
	(void)platform_device_register(&venc_ext15_device);
	(void)platform_driver_register(&venc_ext15_driver);
	V_DBG(VPU_DBG_DEV_REGED, "venc_ext15_device registered");
#endif

	(void)pr_info("Done!!\n");
	return 0;
}

module_init(vdev_init);
module_exit(vdev_cleanup);

MODULE_VERSION(VPU_DRIVER_VERSION);
MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vpu devices driver");
MODULE_LICENSE("GPL");
