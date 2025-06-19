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

#include <dptx_v14.h>
#include <dptx_drm_dp_addition.h>
#include <dptx_reg.h>
#include <dptx_dbg.h>


/**
 * @brief Handle potential HPD unplug event with debounce check.
 *
 * This function checks whether an HPD (Hot Plug Detect) signal de-assertion
 * truly indicates a physical unplug event or a short HPD pulse.
 * It performs repeated polling up to 100ms (10 times with 10ms sleep)
 * to detect if the HPD signal is re-asserted (i.e., a pulse).
 *
 * If no re-assertion occurs within the debounce window and HPD remains low,
 * it treats the situation as a full unplug event and performs necessary cleanup,
 * such as:
 *  - Notifying the registered HPD callback (if any)
 *  - Clearing the DP stream mode
 *
 * @param[in] dev_param DisplayPort driver context pointer containing general
 *                    information for DisplayPort control.
 * @return 0 if the function executes successfully; otherwise, a negative value is returned.
 */
static int32_t dptx_intr_handle_hpd_unplug(struct Dptx_Params *dev_param)
{
	uint32_t loop;

	enum hpd_detection_status hpd_status;
	int32_t ret = DPTX_RETURN_NO_ERROR;

	for (loop = 0u; loop < 10u; loop++) {
		hpd_status = dptx_intr_get_hotplug_status(dev_param);

		if ((hpd_status == HPD_STATUS_UNPLUGGED) ||
		    (hpd_status == HPD_STATUS_PLUGGED)) {
			/**
			 * HPD was asserted again within the 100ms debounce
			 * window after de-assertion, indicating an HPD
			 * Pulse event rather than a full unplug.
			 */
			break;
		}
		usleep_range(10000, 10500);
	}
	if (hpd_status != HPD_STATUS_PLUGGED) {
		ret = Dptx_Intr_Handle_HotUnplug(dev_param);
		if (DPTX_RETURN_SUCCESS(ret)) {
			if (dev_param->pvHPD_Intr_CallBack == NULL) {
				dptx_err("HPD callback isn't registered");
				ret = -DPTX_RETURN_ENODEV;
			} else {
				for (loop = 0; loop < dev_param->ucNumOfPorts; loop++) {
					/* HPD status changed — calling registered callback */
					dev_param->pvHPD_Intr_CallBack(loop, (bool)HPD_STATUS_UNPLUGGED);
				}
			}
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			/* HPD was de-asserted — cleaning up the DisplayPort stream. */
			(void)Dptx_Ext_Set_Stream_Mode(dev_param, 0U);
		}
	}
	return ret;
}

/**
 * @brief Performs the necessary actions when an HPD is plugged or unplugged.
 *
 * This function called when an HPD PLUG interrupt or HPD UNPLUG interrupt`
 * occurs. When an HPD is plugged, it sets up the DisplayPort output according
 * to the DisplayPort protocol, and when an HPD is unplugged, it stops the
 * DisplayPort output.
 *
 * @param[in] dev_param DisplayPort driver context pointer containing general
 *                    information for DisplayPort control.
 * @param[in] hpd_plugged It indicates whether the DisplayPort HPD is plugged
 *                        in or not.
 * @return 0 if the function executes successfully; otherwise, a negative value is returned.
 *
 */
static int dptx_intr_handle_hpd(struct Dptx_Params *dev_param, bool hpd_plugged)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;

	if (hpd_plugged == (bool)HPD_STATUS_UNPLUGGED) {
		ret = dptx_intr_handle_hpd_unplug(dev_param);
	} else {
		ret = dptx_intr_linktraining(dev_param);
	}
	return ret;
}

static void dptx_intr_notify(struct Dptx_Params *dev_param)
{
	(void)wake_up_interruptible(&dev_param->WaitQ);
}

static void dptx_intr_handle_hpd_irq(struct Dptx_Params *dev_param)
{
	atomic_set(&dev_param->Sink_request, 1);
	dptx_intr_notify(dev_param);
}

