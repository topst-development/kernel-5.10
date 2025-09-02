// SPDX-License-Identifier: GPL-2.0
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



static int32_t dptx_link_adjust_levels(struct Dptx_Params *dev_param)
{
	uint8_t dptx_sink_lane_config_values[DPTX_MAX_LINK_LANES] = {0, };
	uint8_t dptx_adjust_voltagw_swing[DPTX_MAX_LINK_LANES] = {0, };
	uint8_t dptx_adjust_pre_emphasis[DPTX_MAX_LINK_LANES] = {0, };

	uint32_t dptx_adjust_value, lane_idx;
	uint8_t dpcd_value;

	int32_t ret = DPTX_RETURN_NO_ERROR;

	if (dev_param->stDptxLink.ucNumOfLanes > 4u) {
		dptx_err("Invalid number of lanes %d ", dev_param->stDptxLink.ucNumOfLanes);
		ret = -DPTX_RETURN_EINVAL;
	} else {
		if (dev_param->stDptxLink.ucNumOfLanes == 4u) {
			ret = Dptx_Aux_Read_DPCD(dev_param, DP_ADJUST_REQUEST_LANE2_3, &dpcd_value);
			if (DPTX_RETURN_SUCCESS(ret)) {
				dptx_adjust_value = (uint32_t)dpcd_value & 0xFu;
				dptx_adjust_pre_emphasis[2] = (dptx_adjust_value >> 2) & 0x3u;
				dptx_adjust_voltagw_swing[2] = dptx_adjust_value & 0x3u;

				dptx_adjust_value = ((uint32_t)dpcd_value >> 4) & 0xFu;
				dptx_adjust_pre_emphasis[3] = (dptx_adjust_value >> 2) & 0x3u;
				dptx_adjust_voltagw_swing[3] = dptx_adjust_value & 0x3u;
			}
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			/* For KCS */
			ret = Dptx_Aux_Read_DPCD(dev_param, DP_ADJUST_REQUEST_LANE0_1, &dpcd_value);
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			dptx_adjust_value = (uint32_t)dpcd_value & 0xFu;
			dptx_adjust_pre_emphasis[0] = (dptx_adjust_value >> 2) & 0x3u;
			dptx_adjust_voltagw_swing[0] = dptx_adjust_value & 0x3u;

			dptx_adjust_value = ((uint32_t)dpcd_value >> 4) & 0xFu;
			dptx_adjust_pre_emphasis[1] = (dptx_adjust_value >> 2) & 0x3u;
			dptx_adjust_voltagw_swing[1] = dptx_adjust_value & 0x3u;

			dptx_dbg("LANE0(Vsw %d, PreEm %d), LANE1(Vsw %d, PreEm %d), LANE2(Vsw %d, PreEm %d), LANE3(Vsw %d, PreEm %d)",
				  dptx_adjust_voltagw_swing[0], dptx_adjust_pre_emphasis[0],
				  dptx_adjust_voltagw_swing[1], dptx_adjust_pre_emphasis[1],
				  dptx_adjust_voltagw_swing[2], dptx_adjust_pre_emphasis[2],
				  dptx_adjust_voltagw_swing[3], dptx_adjust_pre_emphasis[3]);
		}
	}

	if (DPTX_RETURN_SUCCESS(ret)) {
		(void)memcpy(dev_param->stDptxLink.aucVoltageSwing_level,
			     dptx_adjust_voltagw_swing,
			     DPTX_MAX_LINK_LANES * sizeof(uint8_t));
		(void)memcpy(dev_param->stDptxLink.aucPreEmphasis_level,
			     dptx_adjust_pre_emphasis,
			     DPTX_MAX_LINK_LANES * sizeof(uint8_t));

		for (lane_idx = 0u; lane_idx < dev_param->stDptxLink.ucNumOfLanes; lane_idx++) {
			dptx_sink_lane_config_values[lane_idx] = dptx_adjust_voltagw_swing[lane_idx];
			dptx_sink_lane_config_values[lane_idx] |= ((uint32_t)dptx_adjust_pre_emphasis[lane_idx] << DP_TRAIN_PRE_EMPHASIS_SHIFT);

			if (dptx_adjust_voltagw_swing[lane_idx] == 3u) {
				dptx_info("Lane %d VSW reached to level 3", lane_idx);
				dptx_sink_lane_config_values[lane_idx] |= DP_TRAIN_MAX_SWING_REACHED;
			}
			if (dptx_adjust_pre_emphasis[lane_idx] == 3u) {
				dptx_dbg("Lane %d Pre-emphasis reached to level 3", lane_idx);
				dptx_sink_lane_config_values[lane_idx] |= DP_TRAIN_MAX_PRE_EMPHASIS_REACHED;
			}
		}
		ret = dptx_core_set_phy_sigan_quality(dev_param);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		ret = Dptx_Aux_Write_Bytes_To_DPCD(dev_param, DP_TRAINING_LANE0_SET,
						   dptx_sink_lane_config_values,
						   dev_param->stDptxLink.ucNumOfLanes);
	}
	return ret;
}

