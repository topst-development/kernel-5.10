// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * TCC DRM DSI Device Driver
 *
 * Copyright (C) 2022 Telechips Inc.
 *
 * Authors:
 *	Jayden Kim
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

//#include <drm/drmP.h>
#include <drm/drm_print.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_panel.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_of.h>

#include <linux/clk.h>
#include <linux/of_graph.h>
#include <linux/regulator/consumer.h>
#include <linux/fs.h>
#include <linux/component.h>
#include <linux/tcc_math.h>
#include <linux/platform_device.h>

#include <video/of_videomode.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>
#include <telechips_drm_drv.h>
#include <telechips_drm_dsi.h>
#include <telechips_drm_types.h>


#include <dsih_core.h>
#include <dsih_api.h>
#include <dsih_dphy.h>
#include <dsih_hal.h>
#include <dsih_serdes.h>
#include <dsih_includes.h>

#define LOG_TAG "DRM_DSI"

#define DRIVER_DATE	"20240514"
#define DRIVER_MAJOR 1
#define DRIVER_MINOR 1
#define DRIVER_PATCH 2

struct tccdrm_dsi_context {
	struct drm_encoder encoder;
	struct drm_connector connector;
	struct device *dev;

	struct drm_panel *panel;
	struct mipi_dsi_dev dsi_dev;
	struct display_timings *timings;

	/* DSI data from device tree */
	unsigned int sdm_bypass;
	unsigned int trvc_bypass;
	unsigned int display_ch;
	unsigned long disp_pclk;

	bool enabled;
	bool binded;
};

#define connector_to_context(x) \
		container_of((x), struct tccdrm_dsi_context, connector)
#define encoder_to_context(x) \
		container_of((x), struct tccdrm_dsi_context, encoder)


//TODO: set get connector status from dsi driver
/* coverity[misra_c_2012_rule_8_13] */
static enum drm_connector_status tccdrm_dsi_detect(struct drm_connector *connector,
						     bool force)
{
	enum drm_connector_status connector_status = connector_status_connected;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter connector is not used
	 * in the function
	 */
	(void)connector;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter force is not used
	 * in the function
	 */
	(void)force;

	return connector_status;
}

static void tccdrm_dsi_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static const struct drm_connector_funcs tccdrm_dsi_connector_funcs = {
	.detect = tccdrm_dsi_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = tccdrm_dsi_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int tccdrm_dsi_register_mode(struct drm_connector *connector,
				      struct drm_display_mode *in_mode)
{
	const struct tccdrm_dsi_context *dev_context;
	bool internal_ok = (bool)true;
	int tmp_val;
	int ret = 0;

	if (connector == NULL) {
		DRM_DEV_ERROR(NULL, "connector is NULL\r\n");
		ret = -EINVAL;
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		/* coverity[cert_arr39_c] */
		/* coverity[cert_dcl37_c] */
		/* coverity[misra_c_2012_rule_8_5] */	//
		/* coverity[misra_c_2012_rule_8_6] */	//
		/* coverity[misra_c_2012_rule_8_13] */
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_11_5] */
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_15_6] */
		/* coverity[misra_c_2012_rule_18_4] */
		/* coverity[misra_c_2012_rule_20_7] */
		/* coverity[misra_c_2012_rule_21_2] */
		dev_context = (const struct tccdrm_dsi_context *)connector_to_context(connector);
		if (dev_context == NULL) {
			DRM_DEV_ERROR(NULL, "dev_context is NULL\r\n");
			ret = -EINVAL;
			internal_ok = (bool)false;
		}
	}

	if (internal_ok) {
		/*
		 * Physical size as value that display
		 * resolution divided by 10.
		 */
		if (in_mode->hdisplay > 10u) {
			/* coverity[misra_c_2012_rule_10_4] */
			tmp_val = DIV_ROUND_UP(in_mode->hdisplay, 10);
		} else {
			tmp_val = (int)in_mode->hdisplay;
		}
		if (tmp_val > 0) {
			connector->display_info.width_mm = (unsigned int)tmp_val;
		} else {
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		/*
		 * Physical size as value that display
		 * resolution divided by 10.
		 */
		if (in_mode->vdisplay > 10u) {
			/* coverity[misra_c_2012_rule_10_4] */
			tmp_val = DIV_ROUND_UP(in_mode->vdisplay, 10);
		} else {
			tmp_val = (int)in_mode->vdisplay;
		}
		if (tmp_val > 0) {
			connector->display_info.height_mm = (unsigned int)tmp_val;
			drm_mode_probed_add(connector, in_mode);
		} else {
			ret = -EINVAL;
		}
	}

	return ret;
}


