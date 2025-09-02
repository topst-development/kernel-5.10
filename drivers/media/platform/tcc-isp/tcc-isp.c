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
#include <linux/pinctrl/consumer.h>
#include <linux/firmware.h>
#include <linux/of_graph.h>
#include <linux/version.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>
#include "tcc-isp.h"
#include "tcc-isp-helper.h"
#ifdef CONFIG_ARCH_TCC805X
#include "func/805x/tcc-isp-reg.h"
#include "func/805x/tcc-isp-core.h"
#include "func/805x/tcc-isp-mcu.h"
#endif
#if defined(CONFIG_ARCH_TCC807X)
#include "func/807x/tcc-isp-reg.h"
#include "func/807x/tcc-isp-core.h"
#include "func/807x/tcc-isp-mcu.h"
#endif
#if defined(CONFIG_ARCH_TCC750X)
#include "func/750x/tcc-isp-reg.h"
#include "func/750x/tcc-isp-core.h"
#include "func/750x/tcc-isp-mcu.h"
#endif
#include "func/tcc-isp-common.h"

static const struct v4l2_mbus_config isp_mbus_config = {
	.type			= V4L2_MBUS_PARALLEL,
	/* de: high, vs: high, hs: high, pclk: high */
	.flags			=
		V4L2_MBUS_DATA_ACTIVE_HIGH	|
		V4L2_MBUS_VSYNC_ACTIVE_LOW	|
		V4L2_MBUS_HSYNC_ACTIVE_HIGH	|
		V4L2_MBUS_PCLK_SAMPLE_RISING	|
		V4L2_MBUS_MASTER,
};

static const uint64_t tcc_isp_pad_flag[TCC_ISP_PAD_NUM] = {
	MEDIA_PAD_FL_SINK,
	MEDIA_PAD_FL_SOURCE
};

static const struct v4l2_mbus_framefmt
	tcc_isp_mbus_frmfmt_default[TCC_ISP_PAD_NUM] = {
		/* SINK PAD */
		{
			.width = 1,
			.height = 1,
			.code = MEDIA_BUS_FMT_SRGGB8_1X8,
			.field = V4L2_FIELD_NONE,
		},
		/* SRC PAD */
		{
			.width = 1,
			.height = 1,
			.code = MEDIA_BUS_FMT_UYVY8_1X16,
			//.code = MEDIA_BUS_FMT_YUV8_1X24,
			//.code = MEDIA_BUS_FMT_RGB888_1X24,
			.field = V4L2_FIELD_NONE,
		},
	};

/*
 * Helper functions for reflection
 */
static inline struct v4l2_subdev *ctrl_to_sd(const struct v4l2_ctrl *c)
{
	return (&(container_of(c->handler,
			       struct tcc_isp_state,
			       ctrl_hdl)->sd));
}

static inline struct tcc_isp_state *sd_to_state(const struct v4l2_subdev *sd)
{
	return (struct tcc_isp_state *)v4l2_get_subdevdata(sd);
}

static int tcc_isp_nf_bound(struct v4l2_async_notifier *nf,
			    struct v4l2_subdev *sd,
			    struct v4l2_async_subdev *asd)
{
	struct tcc_isp_state *state = NULL;
	int ret = 0;

	state = container_of(nf, struct tcc_isp_state, nf);

	logi(&(state->pdev->dev), "v4l2-subdev %s is bounded\n", sd->name);

	return ret;
}

void tcc_isp_nf_unbind(struct v4l2_async_notifier *nf,
		       struct v4l2_subdev *sd,
		       struct v4l2_async_subdev *asd)
{
	struct tcc_isp_state *state = NULL;

	state = container_of(nf, struct tcc_isp_state, nf);

	logi(&(state->pdev->dev), "v4l2-subdev %s is unbounded\n", sd->name);
}

static const struct v4l2_async_notifier_operations tcc_isp_nf_ops = {
	.bound = tcc_isp_nf_bound,
	.unbind = tcc_isp_nf_unbind,
};

