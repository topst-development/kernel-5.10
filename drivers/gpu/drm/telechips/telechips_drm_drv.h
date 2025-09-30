/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * telechips_drm_drv.h
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

#ifndef TCC_DRM_DRV_HEADER
#define TCC_DRM_DRV_HEADER

#if defined(CONFIG_REFCODE_PRE_K510)
#include <drm/drmP.h>
#endif
#include <linux/module.h>

#define MAX_CRTC	3
#define MAX_PLANE	4
#define MAX_FB_BUFFER	3

#define DEFAULT_WIN	0

#define to_tcc_plane(x)	container_of(x, struct tcc_drm_plane, base)


struct tccdrm_versions {
        /** @major: driver major number */
        int major;
        /** @minor: driver minor number */
        int minor;
        /** @patchlevel: driver patch level */
        int patchlevel;
        /** @name: driver name */
        const char *name;
        /** @date: driver date */
        const char *date;
};


/*
 * TCC DRM plane configuration structure.
 *
 * @zpos: initial z-position of the plane.
 * @type: type of the plane (primary, cursor or overlay).
 * @pixel_formats: supported pixel formats.
 * @num_pixel_formats: number of elements in 'pixel_formats'.
 * @capabilities: supported features (see TCC_DRM_PLANE_CAP_*)
 * @virt_addr: address of telechips rdma node register
 */

struct tcc_drm_plane_config {
	unsigned int zpos;
	enum drm_plane_type type;
	const uint32_t *pixel_formats;
	unsigned int num_pixel_formats;
	unsigned int capabilities;

	void __iomem *virt_addr;
};

struct drm_tcc_file_private {
	struct device *ipp_dev;
};

/*
 * TCC drm private structure.
 *
 * @da_start: start address to device address space.
 *	with iommu, device address space starts from this address
 *	otherwise default one.
 * @da_space_size: size of device address space.
 *	if 0 then default value is used for it.
 * @pending: the crtcs that have pending updates to finish
 * @lock: protect access to @pending
 * @wait: wait an atomic commit to finish
 */
struct tccdrm_private {
	struct drm_fb_helper *fb_helper;
	struct drm_atomic_state *suspend_state;
	void *mapping;
	/*
	 * This is needed for devices that don't already have their own dma
	 * parameters structure, e.g. platform devices, and, if necessary, will
	 * be assigned to the 'struct device' during device initialisation. It
	 * should therefore never be accessed directly via this structure as
	 * this may not be the version of dma parameters in use.
	 */
	struct device_dma_parameters dma_parms;

	/* for atomic commit */
	u32 pending;
	spinlock_t lock;
	wait_queue_head_t wait;
};
#endif
