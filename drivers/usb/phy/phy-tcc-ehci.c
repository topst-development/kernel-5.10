// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/gpio.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/usb/otg.h>

#include "phy-tcc-ehci.h"

static int32_t tcc_ehci_phy_create(struct device *dev,
		struct tcc_ehci_phy *tcc_ehci)
{
	int32_t ret = -ENODEV;

	if ((dev != NULL) && (tcc_ehci != NULL)) {
		tcc_ehci->uphy.otg = devm_kzalloc(dev,
				sizeof(*tcc_ehci->uphy.otg), GFP_KERNEL);
		if (tcc_ehci->uphy.otg == NULL) {
			ret = -ENOMEM;
		} else {
			tcc_ehci->dev = dev;

			tcc_ehci->uphy.dev = tcc_ehci->dev;
			tcc_ehci->uphy.label = "tcc_ehci_phy";
			tcc_ehci->uphy.type = USB_PHY_TYPE_USB2;

			tcc_ehci->uphy.init = tcc_ehci_phy_init;
			tcc_ehci->uphy.shutdown = tcc_ehci_phy_shutdown;
			tcc_ehci->uphy.set_vbus = tcc_ehci_phy_set_vbus;

#if 0
			tcc_ehci->uphy.get_base = tcc_ehci_phy_get_base;
			tcc_ehci->uphy.select_mux = tcc_ehci_phy_select_mux;
			tcc_ehci->uphy.set_state = tcc_ehci_phy_set_state;
			tcc_ehci->uphy.set_vbus_resource =
				tcc_ehci_phy_set_vbus_resource;

#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
			tcc_ehci->uphy.get_dc_level = tcc_ehci_phy_get_dc_level;
			tcc_ehci->uphy.set_dc_level = tcc_ehci_phy_set_dc_level;
#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */
#endif

			if (of_find_property(dev->of_node, "mux", NULL) !=
					NULL) {
				tcc_ehci->is_mux = TRUE;
			} else {
				tcc_ehci->is_mux = FALSE;
			}

#if 0
#if defined(CONFIG_ENABLE_BC_20_HOST) || defined(CONFIG_ENABLE_BC_20_DRD)
			if (((IS_ENABLED(CONFIG_ENABLE_BC_20_HOST) != 0) &&
						(tcc_ehci->mux_port == 0)) ||
					((IS_ENABLED(CONFIG_ENABLE_BC_20_DRD) !=
					  0) && (tcc_ehci->mux_port == 1))) {
				tcc_ehci->uphy.set_chg_det =
					tcc_ehci_phy_set_chg_det;
				tcc_ehci->uphy.stop_chg_det =
					tcc_ehci_phy_stop_chg_det;
			}
#endif /* CONFIG_ENABLE_BC_20_HOST || CONFIG_ENABLE_BC_20_DRD */
#endif

			tcc_ehci->uphy.otg->usb_phy = &tcc_ehci->uphy;

			ret = 0;

			if (IS_ENABLED(CONFIG_ARCH_TCC897X) != 0) {
				ret = tcc_ehci_phy_set_vbus_resource(
						&tcc_ehci->uphy);
			}
		}
	}

	return ret;
}

