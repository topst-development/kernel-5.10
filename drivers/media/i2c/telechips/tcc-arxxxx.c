// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */  

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of_graph.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <media/v4l2-async.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <linux/kdev_t.h>
#include <linux/of_gpio.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>

#define LOG_TAG				"VSRC:ARXXXX"

#define loge(fmt, ...)			\
	pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(fmt, ...)			\
	pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(fmt, ...)			\
	pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(fmt, ...)			\
	pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)

#define	DEFAULT_FRAMERATE		(30)

#define ARXXXX_PAD_SRC (0U)
#define ARXXXX_PAD_NUM (1U)

static const struct v4l2_mbus_framefmt arxxxx_mbus_frmfmt_default = {
	.width = 1280U,
	.height	= 720U,
	.code = MEDIA_BUS_FMT_UYVY8_1X16,
	.field = V4L2_FIELD_NONE,
};

struct frame_size {
	u32 width;
	u32 height;
};

/*
 * This object contains essential v4l2 objects
 * such as sub-device and ctrl_handler
 */
struct arxxxx {
	struct i2c_client		*clinet;
	struct v4l2_subdev		sd;
	struct v4l2_ctrl_handler	hdl;

	struct media_pad		pad;
	struct v4l2_mbus_framefmt	fmt;
	int				framerate;

	/* Regmaps */
	struct regmap			*regmap;

	struct mutex lock;
	unsigned int p_cnt;
	unsigned int s_cnt;
	unsigned int i_cnt;
};

const struct reg_sequence arxxxx_reg_init[] = {
};

static const struct regmap_config arxxxx_regmap = {
	.reg_bits		= 16,
	.val_bits		= 16,

	.max_register		= 0xFFFF,
	.cache_type		= REGCACHE_NONE,
};

static struct frame_size arxxxx_framesizes[] = {
	{	1280,	720	},
};

static u32 arxxxx_framerates[] = {
	30,
};

/*
 * v4l2_ctrl_ops implementations
 */
static int arxxxx_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int			ret	= 0;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
	case V4L2_CID_CONTRAST:
	case V4L2_CID_SATURATION:
	case V4L2_CID_HUE:
	case V4L2_CID_DO_WHITE_BALANCE:
	default:
		loge("V4L2_CID_BRIGHTNESS is not implemented yet.\n");
		ret = -EINVAL;
	}

	return ret;
}

static int arxxxx_parse_device_tree(struct arxxxx *dev, struct device_node *node)
{
	int ret = 0;

	if (node == NULL) {
		loge("the device tree is empty\n");
		ret = -ENODEV;
	}


	return ret;
}

/*
 * Helper functions for reflection
 */
static inline struct arxxxx *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct arxxxx, sd);
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int arxxxx_init(struct v4l2_subdev *sd, u32 enable)
{
	struct arxxxx		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if (enable == 1) {
		if (dev->i_cnt == 0) {
			ret = regmap_multi_reg_write(dev->regmap,
					arxxxx_reg_init,
					ARRAY_SIZE(arxxxx_reg_init));
			if (ret < 0) {
				/* err status */
				loge("regmap_multi_reg_write returned %d\n", ret);
			}
		}
		dev->i_cnt++;
	} else {
		dev->i_cnt--;
		if (dev->i_cnt == 0) {
			/* de-init */
			;
		}
	}

	mutex_unlock(&dev->lock);

	return ret;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int arxxxx_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct arxxxx *dev = to_dev(sd);
	int ret = 0;

	mutex_lock(&dev->lock);

	mutex_unlock(&dev->lock);

	return ret;
}

static int arxxxx_g_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct arxxxx		*dev	= NULL;

	dev = to_dev(sd);
	if (!dev) {
		loge("Failed to get video source object by subdev\n");
		return -EINVAL;
	}

	interval->pad = 0;
	interval->interval.numerator = 1;
	interval->interval.denominator = dev->framerate;

	return 0;
}

static int arxxxx_s_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct arxxxx		*dev	= NULL;

	dev = to_dev(sd);
	if (!dev) {
		loge("Failed to get video source object by subdev\n");
		return -EINVAL;
	}

	/* set framerate with i2c setting if supported */

	dev->framerate = interval->interval.denominator;

	return 0;
}

