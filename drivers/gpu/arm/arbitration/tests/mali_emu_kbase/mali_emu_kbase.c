// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note OR MIT
/*
 *
 * (C) COPYRIGHT 2021-2023 ARM Limited. All rights reserved.
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
 * This is an emulated Kbase designed to register with the arbiter and
 * immediately request gpu once it's granted it reports idle, stops then
 * request again.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/of_platform.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/mali_arbiter_interface.h>

#define MALI_REQUIRED_KBASE_ARBITER_INTERFACE_VERSION 5
#if MALI_REQUIRED_KBASE_ARBITER_INTERFACE_VERSION != MALI_ARBITER_INTERFACE_VERSION
#error "Unsupported Mali Arbiter interface version."
#endif

#define KBASE_DT_NAME "arm,mali-midgard"

static int gpu_req_timeout = 100;
module_param(gpu_req_timeout, int, 0644);
MODULE_PARM_DESC(gpu_req_timeout,
		 "if the GPU is not granted within this time(ms) kbase will defer the probe");

static int num_of_emu_kbase_instance = 1;
module_param(num_of_emu_kbase_instance, int, 0644);
MODULE_PARM_DESC(num_of_emu_kbase_instance,
		 "Number of emulated kbase instances (always the first available AW)");

static int request_again_timeout = 8;

/* List of all emulated kbase instances initialized */
static struct list_head emu_kbase_list;
static LIST_HEAD(emu_kbase_list);
static DEFINE_MUTEX(emu_kbase_list_mutex);

/**
 * struct mali_emu_kbase - Internal emulated kbase data
 * @dev: kbase device.
 * @pdev: arbif platform device.
 * @arb_if: arbier interace
 * @vm_state_wait: Wait queue set when GPU is granted
 * @gpu_granted: flag to show if gpu have been granted or not.
 * @wq: Workqueue for scheduling requesting gpu.
 * @request_work: work item for scheduling requesting gpu.
 * @entry: entry in the kernel list.
 */
struct mali_emu_kbase {
	struct device *dev;
	struct platform_device *pdev;
	struct arbiter_if_dev *arb_if;
	wait_queue_head_t vm_state_wait;
	bool gpu_granted;
	struct workqueue_struct *wq;
	struct delayed_work request_work;
	struct list_head entry;
};

/**
 * get_request_again_timeout() - returns request_again_timeout
 * @buffer: buffer to write the state to
 * @kp: pointer to the kernel_param
 *
 * Module param op for request_again_timeout
 *
 * Return: length written if success, or an error code
 */
static int get_request_again_timeout(char *buffer, const struct kernel_param *kp)
{
	return scnprintf(buffer, PAGE_SIZE, "%d\n", request_again_timeout);
}

/**
 * set_request_again_timeout() - sets request_again_timeout
 * @val: buffer to read the state from
 * @kp: pointer to the kernel_param
 *
 * Module param op for request_again_timeout
 *
 * Return: 0 if success, or a Linux error code
 */
static int set_request_again_timeout(const char *val, const struct kernel_param *kp)
{
	int timeout = 0;
	struct mali_emu_kbase *kbase;

	if (kstrtoint(val, 10, &timeout))
		return -EINVAL;

	mutex_lock(&emu_kbase_list_mutex);

	if (list_empty(&emu_kbase_list) || request_again_timeout == timeout) {
		request_again_timeout = timeout;
		goto exit;
	}

	request_again_timeout = timeout;
	list_for_each_entry(kbase, &emu_kbase_list, entry) {
		cancel_delayed_work_sync(&kbase->request_work);
		if (timeout > 0)
			queue_delayed_work(kbase->wq, &kbase->request_work, 0);
	}

exit:
	mutex_unlock(&emu_kbase_list_mutex);
	return 0;
}
static struct kernel_param_ops request_again_timeout_ops = {
	.set = set_request_again_timeout,
	.get = get_request_again_timeout,
};

module_param_cb(request_again_timeout, &request_again_timeout_ops, NULL, 0644);
MODULE_PARM_DESC(
	request_again_timeout,
	"Time in ms to request again the GPU after stopping. '0' disables requesting again");

/**
 * on_gpu_stop() - sends stopped message and schedule requesting gpu again
 * @dev: arbiter interface device handle
 *
 * call back function to signal a GPU STOP event from arbiter interface
 */
