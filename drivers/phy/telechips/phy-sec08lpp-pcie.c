// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>

/*
 * PCIe controller wrapper pma configuration registers
 */
#define PCIE_PMA_CMN_REG00B		(0x002CU)
#define PCIE_PMA_CMN_REG016		(0x0058U)
#define PCIE_PMA_CMN_REG024		(0x0090U)
#define PCIE_PMA_CMN_REG025		(0x0094U)
#define PCIE_PMA_CMN_REG10A		(0x0428U)
#define PCIE_PMA_CMN_REG130		(0x04C0U)
#define PCIE_PMA_CMN_REG13C		(0x04F0U)
#define PCIE_PMA_CMN_REG153		(0x054CU)
#define PCIE_PMA_CMN_REG17B		(0x05ECU)
#define PCIE_PMA_TRSV_REG40B		(0x102CU)
#define PCIE_PMA_TRSV_REG424		(0x1090U)
#define PCIE_PMA_TRSV_REG47A		(0x11E8U)
#define PCIE_PMA_TRSV_REG47B		(0x11ECU)
#define PCIE_PMA_TRSV_REG47C		(0x11F0U)
#define PCIE_PMA_TRSV_REG4CF		(0x133CU)
#define PCIE_PMA_TRSV_REG516		(0x1458U)
#define PCIE_PMA_TRSV_REG517		(0x145CU)
#define PCIE_PMA_TRSV_REG518		(0x1460U)
#define PCIE_PMA_TRSV_REG519		(0x1464U)
#define PCIE_PMA_TRSV_REG56D		(0x15B4U)
#define PCIE_PMA_TRSV_REG5F1		(0x17C4U)
#define PCIE_PMA_TRSV_REG5FD		(0x17F4U)

/*
 * PCIe controller wrapper pcs configuration registers
 */
#define PCIE_PCS_OUT_VEC_4		(0x0154U)

/*
 * PCIe controller wrapper phy configuration registers
 */
#define PCIE_PHY_REG00			(0x00U)
#define PCIE_PHY_REG01			(0x04U)
#define PCIE_PHY_REG04			(0x10U)

/*
 * PCIe controller wrapper clock configuration registers
 */
#define PCIE_CLK_CFG00			(0x000U)
#define PCIE_CLK_CFG04			(0x010U)
#define PCIE_CLK_CFG06			(0x018U)
#define PCIE_CLK_CFG07			(0x01CU)
#define PCIE_CLK_CFG08			(0x020U)

/*
 * Mask/shift bits in PCIe related registers
 */
#define PCIE_CLK_CFG_RESERVED_CON_SHIFT		(13U)
#define PCIE_CLK_CFG_RESERVED_CON_MASK		((u32)0xFFFFU << PCIE_CLK_CFG_RESERVED_CON_SHIFT)

#define PCIE_CLK_CFG_BGR_EN_SHIFT		(30U)
#define PCIE_CLK_CFG_BGR_EN_MASK		((u32)0x1U << PCIE_CLK_CFG_BGR_EN_SHIFT)

#define PCIE_CLK_CFG_PLL_LOCK_SHIFT		(7U)
#define PCIE_CLK_CFG_PLL_LOCK_MASK		((u32)0x1U << PCIE_CLK_CFG_PLL_LOCK_SHIFT)

#define PCIE_CLK_CFG_RESETB_SHIFT		(31U)
#define PCIE_CLK_CFG_RMRES_CTRL_SHIFT		(29U)
#define PCIE_CLK_CFG_VBG_SEL_SHIFT		(20U)
#define PCIE_CLK_CFG_PDIV3_SEL_SHIFT		(19U)
#define PCIE_CLK_CFG_S_SHIFT		(16U)
#define PCIE_CLK_CFG_M_SHIFT		(6U)
#define PCIE_CLK_CFG_P_SHIFT		(0U)
#define PCIE_CLK_CFG_RESETB_MASK		((u32)0x1U << PCIE_CLK_CFG_RESETB_SHIFT)
#define PCIE_CLK_CFG_RMRES_CTRL_MASK		((u32)0x3U << PCIE_CLK_CFG_RMRES_CTRL_SHIFT)
#define PCIE_CLK_CFG_VBG_SEL_MASK		((u32)0x3U << PCIE_CLK_CFG_VBG_SEL_SHIFT)
#define PCIE_CLK_CFG_PDIV3_SEL_MASK		((u32)0x1U << PCIE_CLK_CFG_PDIV3_SEL_SHIFT)
#define PCIE_CLK_CFG_S_MASK		((u32)0x7U << PCIE_CLK_CFG_S_SHIFT)
#define PCIE_CLK_CFG_M_MASK		((u32)0x3FFU << PCIE_CLK_CFG_M_SHIFT)
#define PCIE_CLK_CFG_P_MASK		((u32)0x3FU << PCIE_CLK_CFG_P_SHIFT)
#define PCIE_CLK_CFG_PLL_CTRL_MASK		(PCIE_CLK_CFG_P_MASK | \
		PCIE_CLK_CFG_M_MASK | \
		PCIE_CLK_CFG_S_MASK | \
		PCIE_CLK_CFG_PDIV3_SEL_MASK | \
		PCIE_CLK_CFG_VBG_SEL_MASK | \
		PCIE_CLK_CFG_RMRES_CTRL_MASK)

#define PCIE_CLK_CFG_ERIO_EN_SHIFT		(18U)
#define PCIE_CLK_CFG_ERIO_PLL_LOCK_DONE_SHIFT		(5U)
#define PCIE_CLK_CFG_ERIO_USE_LOCK_DONE_SHIFT		(4U)
#define PCIE_CLK_CFG_ERIO_ISO_ENB_SHIFT		(0U)
#define PCIE_CLK_CFG_ERIO_EN_MASK		((u32)0x1U << PCIE_CLK_CFG_ERIO_EN_SHIFT)
#define PCIE_CLK_CFG_ERIO_PLL_LOCK_DONE_MASK		((u32)0x1U << PCIE_CLK_CFG_ERIO_PLL_LOCK_DONE_SHIFT)
#define PCIE_CLK_CFG_ERIO_USE_LOCK_DONE_MASK		((u32)0x1U << PCIE_CLK_CFG_ERIO_USE_LOCK_DONE_SHIFT)
#define PCIE_CLK_CFG_ERIO_ISO_ENB_MASK		((u32)0x1U << PCIE_CLK_CFG_ERIO_ISO_ENB_SHIFT)

