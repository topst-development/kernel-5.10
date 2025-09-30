// SPDX-License-Identifier: (GPL-2.0+ OR BSD-3-Clause)
/*
 * platform.c - DesignWare HS OTG Controller platform driver
 *
 * Copyright (C) Matthijs Kooijman <matthijs@stdin.nl>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the above-listed copyright holders may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/of_device.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/platform_data/s3c-hsotg.h>
#include <linux/reset.h>

#include <linux/usb/of.h>

#include "core.h"
#include "hcd.h"
#include "debug.h"

static const char dwc2_driver_name[] = "dwc2";

/*
 * Check the dr_mode against the module configuration and hardware
 * capabilities.
 *
 * The hardware, module, and dr_mode, can each be set to host, device,
 * or otg. Check that all these values are compatible and adjust the
 * value of dr_mode if possible.
 *
 *                      actual
 *    HW  MOD dr_mode   dr_mode
 *  ------------------------------
 *   HST  HST  any    :  HST
 *   HST  DEV  any    :  ---
 *   HST  OTG  any    :  HST
 *
 *   DEV  HST  any    :  ---
 *   DEV  DEV  any    :  DEV
 *   DEV  OTG  any    :  DEV
 *
 *   OTG  HST  any    :  HST
 *   OTG  DEV  any    :  DEV
 *   OTG  OTG  any    :  dr_mode
 */

#if defined(CONFIG_USB_DWC2_TCC)
#if defined(CONFIG_VBUS_CTRL_DEF_ENABLE)
static uint32_t dwc2_vbus_control_enable = ENABLE;
#else /* CONFIG_VBUS_CTRL_DEF_ENABLE */
static uint32_t dwc2_vbus_control_enable = DISABLE;
#endif /* !defined(CONFIG_VBUS_CTRL_DEF_ENABLE) */
module_param(dwc2_vbus_control_enable, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dwc2_vbus_control_enable, "TCC DWC2 VBus control enable");

struct pcfg_field old_dwc2_pcfg_field[PCFG_MAX] = {
	{"TXVRT  "}, {"CDT    "}, {"TXPPT  "}, {"TP     "}, {"TXRT   "},
	{"TXREST "}, {"TXHSXVT"},
};

struct pcfg_field new_dwc2_pcfg_field[PCFG_MAX] = {
	{"TXVRT  "}, {"CDT    "}, {"TXPPT  "}, {"TP     "}, {"TXRT   "},
	{"TXREST "}, {"TXHSXVT"},
};

static int32_t dwc2_tcc_vbus_ctrl(struct dwc2_hsotg *hsotg, int32_t on_off)
{
	int ret = 0;

	if (hsotg->uphy == NULL) {
		dev_info(hsotg->dev, "[INFO][USB] PHY driver does NOT exist!\n");
		ret = -ENODEV;
	} else {
		if (dwc2_vbus_control_enable == DISABLE) {
			dev_info(hsotg->dev, "[INFO][USB] VBus control is DISABLED!\n");
		} else {
			hsotg->vbus_status = on_off;

			if (on_off == ON) {
				ret = usb_phy_vbus_on(hsotg->uphy);
			} else {
				ret = usb_phy_vbus_off(hsotg->uphy);
			}
		}
	}

	return ret;
}

static ssize_t dwc2_vbus_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);

	return sprintf(buf, "TCC DWC2 vbus: %s\n",
			(hsotg->vbus_status == ON) ? "on" : "off");
}

static ssize_t dwc2_vbus_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);

	if (strncmp(buf, "on", 2) == 0) {
		(void)dwc2_tcc_vbus_ctrl(hsotg, ON);
	}

	if (strncmp(buf, "off", 3) == 0) {
		(void)dwc2_tcc_vbus_ctrl(hsotg, OFF);
	}

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(dwc2_vbus);

static int dwc2_tcc_set_test_mode(struct dwc2_hsotg *hsotg, int testmode)
{
	void __iomem *addr;
	u32 mask, shift, val;

#if defined(CONFIG_USB_DWC2_TCC_MUX)
	addr = &hsotg->base[1] + PORTSC;
	mask = PRT_TSTCTL_MASK;
	shift = PRT_TSTCTL_SHIFT;
#else /* CONFIG_USB_DWC2_TCC_MUX */
	addr = &hsotg->regs + HPRT0;
	mask = HPRT0_TSTCTL_MASK;
	shift = HPRT0_TSTCTL_SHIFT;
#endif /* !defined(CONFIG_USB_DWC2_TCC_MUX) */
	val = readl(addr);

	pr_info("[INFO][USB] Before\n@0x%8p: 0x%08x\n", addr, val);

	val &= ~mask;

	switch (testmode) {
	case USB_TEST_J:
	case USB_TEST_K:
	case USB_TEST_SE0_NAK:
	case USB_TEST_PACKET:
	case USB_TEST_FORCE_ENABLE:
		val |= (testmode << shift);
		break;
	default:
		return -EINVAL;
	}

	writel(val, addr);
	udelay(100);
	val = readl(addr);

	pr_info("\n[INFO][USB] After\n@0x%8p: 0x%08x\n", addr, val);

	return 0;
}

static ssize_t dwc2_host_testmode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);
	void __iomem *addr;
	u32 mask, shift, val;
	unsigned long flags;

#if defined(CONFIG_USB_DWC2_TCC_MUX)
	addr = &hsotg->base[1] + PORTSC;
	mask = PRT_TSTCTL_MASK;
	shift = PRT_TSTCTL_SHIFT;
#else /* CONFIG_USB_DWC2_TCC_MUX */
	addr = &hsotg->regs + HPRT0;
	mask = HPRT0_TSTCTL_MASK;
	shift = HPRT0_TSTCTL_SHIFT;
#endif /* !defined(CONFIG_USB_DWC2_TCC_MUX) */

	spin_lock_irqsave(&hsotg->lock, flags);
	val = readl(addr);
	val &= mask;
	val >>= shift;
	spin_unlock_irqrestore(&hsotg->lock, flags);

	switch (val) {
	case 0:
		pr_info("no test\n");
		break;
	case USB_TEST_PACKET:
		pr_info("test_packet\n");
		break;
	default:
		pr_info("UNKNOWN test mode\n");
	}

	return 0;
}

static ssize_t dwc2_host_testmode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);
	unsigned long flags;
	u32 testmode = 0;

	if (!strncmp(buf, "test_packet", 11)) {
		testmode = USB_TEST_PACKET;
	} else {
		testmode = 0;
	}

	spin_lock_irqsave(&hsotg->lock, flags);
	dwc2_tcc_set_test_mode(hsotg, testmode);
	spin_unlock_irqrestore(&hsotg->lock, flags);

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(dwc2_host_testmode);

static ssize_t dwc2_dev_testmode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);
	unsigned long flags;
	int dctl;

	spin_lock_irqsave(&hsotg->lock, flags);
	dctl = dwc2_readl(hsotg, DCTL);
	dctl &= DCTL_TSTCTL_MASK;
	dctl >>= DCTL_TSTCTL_SHIFT;
	spin_unlock_irqrestore(&hsotg->lock, flags);

	switch (dctl) {
	case 0:
		pr_info("no test\n");
		break;
	case USB_TEST_PACKET:
		pr_info("test_packet\n");
		break;
	default:
		pr_info("UNKNOWN test mode\n");
	}

	return 0;
}

