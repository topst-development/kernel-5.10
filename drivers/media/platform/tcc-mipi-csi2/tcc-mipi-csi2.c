// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mediabus.h>
#include <linux/of_graph.h>
#include <linux/version.h>

#include "tcc-mipi-csi2.h"
#include "tcc-mipi-csi2-helper.h"
#ifdef CONFIG_ARCH_TCC803X
#include "803x/tcc-mipi-csi2-ckc.h"
#include "csi2_s/v1.2/tcc-mipi-csi2-csis.h"
#endif
#ifdef CONFIG_ARCH_TCC805X
#include "805x/tcc-mipi-csi2-ckc.h"
#include "csi2_s/v1.2/tcc-mipi-csi2-csis.h"
#endif
#ifdef CONFIG_ARCH_TCC807X
#include "807x/tcc-mipi-csi2-ckc.h"
#include "csi2_s/v1.2/tcc-mipi-csi2-csis.h"
#endif
#ifdef CONFIG_ARCH_TCC750X
#include "750x/tcc-mipi-csi2-ckc.h"
#include "csi2_s/v2.0/tcc-mipi-csi2-csis.h"
#endif

#define DEFAULT_WIDTH			(1U)
#define DEFAULT_HEIGHT			(1U)

/*
 * Helper functions for reflection
 */
static inline struct tcc_mipi_csi2_state *sd_to_state(struct v4l2_subdev *sd)
{
	return v4l2_get_subdevdata(sd);
}

struct tcc_mipi_csi2_intr_src {
	const uint32_t mask;
	const char * const desc;
};

struct tcc_mipi_csi2_prop {
	const char * const name;
	uint32_t *out;
	bool is_def;
	uint32_t def;
};

static const struct v4l2_mbus_config mipi_csi2_mbus_config = {
	.type			= V4L2_MBUS_PARALLEL,
	/* de: high, vs: high, hs: high, pclk: high */
	.flags			=
		V4L2_MBUS_DATA_ACTIVE_HIGH	|
#if defined(CONFIG_ARCH_TCC803X)
		V4L2_MBUS_VSYNC_ACTIVE_HIGH	|
#else
		V4L2_MBUS_VSYNC_ACTIVE_LOW	|
#endif
		V4L2_MBUS_HSYNC_ACTIVE_HIGH	|
		V4L2_MBUS_PCLK_SAMPLE_RISING	|
		V4L2_MBUS_MASTER,
};

static const uint64_t tcc_mipi_csi2_pad_flag[TCC_MIPI_CSI2_PAD_NUM] = {
	MEDIA_PAD_FL_SINK,
	MEDIA_PAD_FL_SOURCE,
	MEDIA_PAD_FL_SOURCE,
	MEDIA_PAD_FL_SOURCE,
	MEDIA_PAD_FL_SOURCE,
};

static uint32_t tcc_mipi_csi2_codes[] = {
	MEDIA_BUS_FMT_UYVY8_2X8,
};

static const struct v4l2_mbus_framefmt
	tcc_mipi_csi2_mbus_frmfmt_default[TCC_MIPI_CSI2_PAD_NUM] = {
		/* SINK PAD */
		{
			.width = DEFAULT_WIDTH,
			.height = DEFAULT_HEIGHT,
			.code = MEDIA_BUS_FMT_UYVY8_2X8,
			.field = V4L2_FIELD_NONE,
		},
		/* SRC PAD */
		{
			.width = DEFAULT_WIDTH,
			.height = DEFAULT_HEIGHT,
			.code = MEDIA_BUS_FMT_UYVY8_2X8,
			.field = V4L2_FIELD_NONE,
		},
		/* SRC PAD */
		{
			.width = DEFAULT_WIDTH,
			.height = DEFAULT_HEIGHT,
			.code = MEDIA_BUS_FMT_UYVY8_2X8,
			.field = V4L2_FIELD_NONE,
		},
		/* SRC PAD */
		{
			.width = DEFAULT_WIDTH,
			.height = DEFAULT_HEIGHT,
			.code = MEDIA_BUS_FMT_UYVY8_2X8,
			.field = V4L2_FIELD_NONE,
		},
		/* SRC PAD */
		{
			.width = DEFAULT_WIDTH,
			.height = DEFAULT_HEIGHT,
			.code = MEDIA_BUS_FMT_UYVY8_2X8,
			.field = V4L2_FIELD_NONE,
		},
	};