static int32_t tcc_isp_s_ctrl(struct v4l2_ctrl *ctrl)
{
	const struct v4l2_subdev *sd = ctrl_to_sd(ctrl);
	const struct tcc_isp_state *state = sd_to_state(sd);
	int32_t ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		logi(&(state->pdev->dev),
				"V4L2_CID_BRIGHTNESS(%d)\n",
				ctrl->val);
		break;
	default:
		loge(&(state->pdev->dev),
			"NOT supported CID(0x%x)\n", ctrl->id);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct v4l2_ctrl_ops tcc_isp_ctrl_ops = {
	.s_ctrl = tcc_isp_s_ctrl,
};

static int32_t tcc_isp_dt_res(struct tcc_isp_state *state)
{
	const struct resource *mem_res;
	int32_t ret = 0;

	/* Get ISP base address */
	mem_res = platform_get_resource_byname(state->pdev,
					       IORESOURCE_MEM,
					       "isp_base");
	state->isp_base = devm_ioremap_resource(&(state->pdev->dev),
						mem_res);
	if (IS_ERR((const void *)state->isp_base)) {
		/* error */
		ret = (int32_t)PTR_ERR((const void *)state->isp_base);
		loge(&(state->pdev->dev),
				"invalid isp_base addr(%d)\n",
				ret);
	}

	/* Get mem base address */
	if (ret >= 0) {
		mem_res = platform_get_resource_byname(state->pdev,
						       IORESOURCE_MEM,
						       "mem_base");
		state->mem_base =
			ioremap(mem_res->start, resource_size(mem_res));
		if (IS_ERR((const void *)state->mem_base)) {
			/* error */
			ret = (int32_t)PTR_ERR((const void *)state->mem_base);
			loge(&(state->pdev->dev),
					"invalid mem_base addr(%d)\n",
					ret);
		}
	}

	/* Get CFG base address */
	if (ret >= 0) {
		mem_res = platform_get_resource_byname(state->pdev,
						       IORESOURCE_MEM,
						       "cfg_base");
		state->cfg_base =
			ioremap(mem_res->start, resource_size(mem_res));
		if (IS_ERR((const void *)state->cfg_base)) {
			/* error */
			ret = (int32_t)PTR_ERR((const void *)state->cfg_base);
			loge(&(state->pdev->dev),
					"invalid cfg_base addr(%d)\n",
					ret);
		}
	}

	/* Get RGBIR Sync base address */
	if (ret >= 0) {
		mem_res = platform_get_resource_byname(state->pdev,
						       IORESOURCE_MEM,
						       "rgbir_sync_base");
		if (mem_res != NULL) {
			state->rgbir_sync_base =
				ioremap(mem_res->start, resource_size(mem_res));
			if (IS_ERR((const void *)state->rgbir_sync_base)) {
				/* error */
				ret = (int32_t)PTR_ERR(
					(const void *)state->rgbir_sync_base);
				loge(&(state->pdev->dev),
				     "invalid rgbir_sync_base addr(%d)\n", ret);
			}
		}
	}

#ifdef USE_ISP_UART
	/* Get UART pinctrl */
	if (ret >= 0) {
		state->uart_pctl =
			devm_pinctrl_get_select_default(&(state->pdev->dev));
		if (IS_ERR(state->uart_pctl)) {
			/* error */
#ifdef CONFIG_ARCH_TCC805X
			ret = (int32_t)PTR_ERR((const void *)state->uart_pctl);

			loge(&(state->pdev->dev),
					"invalid uart pinctrl property(%d)\n",
					ret);
#endif
#if defined(CONFIG_ARCH_TCC750X) || defined(CONFIG_ARCH_TCC807X)
			logi(&(state->pdev->dev),
					"invalid uart pinctrl property(%d)\n",
					ret);
#endif
		}
	}
#endif

	/* Get reserved memroy for 3DNR */
	if (ret >= 0) {
		struct device_node *mem_node = NULL;

		mem_node = of_parse_phandle(state->pdev->dev.of_node, "memory-region", 0);
		if (!IS_ERR_OR_NULL(mem_node)) {
			state->rsvd_mem_3dnr = of_reserved_mem_lookup(mem_node);
			if (!IS_ERR_OR_NULL(state->rsvd_mem_3dnr)) {
				logd(&(state->pdev->dev),
				     "%20s: 0x%08llx ~ 0x%08llx (0x%08llx)\n",
				     "3DNR memory", state->rsvd_mem_3dnr->base,
				     state->rsvd_mem_3dnr->base +
					     state->rsvd_mem_3dnr->size,
				     state->rsvd_mem_3dnr->size);
			}
			of_node_put(mem_node);
		} else {
			/*
			 * 3NDR can be used with only ISP0.
			 * If 3DNR is needed, alloc memory for DMA of 3DNR
			 */
			logi(&(state->pdev->dev), "3DNR memory doesn't exist\n");
		}
	}

	/* Get scene data base address */
	if (ret >= 0) {
		mem_res = platform_get_resource_byname(state->pdev,
						       IORESOURCE_MEM,
						       "scene_data_base");
		if (mem_res != NULL) {
			state->scene_data_base =
				ioremap(mem_res->start, resource_size(mem_res));
			if (IS_ERR((const void *)state->scene_data_base)) {
				/* error */
				ret = (int32_t)PTR_ERR(
					(const void *)state->scene_data_base);
				loge(&(state->pdev->dev),
				     "invalid scene_data_base addr(%d)\n", ret);
			}
		}
	}

	return ret;
}

static int32_t tcc_isp_parse_dt(struct tcc_isp_state *state)
{
	int32_t ret = 0;

	ret = of_alias_get_id(state->pdev->dev.of_node, "isp");
	if ((ret >= 0) &&
	    (((uint32_t)ret) < TCC_ISP_MAX_CORE)) {
		/* okay */
		state->pdev->id = ret;
	} else {
		/* error */
		loge(&(state->pdev->dev),
				"of_alias_get_id returned(%d)\n",
				ret);
	}

	/* Parsing used resource */
	if (ret >= 0) {
		ret = tcc_isp_dt_res(state);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
				"tcc_isp_dt_res returned %d",
				ret);
		}
	}

	/* Get mem_share option */
	if (ret >= 0) {
		ret = of_property_read_u32(state->pdev->dev.of_node,
					   "mem_share",
					   &(state->mem_share));
		if (ret < 0) {
			/* default value */
			logi(&(state->pdev->dev),
					"disable mem_share\n");
			ret = 0;
		}
	}

	if (ret >= 0) {
		ret = of_property_read_u32(state->pdev->dev.of_node,
					   "cfa",
					   &(state->demosaic_mode));
		if (ret < 0) {
			/* default value */
			logi(&(state->pdev->dev),
					"default CFA is RGGB\n");
			ret = 0;
		} else {
			logi(&(state->pdev->dev),
					"CFA is %d\n", state->demosaic_mode);
		}
	}

	if (ret >= 0) {
		ret = of_property_read_u32_index(state->pdev->dev.of_node,
					   "out_win_crop", 0U,
					   &(state->out_win_crop.x));
		if (ret < 0) {
			/* default value */
			logi(&(state->pdev->dev),
					"default output win crop x pos is 8\n");
			state->out_win_crop.x = 8U;
			ret = 0;
		} else {
			logi(&(state->pdev->dev),
					"Output win crop x pos is %d\n",
					state->out_win_crop.x);
		}
	}

	if (ret >= 0) {
		ret = of_property_read_u32_index(state->pdev->dev.of_node,
					   "out_win_crop", 1U,
					   &(state->out_win_crop.y));
		if (ret < 0) {
			/* default value */
			logi(&(state->pdev->dev),
					"default output win crop y pos is 8\n");
			state->out_win_crop.y = 8U;
			ret = 0;
		} else {
			logi(&(state->pdev->dev),
					"Output win crop y pos is %d\n",
					state->out_win_crop.y);
		}
	}

	if (ret >= 0 && state->demosaic_mode == 3U) {
		ret = of_property_read_u32(state->pdev->dev.of_node,
					   "rgbir-order",
					   &(state->rgbir_order));
		if (ret < 0) {
			/* default value */
			logi(&(state->pdev->dev),
					"default RGB-IR is GBIRG\n");
			ret = 0;
		} else {
			logi(&(state->pdev->dev),
					"RGB-IR is %d\n", state->rgbir_order);
		}
	}

	return ret;
}

