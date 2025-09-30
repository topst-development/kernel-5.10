/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_MIPI_CSI2_DPHYS_REG_H
#define TCC_MIPI_CSI2_DPHYS_REG_H

/*
 * register offset
 */
/* BIAS */
#define PHY_BIAS_CON0				((uint32_t)0x0000U)
#define PHY_BIAS_CON1				((uint32_t)0x0004U)
#define PHY_BIAS_CON2				((uint32_t)0x0008U)
#define PHY_BIAS_CON3				((uint32_t)0x000CU)
#define PHY_BIAS_CON4				((uint32_t)0x0010U)
/* Slave Clock Lane */
#define PHY_SC_GNR_CON0				((uint32_t)0x0B00U)
#define PHY_SC_GNR_CON1				((uint32_t)0x0B04U)
#define PHY_SC_ANA_CON0				((uint32_t)0x0B08U)
#define PHY_SC_ANA_CON1				((uint32_t)0x0B0CU)
#define PHY_SC_ANA_CON2				((uint32_t)0x0B10U)
#define PHY_SC_ANA_CON3				((uint32_t)0x0B14U)
#define PHY_SC_ANA_CON4				((uint32_t)0x0B18U)
#define PHY_SC_ANA_CON5				((uint32_t)0x0B1CU)
#define PHY_SC_TIME_CON0			((uint32_t)0x0B30U)
#define PHY_SC_TEST_CON0			((uint32_t)0x0B70U)
#define PHY_SC_TEST_CON1			((uint32_t)0x0B74U)
#define PHY_SC_DBG_STAT0			((uint32_t)0x0BE0U)
#define PHY_SC_DBG_STAT1			((uint32_t)0x0BE4U)
#define PHY_SC_PPI_STAT0			((uint32_t)0x0BE8U)
#define PHY_SC_ADI_STAT0			((uint32_t)0x0BECU)
/* Slave Data Lane */
#define PHY_SD_GNR_CON0				((uint32_t)0x0C00U)
#define PHY_SD_GNR_CON1				((uint32_t)0x0C04U)
#define PHY_SD_ANA_CON0				((uint32_t)0x0C08U)
#define PHY_SD_ANA_CON1				((uint32_t)0x0C0CU)
#define PHY_SD_ANA_CON2				((uint32_t)0x0C10U)
#define PHY_SD_ANA_CON3				((uint32_t)0x0C14U)
#define PHY_SD_ANA_CON4				((uint32_t)0x0C18U)
#define PHY_SD_ANA_CON5				((uint32_t)0x0C1CU)
#define PHY_SD_ANA_CON6				((uint32_t)0x0C20U)
#define PHY_SD_ANA_CON7				((uint32_t)0x0C24U)
#define PHY_SD_TIME_CON0			((uint32_t)0x0C30U)
#define PHY_SD_TIME_CON1			((uint32_t)0x0C34U)
#define PHY_SD_DATA_CON0			((uint32_t)0x0C38U)
#define PHY_SD_DESKEW_CON0			((uint32_t)0x0C40U)
#define PHY_SD_DESKEW_CON1			((uint32_t)0x0C44U)
#define PHY_SD_DESKEW_CON2			((uint32_t)0x0C48U)
#define PHY_SD_DESKEW_CON3			((uint32_t)0x0C4CU)
#define PHY_SD_DESKEW_CON4			((uint32_t)0x0C50U)
#define PHY_SD_ETCR_CON0			((uint32_t)0x0C60U)
#define PHY_SD_ETCR_CON1			((uint32_t)0x0C64U)
#define PHY_SD_ETCR_CON2			((uint32_t)0x0C68U)
#define PHY_SD_TEST_CON0			((uint32_t)0x0C70U)
#define PHY_SD_TEST_CON1			((uint32_t)0x0C74U)
#define PHY_SD_TEST_CON2			((uint32_t)0x0C78U)
#define PHY_SD_TEST_CON3			((uint32_t)0x0C7CU)
#define PHY_SD_TEST_CON4			((uint32_t)0x0C80U)
#define PHY_SD_TEST_CON5			((uint32_t)0x0C84U)
#define PHY_SD_TEST_CON6			((uint32_t)0x0C88U)
#define PHY_SD_BIST_CON0			((uint32_t)0x0C90U)
#define PHY_SD_BIST_CON1			((uint32_t)0x0C94U)
#define PHY_SD_BIST_CON2			((uint32_t)0x0C98U)
#define PHY_SD_PKT_MON_CON0			((uint32_t)0x0CA0U)
#define PHY_SD_PKT_STAT0			((uint32_t)0x0CB0U)
#define PHY_SD_PKT_STAT1			((uint32_t)0x0CB4U)
#define PHY_SD_PKT_STAT2			((uint32_t)0x0CB8U)
#define PHY_SD_PKT_STAT3			((uint32_t)0x0CBCU)
#define PHY_SD_PKT_STAT4			((uint32_t)0x0CC0U)
#define PHY_SD_PKT_STAT5			((uint32_t)0x0CC4U)
#define PHY_SD_PKT_STAT6			((uint32_t)0x0CC8U)
#define PHY_SD_PKT_STAT7			((uint32_t)0x0CCCU)
#define PHY_SD_PKT_STAT8			((uint32_t)0x0CD0U)
#define PHY_SD_PKT_STAT9			((uint32_t)0x0CD4U)
#define PHY_SD_PKT_STAT10			((uint32_t)0x0CD8U)
#define PHY_SD_PKT_STAT11			((uint32_t)0x0CDCU)
#define PHY_SD_PHY_DBG_STAT0			((uint32_t)0x0CE0U)
#define PHY_SD_PHY_DBG_STAT1			((uint32_t)0x0CE4U)
#define PHY_SD_PHY_PPI_STAT0			((uint32_t)0x0CE8U)
#define PHY_SD_DPHY_ADI_STAT0			((uint32_t)0x0CECU)