static int tcc_mipi_csi2_nf_bound(struct v4l2_async_notifier *nf,
				  struct v4l2_subdev *sd,
				  struct v4l2_async_subdev *asd)
{
	struct tcc_mipi_csi2_state *state = NULL;
	int ret = 0;

	state = container_of(nf, struct tcc_mipi_csi2_state, nf);

	logi(&(state->pdev->dev), "v4l2-subdev %s is bounded\n", sd->name);

	return ret;
}

static void tcc_mipi_csi2_nf_unbind(struct v4l2_async_notifier *nf,
				    struct v4l2_subdev *sd,
				    struct v4l2_async_subdev *asd)
{
	struct tcc_mipi_csi2_state *state = NULL;

	state = container_of(nf, struct tcc_mipi_csi2_state, nf);

	logi(&(state->pdev->dev), "v4l2-subdev %s is unbounded\n", sd->name);
}

static const struct v4l2_async_notifier_operations tcc_mipi_csi2_nf_ops = {
	.bound = tcc_mipi_csi2_nf_bound,
	.unbind = tcc_mipi_csi2_nf_unbind,
};


static void tcc_mipi_csi2_init_format(struct tcc_mipi_csi2_state *state)
{
	uint32_t i = 0U;

	state->dv_timings.type = V4L2_DV_BT_656_1120;
	state->dv_timings.bt.width =  DEFAULT_WIDTH;
	state->dv_timings.bt.height = DEFAULT_HEIGHT;
	state->dv_timings.bt.interlaced = V4L2_DV_PROGRESSIVE;
	/* IMPORTANT
	 * The below field "polarities" is not used
	 * becasue polarities for vsync and hsync are supported only.
	 * So, use flags of "struct v4l2_mbus_config".
	 */
	state->dv_timings.bt.polarities = 0U;

	for (i = 0U; i < MAX_VC; i++) {
		state->isp_info[i].fmt.width = DEFAULT_WIDTH;
		state->isp_info[i].fmt.height = DEFAULT_HEIGHT;
		state->isp_info[i].fmt.code = MEDIA_BUS_FMT_YUYV8_1X16;
		state->isp_info[i].fmt.field = V4L2_FIELD_NONE;
		state->isp_info[i].fmt.colorspace = V4L2_COLORSPACE_SMPTE170M;
		state->isp_info[i].data_format =
			code_to_csi_dt(state->isp_info[i].fmt.code);
	}
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int tcc_mipi_csi2_init(struct v4l2_subdev *sd, uint32_t enable)
{
	struct tcc_mipi_csi2_state	*state	= sd_to_state(sd);
	int				ret	= 0;

	mutex_lock(&state->lock);

	mutex_unlock(&state->lock);

	return ret;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int tcc_mipi_csi2_get_src_sd(const struct tcc_mipi_csi2_state *state,
				    const struct media_pad *local_pad,
				    struct v4l2_subdev **src_sd)
{
	struct media_pad *src_pad;
	int ret = 0;

	src_pad = media_entity_remote_pad(local_pad);
	if (!src_pad) {
		/* error */
		loge(&(state->pdev->dev), "Failed to find remote source pad\n");
		ret = -ENOLINK;
	} else if (!is_media_entity_v4l2_subdev(src_pad->entity)) {
		/* error */
		loge(&(state->pdev->dev),
		     "Upstream entity is not a v4l2 subdev\n");
		ret = -ENODEV;
	} else {
		/* okay */
		*src_sd = media_entity_to_v4l2_subdev(src_pad->entity);
	}

	return ret;
}

static int tcc_mipi_csi2_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct tcc_mipi_csi2_state *state = sd_to_state(sd);
	struct v4l2_subdev *src_sd;
	int ret = 0;

	mutex_lock(&state->lock);

	ret = tcc_mipi_csi2_get_src_sd(
		state, &sd->entity.pads[TCC_MIPI_CSI2_PAD_SINK], &src_sd);
	if (ret < 0) {
		/* error */
		loge(&state->pdev->dev, "tcc_isp_get_src_sd returned %d\n",
		     ret);
	} else {
		/* okay */
		logi(&(state->pdev->dev), "subdev call(%s - %s)\n",
		     src_sd->name, "s_stream");

		if ((state->use_cnt == 0U) && (enable == 1U)) {
			/* enable */
			tcc_mipi_csi2_enable(state, enable);
			ret = v4l2_subdev_call(src_sd, video, s_stream,
					       enable);
			if (ret < 0) {
				/* error */
				loge(&(state->pdev->dev),
				     "subdev_call(%s - %s %s) returned %d\n",
				     src_sd->name, "s_stream",
				     enable ? "enable" : "disabled", ret);
			} else {
				/* okay */
			}
		} else if ((state->use_cnt == 1U) && (enable == 0U)) {
			/* disabled */
			tcc_mipi_csi2_enable(state, enable);

			ret = v4l2_subdev_call(src_sd, video, s_stream,
					       enable);
			if (ret < 0) {
				/* error */
				loge(&(state->pdev->dev),
				     "subdev_call(%s - %s %s) returned %d\n",
				     src_sd->name, "s_stream",
				     enable ? "enable" : "disabled", ret);
			} else {
				/* okay */
			}
		}

		if (enable == 1U) {
			/* increase use cnt */
			state->use_cnt++;
		} else {
			/* decrease use cnt */
			state->use_cnt--;
		}

		logi(&(state->pdev->dev), "use_cnt is %d\n", state->use_cnt);
	}

	mutex_unlock(&state->lock);

	return ret;
}

static int tcc_mipi_csi2_g_dv_timings(struct v4l2_subdev *sd,
				      struct v4l2_dv_timings *timings)
{
	struct tcc_mipi_csi2_state	*state	= sd_to_state(sd);
	int				ret	= 0;

	mutex_lock(&state->lock);

	*timings = state->dv_timings;

	mutex_unlock(&state->lock);

	return ret;
}

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
static int tcc_mipi_csi2_g_mbus_config(struct v4l2_subdev *sd,
				       unsigned int pad,
				       struct v4l2_mbus_config *cfg)
{
	struct tcc_mipi_csi2_state	*state	= sd_to_state(sd);
	int				ret	= 0;

	mutex_lock(&state->lock);

	*cfg = mipi_csi2_mbus_config;

	mutex_unlock(&state->lock);

	return ret;
}
#else
static int tcc_mipi_csi2_g_mbus_config(struct v4l2_subdev *sd,
				       struct v4l2_mbus_config *cfg)
{
	struct tcc_mipi_csi2_state	*state	= sd_to_state(sd);
	int				ret	= 0;

	mutex_lock(&state->lock);

	*cfg = mipi_csi2_mbus_config;

	mutex_unlock(&state->lock);

	return ret;
}
#endif

/*
 * v4l2_subdev_pad_ops implementations
 */
static int32_t tcc_mipi_csi2_init_cfg(struct v4l2_subdev *sd,
				      struct v4l2_subdev_pad_config *cfg)
{
	const struct tcc_mipi_csi2_state *state = sd_to_state(sd);
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
			loge(&state->pdev->dev, "get_fmt returned %d\n", ret);
			break;
		}

		try = v4l2_subdev_get_try_format(sd, cfg, pad);
		*try = fmt.format;
	}

	return ret;
}

