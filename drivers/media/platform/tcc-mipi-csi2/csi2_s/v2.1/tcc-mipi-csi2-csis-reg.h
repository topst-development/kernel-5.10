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
#define CSIS_UPT_SDW		((uint32_t)0x000CU)
#define CSIS_INT_MSK0		((uint32_t)0x0010U)
#define CSIS_INT_SRC0		((uint32_t)0x0014U)
#define CSIS_INT_MSK1		((uint32_t)0x0018U)
#define CSIS_INT_SRC1		((uint32_t)0x001CU)
#define FS_INT_MSK		((uint32_t)0x0020U)
#define FS_INT_SRC		((uint32_t)0x0024U)
#define FE_INT_MSK		((uint32_t)0x0028U)
#define FE_INT_SRC		((uint32_t)0x002CU)
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
#define ISP_CONFIG_CH4		((uint32_t)0x0080U)
#define ISP_RESOL_CH4		((uint32_t)0x0084U)
#define ISP_SYNC_CH4		((uint32_t)0x0088U)
#define ISP_CONFIG_CH5		((uint32_t)0x0090U)
#define ISP_RESOL_CH5		((uint32_t)0x0094U)
#define ISP_SYNC_CH5		((uint32_t)0x0098U)
#define ISP_CONFIG_CH6		((uint32_t)0x00A0U)
#define ISP_RESOL_CH6		((uint32_t)0x00A4U)
#define ISP_SYNC_CH6		((uint32_t)0x00A8U)
#define ISP_CONFIG_CH7		((uint32_t)0x00B0U)
#define ISP_RESOL_CH7		((uint32_t)0x00B4U)
#define ISP_SYNC_CH7		((uint32_t)0x00B8U)
#define SDW_CONFIG_CH0		((uint32_t)0x0240U)
#define SDW_RESOL_CH0		((uint32_t)0x0244U)
#define SDW_SYNC_CH0		((uint32_t)0x0248U)
#define SDW_CONFIG_CH1		((uint32_t)0x0250U)
#define SDW_RESOL_CH1		((uint32_t)0x0254U)
#define SDW_SYNC_CH1		((uint32_t)0x0258U)
#define SDW_CONFIG_CH2		((uint32_t)0x0260U)
#define SDW_RESOL_CH2		((uint32_t)0x0264U)
#define SDW_SYNC_CH2		((uint32_t)0x0268U)
#define SDW_CONFIG_CH3		((uint32_t)0x0270U)
#define SDW_RESOL_CH3		((uint32_t)0x0274U)
#define SDW_SYNC_CH3		((uint32_t)0x0278U)
#define SDW_CONFIG_CH4		((uint32_t)0x0280U)
#define SDW_RESOL_CH4		((uint32_t)0x0284U)
#define SDW_SYNC_CH4		((uint32_t)0x0288U)
#define SDW_CONFIG_CH5		((uint32_t)0x0290U)
#define SDW_RESOL_CH5		((uint32_t)0x0294U)
#define SDW_SYNC_CH5		((uint32_t)0x0298U)
#define SDW_CONFIG_CH6		((uint32_t)0x02A0U)
#define SDW_RESOL_CH6		((uint32_t)0x02A4U)
#define SDW_SYNC_CH6		((uint32_t)0x02A8U)
#define SDW_CONFIG_CH7		((uint32_t)0x02B0U)
#define SDW_RESOL_CH7		((uint32_t)0x02B4U)
#define SDW_SYNC_CH7		((uint32_t)0x02B8U)
#define FRM_CNT_CH0		((uint32_t)0x0500U)
#define FRM_CNT_CH1		((uint32_t)0x0504U)
#define FRM_CNT_CH2		((uint32_t)0x0508U)
#define FRM_CNT_CH3		((uint32_t)0x050CU)
#define FRM_CNT_CH4		((uint32_t)0x0510U)
#define FRM_CNT_CH5		((uint32_t)0x0514U)
#define FRM_CNT_CH6		((uint32_t)0x0518U)
#define FRM_CNT_CH7		((uint32_t)0x051CU)
#define LINE_INTR_CH0		((uint32_t)0x0580U)
#define LINE_INTR_CH1		((uint32_t)0x0584U)
#define LINE_INTR_CH2		((uint32_t)0x0588U)
#define LINE_INTR_CH3		((uint32_t)0x058CU)
#define LINE_INTR_CH4		((uint32_t)0x0590U)
#define LINE_INTR_CH5		((uint32_t)0x0594U)
#define LINE_INTR_CH6		((uint32_t)0x0598U)
#define LINE_INTR_CH7		((uint32_t)0x059CU)
#define LRTE_CONFIG		((uint32_t)0x0600U)
#define ERR_LOST_FS_MSK		((uint32_t)0x0610U)
#define ERR_LOST_FS		((uint32_t)0x0614U)
#define ERR_LOST_FE_MSK		((uint32_t)0x0618U)
#define ERR_LOST_FE		((uint32_t)0x061CU)
#define ERR_VRESOL_MSK		((uint32_t)0x0620U)
#define ERR_VRESOL		((uint32_t)0x0624U)
#define ERR_HRESOL_MSK		((uint32_t)0x0628U)
#define ERR_HRESOL		((uint32_t)0x062CU)
#define LINE_END_MSK		((uint32_t)0x0630U)
#define LINE_END		((uint32_t)0x0634U)
#define PHY_STATUS		((uint32_t)0x0700U)

