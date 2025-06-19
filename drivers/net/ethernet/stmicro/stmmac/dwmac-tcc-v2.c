// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/stmmac.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>

#include "stmmac.h"
#include "stmmac_platform.h"
#include "dwmac-tcc-v2.h"
#include "dwmac-tcc-ecid.h"

#define GMAC_RESET_CONTROL_REG		0
#define CLK_SRC_EXTERNAL 25

#if defined(CONFIG_ARCH_TCC750X)
#define  GMAC_CONFIG_INTF_SEL_MASK  (0x7000000u)
#define  GMAC_CONFIG_INTF_RGMII		(0x1000000u)
#define  GMAC_CONFIG_INTF_GMII		(0x0000000u)
#define  GMAC_CONFIG_INTF_RMII		(0x4000000u)
#define  GMAC_CONFIG_INTF_MII		(0x0000000u)
#define	 GMAC_CONFIG1_CE			(0x10000000u)
#define GMAC_SW_CONFIG0_REG			(0x0u)
#define GMAC_SW_CONFIG1_REG			(0x4u)
#elif defined(CONFIG_ARCH_TCC805X)
#define  GMAC_CONFIG_INTF_SEL_MASK  (0x700000u)
#define  GMAC_CONFIG_INTF_RGMII		(0x100000u)
#define  GMAC_CONFIG_INTF_GMII		(0x000000u)
#define  GMAC_CONFIG_INTF_RMII		(0x400000u)
#define  GMAC_CONFIG_INTF_MII		(0x000000u)
#define	 GMAC_CONFIG1_CE			(0x80000000u)
#define GMAC_SW_CONFIG0_REG			(0x0u)
#define GMAC_SW_CONFIG1_REG			(0x4u)
#elif defined(CONFIG_ARCH_TCC803X)
#define  GMAC_CONFIG_INTF_SEL_MASK	(0x1C0000u)
#define  GMAC_CONFIG_INTF_RGMII		(0x040000u)
#define  GMAC_CONFIG_INTF_GMII		(0x000000u)
#define  GMAC_CONFIG_INTF_RMII		(0x100000u)
#define  GMAC_CONFIG_INTF_MII		(0x180000u)
#define	 GMAC_CONFIG1_CE			(0x80000000u)
#define GMAC_SW_CONFIG0_REG			(0x0u)
#define GMAC_SW_CONFIG1_REG			(0x4u)
#elif defined(CONFIG_ARCH_TCC897X)
#define GMAC_CONFIG_INTF_SEL_MASK	(0x1C0000u)
#define GMAC_CONFIG_INTF_GMII		(0x000000u)
#define GMAC_CONFIG_INTF_MII		(0x000000u)
#define GMAC_CONFIG_INTF_RGMII		(0x40000u)
#define GMAC_CONFIG_INTF_RMII		(0x100000u)
#define	 GMAC_CONFIG1_CE			(0x80000000u)
#define GMAC_SW_CONFIG0_REG			(0x0u)
#define GMAC_SW_CONFIG1_REG			(0x4u)
#else
#define  GMAC_CONFIG_INTF_SEL_MASK  (0x700000u)
#define  GMAC_CONFIG_INTF_RGMII		(0x100000u)
#define  GMAC_CONFIG_INTF_GMII		(0x000000u)
#define  GMAC_CONFIG_INTF_RMII		(0x400000u)
#define  GMAC_CONFIG_INTF_MII		(0x600000u)
#define	 GMAC_CONFIG1_CE			(0x80000000u)
#define GMAC_SW_CONFIG0_REG			(0x0u)
#define GMAC_SW_CONFIG1_REG			(0x4u)
#endif

#if defined(CONFIG_ARCH_TCC750X)
#define GMAC_CONFIG0_TR			0x10000000u //(0x1 << 28)
#else
#define GMAC_CONFIG0_TR			0x80000000u //(0x1 << 31)
#endif

#define GMAC_CONFIG0_TXDIV		0x00100000u //(0x1 << 20)
#define GMAC_CONFIG1_TCO_OFF	0x00000000u //(0x0 << 16)

