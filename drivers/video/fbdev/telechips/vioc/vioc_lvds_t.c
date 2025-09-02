// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <video/telechips/vioc_lvds.h>

static void __iomem *pLVDS_reg;
#define REG_VIOC_CONFIG(offset) (pLVDS_reg + (offset))
#define LVDS_CTRL_R REG_VIOC_CONFIG(0x40U)
#define LVDS_TXO_SELN_R(x) REG_VIOC_CONFIG(0x44U + (4U * x))
#define LVDS_CFG_R(x) REG_VIOC_CONFIG(0x70U + (4U * x))

#define L_CTRL_SEL_SHIFT 30U
#define L_CTRL_SEL_MASK ((u32)0x3U << L_CTRL_SEL_SHIFT)

#define L_CTRL_TC_SHIFT 21U
#define L_CTRL_TC_MASK ((u32)0x3U << L_CTRL_TC_SHIFT)

#define L_CTRL_P_SHIFT 15U
#define L_CTRL_P_MASK ((u32)0x3FU << L_CTRL_P_SHIFT)

#define L_CTRL_M_SHIFT 8U
#define L_CTRL_M_MASK ((u32)0x3FU << L_CTRL_M_SHIFT)

#define L_CTRL_S_SHIFT 5U
#define L_CTRL_S_MASK ((u32)0x7U << L_CTRL_S_SHIFT)

#define L_CTRL_VSEL_SHIFT 4U
#define L_CTRL_VSEL_MASK BIT(L_CTRL_VSEL_SHIFT)

#define L_CTRL_OC_SHIFT 3U
#define L_CTRL_OC_MASK BIT(L_CTRL_OC_SHIFT)

#define L_CTRL_EN_SHIFT 2U
#define L_CTRL_EN_MASK BIT(L_CTRL_EN_SHIFT)

#define L_CTRL_RST_SHIFT 1U
#define L_CTRL_RST_MASK BIT(L_CTRL_RST_SHIFT)

/* MISC */
#define L_CFG1_LC_SHIFT 1U
#define L_CFG1_LC_MASK BIT(L_CFG1_LC_SHIFT)

#define L_CFG1_CC_SHIFT 1U
#define L_CFG1_CC_MASK BIT(L_CFG1_CC_SHIFT)

#define L_CFG1_CMS_SHIFT 1U
#define L_CFG1_CMS_MASK BIT(L_CFG1_CMS_SHIFT)

#define L_CFG1_VOC_SHIFT 1U
#define L_CFG1_VOC_MASK BIT(L_CFG1_VOC_SHIFT)

#define BITS(x, high, low) \
	((x) & (((1U << ((high) + 1U)) - 1U) & ~((1U << (low)) - 1U)))

/* ddi config register write & read */
#define ddic_writel __raw_writel
#define ddic_readl __raw_readl

void tcc_set_ddi_lvds_reset(unsigned int reset)
{
	void __iomem *reg = (void __iomem *)LVDS_CTRL_R;

	if (reset == 1U) {
		ddic_writel(ddic_readl(reg) & ~(L_CTRL_RST_MASK), reg);
	} else {
		ddic_writel(ddic_readl(reg) | L_CTRL_RST_MASK, reg);
	}
}

