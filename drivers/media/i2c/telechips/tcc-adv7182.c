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

#define LOG_TAG				"VSRC:ADV7182"

#define loge(fmt, ...)			\
	pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(fmt, ...)			\
	pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(fmt, ...)			\
	pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(fmt, ...)			\
	pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)

#define	DEFAULT_FRAMERATE		(60)

#define ADV7182_REG_STATUS_1		(0x10)
#define ADV7182_VAL_STATUS_1_COL_KILL	(1 << 7)
#define ADV7182_VAL_STATUS_1_FSC_LOCK	(1 << 2)
#define ADV7182_VAL_STATUS_1_IN_LOCK	(1 << 0)

#define ADV7182_REG_STATUS_3		(0x13)
#define ADV7182_VAL_STATUS_3_INST_HLOCK	(1 << 0)

#define ADV7182_DEFAULT_WIDTH		(720U)
#define ADV7182_DEFAULT_HEIGHT		(480U)

#define ADV7182_PAD_SRC			(0U)
#define ADV7182_PAD_NUM			(1U)

static const struct v4l2_mbus_framefmt adv7182_mbus_frmfmt_default = {
	.width = ADV7182_DEFAULT_WIDTH,
	.height	= ADV7182_DEFAULT_HEIGHT,
	.code = MEDIA_BUS_FMT_UYVY8_2X8,
	.field = V4L2_FIELD_INTERLACED,
};

struct power_sequence {
	int				pwr_port;
	int				pwd_port;
	int				rst_port;

	enum of_gpio_flags		pwr_value;
	enum of_gpio_flags		pwd_value;
	enum of_gpio_flags		rst_value;
};

struct frame_size {
	u32 width;
	u32 height;
};

/*
 * This object contains essential v4l2 objects
 * such as sub-device and ctrl_handler
 */
struct adv7182 {
	struct i2c_client		*clinet;
	struct v4l2_subdev		sd;
	struct v4l2_ctrl_handler	hdl;

	struct media_pad		pad;
	struct v4l2_mbus_framefmt	fmt;
	int				framerate;

	struct power_sequence		gpio;

	/* Regmaps */
	struct regmap			*regmap;
};

const struct reg_sequence adv7182_reg_defaults[] = {
	{0x0f, 0x00, 0},	/* Exit Power Down Mode */
	{0x00, 0x00, 0},	/* INSEL = CVBS in on Ain 1 */
	{0x03, 0x0c, 0},	/* Enable Pixel & Sync output drivers */
	{0x04, 0x17, 0},	/* Power-up INTRQ pad & Enable SFL */
	{0x13, 0x00, 0},	/* Enable INTRQ output driver */
	{0x17, 0x41, 0},	/* select SH1 */
	{0x1d, 0x40, 0},	/* Enable LLC output driver */
	{0x52, 0xcb, 0},	/* ADI Recommended Writes */
	{0x0e, 0x80, 0},	/* ADI Recommended Writes */
	{0xd9, 0x44, 0},	/* ADI Recommended Writes */
	{0x0e, 0x00, 0},	/* ADI Recommended Writes */
	{0x0e, 0x40, 0},	/* Select User Sub Map 2 */
	{0xe0, 0x01, 0},	/* Select fast Switching Mode */
	{0x0e, 0x00, 0},	/* Select User Map */
};

static const struct regmap_config adv7182_regmap = {
	.reg_bits		= 8,
	.val_bits		= 8,

	.max_register		= 0xFF,
	.cache_type		= REGCACHE_NONE,
};

static struct frame_size adv7182_framesizes[] = {
	{	720,	480	},
};

static u32 adv7182_framerates[] = {
	60,
};

struct v4l2_dv_timings adv7182_dv_timings = {
	.type		= V4L2_DV_BT_656_1120,
	.bt		= {
		.width		= ADV7182_DEFAULT_WIDTH,
		.height		= ADV7182_DEFAULT_HEIGHT,
		.interlaced	= V4L2_DV_INTERLACED,
		/* IMPORTANT
		 * The below field "polarities" is not used
		 * becasue polarities for vsync and hsync are supported only.
		 * So, use flags of "struct v4l2_mbus_config".
		 */
		.polarities	= 0,
	},
};

