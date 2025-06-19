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

	{LINK_RATE_RBR,  DP_REGISTER_BANK_REG_16, REG_BANK_REG_16_RBR_INIT},
	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_4, REG_BANK_REG_4_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_4, REG_BANK_REG_4_INIT},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_7, REG_BANK_REG_7_INIT},

	{LINK_RATE_RBR,  DP_REGISTER_BANK_REG_15, REG_BANK_REG_15_RBR_INIT},
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
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_16, REG_BANK_REG_16_HBR3_INIT},


	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_16, REG_BANK_REG_16_HBR_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_16, REG_BANK_REG_16_HBR2_INIT},
	{LINK_RATE_HBR3, DP_REGISTER_BANK_REG_15, REG_BANK_REG_15_HBR3_INIT},

	{LINK_RATE_HBR,  DP_REGISTER_BANK_REG_15, REG_BANK_REG_15_HBR_INIT},
	{LINK_RATE_HBR2, DP_REGISTER_BANK_REG_15, REG_BANK_REG_15_HBR2_INIT},
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

static int32_t Dptx_Cfg_Reg_Init(struct Dptx_Params *pstDptx, uint8_t ucLinkRate)
{
	bool bOffsetInRange;
	uint8_t ucLink_Rate;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiReg_Addr, uiReg_R_data, uiReg_22_data, uiReg_W_data, uiReg_Offset;
	uint32_t uiElements;
	struct SNPY_CFG_Reg_Data *pstSNPY_CFG_Reg_Data;

	if (pstDptx->ePhy_Dev != PHY_DEVICE_SNPS) {
		dptx_debug("Nothing to do not for Synopsys PHY");

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	uiReg_Offset = pstDptx->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	uiReg_R_data = Dptx_Reg_Readl(pstDptx, (uiReg_Offset + (uint32_t)DP_REGISTER_BANK_REG_22));

	uiReg_22_data = ((uiReg_R_data & (uint32_t)AXI_SLAVE_BRIDGE_RST_MASK) != 0U) ? (uint32_t)0x00000008U : (uint32_t)0x00000000U;

	pstSNPY_CFG_Reg_Data = stSNPY_CFG_Reg_Data;

	for (uiElements = 0; (pstSNPY_CFG_Reg_Data[uiElements].uiReg_Add != (uint32_t)DP_REGISTER_BANK_REG_MAX); uiElements++) {
		ucLink_Rate = pstSNPY_CFG_Reg_Data[uiElements].ucLink_Rate;
		uiReg_W_data = pstSNPY_CFG_Reg_Data[uiElements].uiReg_Val;
		uiReg_Addr = pstSNPY_CFG_Reg_Data[uiElements].uiReg_Add;

		if (ucLinkRate != ucLink_Rate) {
			continue;
		}

		if ((uiReg_Addr == DP_REGISTER_BANK_REG_22) && (uiReg_W_data == DP_REGISTER_BANK_REG_RESET)) {
			uiReg_W_data = uiReg_22_data;

			/* It should be wait 10us before releasing a software reset of the DP PHY. */
			/* Coverity False positives - udelay
			 * [FP] misra_c_2012_rule_10_4_violation
			 * [FP] misra_c_2012_rule_12_1_violation
			 * [FP] misra_c_2012_rule_14_4_violation
			 * [FP] misra_c_2012_rule_15_6_violation
			 * [FP] cert_dcl37_c_violation
			 */
			udelay(10);
		}

		if ((ucLink_Rate == LINK_RATE_HBR3) && (uiReg_Addr == DP_REGISTER_BANK_REG_13)) {
			uiReg_W_data = (pstDptx->bSpreadSpectrum_Clock) ? REG_BANK_REG_13_HBR3_SSC : REG_BANK_REG_13_HBR3_INIT;
		}

		if ((ucLink_Rate == LINK_RATE_HBR3) && (uiReg_Addr == DP_REGISTER_BANK_REG_14)) {
			uiReg_W_data = (pstDptx->bSpreadSpectrum_Clock) ? REG_BANK_REG_14_HBR3_SSC : REG_BANK_REG_14_HBR3_INIT;
		}

		dptx_debug("Reg[0x%x] = 0x%x", pstSNPY_CFG_Reg_Data[uiElements].uireg_add, pstSNPY_CFG_Reg_Data[uiElements].uireg_val);

		Dptx_Reg_Writel(pstDptx, (uiReg_Offset + uiReg_Addr), uiReg_W_data);
	}

return_funcs:
	return iRetVal;
}

static int dptx_cfg_set_phy_sram_ext_ld_done(struct Dptx_Params *pstDptx)
{
	uint32_t regdata, regoffset;
	int ret = DPTX_RETURN_NO_ERROR;

	regoffset = pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_17;

	if (!CHECK_REG_OFFSET(regoffset)) {
		dptx_err("Invalid reg offset as 0x%x", regoffset);
		ret = -DPTX_RETURN_EINVAL;
	} else {
		regdata = Dptx_Reg_Readl(pstDptx, regoffset);
		regdata |= ((uint32_t)1u << 29);
		Dptx_Reg_Writel(pstDptx, regoffset, regdata);
	}

	return ret;
}

static bool dptx_cfg_get_phy_sram_init_done(struct Dptx_Params *pstDptx)
{
	uint32_t regdata, regoffset;
	bool ret = (bool)true;

	regoffset = pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_17;

	if (!CHECK_REG_OFFSET(regoffset)) {
		dptx_err("Invalid reg offset as 0x%x", regoffset);
		ret = (bool)false;
	} else {
		regdata = Dptx_Reg_Readl(pstDptx, regoffset);
		ret = ((regdata & ((uint32_t)1u << 24)) != 0u) ? (bool)true : (bool)false;
	}

	return ret;
}

/*
 * This workaround solves the problem of lane 1 of PHY getting stuck in Rx VCO
 * calibration even though the D3 PHY is DPYX only.
 * Note: This workaround will be only applied to D3 (PHY_DEVICE_SNPS)
 */
static int32_t dptx_cfg_bypass_rx_vco_calibration(struct Dptx_Params *pstDptx)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	unsigned int loop;

	if (pstDptx->ePhy_Dev == PHY_DEVICE_SNPS) {
		/*
		 * 1. DP Workaround
		 * Wait up to 400us for SRAM initialization to complete.
		 * Typically it will be completed within 100us.
		 */
		for (loop = 0u; loop < 400u ; loop++) {
			if (dptx_cfg_get_phy_sram_init_done(pstDptx)) {
				break;
			}
			/* Coverity False positives - udelay
			 * [FP] misra_c_2012_rule_10_4_violation
			 * [FP] misra_c_2012_rule_12_1_violation
			 * [FP] misra_c_2012_rule_14_4_violation
			 * [FP] misra_c_2012_rule_15_6_violation
			 * [FP] cert_dcl37_c_violation
			 */
			udelay(1);
		}

		/*
		 * 2. DP Workaround
		 * RAWLANEN_DIG_AON_FAST_FLAGS. FAST_RX_VCO_CAL
		 */
		dptx_phy_write(pstDptx, 0x315c, 0x4000);
		dptx_phy_write(pstDptx, 0x325c, 0x4000);

		/* 3. DP Workaround - Set sram ext ld done */
		ret = dptx_cfg_set_phy_sram_ext_ld_done(pstDptx);
	}
	return ret;
}