static void on_gpu_stop(struct device *dev)
{
	struct mali_emu_kbase *kbase;

	if (!dev)
		return;
	kbase = dev_get_drvdata(dev);
	if (!kbase)
		return;

	kbase->arb_if->vm_ops.vm_arb_gpu_stopped(kbase->arb_if, 0);
	if (request_again_timeout)
		queue_delayed_work(kbase->wq, &kbase->request_work,
				   msecs_to_jiffies(request_again_timeout));
}

/**
 * on_gpu_granted() - sends idle message and sets gpu_granted flag
 * @dev: arbiter interface device handle
 *
 * call back function to signal a GPU GRANT event from arbiter interface
 */
static void on_gpu_granted(struct device *dev)
{
	struct mali_emu_kbase *kbase;

	if (!dev)
		return;
	kbase = dev_get_drvdata(dev);
	if (!kbase)
		return;

	kbase->gpu_granted = true;
	wake_up(&kbase->vm_state_wait);
	kbase->arb_if->vm_ops.vm_arb_gpu_idle(kbase->arb_if);
}

/**
 * on_gpu_lost() - call back function to signal a GPU LOST
 * @dev: arbiter interface device handle
 *
 * call back function to signal a GPU LOST event from arbiter interface
 */
static void on_gpu_lost(struct device *dev)
{
	struct mali_emu_kbase *kbase;

	dev_info(dev, "gpu_lost\n");

	if (!dev)
		return;
	kbase = dev_get_drvdata(dev);
	if (!kbase)
		return;

	kbase->arb_if->vm_ops.vm_arb_gpu_request(kbase->arb_if);
}

/**
 * on_update_freq() - Handler for when Arbif receives
 * new GPU clock frequency
 * @dev: Device registers with the callbacks
 * @freq: GPU clock frequency value reported from arbiter
 */
static void on_update_freq(struct device *dev, uint32_t freq)
{
}

/**
 * on_max_config() - Handler for when Arbif receives a callback on
 *                  max_config
 * @dev:           Device registers with the callbacks
 * @max_l2_slices: Maximum L2 slice count
 * @max_core_mask: Maximum core mask
 */
static void on_max_config(struct device *dev, uint32_t max_l2_slices, uint32_t max_core_mask)
{
}

/**
 * gpu_request() - requests gpu when timer expires
 * @work: work struct data to obtain emu_kbase data
 */
static void gpu_request(struct work_struct *work)
{
	struct mali_emu_kbase *kbase;
	struct delayed_work *data;

	if (WARN_ON(!work))
		return;

	data = container_of(work, struct delayed_work, work);

	kbase = container_of(data, struct mali_emu_kbase, request_work);

	kbase->arb_if->vm_ops.vm_arb_gpu_request(kbase->arb_if);
}

/**
 * kbase_probe() - Initialize the arbif device
 * @pdev: The platform device
 *
 * Called when device is matched in device tree, allocate the resources
 * for the device.
 *
 * Return: 0 if success, or a Linux error code
 */
