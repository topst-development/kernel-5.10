// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
/*
 *
 * (C) COPYRIGHT 2023 Telechips Inc
 * (C) COPYRIGHT 2019-2023 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 */

/*
 * Part of the Mali arbiter interface related to GPU power operations.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/devfreq.h>
#include <linux/clk.h>
#include <linux/pm_opp.h>
#include <linux/version.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/mutex.h>
#include <linux/mali_gpu_power.h>
#include <linux/list.h>
#include <linux/debugfs.h>
#include <linux/workqueue.h>
#include <linux/of_address.h>

/* Added by Telechips to support Suspend to RAM */
#if IS_ENABLED(CONFIG_ARCH_TELECHIPS)
#define GPU_3DENGINE_CLKMASK                    (0x0)
#define GPU_3DENGINE_CLKMASK_STATUSCHECK        (1)
#define GPU_3DENGINE_CLKMASK_STATUSCHECK_MASK            \
                (0x1 << GPU_3DENGINE_CLKMASK_STATUSCHECK)
#define GPU_3DENGINE_SWRESET                    (0x4)
#define GPU_3DENGINE_SWRESET_3D_SHIFT           (0)
#define GPU_3DENGINE_SWRESET_3DRECOVERY_SHIFT   (1)
#define GPU_3DENGINE_SWRESET_ADB_SHIFT          (2)
#define GPU_3DENGINE_SWRESET_3D_MASK            \
                (0x1 << GPU_3DENGINE_SWRESET_3D_SHIFT)
#define GPU_3DENGINE_SWRESET_3DRECOVERY_MASK            \
                (0x1 << GPU_3DENGINE_SWRESET_3DRECOVERY_SHIFT)
#define GPU_3DENGINE_SWRESET_ADB_MASK           \
                (0x1 << GPU_3DENGINE_SWRESET_ADB_SHIFT)
#define GPU_3DENGINE_SWRESET_FULL_MASK          \
                (GPU_3DENGINE_SWRESET_3D_MASK | \
                 GPU_3DENGINE_SWRESET_3DRECOVERY_MASK | \
                 GPU_3DENGINE_SWRESET_ADB_MASK)


#define GPU_3DENGINE_DEBUG                    (0x24)
#define GPU_3DENGINE_DEBUG_SYS_ASSIGN_ENABLE_SHIFT           (28)
#define GPU_3DENGINE_DEBUG_SYS_ASSIGN_ENABLE_SA            \
                (0x0 << GPU_3DENGINE_DEBUG_SYS_ASSIGN_ENABLE_SHIFT)
#define GPU_3DENGINE_DEBUG_SYS_ASSIGN_ENABLE_SB            \
                (0x1 << GPU_3DENGINE_DEBUG_SYS_ASSIGN_ENABLE_SHIFT)
#endif

/* GPU Power version against which was implemented this file */
#define MALI_IMPLEMENTED_GPU_POWER_VERSION 1
#if MALI_IMPLEMENTED_GPU_POWER_VERSION != MALI_GPU_POWER_VERSION
#error "Unsupported gpu power version."
#endif

/*
 * Module's compatible strings
 */
#define MALI_GPU_POWER_DEV_NAME "mali_gpu_power"
#define MALI_GPU_POWER_DT_NAME "arm,mali-gpu-power"
#define MALI_GPU_POWER_PTM_DT_NAME "arm,mali-ptm"

#if (KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE)
#define MALI_GPU_AW_PTM_DT_NAME "arm,mali-gpu-aw-message"
#define MALI_GPU_KBASE_PTM_DT_NAME "arm,mali-midgard"
#define MALI_GPU_PT_CFG_PTM_DT_NAME "arm,mali-gpu-partition-config"
#define MALI_GPU_PT_CTRL_PTM_DT_NAME "arm,mali-gpu-partition-control"
#define MALI_GPU_RG_PTM_DT_NAME "arm,mali-gpu-resource-group"
#define MALI_GPU_ASSIGN_PTM_DT_NAME "arm,mali-gpu-assign"
#define MALI_GPU_SYSTEM_PTM_DT_NAME "arm,mali-gpu-system"
#define MALI_GPU_IF_PTM_DT_NAME "arm,mali-ptm-interface"
#define TELECHIPS_MALI_GPU_3D_CFG_DT_NAME "telechips,mali-gpu-3d-cfg"
/*
 * The maximum number of nodes to traverse looking for the power device
 */
#define MAX_PARENTS (3)

#endif /* KERNEL_VERSION */

/*
 * How long pm runtime should wait in milliseconds before suspending GPU
 */
#define AUTO_SUSPEND_DELAY (100)

/* Used for conversion between Hz and kHz */
#define KHZ_TO_HZ 1000

#if (KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE)
/* List of devices supported by this GPU power bus. */
static const char *const gpu_bus_devices[] = {
	MALI_GPU_AW_PTM_DT_NAME,     MALI_GPU_KBASE_PTM_DT_NAME,
	MALI_GPU_PT_CFG_PTM_DT_NAME, MALI_GPU_PT_CTRL_PTM_DT_NAME,
	MALI_GPU_RG_PTM_DT_NAME,     MALI_GPU_ASSIGN_PTM_DT_NAME,
	MALI_GPU_SYSTEM_PTM_DT_NAME, MALI_GPU_IF_PTM_DT_NAME,
	MALI_GPU_POWER_PTM_DT_NAME,  NULL
};
#endif /* KERNEL_VERSION */

/**
 * struct mali_arb_info - Arbiter info registered to GPU power node
 * @arbiter:                The arbiter that we can query about GPU
 *                          utilization
 * @arbiter_ops:            callback(s) for communicating with arbiter,
 *                          to query utilization
 * @arb_val:                arbiter unique id assigned by plat node
 * @arb_list:               arbiter list
 */
struct mali_arb_info {
	void *arbiter;
	struct mali_gpu_power_arbiter_cb_ops arbiter_ops;
	int arb_val;
	struct list_head arb_list;
};

/**
 * struct mali_gpu_power - Internal DVFS Integrator state information
 * @mutex:                  Protect DVFS callback and arbiter callback threads
 * @pwr_data:               Public GPU power integration device data.
 * @dev:                    Device for DVFS integrator
 * @arb_list:               arbiter list
 * @arb_count:              arbiter count to assign a unique id
 * @clock:                  Pointer to the input clock resource
 *                          (having an id of 0),
 *                          referenced by the GPU device node.
 * @devfreq_profile:        devfreq profile for the Mali GPU device,
 *                          passed to devfreq_add_device()
 *                          to add devfreq feature to Mali GPU device.
 * @using_ptm:              True if a Partition Manager is in use by the GPU
 * @devfreq:                Pointer to devfreq structure for Mali GPU device,
 *                          returned on the call to devfreq_add_device().
 * @current_freq:           The real frequency, corresponding
 *                          to @current_nominal_freq, at which the
 *                          Mali GPU device is currently operating, as
 *                          retrieved from @opp_table in the target callback of
 *                          @devfreq_profile.
 * @current_nominal_freq:   The nominal frequency currently used for
 *                          the Mali GPU device as retrieved through
 *                          devfreq_recommended_opp() using the freq value
 *                          passed as an argument to target callback
 *                          of @devfreq_profile
 * @current_voltage:        The voltage corresponding to @current_nominal_freq,
 *                          as retrieved through dev_pm_opp_get_voltage().
 * @regulator:              The regulator associated with the GPU.
 * @mali_gpu_power_dfs:     Directory created for mali_gpu_power debugfs
 * @max_utilization:        Max utilization value
 * @child_devices_created:  Whether the child device nodes have been created
 * @system_dev:             Pointer to system device instance
 * @assign_dev:             Pointer to assign device instance
 * @task_wq:                Work queue used to create children devices
 *
 * Structure used to handle DVFS information
 */
