// SPDX-License-Identifier: GPL-2.0-or-later

/* telechips_drm_fb.c
 *
 * Copyright (C) 2016 Telechips Inc.
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *	Seung-Woo Kim <sw0312.kim@samsung.com>
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
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <uapi/drm/telechips_drm.h>
#include <linux/tcc_math.h>

#include <telechips_drm_drv.h>
#include <telechips_drm_fb.h>
#include <telechips_drm_fbdev.h>

#define to_tcc_fb(x) container_of(x, struct tccdrm_fb, fb)

#define TCC_MAX_FB_BUFFER 4

struct tccdrm_fb {
	struct drm_framebuffer fb;
	struct tcc_drm_gem *tcc_gem[TCC_MAX_FB_BUFFER];
};

static int check_fb_gem_memory_type(const struct drm_device *dev,
				    const struct tcc_drm_gem *tcc_gem)
{
	unsigned int fbflags;
	int ret = 0;

	fbflags = tcc_gem->mem_flags;
	/*
	 * Physically non-contiguous memory type for framebuffer is not
	 * supported without IOMMU.
	 */
	if (IS_NONCONTIG_BUFFER(fbflags) != 0U) {
		DRM_DEV_ERROR(dev->dev,
			    "[ERR] Non-contiguous GEM memory is not supported \r\n");
		ret = -EINVAL;
	}

	return ret;
}

static void tccdrm_fb_destroy(struct drm_framebuffer *fb)
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
	struct tccdrm_fb *tcc_fb = to_tcc_fb(fb);
	struct drm_gem_object *obj;
	int i;

	for (i = 0; i < TCC_MAX_FB_BUFFER; i++) {
		if (tcc_fb->tcc_gem[i] == NULL) {
			continue;
		}
		obj = &tcc_fb->tcc_gem[i]->base;
		#if defined(CONFIG_REFCODE_PRE_K54)
		drm_gem_object_unreference_unlocked(obj);
		#elif defined(CONFIG_REFCODE_PRE_K510)
		drm_gem_object_put_unlocked(obj);
		#else
		drm_gem_object_put(obj);
		#endif
	}
	drm_framebuffer_cleanup(fb);
	kfree(tcc_fb);
	tcc_fb = NULL;
}

static int tccdrm_fb_create_handle(struct drm_framebuffer *fb,
					struct drm_file *file_priv,
					unsigned int *fb_handle)
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
	const struct tccdrm_fb *tcc_fb = to_tcc_fb(fb);

	return drm_gem_handle_create(file_priv,
				     &tcc_fb->tcc_gem[0]->base, fb_handle);
}

static const struct drm_framebuffer_funcs tccdrm_fb_funcs = {
	.destroy = tccdrm_fb_destroy,
	.create_handle = tccdrm_fb_create_handle,
};

/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 2>
  * HIS metric violation (HIS_CCM)
 *  DR <case 2>
 */
struct drm_framebuffer *
tccdrm_user_fb_create(struct drm_device *dev, struct drm_file *file_priv,
		   const struct drm_mode_fb_cmd2 *mode_cmd)
{
	const struct drm_format_info *info =
			drm_get_format_info(dev, mode_cmd);
	struct tcc_drm_gem *tcc_gem[TCC_MAX_FB_BUFFER];
	struct drm_gem_object *obj;
	struct drm_framebuffer *fb;

	unsigned long height, size;

	int num_gem_object_unreference = 0;
	bool internal_ok = (bool)true;
	int i, num_planes;
	int ret = 0;

	num_planes = (int)info->num_planes;

	for (i = 0; i < num_planes; i++) {
		/* height = (i == 0) ? mode_cmd->height :
				     DIV_ROUND_UP(
					     mode_cmd->height, info->vsub); */
		if (i == 0) {
			height = mode_cmd->height;
		} else {
			unsigned long param1 = mode_cmd->height;
			unsigned long param2 = info->vsub;

			if (!tcc_math_check_ulong_plus_ulong(param1, param2)) {
				internal_ok = (bool)false;
			}
			if (internal_ok) {
				if (!tcc_math_check_ulong_minus_ulong(param1 +
				    param2, 1UL)) {
					internal_ok = (bool)false;
				}
			}
			if (internal_ok) {
				/* coverity[misra_c_2012_rule_10_4] */
				height = DIV_ROUND_UP(param1, param2);
			}
		}
		/* -- */

		/* size = height * mode_cmd->pitches[i] +
				     mode_cmd->offsets[i]; */
		if (internal_ok) {
			if (!tcc_math_check_ulong_mul_ulong(height, mode_cmd->pitches[i])) {
				internal_ok = (bool)false;
				ret = -EINVAL;
			} else {
				size = height * mode_cmd->pitches[i];
			}
		}
		if (internal_ok) {
			if (tcc_math_check_ulong_plus_ulong(size,
							    mode_cmd->offsets[i])) {
				size += mode_cmd->offsets[i];
			} else {
				internal_ok = (bool)false;
				ret = -EINVAL;
			}
		}
		/* -- */

		if (internal_ok) {
			obj = drm_gem_object_lookup(file_priv, mode_cmd->handles[i]);
			if (obj == NULL) {
				dev_err(
					dev->dev,
					"[ERR][DRMFB] %s Failed to lookup gem object \r\n",
					__func__);
				internal_ok = (bool)false;
				ret = -ENOENT;
				//goto err_gem_object_unreference;
			} else {
				num_gem_object_unreference++;
			}
		}
		if (internal_ok) {
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
			tcc_gem[i] = to_tcc_gem(obj);
			if (size > tcc_gem[i]->size) {
				dev_err(
					dev->dev,
					"[ERR][DRMFB] %s Out of size for gem object \r\n",
					__func__);
				internal_ok = (bool)false;
				ret = -EINVAL;
			}
		}
	}
	if (internal_ok) {
		fb = tccdrm_fb_alloc(dev, mode_cmd, tcc_gem,
				      num_gem_object_unreference);
		/* coverity[misra_c_2012_rule_11_2] */
		if (IS_ERR(fb)) {
			dev_err(
				dev->dev,
				"[ERR][DRMFB] %s Failed to tccdrm_fb_alloc\r\n",
				__func__);
			internal_ok = (bool)false;
			/* coverity[misra_c_2012_rule_10_3] */
			ret = PTR_ERR(fb);
		}
	}
	if (!internal_ok) {
		/* coverity[misra_c_2012_rule_11_5] */
		fb = ERR_PTR(ret);
		for (i = (num_gem_object_unreference - 1); i >= 0; i--) {
			#if defined(CONFIG_REFCODE_PRE_K54)
			drm_gem_object_unreference_unlocked(&tcc_gem[i]->base);
			#elif defined(CONFIG_REFCODE_PRE_K510)
			drm_gem_object_put_unlocked(&tcc_gem[i]->base);
			#else
			drm_gem_object_put(&tcc_gem[i]->base);
			#endif
		}
	}
	return fb;
}
/* coverity[misra_c_2012_rule_5_2] */
/* coverity[misra_c_2012_rule_8_3] */
/* coverity[misra_c_2012_rule_8_5] */
/* coverity[misra_c_2012_rule_8_6] */
/* coverity[misra_c_2012_rule_8_11] */
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
EXPORT_SYMBOL(tccdrm_user_fb_create);

