/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#include "vpu_comm.h"

#ifdef ENABLE_VPU_DRV_JPU_C6

#include "vpu_mgr_sys.h"
#include "jpu_mgr_sys.h"

#define dprintk_jpus(msg...)  V_DBG(VPU_DBG_INFO, "[JPU_MGR_SYS]:" msg)
#define detailk_jpus(msg...)  V_DBG(VPU_DBG_INFO, "[JPU_MGR_SYS]:" msg)
#define err_jpus(msg...)      V_DBG(VPU_DBG_INFO, "[JPU_MGR_SYS][Err]:" msg)
#define info_jpus(msg...)     V_DBG(VPU_DBG_INFO, "[JPU_MGR_SYS][Info]:" msg)

enum JPU_CLOCK {
	VBUS_JPEG_CLK = 0,
};

enum JPU_RESET {
	VBUS_JPEG_RESET = 0,
};

#define JPU_UDELAY(x)	udelay((unsigned long)(x))


static void jmgr_enable_clock(vmgr_clock_t *vmgr_clk, int vbus_no_ctrl)
{
	VPU_DONOTHING(vbus_no_ctrl);

	if (vmgr_clk->vpu_clk[VBUS_JPEG_CLK] != NULL) {
		(void)clk_prepare_enable(vmgr_clk->vpu_clk[VBUS_JPEG_CLK]);
	}
}

static void jmgr_disable_clock(vmgr_clock_t *vmgr_clk, int vbus_no_ctrl)
{
	VPU_DONOTHING(vbus_no_ctrl);

	if (vmgr_clk->vpu_clk[VBUS_JPEG_CLK] != NULL) {
		clk_disable_unprepare(vmgr_clk->vpu_clk[VBUS_JPEG_CLK]);
	}
}

static void jmgr_get_clock(vmgr_clock_t *vmgr_clk, struct device_node *node)
{
	if (node == NULL) {
		err_jpus("device node is null");
	} else {
		vmgr_clk->vpu_clk[VBUS_JPEG_CLK] = of_clk_get(node, 1);
		VPU_BUG_ON(vmgr_clk->vpu_clk[VBUS_JPEG_CLK]);
	}
}

static void jmgr_put_clock(vmgr_clock_t *vmgr_clk)
{
	if (vmgr_clk->vpu_clk[VBUS_JPEG_CLK] != NULL) {
		clk_put(vmgr_clk->vpu_clk[VBUS_JPEG_CLK]);
		vmgr_clk->vpu_clk[VBUS_JPEG_CLK] = NULL;
	}
}

static void jmgr_get_reset(vmgr_clock_t *vmgr_clk, struct device_node *node)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	if (node == NULL) {
		pr_info("device node is null\n");
	} else {
		vmgr_clk->bus_reset[VBUS_JPEG_RESET] = of_reset_control_get_by_index(node, 0);
		VPU_BUG_ON(vmgr_clk->bus_reset[VBUS_JPEG_RESET]);
	}
#endif
}

static void jmgr_put_reset(vmgr_clock_t *vmgr_clk)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	if (vmgr_clk->bus_reset[VBUS_JPEG_RESET] != NULL) {
		reset_control_put(vmgr_clk->bus_reset[VBUS_JPEG_RESET]);
		vmgr_clk->bus_reset[VBUS_JPEG_RESET] = NULL;
	}
#endif
}

static int jmgr_get_reset_register(vmgr_clock_t *vmgr_clk)
{
	return 0;
}

static void jmgr_hw_assert(vmgr_clock_t *vmgr_clk)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	V_DBG(VPU_DBG_RSTCLK, "enter");
	if (vmgr_clk->bus_reset[VBUS_JPEG_RESET] != NULL) {
		(void)reset_control_assert(vmgr_clk->bus_reset[VBUS_JPEG_RESET]);
	}

	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)", jmgr_get_reset_register(vmgr_clk));
#endif
}

static void jmgr_hw_deassert(vmgr_clock_t *vmgr_clk)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	V_DBG(VPU_DBG_RSTCLK, "enter");
	if (vmgr_clk->bus_reset[VBUS_JPEG_RESET] != NULL) {
		(void)reset_control_deassert(vmgr_clk->bus_reset[VBUS_JPEG_RESET]);
	}

	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)", jmgr_get_reset_register(vmgr_clk));
#endif
}

static void jmgr_hw_reset(vmgr_clock_t *vmgr_clk)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	JPU_UDELAY(1000);

	jmgr_hw_assert(vmgr_clk);

	JPU_UDELAY(1000);

	jmgr_hw_deassert(vmgr_clk);

	JPU_UDELAY(1000);
#endif
}

static void jmgr_restore_clock(vmgr_clock_t *vmgr_clk, int vbus_no_ctrl, int opened_cnt)
{
#if 1
	int opened_count = opened_cnt;

	jmgr_hw_assert(vmgr_clk);

	while (opened_count > 0) {
		jmgr_disable_clock(vmgr_clk, vbus_no_ctrl);
		opened_count--;
	}

	JPU_UDELAY(1000);

	opened_count = opened_cnt;
	while (opened_count > 0) {
		jmgr_enable_clock(vmgr_clk, vbus_no_ctrl);
		opened_count--;
	}

	jmgr_hw_deassert(vmgr_clk);
#else
	jmgr_hw_reset(vmgr_clk);
#endif
}

vmgr_clock_t jpu_clock = {
	.vpu_clk = {NULL, },
	.bus_reset = {NULL, },
	.enable_clock = jmgr_enable_clock,
	.disable_clock = jmgr_disable_clock,
	.get_clock = jmgr_get_clock,
	.put_clock = jmgr_put_clock,
	.change_clock = NULL,
	.get_reset = jmgr_get_reset,
	.put_reset = jmgr_put_reset,
	.get_reset_register = jmgr_get_reset_register,
	.hw_assert = jmgr_hw_assert,
	.hw_deassert = jmgr_hw_deassert,
	.hw_reset = jmgr_hw_reset,
	.restore_clock = jmgr_restore_clock
};

#endif //ENABLE_VPU_DRV_JPU_C6
