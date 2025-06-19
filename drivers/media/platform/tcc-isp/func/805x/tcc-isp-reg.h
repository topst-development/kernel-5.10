/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_ISP_REG_H
#define TCC_ISP_REG_H

#include "../../../tcc-mipi-csi2/805x/tcc-mipi-csi2-cfg-reg.h"

#define TCC_ISP_MAX_CORE			((uint32_t)4U)

#define TCC_ISP_PROBED				((uint32_t)1U)
#define TCC_ISP_NOPROBED			((uint32_t)0U)
#define TCC_ISP_STARTED				((uint32_t)1U)
#define TCC_ISP_STOPPED				((uint32_t)0U)
/******************************************************************
 * ISP Factory Only Register Control ( 0 page 0x000~0xFFF )
 ******************************************************************/
#define REG_ISP_SLEEP_MODE			((uint32_t)0x0004U)
#define REG_ISP_SOFT_RESET			((uint32_t)0x0008U)
#define REG_ISP_MEM_SHARE			((uint32_t)0x000CU)
#define REG_ISP_IMG_WIDTH			((uint32_t)0x0010U)
#define REG_ISP_IMG_HEIGHT			((uint32_t)0x0014U)
#define REG_ISP_UP_CTL				((uint32_t)0x0030U)
#define REG_ISP_UP_SEL1				((uint32_t)0x0034U)
#define REG_ISP_UP_SEL2				((uint32_t)0x0038U)
#define REG_ISP_UP_MODE1			((uint32_t)0x003CU)
#define REG_ISP_UP_MODE2			((uint32_t)0x0040U)
#define REG_ISP_USR_CNT1			((uint32_t)0x0048U)
#define REG_ISP_USR_CNT2			((uint32_t)0x004CU)
#define REG_ISP_INTER_MSK			((uint32_t)0x0060U)
#define REG_ISP_INTER_CLR			((uint32_t)0x0064U)
#define REG_ISP_INTER_STATUS			((uint32_t)0x0068U)


/* SLEEP_MODE */
#define SLEEP_MODE_SLEEP_MODE_MASK		((uint32_t)0x1U)

#define SLEEP_MODE_SLEEP_MODE_SHIFT		((uint32_t)0U)

#define SLEEP_MODE_SLEEP_MODE_DISABLE		((uint32_t)0U)
#define SLEEP_MODE_SLEEP_MODE_ENABLE		((uint32_t)1U)


/* SOFT_RESET */
#define SOFT_RESET_ISP_SOFT_RESET_MASK		((uint32_t)0x1U)
#define SOFT_RESET_MCU_SOFT_RESET_MASK		((uint32_t)0x1U)

#define SOFT_RESET_ISP_SOFT_RESET_SHIFT		((uint32_t)0U)
#define SOFT_RESET_MCU_SOFT_RESET_SHIFT		((uint32_t)1U)

#define SOFT_RESET_ISP_SOFT_RESET_RELEASE	((uint32_t)0U)
#define SOFT_RESET_ISP_SOFT_RESET_RESET		((uint32_t)1U)
#define SOFT_RESET_MCU_SOFT_RESET_RELEASE	((uint32_t)0U)
#define SOFT_RESET_MCU_SOFT_RESET_RESET		((uint32_t)1U)


/* MEM_SHARE */
#define MEM_SHARE_MEM_SHARE_EN_MASK		((uint32_t)0x1U)

#define MEM_SHARE_MEM_SHARE_EN_SHIFT		((uint32_t)0U)

#define MEM_SHARE_MEM_SHARE_EN_DISABLE		((uint32_t)0U)
#define MEM_SHARE_MEM_SHARE_EN_ENABLE		((uint32_t)1U)


/* IMG_WIDTH */
#define IMG_WIDTH_IN_IMG_WIDTH_MASK		((uint32_t)0xFFFFU)

#define IMG_WIDTH_IN_IMG_WIDTH_SHIFT		((uint32_t)16U)


/* IMG_HEIGHT */
#define IMG_HEIGHT_IN_IMG_HEIGHT_MASK		((uint32_t)0xFFFFU)

#define IMG_HEIGHT_IN_IMG_HEIGHT_SHIFT		((uint32_t)16U)


/* UP_CTL */
#define UP_CTL_TP_UP_CTL_MASK			((uint32_t)0x1U)
#define UP_CTL_RAW_CH_GAIN_UP_CTL_MASK		((uint32_t)0x1U)
#define UP_CTL_LSC_UP_CTL_MASK			((uint32_t)0x1U)
#define UP_CTL_RAW_AWB_GAIN_UP_CTL_MASK		((uint32_t)0x1U)
#define UP_CTL_RAW_CURVE_UP_CTL_MASK		((uint32_t)0x1U)
#define UP_CTL_PRE_RAW_UP_CTL_MASK		((uint32_t)0x1U)
#define UP_CTL_POST_RAW_UP_CTL_MASK		((uint32_t)0x1U)
#define UP_CTL_RAW_INT_UP_CTL_MASK		((uint32_t)0x1U)
#define UP_CTL_AF_UP_CTL_MASK			((uint32_t)0x1U)
#define UP_CTL_TRI_AUTO_UP_CTL_MASK		((uint32_t)0x1U)
#define UP_CTL_CCM_UP_CTL_MASK			((uint32_t)0x1U)
#define UP_CTL_CONT_UP_CTL_MASK			((uint32_t)0x1U)
#define UP_CTL_YUV_PROC2_UP_CTL_MASK		((uint32_t)0x1U)
#define UP_CTL_YUV_PROC1_UP_CTL_MASK		((uint32_t)0x1U)
#define UP_CTL_WIN_UP_CTL_MASK			((uint32_t)0x1U)
#define UP_CTL_DEBLANK_UP_CTL_MASK		((uint32_t)0x1U)

#define UP_CTL_TP_UP_CTL_SHIFT			((uint32_t)16U)
#define UP_CTL_RAW_CH_GAIN_UP_CTL_SHIFT		((uint32_t)17U)
#define UP_CTL_LSC_UP_CTL_SHIFT			((uint32_t)18U)
#define UP_CTL_RAW_AWB_GAIN_UP_CTL_SHIFT	((uint32_t)19U)
#define UP_CTL_RAW_CURVE_UP_CTL_SHIFT		((uint32_t)20U)
#define UP_CTL_PRE_RAW_UP_CTL_SHIFT		((uint32_t)21U)
#define UP_CTL_POST_RAW_UP_CTL_SHIFT		((uint32_t)22U)
#define UP_CTL_RAW_INT_UP_CTL_SHIFT		((uint32_t)23U)
#define UP_CTL_AF_UP_CTL_SHIFT			((uint32_t)24U)
#define UP_CTL_TRI_AUTO_UP_CTL_SHIFT		((uint32_t)25U)
#define UP_CTL_CCM_UP_CTL_SHIFT			((uint32_t)26U)
#define UP_CTL_CONT_UP_CTL_SHIFT		((uint32_t)27U)
#define UP_CTL_YUV_PROC2_UP_CTL_SHIFT		((uint32_t)28U)
#define UP_CTL_YUV_PROC1_UP_CTL_SHIFT		((uint32_t)29U)
#define UP_CTL_WIN_UP_CTL_SHIFT			((uint32_t)30U)
#define UP_CTL_DEBLANK_UP_CTL_SHIFT		((uint32_t)31U)

#define UP_CTL_TP_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_RAW_CH_GAIN_UP_CTL		((uint32_t)0x1U)
#define UP_CTL_LSC_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_RAW_AWB_GAIN_UP_CTL		((uint32_t)0x1U)
#define UP_CTL_RAW_CURVE_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_PRE_RAW_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_POST_RAW_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_RAW_INT_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_AF_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_TRI_AUTO_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_CCM_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_CONT_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_YUV_PROC2_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_YUV_PROC1_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_WIN_UP_CTL			((uint32_t)0x1U)
#define UP_CTL_DEBLANK_UP_CTL			((uint32_t)0x1U)

