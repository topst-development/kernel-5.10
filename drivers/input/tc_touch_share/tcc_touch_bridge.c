// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/kthread.h>
#include <linux/firmware/tcc_ipi.h>
#include <linux/delay.h>
#include "tcc_touch_cmd.h"

#define TOUCH_BRIDGE_NAME       ("tcc_touch_bridge")
#define MAX_TOUCH				(32)

static struct touch_mbox *ts_dev;
static uint32_t touch_latest_slot = (uint32_t)0;

static bool checkSupportEvent(uint32_t code)
{
	bool ret = false;

	switch (code) {
	case BTN_TOUCH:
	case ABS_X:
	case ABS_Y:
#ifdef CONFIG_TELECHIPS_MULTI_TOUCH_SHARE
	case ABS_MT_SLOT:
	case ABS_MT_POSITION_X:
	case ABS_MT_POSITION_Y:
	case ABS_MT_TRACKING_ID:
#endif
		ret = true;
		break;
	default:
		ret = false;
		break;
	}
	return ret;
}

static void ttb_send_queue(void)
{
	ulong flags;
	struct touch_queue *touch_data = NULL;
	struct touch_queue *touch_data_tmp = NULL;
	struct mbox_list *ttb_list = NULL;
	struct tcc_ipi_msg sendMsg = { 0 };
	uint32_t idx = 0U;

	sendMsg.cmd_len = 6;
	sendMsg.cmd = kzalloc(sizeof(int32_t), GFP_KERNEL);
	 
	sendMsg.data_buf = kzalloc((sizeof(int32_t) * MBOX_MSG_DAT_MAX_LEN), GFP_KERNEL);
	if ((sendMsg.cmd !=  NULL) && (sendMsg.data_buf != NULL)) {
		sendMsg.cmd[0] = (uint32_t)TOUCH_EVENT;
	}
	
	list_for_each_entry_safe(touch_data, touch_data_tmp, &ts_dev->data_queue.list, list) {
		if ((idx + 1U) < (uint32_t)MBOX_MSG_DAT_MAX_LEN) {
			if(sendMsg.data_buf != NULL) {
				sendMsg.data_buf[idx] = touch_data->code;

				if (touch_data->value >= 0) {
					sendMsg.data_buf[idx + 1U] = (uint32_t)touch_data->value;
				}else{
					sendMsg.data_buf[idx + 1U] = UINT_MAX;
				}

				ts_dev->data_len -= 1U;
				idx += 2U;

				list_del(&touch_data->list);
				(void)kfree(touch_data);
			}
		} else {
			break;
		}
	}
	sendMsg.data_len = idx;

	ttb_list = kmalloc(sizeof(struct mbox_list), GFP_ATOMIC);
	
	if (ttb_list != NULL) {
		(void)memset((struct tcc_ipi_msg*)&ttb_list->msg, 0x00, sizeof(struct tcc_ipi_msg));
		(void)memcpy((struct tcc_ipi_msg*)&ttb_list->msg, (struct tcc_ipi_msg*)&sendMsg, sizeof(struct tcc_ipi_msg));

		spin_lock_irqsave(&ts_dev->touch_mbox_queue.queue_lock, flags);
		(void)list_add_tail(&ttb_list->queue,	&ts_dev->touch_mbox_queue.queue);
		(void)spin_unlock_irqrestore(&(ts_dev->touch_mbox_queue.queue_lock), flags);
		(void)kthread_queue_work(&ts_dev->touch_mbox_queue.kworker, &ts_dev->touch_mbox_queue.pump_messages);
	}
}

static void ttb_push_queue(uint32_t code, int32_t value)
{
	struct touch_queue *queue;

	queue = kzalloc(sizeof(struct touch_queue), GFP_KERNEL);

	if (queue != NULL) {
		queue->code = code;
		queue->value = value;

		(void)list_add_tail(&queue->list, &ts_dev->data_queue.list);

		ts_dev->data_len = ts_dev->data_len + 1U;

		if (ts_dev->data_len >= 10U) {
			ttb_send_queue();
		}
	}
}

static void ttb_event(struct input_handle *handle,
		uint32_t type, uint32_t code, int32_t value)
{
	uint32_t state = ts_dev->touch_state;

	if((ts_dev->dev != NULL) && (handle != NULL)){
		if ((state == (uint32_t)1) && (strcmp(ts_dev->dev, dev_name(&handle->dev->dev)) == 0)) {
			if (type == (uint32_t)SYN_REPORT) {
				ttb_push_queue((uint32_t)TOUCH_SYNC, value);
				if (ts_dev->data_len != 0U) {
					ttb_send_queue();
				} 
			} else {
				if (checkSupportEvent(code)) {
#ifdef CONFIG_TELECHIPS_MULTI_TOUCH_SHARE
					if (code == (uint32_t)ABS_MT_SLOT) {
						if (value < 0) {
							(void)pr_err("[ERR][TB] It is not avaliable slot!!\n");
						} else {
							touch_latest_slot = (uint32_t)value;
						}
					}

					if (touch_latest_slot < (uint32_t)MAX_SLOT) {
						ttb_push_queue(code, value);
					}
#else
					ttb_push_queue(code, value);
#endif
				}
			}
		}
	}
}

