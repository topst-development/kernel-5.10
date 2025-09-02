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

#define LOG_TAG				"VSRC:MAX9286"

#define loge(fmt, ...)			\
	pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(fmt, ...)			\
	pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(fmt, ...)			\
	pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(fmt, ...)			\
	pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)

#define	NUM_CHANNELS			(4)

#define MAX9286_REG_STATUS_1		(0x1E)
#define MAX9286_VAL_STATUS_1		(0x40)

#define MAX9286_PAD_SINK0		(0U)
#define MAX9286_PAD_SINK1		(1U)
#define MAX9286_PAD_SINK2		(2U)
#define MAX9286_PAD_SINK3		(3U)
#define MAX9286_PAD_SRC			(4U)

#define MAX9286_PAD_SINK_NUM		(4U)
#define MAX9286_PAD_NUM			(5U)

static const uint64_t max9286_pad_flag[MAX9286_PAD_NUM] = {
	MEDIA_PAD_FL_SINK,
	MEDIA_PAD_FL_SINK,
	MEDIA_PAD_FL_SINK,
	MEDIA_PAD_FL_SINK,
	MEDIA_PAD_FL_SOURCE,
};

static const struct v4l2_mbus_framefmt
	max9286_mbus_frmfmt_default[MAX9286_PAD_SINK_NUM] = {
		/* SRC0 PAD */
		{
			.width = 1,
			.height = 1,
			.code = MEDIA_BUS_FMT_UYVY8_2X8,
			.field = V4L2_FIELD_NONE,
		},
		/* SRC1 PAD */
		{
			.width = 1,
			.height = 1,
			.code = MEDIA_BUS_FMT_UYVY8_2X8,
			.field = V4L2_FIELD_NONE,
		},
		/* SRC2 PAD */
		{
			.width = 1,
			.height = 1,
			.code = MEDIA_BUS_FMT_UYVY8_2X8,
			.field = V4L2_FIELD_NONE,
		},
		/* SR3 PAD */
		{
			.width = 1,
			.height = 1,
			.code = MEDIA_BUS_FMT_UYVY8_2X8,
			.field = V4L2_FIELD_NONE,
		},
	};

struct power_sequence {
	int				pwr_port;
	int				pwd_port;
	int				rst_port;
	int				intb_port;

	enum of_gpio_flags		pwr_value;
	enum of_gpio_flags		pwd_value;
	enum of_gpio_flags		rst_value;
	enum of_gpio_flags		intb_value;
};

/*
 * This object contains essential v4l2 objects
 * such as sub-device and ctrl_handler
 */
struct max9286 {
	struct i2c_client		*clinet;
	struct v4l2_subdev		sd;

	struct v4l2_async_subdev	asd;
	struct v4l2_async_notifier	nf;

	struct media_pad		pads[MAX9286_PAD_NUM];
	struct v4l2_mbus_framefmt	fmt[MAX9286_PAD_SINK_NUM];

	struct power_sequence		gpio;

	/* Regmaps */
	struct regmap			*regmap;

	struct mutex lock;
	unsigned int p_cnt;
	unsigned int s_cnt;
	unsigned int i_cnt;

	bool				broadcasting_mode;
};

const struct reg_sequence max9286_reg_defaults[] = {
	// init
	// enable 4ch
	{0X0A, 0X0F, 0},	/* Disable all Forward control channel */
	{0X34, 0X35, 0},	/* Disable auto acknowledge */
	{0X15, 0X83, 0},	/*
				 * Select the combined camera line format
				 * for CSI2 output
				 */
	{0X12, 0XF3, 5*1000},	/* MIPI Output setting(DBL ON, YUV422) */
	{0X63, 0X00, 0},	/* Widows off */
	{0X64, 0X00, 0},	/* Widows off */
	{0X62, 0X1F, 0},	/* FRSYNC Diff off */

	{0x01, 0xc0, 0},	/* manual mode */
	{0X08, 0X25, 0},	/* FSYNC-period-High */
	{0X07, 0XC3, 0},	/* FSYNC-period-Mid */
	{0X06, 0XF8, 5*1000},	/* FSYNC-period-Low */
	{0X00, 0XEF, 5*1000},	/* Port 0~3 used */
#if defined(CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT)
	{0X15, 0X03, 0},	/* (line concatenation) */
#else
	{0X15, 0X93, 0},	/* (line interleave) */
#endif
	{0X69, 0XF0, 0},	/* Auto mask & comabck enable */
	{0x01, 0x00, 0},
	{0X0A, 0XFF, 0},	/* All forward channel enable */
};

