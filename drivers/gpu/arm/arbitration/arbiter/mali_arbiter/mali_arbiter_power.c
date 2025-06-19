// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note OR MIT

/*
 * (C) COPYRIGHT 2023 Telechips Inc
 * (C) COPYRIGHT 2023 ARM Limited or its affiliates. All rights reserved.
 */

/*
 * Part of the Mali reference arbiter
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>

#include <linux/mali_gpu_power.h>

#include "mali_arbiter.h"
#include "mali_arbiter_power.h"

/**
 * struct mali_arb_power_priv - Internal Arbiter power state information
 * @arb_dev: Device instance for arbiter.
 * @arb_priv: Arbiter private data for power callbacks.
 * @arb_id: arbiter unique id set by GPU power module.
 * @pwr_if: GPU power interface data.
 * @pwr_module: GPU power module reference.
 * @pwr_cb: GPU power callback operations.
 * @global_pwr_ctrl: flag to indicate whether global power control is allowed.
 *                   this will be disabled on PTM/AM.
 */
struct mali_arb_power_priv {
	struct device *arb_dev;
	struct mali_arb_data *arb_priv;
	u32 arb_id;

	struct power_interface pwr_if;
	struct module *pwr_module;
	struct mali_gpu_power_arbiter_cb_ops pwr_cb;

	u8 global_pwr_ctrl;
};

#if !defined(CONFIG_TCC_MALI_VZ) || \
        (defined(CONFIG_TCC_MALI_VZ) && defined(CONFIG_TCC807X_CA55_SUB))
/**
 * arbiter_get_pwr() - gets the GPU Power interfaces
 * @priv: Arbiter power submodule private data
 *
 * This function searches in the DT the presence of the mali_gpu_power node.
 * If the definition of the node is found, the function will attempt to take
 * a reference to its module and in case of error will return a EPROBE_DEFER.
 * If the definition of the node is not found, the function will return success
 * without any further operation.
 *
 * NOTE:If this function finds the GPU power device in the DT and the module is
 * loaded, it will take a reference count to the underling module which therefore
 * will need to be release outside the function whenever not used anymore.
 *
 * Return: 0 on success, or error code
 *
 */
static int arbiter_get_pwr(struct mali_arb_power_priv *priv)
{
	struct device_node *pwr_node;
	struct platform_device *pwr_pdev;
	struct mali_gpu_power_data *pwr_data;
	int err = 0;

	if (WARN_ON(!priv))
		return -EINVAL;

	/* find the gpu power device in Device Tree */
	pwr_node = of_parse_phandle(priv->arb_dev->of_node, "platform", 0);
	if (!pwr_node) {
		dev_info(priv->arb_dev, "No GPU Power in Arbiter Device Tree Node\n");
		/* no arbiter interface defined in device tree. Return success. */
		return 0;
	}

	pwr_pdev = of_find_device_by_node(pwr_node);
	if (!pwr_pdev) {
		dev_err(priv->arb_dev, "Failed to find GPU Power device\n");
		return -EPROBE_DEFER;
	}

	if (!pwr_pdev->dev.driver || !pwr_pdev->dev.driver->owner ||
	    !try_module_get(pwr_pdev->dev.driver->owner)) {
		dev_err(priv->arb_dev, "GPU Power module not available\n");
		err = -EPROBE_DEFER;
		goto cleanup_pwr_pdev;
	}
	priv->pwr_if.dev = &pwr_pdev->dev;
	priv->pwr_module = pwr_pdev->dev.driver->owner;

	pwr_data = platform_get_drvdata(pwr_pdev);
	if (!pwr_data) {
		dev_err(priv->arb_dev, "GPU Power device not ready\n");
		err = -EPROBE_DEFER;
		goto cleanup_module;
	}
	priv->pwr_if.ops = &pwr_data->ops;

	return 0;

cleanup_module:
	module_put(priv->pwr_module);
	priv->pwr_module = NULL;
cleanup_pwr_pdev:
	put_device(&pwr_pdev->dev);
	return err;
}
#endif

/**
 * arbiter_init_pwr() - Initializes the GPU Power interfaces
 * @priv: Arbiter data
 *
 * This function initializes the GPU Power interfaces in the arbiter and
 * registers relevant Arbiter ops with the GPU Power.
 *
 * Return: 0 on success, or error code
 *
 */