/*
 * v4l2_subdev_pad_ops implementations
 */
static int32_t arxxxx_init_cfg(struct v4l2_subdev *sd,
			       struct v4l2_subdev_pad_config *cfg)
{
	struct v4l2_mbus_framefmt *try;
	struct v4l2_subdev_format fmt;
	unsigned int pad;
	int32_t ret = 0;

	for (pad = 0; pad < sd->entity.num_pads; pad++) {
		memset(&fmt, 0, sizeof(fmt));

		fmt.pad = pad;
		fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		ret = v4l2_subdev_call(sd, pad, get_fmt, NULL, &fmt);
		if (ret < 0) {
			loge("get_fmt returned %d\n", ret);
			break;
		}

		try = v4l2_subdev_get_try_format(sd, cfg, pad);
		*try = fmt.format;
	}

	return ret;
}

static int arxxxx_enum_frame_size(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_size_enum *fse)
{
	struct frame_size	*size		= NULL;

	if (ARRAY_SIZE(arxxxx_framesizes) <= fse->index) {
		logd("index(%u) is wrong\n", fse->index);
		return -EINVAL;
	}

	size = &arxxxx_framesizes[fse->index];
	logd("size: %u * %u\n", size->width, size->height);

	fse->min_width = fse->max_width = size->width;
	fse->min_height	= fse->max_height = size->height;
	logd("max size: %u * %u\n", fse->max_width, fse->max_height);

	return 0;
}

static int arxxxx_enum_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_interval_enum *fie)
{
	if (ARRAY_SIZE(arxxxx_framerates) <= fie->index) {
		logd("index(%u) is wrong\n", fie->index);
		return -EINVAL;
	}

	fie->interval.numerator = 1;
	fie->interval.denominator = arxxxx_framerates[fie->index];
	logd("framerate: %u / %u\n",
		fie->interval.numerator, fie->interval.denominator);

	return 0;
}

static int arxxxx_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *f)
{
	struct arxxxx		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if (f->pad >= ARXXXX_PAD_NUM) {
		/* error */
		loge("invalid pad num(%d)\n", f->pad);
		ret = -EINVAL;
	} else {
		if (f->which == V4L2_SUBDEV_FORMAT_TRY) {
			/* get try format */
			f->format =
				*v4l2_subdev_get_try_format(sd, cfg, f->pad);
		} else {
			/* get active format */
			f->format = dev->fmt;
		}
	}

	mutex_unlock(&dev->lock);

	return ret;
}

static int arxxxx_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *f)
{
	struct arxxxx *dev = to_dev(sd);
	struct v4l2_mbus_framefmt *fmt = NULL;
	int ret = 0;

	mutex_lock(&dev->lock);

	/* check pad */
	if (f->pad >= ARXXXX_PAD_NUM) {
		/* error */
		loge("invalid pad num(%d)\n", f->pad);
		ret = -EINVAL;
	}

	/* get try or active mbus framefmt pointer */
	if (ret >= 0) {
		if (f->which == V4L2_SUBDEV_FORMAT_TRY) {
			/* get try format */
			fmt = v4l2_subdev_get_try_format(sd, cfg, f->pad);
		} else {
			/* get active format */
			fmt = &dev->fmt;
		}
	}

	*fmt = f->format;

	mutex_unlock(&dev->lock);

	return ret;
}

/*
 * v4l2_subdev_internal_ops implementations
 */
static int arxxxx_registered(struct v4l2_subdev *sd)
{
	int ret = 0;

	logd("registered with v4l2 dev\n");

	return ret;
}

/*
 * v4l2_subdev_ops implementations
 */
static const struct v4l2_ctrl_ops arxxxx_ctrl_ops = {
	.s_ctrl			= arxxxx_s_ctrl,
};

static const struct v4l2_subdev_core_ops arxxxx_core_ops = {
	.init			= arxxxx_init,
};

