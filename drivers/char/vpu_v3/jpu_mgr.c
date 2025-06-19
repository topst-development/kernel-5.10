/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_comm.h"

#ifdef ENABLE_VPU_DRV_JPU_C6

#include "vpu_rm.h"
#include "vpu_devices.h"
#include "vpu_mgr.h"
#include "vpu_mgr_common.h"
#include "vpu_mgr_context.h"
#include "jpu_mgr_sys.h"
#include "jpu_mgr.h"

#define dlog_jpu(msg...)  	V_DBG(VPU_DBG_INFO, "[JPU_C6][INFO]:" msg)
#define detail_jpu(msg...)  V_DBG(VPU_DBG_DETAIL, "[JPU_C6][DETAIL]:" msg)
#define seq_jpu(msg...)     V_DBG(VPU_DBG_INFO, "[JPU_C6][SEQ]:" msg)
#define err_jpu(msg...)     V_DBG(VPU_DBG_ERROR, "[JPU_C6][ERR]:" msg)

#define JPU_REGISTER_DUMP

#define JPU_ACCESSPOINT_PATH	 "/proc/jpu"

//to avoid potential issues caused by stack frames, parameters are stored in the heap instead of using local variables.
//This value is assigned to ip_param of each driver's vpu_drv_info_t.
typedef struct jpu_c6_dec_papam_t {
	jpu_ctrl_log_status_t dec_log;
	jpu_dec_init_t dec_init;
	jpu_dec_initial_info_t dec_initialInfo;
	jpu_dec_input_t seq_input;
	jpu_dec_buffer_t dec_buffer;
	jpu_dec_buffer3_t dec_buffer3;
	jpu_dec_input_t dec_input;
	jpu_dec_output_t dec_output;
} jpu_c6_dec_papam_t;

typedef struct jpu_c6_enc_papam_t {
	jpu_ctrl_log_status_t enc_log;
	jpu_enc_init_t enc_init;
	jpu_enc_input_t enc_input;
	jpu_enc_output_t enc_output;
} jpu_c6_enc_papam_t;


static vpu_mgr_t *jpu_mgr_ctx = INITIAL_NULL;


#if !defined(USE_ACCESS_POINT)
#if DEFINED_CONFIG_VDEC
extern int tcc_jpu_dec(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2);
#endif

#if DEFINED_CONFIG_VENC
extern int tcc_jpu_enc(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2);
#endif

#endif //!defined(USE_ACCESS_POINT)

static int tcc_jpu_dec_l(vpu_accesspoint_t *vpu_ap, int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2) VPU_NO_SANITIZE_CFI
{
	return vpu_ap->tccfp_vpu_dec(Op, pHandle, pParam1, pParam2);
}

static int tcc_jpu_enc_l(vpu_accesspoint_t *vpu_ap, int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2) VPU_NO_SANITIZE_CFI
{
	return vpu_ap->tccfp_vpu_enc(Op, pHandle, pParam1, pParam2);
}

static int jmgr_internal_handler(void)
{
	int ret, ret_code = RETCODE_INTR_DETECTION_NOT_ENABLED;
	vpu_mgr_t *mgr_ctx = jpu_mgr_ctx;

	if (mgr_ctx != NULL) {
		int timeout = mgr_ctx->each_ip->internal_timeout_ms;
		unsigned long jtimeout;
		long long_max = LONG_MAX;

		jtimeout = msecs_to_jiffies(timeout);
		if (jtimeout > (unsigned long)long_max) {
			jtimeout = (unsigned long)long_max;
		}

		if (atomic_read(&mgr_ctx->oper_intr) > 0) {
			detail_jpu("Success 1: jpu operation!!");
			ret_code = RETCODE_SUCCESS;
		} else {
			ret = wait_event_interruptible_timeout(mgr_ctx->oper_wq, atomic_read(&mgr_ctx->oper_intr) > 0, (long)jtimeout);
			if (atomic_read(&mgr_ctx->oper_intr) > 0) {
				detail_jpu("Success 2: jpu operation!!");
				ret_code = RETCODE_SUCCESS;
			} else {
				static unsigned char fname[] = "jmgr_internal_handler timed_out";
				err_jpu("[CMD 0x%x][%ld]: jpu timed_out(ref %d msec) => oper_intr[%d]!!\n", timeout, atomic_read(&mgr_ctx->oper_intr));

				vetc_dump_reg_all(mgr_ctx->base_addr, fname);
				ret_code = RETCODE_CODEC_EXIT;
			}
		}

		atomic_set(&mgr_ctx->oper_intr, 0);
	}

	return ret_code;
}

static int jmgr_convert_returnType(int err)
{
	int ret;

	switch (err) {
	case JPG_RET_SUCCESS:
		ret = RETCODE_SUCCESS;
		break;

	case JPG_RET_FAILURE:
		ret = RETCODE_FAILURE;
		break;

	case JPG_RET_INVALID_HANDLE:
		ret = RETCODE_INVALID_HANDLE;
		break;

	case JPG_RET_INVALID_PARAM:
		ret = RETCODE_INVALID_PARAM;
		break;

	case JPG_RET_INVALID_COMMAND:
		ret = RETCODE_INVALID_COMMAND;
		break;

	case JPG_RET_FRAME_NOT_COMPLETE:
		ret = RETCODE_FRAME_NOT_COMPLETE;
		break;

	case JPG_RET_INVALID_FRAME_BUFFER:
		ret = RETCODE_INVALID_FRAME_BUFFER;
		break;

	case JPG_RET_INSUFFICIENT_FRAME_BUFFERS:
		ret = RETCODE_INSUFFICIENT_FRAME_BUFFERS;
		break;

	case JPG_RET_INVALID_STRIDE:
		ret = RETCODE_INVALID_STRIDE;
		break;

	case JPG_RET_WRONG_CALL_SEQUENCE:
		ret = RETCODE_WRONG_CALL_SEQUENCE;
		break;

	case JPG_RET_CALLED_BEFORE:
		ret = RETCODE_CALLED_BEFORE;
		break;

	case JPG_RET_NOT_INITIALIZED:
		ret = RETCODE_NOT_INITIALIZED;
		break;

	case JPG_RET_CODEC_EXIT:
		ret = RETCODE_CODEC_EXIT;
		break;

	case JPG_RET_SPEC_OUT:
		ret = RETCODE_CODEC_SPECOUT;
		break;

	case JPG_RET_INSUFFICIENT_BITSTREAM_BUF:
		ret = RETCODE_INSUFFICIENT_BITSTREAM_BUF;
		break;

	case JPG_RET_CODEC_FINISH:
		ret = RETCODE_CODEC_FINISH;
		break;

	default:
		ret = RETCODE_FAILURE;
		break;
	}

	return ret;
}

