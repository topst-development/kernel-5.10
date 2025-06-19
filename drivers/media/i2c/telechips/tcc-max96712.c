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
#include "tcc-max96712-reg.h"

#define LOG_TAG				"VSRC:MAX96712"

#define loge(fmt, ...)			\
	pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(fmt, ...)			\
	pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(fmt, ...)			\
	pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(fmt, ...)			\
	pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)

#define	NUM_CHANNELS			(4)

#define MAX96712_LINK_MODE		MAX96712_GMSL1_4CH

/*
 * TODO
 * The defines below must be modified according to your device
 * The default values are for ar0147(sensor) and max96701(serializer)
 */
/* 1. remote devie info - sensor(ar0147) */
#define AR0147_SLAVE_ADDR			(0x20)

#define MAX96712_SENSOR_SLAVE_ADDR		AR0147_SLAVE_ADDR
#define MAX96712_SENSOR_SLAVE_ADDR_ALIAS0	(AR0147_SLAVE_ADDR + 2)
#define MAX96712_SENSOR_SLAVE_ADDR_ALIAS1	(AR0147_SLAVE_ADDR + 4)
#define MAX96712_SENSOR_SLAVE_ADDR_ALIAS2	(AR0147_SLAVE_ADDR + 6)
#define MAX96712_SENSOR_SLAVE_ADDR_ALIAS3	(AR0147_SLAVE_ADDR + 8)
/* 2. remote devie info - serializer(max96701) */
#define MAX96701_SLAVE_ADDR			(0x80)
#define MAX96701_REG_I2C_SOURCE_B		(0x0B)
#define MAX96701_REG_I2C_DEST_B			(0x0C)

#define MAX96712_SER_SLAVE_ADDR			MAX96701_SLAVE_ADDR
#define MAX96712_SER_REG_I2C_SOURC		MAX96701_REG_I2C_SOURCE_B
#define MAX96712_SER_REG_I2C_DEST		MAX96701_REG_I2C_DEST_B

#define MAX96712_PAD_SINK0			(0U)
#define MAX96712_PAD_SINK1			(1U)
#define MAX96712_PAD_SINK2			(2U)
#define MAX96712_PAD_SINK3			(3U)
#define MAX96712_PAD_SRC			(4U)

#define MAX96712_PAD_SINK_NUM			(4U)
#define MAX96712_PAD_NUM			(5U)

struct max96712_prv_data {
	const char * const name;

	const struct reg_sequence *init;
	const uint32_t init_size;

	const struct reg_sequence *s_stream;
	const uint32_t s_stream_size;

	const uint16_t ser_addr;
	/* reg info of the ser */
	const uint8_t ser_reg_len;
	const uint8_t ser_reg_src_a[2U];
	const uint8_t ser_reg_dst_a[2U];
	/* data to be written to the ser */
	const uint8_t ser_data_len;
	const uint8_t cis_addr[2U];
	const uint8_t cis_alias_addr[4U][2U];
};

static const uint64_t max96712_pad_flag[MAX96712_PAD_NUM] = {
	MEDIA_PAD_FL_SINK,
	MEDIA_PAD_FL_SINK,
	MEDIA_PAD_FL_SINK,
	MEDIA_PAD_FL_SINK,
	MEDIA_PAD_FL_SOURCE,
};