static int tcc_mipi_csi2_enum_mbus_code(struct v4l2_subdev *sd,
					struct v4l2_subdev_pad_config *cfg,
					struct v4l2_subdev_mbus_code_enum *code)
{
	struct tcc_mipi_csi2_state	*state	= sd_to_state(sd);
	int				ret	= 0;

	mutex_lock(&state->lock);

	if ((code->pad != 0U) ||
	    (ARRAY_SIZE(tcc_mipi_csi2_codes) <= code->index)) {
		/* pad is null or index is wrong */
		ret = -EINVAL;
	} else {
		/* get code */
		code->code = tcc_mipi_csi2_codes[code->index];
	}

	mutex_unlock(&state->lock);

	return ret;
}

static int tcc_mipi_csi2_get_fmt(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_format *f)
{
	struct tcc_mipi_csi2_state *state = sd_to_state(sd);
	int ret	= 0;

	mutex_lock(&state->lock);

	if (f->pad >= TCC_MIPI_CSI2_PAD_NUM) {
		/* error */
		loge(&state->pdev->dev, "invalid pad num(%d)\n", f->pad);
		ret = -EINVAL;
	} else {
		if (f->which == V4L2_SUBDEV_FORMAT_TRY) {
			/* get try format */
			f->format =
				*v4l2_subdev_get_try_format(sd, cfg, f->pad);
		} else {
			/* get active format */
			if (f->pad == TCC_MIPI_CSI2_PAD_SINK) {
				/* sink pad */
				f->format = state->fmt[f->pad];
			} else {
				/* source pad */
				f->format = state->isp_info[f->pad - TCC_MIPI_CSI2_PAD_NUM_SINK].fmt;
			}
		}
	}

	mutex_unlock(&state->lock);

	return ret;
}