static u32 adv7182_codes[] = {
	MEDIA_BUS_FMT_UYVY8_2X8,
};

struct v4l2_mbus_config adv7182_mbus_config = {
	.type		= V4L2_MBUS_BT656,
	/* de: high, vs: high, hs: low, pclk: high */
	.flags		=
		V4L2_MBUS_DATA_ACTIVE_HIGH	|
		V4L2_MBUS_VSYNC_ACTIVE_HIGH	|
		V4L2_MBUS_HSYNC_ACTIVE_LOW	|
		V4L2_MBUS_PCLK_SAMPLE_RISING	|
		V4L2_MBUS_MASTER,
};

/*
 * gpio functions
 */
void adv7182_request_gpio(struct adv7182 *dev)
{
	if (dev->gpio.pwr_port > 0) {
		/* power */
		gpio_request(dev->gpio.pwr_port, "adv7182 power");
	}
	if (dev->gpio.pwd_port > 0) {
		/* power-down */
		gpio_request(dev->gpio.pwd_port, "adv7182 power-down");
	}
	if (dev->gpio.rst_port > 0) {
		/* reset */
		gpio_request(dev->gpio.rst_port, "adv7182 reset");
	}
}

void adv7182_free_gpio(struct adv7182 *dev)
{
	if (dev->gpio.pwr_port > 0) {
		/* power */
		gpio_free(dev->gpio.pwr_port);
	}
	if (dev->gpio.pwd_port > 0) {
		/* power-down */
		gpio_free(dev->gpio.pwd_port);
	}
	if (dev->gpio.rst_port > 0) {
		/* reset */
		gpio_free(dev->gpio.rst_port);
	}
}

/*
 * Helper functions for reflection
 */
static inline struct adv7182 *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct adv7182, sd);
}

/*
 * v4l2_ctrl_ops implementations
 */
static int adv7182_s_ctrl(struct v4l2_ctrl *ctrl)
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