#define GDB_RDATA		((uint32_t)0x0000U)
#define GDB_CNT			((uint32_t)0x0004U)
#define GDB_STATUS		((uint32_t)0x0008U)
#define GDB_INT			((uint32_t)0x000CU)
#define GDB_INTM		((uint32_t)0x0010U)
#define GDB_NUM			((uint32_t)0x0014U)
#define VC0_DT_EN0		((uint32_t)0x0100U)
#define VC0_DT_EN1		((uint32_t)0x0104U)
#define VC1_DT_EN0		((uint32_t)0x0108U)
#define VC1_DT_EN1		((uint32_t)0x010CU)
#define VC2_DT_EN0		((uint32_t)0x0110U)
#define VC2_DT_EN1		((uint32_t)0x0114U)
#define VC3_DT_EN0		((uint32_t)0x0118U)
#define VC3_DT_EN1		((uint32_t)0x011CU)
#define VC4_DT_EN0		((uint32_t)0x0120U)
#define VC4_DT_EN1		((uint32_t)0x0124U)
#define VC5_DT_EN0		((uint32_t)0x0128U)
#define VC5_DT_EN1		((uint32_t)0x012CU)
#define VC6_DT_EN0		((uint32_t)0x0130U)
#define VC6_DT_EN1		((uint32_t)0x0134U)
#define VC7_DT_EN0		((uint32_t)0x0138U)
#define VC7_DT_EN1		((uint32_t)0x013CU)
#define GLP_EN			((uint32_t)0x0140U)

#define CSIS_VERSION_SHIFT		((uint32_t)0U)

#define CSIS_VERSION_MASK		\
		(((uint32_t)0xFFFFFFFFU) << CSIS_VERSION_SHIFT)

/*
 * 4.2	CSIS Common Control register
 */
#define CCTRL_DESKEW_LEVEL_SHIFT	((uint32_t)13U)
#define CCTRL_DESKEW_ENABLE_SHIFT	((uint32_t)12U)
#define CCTRL_INTERLEAVE_MODE_SHIFT	((uint32_t)10U)
#define CCTRL_LANE_NUMBER_SHIFT		((uint32_t)8U)
#define CCTRL_DESCRAMBLE_EN_SHIFT	((uint32_t)3U)
#define CCTRL_UPDATE_SHADOW_CTRL_SHIFT	((uint32_t)2U)
#define CCTRL_SW_RESET_SHIFT		((uint32_t)1U)
#define CCTRL_CSI_EN_SHIFT		((uint32_t)0U)

#define CCTRL_DESKEW_LEVEL_MASK		\
		(((uint32_t)0x7U) << CCTRL_DESKEW_LEVEL_SHIFT)