static const struct v4l2_mbus_framefmt
	max96712_mbus_frmfmt_default[MAX96712_PAD_SINK_NUM] = {
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

static const struct v4l2_mbus_framefmt
	max96712_mbus_frmfmt_vtg[MAX96712_PAD_SINK_NUM] = {
		/* SRC0 PAD */
		{
			.width = 1280,
			.height = 720,
			.code = MEDIA_BUS_FMT_RGB888_1X24,
			.field = V4L2_FIELD_NONE,
		},
		/* SRC1 PAD */
		{
			.width = 1280,
			.height = 720,
			.code = MEDIA_BUS_FMT_RGB888_1X24,
			.field = V4L2_FIELD_NONE,
		},
		/* SRC2 PAD */
		{
			.width = 1280,
			.height = 720,
			.code = MEDIA_BUS_FMT_RGB888_1X24,
			.field = V4L2_FIELD_NONE,
		},
		/* SR3 PAD */
		{
			.width = 1280,
			.height = 720,
			.code = MEDIA_BUS_FMT_RGB888_1X24,
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
struct max96712 {
	struct i2c_client		*clinet;
	struct v4l2_subdev		sd;

	struct v4l2_async_subdev	asd;
	struct v4l2_async_notifier	nf;

	struct media_pad		pads[MAX96712_PAD_NUM];
	struct v4l2_mbus_framefmt	fmt[MAX96712_PAD_SINK_NUM];

	struct power_sequence		gpio;

	/* Regmaps */
	struct regmap			*regmap;

	struct mutex lock;
	unsigned int p_cnt;
	unsigned int s_cnt;
	unsigned int i_cnt;

	bool				broadcasting_mode;

	/* private data */
	const char *pvd_name;
	struct max96712_prv_data *pvd;
};

static struct max96712_prv_data max96712_prv_data_list[] = {
	{
		.name = "hd_ar0147",
		.init = max96712_reg_init_hd_ar0147,
		.init_size = ARRAY_SIZE(max96712_reg_init_hd_ar0147),
		.s_stream = max96712_reg_s_stream,
		.s_stream_size = ARRAY_SIZE(max96712_reg_s_stream),

		.ser_addr = 0x80U,
		.ser_reg_len = 1U, /* byte */
		.ser_reg_src_a = {0x0BU, },
		.ser_reg_dst_a = {0x0CU, },
		.ser_data_len = 1U, /* byte */
		.cis_addr = {0x20U, },
		.cis_alias_addr = {
			{0x22U, },
			{0x24U, },
			{0x26U, },
			{0x28U, },
		},
	},
	{
		.name = "fhd",
		.init = max96712_reg_init_fhd,
		.init_size = ARRAY_SIZE(max96712_reg_init_fhd),
		.s_stream = max96712_reg_s_stream,
		.s_stream_size = ARRAY_SIZE(max96712_reg_s_stream),
	},
	{
		.name = "test_vtg",
		.init = max96712_vtg_reg_init,
		.init_size = ARRAY_SIZE(max96712_vtg_reg_init),
		.s_stream = max96712_vtg_reg_s_stream,
		.s_stream_size = ARRAY_SIZE(max96712_vtg_reg_s_stream),
	},

	{
		.name = "fhd_ar0231",
		.init = max96712_reg_init_fhd_ar0231,
		.init_size = ARRAY_SIZE(max96712_reg_init_fhd_ar0231),
		.s_stream = max96712_reg_s_stream,
		.s_stream_size = ARRAY_SIZE(max96712_reg_s_stream),

		.ser_addr = 0x80U,
		.ser_reg_len = 1U, /* byte */
		.ser_reg_src_a = {0x0BU, },
		.ser_reg_dst_a = {0x0CU, },
		.ser_data_len = 1U, /* byte */
		.cis_addr = {0x20U, },
		.cis_alias_addr = {
			{0x22U, },
			{0x24U, },
			{0x26U, },
			{0x28U, },
		},
	},
	{
		.name = "qhd_imx424",
		.init = max96712_reg_init_qhd_imx424,
		.init_size = ARRAY_SIZE(max96712_reg_init_qhd_imx424),
		.s_stream = max96712_reg_s_stream,
		.s_stream_size = ARRAY_SIZE(max96712_reg_s_stream),
	},
	{
		.name = "qhd_ar0820",
		.init = max96712_reg_init_qhd_ar0820,
		.init_size = ARRAY_SIZE(max96712_reg_init_qhd_ar0820),
		.s_stream = max96712_reg_s_stream,
		.s_stream_size = ARRAY_SIZE(max96712_reg_s_stream),

		.ser_addr = 0x80U,
		.ser_reg_len = 2U, /* byte */
		.ser_reg_src_a = {0x00U, 0x42U},
		.ser_reg_dst_a = {0x00U, 0x43U},
		.ser_data_len = 1U, /* byte */
		.cis_addr = {0x20U, },
		.cis_alias_addr = {
			{0x22U, },
			{0x24U, },
		},
	},
	{
		.name = "4k_ar0820",
		.init = max96712_reg_init_4k_ar0820,
		.init_size = ARRAY_SIZE(max96712_reg_init_4k_ar0820),
		.s_stream = max96712_reg_s_stream,
		.s_stream_size = ARRAY_SIZE(max96712_reg_s_stream),

		.ser_addr = 0x80U,
		.ser_reg_len = 2U, /* byte */
		.ser_reg_src_a = {0x00U, 0x42U},
		.ser_reg_dst_a = {0x00U, 0x43U},
		.ser_data_len = 1U, /* byte */
		.cis_addr = {0x20U, },
		.cis_alias_addr = {
			{0x22U, },
			{0x24U, },
		},
	},
	{
		.name = "fhd_ar0239",
		.init = max96712_reg_init_fhd_ar0239,
		.init_size = ARRAY_SIZE(max96712_reg_init_fhd_ar0239),
		.s_stream = max96712_reg_s_stream,
		.s_stream_size = ARRAY_SIZE(max96712_reg_s_stream),
	},
	{
	},
};

static const struct regmap_config max96712_regmap = {
	.reg_bits		= 16,
	.val_bits		= 8,

	.max_register		= 0xFFFF,
	.cache_type		= REGCACHE_NONE,
};

static struct max96712_prv_data *
find_max96712_prv_data(const char *what)
{
	uint32_t idx = 0;

	while (max96712_prv_data_list[idx].name != NULL) {
		if (!strncmp(max96712_prv_data_list[idx].name, what,
			strlen(what))) {
			/* match */
			break;
		}

		/* didn't match */
		idx++;
	}

	if (max96712_prv_data_list[idx].name == NULL) {
		/* error */
		loge("can not find max96712_prv_data\n");
	} else {
		/* success */
		logi("found max96712_prv_data %s\n",
		     max96712_prv_data_list[idx].name);
	}

	return &max96712_prv_data_list[idx];
}

static int max96712_nf_bound(struct v4l2_async_notifier *nf,
			     struct v4l2_subdev *sd,
			     struct v4l2_async_subdev *asd)
{
	struct max96712 *dev;
	int ret = 0;

	dev = container_of(nf, struct max96712, nf);

	logi("v4l2-subdev %s is bounded\n", sd->name);

	return ret;
}

static void max96712_nf_unbind(struct v4l2_async_notifier *nf,
			       struct v4l2_subdev *sd,
			       struct v4l2_async_subdev *asd)
{
	struct max96712 *dev = NULL;

	dev = container_of(nf, struct max96712, nf);

	logi("v4l2-subdev %s is unbounded\n", sd->name);
}

static const struct v4l2_async_notifier_operations max96712_nf_ops = {
	.bound = max96712_nf_bound,
	.unbind = max96712_nf_unbind,
};

static int max96712_parse_device_tree(struct max96712 *dev,
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

	ret = of_property_read_string(node, "pvd-name",
				      &dev->pvd_name);
	if (ret < 0) {
		loge("FAIL - of_property_read_string returns %d\n",
		     ret);
	}

	return ret;
}

/*
 * gpio functions
 */
void max96712_request_gpio(struct max96712 *dev)
{
	if (dev->gpio.pwr_port > 0) {
		/* power */
		gpio_request(dev->gpio.pwr_port, "max96712 power");
	}
	if (dev->gpio.pwd_port > 0) {
		/* power-down */
		gpio_request(dev->gpio.pwd_port, "max96712 power down");
	}
	if (dev->gpio.rst_port > 0) {
		/* reset */
		gpio_request(dev->gpio.rst_port, "max96712 reset");
	}
	if (dev->gpio.intb_port > 0) {
		/* intb */
		gpio_request(dev->gpio.intb_port, "max96712 interrupt");
	}
}

void max96712_free_gpio(struct max96712 *dev)
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
static inline struct max96712 *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct max96712, sd);
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int max96712_init(struct v4l2_subdev *sd, u32 enable)
{
	struct max96712		*dev	= to_dev(sd);
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
	} else if ((dev->i_cnt == 1) && (enable == 0)) {
		/* ret = regmap_write(dev->regmap, 0x15, 0x93); */
	}

	if (enable)
		dev->i_cnt++;
	else
		dev->i_cnt--;

	mutex_unlock(&dev->lock);

	return ret;
}

static int max96712_set_power(struct v4l2_subdev *sd, int on)
{
	struct max96712		*dev	= to_dev(sd);
	struct power_sequence	*gpio	= &dev->gpio;

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
		}
		dev->p_cnt++;
	} else {
		if (dev->p_cnt == 1) {
			/* power-down sequence */
			if (dev->gpio.pwd_port > 0) {
				gpio_set_value_cansleep(gpio->pwd_port, 0);
				msleep(20);
			}
		}
		dev->p_cnt--;
	}

	mutex_unlock(&dev->lock);

	return 0;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int max96712_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	struct max96712		*dev	= to_dev(sd);
	unsigned int		val	= 0;
	unsigned int		link	= 0;
	int			ret	= 0;

	mutex_lock(&dev->lock);

	/* reset status */
	*status	= 0;

	ret = regmap_read(dev->regmap, 0x0006, &link);
	if (ret < 0) {
		loge("failure to check which link is enabled\n");
		goto end;
	}

	if (link & MAX96712_LINK_EN_A) {
		/* check V4L2_IN_ST_NO_SIGNAL */
		ret = regmap_read(dev->regmap, MAX96712_REG_STATUS_A, &val);
		if (ret < 0) {
			loge("failure to check MAX96712_REG_STATUS_A\n");
			*status =
				V4L2_IN_ST_NO_POWER |
				V4L2_IN_ST_NO_SIGNAL |
				V4L2_IN_ST_NO_COLOR;
			goto end;
		} else {
			logd("status a: 0x%08x\n", val);

			if ((val & MAX96712_VAL_STATUS) !=
				MAX96712_VAL_STATUS) {
				logw("STATUS_A is V4L2_IN_ST_NO_SIGNAL\n");
				*status |= V4L2_IN_ST_NO_SIGNAL;
				goto end;
			}
		}
	}

	if (link & MAX96712_LINK_EN_B) {
		/* check V4L2_IN_ST_NO_SIGNAL */
		ret = regmap_read(dev->regmap, MAX96712_REG_STATUS_B, &val);
		if (ret < 0) {
			loge("failure to check MAX96712_REG_STATUS_B\n");
			*status =
				V4L2_IN_ST_NO_POWER |
				V4L2_IN_ST_NO_SIGNAL |
				V4L2_IN_ST_NO_COLOR;
			goto end;
		} else {
			logd("status b: 0x%08x\n", val);

			if ((val & MAX96712_VAL_STATUS) !=
				MAX96712_VAL_STATUS) {
				logw("STATUS_B is V4L2_IN_ST_NO_SIGNAL\n");
				*status |= V4L2_IN_ST_NO_SIGNAL;
				goto end;
			}
		}
	}

	if (link & MAX96712_LINK_EN_C) {
		/* check V4L2_IN_ST_NO_SIGNAL */
		ret = regmap_read(dev->regmap, MAX96712_REG_STATUS_C, &val);
		if (ret < 0) {
			loge("failure to check MAX96712_REG_STATUS_C\n");
			*status =
				V4L2_IN_ST_NO_POWER |
				V4L2_IN_ST_NO_SIGNAL |
				V4L2_IN_ST_NO_COLOR;
			goto end;
		} else {
			logd("status c: 0x%08x\n", val);

			if ((val & MAX96712_VAL_STATUS) !=
				MAX96712_VAL_STATUS) {
				logw("STATUS_C is V4L2_IN_ST_NO_SIGNAL\n");
				*status |= V4L2_IN_ST_NO_SIGNAL;
				goto end;
			}
		}
	}

	if (link & MAX96712_LINK_EN_D) {
		/* check V4L2_IN_ST_NO_SIGNAL */
		ret = regmap_read(dev->regmap, MAX96712_REG_STATUS_D, &val);
		if (ret < 0) {
			loge("failure to check MAX96712_REG_STATUS_D\n");
			*status =
				V4L2_IN_ST_NO_POWER |
				V4L2_IN_ST_NO_SIGNAL |
				V4L2_IN_ST_NO_COLOR;
			goto end;
		} else {
			logd("status d: 0x%08x\n", val);

			if ((val & MAX96712_VAL_STATUS) !=
				MAX96712_VAL_STATUS) {
				logw("STATUS_D is V4L2_IN_ST_NO_SIGNAL\n");
				*status |= V4L2_IN_ST_NO_SIGNAL;
				goto end;
			}
		}
	}

