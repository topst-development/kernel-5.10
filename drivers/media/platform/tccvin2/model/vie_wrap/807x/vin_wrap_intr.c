// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include "vin_wrap_intr.h"
#include "vin_wrap_cfg.h"
#include "../common/vin_wrap_vin.h"
#include "../common/vin_wrap_wdma.h"

static struct device_node *VinWrapIntr_np;
static int vin_wrap_base_irq_num[VIN_IRQI_MAC] = {
	0,
};

/* HIS_GOTO */
int vin_wrap_intr_enable(int irq, int id, unsigned int mask)
{
	void __iomem *reg;
	int sub_id;
	unsigned int type_clr_offset;
	int ret = -1;

	if ((id < 0) || (id > VIN_WRAP_INTR_NUM)) {
		ret = -1;

		goto FUNC_EXIT;
	}

	switch (id) {
	/*
	 * VIN_INT[31]: Not Used
	 * VIN_INT[19]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[18]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[17]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[16]: Enable interrupt if 1 / Disable interrupt if 0
	 */
	case VIN_WRAP_INTR_VIN0:
	case VIN_WRAP_INTR_VIN1:
	case VIN_WRAP_INTR_VIN2:
	case VIN_WRAP_INTR_VIN3:
		sub_id = id - VIN_WRAP_INTR_VIN0;

		reg = VIN_WRAP_VIN_GetAddress((unsigned int)sub_id * 2U) +
		      VIN_INT;

		/* clera irq status */

		__raw_writel((__raw_readl(reg) |
			      (mask & VIN_WRAP_VIN_INT_MASK)),
			     reg);

		/* enable irq */

		__raw_writel(__raw_readl(reg) |
				     ((mask & VIN_WRAP_VIN_INT_ENABLE) << 16U),
			     reg);
		ret = 0;
		break;
	case VIN_WRAP_INTR_WDMA0:
	case VIN_WRAP_INTR_WDMA1:
	case VIN_WRAP_INTR_WDMA2:
	case VIN_WRAP_INTR_WDMA3:
		sub_id = id - VIN_WRAP_INTR_WDMA0;

		/* clera irq status */

		reg = VIN_WRAP_WDMA_GetAddress((unsigned int)sub_id) +
		      WDMAIRQSTS_OFFSET;

		__raw_writel((mask & VIN_WRAP_WDMA_INT_MASK), reg);

		/* enable irq */

		reg = VIN_WRAP_WDMA_GetAddress((unsigned int)sub_id) +
		      WDMAIRQMSK_OFFSET;

		__raw_writel(__raw_readl(reg) &
				     ~(mask & VIN_WRAP_WDMA_INT_MASK),
			     reg);
		ret = 0;
		break;
	default:
		(void)pr_err("[ERR][VIN_WRAP_INTR] %s: id(%d) is wrong.\n",
			     __func__, id);
		ret = -1;
		break;
	}

	if (ret < 0) {
		goto FUNC_EXIT;
	}

	if (irq == vin_wrap_base_irq_num[0]) {
		/* Prevent KCS warning */
		type_clr_offset = VIN_IREQ0_CLR_OFFSET;
	} else if (irq == vin_wrap_base_irq_num[1]) {
		/* Prevent KCS warning */
		type_clr_offset = VIN_IREQ1_CLR_OFFSET;
	} else if (irq == vin_wrap_base_irq_num[2]) {
		/* Prevent KCS warning */
		type_clr_offset = VIN_IREQ2_CLR_OFFSET;
	} else if (irq == vin_wrap_base_irq_num[3]) {
		/* Prevent KCS warning */
		type_clr_offset = VIN_IREQ3_CLR_OFFSET;
	} else {
		(void)pr_err(
			"[ERR][VIN_WRAP_INTR] %s-%d :: irq(%d) is wierd.\n",
			__func__, __LINE__, irq);
		ret = -1;
	}

	if (ret < 0) {
		goto FUNC_EXIT;
	}
	reg = VIN_WRAP_IREQConfig_GetAddress();

	__raw_writel((u32)1U << (unsigned int)id, reg + type_clr_offset);

FUNC_EXIT:
	return ret;
}

