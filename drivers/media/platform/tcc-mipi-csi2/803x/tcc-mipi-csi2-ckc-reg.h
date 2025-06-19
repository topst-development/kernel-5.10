/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_MIPI_CKC_REG_H
#define TCC_MIPI_CKC_REG_H

/*
 * MIPI CKC REG BASE
 */
#define CLKCTRL0	((uint32_t)0x00U)
#define MIPI_BUS_CLK	((uint32_t)0x00U)
#define CLKCTRL1	((uint32_t)0x04U)
#define MIPI_PIXEL_CLK	((uint32_t)0x04U)
#define PLLPMS		((uint32_t)0x08U)
#define PLLCON		((uint32_t)0x0CU)
#define PLLMON		((uint32_t)0x10U)
#define CLKDIVC		((uint32_t)0x14U)

/*
 * CLKCTRL
 */
#define CLKCTRL_CHGRQ_SHIFT	((uint32_t)31U)
#define CLKCTRL_SEL_SHIFT	((uint32_t)0U)

#define CLKCTRL_CHGRQ_MASK	(((uint32_t)0x1U) << CLKCTRL_CHGRQ_SHIFT)
#define CLKCTRL_SEL_MASK	(((uint32_t)0x3U) << CLKCTRL_SEL_SHIFT)

#define CLKCTRL_SEL_XIN		((uint32_t)0U)
#define CLKCTRL_SEL_PLL_DIRECT	((uint32_t)1U)
#define CLKCTRL_SEL_PLL_DIVIDER	((uint32_t)2U)
#define CLKCTRL_SEL_RESERVED	((uint32_t)3U)

/*
 * PLLPMS
 */
#define PLLPMS_RESETB_SHIFT	((uint32_t)31U)
#define PLLPMS_RSEL_SHIFT	((uint32_t)27U)
#define PLLPMS_LOCK_EN_SHIFT	((uint32_t)26U)
#define PLLPMS_ICP_SHIFT	((uint32_t)24U)
#define PLLPMS_LOCK_SHIFT	((uint32_t)23U)
#define PLLPMS_BYPASS_SHIFT	((uint32_t)21U)
#define PLLPMS_S_SHIFT		((uint32_t)16U)
#define PLLPMS_M_SHIFT		((uint32_t)6U)
#define PLLPMS_P_SHIFT		((uint32_t)0U)

#define PLLPMS_RESETB_MASK	(((uint32_t)0x1U) << PLLPMS_RESETB_SHIFT)
#define PLLPMS_RSEL_MASK	(((uint32_t)0xFU) << PLLPMS_RSEL_SHIFT)
#define PLLPMS_LOCK_EN_MASK	(((uint32_t)0x1U) << PLLPMS_LOCK_EN_SHIFT)
#define PLLPMS_ICP_MASK	        (((uint32_t)0x3U) << PLLPMS_ICP_SHIFT)
#define PLLPMS_LOCK_MASK	(((uint32_t)0x1U) << PLLPMS_LOCK_SHIFT)
#define PLLPMS_BYPASS_MASK	(((uint32_t)0x1U) << PLLPMS_BYPASS_SHIFT)
#define PLLPMS_S_MASK		(((uint32_t)0x7U) << PLLPMS_S_SHIFT)
#define PLLPMS_M_MASK		(((uint32_t)0x3FFU) << PLLPMS_M_SHIFT)
#define PLLPMS_P_MASK		(((uint32_t)0x3FU) << PLLPMS_P_SHIFT)

/*
 * PLLCON
 */
#define PLLCON_LOCK_CON_REV_SHIFT	((uint32_t)12U)
#define PLLCON_LOCK_CON_DLY_SHIFT	((uint32_t)10U)
#define PLLCON_LOCK_CON_OUT_SHIFT	((uint32_t)8U)
#define PLLCON_LOCK_CON_IN_SHIFT	((uint32_t)6U)
#define PLLCON_EXTAFC_SHIFT		((uint32_t)1U)
#define PLLCON_AFC_ENB_SHIFT		((uint32_t)0U)

#define PLLCON_LOCK_CON_REV_MASK	\
		(((uint32_t)0x3U) << PLLCON_LOCK_CON_REV_SHIFT)
#define PLLCON_LOCK_CON_DLY_MASK	\
		(((uint32_t)0x3U) << PLLCON_LOCK_CON_DLY_SHIFT)
#define PLLCON_LOCK_CON_OUT_MASK	\
		(((uint32_t)0x3U) << PLLCON_LOCK_CON_OUT_SHIFT)
#define PLLCON_LOCK_CON_IN_MASK		\
		(((uint32_t)0x3U) << PLLCON_LOCK_CON_IN_SHIFT)
#define PLLCON_EXTAFC_MASK		\
		(((uint32_t)0x1FU) << PLLCON_EXTAFC_SHIFT)
#define PLLCON_AFC_ENB_MASK		\
		(((uint32_t)0x1U) << PLLCON_AFC_ENB_SHIFT)

/*
 * PLLMON
 */
#define PLLMON_AFCINIT_SEL_SHIFT	((uint32_t)15U)
#define PLLMON_FOUT_MASK_SHIFT		((uint32_t)14U)
#define PLLMON_FEED_EN_SHIFT		((uint32_t)13U)
#define PLLMON_FSEL_SHIFT		((uint32_t)12U)
#define PLLMON_LRD_EN_SHIFT		((uint32_t)11U)
#define PLLMON_VCO_BOOST_SHIFT		((uint32_t)10U)
#define PLLMON_PBIAS_CTRL_EN_SHIFT	((uint32_t)9U)
#define PLLMON_PBIAS_CTRL__SHIFT	((uint32_t)8U)
#define PLLMON_AFC_CODE_SHIFT		((uint32_t)0U)

#define PLLMON_AFCINIT_SEL_MASK		\
		(((uint32_t)0x1U) << PLLMON_AFCINIT_SEL_SHIFT)
#define PLLMON_FOUT_MASK_MASK		\
		(((uint32_t)0x1U) << PLLMON_FOUT_MASK_SHIFT)
#define PLLMON_FEED_EN_MASK		\
		(((uint32_t)0x1U) << PLLMON_FEED_EN_SHIFT)
#define PLLMON_FSEL_MASK		\
		(((uint32_t)0x1U) << PLLMON_FSEL_SHIFT)
#define PLLMON_LRD_EN_MASK		\
		(((uint32_t)0x1U) << PLLMON_LRD_EN_SHIFT)
#define PLLMON_VCO_BOOST_MASK		\
		(((uint32_t)0x1U) << PLLMON_VCO_BOOST_SHIFT)
#define PLLMON_PBIAS_CTRL_EN_MASK	\
		(((uint32_t)0x1U) << PLLMON_PBIAS_CTRL_EN_SHIFT)
#define PLLMON_PBIAS_CTRL__MASK		\
		(((uint32_t)0x1U) << PLLMON_PBIAS_CTRL__SHIFT)
#define PLLMON_AFC_CODE_MASK		\
		(((uint32_t)0x1FU) << PLLMON_AFC_CODE_SHIFT)

/*
 * CLKDIVC
 */
#define CLKDIVC_PE_SHIFT		((uint32_t)7U)
#define CLKDIVC_PDIV_SHIFT		((uint32_t)0U)

#define CLKDIVC_PE_MASK			\
		(((uint32_t)0x1U) << CLKDIVC_PE_SHIFT)
#define CLKDIVC_PDIV_MASK		\
		(((uint32_t)0x3FU) << CLKDIVC_PDIV_SHIFT)

#endif