static int tccdrm_dsi_get_dev_node_modes(struct tccdrm_dsi_context *dev_context)
{
	struct drm_connector *connector = &dev_context->connector;
	struct drm_display_mode *modes = NULL;
	bool internal_ok = (bool)true;
	int i, mode_count = 0;

	const struct display_timings *timings;
	struct videomode vm;
	int num_timings = 0;

	if (dev_context->timings == NULL) {
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		timings = dev_context->timings;
		if (!tcc_math_uint_gt_intmax(timings->num_timings)) {
			num_timings = (int)timings->num_timings;
		}
	}
	for (i = 0; i < num_timings; i++) {
		if (videomode_from_timings(timings, &vm,
					   (unsigned int)i) < 0) {
			continue;
		}

		modes = drm_mode_create(connector->dev);
		if (modes == NULL) {
			DRM_DEV_ERROR(dev_context->dev,
				     "failed to create drm_mode\r\n");
			continue;
		}

		drm_display_mode_from_videomode(&vm, modes);

		/* coverity[misra_c_2012_rule_10_1] */
		modes->type = DRM_MODE_TYPE_DRIVER;
		drm_mode_set_name(modes);
		if (timings->native_mode == (unsigned int)i) {
			DRM_DEV_INFO(dev_context->dev,
				     "[INFO] Native mode is detected at index [%d] name [%s]\r\n",
				mode_count, modes->name);
			/* coverity[misra_c_2012_rule_10_1] */
			/* coverity[misra_c_2012_rule_10_4] */
			modes->type |= DRM_MODE_TYPE_PREFERRED;
		}
		if (tccdrm_dsi_register_mode(connector, modes) < 0) {
			drm_mode_destroy(connector->dev, modes);
			continue;
		}

		if (mode_count <= (DRM_INT_MAX -1)) {
			mode_count++;
		}
	}
	return mode_count;
}

static void tccdrm_dsi_ch_select(const struct tccdrm_dsi_context *dsi_context, uint32_t mux_id)
{
	void __iomem *reg =
		/* coverity[misra_c_2012_rule_18_4] */
		(void __iomem *)dsi_context->dsi_dev.cfg_addr + 0x10;
	unsigned int val;

	if(dsi_context->dsi_dev.port == 0U){
		/* coverity[cert_int02_c] */
		/* coverity[cert_int31_c] */
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_10_4] */
		val = __raw_readl(reg) & ~(0x7 << 0x0);
		val |= mux_id << (0x0);
	} else {
		/* coverity[cert_int02_c] */
		/* coverity[cert_int31_c] */
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_10_4] */
		val = __raw_readl(reg) & ~(0x7 << 0x3);
		val |= mux_id << (0x3);
	}
 
	/* coverity[cert_int02_c] */
	/* coverity[cert_int31_c] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_12_2] */
	val &= ~(0x3 << 0x8);
	val |= (dsi_context->sdm_bypass << 0x9) | (dsi_context->trvc_bypass << 0x8);
 
	__raw_writel(val, reg);
}

static void
tccdrm_dsi_encoder_mode_set(struct drm_encoder *encoder,
			/* coverity[misra_c_2012_rule_8_13] */
			struct drm_crtc_state *drm_cstate,
			/* coverity[misra_c_2012_rule_8_13] */
			struct drm_connector_state *connector_state)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */	//
	/* coverity[misra_c_2012_rule_8_6] */	//
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_dsi_context *dev_context = (struct tccdrm_dsi_context *)encoder_to_context(encoder);

	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_21_2] */
	const struct tcc_crtc_state *tcc_cstate = to_tcc_crtc_state(drm_cstate);

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter connector_state is
	 * not used in the function.
	 */
	(void)connector_state;

	if (dev_context->enabled == (bool)false) {
		if (dev_context->panel != NULL) {
			(void)drm_panel_prepare(dev_context->panel);
		}

		tccdrm_dsi_ch_select(dev_context, tcc_cstate->lcdc_mux_select);
	}
}

