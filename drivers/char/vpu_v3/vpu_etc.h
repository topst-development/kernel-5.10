// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_ETC_H
#define VPU_ETC_H

#ifdef CONFIG_VPU_TIME_MEASUREMENT
int vetc_GetTimediff_ms(struct timeval time1, struct timeval time2);
#endif

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

char* vetc_strncpy(char* dest, const char* src, int len);

void vetc_vm_flags_set(struct vm_area_struct *vma, vm_flags_t flags);

#endif // VPU_ETC_H_
