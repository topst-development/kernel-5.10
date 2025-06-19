// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Telechips Inc.
 *
 * Telechips Complex Clock implementation
 *
 */

#include <linux/string.h>

#include <soc/telechips/chipinfo.h>

#include "composite.h"


LIST_HEAD(tc_composite_list);

static bool tc_clk_dbg_warn_composite = false;
module_param(tc_clk_dbg_warn_composite, bool, 0x1A4/* 0644 */);
MODULE_PARM_DESC(tc_clk_dbg_warn_composite,
		"Telechips Clock driver will print stack dump "
		"when composite clock operations are failed");

static int tc_composite_enable_chkret(struct tc_composite *tc_composite_clk,
		struct arm_smccc_res const *res)
{
	int ret = 0;
	static uint32_t sysid = 0;
	static unsigned long dbg_suppress_id = 0;
	bool is_main;

	if ((res->a0 != SMC_UNK) && (res->a3 == CKC_OK)) {

		if (sysid == 0U) {
			sysid = get_system_identity();
			is_main = is_main_system(sysid);
			if (is_main) {
				dbg_suppress_id = UART_PERI_MAIN;
			} else {
				dbg_suppress_id = UART_PERI_SUB;
			}
		}

		tc_composite_clk->en = (uint32_t)CKC_ENABLE;

		/* Below if clause added for suppress uart clock debug message.
		 * Uart trigger too much debug print if below if clause is not
		 * exist. Remove it when you need to deebug uart clocks. */
		if (tc_composite_clk->clk_tc.id != dbg_suppress_id) {
			dev_dbg(tc_composite_clk->clk_tc.dev, "[%s] (%s) success\n",
					clk_hw_get_name(&tc_composite_clk->clk_tc.hw),
					__func__);
		}

	} else {

		dev_err(tc_composite_clk->clk_tc.dev, "[%s] (%s) failed. ret: 0x%lx\n",
				clk_hw_get_name(&tc_composite_clk->clk_tc.hw),
				__func__, (res->a0 == SMC_UNK) ? res->a0 : res->a3);

		WARN_ON(tc_clk_dbg_warn_on || tc_clk_dbg_warn_composite);

		ret = -EINVAL;
	}

	return ret;
}

int tc_composite_enable(struct clk_hw *hw)
{
	struct tc_composite	*tc_composite_clk = to_tc_composite(hw);
	struct arm_smccc_res	res = {0};

	unsigned long sip_cmd = 0;
	int ret = 0;

	if (tc_composite_clk->clk_type == TC_CLK_TYPE_BUS) {

		sip_cmd = SIP_CLK_ENABLE_CLKCTRL;

	} else if (tc_composite_clk->clk_type == TC_CLK_TYPE_PERI) {

		sip_cmd = SIP_CLK_ENABLE_PCLKCTRL;

	} else { /* Do nothing */ }

	arm_smccc_smc(sip_cmd, tc_composite_clk->clk_tc.id, 0, 0, 0, 0, 0, 0, &res);

	ret = tc_composite_enable_chkret(tc_composite_clk, &res);

	return ret;
}

static int tc_composite_is_enabled_chkret(struct tc_composite *tc_composite_clk,
		struct arm_smccc_res const *res)
{
	int ret = 0;

	if ((res->a0 != SMC_UNK) && (res->a3 == CKC_OK)) {
		if (res->a0 == 1UL) {
			ret = 1;
			tc_composite_clk->en = (uint32_t)CKC_ENABLE;
		} else {
			ret = 0;
			tc_composite_clk->en = (uint32_t)CKC_DISABLE;
		}

		dev_dbg(tc_composite_clk->clk_tc.dev, "[%s] (%s) success. status: %s\n",
				clk_hw_get_name(&tc_composite_clk->clk_tc.hw),
				__func__,
				(res->a0 == 1UL) ? "enabled" : "disabled");

	} else {

		dev_err(tc_composite_clk->clk_tc.dev, "[%s] (%s) failed. ret: 0x%lx\n",
				clk_hw_get_name(&tc_composite_clk->clk_tc.hw),
				__func__, (res->a0 == SMC_UNK) ? res->a0 : res->a3);

		WARN_ON(tc_clk_dbg_warn_on || tc_clk_dbg_warn_composite);

		ret = -EINVAL;
	}

	return ret;
}

