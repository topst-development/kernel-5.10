// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef DW_EDMA_H
#define DW_EDMA_H

enum edma_data_direction {
	EDMA_NONE = 0,
	EDMA_TO_PNR = 1,
	EDMA_FROM_PNR = 2,
};

static inline s32 valid_edma_direction(enum edma_data_direction dir)
{
	return (dir == EDMA_TO_PNR) || (dir == EDMA_FROM_PNR);
}

#ifdef CONFIG_PCIE_DW_EP
s32 dw_edma_async_transfer(void *arg, enum edma_data_direction dir, u64 src, u64 dst, u32 sz, bool pending);
#else
static inline s32 dw_edma_async_transfer(void *arg, enum edma_data_direction dir, u64 src, u64 dst, u32 sz, bool pending)
{
	return -ENXIO;
}
#endif

#endif	/* DW_EDMA_H */