#define GMAC_CONFIG0_TX_CLK_OFF (GMAC_CONFIG0_TR | GMAC_CONFIG0_TXDIV)
#define GMAC_CONFIG1_TX_CLK_OFF GMAC_CONFIG1_TCO_OFF

#define GMACDLY0_OFFSET         ((0x2000u))
#define GMACDLY1_OFFSET         ((0x2004u))
#define GMACDLY2_OFFSET         ((0x2008u))
#define GMACDLY3_OFFSET         ((0x200Cu))
#define GMACDLY4_OFFSET         ((0x2010u))
#define GMACDLY5_OFFSET         ((0x2014u))
#define GMACDLY6_OFFSET         ((0x2018u))

#ifdef GMAC_ECC_TEST_FEATURE
static void tcc_ecc_write_reg(const struct tcc_dwmac *gmac, u32 reg, u32 val)
{
	writel(val, (void *)(gmac->ecc_block + (uintptr_t)reg));
}

static u32 tcc_ecc_read_reg(const struct tcc_dwmac *gmac, u32 reg)
{
	return readl((void *)(gmac->ecc_block + (uintptr_t)reg));
}

u32 tcc_get_ecc_status(struct tcc_dwmac *gmac)
{
	dev_info(&gmac->dev, "[ECC] TX Even ECC Fault status register : 0x%x\n",
		 tcc_ecc_read_reg(gmac, GMAC_ECC_TEEFS));
	dev_info(&gmac->dev, "[ECC] TX Odd ECC Fault status register  : 0x%x\n",
		 tcc_ecc_read_reg(gmac, GMAC_ECC_TOEFS));
	dev_info(&gmac->dev, "[ECC] RX Even ECC Fault status register : 0x%x\n",
		 tcc_ecc_read_reg(gmac, GMAC_ECC_REEEFS));
	dev_info(&gmac->dev, "[ECC] RX Odd ECC Fault status register  : 0x%x\n",
		 tcc_ecc_read_reg(gmac, GMAC_ECC_ROEFS));

	return 0;
}

void tcc_set_dummy_ecc_err(struct tcc_dwmac *gmac)
{
	/* TX even data mask[31:0] */
	tcc_ecc_write_reg(gmac, GMAC_ECC_TEDM0, 0x1);
	dev_info(&gmac->dev, "[ECC] Set Tx Eeven Data Mask\n");

	/* TX odd data mask[31:0] */
	tcc_ecc_write_reg(gmac, GMAC_ECC_TODM0, 0x1);
	dev_info(&gmac->dev, "[ECC] Set Tx Odd Data Mask\n");

	/* RX even data mask[31:0] */
	tcc_ecc_write_reg(gmac, GMAC_ECC_REDM0, 0x1);
	dev_info(&gmac->dev, "[ECC] Set Rx Even Data Mask\n");

	/* RX odd data mask[31:0] */
	tcc_ecc_write_reg(gmac, GMAC_ECC_RODM0, 0x1);
	dev_info(&gmac->dev, "[ECC] Set Rx Oven Data Mask\n");
}

void tcc_enable_ecc_event(struct tcc_dwmac *gmac, int num)
{
	tcc_ecc_write_reg(gmac, GMAC_ECC_PW, GMAC_ECC_PASSWORD);
	tcc_ecc_write_reg(gmac, GMAC_ECC_SFC, 0x11);
	tcc_ecc_write_reg(gmac, GMAC_ECC_PW, GMAC_ECC_PASSWORD);
	tcc_ecc_write_reg(gmac, GMAC_ECC_SFE0, (1 << num));
	tcc_ecc_write_reg(gmac, GMAC_ECC_PW, GMAC_ECC_PASSWORD);
	tcc_ecc_write_reg(gmac, GMAC_ECC_LOCK, DUMMY_VALUE);
}
#endif
static void tcc_cfg_write_reg(const struct tcc_dwmac *gmac, u32 reg, u32 val)
{
	writel(val, (void *)(gmac->cfg_block + (uintptr_t)reg));
}

