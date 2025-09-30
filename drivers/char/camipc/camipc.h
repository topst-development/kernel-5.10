/* SPDX-License-Identifier: GPL-2.0-or-later */
/*******************************************************************************
 *
 * Copyright (C) 2023 Telechips Inc.
 *
 ******************************************************************************/

#ifndef CAMIPC_H
#define CAMIPC_H

#include <linux/platform_device.h>

#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/mailbox_client.h>

#include <video/telechips/vioc_wmix.h>
#include <video/telechips/vioc_config.h>

#define CAMIPC_SEND (0x1)
#define CAMIPC_ACK (0x10)
#define CAMIPC_STATUS (0x11)

/* control commands */
enum { CAMIPC_CMD_NULL = 0,
       CAMIPC_CMD_OVP,
       CAMIPC_CMD_POS,
       CAMIPC_CMD_RESET,
       CAMIPC_CMD_READY,
       CAMIPC_CMD_STATUS,
       CAMIPC_CMD_MAX,
};

/* driver status */
enum { CAMIPC_STS_NULL = 0,
       CAMIPC_STS_INIT,
       CAMIPC_STS_READY,
       CAMIPC_MAX_STS,
};

enum { CAMIPC_PROBE_INIT_OBJ = 0,
       CAMIPC_PROBE_PARSE_DT,
       CAMIPC_PROBE_INIT_CDEV,
       CAMIPC_PROBE_INIT_CLASS,
       CAMIPC_PROBE_INIT_DEVICE,
       CAMIPC_PROBE_INIT_TX_RX,
       CAMIPC_PROBE_MAX };

#define CAMIPC_MAGIC ('I')
#define IOCTL_CAMIPC_SET_OVP (_IO(CAMIPC_MAGIC, 1))
#define IOCTL_CAMIPC_SET_POS (_IO(CAMIPC_MAGIC, 2))
#define IOCTL_CAMIPC_SET_RESET (_IO(CAMIPC_MAGIC, 3))
#define IOCTL_CAMIPC_SET_READY (_IO(CAMIPC_MAGIC, 4))

#define MODUE_NAME "CAMIPC"
#define CAMIPC_DEV_MINOR (0)
#define LOG_TAG MODUE_NAME

#define loge(fmt, ...)                                                         \
	{                                                                      \
		(void)pr_err("[ERROR][%s] %s - " fmt, LOG_TAG, __func__,       \
			     ##__VA_ARGS__);                                   \
	}
#define logw(fmt, ...)                                                         \
	{                                                                      \
		(void)pr_warn("[WARN][%s] %s - " fmt, LOG_TAG, __func__,       \
			      ##__VA_ARGS__);                                  \
	}
#define logd(fmt, ...)                                                         \
	{                                                                      \
		(void)pr_debug("[DEBUG][%s] %s - " fmt, LOG_TAG, __func__,     \
			       ##__VA_ARGS__);                                 \
	}
#define logi(fmt, ...)                                                         \
	{                                                                      \
		(void)pr_info("[INFO][%s] %s - " fmt, LOG_TAG, __func__,       \
			      ##__VA_ARGS__);                                  \
	}

struct camipc_tx {
	struct mutex lock;
	atomic_t seq;
};

struct camipc_rx {
	struct mutex lock;
	atomic_t seq;
};

struct camipc_ovp_data {
	uint32_t cmd;
	uint32_t wmix_ch;
	uint32_t ovp;
	uint32_t data_len;
};

#endif //CAMIPC_H