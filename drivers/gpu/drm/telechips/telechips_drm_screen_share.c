// SPDX-License-Identifier: GPL-2.0-or-later

/* telechips_drm_screen_share.c
 *
 * Copyright (c) 2022 Telechips Inc.
 * Authors:
 *	Jayden Kim
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#if defined(CONFIG_REFCODE_PRE_K510)
#include <drm/drmP.h>
#endif
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/component.h>
#include <linux/regmap.h>
#include <linux/tcc_math.h>
#include <video/of_display_timing.h>
#include <video/of_videomode.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic_state_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_vblank.h>
#include <drm/drm_print.h>
#include <drm/drm_panel.h>
#include <drm/telechips_drm.h>

#include <video/telechips/vioc_global.h>
#include <video/telechips/tcc_types.h>
#include <video/telechips/vioc_intr.h>
#include <video/telechips/vioc_timer.h>
#include <tcc_shared_buffer.h>

#include <telechips_drm_types.h>
#include <telechips_drm_drv.h>
#include <telechips_drm_fb.h>
#include <telechips_drm_edid.h>
#include <telechips_drm_vioc.h>
#include <telechips_drm_crtc_plane_helper.h>
#include <telechips_drm_screen_share.h>

#define DRIVER_DATE	"20240227"
#define DRIVER_MAJOR 2
#define DRIVER_MINOR 1
#define DRIVER_PATCH 1

/* DEFINES for PLANE ---------------------------------------------------------*/
#define to_tcc_plane_state(plane_state) \
	container_of((plane_state), struct tcc_drm_plane_state, base)

#define to_tccdrm_screen_share_context(x) \
	container_of(x, struct tccdrm_screen_share_context, crtc)

struct tccdrm_screen_share_context {
	struct device *dev;
	struct drm_device *drm;

	struct drm_crtc crtc;
	struct tcc_drm_plane plane;
	unsigned long crtc_flags;
	struct tccdrm_flip_state flip_state;

	/* Whether the CRTC enabled - false disabled, true enabled */
	bool crtc_enabled;

	/* Whether the dev binded - false not binded, true binded */
	bool dev_binded;

	/* Display Disable Done */
	wait_queue_head_t wait_display_done_queue;
	atomic_t wait_display_done_event;

	/* VIOC TIMER */
	unsigned int vrefresh;
	enum vioc_timer_id timer_id;
	#if defined(CONFIG_CHECK_DRM_TELECHIPS_VSYNC_TIME_GAP)
	struct timeval prev_time;
	#endif

	spinlock_t irq_lock;

	/* timer irq */
	unsigned int irq_num;

	#if defined(CONFIG_SMP)
	const struct cpumask *irq_cpumask;
	#endif
};

/* FUNCTIONS for DEVICE-TREE--------------------------------------------------*/

/* FUNCTIONS for PLANE -------------------------------------------------------*/
static int tccdrm_ss_plane_mode_set(struct tcc_drm_plane_state *tcc_pstate)
{
	const struct drm_plane_state *drm_pstate =
		(const struct drm_plane_state *)&tcc_pstate->base;
	unsigned int src_x, src_y, src_w, src_h;
	unsigned int crtc_w, crtc_h, utmp;
	int crtc_x, crtc_y, itmp;
	bool internal_ok = (bool)true;
	int ret = 0;

	/*
	 * The original src/dest coordinates are stored in tcc_pstate->base,
	 * but we want to keep another copy internal to our driver that we can
	 * clip/modify ourselves.
	 */
	crtc_x = drm_pstate->crtc_x;
	crtc_y = drm_pstate->crtc_y;
	crtc_w = drm_pstate->crtc_w;
	crtc_h = drm_pstate->crtc_h;

	/* Source parameters given in 16.16 fixed point, ignore fractional. */
	src_x = drm_pstate->src_x >> 16;
	src_y = drm_pstate->src_y >> 16;
	src_w = drm_pstate->src_w >> 16;
	src_h = drm_pstate->src_h >> 16;

	if (tcc_math_uint_gt_intmax(crtc_w)) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}

	if (internal_ok) {
		if (tcc_math_uint_gt_intmax(crtc_h)) {
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}

	if (internal_ok) {
		if (crtc_x < 0) {
			/* src_x += -crtc_x; */
			itmp = -crtc_x;
			utmp = (unsigned int)itmp;
			src_x += utmp;

			/* crtc_w += crtc_x; */
			if (crtc_w < utmp) {
				internal_ok = (bool)false;
				ret = -EINVAL;
			} else {
				crtc_w -= utmp;
				crtc_x = 0;
			}
		}
	}
	if (internal_ok) {
		if (crtc_y < 0) {
			/* src_y += -crtc_y; */
			itmp = -crtc_y;
			utmp = (unsigned int)itmp;
			src_y += utmp;

			/* crtc_h += crtc_y; */
			if (crtc_h < utmp) {
				internal_ok = (bool)false;
				ret = -EINVAL;
			} else {
				crtc_h -= utmp;
				crtc_y = 0;
			}
		}
	}

	/* set drm framebuffer data. */
	if (internal_ok) {
		tcc_pstate->src.x = src_x;
		tcc_pstate->src.y = src_y;
		tcc_pstate->src.w = src_w;
		tcc_pstate->src.h = src_h;
	}

	/* set plane range to be displayed. */
	if (internal_ok) {
		/* tcc_pstate->crtc.x = crtc_x; */
		utmp = (unsigned int)crtc_x;
		tcc_pstate->crtc.x = utmp;

		/* tcc_pstate->crtc.y = crtc_y; */
		utmp = (unsigned int)crtc_y;
		tcc_pstate->crtc.y = utmp;

		tcc_pstate->crtc.w = crtc_w;
		tcc_pstate->crtc.h = crtc_h;
	}

	return ret;
}