irqreturn_t Dptx_Intr_Threaded_IRQ(int irq, void *dev)
{
	uint32_t uiHpdStatus;
	enum hpd_detection_status hpd_status;

	struct Dptx_Params *dev_param = dev;

	(void)irq;

	mutex_lock(&dev_param->Mutex);

	if (atomic_read(&dev_param->HPD_IRQ_State) != 0) {
		atomic_set(&dev_param->HPD_IRQ_State, 0);

		uiHpdStatus = Dptx_Reg_Readl(dev_param, DPTX_HPDSTS);

		hpd_status = ((uiHpdStatus & DPTX_HPDSTS_STATUS) != 0u) ? HPD_STATUS_PLUGGED : HPD_STATUS_UNPLUGGED;

		dptx_dbg("HPD state is changed from %s to %s",
			(dev_param->last_known_hpd_status == HPD_STATUS_PLUGGED) ? "Plugged" : "Unplugged",
			(hpd_status == HPD_STATUS_PLUGGED) ? "Plugged" : "Unplugged");

		dptx_intr_handle_hpd(dev_param, (bool)hpd_status);

		dev_param->last_known_hpd_status = hpd_status;
	}

	if (atomic_read(&dev_param->Sink_request) != 0) {
		/* For KCS */
		atomic_set(&dev_param->Sink_request, 0);
	}

	dptx_debug("=== DONE ===\n");

	mutex_unlock(&dev_param->Mutex);

	return IRQ_HANDLED;
}