int tc_composite_is_enabled(struct clk_hw *hw)
{
	struct tc_composite	*tc_composite_clk = to_tc_composite(hw);
	struct arm_smccc_res	res = {0};

	unsigned long sip_cmd = 0;
	int ret = 0;

	if (tc_composite_clk->clk_type == TC_CLK_TYPE_BUS) {

		sip_cmd = SIP_CLK_IS_CLKCTRL_ENABLED;

	} else if (tc_composite_clk->clk_type == TC_CLK_TYPE_PERI) {

		sip_cmd = SIP_CLK_IS_PCLKCTRL_ENABLED;

	} else { /* Do nothing */ }

	arm_smccc_smc(sip_cmd, tc_composite_clk->clk_tc.id, 0, 0, 0, 0, 0, 0, &res);

	ret = tc_composite_is_enabled_chkret(tc_composite_clk, &res);

	return ret;
}

static void tc_composite_disable_chkret(struct tc_composite *tc_composite_clk,
		struct arm_smccc_res const *res)
{
	static uint32_t sysid = 0;
	static unsigned long dbg_suppress_id = 0;
	bool is_main;

	if ((res->a0 != SMC_UNK) && (res->a3 == CKC_OK)) {

		if (sysid == 0U) {
			sysid = get_system_identity();
			is_main = is_main_system(sysid);
			if (is_main) {
				dbg_suppress_id = UART_PERI_MAIN;
			} else {
				dbg_suppress_id = UART_PERI_SUB;
			}
		}

		tc_composite_clk->en = (uint32_t)CKC_DISABLE;

		if (tc_composite_clk->clk_tc.id != dbg_suppress_id) {
			dev_dbg(tc_composite_clk->clk_tc.dev, "[%s] (%s) success\n",
					clk_hw_get_name(&tc_composite_clk->clk_tc.hw),
					__func__);
		}

	} else {

		dev_err(tc_composite_clk->clk_tc.dev, "[%s] (%s) failed. ret: 0x%lx\n",
				clk_hw_get_name(&tc_composite_clk->clk_tc.hw),
				__func__, (res->a0 == SMC_UNK) ? res->a0 : res->a3);

		WARN_ON(tc_clk_dbg_warn_on || tc_clk_dbg_warn_composite);
	}
}

void tc_composite_disable(struct clk_hw *hw)
{
	struct tc_composite	*tc_composite_clk = to_tc_composite(hw);
	struct arm_smccc_res	res = {0};

	unsigned long sip_cmd = 0;

	if (tc_composite_clk->clk_type == TC_CLK_TYPE_BUS) {

		sip_cmd = SIP_CLK_DISABLE_CLKCTRL;

	} else if (tc_composite_clk->clk_type == TC_CLK_TYPE_PERI) {

		sip_cmd = SIP_CLK_DISABLE_PCLKCTRL;

	} else { /* Do nothing */ }

	arm_smccc_smc(sip_cmd, tc_composite_clk->clk_tc.id, 0, 0, 0, 0, 0, 0, &res);

	tc_composite_disable_chkret(tc_composite_clk, &res);
}

static void tc_composite_recalc_rate_chkret(struct tc_composite *tc_composite_clk,
		struct arm_smccc_res const *res)
{
	if ((res->a0 != SMC_UNK) && (res->a3 == CKC_OK)) {

		tc_composite_clk->rate    = res->a0;
		tc_composite_clk->sel     = (res->a1 > UINT_MAX) ?
			UINT_MAX : (uint32_t)res->a1;
		tc_composite_clk->divider = (res->a2 > UINT_MAX) ?
			UINT_MAX : (uint32_t)res->a2;

		if (tc_composite_clk->rate == 0UL) {
			tc_composite_clk->en = (uint32_t)CKC_DISABLE;
		}

		dev_dbg(tc_composite_clk->clk_tc.dev, "[%s] (%s) success rate: %llu sel: %u divider: %u\n",
				clk_hw_get_name(&tc_composite_clk->clk_tc.hw),
				__func__,
				tc_composite_clk->rate,
				tc_composite_clk->sel,
				tc_composite_clk->divider);
	} else {

		dev_err(tc_composite_clk->clk_tc.dev, "[%s] (%s) failed. ret: 0x%lx\n",
				clk_hw_get_name(&tc_composite_clk->clk_tc.hw),
				__func__, (res->a0 == SMC_UNK) ? res->a0 : res->a3);

		WARN_ON(tc_clk_dbg_warn_on || tc_clk_dbg_warn_composite);
	}
}

