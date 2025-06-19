// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/io.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <video/telechips/tcc_types.h>
#include <video/telechips/vioc_intr.h>
#include <video/telechips/vioc_config.h>
#include <video/telechips/vioc_vin.h>
#include <video/telechips/vioc_rdma.h>
#include <video/telechips/vioc_wdma.h>
#include <video/telechips/vioc_disp.h>
#include <video/telechips/vioc_global.h>
#include <video/telechips/vioc_ddicfg.h> // is_VIOC_REMAP
#include <video/telechips/vioc_timer.h>

#if defined(CONFIG_ARCH_TCC805X)
#include <video/telechips/vioc_pvric_fbdc.h>
#endif

static int vioc_base_irq_num[4] = {
	0,
};

/* HIS_GOTO */
int vioc_intr_enable(int irq, int id, unsigned int mask)
{
	void __iomem *reg;
	int sub_id, vioc_id;
	unsigned int type_clr_offset;
	int ret = -1;

	if ((id < 0) || (id > VIOC_INTR_NUM)) {
		ret = -1;

		goto FUNC_EXIT;
	}

	switch (id) {
	case VIOC_INTR_DEV0:
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_DEV1:
#endif
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_DEV2:
#endif
#ifdef CONFIG_ARCH_TCC805X
	case VIOC_INTR_DEV3:
		if (id == VIOC_INTR_DEV3) {
			/* Prevent KCS warning */
			sub_id = VIOC_INTR_DEV3 - VIOC_INTR_DISP_OFFSET;
		} else {
			/* Prevent KCS warning */
			sub_id = id - VIOC_INTR_DEV0;
		}

		reg = VIOC_DISP_GetAddress((unsigned int)sub_id);

		__raw_writel(
			(__raw_readl(reg + DIM) & ~(mask & VIOC_DISP_INT_MASK)),
			reg + DIM);
		ret = 0;
		break;
#else
		sub_id = id - VIOC_INTR_DEV0;

		reg = VIOC_DISP_GetAddress((unsigned int)sub_id);

		__raw_writel(
			(__raw_readl(reg + DIM) & ~(mask & VIOC_DISP_INT_MASK)),
			reg + DIM);
		ret = 0;
		break;
#endif
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_RD7:
#endif
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
#endif
		/* clera irq status */
		sub_id = id - VIOC_INTR_RD0;

		reg = VIOC_RDMA_GetAddress((unsigned int)sub_id) + RDMASTAT;
		__raw_writel((mask & VIOC_RDMA_INT_MASK), reg);

		/* enable irq */
		reg = VIOC_RDMA_GetAddress((unsigned int)sub_id) + RDMAIRQMSK;
		__raw_writel(
			__raw_readl(reg) & ~(mask & VIOC_RDMA_INT_MASK), reg);
		ret = 0;
		break;
	case VIOC_INTR_WD0:
	case VIOC_INTR_WD1:
	case VIOC_INTR_WD2:
	case VIOC_INTR_WD3:
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
#endif
		vioc_id = (int)VIOC_WDMA00 + (id - VIOC_INTR_WD0);

		/* clera irq status */
		reg = VIOC_WDMA_GetAddress((unsigned int)vioc_id) + WDMAIRQSTS_OFFSET;
		__raw_writel((mask & VIOC_WDMA_INT_MASK), reg);

		/* enable irq */
		reg = VIOC_WDMA_GetAddress((unsigned int)vioc_id) + WDMAIRQMSK_OFFSET;
		__raw_writel(__raw_readl(reg) & ~(mask & VIOC_WDMA_INT_MASK), reg);

		ret = 0;
		break;

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD9:
	case VIOC_INTR_WD10:
	case VIOC_INTR_WD11:
	case VIOC_INTR_WD12:
		vioc_id = (int)VIOC_WDMA09 + (id - VIOC_INTR_WD9);

		/* clera irq status */
		reg = VIOC_WDMA_GetAddress((unsigned int)vioc_id) + WDMAIRQSTS_OFFSET;
		__raw_writel((mask & VIOC_WDMA_INT_MASK), reg);

		/* enable irq */
		reg = VIOC_WDMA_GetAddress((unsigned int)vioc_id) + WDMAIRQMSK_OFFSET;
		__raw_writel(__raw_readl(reg) & ~(mask & VIOC_WDMA_INT_MASK), reg);

		ret = 0;
		break;
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD13:
		vioc_id = VIOC_WDMA13 + (id - VIOC_INTR_WD13);

		/* clera irq status */
		reg = VIOC_WDMA_GetAddress((unsigned int)vioc_id) + WDMAIRQSTS_OFFSET;
		__raw_writel((mask & VIOC_WDMA_INT_MASK), reg);

		/* enable irq */
		reg = VIOC_WDMA_GetAddress((unsigned int)vioc_id) + WDMAIRQMSK_OFFSET;
		__raw_writel(__raw_readl(reg) & ~(mask & VIOC_WDMA_INT_MASK), reg);

		ret = 0;
		break;
#endif
	/*
	 * VIN_INT[31]: Not Used
	 * VIN_INT[19]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[18]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[17]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[16]: Enable interrupt if 1 / Disable interrupt if 0
	 */
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_VIN0:
	case VIOC_INTR_VIN1:
	case VIOC_INTR_VIN2:
	case VIOC_INTR_VIN3:

		sub_id = id - VIOC_INTR_VIN0;

		reg = VIOC_VIN_GetAddress((unsigned int)sub_id * 2U) + VIN_INT;
		/* clera irq status */
		__raw_writel((__raw_readl(reg) | (mask & VIOC_VIN_INT_MASK)), reg);
		/* enable irq */
		__raw_writel(
			__raw_readl(reg) | ((mask & VIOC_VIN_INT_ENABLE) << 16U),
			reg);
		ret = 0;
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN4:
	case VIOC_INTR_VIN5:
	case VIOC_INTR_VIN6:
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN7:
#endif // defined(CONFIG_ARCH_TCC805X)
		sub_id = id - VIOC_INTR_VIN_OFFSET
			- VIOC_INTR_VIN0;

		reg = VIOC_VIN_GetAddress((unsigned int)sub_id * 2U) + VIN_INT;

		/* clera irq status */
		__raw_writel((__raw_readl(reg) | (mask & VIOC_VIN_INT_MASK)), reg);
		/* enable irq */
		__raw_writel(
			__raw_readl(reg) | ((mask & VIOC_VIN_INT_ENABLE) << 16U),
			reg);
		ret = 0;
		break;
#endif // defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
#endif // !defined(CONFIG_ARCH_TCC750X)
#if defined(CONFIG_VIOC_PVRIC_FBDC)
	case VIOC_INTR_AFBCDEC0:
	case VIOC_INTR_AFBCDEC1:
		sub_id = id - VIOC_INTR_AFBCDEC0;

		reg = VIOC_PVRIC_FBDC_GetAddress((unsigned int)sub_id) + PVRICSTS;
		/* clera irq status */
		__raw_writel((__raw_readl(reg) | (mask & VIOC_PVRIC_FBDC_INT_MASK)),
			reg);
		/* enable irq */
		__raw_writel(
			__raw_readl(reg) & (~(mask & VIOC_PVRIC_FBDC_INT_MASK) << 16U), reg);
		ret = 0;
		break;
#endif // defined(CONFIG_VIOC_PVRIC_FBDC)
	case VIOC_INTR_TIMER:
#if 0 /* Using timer's own low api */
		/* clera irq status */
		reg = VIOC_TIMER_GetAddress() + IRQSTAT;
		__raw_writel((mask & VIOC_TIMER_INT_MASK), reg);
		/* enable irq */
		reg = VIOC_TIMER_GetAddress() + IRQMASK;
		__raw_writel(
			__raw_readl(reg) & ~(mask & VIOC_TIMER_INT_MASK), reg);
#endif
		ret = 0;
		break;
	default:
		(void)pr_err("[ERR][VIOC_INTR] %s: id(%d) is wrong.\n",
		       __func__, id);
		ret = -1;
		break;
	}

	if (ret < 0) {
		goto FUNC_EXIT;
	}
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC750X)
	if (irq == vioc_base_irq_num[0]) {
		/* Prevent KCS warning */
		type_clr_offset = IRQMASKCLR0_0_OFFSET;
	} else if (irq == vioc_base_irq_num[1]) {
		/* Prevent KCS warning */
		type_clr_offset = IRQMASKCLR1_0_OFFSET;
	} else if (irq == vioc_base_irq_num[2]) {
		/* Prevent KCS warning */
		type_clr_offset = IRQMASKCLR2_0_OFFSET;
	} else if (irq == vioc_base_irq_num[3]) {
		/* Prevent KCS warning */
		type_clr_offset = IRQMASKCLR3_0_OFFSET;
	} else {
		(void)pr_err("[ERR][VIOC_INTR] %s-%d :: irq(%d) is wierd.\n",
		       __func__, __LINE__, irq);
		ret = -1;
	}
