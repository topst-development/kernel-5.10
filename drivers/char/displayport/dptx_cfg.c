// SPDX-License-Identifier: GPL-2.0-or-later OR MIT
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/delay.h>

#include "dptx_v14.h"
#include "dptx_reg.h"
#include "dptx_dbg.h"

#define MAX_TRY_PHY_PLL_LOCK 100

#define CHECK_REG_OFFSET(x) (((x) < (uint32_t)DP_MAX_OFFSET) ? (bool)true : (bool)false)

/*
 * For D5 (TCC805x)
 *
 *        +-----------------+
 *        |   pre-emphasis  |
 *        |      0  1  2  3 |
 *        + ----------------+
 * vswing | 0 |  8 10 12 16 |
 *        | 1 | 12 15 18    |
 *        | 2 | 16 20       |
 *        | 3 | 24          |
 *        + ----------------+
 */
static const uint32_t default_d3_main_eq[16] = {
	/* vswing 0 */
	0x08, 0x0A, 0x0C, 0x10,
	/* vswing 1 */
	0x0C, 0x0F, 0x12, 0xFF,
	/* vswing 2 */
	0x10, 0x14, 0xFF, 0xFF,
	/* vswing 3 */
	0x18, 0xFF, 0xFF, 0xFF,
};

/*
 * For D5 (TCC807x)
 *
 *        +-----------------+
 *        |   pre-emphasis  |
 *        |      0  1  2  3 |
 *        + ----------------+
 * vswing | 0 |  0  2  4  8 |
 *        | 1 |  0  3  6    |
 *        | 2 |  0  4       |
 *        | 3 |  0          |
 *        + ----------------+
 */
static const uint32_t default_d3_post_eq[16] = {
	/* vswing 0 */
	0x00, 0x02, 0x04, 0x08,
	/* vswing 1 */
	0x00, 0x03, 0x06, 0xFF,
	/* vswing 2 */
	0x00, 0x04, 0xFF, 0xFF,
	/* vswing 3 */
	0x00, 0xFF, 0xFF, 0xFF,
};

struct SNPY_CFG_Reg_Data {
	uint8_t ucLink_Rate;
	uint32_t uiReg_Add;
	uint32_t uiReg_Val;
};

static struct SNPY_CFG_Reg_Data stSNPY_CFG_Reg_Data[] = {
	{LINK_RATE_RBR,  DP_REGISTER_BANK_REG_0, REG_BANK_REG_0_RBR_INIT},
	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_0, REG_BANK_REG_0_HBR_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_0, REG_BANK_REG_0_HBR2_INIT},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_0, REG_BANK_REG_0_HBR3_INIT},

	{LINK_RATE_RBR,  DP_REGISTER_BANK_REG_20, REG_BANK_REG_20_INIT},
	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_21, REG_BANK_REG_21_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_21, REG_BANK_REG_21_INIT},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_21, REG_BANK_REG_21_INIT},

	{LINK_RATE_RBR,  DP_REGISTER_BANK_REG_11, REG_BANK_REG_11_INIT},
	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_20, REG_BANK_REG_20_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_20, REG_BANK_REG_20_INIT},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_1, REG_BANK_REG_1_INIT},

	{LINK_RATE_RBR,  DP_REGISTER_BANK_REG_12, REG_BANK_REG_12_RBR_INIT},
	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_1, REG_BANK_REG_1_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_1, REG_BANK_REG_1_INIT},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_2, REG_BANK_REG_2_INIT},

	{LINK_RATE_RBR,  DP_REGISTER_BANK_REG_13, REG_BANK_REG_13_RBR_INIT},
	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_2, REG_BANK_REG_2_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_2, REG_BANK_REG_2_INIT},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_3, REG_BANK_REG_3_INIT},

	{LINK_RATE_RBR,  DP_REGISTER_BANK_REG_14, REG_BANK_REG_14_RBR_INIT},
	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_3, REG_BANK_REG_3_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_3, REG_BANK_REG_3_INIT},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_4, REG_BANK_REG_4_INIT},

	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_4, REG_BANK_REG_4_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_4, REG_BANK_REG_4_INIT},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_7, REG_BANK_REG_7_INIT},

	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_7, REG_BANK_REG_7_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_7, REG_BANK_REG_7_INIT},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_8, REG_BANK_REG_8_INIT},

	{LINK_RATE_RBR,  DP_REGISTER_BANK_REG_17, REG_BANK_REG_17_INIT},
	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_8, REG_BANK_REG_8_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_8, REG_BANK_REG_8_INIT},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_9, REG_BANK_REG_9_INIT},

	{LINK_RATE_RBR,  DP_REGISTER_BANK_REG_20, REG_BANK_REG_20_INIT},
	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_9, REG_BANK_REG_9_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_9, REG_BANK_REG_9_INIT},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_10, REG_BANK_REG_10_HBR3_1_INIT},

	{LINK_RATE_RBR,  DP_REGISTER_BANK_REG_21, REG_BANK_REG_21_INIT},
	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_10, REG_BANK_REG_10_HBR_1_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_10, REG_BANK_REG_10_HBR_1_INIT},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_11, REG_BANK_REG_11_INIT},

	{LINK_RATE_RBR,  DP_REGISTER_BANK_REG_23, REG_BANK_REG_23_INIT},
	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_11, REG_BANK_REG_11_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_11, REG_BANK_REG_11_INIT},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_13, DP_REGISTER_BANK_REG_RESET},

	{LINK_RATE_RBR,  DP_REGISTER_BANK_REG_22, REG_BANK_REG_22_INIT},
	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_13, REG_BANK_REG_13_HBR_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_13, REG_BANK_REG_13_HBR2_INIT},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_14, DP_REGISTER_BANK_REG_RESET},
	{LINK_RATE_RBR, DP_REGISTER_BANK_REG_17, 0},
	{LINK_RATE_RBR,  DP_REGISTER_BANK_REG_22, DP_REGISTER_BANK_REG_RESET},
	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_14, REG_BANK_REG_14_HBR_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_14, REG_BANK_REG_14_HBR2_INIT},

	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_17, REG_BANK_REG_17_INIT},

	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_17, REG_BANK_REG_17_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_17, REG_BANK_REG_17_INIT},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_20, REG_BANK_REG_20_INIT},


	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_20, REG_BANK_REG_20_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_20, REG_BANK_REG_20_INIT},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_10, REG_BANK_REG_10_HBR3_INIT},


	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_10, REG_BANK_REG_10_HBR_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_10, REG_BANK_REG_10_HBR2_INIT},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_12, REG_BANK_REG_12_HBR3_INIT},

	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_12, REG_BANK_REG_12_HBR_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_12, REG_BANK_REG_10_HBR2_INIT},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_23, REG_BANK_REG_23_INIT},

	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_23, REG_BANK_REG_23_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_23, REG_BANK_REG_23_INIT},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_22, REG_BANK_REG_22_INIT},
	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_22, REG_BANK_REG_22_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_22, REG_BANK_REG_22_INIT},

	{LINK_RATE_HBR, DP_REGISTER_BANK_REG_17, 0},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_17, 0},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_17, 0},

	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_22, DP_REGISTER_BANK_REG_RESET},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_22, DP_REGISTER_BANK_REG_RESET},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_22, DP_REGISTER_BANK_REG_RESET},

	{LINK_RATE_MAX, DP_REGISTER_BANK_REG_MAX, DP_REGISTER_BANK_REG_RESET}
};

