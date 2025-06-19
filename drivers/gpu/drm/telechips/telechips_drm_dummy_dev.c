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

#if defined(CONFIG_REFCODE_PRE_K510)
#include <drm/drmP.h>
#endif
#include <drm/drm_crtc_helper.h>
#include <drm/drm_panel.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_vblank.h>
#include <drm/drm_print.h>
#include <drm/drm_panel.h>

#include <linux/of_graph.h>
#include <linux/regulator/consumer.h>
#include <linux/fs.h>
#include <linux/component.h>
#include <linux/tcc_math.h>
#include <linux/platform_device.h>

#include <video/of_videomode.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>

#include <telechips_drm_types.h>
#include <telechips_drm_drv.h>
#include <telechips_drm_dummy_dev.h>

#define DRIVER_DATE	"20240227"
#define DRIVER_MAJOR 2
#define DRIVER_MINOR 2
#define DRIVER_PATCH 1

struct tccdrm_dummy_context {
	struct drm_encoder encoder;
	struct drm_connector connector;
	struct device *dev;

	struct drm_panel *panel;
	struct display_timings *timings;
	int connector_type;
	bool binded;
};

#define connector_to_context(x) \
		container_of((x), struct tccdrm_dummy_context, connector)
#define encoder_to_context(x) \
		container_of((x), struct tccdrm_dummy_context, encoder)


