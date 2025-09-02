/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef LINUX_PHY_TCC_EHCI_H
#define LINUX_PHY_TCC_EHCI_H

#include <linux/usb/tcc.h>
#include <linux/usb/tcc-phy.h>

// USB20H PHY Configuration 0 Register (USB20H_PCFG0)
// base address: 0x11DA0014
#define REFCLKSEL	(uint32_t)(BIT(5)) // The PLL uses CLKCORE as reference
#if defined(CONFIG_ARCH_TCC807X)
			// 24MHz, Double bandwidth mode enabled
#define FSEL		(uint32_t)(BIT(3) | BIT(1))
#else
			// 24MHz, reference clock frequency
#define FSEL		(uint32_t)(BIT(2) | BIT(0))
#endif

#define PCFG0_RST /* reset value: 0x83000025 */ \
	(uint32_t)(PHY_POR | BIT(25) | SIDDQ | REFCLKSEL | FSEL)

// USB20H PHY Configuration 2 Register(USB20H_PCFG2)
// base address: 0x11DA001C
#define CHGDET		(uint32_t)(BIT(22))
#define CHRGSEL		(uint32_t)(BIT(10))
#define VDATSRCENB	(uint32_t)(BIT(9))
#define VDATDETENB	(uint32_t)(BIT(8))

// USB20H PHY Configuration 3 Register (USB20H_PCFG3)
// base address: 0x11DA0020
#define VDATREFTUNE0	(uint32_t)(BIT(4))
#define RETENABLEN	(uint32_t)(BIT(0))

// USB20H PHY Configuration 4 Register (USB20H_PCFG4)
// base address: 0x11DA0024
#define IRQ_CLR		(uint32_t)(BIT(31))
#define IRQ_PHYVALIDEN	(uint32_t)(BIT(30))
#define IRQ_CHGDETEN	(uint32_t)(BIT(28))
#define IRQ_PHYVALID	(uint32_t)(BIT(27))
#define DPPULLDOWN	(uint32_t)(BIT(12))
#define DMPULLDOWN	(uint32_t)(BIT(10))

#define DP_DM_PULLDOWN	(uint32_t)(DPPULLDOWN | DMPULLDOWN)

// USB20H LINK Configuration Register 0(USB20H_LCFG0)
// base address: 0x11DA002C
#define PHYRSTN		(uint32_t)(BIT(29))
#define UTMIRSTN	(uint32_t)(BIT(28))
			// should be the same as FLADJ_VAL_HOST(0x20)
#define FLADJ_VAL	(uint32_t)(BIT(15))
			// Frame Length: 60000, FLADJ Value: 32(0x20)
#define FLADJ_VAL_HOST	(uint32_t)(BIT(5))

#define LCFG0_RST /* reset value: 0x30048020 */ \
	(uint32_t)(PHYRSTN | UTMIRSTN | BIT(18) | FLADJ_VAL | FLADJ_VAL_HOST)

static int32_t tcc_ehci_phy_create(struct device *dev,
		struct tcc_ehci_phy *tcc_ehci);
static int32_t tcc_ehci_phy_init(struct usb_phy *uphy);
static void tcc_ehci_phy_shutdown(struct usb_phy *uphy);
static int32_t tcc_ehci_phy_set_vbus(struct usb_phy *uphy, int32_t on_off);

static int32_t tcc_ehci_phy_set_vbus_resource(struct usb_phy *uphy);
#if 0
static void __iomem *tcc_ehci_phy_get_base(struct usb_phy *uphy);
static void tcc_ehci_phy_select_mux(struct usb_phy *uphy, int32_t is_mux);
static int32_t tcc_ehci_phy_set_state(struct usb_phy *uphy, int32_t on_off);
static int32_t tcc_ehci_phy_set_vbus_resource(struct usb_phy *uphy);

#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
static uint32_t tcc_ehci_phy_get_dc_level(struct usb_phy *uphy);
static int32_t tcc_ehci_phy_set_dc_level(struct usb_phy *uphy, uint32_t level);
#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */

#if defined(CONFIG_ENABLE_BC_20_HOST) || defined(CONFIG_ENABLE_BC_20_DRD)
static irqreturn_t tcc_ehci_phy_chg_det_irq(int irq, void *data);
static void tcc_ehci_phy_chg_det_monitor(struct work_struct *data);
static int tcc_ehci_phy_chg_det_thread(void *work);
static void tcc_ehci_phy_set_chg_det(struct usb_phy *uphy);
static void tcc_ehci_phy_stop_chg_det(struct usb_phy *uphy);
static int32_t tcc_ehci_phy_init_battery_charging(struct platform_device *pdev,
		struct tcc_ehci_phy *tcc_ehci);
#endif /* CONFIG_ENABLE_BC_20_HOST || CONFIG_ENABLE_BC_20_DRD */
#endif

static int32_t tcc_ehci_phy_probe(struct platform_device *pdev);
static int32_t tcc_ehci_phy_remove(struct platform_device *pdev);

static int32_t __init tcc_ehci_phy_drv_init(void);
static void __exit tcc_ehci_phy_drv_cleanup(void);

#endif /* LINUX_PHY_TCC_EHCI_H */
