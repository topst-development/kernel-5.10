/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_MIPI_CFG_REG_H
#define TCC_MIPI_CFG_REG_H

#define TCC_MIPI_RESET_RELEASE			(0U)
#define TCC_MIPI_RESET_RESET			(1U)

/*
 * MIPI CFG BASE
 */
#define CAM_SWRST				((uint32_t)0x000U)
#define CAM_CLKMSK				((uint32_t)0x004U)
#define MIPI_PHY_PLL_0				((uint32_t)0x008U)
#define MIPI_PHY_PLL_1				((uint32_t)0x00CU)
#define MIPI_PHY_PLL_2				((uint32_t)0x010U)
#define MIPI_PHY_MCTRL0				((uint32_t)0x014U)
#define MIPI_PHY_MCTRL1				((uint32_t)0x018U)
#define MIPI_PHY_MCTRL2				((uint32_t)0x01CU)
#define MIPI_PHY_MCTRL3				((uint32_t)0x020U)
#define MIPI_PHY_MCTRL4				((uint32_t)0x024U)
#define MIPI_PHY_SCTRL0				((uint32_t)0x028U)
#define MIPI_PHY_SCTRL1				((uint32_t)0x02CU)
#define MIPI_PHY_SCTRL2				((uint32_t)0x030U)
#define MIPI_PHY_SCTRL3				((uint32_t)0x034U)
#define MIPI_PHY_SCTRL4				((uint32_t)0x038U)
#define MIPI_PHY_SCTRL5				((uint32_t)0x03CU)
#define MIPI_PHY_SCTRL6				((uint32_t)0x040U)
#define MIPI_DSI_CTRL				((uint32_t)0x050U)
#define MIPI_CSI_CTRL0				((uint32_t)0x054U)
#define MIPI_CSI_CTRL1				((uint32_t)0x058U)
#define MIPI_CSI_CTRL2				((uint32_t)0x05CU)
#define ISP_CTRL0				((uint32_t)0x078U)
#define ISP_CTRL1				((uint32_t)0x07CU)
#define X2X_SLV_QCH_CC				((uint32_t)0x0A4U)
#define X2X_SLV_QCH_PC				((uint32_t)0x0A8U)
#define IREQ_CTRL				((uint32_t)0x140U)
#define IREQ_CIED_CORE_STAT			((uint32_t)0x144U)
#define IREQ_CIED_RAW_STAT			((uint32_t)0x148U)
#define ISP_ECC_CTRL0				((uint32_t)0x280U)
#define ISP_ECC_CTRL1				((uint32_t)0x284U)
#define ISP_ECC_CTRL2				((uint32_t)0x288U)
#define ISP_ECC_CTRL3				((uint32_t)0x28CU)
#define ISP_ECC_CTRL4				((uint32_t)0x290U)
#define ISP_ECC_CTRL5				((uint32_t)0x294U)
#define ISP_ECC_STS0				((uint32_t)0x2A8U)
#define ISP_ECC_STS1				((uint32_t)0x2ACU)
#define ISP_ECC_STS2				((uint32_t)0x2B0U)
#define ISP_ECC_STS3				((uint32_t)0x2B4U)


/*
 * CAM_SWRST
 */
/* MIPI_PHY_S_RESETN */
#define CAM_SWRST_MPSR_SHIFT			((uint32_t)3U)
/* MIPI_CSI_SWRST */
#define CAM_SWRST_MCS_SHIFT			((uint32_t)4U)
/* ISP_RISCV_SWRST */
#define CAM_SWRST_IRS_SHIFT			((uint32_t)9U)

#define CAM_SWRST_MPSR_MASK			\
		(((uint32_t)0x1U) << CAM_SWRST_MPSR_SHIFT)
#define CAM_SWRST_MCS_MASK			\
		(((uint32_t)0x1U) << CAM_SWRST_MCS_SHIFT)
#define CAM_SWRST_IRS_MASK			\
		(((uint32_t)0x1U) << CAM_SWRST_IRS_SHIFT)

/*
 * MIPI_PHY_SCTRL6
 */
#define MIPI_PHY_SCTRL5_HSSETTLE_SHIFT		((uint32_t)0U)
#define MIPI_PHY_SCTRL5_S_CLKSETTLECTL_SHIFT	((uint32_t)8U)

#define MIPI_PHY_SCTRL5_HSSETTLE_MASK		(GENMASK( 0,  7))
#define MIPI_PHY_SCTRL5_S_CLKSETTLECTL_MASK	(GENMASK( 8,  9))


/*
 * MIPI_PHY_SCTRL6
 */