/* for raw12 1ch input(mcnex ar0147) */
const struct reg_sequence max9286_reg_defaults_raw12[] = {
	{0X0A, 0X0F, 0},	/* Disable all Forward control channel */
	{0X34, 0Xb5, 0},	/* Enable auto acknowledge */
	{0X15, 0X83, 0},	/*
				 * Select the combined camera line format
				 * for CSI2 output
				 */
	{0X12, 0Xc7, 0},	/* Write DBL OFF, MIPI Output setting(RAW12) */
	{0X1C, 0xf6, 5*1000},	/* BWS: 27bit */
	{0X63, 0X00, 0},	/* Widows off */
	{0X64, 0X00, 0},	/* Widows off */
	{0X62, 0X1F, 0},	/* FRSYNC Diff off */

	{0X00, 0XE1, 0},	/* Port 0 used */
	{0x0c, 0x11, 5*1000},	/* disable HS/VS encoding */
	{0X15, 0X93, 0},	/* (line interleave) */
	{0X69, 0XF0, 0},	/* Auto mask & comabck enable */
	{0X0A, 0XFF, 0},	/* All forward channel enable */
};

static const struct regmap_config max9286_regmap = {
	.reg_bits		= 8,
	.val_bits		= 8,

	.max_register		= 0xFF,
	.cache_type		= REGCACHE_NONE,
};

static int max9286_nf_bound(struct v4l2_async_notifier *nf,
			    struct v4l2_subdev *sd,
			    struct v4l2_async_subdev *asd)
{
	struct max9286 *dev;
	int ret = 0;

	dev = container_of(nf, struct max9286, nf);

	logi("v4l2-subdev %s is bounded\n", sd->name);

	return ret;
}

static void max9286_nf_unbind(struct v4l2_async_notifier *nf,
			      struct v4l2_subdev *sd,
			      struct v4l2_async_subdev *asd)
{
	struct max9286 *dev = NULL;

	dev = container_of(nf, struct max9286, nf);

	logi("v4l2-subdev %s is unbounded\n", sd->name);
}

static const struct v4l2_async_notifier_operations max9286_nf_ops = {
	.bound = max9286_nf_bound,
	.unbind = max9286_nf_unbind,
};

static int max9286_parse_device_tree(struct max9286 *dev,
				     struct device_node *node)
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
	dev->gpio.intb_port = of_get_named_gpio_flags(node,
		"intb-gpios", 0, &dev->gpio.intb_value);

	dev->broadcasting_mode =
		of_property_read_bool(node, "broadcasting-mode");

	return ret;
}

/*
 * gpio functions
 */
void max9286_request_gpio(struct max9286 *dev)
{
	if (dev->gpio.pwr_port > 0) {
		/* power */
		gpio_request(dev->gpio.pwr_port, "max9286 power");
	}
	if (dev->gpio.pwd_port > 0) {
		/* power-down */
		gpio_request(dev->gpio.pwd_port, "max9286 power-down");
	}
	if (dev->gpio.rst_port > 0) {
		/* reset */
		gpio_request(dev->gpio.rst_port, "max9286 reset");
	}
	if (dev->gpio.intb_port > 0) {
		/* intb */
		gpio_request(dev->gpio.intb_port, "max9286 interrupt");
	}
}

void max9286_free_gpio(struct max9286 *dev)
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
	if (dev->gpio.intb_port > 0) {
		/* intb */
		gpio_free(dev->gpio.intb_port);
	}
}

/*
 * Helper functions for reflection
 */
static inline struct max9286 *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct max9286, sd);
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int max9286_init(struct v4l2_subdev *sd, u32 enable)
{
	struct max9286 *dev = to_dev(sd);
	int ret = 0;

	mutex_lock(&dev->lock);

	if (enable == 1) {
		if (dev->i_cnt == 0) {
			ret = regmap_multi_reg_write(dev->regmap,
				max9286_reg_defaults,
				ARRAY_SIZE(max9286_reg_defaults));
			if (ret < 0) {
				/* failed to write i2c */
				loge("regmap_multi_reg_write returned %d\n",
				     ret);
			}
		}
		dev->i_cnt++;
	} else {
		dev->i_cnt--;
		if (dev->i_cnt == 0) {
			/* ret = regmap_write(dev->regmap, 0x15, 0x93); */
		}
	}

	mutex_unlock(&dev->lock);

	return ret;
}

