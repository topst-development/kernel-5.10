// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/bitfield.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/usb/otg.h>

#include "ehci.h"
#include "ehci-tcc.h"

struct pcfg_field old_pcfg_field[PCFG_MAX] = {
	{"TXVRT  "}, {"CDT    "}, {"TXPPT  "}, {"TP     "}, {"TXRT   "},
	{"TXREST "}, {"TXHSXVT"},
};

struct pcfg_field new_pcfg_field[PCFG_MAX] = {
	{"TXVRT  "}, {"CDT    "}, {"TXPPT  "}, {"TP     "}, {"TXRT   "},
	{"TXREST "}, {"TXHSXVT"},
};

int32_t ehci_phy_set = -1;
EXPORT_SYMBOL_GPL(ehci_phy_set);

#if defined(CONFIG_VBUS_CTRL_DEF_ENABLE)
static uint32_t ehci_vbus_control_enable = ENABLE;
#else /* CONFIG_VBUS_CTRL_DEF_ENABLE */
static uint32_t ehci_vbus_control_enable = DISABLE;
#endif /* !defined(CONFIG_VBUS_CTRL_DEF_ENABLE) */
module_param(ehci_vbus_control_enable, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(ehci_vbus_control_enable, "TCC EHCI VBus control enable");

static ssize_t ehci_vbus_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	const struct tcc_ehci_hcd *tcc_ehci = dev_get_drvdata(dev);

	return sprintf(buf, "TCC EHCI vbus: %s\n",
			(tcc_ehci->vbus_status == ON) ? "on" : "off");
}

static ssize_t ehci_vbus_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct tcc_ehci_hcd *tcc_ehci = dev_get_drvdata(dev);

	if (strncmp(buf, "on", 2) == 0) {
		(void)tcc_ehci_ctrl_vbus(tcc_ehci, ON);
	}

	if (strncmp(buf, "off", 3) == 0) {
		(void)tcc_ehci_ctrl_vbus(tcc_ehci, OFF);
	}

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(ehci_vbus);

static ssize_t ehci_testmode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	const struct tcc_ehci_hcd *tcc_ehci = dev_get_drvdata(dev);
	const struct ehci_hcd *ehci = tcc_ehci->ehci;
	u32 reg;

	if (ehci != NULL) {
		reg = ehci_readl(ehci, &ehci->regs->port_status[0]);
		reg &= (u32)0xFU << 16; // EHCI_PORTPMSC_TESTMODE_MASK
		reg >>= 16;

		switch (reg) {
		case 0:
			pr_info("no test\n");
			break;
		case USB_TEST_J:
			pr_info("test_j\n");
			break;
		case USB_TEST_K:
			pr_info("test_k\n");
			break;
		case USB_TEST_SE0_NAK:
			pr_info("test_se0_nak\n");
			break;
		case USB_TEST_PACKET:
			pr_info("test_packet\n");
			break;
		case USB_TEST_FORCE_ENABLE:
			pr_info("test_force_enable\n");
			break;
		default:
			pr_info("UNKNOWN test mode\n");
			break;
		}
	}

	return 0;
}

static ssize_t ehci_testmode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
#if 0
	const struct tcc_ehci_hcd *tcc_ehci = dev_get_drvdata(dev);
	struct ehci_hcd *ehci = tcc_ehci->ehci;
#endif
	int testmode = 0;

	if (strncmp(buf, "test_j", 6) == 0) {
		testmode = USB_TEST_J;
	} else if (strncmp(buf, "test_k", 6) == 0) {
		testmode = USB_TEST_K;
	} else if (strncmp(buf, "test_se0_nak", 12) == 0) {
		testmode = USB_TEST_SE0_NAK;
	} else if (strncmp(buf, "test_packet", 11) == 0) {
		testmode = USB_TEST_PACKET;
	} else if (strncmp(buf, "test_force_enable", 17) == 0) {
		testmode = USB_TEST_FORCE_ENABLE;
	} else {
		testmode = 0;
	}

#if 0
	(void)ehci_set_test_mode(ehci, testmode);
#endif

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(ehci_testmode);

