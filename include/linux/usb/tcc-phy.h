/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __LINUX_USB_TCC_PHY_H
#define __LINUX_USB_TCC_PHY_H

#define PHY_POR		(uint32_t)(BIT(31))
#define SIDDQ		(uint32_t)(BIT(24))

// USB PHY Configuration Register
#define PCFG_TXVRT		(uint32_t)(BIT(3) | BIT(2) | BIT(1) | BIT(0))

#define TRUE			(1)
#define FALSE			(0)

#define ON_VOLTAGE		(5000000)
#define OFF_VOLTAGE		(1)

struct U20DH_DEV_PHY { // base address: 0x11DA0100
	volatile uint32_t PCFG0;		// 0x00
	volatile uint32_t PCFG1;		// 0x04
	volatile uint32_t PCFG2;		// 0x08
	volatile uint32_t PCFG3;		// 0x0C
	volatile uint32_t PCFG4;		// 0x10
	volatile uint32_t LSTS;			// 0x14
	volatile uint32_t LCFG0;		// 0x18
	volatile uint32_t reserved[3];		// 0x1C ~ 24
	volatile uint32_t MUX_SEL;		// 0x28
	volatile uint32_t VBUSVLD_SEL;		// 0x2C
};

struct tcc_ehci_phy {
	struct device		*dev;
	struct usb_phy		uphy;
	struct regulator	*vbus_supply;

	struct USB20H_PHY __iomem	*phy_regs;

	bool			vbus_enabled;
	bool			is_mux;

#if defined(CONFIG_ENABLE_BC_20_HOST) || defined(CONFIG_ENABLE_BC_20_DRD)
	struct task_struct	*ehci_chgdet_thread;
	struct work_struct	chgdet_work;

	bool			chg_ready;
	int32_t			charge_detect_irq;
#endif /* CONFIG_ENABLE_BC_20_HOST || CONFIG_ENABLE_BC_20_DRD */
};

struct tcc_dwc_otg_phy {
	struct device		*dev;
	struct usb_phy		uphy;
	struct regulator	*vbus_supply;

	struct U20DH_DEV_PHY __iomem	*phy_regs;

	bool			vbus_enabled;
	bool			is_mux;
};

struct tcc_dwc3_phy {
	struct device		*dev;
	struct usb_phy		uphy;
	struct regulator	*vbus_supply;

	struct U30_PHY __iomem	*phy_regs;
	void __iomem		*ref_base;

	bool			vbus_enabled;

#if defined(CONFIG_ENABLE_BC_30_HOST)
	struct work_struct	dwc3_work;
	struct task_struct	*dwc3_chgdet_thread;

	bool			chg_ready;
	int32_t			charge_detect_irq;
#endif /* CONFIG_ENABLE_BC_30_HOST */
};

#endif /* __LINUX_USB_TCC_PHY_H */
