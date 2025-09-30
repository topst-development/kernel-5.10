// SPDX-License-Identifier: GPL-2.0-or-later

/* telechips_drm_fbdev.c
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
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_crtc.h>

#include <drm/telechips_drm.h>

#include <linux/console.h>
#include <linux/ioctl.h>
#include <linux/tcc_math.h>
#include <telechips_drm_types.h>
#include <telechips_drm_drv.h>
#include <telechips_drm_fb.h>
#include <telechips_drm_fbdev.h>

#define MAX_CONNECTOR 4
#define PREFERRED_BPP 32

#define to_tcc_fbdev(x)	container_of(x, struct tccdrm_fbdev,\
				fb_helper)

#define DRMFBIO_CHECK_CRTC \
		_IOR('D', 0x01, unsigned int)
#define DRMFBIO_CTRL_SET_CHROMAKEY \
		_IOW('D', 0x10, struct drm_ioctl_chromakey_t)
#define DRMFBIO_CTRL_GET_CHROMAKEY \
		_IOR('D', 0x11, struct drm_ioctl_chromakey_t)

struct tccdrm_fbdev {
	struct drm_fb_helper fb_helper;
	struct tcc_drm_gem *tcc_gem;
};

/* coverity[misra_c_2012_rule_8_13] */
static int tccdrm_fbdev_mmap(struct fb_info *info,
			struct vm_area_struct *vma)
{
	/* coverity[misra_c_2012_rule_11_5] */
	struct drm_fb_helper *fb_helper = (struct drm_fb_helper *)info->par;
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
	struct tccdrm_fbdev *tcc_fbdev = to_tcc_fbdev(fb_helper);
	const struct tcc_drm_gem *tcc_gem = tcc_fbdev->tcc_gem;
	bool internal_ok = (bool)true;
	unsigned long vm_size;
	int ret = 0;

	vma->vm_flags |= (vm_flags_t)VM_IO | (vm_flags_t)VM_DONTEXPAND |
			 (vm_flags_t)VM_DONTDUMP;

	vm_size = vma->vm_end - vma->vm_start;

	if (vm_size > tcc_gem->size) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}

	if (internal_ok) {
		/* coverity[misra_c_2012_rule_8_13] */
		/* coverity[misra_c_2012_rule_8_15] */
		/* coverity[misra_c_2012_rule_11_5] */
		ret = dma_mmap_attrs(fb_helper->dev->dev, vma, tcc_gem->cookie,
				tcc_gem->dma_addr, tcc_gem->size,
				tcc_gem->dma_attrs);
		if (ret < 0) {
			DRM_ERROR("failed to mmap.\n");
		}
	}

	return ret;
}

static struct drm_crtc *tccdrm_fbdev_check_crtc_id(
	struct drm_fb_helper *fb_helper, unsigned int req_crtc_id)
{
	#if defined(CONFIG_REFCODE_PRE_K54)
	int i;
	struct drm_crtc *crtc = NULL;

	for (i = 0; i < fb_helper->crtc_count; i++) {
		if (
			fb_helper->crtc_info[i].mode_set.crtc->base.id ==
			req_crtc_id) {
			crtc = fb_helper->crtc_info[i].mode_set.crtc;
			break;
		}
	}
	#else
	struct drm_crtc *crtc = NULL;
	struct drm_client_dev *client = &fb_helper->client;
	const struct drm_mode_set *mode_set;

