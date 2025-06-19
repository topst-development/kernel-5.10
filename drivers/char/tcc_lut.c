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
#include <linux/io.h>
#include <linux/limits.h>
#include <video/telechips/vioc_global.h>
#include <video/telechips/vioc_lut.h>
#include <video/telechips/tcc_lut_ioctl.h>

/* Version */
#define LUT_VERSION "v1.9"
#define LUT_UPDATE_DATE "20240228"

struct lut_drv_type {
	unsigned int dev_opened;
	struct miscdevice *misc;
};

static int lut_mapping_data[PLUGIN_INDEX_MAX];

static void lut_drv_fill_mapping_table(void)
{
	unsigned int i = 0U;

	for (i = 0 ; i < PLUGIN_INDEX_MAX ; i++) {
		lut_mapping_data[i] = -1;
	}

#if defined(VIOC_RDMA00)
	lut_mapping_data[PLUGIN_INDEX_RDMA00] = VIOC_RDMA00;
#endif
#if defined(VIOC_RDMA01)
	lut_mapping_data[PLUGIN_INDEX_RDMA01] = VIOC_RDMA01;
#endif
#if defined(VIOC_RDMA02)
	lut_mapping_data[PLUGIN_INDEX_RDMA02] = VIOC_RDMA02;
#endif
#if defined(VIOC_RDMA03)
	lut_mapping_data[PLUGIN_INDEX_RDMA03] = VIOC_RDMA03;
#endif
#if defined(VIOC_RDMA04)
	lut_mapping_data[PLUGIN_INDEX_RDMA04] = VIOC_RDMA04;
#endif
#if defined(VIOC_RDMA05)
	lut_mapping_data[PLUGIN_INDEX_RDMA05] = VIOC_RDMA05;
#endif
#if defined(VIOC_RDMA06)
	lut_mapping_data[PLUGIN_INDEX_RDMA06] = VIOC_RDMA06;
#endif
#if defined(VIOC_RDMA07)
	lut_mapping_data[PLUGIN_INDEX_RDMA07] = VIOC_RDMA07;
#endif
#if defined(VIOC_RDMA08)
	lut_mapping_data[PLUGIN_INDEX_RDMA08] = VIOC_RDMA08;
#endif
#if defined(VIOC_RDMA09)
	lut_mapping_data[PLUGIN_INDEX_RDMA09] = VIOC_RDMA09;
#endif
#if defined(VIOC_RDMA10)
	lut_mapping_data[PLUGIN_INDEX_RDMA10] = VIOC_RDMA10;
#endif
#if defined(VIOC_RDMA11)
	lut_mapping_data[PLUGIN_INDEX_RDMA11] = VIOC_RDMA11;
#endif
#if defined(VIOC_RDMA12)
	lut_mapping_data[PLUGIN_INDEX_RDMA12] = VIOC_RDMA12;
#endif
#if defined(VIOC_RDMA13)
	lut_mapping_data[PLUGIN_INDEX_RDMA13] = VIOC_RDMA13;
#endif
#if defined(VIOC_RDMA14)
	lut_mapping_data[PLUGIN_INDEX_RDMA14] = VIOC_RDMA14;
#endif
#if defined(VIOC_RDMA15)
	lut_mapping_data[PLUGIN_INDEX_RDMA15] = VIOC_RDMA15;
#endif
#if defined(VIOC_VIN00)
	lut_mapping_data[PLUGIN_INDEX_VIN00] = VIOC_VIN00;
#endif
#if defined(VIOC_RDMA16)
	lut_mapping_data[PLUGIN_INDEX_RDMA16] = VIOC_RDMA16;
#endif
#if defined(VIOC_VIN01)
	lut_mapping_data[PLUGIN_INDEX_VIN01] = VIOC_VIN01;
#endif
#if defined(VIOC_RDMA17)
	lut_mapping_data[PLUGIN_INDEX_RDMA17] = VIOC_RDMA17;
#endif
#if defined(VIOC_WDMA00)
	lut_mapping_data[PLUGIN_INDEX_WDMA00] = VIOC_WDMA00;
#endif
#if defined(VIOC_WDMA01)
	lut_mapping_data[PLUGIN_INDEX_WDMA01] = VIOC_WDMA01;
#endif
#if defined(VIOC_WDMA02)
	lut_mapping_data[PLUGIN_INDEX_WDMA02] = VIOC_WDMA02;
#endif
#if defined(VIOC_WDMA03)
	lut_mapping_data[PLUGIN_INDEX_WDMA03] = VIOC_WDMA03;
#endif
#if defined(VIOC_WDMA04)
	lut_mapping_data[PLUGIN_INDEX_WDMA04] = VIOC_WDMA04;
#endif
#if defined(VIOC_WDMA05)
	lut_mapping_data[PLUGIN_INDEX_WDMA05] = VIOC_WDMA05;
#endif
#if defined(VIOC_WDMA06)
	lut_mapping_data[PLUGIN_INDEX_WDMA06] = VIOC_WDMA06;
#endif
#if defined(VIOC_WDMA07)
	lut_mapping_data[PLUGIN_INDEX_WDMA07] = VIOC_WDMA07;
#endif
#if defined(VIOC_WDMA08)
	lut_mapping_data[PLUGIN_INDEX_WDMA08] = VIOC_WDMA08;
#endif
}

