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
		val = (__raw_readl(reg + DDI_PWDN) & ~(PWDN_VIOC_MASK));
		val |= ((set & 0x1U) << PWDN_VIOC_SHIFT);
		__raw_writel(val, reg + DDI_PWDN);
		break;
#if !(defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC750X))
	case DDICFG_TYPE_HDMNI:
		val = (__raw_readl(reg + DDI_PWDN) & ~(PWDN_HDMI_MASK));
		val |= ((set & 0x1U) << PWDN_HDMI_SHIFT);
		__raw_writel(val, reg + DDI_PWDN);
		break;
#ifdef CONFIG_ARCH_TCC803X
	case DDICFG_TYPE_LVDS:
		val = (__raw_readl(reg + DDI_PWDN) & ~(PWDN_LVDS_MASK));
		val |= ((set & 0x1U) << PWDN_LVDS_SHIFT);
		__raw_writel(val, reg + DDI_PWDN);
		break;
	case DDICFG_TYPE_MIPI:
		val = (__raw_readl(reg + DDI_PWDN) & ~(PWDN_MIPI_MASK));
		val |= ((set & 0x1U) << PWDN_MIPI_SHIFT);
		__raw_writel(val, reg + DDI_PWDN);
		break;
#endif
#endif
#ifdef CONFIG_TCC_VIQE_MADI
	case DDICFG_TYPE_MADI:
		val = (__raw_readl(reg + DDI_PWDN) & ~(PWDN_MADI_MASK));
		val |= ((set & 0x1U) << PWDN_MADI_SHIFT);
		__raw_writel(val, reg + DDI_PWDN);
		break;
	case DDICFG_TYPE_SAR:
		val = (__raw_readl(reg + DDI_PWDN) & ~(PWDN_SAR_MASK));
		val |= ((set & 0x1U) << PWDN_SAR_SHIFT);
		__raw_writel(val, reg + DDI_PWDN);
		break;
#endif
#if defined(CONFIG_ARCH_TCC897X)
	case DDICFG_TYPE_G2D:
		val = (__raw_readl(reg + DDI_PWDN) & ~(PWDN_G2D_MASK));
		val |= ((set & 0x1U) << PWDN_G2D_SHIFT);
		__raw_writel(val, reg + DDI_PWDN);
		break;
#endif
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
#if !(defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC750X))
	case DDICFG_TYPE_HDMNI:
		val = (__raw_readl(reg + SWRESET) & ~(SWRESET_HDMI_MASK));
		val |= ((set & 0x1U) << SWRESET_HDMI_SHIFT);
		__raw_writel(val, reg + SWRESET);
		break;
#ifdef CONFIG_ARCH_TCC803X
	case DDICFG_TYPE_LVDS:
		val = (__raw_readl(reg + SWRESET) & ~(SWRESET_LVDS_MASK));
		val |= ((set & 0x1U) << SWRESET_LVDS_SHIFT);
		__raw_writel(val, reg + SWRESET);
		break;
	case DDICFG_TYPE_MIPI:
		val = (__raw_readl(reg + SWRESET) & ~(SWRESET_MIPI_MASK));
		val |= ((set & 0x1U) << SWRESET_MIPI_SHIFT);
		__raw_writel(val, reg + SWRESET);
		break;
#endif
#else  // CONFIG_ARCH_TCC805X
#if !defined(CONFIG_ARCH_TCC750X)
	case DDICFG_TYPE_DP_AXI:
		val = (__raw_readl(reg + SWRESET) & ~(SWRESET_DP_AXI_MASK));
		val |= ((set & 0x1U) << SWRESET_DP_AXI_SHIFT);
		__raw_writel(val, reg + SWRESET);
		break;
	case DDICFG_TYPE_ISP_AXI:
		val = (__raw_readl(reg + SWRESET) & ~(SWRESET_ISP_AXI_MASK));
		val |= ((set & 0x1U) << SWRESET_ISP_AXI_SHIFT);
		__raw_writel(val, reg + SWRESET);
		break;