static int adv7182_parse_device_tree(struct adv7182 *dev, struct device_node *node)
{
	int ret = 0;

	if (node == NULL) {
		loge("the device tree is empty\n");
		return -ENODEV;
	}

	dev->gpio.pwr_port = of_get_named_gpio_flags(node,
		"pwr-gpios", 0, &dev->gpio.pwr_value);
	dev->gpio.pwd_port = of_get_named_gpio_flags(node,
		"pwd-gpios", 0, &dev->gpio.pwd_value);
	dev->gpio.rst_port = of_get_named_gpio_flags(node,
		"rst-gpios", 0, &dev->gpio.rst_value);

	return ret;
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int adv7182_set_power(struct v4l2_subdev *sd, int on)
{
	struct adv7182		*dev	= to_state(sd);
	struct power_sequence	*gpio	= &dev->gpio;

	if (on) {
		/* port configuration */
		if (dev->gpio.pwr_port > 0) {
			gpio_direction_output(dev->gpio.pwr_port,
				dev->gpio.pwr_value);
			logd("[pwr] gpio: %3d, new val: %d, cur val: %d\n",
				dev->gpio.pwr_port, dev->gpio.pwr_value,
				gpio_get_value(dev->gpio.pwr_port));
		}
		if (dev->gpio.pwd_port > 0) {
			gpio_direction_output(dev->gpio.pwd_port,
				dev->gpio.pwd_value);
			logd("[pwd] gpio: %3d, new val: %d, cur val: %d\n",
				dev->gpio.pwd_port, dev->gpio.pwd_value,
				gpio_get_value(dev->gpio.pwd_port));
		}
		if (dev->gpio.rst_port > 0) {
			gpio_direction_output(dev->gpio.rst_port,
				dev->gpio.rst_value);
			logd("[rst] gpio: %3d, new val: %d, cur val: %d\n",
				dev->gpio.rst_port, dev->gpio.rst_value,
				gpio_get_value(dev->gpio.rst_port));
		}

		/* power-up sequence */
		if (dev->gpio.rst_port > 0) {
			gpio_set_value_cansleep(gpio->rst_port, 1);
			msleep(20);
		}
	} else {
		/* power-down sequence */
		if (dev->gpio.rst_port > 0) {
			gpio_set_value_cansleep(gpio->rst_port, 0);
			msleep(20);
		}
	}

	return 0;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int adv7182_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	struct adv7182		*dev	= to_state(sd);
	unsigned int		val	= 0;
	int			ret	= 0;

	/* reset status */
	*status	= 0;

	/* check STATUS 1 */
	ret = regmap_read(dev->regmap, ADV7182_REG_STATUS_1, &val);
	if (ret < 0) {
		loge("failure to check ADV7182_REG_STATUS_1\n");
		*status =
			V4L2_IN_ST_NO_POWER |
			V4L2_IN_ST_NO_SIGNAL |
			V4L2_IN_ST_NO_COLOR;
		goto end;
	} else {
		logd("status 1: 0x%08x\n", val);

		/* check sync signal lock */
		if (!(val & (ADV7182_VAL_STATUS_1_FSC_LOCK))) {
			logw("V4L2_IN_ST_NO_SIGNAL\n");
			*status |= V4L2_IN_ST_NO_SIGNAL;
		}

		/* check sync signal lock */
		if (!(val & (ADV7182_VAL_STATUS_1_IN_LOCK))) {
			logw("V4L2_IN_ST_NO_SIGNAL\n");
			*status |= V4L2_IN_ST_NO_SIGNAL;
		}

		/* check color kill */
		if (val & ADV7182_VAL_STATUS_1_COL_KILL) {
			logw("V4L2_IN_ST_COLOR_KILL\n");
			*status |= V4L2_IN_ST_COLOR_KILL;
		}
	}

	/* check STATUS 3 */
	ret = regmap_read(dev->regmap, ADV7182_REG_STATUS_3, &val);
	if (ret < 0) {
		loge("failure to check ADV7182_REG_STATUS_3\n");
		*status =
			V4L2_IN_ST_NO_POWER |
			V4L2_IN_ST_NO_SIGNAL |
			V4L2_IN_ST_NO_COLOR;
		goto end;
	} else {
		logd("status 3: 0x%08x\n", val);

		/* check sync signal lock */
		if (!(val & ADV7182_VAL_STATUS_3_INST_HLOCK)) {
			logw("V4L2_IN_ST_NO_H_LOCK\n");
			*status |= V4L2_IN_ST_NO_H_LOCK;
		}
	}

end:
	logi("status: 0x%08x\n", *status);
	return ret;
}

static int adv7182_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct adv7182 *dev = NULL;
	struct pinctrl *pctrl = NULL;
	int ret = 0;

	dev = to_state(sd);
	if (!dev) {
		loge("Failed to get video source object by subdev\n");
		ret = -EINVAL;
	} else {
		ret = regmap_multi_reg_write(dev->regmap,
			adv7182_reg_defaults,
			ARRAY_SIZE(adv7182_reg_defaults));
		if (ret < 0) {
			loge("regmap_multi_reg_write returned %d\n", ret);
		} else {
			msleep(50);
		}
	}

	/* pinctrl for cifport */
	pctrl = pinctrl_get_select(sd->dev, "default");
	if (IS_ERR(pctrl)) {
		pinctrl_put(pctrl);
		ret = -1;
	}

	return ret;
}

static int adv7182_g_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *interval)
{
	struct adv7182		*dev	= NULL;

	dev = to_state(sd);
	if (!dev) {
		loge("Failed to get video source object by subdev\n");
		return -EINVAL;
	}

	if (interval->pad != 0) {
		logd("pad(%u) is wrong\n", interval->pad);
		return -EINVAL;
	}

	interval->pad = 0;
	interval->interval.numerator = 1;
	interval->interval.denominator = dev->framerate;

	return 0;
}

static int adv7182_s_frame_interval(struct v4l2_subdev *sd,
				    struct v4l2_subdev_frame_interval *interval)
{
	struct adv7182		*dev	= NULL;

	dev = to_state(sd);
	if (!dev) {
		loge("Failed to get video source object by subdev\n");
		return -EINVAL;
	}

	if (interval->pad != 0) {
		logd("pad(%u) is wrong\n", interval->pad);
		return -EINVAL;
	}

	/* set framerate with i2c setting if supported */

	dev->framerate = interval->interval.denominator;

	return 0;
}