#define CCTRL_DESKEW_ENABLE_MASK	\
		(((uint32_t)0x1U) << CCTRL_DESKEW_ENABLE_SHIFT)
#define CCTRL_INTERLEAVE_MODE_MASK	\
		(((uint32_t)0x3U) << CCTRL_INTERLEAVE_MODE_SHIFT)
#define CCTRL_LANE_NUMBER_MASK		\
		(((uint32_t)0x3U) << CCTRL_LANE_NUMBER_SHIFT)
#define CCTRL_DESCRAMBLE_EN_MASK	\
		(((uint32_t)0x1U) << CCTRL_DESCRAMBLE_EN_SHIFT)
#define CCTRL_UPDATE_SHADOW_CTRL_MASK	\
		(((uint32_t)0x1U) << CCTRL_UPDATE_SHADOW_CTRL_SHIFT)
#define CCTRL_SW_RESET_MASK		\
		(((uint32_t)0x1U) << CCTRL_SW_RESET_SHIFT)
#define CCTRL_CSI_EN_MASK		\
		(((uint32_t)0x1U) << CCTRL_CSI_EN_SHIFT)


/*
 * 4.3	CSIS Clock Control register
 */
#define CCTRL_CLKGATE_TRAIL_SHIFT	((uint32_t)16U)
#define CCTRL_CLKGATE_EN_SHIFT		((uint32_t)4U)

#define CCTRL_CLKGATE_TRAIL_MASK	\
		(((uint32_t)0xFU) << CCTRL_CLKGATE_TRAIL_SHIFT)
#define CCTRL_CLKGATE_EN_MASK		\
		(((uint32_t)0x1U) << CCTRL_CLKGATE_EN_SHIFT)

/*
 * 4.4 CSIS Update Shadow Register
 */
#define CUS_UPDATE_SHADOW_SHIFT	((uint32_t)0U)

#define CUS_UPDATE_SHADOW_MASK	\
		(((uint32_t)0xFFU) << CUS_UPDATE_SHADOW_SHIFT)


/*
 * 4.5	Interrupt mask register 0
 */
#define CIM_MSK_ERR_SOT_HS_SHIFT	((uint32_t)16U)
#define CIM_MSK_ERR_OVER_SHIFT		((uint32_t)4U)
#define CIM_MSK_ERR_WRONG_CFG_SHIFT	((uint32_t)3U)
#define CIM_MSK_ERR_ECC_SHIFT		((uint32_t)2U)
#define CIM_MSK_ERR_CRC_SHIFT		((uint32_t)1U)
#define CIM_MSK_ERR_ID_SHIFT		((uint32_t)0U)

#define CIM_MSK_ERR_SOT_HS_MASK		\
		(((uint32_t)0xFU) << CIM_MSK_ERR_SOT_HS_SHIFT)
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
		(CIM_MSK_ERR_SOT_HS_MASK |	\
		 CIM_MSK_ERR_OVER_MASK |	\
		 CIM_MSK_ERR_WRONG_CFG_MASK |	\
		 CIM_MSK_ERR_ECC_MASK |		\
		 CIM_MSK_ERR_CRC_MASK |		\
		 CIM_MSK_ERR_ID_MASK)

#define CIM_INTR_DISABLE	(0U)
#define CIM_INTR_ENABLE		(1U)


/*
 * 4.6	Interrupt source register 0
 */
#define CIS_SRC_ERR_SOT_HS_SHIFT	((uint32_t)16U)
#define CIS_SRC_ERR_OVER_SHIFT		((uint32_t)4U)
#define CIS_SRC_ERR_WRONG_CFG_SHIFT	((uint32_t)3U)
#define CIS_SRC_ERR_ECC_SHIFT		((uint32_t)2U)
#define CIS_SRC_ERR_CRC_SHIFT		((uint32_t)1U)
#define CIS_SRC_ERR_ID_SHIFT		((uint32_t)0U)

#define CIS_SRC_ERR_SOT_HS_MASK		\
		(((uint32_t)0xFU) << CIS_SRC_ERR_SOT_HS_SHIFT)
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