static int32_t tcc_ehci_phy_init(struct usb_phy *uphy)
{
	struct tcc_ehci_phy *tcc_ehci =
		container_of(uphy, struct tcc_ehci_phy, uphy);
	uint32_t val;
	int32_t i = 0;
	bool is_valid = FALSE;

	dev_info(tcc_ehci->dev, "[INFO][USB] %s() call SUCCESS\n", __func__);

#if 0
	if (tcc_ehci->mux_port != 0) {
		/* get otg control cfg register */
		val = readl(uphy->otg->mux_cfg_addr);
		BIT_CLR_SET(val, SEL, MUX_HST_SEL);
		writel(val, uphy->otg->mux_cfg_addr);
	}
#endif

	// Reset PHY Registers
	if (IS_ENABLED(CONFIG_ARCH_TCC897X)) {
		writel(0x03000115, &tcc_ehci->phy_regs->PCFG0);
	} else {
		writel(PCFG0_RST, &tcc_ehci->phy_regs->PCFG0);
	}

	if (tcc_ehci->is_mux == TRUE) {
		// EHCI MUX Host PHY Configuration
		writel(0xE31C243A, &tcc_ehci->phy_regs->PCFG1);
	} else {
		// EHCI PHY Configuration
		writel(0xE31C243A, &tcc_ehci->phy_regs->PCFG1);
	}

	if (IS_ENABLED(CONFIG_ARCH_TCC897X)) {
		writel(0x4, &tcc_ehci->phy_regs->PCFG2);
	} else {
		writel(0x0, &tcc_ehci->phy_regs->PCFG2);
	}
	writel(VDATREFTUNE0 | RETENABLEN, &tcc_ehci->phy_regs->PCFG3);
	writel(0x0, &tcc_ehci->phy_regs->PCFG4);
	writel(LCFG0_RST, &tcc_ehci->phy_regs->LCFG0);

	// Set the POR
	val = readl(&tcc_ehci->phy_regs->PCFG0);
	writel(val | PHY_POR, &tcc_ehci->phy_regs->PCFG0);
	// Set the Core Reset
	val = readl(&tcc_ehci->phy_regs->LCFG0);
	writel(val & ~(PHYRSTN | UTMIRSTN), &tcc_ehci->phy_regs->LCFG0);

	if (IS_ENABLED(CONFIG_ARCH_TCC897X) == 0) {
		// Clear SIDDQ
		val = readl(&tcc_ehci->phy_regs->PCFG0);
		writel(val & ~SIDDQ, &tcc_ehci->phy_regs->PCFG0);
	}
	udelay(30);

	// Release POR
	val = readl(&tcc_ehci->phy_regs->PCFG0);
	writel(val & ~PHY_POR, &tcc_ehci->phy_regs->PCFG0);

	if (IS_ENABLED(CONFIG_ARCH_TCC897X)) {
		// Clear SIDDQ
		val = readl(&tcc_ehci->phy_regs->PCFG0);
		writel(val & ~SIDDQ, &tcc_ehci->phy_regs->PCFG0);
	}

	// Set Phyvalid en
	val = readl(&tcc_ehci->phy_regs->PCFG4);
	writel(val | IRQ_PHYVALIDEN, &tcc_ehci->phy_regs->PCFG4);
	// Set DP/DM (pull down)
	val = readl(&tcc_ehci->phy_regs->PCFG4);
	writel(val | DP_DM_PULLDOWN, &tcc_ehci->phy_regs->PCFG4);

	// Wait Phy Valid Interrupt
	while (i < 10000) {
		if (IS_ENABLED(CONFIG_ARCH_TCC897X)) {
			val = readl(&tcc_ehci->phy_regs->PCFG0);
			if ((val & BIT(21)) != 0U) {
				is_valid = TRUE;
			}
		} else {
			val = readl(&tcc_ehci->phy_regs->PCFG4);
			if ((val & IRQ_PHYVALID) != 0U) {
				is_valid = TRUE;
			}
		}

		if (is_valid == TRUE) {
			break;
		}

		i++;
		udelay(5);
	}

	dev_info(tcc_ehci->dev, "[INFO][USB] EHCI PHY valid check %s\n",
			(i >= 9999) ? "FAIL!" : "SUCCESS");

	// Release Core Reset
	val = readl(&tcc_ehci->phy_regs->LCFG0);
	writel(val | (PHYRSTN | UTMIRSTN), &tcc_ehci->phy_regs->LCFG0);

#if 0
#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
	(void)tcc_ehci_phy_set_dc_level(uphy, CONFIG_USB_HS_DC_VOLTAGE_LEVEL);
#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */

#if defined(CONFIG_ENABLE_BC_20_HOST) || defined(CONFIG_ENABLE_BC_20_DRD)
	if (((IS_ENABLED(CONFIG_ENABLE_BC_20_HOST) != 0) &&
				(tcc_ehci->mux_port == 0)) ||
			((IS_ENABLED(CONFIG_ENABLE_BC_20_DRD) !=
			  0) && (tcc_ehci->mux_port == 1))) {
		dev_info(tcc_ehci->dev, "[INFO][USB] Setting for Charging Detection\n");

		// clear irq
		val = readl(&tcc_ehci->phy_regs->PCFG4);
		writel(val | IRQ_CLR, &tcc_ehci->phy_regs->PCFG4);

		// Disable VBUS Detect
		val = readl(&tcc_ehci->phy_regs->PCFG4);
		writel(val & ~IRQ_PHYVALIDEN, &tcc_ehci->phy_regs->PCFG4);

		// clear irq
		val = readl(&tcc_ehci->phy_regs->PCFG4);
		writel(val & ~IRQ_CLR, &tcc_ehci->phy_regs->PCFG4);
		udelay(1);

		val = readl(&tcc_ehci->phy_regs->PCFG2);
		writel(val | (CHRGSEL | VDATDETENB),
				&tcc_ehci->phy_regs->PCFG2);
		udelay(1);

		// enable CHG_DET interrupt
		val = readl(&tcc_ehci->phy_regs->PCFG4);
		writel(val | IRQ_CHGDETEN, &tcc_ehci->phy_regs->PCFG4);

		INIT_WORK(&tcc_ehci->chgdet_work, tcc_ehci_phy_chg_det_monitor);

		enable_irq((unsigned int)tcc_ehci->charge_detect_irq);
	} else {
		dev_info(tcc_ehci->dev, "[INFO][USB] Not support BC1.2 for this port\n");
	}
#endif /* CONFIG_ENABLE_BC_20_HOST || CONFIG_ENABLE_BC_20_DRD */
#endif

	return 0;
}

