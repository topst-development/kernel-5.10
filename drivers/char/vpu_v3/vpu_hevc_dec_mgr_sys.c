/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#include "vpu_comm.h"

#ifdef ENABLE_VPU_DRV_HEVCDEC

#include "vpu_mgr_sys.h"
#include "vpu_hevc_dec_mgr_sys.h"

#define dlog_hevcds(msg...)  	V_DBG(VPU_DBG_INFO,  "[HEVC_DEC_MGR_SYS][LOG]: " msg)
#define detail_hevcds(msg...)  	V_DBG(VPU_DBG_DETAIL,  "[HEVC_DEC_MGR_SYS][DETAIL]: " msg)
#define seq_hevcds(msg...)     	V_DBG(VPU_DBG_SEQUENCE, "[HEVC_DEC_MGR_SYS][SEQ]: " msg)
#define err_hevcds(msg...)     	V_DBG(VPU_DBG_ERROR, "[HEVC_DEC_MGR_SYS][Err]: " msg)
/*
#define dprintk_hevcds(msg...)  V_DBG(VPU_DBG_INFO,  "HEVC_DEC_MGR_SYS: " msg)
#define detailk_hevcds(msg...)  V_DBG(VPU_DBG_INFO,  "HEVC_DEC_MGR_SYS: " msg)
#define err_hevcds(msg...)      V_DBG(VPU_DBG_ERROR, "HEVC_DEC_MGR_SYS[Err]: " msg)
#define info_hevcds(msg...)     V_DBG(VPU_DBG_INFO,  "HEVC_DEC_MGR_SYS[Info]: " msg)
*/

enum VPU_HEVCDEC_CLOCK {
	FBUS_VBUS_CLK = 0,
	FBUS_CHEVC_CLK,
	FBUS_VHEVC_CLK,
	FBUS_BHEVC_CLK,
	VBUS_HEVC_BUS_CLK,
	VBUS_HEVC_CORE_CLK
};

enum VPU_HEVCDEC_RESET {
	VBUS_HEVC_BUS_RESET = 0,
#if !defined(CONFIG_ARCH_TCC897X)
	VBUS_HEVC_CORE_RESET
#endif
};

#ifdef VBUS_QOS_MATRIX_CTL
static inline void vbus_matrix(void)
{
	vetc_reg_write(0x15444100, 0x00, 0x0);	// PRI Core0 read - port0
	vetc_reg_write(0x15444100, 0x04, 0x0);	// PRI Core0 write - port0

	vetc_reg_write(0x15445100, 0x00, 0x3);	// PROC read - port1
	vetc_reg_write(0x15445100, 0x04, 0x3);	// PROC read - port1

	vetc_reg_write(0x15446100, 0x00, 0x2);	// SDMA read - port2
	vetc_reg_write(0x15446100, 0x04, 0x2);	// SDMA read - port2

	vetc_reg_write(0x15447100, 0x00, 0x0);	// SECON Core0 read - port3
	vetc_reg_write(0x15447100, 0x04, 0x0);	// SECON Core0 read - port3

	vetc_reg_write(0x15449100, 0x00, 0x1);	// DMA read - port4
	vetc_reg_write(0x15449100, 0x04, 0x1);	// DMA read - port4

	vetc_reg_write(0x1544A100, 0x00, 0x0);	// PRI Core1 read - port5
	vetc_reg_write(0x1544A100, 0x04, 0x0);	// PRI Core1 read - port5

	vetc_reg_write(0x1544B100, 0x00, 0x0);	// SECON Core1 read - port6
	vetc_reg_write(0x1544B100, 0x04, 0x0);	// SECON Core1 read - port6
}
#endif

