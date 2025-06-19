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


#define MAX_TRY_CLOCK_RECOVERY					5		/* Vesa Spec. Figure 3-20 */
#define MAX_TRY_CHANNEL_EQ						5		/* Vesa Spec. Figure 3-21 */
#define MAX_TRY_SINK_LINK_STATUS_UPDATE			10		/* Vesa Spec. Figure 3-20 */
#define MAX_TRY_SINK_UPDATE_STATUS				100

#define DP_TRAIN_VOLTAGE_LEVEL_MASK				0x03
#define DP_TRAIN_PRE_EMPHASIS_LEVEL_MASK		0x0C

#define DP_TRAIN_CLOCK_RECOVERY					true
#define DP_TRAIN_CHANNEL_EQ						false



static int32_t dptx_link_adjust_levels(struct Dptx_Params *pstDptx)
{
	uint8_t ucDPCD_Adjust_Request_LaneX, ucLane_Index;
	uint8_t aucDPCD_Adjusted_Value[DPTX_MAX_LINK_LANES] = { 0, };
	uint8_t aucSink_ConfigValues[DPTX_MAX_LINK_LANES] = { 0, };
	int32_t iRetVal;

	switch (pstDptx->stDptxLink.ucNumOfLanes) {
	case (uint8_t)PHY_LANE_4:
		iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_ADJUST_REQUEST_LANE2_3, &ucDPCD_Adjust_Request_LaneX);
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}

		aucDPCD_Adjusted_Value[2] = (ucDPCD_Adjust_Request_LaneX & 0x0F);
		aucDPCD_Adjusted_Value[3] = ((ucDPCD_Adjust_Request_LaneX & 0xF0) >> 4);

		iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_ADJUST_REQUEST_LANE0_1, &ucDPCD_Adjust_Request_LaneX);
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}

		aucDPCD_Adjusted_Value[0] = (ucDPCD_Adjust_Request_LaneX & 0x0F);
		aucDPCD_Adjusted_Value[1] = ((ucDPCD_Adjust_Request_LaneX & 0xF0) >> 4);

		dptx_dbg("LANE0(Vsw %d, PreEm %d), LANE1(Vsw %d, PreEm %d), LANE2(Vsw %d, PreEm %d), LANE3(Vsw %d, PreEm %d)",
				(aucDPCD_Adjusted_Value[0] & 0x03), (aucDPCD_Adjusted_Value[0] & 0x0C) >> 2,
				(aucDPCD_Adjusted_Value[1] & 0x03), (aucDPCD_Adjusted_Value[1] & 0x0C) >> 2,
				(aucDPCD_Adjusted_Value[2] & 0x03), (aucDPCD_Adjusted_Value[2] & 0x0C) >> 2,
				(aucDPCD_Adjusted_Value[3] & 0x03), (aucDPCD_Adjusted_Value[3] & 0x0C) >> 2);
		break;
	case (uint8_t)PHY_LANE_2:
	case (uint8_t)PHY_LANE_1:
		iRetVal = Dptx_Aux_Read_DPCD(pstDptx, DP_ADJUST_REQUEST_LANE0_1, &ucDPCD_Adjust_Request_LaneX);
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}

		aucDPCD_Adjusted_Value[0] = (ucDPCD_Adjust_Request_LaneX & 0x0F);
		aucDPCD_Adjusted_Value[1] = ((ucDPCD_Adjust_Request_LaneX & 0xF0) >> 4);

		dptx_dbg("LANE0(Vsw %d, PreEm %d), LANE1(Vsw %d, PreEm %d)",
					(aucDPCD_Adjusted_Value[0] & 0x03), (aucDPCD_Adjusted_Value[0] & 0x0C) >> 2,
					(aucDPCD_Adjusted_Value[1] & 0x03), (aucDPCD_Adjusted_Value[1] & 0x0C) >> 2);
		break;
	default:
		dptx_err("Invalid number of lanes %d ", pstDptx->stDptxLink.ucNumOfLanes);
		return DPTX_RETURN_EINVAL;
	}

	for (ucLane_Index = 0; ucLane_Index < pstDptx->stDptxLink.ucNumOfLanes; ucLane_Index++) {
		pstDptx->stDptxLink.aucVoltageSwing_level[ucLane_Index] = (aucDPCD_Adjusted_Value[ucLane_Index] & DP_TRAIN_VOLTAGE_LEVEL_MASK);
		pstDptx->stDptxLink.aucPreEmphasis_level[ucLane_Index] = ((aucDPCD_Adjusted_Value[ucLane_Index] & DP_TRAIN_PRE_EMPHASIS_LEVEL_MASK) >> 2);

		iRetVal = Dptx_Sec_Set_EQ(pstDptx,
									ucLane_Index,
									pstDptx->stDptxLink.ucLinkRate,
									pstDptx->stDptxLink.aucVoltageSwing_level[ucLane_Index],
									pstDptx->stDptxLink.aucPreEmphasis_level[ucLane_Index]);
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}

		iRetVal = Dptx_Core_Set_PHY_PreEmphasis(pstDptx, ucLane_Index, (enum PHY_PRE_EMPHASIS_LEVEL)pstDptx->stDptxLink.aucPreEmphasis_level[ucLane_Index]);
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}

		iRetVal = Dptx_Core_Set_PHY_VSW(pstDptx, ucLane_Index, (enum PHY_VOLTAGE_SWING_LEVEL)pstDptx->stDptxLink.aucVoltageSwing_level[ucLane_Index]);
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}

		aucSink_ConfigValues[ucLane_Index] = 0;

		aucSink_ConfigValues[ucLane_Index] |= ((pstDptx->stDptxLink.aucVoltageSwing_level[ucLane_Index] << DP_TRAIN_VOLTAGE_SWING_SHIFT) & DP_TRAIN_VOLTAGE_SWING_MASK);
		if (pstDptx->stDptxLink.aucVoltageSwing_level[ucLane_Index] >= DP_TRAIN_VOLTAGE_SWING_LEVEL_3) {
			dptx_dbg("Lane %d VSW reached to level 3", ucLane_Index);
			aucSink_ConfigValues[ucLane_Index] |= DP_TRAIN_MAX_SWING_REACHED;
		}

		aucSink_ConfigValues[ucLane_Index] |= ((pstDptx->stDptxLink.aucPreEmphasis_level[ucLane_Index] <<  DP_TRAIN_PRE_EMPHASIS_SHIFT) & DP_TRAIN_PRE_EMPHASIS_MASK);
		if (pstDptx->stDptxLink.aucPreEmphasis_level[ucLane_Index] == (DP_TRAIN_PRE_EMPH_LEVEL_3 >> 3)) {
			dptx_dbg("Lane %d Pre-emphasis reached to level 3", ucLane_Index);
			aucSink_ConfigValues[ucLane_Index] |= DP_TRAIN_MAX_PRE_EMPHASIS_REACHED;
		}
	}

	iRetVal = Dptx_Aux_Write_Bytes_To_DPCD(pstDptx, DP_TRAINING_LANE0_SET, aucSink_ConfigValues, pstDptx->stDptxLink.ucNumOfLanes);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	return DPTX_RETURN_NO_ERROR;
}

