// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Telechips Inc.
 *
 * Telechips Fixed PLL clock implementation
 *
 * Fixed PLL cannot change it's clock after Kernel booting start.
 * These PLL are shared by Main / Sub Cluster.
 */

#include "gate.h"

LIST_HEAD(tc_gate_list);

static bool tc_clk_dbg_warn_gate = false;
module_param(tc_clk_dbg_warn_gate, bool, 0x1A4/* 0644 */);
MODULE_PARM_DESC(tc_clk_dbg_warn_gate,
		"Telechips Clock driver will print stack dump "
		"when gate clock operations are failed");

static int tc_gate_enable_chkret(struct tc_gate *tc_gate_clk,
		struct arm_smccc_res const *res)
{
	int ret = 0;

	if (res->a3 == CKC_OK) {

		tc_gate_clk->en = (uint32_t)CKC_ENABLE;
		dev_dbg(tc_gate_clk->clk_tc.dev, "[%s] %s: success\n",
				__func__,
				clk_hw_get_name(&tc_gate_clk->clk_tc.hw));
	} else {
		dev_err(tc_gate_clk->clk_tc.dev, "[%s] %s: failed. ret: %lu\n",
				__func__, clk_hw_get_name(&tc_gate_clk->clk_tc.hw),
				res->a3);
		WARN_ON(tc_clk_dbg_warn_on ||tc_clk_dbg_warn_gate);
		ret = -EINVAL;
	}

	return ret;
}

int tc_gate_enable(struct clk_hw *hw)
{
	struct tc_gate		*tc_gate_clk = to_tc_gate(hw);
	struct arm_smccc_res	res = {0};

	unsigned long sip_cmd = SIP_CLK_ENABLE_GATE;
	int ret = 0;

	arm_smccc_smc(sip_cmd, tc_gate_clk->clk_tc.id,
			0, 0, 0, 0, 0, 0, &res);

	ret = tc_gate_enable_chkret(tc_gate_clk, &res);

	return ret;
}

static int tc_gate_is_enabled_chkret(struct tc_gate *tc_gate_clk,
		struct arm_smccc_res const *res)
{
	int ret = 0;

	if (res->a3 == CKC_OK) {
		if (res->a0 == 1UL) {
			tc_gate_clk->en = (uint32_t)CKC_ENABLE;
			ret = 1;
		} else {
			tc_gate_clk->en = (uint32_t)CKC_DISABLE;
			ret = 0;
		}
		dev_dbg(tc_gate_clk->clk_tc.dev, "[%s] %s: success. status: %s\n",
				__func__,
				clk_hw_get_name(&tc_gate_clk->clk_tc.hw),
				(res->a0 == 1UL) ? "enabled" : "disabled");
	} else {
		dev_err(tc_gate_clk->clk_tc.dev, "[%s] %s: failed. ret: %lu\n",
				__func__, clk_hw_get_name(&tc_gate_clk->clk_tc.hw),
				res->a0);
		WARN_ON(tc_clk_dbg_warn_on ||tc_clk_dbg_warn_gate);
		ret = -EINVAL;
	}

	return ret;
}

int tc_gate_is_enabled(struct clk_hw *hw)
{
	struct tc_gate		*tc_gate_clk = to_tc_gate(hw);
	struct arm_smccc_res	res = {0};

	unsigned long sip_cmd = SIP_CLK_IS_GATE_ENABLED;
	int ret = 0;

	arm_smccc_smc(sip_cmd, tc_gate_clk->clk_tc.id,
			0, 0, 0, 0, 0, 0, &res);

	ret = tc_gate_is_enabled_chkret(tc_gate_clk, &res);

	return ret;
}

static void tc_gate_disable_chkret(struct tc_gate *tc_gate_clk,
		struct arm_smccc_res const *res)
{
	if (res->a3 == CKC_OK) {
		tc_gate_clk->en = (uint32_t)CKC_DISABLE;
		dev_dbg(tc_gate_clk->clk_tc.dev, "[%s] %s: success\n",
				__func__,
				clk_hw_get_name(&tc_gate_clk->clk_tc.hw));
	} else {
		dev_err(tc_gate_clk->clk_tc.dev, "[%s] %s: failed. ret: %lu\n",
				__func__, clk_hw_get_name(&tc_gate_clk->clk_tc.hw),
				res->a3);
		WARN_ON(tc_clk_dbg_warn_on ||tc_clk_dbg_warn_gate);
	}
}

void tc_gate_disable(struct clk_hw *hw)
{
	struct tc_gate		*tc_gate_clk = to_tc_gate(hw);
	struct arm_smccc_res	res = {0};

	unsigned long sip_cmd = SIP_CLK_DISABLE_GATE;

	arm_smccc_smc(sip_cmd, tc_gate_clk->clk_tc.id,
			0, 0, 0, 0, 0, 0, &res);

	tc_gate_disable_chkret(tc_gate_clk, &res);
}

static const struct clk_ops tc_gate_ops = {
	.enable		= tc_gate_enable,
	.disable	= tc_gate_disable,
	.is_enabled	= tc_gate_is_enabled,
};

static struct tc_gate *tc_do_gate_register(struct device *dev,
		struct clk_init_data *init, uint32_t index)
{
	struct device_node const*np = dev->of_node;
	struct tc_gate		*tc_gate_clk;
	struct tc_clk		*clk_tc;
	struct clk_hw		*hw;

	uint32_t flags = 0;
	int ret = 0;

	tc_gate_clk = devm_kzalloc(dev, sizeof(struct tc_gate), GFP_KERNEL);

	if (tc_gate_clk != NULL) {
		clk_tc = &tc_gate_clk->clk_tc;
		(void)of_property_read_u32_index(np, "tc-flags", index, &flags);
		tc_gate_clk->flags = flags;

		ret = tc_clk_prepare_clk_data(dev, init, clk_tc, index);
	} else {
		ret = -ENOMEM;
	}

	init->ops = &tc_gate_ops;

	if (ret == 0) {
		clk_tc->hw.init = init;
		clk_tc->dev = dev;

		hw = &clk_tc->hw;
		ret = devm_clk_hw_register(dev, hw);
	}

	if ((ret != 0) && (tc_gate_clk != NULL)) {
		devm_kfree(dev, tc_gate_clk);
		tc_gate_clk = NULL;
	}

	if (ret == 0) {
		tc_clk_register_clkdev(dev, hw);
	}

	return tc_gate_clk;
}

int tc_gate_register(struct platform_device *pdev)
{
	struct clk_hw_onecell_data 	*onecell_data= NULL;
	struct clk_init_data		init = {0};
	struct tc_gate			*gate_tc;
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
			gate_tc = tc_do_gate_register(dev, &init, index);

			if (gate_tc != NULL) {
				onecell_data->hws[index] = &gate_tc->clk_tc.hw;
				list_add_tail(&(gate_tc->clk_tc.list),
						&tc_gate_list);
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
