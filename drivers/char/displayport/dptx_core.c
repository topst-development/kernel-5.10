// SPDX-License-Identifier: GPL-2.0-or-later OR MIT
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

enum dptx_reset_target {
	DPTX_RESET_COREA_ALL = 0,
	DPTX_RESET_PHY,
	DPTX_RESET_HDCP,
};

static int32_t dptx_core_check_vendor_id(struct Dptx_Params *pstDptx)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint32_t dptx_chip_id;

	dptx_chip_id = Dptx_Reg_Readl(pstDptx, DPTX_ID);
	if (dptx_chip_id != (uint32_t)((DPTX_ID_DEVICE_ID << DPTX_ID_DEVICE_ID_SHIFT) | DPTX_ID_VENDOR_ID)) {
		dptx_err("Invalid DPTX Id : 0x%x<->0x%x ",
			  dptx_chip_id,
			  ((DPTX_ID_DEVICE_ID << DPTX_ID_DEVICE_ID_SHIFT) | DPTX_ID_VENDOR_ID));
		ret = -DPTX_RETURN_ENODEV;
	}
	return ret;
}

static void dptx_core_set_phy_width(struct Dptx_Params *pstDptx, uint32_t phy_data_width)
{
	uint32_t reg_val;

	reg_val = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);

	reg_val &= ~DPTX_PHYIF_CTRL_WIDTH_MASK;
	reg_val |= ((phy_data_width << DPTX_PHYIF_CTRL_WIDTH_SHIFT) & DPTX_PHYIF_CTRL_WIDTH_MASK);

	Dptx_Reg_Writel(pstDptx, DPTX_PHYIF_CTRL, reg_val);
}

int32_t Dptx_Core_Init_Params(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	(void)Dptx_Core_Get_PHY_NumOfLanes(pstDptx, &pstDptx->stDptxLink.ucNumOfLanes);
	(void)Dptx_Core_Get_PHY_Rate(pstDptx, &pstDptx->stDptxLink.ucLinkRate);

	(void)Dptx_Core_Get_Stream_Mode(pstDptx, &pstDptx->bMultStreamTransport);

	(void)Dptx_Core_Get_PHY_SSC(pstDptx, &pstDptx->bSpreadSpectrum_Clock);

	return iRetVal;
}

/**
 * @brief Initializes the DPTX core with optional reset targets.
 *
 * This function initializes the DisplayPort Transmitter (DPTX) core. The function allows
 * resetting the entire core, just the PHY depending on the `reset_target` parameter.
 *
 * @param[in] pstDptx Pointer to the DPTX driver context structure.
 * @param[in] reset_target Specifies the reset target:
 *                         - 0: Resets the entire core including PHY.
 *                         - 1: Resets only the PHY.
 *
 * @return int32_t Returns 0 on success, or an error code on failure.
 */