static int32_t dptx_link_reduce_rate(struct Dptx_Params *pstDptx)
{
	uint8_t ucLinkRate = (uint8_t)DPTX_PHYIF_CTRL_RATE_RBR;
	int32_t ret = DPTX_RETURN_NO_ERROR;

	switch (pstDptx->stDptxLink.ucLinkRate) {
	case DPTX_PHYIF_CTRL_RATE_HBR3:
		ucLinkRate = DPTX_PHYIF_CTRL_RATE_HBR2;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR2:
		ucLinkRate = DPTX_PHYIF_CTRL_RATE_HBR;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR:
		ucLinkRate = DPTX_PHYIF_CTRL_RATE_RBR;
		break;
	case DPTX_PHYIF_CTRL_RATE_RBR:
		dptx_warn("Rate is reached to RBR ");
		dptx_err("Rate is reached to RBR ");
		ret = -DPTX_RETURN_EINVAL;
		break;
	default:
		ucLinkRate = DPTX_PHYIF_CTRL_RATE_RBR;
		dptx_err("Invalid PHY rate %d set RBR as defult\n", pstDptx->stDptxLink.ucLinkRate);
		ret = -DPTX_RETURN_EINVAL;
		break;
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		//dptx_info(" Reducing rate from %s to %s",
		dptx_err(" Reducing rate from %s to %s",
			  pstDptx->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_RBR ? "RBR" :
			  (pstDptx->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR) ?  "HBR" :
			  (pstDptx->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR2) ? "HB2":"HBR3",
			  (ucLinkRate == DPTX_PHYIF_CTRL_RATE_RBR) ? "RBR" :
			  (ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR) ?  "HBR" :
			  (ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR2) ? "HB2":"HBR3");
		pstDptx->stDptxLink.ucLinkRate = ucLinkRate;
	}
	return ret;
}

static int32_t dptx_link_reduce_lanes(struct Dptx_Params *pstDptx)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint8_t new_link_lanes;

	switch(pstDptx->stDptxLink.ucNumOfLanes) {
	case (uint8_t)PHY_LANE_4:
		new_link_lanes = (uint8_t)PHY_LANE_2;
		break;
	case (uint8_t)PHY_LANE_2:
		new_link_lanes = (uint8_t)PHY_LANE_1;
		break;
	case (uint8_t)PHY_LANE_1:
		dptx_warn("The number of lanes is reached to 1");
		ret = -DPTX_RETURN_EINVAL;
		break;
	default:
		new_link_lanes = (uint8_t)PHY_LANE_1;
		dptx_err("Invalid number of lanes %d set LANE1 as defult\n", pstDptx->stDptxLink.ucNumOfLanes);
		ret = -DPTX_RETURN_EINVAL;
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		dptx_err("Reducing lanes from %d to %d", pstDptx->stDptxLink.ucNumOfLanes, new_link_lanes);
		pstDptx->stDptxLink.ucNumOfLanes = new_link_lanes;
	}

	return ret;
}