struct mali_gpu_power {
	struct mutex mutex;
	struct mali_gpu_power_data pwr_data;
	struct device *dev;
#if IS_ENABLED(CONFIG_ARCH_TELECHIPS)
	void __iomem *base_addr;
#endif
	struct list_head arb_list;
	int arb_count;
	struct devfreq *devfreq;
	struct devfreq_dev_profile devfreq_profile;
	bool using_ptm;

	struct clk *clock;
	unsigned long current_freq;
	unsigned long current_nominal_freq;
	unsigned long current_voltage;
#if IS_ENABLED(CONFIG_REGULATOR)
	struct regulator *regulator;
#endif
	struct dentry *mali_gpu_power_dfs;
	u8 max_utilization;

	bool child_devices_created;
	struct device *system_dev;
	struct device *assign_dev;
	struct workqueue_struct *task_wq;
};

/**
 * struct task_item - Structure containing parameters to a scheduled job.
 * @work:                   work to be done
 * @bus_dev:                GPU Bus Notifier device used by create_children_work
 * @action:                 GPU Bus action (see device/bus.h)
 * @notifier_data:          Notifier data (device pointer)
 * @compat:                 GPU Bus device filter
 */
struct task_item {
	struct work_struct work;
	struct device *bus_dev;
	unsigned long action;
	void *notifier_data;
	const char *const *compat;
};

/**
 * gpu_power_from_dev() - Convert device to a pointer to mali_gpu_power
 * @dev: GPU Power device
 *
 * Extract a pointer to the struct mali_gpu_power from the mali_gpu_power_data
 *
 * Return: gpu_power or NULL if input parameter was NULL
 */
static inline struct mali_gpu_power *gpu_power_from_dev(struct device *dev)
{
	struct mali_gpu_power_data *pwr_data;

	pwr_data = dev_get_drvdata(dev);
	if (likely(pwr_data))
		return container_of(pwr_data, struct mali_gpu_power, pwr_data);
	return NULL;
}

/**
 * devfreq_target() - Modify the target frequency of the device
 * @dev: GPU Power device of which the frequency should be updated
 * @target_freq: Minimum frequency that should be set into the device
 * @flags: Additional flags
 *
 * This function should pick a frequency at least as high as the passed
 * in *target_freq, then update *target_freq to reflect the actual
 * frequency chosen
 *
 * Return: 0 if success or an error code
 */
static int devfreq_target(struct device *dev, unsigned long *target_freq, u32 flags)
{
	int err;
	unsigned long new_freq = 0;
	unsigned long new_freq_khz;
	struct dev_pm_opp *new_opp;
	unsigned long new_voltage;
	struct mali_arb_info *arb_node;
	struct mali_gpu_power *gpu_power;

	if (!dev || !target_freq)
		return -EINVAL;

	gpu_power = gpu_power_from_dev(dev);
	if (!gpu_power)
		return -ENODEV;

#if KERNEL_VERSION(4, 11, 0) > LINUX_VERSION_CODE
	rcu_read_lock();
#endif
	new_opp = devfreq_recommended_opp(dev, target_freq, flags);
	if (IS_ERR_OR_NULL(new_opp)) {
		dev_err(dev, "Failed to get opp %d\n", PTR_ERR_OR_ZERO(new_opp));
#if KERNEL_VERSION(4, 11, 0) > LINUX_VERSION_CODE
		rcu_read_unlock();
#endif
		return !new_opp ? -ENODEV : PTR_ERR(new_opp);
	}
	new_voltage = dev_pm_opp_get_voltage(new_opp);
	new_freq = dev_pm_opp_get_freq(new_opp);

#if KERNEL_VERSION(4, 11, 0) > LINUX_VERSION_CODE
	rcu_read_unlock();
#endif

#if KERNEL_VERSION(4, 11, 0) <= LINUX_VERSION_CODE
	dev_pm_opp_put(new_opp);
#endif
	/* Only update if there is a change of frequency */
	if (gpu_power->current_nominal_freq == new_freq) {
		*target_freq = new_freq;
		return 0;
	}

#if IS_ENABLED(CONFIG_REGULATOR)
	if (gpu_power->regulator && gpu_power->current_voltage != new_voltage &&
	    gpu_power->current_freq < new_freq) {
		err = regulator_set_voltage(gpu_power->regulator, new_voltage, new_voltage);
		if (err) {
			dev_err(dev, "Failed to increase voltage %d\n", err);
			return err;
		}
		gpu_power->current_voltage = new_voltage;
		dev_info(dev, "voltage set to %lu", new_voltage);
	}
#endif

	err = clk_set_rate(gpu_power->clock, new_freq);
	if (err) {
		dev_err(gpu_power->dev, "clk_set_rate returned error %d\n", err);
		return err;
	}

	if (gpu_power->current_nominal_freq != new_freq) {
		dev_info(gpu_power->dev, "clk_set_rate to %lu\n", new_freq);
		gpu_power->current_nominal_freq = new_freq;
	}

#if IS_ENABLED(CONFIG_REGULATOR)
	if (gpu_power->regulator && gpu_power->current_voltage != new_voltage &&
	    gpu_power->current_freq > new_freq) {
		err = regulator_set_voltage(gpu_power->regulator, new_voltage, new_voltage);
		if (err) {
			dev_err(dev, "Failed to decrease voltage %d\n", err);
			return err;
		}
		gpu_power->current_voltage = new_voltage;
		dev_info(dev, "voltage set to %lu", new_voltage);
	}
#endif
	new_freq_khz = new_freq;
#if BITS_PER_LONG == 64
	do_div(new_freq_khz, KHZ_TO_HZ);
#elif BITS_PER_LONG == 32
	new_freq_khz /= KHZ_TO_HZ;
#else
#error "unsigned long division is not supported for this architecture"
#endif

	mutex_lock(&gpu_power->mutex);
	list_for_each_entry(arb_node, &gpu_power->arb_list, arb_list) {
		if (arb_node->arbiter_ops.update_freq)
			arb_node->arbiter_ops.update_freq(arb_node->arbiter, new_freq_khz);
	}
	mutex_unlock(&gpu_power->mutex);
	return 0;
}

/**
 * devfreq_status() - Get frequency status of the device
 * @dev: GPU Power device of which the frequency status is to be taken
 * @stat: Pointer to be updated with the device frequency status
 *
 * Return: 0 always
 */
