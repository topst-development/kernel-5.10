/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#include "vpu_comm.h"

#if defined(ENABLE_VPU_DRV_VPU_C7) || defined(ENABLE_VPU_DRV_VPU_D6)

#include "vpu_mgr_sys.h"
#include "vpu_c7_mgr_sys.h"

#define dlog_c7s(msg...)  	V_DBG(VPU_DBG_INFO, "[C7_MGR_SYS][LOG]: " msg)
#define detail_c7s(msg...)  V_DBG(VPU_DBG_DETAIL, "[C7_MGR_SYS][DETAIL]: " msg)
#define seq_c7s(msg...)     V_DBG(VPU_DBG_SEQUENCE, "[C7_MGR_SYS][SEQ]: "msg)
#define err_c7s(msg...)     V_DBG(VPU_DBG_ERROR, "[C7_MGR_SYS][ERR]: "msg)

enum VPU_C7_CLOCK {
	FBUS_VBUS_CLK = 0,
	FBUS_XODA_CLK,
	VBUS_XODA_CLK,
	VBUS_CORE_CLK,
} VPU_C7_Clock_e;

enum VPU_C7_RESET {
	VBUS_XODA_RESET = 0,
#if !defined(CONFIG_ARCH_TCC897X)
	VBUS_CORE_RESET,
#endif
};

static void vmgr_enable_clock(vmgr_clock_t *vmgr_clk, int vbus_no_ctrl)
{
	// BCLK > CCLK > ACLK
	V_DBG(VPU_DBG_RSTCLK, "enter");

	if ((vmgr_clk->vpu_clk[FBUS_VBUS_CLK] != NULL) && (vbus_no_ctrl == 0)) {
		(void)clk_prepare_enable(vmgr_clk->vpu_clk[FBUS_VBUS_CLK]);
	}

	if (vmgr_clk->vpu_clk[FBUS_XODA_CLK] != NULL) {
		(void)clk_prepare_enable(vmgr_clk->vpu_clk[FBUS_XODA_CLK]);
	}

#if defined(VBUS_CODA_CORE_CLK_CTRL)
	if (vmgr_clk->vpu_clk[VBUS_CORE_CLK] != NULL) {
		(void)clk_prepare_enable(vmgr_clk->vpu_clk[VBUS_CORE_CLK]);
	}
#endif

	if (vmgr_clk->vpu_clk[VBUS_XODA_CLK] != NULL) {
		(void)clk_prepare_enable(vmgr_clk->vpu_clk[VBUS_XODA_CLK]);
	}
}

static void vmgr_disable_clock(vmgr_clock_t  *vmgr_clk, int vbus_no_ctrl)
{
	// ACLK > CCLK > BCLK
	V_DBG(VPU_DBG_RSTCLK, "enter");

	if (vmgr_clk->vpu_clk[VBUS_XODA_CLK] != NULL) {
		clk_disable_unprepare(vmgr_clk->vpu_clk[VBUS_XODA_CLK]);
	}

#if defined(VBUS_CODA_CORE_CLK_CTRL)
	if (vmgr_clk->vpu_clk[VBUS_CORE_CLK] != NULL) {
		clk_disable_unprepare(vmgr_clk->vpu_clk[VBUS_CORE_CLK]);
	}
#endif

	if (vmgr_clk->vpu_clk[FBUS_XODA_CLK] != NULL) {
		clk_disable_unprepare(vmgr_clk->vpu_clk[FBUS_XODA_CLK]);
	}

#if !defined(VBUS_CLK_ALWAYS_ON)
	if ((vmgr_clk->vpu_clk[FBUS_VBUS_CLK] != NULL) && (vbus_no_ctrl == 0)) {
		clk_disable_unprepare(vmgr_clk->vpu_clk[FBUS_VBUS_CLK]);
	}
#endif
}

static void vmgr_get_clock(vmgr_clock_t *vmgr_clk, struct device_node *node)
{
	if (node == NULL) {
		err_c7s("device node is null");
	} else {
		vmgr_clk->vpu_clk[FBUS_VBUS_CLK] = of_clk_get(node, 0);
		VPU_BUG_ON(vmgr_clk->vpu_clk[FBUS_VBUS_CLK]);

		vmgr_clk->vpu_clk[FBUS_XODA_CLK] = of_clk_get(node, 1);
		VPU_BUG_ON(vmgr_clk->vpu_clk[FBUS_XODA_CLK]);

		vmgr_clk->vpu_clk[VBUS_XODA_CLK] = of_clk_get(node, 2);
		VPU_BUG_ON(vmgr_clk->vpu_clk[VBUS_XODA_CLK]);

#if defined(VBUS_CODA_CORE_CLK_CTRL)
		vmgr_clk->vpu_clk[VBUS_CORE_CLK] = of_clk_get(node, 3);
		VPU_BUG_ON(vmgr_clk->vpu_clk[VBUS_CORE_CLK]);
#endif
	}
}