static int32_t Dptx_Cfg_Set_SDM_Bypass(struct Dptx_Params *pstDptx)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegAddr, uiRegMask;
	uint32_t uiReg_R_data, uiReg_W_data, uiReg_Offset;

	uiReg_Offset = pstDptx->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	uiRegAddr = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)DP_CFG_VIDEO_MUX : (uint32_t)DP_REGISTER_BANK_REG_24;
	uiRegMask = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_SDM_DIS_MASK : (uint32_t)SDM_DIS_MASK;

	uiReg_R_data = Dptx_Reg_Readl(pstDptx, (uiReg_Offset + uiRegAddr));
	uiReg_W_data = (uiReg_R_data | uiRegMask);
	Dptx_Reg_Writel(pstDptx, (uiReg_Offset +uiRegAddr), uiReg_W_data);

	dptx_debug("SDM Bypass - use video data2: Reg[0x%x]: 0x%08x -> 0x%08x", (uiReg_Offset + uiRegAddr), uiReg_R_data, uiReg_W_data);

return_funcs:
	return iRetVal;
}

static int32_t Dptx_Cfg_Set_TRVC_Bypass(struct Dptx_Params *pstDptx)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegAddr, uiRegMask, uiReg_Offset;
	uint32_t uiReg_R_data, uiReg_W_data;

	uiReg_Offset = pstDptx->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(pstDptx->uiRegBank_RegAddr_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", pstDptx->uiRegBank_RegAddr_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	uiRegAddr = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)DP_CFG_VIDEO_MUX : (uint32_t)DP_REGISTER_BANK_REG_24;
	uiRegMask = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_SRVC_DIS_MASK : (uint32_t)SRVC_DIS_MASK;

	uiReg_R_data = Dptx_Reg_Readl(pstDptx, (pstDptx->uiRegBank_RegAddr_Offset + uiRegAddr));
	uiReg_W_data = (uiReg_R_data | uiRegMask);
	Dptx_Reg_Writel(pstDptx, (pstDptx->uiRegBank_RegAddr_Offset +uiRegAddr), uiReg_W_data);

	dptx_debug("TRVC Bypass - use video data3: Reg[0x%x]: 0x%08x -> 0x%08x", (pstDptx->uiRegBank_RegAddr_Offset + uiRegAddr), uiReg_R_data, uiReg_W_data);

return_funcs:
	return iRetVal;
}