static int devfreq_status(struct device *dev, struct devfreq_dev_status *stat)
{
	struct mali_gpu_power *gpu_power;
	u8 utilization_percent = 0;
	u32 arb_max_utilization_id = 0;
	struct mali_arb_info *arb_node;

	if (!dev || !stat)
		return -EINVAL;

	gpu_power = gpu_power_from_dev(dev);
	if (!gpu_power)
		return -ENODEV;

	mutex_lock(&gpu_power->mutex);

	stat->busy_time = 0;
	stat->total_time = 0;
	stat->private_data = NULL;
	stat->current_frequency = gpu_power->current_nominal_freq;

	/* report busy time since last call */
	list_for_each_entry(arb_node, &gpu_power->arb_list, arb_list) {
		u32 busy_time = 0, total_time = 0;
		u8 tmp_percent;

		if (!arb_node->arbiter_ops.arb_get_utilisation)
			continue;

		arb_node->arbiter_ops.arb_get_utilisation(arb_node->arbiter, &busy_time,
							  &total_time);

		/* check to avoid division by zero */
		if (total_time == 0)
			continue;

		tmp_percent = div_u64((u64)busy_time * 100, total_time);

		/*  Updating max GPU utilization */
		if (tmp_percent > utilization_percent) {
			stat->busy_time = busy_time;
			stat->total_time = total_time;
			arb_max_utilization_id = arb_node->arb_val;
			utilization_percent = tmp_percent;
		}
	}

	/* It reports the arbiter with maximum GPU utilization */
	dev_dbg(gpu_power->dev, "arb_%d busy_time = %lu, total_time = %lu (~ %u%%)\n",
		arb_max_utilization_id, stat->busy_time, stat->total_time, utilization_percent);

	gpu_power->max_utilization = utilization_percent;
	mutex_unlock(&gpu_power->mutex);
	return 0;
}

/**
 * devfreq_exit() - Exit Device freq structure of the Mali GPU device
 * @dev: GPU Power device of which the frequency to be exited
 *
 * deinit dev freq structure of the Mali GPU device
 */
static void devfreq_exit(struct device *dev)
{
	struct mali_gpu_power *gpu_power;
	struct devfreq_dev_profile *dp;

	if (!dev)
		return;

	dev_dbg(dev, "%s\n", __func__);

	gpu_power = gpu_power_from_dev(dev);
	if (!gpu_power)
		return;

	dp = &gpu_power->devfreq_profile;
	if (dp && dp->freq_table) {
		devm_kfree(dev, dp->freq_table);
		dp->freq_table = NULL;
	}
}

/**
 * devfreq_cur_freq() - gets the device current frequency
 * @dev: GPU Power device from which the frequency is to be read.
 * @freq: clock frequecy
 *
 * Return : 0 always
 */
static int devfreq_cur_freq(struct device *dev, unsigned long *freq)
{
	struct mali_gpu_power *gpu_power;

	if (!dev || !freq)
		return -EINVAL;

	gpu_power = gpu_power_from_dev(dev);
	if (!gpu_power)
		return -ENODEV;

	dev_dbg(gpu_power->dev, "%s %lu\n", __func__, gpu_power->current_nominal_freq);

	mutex_lock(&gpu_power->mutex);
	*freq = gpu_power->current_nominal_freq;
	mutex_unlock(&gpu_power->mutex);

	return 0;
}

/**
 * devfreq_init_freq_table_locked() - Gets the device initial frequency table
 * @gpu_power: Internal GPU power state information
 * @dp: Device frequency profile
 *
 * Updates the frequency profile with the dev init freq table
 *
 * Return: 0 on success else a negative number
 */
static int devfreq_init_freq_table_locked(struct mali_gpu_power *gpu_power,
					  struct devfreq_dev_profile *dp)
{
	int count;
	int i = 0;
	unsigned long freq;
	struct dev_pm_opp *opp;

#if KERNEL_VERSION(4, 11, 0) > LINUX_VERSION_CODE
	rcu_read_lock();
#endif

	count = dev_pm_opp_get_opp_count(gpu_power->dev);
	dev_dbg(gpu_power->dev, "opp count = %d", count);
#if KERNEL_VERSION(4, 11, 0) > LINUX_VERSION_CODE
	rcu_read_unlock();
#endif
	if (count < 0)
		return count;

	dp->freq_table =
		devm_kmalloc_array(gpu_power->dev, count, sizeof(dp->freq_table[0]), GFP_KERNEL);
	if (!dp->freq_table)
		return -ENOMEM;

#if KERNEL_VERSION(4, 11, 0) > LINUX_VERSION_CODE
	rcu_read_lock();
#endif
	for (i = 0, freq = ULONG_MAX; i < count; i++, freq--) {
		opp = dev_pm_opp_find_freq_floor(gpu_power->dev, &freq);
		if (IS_ERR(opp))
			break;

#if KERNEL_VERSION(4, 11, 0) <= LINUX_VERSION_CODE
		dev_pm_opp_put(opp);
#endif

		dp->freq_table[i] = freq;
	}
#if KERNEL_VERSION(4, 11, 0) > LINUX_VERSION_CODE
	rcu_read_unlock();
#endif

	if (count != i) {
		dev_warn(gpu_power->dev, "Unable to enumerate all OPPs %d!=%d\n", count, i);
	}

	dp->max_state = i;

	return 0;
}

/**
 * start_dvfs() - Register the device with devfreq framework
 * @gpu_power: Internal GPU power state information
 *
 * Read device tree and register with the devfreq framework
 * see @kbase_devfreq_init and @power_control_init in the DDK
 * The mutex must be held when this function is called.
 *
 * Return: 0 if success or an error code
 */
static int start_dvfs(struct mali_gpu_power *gpu_power)
{
	int err;
	struct devfreq_dev_profile *dp;

	if (!gpu_power)
		return -EINVAL;

	dev_dbg(gpu_power->dev, "%s\n", __func__);

	if (!gpu_power->clock) {
		dev_err(gpu_power->dev, "Clock not available for devfreq\n");
		err = 0;
		goto cleanup;
	}

	gpu_power->current_freq = clk_get_rate(gpu_power->clock);
	dev_info(gpu_power->dev, "current_freq = %lu\n", gpu_power->current_freq);
	gpu_power->current_nominal_freq = gpu_power->current_freq;

	dp = &gpu_power->devfreq_profile;

	dp->initial_freq = gpu_power->current_nominal_freq;
	dp->polling_ms = 100; /* DEFAULT_PM_DVFS_PERIOD */
	dp->target = devfreq_target;
	dp->get_dev_status = devfreq_status;
	dp->get_cur_freq = devfreq_cur_freq;
	dp->exit = devfreq_exit;

	if (devfreq_init_freq_table_locked(gpu_power, dp)) {
		dev_err(gpu_power->dev, "devfreq_init_freq_table_locked failed\n");
		err = -EFAULT;
		goto cleanup;
	}

	gpu_power->devfreq = devfreq_add_device(gpu_power->dev, dp, "simple_ondemand", NULL);
	if (IS_ERR(gpu_power->devfreq)) {
		err = PTR_ERR(gpu_power->devfreq);
		goto cleanup_freqtable;
	}
	dev_dbg(gpu_power->dev, "Added devfreq device\n");

	err = devfreq_register_opp_notifier(gpu_power->dev, gpu_power->devfreq);
	if (err) {
		dev_err(gpu_power->dev, "Failed to register OPP notifier %d\n", err);
		goto cleanup_device;
	}

	dev_dbg(gpu_power->dev, "init - registered devfreq device as opp notifier\n");
	return 0;

cleanup_device:
	if (devfreq_remove_device(gpu_power->devfreq))
		dev_warn(gpu_power->dev, "Failed to terminate devfreq %d\n", err);
	gpu_power->devfreq = NULL;
cleanup_freqtable:
	dev_set_drvdata(gpu_power->dev, NULL);
	if (dp->freq_table) {
		devm_kfree(gpu_power->dev, dp->freq_table);
		dp->freq_table = NULL;
	}
cleanup:
	return err;
}