/* coverity[HIS_metric] - HIS_CALLS */
static void tccdrm_dsi_encoder_enable(struct drm_encoder *encoder,
			 /* coverity[misra_c_2012_rule_8_13] */
			 struct drm_atomic_state *drm_astate)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */	//
	/* coverity[misra_c_2012_rule_8_6] */	//
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_dsi_context *dev_context = (struct tccdrm_dsi_context *)encoder_to_context(encoder);

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter drm_astate is not
	 * used in the function.
	 */
	(void)drm_astate;

	if (encoder->crtc == NULL) {
		DRM_DEV_INFO(dev_context->dev, "[INFO] encoder is not ready\r\n");
	} else {
		/* coverity[cert_arr39_c] */
		/* coverity[cert_dcl37_c] */
		/* coverity[misra_c_2012_rule_8_5] */	//
		/* coverity[misra_c_2012_rule_8_6] */	//
		/* coverity[misra_c_2012_rule_8_13] */
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_11_5] */
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_15_6] */
		/* coverity[misra_c_2012_rule_18_4] */
		/* coverity[misra_c_2012_rule_20_7] */
		/* coverity[misra_c_2012_rule_21_2] */
		const struct tcc_crtc_state *tcc_cstate = to_tcc_crtc_state(encoder->crtc->state);

		DRM_DEV_INFO(dev_context->dev,
			"[INFO] It is connectd to lcdc_num %u and lcdc_mux %u \r\n",
			tcc_cstate->lcdc_num, tcc_cstate->lcdc_mux_select);


		if(dev_context->enabled == (bool)false) {
			if (dev_context->panel != NULL) {
				(void)drm_panel_enable(dev_context->panel);
			}
			else {
				DRM_DEV_ERROR(dev_context->dev, "DRM Panel is null \r\n");
			}
			tcc_dsi_phy_init(&dev_context->dsi_dev);
			/* coverity[cert_dcl37_c] */
			/* coverity[misra_c_2012_rule_10_1] */
			/* coverity[misra_c_2012_rule_10_4] */
			/* coverity[misra_c_2012_rule_12_1] */
			/* coverity[misra_c_2012_rule_14_4] */
			/* coverity[misra_c_2012_rule_15_6] */
			mdelay(200);
			tcc_dsi_platform_init(&dev_context->dsi_dev, 0); //enable dsi driver

			dev_context->enabled = (bool)true;
		}
	}
}

static void tccdrm_dsi_encoder_disable(struct drm_encoder *encoder,
			  /* coverity[misra_c_2012_rule_8_13] */
			  struct drm_atomic_state *drm_astate, unsigned int stage)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */	//
	/* coverity[misra_c_2012_rule_8_6] */	//
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_dsi_context *dev_context = (struct tccdrm_dsi_context *)encoder_to_context(encoder);

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter drm_astate is not
	 * used in the function.
	 */
	(void)drm_astate;

	if(dev_context->enabled == (bool)true) {
		switch (stage) {
		case 1:
			if (dev_context->panel != NULL) {
				(void)drm_panel_disable(dev_context->panel);
				/* panel->funcs->disable(panel); */
			}
			tcc_dsi_platform_init(&dev_context->dsi_dev, 1); //disable dsi driver
			break;
		case 2:
			if (dev_context->panel != NULL) {
				mdelay(30);
				(void)drm_panel_unprepare(dev_context->panel);
				/* panel->funcs->unprepare(panel); */
			}
			dev_context->enabled = (bool)false;
			break;
		default:
			(void)pr_info("[INFO] Unknown stage\r\n");
			break;
		}
	}
}


static void tccdrm_dsi_disable_stage1(struct drm_encoder *encoder,
					      struct drm_atomic_state *drm_astate)
{
	tccdrm_dsi_encoder_disable(encoder, drm_astate, 1);
}

static void tccdrm_dsi_disable_stage2(struct drm_encoder *encoder)
{
	tccdrm_dsi_encoder_disable(encoder, NULL, 2);
}

