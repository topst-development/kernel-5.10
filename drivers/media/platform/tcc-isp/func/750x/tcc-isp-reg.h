/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_ISP_REG_H
#define TCC_ISP_REG_H

#include "../../../tcc-mipi-csi2/750x/tcc-mipi-csi2-cfg-reg.h"

#define TCC_ISP_MAX_CORE			((uint32_t)4U)

#define TCC_ISP_PROBED				((uint32_t)1U)
#define TCC_ISP_NOPROBED			((uint32_t)0U)
#define TCC_ISP_STARTED				((uint32_t)1U)
#define TCC_ISP_STOPPED				((uint32_t)0U)

#define TCC_ISP_DEMOSAIC_MODE_RGGB		((uint32_t)0U)
#define TCC_ISP_DEMOSAIC_MODE_RCCB		((uint32_t)1U)
#define TCC_ISP_DEMOSAIC_MODE_RCCC		((uint32_t)2U)
#define TCC_ISP_DEMOSAIC_MODE_RGBIR		((uint32_t)3U)

/******************************************************************
 * ISP Factory Only Register Control ( 0 page 0x000~0xFFF )
 ******************************************************************/
#define REG_ISP_CTL				((uint32_t)0x0004U)
#define REG_ISP_STS				((uint32_t)0x0008U)
#define REG_ISP_IMG_WIDTH			((uint32_t)0x0010U)
#define REG_ISP_IMG_HEIGHT			((uint32_t)0x0014U)
#define REG_ISP_UP_CTL				((uint32_t)0x0030U)
#define REG_ISP_UP_SEL0				((uint32_t)0x0034U)
#define REG_ISP_UP_SEL1				((uint32_t)0x0038U)
#define REG_ISP_UP_MODE0			((uint32_t)0x003CU)
#define REG_ISP_UP_MODE1			((uint32_t)0x0040U)
#define REG_ISP_USR_CNT				((uint32_t)0x0044U)
#define REG_ISP_IRQ_CTL				((uint32_t)0x005CU)
#define REG_ISP_IRQ_MSK				((uint32_t)0x0060U)
#define REG_ISP_IRQ_CLR				((uint32_t)0x0064U)
#define REG_ISP_IRQ_STATUS			((uint32_t)0x0068U)


/* CTL (ISP common control register) */
#define CTL_ISPX_SLP_MODE_MASK			((uint32_t)0x1U)
#define CTL_AXI_SLP_MODE_MASK			((uint32_t)0x1U)
#define CTL_ISPX_SOFT_RESET_MASK		((uint32_t)0x1U)
#define CTL_AXI_SOFT_RESET_MASK			((uint32_t)0x1U)
#define CTL_ISPX_APB_SOFT_RESET_MASK		((uint32_t)0x1U)
#define CTL_MEM_SHARE_EN0_MASK			((uint32_t)0x1U)
#define CTL_MEM_SHARE_EN1_MASK			((uint32_t)0x1U)

#define CTL_ISP_SLP_MODE_SHIFT			((uint32_t)0U)
#define CTL_AXI_SLP_MODE_SHIFT			((uint32_t)4U)
#define CTL_ISP_SOFT_RESET_SHIFT		((uint32_t)8U)
#define CTL_AXI_SOFT_RESET_SHIFT		((uint32_t)12U)
#define CTL_ISP_APB_SOFT_RESET_SHIFT		((uint32_t)16U)
#define CTL_MEM_SHARE_EN0_SHIFT			((uint32_t)20U)
#define CTL_MEM_SHARE_EN1_SHIFT			((uint32_t)21U)


/* STS (Common status register) */
#define STS_ISP_IRQ_MASK			((uint32_t)0xFU)
#define STS_VSYNC_IRQ_MASK			((uint32_t)0xFU)
#define STS_SSD_READ_BUSY_MASK			((uint32_t)0xFU)

#define STS_ISP_IRQ_SHIFT			((uint32_t)0x0U)
#define STS_VSYNC_IRQ_SHIFT			((uint32_t)0x4U)
#define STS_SSD_READ_BUSY_SHIFT			((uint32_t)0x8U)


/* IMG_WIDTH */
#define IMG_WIDTH_IN_IMG_WIDTH_MASK		((uint32_t)0xFFFFU)

#define IMG_WIDTH_IN_IMG_WIDTH_SHIFT		((uint32_t)0U)


/* IMG_HEIGHT */
#define IMG_HEIGHT_IN_IMG_HEIGHT_MASK		((uint32_t)0xFFFFU)

#define IMG_HEIGHT_IN_IMG_HEIGHT_SHIFT		((uint32_t)0U)


/* UP_CTL */
#define UP_CTL_TP_UP_CTL_MASK			((uint32_t)0x1U)
#define UP_CTL_RAW_CH_GAIN_UP_CTL_MASK		((uint32_t)0x1U)
#define UP_CTL_LSC_UP_CTL_MASK			((uint32_t)0x1U)
#define UP_CTL_RAW_AWB_GAIN_UP_CTL_MASK		((uint32_t)0x1U)
#define UP_CTL_RAW_CURVE_UP_CTL_MASK		((uint32_t)0x1U)
#define UP_CTL_PRE_RAW_UP_CTL_MASK		((uint32_t)0x1U)
#define UP_CTL_POST_RAW_UP_CTL_MASK		((uint32_t)0x1U)
#define UP_CTL_RAW_INT_UP_CTL_MASK		((uint32_t)0x1U)
#define UP_CTL_DEBLANK_UP_CTL_MASK		((uint32_t)0x1U)
#define UP_CTL_HDR_UP_CTL_MASK			((uint32_t)0x1U)
#define UP_CTL_CCM_UP_CTL_MASK			((uint32_t)0x1U)
#define UP_CTL_CONT_UP_CTL_MASK			((uint32_t)0x1U)
#define UP_CTL_YUV_PROC_UP_CTL_MASK		((uint32_t)0x1U)
#define UP_CTL_WIN_UP_CTL_MASK			((uint32_t)0x1U)
#define UP_CTL_RGBIR_UP_CTL_MASK		((uint32_t)0x1U)
#define UP_CTL_RCCB_UP_CTL_MASK			((uint32_t)0x1U)
#define UP_CTL_RCCC_UP_CTL_MASK			((uint32_t)0x1U)
#define UP_CTL_3DNR_UP_CTL_MASK			((uint32_t)0x1U)
#define UP_CTL_DCPD_UP_CTL_MASK			((uint32_t)0x1U)

#define UP_CTL_TP_UP_CTL_SHIFT			((uint32_t)0U)
#define UP_CTL_RAW_CH_GAIN_UP_CTL_SHIFT		((uint32_t)1U)
#define UP_CTL_LSC_UP_CTL_SHIFT			((uint32_t)2U)
#define UP_CTL_RAW_AWB_GAIN_UP_CTL_SHIFT	((uint32_t)3U)
#define UP_CTL_RAW_CURVE_UP_CTL_SHIFT		((uint32_t)4U)
#define UP_CTL_PRE_RAW_UP_CTL_SHIFT		((uint32_t)5U)
#define UP_CTL_POST_RAW_UP_CTL_SHIFT		((uint32_t)6U)
#define UP_CTL_RAW_INT_UP_CTL_SHIFT		((uint32_t)7U)
#define UP_CTL_DEBLANK_UP_CTL_SHIFT		((uint32_t)8U)
#define UP_CTL_HDR_UP_CTL_SHIFT			((uint32_t)9U)
#define UP_CTL_CCM_UP_CTL_SHIFT			((uint32_t)10U)
#define UP_CTL_CONT_UP_CTL_SHIFT		((uint32_t)11U)
#define UP_CTL_YUV_PROC_UP_CTL_SHIFT		((uint32_t)12U)
#define UP_CTL_WIN_UP_CTL_SHIFT			((uint32_t)14U)
#define UP_CTL_RGBIR_UP_CTL_SHIFT		((uint32_t)15U)
#define UP_CTL_RCCB_UP_CTL_SHIFT		((uint32_t)16U)
#define UP_CTL_RCCC_UP_CTL_SHIFT		((uint32_t)17U)
#define UP_CTL_3DNR_UP_CTL_SHIFT		((uint32_t)18U)
#define UP_CTL_DCPD_UP_CTL_SHIFT		((uint32_t)19U)

#define UP_CTL_TP_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_RAW_CH_GAIN_UP_CTL		((uint32_t)0x1U)
#define UP_CTL_LSC_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_RAW_AWB_GAIN_UP_CTL		((uint32_t)0x1U)
#define UP_CTL_RAW_CURVE_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_PRE_RAW_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_POST_RAW_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_RAW_INT_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_DEBLANK_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_HDR_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_CCM_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_CONT_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_YUV_PROC_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_WIN_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_RGBIR_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_RCCB_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_RCCC_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_3DNR_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_DCPD_UP_CTL			((uint32_t)0x1U)

#define UP_CTL_UP_ALL					\
	((UP_CTL_TP_UP_CTL << UP_CTL_TP_UP_CTL_SHIFT) | \
	(UP_CTL_RAW_CH_GAIN_UP_CTL << UP_CTL_RAW_CH_GAIN_UP_CTL_SHIFT) | \
	(UP_CTL_LSC_UP_CTL << UP_CTL_LSC_UP_CTL_SHIFT) | \
	(UP_CTL_RAW_AWB_GAIN_UP_CTL << UP_CTL_RAW_AWB_GAIN_UP_CTL_SHIFT) | \
	(UP_CTL_RAW_CURVE_UP_CTL << UP_CTL_RAW_CURVE_UP_CTL_SHIFT) | \
	(UP_CTL_PRE_RAW_UP_CTL << UP_CTL_PRE_RAW_UP_CTL_SHIFT) | \
	(UP_CTL_POST_RAW_UP_CTL << UP_CTL_POST_RAW_UP_CTL_SHIFT) | \
	(UP_CTL_RAW_INT_UP_CTL << UP_CTL_RAW_INT_UP_CTL_SHIFT) | \
	(UP_CTL_DEBLANK_UP_CTL << UP_CTL_DEBLANK_UP_CTL_SHIFT) | \
	(UP_CTL_HDR_UP_CTL << UP_CTL_HDR_UP_CTL_SHIFT) | \
	(UP_CTL_CCM_UP_CTL << UP_CTL_CCM_UP_CTL_SHIFT) | \
	(UP_CTL_CONT_UP_CTL << UP_CTL_CONT_UP_CTL_SHIFT) | \
	(UP_CTL_YUV_PROC_UP_CTL << UP_CTL_YUV_PROC_UP_CTL_SHIFT) | \
	(UP_CTL_WIN_UP_CTL << UP_CTL_WIN_UP_CTL_SHIFT) | \
	(UP_CTL_RGBIR_UP_CTL << UP_CTL_RGBIR_UP_CTL_SHIFT) | \
	(UP_CTL_RCCB_UP_CTL << UP_CTL_RCCB_UP_CTL_SHIFT) | \
	(UP_CTL_RCCC_UP_CTL << UP_CTL_RCCC_UP_CTL_SHIFT) | \
	(UP_CTL_3DNR_UP_CTL << UP_CTL_3DNR_UP_CTL_SHIFT) | \
	(UP_CTL_DCPD_UP_CTL << UP_CTL_DCPD_UP_CTL_SHIFT))


