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
	uint32_t val = 0U, offset = 0U;

	offset = CAM_SWRST;

	val = tcc_mipi_csi2_readl(state->cfg_base, offset);
	val &= ~(CAM_SWRST_MPSR_MASK);

	if (reset == TCC_MIPI_RESET_RELEASE) {
		/* 0x1 : release, 0x0 : reset */
		val |= ((uint32_t)0x1U << CAM_SWRST_MPSR_SHIFT);
	}

	tcc_mipi_csi2_writel(val, state->cfg_base, offset);
}

static void
tcc_mipi_csi2_cfg_set_vpol(const struct tcc_mipi_csi2_state *state,
			   uint32_t ch,
			   uint32_t pol)
{
	uint32_t val = 0U, offset = 0U;

	offset = MIPI_CSI_CTRL0;

	/* TODO:
	 * polarity 확인
	 */
	if (ch >= MAX_VC) {
		loge(&(state->pdev->dev),
			"ch(%d) is not valid\n", ch);
	} else {
		val = tcc_mipi_csi2_readl(state->cfg_base, offset);
		val &= ~(MIPI_CSI_CTRL0_CVI0_MASK << ch);

		val |= (pol << (MIPI_CSI_CTRL0_CVI0_SHIFT + ch));

		tcc_mipi_csi2_writel(val, state->cfg_base, offset);
	}
}

static void
tcc_mipi_csi2_cfg_set_hpol(const struct tcc_mipi_csi2_state *state,
			   uint32_t ch,
			   uint32_t pol)
{
	uint32_t val = 0U, offset = 0U;

	offset = MIPI_CSI_CTRL0;

	/* TODO:
	 * polarity 확인
	 */
	if (ch >= MAX_VC) {
		loge(&(state->pdev->dev),
			"ch(%d) is not valid\n", ch);
	} else {
		val = tcc_mipi_csi2_readl(state->cfg_base, offset);
		val &= ~(MIPI_CSI_CTRL0_CHI0_MASK << ch);

		val |= (pol << (MIPI_CSI_CTRL0_CHI0_SHIFT + ch));

		tcc_mipi_csi2_writel(val, state->cfg_base, offset);
	}
}

static void
tcc_mipi_csi2_cfg_set_isp_bp(const struct tcc_mipi_csi2_state *state)
{
	uint32_t val = 0U;

	val = tcc_mipi_csi2_readl(state->cfg_base, ISP_CTRL0);

	val &= ~(ISP_CTRL_0_I0B_MASK |
		 ISP_CTRL_0_I1B_MASK |
		 ISP_CTRL_0_I2B_MASK |
		 ISP_CTRL_0_I3B_MASK);

	val |= ((state->isp_bypass[0] << ISP_CTRL_0_I0B_SHIFT) |
		(state->isp_bypass[1] << ISP_CTRL_0_I1B_SHIFT) |
		(state->isp_bypass[2] << ISP_CTRL_0_I2B_SHIFT) |
		(state->isp_bypass[3] << ISP_CTRL_0_I3B_SHIFT));

	tcc_mipi_csi2_writel(val, state->cfg_base, ISP_CTRL0);
}

static void tcc_mipi_csi2_cfg_set_MP2SP(const struct tcc_mipi_csi2_state *state)
{
	uint32_t val = 0U, mode = 0U;

	/* set DT of MP2SP */
	val = ((state->isp_info[0].data_format << MIPI_CSI_CTRL2_MCI1D_SHIFT) |
	       (state->isp_info[1].data_format << MIPI_CSI_CTRL2_MCI2D_SHIFT) |
	       (state->isp_info[2].data_format << MIPI_CSI_CTRL2_MCI3D_SHIFT) |
	       (state->isp_info[3].data_format << MIPI_CSI_CTRL2_MCI4D_SHIFT));

	/*
	 * set mode of MP2SP
	 * 0: single pixel
	 * 1: multi pixel mode
	 */
	switch (state->isp_info[0].data_format) {
	case CSI_DT_RAW20:
	case CSI_DT_RAW24:
		mode = 1U;
		break;
	default:
		mode = 0U;
		break;
	}

	val |= ((mode << MIPI_CSI_CTRL2_MCI1M_SHIFT) |
		(mode << MIPI_CSI_CTRL2_MCI2M_SHIFT) |
		(mode << MIPI_CSI_CTRL2_MCI3M_SHIFT) |
		(mode << MIPI_CSI_CTRL2_MCI4M_SHIFT));

	tcc_mipi_csi2_writel(val, state->cfg_base, MIPI_CSI_CTRL2);
}

static inline void
tcc_mipi_csi2_cfg_set_path(const struct tcc_mipi_csi2_state *state)
{
	uint32_t idx = 0U;

	for (idx = 0U; idx < MAX_VC; idx++) {
		/* set vsync polarity */
		tcc_mipi_csi2_cfg_set_vpol(state, idx, 0U);
	}

	for (idx = 0U; idx < MAX_VC; idx++) {
		/* set hsync polarity */
		tcc_mipi_csi2_cfg_set_hpol(state, idx, 1U);
	}

	tcc_mipi_csi2_cfg_set_isp_bp(state);
	tcc_mipi_csi2_cfg_set_MP2SP(state);
}

void tcc_mipi_csi2_cfg_reset(const struct tcc_mipi_csi2_state *state,
			     uint32_t reset)
{
	if (reset == 1U) {
		/* reset state */
		tcc_mipi_csi2_cfg_reset_dphy(state, TCC_MIPI_RESET_RESET);
	} else {
		/* release state */
		tcc_mipi_csi2_cfg_reset_dphy(state, TCC_MIPI_RESET_RELEASE);
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