#endif
#endif // !defined(CONFIG_ARCH_TCC805X)
#ifdef CONFIG_TCC_VIQE_MADI
	case DDICFG_TYPE_MADI:
		val = (__raw_readl(reg + SWRESET) & ~(SWRESET_MADI_MASK));
		val |= ((set & 0x1U) << SWRESET_MADI_SHIFT);
		__raw_writel(val, reg + SWRESET);
		break;
#endif
#if defined(CONFIG_ARCH_TCC897X)
	case DDICFG_TYPE_G2D:
		val = (__raw_readl(reg + SWRESET) & ~(SWRESET_G2D_MASK));
		val |= ((set & 0x1U) << SWRESET_G2D_SHIFT);
		__raw_writel(val, reg + SWRESET);
		break;
#endif
	default:
		(void)pr_err("[ERR][DDICFG] %s: Wrong type:%d\n", __func__, type);
		break;
	}

	dprintk("type(%d) set(%d)\n", type, set);
}
EXPORT_SYMBOL(VIOC_DDICONFIG_SetSWRESET);

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
int VIOC_DDICONFIG_GetPeriClock(const void __iomem *reg, unsigned int num)
{
	u32 val;

	val = __raw_readl(reg + DDI_PWDN);
	val >>= (PWDN_L0S_SHIFT + num);
	return (((val & 1U) != 0U) ? 1 : 0);
}
EXPORT_SYMBOL(VIOC_DDICONFIG_GetPeriClock);

void VIOC_DDICONFIG_SetPeriClock(
	void __iomem *reg, unsigned int num, unsigned int set)
{
	u32 val;

	if (num > 2U) {
		(void)pr_err("[ERR][DDICONFIG] %s num(%d) is wrong\n", __func__, num);
	} else {
		val = (__raw_readl(reg + DDI_PWDN) & ~((u32)0x1U << (PWDN_L0S_SHIFT + num)));
		val |= ((set & 0x1U) << (PWDN_L0S_SHIFT + num));
		__raw_writel(val, reg + DDI_PWDN);
	}
}
EXPORT_SYMBOL(VIOC_DDICONFIG_SetPeriClock);
#endif

#if !(defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC750X))
void VIOC_DDICONFIG_Set_hdmi_enable(
	void __iomem *reg, unsigned int enable)
{
	u32 val;

	val = (__raw_readl(reg + HDMI_CTRL) & ~(HDMI_CTRL_EN_MASK));
	val |= ((enable & (HDMI_CTRL_EN_MASK >> HDMI_CTRL_EN_SHIFT))
		<< HDMI_CTRL_EN_SHIFT);
	__raw_writel(val, reg + HDMI_CTRL);
}
EXPORT_SYMBOL(VIOC_DDICONFIG_Set_hdmi_enable);

void VIOC_DDICONFIG_Set_prng(void __iomem *reg, unsigned int enable)
{
	u32 val;

	val = (__raw_readl(reg + HDMI_CTRL) & ~(HDMI_CTRL_PRNG_MASK));
	val |= ((enable & (HDMI_CTRL_PRNG_MASK >> HDMI_CTRL_PRNG_SHIFT))
		<< HDMI_CTRL_PRNG_SHIFT);
	__raw_writel(val, reg + HDMI_CTRL);
}
EXPORT_SYMBOL(VIOC_DDICONFIG_Set_prng);

void VIOC_DDICONFIG_Set_refclock(
	void __iomem *reg, unsigned int ref_clock)
{
	u32 val;

	val = (__raw_readl(reg + HDMI_CTRL) & ~(HDMI_CTRL_REF_MASK));
	val |= (ref_clock & HDMI_CTRL_REF_MASK);
	__raw_writel(val, reg + HDMI_CTRL);
}
EXPORT_SYMBOL(VIOC_DDICONFIG_Set_refclock);

