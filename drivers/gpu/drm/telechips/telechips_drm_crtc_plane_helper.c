// SPDX-License-Identifier: GPL-2.0-or-later

/* telechips_drm_crtc_plane_helper.c
 *
 * Copyright (C) 2022 Telechips Inc.
 * Authors:
 *	Jayden Kim
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#if defined(CONFIG_REFCODE_PRE_K510)
#include <drm/drmP.h>
#endif
#include <drm/drm_atomic_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_encoder.h>
#include <drm/drm_vblank.h>
#include <drm/drm_atomic.h>
#include <drm/drm_print.h>

#include <telechips_drm_types.h>
#include <telechips_drm_crtc_plane_helper.h>

#include <video/telechips/tcc_types.h>
#include <video/telechips/vioc_global.h>

/* coverity[misra_c_2012_rule_8_13] */
void tccdrm_drmfmt_to_viocfmt(uint32_t pixel_format,
			      struct vioc_fmt_t *vioc_fmt)
{
	(void)memset(vioc_fmt, 0, sizeof(*vioc_fmt));

	switch (pixel_format) {
	case DRM_FORMAT_BGR565:
		vioc_fmt->f_swap = VIOC_SWAP_BGR;  /* B-G-R */
		vioc_fmt->f_fmt = (unsigned int)TCC_LCDC_IMG_FMT_RGB565;
		#if defined(CONFIG_VIOC_PVRIC_FBDC)
		vioc_fmt->swizz = VIOC_PVRICCTRL_SWIZZ_ABGR;
		#endif
		break;
	case DRM_FORMAT_RGB565:
		vioc_fmt->f_fmt = (unsigned int)TCC_LCDC_IMG_FMT_RGB565;
		break;
	case DRM_FORMAT_BGR888:
		vioc_fmt->f_swap = VIOC_SWAP_BGR;  /* B-G-R */
		vioc_fmt->f_fmt = (unsigned int)TCC_LCDC_IMG_FMT_RGB888_3;
		#if defined(CONFIG_VIOC_PVRIC_FBDC)
		vioc_fmt->swizz = VIOC_PVRICCTRL_SWIZZ_ABGR;
		#endif
		break;
	case DRM_FORMAT_RGB888:
		vioc_fmt->f_fmt = (unsigned int)TCC_LCDC_IMG_FMT_RGB888_3;
		break;
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
		vioc_fmt->f_swap = VIOC_SWAP_BGR;  /* B-G-R */
		vioc_fmt->f_fmt = (unsigned int)TCC_LCDC_IMG_FMT_RGB888;
		#if defined(CONFIG_VIOC_PVRIC_FBDC)
		vioc_fmt->swizz = VIOC_PVRICCTRL_SWIZZ_ABGR;
		#endif
		break;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		vioc_fmt->f_fmt = (unsigned int)TCC_LCDC_IMG_FMT_RGB888;
		break;
	case DRM_FORMAT_NV12:
		vioc_fmt->f_fmt = (unsigned int)TCC_LCDC_IMG_FMT_YUV420ITL0;
		vioc_fmt->f_y2r = 1;
		break;
	case DRM_FORMAT_NV21:
		vioc_fmt->f_fmt = (unsigned int)TCC_LCDC_IMG_FMT_YUV420ITL1;
		vioc_fmt->f_y2r = 1;
		break;
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YUV420:
		vioc_fmt->f_fmt = (unsigned int)TCC_LCDC_IMG_FMT_YUV420SP;
		vioc_fmt->f_y2r = 1;
		break;
	default:
		vioc_fmt->f_fmt = (unsigned int)TCC_LCDC_IMG_FMT_RGB888;
		break;
	}
}
/* coverity[misra_c_2012_rule_5_2] */
/* coverity[misra_c_2012_rule_8_3] */
/* coverity[misra_c_2012_rule_8_5] */
/* coverity[misra_c_2012_rule_8_6] */
/* coverity[misra_c_2012_rule_8_11] */
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
EXPORT_SYMBOL(tccdrm_drmfmt_to_viocfmt);

int tcc_prepare_universal_plane(struct drm_device *dev,
	/* coverity[misra_c_2012_rule_8_13] */
	struct tccdrm_universal_plane_data *universal_plane_data)
{
	bool internal_ok = (bool)true;
	unsigned int num_crtc;
	int ret = 0;

