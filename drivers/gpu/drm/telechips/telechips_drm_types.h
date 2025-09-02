/* SPDX-License-Identifier: GPL-2.0-or-later
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

#ifndef TCC_DRM_TYPES_HEADER_H
#define TCC_DRM_TYPES_HEADER_H

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic_state_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_vblank.h>
#include <drm/drm_print.h>
#include <drm/drm_panel.h>
#include <drm/telechips_drm.h>

#if defined(CONFIG_VIOC_PVRIC_FBDC)
#include <video/telechips/vioc_pvric_fbdc.h>
#include <pvrsrvkm/img_drm_fourcc.h>
#endif

#define DRM_INT_MAX				2147483647
/* DEFINES for CRTC ----------------------------------------------------------*/
#define CRTC_FLAGS_IRQ_BIT		0
#define CRTC_FLAGS_VCLK_BIT		1 /* Display BUS - pwdn/swreset for VIOC*/
#define CRTC_FLAGS_PCLK_BIT		2 /* Display BUS - Pixel Clock for LCDn (0-3) */

/*
 * CRTC_FLAGS_TIMING_CHECK_BIT
 * It is used to check whether the timing seted by the bootloader and the
 * timing seted by the kernelare  different.
 */
#define CRTC_FLAGS_TIMING_CHECK_BIT	3

/* Display has totally four hardware windows. */
#define CRTC_WIN_NR_MAX		4

#define TCCDRM_RDMA_MAX_NUM 4
/* Definitions for overlay priority of WMIX */
#define VIOC_WMIX_POR_OVP 5U
#define VIOC_WMIX_DRM_DEFAULT_OVP 24U

#if defined(CONFIG_DRM_TELECHIPS_CTRL_CHROMAKEY)
struct drm_chromakey_t {
	unsigned int red;
	unsigned int green;
	unsigned int blue;
};

struct drm_ioctl_chromakey_t {
	unsigned int crtc_index;
	unsigned int chromakey_layer;
	unsigned int chromakey_enable;
	struct drm_chromakey_t chromakey_value;
	struct drm_chromakey_t chromakey_mask;
};
#endif

struct vioc_fmt_t {
	unsigned int f_y2r;
	unsigned int f_swap;
	unsigned int f_fmt;
	#if defined(CONFIG_VIOC_PVRIC_FBDC)
	VIOC_PVRICCTRL_SWIZZ_MODE swizz;
	#endif
};

/*
 * TCC drm common overlay structure.
 *
 * @base: plane object
 * @index: hardware index of the overlay layer
 *
 * this structure is common to tcc SoC and its contents would be copied
 * to hardware specific overlay info.
 */
struct tcc_drm_plane {
	struct drm_plane base;
	unsigned int win;
	unsigned long caps;
	unsigned long plane_flags;
};

struct tccdrm_flip_state {
	struct drm_pending_vblank_event *flipevent;
	atomic_t flipstatus;
	/**
	 * @fasync:
	 *
	 * This is set when DRM_MODE_PAGE_FLIP_ASYNC is set in the legacy
	 * PAGE_FLIP IOCTL. It's not wired up for the atomic IOCTL itself yet.
	 */
	bool flipasync;
};

struct tcc_hw_block {
	void __iomem *virt_addr;
	/* Kernel IRQ NUMBER */
	unsigned int irq_num;

	/* VIOC INTR NUMBER VIOC_INTR_xxx */
	unsigned int intr_num;

	/* VIOC BLOCK NUMBER VIOC_xxx */
	unsigned int blk_num;
};

struct tcc_hw_device {
	struct clk *vioc_clock;
	struct clk *ddc_clock;
	struct tcc_hw_block display_device;
	struct tcc_hw_block wmixer;
	struct tcc_hw_block wdma;
	struct tcc_hw_block fbdc[TCCDRM_RDMA_MAX_NUM];
	struct tcc_hw_block rdma[TCCDRM_RDMA_MAX_NUM];
	unsigned int rdma_plane_type[TCCDRM_RDMA_MAX_NUM];

	/* rdma valid counts */
	int rdma_counts;

	/* DDIBUS LCD MUX */
	u32 lcdc_mux_select;
	u32 lcdc_mux_bypass;

	/*
	 * LVDS controller on TCC803x is not working normally until LVDS
	 * controller is reseted if reference clock of the display controller
	 * is disabled during operation. The tccdrm_viocs does not provide the
	 * function to reset the display controller on TCC803x yet.
	 * The tccdrm_vioc uses keep_pclk variable to determine whether to
	 * control the reference clock of the display controller. It sets this
	 * variable to true if target soc is TCC803x and connector type is
	 * DRM_MODE_CONNECTOR_LVDS.
	 */
	bool keep_pclk;

	/*
	 * 0: normal mode, 1: tccdrm_vioc only controls plane except crtc
	 */
	bool limited_plane_only_mode;
};

struct tcc_crtc_state {
	struct drm_crtc_state base;
	struct device *dev;

	/* The information that transfer from crtc to encoder */
	unsigned long pixel_clock;
	unsigned int lcdc_mux_select;
	unsigned int lcdc_mux_bypass;
	unsigned int lcdc_num;

	/* The information that transfer from encoder to crtc */
	struct drm_connector *connector;
	int connector_type;

	/* HDMI */
	unsigned long bus_format;
	unsigned long enc_out_encoding;
	/* --- */

	#if defined(CONFIG_DRM_TELECHIPS_CTRL_CHROMAKEY)
	int (*set_chromakey)(struct drm_crtc *crtc,
			     unsigned int chromakey_layer,
			     unsigned int chromakey_enable,
			     const struct drm_chromakey_t *value,
			     const struct drm_chromakey_t *mask);
	int (*get_chromakey)(struct drm_crtc *crtc,
			     unsigned int chromakey_layer,
			     unsigned int *chromakey_enable,
			     struct drm_chromakey_t *value,
			     struct drm_chromakey_t *mask);
	#endif
};

#define to_tcc_plane_state(plane_state) \
	container_of((plane_state), struct tcc_drm_plane_state, base)

#define to_tcc_crtc_state(x) \
	container_of((x), struct tcc_crtc_state, base)

#endif