unsigned long tc_composite_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct tc_composite	*tc_composite_clk = to_tc_composite(hw);
	struct arm_smccc_res	res = {0};

	unsigned long sip_cmd = 0;

	(void)parent_rate;

	if (tc_composite_clk->clk_type == TC_CLK_TYPE_BUS) {

		sip_cmd = SIP_CLK_GET_CLKCTRL;

	} else if (tc_composite_clk->clk_type == TC_CLK_TYPE_PERI) {

		sip_cmd = SIP_CLK_GET_PCLKCTRL;

	} else { /* Do nothing */ }

	arm_smccc_smc(sip_cmd, tc_composite_clk->clk_tc.id, 0, 0, 0, 0, 0, 0, &res);

	tc_composite_recalc_rate_chkret(tc_composite_clk, &res);


	return tc_composite_clk->rate;
}

static int tc_composite_set_rate_chkret(struct tc_composite *tc_composite_clk,
		struct arm_smccc_res const *res)
{
	int ret = 0;

	if ((res->a0 != SMC_UNK) && (res->a3 == CKC_OK)) {

		tc_composite_clk->rate    = res->a0;
		tc_composite_clk->sel     = (res->a1 > UINT_MAX) ?
			UINT_MAX : (uint32_t)res->a1;
		tc_composite_clk->divider = (res->a2 > UINT_MAX) ?
			UINT_MAX : (uint32_t)res->a2;

		dev_dbg(tc_composite_clk->clk_tc.dev, "[%s] (%s) success rate: %llu sel: %u divider: %u\n",
				clk_hw_get_name(&tc_composite_clk->clk_tc.hw),
				__func__,
				tc_composite_clk->rate,
				tc_composite_clk->sel,
				tc_composite_clk->divider);
	} else {
		dev_err(tc_composite_clk->clk_tc.dev, "[%s] (%s) failed. ret: 0x%lx\n",
				clk_hw_get_name(&tc_composite_clk->clk_tc.hw),
				__func__, (res->a0 == SMC_UNK) ? res->a0 : res->a3);

		WARN_ON(tc_clk_dbg_warn_on || tc_clk_dbg_warn_composite);

		ret = -EINVAL;
	}

	return ret;
}

int tc_composite_set_rate(struct clk_hw *hw,
		unsigned long rate, unsigned long parent_rate)
{
	struct tc_composite	*tc_composite_clk = to_tc_composite(hw);
	struct arm_smccc_res	res = {0};

	unsigned long sip_cmd = 0;
	int ret = 0;

	(void)parent_rate;

	if (tc_composite_clk->clk_type == TC_CLK_TYPE_BUS) {

		sip_cmd = SIP_CLK_SET_CLKCTRL;

	} else if (tc_composite_clk->clk_type == TC_CLK_TYPE_PERI) {

		sip_cmd = SIP_CLK_SET_PCLKCTRL;

	} else { /* Do nothing */ }

	arm_smccc_smc(sip_cmd, tc_composite_clk->clk_tc.id, CKC_ENABLE, rate,
			tc_composite_clk->flags, 0, 0, 0, &res);

	ret = tc_composite_set_rate_chkret(tc_composite_clk, &res);

	return ret;
}

u8 tc_composite_get_parent(struct clk_hw *hw)
{
	struct tc_composite const	*tc_composite_clk = to_tc_composite(hw);

	(void)tc_composite_recalc_rate(hw, 0UL);

	dev_dbg(tc_composite_clk->clk_tc.dev, "[%s] (%s) sel: %u\n",
			clk_hw_get_name(&tc_composite_clk->clk_tc.hw),
			__func__, tc_composite_clk->sel);

	return (tc_composite_clk->sel > 255U) ? 255U : (uint8_t)tc_composite_clk->sel;
}

static int tc_composite_set_parent_chkret(struct tc_composite *tc_composite_clk,
		struct arm_smccc_res const *res)
{
	int ret = 0;