#define CIS_SRC0_ALL_MASK			\
		(CIS_SRC_ERR_SOT_HS_MASK |	\
		 CIS_SRC_ERR_OVER_MASK |	\
		 CIS_SRC_ERR_WRONG_CFG_MASK |	\
		 CIS_SRC_ERR_ECC_MASK |		\
		 CIS_SRC_ERR_CRC_MASK |		\
		 CIS_SRC_ERR_ID_MASK)


/*
 * 4.7 Interrupt Mask Register 1
 */
#define CIM_MSK_VRESOL_MISMATCH_SHIFT	((uint32_t)6U)
#define CIM_MSK_HRESOL_MISMATCH_SHIFT	((uint32_t)5U)
#define CIM_MSK_ERR_LOST_FS_SHIFT	((uint32_t)4U)
#define CIM_MSK_ERR_LOST_FE_SHIFT	((uint32_t)3U)
#define CIM_MSK_FRAMESTART_SHIFT	((uint32_t)2U)
#define CIM_MSK_FRAMEEND_SHIFT		((uint32_t)1U)
#define CIM_MSK_LINE_END_SHIFT		((uint32_t)0U)

#define CIM_MSK_VRESOL_MISMATCH_MASK	\
		(((uint32_t)0x1U) << CIM_MSK_VRESOL_MISMATCH_SHIFT)
#define CIM_MSK_HRESOL_MISMATCH_MASK	\
		(((uint32_t)0x1U) << CIM_MSK_HRESOL_MISMATCH_SHIFT)
#define CIM_MSK_ERR_LOST_FS_MASK	\
		(((uint32_t)0x1U) << CIM_MSK_ERR_LOST_FS_SHIFT)
#define CIM_MSK_ERR_LOST_FE_MASK	\
		(((uint32_t)0x1U) << CIM_MSK_ERR_LOST_FE_SHIFT)
#define CIM_MSK_FRAMESTART_MASK		\
		(((uint32_t)0x1U) << CIM_MSK_FRAMESTART_SHIFT)
#define CIM_MSK_FRAMEEND_MASK		\
		(((uint32_t)0x1U) << CIM_MSK_FRAMEEND_SHIFT)
#define CIM_MSK_LINE_END_MASK		\
		(((uint32_t)0x1U) << CIM_MSK_LINE_END_SHIFT)


/*
 * 4.8 Interrupt source register 1
 */
#define CIS_SRC_VRESOL_MISMATCH_SHIFT	((uint32_t)6U)
#define CIS_SRC_HRESOL_MISMATCH_SHIFT	((uint32_t)5U)
#define CIS_SRC_ERR_LOST_FS_SHIFT	((uint32_t)4U)
#define CIS_SRC_ERR_LOST_FE_SHIFT	((uint32_t)3U)
#define CIS_SRC_FRAMESTART_SHIFT	((uint32_t)2U)
#define CIS_SRC_FRAMEEND_SHIFT		((uint32_t)1U)
#define CIS_SRC_LINE_END_SHIFT		((uint32_t)0U)

#define CIS_SRC_VRESOL_MISMATCH_MASK	\
		(((uint32_t)0x1U) << CIS_SRC_VRESOL_MISMATCH_SHIFT)
#define CIS_SRC_HRESOL_MISMATCH_MASK	\
		(((uint32_t)0x1U) << CIS_SRC_HRESOL_MISMATCH_SHIFT)
#define CIS_SRC_ERR_LOST_FS_MASK	\
		(((uint32_t)0x1U) << CIS_SRC_ERR_LOST_FS_SHIFT)
#define CIS_SRC_ERR_LOST_FE_MASK	\
		(((uint32_t)0x1U) << CIS_SRC_ERR_LOST_FE_SHIFT)
#define CIS_SRC_FRAMESTART_MASK		\
		(((uint32_t)0x1U) << CIS_SRC_FRAMESTART_SHIFT)
#define CIS_SRC_FRAMEEND_MASK		\
		(((uint32_t)0x1U) << CIS_SRC_FRAMEEND_SHIFT)