static int32_t Dptx_Cfg_Set_MuxSelect(struct Dptx_Params *pstDptx, uint8_t ucMux_Index, uint8_t ucDP_Idx)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint8_t ucRegMap_MuxSel_Shift = 0;
	uint32_t uiRegAddr, uiReg_Offset;
	uint32_t uiRegMap_MuxSel_Mask = 0;
	uint32_t uiRegMap_R_MuxSel = 0;
	uint32_t uiRegMap_W_MuxSel = 0;

	uiReg_Offset = pstDptx->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	uiRegAddr = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)DP_CFG_VIDEO_MUX : (uint32_t)DP_REGISTER_BANK_REG_24;

	switch (ucDP_Idx) {
	case (uint8_t)PHY_INPUT_STREAM_0:
		uiRegMap_MuxSel_Mask = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_S0_MUX_SEL_MASK : (uint32_t)SOURCE0_MUX_SEL_MASK;
		ucRegMap_MuxSel_Shift = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint8_t)CFG_S0_MUX_SEL_SHIFT : (uint8_t)SOURCE0_MUX_SEL_SHIFT;
		break;
	case (uint8_t)PHY_INPUT_STREAM_1:
		uiRegMap_MuxSel_Mask = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_S1_MUX_SEL_MASK : (uint32_t)SOURCE1_MUX_SEL_MASK;
		ucRegMap_MuxSel_Shift = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint8_t)CFG_S1_MUX_SEL_SHIFT : (uint8_t)SOURCE1_MUX_SEL_SHIFT;
		break;
	case (uint8_t)PHY_INPUT_STREAM_2:
		uiRegMap_MuxSel_Mask = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_S2_MUX_SEL_MASK : (uint32_t)SOURCE2_MUX_SEL_MASK;
		ucRegMap_MuxSel_Shift = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint8_t)CFG_S2_MUX_SEL_SHIFT : (uint8_t)SOURCE2_MUX_SEL_SHIFT;
		break;
	case (uint8_t)PHY_INPUT_STREAM_3:
	default:
		uiRegMap_MuxSel_Mask = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_S3_MUX_SEL_MASK : (uint32_t)SOURCE3_MUX_SEL_MASK;
		ucRegMap_MuxSel_Shift = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint8_t)CFG_S3_MUX_SEL_SHIFT : (uint8_t)SOURCE3_MUX_SEL_SHIFT;
		break;
	}

	uiRegMap_R_MuxSel = Dptx_Reg_Readl(pstDptx, (uint32_t)(uiReg_Offset + uiRegAddr));
	uiRegMap_W_MuxSel = (uiRegMap_R_MuxSel & ~uiRegMap_MuxSel_Mask);
	uiRegMap_W_MuxSel = (uiRegMap_W_MuxSel | (uint32_t)(ucMux_Index << ucRegMap_MuxSel_Shift));
	Dptx_Reg_Writel(pstDptx, (uiReg_Offset + uiRegAddr), uiRegMap_W_MuxSel);

	/* [FP] misra_c_2012_rule_12_1_violation*/
	/* [FP] misra_c_2012_rule_14_3_violation*/
	/* [FP] misra_c_2012_rule_21_6_violation*/
	dptx_debug("Mux select[0x%x](0x%x -> 0x%x): Mux %u -> DP %u",
					(uiReg_Offset + uiRegAddr),
					uiRegMap_R_MuxSel,
					uiRegMap_W_MuxSel,
					ucMux_Index,
					ucDP_Idx);

