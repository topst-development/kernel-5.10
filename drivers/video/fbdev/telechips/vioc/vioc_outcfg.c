// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of_address.h>

#include <video/telechips/vioc_global.h>
#include <video/telechips/vioc_outcfg.h>

/* Debugging stuff */
static int debug_outcfg = 0;
static void __iomem *pOUTCFG_reg;

#define dprintk(msg...)                                           \
	do {                                                      \
		if (debug_outcfg == 1) {                                      \
			(void)pr_info("\e[33m[DBG][OUTCFG]\e[0m " msg); \
		}                                                 \
	} while ((bool)0)

/* -------------------------------
 *2��b00 : Display Device 0 Component
 *2��b01 : Display Device 1 Component
 *2��b10 : Display Device 2 Component
 *2��b11 : NOT USED
 */
void VIOC_OUTCFG_SetOutConfig(unsigned int nType, unsigned int nDisp)
{
	void __iomem *reg = VIOC_OUTCONFIG_GetAddress();
	u32 val;

	if (reg == NULL) {
		/* Prevent KCS Warning */
		(void)pr_err("[ERR][OUTCFG] %s pOUTCFG_reg is NULL\n", __func__);
	} else {
		int ret = -1;
		nDisp = get_vioc_index(nDisp);
		//(void)pr_info("[INF][OUTCFG] %s : addr:%lx nType:%d nDisp:%d\n", __func__,
			//(unsigned long)reg, nType, nDisp);

		switch (nType) {
		case VIOC_OUTCFG_HDMI:
			val = (__raw_readl(reg + MISC) & ~(MISC_HDMISEL_MASK));
			val |= ((nDisp & 0x3U) << MISC_HDMISEL_SHIFT);
			ret = 0;
			break;
		case VIOC_OUTCFG_SDVENC:
			val = (__raw_readl(reg + MISC) & ~(MISC_SDVESEL_MASK));
			val |= ((nDisp & 0x3U) << MISC_SDVESEL_SHIFT);
			ret = 0;
			break;
		case VIOC_OUTCFG_HDVENC:
			val = (__raw_readl(reg + MISC) & ~(MISC_HDVESEL_MASK));
			val |= ((nDisp & 0x3U) << MISC_HDVESEL_SHIFT);
			ret = 0;
			break;
		case VIOC_OUTCFG_M80:
			val = (__raw_readl(reg + MISC) & ~(MISC_M80SEL_MASK));
			val |= ((nDisp & 0x3U) << MISC_M80SEL_SHIFT);
			ret = 0;
			break;
		case VIOC_OUTCFG_MRGB:
			val = (__raw_readl(reg + MISC) & ~(MISC_MRGBSEL_MASK));
			val |= ((nDisp & 0x3U) << MISC_MRGBSEL_SHIFT);
			ret = 0;
			break;
		default:
			WARN_ON(1);
			ret = -1;
			break;
		}
		if (ret < 0) {
			(void)pr_err("[ERR][OUTCFG] %s, wrong type(0x%08x)\n", __func__,
				nType);
		} else {
			__raw_writel(val, reg + MISC);
			dprintk("%s(OUTCFG.MISC=0x%08x)\n", __func__, val);
		}
	}
}
EXPORT_SYMBOL(VIOC_OUTCFG_SetOutConfig);

void __iomem *VIOC_OUTCONFIG_GetAddress(void)
{
	if (pOUTCFG_reg == NULL) {
		/* Prevent KSC warning */
		(void)pr_err("[ERR][OUTCFG] %s pOUTCFG_reg is NULL\n", __func__);
	}

	return pOUTCFG_reg;
}
EXPORT_SYMBOL(VIOC_OUTCONFIG_GetAddress);

void VIOC_OUTCONFIG_DUMP(void)
{
	unsigned int cnt = 0;
	const void __iomem *pReg = VIOC_OUTCONFIG_GetAddress();

	dprintk("[DBG][OUTCFG] OUTCFG ::\n");
	while (cnt < 0x10U) {
		dprintk(
			"[DBG][OUTCFG] OUTCFG + 0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
			cnt, __raw_readl(pReg + cnt),
			__raw_readl(pReg + cnt + 0x4U),
			__raw_readl(pReg + cnt + 0x8U),
			__raw_readl(pReg + cnt + 0xCU));
		cnt += 0x10U;
	}
}
EXPORT_SYMBOL(VIOC_OUTCONFIG_DUMP);

int vioc_outputconfig_init(void)
{
	struct device_node *ViocOutputConfig_np;

	ViocOutputConfig_np =
		of_find_compatible_node(NULL, NULL, "telechips,output_config");
	if (ViocOutputConfig_np == NULL) {
		/* Prevent KCS warning */
		(void)pr_info("[INF][OUTCFG] vioc-outcfg: disabled [this is mandatory for vioc display]\n");
	} else {
		pOUTCFG_reg = of_iomap(ViocOutputConfig_np, 0);

		if (pOUTCFG_reg != NULL) {
			/* Prevent KCS Warning */
			(void)pr_info("[INF][OUTCFG] vioc-outcfg\n");
		}
	}
	return 0;
}
EXPORT_SYMBOL(vioc_outputconfig_init);
