/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_MIPI_CSI2_CSIS_REG_H
#define TCC_MIPI_CSI2_CSIS_REG_H

#define MAX_VC				((uint32_t)4U)
#define MAX_PIXEL_MODE			((uint32_t)3U)

/* YUV */
#define CSI_DT_YUV420_8BIT		((uint32_t)0x18U)
#define CSI_DT_YUV420_10BIT		((uint32_t)0x19U)
#define CSI_DT_YUV420_8BIT_LEGACY	((uint32_t)0x1AU)
#define CSI_DT_YUV420_8BIT_CSPS		((uint32_t)0x1CU)
#define CSI_DT_YUV420_10BIT_CSPS	((uint32_t)0x1DU)
#define CSI_DT_YUV422_8BIT		((uint32_t)0x1EU)
#define CSI_DT_YUV422_10BIT		((uint32_t)0x1FU)
/* RGB */
#define CSI_DT_RGB444			((uint32_t)0x20U)
#define CSI_DT_RGB555			((uint32_t)0x21U)
#define CSI_DT_RGB565			((uint32_t)0x22U)
#define CSI_DT_RGB666			((uint32_t)0x23U)
#define CSI_DT_RGB888			((uint32_t)0x24U)
/* RAW */
#define CSI_DT_RAW6			((uint32_t)0x28U)
#define CSI_DT_RAW7			((uint32_t)0x29U)
#define CSI_DT_RAW8			((uint32_t)0x2AU)
#define CSI_DT_RAW10			((uint32_t)0x2BU)
#define CSI_DT_RAW12			((uint32_t)0x2CU)
#define CSI_DT_RAW14			((uint32_t)0x2DU)
/* USER DEFINE */
#define CSI_DT_USER_DEFINE1		((uint32_t)0x30U)
#define CSI_DT_USER_DEFINE2		((uint32_t)0x31U)
#define CSI_DT_USER_DEFINE3		((uint32_t)0x32U)
#define CSI_DT_USER_DEFINE4		((uint32_t)0x33U)
#define CSI_DT_USER_DEFINE5		((uint32_t)0x34U)
#define CSI_DT_USER_DEFINE6		((uint32_t)0x35U)
#define CSI_DT_USER_DEFINE7		((uint32_t)0x36U)
#define CSI_DT_USER_DEFINE8		((uint32_t)0x37U)


/*
 * register offset
 */
#define CSIS_VERSION		((uint32_t)0x0000U)
#define CSIS_CMN_CTRL		((uint32_t)0x0004U)
#define CSIS_CLK_CTRL		((uint32_t)0x0008U)
#define CSIS_INT_MSK0		((uint32_t)0x0010U)
#define CSIS_INT_SRC0		((uint32_t)0x0014U)
#define CSIS_INT_MSK1		((uint32_t)0x0018U)
#define CSIS_INT_SRC1		((uint32_t)0x001CU)
#define DPHY_STATUS		((uint32_t)0x0020U)
#define DPHY_CMN_CTRL		((uint32_t)0x0024U)
#define DPHY_BCTRL_L		((uint32_t)0x0030U)
#define DPHY_BCTRL_H		((uint32_t)0x0034U)
#define DPHY_SCTRL_L		((uint32_t)0x0038U)
#define DPHY_SCTRL_H		((uint32_t)0x003CU)
#define ISP_CONFIG_CH0		((uint32_t)0x0040U)
#define ISP_RESOL_CH0		((uint32_t)0x0044U)
#define ISP_SYNC_CH0		((uint32_t)0x0048U)
#define ISP_CONFIG_CH1		((uint32_t)0x0050U)
#define ISP_RESOL_CH1		((uint32_t)0x0054U)
#define ISP_SYNC_CH1		((uint32_t)0x0058U)
#define ISP_CONFIG_CH2		((uint32_t)0x0060U)
#define ISP_RESOL_CH2		((uint32_t)0x0064U)
#define ISP_SYNC_CH2		((uint32_t)0x0068U)
#define ISP_CONFIG_CH3		((uint32_t)0x0070U)
#define ISP_RESOL_CH3		((uint32_t)0x0074U)
#define ISP_SYNC_CH3		((uint32_t)0x0078U)
#define SDW_CONFIG_CH0		((uint32_t)0x0080U)
#define SDW_RESOL_CH0		((uint32_t)0x0084U)
#define SDW_SYNC_CH0		((uint32_t)0x0088U)
#define SDW_CONFIG_CH1		((uint32_t)0x0090U)
#define SDW_RESOL_CH1		((uint32_t)0x0094U)
#define SDW_SYNC_CH1		((uint32_t)0x0098U)
#define SDW_CONFIG_CH2		((uint32_t)0x00A0U)
#define SDW_RESOL_CH2		((uint32_t)0x00A4U)
#define SDW_SYNC_CH2		((uint32_t)0x00A8U)
#define SDW_CONFIG_CH3		((uint32_t)0x00B0U)
#define SDW_RESOL_CH3		((uint32_t)0x00B4U)
#define SDW_SYNC_CH3		((uint32_t)0x00B8U)
#define FRM_CNT_CH0		((uint32_t)0x0100U)
#define FRM_CNT_CH1		((uint32_t)0x0104U)
#define FRM_CNT_CH2		((uint32_t)0x0108U)
#define FRM_CNT_CH3		((uint32_t)0x010CU)
#define LINE_INTR_CH0		((uint32_t)0x0110U)
#define LINE_INTR_CH1		((uint32_t)0x0114U)
#define LINE_INTR_CH2		((uint32_t)0x0118U)
#define LINE_INTR_CH3		((uint32_t)0x011CU)