#define MIPI_PHY_SCTRL6_S_DPDN_SWAP_DAT_SHIFT	((uint32_t)0U)
#define MIPI_PHY_SCTRL6_S_DPDN_SWAP_CLK_SHIFT	((uint32_t)1U)
#define MIPI_PHY_SCTRL6_ENABLE_CLK_SHIFT	((uint32_t)8U)
#define MIPI_PHY_SCTRL6_CSI_RXSKEW_EN_SHIFT	((uint32_t)16U)

#define MIPI_PHY_SCTRL6_S_DPDN_SWAP_DAT_MASK	(GENMASK( 0,  0))
#define MIPI_PHY_SCTRL6_S_DPDN_SWAP_CLK_MASK	(GENMASK( 1,  1))
#define MIPI_PHY_SCTRL6_ENABLE_CLK_MASK		(GENMASK( 8,  8))
#define MIPI_PHY_SCTRL6_CSI_RXSKEW_EN_MASK	(GENMASK(16, 16))

/*
 * MIPI_CSI_CTRL0
 */
/* CSI_VSYNC_INVx */
#define MIPI_CSI_CTRL0_CVI0_SHIFT		((uint32_t)0U)
#define MIPI_CSI_CTRL0_CVI1_SHIFT		((uint32_t)1U)
#define MIPI_CSI_CTRL0_CVI2_SHIFT		((uint32_t)2U)
#define MIPI_CSI_CTRL0_CVI3_SHIFT		((uint32_t)3U)
/* CSI_HSYNC_INVX */
#define MIPI_CSI_CTRL0_CHI0_SHIFT		((uint32_t)4U)
#define MIPI_CSI_CTRL0_CHI1_SHIFT		((uint32_t)5U)
#define MIPI_CSI_CTRL0_CHI2_SHIFT		((uint32_t)6U)
#define MIPI_CSI_CTRL0_CHI3_SHIFT		((uint32_t)7U)

#define MIPI_CSI_CTRL0_CVI0_MASK		\
		(((uint32_t)1U) << MIPI_CSI_CTRL0_CVI0_SHIFT)
#define MIPI_CSI_CTRL0_CVI1_MASK		\
		(((uint32_t)1U) << MIPI_CSI_CTRL0_CVI1_SHIFT)
#define MIPI_CSI_CTRL0_CVI2_MASK		\
		(((uint32_t)1U) << MIPI_CSI_CTRL0_CVI2_SHIFT)
#define MIPI_CSI_CTRL0_CVI3_MASK		\
		(((uint32_t)1U) << MIPI_CSI_CTRL0_CVI3_SHIFT)
#define MIPI_CSI_CTRL0_CHI0_MASK		\
		(((uint32_t)1U) << MIPI_CSI_CTRL0_CHI0_SHIFT)
#define MIPI_CSI_CTRL0_CHI1_MASK		\
		(((uint32_t)1U) << MIPI_CSI_CTRL0_CHI1_SHIFT)
#define MIPI_CSI_CTRL0_CHI2_MASK		\
		(((uint32_t)1U) << MIPI_CSI_CTRL0_CHI2_SHIFT)
#define MIPI_CSI_CTRL0_CHI3_MASK		\
		(((uint32_t)1U) << MIPI_CSI_CTRL0_CHI3_SHIFT)


/*
 * MIPI_CSI_CTRL1
 */
/* CAM_FRONT_CHMUXx */
#define MIPI_CSI_CTRL1_CFCH0_SHIFT		((uint32_t)0U)
#define MIPI_CSI_CTRL1_CFCH1_SHIFT		((uint32_t)3U)
#define MIPI_CSI_CTRL1_CFCH2_SHIFT		((uint32_t)6U)
#define MIPI_CSI_CTRL1_CFCH3_SHIFT		((uint32_t)9U)
#define MIPI_CSI_CTRL1_CFCH4_SHIFT		((uint32_t)12U)

#define MIPI_CSI_CTRL1_CFCH0_MASK		\
		(((uint32_t)0x3U) << MIPI_CSI_CTRL1_CFCH0_SHIFT)
#define MIPI_CSI_CTRL1_CFCH1_MASK		\
		(((uint32_t)0x3U) << MIPI_CSI_CTRL1_CFCH1_SHIFT)
#define MIPI_CSI_CTRL1_CFCH2_MASK		\
		(((uint32_t)0x3U) << MIPI_CSI_CTRL1_CFCH2_SHIFT)
#define MIPI_CSI_CTRL1_CFCH3_MASK		\
		(((uint32_t)0x3U) << MIPI_CSI_CTRL1_CFCH3_SHIFT)
#define MIPI_CSI_CTRL1_CFCH4_MASK		\
		(((uint32_t)0x3U) << MIPI_CSI_CTRL1_CFCH4_SHIFT)

/* CAM CH MUX 4X1 */
#define CSI_CFG_MIPI_CHMUX_MAX			((uint32_t)4U)

/*
 * MIPI_CSI_CTRL2
 */
