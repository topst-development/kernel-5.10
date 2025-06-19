// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/bitfield.h>
#include <linux/module.h>
#include <linux/of_platform.h>

#include "core.h"
#include "dwc3-tcc.h"

struct pcfg_field old_fpcfg_field[PCFG_MAX] = {
        {"TXVRT  "}, {"CDT    "}, {"TXPPT  "}, {"TP     "}, {"TXRT   "},
        {"TXREST "}, {"TXHSXVT"},
};

struct pcfg_field new_fpcfg_field[PCFG_MAX] = {
        {"TXVRT  "}, {"CDT    "}, {"TXPPT  "}, {"TP     "}, {"TXRT   "},
        {"TXREST "}, {"TXHSXVT"},
};

#if defined(CONFIG_VBUS_CTRL_DEF_ENABLE)
static uint32_t dwc3_vbus_control_enable = ENABLE;
#else /* CONFIG_VBUS_CTRL_DEF_ENABLE */
static uint32_t dwc3_vbus_control_enable = DISABLE;
#endif /* !defined(CONFIG_VBUS_CTRL_DEF_ENABLE) */
module_param(dwc3_vbus_control_enable, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dwc3_vbus_control_enable, "TCC DWC3 VBus control enable");

static void dwc3_tcc_vbus_ctrl(struct dwc3_tcc_hcd *dwc3_tcc,
		int32_t on_off)
{
	struct usb_phy *uphy;

	if (dwc3_tcc != NULL) {
		uphy = dwc3_tcc->uphy;
		if (uphy == NULL) {
			dev_info(dwc3_tcc->dev, "[INFO][USB] DWC3 PHY driver does NOT exist!\n");
		} else {
			if (dwc3_vbus_control_enable == DISABLE) {
				dev_info(dwc3_tcc->dev, "[INFO][USB] DWC3 VBus control is DISABLED!\n");
			} else {
				dwc3_tcc->vbus_status = on_off;
				(void)uphy->set_vbus(uphy, on_off);
			}
		}
	}
}

static void dwc3_tcc_vbus_ctrl_on(struct dwc3_tcc_hcd *dwc3_tcc)
{
	dwc3_tcc_vbus_ctrl(dwc3_tcc, ON);
}

static void dwc3_tcc_vbus_ctrl_off(struct dwc3_tcc_hcd *dwc3_tcc)
{
	dwc3_tcc_vbus_ctrl(dwc3_tcc, OFF);
}

static ssize_t dwc3_vbus_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	const struct dwc3_tcc_hcd *dwc3_tcc = dev_get_drvdata(dev);

	return sprintf(buf, "TCC DWC3 vbus: %s\n",
			(dwc3_tcc->vbus_status == ON) ? "on" : "off");
}

static ssize_t dwc3_vbus_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct dwc3_tcc_hcd *dwc3_tcc = dev_get_drvdata(dev);

	if (strncmp(buf, "on", 2) == 0) {
		dwc3_tcc_vbus_ctrl_on(dwc3_tcc);
	}

	if (strncmp(buf, "off", 3) == 0) {
		dwc3_tcc_vbus_ctrl_off(dwc3_tcc);
	}

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(dwc3_vbus);

static ssize_t dwc3_drdmode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc3 *dwc = dev_get_drvdata(dev);
	unsigned long flags;
	u32 reg;
	char *mode;

	spin_lock_irqsave(&dwc->lock, flags);
	reg = readl(dwc->regs + DWC3_GCTL - DWC3_GLOBALS_REGS_START);
	spin_unlock_irqrestore(&dwc->lock, flags);

	switch (DWC3_GCTL_PRTCAP(reg)) {
	case DWC3_GCTL_PRTCAP_HOST:
		mode = "host";
		break;
	case DWC3_GCTL_PRTCAP_DEVICE:
		mode = "device";
		break;
	default:
		mode = "UNKNOWN";
		break;
	}

	return sprintf(buf, "dwc3_drdmode: %s\n", mode);
}

static ssize_t dwc3_drdmode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct dwc3 *dwc = dev_get_drvdata(dev);
	unsigned long flags;

	if (strncmp(buf, "host", 4) == 0) {
		spin_lock_irqsave(&dwc->lock, flags);
		dwc->desired_dr_role = DWC3_GCTL_PRTCAP_HOST;
		spin_unlock_irqrestore(&dwc->lock, flags);

		queue_work(system_freezable_wq, &dwc->drd_work);
	} else if (strncmp(buf, "device", 6) == 0) {
		spin_lock_irqsave(&dwc->lock, flags);
		dwc->desired_dr_role = DWC3_GCTL_PRTCAP_DEVICE;
		spin_unlock_irqrestore(&dwc->lock, flags);

		queue_work(system_freezable_wq, &dwc->drd_work);
	} else {
		dev_warn(dwc->dev, "[WARN][USB] Value is invalid!\n");
	}

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(dwc3_drdmode);

