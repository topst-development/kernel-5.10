/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef LINUX_DWC3_TCC_H
#define LINUX_DWC3_TCC_H

#include <linux/usb/tcc.h>
#include <linux/usb/tcc-phy.h>

struct dwc3_tcc_hcd {
	struct device		*dev;
	struct platform_device	*dwc3;
	struct usb_phy		*uphy;
	struct U30_PHY __iomem	*phy_regs;

	int32_t			vbus_status;
};

extern struct pcfg_field old_fpcfg_field[PCFG_MAX];
extern struct pcfg_field new_fpcfg_field[PCFG_MAX];

static void dwc3_tcc_vbus_ctrl(struct dwc3_tcc_hcd *dwc3_tcc,
		int32_t on_off);
static void dwc3_tcc_vbus_ctrl_on(struct dwc3_tcc_hcd *dwc3_tcc);
static void dwc3_tcc_vbus_ctrl_off(struct dwc3_tcc_hcd *dwc3_tcc);
static ssize_t dwc3_vbus_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t dwc3_vbus_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t dwc3_drdmode_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t dwc3_drdmode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static void dwc3_tcc_set_test_mode(struct dwc3 *dwc, u32 mode);
static ssize_t dwc3_testmode_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t dwc3_testmode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static void dwc3_tcc_display_pcfg(uint32_t old_reg, uint32_t new_reg);
static ssize_t dwc3_pcfg_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t dwc3_pcfg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static int32_t dwc3_tcc_set_vbus_resource(struct usb_phy *uphy);
static int32_t dwc3_tcc_remove_child(struct device *dev, void *unused);
static int32_t dwc3_tcc_phy_register(struct dwc3_tcc_hcd *dwc3_tcc);
static void dwc3_tcc_phy_unregister(const struct dwc3_tcc_hcd *dwc3_tcc);

static int32_t dwc3_tcc_probe(struct platform_device *pdev);
static int32_t dwc3_tcc_remove(struct platform_device *pdev);

#if defined(CONFIG_PM_SLEEP)
static int32_t dwc3_tcc_suspend(struct device *dev);
static int32_t dwc3_tcc_resume(struct device *dev);
#endif /* CONFIG_PM_SLEEP */
#endif /* LINUX_DWC3_TCC_H */