static int jmgr_enc_init(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int retEnc;
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;
	jpu_c6_enc_papam_t *ip_param = (jpu_c6_enc_papam_t *)drv_info->ip_param;

	int current_resolution;
	codec_handle_t encHandle;
	jpu_enc_init_t *pEncInit = &ip_param->enc_init;

	venc_v3_init_t *arg_init = (venc_v3_init_t *)cmd_info->args;
	venc_v3_init_in_t *arg_init_in = &arg_init->input;

	//debug settings for vpu_lib: echo 0xPXABBB > /sys/module/vpu/parameters/vdbg_lib
	unsigned int vpulib_dbg_param = get_vpu_lib_dbg_param();

	dlog_jpu("[id:%u] JPU_ENC_INIT start", drv_id);

	pEncInit->m_RegBaseVirtualAddr = (codec_addr_t) mgr_ctx->base_addr;
	pEncInit->m_Memcpy = (void*(*)(void *dest, const void *src, unsigned int count, unsigned int type)) vetc_memcpy;
	pEncInit->m_Memset = (void (*)(void *ptr, int value, unsigned int num, unsigned int type)) vetc_memset;
	pEncInit->m_Interrupt = (int (*)(void))mgr_ctx->each_ip->internal_handler;
	pEncInit->m_Ioremap = (void *(*)(phys_addr_t phy_addr, unsigned int size)) vetc_ioremap;
	pEncInit->m_Iounmap = (void (*)(void *virt_addr))vetc_iounmap;
	pEncInit->m_reg_read = (unsigned int (*)(void *base_addr, unsigned int offset)) vetc_reg_read;
	pEncInit->m_reg_write = (void (*)(void *base_addr, unsigned int offset, unsigned int data)) vetc_reg_write;

	pEncInit->m_iPicWidth = arg_init_in->pic_width;
	pEncInit->m_iPicHeight = arg_init_in->pic_height;

	switch (arg_init_in->yuv_format) {
	case VPU_SOURCE_YUV420:
		pEncInit->m_iSourceFormat = YUV_FORMAT_420;
	break;

	case VPU_SOURCE_YUV422:
		pEncInit->m_iSourceFormat = YUV_FORMAT_422;
	break;

	case VPU_SOURCE_YUV224:
		pEncInit->m_iSourceFormat = YUV_FORMAT_224;
	break;

	case VPU_SOURCE_YUV400:
		pEncInit->m_iSourceFormat = YUV_FORMAT_400;
	break;

	case VPU_SOURCE_YUV444:
		pEncInit->m_iSourceFormat = YUV_FORMAT_444;
	break;

	default:
		err_jpu("[id:%u] not support format:%d", drv_id, (int)arg_init_in->yuv_format);
		ret = -1; //not support format
	break;
	}

	if (ret == 0) {
		pEncInit->m_iEncQuality = arg_init_in->enc_quality;
		pEncInit->m_iCbCrInterleaveMode = arg_init_in->cbcr_interleave_mode;
		pEncInit->m_uiEncOptFlags = 0U;

		pEncInit->m_BitstreamBufferAddr[VPU_PA] = alloc_info->bitstream_buf.addr[VPU_PA];
		pEncInit->m_BitstreamBufferAddr[VPU_KVA] = alloc_info->bitstream_buf.addr[VPU_KVA];
		pEncInit->m_iBitstreamBufferSize = alloc_info->bitstream_buf.size;

		dlog_jpu("[id:%u] Init In => base:0x%x, regbase:0x%x, pic w:%d, h:%d, src format:%d, Quality:%d, bitstream addr:0x%x/0x%x, size:0x%x, interleaved:%d, option:0x%x",
			drv_id,
			mgr_ctx->base_addr,
			pEncInit->m_RegBaseVirtualAddr,
			pEncInit->m_iPicWidth,
			pEncInit->m_iPicHeight,
			pEncInit->m_iSourceFormat,
			pEncInit->m_iEncQuality,
			pEncInit->m_BitstreamBufferAddr[PA],
			pEncInit->m_BitstreamBufferAddr[VA],
			pEncInit->m_iBitstreamBufferSize,
			pEncInit->m_iCbCrInterleaveMode,
			pEncInit->m_uiEncOptFlags);


		//debug settings for vpu_lib: echo 0xPXABBB > /sys/module/vpu/parameters/vdbg_lib
		// Check if debugging is enabled using VPU_DBG_LIB_USE_CB_PRINTK (P part)
		if ((vpulib_dbg_param & VPU_DBG_LIB_USE_CB_PRINTK) == VPU_DBG_LIB_USE_CB_PRINTK) {
			unsigned int codec_ip;

			// Extract codec_ip (A part), please refer to enum vpu_ip_type
			// VPU_IP_C7(D6) = 1, VPU_IP_4KD2 = 2, VPU_IP_HEVC_ENC = 3, VPU_IP_HEVC_ENC2 = 4, VPU_IP_JPU_C6 = 5, VPU_IP_HEVC_DEC
			codec_ip = (vpulib_dbg_param & 0x00F000U) >> 12;
			V_DBG(VPU_DBG_ERROR, "[JPU_C6] codec_ip: %d", codec_ip);

			// Check if codec_ip matches desired value
			if (codec_ip == VPU_IP_JPU_C6) {
				// Extract log_mask (BBB part)
				unsigned int log_mask = (vpulib_dbg_param & 0x000FFFU);

				V_DBG(VPU_DBG_ERROR, "[JPU_C6] log_mask: %d (%x)", log_mask, log_mask);
				ip_param->enc_log.pfLogPrintCb = (void (*)(const char *, ...))vpu_printk;
				ip_param->enc_log.stLogLevel.bVerbose = (log_mask & 1U) ? 1 : 0;
				ip_param->enc_log.stLogLevel.bDebug   = (log_mask & 2U) ? 1 : 0;
				ip_param->enc_log.stLogLevel.bInfo	  = (log_mask & 4U) ? 1 : 0;
				ip_param->enc_log.stLogLevel.bWarn	  = (log_mask & 8U) ? 1 : 0;
				ip_param->enc_log.stLogLevel.bError   = (log_mask & 16U) ? 1 : 0; // 0x10
				ip_param->enc_log.stLogLevel.bAssert  = (log_mask & 32U) ? 1 : 0; // 0x20
				ip_param->enc_log.stLogLevel.bFunc	  = (log_mask & 64U) ? 1 : 0; // 0x40
				ip_param->enc_log.stLogLevel.bTrace   = (log_mask & 128U) ? 1 : 0; // 0x80
				ret = tcc_jpu_enc_l(vpu_ap, JPU_CTRL_LOG_STATUS, NULL, (void *)(&ip_param->enc_log), (void *)NULL);
			}
		}

		current_resolution = pEncInit->m_iPicWidth * pEncInit->m_iPicHeight;
		if (current_resolution > (1920 * 1080)) {
			mgr_ctx->each_ip->internal_timeout_ms = 5000;
		}

		retEnc = tcc_jpu_enc_l(vpu_ap, JPU_ENC_INIT, (codec_handle_t *)&encHandle, (void *)pEncInit, (void *)NULL);
		ret = jmgr_convert_returnType(retEnc);
		if ((ret != RETCODE_CODEC_EXIT) && (encHandle != 0)) {
			drv_info->handle = encHandle;
			dlog_jpu("[id:%u] JPU_ENC_INIT ok, handle:0x%x", drv_id, drv_info->handle);
		}
	}

	return ret;
}