static void vmgr_hevcdec_enable_clock(vmgr_clock_t *vmgr_clk, int vbus_no_ctrl)
{
	//  BCLK > CCLK > ACLK
	if ((vmgr_clk->vpu_clk[FBUS_VBUS_CLK] != NULL) && (vbus_no_ctrl == 0)) {
		(void)clk_prepare_enable(vmgr_clk->vpu_clk[FBUS_VBUS_CLK]);
	}

	if (vmgr_clk->vpu_clk[FBUS_CHEVC_CLK] != NULL) {
		(void)clk_prepare_enable(vmgr_clk->vpu_clk[FBUS_CHEVC_CLK]);
	}

#ifdef CONFIG_ARCH_TCC898X
	if (vmgr_clk->vpu_clk[FBUS_VHEVC_CLK] != NULL) {
		(void)clk_prepare_enable(vmgr_clk->vpu_clk[FBUS_VHEVC_CLK]);
	}
#endif

	if (vmgr_clk->vpu_clk[FBUS_BHEVC_CLK] != NULL) {
		(void)clk_prepare_enable(vmgr_clk->vpu_clk[FBUS_BHEVC_CLK]);
	}

#if defined(VBUS_CODA_CORE_CLK_CTRL)
	if (vmgr_clk->vpu_clk[VBUS_HEVC_CORE_CLK] != NULL) {
		(void)clk_prepare_enable(vmgr_clk->vpu_clk[VBUS_HEVC_CORE_CLK]);
	}
#endif

	if (vmgr_clk->vpu_clk[VBUS_HEVC_BUS_CLK] != NULL) {
		(void)clk_prepare_enable(vmgr_clk->vpu_clk[VBUS_HEVC_BUS_CLK]);
	}

#ifdef VBUS_QOS_MATRIX_CTL
	vbus_matrix();
#endif
}

static void vmgr_hevcdec_disable_clock(vmgr_clock_t *vmgr_clk, int vbus_no_ctrl)
{
	// ACLK > CCLK > BCLK
	if (vmgr_clk->vpu_clk[VBUS_HEVC_BUS_CLK] != NULL) {
		clk_disable_unprepare(vmgr_clk->vpu_clk[VBUS_HEVC_BUS_CLK]);
	}

#if defined(VBUS_CODA_CORE_CLK_CTRL)
	if (vmgr_clk->vpu_clk[VBUS_HEVC_CORE_CLK] != NULL) {
		clk_disable_unprepare(vmgr_clk->vpu_clk[VBUS_HEVC_CORE_CLK]);
	}
#endif

	if (vmgr_clk->vpu_clk[FBUS_BHEVC_CLK] != NULL) {
		clk_disable_unprepare(vmgr_clk->vpu_clk[FBUS_BHEVC_CLK]);
	}

#ifdef CONFIG_ARCH_TCC898X
	if (vmgr_clk->vpu_clk[FBUS_VHEVC_CLK] != NULL) {
		clk_disable_unprepare(vmgr_clk->vpu_clk[FBUS_VHEVC_CLK]);
	}
#endif

	if (vmgr_clk->vpu_clk[FBUS_CHEVC_CLK] != NULL) {
		clk_disable_unprepare(vmgr_clk->vpu_clk[FBUS_CHEVC_CLK]);
	}

#if !defined(VBUS_CLK_ALWAYS_ON)
	if ((vmgr_clk->vpu_clk[FBUS_VBUS_CLK] != NULL) && (vbus_no_ctrl == 0)) {
		clk_disable_unprepare(vmgr_clk->vpu_clk[FBUS_VBUS_CLK]);
	}
#endif
}

static void vmgr_hevcdec_get_clock(vmgr_clock_t *vmgr_clk, struct device_node *pnode)
{
	int i = 0;

	if (pnode == NULL) {
		err_hevcds("device node is null");
	}

	i = 0;
	vmgr_clk->vpu_clk[FBUS_VBUS_CLK] = of_clk_get(pnode, i);
	VPU_BUG_ON(vmgr_clk->vpu_clk[FBUS_VBUS_CLK]);

	i += 1;
	vmgr_clk->vpu_clk[FBUS_CHEVC_CLK] = of_clk_get(pnode, i);
	VPU_BUG_ON(vmgr_clk->vpu_clk[FBUS_CHEVC_CLK]);

#ifdef CONFIG_ARCH_TCC898X
	i += 1;
	vmgr_clk->vpu_clk[FBUS_VHEVC_CLK] = of_clk_get(pnode, i);
	VPU_BUG_ON(vmgr_clk->vpu_clk[FBUS_VHEVC_CLK]);
#endif

	i += 1;
	vmgr_clk->vpu_clk[FBUS_BHEVC_CLK] = of_clk_get(pnode, i);
	VPU_BUG_ON(vmgr_clk->vpu_clk[FBUS_BHEVC_CLK]);

	i += 1;
	vmgr_clk->vpu_clk[VBUS_HEVC_BUS_CLK] = of_clk_get(pnode, i);
	VPU_BUG_ON(vmgr_clk->vpu_clk[VBUS_HEVC_BUS_CLK]);

#if defined(VBUS_CODA_CORE_CLK_CTRL)
	i += 1;
	vmgr_clk->vpu_clk[VBUS_HEVC_CORE_CLK] = of_clk_get(pnode, i);
	VPU_BUG_ON(vmgr_clk->vpu_clk[VBUS_HEVC_CORE_CLK]);
#endif
}