static void tcc_ehci_phy_shutdown(struct usb_phy *uphy)
{
#if 0
	if (uphy != NULL) {
		(void)tcc_ehci_phy_set_state(uphy, OFF);
	}
#endif
}

static int32_t tcc_ehci_phy_set_vbus(struct usb_phy *uphy, int32_t on_off)
{
	struct tcc_ehci_phy *tcc_ehci =
		container_of(uphy, struct tcc_ehci_phy, uphy);
	const struct device *dev = tcc_ehci->dev;
	int32_t ret = 0;

	if ((on_off == ON) && (tcc_ehci->vbus_enabled == FALSE)) {
		ret = regulator_enable(tcc_ehci->vbus_supply);
		if (ret == 0) {
			ret = regulator_set_voltage(tcc_ehci->vbus_supply,
					ON_VOLTAGE, ON_VOLTAGE);
			if (ret == 0) {
				tcc_ehci->vbus_enabled = TRUE;
				usleep_range(3000, 5000);
			} else {
				dev_err(dev, "[ERROR][USB] VBus on is FAILED!\n");
				(void)regulator_disable(tcc_ehci->vbus_supply);
			}
		} else {
			dev_err(dev, "[ERROR][USB] Regulator enable is FAILED!\n");
		}
	} else if ((on_off == OFF) && (tcc_ehci->vbus_enabled == TRUE)) {
		ret = regulator_set_voltage(tcc_ehci->vbus_supply,
				OFF_VOLTAGE, OFF_VOLTAGE);
		if (ret == 0) {
			ret = regulator_disable(tcc_ehci->vbus_supply);
			if (ret == 0) {
				tcc_ehci->vbus_enabled = FALSE;
				usleep_range(3000, 5000);
			} else {
				dev_err(dev, "[ERROR][USB] Regulator disable is FAILED!\n");
			}
		} else {
			dev_err(dev, "[ERROR][USB] VBus off is FAILED!\n");
			(void)regulator_disable(tcc_ehci->vbus_supply);
		}
	} else if ((on_off == ON) && (tcc_ehci->vbus_enabled == TRUE)) {
		dev_info(dev, "[INFO][USB] Vbus is already ENABLED!\n");
	} else if ((on_off == OFF) && (tcc_ehci->vbus_enabled == FALSE)) {
		dev_info(dev, "[INFO][USB] Vbus is already DISABLED!\n");
	} else {
		/* Nothing to do */
	}

	return ret;
}

#if 0
static void __iomem *tcc_ehci_phy_get_base(struct usb_phy *uphy)
{
	const struct tcc_ehci_phy *tcc_ehci =
		container_of(uphy, struct tcc_ehci_phy, uphy);

	return tcc_ehci->phy_regs;
}