static int adv7182_g_dv_timings(struct v4l2_subdev *sd,
				struct v4l2_dv_timings *timings)
{
	int ret = 0;

	memcpy((void *)timings, (const void *)&adv7182_dv_timings,
		sizeof(*timings));

	return ret;
}

static int adv7182_get_mbus_config(struct v4l2_subdev *sd,
				   unsigned int pad,
				   struct v4l2_mbus_config *cfg)
{
	memcpy((void *)cfg, (const void *)&adv7182_mbus_config,
		sizeof(*cfg));

	return 0;
}

/*
 * v4l2_subdev_pad_ops implementations
 */
static int32_t adv7182_init_cfg(struct v4l2_subdev *sd,
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

static int adv7182_enum_frame_size(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct frame_size	*size		= NULL;

	if (ARRAY_SIZE(adv7182_framesizes) <= fse->index) {
		logd("index(%u) is wrong\n", fse->index);
		return -EINVAL;
	}

	size = &adv7182_framesizes[fse->index];
	logd("size: %u * %u\n", size->width, size->height);

	fse->min_width = fse->max_width = size->width;
	fse->min_height	= fse->max_height = size->height;
	logd("max size: %u * %u\n", fse->max_width, fse->max_height);

	return 0;
}

static int
adv7182_enum_frame_interval(struct v4l2_subdev *sd,
			    struct v4l2_subdev_pad_config *cfg,
			    struct v4l2_subdev_frame_interval_enum *fie)
{
	struct frame_size	*size		= NULL;
	int			idxSize		= 0;
	int			nSize		= 0;
	int			ret		= -EINVAL;

	nSize		= ARRAY_SIZE(adv7182_framesizes);
	for (idxSize = 0; idxSize < nSize; idxSize++) {
		size = &adv7182_framesizes[idxSize];
		if ((size->width == fie->width) &&
			(size->height == fie->height)) {
			loge("size (%u * %u) is supported\n",
				fie->width, fie->height);
			ret = 0;
		}
	}

	if (ret == 0) {
		if (ARRAY_SIZE(adv7182_framerates) <= fie->index) {
			loge("index(%u) is wrong\n", fie->index);
			return -EINVAL;
		}

		fie->interval.numerator = 1;
		fie->interval.denominator = adv7182_framerates[fie->index];
		logd("framerate: %u / %u\n",
			fie->interval.numerator, fie->interval.denominator);
	}

	return ret;
}

static int adv7182_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_pad_config *cfg,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	if ((code->pad != 0) || (ARRAY_SIZE(adv7182_codes) <= code->index))
		return -EINVAL;

	code->code = adv7182_codes[code->index];

	return 0;
}

static int adv7182_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *f)
{
	struct adv7182 *dev = to_state(sd);
	int ret = 0;

	if (f->pad >= ADV7182_PAD_NUM) {
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

	return ret;
}

static int adv7182_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *f)
{
	struct adv7182 *dev = to_state(sd);
	struct v4l2_mbus_framefmt *fmt = NULL;
	int ret = 0;

	/* check pad */
	if (f->pad >= ADV7182_PAD_NUM) {
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

			adv7182_dv_timings.bt.width = f->format.width;
			adv7182_dv_timings.bt.height = f->format.height;
		}
	}

	*fmt = f->format;

	return ret;
}

/*
 * v4l2_subdev_internal_ops implementations
 */
static int adv7182_registered(struct v4l2_subdev *sd)
{
	int ret = 0;

	logd("registered with v4l2 dev\n");

	return ret;
}

/*
 * v4l2_subdev_ops implementations
 */
static const struct v4l2_ctrl_ops adv7182_ctrl_ops = {
	.s_ctrl			= adv7182_s_ctrl,
};

static const struct v4l2_subdev_core_ops adv7182_v4l2_subdev_core_ops = {
	.s_power		= adv7182_set_power,
};

static const struct v4l2_subdev_video_ops adv7182_v4l2_subdev_video_ops = {
	.g_input_status		= adv7182_g_input_status,
	.s_stream		= adv7182_s_stream,
	.g_frame_interval	= adv7182_g_frame_interval,
	.s_frame_interval	= adv7182_s_frame_interval,
	.g_dv_timings		= adv7182_g_dv_timings,
};

