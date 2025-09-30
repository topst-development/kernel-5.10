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

#define LOG_TAG				"VSRC:MAX96705"

#define loge(fmt, ...)			\
	pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(fmt, ...)			\
	pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(fmt, ...)			\
	pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(fmt, ...)			\
	pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)

#define MAX96705_PAD_SINK	(0U)
#define MAX96705_PAD_SRC	(1U)
#define MAX96705_PAD_NUM	(2U)

static const uint64_t max96705_pad_flag[MAX96705_PAD_NUM] = {
	MEDIA_PAD_FL_SINK,
	MEDIA_PAD_FL_SOURCE,
};

static const struct v4l2_mbus_framefmt max96705_mbus_frmfmt_default = {
	.width = 1,
	.height = 1,
	.code = MEDIA_BUS_FMT_UYVY8_2X8,
	.field = V4L2_FIELD_NONE,
};

struct max96705_prv_data {
	const char * const name;
	const struct reg_sequence *init;
	const uint32_t init_size;
	const struct reg_sequence *s_stream;
	const uint32_t s_stream_size;
};

/*
 * This object contains essential v4l2 objects
 * such as sub-device and ctrl_handler
 */
struct max96705 {
	struct i2c_client		*clinet;
	struct v4l2_subdev		sd;

	struct v4l2_async_subdev	asd;
	struct v4l2_async_notifier	nf;
	struct v4l2_subdev		*src_sd;

	struct media_pad		pads[MAX96705_PAD_NUM];
	struct v4l2_mbus_framefmt	fmt;

	/* Regmaps */
	struct regmap			*regmap;

	struct mutex lock;
	unsigned int p_cnt;
	unsigned int s_cnt;
	unsigned int i_cnt;

	/* private data */
	const char *pvd_name;
	struct max96705_prv_data *pvd;
};

const struct reg_sequence max96705_reg_init_fhd_ar0231[] = {
	//MAX96705 SER
	//{0x67, 0xE7, 10*1000},	/* AUTO CLINK EN = 1, Delay 10ms */
	{0x04, 0x47, 10 * 1000},	/* Enable CLINK*/
	{0x07, 0xC4, 0},		/* Enable HVEN, DBL, HIBW */
};

static struct max96705_prv_data max96705_prv_data_list[] = {
	{
		.name = "fhd_ar0231",
		.init = max96705_reg_init_fhd_ar0231,
		.init_size = ARRAY_SIZE(max96705_reg_init_fhd_ar0231),
		.s_stream = NULL,
		.s_stream_size = 0U,
	},
	{
	},
};

static const struct regmap_config max96705_regmap = {
	.reg_bits		= 8,
	.val_bits		= 8,

	.max_register		= 0xFF,
	.cache_type		= REGCACHE_NONE,
};

static struct max96705_prv_data *
find_max96705_prv_data(const char *what)
{
	uint32_t idx = 0;

	while (max96705_prv_data_list[idx].name != NULL) {
		if (!strncmp(max96705_prv_data_list[idx].name, what,
			strlen(what))) {
			/* match */
			break;
		}
		/* didn't match */
		idx++;
	}

	if (max96705_prv_data_list[idx].name == NULL) {
		/* error */
		loge("can not find max96705_prv_data\n");
	} else {
		/* success */
		logi("found max96705_prv_data %s\n",
		     max96705_prv_data_list[idx].name);
	}

	return &max96705_prv_data_list[idx];
}

static int max96705_parse_device_tree(struct max96705 *dev,
				      struct device_node *node)
{
	int ret = 0;

	if (node == NULL) {
		loge("the device tree is empty\n");
		return -ENODEV;
	}

	ret = of_property_read_string(node, "pvd-name",
				      &dev->pvd_name);
	if (ret < 0) {
		loge("FAIL - of_property_read_string returns %d\n",
		     ret);
	}

	return ret;
}

static int max96705_nf_bound(struct v4l2_async_notifier *nf,
			     struct v4l2_subdev *sd,
			     struct v4l2_async_subdev *asd)
{
	struct max96705 *dev;
	int ret = 0;

	dev = container_of(nf, struct max96705, nf);

	logi("v4l2-subdev %s is bounded\n", sd->name);

	dev->src_sd = sd;

	return ret;
}

static void max96705_nf_unbind(struct v4l2_async_notifier *nf,
			       struct v4l2_subdev *sd,
			       struct v4l2_async_subdev *asd)
{
	struct max96705 *dev = NULL;

	dev = container_of(nf, struct max96705, nf);

	logi("v4l2-subdev %s is unbounded\n", sd->name);
}

static const struct v4l2_async_notifier_operations max96705_nf_ops = {
	.bound = max96705_nf_bound,
	.unbind = max96705_nf_unbind,
};


/*
 * Helper functions for reflection
 */
