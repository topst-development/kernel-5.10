/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
 * Copyright (C) 2017 Linaro Ltd.
 */
#ifndef TCCVDEC_CTRL_H
#define TCCVDEC_CTRL_H

struct tcc_vdec_ctx;

int tccvdec_ctrl_init(struct tcc_vdec_ctx *inst);
void tccvdec_ctrl_deinit(struct tcc_vdec_ctx *inst);

#endif
