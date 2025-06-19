// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_DBG_INFO__H
#define VPU_DBG_INFO__H

#include "vpu_comm.h"

extern int gs_iDebugTimeMeasure;
extern int gs_iDebugCqCount;
extern int gs_iDebugLogInfo;

#define DLOG_VERBOSE 	(1<<0)
#define DLOG_DEBUG   	(1<<1)
#define DLOG_INFO    	(1<<2)
#define DLOG_WARN    	(1<<3)
#define DLOG_ERROR   	(1<<4)
#define DLOG_ASSERT  	(1<<5)
#define DLOG_FUNC    	(1<<6)
#define DLOG_TRACE   	(1<<7)

void vpudebug_attr_init(void);
void vpudebug_attr_deinit(void);

#define KDLOGV(fmt, ...)                                     \
	if ((gs_iDebugLogInfo & DLOG_VERBOSE) == DLOG_VERBOSE) { \
		printk(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__);       \
	}
#define KDLOGD(fmt, ...)                                 \
	if ((gs_iDebugLogInfo & DLOG_DEBUG) == DLOG_DEBUG) { \
		printk(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__);   \
	}
#define KDLOGI(fmt, ...)                               \
	if ((gs_iDebugLogInfo & DLOG_INFO) == DLOG_INFO) { \
		printk(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__); \
	}
#define KDLOGW(fmt, ...)                               \
	if ((gs_iDebugLogInfo & DLOG_WARN) == DLOG_WARN) { \
		printk(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__); \
	}
#define KDLOGE(fmt, ...)                                 \
	if ((gs_iDebugLogInfo & DLOG_ERROR) == DLOG_ERROR) { \
		printk(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__);   \
	}
#define KDLOGA(fmt, ...)                                   \
	if ((gs_iDebugLogInfo & DLOG_ASSERT) == DLOG_ASSERT) { \
		printk(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__);     \
	}
#define KDLOGF(fmt, ...)                               \
	if ((gs_iDebugLogInfo & DLOG_FUNC) == DLOG_FUNC) { \
		printk(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__); \
	}
#define KDLOGT(fmt, ...)                                 \
	if ((gs_iDebugLogInfo & DLOG_TRACE) == DLOG_TRACE) { \
		printk(KERN_ALERT pr_fmt(fmt), ##__VA_ARGS__);   \
	}

#endif//VPU_DBG_INFO__H