static int jmgr_enc_encode(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int retEnc;
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	jpu_c6_enc_papam_t *ip_param = (jpu_c6_enc_papam_t *)drv_info->ip_param;

	venc_v3_encode_t *arg_encode = (venc_v3_encode_t *)cmd_info->args;
	venc_v3_encode_in_t *arg_init_in = &arg_encode->input;
	venc_v3_encode_out_t *arg_init_out = &arg_encode->output;

	jpu_enc_input_t *pEncInput = &ip_param->enc_input;
	jpu_enc_output_t *pEncOutput = &ip_param->enc_output;

	dlog_jpu("[id:%u] JPU_ENC_ENCODE in", drv_id, ret);

	dlog_jpu("[id:%u] encode input, pic_y_addr:0x%x, pic_cb_addr:0x%x, pic_cr_addr:0x%x", drv_id, arg_init_in->pic_y_addr, arg_init_in->pic_cb_addr, arg_init_in->pic_cr_addr);
	dlog_jpu("[id:%u] bitstream_buffer_addr:0x%x/0x%x, bitstream_buffer_size:%d", drv_id, arg_init_in->bitstream_buffer_addr[VPU_PA], arg_init_in->bitstream_buffer_addr[VPU_KVA], arg_init_in->bitstream_buffer_size);

	pEncInput->m_PicYAddr = arg_init_in->pic_y_addr;
	pEncInput->m_PicCbAddr = arg_init_in->pic_cb_addr;
	pEncInput->m_PicCrAddr = arg_init_in->pic_cr_addr;

	pEncInput->m_BitstreamBufferAddr[VPU_PA] = arg_init_in->bitstream_buffer_addr[VPU_PA];
	pEncInput->m_BitstreamBufferAddr[VPU_KVA] = arg_init_in->bitstream_buffer_addr[VPU_KVA];
	pEncInput->m_iBitstreamBufferSize = arg_init_in->bitstream_buffer_size;

	retEnc = tcc_jpu_enc_l(vpu_ap, JPU_ENC_ENCODE, (codec_handle_t *)&pHandle, (void *)pEncInput, (void *)pEncOutput);
	ret = jmgr_convert_returnType(retEnc);
	if (ret == RETCODE_SUCCESS) {
		arg_init_out->encoded_stream_addr[VPU_PA] = pEncOutput->m_BitstreamOut[VPU_PA];
		arg_init_out->encoded_stream_addr[VPU_KVA] = pEncOutput->m_BitstreamOut[VPU_KVA];
		arg_init_out->encoded_stream_size = pEncOutput->m_iBitstreamOutSize;

		dlog_jpu("[id:%u] JPU_ENC_ENCODE success, addr:%x/%x, size:%d", drv_id,
				pEncOutput->m_BitstreamOut[VPU_PA], pEncOutput->m_BitstreamOut[VPU_KVA], pEncOutput->m_iBitstreamOutSize);
	}

	return ret;
}

static int jmgr_enc_close(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int retEnc;
	int ret = 0;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;

	retEnc = tcc_jpu_enc_l(vpu_ap, JPU_ENC_CLOSE, (codec_handle_t *) &pHandle, (void *)NULL, (void *)NULL);
	ret = jmgr_convert_returnType(retEnc);

	return ret;
}