#define CIS_SRC_LINE_END_MASK		\
		(((uint32_t)0x1U) << CIS_SRC_LINE_END_SHIFT)

#define CIS_SRC1_ALL_MASK			\
		(CIS_SRC_VRESOL_MISMATCH_MASK |	\
		 CIS_SRC_HRESOL_MISMATCH_MASK |	\
		 CIS_SRC_ERR_LOST_FS_MASK |	\
		 CIS_SRC_ERR_LOST_FE_MASK |	\
		 CIS_SRC_FRAMESTART_MASK |	\
		 CIS_SRC_FRAMEEND_MASK |	\
		 CIS_SRC_LINE_END_MASK)

/*
 * 4.9 Frame Start Interrupt Mask Register
 */
#define FSIM_MSK_FRAMESTART_SHIFT	((uint32_t)0U)

#define FSIM_MSK_FRAMESTART_MASK	\
		(((uint32_t)0xFFU) << FSIM_MSK_FRAMESTART_SHIFT)


/*
 * 4.10 Frame Start Interrupt Source Register
 */
#define FSIS_FRAMESTART_SHIFT		((uint32_t)0U)

#define FSIS_FRAMESTART_MASK		\
		(((uint32_t)0xFFU) << FSIS_FRAMESTART_SHIFT)


/*
 * 4.11 Frame End Interrupt Mask Register
 */
#define FEIM_MSK_FRAMEEND_SHIFT	((uint32_t)0U)

#define FEIM_MSK_FRAMEEND_MASK	\
		(((uint32_t)0xFFU) << FEIM_MSK_FRAMEEND_SHIFT)


/*
 * 4.12 Frame End Interrupt Source Register
 */
#define FEIS_FRAMEEND_SHIFT		((uint32_t)0U)

#define FEIS_FRAMEEND_MASK		\
		(((uint32_t)0xFFU) << FEIS_FRAMEEND_SHIFT)


/*
 * 4.13	ISP Configuration register of CH_X
 */
#define ICON_VIRTUAL_CHANNEL_SHIFT	((uint32_t)16U)
#define ICON_PARALLEL_SHIFT		((uint32_t)14U)
#define ICON_PIXEL_MODE_SHIFT		((uint32_t)12U)
#define ICON_RGB_SWAP_SHIFT		((uint32_t)10U)
#define ICON_DATAFORMAT_SHIFT		((uint32_t)2U)

#define ICON_VIRTUAL_CHANNEL_MASK	\
		(((uint32_t)0x7U) << ICON_VIRTUAL_CHANNEL_SHIFT)
#define ICON_PARALLEL_MASK		\
		(((uint32_t)0x3U) << ICON_PARALLEL_SHIFT)
#define ICON_PIXEL_MODE_MASK		\
		(((uint32_t)0x3U) << ICON_PIXEL_MODE_SHIFT)
#define ICON_RGB_SWAP_MASK		\
		(((uint32_t)0x1U) << ICON_RGB_SWAP_SHIFT)
#define ICON_DATAFORMAT_MASK		\
		(((uint32_t)0x3FU) << ICON_DATAFORMAT_SHIFT)


/*
 * 4.14	ISP Resolution register of CH_X
 */
#define IRES_VRESOL_SHIFT		((uint32_t)16U)
#define IRES_HRESOL_SHIFT		((uint32_t)0U)

#define IRES_VRESOL_MASK		\
		(((uint32_t)0xFFFFU) << IRES_VRESOL_SHIFT)
#define IRES_HRESOL_MASK		\
		(((uint32_t)0xFFFFU) << IRES_HRESOL_SHIFT)


/*
 * 4.15	ISP SYNC register of CH_X
 */
#define ISYN_HSYNC_LINTV_SHIFT		((uint32_t)18U)

#define ISYN_HSYNC_LINTV_MASK		\
		(((uint32_t)0x3FU) << ISYN_HSYNC_LINTV_SHIFT)


/*
 * 4.19	Shadow Configuration register of CH_X
 */
