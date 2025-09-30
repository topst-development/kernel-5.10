/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#ifndef VPU_SYS_COMMON_H
#define VPU_SYS_COMMON_H

#include "vpu_comm.h"

void vmgr_enable_irq(unsigned int irq);

void vmgr_disable_irq(unsigned int irq);

void vmgr_free_irq(unsigned int irq, void *dev_id);

int vmgr_request_irq(unsigned int irq, irqreturn_t (*handler)(int irqh, void *dev_idh), unsigned long frags, const char *pdevice, void *dev_id);

unsigned long vmgr_get_int_flags(void);

 #endif //VPU_SYS_COMMON_H