static int32_t ttb_connect(struct input_handler *handler, struct input_dev *dev,
					 const struct input_device_id *id)
{
	struct input_handle *i_handle;
	int32_t err = 0;
	(void)id;

	i_handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);

	if (i_handle == NULL) {
		err = (int32_t)-ENOMEM;
		(void)pr_err("[ERROR][TB]No Memory");
	} else {
		i_handle->dev = dev;
		i_handle->handler = handler;
		i_handle->name = "tcc_tb";

		err = input_register_handle(i_handle);
		if (err != (int32_t)0) {
			(void)pr_err("[ERROR][TB]Connected device: free_handle");
			(void)kfree(i_handle);
		} else {
			err = input_open_device(i_handle);
			if (err != (int32_t)0) {
				(void)pr_err("[ERROR][TB]Connected device: unregister_handle");
				(void)input_unregister_handle(i_handle);
			} else {
				(void)pr_info("[INFO][TB]Connected device: %s (%s at %s)\n",
					dev_name(&dev->dev), (dev->name != NULL) ? dev->name : "unknown", (dev->phys != NULL) ? dev->phys : "unknown");
				if ((dev->phys != NULL) && (strstr(dev->name, ts_dev->name) != NULL)) {
					ts_dev->dev = dev_name(&dev->dev);
					(void)pr_info("[INFO][TB] Touch Device is register!!\n");
				} else {
					pr_debug("[DEBUG][TB] Touch Device is not register!!\n");
				}
			}
		}
	}

	return err;
}

static void ttb_disconnect(struct input_handle *handle)
{
	if (handle != NULL)	{
		(void)pr_info("[INFO][TB]Disconnected device: %s\n",
				dev_name(&handle->dev->dev));

		(void)input_close_device(handle);
		(void)input_unregister_handle(handle);
		(void)kfree(handle);
	}
}

static const struct input_device_id ttb_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT
			| INPUT_DEVICE_ID_MATCH_ABSBIT,

#ifdef CONFIG_TELECHIPS_MULTI_TOUCH_SHARE
		.evbit = { BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) },
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		.absbit = { BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) 
		| BIT_MASK(ABS_MT_SLOT) | BIT_MASK(ABS_MT_POSITION_X) 
		| BIT_MASK(ABS_MT_POSITION_Y) | BIT_MASK(ABS_MT_TRACKING_ID) },
#else
		.evbit = { BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) },
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		.absbit = { BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) },
#endif
	},
	{ .driver_info = 1 },	/* Matches all devices */
	{ },	/* Terminating zero entry */
};

MODULE_DEVICE_TABLE(input, ttb_ids);

static struct input_handler ttb_handler = {
		.event =		ttb_event,
		.connect =		ttb_connect,
		.disconnect =	ttb_disconnect,
		.name =			"ttb",
		.id_table =		ttb_ids,
};

static int receive_message(struct tcc_ipi_msg *message, struct platform_device *pdev)
{
	int32_t ret = -1;

	if ((message != NULL) && (pdev != NULL))	{
		const struct tcc_ipi_msg *msg = (struct tcc_ipi_msg *)message;
		uint32_t cmd = (uint32_t)msg->cmd[0];

		switch (cmd) {
		case (uint32_t)TOUCH_ACK:
			if (msg->cmd[1] == (uint32_t)TOUCH_INIT) {
				ts_dev->touch_state = msg->cmd[2];
				ret = 0;
			}
		break;
		case (uint32_t)TOUCH_STATE:
			ts_dev->touch_state = msg->cmd[1];
			ret = touch_send_ack(ts_dev,
					(uint32_t)TOUCH_STATE,
					ts_dev->touch_state);
		break;
		case (uint32_t)TOUCH_INIT:
			ts_dev->touch_state = msg->cmd[1];
			ret = touch_send_ack(ts_dev,
					(uint32_t)TOUCH_INIT,
					ts_dev->touch_state);
		break;
		default:
			(void)pr_warn("[WARN][TB] This command is invalid\n");
		break;
		}
	} else {
		(void)pr_err("[ERR][TB] receive_message message or pdev is invalid\n");
	}

	return ret;
}

static void ttb_pump_messages(struct kthread_work *work)
{
	struct mbox_list *ttb_list = NULL;
	struct mbox_list *ttb_list_tmp = NULL;
	ulong flags;

	(void)work;

	if (ts_dev != NULL)	{
		spin_lock_irqsave(&ts_dev->touch_mbox_queue.queue_lock, flags);
		list_for_each_entry_safe(ttb_list, ttb_list_tmp, &ts_dev->touch_mbox_queue.queue, queue) {
			spin_unlock_irqrestore(&ts_dev->touch_mbox_queue.queue_lock, flags);
			(void)touch_send_data(ts_dev, &ttb_list->msg);
			spin_lock_irqsave(&ts_dev->touch_mbox_queue.queue_lock, flags);
			list_del(&ttb_list->queue);
			(void)kfree(ttb_list);
		}
		spin_unlock_irqrestore(&ts_dev->touch_mbox_queue.queue_lock, flags);
	}
}

