/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * openedges npu driver
 *
 * Copyright (C) 2020 Openedges
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef NPU_DEF_H__
#define NPU_DEF_H__

#define DEVICE_NAME                 "npu"
#define NPU_MAJOR                   42
#define NPU_MAX_MINORS              2
#define NPU_BUSY_WAIT_DEF_TIMEOUT   1000
#define NPU_WAIT_DMA_BUSY_CNT       200

#define SUPPORT_MLX
#define NDOLPHIN_ENV
#endif
