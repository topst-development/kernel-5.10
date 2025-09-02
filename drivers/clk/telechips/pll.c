// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Telechips Inc.
 *
 * Telechips Fixed PLL clock implementation
 *
 * Fixed PLL cannot change it's clock after Kernel booting start.
 * These PLL are shared by Main / Sub Cluster.
 */

#include "pll.h"

LIST_HEAD(tc_pll_list);


static bool tc_clk_dbg_warn_pll = false;
module_param(tc_clk_dbg_warn_pll, bool, 0x1A4/* 0644 */);
MODULE_PARM_DESC(tc_clk_dbg_warn_pll,
		"Telechips Clock driver will print stack dump "
		"when pll clock operations are failed");

static int tc_pll_enable_chkret(struct tc_pll *clk_pll,
		struct arm_smccc_res const *res)
{
	int ret = 0;

	if (res->a3 == CKC_OK) {
		clk_pll->en = (uint32_t)CKC_ENABLE;
		dev_dbg(clk_pll->clk_tc.dev, "[%s] %s: success\n",
				__func__,
				clk_hw_get_name(&clk_pll->clk_tc.hw));
	} else {
		dev_err(clk_pll->clk_tc.dev, "[%s] %s: failed. ret: %lu\n",
				__func__, clk_hw_get_name(&clk_pll->clk_tc.hw),
				res->a3);
		WARN_ON(tc_clk_dbg_warn_on || tc_clk_dbg_warn_pll);
		ret = -EINVAL;
	}

	return ret;
}

int tc_pll_enable(struct clk_hw *hw)
{
	struct tc_pll		*clk_pll = to_tc_pll(hw);
	struct arm_smccc_res	res = {0};

	unsigned long sip_cmd = SIP_CLK_ENABLE_PLL;
	int ret = 0;

	arm_smccc_smc(sip_cmd, clk_pll->clk_tc.id,
			0, 0, 0, 0, 0, 0, &res);

	ret = tc_pll_enable_chkret(clk_pll, &res);

	return ret;
}

static int tc_pll_is_enabled_chkret(struct tc_pll *clk_pll,
		struct arm_smccc_res const *res)
{
	int ret = 0;

	if (res->a3 == CKC_OK) {
		if (res->a0 == 1UL) {
			clk_pll->en = (uint32_t)CKC_ENABLE;
			ret = 1;
		} else {
			clk_pll->en = (uint32_t)CKC_DISABLE;
			ret = 0;
		}
		dev_dbg(clk_pll->clk_tc.dev, "[%s] %s: success. status: %s\n",
				__func__,
				clk_hw_get_name(&clk_pll->clk_tc.hw),
				(res->a0 == 1UL) ? "enabled" : "disabled");
	} else {
		dev_err(clk_pll->clk_tc.dev, "[%s] %s: failed. ret: %lu\n",
				__func__, clk_hw_get_name(&clk_pll->clk_tc.hw),
				res->a0);
		WARN_ON(tc_clk_dbg_warn_on || tc_clk_dbg_warn_pll);
		ret = -EINVAL;
	}

	return ret;
}

int tc_pll_is_enabled(struct clk_hw *hw)
{
	struct tc_pll		*clk_pll = to_tc_pll(hw);
	struct arm_smccc_res	res = {0};

	unsigned long sip_cmd = SIP_CLK_IS_PLL_ENABLED;
	int ret = 0;

	arm_smccc_smc(sip_cmd, clk_pll->clk_tc.id,
			0, 0, 0, 0, 0, 0, &res);

	ret = tc_pll_is_enabled_chkret(clk_pll, &res);

	return ret;
}

static void tc_pll_disable_chkret(struct tc_pll *clk_pll,
		struct arm_smccc_res const *res)
{
	if (res->a3 == CKC_OK) {
		clk_pll->en = (uint32_t)CKC_DISABLE;
		dev_dbg(clk_pll->clk_tc.dev, "[%s] %s: success\n",
				__func__,
				clk_hw_get_name(&clk_pll->clk_tc.hw));
	} else {
		dev_err(clk_pll->clk_tc.dev, "[%s] %s: failed. ret: %lu\n",
				__func__, clk_hw_get_name(&clk_pll->clk_tc.hw),
				res->a3);
		WARN_ON(tc_clk_dbg_warn_on || tc_clk_dbg_warn_pll);
	}
}

void tc_pll_disable(struct clk_hw *hw)
{
	struct tc_pll		*clk_pll = to_tc_pll(hw);
	struct arm_smccc_res	res = {0};

	unsigned long sip_cmd = SIP_CLK_DISABLE_PLL;

	arm_smccc_smc(sip_cmd, clk_pll->clk_tc.id,
			0, 0, 0, 0, 0, 0, &res);

	tc_pll_disable_chkret(clk_pll, &res);
}

