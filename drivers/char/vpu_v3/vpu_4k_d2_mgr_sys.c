// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_comm.h"

#ifdef ENABLE_VPU_DRV_4K_D2

#include "vpu_mgr_sys.h"
#include "vpu_4k_d2_mgr_sys.h"

#define dlog_4kd2s(msg...)  	V_DBG(VPU_DBG_INFO,  "[4K_D2_MGR_SYS][LOG]: " msg)
#define detail_4kd2s(msg...)  	V_DBG(VPU_DBG_DETAIL,  "[4K_D2_MGR_SYS][DETAIL]: " msg)
#define seq_4kd2s(msg...)     	V_DBG(VPU_DBG_SEQUENCE, "[4K_D2_MGR_SYS][SEQ]: " msg)
#define err_4kd2s(msg...)     	V_DBG(VPU_DBG_ERROR, "[4K_D2_MGR_SYS][Err]: " msg)

enum VPU_4KD2_CLOCK
{
	FBUS_VBUS_CLK = 0,
	FBUS_CHEVC_CLK,
	FBUS_BHEVC_CLK,
	VBUS_HEVC_BUS_CLK,
	VBUS_HEVC_CORE_CLK
};

enum VPU_4KD2_RESET
{
	VBUS_HEVC_BUS_RESET = 0,
	VBUS_HEVC_CORE_RESET
};

#if 0
#define USE_CLK_DYNAMIC_CTRL
#endif

#ifdef VBUS_QOS_MATRIX_CTL
inline void vbus_matrix(void)
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

void vmgr_4k_d2_enable_clock(vmgr_clock_t* vmgr_clk, int vbus_no_ctrl)
{
	// BCLK > CCLK > ACLK
	V_DBG(VPU_DBG_RSTCLK, "enter");

	if ((vmgr_clk->vpu_clk[FBUS_VBUS_CLK] != NULL) && (vbus_no_ctrl == 0))
	{
		(void)clk_prepare_enable(vmgr_clk->vpu_clk[FBUS_VBUS_CLK]);
	}

	if (vmgr_clk->vpu_clk[FBUS_CHEVC_CLK] != NULL)
	{
		(void)clk_prepare_enable(vmgr_clk->vpu_clk[FBUS_CHEVC_CLK]);
	}

	if (vmgr_clk->vpu_clk[FBUS_BHEVC_CLK] != NULL)
	{
		(void)clk_prepare_enable(vmgr_clk->vpu_clk[FBUS_BHEVC_CLK]);
	}

	if (vmgr_clk->vpu_clk[VBUS_HEVC_CORE_CLK] != NULL)
	{
		(void)clk_prepare_enable(vmgr_clk->vpu_clk[VBUS_HEVC_CORE_CLK]);
	}

	if (vmgr_clk->vpu_clk[VBUS_HEVC_BUS_CLK] != NULL)
	{
		(void)clk_prepare_enable(vmgr_clk->vpu_clk[VBUS_HEVC_BUS_CLK]);
	}

#ifdef VBUS_QOS_MATRIX_CTL
	vbus_matrix();
#endif

}

void vmgr_4k_d2_disable_clock(vmgr_clock_t* vmgr_clk, int vbus_no_ctrl)
{
	// ACLK > CCLK > BCLK
	V_DBG(VPU_DBG_RSTCLK, "enter");

	if (vmgr_clk->vpu_clk[VBUS_HEVC_BUS_CLK] != NULL)
	{
		clk_disable_unprepare(vmgr_clk->vpu_clk[VBUS_HEVC_BUS_CLK]);
	}

	if (vmgr_clk->vpu_clk[VBUS_HEVC_CORE_CLK] != NULL)
	{
		clk_disable_unprepare(vmgr_clk->vpu_clk[VBUS_HEVC_CORE_CLK]);
	}

	if (vmgr_clk->vpu_clk[FBUS_BHEVC_CLK] != NULL)
	{
		clk_disable_unprepare(vmgr_clk->vpu_clk[FBUS_BHEVC_CLK]);
	}

	if (vmgr_clk->vpu_clk[FBUS_CHEVC_CLK] != NULL)
	{
		clk_disable_unprepare(vmgr_clk->vpu_clk[FBUS_CHEVC_CLK]);
	}

#if !defined(VBUS_CLK_ALWAYS_ON)
	if ((vmgr_clk->vpu_clk[FBUS_VBUS_CLK] != NULL) && (vbus_no_ctrl == 0))
	{
		clk_disable_unprepare(vmgr_clk->vpu_clk[FBUS_VBUS_CLK]);
	}
#endif

}

