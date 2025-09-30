/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef VIOC_DISP_INTR__H
#define VIOC_DISP_INTR__H

struct vioc_intr_type {
	unsigned int id;
	unsigned int bits;
};

enum {
	VIOC_INTR_DEV0   = 0,
	VIOC_INTR_RESERVED_1,
	VIOC_INTR_RESERVED_2,
	VIOC_INTR_RESERVED_3,
	VIOC_INTR_RESERVED_4,
	VIOC_INTR_RESERVED_5,
	VIOC_INTR_RESERVED_6,
	VIOC_INTR_RESERVED_7,
	VIOC_INTR_RESERVED_8,
	VIOC_INTR_RESERVED_9,
	VIOC_INTR_RESERVED_10,
	VIOC_INTR_RESERVED_11,
	VIOC_INTR_RD0    = 12,
	VIOC_INTR_RD1    = 13,
	VIOC_INTR_RD2    = 14,
	VIOC_INTR_RD3    = 15,
	VIOC_INTR_RD4    = 16,
	VIOC_INTR_RD5    = 17,
	VIOC_INTR_RD6    = 18,
	VIOC_INTR_RESERVED_19,
	VIOC_INTR_RESERVED_20,
	VIOC_INTR_RESERVED_21,
	VIOC_INTR_RESERVED_22,
	VIOC_INTR_RESERVED_23,
	VIOC_INTR_RESERVED_24,
	VIOC_INTR_RESERVED_25,
	VIOC_INTR_RESERVED_26,
	VIOC_INTR_RESERVED_27,
	VIOC_INTR_RESERVED_28,
	VIOC_INTR_RESERVED_29,
	VIOC_INTR_RESERVED_30,
	VIOC_INTR_RESERVED_31,
	VIOC_INTR_RESERVED_32,
	VIOC_INTR_RESERVED_33,
	VIOC_INTR_RESERVED_34,
	VIOC_INTR_RESERVED_35,
	VIOC_INTR_RESERVED_36,
	VIOC_INTR_RESERVED_37,
	VIOC_INTR_RESERVED_38,
	VIOC_INTR_RESERVED_39,
	VIOC_INTR_RESERVED_40,
	VIOC_INTR_RESERVED_41,
	VIOC_INTR_RESERVED_42,
	VIOC_INTR_RESERVED_43,
	VIOC_INTR_RESERVED_44,
	VIOC_INTR_RESERVED_45,
	VIOC_INTR_RESERVED_46,
	VIOC_INTR_RESERVED_47,
	VIOC_INTR_RESERVED_48,
	VIOC_INTR_RESERVED_49,
	VIOC_INTR_RESERVED_50,
	VIOC_INTR_RESERVED_51,
	VIOC_INTR_SC0    = 52,
	VIOC_INTR_SC1    = 53,
	VIOC_INTR_SC2    = 54,
	VIOC_INTR_RESERVED_55,
	VIOC_INTR_RESERVED_56,
	VIOC_INTR_RESERVED_57,
	VIOC_INTR_RESERVED_58,
	VIOC_INTR_RESERVED_59,
	VIOC_INTR_RESERVED_60,
	VIOC_INTR_RESERVED_61,
	VIOC_INTR_WMIX0  = 62,
	VIOC_INTR_WMIX1  = 63,
	VIOC_INTR_RESERVED_64,
	VIOC_INTR_RESERVED_65,
	VIOC_INTR_RESERVED_66,
	VIOC_INTR_RESERVED_67,
	VIOC_INTR_RESERVED_68,
	VIOC_INTR_RESERVED_69,
	VIOC_INTR_RESERVED_70,
	VIOC_INTR_RESERVED_71,
	VIOC_INTR_WD0    = 72,
	VIOC_INTR_WD1    = 73,
	VIOC_INTR_WD2    = 74,
	VIOC_INTR_WD3    = 75,
	VIOC_INTR_RESERVED_76,
	VIOC_INTR_RESERVED_77,
	VIOC_INTR_RESERVED_78,
	VIOC_INTR_RESERVED_79,
	VIOC_INTR_RESERVED_80,
	VIOC_INTR_RESERVED_81,
	VIOC_INTR_RESERVED_82,
	VIOC_INTR_RESERVED_83,
	VIOC_INTR_RESERVED_84,
	VIOC_INTR_RESERVED_85,
	VIOC_INTR_RESERVED_86,
	VIOC_INTR_RESERVED_87,
	VIOC_INTR_RESERVED_88,
	VIOC_INTR_RESERVED_89,
	VIOC_INTR_RESERVED_90,
	VIOC_INTR_RESERVED_91,
	VIOC_INTR_RESERVED_92,
	VIOC_INTR_RESERVED_93,
	VIOC_INTR_RESERVED_94,
	VIOC_INTR_RESERVED_95,
	VIOC_INTR_RESERVED_96,
	VIOC_INTR_RESERVED_97,
	VIOC_INTR_RESERVED_98,
	VIOC_INTR_RESERVED_99,
	VIOC_INTR_TIMER  = 100,
	VIOC_INTR_RESERVED_101,
	VIOC_INTR_FIFO0  = 102,
	VIOC_INTR_FIFO1  = 103,
	VIOC_INTR_NUM    = VIOC_INTR_FIFO1
};

