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

#ifdef CONFIG_ARCH_TCC750X
#include "../../750x/tcc-mipi-csi2-cfg-reg.h"
#else
#include "../../csi2_s/v1.2/tcc-mipi-csi2-csis-reg.h"
#endif
#include "../../tcc-mipi-csi2.h"
#include "../../tcc-mipi-csi2-helper.h"

static void
tcc_mipi_csi2_dphys_set_dphy_bctrl(const struct tcc_mipi_csi2_state *state,
				   uint32_t high_part, uint32_t low_part)
{
	uint32_t val_l = low_part;
	uint32_t val_h = high_part;

#ifdef CONFIG_ARCH_TCC750X
	tcc_mipi_csi2_writel(val_l, state->cfg_base, MIPI_PHY_SCTRL1);
	tcc_mipi_csi2_writel(val_h, state->cfg_base, MIPI_PHY_SCTRL2);
#else
	tcc_mipi_csi2_writel(val_l, state->csi_base, DPHY_BCTRL_L);
	tcc_mipi_csi2_writel(val_h, state->csi_base, DPHY_BCTRL_H);
#endif
}

static void
tcc_mipi_csi2_dphys_set_dphy_sctrl(const struct tcc_mipi_csi2_state *state,
				   uint32_t high_part, uint32_t low_part)
{
	uint32_t val_l = low_part;
	uint32_t val_h = high_part;

#ifdef CONFIG_ARCH_TCC750X
	/* CD750XL-713:
	 * MIPI_PHY_SCTRL2[13]: enable manual skew calibration */
	tcc_mipi_csi2_writel(0x2000U, state->cfg_base, MIPI_PHY_SCTRL2);
	tcc_mipi_csi2_writel(val_l, state->cfg_base, MIPI_PHY_SCTRL3);
	tcc_mipi_csi2_writel(val_h, state->cfg_base, MIPI_PHY_SCTRL4);
#else
	tcc_mipi_csi2_writel(val_l, state->csi_base, DPHY_SCTRL_L);
	tcc_mipi_csi2_writel(val_h, state->csi_base, DPHY_SCTRL_H);
#endif
}

static void
tcc_mipi_csi2_dphys_set_dphy_cmn_ctrl(const struct tcc_mipi_csi2_state *state)
{
#ifdef CONFIG_ARCH_TCC750X
	uint32_t val = 0U;

	val |= ((state->hssettle << MIPI_PHY_SCTRL5_HSSETTLE_SHIFT) |
		(state->s_clksettlectl << MIPI_PHY_SCTRL5_S_CLKSETTLECTL_SHIFT));
	tcc_mipi_csi2_writel(val, state->cfg_base, MIPI_PHY_SCTRL5);

	val = 0U;
	val |= ((((uint32_t)1U) << MIPI_PHY_SCTRL6_ENABLE_CLK_SHIFT) |
		(state->s_dpdn_swap_clk << MIPI_PHY_SCTRL6_S_DPDN_SWAP_CLK_SHIFT) |
		(state->s_dpdn_swap_dat << MIPI_PHY_SCTRL6_S_DPDN_SWAP_DAT_SHIFT) |
		/* CD750X-713:
		 * MIPI_PHY_SCTRL6[16]: disable auto skew calibration */
		(0U << MIPI_PHY_SCTRL6_CSI_RXSKEW_EN_SHIFT));

	tcc_mipi_csi2_writel(val, state->cfg_base,
			     MIPI_PHY_SCTRL6);
#else
	uint32_t val = 0U;
	uint32_t data_lane = 0U;

	switch (state->data_lane_num) {
	case 4U:
		data_lane = (DCCTRL_ENABLE_DATA_LANE_3 |
			     DCCTRL_ENABLE_DATA_LANE_2 |
			     DCCTRL_ENABLE_DATA_LANE_1 |
			     DCCTRL_ENABLE_DATA_LANE_0);
		break;
	case 3U:
		data_lane = (DCCTRL_ENABLE_DATA_LANE_2 |
			     DCCTRL_ENABLE_DATA_LANE_1 |
			     DCCTRL_ENABLE_DATA_LANE_0);
		break;
	case 2U:
		data_lane = (DCCTRL_ENABLE_DATA_LANE_1 |
			     DCCTRL_ENABLE_DATA_LANE_0);
		break;
	case 1U:
		data_lane = DCCTRL_ENABLE_DATA_LANE_0;
		break;
	default:
		data_lane = DCCTRL_ENABLE_DATA_LANE_0;
		break;
	}

	val = tcc_mipi_csi2_readl(state->csi_base, DPHY_CMN_CTRL);

	val &= ~(DCCTRL_HSSETTLE_MASK |
		DCCTRL_S_CLKSETTLECTL_MASK |
		DCCTRL_S_BYTE_CLK_ENABLE_MASK |
		DCCTRL_S_DPDN_SWAP_CLK_MASK |
		DCCTRL_S_DPDN_SWAP_DAT_MASK |
		DCCTRL_ENABLE_DAT_MASK |
		DCCTRL_ENABLE_CLK_MASK);

	val |= ((state->hssettle << DCCTRL_HSSETTLE_SHIFT) |
		(state->s_clksettlectl << DCCTRL_S_CLKSETTLECTL_SHIFT) |
		(((uint32_t)1U) << DCCTRL_S_BYTE_CLK_ENABLE_SHIFT) |
		(state->s_dpdn_swap_clk << DCCTRL_S_DPDN_SWAP_CLK_SHIFT) |
		(state->s_dpdn_swap_dat << DCCTRL_S_DPDN_SWAP_DAT_SHIFT) |
		(data_lane << DCCTRL_ENABLE_DAT_SHIFT) |
		(((uint32_t)1U) << DCCTRL_ENABLE_CLK_SHIFT));

	tcc_mipi_csi2_writel(val, state->csi_base, DPHY_CMN_CTRL);
#endif
}

void tcc_mipi_csi2_dphys_set_dphys(const struct tcc_mipi_csi2_state *state)
{
	/*
	 * Set D-PHY control(Master, Slave)
	 * Refer to 7.2.3(B_DPHYCTL) in D-PHY datasheet
	 * 500 means ULPS EXIT counter value.
	 */
	tcc_mipi_csi2_dphys_set_dphy_bctrl(state, 0x00000000U, 0x000001f4U);

	/*
	 * Set D-PHY control(Slave)
	 * Refer to 7.2.5(S_DPHYCTL) in D-PHY datasheet
	 */
#ifdef CONFIG_ARCH_TCC750X
	/* CD750X-713:
	 * MIPI_PHY_SCTRL4[1]: disable auto skew calibration */
	tcc_mipi_csi2_dphys_set_dphy_sctrl(state, 0x90U, 0xfd008000U);
#else
	tcc_mipi_csi2_dphys_set_dphy_sctrl(state, 0x92U, 0xfd008000U);
#endif

	/*
	 * Set D-PHY Common control
	 */
	tcc_mipi_csi2_dphys_set_dphy_cmn_ctrl(state);
}
