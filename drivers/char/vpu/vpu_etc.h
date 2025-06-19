// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_ETC_H
#define VPU_ETC_H

#include <linux/clk.h>
#include <linux/mm.h>

#include "vpu_type.h"
#include <video/telechips/tcc_video_common.h>
enum videoip {
	vip_vpu,
	vip_jpu,
	vip_wave410,
	vip_wave512,
	vip_wave420l
};

#ifdef CONFIG_VPU_TIME_MEASUREMENT
long long vetc_GetTimediff_us(long long time1, long long time2);
long long vetc_GetKtime(void);
void printMeasurementTime(void *pHandle, vputype type, int isDec, long long time_gap_us);
#endif

unsigned int vetc_reg_read(void *base_addr, unsigned int offset);
void vetc_reg_write(void *base_addr, unsigned int offset,
							unsigned int data);
void vetc_dump_reg_all(char *base_addr, unsigned char *str);

void vetc_reg_init(char *base_addr);
void *vetc_ioremap(phys_addr_t phy_addr, unsigned int size);
void vetc_iounmap(void *virt_addr);
void *vetc_memcpy(void *dest, const void *src,
					unsigned int count, unsigned int type);
void vetc_memset(void *ptr, int value, unsigned int num,
					unsigned int type);
void vetc_usleep(unsigned int uimin, unsigned int uimax);

unsigned int vetc_get_chip_name(void);
unsigned int vetc_get_chip_family(void);
unsigned int vetc_get_chip_rev(void);
int  vetc_check_ip_enabled(int ip_type);

void vetc_vm_flags_set(struct vm_area_struct *vma, vm_flags_t flags);

#endif // VPU_ETC_H_