#define MIPI_CSI_CTRL2_MCI1D_SHIFT		((uint32_t)0U)
#define MIPI_CSI_CTRL2_MCI1M_SHIFT		((uint32_t)7U)
#define MIPI_CSI_CTRL2_MCI2D_SHIFT		((uint32_t)8U)
#define MIPI_CSI_CTRL2_MCI2M_SHIFT		((uint32_t)15U)
#define MIPI_CSI_CTRL2_MCI3D_SHIFT		((uint32_t)16U)
#define MIPI_CSI_CTRL2_MCI3M_SHIFT		((uint32_t)23U)
#define MIPI_CSI_CTRL2_MCI4D_SHIFT		((uint32_t)24U)
#define MIPI_CSI_CTRL2_MCI4M_SHIFT		((uint32_t)31U)

#define MIPI_CSI_CTRL2_MCI1D_MASK		(GENMASK( 5, 0))
#define MIPI_CSI_CTRL2_MCI1M_MASK		(GENMASK( 7, 7))
#define MIPI_CSI_CTRL2_MCI2D_MASK		(GENMASK( 8, 13))
#define MIPI_CSI_CTRL2_MCI2M_MASK		(GENMASK(15, 15))
#define MIPI_CSI_CTRL2_MCI3D_MASK		(GENMASK(16, 21))
#define MIPI_CSI_CTRL2_MCI3M_MASK		(GENMASK(23, 23))
#define MIPI_CSI_CTRL2_MCI4D_MASK		(GENMASK(24, 29))
#define MIPI_CSI_CTRL2_MCI4M_MASK		(GENMASK(31, 31))

/*
 * ISP_CTRL_0
 */
/* ISP_SLEEP_MODE */
#define ISP_CTRL_0_ISM_SHIFT			((uint32_t)0U)
/* ISP_MEM_PROTECT */
#define ISP_CTRL_0_IMP_SHIFT			((uint32_t)1U)
/* ISPx_BYPASS */
#define ISP_CTRL_0_I0B_SHIFT			((uint32_t)16U)
#define ISP_CTRL_0_I1B_SHIFT			((uint32_t)17U)
#define ISP_CTRL_0_I2B_SHIFT			((uint32_t)18U)
#define ISP_CTRL_0_I3B_SHIFT			((uint32_t)19U)
/* ISP_BASE_ADDR */
#define ISP_CTRL_0_IBA_SHIFT			((uint32_t)20U)

#define ISP_CTRL_0_ISM_MASK			\
		(((uint32_t)0x1U) << ISP_CTRL_0_ISM_SHIFT)
#define ISP_CTRL_0_IMP_MASK			\
		(((uint32_t)0x1U) << ISP_CTRL_0_IMP_SHIFT)
#define ISP_CTRL_0_I0B_MASK			\
		(((uint32_t)0x1U) << ISP_CTRL_0_I0B_SHIFT)
#define ISP_CTRL_0_I1B_MASK			\
		(((uint32_t)0x1U) << ISP_CTRL_0_I1B_SHIFT)
#define ISP_CTRL_0_I2B_MASK			\
		(((uint32_t)0x1U) << ISP_CTRL_0_I2B_SHIFT)
#define ISP_CTRL_0_I3B_MASK			\
		(((uint32_t)0x1U) << ISP_CTRL_0_I3B_SHIFT)
#define ISP_CTRL_0_IBA_MASK			\
		(((uint32_t)0xFFFU) << ISP_CTRL_0_IBA_SHIFT)

#define ISP_CTRL_0_IBA				((uint32_t)0x160U)


/* ISP_CTRL_1 */
#define ISP_CTRL_1_I0IF_SHIFT			((uint32_t)0U)
#define ISP_CTRL_1_I1IF_SHIFT			((uint32_t)8U)
#define ISP_CTRL_1_I2IF_SHIFT			((uint32_t)16U)
#define ISP_CTRL_1_I3IF_SHIFT			((uint32_t)24U)

#define ISP_CTRL_1_I0IF_MASK			(GENMASK( 0,  5))
#define ISP_CTRL_1_I1IF_MASK			(GENMASK( 8, 13))
#define ISP_CTRL_1_I2IF_MASK			(GENMASK(16, 21))
#define ISP_CTRL_1_I3IF_MASK			(GENMASK(24, 29))

#define ISP_CTRL_1_IXIF_RAW8_TO_RAW14		((uint32_t)0x0U)
#define ISP_CTRL_1_IXIF_RAW10_TO_RAW14		((uint32_t)0x4U)
#define ISP_CTRL_1_IXIF_RAW12_TO_RAW14		((uint32_t)0x8U)
#define ISP_CTRL_1_IXIF_RAW12_TO_RAW16		((uint32_t)0xCU)

/* ISP CH MUX 2X1 */
#define CSI_CFG_ISP_BYPASS_MAX				((uint32_t)4U)

#endif
