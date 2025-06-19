/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note OR MIT */

/*
 * (C) COPYRIGHT 2023 Arm Limited or its affiliates. All rights reserved.
 */

/*
 * Part of the Mali reference arbiter
 */

#include <linux/kernel.h>
#include "mali_arbiter.h"

/**
 * arbiter_core_create() - Create the Arbiter instance and allocate resources
 * @dev: Kernel Device instance that will contain the arbiter instance.
 * @pwr: Power interface instance given from caller. This can be empty in which case
 *       it will be initialized in this function.
 * @res: Resource interface instance given from caller. When this is empty, it means
 *       the caller is likely not a resource group module.
 * @arb_data: out-parameter that will contain Arbiter data.
 * @request_timeout: GPU time in ms before switching to another VM.
 * @yield_timeout: Max time in ms for VM to stop before GPU lost handling is invoked.
 * @slice_power_off_wait_time: Max time in ms to wait before powering off slices.
 * @no_timeslice_yield_timeout: Max time in ms for a driver to stop before GPU lost
 *                              handling is invoked, when it is not timesliced with
 *                              other drivers.
 *
 * Initializes the arbiter core with all the common internal data.
 *
 * Return: 0 on success, or error code
 */
int arbiter_core_create(struct device *dev, struct power_interface pwr,
			struct resource_interfaces res, struct mali_arb_data **arb_data,
			int request_timeout, int yield_timeout, int slice_power_off_wait_time,
			int no_timeslice_yield_timeout);

/**
 * arbiter_core_destroy() - Terminates the Arbiter instance and release its resources
 * @dev:      Device that contains the arbiter instance. This could either be a
 *            dedicated virtual device for the arbiter or contained in the
 *            platform-specific virtualization device, such as resource group or
 *            xenbus driver
 * @arb_data: Public arbiter data exposed to the backend module.
 *
 * This function is called when the device is removed to free up all the
 * resources.
 */
void arbiter_core_destroy(struct device *dev, struct mali_arb_data *arb_data);