static int32_t dptx_set_link_train(struct Dptx_Params *dptx_param, uint8_t dp_train_pat)
{
	uint8_t dp_train_pat_msg[5] = {0, };
	uint32_t dp_train_pat_len, index;
	uint32_t dp_phy_pattern;
	int32_t ret;

	switch (dp_train_pat & DP_TRAINING_PATTERN_MASK_1_4) {
	case DP_TRAINING_PATTERN_1:
		dp_phy_pattern = DPTX_PHYIF_CTRL_TPS_1;
		dp_train_pat |= DP_LINK_SCRAMBLING_DISABLE;
		break;
	case DP_TRAINING_PATTERN_2:
		dp_phy_pattern = DPTX_PHYIF_CTRL_TPS_2;
		dp_train_pat |= DP_LINK_SCRAMBLING_DISABLE;
		break;
	case DP_TRAINING_PATTERN_3:
		dp_phy_pattern = DPTX_PHYIF_CTRL_TPS_3;
		dp_train_pat |= DP_LINK_SCRAMBLING_DISABLE;
		break;
	case DP_TRAINING_PATTERN_4:
		dp_phy_pattern = DPTX_PHYIF_CTRL_TPS_4;
		dp_train_pat &= ~DP_LINK_SCRAMBLING_DISABLE;
		break;
	case DP_TRAINING_PATTERN_DISABLE:
		dp_phy_pattern = DPTX_PHYIF_CTRL_TPS_NONE;
		dp_train_pat &= ~DP_LINK_SCRAMBLING_DISABLE;
		break;
	default:
		dp_phy_pattern = DPTX_PHYIF_CTRL_TPS_NONE;
		dp_train_pat &= ~DP_LINK_SCRAMBLING_DISABLE;
		break;
	}

	ret = Dptx_Core_Set_PHY_Pattern(dptx_param, dp_phy_pattern);
	if (ret == DPTX_RETURN_NO_ERROR) {
		ret = Dptx_Core_Enable_PHY_XMIT(dptx_param, dptx_param->stDptxLink.ucNumOfLanes);
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		dp_train_pat_msg[0] = dp_train_pat;
		dp_train_pat_len = 1;

		if ((dp_train_pat & DP_TRAINING_PATTERN_MASK) != DP_TRAINING_PATTERN_DISABLE) {
			/* DP_TRAINING_LANEx_SET follow DP_TRAINING_PATTERN_SET */
			for (index = 0u; index < dptx_param->stDptxLink.ucNumOfLanes; index++) {
				dp_train_pat_msg[index + 1] =
					dptx_param->stDptxLink.aucVoltageSwing_level[index] |
					(dptx_param->stDptxLink.aucPreEmphasis_level[index] << DP_TRAIN_PRE_EMPHASIS_SHIFT);
				dp_train_pat_len++;
			}
		}
		ret = Dptx_Aux_Write_Bytes_To_DPCD(dptx_param, DP_TRAINING_PATTERN_SET,
						   dp_train_pat_msg, dp_train_pat_len);
		if (ret != DPTX_RETURN_NO_ERROR) {
			dptx_err("Failed DP_TRAINING_PATTERN_SET");
		}
	}
	return ret;
}

static int32_t dptx_set_signal_levels(struct Dptx_Params *dptx_param)
{
	uint32_t index;
	int32_t ret = DPTX_RETURN_NO_ERROR;

	for (index = 0u; index < dptx_param->stDptxLink.ucNumOfLanes; index++) {
		/* TCC807x+ */
		ret = Dptx_Sec_Set_EQ(dptx_param, index,
				      dptx_param->stDptxLink.ucLinkRate,
				      dptx_param->stDptxLink.aucVoltageSwing_level[index],
				      dptx_param->stDptxLink.aucPreEmphasis_level[index]);
		/* TCC807x- */
		/* TCC805x+ */
		if (ret == DPTX_RETURN_NO_ERROR) {
			ret = Dptx_Core_Set_PHY_PreEmphasis(dptx_param,
							    index,
							    (enum PHY_PRE_EMPHASIS_LEVEL)dptx_param->stDptxLink.aucPreEmphasis_level[index]);
		}
		if (ret == DPTX_RETURN_NO_ERROR) {
			ret = Dptx_Core_Set_PHY_VSW(dptx_param, index,
						    (enum PHY_VOLTAGE_SWING_LEVEL)dptx_param->stDptxLink.aucVoltageSwing_level[index]);
		}
		/* TCC805x- */
		if (ret != DPTX_RETURN_NO_ERROR) {
			break;
		}
	}
	return ret;
}

static void dptx_stop_link_train(struct Dptx_Params *dptx_param)
{
	dptx_set_link_train(dptx_param, DP_TRAINING_PATTERN_DISABLE);
}

static int32_t dptx_reset_link_train(struct Dptx_Params *dptx_param, uint8_t dp_train_pat)
{
	memset(dptx_param->stDptxLink.aucPreEmphasis_level, 0, sizeof(uint8_t) * PRE_EMPHASIS_LEVEL_MAX);
	memset(dptx_param->stDptxLink.aucVoltageSwing_level, 0, sizeof(uint8_t) * VOLTAGE_SWING_LEVEL_MAX);
	dptx_set_signal_levels(dptx_param);
	return dptx_set_link_train(dptx_param, dp_train_pat);
}