static int32_t tcc_touch_bridge_mbox_init(struct platform_device *pdev)
{
	int32_t ret = 0;
	struct tcc_ipi_register_dat mbox_prot_dat = {0,};

	if (ts_dev == NULL) {
		ret = -ENOMEM;
	} else {
		const char *prot_id = "TCHSHR";
		(void)memcpy(&(mbox_prot_dat.id[0]), prot_id, TCC_IPI_PROTOCOL_ID_MAX_LEN );
		mbox_prot_dat.rx_callback = &receive_message;
		mbox_prot_dat.pdev = pdev;

		ts_dev->ts_mbox_client = of_parse_phandle(pdev->dev.of_node, "tcc_ipi", 0);
		ts_dev->index = tcc_ipi_register(ts_dev->ts_mbox_client, &mbox_prot_dat);
		
		if (ts_dev->index < 0 ) {
			ret = -EPROBE_DEFER;
		}
	}
	return ret;
}

static int32_t tcc_touch_bridge_queue_init(struct touch_mbox *dev)
{
	int32_t ret = 0;

	(void)INIT_LIST_HEAD(&dev->touch_mbox_queue.queue);
	(void)INIT_LIST_HEAD(&dev->data_queue.list);
	spin_lock_init(&dev->touch_mbox_queue.queue_lock);

	kthread_init_worker(&dev->touch_mbox_queue.kworker);
	dev->touch_mbox_queue.kworker_task = kthread_run(kthread_worker_fn,
								&dev->touch_mbox_queue.kworker, "ttb_thread");
	if (IS_ERR(dev->touch_mbox_queue.kworker_task)) {
		(void)pr_err("[ERR][TB]%s:failed to create msg\n", __func__);
		ret = -ENOMEM;
	} else {
		kthread_init_work(&dev->touch_mbox_queue.pump_messages,
				ttb_pump_messages);
	}
	return ret;
}

static int32_t tcc_touch_bridge_probe(struct platform_device *pdev)
{
	int32_t ret = 0;
	const struct device_node *np;

	if (pdev != NULL) {
		np = (const struct device_node *)pdev->dev.of_node;

		ts_dev = devm_kzalloc(&pdev->dev,
				sizeof(struct touch_mbox), GFP_KERNEL);
		(void)platform_set_drvdata(pdev, ts_dev);

		
		ret = of_property_read_string(np, "share-display", &ts_dev->name);
		
		if (ret != 0) {
			(void)pr_err("[ERROR][TB] failed to get share-display.\n");
		} else {
			pr_debug("[DEBUG][TB] success to get share-display.\n");
		}

		ret = input_register_handler(&ttb_handler);
		ret = tcc_touch_bridge_mbox_init(pdev);
		if (ret != 0) {
			(void)pr_err("[ERROR][TB] failed to mbox initialize.\n");
		} else {
			pr_debug("[DEBUG][TB] success to mbox initialize.\n");
		}


		ts_dev->touch_state = 1;
		ts_dev->data_len = 0;
		ret = tcc_touch_bridge_queue_init(ts_dev);
		mutex_init(&ts_dev->lock);
		touch_send_init(ts_dev, ts_dev->touch_state);
	}
	return ret;
}

static int32_t tcc_touch_bridge_remove(struct platform_device *pdev)
{
	int32_t ret = -1;
	struct touch_mbox *tb_dev = platform_get_drvdata(pdev);

	if (tb_dev != NULL)	{
		(void)input_unregister_handler(&ttb_handler);
		(void)kthread_flush_worker(&tb_dev->touch_mbox_queue.kworker);
		ret = kthread_stop(tb_dev->touch_mbox_queue.kworker_task);
		(void)pr_info("[INFO][TB]Remove TCC_TB Device\n");
	}
	return ret;
}

#if defined(CONFIG_PM)
static int32_t tcc_touch_bridge_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	(void)pdev;
	(void)state;
	touch_latest_slot = 0U;

	return 0;
}
#endif

static struct platform_driver tcc_touch_bridge = {
	.probe	= tcc_touch_bridge_probe,
	.remove	= tcc_touch_bridge_remove,
#if defined(CONFIG_PM)
	.suspend = tcc_touch_bridge_suspend,
#endif
	.driver	= {
		.name	= TOUCH_BRIDGE_NAME,
		.owner	= THIS_MODULE,
	},
};

static int32_t __init tcc_touch_bridge_init(void)
{
	return platform_driver_register(&tcc_touch_bridge);
}

static void __exit tcc_touch_bridge_exit(void)
{
	platform_driver_unregister(&tcc_touch_bridge);
}

module_init(tcc_touch_bridge_init);
module_exit(tcc_touch_bridge_exit);
MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips Input driver bridge");
MODULE_LICENSE("GPL");