#define UP_CTL_UP_ALL					\
	((UP_CTL_TP_UP_CTL << UP_CTL_TP_UP_CTL_SHIFT) | \
	(UP_CTL_RAW_CH_GAIN_UP_CTL << UP_CTL_RAW_CH_GAIN_UP_CTL_SHIFT) | \
	(UP_CTL_LSC_UP_CTL << UP_CTL_LSC_UP_CTL_SHIFT) | \
	(UP_CTL_RAW_AWB_GAIN_UP_CTL << UP_CTL_RAW_AWB_GAIN_UP_CTL_SHIFT) | \
	(UP_CTL_RAW_CURVE_UP_CTL << UP_CTL_RAW_CURVE_UP_CTL_SHIFT) | \
	(UP_CTL_PRE_RAW_UP_CTL << UP_CTL_PRE_RAW_UP_CTL_SHIFT) | \
	(UP_CTL_POST_RAW_UP_CTL << UP_CTL_POST_RAW_UP_CTL_SHIFT) | \
	(UP_CTL_RAW_INT_UP_CTL << UP_CTL_RAW_INT_UP_CTL_SHIFT) | \
	(UP_CTL_AF_UP_CTL << UP_CTL_AF_UP_CTL_SHIFT) | \
	(UP_CTL_TRI_AUTO_UP_CTL << UP_CTL_TRI_AUTO_UP_CTL_SHIFT) | \
	(UP_CTL_CCM_UP_CTL << UP_CTL_CCM_UP_CTL_SHIFT) | \
	(UP_CTL_CONT_UP_CTL << UP_CTL_CONT_UP_CTL_SHIFT) | \
	(UP_CTL_YUV_PROC2_UP_CTL << UP_CTL_YUV_PROC2_UP_CTL_SHIFT) | \
	(UP_CTL_YUV_PROC1_UP_CTL << UP_CTL_YUV_PROC1_UP_CTL_SHIFT) | \
	(UP_CTL_WIN_UP_CTL << UP_CTL_WIN_UP_CTL_SHIFT) | \
	(UP_CTL_DEBLANK_UP_CTL << UP_CTL_DEBLANK_UP_CTL_SHIFT))


/* UP_SEL1 */
#define UP_SEL1_TP_SEL_CTL_MASK			((uint32_t)0x3U)
#define UP_SEL1_RAW_CH_GAIN_SEL_CTL_MASK	((uint32_t)0x3U)
#define UP_SEL1_LSC_SEL_CTL_MASK		((uint32_t)0x3U)
#define UP_SEL1_RAW_AWB_GAIN_SEL_CTL_MASK	((uint32_t)0x3U)
#define UP_SEL1_RAW_CURVE_SEL_CTL_MASK		((uint32_t)0x3U)
#define UP_SEL1_PRE_RAW_SEL_CTL_MASK		((uint32_t)0x3U)
#define UP_SEL1_POST_RAW_SEL_CTL_MASK		((uint32_t)0x3U)
#define UP_SEL1_RAW_INT_SEL_CTL_MASK		((uint32_t)0x3U)

#define UP_SEL1_TP_SEL_CTL_SHIFT		((uint32_t)16U)
#define UP_SEL1_RAW_CH_GAIN_SEL_CTL_SHIFT	((uint32_t)18U)
#define UP_SEL1_LSC_SEL_CTL_SHIFT		((uint32_t)20U)
#define UP_SEL1_RAW_AWB_GAIN_SEL_CTL_SHIFT	((uint32_t)22U)
#define UP_SEL1_RAW_CURVE_SEL_CTL_SHIFT		((uint32_t)24U)
#define UP_SEL1_PRE_RAW_SEL_CTL_SHIFT		((uint32_t)26U)
#define UP_SEL1_POST_RAW_SEL_CTL_SHIFT		((uint32_t)28U)
#define UP_SEL1_RAW_INT_SEL_CTL_SHIFT		((uint32_t)30U)

#define UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING		\
		((uint32_t)0U)
#define UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING	\
		((uint32_t)1U)
#define UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING		\
		((uint32_t)2U)
#define UP_SEL_SYNC_ON_WRITING_TIMING			\
		((uint32_t)3U)


#define UP_SEL1_ALL_SYNC_ON_USER_SPECIFIED_TIMING \
	((UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL1_TP_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL1_RAW_CH_GAIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL1_LSC_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL1_RAW_AWB_GAIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL1_RAW_CURVE_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL1_PRE_RAW_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL1_POST_RAW_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	UP_SEL1_RAW_INT_SEL_CTL_SHIFT))

#define UP_SEL1_ALL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING \
	((UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	 UP_SEL1_TP_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	 UP_SEL1_RAW_CH_GAIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	 UP_SEL1_LSC_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	 UP_SEL1_RAW_AWB_GAIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	 UP_SEL1_RAW_CURVE_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	 UP_SEL1_PRE_RAW_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	 UP_SEL1_POST_RAW_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	 UP_SEL1_RAW_INT_SEL_CTL_SHIFT))

#define UP_SEL1_ALL_SYNC_ON_VSYNC_RISING_EDGE_TIMING \
	((UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	 UP_SEL1_TP_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	 UP_SEL1_RAW_CH_GAIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	 UP_SEL1_LSC_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	 UP_SEL1_RAW_AWB_GAIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	 UP_SEL1_RAW_CURVE_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	 UP_SEL1_PRE_RAW_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	 UP_SEL1_POST_RAW_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	 UP_SEL1_RAW_INT_SEL_CTL_SHIFT))

#define UP_SEL1_ALL_SYNC_ON_WRITING_TIMING \
	((UP_SEL_SYNC_ON_WRITING_TIMING << \
	 UP_SEL1_TP_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	 UP_SEL1_RAW_CH_GAIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	 UP_SEL1_LSC_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	 UP_SEL1_RAW_AWB_GAIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	 UP_SEL1_RAW_CURVE_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	 UP_SEL1_PRE_RAW_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	 UP_SEL1_POST_RAW_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << \
	 UP_SEL1_RAW_INT_SEL_CTL_SHIFT))


/* UP_SEL2 */
#define UP_SEL2_AF_SEL_CTL_MASK			((uint32_t)0x3U)
#define UP_SEL2_TRI_AUTO_SEL_CTL_MASK		((uint32_t)0x3U)
#define UP_SEL2_CCM_SEL_CTL_MASK		((uint32_t)0x3U)
#define UP_SEL2_CONT_SEL_CTL_MASK		((uint32_t)0x3U)
#define UP_SEL2_YUV_PROC2_SEL_CTL_MASK		((uint32_t)0x3U)
#define UP_SEL2_YUV_PROC1_SEL_CTL_MASK		((uint32_t)0x3U)
#define UP_SEL2_WIN_SEL_CTL_MASK		((uint32_t)0x3U)
#define UP_SEL2_DEBLANK_SEL_CTL_MASK		((uint32_t)0x3U)

#define UP_SEL2_AF_SEL_CTL_SHIFT		((uint32_t)16U)
#define UP_SEL2_TRI_AUTO_SEL_CTL_SHIFT		((uint32_t)18U)
#define UP_SEL2_CCM_SEL_CTL_SHIFT		((uint32_t)20U)
#define UP_SEL2_CONT_SEL_CTL_SHIFT		((uint32_t)22U)
#define UP_SEL2_YUV_PROC2_SEL_CTL_SHIFT		((uint32_t)24U)
#define UP_SEL2_YUV_PROC1_SEL_CTL_SHIFT		((uint32_t)26U)
#define UP_SEL2_WIN_SEL_CTL_SHIFT		((uint32_t)28U)
#define UP_SEL2_DEBLANK_SEL_CTL_SHIFT		((uint32_t)30U)


