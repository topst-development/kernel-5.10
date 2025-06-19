// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <soc/tcc/chipinfo.h>
#include "tcc_dptx.h"
#include "tcc_dptx_api.h"

#undef dptx_audio_dbg
#if 0
#define dptx_audio_dbg(a...) \
	(void) pr_info("[DEBUG][DPTX_AUD] " a)
#else
#define dptx_audio_dbg(a...)
#endif
#define dptx_audio_err(a...) \
	(void) pr_err("[ERROR][DPTX_AUD] " a)
#define dptx_audio_warn(a...) \
	(void) pr_err("[WARN][DPTX_AUD] " a)


static struct tcc_dptx_t *dptx_handle;


/*######################################################################*/
/*#                    DPTX Audio Driver Functions                     #*/
/*######################################################################*/
static int32_t dptx_parse_dt(
	struct platform_device *pdev,
	struct tcc_dptx_t *dptx)
{
	bool is_err = FALSE;
	int32_t ret = 0;
	uint8_t i = 0;
	struct tcc_audio_stream *astream;
	struct tcc_audio_params *aparams;

	dptx->pdev = pdev;

	dptx->link_reg = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR(dptx->link_reg)) {
		dptx->link_reg = NULL;
		dptx_audio_err("%s - link_reg is null\n", __func__);
		ret = -EINVAL;
		is_err = TRUE;
	}

	if (!is_err) {
		dptx->audio_sel_reg = of_iomap(pdev->dev.of_node, 1);
		if (IS_ERR(dptx->audio_sel_reg)) {
			dptx->audio_sel_reg = NULL;
			dptx_audio_err("%s - audio_sel_reg is null\n", __func__);
			ret = -EINVAL;
			is_err = TRUE;
		}
	}

	for (i = 0; i < dptx->max_num_stream; ++i) {
		if (is_err) {
			break;
		}

		astream = &dptx->astream[i];
		aparams = &astream->aparams;
		astream->id = i;
		dptx_audio_dbg("%s - DPTX_AUDIO_STREAM[%d]",
			__func__,
			astream->id);

		ret = of_property_read_u32_index(
			pdev->dev.of_node,
			"audio-input",
			i,
			&astream->audio_input);
		if (ret != 0) {
			dptx_audio_err("%s - audio-input parse error\n",
				__func__);
			ret = -EINVAL;
			is_err = TRUE;
		} else {
			dptx_audio_dbg("%s - 		audio-input: %d",
				__func__,
				astream->audio_input);
		}

		if (!is_err) {
			ret = of_property_read_u32_index(
				pdev->dev.of_node,
				"data-width",
				i,
				&aparams->data_width);
			if (ret != 0) {
				dptx_audio_err("%s - data-width parse error\n",
					__func__);
				ret = -EINVAL;
				is_err = TRUE;
			} else {
				dptx_audio_dbg("%s - 		data-width: %d",
					__func__,
					aparams->data_width);
			}
		}

		if (!is_err) {
			ret = of_property_read_u32_index(
				pdev->dev.of_node,
				"channels",
				i,
				&aparams->channels);
			if (ret != 0) {
				dptx_audio_err("%s - channels parse error\n",
					__func__);
				ret = -EINVAL;
				is_err = TRUE;
			} else {
				dptx_audio_dbg("%s - 		channels: %d",
					__func__,
					aparams->channels);
			}
		}

		if (!is_err) {
			ret = of_property_read_u32_index(
				pdev->dev.of_node,
				"clock-frequency",
				i,
				&aparams->sample_rate);
			if (ret != 0) {
				dptx_audio_err("%s - clock-frequency parse error\n",
					__func__);
				ret = -EINVAL;
				is_err = TRUE;
			} else {
				dptx_audio_dbg("%s - 		clock-frequency: %d",
					__func__,
					aparams->sample_rate);
			}
		}
	}

	return ret;
}

