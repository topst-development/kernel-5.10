/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef LINUX_OHCI_TCC_H
#define LINUX_OHCI_TCC_H

#include <linux/usb/tcc.h>

#define DRIVER_DESC "OHCI Telechips driver"

static struct hc_driver __read_mostly tcc_ohci_hc_driver;

struct tcc_ohci_hcd {
	struct device	*dev;
	struct usb_hcd	*hcd;

	struct USB20H_PHY __iomem	*phy_regs;

	resource_size_t	phy_rsrc_start;
	resource_size_t	phy_rsrc_len;
	unsigned	hcd_tpl_support:1;
};

static void tcc_ohci_parse_dt(const struct platform_device *pdev,
		struct tcc_ohci_hcd *tcc_ohci);
static void tcc_ohci_ctrl_phy(const struct tcc_ohci_hcd *tcc_ohci, int32_t on_off);

static int32_t tcc_ohci_probe(struct platform_device *pdev);
static int32_t tcc_ohci_remove(struct platform_device *pdev);

static int32_t __init tcc_ohci_init(void);
static void __exit tcc_ohci_cleanup(void);

#endif /* LINUX_OHCI_TCC_H  */