static ssize_t dwc2_dev_testmode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);
	unsigned long flags;
	u32 testmode = 0;

	if (!strncmp(buf, "test_packet", 11)) {
		testmode = USB_TEST_PACKET;
	} else {
		testmode = 0;
	}

	spin_lock_irqsave(&hsotg->lock, flags);
	dwc2_hsotg_set_test_mode(hsotg, testmode);
	spin_unlock_irqrestore(&hsotg->lock, flags);

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(dwc2_dev_testmode);

static void dwc2_tcc_print_pcfg(uint32_t old_reg, uint32_t new_reg)
{
	int32_t i;

	old_dwc2_pcfg_field[0].value = FIELD_GET(GENMASK(3, 0), old_reg);
	old_dwc2_pcfg_field[1].value = FIELD_GET(GENMASK(6, 4), old_reg);
	old_dwc2_pcfg_field[2].value = FIELD_GET(BIT(7), old_reg);
	old_dwc2_pcfg_field[3].value = FIELD_GET(GENMASK(9, 8), old_reg);
	old_dwc2_pcfg_field[4].value = FIELD_GET(GENMASK(11, 10), old_reg);
	old_dwc2_pcfg_field[5].value = FIELD_GET(GENMASK(13, 12), old_reg);
	old_dwc2_pcfg_field[6].value = FIELD_GET(GENMASK(15, 14), old_reg);

	new_dwc2_pcfg_field[0].value = FIELD_GET(GENMASK(3, 0), new_reg);
	new_dwc2_pcfg_field[1].value = FIELD_GET(GENMASK(6, 4), new_reg);
	new_dwc2_pcfg_field[2].value = FIELD_GET(BIT(7), new_reg);
	new_dwc2_pcfg_field[3].value = FIELD_GET(GENMASK(9, 8), new_reg);
	new_dwc2_pcfg_field[4].value = FIELD_GET(GENMASK(11, 10), new_reg);
	new_dwc2_pcfg_field[5].value = FIELD_GET(GENMASK(13, 12), new_reg);
	new_dwc2_pcfg_field[6].value = FIELD_GET(GENMASK(15, 14), new_reg);

	for (i = 0; i < PCFG_MAX; i++) {
		if (old_dwc2_pcfg_field[i].value != new_dwc2_pcfg_field[i].value) {
			pr_info("%s: 0x%X -> 0x%X\n", old_dwc2_pcfg_field[i].name,
					old_dwc2_pcfg_field[i].value,
					new_dwc2_pcfg_field[i].value);
		} else {
			pr_info("%s: 0x%X\n", old_dwc2_pcfg_field[i].name,
					old_dwc2_pcfg_field[i].value);
		}
	}
}

static ssize_t dwc2_host_pcfg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	const struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);
	volatile void __iomem *addr;
	char *name;
	uint32_t val;
#if defined(CONFIG_USB_DWC2_TCC_MUX)
	struct tcc_ehci_phy *tcc_ehci =
		container_of(hsotg->mux_uphy, struct tcc_ehci_phy, uphy);

	addr = &tcc_ehci->phy_regs->PCFG1;
	name = "U20DH_HST_PCFG1";
#else /* CONFIG_USB_DWC2_TCC_MUX */
	struct tcc_dwc_otg_phy *tcc_dwc_otg =
		container_of(hsotg->uphy, struct tcc_dwc_otg_phy, uphy);

	addr = &tcc_dwc_otg->phy_regs->PCFG1;
	name = "USBOTG_PCFG1";
#endif /* !defined(CONFIG_USB_DWC2_TCC_MUX) */
	val = readl(addr);

	pr_info("%s: 0x%08X\n", name, val);
	dwc2_tcc_print_pcfg(val, val);

	return 0;
}

static ssize_t dwc2_host_pcfg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	const struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);
	volatile void __iomem *addr;
	char *name;
	ssize_t ret;
	uint32_t old_reg, new_reg;
#if defined(CONFIG_USB_DWC2_TCC_MUX)
	struct tcc_ehci_phy *tcc_ehci =
		container_of(hsotg->mux_uphy, struct tcc_ehci_phy, uphy);

	addr = &tcc_ehci->phy_regs->PCFG1;
	name = "U20DH_HST_PCFG1";
#else /* CONFIG_USB_DWC2_TCC_MUX */
	struct tcc_dwc_otg_phy *tcc_dwc_otg =
		container_of(hsotg->uphy, struct tcc_dwc_otg_phy, uphy);

	addr = &tcc_dwc_otg->phy_regs->PCFG1;
	name = "USBOTG_PCFG1";
#endif /* !defined(CONFIG_USB_DWC2_TCC_MUX) */
	old_reg = readl(addr);

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
		pr_info("1) echo 0x89abcdef > dwc2_host_pcfg\n");
		pr_info("2) echo 0X89ABCDEF > dwc2_host_pcfg\n\n");
	} else {
		ret = count;

		pr_info("[INFO][USB] Before\n");
		pr_info("%s = 0x%08X\n\n", name, old_reg);

		writel(new_reg, addr);
		new_reg = readl(addr);

		pr_info("[INFO][USB] After\n");
		pr_info("%s = 0x%08X\n", name, new_reg);
		dwc2_tcc_print_pcfg(old_reg, new_reg);
	}

	return ret;
}

static DEVICE_ATTR_RW(dwc2_host_pcfg);

static ssize_t dwc2_dev_pcfg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	const struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);
	struct tcc_dwc_otg_phy *tcc_dwc_otg =
		container_of(hsotg->uphy, struct tcc_dwc_otg_phy, uphy);
	uint32_t val = readl(&tcc_dwc_otg->phy_regs->PCFG1);
#if defined(CONFIG_USB_DWC2_TCC_MUX)
	char *name = "U20DH_DEV_PCFG1";
#else /* CONFIG_USB_DWC2_TCC_MUX */
	char *name = "USBOTG_PCFG1";
#endif /* !defined(CONFIG_USB_DWC2_TCC_MUX) */

	pr_info("%s: 0x%08X\n", name, val);
	dwc2_tcc_print_pcfg(val, val);

	return 0;
}

static ssize_t dwc2_dev_pcfg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	const struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);
	struct tcc_dwc_otg_phy *tcc_dwc_otg =
		container_of(hsotg->uphy, struct tcc_dwc_otg_phy, uphy);
	volatile void __iomem *addr = &tcc_dwc_otg->phy_regs->PCFG1;
	ssize_t ret;
	uint32_t old_reg = readl(addr);
	uint32_t new_reg;
#if defined(CONFIG_USB_DWC2_TCC_MUX)
	char *name = "U20DH_DEV_PCFG1";
#else /* CONFIG_USB_DWC2_TCC_MUX */
	char *name = "USBOTG_PCFG1";
#endif /* !defined(CONFIG_USB_DWC2_TCC_MUX) */

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
		pr_info("1) echo 0x89abcdef > dwc2_dev_pcfg\n");
		pr_info("2) echo 0X89ABCDEF > dwc2_dev_pcfg\n\n");
	} else {
		ret = count;

		pr_info("[INFO][USB] Before\n");
		pr_info("%s = 0x%08X\n\n", name, old_reg);

		writel(new_reg, addr);
		new_reg = readl(addr);

		pr_info("[INFO][USB] After\n");
		pr_info("%s = 0x%08X\n", name, new_reg);
		dwc2_tcc_print_pcfg(old_reg, new_reg);
	}

	return ret;
}