static int32_t dptx_link_reduce_rate(struct Dptx_Params *dev_param)
{
	uint8_t ucLinkRate = (uint8_t)DPTX_PHYIF_CTRL_RATE_RBR;
	int32_t ret = DPTX_RETURN_NO_ERROR;

	switch (dev_param->stDptxLink.ucLinkRate) {
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
		dptx_err("Invalid PHY rate %d set RBR as defult\n", dev_param->stDptxLink.ucLinkRate);
		ret = -DPTX_RETURN_EINVAL;
		break;
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dptx_force(" Reducing rate from %s to %s",
			  dev_param->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_RBR ? "RBR" :
			  (dev_param->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR) ?  "HBR" :
			  (dev_param->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR2) ? "HBR2":"HBR3",
			  (ucLinkRate == DPTX_PHYIF_CTRL_RATE_RBR) ? "RBR" :
			  (ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR) ?  "HBR" :
			  (ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR2) ? "HBR2":"HBR3");
		dev_param->stDptxLink.ucLinkRate = ucLinkRate;
	}
	return ret;
}

static int32_t dptx_link_reduce_lanes(struct Dptx_Params *dev_param)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint8_t new_link_lanes;

	switch (dev_param->stDptxLink.ucNumOfLanes) {
	case (uint8_t)4u:
		new_link_lanes = (uint8_t)PHY_LANE_2;
		break;
	case (uint8_t)2u:
		new_link_lanes = (uint8_t)PHY_LANE_1;
		break;
	case (uint8_t)1u:
		dptx_warn("The number of lanes is reached to 1");
		ret = -DPTX_RETURN_EINVAL;
		break;
	default:
		new_link_lanes = (uint8_t)PHY_LANE_1;
		dptx_err("Invalid number of lanes %d set LANE1 as defult\n", dev_param->stDptxLink.ucNumOfLanes);
		ret = -DPTX_RETURN_EINVAL;
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dptx_err("Reducing lanes from %d to %d", dev_param->stDptxLink.ucNumOfLanes, new_link_lanes);
		dev_param->stDptxLink.ucNumOfLanes = new_link_lanes;
	}

	return ret;
}

static int32_t dptx_set_link_train(struct Dptx_Params *dev_param, uint8_t dp_train_pat)
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

	ret = Dptx_Core_Set_PHY_Pattern(dev_param, dp_phy_pattern);
	if (DPTX_RETURN_SUCCESS(ret)) {
		/* For KCS */
		ret = Dptx_Core_Enable_PHY_XMIT(dev_param, dev_param->stDptxLink.ucNumOfLanes);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dp_train_pat_msg[0] = dp_train_pat;
		dp_train_pat_len = 1;

		if ((dp_train_pat & DP_TRAINING_PATTERN_MASK) != DP_TRAINING_PATTERN_DISABLE) {
			/* DP_TRAINING_LANEx_SET follow DP_TRAINING_PATTERN_SET */
			for (index = 0u; index < dev_param->stDptxLink.ucNumOfLanes; index++) {
				dp_train_pat_msg[index + 1] =
					dev_param->stDptxLink.aucVoltageSwing_level[index] |
					(dev_param->stDptxLink.aucPreEmphasis_level[index] << DP_TRAIN_PRE_EMPHASIS_SHIFT);
				dp_train_pat_len++;
			}
		}
		ret = Dptx_Aux_Write_Bytes_To_DPCD(dev_param, DP_TRAINING_PATTERN_SET,
						   dp_train_pat_msg, dp_train_pat_len);
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			dptx_err("Failed DP_TRAINING_PATTERN_SET");
		}
	}
	return ret;
}