static void tcc_ehci_phy_select_mux(struct usb_phy *uphy, int32_t is_mux)
{
	const struct tcc_ehci_phy *tcc_ehci =
		container_of(uphy, struct tcc_ehci_phy, uphy);
	uint32_t mux_sel_val;

	if (tcc_ehci->mux_port != 0) {
		mux_sel_val = readl(uphy->otg->mux_cfg_addr);

		if (is_mux != 0) {
			BIT_CLR_SET(mux_sel_val, SEL, MUX_HST_SEL);
			writel(mux_sel_val, uphy->otg->mux_cfg_addr);
		} else {
			BIT_SET(mux_sel_val,
					(SEL |
					 MUX_HST_SEL | MUX_DEV_SEL));
			writel(mux_sel_val, uphy->otg->mux_cfg_addr);
		}
	}
}

static int32_t tcc_ehci_phy_set_state(struct usb_phy *uphy, int32_t on_off)
{
	const struct tcc_ehci_phy *tcc_ehci =
		container_of(uphy, struct tcc_ehci_phy, uphy);

	if (on_off == 1) {
		BIT_CLR(tcc_ehci->phy_regs->PCFG0, (PHY_POR | SIDDQ));
		dev_info(tcc_ehci->dev, "[INFO][USB] EHCI PHY start\n");
	}

	if (on_off == 0) {
		BIT_SET(tcc_ehci->phy_regs->PCFG0, (PHY_POR | SIDDQ));
		dev_info(tcc_ehci->dev, "[INFO][USB] EHCI PHY stop\n");
	}

	dev_info(tcc_ehci->dev, "[INFO][USB] EHCI PHY PCFG0 : 0x%08X\n",
			tcc_ehci->phy_regs->PCFG0);
	return 0;
}
#endif

static int32_t tcc_ehci_phy_set_vbus_resource(struct usb_phy *uphy)
{
	struct tcc_ehci_phy *tcc_ehci =
		container_of(uphy, struct tcc_ehci_phy, uphy);
	struct device *dev = tcc_ehci->dev;
	int32_t ret = 0;

	/*
	 * Check that the "vbus-ctrl-able" property for the USB PHY driver node
	 * is declared in the device tree.
	 */
	if (of_find_property(dev->of_node, "vbus-ctrl-able", NULL) != NULL) {
		/*
		 * Get the vbus regulator declared in the "vbus-supply" property
		 * for the USB PHY driver node.
		 */
		if (tcc_ehci->vbus_supply == NULL) {
			tcc_ehci->vbus_supply =
				devm_regulator_get_optional(dev, "vbus");
			if (IS_ERR(tcc_ehci->vbus_supply)) {
				dev_info(dev, "[INFO][USB] VBus Supply is not valid.\n");
				ret = -ENODEV;
			}
		}
	} else {
		dev_info(dev, "[INFO][USB] vbus-ctrl-able property is not declared.\n");
		ret = -ENODEV;
	}

	return ret;
}

#if 0
#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
static uint32_t tcc_ehci_phy_get_dc_level(struct usb_phy *uphy)
{
	const struct tcc_ehci_phy *tcc_ehci =
		container_of(uphy, struct tcc_ehci_phy, uphy);
	uint32_t pcfg1_val;

	pcfg1_val = readl(&tcc_ehci->phy_regs->PCFG1);

	return BIT_MSK(pcfg1_val, PCFG_TXVRT);
}

static int32_t tcc_ehci_phy_set_dc_level(struct usb_phy *uphy, uint32_t level)
{
	const struct tcc_ehci_phy *tcc_ehci =
		container_of(uphy, struct tcc_ehci_phy, uphy);
	uint32_t pcfg1_val;

	pcfg1_val = readl(&tcc_ehci->phy_regs->PCFG1);
	BIT_CLR_SET(pcfg1_val, PCFG_TXVRT, level);
	writel(pcfg1_val, &tcc_ehci->phy_regs->PCFG1);

	dev_info(tcc_ehci->dev, "[INFO][USB] current DC voltage level: %d\n",
			BIT_MSK(pcfg1_val, PCFG_TXVRT));

	return 0;
}
#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */

