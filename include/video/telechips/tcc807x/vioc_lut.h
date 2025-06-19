/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 *
 * Copyright (C) Telechips, Inc.
 *
 */
#ifndef VIOC_LUT_H
#define	VIOC_LUT_H

/*
 * register offset
 */
#define LUT_CTRL_OFFSET			(0x0U)
#define LUT_DEV0_CFG_OFFSET		(0x04U)
#define LUT_DEV1_CFG_OFFSET		(0x08U)
#define LUT_DEV2_CFG_OFFSET		(0x0CU)
#define LUT_DEV3_CFG_OFFSET		(0x10U)
#define LUT_DEV4_CFG_OFFSET		(0x14U)
#define LUT_VIOC0_CFG_OFFSET	(0x20U)
#define LUT_VIOC1_CFG_OFFSET	(0x24U)
#define LUT_TABLE_OFFSET		(0x400U)

/*
 * LUT Configuration Register
 */
#define LUT_CTRL_AL_SHIFT		(31U)
#define LUT_CTRL_SEL_SHIFT		(0U)

#define LUT_CTRL_AL_MASK		((u32)0x1U << LUT_CTRL_AL_SHIFT)
#define LUT_CTRL_SEL_MASK		((u32)0xFU << LUT_CTRL_SEL_SHIFT)

/*
 * LUT in DEVk Configuration Register
 */
#define LUT_DEV_CFG_EN_SHIFT	(31U)

#define LUT_DEV_CFG_EN_MASK		((u32)0x1U << LUT_DEV_CFG_EN_SHIFT)

/*
 * LUT in VIOCk Configuration Register
 */
#define LUT_VIOC_CFG_EN_SHIFT	(31U)
#define LUT_VIOC_CFG_SEL_SHFTT	(0U)

#define LUT_VIOC_CFG_EN_MASK	((u32)0x1U << LUT_VIOC_CFG_EN_SHIFT)
#define LUT_VIOC_CFG_SEL_MASK	((u32)0xFFU << LUT_VIOC_CFG_SEL_SHFTT)

/*
 * LUT Initialize Register
 */
#define LUT_TABLE_R_SHIFT		(16U)
#define LUT_TABLE_G_SHIFT		(8U)
#define LUT_TABLE_B_SHIFT		(0U)

#define LUT_TABLE_R_MASK		((u32)0xFFU << LUT_TABLE_R_SHIFT)
#define LUT_TABLE_G_MASK		((u32)0xFFU << LUT_TABLE_G_SHIFT)
#define LUT_TABLE_B_MASK		((u32)0xFFU << LUT_TABLE_B_SHIFT)

#define LUT_COLOR_DEPTH			(8U)
#define LUT_TABLE_SIZE			((u32)1U << LUT_COLOR_DEPTH)

enum {
	LUT_TYPE_NONE 	= -1,
	LUT_TYPE_COMP 	= 0,
	LUT_TYPE_DEV 	= 1,
};

extern int tcc_set_lut_table(unsigned int lut_n, const unsigned int *table);
extern int tcc_set_lut_plugin(unsigned int lut_n, unsigned int plugComp);
extern int tcc_get_lut_plugin(unsigned int lut_n);
extern void tcc_set_lut_enable(unsigned int lut_n, unsigned int enable);
extern int tcc_get_lut_enable(unsigned int lut_n);
extern void __iomem *VIOC_LUT_GetAddress(void);
extern void __iomem *lut_get_address(unsigned int lut_n, int *is_dev);
extern int lut_get_pluginComponent_index(unsigned int tvc_n);
extern int lut_get_Component_index_to_tvc(unsigned int plugin_n);

#endif /* VIOC_LUT_H */