static int32_t dptx_check_aparams(struct tcc_audio_stream *astream)
{
	int32_t ret = 0;
	bool is_err = FALSE;
	struct tcc_audio_params *aparams;

	aparams = &astream->aparams;

	if ((astream->audio_input > TCC_AUDIO_INPUT_MAX)) {
		dptx_audio_err("%s - audio_input %d is invalid\n",
			__func__,
			astream->audio_input);
		ret = -EINVAL;
		is_err = TRUE;
	}

	if (!is_err && (astream->audio_input == 99u)) {
		dptx_audio_warn("%s - audio stream %d is disabled. "
			"this stream does not support audio\n",
			__func__,
			astream->id);
		ret = -EXDEV;
		is_err = TRUE;
	}

	if (!is_err && (aparams->data_width != 16u)) {
		dptx_audio_warn("%s - we cannot support data_width %d bits. "
			"(requested: %d bits, got: 16 bits)",
			__func__,
			aparams->data_width,
			aparams->data_width);
		aparams->data_width = 16u;
	}

	if (!is_err) {
		switch (aparams->channels) {
			// case 1u:
			case 2u:
			// case 8u:
				break;
			default:
				dptx_audio_err("%s - we cannot support %d channels",
					__func__,
					aparams->channels);
				ret = -EINVAL;
				is_err = TRUE;
				break;
		}
	}

	if (!is_err) {
		switch (aparams->sample_rate) {
			case 32000u:
			case 44100u:
			case 48000u:
			case 88200u:
			case 96000u:
			case 176400u:
			case 192000u:
				break;
			default:
				dptx_audio_err("%s - we cannot support sample_rate %dHz",
					__func__,
					aparams->sample_rate);
				ret = -EINVAL;
				break;
		}
	}

	return ret;
}

static void dptx_set_aparams(
	void __iomem *base_addr,
	const struct tcc_audio_stream *astream)
{
	dptx_audio_change_inf_type(base_addr, astream);
	dptx_audio_change_data_width(base_addr, astream);
	dptx_audio_change_num_ch(base_addr, astream);
	dptx_audio_change_ats_ver(base_addr, astream);
	dptx_audio_enable_channel(base_addr, astream);
}

static void dptx_enable_audio(
	void __iomem *base_addr,
	struct tcc_audio_stream *astream)
{
	dptx_audio_enable_sdp(base_addr, astream);
	dptx_audio_enable_timestamp_sdp(base_addr, astream);
	dptx_audio_send_infoframe_sdp(base_addr, astream);
	astream->status = AUDIO_STREAM_STATUS_ENABLED;
}

static void dptx_disable_audio(
	void __iomem *base_addr,
	struct tcc_audio_stream *astream)
{
	dptx_audio_disable_sdp(base_addr, astream);
	dptx_audio_disable_timestamp_sdp(base_addr, astream);
	dptx_audio_reset_infoframe_sdp(base_addr, astream);
	astream->status = AUDIO_STREAM_STATUS_DISABLED;
}

static void dptx_set_device_handle(struct tcc_dptx_t *dptx)
{
	dptx_handle = dptx;
}

static struct tcc_dptx_t *dptx_get_device_handle(void)
{
	return dptx_handle;
}


/*######################################################################*/
/*#                      API for TCC DRM Driver                        #*/
/*######################################################################*/
static void tcc_dptx_dump(void)
{
	const struct tcc_dptx_t *dptx;
	dptx = dptx_get_device_handle();
	dptx_audio_dump(dptx);
}

static int32_t tcc_dptx_set_mute(uint8_t id, bool mute)
{
	int32_t ret = 0;
	bool is_err = FALSE;
	struct tcc_dptx_t *dptx;
	struct tcc_audio_stream *astream;

	dptx = dptx_get_device_handle();
	if (id >= dptx->max_num_stream) {
		dptx_audio_err("%s - invalid stream index (id: %d)",
			__func__,
			id);
		ret = -EINVAL;
		is_err = TRUE;
	} else {
		astream = &dptx->astream[id];
	}

	if (!is_err &&
		(astream->status == AUDIO_STREAM_STATUS_DISABLED)) {
		ret = -EINVAL;
		dptx_audio_err("%s - stream %d is disabled",
			__func__,
			astream->id);
		is_err = TRUE;
	}

	if (!is_err) {
		if (mute) {
			dptx_audio_enable_mute(dptx->link_reg, astream);
			astream->status = AUDIO_STREAM_STATUS_MUTE;
		} else {
			dptx_audio_disable_mute(dptx->link_reg, astream);
			astream->status = AUDIO_STREAM_STATUS_UNMUTE;
		}
	}

	return ret;
}