static void tcc_isp_init_format(struct tcc_isp_state *state)
{
	state->dv_timings.type = V4L2_DV_BT_656_1120;
	state->dv_timings.bt.width =  DEFAULT_WIDTH;
	state->dv_timings.bt.height = DEFAULT_HEIGHT;
	state->dv_timings.bt.interlaced = V4L2_DV_PROGRESSIVE;
	/* IMPORTANT
	 * The below field "polarities" is not used
	 * because polarities for vsync and hsync are supported only.
	 * So, use flags of "struct v4l2_mbus_config".
	 */
	state->dv_timings.bt.polarities = 0U;
}

static int32_t tcc_isp_init_controls(struct tcc_isp_state *state)
{
	int32_t ret = 0;

	v4l2_ctrl_handler_init(&state->ctrl_hdl, 1U);

	if (v4l2_ctrl_new_std(&state->ctrl_hdl, &tcc_isp_ctrl_ops,
			      V4L2_CID_BRIGHTNESS, TCC_ISP_BRI_MIN,
			      TCC_ISP_BRI_MAX, 1, TCC_ISP_BRI_DEF) == NULL) {
		/* error */
		ret  = state->ctrl_hdl.error;
		loge(&(state->pdev->dev),
			"v4l2_ctrl_new_std returned %d\n",
			state->ctrl_hdl.error);
		v4l2_ctrl_handler_free(&state->ctrl_hdl);
	} else {
		/* okay */
		state->sd.ctrl_handler = &state->ctrl_hdl;

		/*
		 * call s_ctrl for all controls unconditionally.
		 * this ensures that both the internal data and
		 * the hardware are in sync
		 */
		ret = v4l2_ctrl_handler_setup(&state->ctrl_hdl);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
				"v4l2_ctrl_handler_setup returned %d\n",
				ret);
		}
	}

	return ret;
}