/* Internal APIs */
static unsigned int lut_get_real_lut_table_number(unsigned int input_lut_number)
{
	unsigned int lut_number = VIOC_NO_COMPONENT;

	switch (input_lut_number) {
	case LUT_DEV0:
		lut_number = VIOC_LUT_DEV0;
		break;
#if !defined(CONFIG_ARCH_TCC750X)
	case LUT_DEV1:
		lut_number = VIOC_LUT_DEV1;
		break;
	case LUT_DEV2:
		lut_number = VIOC_LUT_DEV2;
		break;
	#if defined(CONFIG_ARCH_TCC805X)
	case LUT_DEV3:
		lut_number = VIOC_LUT_DEV3;
		break;
	#if defined(CONFIG_ARCH_TCC807X)
	case LUT_DEV4:
		lut_number = VIOC_LUT_DEV4;
		break;
	#endif
	#endif
#endif
	case LUT_COMP0:
		lut_number = VIOC_LUT_COMP0;
		break;
#if !defined(CONFIG_ARCH_TCC750X)
	case LUT_COMP1:
		lut_number = VIOC_LUT_COMP1;
		break;
#endif
	default:
		break;
	}

	return lut_number;
}

/*
 * This api plugin RDMA/WDMA/VIN to LUT
 */
static int lut_drv_set_plugin(const struct lut_drv_type *lut, unsigned int lut_number,
			      unsigned int plugin, unsigned int plug_in_ch)
{
	int ret = 0;
	int is_dev = -1;

	unsigned int plugComp = 0U;

	if (lut == NULL) {
		ret = -EINVAL;
	}

	if (ret == 0) {
		(void)lut_get_address(lut_number, &is_dev);

		if (is_dev != LUT_TYPE_COMP) {
			(void)pr_err("[ERR][LUT] %s lut number %d is out of range\n", __func__, lut_number);
			ret = -EINVAL;
		}
	}

	if (ret == 0) {
		if (plugin == 0U) {
			if ((lut_number >= VIOC_LUT) && (lut_number < (VIOC_LUT + VIOC_LUT_MAX))) {
				tcc_set_lut_enable(lut_number, 0);
			} else {
				(void)pr_err("[ERR][LUT] %s lut number %d is out of range\n", __func__, lut_number);
				ret = -EINVAL;
			}
		} else {
			if ((plug_in_ch >= (unsigned int)PLUGIN_INDEX_MAX)) {
				(void)pr_err("[ERR][LUT] %s (%d) is out of range on LUT\n", __func__, plug_in_ch);
				ret = -EINVAL;
			} else {
				plugComp = clamp_t(u32, (lut_mapping_data[plug_in_ch]) , 0, INT_MAX);

				if ((lut_number >= VIOC_LUT) && (lut_number < (VIOC_LUT + VIOC_LUT_MAX))) {
					if (tcc_set_lut_plugin(lut_number, plugComp) < 0) {
						(void)pr_err("[ERR][LUT] %s lut number %d is out of range\n", __func__, lut_number);
						ret = -EINVAL;
					} else {
						tcc_set_lut_enable(lut_number, 1);
					}
				} else {
					(void)pr_err("[ERR][LUT] %s lut number %d is out of range\n", __func__, lut_number);
					ret = -EINVAL;
				}
			}
		}
	}

	return ret;
}