static int32_t dptx_core_init_internal(struct Dptx_Params *pstDptx, enum dptx_reset_target reset_target)
{
	uint32_t phy_data_width = PHY_DATA_WIDTH_20BITS;
	uint32_t soft_reset_val = DPTX_SRST_CTRL_ALL;
	uint32_t interrupt_bits = DPTX_IEN_ALL_INTR;
	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint32_t reg_val;

	ret = dptx_core_check_vendor_id(pstDptx);
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (reset_target == DPTX_RESET_PHY) {
			/*  If the condition is to reset only the PHY */
			soft_reset_val = DPTX_SRST_CTRL_PHY;

			interrupt_bits = DPTX_IEN_HPD;
		}
		dptx_core_disable_global_intr(pstDptx, interrupt_bits);

		Dptx_Core_Soft_Reset(pstDptx, soft_reset_val);

		#if defined(DEBUG_DPTX_CHIP_ID)
		if (reset_target == DPTX_RESET_COREA_ALL) {
			/*  If the condition is to reset the entire core, including PHY */
			char dptx_chip_ver[15];
			uint32_t dptx_version;

			memset(dptx_chip_ver, 0, sizeof(dptx_chip_ver));

			dptx_version = Dptx_Reg_Readl(pstDptx, DPTX_VER_NUMBER);
			dptx_chip_ver[0] = (dptx_version >> 24) & 0xff;
			dptx_chip_ver[1] = '.';
			dptx_chip_ver[2] = (dptx_version >> 16) & 0xff;
			dptx_chip_ver[3] = (dptx_version >> 8) & 0xff;
			dptx_chip_ver[4] = (dptx_version & 0xff);

			dptx_version = Dptx_Reg_Readl(pstDptx, DPTX_VER_TYPE);
			dptx_chip_ver[5] = '-';
			dptx_chip_ver[6] = (dptx_version >> 24) & 0xff;
			dptx_chip_ver[7] = (dptx_version >> 16) & 0xff;
			dptx_chip_ver[8] = (dptx_version >> 8) & 0xff;
			dptx_chip_ver[9] = (dptx_version & 0xff);

			dptx_debug("Core version: %s ", dptx_chip_ver);
		}
		#endif

		if (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) {
			/* For KCS */
			phy_data_width = PHY_DATA_WIDTH_40BITS;
		}
		dptx_core_set_phy_width(pstDptx, phy_data_width);

		if (reset_target == DPTX_RESET_COREA_ALL) {
			/*  If the condition is to reset the entire core, including PHY */
			reg_val = Dptx_Reg_Readl(pstDptx, DPTX_HPD_IEN);
			reg_val |= (DPTX_HPD_IEN_IRQ_EN | DPTX_HPD_IEN_HOT_PLUG_EN |
					DPTX_HPD_IEN_HOT_UNPLUG_EN | DPTX_HPDSTS_UNPLUG_ERR_EN);
			Dptx_Reg_Writel(pstDptx, DPTX_HPD_IEN, reg_val);

			reg_val = Dptx_Reg_Readl(pstDptx, DPTX_HDCP_API_INT_MSK);
			reg_val |= DPTX_HDCP22_GPIOINT;
			Dptx_Reg_Writel(pstDptx, DPTX_HDCP_API_INT_MSK, reg_val);
			reg_val = Dptx_Reg_Readl(pstDptx, DPTX_TYPE_C_CTRL);
			reg_val &= ~(DPTX_TYPEC_DISABLE_ACK);
			reg_val &= ~(DPTX_TYPEC_DISABLE_STATUS);
			reg_val |= DPTX_TYPEC_INTRURPPT_STATUS;
			Dptx_Reg_Writel(pstDptx, DPTX_TYPE_C_CTRL, reg_val);

			reg_val = Dptx_Reg_Readl(pstDptx, DPTX_CCTL);
			reg_val |= DPTX_CCTL_ENH_FRAME_EN;
			reg_val &= ~DPTX_CCTL_SCALE_DOWN_MODE;
			reg_val &= ~DPTX_CCTL_FAST_LINK_TRAINED_EN;
			Dptx_Reg_Writel(pstDptx, DPTX_CCTL, reg_val);

			/*
			 * Disable Audio data input to prevent Audio FIFO Overflow
			 */
			reg_val = Dptx_Reg_Readl(pstDptx, DPTX_AUD_CONFIG1);
			reg_val &= ~DPTX_AUD_CONFIG1_DATA_EN_IN_MASK;
			Dptx_Reg_Writel(pstDptx, DPTX_AUD_CONFIG1, reg_val);

		}
		if (reset_target == DPTX_RESET_PHY) {
			/* Enable DisplayPort HPD Interrupts */
			dptx_core_enable_global_intr(pstDptx, interrupt_bits);
		}
	}

	return ret;
}

void dptx_core_enable_global_intr(struct Dptx_Params *pstDptx, uint32_t interrupt_enable_bits)
{
	uint32_t reg_val;

	interrupt_enable_bits &= ~(DPTX_IEN_AUX_REPLY | DPTX_IEN_AUX_CMD_INVALID |
				   DPTX_IEN_VIDEO_FIFO_OVERFLOW);

	Dptx_Reg_Writel(pstDptx, DPTX_HPDSTS,
			DPTX_HPDSTS_IRQ |
			DPTX_HPDSTS_HOT_PLUG |
			DPTX_HPDSTS_HOT_UNPLUG);
	Dptx_Reg_Writel(pstDptx, DPTX_ISTS, DPTX_ISTS_ALL_INTR);

	reg_val = Dptx_Reg_Readl(pstDptx, DPTX_IEN);
	reg_val |= interrupt_enable_bits;
	Dptx_Reg_Writel(pstDptx, DPTX_IEN, reg_val);
}

