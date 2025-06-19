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


#define EDID_START_BIT_OF_1ST_DETAILED_DES	54
#define EDID_SIZE_OF_DETAILED_DES		18


static int dptx_intr_handle_hpd(struct Dptx_Params *pstDptx, bool bHotPlugged)
{
	uint8_t ucDP_Index;
	int32_t iRetVal;

	if (bHotPlugged == (bool)HPD_STATUS_UNPLUGGED) {
		iRetVal = Dptx_Intr_Handle_HotUnplug(pstDptx);
		if (iRetVal !=  DPTX_RETURN_NO_ERROR) {
			return iRetVal;
		}

		for (ucDP_Index = 0; ucDP_Index < (uint8_t)pstDptx->ucNumOfPorts; ucDP_Index++) {
			if (pstDptx->pvHPD_Intr_CallBack == NULL) {
				dptx_warn("DP%u: HPD callback isn't registered", ucDP_Index);
				return DPTX_RETURN_ENODEV;
			}

			pstDptx->pvHPD_Intr_CallBack(ucDP_Index, (bool)HPD_STATUS_UNPLUGGED);
		}

		(void)Dptx_Ext_Set_Stream_Mode(pstDptx, 0U);

		return DPTX_RETURN_NO_ERROR;
	}

	/* Initialize max lanes and max rate */
	pstDptx->ucMax_Rate = DPTX_PHYIF_CTRL_RATE_HBR3;
	pstDptx->ucMax_Lanes = PHY_LANE_4;

	iRetVal = Dptx_Intr_Get_Port_Composition(pstDptx, (bool)DPTX_CLEAR_PAYLOAD);
	if (iRetVal !=  DPTX_RETURN_NO_ERROR)
		return iRetVal;

	if (!dptx_link_get_linktraining_status(pstDptx)) {
		iRetVal = Dptx_Link_Perform_BringUp(pstDptx);
		if (iRetVal !=  DPTX_RETURN_NO_ERROR)
			return iRetVal;

		iRetVal = Dptx_Link_Perform_Training(pstDptx, pstDptx->ucMax_Rate, pstDptx->ucMax_Lanes);
		if (iRetVal !=  DPTX_RETURN_NO_ERROR)
			return iRetVal;

		if (pstDptx->bMultStreamTransport) {
			iRetVal = Dptx_Ext_Set_Topology_Configuration(pstDptx);
			if (iRetVal !=  DPTX_RETURN_NO_ERROR)
				return iRetVal;
		}
	}

	for (ucDP_Index = 0; ucDP_Index < pstDptx->ucNumOfPorts; ucDP_Index++) {
		if (pstDptx->pvHPD_Intr_CallBack == NULL) {
			dptx_err("HPD callback isn't registered");
			return DPTX_RETURN_ENODEV;
		}

		pstDptx->pvHPD_Intr_CallBack(ucDP_Index, (bool)HPD_STATUS_PLUGGED);
	}

	return DPTX_RETURN_NO_ERROR;
}

static void dptx_intr_notify(struct Dptx_Params *pstDptx)
{
	wake_up_interruptible(&pstDptx->WaitQ);
}

static void dptx_intr_handle_hpd_irq(struct Dptx_Params *pstDptx)
{
	atomic_set(&pstDptx->Sink_request, 1);
	dptx_intr_notify(pstDptx);
}