static void tc_pll_recalc_rate_chkret(struct tc_pll *clk_pll,
		struct arm_smccc_res const *res)
{
	if (res->a3 == CKC_OK) {
		clk_pll->rate	= res->a0;
		clk_pll->pllpms	= (res->a1 >= UINT_MAX) ?
			UINT_MAX : (uint32_t)res->a1;
		clk_pll->pllcon	= (res->a2 >= UINT_MAX) ?
			UINT_MAX : (uint32_t)res->a2;

		dev_dbg(clk_pll->clk_tc.dev, "[%s] %s: success rate: %lu pllpms: 0x%08lX pllcon: 0x%08lX\n",
				__func__,
				clk_hw_get_name(&clk_pll->clk_tc.hw),
				res->a0, res->a1, res->a2);
	} else {
		dev_err(clk_pll->clk_tc.dev, "[%s] %s: failed. ret: %lu\n",
				__func__, clk_hw_get_name(&clk_pll->clk_tc.hw),
				res->a3);
		WARN_ON(tc_clk_dbg_warn_on || tc_clk_dbg_warn_pll);
	}
}

unsigned long tc_pll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct tc_pll		*clk_pll = to_tc_pll(hw);
	struct arm_smccc_res	res = {0};

	unsigned long sip_cmd = SIP_CLK_GET_PLL;

	(void)parent_rate;

	arm_smccc_smc(sip_cmd, clk_pll->clk_tc.id,
			0, 0, 0, 0, 0, 0, &res);

	tc_pll_recalc_rate_chkret(clk_pll, &res);

	return clk_pll->rate;
}

static int tc_pll_set_rate_chkret(struct tc_pll *clk_pll,
		struct arm_smccc_res const *res, unsigned long rate)
{
	int ret = 0;

	if (res->a3 == CKC_OK) {
		clk_pll->rate	= res->a0;
		clk_pll->pllpms	= (res->a1 >= UINT_MAX) ?
			UINT_MAX : (uint32_t)res->a1;
		clk_pll->pllcon	= (res->a2 >= UINT_MAX) ?
			UINT_MAX : (uint32_t)res->a2;

		dev_dbg(clk_pll->clk_tc.dev, "[%s] %s: success rate: %lu pllpms: 0x%08lX pllcon: 0x%08lX\n",
				__func__,
				clk_hw_get_name(&clk_pll->clk_tc.hw),
				res->a0, res->a1, res->a2);
	} else {
		dev_err(clk_pll->clk_tc.dev, "[%s] %s: failed. ret: %lu (req_rate: %lu)\n",
				__func__, clk_hw_get_name(&clk_pll->clk_tc.hw),
				res->a3, rate);
		WARN_ON(tc_clk_dbg_warn_on || tc_clk_dbg_warn_pll);
		ret = -EINVAL;
	}

	return ret;
}

int tc_pll_set_rate(struct clk_hw *hw,
		unsigned long rate, unsigned long parent_rate)
{
	struct tc_pll		*clk_pll = to_tc_pll(hw);
	struct arm_smccc_res	res = {0};

	unsigned long sip_cmd = SIP_CLK_SET_PLL;
	int ret = 0;

	(void)parent_rate;

	arm_smccc_smc(sip_cmd, clk_pll->clk_tc.id, rate, (uint32_t)CKC_ENABLE,
			0, 0, 0, 0, &res);

	ret = tc_pll_set_rate_chkret(clk_pll, &res, rate);

	return ret;
}

static int tc_pll_determine_rate_chkret(struct tc_pll *clk_pll,
		struct arm_smccc_res const *res, unsigned long rate)
{
	int ret = 0;

	if (res->a3 == CKC_OK) {
		clk_pll->rate	= res->a0;
		clk_pll->pllpms	= (res->a1 >= UINT_MAX) ?
			UINT_MAX : (uint32_t)res->a1;
		clk_pll->pllcon	= (res->a2 >= UINT_MAX) ?
			UINT_MAX : (uint32_t)res->a2;

		dev_dbg(clk_pll->clk_tc.dev, "[%s] %s: success rate: %lu pllpms: 0x%08X pllcon: 0x%08X\n",
				__func__,
				clk_hw_get_name(&clk_pll->clk_tc.hw),
				res->a0, clk_pll->pllpms, clk_pll->pllcon);
	} else {
		dev_err(clk_pll->clk_tc.dev, "[%s] %s: failed. ret: %lu (req_rate: %lu)\n",
				__func__, clk_hw_get_name(&clk_pll->clk_tc.hw),
				res->a3, rate);
		WARN_ON(tc_clk_dbg_warn_on || tc_clk_dbg_warn_pll);
		ret = -EINVAL;
	}

	return ret;
}