static void vmgr_hevcdec_put_clock(vmgr_clock_t *vmgr_clk)
{
	if (vmgr_clk->vpu_clk[FBUS_CHEVC_CLK] != NULL) {
		clk_put(vmgr_clk->vpu_clk[FBUS_CHEVC_CLK]);
		vmgr_clk->vpu_clk[FBUS_CHEVC_CLK] = NULL;
	}

#ifdef CONFIG_ARCH_TCC898X
	if (vmgr_clk->vpu_clk[FBUS_VHEVC_CLK] != NULL) {
		clk_put(vmgr_clk->vpu_clk[FBUS_VHEVC_CLK]);
		vmgr_clk->vpu_clk[FBUS_VHEVC_CLK] = NULL;
	}
#endif

	if (vmgr_clk->vpu_clk[FBUS_BHEVC_CLK] != NULL) {
		clk_put(vmgr_clk->vpu_clk[FBUS_BHEVC_CLK]);
		vmgr_clk->vpu_clk[FBUS_BHEVC_CLK] = NULL;
	}

	if (vmgr_clk->vpu_clk[VBUS_HEVC_BUS_CLK] != NULL) {
		clk_put(vmgr_clk->vpu_clk[VBUS_HEVC_BUS_CLK]);
		vmgr_clk->vpu_clk[VBUS_HEVC_BUS_CLK] = NULL;
	}

#if defined(VBUS_CODA_CORE_CLK_CTRL)
	if (vmgr_clk->vpu_clk[VBUS_HEVC_CORE_CLK] != NULL) {
		clk_put(vmgr_clk->vpu_clk[VBUS_HEVC_CORE_CLK]);
		vmgr_clk->vpu_clk[VBUS_HEVC_CORE_CLK] = NULL;
	}
#endif

	if (vmgr_clk->vpu_clk[FBUS_VBUS_CLK] != NULL) {
		clk_put(vmgr_clk->vpu_clk[FBUS_VBUS_CLK]);
		vmgr_clk->vpu_clk[FBUS_VBUS_CLK] = NULL;
	}
}

static void vmgr_hevcdec_get_reset(vmgr_clock_t *vmgr_clk, struct device_node *pnode)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	if (pnode == NULL) {
		err_hevcds("device node is null");
	} else {
		V_DBG(VPU_DBG_RSTCLK, "enter");
		vmgr_clk->bus_reset[VBUS_HEVC_BUS_RESET]  = of_reset_control_get_by_index(pnode, 0);
		VPU_BUG_ON(vmgr_clk->bus_reset[VBUS_HEVC_BUS_RESET]);

#if !defined(CONFIG_ARCH_TCC897X)
		vmgr_clk->bus_reset[VBUS_HEVC_CORE_RESET] = of_reset_control_get_by_index(pnode, 1);
		VPU_BUG_ON(vmgr_clk->bus_reset[VBUS_HEVC_CORE_RESET]);
#endif
	}
#endif
}

static void vmgr_hevcdec_put_reset(vmgr_clock_t *vmgr_clk)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	if (vmgr_clk->bus_reset[VBUS_HEVC_BUS_RESET] != NULL) {
		reset_control_put(vmgr_clk->bus_reset[VBUS_HEVC_BUS_RESET]);
		vmgr_clk->bus_reset[VBUS_HEVC_BUS_RESET]  = NULL;
	}

