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

#if defined(CONFIG_VIOC_AFBCDEC)
#include <video/telechips/vioc_afbcdec.h>
#endif

static int vioc_base_irq_num[4] = {
	0,
};

/* HIS_GOTO */
int vioc_intr_enable(int irq, int id, unsigned int mask)
{
	void __iomem *reg;
	int sub_id;
	unsigned int type_clr_offset;
	int ret = -1;

	if ((id < 0) || (id > VIOC_INTR_NUM)) {
		ret = -1;

		goto FUNC_EXIT;
	}

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
	case VIOC_INTR_DEV2:
	case VIOC_INTR_DEV3:
	case VIOC_INTR_DEV4:
		sub_id = id - VIOC_INTR_DEV0;

		reg = VIOC_DISP_GetAddress((unsigned int)sub_id);
		__raw_writel(
			(__raw_readl(reg + DIM) & ~(mask & VIOC_DISP_INT_MASK)),
			reg + DIM);
		ret = 0;
		break;
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
	case VIOC_INTR_RD7:
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
	case VIOC_INTR_RD14:
	case VIOC_INTR_RD15:
	case VIOC_INTR_RD16:
	case VIOC_INTR_RD17:
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
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
		sub_id = id - VIOC_INTR_WD0;

		/* clera irq status */
		reg = VIOC_WDMA_GetAddress((unsigned int)sub_id) + WDMAIRQSTS_OFFSET;
		__raw_writel((mask & VIOC_WDMA_INT_MASK), reg);

		/* enable irq */
		reg = VIOC_WDMA_GetAddress((unsigned int)sub_id) + WDMAIRQMSK_OFFSET;
		__raw_writel(
			__raw_readl(reg) & ~(mask & VIOC_WDMA_INT_MASK), reg);
		ret = 0;
		break;
#if defined(CONFIG_VIOC_AFBCDEC)
	case VIOC_INTR_AFBCDEC0:
	case VIOC_INTR_AFBCDEC1:
		sub_id = id - VIOC_INTR_AFBCDEC0;

		reg = VIOC_AFBCDec_GetAddress((unsigned int)sub_id);
		/* clera irq status */
		__raw_writel((__raw_readl(reg + AFBCDEC_IRQ_CLEAR) | (mask & VIOC_AFBC_INT_MASK)),
			reg + AFBCDEC_IRQ_CLEAR);

		/* enable irq */
		__raw_writel(
			__raw_readl(reg + AFBCDEC_IRQ_MASK) & (mask & VIOC_AFBC_INT_MASK), reg + AFBCDEC_IRQ_MASK);
		ret = 0;
		break;
#endif // defined(CONFIG_VIOC_AFBCDEC)
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

	if (ret < 0) {
		goto FUNC_EXIT;
	}
	reg = VIOC_IREQConfig_GetAddress();

	if (id >= 64) {
		__raw_writel((u32)1U << ((unsigned int)id - 64U), reg + type_clr_offset + 0x8U);
	} else if (id >= 32) {
		__raw_writel((u32)1U << ((unsigned int)id - 32U), reg + type_clr_offset + 0x4U);
	} else {
		__raw_writel((u32)1U << (unsigned int)id, reg + type_clr_offset);
	}
	ret = 0;

FUNC_EXIT:
	return ret;
}
EXPORT_SYMBOL(vioc_intr_enable);

