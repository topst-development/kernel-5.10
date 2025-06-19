// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_4K_D2_MGR_FLEXIO_H
#define VPU_4K_D2_MGR_FLEXIO_H

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/compat.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>

void *v24kd2mgr_unmarshal_ip_inidata(void *args);
void v24kd2mgr_marshal_op_inidata(void *args);
void *v24kd2mgr_unmarshal_ip_seqdata(void *args);
void v24kd2mgr_marshal_op_seqdata(void *args);
int32_t v24kd2mgr_register_hwbuf_fb(void *args, long handle, tccfp_vpu_proc_t tcc_vpu_4k_d2_dec);
void *v24kd2mgr_unmarshal_ip_drndata(void *args);
void *v24kd2mgr_unmarshal_ip_frmdata(void *args);
void v24kd2mgr_marshal_op_frmdata(void *args, bool updated);
void *v24kd2mgr_unmarshal_ip_setpos(void *args);
void *v24kd2mgr_unmarshal_ip_getpos(void *args);
void v24kd2mgr_marshal_op_getpos(void *args);

#endif //VPU_4K_D2_MGR_FLEXIO_H
