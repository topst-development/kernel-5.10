// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_comm.h"

#ifdef ENABLE_VPU_DRV_HEVCENC2

#include "vpu_mgr_sys.h"
#include "vpu_hevc_enc2_mgr_sys.h"

#define dlog_hencs2(msg...)  	V_DBG(VPU_DBG_INFO, "[HEVC_ENC2_MGR_SYS][LOG]:" msg)
#define detail_hencs2(msg...)  	V_DBG(VPU_DBG_DETAIL, "[HEVC_ENC2_MGR_SYS][DETIAL]:" msg)
#define seq_hencs2(msg...)     	V_DBG(VPU_DBG_SEQUENCE, "[HEVC_ENC2_MGR_SYS][SEQ]:" msg)
#define err_hencs2(msg...)      V_DBG(VPU_DBG_ERROR, "[HEVC_ENC2_MGR_SYS][ERR]:" msg)

enum HEVCENC2_CLOCK {
	FBUS_VBUS_CLK = 0,
	FBUS_CHEVCENC_CLK,
	FBUS_BHEVCENC_CLK,
	VBUS_HEVC_ENC_CLK,
};

enum HEVCENC2_RESET {
	VBUS_HEVC_ENC_RESET,
};

void vmgr_hevc_enc2_enable_clock(vmgr_clock_t* vmgr_clk, int vbus_no_ctrl)
{
	V_DBG(VPU_DBG_RSTCLK, "vmgr_hevc_enc_enable_clock");

	if ((vmgr_clk->vpu_clk[FBUS_VBUS_CLK] != NULL) && (vbus_no_ctrl == 0))
	{
		clk_prepare_enable(vmgr_clk->vpu_clk[FBUS_VBUS_CLK]);
	}

	if (vmgr_clk->vpu_clk[FBUS_BHEVCENC_CLK] != NULL)
	{
		clk_prepare_enable(vmgr_clk->vpu_clk[FBUS_BHEVCENC_CLK]);
	}

	if (vmgr_clk->vpu_clk[FBUS_CHEVCENC_CLK] != NULL)
	{
		clk_prepare_enable(vmgr_clk->vpu_clk[FBUS_CHEVCENC_CLK]);
	}

	if (vmgr_clk->vpu_clk[VBUS_HEVC_ENC_CLK] != NULL)
	{
		clk_prepare_enable(vmgr_clk->vpu_clk[VBUS_HEVC_ENC_CLK]);
	}
}

void vmgr_hevc_enc2_disable_clock(vmgr_clock_t* vmgr_clk, int vbus_no_ctrl)
{
	V_DBG(VPU_DBG_RSTCLK, "vmgr_hevc_enc_disable_clock");

	if (vmgr_clk->vpu_clk[VBUS_HEVC_ENC_CLK] != NULL)
	{
		clk_disable_unprepare(vmgr_clk->vpu_clk[VBUS_HEVC_ENC_CLK]);
	}

	if (vmgr_clk->vpu_clk[FBUS_CHEVCENC_CLK] != NULL)
	{
		clk_disable_unprepare(vmgr_clk->vpu_clk[FBUS_CHEVCENC_CLK]);
	}

	if (vmgr_clk->vpu_clk[FBUS_BHEVCENC_CLK] != NULL)
	{
		clk_disable_unprepare(vmgr_clk->vpu_clk[FBUS_BHEVCENC_CLK]);
	}

#if !defined(VBUS_CLK_ALWAYS_ON)
	if ((vmgr_clk->vpu_clk[FBUS_VBUS_CLK] != NULL) && (vbus_no_ctrl == 0))
	{
		clk_disable_unprepare(vmgr_clk->vpu_clk[FBUS_VBUS_CLK]);
	}
#endif
}

void vmgr_hevc_enc2_get_clock(vmgr_clock_t* vmgr_clk, struct device_node *node)
{
	if (node == NULL)
	{
		V_DBG(VPU_DBG_ERROR, "device node is null");
	}

	vmgr_clk->vpu_clk[FBUS_VBUS_CLK] = of_clk_get(node, 0);
	VPU_BUG_ON(vmgr_clk->vpu_clk[FBUS_VBUS_CLK]);

	vmgr_clk->vpu_clk[FBUS_CHEVCENC_CLK] = of_clk_get(node, 1);
	VPU_BUG_ON(vmgr_clk->vpu_clk[FBUS_CHEVCENC_CLK]);

	vmgr_clk->vpu_clk[FBUS_BHEVCENC_CLK] = of_clk_get(node, 2);
	VPU_BUG_ON(vmgr_clk->vpu_clk[FBUS_BHEVCENC_CLK]);

	vmgr_clk->vpu_clk[VBUS_HEVC_ENC_CLK] = of_clk_get(node, 3);
	VPU_BUG_ON(vmgr_clk->vpu_clk[VBUS_HEVC_ENC_CLK]);
}