	mutex_lock(&client->modeset_mutex);
	/* coverity[misra_c_2012_rule_11_9] */
	/* coverity[misra_c_2012_rule_13_4] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	drm_client_for_each_modeset(mode_set, client) {
		if (
			mode_set->crtc->base.id ==
			req_crtc_id) {
			crtc = mode_set->crtc;
			break;
		}
	}
	mutex_unlock(&client->modeset_mutex);
	#endif
	return crtc;
}

#if defined(CONFIG_DRM_TELECHIPS_CTRL_CHROMAKEY)
static struct drm_crtc *tccdrm_fbdev_get_crtc_by_index(
	struct drm_fb_helper *fb_helper, unsigned int req_crtc_index)
{
	#if defined(CONFIG_REFCODE_PRE_K54)
	int i;
	struct drm_crtc *crtc = NULL;

	for (i = 0; i < fb_helper->crtc_count; i++) {
		if (
			drm_crtc_index(
				fb_helper->crtc_info[i].mode_set.crtc) ==
				req_crtc_index) {
			crtc = fb_helper->crtc_info[i].mode_set.crtc;
			break;
		}
	}
	#else
	struct drm_crtc *crtc = NULL;
	struct drm_client_dev *client = &fb_helper->client;
	const struct drm_mode_set *mode_set;

	mutex_lock(&client->modeset_mutex);
	/* coverity[misra_c_2012_rule_11_9] */
	/* coverity[misra_c_2012_rule_13_4] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	drm_client_for_each_modeset(mode_set, client) {
		if (
			drm_crtc_index(
				mode_set->crtc) ==
				req_crtc_index) {
			crtc = mode_set->crtc;
			break;
		}
	}
	mutex_unlock(&client->modeset_mutex);
	#endif
	return crtc;
}
#endif

#if defined(CONFIG_DRM_TELECHIPS_CTRL_CHROMAKEY)
static int tcc_drm_fb_helper_get_chromakey(struct drm_fb_helper *fb_helper,
					   unsigned long arg)
{
	struct drm_ioctl_chromakey_t chromakey;
	const struct tcc_crtc_state *tcc_cstate;
	struct drm_crtc *crtc;

	bool internal_ok = (bool)true;
	int ret = 0;

	if (fb_helper == NULL) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}

	if (internal_ok) {
		/* coverity[cert_int36_c] */
		/* coverity[misra_c_2012_rule_11_6] */
		if (copy_from_user(&chromakey, (void __user *)arg,
				sizeof(struct drm_ioctl_chromakey_t)) != 0UL) {
			DRM_DEV_ERROR(NULL,
				"[ERR][DRMLCD] failed copy_from_user at line(%d)\r\n",
				__LINE__);
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		crtc = tccdrm_fbdev_get_crtc_by_index(
			fb_helper, chromakey.crtc_index);
		if (crtc == NULL) {
			DRM_DEV_ERROR(NULL,
				"[ERR][DRMFB] %s invalid index %d because crtc is NULL\r\n",
				__func__, chromakey.crtc_index);
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		if (crtc->state == NULL) {
			DRM_DEV_ERROR(NULL,
				"crtc [%d] is not ready\r\n", chromakey.crtc_index);
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
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
		tcc_cstate = (const struct tcc_crtc_state *)to_tcc_crtc_state(crtc->state);

		if (tcc_cstate->get_chromakey == NULL) {
			DRM_DEV_ERROR(tcc_cstate->dev, "This crtc does not support get_chromakey\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		/* Layer range is 0 to 2 */
		if (chromakey.chromakey_layer > 2U) {
			DRM_DEV_ERROR(tcc_cstate->dev,
				"DRMFBIO_CTRL_GET_CHROMAKEY layer %d is not valid\r\n",
				chromakey.chromakey_layer);
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		ret = tcc_cstate->get_chromakey(crtc,
						chromakey.chromakey_layer,
						&chromakey.chromakey_enable,
						&chromakey.chromakey_value,
						&chromakey.chromakey_mask);
		if (ret < 0) {
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		/* coverity[misra_c_2012_rule_14_4] */
		if (copy_to_user((void __user *)arg, &chromakey,
				 sizeof(struct drm_ioctl_chromakey_t)) != 0UL) {
			DRM_DEV_ERROR(tcc_cstate->dev,
				      "failed copy_to_user at line(%d)\r\n",
				      __LINE__);
			ret = -EINVAL;
		}
	}
	return ret;
}

static int tcc_drm_fb_helper_set_chromakey(struct drm_fb_helper *fb_helper,
					   unsigned long arg)
{
	struct drm_ioctl_chromakey_t chromakey;
	const struct tcc_crtc_state *tcc_cstate;
	struct drm_crtc *crtc;

	bool internal_ok = (bool)true;
	int ret = 0;

	if (fb_helper == NULL) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}

	if (internal_ok) {
		/* coverity[cert_int36_c] */
		/* coverity[misra_c_2012_rule_11_6] */
		if (copy_from_user(&chromakey, (void __user *)arg,
				   sizeof(struct drm_ioctl_chromakey_t)) != 0UL) {
			DRM_DEV_ERROR(NULL,
				"[ERR][DRMLCD] failed copy_from_user at line(%d)\r\n",
				__LINE__);
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		crtc = tccdrm_fbdev_get_crtc_by_index(
			fb_helper, chromakey.crtc_index);
		if (crtc == NULL) {
			DRM_DEV_ERROR(NULL,
				"[ERR][DRMFB] %s invalid index %d because crtc is NULL\r\n",
				__func__, chromakey.crtc_index);
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		if (crtc->state == NULL) {
			DRM_DEV_ERROR(NULL,
				"crtc [%d] is not ready\r\n", chromakey.crtc_index);
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
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
		tcc_cstate = (const struct tcc_crtc_state *)to_tcc_crtc_state(crtc->state);

		if (tcc_cstate->set_chromakey == NULL) {
			DRM_DEV_ERROR(tcc_cstate->dev, "This crtc does not support set_chromakey\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		/* Layer range is 0 to 2 */
		if (chromakey.chromakey_layer > 2U) {
			DRM_DEV_ERROR(tcc_cstate->dev,
				"DRMFBIO_CTRL_SET_CHROMAKEY layer %d is not valid\r\n",
				chromakey.chromakey_layer);
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		ret = tcc_cstate->set_chromakey(crtc,
						chromakey.chromakey_layer,
						chromakey.chromakey_enable,
						(const struct drm_chromakey_t *)&chromakey.chromakey_value,
						(const struct drm_chromakey_t *)&chromakey.chromakey_mask);
	}
	return ret;
}
#endif

/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 2>
 * HIS metric violation (HIS_CCM)
 *  DR <case 3>
 */
/* coverity[misra_c_2012_rule_8_13] */
static int tcc_drm_fb_helper_ioctl(struct fb_info *info, unsigned int cmd,
			unsigned long arg)
{
	/* coverity[misra_c_2012_rule_11_5] */
	struct drm_fb_helper *fb_helper =
		(struct drm_fb_helper *)info->par;
	int ret = 0;

	mutex_lock(&fb_helper->lock);

	switch (cmd) {
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_12_1] */
	/* coverity[misra_c_2012_rule_14_3] */
	case (unsigned int)DRMFBIO_CHECK_CRTC:
	{
		const struct drm_crtc *crtc;
		unsigned int req_crtc_id;

		/* coverity[cert_dcl37_c] */
		/* coverity[cert_int02_c] */
		/* coverity[cert_int31_c] */
		/* coverity[cert_int36_c] */
		/* coverity[misra_c_2012_rule_2_2] */
		/* coverity[misra_c_2012_rule_8_13] */ // K5.4
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_11_5] */
		/* coverity[misra_c_2012_rule_14_3] */
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_15_6] */
		/* coverity[misra_c_2012_rule_16_1] */
		/* coverity[misra_c_2012_rule_16_3] */
		/* coverity[misra_c_2012_rule_21_2] */
		if (get_user(req_crtc_id, (unsigned int __user *)arg) < 0) {
			ret = -EINVAL;
			break;
		}

		crtc = tccdrm_fbdev_check_crtc_id(
			fb_helper, req_crtc_id);
		if (crtc == NULL) {
			ret = -EINVAL;
		}
	}
	break;
	#if defined(CONFIG_DRM_TELECHIPS_CTRL_CHROMAKEY)
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_12_1] */
	/* coverity[misra_c_2012_rule_14_3] */
	case (unsigned int)DRMFBIO_CTRL_GET_CHROMAKEY:
		ret = tcc_drm_fb_helper_get_chromakey(fb_helper, arg);
		break;
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_12_1] */
	/* coverity[misra_c_2012_rule_14_3] */
	case (unsigned int)DRMFBIO_CTRL_SET_CHROMAKEY:
		ret = tcc_drm_fb_helper_set_chromakey(fb_helper, arg);
		break;
	#endif
	default:
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&fb_helper->lock);
	return ret;
}

static struct fb_ops tcc_drm_fb_ops = {
	.owner = THIS_MODULE,
	DRM_FB_HELPER_DEFAULT_OPS,
	.fb_mmap = tccdrm_fbdev_mmap,
	.fb_fillrect = drm_fb_helper_cfb_fillrect,
	.fb_copyarea = drm_fb_helper_cfb_copyarea,
	.fb_imageblit = drm_fb_helper_cfb_imageblit,
};

/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 2>
 * HIS metric violation (HIS_CCM)
 *  DR <case 3>
 */
static int tccdrm_fbdev_update(struct drm_fb_helper *fb_helper,
				   /* coverity[misra_c_2012_rule_8_13] */
				   struct drm_fb_helper_surface_size *sizes,
				   struct tcc_drm_gem *tcc_gem)
{
	struct fb_info *fbi;
	const struct drm_framebuffer *fb =
		(const struct drm_framebuffer *)fb_helper->fb;
	bool internal_ok = (bool)true;
	unsigned int size, utmp;
	unsigned int nr_pages;
	unsigned long ultmp, addr_offset;
	int ret = 0;

	/* size = fb->width * fb->height * fb->format->cpp[0]; */
	if (!tcc_math_check_ulong_mul_ulong(fb->width, fb->height)) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}
	if (internal_ok) {
		size = fb->width * fb->height;
		utmp = fb->format->cpp[0];
		if (!tcc_math_check_ulong_mul_ulong(size, utmp)) {
			internal_ok = (bool)false;
			ret = -EINVAL;
		//} else {
		//	size *= utmp;
		}
	}
	/* -- */

	if (internal_ok) {
		/* size can be multiplied to utmp */
		size *= utmp;

		fbi = drm_fb_helper_alloc_fbi(fb_helper);
		/* coverity[misra_c_2012_rule_11_2] */
		if (IS_ERR(fbi)) {
			DRM_ERROR("failed to allocate fb info.\n");
			internal_ok = (bool)false;
			/* coverity[misra_c_2012_rule_10_3] */
			ret = PTR_ERR(fbi);
		}
	}

	if (internal_ok) {
		#if defined(CONFIG_REFCODE_PRE_K54)
		fbi->par = fb_helper;
		fbi->flags = FBINFO_FLAG_DEFAULT;
		#endif
		/* replace fb_ioctl */
		tcc_drm_fb_ops.fb_ioctl = tcc_drm_fb_helper_ioctl;
		fbi->fbops = &tcc_drm_fb_ops;

		#if defined(CONFIG_REFCODE_PRE_K54)
		drm_fb_helper_fill_fix(fbi,
				       fb->pitches[0], fb->format->depth);
		drm_fb_helper_fill_var(fbi,
				       fb_helper, sizes->fb_width,
				       sizes->fb_height);
		#else
		drm_fb_helper_fill_info(fbi, fb_helper, sizes);
		#endif
		/* nr_pages = tcc_gem->size >> PAGE_SHIFT; */
		ultmp = tcc_gem->size >> PAGE_SHIFT;
		if (tcc_math_ulong_gt_uintmax(ultmp)) {
			internal_ok = (bool)false;
			ret = -EINVAL;
		//} else {
		//	nr_pages = (unsigned int)ultmp;
		}
		/* -- */
	}

	if (internal_ok) {
		/*
		 * ultmp can convert to uint because it is not great then
		 * UINT_MAX
		 */
		nr_pages = (unsigned int)ultmp;

		tcc_gem->kvaddr = (void __iomem *) vmap(tcc_gem->pagelist, nr_pages,
					/* coverity[misra_c_2012_rule_10_4] */
					/* coverity[misra_c_2012_rule_13_1] */
					VM_MAP, pgprot_writecombine(PAGE_KERNEL));
		if (tcc_gem->kvaddr == NULL) {
			DRM_ERROR("failed to map pages to kernel space.\n");
			internal_ok = (bool)false;
			ret = -EIO;
		}
	}

	/* addr_offset = fbi->var.xoffset * fb->format->cpp[0]; */
	if (internal_ok) {
		ultmp = (unsigned long)fb->format->cpp[0];
		if (!tcc_math_check_ulong_mul_ulong(fbi->var.xoffset, ultmp)) {
			internal_ok = (bool)false;
			ret = -EINVAL;
		//} else {
		//	addr_offset = fbi->var.xoffset * ultmp;
		}
	}
	/* -- */

	/* addr_offset += fbi->var.yoffset * fb->pitches[0]; */
	if (internal_ok) {
		/* fbi->var.xoffset can be multiplied to utmp */
		addr_offset = fbi->var.xoffset * ultmp;

		ultmp = (unsigned long)fb->pitches[0];
		if (!tcc_math_check_ulong_mul_ulong(fbi->var.yoffset, ultmp)) {
			internal_ok = (bool)false;
			ret = -EINVAL;
		//} else {
		//	ultmp = fbi->var.yoffset * ultmp;
		}
	}
	if (internal_ok) {
		/* fbi->var.yoffset can be multiplied to utmp */
		ultmp = fbi->var.yoffset * ultmp;

		if (!tcc_math_check_ulong_plus_ulong(addr_offset, ultmp)) {
			internal_ok = (bool)false;
			ret = -EINVAL;
		//} else {
		//	addr_offset += ultmp;
		}
	}
	/* -- */
	/* fbi->screen_base = tcc_gem->kvaddr + addr_offset */
	if (internal_ok) {
		/* addr_offset can be added to ultmp */
		addr_offset += ultmp;

		/* coverity[misra_c_2012_rule_11_5] */
		/* coverity[misra_c_2012_rule_18_4] */
		fbi->screen_base = (char __iomem *)(tcc_gem->kvaddr + addr_offset);
	}
	/* -- */
	if (internal_ok) {
		fbi->screen_size = size;
	}
	/* fbi->fix.smem_start = (phys_addr_t)(tcc_gem->dma_addr + addr_offset); */
	if (internal_ok) {
		if (!tcc_math_check_ulong_plus_ulong(tcc_gem->dma_addr, addr_offset)) {
			internal_ok = (bool)false;
			ret = -EINVAL;
		//} else {
		//	fbi->fix.smem_start = tcc_gem->dma_addr + addr_offset;
		}
	}
	/* -- */
	if (internal_ok) {
		/* tcc_gem->dma_addr can be added to addr_offset */
		fbi->fix.smem_start = tcc_gem->dma_addr + addr_offset;

		fbi->fix.smem_len = size;
		fb_helper->dev->mode_config.fb_base = tcc_gem->dma_addr;

		/* Initialize framebuffer memory with zero */
		memset_io(fbi->screen_base, 0, fbi->screen_size);
	}

	return ret;
}


static struct tcc_drm_gem *tccdrm_fbdev_alloc_gem(struct drm_device *dev,
						  const struct drm_fb_helper_surface_size *sizes,
						  struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct tcc_drm_gem *tcc_gem = NULL;
	unsigned long size, pitch, height;
	bool internal_ok = (bool)true;
	unsigned int utmp;

	if (dev == NULL) {
		internal_ok = (bool)false;
	}
	if (sizes == NULL) {
		internal_ok = (bool)false;
	}
	if (mode_cmd == NULL) {
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		mode_cmd->width = sizes->surface_width;
		mode_cmd->height = sizes->surface_height;

		/*
		* mode_cmd->pitches[0] =
		*	ALIGN(sizes->surface_width *
		*	      DIV_ROUND_UP(sizes->surface_bpp, 8), 8);
		*/
		/* coverity[cert_int30_c] */
		/* coverity[misra_c_2012_rule_10_4] */
		utmp = DIV_ROUND_UP(sizes->surface_bpp, 8U);
		if (!tcc_math_check_uint_mul_uint(sizes->surface_width, utmp)) {
			internal_ok = (bool)false;
		}
		if (internal_ok) {
			utmp *= sizes->surface_width;
			/* coverity[misra_c_2012_rule_10_4] */
			mode_cmd->pitches[0] = ALIGN(utmp, 8U);
		}
		/* -- */
		if (internal_ok) {
			mode_cmd->pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
									sizes->surface_depth);
			/* size = mode_cmd->pitches[0] * mode_cmd->height; */
			pitch = (unsigned long)mode_cmd->pitches[0];
			height = (unsigned long)mode_cmd->height;

			if (!tcc_math_check_ulong_mul_ulong(pitch, height)) {
				internal_ok = (bool)false;
			//} else {
			//	size = pitch * height;
			}
			/* -- */
		}
		if (internal_ok) {
			/* pitch can be multiplied to height */
			size = pitch * height;

			tcc_gem = tcc_drm_gem_create(dev, TCC_BO_CONTIG, size);
		}
	}
	return tcc_gem;
}


static int tccdrm_fbdev_alloc_fb(struct drm_fb_helper *fb_helper,
						 struct tcc_drm_gem *tcc_gem,
						 struct drm_fb_helper_surface_size *sizes,
						 const struct drm_mode_fb_cmd2 *mode_cmd)
{
	//bool need_destroy_framebuffer = (bool)false;
	struct tccdrm_fbdev *tcc_fbdev;
	struct drm_device *dev;

	bool internal_ok = (bool)true;
	int ret = 0;

	if (fb_helper == NULL) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}
	if (tcc_gem == NULL) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}
	if (sizes == NULL) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}
	if (mode_cmd == NULL) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}
	if (internal_ok) {
		dev = fb_helper->dev;

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
		tcc_fbdev = to_tcc_fbdev(fb_helper);

		tcc_fbdev->tcc_gem = tcc_gem;
		fb_helper->fb = tccdrm_fb_alloc(dev, mode_cmd, &tcc_gem, 1);
		if (IS_ERR(fb_helper->fb) || (fb_helper->fb == NULL)) {
			DRM_ERROR("failed to create drm framebuffer.\n");
			internal_ok = (bool)false;
			/* coverity[misra_c_2012_rule_10_3] */
			/* coverity[misra_c_2012_rule_11_2] */
			ret = PTR_ERR(fb_helper->fb);
		}
		if (internal_ok) {
			/*
			* fb_helper->fb is allocated by tccdrm_fb_alloc.
			* it will be destroyed when this function is failed.
			*/
			//need_destroy_framebuffer = (bool)true;

			ret = tccdrm_fbdev_update(fb_helper, sizes, tcc_gem);
			if (ret < 0) {
				/*
		 		 * if failed, all resources allocated above
				 * would be released by drm_mode_config_cleanup()
				 * when drm_load() had been called prior to any
				 * specific driver such as lcd or hdmi driver.
		 		 */
				drm_framebuffer_cleanup(fb_helper->fb);
			}
		}
	}
	return ret;
}