/* UP_SEL0 */
#define UP_SEL0_TP_SEL_CTL_MASK			((uint32_t)0x3U)
#define UP_SEL0_RAW_CH_GAIN_SEL_CTL_MASK	((uint32_t)0x3U)
#define UP_SEL0_LSC_SEL_CTL_MASK		((uint32_t)0x3U)
#define UP_SEL0_RAW_AWB_GAIN_SEL_CTL_MASK	((uint32_t)0x3U)
#define UP_SEL0_RAW_CURVE_SEL_CTL_MASK		((uint32_t)0x3U)
#define UP_SEL0_PRE_RAW_SEL_CTL_MASK		((uint32_t)0x3U)
#define UP_SEL0_POST_RAW_SEL_CTL_MASK		((uint32_t)0x3U)
#define UP_SEL0_RAW_INT_SEL_CTL_MASK		((uint32_t)0x3U)
#define UP_SEL0_DEBLANK_SEL_CTL_MASK		((uint32_t)0x3U)
#define UP_SEL0_HDR_SEL_CTL_MASK		((uint32_t)0x3U)
#define UP_SEL0_CCM_SEL_CTL_MASK		((uint32_t)0x3U)
#define UP_SEL0_CONT_SEL_CTL_MASK		((uint32_t)0x3U)
#define UP_SEL0_YUV_SEL_CTL_MASK		((uint32_t)0x3U)
#define UP_SEL0_WIN_SEL_CTL_MASK		((uint32_t)0x3U)
#define UP_SEL0_RGBIR_SEL_CTL_MASK		((uint32_t)0x3U)

#define UP_SEL0_TP_SEL_CTL_SHIFT		((uint32_t)0U)
#define UP_SEL0_RAW_CH_GAIN_SEL_CTL_SHIFT	((uint32_t)2U)
#define UP_SEL0_LSC_SEL_CTL_SHIFT		((uint32_t)4U)
#define UP_SEL0_RAW_AWB_GAIN_SEL_CTL_SHIFT	((uint32_t)6U)
#define UP_SEL0_RAW_CURVE_SEL_CTL_SHIFT		((uint32_t)8U)
#define UP_SEL0_PRE_RAW_SEL_CTL_SHIFT		((uint32_t)10U)
#define UP_SEL0_POST_RAW_SEL_CTL_SHIFT		((uint32_t)12U)
#define UP_SEL0_RAW_INT_SEL_CTL_SHIFT		((uint32_t)14U)
#define UP_SEL0_DEBLANK_SEL_CTL_SHIFT		((uint32_t)16U)
#define UP_SEL0_HDR_SEL_CTL_SHIFT		((uint32_t)18U)
#define UP_SEL0_CCM_SEL_CTL_SHIFT		((uint32_t)20U)
#define UP_SEL0_CONT_SEL_CTL_SHIFT		((uint32_t)22U)
#define UP_SEL0_YUV_SEL_CTL_SHIFT		((uint32_t)24U)
#define UP_SEL0_WIN_SEL_CTL_SHIFT		((uint32_t)28U)
#define UP_SEL0_RGBIR_SEL_CTL_SHIFT		((uint32_t)30U)

#define UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING		\
		((uint32_t)0U)
#define UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING	\
		((uint32_t)1U)
#define UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING		\
		((uint32_t)2U)
#define UP_SEL_SYNC_ON_WRITING_TIMING			\
		((uint32_t)3U)


#define UP_SEL0_ALL_SYNC_ON_USER_SPECIFIED_TIMING \
	((UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL0_TP_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL0_RAW_CH_GAIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL0_LSC_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL0_RAW_AWB_GAIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL0_RAW_CURVE_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL0_PRE_RAW_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL0_POST_RAW_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL0_RAW_INT_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL0_DEBLANK_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL0_HDR_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL0_CCM_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL0_CONT_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL0_YUV_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL0_WIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL0_RGBIR_SEL_CTL_SHIFT))


#define UP_SEL0_ALL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING \
	((UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	UP_SEL0_TP_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	UP_SEL0_RAW_CH_GAIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	UP_SEL0_LSC_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	UP_SEL0_RAW_AWB_GAIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	UP_SEL0_RAW_CURVE_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	UP_SEL0_PRE_RAW_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	UP_SEL0_POST_RAW_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	UP_SEL0_RAW_INT_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	UP_SEL0_DEBLANK_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	UP_SEL0_HDR_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	UP_SEL0_CCM_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	UP_SEL0_CONT_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	UP_SEL0_YUV_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	UP_SEL0_WIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	UP_SEL0_RGBIR_SEL_CTL_SHIFT))


#define UP_SEL0_ALL_SYNC_ON_VSYNC_RISING_EDGE_TIMING \
	((UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	UP_SEL0_TP_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	UP_SEL0_RAW_CH_GAIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	UP_SEL0_LSC_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	UP_SEL0_RAW_AWB_GAIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	UP_SEL0_RAW_CURVE_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	UP_SEL0_PRE_RAW_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	UP_SEL0_POST_RAW_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	UP_SEL0_RAW_INT_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	UP_SEL0_DEBLANK_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	UP_SEL0_HDR_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	UP_SEL0_CCM_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	UP_SEL0_CONT_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	UP_SEL0_YUV_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	UP_SEL0_WIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	UP_SEL0_RGBIR_SEL_CTL_SHIFT))


#define UP_SEL0_ALL_SYNC_ON_WRITING_TIMING \
	((UP_SEL_SYNC_ON_WRITING_TIMING << \
	UP_SEL0_TP_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	UP_SEL0_RAW_CH_GAIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	UP_SEL0_LSC_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	UP_SEL0_RAW_AWB_GAIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	UP_SEL0_RAW_CURVE_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	UP_SEL0_PRE_RAW_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	UP_SEL0_POST_RAW_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	UP_SEL0_RAW_INT_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	UP_SEL0_DEBLANK_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	UP_SEL0_HDR_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	UP_SEL0_CCM_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	UP_SEL0_CONT_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	UP_SEL0_YUV_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	UP_SEL0_WIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	UP_SEL0_RGBIR_SEL_CTL_SHIFT))


/* UP_SEL1 */
#define UP_SEL1_RCCB_SEL_CTL_MASK		((uint32_t)0x1U)
#define UP_SEL1_RCCC_SEL_CTL_MASK		((uint32_t)0x1U)
#define UP_SEL1_3DNR_SEL_CTL_MASK		((uint32_t)0x1U)
#define UP_SEL1_DCPD_SEL_CTL_MASK		((uint32_t)0x1U)

#define UP_SEL1_RCCB_SEL_CTL_SHIFT		((uint32_t)0U)
#define UP_SEL1_RCCC_SEL_CTL_SHIFT		((uint32_t)2U)
#define UP_SEL1_3DNR_SEL_CTL_SHIFT		((uint32_t)4U)
#define UP_SEL1_DCPD_SEL_CTL_SHIFT		((uint32_t)6U)


#define UP_SEL1_ALL_SYNC_ON_USER_SPECIFIED_TIMING \
	((UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	 UP_SEL1_RCCB_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	 UP_SEL1_RCCC_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	 UP_SEL1_3DNR_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	 UP_SEL1_DCPD_SEL_CTL_SHIFT))

#define UP_SEL1_ALL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING \
	((UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	 UP_SEL1_RCCB_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	 UP_SEL1_RCCC_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	 UP_SEL1_3DNR_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	 UP_SEL1_DCPD_SEL_CTL_SHIFT))

#define UP_SEL1_ALL_SYNC_ON_VSYNC_RISING_EDGE_TIMING \
	((UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	 UP_SEL1_RCCB_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	 UP_SEL1_RCCC_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	 UP_SEL1_3DNR_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	 UP_SEL1_DCPD_SEL_CTL_SHIFT))

#define UP_SEL1_ALL_SYNC_ON_WRITING_TIMING \
	((UP_SEL_SYNC_ON_WRITING_TIMING << \
	 UP_SEL1_RCCB_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	 UP_SEL1_RCCC_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	 UP_SEL1_3DNR_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	 UP_SEL1_DCPD_SEL_CTL_SHIFT))


/* UP_MODE0 */
#define UP_MODE0_TP_UP_MODE_MASK		((uint32_t)0x1U)
#define UP_MODE0_RAW_CH_GAIN_UP_MODE_MASK	((uint32_t)0x1U)
#define UP_MODE0_LSC_UP_MODE_MASK		((uint32_t)0x1U)
#define UP_MODE0_RAW_AWB_GAIN_UP_MODE_MASK	((uint32_t)0x1U)
#define UP_MODE0_RAW_CURVE_UP_MODE_MASK		((uint32_t)0x1U)
#define UP_MODE0_PRE_RAW_UP_MODE_MASK		((uint32_t)0x1U)
#define UP_MODE0_POST_RAW_UP_MODE_MASK		((uint32_t)0x1U)
#define UP_MODE0_RAW_INT_UP_MODE_MASK		((uint32_t)0x1U)
#define UP_MODE0_DEBLANK_UP_MODE_MASK		((uint32_t)0x1U)
#define UP_MODE0_HDR_UP_MODE_MASK		((uint32_t)0x1U)
#define UP_MODE0_CCM_UP_MODE_MASK		((uint32_t)0x1U)
#define UP_MODE0_CONT_UP_MODE_MASK		((uint32_t)0x1U)
#define UP_MODE0_YUV_PROC_UP_MODE_MASK		((uint32_t)0x1U)
#define UP_MODE0_WIN_UP_MODE_MASK		((uint32_t)0x1U)
#define UP_MODE0_RGBIR_UP_MODE_MASK		((uint32_t)0x1U)
#define UP_MODE0_RCCB_UP_MODE_MASK		((uint32_t)0x1U)
#define UP_MODE0_RCCC_UP_MODE_MASK		((uint32_t)0x1U)
#define UP_MODE0_3DNR_UP_MODE_MASK		((uint32_t)0x1U)
#define UP_MODE0_DCPD_UP_MODE_MASK		((uint32_t)0x1U)

#define UP_MODE0_TP_UP_MODE_SHIFT		((uint32_t)0U)
#define UP_MODE0_RAW_CH_GAIN_UP_MODE_SHIFT	((uint32_t)1U)
#define UP_MODE0_LSC_UP_MODE_SHIFT		((uint32_t)2U)
#define UP_MODE0_RAW_AWB_GAIN_UP_MODE_SHIFT	((uint32_t)3U)
#define UP_MODE0_RAW_CURVE_UP_MODE_SHIFT	((uint32_t)4U)
#define UP_MODE0_PRE_RAW_UP_MODE_SHIFT		((uint32_t)5U)
#define UP_MODE0_POST_RAW_UP_MODE_SHIFT		((uint32_t)6U)
#define UP_MODE0_RAW_INT_UP_MODE_SHIFT		((uint32_t)7U)
#define UP_MODE0_DEBLANK_UP_MODE_SHIFT		((uint32_t)8U)
#define UP_MODE0_HDR_UP_MODE_SHIFT		((uint32_t)9U)
#define UP_MODE0_CCM_UP_MODE_SHIFT		((uint32_t)10U)
#define UP_MODE0_CONT_UP_MODE_SHIFT		((uint32_t)11U)
#define UP_MODE0_YUV_PROC_UP_MODE_SHIFT		((uint32_t)12U)
#define UP_MODE0_WIN_UP_MODE_SHIFT		((uint32_t)14U)
#define UP_MODE0_RGBIR_UP_MODE_SHIFT		((uint32_t)15U)
#define UP_MODE0_RCCB_UP_MODE_SHIFT		((uint32_t)16U)
#define UP_MODE0_RCCC_UP_MODE_SHIFT		((uint32_t)17U)
#define UP_MODE0_3DNR_UP_MODE_SHIFT		((uint32_t)18U)
#define UP_MODE0_DCPD_UP_MODE_SHIFT		((uint32_t)19U)