static void tcc_ehci_display_pcfg(uint32_t old_reg, uint32_t new_reg)
{
	int32_t i;

	old_pcfg_field[0].value = FIELD_GET(GENMASK(3, 0), old_reg);
	old_pcfg_field[1].value = FIELD_GET(GENMASK(6, 4), old_reg);
	old_pcfg_field[2].value = FIELD_GET(BIT(7), old_reg);
	old_pcfg_field[3].value = FIELD_GET(GENMASK(9, 8), old_reg);
	old_pcfg_field[4].value = FIELD_GET(GENMASK(11, 10), old_reg);
	old_pcfg_field[5].value = FIELD_GET(GENMASK(13, 12), old_reg);
	old_pcfg_field[6].value = FIELD_GET(GENMASK(15, 14), old_reg);

	new_pcfg_field[0].value = FIELD_GET(GENMASK(3, 0), new_reg);
	new_pcfg_field[1].value = FIELD_GET(GENMASK(6, 4), new_reg);
	new_pcfg_field[2].value = FIELD_GET(BIT(7), new_reg);
	new_pcfg_field[3].value = FIELD_GET(GENMASK(9, 8), new_reg);
	new_pcfg_field[4].value = FIELD_GET(GENMASK(11, 10), new_reg);
	new_pcfg_field[5].value = FIELD_GET(GENMASK(13, 12), new_reg);
	new_pcfg_field[6].value = FIELD_GET(GENMASK(15, 14), new_reg);

	for (i = 0; i < PCFG_MAX; i++) {
		if (old_pcfg_field[i].value != new_pcfg_field[i].value) {
			pr_info("%s: 0x%X -> 0x%X\n", old_pcfg_field[i].name,
					old_pcfg_field[i].value,
					new_pcfg_field[i].value);
		} else {
			pr_info("%s: 0x%X\n", old_pcfg_field[i].name,
					old_pcfg_field[i].value);
		}
	}
}

static ssize_t ehci_pcfg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	const struct tcc_ehci_hcd *tcc_ehci = dev_get_drvdata(dev);
	uint32_t pcfg1_val = readl(&tcc_ehci->phy_regs->PCFG1);

	pr_info("USB20H_PCFG1: 0x%08X\n", pcfg1_val);
	tcc_ehci_display_pcfg(pcfg1_val, pcfg1_val);

	return 0;
}

static ssize_t ehci_pcfg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	const struct tcc_ehci_hcd *tcc_ehci = dev_get_drvdata(dev);
	ssize_t ret;
	uint32_t old_reg = readl(&tcc_ehci->phy_regs->PCFG1);
	uint32_t new_reg;

	if (count != 11U) {
		ret = -EINVAL;
	} else {
		ret = kstrtouint(buf, 0, &new_reg);
		if (ret == 0) {
			if ((buf[1] != 'x') && (buf[1] != 'X')) {
				ret = -EINVAL;
			}
		}
	}

	if (ret != 0) {
		pr_info("[INFO][USB] Input is NOT valid : %s\n", buf);
		pr_info("1) The length of the input must be 10.\n");
		pr_info("2) The input must start with 0x or 0X, which is the hex notation.\n");
		pr_info("3) Each digit of the input must be 0~9, a~f, or A~F.\n\n");
		pr_info("Usage Example\n");
		pr_info("1) echo 0x6789abcdef > ehci_pcfg\n");
		pr_info("2) echo 0X6789ABCDEF > ehci_pcfg\n\n");
	} else {
		ret = count;

		pr_info("[INFO][USB] Before\n");
		pr_info("USB20H_PCFG1: 0x%08X\n\n", old_reg);

		writel(new_reg, &tcc_ehci->phy_regs->PCFG1);
		new_reg = readl(&tcc_ehci->phy_regs->PCFG1);

		pr_info("[INFO][USB] After\n");
		pr_info("USB20H_PCFG1: 0x%08X\n", new_reg);
		tcc_ehci_display_pcfg(old_reg, new_reg);
	}

	return ret;
}

static DEVICE_ATTR_RW(ehci_pcfg);