void vmgr_4k_d2_get_clock(vmgr_clock_t* vmgr_clk, struct device_node *node)
{
	if (node == NULL)
	{
		err_4kd2s("device node is null");
	}

	vmgr_clk->vpu_clk[FBUS_VBUS_CLK] = of_clk_get(node, 0);
	VPU_BUG_ON(vmgr_clk->vpu_clk[FBUS_VBUS_CLK]);

	vmgr_clk->vpu_clk[FBUS_CHEVC_CLK] = of_clk_get(node, 1);
	VPU_BUG_ON(vmgr_clk->vpu_clk[FBUS_CHEVC_CLK]);

	vmgr_clk->vpu_clk[FBUS_BHEVC_CLK] = of_clk_get(node, 2);
	VPU_BUG_ON(vmgr_clk->vpu_clk[FBUS_BHEVC_CLK]);

	vmgr_clk->vpu_clk[VBUS_HEVC_BUS_CLK] = of_clk_get(node, 3);
	VPU_BUG_ON(vmgr_clk->vpu_clk[VBUS_HEVC_BUS_CLK]);

	vmgr_clk->vpu_clk[VBUS_HEVC_CORE_CLK] = of_clk_get(node, 4);
	VPU_BUG_ON(vmgr_clk->vpu_clk[VBUS_HEVC_CORE_CLK]);
}

void vmgr_4k_d2_put_clock(vmgr_clock_t* vmgr_clk)
{
	if (vmgr_clk->vpu_clk[FBUS_CHEVC_CLK] != NULL)
	{
		clk_put(vmgr_clk->vpu_clk[FBUS_CHEVC_CLK]);
		vmgr_clk->vpu_clk[FBUS_CHEVC_CLK] = NULL;
	}

	if (vmgr_clk->vpu_clk[FBUS_BHEVC_CLK] != NULL)
	{
		clk_put(vmgr_clk->vpu_clk[FBUS_BHEVC_CLK]);
		vmgr_clk->vpu_clk[FBUS_BHEVC_CLK] = NULL;
	}

	if (vmgr_clk->vpu_clk[VBUS_HEVC_BUS_CLK] != NULL)
	{
		clk_put(vmgr_clk->vpu_clk[VBUS_HEVC_BUS_CLK]);
		vmgr_clk->vpu_clk[VBUS_HEVC_BUS_CLK] = NULL;
	}

	if (vmgr_clk->vpu_clk[VBUS_HEVC_CORE_CLK] != NULL)
	{
		clk_put(vmgr_clk->vpu_clk[VBUS_HEVC_CORE_CLK]);
		vmgr_clk->vpu_clk[VBUS_HEVC_CORE_CLK] = NULL;
	}

	if (vmgr_clk->vpu_clk[FBUS_VBUS_CLK] != NULL)
	{
		clk_put(vmgr_clk->vpu_clk[FBUS_VBUS_CLK]);
		vmgr_clk->vpu_clk[FBUS_VBUS_CLK] = NULL;
	}
}

