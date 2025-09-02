/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#ifndef VPU_RM_H
#define VPU_RM_H

#include <linux/types.h>
#include <linux/platform_device.h>
#include "vpu_comm.h"

#define VRM_NAME_LEN           (16U)

#define MAX_VRMS               (64U)
#define MAX_VRM_GROUPS	       (4U)
#define MAX_VRM_VMEM           (22U)
#define MAX_VRM_VSW            (28U)
#define MAX_FREE_VA            (4U)

#define VRM_FLAG_SECURED       ((u32)1U << 1)
#define VRM_FLAG_SHARED        ((u32)1U << 2)
#define VRM_FLAG_VSTORED       ((u32)1U << 3)
#define VRM_FLAG_VSWED         ((u32)1U << 4)
#define VRM_FLAG_VSHARED       ((u32)1U << 5)
#define VRM_FLAG_VREAREND      ((u32)1U << 6)

#define vrm_is_secured(p)      (((p)->flags & VRM_FLAG_SECURED) != 0U)
#define vrm_is_shared(p)       (((p)->flags & VRM_FLAG_SHARED) != 0U)
#define vrm_is_vstored(p)      (((p)->flags & VRM_FLAG_VSTORED) != 0U)
#define vrm_is_vswed(p)        (((p)->flags & VRM_FLAG_VSWED) != 0U)
#define vrm_is_vshared(p)      (((p)->flags & VRM_FLAG_VSHARED) != 0U)
#define vrm_is_vrearend(p)     (((p)->flags & VRM_FLAG_VREAREND) != 0U)

typedef enum {
	BUFFER_ELSE,
	BUFFER_WORK,
	BUFFER_STREAM,
	BUFFER_SEQHEADER,
	BUFFER_FRAMEBUFFER,
	BUFFER_PS,
	BUFFER_SLICE,
	BUFFER_USERDATA
} Buffer_Type;

typedef struct {
	unsigned int request_size;
	phys_addr_t phy_addr;
	void *kernel_remap_addr;
	Buffer_Type buffer_type;
} MEM_ALLOC_INFO_t;

struct vpmap {
	char name[VRM_NAME_LEN];
	u64 base;
	u64 size;
	u32 groups;
	u32 rc;
	u32 flags;
};

struct vrm {
	char name[VRM_NAME_LEN];
	u64 base;
	u64 size;
	u64 pa;
	u64 used;
	void *va;
	u32 groups;
	u32 rc;
	u32 flags;
	u32 ip;
	s32 sidx[MAX_VRM_VMEM];
	u32 reserved;
};

s32 vrm_get_info(const char *name, struct vpmap *mem);
s32 vrm_release_info(const char *name);
s32 vrm_get_freemem(s32 idx);
s32 vrm_alloc_count(s32 idx);
s32 vrm_set_instance(s32 idx);
s32 vrm_get_instance(s32 idx);
unsigned int vrm_check_instance_available(void);
int vrm_check_index_availability(s32 idx);
void vrm_clear_instance(s32 idx);

int vrm_probe(struct platform_device *pdev);
VREMOVE_RET_TYPE vrm_remove(struct platform_device *pdev);

int vmem_proc_alloc_memory(int codec_type, MEM_ALLOC_INFO_t *alloc_info, vputype type);
int vmem_proc_free_memory(vputype type);

unsigned int vmem_get_free_memory(vputype type);
pgprot_t vmem_get_pgprot(pgprot_t ulOldProt, unsigned long ulPageOffset);

int vmem_alloc_count(int type);
int vmem_is_index_in_use(int nIdx);
int vmem_set_instance(int idx);
void vmem_get_instance(int *nIdx);
void mem_check_instance_available(unsigned int *szfreed);
void vmem_clear_instance(int nIdx);

void vmem_set_only_decode_mode(int bDec_only);
int vmem_is_cma_within_a_region(void *start_addr, void *end_addr,
				void *cmp_addr, unsigned int cmp_length);
int vmem_is_cma_within_a_region_cv(const void *start_addr, const void *end_addr,
				   void *cmp_addr, unsigned int cmp_length);
int vmem_is_cma_allocated_virt_region(void *start_virtaddr, unsigned int length);
int vmem_is_cma_allocated_virt_region_cv(const void *start_virtaddr,
					 unsigned int length);
int vmem_is_cma_allocated_phy_region(unsigned int start_phyaddr,
				     unsigned int length);
int vmem_init(void);
int vmem_config(void);
void vmem_deinit(void);
unsigned int vmem_get_freemem_size(vputype type);
#endif  /* VPU_RM_H */