end:
	mutex_unlock(&dev->lock);

	logi("status: 0x%08x\n", *status);
	return ret;
}

static inline int max96712_set_fwdcc(struct v4l2_subdev *sd,
				     unsigned int target_link, int enable)
{
	const uint32_t reg_fwdcc[] = {
		MAX96712_REG_GMSL1_A_FWDCCEN,
		MAX96712_REG_GMSL1_B_FWDCCEN,
		MAX96712_REG_GMSL1_C_FWDCCEN,
		MAX96712_REG_GMSL1_D_FWDCCEN,
	};
	struct max96712	*dev = to_dev(sd);
	unsigned int reg_val = 0;
	int ret = 0;

	if (enable) {
		/* Enable Forward Control Channel */
		reg_val = MAX96712_GMSL1_FWDCC_ENABLE;
	} else {
		/* Disable Forward Control Channel */
		reg_val = MAX96712_GMSL1_FWDCC_DISABLE;
	}

	ret = regmap_write(dev->regmap, reg_fwdcc[target_link], reg_val);
	//msleep(5);

	return ret;
}

static inline int max96712_set_all_fwdcc(struct v4l2_subdev *sd,
					 int enable)
{
	uint32_t idx = 0;
	int ret = 0;

	for (idx = 0; idx < 4; idx++) {
		ret = max96712_set_fwdcc(sd, idx, enable);
		if (ret < 0) {
			loge("Fail %s FWDCC of Link %d\n",
				((enable == 1) ? "enable" : "disable"),
				idx);
		}
	}

	return ret;
}