static void tcc_isp_exit_controls(struct tcc_isp_state *state)
{
	v4l2_ctrl_handler_free(&state->ctrl_hdl);
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int32_t tcc_isp_load_fw(struct v4l2_subdev *sd);
static int32_t tcc_isp_init(struct v4l2_subdev *sd, uint32_t enable)
{
	const struct tcc_isp_state *state = sd_to_state(sd);
	int32_t ret = 0;

	ret = tcc_isp_core_init(state, enable);
	if (ret < 0) {
		/* error */
		loge(&(state->pdev->dev), "tcc_isp_load_fw returned %d\n",
		     ret);
	}

	if ((ret >= 0) && (enable != 0U)) {
		ret = tcc_isp_load_fw(sd);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
			     "tcc_isp_load_fw returned %d\n", ret);
		}
	}

	return ret;
}

static int32_t tcc_isp_load_fw(struct v4l2_subdev *sd)
{
	struct tcc_isp_state *state = sd_to_state(sd);
	int32_t ret = 0;

	if (state->fw_load == 1) {
		logi(&(state->pdev->dev), "skip loading firmware\n");
	} else {
		ret = tcc_isp_cmm_read_file(state,
					    state->isp_fw_name,
					    tcc_isp_mcu_load_firmware);
		if (ret < 0) {
			loge(&(state->pdev->dev),
					"FAIL - loading firmware(%s)\n",
					state->isp_fw_name);
		}
	}

	return ret;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static void tcc_isp_dbg_register(const struct tcc_isp_state *state,
				 const uint64_t reg, const uint64_t val)
{
	if ((reg & 0x10000U) == 0x10000U) {
		logd(&(state->pdev->dev),
			"%pS SW 3A Memory, offset(0x%llx), val(0x%llx)\n",
			__builtin_return_address(0), (reg & 0xFFFFU), val);
	} else {
		logd(&(state->pdev->dev),
			"%pS HW ISP, offset(0x%llx), val(0x%llx)\n",
			__builtin_return_address(0), (reg & 0xFFFFU), val);
	}
}

static int32_t tcc_isp_g_register(struct v4l2_subdev *sd,
				  struct v4l2_dbg_register *reg)
{
	const struct tcc_isp_state *state = sd_to_state(sd);
	int32_t ret = 0;

	ret = tcc_isp_core_get_reg(state, reg->reg, &reg->val);
	if (ret < 0) {
		/* error */
		loge(&(state->pdev->dev),
			"tcc_isp_core_get_reg returned %d\n",
			ret);
	} else {
		/* okay */
		tcc_isp_dbg_register(state, reg->reg, reg->val);
		reg->size = ret;
	}

	return ret;
}

static int32_t tcc_isp_s_register(struct v4l2_subdev *sd,
				  const struct v4l2_dbg_register *reg)
{
	const struct tcc_isp_state *state = sd_to_state(sd);
	int32_t ret = 0;

	if (reg->size != sizeof(uint32_t)) {
		loge(&(state->pdev->dev), "invalid size\n");
		ret = -EINVAL;
	}

	if (ret >= 0) {
		ret = tcc_isp_core_set_reg(state, reg->reg, reg->val);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
				"tcc_isp_core_set_reg returned %d\n",
				ret);
		} else {
			/* okay */
			tcc_isp_dbg_register(state, reg->reg, reg->val);
		}
	}

	return ret;
}
#endif

static int32_t tcc_isp_s_power(struct v4l2_subdev *sd, int on)
{
	struct tcc_isp_state *state = sd_to_state(sd);
	int32_t ret = 0;

	if (on != 0) {
		if (state->setting_load == 1) {
			logi(&(state->pdev->dev), "skip loading setting\n");
		} else {
			ret = tcc_isp_cmm_read_file(state,
						    state->isp_setting_name,
						    tcc_isp_core_load_setting);
			if (ret < 0) {
				loge(&(state->pdev->dev),
						"FAIL - loading setting(%s)\n",
						state->isp_setting_name);
			}
		}
	} else {

	}

	return ret;
}

/*
 * v4l2_subdev_video_ops implementations
 */
