// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/clkdev.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/arm-smccc.h>
#include <linux/clk/tcc_clk.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/syscore_ops.h>
#include <soc/telechips/smc.h>
#include <dt-bindings/clock/telechips,clk-common.h>

#define TCC_CLK_DEBUG 0

LIST_HEAD(pll_clk_list);
LIST_HEAD(bus_clk_list);
LIST_HEAD(peri_clk_list);
LIST_HEAD(gate_clk_list);

#define IGNORE_CLK_DISABLE

/* [DR] - MISRA C-2012 Rule 8.13
 * tcc_onecell_get is call back of struct of_clk_provicer.get
 * function is declare as
 * ‘struct clk *(*get)(struct of_phandle_args *clkspec, void *data)’
 */
static struct clk_hw *tcc_onecell_get(struct of_phandle_args *clkspec, void *data)
{
	/* [DR] - MISRA C-2012 Rule 11.5
	 * The void pointer type must convert to struct clk_onecell_data
	 * pointer type to access data properly.
	 * This data is registered by devm_of_clk_add_hw_provider() and return
	 * when provider->get_hw callback function is called by CCF.
	 */
	const struct clk_hw_onecell_data *onecell_data =
		(const struct clk_hw_onecell_data *)data;
	u32 idx = clkspec->args[0];
	u32 i;
	struct clk_hw *hw = NULL;
	const struct tcc_clk *clk_tcc;

	for (i = 0; i < onecell_data->num; i++) {
		hw = onecell_data->hws[i];
		clk_tcc = to_tcc_clk(hw);

		if (idx == clk_tcc->id) {
			break;
		}
	}

	if (i == onecell_data->num) {
		hw = ERR_PTR(-ENODEV);
	}

	return hw;
}

static struct clk_hw_onecell_data *tcc_prepare_onecell_data(struct device *dev,
		int *num_clks)
{
	struct clk_hw_onecell_data	*onecell_data = NULL;
	const struct device_node	*np = dev->of_node;

	uint32_t unum_clks;

	*num_clks = of_property_count_strings(np, "clock-output-names");

	if (*num_clks > 0) {
		unum_clks = (uint32_t)(*num_clks);
		/* [DR] - MISRA C-2012 Rule 11.5
		 * The void pointer type that returned from devm_kzalloc() must
		 * be converted to struct clk_hw_onecell_data pointer to access
		 * properly.
		 */
		onecell_data =
			devm_kzalloc(dev, sizeof(struct clk_hw_onecell_data) +
				(unum_clks * sizeof(struct clk_hw *)),
				GFP_KERNEL);

		if (onecell_data != NULL) {
			onecell_data->num = unum_clks;
		}
	}

	return onecell_data;
}

static const char **tcc_get_parent_info(struct device *dev, uint32_t *num_parents)
{
	struct device_node *np = dev->of_node;
	const char **parent_names;

	*num_parents = of_clk_get_parent_count(np);

	if (*num_parents > 255U) {
		*num_parents = 255U;
	}

	if (*num_parents > 0U) {
		/* [DR] - MISRA C-2012 Rule 11.5
		 * The void pointer type that returned from devm_kcalloc() must
		 * be converted to char pointer to access properly.
		 */
		parent_names = devm_kcalloc(dev, *num_parents, sizeof(char *),
				GFP_KERNEL);
	} else {
		parent_names = NULL;
	}

	if (parent_names != NULL) {
		(void)of_clk_parent_fill(np, parent_names, *num_parents);
	}

	return parent_names;
}

static int tcc_get_source_index_info(struct device *dev,
		uint32_t **clk_source_idx, int *num_source)
{
	const struct device_node *np = dev->of_node;
	int ret = 0;

	*num_source = of_property_count_elems_of_size(np, "clk-source-idx",
			(int)sizeof(uint32_t));

	/* Root clocks like osc24m, osc32k does not have source clock
	 * Do not handle as error when source clocks are not exist. */
	if (*num_source > 0) {
		/* [DR] - MISRA C-2012 Rule 11.5
		 * The void pointer type that returned from devm_kcalloc() must
		 * be converted to uint32_t pointer to access properly.
		 */
		*clk_source_idx = devm_kcalloc(dev, sizeof(uint32_t),
				(size_t)(*num_source), GFP_KERNEL);

		if (*clk_source_idx != NULL) {
			ret = of_property_read_variable_u32_array(np,
					"clk-source-idx", *clk_source_idx,
					1, (size_t)(*num_source));

			if ((ret <= 0) && (*clk_source_idx != NULL)) {
				devm_kfree(dev, *clk_source_idx);
				*clk_source_idx = NULL;
			}
			ret = 0;
		} else {
			ret = -ENOMEM;
		}
	}

	return ret;
}