static int jmgr_encode_process(void *vpu_private, enum vpu_cmd_type cmd, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)vpu_private;

	switch (cmd) {
	case VPU_CMD_ENC_INIT:
	{
		ret = jmgr_enc_init(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_ENC_ENCODE:
	{
		ret = jmgr_enc_encode(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_ENC_CLOSE:
	{
		ret = jmgr_enc_close(mgr_ctx, cmd_info, drv_info);
	}
	break;

	default:
	{
		err_jpu("[id:%u] not supported command(0x%x)", drv_id, cmd);
		ret = -1;
	}
	break;
	}

	ret = vmgr_convert_retcode(ret);
	return ret;
}

static int jmgr_dec_init(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int retDec;
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;


	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;
	jpu_c6_dec_papam_t *ip_param = (jpu_c6_dec_papam_t *)drv_info->ip_param;

	codec_handle_t decHandle;
	jpu_dec_init_t *pDecInit = &ip_param->dec_init;

	vdec_v3_init_t *arg_init = (vdec_v3_init_t *)cmd_info->args;
	vdec_v3_init_in_t *arg_init_in = &arg_init->input;

	//debug settings for vpu_lib: echo 0xPXABBB > /sys/module/vpu/parameters/vdbg_lib
	unsigned int vpulib_dbg_param = get_vpu_lib_dbg_param();

	dlog_jpu("[id:%u] JPU_DEC_INIT start", drv_id);

	pDecInit->m_RegBaseVirtualAddr = (codec_addr_t) mgr_ctx->base_addr;
	pDecInit->m_Memcpy = (void*(*)(void *dest, const void *src, unsigned int count,	unsigned int type)) vetc_memcpy;
	pDecInit->m_Memset = (void (*)(void *ptr, int value, unsigned int num, unsigned int type)) vetc_memset;
	pDecInit->m_Interrupt = (int (*)(void))mgr_ctx->each_ip->internal_handler;
	pDecInit->m_Ioremap = (void *(*)(phys_addr_t phy_addr, unsigned int size)) vetc_ioremap;
	pDecInit->m_Iounmap = (void (*)(void *virt_addr))vetc_iounmap;
	pDecInit->m_reg_read = (unsigned int (*)(void *base_addr, unsigned int offset)) vetc_reg_read;
	pDecInit->m_reg_write = (void (*)(void *base_addr, unsigned int offset, unsigned int data)) vetc_reg_write;

	pDecInit->m_uiDecOptFlags = 0;

#if defined(ENABLE_SEQHEADER_BUFFER_CHANGE)
	//seqheader use vpu_decode_t instead of seqheader_t
	pDecInit->m_uiDecOptFlags |= (1 << 26);
#endif

	if (arg_init_in->output_format == VPU_OUTPUT_LINEAR_NV12) {
		pDecInit->m_iCbCrInterleaveMode = 1U;
	}

	pDecInit->m_BitstreamBufAddr[PA] = alloc_info->bitstream_buf.addr[VPU_PA];
	pDecInit->m_BitstreamBufAddr[VA] = alloc_info->bitstream_buf.addr[VPU_KVA];
	pDecInit->m_iBitstreamBufSize = alloc_info->bitstream_buf.size;

	dlog_jpu("[id:%u] Init In => Reg(%#x/%#x), Stream(%#x/%#x, %#x) Interleave: %d",
			drv_id,
			mgr_ctx->base_addr,
			pDecInit->m_RegBaseVirtualAddr,
			pDecInit->m_BitstreamBufAddr[PA],
			pDecInit->m_BitstreamBufAddr[VA],
			pDecInit->m_iBitstreamBufSize,
			pDecInit->m_iCbCrInterleaveMode);

	//debug settings for vpu_lib: echo 0xPXABBB > /sys/module/vpu/parameters/vdbg_lib
	// Check if debugging is enabled using VPU_DBG_LIB_USE_CB_PRINTK (P part)
	if ((vpulib_dbg_param & VPU_DBG_LIB_USE_CB_PRINTK) == VPU_DBG_LIB_USE_CB_PRINTK) {
		unsigned int codec_ip;

		// Extract codec_ip (A part), please refer to enum vpu_ip_type
		// VPU_IP_C7(D6) = 1, VPU_IP_4KD2 = 2, VPU_IP_HEVC_ENC = 3, VPU_IP_HEVC_ENC2 = 4, VPU_IP_JPU_C6 = 5, VPU_IP_HEVC_DEC
		codec_ip = (vpulib_dbg_param & 0x00F000U) >> 12;
		V_DBG(VPU_DBG_ERROR, "[JPU_C6] codec_ip: %d", codec_ip);

		// Check if codec_ip matches desired value
		if (codec_ip == VPU_IP_JPU_C6) {
			// Extract log_mask (BBB part)
			unsigned int log_mask = (vpulib_dbg_param & 0x000FFFU);

			V_DBG(VPU_DBG_ERROR, "[JPU_C6] log_mask: %d (%x)", log_mask, log_mask);
			ip_param->dec_log.pfLogPrintCb = (void (*)(const char *, ...))vpu_printk;
			ip_param->dec_log.stLogLevel.bVerbose = (log_mask & 1U) ? 1 : 0;
			ip_param->dec_log.stLogLevel.bDebug   = (log_mask & 2U) ? 1 : 0;
			ip_param->dec_log.stLogLevel.bInfo    = (log_mask & 4U) ? 1 : 0;
			ip_param->dec_log.stLogLevel.bWarn    = (log_mask & 8U) ? 1 : 0;
			ip_param->dec_log.stLogLevel.bError   = (log_mask & 16U) ? 1 : 0; // 0x10
			ip_param->dec_log.stLogLevel.bAssert  = (log_mask & 32U) ? 1 : 0; // 0x20
			ip_param->dec_log.stLogLevel.bFunc    = (log_mask & 64U) ? 1 : 0; // 0x40
			ip_param->dec_log.stLogLevel.bTrace   = (log_mask & 128U) ? 1 : 0; // 0x80
			ret = tcc_jpu_dec_l(vpu_ap, JPU_CTRL_LOG_STATUS, NULL, (void *)(&ip_param->dec_log), (void *)NULL);
		}
	}

	retDec = tcc_jpu_dec_l(vpu_ap, JPU_DEC_INIT, (codec_handle_t *)&decHandle, (void *)pDecInit, (void *)NULL);
	ret = jmgr_convert_returnType(retDec);
	if (ret != RETCODE_CODEC_EXIT && decHandle != 0) {
		drv_info->handle = decHandle;
	}

	dlog_jpu("[id:%u] VPU_DEC_INIT, ret:%d", drv_id, ret);
	return ret;
}

static int jmgr_dec_seqheader(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int retDec;
	int ret = 0;
	int width;
	int height;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	jpu_c6_dec_papam_t *ip_param = (jpu_c6_dec_papam_t *)drv_info->ip_param;

	jpu_dec_initial_info_t *pDecInitialInfo = &ip_param->dec_initialInfo;
	jpu_dec_input_t *pDecInput = &ip_param->seq_input;

	vdec_v3_seqheader_t *arg_seqheader = (vdec_v3_seqheader_t *)cmd_info->args;
	vdec_v3_seqheader_in_t *arg_seqheader_in = &arg_seqheader->input;

	dlog_jpu("[id:%u] seqheader use VDEC_DECODE_t, input size:%d", drv_id, arg_seqheader_in->bitstream_size);

#if defined(ENABLE_SEQHEADER_BUFFER_CHANGE)
	pDecInput->m_BitstreamDataAddr[VPU_PA] = arg_seqheader_in->bitstream_addr[VPU_PA];
	pDecInput->m_BitstreamDataAddr[VPU_KVA] = arg_seqheader_in->bitstream_addr[VPU_KVA];
	pDecInput->m_iBitstreamDataSize = arg_seqheader_in->bitstream_size;

	retDec = tcc_jpu_dec_l(vpu_ap, JPU_DEC_SEQ_HEADER, (codec_handle_t *)&pHandle, (void *)pDecInput, (void *)pDecInitialInfo);
#else
	{
		union {
			unsigned int ul_data;
			int *pi_data;	//NULL
			void *pv_data;
		} uarg;

		uarg.pi_data = NULL;
		uarg.ul_data = arg_seqheader_in->bitstream_size;

		retDec = tcc_jpu_dec_l(vpu_ap, JPU_DEC_SEQ_HEADER, (codec_handle_t *)&pHandle, (void *)(uarg.pv_data), (void *)pDecInitialInfo);
	}
#endif

	ret = jmgr_convert_returnType(retDec);
	detail_jpu("[id:%u] JPU_DEC_SEQ_HEADER out 0x%x :: res info(%d x %d), src_format(%d), Error_reason(%d), minFB(%d)",
					drv_id, ret,
					pDecInitialInfo->m_iPicWidth,
					pDecInitialInfo->m_iPicHeight,
					pDecInitialInfo->m_iSourceFormat,
					pDecInitialInfo->m_iErrorReason,
					pDecInitialInfo->m_iMinFrameBufferCount);
	if (ret == RETCODE_SUCCESS) {
		vdec_v3_initial_info_t *init_info = &arg_seqheader->output.initial_info;

		vetc_memset(init_info, 0x00, sizeof(vdec_v3_initial_info_t), 0);

		init_info->pic_width = pDecInitialInfo->m_iPicWidth;
		init_info->pic_height = pDecInitialInfo->m_iPicHeight;
		init_info->report_error_reason = pDecInitialInfo->m_iErrorReason;
		init_info->min_frame_buffer_count = pDecInitialInfo->m_iMinFrameBufferCount;

		switch (arg_seqheader_in->scale_factor) {
		case JPU_SCALE_FACTOR_1: //100% original
			ip_param->dec_buffer.m_iJPGScaleRatio = 0;
		break;

		case JPU_SCALE_FACTOR_1_DIV_2: //50 %
			ip_param->dec_buffer.m_iJPGScaleRatio = 1;
		break;

		case JPU_SCALE_FACTOR_1_DIV_4: //25 %
			ip_param->dec_buffer.m_iJPGScaleRatio = 2;
		break;

		case JPU_SCALE_FACTOR_1_DIV_8: //12.5 %
			ip_param->dec_buffer.m_iJPGScaleRatio = 3;
		break;

		default:
			ip_param->dec_buffer.m_iJPGScaleRatio = 0;
		break;
		}

		init_info->min_frame_buffer_size = pDecInitialInfo->m_iMinFrameBufferSize[ip_param->dec_buffer.m_iJPGScaleRatio];
		init_info->frame_buffer_format = 0;
		init_info->bitdepth = 8; // vpu c7 only support 8bit output

		init_info->mjpg_spec_info.mjpg_min_frameBufferSize[0] = pDecInitialInfo->m_iMinFrameBufferSize[0];
		init_info->mjpg_spec_info.mjpg_min_frameBufferSize[1] = pDecInitialInfo->m_iMinFrameBufferSize[1];
		init_info->mjpg_spec_info.mjpg_min_frameBufferSize[2] = pDecInitialInfo->m_iMinFrameBufferSize[2];
		init_info->mjpg_spec_info.mjpg_min_frameBufferSize[3] = pDecInitialInfo->m_iMinFrameBufferSize[3];

		switch (pDecInitialInfo->m_iSourceFormat) {
		case YUV_FORMAT_420:
			init_info->mjpg_spec_info.mjpg_source_format = VPU_SOURCE_YUV420;
		break;

		case YUV_FORMAT_422:
			init_info->mjpg_spec_info.mjpg_source_format = VPU_SOURCE_YUV422;
		break;

		case YUV_FORMAT_224:
			init_info->mjpg_spec_info.mjpg_source_format = VPU_SOURCE_YUV224;
		break;

		case YUV_FORMAT_444:
			init_info->mjpg_spec_info.mjpg_source_format = VPU_SOURCE_YUV444;
		break;

		case YUV_FORMAT_400:
			init_info->mjpg_spec_info.mjpg_source_format = VPU_SOURCE_YUV400;
		break;

		default:
			init_info->mjpg_spec_info.mjpg_source_format = VPU_SOURCE_YUV420;
		break;
		}

		vetc_memcpy(&drv_info->initial_info, init_info, sizeof(vdec_v3_initial_info_t), 0);

		dlog_jpu("[id:%u] JPU_DEC_SEQ_HEADER out ret:%d, min framebuffer cont:%d, size:%d, res info:%d x %d",
			drv_id, ret,
			init_info->min_frame_buffer_count,
			init_info->min_frame_buffer_size,
			init_info->pic_width,
			init_info->pic_height);

		width = pDecInitialInfo->m_iPicWidth;
		height = pDecInitialInfo->m_iPicHeight;
		if ((width < 16) || (width > VPU_LIMIT_PICWIDTH) || (height < 16) || (height > VPU_LIMIT_PICHEIGHT)) {
			err_jpu("[id:%u] not supported pic. width(%d) ,  height(%d)", drv_id, width, height);
			ret = RETCODE_INVALID_STRIDE;
		} else {
			if ((width * height) > (1920 * 1080)) {
				mgr_ctx->each_ip->internal_timeout_ms = 5000;
			}
		}

	}

	return ret;
}

static int jmgr_dec_register_framebuffer(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int retDec;
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;
	jpu_c6_dec_papam_t *ip_param = (jpu_c6_dec_papam_t *)drv_info->ip_param;

	jpu_dec_buffer_t *pDecBuffer = &ip_param->dec_buffer;

	pDecBuffer->m_FrameBufferStartAddr[VPU_PA] = alloc_info->frame_buf.addr[VPU_PA];
	pDecBuffer->m_FrameBufferStartAddr[VPU_KVA] = alloc_info->frame_buf.addr[VPU_KVA];
	pDecBuffer->m_iFrameBufferCount = alloc_info->framebuffer_count;
	//m_iJPGScaleRatio was set from VPU_DEC_SEQ_HEADER

	dlog_jpu("[id:%u] JPU_DEC_REG_FRAME_BUFFER in :: addr:0x%x/0x%x, min frame count:%d, scale factor:%d ",
		drv_id, pDecBuffer->m_FrameBufferStartAddr[VPU_PA], pDecBuffer->m_FrameBufferStartAddr[VPU_KVA],
		pDecBuffer->m_iFrameBufferCount, pDecBuffer->m_iJPGScaleRatio);

	retDec = tcc_jpu_dec_l(vpu_ap, JPU_DEC_REG_FRAME_BUFFER, (codec_handle_t *)&pHandle, (void *)pDecBuffer, (void *)NULL);
	detail_jpu("[id:%u] JPU_DEC_REG_FRAME_BUFFER out, ret:%d\n", drv_id, retDec);

	ret = jmgr_convert_returnType(retDec);
	return ret;
}

static int jmgr_dec_user_framebuffer(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int retDec;
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;
	int buffer_idx = 0;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	jpu_c6_dec_papam_t *ip_param = (jpu_c6_dec_papam_t *)drv_info->ip_param;

	vdec_v3_reg_framebuffer_t *arg_register = (vdec_v3_reg_framebuffer_t *)cmd_info->args;
	vdec_v3_reg_framebuffer_in_t *input = &arg_register->input;
	jpu_dec_buffer3_t *pDecBuffer = &ip_param->dec_buffer3;

	pDecBuffer->m_iFrameBufferCount = input->frame_buffer_count;
	for (buffer_idx = 0; buffer_idx < input->frame_buffer_count; buffer_idx++) {
		phys_addr_t paddr = 0U;
		void *vaddr = NULL;
		unsigned int size = 0U;

		paddr = (phys_addr_t)input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_Y].framebuffer[VPU_PA];
		size = input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_Y].size;
		vaddr = (void *) vetc_ioremap((phys_addr_t) paddr, PAGE_ALIGN(size));

		pDecBuffer->m_addrFrameBuffer[PA][buffer_idx][COMP_Y] = (codec_addr_t)paddr;
		pDecBuffer->m_addrFrameBuffer[VA][buffer_idx][COMP_Y] = (codec_addr_t)vaddr;
		detail_jpu("[id:%u] Y buffer %d, paddr:0x%x, vaddr:0x%x, size:%u", drv_id, buffer_idx, paddr, vaddr, size);

		paddr = (phys_addr_t)input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_CB].framebuffer[VPU_PA];
		size = input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_CB].size;
		vaddr = (void *) vetc_ioremap((phys_addr_t) paddr, PAGE_ALIGN(size));

		pDecBuffer->m_addrFrameBuffer[PA][buffer_idx][COMP_U] = (codec_addr_t)paddr;
		pDecBuffer->m_addrFrameBuffer[VA][buffer_idx][COMP_U] = (codec_addr_t)vaddr;
		detail_jpu("[id:%u] CB buffer %d, paddr:0x%x, vaddr:0x%x, size:%u", drv_id, buffer_idx, paddr, vaddr, size);

		paddr = (phys_addr_t)input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_CR].framebuffer[VPU_PA];
		size = input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_CB].size;
		vaddr = (void *) vetc_ioremap((phys_addr_t) paddr, PAGE_ALIGN(size));

		pDecBuffer->m_addrFrameBuffer[PA][buffer_idx][COMP_V] = (codec_addr_t)paddr;
		pDecBuffer->m_addrFrameBuffer[VA][buffer_idx][COMP_V] = (codec_addr_t)vaddr;
		detail_jpu("[id:%u] CR buffer %d, paddr:0x%x, vaddr:0x%x, size:%u", drv_id, buffer_idx, paddr, vaddr, size);
	}

	detail_jpu("[id:%u] JPU_DEC_REG_FRAME_BUFFER3 in", drv_id);
	retDec = tcc_jpu_dec_l(vpu_ap, JPU_DEC_REG_FRAME_BUFFER3, (codec_handle_t *) &pHandle, (void *)pDecBuffer, (void *)NULL);
	detail_jpu("[id:%u] JPU_DEC_REG_FRAME_BUFFER3, ret:%d", drv_id, retDec);
	ret = jmgr_convert_returnType(retDec);

	return ret;
}