static int32_t Dptx_Cfg_Reg_Init(struct Dptx_Params *dev_param, uint8_t ucLinkRate)
{
	bool bOffsetInRange;
	uint8_t ucLink_Rate;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiReg_Addr, uiReg_R_data, uiReg_22_data, uiReg_W_data, uiReg_Offset;
	uint32_t uiElements;
	struct SNPY_CFG_Reg_Data *pstSNPY_CFG_Reg_Data;

	if (dev_param->ePhy_Dev != PHY_DEVICE_SNPS) {
		dptx_debug("Nothing to do not for Synopsys PHY");

		goto return_funcs;
	}

	uiReg_Offset = dev_param->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		goto return_funcs;
	}

	uiReg_R_data = Dptx_Reg_Readl(dev_param, (uiReg_Offset + (uint32_t)DP_REGISTER_BANK_REG_22));

	uiReg_22_data = ((uiReg_R_data & (uint32_t)AXI_SLAVE_BRIDGE_RST_MASK) != 0U) ? (uint32_t)0x00000008U : (uint32_t)0x00000000U;

	pstSNPY_CFG_Reg_Data = stSNPY_CFG_Reg_Data;

	for (uiElements = 0; (pstSNPY_CFG_Reg_Data[uiElements].uiReg_Add != (uint32_t)DP_REGISTER_BANK_REG_MAX); uiElements++) {
		ucLink_Rate = pstSNPY_CFG_Reg_Data[uiElements].ucLink_Rate;
		uiReg_W_data = pstSNPY_CFG_Reg_Data[uiElements].uiReg_Val;
		uiReg_Addr = pstSNPY_CFG_Reg_Data[uiElements].uiReg_Add;

		if (ucLinkRate != ucLink_Rate) {
			/* For KCS */
			continue;
		}

		if ((uiReg_Addr == DP_REGISTER_BANK_REG_22) && (uiReg_W_data == DP_REGISTER_BANK_REG_RESET)) {
			uiReg_W_data = uiReg_22_data;

			/* It should be wait 10us before releasing a software reset of the DP PHY. */
			udelay(10);
		}

		if ((ucLink_Rate == LINK_RATE_HBR3) && (uiReg_Addr == DP_REGISTER_BANK_REG_13)) {
			/* For KCS */
			uiReg_W_data = (dev_param->bSpreadSpectrum_Clock) ? REG_BANK_REG_13_HBR3_SSC : REG_BANK_REG_13_HBR3_INIT;
		}

		if ((ucLink_Rate == LINK_RATE_HBR3) && (uiReg_Addr == DP_REGISTER_BANK_REG_14)) {
			/* For KCS */
			uiReg_W_data = (dev_param->bSpreadSpectrum_Clock) ? REG_BANK_REG_14_HBR3_SSC : REG_BANK_REG_14_HBR3_INIT;
		}

		if (uiReg_Addr == (uint32_t)DP_REGISTER_BANK_REG_20) {
			uiReg_W_data &= ~((uint32_t)3u << 12);
			uiReg_W_data |= ((dev_param->hw_config.aux_hysteresis & 0x3u) << 12);
		}
		dptx_debug("Reg[0x%x] = 0x%x", pstSNPY_CFG_Reg_Data[uiElements].uireg_add, pstSNPY_CFG_Reg_Data[uiElements].uireg_val);

		Dptx_Reg_Writel(dev_param, (uiReg_Offset + uiReg_Addr), uiReg_W_data);
	}

	dptx_cfg_set_manual_phy_signal_quality(dev_param, ucLinkRate, 0u, 0u);

return_funcs:
	return iRetVal;
}

static int dptx_cfg_set_phy_sram_ext_ld_done(struct Dptx_Params *dev_param)
{
	uint32_t regdata, regoffset;
	int ret = DPTX_RETURN_NO_ERROR;

	regoffset = dev_param->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_17;

	if (!CHECK_REG_OFFSET(regoffset)) {
		dptx_err("Invalid reg offset as 0x%x", regoffset);
		ret = -DPTX_RETURN_EINVAL;
	} else {
		regdata = Dptx_Reg_Readl(dev_param, regoffset);
		regdata |= ((uint32_t)1u << 29);
		Dptx_Reg_Writel(dev_param, regoffset, regdata);
	}

	return ret;
}

