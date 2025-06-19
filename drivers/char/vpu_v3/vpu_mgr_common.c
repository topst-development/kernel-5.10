// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_mgr_common.h"

int vmgr_get_bitstream_format(enum vpu_codec_id codec_id, enum vpu_op_type op_type)
{
	int format = -1;

	switch (codec_id) {
	case VCODEC_ID_AVC:
		format = STD_AVC;
	break;

	case VCODEC_ID_VC1:
		format = STD_VC1;
	break;

	case VCODEC_ID_MPEG2:
		format = STD_MPEG2;
	break;

	case VCODEC_ID_MPEG4:
		format = STD_MPEG4;
	break;

	case VCODEC_ID_H263:
		format = STD_H263;
	break;

	case VCODEC_ID_DIVX:
		format = STD_DIV3;
	break;

	case VCODEC_ID_AVS:
		format = STD_AVS;
	break;

	case VCODEC_ID_MJPG:
		format = STD_MJPG;
	break;

	case VCODEC_ID_VP8:
		format = STD_VP8;
	break;

	case VCODEC_ID_MVC:
		format = STD_MVC;
	break;

	case VCODEC_ID_HEVC:
	{
		if (op_type == VPU_OP_TYPE_DEC) {
			format = STD_HEVC;
		} else {
			format = STD_HEVC_ENC;
		}
	}
	break;

	case VCODEC_ID_VP9:
		format = STD_VP9;
	break;

	default:
		format = -1;
	break;
	}

	return format;
}
EXPORT_SYMBOL(vmgr_get_bitstream_format);

enum vpu_display_status vmgr_convert_display_status(int output_status)
{
	enum vpu_display_status status = VPU_DISP_STAT_FAIL;

	switch (output_status) {
	case VPU_DEC_OUTPUT_FAIL:
		status = VPU_DISP_STAT_FAIL;
	break;

	case VPU_DEC_OUTPUT_SUCCESS:
		status = VPU_DISP_STAT_SUCCESS;
	break;

	default:
		status = VPU_DISP_STAT_FAIL;
	break;
	}

	return status;
}
EXPORT_SYMBOL(vmgr_convert_display_status);

enum vpu_decoded_status vmgr_convert_decoding_status(int decoding_status)
{
	enum vpu_decoded_status status = VPU_DEC_STAT_SUCCESS;

	switch (decoding_status) {
	case VPU_DEC_SUCCESS:
		status = VPU_DEC_STAT_SUCCESS;
	break;

	case VPU_DEC_INFO_NOT_SUFFICIENT_SPS_PPS_BUFF:
		status = VPU_DEC_STAT_INFO_NOT_SUFFICIENT_SPS_PPS_BUFF;
	break;

	case VPU_DEC_INFO_NOT_SUFFICIENT_SLICE_BUFF:
		status = VPU_DEC_STAT_INFO_NOT_SUFFICIENT_SLICE_BUFF;
	break;

	case VPU_DEC_BUF_FULL:
		status = VPU_DEC_STAT_BUF_FULL;
	break;

	case VPU_DEC_SUCCESS_FIELD_PICTURE:
		status = VPU_DEC_STAT_SUCCESS_FIELD_PICTURE;
	break;

	case VPU_DEC_DETECT_RESOLUTION_CHANGE:
		status = VPU_DEC_STAT_DETECT_RESOLUTION_CHANGE;
	break;

	case VPU_DEC_INVALID_INSTANCE:
		status = VPU_DEC_STAT_INVALID_INSTANCE;
	break;

	case VPU_DEC_DETECT_DPB_CHANGE:
		status = VPU_DEC_STAT_DETECT_DPB_CHANGE;
	break;

	case VPU_DEC_QUEUEING_FAIL:
		status = VPU_DEC_STAT_QUEUEING_FAIL;
	break;

	case VPU_DEC_VP9_SUPER_FRAME:
		status = VPU_DEC_STAT_VP9_SUPER_FRAME;
	break;

	case VPU_DEC_CQ_EMPTY:
		status = VPU_DEC_STAT_CQ_EMPTY;
	break;

	case VPU_DEC_REPORT_NOT_READY:
		status = VPU_DEC_STAT_REPORT_NOT_READY;
	break;

	default:
	break;
	}

