// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */


#include "clk.h"

#include <linux/syscore_ops.h>

/* [DR] - MISRA C-2012 Rule 8.13
 * tcc_onecell_get is call back of struct of_clk_provicer.get
 * function is declare as
 * ‘struct clk *(*get)(struct of_phandle_args *clkspec, void *data)’
 */
struct clk_hw *tc_onecell_get(struct of_phandle_args *clkspec, void *data)
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
	const struct tc_clk *clk_tc;

	for (i = 0; i < onecell_data->num; i++) {
		hw = onecell_data->hws[i];
		clk_tc = to_tc_clk(hw);

		if (idx == clk_tc->id) {
			break;
		}
	}

	return hw;
}

struct clk_hw_onecell_data *tc_prepare_onecell_data(struct device *dev,
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
		onecell_data = (struct clk_hw_onecell_data *)devm_kzalloc(dev,
				sizeof(struct clk_hw_onecell_data) +
				(unum_clks * sizeof(struct clk_hw *)),
				GFP_KERNEL);

		if (onecell_data != NULL) {
			onecell_data->num = unum_clks;
		} else {
			dev_err(dev, "[%s] failed to allocated memory\n", __func__);
		}
	}

	return onecell_data;
}

const char **tc_get_parent_info(struct device *dev, uint32_t *num_parent)
{
	struct device_node *np = dev->of_node;
	const char **parent_names = NULL;

	*num_parent = of_clk_get_parent_count(np);

	if (*num_parent > 0U) {
		/* [DR] - MISRA C-2012 Rule 11.5
		 * The void pointer type that returned from devm_kcalloc() must
		 * be converted to char pointer to access properly.
		 */
		parent_names = (const char **)devm_kcalloc(dev, *num_parent,
				sizeof(char *), GFP_KERNEL);

		if (parent_names != NULL) {
			(void)of_clk_parent_fill(np, parent_names, *num_parent);
		} else {
			dev_err(dev, "[%s] failed to allocated memory\n", __func__);
		}

	}

	return parent_names;
}

void tc_clk_register_clkdev(struct device *dev, struct clk_hw *hw)
{
	int ret = 0;
	ret = devm_clk_hw_register_clkdev(dev, hw, clk_hw_get_name(hw),
			dev_name(dev));
	if (ret != 0) {
		dev_err(dev, "[%s] '%s' failed to register clk lookup. ret: %d\n",
				__func__, clk_hw_get_name(hw), ret);
	}
}

int tc_clk_prepare_clk_data(struct device const *dev, struct clk_init_data *init,
		struct tc_clk *clk_tc, uint32_t index)
{
	const struct device_node *np = dev->of_node;

	const char	*name = NULL;
	uint32_t	flags = 0;
	int 		sindex;
	int		ret = 0;

	init->name = NULL;

	if (index > 0x7FFFFFFFU /* INT_MAX */) {
		index = 0x7FFFFFFFU;
	}

	sindex = (int)index;

	(void)of_property_read_u32_index(np, "ccf-flags", index,
			&flags);
	init->flags = (flags | CLK_GET_RATE_NOCACHE);

	ret = of_property_read_string_index(np, "clock-output-names", sindex,
			&name);

	if (ret == 0) {
		init->name = name;
		ret = of_property_read_u32_index(np, "clock-indices", index,
				&(clk_tc->id));
	}

	return ret;
}

static const struct of_device_id tc_clk_dt_ids[] = {
	{ .compatible = "telechips,clk-pll" },
	{ .compatible = "telechips,clk-fbus" },
	{ .compatible = "telechips,clk-peri" },
	{ .compatible = "telechips,clk-gate" },
	{ }
};

/* [DR] - MISRA C-2012 Rule 5.8
 * This weak symbol function is needed to inform some feature is not implemented
 * for this SoC. The strong symbol of this function need to be implemented in
 * each SoC's private clock drvier source code like clk-tccNNNx.c
 */
__weak int tc_clk_suspend(void)
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
__weak void tc_clk_resume(void)
{
	(void)pr_warn("[WARN][tcc_clk][%s] clock resume is not implemented\n",
			__func__);
}

static struct syscore_ops tc_clk_syscore_ops = {
        .suspend        = tc_clk_suspend,
        .resume         = tc_clk_resume,
};

static int tcc_clk_probe(struct platform_device *pdev)
{
	int ret = -EINVAL;
	const struct device_node *np = pdev->dev.of_node;
	const char *compat_str = NULL;

	if (of_property_match_string(np, "compatible",
					"telechips,clk-pll") == 0) {
		ret = tc_pll_register(pdev);
	} else if (of_property_match_string(np, "compatible",
					"telechips,clk-divider") == 0) {
		//ret = tc_divider_register(pdev);
	} else if (of_property_match_string(np, "compatible",
					"telechips,clk-fbus") == 0) {
		ret = tc_composite_register(pdev, TC_CLK_TYPE_BUS);
	} else if (of_property_match_string(np, "compatible",
					"telechips,clk-peri") == 0) {
		ret = tc_composite_register(pdev, TC_CLK_TYPE_PERI);
	} else if (of_property_match_string(np, "compatible",
					"telechips,clk-gate") == 0) {
		ret = tc_gate_register(pdev);
	} else {
		(void)of_property_read_string(np, "compatible", &compat_str);
		dev_err(&pdev->dev, "[%s] Unknown  compatible string %s\n",
				__func__, compat_str);
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

MODULE_DEVICE_TABLE(of, tc_clk_dt_ids);

static struct platform_driver clk_tcc_driver = {
	.probe = tcc_clk_probe,
	.remove = tcc_clk_remove,
	.driver = {
		.name 	= "tcc-clock",
		.of_match_table = of_match_ptr(tc_clk_dt_ids),
	},
};

static int __init clk_tcc_driver_init(void)
{
	struct arm_smccc_res	res = {0};

	arm_smccc_smc(SIP_CLK_INIT, 0, 0, 0, 0, 0, 0, 0, &res);

	(void)pr_info("Clock SiP Version : %lu.%lu.%lu\n",
			res.a0, res.a1, res.a2);

	register_syscore_ops(&tc_clk_syscore_ops);
	return platform_driver_register(&clk_tcc_driver);
}
core_initcall(clk_tcc_driver_init);

static void __exit clk_tcc_driver_exit(void)
{
	unregister_syscore_ops(&tc_clk_syscore_ops);
	return platform_driver_unregister(&clk_tcc_driver);
}
module_exit(clk_tcc_driver_exit);

bool tc_clk_dbg_warn_on = true;
module_param(tc_clk_dbg_warn_on, bool, 0x1A4/* 0644 */);
MODULE_PARM_DESC(tc_clk_dbg_warn_on, "Telechips Clock driver will print stack dump when clock operations are failed");


MODULE_AUTHOR("Telechips Corporation");
MODULE_DESCRIPTION("Telechips Clock Driver");
MODULE_LICENSE("GPL v2");