#else
	type_clr_offset = IRQMASKCLR0_0_OFFSET;
	ret = 0;
#endif
	if (ret < 0) {

		goto FUNC_EXIT;
	}
	reg = VIOC_IREQConfig_GetAddress();

#if defined(CONFIG_ARCH_TCC897X)
	if (id < 32) {
		__raw_writel((u32)1U << (u32)id, reg + type_clr_offset);
	} else {
		__raw_writel((u32)1U << ((u32)id - 32U), reg + type_clr_offset + 0x4);
	}
	ret = 0;
#elif defined(CONFIG_ARCH_TCC750X)
	if (id < 32) {
		__raw_writel((u32)1U << (u32)id, reg + type_clr_offset);
	}
	else if (id < 64) {
		__raw_writel((u32)1U << ((u32)id - 32U), reg + type_clr_offset + 0x4U);
	}
	else if (id < 96) {
		__raw_writel((u32)1U << ((u32)id - 64U), reg + type_clr_offset + 0x8U);
	}
	else {
		__raw_writel((u32)1U << ((u32)id - 96U), reg + type_clr_offset + 0xCU);
	}
	ret = 0;
#else
	if (id < 32 ) {
		__raw_writel((u32)1U << (u32)id, reg + type_clr_offset);
	}
	else if (id < 64) {
		__raw_writel((u32)1U << ((u32)id - 32U), reg + type_clr_offset + 0x4U);
	}
	else {
		__raw_writel((u32)1U << ((u32)id - 64U), reg + type_clr_offset + 0x8U);
	}
	ret = 0;
#endif

FUNC_EXIT:
	return ret;
}
EXPORT_SYMBOL(vioc_intr_enable);