static const struct drm_plane_funcs tccdrm_ss_plane_funcs = {
	.update_plane	= drm_atomic_helper_update_plane,
	.disable_plane	= drm_atomic_helper_disable_plane,
	.destroy	= drm_plane_cleanup,
	.reset		= tccdrm_plane_reset,
	.atomic_duplicate_state = tccdrm_plane_duplicate_state,
	.atomic_destroy_state = tccdrm_plane_destory_state,
};

/*
 * HIS metric violation (HIS_CCM)
 *  DR <case 2>
 */
/* coverity[HIS_metric] - HIS_CCM */
/* coverity[misra_c_2012_rule_8_13] */
static int tccdrm_ss_plane_check(struct drm_plane *plane,
				 struct drm_plane_state *drm_pstate)
{
	struct tcc_drm_plane_state *tcc_pstate;
	const struct drm_crtc_state *crtc_state;

	bool internal_ok = (bool)true;
	int crtc_w, crtc_h, itmp;
	int ret = 0;

	if (drm_pstate == NULL) {
		internal_ok = (bool)false;
		ret = -EINVAL;
		DRM_DEV_INFO(plane->dev->dev,
			     "[WARN] drm_pstate is NULL with err(%d)\r\n", ret);
	}

	if (internal_ok) {
		tcc_pstate =
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
			(struct tcc_drm_plane_state *)to_tcc_plane_state(drm_pstate);
		if (drm_pstate->state == NULL) {
			DRM_DEV_INFO(plane->dev->dev,
				     "[WARN]drm_pstate->state is NULL with err(%d)\r\n",
				     ret);
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}

	if (internal_ok &&
		((drm_pstate->crtc == NULL) || (drm_pstate->fb == NULL))) {
		/*
		 * There is no need for further checks if the plane is
		 * being disabled
		 */
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		crtc_state =
			drm_atomic_get_existing_crtc_state(drm_pstate->state,
							   drm_pstate->crtc);
		if (crtc_state == NULL) {
			DRM_DEV_INFO(plane->dev->dev,
				     "[WARN] crtc_state is NULL with err(%d)\r\n",
				     ret);
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}

	if (internal_ok) {
		switch (drm_pstate->fb->modifier) {
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_10_4] */
		/* coverity[misra_c_2012_rule_10_7] */
		/* coverity[misra_c_2012_rule_20_7] */
		case DRM_FORMAT_MOD_LINEAR:
			//DRM_DEV_INFO(plane->dev->dev,
			//	     "[INFO] plane[%d] is support DRM_FORMAT_MOD_LINEAR\r\n",
			//	     tcc_plane->win);
			break;
		default:
			DRM_DEV_ERROR(plane->dev->dev,
				      "[%llx] is not supported\r\n",
				      drm_pstate->fb->modifier);
			internal_ok = (bool)false;
			ret = -EINVAL;
			break;
		}
	}

	if (internal_ok &&
		((drm_pstate->crtc == NULL) || (drm_pstate->fb == NULL))) {
		DRM_DEV_ERROR(plane->dev->dev, "Error crtc or fb is NULL\r\n");
		internal_ok = (bool)false;
		ret = -EINVAL;
	}

	if (internal_ok) {
		if ((tcc_math_uint_gt_intmax(drm_pstate->crtc_w))) {
			DRM_DEV_ERROR(plane->dev->dev,
				      "drm_pstate->crtc_w is out of integer range\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}

	if (internal_ok) {
		if ((tcc_math_uint_gt_intmax(drm_pstate->crtc_h))) {
			DRM_DEV_ERROR(plane->dev->dev,
				      "drm_pstate->crtc_h is out of integer range\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}

	/*
	 * condition
	 * drm_pstate->crtc_x + drm_pstate->crtc_w >
	 * 	crtc_plane_state->adjusted_mode.hdisplay
	 */
	if (internal_ok) {
		crtc_w = (int)drm_pstate->crtc_w;
		if (tcc_math_check_int_plus_int(drm_pstate->crtc_x, crtc_w) != 0) {
			DRM_DEV_ERROR(plane->dev->dev,
				      "result of crtc_x + crtc_w is out of display area\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		itmp = drm_pstate->crtc_x + crtc_w;
		if (itmp < 0) {
			DRM_DEV_ERROR(plane->dev->dev,
				      "result of crtc_x + crtc_w is invalid\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		if (itmp > (int)crtc_state->adjusted_mode.hdisplay) {
			DRM_DEV_ERROR(plane->dev->dev,
				      "result of crtc_x + crtc_w is out of display area\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	/*
	 * condition end
	 * drm_pstate->crtc_x + drm_pstate->crtc_w >
	 * 	crtc_plane_state->adjusted_mode.hdisplay
	 */

	/*
	 * condition
	 * drm_pstate->crtc_y + drm_pstate->crtc_h >
	 * 	crtc_plane_state->adjusted_mode.vdisplay
	 */
	if (internal_ok) {
		crtc_h = (int)drm_pstate->crtc_h;
		if (tcc_math_check_int_plus_int(drm_pstate->crtc_y, crtc_h) != 0) {
			DRM_DEV_ERROR(plane->dev->dev,
				      "result of crtc_y + crtc_h is out of display area\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		itmp = drm_pstate->crtc_y + crtc_h;
		if (itmp < 0) {
			DRM_DEV_ERROR(plane->dev->dev,
				      "result of crtc_y + crtc_h is invalid\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		if (itmp > (int)crtc_state->adjusted_mode.vdisplay) {
			DRM_DEV_ERROR(plane->dev->dev,
				      "result of crtc_y + crtc_h is out of display area\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	/*
	 * condition end
	 * drm_pstate->crtc_y + drm_pstate->crtc_h >
	 * 	crtc_plane_state->adjusted_mode.vdisplay
	 */

	if (internal_ok && ((drm_pstate->src_w >> 16U) != drm_pstate->crtc_w)) {
		DRM_DEV_ERROR(plane->dev->dev,
			      "mismatch %d with %d scaling mode is not supported\r\n",
			      drm_pstate->src_w >> 16, drm_pstate->crtc_w);
		internal_ok = (bool)false;
		ret = -ENOTSUPP;
	}

	if (internal_ok && ((drm_pstate->src_h >> 16U) != drm_pstate->crtc_h)) {
		DRM_DEV_ERROR(plane->dev->dev,
			      "mismatch %d with %d scaling mode is not supported\r\n",
			      drm_pstate->src_h >> 16, drm_pstate->crtc_h);
		internal_ok = (bool)false;
		ret = -ENOTSUPP;
	}
	if (internal_ok) {
		/* translate drm_pstate into tcc_pstate */
		ret = tccdrm_ss_plane_mode_set(tcc_pstate);
	}

	return ret;
}

static unsigned int tccdrm_ss_get_dma_address(const struct tccdrm_screen_share_context *dev_context,
					      const struct tcc_drm_plane_state *plane_state,
					      unsigned int win)
{
	struct drm_framebuffer *fb = plane_state->base.fb;
	unsigned int cpp = fb->format->cpp[0];
	unsigned int pitch = fb->pitches[0];
	unsigned int utmp;

	bool internal_ok = (bool)true;
	unsigned int dma_addr32 = 0U;
	unsigned int dma_offset;
	dma_addr_t dma_addr;

	/*
	 * offset = state->src.x * cpp;
	 * offset += state->src.y * pitch;
	 * dma_addr = tccdrm_fb_dma_addr(fb, 0) + offset;
	 */

	/* offset = state->src.x * cpp; */
	if (!tcc_math_check_uint_mul_uint(plane_state->src.x, cpp)) {
		internal_ok = (bool)false;
	} else {
		dma_offset = plane_state->src.x * cpp;
	}
	/* -- */

	/* offset += state->src.y * pitch; */
	if (internal_ok) {
		if (!tcc_math_check_uint_mul_uint(plane_state->src.y, pitch)) {
			internal_ok = (bool)false;
		} else {
			utmp = plane_state->src.y * pitch;
		}
	}
	if (internal_ok) {
		if (!tcc_math_check_uint_plus_uint(dma_offset, utmp)) {
			internal_ok = (bool)false;
		} else {
			dma_offset += utmp;
		}
	}
	/* -- */

	/* dma_addr = tccdrm_fb_dma_addr(fb, 0) + offset; */
	if (internal_ok) {
		dma_addr = tccdrm_fb_dma_addr(fb, 0) + dma_offset;
		if (dma_addr == (dma_addr_t)0) {
			DRM_DEV_ERROR(dev_context->dev,
				"dma address of win(%d) is NULL\r\n", win);
			internal_ok = (bool)false;
		}
	}
	/* -- */

	if (internal_ok) {
		if (upper_32_bits(dma_addr) > 0U) {
			DRM_DEV_ERROR(dev_context->dev,
				      "dma address of win(%d) is out of range\r\n",
				      win);
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		/* buffer start address */
		dma_addr32 = lower_32_bits(dma_addr);

	}
	return dma_addr32;
}

/* coverity[misra_c_2012_rule_8_13] */ //
static void tccdrm_plane_update(struct drm_plane *plane,
				/* coverity[misra_c_2012_rule_8_13] */ //
				struct drm_plane_state *old_drm_pstate)
{
	struct drm_plane_state *drm_pstate =
		(struct drm_plane_state *)plane->state;
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */ //
	/* coverity[misra_c_2012_rule_8_6] */ //
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_screen_share_context *dev_context = (struct tccdrm_screen_share_context *)to_tccdrm_screen_share_context(drm_pstate->crtc);

	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */ //
	/* coverity[misra_c_2012_rule_8_6] */ //
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tcc_drm_plane *tcc_plane = to_tcc_plane(plane);

	const struct tcc_drm_plane_state *tcc_pstate =
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
		(const struct tcc_drm_plane_state *)to_tcc_plane_state(drm_pstate);

	const struct drm_framebuffer *fb = tcc_pstate->base.fb;
	unsigned int win = tcc_plane->win;
	bool internal_ok = (bool)true;
	struct vioc_fmt_t vioc_fmt;
	unsigned int dma_addr32;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter old_drm_pstate is not
	 * used in the function.
	 */
	(void)old_drm_pstate;

	if (drm_pstate->crtc != NULL) {
		#if defined(CONFIG_REFCODE_PRE_K54)
		plane->crtc = tcc_pstate->crtc;
		#endif

		if (win >= 1U) {
			DRM_DEV_ERROR(dev_context->dev,
				"win(%u) is out of range 1\r\n", win);
			internal_ok = (bool)false;
		}
		if (internal_ok) {
			dma_addr32 = tccdrm_ss_get_dma_address((const struct tccdrm_screen_share_context *)dev_context, tcc_pstate, win);
			if (dma_addr32 == 0U) {
				internal_ok = (bool)false;
			}
		}
		if (internal_ok) {
			tccdrm_drmfmt_to_viocfmt(fb->format->format,
						 &vioc_fmt);

			tcc_scrshare_set_sharedBuffer(dma_addr32,
						      tcc_pstate->crtc.w,
						      tcc_pstate->crtc.h,
						      vioc_fmt.f_fmt,
						      vioc_fmt.f_swap);
		}
	}
}

/* coverity[misra_c_2012_rule_8_13] */ //
static void tccdrm_plane_disable(struct drm_plane *plane,
				/* coverity[misra_c_2012_rule_8_13] */ //
				struct drm_plane_state *old_drm_pstate)
{
	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter plane is not
	 * used in the function.
	 */
	(void)plane;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter old_drm_pstate is not
	 * used in the function.
	 */
	(void)old_drm_pstate;
}

/* plane helper funcs -------------------------------------------------------*/
static const struct drm_plane_helper_funcs tccdrm_ss_plane_helper_funcs = {
	.prepare_fb =  drm_gem_fb_prepare_fb,
	.atomic_check = tccdrm_ss_plane_check,
	.atomic_update = tccdrm_plane_update,
	.atomic_disable = tccdrm_plane_disable,
};

/* FUNCTIONS for CRTC --------------------------------------------------------*/
static int tccdrm_ss_crtc_atomic_check(struct drm_crtc *crtc,
				     struct drm_crtc_state *drm_cstate)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */ //
	/* coverity[misra_c_2012_rule_8_6] */ //
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_screen_share_context *dev_context = (struct tccdrm_screen_share_context *)to_tccdrm_screen_share_context(crtc);
	const struct drm_display_mode *adjusted_mode =
		(const struct drm_display_mode *)&drm_cstate->adjusted_mode;
	bool internal_ok = (bool)true;
	int vrefresh, ret = 0;

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

	if (drm_cstate->enable) {
		if (tcc_cstate->connector_type != DRM_MODE_CONNECTOR_VIRTUAL) {
			DRM_DEV_INFO(dev_context->dev,
				"The connector attached to this crtc is not virtual connector.\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
		if (internal_ok) {
			if (adjusted_mode->clock <= 0) {
				DRM_DEV_INFO(
					dev_context->dev,
					"[WARN] Mode has zero clock value.\n");
				internal_ok = (bool)false;
				ret = -EINVAL;
			}
		}
		if (internal_ok) {
			vrefresh = drm_mode_vrefresh(adjusted_mode);
			if (vrefresh < 0) {
				ret = -EINVAL;
			} else {
				dev_context->vrefresh = (unsigned int)vrefresh;
			}
		}
	}

	return ret;
}

/* coverity[misra_c_2012_rule_8_13] */
static void tccdrm_ss_crtc_atomic_begin(struct drm_crtc *crtc,
				  /* coverity[misra_c_2012_rule_8_13] */
				  struct drm_crtc_state *old_drm_cstate)
{

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter crtc is not
	 * used in the function.
	 */
	(void)crtc;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter old_drm_cstate is not
	 * used in the function.
	 */
	(void)old_drm_cstate;
}

static void tccdrm_ss_crtc_atomic_flush(struct drm_crtc *crtc,
				  /* coverity[misra_c_2012_rule_8_13] */
				  struct drm_crtc_state *old_drm_cstate)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */ //
	/* coverity[misra_c_2012_rule_8_6] */ //
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_screen_share_context *dev_context = (struct tccdrm_screen_share_context *)to_tccdrm_screen_share_context(crtc);

	if (old_drm_cstate->active) {
		tccdrm_crtc_handle_event(crtc, &dev_context->flip_state);
	} else {
		DRM_DEV_DEBUG(dev_context->dev,
			      "[DEBUG] old crtc state is not avtive\r\n");
	}
}

static void tccdrm_ss_crtc_atomic_enable(struct drm_crtc *crtc,
					 /* coverity[misra_c_2012_rule_8_13] */
					 struct drm_crtc_state *old_cstate)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */ //
	/* coverity[misra_c_2012_rule_8_6] */ //
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_screen_share_context *dev_context = (struct tccdrm_screen_share_context *)to_tccdrm_screen_share_context(crtc);

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter old_drm_cstate is not
	 * used in the function.
	 */
	(void)old_cstate;


	if (!dev_context->crtc_enabled) {
		DRM_DEV_INFO(dev_context->dev, "[INFO] Turn on\r\n");
		#if defined(CONFIG_PM)
		(void)pm_runtime_get_sync(dev_context->dev);
		#endif
		dev_context->crtc_enabled = (bool)true;
	}
	drm_crtc_vblank_on(crtc);
	if (crtc->state->event != NULL) {
		/* coverity[misra_c_2012_rule_10_3] */
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_15_6] */
		WARN_ON(drm_crtc_vblank_get(crtc) != 0);
		tccdrm_crtc_update_flip_event(crtc, &dev_context->flip_state);
	}
}

/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 2>
 */
/* coverity[HIS_metric] - HIS_CALLS */
/* coverity[misra_c_2012_rule_8_13] */
static void tccdrm_ss_crtc_atomic_disable(struct drm_crtc *crtc,
					/* coverity[misra_c_2012_rule_8_13] */
					struct drm_crtc_state *old_cstate)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */ //
	/* coverity[misra_c_2012_rule_8_6] */ //
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_screen_share_context *dev_context = (struct tccdrm_screen_share_context *)to_tccdrm_screen_share_context(crtc);

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter old_cstate is not
	 * used in the function.
	 */
	(void)old_cstate;

	DRM_DEV_INFO(dev_context->dev, "[INFO] Turn off\r\n");

	#if defined(CONFIG_PM)
	if (dev_context->crtc_enabled) {
		(void)pm_runtime_put_sync(dev_context->dev);
	}
	#endif

	dev_context->crtc_enabled = (bool)false;

	drm_crtc_vblank_off(crtc);

	if (crtc->state->event != NULL) {
		unsigned long irq_flags;

		/* coverity[cert_dcl37_c] */
		/* coverity[misra_c_2012_rule_14_4] */
		spin_lock_irqsave(&crtc->dev->event_lock, irq_flags);
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		spin_unlock_irqrestore(&crtc->dev->event_lock, irq_flags);
		crtc->state->event = NULL;
	}
}

/* crtc helper funcs --------------------------------------------------------*/
static const struct drm_crtc_helper_funcs tccdrm_ss_crtc_helper_funcs = {
//	.mode_set_nofb = NULL,
	.atomic_check	= tccdrm_ss_crtc_atomic_check,
	.atomic_begin	= tccdrm_ss_crtc_atomic_begin,
	.atomic_flush	= tccdrm_ss_crtc_atomic_flush,
	.atomic_enable	= tccdrm_ss_crtc_atomic_enable,
	.atomic_disable	= tccdrm_ss_crtc_atomic_disable,
};

static void tccdrm_ss_crtc_destory(struct drm_crtc *crtc)
{
	drm_crtc_cleanup(crtc);
}

/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 2>
 */
/* coverity[HIS_metric] - HIS_CALLS */
static int tccdrm_ss_enable_irq_of_timer(struct tccdrm_screen_share_context *dev_context,
					     int irq_num, int vioc_intr_inum)
{
	unsigned long irqflags;

	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_14_4] */
	spin_lock_irqsave(&dev_context->irq_lock, irqflags);
	if (test_and_set_bit(CRTC_FLAGS_IRQ_BIT,
			     &dev_context->crtc_flags) != 1) {
		#if defined(CONFIG_CHECK_DRM_TELECHIPS_VSYNC_TIME_GAP)
		do_gettimeofday(&dev_context->prev_time);
		#endif
		#if defined(CONFIG_SMP)
		(void)irq_set_affinity_hint(
			dev_context->irq_num,
			dev_context->irq_cpumask);
		#endif
		(void)vioc_timer_clear_irq_status(dev_context->timer_id);
		(void)vioc_timer_clear_irq_mask(dev_context->timer_id);
		(void)vioc_intr_enable(irq_num, vioc_intr_inum, VIOC_DISP_INTR_DISPLAY);

		DRM_DEV_DEBUG(dev_context->dev,
				"enable interrupt for display device irq(%d), blk(%d)\r\n",
				irq_num, vioc_intr_inum);

		if ((dev_context->vrefresh >= 24U) &&
			(dev_context->vrefresh <= 120U)) {
			DRM_DEV_INFO(dev_context->dev,
					"[INFO] with vrefresh(%u)\r\n",
					dev_context->vrefresh);
			(void)vioc_timer_set_timer(dev_context->timer_id, 1, dev_context->vrefresh);
		} else {
			DRM_DEV_INFO(
				dev_context->dev,
				"[INFO] It with force 30Hz\r\n");
			(void)vioc_timer_set_timer(dev_context->timer_id, 1, 30U);
		}
	}
	spin_unlock_irqrestore(&dev_context->irq_lock, irqflags);

	return 0;
}

static int tccdrm_ss_disable_irq_of_timer(struct tccdrm_screen_share_context *dev_context,
					      int irq_num, int vioc_intr_inum)
{
	unsigned long irqflags;

	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_14_4] */
	spin_lock_irqsave(&dev_context->irq_lock, irqflags);
	if (test_and_clear_bit(CRTC_FLAGS_IRQ_BIT,
			       &dev_context->crtc_flags) == 1) {
		DRM_DEV_DEBUG(dev_context->dev,
				"disable interrupt for display device irq(%d), blk(%d)\r\n",
				irq_num, vioc_intr_inum);
		(void)vioc_timer_set_irq_mask(dev_context->timer_id);
	}
	spin_unlock_irqrestore(&dev_context->irq_lock, irqflags);

	return 0;
}

static struct drm_crtc_state *tccdrm_ss_crtc_duplicate_state(struct drm_crtc *crtc)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */ //
	/* coverity[misra_c_2012_rule_8_6] */ //
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */

	const struct tccdrm_screen_share_context *dev_context =
		(const struct tccdrm_screen_share_context *)to_tccdrm_screen_share_context(crtc);
	struct tcc_crtc_state *tcc_cstate, *tcc_old_cstate;
	struct drm_crtc_state *crtc_state = NULL;

	/* coverity[misra_c_2012_rule_10_8] */
    /* coverity[misra_c_2012_rule_11_5] */
	tcc_cstate = kzalloc(sizeof(*tcc_cstate), GFP_KERNEL);
	if (tcc_cstate != NULL) {
		tcc_old_cstate = to_tcc_crtc_state(crtc->state);

		__drm_atomic_helper_crtc_duplicate_state(crtc, &tcc_cstate->base);
		crtc_state = &tcc_cstate->base;
		tcc_cstate->dev = dev_context->dev;
		tcc_cstate->connector_type = tcc_old_cstate->connector_type;
	}

	return crtc_state;

}

static int tccdrm_ss_crtc_enable_vblank(struct drm_crtc *crtc)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */ //
	/* coverity[misra_c_2012_rule_8_6] */ //
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_screen_share_context *dev_context = (struct tccdrm_screen_share_context *)to_tccdrm_screen_share_context(crtc);
	bool internal_ok = (bool)true;
	int vioc_intr_inum, irq_num;
	int ret = 0;

	if (tcc_math_uint_gt_intmax(dev_context->irq_num)) {
		DRM_DEV_ERROR(dev_context->dev, "display device irq number is out of range\r\n");
		internal_ok = (bool)false;
		ret = -EINVAL;
	} else {
		irq_num = (int)dev_context->irq_num;
	}
	if (internal_ok) {
		vioc_intr_inum = (int)VIOC_INTR_TIMER;
		ret = tccdrm_ss_enable_irq_of_timer(dev_context,
							irq_num, vioc_intr_inum);
	}
	return ret;
}


static void tccdrm_ss_crtc_disable_vblank(struct drm_crtc *crtc)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */ //
	/* coverity[misra_c_2012_rule_8_6] */ //
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_screen_share_context *dev_context = (struct tccdrm_screen_share_context *)to_tccdrm_screen_share_context(crtc);
	bool internal_ok = (bool)true;
	int vioc_intr_inum, irq_num;

	if (tcc_math_uint_gt_intmax(dev_context->irq_num)) {
		DRM_DEV_ERROR(dev_context->dev, "display device irq number is out of range\r\n");
		internal_ok = (bool)false;
	} else {
		irq_num = (int)dev_context->irq_num;
	}
	if (internal_ok) {
		vioc_intr_inum = (int)VIOC_INTR_TIMER;
		(void)tccdrm_ss_disable_irq_of_timer(dev_context, irq_num,
						     vioc_intr_inum);
	}
}

static const struct drm_crtc_funcs tccdrm_ss_crtc_funcs = {
	.set_config = drm_atomic_helper_set_config,
	.page_flip = drm_atomic_helper_page_flip,
	.destroy = tccdrm_ss_crtc_destory,
	.reset = drm_atomic_helper_crtc_reset,
	.atomic_duplicate_state = tccdrm_ss_crtc_duplicate_state,
	.atomic_destroy_state = tccdrm_crtc_destroy_state,
	.enable_vblank = tccdrm_ss_crtc_enable_vblank,
	.disable_vblank = tccdrm_ss_crtc_disable_vblank,
};

/* FUNCTIONS for LCDC --------------------------------------------------------*/
/*
 * LCD stands for Fully Interactive Various Display and
 * as a display controller, it transfers contents drawn on memory
 * to a LCD Panel through Display Interfaces such as RGB or
 * CPU Interface.
 */

static const uint32_t tccdrm_ss_formats[] = {
	DRM_FORMAT_BGR565,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_BGR888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_NV12,
	DRM_FORMAT_NV21,
	DRM_FORMAT_YUV420,
	DRM_FORMAT_YVU420,
};

static irqreturn_t screen_share_irq_handler(int irq, void *dev_id)
{
	/* coverity[misra_c_2012_rule_11_5] */
	struct tccdrm_screen_share_context *dev_context =
		(struct tccdrm_screen_share_context *)dev_id;
	#if defined(CONFIG_CHECK_DRM_TELECHIPS_VSYNC_TIME_GAP)
	unsigned int diff_ms;
	struct timeval cur_time;
	#endif
	bool internal_ok = (bool)true;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter irq is not used in
	 * the function.
	 */
	(void)irq;

	if (dev_context == NULL) {
		DRM_DEV_INFO(NULL, "[WARN]dev_context is NULL\r\n");
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		if (vioc_timer_is_interrupted(dev_context->timer_id) != 0) {
			#if defined(CONFIG_CHECK_DRM_TELECHIPS_VSYNC_TIME_GAP)
			unsigned int base_v, min_v, max_v;
			#endif

			tccdrm_crtc_vblank_handler(&dev_context->crtc,
						   &dev_context->flip_state);

			#if defined(CONFIG_CHECK_DRM_TELECHIPS_VSYNC_TIME_GAP)
			do_gettimeofday(&cur_time);
			diff_ms = tcc_time_diff_ms(
				(const struct device*)dev_context->dev,
				(const struct timeval *)&dev_context->prev_time,
				(const struct timeval *)&cur_time);

			if (!tcc_math_check_uint_plus_uint(999U, dev_context->vrefresh)) {
				DRM_DEV_INFO(dev_context->dev,
					     "Invalid value at line(%d)\r\n",
					     __LINE__);
				internal_ok = (bool)false;
			} else {
				/* coverity[misra_c_2012_rule_10_4] */
				base_v = DIV_ROUND_UP(1000U, dev_context->vrefresh);
			}

			if (internal_ok) {
				if (!tcc_math_check_uint_minus_uint(base_v, 1U)) {
					DRM_DEV_INFO(dev_context->dev,
						     "Invalid value at line(%d)\r\n",
						     __LINE__);
					internal_ok = (bool)false;
				} else {
					min_v = base_v - 1U;
				}
			}
			if (internal_ok) {
				if (!tcc_math_check_uint_plus_uint(base_v, 1U)) {
					DRM_DEV_INFO(dev_context->dev,
						     "Invalid value at line(%d)\r\n",
						     __LINE__);
					internal_ok = (bool)false;
				} else {
					max_v = base_v + 1U;
				}
			}
			if (internal_ok) {
				if ((diff_ms < min_v) || (diff_ms > max_v)) {
					DRM_DEV_INFO(dev_context->dev,
						     "DIFF %ums %u00us, <%u>",
						     diff_ms,
						     vioc_timer_get_curtime(),
						     base_v);
				}
			}
			(void)memcpy(&dev_context->prev_time, &cur_time, sizeof(cur_time));
			#endif
			(void)vioc_timer_clear_irq_status(dev_context->timer_id);
		}
	}

	return IRQ_HANDLED;
}

/* coverity[misra_c_2012_rule_8_13] */
static int tccdrm_ss_bind_init_variables(struct tccdrm_screen_share_context *dev_context)
{
	int ret = 0;

        /* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_17_7] */
	spin_lock_init(&dev_context->irq_lock);

	/* coverity[cert_dcl37_c] */
	init_waitqueue_head(&dev_context->wait_display_done_queue);
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_21_2] */
	atomic_set(&dev_context->wait_display_done_event, 0);

	dev_context->timer_id = VIOC_TIMER_TIMER0;

	return ret;
}

static struct tccdrm_screen_share_context *tccdrm_ss_bind_create_context(struct device *dev,
						   const struct device *master_dev)
{
	/* coverity[misra_c_2012_rule_11_5] */
	struct drm_device *drm = (struct drm_device *)dev_get_drvdata(master_dev);
	struct tccdrm_screen_share_context *dev_context = NULL;
	bool internal_ok = (bool)true;

	if (dev->of_node == NULL) {
		DRM_DEV_ERROR(
			dev,
			"failed to get the device node\n");
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		/* Create device context */
		/* coverity[misra_c_2012_rule_11_5] */
		dev_context = devm_kzalloc(dev, sizeof(*dev_context),
					   /* coverity[misra_c_2012_rule_10_8] */ // K5.4
					   GFP_KERNEL);
		if (dev_context == NULL) {
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		dev_context->dev = dev;
		dev_context->drm = drm;
	}

	return dev_context;
}

static int tccdrm_ss_bind_parse_context(struct tccdrm_screen_share_context *dev_context)
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
	struct platform_device *plat_dev = to_platform_device(dev_context->dev);
	#if defined(CONFIG_SMP)
	unsigned int u, irq_cpumask;
	#endif
	struct device_node *current_node;
	bool internal_ok = (bool)true;
	int ret = 0;

	platform_set_drvdata(plat_dev, dev_context);

	#if defined(CONFIG_SMP)
	/* coverity[cert_int02_c] */
	/* coverity[cert_int31_c] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_12_1] */
	/* coverity[misra_c_2012_rule_13_4] */
	/* coverity[misra_c_2012_rule_14_2] */
	for_each_online_cpu(u) {
		irq_cpumask = u;
	}
	dev_context->irq_cpumask = cpumask_of(irq_cpumask);
	DRM_DEV_INFO(dev_context->dev,
		     "[INFO] crtc interrupts will be handled by cpu (%ux).\r\n",
		     irq_cpumask);
	#endif

	current_node = of_parse_phandle(dev_context->dev->of_node,
					"timer_device", 0);
	if (current_node == NULL) {
		DRM_DEV_ERROR(dev_context->dev,
				"could not find timer_device node\n");
		internal_ok = (bool)false;
		ret = -ENODEV;
	}
	if (internal_ok) {
		dev_context->irq_num = irq_of_parse_and_map(current_node, 0);
		DRM_DEV_INFO(dev_context->dev,
			     "[INFO] irq_num of timer device is %d\r\n",
			     dev_context->irq_num);
	}
	if (ret < 0) {
		DRM_DEV_ERROR(dev_context->dev, "failed to parse device tree\n");
	}

	return ret;
}

static int tccdrm_ss_bind_init_interrupts(struct tccdrm_screen_share_context *dev_context)
{
	bool internal_ok = (bool)true;
	int ret = 0;

	if (tcc_math_uint_gt_intmax(dev_context->irq_num)) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}
	if (internal_ok) {
		(void)vioc_timer_set_irq_mask(dev_context->timer_id);
		#if defined(CONFIG_CHECK_DRM_TELECHIPS_VSYNC_TIME_GAP)
		do_gettimeofday(&dev_context->prev_time);
		#endif
		#if defined(CONFIG_SMP)
		(void)irq_set_affinity_hint(dev_context->irq_num,
				       dev_context->irq_cpumask);
		#endif
		ret = devm_request_irq(
			dev_context->dev,
			dev_context->irq_num,
			screen_share_irq_handler, IRQF_SHARED, dev_name(dev_context->dev),
			dev_context);
		if (ret < 0) {
			DRM_DEV_ERROR(dev_context->dev,
				      "failed to request irq\r\n");
		}
	}
	return ret;
}

static int tccdrm_ss_bind_init_planes_and_crtcs(struct tccdrm_screen_share_context *dev_context)
{
	struct tccdrm_universal_plane_data universal_plane_data;
	struct drm_plane *primary = NULL;
	bool internal_ok = (bool)true;
	unsigned int formats_list_size;
	const uint32_t *formats_list;
	int ret = 0;

	formats_list = tccdrm_ss_formats;
	/* coverity[misra_c_2012_rule_6_1] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_12_1] */
	formats_list_size = ARRAY_SIZE(tccdrm_ss_formats);

	(void)memset(&universal_plane_data, 0, sizeof(universal_plane_data));

	universal_plane_data.plane = &dev_context->plane.base;
	universal_plane_data.plane_type = DRM_PLANE_TYPE_PRIMARY;
	universal_plane_data.pixel_formats = formats_list;
	universal_plane_data.num_pixel_formats = formats_list_size;
	universal_plane_data.plane_funcs = &tccdrm_ss_plane_funcs;
	universal_plane_data.plane_helper_funcs = &tccdrm_ss_plane_helper_funcs;

	ret = tcc_prepare_universal_plane(dev_context->drm,
					  &universal_plane_data);
	if (ret != 0) {
		DRM_DEV_ERROR(
			dev_context->dev,
			"failed to initizliaed the planes\n");
		internal_ok = (bool)false;
	} else {
		dev_context->plane.win = 0;
		primary = &dev_context->plane.base;
	}

	if (internal_ok) {
		struct tccdrm_crtc_create_data crtc_create_data = {
			.crtc = &dev_context->crtc,
			.crtc_name = NULL,
			.primary = primary,
			.cursor = NULL,
			.crtc_funcs = &tccdrm_ss_crtc_funcs,
			.crtc_helper_funcs = &tccdrm_ss_crtc_helper_funcs
		};

		ret = tccdrm_crtc_create((const struct device *)dev_context->dev,
					 dev_context->drm,
					 (const struct tccdrm_crtc_create_data *)&crtc_create_data);
		if (ret == 0) {
			/* coverity[cert_dcl37_c] */
			/* coverity[misra_c_2012_rule_10_3] */
			/* coverity[misra_c_2012_rule_21_2] */
			atomic_set(&dev_context->flip_state.flipstatus,
				TCC_DRM_CRTC_FLIP_STATUS_NONE);
		}
	}
	return ret;
}

static int tccdrm_ss_bind_init_phase_0(struct tccdrm_screen_share_context *dev_context)
{
	bool internal_ok = (bool)true;
	int ret = 0;

	ret = tccdrm_ss_bind_init_variables(dev_context);
	if (ret < 0) {
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		ret = tccdrm_ss_bind_init_interrupts(dev_context);
	}
	return ret;
}

static int tccdrm_ss_bind_init_phase_1(struct tccdrm_screen_share_context *dev_context)
{
	struct device *dev = dev_context->dev;
	bool internal_ok = (bool)true;
	int ret = 0;

	ret = tccdrm_ss_bind_init_planes_and_crtcs(dev_context);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "failedinit planes and crtcs\r\n");
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		#if defined(CONFIG_PM)
		pm_runtime_enable(dev);
		#endif
	}

	return ret;
}

/* coverity[misra_c_2012_rule_8_13] */
static int tccdrm_ss_bind(struct device *dev, struct device *master_dev,
			/* coverity[misra_c_2012_rule_8_13] */
			void *data)
{
	struct tccdrm_screen_share_context *dev_context = NULL;
	bool free_context = (bool)false;
	bool internal_ok = (bool)true;
	int ret = 0;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter data is not
	 * used in the function.
	 */
	(void)data;

	dev_context = tccdrm_ss_bind_create_context(dev, (const struct device *)master_dev);
	if (dev_context == NULL) {
		internal_ok = (bool)false;
		ret = -ENOMEM;
	} else {
		free_context = (bool)true;
	}
	if (internal_ok) {
		ret = tccdrm_ss_bind_parse_context(dev_context);
		if (ret < 0) {
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		ret = tccdrm_ss_bind_init_phase_0(dev_context);
		if (ret < 0) {
			internal_ok = (bool)false;
		}
	}

	if (internal_ok) {
		ret = tccdrm_ss_bind_init_phase_1(dev_context);
	}

	if (internal_ok) {
		dev_context->dev_binded = (bool)true;
	} else {
		if (free_context) {
			devm_kfree(dev, dev_context);
			dev_context = NULL;
		}
	}
	return ret;
}

/* coverity[misra_c_2012_rule_8_13] */
static void tccdrm_ss_unbind(struct device *dev, struct device *master_dev,
			/* coverity[misra_c_2012_rule_8_13] */
			void *data)
{
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_11_5] */
	struct tccdrm_screen_share_context *dev_context = dev_get_drvdata(dev);
	bool internal_ok = (bool)true;
	int irq_num, vioc_intr_inum;
	//unsigned long irqflags;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter master_dev is not
	 * used in the function.
	 */
	(void)master_dev;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter dev is not used in
	 * the function.
	 */
	(void)data;

	if (dev_context == NULL) {
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		if (tcc_math_uint_gt_intmax(dev_context->irq_num)) {
			internal_ok = (bool)false;
		} else {
			irq_num = (int)dev_context->irq_num;
		}
	}
	if (internal_ok) {
		vioc_intr_inum = (int)VIOC_INTR_TIMER;

		(void)tccdrm_ss_disable_irq_of_timer(dev_context,
							     irq_num,
							     vioc_intr_inum);
		(void)irq_set_affinity_hint(dev_context->irq_num, NULL);
		devm_free_irq(dev_context->dev,
			      dev_context->irq_num,
			      dev_context);
	}
	if (internal_ok) {
		/* Deactivate CRTC clock */
		#if defined(CONFIG_PM)
		pm_runtime_disable(dev);
		#endif

		devm_kfree(dev, dev_context);
	}
}

static const struct component_ops lcd_component_ops = {
	.bind	= tccdrm_ss_bind,
	.unbind = tccdrm_ss_unbind,
};

static int tccdrm_ss_probe(struct platform_device *plat_dev)
{
	(void)DRM_INFO("Initialized %s %d.%d.%d %s\r\n",
			plat_dev->name,
			DRIVER_MAJOR,
			DRIVER_MINOR,
			DRIVER_PATCH,
			DRIVER_DATE);
	return component_add(&plat_dev->dev, &lcd_component_ops);
}


static int tccdrm_ss_remove(struct platform_device *plat_dev)
{
	component_del(&plat_dev->dev, &lcd_component_ops);
	return 0;
}

#ifdef CONFIG_PM
/* coverity[misra_c_2012_rule_8_13] */
static int tccdrm_ss_suspend(struct device *dev)
{
	//struct tccdrm_screen_share_context *dev_context = dev_get_drvdata(dev);
	int ret = vioc_timer_suspend();
	DRM_DEV_INFO(dev, "[INFO] \r\n");
	return ret;
}

/* coverity[misra_c_2012_rule_8_13] */
static int tccdrm_ss_resume(struct device *dev)
{
	//struct tccdrm_screen_share_context *dev_context = dev_get_drvdata(dev);
	int ret = vioc_timer_resume();
	DRM_DEV_INFO(dev, "[INFO] \r\n");
	return ret;
}
#endif

static const struct dev_pm_ops tccdrm_ss_pm_ops = {
	/* coverity[misra_c_2012_rule_20_7] */
	SET_SYSTEM_SLEEP_PM_OPS(tccdrm_ss_suspend, tccdrm_ss_resume)
};

static const struct of_device_id tccdrm_ss_dt_match[] = {
	{
		.compatible = "telechips,tcc-drm-screen-share",
	}, {
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, tccdrm_ss_dt_match);

struct platform_driver tccdrm_ss_driver = {
	.probe		= tccdrm_ss_probe,
	.remove		= tccdrm_ss_remove,
	.driver		= {
		.name	= "tcdrm-ss",
		.owner	= THIS_MODULE,
		.pm	= &tccdrm_ss_pm_ops,
		.of_match_table = of_match_ptr(tccdrm_ss_dt_match),
	},
};
/* coverity[misra_c_2012_rule_8_3] */
/* coverity[misra_c_2012_rule_8_5] */
/* coverity[misra_c_2012_rule_8_6] */
/* coverity[misra_c_2012_rule_8_11] */
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
EXPORT_SYMBOL(tccdrm_ss_driver);

/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_21_2] */ // K5.4
MODULE_DESCRIPTION("Telechips DRM Screen Share Driver");

/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_21_2] */ // K5.4
MODULE_LICENSE("GPL");

/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_5_9] */ //K5.4
/* coverity[misra_c_2012_rule_21_2] */ //K5.4
MODULE_VERSION(__stringify(DRIVER_MAJOR) "."
               __stringify(DRIVER_MINOR) "."
               __stringify(DRIVER_PATCH));

