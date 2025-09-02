// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Telechips Inc.
 *
 * Telechips Fixed PLL clock implementation
 *
 * Fixed PLL cannot change it's clock after Kernel booting start.
 * These PLL are shared by Main / Sub Cluster.
 */

#include "divider.h"

LIST_HEAD(tc_divider_list);

static bool tc_clk_dbg_warn_divider = false;
module_param(tc_clk_dbg_warn_divider, bool, 0x1A4/* 0644 */);
MODULE_PARM_DESC(tc_clk_dbg_warn_divider,
		"Telechips Clock driver will print stack dump "
		"when divider clock operations are failed");



int tc_divider_enable(struct clk_hw *hw)
{
	int ret = 0;
	struct tc_divider	*clk_div = to_tc_divider(hw);
	struct arm_smccc_res	res = {0};


	arm_smccc_smc(SIP_CLK_V2_DCLKCTRL_ENABLE, clk_div->clk_tc.id,
			0, 0, 0, 0, 0, 0, &res);

	if (res.a0 != 0UL) {
		dev_err(clk_div->clk_tc.dev, "[%s] %s: failed. ret: %lu\n",
				__func__, clk_hw_get_name(&clk_div->clk_tc.hw),
				res.a0);
		WARN_ON(tc_clk_dbg_warn_on);
		ret = -EINVAL;
	}

	return ret;
}

int tc_divider_is_enabled(struct clk_hw *hw)
{
	int ret = 0;
	struct tc_divider	*clk_div = to_tc_divider(hw);
	struct arm_smccc_res	res = {0};


	arm_smccc_smc(SIP_CLK_V2_DCLKCTRL_IS_ENABLED, clk_div->clk_tc.id,
			0, 0, 0, 0, 0, 0, &res);

	if (res.a0 != 0UL) {
		dev_err(clk_div->clk_tc.dev, "[%s] %s: failed. ret: %lu\n",
				__func__, clk_hw_get_name(&clk_div->clk_tc.hw),
				res.a0);
		WARN_ON(tc_clk_dbg_warn_on);
		ret = -EINVAL;
	} else {
		if (res.a1 == 1UL) {
			ret = 1;
		} else {
			ret = 0;
		}
	}

	return ret;
}

void tc_divider_disable(struct clk_hw *hw)
{
	struct tc_divider	*clk_div = to_tc_divider(hw);
	struct arm_smccc_res	res = {0};

	arm_smccc_smc(SIP_CLK_V2_DCLKCTRL_DISABLE, clk_div->clk_tc.id,
			0, 0, 0, 0, 0, 0, &res);

	if (res.a0 != 0UL) {
		dev_err(clk_div->clk_tc.dev, "[%s] %s: failed. ret: %lu\n",
				__func__, clk_hw_get_name(&clk_div->clk_tc.hw),
				res.a0);
		WARN_ON(tc_clk_dbg_warn_on);
	}
}

unsigned long tc_divider_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct tc_divider	*clk_div = to_tc_divider(hw);
	struct arm_smccc_res	res = {0};

	arm_smccc_smc(SIP_CLK_V2_DCLKCTRL_GET, clk_div->clk_tc.id,
			0, 0, 0, 0, 0, 0, &res);
	if (res.a0 == 0UL) {
		clk_div->rate	= res.a1;
		clk_div->div	= res.a2;
	} else {
		dev_err(clk_div->clk_tc.dev, "[%s] %s: failed. ret: %lu\n",
				__func__, clk_hw_get_name(&clk_div->clk_tc.hw),
				res.a0);
		WARN_ON(tc_clk_dbg_warn_on);
	}

	return clk_div->rate;
}

int tc_divider_set_rate(struct clk_hw *hw, unsigned long rate, unsigned long parent_rate)
{
	struct tc_divider	*clk_div = to_tc_divider(hw);
	struct arm_smccc_res	res = {0};

	int ret = 0;

	unsigned long sip_cmd = 0;
	int ret = 0;

	sip_cmd = SIP_CLK_SET_DIVIDER;

	arm_smccc_smc(sip_cmd, clk_divider->clk_tc.id, CKC_ENABLE, rate,
			clk_divider->flags, 0, 0, 0, &res);

	if ((res.a0 != SMC_UNK) && (res.a3 == CKC_OK)) {

		clk_divider->rate	= res.a0;
		clk_divider->divider	= res.a1;

		dev_dbg(clk_divider->clk_tc.dev, "[%s] (%s) success rate: %llu divider: %u\n",
				clk_hw_get_name(&clk_divider->clk_tc.hw),
				__func__,
				clk_divider->rate,
				clk_divider->divider);
	} else {
		dev_err(clk_divider->clk_tc.dev, "[%s] (%s) failed. ret: 0x%lx\n",
				clk_hw_get_name(&clk_divider->clk_tc.hw),
				__func__, (res.a0 == SMC_UNK) ? res.a0 : res.a3);

		WARN_ON(tc_clk_dbg_warn_on ||tc_clk_dbg_warn_divider);

		ret = -EINVAL;
	}


	return ret;
}