static int jmgr_dec_decode(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int retDec;
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	jpu_c6_dec_papam_t *ip_param = (jpu_c6_dec_papam_t *)drv_info->ip_param;

	jpu_dec_input_t *pDecInput = &ip_param->dec_input;
	jpu_dec_output_t *pDecOutput = &ip_param->dec_output;
	jpu_dec_initial_info_t *pDecInitialInfo = &ip_param->dec_initialInfo;

	vdec_v3_decode_t *arg_decode = (vdec_v3_decode_t *)cmd_info->args;
	vdec_v3_decode_in_t *arg_decode_in = &arg_decode->input;
	vdec_v3_decode_out_t *arg_decode_out = &arg_decode->output;

	pDecInput->m_BitstreamDataAddr[VPU_PA] = arg_decode_in->bitstream_addr[VPU_PA];
	pDecInput->m_BitstreamDataAddr[VPU_KVA] = arg_decode_in->bitstream_addr[VPU_KVA];
	pDecInput->m_iBitstreamDataSize = arg_decode_in->bitstream_size;

	retDec = tcc_jpu_dec_l(vpu_ap, JPU_DEC_DECODE, (codec_handle_t *)&pHandle, (void *)pDecInput, (void *)pDecOutput);
	detail_jpu("[id:%u] JPU_DEC_DECODE, ret:%d", drv_id, retDec);

	ret = jmgr_convert_returnType(retDec);

	if (ret == RETCODE_SUCCESS) {
		arg_decode_out->display_out[VPU_PA][VPU_COMP_Y] = NULL;
		arg_decode_out->display_out[VPU_PA][VPU_COMP_U] = NULL;
		arg_decode_out->display_out[VPU_PA][VPU_COMP_V] = NULL;

		arg_decode_out->display_out[VPU_KVA][VPU_COMP_Y] = NULL;
		arg_decode_out->display_out[VPU_KVA][VPU_COMP_U] = NULL;
		arg_decode_out->display_out[VPU_KVA][VPU_COMP_V] = NULL;

		arg_decode_out->decoded_out[VPU_PA][VPU_COMP_Y] = pDecOutput->m_pCurrOut[VPU_PA][0];
		arg_decode_out->decoded_out[VPU_PA][VPU_COMP_U] = pDecOutput->m_pCurrOut[VPU_PA][1];
		arg_decode_out->decoded_out[VPU_PA][VPU_COMP_V] = pDecOutput->m_pCurrOut[VPU_PA][2];

		arg_decode_out->decoded_out[VPU_KVA][VPU_COMP_Y] = pDecOutput->m_pCurrOut[VPU_KVA][0];
		arg_decode_out->decoded_out[VPU_KVA][VPU_COMP_U] = pDecOutput->m_pCurrOut[VPU_KVA][1];
		arg_decode_out->decoded_out[VPU_KVA][VPU_COMP_V] = pDecOutput->m_pCurrOut[VPU_KVA][2];

		arg_decode_out->out_info.display_idx = pDecOutput->m_DecOutInfo.m_iDispOutIdx;
		arg_decode_out->out_info.decoded_status = vmgr_convert_decoding_status(pDecOutput->m_DecOutInfo.m_iDecodingStatus);
		arg_decode_out->out_info.display_status = VPU_DISP_STAT_SUCCESS;
		arg_decode_out->out_info.num_of_err_mbs = pDecOutput->m_DecOutInfo.m_iNumOfErrMBs;

		arg_decode_out->out_info.decoded_width = pDecOutput->m_DecOutInfo.m_iWidth;
		arg_decode_out->out_info.decoded_height = pDecOutput->m_DecOutInfo.m_iHeight;
		arg_decode_out->out_info.display_width = pDecOutput->m_DecOutInfo.m_iWidth;
		arg_decode_out->out_info.display_height = pDecOutput->m_DecOutInfo.m_iHeight;

		switch (pDecInitialInfo->m_iSourceFormat) {
		case YUV_FORMAT_420:
			arg_decode_out->out_info.dma_buf_align_width = ALIGNED_BUFF(arg_decode_out->out_info.decoded_width, 16U);
			arg_decode_out->out_info.dma_buf_align_height = ALIGNED_BUFF(arg_decode_out->out_info.decoded_height, 16U);
		break;

		case YUV_FORMAT_422:
			arg_decode_out->out_info.dma_buf_align_width = ALIGNED_BUFF(arg_decode_out->out_info.decoded_width, 16U);
			arg_decode_out->out_info.dma_buf_align_height = ALIGNED_BUFF(arg_decode_out->out_info.decoded_height, 8U);
		break;

		case YUV_FORMAT_224:
			arg_decode_out->out_info.dma_buf_align_width = ALIGNED_BUFF(arg_decode_out->out_info.decoded_width, 8U);
			arg_decode_out->out_info.dma_buf_align_height = ALIGNED_BUFF(arg_decode_out->out_info.decoded_height, 16U);
		break;

		case YUV_FORMAT_444:
			arg_decode_out->out_info.dma_buf_align_width = ALIGNED_BUFF(arg_decode_out->out_info.decoded_width, 8U);
			arg_decode_out->out_info.dma_buf_align_height = ALIGNED_BUFF(arg_decode_out->out_info.decoded_height, 8U);
		break;

		case YUV_FORMAT_400:
			arg_decode_out->out_info.dma_buf_align_width = ALIGNED_BUFF(arg_decode_out->out_info.decoded_width, 8U);
			arg_decode_out->out_info.dma_buf_align_height = ALIGNED_BUFF(arg_decode_out->out_info.decoded_height, 8U);
		break;

		default:
			arg_decode_out->out_info.dma_buf_align_width = ALIGNED_BUFF(arg_decode_out->out_info.decoded_width, 16U);
			arg_decode_out->out_info.dma_buf_align_height = ALIGNED_BUFF(arg_decode_out->out_info.decoded_height, 16U);
		break;
		}

		detail_jpu("[id:%u] Decode output disp:0x%x(%x), 0x%x(%x), 0x%x(%x), decod:0x%x(%x), 0x%x(%x), 0x%x(%x), PicType:%d, disp_idx:%d, dec_idx%d, disp_stat:%d, dec_stat:%d, w:%d, h:%d, interlace_frame:%d, crop:%d,%d - %d,%d",
						drv_info->drv_id,
						arg_decode_out->display_out[VPU_PA][VPU_COMP_Y],
						arg_decode_out->display_out[VPU_KVA][VPU_COMP_Y],
						arg_decode_out->display_out[VPU_PA][VPU_COMP_U],
						arg_decode_out->display_out[VPU_KVA][VPU_COMP_U],
						arg_decode_out->display_out[VPU_PA][VPU_COMP_V],
						arg_decode_out->display_out[VPU_KVA][VPU_COMP_V],
						arg_decode_out->decoded_out[VPU_PA][VPU_COMP_Y],
						arg_decode_out->decoded_out[VPU_KVA][VPU_COMP_Y],
						arg_decode_out->decoded_out[VPU_PA][VPU_COMP_U],
						arg_decode_out->decoded_out[VPU_KVA][VPU_COMP_U],
						arg_decode_out->decoded_out[VPU_PA][VPU_COMP_V],
						arg_decode_out->decoded_out[VPU_KVA][VPU_COMP_V],
						arg_decode_out->out_info.pic_type,
						arg_decode_out->out_info.display_idx,
						arg_decode_out->out_info.decoded_idx,
						arg_decode_out->out_info.display_status,
						arg_decode_out->out_info.decoded_status,
						arg_decode_out->out_info.display_width,
						arg_decode_out->out_info.display_height,
						arg_decode_out->out_info.interlaced_frame,
						arg_decode_out->out_info.display_crop.left,
						arg_decode_out->out_info.display_crop.top,
						arg_decode_out->out_info.display_crop.right,
						arg_decode_out->out_info.display_crop.bottom);

		if (pDecOutput->m_DecOutInfo.m_iDecodingStatus == VPU_DEC_BUF_FULL) {
			err_jpu("[id:%u] Buffer full", drv_id);
		}
	}
	return ret;
}