#define UP_SEL2_ALL_SYNC_ON_USER_SPECIFIED_TIMING \
	((UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	 UP_SEL2_AF_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	 UP_SEL2_TRI_AUTO_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	 UP_SEL2_CCM_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	 UP_SEL2_CONT_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	 UP_SEL2_YUV_PROC2_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	 UP_SEL2_YUV_PROC1_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	 UP_SEL2_WIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_USER_SPECIFIED_TIMING << \
	 UP_SEL2_DEBLANK_SEL_CTL_SHIFT))

#define UP_SEL2_ALL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING \
	((UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	 UP_SEL2_AF_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	 UP_SEL2_TRI_AUTO_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	 UP_SEL2_CCM_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	 UP_SEL2_CONT_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	 UP_SEL2_YUV_PROC2_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	 UP_SEL2_YUV_PROC1_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	 UP_SEL2_WIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_FALLING_EDGE_TIMING << \
	 UP_SEL2_DEBLANK_SEL_CTL_SHIFT))

#define UP_SEL2_ALL_SYNC_ON_VSYNC_RISING_EDGE_TIMING \
	((UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	 UP_SEL2_AF_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	 UP_SEL2_TRI_AUTO_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	 UP_SEL2_CCM_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	 UP_SEL2_CONT_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	 UP_SEL2_YUV_PROC2_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	 UP_SEL2_YUV_PROC1_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	 UP_SEL2_WIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_VSYNC_RISING_EDGE_TIMING << \
	 UP_SEL2_DEBLANK_SEL_CTL_SHIFT))

#define UP_SEL2_ALL_SYNC_ON_WRITING_TIMING \
	((UP_SEL_SYNC_ON_WRITING_TIMING << UP_SEL2_AF_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << UP_SEL2_TRI_AUTO_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << UP_SEL2_CCM_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << UP_SEL2_CONT_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << UP_SEL2_YUV_PROC2_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << UP_SEL2_YUV_PROC1_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << UP_SEL2_WIN_SEL_CTL_SHIFT) | \
	(UP_SEL_SYNC_ON_WRITING_TIMING << UP_SEL2_DEBLANK_SEL_CTL_SHIFT))


/* UP_MODE1 */
#define UP_MODE1_TP_MASK			((uint32_t)0x1U)
#define UP_MODE1_RAW_CH_GAIN_MASK		((uint32_t)0x1U)
#define UP_MODE1_LSC_MASK			((uint32_t)0x1U)
#define UP_MODE1_RAW_AWB_GAIN_MASK		((uint32_t)0x1U)
#define UP_MODE1_RAW_CURVE_MASK			((uint32_t)0x1U)
#define UP_MODE1_PRE_RAW_MASK			((uint32_t)0x1U)
#define UP_MODE1_POST_RAW_MASK			((uint32_t)0x1U)
#define UP_MODE1_RAW_INT_MASK			((uint32_t)0x1U)
#define UP_MODE1_AF_MASK			((uint32_t)0x1U)
#define UP_MODE1_TRI_AUTO_MASK			((uint32_t)0x1U)
#define UP_MODE1_CCM_MASK			((uint32_t)0x1U)
#define UP_MODE1_CONT_MASK			((uint32_t)0x1U)
#define UP_MODE1_YUV_PROC2_MASK			((uint32_t)0x1U)
#define UP_MODE1_YUV_PROC1_MASK			((uint32_t)0x1U)
#define UP_MODE1_WIN_MASK			((uint32_t)0x1U)
#define UP_MODE1_DEBLANK_MASK			((uint32_t)0x1U)

#define UP_MODE1_TP_SHIFT			((uint32_t)16U)
#define UP_MODE1_RAW_CH_GAIN_SHIFT		((uint32_t)17U)
#define UP_MODE1_LSC_SHIFT			((uint32_t)18U)
#define UP_MODE1_RAW_AWB_GAIN_SHIFT		((uint32_t)19U)
#define UP_MODE1_RAW_CURVE_SHIFT		((uint32_t)20U)
#define UP_MODE1_PRE_RAW_SHIFT			((uint32_t)21U)
#define UP_MODE1_POST_RAW_SHIFT			((uint32_t)22U)
#define UP_MODE1_RAW_INT_SHIFT			((uint32_t)23U)
#define UP_MODE1_AF_SHIFT			((uint32_t)24U)
#define UP_MODE1_TRI_AUTO_SHIFT			((uint32_t)25U)
#define UP_MODE1_CCM_SHIFT			((uint32_t)26U)
#define UP_MODE1_CONT_SHIFT			((uint32_t)27U)
#define UP_MODE1_YUV_PROC2_SHIFT		((uint32_t)28U)
#define UP_MODE1_YUV_PROC1_SHIFT		((uint32_t)29U)
#define UP_MODE1_WIN_SHIFT			((uint32_t)30U)
#define UP_MODE1_DEBLANK_SHIFT			((uint32_t)31U)

#define UP_MODE1_SYNC_MODE			((uint32_t)0U)
#define UP_MODE1_ASYNC_MODE			((uint32_t)1U)

#define UP_MODE1_ALL_ASYNC_MODE					\
	((UP_MODE1_ASYNC_MODE << UP_MODE1_TP_SHIFT) |		\
	(UP_MODE1_ASYNC_MODE << UP_MODE1_RAW_CH_GAIN_SHIFT) |	\
	(UP_MODE1_ASYNC_MODE << UP_MODE1_LSC_SHIFT) |		\
	(UP_MODE1_ASYNC_MODE << UP_MODE1_RAW_AWB_GAIN_SHIFT) |	\
	(UP_MODE1_ASYNC_MODE << UP_MODE1_RAW_CURVE_SHIFT) |	\
	(UP_MODE1_ASYNC_MODE << UP_MODE1_PRE_RAW_SHIFT) |	\
	(UP_MODE1_ASYNC_MODE << UP_MODE1_POST_RAW_SHIFT) |	\
	(UP_MODE1_ASYNC_MODE << UP_MODE1_RAW_INT_SHIFT) |	\
	(UP_MODE1_ASYNC_MODE << UP_MODE1_AF_SHIFT) |		\
	(UP_MODE1_ASYNC_MODE << UP_MODE1_TRI_AUTO_SHIFT) |	\
	(UP_MODE1_ASYNC_MODE << UP_MODE1_CCM_SHIFT) |		\
	(UP_MODE1_ASYNC_MODE << UP_MODE1_CONT_SHIFT) |		\
	(UP_MODE1_ASYNC_MODE << UP_MODE1_YUV_PROC2_SHIFT) |	\
	(UP_MODE1_ASYNC_MODE << UP_MODE1_YUV_PROC1_SHIFT) |	\
	(UP_MODE1_ASYNC_MODE << UP_MODE1_WIN_SHIFT) |		\
	(UP_MODE1_ASYNC_MODE << UP_MODE1_DEBLANK_SHIFT))

#define UP_MODE1_ALL_SYNC_MODE					\
	((UP_MODE1_SYNC_MODE << UP_MODE1_TP_SHIFT) |		\
	(UP_MODE1_SYNC_MODE << UP_MODE1_RAW_CH_GAIN_SHIFT) |	\
	(UP_MODE1_SYNC_MODE << UP_MODE1_LSC_SHIFT) |		\
	(UP_MODE1_SYNC_MODE << UP_MODE1_RAW_AWB_GAIN_SHIFT) |	\
	(UP_MODE1_SYNC_MODE << UP_MODE1_RAW_CURVE_SHIFT) |	\
	(UP_MODE1_SYNC_MODE << UP_MODE1_PRE_RAW_SHIFT) |	\
	(UP_MODE1_SYNC_MODE << UP_MODE1_POST_RAW_SHIFT) |	\
	(UP_MODE1_SYNC_MODE << UP_MODE1_RAW_INT_SHIFT) |	\
	(UP_MODE1_SYNC_MODE << UP_MODE1_AF_SHIFT) |		\
	(UP_MODE1_SYNC_MODE << UP_MODE1_TRI_AUTO_SHIFT) |	\
	(UP_MODE1_SYNC_MODE << UP_MODE1_CCM_SHIFT) |		\
	(UP_MODE1_SYNC_MODE << UP_MODE1_CONT_SHIFT) |		\
	(UP_MODE1_SYNC_MODE << UP_MODE1_YUV_PROC2_SHIFT) |	\
	(UP_MODE1_SYNC_MODE << UP_MODE1_YUV_PROC1_SHIFT) |	\
	(UP_MODE1_SYNC_MODE << UP_MODE1_WIN_SHIFT) |		\
	(UP_MODE1_SYNC_MODE << UP_MODE1_DEBLANK_SHIFT))