/**
 * stop_dvfs() - Unregister the device with devfreq framework
 * @gpu_power: Internal GPU power state information
 *
 * Unregister the device from the devfreq framework and release the resources
 * The mutex must be held when calling this function
 */
static void stop_dvfs(struct mali_gpu_power *gpu_power)
{
	int err;
	struct devfreq_dev_profile *dp;

	if (!gpu_power)
		return;

	dev_dbg(gpu_power->dev, "Term Mali Arbiter devfreq\n");
	if (!gpu_power->clock)
		return;

	devfreq_unregister_opp_notifier(gpu_power->dev, gpu_power->devfreq);
	err = devfreq_remove_device(gpu_power->devfreq);
	if (err)
		dev_err(gpu_power->dev, "Failed to terminate devfreq %d\n", err);

	gpu_power->devfreq = NULL;
	dp = &gpu_power->devfreq_profile;
	if (dp->freq_table) {
		devm_kfree(gpu_power->dev, dp->freq_table);
		dp->freq_table = NULL;
	}
}

/**
 * register_arb_dev() - Register arbiter device to GPU power operations
 * @dev: GPU Power device
 * @priv: arbiter device handle
 * @arbiter_cb_ops: arbiter Callbacks operation struct
 * @arbiter_id: arbiter id assigned by the GPU power
 *
 * Register arbiter device to GPU power operations
 *
 * Return:
 * * 0		- successful.
 * * -EINVAL	- invalid argument.
 * * -ENODEV	- expected interface not available.
 * * -EFAULT	- failed to start DVFS.
 * * -ENOMEM	- out of memory.
 */
static int register_arb_dev(struct device *dev, void *priv,
			    struct mali_gpu_power_arbiter_cb_ops *arbiter_cb_ops, u32 *arbiter_id)
{
	int err = 0;
	struct mali_arb_info *arb_new;
	struct mali_gpu_power *gpu_power;

	if (!dev || !priv || !arbiter_cb_ops || !arbiter_id) {
		pr_err("%s : Invalid argument.\n", __func__);
		return -EINVAL;
	}

	gpu_power = gpu_power_from_dev(dev);
	if (!gpu_power) {
		pr_err("%s : Power interface is missing.\n", __func__);
		return -ENODEV;
	}

	dev_dbg(gpu_power->dev, "%s\n", __func__);

	arb_new = kzalloc(sizeof(struct mali_arb_info), GFP_KERNEL);
	if (!arb_new)
		return -ENOMEM;

	/* assign arbiter id and update the structure*/
	arb_new->arb_val = gpu_power->arb_count;
	arb_new->arbiter = priv;
	memcpy(&arb_new->arbiter_ops, arbiter_cb_ops, sizeof(arb_new->arbiter_ops));
	*arbiter_id = gpu_power->arb_count;

	/*
	 * initialise arbiter list to register multiple arbiters
	 * for the first time when count is zero
	 */
	if (gpu_power->arb_count == 0)
		INIT_LIST_HEAD(&gpu_power->arb_list);
	mutex_lock(&gpu_power->mutex);
	list_add(&arb_new->arb_list, &gpu_power->arb_list);

	gpu_power->arb_count++;

	if (list_is_singular(&gpu_power->arb_list)) {
		mutex_unlock(&gpu_power->mutex);
		err = start_dvfs(gpu_power);
		if (err) {
			dev_warn(gpu_power->dev, "Failed to start dvfs %d\n", err);
			err = -EFAULT;
		}

	} else {
		mutex_unlock(&gpu_power->mutex);
	}
	return err;
}

/**
 * unregister_arb_dev() - Deregister Arbiter device
 * @dev: GPU Power device
 * @arbiter_id: arbiter id assigned by the GPU power
 *
 * Deregister arbiter device from GPU power operations
 */
static void unregister_arb_dev(struct device *dev, u32 arbiter_id)
{
	struct mali_gpu_power *gpu_power;
	struct mali_arb_info *arb_node, *n;

	if (!dev)
		return;

	gpu_power = gpu_power_from_dev(dev);
	if (!gpu_power)
		return;

	dev_dbg(gpu_power->dev, "%s\n", __func__);
	mutex_lock(&gpu_power->mutex);
	list_for_each_entry_safe(arb_node, n, &gpu_power->arb_list, arb_list) {
		if ((u32)arb_node->arb_val == arbiter_id) {
			list_del(&arb_node->arb_list);
			kfree(arb_node);
			break;
		}
	}

	if (list_empty(&gpu_power->arb_list)) {
		mutex_unlock(&gpu_power->mutex);
		stop_dvfs(gpu_power);

	} else {
		mutex_unlock(&gpu_power->mutex);
	}
}

/**
 * mali_gpu_pm_power_on() - Activate the GPU
 * @dev: GPU Power device
 *
 * After this call, the arbiter expect the GPU to be powered.
 *
 */
static void mali_gpu_pm_power_on(struct device *dev)
{
	int err;
	struct mali_gpu_power *gpu_power;

	if (!dev)
		return;

	gpu_power = gpu_power_from_dev(dev);
	if (!gpu_power)
		return;

	if (gpu_power->using_ptm) {
		dev_dbg(gpu_power->dev, "GPU-wide power control is disabled for PTM...\n");
		return;
	}

	dev_dbg(gpu_power->dev, "Enabling GPU power...\n");

	err = pm_runtime_get_sync(gpu_power->dev);
	if (err < 0)
		dev_err(gpu_power->dev, "pm_runtime_get_sync returned %d\n", err);
	else
		dev_dbg(gpu_power->dev, "pm_runtime_get_sync returned %d\n", err);
}

/**
 * mali_gpu_pm_power_off() - Deactivate the GPU
 * @dev: GPU Power device
 *
 * After this call the GPU may not be powered and could be
 * power off.
 * NOTE: this function does not explicitly needs to power down the
 * GPU which can therefore be left ON according implementation needs
 */
static void mali_gpu_pm_power_off(struct device *dev)
{
	int err;
	struct mali_gpu_power *gpu_power;

	if (!dev)
		return;

	gpu_power = gpu_power_from_dev(dev);
	if (!gpu_power)
		return;

	if (gpu_power->using_ptm) {
		dev_dbg(gpu_power->dev, "GPU-wide power control is disabled for PTM...\n");
		return;
	}

	dev_dbg(gpu_power->dev, "Disabling GPU power...\n");

	pm_runtime_mark_last_busy(gpu_power->dev);
	err = pm_runtime_put_autosuspend(gpu_power->dev);
	if (err < 0)
		dev_err(gpu_power->dev, "pm_runtime_put_autosuspend returned %d\n", err);
	else
		dev_dbg(gpu_power->dev, "pm_runtime_put_autosuspend returned %d\n", err);
}