irqreturn_t Dptx_Intr_Threaded_IRQ(int irq, void *dev)
{
	uint32_t uiHpdStatus;
	enum HPD_Detection_Status eHPDStatus;

	struct Dptx_Params *pstDptx = dev;

	mutex_lock(&pstDptx->Mutex);

	if (atomic_read(&pstDptx->HPD_IRQ_State)) {
		atomic_set(&pstDptx->HPD_IRQ_State, 0);

		uiHpdStatus = Dptx_Reg_Readl(pstDptx, DPTX_HPDSTS);

		eHPDStatus = (uiHpdStatus & DPTX_HPDSTS_STATUS) ? HPD_STATUS_PLUGGED : HPD_STATUS_UNPLUGGED;

		if (pstDptx->eLast_HPDStatus == eHPDStatus) {
			dptx_dbg("HPD state is not changed as %s", (pstDptx->eLast_HPDStatus == HPD_STATUS_PLUGGED) ? "Plugged":"Unplugged");
		} else {
			dptx_dbg("HPD state is changed from %s to %s",
				(pstDptx->eLast_HPDStatus == HPD_STATUS_PLUGGED) ? "Plugged" : "Unplugged",
				(eHPDStatus == HPD_STATUS_PLUGGED) ? "Plugged" : "Unplugged");

			dptx_intr_handle_hpd(pstDptx, (bool)eHPDStatus);

			pstDptx->eLast_HPDStatus = eHPDStatus;
		}
	}

	if (atomic_read(&pstDptx->Sink_request)) {
		atomic_set(&pstDptx->Sink_request, 0);
	}

	dptx_debug("=== DONE ===\n");

	mutex_unlock(&pstDptx->Mutex);

	return IRQ_HANDLED;
}

irqreturn_t Dptx_Intr_IRQ(int irq, void *dev)
{
	irqreturn_t eRetVal = IRQ_HANDLED;
	uint32_t uiInterrupt_Status, uiHpdStatus, uiRegMap_InterEn;

	struct Dptx_Params *pstDptx = dev;

	uiInterrupt_Status = Dptx_Reg_Readl(pstDptx, DPTX_ISTS);
	uiRegMap_InterEn = Dptx_Reg_Readl(pstDptx, DPTX_IEN);
	uiHpdStatus = Dptx_Reg_Readl(pstDptx, DPTX_HPDSTS);

	dptx_debug("Read: GENERAL_INTERRUPT[0x%08x]: 0x%08x, INTERRUPT_EN[0x%08x]: 0x%08x, INTERRUPT_STATE[0x%08x]: 0x%08x",
				DPTX_ISTS, uiInterrupt_Status, DPTX_IEN, uiRegMap_InterEn, DPTX_HPDSTS, uiHpdStatus);

	if (!(uiInterrupt_Status & DPTX_ISTS_ALL_INTR)) {
		dptx_debug("IRQ_NONE");
		return  IRQ_NONE;
	}

	dptx_debug("Interrrupt type = %s",
				(uiInterrupt_Status & DPTX_ISTS_HPD) ? "HPD" :
				(uiInterrupt_Status & DPTX_ISTS_HDCP) ? "HDCP" :
				(uiInterrupt_Status & DPTX_ISTS_SDP) ? "SDP" :
				(uiInterrupt_Status & DPTX_ISTS_VIDEO_FIFO_OVERFLOW) ? "Video FIFO Overflow" :
				(uiInterrupt_Status & DPTX_ISTS_AUDIO_FIFO_OVERFLOW) ? "Audio FIFO Overflow" : "Other");

	if (uiInterrupt_Status & DPTX_ISTS_VIDEO_FIFO_OVERFLOW) {
		uiRegMap_InterEn = Dptx_Reg_Readl(pstDptx, DPTX_IEN);

		if (uiRegMap_InterEn & DPTX_IEN_VIDEO_FIFO_OVERFLOW) {
			dptx_warn("Video overflow!!!");
			Dptx_Reg_Writel(pstDptx, DPTX_ISTS, DPTX_ISTS_VIDEO_FIFO_OVERFLOW);
		}
	}

	if (uiInterrupt_Status & DPTX_ISTS_AUDIO_FIFO_OVERFLOW) {
		uiRegMap_InterEn = Dptx_Reg_Readl(pstDptx, DPTX_IEN);

		if (uiRegMap_InterEn & DPTX_IEN_AUDIO_FIFO_OVERFLOW) {
			dptx_warn("DPTX_ISTS_AUDIO_FIFO_OVERFLOW");
			Dptx_Reg_Writel(pstDptx, DPTX_ISTS, DPTX_ISTS_AUDIO_FIFO_OVERFLOW);
		}
	}

	if (uiInterrupt_Status & DPTX_ISTS_HPD) {
#ifdef CONFIG_HDCP_DWC_HLD
		/*
		 * == Workaround : Toggling CP_IRQ by software ==
		 * Sending CP_IRQ to ESM when handling HPD interrupt.
		 */
		if ((Dptx_Reg_Readl(pstDptx, DPTX_HDCP_OBS) & DPTX_HDCP22_BOOTED) != 0U) {
			u32 uiHdcpCfg = Dptx_Reg_Readl(pstDptx, DPTX_HDCP_CFG);
			Dptx_Reg_Writel(pstDptx, DPTX_HDCP_CFG, (u32)(uiHdcpCfg | DPTX_CFG_CP_IRQ));
			Dptx_Reg_Writel(pstDptx, DPTX_HDCP_CFG, (u32)(uiHdcpCfg & ~DPTX_CFG_CP_IRQ));
		}
#endif
		uiHpdStatus = Dptx_Reg_Readl(pstDptx, DPTX_HPDSTS);

		if (uiHpdStatus & DPTX_HPDSTS_IRQ) {
			dptx_debug("DPTX_HPDSTS_IRQ");

			Dptx_Reg_Writel(pstDptx, DPTX_HPDSTS, DPTX_HPDSTS_IRQ);

			dptx_intr_handle_hpd_irq(pstDptx);

			eRetVal = IRQ_WAKE_THREAD;
		}

		if (uiHpdStatus & DPTX_HPDSTS_HOT_PLUG) {
			dptx_debug("DPTX_HPDSTS_HOT_PLUG");

			Dptx_Reg_Writel(pstDptx, DPTX_HPDSTS, DPTX_HPDSTS_HOT_PLUG);

			atomic_set(&pstDptx->HPD_IRQ_State, 1);
			if (test_and_clear_bit(0, &pstDptx->wait_flags) == 1) {
				complete(&pstDptx->hpd_plug_comp);
			}

			eRetVal = IRQ_WAKE_THREAD;
		}

		if (uiHpdStatus & DPTX_HPDSTS_HOT_UNPLUG) {
			dptx_debug("DPTX_HPDSTS_HOT_UNPLUG");

			Dptx_Reg_Writel(pstDptx, DPTX_HPDSTS, DPTX_HPDSTS_HOT_UNPLUG);

			atomic_set(&pstDptx->HPD_IRQ_State, 1);

			dptx_intr_notify(pstDptx);

			eRetVal = IRQ_WAKE_THREAD;
		}
	}

	return eRetVal;
}