static int jmgr_dec_close(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int retDec;
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;
	int buffer_idx = 0;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	jpu_c6_dec_papam_t *ip_param = (jpu_c6_dec_papam_t *)drv_info->ip_param;
	jpu_dec_buffer3_t *pDecBuffer = &ip_param->dec_buffer3;

	retDec = tcc_jpu_dec_l(vpu_ap, JPU_DEC_CLOSE, (codec_handle_t *)&pHandle, (void *)NULL, (void *)NULL);
	ret = jmgr_convert_returnType(retDec);

	for (buffer_idx = 0; buffer_idx < pDecBuffer->m_iFrameBufferCount; buffer_idx++) {
		phys_addr_t paddr = 0U;
		void *vaddr = NULL;

		paddr = (phys_addr_t)pDecBuffer->m_addrFrameBuffer[PA][buffer_idx][COMP_Y];
		vaddr = (void *)pDecBuffer->m_addrFrameBuffer[VA][buffer_idx][COMP_Y];
		detail_jpu("[id:%u] Y buffer %d, iounmap paddr:0x%x, vaddr:0x%x", drv_id, buffer_idx, paddr, vaddr);
		iounmap(vaddr);

		paddr = (phys_addr_t)pDecBuffer->m_addrFrameBuffer[PA][buffer_idx][COMP_U];
		vaddr = (void *)pDecBuffer->m_addrFrameBuffer[VA][buffer_idx][COMP_U];
		detail_jpu("[id:%u] CB buffer %d, iounmap paddr:0x%x, vaddr:0x%x", drv_id, buffer_idx, paddr, vaddr);
		iounmap(vaddr);

		paddr = (phys_addr_t)pDecBuffer->m_addrFrameBuffer[PA][buffer_idx][COMP_V];
		vaddr = (void *)pDecBuffer->m_addrFrameBuffer[VA][buffer_idx][COMP_V];
		detail_jpu("[id:%u] CR buffer %d, iounmap paddr:0x%x, vaddr:0x%x", drv_id, buffer_idx, paddr, vaddr);
		iounmap(vaddr);
	}

	dlog_jpu("[id:%u] JPU_DEC_CLOSED !!", drv_id);
	return ret;
}