#if defined(CONFIG_ENABLE_BC_20_HOST) || defined(CONFIG_ENABLE_BC_20_DRD)
static irqreturn_t tcc_ehci_phy_chg_det_irq(int irq, void *data)
{
	struct tcc_ehci_phy *tcc_ehci = (struct tcc_ehci_phy *)data;
	uint32_t val;

	val = readl(&tcc_ehci->phy_regs->PCFG4);
	writel(val & ~IRQ_CHGDETEN, &tcc_ehci->phy_regs->PCFG4);

	dev_dbg(tcc_ehci->dev, "[DEBUG][USB] Charging Detection!\n");

	// clear irq
	val = readl(&tcc_ehci->phy_regs->PCFG4);
	writel(val | IRQ_CLR, &tcc_ehci->phy_regs->PCFG4);
	udelay(1);

	// clear irq
	val = readl(&tcc_ehci->phy_regs->PCFG4);
	writel(val & ~IRQ_CLR, &tcc_ehci->phy_regs->PCFG4);

	(void)schedule_work(&tcc_ehci->chgdet_work);

	return IRQ_HANDLED;
}

static void tcc_ehci_phy_chg_det_monitor(struct work_struct *data)
{
	struct tcc_ehci_phy *tcc_ehci =
			container_of(data, struct tcc_ehci_phy, chgdet_work);
	uint32_t pcfg2 = 0;
	int32_t timeout_count = 500;

	tcc_ehci->chg_ready = TRUE;

	pcfg2 = readl(&tcc_ehci->phy_regs->PCFG2);
	writel((pcfg2 | VDATSRCENB), &tcc_ehci->phy_regs->PCFG2);

	while (timeout_count > 0) {
		pcfg2 = readl(&tcc_ehci->phy_regs->PCFG2);
		if ((pcfg2 & CHGDET) != 0U) { // Check VDP_SRC signal
			usleep_range(1000, 1100);
			timeout_count--;
		} else {
			break;
		}
	}

	if (timeout_count == 0) {
		dev_dbg(tcc_ehci->dev, "[DEBUG][USB] Timeout - VDM_SRC is still High\n");
	} else {
		dev_dbg(tcc_ehci->dev, "[DEBUG][USB] Time count(%d) - VDM_SRC is low\n",
				(500 - timeout_count));
	}

	pcfg2 = readl(&tcc_ehci->phy_regs->PCFG2);
	writel(pcfg2 & ~(VDATSRCENB | VDATDETENB), &tcc_ehci->phy_regs->PCFG2);

	if (tcc_ehci->chg_ready == TRUE) {
		dev_dbg(tcc_ehci->dev, "[DEBUG][USB] Enable chg det monitor!\n");

		if (tcc_ehci->ehci_chgdet_thread != NULL) {
			(void)kthread_stop(tcc_ehci->ehci_chgdet_thread);
			tcc_ehci->ehci_chgdet_thread = NULL;
		}

		dev_dbg(tcc_ehci->dev, "[DEBUG][USB] start chg det thread!\n");

		tcc_ehci->ehci_chgdet_thread =
			kthread_run(tcc_ehci_phy_chg_det_thread,
					(void *)tcc_ehci, "ehci-chgdet");
		if (IS_ERR(tcc_ehci->ehci_chgdet_thread)) {
			dev_err(tcc_ehci->dev, "[ERROR][USB] failed to run tcc_ehci_phy_chg_det_thread\n");
		}
	} else {
		dev_dbg(tcc_ehci->dev, "[DEBUG][USB] No need to start chg det monitor\n");
	}
}