	if (dev->mode_config.num_crtc < 0) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}

	if (internal_ok) {
		num_crtc = (unsigned int)dev->mode_config.num_crtc;
		num_crtc = (unsigned int)1U << num_crtc;

		ret = drm_universal_plane_init(dev, universal_plane_data->plane,
					num_crtc,
					universal_plane_data->plane_funcs,
					universal_plane_data->pixel_formats,
					universal_plane_data->num_pixel_formats,
					universal_plane_data->format_modifiers,
					universal_plane_data->plane_type, NULL);
		if (ret < 0) {
			DRM_ERROR("failed to initialize plane\n");
			internal_ok = (bool)false;
		}
	}

	if (internal_ok) {
		drm_plane_helper_add(universal_plane_data->plane,
				     universal_plane_data->plane_helper_funcs);
	}

	return ret;
}
/* coverity[misra_c_2012_rule_8_3] */
/* coverity[misra_c_2012_rule_8_5] */
/* coverity[misra_c_2012_rule_8_6] */
/* coverity[misra_c_2012_rule_8_11] */
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
EXPORT_SYMBOL(tcc_prepare_universal_plane);

void tccdrm_plane_reset(struct drm_plane *plane)
{
	struct tcc_drm_plane_state *tcc_pstate;

	if (plane->state != NULL) {
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
		tcc_pstate = to_tcc_plane_state(plane->state);
		__drm_atomic_helper_plane_destroy_state(plane->state);
		kfree(tcc_pstate);
	}

	/* coverity[misra_c_2012_rule_10_8] */
	/* coverity[misra_c_2012_rule_11_5] */
	tcc_pstate = kzalloc(sizeof(*tcc_pstate), GFP_KERNEL);
	#if defined(CONFIG_REFCODE_PRE_K54)
	if (tcc_pstate != NULL) {
		plane->state = &tcc_pstate->base;
		plane->state->plane = plane;
	}
	#else
	if (tcc_pstate != NULL) {
		__drm_atomic_helper_plane_reset(plane, &tcc_pstate->base);
	}
	#endif
}
/* coverity[misra_c_2012_rule_5_2] */
/* coverity[misra_c_2012_rule_8_3] */
/* coverity[misra_c_2012_rule_8_5] */
/* coverity[misra_c_2012_rule_8_6] */
/* coverity[misra_c_2012_rule_8_11] */
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
EXPORT_SYMBOL(tccdrm_plane_reset);

struct drm_plane_state *tccdrm_plane_duplicate_state(struct drm_plane *plane)
{
	struct tcc_drm_plane_state *duplicate_state;
	struct drm_plane_state *ptr = NULL;

	/* coverity[misra_c_2012_rule_10_8] */
	/* coverity[misra_c_2012_rule_11_5] */
	duplicate_state = kzalloc(
		sizeof(struct tcc_drm_plane_state), GFP_KERNEL);
	if (duplicate_state != NULL) {
		__drm_atomic_helper_plane_duplicate_state(
					plane, &duplicate_state->base);
		ptr = &duplicate_state->base;
	}
	return ptr;
}
/* coverity[misra_c_2012_rule_5_2] */
/* coverity[misra_c_2012_rule_8_3] */
/* coverity[misra_c_2012_rule_8_5] */
/* coverity[misra_c_2012_rule_8_6] */
/* coverity[misra_c_2012_rule_8_11] */
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
EXPORT_SYMBOL(tccdrm_plane_duplicate_state);


/* coverity[misra_c_2012_rule_8_13] */
void tccdrm_plane_destory_state(struct drm_plane *plane,
				struct drm_plane_state *old_drm_pstate)
{
	const struct tcc_drm_plane_state *old_tcc_pstate =
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
					to_tcc_plane_state(old_drm_pstate);


	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter plane is not used in
	 * the function.
	 */
	(void)plane;

	__drm_atomic_helper_plane_destroy_state(old_drm_pstate);
	kfree(old_tcc_pstate);
}
/* coverity[misra_c_2012_rule_5_2] */
/* coverity[misra_c_2012_rule_8_3] */
/* coverity[misra_c_2012_rule_8_5] */
/* coverity[misra_c_2012_rule_8_6] */
/* coverity[misra_c_2012_rule_8_11] */
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
EXPORT_SYMBOL(tccdrm_plane_destory_state);


struct drm_crtc_state *tccdrm_crtc_duplicate_state(struct drm_crtc *crtc)
{
	struct tcc_crtc_state *tcc_cstate;
	struct drm_crtc_state *crtc_state = NULL;

	/* coverity[misra_c_2012_rule_10_8] */
	/* coverity[misra_c_2012_rule_11_5] */
	tcc_cstate = (struct tcc_crtc_state *)kzalloc(sizeof(*tcc_cstate), GFP_KERNEL);
	if (tcc_cstate != NULL) {
		__drm_atomic_helper_crtc_duplicate_state(crtc, &tcc_cstate->base);
		crtc_state = &tcc_cstate->base;
	}

	return crtc_state;
}
/* coverity[misra_c_2012_rule_5_2] */
/* coverity[misra_c_2012_rule_8_3] */
/* coverity[misra_c_2012_rule_8_5] */
/* coverity[misra_c_2012_rule_8_6] */
/* coverity[misra_c_2012_rule_8_11] */
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
EXPORT_SYMBOL(tccdrm_crtc_duplicate_state);