/* UP_MODE2 */
#define UP_MODE2_TP_MASK			((uint32_t)0x1U)
#define UP_MODE2_RAW_CH_GAIN_MASK		((uint32_t)0x1U)
#define UP_MODE2_LSC_MASK			((uint32_t)0x1U)
#define UP_MODE2_RAW_AWB_GAIN_MASK		((uint32_t)0x1U)
#define UP_MODE2_RAW_CURVE_MASK			((uint32_t)0x1U)
#define UP_MODE2_PRE_RAW_MASK			((uint32_t)0x1U)
#define UP_MODE2_POST_RAW_MASK			((uint32_t)0x1U)
#define UP_MODE2_RAW_INT_MASK			((uint32_t)0x1U)
#define UP_MODE2_AF_MASK			((uint32_t)0x1U)
#define UP_MODE2_TRI_AUTO_MASK			((uint32_t)0x1U)
#define UP_MODE2_CCM_MASK			((uint32_t)0x1U)
#define UP_MODE2_CONT_MASK			((uint32_t)0x1U)
#define UP_MODE2_YUV_PROC2_MASK			((uint32_t)0x1U)
#define UP_MODE2_YUV_PROC1_MASK			((uint32_t)0x1U)
#define UP_MODE2_WIN_MASK			((uint32_t)0x1U)
#define UP_MODE2_DEBLANK_MASK			((uint32_t)0x1U)

#define UP_MODE2_TP_SHIFT			((uint32_t)16U)
#define UP_MODE2_RAW_CH_GAIN_SHIFT		((uint32_t)17U)
#define UP_MODE2_LSC_SHIFT			((uint32_t)18U)
#define UP_MODE2_RAW_AWB_GAIN_SHIFT		((uint32_t)19U)
#define UP_MODE2_RAW_CURVE_SHIFT		((uint32_t)20U)
#define UP_MODE2_PRE_RAW_SHIFT			((uint32_t)21U)
#define UP_MODE2_POST_RAW_SHIFT			((uint32_t)22U)
#define UP_MODE2_RAW_INT_SHIFT			((uint32_t)23U)
#define UP_MODE2_AF_SHIFT			((uint32_t)24U)
#define UP_MODE2_TRI_AUTO_SHIFT			((uint32_t)25U)
#define UP_MODE2_CCM_SHIFT			((uint32_t)26U)
#define UP_MODE2_CONT_SHIFT			((uint32_t)27U)
#define UP_MODE2_YUV_PROC2_SHIFT		((uint32_t)28U)
#define UP_MODE2_YUV_PROC1_SHIFT		((uint32_t)29U)
#define UP_MODE2_WIN_SHIFT			((uint32_t)30U)
#define UP_MODE2_DEBLANK_SHIFT			((uint32_t)31U)


#define UP_MODE2_INDIV_SYNC_MODE		((uint32_t)0U)
#define UP_MODE2_GROUP_SYNC_MODE		((uint32_t)1U)

#define UP_MODE2_ALL_INDIV_SYNC_MODE					\
	((UP_MODE2_INDIV_SYNC_MODE << UP_MODE2_TP_SHIFT) |		\
	(UP_MODE2_INDIV_SYNC_MODE << UP_MODE2_RAW_CH_GAIN_SHIFT) |	\
	(UP_MODE2_INDIV_SYNC_MODE << UP_MODE2_LSC_SHIFT) |		\
	(UP_MODE2_INDIV_SYNC_MODE << UP_MODE2_RAW_AWB_GAIN_SHIFT) |	\
	(UP_MODE2_INDIV_SYNC_MODE << UP_MODE2_RAW_CURVE_SHIFT) |	\
	(UP_MODE2_INDIV_SYNC_MODE << UP_MODE2_PRE_RAW_SHIFT) |		\
	(UP_MODE2_INDIV_SYNC_MODE << UP_MODE2_POST_RAW_SHIFT) |		\
	(UP_MODE2_INDIV_SYNC_MODE << UP_MODE2_RAW_INT_SHIFT) |		\
	(UP_MODE2_INDIV_SYNC_MODE << UP_MODE2_AF_SHIFT) |		\
	(UP_MODE2_INDIV_SYNC_MODE << UP_MODE2_TRI_AUTO_SHIFT) |		\
	(UP_MODE2_INDIV_SYNC_MODE << UP_MODE2_CCM_SHIFT) |		\
	(UP_MODE2_INDIV_SYNC_MODE << UP_MODE2_CONT_SHIFT) |		\
	(UP_MODE2_INDIV_SYNC_MODE << UP_MODE2_YUV_PROC2_SHIFT) |	\
	(UP_MODE2_INDIV_SYNC_MODE << UP_MODE2_YUV_PROC1_SHIFT) |	\
	(UP_MODE2_INDIV_SYNC_MODE << UP_MODE2_WIN_SHIFT) |		\
	(UP_MODE2_INDIV_SYNC_MODE << UP_MODE2_DEBLANK_SHIFT))

#define UP_MODE2_ALL_GROUP_SYNC_MODE					\
	((UP_MODE2_GROUP_SYNC_MODE << UP_MODE2_TP_SHIFT) |		\
	(UP_MODE2_GROUP_SYNC_MODE << UP_MODE2_RAW_CH_GAIN_SHIFT) |	\
	(UP_MODE2_GROUP_SYNC_MODE << UP_MODE2_LSC_SHIFT) |		\
	(UP_MODE2_GROUP_SYNC_MODE << UP_MODE2_RAW_AWB_GAIN_SHIFT) |	\
	(UP_MODE2_GROUP_SYNC_MODE << UP_MODE2_RAW_CURVE_SHIFT) |	\
	(UP_MODE2_GROUP_SYNC_MODE << UP_MODE2_PRE_RAW_SHIFT) |		\
	(UP_MODE2_GROUP_SYNC_MODE << UP_MODE2_POST_RAW_SHIFT) |		\
	(UP_MODE2_GROUP_SYNC_MODE << UP_MODE2_RAW_INT_SHIFT) |		\
	(UP_MODE2_GROUP_SYNC_MODE << UP_MODE2_AF_SHIFT) |		\
	(UP_MODE2_GROUP_SYNC_MODE << UP_MODE2_TRI_AUTO_SHIFT) |		\
	(UP_MODE2_GROUP_SYNC_MODE << UP_MODE2_CCM_SHIFT) |		\
	(UP_MODE2_GROUP_SYNC_MODE << UP_MODE2_CONT_SHIFT) |		\
	(UP_MODE2_GROUP_SYNC_MODE << UP_MODE2_YUV_PROC2_SHIFT) |	\
	(UP_MODE2_GROUP_SYNC_MODE << UP_MODE2_YUV_PROC1_SHIFT) |	\
	(UP_MODE2_GROUP_SYNC_MODE << UP_MODE2_WIN_SHIFT) |		\
	(UP_MODE2_GROUP_SYNC_MODE << UP_MODE2_DEBLANK_SHIFT))