static int kbase_probe(struct platform_device *pdev)
{
	struct mali_emu_kbase *kbase;
	struct arbiter_if_arb_vm_ops ops;
	struct device_node *arbiter_if_node;
	int err;

	kbase = devm_kzalloc(&pdev->dev, sizeof(struct mali_emu_kbase), GFP_KERNEL);
	if (!kbase)
		return -ENOMEM;

	kbase->dev = &pdev->dev;

	arbiter_if_node = of_parse_phandle(pdev->dev.of_node, "arbiter_if", 0);
	if (!arbiter_if_node) {
		dev_dbg(&pdev->dev, "No arbiter_if in Device Tree\n");
		return 0;
	}

	kbase->pdev = of_find_device_by_node(arbiter_if_node);
	if (!kbase->pdev) {
		dev_err(&pdev->dev, "Failed to find arbiter_if device\n");
		return -EPROBE_DEFER;
	}

	if (!kbase->pdev->dev.driver || !try_module_get(kbase->pdev->dev.driver->owner)) {
		dev_err(&pdev->dev, "arbiter_if driver not available\n");
		return -EPROBE_DEFER;
	}

	kbase->arb_if = platform_get_drvdata(kbase->pdev);
	if (!kbase->arb_if) {
		dev_err(&pdev->dev, "arbiter_if driver not ready\n");
		err = -EPROBE_DEFER;
		goto clean_up;
	}

	init_waitqueue_head(&kbase->vm_state_wait);

	kbase->wq = alloc_workqueue("mali_emu_kbase", WQ_UNBOUND, 0);
	INIT_DELAYED_WORK(&kbase->request_work, gpu_request);

	ops.arb_vm_gpu_stop = on_gpu_stop;
	ops.arb_vm_gpu_granted = on_gpu_granted;
	ops.arb_vm_gpu_lost = on_gpu_lost;
	ops.arb_vm_max_config = on_max_config;
	ops.arb_vm_update_freq = on_update_freq;

	if (kbase->arb_if->vm_ops.vm_arb_register_dev) {
		err = kbase->arb_if->vm_ops.vm_arb_register_dev(kbase->arb_if, &pdev->dev, &ops);
		if (err) {
			dev_err(&pdev->dev, "Failed to register with arbiter\n");
			goto clean_up;
		}
	}

	platform_set_drvdata(pdev, kbase);

	if (num_of_emu_kbase_instance) {
		kbase->arb_if->vm_ops.vm_arb_gpu_request(kbase->arb_if);
		err = wait_event_timeout(kbase->vm_state_wait, kbase->gpu_granted,
					 msecs_to_jiffies(gpu_req_timeout));

		if (!err) {
			dev_err(&pdev->dev, "Probe deferred!\n");
			err = -EPROBE_DEFER;
			goto clean_up;
		}

		num_of_emu_kbase_instance--;

	} else {
		dev_dbg(&pdev->dev, "Expected number of emu_kbase instances already reached!\n");
		err = -EINVAL;
		goto clean_up;
	}
	dev_info(&pdev->dev, "Probe successful\n");
	INIT_LIST_HEAD(&kbase->entry);
	mutex_lock(&emu_kbase_list_mutex);
	list_add_tail(&kbase->entry, &emu_kbase_list);
	mutex_unlock(&emu_kbase_list_mutex);
	return 0;

clean_up:
	module_put(kbase->pdev->dev.driver->owner);
	return err;
}

/**
 * kbase_remove() - Remove the arbif device
 * @pdev: The platform device
 *
 * Called when the device is being unloaded to do any cleanup
 *
 * Return: Always returns 0
 */
static int kbase_remove(struct platform_device *pdev)
{
	struct mali_emu_kbase *kbase;

	kbase = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&kbase->request_work);
	destroy_workqueue(kbase->wq);

	mutex_lock(&emu_kbase_list_mutex);
	list_del(&kbase->entry);
	mutex_unlock(&emu_kbase_list_mutex);

	kbase->arb_if->vm_ops.vm_arb_unregister_dev(kbase->arb_if);
	module_put(kbase->pdev->dev.driver->owner);

	devm_kfree(&pdev->dev, kbase);
	return 0;
}

/*
 * kbase_dt_match: Match the platform device with the Device Tree.
 */
static const struct of_device_id kbase_dt_match[] = { { .compatible = KBASE_DT_NAME }, {} };

/*
 * arbif_driver: Platform driver data.
 */
static struct platform_driver kbase_driver = {
	.probe = kbase_probe,
	.remove = kbase_remove,
	.driver = {
		.name = "emu_kbase",
		.of_match_table = kbase_dt_match,
	},
};

/**
 * mali_emu_kbase_init() - Register platform driver
 *
 * Return: See definition of platform_driver_register()
 */
static int __init mali_emu_kbase_init(void)
{
	return platform_driver_register(&kbase_driver);
}
module_init(mali_emu_kbase_init);

/**
 * mali_emu_kbase_exit() - Unregister platform driver
 */
static void __exit mali_emu_kbase_exit(void)
{
	platform_driver_unregister(&kbase_driver);
}
module_exit(mali_emu_kbase_exit);

MODULE_VERSION("1.0");
MODULE_DESCRIPTION("Emulated Kbase designed to force arbitration");
MODULE_AUTHOR("ARM Ltd.");
MODULE_LICENSE("GPL");
MODULE_ALIAS("mali-kbase");
