// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_comm.h"

#ifdef ENABLE_VPU_DRV_HEVCENC2

#include "vpu_rm.h"
#include "vpu_devices.h"
#include "vpu_mgr.h"
#include "vpu_mgr_common.h"
#include "vpu_mgr_context.h"
#include "vpu_hevc_enc2_mgr_sys.h"
#include "vpu_hevc_enc2_mgr.h"

#define dlog_henc2(msg...)  		V_DBG(VPU_DBG_INFO, "[HEVC_ENC2][INFO]: " msg)
#define detail_henc2(msg...)  	V_DBG(VPU_DBG_DETAIL, "[HEVC_ENC2][DETAIL]:" msg)
#define seq_henc2(msg...)     	V_DBG(VPU_DBG_SEQUENCE, "[HEVC_ENC2][SEQ]: " msg)
#define err_henc2(msg...)      	V_DBG(VPU_DBG_ERROR, "[HEVC_ENC2][ERR]: " msg)

//to avoid potential issues caused by stack frames, parameters are stored in the heap instead of using local variables.
//This value is assigned to ip_param of each driver's vpu_drv_insfo_t.
typedef struct vpu_hevc_enc2_papam_t {
	vpu_hevc_enc_ctrl_log_status_t enc_log;
	hevc_enc_init_t enc_init;
	hevc_enc_initial_info_t enc_initialInfo;
	hevc_enc_buffer_t enc_buffer;
	hevc_enc_header_t enc_header;
	hevc_enc_input_t enc_input;
	hevc_enc_output_t enc_output;
} vpu_hevc_enc2_papam_t;

static vpu_mgr_t *vpu_hevc_enc2_mgr_ctx = INITIAL_NULL;

static void __iomem *hevc_enc2_vidsys_conf_reg = INITIAL_NULL;

//static unsigned char str_cmd_time_out[] = "hevc enc vmgr_internal_handler timed_out";
static unsigned char str_cmd_init[] = "hevc enc2 vmgr_internal_handler timed_out";

#define HEVC_ENC2_ACCESSPOINT_PATH	 "/proc/hevc_enc_2"

#if !defined(USE_ACCESS_POINT)
#if DEFINED_CONFIG_VENC
extern int tcc_vpu_hevc_enc2(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2);
#endif
#endif //!defined(USE_ACCESS_POINT)

#define HEVC_AUD_SIZE	7
static const unsigned char hevcAudData[HEVC_AUD_SIZE] = {0x00, 0x00, 0x00, 0x01, 0x46, 0x01, 0x10};

static int tcc_vpu_hevc_enc2_l(vpu_accesspoint_t *vpu_ap, int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2) VPU_NO_SANITIZE_CFI
{
	return vpu_ap->tccfp_vpu_enc(Op, pHandle, pParam1, pParam2);
}

static int vmgr_hevc_enc2_internal_handler(void)
{
	int ret, ret_code = RETCODE_INTR_DETECTION_NOT_ENABLED;
	vpu_mgr_t *mgr_ctx = vpu_hevc_enc2_mgr_ctx;

	if (mgr_ctx != NULL) {
		unsigned long jtimeout;

		jtimeout = msecs_to_jiffies(mgr_ctx->each_ip->internal_timeout_ms);
		if (jtimeout > LONG_MAX) {
			jtimeout = LONG_MAX;
		}

		if (atomic_read(&mgr_ctx->oper_intr) > 0) {
			V_DBG(VPU_DBG_INTERRUPT, "Success-1: vpu hevc enc operation!! (isr cnt:)");
			ret_code = RETCODE_SUCCESS;
		} else {
			ret = wait_event_interruptible_timeout(mgr_ctx->oper_wq, atomic_read(&mgr_ctx->oper_intr) > 0, jtimeout);

			if (atomic_read(&mgr_ctx->oper_intr) > 0) {
				V_DBG(VPU_DBG_INTERRUPT, "Success-2: vpu hevc enc operation!! (isr cnt:)");
				ret_code = RETCODE_SUCCESS;
			} else {
				//FIXME : add debugingg cmd, frame count, frame length
				/*
				V_DBG(VPU_DBG_ERROR,
				"[CMD 0x%x][ret:%d]: vpu timed_out(ref %d msec) => oper_intr[%d], [%d]th frame len %d",
				  mgr_ctx->current_cmd,
				  ret,
				  timeout,
				  atomic_read(&mgr_ctx->oper_intr),
				  vmgr_hevc_enc_data.nDecode_Cmd,
				  vmgr_hevc_enc_data.szFrame_Len
				);
				*/
				vetc_dump_reg_all(mgr_ctx->base_addr, "vmgr_internal_handler timed_out");

				ret_code = RETCODE_CODEC_EXIT;
			}
		}

		atomic_set(&mgr_ctx->oper_intr, 0);

		//FIXME : add debugging for irq and register
		//vmgr_hevc_enc_status_clear(mgr_ctx->base_addr);
	}

	return ret_code;
}