/* coverity[misra_c_2012_rule_8_13] */
static int tccdrm_dsi_encoder_check(struct drm_encoder *encoder,
				      struct drm_crtc_state *drm_cstate,
				      /* coverity[misra_c_2012_rule_8_13] */
				      struct drm_connector_state *conn_state)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tcc_crtc_state *tcc_cstate = to_tcc_crtc_state(drm_cstate);

	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_dsi_context *dev_context = encoder_to_context(encoder);

	int ret = 0;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter conn_state is not
	 * used in the function.
	 */
	(void)conn_state;
	tcc_cstate->connector = &dev_context->connector;
	tcc_cstate->connector_type = DRM_MODE_CONNECTOR_DSI;

	return ret;
}

static const struct drm_encoder_helper_funcs tccdrm_dsi_encoder_helper_funcs = {
	.atomic_mode_set = tccdrm_dsi_encoder_mode_set,
	.atomic_enable = tccdrm_dsi_encoder_enable,
	.atomic_disable = tccdrm_dsi_disable_stage1,
	.atomic_check = tccdrm_dsi_encoder_check,
	.disable = tccdrm_dsi_disable_stage2,
};

static const struct drm_encoder_funcs tccdrm_dsi_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

/*
 * This function called in drm_helper_probe_single_connector_modes
 * return is mode_count
 */
static int tccdrm_dsi_connector_get_modes(struct drm_connector *connector)
{
	//const struct drm_display_mode *modes = NULL;
	struct tccdrm_dsi_context *dev_context;
	//bool internal_step3_ok = (bool)false;
	bool internal_ok = (bool)true;
	bool find_modes = (bool)false;

	int mode_count = 0;

	if (connector == NULL) {
		DRM_DEV_ERROR(NULL, "connector is NULL\r\n");
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		/* coverity[cert_arr39_c] */
		/* coverity[cert_dcl37_c] */
		/* coverity[misra_c_2012_rule_8_5] */	//
		/* coverity[misra_c_2012_rule_8_6] */	//
		/* coverity[misra_c_2012_rule_8_13] */
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_11_5] */
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_15_6] */
		/* coverity[misra_c_2012_rule_18_4] */
		/* coverity[misra_c_2012_rule_20_7] */
		/* coverity[misra_c_2012_rule_21_2] */
		dev_context = (struct tccdrm_dsi_context *)connector_to_context(connector);
		if (dev_context == NULL) {
			DRM_DEV_ERROR(NULL,
				"[ERROR] dev_context is NULL\r\n");
			internal_ok = (bool)false;
		}
	}

	/* step1: Check panels */
	if (internal_ok) {
		if (dev_context->panel != NULL) {
			mode_count = drm_panel_get_modes(dev_context->panel, connector);
			if (mode_count > 0) {
				find_modes = (bool)true;
			} else {
				DRM_DEV_INFO(dev_context->dev,
					     "[WARN] It has a panel node, but there is no detailed-display-timing information in panel node.\r\n");
			}
		}
	}
	/* step2: Check detailed timing from device tree */
	if (internal_ok && !find_modes) {
		mode_count = tccdrm_dsi_get_dev_node_modes(dev_context);
		if (mode_count == 0) {
			DRM_DEV_ERROR(dev_context->dev,
				     "There is no detailed-display-timing information in device node.\r\n");
		}
	}
	return mode_count;
}

static struct drm_encoder *tccdrm_dsi_best_single_encoder(
	struct drm_connector *connector)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */	//
	/* coverity[misra_c_2012_rule_8_6] */	//
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_dsi_context *ctx = connector_to_context(connector);

	return &ctx->encoder;
}

static const struct drm_connector_helper_funcs tccdrm_dsi_connector_helper_funcs = {
	.get_modes = tccdrm_dsi_connector_get_modes,
	.best_encoder = tccdrm_dsi_best_single_encoder,
};

static int tccdrm_dsi_check_possible_crtcs(const struct device *pdev, struct drm_device *pdrmdev, struct drm_encoder *pencoder)
{
	int ret = -1;

	//get drm crtc from remote node
	pencoder->possible_crtcs =
		drm_of_find_possible_crtcs(pdrmdev, pdev->of_node);

	if (pencoder->possible_crtcs > 0U) {
		ret = 0;
	}
	else {
		/*
		* Even if possible_crtcs is 0, no error is returned.
		* The tccdrm determines whether crtc is used or not
		* through device tree and kconfig settings.
		* If this driver returns an error when the crtc is set
		* to enabled in the device tree and disabled in
		* Kconfig, the tccdrm will be failed to bind.
		* Therefore, it does not return an error to operate
		* normally even in this exception condition.
		*/
		DRM_DEV_INFO(pdev,
				"This encoder will also be deactivated because the crtc connected with this encoder is probably in a deactivated state.\r\n");
	}

	return ret;
}

