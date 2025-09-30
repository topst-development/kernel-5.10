// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Telechips DRM Dummy Device Driver
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

#include <linux/of_graph.h>
#include <linux/regulator/consumer.h>
#include <linux/fs.h>
#include <linux/component.h>
#include <linux/tcc_math.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#include <video/of_videomode.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>
#include <telechips_drm_drv.h>
#include <telechips_drm_lvds.h>
#include <telechips_drm_types.h>
#include <video/telechips/vioc_lvds.h>

#define TCC_LVDS_OUTPUT_VESA24 0
#define TCC_LVDS_OUTPUT_JEIDA24 1
#define TCC_LVDS_OUTPUT_MAX 2

#define LOG_TAG "DRMLVDS"

#define DRIVER_DATE "20240514"
#define DRIVER_MAJOR 2
#define DRIVER_MINOR 2
#define DRIVER_PATCH 3

static unsigned int
lvds_outformat[TCC_LVDS_OUTPUT_MAX][TXOUT_MAX_LINE][TXOUT_DATA_PER_LINE] = {
	/* LVDS vesa-24 format */
	{
		{
			TXOUT_G_D(0), TXOUT_R_D(5), TXOUT_R_D(4), TXOUT_R_D(3),
			TXOUT_R_D(2), TXOUT_R_D(1), TXOUT_R_D(0)
		},
		{
			TXOUT_B_D(1), TXOUT_B_D(0), TXOUT_G_D(5), TXOUT_G_D(4),
			TXOUT_G_D(3), TXOUT_G_D(2), TXOUT_G_D(1)
		},
		{
			TXOUT_DE, TXOUT_VS, TXOUT_HS, TXOUT_B_D(5),
			TXOUT_B_D(4), TXOUT_B_D(3), TXOUT_B_D(2)
		},
		{
			TXOUT_DUMMY, TXOUT_B_D(7), TXOUT_B_D(6),
			TXOUT_G_D(7), TXOUT_G_D(6), TXOUT_R_D(7), TXOUT_R_D(6)
		}
	},
	/* LVDS jeida-24 format , not implemented*/
};

struct tccdrm_lvds_context {
	struct drm_encoder encoder;
	struct drm_connector connector;
	struct device *dev;

	struct drm_panel *panel;
	struct display_timings *timings;
	unsigned int bus_format;
	lvds_hw_info_t tcc_lvds_hw;
	bool lvds_hw_registred;
	bool lcdc_mux_bypass;
	bool binded;
	int enabled;
};

#define connector_to_context(x) \
		container_of((x), struct tccdrm_lvds_context, connector)
#define encoder_to_context(x) \
		container_of((x), struct tccdrm_lvds_context, encoder)


