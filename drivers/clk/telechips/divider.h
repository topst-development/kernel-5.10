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

struct tc_divider {
	unsigned long	id; /* Temporary use for STR (D3 & D5) */
	const char	*name;
	struct tc_clk	clk_tc;
	uint32_t	flags;

	uint32_t	en;
	uint64_t	rate;
	uint32_t	divider;
};

static inline struct tc_divider *to_tc_divider(struct clk_hw *hw)
{
	struct tc_clk *clk_tc = to_tc_clk(hw);
	return (struct tc_divider *)container_of(clk_tc, struct tc_divider, clk_tc);
}

int tc_divider_enable(struct clk_hw *hw);
int tc_divider_is_enabled(struct clk_hw *hw);
void tc_divider_disable(struct clk_hw *hw);
unsigned long tc_divider_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate);
int tc_divider_determine_rate(struct clk_hw *hw, struct clk_rate_request *req);
int tc_divider_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate);
