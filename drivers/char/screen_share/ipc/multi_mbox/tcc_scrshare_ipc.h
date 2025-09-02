// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_SCREEN_SHARED_MULTI_MBOX_H
#define TCC_SCREEN_SHARED_MULTI_MBOX_H

#include <linux/mailbox/tcc_multi_mbox.h>
#include <linux/mailbox_client.h>

typedef struct tcc_mbox_data scrshare_msg_data_t;
typedef struct tcc_scrshare_ipc_info {
	const char *mbox_name;
	struct mbox_chan *mbox_ch;
	struct mbox_client cl;
}scrshare_ipc_t;

#endif
/* end of file */