/* coverity[misra_c_2012_rule_8_13] */
static enum drm_connector_status tccdrm_lvds_detect(struct drm_connector *connector,
						     bool force)
{
	enum drm_connector_status connector_status =
				connector_status_connected;

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

static void tccdrm_lvds_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static const struct drm_connector_funcs tccdrm_lvds_connector_funcs = {
	.detect = tccdrm_lvds_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = tccdrm_lvds_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int tccdrm_lvds_register_mode(struct drm_connector *connector,
				      struct drm_display_mode *in_mode)
{
	const struct tccdrm_lvds_context *dev_context;
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
		dev_context = (const struct tccdrm_lvds_context *)connector_to_context(connector);
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


static int tccdrm_lvds_get_dev_node_modes(struct tccdrm_lvds_context *dev_context)
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
		if (tccdrm_lvds_register_mode(connector, modes) < 0) {
			drm_mode_destroy(connector->dev, modes);
			continue;
		}

		if (mode_count <= (DRM_INT_MAX -1)) {
			mode_count++;
		}
	}
	return mode_count;
}

/*
 * This function called in drm_helper_probe_single_connector_modes
 * return is mode_count
 */
static int tccdrm_lvds_get_modes(struct drm_connector *connector)
{
	//const struct drm_display_mode *modes = NULL;
	struct tccdrm_lvds_context *dev_context;
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
		dev_context = (struct tccdrm_lvds_context *)connector_to_context(connector);
		if (dev_context == NULL) {
			DRM_DEV_ERROR(NULL,
				"[ERROR] dev_context is NULL\r\n");
			internal_ok = (bool)false;
		}
	}

	/* step1: Check panels */
	if (internal_ok) {
		if (dev_context->panel != NULL) {
			#if defined(CONFIG_REFCODE_PRE_K510)
			mode_count = drm_panel_get_modes(dev_context->panel);
			#else
			mode_count = drm_panel_get_modes(dev_context->panel, connector);
			#endif
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
		mode_count = tccdrm_lvds_get_dev_node_modes(dev_context);
		if (mode_count == 0) {
			DRM_DEV_ERROR(dev_context->dev,
				     "There is no detailed-display-timing information in device node.\r\n");
		}
	}
	return mode_count;
}

#if defined(CONFIG_REFCODE_PRE_K54)
#else
static struct drm_encoder *tccdrm_lvds_best_single_encoder(
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
	struct tccdrm_lvds_context *lvds_ctx = connector_to_context(connector);

	return &lvds_ctx->encoder;
}
#endif

static const struct drm_connector_helper_funcs tccdrm_lvds_connector_helper_funcs = {
	.get_modes = tccdrm_lvds_get_modes,
	#if defined(CONFIG_REFCODE_PRE_K54)
	.best_encoder = drm_atomic_helper_best_encoder,
	#else
	.best_encoder = tccdrm_lvds_best_single_encoder,
	#endif
};

static int tccdrm_lvds_set_connector(struct tccdrm_lvds_context *dev_context)
{
	struct drm_connector *connector = &dev_context->connector;
	struct drm_encoder *encoder = &dev_context->encoder;
	bool internal_ok = (bool)true;
	int ret = 0;

	ret = drm_connector_init(encoder->dev,
				 connector, &tccdrm_lvds_connector_funcs,
				 DRM_MODE_CONNECTOR_LVDS);
	if (ret < 0) {
		DRM_DEV_ERROR(dev_context->dev,
			      "failed to initialize connector with drm\n");
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		drm_connector_helper_add(connector,
					 &tccdrm_lvds_connector_helper_funcs);
		#if defined(CONFIG_REFCODE_PRE_K54)
		(void)drm_mode_connector_attach_encoder(connector, encoder);
		#else
		(void)drm_connector_attach_encoder(connector, encoder);
		#endif
		#if defined(CONFIG_REFCODE_PRE_K510)
		if ((dev_context->panel != NULL) &&
		    (dev_context->panel->connector == NULL)) {
			(void)drm_panel_attach(dev_context->panel,
			&dev_context->connector);
		}
		#endif
	}

	return ret;
}

static void tccdrm_lvds_hw_register(struct tccdrm_lvds_context *dev_context,
				    const struct tcc_crtc_state *tcc_cstate)
{
	const lvds_hw_info_t *lvds_ret;

	if (!dev_context->lvds_hw_registred) {
		dev_context->tcc_lvds_hw.lcdc_mux_id = tcc_cstate->lcdc_mux_select;
		dev_context->lcdc_mux_bypass =
			(tcc_cstate->lcdc_mux_bypass != 0U) ? (bool)true: (bool)false;

		DRM_DEV_INFO(dev_context->dev,
			     "LVDS connected to lcdc_mux %u bypass %u\r\n",
			     dev_context->tcc_lvds_hw.lcdc_mux_id,
			     dev_context->lcdc_mux_bypass);

		lvds_ret = lvds_register_hw_info(&dev_context->tcc_lvds_hw,
						dev_context->tcc_lvds_hw.lvds_type,
						dev_context->tcc_lvds_hw.port_main,
						dev_context->tcc_lvds_hw.port_sub,
						dev_context->tcc_lvds_hw.p_clk,
						dev_context->tcc_lvds_hw.lcdc_mux_id,
						(unsigned int)dev_context->lcdc_mux_bypass,
						dev_context->timings->timings[
						dev_context->timings->native_mode]->hactive.typ);

		if (lvds_ret != NULL) {
			DRM_DEV_INFO(dev_context->dev, "LVDS registred\r\n");
			dev_context->lvds_hw_registred = (bool)true;
		} else {
			DRM_DEV_ERROR(dev_context->dev, "invalid lcdc_hw ptr\r\n");
		}
	}
}

static void tccdrm_lvds_hw_setting(struct tccdrm_lvds_context *dev_context)
{
	unsigned int lcdc_mux_bypass = 0U;
	unsigned int ts_mux_id = 0U;

	if (dev_context->lvds_hw_registred) {
		if (dev_context->tcc_lvds_hw.lvds_type ==
		(unsigned int)PANEL_LVDS_DUAL) {
			ts_mux_id = (unsigned int)TS_MUX_IDX0;
		} else {
			if (dev_context->tcc_lvds_hw.ts_mux_id >= 0) {
				ts_mux_id = (unsigned int)dev_context->tcc_lvds_hw.ts_mux_id;
			}
		}

		LVDS_WRAP_ResetPHY(ts_mux_id, 1);
		lvds_splitter_init(&dev_context->tcc_lvds_hw);
		DRM_DEV_INFO(dev_context->dev,
			     "LVDS connected to mux_%u bypass=%d\r\n",
			     dev_context->tcc_lvds_hw.lcdc_mux_id,
			     dev_context->lcdc_mux_bypass ? 1 : 0);
		if (dev_context->lcdc_mux_bypass) {
			lcdc_mux_bypass = 1U;
		}
		LVDS_WRAP_SM_Bypass(dev_context->tcc_lvds_hw.lcdc_mux_id,
				    lcdc_mux_bypass);
	} else {
		DRM_DEV_ERROR(dev_context->dev, "LVDS is not registered yet\r\n");
	}
}

static void
tccdrm_lvds_mode_set(struct drm_encoder *encoder,
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
	struct tccdrm_lvds_context *dev_context = encoder_to_context(encoder);

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
	const struct tcc_crtc_state *tcc_cstate = (const struct tcc_crtc_state *)to_tcc_crtc_state(drm_cstate);

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter connector_state is
	 * not used in the function.
	 */
	(void)connector_state;

	if ((dev_context->panel != NULL) && (dev_context->enabled != 1)) {
		(void)drm_panel_prepare(dev_context->panel);
		/*  panel->funcs->prepare(panel) */
	}

		tccdrm_lvds_hw_register(dev_context, tcc_cstate);

	if ((dev_context->lvds_hw_registred) && (dev_context->enabled != 1)) {
		tccdrm_lvds_hw_setting(dev_context);
	}
}

static void tccdrm_lvds_enable(struct drm_encoder *encoder,
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
	struct tccdrm_lvds_context *dev_context = encoder_to_context(encoder);

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

		if (dev_context->lvds_hw_registred) {
			if (dev_context->enabled != 1) {
				lvds_phy_init(&dev_context->tcc_lvds_hw);
			} else {
				DRM_DEV_INFO(dev_context->dev,
				      "LVDS is already enabled\r\n");
			}
		} else {
			DRM_DEV_ERROR(dev_context->dev,
				      "LVDS is not registered yet\r\n");
		}

		if ((dev_context->panel != NULL) && (dev_context->enabled != 1)) {
			(void)drm_panel_enable(dev_context->panel);
			/* panel->funcs->enable(panel); */
		}

		dev_context->enabled = 1;
	}
}