#define SCON_VIRTUAL_CHANNEL_SHIFT	((uint32_t)16U)
#define SCON_PARALLEL_SDW_SHIFT		((uint32_t)14U)
#define SCON_PIXEL_MODE_SHIFT		((uint32_t)12U)
#define SCON_RGB_SWAP_SDW_SHIFT		((uint32_t)10U)
#define SCON_DATAFORMAT_SHIFT		((uint32_t)2U)

#define SCON_VIRTUAL_CHANNEL_MASK	\
		(((uint32_t)0x3U) << SCON_VIRTUAL_CHANNEL_SHIFT)
#define SCON_PARALLEL_SDW_MASK		\
		(((uint32_t)0x1U) << SCON_PARALLEL_SDW_SHIFT)
#define SCON_PIXEL_MODE_MASK		\
		(((uint32_t)0x3U) << SCON_PIXEL_MODE_SHIFT)
#define SCON_RGB_SWAP_SDW_MASK		\
		(((uint32_t)0x1U) << SCON_RGB_SWAP_SDW_SHIFT)
#define SCON_DATAFORMAT_MASK		\
		(((uint32_t)0x3FU) << SCON_DATAFORMAT_SHIFT)


/*
 * 4.20	Shadow Resolution register of CH_X
 */
#define SRES_VRESOL_SDW_SHIFT		((uint32_t)16U)
#define SRES_HRESOL_SDW_SHIFT		((uint32_t)0U)

#define SRES_VRESOL_SDW_MASK		\
		(((uint32_t)0xFFFFU) << SRES_VRESOL_SDW_SHIFT)
#define SRES_HRESOL_SDW_MASK		\
		(((uint32_t)0xFFFFU) << SRES_HRESOL_SDW_SHIFT)


/*
 * 4.21	Shadow SYNC register of CH_X
 */
#define SSYN_HSYNC_LINTV_SDW_SHIFT	((uint32_t)18U)

#define SSYN_HSYNC_LINTV_SDW_MASK	\
		(((uint32_t)0x3FU) << SSYN_HSYNC_LINTV_SDW_SHIFT)


/*
 * 4.25	Frame Counter of CH_X
 */
#define FRM_CNT_SHIFT			((uint32_t)0U)

#define FRM_CNT_MASK			\
		(((uint32_t)0xFFFFFFFFU) << FRM_CNT_SHIFT)


/*
 * 4.27	Line Interrupt Configuration of CH_X
 */
#define LINE_INTR_SHIFT			((uint32_t)0U)

#define LINE_INTR_MASK			\
		(((uint32_t)0xFFFFFFFFU) << LINE_INTR_SHIFT)


/*
 * 4.29 LRTE(Latency Reduction and Transport Efficiency) Configuration
 */
#define LTRE_CFG_EPD_EN_SHIFT			((uint32_t)31U)
#define LTRE_CFG_EPD_EPD_SP_SPACERS_SHIFT	((uint32_t)16U)
#define LTRE_CFG_D_PHY_EPD_OPT_SHIFT		((uint32_t)15U)
#define LTRE_CFG_EPD_LP_SPACERSEPD_SHIFT	((uint32_t)0U)

#define LTRE_CFG_EPD_EN_MASK			\
		(((uint32_t)0x1U) << LTRE_CFG_EPD_EN_SHIFT)
#define LTRE_CFG_EPD_EPD_SP_SPACERS_MASK	\
		(((uint32_t)0x7FFFU) << LTRE_CFG_EPD_EPD_SP_SPACERS_SHIFT)
#define LTRE_CFG_D_PHY_EPD_OPT_MASK		\
		(((uint32_t)0x1U) << LTRE_CFG_D_PHY_EPD_OPT_SHIFT)
#define LTRE_CFG_EPD_LP_SPACERSEPD_MASK		\
		(((uint32_t)0x7FFFU) << LTRE_CFG_EPD_LP_SPACERSEPD_SHIFT)

/*
 * 4.30 Lost Frame Start Interrupt Mask Register
 */
#define ELFSM_MSK_ERR_LOST_FS_SHIFT		((uint32_t)0U)

#define ELFSM_MSK_ERR_LOST_FS_MASK		\
		(((uint32_t)0xFFU) << ELFSM_MSK_ERR_LOST_FS_SHIFT)