/* HIS_GOTO */ /* HIS_CALLS */
int vioc_intr_disable(int irq, int id, unsigned int mask)
{
	void __iomem *reg;
	int sub_id, vioc_id;
	unsigned int do_irq_mask = 1U;
	unsigned int type_set_offset;
	int ret = -1;

	if ((id < 0) || (id > VIOC_INTR_NUM)) {
		ret = -1;

		goto FUNC_EXIT;
	}

	switch (id) {
	case VIOC_INTR_DEV0:
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_DEV1:
#endif
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_DEV2:
#endif
#ifdef CONFIG_ARCH_TCC805X
	case VIOC_INTR_DEV3:
		if (id == VIOC_INTR_DEV3) {
			/* Prevent KCS warning */
			sub_id = VIOC_INTR_DEV3 - VIOC_INTR_DISP_OFFSET;
		}
		else {
			/* Prevent KCS warning */
			sub_id = id - VIOC_INTR_DEV0;
		}

		reg = VIOC_DISP_GetAddress((unsigned int)sub_id) + DIM;
		__raw_writel(
			__raw_readl(reg) | (mask & VIOC_DISP_INT_MASK), reg);
		if ((__raw_readl(reg) & VIOC_DISP_INT_MASK)
		    != VIOC_DISP_INT_MASK) {
		    /* Prevent KCS warning */
			do_irq_mask = 0U;
		}
		ret = 0;
		break;
#else
		sub_id = id - VIOC_INTR_DEV0;

		reg = VIOC_DISP_GetAddress((unsigned int)sub_id) + DIM;
		__raw_writel(
			__raw_readl(reg) | (mask & VIOC_DISP_INT_MASK), reg);
		if ((__raw_readl(reg) & VIOC_DISP_INT_MASK)
		    != VIOC_DISP_INT_MASK) {
		    /* Prevent KCS warning */
			do_irq_mask = 0U;
		}
		ret = 0;
		break;
#endif
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_RD7:
#endif
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
#endif
		sub_id = id - VIOC_INTR_RD0;

		reg = VIOC_RDMA_GetAddress((unsigned int)sub_id) + RDMAIRQMSK;

		__raw_writel(
			__raw_readl(reg) | (mask & VIOC_RDMA_INT_MASK), reg);
		if ((__raw_readl(reg) & VIOC_RDMA_INT_MASK)
		    != VIOC_RDMA_INT_MASK) {
		    /* Prevent KCS warning */
			do_irq_mask = 0U;
		}
		ret = 0;
		break;
	case VIOC_INTR_WD0:
	case VIOC_INTR_WD1:
	case VIOC_INTR_WD2:
	case VIOC_INTR_WD3:
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
#endif
		vioc_id = (int)VIOC_WDMA00 + (id - VIOC_INTR_WD0);

		reg = VIOC_WDMA_GetAddress((unsigned int)vioc_id) + WDMAIRQMSK_OFFSET;

		__raw_writel(__raw_readl(reg) | (mask & VIOC_WDMA_INT_MASK), reg);
		if ((__raw_readl(reg) & VIOC_WDMA_INT_MASK) != VIOC_WDMA_INT_MASK) {
		    /* Prevent KCS warning */
			do_irq_mask = 0U;
		}
		ret = 0;
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD9:
	case VIOC_INTR_WD10:
	case VIOC_INTR_WD11:
	case VIOC_INTR_WD12:
		vioc_id = (int)VIOC_WDMA09 + (id - VIOC_INTR_WD9);

		reg = VIOC_WDMA_GetAddress((unsigned int)vioc_id) + WDMAIRQMSK_OFFSET;
		__raw_writel(
			__raw_readl(reg) | (mask & VIOC_WDMA_INT_MASK), reg);
		if ((__raw_readl(reg) & VIOC_WDMA_INT_MASK)
		    != VIOC_WDMA_INT_MASK) {
		    /* Prevent KCS warning */
			do_irq_mask = 0U;
		}
		ret = 0;
		break;
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD13:
		vioc_id = VIOC_WDMA13 + (id - VIOC_INTR_WD13);

		reg = VIOC_WDMA_GetAddress((unsigned int)vioc_id) + WDMAIRQMSK_OFFSET;
		__raw_writel(
			__raw_readl(reg) | (mask & VIOC_WDMA_INT_MASK), reg);
		if ((__raw_readl(reg) & VIOC_WDMA_INT_MASK)
		    != VIOC_WDMA_INT_MASK) {
		    /* Prevent KCS warning */
			do_irq_mask = 0U;
		}
		ret = 0;
		break;
#endif

	/*
	 * VIN_INT[31]: Not Used
	 * VIN_INT[19]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[18]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[17]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[16]: Enable interrupt if 1 / Disable interrupt if 0
	 */
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_VIN0:
	case VIOC_INTR_VIN1:
	case VIOC_INTR_VIN2:
	case VIOC_INTR_VIN3:
		sub_id = id - VIOC_INTR_VIN0;

		reg = VIOC_VIN_GetAddress((unsigned int)sub_id * 2U) + VIN_INT;

		__raw_writel(
			__raw_readl(reg) & ~((mask & VIOC_VIN_INT_ENABLE) << 16),
			reg);
		//if ((__raw_readl(reg) & VIOC_VIN_INT_MASK) !=
		// VIOC_VIN_INT_MASK) do_irq_mask = 0;
		ret = 0;
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN4:
	case VIOC_INTR_VIN5:
	case VIOC_INTR_VIN6:
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN7:
#endif // defined(CONFIG_ARCH_TCC805X)
		sub_id = id - VIOC_INTR_VIN_OFFSET
			- VIOC_INTR_VIN0;

		reg = VIOC_VIN_GetAddress((unsigned int)sub_id * 2U) + VIN_INT;
		__raw_writel(
			__raw_readl(reg) & ~((mask & VIOC_VIN_INT_ENABLE) << 16U),
			reg);
		//if ((__raw_readl(reg) & VIOC_VIN_INT_MASK) !=
		//VIOC_VIN_INT_MASK) do_irq_mask = 0;
		ret = 0;
		break;
#endif // defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
#endif // !defined(CONFIG_ARCH_TCC750X)
#if defined(CONFIG_VIOC_PVRIC_FBDC)
	case VIOC_INTR_AFBCDEC0:
	case VIOC_INTR_AFBCDEC1:
		sub_id = id - VIOC_INTR_AFBCDEC0;

		reg = VIOC_PVRIC_FBDC_GetAddress((unsigned int)sub_id) + PVRICSTS;
		__raw_writel(__raw_readl(reg)
			& ((mask & VIOC_PVRIC_FBDC_INT_MASK) << 16U), reg);
		//if ((__raw_readl(reg) & (VIOC_PVRIC_FBDC_INT_MASK << 16U))
		//	!= (VIOC_PVRIC_FBDC_INT_MASK << 16))
		//	do_irq_mask = 0;
		ret = 0;
		break;
#endif // defined(CONFIG_VIOC_PVRIC_FBDC)
	case VIOC_INTR_TIMER:
#if 0 /* Using timer's own low api */

		reg = VIOC_TIMER_GetAddress() + IRQMASK;
		__raw_writel(
			__raw_readl(reg) | (mask & VIOC_TIMER_INT_MASK), reg);
		if ((__raw_readl(reg) & VIOC_TIMER_INT_MASK)
		    != VIOC_TIMER_INT_MASK) {
		    /* Prevent KCS warning */
			do_irq_mask = 0U;
		}
#else
		do_irq_mask = 1U;
#endif
		ret = 0;
		break;
	default:
		(void)pr_err("[ERR][VIOC_INTR] %s: id(%d) is wrong.\n",
			__func__, id);
		ret = 0;
		break;
	}

	if (do_irq_mask == 1U) {
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC750X)
		if (irq == vioc_base_irq_num[0]) {
			/* Prevent KCS warning */
			type_set_offset = IRQMASKSET0_0_OFFSET;
		} else if (irq == vioc_base_irq_num[1]) {
			/* Prevent KCS warning */
			type_set_offset = IRQMASKSET1_0_OFFSET;
		} else if (irq == vioc_base_irq_num[2]) {
			/* Prevent KCS warning */
			type_set_offset = IRQMASKSET2_0_OFFSET;
		} else if (irq == vioc_base_irq_num[3]) {
			/* Prevent KCS warning */
			type_set_offset = IRQMASKSET3_0_OFFSET;
		} else {
			(void)pr_err("[ERR][VIOC_INTR] %s-%d :: irq(%d) is wierd.\n",
			       __func__, __LINE__, irq);
			ret = -1;
		}
#else
		type_set_offset = IRQMASKSET0_0_OFFSET;
		ret = 0;
#endif
		if (ret < 0) {

			goto FUNC_EXIT;
		}
		reg = VIOC_IREQConfig_GetAddress();
#if defined(CONFIG_ARCH_TCC897X)
		if (id < 32) {
			__raw_writel((u32)1U << (u32)id, reg + type_set_offset);
		} else {
			__raw_writel(
				(u32)1U << ((u32)id - 32U), reg + type_set_offset + 0x4);
		}
#elif defined(CONFIG_ARCH_TCC750X)
		if (id >= 96) {
			__raw_writel((u32)1U << ((u32)id - 96U), reg + type_set_offset + 0xCU);
		} else if (id >= 64) {
			__raw_writel((u32)1U << ((u32)id - 64U), reg + type_set_offset + 0x8U);
		} else if (id >= 32) {
			__raw_writel((u32)1U << ((u32)id - 32U), reg + type_set_offset + 0x4U);
		} else {
			__raw_writel((u32)1U << (u32)id, reg + type_set_offset);
		}
		ret = 0;
#else
		if (id >= 64) {
			__raw_writel(
				(u32)1U << ((u32)id - 64U), reg + type_set_offset + 0x8U);
		} else if (id >= 32) {
			__raw_writel(
				(u32)1U << ((u32)id - 32U), reg + type_set_offset + 0x4U);
		} else {
			__raw_writel((u32)1U << (u32)id, reg + type_set_offset);
		}
#endif
	}
	ret = 0;

