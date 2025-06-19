// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <video/telechips/vioc_lvds.h>

#define LVDS_PHY_VCO_RANGE_MIN (560000000U)  // 560Mhz
#define LVDS_PHY_VCO_RANGE_MAX (1120000000U) // 1120Mhz
#define LVDS_PHY_UPSAMPLE_RATIO_MAX (0x4U)   // 0~4

#ifndef IDX_LVDS_WRAP
#define IDX_LVDS_WRAP 4
#endif

#ifndef ABS_DIFF
#define ABS_DIFF(a, b) (((a) > (b)) ? ((a) - (b)) : ((b) - (a)))
#endif

/* for upsample ratio calculation
 * n = upsample ratio
 * X = 2^n (X = 1, 2, 4, 8 16)
 */
static unsigned int ref_ratio_arr[5][2] = {
	{0, 1}, {1, 2}, {2, 4}, {3, 8}, {4, 16}
};
static void __iomem *pLVDS_reg[LVDS_PHY_PORT_MAX];
static void __iomem *pLVDS_wrap_reg;

void LVDS_PHY_LaneSwap(
	unsigned int s_port_en, unsigned int lvds_main, unsigned int lvds_sub,
	const unsigned int *lane_main, const unsigned int *lane_sub)
{
	unsigned int idx;

	for (idx = 0U; idx < 5U; idx++) {
		LVDS_PHY_SetLaneSwap(lvds_main, idx, lane_main[idx]);
		if (s_port_en != 0U) {
			/* Prevent KCS warning */
			LVDS_PHY_SetLaneSwap(lvds_sub, idx, lane_sub[idx]);
		}
	}
}

/* LVDS_PHY_GetCalibrationLevel
 * Get setting value for VCM/VSW calibration
 * vcm : typical vcm level of lvds panel
 * vsw : typical vsw level of lvds panel
 */
static void LVDS_PHY_GetCalibrationLevel(
	unsigned int vcm, unsigned int vsw, unsigned int *vcmcal,
	unsigned int *swingcal)
{
	unsigned int swing_max, swing_min;
	unsigned int idx, step;

	if (vcm < 770U) { /* 0 : 190~370*/
		*vcmcal = 0U;
		swing_min = 190U;
		swing_max = 370U;
	} else if (vcm < 870U) { /* 1 : 210 ~ 470*/
		*vcmcal = 1U;
		swing_min = 210U;
		swing_max = 470U;
	} else if (vcm < 960U) { /* 2 : 210 ~ 540 */
		*vcmcal = 2U;
		swing_min = 210U;
		swing_max = 540U;
	} else if (vcm < 1050U) { /* 3 : 210 ~ 570 */
		*vcmcal = 3U;
		swing_min = 210U;
		swing_max = 540U;
	} else if (vcm < 1130U) { /* 4 : 210 ~ 560 */
		*vcmcal = 4U;
		swing_min = 210U;
		swing_max = 560U;
	} else if (vcm < 1210U) { /* 5 : 210 ~ 530 */
		*vcmcal = 5U;
		swing_min = 210U;
		swing_max = 530U;
	} else if (vcm < 1290U) { /* 6 : 210 ~ 500 */
		*vcmcal = 6U;
		swing_min = 210U;
		swing_max = 500U;
	} else { /* 7 : 210 ~ 560 */
		*vcmcal = 7U;
		swing_min = 210U;
		swing_max = 560U;
	}

	step = (swing_max - swing_min) / 16U;
	for (idx = 0U; idx < 16U; idx++) {
		*swingcal = idx;
		if (vsw <= (swing_min + (step * idx))) {
			/* Prevent KCS warning */
			break;
		}
	}
}

/* LVDS_PHY_GetUpsampleRatio
 * Get upsample ratio value for Automatic FCON
 * p_port : the primary port number of lvds phy
 * s_port : the secondary port number of lvds phy
 * freq : lvds pixel clock
 */
/* HIS_GOTO */

unsigned int LVDS_PHY_GetUpsampleRatio(
	unsigned int p_port, unsigned int s_port, unsigned int freq)
{
	const void __iomem *p_reg = LVDS_PHY_GetAddress(p_port);
	const void __iomem *s_reg = LVDS_PHY_GetAddress(s_port);
	unsigned int idx = 0;

	if (p_reg != NULL) {
		unsigned int pxclk;

		if (s_reg != NULL) {
			/* Prevent KCS warning */
			pxclk = freq / 2U;
		} else {
			/* Prevent KCS warning */
			pxclk = freq;
		}

		for (idx = 0;
			(unsigned long)idx < ARRAY_SIZE(ref_ratio_arr); idx++) {

			/* avoid CERT-C Integers Rule INT30-C */
			if ((pxclk < UINT_MAX) && (ref_ratio_arr[idx][1] < UINT_MAX)) {
				if (((pxclk * 7U * ref_ratio_arr[idx][1]))
				    > LVDS_PHY_VCO_RANGE_MIN) {
					goto end_func;
				}
			}
		}

		if ((unsigned long)idx < ARRAY_SIZE(ref_ratio_arr)) {
			(void)pr_err("[ERR][LVDS] %s: can not get upsample ratio (%dMhz)\n",
			       __func__, (pxclk / 1000000U));
		}
	} else {
		/* Prevent KCS warning */
		(void)pr_err("[ERR][LVDS] %s can not get hw address\n", __func__);
	}
end_func:
	return idx;
}

/* LVDS_PHY_GetRefCnt
 * Get Reference count value for Automatic FCON
 * p_port : the primary port number of lvds phy
 * s_port : the secondary port number of lvds phy
 * freq : lvds pixel clock
 * upsample_ratio : upsample_ratio
 * Formula : REF_CNT = ((pixel clk * 7 * (2^upsample_ratio))/20)/24*16*16
 */
unsigned int LVDS_PHY_GetRefCnt(
	unsigned int p_port, unsigned int s_port, unsigned int freq,
	unsigned int upsample_ratio)
{
	const void __iomem *p_reg = LVDS_PHY_GetAddress(p_port);
	const void __iomem *s_reg = LVDS_PHY_GetAddress(s_port);
	unsigned int ret = 0U;

	if (p_reg != NULL) {
		unsigned int pxclk;

		if (s_reg != NULL) {
			/* Prevent KCS warning */
			pxclk = freq / 2U;
		} else {
			/* Prevent KCS warning */
			pxclk = freq;
		}

		if (upsample_ratio > LVDS_PHY_UPSAMPLE_RATIO_MAX) {
			(void)pr_err("[ERR][LVDS] %s: invaild parameter (pxclk:%dMhz / upsample_ratio:%d)\n",
			       __func__, (pxclk / 1000000U), upsample_ratio);
			ret = 0U;
		} else {
			/* avoid CERT-C Integers Rule INT30-C */
			if ((pxclk < UINT_MAX) && (ref_ratio_arr[upsample_ratio][1] < UINT_MAX)) {
				ret = ((((pxclk * 7U * ref_ratio_arr[upsample_ratio][1]) / 20U)
					/ 24U * 16U * 16U)
					/ 1000000U);
			}
		}
	} else {
		/* Prevent KCS warning */
		(void)pr_err("[ERR][LVDS] %s can not get hw address\n", __func__);
		ret = 0U;
	}
	return ret;
}

/* LVDS_PHY_SetFormat
 * Set LVDS phy format information
 * port : the port number of lvds phy
 * balance : balanced mode enable (0-disable, 1-enable)
 * depth : color depth(0-6bit, 1-8bit)
 * format : 0-VESA, 1-JEIDA
 */
void LVDS_PHY_SetFormat(
	unsigned int port, unsigned int balanced, unsigned int depth,
	unsigned int format, unsigned int freq)
{
	void __iomem *reg = LVDS_PHY_GetAddress(port);

	if (reg != NULL) {
		unsigned int value;

		value =
			(__raw_readl(reg + LVDS_FORMAT)
			 & ~(LVDS_BALANCED_EN_MASK
			     | LVDS_COLOR_DEPTH_MASK
			     | LVDS_COLOR_FORMAT_MASK
			     | LVDS_UPSAMPLE_RATIO_MASK));
		value |=
			(((balanced & 0x1U) << LVDS_BALANCED_EN_SHIFT)
			 | ((depth & 0x1U) << LVDS_COLOR_DEPTH_SHIFT)
			 | ((format & 0x1U) << LVDS_COLOR_FORMAT_SHIFT)
			 | ((freq & 0x7U) << LVDS_UPSAMPLE_RATIO_SHIFT));
		__raw_writel(value, reg + LVDS_FORMAT);
	}
}

/* LVDS_PHY_SetUserMode
 * Control lane skew and p/n swap
 * port : the port number of lvds phy
 * lane : lane type
 * skew : lane skew value
 * pnswap : lane p/n swap (0-Normal, 1-Swap p/n)
 */
void LVDS_PHY_SetUserMode(
	unsigned int port, unsigned int lane, unsigned int skew,
	unsigned int pnswap)
{
	void __iomem *reg = LVDS_PHY_GetAddress(port);

	if (reg != NULL) {
		unsigned int value;
		switch (lane) {
		case (unsigned int)LVDS_PHY_CLK_LANE:
			value =
			(__raw_readl(reg + LVDS_USER_MODE_PHY_IF_SET0)
			& ~(LVDS_CLK_PN_SWAP_MASK
			| LVDS_CLK_LANE_SKEW_MASK));
			value |=
			(((skew & 0x7U)
			<< LVDS_CLK_LANE_SKEW_SHIFT)
			| ((pnswap & 0x1U)
			<< LVDS_CLK_PN_SWAP_SHIFT));
			__raw_writel(value, reg + LVDS_USER_MODE_PHY_IF_SET0);
			break;
		case (unsigned int)LVDS_PHY_DATA0_LANE:
			value =
			(__raw_readl(reg + LVDS_USER_MODE_PHY_IF_SET0)
			& ~(LVDS_DATA0_PN_SWAP_MASK
			| LVDS_DATA0_LANE_SKEW_MASK));
			value |=
			(((skew & 0x7U)
			<< LVDS_DATA0_LANE_SKEW_SHIFT)
			| ((pnswap & 0x1U)
			<< LVDS_DATA0_PN_SWAP_SHIFT));
			__raw_writel(value, reg + LVDS_USER_MODE_PHY_IF_SET0);
			break;
		case (unsigned int)LVDS_PHY_DATA1_LANE:
			value =
			(__raw_readl(reg + LVDS_USER_MODE_PHY_IF_SET0)
			& ~(LVDS_DATA1_PN_SWAP_MASK
			| LVDS_DATA1_LANE_SKEW_MASK));
			value |=
			(((skew & 0x7U)
			<< LVDS_DATA1_LANE_SKEW_SHIFT)
			| ((pnswap & 0x1U)
			<< LVDS_DATA1_PN_SWAP_SHIFT));
			__raw_writel(value, reg + LVDS_USER_MODE_PHY_IF_SET0);
			break;
		case (unsigned int)LVDS_PHY_DATA2_LANE:
			value =
			(__raw_readl(reg + LVDS_USER_MODE_PHY_IF_SET0)
			& ~(LVDS_DATA2_PN_SWAP_MASK
			| LVDS_DATA2_LANE_SKEW_MASK));
			value |=
			(((skew & 0x7U)
			<< LVDS_DATA2_LANE_SKEW_SHIFT)
			| ((pnswap & 0x1U)
			<< LVDS_DATA2_PN_SWAP_SHIFT));
			__raw_writel(value, reg + LVDS_USER_MODE_PHY_IF_SET0);
			break;
		case (unsigned int)LVDS_PHY_DATA3_LANE:
			value =
			(__raw_readl(reg + LVDS_USER_MODE_PHY_IF_SET0)
			& ~(LVDS_DATA3_PN_SWAP_MASK
			| LVDS_DATA3_LANE_SKEW_MASK));
			value |=
			(((skew & 0x7U)
			<< LVDS_DATA3_LANE_SKEW_SHIFT)
			| ((pnswap & 0x1U)
			<< LVDS_DATA3_PN_SWAP_SHIFT));
			__raw_writel(value, reg + LVDS_USER_MODE_PHY_IF_SET0);
			break;
		case (unsigned int)LVDS_PHY_LANE_MAX:
		default:
			(void)pr_err("[ERR][LVDS] %s: invaild parameter(lane:%d)\n",
			       __func__, lane);
			break;
		}
	}
}

