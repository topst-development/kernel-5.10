// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <soc/telechips/chipinfo.h>

#include <video/telechips/vioc_outcfg.h>
#include <video/telechips/vioc_ddicfg.h>

/* Debugging stuff */
static int debug_ddicfg;
#define dprintk(msg...)							\
	do {								\
		if (debug_ddicfg == 1) {						\
			(void)pr_info("\e[33m[DBG][DDICFG]\e[0m " msg);	\
		}							\
	} while ((bool)0)

static void __iomem *pDDICFG_reg;

void VIOC_DDICONFIG_SetPWDN(
	void __iomem *reg, unsigned int type, unsigned int set)
{
	u32 val;

	switch (type) {
	case DDICFG_TYPE_VIOC:
		val = (__raw_readl(reg + DDI_CLKEN) & ~(CLKEN_VIOC_MASK));
		val |= ((set & 0x1U) << CLKEN_VIOC_SHIFT);
		__raw_writel(val, reg + DDI_CLKEN);
		break;
	default:
		(void)pr_err("[ERR][DDICFG] %s: Wrong type:%d\n", __func__, type);
		break;
	}

	dprintk("type(%d) set(%d)\n", type, set);
}
EXPORT_SYMBOL(VIOC_DDICONFIG_SetPWDN);

void VIOC_DDICONFIG_SetSWRESET(
	void __iomem *reg, unsigned int type, unsigned int set)
{
	u32 val;

	switch (type) {
	case DDICFG_TYPE_VIOC:
		val = (__raw_readl(reg + SWRESET) & ~(SWRESET_VIOC_MASK));
		val |= ((set & 0x1U) << SWRESET_VIOC_SHIFT);
		__raw_writel(val, reg + SWRESET);
		break;
	default:
		(void)pr_err("[ERR][DDICFG] %s: Wrong type:%d\n", __func__, type);
		break;
	}

	dprintk("type(%d) set(%d)\n", type, set);
}
EXPORT_SYMBOL(VIOC_DDICONFIG_SetSWRESET);

int VIOC_DDICONFIG_GetPeriClock(const void __iomem *reg, unsigned int num)
{
	u32 val;

	val = __raw_readl(reg + DDI_CLKEN);
	val >>= (CLKEN_LCLK0_SEL_SHIFT + (num * 2));
	return (val & 3U);
}
EXPORT_SYMBOL(VIOC_DDICONFIG_GetPeriClock);

void VIOC_DDICONFIG_SetPeriClock(
	void __iomem *reg, unsigned int num, unsigned int set)
{
	u32 val;

	if (num > 4U) {
		(void)pr_err("[ERR][DDICONFIG] %s num(%d) is wrong\n", __func__, num);
	} else {
		val = (__raw_readl(reg + DDI_CLKEN) & ~((u32)0x3U << (CLKEN_LCLK0_SEL_SHIFT + (num * 2))));
		val |= ((set & 0x1U) << (CLKEN_LCLK0_SEL_SHIFT + num));
		__raw_writel(val, reg + DDI_CLKEN);
	}
}
EXPORT_SYMBOL(VIOC_DDICONFIG_SetPeriClock);

void VIOC_DDICONFIG_DUMP(void)
{
	unsigned int cnt = 0;

	const void __iomem *pReg = VIOC_DDICONFIG_GetAddress();

	(void)pr_info("[DBG][DDICFG] DDICONFIG ::\n");
	while (cnt < 0x50U) {
		(void)pr_info(
			"[DBG][DDICFG] DDICONFIG + 0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
			cnt, __raw_readl(pReg + cnt),
			__raw_readl(pReg + cnt + 0x4U),
			__raw_readl(pReg + cnt + 0x8U),
			__raw_readl(pReg + cnt + 0xCU));
		cnt += 0x10U;
	}
}
EXPORT_SYMBOL(VIOC_DDICONFIG_DUMP);

void __iomem *VIOC_DDICONFIG_GetAddress(void)
{
	if (pDDICFG_reg == NULL) {
		/* Prevent KCS warning */
		(void)pr_err("[ERR][DDICFG] %s pDDICFG_reg\n", __func__);
	}

	return pDDICFG_reg;
}
EXPORT_SYMBOL(VIOC_DDICONFIG_GetAddress);

/*
 * postcore_initcall(vioc_ddicfg_init);
 * - for VIOC_REMAP functions
 */
int vioc_ddicfg_init(void)
{
	struct device_node *ViocDDICONFIG_np;

	ViocDDICONFIG_np =
		of_find_compatible_node(NULL, NULL, "telechips,ddi_config");
	if (ViocDDICONFIG_np == NULL) {
		(void)pr_info("[INF][DDICFG] vioc-ddicfg: disabled\n");
	} else {
		pDDICFG_reg =
			(void __iomem *)of_iomap(ViocDDICONFIG_np, 0);

		if (pDDICFG_reg != NULL) {
			/* Prevent KCS warning */
			(void)pr_info("[INF][DDICFG] vioc-ddicfg\n");
		}
	}
	return 0;
}
EXPORT_SYMBOL(vioc_ddicfg_init);