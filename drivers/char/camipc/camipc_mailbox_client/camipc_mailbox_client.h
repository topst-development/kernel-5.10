/* SPDX-License-Identifier: GPL-2.0-or-later */
/*******************************************************************************
 *
 * Copyright (C) 2023 Telechips Inc.
 *
 ******************************************************************************/

#ifndef CAMIPC_MAILBOX_CLIENT_H
#define CAMIPC_MAILBOX_CLIENT_H

#include "../camipc.h"
#include <linux/mailbox/mailbox-tcc-client.h>

struct camipc_device {
	struct platform_device *m_pdev;
	struct device *m_dev;
	struct cdev m_cdev;
	struct class *m_class;
	dev_t m_devt;
	const char *device_name;
	struct device_node *mbox_client;
	int mbox_index;

	atomic_t status;

	struct camipc_tx tx;
	struct camipc_rx rx;
};

typedef struct tcc_mbox_client_msg packet;

int camipc_send_message(struct camipc_device *p_camipc, packet *msg);

int camipc_generate_meessage(struct camipc_device *p_camipc, packet *msg,
			     int ctrl_cmd, int cmd_sts, int *args,
			     int num_args);

int camipc_request_channel(struct camipc_device *p_camipc, const char *name);

int camipc_probe(struct platform_device *p_pdev);

void camipc_parsing_set_ovp(struct camipc_device *p_camipc, packet *msg,
			    unsigned long arg, size_t data_size);

int camipc_ioctl_handle(struct camipc_device *p_camipc, int cmd,
			unsigned long arg);

void camipc_set_ovp(const packet *msg);

void camipc_set_pos(const packet *msg);

void camipc_set_reset(const packet *msg);

extern void camipc_send_and_check_timeout(struct camipc_device *p_camipc,
					  packet *msg);

#endif //CAMIPC_MAILBOX_CLIENT_H