#if defined(CONFIG_ARCH_TCC750X)
static void tcc_cfg_write_reg_16(const struct tcc_dwmac *gmac, u32 reg, u32 val)
{
	u16 *addr;
	u16 data;

	addr = (void *)(gmac->cfg_block +(uintptr_t)reg + 0x2);
	data = (val >> 16) & 0xFFFF;

	*((u16 *)(addr)) = data;
}
#endif

static void stmmac_write_reg(const struct tcc_dwmac *gmac, u32 reg, u32 val)
{
	writel(val, (void *)(gmac->gmac_block + (uintptr_t)reg));
}

void tcc_dwmac_clk_enable(const struct tcc_dwmac *priv)
{
	const struct tcc_dwmac *gmac = priv;
	int ret = 0;
	unsigned long rate;

	if ((!gmac->no_hsio) && (gmac->gmac_hclk != NULL)) {
		ret = clk_prepare_enable(gmac->gmac_hclk);
		if (ret != 0) {
			goto out_clk_enable;
		}
	}

	if (gmac->gmac_clk != NULL) {
		switch (gmac->phy_interface) {
		case PHY_INTERFACE_MODE_RGMII:
		case PHY_INTERFACE_MODE_GMII:
			ret = clk_set_rate(gmac->gmac_clk, 125 * 1000 * 1000);
			break;
		case PHY_INTERFACE_MODE_RMII:
			ret = clk_set_rate(
				gmac->gmac_clk, 50 * 1000 * 1000);
			break;
		case PHY_INTERFACE_MODE_MII:
			ret = clk_set_rate(gmac->gmac_clk, 25 * 1000 * 1000);
			break;
		default:
			dev_err(&gmac->dev,
				"Unknown phy, set default clk : 125MHz\n");
			ret = clk_set_rate(gmac->gmac_clk, 125 * 1000 * 1000);
			break;
		}

		if (ret != 0) {
			dev_err(&gmac->dev, "Set clk fail!\n");
			goto out_clk_enable;
		} else {
			rate = clk_get_rate(gmac->gmac_clk);
			(void)pr_info("gmac_clk: %lu\n", rate);
		}
	} else {
		dev_err(&gmac->dev, "gmac clk is null!\n");
		goto out_clk_enable;
	}

	if (gmac->ptp_clk != NULL) {
		ret = clk_prepare_enable(gmac->ptp_clk);
		if (ret != 0) {
			dev_err(&gmac->dev, "ptp_clk enable fail!\n");
			goto out_clk_enable;
		} else {
			ret = clk_set_rate(gmac->ptp_clk, 50 * 1000 * 1000);
			if (ret != 0) {
				dev_err(&gmac->dev, "ptp_clk set rate fail!\n");
			}
		}
	}

out_clk_enable:
	if (ret != 0) {
		dev_err(&gmac->dev, "clock setting fail!\n");
	}
}