static void dwc3_tcc_set_test_mode(struct dwc3 *dwc, u32 mode)
{
	u32 reg;

	reg = readl(dwc->regs + DWC3_DCTL - DWC3_GLOBALS_REGS_START);
	reg &= ~DWC3_DCTL_TSTCTRL_MASK;

	switch (mode) {
	case USB_TEST_J:
	case USB_TEST_K:
	case USB_TEST_SE0_NAK:
	case USB_TEST_PACKET:
	case USB_TEST_FORCE_ENABLE:
		reg |= mode << 1;
		break;
	default:
		pr_info("UNKNOWN testmode\n");
	}

	writel(reg, dwc->regs + DWC3_DCTL - DWC3_GLOBALS_REGS_START);
}

static ssize_t dwc3_testmode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc3 *dwc = dev_get_drvdata(dev);
	unsigned long flags;
	u32 reg;

	spin_lock_irqsave(&dwc->lock, flags);
	reg = readl(dwc->regs + DWC3_DCTL - DWC3_GLOBALS_REGS_START);
	reg &= DWC3_DCTL_TSTCTRL_MASK;
	reg >>= 1;
	spin_unlock_irqrestore(&dwc->lock, flags);

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
			pr_info("UNKNOWN testmode\n");
	}

	return 0;
}

static ssize_t dwc3_testmode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct dwc3 *dwc = dev_get_drvdata(dev);
	unsigned long flags;
        u32 testmode;

        if (!strncmp(buf, "test_j", 6))
                testmode = USB_TEST_J;
        else if (!strncmp(buf, "test_k", 6))
                testmode = USB_TEST_K;
        else if (!strncmp(buf, "test_se0_nak", 12))
                testmode = USB_TEST_SE0_NAK;
        else if (!strncmp(buf, "test_packet", 11))
                testmode = USB_TEST_PACKET;
        else if (!strncmp(buf, "test_force_enable", 17))
                testmode = USB_TEST_FORCE_ENABLE;
        else
                testmode = 0;

        spin_lock_irqsave(&dwc->lock, flags);
        dwc3_tcc_set_test_mode(dwc, testmode);
        spin_unlock_irqrestore(&dwc->lock, flags);

        return (ssize_t)count;
}

static DEVICE_ATTR_RW(dwc3_testmode);

static void dwc3_tcc_display_pcfg(uint32_t old_reg, uint32_t new_reg)
{
	int32_t i;

	old_fpcfg_field[0].value = FIELD_GET(GENMASK(3, 0), old_reg);
	old_fpcfg_field[1].value = FIELD_GET(GENMASK(6, 4), old_reg);
	old_fpcfg_field[2].value = FIELD_GET(BIT(7), old_reg);
	old_fpcfg_field[3].value = FIELD_GET(GENMASK(9, 8), old_reg);
	old_fpcfg_field[4].value = FIELD_GET(GENMASK(11, 10), old_reg);
	old_fpcfg_field[5].value = FIELD_GET(GENMASK(13, 12), old_reg);
	old_fpcfg_field[6].value = FIELD_GET(GENMASK(15, 14), old_reg);

	new_fpcfg_field[0].value = FIELD_GET(GENMASK(3, 0), new_reg);
	new_fpcfg_field[1].value = FIELD_GET(GENMASK(6, 4), new_reg);
	new_fpcfg_field[2].value = FIELD_GET(BIT(7), new_reg);
	new_fpcfg_field[3].value = FIELD_GET(GENMASK(9, 8), new_reg);
	new_fpcfg_field[4].value = FIELD_GET(GENMASK(11, 10), new_reg);
	new_fpcfg_field[5].value = FIELD_GET(GENMASK(13, 12), new_reg);
	new_fpcfg_field[6].value = FIELD_GET(GENMASK(15, 14), new_reg);

	for (i = 0; i < PCFG_MAX; i++) {
		if (old_fpcfg_field[i].value != new_fpcfg_field[i].value) {
			pr_info("%s: 0x%X -> 0x%X\n", old_fpcfg_field[i].name,
					old_fpcfg_field[i].value,
					new_fpcfg_field[i].value);
		} else {
			pr_info("%s: 0x%X\n", old_fpcfg_field[i].name,
					old_fpcfg_field[i].value);
		}
	}
}