/* USR_CNT1 */
#define USR_CNT1_MASK				((uint32_t)0xFFFFU)

#define USR_CNT1_SHIFT				((uint32_t)16U)


/* USR_CNT2 */
#define USR_CNT2_MASK				((uint32_t)0xFFFFU)

#define USR_CNT2_SHIFT				((uint32_t)16U)



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
#define IMG_WIN_CTL_DEBLANK_VS_BYP_EN_MASK	((uint32_t)0x1U)
#define IMG_WIN_CTL_DEBLANK_EN_MASK		((uint32_t)0x1U)

#define IMG_WIN_CTL_WIN_EN_SHIFT		((uint32_t)16U)
#define IMG_WIN_CTL_IMG_OUT_ORDER_SEL_SHIFT	((uint32_t)17U)
#define IMG_WIN_CTL_IMG_SCALE_SHIFT		((uint32_t)20U)
#define IMG_WIN_CTL_VSYNC_POL_SEL_SHIFT		((uint32_t)22U)
#define IMG_WIN_CTL_DEBLANK_VS_BYP_EN_SHIFT	((uint32_t)29U)
#define IMG_WIN_CTL_DEBLANK_EN_SHIFT		((uint32_t)30U)


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
		((uint32_t)1U)
#define IMG_WIN_CTL_VSYNC_POL_SEL_LOW_VERTICAL_BLANK	\
		((uint32_t)1U)

#define IMG_WIN_CTL_DEBLANK_VS_BYP_DISABLE		\
		((uint32_t)0U)
#define IMG_WIN_CTL_DEBLANK_VS_BYP_ENABLE		\
		((uint32_t)1U)

#define IMG_WIN_CTL_DEBLANK_EN_DISABLE		((uint32_t)0U)
#define IMG_WIN_CTL_DEBLANK_EN_ENABLE		((uint32_t)1U)


/* IMG_WIN_X_START */
#define IMG_WIN_X_START_MASK			((uint32_t)0xFFFFU)

#define IMG_WIN_X_START_SHIFT			((uint32_t)16U)


/* IMG_WIN_Y_START */
#define IMG_WIN_Y_START_MASK			((uint32_t)0xFFFFU)

#define IMG_WIN_Y_START_SHIFT			((uint32_t)16U)


/* IMG_WIN_WIDTH */
#define IMG_WIN_WIDTH_MASK			((uint32_t)0xFFFFU)

#define IMG_WIN_WIDTH_SHIFT			((uint32_t)16U)


/* IMG_WIN_HEIGHT */
#define IMG_WIN_HEIGHT_MASK			((uint32_t)0xFFFFU)

#define IMG_WIN_HEIGHT_SHIFT			((uint32_t)16U)


/* IMG_WIN_FORMAT */
#define IMG_WIN_FORMAT_FORMAT_MASK		((uint32_t)0x3U)
#define IMG_WIN_FORMAT_DATA_ORDER_MASK		((uint32_t)0x3U)

#define IMG_WIN_FORMAT_FORMAT_SHIFT		((uint32_t)16U)
#define IMG_WIN_FORMAT_DATA_ORDER_SHIFT		((uint32_t)20U)


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

#define IMG_IN_ORDER_CTL_ORDER_SHIFT		((uint32_t)16U)


#define IMG_IN_ORDER_CTL_ORDER_B_FIRST		((uint32_t)0U)
#define IMG_IN_ORDER_CTL_ORDER_GB_FIRST		((uint32_t)1U)
#define IMG_IN_ORDER_CTL_ORDER_GR_FIRST		((uint32_t)2U)
#define IMG_IN_ORDER_CTL_ORDER_R_FIRST		((uint32_t)3U)


/******************************************************************
 * 3A Control Register Define (1page 0x200~2FF)
 ******************************************************************/
#define REG_ISP_TRI_AUTO_CTL			((uint32_t)0x1200U)
#define REG_ISP_TRI_AUTO_CFG			((uint32_t)0x1204U)


/******************************************************************
 * Bayer Channel Gain (1page 0x300~0x330)
 ******************************************************************/
#define REG_ISP_RAW_CH_CTL			((uint32_t)0x1300U)
#define REG_ISP_RAW_CH_CFG1			((uint32_t)0x1304U)
#define REG_ISP_RAW_CH_CFG2			((uint32_t)0x1308U)
#define REG_ISP_RAW_CH_CFG3			((uint32_t)0x130CU)
#define REG_ISP_RAW_CH_CFG4			((uint32_t)0x1310U)

/* RAW_CH_CTL */
#define RAW_CH_CTL_BLUE_CH_OFS_MASK		((uint32_t)0x3FFU)
#define RAW_CH_CTL_RAW_CH_GAIN_EN_MASK		((uint32_t)0x1U)
#define RAW_CH_CTL_RAW_CH_OFS_EN_MASK		((uint32_t)0x1U)
#define RAW_CH_CTL_BLUE_SIGN_MASK		((uint32_t)0x1U)
#define RAW_CH_CTL_GB_SIGN_MASK			((uint32_t)0x1U)
#define RAW_CH_CTL_GR_SIGN_MASK			((uint32_t)0x1U)
#define RAW_CH_CTL_RED_SIGN_MASK		((uint32_t)0x1U)

#define RAW_CH_CTL_BLUE_CH_OFS_SHIFT		((uint32_t)0U)
#define RAW_CH_CTL_RAW_CH_GAIN_EN_SHIFT		((uint32_t)16U)
#define RAW_CH_CTL_RAW_CH_OFS_EN_SHIFT		((uint32_t)17U)
#define RAW_CH_CTL_BLUE_SIGN_SHIFT		((uint32_t)20U)
#define RAW_CH_CTL_GB_SIGN_SHIFT		((uint32_t)21U)
#define RAW_CH_CTL_GR_SIGN_SHIFT		((uint32_t)22U)
#define RAW_CH_CTL_RED_SIGN_SHIFT		((uint32_t)23U)


/******************************************************************
 * Bayer AWB Gain (1page 0x400~0x40A)
 ******************************************************************/
#define REG_ISP_RAW_AWB_GCTL			((uint32_t)0x1400U)
#define REG_ISP_RAW_AWB_GCFG			((uint32_t)0x1404U)


/******************************************************************
 * RGB AE Gain (1page 0x500~0x504)
 ******************************************************************/
#define REG_ISP_RGB_AE_GCTL			((uint32_t)0x1500U)


/******************************************************************
 * Bayer Curve Gain (1page 0x800~0x850)
 ******************************************************************/
#define REG_RAW_CUR__GCTL			((uint32_t)0x1800U)
#define REG_RAW_CUR_GCFG1			((uint32_t)0x1804U)
#define REG_RAW_CUR_GCFG2			((uint32_t)0x1808U)
#define REG_RAW_CUR_GCFG3			((uint32_t)0x180CU)
#define REG_RAW_CUR_GCFG4			((uint32_t)0x1810U)
#define REG_RAW_CUR_GCFG5			((uint32_t)0x1814U)
#define REG_RAW_CUR_GCFG6			((uint32_t)0x1818U)
#define REG_RAW_CUR_GCFG7			((uint32_t)0x181CU)
#define REG_RAW_CUR_GCFG8			((uint32_t)0x1820U)
#define REG_RAW_CUR_GCFG9			((uint32_t)0x1824U)
#define REG_RAW_CUR_GCFG10			((uint32_t)0x1828U)


/******************************************************************
 * De-Companding (1page 0x860~0x880)
 ******************************************************************/