/*
 * 4.31 Lost Frame Start Interrupt Source Register
 */
#define ELFSS_ERR_LOST_FS_SHIFT			((uint32_t)0U)

#define ELFSS_ERR_LOST_FS_MASK			\
		(((uint32_t)0xFFU) << ELFSS_ERR_LOST_FS_SHIFT)


/*
 * 4.32 Lost Frame End Interrupt Mask Register
 */
#define ELFEM_MSK_ERR_LOST_FE_SHIFT		((uint32_t)0U)

#define ELFEM_MSK_ERR_LOST_FE_MASK		\
		(((uint32_t)0xFFU) << ELFEM_MSK_ERR_LOST_FE_SHIFT)


/*
 * 4.33 Lost Frame End Interrupt Source Register
 */
#define ELFES_ERR_LOST_FE_SHIFT			((uint32_t)0U)

#define ELFES_ERR_LOST_FE_MASK			\
		(((uint32_t)0xFFU) << ELFES_ERR_LOST_FE_SHIFT)


/*
 * 4.34 Vertical Resolution Mismatch Interrupt Mask Register
 */
#define EVM_MSK_VRESOL_MISMATCH_SHIFT		((uint32_t)0U)

#define EVM_MSK_VRESOL_MISMATCH_MASK		\
		(((uint32_t)0xFFU) << EVM_MSK_VRESOL_MISMATCH_SHIFT)


/*
 * 4.35 Vertical Resolution Mismatch Interrupt Source Register
 */
#define EV_VRESOL_MISMATCH_SHIFT		((uint32_t)0U)

#define EV_VRESOL_MISMATCH_MASK			\
		(((uint32_t)0xFFU) << EV_VRESOL_MISMATCH_SHIFT)


/*
 * 4.36 Horizontal Resolution Mismatch Interrupt Mask Register
 */
#define EHM_MSK_HRESOL_MISMATCH_SHIFT		((uint32_t)0U)

#define EHM_MSK_HRESOL_MISMATCH_MASK		\
		(((uint32_t)0xFFU) << EHM_MSK_HRESOL_MISMATCH_SHIFT)


/*
 * 4.37 Horizontal Resolution Mismatch Interrupt Source Register
 */
#define EH_HRESOL_MISMATCH_SHIFT		((uint32_t)0U)

#define EH_HRESOL_MISMATCH_MASK			\
		(((uint32_t)0xFFU) << EH_HRESOL_MISMATCH_SHIFT)


/*
 * 4.38 Line End Interrupt Mask Register
 */
#define LEM_MSK_LINE_END_SHIFT			((uint32_t)0U)

#define LEM_MSK_LINE_END_MASK			\
		(((uint32_t)0xFFU) << LEM_MSK_LINE_END_SHIFT)


/*
 * 4.39 Line End Interrupt Source Register
 */
#define LE_LINE_END_SHIFT			((uint32_t)0U)

#define LE_LINE_END_MASK			\
		(((uint32_t)0xFFU) << LE_LINE_END_SHIFT)


/*
 * 4.40	PHY_STATUS
 */
#define PSTS_ULPSDAT_SHIFT		((uint32_t)8U)
#define PSTS_STOPSTATEDAT_SHIFT		((uint32_t)4U)
#define PSTS_ULPSCLK_SHIFT		((uint32_t)1U)
#define PSTS_STOPSTATECLK_SHIFT		((uint32_t)0U)

#define PSTS_ULPSDAT_MASK		\
		(((uint32_t)0xFU) << PSTS_ULPSDAT_SHIFT)
#define PSTS_STOPSTATEDAT_MASK		\
		(((uint32_t)0xFU) << PSTS_STOPSTATEDAT_SHIFT)
#define PSTS_ULPSCLK_MASK		\
		(((uint32_t)0x1U) << PSTS_ULPSCLK_SHIFT)
#define PSTS_STOPSTATECLK_MASK		\
		(((uint32_t)0x1U) << PSTS_STOPSTATECLK_SHIFT)

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