/* GNR_CON1 (D-PHY Slave 0 Clock General Control Register 1) */
#define	PHY_SC_GNR_CON1_T_PHY_READY_SHIFT	((uint32_t)0U)

#define PHY_SC_GNR_CON1_T_PHY_READY_MASK	\
		(((uint32_t)0xFFFFU) << PHY_SC_GNR_CON1_T_PHY_READY_SHIFT)

/*
 * SD_ANA_CON2
 * (DC-PHY Combo Slave 0 Data 0 Lane Analog Block Control Register 2)
 */
#define PHY_SD_ANA_CON2_SKEW_DLYSEL_SHIFT	((uint32_t)8U)

#define PHY_SD_ANA_CON2_SKEW_DLYSEL_MASK	\
		(((uint32_t)0x3U) << PHY_SD_ANA_CON2_SKEW_DLYSEL_SHIFT)

/* SD_TIME_CON0 (DC-PHY Combo Slave 0 Data 0 Lane Timing Control Register 0) */
#define PHY_SD_TIME_CON0_T_HS_SETTLE_SHIFT	((uint32_t)0U)
#define PHY_SD_TIME_CON0_SETTLE_CLK_SEL_SHIFT	((uint32_t)8U)

#define PHY_SD_TIME_CON0_T_HS_SETTLE_MASK	\
		(((uint32_t)0xFFU) << PHY_SD_TIME_CON0_T_HS_SETTLE_SHIFT)
#define PHY_SD_TIME_CON0_SETTLE_CLK_SEL_MASK	\
		(((uint32_t)0x1U) << PHY_SD_TIME_CON0_SETTLE_CLK_SEL_SHIFT)

/*
 * SD_DESKEW_CON0
 * (DC-PHY Combo Slave 0 Data 0 Lane Skew Calibration Control Register 0)
 */
#define PHY_SD_DESKEW_CON0_SKEW_CAL_EN_SHIFT		((uint32_t)0U)
#define PHY_SD_DESKEW_CON0_SKEW_CAL_DES_SEL_SHIFT	((uint32_t)4U)

#define PHY_SD_DESKEW_CON0_SKEW_CAL_EN_MASK		\
		(((uint32_t)0x1U) << PHY_SD_DESKEW_CON0_SKEW_CAL_EN_SHIFT)
#define PHY_SD_DESKEW_CON0_SKEW_CAL_DES_SEL_MASK	\
		(((uint32_t)0x1U) << PHY_SD_DESKEW_CON0_SKEW_CAL_DES_SEL_SHIFT)

#endif