FUNC_EXIT:
	return ret;
}
EXPORT_SYMBOL(vioc_intr_disable);

/* HIS_GOTO */
unsigned int vioc_intr_get_status(int id)
{
	const void __iomem *reg;
	int vioc_id;
	unsigned int ret = 0U;

	if ((id < 0) || (id > VIOC_INTR_NUM)) {
		ret = 0U;

		goto FUNC_EXIT;
	}

	switch (id) {
	case VIOC_INTR_DEV0:
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_DEV1:
#endif
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_DEV2:
#endif
#ifdef CONFIG_ARCH_TCC805X
	case VIOC_INTR_DEV3:
		if (id == VIOC_INTR_DEV3) {
			/* Prevent KCS warning */
			id -= VIOC_INTR_DISP_OFFSET;
		}

		reg = VIOC_DISP_GetAddress((unsigned int)id) + DSTATUS;

		ret = (__raw_readl(reg) & VIOC_DISP_INT_MASK);
		break;
#else

		reg = VIOC_DISP_GetAddress((unsigned int)id) + DSTATUS;

		ret = (__raw_readl(reg) & VIOC_DISP_INT_MASK);
		break;
#endif
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_RD7:
#endif
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
#endif
		id -= VIOC_INTR_RD0;

		reg = VIOC_RDMA_GetAddress((unsigned int)id) + RDMASTAT;

		ret = (__raw_readl(reg) & VIOC_RDMA_INT_MASK);
		break;
	case VIOC_INTR_WD0:
	case VIOC_INTR_WD1:
	case VIOC_INTR_WD2:
	case VIOC_INTR_WD3:
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
#endif
		vioc_id = (int)VIOC_WDMA00 + (id - VIOC_INTR_WD0);

		reg = VIOC_WDMA_GetAddress((unsigned int)vioc_id) + WDMAIRQSTS_OFFSET;

		ret = (__raw_readl(reg) & VIOC_WDMA_INT_MASK);
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD9:
	case VIOC_INTR_WD10:
	case VIOC_INTR_WD11:
	case VIOC_INTR_WD12:
		vioc_id = (int)VIOC_WDMA09 + (id - VIOC_INTR_WD9);

		reg = VIOC_WDMA_GetAddress((unsigned int)vioc_id) + WDMAIRQSTS_OFFSET;

		ret = (__raw_readl(reg) & VIOC_WDMA_INT_MASK);
		break;
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD13:
		vioc_id = VIOC_WDMA13 + (id - VIOC_INTR_WD13);

		reg = VIOC_WDMA_GetAddress((unsigned int)vioc_id) + WDMAIRQSTS_OFFSET;

		ret = (__raw_readl(reg) & VIOC_WDMA_INT_MASK);
		break;
#endif
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_VIN0:
	case VIOC_INTR_VIN1:
	case VIOC_INTR_VIN2:
	case VIOC_INTR_VIN3:
		id -= VIOC_INTR_VIN0;

		reg = VIOC_VIN_GetAddress((unsigned int)id * 2U) + VIN_INT;

		ret = (__raw_readl(reg) & VIOC_VIN_INT_MASK);
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN4:
	case VIOC_INTR_VIN5:
	case VIOC_INTR_VIN6:
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN7:
#endif // defined(CONFIG_ARCH_TCC805X)
		id -= (VIOC_INTR_VIN_OFFSET + VIOC_INTR_VIN0);

		reg = VIOC_VIN_GetAddress((unsigned int)id * 2U) + VIN_INT;

		ret = (__raw_readl(reg) & VIOC_VIN_INT_MASK);
		break;
#endif // defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
#endif //!defined(CONFIG_ARCH_TCC750X)
#if defined(CONFIG_VIOC_PVRIC_FBDC)
	case VIOC_INTR_AFBCDEC0:
	case VIOC_INTR_AFBCDEC1:
		id -= VIOC_INTR_AFBCDEC0;

		reg = VIOC_PVRIC_FBDC_GetAddress((unsigned int)id) + PVRICSTS;

		ret = (__raw_readl(reg) & VIOC_PVRIC_FBDC_INT_MASK);
		break;
#endif // defined(CONFIG_VIOC_PVRIC_FBDC)
	case VIOC_INTR_TIMER:
#if 0 /* Using timer's own low api */

		reg = VIOC_TIMER_GetAddress() + IRQSTAT;

		ret = (__raw_readl(reg) & VIOC_TIMER_INT_MASK);
#endif
		break;
	default:
		(void)pr_err("[ERR][VIOC_INTR] %s: id(%d) is wrong.\n",
			   __func__, id);
		ret = 0U;
		break;
	}
FUNC_EXIT:
	return ret;
}
EXPORT_SYMBOL(vioc_intr_get_status);

