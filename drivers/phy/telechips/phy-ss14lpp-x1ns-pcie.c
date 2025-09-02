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
 * PCIe controller wrapper clock configuration registers
 */
#define PCIE_CLK_CFG00			(0x000U)
#define PCIE_CLK_CFG04			(0x010U)

/*
 * PCIe controller wrapper phy configuration registers
 */
#define PCIE_PHY_REG08		(0x020U)
#define PCIE_PHY_REG43		(0x0ACU)

/*
 * Mask/shift bits in PCIe related registers
 */
#define PCIE_PHY_REG_REF_USE_PAD_SHIFT		(3U)
#define PCIE_PHY_REG_REF_USE_PAD_MASK		((u32)0x1U << PCIE_PHY_REG_REF_USE_PAD_SHIFT)

#define PCIE_PHY_REG_PWR_DOWN_SHIFT		(1U)
#define PCIE_PHY_REG_PWR_DOWN_MASK		((u32)0x1U << PCIE_PHY_REG_PWR_DOWN_SHIFT)

#define PCIE_CLK_CFG_RESETB_SHIT		(31U)
#define PCIE_CLK_CFG_LOCK_SHIFT     (23U)
#define PCIE_CLK_CFG_RESETB_MASK		((u32)0x1U << PCIE_CLK_CFG_RESETB_SHIT)
#define PCIE_CLK_CFG_LOCK_MASK      ((u32)0x1U << PCIE_CLK_CFG_LOCK_SHIFT)

#define PCIE_CLK_CFG_ERIO_EN_SHIFT		(18U)
#define PCIE_CLK_CFG_ERIO_PLL_LOCK_DONE_SHIFT		(5U)
#define PCIE_CLK_CFG_ERIO_ISO_ENB_SHIFT		(0U)
#define PCIE_CLK_CFG_ERIO_EN_MASK		((u32)0x1U << PCIE_CLK_CFG_ERIO_EN_SHIFT)
#define PCIE_CLK_CFG_ERIO_PLL_LOCK_DONE_MASK		((u32)0x1U << PCIE_CLK_CFG_ERIO_PLL_LOCK_DONE_SHIFT)
#define PCIE_CLK_CFG_ERIO_ISO_ENB_MASK		((u32)0x1U << PCIE_CLK_CFG_ERIO_ISO_ENB_SHIFT)

#define PCIE_FBUS_CLK_RATE		(333333333UL)
#define PCIE_PHY_CLK_RATE		(100000000UL)

struct ss14lpp_x1ns_pcie_phy {
	struct device	*dev;
	void __iomem *phy_base;
	void __iomem *clk_base;
	struct clk	*phy_clk;
	struct clk	*fbus_clk;
	u32 pms;
	u32	mode;
};

static inline u32 ss14lpp_x1ns_pcie_phy_readl(const void __iomem *base, u32 offset)
{
	u32 ret = 0x0U;

	if (base != NULL) {
		ret = ioread32(base + offset);
	}

	return ret;
}

static inline void ss14lpp_x1ns_pcie_phy_writel(void __iomem *base, u32 offset, u32 val, u32 mask)
{
	if (base != NULL) {
		iowrite32((ioread32(base + offset) & ~mask)|val,
				base + offset);
	}
}