void tcc_dwmac_tuning_timing(const struct tcc_dwmac *priv)
{
	const struct tcc_dwmac *gmac = priv;

	stmmac_write_reg(gmac, GMACDLY0_OFFSET,
			((gmac->txclk_i_dly&0x1fu)<<0) |
			((gmac->txclk_i_inv&0x01u)<<7) |
			((gmac->txclk_o_dly&0x1fu)<<8) |
			((gmac->txclk_o_inv&0x01u)<<15)|
			((gmac->txen_dly&0x1fu)<<16)   |
			((gmac->txer_dly&0x1fu)<<24));
	stmmac_write_reg(gmac, GMACDLY1_OFFSET,
			((gmac->txd0_dly & 0x1fu)<<0) |
			((gmac->txd1_dly & 0x1fu)<<8) |
			((gmac->txd2_dly & 0x1fu)<<16)|
			((gmac->txd3_dly & 0x1fu)<<24));
	stmmac_write_reg(gmac, GMACDLY2_OFFSET,
			((gmac->txd4_dly & 0x1fu)<<0) |
			((gmac->txd5_dly & 0x1fu)<<8) |
			((gmac->txd6_dly & 0x1fu)<<16)|
			((gmac->txd7_dly & 0x1fu)<<24));
	stmmac_write_reg(gmac, GMACDLY3_OFFSET,
			((gmac->rxclk_i_dly & 0x1fu)<<0)|
			((gmac->rxclk_i_inv & 0x01u)<<7)|
			((gmac->rxdv_dly & 0x1fu)<<16)  |
			((gmac->rxer_dly & 0x1fu)<<24));
	stmmac_write_reg(gmac, GMACDLY4_OFFSET,
			((gmac->rxd0_dly & 0x1fu)<<0) |
			((gmac->rxd1_dly & 0x1fu)<<8) |
			((gmac->rxd2_dly & 0x1fu)<<16)|
			((gmac->rxd3_dly & 0x1fu)<<24));
	stmmac_write_reg(gmac, GMACDLY5_OFFSET,
			((gmac->rxd4_dly & 0x1fu)<<0) |
			((gmac->rxd5_dly & 0x1fu)<<8) |
			((gmac->rxd6_dly & 0x1fu)<<16)|
			((gmac->rxd7_dly & 0x1fu)<<24));
	stmmac_write_reg(gmac, GMACDLY6_OFFSET,
			((gmac->col_dly & 0x1fu)<<0)|
			((gmac->crs_dly & 0x1fu)<<8));
}

void tcc_dwmac_phy_reset(const struct tcc_dwmac *priv)
{
	const struct tcc_dwmac *gmac = (const struct tcc_dwmac *)priv;
	int ret;

	if (gmac->phy_rst != 0u) {
		ret = gpio_direction_output(gmac->phy_rst, 0);
		if (ret < 0) {
			dev_err(&gmac->dev, "can't reset phy!\n");
		}
		usleep_range(10000, 15000);
		ret = gpio_direction_output(gmac->phy_rst, 1);
		if (ret < 0) {
			dev_err(&gmac->dev, "can't reset phy!\n");
		}
		usleep_range(50000, 65000);
	} else {
		dev_err(&gmac->dev, "no reset phy gpio found!\n");
	}
}

void tcc_dwmac_set_phy_interface(
	struct platform_device *plat_dev, const struct tcc_dwmac *gmac)
{
	u32 sw_config0 = 0;
	u32 sw_config1 = 0;
	const struct pinctrl *pin;

	sw_config1 &= ~GMAC_CONFIG_INTF_SEL_MASK;

	switch (gmac->phy_interface) {
	case PHY_INTERFACE_MODE_RGMII:
		sw_config1 |=
			(u32)(GMAC_CONFIG_INTF_RGMII
			      & GMAC_CONFIG_INTF_SEL_MASK);
		pin = devm_pinctrl_get_select(&plat_dev->dev, "rgmii");
		break;
	case PHY_INTERFACE_MODE_GMII:
		sw_config1 |=
			(u32)(GMAC_CONFIG_INTF_GMII
			      & GMAC_CONFIG_INTF_SEL_MASK);
		pin = devm_pinctrl_get_select(&plat_dev->dev, "gmii");
		break;
	case PHY_INTERFACE_MODE_RMII:
		sw_config1 |=
			(u32)(GMAC_CONFIG_INTF_RMII
			      & GMAC_CONFIG_INTF_SEL_MASK);
		pin = devm_pinctrl_get_select(
			&plat_dev->dev,
			gmac->rmii_rx_clk_ext ? "rmii-ext" : "rmii");
		break;
	case PHY_INTERFACE_MODE_MII:
		sw_config1 |=
			(u32)(GMAC_CONFIG_INTF_MII & GMAC_CONFIG_INTF_SEL_MASK);
		pin = devm_pinctrl_get_select(&plat_dev->dev, "mii");
		break;
	default:
		dev_err(&plat_dev->dev,
			"undefined phy mode, use rgmii mode!\n");
		sw_config1 |=
			(u32)(GMAC_CONFIG_INTF_RGMII
			      & GMAC_CONFIG_INTF_SEL_MASK);
		pin = devm_pinctrl_get_select(&plat_dev->dev, "rgmii");
		break;
	}

