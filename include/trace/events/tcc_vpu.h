// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM tcc_vpu

#if !defined(_TRACE_TCC_VPU_H) || defined(TRACE_HEADER_MULTI_READ)
#define TRACE_TCC_VPU_H

#include <linux/device.h>
#include <linux/tracepoint.h>

TRACE_EVENT(tcc_vpu_rw,

		TP_PROTO(unsigned int rw, unsigned int con, unsigned int reg, unsigned int width, unsigned int val),

		TP_ARGS(rw, con, reg, width, val),

		TP_STRUCT__entry(
				__field(unsigned int, rw)
				__field(unsigned int, con)
				__field(unsigned int, reg)
				__field(unsigned int, width)
				__field(unsigned int, val)
		),

		TP_fast_assign(
				__entry->rw = rw;
				__entry->con = con;
				__entry->reg = reg;
				__entry->width = width;
				__entry->val = val;
		),

		TP_printk(
				"[VPU%d] %s 0x%08x--0x%08x %d 0x%08x",
				__entry->con,
				__entry->rw ? "WRITE" : "READ",
				__entry->reg, __entry->reg + (__entry->width-1),
				__entry->width, __entry->val)
);

#endif /* TRACE_TCC_VPU_H  */

/* This part must be outside protection */
#include <trace/define_trace.h>
