/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_TOUCH_CMD_H
#define TCC_TOUCH_CMD_H

#define TOUCH_SYNC				(100)
#define MAX_SLOT				(10)

enum {
	TOUCH_INIT = 1,
	TOUCH_STATE,
	TOUCH_EVENT,
	TOUCH_ACK,
	TOUCH_COMMAND_MAX
};

struct touch_queue {
	uint32_t		code;
	int32_t			value;
	struct list_head list;
};

struct mbox_list {
	struct tcc_ipi_msg    msg;
	struct list_head        queue;
};

struct mbox_queue {
	struct kthread_worker	kworker;
	struct task_struct		*kworker_task;
	struct kthread_work		pump_messages;
	spinlock_t				queue_lock;
	struct list_head		queue;
};

struct touch_mbox {
	const char			*name;
	const char			*dev;
	struct device_node	*ts_mbox_client;
	struct mbox_queue	touch_mbox_queue;
	struct mutex		lock;
	struct touch_queue	data_queue;
	uint32_t			data_len;
	uint32_t			touch_state;
	int32_t				index;
};

int touch_send_init(struct touch_mbox *touch_dev, uint32_t touch_state);
int touch_send_change(struct touch_mbox *touch_dev, uint32_t touch_state);
int touch_send_data(struct touch_mbox *touch_dev, struct tcc_ipi_msg *msg);
int touch_send_ack(struct touch_mbox *touch_dev, uint32_t cmd,
		uint32_t touch_state);
#endif
