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

#include "tcc-mipi-csi2-dphys-reg.h"
#include "../../tcc-mipi-csi2.h"
#include "../../tcc-mipi-csi2-helper.h"

void tcc_mipi_csi2_dphys_set_clock_lane(const struct tcc_mipi_csi2_state *state)
{
	tcc_mipi_csi2_writel(0x00001450U, state->phy_base, PHY_SC_GNR_CON1);
	tcc_mipi_csi2_writel(0x00008000U, state->phy_base, PHY_SC_ANA_CON1);
	tcc_mipi_csi2_writel(0x00000002U, state->phy_base, PHY_SC_ANA_CON2);
	tcc_mipi_csi2_writel(0x00000600U, state->phy_base, PHY_SC_ANA_CON3);
	tcc_mipi_csi2_writel(0x00000301U, state->phy_base, PHY_SC_TIME_CON0);
}

uint32_t
tcc_mipi_csi2_dphy_get_skew_dlysel(const struct tcc_mipi_csi2_state *state)
{
	uint32_t datarate, ret = 0;

	datarate = state->datarate;

	if (datarate >= 4000U && datarate < 6500U) {
		/* 4.0 Gbps <= Data rate <= 6.5 Gbsp : 2'b00 */
		ret = 0U;
	} else if (datarate >= 3000U && datarate < 4000U) {
		/* 3.0 Gbps <= Data rate <  4.0 Gbps : 2'b01 */
		ret = 1U;
	} else if (datarate >= 2000U && datarate < 3000U) {
		/* 2.0 Gbps <= Data rate <  3.0 Gbps : 2'b10 */
		ret = 2U;
	} else if (datarate >= 1000U && datarate < 2000U) {
		/* 1.0 Gbps <= Data rate <  2.0 Gbps : 2'b11 */
		ret = 3U;
	} else {
		/* 2'b00 for the rest */
		ret = 0U;
	}

	return ret;
}

uint32_t
tcc_mipi_csi2_dphy_get_settle_clk_sel(const struct tcc_mipi_csi2_state *state)
{
	uint32_t datarate, ret = 0;

	datarate = state->datarate;
	if (datarate >= 1500U) {
		/* 1'b0: when data rate is 1.5 Gbsp or above */
		ret = 0U;
	} else {
		/* 1'b1: when data rate is under 1.5 Gbsp */
		ret = 1U;
	}

	return ret;
}

void tcc_mipi_csi2_dphys_set_data_lane(const struct tcc_mipi_csi2_state *state,
				       uint32_t lane)
{
	uint32_t lane_offset = lane * 0x0100U;
	uint32_t val = 0U, settle_clk_sel = 0U, skew_dlysel = 0U;

	/* T_PHY_READY[15:0] */
	tcc_mipi_csi2_writel(0x00001450U, state->phy_base,
			     PHY_SD_GNR_CON1 + lane_offset);

	/* HS_RX_BIAS_CON[15:11] */
	tcc_mipi_csi2_writel(0x00008000U, state->phy_base,
			     PHY_SD_ANA_CON1 + lane_offset);

	/*
	 * RX_TERM_SW[2:0]
	 *
	 * SKEW_DLYSEL[9:8]
	 *	- 4.0 Gbps <= Data rate <= 6.5 Gbsp : 2'b00
	 *	- 3.0 Gbps <= Data rate <  4.0 Gbps : 2'b01
	 *	- 2.0 Gbps <= Data rate <  3.0 Gbps : 2'b10
	 *	- 1.0 Gbps <= Data rate <  2.0 Gbps : 2'b11
	 *	- 2'b00 for the rest
	 */
	skew_dlysel = tcc_mipi_csi2_dphy_get_skew_dlysel(state);
	val = 0x2U;
	val |= (skew_dlysel << PHY_SD_ANA_CON2_SKEW_DLYSEL_SHIFT);
	tcc_mipi_csi2_writel(val, state->phy_base,
			     PHY_SD_ANA_CON2 + lane_offset);

	/* ULPS_HYS_SW_DPHY[10:8] */
	tcc_mipi_csi2_writel(0x00000600U, state->phy_base,
			     PHY_SD_ANA_CON3 + lane_offset);

	/* CLK_DBL_CTRL[7:6] */
	tcc_mipi_csi2_writel(0x00000040U, state->phy_base,
			     PHY_SD_ANA_CON7 + lane_offset);

	/*
	 * SETTLE_CLK_SEL[8]
	 *	- 1'b0: when data rate is 1.5 Gbsp or above
	 *	- 1'b1: when data rate is under 1.5 Gbsp
	 *
	 * T_HS_SETTLE[7:0]
	 * refer to Supplement Guide
	 */
	settle_clk_sel = tcc_mipi_csi2_dphy_get_settle_clk_sel(state);
	val = ((settle_clk_sel << PHY_SD_TIME_CON0_SETTLE_CLK_SEL_SHIFT) |
	       (state->hssettle << PHY_SD_TIME_CON0_T_HS_SETTLE_SHIFT));
	tcc_mipi_csi2_writel(val, state->phy_base,
			     PHY_SD_TIME_CON0 + lane_offset);

	/*
	 * T_ERR_SOT_SYNC[7:0]
	 * refer to Supplement Guide
	 */
	tcc_mipi_csi2_writel(0x00000003U, state->phy_base,
			     PHY_SD_TIME_CON1 + lane_offset);

	/*
	 * SKEW_CAL_EN[0]
	 */
	val = (state->deskew_enable << PHY_SD_DESKEW_CON0_SKEW_CAL_EN_SHIFT);
	tcc_mipi_csi2_writel(val, state->phy_base,
			     PHY_SD_DESKEW_CON0 + lane_offset);

	/*
	 * SKEW_CAL_COARSE_MAX_SET[4:0]
	 * SKEW_CAL_FINE_MAX_SET[11:8]
	 */
	tcc_mipi_csi2_writel(0x0000081AU, state->phy_base,
			     PHY_SD_DESKEW_CON4 + lane_offset);
}

void
tcc_mipi_csi2_dphys_enable_clock_lane(const struct tcc_mipi_csi2_state *state,
				      bool on)
{
	if (on) {
		/* enable clock lane */
		tcc_mipi_csi2_writel(1U, state->phy_base, PHY_SC_GNR_CON0);
	} else {
		/* disable clock lane */
		tcc_mipi_csi2_writel(0U, state->phy_base, PHY_SC_GNR_CON0);
	}
}

void
tcc_mipi_csi2_dphys_enable_data_lane(const struct tcc_mipi_csi2_state *state,
				     uint32_t lane, bool on)
{
	uint32_t lane_offset = lane * 0x0100U;

	if (on) {
		/* enable clock lane */
		tcc_mipi_csi2_writel(1U, state->phy_base,
				     PHY_SD_GNR_CON0 + lane_offset);
	} else {
		/* disable clock lane */
		tcc_mipi_csi2_writel(0U, state->phy_base,
				     PHY_SD_GNR_CON0 + lane_offset);
	}
}