static int32_t dptx_link_perform_clock_recovery(struct Dptx_Params *pstDptx)
{

	uint32_t max_cr_tries, cr_tries, voltage_tries;
	bool max_vswing_reached = (bool)false;
	bool recovery_ok = (bool)false;
	uint8_t voltage, dpcd_val;
	int32_t ret;

	/* 6. Set the following DPCD registers on the Sink */
	ret = Dptx_Link_PHY_Rate_To_Bandwidth(pstDptx, pstDptx->stDptxLink.ucLinkRate, &dpcd_val);
	if (ret == DPTX_RETURN_NO_ERROR) {
		/* 6.a LINK_BW_SET */
		ret = Dptx_Aux_Write_DPCD(pstDptx, DP_LINK_BW_SET, dpcd_val);
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		dpcd_val = pstDptx->stDptxLink.ucNumOfLanes;
		if (Drm_dp_enhanced_frame_cap(pstDptx->aucDPCD_Caps)) {
			dpcd_val |= DP_ENHANCED_FRAME_CAP;
		}
		/* 6.b LANE_COUNT_SE */
		ret = Dptx_Aux_Write_DPCD(pstDptx, DP_LANE_COUNT_SET, dpcd_val);
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		uint32_t cctl_reg;

		/* 8.h */
		cctl_reg = Dptx_Reg_Readl(pstDptx, DPTX_CCTL);
		if (Drm_dp_enhanced_frame_cap(pstDptx->aucDPCD_Caps)) {
			cctl_reg |= DPTX_CCTL_ENH_FRAME_EN;
		} else {
			cctl_reg &= ~DPTX_CCTL_ENH_FRAME_EN;

		}
		Dptx_Reg_Writel(pstDptx, DPTX_CCTL, cctl_reg);
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		if (pstDptx->bSpreadSpectrum_Clock &&
		(pstDptx->aucDPCD_Caps[DP_MAX_DOWNSPREAD] & SINK_TDOWNSPREAD_MASK)) {
			dpcd_val = DP_SPREAD_AMP_0_5;
		} else {
			dpcd_val = 0;
		}

		/* 6.c DOWNSPREAD_CTRL */
		ret = Dptx_Aux_Write_DPCD(pstDptx, DP_DOWNSPREAD_CTRL, dpcd_val);
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		/* 6.d MAIN_LINK_CHANNEL_CODING_SET */
		dpcd_val = DP_SET_ANSI_8B10B;
		ret = Dptx_Aux_Write_DPCD(pstDptx, DP_MAIN_LINK_CHANNEL_CODING_SET, dpcd_val);
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		dptx_dbg("Training clock_recovery with Setting to TPS 1 in PHY and Pattern Sequence 1 in Sink...");
		ret = dptx_reset_link_train(pstDptx, DP_TRAINING_PATTERN_1);
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		/*
		* The DP 1.4 spec defines the max clock recovery retries value
		* as 10 but for pre-DP 1.4 devices we set a very tolerant
		* retry limit of 80 (4 voltage levels x 4 preemphasis levels x
		* x 5 identical voltage retries). Since the previous specs didn't
		* define a limit and created the possibility of an infinite loop
		* we want to prevent any sync from triggering that corner case.
		*/
		if (pstDptx->aucDPCD_Caps[DP_DPCD_REV] >= 0x14) {
			max_cr_tries = 10;
		} else {
			max_cr_tries = 80;
		}
		voltage_tries = 1u;
		for (cr_tries = 0u; cr_tries < max_cr_tries; cr_tries++) {
			drm_addition_link_train_clock_recovery_delay(pstDptx->aucDPCD_Caps);

			ret = Dptx_Aux_Read_Bytes_From_DPCD(pstDptx, DP_LANE0_1_STATUS,
								pstDptx->stDptxLink.aucTraining_Status,
								DP_LINK_STATUS_SIZE);
			if (ret != DPTX_RETURN_NO_ERROR) {
				dptx_err("Failed DP_LANE0_1_STATUS");
				break;
			}
			if (ret == DPTX_RETURN_NO_ERROR) {
				if (drm_addition_clock_recovery_ok(pstDptx->stDptxLink.aucTraining_Status,
								pstDptx->stDptxLink.ucNumOfLanes)) {
					recovery_ok = (bool)true;
				}
			}
			if ((ret == DPTX_RETURN_NO_ERROR) && !recovery_ok) {
				if (voltage_tries == 5u) {
					ret = -DPTX_RETURN_ENODEV;
					dptx_err("Same voltage tried 5 times");
				}
			}
			if ((ret == DPTX_RETURN_NO_ERROR) && !recovery_ok) {
				if (max_vswing_reached) {
					ret = -DPTX_RETURN_ENODEV;
					dptx_err("Max Voltage Swing reached");
				}
			}
			if ((ret == DPTX_RETURN_NO_ERROR) && !recovery_ok) {
				voltage = pstDptx->stDptxLink.aucVoltageSwing_level[0];

				/* Update training set as requested by target */
				ret = dptx_link_adjust_levels(pstDptx);
			}
			if ((ret == DPTX_RETURN_NO_ERROR) && !recovery_ok) {
				if (pstDptx->stDptxLink.aucVoltageSwing_level[0] == voltage) {
					voltage_tries++;
				} else {
					voltage_tries = 1u;
				}

				if (pstDptx->stDptxLink.aucVoltageSwing_level[0] >= DP_TRAIN_VOLTAGE_SWING_LEVEL_3) {
					max_vswing_reached = (bool)true;
				}
			}
			if (recovery_ok || (ret != DPTX_RETURN_NO_ERROR)) {
				break;
			}
		}
	}

	if (recovery_ok) {
		ret = DPTX_RETURN_NO_ERROR;
	} else {
		ret = -DPTX_RETURN_ENODEV;
		dptx_err("Failed clock recovery");
	}

	return ret;
}

