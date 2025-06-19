// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/kthread.h>
#include <linux/firmware/tcc_ipi.h>
#include <linux/input/mt.h>
#include "tcc_touch_cmd.h"

#define TOUCH_RECEIVE_NAME      ("tcc_touch_receive")
#define TOUCH_RECEIVE_MINOR		(0)

static struct input_dev *virt_tr_dev;

static void touch_event_occur(const struct tcc_ipi_msg *msg)
{
	uint32_t idx;

	for (idx = 0U ; idx < msg->data_len ; idx += 2U) {
		uint32_t code = msg->data_buf[idx];
		int32_t value = 0;
		
		if (msg->data_buf[idx + 1U] < UINT_MAX) {
			value = (int32_t)msg->data_buf[idx + 1U];
		} else if (msg->data_buf[idx + 1U] == UINT_MAX) {
			value = -1;
		}

		switch (code) {
		case (uint32_t)TOUCH_SYNC:
			(void)input_mt_sync_frame(virt_tr_dev);
			(void)input_sync(virt_tr_dev);
			break;
		case (uint32_t)BTN_TOUCH:
			(void)input_report_key(virt_tr_dev, code, value);
			break;
#ifdef CONFIG_TELECHIPS_MULTI_TOUCH_SHARE
		case (uint32_t)ABS_MT_SLOT:
			(void)input_mt_slot(virt_tr_dev, value);
			(void)input_mt_report_slot_state(virt_tr_dev, MT_TOOL_FINGER, true);
		break;
		case (uint32_t)ABS_MT_TRACKING_ID:
			if (value == -1) {
				(void)input_mt_report_slot_state(virt_tr_dev, MT_TOOL_FINGER, false);
			} else {
				int32_t ret = input_mt_get_slot_by_key(virt_tr_dev, value);

				if (ret != -1) {
					(void)input_mt_report_slot_state(virt_tr_dev, MT_TOOL_FINGER, true);
				} else {
					(void)pr_err("[ERR][TR] Can not found available slot! \n");
				}
			}
		break;
		case (uint32_t)ABS_MT_POSITION_X:
		case (uint32_t)ABS_MT_POSITION_Y:
#endif
		case (uint32_t)ABS_X:
		case (uint32_t)ABS_Y:
			(void)input_report_abs(virt_tr_dev, code, value);
			break;
		default:
			(void)pr_info("[INFO][TR] This event does not support \n");
			break;
		}
	}
}

static ssize_t touch_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	(void)attr;

	if ((dev != NULL) && (buf != NULL)) {
		const struct touch_mbox *mdev = dev_get_drvdata(dev);
		count = scnprintf(buf, sizeof(uint32_t), "%d\n", mdev->touch_state);
	}
	return count;
}

static ssize_t touch_state_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = 0;
	(void)attr;

	if ((dev != NULL) && (buf != NULL)) {
		uint32_t data;
		struct touch_mbox *mdev = dev_get_drvdata(dev);
		int32_t error = kstrtouint(buf, 10, &data);

		if (error == 0) {
			mdev->touch_state = data;
			touch_send_change(mdev, mdev->touch_state);
		} else {
			(void)pr_err("[ERR][TR]: state change fail : %s\n", buf);
		}
	}

	if (count <= (ULONG_MAX >> 1)) {
		ret = (ssize_t)count;
	}

	return ret;
}

static DEVICE_ATTR_RW(touch_state);

static int touch_receive_message(struct tcc_ipi_msg *message, struct platform_device *pdev)
{
	int32_t ret = -1;
	if ((pdev != NULL) && (message != NULL)) {
		struct touch_mbox *dev = platform_get_drvdata(pdev);
		const struct tcc_ipi_msg *msg = (struct tcc_ipi_msg *)message;
		uint32_t cmd = (uint32_t)msg->cmd[0];

		switch (cmd) {
		case (uint32_t)TOUCH_EVENT:
			touch_event_occur(msg);
			ret = 0;
		break;
		case (uint32_t)TOUCH_INIT:
			ret = touch_send_ack(dev, (uint32_t)TOUCH_INIT,
					dev->touch_state);
		break;
		case (uint32_t)TOUCH_ACK:
			if (msg->cmd[2] != dev->touch_state) {
				ret = touch_send_change(dev, dev->touch_state);
			}
		break;
		default:
			(void)pr_warn("[WARN][TR]not used command : %u\n", cmd);
		break;
		}
	} else {
		(void)pr_err("[ERR][TR] pdev or message is NULL!! \n");
	}
	return ret;
}