static struct tcc_clk* tcc_clk_prepare_clk_data(struct device *dev,
				struct clk_init_data *init, uint32_t index)
{
	const struct device_node	*np = dev->of_node;
	struct tcc_clk		*clk_tcc = NULL;

	const char	*name = NULL;
	uint32_t	flags = 0;
	int		sindex;
	int		ret = 0;

	init->name = NULL;

	if (index > 0x7FFFFFFFU /* INT_MAX */) {
		index = 0x7FFFFFFFU;
	}

	sindex = (int)index;

	(void)of_property_read_u32_index(np, "telechips,clock-flags", index,
			&flags);
	init->flags = (flags | CLK_GET_RATE_NOCACHE);

	ret = of_property_read_string_index(np, "clock-output-names", sindex,
			&name);

	if (ret == 0) {
		init->name = name;
		/* [DR] - MISRA C-2012 Rule 11.5
		 * The void pointer type that returned from devm_kzalloc() must
		 * be converted to struct tcc_clk pointer to access properly.
		 */
		clk_tcc = devm_kzalloc(dev, sizeof(struct tcc_clk), GFP_KERNEL);
	}

	if (clk_tcc != NULL) {
		ret = of_property_read_u32_index(np, "clock-indices", index,
				&(clk_tcc->id));
		if (ret != 0) {
			clk_tcc->id = UINT_MAX;
		}
	}

	return clk_tcc;
}

static int tcc_clk_do_register_clkdev(struct device *dev, struct clk_hw *hw)
{
	int ret = 0;
	ret = devm_clk_hw_register_clkdev(dev, hw, clk_hw_get_name(hw),
			dev_name(dev));
	if (ret != 0) {
		(void)pr_err("[ERROR][tcc_clk][%s] failed to register clkdev '%s' (%d)\n",
				__func__, clk_hw_get_name(hw), ret);
	}

	return ret;
}

static struct tcc_clk *tcc_clk_do_register(struct device *dev, struct clk_init_data *init,
		uint32_t index, uint32_t *source_idx, int num_source)
{
	struct tcc_clk *clk_tcc;
	struct clk_hw *hw;
	uint8_t src_length;
	int ret = 0;

	if ((num_source > 0) && (num_source < 256)) {
		src_length = (uint8_t)num_source;
	}

	clk_tcc = tcc_clk_prepare_clk_data(dev, init, index);
	if (clk_tcc == NULL) {
		ret = -ENOMEM;
	}

	if (ret == 0) {
		clk_tcc->hw.init = init;
		clk_tcc->ops = init->ops;
		clk_tcc->clk_source_idx = source_idx;
		clk_tcc->clk_src_length = src_length;

		hw = &clk_tcc->hw;
		ret = devm_clk_hw_register(dev, hw);
	}

	if (ret == 0) {
		ret = tcc_clk_do_register_clkdev(dev, hw);
		if (ret != 0) {
			/* TODO:
			 * Change to devm_clk_hw_unregister(dev, hw) after add
			 * it as a GKI kernel symbol.
			 */
			clk_hw_unregister(hw);
		}
	}

	if ((ret != 0) && (clk_tcc != NULL)) {
		devm_kfree(dev, clk_tcc);
		clk_tcc = NULL;
	}

	return clk_tcc;
}

static int tcc_clk_register(struct platform_device *pdev, const struct clk_ops *ops,
				struct list_head *clk_list_head)
{
	struct clk_hw_onecell_data	*onecell_data= NULL;
	struct clk_init_data		init = {0};
	struct tcc_clk			*clk_tcc;
	struct device			*dev = &pdev->dev;

	const char	**parent_names;
	uint32_t 	*source_idx, index, num_parents, unum_clks;

	int num_clks, num_source, ret = 0;

