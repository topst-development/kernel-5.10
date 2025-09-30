// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <soc/telechips/chipinfo.h>

#include "vin_wrap_cfg.h"
#include "../common/vin_wrap_vin.h"
#include "../common/vin_wrap_wdma.h"

static struct device_node *VinWrapCfg_np;
static void __iomem *pIREQ_reg;

void VIN_WRAP_CONFIG_SWReset(unsigned int component, unsigned int resetmode)
{
	u32 value;
	void __iomem *reg = pIREQ_reg;

	if ((resetmode != VIN_WRAP_CONFIG_RESET) &&
	    (resetmode != VIN_WRAP_CONFIG_CLEAR)) {
		(void)pr_err(
			"[ERR][VIN_WRAP_CONFIG_SWReset] %s, in error, invalid mode:%d\n",
			__func__, resetmode);

		goto FUNC_EXIT;
	}

	switch (get_vin_wrap_type(component)) {
	case get_vin_wrap_type(VIN_WRAP_WDMA):
		value = (__raw_readl(reg + VIN_SWREST_OFFSET) &
			 ~(0x1U << (get_vin_wrap_index(component) +
				    WDMA_PWRDN_SHIFT)));

		value |= (resetmode << (get_vin_wrap_index(component) +
					WDMA_PWRDN_SHIFT));

		__raw_writel(value, (reg + VIN_SWREST_OFFSET));
		break;
	case get_vin_wrap_type(VIN_WRAP_VIN):
		value = (__raw_readl(reg + VIN_SWREST_OFFSET) &
			 ~(0x1U << (get_vin_wrap_index(component) +
				    VIN_PWRDN_SHIFT)));

		value |= (resetmode
			  << (get_vin_wrap_index(component) + VIN_PWRDN_SHIFT));

		__raw_writel(value, (reg + VIN_SWREST_OFFSET));
		break;
	default:
		(void)pr_err(
			"[ERR][VIN_WRAP_CONFIG] %s, wrong component(0x%08x)\n",
			__func__, component);

		WARN_ON(1);
		break;
	}

FUNC_EXIT:
	return;
}

void __iomem *VIN_WRAP_IREQConfig_GetAddress(void)
{
	if (pIREQ_reg == NULL) {
		/* Prevent KCS warning */
		(void)pr_err("[ERR][VIN_WRAP_CFG] %s VIN_WRAP_IREQConfig\n",
			     __func__);
	}

	return pIREQ_reg;
}

void vin_wrap_config_init(void)
{
	VinWrapCfg_np =
		of_find_compatible_node(NULL, NULL, "telechips,vin_wrap_cfg");

	if (VinWrapCfg_np == NULL) {
		(void)pr_info(
			"[INF][VIN_WRAP_CONFIG] disabled [this is mandatory for vin wrapper]\n");
	} else {
		pIREQ_reg = of_iomap(VinWrapCfg_np, 0);

		if (pIREQ_reg != NULL) {
			(void)pr_info("[INF][VIN_WRAP_CONFIG] CONFIG\n");
		}
	}
}