#if defined(HDMI_CTRL_PLL_SEL_MASK)
void VIOC_DDICONFIG_Set_pll_sel(
	void __iomem *reg, unsigned int pll_sel)
{
	u32 val;

	val = (__raw_readl(reg + HDMI_CTRL) & ~(HDMI_CTRL_PLL_SEL_MASK));
	val |= (pll_sel & HDMI_CTRL_PLL_SEL_MASK);
	__raw_writel(val, reg + HDMI_CTRL);
}
EXPORT_SYMBOL(VIOC_DDICONFIG_Set_pll_sel);
#endif

#if defined(HDMI_CTRL_PHY_ST_MASK)
int VIOC_DDICONFIG_get_phy_status(
	const void __iomem *reg, unsigned int phy_mode)
{
	int status = 0;
	u32 val;

	val = __raw_readl(reg + HDMI_CTRL);

	val &= ((phy_mode & HDMI_CTRL_PHY_ST_MASK));
	if (val > 0U) {
		status = 1;
	}

	return status;
}
EXPORT_SYMBOL(VIOC_DDICONFIG_get_phy_status);
#endif

#if defined(HDMI_CTRL_TB_MASK)
void VIOC_DDICONFIG_Set_tmds_bit_order(
	void __iomem *reg, unsigned int bit_order)
{
	u32 val;

	val = (__raw_readl(reg + HDMI_CTRL) & ~(HDMI_CTRL_TB_MASK));
	val |= (bit_order & HDMI_CTRL_TB_MASK);
	__raw_writel(val, reg + HDMI_CTRL);
}
EXPORT_SYMBOL(VIOC_DDICONFIG_Set_tmds_bit_order);
#endif

#if defined(HDMI_CTRL_RESET_PHY_MASK)
void VIOC_DDICONFIG_reset_hdmi_phy(
	void __iomem *reg, unsigned int reset_enable)
{
	u32 val;

	val = (__raw_readl(reg + HDMI_CTRL) & ~(HDMI_CTRL_RESET_PHY_MASK));
	val |= ((reset_enable & 0x1U) << HDMI_CTRL_RESET_PHY_SHIFT);
	__raw_writel(val, reg + HDMI_CTRL);
}
EXPORT_SYMBOL(VIOC_DDICONFIG_reset_hdmi_phy);
#endif

#if defined(HDMI_CTRL_RESET_LINK_MASK)
void VIOC_DDICONFIG_reset_hdmi_link(
	void __iomem *reg, unsigned int reset_enable)
{
	u32 val;

	val = (__raw_readl(reg + HDMI_CTRL) & ~(HDMI_CTRL_RESET_LINK_MASK));
	val |= ((reset_enable & 0x1U) << HDMI_CTRL_RESET_LINK_SHIFT);
	__raw_writel(val, reg + HDMI_CTRL);
}
EXPORT_SYMBOL(VIOC_DDICONFIG_reset_hdmi_link);
#endif // //!defined(CONFIG_ARCH_TCC805X)

#endif
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
void VIOC_DDICONFIG_DAC_PWDN_Control(
	void __iomem *reg, enum dac_pwdn_status dac_status)
{
	uint32_t val;

	if (reg == NULL)
		reg = VIOC_DDICONFIG_GetAddress();

	// val = (__raw_readl(reg + DAC_CONFIG) & ~(DAC_CONFIG_PWDN_MASK));
	// if (dac_status)
		// val |= ((dac_status & 0x1) << DAC_CONFIG_PWDN_SHIFT);

	if (dac_status)
		val = 0x17bb0000; // from SoC (Jung HeeSung) - AlanK
	else
		val = 0; // PDB_REF power-down

	__raw_writel(val, reg + DAC_CONFIG);
}
EXPORT_SYMBOL(VIOC_DDICONFIG_DAC_PWDN_Control);
#endif

/*
 * TV-Encoder (TVE)
 * ================
 *
 * Name | Description     | TCC898X | TCC803X | TCC899X |
 * -----+-----------------+---------+---------+---------+
 *  TVO | Telechips TVE   | O       | O       | O       |
 *  BVO | SigmaDesign TVE | X       | X       | O       |
 */