static int arbiter_init_pwr(struct mali_arb_power_priv *priv)
{
	int err;

	if (WARN_ON(!priv))
		return -EINVAL;

	if (!priv->pwr_if.ops || !priv->pwr_if.dev) {
		dev_err(priv->arb_dev, "GPU Power interface data not valid.\n");
		return -EINVAL;
	}

	/* register arbiter callbacks */
	if (priv->pwr_if.ops->register_arb == NULL || priv->pwr_if.ops->unregister_arb == NULL) {
		dev_err(priv->arb_dev, "GPU Power callbacks are not available.\n");
		return -EFAULT;
	}

	/* priv->arb_id will be updated by GPU power node when arbiter registers */
	err = priv->pwr_if.ops->register_arb(priv->pwr_if.dev, priv->arb_priv, &priv->pwr_cb,
					     &priv->arb_id);
	if (err) {
		dev_err(priv->arb_dev,
			"Failed to register arbiter with GPU power device. (err = %d)\n", err);
//Telechips
		dev_err(priv->arb_dev,
			"Telechips : Ignore fail for dvfs during bring-up. This log will be deleted after CONFIG_DEVFREQ_GOV_XX is set\n");
//		return -err;
	}

	return 0;
}

/** Initialize arbiter power submodule
 */
int mali_arbiter_power_init(struct device *arb_dev, struct mali_arb_data *arb_priv,
			    struct power_interface pwr_if,
			    struct mali_gpu_power_arbiter_cb_ops pwr_cb, void **pwr_priv)
{
	struct mali_arb_power_priv *priv;
	int err;

	/* Allocate memory for priv data. */
	priv = devm_kzalloc(arb_dev, sizeof(struct mali_arb_power_priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	priv->arb_dev = arb_dev;
	priv->arb_priv = arb_priv;

	/* Assign power interface. */
	if (pwr_if.dev) {
		priv->pwr_if = pwr_if;
	} else {
#if !defined(CONFIG_TCC_MALI_VZ) || \
	(defined(CONFIG_TCC_MALI_VZ) && defined(CONFIG_TCC807X_CA55_SUB))
		/* pwr_if.dev will be NULL on PV case.
		 * So, try to get the platform specific arbiter integration device
		 * if specified in the DT, otherwise move forward with the init.
		 */
		err = arbiter_get_pwr(priv);
		if (err) {
			dev_err(priv->arb_dev, "Failed to fetch GPU Power device. (err = %d)\n",
				err);
			goto cleanup;
		}

		/* We do global power control only for PV. */
		priv->global_pwr_ctrl = 1;
#endif
	}

	/* Initialize arbiter power */
	if (priv->pwr_if.dev) {
		priv->pwr_cb = pwr_cb;
		err = arbiter_init_pwr(priv);
		if (err) {
			dev_err(priv->arb_dev,
				"Failed to initialize GPU Power interface. (err = %d)\n", err);
			goto cleanup_pwr_module;
		}
	}
	*pwr_priv = (void *)priv;

	return 0;

cleanup_pwr_module:
	if (priv->pwr_module) {
		module_put(priv->pwr_module);
		priv->pwr_module = NULL;
	}
#if !defined(CONFIG_TCC_MALI_VZ) || \
	(defined(CONFIG_TCC_MALI_VZ) && defined(CONFIG_TCC807X_CA55_SUB))
cleanup:
	devm_kfree(priv->arb_dev, priv);
#endif
	return err;
}

/** Release arbiter power submodule resources
 */
void mali_arbiter_power_deinit(void *pwr_priv)
{
	struct mali_arb_power_priv *priv = (struct mali_arb_power_priv *)pwr_priv;

	/* Unregister arbiter from power. */
	if (priv->pwr_if.dev) {
		if (priv->pwr_if.ops->unregister_arb)
			priv->pwr_if.ops->unregister_arb(priv->pwr_if.dev, priv->arb_id);
	}

	/* Clean-up power module when it was assigned from arbiter power submodule. */
	if (priv->pwr_module) {
		module_put(priv->pwr_module);
		priv->pwr_module = NULL;
		put_device(priv->pwr_if.dev);
	}

	/* Free private data */
	devm_kfree(priv->arb_dev, priv);
}

/** Enable GPU power
 */
void mali_arbiter_power_enable(void *pwr_priv)
{
	struct mali_arb_power_priv *priv = (struct mali_arb_power_priv *)pwr_priv;

	/* enable power only when Global power control is allowed */
	if (priv->pwr_if.dev && priv->global_pwr_ctrl) {
		if (priv->pwr_if.ops->gpu_active)
			priv->pwr_if.ops->gpu_active(priv->pwr_if.dev);
	}
}

/** Disable GPU power
 */
void mali_arbiter_power_disable(void *pwr_priv)
{
	struct mali_arb_power_priv *priv = (struct mali_arb_power_priv *)pwr_priv;

	/* disable power only when Global power control is allowed */
	if (priv->pwr_if.dev && priv->global_pwr_ctrl) {
		if (priv->pwr_if.ops->gpu_idle)
			priv->pwr_if.ops->gpu_idle(priv->pwr_if.dev);
	}
}
