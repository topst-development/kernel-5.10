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

struct tc_composite {
	struct tc_clk	clk_tc;
	uint32_t	clk_type;
	uint32_t	flags;

	uint32_t	en;
	uint64_t	rate;
	uint32_t	sel;
	uint32_t	divider;
};

static inline struct tc_composite *to_tc_composite(struct clk_hw *hw)
{
	struct tc_clk *clk_tc = to_tc_clk(hw);
	return (struct tc_composite *)container_of(clk_tc, struct tc_composite, clk_tc);
}

int tc_composite_enable(struct clk_hw *hw);
int tc_composite_is_enabled(struct clk_hw *hw);
void tc_composite_disable(struct clk_hw *hw);
unsigned long tc_composite_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate);
int tc_composite_determine_rate(struct clk_hw *hw, struct clk_rate_request *req);
int tc_composite_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate);
u8 tc_composite_get_parent(struct clk_hw *hw);
int tc_composite_set_parent(struct clk_hw *hw, u8 index);
int tc_composite_set_rate_and_parent(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate, u8 index);
