/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#ifndef VPU_DEBUG_H
#define VPU_DEBUG_H

#define VPU_DEBUG

enum vdbg_use_prtk_type {
	VPU_PRT_SINCE_SET_DBG = 0,
	VPU_ALWAYS_PRT_DBG
};

enum vdbg_mask_type {
	VPU_DBG_ERROR                 = (1<<0),
	VPU_DBG_INFO                  = (1<<1),
	VPU_DBG_DETAIL                = (1<<2),		//repetitive and detailed logs
	VPU_DBG_SEQUENCE              = (1<<3),		//checking the order of function calls
	VPU_DBG_ISR                   = (1<<4),
	VPU_DBG_INTERRUPT             = (1<<5),
	VPU_DBG_PROBE                 = (1<<6),
	VPU_DBG_RSTCLK                = (1<<7),
	VPU_DBG_THREAD	              = (1<<8),
	VPU_DBG_INSTANCE              = (1<<9),
	VPU_DBG_PMAP                  = (1<<10),
	VPU_DBG_MEMORY                = (1<<11),
	VPU_DBG_MEM_SEQ               = (1<<12),
	VPU_DBG_MEM_USAGE             = (1<<13),
	VPU_DBG_CLOSE                 = (1<<14),
	VPU_DBG_IO_FB_INFO            = (1<<15),
	VPU_DBG_REG_DUMP              = (1<<16),
	VPU_DBG_PERF                  = (1<<17),
	VPU_DBG_CMD                   = (1<<18),
	VPU_DBG_VER_INFO              = (1<<19),
	VPU_DBG_BUF_STATUS            = (1<<20),
	VPU_DBG_FB_CLR_STATE          = (1<<21),
	VPU_DBG_DEV_REGED             = (1<<22),
	VPU_DBG_LIB_USE_CB_PRINTK     = (1<<23),
	VPU_DBG_LIB_CQ_COUNT          = (1<<24),
	VPU_DBG_LIB_MEASURE_DEC_PERF  = (1<<25),
};

extern unsigned int vpu_prt_mode;
extern unsigned int vdbg_mask;
extern unsigned int module_param_vdbg_lib;

unsigned int get_vpu_lib_dbg_param(void);

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

void vpu_dbg_get_info(int *type, int *mask);
int get_memtrace_enable(void);
void vpu_printk(const char *fmt, ...);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
#define vpu_wprintk vpu_printk
#else
void vpu_wprintk(const char *fmt, ...);
#endif

#endif // VPU_DEBUG_H
