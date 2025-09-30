// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mediabus.h>
#include <linux/of_graph.h>
#include <video/telechips/vioc_ddicfg.h>

#include "tcc-mipi-csi2-cfg-reg.h"
#include "../tcc-mipi-csi2.h"
#include "../tcc-mipi-csi2-helper.h"

static void
tcc_mipi_csi2_cfg_reset_dphy(const struct tcc_mipi_csi2_state *state,
			     uint32_t reset)
{
	uint32_t val = 0U, offset = 0U;

	offset = CSI0_CFG + ((state->pdev->id == 1) ? (0x4U) : (0x0U));

	val = tcc_mipi_csi2_readl(state->cfg_base, offset);
	val &= ~(CSI_CFG_S_RESETN_MASK);

	if (reset == TCC_MIPI_RESET_RELEASE) {
		/* 0x1 : release, 0x0 : reset */
		val |= ((uint32_t)0x1U << CSI_CFG_S_RESETN_SHIFT);
	}

	tcc_mipi_csi2_writel(val, state->cfg_base, offset);
}

static void
tcc_mipi_csi2_cfg_reset_gd(const struct tcc_mipi_csi2_state *state,
			   uint32_t reset)
{
	uint32_t val = 0U, offset = 0U;

	offset = CSI0_CFG + ((state->pdev->id == 1) ? (0x4U) : (0x0U));

	val = tcc_mipi_csi2_readl(state->cfg_base, offset);
	val &= ~(CSI_CFG_GEN_PX_RST_MASK | CSI_CFG_GEN_APB_RST_MASK);

	if (reset == TCC_MIPI_RESET_RESET) {
		val |= (((uint32_t)0x1U << CSI_CFG_GEN_PX_RST_SHIFT) |
			((uint32_t)0x1U << CSI_CFG_GEN_APB_RST_SHIFT));
	}

	tcc_mipi_csi2_writel(val, state->cfg_base, offset);
}

static void
tcc_mipi_csi2_cfg_set_vpol(const struct tcc_mipi_csi2_state *state,
			   uint32_t ch,
			   uint32_t pol)
{
	uint32_t val = 0U, offset = 0U;

	offset = CSI0_CFG + ((state->pdev->id == 1) ? (0x4U) : (0x0U));

	if (ch >= MAX_VC) {
		loge(&(state->pdev->dev),
			"ch(%d) is not valid\n", ch);
	} else {
		val = tcc_mipi_csi2_readl(state->cfg_base, offset);
		val &= ~(CSI_CFG_VSYNC_INV0_MASK << ch);

		val |= (pol << (CSI_CFG_VSYNC_INV0_SHIFT + ch));

		tcc_mipi_csi2_writel(val, state->cfg_base, offset);
	}
}

static void tcc_mipi_csi2_cfg_chmux(const struct tcc_mipi_csi2_state *state,
				    uint32_t mux,
				    uint32_t sel)
{
	uint32_t val = 0U, offset = 0U;

	offset = CSI0_CFG + ((mux > 3U) ? (0x4U) : (0x0U));
	mux %= 4U;

	val = tcc_mipi_csi2_readl(state->cfg_base, offset);
	val &= ~(CSI_CFG_MIPI_CHMUX_0_MASK << mux);
	val |= (sel << (CSI_CFG_MIPI_CHMUX_0_SHIFT + mux));
	tcc_mipi_csi2_writel(val, state->cfg_base, offset);
}

static void
tcc_mipi_csi2_cfg_set_isp_bp(const struct tcc_mipi_csi2_state *state)
{
	uint32_t val = 0U;

	val = tcc_mipi_csi2_readl(state->cfg_base, ISP_BYPASS);

	val &= ~(ISP_BYPASS_ISP0_BYP_MASK |
		 ISP_BYPASS_ISP1_BYP_MASK |
		 ISP_BYPASS_ISP2_BYP_MASK |
		 ISP_BYPASS_ISP3_BYP_MASK);

	val |= ((state->isp_bypass[0] << ISP_BYPASS_ISP0_BYP_SHIFT) |
		(state->isp_bypass[1] << ISP_BYPASS_ISP1_BYP_SHIFT) |
		(state->isp_bypass[2] << ISP_BYPASS_ISP2_BYP_SHIFT) |
		(state->isp_bypass[3] << ISP_BYPASS_ISP3_BYP_SHIFT));

	tcc_mipi_csi2_writel(val, state->cfg_base, ISP_BYPASS);
}

static inline void
tcc_mipi_csi2_cfg_set_path(const struct tcc_mipi_csi2_state *state)
{
	uint32_t idx = 0U;

	for (idx = 0U; idx < CSI_CFG_MIPI_CHMUX_MAX; idx++) {
		/* set mipi chmux*/
		tcc_mipi_csi2_cfg_chmux(state,
					idx,
					state->mipi_chmux[idx]);
	}

	for (idx = 0U; idx < MAX_VC; idx++) {
		/* set vsync polarity */
		tcc_mipi_csi2_cfg_set_vpol(state, idx, 1U);
	}

	tcc_mipi_csi2_cfg_set_isp_bp(state);
}

void tcc_mipi_csi2_cfg_reset(const struct tcc_mipi_csi2_state *state,
			     uint32_t reset)
{
	if (reset == 1U) {
		/* reset state */
		tcc_mipi_csi2_cfg_reset_dphy(state, TCC_MIPI_RESET_RESET);
		tcc_mipi_csi2_cfg_reset_gd(state, TCC_MIPI_RESET_RESET);
	} else {
		/* release state */
		tcc_mipi_csi2_cfg_reset_dphy(state, TCC_MIPI_RESET_RELEASE);
		tcc_mipi_csi2_cfg_reset_gd(state, TCC_MIPI_RESET_RELEASE);
		tcc_mipi_csi2_cfg_set_path(state);
	}
}

