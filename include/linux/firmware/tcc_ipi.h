// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_IPI
#define TCC_IPI

#include <linux/platform_device.h>

#define MBOX_MSG_CMD_MAX_LEN	6
#define MBOX_MSG_DAT_MAX_LEN	128

struct tcc_ipi_msg {
	u32 cmd_len;
	u32 *cmd;
	u32 data_len;
	u32 *data_buf;
	u32 flags;
};

#define TCC_IPI_PROTOCOL_ID_MAX_LEN 6

struct tcc_ipi_register_dat {
	char id[TCC_IPI_PROTOCOL_ID_MAX_LEN]; /*max 6 char*/
	int (*rx_callback)(struct tcc_ipi_msg *msg, struct platform_device *pdev);
	struct platform_device *pdev;
};

int tcc_ipi_send_data(struct device_node *np, struct tcc_ipi_msg *data, int index);
int tcc_ipi_register(struct device_node *np, struct tcc_ipi_register_dat *prot);

#endif