static void dptx_stop_link_train(struct Dptx_Params *dev_param)
{
	dptx_set_link_train(dev_param, DP_TRAINING_PATTERN_DISABLE);
}

static int32_t dptx_reset_link_train(struct Dptx_Params *dev_param, uint8_t dp_train_pat)
{
	memset(dev_param->stDptxLink.aucPreEmphasis_level, 0, sizeof(uint8_t) * PRE_EMPHASIS_LEVEL_MAX);
	memset(dev_param->stDptxLink.aucVoltageSwing_level, 0, sizeof(uint8_t) * VOLTAGE_SWING_LEVEL_MAX);
	dptx_core_set_phy_sigan_quality(dev_param);
	return dptx_set_link_train(dev_param, dp_train_pat);
}

static int32_t dptx_link_perform_clock_recovery(struct Dptx_Params *dev_param)
{

	uint32_t max_cr_tries, cr_tries, voltage_tries;
	bool max_vswing_reached = (bool)false;
	bool recovery_ok = (bool)false;
	uint8_t voltage, dpcd_val;
	int32_t ret;

	/* 6. Set the following DPCD registers on the Sink */
	ret = Dptx_Link_PHY_Rate_To_Bandwidth(dev_param, dev_param->stDptxLink.ucLinkRate, &dpcd_val);
	if (DPTX_RETURN_SUCCESS(ret)) {
		/* 6.a LINK_BW_SET */
		ret = Dptx_Aux_Write_DPCD(dev_param, DP_LINK_BW_SET, dpcd_val);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dpcd_val = dev_param->stDptxLink.ucNumOfLanes;
		if (Drm_dp_enhanced_frame_cap(dev_param->aucDPCD_Caps)) {
			/* For KCS */
			dpcd_val |= DP_ENHANCED_FRAME_CAP;
		}
		/* 6.b LANE_COUNT_SE */
		ret = Dptx_Aux_Write_DPCD(dev_param, DP_LANE_COUNT_SET, dpcd_val);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		uint32_t cctl_reg;

		/* 8.h */
		cctl_reg = Dptx_Reg_Readl(dev_param, DPTX_CCTL);
		if (Drm_dp_enhanced_frame_cap(dev_param->aucDPCD_Caps)) {
			/* For KCS */
			cctl_reg |= DPTX_CCTL_ENH_FRAME_EN;
		} else {
			cctl_reg &= ~DPTX_CCTL_ENH_FRAME_EN;

		}
		Dptx_Reg_Writel(dev_param, DPTX_CCTL, cctl_reg);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (dev_param->bSpreadSpectrum_Clock &&
		(dev_param->aucDPCD_Caps[DP_MAX_DOWNSPREAD] & SINK_TDOWNSPREAD_MASK)) {
			dpcd_val = DP_SPREAD_AMP_0_5;
		} else {
			dpcd_val = 0;
		}

		/* 6.c DOWNSPREAD_CTRL */
		ret = Dptx_Aux_Write_DPCD(dev_param, DP_DOWNSPREAD_CTRL, dpcd_val);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		/* 6.d MAIN_LINK_CHANNEL_CODING_SET */
		dpcd_val = DP_SET_ANSI_8B10B;
		ret = Dptx_Aux_Write_DPCD(dev_param, DP_MAIN_LINK_CHANNEL_CODING_SET, dpcd_val);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dptx_dbg("Training clock_recovery with Setting to TPS 1 in PHY and Pattern Sequence 1 in Sink...");
		ret = dptx_reset_link_train(dev_param, DP_TRAINING_PATTERN_1);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		/*
		 * The DP 1.4 spec defines the max clock recovery retries value
		 * as 10 but for pre-DP 1.4 devices we set a very tolerant
		 * retry limit of 80 (4 voltage levels x 4 preemphasis levels x
		 * x 5 identical voltage retries). Since the previous specs didn't
		 * define a limit and created the possibility of an infinite loop
		 * we want to prevent any sync from triggering that corner case.
		 */
		if (dev_param->aucDPCD_Caps[DP_DPCD_REV] >= 0x14) {
			/* For KCS */
			max_cr_tries = 10;
		} else {
			max_cr_tries = 80;
		}
		voltage_tries = 1u;
		for (cr_tries = 0u; cr_tries < max_cr_tries; cr_tries++) {
			drm_addition_link_train_clock_recovery_delay(dev_param->aucDPCD_Caps);

			ret = Dptx_Aux_Read_Bytes_From_DPCD(dev_param, DP_LANE0_1_STATUS,
								dev_param->stDptxLink.aucTraining_Status,
								DP_LINK_STATUS_SIZE);
			if (DPTX_RETURN_ERROR(ret)) {
				dptx_err("Failed DP_LANE0_1_STATUS");
				break;
			}
			if (DPTX_RETURN_SUCCESS(ret)) {
				if (drm_addition_clock_recovery_ok(dev_param->stDptxLink.aucTraining_Status,
								dev_param->stDptxLink.ucNumOfLanes)) {
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
				voltage = dev_param->stDptxLink.aucVoltageSwing_level[0];

				/* Update training set as requested by target */
				ret = dptx_link_adjust_levels(dev_param);
			}
			if ((ret == DPTX_RETURN_NO_ERROR) && !recovery_ok) {
				if (dev_param->stDptxLink.aucVoltageSwing_level[0] == voltage) {
					/* For KCS */
					voltage_tries++;
				} else {
					voltage_tries = 1u;
				}

				if (dev_param->stDptxLink.aucVoltageSwing_level[0] >= DP_TRAIN_VOLTAGE_SWING_LEVEL_3) {
					/* For KCS */
					max_vswing_reached = (bool)true;
				}
			}
			if (recovery_ok || (ret != DPTX_RETURN_NO_ERROR)) {
				/* For KCS */
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

static int32_t dptx_link_perform_ch_equalization(struct Dptx_Params *dev_param)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint8_t dp_train_pat;
	uint32_t tries;

	bool channel_eq = (bool)false;

	switch (dev_param->ucMax_Rate) {
	case DPTX_PHYIF_CTRL_RATE_HBR3:
		if (Drm_dp_tps4_supported(dev_param->aucDPCD_Caps)) {
			dp_train_pat = DP_TRAINING_PATTERN_4;
			break;
		}
		if (Drm_dp_tps3_supported(dev_param->aucDPCD_Caps)) {
			dp_train_pat = DP_TRAINING_PATTERN_3;
			break;
		}
		dp_train_pat = DP_TRAINING_PATTERN_2;
		break;
	case DPTX_PHYIF_CTRL_RATE_HBR2:
		if (Drm_dp_tps3_supported(dev_param->aucDPCD_Caps)) {
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
		dptx_err("Invalid rate %d ", dev_param->stDptxLink.ucLinkRate);
		ret = DPTX_RETURN_EINVAL;
	}

	if (DPTX_RETURN_SUCCESS(ret)) {
		dptx_set_link_train(dev_param, dp_train_pat);

		for (tries = 0u; tries < 5u; tries++) {
			drm_addition_link_train_channel_eq_delay(dev_param->aucDPCD_Caps);

			ret = Dptx_Aux_Read_Bytes_From_DPCD(dev_param, DP_LANE0_1_STATUS,
								dev_param->stDptxLink.aucTraining_Status,
								DP_LINK_STATUS_SIZE);
			if (DPTX_RETURN_SUCCESS(ret)) {
				/* Make sure clock is still ok */
				if (!drm_addition_clock_recovery_ok(dev_param->stDptxLink.aucTraining_Status,
								dev_param->stDptxLink.ucNumOfLanes)) {
					ret = -DPTX_RETURN_ENODEV;
				}
				if (drm_addition_channel_eq_ok(dev_param->stDptxLink.aucTraining_Status,
							dev_param->stDptxLink.ucNumOfLanes)) {
					channel_eq = (bool)true;
				}
				ret = dptx_link_adjust_levels(dev_param);
			}
			if (channel_eq || (ret != DPTX_RETURN_NO_ERROR)) {
				/* For KCS */
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

/**
 * @brief Prepare the DisplayPort PHY for link training.
 *
 * This function configures the DisplayPort PHY (Physical Layer) settings necessary for link training.
 * It sets up the link lane configuration, link rate, and spread spectrum clocking (SSC) based on the parameters
 * provided in the `dev_param` structure.
 *
 * The typical steps include:
 * 1. Configuring the number of link lanes to be used for the DisplayPort connection.
 * 2. Setting the appropriate link rate (bandwidth) for data transmission.
 * 3. Enabling or disabling Spread Spectrum Clocking (SSC) based on the device configuration.
 *
 * @param[in] dev_param Pointer to the Dptx_Params structure that contains the device parameters including
 *                      the desired link lane count, link rate, and SSC settings.
 *
 * @return int32_t Returns 0 on success or a negative error code on failure.
 */
static int32_t dptx_prepare_phy_for_link_training(struct Dptx_Params *dev_param)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;

	dptx_dbg("Starting... Num of lanes(%u), Link rate(%u)",
		  dev_param->stDptxLink.ucNumOfLanes, dev_param->stDptxLink.ucLinkRate);

	ret = Dptx_Core_Set_PHY_PowerState(dev_param, PHY_POWER_DOWN_PHY_CLOCK);
	if (DPTX_RETURN_ERROR(ret)) {
		/* For KCS */
		dptx_err("Failed PHY_POWER_DOWN_PHY_CLOCK()");
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		ret = Dptx_Core_Get_PHY_BUSY_Status(dev_param, dev_param->stDptxLink.ucNumOfLanes);
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			dptx_err("Failed Dptx_Core_Get_PHY_BUSY_Status()");
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		ret = Dptx_Core_Set_PHY_NumOfLanes(dev_param, (uint8_t)dev_param->stDptxLink.ucNumOfLanes);
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			dptx_err("Failed Dptx_Core_Set_PHY_NumOfLanes()");
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		ret = Dptx_Core_Get_PHY_BUSY_Status(dev_param, dev_param->stDptxLink.ucNumOfLanes);
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			dptx_err("Failed Dptx_Core_Get_PHY_BUSY_Status()");
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		ret = Dptx_Core_Set_PHY_Rate(dev_param, (enum PHY_LINK_RATE)dev_param->stDptxLink.ucLinkRate);
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			dptx_err("Failed Dptx_Core_Set_PHY_Rate()");
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		ret = Dptx_Core_Get_PHY_BUSY_Status(dev_param, dev_param->stDptxLink.ucNumOfLanes);
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			dptx_err("Failed Dptx_Core_Get_PHY_BUSY_Status()");
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		ret = Dptx_Core_Set_PHY_SSC(dev_param);
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			dptx_err("Failed Dptx_Core_Set_PHY_Rate()");
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		ret = Dptx_Core_Get_PHY_BUSY_Status(dev_param, dev_param->stDptxLink.ucNumOfLanes);
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			dptx_err("Failed Dptx_Core_Get_PHY_BUSY_Status()");
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		ret = Dptx_Core_Set_PHY_PowerState(dev_param, PHY_POWER_ON);
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			dptx_err("Failed PHY_POWER_ON()");
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		ret = Dptx_Core_Get_PHY_BUSY_Status(dev_param, dev_param->stDptxLink.ucNumOfLanes);
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			dptx_err("Failed Dptx_Core_Get_PHY_BUSY_Status()");
		}
	}
	return ret;
}


/**
 * @brief Verifies the PHY settings for link training.
 *
 * This function performs the following steps:
 * 1. Resets voltage swing and pre-emphasis to zero.
 * 2. Compares the link rate and link lane configuration with the settings from DPRX.
 * 3. If DPRX does not support the current link rate of DPTX, the PHY settings of DPTX are adjusted
 *    to match a supported link rate from DPRX.
 *
 * @param dev_param Pointer to the DPTX parameters structure.
 * @param requested_link_rate The desired link rate for the training.
 * @param requested_link_lanes The number of lanes to use for the training.
 * @param retry_training Flag indicating if this is a retry of the training process.
 *
 * @return int32_t Returns 0 on success or a negative error code on failure.
 */
static int32_t dptx_verify_phy_for_link_training(struct Dptx_Params *dev_param,
						  uint8_t requested_link_rate,
						  uint8_t requested_link_lanes,
						  bool retry_training)
{
	uint8_t sink_max_link_rate, sink_max_link_lanes;
	uint8_t source_link_rate, source_link_lanes;
	int32_t ret;

	bool phy_reset_needed = (bool)false;

	/*
	 * For link training, the pre-emphasis and voltage swing should be set
	 * to level 0 before starting.
	 */
	memset(dev_param->stDptxLink.aucPreEmphasis_level, 0, sizeof(uint8_t) * PRE_EMPHASIS_LEVEL_MAX);
	memset(dev_param->stDptxLink.aucVoltageSwing_level, 0, sizeof(uint8_t) * VOLTAGE_SWING_LEVEL_MAX);
	memset(dev_param->stDptxLink.aucTraining_Status, 0, DP_LINK_STATUS_SIZE);

	sink_max_link_lanes = Drm_dp_max_lane_count(dev_param->aucDPCD_Caps);

	ret = Dptx_Link_Bandwidth_To_PHY_Rate(dev_param, dev_param->aucDPCD_Caps[DP_MAX_LINK_RATE], &sink_max_link_rate);
	if (DPTX_RETURN_SUCCESS(ret)) {
		dev_param->stDptxLink.ucNumOfLanes =
			(requested_link_lanes > sink_max_link_lanes) ? sink_max_link_lanes : requested_link_lanes;
		dev_param->stDptxLink.ucLinkRate =
			(requested_link_rate  > sink_max_link_rate) ? sink_max_link_rate : requested_link_rate;

		dptx_info("Init link training :");
		dptx_info("  Src dev %d lanes <-> Sink dev %d lanes", (uint32_t)requested_link_lanes, (uint32_t)sink_max_link_lanes);
		dptx_info("  Src rate = %s  <-> Sink rate = %s\n",
					(requested_link_rate  == DPTX_PHYIF_CTRL_RATE_RBR) ? "RBR" :
					(requested_link_rate  == DPTX_PHYIF_CTRL_RATE_HBR) ? "HBR" :
					(requested_link_rate  == DPTX_PHYIF_CTRL_RATE_HBR2) ? "HBR2":"HBR3",
					(sink_max_link_rate == DPTX_PHYIF_CTRL_RATE_RBR) ? "RBR" :
					(sink_max_link_rate == DPTX_PHYIF_CTRL_RATE_HBR) ?  "HBR" :
					(sink_max_link_rate == DPTX_PHYIF_CTRL_RATE_HBR2) ? "HBR2":"HBR3");

		if (retry_training) {
			ret = Dptx_Core_Get_PHY_NumOfLanes(dev_param, &source_link_lanes);
			if (DPTX_RETURN_SUCCESS(ret)) {
				/* For KCS */
				ret = Dptx_Core_Get_PHY_Rate(dev_param, &source_link_rate);
			}
		} else {
			source_link_lanes = dev_param->ucMax_Lanes;
			source_link_rate = dev_param->ucMax_Rate;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (source_link_rate != dev_param->stDptxLink.ucLinkRate) {
			phy_reset_needed = (bool)true;
			dptx_force("Reducing Link rate = %s -> %s\n",
					(source_link_rate == DPTX_PHYIF_CTRL_RATE_RBR) ? "RBR" :
					(source_link_rate == DPTX_PHYIF_CTRL_RATE_HBR) ?  "HBR" :
					(source_link_rate == DPTX_PHYIF_CTRL_RATE_HBR2) ? "HBR2":"HBR3",
					(dev_param->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_RBR) ? "RBR" :
					(dev_param->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR) ?  "HBR" :
					(dev_param->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR2) ? "HBR2":"HBR3");

		} else {
			if (source_link_lanes != dev_param->stDptxLink.ucNumOfLanes) {
				phy_reset_needed = (bool)true;
				dptx_force("Reducing Link lane = %s -> = %s\n",
						(source_link_lanes == 1u) ? "lane1" :
						(source_link_lanes == 2u) ?  "lane2" :
						(source_link_lanes == 4u) ? "lane3":"unknown",
						(dev_param->stDptxLink.ucNumOfLanes == 1u) ? "lane1" :
						(dev_param->stDptxLink.ucNumOfLanes == 2u) ?  "lane2" :
						(dev_param->stDptxLink.ucNumOfLanes == 4u) ? "lane3":"unknown");
			}
		}
	}
	if (phy_reset_needed) {
		(void)panel_serializer_reset_callback();

		ret = dptx_cfg_reset_phy(dev_param, dev_param->stDptxLink.ucLinkRate,
					 dev_param->stDptxLink.ucNumOfLanes);

		if (!dptx_intr_check_hpd_and_wait_hpd_to_plugged(dev_param)) {
			dev_param->last_known_hpd_status = HPD_STATUS_UNPLUGGED;
			dptx_err("Hot unplugged...");
		}
		/**
		 * In panel mode, the process must restart from the power-on sequence as
		 * the panel is reinitialized. However, since it is currently not possible
		 * to determine whether it is in panel mode, the code has been modified to
		 * always call Dptx_Link_Perform_BringUp again.
		 */
		Dptx_Link_Perform_BringUp(dev_param);
	}
	return ret;
}

int32_t Dptx_Link_Perform_Training(struct Dptx_Params *dev_param, uint8_t ucRate, uint8_t ucNumOfLanes)
{
	bool retry_training = (bool)false;
	uint8_t ucDPCD_SinkCount;
	int32_t ret;

	do {
		if (retry_training) {
			ucRate = dev_param->stDptxLink.ucLinkRate;
			ucNumOfLanes = dev_param->stDptxLink.ucNumOfLanes;
		}
		ret = dptx_verify_phy_for_link_training(dev_param, ucRate, ucNumOfLanes, retry_training);
		retry_training = (bool)false;
		if (DPTX_RETURN_SUCCESS(ret)) {
			/* For KCS */
			ret = dptx_prepare_phy_for_link_training(dev_param);
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			ret = dptx_link_perform_clock_recovery(dev_param);
			if (DPTX_RETURN_ERROR(ret)) {
				/* Failed clock recovery */
				ret = dptx_link_reduce_rate(dev_param);
				if (DPTX_RETURN_ERROR(ret)) {
					/*
					 * Already RBR
					 * If inly lowest-numbered LANx_CR_DONE?
					 */
					if ((dev_param->stDptxLink.aucTraining_Status[PHY_LANE_0] & DP_LANE_CR_DONE) != 0u) {
						ret = dptx_link_reduce_lanes(dev_param);
						if (DPTX_RETURN_SUCCESS(ret)) {
							/* For KCS */
							dev_param->stDptxLink.ucLinkRate = dev_param->ucMax_Rate;
						}
					}
				}
				if (DPTX_RETURN_SUCCESS(ret)) {
					retry_training = (bool)true;
					continue;
				}
				dptx_err("Failed final clock recovery\n");
			}
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			ret = dptx_link_perform_ch_equalization(dev_param);
			if (DPTX_RETURN_ERROR(ret)) {
				/* Failed Channel Equalization */
				ret = dptx_link_reduce_lanes(dev_param);
				if (DPTX_RETURN_ERROR(ret)) {
					/* Already link lane is 1 */
					ret = dptx_link_reduce_rate(dev_param);
					if (DPTX_RETURN_SUCCESS(ret)) {
						/* For KCS */
						dev_param->stDptxLink.ucNumOfLanes = dev_param->ucMax_Lanes;
					}
				}
				if (DPTX_RETURN_SUCCESS(ret)) {
					retry_training = (bool)true;
					continue;
				}
				dptx_err("Failed final Channel Equalization\n");
			}
			break;
		}
	} while (retry_training);

	dptx_stop_link_train(dev_param);

	if (DPTX_RETURN_SUCCESS(ret)) {
		drm_addition_read_mst_cap(dev_param);
		ret = Dptx_Aux_Read_DPCD(dev_param, DP_SINK_COUNT, &ucDPCD_SinkCount);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dptx_info("Link training succeeded with %s - %s",
					dev_param->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_RBR ? "RBR" :
					(dev_param->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR) ? "HBR" :
					(dev_param->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR2) ? "HBR2":"HBR3",
					(dev_param->stDptxLink.ucNumOfLanes == 1U) ? "1 lane" :
					(dev_param->stDptxLink.ucNumOfLanes == 2U) ? "2 lanes" : "4 lanes");
	} else {
		dptx_err("Failed link training !!!");
	}
	return ret;
}

int32_t Dptx_Link_PHY_Rate_To_Bandwidth(struct Dptx_Params *dev_param, uint8_t ucRate, uint8_t *pucBandWidth)
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

int32_t Dptx_Link_Bandwidth_To_PHY_Rate(struct Dptx_Params *dev_param, uint8_t ucBandWidth, uint8_t *pucRate)
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

/**
 * @brief Perform the initial bring-up for the DisplayPort link.
 *
 * This function prepares the DisplayPort transmitter for link training by first changing
 * the power state of the DisplayPort receiver to D0 (fully powered on). It then reads the
 * capabilities of the DisplayPort Rx to determine the appropriate settings for link training.
 *
 * @param[in] dev_param Pointer to the DPTX driver context structure.
 *
 * @return int32_t Returns 0 on success, or a negative error code on failure.
 */
int32_t Dptx_Link_Perform_BringUp(struct Dptx_Params *dev_param)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;

	ret = Dptx_Aux_Write_DPCD(dev_param, DP_SET_POWER, DP_SET_POWER_D0);
	if (DPTX_RETURN_SUCCESS(ret)) {
		memset(dev_param->aucDPCD_Caps, 0, DPTX_SINK_CAP_SIZE);
		ret = drm_addition_read_dpcd_caps(dev_param);
	}
	#if 0
	if (DPTX_RETURN_SUCCESS(ret)) {
		/* For KCS */
		ret = Dptx_Ext_Set_Stream_Capability(dev_param);
	}
	#endif
	if (DPTX_RETURN_SUCCESS(ret)) {
		dptx_info("Sink DP Revision %x.%x ",
			   (dev_param->aucDPCD_Caps[0] & 0xF0) >> 4,
			   dev_param->aucDPCD_Caps[0] & 0xF);
	}
	return ret;
}

bool dptx_link_get_linktraining_status(struct Dptx_Params *dev_param)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	bool training_ok = (bool)false;

	ret = Dptx_Aux_Read_Bytes_From_DPCD(dev_param, DP_LANE0_1_STATUS,
					    dev_param->stDptxLink.aucTraining_Status,
					    DP_LINK_STATUS_SIZE);
	if (DPTX_RETURN_SUCCESS(ret)) {
		dptx_debug("Training status: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
			dev_param->stDptxLink.aucTraining_Status[0], dev_param->stDptxLink.aucTraining_Status[1],
			dev_param->stDptxLink.aucTraining_Status[2], dev_param->stDptxLink.aucTraining_Status[3],
			dev_param->stDptxLink.aucTraining_Status[4], dev_param->stDptxLink.aucTraining_Status[5]);

		if (drm_addition_clock_recovery_ok(dev_param->stDptxLink.aucTraining_Status,
						   dev_param->ucMax_Lanes)) {
			if (drm_addition_channel_eq_ok(dev_param->stDptxLink.aucTraining_Status,
							dev_param->ucMax_Lanes)) {
				dptx_debug("CR and EQ are done");
				training_ok = (bool)true;
			} else {
				dptx_debug("Channel EQ is not done... ");
			}
		}
	}

	return training_ok;
}

int dptx_link_limit_rate_for_monitor(struct Dptx_Params *dev_param)
{
	uint8_t edid_desc_blk[4] = {EDID_DESC_BLK1,
				    EDID_DESC_BLK2,
				    EDID_DESC_BLK3,
				    EDID_DESC_BLK4};
	uint8_t i = 0;
	int32_t ret = 0;

	if (!dev_param->bMultStreamTransport) {
		Dptx_Edid_Read_EDID_I2C_Over_Aux(dev_param);

		for (i = 0; i < 4; i++) {
			if ((dev_param->pucEdidBuf[edid_desc_blk[i] + EDID_PRODUCT_NAME_INDICATOR_OFFSET]
				== EDID_PRODUCT_NAME_INDICATOR_TAG) &&
			    (dev_param->pucEdidBuf[edid_desc_blk[i] + 5u] == 0x32) &&
			    (dev_param->pucEdidBuf[edid_desc_blk[i] + 6u] == 0x37) &&
			    (dev_param->pucEdidBuf[edid_desc_blk[i] + 7u] == 0x55) &&
			    (dev_param->pucEdidBuf[edid_desc_blk[i] + 8u] == 0x53) &&
			    (dev_param->pucEdidBuf[edid_desc_blk[i] + 9u] == 0x44) &&
			    (dev_param->pucEdidBuf[edid_desc_blk[i] + 10u] == 0x35) &&
			    (dev_param->pucEdidBuf[edid_desc_blk[i] + 11u] == 0x35)) {
				/* 
				 * Due to a compatibility issue with the 27USD55 monitor,
				 * setting the Link Rate to HBR3 causes screen flickering.
				 * To prevent this, the Link Rate is forcibly limited to HBR2
				 * (5.4Gbps) for this monitor.
				 */
				dev_param->ucMax_Rate = (uint8_t)DPTX_PHYIF_CTRL_RATE_HBR2;
				dptx_dbg("27USD55 detected - forcing Link Rate to HBR2\n");

				if (dptx_cfg_reset_phy(dev_param, dev_param->ucMax_Rate,
						       dev_param->ucMax_Lanes) != DPTX_RETURN_NO_ERROR) {
					dptx_err("Failed dptx_cfg_reset_phy");
					ret = -DPTX_RETURN_EINVAL;
				} 

				break;
			}
		}
	}

	return ret;
}
