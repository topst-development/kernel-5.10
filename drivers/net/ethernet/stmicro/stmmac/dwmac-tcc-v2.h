// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCA_GMAC_H_
#define TCA_GMAC_H_

#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/phy.h>
#include <linux/delay.h>

#include <linux/stmmac.h>

#define MAC_MDIO_DATA_RA_SHIFT	(16)
#define MAC_MDIO_DATA_RA_MASK	(0xFFFF << MAC_MDIO_DATA_RA_SHIFT)
#define MAC_MDIO_DATA_RDA_SHIFT	(16)
#define MAC_MDIO_DATA_RDA_MASK	(0x1F << MAC_MDIO_DATA_RA_SHIFT)

/*
 * Below FQTSS configuration is legacy configuration for compatibility
 * with tc-ehal. It is not meaningful.
 */

#define IOCTL_AVB_CONFIG (SIOCDEVPRIVATE + 1)
#define MAX_INTERFERENCE_SIZE (16384 * 8)
#define MAX_FRAME_SIZE (1514 * 8)
#define CLASS_A_BW 384
#define CLASS_A_PRIO 3
#define CLASS_B_BW 192
#define CLASS_B_PRIO 2

// IOCTL AVB COMMNAD
enum avb_cmd_t {
	AVB_CMD_SET_CLASS_A_PRIORITY = 0x00000001,
	AVB_CMD_SET_CLASS_B_PRIORITY = 0x00000002,

	AVB_CMD_GET_CLASS_A_PRIORITY = 0x00000003,
	AVB_CMD_GET_CLASS_B_PRIORITY = 0x00000004,

	AVB_CMD_SET_CLASS_A_BANDWIDTH = 0x00000005,
	AVB_CMD_SET_CLASS_B_BANDWIDTH = 0x00000006,

	AVB_CMD_GET_CLASS_A_BANDWIDTH = 0x00000007,
	AVB_CMD_GET_CLASS_B_BANDWIDTH = 0x00000008,
};

struct ifr_avb_config_t {
	u32 cmd;
	u32 data;
};

struct tcc_dwmac {
	struct device dev;
	phy_interface_t phy_interface;
	void __iomem *cfg_block;
	void __iomem *ecc_block;
	void __iomem *gmac_block;
	void __iomem *ecid_block;

	struct clk *hsio_clk;
	struct clk *gmac_clk;
	struct clk *ptp_clk;
	struct clk *gmac_hclk;
	u32 phy_on;
	u32 phy_rst;
	u32 txclk_i_dly;
	u32 txclk_i_inv;
	u32 txclk_o_dly;
	u32 txclk_o_inv;
	u32 txen_dly;
	u32 txer_dly;
	u32 txd0_dly;
	u32 txd1_dly;
	u32 txd2_dly;
	u32 txd3_dly;
	u32 txd4_dly;
	u32 txd5_dly;
	u32 txd6_dly;
	u32 txd7_dly;
	u32 rxclk_i_dly;
	u32 rxclk_i_inv;
	u32 rxdv_dly;
	u32 rxer_dly;
	u32 rxd0_dly;
	u32 rxd1_dly;
	u32 rxd2_dly;
	u32 rxd3_dly;
	u32 rxd4_dly;
	u32 rxd5_dly;
	u32 rxd6_dly;
	u32 rxd7_dly;
	u32 crs_dly;
	u32 col_dly;

	// Clause 45 register
	unsigned int c45_dev_addr_shift;
	unsigned int c45_dev_addr_mask;
	unsigned int c45_reg_addr_shift;
	unsigned int c45_reg_addr_mask;

	bool use_ecid_mac_addr;
	bool rmii_tx_clk_off;
	bool rmii_rx_clk_ext;
	bool no_hsio;
#ifdef CONFIG_DEBUG_FS
	struct dentry *dbgfs_dir;
#endif
	bool fqtss_enable;
};

/* Memory ECC Control Feature */
#ifdef GMAC_ECC_TEST_FEATURE