static int tcc_mipi_csi2_set_fmt(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_format *f)
{
	struct tcc_mipi_csi2_state *state = sd_to_state(sd);
	struct v4l2_mbus_framefmt *fmt = NULL;
	uint32_t i = 0U;
	int ret	= 0;

	mutex_lock(&state->lock);

	/* check pad */
	if (f->pad >= TCC_MIPI_CSI2_PAD_NUM) {
		/* error */
		loge(&state->pdev->dev, "invalid pad num(%d)\n", f->pad);
		ret = -EINVAL;
	}

	/* get try or active mbus framefmt pointer */
	if (ret >= 0) {
		if (f->which == V4L2_SUBDEV_FORMAT_TRY) {
			/* get try format */
			fmt = v4l2_subdev_get_try_format(sd, cfg, f->pad);
		} else {
			/* get active format */
			if (f->pad == TCC_MIPI_CSI2_PAD_SINK) {
				/* sink pad */
				fmt = &state->fmt[f->pad];
			} else {
				/* source pad */
				fmt = &state->isp_info[f->pad - TCC_MIPI_CSI2_PAD_NUM_SINK].fmt;
			}
		}
	}

	*fmt = f->format;

	/* set_fmt of remote subdev */
	for (i = 0U; i < MAX_VC; i++) {
		state->isp_info[i].fmt = f->format;
		state->isp_info[i].data_format =
			code_to_csi_dt(state->isp_info[i].fmt.code);
	}

	state->dv_timings.bt.width = f->format.width;
	state->dv_timings.bt.height = f->format.height;

	mutex_unlock(&state->lock);

	return ret;
}

/*
 * v4l2_subdev_internal_ops implementations
 */
static int tcc_mipi_csi2_registered(struct v4l2_subdev *sd)
{
	struct tcc_mipi_csi2_state *state = sd_to_state(sd);
	int ret = 0;

	logd(&state->pdev->dev, "registered with v4l2 dev\n");

	return ret;
}

/*
 * v4l2_subdev_ops implementations
 */
static const struct v4l2_subdev_core_ops tcc_mipi_csi2_core_ops = {
	.init			= tcc_mipi_csi2_init,
};

static const struct v4l2_subdev_video_ops tcc_mipi_csi2_video_ops = {
	.s_stream		= tcc_mipi_csi2_s_stream,
	.g_dv_timings		= tcc_mipi_csi2_g_dv_timings,
#if KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE
	.g_mbus_config		= tcc_mipi_csi2_g_mbus_config,
#endif
};

static const struct v4l2_subdev_pad_ops tcc_mipi_csi2_pad_ops = {
	.init_cfg		= tcc_mipi_csi2_init_cfg,
	.enum_mbus_code		= tcc_mipi_csi2_enum_mbus_code,
	.get_fmt		= tcc_mipi_csi2_get_fmt,
	.set_fmt		= tcc_mipi_csi2_set_fmt,
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
	.get_mbus_config	= tcc_mipi_csi2_g_mbus_config,
#endif
};