static int max9286_set_power(struct v4l2_subdev *sd, int on)
{
	struct max9286 *dev = to_dev(sd);
	struct power_sequence *gpio = &dev->gpio;

	mutex_lock(&dev->lock);

	if (on) {
		if (dev->p_cnt == 0) {
			/* port configuration */
			if (dev->gpio.pwr_port > 0) {
				gpio_direction_output(dev->gpio.pwr_port,
					dev->gpio.pwr_value);
				logd("[pwr] gpio: %3d, val: %d, cur val: %d\n",
					dev->gpio.pwr_port,
					dev->gpio.pwr_value,
					gpio_get_value(dev->gpio.pwr_port));
			}
			if (dev->gpio.pwd_port > 0) {
				gpio_direction_output(dev->gpio.pwd_port,
					dev->gpio.pwd_value);
				logd("[pwd] gpio: %3d, val: %d, cur val: %d\n",
					dev->gpio.pwd_port,
					dev->gpio.pwd_value,
					gpio_get_value(dev->gpio.pwd_port));
			}
			if (dev->gpio.rst_port > 0) {
				gpio_direction_output(dev->gpio.rst_port,
					dev->gpio.rst_value);
				logd("[rst] gpio: %3d, val: %d, cur val: %d\n",
					dev->gpio.rst_port,
					dev->gpio.rst_value,
					gpio_get_value(dev->gpio.rst_port));
			}
			if (dev->gpio.intb_port > 0) {
				gpio_direction_input(dev->gpio.intb_port);
				logd("[int] gpio: %3d, val: %d, cur val: %d\n",
					dev->gpio.intb_port,
					dev->gpio.intb_value,
					gpio_get_value(dev->gpio.intb_port));
			}

			/* power-up sequence */
			if (dev->gpio.pwd_port > 0) {
				gpio_set_value_cansleep(gpio->pwd_port, 1);
				msleep(20);
			}
			if (dev->gpio.rst_port > 0) {
				gpio_set_value_cansleep(gpio->rst_port, 1);
				msleep(20);
			}
		}
		dev->p_cnt++;
	} else {
		dev->p_cnt--;
		if (dev->p_cnt == 0) {
			/* power-down sequence */
			if (dev->gpio.rst_port > 0) {
				gpio_set_value_cansleep(gpio->rst_port, 0);
				msleep(20);
			}
			if (dev->gpio.pwd_port > 0) {
				gpio_set_value_cansleep(gpio->pwd_port, 0);
				msleep(20);
			}
		}
	}

	mutex_unlock(&dev->lock);

	return 0;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int max9286_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	struct max9286 *dev = to_dev(sd);
	unsigned int val = 0;
	int ret = 0;

	mutex_lock(&dev->lock);

	/* reset status */
	*status	= 0;

	/* check V4L2_IN_ST_NO_SIGNAL */
	ret = regmap_read(dev->regmap, MAX9286_REG_STATUS_1, &val);
	if (ret < 0) {
		loge("failure to check MAX9286_REG_STATUS_1\n");
		*status =
			V4L2_IN_ST_NO_POWER |
			V4L2_IN_ST_NO_SIGNAL |
			V4L2_IN_ST_NO_COLOR;
		goto end;
	} else {
		logd("status 1: 0x%08x\n", val);

		if (val != MAX9286_VAL_STATUS_1) {
			logw("STATUS_1 is V4L2_IN_ST_NO_SIGNAL\n");
			*status |= V4L2_IN_ST_NO_SIGNAL;
			goto end;
		}
	}

end:
	mutex_unlock(&dev->lock);

	logi("status: 0x%08x\n", *status);
	return ret;
}

static int max9286_src_sd_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct v4l2_subdev *src_sd;
	struct media_link *flink;
	int ret = 0;

	list_for_each_entry (flink, &sd->entity.links, list) {
		if (flink->sink->entity == &sd->entity) {
			src_sd = media_entity_to_v4l2_subdev(
				flink->source->entity);
			ret = v4l2_subdev_call(src_sd, video, s_stream,
					       enable);
			if (ret < 0) {
				/* failure of s_stream */
				loge("subdev_call(%s - %s %s) returned %d\n",
				     src_sd->name, "s_stream",
				     enable ? "enable" : "disabled", ret);
			}
		}
	}

	return ret;
}

