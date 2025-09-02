/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * telechips_drm_crtc_plane_helper.h
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
#ifndef TCCDRM_CRTC_PLANE_HELPER_H
#define TCCDRM_CRTC_PLANE_HELPER_H

#define DRM_PLAME_TYPE_MASK     0xFFFFU
#define DRM_PLAME_TYPE_SHIFT    0U
#define DRM_PLAME_FLAG_MASK     0x0FFFU
#define DRM_PLAME_FLAG_SHIFT    16U

#define DRM_PLANE_TYPE(x)	\
	(((uint32_t)(x) >> DRM_PLAME_TYPE_SHIFT) & DRM_PLAME_TYPE_MASK)
#define DRM_PLANE_FLAG(x) 	\
	(((uint32_t)(x) >> DRM_PLAME_FLAG_SHIFT) & DRM_PLAME_FLAG_MASK)

#define DRM_PLANE_FLAG_NONE ((uint32_t)0x00U <<  DRM_PLAME_FLAG_SHIFT)
#define DRM_PLANE_FLAG_TRANSPARENT \
	((uint32_t)0x01U <<  DRM_PLAME_FLAG_SHIFT)
#define DRM_PLANE_FLAG_SKIP_YUV_FORMAT \
	((uint32_t)0x02U <<  DRM_PLAME_FLAG_SHIFT)
#define DRM_PLANE_FLAG_NOT_DEFINED ((uint32_t)0x800U <<  DRM_PLAME_FLAG_SHIFT)

struct tcc_drm_rect {
	unsigned int x, y;
	unsigned int w, h;
};

/*
 * TCC drm plane state structure.
 *
 * @base: plane_state object (contains drm_framebuffer pointer)
 * @src: rectangle of the source image data to be displayed (clipped to
 *       visible part).
 * @crtc: rectangle of the target image position on hardware screen
 *       (clipped to visible part).
 * @h_ratio: horizontal scaling ratio, 16.16 fixed point
 * @v_ratio: vertical scaling ratio, 16.16 fixed point
 *
 * this structure consists plane state data that will be applied to hardware
 * specific overlay info.
 */

struct tcc_drm_plane_state {
	struct drm_plane_state base;
	struct tcc_drm_rect crtc;
	struct tcc_drm_rect src;
	unsigned int h_ratio;
	unsigned int v_ratio;
};


#define TCC_DRM_CRTC_FLIP_STATUS_NONE 0
#define TCC_DRM_CRTC_FLIP_STATUS_PENDING 1
#define TCC_DRM_CRTC_FLIP_STATUS_DONE 2


struct tccdrm_universal_plane_data {
	struct drm_plane *plane;
	enum drm_plane_type plane_type;
	const uint32_t *pixel_formats;
	unsigned int num_pixel_formats;
	const struct drm_plane_funcs *plane_funcs;
	const struct drm_plane_helper_funcs *plane_helper_funcs;
	const uint64_t *format_modifiers;
};

struct tccdrm_crtc_create_data {
	struct drm_crtc *crtc;
	const char *crtc_name;
	struct drm_plane *primary;
	struct drm_plane *cursor;
	const struct drm_crtc_funcs *crtc_funcs;
	const struct drm_crtc_helper_funcs *crtc_helper_funcs;
};

#define TCC_DRM_PLANE_CAP_DOUBLE	(0)
#define TCC_DRM_PLANE_CAP_SCALE		(1)
#define TCC_DRM_PLANE_CAP_ZPOS		(2)
#define TCC_DRM_PLANE_CAP_TILE		(3)
#define TCC_DRM_PLANE_CAP_FBDC		(4)

/* plane flgas */
#define TCC_PLANE_FLAG_FBDC_PLUGGED 	(0)


void tccdrm_drmfmt_to_viocfmt(uint32_t pixel_format,
			      struct vioc_fmt_t *vioc_fmt);

int tcc_prepare_universal_plane(struct drm_device *dev,
			struct tccdrm_universal_plane_data *universal_plane_data);

void tccdrm_plane_reset(struct drm_plane *plane);

struct drm_plane_state *tccdrm_plane_duplicate_state(struct drm_plane *plane);

void tccdrm_plane_destory_state(struct drm_plane *plane,
				struct drm_plane_state *old_drm_pstate);

struct drm_crtc_state *tccdrm_crtc_duplicate_state(struct drm_crtc *crtc);

void tccdrm_crtc_destroy_state(struct drm_crtc *crtc,
			       struct drm_crtc_state *drm_cstate);

int tccdrm_crtc_create(const struct device *dev,
		       struct drm_device *drm,
		       const struct tccdrm_crtc_create_data *crtc_create_data);

void tccdrm_crtc_update_flip_event(struct drm_crtc *crtc,
				   struct tccdrm_flip_state *flip_state);
void tccdrm_crtc_handle_event(struct drm_crtc *crtc,
			      struct tccdrm_flip_state *flip_state);
void tccdrm_crtc_vblank_handler(struct drm_crtc *crtc,
				struct tccdrm_flip_state *flip_state);

struct drm_connector *tccdrm_crtc_get_connector(const struct drm_crtc *crtc);

#endif