static int tcc_isp_get_src_sd(const struct tcc_isp_state *state,
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

static int32_t tcc_isp_s_stream(struct v4l2_subdev *sd, int enable)
{
	const struct tcc_isp_state *state = sd_to_state(sd);
	struct v4l2_subdev *src_sd;
	int32_t ret = 0;

	ret = tcc_isp_get_src_sd(state, &sd->entity.pads[TCC_ISP_PAD_SINK],
				 &src_sd);
	if (ret < 0) {
		/* error */
		loge(&state->pdev->dev, "tcc_isp_get_src_sd returned %d\n",
		     ret);
	} else {
		/* okay */
		logi(&(state->pdev->dev), "subdev call(%s - %s)\n",
		     src_sd->name, "s_stream");
		ret = v4l2_subdev_call(src_sd, video, s_stream, enable);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
			     "subdev_call(%s - %s %s) returned %d\n",
			     src_sd->name, "s_stream",
			     enable ? "enable" : "disabled", ret);
		} else {
			/* okay */
			tcc_isp_mcu_s_stream(state, enable);
#if defined(CONFIG_ARCH_TCC750X) || defined(CONFIG_ARCH_TCC807X)
			tcc_isp_core_set_deblank_on(state);
#endif
		}
	}

	return ret;
}

static int32_t tcc_isp_g_dv_timings(struct v4l2_subdev *sd,
				    struct v4l2_dv_timings *timings)
{
	const struct tcc_isp_state *state = sd_to_state(sd);
	int32_t ret = 0;

	*timings = state->dv_timings;

	return ret;
}

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
static int tcc_isp_g_mbus_config(struct v4l2_subdev *sd,
				 unsigned int pad,
				 struct v4l2_mbus_config *cfg)
{
	const struct tcc_isp_state *state = sd_to_state(sd);
	int32_t ret = 0;

	*cfg = isp_mbus_config;

	logd(&(state->pdev->dev),
	     "type(0x%x), flags(0x%x)\n",
	     isp_mbus_config.type, isp_mbus_config.flags);

	return ret;
}
#else
static int tcc_isp_g_mbus_config(struct v4l2_subdev *sd,
				 struct v4l2_mbus_config *cfg)
{
	const struct tcc_isp_state *state = sd_to_state(sd);
	int32_t ret = 0;

	*cfg = isp_mbus_config;

	logd(&(state->pdev->dev),
			"type(0x%x), flags(0x%x)\n",
			isp_mbus_config.type, isp_mbus_config.flags);

	return ret;
}
#endif

/*
 * v4l2_subdev_pad_ops implementations
 */
static int32_t tcc_isp_init_cfg(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg)
{
	const struct tcc_isp_state *state = sd_to_state(sd);
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

static int32_t tcc_isp_get_fmt(struct v4l2_subdev *sd,
			       struct v4l2_subdev_pad_config *cfg,
			       struct v4l2_subdev_format *f)
{
	const struct tcc_isp_state *state = sd_to_state(sd);
	int32_t ret = 0;

	if (f->pad >= TCC_ISP_PAD_NUM) {
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
			f->format = state->fmt[f->pad];
		}
	}

	return ret;
}

static int32_t tcc_isp_set_fmt(struct v4l2_subdev *sd,
			       struct v4l2_subdev_pad_config *cfg,
			       struct v4l2_subdev_format *f)
{
	struct tcc_isp_state *state = sd_to_state(sd);
	struct v4l2_mbus_framefmt *fmt = NULL;
	int32_t ret = 0;

	/* check pad idx */
	if (f->pad >= TCC_ISP_PAD_NUM) {
		/* error */
		loge(&state->pdev->dev, "invalid pad num(%d)\n", f->pad);
		ret = -EINVAL;
	}

	/* check width and height */
	if (ret >= 0) {
		if ((f->format.width <= 16U) || (f->format.height <= 16U)) {
			/* error */
			ret = -EINVAL;
			loge(&(state->pdev->dev), "invalid resolution(%dx%d)\n",
			     f->format.width, f->format.height);
		}
	}

	/* get try or active mbus framefmt pointer */
	if (ret >= 0) {
		if (f->which == V4L2_SUBDEV_FORMAT_TRY) {
			/* get try format */
			fmt = v4l2_subdev_get_try_format(sd, cfg, f->pad);
		} else {
			/* get active format */
			fmt = &state->fmt[f->pad];
		}
		*fmt = f->format;
	}

	/* set input pad */
	if (ret >= 0) {
		if (f->pad == TCC_ISP_PAD_SINK) {
			/* set padding data */
			tcc_isp_core_padding(state,
					     state->fmt[TCC_ISP_PAD_SINK].code);
		}
	}

