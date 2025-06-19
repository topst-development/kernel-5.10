// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_MEM_H
#define VPU_MEM_H

typedef struct VPU_PhyMemInfo_t{
	unsigned long phys;        // physical address
	unsigned long size;        // allocation size
	int fd;
} VPU_PhyMemInfo_t;

int vmem_probe(struct platform_device *pdev);
int vmem_remove(struct platform_device *pdev);

int tcc_mem_create_dma_buf(VPU_PhyMemInfo_t *pmap_info);
int tcc_mem_release_dma_buf(int ifd);

#endif /*VPU_MEM_H*/