/* HIS_GOTO */ /* HIS_CALLS */
int vioc_intr_disable(int irq, int id, unsigned int mask)
{
	void __iomem *reg;
	int sub_id;
	unsigned int do_irq_mask = 1U;
	unsigned int type_set_offset;
	int ret = -1;

	if ((id < 0) || (id > VIOC_INTR_NUM)) {
		ret = -1;

		goto FUNC_EXIT;
	}

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
	case VIOC_INTR_DEV2:
	case VIOC_INTR_DEV3:
	case VIOC_INTR_DEV4:
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
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
	case VIOC_INTR_RD7:
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
	case VIOC_INTR_RD14:
	case VIOC_INTR_RD15:
	case VIOC_INTR_RD16:
	case VIOC_INTR_RD17:
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
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
		sub_id = id - VIOC_INTR_WD0;

		reg = VIOC_WDMA_GetAddress((unsigned int)sub_id) + WDMAIRQMSK_OFFSET;
		__raw_writel(
			__raw_readl(reg) | (mask & VIOC_WDMA_INT_MASK), reg);
		if ((__raw_readl(reg) & VIOC_WDMA_INT_MASK)
		    != VIOC_WDMA_INT_MASK) {
		    /* Prevent KCS warning */
			do_irq_mask = 0U;
		}
		ret = 0;
		break;
#if defined(CONFIG_VIOC_AFBCDEC)
	case VIOC_INTR_AFBCDEC0:
	case VIOC_INTR_AFBCDEC1:
		sub_id = id - VIOC_INTR_AFBCDEC0;

		reg = VIOC_AFBCDec_GetAddress((unsigned int)sub_id + AFBCDEC_IRQ_MASK);
		__raw_writel(__raw_readl(reg)
			& (~(mask & VIOC_AFBC_INT_MASK)), reg);
		//if ((__raw_readl(reg) & (VIOC_AFBC_INT_MASK << 16U))
		//	!= (VIOC_AFBC_INT_MASK << 16))
		//	do_irq_mask = 0;
		ret = 0;
		break;
#endif // defined(CONFIG_VIOC_AFBCDEC)
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

		if (ret < 0) {
			goto FUNC_EXIT;
		}
		reg = VIOC_IREQConfig_GetAddress();

		if (id >= 64) {
			__raw_writel(
				(u32)1U << ((unsigned int)id - 64U), reg + type_set_offset + 0x8U);
		} else if (id >= 32) {
			__raw_writel(
				(u32)1U << ((unsigned int)id - 32U), reg + type_set_offset + 0x4U);
		} else {
			__raw_writel((u32)1U << (unsigned int)id, reg + type_set_offset);
		}
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
	unsigned int ret = 0U;

	if ((id < 0) || (id > VIOC_INTR_NUM)) {
		ret = 0U;

		goto FUNC_EXIT;
	}

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
	case VIOC_INTR_DEV2:
	case VIOC_INTR_DEV3:
	case VIOC_INTR_DEV4:
		reg = VIOC_DISP_GetAddress((unsigned int)id) + DSTATUS;
		ret = (__raw_readl(reg) & VIOC_DISP_INT_MASK);
		break;
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
	case VIOC_INTR_RD7:
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
	case VIOC_INTR_RD14:
	case VIOC_INTR_RD15:
	case VIOC_INTR_RD16:
	case VIOC_INTR_RD17:
		id -= VIOC_INTR_RD0;
		reg = VIOC_RDMA_GetAddress((unsigned int)id) + RDMASTAT;
		ret = (__raw_readl(reg) & VIOC_RDMA_INT_MASK);
		break;
	case VIOC_INTR_WD0:
	case VIOC_INTR_WD1:
	case VIOC_INTR_WD2:
	case VIOC_INTR_WD3:
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
		id -= VIOC_INTR_WD0;
		reg = VIOC_WDMA_GetAddress((unsigned int)id) + WDMAIRQSTS_OFFSET;
		ret = (__raw_readl(reg) & VIOC_WDMA_INT_MASK);
		break;
#if defined(CONFIG_VIOC_AFBCDEC)
	case VIOC_INTR_AFBCDEC0:
	case VIOC_INTR_AFBCDEC1:
		id -= VIOC_INTR_AFBCDEC0;
		reg = VIOC_AFBCDec_GetAddress((unsigned int)id) + AFBCDEC_IRQ_STATUS;
		ret = (__raw_readl(reg) & VIOC_AFBC_INT_MASK);
		break;
#endif // defined(CONFIG_VIOC_AFBCDEC)
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
#if defined (CONFIG_TCC807x_CA55_SUB)
			flag = ((__raw_readl(reg + IRQSELECT0_0_OFFSET) & ((u32)1U << (unsigned int)id)) != 0U) ?
#else
			flag = ((__raw_readl(reg + IRQSELECT1_0_OFFSET) & ((u32)1U << (unsigned int)id)) != 0U) ?
#endif
				(__raw_readl(reg + SYNCSTATUS0_OFFSET) & ((u32)1U << (unsigned int)id)) :
				(__raw_readl(reg + RAWSTATUS0_OFFSET) & ((u32)1U << (unsigned int)id));
		} else if (id < 64) {
#if defined (CONFIG_TCC807x_CA55_SUB)
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
		} else {
#if defined (CONFIG_TCC807x_CA55_SUB)
			flag = ((__raw_readl(reg + IRQMASKCLR0_2_OFFSET)
				& ((u32)1U << ((unsigned int)id - 64U))) != 0U) ?
#else
			flag = ((__raw_readl(reg + IRQMASKCLR1_2_OFFSET)
				& ((u32)1U << ((unsigned int)id - 64U))) != 0U) ?
#endif
				(__raw_readl(reg + SYNCSTATUS2_OFFSET)
				& ((u32)1U << ((unsigned int)id - 64U))) :
				(__raw_readl(reg + RAWSTATUS2_OFFSET)
				& ((u32)1U << ((unsigned int)id - 64U)));
		}

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
	bool ret = (bool)false;

	if ((id < 0) || (id > VIOC_INTR_NUM)) {
		ret = (bool)false;

		goto FUNC_EXIT;
	}

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
	case VIOC_INTR_DEV2:
	case VIOC_INTR_DEV3:
	case VIOC_INTR_DEV4:
		reg = VIOC_DISP_GetAddress((unsigned int)id);
		if ((__raw_readl(reg + DSTATUS) & (mask & VIOC_DISP_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
	case VIOC_INTR_RD7:
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
	case VIOC_INTR_RD14:
	case VIOC_INTR_RD15:
	case VIOC_INTR_RD16:
	case VIOC_INTR_RD17:
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
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
		id -= VIOC_INTR_WD0;
		reg = VIOC_WDMA_GetAddress((unsigned int)id) + WDMAIRQSTS_OFFSET;
		if ((__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			/* Prevent KCS warning */
			ret = (bool)false;
		}
		break;
#if defined(CONFIG_VIOC_AFBCDEC)
	case VIOC_INTR_AFBCDEC0:
	case VIOC_INTR_AFBCDEC1:
		id -= VIOC_INTR_AFBCDEC0;
		reg = VIOC_AFBCDec_GetAddress((unsigned int)id) + AFBCDEC_IRQ_STATUS;
		if ((__raw_readl(reg) & (mask & VIOC_AFBC_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)true;
		} else {
			ret = (bool)false;
		}
		break;
#endif // defined(CONFIG_VIOC_AFBCDEC)
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
	bool ret = (bool)false;

	if ((id < 0) || (id > VIOC_INTR_NUM)) {
		ret = (bool)false;

		goto FUNC_EXIT;
	}

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
	case VIOC_INTR_DEV2:
	case VIOC_INTR_DEV3:
	case VIOC_INTR_DEV4:
		reg = VIOC_DISP_GetAddress((unsigned int)id);

		if ((__raw_readl(reg + DIM) & (mask & VIOC_DISP_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)false;
		} else {
			/* Prevent KCS warning */
			ret = (bool)true;
		}
		break;
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
	case VIOC_INTR_RD7:
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
	case VIOC_INTR_RD14:
	case VIOC_INTR_RD15:
	case VIOC_INTR_RD16:
	case VIOC_INTR_RD17:
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
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
		id -= VIOC_INTR_WD0;

		reg = VIOC_WDMA_GetAddress((unsigned int)id) + WDMAIRQMSK_OFFSET;

		if ((__raw_readl(reg) & (mask & VIOC_WDMA_INT_MASK)) != 0U) {
			/* Prevent KCS warning */
			ret = (bool)false;
		} else {
			/* Prevent KCS warning */
			ret = (bool)true;
		}
		break;
#if defined(CONFIG_VIOC_AFBCDEC)
	case VIOC_INTR_AFBCDEC0:
	case VIOC_INTR_AFBCDEC1:
		id -= VIOC_INTR_AFBCDEC0;

		reg = VIOC_AFBCDec_GetAddress((unsigned int)id) + AFBCDEC_IRQ_MASK;

		if ((__raw_readl(reg) & (mask & VIOC_AFBC_INT_MASK)) == 0U) {
			/* Prevent KCS warning */
			ret = (bool)false;
		} else {
			/* Prevent KCS warning */
			ret = (bool)true;
		}
		break;
#endif // defined(CONFIG_VIOC_AFBCDEC)
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
	#if defined(VIOC_DISP4)
	case get_vioc_index(VIOC_DISP4):
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
	int ret = -1;

	if ((id < 0) || (id > VIOC_INTR_NUM)) {
		ret = -1;

		goto FUNC_EXIT;
	}

	switch (id) {
	case VIOC_INTR_DEV0:
	case VIOC_INTR_DEV1:
	case VIOC_INTR_DEV2:
	case VIOC_INTR_DEV3:
	case VIOC_INTR_DEV4:
		reg = VIOC_DISP_GetAddress((unsigned int)id);

		__raw_writel((mask & VIOC_DISP_INT_MASK), reg + DSTATUS);
		ret = 0;
		break;
	case VIOC_INTR_RD0:
	case VIOC_INTR_RD1:
	case VIOC_INTR_RD2:
	case VIOC_INTR_RD3:
	case VIOC_INTR_RD4:
	case VIOC_INTR_RD5:
	case VIOC_INTR_RD6:
	case VIOC_INTR_RD7:
	case VIOC_INTR_RD8:
	case VIOC_INTR_RD9:
	case VIOC_INTR_RD10:
	case VIOC_INTR_RD11:
	case VIOC_INTR_RD12:
	case VIOC_INTR_RD13:
	case VIOC_INTR_RD14:
	case VIOC_INTR_RD15:
	case VIOC_INTR_RD16:
	case VIOC_INTR_RD17:
		id -= VIOC_INTR_RD0;

		reg = VIOC_RDMA_GetAddress((unsigned int)id) + RDMASTAT;

		__raw_writel((mask & VIOC_RDMA_INT_MASK), reg);
		ret = 0;
		break;
	case VIOC_INTR_WD0:
	case VIOC_INTR_WD1:
	case VIOC_INTR_WD2:
	case VIOC_INTR_WD3:
	case VIOC_INTR_WD4:
	case VIOC_INTR_WD5:
	case VIOC_INTR_WD6:
	case VIOC_INTR_WD7:
	case VIOC_INTR_WD8:
		id -= VIOC_INTR_WD0;

		reg = VIOC_WDMA_GetAddress((unsigned int)id) + WDMAIRQSTS_OFFSET;

		__raw_writel((mask & VIOC_WDMA_INT_MASK), reg);
		ret = 0;
		break;
#if defined(CONFIG_VIOC_AFBCDEC)
	case VIOC_INTR_AFBCDEC0:
	case VIOC_INTR_AFBCDEC1:
		id -= VIOC_INTR_AFBCDEC0;

		reg = VIOC_AFBCDec_GetAddress((unsigned int)id) + AFBCDEC_IRQ_CLEAR;

		__raw_writel((__raw_readl(reg) | (mask & VIOC_AFBC_INT_MASK)),
			reg);
		ret = 0;
		break;
#endif // defined(CONFIG_VIOC_AFBCDEC)
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

/* if you change the VIOC interrupt, you need to modify the IRQSELECTn_n offset and IRQMASKCLRn_n part below. */
/* for more detail, when you use VIOC1_SUB_IRQI in subcore or VIOC1_IRQI in maincore, */
/* you need to changed it to IRQSELECT1_0_OFFSET, IRQSELECT1_1_OFFSET, IRQSELECT1_2_OFFSET and IRQMASKCLRn_n_OFFSET should be applied the same.*/

#if defined(CONFIG_TCC807x_CA55_SUB)
	__raw_writel(0xffffffff, reg + IRQMASKCLR0_0_OFFSET);
	__raw_writel(0xffffffff, reg + IRQMASKCLR0_1_OFFSET);
	__raw_writel(0xffffffff, reg + IRQMASKCLR0_2_OFFSET);
#else
	__raw_writel(0xffffffff, reg + IRQMASKCLR1_0_OFFSET);
	__raw_writel(0xffffffff, reg + IRQMASKCLR1_1_OFFSET);
	__raw_writel(0xffffffff, reg + IRQMASKCLR1_2_OFFSET);
#endif

	/* disp irq mask & status clear */
	for (i = 0; i <= (VIOC_INTR_DEV4 - VIOC_INTR_DEV0); i++) {
		reg = VIOC_DISP_GetAddress((unsigned int)i);
		if (reg == NULL) {
			/* Prevent KCS warning */
			continue;
		}
		__raw_writel(VIOC_DISP_INT_MASK, reg + DIM);
		__raw_writel(VIOC_DISP_INT_MASK, reg + DSTATUS);
	}

	/* rdma irq mask & status clear */
	for (i = 0; i <= (VIOC_INTR_RD17 - VIOC_INTR_RD0); i++) {
		reg = VIOC_RDMA_GetAddress((unsigned int)i);
		if (reg == NULL) {
			/* Prevent KCS warning */
			continue;
		}

		__raw_writel(VIOC_RDMA_INT_MASK, reg + RDMAIRQMSK);
		__raw_writel(VIOC_RDMA_INT_MASK, reg + RDMASTAT);
	}

		/* wdma irq mask & status clear */

	for (i = 0; i <= (VIOC_INTR_WD8 - VIOC_INTR_WD0); i++) {
		reg = VIOC_WDMA_GetAddress((unsigned int)i);
		if (reg == NULL) {
			/* Prevent KCS warning */
			continue;
		}

		__raw_writel(VIOC_WDMA_INT_MASK, reg + WDMAIRQMSK_OFFSET);
		__raw_writel(VIOC_WDMA_INT_MASK, reg + WDMAIRQSTS_OFFSET);
	}
}
EXPORT_SYMBOL(vioc_intr_initialize);

static void vioc_intr_disable_core_intr(void)
{
	void __iomem *reg = VIOC_IREQConfig_GetAddress();

	/* if you change the VIOC interrupt, you need to modify the IRQSELECTn_n offset and IRQMASKCLRn_n part below. */
	/* for more detail, when you use VIOC1_SUB_IRQI in subcore or VIOC1_IRQI in maincore, */
	/* you need to changed it to IRQSELECT1_0_OFFSET, IRQSELECT1_1_OFFSET, IRQSELECT1_2_OFFSET and IRQMASKCLRn_n_OFFSET should be applied the same.*/
	#if defined(CONFIG_TCC807X_CA55_SUB)
	(void)pr_info("[INF][VIOC_INTR] disable all VIOC interrupts of VIOC0_IRQI\n");

	__raw_writel(0xffffffff, reg + IRQMASKSET0_0_OFFSET);
	__raw_writel(0xffffffff, reg + IRQMASKSET0_1_OFFSET);
	__raw_writel(0xffffffff, reg + IRQMASKSET0_2_OFFSET);
	#else
	(void)pr_info("[INF][VIOC_INTR] disable all VIOC interrupts of VIOC1_IRQI\n");

	__raw_writel(0xffffffff, reg + IRQMASKSET1_0_OFFSET);
	__raw_writel(0xffffffff, reg + IRQMASKSET1_1_OFFSET);
	__raw_writel(0xffffffff, reg + IRQMASKSET1_2_OFFSET);
	#endif
}

int vioc_intr_init(void)
{
	struct device_node *ViocIntr_np;

	ViocIntr_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_intr");

	if (ViocIntr_np == NULL) {
		(void)pr_info("[INF][VIOC_INTR] disabled [this is mandatory for vioc display]\n");
	} else {
		int i = 0;
		unsigned int temp = 0;/* avoid CERT-C Integers Rule INT31-C */
		for (i = 0; i < (int)VIOC_IRQ_MAX; i++) {
			temp = irq_of_parse_and_map(ViocIntr_np, i);
			if (temp < (UINT_MAX / 2U)) {
  				vioc_base_irq_num[i] = (int)temp;
				(void)pr_info("[INF][VIOC_INTR] vioc-intr%d: irq %d\n", i,
					vioc_base_irq_num[i]);
			}
		}

	vioc_intr_disable_core_intr();
	}
	return 0;
}
EXPORT_SYMBOL(vioc_intr_init);