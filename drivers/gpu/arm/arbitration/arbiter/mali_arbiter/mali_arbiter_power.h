/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note OR MIT */

/*
 * (C) COPYRIGHT 2023 ARM Limited or its affiliates. All rights reserved.
 */

/*
 * Part of the Mali reference arbiter
 */

#include <linux/mali_gpu_power.h>
#include "mali_arbiter.h"

/**
 * mali_arbiter_power_init() - Initialize arbiter power submodule
 * @arb_dev:  Arbiter device instance.
 * @arb_priv: Arbiter private data pointer to find arbiter instance in callbacks.
 * @pwr_if:   Power interface given from arbiter core.
 * @pwr_cb:  Power operation callbacks given from arbiter core.
 * @pwr_priv: Arbiter power submodule private data.
 *
 * Initializes the arbiter power sub-module with given power-interface.
 * For PV, power interface parameter will be empty and this function will
 * initialize one from the Device-Tree.
 *
 * Return: 0 on success, or error code
 */
int mali_arbiter_power_init(struct device *arb_dev, struct mali_arb_data *arb_priv,
			    struct power_interface pwr_if,
			    struct mali_gpu_power_arbiter_cb_ops pwr_cb, void **pwr_priv);

/**
 * mali_arbiter_power_deinit() - Release arbiter power submodule resources
 * @pwr_priv: Arbiter power submodule private data.
 *
 * This function is called when the device is removed to free up power resources.
 */
void mali_arbiter_power_deinit(void *pwr_priv);

/**
 * mali_arbiter_power_enable() - Enable GPU power
 * @pwr_priv: Arbiter power submodule private data.
 *
 * This function tries to enable power with Power operation interface.
 */
void mali_arbiter_power_enable(void *pwr_priv);

/**
 * mali_arbiter_power_disable() - Disable GPU power
 * @pwr_priv: Arbiter power submodule private data.
 *
 * This function tries to disable power with Power operation interface.
 */
void mali_arbiter_power_disable(void *pwr_priv);
