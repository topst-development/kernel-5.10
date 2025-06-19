// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note OR MIT

/*
 * (C) COPYRIGHT 2019-2023 ARM Limited or its affiliates. All rights reserved.
 */

/*
 * Part of the Mali reference arbiter
 */

#include <linux/completion.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>

#include "mali_arbiter.h"
#include "mali_arbiter_core.h"

/* Device tree compatible ID of the Arbiter */
#define MALI_ARBITER_DT_NAME "arm,mali-arbiter"

/* Module parameters */
static int request_timeout = 7;
module_param(request_timeout, int, 0644);
MODULE_PARM_DESC(request_timeout, "GPU time in ms before switching to another VM");

static int yield_timeout = 7;
module_param(yield_timeout, int, 0644);
MODULE_PARM_DESC(yield_timeout,
		 "Max time in ms for VM to stop before GPU lost handling is invoked");

static int slice_power_off_wait_time = 100;
module_param(slice_power_off_wait_time, int, 0644);
MODULE_PARM_DESC(slice_power_off_wait_time, "Max time in ms to wait before powering off slices");

static int no_timeslice_yield_timeout = 5000;
module_param(no_timeslice_yield_timeout, int, 0644);
MODULE_PARM_DESC(
	no_timeslice_yield_timeout,
	"Max time in ms for a driver to stop before GPU lost handling is invoked, when it is not timesliced with other drivers");

/**
 * arbiter_create() - Create an arbiter instance
 * @dev: Platform device.
 * @pwr: power module function interface.
 * @res: resource group function interface.
 * @arb_data: arbiter private data for resource group to use.
 *
 * Create and initialize the arbiter device and all its resources
 * This function does same thing with arbiter_probe function, but with pre-allocated resources.
 *
 * Return: 0 if success or a Linux error code
 */
int arbiter_create(struct device *dev, struct power_interface pwr, struct resource_interfaces res,
		   struct mali_arb_data **arb_data)
{
	int err;

	/* Function param check */
	if (!dev || !arb_data) {
		pr_err("%s : Invalid argument.\n", __func__);
		return -EINVAL;
	}

	/* Initialize arbiter core */
	err = arbiter_core_create(dev, pwr, res, arb_data, request_timeout, yield_timeout,
				  slice_power_off_wait_time, no_timeslice_yield_timeout);
	if (err)
		return err;

	dev_info(dev, "Arbiter created\n");
	return 0;
}
EXPORT_SYMBOL(arbiter_create);

/**
 * arbiter_destroy() - Destroy an arbiter instance
 * @dev: Platform device.
 * @arb_data: arbiter private data.
 *
 * Create and initialize the arbiter device and all its resources
 * This function does same thing with arbiter_probe function, but with pre-allocated resources.
 */
void arbiter_destroy(struct device *dev, struct mali_arb_data *arb_data)
{
	if (WARN_ON(!dev || !arb_data))
		return;

	arbiter_core_destroy(dev, arb_data);

	dev_info(dev, "Arbiter destroyed\n");
}
EXPORT_SYMBOL(arbiter_destroy);

/**
 * arbiter_probe() - Called when device is matched in device tree
 * @pdev: Platform device
 *
 * Probe and initialize the arbiter device and all its resources
 *
 * Return: 0 if success or a Linux error code
 */
static int arbiter_probe(struct platform_device *pdev)
{
	struct mali_arb_data *arb_data;
	struct power_interface no_pwr = { .dev = NULL, .ops = NULL };
	struct resource_interfaces no_res = { .num_if = 0, .vm_assign = NULL, .repartition = NULL };
	int err;

	/* Module param check */
	if (no_timeslice_yield_timeout < yield_timeout)
		dev_warn(&pdev->dev, "no_timeslice_yield_timeout is shorter than yield_timeout");

	/* Initialize arbiter core */
	err = arbiter_core_create(&pdev->dev, no_pwr, no_res, &arb_data, request_timeout,
				  yield_timeout, slice_power_off_wait_time,
				  no_timeslice_yield_timeout);
	if (err) {
		pr_err("arbiter_probe: failed to create arbiter_core (err:%d)\n", err);
		return err;
	}

	/* Set driver data */
	platform_set_drvdata(pdev, arb_data);

	dev_info(&pdev->dev, "arbiter_probe: probed\n");
	return 0;
}

/**
 * arbiter_remove() - Called when device is removed
 * @pdev: Platform device
 *
 * Free the resources acquired by the arbiter device.
 *
 * Return: 0 if success or a Linux error code
 */
static int arbiter_remove(struct platform_device *pdev)
{
	struct mali_arb_data *arb_data;

	arb_data = platform_get_drvdata(pdev);
	platform_set_drvdata(pdev, NULL);
	if (arb_data)
		arbiter_core_destroy(&pdev->dev, arb_data);

	dev_info(&pdev->dev, "Arbiter removed\n");
	return 0;
}

static const struct of_device_id arbiter_dt_match[] = { { .compatible = MALI_ARBITER_DT_NAME },
							{} };

static struct platform_driver arbiter_driver = {
	.probe = arbiter_probe,
	.remove = arbiter_remove,
	.driver = {
		.name = "mali_arbiter",
		.of_match_table = arbiter_dt_match,
	},
};

/**
 * arbiter_init() - Register platform driver.
 *
 * Return: See definition of platform_driver_register().
 */
static int __init arbiter_init(void)
{
	return platform_driver_register(&arbiter_driver);
}
module_init(arbiter_init);

/**
 * arbiter_exit() - Unregister platform driver
 */
static void __exit arbiter_exit(void)
{
	platform_driver_unregister(&arbiter_driver);
}
module_exit(arbiter_exit);

MODULE_ALIAS("mali-arbiter");
MODULE_DESCRIPTION("Arbiter driver.");
MODULE_AUTHOR("ARM Ltd.");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION("1.0");