static int max9286_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct max9286 *dev = to_dev(sd);
	int ret = 0;

	mutex_lock(&dev->lock);

	if (enable == 1) {
		if (dev->s_cnt == 0) {
			ret = max9286_src_sd_s_stream(sd, enable);
			if (ret < 0) {
				/* error */
				loge("max9286_src_sd_s_stream returned %d\n",
				     ret);
			}
#if defined(CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT)
			ret = regmap_write(dev->regmap, 0x15, 0x0B);
#else
			ret = regmap_write(dev->regmap, 0x15, 0x9B);
#endif
			if (ret < 0) {
				/* failure of enabling output  */
				loge("Fail enable output of max9286 device\n");
			}
		}
		/* count up */
		dev->s_cnt++;
	} else {
		/* count down */
		dev->s_cnt--;
		if (dev->s_cnt == 0) {
			ret = max9286_src_sd_s_stream(sd, enable);
			if (ret < 0) {
				/* error */
				loge("max9286_src_sd_s_stream returned %d\n",
				     ret);
			}

#if defined(CONFIG_MIPI_OUTPUT_TYPE_LINE_CONCAT)
			ret = regmap_write(dev->regmap, 0x15, 0x03);
#else
			ret = regmap_write(dev->regmap, 0x15, 0x93);
#endif
			if (ret < 0) {
				/* failure of disabling output  */
				loge("Fail disable output of max9286 device\n");
			}
		}
	}

	msleep(30);

	mutex_unlock(&dev->lock);
	return ret;
}

/*
 * v4l2_subdev_pad_ops implementations
 */
static int32_t max9286_init_cfg(struct v4l2_subdev *sd,
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

static int max9286_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *f)
{
	struct max9286 *dev = to_dev(sd);
	int ret = 0;

	mutex_lock(&dev->lock);

	if (f->pad >= MAX9286_PAD_NUM) {
		/* error */
		loge("invalid pad num(%d)\n", f->pad);
		ret = -EINVAL;
	} else {
		if (f->which == V4L2_SUBDEV_FORMAT_TRY) {
			/* get try format */
			f->format =
				*v4l2_subdev_get_try_format(sd, cfg, f->pad);
		} else {
			if (f->pad == MAX9286_PAD_SRC) {
				/* TODO:
				 * if each camera is different format,
				 * which format should be the format
				 * of source pad?
				 */
				f->format = dev->fmt[MAX9286_PAD_SINK0];
			} else {
				/* get active format */
				f->format = dev->fmt[f->pad];
			}
		}
	}

	mutex_unlock(&dev->lock);

	return ret;
}

static int max9286_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *f)
{
	struct max9286 *dev = to_dev(sd);
	struct v4l2_mbus_framefmt *fmt = NULL;
	int ret = 0;

	mutex_lock(&dev->lock);

	/* check pad */
	if (f->pad >= MAX9286_PAD_SINK_NUM) {
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
			fmt = &dev->fmt[f->pad];
		}
	}

	*fmt = f->format;

	mutex_unlock(&dev->lock);

	return ret;
}

/*
 * v4l2_subdev_internal_ops implementations
 */
static int max9286_registered(struct v4l2_subdev *sd)
{
	int ret = 0;

	logd("registered with v4l2 dev\n");

	return ret;
}

/*
 * v4l2_subdev_ops implementations
 */
static const struct v4l2_subdev_core_ops max9286_core_ops = {
	.init			= max9286_init,
	.s_power		= max9286_set_power,
};

static const struct v4l2_subdev_video_ops max9286_video_ops = {
	.g_input_status		= max9286_g_input_status,
	.s_stream		= max9286_s_stream,
};

static const struct v4l2_subdev_pad_ops max9286_pad_ops = {
	.init_cfg		= max9286_init_cfg,
	.get_fmt		= max9286_get_fmt,
	.set_fmt		= max9286_set_fmt,
};

static const struct v4l2_subdev_ops max9286_ops = {
	.core			= &max9286_core_ops,
	.video			= &max9286_video_ops,
	.pad			= &max9286_pad_ops,
};

static const struct v4l2_subdev_internal_ops max9286_internal_ops = {
	.registered = max9286_registered,
};