#if defined(CONFIG_FB_TCC_COMPOSITE_BVO)
static void VIOC_DDICONFIG_BVOVENC_SetEnable(
	void __iomem *reg, unsigned int enable)
{
	uint32_t val;

	dprintk("%s(%d)\n", __func__, enable);

	val = (__raw_readl(reg + BVOVENC_EN) & ~(BVOVENC_EN_EN_MASK));
	if (enable)
		val |= ((u32)0x1U << BVOVENC_EN_EN_SHIFT);
	__raw_writel(val, reg + BVOVENC_EN);
}

static void VIOC_DDICONFIG_BVOVENC_Reset(void __iomem *reg)
{
	uint32_t val;

	val = (__raw_readl(reg + BVOVENC_EN) & ~(BVOVENC_EN_BVO_RST_MASK));

	/* Reset BVO (sync signals & registers) */
	val |= (BVOVENC_RESET_BIT_ALL << BVOVENC_EN_BVO_RST_SHIFT);
	__raw_writel(val, reg + BVOVENC_EN);

	/* 0x0 is normal status (not reset) */
	val = (__raw_readl(reg + BVOVENC_EN) & ~(BVOVENC_EN_BVO_RST_MASK));
	__raw_writel(val, reg + BVOVENC_EN);

	dprintk("%s\n", __func__);
}

void VIOC_DDICONFIG_BVOVENC_Reset_ctrl(int reset_bit)
{
	uint32_t val;

	val = (__raw_readl(pDDICFG_reg + BVOVENC_EN)
	       & ~(BVOVENC_EN_BVO_RST_MASK));

	/* Reset the reset_bit */
	val |= (reset_bit << BVOVENC_EN_BVO_RST_SHIFT);
	__raw_writel(val, pDDICFG_reg + BVOVENC_EN);

	/* 0x0 is normal status (not reset) */
	val = (__raw_readl(pDDICFG_reg + BVOVENC_EN)
	       & ~(BVOVENC_EN_BVO_RST_MASK));
	__raw_writel(val, pDDICFG_reg + BVOVENC_EN);

	dprintk("%s(0x%x)\n", __func__, reset_bit);
}
EXPORT_SYMBOL(VIOC_DDICONFIG_BVOVENC_Reset_ctrl);
#else
static void VIOC_DDICONFIG_TVOVENC_SetEnable(
	const void __iomem *reg, unsigned int enable)
{
#if !(defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC897X) \
	|| defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC750X))
	uint32_t val;

	dprintk("%s(%d)\n", __func__, enable);

	val = (__raw_readl(reg + NTSCPAL_EN) & ~(SDVENC_PCLKEN_MASK));
	if (enable == 1U) {
		/* Prevent KCS warning */
		val |= ((enable & 0x1U) << SDVENC_PCLKEN_SHIFT);
	}
	__raw_writel(val, reg + NTSCPAL_EN);
#else
	/* avoid MISRA C-2012 Rule 2.7 */
    (void)reg;
    (void)enable;
#endif
}
#endif // CONFIG_FB_TCC_COMPOSITE_BVO

static void VIOC_DDICONFIG_Select_TVEncoder(
	const void __iomem *reg, unsigned int tve_type)
{
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	uint32_t val;

	dprintk("%s(%d)\n", __func__, tve_type);

	val = (__raw_readl(reg + BVOVENC_EN) & ~(BVOVENC_EN_SEL_MASK));
	val |= (tve_type << BVOVENC_EN_SEL_SHIFT);
	__raw_writel(val, reg + BVOVENC_EN);
#else
	/* avoid MISRA C-2012 Rule 2.7 */
    (void)reg;
    (void)tve_type;
#endif
}