static int lut_drv_set_onoff(struct lut_drv_type *lut, unsigned int lut_number, unsigned int onoff)
{
	int ret = 0;

	if (lut == NULL) {
		ret = -EFAULT;
	}

	if(ret == 0) {
		if ((lut_number >= VIOC_LUT) && (lut_number < (VIOC_LUT + VIOC_LUT_MAX))) {
			tcc_set_lut_enable((lut_number), onoff);
		} else {
			ret = -EINVAL;
		}
	}

	return ret;
}

static long lut_drv_ioctl(struct file *filp, unsigned int cmd,
			  unsigned long arg)
{
	int ret = -EFAULT;
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct lut_drv_type *lut = dev_get_drvdata(misc->parent);

	switch (cmd) {
		case TCC_LUT_SET:
			{
				struct VIOC_LUT_VALUE_SET *lut_cmd = NULL;
				unsigned int lut_number = 0U;

				lut_cmd = kzalloc(sizeof(struct VIOC_LUT_VALUE_SET), GFP_KERNEL);
				if (lut_cmd == NULL) {
					(void)pr_err("[ERR][LUT] %s TCC_LUT_SET_EX out of memory\n", __func__);
					ret = -ENOMEM;
				} else {
					ret = 0;
				}
				
				if (ret == 0) {
					if ((bool)copy_from_user((void *)lut_cmd, (const void *)arg, sizeof(struct VIOC_LUT_VALUE_SET))) {
						(void)pr_err("[ERR][LUT] %s TCC_LUT_SET failed copy from user\n", __func__);
						ret = -ENOSYS;
					}
				}

				if (ret == 0) {
					lut_number = lut_get_real_lut_table_number(lut_cmd->lut_number);
					if (lut_number == VIOC_NO_COMPONENT) {
						(void)pr_err("[ERR][LUT] %s TCC_LUT_SET invalid lut number[%d]\n", __func__, lut_cmd->lut_number);
						ret = -EINVAL;
					}
				}

				if (ret == 0) {
					ret = tcc_set_lut_table(lut_number,lut_cmd->Gamma);
					if (ret < 0) {
						(void)pr_err("[ERR][LUT] %s TCC_LUT_SET Fail[%d]\n", __func__, ret);
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
		case TCC_LUT_SET_EX:
			{
				int lut_number;
				struct VIOC_LUT_VALUE_SET_EX *lut_value_set_ex = NULL;

				lut_value_set_ex = kzalloc(sizeof(struct VIOC_LUT_VALUE_SET_EX), GFP_KERNEL);
				if (lut_value_set_ex == NULL) {
					(void)pr_err( "[ERR][LUT] %s TCC_LUT_SET_EX out of memory\n", __func__);
					ret = -ENOMEM;
				} else {
					ret = 0;
				}

				if(ret == 0) {
					if ((bool)copy_from_user((void *)lut_value_set_ex, (const void *)arg, sizeof(struct VIOC_LUT_VALUE_SET_EX))) {
						(void)pr_err("[ERR][LUT] %s TCC_LUT_SET_EX failed copy from user\n", __func__);
						ret = -ENOSYS;
					}
				}

				if(ret == 0) {
					if (LUT_TABLE_SIZE != lut_value_set_ex->lut_size) {
						(void)pr_err("[ERR][LUT] %s TCC_LUT_SET_EX table size mismatch %d != %d\n", __func__, LUT_TABLE_SIZE, lut_value_set_ex->lut_size);
						ret = -EINVAL;
					}
				}

				if(ret == 0) {
					lut_number = lut_get_real_lut_table_number(lut_value_set_ex->lut_number);
					if (lut_number == VIOC_NO_COMPONENT) {
						(void)pr_err("[ERR][LUT] %s TCC_LUT_SET_EX invalid lut number[%d]\n", __func__, lut_value_set_ex->lut_number);
						ret = -EINVAL;
					}
				}

				if(ret == 0) {
					ret = tcc_set_lut_table(lut_number, lut_value_set_ex->Gamma);
					if (ret < 0) {
						(void)pr_err("[ERR][LUT] %s TCC_LUT_SET_EX Fail[%d]\n", __func__, ret);
						ret = -EINVAL;
					} else {
						ret = 0;
					}
				}

				if (lut_value_set_ex != NULL) {
					kfree(lut_value_set_ex);
				}
			}
			break;
		case TCC_LUT_ONOFF:
			{
				struct VIOC_LUT_ONOFF_SET lut_cmd = {0, };
				int lut_number = -1;

				if ((bool)copy_from_user((void *)&lut_cmd, (const void *)arg, sizeof(struct VIOC_LUT_ONOFF_SET))) {
					(void)pr_err("[ERR][LUT] %s TCC_LUT_ONOFF failed copy from user\n", __func__);
					ret = -ENOSYS;
				} else {
					ret = 0;
				}

				if (ret == 0) {
					lut_number = lut_get_real_lut_table_number(lut_cmd.lut_number);
					if (lut_number == VIOC_NO_COMPONENT) {
						(void)pr_err("[ERR][LUT] %s TCC_LUT_ONOFF invalid lut number[%d]\n", __func__, lut_cmd.lut_number);
						ret = -EINVAL;
					}
				}

				if (ret == 0) {
					ret = lut_drv_set_onoff(lut, lut_number, lut_cmd.lut_onoff);
					if (ret < 0) {
						(void)pr_err("[ERR][LUT] %s TCC_LUT_ONOFF Fail[%d]\n", __func__, ret);
						ret = -EINVAL;
					} else {
						ret = 0;
					}
				}
			}
			break;
		case TCC_LUT_PLUG_IN:
			{
				struct VIOC_LUT_PLUG_IN_SET lut_cmd;
				int lut_number = -1;

				if ((bool)copy_from_user((void *)&lut_cmd, (const void *)arg, sizeof(lut_cmd))) {
					(void)pr_err("[ERR][LUT] %s TCC_LUT_PLUG_IN failed copy from user\n", __func__);
					ret = -ENOSYS;
				} else {
					ret = 0;
				}

				if(ret == 0) {
					lut_number = lut_get_real_lut_table_number(lut_cmd.lut_number);
					if (lut_number == VIOC_NO_COMPONENT) {
						(void)pr_err("[ERR][LUT] %s TCC_LUT_PLUG_IN invalid lut number[%d]\n", __func__, lut_cmd.lut_number);
						ret = -EINVAL;
					}
				}

				if(ret == 0) {
					ret = lut_drv_set_plugin(lut, lut_number, lut_cmd.enable, lut_cmd.lut_plug_in_ch);
					if (ret < 0) {
						(void)pr_err("[ERR][LUT] %s TCC_LUT_PLUG_IN Fail[%d]\n", __func__, ret);
						ret = -EINVAL;
					} else {
						ret = 0;
					}
				}
			}
			break;
		case TCC_LUT_GET_DEPTH:
			{
				unsigned int lut_depth = LUT_COLOR_DEPTH;

				if ((bool)copy_to_user((void __user *)arg, &lut_depth, sizeof(lut_depth))) {
					(void)pr_err("[ERR][LUT] %s TCC_LUT_GET_DEPTH failed copy to user\n", __func__);
					ret = -ENOSYS;
				} else {
					ret = 0;
				}
			}
			break;
		default:
			(void)pr_err("[ERR][LUT]  not supported LUT IOCTL(0x%x).\n", cmd);
			break;
	}

	return ret;
}

static int lut_drv_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct lut_drv_type *lut = dev_get_drvdata(misc->parent);
	(void)inode;
	(void)filp;

	if (lut != NULL) {
		lut->dev_opened++;
	}
	return ret;
}

static int lut_drv_release(struct inode *inode, struct file *filp)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct lut_drv_type *lut = dev_get_drvdata(misc->parent);

	(void)inode;
	(void)filp;
	if ((lut != NULL) && (lut->dev_opened > 0U)) {
		lut->dev_opened--;
	}

	return 0;
}

static const struct file_operations lut_drv_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = lut_drv_ioctl,
	.open = lut_drv_open,
	.release = lut_drv_release,
};