struct max9286 max9286_data = {
};

static const struct i2c_device_id max9286_id[] = {
	{ "max9286", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max9286_id);

#if IS_ENABLED(CONFIG_OF)
const static struct of_device_id max9286_of_match[] = {
	{
		.compatible	= "tcc-maxim,max9286",
		.data		= &max9286_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, max9286_of_match);
#endif

static int max9286_add_asd(struct max9286 *dev)
{
	struct device_node *np = dev->sd.dev->of_node;
	struct device_node *ep;
	unsigned int num_pads = 0;
	unsigned int i;
	int ret = 0;

	for_each_endpoint_of_node(np, ep) {
		struct of_endpoint endpoint;

		of_graph_parse_endpoint(ep, &endpoint);
		num_pads = max(num_pads, endpoint.port + 1);
	}

	num_pads -= 1;

	for (i = 0; i < num_pads; i++) {
		v4l2_async_notifier_parse_fwnode_endpoints_by_port(
			dev->sd.dev, &dev->nf, sizeof(struct v4l2_async_subdev),
			i, NULL);
	}
	if (ret < 0) {
		loge("v4l2_async_nf_parse_fwnode_endpoints_by_port returned %d\n",
		     ret);
	} else {
		struct v4l2_async_subdev *found_asd;
		list_for_each_entry (found_asd, &dev->nf.asd_list,
				     asd_list) {
			logi("asd %s has been found\n",
			     to_of_node(found_asd->match.fwnode)->name);
		}
	}

	return ret;
}

int max9286_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct max9286			*dev	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	uint32_t			idx	= 0U;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct max9286), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(max9286_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	mutex_init(&dev->lock);

	/* parse device tree */
	ret = max9286_parse_device_tree(dev, client->dev.of_node);
	if (ret < 0) {
		loge("max9286_parse_device_tree, ret: %d\n", ret);
		return ret;
	}

	/* Register with V4L2 layer as a slave device */
	v4l2_i2c_subdev_init(&dev->sd, client, &max9286_ops);
	dev->sd.internal_ops = &max9286_internal_ops;
	dev->sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->sd.entity.function = MEDIA_ENT_F_VID_IF_BRIDGE;
	dev->sd.entity.flags = MEDIA_ENT_FL_DEFAULT;

	if (ret >= 0) {
		/* init pads */
		for (idx = 0U; idx < MAX9286_PAD_NUM; idx++) {
			dev->pads[idx].index = idx;
			dev->pads[idx].flags = max9286_pad_flag[idx];

			if (idx < MAX9286_PAD_SINK_NUM) {
				dev->fmt[idx] =
					max9286_mbus_frmfmt_default[idx];
			}
		}
		media_entity_pads_init(&dev->sd.entity, MAX9286_PAD_NUM,
				       dev->pads);

		/* init notifier */
		v4l2_async_notifier_init(&dev->nf);
		dev->nf.ops = &max9286_nf_ops;
	}

	/* add async subdevs */
	if (ret >= 0) {
		ret = max9286_add_asd(dev);
		if (ret < 0) {
			loge("max9286_add_asd returned %d\n", ret);
		}
	}

	/* register async notifier */
	if (ret >= 0) {
		ret = v4l2_async_subdev_notifier_register(&dev->sd, &dev->nf);
		if (ret < 0) {
			loge("v4l2_async_subdev_notifier_register, ret: %d\n",
			     ret);
			v4l2_async_notifier_cleanup(&dev->nf);
		}
	}

	/* register a v4l2 sub device */
	if (ret >= 0) {
		ret = v4l2_async_register_subdev(&(dev->sd));
		if (ret < 0) {
			/* error */
			loge("v4l2_async_register_subdev returned %d\n", ret);
		}
	}


	/* request gpio */
	max9286_request_gpio(dev);

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &max9286_regmap);
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

int max9286_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct max9286		*dev	= to_dev(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	/* gree gpio */
	max9286_free_gpio(dev);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver max9286_driver = {
	.probe		= max9286_probe,
	.remove		= max9286_remove,
	.driver		= {
		.name		= "max9286",
		.of_match_table	= of_match_ptr(max9286_of_match),
	},
	.id_table	= max9286_id,
};

module_i2c_driver(max9286_driver);
MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips MAX96705 Driver");
MODULE_LICENSE("GPL");