/* LVDS_PHY_SetLaneSwap
 * Swap the data lanes
 * port : the port number of lvds phy
 * lane : the lane number that will be changed
 * select : the lane number that want to swap
 */
void LVDS_PHY_SetLaneSwap(
	unsigned int port, unsigned int lane, unsigned int select)
{
	void __iomem *reg = LVDS_PHY_GetAddress(port);
	unsigned int value;
	int ret = -1;

	if (reg != NULL) {
		switch (lane) {
		case (unsigned int)LVDS_PHY_CLK_LANE:
		case (unsigned int)LVDS_PHY_DATA0_LANE:
		case (unsigned int)LVDS_PHY_DATA1_LANE:
		case (unsigned int)LVDS_PHY_DATA2_LANE:
		case (unsigned int)LVDS_PHY_DATA3_LANE:
			ret = 0;
			break;
		case (unsigned int)LVDS_PHY_LANE_MAX:
		default:
			ret = -1;
			break;
		}

		if (ret < 0) {
			(void)pr_err("[ERR][LVDS] %s: invaild parameter(lane:%d)\n",
				__func__, lane);
		} else {

			value =
				(__raw_readl(reg + LVDS_USER_MODE_PHY_IF_SET1)
				& ~((u32)0x7U << (LVDS_SET_LANE0_SHIFT
						+ (lane * 0x4U))));
			value |=
				((select & 0x7U)
				<< (LVDS_SET_LANE0_SHIFT
					+ (lane * 0x4U)));
			__raw_writel(value, reg + LVDS_USER_MODE_PHY_IF_SET1);
		}
	}
}

/* LVDS_PHY_SetFifoEnableTiming
 * Select FIFO2 enable timing
 * port : the port number of lvds phy
 * cycle : FIFO enable after n clock cycle (0~3 cycle)
 */
void LVDS_PHY_SetFifoEnableTiming(unsigned int port, unsigned int cycle)
{
	void __iomem *reg = LVDS_PHY_GetAddress(port);

	if (reg != NULL) {
		unsigned int value;

		__raw_writel(0x00000000, reg + LVDS_STARTUP_MODE);

		value =
			(__raw_readl(reg + LVDS_STARTUP_MODE)
			 & ~(LVDS_FIFO2_RD_EN_TIMING_MASK));
		value |=
			((cycle & 0x3U)
			 << LVDS_FIFO2_RD_EN_TIMING_SHIFT);
		__raw_writel(value, reg + LVDS_STARTUP_MODE);
	}
}

/* LVDS_PHY_SetPortOption
 * Selects a port option for dual pixel mode
 * port : the port number of lvds phy
 * port_mode : the mode of this port(0-main port, 1-sub port)
 * sync_swap : swap vsync/hsync position (0-do not swap, 1-swap)
 * use_other_port : 0-normal, 1-use sync from other port
 * lane_en : lane enable (CLK, DATA0~3)
 * sync_transmit_src : sync transmit source (0-normal, 1-other port)
 */
void LVDS_PHY_SetPortOption(
	unsigned int port, unsigned int port_mode, unsigned int sync_swap,
	unsigned int use_other_port, unsigned int lane_en,
	unsigned int sync_transmit_src)
{
	void __iomem *reg = LVDS_PHY_GetAddress(port);

	if (reg != NULL) {
		unsigned int value;

		value =
			(__raw_readl(reg + LVDS_PORT)
			 & ~(LVDS_SET_SECONDARY_PORT_MASK
			     | LVDS_VSYNC_HSYNC_SWAP_MASK
			     | LVDS_USE_SYNC_FROM_OP_MASK
			     | LVDS_LANE_EN_MASK
			     | LVDS_SYNC_TRANSMITTED_MASK));

		value |=
			(((port_mode & 0x1U)
			  << LVDS_SET_SECONDARY_PORT_SHIFT)
			 | ((sync_swap & 0x1U)
			    << LVDS_VSYNC_HSYNC_SWAP_SHIFT)
			 | ((use_other_port & 0x1U)
			    << LVDS_USE_SYNC_FROM_OP_SHIFT)
			 | ((lane_en & 0x1FU) << LVDS_LANE_EN_SHIFT)
			 | ((sync_transmit_src & 0x7U)
			    << LVDS_SYNC_TRANSMITTED_SHIFT));
		__raw_writel(value, reg + LVDS_PORT);
	}
}

/* LVDS_PHY_LaneEnable
 * Set lvds lane enable/disable
 * port : the port number of lvds phy
 */
void LVDS_PHY_LaneEnable(unsigned int port, unsigned int enable)
{
	void __iomem *reg = LVDS_PHY_GetAddress(port);

	if (reg != NULL) {
		unsigned int value;

		value =
			(__raw_readl(reg + LVDS_PORT)
			 & ~(LVDS_LANE_EN_MASK));

		if (enable == 1U) {
			/* Prevent KCS warning */
			value |= (0x1FU << LVDS_LANE_EN_SHIFT);
		}
		__raw_writel(value, reg + LVDS_PORT);
	}
}

/* LVDS_PHY_FifoEnable
 * Set lvds phy fifo enable
 * port : the port number of lvds phy
 */
void LVDS_PHY_FifoEnable(unsigned int port, unsigned int enable)
{
	void __iomem *reg = LVDS_PHY_GetAddress(port);

	if (reg != NULL) {
		unsigned int value;

		value =
			(__raw_readl(reg + LVDS_EN)
			 & ~(LVDS_FIFO2_EN_MASK | LVDS_FIFO1_EN_MASK
			     | LVDS_FIFO0_EN_MASK | LVDS_DATA_EN_MASK));

		if (enable == 1U) {
			value |=
				((0x1U << LVDS_FIFO2_EN_SHIFT)
				 | (0x1U << LVDS_FIFO1_EN_SHIFT)
				 | (0x1U << LVDS_FIFO0_EN_SHIFT)
				 | (0x1U << LVDS_DATA_EN_SHIFT));
		}
		__raw_writel(value, reg + LVDS_EN);
	}
}

/* LVDS_PHY_FifoReset
 * Reset LVDS PHY Fifo
 * port : the port number of lvds phy
 * reset : 0-release, 1-reset
 */
void LVDS_PHY_FifoReset(unsigned int port, unsigned int reset)
{
	void __iomem *reg = LVDS_PHY_GetAddress(port);

	if (reg != NULL) {
		if (reset == 1U) {
			__raw_writel(0x0000118F, reg + LVDS_RESETB);
		} else {
			__raw_writel(0x00001FFF, reg + LVDS_RESETB);
		}
	}
}

/* LVDS_PHY_SWReset
 * Control lvds phy swreset
 * port : the port number of lvds phy
 * reset : 0-release, 1-reset
 */
void LVDS_PHY_SWReset(unsigned int port, unsigned int reset)
{
	void __iomem *reg = LVDS_PHY_GetAddress(port);

	if (reg != NULL) {
		if (reset == 1U) {
			__raw_writel(0x00000000, reg + LVDS_RESETB);
		} else {
			__raw_writel(0x00001FFF, reg + LVDS_RESETB);
		}
	}
}

/* LVDS_PHY_ClockEnable
 * Control lvds phy clock enable
 * port : the port number of lvds phy
 * enable : 0-disable, 1-enable
 */
void LVDS_PHY_ClockEnable(unsigned int port, unsigned int enable)
{
	void __iomem *reg = LVDS_PHY_GetAddress(port);

	if (reg != NULL) {
		if (enable == 1U) {
			__raw_writel(0x000100FF, reg + LVDS_CLK_SET);
		} else {
			__raw_writel(0x00000000, reg + LVDS_CLK_SET);
		}
	}
}

/* LVDS_PHY_SetStrobe
 * Sets LVDS strobe registers
 * port : the port number of lvds phy
 * mode : 0-Manual, 1- Auto
 * enable : 0-disable, 1-enable
 */
void LVDS_PHY_SetStrobe(
	unsigned int port, unsigned int automode, unsigned int enable)
{
	void __iomem *reg = LVDS_PHY_GetAddress(port);

	if (reg != NULL) {
		unsigned int value;

		value =
			(__raw_readl(reg + LVDS_STB_EN)
			 & ~(LVDS_STB_EN_MASK));
		value |= ((enable & 0x1U) << LVDS_STB_EN_SHIFT);
		__raw_writel(value, reg + LVDS_STB_EN);

		value =
			(__raw_readl(reg + LVDS_AUTO_STB_SET)
			 & ~(LVDS_STB_AUTO_EN_MASK));
		value |= ((automode & 0x1U) << LVDS_STB_AUTO_EN_SHIFT);
		__raw_writel(value, reg + LVDS_AUTO_STB_SET);
	}
}

/* LVDS_PHY_SetFcon
 * Setup LVDS PHY Manual/Automatic Coarse tunning
 * port : the port number of lvds phy
 * mode : coarse tunning method 0-manual, 1-automatic
 * loop : feedback loop mode 0-closed loop, 1-open loop
 * fcon : frequency control value
 */
void LVDS_PHY_SetFcon(
	unsigned int port, unsigned int automode, unsigned int loop,
	unsigned int division, unsigned int fcon)
{
	void __iomem *reg = LVDS_PHY_GetAddress(port);
	unsigned int value;

	if (reg != NULL) {
		/* Alphachips Guide for FCON */
		unsigned int target_th = 0x2020U;

		switch (automode) {
		case (unsigned int)LVDS_PHY_FCON_MANUAL:
			value =
				(__raw_readl(reg + LVDS_CTSET1)
				 & ~(LVDS_MPLL_CTLCK_MASK
				     | LVDS_MPLL_DIVN_MASK
				     | LVDS_MPLL_FCON_MASK));
			value |=
				(((loop & 0x1U) << LVDS_MPLL_CTLCK_SHIFT)
				 | ((division & 0x3U)
				    << LVDS_MPLL_DIVN_SHIFT)
				 | ((fcon & 0x3FFU)
				    << LVDS_MPLL_FCON_SHIFT));
			__raw_writel(value, reg + LVDS_CTSET1);
			// offset: 0x094

			value =
				(__raw_readl(reg + LVDS_FCOPT)
				 & ~(LVDS_FCOPT_CLK_DET_SEL_MASK
				     | LVDS_FCOPT_CT_SEL_MASK));
			value |= ((automode & 0x1U) << LVDS_FCOPT_CT_SEL_SHIFT);
			__raw_writel(value, reg + LVDS_FCOPT); // offset: 0x09C
			break;
		case (unsigned int)LVDS_PHY_FCON_AUTOMATIC:
			value =
				(__raw_readl(reg + LVDS_FCCNTR1)
				 & ~(LVDS_CONTIN_TARGET_TH_MASK
				     | LVDS_REF_CNT_MASK));
			value |=
				(((target_th & 0xFFFFU)
				  << LVDS_CONTIN_TARGET_TH_SHIFT)
				 | ((fcon & 0xFFFFU)
					 << LVDS_REF_CNT_SHIFT));
			__raw_writel(
				value, reg + LVDS_FCCNTR1); // offset: 0x0B0

			value =
				(__raw_readl(reg + LVDS_CTSET1)
				 & ~(LVDS_MPLL_CTLCK_MASK
				     | LVDS_MPLL_DIVN_MASK
				     | LVDS_MPLL_FCON_MASK));
			value |=
				(((loop & 0x1U) << LVDS_MPLL_CTLCK_SHIFT)
				 | /* Shoulb be set to 'Open' loop */
				 ((division & 0x3U)
				  << LVDS_MPLL_DIVN_SHIFT));
			__raw_writel(value, reg + LVDS_CTSET1); // offset: 0x094

			value =
				(__raw_readl(reg + LVDS_CTSET0)
				 & ~(LVDS_ENABLE_MASK
				     | LVDS_RUN_MASK));
			value |= ((u32)0x1U << LVDS_ENABLE_SHIFT);
			__raw_writel(value, reg + LVDS_CTSET0); // offset: 0x090

			value =
				(__raw_readl(reg + LVDS_FCOPT)
				 & ~(LVDS_FCOPT_CLK_DET_SEL_MASK
				     | LVDS_FCOPT_CT_SEL_MASK));
			value |=
				(((automode & 0x1U) << LVDS_FCOPT_CT_SEL_SHIFT)
				 | ((u32)0x1U << LVDS_FCOPT_CLK_DET_SEL_SHIFT));
			__raw_writel(value, reg + LVDS_FCOPT); // offset: 0x09C

			__raw_writel(0x10101010, reg + LVDS_FCCNTR0);
			/* Alphachips Guide for FCON
			 */
			// offset: 0x098
			break;
		case (unsigned int)LVDS_PHY_FCON_MAX:
		default:
			(void)pr_err("[ERR][LVDS] %s: invaild parameter(mode: %d)\n",
			       __func__, automode);
			break;
		}
	}
}

