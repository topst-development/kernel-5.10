// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/usb/phy.h>
#include <linux/usb/otg.h>

#include "phy-tcc-dwc_otg.h"

static int32_t tcc_dwc_otg_phy_create(struct device *dev,
		struct tcc_dwc_otg_phy *tcc_dwc_otg)
{
	int32_t ret = -ENODEV;

	if ((dev != NULL) && (tcc_dwc_otg != NULL)) {
		tcc_dwc_otg->uphy.otg = devm_kzalloc(dev,
				sizeof(*tcc_dwc_otg->uphy.otg), GFP_KERNEL);
		if (tcc_dwc_otg->uphy.otg == NULL) {
			ret = -ENOMEM;
		} else {
			tcc_dwc_otg->dev = dev;

			tcc_dwc_otg->uphy.dev = tcc_dwc_otg->dev;
			tcc_dwc_otg->uphy.label = "tcc_dwc_otg_phy";
			tcc_dwc_otg->uphy.type = USB_PHY_TYPE_USB2;

			tcc_dwc_otg->uphy.init = tcc_dwc_otg_phy_init;
			tcc_dwc_otg->uphy.set_vbus = tcc_dwc_otg_phy_set_vbus;

#if 0
			tcc_dwc_otg->uphy.get_base = tcc_dwc_otg_phy_get_base;
			tcc_dwc_otg->uphy.set_vbus_resource =
				tcc_dwc_otg_phy_set_vbus_resource;

#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
			tcc_dwc_otg->uphy.get_dc_level =
				tcc_dwc_otg_phy_get_dc_level;
			tcc_dwc_otg->uphy.set_dc_level =
				tcc_dwc_otg_phy_set_dc_level;
#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */
#endif

			if (of_find_property(dev->of_node, "mux", NULL) !=
					NULL) {
				tcc_dwc_otg->is_mux = TRUE;
			} else {
				tcc_dwc_otg->is_mux = FALSE;
			}

			tcc_dwc_otg->uphy.otg->usb_phy = &tcc_dwc_otg->uphy;

			ret = 0;

#if 0
			if (IS_ENABLED(CONFIG_ARCH_TCC897X) != 0) {
				ret = tcc_dwc_otg_phy_set_vbus_resource(
						&tcc_dwc_otg->uphy);
			}
#endif
		}
	}

	return ret;
}

