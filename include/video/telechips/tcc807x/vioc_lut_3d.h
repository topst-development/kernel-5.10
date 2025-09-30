/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef VIOC_3D_LUT_H
#define VIOC_3D_LUT_H

#define LUT_3D_CTRL_OFFSET			(0x00U)
#define LUT_3D_PEND_OFFSET			(0x08U)
#define LUT_3D_TABLE_OFFSET			(0x800U)

// 3D LUT Ctrl register
#define LUT_3D_CTRL_SEL_SHIFT		(28U)
#define LUT_3D_CTRL_UPD_SHIFT		(16U)
#define LUT_3D_CTRL_BYPASS_SHIFT	(1U)
#define LUT_3D_CTRL_ENABLE_SHIFT	(0U)

#define LUT_3D_CTRL_SEL_MASK		((u32)0xFU << LUT_3D_CTRL_SEL_SHIFT)
#define LUT_3D_CTRL_UPD_MASK		((u32)0x1U << LUT_3D_CTRL_UPD_SHIFT)
#define LUT_3D_CTRL_BYPASS_MASK		((u32)0x1U << LUT_3D_CTRL_BYPASS_SHIFT)
#define LUT_3D_CTRL_ENABLE_MASK		((u32)0x1U << LUT_3D_CTRL_ENABLE_SHIFT)

// 3D LUT Pend register
#define LUT_3D_PEND_PEND_SHIFT		(0U)
#define LUT_3D_PEND_PEND_MASK		((u32)0x1U << LUT_3D_PEND_PEND_SHIFT)

extern int vioc_lut_3d_init(void);
extern int vioc_lut_3d_set_table(unsigned int lut_n, const unsigned int *lut3dtable);
extern int vioc_lut_3d_bypass(unsigned int lut_n, unsigned int onoff);
extern void __iomem *VIOC_LUT_3D_GetAddress(unsigned int lut_n);

#endif /* VIOC_3D_LUT_H */