return_funcs:
	return iRetVal;
}

static int32_t Dptx_Cfg_Set_PHY_Standard_LaneCfg(struct Dptx_Params *pstDptx)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegAddr, uiReg_Offset;
	uint32_t uiRegMap_R_StdEn, uiRegMap_W_StdEn;

	if (pstDptx->ePhy_Dev != PHY_DEVICE_SNPS) {
		dptx_debug("Nothing to do not for Synopsys PHY");

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	uiReg_Offset = pstDptx->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	uiRegAddr = (uint32_t)DP_REGISTER_BANK_REG_24;
	uiRegMap_R_StdEn = Dptx_Reg_Readl(pstDptx, (uint32_t)(uiReg_Offset + uiRegAddr));

	uiRegMap_W_StdEn = (uiRegMap_R_StdEn | (uint32_t)STD_EN_MASK);

	Dptx_Reg_Writel(pstDptx, (uiReg_Offset + uiRegAddr), uiRegMap_W_StdEn);

	/* [FP] misra_c_2012_rule_12_1_violation*/
	/* [FP] misra_c_2012_rule_14_3_violation*/
	/* [FP] misra_c_2012_rule_21_6_violation*/
	dptx_debug("PHY Lanes sets to standard: 0x%08x -> 0x%08x", uiRegMap_R_StdEn, uiRegMap_W_StdEn);

return_funcs:
	return iRetVal;
}

int32_t Dptx_Cfg_Get_SDM_Bypass(struct Dptx_Params *pstDptx, bool *pbSdm_Bypass)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegAddr, uiRegMask;
	uint32_t uiReg_R_data,  uiReg_Offset;

	uiReg_Offset = pstDptx->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	uiRegAddr = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)DP_CFG_VIDEO_MUX : (uint32_t)DP_REGISTER_BANK_REG_24;
	uiRegMask = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_SDM_DIS_MASK : (uint32_t)SDM_DIS_MASK;

	uiReg_R_data = Dptx_Reg_Readl(pstDptx, (uiReg_Offset + uiRegAddr));
	*pbSdm_Bypass = (uiReg_R_data & uiRegMask) ? (bool)true : (bool)false;

	dptx_debug("SDM Path -> %s", (*pbSdm_Bypass) ? "bypass" : "use video data");

return_funcs:
	return iRetVal;
}

int32_t Dptx_Cfg_Get_TRVC_Bypass(struct Dptx_Params *pstDptx, bool *pbTrvc_Bypass)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegAddr, uiRegMask, uiReg_Offset;
	uint32_t uiReg_R_data;

	uiReg_Offset = pstDptx->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(pstDptx->uiRegBank_RegAddr_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", pstDptx->uiRegBank_RegAddr_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	uiRegAddr = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)DP_CFG_VIDEO_MUX : (uint32_t)DP_REGISTER_BANK_REG_24;
	uiRegMask = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_SRVC_DIS_MASK : (uint32_t)SRVC_DIS_MASK;

	uiReg_R_data = Dptx_Reg_Readl(pstDptx, (pstDptx->uiRegBank_RegAddr_Offset + uiRegAddr));
	*pbTrvc_Bypass = (uiReg_R_data & uiRegMask) ? (bool)true : (bool)false;

	dptx_debug("TRVC Path -> %s", (*pbTrvc_Bypass) ? "bypass" : "use video data");

return_funcs:
	return iRetVal;
}