void VIOC_DDICONFIG_NTSCPAL_SetEnable(
	const void __iomem *reg, unsigned int enable, unsigned int lcdc_num)
{
	dprintk("%s(lcdc%d %s)\n", __func__, lcdc_num,
		(enable == 1U) ? "enable" : "disable");

#if defined(CONFIG_FB_TCC_COMPOSITE_BVO)
	VIOC_OUTCFG_BVO_SetOutConfig(lcdc_num);  // pre: select lcdc for TVE
	VIOC_DDICONFIG_Select_TVEncoder(reg, 1U); // 1st: select bvo
	VIOC_DDICONFIG_BVOVENC_SetEnable(reg, enable); // 2nd: enable bvo
	if (enable == 1U) {
		VIOC_DDICONFIG_BVOVENC_Reset(reg);       // 3rd: reset bvo
		VIOC_DDICONFIG_Select_TVEncoder(reg, 1U); // 4th: select bvo
		VIOC_DDICONFIG_BVOVENC_SetEnable(
			reg, enable); // 5th: enable bvo
	}
#else
	VIOC_DDICONFIG_Select_TVEncoder(reg, 0U);       // 1st: select tvo
	VIOC_DDICONFIG_TVOVENC_SetEnable(reg, enable); // 2nd: enable tvo
#endif
}
EXPORT_SYMBOL(VIOC_DDICONFIG_NTSCPAL_SetEnable);

#if defined(CONFIG_ARCH_TCC897X)
void VIOC_DDICONFIG_LVDS_SetEnable(
	void __iomem *reg, unsigned int enable)
{
	unsigned int val;

	val = (__raw_readl(reg + LVDS_CTRL) & ~(LVDS_CTRL_EN_MASK));
	if (enable == 1U) {
		val |= ((u32)0x1U << LVDS_CTRL_EN_SHIFT);
	}
	__raw_writel(val, reg + LVDS_CTRL);
}
EXPORT_SYMBOL(VIOC_DDICONFIG_LVDS_SetEnable);

void VIOC_DDICONFIG_LVDS_SetReset(
	void __iomem *reg, unsigned int reset)
{
	unsigned int val;

	val = (__raw_readl(reg + LVDS_CTRL) & ~(LVDS_CTRL_RST_MASK));
	if (reset == 1U) {
		val |= ((u32)0x1U << LVDS_CTRL_RST_SHIFT);
	}
	__raw_writel(val, reg + LVDS_CTRL);
}
EXPORT_SYMBOL(VIOC_DDICONFIG_LVDS_SetReset);

void VIOC_DDICONFIG_LVDS_SetPort(
	void __iomem *reg, unsigned int select)
{
	unsigned int val;

	val = (__raw_readl(reg + LVDS_CTRL) & ~(LVDS_CTRL_SEL_MASK));
	val |= ((select & 0x3U) << LVDS_CTRL_SEL_SHIFT);
	__raw_writel(val, reg + LVDS_CTRL);
}
EXPORT_SYMBOL(VIOC_DDICONFIG_LVDS_SetPort);

void VIOC_DDICONFIG_LVDS_SetPLL(
	void __iomem *reg, unsigned int vsel, unsigned int p,
	unsigned int m, unsigned int s, unsigned int tc)
{
	unsigned int val;

	val = (__raw_readl(reg + LVDS_CTRL)
	       & ~(LVDS_CTRL_VSEL_MASK | LVDS_CTRL_P_MASK | LVDS_CTRL_M_MASK
		   | LVDS_CTRL_S_MASK | LVDS_CTRL_OC_MASK));
	val |= (((vsel & 0x1U) << LVDS_CTRL_VSEL_SHIFT)
		| ((p & 0x7FU) << LVDS_CTRL_P_SHIFT)
		| ((m & 0x7FU) << LVDS_CTRL_M_SHIFT)
		| ((s & 0x7U) << LVDS_CTRL_S_SHIFT)
		| ((tc & 0x7U) << LVDS_CTRL_OC_SHIFT));
	__raw_writel(val, reg + LVDS_CTRL);
}
EXPORT_SYMBOL(VIOC_DDICONFIG_LVDS_SetPLL);

