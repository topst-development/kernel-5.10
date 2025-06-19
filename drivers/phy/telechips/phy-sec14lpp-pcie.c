// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>

/*
 * PCIe controller wrapper link configuration registers
 */
#define PCIE_LINK_CFG08			(0x020U)
#define PCIE_LINK_CFG43			(0x0ACU)
#define PCIE_LINK_CFG44			(0x0B0U)

/*
 * PCIe controller wrapper phy configuration registers
 */
#define PCIE_PHY_CMN_REG062		(0x0188U)
#define PCIE_PHY_CMN_REG064		(0x0190U)

/*
 * Mask/shift bits in PCIe related registers
 */
#define PCIE_LINK_CFG_RCVRY_IDLE_STATE_SHIFT		(16U)
#define PCIE_LINK_CFG_PHY_POWER_OFF_SHIFT		(5U)
#define PCIE_LINK_CFG_PHY_PLL_LOCKED_SHIFT		(1U)
#define PCIE_LINK_CFG_PHY_CDR_LOCKED_SHIFT		(0U)
#define PCIE_LINK_CFG_RCVRY_IDLE_STATE_MASK		((u32)0x1U << PCIE_LINK_CFG_RCVRY_IDLE_STATE_SHIFT)
#define PCIE_LINK_CFG_PHY_POWER_OFF_MASK		((u32)0x1U << PCIE_LINK_CFG_PHY_POWER_OFF_SHIFT)
#define PCIE_LINK_CFG_PHY_PLL_LOCKED_MASK		((u32)0x1U << PCIE_LINK_CFG_PHY_PLL_LOCKED_SHIFT)
#define PCIE_LINK_CFG_PHY_CDR_LOCKED_MASK		((u32)0x1U << PCIE_LINK_CFG_PHY_CDR_LOCKED_SHIFT)

#define PCIE_LINK_CFG_AUX_SEL_SHIFT		(4U)
#define PCIE_LINK_CFG_AUX_SEL_MASK		((u32)0x1U << PCIE_LINK_CFG_AUX_SEL_SHIFT)

#define PCIE_LINK_CFG_POWER_UP_RST_SHIFT		(6U)
#define PCIE_LINK_CFG_PERST_SHIFT		(5U)
#define PCIE_LINK_CFG_PHY_CMN_REG_RST_SHIFT		(4U)
#define PCIE_LINK_CFG_PHY_CMN_RST_SHIFT		(3U)
#define PCIE_LINK_CFG_PHY_G_RST_SHIFT		(2U)
#define PCIE_LINK_CFG_PHY_TRSV_REG_RST_SHIFT		(1U)
#define PCIE_LINK_CFG_PHY_TRSV_RST_SHIFT		(0U)
#define PCIE_LINK_CFG_POWER_UP_RST_MASK		((u32)0x1U << PCIE_LINK_CFG_POWER_UP_RST_SHIFT)
#define PCIE_LINK_CFG_PERST_MASK		((u32)0x1U << PCIE_LINK_CFG_PERST_SHIFT)
#define PCIE_LINK_CFG_PHY_CMN_REG_RST_MASK		((u32)0x1U << PCIE_LINK_CFG_PHY_CMN_REG_RST_SHIFT)
#define PCIE_LINK_CFG_PHY_CMN_RST_MASK		((u32)0x1U << PCIE_LINK_CFG_PHY_CMN_RST_SHIFT)
#define PCIE_LINK_CFG_PHY_G_RST_MASK		((u32)0x1U << PCIE_LINK_CFG_PHY_G_RST_SHIFT)
#define PCIE_LINK_CFG_PHY_TRSV_REG_RST_MASK		((u32)0x1U << PCIE_LINK_CFG_PHY_TRSV_REG_RST_SHIFT)
#define PCIE_LINK_CFG_PHY_TRSV_RST_MASK		((u32)0x1U << PCIE_LINK_CFG_PHY_TRSV_RST_SHIFT)
#define PCIE_LINK_CFG_PHY_RESET_MASK		(PCIE_LINK_CFG_PHY_TRSV_RST_MASK | \
		PCIE_LINK_CFG_PHY_TRSV_REG_RST_MASK | \
		PCIE_LINK_CFG_PHY_G_RST_MASK | \
		PCIE_LINK_CFG_PHY_CMN_RST_MASK | \
		PCIE_LINK_CFG_PHY_CMN_REG_RST_MASK | \
		PCIE_LINK_CFG_PERST_MASK | \
		PCIE_LINK_CFG_POWER_UP_RST_MASK)