#define UP_MODE0_INDIV_SYNC_MODE		((uint32_t)0U)
#define UP_MODE0_GROUP_SYNC_MODE		((uint32_t)1U)

#define UP_MODE0_ALL_INDIV_SYNC_MODE \
	((UP_MODE0_INDIV_SYNC_MODE << UP_MODE0_TP_UP_MODE_SHIFT) | \
	(UP_MODE0_INDIV_SYNC_MODE << UP_MODE0_RAW_CH_GAIN_UP_MODE_SHIFT) | \
	(UP_MODE0_INDIV_SYNC_MODE << UP_MODE0_LSC_UP_MODE_SHIFT) | \
	(UP_MODE0_INDIV_SYNC_MODE << UP_MODE0_RAW_AWB_GAIN_UP_MODE_SHIFT) | \
	(UP_MODE0_INDIV_SYNC_MODE << UP_MODE0_RAW_CURVE_UP_MODE_SHIFT) | \
	(UP_MODE0_INDIV_SYNC_MODE << UP_MODE0_PRE_RAW_UP_MODE_SHIFT) | \
	(UP_MODE0_INDIV_SYNC_MODE << UP_MODE0_POST_RAW_UP_MODE_SHIFT) | \
	(UP_MODE0_INDIV_SYNC_MODE << UP_MODE0_RAW_INT_UP_MODE_SHIFT) | \
	(UP_MODE0_INDIV_SYNC_MODE << UP_MODE0_DEBLANK_UP_MODE_SHIFT) | \
	(UP_MODE0_INDIV_SYNC_MODE << UP_MODE0_HDR_UP_MODE_SHIFT) | \
	(UP_MODE0_INDIV_SYNC_MODE << UP_MODE0_CCM_UP_MODE_SHIFT) | \
	(UP_MODE0_INDIV_SYNC_MODE << UP_MODE0_CONT_UP_MODE_SHIFT) | \
	(UP_MODE0_INDIV_SYNC_MODE << UP_MODE0_YUV_PROC_UP_MODE_SHIFT) | \
	(UP_MODE0_INDIV_SYNC_MODE << UP_MODE0_WIN_UP_MODE_SHIFT) | \
	(UP_MODE0_INDIV_SYNC_MODE << UP_MODE0_RGBIR_UP_MODE_SHIFT) | \
	(UP_MODE0_INDIV_SYNC_MODE << UP_MODE0_RCCB_UP_MODE_SHIFT) | \
	(UP_MODE0_INDIV_SYNC_MODE << UP_MODE0_RCCC_UP_MODE_SHIFT) | \
	(UP_MODE0_INDIV_SYNC_MODE << UP_MODE0_3DNR_UP_MODE_SHIFT) | \
	(UP_MODE0_INDIV_SYNC_MODE << UP_MODE0_DCPD_UP_MODE_SHIFT))

#define UP_MODE0_ALL_GROUP_SYNC_MODE \
	((UP_MODE0_GROUP_SYNC_MODE << UP_MODE0_TP_UP_MODE_SHIFT) | \
	(UP_MODE0_GROUP_SYNC_MODE << UP_MODE0_RAW_CH_GAIN_UP_MODE_SHIFT) | \
	(UP_MODE0_GROUP_SYNC_MODE << UP_MODE0_LSC_UP_MODE_SHIFT) | \
	(UP_MODE0_GROUP_SYNC_MODE << UP_MODE0_RAW_AWB_GAIN_UP_MODE_SHIFT) | \
	(UP_MODE0_GROUP_SYNC_MODE << UP_MODE0_RAW_CURVE_UP_MODE_SHIFT) | \
	(UP_MODE0_GROUP_SYNC_MODE << UP_MODE0_PRE_RAW_UP_MODE_SHIFT) | \
	(UP_MODE0_GROUP_SYNC_MODE << UP_MODE0_POST_RAW_UP_MODE_SHIFT) | \
	(UP_MODE0_GROUP_SYNC_MODE << UP_MODE0_RAW_INT_UP_MODE_SHIFT) | \
	(UP_MODE0_GROUP_SYNC_MODE << UP_MODE0_DEBLANK_UP_MODE_SHIFT) | \
	(UP_MODE0_GROUP_SYNC_MODE << UP_MODE0_HDR_UP_MODE_SHIFT) | \
	(UP_MODE0_GROUP_SYNC_MODE << UP_MODE0_CCM_UP_MODE_SHIFT) | \
	(UP_MODE0_GROUP_SYNC_MODE << UP_MODE0_CONT_UP_MODE_SHIFT) | \
	(UP_MODE0_GROUP_SYNC_MODE << UP_MODE0_YUV_PROC_UP_MODE_SHIFT) | \
	(UP_MODE0_GROUP_SYNC_MODE << UP_MODE0_WIN_UP_MODE_SHIFT) | \
	(UP_MODE0_GROUP_SYNC_MODE << UP_MODE0_RGBIR_UP_MODE_SHIFT) | \
	(UP_MODE0_GROUP_SYNC_MODE << UP_MODE0_RCCB_UP_MODE_SHIFT) | \
	(UP_MODE0_GROUP_SYNC_MODE << UP_MODE0_RCCC_UP_MODE_SHIFT) | \
	(UP_MODE0_GROUP_SYNC_MODE << UP_MODE0_3DNR_UP_MODE_SHIFT) | \
	(UP_MODE0_GROUP_SYNC_MODE << UP_MODE0_DCPD_UP_MODE_SHIFT))


/* UP_MODE1 */
#define UP_MODE1_TP_VSYNC_SEL_MASK		((uint32_t)0x1U)
#define UP_MODE1_RAW_CH_GAIN_VSYNC_SEL_MASK	((uint32_t)0x1U)
#define UP_MODE1_LSC_VSYNC_SEL_MASK		((uint32_t)0x1U)
#define UP_MODE1_RAW_AWB_GAIN_VSYNC_SEL_MASK	((uint32_t)0x1U)
#define UP_MODE1_RAW_CURVE_VSYNC_SEL_MASK	((uint32_t)0x1U)
#define UP_MODE1_PRE_RAW_VSYNC_SEL_MASK		((uint32_t)0x1U)
#define UP_MODE1_POST_RAW_VSYNC_SEL_MASK	((uint32_t)0x1U)
#define UP_MODE1_RAW_INT_VSYNC_SEL_MASK		((uint32_t)0x1U)
#define UP_MODE1_DEBLANK_VSYNC_SEL_MASK		((uint32_t)0x1U)
#define UP_MODE1_HDR_VSYNC_SEL_MASK		((uint32_t)0x1U)
#define UP_MODE1_CCM_VSYNC_SEL_MASK		((uint32_t)0x1U)
#define UP_MODE1_CONT_VSYNC_SEL_MASK		((uint32_t)0x1U)
#define UP_MODE1_YUV_PROC_VSYNC_SEL_MASK	((uint32_t)0x1U)
#define UP_MODE1_WIN_VSYNC_SEL_MASK		((uint32_t)0x1U)
#define UP_MODE1_RGBIR_VSYNC_SEL_MASK		((uint32_t)0x1U)
#define UP_MODE1_RCCB_VSYNC_SEL_MASK		((uint32_t)0x1U)
#define UP_MODE1_RCCC_VSYNC_SEL_MASK		((uint32_t)0x1U)
#define UP_MODE1_3DNR_VSYNC_SEL_MASK		((uint32_t)0x1U)
#define UP_MODE1_DCPD_VSYNC_SEL_MASK		((uint32_t)0x1U)

#define UP_MODE1_TP_VSYNC_SEL_SHIFT		((uint32_t)0U)
#define UP_MODE1_RAW_CH_GAIN_VSYNC_SEL_SHIFT	((uint32_t)1U)
#define UP_MODE1_LSC_VSYNC_SEL_SHIFT		((uint32_t)2U)
#define UP_MODE1_RAW_AWB_GAIN_VSYNC_SEL_SHIFT	((uint32_t)3U)
#define UP_MODE1_RAW_CURVE_VSYNC_SEL_SHIFT	((uint32_t)4U)
#define UP_MODE1_PRE_RAW_VSYNC_SEL_SHIFT	((uint32_t)5U)
#define UP_MODE1_POST_RAW_VSYNC_SEL_SHIFT	((uint32_t)6U)
#define UP_MODE1_RAW_INT_VSYNC_SEL_SHIFT	((uint32_t)7U)
#define UP_MODE1_DEBLANK_VSYNC_SEL_SHIFT	((uint32_t)8U)
#define UP_MODE1_HDR_VSYNC_SEL_SHIFT		((uint32_t)9U)
#define UP_MODE1_CCM_VSYNC_SEL_SHIFT		((uint32_t)10U)
#define UP_MODE1_CONT_VSYNC_SEL_SHIFT		((uint32_t)11U)
#define UP_MODE1_YUV_PROC_VSYNC_SEL_SHIFT	((uint32_t)12U)
#define UP_MODE1_WIN_VSYNC_SEL_SHIFT		((uint32_t)14U)
#define UP_MODE1_RGBIR_VSYNC_SEL_SHIFT		((uint32_t)15U)
#define UP_MODE1_RCCB_VSYNC_SEL_SHIFT		((uint32_t)16U)
#define UP_MODE1_RCCC_VSYNC_SEL_SHIFT		((uint32_t)17U)
#define UP_MODE1_3DNR_VSYNC_SEL_SHIFT		((uint32_t)18U)
#define UP_MODE1_DCPD_VSYNC_SEL_SHIFT		((uint32_t)19U)

#define UP_MODE1_RISING_EDGE_MODE		((uint32_t)0U)
#define UP_MODE1_FALLING_EDGE_MODE		((uint32_t)1U)

#define UP_MODE1_ALL_RISING_EDGE_MODE \
	((UP_MODE1_RISING_EDGE_MODE << UP_MODE1_TP_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_RISING_EDGE_MODE << UP_MODE1_RAW_CH_GAIN_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_RISING_EDGE_MODE << UP_MODE1_LSC_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_RISING_EDGE_MODE << UP_MODE1_RAW_AWB_GAIN_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_RISING_EDGE_MODE << UP_MODE1_RAW_CURVE_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_RISING_EDGE_MODE << UP_MODE1_PRE_RAW_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_RISING_EDGE_MODE << UP_MODE1_POST_RAW_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_RISING_EDGE_MODE << UP_MODE1_RAW_INT_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_RISING_EDGE_MODE << UP_MODE1_DEBLANK_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_RISING_EDGE_MODE << UP_MODE1_HDR_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_RISING_EDGE_MODE << UP_MODE1_CCM_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_RISING_EDGE_MODE << UP_MODE1_CONT_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_RISING_EDGE_MODE << UP_MODE1_YUV_PROC_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_RISING_EDGE_MODE << UP_MODE1_WIN_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_RISING_EDGE_MODE << UP_MODE1_RGBIR_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_RISING_EDGE_MODE << UP_MODE1_RCCB_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_RISING_EDGE_MODE << UP_MODE1_RCCC_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_RISING_EDGE_MODE << UP_MODE1_3DNR_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_RISING_EDGE_MODE << UP_MODE1_DCPD_VSYNC_SEL_SHIFT))