static const struct v4l2_subdev_ops tcc_mipi_csi2_ops = {
	.core			= &tcc_mipi_csi2_core_ops,
	.video			= &tcc_mipi_csi2_video_ops,
	.pad			= &tcc_mipi_csi2_pad_ops,
};

static const struct v4l2_subdev_internal_ops tcc_mipi_csi2_internal_ops = {
	.registered = tcc_mipi_csi2_registered,
};

static const struct of_device_id tcc_mipi_csi2_of_match[];

static int tcc_mipi_csi2_add_asd(struct tcc_mipi_csi2_state *state)
{
	int ret = 0;

	/* add upstream device to the async subdev of notifier */
	ret = v4l2_async_notifier_parse_fwnode_endpoints_by_port(
		&(state->pdev->dev), &state->nf,
		sizeof(struct v4l2_async_subdev), 0U, NULL);
	if (ret < 0) {
		loge(&(state->pdev->dev),
		     "v4l2_async_notifier_parse_fwnode_endpoints_by_port returned %d\n",
		     ret);
	} else {
		struct v4l2_async_subdev *found_asd;

		list_for_each_entry(found_asd, &state->nf.asd_list, asd_list) {
			logi(&state->pdev->dev, "asd %s has been found\n",
			     to_of_node(found_asd->match.fwnode)->name);
		}
	}

	return ret;
}

static int32_t tcc_mipi_csi2_register_asd_nf(struct tcc_mipi_csi2_state *state)
{
	int32_t ret = 0;

	/* register a notifier */
	ret = v4l2_async_subdev_notifier_register(&state->sd, &state->nf);
	if (ret < 0) {
		loge(&(state->pdev->dev),
		     "v4l2_async_subdev_notifier_register, ret: %d\n", ret);
		v4l2_async_notifier_cleanup(&state->nf);
	}

	return ret;
}

static int tcc_mipi_csi2_init_sd(struct tcc_mipi_csi2_state *state)
{
	uint32_t idx = 0U;
	int ret = 0;

	/* init subdev */
	v4l2_subdev_init(&state->sd, &tcc_mipi_csi2_ops);
	state->sd.internal_ops = &tcc_mipi_csi2_internal_ops;
	state->sd.owner = state->pdev->dev.driver->owner;
	state->sd.dev = &state->pdev->dev;
	state->sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	state->sd.entity.function = MEDIA_ENT_F_VID_IF_BRIDGE;
	state->sd.entity.flags = MEDIA_ENT_FL_DEFAULT;
	v4l2_set_subdevdata(&state->sd, state);
	/* initialize name */
	ret = scnprintf(state->sd.name, sizeof(state->sd.name), "%s",
			dev_of_node(&state->pdev->dev)->name);
	if (ret == 0) {
		/* error */
		loge(&(state->pdev->dev), "scnprintf returned %d\n", ret);
		ret = -EINVAL;
	}

	if (ret >= 0) {
		struct v4l2_mbus_framefmt *f;

		/* init pads */
		for (idx = 0U; idx < TCC_MIPI_CSI2_PAD_NUM; idx++) {
			state->pads[idx].index = idx;
			state->pads[idx].flags = tcc_mipi_csi2_pad_flag[idx];
			if (idx == TCC_MIPI_CSI2_PAD_SINK) {
				/* sink pad */
				f = &state->fmt[idx];
			} else {
				/* source pad */
				f = &state->isp_info[idx - TCC_MIPI_CSI2_PAD_NUM_SINK].fmt;
			}
			*f = tcc_mipi_csi2_mbus_frmfmt_default[idx];
		}
		media_entity_pads_init(&state->sd.entity, TCC_MIPI_CSI2_PAD_NUM,
				       state->pads);

		/* init notifier */
		v4l2_async_notifier_init(&state->nf);
		state->nf.ops = &tcc_mipi_csi2_nf_ops;
	}

	/* add async subdevs */
	if (ret >= 0) {
		ret = tcc_mipi_csi2_add_asd(state);
		if (ret < 0) {
			loge(&(state->pdev->dev),
			     "tcc_mipi_csi2_add_asd returned %d\n", ret);
		}
	}

	/* register async notifier */
	if (ret >= 0) {
		ret = tcc_mipi_csi2_register_asd_nf(state);
		if (ret < 0) {
			loge(&(state->pdev->dev),
			     "tcc_mipi_csi2_register_asd_nf returned %d\n",
			     ret);
		}
	}

	/* register a v4l2 sub device */
	if (ret >= 0) {
		ret = v4l2_async_register_subdev(&(state->sd));
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
				"v4l2_async_register_subdev returned %d\n",
				ret);
		}
	}

	return ret;
}

