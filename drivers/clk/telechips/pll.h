/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 Telechips Inc.
 *
 * Telechips Fixed PLL clock implementation
 *
 * Fixed PLL cannot change it's clock after Kernel booting start.
 * These PLL are shared by Main / Sub Cluster.
 */

#include "clk.h"

/* DPLL K used as signed variable for calculating but, treated as unsigned
 * variable for read/write. Avoid coverity defect by using union type. */
typedef union {
	int16_t	 s;
	uint16_t u;
} pll_k_t;

struct tc_pll {
	struct tc_clk	clk_tc;
	uint32_t	flags;

	uint32_t	en;
	uint64_t	rate;
	uint32_t	pllpms;
	uint32_t	pllcon;
};

static inline struct tc_pll *to_tc_pll(struct clk_hw *hw)
{
	struct tc_clk *clk_tc = to_tc_clk(hw);
	return (struct tc_pll *)container_of(clk_tc, struct tc_pll, clk_tc);
}

int tc_pll_fixed_enable(struct clk_hw *hw);
void tc_pll_fixed_disable(struct clk_hw *hw);

int tc_pll_enable(struct clk_hw *hw);
int tc_pll_is_enabled(struct clk_hw *hw);
void tc_pll_disable(struct clk_hw *hw);
unsigned long tc_pll_recalc_rate(struct clk_hw *hw, unsigned long parent_rate);
int tc_pll_determine_rate(struct clk_hw *hw, struct clk_rate_request *req);
int tc_pll_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate);