static DEVICE_ATTR_RW(dwc2_dev_pcfg);

#if defined(CONFIG_USB_DWC2_TCC_MUX)
static struct platform_device *dwc2_tcc_create_pdev(struct dwc2_hsotg *hsotg,
		u8 index)
{
	struct resource hci_res[2];
	struct platform_device *hci_dev;
	int ret;

	memset(hci_res, 0, sizeof(hci_res));

	hci_res[0].start = hsotg->base[index];
	hci_res[0].end = hsotg->base[index] + hsotg->size[index] - 1;
	hci_res[0].flags = IORESOURCE_MEM;

	hci_res[1].start = hsotg->mux_irq;
	hci_res[1].flags = IORESOURCE_IRQ;

	hci_dev = platform_device_alloc((index == 1) ?
			"ehci-platform" : "ohci-platform", 0);
	if (!hci_dev)
		return ERR_PTR(-ENOMEM);

	hci_dev->dev.parent = hsotg->dev;
	hci_dev->dev.dma_mask = &hci_dev->dev.coherent_dma_mask;

	ret = platform_device_add_resources(hci_dev, hci_res,
			ARRAY_SIZE(hci_res));
	if (ret)
		goto err_alloc;

	if (index == 1) {
		ret = platform_device_add_data(hci_dev, &hsotg->ehci_pdata,
				sizeof(hsotg->ehci_pdata));
	} else {
		ret = platform_device_add_data(hci_dev, &hsotg->ohci_pdata,
				sizeof(hsotg->ohci_pdata));
	}
	if (ret)
		goto err_alloc;

	ret = platform_device_add(hci_dev);
	if (ret)
		goto err_alloc;

	return hci_dev;

err_alloc:
	platform_device_put(hci_dev);

	return ERR_PTR(ret);
}

int dwc2_tcc_mux_hcd_init(struct dwc2_hsotg *hsotg)
{
	int32_t ret = 0;
	u8 i;

	for (i = 1; i < 3; i++) {
		hsotg->mux_hcd[i] = dwc2_tcc_create_pdev(hsotg, i);
		if (IS_ERR(hsotg->mux_hcd[i])) {
			if (i == 2) {
				platform_device_unregister(hsotg->mux_hcd[1]);
			}

			ret = PTR_ERR(hsotg->mux_hcd[i]);
			if (ret != 0) {
				dev_err(hsotg->dev, "[ERROR][USB] MUX HCD init FAIL!\n");
				break;
			}
		}
	}

	return ret;
}

void dwc2_tcc_mux_hcd_remove(struct dwc2_hsotg *hsotg)
{
	u8 i;

	for (i = 1; i < 3; i++) {
		if (hsotg->mux_hcd[i] != NULL) {
			platform_device_unregister(hsotg->mux_hcd[i]);
		}
	}
}
#endif /* CONFIG_USB_DWC2_TCC_MUX */

#if defined(CONFIG_USB_DWC2_DUAL_ROLE)
static ssize_t dwc2_drdmode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);

	return sprintf(buf, "dwc2_drdmode: %s\n",
			(hsotg->dr_mode == USB_DR_MODE_PERIPHERAL) ?
			"Device" : "Host");
}

static ssize_t dwc2_drdmode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct dwc2_hsotg *hsotg = dev_get_drvdata(dev);
	enum usb_dr_mode current_mode = hsotg->dr_mode;

	if (strncmp(buf, "host", 4) == 0) {
		if (hsotg->dr_mode == USB_DR_MODE_HOST) {
			dev_info(dev, "[INFO][USB] Already in HOST mode!\n");
		} else {
			hsotg->dr_mode = USB_DR_MODE_HOST;
		}
	} else if (strncmp(buf, "device", 6) == 0) {
		if (hsotg->dr_mode == USB_DR_MODE_PERIPHERAL)
			dev_info(dev, "[INFO][USB] Already in DEVICE mode!\n");

		hsotg->dr_mode = USB_DR_MODE_PERIPHERAL;
		if (hsotg->gadget_enabled != 1) {
			int ret;
			ret = dwc2_get_hwparams(hsotg);

			dwc2_init_params(hsotg);

			ret += dwc2_gadget_init(hsotg);
			if (ret) {
				dwc2_lowlevel_hw_disable(hsotg);
				dwc2_drd_exit(hsotg);
				dev_info(dev, "[INFO][USB] Invalid dwc2_drdmode: %s", buf);
				return (ssize_t)count;
			}
			hsotg->gadget_enabled = 1;

			dwc2_debugfs_init(hsotg);
			ret = usb_add_gadget_udc(hsotg->dev, &hsotg->gadget);
			if (ret) {
				hsotg->gadget.udc = NULL;
				dwc2_hsotg_remove(hsotg);
				dwc2_debugfs_exit(hsotg);
				if (hsotg->hcd_enabled)
					dwc2_hcd_remove(hsotg);
			}
			dwc2_lowlevel_hw_disable(hsotg);
		}
	} else {
		dev_info(dev, "[INFO][USB] Invalid dwc2_drdmode: %s", buf);
	}

	if (current_mode != hsotg->dr_mode) {
		if (!work_pending(&hsotg->wf_otg)) {
			queue_work(hsotg->wq_otg, &hsotg->wf_otg);
			flush_work(&hsotg->wf_otg);
		}
	}

	return (ssize_t)count;
}

static DEVICE_ATTR_RW(dwc2_drdmode);

static u32 dwc2_tcc_read_frameno(struct dwc2_hsotg *hsotg)
{
	u32 dsts;

	dsts = dwc2_readl(hsotg, DSTS);
	dsts &= DSTS_SOFFN_MASK;
	dsts >>= DSTS_SOFFN_SHIFT;

	return dsts;
}

static u32 dwc2_tcc_read_suspend_status(struct dwc2_hsotg *hsotg)
{
	u32 dsts;

	dsts = dwc2_readl(hsotg, DSTS);
	dsts &= DSTS_SUSPSTS;

	return dsts;
}

static u32 dwc2_tcc_read_erratic_error(struct dwc2_hsotg *hsotg)
{
	u32 dsts;

	dsts = dwc2_readl(hsotg, DSTS);
	dsts &= DSTS_ERRATICERR;

	if (dsts) {
		dev_info(hsotg->dev, "[INFO][USB] Erratic error has occurred!\n");
	}

	return dsts;
}