void vmgr_4k_d2_change_clock(vmgr_clock_t* vmgr_clk, unsigned int width, unsigned int height)
{
#ifdef USE_CLK_DYNAMIC_CTRL
	static unsigned int prev_resolution; // = 0x0;
	unsigned long vbus_clk_value, bhevc_clk_value = 0, chevc_clk_value = 0;
	unsigned long curr_resolution = (unsigned long)(width * height);
	unsigned int bclk_changed = 0x0;
	int err;

//pll: Based on 1500/800
// => path to configure PLL:
// bootable/bootloader/uboot/board/telechips/tcc8990_stb/clock.c,
// clock_init_early

#if 0 //Remove below lines if not necessary
	tcc_set_pll(PLL_VIDEO_0, ENABLE, 800000000, 2);
	tcc_set_pll(PLL_VIDEO_1, ENABLE, 1500000000, 2);
#endif

	if (prev_resolution != curr_resolution)
	{
		prev_resolution = curr_resolution;

		if (curr_resolution > (1920 * 1088))
		{
			vbus_clk_value = 800000000;
			bhevc_clk_value = 500000000;
			chevc_clk_value = 800000000;
		}
		else if (curr_resolution > (1280 * 720))
		{
			vbus_clk_value = 500000000;
			bhevc_clk_value = 250000000;
			chevc_clk_value = 500000000;
		}
		else if (curr_resolution > (720 * 480))
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
			err = clk_set_rate(vmgr_clk->vpu_clk[FBUS_BHEVC_CLK], vbus_clk_value);
			if (err != 0)
			{
				pr_err("cannot change vmgr_clk->vpu_clk[FBUS_VBUS_CLK] rate to %ld: %d\n",
				       vbus_clk_value, err);
			}
			else
			{
				bclk_changed |= 0x1;
			}
		}

		if (vmgr_clk->vpu_clk[FBUS_BHEVC_CLK] != NULL)
		{
			err = clk_set_rate(vmgr_clk->vpu_clk[FBUS_BHEVC_CLK], bhevc_clk_value);
			if (err != 0)
			{
				pr_err("cannot change vmgr_clk->vpu_clk[FBUS_BHEVC_CLK] rate to %ld: %d\n",
				       bhevc_clk_value, err);
			}
			else
			{
				bclk_changed |= 0x2;
			}
		}

		if (vmgr_clk->vpu_clk[FBUS_CHEVC_CLK] != NULL)
		{
			err = clk_set_rate(vmgr_clk->vpu_clk[FBUS_CHEVC_CLK], chevc_clk_value);
			if (err != 0)
			{
				pr_err("cannot change vmgr_clk->vpu_clk[FBUS_CHEVC_CLK] rate to %ld: %d\n",
				       chevc_clk_value, err);
			}
			else
			{
				bclk_changed |= 0x4;
			}
		}

		V_DBG(VPU_DBG_RSTCLK,
				"[0x%x]: %d x %d => clock : vbus(%d Mhz), hevc(%d/%d Mhz)",
				bclk_changed, width, height,
				(unsigned int)(vbus_clk_value / 1000000),
				(unsigned int)(chevc_clk_value / 1000000),
				(unsigned int)(bhevc_clk_value / 1000000));
	}
#else
	if ((width > 0) && (height > 0))
	{
		VPU_DONOTHING(width, height);
	}
#endif
}

void vmgr_4k_d2_get_reset(vmgr_clock_t* vmgr_clk, struct device_node *node)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	if (node == NULL)
	{
		err_4kd2s("device node is null");
	}
	else
	{
		vmgr_clk->bus_reset[VBUS_HEVC_BUS_RESET] = of_reset_control_get_by_index(node, 0);
		VPU_BUG_ON(vmgr_clk->bus_reset[VBUS_HEVC_BUS_RESET]);

		vmgr_clk->bus_reset[VBUS_HEVC_CORE_RESET] = of_reset_control_get_by_index(node, 1);
		VPU_BUG_ON(vmgr_clk->bus_reset[VBUS_HEVC_CORE_RESET]);
	}
#endif
}

void vmgr_4k_d2_put_reset(vmgr_clock_t* vmgr_clk)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	if (vmgr_clk->bus_reset[VBUS_HEVC_BUS_RESET] != NULL)
	{
		reset_control_put(vmgr_clk->bus_reset[VBUS_HEVC_BUS_RESET]);
		vmgr_clk->bus_reset[VBUS_HEVC_BUS_RESET] = NULL;
	}

	if (vmgr_clk->bus_reset[VBUS_HEVC_CORE_RESET] != NULL)
	{
		reset_control_put(vmgr_clk->bus_reset[VBUS_HEVC_CORE_RESET]);
		vmgr_clk->bus_reset[VBUS_HEVC_CORE_RESET] = NULL;
	}