static int32_t dptx_link_perform_ch_equalization(struct Dptx_Params *pstDptx)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint8_t dp_train_pat;
	uint32_t tries;

	bool channel_eq = (bool)false;

	switch (pstDptx->ucMax_Rate) {
	case DPTX_PHYIF_CTRL_RATE_HBR3:
		if (Drm_dp_tps4_supported(pstDptx->aucDPCD_Caps)) {
			dp_train_pat = DP_TRAINING_PATTERN_4;
			break;
		}
		if (Drm_dp_tps3_supported(pstDptx->aucDPCD_Caps)) {
			dp_train_pat = DP_TRAINING_PATTERN_3;
			break;
		}
		dp_train_pat = DP_TRAINING_PATTERN_2;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR2:
		if (Drm_dp_tps3_supported(pstDptx->aucDPCD_Caps)) {
			dp_train_pat = DP_TRAINING_PATTERN_3;
			break;
		}
		dp_train_pat = DP_TRAINING_PATTERN_2;
		break;
	case DPTX_PHYIF_CTRL_RATE_RBR:
	case DPTX_PHYIF_CTRL_RATE_HBR:
		dp_train_pat = DP_TRAINING_PATTERN_2;
		break;
	default:
		dptx_err("Invalid rate %d ", pstDptx->stDptxLink.ucLinkRate);
		ret = DPTX_RETURN_EINVAL;
	}

	if (ret == DPTX_RETURN_NO_ERROR) {
		dptx_set_link_train(pstDptx, dp_train_pat);

		for (tries = 0u; tries < 5u; tries++) {
			drm_addition_link_train_channel_eq_delay(pstDptx->aucDPCD_Caps);

			ret = Dptx_Aux_Read_Bytes_From_DPCD(pstDptx, DP_LANE0_1_STATUS,
								pstDptx->stDptxLink.aucTraining_Status,
								DP_LINK_STATUS_SIZE);
			if (ret == DPTX_RETURN_NO_ERROR) {
				/* Make sure clock is still ok */
				if (!drm_addition_clock_recovery_ok(pstDptx->stDptxLink.aucTraining_Status,
								pstDptx->stDptxLink.ucNumOfLanes)) {
					ret = -DPTX_RETURN_ENODEV;
				}
				if (drm_addition_channel_eq_ok(pstDptx->stDptxLink.aucTraining_Status,
							pstDptx->stDptxLink.ucNumOfLanes)) {
					channel_eq = (bool)true;
				}
				ret = dptx_link_adjust_levels(pstDptx);
			}
			if (channel_eq || (ret != DPTX_RETURN_NO_ERROR)) {
				break;
			}
		}
	}
	if (channel_eq) {
		ret = DPTX_RETURN_NO_ERROR;
	} else {
		ret = -DPTX_RETURN_ENODEV;
		dptx_err("Failed to channel equalization");
	}

	return ret;
}

static int32_t dptx_prepare_link_for_link_training(struct Dptx_Params *pstDptx)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;

	dptx_info("Starting... Num of lanes(%u), Link rate(%u)",
		  pstDptx->stDptxLink.ucNumOfLanes, pstDptx->stDptxLink.ucLinkRate);

	ret = Dptx_Core_Set_PHY_PowerState(pstDptx, PHY_POWER_DOWN_PHY_CLOCK);
	if (ret != DPTX_RETURN_NO_ERROR) {
		dptx_err("Failed PHY_POWER_DOWN_PHY_CLOCK()");
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		ret = Dptx_Core_Get_PHY_BUSY_Status(pstDptx, pstDptx->stDptxLink.ucNumOfLanes);
		if (ret != DPTX_RETURN_NO_ERROR) {
			dptx_err("Failed Dptx_Core_Get_PHY_BUSY_Status()");
		}
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		ret = Dptx_Core_Set_PHY_NumOfLanes(pstDptx, (uint8_t)pstDptx->stDptxLink.ucNumOfLanes);
		if (ret != DPTX_RETURN_NO_ERROR) {
			dptx_err("Failed Dptx_Core_Set_PHY_NumOfLanes()");
		}
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		ret = Dptx_Core_Set_PHY_PowerState(pstDptx, PHY_POWER_ON);
		if (ret != DPTX_RETURN_NO_ERROR) {
			dptx_err("Failed PHY_POWER_ON()");
		}
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		ret = Dptx_Core_Get_PHY_BUSY_Status(pstDptx, pstDptx->stDptxLink.ucNumOfLanes);
		if (ret != DPTX_RETURN_NO_ERROR) {
			dptx_err("Failed Dptx_Core_Get_PHY_BUSY_Status()");
		}
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		ret = Dptx_Core_Set_PHY_PowerState(pstDptx, PHY_POWER_DOWN_PHY_CLOCK);
		if (ret != DPTX_RETURN_NO_ERROR) {
			dptx_err("Failed PHY_POWER_DOWN_PHY_CLOCK()");
		}
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		ret = Dptx_Core_Get_PHY_BUSY_Status(pstDptx, pstDptx->stDptxLink.ucNumOfLanes);
		if (ret != DPTX_RETURN_NO_ERROR) {
			dptx_err("Failed Dptx_Core_Get_PHY_BUSY_Status()");
		}
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		ret = Dptx_Core_Set_PHY_Rate(pstDptx, (enum PHY_LINK_RATE)pstDptx->stDptxLink.ucLinkRate);
		if (ret != DPTX_RETURN_NO_ERROR) {
			dptx_err("Failed Dptx_Core_Set_PHY_Rate()");
		}
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		ret = Dptx_Core_Set_PHY_PowerState(pstDptx, PHY_POWER_ON);
		if (ret != DPTX_RETURN_NO_ERROR) {
			dptx_err("Failed PHY_POWER_ON()");
		}
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		ret = Dptx_Core_Get_PHY_BUSY_Status(pstDptx, pstDptx->stDptxLink.ucNumOfLanes);
		if (ret != DPTX_RETURN_NO_ERROR) {
			dptx_err("Failed Dptx_Core_Get_PHY_BUSY_Status()");
		}
	}
	return ret;
}