static int dwc2_tcc_soffn_monitor_thread(void *work)
{
	struct dwc2_hsotg *hsotg = (struct dwc2_hsotg *)work;
	int retry = 100; // Wait for the iPhone to switch roles
	bool is_disconnected = false;
	unsigned long flags;

	while (!kthread_should_stop() && (retry > 0)) {
		usleep_range(10000, 10020);

		/*
		 * Monitoring starts when the frame number of
		 * SoF(Start of Frame) is not NULL or is not in suspend state.
		 * If the host is not connected for 1 second after role
		 * switching, it is deternined as disconnection
		 */
		if (!dwc2_tcc_read_frameno(hsotg) ||
				dwc2_tcc_read_suspend_status(hsotg) ||
				dwc2_tcc_read_erratic_error(hsotg)) {
			retry--;

			if ((is_disconnected == true) && (retry <= 0))
				break;

			continue;
		}

		is_disconnected = true;
		retry = 3;
	}

	(void)dwc2_tcc_vbus_ctrl(hsotg, OFF);

	if (hsotg->dr_mode == USB_DR_MODE_PERIPHERAL) {
		spin_lock_irqsave(&hsotg->lock, flags);
		dwc2_hsotg_disconnect(hsotg);
		spin_unlock_irqrestore(&hsotg->lock, flags);

		if (hsotg->driver && hsotg->driver->disconnect)
			hsotg->driver->disconnect(&hsotg->gadget);
	} else {
		dev_info(hsotg->dev, "[INFO][USB] Current USB mode: host\n");
	}

	hsotg->soffn_thread = NULL;
	msleep(200);

	return 0;
}

static void dwc2_tcc_set_drdmode(struct work_struct *work)
{
	struct dwc2_hsotg *hsotg =
		container_of(work, struct dwc2_hsotg, wf_otg);
	int retry = 0;
	unsigned long flags;

	if (hsotg->dr_mode == USB_DR_MODE_PERIPHERAL) {
#if defined(CONFIG_USB_DWC2_TCC_MUX)
		dwc2_tcc_mux_hcd_remove(hsotg);
#endif /* CONFIG_USB_DWC2_TCC_MUX */
	} else {
		if (hsotg->soffn_thread != NULL) {
			kthread_stop(hsotg->soffn_thread);
			hsotg->soffn_thread = NULL;

			spin_lock_irqsave(&hsotg->lock, flags);
			dwc2_hsotg_disconnect(hsotg);
			spin_unlock_irqrestore(&hsotg->lock, flags);
		}

		do {
			if ((dwc2_tcc_vbus_ctrl(hsotg, ON) == 0) ||
					(dwc2_vbus_control_enable == DISABLE)) {
				break;
			}

			retry++;
			msleep(50);

			dev_info(hsotg->dev, "[INFO][USB] Retrying VBus control for %d times\n",
					retry);
		} while (retry < 10);
	}

	dwc2_tcc_change_drdmode(hsotg);

	if (hsotg->dr_mode == USB_DR_MODE_HOST) {
#if defined(CONFIG_USB_DWC2_TCC_MUX)
		dwc2_tcc_mux_hcd_init(hsotg);
#endif /* CONFIG_USB_DWC2_TCC_MUX */
	} else {
		if (hsotg->vbus_status == ON) {
			if (hsotg->soffn_thread != NULL) {
				kthread_stop(hsotg->soffn_thread);
				hsotg->soffn_thread = NULL;
			}

			hsotg->soffn_thread =
				kthread_run(dwc2_tcc_soffn_monitor_thread,
						(void *)hsotg, "dwc2-soffn");
			if (IS_ERR(hsotg->soffn_thread))
				dev_warn(hsotg->dev, "[WARN][USB] Running soffn_thread is FAILED!\n");
		}
	}

	dev_info(hsotg->dev, "[INFO][USB] Current USB mode: %s\n",
			(hsotg->dr_mode == USB_DR_MODE_PERIPHERAL) ?
			"Device" : "Host");
}
#endif /* CONFIG_USB_DWC2_DUAL_ROLE */
#endif /* CONFIG_USB_DWC2_TCC */

static int dwc2_get_dr_mode(struct dwc2_hsotg *hsotg)
{
	enum usb_dr_mode mode;

	hsotg->dr_mode = usb_get_dr_mode(hsotg->dev);
	if (hsotg->dr_mode == USB_DR_MODE_UNKNOWN)
		hsotg->dr_mode = USB_DR_MODE_OTG;

	mode = hsotg->dr_mode;

	if (dwc2_hw_is_device(hsotg)) {
		if (IS_ENABLED(CONFIG_USB_DWC2_HOST)) {
			dev_err(hsotg->dev,
				"Controller does not support host mode.\n");
			return -EINVAL;
		}
		mode = USB_DR_MODE_PERIPHERAL;
	} else if (dwc2_hw_is_host(hsotg)) {
		if (IS_ENABLED(CONFIG_USB_DWC2_PERIPHERAL)) {
			dev_err(hsotg->dev,
				"Controller does not support device mode.\n");
			return -EINVAL;
		}
		mode = USB_DR_MODE_HOST;
	} else {
		if (IS_ENABLED(CONFIG_USB_DWC2_HOST))
			mode = USB_DR_MODE_HOST;
		else if (IS_ENABLED(CONFIG_USB_DWC2_PERIPHERAL))
			mode = USB_DR_MODE_PERIPHERAL;
	}

	if (mode != hsotg->dr_mode) {
		dev_warn(hsotg->dev,
			 "Configuration mismatch. dr_mode forced to %s\n",
			mode == USB_DR_MODE_HOST ? "host" : "device");

		hsotg->dr_mode = mode;
	}

	return 0;
}

static int __dwc2_lowlevel_hw_enable(struct dwc2_hsotg *hsotg)
{
	struct platform_device *pdev = to_platform_device(hsotg->dev);
	int ret;

#if defined(CONFIG_USB_DWC2_TCC)
#if defined(CONFIG_USB_DWC2_HOST) || defined(CONFIG_USB_DWC2_TCC_FIRST_HOST)
#if !defined(CONFIG_TCC_EH_ELECT_TST) || !defined(CONFIG_USB_OTG_WHITELIST)
	dwc2_tcc_vbus_ctrl(hsotg, ON);
#endif
#endif /* CONFIG_USB_DWC2_HOST || CONFIG_USB_DWC2_TCC_FIRST_HOST */
#else /* CONFIG_USB_DWC2_TCC */
	ret = regulator_bulk_enable(ARRAY_SIZE(hsotg->supplies),
				    hsotg->supplies);
	if (ret)
		return ret;

	if (hsotg->clk) {
		ret = clk_prepare_enable(hsotg->clk);
		if (ret)
			return ret;
	}
#endif /* !defined(CONFIG_USB_DWC2_TCC) */

	if (hsotg->uphy) {
		ret = usb_phy_init(hsotg->uphy);
	} else if (hsotg->plat && hsotg->plat->phy_init) {
		ret = hsotg->plat->phy_init(pdev, hsotg->plat->phy_type);
	} else {
		ret = phy_init(hsotg->phy);
		if (ret == 0)
			ret = phy_power_on(hsotg->phy);
	}

#if defined(CONFIG_USB_DWC2_TCC_MUX)
	if (hsotg->mux_uphy) {
		if (hsotg->dr_mode != USB_DR_MODE_PERIPHERAL) {
			ret = usb_phy_init(hsotg->mux_uphy);
			dwc2_tcc_select_mux(hsotg->uphy, HOST);
		}
	}
#endif /* CONFIG_USB_DWC2_TCC_MUX */

	return ret;
}

/**
 * dwc2_lowlevel_hw_enable - enable platform lowlevel hw resources
 * @hsotg: The driver state
 *
 * A wrapper for platform code responsible for controlling
 * low-level USB platform resources (phy, clock, regulators)
 */
int dwc2_lowlevel_hw_enable(struct dwc2_hsotg *hsotg)
{
	int ret = __dwc2_lowlevel_hw_enable(hsotg);

	if (ret == 0)
		hsotg->ll_hw_enabled = true;
	return ret;
}