/* LVDS_PHY_SetCFcon
 * Check fcon status and setup cfcon value
 * port : the port number of lvds phy
 * mode : fcon control mode 0-Manual, 1-Automatic
 * enable : 0-cfcon disable 1-cfcon enable
 */
/* HIS_GOTO */

void LVDS_PHY_SetCFcon(
	unsigned int port, unsigned int automode, unsigned int enable)
{
	void __iomem *reg = LVDS_PHY_GetAddress(port);
	unsigned int value;
	unsigned int time_out = 0;
	unsigned int update_step = 0x1U;

	if (reg != NULL) {
		if (automode == (unsigned int)LVDS_PHY_FCON_AUTOMATIC) {
			while (time_out++ < 100U) {
				unsigned int pd_fcstat, pd_fccntval1,
					pd_fcresval;

				pd_fcstat =
					(__raw_readl(reg + LVDS_FCSTAT)
					 & (LVDS_FCST_CLK_OK_MASK
					    | LVDS_FCST_DONE_MASK
					    | LVDS_FCST_ERROR_MASK));
				pd_fccntval1 =
					__raw_readl(reg + LVDS_FCCNTVAL1);
				pd_fcresval = __raw_readl(reg + LVDS_FCRESEVAL);
				if (pd_fcstat
				    == (LVDS_FCST_CLK_OK_MASK
					| LVDS_FCST_DONE_MASK)) {
					if (enable == 1U) {
					/* Alphachips Guide For CFCON */

						__raw_writel(
				0x0000000CU | (update_step << 4U),
				reg + LVDS_FCCONTINSET0); // offset: 0x0B4
						__raw_writel(
				0x00000960U,
				reg + LVDS_FCCONTINSET1); // offset: 0x0B8
						__raw_writel(
				0x003FF005U,
				reg + LVDS_FCCONTINSET2); // offset: 0x0BC
						__raw_writel(
				0x0000000DU | (update_step << 4U),
				reg + LVDS_FCCONTINSET0); // offset: 0x0B4
					}

					goto closed_loop;
				} else {
					mdelay(1);
				}
			}
		}
		(void)pr_err("[ERR][LVDS] %s time out\n", __func__);
closed_loop:
		/* Change loop mode to 'Closed' loop */
		value =
			(__raw_readl(reg + LVDS_CTSET1)
			 & ~(LVDS_MPLL_CTLCK_MASK));
		value |= ((u32)0x1U << LVDS_MPLL_CTLCK_SHIFT);
		__raw_writel(value, reg + LVDS_CTSET1); // offset: 0x094
	}
}

/* HIS_GOTO */
void LVDS_PHY_CheckPLLStatus(unsigned int p_port, unsigned int s_port)
{
	const void __iomem *p_reg = LVDS_PHY_GetAddress(p_port);
	const void __iomem *s_reg = LVDS_PHY_GetAddress(s_port);
	unsigned int offset = LVDS_MONITOR_DEBUG1;
	unsigned int time_out = 0;

	if ((p_reg != NULL) || (s_reg != NULL)) {
		while (time_out++ < 100U) {
			unsigned int ext_flag = 0;
			unsigned int p_pllstatus =
				(__raw_readl(p_reg + offset)
				 & LVDS_PLL_STATUS_MASK);
			if (s_reg != NULL) {
				unsigned int s_pllstatus =
					(__raw_readl(s_reg + offset)
					 & LVDS_PLL_STATUS_MASK);
				if ((p_pllstatus
				     == LVDS_PLL_STATUS_MASK)
				    && (s_pllstatus
					== LVDS_PLL_STATUS_MASK)) {
					ext_flag = 1U;
				}
			} else {
				if (p_pllstatus
				    == LVDS_PLL_STATUS_MASK) {
				    ext_flag = 1U;
				}
			}
			if (ext_flag == 1U) {
				goto FUNC_EXIT;
			}

			mdelay(1);
		}
	}
	(void)pr_err("[ERR][LVDS] %s time out\n", __func__);
FUNC_EXIT:
	return;
}

/* read the fcon value of the port */
unsigned int LVDS_PHY_Fcon_Value(unsigned int port)
{
	const void __iomem *reg = LVDS_PHY_GetAddress(port);
	unsigned int fcon = 0U;

	if (reg != NULL) {
		/* Prevent KCS warning */
		fcon = ((__raw_readl(reg + LVDS_FCRESEVAL)) >> 3U) & 0x3ffU;
	}
	return fcon;
}

struct fcon_info_t {
	unsigned int num;
	unsigned int mfcon;
	unsigned int sfcon;
	unsigned int mpllstat;
	unsigned int spllstat;
};

int LVDS_PHY_CheckFcon(
	unsigned int p_port, unsigned int s_port, unsigned int mfcon,
	unsigned int sfcon)
{
	const void __iomem *p_reg = LVDS_PHY_GetAddress(p_port);
	const void __iomem *s_reg = LVDS_PHY_GetAddress(s_port);
	unsigned int offset = LVDS_MONITOR_DEBUG1;
	/* the max count to check fcon and status */
	unsigned int fcon_check_maxcnt = 200000U;
	unsigned int p_pllstatus = 0U;
	unsigned int s_pllstatus = 0U;
	/* the count : sequential phydet value : 0xf */
	unsigned int lock_max = 1000U;
	unsigned int lock_cnt = 0U;
	unsigned int loop_cnt = 0U;
	int ret = 0;

	/* avoid MISRA C-2012 Rule 2.7 */
	(void)mfcon;
	(void)sfcon;

	if ((p_reg != NULL) || (s_reg != NULL)) {
		while ((loop_cnt < fcon_check_maxcnt)
		       && (lock_cnt < lock_max)) {
			p_pllstatus =
				(__raw_readl(p_reg + offset)
				 & LVDS_PLL_STATUS_MASK);
			if (s_reg != NULL) {
				s_pllstatus =
					(__raw_readl(s_reg + offset)
					 & LVDS_PLL_STATUS_MASK);
				if ((p_pllstatus
				       == LVDS_PLL_STATUS_MASK)
				       && (s_pllstatus
				       == LVDS_PLL_STATUS_MASK)) {
				       /* Prevent KCS warning */
					lock_cnt += 1U;
				} else {
					/* Prevent KCS warning */
					lock_cnt = 0U;
				}
			} else { // single LVDS
				if (p_pllstatus
				    == LVDS_PLL_STATUS_MASK) {
				    /* Prevent KCS warning */
					lock_cnt += 1U;
				} else {
					/* Prevent KCS warning */
					lock_cnt = 0U;
				}
			}
			loop_cnt += 1U;
		}
	}
	if (lock_cnt < lock_max) {
		ret = -1;
		(void)pr_info("%s :[FAILED] PLL locking loop_cnt = %d, lock_cnt = %d\n",
				__func__, loop_cnt, lock_cnt);
	} else {
		ret = 0;
		(void)pr_info("%s :[OK] PLL locking loop_cnt = %d, lock_cnt = %d\n",
		       __func__, loop_cnt, lock_cnt);
	}

	return ret;
}

/* LVDS_PHY_FConEnable
 * Controls FCON running enable
 * port : the port number of lvds phy
 * enable : 0-disable, 1-enable
 */
void LVDS_PHY_FConEnable(unsigned int port, unsigned int enable)
{
	void __iomem *reg = LVDS_PHY_GetAddress(port);
	unsigned int value;

	if (reg != NULL) {
		value =
			(__raw_readl(reg + LVDS_CTSET0)
			 & ~(LVDS_ENABLE_MASK | LVDS_RUN_MASK));
		value |=
			(((enable & 0x1U) << LVDS_ENABLE_SHIFT)
			 | ((enable & 0x1U) << LVDS_RUN_SHIFT));
		__raw_writel(value, reg + LVDS_CTSET0);
	}
}

/* LVDS_PHY_StrobeWrite
 * Write LVDS PHY Strobe register
 * reg : LVDS PHY Strobe register address
 * offset : LVDS PHY register offset
 * value : the value you want
 */
 /* HIS_GOTO */
void LVDS_PHY_StrobeWrite(
	void __iomem *reg, unsigned int offset, unsigned int value)
{
	unsigned int time_out = 0;

	__raw_writel(value, reg + offset);
	while (time_out++ < 10U) {
		if (__raw_readl(reg + LVDS_AUTO_STB_DONE) == 0x1U) {

			goto FUNC_EXIT;
		}

		mdelay(1);
	}
	(void)pr_err("[ERR][LVDS] %s time out\n", __func__);
FUNC_EXIT:
	return;
}

void LVDS_PHY_VsSet(unsigned int p_port, unsigned int s_port, unsigned int vs)
{
	void __iomem *p_reg = LVDS_PHY_GetAddress(p_port);
	void __iomem *s_reg = LVDS_PHY_GetAddress(s_port);

	LVDS_PHY_StrobeWrite(
		p_reg, 0x3A0U,
		(0x00000094U | ((vs & 0x3U) << 5U))); // STB_PLL ADDR 0x8
	if (s_reg != NULL) {
		LVDS_PHY_StrobeWrite(
			s_reg, 0x3A0U,
			(0x00000094U | ((vs & 0x3U) << 5U))); // STB_PLL ADDR 0x8
	}
}

/* LVDS_PHY_Config
 * Setup LVDS PHY Strobe. (Alphachips Guide Value Only)
 * p_port : the primary port number
 * s_port : the secondary port number
 * upsample_ratio : division ratio (0: fcon automatic, 0~4: fcon manual)
 * step : the step of lvds phy configure
 * vcm : typical vcm level in mV
 * vsw : typical vsw level in mV
 */