static int tcc_ehci_phy_chg_det_thread(void *work)
{
	struct tcc_ehci_phy *tcc_ehci = (struct tcc_ehci_phy *)work;
	uint32_t pcfg2 = 0;
	int32_t timeout = 1000;

	dev_dbg(tcc_ehci->dev, "[DEBUG][USB] Start to check CHGDET\n");

	while (!kthread_should_stop() && (timeout > 0)) {
		pcfg2 = readl(&tcc_ehci->phy_regs->PCFG2);
		usleep_range(1000, 1100);
		timeout--;
	}

	if (timeout <= 0) {
		pcfg2 = readl(&tcc_ehci->phy_regs->PCFG2);
		writel(pcfg2 | VDATDETENB, &tcc_ehci->phy_regs->PCFG2);
		pcfg2 = readl(&tcc_ehci->phy_regs->PCFG4);
		writel(pcfg2 | IRQ_CHGDETEN, &tcc_ehci->phy_regs->PCFG4);
		dev_dbg(tcc_ehci->dev, "[DEBUG][USB] Enable VDATDETENB\n");
	}

	tcc_ehci->ehci_chgdet_thread = NULL;

	dev_dbg(tcc_ehci->dev, "[DEBUG][USB] Monitoring is finished(%d)\n",
			timeout);

	return 0;
}

static void tcc_ehci_phy_set_chg_det(struct usb_phy *uphy)
{
	const struct tcc_ehci_phy *tcc_ehci =
		container_of(uphy, struct tcc_ehci_phy, uphy);
	uint32_t val;

	dev_dbg(tcc_ehci->dev, "[DEBUG][USB] start chg det!\n");

	// clear irq
	val = readl(&tcc_ehci->phy_regs->PCFG4);
	writel(val | IRQ_CLR, &tcc_ehci->phy_regs->PCFG4);
	udelay(1);

	// clear irq
	val = readl(&tcc_ehci->phy_regs->PCFG4);
	writel(val & ~IRQ_CLR, &tcc_ehci->phy_regs->PCFG4);

	// enable chg det
	val = readl(&tcc_ehci->phy_regs->PCFG2);
	writel(val | VDATDETENB, &tcc_ehci->phy_regs->PCFG2);
	val = readl(&tcc_ehci->phy_regs->PCFG4);
	writel(val | IRQ_CHGDETEN, &tcc_ehci->phy_regs->PCFG4);
}

static void tcc_ehci_phy_stop_chg_det(struct usb_phy *uphy)
{
	struct tcc_ehci_phy *tcc_ehci =
		container_of(uphy, struct tcc_ehci_phy, uphy);
	uint32_t pcfg2, pcfg4;

	tcc_ehci->chg_ready = FALSE;

	dev_dbg(tcc_ehci->dev, "[DEBUG][USB] stop chg det!\n");

	if (tcc_ehci->ehci_chgdet_thread != NULL) {
		dev_dbg(tcc_ehci->dev, "[DEBUG][USB] kill chg det thread!\n");
		(void)kthread_stop(tcc_ehci->ehci_chgdet_thread);
		tcc_ehci->ehci_chgdet_thread = NULL;
	}

	pcfg2 = readl(&tcc_ehci->phy_regs->PCFG2);
	pcfg4 = readl(&tcc_ehci->phy_regs->PCFG4);
	dev_dbg(tcc_ehci->dev, "[DEBUG][USB] PCFG2= 0x%x/PCFG4=0x%x\n",
			pcfg2, pcfg4);

	// disable chg det
	pcfg4 = readl(&tcc_ehci->phy_regs->PCFG4);
	writel(pcfg4 & ~IRQ_CHGDETEN, &tcc_ehci->phy_regs->PCFG4);
	pcfg2 = readl(&tcc_ehci->phy_regs->PCFG2);
	writel(pcfg2 & ~(VDATSRCENB | VDATDETENB), &tcc_ehci->phy_regs->PCFG2);

	pcfg2 = readl(&tcc_ehci->phy_regs->PCFG2);
	pcfg4 = readl(&tcc_ehci->phy_regs->PCFG4);
	dev_dbg(tcc_ehci->dev, "[DEBUG][USB] Disable chg det! PCFG2= 0x%x/PCFG4=0x%x\n",
			pcfg2, pcfg4);
}

static int32_t tcc_ehci_phy_init_battery_charging(struct platform_device *pdev,
		struct tcc_ehci_phy *tcc_ehci)
{
	int32_t irq = platform_get_irq(pdev, 0);
	struct device *dev = &pdev->dev;
	int32_t ret = -ENODEV;