void vmgr_hevc_enc2_put_clock(vmgr_clock_t* vmgr_clk)
{
	if (vmgr_clk->vpu_clk[FBUS_CHEVCENC_CLK] != NULL)
	{
		clk_put(vmgr_clk->vpu_clk[FBUS_CHEVCENC_CLK]);
		vmgr_clk->vpu_clk[FBUS_CHEVCENC_CLK] = NULL;
	}

	if (vmgr_clk->vpu_clk[FBUS_BHEVCENC_CLK] != NULL)
	{
		clk_put(vmgr_clk->vpu_clk[FBUS_BHEVCENC_CLK]);
		vmgr_clk->vpu_clk[FBUS_BHEVCENC_CLK] = NULL;
	}

	if (vmgr_clk->vpu_clk[VBUS_HEVC_ENC_CLK] != NULL)
	{
		clk_put(vmgr_clk->vpu_clk[VBUS_HEVC_ENC_CLK]);
		vmgr_clk->vpu_clk[VBUS_HEVC_ENC_CLK] = NULL;
	}

	if (vmgr_clk->vpu_clk[FBUS_VBUS_CLK] != NULL)
	{
		clk_put(vmgr_clk->vpu_clk[FBUS_VBUS_CLK]);
		vmgr_clk->vpu_clk[FBUS_VBUS_CLK] = NULL;
	}
}

void vmgr_hevc_enc2_change_clock(vmgr_clock_t* vmgr_clk, unsigned int width, unsigned int height)
{
#ifdef USE_CLK_DYNAMIC_CTRL
	static unsigned int prev_resolution;	// = 0x0;
	unsigned long vbus_clk_value, bhevc_clk_value = 0, chevc_clk_value = 0;
	unsigned long curr_resolution = width * height;
	unsigned int bclk_changed = 0x0;
	int err;

//pll: Based on 1500/800
// => path to confiture PLL:
// bootable/bootloader/uboot/board/telechips/tcc8990_stb/clock.c,
// clock_init_early
#if 0 //Remove below lines if not necessary
	tcc_set_pll(PLL_VIDEO_0, ENABLE, 800000000, 2);
	tcc_set_pll(PLL_VIDEO_1, ENABLE, 1500000000, 2);
#endif

	if (prev_resolution == curr_resolution)
	{
		return;
	}

	prev_resolution = curr_resolution;

	if (curr_resolution > 1920 * 1088)
	{
		vbus_clk_value = 800000000;
		bhevc_clk_value = 500000000;
		chevc_clk_value = 800000000;
	}
	else if (curr_resolution > 1280 * 720)
	{
		vbus_clk_value = 500000000;
		bhevc_clk_value = 250000000;
		chevc_clk_value = 500000000;
	}
	else if (curr_resolution > 720 * 480)
	{
		vbus_clk_value = 300000000;
		bhevc_clk_value = 200000000;
		chevc_clk_value = 300000000;
	}
	else
	{	//curr_resolution > 0
		vbus_clk_value = 200000000;
		bhevc_clk_value = 150000000;
		chevc_clk_value = 200000000;
	}

	if (vmgr_clk->vpu_clk[FBUS_VBUS_CLK] != NULL)
	{
		err = clk_set_rate(vmgr_clk->vpu_clk[FBUS_VBUS_CLK], vbus_clk_value);
		if (err)
		{
			pr_err("cannot change vmgr_clk->vpu_clk[FBUS_VBUS_CLK] rate to %ld: %d\n", vbus_clk_value, err);
		}
		else
		{
			bclk_changed |= 0x1;
		}
	}

	if (vmgr_clk->vpu_clk[FBUS_BHEVCENC_CLK] != NULL)
	{
		err = clk_set_rate(vmgr_clk->vpu_clk[FBUS_BHEVCENC_CLK], bhevc_clk_value);
		if (err)
		{
			pr_err("cannot change vmgr_clk->vpu_clk[FBUS_BHEVCENC_CLK] rate to %ld: %d\n", bhevc_clk_value, err);
		}
		else
		{
			bclk_changed |= 0x2;
		}
	}

	if (vmgr_clk->vpu_clk[FBUS_CHEVCENC_CLK] != NULL)
	{
		err = clk_set_rate(vmgr_clk->vpu_clk[FBUS_CHEVCENC_CLK], chevc_clk_value);
		if (err)
		{
			pr_err("cannot change vmgr_clk->vpu_clk[FBUS_CHEVCENC_CLK] rate to %ld: %d\n", chevc_clk_value, err);
		}
		else
		{
			bclk_changed |= 0x4;
		}
	}

	(void)pr_info("%s-%d :[0x%x]: %d x %d => clock : vbus(%d Mhz), hevc(%d/%d Mhz)\n",
			 __func__, __LINE__,
			bclk_changed, width, height,
			(unsigned int)(vbus_clk_value/1000000),
			(unsigned int)(chevc_clk_value/1000000),
			(unsigned int)(bhevc_clk_value/1000000));
#endif
}