#define UP_MODE1_ALL_FALLING_EDGE_MODE \
	((UP_MODE1_FALLING_EDGE_MODE << UP_MODE1_TP_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_FALLING_EDGE_MODE << UP_MODE1_RAW_CH_GAIN_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_FALLING_EDGE_MODE << UP_MODE1_LSC_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_FALLING_EDGE_MODE << UP_MODE1_RAW_AWB_GAIN_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_FALLING_EDGE_MODE << UP_MODE1_RAW_CURVE_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_FALLING_EDGE_MODE << UP_MODE1_PRE_RAW_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_FALLING_EDGE_MODE << UP_MODE1_POST_RAW_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_FALLING_EDGE_MODE << UP_MODE1_RAW_INT_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_FALLING_EDGE_MODE << UP_MODE1_DEBLANK_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_FALLING_EDGE_MODE << UP_MODE1_HDR_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_FALLING_EDGE_MODE << UP_MODE1_CCM_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_FALLING_EDGE_MODE << UP_MODE1_CONT_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_FALLING_EDGE_MODE << UP_MODE1_YUV_PROC_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_FALLING_EDGE_MODE << UP_MODE1_WIN_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_FALLING_EDGE_MODE << UP_MODE1_RGBIR_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_FALLING_EDGE_MODE << UP_MODE1_RCCB_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_FALLING_EDGE_MODE << UP_MODE1_RCCC_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_FALLING_EDGE_MODE << UP_MODE1_3DNR_VSYNC_SEL_SHIFT) | \
	(UP_MODE1_FALLING_EDGE_MODE << UP_MODE1_DCPD_VSYNC_SEL_SHIFT))


/* USR_CNT */
#define USR_CNT_MASK				((uint32_t)0xFFFFFFFFU)

#define USR_CNT_SHIFT				((uint32_t)0U)



/******************************************************************
 * Image Format Control (1page 0x000~0x0FF)
 ******************************************************************/
#define REG_ISP_IMG_WIN_CTL			((uint32_t)0x1000U)
#define REG_ISP_IMG_WIN_X_START			((uint32_t)0x1004U)
#define REG_ISP_IMG_WIN_Y_START			((uint32_t)0x1008U)
#define REG_ISP_IMG_WIN_WIDTH			((uint32_t)0x100CU)
#define REG_ISP_IMG_WIN_HEIGHT			((uint32_t)0x1010U)
#define REG_ISP_IMG_WIN_FORMAT			((uint32_t)0x1014U)
#define REG_ISP_IMG_IN_ORDER_CTL		((uint32_t)0x1020U)
#define REG_ISP_PT_GEN_CTL			((uint32_t)0x1030U)
#define REG_ISP_PT_IDX_CTL			((uint32_t)0x1034U)
#define REG_ISP_PT_RAND_CTL			((uint32_t)0x1038U)
#define REG_ISP_PT_RAND_SEED_CTL		((uint32_t)0x103CU)



/* IMG_WIN_CTL */
#define IMG_WIN_CTL_WIN_EN_MASK			((uint32_t)0x1U)
#define IMG_WIN_CTL_IMG_OUT_ORDER_SEL_MASK	((uint32_t)0x3U)
#define IMG_WIN_CTL_IMG_SCALE_MASK		((uint32_t)0x3U)
#define IMG_WIN_CTL_VSYNC_POL_SEL_MASK		((uint32_t)0x1U)

#define IMG_WIN_CTL_WIN_EN_SHIFT		((uint32_t)0U)
#define IMG_WIN_CTL_IMG_OUT_ORDER_SEL_SHIFT	((uint32_t)1U)
#define IMG_WIN_CTL_IMG_SCALE_SHIFT		((uint32_t)4U)
#define IMG_WIN_CTL_VSYNC_POL_SEL_SHIFT		((uint32_t)6U)


#define IMG_WIN_CTL_WIN_EN_DISABLE		((uint32_t)0U)
#define IMG_WIN_CTL_WIN_EN_ENABLE		((uint32_t)1U)

#define IMG_WIN_CTL_ORDER_YCBYCR		((uint32_t)0U)
#define IMG_WIN_CTL_ORDER_YCRYCB		((uint32_t)1U)
#define IMG_WIN_CTL_ORDER_CBYCRY		((uint32_t)2U)
#define IMG_WIN_CTL_ORDER_CRYCBY		((uint32_t)3U)

#define IMG_WIN_CTL_IMG_SCALE_NO_SCALER		((uint32_t)0U)
#define IMG_WIN_CTL_IMG_SCALE_1_2		((uint32_t)1U)
#define IMG_WIN_CTL_IMG_SCALE_1_4		((uint32_t)2U)
#define IMG_WIN_CTL_IMG_SCALE_1_8		((uint32_t)3U)

#define IMG_WIN_CTL_VSYNC_POL_SEL_HIGH_VERTICAL_BLANK	\
		((uint32_t)0U)
#define IMG_WIN_CTL_VSYNC_POL_SEL_LOW_VERTICAL_BLANK	\
		((uint32_t)1U)


/* IMG_WIN_X_START */
#define IMG_WIN_X_START_MASK			((uint32_t)0xFFFFU)

#define IMG_WIN_X_START_SHIFT			((uint32_t)0U)


/* IMG_WIN_Y_START */
#define IMG_WIN_Y_START_MASK			((uint32_t)0xFFFFU)

#define IMG_WIN_Y_START_SHIFT			((uint32_t)0U)


/* IMG_WIN_WIDTH */
#define IMG_WIN_WIDTH_MASK			((uint32_t)0xFFFFU)

#define IMG_WIN_WIDTH_SHIFT			((uint32_t)0U)


/* IMG_WIN_HEIGHT */
#define IMG_WIN_HEIGHT_MASK			((uint32_t)0xFFFFU)

#define IMG_WIN_HEIGHT_SHIFT			((uint32_t)0U)


/* IMG_WIN_FORMAT */
#define IMG_WIN_FORMAT_FORMAT_MASK		((uint32_t)0x3U)
#define IMG_WIN_FORMAT_DATA_ORDER_MASK		((uint32_t)0x3U)

#define IMG_WIN_FORMAT_FORMAT_SHIFT		((uint32_t)0U)
#define IMG_WIN_FORMAT_DATA_ORDER_SHIFT		((uint32_t)4U)


#define IMG_WIN_FORMAT_FORMAT_INVALID		((uint32_t)0U)
#define IMG_WIN_FORMAT_FORMAT_YUV422		((uint32_t)1U)
#define IMG_WIN_FORMAT_FORMAT_YUV444		((uint32_t)2U)
#define IMG_WIN_FORMAT_FORMAT_RGB888		((uint32_t)3U)

#define IMG_WIN_FORMAT_DATA_ORDER_P2P1P0	((uint32_t)0U)
#define IMG_WIN_FORMAT_DATA_ORDER_P0P2P1	((uint32_t)1U)
#define IMG_WIN_FORMAT_DATA_ORDER_P0P1P2	((uint32_t)2U)
#define IMG_WIN_FORMAT_DATA_ORDER_P2P0P1	((uint32_t)3U)


/* IMG_IN_ORDER_CTL */
#define IMG_IN_ORDER_CTL_ORDER_MASK		((uint32_t)0x3U)

#define IMG_IN_ORDER_CTL_ORDER_SHIFT		((uint32_t)0U)


#define IMG_IN_ORDER_CTL_ORDER_B_FIRST		((uint32_t)0U)
#define IMG_IN_ORDER_CTL_ORDER_GB_FIRST		((uint32_t)1U)
#define IMG_IN_ORDER_CTL_ORDER_GR_FIRST		((uint32_t)2U)
#define IMG_IN_ORDER_CTL_ORDER_R_FIRST		((uint32_t)3U)


/******************************************************************
 * deblank Register Define (1page 0x060)
 ******************************************************************/
#define REG_ISP_DEBLANK_CTL			((uint32_t)0x1060U)

#define ISP_DEBLANK_ONOFF_SHIFT			((uint32_t)0U)
#define ISP_DEBLANK_VSEL_SHIFT			((uint32_t)1U)
#define ISP_DEBLANK_FRONTPORCH_CONFIGURE_SHIFT	((uint32_t)16U)
#define ISP_DEBLANK_VBLANK_CONFIGURE_SHIFT	((uint32_t)20U)
#define ISP_DEBLANK_IB_SEL_SHIFT		((uint32_t)28U)

#define ISP_DEBLANK_ONOFF_MASK			\
	(((uint32_t)0x1U) << ISP_DEBLANK_ONOFF_SHIFT)
#define ISP_DEBLANK_VSEL_MASK			\
	(((uint32_t)0x1U) << ISP_DEBLANK_VSEL_SHIFT)
#define ISP_DEBLANK_FRONTPORCH_CONFIGURE_MASK	\
	(((uint32_t)0xFU) << ISP_DEBLANK_FRONTPORCH_CONFIGURE_SHIFT)
#define ISP_DEBLANK_VBLANK_CONFIGURE_MASK	\
	(((uint32_t)0xFFU) << ISP_DEBLANK_VBLANK_CONFIGURE_SHIFT)
#define ISP_DEBLANK_IB_SEL_MASK			\
	(((uint32_t)0x3U) << ISP_DEBLANK_IB_SEL_SHIFT)

#define ISP_DEBLANK_OFF				((uint32_t)0U)
#define ISP_DEBLANK_ON				((uint32_t)1U)

#define ISP_DEBLANK_VS_BYPASS			((uint32_t)0U)
#define ISP_DEBLANK_VS_GENERATE			((uint32_t)1U)

#define ISP_DEBLANK_IB_BYPASS			((uint32_t)0U)
#define ISP_DEBLANK_IB_8BIT_INPUT		((uint32_t)1U)
#define ISP_DEBLANK_IB_10BIT_INPUT		((uint32_t)2U)
#define ISP_DEBLANK_IB_12BIT_INPUT		((uint32_t)3U)


/******************************************************************
 * 3A Control Register Define (1page 0x200~2FF)
 ******************************************************************/
#define REG_ISP_TRI_AUTO_CTL0			((uint32_t)0x1200U)
#define REG_ISP_TRI_AUTO_CTL1			((uint32_t)0x1204U)


/******************************************************************
 * Bayer Channel Gain (1page 0x300~0x330)
 ******************************************************************/
#define REG_ISP_RAW_CH_CTL0			((uint32_t)0x1300U)
#define REG_ISP_RAW_CH_CTL1			((uint32_t)0x1304U)
#define REG_ISP_RAW_CH_CTL2			((uint32_t)0x1308U)
#define REG_ISP_RAW_CH_CTL3			((uint32_t)0x130CU)
#define REG_ISP_RAW_CH_CTL4			((uint32_t)0x1310U)


/******************************************************************
 * Bayer AWB Gain (1page 0x400~0x40A)
 ******************************************************************/
#define REG_ISP_RAW_AWB_GCTL0			((uint32_t)0x1400U)
#define REG_ISP_RAW_AWB_GCTL1			((uint32_t)0x1404U)


/******************************************************************
 * RGB AE Gain (1page 0x500~0x504)
 ******************************************************************/
#define REG_ISP_RGB_AE_GCTL			((uint32_t)0x1500U)


/******************************************************************
 * Bayer Curve Gain (1page 0x800~0x850)
 ******************************************************************/
#define REG_RAW_CUR_GCTL0			((uint32_t)0x1800U)
#define REG_RAW_CUR_GCTL1			((uint32_t)0x1804U)
#define REG_RAW_CUR_GCTL2			((uint32_t)0x1808U)
#define REG_RAW_CUR_GCTL3			((uint32_t)0x180CU)
#define REG_RAW_CUR_GCTL4			((uint32_t)0x1810U)
#define REG_RAW_CUR_GCTL5			((uint32_t)0x1814U)
#define REG_RAW_CUR_GCTL6			((uint32_t)0x1818U)
#define REG_RAW_CUR_GCTL7			((uint32_t)0x181CU)
#define REG_RAW_CUR_GCTL8			((uint32_t)0x1820U)
#define REG_RAW_CUR_GCTL9			((uint32_t)0x1824U)
#define REG_RAW_CUR_GCTL10			((uint32_t)0x1828U)