static int
max96712_set_ser_i2c_translator(struct v4l2_subdev *sd,
				const uint8_t *src, const uint8_t *dst)
{
	struct max96712 *dev = to_dev(sd);
	struct i2c_client *client = 0;
	uint32_t idx = 0U, total_len = 0U;
	unsigned char buf[4] = {0,};
	unsigned short backup_addr = 0;
	int ret = 0;

	client = v4l2_get_subdevdata(sd);
	if (client == NULL) {
		ret = -ENODEV;
		loge("no i2c client info\n");
	}

	/*
	 * Set alias.
	 * Src address is a alias.
	 * Dest address is a real slave address of remote device.
	 */
	/* set src */
	if (ret >= 0) {
		/* backup deserializer slave address */
		backup_addr = client->addr;
		client->addr = (dev->pvd->ser_addr >> 1U);

		for (idx = 0; idx < dev->pvd->ser_reg_len; idx++) {
			/* set reg addr data */
			buf[idx] = dev->pvd->ser_reg_src_a[idx];
			total_len++;
		}
		for (idx = 0; idx < dev->pvd->ser_data_len; idx++) {
			buf[idx + dev->pvd->ser_reg_len] = src[idx];
			total_len++;
		}

		ret = i2c_master_send(client, buf, total_len);
		if (ret < 0) {
			/* error */
			loge("Fail setting source address");
		}
	}

	/* set dst */
	if (ret >= 0) {
		total_len = 0U;

		/* set src */
		for (idx = 0; idx < dev->pvd->ser_reg_len; idx++) {
			/* set reg addr data */
			buf[idx] = dev->pvd->ser_reg_dst_a[idx];
			total_len++;
		}
		for (idx = 0; idx < dev->pvd->ser_data_len; idx++) {
			buf[idx + dev->pvd->ser_reg_len] = dst[idx];
			total_len++;
		}

		ret = i2c_master_send(client, buf, total_len);
		if (ret < 0) {
			/* error */
			loge("Fail setting destination address");
		}
	}

	if (client != NULL) {
		/* restore deserializer slave address */
		client->addr = backup_addr;
	}

	return ret;
}