static int tccdrm_fbdev_probe(struct drm_fb_helper *fb_helper,
				    struct drm_fb_helper_surface_size *sizes)
{
	struct tcc_drm_gem *tcc_gem;
	struct drm_device *dev;

	struct drm_mode_fb_cmd2 mode_cmd = { 0 };
	bool internal_ok = (bool)true;
	int ret = 0;

	if (fb_helper == NULL) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}
	if (sizes == NULL) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}
	if (internal_ok) {
		DRM_DEBUG_KMS("surface width(%d), height(%d) and bpp(%d\n",
			sizes->surface_width, sizes->surface_height,
			sizes->surface_bpp);

		dev = fb_helper->dev;

		tcc_gem = tccdrm_fbdev_alloc_gem(dev,
						 (const struct drm_fb_helper_surface_size *)sizes, &mode_cmd);
		/* coverity[misra_c_2012_rule_11_2] */
		if (IS_ERR(tcc_gem) || (tcc_gem == NULL)) {
			internal_ok = (bool)false;
			/* coverity[misra_c_2012_rule_10_3] */
			/* coverity[misra_c_2012_rule_11_2] */
			ret = PTR_ERR(tcc_gem);
		}
	}

	if (internal_ok) {
		ret = tccdrm_fbdev_alloc_fb(fb_helper, tcc_gem, sizes,
					    (const struct drm_mode_fb_cmd2 *)&mode_cmd);

		if (ret < 0) {
			tcc_drm_gem_destroy(tcc_gem);
		}
	}

	return ret;
}

