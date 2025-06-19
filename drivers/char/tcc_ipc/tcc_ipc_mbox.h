/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_IPC_MBOX_H
#define TCC_IPC_MBOX_H

typedef int (*ipc_mbox_receive)(struct tcc_ipi_msg *msg, struct platform_device *pdev);

IPC_INT32 ipc_mailbox_send(
			struct ipc_device *ipc_dev,
			struct tcc_ipc_data *ipc_msg);
int mbox_register(struct platform_device *pdev,
					ipc_mbox_receive handler);
#endif