static bool dptx_cfg_get_phy_sram_init_done(struct Dptx_Params *dev_param)
{
	uint32_t regdata, regoffset;
	bool ret = (bool)true;

	regoffset = dev_param->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_17;

	if (!CHECK_REG_OFFSET(regoffset)) {
		dptx_err("Invalid reg offset as 0x%x", regoffset);
		ret = (bool)false;
	} else {
		regdata = Dptx_Reg_Readl(dev_param, regoffset);
		ret = ((regdata & ((uint32_t)1u << 24)) != 0u) ? (bool)true : (bool)false;
	}

	return ret;
}

/*
 * This workaround solves the problem of lane 1 of PHY getting stuck in Rx VCO
 * calibration even though the D3 PHY is DPYX only.
 * Note: This workaround will be only applied to D3 (PHY_DEVICE_SNPS)
 */
static int32_t dptx_cfg_bypass_rx_vco_calibration(struct Dptx_Params *dev_param)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	unsigned int loop;

	if (dev_param->ePhy_Dev == PHY_DEVICE_SNPS) {
		/*
		 * 1. DP Workaround
		 * Wait up to 400us for SRAM initialization to complete.
		 * Typically it will be completed within 100us.
		 */
		for (loop = 0u; loop < 400u ; loop++) {
			if (dptx_cfg_get_phy_sram_init_done(dev_param)) {
				/* For KCS */
				break;
			}
			udelay(1);
		}

		/*
		 * 2. DP Workaround
		 * RAWLANEN_DIG_AON_FAST_FLAGS. FAST_RX_VCO_CAL
		 */
		dptx_phy_write(dev_param, 0x315c, 0x4000);
		dptx_phy_write(dev_param, 0x325c, 0x4000);

		/* 3. DP Workaround - Set sram ext ld done */
		ret = dptx_cfg_set_phy_sram_ext_ld_done(dev_param);
	}
	return ret;
}

static int32_t Dptx_Cfg_Set_SDM_Bypass(struct Dptx_Params *dev_param)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegAddr, uiRegMask;
	uint32_t uiReg_R_data, uiReg_W_data, uiReg_Offset;

	uiReg_Offset = dev_param->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		goto return_funcs;
	}

	uiRegAddr = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)DP_CFG_VIDEO_MUX : (uint32_t)DP_REGISTER_BANK_REG_24;
	uiRegMask = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_SDM_DIS_MASK : (uint32_t)SDM_DIS_MASK;

	uiReg_R_data = Dptx_Reg_Readl(dev_param, (uiReg_Offset + uiRegAddr));
	uiReg_W_data = (uiReg_R_data | uiRegMask);
	Dptx_Reg_Writel(dev_param, (uiReg_Offset + uiRegAddr), uiReg_W_data);

	dptx_debug("SDM Bypass - use video data2: Reg[0x%x]: 0x%08x -> 0x%08x", (uiReg_Offset + uiRegAddr), uiReg_R_data, uiReg_W_data);

return_funcs:
	return iRetVal;
}

static int32_t Dptx_Cfg_Set_TRVC_Bypass(struct Dptx_Params *dev_param)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegAddr, uiRegMask, uiReg_Offset;
	uint32_t uiReg_R_data, uiReg_W_data;

	uiReg_Offset = dev_param->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(dev_param->uiRegBank_RegAddr_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", dev_param->uiRegBank_RegAddr_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		goto return_funcs;
	}

	uiRegAddr = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)DP_CFG_VIDEO_MUX : (uint32_t)DP_REGISTER_BANK_REG_24;
	uiRegMask = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_SRVC_DIS_MASK : (uint32_t)SRVC_DIS_MASK;

	uiReg_R_data = Dptx_Reg_Readl(dev_param, (dev_param->uiRegBank_RegAddr_Offset + uiRegAddr));
	uiReg_W_data = (uiReg_R_data | uiRegMask);
	Dptx_Reg_Writel(dev_param, (dev_param->uiRegBank_RegAddr_Offset + uiRegAddr), uiReg_W_data);

	dptx_debug("TRVC Bypass - use video data3: Reg[0x%x]: 0x%08x -> 0x%08x", (dev_param->uiRegBank_RegAddr_Offset + uiRegAddr), uiReg_R_data, uiReg_W_data);

return_funcs:
	return iRetVal;
}