static int32_t tcc_ehci_set_vbus_resource(struct usb_phy *uphy)
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
		tcc_ehci->vbus_supply =
			devm_regulator_get_optional(dev, "vbus");
		if (IS_ERR(tcc_ehci->vbus_supply)) {
			dev_err(dev, "[ERROR][USB] VBus Supply is not valid.\n");
			ret = PTR_ERR(tcc_ehci->vbus_supply);
		}
	} else {
		dev_info(dev, "[INFO][USB] vbus-ctrl-able property is not declared.\n");
		ret = -ENODEV;
	}

	return ret;
}

static int32_t tcc_ehci_parse_dt(struct platform_device *pdev,
		struct tcc_ehci_hcd *tcc_ehci)
{
	int32_t ret = 0;

	tcc_ehci->uphy = devm_usb_get_phy_by_phandle(&pdev->dev,
			"telechips,ehci_phy", 0);
	if (IS_ERR(tcc_ehci->uphy)) {
		if (PTR_ERR(tcc_ehci->uphy) == -EPROBE_DEFER) {
			ret = -EPROBE_DEFER;
		} else {
			ret = -ENODEV;
		}

		tcc_ehci->uphy = NULL;
	} else {
#if 0
		tcc_ehci->phy_regs = tcc_ehci->uphy->base;
#endif

		// Check TPL Support
		if (of_usb_host_tpl_support(pdev->dev.of_node)) {
			tcc_ehci->hcd_tpl_support = ENABLE;
		} else {
			tcc_ehci->hcd_tpl_support = DISABLE;
		}

		if (IS_ENABLED(CONFIG_ARCH_TCC897X) == 0) {
			ret = tcc_ehci_set_vbus_resource(tcc_ehci->uphy);
		}
	}

	return ret;
}