	if (IS_ERR(pin)) {
		dev_err(&plat_dev->dev, "pinctrl err!\n");
	} else {
		sw_config1 |= GMAC_CONFIG1_CE;
#if defined(CONFIG_ARCH_TCC750X)
		tcc_cfg_write_reg_16(gmac, GMAC_SW_CONFIG1_REG, sw_config1);
#else
		tcc_cfg_write_reg(gmac, GMAC_SW_CONFIG1_REG, sw_config1);
#endif
	}

	if ((gmac->phy_interface == PHY_INTERFACE_MODE_RMII)
	    && (gmac->rmii_tx_clk_off)) {
		dev_info(&plat_dev->dev, "disable gmac clk\n");
		sw_config0 |= GMAC_CONFIG0_TX_CLK_OFF;
		tcc_cfg_write_reg(gmac, GMAC_SW_CONFIG0_REG, sw_config0);
	}
}

int tcc_dwmac_init(struct platform_device *plat_dev, void *priv)
{
	const struct tcc_dwmac *gmac = (const struct tcc_dwmac *)priv;

	tcc_dwmac_set_phy_interface(plat_dev, gmac);
	tcc_dwmac_clk_enable(gmac);
	tcc_dwmac_tuning_timing(gmac);
	tcc_dwmac_phy_reset(gmac);

	return 0;
}

static void tcc_config_phy_mode_dt(
	const struct platform_device *plat_dev, struct tcc_dwmac *gmac)
{
	int ret;
	phy_interface_t phy_interface;

	ret = of_get_phy_mode(plat_dev->dev.of_node,  &phy_interface);

	if (ret != 0) {
		goto out_mode_dt;
	}

	switch (phy_interface) {
	case PHY_INTERFACE_MODE_RGMII:
		gmac->phy_interface = PHY_INTERFACE_MODE_RGMII;
		(void)pr_info("[INFO][GMAC] Phy interface: RGMII\n");
		break;
	case PHY_INTERFACE_MODE_GMII:
		gmac->phy_interface = PHY_INTERFACE_MODE_GMII;
		(void)pr_info("[INFO][GMAC] Phy interface: GMII\n");
		break;
	case PHY_INTERFACE_MODE_RMII:
		gmac->phy_interface = PHY_INTERFACE_MODE_RMII;
		(void)pr_info("[INFO][GMAC] Phy interface: RMII\n");
		break;
	case PHY_INTERFACE_MODE_MII:
		gmac->phy_interface = PHY_INTERFACE_MODE_MII;
		(void)pr_info("[INFO][GMAC] Phy interface: MII\n");
		break;
	default:
		(void)pr_err("[ERROR][GMAC] Unsupported phy interface: %d\n",
			     (int)phy_interface);
		(void)pr_err(
			"[ERROR][GMAC] set default phy interface mode RGMII\n");
		gmac->phy_interface = PHY_INTERFACE_MODE_RGMII;
		break;
	}

out_mode_dt:
	if (ret != 0) {
		(void)pr_err("[ERROR][GMAC] can't set phy interface \n");
	}
}

static void tcc_config_feature_dt(
	const struct platform_device *plat_dev, struct tcc_dwmac *gmac)
{
	gmac->use_ecid_mac_addr =
		of_property_read_bool(plat_dev->dev.of_node, "ecid-mac-addr");
	gmac->rmii_tx_clk_off = of_property_read_bool(
		plat_dev->dev.of_node, "telechips,rmii_tx_clk_off");
	gmac->no_hsio = of_property_read_bool(
		plat_dev->dev.of_node, "telechips,no_hsio");
}