dma_addr_t tccdrm_fb_dma_addr(struct drm_framebuffer *fb, int fb_index)
{
	const struct tccdrm_fb *tcc_fb;
	const struct tcc_drm_gem *tcc_gem;
	bool internal_ok = (bool)true;
	dma_addr_t dma_addr = 0UL;

	if (fb == NULL) {
		DRM_DEV_ERROR(NULL, "fb is NULL pointer\r\n");
		internal_ok = (bool)false;
	}
	if (internal_ok &&
	    ((fb_index < 0) || (fb_index >= TCC_MAX_FB_BUFFER))) {
		internal_ok = (bool)false;
	}
	if (internal_ok) {
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
		tcc_fb = to_tcc_fb(fb);
		if (tcc_fb == NULL) {
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		tcc_gem = tcc_fb->tcc_gem[fb_index];
		if (tcc_math_check_ulong_plus_ulong(tcc_gem->dma_addr,
						    fb->offsets[fb_index])) {
			dma_addr = tcc_gem->dma_addr + fb->offsets[fb_index];
		}
	}
	return dma_addr;
}
/* coverity[misra_c_2012_rule_5_2] */
/* coverity[misra_c_2012_rule_8_3] */
/* coverity[misra_c_2012_rule_8_5] */
/* coverity[misra_c_2012_rule_8_6] */
/* coverity[misra_c_2012_rule_8_11] */
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
EXPORT_SYMBOL(tccdrm_fb_dma_addr);

struct drm_framebuffer *
tccdrm_fb_alloc(struct drm_device *dev,
		  const struct drm_mode_fb_cmd2 *mode_cmd,
		  /* coverity[misra_c_2012_rule_8_13] */
		  struct tcc_drm_gem **tcc_gem, int num_planes)
{
	struct drm_framebuffer *framebuffer;
	bool need_free_alloc = (bool)false;
	bool internal_ok = (bool)true;
	struct tccdrm_fb *tcc_fb;
	int ret, i;

	if (num_planes < 1) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}

	if (internal_ok) {
		/* coverity[misra_c_2012_rule_10_8] */
		/* coverity[misra_c_2012_rule_11_5] */
		tcc_fb = kzalloc(sizeof(*tcc_fb), GFP_KERNEL);
		if (tcc_fb == NULL) {
			internal_ok = (bool)false;
			ret = -ENOMEM;
		} else {
			need_free_alloc = (bool)true;
		}
	}
	if (internal_ok) {
		for (i = 0; i < num_planes; i++) {
			ret = check_fb_gem_memory_type(
					(const struct drm_device *)dev,
					tcc_gem[i]);
			if (ret < 0) {
				dev_err(
					dev->dev,
					"[ERR][DRMFB] %s Failed to check_fb_gem_memory_type \r\n",
					__func__);
				internal_ok = (bool)false;
				break;
			}
			tcc_fb->tcc_gem[i] = tcc_gem[i];
		}
	}
	if (internal_ok) {
		drm_helper_mode_fill_fb_struct(dev, &tcc_fb->fb, mode_cmd);

		/* Map gem object to FB handle */
		for (i = 0; i < num_planes; i++) {
			tcc_fb->fb.obj[i] = &tcc_gem[i]->base;
		}
		ret = drm_framebuffer_init(dev, &tcc_fb->fb,
					&tccdrm_fb_funcs);
		if (ret < 0) {
			dev_err(dev->dev, "Failed to initialize framebuffer: %d\n",
				ret);
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		framebuffer = &tcc_fb->fb;
	} else {
		if (need_free_alloc) {
			kfree(tcc_fb);
		}
		/* coverity[misra_c_2012_rule_11_5] */
		framebuffer = ERR_PTR(ret);
	}
	return framebuffer;
}
/* coverity[misra_c_2012_rule_8_3] */
/* coverity[misra_c_2012_rule_8_5] */
/* coverity[misra_c_2012_rule_8_6] */
/* coverity[misra_c_2012_rule_8_11] */
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
EXPORT_SYMBOL(tccdrm_fb_alloc);