	/* set output pad */
	if (ret >= 0) {
		uint32_t crop_w = state->out_win_crop.x * 2;
		uint32_t crop_h = state->out_win_crop.y * 2;

		/*
		 * Because of ISP algorithm characteristics,
		 * isp output resolution is small than input resolution
		 * (top, bottom, left, top 8 lines will be reduced)
		 */
		state->dv_timings.bt.width = state->fmt[TCC_ISP_PAD_SRC].width =
			state->fmt[TCC_ISP_PAD_SINK].width - crop_w;
		state->dv_timings.bt.height = state->fmt[TCC_ISP_PAD_SRC].height =
			state->fmt[TCC_ISP_PAD_SINK].height - crop_h;
	}

	return ret;
}

/*
 * v4l2_subdev_internal_ops implementations
 */
static int tcc_isp_registered(struct v4l2_subdev *sd)
{
	struct tcc_isp_state *state = sd_to_state(sd);
	int ret = 0;

	logd(&state->pdev->dev, "registered with v4l2 dev\n");

	return ret;
}

/*
 * v4l2_subdev_ops implementations
 */
static const struct v4l2_subdev_core_ops tcc_isp_core_ops = {
	.init			= tcc_isp_init,
	.load_fw		= tcc_isp_load_fw,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register		= tcc_isp_g_register,
	.s_register		= tcc_isp_s_register,
#endif
	.s_power		= tcc_isp_s_power,
};

static const struct v4l2_subdev_video_ops tcc_isp_video_ops = {
	.s_stream		= tcc_isp_s_stream,
	.g_dv_timings		= tcc_isp_g_dv_timings,
#if KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE
	.g_mbus_config		= tcc_isp_g_mbus_config,
#endif
};

static const struct v4l2_subdev_pad_ops tcc_isp_pad_ops = {
	.init_cfg		= tcc_isp_init_cfg,
	.get_fmt		= tcc_isp_get_fmt,
	.set_fmt		= tcc_isp_set_fmt,
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
	.get_mbus_config	= tcc_isp_g_mbus_config,
#endif
};

static const struct v4l2_subdev_ops tcc_isp_ops = {
	.core			= &tcc_isp_core_ops,
	.video			= &tcc_isp_video_ops,
	.pad			= &tcc_isp_pad_ops,
};

static const struct v4l2_subdev_internal_ops tcc_isp_internal_ops = {
	.registered = tcc_isp_registered,
};

static const struct of_device_id tcc_isp_of_match[];

static ssize_t mdelay_to_output_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	const struct tcc_isp_state *state =
		platform_get_drvdata(to_platform_device(dev));
	ssize_t ret = 0;

	if (state == NULL) {
		loge(dev, "Fail - tcc_isp_state pointer\n");
		ret = -ENODEV;
	}


	if (ret >= 0) {
		ret = scnprintf(buf,
				PAGE_SIZE,
				"%llu\n",
				state->mdelay_to_out);
		if (!((ret > 0) && (((uint32_t)ret) < PAGE_SIZE))) {
			loge(&(state->pdev->dev),
					"scnprintf returned %ld\n",
					ret);
			ret = -EINVAL;
		}
	}

	return ret;
}

static ssize_t mdelay_to_output_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct tcc_isp_state *state =
		platform_get_drvdata(to_platform_device(dev));
	uint64_t val;
	ssize_t ret = 0;

	if (state == NULL) {
		loge(dev, "Fail - tcc_isp_state pointer\n");
		ret = -ENODEV;
	}

	if (ret >= 0) {
		ret = kstrtoull(buf, 0, &val);
		if (ret < 0) {
			loge(&(state->pdev->dev),
					"Fail - read data from attribute\n");
		}
	}

	if (ret >= 0) {
		/* print value */
		if (count <= (SIZE_MAX >> 1U)) {
			logi(&(state->pdev->dev),
					"set mdelay_to_output (%llu)\n",
					val);
			state->mdelay_to_out = val;
			ret = (ssize_t)count;
		} else {
			ret = -EINVAL;
		}
	}

	return ret;
}

static DEVICE_ATTR_RW(mdelay_to_output);

static int32_t tcc_isp_set_file_name(struct tcc_isp_state *state)
{
	int32_t ret = 0;

	/* set MCU F/W name  */
	ret = scnprintf(state->isp_fw_name,
			sizeof(state->isp_fw_name),
#ifdef CONFIG_ARCH_TCC805X
			"%s-%d",
			TCC_ISP_FIRMWARE_NAME, state->pdev->id);
#endif
#if defined(CONFIG_ARCH_TCC750X) || defined(CONFIG_ARCH_TCC807X)
			"%s",
			TCC_ISP_FIRMWARE_NAME);
