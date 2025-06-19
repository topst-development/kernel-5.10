// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <video/telechips/vioc_global.h>
#include <video/telechips/vioc_lut_3d.h>
#include <video/telechips/tcc_lut_3d_ioctl.h>

/* Version */
#define LUT_3D_VERSION "v1.1"
#define LUT_3D_UPDATE_DATE "20240229"

struct lut_3d_drv_vioc {
	void __iomem *reg;
	unsigned int id;
};

struct lut_3d_drv_type {
	struct miscdevice 		*misc;
	unsigned int			id;

	struct lut_3d_drv_vioc lut_drv;
};

static long lut_3d_drv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = -EFAULT;
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct lut_3d_drv_type	*lut_3d = dev_get_drvdata(misc->parent);

	switch (cmd) {
	case TCC_LUT_3D_SET_TABLE:
		{
			struct VIOC_LUT_3D_SET_TABLE *lut_cmd = NULL;

			lut_cmd = kzalloc(sizeof(struct VIOC_LUT_3D_SET_TABLE), GFP_KERNEL);
			if(lut_cmd == NULL) {
				ret = -ENOMEM;
			} else {
				ret = 0;
			}

			if (ret == 0) {
				if ((bool)copy_from_user((void *)lut_cmd, (const void *)arg, sizeof(struct VIOC_LUT_3D_SET_TABLE))) {
					ret = -ENOSYS;
				}
			}

			if (ret == 0) {
				ret = vioc_lut_3d_set_table(lut_3d->lut_drv.id, lut_cmd->table);
				if (ret < 0) {
					(void)pr_err("[ERR][3D LUT] %s TCC_LUT_3D_SET_TABLE Fail[%d]\n", __func__, ret);
					ret = -EINVAL;
				} else {
					ret = 0;
				}
			}

			if (lut_cmd != NULL) {
				kfree(lut_cmd);
			}
		}
		break;
	case TCC_LUT_3D_ONOFF:
		{
			struct VIOC_LUT_3D_ONOFF lut_cmd = { 0 };
			
			if ((bool)copy_from_user((void *)&lut_cmd, (const void *)arg, sizeof(struct VIOC_LUT_3D_ONOFF))) {
				(void)pr_err("[ERR][3D LUT] %s TCC_LUT_PLUG_IN failed copy from user\n",__func__);
				ret = -ENOSYS;
			} else {
				ret = 0;
			}

			if (ret == 0) {
				ret = vioc_lut_3d_bypass(lut_3d->lut_drv.id, lut_cmd.lut_3d_onoff);
				if (ret < 0) {
					(void)pr_err("[ERR][3D LUT] %s TCC_LUT_3D_ONOFF Fail[%d]\n", __func__, ret);
					ret = -EINVAL;
				} else {
					ret = 0;
				}
			}
		}
		break;
	default:
		(void)pr_err("[ERR][3D LUT] not supported 3D LUT IOCTL\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int lut_3d_drv_open(struct inode *inode, struct file *filp)
{
	(void)inode;
	(void)filp;
	return 0;
}

static int lut_3d_drv_release(struct inode *inode, struct file *filp)
{
	(void)inode;
	(void)filp;
	return 0;
}

static const struct file_operations lut_3d_drv_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = lut_3d_drv_ioctl,
	.open = lut_3d_drv_open,
	.release = lut_3d_drv_release,
};

static int lut_3d_drv_probe(struct platform_device *pdev)
{
	struct lut_3d_drv_type *lut_3d = NULL;
	struct device_node *dev_np;
	unsigned int index = 0U;
	int ret = -EFAULT;
	int itemp = -1;

	lut_3d = kzalloc(sizeof(struct lut_3d_drv_type), GFP_KERNEL);
	if (lut_3d == NULL) {
		(void)pr_err( "[ERR][3D LUT] %s: out of memory\n", __func__);
		ret = -ENOMEM;
	} else {
		ret = 0;
	}

	if (ret == 0) {
		lut_3d->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
		if (lut_3d->misc == NULL) {
			(void)pr_err( "[ERR][3D LUT] %s: misc out of memory\n", __func__);
			kfree(lut_3d);
			ret = -ENOMEM;
		}
	}

	if (ret == 0) {
		itemp = of_alias_get_id(pdev->dev.of_node, "tcc-lut-3d-drv");
		if(itemp >= 0) {
			lut_3d->id = (unsigned int)itemp;
		} else {
			(void)pr_err("[ERR][3D LUT] 3D_LUT alias not define\n");
			kfree(lut_3d->misc);
			kfree(lut_3d);
			ret = -EINVAL;
		}
	}

	if (ret == 0) {
		lut_3d->misc->minor = MISC_DYNAMIC_MINOR;
		lut_3d->misc->fops = &lut_3d_drv_fops;
		lut_3d->misc->name = kasprintf(GFP_KERNEL, "tcc_lut_3d_d%d", lut_3d->id);
		lut_3d->misc->parent = &pdev->dev;
		ret = misc_register(lut_3d->misc);
		if (ret < 0) {
			kfree(lut_3d->misc);
			kfree(lut_3d);
			ret = -EINVAL;
		} else {
			ret = 0;
		}
	}

	if (ret == 0) {
		dev_np = of_parse_phandle(pdev->dev.of_node, "lut_3d", 0);
		if (dev_np != NULL) {
			(void)of_property_read_u32_index(pdev->dev.of_node, "lut_3d", 1, &index);
			lut_3d->lut_drv.reg = VIOC_LUT_3D_GetAddress(index);
			lut_3d->lut_drv.id = index;
		} else {
			(void)pr_err("[ERR][3D LUT] could not find 3D LUT node of %s driver.\n", lut_3d->misc->name);
			misc_deregister(lut_3d->misc);
			kfree(lut_3d->misc);
			kfree(lut_3d);
			ret = -EINVAL;
		}
	}
	
	if (ret == 0) {
		platform_set_drvdata(pdev, lut_3d);
		(void)pr_info("[INF][LUT 3D] %s: :%s, Driver %s Initialized id:0x%04X\n", __func__, LUT_3D_VERSION, lut_3d->misc->name, lut_3d->lut_drv.id);
	}

	return ret;
}

static int lut_3d_drv_remove(struct platform_device *pdev)
{
	const struct lut_3d_drv_type *lut_3d = (struct lut_3d_drv_type *)platform_get_drvdata(pdev);

	misc_deregister(lut_3d->misc);
	kfree(lut_3d->misc);
	kfree(lut_3d);

	return 0;
}

static const struct of_device_id lut_3d_of_match[] = {
	{.compatible = "telechips,tcc_lut_3d_drv"},
	{}
};
MODULE_DEVICE_TABLE(of, lut_3d_of_match);

static struct platform_driver lut_3d_driver = {
	.probe = lut_3d_drv_probe,
	.remove = lut_3d_drv_remove,
	.driver = {
		.name = "tcc_lut_3d_drv",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(lut_3d_of_match),
#endif
	},
};

static int __init lut_3d_drv_init(void)
{
	return platform_driver_register(&lut_3d_driver);
}

static void __exit lut_3d_drv_exit(void)
{
	platform_driver_unregister(&lut_3d_driver);
}

module_init(lut_3d_drv_init);
module_exit(lut_3d_drv_exit);

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("Telechips 3D look up table Driver");
MODULE_LICENSE("GPL");