	parent_names = tcc_get_parent_info(dev, &num_parents);
	if ((num_parents != 0U) && !(parent_names != NULL)) {
		/* If parents exist, parent_names should not be 0,
		 * May be failed to allocate memory. */
		ret = -ENOMEM;
	}

	if (ret == 0) {
		ret = tcc_get_source_index_info(dev, &source_idx, &num_source);
	}

	if (ret == 0) {
		onecell_data = tcc_prepare_onecell_data(dev, &num_clks);
	}

	if (onecell_data == NULL) {
		ret = -ENOMEM;
	} else if (num_clks >= 0) {
		unum_clks = (uint32_t)num_clks;
		init.ops = ops;
		/* [DR]
		 * The num_parents value is restricted under 255 by tcc_get_parent_info().
		 */
		init.num_parents = (u8)num_parents;
		init.parent_names = parent_names;

		for (index = 0U; index < unum_clks; index++) {

			clk_tcc = tcc_clk_do_register(dev, &init, index, source_idx,
					num_source);

			if (clk_tcc != NULL) {
				onecell_data->hws[index] = &clk_tcc->hw;
				list_add_tail(&(clk_tcc->list), clk_list_head);
#if TCC_CLK_DEBUG
				(void)pr_err("[DEBUG][tcc_clk][%s] %pOFfp: '%s' registered (index=%d)\n",
						__func__, np, init.name, index);
#endif
			}
		}

		ret = devm_of_clk_add_hw_provider(dev, tcc_onecell_get, onecell_data);
	} else {
		/* Do nothing */
	}

	if (parent_names != NULL) {
		devm_kfree(dev, parent_names);
	}

	return ret;
}

/* common function */
/* [DR] - MISRA C-2012 Rule 8.13
 * tcc_round_rate is call back of struct clk_ops.round_rate
 * function is declare as
 * ‘long (*round_rate)(struct clk_hw *hw, unsigned long rate,
 *		       unsigned long *parent_rate)’
 */
static long tcc_round_rate(struct clk_hw *hw, unsigned long rate,
			   unsigned long *best_parent_rate)
{
	/* return type is defined on open source prototype. (long) */
	long ret = (rate > (ULONG_MAX / 2UL)) ? __LONG_MAX__ : (long)rate;
	(void)hw;
	(void)best_parent_rate;

	return ret;
}

static s32 tcc_clkctrl_is_enabled(struct clk_hw *hw);

static s32 tcc_clkctrl_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 ret = 0;

	arm_smccc_smc(SIP_CLK_WDPR_ENABLE_CLKCTRL, tcc->id, 0,
		      0, 0, 0, 0, 0, &res);
#if TCC_CLK_DEBUG
	(void)pr_err("[DEBUG][tcc_clk][%s] result: %d\n",
			__func__, tcc_clkctrl_is_enabled(hw));
#endif

	return ret;
}

#ifndef IGNORE_CLK_DISABLE
static void tcc_clkctrl_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));

	arm_smccc_smc(SIP_CLK_WDPR_DISABLE_CLKCTRL, tcc->id,
		      0, 0, 0, 0, 0, 0, &res);
#if TCC_CLK_DEBUG
	(void)pr_err("[DEBUG][tcc_clk][%s] result: %d\n",
			__func__, tcc_clkctrl_is_enabled(hw));
#endif
}
#endif

static ulong tcc_clkctrl_recalc_rate(struct clk_hw *hw, ulong parent_rate)
{
	struct arm_smccc_res res;
	ulong rate = 0;
	const struct tcc_clk *tcc = to_tcc_clk((hw));

	(void)parent_rate;

	arm_smccc_smc(SIP_CLK_WDPR_GET_CLKCTRL, tcc->id, 0, 0,
		      0, 0, 0, 0, &res);
	rate = res.a0;

	return rate;
}

static int tcc_clkctrl_set_parent(struct clk_hw *hw, u8 index)
{
	const struct tcc_clk *tcc = to_tcc_clk((hw));
	struct arm_smccc_res res;
	unsigned long current_rate, vflags;

	arm_smccc_smc(SIP_CLK_WDPR_GET_CLKCTRL, tcc->id, 0, 0, 0, 0, 0, 0, &res);
	current_rate = res.a0;

	vflags = CLK_F_SRC_CLK(tcc->clk_source_idx[index]);
	arm_smccc_smc(SIP_CLK_WDPR_SET_CLKCTRL, tcc->id, 1UL,
			current_rate, vflags, 0, 0, 0, &res);
	return 0;
}

