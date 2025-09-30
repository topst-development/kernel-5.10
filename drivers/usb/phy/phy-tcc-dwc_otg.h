/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef LINUX_PHY_TCC_DWC_OTG_H
#define LINUX_PHY_TCC_DWC_OTG_H

#include <linux/usb/tcc.h>
#include <linux/usb/tcc-phy.h>

// USB Host/Device PHY Configuration Register0 (U20DH_DEV_PCFG0)
// base address: 0x11DA0100
#define IRQ_PHYVLDEN	(uint32_t)(BIT(20))
#define RCS		(uint32_t)(BIT(5)) // The PLL uses CLKCORE as reference
#if defined(CONFIG_ARCH_TCC807X)
			// 24MHz, Double bandwidth mode enabled
#define FSEL		(uint32_t)(BIT(3) | BIT(1))
#else
			// 24MHz, reference clock frequency
#define FSEL		(uint32_t)(BIT(2) | BIT(0))
#endif

#define PCFG0_RST /* reset value: 0x83000025 */ \
	(uint32_t)(PHY_POR | BIT(25) | SIDDQ | RCS | FSEL)

// USB Host/Device PHY Configuration Register2 (USB20DH_PCFG2)
// base address: 0x11DA0108
#define ACAENB		(uint32_t)(BIT(13))

// USB Host/Device PHY Configuration 3 Register (USB20DH_PCFG3)
// base address: 0x11DA010C
#define VDATREFTUNE0	(uint32_t)(BIT(4))
#define RETENABLEN	(uint32_t)(BIT(0))

// USB Host/Device PHY Configuration 4 Register (USB20DH_PCFG4)
// base address: 0x11DA0110
#define IRQ_PHYVALID	(uint32_t)(BIT(27))
#define DPPD		(uint32_t)(BIT(12))
#define DMPD		(uint32_t)(BIT(10))

// USB Host/Device LINK Configuration 0 Register (USB20DH_LCFG0)
// base address: 0x11DA0118
#define PRSTN		(uint32_t)(BIT(29))

static int32_t tcc_dwc_otg_phy_create(struct device *dev,
		struct tcc_dwc_otg_phy *tcc_dwc_otg);
static int32_t tcc_dwc_otg_phy_init(struct usb_phy *uphy);
static int32_t tcc_dwc_otg_phy_set_vbus(struct usb_phy *uphy, int32_t on_off);

#if 0
static void __iomem *tcc_dwc_otg_phy_get_base(struct usb_phy *uphy);
static int32_t tcc_dwc_otg_phy_set_vbus_resource(struct usb_phy *uphy);

#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
static uint32_t tcc_dwc_otg_phy_get_dc_level(struct usb_phy *uphy);
static int32_t tcc_dwc_otg_phy_set_dc_level(struct usb_phy *uphy, uint32_t level);
#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */
#endif

static int32_t tcc_dwc_otg_phy_probe(struct platform_device *pdev);
static int32_t tcc_dwc_otg_phy_remove(struct platform_device *pdev);

static int32_t __init tcc_dwc_otg_phy_drv_init(void);
static void __exit tcc_dwc_otg_phy_drv_cleanup(void);

#endif /* LINUX_PHY_TCC_DWC_OTG_H */
