/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef LINUX_EHCI_TCC_H
#define LINUX_EHCI_TCC_H

#include <linux/usb/tcc.h>
#include <linux/usb/tcc-phy.h>

#define DRIVER_DESC "EHCI Telechips driver"

static struct hc_driver __read_mostly tcc_ehci_hc_driver;

struct tcc_ehci_hcd {
	struct device	*dev;
	struct ehci_hcd	*ehci;
	struct usb_phy	*uphy;

	struct USB20H_PHY __iomem	*phy_regs;

	int32_t		vbus_status;
	unsigned	hcd_tpl_support:1;
};

extern struct pcfg_field old_pcfg_field[PCFG_MAX];
extern struct pcfg_field new_pcfg_field[PCFG_MAX];

static ssize_t ehci_vbus_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t ehci_vbus_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t ehci_testmode_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t ehci_testmode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static void tcc_ehci_display_pcfg(uint32_t old_reg, uint32_t new_reg);
static ssize_t ehci_pcfg_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t ehci_pcfg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static int32_t tcc_ehci_parse_dt(struct platform_device *pdev,
		struct tcc_ehci_hcd *tcc_ehci);
static int32_t tcc_ehci_ctrl_phy(const struct tcc_ehci_hcd *tcc_ehci,
		int32_t on_off);
static int32_t tcc_ehci_init_phy(const struct tcc_ehci_hcd *tcc_ehci);
static int32_t tcc_ehci_ctrl_vbus(struct tcc_ehci_hcd *tcc_ehci,
		int32_t on_off);
static void tcc_ehci_ctrl_phy_and_vbus(struct tcc_ehci_hcd *tcc_ehci,
		int32_t on_off);

static int32_t tcc_ehci_probe(struct platform_device *pdev);
static int32_t tcc_ehci_remove(struct platform_device *pdev);
static void tcc_ehci_hcd_shutdown(struct platform_device *pdev);

static int32_t tcc_ehci_suspend(struct device *dev);
static int32_t tcc_ehci_resume(struct device *dev);

static int32_t __init tcc_ehci_init(void);
static void __exit tcc_ehci_cleanup(void);

#endif /* LINUX_EHCI_TCC_H */