bool check_vioc_irq_status(const void __iomem *reg, int id)
{
	unsigned int flag;
	bool ret = (bool)false;

	if ((id < 0) || (id > VIOC_INTR_NUM)) {
		ret = (bool)false;
	} else {
		if (id < 32) {

/* if you change the VIOC interrupt, you need to modify the IRQSELECTn_n offset and IRQMASKCLRn_n part below. */
/* for more detail, when you use VIOC1_SUB_IRQI in subcore or VIOC1_IRQI in maincore, */
/* you need to changed it to IRQSELECT1_0_OFFSET, IRQSELECT1_1_OFFSET, IRQSELECT1_2_OFFSET and IRQMASKCLRn_n_OFFSET should be applied the same.*/
#if defined(CONFIG_TCC803X_CA7S) || defined(CONFIG_TCC805X_CA53Q)
			flag = ((__raw_readl(reg + IRQSELECT0_0_OFFSET) & ((u32)1U << (unsigned int)id)) != 0U) ?
#else
			flag = ((__raw_readl(reg + IRQSELECT1_0_OFFSET) & ((u32)1U << (unsigned int)id)) != 0U) ?
#endif
				(__raw_readl(reg + SYNCSTATUS0_OFFSET) & ((u32)1U << (unsigned int)id)) :
				(__raw_readl(reg + RAWSTATUS0_OFFSET) & ((u32)1U << (unsigned int)id));
		} else if (id < 64) {

#if defined(CONFIG_TCC803X_CA7S) || defined(CONFIG_TCC805X_CA53Q)
			flag = ((__raw_readl(reg + IRQMASKCLR0_1_OFFSET)
				& ((u32)1U << ((unsigned int)id - 32U))) != 0U) ?
#else
			flag = ((__raw_readl(reg + IRQMASKCLR1_1_OFFSET)
				& ((u32)1U << ((unsigned int)id - 32U))) != 0U) ?
#endif
				(__raw_readl(reg + SYNCSTATUS1_OFFSET)
				& ((u32)1U << ((unsigned int)id - 32U))) :
				(__raw_readl(reg + RAWSTATUS1_OFFSET)
				& ((u32)1U << ((unsigned int)id - 32U)));
		}
#if defined(CONFIG_ARCH_TCC750X)
		else if (id < 96){
			flag = ((__raw_readl(reg + IRQMASKCLR0_2_OFFSET)
				& ((u32)1U << ((unsigned int)id - 64U))) != 0U) ?
				(__raw_readl(reg + SYNCSTATUS2_OFFSET)
				& ((u32)1U << ((unsigned int)id - 64U))) :
				(__raw_readl(reg + RAWSTATUS2_OFFSET)
				& ((u32)1U << ((unsigned int)id - 64U)));
		} else {
			flag = ((__raw_readl(reg + IRQMASKCLR0_3_OFFSET)
				& ((u32)1U << ((unsigned int)id - 96U))) != 0U) ?
				(__raw_readl(reg + SYNCSTATUS3_OFFSET)
				& ((u32)1U << ((unsigned int)id - 96U))) :
				(__raw_readl(reg + RAWSTATUS3_OFFSET)
				& ((u32)1U << ((unsigned int)id - 96U)));
		}
#elif !defined(CONFIG_ARCH_TCC897X)
		else {
#if defined(CONFIG_TCC803X_CA7S) || defined(CONFIG_TCC805X_CA53Q)
			flag = ((__raw_readl(reg + IRQMASKCLR0_2_OFFSET)
				& ((u32)1U << ((unsigned int)id - 64U))) != 0U) ?
#else
			flag = ((__raw_readl(reg + IRQMASKCLR1_2_OFFSET)
				& ((u32)1U << ((unsigned int)id - 32U))) != 0U) ?
#endif
				(__raw_readl(reg + SYNCSTATUS2_OFFSET)
				& ((u32)1U << ((unsigned int)id - 64U))) :
				(__raw_readl(reg + RAWSTATUS2_OFFSET)
				& ((u32)1U << ((unsigned int)id - 64U)));
		}
#elif defined(CONFIG_ARCH_TCC897X)
		else {
			/* avoid MISRA C-2012 Rule 15.7 */
		}
#endif

		if (flag != 0U) {
			ret = (bool)true;
		} else {
			ret = (bool)false;
		}
	}
	return ret;
}
EXPORT_SYMBOL(check_vioc_irq_status);