irqreturn_t Dptx_Intr_IRQ(int irq, void *dev)
{
	irqreturn_t eRetVal = IRQ_HANDLED;
	uint32_t uiInterrupt_Status, uiHpdStatus, uiRegMap_InterEn;

	struct Dptx_Params *dev_param = dev;

	(void)irq;

	uiInterrupt_Status = Dptx_Reg_Readl(dev_param, DPTX_ISTS);
	uiRegMap_InterEn = Dptx_Reg_Readl(dev_param, DPTX_IEN);
	uiHpdStatus = Dptx_Reg_Readl(dev_param, DPTX_HPDSTS);

	dptx_debug("Read: GENERAL_INTERRUPT[0x%08x]: 0x%08x, INTERRUPT_EN[0x%08x]: 0x%08x, INTERRUPT_STATE[0x%08x]: 0x%08x",
				DPTX_ISTS, uiInterrupt_Status, DPTX_IEN, uiRegMap_InterEn, DPTX_HPDSTS, uiHpdStatus);

	if ((uiInterrupt_Status & DPTX_ISTS_ALL_INTR) == 0u) {
		dptx_debug("IRQ_NONE");
		return  IRQ_NONE;
	}

	dptx_debug("Interrrupt type = %s 0x%x",
				(uiInterrupt_Status & DPTX_ISTS_HPD) ? "HPD" :
				(uiInterrupt_Status & DPTX_ISTS_HDCP) ? "HDCP" :
				(uiInterrupt_Status & DPTX_ISTS_SDP) ? "SDP" :
				(uiInterrupt_Status & DPTX_ISTS_VIDEO_FIFO_OVERFLOW) ? "Video FIFO Overflow" :
				(uiInterrupt_Status & DPTX_ISTS_AUDIO_FIFO_OVERFLOW) ? "Audio FIFO Overflow" : "Other",
				uiInterrupt_Status);

	if ((uiInterrupt_Status & DPTX_ISTS_VIDEO_FIFO_OVERFLOW) != 0u) {
		uiRegMap_InterEn = Dptx_Reg_Readl(dev_param, DPTX_IEN);

		if (uiRegMap_InterEn & DPTX_IEN_VIDEO_FIFO_OVERFLOW) {
			/* For KCS */
			dptx_warn("Video overflow!!!");
		}
		Dptx_Reg_Writel(dev_param, DPTX_ISTS, DPTX_ISTS_VIDEO_FIFO_OVERFLOW);
	}

	if ((uiInterrupt_Status & DPTX_ISTS_AUDIO_FIFO_OVERFLOW) != 0u) {
		uiRegMap_InterEn = Dptx_Reg_Readl(dev_param, DPTX_IEN);

		if (uiRegMap_InterEn & DPTX_IEN_AUDIO_FIFO_OVERFLOW) {
			/* For KCS */
			dptx_warn("DPTX_ISTS_AUDIO_FIFO_OVERFLOW");
		}
		Dptx_Reg_Writel(dev_param, DPTX_ISTS, DPTX_ISTS_AUDIO_FIFO_OVERFLOW);
	}

	if ((uiInterrupt_Status & DPTX_ISTS_HPD) != 0u) {
#ifdef CONFIG_HDCP_DWC_HLD
		/*
		 * == Workaround : Toggling CP_IRQ by software ==
		 * Sending CP_IRQ to ESM when handling HPD interrupt.
		 */
		if ((Dptx_Reg_Readl(dev_param, DPTX_HDCP_OBS) & DPTX_HDCP22_BOOTED) != 0U) {
			u32 uiHdcpCfg = Dptx_Reg_Readl(dev_param, DPTX_HDCP_CFG);

			Dptx_Reg_Writel(dev_param, DPTX_HDCP_CFG, (u32)(uiHdcpCfg | DPTX_CFG_CP_IRQ));
			Dptx_Reg_Writel(dev_param, DPTX_HDCP_CFG, (u32)(uiHdcpCfg & ~DPTX_CFG_CP_IRQ));
		}
#endif
		uiHpdStatus = Dptx_Reg_Readl(dev_param, DPTX_HPDSTS);

		if ((uiHpdStatus & DPTX_HPDSTS_IRQ) != 0u) {
			Dptx_Reg_Writel(dev_param, DPTX_HPDSTS, DPTX_HPDSTS_IRQ);

			dptx_intr_handle_hpd_irq(dev_param);

			eRetVal = IRQ_WAKE_THREAD;
		}

		if ((uiHpdStatus & DPTX_HPDSTS_HOT_PLUG) != 0u) {
			Dptx_Reg_Writel(dev_param, DPTX_HPDSTS, DPTX_HPDSTS_HOT_PLUG);

			atomic_set(&dev_param->HPD_IRQ_State, 1);
			if (test_and_clear_bit(0, &dev_param->wait_flags)) {
				/* For KCS */
				complete(&dev_param->hpd_plug_comp);
			}

			eRetVal = IRQ_WAKE_THREAD;
		}

		if ((uiHpdStatus & DPTX_HPDSTS_HOT_UNPLUG) != 0) {
			Dptx_Reg_Writel(dev_param, DPTX_HPDSTS, DPTX_HPDSTS_HOT_UNPLUG);

			atomic_set(&dev_param->HPD_IRQ_State, 1);

			dptx_intr_notify(dev_param);

			eRetVal = IRQ_WAKE_THREAD;
		}
	}

	return eRetVal;
}

int32_t Dptx_Intr_Init_Params(struct Dptx_Params *dev_param)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;

	if (dptx_intr_get_hotplug_status(dev_param) != HPD_STATUS_UNPLUGGED) {
		/* For KCS */
		dev_param->last_known_hpd_status = HPD_STATUS_PLUGGED;
	} else {
		dev_param->last_known_hpd_status = HPD_STATUS_UNPLUGGED;
	}
	if (dev_param->dp_slave_mode) {
		dev_param->bSideBand_MSG_Supported = (bool)false;
	} else {
		if (dev_param->last_known_hpd_status != HPD_STATUS_UNPLUGGED) {
			ret = Dptx_Edid_Read_EDID_I2C_Over_Aux(dev_param);
			dev_param->bSideBand_MSG_Supported = (ret == DPTX_RETURN_NO_ERROR) ? (bool)true : (bool)false;
		}
	}

	return ret;
}