/******************************************************************
 * De-Companding (1page 0x860~0x880)
 ******************************************************************/
#define REG_ISP_DCPD_CTL			((uint32_t)0x1860U)
#define REG_ISP_DCPD_CUR_XCTL0			((uint32_t)0x1864U)
#define REG_ISP_DCPD_CUR_XCTL1			((uint32_t)0x1868U)
#define REG_ISP_DCPD_CUR_XCTL2			((uint32_t)0x186CU)
#define REG_ISP_DCPD_CUR_XCTL3			((uint32_t)0x1870U)
#define REG_ISP_DCPD_CUR_XCTL4			((uint32_t)0x1874U)
#define REG_ISP_DCPD_CUR_XCTL5			((uint32_t)0x1878U)
#define REG_ISP_DCPD_CUR_XCTL6			((uint32_t)0x187CU)
#define REG_ISP_DCPD_CUR_XCTL7			((uint32_t)0x1880U)
#define REG_ISP_DCPD_CUR_XCTL8			((uint32_t)0x1884U)
#define REG_ISP_DCPD_CUR_XCTL9			((uint32_t)0x1888U)
#define REG_ISP_DCPD_CUR_XCTL10			((uint32_t)0x188CU)
#define REG_ISP_DCPD_CUR_XCTL11			((uint32_t)0x1890U)
#define REG_ISP_DCPD_CUR_XCTL12			((uint32_t)0x1894U)
#define REG_ISP_DCPD_CUR_XCTL13			((uint32_t)0x1898U)
#define REG_ISP_DCPD_CUR_XCTL14			((uint32_t)0x189CU)
#define REG_ISP_DCPD_CUR_XCTL15			((uint32_t)0x18A0U)
#define REG_ISP_DCPD_CUR_XCTL16			((uint32_t)0x18A4U)
#define REG_ISP_DCPD_CUR_GCTL0			((uint32_t)0x18A8U)
#define REG_ISP_DCPD_CUR_GCTL1			((uint32_t)0x18ACU)
#define REG_ISP_DCPD_CUR_GCTL2			((uint32_t)0x18B0U)
#define REG_ISP_DCPD_CUR_GCTL3			((uint32_t)0x18B4U)
#define REG_ISP_DCPD_CUR_GCTL4			((uint32_t)0x18B8U)
#define REG_ISP_DCPD_CUR_GCTL5			((uint32_t)0x18BCU)
#define REG_ISP_DCPD_CUR_GCTL6			((uint32_t)0x18C0U)
#define REG_ISP_DCPD_CUR_GCTL7			((uint32_t)0x18C4U)
#define REG_ISP_DCPD_CUR_GCTL8			((uint32_t)0x18C8U)
#define REG_ISP_DCPD_CUR_GCTL9			((uint32_t)0x18CCU)
#define REG_ISP_DCPD_CUR_GCTL10			((uint32_t)0x18D0U)
#define REG_ISP_DCPD_CUR_GCTL11			((uint32_t)0x18D4U)
#define REG_ISP_DCPD_CUR_GCTL12			((uint32_t)0x18D8U)
#define REG_ISP_DCPD_CUR_GCTL13			((uint32_t)0x18DCU)
#define REG_ISP_DCPD_CUR_GCTL14			((uint32_t)0x18E0U)
#define REG_ISP_DCPD_CUR_GCTL15			((uint32_t)0x18E4U)
#define REG_ISP_DCPD_CUR_GCTL16			((uint32_t)0x18E8U)


/******************************************************************
 * Shading Correction Register Define (1page 0x900~0x918)
 ******************************************************************/
#define REG_ISP_LSC_CTL				((uint32_t)0x1900U)
#define REG_ISP_LSC_CFG				((uint32_t)0x1904U)
#define REG_ISP_LSC_GCTL			((uint32_t)0x1908U)


/******************************************************************
 * Pre/Post Bayer DPC (1page 0xA00~0xA02)
 ******************************************************************/
#define REG_ISP_DPC_CTL				((uint32_t)0x1A00U)


/******************************************************************
 * GBGR Correction (1page 0xA10~0xA16)
 ******************************************************************/
#define REG_ISP_GBGR_CCTL			((uint32_t)0x1A10U)
#define REG_ISP_GBGR_GCTL			((uint32_t)0x1A14U)


/******************************************************************
 * Pre Bayer NR (1page 0xA30~0xA36)
 ******************************************************************/
#define REG_ISP_PRRAW_NR_CTL			((uint32_t)0x1A30U)
#define REG_ISP_PRRAW_NR_GCTL			((uint32_t)0x1A34U)


/******************************************************************
 * Pre Bayer Sharpness (1page 0xA50~0xA5C)
 ******************************************************************/
#define REG_ISP_PRRAW_SH_CTL			((uint32_t)0x1A50U)
#define REG_ISP_PRRAW_SH_GCTL0			((uint32_t)0x1A54U)
#define REG_ISP_PRRAW_SH_GCTL1			((uint32_t)0x1A58U)
#define REG_ISP_PRRAW_SH_CCTL			((uint32_t)0x1A5CU)


/******************************************************************
 * Post Bayer NR(Noise Reduction) (1page 0xA80~0xA8E)
 ******************************************************************/
#define REG_ISP_PORAW_NR_CTL			((uint32_t)0x1A80U)
#define REG_ISP_PORAW_NR_GCTL0			((uint32_t)0x1A84U)
#define REG_ISP_PORAW_NR_GCTL1			((uint32_t)0x1A88U)
#define REG_ISP_PORAW_NR_GCTL2			((uint32_t)0x1A8CU)


/******************************************************************
 * Post Bayer Sharpness (1page 0xAB0~0xABA)
 ******************************************************************/
#define REG_ISP_PORAW_SH_CTL			((uint32_t)0x1AB0U)
#define REG_ISP_PORAW_SH_GCTL0			((uint32_t)0x1AB4U)
#define REG_ISP_PORAW_SH_GCTL1			((uint32_t)0x1AB8U)


/******************************************************************
 * RGBInt (2page 0x000~0x060)
 ******************************************************************/
#define REG_ISP_RGB_INT_CTL0			((uint32_t)0x2000U)
#define REG_ISP_RGB_INT_CTL1			((uint32_t)0x2004U)
#define REG_ISP_RGB_INT_CTL2			((uint32_t)0x2008U)
#define REG_ISP_RGB_INT_GCTL0			((uint32_t)0x200CU)
#define REG_ISP_RGB_INT_GCTL1			((uint32_t)0x2010U)
#define REG_ISP_RGB_INT_GCTL2			((uint32_t)0x2014U)


/******************************************************************
 * RGB Sharpness (2page 0x040~0x068)
 ******************************************************************/
#define REG_ISP_RGB_SH_CTL0			((uint32_t)0x2040U)
#define REG_ISP_RGB_SH_CTL1			((uint32_t)0x2044U)
#define REG_ISP_RGB_SH_GCTL0			((uint32_t)0x2048U)
#define REG_ISP_RGB_SH_GCTL1			((uint32_t)0x204CU)
#define REG_ISP_RGB_SH_GCTL2			((uint32_t)0x2050U)
#define REG_ISP_RGB_SH_GCTL3			((uint32_t)0x2054U)


/******************************************************************
 * Y NR(Noise Reduction) Define (2page 0x070~0x0A8)
 ******************************************************************/
#define REG_ISP_YF_CTL				((uint32_t)0x2070U)
#define REG_ISP_YF_GCTL0			((uint32_t)0x2074U)
#define REG_ISP_YF_GCTL1			((uint32_t)0x2078U)
#define REG_ISP_YF_GCTL2			((uint32_t)0x207CU)
#define REG_ISP_YF_OFS0				((uint32_t)0x2080U)
#define REG_ISP_YF_OFS1				((uint32_t)0x2084U)


/******************************************************************
 * Y Contrast (2page 0x0A8~0x0B0)
 ******************************************************************/
#define REG_ISP_YC_CTL				((uint32_t)0x20A8U)
#define REG_ISP_YC_CFG				((uint32_t)0x20ACU)


/******************************************************************
 * CNR(Chroma Noise Reduction) (2page 0x0B8~0x108)
 ******************************************************************/
#define REG_ISP_CF_CTL				((uint32_t)0x20B8U)
#define REG_ISP_CF_CFG0				((uint32_t)0x20BCU)
#define REG_ISP_CF_CFG1				((uint32_t)0x20C0U)
#define REG_ISP_CF_CFG2				((uint32_t)0x20C4U)
#define REG_ISP_CF_CFG3				((uint32_t)0x20C8U)


/******************************************************************
 * Y Sharpness (2page 0x0E0~0x108)
 ******************************************************************/
#define REG_ISP_Y_SH_CTL			((uint32_t)0x20E0U)
#define REG_ISP_Y_SH_CFG0			((uint32_t)0x20E4U)
#define REG_ISP_Y_SH_CFG1			((uint32_t)0x20E8U)
#define REG_ISP_Y_SH_CFG2			((uint32_t)0x20ECU)
#define REG_ISP_Y_SH_CFG3			((uint32_t)0x20F0U)
#define REG_ISP_Y_SH_CFG4			((uint32_t)0x20F4U)


/******************************************************************
 * RGB Color Correction Matrix (2page 0x500~0x530)
 ******************************************************************/
#define REG_ISP_RGB_CCM_CTL			((uint32_t)0x2500U)
#define REG_ISP_RGB_CCM_CFG0			((uint32_t)0x2504U)
#define REG_ISP_RGB_CCM_CFG1			((uint32_t)0x2508U)
#define REG_ISP_RGB_CCM_CFG2			((uint32_t)0x250CU)
#define REG_ISP_RGB_CCM_CFG3			((uint32_t)0x2510U)


/******************************************************************
 * YUV Saturation Control (2page 0x600~0x61C)
 ******************************************************************/
#define REG_ISP_YUV_SA_CTL			((uint32_t)0x2600U)
#define REG_ISP_YUV_SA_GCTL0			((uint32_t)0x2604U)
#define REG_ISP_YUV_SA_GCTL1			((uint32_t)0x2608U)
#define REG_ISP_YUV_SA_GCTL2			((uint32_t)0x260CU)


/******************************************************************
 * RGB Multi-Color Enhancement (2page 0x700~0x754)
 ******************************************************************/
#define REG_ISP_RGB_CE_CTL			((uint32_t)0x2700U)
#define REG_ISP_RGB_CE_CFG0			((uint32_t)0x2704U)
#define REG_ISP_RGB_CE_CFG1			((uint32_t)0x2708U)
#define REG_ISP_RGB_CE_CFG2			((uint32_t)0x270CU)
#define REG_ISP_RGB_CE_CFG3			((uint32_t)0x2710U)
#define REG_ISP_RGB_CE_CFG4			((uint32_t)0x2714U)
#define REG_ISP_RGB_CE_CFG5			((uint32_t)0x2718U)
#define REG_ISP_RGB_CE_CFG6			((uint32_t)0x271CU)
#define REG_ISP_RGB_CE_CFG7			((uint32_t)0x2720U)
#define REG_ISP_RGB_CE_CFG8			((uint32_t)0x2724U)


/******************************************************************
 * HDR (2page 0x810~0x8FF)
 ******************************************************************/
#define REG_ISP_HDR_CTL				((uint32_t)0x2810U)
#define REG_ISP_HDR_IMG				((uint32_t)0x2814U)

