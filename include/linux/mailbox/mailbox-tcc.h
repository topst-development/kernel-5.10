// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef MAILBOX_TCC
#define MAILBOX_TCC

#include <linux/interrupt.h>
#include <linux/mailbox_controller.h>

#define TCC_MBOX_FLAG_SKIP_XFER      (1U << 0U)

struct mbox_soc_ops;

struct tcc_mbox_msg {
	u32 cmd_len;
	u32 *cmd;
	u32 data_len;
	u32 *data_buf;
	u32 flags;
};

struct tcc_mbox_device {
	struct mbox_controller mbox;
	void __iomem *mbox_base;
	s32 rx_irq;
	s32 tx_irq;

	u32 rx_cmd_buf_len;
	u32 rx_data_buf_len;
	struct tcc_mbox_msg rx_msg;

	struct device *dev;
	const struct mbox_soc_ops *soc_ops;
	bool opposite_ready;

	spinlock_t lock;
	struct tasklet_struct finish_tasklet;
};

struct mbox_soc_ops {
	unsigned int version;
	bool (*tcc_mbox_can_trans)(const struct tcc_mbox_device *);
	irqreturn_t (*tcc_mbox_tx_irq_handler)(s32 irq, void * data);
};


struct tcc_mbox_device *mbox_chan_to_tcc_mbox(const struct mbox_chan *chan);

#endif