int32_t Dptx_Intr_Get_Port_Composition(struct Dptx_Params *dev_param, bool should_clear_payload)
{
	uint8_t dptx_daisy_chain_order;
	uint8_t num_of_plugged_ports = 1u;

	int32_t ret = DPTX_RETURN_NO_ERROR;

	bool dptx_sink_supports_mst = (bool)false;

	dev_param->bSideBand_MSG_Supported = false;
	dptx_sink_supports_mst = drm_addition_read_mst_cap(dev_param);

	ret = Dptx_Edid_Read_EDID_I2C_Over_Aux(dev_param);

	if (DPTX_RETURN_SUCCESS(ret)) {
		/* For KCS */
		dev_param->bSideBand_MSG_Supported = true;
	}

	for (dptx_daisy_chain_order = 0; dptx_daisy_chain_order < (uint8_t)PHY_INPUT_STREAM_MAX; dptx_daisy_chain_order++) {
		(void)memset(dev_param->paucEdidBuf_Entry[dptx_daisy_chain_order],
		       0, DPTX_EDID_BUFLEN);
	}

	if (dev_param->bSideBand_MSG_Supported) {
		if (dptx_sink_supports_mst) {
			ret = Dptx_Ext_Get_TopologyState(dev_param, should_clear_payload, &num_of_plugged_ports);
			if (DPTX_RETURN_ERROR(ret)) {
				/* For KCS */
				num_of_plugged_ports = 1u;
			}
		}

		if (num_of_plugged_ports == 1U) {
			(void)Dptx_Edid_Read_EDID_I2C_Over_Aux(dev_param);

			(void)memcpy(dev_param->paucEdidBuf_Entry[0], dev_param->pucEdidBuf, DPTX_EDID_BUFLEN);
		} else {
			for (dptx_daisy_chain_order = 0u; dptx_daisy_chain_order < num_of_plugged_ports; dptx_daisy_chain_order++) {
				(void)Dptx_Edid_Read_EDID_Over_Sideband_Msg(dev_param, dptx_daisy_chain_order);
				(void)memcpy(dev_param->paucEdidBuf_Entry[dptx_daisy_chain_order], dev_param->pucEdidBuf, DPTX_EDID_BUFLEN);
			}
		}
		dptx_info("%d %s connected", num_of_plugged_ports, (num_of_plugged_ports == 1u) ? "Ext. monitor is":"Ext. monitors are");
	} else {
		if (dev_param->pvPanel_Topology_CallBack != NULL) {
			ret = dev_param->pvPanel_Topology_CallBack(&num_of_plugged_ports);
			if (DPTX_RETURN_ERROR(ret)) {
				/* For KCS */
				num_of_plugged_ports = 1;
			}
		}
		dptx_info("%d %s connected", num_of_plugged_ports, (num_of_plugged_ports == 1u) ? " panel is":" panels are");
	}

	(void)Dptx_Ext_Set_Stream_Mode(dev_param, num_of_plugged_ports);

	return DPTX_RETURN_NO_ERROR;
}