static void tcc_config_phy_io_tx_dt(
	const struct platform_device *plat_dev, struct tcc_dwmac *gmac)
{
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "txclk-o-dly", &gmac->txclk_o_dly)
	    < 0) {
		gmac->txclk_o_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "txclk-o-inv", &gmac->txclk_o_inv)
	    < 0) {
		gmac->txclk_o_inv = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "txclk-i-dly", &gmac->txclk_i_dly)
	    < 0) {
		gmac->txclk_i_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "txclk-i-inv", &gmac->txclk_i_inv)
	    < 0) {
		gmac->txclk_i_inv = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "txen-dly", &gmac->txen_dly)
	    < 0) {
		gmac->txen_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "txer-dly", &gmac->txer_dly)
	    < 0) {
		gmac->txer_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "txd0-dly", &gmac->txd0_dly)
	    < 0) {
		gmac->txd0_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "txd1-dly", &gmac->txd1_dly)
	    < 0) {
		gmac->txd1_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "txd2-dly", &gmac->txd2_dly)
	    < 0) {
		gmac->txd2_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "txd3-dly", &gmac->txd3_dly)
	    < 0) {
		gmac->txd3_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "txd4-dly", &gmac->txd4_dly)
	    < 0) {
		gmac->txd4_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "txd5-dly", &gmac->txd5_dly)
	    < 0) {
		gmac->txd5_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "txd6-dly", &gmac->txd6_dly)
	    < 0) {
		gmac->txd6_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "txd7-dly", &gmac->txd7_dly)
	    < 0) {
		gmac->txd7_dly = 0;
	}
#if defined(CONFIG_TCC_MARVELL_PHY)
#if defined(CONFIG_ARCH_TCC807X)
	gmac->txclk_o_dly = 27;
#endif
#endif
}

static void tcc_config_phy_io_rx_dt(
	const struct platform_device *plat_dev, struct tcc_dwmac *gmac)
{
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "rxclk-i-dly", &gmac->rxclk_i_dly)
	    < 0) {
		gmac->rxclk_i_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "rxclk-i-inv", &gmac->rxclk_i_inv)
	    < 0) {
		gmac->rxclk_i_inv = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "rxdv-dly", &gmac->rxdv_dly)
	    < 0) {
		gmac->rxdv_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "rxer-dly", &gmac->rxer_dly)
	    < 0) {
		gmac->rxer_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "rxd0-dly", &gmac->rxd0_dly)
	    < 0) {
		gmac->rxd0_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "rxd1-dly", &gmac->rxd1_dly)
	    < 0) {
		gmac->rxd1_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "rxd2-dly", &gmac->rxd2_dly)
	    < 0) {
		gmac->rxd2_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "rxd3-dly", &gmac->rxd3_dly)
	    < 0) {
		gmac->rxd3_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "rxd4-dly", &gmac->rxd4_dly)
	    < 0) {
		gmac->rxd4_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "rxd5-dly", &gmac->rxd5_dly)
	    < 0) {
		gmac->rxd5_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "rxd6-dly", &gmac->rxd6_dly)
	    < 0) {
		gmac->rxd6_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "rxd7-dly", &gmac->rxd7_dly)
	    < 0) {
		gmac->rxd7_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "crs-dly", &gmac->crs_dly)
	    < 0) {
		gmac->crs_dly = 0;
	}
	if (of_property_read_u32(
		    plat_dev->dev.of_node, "col-dly", &gmac->col_dly)
	    < 0) {
		gmac->col_dly = 0;
	}

	gmac->fqtss_enable = of_property_read_bool(
		plat_dev->dev.of_node, "telechips,fqtss-enable");

	if (gmac->fqtss_enable) {
		dev_info(&plat_dev->dev, "enable FQTSS\n");
	}

#if defined(CONFIG_TCC_RTL9000_PHY)
	gmac->rxclk_i_inv = 1;
	gmac->rxdv_dly = 31;
#endif

#if (defined(CONFIG_TCC_MARVELL_PHY) && !defined(CONFIG_ARCH_TCN100X))
#if defined(CONFIG_ARCH_TCC807X)
	gmac->rxclk_i_dly = 3;
#else
	gmac->rxclk_i_dly = 9;
#endif
#endif
}

