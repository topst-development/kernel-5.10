/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#ifndef VPU_ETC_H
#define VPU_ETC_H

#include <video/telechips/TCCxxxx_VPU_CODEC_COMMON.h>
#include "vpu_comm.h"
#include "vpu_dllist.h"
#include "vpu_internal_type.h"

#ifdef CONFIG_VPU_TIME_MEASUREMENT
int vetc_GetTimediff_ms(struct timeval time1, struct timeval time2);
#endif

u8 vetc_reg_readb(int vpu_ip, u32 raw_base, void *ioaddr, int offset);
u16 vetc_reg_readw(int vpu_ip, u32 raw_base, void *ioaddr, int offset);
u32 vetc_reg_readl(int vpu_ip, u32 raw_base, void *ioaddr, int offset);

void vetc_reg_writeb(int vpu_ip, u32 raw_base, void *ioaddr, int offset, u8 val);
void vetc_reg_writew(int vpu_ip, u32 raw_base, void *ioaddr, int offset, u16 val);
void vetc_reg_writel(int vpu_ip, u32 raw_base, void *ioaddr, int offset, u32 val);

unsigned int vetc_reg_read(void *base_addr, unsigned int offset);
void vetc_reg_write(void *base_addr, unsigned int offset,
							unsigned int data);
void vetc_dump_reg_all(char *base_addr, unsigned char *str);

void vetc_reg_init(char *base_addr);
void *vetc_ioremap(unsigned int phy_addr, unsigned int size);
void vetc_iounmap(void *virt_addr);
void *vetc_memcpy(void *dest, const void *src,
					unsigned int count, unsigned int type);
void vetc_memset(void *ptr, int value, unsigned int num, unsigned int type);
void vetc_usleep(unsigned int uimin, unsigned int uimax);

void vetc_mutex_lock(void *lock);
void vetc_mutex_unlock(void *lock);

char *vetc_strncpy(char *dest, const char *src, int len);

void vetc_vm_flags_set(struct vm_area_struct *vma, vm_flags_t flags);

int vetc_prepare_firmware(struct platform_device *pdev, const enum vpu_ip_type ip_type, codec_addr_t *fw_addr);
#endif // VPU_ETC_H_
