// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_LINUX_KERNEL_H
#define VPU_LINUX_KERNEL_H

#include <linux/miscdevice.h>
#include <linux/version.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <linux/io.h>
#include <asm/div64.h>
#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/debugfs.h>
#include <linux/reset.h>
#include <linux/memblock.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/types.h>
#include <linux/mman.h>
#include <linux/ptrace.h>
#include <linux/syscalls.h>
#include <linux/cpufreq.h>
#include <linux/utsname.h>
#include <linux/crc32.h>
#include <linux/firmware.h>
#include <linux/devcoredump.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/export.h>

#ifdef CONFIG_PROC_FS
#include <linux/list_sort.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif

#ifdef CONFIG_OPTEE
#include <linux/tee_drv.h>
#endif

#ifdef CONFIG_PMAP_TO_CMA
#include <linux/highmem.h>
#endif

#include <linux/io.h>
//#if (KERNEL_VERSION(2, 6, 39) <= LINUX_VERSION_CODE)
#include <asm/pgtable.h>
//#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 1, 0)
#include <soc/tcc/pmap.h>
#endif


//telechips header
#include <soc/telechips/chipinfo.h>

//vpu v3 header
#include <video/telechips/vpu_v3/tcc_vpu_v3_version.h>
#include <video/telechips/vpu_v3/tcc_vpu_v3_ioctl.h>
#include <video/telechips/vpu_v3/tcc_vpu_v3_decoder.h>
#include <video/telechips/vpu_v3/tcc_vpu_v3_encoder.h>

//vpu legacy header
#include <video/telechips/tcc_vpu_wbuffer.h>
#include <video/telechips/tcc_vpu_mem_ioctl.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)
	#define USE_ACCESS_POINT
#endif


#endif //VPU_LINUX_KERNEL_H