/* coverity[misra_c_2012_rule_8_13] */
static enum drm_connector_status tccdrm_dummy_detect(struct drm_connector *connector,
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

static void tccdrm_dummy_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static const struct drm_connector_funcs tccdrm_dummy_connector_funcs = {
	.detect = tccdrm_dummy_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = tccdrm_dummy_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
};

static int tccdrm_dummy_register_mode(struct drm_connector *connector,
				      struct drm_display_mode *in_mode)
{
	const struct tccdrm_dummy_context *dev_context;
	bool internal_ok = (bool)true;
	int tmp_val;
	int ret = 0;

	if (connector == NULL) {
		(void)pr_err(
			"[ERROR] connector is NULL\r\n");
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
		dev_context =
			(const struct tccdrm_dummy_context *)connector_to_context(connector);
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
			/* DP no.14 */
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
			/* DP no.14 */
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


static int tccdrm_dummy_get_dev_node_modes(struct tccdrm_dummy_context *dev_context)
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
		if (tccdrm_dummy_register_mode(connector, modes) < 0) {
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
static int tccdrm_dummy_get_modes(struct drm_connector *connector)
{
	struct tccdrm_dummy_context *dev_context;

	bool internal_ok = (bool)true;
	int mode_count = 0;

	if (connector == NULL) {
		DRM_DEV_ERROR(NULL, "connector is NULL\r\n");
		internal_ok = (bool)false;
	}
	if (internal_ok) {
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
		dev_context = (struct tccdrm_dummy_context *)connector_to_context(connector);
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
			if (mode_count == 0) {
				DRM_DEV_INFO(dev_context->dev,
					     "[WARN] It has a panel node, but there is no detailed-display-timing information in panel node.\r\n");
			}
		}
	}
	/* step2: Check detailed timing from device tree */
	if (internal_ok) {
		if (mode_count == 0) {
			mode_count = tccdrm_dummy_get_dev_node_modes(dev_context);
		}
		if (mode_count == 0) {
			DRM_DEV_ERROR(dev_context->dev,
				      "There is no detailed-display-timing information in device node.\r\n");
		}
	}
	return mode_count;
}

#if defined(CONFIG_REFCODE_PRE_K54)
#else
static struct drm_encoder *tccdrm_dummy_best_single_encoder(
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
	struct tccdrm_dummy_context *dev_context = connector_to_context(connector);

	return &dev_context->encoder;
}
#endif

static const struct drm_connector_helper_funcs tccdrm_dummy_connector_helper_funcs = {
	.get_modes = tccdrm_dummy_get_modes,
	#if defined(CONFIG_REFCODE_PRE_K54)
	.best_encoder = drm_atomic_helper_best_encoder,
	#else
	.best_encoder = tccdrm_dummy_best_single_encoder,
	#endif
};

static int tccdrm_dummy_set_connector(struct tccdrm_dummy_context *dev_context)
{
	struct drm_connector *connector = &dev_context->connector;
	struct drm_encoder *encoder = &dev_context->encoder;
	bool internal_ok = (bool)true;
	int ret = 0;

	ret = drm_connector_init(encoder->dev,
				 connector, &tccdrm_dummy_connector_funcs,
				 dev_context->connector_type);
	if (ret < 0) {
		DRM_DEV_ERROR(dev_context->dev,
			      "failed to initialize connector with drm\n");
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		drm_connector_helper_add(connector,
					 &tccdrm_dummy_connector_helper_funcs);
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

static void
tccdrm_dummy_mode_set(struct drm_encoder *encoder,
			/* coverity[misra_c_2012_rule_8_13] */
			struct drm_crtc_state *crtc_state,
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
	const struct tccdrm_dummy_context *dev_context = encoder_to_context(encoder);

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter crtc_state is
	 * not used in the function.
	 */
	(void)crtc_state;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter connector_state is
	 * not used in the function.
	 */
	(void)connector_state;

	if (dev_context->panel != NULL) {
		(void)drm_panel_prepare(dev_context->panel);
		/*  panel->funcs->prepare(panel) */
	}
}

static void tccdrm_dummy_enable(struct drm_encoder *encoder,
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
	const struct tccdrm_dummy_context *dev_context = encoder_to_context(encoder);

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
		struct tcc_crtc_state *tcc_cstate = to_tcc_crtc_state(encoder->crtc->state);

		DRM_DEV_INFO(dev_context->dev,
			"[INFO] tccdrm_dummy is connectd to lcdc_num %u , 0x%px\r\n",
			tcc_cstate->lcdc_num, tcc_cstate);

		if (dev_context->panel != NULL) {
			(void)drm_panel_enable(dev_context->panel);
			/* panel->funcs->enable(panel); */
		}
	}
}

static void tccdrm_dummy_disable(struct drm_encoder *encoder,
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
	const struct tccdrm_dummy_context *dev_context =
		(const struct tccdrm_dummy_context *)encoder_to_context(encoder);

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter drm_astate is not
	 * used in the function.
	 */
	(void)drm_astate;

	if (dev_context->panel != NULL) {
		(void)drm_panel_disable(dev_context->panel);
		/* panel->funcs->disable(panel); */

		(void)drm_panel_unprepare(dev_context->panel);
		/* panel->funcs->unprepare(panel); */
	}
}

/* coverity[misra_c_2012_rule_8_13] */
static int tccdrm_dummy_check(struct drm_encoder *encoder,
				      struct drm_crtc_state *crtc_state,
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
	struct tcc_crtc_state *tcc_cstate = to_tcc_crtc_state(crtc_state);

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
	const struct tccdrm_dummy_context *dev_context = encoder_to_context(encoder);

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter conn_state is not
	 * used in the function.
	 */
	(void)conn_state;

	tcc_cstate->connector_type = dev_context->connector_type;

	return 0;
}

static const struct drm_encoder_helper_funcs tccdrm_dummy_encoder_helper_funcs = {
	.atomic_mode_set = tccdrm_dummy_mode_set,
	.atomic_enable = tccdrm_dummy_enable,
	.atomic_disable = tccdrm_dummy_disable,
	.atomic_check = tccdrm_dummy_check,
};

static const struct drm_encoder_funcs tccdrm_dummy_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static int tccdrm_dummy_parse_dt(struct tccdrm_dummy_context *dev_context)
{
	const struct device *dev = dev_context->dev;
	const struct device_node *dn = dev->of_node;
	const char *connector_type;

	struct device_node *np;
	int ret = 0;

	if (of_property_read_string(dn, "connector_type", &connector_type) != 0) {
		dev_context->connector_type = DRM_MODE_CONNECTOR_LVDS;
		DRM_DEV_INFO(dev,
			     "[INFO] Failed to find connector_type -> LVDS\n");
	} else {
		if (strcmp(connector_type, "DP") == 0) {
			dev_context->connector_type =
				DRM_MODE_CONNECTOR_DisplayPort;
		} else if (strcmp(connector_type, "HDMI-A") == 0) {
			dev_context->connector_type =
				DRM_MODE_CONNECTOR_HDMIA;
		} else if (strcmp(connector_type, "HDMI-B") == 0) {
			dev_context->connector_type =
				DRM_MODE_CONNECTOR_HDMIB;
		} else if (strcmp(connector_type, "VIRTUAL") == 0) {
			dev_context->connector_type =
				DRM_MODE_CONNECTOR_VIRTUAL;
		}  else if (strcmp(connector_type, "DSI") == 0) {
			dev_context->connector_type =
				DRM_MODE_CONNECTOR_DSI;
		} else {
			dev_context->connector_type =
				DRM_MODE_CONNECTOR_LVDS;
		}
		DRM_DEV_INFO(dev,
			     "[INFO] connector type from dtb is DRM_MODE_CONNECTOR_%s\n",
			     connector_type);
	}
	np = of_get_child_by_name(dn, "display-timings");
	if (np != NULL) {
		of_node_put(np);

		dev_context->timings = of_get_display_timings(dn);
		if (dev_context->timings == NULL) {
			DRM_DEV_INFO(dev,
				     "[WARN] failed to of_get_display_timings\n");
			ret = -ENODEV;
		}
	} else {
		DRM_DEV_DEBUG(dev,
			      "[DEBUG] cannot find display-timings node\n");
	}

	return ret;
}

static void tccdrm_dummy_parse_panel(struct tccdrm_dummy_context *dev_context)
{
	const struct device *dev = dev_context->dev;
	int ret = 0;

	/* find panel from port reg <1> */
	ret = drm_of_find_panel_or_bridge(dev->of_node, 1, -1,
					  &dev_context->panel, NULL);

	if (ret < 0) {
		DRM_DEV_INFO(dev, "[INFO] has no DRM dummy panel\r\n");
	} else {
		if (dev_context->panel != NULL) {
			DRM_DEV_INFO(dev, "[INFO] has DRM dummy panel\r\n");
		}
	}
}

static int tccdrm_dummy_check_possible_crtcs(const struct device *dev,
											struct drm_device *drm_dev,
											struct tccdrm_dummy_context *tcc_dev_context)
{
	struct drm_encoder *encoder;
	int ret = -1;

	encoder = &tcc_dev_context->encoder;
	encoder->possible_crtcs = drm_of_find_possible_crtcs(drm_dev, dev->of_node);

	if (encoder->possible_crtcs > 0U) {
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
		DRM_DEV_INFO(dev,
				"This encoder will also be deactivated because the crtc connected with this encoder is probably in a deactivated state.\r\n");
	}
	//TODO: it could be error when a possible_crtcs is less than zero.

	return ret;
}

static int tccdrm_dummy_set_encoder(struct tccdrm_dummy_context *tcc_dev_context,
									struct drm_device *drm_dev)
{
	struct drm_encoder *encoder;
	int ret;
	int encoder_type;

	encoder = &tcc_dev_context->encoder;

	switch (tcc_dev_context->connector_type) {
		case DRM_MODE_CONNECTOR_DisplayPort:
		case DRM_MODE_CONNECTOR_HDMIA:
		case DRM_MODE_CONNECTOR_HDMIB:
			encoder_type = DRM_MODE_ENCODER_TMDS;
			break;
		case DRM_MODE_CONNECTOR_VIRTUAL:
			encoder_type = DRM_MODE_ENCODER_VIRTUAL;
			break;
		case DRM_MODE_CONNECTOR_DSI:
			encoder_type = DRM_MODE_ENCODER_DSI;
			break;
		default:
			encoder_type = DRM_MODE_ENCODER_LVDS;
			break;
	}

	ret = drm_encoder_init(drm_dev,
				encoder,
				&tccdrm_dummy_encoder_funcs,
				encoder_type, NULL);

	if (ret == 0) {
		drm_encoder_helper_add(encoder, &tccdrm_dummy_encoder_helper_funcs);

	} else {
		DRM_DEV_ERROR(tcc_dev_context->dev, "failed to initialize encoder with drm\n");
		drm_encoder_cleanup(encoder);

	}

	return ret;
}


/* coverity[misra_c_2012_rule_8_13] */
static int tccdrm_dummy_bind(struct device *dev,
		      /* coverity[misra_c_2012_rule_8_13] */
		      struct device *master_dev,
		      /* coverity[misra_c_2012_rule_8_13] */
		      void *data)
{
	struct tccdrm_dummy_context *dev_context;
	/* coverity[misra_c_2012_rule_11_5] */
	struct drm_device *drm_dev = (struct drm_device *)data;
	int ret = 0;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter master_dev is not
	 * used in the function.
	 */
	(void)master_dev;

	/* coverity[misra_c_2012_rule_11_5] */
	dev_context = dev_get_drvdata(dev);
	if (dev_context != NULL) {
		if(tccdrm_dummy_check_possible_crtcs(dev, drm_dev, dev_context) == 0) {
			ret = tccdrm_dummy_set_encoder(dev_context, drm_dev);
			if(ret == 0) {
				ret = tccdrm_dummy_set_connector(dev_context);
			}

			if (ret == 0 ) {
				dev_context->binded = (bool)true;
			}
		}
	} else {
		ret = -ENOMEM;
	}

	return ret;
}

/* coverity[misra_c_2012_rule_8_13] */
static void tccdrm_dummy_unbind(struct device *dev,
			 /* coverity[misra_c_2012_rule_8_13] */
			 struct device *master_dev,
			 /* coverity[misra_c_2012_rule_8_13] */
			 void *data)
{
	/* coverity[misra_c_2012_rule_11_5] */
	struct tccdrm_dummy_context *dev_context = dev_get_drvdata(dev);
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
}

static const struct component_ops tccdrm_dummy_component_ops = {
	.bind = tccdrm_dummy_bind,
	.unbind = tccdrm_dummy_unbind,
};

static int tccdrm_dummy_probe(struct platform_device *plat_dev)
{
	struct tccdrm_dummy_context *dev_context;
	struct device *dev = &plat_dev->dev;
	bool internal_ok = (bool)true;
	int ret = 0;

	/* coverity[misra_c_2012_rule_10_8] */
	/* coverity[misra_c_2012_rule_11_5] */
	dev_context = devm_kzalloc(dev, sizeof(*dev_context), GFP_KERNEL);
	if (dev_context == NULL) {
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		dev_context->dev = dev;
		dev_context->timings = NULL;

		(void)tccdrm_dummy_parse_dt(dev_context);

		tccdrm_dummy_parse_panel(dev_context);
	}

	if (internal_ok) {
		(void)DRM_INFO("Initialized %s %d.%d.%d %s\r\n",
				plat_dev->name,
				DRIVER_MAJOR,
				DRIVER_MINOR,
				DRIVER_PATCH,
				DRIVER_DATE);
		platform_set_drvdata(plat_dev, dev_context);
		ret = component_add(dev, &tccdrm_dummy_component_ops);
	}
	return ret;
}

static int tccdrm_dummy_remove(struct platform_device *plat_dev)
{
	/* coverity[misra_c_2012_rule_11_5] */
	const struct tccdrm_dummy_context *dev_context =
		(const struct tccdrm_dummy_context *)platform_get_drvdata(plat_dev);
	struct device *dev = &plat_dev->dev;

	component_del(dev, &tccdrm_dummy_component_ops);

	devm_kfree(dev, dev_context);

	return 0;
}

static const struct of_device_id tccdrm_dummy_dt_match[] = {
	{
		.compatible = "telechips,drm-dummy-dev",
	},
	{
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, tccdrm_dummy_dt_match);

struct platform_driver tccdrm_dummy_driver = {
	.probe		= tccdrm_dummy_probe,
	.remove		= tccdrm_dummy_remove,
	.driver		= {
		.name	= "tccdrm-dummy-dev",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tccdrm_dummy_dt_match),
	},
};
/* coverity[misra_c_2012_rule_8_3] */
/* coverity[misra_c_2012_rule_8_5] */
/* coverity[misra_c_2012_rule_8_6] */
/* coverity[misra_c_2012_rule_8_11] */
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
EXPORT_SYMBOL(tccdrm_dummy_driver);

/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_21_2] */ // K5.4
MODULE_DESCRIPTION("Telechips DRM Dummy display");

/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_21_2] */ // K5.4
MODULE_LICENSE("GPL");

/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_5_9] */ //K5.4
/* coverity[misra_c_2012_rule_21_2] */ //K5.4
MODULE_VERSION(__stringify(DRIVER_MAJOR) "."
               __stringify(DRIVER_MINOR) "."
               __stringify(DRIVER_PATCH));
