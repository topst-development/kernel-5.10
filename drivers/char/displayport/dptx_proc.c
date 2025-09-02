// SPDX-License-Identifier: GPL-2.0-or-later OR MIT
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/stat.h>

#include "dptx_v14.h"
#include "dptx_reg.h"
#include "dptx_dbg.h"
#include "dptx_drm_dp_addition.h"

#define DPTX_DEBUGFS_BUF_SIZE		1024
#define DATA_DUMP_BUF_SIZE		DPTX_EDID_BUFLEN

static int dptx_proc_open(struct inode *pinode, struct file *filp)
{
	int32_t iRet = DPTX_RETURN_NO_ERROR;

	(void)pinode;
	(void)filp;

	if (try_module_get(THIS_MODULE) == (bool)false) {
		/*For KCS*/
		iRet = -ENODEV;
	}

	return iRet;
}

static int dptx_proc_close(struct inode *pinode, struct file *filp)
{
	(void)pinode;
	(void)filp;

	module_put(THIS_MODULE);

	return 0;
}

static void dptx_proc_print_edid_Buf(const u8 *pucBuf, u32 uiLength)
{
	uint32_t uiOffset;

	if (uiLength < (uint32_t)DATA_DUMP_BUF_SIZE) {
		for (uiOffset = 0; uiOffset < uiLength; uiOffset++) {
			if ((uiOffset % 16U) == 0U) {
				/*For KCS*/
				dptx_dump("\n0x%02x:", uiOffset);
			} else {
				/*For KCS*/
				dptx_dump(" 0x%02x", pucBuf[uiOffset]);
			}
		}
	} else {
		/*For KCS*/
		dptx_err("Invalid dump length as %d", uiLength);
	}
}

static void dptx_ext_proc_handle_input(const struct file *pfilp,
				       struct Dptx_Params **ppstDptx,
				       char **ppcTxtBuf)
{
	char *pcOutput_Buf = NULL;
	struct Dptx_Params *pstDptx;

	pstDptx = (struct Dptx_Params *)PDE_DATA(file_inode(pfilp));

	pcOutput_Buf = (char *)devm_kzalloc(pstDptx->dev, DPTX_DEBUGFS_BUF_SIZE, GFP_KERNEL);
	if (pcOutput_Buf == NULL) {
		/*For KCS*/
		dptx_err("Could not allocate output state buffer");
	}

	*ppstDptx = pstDptx;
	*ppcTxtBuf = pcOutput_Buf;
}

static void dptx_ext_proc_handle_input_ex(const struct file *pfilp,
					  struct Dptx_Params **ppstDptx,
					  char **ppcTxtBuf)
{
	char *pcOutput_Buf = NULL;
	struct Dptx_Params *pstDptx;

	pstDptx = (struct Dptx_Params *)PDE_DATA(file_inode(pfilp));

	pcOutput_Buf = (char *)devm_kzalloc(pstDptx->dev, DPTX_DEBUGFS_BUF_SIZE, GFP_KERNEL);
	if (pcOutput_Buf == NULL) {
		/*For KCS*/
		dptx_err("Could not allocate output state buffer");
	}

	*ppstDptx = pstDptx;
	*ppcTxtBuf = pcOutput_Buf;
}

static ssize_t dptx_ext_proc_handle_output(const struct file *pfilp,
					   char __user *pusr_buf,
					   size_t stcnt, loff_t *poff_set,
					   char *pcBuf, size_t lFmtSize)
{
	ssize_t ulSize = 0;
	const struct Dptx_Params *pstDptx;

	pstDptx = (struct Dptx_Params *)PDE_DATA(file_inode(pfilp));

	ulSize = simple_read_from_buffer(pusr_buf, stcnt, poff_set, (void *)pcBuf, lFmtSize);

	devm_kfree(pstDptx->dev, pcBuf);

	return ulSize;
}