#if !defined(CONFIG_ARCH_TCC897X)
	if (vmgr_clk->bus_reset[VBUS_HEVC_CORE_RESET] != NULL) {
		reset_control_put(vmgr_clk->bus_reset[VBUS_HEVC_CORE_RESET]);
		vmgr_clk->bus_reset[VBUS_HEVC_CORE_RESET] = NULL;
	}
#endif
#endif
}

static int vmgr_hevcdec_get_reset_register(vmgr_clock_t *vmgr_clk)
{
	return 0;
}

static void vmgr_hevcdec_hw_assert(vmgr_clock_t *vmgr_clk)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	// ACLK > CCLK > BCLK
	V_DBG(VPU_DBG_RSTCLK, "enter");

	if (vmgr_clk->bus_reset[VBUS_HEVC_BUS_RESET] != NULL) {
		(void)reset_control_assert(vmgr_clk->bus_reset[VBUS_HEVC_BUS_RESET]);
	}

#if !defined(CONFIG_ARCH_TCC897X)
	if (vmgr_clk->bus_reset[VBUS_HEVC_CORE_RESET] != NULL) {
		(void)reset_control_assert(vmgr_clk->bus_reset[VBUS_HEVC_CORE_RESET]);
	}
#endif
	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)", vmgr_hevcdec_get_reset_register(vmgr_clk));
#endif
}

static void vmgr_hevcdec_hw_deassert(vmgr_clock_t *vmgr_clk)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	//  BCLK > CCLK > ACLK
	V_DBG(VPU_DBG_RSTCLK, "enter");

#if !defined(CONFIG_ARCH_TCC897X)
	if (vmgr_clk->bus_reset[VBUS_HEVC_CORE_RESET] != NULL) {
		(void)reset_control_deassert(vmgr_clk->bus_reset[VBUS_HEVC_CORE_RESET]);
	}
#endif

	if (vmgr_clk->bus_reset[VBUS_HEVC_BUS_RESET] != NULL) {
		(void)reset_control_deassert(vmgr_clk->bus_reset[VBUS_HEVC_BUS_RESET]);
	}

	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)", vmgr_hevcdec_get_reset_register(vmgr_clk));
#endif
}

static void vmgr_hevcdec_hw_reset(vmgr_clock_t *vmgr_clk)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	udelay(1000); //1ms

	vmgr_hevcdec_hw_assert(vmgr_clk);
	udelay(1000); //1ms

	vmgr_hevcdec_hw_deassert(vmgr_clk);

	udelay(1000); //1ms
#endif
}

static void vmgr_hevcdec_restore_clock(vmgr_clock_t *vmgr_clk, int vbus_no_ctrl, int opened_cnt)
{
#if 1
	int opened_count = opened_cnt;

	vmgr_hevcdec_hw_assert(vmgr_clk);

	udelay(1000);	//1ms

	while (opened_count > 0) {
		vmgr_hevcdec_disable_clock(vmgr_clk, vbus_no_ctrl);
		opened_count--;
	}

	udelay(1000); //1ms

	opened_count = opened_cnt;
	while (opened_count > 0) {
		vmgr_hevcdec_enable_clock(vmgr_clk, vbus_no_ctrl);
		opened_count--;
	}

	vmgr_hevcdec_hw_deassert(vmgr_clk);
#else
	hmgr_hw_reset();
#endif
}

vmgr_clock_t vpu_hevc_dec_clock = {
	.vpu_clk = {NULL, },
	.bus_reset = {NULL, },
	.enable_clock = vmgr_hevcdec_enable_clock,
	.disable_clock = vmgr_hevcdec_disable_clock,
	.get_clock = vmgr_hevcdec_get_clock,
	.put_clock = vmgr_hevcdec_put_clock,
	.change_clock = NULL,
	.get_reset = vmgr_hevcdec_get_reset,
	.put_reset = vmgr_hevcdec_put_reset,
	.get_reset_register = vmgr_hevcdec_get_reset_register,
	.hw_assert = vmgr_hevcdec_hw_assert,
	.hw_deassert = vmgr_hevcdec_hw_deassert,
	.hw_reset = vmgr_hevcdec_hw_reset,
	.restore_clock = vmgr_hevcdec_restore_clock
};

#endif //ENABLE_VPU_DRV_HEVCDEC