static const struct drm_fb_helper_funcs tcc_drm_fb_helper_funcs = {
	.fb_probe =	tccdrm_fbdev_probe,
};

int tccdrm_fbdev_init(struct drm_device *dev)
{
	struct tccdrm_fbdev *fbdev;
	/* coverity[misra_c_2012_rule_11_5] */
	struct tccdrm_private *private =
		(struct tccdrm_private *)dev->dev_private;
	bool need_free_fbdev = (bool)false;
	bool need_clear_fb_helper_init = (bool)false;
	bool internal_ok = (bool)true;

	int ret = 0;

	if ((dev->mode_config.num_crtc == 0) ||
	    (dev->mode_config.num_connector == 0)) {
		DRM_DEV_ERROR(dev->dev,
			      "Please check Kconfig or device-tree: number of CRTC<%d>, number of Connector <%d>\r\n",
			      dev->mode_config.num_crtc,
			      dev->mode_config.num_connector);
		internal_ok = (bool)false;
		ret = -EINVAL;
	}

	if (internal_ok) {
		/* coverity[misra_c_2012_rule_10_8] */
		/* coverity[misra_c_2012_rule_11_5] */
		fbdev = kzalloc(sizeof(*fbdev), GFP_KERNEL);
		if (fbdev == NULL) {
			internal_ok = (bool)false;
			ret = -ENOMEM;
		} else {
			need_free_fbdev = (bool)true;
		}
	}
	if (internal_ok) {
		private->fb_helper = &fbdev->fb_helper;
		drm_fb_helper_prepare(
			dev, &fbdev->fb_helper, &tcc_drm_fb_helper_funcs);
		#if defined(CONFIG_REFCODE_PRE_K510)
		ret = drm_fb_helper_init(dev, &fbdev->fb_helper, MAX_CONNECTOR);
		#else
		ret = drm_fb_helper_init(dev, &fbdev->fb_helper);
		#endif
		if (ret < 0) {
			internal_ok = (bool)false;
			DRM_DEV_ERROR(dev->dev,
				    "failed to initialize drm fb helper.\n");
		} else {
			need_clear_fb_helper_init = (bool)true;
		}
	}
	#if defined(CONFIG_REFCODE_PRE_K510)
	if (internal_ok) {
		ret = drm_fb_helper_single_add_all_connectors(&fbdev->fb_helper);
		if (ret < 0) {
			DRM_DEV_ERROR(dev->dev,
				    "failed to register drm_fb_helper_connector.\n");
			internal_ok = (bool)false;
		}
	}
	#endif
	if (internal_ok) {
		ret = drm_fb_helper_initial_config(private->fb_helper, PREFERRED_BPP);
		if (ret < 0) {
			DRM_DEV_ERROR(dev->dev,
				    "failed to set up hw configuration.\n");
			internal_ok = (bool)false;
		}
	}
	if (!internal_ok) {
		if (need_clear_fb_helper_init) {
			drm_fb_helper_fini(private->fb_helper);
		}

		if (need_free_fbdev) {
			private->fb_helper = NULL;
		}
	}
	return ret;
}