/**
 * max96712_set_alias - Set alias of remote device's I2C slave address
 *
 * @sd: pointer to &struct v4l2_subdev
 * @tlink: target input link
 * @src: alias address
 * @dst: real I2C slave address
 *
 * I2C master -> (source addr) -> ... -> serializer -> (dest addr) -> device
 */
static int
max96712_set_alias(struct v4l2_subdev *sd,
		   const uint32_t link_mode, const uint32_t gmsl_ver,
		   const uint32_t tlink,
		   const uint8_t *src, const uint8_t *dst)
{
	struct max96712	*dev = to_dev(sd);
	int ret = 0;

	if (tlink > 3U) {
		ret = -EINVAL;
		loge("fail - invalid target link(%d)\n", tlink);
	}

	if (gmsl_ver == 1U) {
		/* disable all FWDCC */
		ret = max96712_set_all_fwdcc(sd, 0);
		if (ret < 0) {
			/* error */
			loge("Fail disable all FWDCC\n");
		}

		/* enable target FWDCC */
		ret = max96712_set_fwdcc(sd, tlink, 1);
		if (ret < 0) {
			/* error */
			loge("Fail enable target FWDCC\n");
		}
	} else {
		ret = regmap_write(dev->regmap, 0x0006,
				((link_mode & MAX96712_GMSL2_ABCD) |
				 (MAX96712_LINK_EN_A << tlink)));
		if (ret < 0) {
			/* error */
			loge("Fail enable target FWDCC\n");
		}
		msleep(20);
	}

	if (ret >= 0) {
		/* change remote device's slave address */
		ret = max96712_set_ser_i2c_translator(sd, src, dst);
		if (ret < 0) {
			/* error */
			loge("Fail max96712_set_ser_i2c_translator(%d)\n",
			     ret);
		}
	}

	if (gmsl_ver == 1U) {
		/* disable target FWDCC */
		ret = max96712_set_fwdcc(sd, tlink, 0);
		if (ret < 0) {
			/* error */
			loge("Fail enable target FWDCC\n");
		}

		/* enable all FWDCC */
		ret = max96712_set_all_fwdcc(sd, 1);
		if (ret < 0) {
			/* error */
			loge("Fail enable all FWDCC\n");
		}
	} else {
		ret = regmap_write(dev->regmap, 0x0006, link_mode);
		if (ret < 0) {
			/* error */
			loge("Fail enable target FWDCC\n");
		}
		msleep(20);
	}

	return ret;
}
/**
 * max96712_set_alias_remote_slave_addr - set alias of remote slave address
 *
 * @sd: pointer to &struct v4l2_subdev
 *
 * MAX96712 has 4 input ports.
 * So 4 remote devices(serializer, sensor and etc...) can be connected.
 *
 * If each remote devices are the same, all the I2C salve address will be same.
 * So, the serializer supports the feature translating input I2C slave address.
 */