/**
 * mali_gpu_power_off() - Power off the GPU
 * @dev: GPU Power device
 *
 * Power off the GPU right away using clocks and regulators (if present).
 * This function is used by pm_runtime framework to suspend GPU.
 *
 * Return: 0 if successful, error code otherwise.
 */
static int mali_gpu_power_off(struct device *dev)
{
	struct mali_gpu_power *gpu_power;
	int err = 0;

	if (!dev)
		return -EINVAL;

	gpu_power = gpu_power_from_dev(dev);
	if (!gpu_power)
		return -EINVAL;

	if (gpu_power->clock)
		clk_disable(gpu_power->clock);

#if IS_ENABLED(CONFIG_REGULATOR)
	if (gpu_power->regulator) {
		if (regulator_is_enabled(gpu_power->regulator) > 0) {
			err = regulator_disable(gpu_power->regulator);
			if (err)
				dev_err(dev, "Couldn't power off regulator");
		}
	}
#endif /* IS_ENABLED(CONFIG_REGULATOR) */

	return 0;
}

/**
 * mali_gpu_power_on() - Power on the GPU
 * @dev: GPU Power device
 *
 * Power on the GPU right away using clocks and regulators (if present).
 * This function is used by pm_runtime framework to suspend GPU.
 *
 * Return: 0 if successful, error code otherwise
 */
static int mali_gpu_power_on(struct device *dev)
{
	struct mali_gpu_power *gpu_power;
	int err = 0;

	if (!dev)
		return -EINVAL;

	gpu_power = gpu_power_from_dev(dev);
	if (!gpu_power)
		return -EINVAL;

#if IS_ENABLED(CONFIG_REGULATOR)
	if (gpu_power->regulator) {
		if (regulator_is_enabled(gpu_power->regulator) <= 0) {
			err = regulator_enable(gpu_power->regulator);
			if (err)
				dev_err(dev, "Couldn't power on regulator\n");
		}
	}
#endif /* IS_ENABLED(CONFIG_REGULATOR) */

	if (gpu_power->clock) {
		err = clk_enable(gpu_power->clock);
		if (err < 0)
			dev_err(gpu_power->dev, "clk_enable returned %d\n", err);
		else
			dev_dbg(gpu_power->dev, "clk_enable returned %d\n", err);
	}
	return err;
}

/**
 * set_module_mode() - Setup the mode which the module should use
 * @gpu_power: Internal GPU power state information
 *
 * This function set the using_ptm flag according the device tree node which
 * matched with the module.
 *
 * Return: 0 on success, or error code
 */
static int set_module_mode(struct mali_gpu_power *gpu_power)
{
	const char *dt_node_compatible = NULL;

	if (WARN_ON(!gpu_power) || WARN_ON(!gpu_power->dev))
		return -EINVAL;

	if (of_property_read_string(gpu_power->dev->of_node, "compatible", &dt_node_compatible)) {
		dev_err(gpu_power->dev, "Cannot find compatible string\n");
		return -ENODEV;
	}

	if (!strncmp(dt_node_compatible, MALI_GPU_POWER_DT_NAME, strlen(MALI_GPU_POWER_DT_NAME)))
		gpu_power->using_ptm = false;
	else
		gpu_power->using_ptm = true;

	dev_info(gpu_power->dev, "Initializing module mode = %s",
		 gpu_power->using_ptm ? "PTM" : "NORMAL");

	return 0;
}

/**
 * parse_dt() - Parse the device tree node associated to the device
 * @gpu_power: Internal GPU power state information
 *
 * This function reads the device tree to exctract and configure the module
 * according its properties
 *
 * Return: 0 on success, or error code
 */