// Show the current value of the USB30 PHY Configuration Register (FPCFG1)
static ssize_t dwc3_pcfg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	const struct dwc3_tcc_hcd *dwc3_tcc = dev_get_drvdata(dev);
	uint32_t reg = readl(&dwc3_tcc->phy_regs->FPCFG1);

	pr_info("U30_FPCFG1: 0x%08X\n", reg);
	dwc3_tcc_display_pcfg(reg, reg);

	return 0;
}

// HS DC Voltage Level is set
static ssize_t dwc3_pcfg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	const struct dwc3_tcc_hcd *dwc3_tcc = dev_get_drvdata(dev);
	ssize_t ret;
	uint32_t old_reg = readl(&dwc3_tcc->phy_regs->FPCFG1);
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
		pr_info("1) echo 0x6789abcdef > dwc3_pcfg\n");
		pr_info("2) echo 0X6789ABCDEF > dwc3_pcfg\n\n");
	} else {
		ret = count;

		pr_info("[INFO][USB] Before\n");
		pr_info("U30_FPCFG1 = 0x%08X\n\n", old_reg);

		writel(new_reg, &dwc3_tcc->phy_regs->FPCFG1);
		new_reg = readl(&dwc3_tcc->phy_regs->FPCFG1);

		pr_info("[INFO][USB] After\n");
		pr_info("U30_FPCFG1 = 0x%08X\n", new_reg);
		dwc3_tcc_display_pcfg(old_reg, new_reg);
	}

	return ret;
}

static DEVICE_ATTR_RW(dwc3_pcfg);

static int32_t dwc3_tcc_set_vbus_resource(struct usb_phy *uphy)
{
	struct tcc_dwc3_phy *tcc_dwc3 =
		container_of(uphy, struct tcc_dwc3_phy, uphy);
	struct device *dev = tcc_dwc3->dev;
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
		tcc_dwc3->vbus_supply =
			devm_regulator_get_optional(dev, "vbus");
		if (IS_ERR(tcc_dwc3->vbus_supply)) {
			dev_err(dev, "[ERROR][USB] VBus Supply is not valid.\n");
			ret = PTR_ERR(tcc_dwc3->vbus_supply);
		}
	} else {
		dev_info(dev, "[INFO][USB] vbus-ctrl-able property is not declared.\n");
		ret = -ENODEV;
	}

	return ret;
}

static int32_t dwc3_tcc_remove_child(struct device *dev, void *unused)
{
	struct platform_device *pdev = to_platform_device(dev);

	platform_device_unregister(pdev);

	return 0;
}

static int32_t dwc3_tcc_phy_register(struct dwc3_tcc_hcd *dwc3_tcc)
{
	int32_t ret = -ENODEV;

	if (of_find_property(dwc3_tcc->dev->of_node, "usb-phy", NULL)
			!= NULL) {
		dwc3_tcc->uphy = devm_usb_get_phy_by_phandle(dwc3_tcc->dev,
				"usb-phy", 0);
		if (IS_ERR(dwc3_tcc->uphy)) {
			if (PTR_ERR(dwc3_tcc->uphy) == -EPROBE_DEFER) {
				ret = -EPROBE_DEFER;
			} else {
				pr_err("[ERROR][USB] error getting dwc3 phy\n");
			}

			dwc3_tcc->uphy = NULL;
		} else {
			ret = dwc3_tcc_set_vbus_resource(dwc3_tcc->uphy);
			if (ret == 0) {
				ret = dwc3_tcc->uphy->init(dwc3_tcc->uphy);

#if defined(CONFIG_DWC3_DUAL_FIRST_HOST) || defined(CONFIG_USB_DWC3_HOST)
				dwc3_tcc_vbus_ctrl_on(dwc3_tcc);
#endif // defined(CONFIG_DWC3_DUAL_FIRST_HOST) || defined(CONFIG_USB_DWC3_HOST)
			}
		}
	}

	return ret;
}

static void dwc3_tcc_phy_unregister(const struct dwc3_tcc_hcd *dwc3_tcc)
{
	if (dwc3_tcc != NULL) {
		(void)usb_phy_set_suspend(dwc3_tcc->uphy, 1);
	}
}