static int __dwc2_lowlevel_hw_disable(struct dwc2_hsotg *hsotg)
{
	struct platform_device *pdev = to_platform_device(hsotg->dev);
	int ret = 0;

#if defined(CONFIG_USB_DWC2_TCC_MUX)
	if (hsotg->mux_uphy) {
		usb_phy_shutdown(hsotg->mux_uphy);
	}
#endif /* CONFIG_USB_DWC2_TCC_MUX */

	if (hsotg->uphy) {
		usb_phy_shutdown(hsotg->uphy);
	} else if (hsotg->plat && hsotg->plat->phy_exit) {
		ret = hsotg->plat->phy_exit(pdev, hsotg->plat->phy_type);
	} else {
		ret = phy_power_off(hsotg->phy);
		if (ret == 0)
			ret = phy_exit(hsotg->phy);
	}
	if (ret)
		return ret;

#if defined(CONFIG_USB_DWC2_TCC)
	ret = dwc2_tcc_vbus_ctrl(hsotg, OFF);
	return ret;
#else /* CONFIG_USB_DWC2_TCC */
	if (hsotg->clk)
		clk_disable_unprepare(hsotg->clk);
#endif /* !defined(CONFIG_USB_DWC2_TCC) */

	return regulator_bulk_disable(ARRAY_SIZE(hsotg->supplies), hsotg->supplies);
}

/**
 * dwc2_lowlevel_hw_disable - disable platform lowlevel hw resources
 * @hsotg: The driver state
 *
 * A wrapper for platform code responsible for controlling
 * low-level USB platform resources (phy, clock, regulators)
 */
int dwc2_lowlevel_hw_disable(struct dwc2_hsotg *hsotg)
{
	int ret = __dwc2_lowlevel_hw_disable(hsotg);

	if (ret == 0)
		hsotg->ll_hw_enabled = false;
	return ret;
}

static int dwc2_lowlevel_hw_init(struct dwc2_hsotg *hsotg)
{
#if defined(CONFIG_USB_DWC2_TCC)
	int ret;
	struct tcc_dwc_otg_phy *tcc_dwc_otg;
#else /* CONFIG_USB_DWC2_TCC */
	int i, ret;
#endif /* !defined(CONFIG_USB_DWC2_TCC) */

	hsotg->reset = devm_reset_control_get_optional(hsotg->dev, "dwc2");
	if (IS_ERR(hsotg->reset)) {
		ret = PTR_ERR(hsotg->reset);
		dev_err(hsotg->dev, "error getting reset control %d\n", ret);
		return ret;
	}

	reset_control_deassert(hsotg->reset);

	hsotg->reset_ecc = devm_reset_control_get_optional(hsotg->dev, "dwc2-ecc");
	if (IS_ERR(hsotg->reset_ecc)) {
		ret = PTR_ERR(hsotg->reset_ecc);
		dev_err(hsotg->dev, "error getting reset control for ecc %d\n", ret);
		return ret;
	}

	reset_control_deassert(hsotg->reset_ecc);

	/*
	 * Attempt to find a generic PHY, then look for an old style
	 * USB PHY and then fall back to pdata
	 */
	hsotg->phy = devm_phy_get(hsotg->dev, "usb2-phy");
	if (IS_ERR(hsotg->phy)) {
		ret = PTR_ERR(hsotg->phy);
		switch (ret) {
		case -ENODEV:
		case -ENOSYS:
			hsotg->phy = NULL;
			break;
		case -EPROBE_DEFER:
			return ret;
		default:
			dev_err(hsotg->dev, "error getting phy %d\n", ret);
			return ret;
		}
	}

	if (!hsotg->phy) {
#if defined(CONFIG_USB_DWC2_TCC)
		hsotg->uphy = devm_usb_get_phy_by_phandle(hsotg->dev,
				"usb-phy", 0);
#else /* CONFIG_USB_DWC2_TCC */
		hsotg->uphy = devm_usb_get_phy(hsotg->dev, USB_PHY_TYPE_USB2);
#endif /* !defined(CONFIG_USB_DWC2_TCC) */
		if (IS_ERR(hsotg->uphy)) {
			ret = PTR_ERR(hsotg->uphy);
			switch (ret) {
			case -ENODEV:
			case -ENXIO:
				hsotg->uphy = NULL;
				break;
			case -EPROBE_DEFER:
				return ret;
			default:
				dev_err(hsotg->dev, "error getting usb phy %d\n",
					ret);
				return ret;
			}
		}
	}

	hsotg->plat = dev_get_platdata(hsotg->dev);

#if defined(CONFIG_USB_DWC2_TCC)
#if defined(CONFIG_USB_DWC2_TCC_MUX)
	hsotg->mux_uphy = devm_usb_get_phy_by_phandle(hsotg->dev,
			"usb-phy", 1);
	if (IS_ERR(hsotg->mux_uphy)) {
		ret = PTR_ERR(hsotg->mux_uphy);
		switch (ret) {
		case -ENODEV:
		case -ENXIO:
			hsotg->mux_uphy = NULL;
			break;
		case -EPROBE_DEFER:
			return ret;
		default:
			dev_err(hsotg->dev, "[ERROR][USB] error getting mux host usb phy %d\n",
					ret);
			return ret;
		}
	}

	if ((hsotg->uphy == NULL) || (hsotg->mux_uphy == NULL)) {
		dev_err(hsotg->dev, "[ERROR][USB] No such device of usb phy or mux host usb phy\n");
		return 0;
	}
#endif /* CONFIG_USB_DWC2_TCC_MUX */

	tcc_dwc_otg = container_of(hsotg->uphy, struct tcc_dwc_otg_phy, uphy);

	tcc_dwc_otg->vbus_supply =
		devm_regulator_get_optional(tcc_dwc_otg->dev, "vbus");
	if (IS_ERR(tcc_dwc_otg->vbus_supply)) {
		dev_err(tcc_dwc_otg->dev, "[ERROR][USB] VBus Supply is not valid.\n");
		return PTR_ERR(tcc_dwc_otg->vbus_supply);
	}
#else /* CONFIG_USB_DWC2_TCC */

	/* Clock */
	hsotg->clk = devm_clk_get_optional(hsotg->dev, "otg");
	if (IS_ERR(hsotg->clk)) {
		dev_err(hsotg->dev, "cannot get otg clock\n");
		return PTR_ERR(hsotg->clk);
	}

	/* Regulators */
	for (i = 0; i < ARRAY_SIZE(hsotg->supplies); i++)
		hsotg->supplies[i].supply = dwc2_hsotg_supply_names[i];

	ret = devm_regulator_bulk_get(hsotg->dev, ARRAY_SIZE(hsotg->supplies),
				      hsotg->supplies);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(hsotg->dev, "failed to request supplies: %d\n",
				ret);
		return ret;
	}
#endif /* !defined(CONFIG_USB_DWC2_TCC) */

	return 0;
}

/**
 * dwc2_driver_remove() - Called when the DWC_otg core is unregistered with the
 * DWC_otg driver
 *
 * @dev: Platform device
 *
 * This routine is called, for example, when the rmmod command is executed. The
 * device may or may not be electrically present. If it is present, the driver
 * stops device processing. Any resources used on behalf of this device are
 * freed.
 */
