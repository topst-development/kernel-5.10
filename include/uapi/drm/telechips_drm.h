/* telechips_drm.h
 *
 * Copyright (C) 2022 Telechips
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
 * @note Tab size is 8.
 */

#ifndef UAPI_TELECHIPS_DRM_HEADER
#define UAPI_TELECHIPS_DRM_HEADER

#include <drm/drm.h>

/**
 * User-desired buffer creation information structure.
 *
 * @size: user-desired memory allocation size.
 *	- this size value would be page-aligned internally.
 * @flags: user request for setting memory type or cache attributes.
 * @handle: returned a handle to created gem object.
 *	- this handle will be set by gem module of kernel side.
 */
struct drm_tcc_gem_create {
	uint64_t size;
	unsigned int flags;
	unsigned int handle;
};

/**
 * A structure to gem information.
 *
 * @handle: a handle to gem object created.
 * @flags: flag value including memory type and cache attribute and
 *	this value would be set by driver.
 * @size: size to memory region allocated by gem and this size would
 *	be set by driver.
 */
struct drm_tcc_gem_info {
	unsigned int handle;
	unsigned int flags;
	uint64_t size;
};

#define TCC_GEM_CPU_PREP_READ	((unsigned int)1U << 0U)
#define TCC_GEM_CPU_PREP_WRITE	((unsigned int)1U << 1U)
#define TCC_GEM_CPU_PREP_NOWAIT	((unsigned int)1U << 2U)

#define TCC_GEM_CPU_PREP_FLAGS	( \
					TCC_GEM_CPU_PREP_READ | \
					TCC_GEM_CPU_PREP_WRITE | \
					TCC_GEM_CPU_PREP_NOWAIT)

struct drm_tcc_gem_cpu_prep {
	__u32 handle; /* in */
	__u32 flags; /* in, mask of TCC_GEM_CPU_PREP_x */
};

struct drm_tcc_gem_cpu_fini {
	__u32 handle; /* in */
};

struct drm_tcc_edid {
	__u32 crtc_id; /* in */
	__u8 data[512];
};

/* memory type definitions. */
/* Physically Continuous memory and used as default. */
#define TCC_BO_CONTIG		((unsigned int)0x0U)

/* Physically Non-Continuous memory. */
#define TCC_BO_NONCONTIG	((unsigned int)0x1U)

/* non-cachable mapping and used as default. */
#define TCC_BO_NONCACHABLE	((unsigned int)0x0U)

/* cachable mapping. */
#define TCC_BO_CACHABLE		((unsigned int)0x2U)

/* write-combine mapping. */
#define TCC_BO_WC		((unsigned int)0x4U)

#define TCC_BO_MASK (TCC_BO_NONCONTIG | TCC_BO_CACHABLE | TCC_BO_WC)


/**
 * A structure for getting a fake-offset that can be used with mmap.
 *
 * @handle: handle of gem object.
 * @reserved: just padding to be 64-bit aligned.
 * @offset: a fake-offset of gem object.
 */
struct drm_tcc_gem_map {
	__u32 handle;
	__u32 reserved;
	__u64 offset;
};

#define DRM_TCC_GEM_CREATE		0x00
/* Reserved 0x03 ~ 0x05 for tcc specific gem ioctl */
#define DRM_TCC_GEM_MAP			0x01
#define DRM_TCC_GEM_CPU_PREP            0x02
#define DRM_TCC_GEM_CPU_FINI            0x03

#define DRM_TCC_GEM_GET			0x04
#define DRM_TCC_GET_EDID		0x10 // Additional crtc ioctl for edid

#define DRM_IOCTL_TCC_GEM_CREATE	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TCC_GEM_CREATE, struct drm_tcc_gem_create)

#define DRM_IOCTL_TCC_GEM_MAP		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TCC_GEM_MAP, struct drm_tcc_gem_map)

#define DRM_IOCTL_TCC_GEM_GET	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TCC_GEM_GET,	struct drm_tcc_gem_info)

#define DRM_IOCTL_TCC_GEM_CPU_PREP	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TCC_GEM_CPU_PREP, struct drm_tcc_gem_cpu_prep)

#define DRM_IOCTL_TCC_GEM_CPU_FINI	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TCC_GEM_CPU_FINI, struct drm_tcc_gem_cpu_fini)

#define DRM_IOCTL_TCC_GET_EDID	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_TCC_GET_EDID, struct drm_tcc_edid)
#endif /* UAPI_TELECHIPS_DRM_HEADER */