#endif
	if (!((ret > 0) &&
	      (((uint32_t)ret) < sizeof(state->isp_fw_name)))) {
		/* error */
		loge(&(state->pdev->dev),
				"scnprintf returned %d\n",
				ret);
		ret = -EINVAL;
	}

	/* set ISP setting file name  */
	if (ret >= 0) {
		ret = scnprintf(state->isp_setting_name,
				sizeof(state->isp_setting_name),
				"%s-%d",
				TCC_ISP_SETTING_NAME, state->pdev->id);
		if (!((ret > 0) &&
		      (((uint32_t)ret) < sizeof(state->isp_setting_name)))) {
			/* error */
			loge(&(state->pdev->dev),
					"scnprintf returned %d\n",
					ret);
			ret = -EINVAL;
		}
	}

	return ret;
}

static int tcc_isp_add_asd(struct tcc_isp_state *state)
{
	struct v4l2_async_subdev *found_asd;
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
		list_for_each_entry(found_asd, &state->nf.asd_list, asd_list) {
			logi(&state->pdev->dev, "asd %s has been found\n",
			     to_of_node(found_asd->match.fwnode)->name);
		}
	}

	return ret;
}

static int32_t tcc_isp_register_asd_nf(struct tcc_isp_state *state)
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

static int32_t tcc_isp_init_sd(struct tcc_isp_state *state)
{
	uint32_t idx = 0U;
	int ret = 0;

	/* init subdev */
	v4l2_subdev_init(&(state->sd), &tcc_isp_ops);
	state->sd.internal_ops = &tcc_isp_internal_ops;
	state->sd.owner = state->pdev->dev.driver->owner;
	state->sd.dev = &state->pdev->dev;
	state->sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	state->sd.entity.function = MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER;
	state->sd.entity.flags = MEDIA_ENT_FL_DEFAULT;
	v4l2_set_subdevdata(&(state->sd), state);
	/* initialize name */
	ret = scnprintf(state->sd.name,
			sizeof(state->sd.name),
			"tcc-isp-%d", state->pdev->id);
	if (!((ret > 0) &&
	      (((uint32_t)ret) < sizeof(state->sd.name)))) {
		/* error */
		loge(&(state->pdev->dev),
				"scnprintf returned %d\n",
				ret);
		ret = -EINVAL;
	}

	/* init pads */
	for (idx = 0U; idx < TCC_ISP_PAD_NUM; idx++) {
		state->pads[idx].index = idx;
		state->pads[idx].flags = tcc_isp_pad_flag[idx];
		state->fmt[idx] = tcc_isp_mbus_frmfmt_default[idx];
	}
	media_entity_pads_init(&state->sd.entity, TCC_ISP_PAD_NUM, state->pads);

	/* init notifier */
	v4l2_async_notifier_init(&state->nf);
	state->nf.ops = &tcc_isp_nf_ops;

	/* add async subdevs */
	if (ret >= 0) {
		ret = tcc_isp_add_asd(state);
		if (ret < 0) {
			loge(&(state->pdev->dev),
			     "tcc_isp_add_asd returned %d\n", ret);
		}
	}

	/* register async notifier */
	if (ret >= 0) {
		ret = tcc_isp_register_asd_nf(state);
		if (ret < 0) {
			loge(&(state->pdev->dev),
			     "tcc_isp_register_asd_nf returned %d\n", ret);
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

	/* initialize v4l2 control handler */
	if (ret >= 0) {
		ret = tcc_isp_init_controls(state);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
				"stcc_isp_init_controls returned %d\n",
				ret);
			v4l2_async_unregister_subdev(&(state->sd));
		}
	}

	return ret;
}

static int32_t tcc_isp_init_state(struct tcc_isp_state *state,
				  struct platform_device *pdev)
{
	const struct of_device_id *of_id = NULL;
	int32_t ret = 0;

	state->pdev = pdev;
	platform_set_drvdata(pdev, state);

	tcc_isp_init_format(state);

	/* Parse device tree */
	ret = tcc_isp_parse_dt(state);
	if (ret < 0) {
		/* error */
		loge(&(state->pdev->dev),
				"tcc_isp_parse_dt returned %d\n",
				ret);
	}

	if (ret >= 0) {
		of_id = of_match_node(tcc_isp_of_match,
				      state->pdev->dev.of_node);
		if (of_id == NULL) {
			/* error */
			loge(&(state->pdev->dev),
					"of_match_node returned NULL\n");
			ret = -EINVAL;
		}
	}