static void tcc_config_phy_io_dt(
	const struct platform_device *plat_dev, struct tcc_dwmac *gmac)
{
	tcc_config_phy_io_tx_dt(plat_dev, gmac);
	tcc_config_phy_io_rx_dt(plat_dev, gmac);
}

static int tcc_config_clk_dt(
	const struct platform_device *plat_dev, struct tcc_dwmac *gmac)
{
	int ret = 0;

	gmac->gmac_clk = of_clk_get_by_name(plat_dev->dev.of_node, "pclk");
	if (IS_ERR(gmac->gmac_clk)) {
		ret = (int)PTR_ERR(gmac->gmac_clk);
		goto out_clk_dt;
	}

	if (!gmac->no_hsio) {
		gmac->gmac_hclk =
			of_clk_get_by_name(plat_dev->dev.of_node, STMMAC_RESOURCE_NAME);
		if (IS_ERR(gmac->gmac_hclk)) {
			ret = (int)PTR_ERR(gmac->gmac_hclk);
			goto out_clk_dt;
		}
	}

	gmac->ptp_clk = of_clk_get_by_name(plat_dev->dev.of_node, "ptp-pclk");
	if (IS_ERR(gmac->ptp_clk)) {
		ret = (int)PTR_ERR(gmac->ptp_clk);
		goto out_clk_dt;
	}

out_clk_dt:
	return ret;
}

static void tcc_config_gpio_dt(
	struct platform_device *plat_dev, struct tcc_dwmac *gmac)
{
	int ret;

	ret = of_get_named_gpio(plat_dev->dev.of_node, "phyrst-gpio", 0);
	if (ret < 0) {
		gmac->phy_rst = 0;
		dev_err(&plat_dev->dev, "phy reset gpio not found\n");
	} else {
		gmac->phy_rst = (u32)ret;
		if (devm_gpio_request(&plat_dev->dev, gmac->phy_rst, "PHY_RST")
		    != 0) {
			dev_err(&plat_dev->dev,
				"phy reset gpio request fail\n");
		}
	}
}

static void
tcc_config_resource_dt(struct platform_device *plat_dev, struct tcc_dwmac *gmac)
{
	const struct resource *res;
	void __iomem *cfg_block;
	void __iomem *ecid_block;

	res = platform_get_resource(plat_dev, IORESOURCE_MEM, 1);
	cfg_block = devm_ioremap_resource(&plat_dev->dev, res);
	if (IS_ERR(cfg_block)) {
		dev_err(&plat_dev->dev, "Cannot get cfg block region (%ld)!\n",
			PTR_ERR(cfg_block));
	} else {
		gmac->cfg_block = cfg_block;
	}

	if (gmac->use_ecid_mac_addr) {
		ecid_block = of_iomap(plat_dev->dev.of_node, 2);
		if (IS_ERR(ecid_block)) {
			dev_err(&plat_dev->dev,
				"Cannot get ecid block region (%ld)!\n",
				PTR_ERR(ecid_block));
			ecid_block = NULL;
		} else {
			gmac->ecid_block = ecid_block;
		}
	}

#ifdef GMAC_ECC_TEST_FEATURE
	res = platform_get_resource(plat_dev, IORESOURCE_MEM, 3);
	gmac->ecc_block = devm_ioremap_resource(&plat_dev->dev, res);
#endif
	tcc_config_gpio_dt(plat_dev, gmac);
}

static int tcc_config_dt(struct platform_device *plat_dev, struct tcc_dwmac *gmac)
{
	int ret = 0;

	if (gmac != NULL) {
		tcc_config_feature_dt(plat_dev, gmac);
		tcc_config_resource_dt(plat_dev, gmac);
		tcc_config_phy_mode_dt(plat_dev, gmac);
		tcc_config_phy_io_dt(plat_dev, gmac);
		ret = tcc_config_clk_dt(plat_dev, gmac);
	} else {
		ret = -EINVAL;
	}

	if (ret != 0) {
		dev_err(&plat_dev->dev,
			"Ethernet clock config not set properly\n");
	}

	return ret;
}

