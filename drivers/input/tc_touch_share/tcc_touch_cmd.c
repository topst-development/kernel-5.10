// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/firmware/tcc_ipi.h>
#include "tcc_touch_cmd.h"

int touch_send_init(struct touch_mbox *touch_dev, 
							uint32_t touch_state)
{
	int32_t ret = -1;

	if (touch_dev != NULL) {
		struct tcc_ipi_msg sendMsg = { 0 };
		sendMsg.cmd_len = 2;
		sendMsg.cmd = kzalloc((sizeof(int32_t) * 2UL), GFP_KERNEL);
		if(sendMsg.cmd !=  NULL){
			sendMsg.cmd[0] = (int32_t)TOUCH_INIT;
			sendMsg.cmd[1] = touch_state;

			mutex_lock(&touch_dev->lock);
			ret = tcc_ipi_send_data(touch_dev->ts_mbox_client, &sendMsg, touch_dev->index);
			mutex_unlock(&touch_dev->lock);
		}
	}
	return ret;
}

int touch_send_change(struct touch_mbox *touch_dev, uint32_t touch_state)
{
	int32_t ret = -1;
	if (touch_dev != NULL) {
		struct tcc_ipi_msg sendMsg = { 0 };

		sendMsg.cmd_len = 2;
		sendMsg.data_len = 0;
		sendMsg.cmd = kzalloc((sizeof(int32_t) * 2UL), GFP_KERNEL);
		if(sendMsg.cmd !=  NULL){
			sendMsg.cmd[0] = (int32_t)TOUCH_STATE;
			sendMsg.cmd[1] = touch_state;

			mutex_lock(&touch_dev->lock);
			ret = tcc_ipi_send_data(touch_dev->ts_mbox_client, &sendMsg, touch_dev->index);
			mutex_unlock(&touch_dev->lock);
		}
	}
	return ret;
}

int touch_send_data(struct touch_mbox *touch_dev, struct tcc_ipi_msg *msg)
{
	int32_t ret = -1;
	if (touch_dev != NULL) {
		mutex_lock(&touch_dev->lock);
		ret = tcc_ipi_send_data(touch_dev->ts_mbox_client, msg, touch_dev->index);
		mutex_unlock(&touch_dev->lock);
	}
	return ret;
}

int touch_send_ack(struct touch_mbox *touch_dev, uint32_t cmd,
		uint32_t touch_state)
{

	int32_t ret = -1;
	if (touch_dev != NULL) {
		struct tcc_ipi_msg sendMsg = { 0 };
		sendMsg.cmd_len = 3;
		sendMsg.cmd = kzalloc((sizeof(int32_t) * 3UL), GFP_KERNEL);

		if(sendMsg.cmd !=  NULL){

			sendMsg.cmd[0] = (int32_t)TOUCH_ACK;
			sendMsg.cmd[1] = cmd;
			sendMsg.cmd[2] = touch_state;

			mutex_lock(&touch_dev->lock);
			ret = tcc_ipi_send_data(touch_dev->ts_mbox_client, &sendMsg, touch_dev->index);
			mutex_unlock(&touch_dev->lock);
		}
	}

	return ret;
}