#endif
}

int vmgr_4k_d2_get_reset_register(vmgr_clock_t* vmgr_clk)
{
	return 0;
}

void vmgr_4k_d2_hw_assert(vmgr_clock_t* vmgr_clk)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	V_DBG(VPU_DBG_RSTCLK, "enter");

	if (vmgr_clk->bus_reset[VBUS_HEVC_BUS_RESET] != NULL)
	{
		(void)reset_control_assert(vmgr_clk->bus_reset[VBUS_HEVC_BUS_RESET]);
	}

	if (vmgr_clk->bus_reset[VBUS_HEVC_CORE_RESET] != NULL)
	{
		(void)reset_control_assert(vmgr_clk->bus_reset[VBUS_HEVC_CORE_RESET]);
	}

	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)", vmgr_4k_d2_get_reset_register(vmgr_clk));
#endif
}

void vmgr_4k_d2_hw_deassert(vmgr_clock_t* vmgr_clk)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	V_DBG(VPU_DBG_RSTCLK, "enter");

	if (vmgr_clk->bus_reset[VBUS_HEVC_CORE_RESET] != NULL)
	{
		(void)reset_control_deassert(vmgr_clk->bus_reset[VBUS_HEVC_CORE_RESET]);
	}

	if (vmgr_clk->bus_reset[VBUS_HEVC_BUS_RESET] != NULL)
	{
		(void)reset_control_deassert(vmgr_clk->bus_reset[VBUS_HEVC_BUS_RESET]);
	}

	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)", vmgr_4k_d2_get_reset_register(vmgr_clk));
#endif
}

void vmgr_4k_d2_hw_reset(vmgr_clock_t* vmgr_clk)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	V_DBG(VPU_DBG_RSTCLK, "enter");

	udelay(1000); //1ms

	vmgr_4k_d2_hw_assert(vmgr_clk);

	udelay(1000); //1ms

	vmgr_4k_d2_hw_deassert(vmgr_clk);

	udelay(1000); //1ms

	V_DBG(VPU_DBG_RSTCLK, "out (rsr:0x%x)", vmgr_4k_d2_get_reset_register(vmgr_clk));
#endif
}

void vmgr_4k_d2_restore_clock(vmgr_clock_t* vmgr_clk, int vbus_no_ctrl, int opened_cnt)
{
	int opened_count = opened_cnt;

	vmgr_4k_d2_hw_assert(vmgr_clk);

	udelay(1000);	//1ms

	while (opened_count > 0)
	{
		vmgr_4k_d2_disable_clock(vmgr_clk, vbus_no_ctrl);
		opened_count--;
	}

	udelay(1000); //1ms

	opened_count = opened_cnt;
	while (opened_count > 0)
	{
		vmgr_4k_d2_enable_clock(vmgr_clk, vbus_no_ctrl);
		opened_count--;
	}

	vmgr_4k_d2_hw_deassert(vmgr_clk);
}

vmgr_clock_t vpu_4kd2_clock =
{
	.vpu_clk = {NULL, },
	.bus_reset = {NULL, },
	.enable_clock = vmgr_4k_d2_enable_clock,
	.disable_clock = vmgr_4k_d2_disable_clock,
	.get_clock = vmgr_4k_d2_get_clock,
	.put_clock = vmgr_4k_d2_put_clock,
	.change_clock = vmgr_4k_d2_change_clock,
	.get_reset = vmgr_4k_d2_get_reset,
	.put_reset = vmgr_4k_d2_put_reset,
	.get_reset_register = vmgr_4k_d2_get_reset_register,
	.hw_assert = vmgr_4k_d2_hw_assert,
	.hw_deassert = vmgr_4k_d2_hw_deassert,
	.hw_reset = vmgr_4k_d2_hw_reset,
	.restore_clock = vmgr_4k_d2_restore_clock
};

#endif //ENABLE_VPU_DRV_4K_D2