static u8 tcc_clkctrl_get_parent(struct clk_hw *hw)
{
	const struct tcc_clk *tcc = to_tcc_clk((hw));
	struct arm_smccc_res res;
	u8 idx = 255U;

	arm_smccc_smc(SIP_CLK_WDPR_GET_CLKCTRL, tcc->id, 0, 0, 0, 0, 0, 0, &res);

	if (tcc->clk_src_length >= res.a1) {
		if (tcc->clk_source_idx[res.a1] <= 255U) {
			idx = (u8)tcc->clk_source_idx[res.a1];
		}
	}

	return idx;
}

static s32 tcc_clkctrl_set_rate(struct clk_hw *hw, ulong rate, ulong parent_rate)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 ret = 0;

	(void)parent_rate;

	arm_smccc_smc(SIP_CLK_WDPR_SET_CLKCTRL, tcc->id, 1,
		      rate, 0, 0, 0, 0, &res);
	ret = (res.a0 == 0UL) ? 0 : -1;

#if TCC_CLK_DEBUG
	(void)pr_err("[DEBUG][tcc_clk][%s] ID: %u req_rate: %lu cur_rate: %lu\n",
		 __func__, tcc->id, rate, tcc_clkctrl_recalc_rate(hw, rate));
#endif

	return ret;
}

static s32 tcc_clkctrl_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 ret = 0;

	arm_smccc_smc(SIP_CLK_WDPR_IS_CLKCTRL, tcc->id, 0, 0,
		      0, 0, 0, 0, &res);
	ret = (res.a0 == 1UL) ? 1 : 0;

	return ret;
}

static const struct clk_ops tcc_clkctrl_ops = {
	.enable = tcc_clkctrl_enable,
#ifndef IGNORE_CLK_DISABLE
	.disable = tcc_clkctrl_disable,
#endif
	.is_enabled = tcc_clkctrl_is_enabled,
	.recalc_rate = tcc_clkctrl_recalc_rate,
	.round_rate = tcc_round_rate,
	.set_rate = tcc_clkctrl_set_rate,
	.set_parent = tcc_clkctrl_set_parent,
	.get_parent = tcc_clkctrl_get_parent,
};

static s32 tcc_peri_is_enabled(struct clk_hw *hw);

static s32 tcc_peri_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 ret = 0;


	arm_smccc_smc(SIP_CLK_WDPR_ENABLE_PERI, tcc->id, 0, 0,
		      0, 0, 0, 0, &res);
	/* Return Nothing */

	return ret;
}

static void tcc_peri_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));


	arm_smccc_smc(SIP_CLK_WDPR_DISABLE_PERI, tcc->id, 0,
		      0, 0, 0, 0, 0, &res);
}

static ulong tcc_peri_recalc_rate(struct clk_hw *hw, ulong parent_rate)
{
	struct arm_smccc_res res;
	ulong rate = 0;
	const struct tcc_clk *tcc = to_tcc_clk((hw));

	(void)parent_rate;

	arm_smccc_smc(SIP_CLK_WDPR_GET_PCLKCTRL, tcc->id, 0,
		      0, 0, 0, 0, 0, &res);
	rate = res.a0;

	return rate;
}

static int tcc_peri_set_parent(struct clk_hw *hw, u8 index)
{
	const struct tcc_clk *tcc = to_tcc_clk((hw));
	struct arm_smccc_res res;
	unsigned long current_rate, vflags;

	arm_smccc_smc(SIP_CLK_WDPR_GET_PCLKCTRL, tcc->id, 0, 0, 0, 0, 0, 0, &res);
	current_rate = res.a0;

	vflags = CLK_F_SRC_CLK(tcc->clk_source_idx[index]);
	arm_smccc_smc(SIP_CLK_WDPR_SET_PCLKCTRL, tcc->id, 1UL,
			current_rate, vflags, 0, 0, 0, &res);
	return 0;
}