static void vmgr_put_clock(vmgr_clock_t  *vmgr_clk)
{
	if (vmgr_clk->vpu_clk[FBUS_VBUS_CLK] != NULL) {
		clk_put(vmgr_clk->vpu_clk[FBUS_VBUS_CLK]);
		vmgr_clk->vpu_clk[FBUS_VBUS_CLK] = NULL;
	}

	if (vmgr_clk->vpu_clk[FBUS_XODA_CLK] != NULL) {
		clk_put(vmgr_clk->vpu_clk[FBUS_XODA_CLK]);
		vmgr_clk->vpu_clk[FBUS_XODA_CLK] = NULL;
	}

	if (vmgr_clk->vpu_clk[VBUS_XODA_CLK] != NULL) {
		clk_put(vmgr_clk->vpu_clk[VBUS_XODA_CLK]);
		vmgr_clk->vpu_clk[VBUS_XODA_CLK] = NULL;
	}

#if defined(VBUS_CODA_CORE_CLK_CTRL)
	if (vmgr_clk->vpu_clk[VBUS_CORE_CLK] != NULL) {
		clk_put(vmgr_clk->vpu_clk[VBUS_CORE_CLK]);
		vmgr_clk->vpu_clk[VBUS_CORE_CLK] = NULL;
	}
#endif
}

static void vmgr_get_reset(vmgr_clock_t *vmgr_clk, struct device_node *node)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	if (node == NULL) {
		err_c7s("device node is null");
	} else {
		V_DBG(VPU_DBG_RSTCLK, "enter");
		vmgr_clk->bus_reset[VBUS_XODA_RESET] = of_reset_control_get_by_index(node, 0);
		VPU_BUG_ON(vmgr_clk->bus_reset[VBUS_XODA_RESET]);

#if !defined(CONFIG_ARCH_TCC897X)
		vmgr_clk->bus_reset[VBUS_CORE_RESET] = of_reset_control_get_by_index(node, 1);
		VPU_BUG_ON(vmgr_clk->bus_reset[VBUS_CORE_RESET]);
#endif
	}
#endif
}

static void vmgr_put_reset(vmgr_clock_t *vmgr_clk)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	if (vmgr_clk->bus_reset[VBUS_XODA_RESET] != NULL) {
		reset_control_put(vmgr_clk->bus_reset[VBUS_XODA_RESET]);
		vmgr_clk->bus_reset[VBUS_XODA_RESET] = NULL;
	}

#if !defined(CONFIG_ARCH_TCC897X)
	if (vmgr_clk->bus_reset[VBUS_CORE_RESET] != NULL) {
		reset_control_put(vmgr_clk->bus_reset[VBUS_CORE_RESET]);
		vmgr_clk->bus_reset[VBUS_CORE_RESET] = NULL;
	}
#endif
#endif
}

static int vmgr_get_reset_register(vmgr_clock_t *vmgr_clk)
{
	return 0;
}

static void vmgr_hw_assert(vmgr_clock_t *vmgr_clk)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	//  ACLK > CCLK > BCLK
	V_DBG(VPU_DBG_RSTCLK, "enter");
	if (vmgr_clk->bus_reset[VBUS_XODA_RESET] != NULL) {
		(void)reset_control_assert(vmgr_clk->bus_reset[VBUS_XODA_RESET]);
	}

#if !defined(CONFIG_ARCH_TCC897X)
	if (vmgr_clk->bus_reset[VBUS_CORE_RESET] != NULL) {
		(void)reset_control_assert(vmgr_clk->bus_reset[VBUS_CORE_RESET]);
	}
#endif
	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)", vmgr_get_reset_register(vmgr_clk));
#endif
}

static void vmgr_hw_deassert(vmgr_clock_t *vmgr_clk)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	// BCLK > CCLK > ACLK
#if !defined(CONFIG_ARCH_TCC897X)
	V_DBG(VPU_DBG_RSTCLK, "enter");
	if (vmgr_clk->bus_reset[VBUS_CORE_RESET] != NULL) {
		(void)reset_control_deassert(vmgr_clk->bus_reset[VBUS_CORE_RESET]);
	}
#endif

	if (vmgr_clk->bus_reset[VBUS_XODA_RESET] != NULL) {
		(void)reset_control_deassert(vmgr_clk->bus_reset[VBUS_XODA_RESET]);
	}

	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)", vmgr_get_reset_register(vmgr_clk));
#endif
}

static void vmgr_hw_reset(vmgr_clock_t *vmgr_clk)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	udelay(1000);		//1ms

	vmgr_hw_assert(vmgr_clk);

	udelay(1000);		//1ms

	vmgr_hw_deassert(vmgr_clk);

	udelay(1000);		//1ms
#endif
}

static void vmgr_restore_clock(vmgr_clock_t *vmgr_clk, int vbus_no_ctrl, int opened_cnt)
{
	int opened_count = opened_cnt;

	vmgr_hw_assert(vmgr_clk);

	while (opened_count > 0) {
		vmgr_disable_clock(vmgr_clk, vbus_no_ctrl);
		opened_count--;
	}

	udelay(1000);		//1ms

	opened_count = opened_cnt;
	while (opened_count > 0) {
		vmgr_enable_clock(vmgr_clk, vbus_no_ctrl);
		opened_count--;
	}

	vmgr_hw_deassert(vmgr_clk);
}

vmgr_clock_t vpu_c7_clock = {
	.vpu_clk = {NULL, },
	.bus_reset = {NULL, },
	.enable_clock = vmgr_enable_clock,
	.disable_clock = vmgr_disable_clock,
	.get_clock = vmgr_get_clock,
	.put_clock = vmgr_put_clock,
	.change_clock = NULL,
	.get_reset = vmgr_get_reset,
	.put_reset = vmgr_put_reset,
	.get_reset_register = vmgr_get_reset_register,
	.hw_assert = vmgr_hw_assert,
	.hw_deassert = vmgr_hw_deassert,
	.hw_reset = vmgr_hw_reset,
	.restore_clock = vmgr_restore_clock
};

#endif //ENABLE_VPU_DRV_VPU_C7