/* coverity[misra_c_2012_rule_8_13] */
void tccdrm_crtc_destroy_state(struct drm_crtc *crtc,
			       struct drm_crtc_state *drm_cstate)
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
	struct tcc_crtc_state *tcc_cstate = (struct tcc_crtc_state *)to_tcc_crtc_state(drm_cstate);

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter crtc is not
	 * used in the function.
	 */
	(void)crtc;

	__drm_atomic_helper_crtc_destroy_state(&tcc_cstate->base);
	kfree(tcc_cstate);
}
/* coverity[misra_c_2012_rule_5_2] */
/* coverity[misra_c_2012_rule_8_3] */
/* coverity[misra_c_2012_rule_8_5] */
/* coverity[misra_c_2012_rule_8_6] */
/* coverity[misra_c_2012_rule_8_11] */
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
EXPORT_SYMBOL(tccdrm_crtc_destroy_state);

int tccdrm_crtc_create(const struct device *dev,
		       struct drm_device *drm,
		       const struct tccdrm_crtc_create_data *crtc_create_data)
{
	const struct drm_crtc_helper_funcs *crtc_helper_funcs =
		crtc_create_data->crtc_helper_funcs;
	const struct drm_crtc_funcs *crtc_funcs  =
		crtc_create_data->crtc_funcs;
	struct drm_plane *primary  = crtc_create_data->primary;
	struct drm_plane *cursor  = crtc_create_data->cursor;
	const char *crtc_name  = crtc_create_data->crtc_name;
	struct drm_crtc *crtc  = crtc_create_data->crtc;
	bool internal_ok = (bool)true;
	struct device_node *port;
	int ret = 0;

	port = of_get_child_by_name(dev->of_node, "ports");
	if (port == NULL) {
		DRM_DEV_ERROR(dev, "no ports node found for %s\n", dev_name(dev));
		internal_ok = (bool)false;
		ret = -ENOENT;
	}
	if (internal_ok) {
		port = of_get_child_by_name(port, "port");
		if (port == NULL) {
			DRM_DEV_ERROR(dev, "no port node found for %s\n",
				dev_name(dev));
			internal_ok = (bool)false;
			ret = -ENOENT;
		} else {
			crtc->port = port;
			//DRM_DEV_INFO(dev, "crtc=0x%px, port=0x%px\r\n", crtc, crtc->port);
		}
	}

	if (internal_ok) {
		ret = drm_crtc_init_with_planes(drm,
						crtc, primary, cursor,
						crtc_funcs, crtc_name);
		if (ret < 0) {
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		drm_crtc_helper_add(crtc, crtc_helper_funcs);
	} else {
		if ((primary != NULL) &&
		    (primary->funcs->destroy != NULL)) {
			primary->funcs->destroy(primary);
		}
		if ((cursor != NULL) &&
		    (cursor->funcs->destroy != NULL)) {
			cursor->funcs->destroy(cursor);
		}
	}
	return ret;
}
/* coverity[misra_c_2012_rule_8_3] */
/* coverity[misra_c_2012_rule_8_5] */
/* coverity[misra_c_2012_rule_8_6] */
/* coverity[misra_c_2012_rule_8_11] */
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
EXPORT_SYMBOL(tccdrm_crtc_create);

/* coverity[misra_c_2012_rule_8_13] */
void tccdrm_crtc_update_flip_event(struct drm_crtc *crtc,
				   struct tccdrm_flip_state *flip_state)
{
	unsigned long irq_flags;

	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_14_4] */
	spin_lock_irqsave(&crtc->dev->event_lock, irq_flags);
	flip_state->flipevent = crtc->state->event;
	crtc->state->event = NULL;

	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_21_2] */
	atomic_set(&flip_state->flipstatus,
			TCC_DRM_CRTC_FLIP_STATUS_DONE);
	spin_unlock_irqrestore(&crtc->dev->event_lock, irq_flags);
}
/* coverity[misra_c_2012_rule_5_2] */
/* coverity[misra_c_2012_rule_8_3] */
/* coverity[misra_c_2012_rule_8_5] */
/* coverity[misra_c_2012_rule_8_6] */
/* coverity[misra_c_2012_rule_8_11] */
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
EXPORT_SYMBOL(tccdrm_crtc_update_flip_event);

static void tccdrm_crtc_flip_complete(struct drm_crtc *crtc,
				      struct tccdrm_flip_state *flip_state)
{
	unsigned long irq_flags;

	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_14_4] */
	spin_lock_irqsave(&crtc->dev->event_lock, irq_flags);

	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_21_2] */
	atomic_set(&flip_state->flipstatus, (int)TCC_DRM_CRTC_FLIP_STATUS_NONE);
	flip_state->flipasync = (bool)false;

	if (flip_state->flipevent != NULL) {
		drm_crtc_send_vblank_event(crtc, flip_state->flipevent);
		flip_state->flipevent = NULL;
	}

	spin_unlock_irqrestore(&crtc->dev->event_lock, irq_flags);
}