int32_t Dptx_Cfg_Get_MuxSelect(struct Dptx_Params *pstDptx, uint8_t ucDP_Idx, uint8_t *pucMux_Index)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint8_t ucRegMap_MuxSel_Shift = 0;
	uint32_t uiRegAddr, uiReg_Offset;
	uint32_t uiRegMap_MuxSel_Mask = 0;
	uint32_t uiRegMap_R_MuxSel = 0;

	uiReg_Offset = pstDptx->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	uiRegAddr = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)DP_CFG_VIDEO_MUX : (uint32_t)DP_REGISTER_BANK_REG_24;

	switch (ucDP_Idx) {
	case (uint8_t)PHY_INPUT_STREAM_0:
		uiRegMap_MuxSel_Mask = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_S0_MUX_SEL_MASK : (uint32_t)SOURCE0_MUX_SEL_MASK;
		ucRegMap_MuxSel_Shift = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint8_t)CFG_S0_MUX_SEL_SHIFT : (uint8_t)SOURCE0_MUX_SEL_SHIFT;
		break;
	case (uint8_t)PHY_INPUT_STREAM_1:
		uiRegMap_MuxSel_Mask = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_S1_MUX_SEL_MASK : (uint32_t)SOURCE1_MUX_SEL_MASK;
		ucRegMap_MuxSel_Shift = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint8_t)CFG_S1_MUX_SEL_SHIFT : (uint8_t)SOURCE1_MUX_SEL_SHIFT;
		break;
	case (uint8_t)PHY_INPUT_STREAM_2:
		uiRegMap_MuxSel_Mask = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_S2_MUX_SEL_MASK : (uint32_t)SOURCE2_MUX_SEL_MASK;
		ucRegMap_MuxSel_Shift = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint8_t)CFG_S2_MUX_SEL_SHIFT : (uint8_t)SOURCE2_MUX_SEL_SHIFT;
		break;
	case (uint8_t)PHY_INPUT_STREAM_3:
	default:
		uiRegMap_MuxSel_Mask = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)CFG_S3_MUX_SEL_MASK : (uint32_t)SOURCE3_MUX_SEL_MASK;
		ucRegMap_MuxSel_Shift = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint8_t)CFG_S3_MUX_SEL_SHIFT : (uint8_t)SOURCE3_MUX_SEL_SHIFT;
		break;
	}

	uiRegMap_R_MuxSel = Dptx_Reg_Readl(pstDptx, (uint32_t)(uiReg_Offset + uiRegAddr));

	*pucMux_Index = (uint8_t)((uiRegMap_R_MuxSel & uiRegMap_MuxSel_Mask) >> ucRegMap_MuxSel_Shift);

	/* [FP] misra_c_2012_rule_12_1_violation*/
	/* [FP] misra_c_2012_rule_14_3_violation*/
	/* [FP] misra_c_2012_rule_21_6_violation*/
	dptx_debug("Mux select[0x%x](0x%x): Mux %u -> DP %u",
					(uiReg_Offset + uiRegAddr),
					uiRegMap_R_MuxSel,
					*pucMux_Index,
					ucDP_Idx);

return_funcs:
	return iRetVal;
}