static int tcc_mipi_csi2_init_state(struct tcc_mipi_csi2_state *state,
				    struct platform_device *pdev)
{
	const struct of_device_id *of_id = NULL;
	int ret = 0;

	mutex_init(&state->lock);

	state->pdev = pdev;
	platform_set_drvdata(pdev, state);

	tcc_mipi_csi2_init_format(state);

	/* parse device tree */
	ret = tcc_mipi_csi2_parse_dt(state);
	if (ret < 0) {
		/* error */
		loge(&(state->pdev->dev),
				"tcc_mipi_csi2_parse_dt returned %d\n",
				ret);
	}

	if (ret == 0) {
		/* set the specific information */
		of_id = of_match_node(tcc_mipi_csi2_of_match,
				state->pdev->dev.of_node);
		if (of_id == NULL) {
			/* error */
			loge(&(state->pdev->dev),
				"of_match_node returned NULL\n");
			ret = -EINVAL;
		}
	}

	if (ret == 0) {
		/* initialize a subdev */
		ret = tcc_mipi_csi2_init_sd(state);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
				"tcc_mipi_csi2_init_sd returned %d\n",
				ret);
		}
	}

	return ret;
}

static int tcc_mipi_csi2_probe(struct platform_device *pdev)
{
	struct tcc_mipi_csi2_state *state = NULL;
	static const char * const INTERLEAVE_MODE[] = {
		"CH0 only, no data interleave",
		"DT only",
		"VC only",
		"VC and DT",
	};
	static const char * const PIXEL_MODE[] = {
		"Single pixel mode",
		"Dual pixel mode",
		"Quad pixel mode"
	};
	uint32_t idx = 0;
	int ret = 0;

	/* allocate and clear memory for a device */
	state = devm_kzalloc(&pdev->dev, sizeof(*state), GFP_KERNEL);
	if (state == NULL) {
		/* error */
		loge(&(pdev->dev),
				"devm_kzalloc returned NULL\n");
		ret = -ENOMEM;
	}

	/* initialize a state */
	if (ret == 0) {
		ret = tcc_mipi_csi2_init_state(state, pdev);
		if (ret < 0) {
			/* error */
			loge(&(pdev->dev),
				"tcc_mipi_csi2_init_state returned %d\n",
				ret);
		}
	}

	if (ret == 0) {
		/* get clock */
		ret = tcc_mipi_csi2_ckc_enable(state);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
				"tcc_mipi_csi2_ckc_enable returned %d\n",
				ret);
		}
	}

	/* request irq */
	if (ret == 0) {
		ret = devm_request_irq(&pdev->dev,
				state->irq, tcc_mipi_csi2_isr,
				0U, dev_name(&pdev->dev), state);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
					"devm_request_irq returned %d",
					ret);
		}
	}

	if (ret == 0 && (state->gdb_irq != 0U)) {
		ret = devm_request_irq(&pdev->dev,
				state->gdb_irq, tcc_mipi_csi2_gdb_isr,
				0U, dev_name(&pdev->dev), state);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
					"devm_request_irq returned %d",
					ret);
		}
	}

	if (ret == 0) {
		logi(&(state->pdev->dev),
			"Success probe MIPI-CSI2-%d\n",
			pdev->id);
		logi(&(state->pdev->dev),
			"    update-shadow-ctrl(%d)\n",
			state->update_shadow_ctrl);
#if !defined(CONFIG_ARCH_TCC803X) && !defined(CONFIG_ARCH_TCC750X)
		logi(&(state->pdev->dev),
			"    mipi_chmux(%d %d %d %d %d %d %d %d)\n",
			state->mipi_chmux[0], state->mipi_chmux[1],
			state->mipi_chmux[2], state->mipi_chmux[3],
			state->mipi_chmux[4], state->mipi_chmux[5],
			state->mipi_chmux[6], state->mipi_chmux[7]);
#endif
#if !defined(CONFIG_ARCH_TCC803X)
		logi(&(state->pdev->dev),
			"    isp-bypass(%d %d %d %d)\n",
			state->isp_bypass[0], state->isp_bypass[1],
			state->isp_bypass[2], state->isp_bypass[3]);
#endif
		logi(&(state->pdev->dev),
			"    deskew-level(%d) deskew-enable(%d)\n",
			state->deskew_level, state->deskew_enable);
		logi(&(state->pdev->dev),
			"    hssettle(0x%x) s-clksettlectl(%d)\n",
			state->hssettle, state->s_clksettlectl);
		logi(&(state->pdev->dev),
			"    data DPDN swap(%d), clk DPDN swap(%d)\n",
			state->s_dpdn_swap_clk,
			state->s_dpdn_swap_dat);
		logi(&(state->pdev->dev),
			"    num-channel(%d)\n",
			state->input_ch_num);
		logi(&(state->pdev->dev),
			"    interleave-mode(%s)\n",
			INTERLEAVE_MODE[state->interleave_mode]);
		for (idx = 0; idx < MAX_VC; idx++) {
			logi(&(state->pdev->dev),
				"    CH%d: VC(%d), Pixel mode(%s)\n",
				idx,
				state->isp_info[idx].virtual_channel,
				PIXEL_MODE[state->isp_info[idx].pixel_mode]);
		}
	} else {
		/* error */
		loge(&(pdev->dev), "Fail probe");
	}

	return ret;
}