static int jmgr_decode_process(void *vpu_private, enum vpu_cmd_type cmd, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;

	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)vpu_private;

	dlog_jpu("[id:%u] command in:%d", drv_id, cmd);

	switch (cmd) {
	case VPU_CMD_DEC_INIT:
	{
		ret = jmgr_dec_init(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_DEC_SEQ_HEADER:
	{
		ret = jmgr_dec_seqheader(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_DEC_REG_FRAME_BUFFER:
	{
		ret = jmgr_dec_register_framebuffer(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_DEC_REG_USER_FRAME_BUFFER:
	{
		ret = jmgr_dec_user_framebuffer(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_DEC_DECODE:
	{
		ret = jmgr_dec_decode(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_DEC_CLOSE:
	{
		ret = jmgr_dec_close(mgr_ctx, cmd_info, drv_info);
	}
	break;

	default:
	{
		err_jpu("[id:%u] not supported command(%d)", drv_id, cmd);
		ret = -1;
	}
	break;
	}

	ret = vmgr_convert_retcode(ret);
	return ret;
}

static int jmgr_get_buffer_size(void *vpu_private, enum vmgr_buffer_type buf_type, vpu_drv_info_t *drv_info)
{
	int size = 0;

	//for unused buffers, they must be set to 0.
	switch (buf_type) {
	case VMGR_BUF_BITSTREAM:
		size = ALIGNED_BUFF(LARGE_STREAM_BUF_SIZE, 1024u);
	break;

	case VMGR_BUF_NUM_OF_BITSTREAM:
		size = 1;
	break;

	case VMGR_BUF_BITWORK:
		size = 0;
	break;

	case VMGR_BUF_FRAMEBUF:
		size = 0; //jpu doesn't use framebuffer
	break;

	case VMGR_BUF_SPSPPS:
	case VMGR_BUF_USERDATA:
	case VMGR_BUF_SLICE:
	case VMGR_BUF_MBDATA:
	case VMGR_BUF_MESEARCH:
	case VMGR_BUF_SLICEINFO:
	{
		size = 0;
	}
	break;


	//from here
	//when using the user framebuffer register,
	//it is called after the seq header initialization is complete and the result is passed to the output of the seq header.
	case VMGR_BUF_Y:
	{
		//jpu_c6_dec_papam_t *ip_param = (jpu_c6_dec_papam_t *)drv_info->ip_param;
		//jpu_dec_init_t *pDecInit = &ip_param->dec_init;
		//drv_info->initial_info.mjpg_spec_info.mjpg_source_format;

		size = ALIGNED_BUFF((drv_info->initial_info.pic_width * drv_info->initial_info.pic_height), 4096u);
	}
	break;

	case VMGR_BUF_CB:
	{
		jpu_c6_dec_papam_t *ip_param = (jpu_c6_dec_papam_t *)drv_info->ip_param;
		jpu_dec_init_t *pDecInit = &ip_param->dec_init;


		if (pDecInit->m_iCbCrInterleaveMode == 1U) {
			//nv12
			size = ALIGNED_BUFF(((drv_info->initial_info.pic_width * drv_info->initial_info.pic_height) / 2), 4096u);
		} else {
			//yuuv420
			size = ALIGNED_BUFF(((drv_info->initial_info.pic_width * drv_info->initial_info.pic_height) / 4), 4096u);
		}
	}
	break;

	case VMGR_BUF_CR:
	{
		jpu_c6_dec_papam_t *ip_param = (jpu_c6_dec_papam_t *)drv_info->ip_param;
		jpu_dec_init_t *pDecInit = &ip_param->dec_init;

		if (pDecInit->m_iCbCrInterleaveMode == 1U) {
			//nv12
			size = 0;
		} else {
			//yuuv420
			size = ALIGNED_BUFF(((drv_info->initial_info.pic_width * drv_info->initial_info.pic_height) / 4), 4096u);
		}
	}
	break;

	case VMGR_BUF_MVCOL:
	case VMGR_BUF_FBCY:
	case VMGR_BUF_FBCC:
		size = 0;
	break;

	default:
		size = 0;
	break;
	}

	detail_jpu("[%s][%s][id:%u]: buf_type:%d, size:%d", vmgr_get_ip_name(drv_info->ip_type), vmgr_get_optype_name(drv_info->op_type), drv_info->drv_id, buf_type, size);
	return size;
}


static irqreturn_t jmgr_isr_handler(int irq, void *vpu_private)
{
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)vpu_private;

	atomic_inc(&mgr_ctx->oper_intr);
	wake_up_interruptible(&mgr_ctx->oper_wq);
	return IRQ_HANDLED;
}

extern vmgr_clock_t jpu_clock;

static vpu_ip_module_t jpu_module = {
	.ip_type = VPU_IP_JPU_C6,
	.cq_type = VPU_CQ_LEGACY,
	.cq_depth = 0,
	.buffer_mode = VPU_BS_MODE_LINEARBUFFR,
	.internal_timeout_ms = 200,
	.enc_param_size = sizeof(jpu_c6_enc_papam_t),
	.dec_param_size = sizeof(jpu_c6_dec_papam_t),
	.internal_handler = jmgr_internal_handler,
	//codec id, codec name(string), profile(string), level(string), width(unsigned int), height(unsigned int), fps(unsigned int)
	.dec_capa = {{VCODEC_ID_MJPG, CODEC_NAME_MJPEG, "baseline", NULL, 8192, 8192, 0},
				{VCODEC_ID_NONE, NULL, NULL, NULL, 0, 0, 0}},
	.enc_capa = {{VCODEC_ID_MJPG, CODEC_NAME_MJPEG, "baseline", NULL, 8192, 8192, 0},
				{VCODEC_ID_NONE, NULL, NULL, NULL, 0, 0, 0}},
	.clock_ctrl = &jpu_clock,
	.proc_encode = jmgr_encode_process,
	.proc_decode = jmgr_decode_process,
	.proc_get_buffer_size = jmgr_get_buffer_size,
	.isr_handler = jmgr_isr_handler,
	.cq_func = NULL,
	.ip_private = NULL,
	.access_point_path = JPU_ACCESSPOINT_PATH,
};

int jmgr_probe(struct platform_device *pdev)
{
	int ret = 0;

	vpu_mgr_t *mgr_ctx = NULL;

	mgr_ctx = vmgr_alloc(&jpu_module);
	if (mgr_ctx != NULL) {
#if !defined(USE_ACCESS_POINT)
		mgr_ctx->access_point->tccfp_vpu_dec = tcc_jpu_dec;
		mgr_ctx->access_point->tccfp_vpu_enc = tcc_jpu_enc;
#else
		mgr_ctx->access_point = NULL;
#endif

		ret = vmgr_probe(mgr_ctx, pdev, JMGR_NAME);
		if (ret == 0) {
			//assigning VPU manager context to avoid mutex race condition in interrupt handler
			jpu_mgr_ctx = (vpu_mgr_t *)vmgr_get_context(VPU_IP_JPU_C6);
		}
	}

	platform_set_drvdata(pdev, mgr_ctx);
	return ret;
}

EXPORT_SYMBOL(jmgr_probe);

VREMOVE_RET_TYPE jmgr_remove(struct platform_device *pdev)
{
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)platform_get_drvdata(pdev);

	if (mgr_ctx->each_ip->ip_private != NULL) {
		VPU_free(mgr_ctx->each_ip->ip_private);
		mgr_ctx->each_ip->ip_private = NULL;
	}

	vmgr_remove(mgr_ctx, pdev);
	vmgr_free(mgr_ctx);
	jpu_mgr_ctx = NULL;

	VREMOVE_RETURN();
}

EXPORT_SYMBOL(jmgr_remove);

#if defined(CONFIG_PM)
int jmgr_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)platform_get_drvdata(pdev);

	ret = vmgr_suspend(mgr_ctx, pdev, state);
	return ret;
}

EXPORT_SYMBOL(jmgr_suspend);

int jmgr_resume(struct platform_device *pdev)
{
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)platform_get_drvdata(pdev);

	vmgr_resume(mgr_ctx, pdev);
	return 0;
}

EXPORT_SYMBOL(jmgr_resume);
#endif

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
MODULE_SOFTDEP("pre: vpu_lib jpu_lib hevc_lib vpu_4k_d2_lib vpu_hevc_enc_lib vpu");

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC jpu manager");
MODULE_LICENSE("GPL");

#endif //ENABLE_VPU_DRV_JPU_C6
