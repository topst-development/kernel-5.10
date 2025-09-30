/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef LINUX_PHY_TCC_DWC3_H
#define LINUX_PHY_TCC_DWC3_H

#include <linux/usb/tcc.h>
#include <linux/usb/tcc-phy.h>

// USB30 PHY Configuration Register 0 (U30_PCFG0)
// base address: 0x11D90010
#define PHY_RESET		(uint32_t)(BIT(30))
#define PD_SS			(uint32_t)(BIT(25))
#define SRAM_INIT_DONE		(uint32_t)(BIT(5))
#define SRAM_EXT_LD_DONE	(uint32_t)(BIT(3))
#define PHY_STABLE		(uint32_t)(BIT(2))

// USB30 PHY Configuration Register 4 (U30_PCFG4)
// base address: 0x11D90020
#define PHY_EXT_CTRL_SEL	(uint32_t)(BIT(0))

// USB30 PHY Configuration Register 5 (U30_PCFG5)
// base address: 0x11D90024
#define EXT_MPLLA_BANDWIDTH_RST	(uint32_t)(BIT(20))
#define EXT_MPLLA_BANDWIDTH	(uint32_t)(BIT(21) | BIT(17))

// USB30 PHY Configuration Register 6 (U30_PCFG6)
// base address: 0x11D90028
#define EXT_MPLLA_SSC_FREQ_CNT_PEAK \
	(uint32_t)(BIT(22) | BIT(21) | BIT(18) | BIT(17))
#define EXT_MPLLA_SSC_FREQ_CNT_INIT \
	(uint32_t)(BIT(9) | BIT(6) | BIT(5))

// USB30 PHY Interrupt Register (U30_PINT)
// base address: 0x11D9007C
#define PINT_EN			(uint32_t)(BIT(31))
#define PINT_STS		(uint32_t)(BIT(22)) // 64(0x40)
#define PINT_MSK_RESERVED	(uint32_t)(BIT(7) | \
		BIT(3) | BIT(2) | BIT(1) | BIT(0))
#define PINT_MSK_CHGDET		(uint32_t)(BIT(6))
#define PINT_MSK_PHY		(uint32_t)(BIT(5))
#define PINT_MSK_BC_CHIRP_ON	(uint32_t)(BIT(4))

// USB30 Link Configuration Register 0 (U30_LCFG)
// base address: 0x11D90080
#define VCC_RESET_N		(uint32_t)(BIT(31))
#define PPC			(uint32_t)(BIT(7))
#define HUB_PORT_PERM_ATTACH \
	(uint32_t)(BIT(27) | BIT(26)) // USB2.0 & 3.0 Port Permanently Attached
				// HS Jitter Adjust: 32(0x20)
#define FLADJ			(uint32_t)(BIT(17))

// USB 3.0 High-speed PHY Configuration Register Set 0 (U30_FPCFG0)
// base address: 0x11D900A0
#define PLLBTUNE		(uint32_t)(BIT(3))
#define FSEL_RESET		(uint32_t)(BIT(2) | BIT(0))
#define FSEL			(uint32_t)(BIT(1))

// USB 3.0 High-speed PHY Configuration Register Set 2 (U30_FPCFG2)
// base address: 0x11D900A8
#define CHGDET			(uint32_t)(BIT(22))
#define CHRGSEL			(uint32_t)(BIT(10))
#define VDATSRCENB		(uint32_t)(BIT(9))
#define VDATDETENB		(uint32_t)(BIT(8))

// USB 3.0 High-speed PHY Configuration Register Set 3 (U30_FPCFG3)
// base address: 0x11D900AC
#define TDOSEL			(uint32_t)(BIT(29))
#define TCK			(uint32_t)(BIT(28))
#define TAD \
	(uint32_t)(BIT(26) | BIT(25)) // Test Address(0x6)
#define TDI \
	(uint32_t)(BIT(23) | BIT(22) | BIT(21) | BIT(20)) // Test Data In(0xC0)
#define TDO \
	(uint32_t)(BIT(15) | BIT(14) | BIT(13) | BIT(12)) // Test Data Out(0xF)

// USB 3.0 High-speed PHY Configuration Register Set 4 (U30_FPCFG4)
// base address: 0x11D900B0
#define	IRQ_CLR			(uint32_t)(BIT(31))
#define IRQ_PHYVALIDEN		(uint32_t)(BIT(30))
#define IRQ_PHYVALID		(uint32_t)(BIT(28))

#define RETRY_CNT		(10000)

static int32_t is_suspended = 1;

static int32_t tcc_dwc3_phy_create(struct device *dev,
		struct tcc_dwc3_phy *tcc_dwc3);
static int32_t tcc_dwc3_phy_init(struct usb_phy *uphy);
static int32_t tcc_dwc3_phy_set_vbus(struct usb_phy *uphy, int32_t on_off);
static int32_t tcc_dwc3_phy_set_suspend(struct usb_phy *uphy, int32_t suspend);

#if 0
#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
static uint32_t tcc_dwc3_phy_get_dc_level(struct usb_phy *uphy);
static int32_t tcc_dwc3_phy_set_dc_level(struct usb_phy *uphy, uint32_t level);
#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */

#if defined(CONFIG_ENABLE_BC_30_HOST)
static irqreturn_t tcc_dwc3_phy_chg_det_irq(int32_t irq, void *data);
static void tcc_dwc3_phy_chg_det_monitor(struct work_struct *data);
static int tcc_dwc3_phy_chg_det_thread(void *work);
static void tcc_dwc3_phy_set_chg_det(struct usb_phy *uphy);
static void tcc_dwc3_phy_stop_chg_det(struct usb_phy *uphy);
static int32_t tcc_dwc3_phy_init_battery_charging(struct platform_device *pdev,
		struct tcc_dwc3_phy *tcc_dwc3);
#endif /* CONFIG_ENABLE_BC_30_HOST */
#endif

static int32_t tcc_dwc3_phy_probe(struct platform_device *pdev);
static int32_t tcc_dwc3_phy_remove(struct platform_device *pdev);

static int32_t __init tcc_dwc3_phy_drv_init(void);
static void __exit tcc_dwc3_phy_drv_cleanup(void);

#endif /* LINUX_PHY_TCC_DWC3_H */
