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

#include "tcc-mipi-csi2-cfg-reg.h"
#include "../tcc-mipi-csi2.h"
#include "../tcc-mipi-csi2-helper.h"

static void
tcc_mipi_csi2_cfg_reset_dphy(const struct tcc_mipi_csi2_state *state,
			     uint32_t reset)
{
	uint32_t val = 0U, offset = 0U, mask = 0U, shift = 0U;

	offset = CAM_SWRST0;
	mask = ((state->pdev->id == 1) ? CAM_SWRST0_MIPI1_PHY_S_SWRST_MASK :
					 CAM_SWRST0_MIPI0_PHY_S_SWRST_MASK);
	shift = ((state->pdev->id == 1) ? CAM_SWRST0_MIPI1_PHY_S_SWRST_SHIFT :
					 CAM_SWRST0_MIPI0_PHY_S_SWRST_SHIFT);

	val = tcc_mipi_csi2_readl(state->cfg_base, offset);
	val &= ~(mask);

	if (reset == TCC_MIPI_RESET_RELEASE) {
		/* release */
		val |= ((uint32_t)0x0U << shift);
	} else {
		/* reset */
		val |= ((uint32_t)0x1U << shift);
	}

	tcc_mipi_csi2_writel(val, state->cfg_base, offset);
}

static void
tcc_mipi_csi2_cfg_reset_gd(const struct tcc_mipi_csi2_state *state,
			   uint32_t reset)
{
	uint32_t val = 0U, offset = 0U, mask = 0U, shift0 = 0U, shift1 = 0U;

	offset = CAM_SWRST0;
	mask = ((state->pdev->id == 1) ?
			(CAM_SWRST0_MIPI1_GDP_PIX_SWRST_MASK |
			CAM_SWRST0_MIPI1_GDP_APB_SWRST_MASK) :
			(CAM_SWRST0_MIPI0_GDP_PIX_SWRST_MASK |
			CAM_SWRST0_MIPI0_GDP_APB_SWRST_MASK));
	shift0 =
		((state->pdev->id == 1) ? CAM_SWRST0_MIPI1_GDP_PIX_SWRST_SHIFT :
					  CAM_SWRST0_MIPI0_GDP_PIX_SWRST_SHIFT);
	shift1 =
		((state->pdev->id == 1) ? CAM_SWRST0_MIPI1_GDP_APB_SWRST_SHIFT :
					  CAM_SWRST0_MIPI0_GDP_APB_SWRST_SHIFT);

	val = tcc_mipi_csi2_readl(state->cfg_base, offset);
	val &= ~(mask);

	if (reset == TCC_MIPI_RESET_RESET) {
		val |= (((uint32_t)0x1U << shift0) |
			((uint32_t)0x1U << shift1));
	}

	tcc_mipi_csi2_writel(val, state->cfg_base, offset);
}

static void
tcc_mipi_csi2_cfg_set_vpol(const struct tcc_mipi_csi2_state *state,
			   uint32_t ch,
			   uint32_t pol)
{
	uint32_t val = 0U, offset = 0U;

	offset = MIPI0_CSI_CTL + ((state->pdev->id == 1) ? (0x10U) : (0x0U));

	/* TODO:
	 * polarity 확인
	 */
	if (ch >= MAX_VC) {
		loge(&(state->pdev->dev),
			"ch(%d) is not valid\n", ch);
	} else {
		val = tcc_mipi_csi2_readl(state->cfg_base, offset);
		val &= ~(MIPI0_CSI_CTL_CVI0_MASK << ch);

		val |= (pol << (MIPI0_CSI_CTL_CVI0_SHIFT + ch));

		tcc_mipi_csi2_writel(val, state->cfg_base, offset);
	}
}

static void tcc_mipi_csi2_cfg_chmux(const struct tcc_mipi_csi2_state *state,
				    uint32_t mux,
				    uint32_t sel)
{
	uint32_t val = 0U, offset = 0U;

	offset = MIPI0_CSI_CTL + ((mux > 3U) ? (0x10U) : (0x0U));
	mux %= 4U;

	val = tcc_mipi_csi2_readl(state->cfg_base, offset);
	val &= ~(MIPI0_CSI_CTL_CH0_MASK << mux);
	val |= (sel << (MIPI0_CSI_CTL_CH0_SHIFT + mux));
	tcc_mipi_csi2_writel(val, state->cfg_base, offset);
}

static void
tcc_mipi_csi2_cfg_set_isp_bp(const struct tcc_mipi_csi2_state *state)
{
	uint32_t val = 0U;

	val = tcc_mipi_csi2_readl(state->cfg_base, ISP_CTL);

	val &= ~(ISP_CTL_I0B_MASK |
		 ISP_CTL_I1B_MASK |
		 ISP_CTL_I2B_MASK |
		 ISP_CTL_I3B_MASK);

	val |= ((state->isp_bypass[0] << ISP_CTL_I0B_SHIFT) |
		(state->isp_bypass[1] << ISP_CTL_I1B_SHIFT) |
		(state->isp_bypass[2] << ISP_CTL_I2B_SHIFT) |
		(state->isp_bypass[3] << ISP_CTL_I3B_SHIFT));

	tcc_mipi_csi2_writel(val, state->cfg_base, ISP_CTL);
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

void tcc_mipi_csi2_cfg_monitor(const struct tcc_mipi_csi2_state *state,
			       uint32_t onOff)
{
	/* TODO:
	 */
	//mem_wr32(CAMB_PERF_CFG_ADDR + 0x320 , 0x0000000F);
}
