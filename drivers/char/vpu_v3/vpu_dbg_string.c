// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_dbg_string.h"

const char* codec_name[] =
{
	"none",  // 0
	CODEC_NAME_AVC,
	CODEC_NAME_VC1,
	CODEC_NAME_MPEG2,
	CODEC_NAME_MPEG4,
	CODEC_NAME_H263,
	CODEC_NAME_DIVX,
	CODEC_NAME_AVS,
	CODEC_NAME_MJPEG,
	CODEC_NAME_VP8,
	CODEC_NAME_MVC, // 10
	CODEC_NAME_HEVC,
	CODEC_NAME_VP9,
};

const char* op_type_name_invalid = "OP_TYPE_INVALID";

//enum vpu_op_type in vpu_internal_type.h
const char* op_type_name[VPU_OP_TYPE_MAX] =
{
	"OP_TYPE_DEC",
	"OP_TYPE_ENC"
};

//enum vpu_ip_type in vpu_internal_type.h
const char* vmgr_name[VPU_IP_MAX] =
{
	"VPU_IP_UNKNOWN",
	"VPU_IP_C7",
	"VPU_IP_4KD2",
	"VPU_IP_HEVC_ENC",
	"VPU_IP_HEVC_ENC2",
	"VPU_IP_JPU_C6",
	"VPU_IP_HEVC_DEC"
};

const char* invalid_ip_type_name = "INVALID_IP_TYPE";

//for debugging
const char* vpu_cmd_name_invalid = "VPU_INVALID_COMMAND";

//enum vpu_cmd_type in vpu_internal_type.h
const char* vpu_cmd_name[] =
{
	"VPU_CMD_DEC_INIT",
	"VPU_CMD_DEC_SEQ_HEADER",
	"VPU_CMD_DEC_GET_INFO",
	"VPU_CMD_DEC_REG_FRAME_BUFFER",
	"VPU_CMD_DEC_REG_USER_FRAME_BUFFER",
	"VPU_CMD_DEC_GET_OUTPUT_INFO",
	"VPU_CMD_DEC_DECODE",
	"VPU_CMD_DEC_BUF_FLAG_CLEAR",
	"VPU_CMD_DEC_FLUSH",
	"VPU_CMD_DEC_DRAIN",
	"VPU_CMD_DEC_RING_GET_INFO",
	"VPU_CMD_DEC_RING_SET_INFO",
	"VPU_CMD_DEC_CLOSE",

	"VPU_CMD_ENC_INIT",
	"VPU_CMD_ENC_REG_FRAME_BUFFER",
	"VPU_CMD_ENC_PUT_HEADER",
	"VPU_CMD_ENC_ENCODE",
	"VPU_CMD_ENC_CLOSE",
};

// RETCODE from TCCxxxx_VPU_CODEC_COMMON.h
const char* vpu_error_name[] =
{
	"RETCODE_SUCCESS", //0
	"RETCODE_FAILURE",
	"RETCODE_INVALID_HANDLE",
	"RETCODE_INVALID_PARAM",
	"RETCODE_INVALID_COMMAND",
	"RETCODE_ROTATOR_OUTPUT_NOT_SET", //5
	"RETCODE_ROTATOR_STRIDE_NOT_SET",
	"RETCODE_FRAME_NOT_COMPLETE",
	"RETCODE_INVALID_FRAME_BUFFER",
	"RETCODE_INSUFFICIENT_FRAME_BUFFERS",
	"RETCODE_INVALID_STRIDE", //10
	"RETCODE_WRONG_CALL_SEQUENCE",
	"RETCODE_CALLED_BEFORE",
	"RETCODE_NOT_INITIALIZED",
	"RETCODE_USERDATA_BUF_NOT_SET",
	"RETCODE_CODEC_FINISH", //15
	"RETCODE_CODEC_EXIT",
	"RETCODE_CODEC_SPECOUT",
	"RETCODE_MEM_ACCESS_VIOLATION",
	"UNKNOWN_ERROR", //19
	"RETCODE_INSUFFICIENT_BITSTREAM", //20
	"RETCODE_INSUFFICIENT_BITSTREAM_BUF",
	"RETCODE_INSUFFICIENT_PS_BUF",
	"RETCODE_ACCESS_VIOLATION_HW",
	"RETCODE_INSUFFICIENT_SECAXI_BUF",
	"RETCODE_QUEUEING_FAILURE", //25
	"RETCODE_VPU_STILL_RUNNING",
	"RETCODE_REPORT_NOT_READY",
};

const char* vpu_error_invalid = "INVALID ERROR_CODE";

