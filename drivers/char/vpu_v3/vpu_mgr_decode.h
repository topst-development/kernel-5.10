/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#ifndef VPU_MGR_DECODE_H
#define VPU_MGR_DECODE_H

#include "vpu_mgr.h"
#include "vpu_internal_type.h"

int vmgr_decode_proc(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd);

int vmgr_dec_decode_update_bitstream_addr(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd, vpu_drv_info_t *drv_info);

#endif //VPU_MGR_DECODE_H