static int max96712_set_alias_remote_slave_addr(struct v4l2_subdev *sd)
{
	const uint32_t reg_link_sts[] = {
		MAX96712_LINK_EN_A,
		MAX96712_LINK_EN_B,
		MAX96712_LINK_EN_C,
		MAX96712_LINK_EN_D,
	};
	const uint32_t link_ver[] = {
		MAX96712_GMSL2_A,
		MAX96712_GMSL2_B,
		MAX96712_GMSL2_C,
		MAX96712_GMSL2_D,
	};
	struct max96712 *dev = to_dev(sd);
	unsigned int link_sts = 0, idx = 0;
	int ret = 0;

	ret = regmap_read(dev->regmap, 0x0006, &link_sts);
	if (ret < 0) {
		loge("failure to check which link is enabled\n");
		goto end;
	}

	for (idx = 0; idx < 4; idx++) {
		if (link_sts & reg_link_sts[idx]) {
			ret = max96712_set_alias(sd,
					link_sts,
					(link_sts & link_ver[idx]) ? 2U : 1U,
					idx,
					dev->pvd->cis_alias_addr[idx],
					dev->pvd->cis_addr);
			if (ret < 0) {
				/* failure of changing remote device address */
				loge("Fail set alias of link %d device\n", idx);
				goto end;
			}
		}
	}

end:
	return ret;
}