static int dwc2_driver_remove(struct platform_device *dev)
{
	struct dwc2_hsotg *hsotg = platform_get_drvdata(dev);

	dwc2_debugfs_exit(hsotg);
	if (hsotg->hcd_enabled)
#if defined(CONFIG_USB_DWC2_TCC_MUX)
		dwc2_tcc_mux_hcd_remove(hsotg);
#else /* CONFIG_USB_DWC2_TCC_MUX */
		dwc2_hcd_remove(hsotg);
#endif /* !defined(CONFIG_USB_DWC2_TCC_MUX) */
	if (hsotg->gadget_enabled)
		dwc2_hsotg_remove(hsotg);

	dwc2_drd_exit(hsotg);

	if (hsotg->params.activate_stm_id_vb_detection)
		regulator_disable(hsotg->usb33d);

	if (hsotg->ll_hw_enabled)
		dwc2_lowlevel_hw_disable(hsotg);

	reset_control_assert(hsotg->reset);
	reset_control_assert(hsotg->reset_ecc);

#if defined(CONFIG_USB_DWC2_TCC)
#if defined(CONFIG_USB_DWC2_DUAL_ROLE)
	cancel_work_sync(&hsotg->wf_otg);
	destroy_workqueue(hsotg->wq_otg);

	device_remove_file(&dev->dev, &dev_attr_dwc2_drdmode);
#endif /* CONFIG_USB_DWC2_DUAL_ROLE */

	device_remove_file(&dev->dev, &dev_attr_dwc2_vbus);
	device_remove_file(&dev->dev, &dev_attr_dwc2_host_testmode);
	device_remove_file(&dev->dev, &dev_attr_dwc2_dev_testmode);
	device_remove_file(&dev->dev, &dev_attr_dwc2_host_pcfg);
	device_remove_file(&dev->dev, &dev_attr_dwc2_dev_pcfg);
#endif /* CONFIG_USB_DWC2_TCC */

	return 0;
}

/**
 * dwc2_driver_shutdown() - Called on device shutdown
 *
 * @dev: Platform device
 *
 * In specific conditions (involving usb hubs) dwc2 devices can create a
 * lot of interrupts, even to the point of overwhelming devices running
 * at low frequencies. Some devices need to do special clock handling
 * at shutdown-time which may bring the system clock below the threshold
 * of being able to handle the dwc2 interrupts. Disabling dwc2-irqs
 * prevents reboots/poweroffs from getting stuck in such cases.
 */
static void dwc2_driver_shutdown(struct platform_device *dev)
{
	struct dwc2_hsotg *hsotg = platform_get_drvdata(dev);

#if defined(CONFIG_USB_DWC2_TCC)
	if (hsotg->ll_hw_enabled) {
		dwc2_lowlevel_hw_disable(hsotg);
	}

#if defined(CONFIG_USB_DWC2_TCC_MUX)
	disable_irq(hsotg->mux_irq);
#endif /* CONFIG_USB_DWC2_TCC_MUX */
#endif /* CONFIG_USB_DWC2_TCC */

	dwc2_disable_global_interrupts(hsotg);
	synchronize_irq(hsotg->irq);
}

/**
 * dwc2_check_core_endianness() - Returns true if core and AHB have
 * opposite endianness.
 * @hsotg:	Programming view of the DWC_otg controller.
 */
static bool dwc2_check_core_endianness(struct dwc2_hsotg *hsotg)
{
	u32 snpsid;

	snpsid = ioread32(hsotg->regs + GSNPSID);
	if ((snpsid & GSNPSID_ID_MASK) == DWC2_OTG_ID ||
	    (snpsid & GSNPSID_ID_MASK) == DWC2_FS_IOT_ID ||
	    (snpsid & GSNPSID_ID_MASK) == DWC2_HS_IOT_ID)
		return false;
	return true;
}

/**
 * Check core version
 *
 * @hsotg: Programming view of the DWC_otg controller
 *
 */
int dwc2_check_core_version(struct dwc2_hsotg *hsotg)
{
	struct dwc2_hw_params *hw = &hsotg->hw_params;

	/*
	 * Attempt to ensure this device is really a DWC_otg Controller.
	 * Read and verify the GSNPSID register contents. The value should be
	 * 0x45f4xxxx, 0x5531xxxx or 0x5532xxxx
	 */

	hw->snpsid = dwc2_readl(hsotg, GSNPSID);
	if ((hw->snpsid & GSNPSID_ID_MASK) != DWC2_OTG_ID &&
	    (hw->snpsid & GSNPSID_ID_MASK) != DWC2_FS_IOT_ID &&
	    (hw->snpsid & GSNPSID_ID_MASK) != DWC2_HS_IOT_ID) {
		dev_err(hsotg->dev, "Bad value for GSNPSID: 0x%08x\n",
			hw->snpsid);
		return -ENODEV;
	}

	dev_dbg(hsotg->dev, "Core Release: %1x.%1x%1x%1x (snpsid=%x)\n",
		hw->snpsid >> 12 & 0xf, hw->snpsid >> 8 & 0xf,
		hw->snpsid >> 4 & 0xf, hw->snpsid & 0xf, hw->snpsid);
	return 0;
}

/**
 * dwc2_driver_probe() - Called when the DWC_otg core is bound to the DWC_otg
 * driver
 *
 * @dev: Platform device
 *
 * This routine creates the driver components required to control the device
 * (core, HCD, and PCD) and initializes the device. The driver components are
 * stored in a dwc2_hsotg structure. A reference to the dwc2_hsotg is saved
 * in the device private data. This allows the driver to access the dwc2_hsotg
 * structure on subsequent calls to driver methods for this device.
 */