int32_t Dptx_Cfg_Init_Params(struct Dptx_Params *pstDptx)
{
	uint8_t ucDpIdx;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	if ((pstDptx->ePhy_Dev == PHY_DEVICE_SNPS) && (pstDptx->uiTCC80xx_Rev == (uint32_t)TCC80XX_REVISION_ES)) {
		pstDptx->bPhy_Lane_Std = (bool)false;
		pstDptx->bSDM_Bypass = (bool)false;
		pstDptx->bTRVC_Bypass = (bool)false;

		for (ucDpIdx = 0; ucDpIdx < (uint8_t)PHY_INPUT_STREAM_MAX; ucDpIdx++) {
			/*For KCS*/
			pstDptx->aucMux_Idx[ ucDpIdx] = ucDpIdx;
		}

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	(void)Dptx_Cfg_Get_PHY_Standard_LaneCfg(pstDptx, &pstDptx->bPhy_Lane_Std);
	(void)Dptx_Cfg_Get_SDM_Bypass(pstDptx, &pstDptx->bSDM_Bypass);
	(void)Dptx_Cfg_Get_TRVC_Bypass(pstDptx, &pstDptx->bTRVC_Bypass);

	for (ucDpIdx = 0; ucDpIdx < (uint8_t)PHY_INPUT_STREAM_MAX; ucDpIdx++) {
		/*For KCS*/
		(void)Dptx_Cfg_Get_MuxSelect(pstDptx, ucDpIdx, &pstDptx->aucMux_Idx[      ucDpIdx]);
	}

return_funcs:
	return iRetVal;
}

int32_t Dptx_Cfg_Get_PHY_Standard_LaneCfg(struct Dptx_Params *pstDptx, bool *pbPhy_LaneCfg)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegAddr, uiReg_Offset;
	uint32_t uiRegMap_R_StdEn;

	if (pstDptx->ePhy_Dev != PHY_DEVICE_SNPS) {
		dptx_debug("Nothing to do not for Synopsys PHY");

		*pbPhy_LaneCfg = (bool)true;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	uiReg_Offset = pstDptx->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	uiRegAddr = (uint32_t)DP_REGISTER_BANK_REG_24;
	uiRegMap_R_StdEn = Dptx_Reg_Readl(pstDptx, (uint32_t)(uiReg_Offset + uiRegAddr));

	*pbPhy_LaneCfg = (uiRegMap_R_StdEn & (uint32_t)STD_EN_MASK) ? (bool)true : (bool)false;

	dptx_debug("PHY Lanes configuration to %s", (*pbPhy_LaneCfg) ? "standard" : "swap");

return_funcs:
	return iRetVal;
}

int32_t Dptx_Cfg_Init(struct Dptx_Params *pstDptx, uint8_t ucLinkRate)
{
	uint8_t ucDpIdx;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	iRetVal = Dptx_Cfg_Reg_Init(pstDptx, ucLinkRate);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	iRetVal = dptx_cfg_bypass_rx_vco_calibration(pstDptx);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	iRetVal = (pstDptx->bSDM_Bypass) ? Dptx_Cfg_Set_SDM_Bypass(pstDptx) : DPTX_RETURN_NO_ERROR;
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	iRetVal = (pstDptx->bTRVC_Bypass) ? Dptx_Cfg_Set_TRVC_Bypass(pstDptx) : DPTX_RETURN_NO_ERROR;
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	iRetVal = (pstDptx->bPhy_Lane_Std) ? Dptx_Cfg_Set_PHY_Standard_LaneCfg(pstDptx) : DPTX_RETURN_NO_ERROR;
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	for (ucDpIdx = 0; ucDpIdx < pstDptx->ucNumOfPorts; ucDpIdx++) {
		iRetVal = (pstDptx->aucMux_Idx[ucDpIdx] != ucDpIdx) ? Dptx_Cfg_Set_MuxSelect(pstDptx, pstDptx->aucMux_Idx[ucDpIdx], ucDpIdx) : DPTX_RETURN_NO_ERROR;
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			/* [FP] misra_c_2012_rule_15_1_violation*/
			goto return_funcs;
		}
	}

return_funcs:
	return iRetVal;
}

int32_t Dptx_Cfg_Reset(struct Dptx_Params *pstDptx, uint8_t ucLinkRate)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	if (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) {
		uint32_t reg_wval;

		dptx_debug("Reset for Samsung PHY");

		reg_wval = (uint32_t)(PHY_CLK_DIV_DISABLE|LC_REF_RO_PLL);
		Dptx_Cfg_Set_PHY_Cfg(pstDptx, reg_wval);

		reg_wval = (uint32_t)(CFG_LANE_RESET | CFG_CMN_RESET | CFG_INT_RESET);
		Dptx_Cfg_SoftReset(pstDptx, reg_wval);


		reg_wval |= (uint32_t)CFG_DP_PHY_APB_RESET;
		Dptx_Cfg_SoftReset(pstDptx, reg_wval);
		/* Coverity False positives - udelay
		 * [FP] misra_c_2012_rule_10_4_violation
		 * [FP] misra_c_2012_rule_12_1_violation
		 * [FP] misra_c_2012_rule_14_4_violation
		 * [FP] misra_c_2012_rule_15_6_violation
		 * [FP] cert_dcl37_c_violation
		 */
		udelay(1);

		reg_wval &= ~(uint32_t)CFG_DP_PHY_APB_RESET;
		Dptx_Cfg_SoftReset(pstDptx, reg_wval);

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	iRetVal = Dptx_Cfg_Reg_Init(pstDptx, ucLinkRate);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	iRetVal = dptx_cfg_bypass_rx_vco_calibration(pstDptx);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

return_funcs:
	return iRetVal;
}