static void dptx_link_wait_hpd_status(struct Dptx_Params *pstDptx, uint32_t max_wait_ms)
{
	uint32_t spent_ms;

	for (spent_ms = 0u; spent_ms < max_wait_ms; spent_ms++) {
		if (dptx_intr_get_hotplug_status(pstDptx)) {
			break;
		}
		mdelay(1);
	}

	dptx_info("wait %d ms", spent_ms);
}

/*
 * dptx_prepare_link_training
 *  1. Reset voltage swing and preemphasis to zero.
 *  2. Compare link rate and link lane from DPRX
 *  3. If DPRX does not support the current link rate of DPTX, It changes
 *     PHY of DPTX to a supported link rate from DPRX.
 */
static int32_t dptx_prepare_link_training(struct Dptx_Params *pstDptx, uint8_t ucLink_Rate, uint8_t ucNumOfLanes, bool retry_training)
{
	uint8_t ucSink_Max_Rate, ucSink_Max_Lanes;
	uint8_t source_link_rate, source_link_lanes;
	int32_t ret;

	memset(pstDptx->stDptxLink.aucPreEmphasis_level, 0, sizeof(uint8_t) * PRE_EMPHASIS_LEVEL_MAX);
	memset(pstDptx->stDptxLink.aucVoltageSwing_level, 0, sizeof(uint8_t) * VOLTAGE_SWING_LEVEL_MAX);
	memset(pstDptx->stDptxLink.aucTraining_Status, 0, DP_LINK_STATUS_SIZE);

	ucSink_Max_Lanes = Drm_dp_max_lane_count(pstDptx->aucDPCD_Caps);

	ret = Dptx_Link_Bandwidth_To_PHY_Rate(pstDptx, pstDptx->aucDPCD_Caps[DP_MAX_LINK_RATE], &ucSink_Max_Rate);
	if (ret == DPTX_RETURN_NO_ERROR) {
		pstDptx->stDptxLink.ucNumOfLanes = (ucNumOfLanes > ucSink_Max_Lanes) ? ucSink_Max_Lanes : ucNumOfLanes;
		pstDptx->stDptxLink.ucLinkRate = (ucLink_Rate > ucSink_Max_Rate) ? ucSink_Max_Rate : ucLink_Rate;

		dptx_info("Init link training :");
		dptx_info("  Src dev %d lanes <-> Sink dev %d lanes", (uint32_t)ucNumOfLanes, (uint32_t)ucSink_Max_Lanes);
		dptx_info("  Src rate = %s  <-> Sink rate = %s\n",
					(ucLink_Rate == DPTX_PHYIF_CTRL_RATE_RBR) ? "RBR" :
					(ucLink_Rate == DPTX_PHYIF_CTRL_RATE_HBR) ? "HBR" :
					(ucLink_Rate == DPTX_PHYIF_CTRL_RATE_HBR2) ? "HBR2":"HBR3",
					ucSink_Max_Rate == DPTX_PHYIF_CTRL_RATE_RBR ? "RBR" :
					(ucSink_Max_Rate == DPTX_PHYIF_CTRL_RATE_HBR) ?  "HBR" :
					(ucSink_Max_Rate == DPTX_PHYIF_CTRL_RATE_HBR2) ? "HBR2":"HBR3");

		if (retry_training) {
			ret = Dptx_Core_Get_PHY_NumOfLanes(pstDptx, &source_link_lanes);
			if (ret == DPTX_RETURN_NO_ERROR) {
				ret = Dptx_Core_Get_PHY_Rate(pstDptx, &source_link_rate);
			}
		} else {
			source_link_lanes = pstDptx->ucMax_Lanes;
			source_link_rate = pstDptx->ucMax_Rate;
		}
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		if (source_link_rate != pstDptx->stDptxLink.ucLinkRate) {
			dptx_info("Reducing Link rate = %s -> Sink one = %s\n",
					(source_link_rate == DPTX_PHYIF_CTRL_RATE_RBR) ? "RBR" :
					(source_link_rate == DPTX_PHYIF_CTRL_RATE_HBR) ?  "HBR" :
					(source_link_rate == DPTX_PHYIF_CTRL_RATE_HBR2) ? "HBR2":"HBR3",
					pstDptx->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_RBR ? "RBR" :
					(pstDptx->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR) ?  "HBR" :
					(pstDptx->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR2) ? "HBR2":"HBR3");

			ret = dptx_reset_link_rate_and_lanes(pstDptx,
							     pstDptx->stDptxLink.ucLinkRate,
							     pstDptx->stDptxLink.ucNumOfLanes);
		} else {
			if (source_link_lanes != pstDptx->stDptxLink.ucNumOfLanes) {
				dptx_info("Reducing Link lane = %s -> Sink one = %s\n",
						(source_link_lanes == PHY_LANE_1) ? "lane1" :
						(source_link_lanes == PHY_LANE_2) ?  "lane2" :
						(source_link_lanes == PHY_LANE_4) ? "lane3":"unknown",
						pstDptx->stDptxLink.ucNumOfLanes == PHY_LANE_1 ? "lane1" :
						(pstDptx->stDptxLink.ucNumOfLanes == PHY_LANE_2) ?  "lane2" :
						(pstDptx->stDptxLink.ucNumOfLanes == PHY_LANE_4) ? "lane3":"unknown");
				Dptx_Sec_Set_NumOfLanes(pstDptx, pstDptx->stDptxLink.ucNumOfLanes);
			}
		}
	}
	return ret;
}