static int max96712_src_sd_s_stream(struct v4l2_subdev *sd, int enable)
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

static int max96712_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct max96712 *dev = to_dev(sd);
	unsigned int reg_val = 0;
	int ret = 0;

	mutex_lock(&dev->lock);

	if ((dev->s_cnt == 0) && (enable == 1)) {
		ret = max96712_src_sd_s_stream(sd, enable);
		if (ret < 0) {
			/* error */
			loge("max96712_src_sd_s_stream returned %d\n",
			     ret);
		}

		ret = regmap_multi_reg_write(dev->regmap,
					     dev->pvd->s_stream,
					     dev->pvd->s_stream_size);
		if (ret < 0) {
			/* failure of enabling output  */
			loge("Fail enable output of max96712 device\n");
		}

		if (dev->pvd->cis_addr[0U] != 0U) {
			ret = max96712_set_alias_remote_slave_addr(sd);
			if (ret < 0) {
				/* failure of changing remote device address */
				loge("Fail set alias of remote address\n");
			}
		}

		ret = regmap_read(dev->regmap, 0x040B, &reg_val);
		if (ret < 0) {
			/* failure of enabling output  */
			loge("Fail read 0x040B\n");
		} else {
			/* output enable */
			reg_val |= 0x02;
		}

		ret = regmap_write(dev->regmap, 0x040B, reg_val);
		if (ret < 0) {
			/* failure of enabling output  */
			loge("Fail enable output of max96712 device\n");
		}
	} else if ((dev->s_cnt == 1) && (enable == 0)) {
		ret = max96712_src_sd_s_stream(sd, enable);
		if (ret < 0) {
			/* error */
			loge("max96712_src_sd_s_stream returned %d\n",
			     ret);
		}

		ret = regmap_read(dev->regmap, 0x040B, &reg_val);
		if (ret < 0) {
			/* failure of enabling output  */
			loge("Fail read 0x040B\n");
		} else {
			/* output disable */
			reg_val &= ~(0x02);
		}

		ret = regmap_write(dev->regmap, 0x040B, reg_val);
		if (ret < 0) {
			/* failure of disabling output  */
			loge("Fail disable output of max96712 device\n");
		}
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
static int32_t max96712_init_cfg(struct v4l2_subdev *sd,
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

static int max96712_get_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_pad_config *cfg,
			    struct v4l2_subdev_format *f)
{
	struct max96712 *dev = to_dev(sd);
	int ret = 0;

	mutex_lock(&dev->lock);

	if (f->pad >= MAX96712_PAD_NUM) {
		/* error */
		loge("invalid pad num(%d)\n", f->pad);
		ret = -EINVAL;
	} else {
		if (f->which == V4L2_SUBDEV_FORMAT_TRY) {
			/* get try format */
			f->format =
				*v4l2_subdev_get_try_format(sd, cfg, f->pad);
		} else {
			if (f->pad == MAX96712_PAD_SRC) {
				/* TODO:
				 * if each camera is different format,
				 * which format should be the format
				 * of source pad?
				 */
				f->format = dev->fmt[MAX96712_PAD_SINK0];
			} else {
				/* get active format */
				f->format = dev->fmt[f->pad];
			}
		}
	}

	mutex_unlock(&dev->lock);

	return ret;
}

static int max96712_set_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_pad_config *cfg,
			    struct v4l2_subdev_format *f)
{
	struct max96712 *dev = to_dev(sd);
	struct v4l2_mbus_framefmt *fmt = NULL;
	int ret = 0;

	mutex_lock(&dev->lock);