static u8 tcc_peri_get_parent(struct clk_hw *hw)
{
	const struct tcc_clk *tcc = to_tcc_clk((hw));
	struct arm_smccc_res res;
	u8 idx = 255U;

	arm_smccc_smc(SIP_CLK_WDPR_GET_PCLKCTRL, tcc->id, 0, 0, 0, 0, 0, 0, &res);

	if (tcc->clk_src_length >= res.a1) {
		if (tcc->clk_source_idx[res.a1] <= 255U) {
			idx = (u8)tcc->clk_source_idx[res.a1];
		}
	}

	return idx;
}

#define PERI_SRC_EXT0_MASK (CLK_F_FIXED | 25UL)
#define PERI_SRC_EXT1_MASK (CLK_F_FIXED | 26UL)
static s32 tcc_peri_set_src(struct clk_hw *hw, ulong src, ulong divider);

static s32 tcc_peri_set_rate(struct clk_hw *hw, ulong rate, ulong parent_rate)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));
	ulong cflags = clk_hw_get_flags(hw);
	ulong vflags = 0;
	s32 ret = 0;

	(void)parent_rate;

	/* We care only about vendor-specific flags */
	if ((cflags & CLK_F_FIXED) != 0UL) {
		/* TODO: Divide vendor specific flags & CCF flags */
		vflags = ((cflags >> CLK_F_SRC_CLK_SHIFT) &
			 CLK_F_SRC_CLK_MASK);
		vflags |= CLK_F_FIXED;
	}
	if ((cflags & CLK_F_DCO_MODE) != 0UL) {
		vflags |= CLK_F_DCO_MODE;
	}
	if ((cflags & CLK_F_SKIP_SSCG) != 0UL) {
		vflags |= CLK_F_SKIP_SSCG;
	}
	if ((cflags & CLK_F_DIV_MODE) != 0UL) {
		vflags |= CLK_F_DIV_MODE;
	}

	if (((vflags & PERI_SRC_EXT0_MASK) == PERI_SRC_EXT0_MASK) ||
			((vflags & PERI_SRC_EXT1_MASK) == PERI_SRC_EXT1_MASK)) {
		ret = tcc_peri_set_src(hw, (vflags&CLK_F_SRC_CLK_MASK), 0);
	} else {
		arm_smccc_smc(SIP_CLK_WDPR_SET_PCLKCTRL, tcc->id, 1UL,
				rate, vflags, 0, 0, 0, &res);
		ret = (res.a0 == 0UL) ? 0 : -1;
	}

#if TCC_CLK_DEBUG
	(void)pr_err("[DEBUG][tcc_clk][%s] ID: %u req_rate: %lu cur_rate: %lu\n",
		 __func__, tcc->id, rate, tcc_peri_recalc_rate(hw, rate));
#endif

	return ret;
}

static s32 tcc_peri_set_src(struct clk_hw *hw, ulong src, ulong divider)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));
	ulong cflags = clk_hw_get_flags(hw);
	s32 ret = 0;

	arm_smccc_smc(SIP_CLK_WDPR_GET_PCLKCTRL, tcc->id, 0,
			0, 0, 0, 0, 0, &res);
	if (res.a1 != src) {
		if ((cflags & CLK_IS_PCLKDCO) == 0UL) {
			arm_smccc_smc(SIP_CLK_WDPR_SET_PCLKCTRL_DIV,	tcc->id,
					src, (divider > 0UL) ?
					(divider - 1UL) : 0UL,
					0, 0, 0, 0, &res);
		} else {
			arm_smccc_smc(SIP_CLK_WDPR_SET_PCLKCTRL_DCO,	tcc->id,
					src, (divider > 1UL) ?
					(divider - 1UL) : 1UL,
					0, 0, 0, 0, &res);

		}
		ret = (res.a0 == 0UL) ? 0 : -1;
	}

	return ret;
}

static s32 tcc_peri_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 ret = 0;

	arm_smccc_smc(SIP_CLK_WDPR_IS_PERI, tcc->id, 0, 0, 0,
		      0, 0, 0, &res);
	ret = (res.a0 == 1UL) ? 1 : 0;

	return ret;
}

#ifdef CONFIG_DEBUG_FS
static int debugfs_peri_clk_src_get(void *data, u64 *val)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk(data);

	arm_smccc_smc(SIP_CLK_WDPR_GET_PCLKCTRL, tcc->id, 0,
		      0, 0, 0, 0, 0, &res);
	*val = res.a1;

	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(tcc_peri_clk_src_fops, debugfs_peri_clk_src_get,
			 NULL, "%llu\n");

