/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __LINUX_USB_TCC_H
#define __LINUX_USB_TCC_H

#include <linux/bitfield.h>
#include <linux/kthread.h>
#include <linux/usb/ehci_def.h>
#include <linux/usb/ehci_pdriver.h>
#include <linux/usb/ohci_pdriver.h>

// EHCI Port Status and Control Register (PORTSC)
#define PORTSC			(0x54)
#define PRT_TSTCTL_MASK		(uint32_t)(BIT(19) | BIT(18) | BIT(17) | BIT(16))
#define PRT_TSTCTL_SHIFT	(16)

// HSIO Sub-system USB Host/Device SEL (USB20DH_SEL)
// base address: 0x11DA0128
#define H_SWRST			(uint32_t)(BIT(4))
#define H_CLKMSK		(uint32_t)(BIT(3))
#define D_SWRST			(uint32_t)(BIT(2))
#define D_CLKMSK		(uint32_t)(BIT(1))
#define SEL			(uint32_t)(BIT(0))

#define MUX_HST_SEL		(H_SWRST | H_CLKMSK)
#define MUX_DEV_SEL		(D_SWRST | D_CLKMSK)

#define ENABLE			(1)
#define DISABLE			(0U)

#define ON			(1)
#define OFF			(0)

#define HOST			(1)
#define DEV			(0)

#define PCFG_MAX		(7)

#define BIT_MSK(val, mask)	((val) & (mask))
#define BIT_SET(val, mask)	((val) |= (mask))
#define BIT_CLR(val, mask)	((val) &= ~((uint32_t)mask))
#define BIT_CLR_SET(val, clr_mask, set_mask) \
	((val) = (((val) & ~((uint32_t)clr_mask)) | ((uint32_t)set_mask)))

struct USB20H_PHY {	    // base address: 0x11DA0010
	volatile uint32_t BCFG;			// 0x00
	volatile uint32_t PCFG0;		// 0x04
	volatile uint32_t PCFG1;		// 0x08
	volatile uint32_t PCFG2;		// 0x0C
	volatile uint32_t PCFG3;		// 0x10
	volatile uint32_t PCFG4;		// 0x14
	volatile uint32_t STS;			// 0x18
	volatile uint32_t LCFG0;		// 0x1C
	volatile uint32_t LCFG1;		// 0x20
};

struct U30_PHY { // base address: 0x11D90000
	volatile uint32_t CLKMASK;              // 0x00
	volatile uint32_t SWRESETN;             // 0x04
	volatile uint32_t PWRCTRL;              // 0x08
	volatile uint32_t OVERCRNT_SEL;         // 0x0C
	volatile uint32_t PCFG0;                // 0x10
	volatile uint32_t PCFG1;                // 0x14
	volatile uint32_t PCFG2;                // 0x18
	volatile uint32_t PCFG3;                // 0x1C
	volatile uint32_t PCFG4;                // 0x20
	volatile uint32_t PCFG5;                // 0x24
	volatile uint32_t PCFG6;                // 0x28
	volatile uint32_t PCFG7;                // 0x2C
	volatile uint32_t PCFG8;                // 0x30
	volatile uint32_t PCFG9;                // 0x34
	volatile uint32_t PCFG10;               // 0x38
	volatile uint32_t PCFG11;               // 0x3C
	volatile uint32_t PCFG12;               // 0x40
	volatile uint32_t PCFG13;               // 0x44
	volatile uint32_t PCFG14;               // 0x48
	volatile uint32_t PCFG15;               // 0x4C
	volatile uint32_t PCFG16;               // 0x50
	volatile uint32_t reserved0[10];        // 0x54 ~ 0x78
	volatile uint32_t PINT;                 // 0x7C
	volatile uint32_t LCFG;                 // 0x80
	volatile uint32_t PCR0;                 // 0x84
	volatile uint32_t PCR1;                 // 0x88
	volatile uint32_t PCR2;                 // 0x8C
	volatile uint32_t reserved1[1];         // 0x90
	volatile uint32_t DBG0;                 // 0x94
	volatile uint32_t DBG1;                 // 0x98
	volatile uint32_t reserved2[1];         // 0x9C
	volatile uint32_t FPCFG0;               // 0xA0
	volatile uint32_t FPCFG1;               // 0xA4
	volatile uint32_t FPCFG2;               // 0xA8
	volatile uint32_t FPCFG3;               // 0xAC
	volatile uint32_t FPCFG4;               // 0xB0
	volatile uint32_t FLCFG0;               // 0xB4
};

struct pcfg_field {
	char *name;
	uint32_t value;
};

extern int32_t ehci_phy_set;

extern bool of_usb_host_tpl_support(struct device_node *np);

#endif /* __LINUX_USB_TCC_H */