static int32_t tcc_dptx_set_aparams(
	uint8_t id,
	const struct tcc_audio_params *cli_aparams)
{
	int32_t ret = 0;
	bool is_err = FALSE;
	struct tcc_dptx_t *dptx;
	struct tcc_audio_stream *astream;
	struct tcc_audio_params *aparams;

	dptx = dptx_get_device_handle();
	if (id >= dptx->max_num_stream) {
		ret = -EINVAL;
		is_err = TRUE;
		dptx_audio_err("%s - invalid stream index (id: %d)",
			__func__,
			id);
	} else {
		astream = &dptx->astream[id];
		aparams = &astream->aparams;

		aparams->data_width = cli_aparams->data_width;
		aparams->channels = cli_aparams->channels;
		aparams->sample_rate = cli_aparams->sample_rate;
	}

	if (!is_err && (astream->status != AUDIO_STREAM_STATUS_DISABLED)) {
		dptx_audio_err("%s - set_aparams is only possible "
			"when the stream %d status is disabled",
			__func__,
			astream->id);
		ret = -EINVAL;
		is_err = TRUE;
	}

	if (!is_err) {
		ret = dptx_check_aparams(astream);
		if (ret < 0) {
			dptx_audio_err("%s - invalid audio parameter setting error\n",
				__func__);
		}
	}

	if (!is_err) {
		dptx_set_aparams(dptx->link_reg, astream);
	}

	return ret;
}

static int32_t tcc_dptx_enable_audio(uint8_t id)
{
	int32_t ret = 0;
	struct tcc_dptx_t *dptx;
	struct tcc_audio_stream *astream;

	dptx = dptx_get_device_handle();
	if (id >= dptx->max_num_stream) {
		ret = -EINVAL;
		dptx_audio_err("%s - invalid stream index (id: %d)",
			__func__,
			id);
	} else {
		astream = &dptx->astream[id];

		dptx_enable_audio(dptx->link_reg, astream);
		astream->status = AUDIO_STREAM_STATUS_ENABLED;
	}

	return ret;
}

static int32_t tcc_dptx_disable_audio(uint8_t id)
{
	int32_t ret = 0;
	struct tcc_dptx_t *dptx;
	struct tcc_audio_stream *astream;

	dptx = dptx_get_device_handle();
	if (id >= dptx->max_num_stream) {
		ret = -EINVAL;
		dptx_audio_err("%s - invalid stream index (id: %d)",
			__func__,
			id);
	} else {
		astream = &dptx->astream[id];

		dptx_audio_disable_mute(dptx->link_reg, astream);
		dptx_disable_audio(dptx->link_reg, astream);
		astream->status = AUDIO_STREAM_STATUS_DISABLED;
	}

	return ret;
}

static const struct tcc_dptx_audio_ops dptx_audio_ops = {
	.set_audio_mute = tcc_dptx_set_mute,
	.set_audio_params = tcc_dptx_set_aparams,
	.enable_audio = tcc_dptx_enable_audio,
	.disable_audio = tcc_dptx_disable_audio,
	.dump = tcc_dptx_dump,
};


/*######################################################################*/
/*#               DPTX Audio Character Drivers Functions               #*/
/*######################################################################*/
static int tcc_dptx_audio_open(struct inode *inode_p, struct file *filp)
{
	(void) inode_p;
	(void) filp;
	dptx_audio_dbg("%s \n", __func__);
	return 0;
}

static long tcc_dptx_audio_ioctl(
	struct file *filp,
	unsigned int cmd,
	unsigned long arg)
{
	long ret = 0;
	struct tcc_audio_stream astream;
	const struct tcc_dptx_t *dptx;

	(void) filp;
	(void) memset(&astream, 0, sizeof(struct tcc_audio_stream));

	switch (cmd) {
		case IOCTL_CMD_DPTX_AUDIO_SET_APARAMS:
			ret = (copy_from_user(
				(void *)&astream,
				(const void __user *)arg, sizeof(astream))
				 == 0u) ? 0 : -EINVAL;
			if (ret != 0) {
				dptx_audio_err("%s - copy_from_user error\n", __func__);
			}
			(void) tcc_dptx_set_aparams(astream.id, &astream.aparams);
			break;
		case IOCTL_CMD_DPTX_AUDIO_ENABLE_AUDIO:
			ret = (copy_from_user(
				(void *)&astream,
				(const void __user *)arg, sizeof(astream))
				 == 0u) ? 0 : -EINVAL;
			if (ret != 0) {
				dptx_audio_err("%s - copy_from_user error\n", __func__);
			}
			(void) tcc_dptx_enable_audio(astream.id);
			break;
		case IOCTL_CMD_DPTX_AUDIO_DISABLE_AUDIO:
			ret = (copy_from_user(
				(void *)&astream,
				(const void __user *)arg, sizeof(astream))
				 == 0u) ? 0 : -EINVAL;
			if (ret != 0) {
				dptx_audio_err("%s - copy_from_user error\n", __func__);
			}
			(void) tcc_dptx_disable_audio(astream.id);
			break;
		case IOCTL_CMD_DPTX_AUDIO_MUTE:
			ret = (copy_from_user(
				(void *)&astream,
				(const void __user *)arg, sizeof(astream))
				 == 0u) ? 0 : -EINVAL;
			if (ret != 0) {
				dptx_audio_err("%s - copy_from_user error\n", __func__);
			}
			(void) tcc_dptx_set_mute(astream.id, TRUE);
			break;
		case IOCTL_CMD_DPTX_AUDIO_UNMUTE:
			ret = (copy_from_user(
				(void *)&astream,
				(const void __user *)arg, sizeof(astream))
				 == 0u) ? 0 : -EINVAL;
			if (ret != 0) {
				dptx_audio_err("%s - copy_from_user error\n", __func__);
			}
			(void) tcc_dptx_set_mute(astream.id, FALSE);
			break;
		case IOCTL_CMD_DPTX_AUDIO_DUMP:
			ret = (copy_from_user(
				(void *)&astream,
				(const void __user *)arg, sizeof(astream))
				 == 0u) ? 0 : -EINVAL;
			if (ret != 0) {
				dptx_audio_err("%s - copy_from_user error\n", __func__);
			}
			tcc_dptx_dump();
			break;
		case IOCTL_CMD_DPTX_AUDIO_SET_INPUT:
			ret = (copy_from_user(
				(void *)&astream,
				(const void __user *)arg, sizeof(astream))
				 == 0u) ? 0 : -EINVAL;
			if (ret != 0) {
				dptx_audio_err("%s - copy_from_user error\n", __func__);
			}
			dptx = dptx_get_device_handle();
			dptx_link_audio_input(dptx->audio_sel_reg, &astream);
			break;
		default:
			dptx_audio_err("%s - invalid cmd\n", __func__);
			ret = -EINVAL;
			break;
	}

	return ret;

}

