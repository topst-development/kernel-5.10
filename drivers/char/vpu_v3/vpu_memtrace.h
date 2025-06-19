/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (C) 2020 Telechips Inc.
 */

#ifndef VPU_MEMTRACE_H
#define VPU_MEMTRACE_H

void* KALLOC(size_t size, const char* func_str, int func_line);
void KFREE(void *ptr, const char* func_str, int line);

//use this funciton
#define VPU_alloc(size)			KALLOC(size,  __FUNCTION__, __LINE__)
#define VPU_free(ptr)			KFREE(ptr, __FUNCTION__, __LINE__)

//show alloc status, return alloc count
int VPU_mem_usage(void);

//get alloc count
int VPU_get_alloc_count(void);

#endif //VPU_MEMTRACE_H