// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_comm.h"
#include "vpu_sys_common.h"

#define IRQ_INT_TYPE    (((unsigned long)IRQ_TYPE_EDGE_RISING) | ((unsigned long)IRQF_SHARED))

void vmgr_enable_irq(unsigned int irq)
{
	enable_irq(irq);
}

void vmgr_disable_irq(unsigned int irq)
{
	disable_irq(irq);
}

void vmgr_free_irq(unsigned int irq, void *dev_id)
{
	(void)free_irq(irq, dev_id);
}

int vmgr_request_irq(unsigned int irq, irqreturn_t (*handler)(int irqh, void *dev_idh), unsigned long frags, const char *pdevice, void *dev_id)
{
	return request_irq(irq, handler, frags, pdevice, dev_id);
}

unsigned long vmgr_get_int_flags(void)
{
	return IRQ_INT_TYPE;
}

