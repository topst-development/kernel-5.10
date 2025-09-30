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

#include "tcc-ar0820-reg.h"

#define LOG_TAG				"VSRC:AR0820"

#define loge(fmt, ...)			\
	pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(fmt, ...)			\
	pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(fmt, ...)			\
	pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(fmt, ...)			\
	pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)

#define	AR0820_DEFAULT_FRAMERATE (30)

#define AR0820_4K_WIDTH (3840U + 8U)
#define AR0820_4K_HEIGHT (2160U + 8U)

#define AR0820_DEFAULT_WIDTH (2560U + 16U)
#define AR0820_DEFAULT_HEIGHT (1440U + 16U)

#define AR0820_PAD_SRC (0U)
#define AR0820_PAD_NUM (1U)

char resolution[][5] = {"QHD", "4k"};

static const struct v4l2_mbus_framefmt ar0820_mbus_frmfmt_default[] = {
	{
		.width = AR0820_DEFAULT_WIDTH,
		.height	= AR0820_DEFAULT_HEIGHT,
		.code = MEDIA_BUS_FMT_SGRBG12_1X12,
		.field = V4L2_FIELD_NONE,
	},
	{
		.width = AR0820_4K_WIDTH,
		.height	= AR0820_4K_HEIGHT,
		.code = MEDIA_BUS_FMT_SGRBG12_1X12,
		.field = V4L2_FIELD_NONE,
	},
};

struct frame_size {
	u32 width;
	u32 height;
	const struct reg_sequence *regs;
	u32 reg_num;
};

/*
 * This object contains essential v4l2 objects
 * such as sub-device and ctrl_handler
 */
struct ar0820 {
	struct i2c_client		*clinet;
	struct v4l2_subdev		sd;
	struct v4l2_ctrl_handler	hdl;

	struct media_pad		pad;
	struct v4l2_mbus_framefmt	fmt;
	int				framerate;
	int				resolution;

	/* Regmaps */
	struct regmap			*regmap;

	struct mutex lock;
	unsigned int p_cnt;
	unsigned int s_cnt;
	unsigned int i_cnt;
};

static const struct regmap_config ar0820_regmap = {
	.reg_bits		= 16,
	.val_bits		= 16,

	.max_register		= 0xFFFF,
	.cache_type		= REGCACHE_NONE,
};

static struct frame_size ar0820_framesizes[] = {
	{	.width	= AR0820_DEFAULT_WIDTH,
		.height = AR0820_DEFAULT_HEIGHT,
		.regs	= ar0820_reg_init,
		.reg_num = ARRAY_SIZE(ar0820_reg_init),
	},
	{	.width = AR0820_4K_WIDTH,
		.height = AR0820_4K_HEIGHT,
		.regs	= ar0820_4k_reg_init,
		.reg_num = ARRAY_SIZE(ar0820_4k_reg_init),
	},
};

static u32 ar0820_framerates[] = {
	AR0820_DEFAULT_FRAMERATE,
};

/*
 * v4l2_ctrl_ops implementations
 */
static int ar0820_s_ctrl(struct v4l2_ctrl *ctrl)
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

static int ar0820_parse_device_tree(struct ar0820 *dev, struct device_node *node)
{
	int ret = 0;
	const char *buf = NULL;

	if (node == NULL) {
		loge("the device tree is empty\n");
		ret = -ENODEV;
	} else {
		ret = of_property_read_string(node, "resolution", &buf);

		if( ret < 0 ) {
			logi("resolution is default value:%s\n", resolution[dev->resolution]);
			ret = 0;
		} else {
			if( strncmp(buf, "qhd", strlen(buf)) == 0 ) {
				dev->resolution = 0;
			} else if( strncmp(buf, "4k", strlen(buf)) == 0 ) {
				dev->resolution = 1;
			} else {
				logi("Invalid property(%s), resolution set default value:%s\n",
						buf, resolution[dev->resolution]);
			}

			logi("ar0820 resolution:%s\n", resolution[dev->resolution]);
		}
	}

	return ret;
}

