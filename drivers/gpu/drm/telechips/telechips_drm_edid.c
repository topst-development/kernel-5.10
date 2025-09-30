// SPDX-License-Identifier: GPL-2.0-or-later

/*
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
#include <drm/drm_crtc.h>
#include <drm/drm_print.h>
#include <drm/drm_edid.h>

#include <linux/tcc_math.h>

#include <video/of_videomode.h>
#include <video/videomode.h>
#include <telechips_drm_types.h>
#include <telechips_drm_crtc_plane_helper.h>
#include <telechips_drm_edid.h>
#include <uapi/drm/telechips_drm.h>

static const u8 base_edid[EDID_LENGTH] = {
	0x00U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x00U,
	0x50U, 0x63U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
	0x01U, 0x1EU, 0x01U, 0x03U, 0x80U, 0x50U, 0x2DU, 0x78U,
	0x1AU, 0x0DU, 0xC9U, 0xA0U, 0x57U, 0x47U, 0x98U, 0x27U,
	0x12U, 0x48U, 0x4CU, 0x00U, 0x00U, 0x00U, 0x01U, 0x01U,
	0x01U, 0x01U, 0x01U, 0x01U, 0x01U, 0x01U, 0x01U, 0x01U,
	0x01U, 0x01U, 0x01U, 0x01U, 0x01U, 0x01U, 0x70U, 0x17U,
	0x80U, 0x00U, 0x70U, 0xD0U, 0x00U, 0x20U, 0x00U, 0x00U,
	0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x18U,
	0x00U, 0x00U, 0x00U, 0xFCU, 0x00U, 0x42U, 0x4FU, 0x45U,
	0x20U, 0x57U, 0x4CU, 0x43U, 0x44U, 0x20U, 0x31U, 0x32U,
	0x2EU, 0x33U, 0x00U, 0x00U, 0x00U, 0x10U, 0x00U, 0x00U,
	0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
	0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x10U,
	0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
	0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
};

/*
 * HIS metric violation (HIS_CCM)
 *  DR <case 2>
 */
int tcc_make_edid_from_display_mode(
	struct edid *target_edid, const struct drm_display_mode *in_mode)
{
	struct detailed_pixel_timing *pixel_data;
	bool internal_ok = (bool)true;
	const u8 *raw_edid = NULL;
	int i, itmp, ret = 0;
	u32 blank, utmp;
	u8 csum;

