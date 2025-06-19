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


/* If we handle all clock registers in kernel, we can use below CCF functions
 * clk_save_context() / clk_restore_context()
 */
struct ckc_backup_sts {
	struct clk_hw *hw;
	unsigned long rate;
	unsigned long src;
	bool en;
	struct list_head list;
};

/* PLL Divider is not handled as clock device backup manually */
struct plldiv_backup_sts {
	unsigned long id;
	unsigned long divider;
};

static struct tc_divider tc_divider_list[20] = {
	{.id = PLL_0_DIV,		.name = "pll-0-div",		.flags = 0U},
	{.id = PLL_1_DIV,		.name = "pll-1-div",		.flags = 0U},
	{.id = PLL_2_DIV,		.name = "pll-2-div",		.flags = 0U},
	{.id = PLL_3_DIV,		.name = "pll-3-div",		.flags = 0U},
	{.id = PLL_4_DIV,		.name = "pll-4-div",		.flags = 0U},
	{.id = PLL_DP_DIV_0,		.name = "pll-dp-div-0",		.flags = 0U},
	{.id = PLL_DP_DIV_1,		.name = "pll-dp-div-1",		.flags = 0U},
	{.id = PLL_DP_DIV_2,		.name = "pll-dp-div-2",		.flags = 0U},
	{.id = PLL_VIDEO_0_DIV_0,	.name = "pll-video-0-div-0",	.flags = 0U},
	{.id = PLL_VIDEO_0_DIV_1,	.name = "pll-video-0-div-1",	.flags = 0U},
	{.id = PLL_VIDEO_1_DIV_0,	.name = "pll-video-1-div-0",	.flags = 0U},
	{.id = PLL_VIDEO_1_DIV_1,	.name = "pll-video-1-div-1",	.flags = 0U},
	{.id = PLL_AUDIO_0_DIV,		.name = "pll-audio-0-div",	.flags = 0U},
	{.id = PLL_AUDIO_1_DIV,		.name = "pll-audio-1-div",	.flags = 0U},
	{.id = PLL_CAM_DIV_0,		.name = "pll-cam-div-0",	.flags = 0U},
	{.id = PLL_CAM_DIV_1,		.name = "pll-cam-div-1",	.flags = 0U},
	{.id = PLL_CAM_DIV_2,		.name = "pll-cam-div-2",	.flags = 0U},
	{.id = PLL_CAM_DIV_3,		.name = "pll-cam-div-3",	.flags = 0U},
	{.id = PLL_CAM_DIV_4,		.name = "pll-cam-div-4",	.flags = 0U},
	{.id = PLL_NPU_DIV,		.name = "pll-npu-div",		.flags = 0U},
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
		    (strstr(clk_name, "pll_mp") != NULL) ||
		    (strstr(clk_name, "pll_sp") != NULL) ||
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