static void tccdrm_lvds_disable(struct drm_encoder *encoder,
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
	struct tccdrm_lvds_context *dev_context = (struct tccdrm_lvds_context *)encoder_to_context(encoder);
	unsigned int ts_mux_id = 0;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter drm_astate is not
	 * used in the function.
	 */
	(void)drm_astate;

	if (dev_context->lvds_hw_registred) {
		switch (stage) {
		case 1:
			if (dev_context->panel != NULL) {
				(void)drm_panel_disable(dev_context->panel);
				/* panel->funcs->disable(panel); */
			}
			if (dev_context->tcc_lvds_hw.lvds_type ==
				(unsigned int)PANEL_LVDS_DUAL) {
				ts_mux_id = (unsigned int)TS_MUX_IDX0;
			} else {
				if (dev_context->tcc_lvds_hw.ts_mux_id >= 0) {
					ts_mux_id =
						(unsigned int)dev_context->
						tcc_lvds_hw.ts_mux_id;
				}
			}
			LVDS_WRAP_ResetPHY(ts_mux_id, 1);
			break;
		case 2:
			if (dev_context->panel != NULL) {
				mdelay(30);
				(void)drm_panel_unprepare(dev_context->panel);
				/* panel->funcs->unprepare(panel); */
			}
			break;
		default:
			(void)pr_info("[INFO] Unknown stage\r\n");
			break;
		}
	} else {
		DRM_DEV_ERROR(dev_context->dev, "LVDS is not registered yet\r\n");
	}
	dev_context->enabled = 0;
}