#define PCIE_PHY_ANA_PLL_CLK_OUT_TO_EXT_IO_SEL_SHIFT		(3U)
#define PCIE_PHY_ANA_PLL_CLK_OUT_TO_EXT_IO_SEL_MASK		((u32)0x1U << PCIE_PHY_ANA_PLL_CLK_OUT_TO_EXT_IO_SEL_SHIFT)

#define PCIE_PHY_ANA_AUX_RX_TX_SEL_SHIFT		(7U)
#define PCIE_PHY_ANA_AUX_RX_TX_SEL_MASK		((u32)0x1U << PCIE_PHY_ANA_AUX_RX_TX_SEL_SHIFT)

#define PCIE_AUX_CLK_RATE		(125000000UL)
#define PCIE_APB_CLK_RATE		(100000000UL)
#define PCIE_PCS_CLK_RATE		(100000000UL)
#define PCIE_REF_CLK_RATE		(100000000UL)

struct sec14lpp_pcie_phy {
	struct device	*dev;
	void __iomem *phy_base;
	void __iomem *link_base;
	struct clk		*fbus_clk;
	struct clk		*aux_clk;
	struct clk		*apb_clk;
	struct clk		*pcs_clk;
	struct clk		*ref_clk;
	u32 pms;
	u32	mode;
};

static inline u32 sec14lpp_pcie_phy_readl(const void __iomem *base, u32 offset)
{
	u32 ret = 0x0U;

	if (base != NULL) {
		ret = ioread32(base + offset);
	}

	return ret;
}

static inline void sec14lpp_pcie_phy_writel(void __iomem *base, u32 offset, u32 val, u32 mask)
{
	if (base != NULL) {
		iowrite32((ioread32(base + offset) & ~mask)|val,
				base + offset);
	}
}