#define GDB_RDATA		((uint32_t)0x0000U)
#define GDB_CNT			((uint32_t)0x0004U)
#define GDB_STATUS		((uint32_t)0x0008U)
#define GDB_INT			((uint32_t)0x000CU)
#define GDB_INTM		((uint32_t)0x0010U)
#define GDB_NUM			((uint32_t)0x0014U)
#define VC0_DT_EN0		((uint32_t)0x0040U)
#define VC0_DT_EN1		((uint32_t)0x0044U)
#define VC1_DT_EN0		((uint32_t)0x0048U)
#define VC1_DT_EN1		((uint32_t)0x004CU)
#define VC2_DT_EN0		((uint32_t)0x0050U)
#define VC2_DT_EN1		((uint32_t)0x0054U)
#define VC3_DT_EN0		((uint32_t)0x0058U)
#define VC3_DT_EN1		((uint32_t)0x005CU)

#define CSIS_VERSION_SHIFT		((uint32_t)0U)

#define CSIS_VERSION_MASK		\
		(((uint32_t)0xFFFFFFFFU) << CSIS_VERSION_SHIFT)

/*
 * 5.2	CSIS Common Control register
 */
#define CCTRL_UPDATE_SHADOW_SHIFT	((uint32_t)16U)
#define CCTRL_DESKEW_LEVEL_SHIFT	((uint32_t)13U)
#define CCTRL_DESKEW_ENABLE_SHIFT	((uint32_t)12U)
#define CCTRL_INTERLEAVE_MODE_SHIFT	((uint32_t)10U)
#define CCTRL_LANE_NUMBER_SHIFT		((uint32_t)8U)
#define CCTRL_UPDATE_SHADOW_CTRL_SHIFT	((uint32_t)2U)
#define CCTRL_SW_RESET_SHIFT		((uint32_t)1U)
#define CCTRL_CSI_EN_SHIFT		((uint32_t)0U)

#define CCTRL_UPDATE_SHADOW_MASK	\
		(((uint32_t)0xFU) << CCTRL_UPDATE_SHADOW_SHIFT)
#define CCTRL_DESKEW_LEVEL_MASK		\
		(((uint32_t)0x7U) << CCTRL_DESKEW_LEVEL_SHIFT)
#define CCTRL_DESKEW_ENABLE_MASK	\
		(((uint32_t)0x1U) << CCTRL_DESKEW_ENABLE_SHIFT)
#define CCTRL_INTERLEAVE_MODE_MASK	\
		(((uint32_t)0x3U) << CCTRL_INTERLEAVE_MODE_SHIFT)
#define CCTRL_LANE_NUMBER_MASK		\
		(((uint32_t)0x3U) << CCTRL_LANE_NUMBER_SHIFT)