static int32_t dptx_ext_proc_get_edid(struct Dptx_Params *pstDptx,
				      bool *pbSinkSupportsMST,
				      bool *pbSinkSupportsEdid,
				      uint8_t *pucNumOfPorts)
{
	bool bSinkSupportsMST = (bool)false;
	uint8_t ucNumOfPorts = 0, ucDpIdx;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	iRetVal = Dptx_Ext_Get_Stream_Mode(pstDptx, &bSinkSupportsMST, &ucNumOfPorts);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* For KCS */
		goto return_funcs;
	}

	if ((ucNumOfPorts == 0U) || (ucNumOfPorts > (uint8_t)PHY_INPUT_STREAM_MAX)) {
		dptx_err("Invalid the Num. of Ports %d -> Get Port composition first\n", ucNumOfPorts);
		iRetVal = DPTX_RETURN_ENODEV;

		goto return_funcs;
	}

	if ((bSinkSupportsMST == (bool)true) && (ucNumOfPorts <= 1U)) {
		dptx_err("MST on but the Num. of Port is %d -> MST mode to off\n", ucNumOfPorts);
		bSinkSupportsMST = (bool)false;
	}

	*pbSinkSupportsEdid = (bool)true;
	*pbSinkSupportsMST = bSinkSupportsMST;
	*pucNumOfPorts = ucNumOfPorts;

	if (bSinkSupportsMST) {
		for (ucDpIdx = 0; ucDpIdx < ucNumOfPorts; ucDpIdx++) {
			iRetVal = Dptx_Edid_Read_EDID_Over_Sideband_Msg(pstDptx, ucDpIdx);
			if (iRetVal == DPTX_RETURN_NO_ERROR) {
				/*For KCS*/
				dptx_proc_print_edid_Buf(pstDptx->pucEdidBuf,      (DPTX_ONE_EDID_BLK_LEN * 2));
			} else {
				dptx_info("Sink doesn't support EDID");
				*pbSinkSupportsEdid = (bool)false;
				break;
			}
		}
	} else {
		iRetVal = Dptx_Edid_Read_EDID_I2C_Over_Aux(pstDptx);
		if (iRetVal == DPTX_RETURN_NO_ERROR) {
			/*For KCS*/
			dptx_proc_print_edid_Buf(pstDptx->pucEdidBuf, (DPTX_ONE_EDID_BLK_LEN * 2));
		} else {
			dptx_info("Sink doesn't support EDID");
			*pbSinkSupportsEdid = (bool)false;
		}
	}

return_funcs:
	return iRetVal;
}

static int32_t dptx_ext_proc_get_link_status(struct Dptx_Params *pstDptx,
					     bool *pbSinkSupportsMST,
					     bool *pbPrevLinkTState)
{
	bool bSinkSupportsMST = (bool)false;
	uint8_t ucNumOfPorts = 0;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	*pbPrevLinkTState = (bool)false;
	*pbSinkSupportsMST = (bool)false;

	iRetVal = Dptx_Ext_Get_Stream_Mode(pstDptx, &bSinkSupportsMST, &ucNumOfPorts);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* For KCS */
		goto return_funcs;
	}

	if ((ucNumOfPorts == 0U) || (ucNumOfPorts > (uint8_t)PHY_INPUT_STREAM_MAX)) {
		dptx_err("Invalid the Num. of Ports %d -> Get Port composition first\n", ucNumOfPorts);
		iRetVal = DPTX_RETURN_ENODEV;

		goto return_funcs;
	}

	if ((bSinkSupportsMST == (bool)true) && (ucNumOfPorts <= 1U)) {
		dptx_err("MST on but the Num. of Port is %d -> MST mode to off\n", ucNumOfPorts);
		bSinkSupportsMST = (bool)false;
	}

	*pbSinkSupportsMST = bSinkSupportsMST;

	if (dptx_link_get_linktraining_status(pstDptx)) {
		*pbPrevLinkTState = (bool)true;

		goto return_funcs;
	}

	iRetVal = Dptx_Link_Perform_BringUp(pstDptx);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* For KCS */
		goto return_funcs;
	}

	iRetVal = Dptx_Link_Perform_Training(pstDptx, pstDptx->ucMax_Rate, pstDptx->ucMax_Lanes);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* For KCS */
		goto return_funcs;
	}

	if (bSinkSupportsMST == (bool)true) {
		iRetVal = Dptx_Ext_Set_Topology_Configuration(pstDptx);
		if (iRetVal !=  DPTX_RETURN_NO_ERROR) {
			/* For KCS */
			goto return_funcs;
		}
	}

return_funcs:
	return iRetVal;
}

