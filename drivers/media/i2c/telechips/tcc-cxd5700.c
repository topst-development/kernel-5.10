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

#define LOG_TAG				"VSRC:CXD5700"

#define loge(fmt, ...)			\
	pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(fmt, ...)			\
	pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(fmt, ...)			\
	pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(fmt, ...)			\
	pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)

#define	DEFAULT_FRAMERATE		(30)

#define CXD5700_PAD_SRC (0U)
#define CXD5700_PAD_NUM (1U)

static const struct v4l2_mbus_framefmt cxd5700_mbus_frmfmt_default = {
	.width = 1920U,
	.height	= 1080U,
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
struct cxd5700 {
	struct i2c_client		*clinet;
	struct v4l2_subdev		sd;

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

const char cxd5700_reg_defaults[] = {
	/* 0x0A, */0x01, 0x07, 0x02, 0x01, 0x00, 0x00, 0x30, 0x80, 0xC5,
};

static const struct regmap_config cxd5700_regmap = {
	.reg_bits		= 8,
	.val_bits		= 8,

	.max_register		= 0xFF,
	.cache_type		= REGCACHE_NONE,
};

static struct frame_size cxd5700_framesizes[] = {
	{	1920,	1080	},
};

static u32 cxd5700_framerates[] = {
	30,
};

static int cxd5700_parse_device_tree(struct cxd5700 *dev,
				     struct device_node *node)
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
static inline struct cxd5700 *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct cxd5700, sd);
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int cxd5700_init(struct v4l2_subdev *sd, u32 enable)
{
	struct cxd5700 *dev = to_dev(sd);
	int ret = 0;

	mutex_lock(&dev->lock);

	if (enable == 1) {
		if (dev->i_cnt == 0) {
			ret = regmap_bulk_write(dev->regmap,
					0x0a,
					cxd5700_reg_defaults,
					ARRAY_SIZE(cxd5700_reg_defaults));
			if (ret < 0) {
				/* err status */
				loge("regmap_multi_reg_write returned %d\n",
				     ret);
			}
			msleep(100);	/* stabilization time */
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
static int cxd5700_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct cxd5700 *dev = to_dev(sd);
	int ret = 0;

	mutex_lock(&dev->lock);

	mutex_unlock(&dev->lock);

	return ret;
}

static int cxd5700_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *interval)
{
	struct cxd5700 *dev = NULL;

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

static int cxd5700_s_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *interval)
{
	struct cxd5700 *dev = NULL;

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
static int32_t cxd5700_init_cfg(struct v4l2_subdev *sd,
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

static int cxd5700_enum_frame_size(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct frame_size	*size		= NULL;

	if (ARRAY_SIZE(cxd5700_framesizes) <= fse->index) {
		logd("index(%u) is wrong\n", fse->index);
		return -EINVAL;
	}

	size = &cxd5700_framesizes[fse->index];
	logd("size: %u * %u\n", size->width, size->height);

	fse->min_width = fse->max_width = size->width;
	fse->min_height	= fse->max_height = size->height;
	logd("max size: %u * %u\n", fse->max_width, fse->max_height);

	return 0;
}

static int
cxd5700_enum_frame_interval(struct v4l2_subdev *sd,
			    struct v4l2_subdev_pad_config *cfg,
			    struct v4l2_subdev_frame_interval_enum *fie)
{
	if (ARRAY_SIZE(cxd5700_framerates) <= fie->index) {
		logd("index(%u) is wrong\n", fie->index);
		return -EINVAL;
	}

	fie->interval.numerator = 1;
	fie->interval.denominator = cxd5700_framerates[fie->index];
	logd("framerate: %u / %u\n",
		fie->interval.numerator, fie->interval.denominator);

	return 0;
}

static int cxd5700_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *f)
{
	struct cxd5700		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if (f->pad >= CXD5700_PAD_NUM) {
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

static int cxd5700_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *f)
{
	struct cxd5700 *dev = to_dev(sd);
	struct v4l2_mbus_framefmt *fmt = NULL;
	int ret = 0;

	mutex_lock(&dev->lock);

	/* check pad */
	if (f->pad >= CXD5700_PAD_NUM) {
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
static int cxd5700_registered(struct v4l2_subdev *sd)
{
	int ret = 0;

	logd("registered with v4l2 dev\n");

	return ret;
}

/*
 * v4l2_subdev_ops implementations
 */
static const struct v4l2_subdev_core_ops cxd5700_core_ops = {
	.init			= cxd5700_init,
};

static const struct v4l2_subdev_video_ops cxd5700_video_ops = {
	.s_stream		= cxd5700_s_stream,
	.g_frame_interval	= cxd5700_g_frame_interval,
	.s_frame_interval	= cxd5700_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops cxd5700_pad_ops = {
	.init_cfg		= cxd5700_init_cfg,
	.enum_frame_size	= cxd5700_enum_frame_size,
	.enum_frame_interval	= cxd5700_enum_frame_interval,
	.get_fmt		= cxd5700_get_fmt,
	.set_fmt		= cxd5700_set_fmt,
};

static const struct v4l2_subdev_ops cxd5700_ops = {
	.core			= &cxd5700_core_ops,
	.video			= &cxd5700_video_ops,
	.pad			= &cxd5700_pad_ops,
};

static const struct v4l2_subdev_internal_ops cxd5700_internal_ops = {
	.registered = cxd5700_registered,
};

struct cxd5700 cxd5700_data = {
};

static const struct i2c_device_id cxd5700_id[] = {
	{ "cxd5700", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cxd5700_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id cxd5700_of_match[] = {
	{
		.compatible	= "tcc-sony,cxd5700",
		.data		= &cxd5700_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, cxd5700_of_match);
#endif

int cxd5700_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct cxd5700			*dev	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct cxd5700), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(cxd5700_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	mutex_init(&dev->lock);

	/* parse device tree */
	ret = cxd5700_parse_device_tree(dev, client->dev.of_node);
	if (ret < 0) {
		loge("cxd5700_parse_device_tree, ret: %d\n", ret);
		return ret;
	}


	/* Register with V4L2 layer as a slave device */
	v4l2_i2c_subdev_init(&dev->sd, client, &cxd5700_ops);
	dev->sd.internal_ops = &cxd5700_internal_ops;
	dev->sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	dev->sd.entity.flags = MEDIA_ENT_FL_DEFAULT;
	v4l2_set_subdevdata(&dev->sd, dev);

	if (ret >= 0) {
		/* init pads */
		dev->pad.index = 0U;
		dev->pad.flags = MEDIA_PAD_FL_SOURCE;
		dev->fmt = cxd5700_mbus_frmfmt_default;
		media_entity_pads_init(&dev->sd.entity, CXD5700_PAD_NUM,
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
	dev->regmap = devm_regmap_init_i2c(client, &cxd5700_regmap);
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

int cxd5700_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct cxd5700		*dev	= to_dev(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver cxd5700_driver = {
	.probe		= cxd5700_probe,
	.remove		= cxd5700_remove,
	.driver		= {
		.name		= "cxd5700",
		.of_match_table	= of_match_ptr(cxd5700_of_match),
	},
	.id_table	= cxd5700_id,
};

module_i2c_driver(cxd5700_driver);
MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips MAX96705 Driver");
MODULE_LICENSE("GPL");