static const struct v4l2_subdev_pad_ops adv7182_v4l2_subdev_pad_ops = {
	.init_cfg		= adv7182_init_cfg,
	.enum_mbus_code		= adv7182_enum_mbus_code,
	.enum_frame_size	= adv7182_enum_frame_size,
	.enum_frame_interval	= adv7182_enum_frame_interval,
	.get_fmt		= adv7182_get_fmt,
	.set_fmt		= adv7182_set_fmt,
	.get_mbus_config	= adv7182_get_mbus_config,
};

static const struct v4l2_subdev_ops adv7182_ops = {
	.core			= &adv7182_v4l2_subdev_core_ops,
	.video			= &adv7182_v4l2_subdev_video_ops,
	.pad			= &adv7182_v4l2_subdev_pad_ops,
};

static const struct v4l2_subdev_internal_ops adv7182_internal_ops = {
	.registered = adv7182_registered,
};

struct adv7182 adv7182_data = {
};

static const struct i2c_device_id adv7182_id[] = {
	{ "adv7182", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, adv7182_id);

#if IS_ENABLED(CONFIG_OF)
const static struct of_device_id adv7182_of_match[] = {
	{
		.compatible	= "tcc-adi,adv7182",
		.data		= &adv7182_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, adv7182_of_match);
#endif

int adv7182_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct adv7182			*dev	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct adv7182), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(adv7182_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	/* parse device tree */
	ret = adv7182_parse_device_tree(dev, client->dev.of_node);
	if (ret < 0) {
		loge("adv7182_parse_device_tree, ret: %d\n", ret);
		return ret;
	}

	/* regitster v4l2 control handlers */
	v4l2_ctrl_handler_init(&dev->hdl, 2);
	v4l2_ctrl_new_std(&dev->hdl, &adv7182_ctrl_ops,
		V4L2_CID_BRIGHTNESS, 0, 255, 1, 128);
	v4l2_ctrl_new_std_menu(&dev->hdl, &adv7182_ctrl_ops,
		V4L2_CID_DV_RX_IT_CONTENT_TYPE, V4L2_DV_IT_CONTENT_TYPE_NO_ITC,
		0, V4L2_DV_IT_CONTENT_TYPE_NO_ITC);
	dev->sd.ctrl_handler = &dev->hdl;
	if (dev->hdl.error) {
		loge("v4l2_ctrl_handler_init is wrong\n");
		ret = dev->hdl.error;
		goto goto_free_device_data;
	}

	/* Register with V4L2 layer as a slave device */
	v4l2_i2c_subdev_init(&dev->sd, client, &adv7182_ops);
	dev->sd.internal_ops = &adv7182_internal_ops;
	dev->sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->sd.entity.function = MEDIA_ENT_F_ATV_DECODER;
	dev->sd.entity.flags = MEDIA_ENT_FL_DEFAULT;

	if (ret >= 0) {
		/* init pads */
		dev->pad.index = 0U;
		dev->pad.flags = MEDIA_PAD_FL_SOURCE;
		dev->fmt = adv7182_mbus_frmfmt_default;
		media_entity_pads_init(&dev->sd.entity, ADV7182_PAD_NUM,
				       &dev->pad);
	}

	/* register a v4l2 sub device */
	if (ret >= 0) {
		ret = v4l2_async_register_subdev(&(dev->sd));
		if (ret < 0) {
			/* error */
			loge("v4l2_async_register_subdev returned %d\n", ret);
		}
	}

	/* init framerate */
	dev->framerate = DEFAULT_FRAMERATE;

	/* request gpio */
	adv7182_request_gpio(dev);

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &adv7182_regmap);
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

int adv7182_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct adv7182		*dev	= to_state(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	/* gree gpio */
	adv7182_free_gpio(dev);

	v4l2_ctrl_handler_free(&dev->hdl);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver adv7182_driver = {
	.probe		= adv7182_probe,
	.remove		= adv7182_remove,
	.driver		= {
		.name		= "adv7182",
		.of_match_table	= of_match_ptr(adv7182_of_match),
	},
	.id_table	= adv7182_id,
};

module_i2c_driver(adv7182_driver);
MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips MAX96705 Driver");
MODULE_LICENSE("GPL");