	if (target_edid == NULL) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}

	if (internal_ok) {
		raw_edid = (u8 *)target_edid;
		pixel_data = &target_edid->detailed_timings[0].data.pixel_data;

		/* Duplicate edid from base_edid */
		(void)memcpy(target_edid, base_edid, EDID_LENGTH);

		/* Set detailed timing information from display mode */
		if (in_mode->clock < 0) {
			DRM_DEV_ERROR(NULL, "mode clock is negative\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		/* edid->detailed_timings[0].pixel_clock =
			cpu_to_le16(DIV_ROUND_UP(in_mode->clock, 10)); */
		itmp = DIV_ROUND_UP(in_mode->clock, 10);
		if (itmp < 0) {
			DRM_DEV_ERROR(NULL, "mode clock divided 10 is negative\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
		if (internal_ok) {
			utmp = (unsigned int)itmp;
			target_edid->detailed_timings[0].pixel_clock =
				cpu_to_le16(utmp);
		}
		/* -- */
	}

	if (internal_ok) {
		/* pixel_data->hactive_lo = in_mode->hdisplay & 0xff; */
		utmp = (unsigned int)in_mode->hdisplay;
		pixel_data->hactive_lo = (u8)(utmp & 0xFFU);
		/* -- */

		/* h blank = (in_mode->htotal - in_mode->hdisplay); */
		itmp = (int)in_mode->htotal - (int)in_mode->hdisplay;
		if (itmp < 0) {
			DRM_DEV_ERROR(NULL, "mode htotal is less than hdisplay\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		} else {
			blank = (unsigned int)itmp;
		}
		/* -- */
	}
	if (internal_ok) {
		pixel_data->hblank_lo = (u8)(blank & 0xFFU);
		/* -- */

		/* pixel_data->hactive_hblank_hi = (in_mode->hdisplay >> 4) & 0xF0U; */

		utmp = (unsigned int)in_mode->hdisplay;
		utmp >>= 4;
		pixel_data->hactive_hblank_hi = (u8)(utmp & 0xF0U);
		/* -- */

		/* pixel_data->hactive_hblank_hi |= ((blank >> 8) & 0xf); */
		utmp = blank >> 8;
		pixel_data->hactive_hblank_hi |= (u8)(utmp & 0xFU);
		/* -- */

		/* pixel_data->vactive_lo = in_mode->vdisplay & 0xff; */
		utmp = (unsigned int)in_mode->vdisplay;;
		pixel_data->vactive_lo = (u8)(utmp & 0xFFU);
		/* -- */

		/* v blank = (in_mode->vtotal - in_mode->vdisplay); */
		itmp = (int)in_mode->vtotal - (int)in_mode->vdisplay;
		if (itmp < 0) {
			DRM_DEV_ERROR(NULL, "mode htotal is less than vtotal\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		} else {
			blank = (unsigned int)itmp;
		}
		/* -- */
	}

	if (internal_ok) {
		/* pixel_data->vblank_lo = blank & 0xff; */
		pixel_data->vblank_lo = (u8)(blank & 0xFFU);
		/* -- */

		/* pixel_data->vactive_vblank_hi = (in_mode->vdisplay >> 4) & 0xF0U; */
		utmp = (unsigned int)in_mode->vdisplay;
		utmp >>= 4;
		pixel_data->vactive_vblank_hi = (u8)(utmp & 0xF0U);
		/* -- */
		/* pixel_data->vactive_vblank_hi |= ((blank >> 8) & 0xf); */
		utmp = blank >> 8;
		pixel_data->vactive_vblank_hi |= (u8)(utmp & 0xFU);
		/* -- */

		/* pixel_data->hsync_offset_lo =
			(in_mode->hsync_start - in_mode->hdisplay) & 0xff; */
		itmp = (int)in_mode->hsync_start - (int)in_mode->hdisplay;
		if (itmp < 0) {
			DRM_DEV_ERROR(NULL, "mode hsync_start is less than hdisplay\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		} else {
			utmp = (unsigned int)itmp;
			pixel_data->hsync_offset_lo = (u8)(utmp & 0xFFU);
		}
		/* -- */
	}
	if (internal_ok) {
		/* pixel_data->hsync_pulse_width_lo =
			(in_mode->hsync_end - in_mode->hsync_start) & 0xff; */
		itmp = (int)in_mode->hsync_end - (int)in_mode->hsync_start;
		if (itmp < 0) {
			DRM_DEV_ERROR(NULL, "mode hsync_end is less than hsync_start\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		} else {
			utmp = (unsigned int)itmp;
			pixel_data->hsync_pulse_width_lo = (u8)(utmp & 0xFFU);
		}
		/* -- */
	}
	if (internal_ok) {
		/* pixel_data->vsync_offset_pulse_width_lo =
			(((in_mode->vsync_start - in_mode->vdisplay) & 0xf) << 4) |
			((in_mode->vsync_end - in_mode->vsync_start) & 0xf); */
		itmp = (int)in_mode->vsync_start - (int)in_mode->vdisplay;
		if (itmp < 0) {
			DRM_DEV_ERROR(NULL, "mode vsync_start is less than vdisplay\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		} else {
			utmp = (unsigned int)itmp;
			utmp = (utmp & 0xfu) << 4;
			pixel_data->vsync_offset_pulse_width_lo = (u8)utmp;
		}
		itmp = (int)in_mode->vsync_end - (int)in_mode->vsync_start;
		if (itmp < 0) {
			DRM_DEV_ERROR(NULL, "mode vsync_end is less than vsync_start\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		} else {
			utmp = (unsigned int)itmp;
			utmp &= 0xfu;
		}
		if (internal_ok) {
			pixel_data->vsync_offset_pulse_width_lo |= (u8)utmp;
		}
		/* -- */
	}
	if (internal_ok) {
		/* pixel_data->hsync_vsync_offset_pulse_width_hi =
			(((in_mode->hsync_start - in_mode->hdisplay) >> 8) << 6) |
			(((in_mode->hsync_end - in_mode->hsync_start) >> 8) << 4) |
			(((in_mode->vsync_start - in_mode->vdisplay) >> 4) << 2) |
			((in_mode->vsync_end - in_mode->vsync_start) >> 4); */
		itmp = (int)in_mode->hsync_start - (int)in_mode->hdisplay;
		if (itmp < 0) {
			DRM_DEV_ERROR(NULL, "mode hsync_start is less than hdisplay\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		} else {
			utmp = (unsigned int)itmp;
			utmp >>= 8;
			utmp = (utmp & 3u) << 6;
			pixel_data->hsync_vsync_offset_pulse_width_hi = (u8)utmp;
		}
		itmp = (int)in_mode->hsync_end - (int)in_mode->hsync_start;
		if (itmp < 0) {
			DRM_DEV_ERROR(NULL, "mode hsync_end is less than hsync_start\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		} else {
			utmp = (unsigned int)itmp;
			utmp >>= 8;
			utmp = (utmp & 3u) << 4;
		}
		if (internal_ok) {
			pixel_data->hsync_vsync_offset_pulse_width_hi |= (u8)utmp;
		}
		itmp = (int)in_mode->vsync_start - (int)in_mode->vdisplay;
		if (itmp < 0) {
			DRM_DEV_ERROR(NULL, "mode vsync_start is less than vdisplay\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		} else {
			utmp = (unsigned int)itmp;
			utmp >>= 4;
			utmp = (utmp & 3u) << 2;
		}
		if (internal_ok) {
			pixel_data->hsync_vsync_offset_pulse_width_hi |= (u8)utmp;
		}
		itmp = (int)in_mode->vsync_end - (int)in_mode->vsync_start;
		if (itmp < 0) {
			DRM_DEV_ERROR(NULL, "mode vsync_end is less than vsync_start\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		} else {
			utmp = (unsigned int)itmp;
			utmp >>= 4;
			utmp &= 3u;
		}
		if (internal_ok) {
			pixel_data->hsync_vsync_offset_pulse_width_hi |= (u8)utmp;
		}
		/* -- */
	}
	if (internal_ok) {
		utmp = (unsigned int)in_mode->width_mm;
		pixel_data->width_mm_lo = (u8)(utmp & 0xFFU);
	}
	if (internal_ok) {
		utmp = (unsigned int)in_mode->height_mm;
		pixel_data->height_mm_lo = (u8)(utmp & 0xFFU);
	}
	if (internal_ok) {
		/* pixel_data->width_height_mm_hi =
			(in_mode->width_mm >> 8) << 4 | in_mode->height_mm >> 8; */
		utmp = (unsigned int)in_mode->width_mm;
		utmp >>= 8;
		utmp <<= 4;
		pixel_data->width_height_mm_hi = (u8)(utmp & 0xf0u);

		utmp = (unsigned int)in_mode->height_mm;
		utmp >>= 8;

		pixel_data->width_height_mm_hi |= (u8)(utmp & 0xfu);
		/* -- */
	}
	if (internal_ok) {
		pixel_data->hborder = 0x0U;
		pixel_data->vborder = 0x0U;
		/* coverity[misra_c_2012_rule_10_1] */
		utmp = DRM_EDID_PT_SEPARATE_SYNC;
		pixel_data->misc = (u8)(utmp & 0xFFU);
		/* coverity[misra_c_2012_rule_10_1] */
		utmp = DRM_MODE_FLAG_PHSYNC;
		if ((in_mode->flags & utmp) == utmp) {
			/* coverity[misra_c_2012_rule_10_1] */
			utmp = DRM_EDID_PT_HSYNC_POSITIVE;
			pixel_data->misc |= (u8)(utmp & 0xFFU);
		}
		/* coverity[misra_c_2012_rule_10_1] */
		utmp = DRM_MODE_FLAG_PVSYNC;
		if ((in_mode->flags & utmp) == utmp) {
			/* coverity[misra_c_2012_rule_10_1] */
			utmp = DRM_EDID_PT_VSYNC_POSITIVE;
			pixel_data->misc |= (u8)(utmp & 0xFFU);
		}

		/* Checksum */
		csum = 0;
		for (i = 0; i < EDID_LENGTH; i++) {
			utmp = csum;
			if (!tcc_math_check_uint_plus_uint(utmp, raw_edid[i])) {
				ret = -EINVAL;
				break;
			}
			utmp += raw_edid[i];
			csum = (u8)(utmp & 0xFFU);
		}
		if (csum == (u8)0U) {
			target_edid->checksum = 1;
		} else {
			target_edid->checksum = ((u8)0xFFU - csum)+ (u8)1U;
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
EXPORT_SYMBOL_GPL(tcc_make_edid_from_display_mode);

int tccdrm_parse_edid_from_crtc_id_ioctl(
	/* coverity[misra_c_2012_rule_8_13] */
	struct drm_device *dev, void *data, struct drm_file *infile)
{
	const struct drm_crtc *crtc;
	/* coverity[misra_c_2012_rule_11_5] */
	struct drm_tcc_edid *args = (void __user *)data;
	const struct drm_property_blob *edid_blob = NULL;
	const struct drm_connector *connector;
	struct edid *edid_ptr = NULL;
	bool need_make_edid = (bool)false;
	bool need_free_edid = (bool)false;
	bool internal_ok = (bool)true;
	int ret = 0;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter infile is not
	 * used in the function.
	 */
	(void)infile;

	if (dev == NULL) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}

	if (internal_ok &&
	    (dev->dev == NULL)) {
		ret = -EINVAL;
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		DRM_DEV_INFO(dev->dev,
			     "[INFO] Ioctl called\n");

		/* get crtc */
		#if defined(CONFIG_REFCODE_PRE_K54)
		crtc = drm_crtc_find(dev, args->crtc_id);
		#else
		crtc = drm_crtc_find(dev, infile, args->crtc_id);
		#endif
		if (crtc == NULL) {
			DRM_DEV_ERROR(dev->dev, "Invalid CRTC ID \r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}

	if (internal_ok) {
		DRM_DEV_INFO(dev->dev,
			     "[INFO] CRTC[%d]\n", args->crtc_id);

		if (crtc->state == NULL) {
			DRM_DEV_ERROR(dev->dev, "CRTC is not ready\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		connector = tccdrm_crtc_get_connector(crtc);
		if (connector == NULL) {
			internal_ok = (bool)false;
			ret = -ENODEV;
		}
	}
	if (internal_ok) {
		edid_blob = connector->edid_blob_ptr;

		if (edid_blob != NULL) {
			(void)memcpy(args->data, edid_blob->data,
				     edid_blob->length);
			need_make_edid = (bool)true;
		}
	}
	/* if connector has no edid_blob then It makes edid as below: */
	if (need_make_edid) {
		/* coverity[misra_c_2012_rule_10_8] */
		/* coverity[misra_c_2012_rule_11_5] */
		edid_ptr =
			devm_kzalloc(dev->dev, EDID_LENGTH, GFP_KERNEL);

		if (edid_ptr == NULL) {
			internal_ok = (bool)false;
			ret = -ENOMEM;
		} else {
			need_free_edid = (bool)true;
		}

		if (internal_ok) {
			const struct drm_crtc_state *crtc_state = crtc->state;
			const struct drm_display_mode *modes =
				(const struct drm_display_mode *)&crtc_state->mode;

			if (tcc_make_edid_from_display_mode(edid_ptr,
							    modes) < 0) {
				internal_ok = (bool)false;
				ret = -EINVAL;
			}
		}
		if (internal_ok) {
			(void)memcpy(args->data, edid_ptr, EDID_LENGTH);
		}
	}
	if (need_free_edid) {
		devm_kfree(dev->dev, edid_ptr);
		edid_ptr = NULL;
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
EXPORT_SYMBOL(tccdrm_parse_edid_from_crtc_id_ioctl);