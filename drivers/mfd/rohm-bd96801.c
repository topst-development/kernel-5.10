// SPDX-License-Identifier: GPL-2.0-or-later
//
// Copyright (C) 2020 ROHM Semiconductors
//
// ROHM BD96801 PMIC driver

#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/mfd/core.h>
#include <linux/mfd/rohm-bd96801.h>
#include <linux/mfd/rohm-generic.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/types.h>

static struct mfd_cell bd96801_mfd_cells[] = {
	{
		.name = "bd96801-pmic",
		.resources = 0,
		.num_resources = 0,
	},
};

static const struct regmap_range bd96801_volatile_ranges[] = {
	/* Status regs */
	regmap_reg_range(BD96801_REG_PWR_CTRL, BD96801_REG_PWR_CTRL),
	regmap_reg_range(BD96801_REG_WD_FEED, BD96801_REG_WD_FAILCOUNT),
	regmap_reg_range(BD96801_REG_WD_ASK, BD96801_REG_WD_ASK),
	regmap_reg_range(BD96801_REG_WD_STATUS, BD96801_REG_WD_STATUS),
	regmap_reg_range(BD96801_REG_PMIC_STATE, BD96801_REG_INT_LDO7_INTB),
	/* Registers which do not update value unless PMIC is in STBY */
	regmap_reg_range(BD96801_REG_SSCG_CTRL, BD96801_REG_SHD_INTB),
	regmap_reg_range(BD96801_REG_BUCK_OVP, BD96801_REG_BOOT_OVERTIME),
	/*
	 * LDO control registers have single bit (LDO MODE) which does not
	 * change when we write it unless PMIC is in STBY. It's safer to not
	 * cache it.
	 */
	regmap_reg_range(BD96801_LDO5_VOL_LVL_REG, BD96801_LDO7_VOL_LVL_REG),
};

static const struct regmap_access_table volatile_regs = {
	.yes_ranges = bd96801_volatile_ranges,
	.n_yes_ranges = ARRAY_SIZE(bd96801_volatile_ranges),
};

static const struct regmap_config bd96801_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.volatile_table = &volatile_regs,
	.cache_type = REGCACHE_RBTREE,
};


static int bd96801_i2c_probe(struct i2c_client *i2c)
{
	struct regmap *regmap;
	int ret;

	regmap = devm_regmap_init_i2c(i2c, &bd96801_regmap_config);

	if (IS_ERR(regmap)) {
		dev_err(&i2c->dev, "regmap initialization failed\n");
		return PTR_ERR(regmap);
	}

	ret = devm_mfd_add_devices(&i2c->dev, PLATFORM_DEVID_AUTO,
				   bd96801_mfd_cells,
				   ARRAY_SIZE(bd96801_mfd_cells), NULL, 0,
				   regmap_irq_get_domain(NULL));
	if (ret)
		dev_err(&i2c->dev, "Failed to create subdevices\n");

	return ret;
}

static const struct of_device_id bd96801_of_match[] = {
	{
		.compatible = "rohm,bd96801",
	},
	{ }
};
MODULE_DEVICE_TABLE(of, bd96801_of_match);

static struct i2c_driver bd96801_i2c_driver = {
	.driver = {
		.name = "rohm-bd96801",
		.of_match_table = bd96801_of_match,
	},
	.probe_new = bd96801_i2c_probe,
};

static int __init bd96801_i2c_init(void)
{
	return i2c_add_driver(&bd96801_i2c_driver);
}

/* Initialise early so consumer devices can complete system boot? */
subsys_initcall(bd96801_i2c_init);

static void __exit bd96801_i2c_exit(void)
{
	i2c_del_driver(&bd96801_i2c_driver);
}
module_exit(bd96801_i2c_exit);

MODULE_AUTHOR("Matti Vaittinen <matti.vaittinen@fi.rohmeurope.com>");
MODULE_DESCRIPTION("ROHM BD96801 Power Management IC driver");
MODULE_LICENSE("GPL");
