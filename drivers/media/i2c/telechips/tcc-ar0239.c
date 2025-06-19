// SPDX-License-Identifier: GPL-2.0
/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 ****************************************************************************/

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

#define LOG_TAG				"VSRC:AR0239"

#define loge(fmt, ...)			\
	pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(fmt, ...)			\
	pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(fmt, ...)			\
	pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(fmt, ...)			\
	pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)

#define	AR0239_DEFAULT_FRAMERATE (30)

#define AR0239_DEFAULT_WIDTH (1920U + 12U)
#define AR0239_DEFAULT_HEIGHT (1080U + 16U)

#define AR0239_PAD_SRC (0U)
#define AR0239_PAD_NUM (1U)

static const struct v4l2_mbus_framefmt ar0239_mbus_frmfmt_default = {
	.width = AR0239_DEFAULT_WIDTH,
	.height	= AR0239_DEFAULT_HEIGHT,
	.code = MEDIA_BUS_FMT_SGRBG12_1X12, /* it doesn't matter if RGB-IR */
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
struct ar0239 {
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

const struct reg_sequence ar0239_reg_init[] = {
	// Register Log created on Friday, October 14, 2022 : 13:59:07
	// [Register Log 10/14/22 13:59:07]
	{0x301A, 0x0001, 400 * 1000}, // RESET_REGISTER
	{0x3324, 0x001C, 0}, // ATTENUATION_CTRL
	{0x30B4, 0x01C1, 0}, // TEMPSENS_CTRL_REG
	{0x3092, 0x006F, 0}, // ROW_NOISE_CONTROL
	{0x31AC, 0x0C0C, 0}, // DATA_FORMAT_BITS
	{0x3030, 0x0080, 0}, // PLL_MULTIPLIER
	{0x302E, 0x0009, 0}, // PRE_PLL_CLK_DIV
	{0x302C, 0x0001, 0}, // VT_SYS_CLK_DIV
	{0x302A, 0x0006, 0}, // VT_PIX_CLK_DIV
	{0x3038, 0x0001, 0}, // OP_SYS_CLK_DIV
	{0x3036, 0x000C, 0}, // OP_PIX_CLK_DIV
	{0x3082, 0x0001, 0}, // OPERATION_MODE_CTRL
	{0x318E, 0x0000, 0}, // HDR_MC_CTRL3
	{0x31D0, 0x0000, 0}, // COMPANDING
	{0x3004, 0x0002, 0}, //X_ADDR_START = 2
	{0x3008, 0x078D, 0}, //X_ADDR_END = 1933
	{0x3002, 0x002E, 0}, //Y_ADDR_START = 46
	{0x3006, 0x0475, 0}, //Y_ADDR_END = 1141
	{0x300C, 0x0776, 0}, //LINE_LENGTH_PCK = 1910
	{0x300A, 0x045C, 0}, //FRAME_LENGTH_LINES = 1116
	{0x3012, 0x044B, 0}, // COARSE_INTEGRATION_TIME
	{0x301A, 0x2058, 0}, // RESET_REGISTER
	{0x31AE, 0x0204, 0}, // SERIAL_FORMAT
	{0x3354, 0x002C, 0}, // MIPI_CNTRL
	{0x30B0, 0x022A, 0}, // DIGITAL_TEST
	{0x31B0, 0x0090, 0}, // FRAME_PREAMBLE
	{0x31B2, 0x0065, 0}, // LINE_PREAMBLE
	{0x31B4, 0x2A86, 0}, // MIPI_TIMING_0
	{0x31B6, 0x21D6, 0}, // MIPI_TIMING_1
	{0x31B8, 0x6049, 0}, // MIPI_TIMING_2
	{0x31BA, 0x0208, 0}, // MIPI_TIMING_3
	{0x31BC, 0x8007, 0}, // MIPI_TIMING_4
	{0x3064, 0x1802, 0}, // SMIA_TEST
	{0x301A, 0x205C, 0}, // RESET_REGISTER
	{0x3F4E, 0x3E3C, 0}, // PIX_DEF_1D_DDC_HI_DEF
	{0x3F50, 0x080B, 0}, // PIX_DEF_1D_DDC_EDGE
};

static const struct regmap_config ar0239_regmap = {
	.reg_bits		= 16,
	.val_bits		= 16,

	.max_register		= 0xFFFF,
	.cache_type		= REGCACHE_NONE,
};

static struct frame_size ar0239_framesizes[] = {
	{	AR0239_DEFAULT_WIDTH,	AR0239_DEFAULT_HEIGHT	},
};

static u32 ar0239_framerates[] = {
	AR0239_DEFAULT_FRAMERATE,
};

/*
 * v4l2_ctrl_ops implementations
 */
static int ar0239_s_ctrl(struct v4l2_ctrl *ctrl)
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

static int ar0239_parse_device_tree(struct ar0239 *dev, struct device_node *node)
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
static inline struct ar0239 *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ar0239, sd);
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int ar0239_init(struct v4l2_subdev *sd, u32 enable)
{
	struct ar0239		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if (enable == 1) {
		if (dev->i_cnt == 0) {
			ret = regmap_multi_reg_write(dev->regmap,
					ar0239_reg_init,
					ARRAY_SIZE(ar0239_reg_init));
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

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int32_t ar0239_g_register(struct v4l2_subdev *sd,
				 struct v4l2_dbg_register *reg)
{
	const struct ar0239 *dev = to_dev(sd);
	uint32_t val = 0U;
	int32_t ret = 0;

	ret = regmap_read(dev->regmap, reg->reg, &val);
	if (ret < 0) {
		/* error */
		loge("ar0239_core_get_reg returned %d\n", ret);
	} else {
		/* okay */
		reg->val = val;
	}

	return ret;
}

static int32_t ar0239_s_register(struct v4l2_subdev *sd,
				 const struct v4l2_dbg_register *reg)
{
	const struct ar0239 *dev = to_dev(sd);
	int32_t ret = 0;

	if (ret >= 0) {
		ret = regmap_write(dev->regmap, reg->reg, reg->val);
		if (ret < 0) {
			/* error */
			loge("ar0239_core_set_reg returned %d\n", ret);
		}
	}

	return ret;
}
#endif

/*
 * v4l2_subdev_video_ops implementations
 */
static int ar0239_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct ar0239 *dev = to_dev(sd);
	int ret = 0;

	mutex_lock(&dev->lock);

	mutex_unlock(&dev->lock);

	return ret;
}

static int ar0239_g_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct ar0239		*dev	= NULL;

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

static int ar0239_s_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct ar0239		*dev	= NULL;

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
static int32_t ar0239_init_cfg(struct v4l2_subdev *sd,
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

static int ar0239_enum_frame_size(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_size_enum *fse)
{
	struct frame_size	*size		= NULL;

	if (ARRAY_SIZE(ar0239_framesizes) <= fse->index) {
		logd("index(%u) is wrong\n", fse->index);
		return -EINVAL;
	}

	size = &ar0239_framesizes[fse->index];
	logd("size: %u * %u\n", size->width, size->height);

	fse->min_width = fse->max_width = size->width;
	fse->min_height	= fse->max_height = size->height;
	logd("max size: %u * %u\n", fse->max_width, fse->max_height);

	return 0;
}

static int ar0239_enum_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_interval_enum *fie)
{
	if (ARRAY_SIZE(ar0239_framerates) <= fie->index) {
		logd("index(%u) is wrong\n", fie->index);
		return -EINVAL;
	}

	fie->interval.numerator = 1;
	fie->interval.denominator = ar0239_framerates[fie->index];
	logd("framerate: %u / %u\n",
		fie->interval.numerator, fie->interval.denominator);

	return 0;
}

static int ar0239_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *f)
{
	struct ar0239 *dev = to_dev(sd);
	int ret = 0;

	mutex_lock(&dev->lock);

	if (f->pad >= AR0239_PAD_NUM) {
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

static int ar0239_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *f)
{
	struct ar0239 *dev = to_dev(sd);
	struct v4l2_mbus_framefmt *fmt = NULL;
	int ret = 0;

	mutex_lock(&dev->lock);

	/* check pad */
	if (f->pad >= AR0239_PAD_NUM) {
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
static int ar0239_registered(struct v4l2_subdev *sd)
{
	int ret = 0;

	logd("registered with v4l2 dev\n");

	return ret;
}

/*
 * v4l2_subdev_ops implementations
 */
static const struct v4l2_ctrl_ops ar0239_ctrl_ops = {
	.s_ctrl			= ar0239_s_ctrl,
};

static const struct v4l2_subdev_core_ops ar0239_core_ops = {
	.init			= ar0239_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register		= ar0239_g_register,
	.s_register		= ar0239_s_register,
#endif
};

static const struct v4l2_subdev_video_ops ar0239_video_ops = {
	.s_stream		= ar0239_s_stream,
	.g_frame_interval	= ar0239_g_frame_interval,
	.s_frame_interval	= ar0239_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops ar0239_pad_ops = {
	.init_cfg		= ar0239_init_cfg,
	.enum_frame_size	= ar0239_enum_frame_size,
	.enum_frame_interval	= ar0239_enum_frame_interval,
	.get_fmt		= ar0239_get_fmt,
	.set_fmt		= ar0239_set_fmt,
};

static const struct v4l2_subdev_ops ar0239_ops = {
	.core			= &ar0239_core_ops,
	.video			= &ar0239_video_ops,
	.pad			= &ar0239_pad_ops,
};

static const struct v4l2_subdev_internal_ops ar0239_internal_ops = {
	.registered = ar0239_registered,
};

struct ar0239 ar0239_data = {
};

static const struct i2c_device_id ar0239_id[] = {
	{ "ar0239", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ar0239_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id ar0239_of_match[] = {
	{
		.compatible	= "tcc-onnn,ar0239",
		.data		= &ar0239_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, ar0239_of_match);
#endif

int ar0239_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ar0239			*dev	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct ar0239), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(ar0239_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	mutex_init(&dev->lock);

	/* parse device tree */
	ret = ar0239_parse_device_tree(dev, client->dev.of_node);
	if (ret < 0) {
		loge("cxd5700_parse_device_tree, ret: %d\n", ret);
		return ret;
	}

	/* regitster v4l2 control handlers */
	v4l2_ctrl_handler_init(&dev->hdl, 2);
	v4l2_ctrl_new_std(&dev->hdl, &ar0239_ctrl_ops,
		V4L2_CID_BRIGHTNESS, 0, 255, 1, 128);
	v4l2_ctrl_new_std_menu(&dev->hdl,
		&ar0239_ctrl_ops,
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
	v4l2_i2c_subdev_init(&dev->sd, client, &ar0239_ops);
	dev->sd.internal_ops = &ar0239_internal_ops;
	dev->sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	dev->sd.entity.flags = MEDIA_ENT_FL_DEFAULT;

	if (ret >= 0) {
		/* init pads */
		dev->pad.index = 0U;
		dev->pad.flags = MEDIA_PAD_FL_SOURCE;
		dev->fmt = ar0239_mbus_frmfmt_default;
		media_entity_pads_init(&dev->sd.entity, AR0239_PAD_NUM,
				       &dev->pad);
	}

	/* add async subdevs and register notifier */
	ret = v4l2_async_register_subdev_sensor_common(&dev->sd);
	if (ret < 0) {
		loge("v4l2_async_register_subdev returned %d\n", ret);
	}

	/* init framerate */
	dev->framerate = AR0239_DEFAULT_FRAMERATE;

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &ar0239_regmap);
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

int ar0239_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct ar0239		*dev	= to_dev(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	v4l2_ctrl_handler_free(&dev->hdl);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver ar0239_driver = {
	.probe		= ar0239_probe,
	.remove		= ar0239_remove,
	.driver		= {
		.name		= "ar0239",
		.of_match_table	= of_match_ptr(ar0239_of_match),
	},
	.id_table	= ar0239_id,
};

module_i2c_driver(ar0239_driver);
MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips AR0239 Driver");
MODULE_LICENSE("GPL");