#define PCIE_PHY_REG_PCS_RSTN_SHIFT		(3U)
#define PCIE_PHY_REG_PMA_PORT0_RSTN_SHIFT		(2U)
#define PCIE_PHY_REG_PMA_CMN_RSTN_SHIFT		(1U)
#define PCIE_PHY_REG_PMA_INIT_RSTN_SHIFT		(0U)
#define PCIE_PHY_REG_PCS_RSTN_MASK		((u32)0x1U << PCIE_PHY_REG_PCS_RSTN_SHIFT)
#define PCIE_PHY_REG_PMA_PORT0_RSTN_MASK		((u32)0x1U << PCIE_PHY_REG_PMA_PORT0_RSTN_SHIFT)
#define PCIE_PHY_REG_PMA_CMN_RSTN_MASK		((u32)0x1U << PCIE_PHY_REG_PMA_CMN_RSTN_SHIFT)
#define PCIE_PHY_REG_PMA_INIT_RSTN_MASK		((u32)0x1U << PCIE_PHY_REG_PMA_INIT_RSTN_SHIFT)

#define PCIE_PHY_REG_LPLL_REF_CLK_SEL_SHIFT		(3U)
#define PCIE_PHY_REG_PORT0_REF_CLK_EN_SHIFT		(0U)
#define PCIE_PHY_REG_LPLL_REF_CLK_SEL_MASK		((u32)0x3U << PCIE_PHY_REG_LPLL_REF_CLK_SEL_SHIFT)
#define PCIE_PHY_REG_PORT0_REF_CLK_EN_MASK		((u32)0x1U << PCIE_PHY_REG_PORT0_REF_CLK_EN_SHIFT)

#define PCIE_PHY_REG_PMA_POWER_OFF_SHIFT		(1U)
#define PCIE_PHY_REG_PMA_POWER_OFF_MASK		((u32)0x1U << PCIE_PHY_REG_PMA_POWER_OFF_SHIFT)

#define PCIE_PMA_ANA_LCPLL_SDM_DENOMINATOR_IC1234_G4_SHIFT		(0U)
#define PCIE_PMA_ANA_LCPLL_SDM_DENOMINATOR_IC1234_G4_MASK		((u32)0xFFU << PCIE_PMA_ANA_LCPLL_SDM_DENOMINATOR_IC1234_G4_SHIFT)

#define PCIE_PMA_IGNORE_ADAP_DONE_SHIFT		(0U)
#define PCIE_PMA_IGNORE_ADAP_DONE_MASK		((u32)0x1U << PCIE_PMA_IGNORE_ADAP_DONE_SHIFT)

#define PCIE_PMA_ANA_LCPLL_AFC_VCO_CNT_RUN_NUM_SHIFT		(3U)
#define PCIE_PMA_ANA_LCPLL_AFC_VCO_CNT_RUN_NUM_MASK		((u32)0x1FU << PCIE_PMA_ANA_LCPLL_AFC_VCO_CNT_RUN_NUM_SHIFT)

#define PCIE_PMA_ANA_LCPLL_AVC_MAN_CAP_BIAS_CODE_SHIFT		(3U)
#define PCIE_PMA_ANA_LCPLL_AVC_MAN_CAP_BIAS_CODE_MASK		((u32)0x7U << PCIE_PMA_ANA_LCPLL_AVC_MAN_CAP_BIAS_CODE_SHIFT)

#define PCIE_PMA_ANA_LCPLL_ANA_LPF_R_SEL_LC1234_G3_SHIFT		(0U)
#define PCIE_PMA_ANA_LCPLL_ANA_LPF_R_SEL_LC1234_G3_MASK		((u32)0xFU << PCIE_PMA_ANA_LCPLL_ANA_LPF_R_SEL_LC1234_G3_SHIFT)

#define PCIE_PMA_ANA_LCPLL_ANA_LPF_R_SEL_LC1234_G4_SHIFT		(4U)
#define PCIE_PMA_ANA_LCPLL_ANA_LPF_R_SEL_LC1234_G4_MASK		((u32)0xFU << PCIE_PMA_ANA_LCPLL_ANA_LPF_R_SEL_LC1234_G4_SHIFT)

#define PCIE_PMA_TG_LCPLL_FINE_LOCK_DELAY_TIME_SHIFT		(3U)
#define PCIE_PMA_TG_LCPLL_FINE_LOCK_DELAY_TIME_MASK		((u32)0x7U << PCIE_PMA_TG_LCPLL_FINE_LOCK_DELAY_TIME_SHIFT)

#define PCIE_PMA_PLL_LOCK_SHIFT		(2U)
#define PCIE_PMA_PLL_LOCK_MASK		((u32)0x1U << PCIE_PMA_PLL_LOCK_SHIFT)

#define PCIE_PMA_RX_MARGIN_SCALE_SHIFT		(2U)
#define PCIE_PMA_RX_MARGIN_SCALE_MASK		((u32)0x3U << PCIE_PMA_RX_MARGIN_SCALE_SHIFT)

#define PCIE_PMA_LANE_SEQ_AES_EN_SHIFT		(6U)
#define PCIE_PMA_LANE_SEQ_AES_EN_MASK		((u32)0x1U << PCIE_PMA_LANE_SEQ_AES_EN_SHIFT)

#define PCIE_PMA_LN0_OV_S_ANA_TX_DRV_IDRV_EN_SHIFT		(1U)
#define PCIE_PMA_LN0_OV_S_ANA_TX_DRV_IDRV_EN_MASK		((u32)0x1U << PCIE_PMA_LN0_OV_S_ANA_TX_DRV_IDRV_EN_SHIFT)

#define PCIE_PMA_LN0_ANA_TX_RESERVED_SHIFT		(0U)
#define PCIE_PMA_LN0_ANA_TX_RESERVED_MASK		((u32)0xFFU << PCIE_PMA_LN0_ANA_TX_RESERVED_SHIFT)