static int tcc_mipi_csi2_remove(struct platform_device *pdev)
{
	struct tcc_mipi_csi2_state *state =
		(struct tcc_mipi_csi2_state *)platform_get_drvdata(pdev);
	int ret = 0;

	tcc_mipi_csi2_ckc_disable(state);

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int tcc_mipi_csi2_suspend(struct device *dev)
{
	const struct platform_device *pdev = to_platform_device(dev);
	struct tcc_mipi_csi2_state *state =
		(struct tcc_mipi_csi2_state *)platform_get_drvdata(pdev);
	int ret = 0;

	tcc_mipi_csi2_ckc_disable(state);

	return ret;
}

static int tcc_mipi_csi2_resume(struct device *dev)
{
	const struct platform_device *pdev = to_platform_device(dev);
	struct tcc_mipi_csi2_state *state =
		(struct tcc_mipi_csi2_state *)platform_get_drvdata(pdev);
	int ret = 0;

	ret = tcc_mipi_csi2_ckc_enable(state);
	if (ret < 0) {
		/* error */
		loge(&(state->pdev->dev),
				"tcc_mipi_csi2_ckc_enable returned %d\n",
				ret);
	}

	return ret;
}
#endif

static const struct of_device_id tcc_mipi_csi2_of_match[] = {
	{
		.compatible	= "telechips,tcc803x-mipi-csi2",
	},
	{
		.compatible	= "telechips,tcc805x-mipi-csi2",
	},
	{
		.compatible	= "telechips,tcc807x-mipi-csi2",
	},
	{
		.compatible	= "telechips,tcc750x-mipi-csi2",
	},
	{
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, tcc_mipi_csi2_of_match);

static const struct dev_pm_ops tcc_mipi_csi2_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tcc_mipi_csi2_suspend, tcc_mipi_csi2_resume)
};

static struct platform_driver tcc_mipi_csi2_driver = {
	.probe = tcc_mipi_csi2_probe,
	.remove = tcc_mipi_csi2_remove,
	.driver = {
		.name = TCC_MIPI_CSI2_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table	= tcc_mipi_csi2_of_match,
		.pm = &tcc_mipi_csi2_pm_ops,
	},
};
module_platform_driver(tcc_mipi_csi2_driver);

MODULE_AUTHOR("Telechips <www.telechips.com>");
MODULE_DESCRIPTION("Telechips TCCXXXX SoC MIPI-CSI2 receiver driver");
MODULE_LICENSE("GPL");