static void tccdrm_lvds_disable_stage1(struct drm_encoder *encoder,
					      struct drm_atomic_state *drm_astate)
{
	tccdrm_lvds_disable(encoder, drm_astate, 1);
}

static void tccdrm_lvds_disable_stage2(struct drm_encoder *encoder)
{
	tccdrm_lvds_disable(encoder, NULL, 2);
}

/* coverity[misra_c_2012_rule_8_13] */
static int tccdrm_lvds_check(struct drm_encoder *encoder,
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
	struct tccdrm_lvds_context *dev_context = encoder_to_context(encoder);

	int ret = 0;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter conn_state is not
	 * used in the function.
	 */
	(void)conn_state;
	tcc_cstate->connector = &dev_context->connector;
	tcc_cstate->connector_type = DRM_MODE_CONNECTOR_LVDS;

	return ret;
}

static const struct drm_encoder_helper_funcs tccdrm_lvds_encoder_helper_funcs = {
	.atomic_mode_set = tccdrm_lvds_mode_set,
	.atomic_enable = tccdrm_lvds_enable,
	.atomic_disable = tccdrm_lvds_disable_stage1,
	.atomic_check = tccdrm_lvds_check,
	.disable = tccdrm_lvds_disable_stage2,
};

static const struct drm_encoder_funcs tccdrm_lvds_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 1>
 * HIS metric violation (HIS_CCM)
 *  DR <case 1>
 */