/* HIS_GOTO */
bool is_vioc_intr_activatied(int id, unsigned int mask)
{
	const void __iomem *reg;
	int vioc_id;
	bool ret = (bool)false;

	if ((id < 0) || (id > VIOC_INTR_NUM)) {
		ret = (bool)false;

		goto FUNC_EXIT;
	}

	switch (id) {
	case VIOC_INTR_DEV0:
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_DEV1:
#endif
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_DEV2:
#endif
#ifdef CONFIG_ARCH_TCC805X
	case VIOC_INTR_DEV3:
		if (id == VIOC_INTR_DEV3) {
			/* Prevent KCS warning */
			id -= VIOC_INTR_DISP_OFFSET;
		}

		reg = VIOC_DISP_GetAddress((unsigned int)id);
		if ((__raw_readl(reg + DSTATUS) & (mask & VIOC_DISP_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
#else
		reg = VIOC_DISP_GetAddress((unsigned int)id);
		if ((__raw_readl(reg + DSTATUS) & (mask & VIOC_DISP_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
#endif
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_RD7:
#endif
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
#endif
		id -= VIOC_INTR_RD0;

		reg = VIOC_RDMA_GetAddress((unsigned int)id) + RDMASTAT;

		if ((__raw_readl(reg) & (mask & VIOC_RDMA_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
	case VIOC_INTR_WD0:
	case VIOC_INTR_WD1:
	case VIOC_INTR_WD2:
	case VIOC_INTR_WD3:
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
#endif
		vioc_id = (int)VIOC_WDMA00 + (id - VIOC_INTR_WD0);

		reg = VIOC_WDMA_GetAddress((unsigned int)vioc_id) + WDMAIRQSTS_OFFSET;

		if ((__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD9:
	case VIOC_INTR_WD10:
	case VIOC_INTR_WD11:
	case VIOC_INTR_WD12:
		vioc_id = (int)VIOC_WDMA09 + (id - VIOC_INTR_WD9);

		reg = VIOC_WDMA_GetAddress((unsigned int)vioc_id) + WDMAIRQSTS_OFFSET;

		if ((__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD13:
		vioc_id = VIOC_WDMA13 + (id - VIOC_INTR_WD13);

		reg = VIOC_WDMA_GetAddress((unsigned int)vioc_id) + WDMAIRQSTS_OFFSET;

		if ((__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
#endif
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_VIN0:
	case VIOC_INTR_VIN1:
	case VIOC_INTR_VIN2:
	case VIOC_INTR_VIN3:
		id -= VIOC_INTR_VIN0;

		reg = VIOC_VIN_GetAddress((unsigned int)id * 2U) + VIN_INT;

		if ((__raw_readl(reg) & (mask & VIOC_VIN_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN4:
	case VIOC_INTR_VIN5:
	case VIOC_INTR_VIN6:
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN7:
#endif // defined(CONFIG_ARCH_TCC805X)
		id -= (VIOC_INTR_VIN_OFFSET + VIOC_INTR_VIN0);

		reg = VIOC_VIN_GetAddress((unsigned int)id * 2U) + VIN_INT;

		if ((__raw_readl(reg) & (mask & VIOC_VIN_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			ret = (bool)false;
		}
		break;
#endif //defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
#endif // !defined(CONFIG_ARCH_TCC750X)
#if defined(CONFIG_VIOC_PVRIC_FBDC)
	case VIOC_INTR_AFBCDEC0:
	case VIOC_INTR_AFBCDEC1:
		id -= VIOC_INTR_AFBCDEC0;

		reg = VIOC_PVRIC_FBDC_GetAddress((unsigned int)id) + PVRICSTS;

		if ((__raw_readl(reg) & (mask & VIOC_PVRIC_FBDC_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			ret = (bool)false;
		}
		break;
#endif // defined(CONFIG_VIOC_PVRIC_FBDC)
	case VIOC_INTR_TIMER:
#if 0 /* Using timer's own low api */

		reg = VIOC_TIMER_GetAddress() + IRQSTAT;

		if ((__raw_readl(reg) & (mask & VIOC_TIMER_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
#endif
		break;
	default:
		(void)pr_err("[ERR][VIOC_INTR] %s: id(%d) is wrong.\n",
			   __func__, id);
		ret = (bool)false;
		break;
	}
FUNC_EXIT:
	return ret;
}
EXPORT_SYMBOL(is_vioc_intr_activatied);

/* HIS_GOTO */
bool is_vioc_intr_unmasked(int id, unsigned int mask)
{
	const void __iomem *reg;
	int vioc_id;
	bool ret = (bool)false;

	if ((id < 0) || (id > VIOC_INTR_NUM)) {
		ret = (bool)false;

		goto FUNC_EXIT;
	}

	switch (id) {
	case VIOC_INTR_DEV0:
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_DEV1:
#endif
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_DEV2:
#endif
#ifdef CONFIG_ARCH_TCC805X
	case VIOC_INTR_DEV3:
		if (id == VIOC_INTR_DEV3) {
			/* Prevent KCS warning */
			id -= VIOC_INTR_DISP_OFFSET;
		}

		reg = VIOC_DISP_GetAddress((unsigned int)id);

		if ((__raw_readl(reg + DIM) & (mask & VIOC_DISP_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)false;
		} else {
			/* Prevent KCS warning */
			ret = (bool)true;
		}
		break;
#else
		reg = VIOC_DISP_GetAddress((unsigned int)id);

		if ((__raw_readl(reg + DIM) & (mask & VIOC_DISP_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)false;
		} else {
			/* Prevent KCS warning */
			ret = (bool)true;
		}
		break;
#endif
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_RD7:
#endif
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
#endif
		id -= VIOC_INTR_RD0;

		reg = VIOC_RDMA_GetAddress((unsigned int)id) + RDMAIRQMSK;

		if ((__raw_readl(reg) & (mask & VIOC_RDMA_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)false;
		} else {
			/* Prevent KCS warning */
			ret = (bool)true;
		}
		break;
	case VIOC_INTR_WD0:
	case VIOC_INTR_WD1:
	case VIOC_INTR_WD2:
	case VIOC_INTR_WD3:
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
#endif
		vioc_id = (int)VIOC_WDMA00 + (id - VIOC_INTR_WD0);

		reg = VIOC_WDMA_GetAddress((unsigned int)vioc_id) + WDMAIRQMSK_OFFSET;

		if ((__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)false;
		} else {
			/* Prevent KCS warning */
			ret = (bool)true;
		}
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD9:
	case VIOC_INTR_WD10:
	case VIOC_INTR_WD11:
	case VIOC_INTR_WD12:
		vioc_id = (int)VIOC_WDMA09 + (id - VIOC_INTR_WD9);

		reg = VIOC_WDMA_GetAddress((unsigned int)vioc_id) + WDMAIRQMSK_OFFSET;

		if ((__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)false;
		} else {
			/* Prevent KCS warning */
			ret = (bool)true;
		}
		break;
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD13:
		vioc_id = VIOC_WDMA13 + (id - VIOC_INTR_WD13);

		reg = VIOC_WDMA_GetAddress((unsigned int)vioc_id) + WDMAIRQMSK_OFFSET;

		if ((__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)false;
		} else {
			/* Prevent KCS warning */
			ret = (bool)true;
		}
		break;
#endif
	/*
	 * VIN_INT[31]: Not Used
	 * VIN_INT[19]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[18]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[17]: Enable interrupt if 1 / Disable interrupt if 0
	 * VIN_INT[16]: Enable interrupt if 1 / Disable interrupt if 0
	 */
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_VIN0:
	case VIOC_INTR_VIN1:
	case VIOC_INTR_VIN2:
	case VIOC_INTR_VIN3:
		id -= VIOC_INTR_VIN0;

		reg = VIOC_VIN_GetAddress((unsigned int)id * 2U) + VIN_INT;

		if ((__raw_readl(reg) & ((mask & VIOC_VIN_INT_ENABLE) << 16U)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN4:
	case VIOC_INTR_VIN5:
	case VIOC_INTR_VIN6:
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN7:
#endif // defined(CONFIG_ARCH_TCC805X)
		id -= (VIOC_INTR_VIN_OFFSET + VIOC_INTR_VIN0);

		reg = VIOC_VIN_GetAddress((unsigned int)id * 2U) + VIN_INT;

		if ((__raw_readl(reg) & ((mask & VIOC_VIN_INT_ENABLE) << 16U)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
#endif // defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
#endif // !defined(CONFIG_ARCH_TCC750X)
#if defined(CONFIG_VIOC_PVRIC_FBDC)
	case VIOC_INTR_AFBCDEC0:
	case VIOC_INTR_AFBCDEC1:
		id -= VIOC_INTR_AFBCDEC0;

		reg = VIOC_PVRIC_FBDC_GetAddress((unsigned int)id) + PVRICSTS;

		if ((__raw_readl(reg) & ((mask & VIOC_PVRIC_FBDC_INT_MASK) << 16U)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)false;
		} else {
			/* Prevent KCS warning */
			ret = (bool)true;
		}
		break;
#endif // defined(CONFIG_VIOC_PVRIC_FBDC)
	case VIOC_INTR_TIMER:
#if 0 /* Using timer's own low api */

		reg = VIOC_TIMER_GetAddress() + IRQMASK;

		if ((__raw_readl(reg) & (mask & VIOC_TIMER_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)false;
		} else {
			/* Prevent KCS warning */
			ret = (bool)true;
		}
#endif
		break;
	default:
		(void)pr_err("[ERR][VIOC_INTR] %s: id(%d) is wrong.\n",
			   __func__, id);
		ret = (bool)false;
		break;
	}
FUNC_EXIT:
	return ret;
}
EXPORT_SYMBOL(is_vioc_intr_unmasked);

/* HIS_GOTO */

bool is_vioc_display_device_intr_masked(int id, unsigned int mask)
{
	const void __iomem *reg;
	bool ret = (bool)false;
	u32 reg_val;

	if (id < 0) {
		ret = (bool)true;

		goto FUNC_EXIT;
	} else {
		if (get_vioc_type((unsigned int)id) != get_vioc_type(VIOC_DISP)) {
			ret = (bool)true;

			goto FUNC_EXIT;
		}
	}

	switch (get_vioc_index((unsigned int)id)) {
	case get_vioc_index(VIOC_DISP0):
	#if defined(VIOC_DISP1)
	case get_vioc_index(VIOC_DISP1):
	#endif
	#if defined(VIOC_DISP2)
	case get_vioc_index(VIOC_DISP2):
	#endif
	#if defined(VIOC_DISP3)
	case get_vioc_index(VIOC_DISP3):
	#endif
		ret = (bool)false;
		break;
	default:
		ret = (bool)true;
		break;
	}
	if (ret == (bool)true) {

		goto FUNC_EXIT;
	}
	reg = (void __iomem *)VIOC_DISP_GetAddress((unsigned int)id);
	if (reg == NULL) {
		ret = (bool)true;

		goto FUNC_EXIT;
	}

	reg_val = __raw_readl(reg + DIM);
	if (((reg_val & mask) & VIOC_DISP_INT_MASK) != 0U) {
		ret = (bool)true;
	} else {
		ret = (bool)false;
	}
FUNC_EXIT:
	return ret;
}
EXPORT_SYMBOL(is_vioc_display_device_intr_masked);

/* HIS_GOTO */ /* HIS_CALLS */

int vioc_intr_clear(int id, unsigned int mask)
{
	void __iomem *reg;
	int vioc_id;
	int ret = -1;

	if ((id < 0) || (id > VIOC_INTR_NUM)) {
		ret = -1;

		goto FUNC_EXIT;
	}

	switch (id) {
	case VIOC_INTR_DEV0:
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_DEV1:
#endif
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_DEV2:
#endif
#ifdef CONFIG_ARCH_TCC805X
	case VIOC_INTR_DEV3:
		if (id == VIOC_INTR_DEV3) {
			/* Prevent KCS warning */
			id -= VIOC_INTR_DISP_OFFSET;
		}

		reg = VIOC_DISP_GetAddress((unsigned int)id);

		__raw_writel((mask & VIOC_DISP_INT_MASK), reg + DSTATUS);
		ret = 0;
		break;
#else
		reg = VIOC_DISP_GetAddress((unsigned int)id);

		__raw_writel((mask & VIOC_DISP_INT_MASK), reg + DSTATUS);
		ret = 0;
		break;
#endif
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_RD7:
#endif
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
#endif
		id -= VIOC_INTR_RD0;

		reg = VIOC_RDMA_GetAddress((unsigned int)id) + RDMASTAT;

		__raw_writel((mask & VIOC_RDMA_INT_MASK), reg);
		ret = 0;
		break;
	case VIOC_INTR_WD0:
	case VIOC_INTR_WD1:
	case VIOC_INTR_WD2:
	case VIOC_INTR_WD3:
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
#endif
		vioc_id = (int)VIOC_WDMA00 + (id - VIOC_INTR_WD0);

		reg = VIOC_WDMA_GetAddress((unsigned int)vioc_id) + WDMAIRQSTS_OFFSET;

		__raw_writel((mask & VIOC_WDMA_INT_MASK), reg);
		ret = 0;
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD9:
	case VIOC_INTR_WD10:
	case VIOC_INTR_WD11:
	case VIOC_INTR_WD12:
		vioc_id = (int)VIOC_WDMA09 + (id - VIOC_INTR_WD9);

		reg = VIOC_WDMA_GetAddress((unsigned int)vioc_id) + WDMAIRQSTS_OFFSET;

		__raw_writel((mask & VIOC_WDMA_INT_MASK), reg);
		ret = 0;
		break;
#endif
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_WD13:
		vioc_id = VIOC_WDMA13 + (id - VIOC_INTR_WD13);

		reg = VIOC_WDMA_GetAddress((unsigned int)vioc_id) + WDMAIRQSTS_OFFSET;

		__raw_writel((mask & VIOC_WDMA_INT_MASK), reg);
		ret = 0;
		break;
#endif
#if !defined(CONFIG_ARCH_TCC750X)
	case VIOC_INTR_VIN0:
	case VIOC_INTR_VIN1:
	case VIOC_INTR_VIN2:
	case VIOC_INTR_VIN3:
		id -= VIOC_INTR_VIN0;

		reg = VIOC_VIN_GetAddress((unsigned int)id * 2U) + VIN_INT;

		__raw_writel((__raw_readl(reg) | (mask & VIOC_VIN_INT_MASK)), reg);
		ret = 0;
		break;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN4:
	case VIOC_INTR_VIN5:
	case VIOC_INTR_VIN6:
#if defined(CONFIG_ARCH_TCC805X)
	case VIOC_INTR_VIN7:
#endif // defined(CONFIG_ARCH_TCC805X)
		id -= (VIOC_INTR_VIN_OFFSET + VIOC_INTR_VIN0);

		reg = VIOC_VIN_GetAddress((unsigned int)id * 2U) + VIN_INT;

		__raw_writel((__raw_readl(reg) | (mask & VIOC_VIN_INT_MASK)), reg);
		ret = 0;
		break;
#endif // defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
#endif // !defined(CONFIG_ARCH_TCC750X)
#if defined(CONFIG_VIOC_PVRIC_FBDC)
	case VIOC_INTR_AFBCDEC0:
	case VIOC_INTR_AFBCDEC1:
		id -= VIOC_INTR_AFBCDEC0;

		reg = VIOC_PVRIC_FBDC_GetAddress((unsigned int)id) + PVRICSTS;

		__raw_writel((__raw_readl(reg) | (mask & VIOC_PVRIC_FBDC_INT_MASK)),
			reg);
		ret = 0;
		break;
#endif // defined(CONFIG_VIOC_PVRIC_FBDC)
	case VIOC_INTR_TIMER:
#if 0 /* Using timer's own low api */

		reg = VIOC_TIMER_GetAddress() + IRQSTAT;

		__raw_writel((mask & VIOC_TIMER_INT_MASK), reg);
#endif
		ret = 0;
		break;
	default:
	(void)pr_err("[ERR][VIOC_INTR] %s: id(%d) is wrong.\n",
		       __func__, id);
		ret = -1;
		break;
	}

FUNC_EXIT:
	return ret;
}
EXPORT_SYMBOL(vioc_intr_clear);

void vioc_intr_initialize(void)
{
	void __iomem *reg = VIOC_IREQConfig_GetAddress();
	int i;

/* if you change the VIOC interrupt, you need to modify the IRQMASKCLRn_n offset part below. */
/* for more detail, when you use VIOC1_SUB_IRQI in subcore or VIOC1_IRQI in maincore, */
/* you need to changed it to IRQMASKCLR1_0_OFFSET, IRQMASKCLR1_1_OFFSET, IRQMASKCLR1_2_OFFSET */

#if defined(CONFIG_TCC803X_CA7S) || defined(CONFIG_TCC805X_CA53Q)
	__raw_writel(0xffffffffu, reg + IRQMASKCLR0_0_OFFSET);
	__raw_writel(0xffffffffu, reg + IRQMASKCLR0_1_OFFSET);
#else 
	__raw_writel(0xffffffffu, reg + IRQMASKCLR1_0_OFFSET);
	__raw_writel(0xffffffffu, reg + IRQMASKCLR1_1_OFFSET);
#endif

#if !defined(CONFIG_ARCH_TCC897X)
#if defined(CONFIG_TCC803X_CA7S) || defined(CONFIG_TCC805X_CA53Q)
	__raw_writel(0xffffffffu, reg + IRQMASKCLR0_2_OFFSET);
#else 
	__raw_writel(0xffffffffu, reg + IRQMASKCLR1_2_OFFSET);
#endif
#endif
#if defined(CONFIG_ARCH_TCC750X)
	__raw_writel(0xffffffffu, reg + IRQMASKCLR0_3_OFFSET);
#endif

	/* disp irq mask & status clear */
#if defined(CONFIG_ARCH_TCC805X)
	for (i = 0;
	     i <= (VIOC_INTR_DEV3 - (VIOC_INTR_DISP_OFFSET + VIOC_INTR_DEV0));
	     i++) {
#elif defined(CONFIG_ARCH_TCC750X)
	for (i = 0; i <= (VIOC_INTR_DEV0 - VIOC_INTR_DEV0); i++) {
#else
	for (i = 0; i <= (VIOC_INTR_DEV2 - VIOC_INTR_DEV0); i++) {
#endif
		reg = VIOC_DISP_GetAddress((unsigned int)i);
		if (reg == NULL) {
			/* Prevent KCS warning */
			continue;
		}

		__raw_writel(VIOC_DISP_INT_MASK, reg + DIM);
		__raw_writel(VIOC_DISP_INT_MASK, reg + DSTATUS);
	}

	/* rdma irq mask & status clear */
#if !defined(CONFIG_ARCH_TCC750X)
	for (i = 0; i <= (VIOC_INTR_RD17 - VIOC_INTR_RD0); i++) {
#else
	for (i = 0; i <= (VIOC_INTR_RD6 - VIOC_INTR_RD0); i++) {
#endif
		reg = VIOC_RDMA_GetAddress((unsigned int)i);
		if (reg == NULL) {
			/* Prevent KCS warning */
			continue;
		}

		__raw_writel(VIOC_RDMA_INT_MASK, reg + RDMAIRQMSK);
		__raw_writel(VIOC_RDMA_INT_MASK, reg + RDMASTAT);
	}

		/* wdma irq mask & status clear */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
#if defined(CONFIG_ARCH_TCC805X)
	for (i = 0;
		i <= (
			VIOC_INTR_WD13 - (
				VIOC_INTR_WD_OFFSET + VIOC_INTR_WD_OFFSET2 +
				VIOC_INTR_WD0)); i++) {
#else
	for (i = 0;
	     i <= (VIOC_INTR_WD12 - (VIOC_INTR_WD_OFFSET + VIOC_INTR_WD0));
	     i++) {
#endif
		reg = VIOC_WDMA_GetAddress((unsigned int)i);
		if (reg == NULL) {
			/* Prevent KCS warning */
			continue;
		}

		__raw_writel(VIOC_WDMA_INT_MASK, reg + WDMAIRQMSK_OFFSET);
		__raw_writel(VIOC_WDMA_INT_MASK, reg + WDMAIRQSTS_OFFSET);
	}
#elif defined(CONFIG_ARCH_TCC750X)
	for (i = 0; i <= (VIOC_INTR_WD3 - VIOC_INTR_WD0); i++) {
		reg = VIOC_WDMA_GetAddress((unsigned int)i);
		if (reg == NULL) {
			continue;
		}

		__raw_writel(VIOC_WDMA_INT_MASK, reg + WDMAIRQMSK_OFFSET);
		__raw_writel(VIOC_WDMA_INT_MASK, reg + WDMAIRQSTS_OFFSET);
	}
#else
	for (i = 0; i <= (VIOC_INTR_WD8 - VIOC_INTR_WD0); i++) {
		reg = VIOC_WDMA_GetAddress((unsigned int)i);
		if (reg == NULL) {
			continue;
		}

		__raw_writel(VIOC_WDMA_INT_MASK, reg + WDMAIRQMSK_OFFSET);
		__raw_writel(VIOC_WDMA_INT_MASK, reg + WDMAIRQSTS_OFFSET);
	}
#endif

}
EXPORT_SYMBOL(vioc_intr_initialize);

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC803X)
static void vioc_intr_disable_core_intr(void)
{
	void __iomem *reg = VIOC_IREQConfig_GetAddress();

	/* if you change the VIOC interrupt, you need to modify the IRQMASKSETn_n offset part below. */
	/* for more detail, when you use VIOC1_SUB_IRQI in subcore or VIOC1_IRQI in maincore, */
	/* you need to changed it to IRQMASKET1_0_OFFSET, IRQMASKSET1_1_OFFSET, IRQMASKSET1_2_OFFSET */

	#if defined(CONFIG_TCC803X_CA7S) || defined(CONFIG_TCC805X_CA53Q)
	(void)pr_info("[INF][VIOC_INTR] disable all VIOC interrupts of VIOC0_IRQI\n");

	__raw_writel(0xffffffffu, reg + IRQMASKSET0_0_OFFSET);
	__raw_writel(0xffffffffu, reg + IRQMASKSET0_1_OFFSET);
	__raw_writel(0xffffffffu, reg + IRQMASKSET0_2_OFFSET);
	#else
	(void)pr_info("[INF][VIOC_INTR] disable all VIOC interrupts of VIOC1_IRQI\n");

	__raw_writel(0xffffffffu, reg + IRQMASKSET1_0_OFFSET);
	__raw_writel(0xffffffffu, reg + IRQMASKSET1_1_OFFSET);
	__raw_writel(0xffffffffu, reg + IRQMASKSET1_2_OFFSET);
	#endif
}
#endif

int vioc_intr_init(void)
{
	struct device_node *ViocIntr_np;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC750X)
	int i = 0;
#endif
	unsigned int temp = 0;/* avoid CERT-C Integers Rule INT31-C */

	ViocIntr_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_intr");

	if (ViocIntr_np == NULL) {
		(void)pr_info("[INF][VIOC_INTR] disabled [this is mandatory for vioc display]\n");
	} else {
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
		for (i = 0; i < (int)VIOC_IRQ_MAX; i++) {
			temp = irq_of_parse_and_map(ViocIntr_np, i);
			if (temp < (UINT_MAX / 2U)) {
  				vioc_base_irq_num[i] = (int)temp;
				(void)pr_info("[INF][VIOC_INTR] vioc-intr%d: irq %d\n", i,
					vioc_base_irq_num[i]);
			}
		}
#elif defined(CONFIG_ARCH_TCC750X)
		for (i = 0; i < (int)DDIBUS_LCD_IRQ_MAX; i++) {
			temp = irq_of_parse_and_map(ViocIntr_np, i);
			if (temp < (UINT_MAX / 2U)) {
  				vioc_base_irq_num[i] = (int)temp;
				(void)pr_info("[INF][VIOC_INTR] vioc-intr%d: irq %d\n", i,
					vioc_base_irq_num[i]);
			}
		}
#else
		temp = irq_of_parse_and_map(ViocIntr_np, 0);
		if (temp < (UINT_MAX / 2U)) {
			vioc_base_irq_num[0] = (int)temp;
			(void)pr_info("[INF][VIOC_INTR] vioc-intr%d : %d\n",
				0, vioc_base_irq_num[0]);
		}
#endif

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC803X)
	vioc_intr_disable_core_intr();
#endif
	}
	return 0;
}
EXPORT_SYMBOL(vioc_intr_init);