static int vmgr_hevc_enc2_init(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;
	vpu_hevc_enc2_papam_t *ip_param = (vpu_hevc_enc2_papam_t *)drv_info->ip_param;

	codec_handle_t encHandle;
	hevc_enc_init_t *pEncInit = &ip_param->enc_init;
	hevc_enc_initial_info_t *pEncInitialInfo = &ip_param->enc_initialInfo;

	venc_v3_init_t *arg_init = (venc_v3_init_t *)cmd_info->args;
	venc_v3_init_in_t *arg_init_in = &arg_init->input;
	venc_v3_init_out_t *arg_init_out = &arg_init->output;

	//debug settings for vpu_lib: echo 0xPXABBB > /sys/module/vpu/parameters/vdbg_lib
	unsigned int vpulib_dbg_param = get_vpu_lib_dbg_param();

	dlog_henc2("[id:%u] VPU_ENC_INIT start", drv_id);

	pEncInit->m_RegBaseAddr[VA] = (codec_addr_t) mgr_ctx->base_addr;
	pEncInit->m_Memcpy = vetc_memcpy;
	pEncInit->m_Memset = (void (*) (void *, int, unsigned int, unsigned int))vetc_memset;
	pEncInit->m_Interrupt = (int (*) (void))mgr_ctx->each_ip->internal_handler;
	pEncInit->m_Ioremap = (void * (*) (phys_addr_t, unsigned int))vetc_ioremap;
	pEncInit->m_Iounmap = (void (*) (void *))vetc_iounmap;
	pEncInit->m_reg_read = (unsigned int (*)(void *, unsigned int))vetc_reg_read;
	pEncInit->m_reg_write = (void (*)(void *, unsigned int, unsigned int))vetc_reg_write;
	pEncInit->m_Usleep = (void (*)(unsigned int, unsigned int))vetc_usleep;

	pEncInit->m_iBitstreamFormat = vmgr_get_bitstream_format(arg_init_in->codec_id, VPU_OP_TYPE_ENC);
	pEncInit->m_iPicWidth = arg_init_in->pic_width;
	pEncInit->m_iPicHeight = arg_init_in->pic_height;
	pEncInit->m_iFrameRate = arg_init_in->frame_rate;
	pEncInit->m_iTargetKbps = arg_init_in->target_kbps;
	pEncInit->m_iKeyInterval = arg_init_in->key_interval;
	pEncInit->m_bCbCrInterleaveMode = arg_init_in->cbcr_interleave_mode;
	pEncInit->m_uiEncOptFlags = 0U;

	pEncInit->m_BitWorkAddr[VPU_PA] = alloc_info->bitwork_buf.addr[VPU_PA];
	pEncInit->m_BitWorkAddr[VPU_KVA] = alloc_info->bitwork_buf.addr[VPU_KVA];

	pEncInit->m_BitstreamBufferAddr[VPU_PA] = alloc_info->bitstream_buf.addr[VPU_PA];
	pEncInit->m_BitstreamBufferAddr[VPU_KVA] = alloc_info->bitstream_buf.addr[VPU_KVA];
	pEncInit->m_iBitstreamBufferSize = alloc_info->bitstream_buf.size;

	dlog_henc2("[id:%u] Init In =>Memcpy(0x%px),Memset(0x%px),Interrupt(0x%px),remap(0x%px),unmap(0x%px),read(0x%px),write(0x%px),sleep(0x%px)||workbuff(0x%px/0x%px),Reg(0x%px/0x%px), format(%d),W:H(%d:%d),Fps(%d),Bps(%d),Keyi(%d),Stream(0x%px/0x%px, %d), interleave:%d",
		drv_id,
		pEncInit->m_Memcpy,
		pEncInit->m_Memset,
		pEncInit->m_Interrupt,
		pEncInit->m_Ioremap,
		pEncInit->m_Iounmap,
		pEncInit->m_reg_read,
		pEncInit->m_reg_write,
		pEncInit->m_Usleep,
		pEncInit->m_BitWorkAddr[VPU_PA],
		pEncInit->m_BitWorkAddr[VPU_KVA],
		mgr_ctx->base_addr,
		pEncInit->m_RegBaseAddr[VPU_KVA],
		pEncInit->m_iBitstreamFormat,
		pEncInit->m_iPicWidth,
		pEncInit->m_iPicHeight,
		pEncInit->m_iFrameRate,
		pEncInit->m_iTargetKbps,
		pEncInit->m_iKeyInterval,
		pEncInit->m_BitstreamBufferAddr[PA],
		pEncInit->m_BitstreamBufferAddr[VA],
		pEncInit->m_iBitstreamBufferSize,
		pEncInit->m_bCbCrInterleaveMode);

	if ((arg_init_in->rc.initial_qp == 0) &&
		(arg_init_in->rc.intra_qp_max == 0) && (arg_init_in->rc.inter_qp_max == 0) &&
		(arg_init_in->rc.intra_qp_min == 0) && (arg_init_in->rc.inter_qp_min == 0)) {
		pEncInit->m_iUseSpecificRcOption = 0;
		pEncInit->m_stRcInit.m_Reserved[0] = 0;
	} else {
		pEncInit->m_iUseSpecificRcOption = 1;
		pEncInit->m_stRcInit.m_Reserved[0] = 1;

		if (arg_init_in->rc.initial_qp > 0) {
			pEncInit->m_stRcInit.m_Reserved[0] |= 0x0100U;
			pEncInit->m_stRcInit.m_Reserved[5] = arg_init_in->rc.initial_qp;
		}

		if (arg_init_in->rc.intra_qp_max > 0) {
			pEncInit->m_stRcInit.m_Reserved[0] |= 0x0020U;
			pEncInit->m_stRcInit.m_Reserved[2] = arg_init_in->rc.intra_qp_max;
		}

		if (arg_init_in->rc.inter_qp_max > 0) {
			pEncInit->m_stRcInit.m_Reserved[0] |= 0x0080U;
			pEncInit->m_stRcInit.m_Reserved[4] = arg_init_in->rc.inter_qp_max;
		}

		if (arg_init_in->rc.intra_qp_min > 0) {
			pEncInit->m_stRcInit.m_Reserved[0] |= 0x0010U;
			pEncInit->m_stRcInit.m_Reserved[1] = arg_init_in->rc.intra_qp_min;
		}

		if (arg_init_in->rc.inter_qp_min > 0) {
			pEncInit->m_stRcInit.m_Reserved[0] |= 0x0040U;
			pEncInit->m_stRcInit.m_Reserved[3] = arg_init_in->rc.inter_qp_min;
		}
	}

	//debug settings for vpu_lib: echo 0xPXABBB > /sys/module/vpu/parameters/vdbg_lib
	// Check if debugging is enabled using VPU_DBG_LIB_USE_CB_PRINTK (P part)
	if ((vpulib_dbg_param & VPU_DBG_LIB_USE_CB_PRINTK) == VPU_DBG_LIB_USE_CB_PRINTK) {
		unsigned int codec_ip;

		// Extract codec_ip (A part), please refer to enum vpu_ip_type
		// VPU_IP_C7(D6) = 1, VPU_IP_4KD2 = 2, VPU_IP_HEVC_ENC = 3, VPU_IP_HEVC_ENC2 = 4, VPU_IP_JPU_C6 = 5, VPU_IP_HEVC_DEC
		codec_ip = (vpulib_dbg_param & 0x00F000U) >> 12;
		V_DBG(VPU_DBG_ERROR, "[VPU_HEVC_ENC2] codec_ip: %d", codec_ip);

		// Check if codec_ip matches desired value
		if (codec_ip == VPU_IP_HEVC_ENC2) {
			// Extract log_mask (BBB part)
			unsigned int log_mask = (vpulib_dbg_param & 0x000FFFU);

			V_DBG(VPU_DBG_ERROR, "[VPU_HEVC_ENC2] log_mask: %d (%x)", log_mask, log_mask);
			ip_param->enc_log.pfLogPrintCb = (void (*)(const char *, ...))vpu_printk;
			ip_param->enc_log.stLogLevel.bVerbose = (log_mask & 1U) ? 1 : 0;
			ip_param->enc_log.stLogLevel.bDebug   = (log_mask & 2U) ? 1 : 0;
			ip_param->enc_log.stLogLevel.bInfo	  = (log_mask & 4U) ? 1 : 0;
			ip_param->enc_log.stLogLevel.bWarn	  = (log_mask & 8U) ? 1 : 0;
			ip_param->enc_log.stLogLevel.bError   = (log_mask & 16U) ? 1 : 0;
			ip_param->enc_log.stLogLevel.bAssert  = (log_mask & 32U) ? 1 : 0;
			ip_param->enc_log.stLogLevel.bFunc	  = (log_mask & 64U) ? 1 : 0;
			ip_param->enc_log.stLogLevel.bTrace   = (log_mask & 128U) ? 1 : 0;
			ret = tcc_vpu_hevc_enc2_l(vpu_ap, VPU_HEVC_ENC_CTRL_LOG_STATUS, NULL, (void *)(&ip_param->enc_log), (void *)NULL);
		}
	}

	ret = tcc_vpu_hevc_enc2_l(vpu_ap, VPU_ENC_INIT, (codec_handle_t *)&encHandle, (void *)pEncInit, (void *)pEncInitialInfo);
	if (ret != RETCODE_SUCCESS) {
		err_henc2("[id:%u] Init failed with ret(0x%x)", drv_id, ret);
		if (ret != RETCODE_CODEC_EXIT) {
			vetc_dump_reg_all((char *)mgr_ctx->base_addr, str_cmd_init);
		}
	}

	if ((ret != RETCODE_CODEC_EXIT) && (encHandle != 0)) {
		drv_info->handle = encHandle;
		arg_init_out->min_frame_buffer_count = pEncInitialInfo->m_iMinFrameBufferCount;
		arg_init_out->min_frame_buffer_size = pEncInitialInfo->m_iMinFrameBufferSize;
	}

	dlog_henc2("[id:%u] init Done Handle(0x%x)", drv_id, encHandle);

	return ret;
}

