/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_MIPI_CSI2_H
#define TCC_MIPI_CSI2_H

#include <media/v4l2-mediabus.h>
#include <media/v4l2-subdev.h>
#ifdef CONFIG_ARCH_TCC803X
#include "803x/tcc-mipi-csi2-cfg-reg.h"
#include "803x/tcc-mipi-csi2-ckc-reg.h"
#include "csi2_s/v1.2/tcc-mipi-csi2-csis-reg.h"
#endif
#ifdef CONFIG_ARCH_TCC805X
#include "805x/tcc-mipi-csi2-cfg-reg.h"
#include "805x/tcc-mipi-csi2-ckc-reg.h"
#include "csi2_s/v1.2/tcc-mipi-csi2-csis-reg.h"
#endif
#ifdef CONFIG_ARCH_TCC750X
#include "750x/tcc-mipi-csi2-cfg-reg.h"
#include "750x/tcc-mipi-csi2-ckc-reg.h"
#include "csi2_s/v2.0/tcc-mipi-csi2-csis-reg.h"
#endif
#ifdef CONFIG_ARCH_TCC807X
#include "807x/tcc-mipi-csi2-cfg-reg.h"
#include "807x/tcc-mipi-csi2-ckc-reg.h"
#include "csi2_s/v2.1/tcc-mipi-csi2-csis-reg.h"
#endif

#ifndef ON
#define ON		1
#endif

#ifndef OFF
#define OFF		0
#endif

#define MIPI_CSI2_0	0
#define MIPI_CSI2_1	1

#define TCC_MIPI_CSI2_DRIVER_NAME "tcc-mipi-csi2"
#define TCC_MIPI_CSI2_SUBDEV_NAME TCC_MIPI_CSI2_DRIVER_NAME

#define TCC_MIPI_CSI2_PAD_SINK		(0U)
#define TCC_MIPI_CSI2_PAD_SRC0		(1U)
#define TCC_MIPI_CSI2_PAD_SRC1		(2U)
#define TCC_MIPI_CSI2_PAD_SRC2		(3U)
#define TCC_MIPI_CSI2_PAD_SRC3		(4U)

#define TCC_MIPI_CSI2_PAD_NUM_SINK	(1U)
#define TCC_MIPI_CSI2_PAD_NUM_SRC	(4U)
#define TCC_MIPI_CSI2_PAD_NUM		\
		(TCC_MIPI_CSI2_PAD_NUM_SINK + TCC_MIPI_CSI2_PAD_NUM_SRC)

struct tcc_mipi_csi2_isp_state {
	struct v4l2_mbus_framefmt fmt;

	uint32_t virtual_channel;
	uint32_t data_format;

	/* output */
	uint32_t pixel_mode;
};

/* CSI specific configuration */
struct csi_data {
	u32 ipi_adv_features;
	u32 ipi_auto_flush;
	u32 ipi_mode;
	u32 ipi_cut_through;
	u32 hsa;
	u32 hbp;
	u32 hsd;
	u32 htotal;
};

struct tcc_mipi_csi2_state {
	struct platform_device *pdev;
	struct v4l2_subdev sd;

	struct v4l2_dv_timings dv_timings;

	struct v4l2_async_notifier nf;

	void __iomem *csi_base;
	void __iomem *gdb_base;
	void __iomem *phy_base;
	void __iomem *ckc_base;
	void __iomem *cfg_base;
	void __iomem *ddicfg_base;

	uint32_t irq;
	uint32_t gdb_irq;

	struct clk *pixel_clock;
	uint32_t csi_pclk;

#if !defined(CONFIG_ARCH_TCC803X)
	uint32_t mipi_chmux[CSI_CFG_MIPI_CHMUX_MAX];
	uint32_t isp_bypass[CSI_CFG_ISP_BYPASS_MAX];
#endif

	/* CSIS common control parameter */
	uint32_t deskew_level;
	uint32_t deskew_enable;
	uint32_t interleave_mode;
	uint32_t data_lane_num;
	uint32_t update_shadow_ctrl;

	/* DPHY common control parameter */
	uint32_t hssettle;
	uint32_t s_clksettlectl;
	uint32_t s_dpdn_swap_clk;
	uint32_t s_dpdn_swap_dat;
	uint32_t datarate;

	/* ISP configuration parameter */
	uint32_t input_ch_num;
	uint32_t pixel_mode;

	/* dwc_mipi_csi2 data */
	struct csi_data hw;

	struct media_pad pads[TCC_MIPI_CSI2_PAD_NUM];
	struct v4l2_mbus_framefmt fmt[TCC_MIPI_CSI2_PAD_NUM_SINK];

	/* TODO: use v4l2_mbus_framefmt */
	struct tcc_mipi_csi2_isp_state isp_info[TCC_MIPI_CSI2_PAD_NUM_SRC];

	struct mutex lock;
	uint32_t use_cnt;
};

#endif