static int debugfs_peri_clk_div_get(void *data, u64 *val)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk(data);

	arm_smccc_smc(SIP_CLK_WDPR_GET_PCLKCTRL, tcc->id, 0,
		      0, 0, 0, 0, 0, &res);
	*val = res.a2;

	return 0;
}

/*
 * [DR]
 * kernel API DEFINE_DEBUGFS_ATTRIBUTE has defects it's inside.
 */
DEFINE_DEBUGFS_ATTRIBUTE(tcc_peri_clk_div_fops, debugfs_peri_clk_div_get,
			 NULL, "%llu\n");
#endif

static void tcc_peri_debug_init(struct clk_hw *hw, struct dentry *dt)
{
#ifdef CONFIG_DEBUG_FS
	struct tcc_clk *tcc = to_tcc_clk((hw));
	const char clk_src_str[] = "clk_src";
	const char clk_div_str[] = "clk_div";
	const struct dentry *ret_ptr;
	ushort dbg_fs_md = (ushort)0x124; //S_IRUSR | S_IRGRP | S_IROTH (0444)

	ret_ptr = debugfs_create_file(clk_src_str, dbg_fs_md, dt,
					tcc, &tcc_peri_clk_src_fops);

	if (ret_ptr == NULL) {
		(void)pr_err("[ERROR][tcc_clk][%s] clk_debugfs_add_file returned NULL\n",
			__func__);
	}

	ret_ptr = debugfs_create_file(clk_div_str, dbg_fs_md, dt,
					tcc, &tcc_peri_clk_div_fops);

	if (ret_ptr == NULL) {
		(void)pr_err("[ERROR][tcc_clk][%s] clk_debugfs_add_file returned NULL\n",
			__func__);
	}

#endif
}

static const struct clk_ops tcc_peri_ops = {
	.enable = tcc_peri_enable,
	.disable = tcc_peri_disable,
	.recalc_rate = tcc_peri_recalc_rate,
	.round_rate = tcc_round_rate,
	.set_rate = tcc_peri_set_rate,
	.set_parent = tcc_peri_set_parent,
	.get_parent = tcc_peri_get_parent,
	.is_enabled = tcc_peri_is_enabled,
	.debug_init = tcc_peri_debug_init,
};

static s32 tcc_ddibus_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 ret = 0;


	arm_smccc_smc(SIP_CLK_WDPR_ENABLE_DDIBUS, tcc->id, 0,
		      0, 0, 0, 0, 0, &res);
	/* Return Nothing */

	return ret;
}

static void tcc_ddibus_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));

	arm_smccc_smc(SIP_CLK_WDPR_DISABLE_DDIBUS, tcc->id, 0,
		      0, 0, 0, 0, 0, &res);
}

static s32 tcc_ddibus_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 ret = 0;


	arm_smccc_smc(SIP_CLK_WDPR_IS_DDIBUS, tcc->id, 0, 0,
		      0, 0, 0, 0, &res);

	ret = (res.a0 == 0UL) ? 1 : 0;

	return ret;
}

static const struct clk_ops tcc_ddibus_ops = {
	.enable = tcc_ddibus_enable,
	.disable = tcc_ddibus_disable,
	.is_enabled = tcc_ddibus_is_enabled,
};

static s32 tcc_iobus_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 ret = 0;

	arm_smccc_smc(SIP_CLK_WDPR_ENABLE_IOBUS, tcc->id, 0,
		      0, 0, 0, 0, 0, &res);
	/* Return Nothing */

	return ret;
}

static void tcc_iobus_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));

	arm_smccc_smc(SIP_CLK_WDPR_DISABLE_IOBUS, tcc->id, 0,
		      0, 0, 0, 0, 0, &res);
}

static s32 tcc_iobus_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 ret = 0;

	arm_smccc_smc(SIP_CLK_WDPR_IS_IOBUS, tcc->id, 0, 0, 0,
		      0, 0, 0, &res);

	ret = (res.a0 == 0UL) ? 1 : 0;

	return ret;
}

static const struct clk_ops tcc_iobus_ops = {
	.enable = tcc_iobus_enable,
	.disable = tcc_iobus_disable,
	.is_enabled = tcc_iobus_is_enabled,
};

