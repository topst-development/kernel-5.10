// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_DEBUG_H
#define VPU_DEBUG_H

#include <linux/types.h>

#define VPU_DEBUG

enum vdbg_use_prtk_type {
	VPU_PRT_SINCE_SET_DBG = 0,
	VPU_ALWAYS_PRT_DBG
};

enum vdbg_mask_type {
	VPU_DBG_ERROR             = (1<<0),
	VPU_DBG_INFO              = (1<<1),
	VPU_DBG_SEQUENCE          = (1<<2),
	VPU_DBG_ISR               = (1<<3),
	VPU_DBG_INTERRUPT         = (1<<4),
	VPU_DBG_PROBE             = (1<<5),
	VPU_DBG_RSTCLK            = (1<<6),
	VPU_DBG_THREAD	          = (1<<7),
	VPU_DBG_INSTANCE          = (1<<8),
	VPU_DBG_PMAP              = (1<<9),
	VPU_DBG_MEMORY            = (1<<10),
	VPU_DBG_MEM_SEQ           = (1<<11),
	VPU_DBG_MEM_USAGE         = (1<<12),
	VPU_DBG_CLOSE             = (1<<13),
	VPU_DBG_IO_FB_INFO        = (1<<14),
	VPU_DBG_REG_DUMP          = (1<<15),
	VPU_DBG_PERF              = (1<<16),
	VPU_DBG_CMD               = (1<<17),
	VPU_DBG_ILV_INFO          = (1<<18),
	VPU_DBG_BUF_STATUS        = (1<<19),
	VPU_DBG_FB_CLR_STATE      = (1<<20),
	VPU_DBG_DEV_REGED         = (1<<21),
};

//module_param_vdbg_lib
//#define VPU_LIB_WRITE_OUTPUT_TO_YUV

enum vdbg_log_mask_type {
	VLOG_MASK_DRV_MEASURE_PERF_WITH_OUT_INFO    = (0x0001U),
	VLOG_MASK_LIB_MEASURE_PERF                  = (0x0800U),
	VLOG_MASK_LIB_USE_CB_PRINTK                 = (0x1000U),
	VLOG_MASK_LIB_WRITE_OUTPUT_TO_YUV           = (0x2000U),
};

extern unsigned int vpu_prt_mode;
extern unsigned int vdbg_mask;
extern unsigned int module_param_vdbg_lib;
extern unsigned int module_param_vdbg_drv;

void V_DEBUG(enum vdbg_mask_type dbg_mask, const char *fn,
			int ln, const char *fmt, ...);

#ifdef VPU_DEBUG
#define V_DBG(x, fmt, args...) { V_DEBUG(x, __func__, __LINE__, fmt, ##args); }
#else
#define V_DBG(x, fmt, args...) \
	do { \
		if (x & VPU_DBG_ERROR) {\
			(void)pr_err("[%s:%d] " fmt "\n", \
				__func__, __LINE__, ##args); \
		} \
	} while (0)
#endif

void init_vdbg_log(void);
void vdbg_log(const char *fmt, ...);
void vdbg_log_printk(const char *fmt, ...);
#ifdef VPU_LIB_WRITE_OUTPUT_TO_YUV
void init_vdbg_save_yuv(int iCodec, unsigned int iYuvInterleaved, int iBitDepth);
void vdbg_get_codec_info(int* piCodec, int* piYuvInterleaved, int* piByteDepth);
void vdbg_save_yuv_frame(const unsigned char* yuv_data, size_t yuv_size);
#define VPU_ALIGN32(_X)             ((_X+0x1f)&~0x1f)
#define VPU_ALIGN64(_X)             ((_X+0x3f)&~0x3f)

void request_thread_dump(int w, int h, int idx, unsigned int y, unsigned int u, unsigned int v);
#endif
bool vdbg_mode(void);

#endif // VPU_DEBUG_H