#define PCIE_PMA_LN0_ANA_RX_SR_RESERVED_G3_SHIFT		(0U)
#define PCIE_PMA_LN0_ANA_RX_SR_RESERVED_G3_MASK		((u32)0xFFU << PCIE_PMA_LN0_ANA_RX_SR_RESERVED_G3_SHIFT)

#define PCIE_PMA_LN0_ANA_RX_SR_RESERVED_G4_SHIFT		(0U)
#define PCIE_PMA_LN0_ANA_RX_SR_RESERVED_G4_MASK		((u32)0xFFU << PCIE_PMA_LN0_ANA_RX_SR_RESERVED_G4_SHIFT)

#define PCIE_PMA_LN0_ANA_RX_CDR_CCO_VCI_AMP_I_CTRL_SHIFT		(2U)
#define PCIE_PMA_LN0_ANA_RX_CDR_CCO_VCI_AMP_I_CTRL_MASK		((u32)0x3U << PCIE_PMA_LN0_ANA_RX_CDR_CCO_VCI_AMP_I_CTRL_SHIFT)

#define PCIE_PMA_LN0_BER_ADAP_RX_START_CURSOR_G4_SHIFT		(0U)
#define PCIE_PMA_LN0_BER_ADAP_RX_START_CURSOR_G4_MASK		((u32)0x1FU << PCIE_PMA_LN0_BER_ADAP_RX_START_CURSOR_G4_SHIFT)

#define PCIE_PMA_LN0_RX_EQ_PH_0_1_TIMEOUT_G3_SHIFT		(0U)
#define PCIE_PMA_LN0_RX_EQ_PH_0_1_TIMEOUT_G3_MASK		((u32)0xFFU << PCIE_PMA_LN0_RX_EQ_PH_0_1_TIMEOUT_G3_SHIFT)

#define PCIE_PMA_LN0_RX_EQ_PH_2_3_TIMEOUT_G3_SHIFT		(0U)
#define PCIE_PMA_LN0_RX_EQ_PH_2_3_TIMEOUT_G3_MASK		((u32)0xFFU << PCIE_PMA_LN0_RX_EQ_PH_2_3_TIMEOUT_G3_SHIFT)

#define PCIE_PMA_LN0_RX_EQ_PH_0_1_TIMEOUT_G4_SHIFT		(0U)
#define PCIE_PMA_LN0_RX_EQ_PH_0_1_TIMEOUT_G4_MASK		((u32)0xFFU << PCIE_PMA_LN0_RX_EQ_PH_0_1_TIMEOUT_G4_SHIFT)

#define PCIE_PMA_LN0_RX_EQ_PH_2_3_TIMEOUT_G4_SHIFT		(0U)
#define PCIE_PMA_LN0_RX_EQ_PH_2_3_TIMEOUT_G4_MASK		((u32)0xFFU << PCIE_PMA_LN0_RX_EQ_PH_2_3_TIMEOUT_G4_SHIFT)

#define PCIE_PMA_LN0_OV_I_PMAD_RX_CTLE_RS_MF_CTRL_G4_SHIFT		(0U)
#define PCIE_PMA_LN0_OV_I_PMAD_RX_CTLE_RS_MF_CTRL_G4_MASK		((u32)0x1FU << PCIE_PMA_LN0_OV_I_PMAD_RX_CTLE_RS_MF_CTRL_G4_SHIFT)

#define PCIE_PMA_LN0_ANA_RX_RESERVED__7_0_SHIFT		(0U)
#define PCIE_PMA_LN0_ANA_RX_RESERVED__7_0_MASK		((u32)0xFFU << PCIE_PMA_LN0_ANA_RX_RESERVED__7_0_SHIFT)

#define PCIE_PMA_LN0_OV_S_PI_OFFSET_QCLK_SHIFT		(5U)
#define PCIE_PMA_LN0_OV_S_PI_OFFSET_QCLK_MASK		((u32)0x1U << PCIE_PMA_LN0_OV_S_PI_OFFSET_QCLK_SHIFT)

#define PCIE_PCS_PMA_CONTROL_VECTOR_MASK		(0xFFFFFU)

#define PCIE_CLK_CFG_PMS_PRESET		(0x201C8001U)
#define PCIE_FBUS_CLK_RATE		(333333333UL)
#define PCIE_PHY_CLK_RATE		(100000000UL)

struct ber_preset {
	s32 phase;
	u32 offset;
	u32 value;
};