static int32_t tcc_dwc_otg_phy_init(struct usb_phy *uphy)
{
	const struct tcc_dwc_otg_phy *tcc_dwc_otg =
		container_of(uphy, struct tcc_dwc_otg_phy, uphy);
	int32_t i = 0;
	uint32_t val;

	dev_info(tcc_dwc_otg->dev, "[INFO][USB] %s() call SUCCESS\n", __func__);

#if defined(CONFIG_USB_DWC2_TCC_MUX)
	writel(0x0, &tcc_dwc_otg->phy_regs->MUX_SEL);
	udelay(10);

	val = readl(&tcc_dwc_otg->phy_regs->MUX_SEL);
	writel(val | (SEL | MUX_HST_SEL | MUX_DEV_SEL),
			&tcc_dwc_otg->phy_regs->MUX_SEL);
#endif /* CONFIG_USB_DWC2_TCC_MUX */

	if (IS_ENABLED(CONFIG_ARCH_TCC897X) != 0) {
		writel(0x83000015, &tcc_dwc_otg->phy_regs->PCFG0);
	} else {
		writel(PCFG0_RST, &tcc_dwc_otg->phy_regs->PCFG0);
	}

	if (IS_ENABLED(CONFIG_ARCH_TCC897X) != 0) {
		writel(0x0330D643, &tcc_dwc_otg->phy_regs->PCFG1);
	} else {
		writel(0xE31C243A, &tcc_dwc_otg->phy_regs->PCFG1);
	}

#if defined(CONFIG_TCC_EH_ELECT_TST)
	writel((ACAENB | BIT(2)), &tcc_dwc_otg->phy_regs->PCFG2);
#else
	if (IS_ENABLED(CONFIG_ARCH_TCC897X) != 0) {
		writel(BIT(2), &tcc_dwc_otg->phy_regs->PCFG2);
	} else {
		writel(0x75852004, &tcc_dwc_otg->phy_regs->PCFG2);
	}
#endif /* CONFIG_TCC_EH_ELECT_TST */

	writel(VDATREFTUNE0 | RETENABLEN, &tcc_dwc_otg->phy_regs->PCFG3);
	writel(0x0, &tcc_dwc_otg->phy_regs->PCFG4);

	if (IS_ENABLED(CONFIG_ARCH_TCC897X) != 0) {
		writel(PRSTN | BIT(28), &tcc_dwc_otg->phy_regs->LCFG0);
	} else {
		writel(PRSTN, &tcc_dwc_otg->phy_regs->LCFG0);
	}

	if (IS_ENABLED(CONFIG_ARCH_TCC897X) == 0) {
		// Set the POR
		val = readl(&tcc_dwc_otg->phy_regs->PCFG0);
		writel(val | PHY_POR, &tcc_dwc_otg->phy_regs->PCFG0);
	}

	// Set the Core Reset
	val = readl(&tcc_dwc_otg->phy_regs->LCFG0);
	writel(val & ~PRSTN, &tcc_dwc_otg->phy_regs->LCFG0);

	if (IS_ENABLED(CONFIG_ARCH_TCC897X) != 0) {
		val = readl(&tcc_dwc_otg->phy_regs->LCFG0);
		writel(val & ~BIT(28), &tcc_dwc_otg->phy_regs->LCFG0);
#if !defined(CONFIG_TCC_EH_ELECT_TST)
		val = readl(&tcc_dwc_otg->phy_regs->LCFG0);
		writel(val | (BIT(22) | BIT(21)),
				&tcc_dwc_otg->phy_regs->LCFG0);
#endif
	}
	udelay(30);

	// Release POR
	val = readl(&tcc_dwc_otg->phy_regs->PCFG0);
	writel(val & ~PHY_POR, &tcc_dwc_otg->phy_regs->PCFG0);
	// Clear SIDDQ
	val = readl(&tcc_dwc_otg->phy_regs->PCFG0);
	writel(val & ~SIDDQ, &tcc_dwc_otg->phy_regs->PCFG0);

	if (IS_ENABLED(CONFIG_ARCH_TCC897X) == 0) {
		// Set Phyvalid en
		val = readl(&tcc_dwc_otg->phy_regs->PCFG0);
		writel(val | IRQ_PHYVLDEN, &tcc_dwc_otg->phy_regs->PCFG0);
		// Set DP/DM (pull down)
		val = readl(&tcc_dwc_otg->phy_regs->PCFG4);
		writel(val | (DPPD | DMPD), &tcc_dwc_otg->phy_regs->PCFG4);

		// Wait Phy Valid Interrupt
		while (i < 10000) {
			val = readl(&tcc_dwc_otg->phy_regs->PCFG4);
			if ((val & IRQ_PHYVALID) != 0U) {
				break;
			}

			i++;
			udelay(5);
		}

		dev_info(tcc_dwc_otg->dev, "[INFO][USB] DWC OTG PHY valid check %s\n",
				(i >= 9999) ? "FAIL!" : "SUCCESS");
	}

	// disable PHYVALID_EN -> no irq
	val = readl(&tcc_dwc_otg->phy_regs->PCFG0);
	writel(val & ~IRQ_PHYVLDEN, &tcc_dwc_otg->phy_regs->PCFG0);
	val = readl(&tcc_dwc_otg->phy_regs->PCFG0);
	writel(val & ~BIT(25), &tcc_dwc_otg->phy_regs->PCFG0);

	if (IS_ENABLED(CONFIG_ARCH_TCC897X) != 0) {
		usleep_range(10000, 20000);
	}

	// Release Core Reset
	val = readl(&tcc_dwc_otg->phy_regs->LCFG0);
	writel(val | PRSTN, &tcc_dwc_otg->phy_regs->LCFG0);

#if 0
#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
	(void)tcc_dwc_otg_phy_set_dc_level(uphy, CONFIG_USB_HS_DC_VOLTAGE_LEVEL);
#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */
#endif

	return 0;
}