static int32_t Dptx_Cfg_Set_MuxSelect(struct Dptx_Params *dev_param, uint8_t ucMux_Index, uint8_t ucDP_Idx)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint8_t ucRegMap_MuxSel_Shift = 0;
	uint32_t uiRegAddr, uiReg_Offset;
	uint32_t uiRegMap_MuxSel_Mask = 0;
	uint32_t uiRegMap_R_MuxSel = 0;
	uint32_t uiRegMap_W_MuxSel = 0;

	uiReg_Offset = dev_param->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		goto return_funcs;
	}

	uiRegAddr = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)DP_CFG_VIDEO_MUX : (uint32_t)DP_REGISTER_BANK_REG_24;

	switch (ucDP_Idx) {
	case (uint8_t)PHY_INPUT_STREAM_0:
		uiRegMap_MuxSel_Mask = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_S0_MUX_SEL_MASK : (uint32_t)SOURCE0_MUX_SEL_MASK;
		ucRegMap_MuxSel_Shift = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint8_t)CFG_S0_MUX_SEL_SHIFT : (uint8_t)SOURCE0_MUX_SEL_SHIFT;
		break;
	case (uint8_t)PHY_INPUT_STREAM_1:
		uiRegMap_MuxSel_Mask = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_S1_MUX_SEL_MASK : (uint32_t)SOURCE1_MUX_SEL_MASK;
		ucRegMap_MuxSel_Shift = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint8_t)CFG_S1_MUX_SEL_SHIFT : (uint8_t)SOURCE1_MUX_SEL_SHIFT;
		break;
	case (uint8_t)PHY_INPUT_STREAM_2:
		uiRegMap_MuxSel_Mask = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_S2_MUX_SEL_MASK : (uint32_t)SOURCE2_MUX_SEL_MASK;
		ucRegMap_MuxSel_Shift = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint8_t)CFG_S2_MUX_SEL_SHIFT : (uint8_t)SOURCE2_MUX_SEL_SHIFT;
		break;
	case (uint8_t)PHY_INPUT_STREAM_3:
	default:
		uiRegMap_MuxSel_Mask = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_S3_MUX_SEL_MASK : (uint32_t)SOURCE3_MUX_SEL_MASK;
		ucRegMap_MuxSel_Shift = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint8_t)CFG_S3_MUX_SEL_SHIFT : (uint8_t)SOURCE3_MUX_SEL_SHIFT;
		break;
	}

	uiRegMap_R_MuxSel = Dptx_Reg_Readl(dev_param, (uint32_t)(uiReg_Offset + uiRegAddr));
	uiRegMap_W_MuxSel = (uiRegMap_R_MuxSel & ~uiRegMap_MuxSel_Mask);
	uiRegMap_W_MuxSel = (uiRegMap_W_MuxSel | (uint32_t)(ucMux_Index << ucRegMap_MuxSel_Shift));
	Dptx_Reg_Writel(dev_param, (uiReg_Offset + uiRegAddr), uiRegMap_W_MuxSel);

	dptx_debug("Mux select[0x%x](0x%x -> 0x%x): Mux %u -> DP %u",
					(uiReg_Offset + uiRegAddr),
					uiRegMap_R_MuxSel,
					uiRegMap_W_MuxSel,
					ucMux_Index,
					ucDP_Idx);

return_funcs:
	return iRetVal;
}

static int32_t Dptx_Cfg_Set_PHY_Standard_LaneCfg(struct Dptx_Params *dev_param)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegAddr, uiReg_Offset;
	uint32_t uiRegMap_R_StdEn, uiRegMap_W_StdEn;

	if (dev_param->ePhy_Dev != PHY_DEVICE_SNPS) {
		dptx_debug("Nothing to do not for Synopsys PHY");

		goto return_funcs;
	}

	uiReg_Offset = dev_param->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		goto return_funcs;
	}

	uiRegAddr = (uint32_t)DP_REGISTER_BANK_REG_24;
	uiRegMap_R_StdEn = Dptx_Reg_Readl(dev_param, (uint32_t)(uiReg_Offset + uiRegAddr));

	uiRegMap_W_StdEn = (uiRegMap_R_StdEn | (uint32_t)STD_EN_MASK);

	Dptx_Reg_Writel(dev_param, (uiReg_Offset + uiRegAddr), uiRegMap_W_StdEn);

	dptx_debug("PHY Lanes sets to standard: 0x%08x -> 0x%08x", uiRegMap_R_StdEn, uiRegMap_W_StdEn);

return_funcs:
	return iRetVal;
}

int32_t Dptx_Cfg_Get_SDM_Bypass(struct Dptx_Params *dev_param, bool *pbSdm_Bypass)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegAddr, uiRegMask;
	uint32_t uiReg_R_data,  uiReg_Offset;

	uiReg_Offset = dev_param->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		goto return_funcs;
	}

	uiRegAddr = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)DP_CFG_VIDEO_MUX : (uint32_t)DP_REGISTER_BANK_REG_24;
	uiRegMask = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_SDM_DIS_MASK : (uint32_t)SDM_DIS_MASK;

	uiReg_R_data = Dptx_Reg_Readl(dev_param, (uiReg_Offset + uiRegAddr));
	*pbSdm_Bypass = ((uiReg_R_data & uiRegMask) != 0u) ? (bool)true : (bool)false;

	dptx_debug("SDM Path -> %s", (*pbSdm_Bypass) ? "bypass" : "use video data");

return_funcs:
	return iRetVal;
}

int32_t Dptx_Cfg_Get_TRVC_Bypass(struct Dptx_Params *dev_param, bool *pbTrvc_Bypass)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegAddr, uiRegMask, uiReg_Offset;
	uint32_t uiReg_R_data;

	uiReg_Offset = dev_param->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(dev_param->uiRegBank_RegAddr_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", dev_param->uiRegBank_RegAddr_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		goto return_funcs;
	}

	uiRegAddr = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)DP_CFG_VIDEO_MUX : (uint32_t)DP_REGISTER_BANK_REG_24;
	uiRegMask = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_SRVC_DIS_MASK : (uint32_t)SRVC_DIS_MASK;

	uiReg_R_data = Dptx_Reg_Readl(dev_param, (dev_param->uiRegBank_RegAddr_Offset + uiRegAddr));
	*pbTrvc_Bypass = ((uiReg_R_data & uiRegMask) != 0u) ? (bool)true : (bool)false;

	dptx_debug("TRVC Path -> %s", (*pbTrvc_Bypass) ? "bypass" : "use video data");

return_funcs:
	return iRetVal;
}

