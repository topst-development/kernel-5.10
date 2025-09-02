/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2016 Telechips Inc.
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *
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

#ifndef TCC_DRM_FBDEV_HEADER
#define TCC_DRM_FBDEV_HEADER

#ifdef CONFIG_DRM_FBDEV_EMULATION

int tccdrm_fbdev_init(struct drm_device *dev);
void tccdrm_fbdev_fini(struct drm_device *dev);
void tccdrm_output_poll_changed(struct drm_device *dev);

#else
static inline int tccdrm_fbdev_init(struct drm_device *dev)
{
	return 0;
}

static inline void tccdrm_fbdev_fini(struct drm_device *dev)
{

}

#define tccdrm_output_poll_changed (NULL)
#endif
#endif