static int32_t dptx_ext_proc_set_video_timing(struct Dptx_Params *dptx,
					      uint32_t stream_idx,
					      uint32_t vic,
					      uint32_t video_format)
{
	int32_t ret_val = DPTX_RETURN_NO_ERROR;
	struct dptx_dtd_params dtd_params;

	ret_val = Dptx_VidIn_Fill_Dtd(&dtd_params, vic, 60000, video_format);
	if (ret_val != DPTX_RETURN_NO_ERROR) {
		/* For KCS */
		dptx_err("Can't find VIC %d from dtd", (u32)vic);
	}

	if (DPTX_RETURN_SUCCESS(ret_val)) {
		/* For KCS */
		ret_val = Dptx_VidIn_Set_Detailed_Timing(dptx, (uint8_t)(stream_idx & 0xFFU), &dtd_params);
	}

	if (DPTX_RETURN_SUCCESS(ret_val)) {
		/* For KCS */
		ret_val = Dptx_VidIn_Set_Stream_Enable(dptx, (bool)true, (uint8_t)(stream_idx & 0xFFU));
	}

	return ret_val;
}

static int32_t dptx_ext_proc_get_video_timing(struct Dptx_Params *pstDptx,
					      bool *pbSinkSupportsMST,
					      uint16_t *pusHActive,
					      uint16_t *pusVActive)
{
	bool bSinkSupportsMST = (bool)false;
	uint8_t ucNumOfPorts = 0, ucDpIdx;
	uint16_t usH_Active[PHY_INPUT_STREAM_MAX] = { 0, };
	uint16_t usV_Active[PHY_INPUT_STREAM_MAX] = { 0, };
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	struct dptx_dtd_params stDtd_Params;

	*pbSinkSupportsMST = (bool)false;

	iRetVal = Dptx_Ext_Get_Stream_Mode(pstDptx, &bSinkSupportsMST, &ucNumOfPorts);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* For KCS */
		goto return_funcs;
	}

	if ((ucNumOfPorts == 0U) || (ucNumOfPorts > (uint8_t)PHY_INPUT_STREAM_MAX)) {
		dptx_err("Invalid the Num. of Ports %d -> Get Port composition first\n", ucNumOfPorts);
		iRetVal = DPTX_RETURN_ENODEV;

		goto return_funcs;
	}

	if ((bSinkSupportsMST == (bool)true) && (ucNumOfPorts <= 1U)) {
		dptx_err("MST on but the Num. of Port is %d -> MST mode to off\n", ucNumOfPorts);
		bSinkSupportsMST = (bool)false;
	}

	*pbSinkSupportsMST = bSinkSupportsMST;

	for (ucDpIdx = 0; ucDpIdx < ucNumOfPorts; ucDpIdx++) {
		iRetVal = Dptx_VidIn_Get_Configured_Timing(pstDptx, ucDpIdx, &stDtd_Params);
		if (iRetVal != DPTX_RETURN_NO_ERROR) {
			/* For KCS */
			break;
		}

		usH_Active[ucDpIdx] = stDtd_Params.h_active;
		usV_Active[ucDpIdx] = stDtd_Params.v_active;
	}

	pusHActive[PHY_INPUT_STREAM_0] = usH_Active[PHY_INPUT_STREAM_0];
	pusVActive[PHY_INPUT_STREAM_0] = usV_Active[PHY_INPUT_STREAM_0];
	pusHActive[PHY_INPUT_STREAM_1] = usH_Active[PHY_INPUT_STREAM_1];
	pusVActive[PHY_INPUT_STREAM_1] = usV_Active[PHY_INPUT_STREAM_1];
	pusHActive[PHY_INPUT_STREAM_2] = usH_Active[PHY_INPUT_STREAM_2];
	pusVActive[PHY_INPUT_STREAM_2] = usV_Active[PHY_INPUT_STREAM_2];
	pusHActive[PHY_INPUT_STREAM_3] = usH_Active[PHY_INPUT_STREAM_3];
	pusVActive[PHY_INPUT_STREAM_3] = usV_Active[PHY_INPUT_STREAM_3];

return_funcs:
	return iRetVal;
}

