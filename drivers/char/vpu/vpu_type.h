// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_TYPE_H
#define VPU_TYPE_H

#include "video/telechips/TCCxxxx_VPU_CODEC_COMMON.h"

#if defined(CONFIG_ARCH_TCC897X) || \
	defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC570X) || \
	defined(CONFIG_ARCH_TCC901X)
#define VPU_D6
#else
#define VPU_C7
#endif

#define JPU_C6

#if !defined(CONFIG_ARCH_TCC897X)
#define VIDEO_IP_DIRECT_RESET_CTRL
#endif

//#define VBUS_CLK_ALWAYS_ON
#define VBUS_CODA_CORE_CLK_CTRL

typedef int (*tccfp_vpu_proc_t)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);

#endif /*VPU_TYPE_H*/

#define COVERITY_DEBUG_LEVEL 0

#define VPU_DONOTHING(fmt, ...) do { (void)0; } while (0)

#define LOG_COVERITY(...)  do { if (COVERITY_DEBUG_LEVEL > 0) { (void)printk(__VA_ARGS__); } } while (0)
#define VPU_IN_PTARG(tar, src) ((void)memcpy((void *)&(tar), (void *)&(src), sizeof(void *)))
#define VPU_CAST_PT(tar, src) ((void)memcpy((void *)&(tar), (void *)&(src), sizeof(void *)))

#define INITIAL_NULL (NULL)
#define INITIAL_ZERO (0)
