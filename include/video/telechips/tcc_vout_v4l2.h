/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_VOUT_V4L2_H__
#define TCC_VOUT_V4L2_H__

enum tcc_vout_status {
	TCC_VOUT_IDLE,
	TCC_VOUT_INITIALISING,
	TCC_VOUT_RUNNING,
	TCC_VOUT_STOP,
};

#endif