void dptx_core_disable_global_intr(struct Dptx_Params *pstDptx, uint32_t interrupt_disable_bits)
{
	uint32_t reg_val;

	interrupt_disable_bits |= (DPTX_IEN_AUX_REPLY | DPTX_IEN_AUX_CMD_INVALID |
				   DPTX_IEN_VIDEO_FIFO_OVERFLOW);
	reg_val = Dptx_Reg_Readl(pstDptx, DPTX_IEN);
	reg_val &= ~interrupt_disable_bits;
	Dptx_Reg_Writel(pstDptx, DPTX_IEN, reg_val);

	if (interrupt_disable_bits == DPTX_IEN_ALL_INTR) {
		/* Clean HPD interrupt and global interrupts */
		Dptx_Reg_Writel(pstDptx, DPTX_HPDSTS,
				DPTX_HPDSTS_IRQ |
				DPTX_HPDSTS_HOT_PLUG |
				DPTX_HPDSTS_HOT_UNPLUG);
	}
	Dptx_Core_Clear_General_Interrupt(pstDptx, interrupt_disable_bits);
}

/**
 * @brief Initializes the DisplayPort Core and other necessary components for DisplayPort usage.
 *
 * This function is responsible for initializing the DisplayPort Core along with other necessary
 * components to enable the usage of DisplayPort. It sets up the required parameters and
 * configurations to prepare the DisplayPort for operation.
 *
 * @param[in] pstDptx Pointer to the DPTX driver context structure.
 *
 * @return int32_t Returns 0 on successful initialization, or an error code on failure.
 */
int32_t Dptx_Core_Init(struct Dptx_Params *pstDptx)
{
	return dptx_core_init_internal(pstDptx, DPTX_RESET_COREA_ALL);
}

/**
 * @brief Initializes only the DisplayPort PHY for DisplayPort usage.
 *
 * This function is responsible for initializing the PHY (Physical Layer) of the DisplayPort.
 * It configures and prepares the PHY layer for proper operation, ensuring that the physical transmission
 * of data over the DisplayPort interface is correctly set up.
 *
 * @param[in] pstDptx Pointer to the DPTX driver context structure.
 *
 * @return int32_t Returns 0 on successful initialization, or an error code on failure.
 */
int32_t dptx_core_phy_init(struct Dptx_Params *pstDptx)
{
	return dptx_core_init_internal(pstDptx, DPTX_RESET_PHY);
}

int32_t Dptx_Core_Deinit(struct Dptx_Params *pstDptx)
{
	dptx_core_disable_global_intr(pstDptx, DPTX_IEN_ALL_INTR);
	Dptx_Core_Soft_Reset(pstDptx, DPTX_SRST_CTRL_ALL);

	return DPTX_RETURN_NO_ERROR;
}

void Dptx_Core_Soft_Reset(struct Dptx_Params *pstDptx, uint32_t uiReset_Bits)
{
	uint32_t uiRegMap_Reset, uiRegMap_BitMask;

	uiRegMap_BitMask = (uiReset_Bits & DPTX_SRST_CTRL_ALL);

	uiRegMap_Reset = Dptx_Reg_Readl(pstDptx, DPTX_SRST_CTRL);
	uiRegMap_Reset |= uiRegMap_BitMask;
	Dptx_Reg_Writel(pstDptx, DPTX_SRST_CTRL, uiRegMap_Reset);

	udelay(20);

	uiRegMap_Reset = Dptx_Reg_Readl(pstDptx, DPTX_SRST_CTRL);
	uiRegMap_Reset &= ~uiRegMap_BitMask;
	Dptx_Reg_Writel(pstDptx, DPTX_SRST_CTRL, uiRegMap_Reset);
}



