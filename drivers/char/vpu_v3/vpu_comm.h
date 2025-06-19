// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_COMM_H
#define VPU_COMM_H

#include "vpu_linux_kernel.h"

#if VPU_INST_MAX > 0
#define DEFINED_CONFIG_VDEC 1
#else
#define DEFINED_CONFIG_VDEC 0
#endif

#if VPU_ENC_MAX_CNT > 0
#define DEFINED_CONFIG_VENC 1
#else
#define DEFINED_CONFIG_VENC 0
#endif

#define VIDEO_IP_DIRECT_RESET_CTRL

//#define VBUS_CLK_ALWAYS_ON
#define VBUS_CODA_CORE_CLK_CTRL

//#define ENABLE_CQ2
#if defined(ENABLE_CQ2)
#define IS_CQ2 "with CQ2"
#else
#define IS_CQ2 "without CQ2"
#endif

#include "vpu_etc.h"
#include "vpu_dbg.h"
#include "vpu_dllist.h"
#include "vpu_memtrace.h"
#include "vpu_internal_type.h"

#if defined(CONFIG_SUPPORT_TCC_VPU)
#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC802X) || \
		defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
		defined(CONFIG_ARCH_TCC807X)
#include <video/telechips/TCC_VPU_C7_CODEC.h>
#define ENABLE_VPU_DRV_VPU_C7

#elif defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC897X) || \
		defined(CONFIG_ARCH_TCC901X)
#include <video/telechips/TCC_VPU_D6.h>
#define ENABLE_VPU_DRV_VPU_D6
#endif

#endif

#if defined(CONFIG_SUPPORT_TCC_JPU)
#include <video/telechips/TCC_JPU_C6.h>
#define ENABLE_VPU_DRV_JPU_C6
#endif

#if defined(CONFIG_SUPPORT_TCC_WAVE410_HEVC)
#include <video/telechips/TCC_HEVCDEC.h>
#define ENABLE_VPU_DRV_HEVCDEC
#endif

#if defined(CONFIG_SUPPORT_TCC_WAVE512_4K_D2)
#include <video/telechips/TCC_VPU_4K_D2.h>
#define ENABLE_VPU_DRV_4K_D2
#endif

#if defined(CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC) || defined(CONFIG_SUPPORT_TCC_WAVE420L_2ND_VPU_HEVC_ENC)
#include <video/telechips/TCC_VPU_HEVC_ENC.h>

#if defined(CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC)
#define ENABLE_VPU_DRV_HEVCENC
#endif

#if defined(CONFIG_SUPPORT_TCC_WAVE420L_2ND_VPU_HEVC_ENC)
#define ENABLE_VPU_DRV_HEVCENC2
#endif
#endif

#include <video/telechips/TCCxxxx_VPU_CODEC_COMMON.h>


#if defined(USE_ACCESS_POINT)
#define vpu_check_fourcc(a, b, c, d)\
	(((__u32)(a) | ((__u32)(b) << 8U)) | (((__u32)(c) << 16U) | ((__u32)(d) << 24U)))
#define GET_FOURCC_1(a) (((__u32)(a)) & (0x00FFU))
#define GET_FOURCC_2(a) (((__u32)(a) >>  8U) & (0x00FFU))
#define GET_FOURCC_3(a) (((__u32)(a) >> 16U) & (0x00FFU))
#define GET_FOURCC_4(a) (((__u32)(a) >> 24U) & (0x00FFU))

#define CHECK_CODE_01 vpu_check_fourcc('T', 'e', 'l', 'e')
#define CHECK_CODE_02 vpu_check_fourcc('c', 'h', 'i', 'p')
#define CHECK_CODE_03 vpu_check_fourcc('s', 'V', 'i', 'd')
#define CHECK_CODE_04 vpu_check_fourcc('e', 'o', 0xFF, 0xFF)

#if defined(CONFIG_TCC805X_CA53Q) || defined(CONFIG_TCC807X_CA55_SUB)
	#define SHARE_POINT_ADDR 0x50000000
#elif defined(CONFIG_ARCH_TCC897X)
	#define SHARE_POINT_ADDR 0x90000000
#else
	#define SHARE_POINT_ADDR 0x30000000
#endif

#define SHARD_POINT_GAP  128U
#endif

#ifdef CONFIG_CFI_CLANG
#   if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
#       define VPU_NO_SANITIZE_CFI __attribute__((no_sanitize("kcfi")))
#   else
#       define VPU_NO_SANITIZE_CFI __attribute__((no_sanitize("cfi")))
#   endif
#else
#	define VPU_NO_SANITIZE_CFI
#endif

#define VPU_CAST_PT(tar, src) ((void)memcpy((void *)&(tar), (void *)&(src), sizeof(void *)))

typedef int (*tccfp_vpu_proc_t)(int, codec_handle_t *, void *, void *);

/* COMMON */

#define VPU_LIMIT_PICWIDTH  (8*1024)
#define VPU_LIMIT_PICHEIGHT (VPU_LIMIT_PICWIDTH)

#define LIST_MAX 10

#define RET4_WAIT   0X00010000
#define RET3        0x00008000
#define RET2        0x00004000
#define RET1        0x00002000
#define RET0        0x00001000

#define INT_MAX_U		(~0U >> 1)
#define INT_MAX_S 		(0x7fffffff)

#define VPU_BUG_ON(x)	\
{ \
	void *tTmpPtr = NULL; \
	VPU_CAST_PT(tTmpPtr, x); \
	if (IS_ERR(tTmpPtr)) { \
		BUG(); \
	} \
}

#define VPU_DONOTHING(fmt, ...) do { (void)(0); } while (0)


#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
#	define VREMOVE_RET_TYPE void
#	define VREMOVE_RETURN()	VPU_DONOTHING(void)
#else
static inline int vremove_return(void) { return 0; }
#	define VREMOVE_RET_TYPE int
#	define VREMOVE_RETURN()  do { return vremove_return(); } while (0)
#endif

#define CODEC_NAME_AVC		"h.264/avc"
#define CODEC_NAME_VC1		"vc-1"
#define CODEC_NAME_MPEG2	"mpeg-1/2"
#define CODEC_NAME_MPEG4	"mpeg-4"
#define CODEC_NAME_H263		"h.263"
#define CODEC_NAME_DIVX		"divx"
#define CODEC_NAME_AVS		"avs"
#define CODEC_NAME_MJPEG	"mjpeg"
#define CODEC_NAME_VP8		"vp8"
#define CODEC_NAME_MVC		"mvc"
#define CODEC_NAME_HEVC		"h.265/hevc"
#define CODEC_NAME_VP9		"vp9"

#define MAX_COMMAND_ID		(65535)

#define INITIAL_NULL	(NULL)
#define INITIAL_ZERO	(0)

#endif /*VPU_COMM_H*/