static int dwc2_driver_probe(struct platform_device *dev)
{
	struct dwc2_hsotg *hsotg;
	struct resource *res;
	int retval;
#if defined(CONFIG_USB_DWC2_TCC_MUX)
	u8 i;
	struct resource *hci_res[3];
#endif /* CONFIG_USB_DWC2_TCC_MUX */

	hsotg = devm_kzalloc(&dev->dev, sizeof(*hsotg), GFP_KERNEL);
	if (!hsotg)
		return -ENOMEM;

	hsotg->dev = &dev->dev;

	/*
	 * Use reasonable defaults so platforms don't have to provide these.
	 */
	if (!dev->dev.dma_mask)
		dev->dev.dma_mask = &dev->dev.coherent_dma_mask;
	retval = dma_set_coherent_mask(&dev->dev, DMA_BIT_MASK(32));
	if (retval) {
		dev_err(&dev->dev, "can't set coherent DMA mask: %d\n", retval);
		return retval;
	}

	hsotg->regs = devm_platform_get_and_ioremap_resource(dev, 0, &res);
	if (IS_ERR(hsotg->regs))
		return PTR_ERR(hsotg->regs);

	dev_dbg(&dev->dev, "mapped PA %08lx to VA %p\n",
		(unsigned long)res->start, hsotg->regs);

#if defined(CONFIG_USB_DWC2_TCC_MUX)
	// Get EHCI MUX and OHCI MUX register base
	for (i = 1; i < 3; i++) {
		hci_res[i] = platform_get_resource(dev, IORESOURCE_MEM, i);
		if (hci_res[i] == NULL) {
			return -ENODEV;
		}

		hsotg->base[i] = hci_res[i]->start;
		hsotg->size[i] = resource_size(hci_res[i]);

		dev_info(&dev->dev, "[INFO][USB] %s mapped PA %08llx to VA %p, size %lld\n",
				(i == 1) ? "EHCI" : "OHCI", (long long int)hsotg->base[i],
				&hsotg->base[i], (long long int)hsotg->size[i]);
	}
#endif /* CONFIG_USB_DWC2_TCC_MUX */

	retval = dwc2_lowlevel_hw_init(hsotg);
	if (retval)
		return retval;

	spin_lock_init(&hsotg->lock);

	hsotg->irq = platform_get_irq(dev, 0);
	if (hsotg->irq < 0)
		return hsotg->irq;

	dev_dbg(hsotg->dev, "registering common handler for irq%d\n",
		hsotg->irq);

#if defined(CONFIG_USB_DWC2_TCC)
	// Set IRQ affinity to handle the IRQ more stably
	retval = irq_set_affinity_hint(hsotg->irq, cpumask_of(1));
	if (retval) {
		dev_err(&dev->dev, "[ERROR][USB] Setting IRQ affinity FAIL!, cpu: 1 irq: %d err: %d\n",
				hsotg->irq, retval);
		return retval;
	}

#if defined(CONFIG_USB_DWC2_TCC_MUX)
	hsotg->mux_irq = platform_get_irq(dev, 1);
	if (hsotg->mux_irq < 0) {
		dev_err(&dev->dev, "[ERROR][USB] Getting MUX host IRQ FAIL!\n");
		return hsotg->mux_irq;
	}

	dev_dbg(hsotg->dev, "[DEBUG][USB] registering common handler for MUX host irq%d\n",
			hsotg->mux_irq);
#endif /* CONFIG_USB_DWC2_TCC_MUX */
#endif /* CONFIG_USB_DWC2_TCC */

	retval = devm_request_irq(hsotg->dev, hsotg->irq,
				  dwc2_handle_common_intr, IRQF_SHARED,
				  dev_name(hsotg->dev), hsotg);
	if (retval)
		return retval;

	hsotg->vbus_supply = devm_regulator_get_optional(hsotg->dev, "vbus");
	if (IS_ERR(hsotg->vbus_supply)) {
		retval = PTR_ERR(hsotg->vbus_supply);
		hsotg->vbus_supply = NULL;
		if (retval != -ENODEV)
			return retval;
	}

	retval = dwc2_lowlevel_hw_enable(hsotg);
	if (retval)
		return retval;

	hsotg->needs_byte_swap = dwc2_check_core_endianness(hsotg);

	retval = dwc2_get_dr_mode(hsotg);
	if (retval)
		goto error;

	hsotg->need_phy_for_wake =
		of_property_read_bool(dev->dev.of_node,
				      "snps,need-phy-for-wake");

	/*
	 * Before performing any core related operations
	 * check core version.
	 */
	retval = dwc2_check_core_version(hsotg);
	if (retval)
		goto error;

	/*
	 * Reset before dwc2_get_hwparams() then it could get power-on real
	 * reset value form registers.
	 */
	retval = dwc2_core_reset(hsotg, false);
	if (retval)
		goto error;

	/* Detect config values from hardware */
	retval = dwc2_get_hwparams(hsotg);
	if (retval)
		goto error;

	/*
	 * For OTG cores, set the force mode bits to reflect the value
	 * of dr_mode. Force mode bits should not be touched at any
	 * other time after this.
	 */
	dwc2_force_dr_mode(hsotg);

	retval = dwc2_init_params(hsotg);
	if (retval)
		goto error;

	if (hsotg->params.activate_stm_id_vb_detection) {
		u32 ggpio;

		hsotg->usb33d = devm_regulator_get(hsotg->dev, "usb33d");
		if (IS_ERR(hsotg->usb33d)) {
			retval = PTR_ERR(hsotg->usb33d);
			if (retval != -EPROBE_DEFER)
				dev_err(hsotg->dev,
					"failed to request usb33d supply: %d\n",
					retval);
			goto error;
		}
		retval = regulator_enable(hsotg->usb33d);
		if (retval) {
			dev_err(hsotg->dev,
				"failed to enable usb33d supply: %d\n", retval);
			goto error;
		}

		ggpio = dwc2_readl(hsotg, GGPIO);
		ggpio |= GGPIO_STM32_OTG_GCCFG_IDEN;
		ggpio |= GGPIO_STM32_OTG_GCCFG_VBDEN;
		dwc2_writel(hsotg, ggpio, GGPIO);

		/* ID/VBUS detection startup time */
		usleep_range(5000, 7000);
	}

	retval = dwc2_drd_init(hsotg);
	if (retval) {
		if (retval != -EPROBE_DEFER)
			dev_err(hsotg->dev, "failed to initialize dual-role\n");
		goto error_init;
	}

	if (hsotg->dr_mode != USB_DR_MODE_HOST) {
		retval = dwc2_gadget_init(hsotg);
		if (retval)
			goto error_drd;
		hsotg->gadget_enabled = 1;
	}

	/*
	 * If we need PHY for wakeup we must be wakeup capable.
	 * When we have a device that can wake without the PHY we
	 * can adjust this condition.
	 */
	if (hsotg->need_phy_for_wake)
		device_set_wakeup_capable(&dev->dev, true);

	hsotg->reset_phy_on_wake =
		of_property_read_bool(dev->dev.of_node,
				      "snps,reset-phy-on-wake");
	if (hsotg->reset_phy_on_wake && !hsotg->phy) {
		dev_warn(hsotg->dev,
			 "Quirk reset-phy-on-wake only supports generic PHYs\n");
		hsotg->reset_phy_on_wake = false;
	}

	if (hsotg->dr_mode != USB_DR_MODE_PERIPHERAL) {
#if defined(CONFIG_USB_DWC2_TCC_MUX)
		retval = dwc2_tcc_mux_hcd_init(hsotg);
#else /* CONFIG_USB_DWC2_TCC_MUX */
		retval = dwc2_hcd_init(hsotg);
#endif /* !defined(CONFIG_USB_DWC2_TCC_MUX) */
		if (retval) {
			if (hsotg->gadget_enabled)
				dwc2_hsotg_remove(hsotg);
			goto error_drd;
		}
		hsotg->hcd_enabled = 1;
	}

	platform_set_drvdata(dev, hsotg);
	hsotg->hibernated = 0;

	if (hsotg->dr_mode != USB_DR_MODE_HOST)
		dwc2_debugfs_init(hsotg);

	/* Gadget code manages lowlevel hw on its own */
	if (hsotg->dr_mode == USB_DR_MODE_PERIPHERAL)
		dwc2_lowlevel_hw_disable(hsotg);

#if defined(CONFIG_USB_DWC2_TCC)
#if defined(CONFIG_USB_DWC2_DUAL_ROLE)
	hsotg->wq_otg = create_singlethread_workqueue("dwc2");
	if (!hsotg->wq_otg)
		goto error;

	INIT_WORK(&hsotg->wf_otg, dwc2_tcc_set_drdmode);

	retval = device_create_file(&dev->dev, &dev_attr_dwc2_drdmode);
	if (retval)
		dev_err(hsotg->dev, "[ERROR][USB] Failed to create dwc2_drdmode sysfs\n");
#endif /* CONFIG_USB_DWC2_DUAL_ROLE */

	retval = device_create_file(&dev->dev, &dev_attr_dwc2_vbus);
	if (retval)
		dev_err(hsotg->dev, "[ERROR][USB] Failed to create dwc2_vbus sysfs\n");

	retval = device_create_file(&dev->dev, &dev_attr_dwc2_host_testmode);
	if (retval)
		dev_err(hsotg->dev, "[ERROR][USB] Failed to create dwc2_host_testmode sysfs\n");

	retval = device_create_file(&dev->dev, &dev_attr_dwc2_dev_testmode);
	if (retval)
		dev_err(hsotg->dev, "[ERROR][USB] Failed to create dwc2_dev_testmode sysfs\n");

	retval = device_create_file(&dev->dev, &dev_attr_dwc2_host_pcfg);
	if (retval)
		dev_err(hsotg->dev, "[ERROR][USB] Failed to create dwc2_host_pcfg sysfs\n");

	retval = device_create_file(&dev->dev, &dev_attr_dwc2_dev_pcfg);
	if (retval)
		dev_err(hsotg->dev, "[ERROR][USB] Failed to create dwc2_dev_pcfg sysfs\n");
#endif /* CONFIG_USB_DWC2_TCC */

#if IS_ENABLED(CONFIG_USB_DWC2_PERIPHERAL) || \
	IS_ENABLED(CONFIG_USB_DWC2_DUAL_ROLE)
	/* Postponed adding a new gadget to the udc class driver list */
	if (hsotg->gadget_enabled) {
		retval = usb_add_gadget_udc(hsotg->dev, &hsotg->gadget);
		if (retval) {
			hsotg->gadget.udc = NULL;
			dwc2_hsotg_remove(hsotg);
			goto error_debugfs;
		}
	}
#endif /* CONFIG_USB_DWC2_PERIPHERAL || CONFIG_USB_DWC2_DUAL_ROLE */
	return 0;

#if IS_ENABLED(CONFIG_USB_DWC2_PERIPHERAL) || \
	IS_ENABLED(CONFIG_USB_DWC2_DUAL_ROLE)
error_debugfs:
	dwc2_debugfs_exit(hsotg);
	if (hsotg->hcd_enabled)
		dwc2_hcd_remove(hsotg);
#endif
error_drd:
	dwc2_drd_exit(hsotg);

error_init:
	if (hsotg->params.activate_stm_id_vb_detection)
		regulator_disable(hsotg->usb33d);
error:
	if (hsotg->ll_hw_enabled)
		dwc2_lowlevel_hw_disable(hsotg);
	return retval;
}