static ssize_t dptx_ext_proc_read_hpd_state(struct file *filp,
					    char __user *usr_buf,
					    size_t cnt, loff_t *off_set)
{
	char *pcHpd_Buf = NULL;
	int32_t iFmt_Size;
	ssize_t ulSize = 0;
	size_t lFmtSize;
	bool hpd_plugged = false;
	struct Dptx_Params *pstDptx;

	dptx_ext_proc_handle_input(filp, &pstDptx, &pcHpd_Buf);

	if (pstDptx == NULL) {
		dptx_err("NULL of Drv Ptr is returned from input handling");

		goto return_funcs;
	}

	if (pcHpd_Buf == NULL) {
		dptx_err("NULL of Txt Ptr is returned from input handling");

		goto return_funcs;
	}

	if (dptx_intr_get_hotplug_status(pstDptx) == HPD_STATUS_PLUGGED) {
		/* For KCS */
		hpd_plugged = true;
	}
	iFmt_Size = scnprintf(pcHpd_Buf, (size_t)DPTX_DEBUGFS_BUF_SIZE, "%s\n",
			     (hpd_plugged) ? "Hot plugged" : "Hot unplugged");

	lFmtSize = (iFmt_Size > 0) ? (size_t)iFmt_Size : (size_t)0U;

	ulSize = dptx_ext_proc_handle_output(filp, usr_buf, cnt, off_set, pcHpd_Buf, lFmtSize);

return_funcs:
	return ulSize;
}

static ssize_t dptx_ext_proc_read_port_composition(struct file *filp,
						   char __user *usr_buf,
						   size_t cnt, loff_t *off_set)
{
	char *pcTopology_Buf = NULL;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	int32_t iFmt_Size;
	ssize_t ulSize = 0;
	size_t lFmtSize;
	struct Dptx_Params *pstDptx;
	bool hpd_plugged = false;

	dptx_ext_proc_handle_input(filp, &pstDptx, &pcTopology_Buf);

	if (pstDptx == NULL) {
		dptx_err("NULL of Drv Ptr is returned from input handling");

		goto return_funcs;
	}

	if (pcTopology_Buf == NULL) {
		dptx_err("NULL of Txt Ptr is returned from input handling");
		goto return_funcs;
	}

	if (dptx_intr_get_hotplug_status(pstDptx) == HPD_STATUS_PLUGGED) {
		/* For KCS */
		hpd_plugged = true;
	}
	if (!hpd_plugged) {
		dptx_err("Hot unplugged..");

		goto return_funcs;
	}

	iRetVal = Dptx_Intr_Get_Port_Composition(pstDptx, (bool)DPTX_KEEP_PAYLOAD);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* For KCS */
		goto return_funcs;
	}

	iFmt_Size = scnprintf(pcTopology_Buf, (size_t)DPTX_DEBUGFS_BUF_SIZE, "%s : %d %s %s connected\n",
							(pstDptx->bMultStreamTransport == (bool)true) ? "MST mode":"SST mode",
							pstDptx->ucNumOfPorts,
							(pstDptx->bSideBand_MSG_Supported == (bool)true) ? "Ext. monitor":"SerDes",
							(pstDptx->ucNumOfPorts == 1U) ? "port is":"ports are");

	lFmtSize = (iFmt_Size > 0) ? (size_t)iFmt_Size : (size_t)0U;

	ulSize = dptx_ext_proc_handle_output(filp, usr_buf, cnt, off_set, pcTopology_Buf, lFmtSize);

return_funcs:
	return ulSize;
}

static ssize_t dptx_ext_proc_read_edid_data(struct file *filp,
					    char __user *usr_buf,
					    size_t cnt, loff_t *off_set)
{
	bool bSinkSupportsEDID = (bool)false;
	bool bSinkSupportsMST = (bool)false;
	char *pcEdid_Buf = NULL;
	uint8_t ucNumOfPluggedPorts = 0;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	int32_t iFmt_Size;
	ssize_t ulSize = 0;
	size_t lFmtSize;
	struct Dptx_Params *pstDptx;
	bool hpd_plugged = false;

	dptx_ext_proc_handle_input(filp, &pstDptx, &pcEdid_Buf);

	if (pstDptx == NULL) {
		dptx_err("NULL of Drv Ptr is returned from input handling");

		goto return_funcs;
	}

	if (pcEdid_Buf == NULL) {
		dptx_err("NULL of Txt Ptr is returned from input handling");

		goto return_funcs;
	}

	if (dptx_intr_get_hotplug_status(pstDptx) == HPD_STATUS_PLUGGED) {
		/* For KCS */
		hpd_plugged = true;
	}
	if (!hpd_plugged) {
		dptx_err("Hot unplugged..");

		goto return_funcs;
	}

	iRetVal = dptx_ext_proc_get_edid(pstDptx, &bSinkSupportsMST,     &bSinkSupportsEDID, &ucNumOfPluggedPorts);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* For KCS */
		goto return_funcs;
	}

	iFmt_Size = scnprintf(pcEdid_Buf, (size_t)DPTX_DEBUGFS_BUF_SIZE, "%s : %s %d %s connected\n",
							(bSinkSupportsMST == (bool)true) ? "MST mode":"SST mode",
							(bSinkSupportsEDID == (bool)true) ? "Sink has EDID from":"Sink doesn't have EDID from",
							ucNumOfPluggedPorts,
							(ucNumOfPluggedPorts > 1U) ? "port is":"ports are");

	lFmtSize = (iFmt_Size > 0) ? (size_t)iFmt_Size : (size_t)0U;

	ulSize = dptx_ext_proc_handle_output(filp, usr_buf, cnt, off_set, pcEdid_Buf, lFmtSize);

