// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Telechips Inc.
 */

#include <linux/slab.h>

#include <soc/telechips/chipinfo.h>
#include <soc/telechips/smc.h>

#include "pll.h"
#include "divider.h"
#include "composite.h"
#include "gate.h"

static struct tc_divider tc_divider_list[11] = {
	{.id = PLL_0_DIV,		.name = "pll_0_div"},
	{.id = PLL_1_DIV,		.name = "pll_1_div"},
	{.id = PLL_2_DIV,		.name = "pll_2_div"},
	{.id = PLL_3_DIV,		.name = "pll_3_div"},
	{.id = PLL_4_DIV,		.name = "pll_4_div"},
	{.id = OSC_XIN_DIV,		.name = "osc_xin_div"},
	{.id = OSC_XTIN_DIV,		.name = "osc_xtin_div"},
	{.id = PLL_VIDEO_0_DIV_0,	.name = "pll_video_0_div_0"},
	{.id = PLL_VIDEO_0_DIV_1,	.name = "pll_video_0_div_1"},
	{.id = PLL_VIDEO_1_DIV_0,	.name = "pll_video_1_div_0"},
	{.id = PLL_VIDEO_1_DIV_1,	.name = "pll_video_1_div_1"},
};

static int tc_save_pll_clk(struct tc_pll *clk_pll)
{
	struct arm_smccc_res	res;

	const char *clk_name;
	int ret = 0;

	if (clk_pll != NULL) {
		clk_name = clk_hw_get_name(&clk_pll->clk_tc.hw);

		/* Skip Backup and restore below clocks
		 * These clocks will be restored by TF-A BL2 or BUS */
		if ((strstr(clk_name, "pll_mem") != NULL) ||
		    (strstr(clk_name, "pll_ca72") != NULL) ||
		    (strstr(clk_name, "pll_ca53") != NULL) ||
		    (strstr(clk_name, "pll_gpu") != NULL) ||
		    (strstr(clk_name, "pll_g2d") != NULL)) {
			/* Do nothing */
		} else {
			arm_smccc_smc(SIP_CLK_GET_PLL, clk_pll->clk_tc.id,
				0, 0, 0, 0, 0, 0, &res);

			if (res.a3 != CKC_OK) {
				dev_err(clk_pll->clk_tc.dev, "[%s] %s: Failed\n",
						__func__, clk_name);
				ret = -EINVAL;
			} else {
				dev_dbg(clk_pll->clk_tc.dev, "[%s] %s: rate: %lu pllpms: 0x%08lX pllcon: 0x%08lX\n",
						__func__, clk_name,
						res.a0, res.a1, res.a2);
				clk_pll->rate   = res.a0;
				clk_pll->pllpms = (uint32_t)(res.a1 & 0xFFFFFFFFUL);
				clk_pll->pllcon = (uint32_t)(res.a2 & 0xFFFFFFFFUL);
			}
		}
	}

	return ret;
}

static int tc_save_divider_clk(struct tc_divider *clk_element) {
	struct arm_smccc_res res;

	int ret = 0;

	if (clk_element != NULL) {
		arm_smccc_smc(SIP_CLK_GET_DIVIDER, clk_element->id,
			0, 0, 0, 0, 0, 0, &res);

		if (res.a3 != CKC_OK) {
			(void)pr_err("[%s] %s: Failed\n", __func__, clk_element->name);
			ret = -EINVAL;
		} else {
			pr_debug("[%s] %s: rate: %lu en: %lu divider: %lu\n",
					__func__, clk_element->name,
					res.a0, res.a1, res.a2);

			clk_element->rate    = res.a0;
			clk_element->en	     = (uint32_t)(res.a1 & 0xFFFFFFFFUL);
			clk_element->divider = (uint32_t)((res.a2 & 0x00FFFFFFUL) + 1UL);
		}
	}

	return ret;
}

static int tc_save_composite_clk(struct tc_composite *clk_element) {
	struct arm_smccc_res	res;

	unsigned long sip_id_get, sip_id_en;
	const char *clk_name;
	int ret = 0;

	if (clk_element != NULL) {
		clk_name = clk_hw_get_name(&clk_element->clk_tc.hw);

		switch (clk_element->clk_type) {
		case TC_CLK_TYPE_BUS:
			sip_id_get = SIP_CLK_GET_CLKCTRL;
			sip_id_en  = SIP_CLK_IS_CLKCTRL_ENABLED;
			break;
		case TC_CLK_TYPE_PERI:
			sip_id_get = SIP_CLK_GET_PCLKCTRL;
			sip_id_en  = SIP_CLK_IS_PCLKCTRL_ENABLED;
			break;
		default:
			sip_id_get = 0UL;
			sip_id_en  = 0UL;
			dev_err(clk_element->clk_tc.dev, "[%s] %s: Clock Type is not defined.\n",
					__func__, clk_name);
			ret = -EINVAL;
			break;
		}

		if (ret == 0) {
			arm_smccc_smc(sip_id_en, clk_element->clk_tc.id,
				0, 0, 0, 0, 0, 0, &res);
			if (res.a3 == CKC_OK) {
				clk_element->en = (uint32_t)(res.a0 & 0xFFFFFFFFUL);
				arm_smccc_smc(sip_id_get,
						clk_element->clk_tc.id,
						0, 0, 0, 0, 0, 0, &res);
			}

			if (res.a3 != CKC_OK) {
				dev_err(clk_element->clk_tc.dev, "[%s] %s: Failed\n",
						__func__, clk_name);
				ret = -EINVAL;
			} else {
				dev_dbg(clk_element->clk_tc.dev, "[%s] %s: rate: %lu  en: %u sel: %lu divider: %lu\n",
						__func__, clk_name, res.a0,
						clk_element->en,
						res.a1, res.a2);

				clk_element->rate    = res.a0;
				clk_element->sel     = (uint32_t)(res.a1 & 0xFFFFFFFFUL);
				clk_element->divider = (uint32_t)(res.a2 & 0x00FFFFFFUL);
			}

		}
	}

	return ret;
}