/*
 * dptx_reset_link_rate_and_lanes
 * - This API used to changes link rate and link lane at runtime
 */
int32_t dptx_reset_link_rate_and_lanes(struct Dptx_Params *pstDptx,
				       uint8_t link_rate, uint8_t link_lanes)
{
	int32_t ret;

	ret = Dptx_Cfg_Reset(pstDptx, link_rate);
	if (ret == DPTX_RETURN_NO_ERROR) {
		ret = Dptx_Sec_PHY_Init(pstDptx, link_rate, link_lanes);
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		ret = Dptx_Core_Init(pstDptx);
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		/*
			* It need to check HDP status to send DPCD msg to
			* DPRX after resetting the displayport core.
			* It can be checked HPD 100ms after displayport is reset.
			*/
		dptx_link_wait_hpd_status(pstDptx, 200);

		ret = Dptx_Ext_Set_Stream_Capability(pstDptx);
	}
	return ret;
}

int32_t Dptx_Link_Perform_Training(struct Dptx_Params *pstDptx, uint8_t ucRate, uint8_t ucNumOfLanes)
{
	bool retry_training = (bool)false;
	bool reduce_state = (bool)false;
	uint8_t ucDPCD_SinkCount;
	int32_t ret;

	do {
		if (retry_training) {
			ucRate = pstDptx->stDptxLink.ucLinkRate;
			ucNumOfLanes = pstDptx->stDptxLink.ucNumOfLanes;
			retry_training = (bool)false;
		}
		ret = dptx_prepare_link_training(pstDptx, ucRate, ucNumOfLanes, retry_training);
		if (ret == DPTX_RETURN_NO_ERROR) {
			ret = dptx_prepare_link_for_link_training(pstDptx);
		}
		if (ret == DPTX_RETURN_NO_ERROR) {
			ret = dptx_link_perform_clock_recovery(pstDptx);
			if (ret != DPTX_RETURN_NO_ERROR) {
				/* Failed clock recovery */
				ret = dptx_link_reduce_rate(pstDptx);
				if (ret != DPTX_RETURN_NO_ERROR) {
					reduce_state = (bool)true;
				}
				if (reduce_state) {
					/*
					* Already RBR
					* If inly lowest-numbered LANx_CR_DONE?
					*/
					if((pstDptx->stDptxLink.aucTraining_Status[PHY_LANE_0] & DP_LANE_CR_DONE) != 0u) {
						ret = dptx_link_reduce_lanes(pstDptx);
						if (ret == DPTX_RETURN_NO_ERROR) {
							pstDptx->stDptxLink.ucLinkRate = pstDptx->ucMax_Rate;
						}
					}
					reduce_state = (bool)false;
				}
				if (ret == DPTX_RETURN_NO_ERROR) {
					retry_training = (bool)true;
					continue;
				}
				dptx_err("Failed final clock recovery\n");
			}
		}
		if (ret == DPTX_RETURN_NO_ERROR) {
			ret = dptx_link_perform_ch_equalization(pstDptx);
			if (ret != DPTX_RETURN_NO_ERROR) {
				/* Failed Channel Equalization */
				ret = dptx_link_reduce_lanes(pstDptx);
				if (ret != DPTX_RETURN_NO_ERROR) {
					reduce_state = (bool)true;
				}
				if (reduce_state) {
					/* Already link lane is 1 */
					ret = dptx_link_reduce_rate(pstDptx);
					if (ret == DPTX_RETURN_NO_ERROR) {
						pstDptx->stDptxLink.ucNumOfLanes = pstDptx->ucMax_Lanes;
					}
					reduce_state = (bool)false;
				}
				if (ret == DPTX_RETURN_NO_ERROR) {
					retry_training = (bool)true;
					continue;
				}
				dptx_err("Failed final Channel Equalization\n");
			}
			break;
		}
	} while(retry_training);

	dptx_stop_link_train(pstDptx);

	if (ret == DPTX_RETURN_NO_ERROR) {
		drm_addition_read_mst_cap(pstDptx);
		ret = Dptx_Aux_Read_DPCD(pstDptx, DP_SINK_COUNT, &ucDPCD_SinkCount);
	}
	if (ret == DPTX_RETURN_NO_ERROR) {
		dptx_info("Link training succeeded with %s - %s",
					pstDptx->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_RBR ? "RBR" :
					(pstDptx->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR) ? "HBR" :
					(pstDptx->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR2) ? "HBR2":"HBR3",
					pstDptx->stDptxLink.ucNumOfLanes == 1U ? "1 lane" :
					(pstDptx->stDptxLink.ucNumOfLanes == 2U) ?  "2 lanes" :"4 lanes");
	} else {
		dptx_err("Failed link training !!!");
	}
	return ret;
}