static s32 tcc_vpubus_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 ret = 0;

	arm_smccc_smc(SIP_CLK_WDPR_ENABLE_VPUBUS, tcc->id, 0,
		      0, 0, 0, 0, 0, &res);
	/* Return Nothing */

	return ret;
}

static void tcc_vpubus_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));

	arm_smccc_smc(SIP_CLK_WDPR_DISABLE_VPUBUS, tcc->id, 0,
		      0, 0, 0, 0, 0, &res);
}

static s32 tcc_vpubus_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 ret = 0;

	arm_smccc_smc(SIP_CLK_WDPR_IS_VPUBUS, tcc->id, 0, 0,
		      0, 0, 0, 0, &res);

	ret = (res.a0 == 1UL) ? 1 : 0;

	return ret;
}

static const struct clk_ops tcc_vpubus_ops = {
	.enable = tcc_vpubus_enable,
	.disable = tcc_vpubus_disable,
	.is_enabled = tcc_vpubus_is_enabled,
};

static s32 tcc_hsiobus_enable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 ret = 0;

	arm_smccc_smc(SIP_CLK_WDPR_ENABLE_HSIOBUS, tcc->id, 0,
		      0, 0, 0, 0, 0, &res);
	/* Return Nothing */

	return ret;
}

static void tcc_hsiobus_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));

	arm_smccc_smc(SIP_CLK_WDPR_DISABLE_HSIOBUS, tcc->id,
		      0, 0, 0, 0, 0, 0, &res);
}

static s32 tcc_hsiobus_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 ret = 0;

	arm_smccc_smc(SIP_CLK_WDPR_IS_HSIOBUS, tcc->id, 0, 0,
		      0, 0, 0, 0, &res);

	ret = (res.a0 == 1UL) ? 1 : 0;

	return ret;
}

static const struct clk_ops tcc_hsiobus_ops = {
	.enable = tcc_hsiobus_enable,
	.disable = tcc_hsiobus_disable,
	.is_enabled = tcc_hsiobus_is_enabled,
};

static s32 tcc_pll_is_enabled(struct clk_hw *hw)
{
	struct arm_smccc_res res;
	const struct tcc_clk *tcc = to_tcc_clk((hw));
	s32 ret = 0;

	arm_smccc_smc(SIP_CLK_WDPR_IS_PLL_ENABLED, tcc->id,
		      0, 0, 0, 0, 0, 0, &res);
	ret = (res.a0 == 1UL) ? 1 : 0;

	return ret;
}

static ulong tcc_pll_recalc_rate(struct clk_hw *hw, ulong parent_rate)
{
	struct arm_smccc_res res;
	ulong rate = 0;
	const struct tcc_clk *tcc = to_tcc_clk((hw));

	(void)parent_rate;

	arm_smccc_smc(SIP_CLK_WDPR_GET_PLL, tcc->id, 0, 0, 0,
		      0, 0, 0, &res);
	rate = res.a0;

	return rate;
}

static const struct clk_ops tcc_pll_ops = {
	.is_enabled = tcc_pll_is_enabled,
	.recalc_rate = tcc_pll_recalc_rate,
};

static const struct of_device_id tcc_clk_dt_ids[] = {
	{ .compatible = "telechips,clk-fbus" },
	{ .compatible = "telechips,clk-peri" },
	{ .compatible = "telechips,clk-ddibus" },
	{ .compatible = "telechips,clk-iobus" },
	{ .compatible = "telechips,clk-vpubus" },
	{ .compatible = "telechips,clk-hsiobus" },
	{ .compatible = "telechips,clk-pll" },
	{ }
};

/* [DR] - MISRA C-2012 Rule 5.8
 * This weak symbol function is needed to inform some feature is not implemented
 * for this SoC. The strong symbol of this function need to be implemented in
 * each SoC's private clock drvier source code like clk-tccNNNx.c
 */
__weak void tcc_clk_proc_init(void)
{
	(void)pr_warn("[WARN][tcc_clk][%s] /proc/clocks is not initialized\n",
			__func__);
}

/* [DR] - MISRA C-2012 Rule 5.8
 * This weak symbol function is needed to inform some feature is not implemented
 * for this SoC. The strong symbol of this function need to be implemented in
 * each SoC's private clock drvier source code like clk-tccNNNx.c
 */