static int32_t tcc_ehci_set_state(struct usb_phy *uphy, int32_t on_off)
{
	struct tcc_ehci_phy *tcc_ehci =
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

static int32_t tcc_ehci_ctrl_phy(const struct tcc_ehci_hcd *tcc_ehci,
		int32_t on_off)
{
	struct usb_phy *uphy = tcc_ehci->uphy;
	int32_t ret = 0;

#if 0
	if ((uphy == NULL) || (uphy->set_state == NULL)) {
#else
	if (uphy == NULL) {
#endif
		pr_info("[INFO][USB] TCC EHCI PHY control failed!\n");
		pr_info("[INFO][USB] No TCC EHCI PHY driver or No set_state function\n");
		ret = -ENODEV;
	} else {
		ehci_phy_set = 1;
		ret = tcc_ehci_set_state(uphy, on_off);
	}

	return ret;
}

static int32_t tcc_ehci_init_phy(const struct tcc_ehci_hcd *tcc_ehci)
{
	struct usb_phy *uphy = tcc_ehci->uphy;
	int32_t ret = 0;

	if ((uphy == NULL) || (uphy->init == NULL)) {
		pr_info("[INFO][USB] TCC EHCI PHY init failed!\n");
		pr_info("[INFO][USB] No TCC EHCI PHY driver or No init function\n");
		ret = -ENODEV;
	} else {
		ret = uphy->init(uphy);
	}

	return ret;
}

static int32_t tcc_ehci_ctrl_vbus(struct tcc_ehci_hcd *tcc_ehci, int32_t on_off)
{
	struct usb_phy *uphy = tcc_ehci->uphy;
	int32_t ret = 0;

	if ((uphy == NULL) || (uphy->set_vbus == NULL)) {
		dev_err(tcc_ehci->dev, "[ERROR][USB] PHY driver does NOT exist!\n");
		ret = -ENODEV;
	} else {
		if (ehci_vbus_control_enable == DISABLE) {
			dev_info(tcc_ehci->dev, "[INFO][USB] VBus control is DISABLED!\n");
			ret = -ENOENT;
		} else {
			tcc_ehci->vbus_status = on_off;
			ret = uphy->set_vbus(uphy, on_off);
		}
	}

	return ret;
}

static void tcc_ehci_ctrl_phy_and_vbus(struct tcc_ehci_hcd *tcc_ehci,
		int32_t on_off)
{
	(void)tcc_ehci_ctrl_phy(tcc_ehci, on_off);
	(void)tcc_ehci_ctrl_vbus(tcc_ehci, on_off);
}

static int32_t tcc_ehci_probe(struct platform_device *pdev)
{
	struct tcc_ehci_hcd *tcc_ehci;
	struct usb_hcd *hcd;
	const struct resource *res;

	int32_t ret = 0;
	int32_t irq;

	if (usb_disabled() != 0) {
		ret = -ENODEV;
		goto err0;
	}

	ret = dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (ret != 0) {
		goto err0;
	}

	tcc_ehci = devm_kzalloc(&pdev->dev, sizeof(struct tcc_ehci_hcd),
			GFP_KERNEL);
	if (tcc_ehci == NULL) {
		ret = -ENOMEM;
		goto err0;
	}

	/* Parsing the device table */
	ret = tcc_ehci_parse_dt(pdev, tcc_ehci);
	if (ret != 0) {
		/*
		 * If the return value of tcc_ehci_parse_dt() is -EPROBE_DEFER,
		 * the VBus GPIO number is set to an invalid number in
		 * set_vbus_resource() called in tcc_ehci_parse_dt(). In such a
		 * case, set the value of the ehci_phy_set variable to
		 * -EPROBE_DEFER and tcc_ohci_probe() in ohci-tcc.c check the
		 * value of the ehci_phy_set variable. If the value of the
		 * ehci_phy_set variable is -EPROBE_DEFER, the tcc_ohci_probe()
		 * also fails.
		 */
		if (ret == -EPROBE_DEFER) {
			ehci_phy_set = -EPROBE_DEFER;

			goto end;
		} else {
			dev_err(&pdev->dev, "[ERROR][USB] Device table parsing failed.\n");
			ret = -EIO;
		}

		goto err0;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		dev_err(&pdev->dev, "[ERROR][USB] Found HC with no IRQ. Check %s setup!\n",
				dev_name(&pdev->dev));
		ret = -ENODEV;
		goto err0;
	}

	hcd = usb_create_hcd(&tcc_ehci_hc_driver, &pdev->dev,
			dev_name(&pdev->dev));
	if (hcd == NULL) {
		ret = -ENOMEM;
		goto err0;
	}

	platform_set_drvdata(pdev, tcc_ehci);
	tcc_ehci->dev = &(pdev->dev);

	/* USB ECHI Base Address*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "[ERROR][USB] Found HC with no register addr. Check %s setup!\n",
				dev_name(&pdev->dev));
		ret = -ENODEV;
		goto err1;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);
	hcd->regs = devm_ioremap(&pdev->dev, res->start, hcd->rsrc_len);

	/* USB HS Phy Enable */
	tcc_ehci_ctrl_phy_and_vbus(tcc_ehci, ON);
	(void)tcc_ehci_init_phy(tcc_ehci);

	/* ehci setup */
	tcc_ehci->ehci = hcd_to_ehci(hcd);

	/* registers start at offset 0x0 */
	tcc_ehci->ehci->caps = hcd->regs;
	tcc_ehci->ehci->regs = hcd->regs + HC_LENGTH(tcc_ehci->ehci,
			ehci_readl(tcc_ehci->ehci,
				&tcc_ehci->ehci->caps->hc_capbase));

	/* cache this readonly data; minimize chip reads */
	tcc_ehci->ehci->hcs_params = ehci_readl(tcc_ehci->ehci,
			&tcc_ehci->ehci->caps->hcs_params);

	/* connect the hcd phy pointer */
	hcd->usb_phy = tcc_ehci->uphy;

	/* TPL Support Set */
	hcd->tpl_support = tcc_ehci->hcd_tpl_support;

	ret = usb_add_hcd(hcd, (unsigned int)irq, IRQF_SHARED);
	if (ret != 0) {
		dev_err(&pdev->dev, "[ERROR][USB] Failed to add USB HCD\n");
		goto err1;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_ehci_vbus);
	if (ret < 0) {
		pr_err("[ERROR][USB] Cannot create ehci_vbus sysfs : %d\n", ret);
		goto err1;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_ehci_testmode);
	if (ret < 0) {
		pr_err("[ERROR][USB] Cannot create SQ ehci_testmode sysfs : %d\n",
				ret);
		goto err1;
	}

	ret = device_create_file(&pdev->dev, &dev_attr_ehci_pcfg);
	if (ret < 0) {
		pr_err("[ERROR][USB] Cannot create SQ ehci_pcfg sysfs : %d\n",
				ret);
		goto err1;
	}

	goto end;
err1:
	usb_put_hcd(hcd);
err0:
	dev_err(&pdev->dev, "[ERROR][USB] init %s fail, %d\n",
			dev_name(&pdev->dev), ret);
end:
	return ret;
}

static int32_t tcc_ehci_remove(struct platform_device *pdev)
{
	struct tcc_ehci_hcd *tcc_ehci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = ehci_to_hcd(tcc_ehci->ehci);

	device_remove_file(&pdev->dev, &dev_attr_ehci_vbus);
	device_remove_file(&pdev->dev, &dev_attr_ehci_testmode);
	device_remove_file(&pdev->dev, &dev_attr_ehci_pcfg);

#if 0
	ehci_shutdown(hcd);
#endif
	usb_remove_hcd(hcd);
	usb_put_hcd(hcd);

	tcc_ehci_ctrl_phy_and_vbus(tcc_ehci, OFF);

	return 0;
}

static void tcc_ehci_hcd_shutdown(struct platform_device *pdev)
{
	struct tcc_ehci_hcd *tcc_ehci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = ehci_to_hcd(tcc_ehci->ehci);

	(void)tcc_ehci_ctrl_vbus(tcc_ehci, OFF);

	if (hcd != NULL) {
		if (hcd->driver->shutdown != NULL) {
			hcd->driver->shutdown(hcd);
		}
	}
}

static int32_t tcc_ehci_suspend(struct device *dev)
{
	struct tcc_ehci_hcd *tcc_ehci =	dev_get_drvdata(dev);
	struct usb_hcd *hcd = ehci_to_hcd(tcc_ehci->ehci);
	bool do_wakeup = device_may_wakeup(dev);

	(void)ehci_suspend(hcd, do_wakeup);

	/* Telechips specific routine */
	tcc_ehci_ctrl_phy_and_vbus(tcc_ehci, OFF);

	return 0;
}

static int32_t tcc_ehci_resume(struct device *dev)
{
	struct tcc_ehci_hcd *tcc_ehci =	dev_get_drvdata(dev);
	struct usb_hcd *hcd = ehci_to_hcd(tcc_ehci->ehci);

	/* Telechips specific routine */
	tcc_ehci_ctrl_phy_and_vbus(tcc_ehci, ON);
	(void)tcc_ehci_init_phy(tcc_ehci);

	/*
	 * for compatibility issue(suspend/resume). Some USB devices are failed
	 * to connect when resume.
	 */
	usleep_range(1000, 2000);
	(void)ehci_resume(hcd, (bool)false);

	return 0;
}

static SIMPLE_DEV_PM_OPS(tcc_ehci_pm_ops, tcc_ehci_suspend, tcc_ehci_resume);

static const struct of_device_id tcc_ehci_match[] = {
	{ .compatible = "telechips,tcc-ehci" },
	{},
};
MODULE_DEVICE_TABLE(of, tcc_ehci_match);

static struct platform_driver tcc_ehci_driver = {
	.probe			= tcc_ehci_probe,
	.remove			= tcc_ehci_remove,
	.shutdown		= tcc_ehci_hcd_shutdown,
	.driver = {
		.name		= "tcc-ehci",
		.pm		= &tcc_ehci_pm_ops,
		.of_match_table = of_match_ptr(tcc_ehci_match),
	}
};

static int32_t __init tcc_ehci_init(void)
{
	int32_t ret = 0;

	ehci_init_driver(&tcc_ehci_hc_driver, NULL);
	set_bit(USB_EHCI_LOADED, &usb_hcds_loaded);

	ret = platform_driver_register(&tcc_ehci_driver);
	if (ret < 0) {
		clear_bit(USB_EHCI_LOADED, &usb_hcds_loaded);
	}

	return ret;
}
module_init(tcc_ehci_init);

static void __exit tcc_ehci_cleanup(void)
{
	platform_driver_unregister(&tcc_ehci_driver);
	clear_bit(USB_EHCI_LOADED, &usb_hcds_loaded);
}
module_exit(tcc_ehci_cleanup);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