static int32_t tcc_dwc_otg_phy_set_vbus(struct usb_phy *uphy, int32_t on_off)
{
	struct tcc_dwc_otg_phy *tcc_dwc_otg =
		container_of(uphy, struct tcc_dwc_otg_phy, uphy);
	const struct device *dev = tcc_dwc_otg->dev;
	int32_t ret = 0;

	if ((on_off == ON) && (tcc_dwc_otg->vbus_enabled == FALSE)) {
		ret = regulator_enable(tcc_dwc_otg->vbus_supply);
		if (ret == 0) {
			ret = regulator_set_voltage(tcc_dwc_otg->vbus_supply,
					ON_VOLTAGE, ON_VOLTAGE);
			if (ret == 0) {
				tcc_dwc_otg->vbus_enabled = TRUE;
				usleep_range(3000, 5000);
			} else {
				dev_err(dev, "[ERROR][USB] VBus on is FAILED!\n");
				(void)regulator_disable(
						tcc_dwc_otg->vbus_supply);
			}
		} else {
			dev_err(dev, "[ERROR][USB] Regulator enable is FAILED!\n");
		}
	} else if ((on_off == OFF) && (tcc_dwc_otg->vbus_enabled == TRUE)) {
		ret = regulator_set_voltage(tcc_dwc_otg->vbus_supply,
				OFF_VOLTAGE, OFF_VOLTAGE);
		if (ret == 0) {
			ret = regulator_disable(tcc_dwc_otg->vbus_supply);
			if (ret == 0) {
				tcc_dwc_otg->vbus_enabled = FALSE;
				usleep_range(3000, 5000);
			} else {
				dev_err(dev, "[ERROR][USB] Regulator disable is FAILED!\n");
			}
		} else {
			dev_err(dev, "[ERROR][USB] VBus off is FAILED!\n");
			(void)regulator_disable(tcc_dwc_otg->vbus_supply);
		}
	} else if ((on_off == ON) && (tcc_dwc_otg->vbus_enabled == TRUE)) {
		dev_info(dev, "[INFO][USB] Vbus is already ENABLED!\n");
	} else if ((on_off == OFF) && (tcc_dwc_otg->vbus_enabled == FALSE)) {
		dev_info(dev, "[INFO][USB] Vbus is already DISABLED!\n");
	} else {
		/* Nothing to do */
	}

	return ret;
}

#if 0
static void __iomem *tcc_dwc_otg_phy_get_base(struct usb_phy *uphy)
{
	const struct tcc_dwc_otg_phy *tcc_dwc_otg =
		container_of(uphy, struct tcc_dwc_otg_phy, uphy);

	return tcc_dwc_otg->phy_regs;
}

static int32_t tcc_dwc_otg_phy_set_vbus_resource(struct usb_phy *uphy)
{
	struct tcc_dwc_otg_phy *tcc_dwc_otg =
		container_of(uphy, struct tcc_dwc_otg_phy, uphy);
	struct device *dev = tcc_dwc_otg->dev;
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
		if (tcc_dwc_otg->vbus_supply == NULL) {
			tcc_dwc_otg->vbus_supply =
				devm_regulator_get_optional(dev, "vbus");
			if (IS_ERR(tcc_dwc_otg->vbus_supply)) {
				dev_err(dev, "[ERROR][USB] VBus Supply is not valid.\n");
				ret = -ENODEV;
			}
		}
	} else {
		dev_info(dev, "[INFO][USB] vbus-ctrl-able property is not declared.\n");
		ret = -ENODEV;
	}

	return ret;
}

#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
static uint32_t tcc_dwc_otg_phy_get_dc_level(struct usb_phy *uphy)
{
	const struct tcc_dwc_otg_phy *tcc_dwc_otg =
		container_of(uphy, struct tcc_dwc_otg_phy, uphy);
	uint32_t pcfg1_val;

	pcfg1_val = readl(&tcc_dwc_otg->phy_regs->PCFG1);

	return BIT_MSK(pcfg1_val, PCFG_TXVRT);
}