static struct ber_preset ber_preset_table[] = {
	/* Preset 0 */
	{ 0, 0x5F8U, 0x00U, },
	{ 0, 0x600U, 0x23U, },
	{ 0, 0x604U, 0x44U, },
	{ 0, 0x608U, 0x61U, },
	{ 0, 0x60CU, 0x55U, },
	{ 0, 0x610U, 0x14U, },
	{ 0, 0x614U, 0x23U, },
	{ 0, 0x618U, 0x1AU, },
	{ 0, 0x61CU, 0x04U, },
	{ 0, 0x5F8U, 0x04U, },
	{ 0, 0x5F8U, 0x00U, },
	/* Preset 1 */
	{ 1, 0x5F8U, 0x08U, },
	{ 1, 0x604U, 0x42U, },
	{ 1, 0x5F8U, 0x0CU, },
	{ 1, 0x5F8U, 0x08U, },
	/* Preset 2 */
	{ 2, 0x5F8U, 0x10U, },
	{ 2, 0x604U, 0x40U, },
	{ 2, 0x5F8U, 0x14U, },
	{ 2, 0x5F8U, 0x10U, },
	/* Preset 3 */
	{ 3, 0x5F8U, 0x18U, },
	{ 3, 0x604U, 0x45U, },
	{ 3, 0x5F8U, 0x1CU, },
	{ 3, 0x5F8U, 0x18U, },
	/* Preset 4 */
	{ 4, 0x5F8U, 0x20U, },
	{ 4, 0x604U, 0x46U, },
	{ 4, 0x5F8U, 0x24U, },
	{ 4, 0x5F8U, 0x20U, },
	/* Preset 5 */
	{ 5, 0x5F8U, 0x28U, },
	{ 5, 0x604U, 0x48U, },
	{ 5, 0x5F8U, 0x2CU, },
	{ 5, 0x5F8U, 0x28U, },
	/* Preset 6 */
	{ 6, 0x5F8U, 0x30U, },
	{ 6, 0x604U, 0x4AU, },
	{ 6, 0x5F8U, 0x34U, },
	{ 6, 0x5F8U, 0x30U, },
	/* Preset 7 */
	{ 7, 0x5F8U, 0x38U, },
	{ 7, 0x604U, 0x4CU, },
	{ 7, 0x5F8U, 0x3CU, },
	{ 7, 0x5F8U, 0x38U, },
	/* Preset 8 */
	{ 8, 0x5F8U, 0x40U, },
	{ 8, 0x600U, 0x20U, },
	{ 8, 0x604U, 0x20U, },
	{ 8, 0x608U, 0x01U, },
	{ 8, 0x5F8U, 0x44U, },
	{ 8, 0x5F8U, 0x40U, },
	/* Preset 9 */
	{ 9, 0x5F8U, 0x48U, },
	{ 9, 0x600U, 0x20U, },
	{ 9, 0x604U, 0x21U, },
	{ 9, 0x608U, 0x01U, },
	{ 9, 0x5F8U, 0x4CU, },
	{ 9, 0x5F8U, 0x48U, },
	/* Preset A */
	{ 10, 0x5F8U, 0x50U, },
	{ 10, 0x600U, 0x26U, },
	{ 10, 0x604U, 0x80U, },
	{ 10, 0x608U, 0x41U, },
	{ 10, 0x60CU, 0xAFU, },
	{ 10, 0x610U, 0x26U, },
	{ 10, 0x614U, 0x34U, },
	{ 10, 0x618U, 0x24U, },
	{ 10, 0x61CU, 0x06U, },
	{ 10, 0x5F8U, 0x54U, },
	{ 10, 0x5F8U, 0x50U, },
	/* Preset B */
	{ 11, 0x5F8U, 0x58U, },
	{ 11, 0x604U, 0x81U, },
	{ 11, 0x5F8U, 0x5CU, },
	{ 11, 0x5F8U, 0x58U, },
	/* Preset C */
	{ 12, 0x5F8U, 0x60U, },
	{ 12, 0x604U, 0x82U, },
	{ 12, 0x5F8U, 0x64U, },
	{ 12, 0x5F8U, 0x60U, },
	/* Preset D */
	{ 13, 0x5F8U, 0x68U, },
	{ 13, 0x604U, 0x83U, },
	{ 13, 0x5F8U, 0x6CU, },
	{ 13, 0x5F8U, 0x68U, },
	/* Preset E */
	{ 14, 0x5F8U, 0x70U, },
	{ 14, 0x604U, 0x84U, },
	{ 14, 0x5F8U, 0x74U, },
	{ 14, 0x5F8U, 0x70U, },
	/* Preset F */
	{ 15, 0x5F8U, 0x78U, },
	{ 15, 0x600U, 0x26U, },
	{ 15, 0x604U, 0x85U, },
	{ 15, 0x608U, 0x80U, },
	{ 15, 0x60CU, 0x7FU, },
	{ 15, 0x610U, 0x2DU, },
	{ 15, 0x614U, 0x34U, },
	{ 15, 0x618U, 0x24U, },
	{ 15, 0x61CU, 0x05U, },
	{ 15, 0x5F8U, 0x7CU, },
	{ 15, 0x5F8U, 0x78U, },
	/* Preset 10 */
	{ 16, 0x5F8U, 0x80U, },
	{ 16, 0x604U, 0x86U, },
	{ 16, 0x5F8U, 0x84U, },
	{ 16, 0x5F8U, 0x80U, },
	/* Preset 11 */
	{ 17, 0x5F8U, 0x88U, },
	{ 17, 0x604U, 0x87U, },
	{ 17, 0x5F8U, 0x8CU, },
	{ 17, 0x5F8U, 0x88U, },
	/* Preset 12 */
	{ 18, 0x5F8U, 0x90U, },
	{ 18, 0x604U, 0x88U, },
	{ 18, 0x5F8U, 0x94U, },
	{ 18, 0x5F8U, 0x90U, },
	/* Preset 13 */
	{ 19, 0x5F8U, 0x98U, },
	{ 19, 0x604U, 0x89U, },
	{ 19, 0x5F8U, 0x9CU, },
	{ 19, 0x5F8U, 0x98U, },
};

struct sec08lpp_pcie_phy {
	struct device *dev;
	struct clk *phy_clk;
	struct clk *fbus_clk;
	void __iomem *phy_base;
	void __iomem *clk_base;
	void __iomem *pma_base;
	void __iomem *pcs_base;
	u32 mode;
};

static inline u32 sec08lpp_pcie_phy_readl(const void __iomem *base, u32 offset)
{
	u32 ret = 0x0U;

	if (base != NULL) {
		ret = ioread32(base + offset);
	}

	return ret;
}

static inline void sec08lpp_pcie_phy_writel(void __iomem *base, u32 offset, u32 val, u32 mask)
{
	if (base != NULL) {
		iowrite32((ioread32(base + offset) & ~mask)|val,
				base + offset);
	}
}

static s32 sec08lpp_pcie_phy_reset(struct phy *phy)
{
	s32 err = 0;

	if (phy != NULL) {
		const struct sec08lpp_pcie_phy *priv =
			(const struct sec08lpp_pcie_phy *)phy_get_drvdata(phy);
		u32 val, mask;

		/* assert tx and rx resets */
		val = 0x0U;
		mask = PCIE_PHY_REG_PCS_RSTN_MASK;
		sec08lpp_pcie_phy_writel(priv->phy_base, PCIE_PHY_REG00, val, mask);

		udelay(1);

		/* deassert tx, rx and global reset */
		mask = PCIE_PHY_REG_PCS_RSTN_MASK | PCIE_PHY_REG_PMA_INIT_RSTN_MASK;
		val = mask;
		sec08lpp_pcie_phy_writel(priv->phy_base, PCIE_PHY_REG00, val, mask);

		udelay(1);

		/* deassert port and common blk resets */
		mask = PCIE_PHY_REG_PMA_CMN_RSTN_MASK | PCIE_PHY_REG_PMA_PORT0_RSTN_MASK;
		val = mask;
		sec08lpp_pcie_phy_writel(priv->phy_base, PCIE_PHY_REG00, val, mask);

		udelay(500); //need to find proper delay
	} else {
		err = -ENODEV;
	}

	return err;
}

