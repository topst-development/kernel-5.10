/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef VIN_WRAP_INTR_H
#define VIN_WRAP_INTR_H

/* VIN Interrupt source */
#define VIN0_IRQI	(607U)
#define VIN1_IRQI	(608U)
#define VIN2_IRQI	(609U)
#define VIN3_IRQI	(610U)
#define VIN_IRQI_MAC	(0x4U)

struct vin_wrap_intr_type {
	unsigned int id;
	unsigned int bits;
};

enum {
	VIN_WRAP_INTR_VIN0 = 0,
	VIN_WRAP_INTR_VIN1 = 1,
	VIN_WRAP_INTR_VIN2 = 2,
	VIN_WRAP_INTR_VIN3 = 3,
	VIN_WRAP_INTR_WDMA0 = 4,
	VIN_WRAP_INTR_WDMA1 = 5,
	VIN_WRAP_INTR_WDMA2 = 6,
	VIN_WRAP_INTR_WDMA3 = 7,
	VIN_WRAP_INTR_NUM = VIN_WRAP_INTR_WDMA3
};

/* VIN WRAPPER WDMA irqs */
enum vin_wrap_wdma_intr_src {
	VIN_WRAP_WDMA_INTR_UPD = 0,		/* Register Update */
	VIN_WRAP_WDMA_INTR_SREQ,		/* VIN_WRAP_WDMA_INTR_EOFF */
	VIN_WRAP_WDMA_INTR_ROL,			/* Rolling */
	VIN_WRAP_WDMA_INTR_ENR,			/* Synchronized Enable Rising */
	VIN_WRAP_WDMA_INTR_ENF,			/* Synchronized Enable Falling */
	VIN_WRAP_WDMA_INTR_EOFR,		/* EOF Rising */
	VIN_WRAP_WDMA_INTR_EOFF,		/* EOF Falling */
	VIN_WRAP_WDMA_INTR_SEOFR,		/* Sync EOF Rising */
	VIN_WRAP_WDMA_INTR_SEOFF,		/* Sync EOF Falling */
	VIN_WRAP_WDMA_INTR_RESERVED,
	VIN_WRAP_WDMA_INTR_MAX
};
#define VIN_WRAP_WDMA_INT_MASK			(((u32)1U << VIN_WRAP_WDMA_INTR_MAX) - 1U)

//#define VIN_WRAP_INTR_WD_OFFSET (VIN_WRAP_INTR_WD5 - (VIN_WRAP_INTR_WD4 + 1))

int vin_wrap_intr_enable(int irq, int id, unsigned int mask);
int vin_wrap_intr_disable(int irq, int id, unsigned int mask);
unsigned int vin_wrap_intr_get_status(int id);
bool is_vin_wrap_intr_activatied(int id, unsigned int mask);
int vin_wrap_intr_clear(int id, unsigned int mask);
bool is_vin_wrap_intr_unmasked(int id, unsigned int mask);
void vin_wrap_intr_init(void);

#endif