/* HIS_GOTO */ /* HIS_CALLS */
int vin_wrap_intr_disable(int irq, int id, unsigned int mask)
{
	void __iomem *reg;
	int sub_id;
	unsigned int do_irq_mask = 1U;
	unsigned int type_set_offset;
	int ret = -1;

	if ((id < 0) || (id > VIN_WRAP_INTR_NUM)) {
		ret = -1;

		goto FUNC_EXIT;
	}

	switch (id) {
	/*
	 * VIN_INT[31]: Not Used
	 * VIN_INT[19]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[18]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[17]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[16]: Enable interrupt if 1 / Disable interrupt if 0
	 */
	case VIN_WRAP_INTR_VIN0:
	case VIN_WRAP_INTR_VIN1:
	case VIN_WRAP_INTR_VIN2:
	case VIN_WRAP_INTR_VIN3:
		sub_id = id - VIN_WRAP_INTR_VIN0;

		reg = VIN_WRAP_VIN_GetAddress((unsigned int)sub_id * 2U) +
		      VIN_INT;

		__raw_writel(__raw_readl(reg) &
				     ~((mask & VIN_WRAP_VIN_INT_ENABLE) << 16),
			     reg);
		ret = 0;
		break;
	case VIN_WRAP_INTR_WDMA0:
	case VIN_WRAP_INTR_WDMA1:
	case VIN_WRAP_INTR_WDMA2:
	case VIN_WRAP_INTR_WDMA3:
		sub_id = id - VIN_WRAP_INTR_WDMA0;

		reg = VIN_WRAP_WDMA_GetAddress((unsigned int)sub_id) +
		      WDMAIRQMSK_OFFSET;

		__raw_writel(__raw_readl(reg) | (mask & VIN_WRAP_WDMA_INT_MASK),
			     reg);
		if ((__raw_readl(reg) & VIN_WRAP_WDMA_INT_MASK) !=
		    VIN_WRAP_WDMA_INT_MASK) {
			/* Prevent KCS warning */
			do_irq_mask = 0U;
		}
		ret = 0;
		break;
	default:
		(void)pr_err("[ERR][VIN_WRAP_INTR] %s: id(%d) is wrong.\n",
			     __func__, id);
		ret = 0;
		break;
	}

	if (do_irq_mask == 1U) {
		if (irq == vin_wrap_base_irq_num[0]) {
			/* Prevent KCS warning */
			type_set_offset = VIN_IREQ0_MSK_OFFSET;
		} else if (irq == vin_wrap_base_irq_num[1]) {
			/* Prevent KCS warning */
			type_set_offset = VIN_IREQ1_MSK_OFFSET;
		} else if (irq == vin_wrap_base_irq_num[2]) {
			/* Prevent KCS warning */
			type_set_offset = VIN_IREQ2_MSK_OFFSET;
		} else if (irq == vin_wrap_base_irq_num[3]) {
			/* Prevent KCS warning */
			type_set_offset = VIN_IREQ3_MSK_OFFSET;
		} else {
			(void)pr_err(
				"[ERR][VIN_WRAP_INTR] %s-%d :: irq(%d) is wierd.\n",
				__func__, __LINE__, irq);
			ret = -1;
		}

		if (ret < 0) {
			goto FUNC_EXIT;
		}
		reg = VIN_WRAP_IREQConfig_GetAddress();

		__raw_writel((u32)1U << (unsigned int)id,
			     reg + type_set_offset);
	}
	ret = 0;

FUNC_EXIT:
	return ret;
}

/* HIS_GOTO */

unsigned int vin_wrap_intr_get_status(int id)
{
	const void __iomem *reg;
	unsigned int ret = 0U;

	if ((id < 0) || (id > VIN_WRAP_INTR_NUM)) {
		ret = 0U;

		goto FUNC_EXIT;
	}

	switch (id) {
	case VIN_WRAP_INTR_VIN0:
	case VIN_WRAP_INTR_VIN1:
	case VIN_WRAP_INTR_VIN2:
	case VIN_WRAP_INTR_VIN3:
		id -= VIN_WRAP_INTR_VIN0;

		reg = VIN_WRAP_VIN_GetAddress((unsigned int)id * 2U) + VIN_INT;

		ret = (__raw_readl(reg) & VIN_WRAP_VIN_INT_MASK);
		break;
	case VIN_WRAP_INTR_WDMA0:
	case VIN_WRAP_INTR_WDMA1:
	case VIN_WRAP_INTR_WDMA2:
	case VIN_WRAP_INTR_WDMA3:
		id -= VIN_WRAP_INTR_WDMA0;

		reg = VIN_WRAP_WDMA_GetAddress((unsigned int)id) +
		      WDMAIRQSTS_OFFSET;

		ret = (__raw_readl(reg) & VIN_WRAP_WDMA_INT_MASK);
		break;
	default:
		(void)pr_err("[ERR][VIN_WRAP_INTR] %s: id(%d) is wrong.\n",
			     __func__, id);
		ret = 0U;
		break;
	}
FUNC_EXIT:
	return ret;
}

/* HIS_GOTO */