static int tc_save_gate_clk(struct tc_gate *clk_element) {
	struct arm_smccc_res	res;

	const char *clk_name;
	int ret = 0;

	if (clk_element != NULL) {
		clk_name = clk_hw_get_name(&clk_element->clk_tc.hw);

		arm_smccc_smc(SIP_CLK_IS_GATE_ENABLED, clk_element->clk_tc.id,
			0, 0, 0, 0, 0, 0, &res);

		if (res.a3 != CKC_OK) {
			dev_err(clk_element->clk_tc.dev, "[%s] %s: Failed\n",
					__func__, clk_name);
			ret = -EINVAL;
		} else {
			dev_dbg(clk_element->clk_tc.dev, "[%s] %s: en: %lu\n",
					__func__, clk_name, res.a0);
			clk_element->en = (uint32_t)(res.a0 & 0xFFFFFFFFUL);
		}
	}

	return ret;
}

static int tc_ckc_save(void)
{
	struct tc_clk *clk_elemnt;
	uint32_t index;
	int ret = 0;

	list_for_each_entry(clk_elemnt, &tc_gate_list, list) {
		if (ret == 0) {
			ret = tc_save_gate_clk(to_tc_gate(&clk_elemnt->hw));
		} else {
			break;
		}
	}

	list_for_each_entry(clk_elemnt, &tc_composite_list, list) {
		if (ret == 0) {
			ret = tc_save_composite_clk(to_tc_composite(&clk_elemnt->hw));
		} else {
			break;
		}
	}

	for (index = 0; index < ARRAY_SIZE(tc_divider_list); index++) {
		if (ret == 0) {
			ret = tc_save_divider_clk(&tc_divider_list[index]);
		} else {
			break;
		}
	}

	list_for_each_entry(clk_elemnt, &tc_pll_list, list) {
		if (ret == 0) {
			ret = tc_save_pll_clk(to_tc_pll(&clk_elemnt->hw));
		} else {
			break;
		}
	}

	return ret;
}



int tc_clk_suspend(void) {
	int ret = 0;
	u32 sysid = get_system_identity();

	if (is_main_system(sysid)) {
		ret = tc_ckc_save();
	}

	return ret;
}

static void tc_restore_pll_clk(struct tc_pll *clk_pll)
{
	struct arm_smccc_res	res;

	const char *clk_name;

	if (clk_pll != NULL) {
		clk_name = clk_hw_get_name(&clk_pll->clk_tc.hw);

		/* Skip Backup and restore below clocks
		 * These clocks will be restored by TF-A BL2 or BUS */
		if ((strstr(clk_name, "pll_mem") != NULL) ||
		    (strstr(clk_name, "pll_ca72") != NULL) ||
		    (strstr(clk_name, "pll_ca53") != NULL) ||
		    (strstr(clk_name, "pll_gpu") != NULL) ||
		    (strstr(clk_name, "pll_g2d") != NULL)) {
			/* Do nothing */
		} else {
			arm_smccc_smc(SIP_CLK_SET_PLL_RAW, clk_pll->clk_tc.id,
				clk_pll->pllpms, clk_pll->pllcon,
				0, 0, 0, 0, &res);

			if (res.a3 != CKC_OK) {
				dev_err(clk_pll->clk_tc.dev, "[%s] %s: Failed\n",
						__func__, clk_name);
			} else {
				dev_dbg(clk_pll->clk_tc.dev, "[%s] %s: pllpms: 0x%08X pllcon: 0x%08X\n",
						__func__, clk_name,
						clk_pll->pllpms,
						clk_pll->pllcon);
			}
		}
	}
}

static void tc_restore_divider_clk(struct tc_divider *clk_divider) {
	struct arm_smccc_res res;

	if (clk_divider != NULL) {
		arm_smccc_smc(SIP_CLK_SET_DIVIDER, clk_divider->id,
			clk_divider->divider, clk_divider->en, 0, 0, 0, 0, &res);

		if (res.a3 != CKC_OK) {
			(void)pr_err("[%s] %s: Failed\n", __func__, clk_divider->name);
		} else {
			pr_debug("[%s] %s: rate: %lu en: %lu divider: %lu\n",
					__func__, clk_divider->name,
					res.a0, res.a1, res.a2);
		}
	}
}

