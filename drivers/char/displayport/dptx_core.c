/*
 * Copyright (c) 2016 Synopsys, Inc.
 *
 * Synopsys DP TX Linux Software Driver and documentation (hereinafter,
 * "Software") is an Unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto. You are permitted to use and
 * redistribute this Software in source and binary forms, with or without
 * modification, provided that redistributions of source code must retain this
 * notice. You may not view, use, disclose, copy or distribute this file or
 * any information contained herein except pursuant to this license grant from
 * Synopsys. If you do not agree with this notice, including the disclaimer
 * below, then you are not authorized to use the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
*/

/*
* Modified by Telechips Inc.
*/

#include <linux/delay.h>

#include "dptx_v14.h"
#include "dptx_drm_dp_addition.h"
#include "dptx_reg.h"
#include "dptx_dbg.h"

#define	MAX_NUM_OF_LOOP_PHY_STATUS	100

#define PHY_LANE_ID_0			0
#define PHY_LANE_ID_1			1
#define PHY_LANE_ID_2			2
#define PHY_LANE_ID_3			3

#define	PHY_NUM_OF_1_LANE		1
#define	PHY_NUM_OF_2_LANE		2
#define	PHY_NUM_OF_4_LANE		4


static int32_t dptx_core_check_vendor_id(struct Dptx_Params *pstDptx)
{
	uint32_t uiDptx_id;

	uiDptx_id = Dptx_Reg_Readl(pstDptx, DPTX_ID);
	if (uiDptx_id != (uint32_t)((DPTX_ID_DEVICE_ID << DPTX_ID_DEVICE_ID_SHIFT) | DPTX_ID_VENDOR_ID)) {
		dptx_err("Invalid DPTX Id : 0x%x<->0x%x ",
					uiDptx_id,
					((DPTX_ID_DEVICE_ID << DPTX_ID_DEVICE_ID_SHIFT) | DPTX_ID_VENDOR_ID));
		return DPTX_RETURN_ENODEV;
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Init_Params(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	(void)Dptx_Core_Get_PHY_NumOfLanes(pstDptx, &pstDptx->ucMax_Lanes);
	pstDptx->stDptxLink.ucNumOfLanes = pstDptx->ucMax_Lanes;

	(void)Dptx_Core_Get_PHY_Rate(pstDptx, &pstDptx->ucMax_Rate);
	pstDptx->stDptxLink.ucLinkRate  = (uint8_t)pstDptx->ucMax_Rate;

	(void)Dptx_Core_Get_Stream_Mode(pstDptx, &pstDptx->bMultStreamTransport);

	(void)Dptx_Core_Get_PHY_SSC(pstDptx, &pstDptx->bSpreadSpectrum_Clock);

	return iRetVal;
}

int32_t Dptx_Core_Init(struct Dptx_Params *pstDptx)
{
	char aucVerStr[15];
	uint8_t ucPhy_data_width;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	unsigned int reg_data;

	iRetVal = dptx_core_check_vendor_id(pstDptx);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	Dptx_Core_Disable_Global_Intr(pstDptx);

	Dptx_Core_Soft_Reset(pstDptx, DPTX_SRST_CTRL_ALL);

	memset(aucVerStr, 0, sizeof(aucVerStr));

	/* DPTX version */
	reg_data = Dptx_Reg_Readl(pstDptx, DPTX_VER_NUMBER);
	aucVerStr[0] = (reg_data >> 24) & 0xff;
	aucVerStr[1] = '.';
	aucVerStr[2] = (reg_data >> 16) & 0xff;
	aucVerStr[3] = (reg_data >> 8) & 0xff;
	aucVerStr[4] = (reg_data & 0xff);

	reg_data = Dptx_Reg_Readl(pstDptx, DPTX_VER_TYPE);
	aucVerStr[5] = '-';
	aucVerStr[6] = (reg_data >> 24) & 0xff;
	aucVerStr[7] = (reg_data >> 16) & 0xff;
	aucVerStr[8] = (reg_data >> 8) & 0xff;
	aucVerStr[9] = (reg_data & 0xff);

	dptx_debug("Core version: %s ", aucVerStr);

	ucPhy_data_width = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint8_t)PHY_DATA_WIDTH_40BITS : (uint8_t)PHY_DATA_WIDTH_20BITS;
	Dptx_Core_Init_PHY(pstDptx, ucPhy_data_width);

	reg_data = Dptx_Reg_Readl(pstDptx, DPTX_HPD_IEN);
	reg_data |= (DPTX_HPD_IEN_IRQ_EN | DPTX_HPD_IEN_HOT_PLUG_EN | DPTX_HPD_IEN_HOT_UNPLUG_EN | DPTX_HPDSTS_UNPLUG_ERR_EN);
	Dptx_Reg_Writel(pstDptx, DPTX_HPD_IEN, reg_data);

	reg_data = Dptx_Reg_Readl(pstDptx, DPTX_HDCP_API_INT_MSK);
	reg_data |= DPTX_HDCP22_GPIOINT;
	Dptx_Reg_Writel(pstDptx, DPTX_HDCP_API_INT_MSK, reg_data);

	Dptx_Core_Enable_Global_Intr(pstDptx, DPTX_IEN_ALL_INTR);

	reg_data = Dptx_Reg_Readl(pstDptx, DPTX_TYPE_C_CTRL);
	reg_data &= ~(DPTX_TYPEC_DISABLE_ACK);
	reg_data &= ~(DPTX_TYPEC_DISABLE_STATUS);
	reg_data |= DPTX_TYPEC_INTRURPPT_STATUS;
	Dptx_Reg_Writel(pstDptx, DPTX_TYPE_C_CTRL, reg_data);

	reg_data = Dptx_Reg_Readl(pstDptx, DPTX_CCTL);
	reg_data |= DPTX_CCTL_ENH_FRAME_EN;
	reg_data &= ~DPTX_CCTL_SCALE_DOWN_MODE;
	reg_data &= ~DPTX_CCTL_FAST_LINK_TRAINED_EN;
	Dptx_Reg_Writel(pstDptx, DPTX_CCTL, reg_data);

	/*
	 * Disable Audio data input to prevent Audio FIFO Overflow
	 */
	reg_data = Dptx_Reg_Readl(pstDptx, DPTX_AUD_CONFIG1);
	reg_data &= ~DPTX_AUD_CONFIG1_DATA_EN_IN_MASK;
	Dptx_Reg_Writel(pstDptx, DPTX_AUD_CONFIG1, reg_data);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Deinit(struct Dptx_Params *pstDptx)
{
	Dptx_Core_Disable_Global_Intr(pstDptx);
	Dptx_Core_Soft_Reset(pstDptx, DPTX_SRST_CTRL_ALL);

	return DPTX_RETURN_NO_ERROR;
}

void Dptx_Core_Init_PHY(struct Dptx_Params *pstDptx, uint8_t ucData_Width)
{
	uint32_t ucRegMap_PhyCtrl;

	ucRegMap_PhyCtrl = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);

	ucRegMap_PhyCtrl &= ~DPTX_PHYIF_CTRL_WIDTH_MASK;
	ucRegMap_PhyCtrl |= ((ucData_Width << DPTX_PHYIF_CTRL_WIDTH_SHIFT) & DPTX_PHYIF_CTRL_WIDTH_MASK);

	Dptx_Reg_Writel(pstDptx, DPTX_PHYIF_CTRL, ucRegMap_PhyCtrl);
}

void Dptx_Core_Soft_Reset(struct Dptx_Params *pstDptx, uint32_t uiReset_Bits)
{
	uint32_t uiRegMap_Reset, uiRegMap_BitMask;

	uiRegMap_BitMask = (uiReset_Bits & DPTX_SRST_CTRL_ALL);

	uiRegMap_Reset = Dptx_Reg_Readl(pstDptx, DPTX_SRST_CTRL);
	uiRegMap_Reset |= uiRegMap_BitMask;
	Dptx_Reg_Writel(pstDptx, DPTX_SRST_CTRL, uiRegMap_Reset);
	/* Coverity False positives - udelay
	 * [FP] misra_c_2012_rule_10_4_violation
	 * [FP] misra_c_2012_rule_12_1_violation
	 * [FP] misra_c_2012_rule_14_4_violation
	 * [FP] misra_c_2012_rule_15_6_violation
	 * [FP] cert_dcl37_c_violation
	 */
	udelay(20);

	uiRegMap_Reset = Dptx_Reg_Readl(pstDptx, DPTX_SRST_CTRL);
	uiRegMap_Reset &= ~uiRegMap_BitMask;
	Dptx_Reg_Writel(pstDptx, DPTX_SRST_CTRL, uiRegMap_Reset);
}

void Dptx_Core_Enable_Global_Intr(struct Dptx_Params *pstDptx, uint32_t uiEnable_Bits)
{
	uint32_t uiRegMap_IntEn;

	uiEnable_Bits &= ~(DPTX_IEN_AUX_REPLY | DPTX_IEN_AUX_CMD_INVALID);

	uiRegMap_IntEn = Dptx_Reg_Readl(pstDptx, DPTX_IEN);
	uiRegMap_IntEn |= uiEnable_Bits;
	Dptx_Reg_Writel(pstDptx, DPTX_IEN, uiRegMap_IntEn);
}

void Dptx_Core_Disable_Global_Intr(struct Dptx_Params *pstDptx)
{
	uint32_t uiIntEnable;

	uiIntEnable = Dptx_Reg_Readl(pstDptx, DPTX_IEN);
	uiIntEnable &= ~DPTX_IEN_ALL_INTR;
	Dptx_Reg_Writel(pstDptx, DPTX_IEN, uiIntEnable);

	/* Clean HPD interrupt and global interrupts */
	Dptx_Reg_Writel(pstDptx, DPTX_HPDSTS, DPTX_HPDSTS_IRQ);
	Dptx_Core_Clear_General_Interrupt(pstDptx, DPTX_IEN_ALL_INTR);
}

int32_t Dptx_Core_Clear_General_Interrupt(struct Dptx_Params *pstDptx, uint32_t uiClear_Bits)
{
	uint32_t ucRegMap_GeneralIntr;

	ucRegMap_GeneralIntr = uiClear_Bits;
	Dptx_Reg_Writel(pstDptx, DPTX_ISTS, ucRegMap_GeneralIntr);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Set_PHY_SSC(struct Dptx_Params *pstDptx, bool bSink_Supports_SSC)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegMap_PhyIfCtrl;

	iRetVal = Dptx_Core_Set_PHY_PowerState(pstDptx, PHY_POWER_DOWN_PHY_CLOCK);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Core_Get_PHY_BUSY_Status(pstDptx, pstDptx->ucMax_Lanes);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		dptx_err("From Dptx_Core_Get_PHY_BUSY_Status()");
                return iRetVal;
	}

	uiRegMap_PhyIfCtrl = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);
	if (pstDptx->bSpreadSpectrum_Clock && bSink_Supports_SSC) {
		uiRegMap_PhyIfCtrl &= ~DPTX_PHYIF_CTRL_SSC_DIS;
	} else {
		uiRegMap_PhyIfCtrl |= DPTX_PHYIF_CTRL_SSC_DIS;
	}

	Dptx_Reg_Writel(pstDptx, DPTX_PHYIF_CTRL, uiRegMap_PhyIfCtrl);

	iRetVal = Dptx_Core_Set_PHY_PowerState(pstDptx, PHY_POWER_ON);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Core_Get_PHY_BUSY_Status(pstDptx, pstDptx->ucMax_Lanes);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		dptx_err("From Dptx_Core_Get_PHY_BUSY_Status()");
	}

	return iRetVal;
}