static const struct v4l2_subdev_video_ops arxxxx_video_ops = {
	.s_stream		= arxxxx_s_stream,
	.g_frame_interval	= arxxxx_g_frame_interval,
	.s_frame_interval	= arxxxx_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops arxxxx_pad_ops = {
	.init_cfg		= arxxxx_init_cfg,
	.enum_frame_size	= arxxxx_enum_frame_size,
	.enum_frame_interval	= arxxxx_enum_frame_interval,
	.get_fmt		= arxxxx_get_fmt,
	.set_fmt		= arxxxx_set_fmt,
};

static const struct v4l2_subdev_ops arxxxx_ops = {
	.core			= &arxxxx_core_ops,
	.video			= &arxxxx_video_ops,
	.pad			= &arxxxx_pad_ops,
};

static const struct v4l2_subdev_internal_ops arxxxx_internal_ops = {
	.registered = arxxxx_registered,
};

struct arxxxx arxxxx_data = {
};

static const struct i2c_device_id arxxxx_id[] = {
	{ "arxxxx", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, arxxxx_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id arxxxx_of_match[] = {
	{
		.compatible	= "tcc-onnn,arxxxx",
		.data		= &arxxxx_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, arxxxx_of_match);
#endif

int arxxxx_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct arxxxx			*dev	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct arxxxx), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(arxxxx_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	mutex_init(&dev->lock);

	/* parse device tree */
	ret = arxxxx_parse_device_tree(dev, client->dev.of_node);
	if (ret < 0) {
		loge("arxxxx_parse_device_tree, ret: %d\n", ret);
		return ret;
	}

	/* regitster v4l2 control handlers */
	v4l2_ctrl_handler_init(&dev->hdl, 2);
	v4l2_ctrl_new_std(&dev->hdl, &arxxxx_ctrl_ops,
		V4L2_CID_BRIGHTNESS, 0, 255, 1, 128);
	v4l2_ctrl_new_std_menu(&dev->hdl,
		&arxxxx_ctrl_ops,
		V4L2_CID_DV_RX_IT_CONTENT_TYPE,
		V4L2_DV_IT_CONTENT_TYPE_NO_ITC,
		0,
		V4L2_DV_IT_CONTENT_TYPE_NO_ITC);
	dev->sd.ctrl_handler = &dev->hdl;
	if (dev->hdl.error) {
		loge("v4l2_ctrl_handler_init is wrong\n");
		ret = dev->hdl.error;
		goto goto_free_device_data;
	}

	/* Register with V4L2 layer as a slave device */
	v4l2_i2c_subdev_init(&dev->sd, client, &arxxxx_ops);
	dev->sd.internal_ops = &arxxxx_internal_ops;
	dev->sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	dev->sd.entity.flags = MEDIA_ENT_FL_DEFAULT;
	v4l2_set_subdevdata(&dev->sd, dev);

	if (ret >= 0) {
		/* init pads */
		dev->pad.index = 0U;
		dev->pad.flags = MEDIA_PAD_FL_SOURCE;
		dev->fmt = arxxxx_mbus_frmfmt_default;
		media_entity_pads_init(&dev->sd.entity, ARXXXX_PAD_NUM,
				       &dev->pad);
	}

	/* add async subdevs and register notifier */
	ret = v4l2_async_register_subdev_sensor_common(&dev->sd);
	if (ret < 0) {
		loge("v4l2_async_register_subdev returned %d\n", ret);
	}

	/* init framerate */
	dev->framerate = DEFAULT_FRAMERATE;

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &arxxxx_regmap);
	if (IS_ERR(dev->regmap)) {
		loge("devm_regmap_init_i2c is wrong\n");
		ret = -1;
		goto goto_free_device_data;
	}

	goto goto_end;

goto_free_device_data:
	/* free the videosource data */
	kfree(dev);

goto_end:
	return ret;
}

int arxxxx_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct arxxxx		*dev	= to_dev(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	v4l2_ctrl_handler_free(&dev->hdl);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver arxxxx_driver = {
	.probe		= arxxxx_probe,
	.remove		= arxxxx_remove,
	.driver		= {
		.name		= "arxxxx",
		.of_match_table	= of_match_ptr(arxxxx_of_match),
	},
	.id_table	= arxxxx_id,
};

module_i2c_driver(arxxxx_driver);
MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips MAX96705 Driver");
MODULE_LICENSE("GPL");
