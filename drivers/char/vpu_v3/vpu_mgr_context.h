// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_MGR_CONTEXT_H
#define VPU_MGR_CONTEXT_H

#include "vpu_comm.h"
#include "vpu_internal_type.h"

enum vpu_ip_type vmgr_find_ip_type(unsigned int drv_id, const enum vpu_codec_id codec_id, enum vpu_op_type op_type, int forced_index);

int vmgr_get_capability(enum vpu_op_type op_type, vpu_capability_t *capability);

int vmgr_set_context(const enum vpu_ip_type ip_type, void *ctx);

void *vmgr_get_context(const enum vpu_ip_type ip_type);

#endif //VPU_MGR_CONTEXT_H