/* coverity[misra_c_2012_rule_8_13] */
static void tccdrm_fbdev_destroy(struct drm_device *dev,
				      struct drm_fb_helper *fb_helper)
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
	struct tccdrm_fbdev *tcc_fbd = to_tcc_fbdev(fb_helper);
	const struct tcc_drm_gem *tcc_gem = tcc_fbd->tcc_gem;
	struct drm_framebuffer *fb;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter dev is not used in
	 * the function.
	 */
	(void)dev;

	vunmap(tcc_gem->kvaddr);

	/* release drm framebuffer and real buffer */
	if ((fb_helper->fb != NULL) &&
	    (fb_helper->fb->funcs != NULL)) {
		fb = fb_helper->fb;
		if (fb != NULL) {
			drm_framebuffer_remove(fb);
		}
	}

	drm_fb_helper_unregister_fbi(fb_helper);

	drm_fb_helper_fini(fb_helper);
}

void tccdrm_fbdev_fini(struct drm_device *dev)
{
	/* coverity[misra_c_2012_rule_11_5] */
	struct tccdrm_private *private =
		(struct tccdrm_private *)dev->dev_private;
	const struct tccdrm_fbdev *fbdev;

	if ((private != NULL) && (private->fb_helper != NULL)) {
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
		fbdev = to_tcc_fbdev(private->fb_helper);

		tccdrm_fbdev_destroy(dev, private->fb_helper);
		kfree(fbdev);
		fbdev = NULL;
		private->fb_helper = NULL;
	}
}

/* coverity[misra_c_2012_rule_8_13] */
void tccdrm_output_poll_changed(struct drm_device *dev)
{
	/* coverity[misra_c_2012_rule_11_5] */
	const struct tccdrm_private *private =
		(const struct tccdrm_private *)dev->dev_private;
	struct drm_fb_helper *fb_helper = private->fb_helper;

	if (drm_fb_helper_hotplug_event(fb_helper) != 0) {
		DRM_DEV_ERROR(dev->dev,
			      "%s line(%d) failed to call drm_fb_helper_hotplug_event\r\n ",
			      __func__, __LINE__);
	}
}