int tc_divider_determine_rate(struct clk_hw *hw, struct clk_rate_request *req)
{
	struct tc_divider	*clk_div = to_tc_divider(hw);
	struct arm_smccc_res	res = {0};

	unsigned long sip_cmd = 0;
	int ret = 0;

	sip_cmd = SIP_CLK_DETERMINE_DIVIDER;

	arm_smccc_smc(sip_cmd, clk_divider->clk_tc.id, req->rate,
			clk_divider->flags, 0, 0, 0, 0, &res);

	if ((res.a0 != SMC_UNK) && (res.a3 == CKC_OK)) {

		req->rate = res.a0;
		req->best_parent_hw = clk_hw_get_parent(hw);
		/* XXX
		 * If CLK_SET_RATE_PARENT is set, CCF try to change parent clock
		 * Set beset_parent_rate same with best_parent_hw
		 */
		//req->best_parent_rate = clk_hw_get_rate(req->best_parent_hw);

		dev_dbg(clk_divider->clk_tc.dev, "[%s] (%s): success rate: %lu sel: %lu divider: %lu\n",
				clk_hw_get_name(&clk_divider->clk_tc.hw),
				__func__, res.a0, res.a1, res.a2);
	} else {
		dev_err(clk_divider->clk_tc.dev, "[%s] (%s) failed. ret: 0x%lx\n",
				clk_hw_get_name(&clk_divider->clk_tc.hw),
				__func__, (res.a0 == SMC_UNK) ? res.a0 : res.a3);

		WARN_ON(tc_clk_dbg_warn_on ||tc_clk_dbg_warn_divider);

		ret = -EINVAL;
	}

	return 0;
}



static const struct clk_ops tc_div_ops = {
	.recalc_rate	= tc_divider_recalc_rate,
	.enable		= tc_divider_enable,
	.is_enabled	= tc_divider_is_enabled,
	.disable	= tc_divider_disable,
	.determine_rate	= tc_divider_determine_rate,
	.set_rate	= tc_divider_set_rate,
};

static struct tc_divider *tc_do_divider_register(struct device *dev,
		struct clk_init_data *init, uint32_t index)
{
	struct device_node	*np = dev->of_node;
	struct tc_divider	*clk_div;
	struct tc_clk		*clk_tc;
	struct clk_hw		*hw;

	uint32_t flags = 0;
	int ret = 0;

	clk_div = devm_kzalloc(dev, sizeof(struct tc_divider), GFP_KERNEL);

	if (clk_div != NULL) {
		clk_tc = &clk_div->clk_tc;
		(void)of_property_read_u32_index(np, "tc-flags", index, &flags);
		clk_div->flags = flags;

		ret = tc_clk_prepare_clk_data(dev, init, clk_tc, index);
	} else {
		ret = -ENOMEM;
	}

	if (ret == 0) {
		clk_tc->hw.init = init;
		clk_tc->dev = dev;

		hw = &clk_tc->hw;
		ret = devm_clk_hw_register(dev, hw);
	}

	if ((ret != 0) && (clk_div != NULL)) {
		devm_kfree(dev, clk_div);
		clk_div = NULL;
	}

	if (ret == 0) {
		ret = tc_clk_register_clkdev(dev, hw);
	}

	return clk_div;
}



int tc_divider_register(struct platform_device *pdev)
{
	struct clk_hw_onecell_data 	*onecell_data= NULL;
	struct clk_init_data		init = {0};
	struct tc_divider		*clk_div;
	struct device			*dev = &pdev->dev;

	const char	**parent_names;
	uint32_t 	index, num_parents, unum_clks;

	int num_clks, ret = 0;

	parent_names = tc_get_parent_info(dev, &num_parents);
	if ((num_parents != 0U) && !(parent_names != NULL)) {
		/* If parents exist, parent_names should not be 0,
		 * May be failed to allocate memory. */
		ret = -ENOMEM;
	}

	if (ret == 0) {
		init.ops = &tc_div_ops;
		onecell_data = tc_prepare_onecell_data(dev, &num_clks);

	}

	if (onecell_data == NULL) {
		ret = -ENOMEM;
	} else if (num_clks >= 0) {
		unum_clks = (uint32_t)num_clks;
		/* [DR]
		 * The num_parents value is restricted under 255 by tcc_get_parent_info().
		 */
		init.num_parents = (u8)num_parents;
		init.parent_names = parent_names;

		for (index = 0U; index < unum_clks; index++) {
			clk_div = tc_do_divider_register(dev, &init, index);

			if (clk_div != NULL) {
				onecell_data->hws[index] = &clk_div->clk_tc.hw;
				list_add_tail(&(clk_div->clk_tc.list), &tc_divider_list);
			}
		}

		ret = devm_of_clk_add_hw_provider(dev, tc_onecell_get, onecell_data);
	} else {
		/* Do nothing */
	}

	if (parent_names != NULL) {
		devm_kfree(dev, parent_names);
	}

	return ret;
}