#define CCTRL_UPDATE_SHADOW_CTRL_MASK	\
		(((uint32_t)0x1U) << CCTRL_UPDATE_SHADOW_CTRL_SHIFT)
#define CCTRL_SW_RESET_MASK		\
		(((uint32_t)0x1U) << CCTRL_SW_RESET_SHIFT)
#define CCTRL_CSI_EN_MASK		\
		(((uint32_t)0x1U) << CCTRL_CSI_EN_SHIFT)


/*
 * 5.3	CSIS Clock Control register
 */
#define CCTRL_CLKGATE_TRAIL_SHIFT	((uint32_t)16U)
#define CCTRL_CLKGATE_EN_SHIFT		((uint32_t)4U)

#define CCTRL_CLKGATE_TRAIL_MASK	\
		(((uint32_t)0xFFFFU) << CCTRL_CLKGATE_TRAIL_SHIFT)
#define CCTRL_CLKGATE_EN_MASK		\
		(((uint32_t)0xFU) << CCTRL_CLKGATE_EN_SHIFT)


/*
 * 5.4	Interrupt mask register 0
 */
#define CIM_MSK_FRAMESTART_SHIFT	((uint32_t)24U)
#define CIM_MSK_FRAMEEND_SHIFT		((uint32_t)20U)
#define CIM_MSK_ERR_SOT_HS_SHIFT	((uint32_t)16U)
#define CIM_MSK_ERR_LOST_FS_SHIFT	((uint32_t)12U)
#define CIM_MSK_ERR_LOST_FE_SHIFT	((uint32_t)8U)
#define CIM_MSK_ERR_OVER_SHIFT		((uint32_t)4U)
#define CIM_MSK_ERR_WRONG_CFG_SHIFT	((uint32_t)3U)
#define CIM_MSK_ERR_ECC_SHIFT		((uint32_t)2U)
#define CIM_MSK_ERR_CRC_SHIFT		((uint32_t)1U)
#define CIM_MSK_ERR_ID_SHIFT		((uint32_t)0U)

#define CIM_MSK_FRAMESTART_MASK		\
		(((uint32_t)0xFU) << CIM_MSK_FRAMESTART_SHIFT)
#define CIM_MSK_FRAMEEND_MASK		\
		(((uint32_t)0xFU) << CIM_MSK_FRAMEEND_SHIFT)
#define CIM_MSK_ERR_SOT_HS_MASK		\
		(((uint32_t)0xFU) << CIM_MSK_ERR_SOT_HS_SHIFT)
#define CIM_MSK_ERR_LOST_FS_MASK	\
		(((uint32_t)0xFU) << CIM_MSK_ERR_LOST_FS_SHIFT)
#define CIM_MSK_ERR_LOST_FE_MASK	\
		(((uint32_t)0xFU) << CIM_MSK_ERR_LOST_FE_SHIFT)
#define CIM_MSK_ERR_OVER_MASK		\
		(((uint32_t)0x1U) << CIM_MSK_ERR_OVER_SHIFT)
#define CIM_MSK_ERR_WRONG_CFG_MASK	\
		(((uint32_t)0x1U) << CIM_MSK_ERR_WRONG_CFG_SHIFT)
#define CIM_MSK_ERR_ECC_MASK		\
		(((uint32_t)0x1U) << CIM_MSK_ERR_ECC_SHIFT)
#define CIM_MSK_ERR_CRC_MASK		\
		(((uint32_t)0x1U) << CIM_MSK_ERR_CRC_SHIFT)
#define CIM_MSK_ERR_ID_MASK		\
		(((uint32_t)0x1U) << CIM_MSK_ERR_ID_SHIFT)

#define CIM_MSK_ALL_MASK			\
		(CIM_MSK_FRAMESTART_MASK |	\
		 CIM_MSK_FRAMEEND_MASK |	\
		 CIM_MSK_ERR_SOT_HS_MASK |	\
		 CIM_MSK_ERR_LOST_FS_MASK |	\
		 CIM_MSK_ERR_LOST_FE_MASK |	\
		 CIM_MSK_ERR_OVER_MASK |	\
		 CIM_MSK_ERR_WRONG_CFG_MASK |	\
		 CIM_MSK_ERR_ECC_MASK |		\
		 CIM_MSK_ERR_CRC_MASK |		\
		 CIM_MSK_ERR_ID_MASK)

