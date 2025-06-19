// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_DEC_FLEXIO_H
#define VPU_DEC_FLEXIO_H

#include <linux/fs.h>

long vdec_ioctl_flexio(struct file *filp, unsigned int cmd, unsigned long arg);

#endif // VPU_DEC_FLEXIO_H