/* REG_ISP_RGBIR_CTL */
#define ISP_HDR_CTL_HE_SHIFT			((uint32_t)0U)

#define ISP_HDR_CTL_HE_MASK			\
	(((uint32_t)0x1U) << ISP_HDR_CTL_HE_SHIFT)

/******************************************************************
 * WDR (2page 0x900~0x96C)
 ******************************************************************/
#define REG_ISP_WDR_CTL				((uint32_t)0x2900U)
#define REG_ISP_WDR_GCTL0			((uint32_t)0x2904U)
#define REG_ISP_WDR_GCTL1			((uint32_t)0x2908U)
#define REG_ISP_WDR_GCTL2			((uint32_t)0x290CU)
#define REG_ISP_WDR_GCTL3			((uint32_t)0x2910U)
#define REG_ISP_WDR_GCTL4			((uint32_t)0x2914U)
#define REG_ISP_WDR_GCTL5			((uint32_t)0x2918U)
#define REG_ISP_WDR_GCTL6			((uint32_t)0x291CU)
#define REG_ISP_WDR_CFG0			((uint32_t)0x2920U)
#define REG_ISP_WDR_CFG1			((uint32_t)0x2924U)
#define REG_ISP_WDR_CUR_GCTL0			((uint32_t)0x2928U)
#define REG_ISP_WDR_CUR_GCTL1			((uint32_t)0x292CU)
#define REG_ISP_WDR_CUR_GCTL2			((uint32_t)0x2930U)
#define REG_ISP_WDR_CUR_GCTL3			((uint32_t)0x2934U)


/******************************************************************
 * ATI Configuration (2page 0xC00~0xC4E)
 ******************************************************************/
#define REG_ISP_ATI_I2C_SLV_CTL			((uint32_t)0x2C00U)
#define REG_ISP_ATI_I2C_SLV_AWD0		((uint32_t)0x2C04U)
#define REG_ISP_ATI_I2C_SLV_AWD1		((uint32_t)0x2C08U)
#define REG_ISP_ATI_I2C_SLV_AWD2		((uint32_t)0x2C0CU)
#define REG_ISP_ATI_I2C_SLV_AWD3		((uint32_t)0x2C10U)
#define REG_ISP_ATI_I2C_SLV_AWD4		((uint32_t)0x2C14U)
#define REG_ISP_ATI_I2C_SLV_AWD5		((uint32_t)0x2C18U)
#define REG_ISP_ATI_I2C_SLV_AWD6		((uint32_t)0x2C1CU)
#define REG_ISP_ATI_I2C_SLV_AWD7		((uint32_t)0x2C20U)
#define REG_ISP_ATI_I2C_SLV_RD01		((uint32_t)0x2C24U)
#define REG_ISP_ATI_I2C_SLV_RD23		((uint32_t)0x2C28U)
#define REG_ISP_ATI_I2C_SLV_RD45		((uint32_t)0x2C2CU)
#define REG_ISP_ATI_I2C_SLV_RD67		((uint32_t)0x2C30U)
#define REG_ISP_ATI_I2C_MST_CTL			((uint32_t)0x2C44U)
#define REG_ISP_ATI_I2C_MST_WD			((uint32_t)0x2C48U)
#define REG_ISP_ATI_I2C_MST_RD			((uint32_t)0x2C4CU)


/******************************************************************
 * ROSE(RGBIR) Register Control ( 2 page 0xD00~0xD86 )
 ******************************************************************/
#define REG_ISP_RGBIR_CTL			((uint32_t)0x2D00U)
#define REG_ISP_RGBIR_IMG			((uint32_t)0x2D04U)
#define REG_ISP_RGBIR_DPC0			((uint32_t)0x2D08U)
#define REG_ISP_RGBIR_DPC1			((uint32_t)0x2D0CU)
#define REG_ISP_RGBIR_INTP0			((uint32_t)0x2D10U)
#define REG_ISP_RGBIR_INTP1			((uint32_t)0x2D14U)
#define REG_ISP_RGBIR_INTP2			((uint32_t)0x2D18U)
#define REG_ISP_RGBIR_CROP_POS			((uint32_t)0x2D1CU)
#define REG_ISP_RGBIR_CROP_IMG			((uint32_t)0x2D20U)
#define REG_ISP_RGBIR_ICM_CTL			((uint32_t)0x2D30U)
#define REG_ISP_RGBIR_ICM_MAT0			((uint32_t)0x2D38U)
#define REG_ISP_RGBIR_ICM_MAT1			((uint32_t)0x2D3CU)
#define REG_ISP_RGBIR_ICM_MAT2			((uint32_t)0x2D40U)
#define REG_ISP_RGBIR_ICM_MAT3			((uint32_t)0x2D44U)
#define REG_ISP_RGBIR_ICM_MAT4			((uint32_t)0x2D48U)
#define REG_ISP_RGBIR_ICM_MAT5			((uint32_t)0x2D4CU)
#define REG_ISP_RGBIR_ICM_SIGN			((uint32_t)0x2D50U)
#define REG_ISP_RGBIR_ICM_GMAT			((uint32_t)0x2D54U)
#define REG_ISP_RGBIR_MIX0_Y0			((uint32_t)0x2D58U)
#define REG_ISP_RGBIR_MIX0_Y1			((uint32_t)0x2D5CU)
#define REG_ISP_RGBIR_MIX0_Y_INIT		((uint32_t)0x2D60U)
#define REG_ISP_RGBIR_MIX0_X0			((uint32_t)0x2D64U)
#define REG_ISP_RGBIR_MIX0_X1			((uint32_t)0x2D68U)
#define REG_ISP_RGBIR_MIX1_Y0			((uint32_t)0x2D6CU)
#define REG_ISP_RGBIR_MIX1_Y1			((uint32_t)0x2D70U)
#define REG_ISP_RGBIR_MIX1_Y_INIT		((uint32_t)0x2D74U)
#define REG_ISP_RGBIR_MIX1_X0			((uint32_t)0x2D78U)
#define REG_ISP_RGBIR_MIX1_X1			((uint32_t)0x2D7CU)
#define REG_ISP_RGBIR_MIX2_Y0			((uint32_t)0x2D80U)
#define REG_ISP_RGBIR_MIX2_Y1			((uint32_t)0x2D84U)
#define REG_ISP_RGBIR_MIX2_Y_INIT		((uint32_t)0x2D88U)
#define REG_ISP_RGBIR_MIX2_X0			((uint32_t)0x2D8CU)
#define REG_ISP_RGBIR_MIX2_X1			((uint32_t)0x2D90U)
#define REG_ISP_RGBIR_ICM_GCTL0			((uint32_t)0x2D94U)
#define REG_ISP_RGBIR_ICM_GCTL1			((uint32_t)0x2D98U)

/* REG_ISP_RGBIR_CTL */
#define ISP_RGBIR_CTL_RE_SHIFT			((uint32_t)0U)
#define ISP_RGBIR_CTL_RGE_SHIFT			((uint32_t)1U)
#define ISP_RGBIR_CTL_ROBP_SHIFT		((uint32_t)2U)
#define ISP_RGBIR_CTL_RIBP_SHIFT		((uint32_t)4U)
#define ISP_RGBIR_CTL_RSE_SHIFT			((uint32_t)8U)
#define ISP_RGBIR_CTL_RDE_SHIFT			((uint32_t)9U)
#define ISP_RGBIR_CTL_RWE_SHIFT			((uint32_t)12U)

#define ISP_RGBIR_CTL_RE_MASK			\
	(((uint32_t)0x1U) << ISP_RGBIR_CTL_RE_SHIFT)
#define ISP_RGBIR_CTL_RGE_MASK			\
	(((uint32_t)0x1U) << ISP_RGBIR_CTL_RGE_SHIFT)
#define ISP_RGBIR_CTL_ROBP_MASK			\
	(((uint32_t)0x3U) << ISP_RGBIR_CTL_ROBP_SHIFT)
#define ISP_RGBIR_CTL_RIBP_MASK			\
	(((uint32_t)0x7U) << ISP_RGBIR_CTL_RIBP_SHIFT)
#define ISP_RGBIR_CTL_RSE_MASK			\
	(((uint32_t)0x1U) << ISP_RGBIR_CTL_RSE_SHIFT)
#define ISP_RGBIR_CTL_RDE_MASK			\
	(((uint32_t)0x1U) << ISP_RGBIR_CTL_RDE_SHIFT)
#define ISP_RGBIR_CTL_RWE_MASK			\
	(((uint32_t)0x1U) << ISP_RGBIR_CTL_RWE_SHIFT)

#define ISP_RGBIR_CTL_ROBP_GR_FIRST		((uint32_t)0U)
#define ISP_RGBIR_CTL_ROBP_R_FIRST		((uint32_t)1U)
#define ISP_RGBIR_CTL_ROBP_B_FIRST		((uint32_t)2U)
#define ISP_RGBIR_CTL_ROBP_GB_FIRST		((uint32_t)3U)

/* REG_ISP_RGBIR_IMG */
#define ISP_RGBIR_IMG_RIW_SHIFT			((uint32_t)0U)
#define ISP_RGBIR_IMG_RIH_SHIFT			((uint32_t)16U)

#define ISP_RGBIR_IMG_RIW_MASK			\
	(((uint32_t)0xFFFFU) << ISP_RGBIR_IMG_RIW_SHIFT)
#define ISP_RGBIR_IMG_RIH_MASK			\
	(((uint32_t)0xFFFFU) << ISP_RGBIR_IMG_RIH_SHIFT)

/* REG_ISP_RGBIR_CROP_POS */
#define ISP_RGBIR_CROP_POS_RCXP_SHIFT		((uint32_t)0U)
#define ISP_RGBIR_CROP_POS_RCYP_SHIFT		((uint32_t)16U)

#define ISP_RGBIR_CROP_POS_RCXP_MASK		\
	(((uint32_t)0xFFFFU) << ISP_RGBIR_CROP_POS_RCXP_SHIFT)
#define ISP_RGBIR_CROP_POS_RCYP_MASK		\
	(((uint32_t)0xFFFFU) << ISP_RGBIR_CROP_POS_RCYP_SHIFT)

/* REG_ISP_RGBIR_CROP_IMG */
#define ISP_RGBIR_CROP_IMG_RCIW_SHIFT		((uint32_t)0U)
#define ISP_RGBIR_CROP_IMG_RCIH_SHIFT		((uint32_t)16U)

#define ISP_RGBIR_CROP_IMG_RCIW_MASK		\
	(((uint32_t)0xFFFFU) << ISP_RGBIR_CROP_IMG_RCIW_SHIFT)
#define ISP_RGBIR_CROP_IMG_RCIH_MASK		\
	(((uint32_t)0xFFFFU) << ISP_RGBIR_CROP_IMG_RCIH_SHIFT)

/******************************************************************
 * DAISY(RCCC) Register Control ( 2 page 0xDA0~0xDE0 )
 ******************************************************************/
