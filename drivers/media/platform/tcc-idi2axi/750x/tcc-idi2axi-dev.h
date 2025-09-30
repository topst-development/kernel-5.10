/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_IDI2AXI_DEV_H
#define TCC_IDI2AXI_DEV_H

#include <linux/interrupt.h>
#include "../tcc-idi2axi.h"

void tcc_idi2axi_dev_try_fmt(const struct tcc_idi2axi_state *state,
			     struct v4l2_format *f);
int tcc_idi2axi_dev_parse_dt(struct tcc_idi2axi_state *state);
irqreturn_t tcc_idi2axi_dev_isr(int irq, void *client_data);
void tcc_idi2axi_dev_disable(struct tcc_idi2axi_state *state);
int tcc_idi2axi_dev_streamon(struct tcc_idi2axi_state *state);
void tcc_idi2axi_dev_streamoff(struct tcc_idi2axi_state *state);

#endif
