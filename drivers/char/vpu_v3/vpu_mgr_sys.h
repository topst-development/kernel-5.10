// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_MGR_SYS_H
#define VPU_MGR_SYS_H

#include "vpu_comm.h"
#include "vpu_dbg.h"

#define MAX_VPU_CLOCK_CNT	6
#define MAX_VPU_RESET_CNT	2

typedef struct vmgr_clock_t
{
	struct clk *vpu_clk[MAX_VPU_CLOCK_CNT];
	struct reset_control *bus_reset[MAX_VPU_RESET_CNT];

	void (*enable_clock)(struct vmgr_clock_t* vmgr_clk, int vbus_no_ctrl);
	void (*disable_clock)(struct vmgr_clock_t* vmgr_clk, int vbus_no_ctrl);
	void (*get_clock)(struct vmgr_clock_t* vmgr_clk, struct device_node *node);
	void (*put_clock)(struct vmgr_clock_t* vmgr_clk);
	void (*change_clock)(struct vmgr_clock_t* vmgr_clk, unsigned int width, unsigned int height);
	void (*get_reset)(struct vmgr_clock_t* vmgr_clk, struct device_node *node);
	void (*put_reset)(struct vmgr_clock_t* vmgr_clk);
	int (*get_reset_register)(struct vmgr_clock_t* vmgr_clk);
	void (*hw_assert)(struct vmgr_clock_t* vmgr_clk);
	void (*hw_deassert)(struct vmgr_clock_t* vmgr_clk);
	void (*hw_reset)(struct vmgr_clock_t* vmgr_clk);
	void (*restore_clock)(struct vmgr_clock_t* vmgr_clk, int vbus_no_ctrl, int opened_cnt);
} vmgr_clock_t;

#endif //VPU_MGR_SYS_H