static struct tcc_dwmac *
tcc_dwmac_config(struct platform_device *plat_dev, void __iomem *gmac_block)
{
	struct tcc_dwmac *gmac;
	int ret = 0;

	gmac = (struct tcc_dwmac *)devm_kzalloc(
		&plat_dev->dev, sizeof(*gmac), GFP_KERNEL);
	if (gmac == NULL) {
		gmac = ERR_PTR(-ENOMEM);
		goto out_dwmac_config;
	}

	gmac->dev = plat_dev->dev;
	ret = tcc_config_dt(plat_dev, gmac);
	if (ret != 0) {
		goto out_dwmac_config;
	}

	gmac->gmac_block = gmac_block;

	ret = tcc_dwmac_init(plat_dev, gmac);
	if (ret != 0) {
		goto out_dwmac_config;
	}
#ifdef CONFIG_DEBUG_FS
	tcc_dwmac_debugfs_init(gmac);
#endif
out_dwmac_config:
	if (ret != 0) {
		if (!IS_ERR(gmac)) {
			devm_kfree(&plat_dev->dev, gmac);
			gmac = ERR_PTR(-EINVAL);
		}
	}
	return gmac;
}

static struct plat_stmmacenet_data *dwmac_get_plat_data(
	struct platform_device *plat_dev, struct tcc_dwmac *gmac,
	struct stmmac_resources *stmmac_res)
{
	struct plat_stmmacenet_data *plat_dat;

	plat_dat = stmmac_probe_config_dt(plat_dev, &stmmac_res->mac);
	if (IS_ERR(plat_dat)) {
		goto out_plat_data;
	}

	if ((gmac->use_ecid_mac_addr) && (gmac->ecid_block != NULL)) {
		dev_info(&plat_dev->dev, "get mac address from ecid\n");
		kfree(stmmac_res->mac);
		stmmac_res->mac = (const char *)tcc_read_mac_addr_from_ecid(
			gmac->ecid_block);
	}

	plat_dat->init = tcc_dwmac_init;
	plat_dat->bsp_priv = gmac;

out_plat_data:
	return plat_dat;
}

static int tcc_dwmac_probe(struct platform_device *plat_dev)
{
	int ret;
	struct tcc_dwmac *gmac = NULL;
	struct plat_stmmacenet_data *plat_dat = NULL;
	struct stmmac_resources stmmac_res;

	ret = stmmac_get_platform_resources(plat_dev, &stmmac_res);
	if (ret == 0) {
		gmac = tcc_dwmac_config(plat_dev, stmmac_res.addr);
		if (IS_ERR(gmac)) {
			ret = -ENOMEM;
			goto out_probe;
		}
		plat_dat = dwmac_get_plat_data(plat_dev, gmac, &stmmac_res);
		if (IS_ERR(plat_dat)) {
			goto out_probe;
		}
		ret = stmmac_dvr_probe(&plat_dev->dev, plat_dat, &stmmac_res);
	}

out_probe:
	if (ret != 0) {
		if (!IS_ERR(plat_dat) && (plat_dat != NULL)) {
			stmmac_remove_config_dt(plat_dev, plat_dat);
		}
#ifdef CONFIG_DEBUG_FS
		if (!IS_ERR(gmac) && (gmac != NULL)) {
			tcc_dwmac_debugfs_exit(gmac);
		}
#endif
	}
	return ret;
}

static const struct of_device_id tcc_dwmac_match[] = {
	{ .compatible = "telechips,gmac" },
	{ }
};
MODULE_DEVICE_TABLE(of, tcc_dwmac_match);

static struct platform_driver tcc_dwmac_driver = {
	.probe  = tcc_dwmac_probe,
	.remove = stmmac_pltfr_remove,
	.driver = {
		.name           = "tcc-dwmac",
		.pm		= &stmmac_pltfr_pm_ops,
		.of_match_table = tcc_dwmac_match,
	},
};
module_platform_driver(tcc_dwmac_driver);

MODULE_AUTHOR("Telechips <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips 10/100/1000 Ethernet Driver\n");
MODULE_LICENSE("GPL v2");