static int32_t tcc_dwc_otg_phy_set_dc_level(struct usb_phy *uphy, uint32_t level)
{
	const struct tcc_dwc_otg_phy *tcc_dwc_otg =
		container_of(uphy, struct tcc_dwc_otg_phy, uphy);
	uint32_t pcfg1_val;

	pcfg1_val = readl(&tcc_dwc_otg->phy_regs->PCFG1);
	BIT_CLR_SET(pcfg1_val, PCFG_TXVRT, level);
	writel(pcfg1_val, &tcc_dwc_otg->phy_regs->PCFG1);

	dev_info(tcc_dwc_otg->dev, "[INFO][USB] current DC voltage level: %d\n",
			BIT_MSK(pcfg1_val, PCFG_TXVRT));

	return 0;
}
#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */
#endif

static int32_t tcc_dwc_otg_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tcc_dwc_otg_phy *tcc_dwc_otg;
	int32_t ret;

	dev_info(dev, "[INFO][USB] %s() call SUCCESS\n", __func__);

	tcc_dwc_otg = devm_kzalloc(dev, sizeof(*tcc_dwc_otg), GFP_KERNEL);

	ret = tcc_dwc_otg_phy_create(dev, tcc_dwc_otg);
	if (ret != 0) {
		dev_err(dev, "[ERROR][USB] tcc_dwc_otg_phy_create() FAIL!\n");
	} else {
		if (request_mem_region(pdev->resource[0].start,
					resource_size(&pdev->resource[0]),
					"dwc_otg_phy") == NULL) {
			dev_dbg(dev, "[DEBUG][USB] error reserving mapped memory\n");
			ret = -EFAULT;
		} else {
			tcc_dwc_otg->phy_regs = ioremap((resource_size_t)
					pdev->resource[0].start,
					resource_size(&pdev->resource[0]));
#if 0
			tcc_dwc_otg->uphy.base = tcc_dwc_otg->phy_regs;
#endif

			platform_set_drvdata(pdev, tcc_dwc_otg);

			ret = usb_add_phy_dev(&tcc_dwc_otg->uphy);
			if (ret != 0) {
				dev_err(dev, "[ERROR][USB] usb_add_phy_dev() FAIL!, ret: %d\n",
						ret);
			}
		}
	}

	return ret;
}

static int32_t tcc_dwc_otg_phy_remove(struct platform_device *pdev)
{
	struct tcc_dwc_otg_phy *tcc_dwc_otg = platform_get_drvdata(pdev);

	usb_remove_phy(&tcc_dwc_otg->uphy);

	return 0;
}

static const struct of_device_id tcc_dwc_otg_phy_match[] = {
	{ .compatible = "telechips,tcc_dwc_otg_phy" },
	{},
};
MODULE_DEVICE_TABLE(of, tcc_dwc_otg_phy_match);

static struct platform_driver tcc_dwc_otg_phy_driver = {
	.probe			= tcc_dwc_otg_phy_probe,
	.remove			= tcc_dwc_otg_phy_remove,
	.driver = {
		.name		= "dwc_otg_phy",
		.owner		= THIS_MODULE,
		.of_match_table = of_match_ptr(tcc_dwc_otg_phy_match),
	},
};

static int32_t __init tcc_dwc_otg_phy_drv_init(void)
{
	int32_t ret = 0;

	ret = platform_driver_register(&tcc_dwc_otg_phy_driver);
	if (ret < 0) {
		pr_err("[ERROR][USB] %s() FAIL! ret: %d\n", __func__, ret);
	}

	return ret;
}
subsys_initcall_sync(tcc_dwc_otg_phy_drv_init);

static void __exit tcc_dwc_otg_phy_drv_cleanup(void)
{
	platform_driver_unregister(&tcc_dwc_otg_phy_driver);
}
module_exit(tcc_dwc_otg_phy_drv_cleanup);

MODULE_DESCRIPTION("Telechips DWC OTG USB transceiver driver");
MODULE_LICENSE("GPL v2");
