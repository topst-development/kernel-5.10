/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#ifndef VPU_DBG_STRING_H
#define VPU_DBG_STRING_H

#include "vpu_comm.h"
#include "vpu_internal_type.h"

const char *vmgr_error_name(const int err_code);

const char *vmgr_cmd_name(const enum vpu_cmd_type cmd);

const char *vmgr_get_codec_name(const enum vpu_codec_id codec_id);

const char *vmgr_get_optype_name(const enum vpu_op_type op_type);

const char *vmgr_get_ip_name(const enum vpu_ip_type ip_type);

const char *vmgr_get_pmap_name(const int pmap_type);

const char *vmgr_get_return_name(int ret_value);

#endif //VPU_DBG_STRING_H