static int vmgr_hevc_enc2_register_framebuffer(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;
	vpu_hevc_enc2_papam_t *ip_param = (vpu_hevc_enc2_papam_t *)drv_info->ip_param;

	hevc_enc_buffer_t *pEncBuffer = &ip_param->enc_buffer;

	pEncBuffer->m_FrameBufferStartAddr[PA] = alloc_info->frame_buf.addr[VPU_PA];
	pEncBuffer->m_FrameBufferStartAddr[VA] = alloc_info->frame_buf.addr[VPU_KVA];

	dlog_henc2("[id:%u] Register a frame buffer w PA(0x%px)/VA(0x%px)",
		drv_id,
		pEncBuffer->m_FrameBufferStartAddr[PA],
		pEncBuffer->m_FrameBufferStartAddr[VA]);

	ret = tcc_vpu_hevc_enc2_l(vpu_ap, VPU_ENC_REG_FRAME_BUFFER, (codec_handle_t *) &pHandle, (void *)pEncBuffer, (void *)NULL);

	return ret;
}

static int vmgr_hevc_enc2_put_header(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_hevc_enc2_papam_t *ip_param = (vpu_hevc_enc2_papam_t *)drv_info->ip_param;

	venc_v3_putheader_t *arg_putheader = (venc_v3_putheader_t *)cmd_info->args;
	hevc_enc_header_t *pEncHeader = &ip_param->enc_header;

	dlog_henc2("[id:%u] VPU_ENC_PUT_HEADER In, codec id:%d, bitstream addr:%x/%x, size:%d, header type:0x%x",
				drv_id,
				arg_putheader->bitstream_buffer_addr[VPU_PA],
				arg_putheader->bitstream_buffer_addr[VPU_KVA],
				arg_putheader->bitstream_buffer_size,
				arg_putheader->header_type);

	if (drv_info->codec_id == VCODEC_ID_HEVC) {
		if ((arg_putheader->header_type & VPU_HEADER_HEVC_VPS) != 0) {
			pEncHeader->m_iHeaderType |= HEVC_ENC_CODEOPT_ENC_VPS;
			dlog_henc2("[id:%u] add VPS Header", drv_id);
		}

		if ((arg_putheader->header_type & VPU_HEADER_HEVC_SPS) != 0) {
			pEncHeader->m_iHeaderType |= HEVC_ENC_CODEOPT_ENC_SPS;
			dlog_henc2("[id:%u] add SPS Header", drv_id);
		}

		if ((arg_putheader->header_type & VPU_HEADER_HEVC_PPS) != 0) {
			pEncHeader->m_iHeaderType |= HEVC_ENC_CODEOPT_ENC_PPS;
			dlog_henc2("[id:%u] add PPS Header", drv_id);
		}
	} else {
		err_henc2("[id:%u] not support codec header, codec id:%d, header type:0x%x", drv_id, drv_info->codec_id, pEncHeader->m_iHeaderType);
		ret = -1;
	}

	if (ret == 0) {
		pEncHeader->m_HeaderAddr[VPU_PA] = arg_putheader->bitstream_buffer_addr[VPU_PA];
		pEncHeader->m_HeaderAddr[VPU_KVA] = arg_putheader->bitstream_buffer_addr[VPU_KVA];
		pEncHeader->m_iHeaderSize = arg_putheader->bitstream_buffer_size;

		ret = tcc_vpu_hevc_enc2_l(vpu_ap, VPU_ENC_PUT_HEADER, (codec_handle_t *) &pHandle, (void *)pEncHeader, (void *)NULL);
		if (ret == RETCODE_SUCCESS) {
			arg_putheader->bitstream_buffer_addr[VPU_PA] = pEncHeader->m_HeaderAddr[VPU_PA];
			arg_putheader->bitstream_buffer_addr[VPU_KVA] = pEncHeader->m_HeaderAddr[VPU_KVA];
			arg_putheader->bitstream_buffer_size = pEncHeader->m_iHeaderSize;

			dlog_henc2("[id:%u] VPU_ENC_PUT_HEADER out, bitstream addr:%x/%x, size:%d",
				drv_id,
				arg_putheader->bitstream_buffer_addr[VPU_PA],
				arg_putheader->bitstream_buffer_addr[VPU_KVA],
				arg_putheader->bitstream_buffer_size);
		}

		dlog_henc2("[id:%u] VPU_ENC_PUT_HEADER done, ret:%d", drv_id, ret);
	}

	return ret;
}

