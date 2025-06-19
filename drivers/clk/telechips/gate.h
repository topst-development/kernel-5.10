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

struct tc_gate {
	struct tc_clk	clk_tc;
	uint32_t	flags;

	uint32_t	en;
};

static inline struct tc_gate *to_tc_gate(struct clk_hw *hw)
{
	struct tc_clk *clk_tc = to_tc_clk(hw);
	return (struct tc_gate *)container_of(clk_tc, struct tc_gate, clk_tc);
}

int tc_gate_enable(struct clk_hw *hw);
int tc_gate_is_enabled(struct clk_hw *hw);
void tc_gate_disable(struct clk_hw *hw);