//enum vpu_pmap_type in tcc_vpu_v3_common.h
const char* pmap_type_name[] =
{
	"VPU_PMAP_DEC",
	"VPU_PMAP_DEC_EXT",
	"VPU_PMAP_DEC_EXT2",
	"VPU_PMAP_DEC_EXT3",
	"VPU_PMAP_DEC_EXT4",
	"VPU_PMAP_ENC",
	"VPU_PMAP_ENC_EXT",
	"VPU_PMAP_ENC_EXT2",
	"VPU_PMAP_ENC_EXT3",
	"VPU_PMAP_ENC_EXT4",
	"VPU_PMAP_ENC_EXT5",
	"VPU_PMAP_ENC_EXT6",
	"VPU_PMAP_ENC_EXT7",
	"VPU_PMAP_ENC_EXT8",
	"VPU_PMAP_ENC_EXT9",
	"VPU_PMAP_ENC_EXT10",
	"VPU_PMAP_ENC_EXT11",
	"VPU_PMAP_ENC_EXT12",
	"VPU_PMAP_ENC_EXT13",
	"VPU_PMAP_ENC_EXT14",
	"VPU_PMAP_ENC_EXT15",
};

const char* invalid_pmap_type_name = "INVALID_PMAP_TYPE";

//VPU_RETURN in tcc_vpu_v3_common.h
const char* return_type_name[] =
{
	"VPU_RETCODE_SUCCESS",
	"VPU_RETCODE_FAILURE",
	"VPU_RETCODE_INSUFFICIENT_MEMORY",
	"VPU_RETCODE_INVALID_PARAM",
	"VPU_RETCODE_FRAME_NOT_COMPLETE",
	"VPU_RETCODE_INVALID_STRIDE",
	"VPU_RETCODE_NOT_INITIALIZED",
	"VPU_RETCODE_CODEC_FINISH",
	"VPU_RETCODE_CODEC_EXIT",
	"VPU_RETCODE_CODEC_SPECOUT",
	"VPU_RETCODE_REPORT_NOT_READY",
	"VPU_RETCODE_MULTI_CODEC_EXIT_TIMEOUT",
	"VPU_RETCODE_INFO_INSUFFICIENT_DATA"
};

const char* invalid_return_name = "INVALID_RETURN_TYPE";


const char* vmgr_error_name(const int err_code)
{
	const char* ret_name = NULL;

	if(err_code >= 0 && err_code <= RETCODE_REPORT_NOT_READY)
	{
		ret_name = vpu_error_name[err_code];
	}
	else
	{
		ret_name = vpu_error_invalid;
	}

	return ret_name;
}

const char* vmgr_cmd_name(const enum vpu_cmd_type cmd)
{
	const char* ret_name = NULL;

	if(cmd >= VPU_CMD_DEC_INIT && cmd < VPU_CMD_TYPE_MAX)
	{
		ret_name = vpu_cmd_name[cmd];
	}
	else
	{
		ret_name = vpu_cmd_name_invalid;
	}

	return ret_name;
}

const char* vmgr_get_codec_name(const enum vpu_codec_id codec_id)
{
	const char* cname = NULL;
	int nb_array = sizeof(codec_name) / sizeof(codec_name[0]);

	if((codec_id >= 0) && (codec_id < nb_array))
	{
		cname = codec_name[codec_id];
	}

	return cname;
}

const char* vmgr_get_optype_name(const enum vpu_op_type op_type)
{
	const char* opname = NULL;

	if(op_type >= 0 && op_type < VPU_OP_TYPE_MAX)
	{
		opname = op_type_name[op_type];
	}
	else
	{
		opname = op_type_name_invalid;
	}

	return opname;
}

const char* vmgr_get_ip_name(const enum vpu_ip_type ip_type)
{
	const char* ret_name = NULL;

	if((ip_type >= VPU_IP_UNKNOWN) && (ip_type < VPU_IP_MAX))
	{
		ret_name  = vmgr_name[ip_type];
	}
	else
	{
		ret_name = invalid_ip_type_name;
	}

	return ret_name;
}

const char* vmgr_get_pmap_name(const int pmap_type)
{
	const char* ret_name = NULL;

	if((pmap_type >= VPU_PMAP_DEC) && (pmap_type < VPU_PMAP_MAX))
	{
		ret_name  = pmap_type_name[pmap_type];
	}
	else
	{
		ret_name = invalid_pmap_type_name;
	}

	return ret_name;
}

const char* vmgr_get_return_name(int ret_value)
{
	const char* ret_name = NULL;

	if((ret_value >= VPU_RETCODE_SUCCESS) && (ret_value < VPU_RETCODE_MAX))
	{
		ret_name  = return_type_name[ret_value];
	}
	else
	{
		ret_name = invalid_return_name;
	}

	return ret_name;
}

EXPORT_SYMBOL(vmgr_error_name);
EXPORT_SYMBOL(vmgr_cmd_name);
EXPORT_SYMBOL(vmgr_get_codec_name);
EXPORT_SYMBOL(vmgr_get_optype_name);
EXPORT_SYMBOL(vmgr_get_ip_name);
EXPORT_SYMBOL(vmgr_get_pmap_name);
EXPORT_SYMBOL(vmgr_get_return_name);