/*
 * Helper functions for reflection
 */
static inline struct ar0820 *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ar0820, sd);
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int ar0820_init(struct v4l2_subdev *sd, u32 enable)
{
	struct ar0820		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if ((dev->i_cnt == 0) && (enable == 1)) {
		/* enable ar0820 */
		ret = regmap_multi_reg_write(dev->regmap,
				ar0820_framesizes[dev->resolution].regs,
				ar0820_framesizes[dev->resolution].reg_num);
		if (ret) {
			/* err status */
			loge("regmap_multi_reg_write returned %d\n", ret);
		}
	} else if ((dev->i_cnt == 1) && (enable == 0)) {
		/* disable ar0820 */
	}

	if (enable)
		dev->i_cnt++;
	else
		dev->i_cnt--;

	mutex_unlock(&dev->lock);

	return ret;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int32_t ar0820_g_register(struct v4l2_subdev *sd,
				 struct v4l2_dbg_register *reg)
{
	const struct ar0820 *dev = to_dev(sd);
	uint32_t val = 0U;
	int32_t ret = 0;

	ret = regmap_read(dev->regmap, reg->reg, &val);
	if (ret < 0) {
		/* error */
		loge("ar0820_core_get_reg returned %d\n", ret);
	} else {
		/* okay */
		reg->val = val;
	}

	return ret;
}

static int32_t ar0820_s_register(struct v4l2_subdev *sd,
				 const struct v4l2_dbg_register *reg)
{
	const struct ar0820 *dev = to_dev(sd);
	int32_t ret = 0;

	if (ret >= 0) {
		ret = regmap_write(dev->regmap, reg->reg, reg->val);
		if (ret < 0) {
			/* error */
			loge("ar0820_core_set_reg returned %d\n", ret);
		}
	}

	return ret;
}
#endif

/*
 * v4l2_subdev_video_ops implementations
 */
static int ar0820_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct ar0820 *dev = to_dev(sd);
	int ret = 0;

	mutex_lock(&dev->lock);

	mutex_unlock(&dev->lock);

	return ret;
}

static int ar0820_g_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct ar0820		*dev	= NULL;

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

static int ar0820_s_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct ar0820		*dev	= NULL;

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
static int32_t ar0820_init_cfg(struct v4l2_subdev *sd,
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

static int ar0820_enum_frame_size(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_size_enum *fse)
{
	struct frame_size	*size	= NULL;
	struct ar0820		*dev	= NULL;

	dev = to_dev(sd);

	if (ARRAY_SIZE(ar0820_framesizes) <= fse->index) {
		logd("index(%u) is wrong\n", fse->index);
		return -EINVAL;
	}

	size = &ar0820_framesizes[fse->index];
	logd("size: %u * %u\n", size->width, size->height);

	fse->min_width = fse->max_width = size->width;
	fse->min_height	= fse->max_height = size->height;
	logd("max size: %u * %u\n", fse->max_width, fse->max_height);

	return 0;
}

static int ar0820_enum_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_interval_enum *fie)
{
	if (ARRAY_SIZE(ar0820_framerates) <= fie->index) {
		logd("index(%u) is wrong\n", fie->index);
		return -EINVAL;
	}

	fie->interval.numerator = 1;
	fie->interval.denominator = ar0820_framerates[fie->index];
	logd("framerate: %u / %u\n",
		fie->interval.numerator, fie->interval.denominator);

	return 0;
}

