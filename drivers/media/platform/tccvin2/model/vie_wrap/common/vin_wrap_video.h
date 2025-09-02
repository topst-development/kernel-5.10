/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VIN_WRAP_VIDEO_H
#define VIN_WRAP_VIDEO_H

#include "vin_wrap_vin.h"
#include "vin_wrap_wdma.h"
#if defined(CONFIG_ARCH_TCC750X)
#include "../750x/vin_wrap_intr.h"
#include "../750x/vin_wrap_cfg.h"
#endif
#if defined(CONFIG_ARCH_TCC807X)
#include "../807x/vin_wrap_intr.h"
#include "../807x/vin_wrap_cfg.h"
#endif

#include <video/telechips/tcc_cam_ioctrl.h>

#define get_vioc_type(x)		((x) >> 8U)
#define get_vioc_index(x)		((x) & 0xFFU)

enum vin_comp {
	VIN_COMP_VIN,
	VIN_COMP_WDMA,
	VIN_COMP_MAX,
};

/* vioc interrupt */
struct vioc_intr {
	u32 reg;
	int num;
	struct vin_wrap_intr_type source;
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
	u32 cam_mux;
	void __iomem *cam_mux_addr;
	u32 cam_ch;

	struct clk *vioc_clk_axi;
	struct clk *vioc_clk_pix;
	struct clk *vioc_clk_apb;

	u32 use_pgl;
	struct vioc_comp vin_path[VIN_COMP_MAX];
	struct vin_lut vin_internal_lut;

	struct reserved_mem *rsvd_mem[RESERVED_MEM_MAX];

	u32 recovery_trigger;
};

struct tccvin_format *tccvin_get_tccvin_format_by_pixelformat(u32 pixelformat);
u32 tccvin_video_get_pixelformat_by_index(u32 index);
#endif