#define CIM_INTR_DISABLE	(0U)
#define CIM_INTR_ENABLE		(1U)

/*
 * 5.5	Interrupt source register 0
 */
#define CIS_SRC_FRAMESTART_SHIFT	((uint32_t)24U)
#define CIS_SRC_FRAMEEND_SHIFT		((uint32_t)20U)
#define CIS_SRC_ERR_SOT_HS_SHIFT	((uint32_t)16U)
#define CIS_SRC_ERR_LOST_FS_SHIFT	((uint32_t)12U)
#define CIS_SRC_ERR_LOST_FE_SHIFT	((uint32_t)8U)
#define CIS_SRC_ERR_OVER_SHIFT		((uint32_t)4U)
#define CIS_SRC_ERR_WRONG_CFG_SHIFT	((uint32_t)3U)
#define CIS_SRC_ERR_ECC_SHIFT		((uint32_t)2U)
#define CIS_SRC_ERR_CRC_SHIFT		((uint32_t)1U)
#define CIS_SRC_ERR_ID_SHIFT		((uint32_t)0U)

#define CIS_SRC_FRAMESTART_MASK		\
		(((uint32_t)0xFU) << CIS_SRC_FRAMESTART_SHIFT)
#define CIS_SRC_FRAMEEND_MASK		\
		(((uint32_t)0xFU) << CIS_SRC_FRAMEEND_SHIFT)
#define CIS_SRC_ERR_SOT_HS_MASK		\
		(((uint32_t)0xFU) << CIS_SRC_ERR_SOT_HS_SHIFT)
#define CIS_SRC_ERR_LOST_FS_MASK	\
		(((uint32_t)0xFU) << CIS_SRC_ERR_LOST_FS_SHIFT)
#define CIS_SRC_ERR_LOST_FE_MASK	\
		(((uint32_t)0xFU) << CIS_SRC_ERR_LOST_FE_SHIFT)
#define CIS_SRC_ERR_OVER_MASK		\
		(((uint32_t)0x1U) << CIS_SRC_ERR_OVER_SHIFT)
#define CIS_SRC_ERR_WRONG_CFG_MASK	\
		(((uint32_t)0x1U) << CIS_SRC_ERR_WRONG_CFG_SHIFT)
#define CIS_SRC_ERR_ECC_MASK		\
		(((uint32_t)0x1U) << CIS_SRC_ERR_ECC_SHIFT)
#define CIS_SRC_ERR_CRC_MASK		\
		(((uint32_t)0x1U) << CIS_SRC_ERR_CRC_SHIFT)
#define CIS_SRC_ERR_ID_MASK		\
		(((uint32_t)0x1U) << CIS_SRC_ERR_ID_SHIFT)

#define CIS_SRC_ALL_MASK			\
		(CIS_SRC_FRAMESTART_MASK |	\
		 CIS_SRC_FRAMEEND_MASK |	\
		 CIS_SRC_ERR_SOT_HS_MASK |	\
		 CIS_SRC_ERR_LOST_FS_MASK |	\
		 CIS_SRC_ERR_LOST_FE_MASK |	\
		 CIS_SRC_ERR_OVER_MASK |	\
		 CIS_SRC_ERR_WRONG_CFG_MASK |	\
		 CIS_SRC_ERR_ECC_MASK |		\
		 CIS_SRC_ERR_CRC_MASK |		\
		 CIS_SRC_ERR_ID_MASK)

/*
 * 5.6	Interrupt mask register 1
 */
#define CIM_MSK_LINE_END_SHIFT		((uint32_t)0U)

#define CIM_MSK_LINE_END_MASK		\
		((uint32_t)0xFU << CIM_MSK_LINE_END_SHIFT)


/*
 * 5.7	Interrupt source register 1
 */
#define CIS_SRC_LINE_END_SHIFT		((uint32_t)0U)

#define CIS_SRC_LINE_END_MASK		\
		((uint32_t)0xFU << CIS_SRC_LINE_END_SHIFT)


/*
 * 5.8	D-PHY status register
 */