static int tcc_dptx_audio_release(struct inode *inode_p, struct file *filp)
{
	int ret = 0;
	(void) inode_p;
	filp->private_data = NULL;
	dptx_audio_dbg("%s \n", __func__);
	return ret;
}


static const struct file_operations tcc_dptx_audio_fops = {
	.owner = THIS_MODULE,
	.open = tcc_dptx_audio_open,
	.unlocked_ioctl = tcc_dptx_audio_ioctl,
	.compat_ioctl = tcc_dptx_audio_ioctl,
	.release = tcc_dptx_audio_release,
};


/*######################################################################*/
/*#               DPTX Audio Platform Driver Functions                 #*/
/*######################################################################*/
static int tcc_dptx_audio_probe(struct platform_device *pdev)
{
	uint32_t i;
	int32_t ret = 0;
	uint32_t platform_info;
	bool is_err = FALSE;
	struct tcc_dptx_t *dptx;
	struct tcc_audio_stream *astream;

	dptx = (struct tcc_dptx_t *)devm_kzalloc(
		&pdev->dev,
		sizeof(struct tcc_dptx_t),
		GFP_KERNEL);
	if (dptx == NULL) {
		dptx_audio_dbg("%s - dptx is null\n", __func__);
		ret = -ENOMEM;
		is_err = TRUE;
	}

	if (!is_err) {
		platform_info = get_chip_name();
		switch(platform_info) {
			case 0x8050u:
			case 0x8053u:
			case 0x8070u:
				dptx->max_num_stream = 4u;
				break;
			case 0x8059u:
				dptx->max_num_stream = 3u;
				break;
			default:
				dptx->max_num_stream = 4u;
				dptx_audio_warn("%s - max num stream is set as %d \n",
				 __func__,
				 dptx->max_num_stream);
				is_err = TRUE;
				break;
		}
	}

	if (!is_err) {
		ret = dptx_parse_dt(pdev, dptx);
		if (ret < 0) {
			ret = -EINVAL;
			is_err = TRUE;
			dptx_audio_err("%s - parse error\n", __func__);
		}
	}

	for (i = 0u; !is_err && (i < dptx->max_num_stream); ++i) {
		astream = &dptx->astream[i];
		dptx_link_audio_input(dptx->audio_sel_reg, astream);
		// dptx_disable_audio(dptx->link_reg, astream);
	}

	if (!is_err) {
		ret = platform_device_add_data(pdev,
			&dptx_audio_ops,
			sizeof(dptx_audio_ops));
		if (ret != 0) {
			dptx_audio_err("%s - failed to add platform data error\n",
				__func__);
			is_err = TRUE;
		} else {
			dptx_set_device_handle(dptx);
			platform_set_drvdata(pdev, dptx);
		}
	}

	if (!is_err) {
		ret = alloc_chrdev_region(&dptx->dev_num, MINOR_BASE, 1, DEVICE_NAME);
		if (ret < 0) {
			dptx_audio_err("%s - failed to allocate char device region\n",
				__func__);
			is_err = TRUE;
		}
	}

	if (!is_err) {
		cdev_init(&dptx->char_dev, &tcc_dptx_audio_fops);
		ret = cdev_add(&dptx->char_dev, dptx->dev_num, 1);
		if (ret < 0) {
			dptx_audio_err("%s - failed to add char device\n", __func__);
			is_err = TRUE;
			unregister_chrdev_region(dptx->dev_num, 1);
			devm_kfree(&pdev->dev, dptx);
		}
	}

	if (!is_err) {
		dptx->char_class = class_create(THIS_MODULE, "tcc_dptx_audio");
		if (dptx->char_class == NULL) {
			ret = -EINVAL;
			dptx_audio_err("%s - failed to create class\n", __func__);
			is_err = TRUE;
			cdev_del(&dptx->char_dev);
			unregister_chrdev_region(dptx->dev_num, 1);
			devm_kfree(&pdev->dev, dptx);
		}
	}

	if (!is_err) {
		dptx->char_device = device_create(
			dptx->char_class,
			&pdev->dev,
			dptx->dev_num,
			NULL,
			"tcc_dptx_audio");
		if (dptx->char_device == NULL) {
			ret = -EINVAL;
			dptx_audio_err("%s - failed to create device\n", __func__);
			class_destroy(dptx->char_class);
			cdev_del(&dptx->char_dev);
			unregister_chrdev_region(dptx->dev_num, 1);
			devm_kfree(&pdev->dev, dptx);
		}
	}

	dptx_audio_dbg("%s - ret %d \n", __func__, ret);
	return ret;
}