int tc_pll_determine_rate(struct clk_hw *hw, struct clk_rate_request *req)
{
	struct tc_pll		*clk_pll = to_tc_pll(hw);
	struct arm_smccc_res	res = {0};

	unsigned long sip_cmd = SIP_CLK_DETERMINE_PLL;
	int ret = 0;

	arm_smccc_smc(sip_cmd, clk_pll->clk_tc.id, req->rate,
			0, 0, 0, 0, 0, &res);

	ret = tc_pll_determine_rate_chkret(clk_pll, &res, req->rate);
	if (ret == 0) {
		req->rate = res.a0;
		/* PLLs only get XIN as a clock source */
		req->best_parent_hw = clk_hw_get_parent(hw);
		if (req->best_parent_hw != NULL) {
			req->best_parent_rate =
				clk_hw_get_rate(req->best_parent_hw);
		}
	}

	return ret;
}

static const struct clk_ops tc_pll_ops = {
	.recalc_rate	= tc_pll_recalc_rate,
	.enable		= tc_pll_enable,
	.disable	= tc_pll_disable,
	.is_enabled	= tc_pll_is_enabled,
	.set_rate	= tc_pll_set_rate,
	.determine_rate	= tc_pll_determine_rate,
};

int tc_pll_fixed_enable(struct clk_hw *hw)
{
	(void)hw;
	return 0;
}

void tc_pll_fixed_disable(struct clk_hw *hw)
{
	(void) hw;
	return;
}

static const struct clk_ops tc_fixed_pll_ops = {
	.recalc_rate	= tc_pll_recalc_rate,
	.enable		= tc_pll_fixed_enable,
	.disable	= tc_pll_fixed_disable,
	.is_enabled	= tc_pll_is_enabled,
};


static const struct clk_ops *tc_find_pll_ops(struct device const *dev)
{
	const struct clk_ops	*ops = NULL;
	struct device_node const*np = dev->of_node;

	if (of_property_read_bool(np, "type-fixed")) {
		ops = &tc_fixed_pll_ops;
	} else {
		ops = &tc_pll_ops;
	}

	return ops;
}

static struct tc_pll *tc_do_pll_register(struct device *dev,
		struct clk_init_data *init, uint32_t index)
{
	struct device_node const*np = dev->of_node;
	struct tc_pll		*clk_pll;
	struct tc_clk		*clk_tc;
	struct clk_hw		*hw;

	uint32_t flags = 0;
	int ret = 0;

	clk_pll = devm_kzalloc(dev, sizeof(struct tc_pll), GFP_KERNEL);

	if (clk_pll != NULL) {
		clk_tc = &clk_pll->clk_tc;
		(void)of_property_read_u32_index(np, "tc-flags", index, &flags);
		clk_pll->flags = flags;

		ret = tc_clk_prepare_clk_data(dev, init, clk_tc, index);
	} else {
		ret = -ENOMEM;
	}

	if (ret == 0) {
		init->ops = tc_find_pll_ops(dev);
		if (init->ops == NULL) {
			ret = -EINVAL;
		}
	}

	if (ret == 0) {
		clk_tc->hw.init = init;
		clk_tc->dev = dev;

		hw = &clk_tc->hw;
		ret = devm_clk_hw_register(dev, hw);
	}

	if ((ret != 0) && (clk_pll != NULL)) {
		devm_kfree(dev, clk_pll);
		clk_pll = NULL;
	}

	if (ret == 0) {
		tc_clk_register_clkdev(dev, hw);
	}

	return clk_pll;
}

int tc_pll_register(struct platform_device *pdev)
{
	struct clk_hw_onecell_data 	*onecell_data= NULL;
	struct clk_init_data		init = {0};
	struct tc_pll			*pll_tc;
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

	onecell_data = tc_prepare_onecell_data(dev, &num_clks);
	if ((num_clks != 0) && (onecell_data == NULL)) {
		ret = -ENOMEM;
	}

	if ((ret == 0) && (num_clks > 0)) {
		unum_clks = (uint32_t)num_clks;
		/* [DR]
		 * The num_parents value is restricted under 255 by tcc_get_parent_info().
		 */
		init.num_parents = (u8)num_parents;
		init.parent_names = parent_names;

		for (index = 0U; index < unum_clks; index++) {
			pll_tc = tc_do_pll_register(dev, &init, index);

			if (pll_tc != NULL) {
				onecell_data->hws[index] = &pll_tc->clk_tc.hw;
				list_add_tail(&(pll_tc->clk_tc.list),
						&tc_pll_list);
			}
		}

		ret = devm_of_clk_add_hw_provider(dev, tc_onecell_get,
				onecell_data);
	} else {
		/* Do nothing */
	}

	if (parent_names != NULL) {
		devm_kfree(dev, parent_names);
	}

	if (ret == 0) {
		dev_dbg(dev, "[%s] success\n", __func__);
	} else {
		dev_dbg(dev, "[%s] failed\n", __func__);
	}

	return ret;
}