static s32 ss14lpp_x1ns_pcie_phy_enable_clk(const struct ss14lpp_x1ns_pcie_phy *priv)
{
	s32 err = 0;

	if (priv != NULL) {
		err = clk_prepare_enable(priv->fbus_clk);
		if (err == 0) {
			err = clk_prepare_enable(priv->phy_clk);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 ss14lpp_x1ns_pcie_phy_disable_clk(const struct ss14lpp_x1ns_pcie_phy *priv)
{
	s32 err = 0;

	if (priv != NULL) {
		clk_disable_unprepare(priv->phy_clk);
		clk_disable_unprepare(priv->fbus_clk);
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 ss14lpp_x1ns_pcie_phy_disable_clkout(const struct ss14lpp_x1ns_pcie_phy *priv)
{
	s32 err = 0;

	if (priv != NULL) {
		u32 val, mask;

		val = 0x0U;
		mask = PCIE_CLK_CFG_RESETB_MASK;
		ss14lpp_x1ns_pcie_phy_writel(priv->clk_base, PCIE_CLK_CFG00, val, mask);
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 ss14lpp_x1ns_pcie_phy_set_clk(const struct ss14lpp_x1ns_pcie_phy *priv, u32 mode)
{
	s32 err = 0;

	if (priv != NULL) {
		/* Use internal reference clock */
		err = ss14lpp_x1ns_pcie_phy_enable_clk(priv);
		if (err == 0) {
			u32 val, mask;

			mask = PCIE_PHY_REG_REF_USE_PAD_MASK;
			val = ((mode & 0xF0U) != 0x0U) ? mask : 0x0U;
			ss14lpp_x1ns_pcie_phy_writel(priv->phy_base, PCIE_PHY_REG08, val, mask);

			if ((mode & 0xF0U) == 0x0U) {
				val = 0x0U;
				mask = PCIE_CLK_CFG_RESETB_MASK;
				ss14lpp_x1ns_pcie_phy_writel(priv->clk_base, PCIE_CLK_CFG00, val, mask);

				val = priv->pms | PCIE_CLK_CFG_RESETB_MASK;
				mask = 0xFFFFFFFFU;
				ss14lpp_x1ns_pcie_phy_writel(priv->clk_base, PCIE_CLK_CFG00, val, mask);

				mask = PCIE_CLK_CFG_LOCK_MASK;
				do {
					val = ss14lpp_x1ns_pcie_phy_readl(priv->clk_base, PCIE_CLK_CFG00) & mask;
				} while (val != mask);

				mask = PCIE_CLK_CFG_ERIO_EN_MASK |
					PCIE_CLK_CFG_ERIO_PLL_LOCK_DONE_MASK |
					PCIE_CLK_CFG_ERIO_ISO_ENB_MASK;
				val = mask;
				ss14lpp_x1ns_pcie_phy_writel(priv->clk_base, PCIE_CLK_CFG04, val, mask);
			}
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 ss14lpp_x1ns_pcie_phy_pwr_down(const struct ss14lpp_x1ns_pcie_phy *priv, bool enable)
{
	s32 err = 0;

	if (priv != NULL) {
		u32 val, mask;

		mask = PCIE_PHY_REG_PWR_DOWN_MASK;
		val = enable ? mask : 0x0U;
		ss14lpp_x1ns_pcie_phy_writel(priv->phy_base, PCIE_PHY_REG43, val, mask);
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 ss14lpp_x1ns_pcie_phy_set_mode(struct phy *phy, enum phy_mode mode, s32 submode)
{
	s32 err = 0;

	if (phy != NULL) {
		struct ss14lpp_x1ns_pcie_phy *priv = (struct ss14lpp_x1ns_pcie_phy *)phy_get_drvdata(phy);

		if (mode == PHY_MODE_PCIE) {
			u8 clk_mode;

			if (!__builtin_add_overflow(submode, 0, &clk_mode)) {
				err = ss14lpp_x1ns_pcie_phy_set_clk((const struct ss14lpp_x1ns_pcie_phy *)priv, (u32)clk_mode);
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

static s32 ss14lpp_x1ns_pcie_phy_reset(struct phy *phy)
{
	s32 err = 0;

	if (phy != NULL) {
		const struct ss14lpp_x1ns_pcie_phy *priv =
			(const struct ss14lpp_x1ns_pcie_phy *)phy_get_drvdata(phy);

		/* TODO */
		(void)priv;
	} else {
		err = -ENODEV;
	}

	return err;
}

static s32 ss14lpp_x1ns_pcie_phy_power_on(struct phy *phy)
{
	s32 err = 0;

	if (phy != NULL) {
		const struct ss14lpp_x1ns_pcie_phy *priv =
			(const struct ss14lpp_x1ns_pcie_phy *)phy_get_drvdata(phy);

		err = ss14lpp_x1ns_pcie_phy_pwr_down(priv, false);
	} else {
		err = -ENODEV;
	}

	return err;
}

static s32 ss14lpp_x1ns_pcie_phy_power_off(struct phy *phy)
{
	s32 err = 0;

	if (phy != NULL) {
		const struct ss14lpp_x1ns_pcie_phy *priv =
			(const struct ss14lpp_x1ns_pcie_phy *)phy_get_drvdata(phy);

		err = ss14lpp_x1ns_pcie_phy_pwr_down(priv, true);
	} else {
		err = -ENODEV;
	}

	return err;
}

static s32 ss14lpp_x1ns_pcie_phy_init(struct phy *phy)
{
	s32 err = 0;

	if (phy != NULL) {
		const struct ss14lpp_x1ns_pcie_phy *priv =
			(const struct ss14lpp_x1ns_pcie_phy *)phy_get_drvdata(phy);

		/* TODO */
		(void)priv;
	} else {
		err = -ENODEV;
	}

	return err;
}

static s32 ss14lpp_x1ns_pcie_phy_exit(struct phy *phy)
{
	s32 err = 0;

	if (phy != NULL) {
		const struct ss14lpp_x1ns_pcie_phy *priv =
			(const struct ss14lpp_x1ns_pcie_phy *)phy_get_drvdata(phy);

		/* TODO */
		(void)priv;
	} else {
		err = -ENODEV;
	}

	return err;
}

static const struct phy_ops ss14lpp_x1ns_pcie_phy_ops = {
	.init		= ss14lpp_x1ns_pcie_phy_init,
	.exit		= ss14lpp_x1ns_pcie_phy_exit,
	.power_on		= ss14lpp_x1ns_pcie_phy_power_on,
	.power_off		= ss14lpp_x1ns_pcie_phy_power_off,
	.reset		= ss14lpp_x1ns_pcie_phy_reset,
	.set_mode		= ss14lpp_x1ns_pcie_phy_set_mode,
	.owner		= THIS_MODULE,
};

static s32 ss14lpp_x1ns_pcie_phy_suspend_late(struct device *dev)
{
	s32 err = 0;

	if (dev != NULL) {
		const struct ss14lpp_x1ns_pcie_phy *priv =
			(const struct ss14lpp_x1ns_pcie_phy *)dev_get_drvdata(dev);

		err = ss14lpp_x1ns_pcie_phy_disable_clk(priv);
		if (err == 0) {
			err = ss14lpp_x1ns_pcie_phy_disable_clkout(priv);
		}
	} else {
		err = -ENODEV;
	}

	return err;
}

static const struct dev_pm_ops ss14lpp_x1ns_pcie_phy_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(ss14lpp_x1ns_pcie_phy_suspend_late,
		NULL)
};

#ifdef CONFIG_OF
static const struct of_device_id ss14lpp_x1ns_pcie_phy_id_table[] = {
	{
		.compatible = "synopsys,ln14lpp-pcie-phy",
	},
	{},
};
MODULE_DEVICE_TABLE(of, ss14lpp_x1ns_pcie_phy_id_table);
#endif

static s32 ss14lpp_x1ns_pcie_phy_of_parse_clk(struct platform_device *pdev, struct ss14lpp_x1ns_pcie_phy *priv)
{
	s32 err = 0;

	if ((pdev != NULL) && (priv != NULL)) {
		priv->fbus_clk = devm_clk_get(&pdev->dev, "pcie_fbus");
		if (IS_ERR(priv->fbus_clk)) {
			err = (s32)PTR_ERR(priv->fbus_clk);
		} else {
			err = clk_set_rate(priv->fbus_clk, PCIE_FBUS_CLK_RATE);
		}

		if (err == 0) {
			priv->phy_clk = devm_clk_get(&pdev->dev, "pcie_phy");
			if (IS_ERR(priv->phy_clk)) {
				err = (s32)PTR_ERR(priv->phy_clk);
			} else {
				err = clk_set_rate(priv->phy_clk, PCIE_PHY_CLK_RATE);
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

static s32 ss14lpp_x1ns_pcie_phy_of_parse_reg(struct platform_device *pdev, struct ss14lpp_x1ns_pcie_phy *priv)
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
					IORESOURCE_MEM, "clk");
			if (res != NULL) {
				priv->clk_base = devm_ioremap(&pdev->dev, res->start,
						resource_size(res));
				if (IS_ERR(priv->clk_base)) {
					err = (s32)PTR_ERR(priv->clk_base);
				}
			}
		}
	} else {
		err = -EINVAL;
	}
	return err;
}

static s32 ss14lpp_x1ns_pcie_phy_of_parse_dt(struct platform_device *pdev, struct ss14lpp_x1ns_pcie_phy *priv)
{
	s32 err = 0;

	if ((pdev != NULL) && (priv != NULL)) {
		err = ss14lpp_x1ns_pcie_phy_of_parse_reg(pdev, priv);
		if (err == 0) {
			err = ss14lpp_x1ns_pcie_phy_of_parse_clk(pdev, priv);
		}
	} else {
		err = -EINVAL;
	}
	return err;
}

static s32 ss14lpp_x1ns_pcie_phy_register(struct ss14lpp_x1ns_pcie_phy *priv)
{
	s32 err = 0;

	if (priv != NULL) {
		struct phy_provider *provider;
		struct phy *phy;

		phy = devm_phy_create(priv->dev, NULL, &ss14lpp_x1ns_pcie_phy_ops);
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

static s32 ss14lpp_x1ns_pcie_phy_probe(struct platform_device *pdev)
{
	struct ss14lpp_x1ns_pcie_phy *priv;
	s32 err = 0;

	priv = (struct ss14lpp_x1ns_pcie_phy *)devm_kzalloc(&pdev->dev,
			sizeof(struct ss14lpp_x1ns_pcie_phy), GFP_KERNEL);
	if (priv != NULL) {
		err = ss14lpp_x1ns_pcie_phy_of_parse_dt(pdev, priv);
		if (err == 0) {
			priv->dev = &pdev->dev;
			platform_set_drvdata(pdev, priv);
			err = ss14lpp_x1ns_pcie_phy_register(priv);
		}

		if (err == 0) {
			pm_runtime_enable(priv->dev);
		}
	} else {
		err = -ENOMEM;
	}

	return err;
}

static s32 ss14lpp_x1ns_pcie_phy_remove(struct platform_device *pdev)
{
	s32 err = 0;

	if (pdev != NULL) {
		const struct ss14lpp_x1ns_pcie_phy *priv = (const struct ss14lpp_x1ns_pcie_phy *)platform_get_drvdata(pdev);

		pm_runtime_disable(priv->dev);
		err = ss14lpp_x1ns_pcie_phy_disable_clk(priv);
	} else {
		err = -ENODEV;
	}

	return err;
}

static struct platform_driver ss14lpp_x1ns_pcie_phy_driver = {
	.probe		= ss14lpp_x1ns_pcie_phy_probe,
	.remove		= ss14lpp_x1ns_pcie_phy_remove,
	.driver		= {
		.name	= "ss14lpp-pcie-phy",
		.pm	= &ss14lpp_x1ns_pcie_phy_pm_ops,
		.of_match_table = of_match_ptr(ss14lpp_x1ns_pcie_phy_id_table),
	},
};
module_platform_driver(ss14lpp_x1ns_pcie_phy_driver);

MODULE_DESCRIPTION("Telechips Dolphin3 PCIe PHY Driver");
MODULE_LICENSE("GPL v2");