int32_t Dptx_Cfg_Get_MuxSelect(struct Dptx_Params *dev_param, uint8_t ucDP_Idx, uint8_t *pucMux_Index)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint8_t ucRegMap_MuxSel_Shift = 0;
	uint32_t uiRegAddr, uiReg_Offset;
	uint32_t uiRegMap_MuxSel_Mask = 0;
	uint32_t uiRegMap_R_MuxSel = 0;

	uiReg_Offset = dev_param->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		goto return_funcs;
	}

	uiRegAddr = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)DP_CFG_VIDEO_MUX : (uint32_t)DP_REGISTER_BANK_REG_24;

	switch (ucDP_Idx) {
	case (uint8_t)PHY_INPUT_STREAM_0:
		uiRegMap_MuxSel_Mask = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_S0_MUX_SEL_MASK : (uint32_t)SOURCE0_MUX_SEL_MASK;
		ucRegMap_MuxSel_Shift = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint8_t)CFG_S0_MUX_SEL_SHIFT : (uint8_t)SOURCE0_MUX_SEL_SHIFT;
		break;
	case (uint8_t)PHY_INPUT_STREAM_1:
		uiRegMap_MuxSel_Mask = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_S1_MUX_SEL_MASK : (uint32_t)SOURCE1_MUX_SEL_MASK;
		ucRegMap_MuxSel_Shift = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint8_t)CFG_S1_MUX_SEL_SHIFT : (uint8_t)SOURCE1_MUX_SEL_SHIFT;
		break;
	case (uint8_t)PHY_INPUT_STREAM_2:
		uiRegMap_MuxSel_Mask = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_S2_MUX_SEL_MASK : (uint32_t)SOURCE2_MUX_SEL_MASK;
		ucRegMap_MuxSel_Shift = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint8_t)CFG_S2_MUX_SEL_SHIFT : (uint8_t)SOURCE2_MUX_SEL_SHIFT;
		break;
	case (uint8_t)PHY_INPUT_STREAM_3:
	default:
		uiRegMap_MuxSel_Mask = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_S3_MUX_SEL_MASK : (uint32_t)SOURCE3_MUX_SEL_MASK;
		ucRegMap_MuxSel_Shift = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint8_t)CFG_S3_MUX_SEL_SHIFT : (uint8_t)SOURCE3_MUX_SEL_SHIFT;
		break;
	}

	uiRegMap_R_MuxSel = Dptx_Reg_Readl(dev_param, (uint32_t)(uiReg_Offset + uiRegAddr));

	*pucMux_Index = (uint8_t)((uiRegMap_R_MuxSel & uiRegMap_MuxSel_Mask) >> ucRegMap_MuxSel_Shift);

	dptx_debug("Mux select[0x%x](0x%x): Mux %u -> DP %u",
					(uiReg_Offset + uiRegAddr),
					uiRegMap_R_MuxSel,
					*pucMux_Index,
					ucDP_Idx);

return_funcs:
	return iRetVal;
}

int32_t Dptx_Cfg_Init_Params(struct Dptx_Params *dev_param)
{
	uint8_t ucDpIdx;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	if ((dev_param->ePhy_Dev == PHY_DEVICE_SNPS) && (dev_param->uiTCC80xx_Rev == (uint32_t)TCC80XX_REVISION_ES)) {
		dev_param->bPhy_Lane_Std = (bool)false;
		dev_param->bSDM_Bypass = (bool)false;
		dev_param->bTRVC_Bypass = (bool)false;

		for (ucDpIdx = 0; ucDpIdx < (uint8_t)PHY_INPUT_STREAM_MAX; ucDpIdx++) {
			/*For KCS*/
			dev_param->aucMux_Idx[ucDpIdx] = ucDpIdx;
		}

		goto return_funcs;
	}

	(void)Dptx_Cfg_Get_PHY_Standard_LaneCfg(dev_param, &dev_param->bPhy_Lane_Std);
	(void)Dptx_Cfg_Get_SDM_Bypass(dev_param, &dev_param->bSDM_Bypass);
	(void)Dptx_Cfg_Get_TRVC_Bypass(dev_param, &dev_param->bTRVC_Bypass);

	for (ucDpIdx = 0; ucDpIdx < (uint8_t)PHY_INPUT_STREAM_MAX; ucDpIdx++) {
		/*For KCS*/
		(void)Dptx_Cfg_Get_MuxSelect(dev_param, ucDpIdx, &dev_param->aucMux_Idx[ucDpIdx]);
	}

return_funcs:
	return iRetVal;
}

int32_t Dptx_Cfg_Get_PHY_Standard_LaneCfg(struct Dptx_Params *dev_param, bool *pbPhy_LaneCfg)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegAddr, uiReg_Offset;
	uint32_t uiRegMap_R_StdEn;

	if (dev_param->ePhy_Dev != PHY_DEVICE_SNPS) {
		dptx_debug("Nothing to do not for Synopsys PHY");

		*pbPhy_LaneCfg = (bool)true;

		goto return_funcs;
	}

	uiReg_Offset = dev_param->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		goto return_funcs;
	}

	uiRegAddr = (uint32_t)DP_REGISTER_BANK_REG_24;
	uiRegMap_R_StdEn = Dptx_Reg_Readl(dev_param, (uint32_t)(uiReg_Offset + uiRegAddr));

	*pbPhy_LaneCfg = (uiRegMap_R_StdEn & (uint32_t)STD_EN_MASK) ? (bool)true : (bool)false;

	dptx_debug("PHY Lanes configuration to %s", (*pbPhy_LaneCfg) ? "standard" : "swap");

return_funcs:
	return iRetVal;
}