static int tccdrm_lvds_parse_dt(struct tccdrm_lvds_context *dev_context)
{
	const struct device *dev = dev_context->dev;
	const struct device_node *dn = dev->of_node;
	struct device_node *np;
	const char *mapping;
	unsigned int lane_idx;
	unsigned int lvds_format;
	int ret = 0;

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
	//todo LVDS timing function
	ret = of_property_read_string(dn, "data-mapping", &mapping);
	if (ret < 0) {
		DRM_DEV_ERROR(
			dev,
			"[ERROR][%s] failed to get data-mapping property\r\n",
			__func__);
		ret = -ENODEV;
	}

	if (ret == 0) {
		if (strcmp(mapping, "vesa-24") == 0) {
			//dev_context->bus_format = MEDIA_BUS_FMT_RGB888_1X7X4_SPWG;
			lvds_format = TCC_LVDS_OUTPUT_VESA24;
		} else {
			/* need to developed if vesa-16 and jeida-24 are needed. */
			DRM_DEV_ERROR(
				dev,
				"[ERROR][%s] invalid or missing data-mapping property\r\n",
				__func__);
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		if (
		    of_property_read_u32_index(dn, "mode", 0,
					       &dev_context->tcc_lvds_hw.lvds_type) < 0) {
			dev_err(
				dev,
				"[ERROR][%s] failed to get mode property\r\n",
				__func__);
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		DRM_DEV_INFO(dev, "%s lvds - port type: %d\n",
			 __func__, dev_context->tcc_lvds_hw.lvds_type);

		if (
		    of_property_read_u32_index(dn, "phy-ports", 0,
					       &dev_context->tcc_lvds_hw.port_main) < 0) {
			dev_err(
				dev,
				"[ERROR][%s] failed to get phy-ports property\r\n",
				 __func__);
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		DRM_DEV_INFO(dev, "%s lvds - main_port: %d\n",
				__func__, dev_context->tcc_lvds_hw.port_main);

		if (dev_context->tcc_lvds_hw.lvds_type == (unsigned int)PANEL_LVDS_DUAL) {
			if (
			    of_property_read_u32_index(dn, "phy-ports", 1,
						&dev_context->tcc_lvds_hw.port_sub) < 0) {
				dev_err(
					dev,
					"[ERROR][%s] failed to get phy-ports for sub property\r\n",
					__func__);
				ret = -ENODEV;
			}
			DRM_DEV_INFO(dev,
				 "%s lvds - sub_port: %d\n", __func__,
				 dev_context->tcc_lvds_hw.port_sub);
		} else if (dev_context->tcc_lvds_hw.lvds_type ==
					(unsigned int)PANEL_LVDS_SINGLE) {
			dev_context->tcc_lvds_hw.port_sub = LVDS_PHY_PORT_MAX;
		} else {
			dev_err(
				dev,
				"[ERROR][%s] wrong port number\r\n",
				__func__);
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		for (lane_idx = 0 ; lane_idx < (unsigned int)LVDS_PHY_LANE_MAX;
								lane_idx++) {
			if (
			    of_property_read_u32_index(dn, "lane-main",
					lane_idx,
					&dev_context->tcc_lvds_hw.lane_main[lane_idx]) < 0) {
				dev_err(dev,
					"[ERROR][%s] failed to get lane-main property\r\n",
					__func__);
				ret = -ENODEV;
			}
			DRM_DEV_INFO(dev,
				 "%s lvds - lane main[%d] : %d\n", __func__,
				 lane_idx,
				 dev_context->tcc_lvds_hw.lane_main[lane_idx]);
		}
	}

	if (ret == 0) {
		if(dev_context->tcc_lvds_hw.lvds_type == (unsigned int)PANEL_LVDS_DUAL){
			for (lane_idx = 0 ; lane_idx < (unsigned int)LVDS_PHY_LANE_MAX;
									lane_idx++) {
				if (
				of_property_read_u32_index(dn, "lane-sub",
						lane_idx,
						&dev_context->tcc_lvds_hw.lane_sub[lane_idx]) < 0) {
					dev_err(dev,
						"[ERROR][%s] failed to get lane-sub property\r\n",
						__func__);
					ret = -ENODEV;
				}
				DRM_DEV_INFO(dev,
					"%s lvds - lane sub[%d] : %d\n", __func__,
					lane_idx,
					dev_context->tcc_lvds_hw.lane_sub[lane_idx]);
			}
		}
	}

	if (ret == 0) {
		if (of_property_read_u32_index(dn, "vcm", 0, &dev_context->tcc_lvds_hw.vcm) < 0) {
			dev_err(
				dev,
				"[ERROR][%s] failed to get vcm property\r\n",
				__func__);
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		DRM_DEV_INFO(dev, "%s lvds - vcm: %d\n", __func__, dev_context->tcc_lvds_hw.vcm);

		if (of_property_read_u32_index(dn, "vsw", 0, &dev_context->tcc_lvds_hw.vsw) < 0) {
			dev_err(
				dev,
				"[ERROR][%s] failed to get vsw property\r\n",
				__func__);
			ret = -ENODEV;
		}
	}

	DRM_DEV_INFO(dev, "%s lvds - vsw: %d\n", __func__, dev_context->tcc_lvds_hw.vsw);

	if (ret == 0) {
		/* lvds hw_info registration */
		(void)memcpy(dev_context->tcc_lvds_hw.txout_main,
			     lvds_outformat[lvds_format],
		       sizeof(dev_context->tcc_lvds_hw.txout_main));
		if (dev_context->tcc_lvds_hw.lvds_type == (unsigned int)PANEL_LVDS_DUAL) {
			(void)memcpy(dev_context->tcc_lvds_hw.txout_sub,
			       lvds_outformat[lvds_format],
			       sizeof(dev_context->tcc_lvds_hw.txout_sub));
		}

		dev_context->tcc_lvds_hw.p_clk = (unsigned int)dev_context->timings->timings[
				dev_context->timings->native_mode]->pixelclock.typ;
	}

	return ret;
}

static void tccdrm_lvds_parse_panel(struct tccdrm_lvds_context *dev_context)
{
	const struct device *dev = dev_context->dev;
	int ret = 0;

	/* find panel from port reg <1> */
	ret = drm_of_find_panel_or_bridge(dev->of_node, 1, -1,
					  &dev_context->panel, NULL);

	if (ret < 0) {
		DRM_DEV_INFO(dev, "[INFO] has no DRM lvds panel\r\n");
	} else {
		if (dev_context->panel != NULL) {
			DRM_DEV_INFO(dev, "[INFO] has DRM lvds panel\r\n");
		}
	}
}

/* coverity[misra_c_2012_rule_8_13] */
static int tccdrm_lvds_bind(struct device *dev,
		      /* coverity[misra_c_2012_rule_8_13] */
		      struct device *master_dev,
		      /* coverity[misra_c_2012_rule_8_13] */
		      void *data)
{
	struct tccdrm_lvds_context *dev_context;
	/* coverity[misra_c_2012_rule_11_5] */
	struct drm_device *drm_dev = (struct drm_device *)data;
	bool cleanup_encoder = (bool)false;
	bool internal_ok = (bool)true;
	struct drm_encoder *encoder;
	int ret = 0;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter master_dev is not
	 * used in the function.
	 */
	(void)master_dev;

	/* coverity[misra_c_2012_rule_11_5] */
	dev_context = dev_get_drvdata(dev);
	if (dev_context == NULL) {
		internal_ok = (bool)false;
		ret = -ENOMEM;
	}
	if (internal_ok) {
		encoder = &dev_context->encoder;

		encoder->possible_crtcs =
			drm_of_find_possible_crtcs(drm_dev, dev->of_node);
		if (encoder->possible_crtcs == 0U) {
			internal_ok = (bool)false;
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
			DRM_DEV_INFO(dev,
				     "This encoder will also be deactivated because the crtc connected with this encoder is probably in a deactivated state.\r\n");
		}
	}
	if (internal_ok) {
		ret = drm_encoder_init(drm_dev,
				       encoder,
				       &tccdrm_lvds_encoder_funcs,
				       DRM_MODE_ENCODER_LVDS, NULL);
		if (ret != 0) {
			DRM_DEV_ERROR(dev, "failed to initialize encoder with drm\n");
			internal_ok = (bool)false;
		} else {
			cleanup_encoder = (bool)true;
		}
	}
	if (internal_ok) {
		drm_encoder_helper_add(encoder, &tccdrm_lvds_encoder_helper_funcs);

		ret = tccdrm_lvds_set_connector(dev_context);
		if (ret < 0) {
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		dev_context->binded = (bool)true;
	} else {
		if (cleanup_encoder) {
			drm_encoder_cleanup(encoder);
		}
	}
	return ret;
}

/* coverity[misra_c_2012_rule_8_13] */
static void tccdrm_lvds_unbind(struct device *dev,
			 /* coverity[misra_c_2012_rule_8_13] */
			 struct device *master_dev,
			 /* coverity[misra_c_2012_rule_8_13] */
			 void *data)
{
	/* coverity[misra_c_2012_rule_11_5] */
	struct tccdrm_lvds_context *dev_context = dev_get_drvdata(dev);
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

	#if defined(CONFIG_REFCODE_PRE_K510)
	if (dev_context->panel != NULL) {
		(void)drm_panel_detach(dev_context->panel);
	}
	#endif

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

static const struct component_ops tccdrm_lvds_component_ops = {
	.bind = tccdrm_lvds_bind,
	.unbind = tccdrm_lvds_unbind,
};

static int tccdrm_lvds_probe(struct platform_device *plat_dev)
{
	struct tccdrm_lvds_context *dev_context;
	struct device *dev = &plat_dev->dev;
	bool internal_ok = (bool)true;
	int ret = 0;
	unsigned int lvds_status = 0;

	/* coverity[misra_c_2012_rule_10_8] */
	/* coverity[misra_c_2012_rule_11_5] */
	dev_context = devm_kzalloc(dev, sizeof(*dev_context), GFP_KERNEL);
	if (dev_context == NULL) {
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		dev_context->dev = dev;
		dev_context->timings = NULL;

		ret = tccdrm_lvds_parse_dt(dev_context);

		if (ret < 0) {
			internal_ok = (bool)false;
		}

		tccdrm_lvds_parse_panel(dev_context);

		lvds_status =
		LVDS_PHY_CheckStatus(
			dev_context->tcc_lvds_hw.port_main,
			dev_context->tcc_lvds_hw.port_sub);
		if ((lvds_status & 0x01U) == 0U) {
			dev_err(dev_context->dev,
				"[ERROR][%s:%s] %s Primary port(%d) is in death\r\n",
				LOG_TAG, plat_dev->name, __func__,
				dev_context->tcc_lvds_hw.port_main);
		} else {
			if (dev_context->tcc_lvds_hw.lvds_type == (unsigned int)PANEL_LVDS_SINGLE) {
				dev_context->enabled = 1;
			}
			dev_info(dev_context->dev,
				"[INFO][%s:%s] %s Primary port(%d) is in alive\r\n",
				LOG_TAG, plat_dev->name, __func__,
				dev_context->tcc_lvds_hw.port_main);
		}
		if (dev_context->tcc_lvds_hw.lvds_type == (unsigned int)PANEL_LVDS_DUAL) {
			if ((lvds_status & 0x02U) == 0U) {
				dev_err(dev_context->dev,
					"[ERROR][%s:%s] %s Secondary port(%d) is in death\r\n",
					LOG_TAG, plat_dev->name, __func__,
					dev_context->tcc_lvds_hw.port_sub);
			} else {
				dev_context->enabled = 1;
				dev_info(dev_context->dev,
					"[INFO][%s:%s] %s Secondary port(%d) is in alive\r\n",
					LOG_TAG, plat_dev->name, __func__,
					dev_context->tcc_lvds_hw.port_sub);
			}
		}
	}

	if (internal_ok) {
		(void)DRM_INFO("Initialized %s %d.%d.%d %s\r\n",
				plat_dev->name,
				DRIVER_MAJOR,
				DRIVER_MINOR,
				DRIVER_PATCH,
				DRIVER_DATE);
		platform_set_drvdata(plat_dev, dev_context);
		ret = component_add(dev, &tccdrm_lvds_component_ops);
	}
	return ret;
}

static int tccdrm_lvds_remove(struct platform_device *plat_dev)
{
	/* coverity[misra_c_2012_rule_11_5] */
	const struct tccdrm_lvds_context *dev_context =
		(const struct tccdrm_lvds_context *)platform_get_drvdata(plat_dev);
	struct device *dev = &plat_dev->dev;

	component_del(dev, &tccdrm_lvds_component_ops);

	devm_kfree(dev, dev_context);

	return 0;
}

static const struct of_device_id tccdrm_lvds_dt_match[] = {
	{
		.compatible = "telechips,drm-lvds-dual",
	},
	{
		.compatible = "telechips,drm-lvds-single",
	},
	{
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, tccdrm_lvds_dt_match);

struct platform_driver tccdrm_lvds_driver = {
	.probe		= tccdrm_lvds_probe,
	.remove		= tccdrm_lvds_remove,
	.driver		= {
		.name	= "tccdrm-lvds",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tccdrm_lvds_dt_match),
	},
};

/* coverity[misra_c_2012_rule_8_3] */
/* coverity[misra_c_2012_rule_8_5] */
/* coverity[misra_c_2012_rule_8_6] */
/* coverity[misra_c_2012_rule_8_11] */
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
EXPORT_SYMBOL(tccdrm_lvds_driver);

/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_21_2] */
MODULE_DESCRIPTION("Telechips DRM LVDS");

/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_21_2] */
MODULE_LICENSE("GPL");

/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_5_9] */ //K5.4
/* coverity[misra_c_2012_rule_21_2] */ //K5.4
MODULE_VERSION(__stringify(DRIVER_MAJOR) "."
               __stringify(DRIVER_MINOR) "."
               __stringify(DRIVER_PATCH));