static int tccdrm_dsi_set_encoder(const struct device *pdev, struct drm_device *pdrmdev, struct drm_encoder *pencoder)
{
	int ret;

	//get drm crtc from remote node
	ret = drm_encoder_init(pdrmdev,
				pencoder,
				&tccdrm_dsi_encoder_funcs,
				DRM_MODE_ENCODER_DSI, NULL);
	if (ret == 0) {
		drm_encoder_helper_add(pencoder, &tccdrm_dsi_encoder_helper_funcs);
	} else {
		DRM_DEV_ERROR(pdev, "failed to initialize encoder with drm\n");

		drm_encoder_cleanup(pencoder);
	}

	return ret;
}

static int tccdrm_dsi_set_connector(struct tccdrm_dsi_context *dev_context)
{
	struct drm_connector *connector = &dev_context->connector;
	struct drm_encoder *encoder = &dev_context->encoder;
	bool internal_ok = (bool)true;
	int ret = 0;

	ret = drm_connector_init(encoder->dev,
				 connector, &tccdrm_dsi_connector_funcs,
				 DRM_MODE_CONNECTOR_DSI);
	if (ret < 0) {
		DRM_DEV_ERROR(dev_context->dev,
			      "failed to initialize connector with drm\n");
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		drm_connector_helper_add(connector,
					 &tccdrm_dsi_connector_helper_funcs);

		(void)drm_connector_attach_encoder(connector, encoder);
	}

	return ret;
}

/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 1>
 * HIS metric violation (HIS_CCM)
 *  DR <case 1>
 */
static void tccdrm_dsi_parse_dt(struct tccdrm_dsi_context *dev_context)
{
	const struct device *dev = dev_context->dev;
	const struct device_node *dn = dev->of_node;
	struct device_node *np;

	np = of_get_child_by_name(dn, "display-timings");
	if (np != NULL) {
		of_node_put(np);

		dev_context->timings = of_get_display_timings(dn);
		if (dev_context->timings == NULL) {
			DRM_DEV_INFO(dev,
				     "[WARN] failed to of_get_display_timings\n");
		}
	} else {
		DRM_DEV_DEBUG(dev,
			      "[DEBUG] cannot find display-timings node\n");
	}
}