#define DSTS_ULPSDAT_SHIFT		((uint32_t)8U)
#define DSTS_STOPSTATEDAT_SHIFT		((uint32_t)4U)
#define DSTS_ULPSCLK_SHIFT		((uint32_t)1U)
#define DSTS_STOPSTATECLK_SHIFT		((uint32_t)0U)

#define DSTS_ULPSDAT_MASK		\
		(((uint32_t)0xFU) << DSTS_ULPSDAT_SHIFT)
#define DSTS_STOPSTATEDAT_MASK		\
		(((uint32_t)0xFU) << DSTS_STOPSTATEDAT_SHIFT)
#define DSTS_ULPSCLK_MASK		\
		(((uint32_t)0x1U) << DSTS_ULPSCLK_SHIFT)
#define DSTS_STOPSTATECLK_MASK		\
		(((uint32_t)0x1U) << DSTS_STOPSTATECLK_SHIFT)


/*
 * 5.9	D-PHY Common Control register
 */
#define DCCTRL_HSSETTLE_SHIFT		((uint32_t)24U)
#define DCCTRL_S_CLKSETTLECTL_SHIFT	((uint32_t)22U)
#define DCCTRL_S_BYTE_CLK_ENABLE_SHIFT	((uint32_t)21U)
#define DCCTRL_S_DPDN_SWAP_CLK_SHIFT	((uint32_t)6U)
#define DCCTRL_S_DPDN_SWAP_DAT_SHIFT	((uint32_t)5U)
#define DCCTRL_ENABLE_DAT_SHIFT		((uint32_t)1U)
#define DCCTRL_ENABLE_CLK_SHIFT		((uint32_t)0U)

#define DCCTRL_HSSETTLE_MASK		\
		(((uint32_t)0xFFU) << DCCTRL_HSSETTLE_SHIFT)
#define DCCTRL_S_CLKSETTLECTL_MASK	\
		(((uint32_t)0x3U) << DCCTRL_S_CLKSETTLECTL_SHIFT)
#define DCCTRL_S_BYTE_CLK_ENABLE_MASK	\
		(((uint32_t)0x1U) << DCCTRL_S_BYTE_CLK_ENABLE_SHIFT)
#define DCCTRL_S_DPDN_SWAP_CLK_MASK	\
		(((uint32_t)0x1U) << DCCTRL_S_DPDN_SWAP_CLK_SHIFT)
#define DCCTRL_S_DPDN_SWAP_DAT_MASK	\
		(((uint32_t)0x1U) << DCCTRL_S_DPDN_SWAP_DAT_SHIFT)
#define DCCTRL_ENABLE_DAT_MASK		\
		(((uint32_t)0xFU) << DCCTRL_ENABLE_DAT_SHIFT)
#define DCCTRL_ENABLE_CLK_MASK		\
		(((uint32_t)0x1U) << DCCTRL_ENABLE_CLK_SHIFT)

#define DCCTRL_ENABLE_DATA_LANE_0	(((uint32_t)0x1U) << 0U)
#define DCCTRL_ENABLE_DATA_LANE_1	(((uint32_t)0x1U) << 1U)
#define DCCTRL_ENABLE_DATA_LANE_2	(((uint32_t)0x1U) << 2U)
#define DCCTRL_ENABLE_DATA_LANE_3	(((uint32_t)0x1U) << 3U)

/*
 * 5.10	D-PHY Master and Slave Control register Low
 */
#define DBLCTRL_B_DPHYCTRL_SHIFT	((uint32_t)0U)

#define DBLCTRL_B_DPHYCTRL_MASK		\
		(((uint32_t)0xFFFFFFFFU) << DBLCTRL_B_DPHYCTRL_SHIFT)


/*
 * 5.11	D-PHY Master and Slave Control register High
 */
#define DBHCTRL_B_DPHYCTRL_SHIFT	((uint32_t)0U)

#define DBHCTRL_B_DPHYCTRL_MASK		\
		(((uint32_t)0xFFFFFFFFU) << DBHCTRL_B_DPHYCTRL_SHIFT)


/*
 * 5.12	D-PHY Slave Control register Low
 */
#define DSLCTRL_S_DPHYCTRL_SHIFT	((uint32_t)0U)

#define DSLCTRL_S_DPHYCTRL_MASK		\
		(((uint32_t)0xFFFFFFFFU) << DSLCTRL_S_DPHYCTRL_SHIFT)