bool is_vin_wrap_intr_activatied(int id, unsigned int mask)
{
	const void __iomem *reg;
	bool ret = (bool)false;

	if ((id < 0) || (id > VIN_WRAP_INTR_NUM)) {
		ret = (bool)false;

		goto FUNC_EXIT;
	}

	switch (id) {
	case VIN_WRAP_INTR_VIN0:
	case VIN_WRAP_INTR_VIN1:
	case VIN_WRAP_INTR_VIN2:
	case VIN_WRAP_INTR_VIN3:
		id -= VIN_WRAP_INTR_VIN0;

		reg = VIN_WRAP_VIN_GetAddress((unsigned int)id * 2U) + VIN_INT;

		if ((__raw_readl(reg) & (mask & VIN_WRAP_VIN_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
	case VIN_WRAP_INTR_WDMA0:
	case VIN_WRAP_INTR_WDMA1:
	case VIN_WRAP_INTR_WDMA2:
	case VIN_WRAP_INTR_WDMA3:
		id -= VIN_WRAP_INTR_WDMA0;

		reg = VIN_WRAP_WDMA_GetAddress((unsigned int)id) +
		      WDMAIRQSTS_OFFSET;

		if ((__raw_readl(reg) & (mask & VIN_WRAP_WDMA_INT_MASK)) !=
		    0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
	default:
		(void)pr_err("[ERR][VIN_WRAP_INTR] %s: id(%d) is wrong.\n",
			     __func__, id);
		ret = (bool)false;
		break;
	}
FUNC_EXIT:
	return ret;
}

/* HIS_GOTO */

bool is_vin_wrap_intr_unmasked(int id, unsigned int mask)
{
	const void __iomem *reg;
	bool ret = (bool)false;

	if ((id < 0) || (id > VIN_WRAP_INTR_NUM)) {
		ret = (bool)false;

		goto FUNC_EXIT;
	}

	switch (id) {
	/*
	 * VIN_INT[31]: Not Used
	 * VIN_INT[19]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[18]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[17]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[16]: Enable interrupt if 1 / Disable interrupt if 0
	 */
	case VIN_WRAP_INTR_VIN0:
	case VIN_WRAP_INTR_VIN1:
	case VIN_WRAP_INTR_VIN2:
	case VIN_WRAP_INTR_VIN3:
		id -= VIN_WRAP_INTR_VIN0;

		reg = VIN_WRAP_VIN_GetAddress((unsigned int)id * 2U) + VIN_INT;

		if ((__raw_readl(reg) &
		     ((mask & VIN_WRAP_VIN_INT_ENABLE) << 16U)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
	case VIN_WRAP_INTR_WDMA0:
	case VIN_WRAP_INTR_WDMA1:
	case VIN_WRAP_INTR_WDMA2:
	case VIN_WRAP_INTR_WDMA3:
		id -= VIN_WRAP_INTR_WDMA0;

		reg = VIN_WRAP_WDMA_GetAddress((unsigned int)id) +
		      WDMAIRQMSK_OFFSET;

		if ((__raw_readl(reg) & (mask & VIN_WRAP_WDMA_INT_MASK)) !=
		    0U) {
			/* Prevent KCS warning */
			ret = (bool)false;
		} else {
			/* Prevent KCS warning */
			ret = (bool)true;
		}
		break;
	default:
		(void)pr_err("[ERR][VIN_WRAP_INTR] %s: id(%d) is wrong.\n",
			     __func__, id);
		ret = (bool)false;
		break;
	}
FUNC_EXIT:
	return ret;
}

/* HIS_GOTO */ /* HIS_CALLS */

int vin_wrap_intr_clear(int id, unsigned int mask)
{
	void __iomem *reg;
	int ret = -1;

	if ((id < 0) || (id > VIN_WRAP_INTR_NUM)) {
		ret = -1;

		goto FUNC_EXIT;
	}

	switch (id) {
	case VIN_WRAP_INTR_VIN0:
	case VIN_WRAP_INTR_VIN1:
	case VIN_WRAP_INTR_VIN2:
	case VIN_WRAP_INTR_VIN3:
		id -= VIN_WRAP_INTR_VIN0;

		reg = VIN_WRAP_VIN_GetAddress((unsigned int)id * 2U) + VIN_INT;

		__raw_writel((__raw_readl(reg) |
			      (mask & VIN_WRAP_VIN_INT_MASK)),
			     reg);
		ret = 0;
		break;
	case VIN_WRAP_INTR_WDMA0:
	case VIN_WRAP_INTR_WDMA1:
	case VIN_WRAP_INTR_WDMA2:
	case VIN_WRAP_INTR_WDMA3:
		id -= VIN_WRAP_INTR_WDMA0;

		reg = VIN_WRAP_WDMA_GetAddress((unsigned int)id) +
		      WDMAIRQSTS_OFFSET;

		__raw_writel((mask & VIN_WRAP_WDMA_INT_MASK), reg);
		ret = 0;
		break;
	default:
		(void)pr_err("[ERR][VIN_WRAP_INTR] %s: id(%d) is wrong.\n",
			     __func__, id);
		ret = -1;
		break;
	}

FUNC_EXIT:
	return ret;
}

void vin_wrap_intr_init(void)
{
	VinWrapIntr_np =
		of_find_compatible_node(NULL, NULL, "telechips,vin_wrap_intr");

	if (VinWrapIntr_np == NULL) {
		(void)pr_info(
			"[INF][VIN_WRAP_INTR] disabled [this is mandatory for vin wrapper]\n");
	} else {
		int i = 0;
		unsigned int temp = 0; /* avoid CERT-C Integers Rule INT31-C */
		for (i = 0; i < (int)VIN_IRQI_MAC; i++) {
			temp = irq_of_parse_and_map(VinWrapIntr_np, i);
			if (temp < (UINT_MAX / 2U)) {
				vin_wrap_base_irq_num[i] = (int)temp;
				(void)pr_info(
					"[INF][VIN_WRAP_INTR] vin-wrap-intr%d: irq %d\n",
					i, vin_wrap_base_irq_num[i]);
			}
		}
	}
}
