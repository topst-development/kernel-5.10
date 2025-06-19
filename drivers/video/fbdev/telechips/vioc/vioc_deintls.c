// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <video/telechips/vioc_ddicfg.h>	// is_VIOC_REMAP
#include <video/telechips/vioc_deintls.h>

static void __iomem *pDEINTLS_reg;

void __iomem *VIOC_DEINTLS_GetAddress(void)
{
	if (pDEINTLS_reg == NULL) {
		/* Prevent KSC Warning */
		(void)pr_err("[ERR][DEINTLS] %s: address NULL\n", __func__);
	}

	return pDEINTLS_reg;
}
EXPORT_SYMBOL(VIOC_DEINTLS_GetAddress);

int vioc_deintls_init(void)
{
	struct device_node *dev_np;

	dev_np = of_find_compatible_node(NULL, NULL, "telechips,vioc_deintls");
	if (dev_np == NULL) {
		/* Prevent KSC Warning */
		(void)pr_info("[INF][DEINTLS] disabled\n");
	} else {
		pDEINTLS_reg = (void __iomem *)of_iomap(dev_np,
						((is_VIOC_REMAP != 0U) ? 1 : 0));
		if (pDEINTLS_reg != NULL) {
			/* Prevent KSC Warning */
			(void)pr_info("[INF][DEINTLS] DEINTLS\n");
		}
	}

	return 0;
}
EXPORT_SYMBOL(vioc_deintls_init);