	if (ret >= 0) {
		ret = tcc_isp_set_file_name(state);
		if (ret < 0) {
			loge(&(state->pdev->dev),
				"tcc_isp_set_file_name returned %d\n",
				ret);
		}
	}

	if (ret >= 0) {
		ret = tcc_isp_init_sd(state);
		if (ret < 0) {
			loge(&(state->pdev->dev),
				"tcc_isp_init_sd returned %d\n",
				ret);
		}
	}

	return ret;
}

static int tcc_isp_probe(struct platform_device *pdev)
{
	struct tcc_isp_state *state;
	int ret = 0;

	state = devm_kzalloc(&pdev->dev, sizeof(*state), GFP_KERNEL);
	if (state == NULL) {
		/* error */
		loge(&(pdev->dev),
			"devm_kzalloc returned NULL\n");
		ret = -ENOMEM;
	}

	if (ret >= 0) {
		ret = device_create_file(&(pdev->dev),
					 &dev_attr_mdelay_to_output);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
					"device_create_file returned %d\n",
					ret);
		}
	}

	if (ret >= 0) {
		ret = tcc_isp_init_state(state, pdev);
		if (ret < 0) {
			loge(&(pdev->dev),
				"tcc_isp_init_state returned %d\n",
				ret);
		}
	}

	if (ret >= 0) {
		logi(&(state->pdev->dev),
				"Success proving tcc-isp-%d\n",
				pdev->id);
		tcc_isp_core_set_default(state);
		tcc_isp_mcu_set_ctl_probed(state, TCC_ISP_PROBED);
	}

	goto end;

end:
	return ret;
}

static int tcc_isp_remove(struct platform_device *pdev)
{
	struct tcc_isp_state *state =
		(struct tcc_isp_state *)platform_get_drvdata(pdev);
	int ret = 0;

	logi(&(state->pdev->dev), "%s in\n", __func__);

	device_remove_file(&(state->pdev->dev), &dev_attr_mdelay_to_output);
	tcc_isp_exit_controls(state);

	tcc_isp_mcu_set_ctl_probed(state, TCC_ISP_NOPROBED);

	logi(&(state->pdev->dev), "%s out\n", __func__);

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int tcc_isp_suspend(struct device *dev)
{
	const struct platform_device *pdev = to_platform_device(dev);
	const struct tcc_isp_state *state = platform_get_drvdata(pdev);
	int ret = 0;

	logi(&(state->pdev->dev), "%s in\n", __func__);


	return ret;
}

static int tcc_isp_resume(struct device *dev)
{
	const struct platform_device *pdev = to_platform_device(dev);
	struct tcc_isp_state *state = platform_get_drvdata(pdev);
	int ret = 0;

	logi(&(state->pdev->dev), "%s in\n", __func__);

	tcc_isp_core_set_default(state);

	state->fw_load = 0;
	state->setting_load = 0;

	return ret;
}
#endif

static const struct of_device_id tcc_isp_of_match[] = {
	{
		.compatible = "telechips,tcc805x-isp",
	},
	{
		.compatible = "telechips,tcc750x-isp",
	},
	{
		.compatible = "telechips,tcc807x-isp",
	},
	{
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, tcc_isp_of_match);

static const struct dev_pm_ops tcc_isp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tcc_isp_suspend, tcc_isp_resume)
};

static struct platform_driver tcc_isp_driver = {
	.probe = tcc_isp_probe,
	.remove = tcc_isp_remove,
	.driver = {
		.name = TCC_ISP_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tcc_isp_of_match,
		.pm = &tcc_isp_pm_ops,
	},
};

static int __init tcc_isp_driver_init(void)
{
	int ret;

	ret = platform_driver_register(&tcc_isp_driver);
	if (ret < 0) {
		/* error */
		tccisp_loge("platform_driver_register returned %d\n", ret);
	}

	if (ret >= 0) {
		ret = tcc_isp_cmm_add_drv_attr(&tcc_isp_driver);
		if (ret < 0) {
			/* error */
			tccisp_loge("tcc_isp_cmm_add_drv_attr returned %d\n",
				    ret);
		}
	}

	return ret;
}
module_init(tcc_isp_driver_init);

static void __exit tcc_isp_driver_exit(void)
{
	tcc_isp_cmm_rm_drv_attr(&tcc_isp_driver);

	platform_driver_unregister(&tcc_isp_driver);
}
module_exit(tcc_isp_driver_exit);

MODULE_AUTHOR("Telechips <www.telechips.com>");
MODULE_DESCRIPTION("Telechips TCCXXXX SoC ISP driver");
MODULE_LICENSE("GPL");