/*
 * 5.13	D-PHY Slave Control register High
 */
#define DSHCTRL_S_DPHYCTRL_SHIFT	((uint32_t)0U)

#define DSHCTRL_S_DPHYCTRL_MASK		\
		(((uint32_t)0xFFFFFFFFU) << DSHCTRL_S_DPHYCTRL_SHIFT)


/*
 * 5.14	ISP Configuration register of CH_X
 */
#define ICON_PIXEL_MODE_SHIFT		((uint32_t)12U)
#define ICON_PARALLEL_SHIFT		((uint32_t)11U)
#define ICON_RGB_SWAP_SHIFT		((uint32_t)10U)
#define ICON_DATAFORMAT_SHIFT		((uint32_t)2U)
#define ICON_VIRTUAL_CHANNEL_SHIFT	((uint32_t)0U)

#define ICON_PIXEL_MODE_MASK		\
		(((uint32_t)0x3U) << ICON_PIXEL_MODE_SHIFT)
#define ICON_PARALLEL_MASK		\
		(((uint32_t)0x1U) << ICON_PARALLEL_SHIFT)
#define ICON_RGB_SWAP_MASK		\
		(((uint32_t)0x1U) << ICON_RGB_SWAP_SHIFT)
#define ICON_DATAFORMAT_MASK		\
		(((uint32_t)0x3FU) << ICON_DATAFORMAT_SHIFT)
#define ICON_VIRTUAL_CHANNEL_MASK	\
		(((uint32_t)0x3U) << ICON_VIRTUAL_CHANNEL_SHIFT)


/*
 * 5.15	ISP Resolution register of CH_X
 */
#define IRES_VRESOL_SHIFT		((uint32_t)16U)
#define IRES_HRESOL_SHIFT		((uint32_t)0U)

#define IRES_VRESOL_MASK		\
		(((uint32_t)0xFFFFU) << IRES_VRESOL_SHIFT)
#define IRES_HRESOL_MASK		\
		(((uint32_t)0xFFFFU) << IRES_HRESOL_SHIFT)


/*
 * 5.16	ISP SYNC register of CH_X
 */
#define ISYN_HSYNC_LINTV_SHIFT		((uint32_t)18U)

#define ISYN_HSYNC_LINTV_MASK		\
		(((uint32_t)0x3FU) << ISYN_HSYNC_LINTV_SHIFT)


/*
 * 5.26	Shadow Configuration register of CH_X
 */
#define SCON_PIXEL_MODE_SHIFT		((uint32_t)12U)
#define SCON_PARALLEL_SDW_SHIFT		((uint32_t)11U)
#define SCON_RGB_SWAP_SDW_SHIFT		((uint32_t)10U)
#define SCON_DATAFORMAT_SHIFT		((uint32_t)2U)
#define SCON_VIRTUAL_CHANNEL_SHIFT	((uint32_t)0U)

#define SCON_PIXEL_MODE_MASK		\
		(((uint32_t)0x3U) << SCON_PIXEL_MODE_SHIFT)
#define SCON_PARALLEL_SDW_MASK		\
		(((uint32_t)0x1U) << SCON_PARALLEL_SDW_SHIFT)
#define SCON_RGB_SWAP_SDW_MASK		\
		(((uint32_t)0x1U) << SCON_RGB_SWAP_SDW_SHIFT)
#define SCON_DATAFORMAT_MASK		\
		(((uint32_t)0x3FU) << SCON_DATAFORMAT_SHIFT)
#define SCON_VIRTUAL_CHANNEL_MASK	\
		(((uint32_t)0x3U) << SCON_VIRTUAL_CHANNEL_SHIFT)


/*
 * 5.27	Shadow Resolution register of CH_X
 */
#define SRES_VRESOL_SDW_SHIFT		((uint32_t)16U)
#define SRES_HRESOL_SDW_SHIFT		((uint32_t)0U)

#define SRES_VRESOL_SDW_MASK		\
		(((uint32_t)0xFFFFU) << SRES_VRESOL_SDW_SHIFT)
#define SRES_HRESOL_SDW_MASK		\
		(((uint32_t)0xFFFFU) << SRES_HRESOL_SDW_SHIFT)