#define REG_ISP_DCPD_CTL			((uint32_t)0x1860U)
#define REG_ISP_DCPD_CUR_GCFG1			((uint32_t)0x1864U)
#define REG_ISP_DCPD_CUR_GCFG2			((uint32_t)0x1868U)
#define REG_ISP_DCPD_CUR_GCFG3			((uint32_t)0x186CU)
#define REG_ISP_DCPD_CUR_GCFG4			((uint32_t)0x1870U)
#define REG_ISP_DCPD_CUR_XCFG1			((uint32_t)0x1874U)
#define REG_ISP_DCPD_CUR_XCFG2			((uint32_t)0x1878U)
#define REG_ISP_DCPD_CUR_XCFG3			((uint32_t)0x187CU)
#define REG_ISP_DCPD_CUR_XCFG4			((uint32_t)0x1880U)


/* DCPD_CTL */
#define DCPD_CTL_DCPD_CUR_GAIN0_MASK		((uint32_t)0x3FFU)
#define DCPD_CTL_DCPD_EN_MASK			((uint32_t)0x1U)
#define DCPD_CTL_IN_BIT_SEL_MASK		((uint32_t)0x7U)
#define DCPD_CTL_OUT_BIT_SEL_MASK		((uint32_t)0x7U)
#define DCPD_CTL_DCPD_CUR_MAXVAL_MASK		((uint32_t)0x7U)

#define DCPD_CTL_DCPD_CUR_GAIN0_SHIFT		((uint32_t)0U)
#define DCPD_CTL_DCPD_EN_SHIFT			((uint32_t)16U)
#define DCPD_CTL_IN_BIT_SEL_SHIFT		((uint32_t)17U)
#define DCPD_CTL_OUT_BIT_SEL_SHIFT		((uint32_t)20U)
#define DCPD_CTL_DCPD_CUR_MAXVAL_SHIFT		((uint32_t)24U)

#define DCPD_CTL_DCPD_EN_DISABLE		((uint32_t)0U)
#define DCPD_CTL_DCPD_EN_ENABLE			((uint32_t)1U)

#define DCPD_CTL_IN_BIT_SEL_10BITS		((uint32_t)0U)
#define DCPD_CTL_IN_BIT_SEL_12BITS		((uint32_t)1U)
#define DCPD_CTL_IN_BIT_SEL_14BITS		((uint32_t)2U)

#define DCPD_CTL_OUT_BIT_SEL_10BITS		((uint32_t)0U)
#define DCPD_CTL_OUT_BIT_SEL_12BITS		((uint32_t)1U)
#define DCPD_CTL_OUT_BIT_SEL_14BITS		((uint32_t)2U)
#define DCPD_CTL_OUT_BIT_SEL_15BITS		((uint32_t)3U)
#define DCPD_CTL_OUT_BIT_SEL_16BITS		((uint32_t)4U)
#define DCPD_CTL_OUT_BIT_SEL_17BITS		((uint32_t)5U)
#define DCPD_CTL_OUT_BIT_SEL_20BITS		((uint32_t)6U)

/* DCPD_CUR_GCF1 */
#define DCPD_CUR_GCFG1_GAIN2_MASK		((uint32_t)0x3FFU)
#define DCPD_CUR_GCFG1_GAIN1_MASK		((uint32_t)0x3FFU)

#define DCPD_CUR_GCFG1_GAIN2_SHIFT		((uint32_t)0U)
#define DCPD_CUR_GCFG1_GAIN1_SHIFT		((uint32_t)16U)

/* DCPD_CUR_GCF2 */
#define DCPD_CUR_GCFG2_GAIN4_MASK		((uint32_t)0x3FFU)
#define DCPD_CUR_GCFG2_GAIN3_MASK		((uint32_t)0x3FFU)

#define DCPD_CUR_GCFG2_GAIN4_SHIFT		((uint32_t)0U)
#define DCPD_CUR_GCFG2_GAIN3_SHIFT		((uint32_t)16U)

/* DCPD_CUR_GCF3 */
#define DCPD_CUR_GCFG3_GAIN6_MASK		((uint32_t)0x3FFU)
#define DCPD_CUR_GCFG3_GAIN5_MASK		((uint32_t)0x3FFU)

#define DCPD_CUR_GCFG3_GAIN6_SHIFT		((uint32_t)0U)
#define DCPD_CUR_GCFG3_GAIN5_SHIFT		((uint32_t)16U)

/* DCPD_CUR_GCF4 */
#define DCPD_CUR_GCFG4_X_AXIS0_MASK		((uint32_t)0x3FFU)
#define DCPD_CUR_GCFG4_GAIN7_MASK		((uint32_t)0x3FFU)

#define DCPD_CUR_GCFG4_X_AXIS0_SHIFT		((uint32_t)0U)
#define DCPD_CUR_GCFG4_GAIN7_SHIFT		((uint32_t)16U)

/* DCPD_CUR_XCFG1 */
#define DCPD_CUR_XCFG1_X_AXIS2_MASK		((uint32_t)0x3FFU)
#define DCPD_CUR_XCFG1_X_AXIS1_MASK		((uint32_t)0x3FFU)

#define DCPD_CUR_XCFG1_X_AXIS2_SHIFT		((uint32_t)0U)
#define DCPD_CUR_XCFG1_X_AXIS1_SHIFT		((uint32_t)16U)

/* DCPD_CUR_XCFG2 */
#define DCPD_CUR_XCFG2_X_AXIS4_MASK		((uint32_t)0x3FFU)
#define DCPD_CUR_XCFG2_X_AXIS3_MASK		((uint32_t)0x3FFU)

#define DCPD_CUR_XCFG2_X_AXIS4_SHIFT		((uint32_t)0U)
#define DCPD_CUR_XCFG2_X_AXIS3_SHIFT		((uint32_t)16U)

/* DCPD_CUR_XCFG3 */
#define DCPD_CUR_XCFG3_X_AXIS6_MASK		((uint32_t)0x3FFU)
#define DCPD_CUR_XCFG3_X_AXIS5_MASK		((uint32_t)0x3FFU)

#define DCPD_CUR_XCFG3_X_AXIS6_SHIFT		((uint32_t)0U)
#define DCPD_CUR_XCFG3_X_AXIS5_SHIFT		((uint32_t)16U)

/* DCPD_CUR_XCFG4 */
#define DCPD_CUR_XCFG4_X_AXIS7_MASK		((uint32_t)0x3FFU)

#define DCPD_CUR_XCFG4_X_AXIS7_SHIFT		((uint32_t)16U)


/******************************************************************
 * Shading Correction Register Define (1page 0x900~0x918)
 ******************************************************************/
#define REG_ISP_LSC_CTL				((uint32_t)0x1900U)
#define REG_ISP_LSC_CFG				((uint32_t)0x1904U)
#define REG_ISP_LSC_GCFG			((uint32_t)0x1908U)


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
#define REG_ISP_PRRAW_SH_GCTL1			((uint32_t)0x1A54U)
#define REG_ISP_PRRAW_SH_GCTL2			((uint32_t)0x1A58U)
#define REG_ISP_PRRAW_SH_CCTL			((uint32_t)0x1A5CU)


/******************************************************************
 * Post Bayer NR(Noise Reduction) (1page 0xA80~0xA8E)
 ******************************************************************/
#define REG_ISP_PORAW_NR_CTL			((uint32_t)0x1A80U)
#define REG_ISP_PORAW_NR_GCTL1			((uint32_t)0x1A84U)
#define REG_ISP_PORAW_NR_GCTL2			((uint32_t)0x1A88U)
#define REG_ISP_PORAW_NR_GCTL3			((uint32_t)0x1A8CU)


/******************************************************************
 * Post Bayer Sharpness (1page 0xAB0~0xABA)
 ******************************************************************/
#define REG_ISP_PORAW_SH_CTL			((uint32_t)0x1AB0U)
#define REG_ISP_PORAW_SH_GCTL1			((uint32_t)0x1AB4U)
#define REG_ISP_PORAW_SH_GCTL2			((uint32_t)0x1AB8U)


/******************************************************************
 * RGBInt (2page 0x000~0x060)
 ******************************************************************/