int32_t Dptx_Core_Get_PHY_SSC(struct Dptx_Params *pstDptx, bool *pbSSC_Enabled)
{
	uint32_t uiRegMap_PhyIfCtrl;

	uiRegMap_PhyIfCtrl = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);

	if (uiRegMap_PhyIfCtrl & DPTX_PHYIF_CTRL_SSC_DIS) {
		*pbSSC_Enabled = false;
	} else {
		*pbSSC_Enabled = true;
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Get_Sink_SSC_Capability(struct Dptx_Params *pstDptx, bool *pbSSC_Profiled)
{
	u8	ucDCDPValue;
	int32_t	iRetVal;

	iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_MAX_DOWNSPREAD, &ucDCDPValue);
	if (iRetVal != DPTX_RETURN_NO_ERROR)
		return iRetVal;

	if (ucDCDPValue & SINK_TDOWNSPREAD_MASK) {
		dptx_debug("SSC enable on the sink side");
		*pbSSC_Profiled = true;
	} else {
		dptx_debug("SSC disabled on the sink side");
		*pbSSC_Profiled = false;
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Get_Stream_Mode(struct Dptx_Params *pstDptx, bool *pbMST_Mode)
{
	uint32_t uiRegMap_Cctl;

	uiRegMap_Cctl = Dptx_Reg_Readl(pstDptx, DPTX_CCTL);

	if (uiRegMap_Cctl & DPTX_CCTL_ENABLE_MST_MODE) {
		*pbMST_Mode = true;
	} else {
		*pbMST_Mode = false;
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Set_PHY_NumOfLanes(struct Dptx_Params *pstDptx, uint8_t ucNumOfLanes)
{
	uint8_t ucPHY_Lanes;
	uint32_t uiRegMap_PhyIfCtrl;

	switch (ucNumOfLanes) {
	case 1:
		ucPHY_Lanes = 0;
		break;
	case 2:
		ucPHY_Lanes = 1;
		break;
	case 4:
		ucPHY_Lanes = 2;
		break;
	default:
		dptx_err("Invalid number of lanes -> %u lanes", ucNumOfLanes);
		return DPTX_RETURN_EINVAL;
	}

	uiRegMap_PhyIfCtrl = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);
	uiRegMap_PhyIfCtrl &= ~DPTX_PHYIF_CTRL_LANES_MASK;
	uiRegMap_PhyIfCtrl |= (ucPHY_Lanes << DPTX_PHYIF_CTRL_LANES_SHIFT);
	Dptx_Reg_Writel(pstDptx, DPTX_PHYIF_CTRL, uiRegMap_PhyIfCtrl);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Get_PHY_NumOfLanes(struct Dptx_Params *pstDptx, uint8_t *pucNumOfLanes)
{
	uint8_t ucNumOfLanes;
	uint32_t uiRagMap_PhyIfCtrl;

	uiRagMap_PhyIfCtrl = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);
	ucNumOfLanes = (uint8_t)((uiRagMap_PhyIfCtrl & DPTX_PHYIF_CTRL_LANES_MASK) >> DPTX_PHYIF_CTRL_LANES_SHIFT);

	*pucNumOfLanes = (1 << ucNumOfLanes);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Set_PHY_PowerState(struct Dptx_Params *pstDptx, enum PHY_POWER_STATE ePowerState)
{
	uint32_t uiRegMap_PhyIfCtrl;

	uiRegMap_PhyIfCtrl = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);
	uiRegMap_PhyIfCtrl &= ~DPTX_PHYIF_CTRL_LANE_PWRDOWN_MASK;

	switch (ePowerState) {
	case PHY_POWER_ON:
	case PHY_POWER_DOWN_SWITCHING_RATE:
	case PHY_POWER_DOWN_PHY_CLOCK:
	case PHY_POWER_DOWN_REF_CLOCK:
		uiRegMap_PhyIfCtrl |= (ePowerState << DPTX_PHYIF_CTRL_LANE_PWRDOWN_SHIFT);
		break;
	default:
		dptx_err("Invalid power state: %d\n", (uint32_t)ePowerState);
		return DPTX_RETURN_EINVAL;
	}

	Dptx_Reg_Writel(pstDptx, DPTX_PHYIF_CTRL, uiRegMap_PhyIfCtrl);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Get_PHY_BUSY_Status(struct Dptx_Params *pstDptx, uint8_t ucNumOfLanes)
{
	int32_t uiRegMap_PhyIfCtrl, uiBitMask = 0, uiCount = 0;

	if (pstDptx->ePhy_Dev != PHY_DEVICE_SNPS) {
		dptx_debug("Nothing to do not for Synopsys PHY");

		return DPTX_RETURN_NO_ERROR;
	}

	switch (ucNumOfLanes) {
	case PHY_NUM_OF_4_LANE:
		uiBitMask |= DPTX_PHYIF_CTRL_BUSY(3);
		uiBitMask |= DPTX_PHYIF_CTRL_BUSY(2);
		uiBitMask |= DPTX_PHYIF_CTRL_BUSY(1);
		uiBitMask |= DPTX_PHYIF_CTRL_BUSY(0);
		break;
	case PHY_NUM_OF_2_LANE:
		uiBitMask |= DPTX_PHYIF_CTRL_BUSY(1);
		uiBitMask |= DPTX_PHYIF_CTRL_BUSY(0);
		break;
	case PHY_NUM_OF_1_LANE:
		uiBitMask |= DPTX_PHYIF_CTRL_BUSY(0);
		break;
	default:
		dptx_err("Invalid number of lanes %d", (uint32_t)ucNumOfLanes);
		return DPTX_RETURN_EINVAL;
	}

	do {
		uiRegMap_PhyIfCtrl  = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);

		if (!(uiRegMap_PhyIfCtrl & uiBitMask)) {
			break;
		}

		if (uiCount == MAX_NUM_OF_LOOP_PHY_STATUS) {
			dptx_err("PHY BUSY timed out");
			return DPTX_RETURN_ENODEV;
		}

		mdelay(1);
	} while (uiCount++ < MAX_NUM_OF_LOOP_PHY_STATUS);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Set_PHY_Rate(struct Dptx_Params *pstDptx, enum PHY_LINK_RATE eRate)
{
	uint32_t uiPhyIfCtrl;

	uiPhyIfCtrl = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);
	uiPhyIfCtrl &= ~DPTX_PHYIF_CTRL_RATE_MASK;
	uiPhyIfCtrl |= (uint32_t)eRate << DPTX_PHYIF_CTRL_RATE_SHIFT;

	Dptx_Reg_Writel(pstDptx, DPTX_PHYIF_CTRL, uiPhyIfCtrl);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Get_PHY_Rate(struct Dptx_Params *pstDptx, uint8_t *pucPHY_Rate)
{
	uint32_t UiRegMap_PHY_IF_Ctrl, uiRate;

	UiRegMap_PHY_IF_Ctrl = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);
	uiRate = (UiRegMap_PHY_IF_Ctrl & DPTX_PHYIF_CTRL_RATE_MASK) >> DPTX_PHYIF_CTRL_RATE_SHIFT;

	*pucPHY_Rate = (uint8_t)uiRate;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Set_PHY_PreEmphasis(struct Dptx_Params *pstDptx, uint32_t uiLane_Index, enum PHY_PRE_EMPHASIS_LEVEL ePreEmphasisLevel)
{
	uint32_t uiRegMap_PhyTxEQ;

	if (pstDptx->ePhy_Dev != PHY_DEVICE_SNPS) {
		dptx_debug("Nothing to do not for Synopsys PHY");

		/* [FP] misra_c_2012_rule_15_1_violation*/
		return DPTX_RETURN_NO_ERROR;
	}

	if (uiLane_Index > (uint32_t)PHY_LANE_ID_3) {
		dptx_err("Invalid lane %d ", uiLane_Index);
		return DPTX_RETURN_EINVAL;
	}

	if (ePreEmphasisLevel > (uint32_t)PRE_EMPHASIS_LEVEL_3) {
		dptx_err("Invalid pre-emphasis level %d, using 3 ", ePreEmphasisLevel);
		ePreEmphasisLevel = PRE_EMPHASIS_LEVEL_3;
	}

	uiRegMap_PhyTxEQ = Dptx_Reg_Readl(pstDptx, DPTX_PHY_TX_EQ);
	uiRegMap_PhyTxEQ &= ~(DPTX_PHY_TX_EQ_PREEMP_MASK(uiLane_Index));
	uiRegMap_PhyTxEQ |= ((uint32_t)ePreEmphasisLevel << DPTX_PHY_TX_EQ_PREEMP_SHIFT(uiLane_Index)) & DPTX_PHY_TX_EQ_PREEMP_MASK(uiLane_Index);

	Dptx_Reg_Writel(pstDptx, DPTX_PHY_TX_EQ, uiRegMap_PhyTxEQ);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Set_PHY_VSW(struct Dptx_Params *pstDptx, uint32_t uiLane_Index, enum PHY_VOLTAGE_SWING_LEVEL eVoltageSwingLevel)
{
	uint32_t uiRegMap_PhyTxEQ;

	if (pstDptx->ePhy_Dev != PHY_DEVICE_SNPS) {
		dptx_debug("Nothing to do not for Synopsys PHY");

		/* [FP] misra_c_2012_rule_15_1_violation*/
		return DPTX_RETURN_NO_ERROR;
	}

	if (uiLane_Index > (uint32_t)PHY_LANE_ID_3) {
		dptx_err("Invalid lane %d ", uiLane_Index);
		return DPTX_RETURN_EINVAL;
	}

	if (eVoltageSwingLevel > VOLTAGE_SWING_LEVEL_3) {
		dptx_err("Invalid vswing level %d, using 3 ", eVoltageSwingLevel);
		eVoltageSwingLevel = VOLTAGE_SWING_LEVEL_3;
	}

	uiRegMap_PhyTxEQ = Dptx_Reg_Readl(pstDptx, DPTX_PHY_TX_EQ);
	uiRegMap_PhyTxEQ &= ~(DPTX_PHY_TX_EQ_VSWING_MASK(uiLane_Index));
	uiRegMap_PhyTxEQ |= ((uint32_t)eVoltageSwingLevel << DPTX_PHY_TX_EQ_VSWING_SHIFT(uiLane_Index)) & DPTX_PHY_TX_EQ_VSWING_MASK(uiLane_Index);

	Dptx_Reg_Writel(pstDptx, DPTX_PHY_TX_EQ, uiRegMap_PhyTxEQ);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Set_PHY_Pattern(struct Dptx_Params *pstDptx, uint32_t uiPattern)
{
	uint32_t	uiPhyTPSSelection = 0;

	uiPhyTPSSelection = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);
	uiPhyTPSSelection &= ~DPTX_PHYIF_CTRL_TPS_SEL_MASK;
	uiPhyTPSSelection |= ((uiPattern << DPTX_PHYIF_CTRL_TPS_SEL_SHIFT) & DPTX_PHYIF_CTRL_TPS_SEL_MASK);

	Dptx_Reg_Writel(pstDptx, DPTX_PHYIF_CTRL, uiPhyTPSSelection);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Enable_PHY_XMIT(struct Dptx_Params *pstDptx, uint32_t iNumOfLanes)
{
	uint32_t	uiRegMap_PhyIfCtrl, uiBitMask = 0;

	uiRegMap_PhyIfCtrl = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);

	switch (iNumOfLanes) {
	case PHY_NUM_OF_4_LANE:
		uiBitMask |= (uint32_t)DPTX_PHYIF_CTRL_XMIT_EN(3);
		uiBitMask |= (uint32_t)DPTX_PHYIF_CTRL_XMIT_EN(2);
		uiBitMask |= (uint32_t)DPTX_PHYIF_CTRL_XMIT_EN(1);
		uiBitMask |= (uint32_t)DPTX_PHYIF_CTRL_XMIT_EN(0);
		break;
	case PHY_NUM_OF_2_LANE:
		uiBitMask |= (uint32_t)DPTX_PHYIF_CTRL_XMIT_EN(1);
		uiBitMask |= (uint32_t)DPTX_PHYIF_CTRL_XMIT_EN(0);
		break;
	case PHY_NUM_OF_1_LANE:
		uiBitMask |= (uint32_t)DPTX_PHYIF_CTRL_XMIT_EN(0);
		break;
	default:
		dptx_err("Invalid number of lanes %d", (uint32_t)iNumOfLanes);
		return DPTX_RETURN_EINVAL;
	}

	uiRegMap_PhyIfCtrl |= uiBitMask;

	Dptx_Reg_Writel(pstDptx, DPTX_PHYIF_CTRL, uiRegMap_PhyIfCtrl);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Disable_PHY_XMIT(struct Dptx_Params *pstDptx, uint32_t iNumOfLanes)
{
	uint32_t	uiRegMap_PhyIfCtrl, uiBitMask = 0;

	uiRegMap_PhyIfCtrl = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);

	switch (iNumOfLanes) {
	case PHY_NUM_OF_4_LANE:
		uiBitMask |= DPTX_PHYIF_CTRL_XMIT_EN(3);
		uiBitMask |= DPTX_PHYIF_CTRL_XMIT_EN(2);
		uiBitMask |= DPTX_PHYIF_CTRL_XMIT_EN(1);
		uiBitMask |= DPTX_PHYIF_CTRL_XMIT_EN(0);
		break;
	case PHY_NUM_OF_2_LANE:
		uiBitMask |= DPTX_PHYIF_CTRL_XMIT_EN(1);
		uiBitMask |= DPTX_PHYIF_CTRL_XMIT_EN(0);
		break;
	case PHY_NUM_OF_1_LANE:
		uiBitMask |= DPTX_PHYIF_CTRL_XMIT_EN(0);
		break;
	default:
		dptx_err("Invalid number of lanes %d", (uint32_t)iNumOfLanes);
		return DPTX_RETURN_EINVAL;
	}

	uiRegMap_PhyIfCtrl &= ~uiBitMask;

	Dptx_Reg_Writel(pstDptx, DPTX_PHYIF_CTRL, uiRegMap_PhyIfCtrl);

	return DPTX_RETURN_NO_ERROR;
}