int32_t Dptx_Cfg_Init(struct Dptx_Params *dev_param, uint8_t ucLinkRate)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint8_t dp_idx;

	if (DPTX_RETURN_SUCCESS(ret)) {
		/* For KCS */
		ret = Dptx_Cfg_Reg_Init(dev_param, ucLinkRate);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		/* For KCS */
		ret = dptx_cfg_bypass_rx_vco_calibration(dev_param);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (dev_param->bSDM_Bypass) {
			/* For KCS */
			ret = Dptx_Cfg_Set_SDM_Bypass(dev_param);
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (dev_param->bTRVC_Bypass) {
			/* For KCS */
			ret = Dptx_Cfg_Set_TRVC_Bypass(dev_param);
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (dev_param->bPhy_Lane_Std) {
			/* For KCS */
			ret = Dptx_Cfg_Set_PHY_Standard_LaneCfg(dev_param);
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		for (dp_idx = 0; dp_idx < dev_param->ucNumOfPorts; dp_idx++) {
			ret = Dptx_Cfg_Set_MuxSelect(dev_param,
						     dev_param->aucMux_Idx[dp_idx],
						     dp_idx);
			if (DPTX_RETURN_ERROR(ret)) {
				/* For KCS */
				break;
			}
		}
	}
	return ret;
}


/**
 * @brief Resets both the DisplayPort Link and PHY.
 *
 * This function performs a more comprehensive reset compared to `dptx_cfg_reset_phy`. It resets both
 * the DisplayPort Link and the DisplayPort PHY (Physical Layer) to ensure that they are both
 * properly configured and synchronized. The function first resets the DisplayPort PHY based on the
 * provided link rate and link lanes, and then proceeds to reset and configure the DisplayPort Link
 * to match the PHY settings.
 *
 * @param[in] dev_param Pointer to the Dptx_Params structure that contains the device context and configuration parameters.
 * @param[in] link_rate The link rate to be used for configuring both the DisplayPort PHY and Link.
 * @param[in] link_lanes The number of link lanes to be used for configuring both the DisplayPort PHY and Link.
 *
 * @return int32_t Returns 0 on success, or a negative error code on failure.
 */
int32_t dptx_cfg_reset_ip(struct Dptx_Params *dev_param, uint8_t link_rate, uint8_t link_lanes)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;

	ret = Dptx_Cfg_Init(dev_param, link_rate);
	if (DPTX_RETURN_SUCCESS(ret)) {
		/* For KCS */
		ret = Dptx_Sec_PHY_Init(dev_param, link_rate, link_lanes);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		/* For KCS */
		ret = Dptx_Core_Init(dev_param);
	}
	return ret;
}

/**
 * @brief Resets and configures the DisplayPort PHY.
 *
 * This function handles the overall process of resetting and configuring the DisplayPort PHY
 * (Physical Layer). When this function is called, it resets the DisplayPort PHY and configures it
 * according to the specified link rate and link lanes. After resetting the PHY, the function also
 * reconfigures the DisplayPort Link to ensure it matches the PHY settings.
 *
 * @param[in] dev_param Pointer to the Dptx_Params structure that contains the device context and configuration parameters.
 * @param[in] link_rate The link rate to be used for configuring the DisplayPort PHY.
 * @param[in] link_lanes The number of link lanes to be used for configuring the DisplayPort PHY.
 *
 * @return int32_t Returns 0 on success, or a negative error code on failure.
 */
int32_t dptx_cfg_reset_phy(struct Dptx_Params *dev_param, uint8_t link_rate, uint8_t link_lanes)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;

	/* Resets DisplayPort PHY for D3 */
	ret = Dptx_Cfg_Reg_Init(dev_param, link_rate);
	if (DPTX_RETURN_SUCCESS(ret)) {
		/* For KCS */
		ret = dptx_cfg_bypass_rx_vco_calibration(dev_param);
	}
	/* Resets DisplayPort PHY for D5 */
	if (DPTX_RETURN_SUCCESS(ret)) {
		/* For KCS */
		ret = Dptx_Sec_PHY_Init(dev_param, link_rate, link_lanes);
	}
	/* Resets DisplayPort PHY for D3/D5 */
	if (DPTX_RETURN_SUCCESS(ret)) {
		/* For KCS */
		ret = dptx_core_phy_init(dev_param);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		/* Mandatory: Wait for 20 milliseconds to stabilize the Display PHY. */
		mdelay(20);
	}
	return ret;
}

int32_t Dptx_Cfg_SoftReset(struct Dptx_Params *dev_param, uint32_t uiVal)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiReg_W_data, uiReg_Offset;

	if (dev_param->ePhy_Dev != PHY_DEVICE_SEC) {
		dptx_debug("Nothing to do not for Samsung Phy");

		goto return_funcs;
	}

	uiReg_Offset = dev_param->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		goto return_funcs;
	}

	uiReg_W_data = uiVal;

	dptx_debug("Set Reset : 0x%x", uiReg_W_data);

	Dptx_Reg_Writel(dev_param, (uint32_t)(uiReg_Offset + (uint32_t)DP_CFG_SOFT_RESET), uiReg_W_data);

return_funcs:
	return iRetVal;
}



int32_t Dptx_Cfg_Set_PHY_Cfg(struct Dptx_Params *dev_param, uint32_t uiCfg_Val)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiReg_Offset, uiReg_W_data = 0, uiReg_R_data = 0;

	if (dev_param->ePhy_Dev != PHY_DEVICE_SEC) {
		dptx_debug("Nothing to do not for Samsung Phy");

		goto return_funcs;
	}

	uiReg_Offset = dev_param->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		goto return_funcs;
	}

	uiReg_R_data = Dptx_Reg_Readl(dev_param, (uiReg_Offset + (uint32_t)DP_CFG_PHY_CFG));

	//uiReg_W_data = (uiReg_R_data | uiCfg_Val);
	uiReg_W_data = uiCfg_Val;

	dptx_debug("Set PHY CFG : 0x%x -> 0x%x", uiReg_R_data, uiReg_W_data);

	Dptx_Reg_Writel(dev_param, (uiReg_Offset + (uint32_t)DP_CFG_PHY_CFG), uiReg_W_data);
return_funcs:
	return iRetVal;
}