return_funcs:
	return ulSize;
}

static ssize_t dptx_ext_proc_read_link_training_status(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set)
{
	bool bSink_MST_Supported = (bool)false;
	bool bPrevTrainingState = (bool)false;
	char *pcLinkT_Buf = NULL;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	int32_t iFmt_Size;
	ssize_t ulSize = 0;
	size_t lFmtSize;
	struct Dptx_Params *pstDptx;
	bool hpd_plugged = false;

	dptx_ext_proc_handle_input(filp, &pstDptx, &pcLinkT_Buf);

	if (pstDptx == NULL) {
		dptx_err("NULL of Drv Ptr is returned from input handling");

		goto return_funcs;
	}

	if (pcLinkT_Buf == NULL) {
		dptx_err("NULL of Txt Ptr is returned from input handling");

		goto return_funcs;
	}

	if (dptx_intr_get_hotplug_status(pstDptx) == HPD_STATUS_PLUGGED) {
		/* For KCS */
		hpd_plugged = true;
	}
	if (!hpd_plugged) {
		dptx_err("Hot unplugged..");

		goto return_funcs;
	}

	iRetVal = dptx_ext_proc_get_link_status(pstDptx, &bSink_MST_Supported, &bPrevTrainingState);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* For KCS */
		goto return_funcs;
	}

	iFmt_Size = scnprintf(pcLinkT_Buf, (size_t)DPTX_DEBUGFS_BUF_SIZE, "%s : link training %s with %s on %d lanes\n",
				(bSink_MST_Supported == (bool)true) ? "MST mode" : "SST mode",
				(bPrevTrainingState == (bool)true) ? "already successed" : "newly successed",
				(pstDptx->stDptxLink.ucLinkRate == (uint8_t)DPTX_PHYIF_CTRL_RATE_RBR) ? "RBR" :
				(pstDptx->stDptxLink.ucLinkRate == (uint8_t)DPTX_PHYIF_CTRL_RATE_HBR) ? "HBR" :
				(pstDptx->stDptxLink.ucLinkRate == (uint8_t)DPTX_PHYIF_CTRL_RATE_HBR2) ? "HB2" : "HBR3",
				pstDptx->stDptxLink.ucNumOfLanes);

	lFmtSize = (iFmt_Size > 0) ? (size_t)iFmt_Size : (size_t)0U;

	ulSize = dptx_ext_proc_handle_output(filp, usr_buf, cnt, off_set, pcLinkT_Buf, lFmtSize);

return_funcs:
	return  ulSize;
}