	if ((res->a0 != SMC_UNK) && (res->a3 == CKC_OK)) {
		tc_composite_clk->rate    = res->a0;
		tc_composite_clk->sel     = (res->a1 > UINT_MAX) ?
			UINT_MAX : (uint32_t)res->a1;
		tc_composite_clk->divider = (res->a2 > UINT_MAX) ?
			UINT_MAX : (uint32_t)res->a2;

		dev_dbg(tc_composite_clk->clk_tc.dev, "[%s] (%s) success rate: %llu sel: %u divider: %u\n",
				clk_hw_get_name(&tc_composite_clk->clk_tc.hw),
				__func__,
				tc_composite_clk->rate,
				tc_composite_clk->sel,
				tc_composite_clk->divider);

	} else {
		dev_err(tc_composite_clk->clk_tc.dev, "[%s] (%s) failed. ret: 0x%lx\n",
				clk_hw_get_name(&tc_composite_clk->clk_tc.hw),
				__func__, (res->a0 == SMC_UNK) ? res->a0 : res->a3);
			WARN_ON(tc_clk_dbg_warn_on || tc_clk_dbg_warn_composite);
			ret = -EINVAL;
	}

	return ret;
}

static int tc_composite_do_set_parent(struct tc_composite *tc_composite_clk,
		struct of_phandle_args const *clkspec)
{
	struct arm_smccc_res res = {0};
	unsigned long sip_cmd = 0, flags;
	int ret = 0;

	flags = TC_CLK_F_SRC(clkspec->args[1]);
	tc_composite_clk->flags = (uint32_t)(flags & 0xFFFFFFFFUL);

	if (tc_composite_clk->clk_type == TC_CLK_TYPE_BUS) {

		sip_cmd = SIP_CLK_SET_CLKCTRL;

	} else if (tc_composite_clk->clk_type == TC_CLK_TYPE_PERI) {

		sip_cmd = SIP_CLK_SET_PCLKCTRL;

	} else { /* Do nothing */ }

	arm_smccc_smc(sip_cmd,
			tc_composite_clk->clk_tc.id,
			tc_composite_clk->en,
			tc_composite_clk->rate,
			tc_composite_clk->flags,
			0, 0, 0, &res);

	ret = tc_composite_set_parent_chkret(tc_composite_clk, &res);

	return ret;
}

int tc_composite_set_parent(struct clk_hw *hw, u8 index)
{
	struct tc_composite	*tc_composite_clk = to_tc_composite(hw);
	struct of_phandle_args	clkspec;
	struct device_node const*np;
	uint32_t num_parent;

	int ret = 0;

	np = tc_composite_clk->clk_tc.dev->of_node;
	num_parent = clk_hw_get_num_parents(hw);

	ret = of_parse_phandle_with_args(np, "clocks", "#clock-cells", (int)index,
			&clkspec);

	if (ret == 0) {
		ret = tc_composite_do_set_parent(tc_composite_clk, &clkspec);

	} else {
		dev_err(tc_composite_clk->clk_tc.dev, "[%s] (%s): failed to get parent info. ret: %d index: %u\n",
				clk_hw_get_name(&tc_composite_clk->clk_tc.hw),
				__func__, ret, index);
		WARN_ON(tc_clk_dbg_warn_on || tc_clk_dbg_warn_composite);
	}

	return ret;
}

int tc_composite_set_rate_and_parent(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate, u8 index)
{
	int ret = 0;

	(void)index;

	ret = tc_composite_set_rate(hw, rate, parent_rate);

	return ret;
}

static int tc_composite_determine_rate_chkret(struct tc_composite const *tc_composite_clk,
		struct arm_smccc_res const *res)
{
	int ret = 0;

	if ((res->a0 != SMC_UNK) && (res->a3 == CKC_OK)) {
		dev_dbg(tc_composite_clk->clk_tc.dev, "[%s] (%s): success rate: %lu sel: %lu divider: %lu\n",
				clk_hw_get_name(&tc_composite_clk->clk_tc.hw),
				__func__, res->a0, res->a1, res->a2);
	} else {
		dev_err(tc_composite_clk->clk_tc.dev, "[%s] (%s) failed. ret: 0x%lx\n",
				clk_hw_get_name(&tc_composite_clk->clk_tc.hw),
				__func__, (res->a0 == SMC_UNK) ? res->a0 : res->a3);

		WARN_ON(tc_clk_dbg_warn_on || tc_clk_dbg_warn_composite);

		ret = -EINVAL;
	}

	return ret;
}