static int parse_dt(struct mali_gpu_power *gpu_power)
{
	int err;

	if (WARN_ON(!gpu_power) || WARN_ON(!gpu_power->dev))
		return -EINVAL;

	err = set_module_mode(gpu_power);
	if (err) {
		dev_err(gpu_power->dev, "Failed to set module mode\n");
		goto fail;
	}

#if IS_ENABLED(CONFIG_OF)
#if IS_ENABLED(CONFIG_REGULATOR)
	/* setup regulators */
	gpu_power->regulator = regulator_get_optional(gpu_power->dev, "vdd_mali");
	if (IS_ERR(gpu_power->regulator)) {
		err = PTR_ERR(gpu_power->regulator);
		gpu_power->regulator = NULL;
		if (err == -EPROBE_DEFER) {
			dev_err(gpu_power->dev, "Failed to get regulator\n");
			goto fail_regulator;
		}
		dev_info(gpu_power->dev, "Continuing without Mali regulator control\n");
	} else
		dev_info(gpu_power->dev, "Module doing Mali regulator control\n");
#endif /* IS_ENABLED(CONFIG_REGULATOR) */

	/* find the GPU clock via Device Tree and enable it */
	gpu_power->clock = of_clk_get(gpu_power->dev->of_node, 0);
	if (IS_ERR(gpu_power->clock)) {
		err = PTR_ERR(gpu_power->clock);
		gpu_power->clock = NULL;
		if (err == -EPROBE_DEFER) {
			dev_err(gpu_power->dev, "Failed to get clock\n");
			goto fail_clock;
		}
		dev_info(gpu_power->dev, "Continuing without Mali clock control\n");
	} else {
		dev_info(gpu_power->dev, "Mali doing clock control\n");
		err = clk_prepare(gpu_power->clock);
		if (err) {
			dev_err(gpu_power->dev, "Failed to prepare clock %d\n", err);
			goto fail_prepare;
		}
	}

	/* Initialize OPP table from Device Tree */
	if (dev_pm_opp_of_add_table(gpu_power->dev))
		dev_warn(gpu_power->dev, "Invalid operating-points in device tree.\n");
	else
		dev_info(gpu_power->dev,
			 "opp table initialised from device tree operating-points\n");

	if (!gpu_power->using_ptm) {
		pm_runtime_set_autosuspend_delay(gpu_power->dev, AUTO_SUSPEND_DELAY);
		pm_runtime_use_autosuspend(gpu_power->dev);

		err = pm_runtime_set_active(gpu_power->dev);
		if (err)
			dev_warn(gpu_power->dev, "error setting pm_runtime to active, err: %d\n",
				 err);
		pm_runtime_enable(gpu_power->dev);
		if (!pm_runtime_enabled(gpu_power->dev))
			dev_warn(gpu_power->dev, "pm_runtime not enabled\n");
	}

#else
	gpu_power->clock = NULL;
	gpu_power->regulator = NULL;
#endif /* IS_ENABLED(CONFIG_OF) */

	return 0;

fail:
#if IS_ENABLED(CONFIG_OF)
	if (gpu_power->clock != NULL)
		clk_unprepare(gpu_power->clock);
fail_prepare:
	if (gpu_power->clock != NULL)
		clk_put(gpu_power->clock);
fail_clock:
#if IS_ENABLED(CONFIG_REGULATOR)
	if (gpu_power->regulator != NULL)
		regulator_put(gpu_power->regulator);
fail_regulator:
#endif
#endif
	return err;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
#ifdef CONFIG_MALI_DEBUG
/**
 * max_util_debugfs_get() - Provides the max utilization the debugfs.
 * @data: Module's private data
 * @val: Max utilization value written into this
 *
 * Return: 0 on success, or error code
 */
static int max_util_debugfs_get(void *data, u64 *val)
{
	struct mali_gpu_power *gpu_power = data;

	if (!gpu_power) {
		pr_err("Can not access GPU power data\n");
		return -EINVAL;
	}

	mutex_lock(&gpu_power->mutex);
	*val = gpu_power->max_utilization;
	mutex_unlock(&gpu_power->mutex);
	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(max_util_debugfs_fops, max_util_debugfs_get, NULL, "%llu\n");

/**
 * create_debugfs() - Create debugfs for Mali GPU power
 * @gpu_power: internal arbiter platform device
 *
 * This function creates debugfs for Mali GPU power reporting
 * Max utilization
 */
static void create_debugfs(struct mali_gpu_power *gpu_power)
{
	struct dentry *max_util_file;

	gpu_power->mali_gpu_power_dfs = debugfs_create_dir("mali_gpu_power", NULL);
	if (IS_ERR_OR_NULL(gpu_power->mali_gpu_power_dfs)) {
		dev_err(gpu_power->dev, "couldn't create debugfs directory\n");
		return;
	}

	max_util_file = debugfs_create_file("max_utilization", 0400, gpu_power->mali_gpu_power_dfs,
					    gpu_power, &max_util_debugfs_fops);
	if (IS_ERR_OR_NULL(max_util_file))
		dev_err(gpu_power->dev, "couldn't create debugfs file\n");
}

/**
 * remove_debugfs() - Remove debugfs for Mali GPU power
 * @gpu_power: internal arbiter platform device
 *
 * This function removes debugfs for Mali GPU power
 */
static void remove_debugfs(struct mali_gpu_power *gpu_power)
{
	debugfs_remove_recursive(gpu_power->mali_gpu_power_dfs);
}
#endif
#endif

#if (KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE)
/**
 * gpu_bus_create_children() - Create devices on DT nodes.
 * @dev: Owning (parent) device
 * @compat: Compatible string for child device(s)
 *
 * Helper function to create children of a node using a compatible flag.
 * Devices created this way must also be destroyed manually.
 * Return: 0 if successful, otherwise standard error code.
 */
static int gpu_bus_create_children(struct device *dev, const char *compat)
{
	int ret = -ENODEV;
	struct device_node *interface = NULL;

	/* Iterate through the interfaces */
	for_each_child_of_node(dev->of_node, interface) {
		struct device_node *child = NULL;
		struct platform_device *ifpdev;
		struct platform_device *cpdev;

		/* Check any compatible nodes at the first level */
		if (of_device_is_compatible(interface, compat)) {
			cpdev = of_platform_device_create(interface, NULL, dev);
			if (cpdev)
				ret = 0;
			continue;
		}

		/* Only interested in interfaces from here on */
		if (!of_device_is_compatible(interface, MALI_GPU_IF_PTM_DT_NAME))
			continue;

		/* Skip if not enabled/available */
		if (!of_device_is_available(interface))
			continue;

		/* Create the interface device if not already */
		ifpdev = of_find_device_by_node(interface);
		if (!ifpdev)
			ifpdev = of_platform_device_create(interface, NULL, dev);
		if (!ifpdev)
			continue;

		/* Iterate through the interface nodes */
		for_each_child_of_node(interface, child) {
			if (!of_device_is_compatible(child, compat))
				continue;

			if (!of_device_is_available(child))
				continue;

			cpdev = of_platform_device_create(child, NULL, &ifpdev->dev);
			if (cpdev)
				ret = 0;
		}
	}

	return ret;
}

/**
 * gpu_bus_destroy_children() - Destroy devices on DT nodes.
 * @dev: Owning (parent) device
 * @compat: Compatible string for child device(s)
 *
 * Helper function to destroy children created manually.
 *
 * Return: 0 if successful, otherwise standard error code.
 */
static int gpu_bus_destroy_children(struct device *dev, const char *compat)
{
	int ret = -ENODEV;
#if IS_ENABLED(CONFIG_OF_ADDRESS)
	struct device_node *interface = NULL;

	/* Iterate through the interfaces */
	for_each_child_of_node(dev->of_node, interface) {
		struct device_node *child = NULL;
		struct platform_device *cpdev = NULL;

		/* Check the top level devices */
		if (of_device_is_compatible(interface, compat)) {
			cpdev = of_find_device_by_node(interface);
			if (cpdev)
				ret = of_platform_device_destroy(&cpdev->dev, NULL);
			continue;
		}

		/* Iterate through the child nodes */
		for_each_child_of_node(interface, child) {
			if (of_device_is_compatible(child, compat)) {
				cpdev = of_find_device_by_node(child);
				if (cpdev)
					ret = of_platform_device_destroy(&cpdev->dev, NULL);
			}
		}
	}
#endif /* CONFIG_OF_ADDRESS */

	return ret;
}

/**
 * create_children_devices() - Create children devices
 * @work: Work struct pointer
 *
 */
static void create_children_devices(struct work_struct *work)
{
	struct task_item *task = NULL;
	unsigned long action;
	struct device *bus_dev = NULL;
	void *data = NULL;
	const char *const *compat = NULL;
	struct device *dev = NULL;
	struct mali_gpu_power *gpu_power = NULL;

	task = container_of(work, struct task_item, work);
	action = task->action;
	bus_dev = task->bus_dev;
	data = task->notifier_data;
	compat = task->compat;

	dev = (struct device *)data;

	gpu_power = gpu_power_from_dev(bus_dev);
	if (!gpu_power)
		goto clean_task;

	if (!gpu_power->using_ptm || gpu_power->child_devices_created)
		goto clean_task;

	/* Keep track of system and assign. When they have both probed,
	 * create all of the other child devices.
	 */
	if (action == BUS_NOTIFY_BOUND_DRIVER) {
		if (strcmp(*compat, MALI_GPU_SYSTEM_PTM_DT_NAME) == 0) {
			get_device(dev);
			gpu_power->system_dev = dev;
		}
		if (strcmp(*compat, MALI_GPU_ASSIGN_PTM_DT_NAME) == 0) {
			get_device(dev);
			gpu_power->assign_dev = dev;
		}

		if (gpu_power->system_dev && gpu_power->assign_dev) {
			gpu_power->child_devices_created = true;

			gpu_bus_create_children(bus_dev, MALI_GPU_PT_CFG_PTM_DT_NAME);
			gpu_bus_create_children(bus_dev, MALI_GPU_PT_CTRL_PTM_DT_NAME);
			gpu_bus_create_children(bus_dev, MALI_GPU_RG_PTM_DT_NAME);
			gpu_bus_create_children(bus_dev, MALI_GPU_AW_PTM_DT_NAME);
			gpu_bus_create_children(bus_dev, MALI_GPU_KBASE_PTM_DT_NAME);
		}
	}

clean_task:
	kfree((void *)task);
}

/**
 * gpu_bus_notifier() - Notifier callback for platform bus
 * @nb: Notifier block
 * @action: Bus action (see device/bus.h)
 * @data: Notifier data (device pointer)
 *
 * Called when a device changes state on the platform bus.
 * The new kernel API for supplier / consumer would be a cleaner way to do
 * this, but is not available in all of our supported kernels.
 *
 * Return: Always 0.
 */
static int gpu_bus_notifier(struct notifier_block *nb, unsigned long action, void *data)
{
	struct device *dev = (struct device *)data;
	struct device *bus_dev = NULL;
	struct mali_gpu_power *gpu_power = NULL;
	int max_parents = MAX_PARENTS;
	const char *const *compat;
	struct task_item *task = NULL;
	int err = 0;

	/* Filter the notifications to only the GPU bus devices. */
	for (compat = gpu_bus_devices; *compat != NULL; ++compat) {
		if (of_device_is_compatible(dev->of_node, *compat) > 0)
			break;
	}
	if (*compat == NULL)
		return 0;

	/* Find mali-gpu-power device in the hierarchy.
	 * Depending on the configuration this could be current, parent,
	 * or grandparent.
	 */
	for (bus_dev = dev; max_parents > 0; max_parents--) {
		if (of_device_is_compatible(bus_dev->of_node, MALI_GPU_POWER_PTM_DT_NAME))
			break;
		bus_dev = bus_dev->parent;
		if (bus_dev == NULL)
			return 0;
	}
	if (max_parents == 0)
		return 0;

	gpu_power = gpu_power_from_dev(bus_dev);
	if (!gpu_power)
		return 0;

	if (!gpu_power->using_ptm || gpu_power->child_devices_created)
		return 0;

	task = kmalloc(sizeof(struct task_item), GFP_ATOMIC | __GFP_NORETRY);
	if (!task)
		return -ENOMEM;

	task->action = action;
	task->bus_dev = bus_dev;
	task->notifier_data = data;
	task->compat = compat;
	INIT_WORK(&task->work, create_children_devices);
	if (!queue_work(gpu_power->task_wq, &task->work)) {
		err = -EBUSY;
		kfree((void *)task);
	}

	return err;
}
#endif /* KERNEL_VERSION */

/**
 * gpu_power_probe() - Called when device is matched in device tree
 * @pdev: platform device to probe
 *
 * Register the device in the system and initialize several power
 * related resources
 *
 * Return: 0 if success or an error code
 */
static int gpu_power_probe(struct platform_device *pdev)
{
	int err;
	struct device *dev = &pdev->dev;
	struct mali_gpu_power *gpu_power;
#if IS_ENABLED(CONFIG_ARCH_TELECHIPS)
	struct device_node *tcc_3d_cfg_np;
	u32 val = 0;
#endif

	gpu_power = devm_kzalloc(dev, sizeof(struct mali_gpu_power), GFP_KERNEL);
	if (!gpu_power)
		return -ENOMEM;

	gpu_power->dev = dev;
	mutex_init(&gpu_power->mutex);

	err = parse_dt(gpu_power);
	if (err)
		goto fail;

#if IS_ENABLED(CONFIG_ARCH_TELECHIPS)
	tcc_3d_cfg_np = of_find_compatible_node(NULL, NULL, 
						TELECHIPS_MALI_GPU_3D_CFG_DT_NAME);
						
	if (tcc_3d_cfg_np == NULL) {
		dev_err(dev, "tcc_3d_cfg node not found \n");
		goto fail;
	} else {
		gpu_power->base_addr = (void __iomem *)of_iomap(tcc_3d_cfg_np, 0);
		if (!gpu_power->base_addr) {
			dev_err(dev, "Failed to get the register address of tcc_3d_cfg\n");
			goto fail;
		}
		val = ioread32(gpu_power->base_addr + GPU_3DENGINE_CLKMASK);
		iowrite32((val &~ GPU_3DENGINE_CLKMASK_STATUSCHECK_MASK),
		 					gpu_power->base_addr + GPU_3DENGINE_CLKMASK);
	}
#endif

	if (of_device_is_compatible(dev->of_node, "simple-bus")) {
		dev_err(dev, "Use of simple-bus instantiation is not supported.\n"
			     "Please update your device tree.\n");
		err = -EINVAL;
		goto fail;
	}

	gpu_power->pwr_data.ops.register_arb = register_arb_dev;
	gpu_power->pwr_data.ops.unregister_arb = unregister_arb_dev;
	gpu_power->pwr_data.ops.gpu_active = mali_gpu_pm_power_on;
	gpu_power->pwr_data.ops.gpu_idle = mali_gpu_pm_power_off;
	platform_set_drvdata(pdev, &gpu_power->pwr_data);
#if IS_ENABLED(CONFIG_DEBUG_FS)
#ifdef CONFIG_MALI_DEBUG
	create_debugfs(gpu_power);
#endif
#endif

	/* mali_gpu_power_on need to be called expicitly because
	 * pm_runtime framework won't call mali_gpu_power_on unless
	 * it's in a suspended state which is not initally so we
	 * need to call it to power on GPU before using it.
	 * pm_runtime is only used in PV and mali_gpu_pm_power_on
	 * will exit early in case of PTM.
	 */
	mali_gpu_pm_power_on(gpu_power->dev);
	mali_gpu_power_on(dev);

#if (KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE)
	gpu_power->task_wq = alloc_ordered_workqueue("children create wq", WQ_HIGHPRI);
	if (!gpu_power->task_wq) {
		err = -ENOMEM;
		goto fail;
	}

	if (gpu_power->using_ptm) {
		bool create_children = true;

		/* Create the system and assign devices immediately, but wait
		 * until they have probed before creating the other devices.
		 */
		err = gpu_bus_create_children(&pdev->dev, MALI_GPU_SYSTEM_PTM_DT_NAME);
		if (err)
			create_children = false;
		err = gpu_bus_create_children(&pdev->dev, MALI_GPU_ASSIGN_PTM_DT_NAME);
		if (err)
			create_children = false;

		if (create_children) {
			dev_info(&pdev->dev, "Probed with children\n");
		} else {
			/* Pretend that we've created the child nodes to
			 * avoid the bus watcher creating them.
			 */
			gpu_power->child_devices_created = true;
			dev_info(&pdev->dev, "Probed without children\n");
		}
	}
#endif /* KERNEL_VERSION */

	return 0;

fail:
#if (KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE)
	if (gpu_power->task_wq) {
		flush_workqueue(gpu_power->task_wq);
		destroy_workqueue(gpu_power->task_wq);
	}
#endif /* KERNEL_VERSION */
#if IS_ENABLED(CONFIG_ARCH_TELECHIPS)
	if (gpu_power->base_addr)
		iounmap(gpu_power->base_addr);
#endif
	devm_kfree(&pdev->dev, gpu_power);
	return err;
}

/**
 * gpu_power_cleanup() - Free up DVFS resources
 * @gpu_power: Internal GPU power state information
 *
 * Called to free up and cleanup all the DVFS resources
 */
static void gpu_power_cleanup(struct mali_gpu_power *gpu_power)
{
	struct devfreq_dev_profile *dp;

	dev_dbg(gpu_power->dev, "%s\n", __func__);

	/* mali_gpu_power_off need to be called to power off
	 * GPU we can't rely on mali_gpu_pm_power_off calling it
	 * because first we will need to wait for AUTO_SUSPEND_DELAY
	 * and second because we call pm_runtime_disable which will
	 * disable pm_runtime and thus stop any call to suspend GPU
	 */
	mali_gpu_power_off(gpu_power->dev);

	mutex_lock(&gpu_power->mutex);
#if IS_ENABLED(CONFIG_OF)
	mali_gpu_pm_power_off(gpu_power->dev);
	pm_runtime_disable(gpu_power->dev);

	dev_pm_opp_of_remove_table(gpu_power->dev);

	dp = &gpu_power->devfreq_profile;
	if (dp->freq_table) {
		devm_kfree(gpu_power->dev, dp->freq_table);
		dp->freq_table = NULL;
	}

	if (gpu_power->clock != NULL) {
		clk_unprepare(gpu_power->clock);
		clk_put(gpu_power->clock);
	}

#if IS_ENABLED(CONFIG_REGULATOR)
	if (gpu_power->regulator != NULL)
		regulator_put(gpu_power->regulator);
#endif
#endif

#if IS_ENABLED(CONFIG_DEBUG_FS)
#ifdef CONFIG_MALI_DEBUG
	remove_debugfs(gpu_power);
#endif
#endif

	mutex_unlock(&gpu_power->mutex);
}

/**
 * gpu_power_remove() - Remove the platform device
 * @pdev: platform device
 *
 * Called when device is removed
 *
 * Return: 0 always
 */
static int gpu_power_remove(struct platform_device *pdev)
{
	struct mali_gpu_power *gpu_power;

	if (!pdev)
		return 0;

	gpu_power = gpu_power_from_dev(&pdev->dev);

	if (gpu_power) {
#if (KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE)
		flush_workqueue(gpu_power->task_wq);
		destroy_workqueue(gpu_power->task_wq);

		if (gpu_power->child_devices_created) {
			/* Remove the manually created devices */

			gpu_bus_destroy_children(&pdev->dev, MALI_GPU_KBASE_PTM_DT_NAME);
			gpu_bus_destroy_children(&pdev->dev, MALI_GPU_AW_PTM_DT_NAME);
			gpu_bus_destroy_children(&pdev->dev, MALI_GPU_RG_PTM_DT_NAME);
			gpu_bus_destroy_children(&pdev->dev, MALI_GPU_PT_CFG_PTM_DT_NAME);
			gpu_bus_destroy_children(&pdev->dev, MALI_GPU_PT_CTRL_PTM_DT_NAME);
		}

		if (gpu_power->using_ptm) {
			/* Decrement the reference counts */
			if (gpu_power->system_dev) {
				put_device(gpu_power->system_dev);
				gpu_power->system_dev = NULL;
			}
			if (gpu_power->assign_dev) {
				put_device(gpu_power->assign_dev);
				gpu_power->assign_dev = NULL;
			}

			gpu_bus_destroy_children(&pdev->dev, MALI_GPU_SYSTEM_PTM_DT_NAME);
			gpu_bus_destroy_children(&pdev->dev, MALI_GPU_ASSIGN_PTM_DT_NAME);
			gpu_bus_destroy_children(&pdev->dev, MALI_GPU_IF_PTM_DT_NAME);
		}
#endif /* KERNEL_VERSION */
		gpu_power_cleanup(gpu_power);
#if IS_ENABLED(CONFIG_ARCH_TELECHIPS)
		if (gpu_power->base_addr)
			iounmap(gpu_power->base_addr);
#endif
		devm_kfree(&pdev->dev, gpu_power);
	}

	platform_set_drvdata(pdev, NULL);

	dev_info(&pdev->dev, "dvfs_integrator_remove done\n");
	return 0;
}

/* Added by Telechips to support Suspend to RAM */
#if IS_ENABLED(CONFIG_ARCH_TELECHIPS)
static int mali_gpu_power_resume(struct device *dev)
{
#if !defined(CONFIG_TCC_MALI_VZ) || \
        (defined(CONFIG_TCC_MALI_VZ) && defined(CONFIG_TCC807X_CA55_SUB))
	struct mali_gpu_power *gpu_power;
	u32 val = 0;

	if (!dev)
		return -ENODEV;

	gpu_power = gpu_power_from_dev(dev);
	if (!gpu_power)
		return -EINVAL;

	iowrite32(GPU_3DENGINE_DEBUG_SYS_ASSIGN_ENABLE_SB, gpu_power->base_addr + GPU_3DENGINE_DEBUG);
	val = ioread32(gpu_power->base_addr + GPU_3DENGINE_SWRESET);
	iowrite32((val &~ GPU_3DENGINE_SWRESET_3D_MASK), gpu_power->base_addr + GPU_3DENGINE_SWRESET);
	iowrite32(GPU_3DENGINE_SWRESET_FULL_MASK, gpu_power->base_addr + GPU_3DENGINE_SWRESET);
#endif
	return 0;
}
#endif

/*
 * struct gpu_power_dt_match - Match the platform device with the Device Tree.
 */
static const struct of_device_id gpu_power_dt_match[] = { { .compatible = MALI_GPU_POWER_DT_NAME },
							  { .compatible =
								    MALI_GPU_POWER_PTM_DT_NAME },
							  { /* sentinel */ } };

static const struct dev_pm_ops gpu_power_pm_ops = {
	.runtime_suspend = mali_gpu_power_off,
	.runtime_resume = mali_gpu_power_on,
	.runtime_idle = NULL,
#if IS_ENABLED(CONFIG_ARCH_TELECHIPS)
	.resume = mali_gpu_power_resume,
#endif
};
/*
 * struct gpu_power_driver - Platform driver data.
 */
static struct platform_driver gpu_power_driver = {
	.probe = gpu_power_probe,
	.remove = gpu_power_remove,
	.driver = {
		.name = MALI_GPU_POWER_DEV_NAME,
		.of_match_table = gpu_power_dt_match,
		.pm = &gpu_power_pm_ops,
	},
};

#if (KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE)
/* Notifier block registered with platform bus. */
static struct notifier_block g_bus_nb = {
	.notifier_call = gpu_bus_notifier
};
#endif /* KERNEL_VERSION */

/**
 * gpu_power_init() - Register platform driver
 *
 * Return: 0 if successful, otherwise error code.
 */
static int __init gpu_power_init(void)
{
#if (KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE)
	int ret;
	/* Register a bus notifier with the platform bus */
	ret = bus_register_notifier(&platform_bus_type, &g_bus_nb);
	if (ret)
		return ret;
#else
	pr_info("Kernels < 4.12 don't support device creation by bus notifier");
#endif /* KERNEL_VERSION */
	return platform_driver_register(&gpu_power_driver);
}
module_init(gpu_power_init);

/**
 * gpu_power_exit() - Unregister platform driver
 *
 * Unregister the platform driver when no longer needed
 */
static void __exit gpu_power_exit(void)
{
#if (KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE)
	(void)bus_unregister_notifier(&platform_bus_type, &g_bus_nb);
#endif /* KERNEL_VERSION */
	platform_driver_unregister(&gpu_power_driver);
}
module_exit(gpu_power_exit);

MODULE_ALIAS("mali-gpu-power-integration-driver");
MODULE_DESCRIPTION("Power driver.");
MODULE_AUTHOR("ARM Ltd.");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