void VIOC_DDICONFIG_LVDS_SetConnectivity(
	void __iomem *reg, unsigned int voc, unsigned int cms,
	unsigned int cc, unsigned int lc)
{
	unsigned int val;

	val = (__raw_readl(reg + LVDS_MISC1)
	       & ~(LVDS_MISC1_VOC_MASK | LVDS_MISC1_CMS_MASK
		   | LVDS_MISC1_CC_MASK | LVDS_MISC1_LC_MASK));
	val |= (((voc & 0x1U) << LVDS_MISC1_VOC_SHIFT)
		| ((cms & 0x1U) << LVDS_MISC1_CMS_SHIFT)
		| ((cc & 0x3U) << LVDS_MISC1_CC_SHIFT)
		| ((lc & 0x1U) << LVDS_MISC1_LC_SHIFT));
	__raw_writel(val, reg + LVDS_MISC1);
}
EXPORT_SYMBOL(VIOC_DDICONFIG_LVDS_SetConnectivity);

void VIOC_DDICONFIG_LVDS_SetPath(
	void __iomem *reg, unsigned int path, unsigned int bit)
{
	if (path > 8U) {
		pr_err("[ERR][DDICFG] %s: path:%d wrong path\n", __func__, path);
	} else {
		__raw_writel((bit & 0xFFFFFFFFU), reg + (LVDS_TXO_SEL0 + (0x4U * path)));
	}
}
EXPORT_SYMBOL(VIOC_DDICONFIG_LVDS_SetPath);
#endif

#if defined(CONFIG_ARCH_TCC803X)
void VIOC_DDICONFIG_MIPI_Reset_DPHY(
	void __iomem *reg, unsigned int reset)
{
	u32 val;

	val = (__raw_readl(reg + MIPI_CFG) & ~(MIPI_CFG_S_RESETN_MASK));
	// 0x1 : release, 0x0 : reset
	if (reset == 0U) {
		/* Prevent KCS warning */
		val |= ((u32)0x1U << MIPI_CFG_S_RESETN_SHIFT);
	}
	__raw_writel(val, reg + MIPI_CFG);
}
EXPORT_SYMBOL(VIOC_DDICONFIG_MIPI_Reset_DPHY);

void VIOC_DDICONFIG_MIPI_Reset_GEN(
	void __iomem *reg, unsigned int reset)
{
	u32 val;

	val = (__raw_readl(reg + MIPI_CFG)
	       & ~(MIPI_CFG_GEN_PX_RST_MASK | MIPI_CFG_GEN_APB_RST_MASK));
	if (reset == 1U) {
		/* Prevent KCS warning */
		val |= (((u32)0x1U << MIPI_CFG_GEN_PX_RST_SHIFT)
			| ((u32)0x1U << MIPI_CFG_GEN_APB_RST_SHIFT));
	}
	__raw_writel(val, reg + MIPI_CFG);
}
EXPORT_SYMBOL(VIOC_DDICONFIG_MIPI_Reset_GEN);
#endif

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
/*
 * VIOC_REMAP (VIOC Register Address Remap Enable Register)
 */
/* HIS_CALLING */
unsigned int VIOC_DDICONFIG_GetViocRemap(void)
{
	u32 val;

	if (get_chip_rev() != 0U) {
		/* Prevent KCS warning */
		val = __raw_readl(pDDICFG_reg + VIOC_REMAP);
	}
	else {
		/* Prevent KCS warning */
		val = 0U;
	}

	// (void)pr_info("[INF][DDICFG] %s: chip(%s) remap(%s)\n", __func__,
		// get_chip_rev() ? "CS" : "ES",
		// val ? "on" : "off");

	return val;
}
EXPORT_SYMBOL(VIOC_DDICONFIG_GetViocRemap);

unsigned int VIOC_DDICONFIG_SetViocRemap(unsigned int enable)
{
	u32 val = 0;

	if (get_chip_rev() != 0U) {
		__raw_writel(
			(enable & VIOC_REMAP_MASK), pDDICFG_reg + VIOC_REMAP);
		val = __raw_readl(pDDICFG_reg + VIOC_REMAP);
	}

	return val;
}
EXPORT_SYMBOL(VIOC_DDICONFIG_SetViocRemap);
#endif

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
