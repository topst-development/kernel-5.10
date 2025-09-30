/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * openedges npu driver
 *
 * Copyright (C) 2020 Openedges
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef ENLIGHT_NPU_H
#define ENLIGHT_NPU_H

#include <linux/ioctl.h>

enum {
	NPU_COLOR_YUV,
	NPU_COLOR_RGB,
	NPU_COLOR_END
};

struct buf_alloc_req {
	int size;
	unsigned long addr;
};

struct net_load_req {
	char *cmd_data;
	int cmd_size;

	char *wei_data;
	int wei_size;
};

struct net_run_req {
	int in_fd;
	int out_fd;
};

struct net_profile_req {
	int in_fd;
	int out_fd;

	unsigned int elapsed_in_us;

	unsigned int dma;
	unsigned int comp;
	unsigned int all;
};

struct npu_init_req {
	unsigned int disable_ue_fail;
	unsigned int disable_ce_fail;
	unsigned int disable_wdt;
	unsigned int soft_reset;

	// ONLY for CHIPTEST
	unsigned int dev_mlx_bin_idx;
	unsigned int dev_wdt_ext_cnt;
	unsigned int dev_wdt_int_cnt;
	unsigned int dev_ecc_test_ctrl;
	unsigned int dev_mlx_err_inj_mask_data;
	unsigned int dev_mlx_err_inj_mask_par;
};

struct mlx_init_req {
	char *kernel;
	int kernel_size;
};

struct reg_access_req {
	unsigned int addr;
	unsigned int data;
};

#define MAX_NUM_NPU_CORE    (0x4U)

typedef struct {
	unsigned int ue_irq_flag;
	unsigned int ce_irq_flag;
	unsigned int ue_status;
	unsigned int ce_status;
	unsigned int ue_cnt;
	unsigned int ce_cnt;
	unsigned int ce_addr;
	unsigned int ce_data;
	unsigned int ue_addr;
	unsigned int ue_data;
} ecc_sram_t;

typedef struct {
	unsigned int ue_irq_flag;
	unsigned int ce_irq_flag;
	unsigned int ue_cnt;
	unsigned int ce_cnt;
} ecc_gbuf_t;

typedef struct {
	unsigned int ue_irq_flag;
	unsigned int ce_irq_flag;
	unsigned int ue_cnt;
	unsigned int ce_cnt;
} ecc_cbuf_t;

typedef struct {
	ecc_cbuf_t cbuf;
	ecc_gbuf_t gbuf[MAX_NUM_NPU_CORE];
	ecc_sram_t sram[MAX_NUM_NPU_CORE];
	unsigned int wdt_to;
	unsigned int irq_reason;
} ecc_wdt_access_req_t;

#define NPU_IOCTL_MAGIC                 'k'
#define NPU_IOCTL_INIT_MLX              _IOW(NPU_IOCTL_MAGIC,  0, struct mlx_init_req *)
#define NPU_IOCTL_ALLOC_BUFFER          _IOW(NPU_IOCTL_MAGIC,  1, struct buf_alloc_req *)
#define NPU_IOCTL_LOAD_NETWORK          _IOW(NPU_IOCTL_MAGIC,  2, struct net_load_req *)
#define NPU_IOCTL_READ_REG              _IOWR(NPU_IOCTL_MAGIC, 3, struct reg_access_req *)
#define NPU_IOCTL_WRITE_REG             _IOW(NPU_IOCTL_MAGIC,  4, struct reg_access_req *)
#define NPU_IOCTL_RESET_NPU             _IOW(NPU_IOCTL_MAGIC,  5, struct npu_init_req *)
#define NPU_IOCTL_READ_ECC              _IOWR(NPU_IOCTL_MAGIC, 6, struct ecc_wdt_access_req_t*)

#define NPU_NET_IOCTL_RUN               _IOW(NPU_IOCTL_MAGIC,  0, struct net_run_req *)
#define NPU_NET_IOCTL_PROFILE           _IOWR(NPU_IOCTL_MAGIC, 1, struct net_profile_req *)
#define NPU_NET_IOCTL_SET_COLOR_FMT     _IOW(NPU_IOCTL_MAGIC,  2, int)

#endif //ENLIGHT_NPU_H
