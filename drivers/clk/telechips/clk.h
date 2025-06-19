/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2023 Telechips Inc.
 */

#ifndef TELECHIPS_CLK_H
#define TELECHIPS_CLK_H

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/arm-smccc.h>
#include <soc/telechips/smc.h>
#include <dt-bindings/clock/telechips,clk-common.h>

#if IS_ENABLED(CONFIG_ARCH_TCC750X)
#include <dt-bindings/clock/telechips,tcc750x-clks.h>
#elif IS_ENABLED(CONFIG_ARCH_TCC807X)
#include <dt-bindings/clock/telechips,tcc807x-clks.h>
#elif IS_ENABLED(CONFIG_ARCH_TCC805X)
#include <dt-bindings/clock/telechips,tcc805x-clks.h>
#elif IS_ENABLED(CONFIG_ARCH_TCN100X)
#include <dt-bindings/clock/telechips,tcn100x-clks.h>
#endif

/* Clock SiP Return values */
#define	CKC_OK			0UL
#define CKC_FAILED		BIT(0)
#define CKC_INVALID_SRC		BIT(1)
#define CKC_INVALID_ID		BIT(2)
#define CKC_INVALID_SIP		BIT(3)

#define CKC_NO_OPS		BIT(8)
#define CKC_NO_OPS_FN		BIT(9)

/* Clock Status */
#define CKC_DISABLE		0UL
#define CKC_ENABLE		1UL

/* STR flags */
#define CKC_RESUME		0UL
#define CKC_SUSPEND		1UL

/* Clock type for composite */
#define TC_CLK_TYPE_INVALID	0U
#define TC_CLK_TYPE_BUS		1U
#define TC_CLK_TYPE_PERI 	2U

/* Common osc clock frequency */
#define XIN_CLK_RATE	24000000UL	/* 24 MHz */
#define XTIN_CLK_RATE	32768UL		/* 32.768 KHz */

/* Clock Flags */
#ifdef TC_CLK_F_SRC
#undef TC_CLK_F_SRC
#define TC_CLK_F_SRC_MASK	((unsigned long)0x3FUL)
#define TC_CLK_F_SRC_FIXED	((unsigned long)1UL << 29)
#define TC_CLK_F_SRC(x)		(((unsigned long)(x) & TC_CLK_F_SRC_MASK) | TC_CLK_F_SRC_FIXED)
#endif


struct tc_clk {
	struct clk_hw hw;
	struct device *dev;
	struct list_head list;
	uint32_t id;
};

extern struct list_head tc_pll_list;
//extern struct list_head tc_divider_list;
extern struct list_head tc_composite_list;
extern struct list_head tc_gate_list;

extern bool tc_clk_dbg_warn_on;

struct platform_device;

static inline struct tc_clk *to_tc_clk(void *data)
{
	return (struct tc_clk *)container_of((data), struct tc_clk, hw);
}

const char **tc_get_parent_info(struct device *dev, uint32_t *num_parent);
int tc_clk_prepare_clk_data(struct device const *dev, struct clk_init_data *init,
		struct tc_clk *clk_tc, uint32_t index);
void tc_clk_register_clkdev(struct device *dev, struct clk_hw *hw);

struct clk_hw_onecell_data *tc_prepare_onecell_data(struct device *dev, int *num_clks);
struct clk_hw *tc_onecell_get(struct of_phandle_args *clkspec, void *data);

/* Clock additional feature */
int tc_clk_suspend(void);
void tc_clk_resume(void);


int tc_pll_register(struct platform_device *pdev);
int tc_composite_register(struct platform_device *pdev, uint32_t composite_type);
//int tc_divider_register(struct platform_device *pdev);
//int tc_mux_register(struct platform_device *pdev);
int tc_gate_register(struct platform_device *pdev);

#endif