static int vmgr_hevc_enc2_encode(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_hevc_enc2_papam_t *ip_param = (vpu_hevc_enc2_papam_t *)drv_info->ip_param;

	venc_v3_encode_t *arg_encode = (venc_v3_encode_t *)cmd_info->args;
	venc_v3_encode_in_t *arg_init_in = &arg_encode->input;
	venc_v3_encode_out_t *arg_init_out = &arg_encode->output;

	hevc_enc_input_t *pEncInput = &ip_param->enc_input;
	hevc_enc_output_t *pEncOutput = &ip_param->enc_output;

	detail_henc2("[id:%u] VPU_ENC_ENCODE in", drv_id, ret);

	detail_henc2("[id:%u] encode input, pic_y_addr:0x%x, pic_cb_addr:0x%x, pic_cr_addr:0x%x", drv_id, arg_init_in->pic_y_addr, arg_init_in->pic_cb_addr, arg_init_in->pic_cr_addr);
	detail_henc2("[id:%u] force_i_picture:%d, skip_picture:%d, quant_param:%d", drv_id, arg_init_in->force_i_picture, arg_init_in->skip_picture, arg_init_in->quant_param);
	detail_henc2("[id:%u] bitstream_buffer_addr:0x%x/0x%x, bitstream_buffer_size:%d", drv_id, arg_init_in->bitstream_buffer_addr[VPU_PA], arg_init_in->bitstream_buffer_addr[VPU_KVA], arg_init_in->bitstream_buffer_size);
	detail_henc2("[id:%u] change_rc_param_flag:%d, change_target_kbps:%d, change_framerate:%d, change_key_interval:%d", drv_id, arg_init_in->change_rc_param_flag, arg_init_in->change_target_kbps, arg_init_in->change_framerate, arg_init_in->change_key_interval);

	pEncInput->m_PicYAddr = arg_init_in->pic_y_addr;
	pEncInput->m_PicCbAddr = arg_init_in->pic_cb_addr;
	pEncInput->m_PicCrAddr = arg_init_in->pic_cr_addr;

	pEncInput->m_iForceIPicture = arg_init_in->force_i_picture;
	pEncInput->m_iSkipPicture = arg_init_in->skip_picture;
	pEncInput->m_iQuantParam = arg_init_in->quant_param;

	pEncInput->m_BitstreamBufferAddr[VPU_PA] = arg_init_in->bitstream_buffer_addr[VPU_PA];
	pEncInput->m_BitstreamBufferAddr[VPU_KVA] = arg_init_in->bitstream_buffer_addr[VPU_KVA];
	pEncInput->m_iBitstreamBufferSize = arg_init_in->bitstream_buffer_size;

	if ((arg_init_in->change_rc_param_flag & VENC_V3_RC_FLAG_ENABLE) != 0) {
		pEncInput->m_iChangeRcParamFlag |= 0x01;
	}

	if ((arg_init_in->change_rc_param_flag & VENC_V3_RC_FLAG_BITRATE) != 0) {
		pEncInput->m_iChangeRcParamFlag |= (0x01 << 1);
	}

	if ((arg_init_in->change_rc_param_flag & VENC_V3_RC_FLAG_FRAMERATE) != 0) {
		pEncInput->m_iChangeRcParamFlag |= (0x01 << 2);
	}

	if ((arg_init_in->change_rc_param_flag & VENC_V3_RC_FLAG_KEY_INTERVAL) != 0) {
		pEncInput->m_iChangeRcParamFlag |= (0x01 << 3);
	}

	pEncInput->m_iChangeTargetKbps = arg_init_in->change_target_kbps;
	pEncInput->m_iChangeFrameRate = arg_init_in->change_framerate;
	pEncInput->m_iChangeKeyInterval = arg_init_in->change_key_interval;

	ret = tcc_vpu_hevc_enc2_l(vpu_ap, VPU_ENC_ENCODE, (codec_handle_t *)&pHandle, (void *)pEncInput, (void *)pEncOutput);
	if (ret == RETCODE_SUCCESS) {
		arg_init_out->encoded_stream_addr[VPU_PA] = pEncOutput->m_BitstreamOutAddr[VPU_PA];
		arg_init_out->encoded_stream_addr[VPU_KVA] = pEncOutput->m_BitstreamOutAddr[VPU_KVA];
		arg_init_out->encoded_stream_size = pEncOutput->m_iBitstreamOutSize;

		switch (pEncOutput->m_iPicType) {
		case PIC_TYPE_I:
			arg_init_out->pic_type = VPU_PICTURE_I;
		break;

		case PIC_TYPE_P:
			arg_init_out->pic_type = VPU_PICTURE_P;
		break;

		case PIC_TYPE_B:
			arg_init_out->pic_type = VPU_PICTURE_B;
		break;

		case PIC_TYPE_IDR:
			arg_init_out->pic_type = VPU_PICTURE_IDR;
		break;

		case PIC_TYPE_B_PB:
			arg_init_out->pic_type = VPU_PICTURE_B_PB;
		break;

		default:
			arg_init_out->pic_type = VPU_PICTURE_MAX;
		break;
		}

		detail_henc2("[id:%u] VPU_ENC_ENCODE success, pic type:%d, addr:%x/%x, size:%d",
			drv_id, pEncOutput->m_iPicType, pEncOutput->m_BitstreamOutAddr[VPU_PA], pEncOutput->m_BitstreamOutAddr[VPU_KVA], pEncOutput->m_iBitstreamOutSize);
	}

	detail_henc2("[id:%u] VPU_ENC_ENCODE done, ret:%d", drv_id, ret);

	return ret;
}

