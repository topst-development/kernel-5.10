// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_MGR_COMMON_H
#define VPU_MGR_COMMON_H

#include "vpu_comm.h"
#include "vpu_mgr.h"
#include "vpu_rm.h"

int vmgr_get_bitstream_format(enum vpu_codec_id codec_id, enum vpu_op_type op_type);

enum vpu_display_status vmgr_convert_display_status(int output_status);

enum vpu_decoded_status vmgr_convert_decoding_status(int decoding_status);

int vmgr_convert_retcode(int retcode);

Buffer_Type vmgr_convert_buffer_type(enum vpu_buffer_type buffer_type);

vputype vmgr_convert_pmap_to_vputype(enum vpu_pmap_type pmap_type);

#endif //VPU_MGR_COMMON_H