static int lut_drv_probe(struct platform_device *pdev)
{
	struct lut_drv_type *lut;
	int ret = -EFAULT;

	lut = kzalloc(sizeof(struct lut_drv_type), GFP_KERNEL);
	if (lut == NULL) {
		ret = -ENOMEM;
	} else {
		ret = 0;
	}

	if (ret == 0) {
		lut->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
		if (lut->misc == NULL) {
			ret = -ENOMEM;
			kfree(lut);
		}
	}

	if(ret == 0) {
		lut->misc->minor = MISC_DYNAMIC_MINOR;
		lut->misc->fops = &lut_drv_fops;
		lut->misc->name = "tcc_lut";
		lut->misc->parent = &pdev->dev;
		ret = misc_register(lut->misc);
		if (ret < 0) {
			kfree(lut->misc);
			kfree(lut);
			ret = -EINVAL;
		} else {
			ret = 0;
		}
	}

	if(ret == 0) {
		lut_drv_fill_mapping_table();
		platform_set_drvdata(pdev, lut);
		(void)pr_info("[INF][LUT] %s: :%s, Driver %s Initialized\n", __func__, LUT_VERSION, pdev->name);
	}

	return ret;
}

static int lut_drv_remove(struct platform_device *pdev)
{
	struct lut_drv_type *lut = (struct lut_drv_type *)platform_get_drvdata(pdev);

	misc_deregister(lut->misc);
	kfree(lut->misc);
	kfree(lut);

	return 0;
}

static int lut_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	(void)pdev;
	(void)state;
	return 0;
}

static int lut_drv_resume(struct platform_device *pdev)
{
	(void)pdev;
	return 0;
}

static const struct of_device_id lut_of_match[] = {
	{.compatible = "telechips,tcc_lut_drv"},
	{}
};
MODULE_DEVICE_TABLE(of, lut_of_match);

static struct platform_driver lut_driver = {
	.probe = lut_drv_probe,
	.remove = lut_drv_remove,
	.suspend = lut_drv_suspend,
	.resume = lut_drv_resume,
	.driver = {
		.name = "tcc_lut_drv",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(lut_of_match),
#endif
	},
};

static int __init lut_drv_init(void)
{
	return platform_driver_register(&lut_driver);
}

static void __exit lut_drv_exit(void)
{
	platform_driver_unregister(&lut_driver);
}

module_init(lut_drv_init);
module_exit(lut_drv_exit);

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("Telechips look up table Driver");
MODULE_LICENSE("GPL");