void LVDS_PHY_StrobeConfig(
	unsigned int p_port, unsigned int s_port, unsigned int upsample_ratio,
	unsigned int step, unsigned int vcm, unsigned int vsw)
{
	void __iomem *p_reg = LVDS_PHY_GetAddress(p_port);
	void __iomem *s_reg = LVDS_PHY_GetAddress(s_port);
	unsigned int vcmcal, swingcal;
	unsigned int value;
	unsigned int cpzs_main = 0x0000001FU;
	unsigned int cpzs_sub = 0x0000001FU;
	unsigned int strobe_LFR1S = 0x08U;
	unsigned int strobe_LFC1S = 0x18U;
	unsigned int strobe_LFC2S = 0x25U;
	unsigned int vs = 0U;
	unsigned int clk_sync_mode = DUAL_CLK_MODE_MAIN;

	if (p_reg != NULL) {
		switch (step) {
		case (unsigned int)LVDS_PHY_INIT:
			LVDS_PHY_GetCalibrationLevel(
				vcm, vsw, &vcmcal, &swingcal);
			value = ((swingcal & 0xFU) | ((vcmcal & 0x7U) << 4U));

			LVDS_PHY_StrobeWrite(p_reg, 0x380U, 0x000000FFU);
			LVDS_PHY_StrobeWrite(p_reg, 0x384U, 0x0000001FU);
			LVDS_PHY_StrobeWrite(p_reg, 0x388U, 0x00000032U);
			LVDS_PHY_StrobeWrite(p_reg, 0x38CU, cpzs_main);
			LVDS_PHY_StrobeWrite(p_reg, 0x390U, 0x00000000U);
			LVDS_PHY_StrobeWrite(p_reg, 0x394U, strobe_LFR1S);
			LVDS_PHY_StrobeWrite(p_reg, 0x398U, strobe_LFC1S);
			LVDS_PHY_StrobeWrite(p_reg, 0x39CU, strobe_LFC2S);
			LVDS_PHY_StrobeWrite(
				p_reg, 0x3A0, (0x00000094U | ((vs & 0x3U) << 5U)));
			LVDS_PHY_StrobeWrite(p_reg, 0x3A4U, 0x00000000U);
			LVDS_PHY_StrobeWrite(p_reg, 0x3A8U, 0x0000000CU);
			LVDS_PHY_StrobeWrite(p_reg, 0x3ACU, 0x00000000U);
			LVDS_PHY_StrobeWrite(p_reg, 0x3B0U, 0x00000000U);
			if (s_reg != NULL) {
				/* Prevent KCS warning */
				LVDS_PHY_StrobeWrite(p_reg, 0x3B4U, 0x00000007U);
			} else {
				/* Prevent KCS warning */
				LVDS_PHY_StrobeWrite(p_reg, 0x3B4U, 0x00000001U);
			}
			LVDS_PHY_StrobeWrite(p_reg, 0x3B8U, 0x00000001U);

			if (s_reg != NULL) {
				LVDS_PHY_StrobeWrite(s_reg, 0x380U, 0x000000FFU);
				LVDS_PHY_StrobeWrite(s_reg, 0x384U, 0x0000001FU);
				LVDS_PHY_StrobeWrite(s_reg, 0x388U, 0x00000032U);
				LVDS_PHY_StrobeWrite(s_reg, 0x38CU, cpzs_sub);
				LVDS_PHY_StrobeWrite(s_reg, 0x390U, 0x00000000U);
				LVDS_PHY_StrobeWrite(
					s_reg, 0x394U, strobe_LFR1S);
				LVDS_PHY_StrobeWrite(
					s_reg, 0x398U, strobe_LFC1S);
				LVDS_PHY_StrobeWrite(
					s_reg, 0x39CU, strobe_LFC2S);
				LVDS_PHY_StrobeWrite(
					s_reg, 0x3A0U,
					0x00000094U | ((vs & 0x3U) << 5U));
				LVDS_PHY_StrobeWrite(s_reg, 0x3A4U, 0x00000000U);
				LVDS_PHY_StrobeWrite(s_reg, 0x3A8U, 0x0000000CU);
				LVDS_PHY_StrobeWrite(s_reg, 0x3ACU, 0x00000000U);
				LVDS_PHY_StrobeWrite(s_reg, 0x3B0U, 0x00000000U);
				LVDS_PHY_StrobeWrite(s_reg, 0x3B4U, 0x0000000CU);
				LVDS_PHY_StrobeWrite(s_reg, 0x3B8U, 0x00000001U);

				/* Uncomment it if you want to use it. */
				#if 0
				switch (clk_sync_mode)
				{
				case DUAL_CLK_MODE_MAIN:
					LVDS_PHY_StrobeWrite(s_reg, 0x3B8,
								0x00000003U);
					break;

				case DUAL_CLK_MODE_SUB:
					LVDS_PHY_StrobeWrite(p_reg, 0x3B8,
								0x00000003U);
					break;
				default:
					/* do nothing : previous dual clk mode */
					(void)pr_info("%s : unknown clock mode.\n",
						__func__);
					break;
				}
				#else
				LVDS_PHY_StrobeWrite(s_reg, 0x3B8,
								0x00000003U);
				#endif

				(void)pr_info("%s : CS6 : clk_sync_mode =  %d\n",
				       __func__, clk_sync_mode);
			}

			LVDS_PHY_StrobeWrite(p_reg, 0x204U, value);
			LVDS_PHY_StrobeWrite(p_reg, 0x244U, value);
			LVDS_PHY_StrobeWrite(p_reg, 0x284U, value);
			LVDS_PHY_StrobeWrite(p_reg, 0x2C4U, value);
			LVDS_PHY_StrobeWrite(p_reg, 0x304U, value);
			if (s_reg != NULL) {
				LVDS_PHY_StrobeWrite(s_reg, 0x204U, value);
				LVDS_PHY_StrobeWrite(s_reg, 0x244U, value);
				LVDS_PHY_StrobeWrite(s_reg, 0x284U, value);
				LVDS_PHY_StrobeWrite(s_reg, 0x2C4U, value);
				LVDS_PHY_StrobeWrite(s_reg, 0x304U, value);
			}
			break;
		case (unsigned int)LVDS_PHY_READY:
			value = (upsample_ratio & 0x7U);
			LVDS_PHY_StrobeWrite(p_reg, 0x380U, 0x00000000U);
			LVDS_PHY_StrobeWrite(p_reg, 0x380U, 0x000000FFU);
			LVDS_PHY_StrobeWrite(p_reg, 0x384U, 0x0000001FU);
			LVDS_PHY_StrobeWrite(p_reg, 0x3A4U, value);
			LVDS_PHY_StrobeWrite(p_reg, 0x3ACU, value);
			if (s_reg != NULL) {
				LVDS_PHY_StrobeWrite(s_reg, 0x380U, 0x00000000U);
				LVDS_PHY_StrobeWrite(s_reg, 0x380U, 0x000000FFU);
				LVDS_PHY_StrobeWrite(s_reg, 0x384U, 0x0000001FU);
				LVDS_PHY_StrobeWrite(s_reg, 0x3A4U, value);
				LVDS_PHY_StrobeWrite(s_reg, 0x3ACU, value);
			}
			break;
		case (unsigned int)LVDS_PHY_START:
			if (s_reg != NULL) {
				LVDS_PHY_StrobeWrite(s_reg, 0x3B8U, 0x00000001U);
				LVDS_PHY_StrobeWrite(s_reg, 0x3B4U, 0x0000003CU);

				LVDS_PHY_StrobeWrite(p_reg, 0x3B8U, 0x00000001U);
				// PHY Start up  OFF
				LVDS_PHY_StrobeWrite(p_reg, 0x3B4U, 0x000000C7U);
				// PHY Start up ON
				LVDS_PHY_StrobeWrite(p_reg, 0x3B4U, 0x000000D7U);

				/* Uncomment it if you want to use it. */
				#if 0
				switch (clk_sync_mode)
				{
				case DUAL_CLK_MODE_MAIN:
					LVDS_PHY_StrobeWrite(s_reg, 0x3B8, 0x00000003U);
					break;

				case DUAL_CLK_MODE_SUB:
					LVDS_PHY_StrobeWrite(p_reg, 0x3B8, 0x00000003U);
					break;
				default:
					/* do nothing : previous dual clk mode */
					(void)pr_info("%s : unknown clock mode.\n", __func__);
					break;
				}
				#else
				/* main PLL syncrhonization */
				LVDS_PHY_StrobeWrite(s_reg, 0x3B8U, 0x00000003U);
				#endif
			} else {
				unsigned int val =
					(LVDS_PHY_GetRegValue(p_port, 0x3B4U)
					 & ~(0x000000F0U));
				LVDS_PHY_StrobeWrite(p_reg, 0x3B4U, val);
				LVDS_PHY_StrobeWrite(p_reg, 0x3B8U, 0x00000000U);
				LVDS_PHY_StrobeWrite(p_reg, 0x3B8, 0x00000001U);
				val =
					(LVDS_PHY_GetRegValue(p_port, 0x3B4U)
					 & ~(0x000000F0U));
				val |= (0x1U << 4U);
				LVDS_PHY_StrobeWrite(p_reg, 0x3B4U, val);
			}

			LVDS_PHY_StrobeWrite(p_reg, 0x200U, 0x00000079U);
			LVDS_PHY_StrobeWrite(p_reg, 0x240U, 0x00000079U);
			LVDS_PHY_StrobeWrite(p_reg, 0x280U, 0x00000079U);
			LVDS_PHY_StrobeWrite(p_reg, 0x2C0U, 0x00000079U);
			LVDS_PHY_StrobeWrite(p_reg, 0x300U, 0x00000079U);
			if (s_reg != NULL) {
				LVDS_PHY_StrobeWrite(s_reg, 0x200U, 0x00000079U);
				LVDS_PHY_StrobeWrite(s_reg, 0x240U, 0x00000079U);
				LVDS_PHY_StrobeWrite(s_reg, 0x280U, 0x00000079U);
				LVDS_PHY_StrobeWrite(s_reg, 0x2C0U, 0x00000079U);
				LVDS_PHY_StrobeWrite(s_reg, 0x300U, 0x00000079U);
			}
			break;
		case (unsigned int)LVDS_PHY_CONFIG_MAX:
		default:
			(void)pr_info("%s in error, invaild parameter(step: %d)\n",
				__func__, step);
			break;
		}
	}
}

/* LVDS_PHY_CheckStatus
 * Check the status of lvds phy
 * p_port : the primary port number of lvds phy
 * s_port : the secondary port number of lvds phy
 */