/*
 * 5.28	Shadow SYNC register of CH_X
 */
#define SSYN_HSYNC_LINTV_SDW_SHIFT	((uint32_t)18U)

#define SSYN_HSYNC_LINTV_SDW_MASK	\
		(((uint32_t)0x3FU) << SSYN_HSYNC_LINTV_SDW_SHIFT)


/*
 * 5.38	Frame Counter of CH_X
 */
#define FRM_CNT_SHIFT			((uint32_t)0U)

#define FRM_CNT_MASK			\
		(((uint32_t)0xFFFFFFFFU) << FRM_CNT_SHIFT)


/*
 * 5.42	Line Interrupt Ratio of CH0
 */
#define LINE_INTR_SHIFT			((uint32_t)0U)

#define LINE_INTR_MASK			\
		(((uint32_t)0xFFFFFFFFU) << LINE_INTR_SHIFT)

/*
 * Generic Data Buffer
 */
#define GDB_RDATA_FIFO_RDATA_SHIFT	((uint32_t)0U)

#define GDB_RDATA_FIFO_RDATA_MASK	\
		(((uint32_t)0xFFFFFFFFU) << GDB_RDATA_FIFO_RDATA_SHIFT)


#define GDB_CNT_FIFO_CNT_SHIFT		((uint32_t)0U)

#define GDB_CNT_FIFO_CNT_MASK		\
		(((uint32_t)0x3FFU) << GDB_CNT_FIFO_CNT_SHIFT)


#define GDB_STATUS_CNT_IRQ_SHIFT	((uint32_t)8U)
#define GDB_STATUS_EMPTY_STATUS_SHIFT	((uint32_t)16U)
#define GDB_STATUS_FULL_STATUS_SHIFT	((uint32_t)17U)

#define GDB_STATUS_CNT_IRQ_MASK		\
		(((uint32_t)0x1U) << GDB_STATUS_CNT_IRQ_SHIFT)
#define GDB_STATUS_EMPTY_STATUS_MASK	\
		(((uint32_t)0x1U) << GDB_STATUS_EMPTY_STATUS_SHIFT)
#define GDB_STATUS_FULL_STATUS_MASK	\
		(((uint32_t)0x1U) << GDB_STATUS_FULL_STATUS_SHIFT)


#define GDB_INT_CNT_IRQ_SHIFT		((uint32_t)8U)
#define GDB_INT_EMPTY_INT_SHIFT		((uint32_t)16U)
#define GDB_INT_FULL_INT_SHIFT		((uint32_t)17U)

#define GDB_INT_CNT_IRQ_MASK		\
		(((uint32_t)0x1U) << GDB_INT_CNT_IRQ_SHIFT)
#define GDB_INT_EMPTY_INT_MASK		\
		(((uint32_t)0x1U) << GDB_INT_EMPTY_INT_SHIFT)
#define GDB_INT_FULL_INT_MASK		\
		(((uint32_t)0x1U) << GDB_INT_FULL_INT_SHIFT)


#define GDB_INTM_CNT_IRQ_MASK_SHIFT	((uint32_t)8U)
#define GDB_INTM_EMPTY_INT_MASK_SHIFT	((uint32_t)16U)
#define GDB_INTM_FULL_INT_MASK_SHIFT	((uint32_t)17U)

#define GDB_INTM_CNT_IRQ_MASK_MASK	\
		(((uint32_t)0x1U) << GDB_INTM_CNT_IRQ_MASK_SHIFT)
#define GDB_INTM_EMPTY_INT_MASK_MASK	\
		(((uint32_t)0x1U) << GDB_INTM_EMPTY_INT_MASK_SHIFT)
#define GDB_INTM_FULL_INT_MASK_MASK	\
		(((uint32_t)0x1U) << GDB_INTM_FULL_INT_MASK_SHIFT)

#define GDB_INTM_INTR_DISABLE		((uint32_t)0x0U)
#define GDB_INTM_INTR_ENABLE		((uint32_t)0x1U)


#define GDB_NUM_NUM_IRQ_SHIFT		((uint32_t)0x0U)

#define GDB_NUM_NUM_IRQ_MASK		\
		(((uint32_t)0x3FFU) << GDB_NUM_NUM_IRQ_SHIFT)
#endif