	/* check pad */
	if (f->pad >= MAX96712_PAD_SINK_NUM) {
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
static int max96712_registered(struct v4l2_subdev *sd)
{
	int ret = 0;

	logd("registered with v4l2 dev\n");

	return ret;
}

/*
 * v4l2_subdev_ops implementations
 */
static const struct v4l2_subdev_core_ops max96712_core_ops = {
	.init			= max96712_init,
	.s_power		= max96712_set_power,
};

static const struct v4l2_subdev_video_ops max96712_video_ops = {
	.g_input_status		= max96712_g_input_status,
	.s_stream		= max96712_s_stream,
};

static const struct v4l2_subdev_pad_ops max96712_pad_ops = {
	.init_cfg		= max96712_init_cfg,
	.get_fmt		= max96712_get_fmt,
	.set_fmt		= max96712_set_fmt,
};

static const struct v4l2_subdev_ops max96712_ops = {
	.core			= &max96712_core_ops,
	.video			= &max96712_video_ops,
	.pad			= &max96712_pad_ops,
};

static const struct v4l2_subdev_internal_ops max96712_internal_ops = {
	.registered = max96712_registered,
};

struct max96712 max96712_data = {
};

static const struct i2c_device_id max96712_id[] = {
	{ "max96712", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max96712_id);

#if IS_ENABLED(CONFIG_OF)
const struct of_device_id max96712_of_match[] = {
	{
		.compatible	= "tcc-maxim,max96712",
		.data		= &max96712_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, max96712_of_match);
#endif

static int max96712_add_asd(struct max96712 *dev)
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

int max96712_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct max96712			*dev	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	uint32_t			idx	= 0U;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct max96712), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(max96712_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	mutex_init(&dev->lock);

	/* parse device tree */
	ret = max96712_parse_device_tree(dev, client->dev.of_node);
	if (ret < 0) {
		loge("max96712_parse_device_tree, ret: %d\n", ret);
		return ret;
	}

	dev->pvd = find_max96712_prv_data(dev->pvd_name);
	if (dev->pvd == NULL) {
		/* error */
		loge("Fail - pvd_data(%s)\n", dev->pvd_name);
		goto goto_free_device_data;
	}

	/* Register with V4L2 layer as a slave device */
	v4l2_i2c_subdev_init(&dev->sd, client, &max96712_ops);
	dev->sd.internal_ops = &max96712_internal_ops;
	dev->sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->sd.entity.function = MEDIA_ENT_F_VID_IF_BRIDGE;
	dev->sd.entity.flags = MEDIA_ENT_FL_DEFAULT;

	if (ret >= 0) {
		/* init pads */
		if(strcmp(dev->pvd_name, "test_vtg") == 0) {
			dev->pads[0].index = 0;
			dev->pads[0].flags = MEDIA_PAD_FL_SOURCE;
			dev->fmt[0] = max96712_mbus_frmfmt_vtg[0];
			media_entity_pads_init(&dev->sd.entity, 1, dev->pads);
		} else {
			for (idx = 0U; idx < MAX96712_PAD_NUM; idx++) {
				dev->pads[idx].index = idx;
				dev->pads[idx].flags = max96712_pad_flag[idx];

				if (idx < MAX96712_PAD_SINK_NUM) {
					dev->fmt[idx] =
						max96712_mbus_frmfmt_default[idx];
				}
			}
			media_entity_pads_init(&dev->sd.entity, MAX96712_PAD_NUM,
					       dev->pads);
		}

		/* init notifier */
		v4l2_async_notifier_init(&dev->nf);
		dev->nf.ops = &max96712_nf_ops;
	}

	/* add async subdevs */
	if (ret >= 0) {
		ret = max96712_add_asd(dev);
		if (ret < 0) {
			loge("max96712_add_asd returned %d\n", ret);
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
	max96712_request_gpio(dev);

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &max96712_regmap);
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

int max96712_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct max96712		*dev	= to_dev(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	/* gree gpio */
	max96712_free_gpio(dev);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver max96712_driver = {
	.probe		= max96712_probe,
	.remove		= max96712_remove,
	.driver		= {
		.name		= "max96712",
		.of_match_table	= of_match_ptr(max96712_of_match),
	},
	.id_table	= max96712_id,
};

module_i2c_driver(max96712_driver);
MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips MAX96712 Driver");
MODULE_LICENSE("GPL");
