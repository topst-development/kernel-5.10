// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_SCREEN_SHARED_MBOX_CLIENT_H
#define TCC_SCREEN_SHARED_MBOX_CLIENT_H

#include <linux/mailbox/mailbox-tcc-client.h>
#include <linux/mailbox_client.h>

#define USE_TCC_IPC_SYSTEM
#define IPC_NODE_NAME "tcc_mbox_client"
#define tcc_ipc_send_data(a,b,c) tcc_mbox_client_send_data(a,b,c)
#define tcc_ipc_register(a,b) tcc_mbox_client_register(a,b)
#define TCC_IPC_PROTOCOL_ID_MAX_LEN MBOX_CLIENT_PROTOCOL_ID_MAX_LEN

typedef struct tcc_mbox_client_msg tcc_ipc_msg_t;
typedef struct tcc_mbox_register_dat tcc_ipc_register_data_t;

typedef struct tcc_scrshare_ipc_data {
	u32 cmd_len;
	u32 *cmd;
	u32 data_len;
	u32 *data;
	u32 flags;
}scrshare_msg_data_t;
typedef struct tcc_scrshare_ipc_info {
	struct device_node *np;
	int idx;
}scrshare_ipc_t;

#endif
/* end of file*/