#define REG_ISP_RGB_INT_CTL			((uint32_t)0x2000U)
#define REG_ISP_RGB_INT_CFG1			((uint32_t)0x2004U)
#define REG_ISP_RGB_INT_CFG2			((uint32_t)0x2008U)
#define REG_ISP_RGB_INT_GCTL1			((uint32_t)0x200CU)
#define REG_ISP_RGB_INT_GCTL2			((uint32_t)0x2010U)
#define REG_ISP_RGB_INT_GCTL3			((uint32_t)0x2014U)


/******************************************************************
 * RGB Sharpness (2page 0x040~0x068)
 ******************************************************************/
#define REG_ISP_RGB_SH_CTL			((uint32_t)0x2040U)
#define REG_ISP_RGB_SH_CFG			((uint32_t)0x2044U)
#define REG_ISP_RGB_SH_GCTL1			((uint32_t)0x2048U)
#define REG_ISP_RGB_SH_GCTL2			((uint32_t)0x204CU)
#define REG_ISP_RGB_SH_GCTL3			((uint32_t)0x2050U)
#define REG_ISP_RGB_SH_GCTL4			((uint32_t)0x2054U)


/******************************************************************
 * Y NR(Noise Reduction) Define (2page 0x070~0x0A8)
 ******************************************************************/
#define REG_ISP_YF_CTL				((uint32_t)0x2070U)
#define REG_ISP_YF_CFG1				((uint32_t)0x2074U)
#define REG_ISP_YF_CFG2				((uint32_t)0x2078U)
#define REG_ISP_YF_CFG3				((uint32_t)0x207CU)
#define REG_ISP_YF_CFG4				((uint32_t)0x2080U)
#define REG_ISP_YF_CFG5				((uint32_t)0x2084U)


/******************************************************************
 * Y Contrast (2page 0x0A8~0x0B0)
 ******************************************************************/
#define REG_ISP_YC_CTL				((uint32_t)0x20A8U)
#define REG_ISP_YC_CFG				((uint32_t)0x20ACU)


/******************************************************************
 * CNR(Chroma Noise Reduction) (2page 0x0B8~0x108)
 ******************************************************************/
#define REG_ISP_CF_CTL				((uint32_t)0x20B8U)
#define REG_ISP_CF_CFG1				((uint32_t)0x20BCU)
#define REG_ISP_CF_CFG2				((uint32_t)0x20C0U)
#define REG_ISP_CF_CFG3				((uint32_t)0x20C4U)
#define REG_ISP_CF_CFG4				((uint32_t)0x20C8U)


/******************************************************************
 * Y Sharpness (2page 0x0E0~0x108)
 ******************************************************************/
#define REG_ISP_Y_SH_CTL			((uint32_t)0x20E0U)
#define REG_ISP_Y_SH_CFG1			((uint32_t)0x20E4U)
#define REG_ISP_Y_SH_CFG2			((uint32_t)0x20E8U)
#define REG_ISP_Y_SH_CFG3			((uint32_t)0x20ECU)
#define REG_ISP_Y_SH_CFG4			((uint32_t)0x20F0U)
#define REG_ISP_Y_SH_CFG5			((uint32_t)0x20F4U)


/******************************************************************
 * RGB Color Correction Matrix (2page 0x500~0x530)
 ******************************************************************/
#define REG_ISP_RGB_CCM_CTL			((uint32_t)0x2500U)
#define REG_ISP_RGB_CCM_CFG1			((uint32_t)0x2504U)
#define REG_ISP_RGB_CCM_CFG2			((uint32_t)0x2508U)
#define REG_ISP_RGB_CCM_CFG3			((uint32_t)0x250CU)
#define REG_ISP_RGB_CCM_CFG4			((uint32_t)0x2510U)


/******************************************************************
 * YUV Saturation Control (2page 0x600~0x61C)
 ******************************************************************/
#define REG_ISP_YUV_SA_CTL			((uint32_t)0x2600U)
#define REG_ISP_YUV_SA_GCTL1			((uint32_t)0x2604U)
#define REG_ISP_YUV_SA_GCTL2			((uint32_t)0x2608U)
#define REG_ISP_YUV_SA_GCTL3			((uint32_t)0x260CU)


/******************************************************************
 * RGB Multi-Color Enhancement (2page 0x700~0x754)
 ******************************************************************/
#define REG_ISP_RGB_CE_CTL			((uint32_t)0x2700U)
#define REG_ISP_RGB_CE_CFG1			((uint32_t)0x2704U)
#define REG_ISP_RGB_CE_CFG2			((uint32_t)0x2708U)
#define REG_ISP_RGB_CE_CFG3			((uint32_t)0x270CU)
#define REG_ISP_RGB_CE_CFG4			((uint32_t)0x2710U)
#define REG_ISP_RGB_CE_CFG5			((uint32_t)0x2714U)
#define REG_ISP_RGB_CE_CFG6			((uint32_t)0x2718U)
#define REG_ISP_RGB_CE_CFG7			((uint32_t)0x271CU)
#define REG_ISP_RGB_CE_CFG8			((uint32_t)0x2720U)
#define REG_ISP_RGB_CE_CFG9			((uint32_t)0x2724U)


/******************************************************************
 * HDR (2page 0x810~0x8FF)
 ******************************************************************/
#define REG_ISP_HDR_CTL				((uint32_t)0x2810U)
#define REG_ISP_HDR_CGAIN1			((uint32_t)0x2814U)
#define REG_ISP_HDR_CGAIN2			((uint32_t)0x2818U)
#define REG_ISP_HDR_CGAIN3			((uint32_t)0x281CU)
#define REG_ISP_HDR_CGAIN4			((uint32_t)0x2820U)
#define REG_ISP_HDR_CGAIN5			((uint32_t)0x2824U)
#define REG_ISP_HDR_CGAIN6			((uint32_t)0x2828U)
#define REG_ISP_HDR_WGAIN1			((uint32_t)0x282CU)
#define REG_ISP_HDR_WGAIN2			((uint32_t)0x2830U)
#define REG_ISP_HDR_WGAIN3			((uint32_t)0x2834U)
#define REG_ISP_HDR_WGAIN4			((uint32_t)0x2838U)
#define REG_ISP_HDR_WGAIN5			((uint32_t)0x283CU)
#define REG_ISP_HDR_GM_CGAIN1			((uint32_t)0x2840U)
#define REG_ISP_HDR_GM_CGAIN2			((uint32_t)0x2844U)
#define REG_ISP_HDR_GM_WGAIN1			((uint32_t)0x2848U)
#define REG_ISP_HDR_GM_WGAIN2			((uint32_t)0x284CU)
#define REG_ISP_HDR_GM_WGAIN3			((uint32_t)0x2850U)
#define REG_ISP_HDR_LUM_CGCTL1			((uint32_t)0x2854U)
#define REG_ISP_HDR_LUM_CGCTL2			((uint32_t)0x2858U)
#define REG_ISP_HDR_LUM_CGCTL3			((uint32_t)0x285CU)
#define REG_ISP_HDR_LUM_WGCTL1			((uint32_t)0x2860U)
#define REG_ISP_HDR_LUM_WGCTL2			((uint32_t)0x2864U)
#define REG_ISP_HDR_LUM_WGCTL3			((uint32_t)0x2868U)
#define REG_ISP_HDR_LUM_WGCTL4			((uint32_t)0x286CU)
#define REG_ISP_HDR_LUM_WGCTL5			((uint32_t)0x2870U)
#define REG_ISP_HDR_CFG1			((uint32_t)0x2874U)
#define REG_ISP_HDR_CFG2			((uint32_t)0x2878U)
#define REG_ISP_HDR_CFG3			((uint32_t)0x287CU)
#define REG_ISP_HDR_CFG4			((uint32_t)0x2880U)