static int32_t dwc3_tcc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *dev_node, *child_node;
	struct dwc3_tcc_hcd *dwc3_tcc;
	int32_t ret = -ENODEV;

	if (dev != NULL) {
		dwc3_tcc = devm_kzalloc(dev, sizeof(*dwc3_tcc), GFP_KERNEL);
		if (dwc3_tcc == NULL) {
			ret = -ENOMEM;
		} else {
			platform_set_drvdata(pdev, dwc3_tcc);

			dwc3_tcc->dev = dev;

			ret = dwc3_tcc_phy_register(dwc3_tcc);
			if (ret != 0) {
				if (ret != -EPROBE_DEFER) {
					dev_err(dev, "[ERROR][USB] couldn't register dwc3 PHY\n");
				}
			} else {
				dev_node = dev->of_node;
				if (dev_node != NULL) {
					child_node = of_get_child_by_name(dev_node, "dwc3");
					if (child_node != NULL) {
						ret = of_platform_populate(dev_node, NULL, NULL, dev);
						if (ret != 0) {
							dev_err(dev, "[ERROR][USB] failed to populate dwc3 tcc\n");
						} else {
							ret = device_create_file(dev, &dev_attr_dwc3_vbus);
							if (ret != 0) {
								dev_err(dev, "[ERROR][USB] failed to create dwc3_vbus\n");
							} else {
								dwc3_tcc->dwc3 = of_find_device_by_node(child_node);

								ret = device_create_file(&dwc3_tcc->dwc3->dev, &dev_attr_dwc3_drdmode);
								if (ret != 0) {
									dev_err(dev, "[ERROR][USB] failed to create dwc3_drdmode\n");
								} else {
									ret = device_create_file(&dwc3_tcc->dwc3->dev, &dev_attr_dwc3_testmode);
									if (ret != 0) {
										dev_err(dev, "[ERROR][USB] failed to create dwc3_testmode\n");
									} else {
										ret = device_create_file(dev, &dev_attr_dwc3_pcfg);
										if (ret != 0) {
											dev_err(dev, "[ERROR][USB] failed to create dwc3_pcfg\n");
										}
									}
								}
							}
						}
					} else {
						dev_err(dev, "[ERROR][USB] failed to find dwc3 core\n");
						ret = -ENODEV;
					}
				} else {
					dev_err(dev, "[ERROR][USB] failed to find dwc3 tcc\n");
					ret = -ENODEV;
				}
			}
		}
	}

	return ret;
}

static int32_t dwc3_tcc_remove(struct platform_device *pdev)
{
	struct dwc3_tcc_hcd *dwc3_tcc = platform_get_drvdata(pdev);
	int32_t ret = -ENODEV;

	if (dwc3_tcc != NULL) {
		of_platform_depopulate(dwc3_tcc->dev);
		device_remove_file(&pdev->dev, &dev_attr_dwc3_pcfg);
		device_remove_file(&dwc3_tcc->dwc3->dev, &dev_attr_dwc3_drdmode);
		device_remove_file(&dwc3_tcc->dwc3->dev, &dev_attr_dwc3_testmode);
		device_remove_file(&pdev->dev, &dev_attr_dwc3_vbus);

		(void)device_for_each_child(&pdev->dev, NULL,
				&dwc3_tcc_remove_child);

		dwc3_tcc_phy_unregister(dwc3_tcc);

		dwc3_tcc_vbus_ctrl_off(dwc3_tcc);

		ret = 0;
	}

	return ret;
}

static void dwc3_tcc_shutdown(struct platform_device *dev)
{
	struct dwc3_tcc_hcd *dwc3_tcc = platform_get_drvdata(dev);

	dwc3_tcc_vbus_ctrl_off(dwc3_tcc);
}

#if defined(CONFIG_PM_SLEEP)
static int32_t dwc3_tcc_suspend(struct device *dev)
{
	struct dwc3_tcc_hcd *dwc3_tcc = dev_get_drvdata(dev);

	dwc3_tcc_vbus_ctrl_off(dwc3_tcc);

	return 0;
}

static int32_t dwc3_tcc_resume(struct device *dev)
{
	struct dwc3_tcc_hcd *dwc3_tcc = dev_get_drvdata(dev);

	dwc3_tcc_vbus_ctrl_on(dwc3_tcc);

	return 0;
}

static const struct dev_pm_ops dwc3_tcc_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dwc3_tcc_suspend, dwc3_tcc_resume)
};

#define DEV_PM_OPS	(&dwc3_tcc_dev_pm_ops)
#else
#define DEV_PM_OPS	(NULL)
#endif /* CONFIG_PM_SLEEP */

static const struct of_device_id dwc3_tcc_match[] = {
	{ .compatible = "telechips,tcc-dwc3" },
	{ },
};
MODULE_DEVICE_TABLE(of, dwc3_tcc_match);

static struct platform_driver dwc3_tcc_driver = {
	.probe			= dwc3_tcc_probe,
	.remove			= dwc3_tcc_remove,
	.shutdown		= dwc3_tcc_shutdown,
	.driver = {
		.name		= "tcc-dwc3",
		.owner		= THIS_MODULE,
		.of_match_table	= dwc3_tcc_match,
		.pm		= DEV_PM_OPS,
	},
};

module_platform_driver(dwc3_tcc_driver);

MODULE_ALIAS("platform:tcc-dwc3");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DesignWare USB3 Telechips Glue Layer");