static int tcc_dptx_audio_remove(struct platform_device *pdev)
{
	// uint8_t i;
	struct tcc_dptx_t *dptx;
	// struct tcc_audio_stream *astream;
	dptx_audio_dbg("%s \n", __func__);

 	dptx = (struct tcc_dptx_t *)platform_get_drvdata(pdev);
	// for (i = 0; i < dptx->max_num_stream; ++i) {
	//  	astream = &dptx->astream[i];
	// 	dptx_audio_disable_mute(dptx->link_reg, astream);
	// 	dptx_disable_audio(dptx->link_reg, astream);
	// 	astream->status = AUDIO_STREAM_STATUS_DISABLED;
	// 	dptx_audio_dbg("%s - stream %d is disabled", __func__, astream->id);
	// }

	device_destroy(dptx->char_class, dptx->dev_num);
	class_destroy(dptx->char_class);
	cdev_del(&dptx->char_dev);
	unregister_chrdev_region(dptx->dev_num, 1);
	devm_kfree(&pdev->dev, dptx);

	if (dptx_handle != NULL) {
		dptx_handle = NULL;
	}

	return 0;
}

// Todo. STR implementation
// #if defined(CONFIG_PM)
// static int tcc_dptx_audio_suspend(struct platform_device *pdev,
// 	pm_message_t state)
// {
// 	return 0;
// }

// static int tcc_dptx_audio_resume(struct platform_device *pdev)
// {
// 	return 0;
// }
// #endif


#ifdef CONFIG_OF
static const struct of_device_id tcc_dptx_audio_of_match[] = {
	{ .compatible = "telechips,dptx_audio", },
	{ },
};
MODULE_DEVICE_TABLE(of, tcc_dptx_audio_of_match);
#endif

static struct platform_driver tcc_dptx_audio_driver = {
	.probe= tcc_dptx_audio_probe,
	.remove= tcc_dptx_audio_remove,
// #if defined(CONFIG_PM)
// 	.suspend = tcc_dptx_audio_suspend,
// 	.resume = tcc_dptx_audio_resume,
// #endif
	.driver= {
		.name= "tcc_dptx_audio",
		.owner= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = tcc_dptx_audio_of_match,
#endif
	},
};

static int __init tcc_dptx_audio_init(void)
{
	dptx_audio_dbg("%s \n", __func__);
	return platform_driver_register(&tcc_dptx_audio_driver);
}

static void __exit tcc_dptx_audio_exit(void)
{
	dptx_audio_dbg("%s \n", __func__);
	platform_driver_unregister(&tcc_dptx_audio_driver);
}

subsys_initcall(tcc_dptx_audio_init);
module_exit(tcc_dptx_audio_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("<mk.hwang@telechips.com>");
MODULE_DESCRIPTION("DisplayPort TX Audio Link Driver");