/* HIS CALLS, HIS_CCM */
/* coverity[HIS_metric] */
static int tccdrm_dsi_parse_panel_dt(struct platform_device *pDev, struct tccdrm_dsi_context *dev_context)
{
	struct device_node *dn = dev_context->dev->of_node;
	struct device_node *np;
	struct clk *disp_peri_clk;
	const struct resource *pstresource;
	void __iomem *pioaddr;
	struct mipi_dsi_dev *dsi_info = &dev_context->dsi_dev;
	int ret = 0;
	bool internal_ok = true;

	dsi_info->core_addr = NULL;
	dsi_info->cfg_addr = NULL;

	if (of_property_read_u32_index(dn, "dsi-port", 0, &dsi_info->port) < 0) {
		dev_err(dev_context->dev,
			"[ERROR][%s] %s failed to get dsi-port property\n",
			LOG_TAG, __func__);
		ret = -ENODEV;
	}
	dev_info(dev_context->dev, "port %d \n", dsi_info->port);

	if( pDev != NULL ) {
		/* Get Resource DSI Core */
		pstresource = platform_get_resource(pDev, IORESOURCE_MEM, dsi_info->port);
		if (pstresource == NULL) {
			dev_err(dev_context->dev,
					"[ERROR][%s]can't get %u device resource\n", 
						LOG_TAG, dsi_info->port);
			ret = -ENODEV;
			internal_ok = false;
		}

		if( internal_ok ) {
			/* coverity[cert_exp34_c] */
			pioaddr = devm_ioremap(&pDev->dev, pstresource->start, (pstresource->end - pstresource->start));
			if (pioaddr == NULL) {
				dev_err(dev_context->dev,
						"[ERROR][%s]Failed to remap device resource\n", 
							LOG_TAG);
				ret = -ENODEV;
				internal_ok = false;
			}
			else {
				dsi_info->core_addr = pioaddr;
				/* coverity[misra_c_2012_rule_18_4] */
				dsi_info->phy_addr =( pioaddr + DSI_PHY_OFFSET);
			}
		}

		if( internal_ok ) {
			/* Get Resouces CAM CFG */
			pstresource = platform_get_resource(pDev, IORESOURCE_MEM, 2);
			if (pstresource == NULL) {
				dev_err(dev_context->dev,
						"[ERROR][%s]can't get CAM CFG device resource\n", 
							LOG_TAG);
				ret = -ENODEV;
				internal_ok = false;
			}
		}

		if( internal_ok ) {
			pioaddr = devm_ioremap(&pDev->dev, pstresource->start, (pstresource->end - pstresource->start));
			if (pioaddr == NULL) {
				dev_err(dev_context->dev,
						"[ERROR][%s]Failed to remap device resource\n", 
							LOG_TAG);
				ret = -ENODEV;
			}
			else {
				dsi_info->cfg_addr = pioaddr;
			}
		}
	}
	else { 
		ret = -ENODEV;
	}


	if (ret == 0) {
		if (of_property_read_u32_index(dn, "auto-mode", 0, &dsi_info->automode) < 0) {
			dev_err(dev_context->dev,
				"[ERROR][%s] %s failed to get automode property\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
		}
	}
	dev_info(dev_context->dev, "automode %d \n", dsi_info->automode);

	if (ret == 0) {
		if (of_property_read_u32_index(dn, "no_of_lane", 0, &dsi_info->phy_cfg.phy_lanes) < 0) {
			dev_err(dev_context->dev,
				"[ERROR][%s] %s failed to get no_of_lane property\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
		}
	}
	dev_info(dev_context->dev, "lane %d \n", dsi_info->phy_cfg.phy_lanes);

	if (ret == 0) {
		if (of_property_read_u32_index(dn, "dsi-sdm-bypass", 0, &dev_context->sdm_bypass) < 0) {
			dev_err(dev_context->dev,
				"[ERROR][%s] %s failed to get dsi-sdm-bypass property\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
		}
	}
	dev_info(dev_context->dev, "dsi-sdm-bypass %d \n", dev_context->sdm_bypass);

	if (ret == 0) {
		if (of_property_read_u32_index(dn, "dsi-trvc-bypass", 0, &dev_context->trvc_bypass) < 0) {
			dev_err(dev_context->dev,
				"[ERROR][%s] %s failed to get dsi-trvc-bypass property\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
		}
	}
	dev_info(dev_context->dev, "trvc_bypass %d \n", dev_context->trvc_bypass);

	if (ret == 0) {
		if (of_property_read_u32_index(dn, "dsi-display-source", 0, &dev_context->display_ch) < 0) {
			dev_err(dev_context->dev,
				"[ERROR][%s] %s failed to get dsi-display-source property\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
		}
	}
	dev_info(dev_context->dev, "display_ch %d \n", dev_context->display_ch);

	switch(dev_context->display_ch) {
		case 0 :
			disp_peri_clk = of_clk_get_by_name(dn, "disp0-clk");
			break;
		case 1 :
			disp_peri_clk = of_clk_get_by_name(dn, "disp1-clk");
			break;
		case 2 :
			disp_peri_clk = of_clk_get_by_name(dn, "disp2-clk");
			break;
		case 3 :
			disp_peri_clk = of_clk_get_by_name(dn, "disp3-clk");
			break;
		case 4 :
			disp_peri_clk = of_clk_get_by_name(dn, "disp4-clk");
			break;
		default :
			disp_peri_clk = NULL;
			(void)pr_err(
					"[ERROR][DSI] error in %s: can not get ddc clock\n",
					__func__);
			break;
	}

	if(disp_peri_clk != NULL) {
		dev_context->disp_pclk = clk_get_rate(disp_peri_clk) / 1000U;
		dsi_info->pclk = dev_context->disp_pclk;
		dev_info(dev_context->dev, "dsip pclk %lu hz \n", dev_context->disp_pclk);
	}

	if( dev_context->disp_pclk < (UINT_MAX/24U) ) {
		dsi_info->data_rate = (((dev_context->disp_pclk) * 24U) / dsi_info->phy_cfg.phy_lanes);
		dsi_info->data_rate /= 1000U;
	} else {
		(void)pr_err("[DSI] ddisp_pclk range over\n");
	}

	if( (dsi_info->data_rate % 10U) != 0U ) {
		(void)pr_err("[DSI]Request D-PHY Data rate not supported!\n Please set as multiple of 10mbps\n");
		dsi_info->data_rate /= 10U;
		dsi_info->data_rate += 1U;
		dsi_info->data_rate *= 10U;
	}

	if((dsi_info->data_rate > 2500U) || (dsi_info->data_rate < 100U)) {
		(void)pr_err("D-PHY Bandwidth out of range\n");
	}

	dev_info(dev_context->dev, "data_rate %ld hz \n", dsi_info->data_rate);

	if (ret == 0) {
		np = of_get_child_by_name(dn, "display-timings");
		if (np != NULL) {
			of_node_put(np);

			dev_context->timings = of_get_display_timings(dn);
			if (dev_context->timings == NULL) {
				DRM_DEV_INFO(dev_context->dev,
						"[WARN] failed to of_get_display_timings\n");
			}
		} else {
			dev_err(dev_context->dev,
				"[DEBUG][%s] %s cannot find display-timings node\n",
				LOG_TAG, __func__);
		}
	}

	return ret;
}

/* coverity[HIS_metric] - HIS_CALLS */
static int tccdrm_dsi_parse_panel(struct tccdrm_dsi_context *dev_context, struct platform_device *plat_dev)
{
	const struct device *dev = dev_context->dev;
	const struct device_node *np = dev_context->dev->of_node;
	struct drm_panel *p_panel = NULL;
	int ret = 0;
	const struct device_node *remote = NULL;

	/*
	* of_graph_get_remote_node() produces a noisy error message if port
	* node isn't found and the absence of the port is a legit case here,
	* so at first we silently check whether graph presents in the
	* device-tree node.
	*/
	if (of_graph_is_present(np)) {
		remote = of_graph_get_remote_node(np, 1, 0);
		if (remote != NULL) {
			p_panel = of_drm_find_panel(remote);
			if (IS_ERR(p_panel)) {
				/* coverity[misra_c_2012_rule_10_3] */
				ret = PTR_ERR(p_panel);
				if (ret == -EPROBE_DEFER) {
					DRM_DEV_INFO(dev, "[INFO] DRM DSI panel is not ready\r\n");
				}

				p_panel = NULL;
			}

			dev_context->panel = p_panel;

			/* find a dsi dt */
			ret = tccdrm_dsi_parse_panel_dt(plat_dev, dev_context);
		} else {
			DRM_DEV_ERROR(dev, "remote is null\r\n");
			ret = -ENODEV;
		}
	} else {
		ret = -ENODEV;
	}

	if(ret < 0) {
		if(dev_context->dsi_dev.core_addr == NULL) {
			iounmap(dev_context->dsi_dev.core_addr);
		}
		if(dev_context->dsi_dev.cfg_addr == NULL) {
			iounmap(dev_context->dsi_dev.cfg_addr);
		}
	}

	return ret;
}

/* coverity[misra_c_2012_rule_8_13] */
static int tccdrm_dsi_bind(struct device *dev,
		      /* coverity[misra_c_2012_rule_8_13] */
		      struct device *master_dev,
		      /* coverity[misra_c_2012_rule_8_13] */
		      void *data)
{
	struct tccdrm_dsi_context *dev_context;
	/* coverity[misra_c_2012_rule_11_5] */
	struct drm_device *drm_dev = (struct drm_device *)data;
	// bool internal_ok = (bool)true;
	struct drm_encoder *encoder;
	int ret = -ENOMEM;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter master_dev is not
	 * used in the function.
	 */
	(void)master_dev;

	/* coverity[misra_c_2012_rule_11_5] */
	dev_context = dev_get_drvdata(dev);
	if (dev_context != NULL) {
		encoder = &dev_context->encoder;
		if(tccdrm_dsi_check_possible_crtcs(dev, drm_dev, encoder) == 0) {
			ret = tccdrm_dsi_set_encoder(dev, drm_dev, encoder);
			if( ret == 0) {
				ret = tccdrm_dsi_set_connector(dev_context);
			}

			if (ret == 0) {
				//case for dsi encoder has a possible crtc
				dev_context->enabled = (bool)true;
				dev_context->binded = (bool)true;
			}
		}
		else {
			//case for dsi encoder has a NO possible crtc
			ret = 0;
		}
	}

	return ret;

}

/* coverity[misra_c_2012_rule_8_13] */
static void tccdrm_dsi_unbind(struct device *dev,
			 /* coverity[misra_c_2012_rule_8_13] */
			 struct device *master_dev,
			 /* coverity[misra_c_2012_rule_8_13] */
			 void *data)
{
	/* coverity[misra_c_2012_rule_11_5] */
	struct tccdrm_dsi_context *dev_context = dev_get_drvdata(dev);
	struct drm_connector *connector = &dev_context->connector;
	struct drm_encoder *encoder = &dev_context->encoder;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter master_dev is not
	 * used in the function.
	 */
	(void)master_dev;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter data is not
	 * used in the function.
	 */
	(void)data;

	if (dev_context->binded) {
		if (encoder->funcs->destroy != NULL) {
			encoder->funcs->destroy(encoder);
		}
		if (connector->funcs->destroy != NULL) {
			connector->funcs->destroy(connector);
		}
	}

	dev_context->binded = (bool)false;
}

static const struct component_ops tccdrm_dsi_component_ops = {
	.bind = tccdrm_dsi_bind,
	.unbind = tccdrm_dsi_unbind,
};


static int tccdrm_dsi_probe(struct platform_device *plat_dev)
{
	struct tccdrm_dsi_context *dev_context;
	struct device *dev = &plat_dev->dev;
	int ret = 0;

	/* coverity[misra_c_2012_rule_10_8] */
	/* coverity[misra_c_2012_rule_11_5] */
	dev_context = devm_kzalloc(dev, sizeof(*dev_context), GFP_KERNEL);
	if (dev_context != NULL) {
		dev_context->dev = dev;
		dev_context->timings = NULL;

		tccdrm_dsi_parse_dt(dev_context);

		ret = tccdrm_dsi_parse_panel(dev_context, plat_dev);
		if(ret == 0){
			(void)DRM_INFO("Initialized %s %d.%d.%d %s\r\n",
					plat_dev->name,
					DRIVER_MAJOR,
					DRIVER_MINOR,
					DRIVER_PATCH,
					DRIVER_DATE);

			platform_set_drvdata(plat_dev, dev_context);

			ret = component_add(dev, &tccdrm_dsi_component_ops);
		}
	}

	return ret;
}

static int tccdrm_dsi_remove(struct platform_device *plat_dev)
{
	/* coverity[misra_c_2012_rule_11_5] */
	const struct tccdrm_dsi_context *dev_context = (const struct tccdrm_dsi_context *)platform_get_drvdata(plat_dev);
	struct device *dev = &plat_dev->dev;

	component_del(dev, &tccdrm_dsi_component_ops);

	if(dev_context->dsi_dev.core_addr == NULL) {
		iounmap(dev_context->dsi_dev.core_addr);
	}
	if(dev_context->dsi_dev.cfg_addr == NULL) {
		iounmap(dev_context->dsi_dev.cfg_addr);
	}

	devm_kfree(dev, dev_context);

	return 0;
}

static const struct of_device_id tccdrm_dsi_dt_match[] = {
	{
		.compatible = "telechips,drm-dsi",
	},
	{
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, tccdrm_dsi_dt_match);


//TODO: need to check, using pm in platform driver
struct platform_driver tccdrm_dsi_driver = {
	.probe		= tccdrm_dsi_probe,
	.remove		= tccdrm_dsi_remove,
	.driver		= {
		.name	= "tccdrm-dsi",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tccdrm_dsi_dt_match),
	},
};

/* coverity[misra_c_2012_rule_8_3] */
/* coverity[misra_c_2012_rule_8_5] */
/* coverity[misra_c_2012_rule_8_6] */
/* coverity[misra_c_2012_rule_8_11] */
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
EXPORT_SYMBOL(tccdrm_dsi_driver);

/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_21_2] */
MODULE_DESCRIPTION("Telechips DRM DSI");

/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_21_2] */
MODULE_LICENSE("GPL");

/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_5_9] */ //K5.4
/* coverity[misra_c_2012_rule_21_2] */ //K5.4
MODULE_VERSION(__stringify(DRIVER_MAJOR) "."
               __stringify(DRIVER_MINOR) "."
               __stringify(DRIVER_PATCH));