int32_t Dptx_Intr_Init_Params(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	if (dptx_intr_get_hotplug_status(pstDptx)) {
		pstDptx->eLast_HPDStatus = HPD_STATUS_PLUGGED;
	} else {
		pstDptx->eLast_HPDStatus = HPD_STATUS_UNPLUGGED;
	}

	iRetVal = Dptx_Edid_Read_EDID_I2C_Over_Aux(pstDptx);
	pstDptx->bSideBand_MSG_Supported = (iRetVal == DPTX_RETURN_NO_ERROR) ? (bool)true : (bool)false;

	return iRetVal;
}

int32_t Dptx_Intr_Get_Port_Composition(struct Dptx_Params *pstDptx, bool bClear_PayloadID)
{
	bool bMST_Supported = (bool)false;
	uint8_t ucDP_Idx;
	uint8_t ucNumOfPluggedPorts = 0;
	int32_t iRetVal;

	(void)Dptx_Core_Get_Stream_Mode(pstDptx, &bMST_Supported);

	iRetVal = Dptx_Edid_Read_EDID_I2C_Over_Aux(pstDptx);

	pstDptx->bSideBand_MSG_Supported = (iRetVal == DPTX_RETURN_NO_ERROR) ? (bool)true : (bool)false;

	for(ucDP_Idx = 0; ucDP_Idx < (uint8_t)PHY_INPUT_STREAM_MAX; ucDP_Idx++) {
		memset(pstDptx->paucEdidBuf_Entry[ucDP_Idx], 0, DPTX_EDID_BUFLEN);
	}

	if (pstDptx->bSideBand_MSG_Supported) {

		ucNumOfPluggedPorts = (bMST_Supported) ? 0 : 1;

		iRetVal = (bMST_Supported) ? Dptx_Ext_Get_TopologyState(pstDptx, (bool)DPTX_CLEAR_PAYLOAD, &ucNumOfPluggedPorts) : DPTX_RETURN_NO_ERROR;
		if (iRetVal !=  DPTX_RETURN_NO_ERROR) {
			ucNumOfPluggedPorts = 1U;
		}

		if (ucNumOfPluggedPorts == 1U) {
			(void)Dptx_Edid_Read_EDID_I2C_Over_Aux(pstDptx);

			(void)memcpy(pstDptx->paucEdidBuf_Entry[0], pstDptx->pucEdidBuf, DPTX_EDID_BUFLEN);
		} else {
			for (ucDP_Idx = 0; ucDP_Idx < ucNumOfPluggedPorts; ucDP_Idx++) {
				(void)Dptx_Edid_Read_EDID_Over_Sideband_Msg(pstDptx, ucDP_Idx);
				(void)memcpy(pstDptx->paucEdidBuf_Entry[ucDP_Idx], pstDptx->pucEdidBuf, DPTX_EDID_BUFLEN);
			}
		}
		dptx_dbg("%d %s connected", ucNumOfPluggedPorts, ucNumOfPluggedPorts == 1 ? "Ext. monitor is":"Ext. monitors are");
	} else {

		iRetVal = (pstDptx->pvPanel_Topology_CallBack != NULL) ? pstDptx->pvPanel_Topology_CallBack(&ucNumOfPluggedPorts) : DPTX_RETURN_NO_ERROR;
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			ucNumOfPluggedPorts = 1;
		}

		dptx_dbg("%d %s connected", ucNumOfPluggedPorts, ucNumOfPluggedPorts == 1 ? " panel is":" panels are");
	}

	(void)Dptx_Ext_Set_Stream_Mode(pstDptx, ucNumOfPluggedPorts);

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Intr_Register_HPD_Callback(struct Dptx_Params *pstDptx, Dptx_HPD_Intr_Callback HPD_Intr_Callback)
{
	pstDptx->pvHPD_Intr_CallBack = HPD_Intr_Callback;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Intr_Register_Panel_Callback(struct Dptx_Params *pstDptx, Dptx_Panel_Topology_Callback Panel_Topology_CallBack)
{
	pstDptx->pvPanel_Topology_CallBack = Panel_Topology_CallBack;

	return DPTX_RETURN_NO_ERROR;
}

int32_t Dptx_Intr_Handle_HotUnplug(struct Dptx_Params *pstDptx)
{
	pstDptx->eLast_HPDStatus = HPD_STATUS_UNPLUGGED;

	(void)Dptx_Core_Disable_PHY_XMIT(pstDptx, pstDptx->ucMax_Lanes);

	(void)Dptx_Core_Set_PHY_PowerState(pstDptx, PHY_POWER_DOWN_PHY_CLOCK);

	(void)Dptx_Core_Get_PHY_BUSY_Status(pstDptx, pstDptx->ucMax_Lanes);

	return DPTX_RETURN_NO_ERROR;
}

bool dptx_intr_get_hotplug_status(struct Dptx_Params *pstDptx)
{
	bool hpd_plugged = (bool)false;
	uint32_t hpd_status;

	hpd_status = Dptx_Reg_Readl(pstDptx, DPTX_HPDSTS);
	if (((hpd_status & (uint32_t)DPTX_HPDSTS_STATE_MASK) >> DPTX_HPDSTS_STATE_SHIFT) == 7u) {
		dptx_dbg("Hot plugged -> HPD_STATUS[0x%08x]: 0x%08x", DPTX_HPDSTS, hpd_status);
		hpd_plugged = (bool)true;
	}

	return hpd_plugged;
}