static s32 sec08lpp_pcie_phy_enable_clk(const struct sec08lpp_pcie_phy *priv)
{
	s32 err = 0;

	if (priv != NULL) {
		err = clk_prepare_enable(priv->fbus_clk);
		if (err == 0) {
			err = clk_prepare_enable(priv->phy_clk);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 sec08lpp_pcie_phy_disable_clk(const struct sec08lpp_pcie_phy *priv)
{
	s32 err = 0;

	if (priv != NULL) {
		clk_disable_unprepare(priv->phy_clk);
		clk_disable_unprepare(priv->fbus_clk);
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 sec08lpp_pcie_phy_pma_pll_lock(const struct sec08lpp_pcie_phy *priv)
{
	s32 err = 0;

	if (priv != NULL) {
		u32 val, mask;

		mask = PCIE_CLK_CFG_PLL_LOCK_MASK;
		val = sec08lpp_pcie_phy_readl(priv->clk_base, PCIE_CLK_CFG08) & mask;
		if (val == mask) {
			mask = PCIE_PMA_PLL_LOCK_MASK;
			while ((sec08lpp_pcie_phy_readl(priv->pma_base, PCIE_PMA_CMN_REG13C) & mask) != mask) {
				mdelay(10);
			}
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 sec08lpp_pcie_phy_disable_clkout(const struct sec08lpp_pcie_phy *priv)
{
	s32 err = 0;

	if (priv != NULL) {
		u32 val, mask;

		val = 0x0U;
		mask = PCIE_CLK_CFG_RESETB_MASK;
		sec08lpp_pcie_phy_writel(priv->clk_base, PCIE_CLK_CFG00, val, mask);
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 sec08lpp_pcie_phy_set_clk(const struct sec08lpp_pcie_phy *priv, u32 mode)
{
	s32 err = 0;

	if (priv != NULL) {
		err = sec08lpp_pcie_phy_enable_clk(priv);
		if (err == 0) {
			u32 val, mask;

			if ((mode & 0xF0U) == 0x0U) {
				/* RESERVED_CON b[20] */
				mask = (u32)BIT(20);
				val = 0x0U;
				sec08lpp_pcie_phy_writel(priv->clk_base, PCIE_CLK_CFG07, val, mask);

				mask = PCIE_CLK_CFG_RESETB_MASK;
				val = 0x0U;
				sec08lpp_pcie_phy_writel(priv->clk_base, PCIE_CLK_CFG00, val, mask);
				udelay(10);

				/*
				 * Skip PLL P,M,S settings
				 */

				mask = PCIE_CLK_CFG_BGR_EN_MASK;
				val = mask;
				sec08lpp_pcie_phy_writel(priv->clk_base, PCIE_CLK_CFG06, val, mask);
				udelay(100);

				mask = PCIE_CLK_CFG_RESETB_MASK;
				val = mask;
				sec08lpp_pcie_phy_writel(priv->clk_base, PCIE_CLK_CFG00, val, mask);

				mask = PCIE_CLK_CFG_PLL_LOCK_MASK;
				do {
					val = sec08lpp_pcie_phy_readl(priv->clk_base, PCIE_CLK_CFG08) & mask;
				} while (val == 0x0U);

				mask = PCIE_CLK_CFG_ERIO_ISO_ENB_MASK |
					PCIE_CLK_CFG_ERIO_PLL_LOCK_DONE_MASK |
					PCIE_CLK_CFG_ERIO_USE_LOCK_DONE_MASK |
					PCIE_CLK_CFG_ERIO_EN_MASK;
				val = mask;
				sec08lpp_pcie_phy_writel(priv->clk_base, PCIE_CLK_CFG04, val, mask);

				mdelay(15);
			} else {
				mask = PCIE_PHY_REG_LPLL_REF_CLK_SEL_MASK |
					PCIE_PHY_REG_PORT0_REF_CLK_EN_MASK;
				val = sec08lpp_pcie_phy_readl(priv->phy_base, PCIE_PHY_REG01) & ~mask;
				val |= ((u32)0x1U << PCIE_PHY_REG_PORT0_REF_CLK_EN_SHIFT);
				val |= (((mode & 0xF0U) >> 4U)  << PCIE_PHY_REG_LPLL_REF_CLK_SEL_SHIFT);
				sec08lpp_pcie_phy_writel(priv->phy_base, PCIE_PHY_REG01, val, mask);
			}
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 sec08lpp_pcie_phy_set_mode(struct phy *phy, enum phy_mode mode, s32 submode)
{
	s32 err = 0;

	if (phy != NULL) {
		struct sec08lpp_pcie_phy *priv = (struct sec08lpp_pcie_phy *)phy_get_drvdata(phy);

		if ((mode != PHY_MODE_PCIE) || (submode < 0)) {
			err = -EINVAL;
		} else {
			u8 clk_mode;

			if (!__builtin_add_overflow(submode, 0, &clk_mode)) {
				err = sec08lpp_pcie_phy_set_clk((const struct sec08lpp_pcie_phy *)priv, (u32)clk_mode);
				if (err == 0) {
					err = sec08lpp_pcie_phy_pma_pll_lock((const struct sec08lpp_pcie_phy *)priv);
				}

				if (err == 0) {
					priv->mode = (u32)clk_mode;
				}
			} else {
				err = -EINVAL;
			}
		}
	} else {
		err = -ENODEV;
	}

	return err;
}

static s32 sec08lpp_pcie_phy_pwr_down(const struct sec08lpp_pcie_phy *priv, bool enable)
{
	s32 err = 0;

	if (priv != NULL) {
		u32 val, mask;

		mask = PCIE_PHY_REG_PMA_POWER_OFF_MASK;
		val = enable ? mask : 0x0U;
		sec08lpp_pcie_phy_writel(priv->phy_base, PCIE_PHY_REG04, val, mask);
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 sec08lpp_pcie_phy_power_on(struct phy *phy)
{
	s32 err = 0;

	if (phy != NULL) {
		const struct sec08lpp_pcie_phy *priv =
			(const struct sec08lpp_pcie_phy *)phy_get_drvdata(phy);

		err = sec08lpp_pcie_phy_pwr_down(priv, false);
	} else {
		err = -ENODEV;
	}

	return err;
}

static s32 sec08lpp_pcie_phy_power_off(struct phy *phy)
{
	s32 err = 0;

	if (phy != NULL) {
		const struct sec08lpp_pcie_phy *priv =
			(const struct sec08lpp_pcie_phy *)phy_get_drvdata(phy);

		err = sec08lpp_pcie_phy_pwr_down(priv, true);
	} else {
		err = -ENODEV;
	}

	return err;
}

static s32 sec08lpp_pcie_phy_revert_to_default(const struct sec08lpp_pcie_phy *priv)
{
	s32 err = 0;

	if (priv != NULL) {
		u32 val, mask;

		mask = PCIE_PMA_LANE_SEQ_AES_EN_MASK;
		val = 0x0U;
		sec08lpp_pcie_phy_writel(priv->pma_base, PCIE_PMA_CMN_REG17B, val, mask);

		mask = PCIE_PMA_LN0_OV_S_ANA_TX_DRV_IDRV_EN_MASK;
		val = mask;
		sec08lpp_pcie_phy_writel(priv->pma_base, PCIE_PMA_TRSV_REG40B, val, mask);

		mask = PCIE_PMA_LN0_OV_S_PI_OFFSET_QCLK_MASK;
		val = 0x0U;
		sec08lpp_pcie_phy_writel(priv->pma_base, PCIE_PMA_TRSV_REG5FD, val, mask);
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 sec08lpp_pcie_phy_sfr_init(const struct sec08lpp_pcie_phy *priv)
{
	s32 err = 0;

	if (priv != NULL) {
		u32 val, mask, idx;

		mask = PCIE_PMA_ANA_LCPLL_AFC_VCO_CNT_RUN_NUM_MASK;
		val = ((u32)0x11U << PCIE_PMA_ANA_LCPLL_AFC_VCO_CNT_RUN_NUM_SHIFT);
		sec08lpp_pcie_phy_writel(priv->pma_base, PCIE_PMA_CMN_REG00B, val, mask);

		mask = PCIE_PMA_ANA_LCPLL_AVC_MAN_CAP_BIAS_CODE_MASK;
		val = ((u32)0x3U << PCIE_PMA_ANA_LCPLL_AVC_MAN_CAP_BIAS_CODE_SHIFT);
		sec08lpp_pcie_phy_writel(priv->pma_base, PCIE_PMA_CMN_REG016, val, mask);

		mask = PCIE_PMA_ANA_LCPLL_ANA_LPF_R_SEL_LC1234_G3_MASK;
		val = ((u32)0x6U << PCIE_PMA_ANA_LCPLL_ANA_LPF_R_SEL_LC1234_G3_SHIFT);
		sec08lpp_pcie_phy_writel(priv->pma_base, PCIE_PMA_CMN_REG024, val, mask);

		mask = PCIE_PMA_ANA_LCPLL_ANA_LPF_R_SEL_LC1234_G4_MASK;
		val = ((u32)0x6U << PCIE_PMA_ANA_LCPLL_ANA_LPF_R_SEL_LC1234_G4_SHIFT);
		sec08lpp_pcie_phy_writel(priv->pma_base, PCIE_PMA_CMN_REG025, val, mask);

		mask = PCIE_PMA_TG_LCPLL_FINE_LOCK_DELAY_TIME_MASK;
		val = ((u32)0x7U << PCIE_PMA_TG_LCPLL_FINE_LOCK_DELAY_TIME_SHIFT);
		sec08lpp_pcie_phy_writel(priv->pma_base, PCIE_PMA_CMN_REG10A, val, mask);

		mask = PCIE_PMA_RX_MARGIN_SCALE_MASK;
		val = ((u32)0x1U << PCIE_PMA_RX_MARGIN_SCALE_SHIFT);
		sec08lpp_pcie_phy_writel(priv->pma_base, PCIE_PMA_CMN_REG153, val, mask);

		mask = PCIE_PMA_LN0_ANA_TX_RESERVED_MASK;
		val = ((u32)0x30U << PCIE_PMA_LN0_ANA_TX_RESERVED_SHIFT);
		for (idx = 0U; idx < 4U; idx++) {
			sec08lpp_pcie_phy_writel(priv->pma_base,
					PCIE_PMA_TRSV_REG424 + (idx * 0x1000U),
					val,
					mask);
		}

		mask = PCIE_PMA_LN0_ANA_RX_SR_RESERVED_G3_MASK;
		val = ((u32)0x2U << PCIE_PMA_LN0_ANA_RX_SR_RESERVED_G3_SHIFT);
		for (idx = 0U; idx < 4U; idx++) {
			sec08lpp_pcie_phy_writel(priv->pma_base,
					PCIE_PMA_TRSV_REG47A + (idx * 0x1000U),
					val,
					mask);
		}

		mask = PCIE_PMA_LN0_ANA_RX_SR_RESERVED_G4_MASK;
		val = ((u32)0x4U << PCIE_PMA_LN0_ANA_RX_SR_RESERVED_G4_SHIFT);
		for (idx = 0U; idx < 4U; idx++) {
			sec08lpp_pcie_phy_writel(priv->pma_base,
					PCIE_PMA_TRSV_REG47B + (idx * 0x1000U),
					val,
					mask);
		}

		mask = PCIE_PMA_LN0_ANA_RX_CDR_CCO_VCI_AMP_I_CTRL_MASK;
		val = ((u32)0x0U << PCIE_PMA_LN0_ANA_RX_CDR_CCO_VCI_AMP_I_CTRL_SHIFT);
		for (idx = 0U; idx < 4U; idx++) {
			sec08lpp_pcie_phy_writel(priv->pma_base,
					PCIE_PMA_TRSV_REG47C + (idx * 0x1000U),
					val,
					mask);
		}

		mask = PCIE_PMA_LN0_BER_ADAP_RX_START_CURSOR_G4_MASK;
		val = ((u32)0xCU << PCIE_PMA_LN0_BER_ADAP_RX_START_CURSOR_G4_SHIFT);
		for (idx = 0U; idx < 4U; idx++) {
			sec08lpp_pcie_phy_writel(priv->pma_base,
					PCIE_PMA_TRSV_REG4CF + (idx * 0x1000U),
					val,
					mask);
		}

		mask = PCIE_PMA_LN0_RX_EQ_PH_0_1_TIMEOUT_G3_MASK;
		val = ((u32)0xE5U << PCIE_PMA_LN0_RX_EQ_PH_0_1_TIMEOUT_G3_SHIFT);
		for (idx = 0U; idx < 4U; idx++) {
			sec08lpp_pcie_phy_writel(priv->pma_base,
					PCIE_PMA_TRSV_REG516 + (idx * 0x1000U),
					val,
					mask);
		}

		mask = PCIE_PMA_LN0_RX_EQ_PH_2_3_TIMEOUT_G3_MASK;
		val = ((u32)0xB6U << PCIE_PMA_LN0_RX_EQ_PH_2_3_TIMEOUT_G3_SHIFT);
		for (idx = 0U; idx < 4U; idx++) {
			sec08lpp_pcie_phy_writel(priv->pma_base,
					PCIE_PMA_TRSV_REG517 + (idx * 0x1000U),
					val,
					mask);
		}

		mask = PCIE_PMA_LN0_RX_EQ_PH_0_1_TIMEOUT_G4_MASK;
		val = ((u32)0xF5U << PCIE_PMA_LN0_RX_EQ_PH_0_1_TIMEOUT_G4_SHIFT);
		for (idx = 0U; idx < 4U; idx++) {
			sec08lpp_pcie_phy_writel(priv->pma_base,
					PCIE_PMA_TRSV_REG518 + (idx * 0x1000U),
					val,
					mask);
		}

		mask = PCIE_PMA_LN0_RX_EQ_PH_2_3_TIMEOUT_G4_MASK;
		val = ((u32)0xC4U << PCIE_PMA_LN0_RX_EQ_PH_2_3_TIMEOUT_G4_SHIFT);
		for (idx = 0U; idx < 4U; idx++) {
			sec08lpp_pcie_phy_writel(priv->pma_base,
					PCIE_PMA_TRSV_REG519 + (idx * 0x1000U),
					val,
					mask);
		}

		mask = PCIE_PMA_LN0_OV_I_PMAD_RX_CTLE_RS_MF_CTRL_G4_MASK;
		val = ((u32)0x6U << PCIE_PMA_LN0_OV_I_PMAD_RX_CTLE_RS_MF_CTRL_G4_SHIFT);
		for (idx = 0U; idx < 4U; idx++) {
			sec08lpp_pcie_phy_writel(priv->pma_base,
					PCIE_PMA_TRSV_REG56D + (idx * 0x1000U),
					val,
					mask);
		}

		mask = PCIE_PMA_LN0_ANA_RX_RESERVED__7_0_MASK;
		val = ((u32)0x33U << PCIE_PMA_LN0_ANA_RX_RESERVED__7_0_SHIFT);
		for (idx = 0U; idx < 4U; idx++) {
			sec08lpp_pcie_phy_writel(priv->pma_base,
					PCIE_PMA_TRSV_REG5F1 + (idx * 0x1000U),
					val,
					mask);
		}

		mask = 0xFFFFFFFFU;
		for (idx = 0U; idx < (sizeof(ber_preset_table)/sizeof(struct ber_preset)); idx++) {
			if (ber_preset_table[idx].offset != mask) {
				sec08lpp_pcie_phy_writel(priv->pma_base,
						ber_preset_table[idx].offset,
						ber_preset_table[idx].value,
						mask);
			}
		}

		val = ((u32)0x1U << PCIE_PMA_IGNORE_ADAP_DONE_SHIFT);
		mask = PCIE_PMA_IGNORE_ADAP_DONE_MASK;
		sec08lpp_pcie_phy_writel(priv->pma_base, PCIE_PMA_CMN_REG130, val, mask);

		mask = PCIE_PCS_PMA_CONTROL_VECTOR_MASK;
		if((priv->mode & (u32)BIT(1)) != 0x0U) {
			val = 0x700D5U;
		} else {
			val = 0x700DDU;
		}
		sec08lpp_pcie_phy_writel(priv->pcs_base, PCIE_PCS_OUT_VEC_4, val, mask);
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 sec08lpp_pcie_phy_init(struct phy *phy)
{
	s32 err = 0;

	if (phy != NULL) {
		const struct sec08lpp_pcie_phy *priv =
			(const struct sec08lpp_pcie_phy *)phy_get_drvdata(phy);

		err = sec08lpp_pcie_phy_revert_to_default(priv);
		if (err == 0) {
			err = sec08lpp_pcie_phy_sfr_init(priv);
		}
	} else {
		err = -ENODEV;
	}

	return err;
}

static s32 sec08lpp_pcie_phy_exit(struct phy *phy)
{
	s32 err = 0;

	if (phy != NULL) {
		const struct sec08lpp_pcie_phy *priv =
			(const struct sec08lpp_pcie_phy *)phy_get_drvdata(phy);

		/* TODO */
		(void)priv;
	} else {
		err = -ENODEV;
	}

	return err;
}

static const struct phy_ops sec08lpp_pcie_phy_ops = {
	.init		= sec08lpp_pcie_phy_init,
	.exit		= sec08lpp_pcie_phy_exit,
	.power_on		= sec08lpp_pcie_phy_power_on,
	.power_off		= sec08lpp_pcie_phy_power_off,
	.reset		= sec08lpp_pcie_phy_reset,
	.set_mode		= sec08lpp_pcie_phy_set_mode,
	.owner		= THIS_MODULE,
};

static s32 sec08lpp_pcie_phy_suspend_late(struct device *dev)
{
	s32 err = 0;

	if (dev != NULL) {
		const struct sec08lpp_pcie_phy *priv =
			(const struct sec08lpp_pcie_phy *)dev_get_drvdata(dev);

		err = sec08lpp_pcie_phy_disable_clk(priv);
		if (err == 0) {
			err = sec08lpp_pcie_phy_disable_clkout(priv);
		}
	} else {
		err = -ENODEV;
	}

	return err;
}

static const struct dev_pm_ops sec08lpp_pcie_phy_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(sec08lpp_pcie_phy_suspend_late,
			NULL)
};

static const struct of_device_id sec08lpp_pcie_phy_id_table[] = {
	{
		.compatible = "samsung,ln08lpp-pcie-phy",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sec08lpp_pcie_phy_id_table);

static s32 sec08lpp_pcie_phy_of_parse_clk(struct platform_device *pdev, struct sec08lpp_pcie_phy *priv)
{
	s32 err = 0;

	if ((pdev != NULL) && (priv != NULL)) {
		priv->fbus_clk = devm_clk_get(&pdev->dev, "pcie_fbus");
		if (IS_ERR(priv->fbus_clk)) {
			err = (s32)PTR_ERR(priv->fbus_clk);
		} else {
			err = clk_set_rate(priv->fbus_clk, PCIE_FBUS_CLK_RATE);
		}

		if (err == 0) {
			priv->phy_clk = devm_clk_get(&pdev->dev, "pcie_phy");
			if (IS_ERR(priv->phy_clk)) {
				err = (s32)PTR_ERR(priv->phy_clk);
			} else {
				err = clk_set_rate(priv->phy_clk, PCIE_PHY_CLK_RATE);
			}
		}
	} else {
		err = -EINVAL;
	}
	return err;
}

static s32 sec08lpp_pcie_phy_of_parse_reg(struct platform_device *pdev, struct sec08lpp_pcie_phy *priv)
{
	s32 err = 0;

	if ((pdev != NULL) && (priv != NULL)) {
		struct resource *res;

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "phy");
		if (res != NULL) {
			priv->phy_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
			if (IS_ERR(priv->phy_base)) {
				err = (s32)PTR_ERR(priv->phy_base);
			}
		}

		if (err == 0) {
			res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "clk");
			if (res != NULL) {
				priv->clk_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
				if (IS_ERR(priv->clk_base)) {
					err = (s32)PTR_ERR(priv->clk_base);
				}
			}
		}

		if (err == 0) {
			res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pma");
			if (res != NULL) {
				priv->pma_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
				if (IS_ERR(priv->pma_base)) {
					err = (s32)PTR_ERR(priv->pma_base);
				}
			}
		}

		if (err == 0) {
			res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pcs");
			if (res != NULL) {
				priv->pcs_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
				if (IS_ERR(priv->pcs_base)) {
					err = (s32)PTR_ERR(priv->pcs_base);
				}
			}
		}
	} else {
		err = -EINVAL;
	}
	return err;
}

static s32 sec08lpp_pcie_phy_of_parse_dt(struct platform_device *pdev, struct sec08lpp_pcie_phy *priv)
{
	s32 err = 0;

	if ((pdev != NULL) &&
			(priv != NULL)) {
		err = sec08lpp_pcie_phy_of_parse_reg(pdev, priv);
		if (err == 0) {
			err = sec08lpp_pcie_phy_of_parse_clk(pdev, priv);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 sec08lpp_pcie_phy_register(struct sec08lpp_pcie_phy *priv)
{
	s32 err = 0;

	if (priv != NULL) {
		struct phy_provider *provider;
		struct phy *phy;

		phy = devm_phy_create(priv->dev, NULL, &sec08lpp_pcie_phy_ops);
		if (IS_ERR(phy)) {
			err = PTR_ERR(phy);
		} else {
			phy_set_drvdata(phy, priv);
			provider = devm_of_phy_provider_register(priv->dev,
					of_phy_simple_xlate);
			if (IS_ERR(provider)) {
				err = PTR_ERR(provider);
			}
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 sec08lpp_pcie_phy_probe(struct platform_device *pdev)
{
	struct sec08lpp_pcie_phy *priv;
	s32 err = 0;

	priv = (struct sec08lpp_pcie_phy *)devm_kzalloc(&pdev->dev,
			sizeof(struct sec08lpp_pcie_phy), GFP_KERNEL);
	if (priv != NULL) {
		err = sec08lpp_pcie_phy_of_parse_dt(pdev, priv);
		if (err == 0) {
			priv->dev = &pdev->dev;
			platform_set_drvdata(pdev, priv);
			err = sec08lpp_pcie_phy_register(priv);
		}

		if (err == 0) {
			pm_runtime_enable(priv->dev);
		}
	} else {
		err = -ENOMEM;
	}

	return err;
}

static s32 sec08lpp_pcie_phy_remove(struct platform_device *pdev)
{
	s32 err = 0;

	if (pdev != NULL) {
		const struct sec08lpp_pcie_phy *priv =
			(const struct sec08lpp_pcie_phy *)platform_get_drvdata(pdev);

		pm_runtime_disable(priv->dev);
		err = sec08lpp_pcie_phy_disable_clk(priv);
	} else {
		err = -ENODEV;
	}

	return err;
}

static struct platform_driver sec08lpp_pcie_phy_driver = {
	.probe		= sec08lpp_pcie_phy_probe,
	.remove		= sec08lpp_pcie_phy_remove,
	.driver		= {
		.name	= "sec-ln08lpp-pcie-phy",
		.pm	= &sec08lpp_pcie_phy_pm_ops,
		.of_match_table = of_match_ptr(sec08lpp_pcie_phy_id_table),
	},
};
module_platform_driver(sec08lpp_pcie_phy_driver);

MODULE_DESCRIPTION("Telechips Dolphin5 PCIe PHY Driver");
MODULE_LICENSE("GPL v2");