	return status;
}
EXPORT_SYMBOL(vmgr_convert_decoding_status);

int vmgr_convert_retcode(int retcode)
{
	int ret = 0;

	switch (retcode) {
	case RETCODE_SUCCESS:
		ret = VPU_RETCODE_SUCCESS;
	break;

	case RETCODE_FAILURE:
		ret = VPU_RETCODE_FAILURE;
	break;

	case RETCODE_INVALID_HANDLE:
		ret = VPU_RETCODE_INVALID_PARAM;
	break;

	case RETCODE_INVALID_PARAM:
		ret = VPU_RETCODE_INVALID_PARAM;
	break;

	case RETCODE_INVALID_COMMAND:
		ret = VPU_RETCODE_INVALID_PARAM;
	break;

	case RETCODE_ROTATOR_OUTPUT_NOT_SET:
		ret = VPU_RETCODE_FAILURE;
	break;

	case RETCODE_ROTATOR_STRIDE_NOT_SET:
		ret = VPU_RETCODE_FAILURE;
	break;

	case RETCODE_FRAME_NOT_COMPLETE:
		ret = VPU_RETCODE_FRAME_NOT_COMPLETE;
	break;

	case RETCODE_INVALID_FRAME_BUFFER:
		ret = VPU_RETCODE_FAILURE;
	break;

	case RETCODE_INSUFFICIENT_FRAME_BUFFERS:
		ret = VPU_RETCODE_INSUFFICIENT_MEMORY;
	break;

	case RETCODE_INVALID_STRIDE:
		ret = VPU_RETCODE_INVALID_STRIDE;
	break;

	case RETCODE_WRONG_CALL_SEQUENCE:
		ret = VPU_RETCODE_FAILURE;
	break;

	case RETCODE_CALLED_BEFORE:
		ret = VPU_RETCODE_FAILURE;
	break;

	case RETCODE_NOT_INITIALIZED:
		ret = VPU_RETCODE_NOT_INITIALIZED;
	break;

	case RETCODE_USERDATA_BUF_NOT_SET:
		ret = VPU_RETCODE_FAILURE;
	break;

	case RETCODE_CODEC_FINISH:
		ret = VPU_RETCODE_CODEC_FINISH;
	break;

	case RETCODE_CODEC_EXIT:
		ret = VPU_RETCODE_CODEC_EXIT;
	break;

	case RETCODE_CODEC_SPECOUT:
		ret = VPU_RETCODE_CODEC_SPECOUT;
	break;

	case RETCODE_MEM_ACCESS_VIOLATION:
		ret = VPU_RETCODE_FAILURE;
	break;

	case RETCODE_INSUFFICIENT_BITSTREAM:
		ret = VPU_RETCODE_INFO_INSUFFICIENT_DATA;
	break;

	case RETCODE_INSUFFICIENT_BITSTREAM_BUF:
		ret = VPU_RETCODE_INSUFFICIENT_MEMORY;
	break;

	case RETCODE_INSUFFICIENT_PS_BUF:
		ret = VPU_RETCODE_INSUFFICIENT_MEMORY;
	break;

	case RETCODE_ACCESS_VIOLATION_HW:
		ret = VPU_RETCODE_FAILURE;
	break;

	case RETCODE_INSUFFICIENT_SECAXI_BUF:
		ret = VPU_RETCODE_INSUFFICIENT_MEMORY;
	break;

	case RETCODE_QUEUEING_FAILURE:
		ret = VPU_RETCODE_FAILURE;
	break;

	case RETCODE_VPU_STILL_RUNNING:
		ret = VPU_RETCODE_FAILURE;
	break;

	case RETCODE_REPORT_NOT_READY:
		ret = VPU_RETCODE_REPORT_NOT_READY;
	break;

	default:
		ret = VPU_RETCODE_FAILURE;
	break;
	}

	return ret;
}
EXPORT_SYMBOL(vmgr_convert_retcode);

