// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>

#include "ohci.h"
#include "ohci-tcc.h"

static void tcc_ohci_parse_dt(const struct platform_device *pdev,
		struct tcc_ohci_hcd *tcc_ohci)
{
	// Check TPL Support
	if (of_usb_host_tpl_support(pdev->dev.of_node)) {
		tcc_ohci->hcd_tpl_support = ENABLE;
	} else {
		tcc_ohci->hcd_tpl_support = DISABLE;
	}
}

static void tcc_ohci_ctrl_phy(const struct tcc_ohci_hcd *tcc_ohci, int32_t on_off)
{
	uint32_t val;

	if (tcc_ohci != NULL) {
		if (on_off == ON) {
			if (ehci_phy_set == 0) {
				pr_info("[INFO][USB] ehci load first!\n");
			}
		}

		if (on_off == OFF) {
			val = readl(&tcc_ohci->phy_regs->PCFG0);
			writel(val | BIT(8), &tcc_ohci->phy_regs->PCFG0);
			val = readl(&tcc_ohci->phy_regs->PCFG1);
			writel(val | BIT(28), &tcc_ohci->phy_regs->PCFG1);
		}
	}
}

static int32_t tcc_ohci_probe(struct platform_device *pdev)
{
	struct tcc_ohci_hcd *tcc_ohci;
	struct usb_hcd *hcd;
	const struct resource *res0, *res1;
	struct ohci_hcd *ohci;

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

	tcc_ohci = devm_kzalloc(&pdev->dev, sizeof(struct tcc_ohci_hcd),
			GFP_KERNEL);
	if (tcc_ohci == NULL) {
		ret = -ENOMEM;
		goto err0;
	}

	/* Parsing the device table */
	tcc_ohci_parse_dt(pdev, tcc_ohci);

	/*
	 * If the value of the ehci_phy_set variable is -EPROBE_DEFER,
	 * tcc_ehci_probe() in ehci-tcc.c failed because the VBus GPIO pin
	 * number is set to an invalid number. In such a case, the
	 * tcc_ohci_probe() in this ohci-tcc.c also fails.
	 */
	if (ehci_phy_set == -EPROBE_DEFER) {
		pr_info("[INFO][USB] ehci-tcc and ohci-tcc driver requests the probe retry\n");
		ret = -EPROBE_DEFER;

		goto end;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq <= 0) {
		dev_err(&pdev->dev, "[ERROR][USB] Found HC with no IRQ. Check %s setup!\n",
				dev_name(&pdev->dev));
		ret = -ENODEV;
		goto err0;
	}

	hcd = usb_create_hcd(&tcc_ohci_hc_driver, &pdev->dev,
			dev_name(&pdev->dev));
	if (hcd == NULL) {
		ret = -ENOMEM;
		goto err0;
	}

	ohci = hcd_to_ohci(hcd);

	/* USB OHCI Base Address*/
	res0 = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res0 == NULL) {
		dev_err(&pdev->dev, "[ERROR][USB] Found HC with no register addr. Check %s setup!\n",
				dev_name(&pdev->dev));
		ret = -ENODEV;
		goto err1;
	}

	hcd->rsrc_start = res0->start;
	hcd->rsrc_len = resource_size(res0);
	hcd->regs = devm_ioremap(&pdev->dev, res0->start, hcd->rsrc_len);

	/* USB PHY (UTMI) Base Address*/
	res1 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res1 == NULL) {
		dev_err(&pdev->dev, "[ERROR][USB] Found HC with no register addr. Check %s setup!\n",
				dev_name(&pdev->dev));
		ret = -ENODEV;
		goto err1;
	}

	tcc_ohci->hcd = hcd;

	tcc_ohci->phy_rsrc_start = res1->start;
	tcc_ohci->phy_rsrc_len = resource_size(res1);
	tcc_ohci->phy_regs = devm_ioremap(&pdev->dev, res1->start,
			tcc_ohci->phy_rsrc_len);

	platform_set_drvdata(pdev, tcc_ohci);
	tcc_ohci->dev = &(pdev->dev);
	pdev->dev.platform_data = dev_get_platdata(&pdev->dev);

	/* TPL Support Set */
	hcd->tpl_support = tcc_ohci->hcd_tpl_support;

	tcc_ohci_ctrl_phy(tcc_ohci, ON);

	ohci->next_statechange = jiffies;
	spin_lock_init(&ohci->lock);
	INIT_LIST_HEAD(&ohci->pending);
	INIT_LIST_HEAD(&ohci->eds_in_use);

	ret = usb_add_hcd(hcd, (unsigned int)irq, IRQF_SHARED);
	if (ret != 0) {
		dev_err(&pdev->dev, "[ERROR][USB] Failed to add USB HCD\n");
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

static int32_t tcc_ohci_remove(struct platform_device *pdev)
{
	const struct tcc_ohci_hcd *tcc_ohci = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = tcc_ohci->hcd;
	int32_t ret = -ENODEV;

	if (hcd != NULL) {
		usb_remove_hcd(hcd);
		release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
		usb_put_hcd(hcd);
		platform_set_drvdata(pdev, NULL);
		ret = 0;
	}

	return ret;
}

static const struct of_device_id tcc_ohci_match[] = {
	{ .compatible = "telechips,tcc-ohci" },
	{},
};
MODULE_DEVICE_TABLE(of, tcc_ohci_match);

static struct platform_driver tcc_ohci_driver = {
	.probe			= tcc_ohci_probe,
	.remove			= tcc_ohci_remove,
	.driver	= {
		.name		= "tcc-ohci",
		.of_match_table	= of_match_ptr(tcc_ohci_match),
	}
};

static int32_t __init tcc_ohci_init(void)
{
	int32_t ret = 0;

	ohci_init_driver(&tcc_ohci_hc_driver, NULL);
	set_bit(USB_EHCI_LOADED, &usb_hcds_loaded);

	ret = platform_driver_register(&tcc_ohci_driver);
	if (ret < 0) {
		clear_bit(USB_EHCI_LOADED, &usb_hcds_loaded);
	}

	return ret;
}
module_init(tcc_ohci_init);

static void __exit tcc_ohci_cleanup(void)
{
	platform_driver_unregister(&tcc_ohci_driver);
	clear_bit(USB_EHCI_LOADED, &usb_hcds_loaded);
}
module_exit(tcc_ohci_cleanup);

MODULE_ALIAS("platform:tcc-ohci");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