int tc_composite_determine_rate(struct clk_hw *hw, struct clk_rate_request *req)
{
	struct tc_composite const	*tc_composite_clk = to_tc_composite(hw);
	struct arm_smccc_res	res = {0};

	unsigned long sip_cmd = 0;
	uint32_t index;
	int ret = 0;

	if (tc_composite_clk->clk_type == TC_CLK_TYPE_BUS) {

		sip_cmd = SIP_CLK_DETERMINE_CLKCTRL;

	} else if (tc_composite_clk->clk_type == TC_CLK_TYPE_PERI) {

		sip_cmd = SIP_CLK_DETERMINE_PCLKCTRL;

	} else {
		ret = -EINVAL;
	}

	if (ret == 0) {
		arm_smccc_smc(sip_cmd, tc_composite_clk->clk_tc.id, req->rate,
				tc_composite_clk->flags, 0, 0, 0, 0, &res);

		ret = tc_composite_determine_rate_chkret(tc_composite_clk,  &res);
	}

	if (ret == 0) {
		req->rate = res.a0;
		index = (res.a1 >= UINT_MAX) ? UINT_MAX : (uint32_t)res.a1;
		req->best_parent_hw = clk_hw_get_parent_by_index(hw, index);
		/* XXX
		 * If CLK_SET_RATE_PARENT is set, CCF try to change parent clock
		 * Set beset_parent_rate same with best_parent_hw
		 */
		//req->best_parent_rate = clk_hw_get_rate(req->best_parent_hw);
	}

	return ret;
}

static const struct clk_ops tc_fixed_composite_ops = {
	.recalc_rate	= tc_composite_recalc_rate,
	.is_enabled	= tc_composite_is_enabled,
	.get_parent	= tc_composite_get_parent,
};

static const struct clk_ops tc_composite_ops = {
	.recalc_rate	= tc_composite_recalc_rate,
	.determine_rate	= tc_composite_determine_rate,
	.set_rate	= tc_composite_set_rate,
	.is_enabled	= tc_composite_is_enabled,
	.enable		= tc_composite_enable,
	.disable	= tc_composite_disable,
	.set_parent	= tc_composite_set_parent,
	.get_parent	= tc_composite_get_parent,
	.set_rate_and_parent = tc_composite_set_rate_and_parent,
};

static const struct clk_ops *tc_find_composite_ops(struct device const *dev)
{
	const struct clk_ops	*ops = NULL;
	struct device_node const*np = dev->of_node;

	if (of_property_read_bool(np, "type-fixed")) {
		ops = &tc_fixed_composite_ops;
	} else {
		ops = &tc_composite_ops;
	}

	return ops;
}

static struct tc_composite *tc_do_composite_register(struct device *dev,
		struct clk_init_data *init, uint32_t index, uint32_t composite_type)
{
	struct device_node const*np = dev->of_node;
	struct tc_composite	*tc_composite_clk;
	struct tc_clk		*clk_tc;
	struct clk_hw		*hw;

	uint32_t flags = 0;
	int ret = 0;

	tc_composite_clk = (struct tc_composite *)devm_kzalloc(dev,
			sizeof(struct tc_composite), GFP_KERNEL);

	if (tc_composite_clk != NULL) {
		clk_tc = &tc_composite_clk->clk_tc;
		(void)of_property_read_u32_index(np, "tc-flags", index, &flags);
		tc_composite_clk->flags = flags;
		tc_composite_clk->clk_type = composite_type;
		tc_composite_clk->en = (uint32_t)CKC_ENABLE;
		ret = tc_clk_prepare_clk_data(dev, init, clk_tc, index);
	} else {
		ret = -ENOMEM;
	}

	if (ret == 0) {
		init->ops = tc_find_composite_ops(dev);
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

	if ((ret != 0) && (tc_composite_clk != NULL)) {
		devm_kfree(dev, tc_composite_clk);
		tc_composite_clk = NULL;
	}

	if (ret == 0) {
		tc_clk_register_clkdev(dev, hw);
	}

	return tc_composite_clk;
}

int tc_composite_register(struct platform_device *pdev, uint32_t composite_type)
{
	struct clk_hw_onecell_data 	*onecell_data= NULL;
	struct clk_init_data		init = {0};
	struct tc_composite		*composite_tc;
	struct device			*dev = &pdev->dev;

	const char	**parent_names;
	uint32_t 	index, num_parents, unum_clks;

	int num_clks, ret = 0;

	parent_names = tc_get_parent_info(dev, &num_parents);
	if ((num_parents != 0U) && (parent_names == NULL)) {
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
			composite_tc = tc_do_composite_register(dev, &init, index,
					composite_type);

			if (composite_tc != NULL) {
				onecell_data->hws[index] = &composite_tc->clk_tc.hw;
				list_add_tail(&(composite_tc->clk_tc.list),
						&tc_composite_list);
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