int32_t Dptx_Core_Clear_General_Interrupt(struct Dptx_Params *pstDptx, uint32_t uiClear_Bits)
{
	uint32_t ucRegMap_GeneralIntr;

	ucRegMap_GeneralIntr = uiClear_Bits;
	Dptx_Reg_Writel(pstDptx, DPTX_ISTS, ucRegMap_GeneralIntr);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Set_PHY_SSC(struct Dptx_Params *pstDptx)
{
	uint32_t reg_val = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);

	reg_val &= ~DPTX_PHYIF_CTRL_SSC_DIS;

	if (!pstDptx->bSpreadSpectrum_Clock ||
	    (pstDptx->aucDPCD_Caps[DP_MAX_DOWNSPREAD] & SINK_TDOWNSPREAD_MASK) == 0u) {
		reg_val |= DPTX_PHYIF_CTRL_SSC_DIS;
	}
	Dptx_Reg_Writel(pstDptx, DPTX_PHYIF_CTRL, reg_val);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Core_Get_PHY_SSC(struct Dptx_Params *pstDptx, bool *pbSSC_Enabled)
{
	uint32_t uiRegMap_PhyIfCtrl;

	uiRegMap_PhyIfCtrl = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);

	if (uiRegMap_PhyIfCtrl & DPTX_PHYIF_CTRL_SSC_DIS) {
		/* For KCS */
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
		/* For KCS */
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
			/* For KCS */
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

int32_t dptx_core_set_phy_lane_sigan_quality(struct Dptx_Params *dev_param,
					     uint32_t lane_idx, uint8_t link_rate,
					     enum PHY_VOLTAGE_SWING_LEVEL voltage_swing,
					     enum PHY_PRE_EMPHASIS_LEVEL pre_emphasis)
{
	int32_t ret =  DPTX_RETURN_NO_ERROR;

	if (dev_param->hw_config.phy_eq_manual_mode) {
		ret = dptx_cfg_set_manual_phy_signal_quality(dev_param, link_rate, voltage_swing, pre_emphasis);
		if (DPTX_RETURN_SUCCESS(ret)) {
			/* For KCS */
			ret = dptx_sec_set_phy_sigan_quality(dev_param, lane_idx, link_rate, voltage_swing, pre_emphasis);
		}
	} else {
		ret = Dptx_Core_Set_PHY_PreEmphasis(dev_param, lane_idx, pre_emphasis);
		if (DPTX_RETURN_SUCCESS(ret)) {
			/* For KCS */
			ret = Dptx_Core_Set_PHY_VSW(dev_param, lane_idx, voltage_swing);
		}
	}
	return ret;
}

int32_t dptx_core_set_phy_sigan_quality(struct Dptx_Params *dptx_param)
{
	uint32_t lane_idx;
	int32_t ret = DPTX_RETURN_NO_ERROR;

	for (lane_idx = 0u; lane_idx < dptx_param->stDptxLink.ucNumOfLanes; lane_idx++) {
		ret = dptx_core_set_phy_lane_sigan_quality(dptx_param, lane_idx,
							   dptx_param->stDptxLink.ucLinkRate,
							   dptx_param->stDptxLink.aucVoltageSwing_level[lane_idx],
							   dptx_param->stDptxLink.aucPreEmphasis_level[lane_idx]);
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			break;
		}
	}
	return ret;
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

int32_t Dptx_Core_Disable_PHY_XMIT(struct Dptx_Params *pstDptx, uint32_t num_of_lanes)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint32_t reg_val;

	if (num_of_lanes > PHY_NUM_OF_4_LANE) {
		dptx_err("Invalid number of lanes %d", num_of_lanes);
		ret = DPTX_RETURN_EINVAL;
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		reg_val = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);
		reg_val &= ~DPTX_PHYIF_CTRL_XMIT_EN_MASK;
		Dptx_Reg_Writel(pstDptx, DPTX_PHYIF_CTRL, reg_val);
	}

	return ret;
}

uint32_t dptx_core_get_phy_xmit(struct Dptx_Params *pstDptx)
{
	uint32_t reg_val;

	reg_val = Dptx_Reg_Readl(pstDptx, DPTX_PHYIF_CTRL);

	reg_val = (reg_val & DPTX_PHYIF_CTRL_XMIT_EN_MASK) >> DPTX_PHYIF_CTRL_XMIT_EN_SHIFT;

	return reg_val;
}

void dptx_core_set_default_phy_eq(struct Dptx_Params *dev_param)
{
	uint32_t default_main_eq[16] = {0, };
	uint32_t default_post_eq[16] = {0, };

	dptx_cfg_get_default_phy_eq(dev_param, default_main_eq, default_post_eq);
	dptx_sec_get_default_phy_eq(dev_param, default_main_eq, default_post_eq);

	(void)memcpy(dev_param->hw_config.phy_eq[0].main_eq, default_main_eq, sizeof(uint32_t) * 16u);
	(void)memcpy(dev_param->hw_config.phy_eq[0].post_eq, default_post_eq, sizeof(uint32_t) * 16u);
	(void)memcpy(dev_param->hw_config.phy_eq[1].main_eq, default_main_eq, sizeof(uint32_t) * 16u);
	(void)memcpy(dev_param->hw_config.phy_eq[1].post_eq, default_post_eq, sizeof(uint32_t) * 16u);
	(void)memcpy(dev_param->hw_config.phy_eq[2].main_eq, default_main_eq, sizeof(uint32_t) * 16u);
	(void)memcpy(dev_param->hw_config.phy_eq[2].post_eq, default_post_eq, sizeof(uint32_t) * 16u);
	(void)memcpy(dev_param->hw_config.phy_eq[3].main_eq, default_main_eq, sizeof(uint32_t) * 16u);
	(void)memcpy(dev_param->hw_config.phy_eq[3].post_eq, default_post_eq, sizeof(uint32_t) * 16u);
}