void tcc_set_ddi_lvds_pms(
	unsigned int lcdc_n, unsigned int pclk, unsigned int enable)
{
	void __iomem *reg = (void __iomem *)LVDS_CTRL_R;
	unsigned int value;
	unsigned int P, M, S, VSEL, TC, SEL;

	if ((pclk >= 45000000U) && (pclk < 60000000U)) {
		M = 10U;
		P = 10U;
		S = 2U;
		VSEL = 0U;
		TC = 4U;
	} else {
		M = 10U;
		P = 10U;
		S = 1U;
		VSEL = 0U;
		TC = 4U;
	}

	if (lcdc_n == 0U) {
		SEL = 0U;
	} else {
		SEL = 1U;
	}

	value = ddic_readl(reg)
		& ~(L_CTRL_SEL_MASK | L_CTRL_TC_MASK | L_CTRL_P_MASK
		    | L_CTRL_M_MASK | L_CTRL_S_MASK | L_CTRL_VSEL_MASK);

	value |=
		((SEL << L_CTRL_SEL_SHIFT) | (TC << L_CTRL_TC_SHIFT)
		 | (P << L_CTRL_P_SHIFT) | (M << L_CTRL_M_SHIFT)
		 | (S << L_CTRL_S_SHIFT) | (VSEL << L_CTRL_VSEL_SHIFT));

	// enable , disable
	if (enable == 1U) {
		value |= BIT(L_CTRL_EN_SHIFT);
	} else {
		value &= ~(BIT(L_CTRL_EN_SHIFT));
	}

	ddic_writel(value, reg);
}

void tcc_set_ddi_lvds_data_arrary(
	unsigned int data[LVDS_MAX_LINE][LVDS_DATA_PER_LINE])
{
#define LVDS_GET_DATA(i)                                       \
	((LVDS_DATA_PER_LINE - 1U) - ((i) % LVDS_DATA_PER_LINE) \
	 + (LVDS_DATA_PER_LINE * ((i) / LVDS_DATA_PER_LINE)))
	void __iomem *reg;
	unsigned int i, value, reg_num;
	unsigned int *lvdsdata = (unsigned int *)data;
	unsigned int arry0, arry1, arry2, arry3;

	for (i = 0; i < (LVDS_MAX_LINE * LVDS_DATA_PER_LINE); i += 4U) {
		arry0 = LVDS_GET_DATA(i);
		arry1 = LVDS_GET_DATA(i + 1U);
		arry2 = LVDS_GET_DATA(i + 2U);
		arry3 = LVDS_GET_DATA(i + 3U);

		reg_num = i / 4U;
		reg = (void __iomem *)LVDS_TXO_SELN_R(reg_num);
		value = (lvdsdata[arry3] << 24U) | (lvdsdata[arry2] << 16U)
			| (lvdsdata[arry1] << 8U) | (lvdsdata[arry0]);
		ddic_writel(value, reg);
	}
}

/* default setting value for LVDS controller. */
void tcc_set_ddi_lvds_config(void)
{
	void __iomem *reg = (void __iomem *)LVDS_CFG_R(1U);
	unsigned int MISC1_LC = 0, MISC1_CC = 0, MISC1_CMS = 0, MISC1_VOC = 1;
	unsigned int value = 0;

	if (IS_ERR((void *)reg)) {
		(void)pr_err("[ERR][LVDS] ddi lvds address error");
		goto err;
	}

	value = ddic_readl(reg);

	value &=
		~(L_CFG1_LC_MASK | L_CFG1_CC_MASK | L_CFG1_CMS_MASK
		  | L_CFG1_VOC_MASK);
	value |=
		((MISC1_LC << L_CFG1_LC_SHIFT) | (MISC1_CC << L_CFG1_CC_SHIFT)
		 | (MISC1_CMS << L_CFG1_CMS_SHIFT)
		 | (MISC1_VOC << L_CFG1_VOC_SHIFT));

	ddic_writel(value, reg);

err:
	return;
}

int vioc_lvds_init(void)
{
	struct device_node *ViocLVDS_np;

	ViocLVDS_np = of_find_compatible_node(NULL, NULL, "telechips,lvds_phy");
	if (ViocLVDS_np == NULL) {
		(void)pr_info("[INF][LVDS] lvds_phy : disabled\n");
	} else {
		pLVDS_reg = (void __iomem *)of_iomap(ViocLVDS_np, 0);
		(void)pr_info("[INF][LVDS] lvds_phy address :  0x%p\n", pLVDS_reg);
	}
	return 0;
}
EXPORT_SYMBOL(vioc_lvds_init);