#define REG_ISP_RCCC_CTL			((uint32_t)0x2DA0U)
#define REG_ISP_RCCC_IMG			((uint32_t)0x2DA4U)
#define REG_ISP_RCCC_PIX_CTL0			((uint32_t)0x2DA8U)
#define REG_ISP_RCCC_PXL_CTL1			((uint32_t)0x2DACU)
#define REG_ISP_RCCC_PXL_CTL2			((uint32_t)0x2DB0U)
#define REG_ISP_RCCC_RGM_0			((uint32_t)0x2DB4U)
#define REG_ISP_RCCC_RGM_1			((uint32_t)0x2DB8U)
#define REG_ISP_RCCC_RGM_2			((uint32_t)0x2DBCU)
#define REG_ISP_RCCC_RGM_3			((uint32_t)0x2DC0U)
#define REG_ISP_RCCC_RGM_4			((uint32_t)0x2DC4U)
#define REG_ISP_RCCC_RGM_5			((uint32_t)0x2DC8U)
#define REG_ISP_RCCC_CGM_0			((uint32_t)0x2DCCU)
#define REG_ISP_RCCC_CGM_1			((uint32_t)0x2DD0U)
#define REG_ISP_RCCC_CGM_2			((uint32_t)0x2DD4U)
#define REG_ISP_RCCC_CGM_3			((uint32_t)0x2DD8U)
#define REG_ISP_RCCC_CGM_4			((uint32_t)0x2DDCU)
#define REG_ISP_RCCC_CGM_5			((uint32_t)0x2DE0U)

/* REG_ISP_RCCC_CTL */
#define ISP_RCCC_CTL_RCE_SHIFT			((uint32_t)0U)
#define ISP_RCCC_CTL_RCIO_SHIFT			((uint32_t)1U)

#define ISP_RCCC_CTL_RCE_MASK			\
	(((uint32_t)0x1U) << ISP_RCCC_CTL_RCE_SHIFT)
#define ISP_RCCC_CTL_RCIO_MASK			\
	(((uint32_t)0x3U) << ISP_RCCC_CTL_RCIO_SHIFT)

/* REG_RCCC_IMG */
#define ISP_RCCC_IMG_RCIW_SHIFT			((uint32_t)0U)
#define ISP_RCCC_IMG_RCIH_SHIFT			((uint32_t)16U)

#define ISP_RCCC_IMG_RCIW_MASK			\
	(((uint32_t)0xFFFF) << ISP_RCCC_IMG_RCIW_SHIFT)
#define ISP_RCCC_IMG_RCIH_MASK			\
	(((uint32_t)0xFFFF) << ISP_RCCC_IMG_RCIH_SHIFT)


/******************************************************************
 * DMA Register Control ( 2 page 0xE00~0xE4C )
 ******************************************************************/
#define REG_ISP_WDMA_CTL0			((uint32_t)0x2E00U)
#define REG_ISP_WDMA_CTL1			((uint32_t)0x2E04U)
/* B0P0: WDMA bank 0 plane 0 */
#define REG_ISP_WDMA_B0P0			((uint32_t)0x2E08U)
#define REG_ISP_WDMA_B0P1			((uint32_t)0x2E0CU)
#define REG_ISP_WDMA_B0P2			((uint32_t)0x2E10U)
/* B0P0: WDMA bank 1 plane 0 */
#define REG_ISP_WDMA_B1P0			((uint32_t)0x2E14U)
#define REG_ISP_WDMA_B1P1			((uint32_t)0x2E18U)
#define REG_ISP_WDMA_B1P2			((uint32_t)0x2E1CU)
#define REG_ISP_RDMA_CTL			((uint32_t)0x2E30U)
#define REG_ISP_RDMA_IMG			((uint32_t)0x2E40U)
#define REG_ISP_RDMA_BLK_CTL			((uint32_t)0x2E44U)
#define REG_ISP_RDMA_BUF_CTL			((uint32_t)0x2E48U)

/* WDMA_CTL0 */
#define WDMA_CTL0_WDMA_ENABLE_SHIFT		((uint32_t)0U)
#define WDMA_CTL0_MCO_SHIFT			((uint32_t)2U)
#define WDMA_CTL0_DLR_SHIFT			((uint32_t)3U)
#define WDMA_CTL0_ER_SHIFT			((uint32_t)4U)
#define WDMA_CTL0_WCO_SHIFT			((uint32_t)8U)
#define WDMA_CTL0_WMM_SHIFT			((uint32_t)16U)
#define WDMA_CTL0_WBM_SHIFT			((uint32_t)24U)

#define WDMA_CTL0_WDMA_ENABLE_MASK		\
	(((uint32_t)0x1U) << WDMA_CTL0_WDMA_ENABLE_SHIFT)
#define WDMA_CTL0_MCO_MASK			\
	(((uint32_t)0x1U) << WDMA_CTL0_MCO_SHIFT)
#define WDMA_CTL0_DLR_MASK			\
	(((uint32_t)0x1U) << WDMA_CTL0_DLR_SHIFT)
#define WDMA_CTL0_ER_MASK			\
	(((uint32_t)0x1U) << WDMA_CTL0_ER_SHIFT)
#define WDMA_CTL0_WCO_MASK			\
	(((uint32_t)0x3U) << WDMA_CTL0_WCO_SHIFT)
#define WDMA_CTL0_WMM_MASK			\
	(((uint32_t)0xFFU) << WDMA_CTL0_WMM_SHIFT)
#define WDMA_CTL0_WBM_MASK			\
	(((uint32_t)0xFU) << WDMA_CTL0_WBM_SHIFT)

#define WDMA_CTL0_WDMA_DISABLE			((uint32_t)0U)
#define WDMA_CTL0_WDMA_ENABLE			((uint32_t)1U)

#define WDMA_CTL0_MCO_ON			((uint32_t)1U)
#define WDMA_CTL0_MCO_OFF			((uint32_t)0U)

#define WDMA_CTL0_DLR_ON			((uint32_t)1U)
#define WDMA_CTL0_DLR_OFF			((uint32_t)0U)

#define WDMA_CTL0_ER_ON				((uint32_t)1U)
#define WDMA_CTL0_ER_OFF			((uint32_t)0U)

#define WDMA_CTL0_WCO_YCBYCR			((uint32_t)0U)
#define WDMA_CTL0_WCO_YCRYCB			((uint32_t)1U)
#define WDMA_CTL0_WCO_CBYCRY			((uint32_t)2U)
#define WDMA_CTL0_WCO_CRYCBY			((uint32_t)3U)


/* WDMA CTL1 */
/* WCTH: WDMA_CBUF_TH */
#define WDMA_CTL1_WCTH_SHIFT			((uint32_t)0U)
/* WDTH: WDMA_DBUF_TH */
#define WDMA_CTL1_WDTH_SHIFT			((uint32_t)8U)

#define WDMA_CTL1_WCTH_MASK			\
	(((uint32_t)0xFU) << WDMA_CTL1_WCTH_SHIFT)
#define WDMA_CTL1_WDTH_MASK			\
	(((uint32_t)0xFFU) << WDMA_CTL1_WDTH_SHIFT)

/* RDMA_CTL (rdma control register) */
/* RE: RDMA_ENABLE */
#define ISP_RDMA_CTL_RE_SHIFT			((uint32_t)0U)
/* RDF: RDMA_DATA_FORMAT */
#define ISP_RDMA_CTL_RDF_SHIFT			((uint32_t)4U)
/* RAL: RDMA_AR_LEN */
#define ISP_RDMA_CTL_RAL_SHIFT			((uint32_t)8U)

#define ISP_RDMA_CTL_RE_MASK			\
	(((uint32_t)0x1U) << ISP_RDMA_CTL_RE_SHIFT)
#define ISP_RDMA_CTL_RDF_MASK			\
	(((uint32_t)0xFU) << ISP_RDMA_CTL_RDF_SHIFT)
#define ISP_RDMA_CTL_RAL_MASK			\
	(((uint32_t)0xFU) << ISP_RDMA_CTL_RAL_SHIFT)

#define ISP_RDMA_CTL_RE_DISABLE			((uint32_t)0U)
#define ISP_RDMA_CTL_RE_ENABLE			((uint32_t)1U)
#define ISP_RDMA_CTL_RDF_YUV420_ODD_CBCR_2P	((uint32_t)4U)
#define ISP_RDMA_CTL_RDF_YUV420_ODD_CBCR_3P	((uint32_t)5U)
#define ISP_RDMA_CTL_RDF_YUV420_EVEN_CBCR_2P	((uint32_t)6U)
#define ISP_RDMA_CTL_RDF_YUV420_EVEN_CBCR_3P	((uint32_t)7U)
#define ISP_RDMA_CTL_RDF_YUV422_1P		((uint32_t)8U)
#define ISP_RDMA_CTL_RDF_YUV422_2P		((uint32_t)9U)
#define ISP_RDMA_CTL_RDF_YUV422_3P		((uint32_t)10U)

/* RDMA_IMG */
#define ISP_RDMA_IMG_WIDTH_SHIFT		((uint32_t)0U)
#define ISP_RDMA_IMG_HEIGHT_SHIFT		((uint32_t)16U)

#define ISP_RDMA_IMG_WIDTH_MASK			\
	(((uint32_t)0x1FFFU) << ISP_RDMA_IMG_WIDTH_SHIFT)
#define ISP_RDMA_IMG_HEIGHT_MASK		\
	(((uint32_t)0x1FFFU) << ISP_RDMA_IMG_HEIGHT_SHIFT)

/* RDMA_BLK_CTL (rdma blank control register) */
/* RHBT: RDMA_H_BLK_TIME */
#define ISP_RDMA_BLK_CTL_RHBT_SHIFT		((uint32_t)0U)
/* RVBT: RDMA_V_BLK_TIME */
#define ISP_RDMA_BLK_CTL_RVBT_SHFIT		((uint32_t)16U)

#define ISP_RDMA_BLK_CTL_RHBT_MASK		\
	(((uint32_t)0xFFU) << ISP_RDMA_BLK_CTL_RHBT_SHIFT)
#define ISP_RDMA_BLK_CTL_RVBT_MASK		\
	(((uint32_t)0xFFU) << ISP_RDMA_BLK_CTL_RVBT_SHFIT)

/* RDMA_BUF_CTL (rdma buffer control register) */
/* RMM: RDMA_MAX_MOR */
#define ISP_RDMA_BUF_CTL_RMM_SHIFT		((uint32_t)0U)
/* RBTH0: RDMA_BUF_TH0 */
#define ISP_RDMA_BUF_CTL_RBTH0_SHIFT		((uint32_t)16U)
/* RBTH1: RDMA_BUF_TH1 */
#define ISP_RDMA_BUF_CTL_RBTH1_SHIFT		((uint32_t)24U)

#define ISP_RDMA_BUF_CTL_RMM_MASK		\
	(((uint32_t)0xFFU) << ISP_RDMA_BUF_CTL_RMM_SHIFT)
#define ISP_RDMA_BUF_CTL_RBTH0_MASK		\
	(((uint32_t)0xFFU) << ISP_RDMA_BUF_CTL_RBTH0_SHIFT)
#define ISP_RDMA_BUF_CTL_RBTH1_MASK		\
	(((uint32_t)0xFFU) << ISP_RDMA_BUF_CTL_RBTH1_MASK)


/******************************************************************
 * AMUR(3DNR) Register Control ( 2 page 0xE80~0xEB0 )
 ******************************************************************/
#define REG_ISP_3DNR_CTL			((uint32_t)0x2E80U)
#define REG_ISP_3DNR_IMG			((uint32_t)0x2E84U)
#define REG_ISP_3DNR_PATCH			((uint32_t)0x2E88U)
#define REG_ISP_3DNR_YTH_CTL0			((uint32_t)0x2E8CU)
#define REG_ISP_3DNR_YTH_CTL1			((uint32_t)0x2E90U)
#define REG_ISP_3DNR_YMD_WGT0			((uint32_t)0x2E94U)
#define REG_ISP_3DNR_YMD_WGT1			((uint32_t)0x2E98U)
#define REG_ISP_3DNR_YNR_RATIO			((uint32_t)0x2E9CU)
#define REG_ISP_3DNR_CTH_CTL0			((uint32_t)0x2EA0U)
#define REG_ISP_3DNR_CTH_CTL1			((uint32_t)0x2EA4U)
#define REG_ISP_3DNR_CMD_WGT0			((uint32_t)0x2EA8U)
#define REG_ISP_3DNR_CMD_WGT1			((uint32_t)0x2EACU)
#define REG_ISP_3DNR_CNR_RATIO			((uint32_t)0x2EB0U)