int32_t Dptx_Cfg_SoftReset(struct Dptx_Params *pstDptx, uint32_t uiVal)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiReg_W_data, uiReg_Offset;

	if (pstDptx->ePhy_Dev != PHY_DEVICE_SEC) {
		dptx_debug("Nothing to do not for Samsung Phy");

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	uiReg_Offset = pstDptx->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	uiReg_W_data = uiVal;

	dptx_debug("Set Reset : 0x%x", uiReg_W_data);

	Dptx_Reg_Writel(pstDptx, (uint32_t)(uiReg_Offset + (uint32_t)DP_CFG_SOFT_RESET), uiReg_W_data);

return_funcs:
	return iRetVal;
}



int32_t Dptx_Cfg_Set_PHY_Cfg(struct Dptx_Params *pstDptx, uint32_t uiCfg_Val)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiReg_Offset, uiReg_W_data = 0, uiReg_R_data = 0;

	if (pstDptx->ePhy_Dev != PHY_DEVICE_SEC) {
		dptx_debug("Nothing to do not for Samsung Phy");

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	uiReg_Offset = pstDptx->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	uiReg_R_data = Dptx_Reg_Readl(pstDptx, (uiReg_Offset + (uint32_t)DP_CFG_PHY_CFG));

	//uiReg_W_data = (uiReg_R_data | uiCfg_Val);
	uiReg_W_data = uiCfg_Val;

	dptx_debug("Set PHY CFG : 0x%x -> 0x%x", uiReg_R_data, uiReg_W_data);

	Dptx_Reg_Writel(pstDptx, (uiReg_Offset + (uint32_t)DP_CFG_PHY_CFG), uiReg_W_data);
return_funcs:
	return iRetVal;
}