		if ((strstr(clk_name, "dsu") != NULL)) {
			/* Don't need to save dsu clocks */
		} else {

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
	struct tc_clk *clk_element;
	uint32_t index;
	int ret = 0;

	u32 sysid = get_system_identity();
	bool is_main;

	struct tc_pll		*clk_pll;
	struct tc_divider	*clk_divider;
	struct tc_composite	*clk_composite;
	struct tc_gate		*clk_gate;

	is_main = is_main_system(sysid);

	list_for_each_entry(clk_element, &tc_gate_list, list) {
		if (ret == 0) {
			clk_gate = to_tc_gate(&clk_element->hw);
			if (is_main != ((clk_gate->flags & CLK_RESTORE_BY_SUBCORE) != 0)) {
				ret = tc_save_gate_clk(clk_gate);
			}
		} else {
			break;
		}
	}

	list_for_each_entry(clk_element, &tc_composite_list, list) {
		if (ret == 0) {
			clk_composite = to_tc_composite(&clk_element->hw);
			if (is_main != ((clk_composite->flags & CLK_RESTORE_BY_SUBCORE) != 0)) {
				ret = tc_save_composite_clk(clk_composite);
			}
		} else {
			break;
		}
	}

	for (index = 0; index < ARRAY_SIZE(tc_divider_list); index++) {
		if (ret == 0) {
			clk_divider = &tc_divider_list[index];
			if (is_main != ((clk_divider->flags & CLK_RESTORE_BY_SUBCORE) != 0)) {
				ret = tc_save_divider_clk(clk_divider);
			}
		} else {
			break;
		}
	}

	list_for_each_entry(clk_element, &tc_pll_list, list) {
		if (ret == 0) {
			clk_pll = to_tc_pll(&clk_element->hw);
			if (is_main != ((clk_pll->flags & CLK_RESTORE_BY_SUBCORE) != 0)) {
				ret = tc_save_pll_clk(clk_pll);
			}
		} else {
			break;
		}
	}

	return ret;
}

int tc_clk_suspend(void) {
	int ret = 0;

	ret = tc_ckc_save();

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
		    (strstr(clk_name, "pll_mp") != NULL) ||
		    (strstr(clk_name, "pll_sp") != NULL) ||
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
				dev_dbg(clk_pll->clk_tc.dev, "[%s] %s: rate: %lu pllpms: 0x%08lX pllcon: 0x%08lX\n",
						__func__, clk_name,
						res.a0, res.a1, res.a2);
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

		if ((strstr(clk_name, "dsu") != NULL)) {
			/* Don't need to save dsu clocks */
		} else {

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
	struct tc_clk *clk_element;
	u32 sysid = get_system_identity();
	uint32_t index;
	bool is_main;

	struct tc_pll		*clk_pll;
	struct tc_divider	*clk_divider;
	struct tc_composite	*clk_composite;
	struct tc_gate		*clk_gate;

	const unsigned long bus_reset_list[6] = {
		FBUS_IO, FBUS_SMU, FBUS_HSIO, FBUS_CPU, FBUS_HSM, FBUS_TAW,
	};

	is_main = is_main_system(sysid);

	if (is_main) {
		for (index = 0; index < ARRAY_SIZE(bus_reset_list); index++) {
			tc_prepare_restore(bus_reset_list[index]);
		}
 	}

	list_for_each_entry(clk_element, &tc_pll_list, list) {
		clk_pll = to_tc_pll(&clk_element->hw);
		if (is_main != ((clk_pll->flags & CLK_RESTORE_BY_SUBCORE) != 0)) {
			tc_restore_pll_clk(clk_pll);
		}
	}

	for (index = 0; index < ARRAY_SIZE(tc_divider_list); index++) {
		clk_divider = &tc_divider_list[index];
		if (is_main != ((clk_divider->flags & CLK_RESTORE_BY_SUBCORE) != 0)) {
			tc_restore_divider_clk(clk_divider);
		}
	}

	list_for_each_entry(clk_element, &tc_composite_list, list) {
		clk_composite = to_tc_composite(&clk_element->hw);
		if (is_main != ((clk_composite->flags & CLK_RESTORE_BY_SUBCORE) != 0)) {
			tc_restore_composite_clk(clk_composite);
		}
	}

	list_for_each_entry(clk_element, &tc_gate_list, list) {
		clk_gate = to_tc_gate(&clk_element->hw);
		if (is_main != ((clk_gate->flags & CLK_RESTORE_BY_SUBCORE) != 0)) {
			tc_restore_gate_clk(clk_gate);
		}
	}

}

void tc_clk_resume(void)
{
	struct arm_smccc_res res = {0};
	u32 sysid = get_system_identity();
	bool is_main;

	void *dp_cfg_access = ioremap(0x124D0000, sizeof(uint32_t));

	is_main = is_main_system(sysid);
	/* If DP related clocks resotre by Sub Cluster, below if clause MUST
	 * Change as below
	 * if (!is_main)
	 */
	if (is_main) {
		writel(0x1, dp_cfg_access);
	}

	tc_ckc_restore();

	/* If DP related clocks resotre by Sub Cluster, below if clause MUST
	 * Change as below
	 * if (!is_main)
	 */
	if (is_main) {
		writel(0x0, dp_cfg_access);
	}

	/* Wait "clock resume ready" be flagged */
	do {
		arm_smccc_smc(SIP_CLK_SYNC, CKC_RESUME, is_main ? 1UL : 0UL,
				0, 0, 0, 0, 0, &res);
	} while (res.a3 != CKC_OK);
}