int32_t Dptx_Link_PHY_Rate_To_Bandwidth(struct Dptx_Params *pstDptx, uint8_t ucRate, uint8_t *pucBandWidth)
{
	switch (ucRate) {
	case DPTX_PHYIF_CTRL_RATE_RBR:
		*pucBandWidth = DP_LINK_BW_1_62;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR:
		*pucBandWidth = DP_LINK_BW_2_7;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR2:
		*pucBandWidth = DP_LINK_BW_5_4;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR3:
		*pucBandWidth = DP_LINK_BW_8_1;
		break;
	default:
		dptx_err("Invalid rate %d ", (uint32_t)ucRate);
		return DPTX_RETURN_EINVAL;
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Link_Bandwidth_To_PHY_Rate(struct Dptx_Params *pstDptx, uint8_t ucBandWidth, uint8_t *pucRate)
{
	switch (ucBandWidth) {
	case DP_LINK_BW_1_62:
		*pucRate = DPTX_PHYIF_CTRL_RATE_RBR;
		break;
	case DP_LINK_BW_2_7:
		*pucRate = DPTX_PHYIF_CTRL_RATE_HBR;
		break;
	case DP_LINK_BW_5_4:
		*pucRate = DPTX_PHYIF_CTRL_RATE_HBR2;
		break;
	case DP_LINK_BW_8_1:
		*pucRate = DPTX_PHYIF_CTRL_RATE_HBR3;
		break;
	default:
		dptx_err("Invalid link rate -> %d ", (uint32_t)ucBandWidth);
		return DPTX_RETURN_EINVAL;
	}

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Link_Perform_BringUp(struct Dptx_Params *pstDptx)
{
	bool bSink_SSC_Profiled;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	iRetVal = Dptx_Aux_Write_DPCD(pstDptx, DP_SET_POWER, DP_SET_POWER_D0);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Core_Set_PHY_NumOfLanes(pstDptx, (u8)pstDptx->ucMax_Lanes);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Core_Get_Sink_SSC_Capability(pstDptx, &bSink_SSC_Profiled);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	iRetVal = Dptx_Core_Set_PHY_SSC(pstDptx, bSink_SSC_Profiled);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	memset(pstDptx->aucDPCD_Caps, 0, DPTX_SINK_CAP_SIZE);
	iRetVal = drm_addition_read_dpcd_caps(pstDptx);

	iRetVal = Dptx_Ext_Set_Stream_Capability(pstDptx);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		return iRetVal;
	}

	dptx_debug("Sink DP Revision %x.%x ", (pstDptx->aucDPCD_Caps[0] & 0xF0) >> 4, pstDptx->aucDPCD_Caps[0] & 0xF);

	return DPTX_RETURN_NO_ERROR;
}

bool dptx_link_get_linktraining_status(struct Dptx_Params *pstDptx)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	bool training_ok = (bool)false;

	ret = Dptx_Aux_Read_Bytes_From_DPCD(pstDptx, DP_LANE0_1_STATUS,
					    pstDptx->stDptxLink.aucTraining_Status,
					    DP_LINK_STATUS_SIZE);
	if (ret == DPTX_RETURN_NO_ERROR) {
		dptx_debug("Training status: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
			pstDptx->stDptxLink.aucTraining_Status[0], pstDptx->stDptxLink.aucTraining_Status[1],
			pstDptx->stDptxLink.aucTraining_Status[2], pstDptx->stDptxLink.aucTraining_Status[3],
			pstDptx->stDptxLink.aucTraining_Status[4], pstDptx->stDptxLink.aucTraining_Status[5]);

		if (drm_addition_clock_recovery_ok(pstDptx->stDptxLink.aucTraining_Status,
						   pstDptx->ucMax_Lanes)) {
			if (drm_addition_channel_eq_ok(pstDptx->stDptxLink.aucTraining_Status,
							pstDptx->ucMax_Lanes)) {
				dptx_debug("CR and EQ are done");
				training_ok = (bool)true;
			} else {
				dptx_debug("Channel EQ is not done... ");
			}
		}
	}

	return training_ok;
}
