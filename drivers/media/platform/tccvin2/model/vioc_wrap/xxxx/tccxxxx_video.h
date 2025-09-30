/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCCXXXX_VIDEO_H
#define TCCXXXX_VIDEO_H

/* vioc path */
#include <video/telechips/vioc_global.h>
#include <video/telechips/vioc_config.h>
#include <video/telechips/vioc_rdma.h>
#include <video/telechips/vioc_vin.h>
#include <video/telechips/vioc_viqe.h>
#include <video/telechips/vioc_deintls.h>
#include <video/telechips/vioc_scaler.h>
#include <video/telechips/vioc_wmix.h>
#include <video/telechips/vioc_wdma.h>
#include <video/telechips/vioc_intr.h>

#include <video/telechips/tcc_cam_ioctrl.h>

#define PGL_FORMAT (VIOC_IMG_FMT_ARGB8888)
#define PGL_BG_R (0xFFU)
#define PGL_BG_G (0xFFU)
#define PGL_BG_B (0xFFU)
#define PGL_BGM_R (PGL_BG_R & 0xF8U)
#define PGL_BGM_G (PGL_BG_G & 0xF8U)
#define PGL_BGM_B (PGL_BG_B & 0xF8U)

#define BYPASS_MODE (1U)

enum vin_comp {
	VIN_COMP_VIN,
	VIN_COMP_VIQE,
	VIN_COMP_SDEINTL,
	VIN_COMP_SCALER,
	VIN_COMP_PGL,
	VIN_COMP_WMIX,
	VIN_COMP_WDMA,
	VIN_COMP_MAX,
};

/* vioc interrupt */
struct vioc_intr {
	u32 reg;
	int num;
	struct vioc_intr_type source;
};

/* vioc component */
struct vioc_comp {
	/* device node */
	struct device_node *np;

	/* vioc type and index */
	u32 type;
	u32 index;

	/* vioc interrupt */
	struct vioc_intr intr;
};


struct tccvin_cif {
	u32 cif_port;
	void __iomem *cifport_addr;

	struct clk *vioc_clk;
	u32 use_pgl;
	struct vioc_comp vin_path[VIN_COMP_MAX];
	struct vin_lut vin_internal_lut;

	struct reserved_mem *rsvd_mem[RESERVED_MEM_MAX];

	u32 recovery_trigger;
	u32 wmix_bypass;
};

struct tccvin_format *tccvin_get_tccvin_format_by_pixelformat(u32 pixelformat);
u32 tccvin_video_get_pixelformat_by_index(u32 index);
#endif