static int32_t tcc_touch_receive_touch_init(int32_t xMax, int32_t yMax)
{
	int32_t ret = 0;

	virt_tr_dev = input_allocate_device();
	if (virt_tr_dev == NULL) {
		ret = -ENODEV;
	} else {
		(void)set_bit(EV_KEY, virt_tr_dev->evbit);
		(void)set_bit(EV_ABS, virt_tr_dev->evbit);
		(void)set_bit(EV_SYN, virt_tr_dev->evbit);

		(void)set_bit(BTN_TOUCH, virt_tr_dev->keybit);

		(void)set_bit(ABS_X, virt_tr_dev->absbit);
		(void)set_bit(ABS_Y, virt_tr_dev->absbit);

		(void)input_set_abs_params(virt_tr_dev, ABS_X, 0, xMax, 0, 0);
		(void)input_set_abs_params(virt_tr_dev, ABS_Y, 0, yMax, 0, 0);
		
#ifdef CONFIG_TELECHIPS_MULTI_TOUCH_SHARE
		(void)set_bit(ABS_MT_SLOT, virt_tr_dev->absbit);
		(void)set_bit(ABS_MT_POSITION_X, virt_tr_dev->absbit);
		(void)set_bit(ABS_MT_POSITION_Y, virt_tr_dev->absbit);
		(void)set_bit(ABS_MT_TRACKING_ID, virt_tr_dev->absbit);

		(void)input_set_abs_params(virt_tr_dev, ABS_MT_SLOT, 0, MAX_SLOT, 0, 0);
		(void)input_set_abs_params(virt_tr_dev, ABS_MT_POSITION_X, 0, xMax, 0, 0);
		(void)input_set_abs_params(virt_tr_dev, ABS_MT_POSITION_Y, 0, yMax, 0, 0);

		(void)input_mt_init_slots(virt_tr_dev, MAX_SLOT, 0);
#endif

		virt_tr_dev->name = TOUCH_RECEIVE_NAME;
		virt_tr_dev->phys = "dev/input/virt_touch_receive";
		

		ret = input_register_device(virt_tr_dev);
		(void)pr_info("[INFO][TR]Register Input Device %d\n", ret);
	}
	return ret;
}
static int tcc_touch_receive_mbox_init(struct platform_device *pdev)
{
	int32_t ret = 0;
	struct tcc_ipi_register_dat mbox_prot_dat = {0,};
	struct touch_mbox *dev = platform_get_drvdata(pdev);

	if (dev == NULL) {
		ret = -ENOMEM;
	} else {
		
		const char *prot_id = "TCHSHR";
		(void)memcpy(&(mbox_prot_dat.id[0]), prot_id, TCC_IPI_PROTOCOL_ID_MAX_LEN );
		mbox_prot_dat.rx_callback = &touch_receive_message;
		mbox_prot_dat.pdev = pdev;

		dev->ts_mbox_client = of_parse_phandle(pdev->dev.of_node, "tcc_ipi", 0);
		dev->index = tcc_ipi_register(dev->ts_mbox_client, &mbox_prot_dat);
		
		if (dev->index < 0 ) {
			ret = -EPROBE_DEFER;
			(void)pr_err("[ERR][TR] tcc_ipi_register error: %d\n", ret);
		} else {
			pr_debug("[DEBUG][TR] tcc_ipi_register id done\n");
		}
	}
	return ret;
}

static int32_t tcc_touch_receive_probe(struct platform_device *pdev)
{
	int32_t ret = 0;
	int32_t xMax;
	int32_t yMax;

	if (pdev != NULL) {
		struct touch_mbox *tr_dev;

		tr_dev = devm_kzalloc(&pdev->dev, sizeof(struct touch_mbox), GFP_KERNEL);

		if (tr_dev == NULL) {
			ret = -ENOMEM;
			(void)pr_err("[ERR][TR]Device is NULL : %d\n", ret);
		} else {
			(void)platform_set_drvdata(pdev, tr_dev);
			(void)tcc_touch_receive_mbox_init(pdev);
			(void)of_property_read_s32(pdev->dev.of_node, "xmax", &xMax);
			(void)of_property_read_s32(pdev->dev.of_node, "ymax", &yMax);
			ret = tcc_touch_receive_touch_init(xMax, yMax);
			ret = device_create_file(&pdev->dev, &dev_attr_touch_state);
			mutex_init(&tr_dev->lock);
			touch_send_init(tr_dev, tr_dev->touch_state);
		}
	}
	return ret;
}
static int32_t tcc_touch_receive_remove(struct platform_device *pdev)
{
	int32_t ret = -1;
	if (pdev != NULL) {
		(void)device_remove_file(&pdev->dev, &dev_attr_touch_state);
		(void)input_unregister_device(virt_tr_dev);
		(void)pr_info("[INFO][TR]Remove TCC_TR Device\n");
		ret = 0;
	}
	return ret;
}

static const struct of_device_id ttr_of_match[] = {
	{.compatible = "telechips,tcc_touch_receive",},
	{ },
};

static struct platform_driver tcc_touch_receive = {
	.probe	= tcc_touch_receive_probe,
	.remove	= tcc_touch_receive_remove,
	.driver	= {
		.name	= TOUCH_RECEIVE_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = ttr_of_match,
	},
};

static int32_t __init tcc_touch_receive_init(void)
{
	return platform_driver_register(&tcc_touch_receive);
}

static void __exit tcc_touch_receive_exit(void)
{
	platform_driver_unregister(&tcc_touch_receive);
}

module_init(tcc_touch_receive_init);
module_exit(tcc_touch_receive_exit);
MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Input driver receive");
MODULE_LICENSE("GPL");