	if (irq <= 0) {
		dev_err(dev, "[ERROR][USB] Found HC with no IRQ. Check %s setup!\n",
				dev_name(dev));
	} else {
		dev_info(dev, "[INFO][USB] platform_get_irq() SUCCESS, irq: %d\n",
				irq);

		ret = devm_request_irq(dev, (unsigned int)irq,
				tcc_ehci_phy_chg_det_irq,
				IRQF_SHARED, pdev->dev.kobj.name, tcc_ehci);
		if (ret != 0) {
			dev_err(dev, "[ERROR][USB] devm_request_irq() FAIL!, ret: %d\n",
					ret);
		} else {
			disable_irq((unsigned int)irq);
			tcc_ehci->charge_detect_irq = irq;
		}
	}

	return ret;
}
#endif /* CONFIG_ENABLE_BC_20_HOST || CONFIG_ENABLE_BC_20_DRD */
#endif

static int32_t tcc_ehci_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tcc_ehci_phy *tcc_ehci;
	int32_t ret;

	dev_info(dev, "[INFO][USB] %s() call SUCCESS\n", __func__);

	tcc_ehci = devm_kzalloc(dev, sizeof(*tcc_ehci), GFP_KERNEL);

	ret = tcc_ehci_phy_create(dev, tcc_ehci);
	if (ret != 0) {
		dev_err(dev, "[ERROR][USB] tcc_ehci_phy_create() FAIL!\n");
	} else {
		if (request_mem_region(pdev->resource[0].start,
					resource_size(&pdev->resource[0]),
					"ehci_phy") == NULL) {
			dev_dbg(dev, "[DEBUG][USB] error reserving mapped memory\n");
			ret = -EFAULT;
		} else {
			tcc_ehci->phy_regs = ioremap((resource_size_t)
					pdev->resource[0].start,
					resource_size(&pdev->resource[0]));
#if 0
			tcc_ehci->uphy.base = tcc_ehci->phy_regs;
#endif

			platform_set_drvdata(pdev, tcc_ehci);

			ret = usb_add_phy_dev(&tcc_ehci->uphy);
			if (ret != 0) {
				dev_err(dev, "[ERROR][USB] usb_add_phy_dev() FAIL!, ret: %d\n",
						ret);
#if 0
			} else {
#if defined(CONFIG_ENABLE_BC_20_HOST) || defined(CONFIG_ENABLE_BC_20_DRD)
				ret = tcc_ehci_phy_init_battery_charging(pdev,
						tcc_ehci);
#endif /* CONFIG_ENABLE_BC_20_HOST || CONFIG_ENABLE_BC_20_DRD */
				/* Do nothing, if BC 1.2 is not configured */
#endif
			}
		}
	}

	return ret;
}

static int32_t tcc_ehci_phy_remove(struct platform_device *pdev)
{
	struct tcc_ehci_phy *tcc_ehci = platform_get_drvdata(pdev);

	usb_remove_phy(&tcc_ehci->uphy);

	return 0;
}

static const struct of_device_id tcc_ehci_phy_match[] = {
	{ .compatible = "telechips,tcc_ehci_phy" },
	{},
};
MODULE_DEVICE_TABLE(of, tcc_ehci_phy_match);

static struct platform_driver tcc_ehci_phy_driver = {
	.probe			= tcc_ehci_phy_probe,
	.remove			= tcc_ehci_phy_remove,
	.driver = {
		.name		= "ehci_phy",
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(tcc_ehci_phy_match),
	},
};

static int32_t __init tcc_ehci_phy_drv_init(void)
{
	int32_t ret = 0;

	ret = platform_driver_register(&tcc_ehci_phy_driver);
	if (ret < 0) {
		pr_err("[ERROR][USB] %s() FAIL! ret: %d\n", __func__, ret);
	}

	return ret;
}
subsys_initcall_sync(tcc_ehci_phy_drv_init);

static void __exit tcc_ehci_phy_drv_cleanup(void)
{
	platform_driver_unregister(&tcc_ehci_phy_driver);
}
module_exit(tcc_ehci_phy_drv_cleanup);

MODULE_DESCRIPTION("Telechips EHCI USB transceiver driver");
MODULE_LICENSE("GPL v2");
