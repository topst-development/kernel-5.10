/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_VIOC_LUT_3D_IOCTL_H
#define TCC_VIOC_LUT_3D_IOCTL_H

#define LUT_3D_IOC_MAGIC	'D'

struct VIOC_LUT_3D_SET_TABLE {
	unsigned int table[729];
};

struct VIOC_LUT_3D_ONOFF {
	unsigned int lut_3d_onoff;
};

#define TCC_LUT_3D_SET_TABLE	_IOW(LUT_3D_IOC_MAGIC, 0, struct VIOC_LUT_3D_SET_TABLE)
#define TCC_LUT_3D_ONOFF		_IOW(LUT_3D_IOC_MAGIC, 1, struct VIOC_LUT_3D_ONOFF)

#endif /* TCC_VIOC_LUT_3D_IOCTL_H */
