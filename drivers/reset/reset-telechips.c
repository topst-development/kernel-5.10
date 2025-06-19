// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Telechips Inc.
 */

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <soc/telechips/smc.h>

union of_data {
	const void *priv;
	ulong data;
};

struct reset_data {
	struct reset_controller_dev rcdev;
	spinlock_t lock;
	union of_data fn;
	unsigned int reset_range[2];
};

static struct reset_data *to_tcc_reset_data(struct reset_controller_dev *rcdev)
{
	return container_of(rcdev, struct reset_data, rcdev);
}

static inline void reset_ctrl(ulong fn, ulong id, ulong op, spinlock_t *lock)
{
	struct arm_smccc_res res;
	ulong flags;

	spin_lock_irqsave(lock, flags);
	arm_smccc_smc(fn, id, op, 0, 0, 0, 0, 0, &res);
	spin_unlock_irqrestore(lock, flags);
}

static int reset_common(struct reset_controller_dev *rcdev, ulong id, ulong op)
{
	struct reset_data *data = to_tcc_reset_data(rcdev);
	const char *desc = (op == 1UL) ? "Assert" : "Release";

	dev_dbg(rcdev->dev, "%s soft reset %lu\n", desc, id);
	reset_ctrl(data->fn.data, id, op, &data->lock);

	return 0;
}

static int reset_assert(struct reset_controller_dev *rcdev, unsigned long id)
{
	return reset_common(rcdev, id, 1UL);
}

static int reset_deassert(struct reset_controller_dev *rcdev, unsigned long id)
{
	return reset_common(rcdev, id, 0UL);
}

static const struct reset_control_ops reset_ops = {
	.assert		= reset_assert,
	.deassert	= reset_deassert,
};

static int tcc_reset_of_xlate(struct reset_controller_dev *rcdev,
			      const struct of_phandle_args *reset_spec)
{
	int ret;
	u32 id = reset_spec->args[0];
	const u32 range_max = (~((u32)0) >> 1U);
	const struct reset_data *data = to_tcc_reset_data(rcdev);

	if ((id >= data->reset_range[0]) && (id <= data->reset_range[1])) {
		ret = (id <= range_max) ? (int)id : -EINVAL;
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static int tcc_reset_get_range(const struct device_node *np, u32 *reset_range)
{
	int ret;
	const u32 nr_max = (~((u32)0) >> 1U) - 1U;

	ret = of_property_read_u32_array(np, "reset-range", reset_range, 2U);
	if (ret == 0) {
		if ((reset_range[0] <= reset_range[1]) &&
		    ((reset_range[1] - reset_range[0]) <= nr_max)) {
			u32 nr_id = reset_range[1] - reset_range[0] + 1U;

			ret = (int)nr_id;
		}
	}

	return ret;
}

static int tcc_reset_data_init(struct device *dev, struct reset_data *data)
{
	int ret;
	const char *fail = NULL;
	const struct device_node *np = dev->of_node;

	/* Get SiP service call ID for reset control */
	data->fn.priv = of_device_get_match_data(dev);
	if (data->fn.priv == NULL) {
		fail = "Failed to get SiP service call";
		ret = -EINVAL;
	} else {
		struct reset_controller_dev *rcdev = &data->rcdev;

		/* Register reset controller */
		rcdev->owner = THIS_MODULE;
		rcdev->ops = &reset_ops;
		rcdev->of_node = dev->of_node;
		rcdev->of_reset_n_cells = 1;

		ret = tcc_reset_get_range(np, data->reset_range);
		if (ret < 0) {
			fail = "Failed to get reset-range";
		} else if (ret == 0) {
			/*
			 * If reset-range is incorrect, use default of_xlate()
			 * so that the it always returns -EINVAL.
			 */
			rcdev->of_xlate = NULL;
			rcdev->nr_resets = 0;

			dev_err(dev, "Invalid reset-range\n");
		} else {
			rcdev->of_xlate = tcc_reset_of_xlate;
			rcdev->nr_resets = (u32)ret;

			dev_info(dev, "reset-range [%u - %u]\n",
				 data->reset_range[0], data->reset_range[1]);
		}
	}

	if (fail == NULL) {
		spin_lock_init(&data->lock);

		ret = devm_reset_controller_register(dev, &data->rcdev);
		if (ret != 0) {
			fail = "Failed to register reset controller";
		}
	}

	if (fail != NULL) {
		dev_err(dev, "%s (err: %d)\n", fail, ret);
	}

	return ret;
}

static int tcc_reset_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct reset_data *data;
	int ret;

	/* Allocate memory for driver data */
	data = devm_kzalloc(dev, sizeof(struct reset_data), GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		dev_err(dev, "Failed to allocate driver data (err: %d)\n", ret);
	} else {
		/* Initialize tcc-reset driver data */
		ret = tcc_reset_data_init(dev, data);
		if (ret != 0) {
			devm_kfree(dev, data);
		} else {
			platform_set_drvdata(pdev, data);
		}
	}

	return ret;
}

static const struct of_device_id tcc_reset_match[6] = {
	{
		.compatible = "telechips,reset",
		.data = (void *)SIP_CLK_WDPR_SWRESET,
	},
	{
		.compatible = "telechips,vpubus-reset",
		.data = (void *)SIP_CLK_WDPR_RESET_VPUBUS,
	},
	{
		.compatible = "telechips,ddibus-reset",
		.data = (void *)SIP_CLK_WDPR_RESET_DDIBUS,
	},
	{
		.compatible = "telechips,iobus-reset",
		.data = (void *)SIP_CLK_WDPR_RESET_IOBUS,
	},
	{
		.compatible = "telechips,hsiobus-reset",
		.data = (void *)SIP_CLK_WDPR_RESET_HSIOBUS,
	},
	{ .compatible = "" }
};

MODULE_DEVICE_TABLE(of, tcc_reset_match);

static struct platform_driver tcc_reset_driver = {
	.probe = tcc_reset_probe,
	.driver = {
		.name = "tcc-reset",
		.of_match_table = of_match_ptr(tcc_reset_match),
	},
};

static int __init tcc_reset_init(void)
{
	return platform_driver_register(&tcc_reset_driver);
}

postcore_initcall(tcc_reset_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jigi Kim <jigi.kim@telechips.com>");
MODULE_DESCRIPTION("Telechips reset driver");
