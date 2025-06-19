// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_MGR_FLEXIO_H
#define VPU_MGR_FLEXIO_H

#include "vpu_comm.h"

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/compat.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>

void *v2fhdmgr_unmarshal_ip_inidata(void *args);
void v2fhdmgr_marshal_op_inidata(void *args);

void *v2fhdmgr_unmarshal_ip_seqdata(void *args);
void v2fhdmgr_marshal_op_seqdata(void *args);

int32_t v2fhdmgr_register_hwbuf(void *args, long handle, tccfp_vpu_proc_t tcc_vpu_dec);
void *v2fhdmgr_unmarshal_ip_drndata(void *args);

bool v2fhdmgr_ip_check_delayed_ring_mode(void *args);

void *v2fhdmgr_unmarshal_ip_frmdata(void *args, vcodec_handle_t *pHandle, tccfp_vpu_proc_t tcc_vpu_dec);
void v2fhdmgr_marshal_op_frmdata(void *args, bool updated);

void *v2fhdmgr_unmarshal_ip_setpos(void *args);
void *v2fhdmgr_unmarshal_ip_getpos(void *args);
void v2fhdmgr_marshal_op_getpos(void *args);

#endif //VPU_MGR_FLEXIO_H
