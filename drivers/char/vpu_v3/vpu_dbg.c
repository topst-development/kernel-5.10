// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_comm.h"
#include "vpu_dbg.h"


/**
 * enum vpu_fw_dump_type - types of data in the dump file
 * @VPU_FW_DUMP_REGDUMP: Register dump in binary format
 * @VPU_FW_DUMP_DATA: Data dump in binary format
 */
enum vpu_fw_crash_dump_type {
	VPU_FW_DUMP_REGISTERS = 0,
	VPU_FW_DUMP_DATA = 1,
	VPU_FW_CRASH_DUMP_MAX,
};


unsigned int vpu_prt_mode = (unsigned int)VPU_PRT_SINCE_SET_DBG;
unsigned int vdbg_mask = (unsigned int)VPU_DBG_ERROR;
unsigned int vpu_memtrace_enable = 0U;

module_param_named(use_prtk, vpu_prt_mode, uint, 0644);
MODULE_PARM_DESC(use_prtk, "use always debug print mode (default: disabled(=0))");

module_param_named(debug_mask, vdbg_mask, uint, 0644);
MODULE_PARM_DESC(debug_mask, "debug output mask");

module_param_named(memtrace_enable, vpu_memtrace_enable, uint, 0644);
MODULE_PARM_DESC(memtrace_enable, "enable(1)/disable(0) memtrace (default 0)");

void vpu_dbg_get_info(int* type, int* mask)
{
	if (type != NULL)
	{
		*type = vpu_prt_mode;
	}

	if (mask != NULL)
	{
		*mask = vdbg_mask;
	}
}

EXPORT_SYMBOL(vpu_dbg_get_info);

static inline bool vpu_have_debug_mask(enum vdbg_mask_type dbg_mask)
{
	bool ret = (bool)false;

	if (((unsigned int)dbg_mask & vdbg_mask) == dbg_mask) {
		ret = (bool)true;
	}
	return ret;
}

void V_DEBUG(enum vdbg_mask_type dbg_mask, const char *fn, int ln, const char *fmt, ...)
{
	struct va_format vaf = {
		.fmt = fmt,
	};

	va_list args;
	va_start(args, fmt);
	vaf.va = &args;

	if (vpu_prt_mode == (unsigned int)VPU_ALWAYS_PRT_DBG) {
		if (((unsigned int)dbg_mask & (unsigned int)VPU_DBG_ERROR) == (unsigned int)VPU_DBG_ERROR) {
			(void)pr_err("[%s:%d] %pV\n", fn, ln, &vaf);
		} else {
			if (vpu_have_debug_mask(dbg_mask) == (bool)true) {
				(void)pr_err("[%s:%d] %pV\n", fn, ln, &vaf);
			}
		}
	} else {
		if (((unsigned int)dbg_mask & (unsigned int)VPU_DBG_ERROR) == (unsigned int)VPU_DBG_ERROR) {
			(void)pr_info("[%s:%d] %pV\n", fn, ln, &vaf);
		} else {
			if (vpu_have_debug_mask(dbg_mask) == (bool)true) {
				(void)pr_info("[%s:%d] %pV\n", fn, ln, &vaf);;
			}
		}
	}
	//trace_vpu_log_dbg(dev, &vaf);
	va_end(args);
}

EXPORT_SYMBOL(V_DEBUG);

int get_memtrace_enable(void)
{
	return vpu_memtrace_enable;
}
EXPORT_SYMBOL(get_memtrace_enable);

void vpu_printk(const char* fmt, ...)
{
	va_list args;
    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
}
EXPORT_SYMBOL(vpu_printk);