void vmgr_hevc_enc2_get_reset(vmgr_clock_t* vmgr_clk, struct device_node *node)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	if (node == NULL)
	{
		(void)pr_info("device node is null");
	}

	vmgr_clk->bus_reset[VBUS_HEVC_ENC_RESET] = of_reset_control_get_by_index(node, 0);
	VPU_BUG_ON(vmgr_clk->bus_reset[VBUS_HEVC_ENC_RESET]);
#endif
}

void vmgr_hevc_enc2_put_reset(vmgr_clock_t* vmgr_clk)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	if (vmgr_clk->bus_reset[VBUS_HEVC_ENC_RESET] != NULL)
	{
		reset_control_put(vmgr_clk->bus_reset[VBUS_HEVC_ENC_RESET]);
		vmgr_clk->bus_reset[VBUS_HEVC_ENC_RESET] = NULL;
	}
#endif
}

int vmgr_hevc_enc2_get_reset_register(vmgr_clock_t* vmgr_clk)
{
#ifdef ENABLE_LOG_RESET_REGISTER
	return vetc_reg_read(vbus, 0x4);
#else
	return 0;
#endif
}

void vmgr_hevc_enc2_hw_assert(vmgr_clock_t* vmgr_clk)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	V_DBG(VPU_DBG_RSTCLK, "enter");

	if (vmgr_clk->bus_reset[VBUS_HEVC_ENC_RESET] != NULL)
	{
		V_DBG(VPU_DBG_RSTCLK, "Video bus hevc encoder reset: assert (rsr:0x%x)",
			vmgr_hevc_enc2_get_reset_register(vmgr_clk));
		(void)reset_control_assert(vmgr_clk->bus_reset[VBUS_HEVC_ENC_RESET]);
	}

	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)", vmgr_hevc_enc2_get_reset_register(vmgr_clk));
#endif
}

void vmgr_hevc_enc2_hw_deassert(vmgr_clock_t* vmgr_clk)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	V_DBG(VPU_DBG_RSTCLK, "enter");

	if (vmgr_clk->bus_reset[VBUS_HEVC_ENC_RESET] != NULL)
	{
		V_DBG(VPU_DBG_RSTCLK, "Video bus hevc encoder reset: deassert (rsr:0x%x)",
			vmgr_hevc_enc2_get_reset_register(vmgr_clk));
		(void)reset_control_deassert(vmgr_clk->bus_reset[VBUS_HEVC_ENC_RESET]);
	}

	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)", vmgr_hevc_enc2_get_reset_register(vmgr_clk));
#endif
}

void vmgr_hevc_enc2_hw_reset(vmgr_clock_t* vmgr_clk)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	V_DBG(VPU_DBG_RSTCLK, "enter");

	udelay(1000); //1ms

	vmgr_hevc_enc2_hw_assert(vmgr_clk);

	udelay(1000); //1ms

	vmgr_hevc_enc2_hw_deassert(vmgr_clk);

	udelay(1000); //1ms

	V_DBG(VPU_DBG_RSTCLK, "out (rsr:0x%x)",
		vmgr_hevc_enc2_get_reset_register(vmgr_clk));
#endif
}

void vmgr_hevc_enc2_restore_clock(vmgr_clock_t* vmgr_clk, int vbus_no_ctrl, int opened_cnt)
{
#if 1 // unnecessary process: recommended by soc
	int opened_count = opened_cnt;

	V_DBG(VPU_DBG_RSTCLK, "opened_cnt: %d", opened_cnt);

	vmgr_hevc_enc2_hw_assert(vmgr_clk);

	udelay(1000); //1ms

	while (opened_count > 0)
	{
		vmgr_hevc_enc2_disable_clock(vmgr_clk, vbus_no_ctrl);
		opened_count--;
	}

	udelay(1000); //1ms

	opened_count = opened_cnt;
	while (opened_count > 0)
	{
		vmgr_hevc_enc2_enable_clock(vmgr_clk, vbus_no_ctrl);
		opened_count--;
	}

	udelay(1000); //1ms

	vmgr_hevc_enc2_hw_deassert(vmgr_clk);
#else
	vmgr_hevc_enc_hw_reset();
#endif
}

vmgr_clock_t vpu_hevc_enc2_clock =
{
	.vpu_clk = {NULL, },
	.bus_reset = {NULL, },
	.enable_clock = vmgr_hevc_enc2_enable_clock,
	.disable_clock = vmgr_hevc_enc2_disable_clock,
	.get_clock = vmgr_hevc_enc2_get_clock,
	.put_clock = vmgr_hevc_enc2_put_clock,
	.change_clock = vmgr_hevc_enc2_change_clock,
	.get_reset = vmgr_hevc_enc2_get_reset,
	.put_reset = vmgr_hevc_enc2_put_reset,
	.get_reset_register = vmgr_hevc_enc2_get_reset_register,
	.hw_assert = vmgr_hevc_enc2_hw_assert,
	.hw_deassert = vmgr_hevc_enc2_hw_deassert,
	.hw_reset = vmgr_hevc_enc2_hw_reset,
	.restore_clock = vmgr_hevc_enc2_restore_clock
};

#endif //ENABLE_VPU_DRV_HEVCENC2