int32_t Dptx_Cfg_Check_PHY_Pll(struct Dptx_Params *dev_param, bool *pbPll_Ready)
{
	bool bOffsetInRange, bPll_Lock, bPll_Rdy;
	uint8_t ucCount = 0;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiReg_Offset, uiReg_R_data, uiReg_Mask = 0;

	if (dev_param->ePhy_Dev != PHY_DEVICE_SEC) {
		dptx_debug("Nothing to do not for Samsung Phy");

		goto return_funcs;
	}

	*pbPll_Ready = (bool)false;
	uiReg_Offset = dev_param->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		goto return_funcs;
	}

	uiReg_Mask = (uint32_t)PLL_LOCK_DONE;

	do {
		uiReg_R_data = Dptx_Reg_Readl(dev_param, (uiReg_Offset + (uint32_t)DP_CFG_PHY_DEBUG));

		bPll_Lock = ((uiReg_R_data & uiReg_Mask) == uiReg_Mask) ? (bool)true : (bool)false;

		if (bPll_Lock) {
			dptx_debug("Sec Phy Pll locked[0x%x] after %u us", uiReg_R_data, (ucCount * 100));
			break;
		}

		udelay(100);
		ucCount++;
	} while (ucCount < (uint8_t)MAX_TRY_PHY_PLL_LOCK);

	if (!bPll_Lock) {
		dptx_err("Sec Phy Pll unlocked after 10ms");

		iRetVal = DPTX_RETURN_EBUSY;

		goto return_funcs;
	}

	ucCount = 0;
	uiReg_Mask = (uint32_t)(PLL_LOCK_RDY | PLL_LOCK_DONE);

	do {
		uiReg_R_data = Dptx_Reg_Readl(dev_param, (uiReg_Offset + (uint32_t)DP_CFG_PHY_DEBUG));

		bPll_Rdy = ((uiReg_R_data & uiReg_Mask) == uiReg_Mask) ? (bool)true : (bool)false;

		if (bPll_Rdy) {
			dptx_debug("Sec Phy Pll done[0x%x] ater %u us", uiReg_R_data, (ucCount * 100));
			break;
		}
		udelay(100);
		ucCount++;
	} while (ucCount < (uint8_t)MAX_TRY_PHY_PLL_LOCK);

	if (!bPll_Rdy) {
		dptx_err("Sec Phy Pll locking isn't ready");

		iRetVal = DPTX_RETURN_EBUSY;

		goto return_funcs;
	}

	*pbPll_Ready = (bool)true;

return_funcs:
	return iRetVal;
}

int32_t Dptx_Cfg_Check_PHY_Ready(struct Dptx_Params *dev_param, bool *pbPhy_Ready)
{
	bool bOffsetInRange, bPhy_Rdy;
	uint8_t ucCount = 0;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiReg_Offset, uiReg_R_data, uiReg_Mask = 0;

	if (dev_param->ePhy_Dev != PHY_DEVICE_SEC) {
		dptx_debug("Nothing to do not for Samsung Phy");

		goto return_funcs;
	}

	*pbPhy_Ready = (bool)false;

	uiReg_Offset = dev_param->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		goto return_funcs;
	}

	uiReg_Mask = (uint32_t)(PHY_RDY | PLL_LOCK_RDY | PLL_LOCK_DONE);

	do {
		uiReg_R_data = Dptx_Reg_Readl(dev_param, (uiReg_Offset + (uint32_t)DP_CFG_PHY_DEBUG));

		bPhy_Rdy = ((uiReg_R_data & uiReg_Mask) == uiReg_Mask) ? (bool)true : (bool)false;

		if (bPhy_Rdy) {
			dptx_debug("Sec Phy ready[0x%x] after %u us", uiReg_R_data, (ucCount * 100));
			break;
		}
		udelay(100);
		ucCount++;
	} while (ucCount < (uint8_t)MAX_TRY_PHY_PLL_LOCK);

	if (!bPhy_Rdy) {
		dptx_err("Sec Phy not ready(0x%x)", uiReg_R_data);

		iRetVal = DPTX_RETURN_EBUSY;

		goto return_funcs;
	}

	*pbPhy_Ready = (bool)true;

return_funcs:
	return iRetVal;
}

int32_t Dptx_Cfg_Check_Sec_PHY_SB_Ready(struct Dptx_Params *dev_param, bool *pbSb_Ready)
{
	bool bOffsetInRange, bSb_Rdy;
	uint8_t ucCount = 0;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiReg_Offset, uiReg_R_data, uiReg_Mask = 0;

	*pbSb_Ready = (bool)false;

	if (dev_param->ePhy_Dev != PHY_DEVICE_SEC) {
		dptx_debug("Nothing to do not for Samsung Phy");

		goto return_funcs;
	}

	uiReg_Offset = dev_param->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		goto return_funcs;
	}

	uiReg_Mask = (uint32_t)SB_RDY;

	do {
		uiReg_R_data = Dptx_Reg_Readl(dev_param, (uiReg_Offset + (uint32_t)DP_CFG_PHY_DEBUG));

		bSb_Rdy = ((uiReg_R_data & uiReg_Mask) == uiReg_Mask) ? (bool)true : (bool)false;

		if (bSb_Rdy) {
			dptx_debug("Sec Phy SB ready[0x%x] after %u us", uiReg_R_data, (ucCount * 100));
			break;
		}
		udelay(100);
		ucCount++;
	} while (ucCount < (uint8_t)MAX_TRY_PHY_PLL_LOCK);

	if (!bSb_Rdy) {
		dptx_err("Sec Phy SB not ready(0x%x)", uiReg_R_data);

		iRetVal = DPTX_RETURN_EBUSY;

		goto return_funcs;
	}

	*pbSb_Ready = (bool)true;

return_funcs:
	return iRetVal;
}

void dptx_cfg_get_default_phy_eq(struct Dptx_Params *dev_param, uint32_t default_main_eq[16], uint32_t default_post_eq[16])
{
	if (dev_param->ePhy_Dev == PHY_DEVICE_SNPS) {
		(void)memcpy(default_main_eq, default_d3_main_eq, sizeof(uint32_t) * 16u);
		(void)memcpy(default_post_eq, default_d3_post_eq, sizeof(uint32_t) * 16u);
	}
}