void tccdrm_crtc_handle_event(struct drm_crtc *crtc,
			      struct tccdrm_flip_state *flip_state)
{
	const struct drm_crtc_state *new_crtc_state = crtc->state;
	bool internal_ok = (bool)true;

	if (!new_crtc_state->active) {
		internal_ok = (bool)false;
		DRM_DEV_DEBUG(crtc->dev->dev,
			      "[DEBUG] new_crtc_state is not actived\r\n");
	}

	if (internal_ok && (new_crtc_state->event == NULL)) {
		DRM_DEV_DEBUG(crtc->dev->dev,
			      "[DEBUG] event on new crtc state is NULL\r\n");
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		#if defined(CONFIG_REFCODE_PRE_K54)
		unsigned int pageflip_flags =
			(unsigned int)DRM_MODE_PAGE_flip_state;

		flip_state->flipasync =
			((new_crtc_state->pageflip_flags & pageflip_flags) != 0U) ?
			(bool)true : (bool)false;
		#else
		flip_state->flipasync = new_crtc_state->async_flip;
		#endif

		if (flip_state->flipasync) {
			/* coverity[misra_c_2012_rule_10_3] */
			/* coverity[misra_c_2012_rule_14_4] */
			/* coverity[misra_c_2012_rule_15_6] */
			WARN_ON(drm_crtc_vblank_get(crtc) != 0);
		}

		tccdrm_crtc_update_flip_event(crtc, flip_state);
		if (flip_state->flipasync) {
			tccdrm_crtc_flip_complete(crtc, flip_state);
		}
	}
}
/* coverity[misra_c_2012_rule_5_2] */
/* coverity[misra_c_2012_rule_8_3] */
/* coverity[misra_c_2012_rule_8_5] */
/* coverity[misra_c_2012_rule_8_6] */
/* coverity[misra_c_2012_rule_8_11] */
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
EXPORT_SYMBOL(tccdrm_crtc_handle_event);

void tccdrm_crtc_vblank_handler(struct drm_crtc *crtc,
				struct tccdrm_flip_state *flip_state)
{
	int status;

	#if defined(CONFIG_REFCODE_PRE_K510)
	(void)drm_handle_vblank(crtc->dev, drm_crtc_index(crtc));
	#else
	(void)drm_crtc_handle_vblank(crtc);
	#endif

	/* coverity[cert_dcl37_c] */
	/* coverity[cert_pre31_c] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_21_2] */
	status = atomic_read(&flip_state->flipstatus);
	if (status == TCC_DRM_CRTC_FLIP_STATUS_DONE) {
		if (!flip_state->flipasync) {
			tccdrm_crtc_flip_complete(crtc, flip_state);
		}
	}
}
/* coverity[misra_c_2012_rule_5_2] */
/* coverity[misra_c_2012_rule_8_3] */
/* coverity[misra_c_2012_rule_8_5] */
/* coverity[misra_c_2012_rule_8_6] */
/* coverity[misra_c_2012_rule_8_11] */
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
EXPORT_SYMBOL(tccdrm_crtc_vblank_handler);

struct drm_connector *tccdrm_crtc_get_connector(const struct drm_crtc *crtc)
{
	struct drm_connector_list_iter conn_iter;
	struct drm_connector *conn_target = NULL;
	struct drm_connector *connector;
	const struct drm_encoder *encoder;
	struct drm_device *dev;

	dev = crtc->dev;
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
	drm_for_each_encoder(encoder, dev) {
		if (encoder->crtc != crtc) {
			continue;
		}

		drm_connector_list_iter_begin(dev, &conn_iter);
		/* coverity[misra_c_2012_rule_11_9] */
		/* coverity[misra_c_2012_rule_13_4] */
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_20_7] */
		drm_for_each_connector_iter(connector, &conn_iter) {
			if (connector->encoder == encoder) {
				conn_target = connector;
				break;
			}
		}
		drm_connector_list_iter_end(&conn_iter);
	}
	return conn_target;
}
/* coverity[misra_c_2012_rule_5_2] */
/* coverity[misra_c_2012_rule_8_3] */
/* coverity[misra_c_2012_rule_8_5] */
/* coverity[misra_c_2012_rule_8_6] */
/* coverity[misra_c_2012_rule_8_11] */
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
EXPORT_SYMBOL(tccdrm_crtc_get_connector);



/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_21_2] */ // K5.4
MODULE_DESCRIPTION("Telechips DRM Commons");

/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_21_2] */ // K5.4
MODULE_LICENSE("GPL");