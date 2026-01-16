/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#ifndef VPU_MEM_H
#define VPU_MEM_H

typedef struct VPU_PhyMemInfo_t{
	unsigned long phys;        // physical address
	unsigned long size;        // allocation size
	int fd;
} VPU_PhyMemInfo_t;

int vmem_probe(struct platform_device *pdev);
VREMOVE_RET_TYPE vmem_remove(struct platform_device *pdev);

int tcc_mem_create_dma_buf(VPU_PhyMemInfo_t *pmap_info);
int tcc_mem_release_dma_buf(int ifd);

#endif /*VPU_MEM_H*/