static void tc_restore_composite_clk(struct tc_composite *clk_element) {
	struct arm_smccc_res	res;

	unsigned long sip_id_set;
	const char *clk_name;

	if (clk_element != NULL) {
		clk_name = clk_hw_get_name(&clk_element->clk_tc.hw);

		switch (clk_element->clk_type) {
		case TC_CLK_TYPE_BUS:
			sip_id_set = SIP_CLK_SET_CLKCTRL;
			break;
		case TC_CLK_TYPE_PERI:
			sip_id_set = SIP_CLK_SET_PCLKCTRL;
			break;
		default:
			sip_id_set = 0UL;
			dev_err(clk_element->clk_tc.dev, "[%s] %s: Clock Type is not defined.\n",
					__func__, clk_name);
			break;
		}

		if (sip_id_set != 0UL) {
			arm_smccc_smc(sip_id_set, clk_element->clk_tc.id,
				clk_element->en, clk_element->rate,
				((unsigned long)clk_element->flags | TC_CLK_F_SRC(clk_element->sel)),
				0, 0, 0, &res);
			if (res.a3 != CKC_OK) {
				dev_err(clk_element->clk_tc.dev, "[%s] %s: Failed\n",
						__func__, clk_name);
			} else {
				dev_dbg(clk_element->clk_tc.dev, "[%s] %s: rate: %lu sel: %lu divider: %lu\n",
						__func__, clk_name, res.a0,
						res.a1, res.a2);
			}

		}
	}
}

static void tc_restore_gate_clk(struct tc_gate *clk_element) {
	struct arm_smccc_res	res;

	const char *clk_name;

	if (clk_element != NULL) {
		clk_name = clk_hw_get_name(&clk_element->clk_tc.hw);

		if (clk_element->en == CKC_ENABLE) {
			arm_smccc_smc(SIP_CLK_ENABLE_GATE, clk_element->clk_tc.id,
				0, 0, 0, 0, 0, 0, &res);
		} else {
			arm_smccc_smc(SIP_CLK_DISABLE_GATE, clk_element->clk_tc.id,
				0, 0, 0, 0, 0, 0, &res);
		}

		if (res.a3 != CKC_OK) {
			dev_err(clk_element->clk_tc.dev, "[%s] %s: Failed\n",
					__func__, clk_name);
		} else {
			dev_dbg(clk_element->clk_tc.dev, "[%s] %s: en: %u\n",
					__func__, clk_name, clk_element->en);
		}
	}
}

static void tc_prepare_restore(unsigned long reset_id) {
	struct arm_smccc_res	res;

	arm_smccc_smc(SIP_CLK_SET_CLKCTRL, reset_id, CKC_ENABLE,
			(XIN_CLK_RATE/2UL), TC_CLK_F_SRC(SMU_CLKCTRL_SEL_XIN),
			0, 0, 0, &res);

	if (res.a3 != CKC_OK) {
		(void)pr_err("[%s] 0x%08lX: Failed (%lu)\n", __func__, reset_id, res.a3);
	} else {
		pr_debug("[%s] 0x%08lX: rate: %lu sel: %lu divider: %lu\n",
				__func__, reset_id, res.a0,
				res.a1, res.a2);
	}
}

static void tc_ckc_restore(void)
{
	struct tc_clk *clk_elemnt;
	const unsigned long bus_reset_list[3] = {
		FBUS_IO, FBUS_SMU, FBUS_HSIO,
	};
	uint32_t index;

	for (index = 0; index < ARRAY_SIZE(bus_reset_list); index++) {
		tc_prepare_restore(bus_reset_list[index]);
	}

	list_for_each_entry(clk_elemnt, &tc_pll_list, list) {
		tc_restore_pll_clk(to_tc_pll(&clk_elemnt->hw));
	}

	for (index = 0; index < ARRAY_SIZE(tc_divider_list); index++) {
		tc_restore_divider_clk(&tc_divider_list[index]);
	}

	list_for_each_entry(clk_elemnt, &tc_composite_list, list) {
		tc_restore_composite_clk(to_tc_composite(&clk_elemnt->hw));
	}

	list_for_each_entry(clk_elemnt, &tc_gate_list, list) {
		tc_restore_gate_clk(to_tc_gate(&clk_elemnt->hw));
	}

}

void tc_clk_resume(void)
{
	struct arm_smccc_res res = {0};
	u32 sysid = get_system_identity();
	bool is_main;

	is_main = is_main_system(sysid);
	if (is_main) {
		/*
		 * Just do your best to restore clock frequency.
		 */
		tc_ckc_restore();
	}

	/* Wait "clock resume ready" be flagged */
	do {
		arm_smccc_smc(SIP_CLK_SYNC, CKC_RESUME, is_main ? 1UL : 0UL,
				0, 0, 0, 0, 0, &res);
	} while (res.a3 != CKC_OK);
}