static ssize_t dptx_ext_proc_read_video_timing(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set)
{
	bool bSinkSupportsMST;
	char *pcVideoTiming_Buf = NULL;
	uint16_t usH_Active[PHY_INPUT_STREAM_MAX] = { 0, };
	uint16_t usV_Active[PHY_INPUT_STREAM_MAX] = { 0, };
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	int32_t iFmt_Size;
	ssize_t ulSize = 0;
	size_t lFmtSize;
	struct Dptx_Params *pstDptx;
	bool hpd_plugged = false;

	dptx_ext_proc_handle_input(filp, &pstDptx, &pcVideoTiming_Buf);

	if (pstDptx == NULL) {
		dptx_err("NULL of Drv Ptr is returned from input handling");

		goto return_funcs;
	}

	if (pcVideoTiming_Buf == NULL) {
		dptx_err("NULL of Txt Ptr is returned from input handling");

		goto return_funcs;
	}

	if (dptx_intr_get_hotplug_status(pstDptx) == HPD_STATUS_PLUGGED) {
		/* For KCS */
		hpd_plugged = true;
	}
	if (!hpd_plugged) {
		dptx_err("Hot unplugged..");

		goto return_funcs;
	}

	iRetVal = dptx_ext_proc_get_video_timing(pstDptx,
						 &bSinkSupportsMST,
						 &usH_Active[PHY_INPUT_STREAM_0],
						 &usV_Active[PHY_INPUT_STREAM_0]);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* For KCS */
		goto return_funcs;
	}

	iFmt_Size = scnprintf(pcVideoTiming_Buf, (size_t)DPTX_DEBUGFS_BUF_SIZE, "%s : 1st %d x %d, 2nd : %d x %d, 3rd : %d x %d, 4th : %d x %d\n",
						(bSinkSupportsMST == (bool)true) ? "MST mode":"SST mode",
						usH_Active[PHY_INPUT_STREAM_0],
						usV_Active[PHY_INPUT_STREAM_0],
						usH_Active[PHY_INPUT_STREAM_1],
						usV_Active[PHY_INPUT_STREAM_1],
						usH_Active[PHY_INPUT_STREAM_2],
						usV_Active[PHY_INPUT_STREAM_2],
						usH_Active[PHY_INPUT_STREAM_3],
						usV_Active[PHY_INPUT_STREAM_3]);

	lFmtSize = (iFmt_Size > 0) ? (size_t)iFmt_Size : (size_t)0U;

	ulSize = dptx_ext_proc_handle_output(filp, usr_buf, cnt, off_set, pcVideoTiming_Buf, lFmtSize);

return_funcs:
	return ulSize;
}

static ssize_t dptx_ext_proc_write_video_timing(struct file *filp, const char __user *buffer, size_t cnt, loff_t *off_set)
{
	char *video_timing_buf = NULL;
	int32_t scan_ret = 0;
	uint32_t video_code, stream_index, video_format;
	ssize_t size = 0;
	struct Dptx_Params *dptx = NULL;
	bool internal_ok = true;

	dptx_ext_proc_handle_input_ex(filp, &dptx, &video_timing_buf);

	if (dptx == NULL) {
		dptx_err("NULL of Drv Ptr is returned from input handling");

		internal_ok = false;
	}

	if (video_timing_buf == NULL) {
		dptx_err("NULL of Txt Ptr is returned from input handling");

		internal_ok = false;
	}

	if (internal_ok) {
		size = simple_write_to_buffer(video_timing_buf, cnt, off_set, buffer, cnt);
		if (size >= 0) {
			if ((size_t)size != cnt) {
				dptx_err("Can't get input data : %ld <-> %ld ", size, cnt);

				internal_ok = false;
			}
		} else {
			dptx_err("Can't get input data : %ld <-> %ld ", size, cnt);

			internal_ok = false;
		}

		video_timing_buf[cnt] = '\0';
	}

	if (internal_ok) {
		scan_ret = sscanf(video_timing_buf, "%u %u %u", &stream_index, &video_code, &video_format);
		if (scan_ret < 2) {
			dptx_err("Can't scan input data");
			internal_ok = false;
		}
	}

	if (internal_ok) {
		if (scan_ret == 2) {
			/* For KCS */
			video_format = (uint32_t)VIDEO_FORMAT_CEA_861;
		}

		dptx_info("Stream index : %u, Video code : %u, Video format: %u",
			  stream_index, video_code, video_format);

		(void)dptx_ext_proc_set_video_timing(dptx, stream_index, video_code, video_format);
	}

	if (video_timing_buf != NULL) {
		/* For KCS */
		devm_kfree(dptx->dev, video_timing_buf);
	}

	return size;
}

static ssize_t dptx_ext_proc_read_str_status(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set)
{
	ssize_t ulSize = 0;
	struct Dptx_Params *pstDptx;

	(void)cnt;
	(void)usr_buf;
	(void)off_set;

	pstDptx = (struct Dptx_Params *)PDE_DATA(file_inode(filp));

	(void)dptx_driver_suspend_core(pstDptx);

	mdelay(5000);

	(void)dptx_driver_resume_core(pstDptx);

	return ulSize;
}