static uint32_t dptx_cfg_get_equalization_internal(struct Dptx_Params *dev_param,
						  bool main_equalization,
						  uint8_t link_rate)
{
	uint32_t reg_val, reg_mask, reg_shift, reg_offset;
	uint32_t eq_val = 0u;

	int32_t ret = DPTX_RETURN_NO_ERROR;

	if (dev_param->ePhy_Dev != PHY_DEVICE_SNPS) {
		dptx_debug("Nothing to do not for Samsung PHY");
	} else {
		if (eq_val > 31u) {  /* 0 to 31 */
			dptx_err("Invalid post equalization as %u", eq_val);
			ret = -DPTX_RETURN_EINVAL;
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			if (link_rate >= LINK_RATE_MAX) {
				/* For KCS */
				ret = -DPTX_RETURN_EINVAL;
			}
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			if (!CHECK_REG_OFFSET(dev_param->uiRegBank_RegAddr_Offset)) {
				dptx_err("Invalid reg offset as 0x%x", dev_param->uiRegBank_RegAddr_Offset);
				ret = -DPTX_RETURN_EINVAL;
			}
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			if (main_equalization) {
				reg_offset = DP_REGISTER_BANK_REG_15;
				reg_shift = 8u * link_rate;
				reg_mask = 0x1Fu;
			} else {
				reg_offset = DP_REGISTER_BANK_REG_16;
				reg_shift = 4u * link_rate;
				reg_mask = 0xFu;
			}
			reg_val = Dptx_Reg_Readl(dev_param, (dev_param->uiRegBank_RegAddr_Offset +  reg_offset));
			eq_val = (reg_val >> reg_shift) & reg_mask;
		}
	}
	return eq_val;
}

uint32_t dptx_cfg_get_main_equalization(struct Dptx_Params *dev_param, uint8_t link_rate)
{
	return dptx_cfg_get_equalization_internal(dev_param, true, link_rate);
}

uint32_t dptx_cfg_get_post_equalization(struct Dptx_Params *dev_param, uint8_t link_rate)
{
	return dptx_cfg_get_equalization_internal(dev_param, false, link_rate);
}

static int32_t dptx_cfg_set_equalization_internal(struct Dptx_Params *dev_param,
						  bool main_equalization,
						  uint8_t link_rate, uint8_t eq_val)
{
	uint32_t reg_val, reg_shift, reg_offset;
	int32_t ret = DPTX_RETURN_NO_ERROR;

	if (dev_param->ePhy_Dev != PHY_DEVICE_SNPS) {
		dptx_debug("Nothing to do not for Samsung PHY");
	} else {
		if (eq_val > 31u) {  /* 0 to 31 */
			dptx_err("Invalid post equalization as %u", eq_val);
			ret = -DPTX_RETURN_EINVAL;
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			if (link_rate >= LINK_RATE_MAX) {
				/* For KCS */
				ret = -DPTX_RETURN_EINVAL;
			}
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			if (!CHECK_REG_OFFSET(dev_param->uiRegBank_RegAddr_Offset)) {
				dptx_err("Invalid reg offset as 0x%x", dev_param->uiRegBank_RegAddr_Offset);
				ret = -DPTX_RETURN_EINVAL;
			}
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			if (main_equalization) {
				reg_offset = DP_REGISTER_BANK_REG_15;
				reg_shift = 8u * link_rate;
				reg_val = ((uint32_t)1u << (reg_shift + 7u)) | ((eq_val & 0x1Fu) << reg_shift);
			} else {
				reg_offset = DP_REGISTER_BANK_REG_16;
				reg_shift = 4u * link_rate;
				reg_val = (eq_val & 0xFu) << reg_shift;
			}

			Dptx_Reg_Writel(dev_param,
					(dev_param->uiRegBank_RegAddr_Offset + reg_offset),
					reg_val);
		}
	}
	return ret;
}

int32_t dptx_cfg_set_main_equalization(struct Dptx_Params *dev_param, uint8_t link_rate, uint8_t eq_val)
{
	return dptx_cfg_set_equalization_internal(dev_param, true, link_rate, eq_val);
}

int32_t dptx_cfg_set_post_equalization(struct Dptx_Params *dev_param, uint8_t link_rate, uint8_t eq_val)
{
	return dptx_cfg_set_equalization_internal(dev_param, false, link_rate, eq_val);
}

int32_t dptx_cfg_set_manual_phy_signal_quality(struct Dptx_Params *dev_param,
					      uint8_t link_rate,
					      enum PHY_VOLTAGE_SWING_LEVEL voltage_swing,
					      enum PHY_PRE_EMPHASIS_LEVEL pre_emphasis)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint32_t eq_val, linear_idx;

	if (dev_param->ePhy_Dev == PHY_DEVICE_SNPS) {
		linear_idx = (voltage_swing * 4u) + pre_emphasis;

		if (!dev_param->hw_config.phy_eq_manual_mode) {
			/* For KCS */
			ret = -DPTX_RETURN_EINVAL;
		}
		if ((linear_idx == 7u) || (linear_idx == 10u) ||
			(linear_idx == 11u) || (linear_idx == 13u) ||
			(linear_idx == 14u) || (linear_idx == 15u) ||
			(linear_idx >= 16u)) {
			dptx_err("Not allowed voltage swing %u level and pre-emphsis %u level", voltage_swing, pre_emphasis);
			ret = -DPTX_RETURN_EINVAL;
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			if (link_rate >= LINK_RATE_MAX) {
				/* For KCS */
				ret = -DPTX_RETURN_EINVAL;
			}
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			eq_val = dev_param->hw_config.phy_eq[link_rate].main_eq[linear_idx];
			dptx_dbg("main %u %u = %u", voltage_swing, pre_emphasis, eq_val);
			ret = dptx_cfg_set_main_equalization(dev_param, link_rate, eq_val);
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			eq_val = dev_param->hw_config.phy_eq[link_rate].post_eq[linear_idx];
			dptx_dbg("post %u %u = %u", voltage_swing, pre_emphasis, eq_val);
			ret = dptx_cfg_set_post_equalization(dev_param, link_rate, eq_val);
		}
	}
	return ret;
}