static int vmgr_hevc_enc2_close(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;

	dlog_henc2("[id:%u] VPU_ENC_CLOSE in", drv_id, ret);
	ret = tcc_vpu_hevc_enc2_l(vpu_ap, VPU_ENC_CLOSE, (codec_handle_t *)&pHandle, (void *)NULL, (void *)NULL);

	dlog_henc2("[id:%u] VPU_ENC_CLOSE done, ret:%d", drv_id, ret);

	return ret;
}

static int vmgr_hevc_enc2_encode_process(void *vpu_private, enum vpu_cmd_type cmd, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)vpu_private;

	dlog_henc2("[id:%u] %s(%d)/start mgr_ctx:%p, drv_id:%d", drv_id, vmgr_cmd_name(cmd), cmd, mgr_ctx, drv_id);

	switch (cmd) {
	case VPU_CMD_ENC_INIT:
	{
		ret = vmgr_hevc_enc2_init(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_ENC_REG_FRAME_BUFFER:
	{
		ret = vmgr_hevc_enc2_register_framebuffer(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_ENC_PUT_HEADER:
	{
		ret = vmgr_hevc_enc2_put_header(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_ENC_ENCODE:
	{
		ret = vmgr_hevc_enc2_encode(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_ENC_CLOSE:
	{
		ret = vmgr_hevc_enc2_close(mgr_ctx, cmd_info, drv_info);
	}
	break;

	default:
	{
		err_henc2("[id:%u] unknown command(%d)", drv_id, cmd);
		ret = -1;
	}
	}

	ret = vmgr_convert_retcode(ret);
	return ret;
}

static int vmgr_hevc_enc2_get_buffer_size(void *vpu_private, enum vmgr_buffer_type buf_type, vpu_drv_info_t *drv_info)
{
	int size = 0;

	//for unused buffers, they must be set to 0.
	switch (buf_type) {
	case VMGR_BUF_BITSTREAM:
		size = ALIGNED_BUFF(VPU_HEVC_ENC_STREAM_BUF_SIZE, 4096u);
	break;

	case VMGR_BUF_NUM_OF_BITSTREAM:
		size = 1;
	break;

	case VMGR_BUF_BITWORK:
		size = ALIGNED_BUFF(VPU_HEVC_ENC_WORK_CODE_BUF_SIZE, 4096u);
	break;

	case VMGR_BUF_FRAMEBUF:
		size = 1; //use framebuffer, calculating from vpu_mgr.c using min framebuffer count, size
	break;

	case VMGR_BUF_SPSPPS:
		size = 0;
	break;

	case VMGR_BUF_USERDATA:
		size = 0;
	break;

	case VMGR_BUF_SLICE:
		size = 0;
	break;

	case VMGR_BUF_MBDATA:
	{
		size = 0;
	}
	break;

	case VMGR_BUF_MESEARCH:
	{
		size = 0;
	}
	break;

	case VMGR_BUF_SLICEINFO:
	{
		size = 0;
	}
	break;

	default:
		size = 0;
	break;
	}

	return size;
}

static irqreturn_t vmgr_hevc_enc2_isr_handler(int irq, void *vpu_private)
{
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)vpu_private;

	atomic_inc(&mgr_ctx->oper_intr);

	wake_up_interruptible(&mgr_ctx->oper_wq);

	return IRQ_HANDLED;
}

extern vmgr_clock_t vpu_hevc_enc2_clock;

static vpu_ip_module_t vpu_hevc_enc2_module = {
	.ip_type = VPU_IP_HEVC_ENC2,
	.cq_type = VPU_CQ_LEGACY,
	.cq_depth = 1,
	.buffer_mode = VPU_BS_MODE_LINEARBUFFR,
	.internal_timeout_ms = 200,
	.enc_param_size = sizeof(vpu_hevc_enc2_papam_t),
	.dec_param_size = 0,
	.internal_handler = vmgr_hevc_enc2_internal_handler,
	//codec id, codec name(string), profile(string), level(string), width(unsigned int), height(unsigned int), fps(unsigned int)
	.dec_capa = {{VCODEC_ID_NONE, NULL, NULL, NULL, 0, 0, 0}},
	.enc_capa = {{VCODEC_ID_HEVC, CODEC_NAME_HEVC, "Main", "5.0", 3840, 2160, 30},
				{VCODEC_ID_NONE, NULL, NULL, NULL, 0, 0, 0}},
	.clock_ctrl = &vpu_hevc_enc2_clock,
	.proc_encode = vmgr_hevc_enc2_encode_process,
	.proc_decode = NULL,
	.proc_get_buffer_size = vmgr_hevc_enc2_get_buffer_size,
	.isr_handler = vmgr_hevc_enc2_isr_handler,
	.cq_func = NULL,
	.ip_private = NULL,
	.access_point_path = HEVC_ENC2_ACCESSPOINT_PATH,
};

int vmgr_hevc_enc2_probe(struct platform_device *pdev)
{
	int ret = 0;
	vpu_mgr_t *mgr_ctx = NULL;

	mgr_ctx = vmgr_alloc(&vpu_hevc_enc2_module);
	if (mgr_ctx != NULL) {
		hevc_enc2_vidsys_conf_reg = (void __iomem *)of_iomap(pdev->dev.of_node, 1);
		if (hevc_enc2_vidsys_conf_reg == NULL) {
			err_henc2("hevc_enc2_vidsys_conf_reg: NULL");
		} else {
			vetc_reg_write((void *)hevc_enc2_vidsys_conf_reg, 0x84, 0x9c9a3000);
			detail_henc2("Video sub-system cfg reg (0x%px) : Sec. AXI (0x%x)", hevc_enc2_vidsys_conf_reg, vetc_reg_read((void *) hevc_enc2_vidsys_conf_reg, 0x84));
		}

#if !defined(USE_ACCESS_POINT)
		mgr_ctx->access_point->tccfp_vpu_dec = NULL;
		mgr_ctx->access_point->tccfp_vpu_enc = tcc_vpu_hevc_enc;
#else
		mgr_ctx->access_point = NULL;
#endif

		ret = vmgr_probe(mgr_ctx, pdev, VPU_HEVC_ENC2_MGR_NAME);
		if (ret == 0) {
			//assigning VPU manager context to avoid mutex race condition in interrupt handler
			vpu_hevc_enc2_mgr_ctx = (vpu_mgr_t *)vmgr_get_context(VPU_IP_HEVC_ENC2);
		}
	}

	platform_set_drvdata(pdev, mgr_ctx);
	return ret;
}

EXPORT_SYMBOL(vmgr_hevc_enc2_probe);

int vmgr_hevc_enc2_remove(struct platform_device *pdev)
{
	int ret = 0;
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)platform_get_drvdata(pdev);

	if (mgr_ctx->each_ip->ip_private != NULL) {
		VPU_free(mgr_ctx->each_ip->ip_private);
		mgr_ctx->each_ip->ip_private = NULL;
	}

	ret = vmgr_remove(mgr_ctx, pdev);
	vmgr_free(mgr_ctx);
	vpu_hevc_enc2_mgr_ctx = NULL;
	return ret;
}

EXPORT_SYMBOL(vmgr_hevc_enc2_remove);

#if defined(CONFIG_PM)
int vmgr_hevc_enc2_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)platform_get_drvdata(pdev);

	ret = vmgr_suspend(mgr_ctx, pdev, state);
	return ret;
}

EXPORT_SYMBOL(vmgr_hevc_enc2_suspend);

int vmgr_hevc_enc2_resume(struct platform_device *pdev)
{
	int ret = 0;
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)platform_get_drvdata(pdev);

	ret = vmgr_resume(mgr_ctx, pdev);
	return ret;
}

EXPORT_SYMBOL(vmgr_hevc_enc2_resume);
#endif	//CONFIG_PM

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
MODULE_SOFTDEP("pre: vpu_lib jpu_lib hevc_lib vpu_4k_d2_lib vpu_hevc_enc_lib vpu");

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vpu hevc enc2 manager");
MODULE_LICENSE("GPL");

#endif //ENABLE_VPU_DRV_HEVCENC2