__weak int tcc_clk_suspend(void)
{
	(void)pr_warn("[WARN][tcc_clk][%s] clock suspend is not implemented\n",
			__func__);
	return 0;
}

/* [DR] - MISRA C-2012 Rule 5.8
 * This weak symbol function is needed to inform some feature is not implemented
 * for this SoC. The strong symbol of this function need to be implemented in
 * each SoC's private clock drvier source code like clk-tccNNNx.c
 */
__weak void tcc_clk_resume(void)
{
	(void)pr_warn("[WARN][tcc_clk][%s] clock resume is not implemented\n",
			__func__);
}

static struct syscore_ops tcc_clk_syscore_ops = {
        .suspend        = tcc_clk_suspend,
        .resume         = tcc_clk_resume,
};

static int tcc_clk_probe(struct platform_device *pdev)
{
	int ret = 0;
	const struct device_node *np = pdev->dev.of_node;
	static uint32_t is_init_proc = 0U;
	struct clk_ops const *ops = NULL;
	struct list_head *tc_clk_list = NULL;
	const char *compat_str = NULL;


	if (of_property_match_string(np, "compatible",
					"telechips,clk-pll") == 0) {
		ops = &tcc_pll_ops;
		tc_clk_list = &pll_clk_list;
	} else if (of_property_match_string(np, "compatible",
					"telechips,clk-fbus") == 0) {
		ops = &tcc_clkctrl_ops;
		tc_clk_list = &bus_clk_list;
	} else if (of_property_match_string(np, "compatible",
					"telechips,clk-peri") == 0) {
		ops = &tcc_peri_ops;
		tc_clk_list = &peri_clk_list;
	} else if (of_property_match_string(np, "compatible",
					"telechips,clk-ddibus") == 0) {
		ops = &tcc_ddibus_ops;
		tc_clk_list = &gate_clk_list;
	} else if (of_property_match_string(np, "compatible",
					"telechips,clk-iobus") == 0) {
		ops = &tcc_iobus_ops;
		tc_clk_list = &gate_clk_list;
	} else if (of_property_match_string(np, "compatible",
					"telechips,clk-vpubus") == 0) {
		ops = &tcc_vpubus_ops;
		tc_clk_list = &gate_clk_list;
	} else if (of_property_match_string(np, "compatible",
					"telechips,clk-hsiobus") == 0) {
		ops = &tcc_hsiobus_ops;
		tc_clk_list = &gate_clk_list;
	} else {
		(void)of_property_read_string(np, "compatible", &compat_str);
		dev_err(&pdev->dev, "[%s] Unknown  compatible string %s\n",
				__func__, compat_str);
	}

	if (tc_clk_list != NULL) {
		ret = tcc_clk_register(pdev, ops, tc_clk_list);

		if (ret != 0) {
			dev_err(&pdev->dev, "[%s] register failed with err: %d\n",
					__func__, ret);
		}
	}

	if (is_init_proc == 0U) {
		tcc_clk_proc_init();
		is_init_proc = 1;
	}

	return ret;
}

/* [DR] - MISRA C-2012 Rule 8.13
 * tcc_clk_remove is call back of struct platform_driver.remove
 * The function prototype is function is declare as
 * ‘int (*remove)(struct platform_device *)’
 */
static int tcc_clk_remove(struct platform_device *pdev)
{
	of_clk_del_provider(pdev->dev.of_node);

	return 0;
}

MODULE_DEVICE_TABLE(of, tcc_clk_dt_ids);

static struct platform_driver clk_tcc_driver = {
	.probe = tcc_clk_probe,
	.remove = tcc_clk_remove,
	.driver = {
		.name 	= "tcc-clock",
		.of_match_table = of_match_ptr(tcc_clk_dt_ids),
	},
};

static int __init clk_tcc_driver_init(void)
{
	register_syscore_ops(&tcc_clk_syscore_ops);
	return platform_driver_register(&clk_tcc_driver);
}
core_initcall(clk_tcc_driver_init);
static void __exit clk_tcc_driver_exit(void)
{
	unregister_syscore_ops(&tcc_clk_syscore_ops);
	return platform_driver_unregister(&clk_tcc_driver);
}
module_exit(clk_tcc_driver_exit);

MODULE_AUTHOR("Telechips Corporation");
MODULE_DESCRIPTION("Telechips Clock Driver");
MODULE_LICENSE("GPL v2");