Buffer_Type vmgr_convert_buffer_type(enum vpu_buffer_type buffer_type)
{
	Buffer_Type ret_type = BUFFER_ELSE;

	switch (buffer_type) {
	case VPU_BUFFER_ELSE:
		ret_type = BUFFER_ELSE;
	break;

	case VPU_BUFFER_WORK:
		ret_type = BUFFER_WORK;
	break;

	case VPU_BUFFER_STREAM:
		ret_type = BUFFER_STREAM;
	break;

	case VPU_BUFFER_SEQHEADER:
		ret_type = BUFFER_SEQHEADER;
	break;

	case VPU_BUFFER_FRAMEBUFFER:
		ret_type = BUFFER_FRAMEBUFFER;
	break;

	case VPU_BUFFER_PS:
		ret_type = BUFFER_PS;
	break;

	case VPU_BUFFER_SLICE:
		ret_type = BUFFER_SLICE;
	break;

	case VPU_BUFFER_USERDATA:
		ret_type = BUFFER_USERDATA;
	break;

	default:
		ret_type = BUFFER_ELSE;
	break;
	}

	return ret_type;
}
EXPORT_SYMBOL(vmgr_convert_buffer_type);

vputype vmgr_convert_pmap_to_vputype(enum vpu_pmap_type pmap_type)
{
	vputype ret_type = VPU_MAX;

	switch (pmap_type) {
	case VPU_PMAP_DEC:
		ret_type = VPU_DEC;
	break;

	case VPU_PMAP_DEC_EXT:
		ret_type = VPU_DEC_EXT;
	break;

	case VPU_PMAP_DEC_EXT2:
		ret_type = VPU_DEC_EXT2;
	break;

	case VPU_PMAP_DEC_EXT3:
		ret_type = VPU_DEC_EXT3;
	break;

	case VPU_PMAP_DEC_EXT4:
		ret_type = VPU_DEC_EXT4;
	break;

	case VPU_PMAP_ENC:
		ret_type = VPU_ENC;
	break;

	case VPU_PMAP_ENC_EXT:
		ret_type = VPU_ENC_EXT;
	break;

	case VPU_PMAP_ENC_EXT2:
		ret_type = VPU_ENC_EXT2;
	break;

	case VPU_PMAP_ENC_EXT3:
		ret_type = VPU_ENC_EXT3;
	break;

	case VPU_PMAP_ENC_EXT4:
		ret_type = VPU_ENC_EXT4;
	break;

	case VPU_PMAP_ENC_EXT5:
		ret_type = VPU_ENC_EXT5;
	break;

	case VPU_PMAP_ENC_EXT6:
		ret_type = VPU_ENC_EXT6;
	break;

	case VPU_PMAP_ENC_EXT7:
		ret_type = VPU_ENC_EXT7;
	break;

	case VPU_PMAP_ENC_EXT8:
		ret_type = VPU_ENC_EXT8;
	break;

	case VPU_PMAP_ENC_EXT9:
		ret_type = VPU_ENC_EXT9;
	break;

	case VPU_PMAP_ENC_EXT10:
		ret_type = VPU_ENC_EXT10;
	break;

	case VPU_PMAP_ENC_EXT11:
		ret_type = VPU_ENC_EXT11;
	break;

	case VPU_PMAP_ENC_EXT12:
		ret_type = VPU_ENC_EXT12;
	break;

	case VPU_PMAP_ENC_EXT13:
		ret_type = VPU_ENC_EXT13;
	break;

	case VPU_PMAP_ENC_EXT14:
		ret_type = VPU_ENC_EXT14;
	break;

	case VPU_PMAP_ENC_EXT15:
		ret_type = VPU_ENC_EXT15;
	break;

	default:
		ret_type = VPU_MAX;
	break;
	}

	return ret_type;
}

EXPORT_SYMBOL(vmgr_convert_pmap_to_vputype);