/* REG_ISP_3DNR_CTL */
#define ISP_3DNR_CTL_3DYE_SHIFT			((uint32_t)0U)
#define ISP_3DNR_CTL_3DCE_SHIFT			((uint32_t)1U)
#define ISP_3DNR_CTL_Y2DCS_SHIFT		((uint32_t)4U)
#define ISP_3DNR_CTL_RSC_SHIFT			((uint32_t)6U)

#define ISP_3DNR_CTL_3DYE_MASK			\
	(((uint32_t)0U) << ISP_3DNR_CTL_3DYE_SHIFT)
#define ISP_3DNR_CTL_3DCE_MASK			\
	(((uint32_t)1U) << ISP_3DNR_CTL_3DCE_SHIFT)
#define ISP_3DNR_CTL_Y2DCS_MASK			\
	(((uint32_t)4U) << ISP_3DNR_CTL_Y2DCS_SHIFT)
#define ISP_3DNR_CTL_RSC_MASK			\
	(((uint32_t)6U) << ISP_3DNR_CTL_RSC_SHIFT)

/* REG_3DNR_IMG */
#define ISP_3DNR_IMG_AIW_SHIFT			((uint32_t)0U)
#define ISP_3DNR_IMG_AIH_SHIFT			((uint32_t)16U)

#define ISP_3DNR_IMG_AIW_MASK			\
	(((uint32_t)0xFFFF) << ISP_3DNR_IMG_AIW_SHIFT)
#define ISP_3DNR_IMG_AIH_MASK			\
	(((uint32_t)0xFFFF) << ISP_3DNR_IMG_AIH_SHIFT)

/* REG_ISP_3DNR_PATCH */
#define ISP_3DNR_PATCH_3DPW_SHIFT		((uint32_t)0U)
#define ISP_3DNR_PATCH_3DPH_SHIFT		((uint32_t)8U)
#define ISP_3DNR_PATCH_3DPXS_SHIFT		((uint32_t)16U)
#define ISP_3DNR_PATCH_3DPYS_SHIFT		((uint32_t)24U)

#define ISP_3DNR_PATCH_3DPW_MASK		((uint32_t)0xFFU)
#define ISP_3DNR_PATCH_3DPH_MASK		((uint32_t)0xFFU)
#define ISP_3DNR_PATCH_3DPXS_MASK		((uint32_t)0xFFU)
#define ISP_3DNR_PATCH_3DPYS_MASK		((uint32_t)0xFFU)


/******************************************************************
 * LOTUS(RCCB) Register Control ( 2 page 0xF00~0xF30 )
 ******************************************************************/
#define REG_ISP_RCCB_CTL			((uint32_t)0x2F00)
#define REG_ISP_RCCB_IMG			((uint32_t)0x2F04)
#define REG_ISP_RCCB_NR_RATE0			((uint32_t)0x2F08)
#define REG_ISP_RCCB_NR_RATE1			((uint32_t)0x2F0C)
#define REG_ISP_RCCB_NR_RATE2			((uint32_t)0x2F10)
#define REG_ISP_RCCB_SH_RATE0			((uint32_t)0x2F14)
#define REG_ISP_RCCB_SH_RATE1			((uint32_t)0x2F18)
#define REG_ISP_RCCB_INTP			((uint32_t)0x2F1C)
#define REG_ISP_RCCB_PIX0_MAT			((uint32_t)0x2F20)
#define REG_ISP_RCCB_PIX1_MAT			((uint32_t)0x2F24)
#define REG_ISP_RCCB_PRE_PIX			((uint32_t)0x2F28)
#define REG_ISP_RCCB_POST_PIX			((uint32_t)0x2F2C)

/* REG_ISP_RCCB_CTL */
#define ISP_RCCB_CTL_RBE_SHIFT			((uint32_t)0U)
#define ISP_RCCB_CTL_RBNE_SHIFT			((uint32_t)1U)
#define ISP_RCCB_CTL_RBSE_SHIFT			((uint32_t)2U)
#define ISP_RCCB_CTL_RBCE_SHIFT			((uint32_t)4U)
#define ISP_RCCB_CTL_RBFE_SHIFT			((uint32_t)5U)
#define ISP_RCCB_CTL_SEB_SHIFT			((uint32_t)8U)
#define ISP_RCCB_CTL_SER_SHIFT			((uint32_t)9U)
#define ISP_RCCB_CTL_RBIO_SHIFT			((uint32_t)12U)
#define ISP_RCCB_CTL_RBDRC_SHIFT		((uint32_t)16U)
#define ISP_RCCB_CTL_RBDRN_SHIFT		((uint32_t)24U)

#define ISP_RCCB_CTL_RBE_MASK			\
	(((uint32_t)0x1U) << ISP_RCCB_CTL_RBE_SHIFT)
#define ISP_RCCB_CTL_RBNE_MASK			\
	(((uint32_t)0x1U) << ISP_RCCB_CTL_RBNE_SHIFT)
#define ISP_RCCB_CTL_RBSE_MASK			\
	(((uint32_t)0x1U) << ISP_RCCB_CTL_RBSE_SHIFT)
#define ISP_RCCB_CTL_RBCE_MASK			\
	(((uint32_t)0x1U) << ISP_RCCB_CTL_RBCE_SHIFT)
#define ISP_RCCB_CTL_RBFE_MASK			\
	(((uint32_t)0x1U) << ISP_RCCB_CTL_RBFE_SHIFT)
#define ISP_RCCB_CTL_SEB_MASK			\
	(((uint32_t)0x1U) << ISP_RCCB_CTL_SEB_SHIFT)
#define ISP_RCCB_CTL_SER_MASK			\
	(((uint32_t)0x1U) << ISP_RCCB_CTL_SER_SHIFT)
#define ISP_RCCB_CTL_RBIO_MASK			\
	(((uint32_t)0x3U) << ISP_RCCB_CTL_RBIO_SHIFT)
#define ISP_RCCB_CTL_RBDRC_MASK			\
	(((uint32_t)0xFFU) << ISP_RCCB_CTL_RBDRC_SHIFT)
#define ISP_RCCB_CTL_RBDRN_MASK			\
	(((uint32_t)0xFFU) << ISP_RCCB_CTL_RBDRN_SHIFT)

#define ISP_RCCB_CTL_RBE_DISABLE		((uint32_t)0U)
#define ISP_RCCB_CTL_RBE_ENABLE			((uint32_t)1U)

#define ISP_RCCB_CTL_RBNE_DISABLE		((uint32_t)0U)
#define ISP_RCCB_CTL_RBNE_ENABLE		((uint32_t)1U)

#define ISP_RCCB_CTL_RBSE_DISABLE		((uint32_t)0U)
#define ISP_RCCB_CTL_RBSE_ENABLE		((uint32_t)1U)

#define ISP_RCCB_CTL_RBCE_DISABLE		((uint32_t)0U)
#define ISP_RCCB_CTL_RBCE_ENABLE		((uint32_t)1U)

#define ISP_RCCB_CTL_RBFE_DISABLE		((uint32_t)0U)
#define ISP_RCCB_CTL_RBFE_ENABLE		((uint32_t)1U)

#define ISP_RCCB_CTL_RBIO_B_FIRST		((uint32_t)0U)
#define ISP_RCCB_CTL_RBIO_CB_FIRST		((uint32_t)1U)
#define ISP_RCCB_CTL_RBIO_CR_FIRST		((uint32_t)2U)
#define ISP_RCCB_CTL_RBIO_R_FIRST		((uint32_t)3U)


/* REG_ISP_RCCB_IMG */
#define ISP_RCCB_IMG_RBIW_SHIFT			((uint32_t)0U)
#define ISP_RCCB_IMG_RBIH_SHIFT			((uint32_t)16U)

#define ISP_RCCB_IMG_RBIW_MASK			\
	(((uint32_t)0xFFFF) << ISP_RCCB_IMG_RBIW_SHIFT)
#define ISP_RCCB_IMG_RBIH_MASK			\
	(((uint32_t)0xFFFF) << ISP_RCCB_IMG_RBIH_SHIFT)

/******************************************************************
 * MCU Configuration
 ******************************************************************/
#define ISP_MEM_SIZE_PROGRAM			\
		((uint32_t)(128U) * (uint32_t)(1024U))
#define ISP_MEM_SIZE_ADT			\
		((uint32_t)(8U) * (uint32_t)(1024U))


/******************************************************************
 * RGB-IR Synchronizer
 ******************************************************************/
#define REG_RGBIR_SYNC_RESOLUTION		((uint32_t)0x0000U)
#define REG_RGBIR_SYNC_INTERPOLATION_MODE	((uint32_t)0x0004U)

/* Resolution */
#define RGBIR_SYNC_RESOLUTION_WIDTH_SHIFT	((uint32_t)0U)
#define RGBIR_SYNC_RESOLUTION_HEIGHT_SHIFT	((uint32_t)16U)

#define RGBIR_SYNC_RESOLUTION_WIDTH_MASK	\
	(((uint32_t)0xFFF) << RGBIR_SYNC_RESOLUTION_WIDTH_SHIFT)
#define RGBIR_SYNC_RESOLUTION_HEIGHT_MASK	\
	(((uint32_t)0xFFF) << RGBIR_SYNC_RESOLUTION_HEIGHT_SHIFT)

/* Interpolation mode */
#define RGBIR_SYNC_INTERPOLATION_MODE_MODE_SHIFT	((uint32_t)0U)
#define RGBIR_SYNC_INTERPOLATION_MODE_RGB_BYPASS_SHIFT	((uint32_t)16U)
#define RGBIR_SYNC_INTERPOLATION_MODE_IR_BYPASS_SHIFT	((uint32_t)17U)

#define RGBIR_SYNC_INTERPOLATION_MODE_MODE_MASK		\
	(((uint32_t)0x3U) << RGBIR_SYNC_INTERPOLATION_MODE_MODE_SHIFT)
#define RGBIR_SYNC_INTERPOLATION_MODE_RGB_BYPASS_MASK	\
	(((uint32_t)0x1U) << RGBIR_SYNC_INTERPOLATION_MODE_RGB_BYPASS_SHIFT)
#define RGBIR_SYNC_INTERPOLATION_MODE_IR_BYPASS_MASK	\
	(((uint32_t)0x1U) << RGBIR_SYNC_INTERPOLATION_MODE_IR_BYPASS_SHIFT)

#define RGBIR_SYNC_INTERPOLATION_MODE0			((uint32_t)0u)
#define RGBIR_SYNC_INTERPOLATION_MODE1			((uint32_t)1u)
#define RGBIR_SYNC_INTERPOLATION_MODE2			((uint32_t)2u)
#define RGBIR_SYNC_INTERPOLATION_MODE3			((uint32_t)3u)

#define TCC_ISP_CORE_CMD_ID_ISP0_CORE		((uint32_t)0x82U)
#define TCC_ISP_CORE_CMD_ID_ISP1_CORE		((uint32_t)0x84U)
#define TCC_ISP_CORE_CMD_ID_ISP2_CORE		((uint32_t)0x86U)
#define TCC_ISP_CORE_CMD_ID_ISP3_CORE		((uint32_t)0x88U)
#define TCC_ISP_CORE_CMD_ID_SCENE_DATA		((uint32_t)0x8EU)

#endif