/* VIOC DEV0/1/2 irqs */
enum vioc_disp_intr_src {
	VIOC_DISP_INTR_FU = 0, /* LCD output fifo under-run */
	VIOC_DISP_INTR_VSR,    /* VS Rising */
	VIOC_DISP_INTR_VSF,    /* VS Falling */
	VIOC_DISP_INTR_RU,     /* Register Update */
	VIOC_DISP_INTR_DD,     /* Disable Done */
	VIOC_DISP_INTR_SREQ,   /* Device Stop Request */
	VIOC_DISP_INTR_MAX
};
#if !defined(CONFIG_ARCH_TCC750X)
#define VIOC_INTR_DISP_OFFSET \
	(VIOC_INTR_DEV3 - (VIOC_INTR_DEV2 + 1))
#endif
#define VIOC_DISP_INTR_DISPLAY \
	(/*(1<<VIOC_DISP_INTR_FU)|*/ \
	(1U << VIOC_DISP_INTR_RU) \
	| (1U << VIOC_DISP_INTR_DD))
#define VIOC_DISP_INT_MASK	(((u32)1U << VIOC_DISP_INTR_MAX) - 1U)

/* VIOC RDMA irqs */
enum vioc_rdma_intr_src {
	VIOC_RDMA_INTR_CFG = 0,		/* Configurate Update */
	VIOC_RDMA_INTR_EOFR,		/* EOF Rising*/
	VIOC_RDMA_INTR_EOFF,		/* EOF Falling */
	VIOC_RDMA_INTR_UPDD,		/* Update Done */
	VIOC_RDMA_INTR_EOFW,		/* EOF-WAIT Rising */
	VIOC_RDMA_INTR_TOPR,		/* TOP Ready */
	VIOC_RDMA_INTR_BOTR,		/* Bottom Ready */
	VIOC_RDMA_INTR_MAX
};
#define VIOC_RDMA_INT_MASK	(((u32)1U << VIOC_RDMA_INTR_MAX) - 1U)

/* VIOC WDMA irqs */
enum vioc_wdma_intr_src {
	VIOC_WDMA_INTR_UPD = 0,	/* Register Update */
	VIOC_WDMA_INTR_SREQ,	/* VIOC_WDMA_INTR_EOFF */
	VIOC_WDMA_INTR_ROL,		/* Rolling */
	VIOC_WDMA_INTR_ENR,		/* Synchronized Enable Rising */
	VIOC_WDMA_INTR_ENF,		/* Synchronized Enable Falling */
	VIOC_WDMA_INTR_EOFR,	/* EOF Rising */
	VIOC_WDMA_INTR_EOFF,	/* EOF Falling */
	VIOC_WDMA_INTR_SEOFR,	/* Sync EOF Rising */
	VIOC_WDMA_INTR_SEOFF,	/* Sync EOF Falling */
	VIOC_WDMA_INTR_RESERVED,
	VIOC_WDMA_INTR_MAX
};
#define VIOC_WDMA_INT_MASK	(((u32)1U << VIOC_WDMA_INTR_MAX) - 1U)

#if !defined(CONFIG_ARCH_TCC750X)
#define VIOC_INTR_WD_OFFSET (VIOC_INTR_WD9 - (VIOC_INTR_WD8 + 1))
#define VIOC_INTR_WD_OFFSET2 (VIOC_INTR_WD13 - (VIOC_INTR_WD12 + 1))
#endif


/* VIOC VIN irqs */
enum vioc_vin_intr_src {
	VIOC_VIN_INTR_UPD = 0,
	VIOC_VIN_INTR_EOF,
	VIOC_VIN_INTR_VS,
	VIOC_VIN_INTR_INVS,
	VIOC_VIN_INTR_MAX
};
#define VIOC_VIN_INT_ENABLE			(((u32)1U << VIOC_VIN_INTR_MAX) - 1U)
#define VIOC_VIN_INT_MASK			0xCU

#if !defined(CONFIG_ARCH_TCC750X)
#define VIOC_INTR_VIN_OFFSET		(VIOC_INTR_VIN4 - (VIOC_INTR_VIN3 + 1))
#endif

/* VIOC SC irqs */
#define VIOC_SC_INT_MASK	0xFU

/* VIOC WMIX irqs */
#define VIOC_WMIX_INT_MASK	0x1FU

#if defined(CONFIG_VIOC_PVRIC_FBDC)
enum vioc_pvric_fbdc_intr_src {
	VIOC_PVRIC_FBDC_INTR_IDLE = 0,
	VIOC_PVRIC_FBDC_INTR_UPD,
	VIOC_PVRIC_FBDC_INTR_TILE_ERR,
	VIOC_PVRIC_FBDC_INTR_ADDR_ERR,
	VIOC_PVRIC_FBDC_INTR_EOF_ERR,
	VIOC_PVRIC_FBDC_INTR_MAX
};
#define VIOC_PVRIC_FBDC_INT_MASK	(((u32)1U << VIOC_PVRIC_FBDC_INTR_MAX) - 1U)
#endif

extern int vioc_intr_enable(int irq, int id, unsigned int mask);
extern int vioc_intr_disable(int irq, int id, unsigned int mask);
extern unsigned int vioc_intr_get_status(int id);
extern bool check_vioc_irq_status(const void __iomem *reg, int id);
extern bool is_vioc_intr_activatied(int id, unsigned int mask);
extern int vioc_intr_clear(int id, unsigned int mask);
extern void vioc_intr_initialize(void);
extern bool is_vioc_intr_unmasked(int id, unsigned int mask);
bool is_vioc_display_device_intr_masked(int id, unsigned int mask);
extern int vioc_intr_init(void);

#endif