int32_t Dptx_Cfg_Check_PHY_Pll(struct Dptx_Params *pstDptx, bool *pbPll_Ready)
{
	bool bOffsetInRange, bPll_Lock, bPll_Rdy;
	uint8_t ucCount = 0;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiReg_Offset, uiReg_R_data, uiReg_Mask = 0;

	if (pstDptx->ePhy_Dev != PHY_DEVICE_SEC) {
		dptx_debug("Nothing to do not for Samsung Phy");

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	*pbPll_Ready = (bool)false;
	uiReg_Offset = pstDptx->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	uiReg_Mask = (uint32_t)PLL_LOCK_DONE;

	do {
		uiReg_R_data = Dptx_Reg_Readl(pstDptx, (uiReg_Offset + (uint32_t)DP_CFG_PHY_DEBUG));

		bPll_Lock = ((uiReg_R_data & uiReg_Mask) == uiReg_Mask) ? (bool)true : (bool)false;

		if (bPll_Lock) {
			dptx_debug("Sec Phy Pll locked[0x%x] after %u us", uiReg_R_data, (ucCount * 100));
			break;
		}

		/* Coverity False positives - udelay
		 * [FP] misra_c_2012_rule_10_4_violation
		 * [FP] misra_c_2012_rule_12_1_violation
		 * [FP] misra_c_2012_rule_14_4_violation
		 * [FP] misra_c_2012_rule_15_6_violation
		 * [FP] cert_dcl37_c_violation
		 */
		udelay(100);
		ucCount++;
	} while (!bPll_Lock && (ucCount < (uint8_t)MAX_TRY_PHY_PLL_LOCK));

	if (!bPll_Lock) {
		dptx_err("Sec Phy Pll unlocked after 10ms");

		iRetVal = DPTX_RETURN_EBUSY;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	ucCount = 0;
	uiReg_Mask = (uint32_t)(PLL_LOCK_RDY | PLL_LOCK_DONE);

	do {
		uiReg_R_data = Dptx_Reg_Readl(pstDptx, (uiReg_Offset + (uint32_t)DP_CFG_PHY_DEBUG));

		bPll_Rdy = ((uiReg_R_data & uiReg_Mask) == uiReg_Mask) ? (bool)true : (bool)false;

		if (bPll_Rdy) {
			dptx_debug("Sec Phy Pll done[0x%x] ater %u us", uiReg_R_data, (ucCount * 100));
			break;
		}
		/* Coverity False positives - udelay
		 * [FP] misra_c_2012_rule_10_4_violation
		 * [FP] misra_c_2012_rule_12_1_violation
		 * [FP] misra_c_2012_rule_14_4_violation
		 * [FP] misra_c_2012_rule_15_6_violation
		 * [FP] cert_dcl37_c_violation
		 */
		udelay(100);
		ucCount++;
	} while (!bPll_Rdy && (ucCount < (uint8_t)MAX_TRY_PHY_PLL_LOCK));

	if (!bPll_Rdy) {
		dptx_err("Sec Phy Pll locking isn't ready");

		iRetVal = DPTX_RETURN_EBUSY;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	*pbPll_Ready = (bool)true;

return_funcs:
	return iRetVal;
}

int32_t Dptx_Cfg_Check_PHY_Ready(struct Dptx_Params *pstDptx, bool *pbPhy_Ready)
{
	bool bOffsetInRange, bPhy_Rdy;
	uint8_t ucCount = 0;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiReg_Offset, uiReg_R_data, uiReg_Mask = 0;

	if (pstDptx->ePhy_Dev != PHY_DEVICE_SEC) {
		dptx_debug("Nothing to do not for Samsung Phy");

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	*pbPhy_Ready = (bool)false;

	uiReg_Offset = pstDptx->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	uiReg_Mask = (uint32_t)(PHY_RDY | PLL_LOCK_RDY | PLL_LOCK_DONE);

	do {
		uiReg_R_data = Dptx_Reg_Readl(pstDptx, (uiReg_Offset + (uint32_t)DP_CFG_PHY_DEBUG));

		bPhy_Rdy = ((uiReg_R_data & uiReg_Mask) == uiReg_Mask) ? (bool)true : (bool)false;

		if (bPhy_Rdy) {
			dptx_debug("Sec Phy ready[0x%x] after %u us", uiReg_R_data, (ucCount * 100));
			break;
		}
		/* Coverity False positives - udelay
		 * [FP] misra_c_2012_rule_10_4_violation
		 * [FP] misra_c_2012_rule_12_1_violation
		 * [FP] misra_c_2012_rule_14_4_violation
		 * [FP] misra_c_2012_rule_15_6_violation
		 * [FP] cert_dcl37_c_violation
		 */
		udelay(100);
		ucCount++;
	} while (!bPhy_Rdy && (ucCount < (uint8_t)MAX_TRY_PHY_PLL_LOCK));

	if (!bPhy_Rdy) {
		dptx_err("Sec Phy not ready(0x%x)", uiReg_R_data);

		iRetVal = DPTX_RETURN_EBUSY;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	*pbPhy_Ready = (bool)true;

return_funcs:
	return iRetVal;
}

int32_t Dptx_Cfg_Check_Sec_PHY_SB_Ready(struct Dptx_Params *pstDptx, bool *pbSb_Ready)
{
	bool bOffsetInRange, bSb_Rdy;
	uint8_t ucCount = 0;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiReg_Offset, uiReg_R_data, uiReg_Mask = 0;

	*pbSb_Ready = (bool)false;

	if (pstDptx->ePhy_Dev != PHY_DEVICE_SEC) {
		dptx_debug("Nothing to do not for Samsung Phy");

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	uiReg_Offset = pstDptx->uiRegBank_RegAddr_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(uiReg_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", uiReg_Offset);

		iRetVal = DPTX_RETURN_EINVAL;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	uiReg_Mask = (uint32_t)SB_RDY;

	do {
		uiReg_R_data = Dptx_Reg_Readl(pstDptx, (uiReg_Offset + (uint32_t)DP_CFG_PHY_DEBUG));

		bSb_Rdy = ((uiReg_R_data & uiReg_Mask) == uiReg_Mask) ? (bool)true : (bool)false;

		if (bSb_Rdy) {
			dptx_debug("Sec Phy SB ready[0x%x] after %u us", uiReg_R_data, (ucCount * 100));
			break;
		}
		/* Coverity False positives - udelay
		 * [FP] misra_c_2012_rule_10_4_violation
		 * [FP] misra_c_2012_rule_12_1_violation
		 * [FP] misra_c_2012_rule_14_4_violation
		 * [FP] misra_c_2012_rule_15_6_violation
		 * [FP] cert_dcl37_c_violation
		 */
		udelay(100);
		ucCount++;
	} while (!bSb_Rdy && (ucCount < (uint8_t)MAX_TRY_PHY_PLL_LOCK));

	if (!bSb_Rdy) {
		dptx_err("Sec Phy SB not ready(0x%x)", uiReg_R_data);

		iRetVal = DPTX_RETURN_EBUSY;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	*pbSb_Ready = (bool)true;

return_funcs:
	return iRetVal;
}