static int __maybe_unused dwc2_suspend(struct device *dev)
{
	struct dwc2_hsotg *dwc2 = dev_get_drvdata(dev);
	bool is_device_mode = dwc2_is_device_mode(dwc2);
	int ret = 0;

	if (is_device_mode)
		dwc2_hsotg_suspend(dwc2);

	dwc2_drd_suspend(dwc2);

	if (dwc2->params.activate_stm_id_vb_detection) {
		unsigned long flags;
		u32 ggpio, gotgctl;

		/*
		 * Need to force the mode to the current mode to avoid Mode
		 * Mismatch Interrupt when ID detection will be disabled.
		 */
		dwc2_force_mode(dwc2, !is_device_mode);

		spin_lock_irqsave(&dwc2->lock, flags);
		gotgctl = dwc2_readl(dwc2, GOTGCTL);
		/* bypass debounce filter, enable overrides */
		gotgctl |= GOTGCTL_DBNCE_FLTR_BYPASS;
		gotgctl |= GOTGCTL_BVALOEN | GOTGCTL_AVALOEN;
		/* Force A / B session if needed */
		if (gotgctl & GOTGCTL_ASESVLD)
			gotgctl |= GOTGCTL_AVALOVAL;
		if (gotgctl & GOTGCTL_BSESVLD)
			gotgctl |= GOTGCTL_BVALOVAL;
		dwc2_writel(dwc2, gotgctl, GOTGCTL);
		spin_unlock_irqrestore(&dwc2->lock, flags);

		ggpio = dwc2_readl(dwc2, GGPIO);
		ggpio &= ~GGPIO_STM32_OTG_GCCFG_IDEN;
		ggpio &= ~GGPIO_STM32_OTG_GCCFG_VBDEN;
		dwc2_writel(dwc2, ggpio, GGPIO);

		regulator_disable(dwc2->usb33d);
	}

	if (dwc2->ll_hw_enabled &&
	    (is_device_mode || dwc2_host_can_poweroff_phy(dwc2))) {
		ret = __dwc2_lowlevel_hw_disable(dwc2);
		dwc2->phy_off_for_suspend = true;
	}

	return ret;
}

static int __maybe_unused dwc2_resume(struct device *dev)
{
	struct dwc2_hsotg *dwc2 = dev_get_drvdata(dev);
	int ret = 0;

	if (dwc2->phy_off_for_suspend && dwc2->ll_hw_enabled) {
		ret = __dwc2_lowlevel_hw_enable(dwc2);
		if (ret)
			return ret;
	}
	dwc2->phy_off_for_suspend = false;

	if (dwc2->params.activate_stm_id_vb_detection) {
		unsigned long flags;
		u32 ggpio, gotgctl;

		ret = regulator_enable(dwc2->usb33d);
		if (ret)
			return ret;

		ggpio = dwc2_readl(dwc2, GGPIO);
		ggpio |= GGPIO_STM32_OTG_GCCFG_IDEN;
		ggpio |= GGPIO_STM32_OTG_GCCFG_VBDEN;
		dwc2_writel(dwc2, ggpio, GGPIO);

		/* ID/VBUS detection startup time */
		usleep_range(5000, 7000);

		spin_lock_irqsave(&dwc2->lock, flags);
		gotgctl = dwc2_readl(dwc2, GOTGCTL);
		gotgctl &= ~GOTGCTL_DBNCE_FLTR_BYPASS;
		gotgctl &= ~(GOTGCTL_BVALOEN | GOTGCTL_AVALOEN |
			     GOTGCTL_BVALOVAL | GOTGCTL_AVALOVAL);
		dwc2_writel(dwc2, gotgctl, GOTGCTL);
		spin_unlock_irqrestore(&dwc2->lock, flags);
	}

	/* Need to restore FORCEDEVMODE/FORCEHOSTMODE */
	dwc2_force_dr_mode(dwc2);

	dwc2_drd_resume(dwc2);

	if (dwc2_is_device_mode(dwc2))
		ret = dwc2_hsotg_resume(dwc2);

	return ret;
}

static const struct dev_pm_ops dwc2_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dwc2_suspend, dwc2_resume)
};

static struct platform_driver dwc2_platform_driver = {
	.driver = {
		.name = dwc2_driver_name,
		.of_match_table = dwc2_of_match_table,
		.pm = &dwc2_dev_pm_ops,
	},
	.probe = dwc2_driver_probe,
	.remove = dwc2_driver_remove,
	.shutdown = dwc2_driver_shutdown,
};

module_platform_driver(dwc2_platform_driver);

MODULE_DESCRIPTION("DESIGNWARE HS OTG Platform Glue");
MODULE_AUTHOR("Matthijs Kooijman <matthijs@stdin.nl>");
MODULE_LICENSE("Dual BSD/GPL");