static inline struct max96705 *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct max96705, sd);
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int max96705_init(struct v4l2_subdev *sd, u32 enable)
{
	struct max96705		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if ((dev->i_cnt == 0) && (enable == 1)) {
		ret = regmap_multi_reg_write(dev->regmap,
					     dev->pvd->init,
					     dev->pvd->init_size);
		if (ret < 0) {
			/* failed to write i2c */
			loge("regmap_multi_reg_write returned %d\n", ret);
		}
		if (ret < 0)
			loge("Fail initializing max96705 device\n");
	} else if ((dev->i_cnt == 1) && (enable == 0)) {
		/* ret = regmap_write(dev->regmap, 0x15, 0x93); */
	}

	if (enable)
		dev->i_cnt++;
	else
		dev->i_cnt--;

	mutex_unlock(&dev->lock);

	msleep(100);

	return ret;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int max96705_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct max96705 *dev = to_dev(sd);
	int ret = 0;

	mutex_lock(&dev->lock);

	logi("subdev call(%s - %s)\n", dev->src_sd->name, "s_stream");

	if ((dev->s_cnt == 0) && (enable == 1)) {
		ret = v4l2_subdev_call(dev->src_sd, video, s_stream,
				       enable);
		if (ret < 0) {
			/* failure of s_stream */
			loge("subdev_call(%s - %s %s) returned %d\n",
			     dev->src_sd->name, "s_stream",
			     enable ? "enable" : "disabled", ret);
		}

		ret = regmap_write(dev->regmap, 0x04, 0x87);
		if (ret < 0)
			loge("Fail Serialization max96705 device\n");
	} else if ((dev->s_cnt == 1) && (enable == 0)) {
		ret = v4l2_subdev_call(dev->src_sd, video, s_stream,
				       enable);
		if (ret < 0) {
			/* failure of s_stream */
			loge("subdev_call(%s - %s %s) returned %d\n",
			     dev->src_sd->name, "s_stream",
			     enable ? "enable" : "disabled", ret);
		}

		ret = regmap_write(dev->regmap, 0x04, 0x47);
	}

	if (enable) {
		/* count up */
		dev->s_cnt++;
	} else {
		/* count down */
		dev->s_cnt--;
	}

	msleep(30);

	mutex_unlock(&dev->lock);
	return ret;
}

/*
 * v4l2_subdev_pad_ops implementations
 */
static int32_t max96705_init_cfg(struct v4l2_subdev *sd,
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

static int max96705_get_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_pad_config *cfg,
			    struct v4l2_subdev_format *f)
{
	struct max96705 *dev = to_dev(sd);
	int ret = 0;

	mutex_lock(&dev->lock);

	if (f->pad >= MAX96705_PAD_NUM) {
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

static int max96705_set_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_pad_config *cfg,
			    struct v4l2_subdev_format *f)
{
	struct max96705 *dev = to_dev(sd);
	struct v4l2_mbus_framefmt *fmt = NULL;
	int ret = 0;

	mutex_lock(&dev->lock);

	/* check pad */
	if (f->pad >= MAX96705_PAD_NUM) {
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
static int max96705_registered(struct v4l2_subdev *sd)
{
	int ret = 0;

	logd("registered with v4l2 dev\n");

	return ret;
}

/*
 * v4l2_subdev_ops implementations
 */
static const struct v4l2_subdev_core_ops max96705_core_ops = {
	.init			= max96705_init,
};

static const struct v4l2_subdev_video_ops max96705_video_ops = {
	.s_stream		= max96705_s_stream,
};

static const struct v4l2_subdev_pad_ops max96705_pad_ops = {
	.init_cfg		= max96705_init_cfg,
	.get_fmt		= max96705_get_fmt,
	.set_fmt		= max96705_set_fmt,
};

static const struct v4l2_subdev_ops max96705_ops = {
	.core			= &max96705_core_ops,
	.video			= &max96705_video_ops,
	.pad			= &max96705_pad_ops,
};

static const struct v4l2_subdev_internal_ops max96705_internal_ops = {
	.registered = max96705_registered,
};

struct max96705 max96705_data = {
};

static const struct i2c_device_id max96705_id[] = {
	{ "max96705", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max96705_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id max96705_of_match[] = {
	{
		.compatible	= "tcc-maxim,max96705",
		.data		= &max96705_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, max96705_of_match);
#endif

static int max96705_add_asd(struct max96705 *dev)
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

int max96705_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct max96705			*dev	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	uint32_t			idx	= 0U;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct max96705), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(max96705_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	mutex_init(&dev->lock);

	/* parse device tree */
	ret = max96705_parse_device_tree(dev, client->dev.of_node);
	if (ret < 0) {
		loge("max96705_parse_device_tree, ret: %d\n", ret);
		return ret;
	}

	dev->pvd = find_max96705_prv_data(dev->pvd_name);
	if (dev->pvd == NULL) {
		/* error */
		loge("Fail - pvd_data(%s)\n", dev->pvd_name);
		goto goto_free_device_data;
	}

	/* Register with V4L2 layer as a slave device */
	v4l2_i2c_subdev_init(&dev->sd, client, &max96705_ops);
	dev->sd.internal_ops = &max96705_internal_ops;
	dev->sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->sd.entity.function = MEDIA_ENT_F_VID_IF_BRIDGE;
	dev->sd.entity.flags = MEDIA_ENT_FL_DEFAULT;

	if (ret >= 0) {
		/* init pads */
		for (idx = 0U; idx < MAX96705_PAD_NUM; idx++) {
			dev->pads[idx].index = idx;
			dev->pads[idx].flags = max96705_pad_flag[idx];
			dev->fmt = max96705_mbus_frmfmt_default;
		}
		media_entity_pads_init(&dev->sd.entity, MAX96705_PAD_NUM,
				       dev->pads);

		/* init notifier */
		v4l2_async_notifier_init(&dev->nf);
		dev->nf.ops = &max96705_nf_ops;
	}

	/* add async subdevs */
	if (ret >= 0) {
		ret = max96705_add_asd(dev);
		if (ret < 0) {
			loge("max96801_add_asd returned %d\n", ret);
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

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &max96705_regmap);
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

int max96705_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct max96705		*dev	= to_dev(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver max96705_driver = {
	.probe		= max96705_probe,
	.remove		= max96705_remove,
	.driver		= {
		.name		= "max96705",
		.of_match_table	= of_match_ptr(max96705_of_match),
	},
	.id_table	= max96705_id,
};

module_i2c_driver(max96705_driver);
MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips MAX96705 Driver");
MODULE_LICENSE("GPL");