#define HDR_MODE_INTERLEAVED			((uint32_t)0U)
#define HDR_MODE_COMPANDING			((uint32_t)1U)
#define HDR_MODE_NONE				((uint32_t)2U)


/******************************************************************
 * WDR (2page 0x900~0x96C)
 ******************************************************************/
#define REG_ISP_WDR_CTL				((uint32_t)0x2900U)
#define REG_ISP_WDR_GAIN1			((uint32_t)0x2904U)
#define REG_ISP_WDR_GAIN2			((uint32_t)0x2908U)
#define REG_ISP_WDR_GAIN3			((uint32_t)0x290CU)
#define REG_ISP_WDR_GAIN4			((uint32_t)0x2910U)
#define REG_ISP_WDR_GAIN5			((uint32_t)0x2914U)
#define REG_ISP_WDR_GAIN6			((uint32_t)0x2918U)
#define REG_ISP_WDR_GAIN7			((uint32_t)0x291CU)
#define REG_ISP_WDR_CFG1			((uint32_t)0x2920U)
#define REG_ISP_WDR_CFG2			((uint32_t)0x2924U)
#define REG_ISP_WDR_CUR_GAIN1			((uint32_t)0x2928U)
#define REG_ISP_WDR_CUR_GAIN2			((uint32_t)0x292CU)
#define REG_ISP_WDR_CUR_GAIN3			((uint32_t)0x2930U)
#define REG_ISP_WDR_CUR_GAIN4			((uint32_t)0x2934U)


/******************************************************************
 * I2C Master Control (2page 0xB00~0xB14)
 ******************************************************************/
#define REG_ISP_I2C_MST_CTL			((uint32_t)0x2B00U)
#define REG_ISP_I2C_MST_TEN			((uint32_t)0x2B04U)
#define REG_ISP_I2C_MST_SPEED			((uint32_t)0x2B08U)
#define REG_ISP_I2C_MST_SUB_ADDR		((uint32_t)0x2B0CU)
#define REG_ISP_I2C_MST_WDATA			((uint32_t)0x2B10U)
#define REG_ISP_I2C_MST_RDATA			((uint32_t)0x2B14U)


/******************************************************************
 * MCU Configuration (0x2B80~0x2C00)
 ******************************************************************/
#define REG_ISP_MCU_CTL				((uint32_t)0x2B80U)
#define REG_ISP_MCU_MEM_CTL			((uint32_t)0x2BE0U)

/* MCU_CTL */
#define MCU_CTL_MCU_EN_MASK			((uint32_t)0x1U)
#define MCU_CTL_MCU_CLK_DIV_MASK		((uint32_t)0x3U)
#define MCU_CTL_MCU_DBG_EN_MASK			((uint32_t)0x1U)

#define MCU_CTL_MCU_EN_SHIFT			((uint32_t)0U)
#define MCU_CTL_MCU_CLK_DIV_SHIFT		((uint32_t)2U)
#define MCU_CTL_MCU_DBG_EN_SHIFT		((uint32_t)4U)

#define MCU_CTL_MCU_EN_DISABLE			((uint32_t)0U)
#define MCU_CTL_MCU_EN_ENABLE			((uint32_t)1U)

#define MCU_CTL_MCU_CLK_DIV_1_2			((uint32_t)0U)
#define MCU_CTL_MCU_CLK_DIV_1_4			((uint32_t)1U)
#define MCU_CTL_MCU_CLK_DIV_1_8			((uint32_t)2U)
#define MCU_CTL_MCU_CLK_DIV_1_16		((uint32_t)3U)

/* MCU_MEM_CTL */
#define MCU_MEM_CTL_MCU_MEM_DL_EN_MASK		((uint32_t)0x1U)
#define MCU_MEM_CTL_MCU_HOLD_EN_MASK		((uint32_t)0x1U)

#define MCU_MEM_CTL_MCU_MEM_DL_EN_SHIFT		((uint32_t)0U)
#define MCU_MEM_CTL_MCU_HOLD_EN_SHIFT		((uint32_t)1U)

#define MCU_MEM_CTL_MCU_MEM_DL_EN_DISABLE	((uint32_t)0U)
#define MCU_MEM_CTL_MCU_MEM_DL_EN_ENABLE	((uint32_t)1U)

#define MCU_MEM_CTL_MCU_HOLD_EN_DISABLE		((uint32_t)0U)
#define MCU_MEM_CTL_MCU_HOLD_EN_ENABLE		((uint32_t)1U)

#define ISP_MEM_OFFSET_SFR			((uint32_t)0x0U)
#define ISP_MEM_OFFSET_DATA			((uint32_t)0x200U)
#define ISP_MEM_OFFSET_CODE			((uint32_t)0x3800U)
#define ISP_MEM_SIZE_DATA			\
		(((uint32_t)(14U) * (uint32_t)(1024U)) - (uint32_t)(512U))
#define ISP_MEM_SIZE_CODE			\
		((uint32_t)(50U) * (uint32_t)(1024U))


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

/* ATI_I2C_SLV_CTL */
#define ATI_I2C_SLV_CTL_ID_MASK			((uint32_t)0x7FU)
#define ATI_I2C_SLV_CTL_MODE_MASK		((uint32_t)0x3U)

#define ATI_I2C_SLV_CTL_ID_SHIFT		((uint32_t)0U)
#define ATI_I2C_SLV_CTL_MODE_SHIFT		((uint32_t)8U)

/* set I2C data length(address / data bit) */
#define ATI_I2C_SLV_CTL_MODE_16_16		((uint32_t)3U)
#define ATI_I2C_SLV_CTL_MODE_16_8		((uint32_t)2U)
#define ATI_I2C_SLV_CTL_MODE_8_16		((uint32_t)1U)
#define ATI_I2C_SLV_CTL_MODE_8_8		((uint32_t)0U)

/******************************************************************
 * DMA Configuration (4page 0x000~0x01C)
 ******************************************************************/
#define REG_ISP_WDMA_CTL0			((uint32_t)0x4000U)

/* WDMA_CTL0 */
#define WDMA_CTL0_WDMA_ENABLE_MASK		((uint32_t)0x1U)

#define WDMA_CTL0_WDMA_ENABLE_SHIFT		((uint32_t)16U)

#define WDMA_CTL0_WDMA_DISABLE			((uint32_t)0U)
#define WDMA_CTL0_WDMA_ENABLE			((uint32_t)1U)


/******************************************************************
 * H/W Statistics
 ******************************************************************/
#define REG_HW_STATISTICS			((uint32_t)0x8000U)
#define REG_HW_STATISTICS_R_SUM			((uint32_t)0x8000U)
#define REG_HW_STATISTICS_G_SUM			((uint32_t)0x8400U)
#define REG_HW_STATISTICS_B_SUM			((uint32_t)0x8800U)
#define REG_HW_STATISTICS_Y_AVERAGE		((uint32_t)0x8C00U)
#define REG_HW_STATISTICS_RG_RATIO		((uint32_t)0x9000U)
#define REG_HW_STATISTICS_BG_RATIO		((uint32_t)0x9400U)

#define REG_ANN_FLAG				((uint32_t)0x2C1CU)

#define ANN_FLAG_WDR_SHIFT			((uint32_t)0U)
#define ANN_FLAG_AWB_SHIFT			((uint32_t)8U)

#define ANN_FLAG_WDR_MASK			((uint32_t)0x3U)
#define ANN_FLAG_AWB_MASK			((uint32_t)0x7U)

#define REG_ANN_AWB_APPLY			((uint32_t)0x250CU)

#define ANN_AWB_APPLY_SHIFT			((uint32_t)0U)

#define ANN_AWB_APPLY_MASK			((uint32_t)0x1U)

#define TCC_ISP_CORE_CMD_ID_ISP_CORE		((uint32_t)0x82U)
#define TCC_ISP_CORE_CMD_ID_SCENE_DATA		((uint32_t)0x86U)

#endif