static s32 sec14lpp_pcie_phy_enable_clk(const struct sec14lpp_pcie_phy *priv)
{
	s32 err = 0;

	if (priv != NULL) {
		err = clk_prepare_enable(priv->aux_clk);
		if (err == 0) {
			err = clk_prepare_enable(priv->apb_clk);
		}

		if (err == 0) {
			err = clk_prepare_enable(priv->pcs_clk);
		}

		if (err == 0) {
			err = clk_prepare_enable(priv->ref_clk);
		}

		if (err == 0) {
			err = clk_prepare_enable(priv->fbus_clk);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 sec14lpp_pcie_phy_disable_clk(const struct sec14lpp_pcie_phy *priv)
{
	s32 err = 0;

	if (priv != NULL) {
		clk_disable_unprepare(priv->ref_clk);
		clk_disable_unprepare(priv->pcs_clk);
		clk_disable_unprepare(priv->apb_clk);
		clk_disable_unprepare(priv->aux_clk);
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 sec14lpp_pcie_phy_set_clk(const struct sec14lpp_pcie_phy *priv, u32 mode)
{
	s32 err = 0;

	if (priv != NULL) {
		err = sec14lpp_pcie_phy_enable_clk(priv);
		if (err == 0) {
			u32 val, mask;

			if ((mode & 0xF0U) == 0x0U) {
				val = (0x1U << PCIE_PHY_ANA_PLL_CLK_OUT_TO_EXT_IO_SEL_SHIFT);
				mask = PCIE_PHY_ANA_PLL_CLK_OUT_TO_EXT_IO_SEL_MASK;
				sec14lpp_pcie_phy_writel(priv->phy_base, PCIE_PHY_CMN_REG062, val, mask);

				val = (0x1U << PCIE_PHY_ANA_AUX_RX_TX_SEL_SHIFT);
				mask = PCIE_PHY_ANA_AUX_RX_TX_SEL_MASK;
				sec14lpp_pcie_phy_writel(priv->phy_base, PCIE_PHY_CMN_REG064, val, mask);
			}
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 sec14lpp_pcie_phy_pwr_down(const struct sec14lpp_pcie_phy *priv, bool enable)
{
	s32 err = 0;

	if (priv != NULL) {
		u32 val, mask;

		mask = PCIE_LINK_CFG_PHY_POWER_OFF_MASK;
		val = enable ? mask : 0x0U;
		sec14lpp_pcie_phy_writel(priv->link_base, PCIE_LINK_CFG08, val, mask);
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 sec14lpp_pcie_phy_set_mode(struct phy *phy, enum phy_mode mode, s32 submode)
{
	s32 err = 0;

	if (phy != NULL) {
		struct sec14lpp_pcie_phy *priv = (struct sec14lpp_pcie_phy *)phy_get_drvdata(phy);

		if (mode == PHY_MODE_PCIE) {
			u8 clk_mode;

			if (!__builtin_add_overflow(submode, 0, &clk_mode)) {
				err = sec14lpp_pcie_phy_set_clk((const struct sec14lpp_pcie_phy *)priv, (u32)clk_mode);
				if (err == 0) {
					priv->mode = (u32)clk_mode;
				}
			} else {
				err = -EINVAL;
			}
		} else {
			err = -EINVAL;
		}
	} else {
		err = -ENODEV;
	}

	return err;
}

static s32 sec14lpp_pcie_phy_reset(struct phy *phy)
{
	s32 err = 0;

	if (phy != NULL) {
		const struct sec14lpp_pcie_phy *priv =
			(const struct sec14lpp_pcie_phy *)phy_get_drvdata(phy);

		/* TODO */
		(void)priv;
	} else {
		err = -ENODEV;
	}

	return err;
}

static s32 sec14lpp_pcie_phy_power_on(struct phy *phy)
{
	s32 err = 0;

	if (phy != NULL) {
		const struct sec14lpp_pcie_phy *priv =
			(const struct sec14lpp_pcie_phy *)phy_get_drvdata(phy);

		err = sec14lpp_pcie_phy_pwr_down(priv, false);
	} else {
		err = -ENODEV;
	}

	return err;
}

static s32 sec14lpp_pcie_phy_power_off(struct phy *phy)
{
	s32 err = 0;

	if (phy != NULL) {
		const struct sec14lpp_pcie_phy *priv =
			(const struct sec14lpp_pcie_phy *)phy_get_drvdata(phy);

		err = sec14lpp_pcie_phy_pwr_down(priv, true);
	} else {
		err = -ENODEV;
	}

	return err;
}

static s32 sec14lpp_pcie_phy_init(struct phy *phy)
{
	s32 err = 0;

	if (phy != NULL) {
		const struct sec14lpp_pcie_phy *priv =
			(const struct sec14lpp_pcie_phy *)phy_get_drvdata(phy);
		u32 val, mask;

		/* wait for PLL lock */
		mask = PCIE_LINK_CFG_PHY_PLL_LOCKED_MASK;
		do {
			val = sec14lpp_pcie_phy_readl(priv->link_base, PCIE_LINK_CFG08) & mask;
		} while (val == 0U);

		/* wait for CDR lock */
		mask = PCIE_LINK_CFG_PHY_CDR_LOCKED_MASK;
		do {
			val = sec14lpp_pcie_phy_readl(priv->link_base, PCIE_LINK_CFG08) & mask;
		} while (val == 0U);

		mask = PCIE_LINK_CFG_AUX_SEL_MASK;
		val = mask;
		sec14lpp_pcie_phy_writel(priv->link_base, PCIE_LINK_CFG43, val, mask);
		udelay(100);

		mask = PCIE_LINK_CFG_RCVRY_IDLE_STATE_MASK;
		do {
			val = sec14lpp_pcie_phy_readl(priv->link_base, PCIE_LINK_CFG08) & mask;
		} while (val != 0U);
	} else {
		err = -ENODEV;
	}

	return err;
}

static s32 sec14lpp_pcie_phy_exit(struct phy *phy)
{
	s32 err = 0;

	if (phy != NULL) {
		const struct sec14lpp_pcie_phy *priv =
			(const struct sec14lpp_pcie_phy *)phy_get_drvdata(phy);

		/* TODO */
		(void)priv;
	} else {
		err = -ENODEV;
	}

	return err;
}

static const struct phy_ops sec14lpp_pcie_phy_ops = {
	.init		= sec14lpp_pcie_phy_init,
	.exit		= sec14lpp_pcie_phy_exit,
	.power_on		= sec14lpp_pcie_phy_power_on,
	.power_off		= sec14lpp_pcie_phy_power_off,
	.reset		= sec14lpp_pcie_phy_reset,
	.set_mode		= sec14lpp_pcie_phy_set_mode,
	.owner		= THIS_MODULE,
};

static s32 sec14lpp_pcie_phy_suspend_late(struct device *dev)
{
	s32 err = 0;

	if (dev != NULL) {
		const struct sec14lpp_pcie_phy *priv =
			(const struct sec14lpp_pcie_phy *)dev_get_drvdata(dev);

		err = sec14lpp_pcie_phy_disable_clk(priv);
	} else {
		err = -ENODEV;
	}

	return err;
}

static const struct dev_pm_ops sec14lpp_pcie_phy_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(sec14lpp_pcie_phy_suspend_late,
		NULL)
};


#ifdef CONFIG_OF
static const struct of_device_id sec14lpp_pcie_phy_id_table[] = {
	{
		.compatible = "samsung,14lpp-pcie-phy",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sec14lpp_pcie_phy_id_table);
#endif

static s32 sec14lpp_pcie_phy_of_parse_clk(struct platform_device *pdev, struct sec14lpp_pcie_phy *priv)
{
	s32 err = 0;

	if ((pdev != NULL) && (priv != NULL)) {
		priv->aux_clk = devm_clk_get(&pdev->dev, "pcie_aux");
		if (IS_ERR(priv->aux_clk)) {
			err = (s32)PTR_ERR(priv->aux_clk);
		} else {
			err = clk_set_rate(priv->aux_clk, PCIE_AUX_CLK_RATE);
		}

		if (err == 0) {
			priv->apb_clk = devm_clk_get(&pdev->dev, "pcie_apb");
			if (IS_ERR(priv->apb_clk)) {
				err = (s32)PTR_ERR(priv->apb_clk);
			} else {
				err = clk_set_rate(priv->apb_clk, PCIE_APB_CLK_RATE);
			}
		}

		if (err == 0) {
			priv->pcs_clk = devm_clk_get(&pdev->dev, "pcie_pcs");
			if (IS_ERR(priv->pcs_clk)) {
				err = (s32)PTR_ERR(priv->pcs_clk);
			} else {
				err = clk_set_rate(priv->pcs_clk, PCIE_PCS_CLK_RATE);
			}
		}

		if (err == 0) {
			priv->fbus_clk = devm_clk_get(&pdev->dev, "fbus_hsio");
			if (IS_ERR(priv->fbus_clk)) {
				err = (s32)PTR_ERR(priv->fbus_clk);
			}
		}

		if (err == 0) {
			priv->ref_clk = devm_clk_get(&pdev->dev, "pcie_ref");
			if (!IS_ERR(priv->ref_clk)) {
				err = clk_set_rate(priv->ref_clk, PCIE_REF_CLK_RATE);
			}
		}

		if (err == 0) {
			err = of_property_read_u32(pdev->dev.of_node, "pms",
					&priv->pms);
		}
	} else {
		err = -EINVAL;
	}
	return err;
}

static s32 sec14lpp_pcie_phy_of_parse_reg(struct platform_device *pdev, struct sec14lpp_pcie_phy *priv)
{
	s32 err = 0;

	if ((pdev != NULL) && (priv != NULL)) {
		struct resource *res;

		res = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "phy");
		if (res != NULL) {
			priv->phy_base = devm_ioremap(&pdev->dev, res->start,
					resource_size(res));
			if (IS_ERR(priv->phy_base)) {
				err = (s32)PTR_ERR(priv->phy_base);
			}
		}

		if (err == 0) {
			res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "link");
			if (res != NULL) {
				priv->link_base = devm_ioremap(&pdev->dev, res->start,
						resource_size(res));
				if (IS_ERR(priv->link_base)) {
					err = (s32)PTR_ERR(priv->link_base);
				}
			}
		}
	} else {
		err = -EINVAL;
	}
	return err;
}

static s32 sec14lpp_pcie_phy_of_parse_dt(struct platform_device *pdev, struct sec14lpp_pcie_phy *priv)
{
	s32 err = 0;

	if ((pdev != NULL) && (priv != NULL)) {
		err = sec14lpp_pcie_phy_of_parse_reg(pdev, priv);
		if (err == 0) {
			err = sec14lpp_pcie_phy_of_parse_clk(pdev, priv);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 sec14lpp_pcie_phy_register(struct sec14lpp_pcie_phy *priv)
{
	s32 err = 0;

	if (priv != NULL) {
		struct phy_provider *provider;
		struct phy *phy;

		phy = devm_phy_create(priv->dev, NULL, &sec14lpp_pcie_phy_ops);
		if (IS_ERR(phy)) {
			err = PTR_ERR(phy);
		}

		if (err == 0) {
			phy_set_drvdata(phy, priv);
			provider = devm_of_phy_provider_register(priv->dev,
					of_phy_simple_xlate);
			if (IS_ERR(provider)) {
				err = PTR_ERR(provider);
			}
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 sec14lpp_pcie_phy_probe(struct platform_device *pdev)
{
	struct sec14lpp_pcie_phy *priv;
	s32 err = 0;

	priv = (struct sec14lpp_pcie_phy *)devm_kzalloc(&pdev->dev,
			sizeof(struct sec14lpp_pcie_phy), GFP_KERNEL);
	if (priv != NULL) {
		err = sec14lpp_pcie_phy_of_parse_dt(pdev, priv);
		if (err == 0) {
			priv->dev = &pdev->dev;
			platform_set_drvdata(pdev, priv);
			err = sec14lpp_pcie_phy_register(priv);
		}

		if (err == 0) {
			pm_runtime_enable(priv->dev);
		}
	} else {
		err = -ENOMEM;
	}

	return err;
}

static s32 sec14lpp_pcie_phy_remove(struct platform_device *pdev)
{
	s32 err = 0;

	if (pdev != NULL) {
		const struct sec14lpp_pcie_phy *priv =
			(const struct sec14lpp_pcie_phy *)platform_get_drvdata(pdev);

		pm_runtime_disable(priv->dev);
		err = sec14lpp_pcie_phy_disable_clk(priv);
	} else {
		err = -ENODEV;
	}

	return err;
}

static struct platform_driver sec14lpp_pcie_phy_driver = {
	.probe		= sec14lpp_pcie_phy_probe,
	.remove		= sec14lpp_pcie_phy_remove,
	.driver		= {
		.name	= "sec14lpp-pcie-phy",
		.pm	= &sec14lpp_pcie_phy_pm_ops,
		.of_match_table = of_match_ptr(sec14lpp_pcie_phy_id_table),
	},
};
module_platform_driver(sec14lpp_pcie_phy_driver);

MODULE_DESCRIPTION("Telechips Dolphin3s PCIe PHY Driver");
MODULE_LICENSE("GPL v2");