int32_t Dptx_Intr_Register_HPD_Callback(struct Dptx_Params *dev_param, Dptx_HPD_Intr_Callback HPD_Intr_Callback)
{
	dev_param->pvHPD_Intr_CallBack = HPD_Intr_Callback;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Intr_Register_Panel_Callback(struct Dptx_Params *dev_param, Dptx_Panel_Topology_Callback Panel_Topology_CallBack)
{
	dev_param->pvPanel_Topology_CallBack = Panel_Topology_CallBack;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Intr_Handle_HotUnplug(struct Dptx_Params *dev_param)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint32_t dp_idx;

	dev_param->last_known_hpd_status = HPD_STATUS_UNPLUGGED;

	ret = Dptx_Core_Disable_PHY_XMIT(dev_param, dev_param->ucMax_Lanes);
	if (DPTX_RETURN_SUCCESS(ret)) {
		/* PHY PHY_POWER_DOWN_PHY_CLOCK */
		ret = Dptx_Core_Set_PHY_PowerState(dev_param, PHY_POWER_DOWN_PHY_CLOCK);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		/* Wait to phy busy check */
		ret = Dptx_Core_Get_PHY_BUSY_Status(dev_param, dev_param->ucMax_Lanes);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		ret = dptx_cfg_reset_phy(dev_param, dev_param->stDptxLink.ucLinkRate,
					 dev_param->stDptxLink.ucNumOfLanes);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		for (dp_idx = 0u; dp_idx < PHY_INPUT_STREAM_MAX; dp_idx++) {
			/* Disable all video streams */
			(void)Dptx_VidIn_Set_Stream_Enable(dev_param, false, dp_idx);
		}
	}

	return ret;
}

/**
 * @brief Check if HPD is high
 *
 * * This function checks whether HPD is high by reading the HPD_STATUS register.
 * Note: The DPTX_HPDSTS_STATUS bit indicates the current status of the HPD input.
 *       However, even if this value is high, it does not guarantee that HPD is
 *       in the PLUGGED state.
 *
 * @param[in] dev_param DisplayPort driver context pointer containing general
 *                    information for DisplayPort control.
 * @return true if the HPD is high; otherwise, false indicating HPD is low.
 *
 */
static bool dptx_intr_get_hotplug_pin(const struct Dptx_Params *dev_param)
{
	bool hpd_high = (bool)false;
	uint32_t reg_val;

	reg_val = Dptx_Reg_Readl(dev_param, DPTX_HPDSTS);
	if ((reg_val & (uint32_t)DPTX_HPDSTS_STATUS) != 0u) {
		/* For KCS */
		hpd_high = (bool)true;
	}

	return hpd_high;
}

/**
 * @brief Get the current HPD status.
 *
 * This function checks the HPD (Hot Plug Detect) status by reading the HPD_STATUS register
 * from the DisplayPort transmitter (DPTX). It interprets the read value to determine
 * if the HPD signal is in an unplugged, transient (PLUS), or stable (PLUGGED) state.
 *
 * The DPTX_HPDSTS_STATUS bit indicates the current status of the HPD input. Note that even if
 * this bit is high, it does not necessarily guarantee that the HPD is in the PLUGGED state.
 *
 * The function checks specific values from the register:
 * - If the value is 1 or 4, it returns HPD_STATUS_PLUS, indicating a transient or IRQ state.
 * - If the value is 7, it returns HPD_STATUS_PLUGGED, indicating a stable connection.
 * - In all other cases, it returns HPD_STATUS_UNPLUGGED.
 *
 * @param[in] dev_param DisplayPort driver context pointer containing general
 *                      information for DisplayPort control.
 *
 * @return enum hpd_detection_status
 *         - HPD_STATUS_UNPLUGGED if the connection is not detected.
 *         - HPD_STATUS_PLUS if a transient IRQ-like state is detected (0.5ms ~ 1ms or 2ms ~ < 100ms low).
 *         - HPD_STATUS_PLUGGED if the connection is stable and active.
 */
enum hpd_detection_status dptx_intr_get_hotplug_status(struct Dptx_Params *dev_param)
{
	enum hpd_detection_status hpd_status = HPD_STATUS_UNPLUGGED;
	uint32_t hpd_val;

	if (dev_param != NULL) {
		hpd_val = Dptx_Reg_Readl(dev_param, DPTX_HPDSTS);
		hpd_val &= DPTX_HPDSTS_STATE_MASK;
		hpd_val >>= DPTX_HPDSTS_STATE_SHIFT;

		if ((hpd_val == 1u) || (hpd_val == 4u)) {
			/* For KCS */
			hpd_status = HPD_STATUS_PLUS;
		}
		if (hpd_val == 7u) {
			/* For KCS */
			hpd_status = HPD_STATUS_PLUGGED;
		}

	}
	return hpd_status;
}

/**
 * @brief Check if HPD and wait HPD to PLUGGED STATE
 *
 * This function first checks if HPD is high and if HPD is hig, check whether
 * HPD is in PLUGGED state for up to 100ms.
 * Note: When DislayPort is initialized, it takes at least 100ms to confirm
 * HPD PLUGGED.
 *
 * @param[in] dev_param DisplayPort driver context pointer containing general
 *                    information for DisplayPort control.
 * @return true if the HPD is PLUGGED; otherwise, false indicating HPD is UNPLUGGED.
 *
 */
bool dptx_intr_check_hpd_and_wait_hpd_to_plugged(struct Dptx_Params *dev_param)
{
	bool hpd_plugged = (bool)false;
	bool loop_end = (bool)false;
	uint32_t loop;

	if (dptx_intr_get_hotplug_pin(dev_param)) {
		for (loop = 0u; loop < 20u; loop++) {
			if (dptx_intr_get_hotplug_status(dev_param) == HPD_STATUS_PLUGGED) {
				hpd_plugged = (bool)true;
				loop_end = (bool)true;
			}
			if (!dptx_intr_get_hotplug_pin(dev_param)) {
				/* For KCS */
				loop_end = (bool)true;
			}
			if (loop_end) {
				/* For KCS */
				break;
			}
			mdelay(10);
		}
	}
	return hpd_plugged;
}

/**
 * @brief Manages the overall process of DisplayPort link training.
 *
 * This function oversees the entire DisplayPort link training process.
 *
 * @return int32_t
 * - Returns 0 on successful completion of link training.
 * - Returns a negative error code if link training fails at any stage.
 */
static int32_t dptx_intr_linktraining_internal(struct Dptx_Params *dev_param, bool dptx_probe_stage)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint32_t dp_idx;

	bool link_is_trained, prev_mst_mode;
	bool should_clear_payload = (bool)true;

	dptx_update_aux_hysteresis_from_dtb(dev_param);
	ret = dptx_check_aux_communication(dev_param);
	if (DPTX_RETURN_SUCCESS(ret)) {
		/* For KCS */
		ret = Dptx_Link_Perform_BringUp(dev_param);
	}

	if (DPTX_RETURN_SUCCESS(ret)) {
		if (dptx_probe_stage) {
			should_clear_payload = (bool)false;
		}
		prev_mst_mode = dev_param->bMultStreamTransport;
		ret = Dptx_Intr_Get_Port_Composition(dev_param,
						     should_clear_payload);
	}

	if (DPTX_RETURN_SUCCESS(ret)) {
		/* For KCS */
		ret = Dptx_Ext_Set_Stream_Capability(dev_param);
	}

	if (DPTX_RETURN_SUCCESS(ret)) {
		link_is_trained = dptx_link_get_linktraining_status(dev_param);
		if (prev_mst_mode != dev_param->bMultStreamTransport) {
			/**
			 * If the MST mode set by the bootloader differs from
			 * the one configured in the kernel, link training must
			 * be performed again.
			 */
			link_is_trained = (bool)false;
		}
		if (!link_is_trained) {
			ret = Dptx_Link_Perform_Training(dev_param, dev_param->ucMax_Rate, dev_param->ucMax_Lanes);
			if (DPTX_RETURN_SUCCESS(ret)) {
				if (dev_param->bMultStreamTransport) {
					/* For KCS */
					ret = Dptx_Ext_Set_Topology_Configuration(dev_param);
				}
			}
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (dev_param->pvHPD_Intr_CallBack == NULL) {
			dptx_err("HPD callback isn't registered");
			ret = -DPTX_RETURN_ENODEV;
		} else {
			for (dp_idx = 0; dp_idx < dev_param->ucNumOfPorts; dp_idx++) {
				dev_param->pvHPD_Intr_CallBack(dp_idx,
								(bool)HPD_STATUS_PLUGGED);
			}
		}
	}
	return ret;
}

int32_t dptx_probe_linktraining(struct Dptx_Params *dev_param)
{
	return dptx_intr_linktraining_internal(dev_param, (bool)true);
}

int32_t dptx_intr_linktraining(struct Dptx_Params *dev_param)
{
	return dptx_intr_linktraining_internal(dev_param, (bool)false);
}