static int ar0820_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *f)
{
	struct ar0820 *dev = to_dev(sd);
	int ret = 0;

	mutex_lock(&dev->lock);

	if (f->pad >= AR0820_PAD_NUM) {
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

static int ar0820_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *f)
{
	struct ar0820 *dev = to_dev(sd);
	struct v4l2_mbus_framefmt *fmt = NULL;
	int ret = 0;

	mutex_lock(&dev->lock);

	/* check pad */
	if (f->pad >= AR0820_PAD_NUM) {
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
static int ar0820_registered(struct v4l2_subdev *sd)
{
	int ret = 0;

	logd("registered with v4l2 dev\n");

	return ret;
}

/*
 * v4l2_subdev_ops implementations
 */
static const struct v4l2_ctrl_ops ar0820_ctrl_ops = {
	.s_ctrl			= ar0820_s_ctrl,
};

static const struct v4l2_subdev_core_ops ar0820_core_ops = {
	.init			= ar0820_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register		= ar0820_g_register,
	.s_register		= ar0820_s_register,
#endif
};

static const struct v4l2_subdev_video_ops ar0820_video_ops = {
	.s_stream		= ar0820_s_stream,
	.g_frame_interval	= ar0820_g_frame_interval,
	.s_frame_interval	= ar0820_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops ar0820_pad_ops = {
	.init_cfg		= ar0820_init_cfg,
	.enum_frame_size	= ar0820_enum_frame_size,
	.enum_frame_interval	= ar0820_enum_frame_interval,
	.get_fmt		= ar0820_get_fmt,
	.set_fmt		= ar0820_set_fmt,
};

static const struct v4l2_subdev_ops ar0820_ops = {
	.core			= &ar0820_core_ops,
	.video			= &ar0820_video_ops,
	.pad			= &ar0820_pad_ops,
};

static const struct v4l2_subdev_internal_ops ar0820_internal_ops = {
	.registered = ar0820_registered,
};

struct ar0820 ar0820_data = {
};

static const struct i2c_device_id ar0820_id[] = {
	{ "ar0820", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ar0820_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id ar0820_of_match[] = {
	{
		.compatible	= "tcc-onnn,ar0820",
		.data		= &ar0820_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, ar0820_of_match);
#endif

int ar0820_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ar0820			*dev	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct ar0820), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(ar0820_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	mutex_init(&dev->lock);

	/* parse device tree */
	ret = ar0820_parse_device_tree(dev, client->dev.of_node);
	if (ret < 0) {
		loge("ar0820_parse_device_tree, ret: %d\n", ret);
		return ret;
	}

	/* regitster v4l2 control handlers */
	v4l2_ctrl_handler_init(&dev->hdl, 2);
	v4l2_ctrl_new_std(&dev->hdl, &ar0820_ctrl_ops,
		V4L2_CID_BRIGHTNESS, 0, 255, 1, 128);
	v4l2_ctrl_new_std_menu(&dev->hdl,
		&ar0820_ctrl_ops,
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
	v4l2_i2c_subdev_init(&dev->sd, client, &ar0820_ops);
	dev->sd.internal_ops = &ar0820_internal_ops;
	dev->sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	dev->sd.entity.flags = MEDIA_ENT_FL_DEFAULT;

	if (ret >= 0) {
		/* init pads */
		dev->pad.index = 0U;
		dev->pad.flags = MEDIA_PAD_FL_SOURCE;
		dev->fmt = ar0820_mbus_frmfmt_default[dev->resolution];
		media_entity_pads_init(&dev->sd.entity, AR0820_PAD_NUM,
				       &dev->pad);
	}

	/* add async subdevs and register notifier */
	ret = v4l2_async_register_subdev_sensor_common(&dev->sd);
	if (ret < 0) {
		loge("v4l2_async_register_subdev returned %d\n", ret);
	}

	/* init framerate */
	dev->framerate = AR0820_DEFAULT_FRAMERATE;

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &ar0820_regmap);
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

int ar0820_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct ar0820		*dev	= to_dev(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	v4l2_ctrl_handler_free(&dev->hdl);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver ar0820_driver = {
	.probe		= ar0820_probe,
	.remove		= ar0820_remove,
	.driver		= {
		.name		= "ar0820",
		.of_match_table	= of_match_ptr(ar0820_of_match),
	},
	.id_table	= ar0820_id,
};

module_i2c_driver(ar0820_driver);
MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips AR0820 Driver");
MODULE_LICENSE("GPL");