unsigned int LVDS_PHY_CheckStatus(unsigned int p_port, unsigned int s_port)
{
	const void __iomem *p_reg = LVDS_PHY_GetAddress(p_port);
	const void __iomem *s_reg = LVDS_PHY_GetAddress(s_port);
	unsigned int ret = 0, value;

	if (p_reg != NULL) {
		value =
			(__raw_readl(p_reg + LVDS_MONITOR_DEBUG1)
			 & (LVDS_PLL_STATUS_MASK));
		if ((value
		    & (LVDS_MONITOR_DEBUG1_LKVDETLOW_MASK
		       | LVDS_MONITOR_DEBUG1_LKVDETHIGH_MASK)) != 0U) {
		    /* Prevent KCS warning */
			ret |= 0x1U; /* primary port status b[0]*/
		}
	}

	if (s_reg != NULL) {
		value =
			(__raw_readl(s_reg + LVDS_MONITOR_DEBUG1)
			 & (LVDS_PLL_STATUS_MASK));
		if ((value
		    & (LVDS_MONITOR_DEBUG1_LKVDETLOW_MASK
		       | LVDS_MONITOR_DEBUG1_LKVDETHIGH_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret |= 0x2U; /* secondary port status b[1] */
		}
	}
	return ret;
}
EXPORT_SYMBOL(LVDS_PHY_CheckStatus);

/* LVDS_PHY_GetRegValue
 * Read the register corresponding to 'offset'
 * port : the port number of lvds phy
 * offset : the register offset
 */
/* HIS_GOTO */
unsigned int LVDS_PHY_GetRegValue(unsigned int port, unsigned int offset)
{
	const void __iomem *reg = LVDS_PHY_GetAddress(port);
	unsigned int ret = 0U;
	unsigned int time_out = 0;

	ret = __raw_readl(reg + offset);
	if (offset > 0x1FFU) { /* Write Only Registers */
		while (time_out++ < 10U) {
			if ((__raw_readl(reg + LVDS_AUTO_STB_DONE) & 0x1U) != 0U) {
				goto end_func;
			}

			mdelay(1);
		}
	}
	(void)pr_err("[ERR][LVDS] %s time out\n", __func__);
end_func:
	ret = __raw_readl(reg + LVDS_AUTO_STB_RDATA);
	return ret;
}

/* HIS_CALLING */
void __iomem *LVDS_PHY_GetAddress(unsigned int port)
{
	void __iomem *ret = NULL;

	if (port >= (unsigned int)LVDS_PHY_PORT_MAX) {
		//(void)pr_warn("[WARN][LVDS] %s: unused port value. return NULL.",
		//	__func__);
		ret = NULL;
	} else {
		if (pLVDS_reg[port] == NULL) {
			/* Prevent KCS warning */
			(void)pr_err("[ERR][LVDS] %s: pLVDS_reg\n", __func__);
		}
		ret = pLVDS_reg[port];
	}
	return ret;
}

/* LVDS_WRAP_SetConfigure
 * Set Tx splitter configuration
 * lr : tx splitter output mode - 0: even/odd, 1: left/right
 * bypass : tx splitter bypass mode
 * width : tx splitter width - single port: real width, dual port: half width
 */
void LVDS_WRAP_SetConfigure(
	unsigned int lr, unsigned int bypass, unsigned int width)
{
	void __iomem *reg = (void __iomem *)pLVDS_wrap_reg;

	if (reg != NULL) {
		unsigned int val =
			(__raw_readl(reg + TS_CFG)
			 & ~(TS_CFG_WIDTH_MASK | TS_CFG_MODE_MASK
			     | TS_CFG_LR_MASK | TS_CFG_BP_MASK));

		val |= (((width & 0xFFFU) << TS_CFG_WIDTH_SHIFT)
			| ((bypass & 0x1U) << TS_CFG_BP_SHIFT)
			| ((lr & 0x1U) << TS_CFG_LR_SHIFT));

		__raw_writel(val, reg + TS_CFG);
	}
}
EXPORT_SYMBOL(LVDS_WRAP_SetConfigure);

void LVDS_WRAP_Set(
	unsigned int lvds_type, unsigned int val, unsigned int select,
	unsigned int sel0[TXOUT_MAX_LINE][TXOUT_DATA_PER_LINE],
	unsigned int sel1[TXOUT_MAX_LINE][TXOUT_DATA_PER_LINE])
{
	LVDS_WRAP_SetAccessCode();
	if (lvds_type != 0U) {
		unsigned int idx;

		LVDS_WRAP_SetConfigure(0U, 0U, val);
		for (idx = 0; idx < (unsigned int)TS_SWAP_CH_MAX; idx++) {
			LVDS_WRAP_SetDataSwap(idx, idx);
		}
		LVDS_WRAP_SetMuxOutput(DISP_MUX_TYPE, 0, select, 1U);
		LVDS_WRAP_SetMuxOutput(
			TS_MUX_TYPE, (int)TS_MUX_IDX0, (unsigned int)TS_MUX_PATH_CORE, 1U);
		LVDS_WRAP_SetDataArray((int)TS_MUX_IDX0, sel0);
		LVDS_WRAP_SetMuxOutput(
			TS_MUX_TYPE, (int)TS_MUX_IDX1, (unsigned int)TS_MUX_PATH_CORE, 1U);
		LVDS_WRAP_SetDataArray((int)TS_MUX_IDX1, sel1);
	} else {
		if (val < (UINT_MAX / 2U)) { /* avoid CERT-C Integers Rule INT31-C */
			LVDS_WRAP_SetMuxOutput(TS_MUX_TYPE, (int)val, select, 1U);
			LVDS_WRAP_SetDataArray((int)val, sel0);
		}
	}
}
EXPORT_SYMBOL(LVDS_WRAP_Set);

void LVDS_WRAP_SetSyncPolarity(unsigned int sync)
{
	void __iomem *reg = (void __iomem *)pLVDS_wrap_reg;
	unsigned int val;

	val = (__raw_readl(reg + SAL_1) & ~(SAL_HS_MASK | SAL_VS_MASK));
	if ((sync & 0x2U) == 0U) {
		/* Prevent KCS warning */
		val |= ((u32)0x1U << SAL_VS_SHIFT);
	}

	if ((sync & 0x4U) == 0U) {
		/* Prevent KCS warning */
		val |= ((u32)0x1U << SAL_HS_SHIFT);
	}

	writel(val, reg + SAL_1);
}
EXPORT_SYMBOL(LVDS_WRAP_SetSyncPolarity);

/* LVDS_WRAP_SetDataSwap
 * Set Tx splitter output data swap
 * ch : Tx splitter output channel(0, 1, 2, 3)
 * set : Tx splitter data swap mode
 */
void LVDS_WRAP_SetDataSwap(unsigned int ch, unsigned int set)
{
	void __iomem *reg = (void __iomem *)pLVDS_wrap_reg;

	if (reg != NULL) {
		unsigned int val;

		switch (ch) {
		case 0:
			val = (__raw_readl(reg + TS_CFG)
			       & ~(TS_CFG_SWAP0_MASK));
			val |= ((set & 0x3U) << TS_CFG_SWAP0_SHIFT);
			__raw_writel(val, reg + TS_CFG);
			break;
		case 1:
			val = (__raw_readl(reg + TS_CFG)
			       & ~(TS_CFG_SWAP1_MASK));
			val |= ((set & 0x3U) << TS_CFG_SWAP1_SHIFT);
			__raw_writel(val, reg + TS_CFG);
			break;
		case 2:
			val = (__raw_readl(reg + TS_CFG)
			       & ~(TS_CFG_SWAP2_MASK));
			val |= ((set & 0x3U) << TS_CFG_SWAP2_SHIFT);
			__raw_writel(val, reg + TS_CFG);
			break;
		case 3:
			val = (__raw_readl(reg + TS_CFG)
			       & ~(TS_CFG_SWAP3_MASK));
			val |= ((set & 0x3U) << TS_CFG_SWAP3_SHIFT);
			__raw_writel(val, reg + TS_CFG);
			break;
		default:
			(void)pr_err("[ERR][LVDS] %s: invalid parameter(%d, %d)\n",
			       __func__, ch, set);
			break;
		}
	}
}
EXPORT_SYMBOL(LVDS_WRAP_SetDataSwap);

/* LVDS_WRAP_SetMuxOutput
 * Set Tx splitter MUX output selection
 * mux: the type of mux (DISP_MUX_TYPE, TS_MUX_TYPE)
 * select : the select
 */
void LVDS_WRAP_SetMuxOutput(
	MUX_TYPE mux, int ch, unsigned int select, unsigned int enable)
{
	void __iomem *reg = (void __iomem *)pLVDS_wrap_reg;
	unsigned int val;

	if (reg != NULL) {
		switch (mux) {
		case DISP_MUX_TYPE:
			val = (__raw_readl(reg + DISP_MUX_SEL)
			       & ~(DISP_MUX_SEL_SEL_MASK));
			val |= ((select & 0x3U) << DISP_MUX_EN_EN_SHIFT);
			__raw_writel(val, reg + DISP_MUX_SEL);
			val = (__raw_readl(reg + DISP_MUX_EN)
			       & ~(DISP_MUX_EN_EN_MASK));
			val |= ((enable & 0x1U) << DISP_MUX_EN_EN_SHIFT);
			__raw_writel(val, reg + DISP_MUX_EN);
			break;
		case TS_MUX_TYPE:
			switch (ch) {
			case 0:
				val = (__raw_readl(reg + TS_MUX_SEL0)
				       & ~(TS_MUX_SEL_SEL_MASK));
				val |= ((select & 0x7U) << TS_MUX_SEL_SEL_SHIFT);
				__raw_writel(val, reg + TS_MUX_SEL0);
				val = (__raw_readl(reg + TS_MUX_EN0)
				       & ~(TS_MUX_EN_EN_MASK));
				val |= ((enable & 0x1U) << TS_MUX_EN_EN_SHIFT);
				__raw_writel(val, reg + TS_MUX_EN0);
				break;
			case 1:
				val = (__raw_readl(reg + TS_MUX_SEL1)
				       & ~(TS_MUX_SEL_SEL_MASK));
				val |= ((select & 0x7U) << TS_MUX_SEL_SEL_SHIFT);
				__raw_writel(val, reg + TS_MUX_SEL1);
				val = (__raw_readl(reg + TS_MUX_EN1)
				       & ~(TS_MUX_EN_EN_MASK));
				val |= ((enable & 0x1U) << TS_MUX_EN_EN_SHIFT);
				__raw_writel(val, reg + TS_MUX_EN1);
				break;
			case 2:
				val = (__raw_readl(reg + TS_MUX_SEL2)
				       & ~(TS_MUX_SEL_SEL_MASK));
				val |= ((select & 0x7U) << TS_MUX_SEL_SEL_SHIFT);
				__raw_writel(val, reg + TS_MUX_SEL2);
				val = (__raw_readl(reg + TS_MUX_EN2)
				       & ~(TS_MUX_EN_EN_MASK));
				val |= ((enable & 0x1U) << TS_MUX_EN_EN_SHIFT);
				__raw_writel(val, reg + TS_MUX_EN2);
				break;
			default:
				(void)pr_err("[ERR][LVDS] %s: invalid parameter(mux: %d, ch: %d)\n",
	      			__func__, mux, ch);
				break;
			}
			break;
		case MUX_TYPE_MAX:
		default:
			(void)pr_err("[ERR][LVDS] %s: invalid parameter(mux: %d, ch: %d)\n",
	       		__func__, mux, ch);
			break;
		}
	}
}
EXPORT_SYMBOL(LVDS_WRAP_SetMuxOutput);

/* LVDS_WRAP_SetDataPath
 * Set Data output format of tx splitter
 * ch : channel number of tx splitter mux
 * path : path number of tx splitter mux
 * set : data output format of tx splitter mux
 */
 /* HIS_GOTO */
void LVDS_WRAP_SetDataPath(int ch, unsigned int data_path, unsigned int set)
{
	void __iomem *reg = (void __iomem *)pLVDS_wrap_reg;
	unsigned int offset;
	int ret = -1;

	if (data_path >= (unsigned int)TS_TXOUT_SEL_MAX) {
		ret = -1;

		goto FUNC_EXIT;
	}

	if (reg != NULL) {
		switch (ch) {
		case 0:
			offset = TXOUT_SEL0_0;
			ret = 0;
			break;
		case 1:
			offset = TXOUT_SEL0_1;
			ret = 0;
			break;
		case 2:
			offset = TXOUT_SEL0_2;
			ret = 0;
			break;
		default:
			ret = -1;
			break;
		}
		if (ret < 0) {
			goto FUNC_EXIT;
		}

		__raw_writel((set & 0xFFFFFFFFU), reg + (offset + (0x4U * data_path)));
	}
FUNC_EXIT:
	if (ret < 0) {
		(void)pr_err("[ERR][LVDS] %s: invalid parameter(ch: %d, path: %d)\n",
			__func__, ch, data_path);
	}
}
EXPORT_SYMBOL(LVDS_WRAP_SetDataPath);

/* LVDS_WRAP_SetDataArray
 * Set the data output format of tx splitter mux
 * ch : channel number of tx splitter mux
 * data : the array included the data output format
 */
void LVDS_WRAP_SetDataArray(
	int ch, unsigned int data[TXOUT_MAX_LINE][TXOUT_DATA_PER_LINE])
{
	const void __iomem *reg = (void __iomem *)pLVDS_wrap_reg;
	const unsigned int *lvdsdata = (unsigned int *)data;
	unsigned int idx, value, data_path;
	unsigned int data0, data1, data2, data3;

	if ((ch < 0) || (ch >= (int)TS_MUX_IDX_MAX)) {
		(void)pr_err("[ERR][LVDS] %s: invalid parameter(ch: %d)\n", __func__, ch);
	} else {
		if (reg != NULL) {
			for (idx = 0; idx < (TXOUT_MAX_LINE * TXOUT_DATA_PER_LINE);
			     idx += 4U) {
				data0 = TXOUT_GET_DATA(idx);
				data1 = TXOUT_GET_DATA(idx + 1U);
				data2 = TXOUT_GET_DATA(idx + 2U);
				data3 = TXOUT_GET_DATA(idx + 3U);

				data_path = idx / 4U;
				value =
					((lvdsdata[data3] << 24U)
					 | (lvdsdata[data2] << 16U)
					 | (lvdsdata[data1] << 8U) | (lvdsdata[data0]));
				LVDS_WRAP_SetDataPath(ch, data_path, value);
			}
		}
	}
}
EXPORT_SYMBOL(LVDS_WRAP_SetDataArray);

/* LVDS_WRAP_SetAccessCode
 * Set the access code of LVDS Wrapper
 */
 /* HIS_GOTO */
void LVDS_WRAP_SetAccessCode(void)
{
	void __iomem *reg =
		(void __iomem *)pLVDS_wrap_reg + 0x1EC;

	if (__raw_readl(reg) == 0x1ACCE551U) {
		goto FUNC_EXIT;
	}

	/* Please delete this code, after making a decision about safety
	 * mechanism
	 */
	__raw_writel(0x1ACCE551, reg);
FUNC_EXIT:
	return;
}
EXPORT_SYMBOL(LVDS_WRAP_SetAccessCode);

/* LVDS_WRAP_REsetPHY
 * software reset for PHY port
 */
void LVDS_WRAP_ResetPHY(unsigned int port, unsigned int reset)
{
	void __iomem *reg =
		(void __iomem *)pLVDS_wrap_reg + TS_SWRESET;

	if (reset == 1U) {
		switch (port) {
		case (unsigned int)TS_MUX_IDX0:
		case (unsigned int)TS_MUX_IDX1:
			__raw_writel(__raw_readl(reg) | 0x1U, reg); // LVDS_PHY_2PORT_SYS
			__raw_writel(__raw_readl(reg) | ((u32)0x1U << 4U),
			       reg); // LVDS_PHY_2PORT_SYS
			break;
		case (unsigned int)TS_MUX_IDX2:
			__raw_writel(__raw_readl(reg) | ((u32)0x1U << 3U),
			       reg); // LVDS_PHY_1PORT_SYS
			break;
		default:
			(void)pr_err("%s : invalid mux port(%d).\n", __func__, port);
			break;
		}
	}

	__raw_writel(0x0, reg);
}
EXPORT_SYMBOL(LVDS_WRAP_ResetPHY);

#if defined(CONFIG_ARCH_TCC805X)
void LVDS_WRAP_SM_Bypass(unsigned int lcdc_mux_id, unsigned int en)
{
        void __iomem *reg =
                (void __iomem *)pLVDS_wrap_reg + SM_BYPASS;
        unsigned int val;

        if (en > 1U) {
                (void)pr_err("%s : invalid en value. en should be 0 or 1.\n",
                       __func__);
        }
        switch (lcdc_mux_id) {
        case 2:
                val = (__raw_readl(reg) & ~(0x1U << SHIFT_SDC)) | (en << SHIFT_SDC);
                __raw_writel(val, reg);
                break;
        case 3:
                val = (__raw_readl(reg) & ~(0x1U << SHIFT_SRC)) | (en << SHIFT_SRC);
                __raw_writel(val, reg);
                break;
        default:
                (void)pr_err("%s : invalid lcdc mux id\n", __func__);
		val = 0xFFFFU;
                break;
        }
	if (val != 0xFFFFU) {
		(void)pr_info("%s : lcdc mux id = %d, SM_BYPASS = 0x%08x\n", __func__,
			      lcdc_mux_id, __raw_readl(reg));
	}
}
EXPORT_SYMBOL(LVDS_WRAP_SM_Bypass);
#endif

/* HIS_PARAM */
lvds_hw_info_t *lvds_register_hw_info(
	lvds_hw_info_t *l_hw, unsigned int l_type, unsigned int port1,
	unsigned int port2, unsigned int p_clk, unsigned int lcdc_select,
	unsigned int lcdc_bypass, unsigned int xres)
{
	lvds_hw_info_t *lvds_ptr;

	if ((l_hw != NULL)
	    && ((l_type == (unsigned int)PANEL_LVDS_DUAL) || (l_type == (unsigned int)PANEL_LVDS_SINGLE))) {
		l_hw->lvds_type = l_type;
		l_hw->port_main = port1;
		l_hw->port_sub = port2;
		l_hw->p_clk = p_clk;
		l_hw->lcdc_mux_id = lcdc_select;
		l_hw->lcdc_mux_bypass = lcdc_bypass;
		l_hw->xres = xres;
		if (l_type == (unsigned int)PANEL_LVDS_SINGLE) {
			if (l_hw->port_main < (UINT_MAX / 2U)) { /* avoid CERT-C Integers Rule INT31-C */
				l_hw->ts_mux_id = ((int)l_hw->port_main + 2) % 4;
			}
			// lvds 2,3,0 map to ts_mux 0,1,2
		} else {		      // dual
			l_hw->ts_mux_id = (int)-1; // not used
		}
		lvds_ptr = l_hw;
	} else {
		lvds_ptr = NULL;
		(void)pr_err("%s : invalid lvds_ptr.\n", __func__);
	}
	/*(void)pr_info("%s :\n lvds_type = %d\n port_main = %d\n port_sub = %d\n
	 *p_clk = %d\n lcdc_mux_id = %d\n xres = %d\n ts_mux_id = %d\n"
	 *		,__func__,l_hw->lvds_type,l_hw->port_main,
	 *  l_hw->port_sub, l_hw->p_clk, l_hw->lcdc_mux_id,
	 *  l_hw->xres,l_hw->ts_mux_id);
	 */
	return lvds_ptr;
}
EXPORT_SYMBOL(lvds_register_hw_info);

void lvds_splitter_init(lvds_hw_info_t *lvds_hw)
{
	lvds_wrap_core_init(
		lvds_hw->lvds_type, lvds_hw->xres, lvds_hw->ts_mux_id,
		lvds_hw->lcdc_mux_id, lvds_hw->txout_main, lvds_hw->txout_sub);
}
EXPORT_SYMBOL(lvds_splitter_init);

void lvds_phy_init(const lvds_hw_info_t *lvds_hw)
{
	unsigned int ref_clk;
	unsigned int upsample_ratio;
	unsigned int ref_cnt;

	if (lvds_hw->ts_mux_id == -1) { // lvds dual mode
		/* Prevent KCS warning */
		ref_clk = lvds_hw->p_clk * 2U;
	} else { // lvds single mode
		/* Prevent KCS warning */
		ref_clk = lvds_hw->p_clk;
	}
	(void)pr_info("[%s] ref_clk for LVDS PHY = %d\n", __func__, ref_clk);

	upsample_ratio = LVDS_PHY_GetUpsampleRatio(
		lvds_hw->port_main, lvds_hw->port_sub, ref_clk);
	ref_cnt = LVDS_PHY_GetRefCnt(
		lvds_hw->port_main, lvds_hw->port_sub, ref_clk,
		upsample_ratio);

	lvds_phy_core_init(
		lvds_hw->lvds_type, lvds_hw->port_main, lvds_hw->port_sub,
		upsample_ratio, ref_cnt, lvds_hw->vcm, lvds_hw->vsw,
		lvds_hw->lane_main, lvds_hw->lane_sub);
}
EXPORT_SYMBOL(lvds_phy_init);

void lvds_wrap_core_init(
	unsigned int lvds_type, unsigned int width, int tx_mux_id,
	unsigned int lcdc_mux_id, unsigned int (*sel0)[TXOUT_DATA_PER_LINE],
	unsigned int (*sel1)[TXOUT_DATA_PER_LINE])
{
	LVDS_WRAP_SetAccessCode();
	if (lvds_type == (unsigned int)PANEL_LVDS_DUAL) {
		unsigned int idx;

		LVDS_WRAP_SetConfigure(0, 0, width);
		for (idx = 0; idx < (unsigned int)TS_SWAP_CH_MAX; idx++) {
			LVDS_WRAP_SetDataSwap(idx, idx);
		}
		LVDS_WRAP_SetMuxOutput(DISP_MUX_TYPE, 0, lcdc_mux_id, 1U);
		LVDS_WRAP_SetMuxOutput(
			TS_MUX_TYPE, (int)TS_MUX_IDX0, (unsigned int)TS_MUX_PATH_CORE, 1U);
		LVDS_WRAP_SetDataArray((int)TS_MUX_IDX0, sel0);
		LVDS_WRAP_SetMuxOutput(
			TS_MUX_TYPE, (int)TS_MUX_IDX1, (unsigned int)TS_MUX_PATH_CORE, 1U);
		LVDS_WRAP_SetDataArray((int)TS_MUX_IDX1, sel1);
	} else if (lvds_type == (unsigned int)PANEL_LVDS_SINGLE) {
		LVDS_WRAP_SetMuxOutput(TS_MUX_TYPE, tx_mux_id, lcdc_mux_id, 1U);
		LVDS_WRAP_SetDataArray(tx_mux_id, sel0);
	} else {
		(void)pr_err("%s : unknown lvds type. lvds wrap not initialized.\n",
		       __func__);
	}
}

/* HIS_GOTO */ /* HIS_CALLS */ /* HIS_PARAM */ /* HIS_CCM */
void lvds_phy_core_init(
	unsigned int lvds_type, unsigned int lvds_main, unsigned int lvds_sub,
	unsigned int upsample_ratio, unsigned int ref_cnt, unsigned int vcm,
	unsigned int vsw, const unsigned int *LVDS_LANE_MAIN,
	const unsigned int *LVDS_LANE_SUB)
{
	unsigned int status;
	unsigned int mfcon = 0U; // main fcon
	unsigned int sfcon = 0U; // sub fcon
	unsigned int pre_mfcon = 0U;
	unsigned int pre_sfcon = 0U;
	unsigned int fcon_threshold = 2U;

	unsigned int s_port_en = 0U;

	if ((lvds_type != (unsigned int)PANEL_LVDS_DUAL)
		&& (lvds_type != (unsigned int)PANEL_LVDS_SINGLE)) {
		(void)pr_err("%s : unknown lvds type. lvds phy not initialized.\n",
		       __func__);

		goto FUNC_EXIT;
	} else if (lvds_type == (unsigned int)PANEL_LVDS_DUAL) {
		/* Prevent KCS warning */
		s_port_en = 1U;
	} else {
		/* avoid MISRA C-2012 Rule 15.7 */
	}
	LVDS_PHY_ClockEnable(lvds_main, 1);
	if (s_port_en == 1U) {
		/* Prevent KCS warning */
		LVDS_PHY_ClockEnable(lvds_sub, 1);
	}

	LVDS_PHY_SWReset(lvds_main, 1);
	if (s_port_en == 1U) {
		/* Prevent KCS warning */
		LVDS_PHY_SWReset(lvds_sub, 1);
	}

	udelay(1000); // Alphachips Guide

	LVDS_PHY_SWReset(lvds_main, 0);
	if (s_port_en == 1U) {
		/* Prevent KCS warning */
		LVDS_PHY_SWReset(lvds_sub, 0);
	}

	/* LVDS PHY Strobe setup */
	LVDS_PHY_SetStrobe(lvds_main, 1, 1);
	if (s_port_en == 1U) {
		/* Prevent KCS warning */
		LVDS_PHY_SetStrobe(lvds_sub, 1, 1);
	}

	LVDS_PHY_StrobeConfig(
		lvds_main, lvds_sub, upsample_ratio, (unsigned int)LVDS_PHY_INIT, vcm, vsw);

	LVDS_PHY_LaneEnable(lvds_main, 0);
	if (s_port_en == 1U) {
		/* Prevent KCS warning */
		LVDS_PHY_LaneEnable(lvds_sub, 0);
	}

	LVDS_PHY_SetPortOption(lvds_main, 0, 0, 0, 0x0, 0x0);
	if (s_port_en == 1U) {
		/* Prevent KCS warning */
		LVDS_PHY_SetPortOption(lvds_sub, 1, 0, 1, 0x0, 0x7);
	}

	LVDS_PHY_LaneSwap(
		s_port_en, lvds_main, lvds_sub, LVDS_LANE_MAIN, LVDS_LANE_SUB);

	LVDS_PHY_StrobeConfig(
		lvds_main, lvds_sub, upsample_ratio, (unsigned int)LVDS_PHY_READY, vcm, vsw);

	LVDS_PHY_SetFcon(
		lvds_main, (unsigned int)LVDS_PHY_FCON_AUTOMATIC, 0U, 0U,
		ref_cnt); // fcon value, for 44.1Mhz
	if (s_port_en == 1U) {
		/* Prevent KCS warning */
		LVDS_PHY_SetFcon(
			lvds_sub, (unsigned int)LVDS_PHY_FCON_AUTOMATIC, 0U, 0U,
			ref_cnt); // fcon value, for 44.1Mhz
	}

	LVDS_PHY_FConEnable(lvds_main, 1U);
	if (s_port_en == 1U) {
		/* Prevent KCS warning */
		LVDS_PHY_FConEnable(lvds_sub, 1U);
	}

	// dummy startup clk2a enable, Is it needed for single LVDS?
	if (s_port_en == 1U) {
		/* Prevent KCS warning */
		LVDS_PHY_StrobeConfig(
			lvds_main, lvds_sub, upsample_ratio, (unsigned int)LVDS_PHY_START,
			vcm, vsw);
	}

	LVDS_PHY_SetCFcon(lvds_main, (unsigned int)LVDS_PHY_FCON_AUTOMATIC, 1);
	if (s_port_en == 1U) {
		/* Prevent KCS warning */
		LVDS_PHY_SetCFcon(lvds_sub, (unsigned int)LVDS_PHY_FCON_AUTOMATIC, 1);
	}

	mfcon = LVDS_PHY_Fcon_Value(lvds_main);
	if (s_port_en == 1U) {
		/* Prevent KCS warning */
		sfcon = LVDS_PHY_Fcon_Value(lvds_sub);
	}

	(void)LVDS_PHY_CheckFcon(lvds_main, lvds_sub, mfcon, sfcon);

	LVDS_PHY_StrobeConfig(
		lvds_main, lvds_sub, upsample_ratio, (unsigned int)LVDS_PHY_START, vcm, vsw);

	if (s_port_en == 1U) {
		mfcon = LVDS_PHY_Fcon_Value(lvds_main);
		sfcon = LVDS_PHY_Fcon_Value(lvds_sub);
		(void)LVDS_PHY_CheckFcon(lvds_main, lvds_sub, mfcon, sfcon);
		// save mfcon & sfcon after PLL is considered as locked.
		pre_mfcon = LVDS_PHY_Fcon_Value(lvds_main);
		pre_sfcon = LVDS_PHY_Fcon_Value(lvds_sub);
	}

	/* LVDS PHY digital setup */
	LVDS_PHY_SetFormat(lvds_main, 0U, 1U, 0U, upsample_ratio);
	if (s_port_en == 1U) {
		/* Prevent KCS warning */
		LVDS_PHY_SetFormat(lvds_sub, 0U, 1U, 0U, upsample_ratio);
	}

	LVDS_PHY_SetFifoEnableTiming(lvds_main, 0x3U);
	if (s_port_en == 1U) {
		/* Prevent KCS warning */
		LVDS_PHY_SetFifoEnableTiming(lvds_sub, 0x3U);
	}

	/* LVDS PHY Main/Sub Lane Disable */
	LVDS_PHY_LaneEnable(lvds_main, 0U);
	if (s_port_en == 1U) {
		/* Prevent KCS warning */
		LVDS_PHY_LaneEnable(lvds_sub, 0U);
	}

	/* LVDS PHY Main port FIFO Disable */
	LVDS_PHY_FifoEnable(lvds_main, 0U);
	if (s_port_en == 1U) {
		/* Prevent KCS warning */
		LVDS_PHY_FifoEnable(lvds_sub, 0U);
	}

	LVDS_PHY_FifoReset(lvds_main, 1U);
	if (s_port_en == 1U) {
		/* Prevent KCS warning */
		LVDS_PHY_FifoReset(lvds_sub, 1U);
	}

	udelay(1000); // Alphachips Guide

	LVDS_PHY_FifoReset(lvds_main, 0U);
	if (s_port_en == 1U) {
		/* Prevent KCS warning */
		LVDS_PHY_FifoReset(lvds_sub, 0U);
	}

	/* LVDS PHY Main/Sub port FIFO Enable */
	LVDS_PHY_FifoEnable(lvds_main, 1U);
	if (s_port_en == 1U) {
		/* Prevent KCS warning */
		LVDS_PHY_FifoEnable(lvds_sub, 1U);
	}

	/* LVDS PHY Main/Sub port Lane Enable */
	LVDS_PHY_LaneEnable(lvds_main, 1U);
	if (s_port_en == 1U) {
		/* Prevent KCS warning */
		LVDS_PHY_LaneEnable(lvds_sub, 1U);
	}

	if (s_port_en == 1U) {
		/* LVDS PHY Main/Sub port Lane Enable(to apply new power on
		 * sequence)
		 */
		mfcon = LVDS_PHY_Fcon_Value(lvds_main);
		sfcon = LVDS_PHY_Fcon_Value(lvds_sub);
		if ((ABS_DIFF(pre_mfcon, mfcon) > fcon_threshold)
		    || (ABS_DIFF(pre_sfcon, sfcon) > fcon_threshold)) {
			/* LVDS PHY Main/Sub port FIFO Disable & Reset*/
			LVDS_PHY_FifoEnable(lvds_main, 0U);
			LVDS_PHY_FifoEnable(lvds_sub, 0U);

			LVDS_PHY_FifoReset(lvds_main, 1U);
			LVDS_PHY_FifoReset(lvds_sub, 1U);

			LVDS_PHY_StrobeConfig(
				lvds_main, lvds_sub, upsample_ratio,
				(unsigned int)LVDS_PHY_START, vcm, vsw);

			udelay(1000); // Alphachips Guide
			LVDS_PHY_FifoReset(lvds_main, 0U);
			LVDS_PHY_FifoReset(lvds_sub, 0U);

			/* LVDS PHY Main/Sub port FIFO Enable */
			LVDS_PHY_FifoEnable(lvds_main, 1U);
			LVDS_PHY_FifoEnable(lvds_sub, 1U);
			(void)pr_info("%s : [LVDS RESET] LVDS PHY mfcon : %d, pre_mfcon = %d, sfcon = %d, pre_sfcon = %d, fcon_threshold = %d\n"
			       , __func__, mfcon, pre_mfcon, sfcon, pre_sfcon,
			       fcon_threshold);
		} else {
			(void)pr_info("%s : [LVDS OK] LVDS PHY mfcon : %d, pre_mfcon = %d, sfcon = %d, pre_sfcon = %d, fcon_threshold = %d\n"
			       , __func__, mfcon, pre_mfcon, sfcon, pre_sfcon,
			       fcon_threshold);
		}
	}
	// Restore VS to 2
	LVDS_PHY_VsSet(lvds_main, lvds_sub, 2U);

	status = LVDS_PHY_CheckStatus(lvds_main, lvds_sub);
	if ((status & 0x1U) == 0U) {
		(void)pr_err("%s: LVDS_PHY Primary port(%d) is in death [error]\n",
		       __func__, lvds_main);
	} else {
		(void)pr_info("%s: LVDS_PHY Primary port(%d) is alive\n", __func__,
			lvds_main);
	}
	if (s_port_en == 1U) {
		if ((status & 0x2U) == 0U) {
			(void)pr_err("%s: LVDS_PHY Secondary port(%d) is in death [error]\n",
			       __func__, lvds_sub);
		} else {
			(void)pr_info("%s: LVDS_PHY Secondary port(%d) is alive\n",
				__func__, lvds_sub);
		}
	}
FUNC_EXIT:
	return;
}

#if 0	/* Check build in GKI (lvds debug_fs occured build error in GKI build) */
#ifdef CONFIG_DEBUG_FS
/* debugfs for LVDS PHY */
#define LVDS_DSOP_TYPE_VCM 0
#define LVDS_DSOP_TYPE_VSW 1
#define LVDS_DSOP_MODE_READ 0
#define LVDS_DSOP_MODE_WRITE 1
#define LVDS_PHY_DIR_MAX (3U)
#define LVDS_STB_CS0_VCM_MAX (7U)
#define LVDS_STB_CS0_VCM_SHIFT (4U)
#define LVDS_STB_CS0_VCM_MASK (0x7U << LVDS_STB_CS0_VCM_SHIFT)
#define LVDS_STB_CS0_VSW_MAX (15U)
#define LVDS_STB_CS0_VSW_SHIFT (0U)
#define LVDS_STB_CS0_VSW_MASK (0xFU << LVDS_STB_CS0_VSW_SHIFT)

static struct dentry *lvds_root_den;
static struct dentry *lvds_port_den[LVDS_PHY_DIR_MAX];
static unsigned int lvds_phy_control_lanes[LVDS_PHY_LANE_MAX];

static int vioc_lvds_get_port_from_dentry(const struct dentry *in_den)
{
	int port_idx;
	int port_lvds_phy;
	unsigned int idx_dir = 0;

	if (in_den == NULL) {
		//invalid ptr
		return -1;
	}

	while (idx_dir < LVDS_PHY_DIR_MAX) {
		if (in_den == lvds_port_den[idx_dir]) {
			port_idx = idx_dir;
			break;
		}
		++idx_dir;
	}

	if (idx_dir == LVDS_PHY_DIR_MAX) {
		port_idx = -1;
	} else {
		port_lvds_phy = (port_idx + 2) % 4;
	}

	return (port_idx != -1) ? port_lvds_phy : port_idx;
}

static int vioc_lvds_drive_strength_op(int port, unsigned int type, unsigned int mode, unsigned int in_val)
{
	unsigned int idx_lane;
	int ret = 0;
	char* name_type[2] = {"vcm", "vsw"};
	unsigned int val_lane[LVDS_PHY_LANE_MAX];
	unsigned int offset_val[LVDS_PHY_LANE_MAX] = {0x204U, 0x244U, 0x284U, 0x2C4U, 0x304U};
	unsigned int offset_lane[LVDS_PHY_LANE_MAX];
	void __iomem *port_reg = LVDS_PHY_GetAddress(port);
	unsigned int val_lane_order;
	unsigned int status;

	if (port < 0) {
		pr_err("%s: invalid port num\n", __func__);
		return -1;
	}

	if (mode == LVDS_DSOP_MODE_WRITE) { //write
		if (type == LVDS_DSOP_TYPE_VCM) {
			if (in_val > LVDS_STB_CS0_VCM_MAX) { //vcm
				pr_err("%s: invalid vcm level = %u, valid range[0-7]\n",
				       __func__, in_val);
				return -1;
			}
		} else if (type == LVDS_DSOP_TYPE_VSW) {
			if (in_val > LVDS_STB_CS0_VSW_MAX) { //vsw
				pr_err("%s: invalid vsw level = %u, valid range[0-15]\n",
				       __func__, in_val);
				return -1;
			}
		} else {
			pr_err("invalid type\n");
			return -1;
		}
	}

	status = LVDS_PHY_CheckStatus(port, port);

	if ((status & 0x1U) == 0U) {
		(void)pr_err(
			"%s: LVDS_PHY port(%d) is in death vcm/vsw cannot be controlled\n",
			__func__, port);
		return 0;
	}
	val_lane_order = __raw_readl(port_reg + LVDS_USER_MODE_PHY_IF_SET1);
	for (idx_lane = 0; idx_lane < LVDS_PHY_LANE_MAX; idx_lane++) {
		offset_lane[(val_lane_order >> (idx_lane * 4)) & 0xF] =
			offset_val[idx_lane];
	}

	for (idx_lane = 0; idx_lane < LVDS_PHY_LANE_MAX; idx_lane++) {
		val_lane[idx_lane] = LVDS_PHY_GetRegValue(
			port, offset_lane[idx_lane]);
		if (type == LVDS_DSOP_TYPE_VCM) {
			if ((mode == LVDS_DSOP_MODE_WRITE) &&
			    (lvds_phy_control_lanes[idx_lane] == 1)) {
				val_lane[idx_lane] &= ~(LVDS_STB_CS0_VCM_MASK);
				val_lane[idx_lane] |= in_val
						      << LVDS_STB_CS0_VCM_SHIFT;
				LVDS_PHY_StrobeWrite(port_reg,
						     offset_lane[idx_lane],
						     val_lane[idx_lane]);
			} else if (mode == LVDS_DSOP_MODE_READ) {
				val_lane[idx_lane] = (val_lane[idx_lane] &
						      LVDS_STB_CS0_VCM_MASK) >>
						     LVDS_STB_CS0_VCM_SHIFT;
				if (lvds_phy_control_lanes[idx_lane] == 1) {
					ret = val_lane[idx_lane];
				}
			}
		} else if (type == LVDS_DSOP_TYPE_VSW) {
			if ((mode == LVDS_DSOP_MODE_WRITE) &&
			    (lvds_phy_control_lanes[idx_lane] == 1)) {
				val_lane[idx_lane] &= ~(LVDS_STB_CS0_VSW_MASK);
				val_lane[idx_lane] |= in_val
						      << LVDS_STB_CS0_VSW_SHIFT;
				LVDS_PHY_StrobeWrite(port_reg,
						     offset_lane[idx_lane],
						     val_lane[idx_lane]);
			} else if (mode == LVDS_DSOP_MODE_READ) {
				val_lane[idx_lane] = (val_lane[idx_lane] &
						      LVDS_STB_CS0_VSW_MASK) >>
						     LVDS_STB_CS0_VSW_SHIFT;
				if (lvds_phy_control_lanes[idx_lane] == 1) {
					ret = val_lane[idx_lane];
				}
			}
		}
	}

	if (mode == LVDS_DSOP_MODE_READ) {
		pr_info("%s: %s_level Port(%d) = CLK[%d] DATA0[%d] DATA1[%d] DATA2[%d] DATA3[%d]\n",
			__func__, name_type[type], port, val_lane[0],
			val_lane[1], val_lane[2], val_lane[3], val_lane[4]);
	}

	return ret;
}

static int vcm_write_value(void *data, u64 value)
{
	struct dentry *cur = (struct dentry *)data;
	int port_lvds_phy = vioc_lvds_get_port_from_dentry(cur);

	return vioc_lvds_drive_strength_op(port_lvds_phy, LVDS_DSOP_TYPE_VCM,
					   LVDS_DSOP_MODE_WRITE, (u32)value);
}

static int vcm_read_value(void *data, u64 *value)
{
	struct dentry *cur = (struct dentry *)data;
	int port_lvds_phy = vioc_lvds_get_port_from_dentry(cur);

	*value = vioc_lvds_drive_strength_op(port_lvds_phy, LVDS_DSOP_TYPE_VCM,
					     LVDS_DSOP_MODE_READ, 0);
	return 0;
}

static int vsw_write_value(void *data, u64 value)
{
	struct dentry *cur = (struct dentry *)data;
	int port_lvds_phy = vioc_lvds_get_port_from_dentry(cur);

	return vioc_lvds_drive_strength_op(port_lvds_phy, LVDS_DSOP_TYPE_VSW,
					   LVDS_DSOP_MODE_WRITE, (u32)value);
}

static int vsw_read_value(void *data, u64 *value)
{
	struct dentry *cur = (struct dentry *)data;
	int port_lvds_phy = vioc_lvds_get_port_from_dentry(cur);

	*value = vioc_lvds_drive_strength_op(port_lvds_phy, LVDS_DSOP_TYPE_VSW,
					     LVDS_DSOP_MODE_READ, 0);
	return 0;
}

static int vioc_lvds_lane_output_op(int port, unsigned int mode, unsigned int in_val)
{
	unsigned int idx_lane;
	unsigned int val_reg;
	int ret = 0;
	unsigned int val_lane[LVDS_PHY_LANE_MAX];
	unsigned int status;
	void __iomem *port_reg = LVDS_PHY_GetAddress(port);

	if (port < 0) {
		pr_err("%s: invalid port num.", __func__);
		return -1;
	}

	if (mode == LVDS_DSOP_MODE_WRITE) {
		if ((in_val != 0) && (in_val != 1)) {
			pr_err("%s: invalid enable in_val = %d, valid range[0-1] 0 : disable, 1 : enable\n",
			       __func__, in_val);
			return -1;
		}
	}

	status = LVDS_PHY_CheckStatus(port, port);

	if ((status & 0x1U) == 0U) {
		(void)pr_err(
			"%s: LVDS_PHY port(%d) is in death vcm/vsw cannot be controlled\n",
			__func__, port);
		return 0;
	}

	val_reg = __raw_readl(port_reg + LVDS_PORT);
	if( mode == LVDS_DSOP_MODE_READ) {
		val_reg &= LVDS_LANE_EN_MASK;
	}

	for (idx_lane = 0; idx_lane < LVDS_PHY_LANE_MAX; idx_lane++) {
		if ((mode == LVDS_DSOP_MODE_WRITE) &&
		    (lvds_phy_control_lanes[idx_lane] == 1)) {
			val_reg &=
				~(1 << (idx_lane + LVDS_LANE_EN_SHIFT)); //clear
			val_reg |= in_val << (idx_lane + LVDS_LANE_EN_SHIFT);
		} else if (mode == LVDS_DSOP_MODE_READ) {
			if (val_reg & (1 << (idx_lane + LVDS_LANE_EN_SHIFT))) {
				val_lane[idx_lane] = 1;
			} else {
				val_lane[idx_lane] = 0;
			}
			if (lvds_phy_control_lanes[idx_lane] == 1) {
				ret = val_lane[idx_lane];
			}
		}
	}

	if (mode == LVDS_DSOP_MODE_WRITE) {
		__raw_writel(val_reg, port_reg + LVDS_PORT);
	} else if (mode == LVDS_DSOP_MODE_READ) {
		pr_info("%s: output_en for Port(%d) = CLK[%d] DATA0[%d] DATA1[%d] DATA2[%d] DATA3[%d]\n",
			__func__, port, val_lane[0], val_lane[1],
			val_lane[2], val_lane[3], val_lane[4]);
	}

	return ret;
}

static int lane_output_enable(void *data, u64 value)
{
	struct dentry *cur = (struct dentry *)data;
	int port_lvds_phy = vioc_lvds_get_port_from_dentry(cur);

	return vioc_lvds_lane_output_op(port_lvds_phy, LVDS_DSOP_MODE_WRITE, (u32)value);
}

static int lane_output_read_value(void *data, u64 *value)
{
	struct dentry *cur = (struct dentry *)data;
	int port_lvds_phy = vioc_lvds_get_port_from_dentry(cur);

	*value = vioc_lvds_lane_output_op(port_lvds_phy, LVDS_DSOP_MODE_READ, 0);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(vcm_fops, vcm_read_value, vcm_write_value, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(vsw_fops, vsw_read_value, vsw_write_value, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(lane_out_fops, lane_output_read_value, lane_output_enable, "%llu\n");

static void vioc_lvds_init_debug_fs(void)
{
	struct dentry *temp_den;
	unsigned int idx_dir, idx_lane;
	static char *name_dir[LVDS_PHY_DIR_MAX] = { "lvds0", "lvds1", "lvds2" };
	static char *name_lane[LVDS_PHY_LANE_MAX] = { "CLK", "DATA0", "DATA1", "DATA2",
						      "DATA3" };

	lvds_root_den = debugfs_create_dir("vioc_lvds", 0);
	if (!lvds_root_den) {
		pr_err("%s : Failed to create vioc_lvds\n", __func__);
		return;
	}

	for (idx_lane = 0; idx_lane < LVDS_PHY_LANE_MAX; idx_lane++) {
		temp_den =
			debugfs_create_u32(name_lane[idx_lane], S_IWUSR,
					   lvds_root_den,
					   &lvds_phy_control_lanes[idx_lane]);
		if (!temp_den) {
			pr_err("%s : Failed to create debug file[%s]\n",
			       __func__, name_lane[idx_lane]);
			return;
		}
	}
	/* declare a read only file*/
	debugfs_create_u32_array("control_lane", S_IWUSR,
				 lvds_root_den, lvds_phy_control_lanes,
				 LVDS_PHY_LANE_MAX);

	/* start to control all lanes */
	for (idx_lane = 0; idx_lane < LVDS_PHY_LANE_MAX; idx_lane++) {
		lvds_phy_control_lanes[idx_lane] = 1;
	}

	for (idx_dir = 0; idx_dir < LVDS_PHY_DIR_MAX; idx_dir++) {
		lvds_port_den[idx_dir] =
			debugfs_create_dir(name_dir[idx_dir], lvds_root_den);
		if (!lvds_port_den[idx_dir]) {
			pr_err("%s : Failed to create [%s]\n", __func__,
			       name_dir[idx_dir]);
			return;
		}

		temp_den =
			debugfs_create_file("vcm_level", (S_IRUGO | S_IWUSR),
					    lvds_port_den[idx_dir],
					    lvds_port_den[idx_dir], &vcm_fops);
		if (!temp_den) {
			pr_err("%s : Failed to create debug file[vcm_level]\n",
			       __func__);
			return;
		}

		temp_den =
			debugfs_create_file("vsw_level", (S_IRUGO | S_IWUSR),
					    lvds_port_den[idx_dir],
					    lvds_port_den[idx_dir], &vsw_fops);
		if (!temp_den) {
			pr_err("%s : Failed to create debug file[vsw_level]\n",
			       __func__);
			return;
		}

		temp_den =
			debugfs_create_file("lane_out_en", (S_IRUGO | S_IWUSR),
					    lvds_port_den[idx_dir],
					    lvds_port_den[idx_dir], &lane_out_fops);
		if (!temp_den) {
			pr_err("%s : Failed to create debug file[lane_out_en]\n",
			       __func__);
			return;
		}

	}
}
#endif	/*CONFIG_DEBUG_FS*/
#endif	/*0*/

int vioc_lvds_init(void)
{
	struct device_node *ViocLVDS_np;
	unsigned int i;

	ViocLVDS_np = of_find_compatible_node(NULL, NULL, "telechips,lvds_phy");
	if (ViocLVDS_np == NULL) {
		(void)pr_info("[INF][LVDS] vioc-lvdsphy: disabled\n");
	} else {
		for (i = 0; i < (unsigned int)LVDS_PHY_PORT_MAX; i++) {
			pLVDS_reg[i] = (void __iomem *)of_iomap(
				ViocLVDS_np, (int)i);

			if (pLVDS_reg[i] != NULL) {
				/* Prevent KCS warning */
				(void)pr_info("[INF][LVDS] vioc-lvdsphy%d\n", i);
			}
		}
		pLVDS_wrap_reg = (void __iomem *)of_iomap(
			ViocLVDS_np, IDX_LVDS_WRAP);
		if (pLVDS_wrap_reg != NULL) {
			/* Prevent KCS warning */
			(void)pr_info("[INF][LVDS] vioc-lvdswrap\n");
		}
	}

	#if 0
	#ifdef CONFIG_DEBUG_FS
	vioc_lvds_init_debug_fs();
	#endif	/*CONFIG_DEBUG_FS*/
	#endif	/*0*/

	return 0;
}
EXPORT_SYMBOL(vioc_lvds_init);
