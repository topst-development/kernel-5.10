// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/of_address.h>
#include <linux/of_net.h>
#include <linux/if_ether.h>

#include "dwmac-tcc-ecid.h"

static void tcc_read_ecid(void __iomem *ecid_block, unsigned int ecid[])
{
	void __iomem *pgpio;
	unsigned int i;

	pgpio = ecid_block;

	writel(MODE, pgpio + TCC_ECID_OFFSET_0);
	writel(MODE | CS | TCC_ECID_CON_SELECT, pgpio + TCC_ECID_OFFSET_0);

	for (i = 0; i < 8U; i++) {
		writel(MODE | CS | (i << 17) | TCC_ECID_CON_SELECT,
		       pgpio + TCC_ECID_OFFSET_0);
		writel(MODE | CS | (i << 17) | TSIGDEV | TCC_ECID_CON_SELECT,
		       pgpio + TCC_ECID_OFFSET_0);
		writel(MODE | CS | (i << 17) | TSIGDEV | PRCHG
			       | TCC_ECID_CON_SELECT,
		       pgpio + TCC_ECID_OFFSET_0);
		writel(MODE | CS | (i << 17) | TSIGDEV | PRCHG | FSET
			       | TCC_ECID_CON_SELECT,
		       pgpio + TCC_ECID_OFFSET_0);
		writel(MODE | CS | (i << 17) | PRCHG | FSET
			       | TCC_ECID_CON_SELECT,
		       pgpio + TCC_ECID_OFFSET_0);
		writel(MODE | CS | (i << 17) | FSET | TCC_ECID_CON_SELECT,
		       pgpio + TCC_ECID_OFFSET_0);
		writel(MODE | CS | (i << 17) | TCC_ECID_CON_SELECT,
		       pgpio + TCC_ECID_OFFSET_0);
	}

	ecid[0] = readl(pgpio + TCC_ECID_OFFSET_2); // Low	32 Bit
	ecid[1] = readl(pgpio + TCC_ECID_OFFSET_3); // High 16 Bit

	writel(0x00000000, pgpio + TCC_ECID_OFFSET_0); // ECID Closed
}

unsigned char *tcc_read_mac_addr_from_ecid(void __iomem *ecid_block)
{
	unsigned int ecid[2] = {0, 0};
	unsigned int id_bit = 0;
	int ret = 0;
	unsigned char *mac_addr;

	mac_addr = (unsigned char *)kzalloc(ETH_ALEN, GFP_KERNEL);
	if (mac_addr == NULL) {
		ret = -ENOMEM;
		goto out_ecid;
	}
	tcc_read_ecid(ecid_block, ecid);

#if defined(CONFIG_ARCH_TCC750X) || defined(CONFIG_ARCH_TCC807X)
	id_bit = (ecid[0] >> 6) & TCC_ECID_MAC_ID_BIT_MASK;
#else
	id_bit = (ecid[0] >> 22) & TCC_ECID_MAC_ID_BIT_MASK;
#endif
	if ((ecid[0] != 0u) || (ecid[1] != 0u)) {
		if (id_bit == OUI_TCC_2011) {
			mac_addr[0] = (unsigned char)0xF4;
			mac_addr[1] = (unsigned char)0x50;
			mac_addr[2] = (unsigned char)0xEB;
		} else if (id_bit == OUI_TCC_2013) {
			mac_addr[0] = (unsigned char)0x88;
			mac_addr[1] = (unsigned char)0x46;
			mac_addr[2] = (unsigned char)0x2A;
		} else if (id_bit == OUI_TCC_2016) {
			mac_addr[0] = (unsigned char)0x3C;
			mac_addr[1] = (unsigned char)0x7F;
			mac_addr[2] = (unsigned char)0x6F;
		} else if (id_bit == OUI_TCC_2018) {
			mac_addr[0] = (unsigned char)0x7C;
			mac_addr[1] = (unsigned char)0x24;
			mac_addr[2] = (unsigned char)0x0C;
		} else {
			(void)pr_err(
				"[ERR][ECID] fail to set mac oui, need to check!\n");
		}

#if defined(CONFIG_ARCH_TCC750X) || defined(CONFIG_ARCH_TCC807X)
		mac_addr[3] = (unsigned char)((ecid[0] >> 24) & 0xFFu);
		mac_addr[4] = (unsigned char)((ecid[0] >> 16) & 0xFFu);
		mac_addr[5] = (unsigned char)((ecid[0] >> 8) & 0xFFu);
#else
		mac_addr[3] = (unsigned char)((ecid[1] >> 8) & 0xFFu);
		mac_addr[4] = (unsigned char)(ecid[1] & 0xFFu);
		mac_addr[5] = (unsigned char)((ecid[0] >> 24) & 0xFFu);
#endif
	} else {
		(void)pr_err("[ERR][ECID] fail to set mac address from ecid\n");
		ret = -EINVAL;
		goto out_ecid;
	}

out_ecid:
	if ((ret != 0) && (mac_addr != NULL)) {
		kfree(mac_addr);
		mac_addr = NULL;
	}
	return mac_addr;
}