static const struct proc_ops proc_fops_hpd_state = {
	.proc_open = dptx_proc_open,
	.proc_release = dptx_proc_close,
	.proc_read = dptx_ext_proc_read_hpd_state,
};

static const struct proc_ops proc_fops_topology_state = {
	.proc_open = dptx_proc_open,
	.proc_release = dptx_proc_close,
	.proc_read = dptx_ext_proc_read_port_composition,
};

static const struct proc_ops proc_fops_edid_data = {
	.proc_open = dptx_proc_open,
	.proc_release = dptx_proc_close,
	.proc_read = dptx_ext_proc_read_edid_data,
};

static const struct proc_ops proc_fops_linkT_data = {
	.proc_open = dptx_proc_open,
	.proc_release = dptx_proc_close,
	.proc_read = dptx_ext_proc_read_link_training_status,
};

static const struct proc_ops proc_fops_str_data = {
	.proc_open = dptx_proc_open,
	.proc_release = dptx_proc_close,
	.proc_read = dptx_ext_proc_read_str_status,
};

static const struct proc_ops proc_fops_video_data = {
	.proc_open = dptx_proc_open,
	.proc_release = dptx_proc_close,
	.proc_read = dptx_ext_proc_read_video_timing,
	.proc_write = dptx_ext_proc_write_video_timing,
};

int32_t Dptx_Ext_Proc_Interface_Init(struct Dptx_Params *pstDptx)
{
	pstDptx->pstDP_Proc_Dir = proc_mkdir("dptx_v14", NULL);
	if (pstDptx->pstDP_Proc_Dir == NULL) {
		/*For KCS*/
		dptx_err("Could't create file system @ /proc/dptx_v14");
	}

	pstDptx->pstDP_HPD_Dir = proc_create_data("hpd", ((umode_t)S_IFREG | (umode_t)0444), pstDptx->pstDP_Proc_Dir, &proc_fops_hpd_state, pstDptx);
	if (pstDptx->pstDP_HPD_Dir == NULL) {
		/*For KCS*/
		dptx_err("Could't create file system data @ /proc/dptx_v14/hpd");
	}

	pstDptx->pstDP_Topology_Dir = proc_create_data("topology", ((umode_t)S_IFREG | (umode_t)0444), pstDptx->pstDP_Proc_Dir, &proc_fops_topology_state, pstDptx);
	if (pstDptx->pstDP_Topology_Dir == NULL) {
		/*For KCS*/
		dptx_err("Could't create file system data @ /proc/dptx_v14/topology");
	}

	pstDptx->pstDP_EDID_Dir = proc_create_data("edid", ((umode_t)S_IFREG | (umode_t)0444), pstDptx->pstDP_Proc_Dir, &proc_fops_edid_data, pstDptx);
	if (pstDptx->pstDP_EDID_Dir == NULL) {
		/*For KCS*/
		dptx_err("Could't create file system data @ /proc/dptx_v14/edid");
	}

	pstDptx->pstDP_LinkT_Dir = proc_create_data("link", ((umode_t)S_IFREG | (umode_t)0444), pstDptx->pstDP_Proc_Dir, &proc_fops_linkT_data, pstDptx);
	if (pstDptx->pstDP_LinkT_Dir == NULL) {
		/*For KCS*/
		dptx_err("Could't create file system data @ /proc/dptx_v14/link");
	}

	pstDptx->pstDP_LinkT_Dir = proc_create_data("str", ((umode_t)S_IFREG | (umode_t)0444), pstDptx->pstDP_Proc_Dir, &proc_fops_str_data, pstDptx);
	if (pstDptx->pstDP_LinkT_Dir == NULL) {
		/*For KCS*/
		dptx_err("Could't create file system data @ /proc/dptx_v14/str");
	}

	pstDptx->pstDP_Video_Dir = proc_create_data("video", ((umode_t)S_IFREG | (umode_t)0666), pstDptx->pstDP_Proc_Dir, &proc_fops_video_data, pstDptx);
	if (pstDptx->pstDP_Video_Dir == NULL) {
		/*For KCS*/
		dptx_err("Could't create file system data @ /proc/dptx_v14/video");
	}

	return DPTX_RETURN_NO_ERROR;
}