#define GMAC_ECC_PASSWORD	(0x5AFEACE5)
#define GMAC_ECC_TEEE		(0x0)
#define GMAC_ECC_TEECE		(0x4)
#define GMAC_ECC_TEESC		(0x8)
#define GMAC_ECC_TEDM0		(0xc)
#define GMAC_ECC_TEDM1		(0x10)
#define GMAC_ECC_TEDM2		(0x14)
#define GMAC_ECC_TEEM		(0x18)
#define GMAC_ECC_TEEFRC		(0x1c)
#define GMAC_ECC_TEERFS		(0x20)
#define GMAC_ECC_TEEFS		(0x24)
#define GMAC_ECC_TEEFA		(0x28)
#define GMAC_ECC_TEFS		(0x2C)
#define GMAC_ECC_TOEE		(0x30)
#define GMAC_ECC_TOECE		(0x34)
#define GMAC_ECC_TOESC		(0x38)
#define GMAC_ECC_TODM0		(0x3C)
#define GMAC_ECC_TODM1		(0x40)
#define GMAC_ECC_TODM2		(0x44)
#define GMAC_ECC_TOEM		(0x48)
#define GMAC_ECC_TOEFRC		(0x4C)
#define GMAC_ECC_TOERFS		(0x50)
#define GMAC_ECC_TOEFS		(0x54)
#define GMAC_ECC_TOFA		(0x58)
#define GMAC_ECC_TOFS		(0x5C)
#define GMAC_ECC_REEE		(0x60)
#define GMAC_ECC_REECE		(0x64)
#define GMAC_ECC_REESC		(0x68)
#define GMAC_ECC_REDM0		(0x6C)
#define GMAC_ECC_REDM1		(0x70)
#define GMAC_ECC_REDM2		(0x74)
#define GMAC_ECC_REEM		(0x78)
#define GMAC_ECC_REEFRC		(0x7C)
#define GMAC_ECC_REERFS		(0x80)
#define GMAC_ECC_REEEFS		(0x84)
#define GMAC_ECC_REFA		(0x88)
#define GMAC_ECC_REFS		(0x8C)
#define GMAC_ECC_ROEE		(0x90)
#define GMAC_ECC_ROECE		(0x94)
#define GMAC_ECC_ROESC		(0x98)
#define GMAC_ECC_RODM0		(0x9C)
#define GMAC_ECC_RODM1		(0xA0)
#define GMAC_ECC_RODM2		(0xA4)
#define GMAC_ECC_ROEM		(0xA8)
#define GMAC_ECC_ROEFRC		(0xAC)
#define GMAC_ECC_ROERFS		(0xB0)
#define GMAC_ECC_ROEFS		(0xB4)
#define GMAC_ECC_ROFA		(0xB8)
#define GMAC_ECC_ROFS		(0xBC)
#define GMAC_ECC_DCEE		(0xC0)
#define GMAC_ECC_DCECE		(0xC4)
#define GMAC_ECC_DCESC		(0xC8)
#define GMAC_ECC_DCDM0		(0xCC)
#define GMAC_ECC_DCDM1		(0xD0)
#define GMAC_ECC_DCDM2		(0xD4)
#define GMAC_ECC_PW			(0xF0)
#define GMAC_ECC_LOCK		(0xF4)
#define GMAC_ECC_SFE0		(0xF8)
#define GMAC_ECC_SFE1		(0xFC)
#define GMAC_ECC_SFS0		(0x100)
#define GMAC_ECC_SFS1		(0x104)
#define GMAC_ECC_SFC		(0x108)
#define GMAC_ECC_SFCS		(0x10C)

#define DUMMY_VALUE	(0xa)

void tcc_set_dummy_ecc_err(struct tcc_dwmac *gmac);
void tcc_enable_ecc_event(struct tcc_dwmac *gmac, int event_num);
u32 tcc_get_ecc_status(struct tcc_dwmac *gmac);
#endif

void tcc_dwmac_clk_enable(const struct tcc_dwmac *priv);
int tcc_dwmac_init(struct platform_device *plat_dev, void *priv);
void tcc_dwmac_phy_reset(const struct tcc_dwmac *priv);
void tcc_dwmac_tuning_timing(const struct tcc_dwmac *priv);
void tcc_dwmac_set_phy_interface(
	struct platform_device *plat_dev, const struct tcc_dwmac *gmac);
#ifdef CONFIG_DEBUG_FS
void tcc_dwmac_debugfs_init(struct tcc_dwmac *gmac);
void tcc_dwmac_debugfs_exit(struct tcc_dwmac *gmac);
#endif
#endif /*TCA_GMAC_H_*/
