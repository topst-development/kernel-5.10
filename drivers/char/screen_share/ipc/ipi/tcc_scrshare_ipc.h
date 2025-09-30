// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_SCREEN_SHARED_IPI_H
#define TCC_SCREEN_SHARED_IPI_H

#include <linux/firmware/tcc_ipi.h>

#define USE_TCC_IPC_SYSTEM
#define IPC_NODE_NAME "tcc-ipi"
#define tcc_ipc_send_data(a,b,c) tcc_ipi_send_data(a,b,c)
#define tcc_ipc_register(a,b) tcc_ipi_register(a,b)
#define TCC_IPC_PROTOCOL_ID_MAX_LEN TCC_IPI_PROTOCOL_ID_MAX_LEN

typedef struct tcc_ipi_msg tcc_ipc_msg_t;
typedef struct tcc_ipi_register_dat tcc_ipc_register_data_t;

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
