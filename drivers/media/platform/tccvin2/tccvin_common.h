/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCCVIN_DEVICE_H
#define TCCVIN_DEVICE_H

#ifdef ON
#undef ON
#endif

#ifdef OFF
#undef OFF
#endif

#define ON (1)
#define OFF (0)

enum reserved_memory {
	RESERVED_MEM_PGL,
	RESERVED_MEM_VIQE,
	RESERVED_MEM_PREV,
	RESERVED_MEM_LFRAME,
	RESERVED_MEM_MAX,
};

enum tccvin_buffer_state {
	TCCVIN_BUF_STATE_IDLE,
	TCCVIN_BUF_STATE_QUEUED,
	TCCVIN_BUF_STATE_ACTIVE,
	TCCVIN_BUF_STATE_READY,
	TCCVIN_BUF_STATE_DONE,
	TCCVIN_BUF_STATE_ERROR,
};

enum tccvin_queue_state {
	TCCVIN_QUEUE_DISCONNECTED = 1, /* 1 << 0 */
	TCCVIN_QUEUE_DROP_CORRUPTED = 2, /* 1 << 1 */
};

enum tccvin_test {
	TCCVIN_TEST_STOP_SUBDEV,
	TCCVIN_TEST_STOP_WDMA,
	TCCVIN_TEST_MAX,
};

/* Macros */
/* (struct tccvin_stream *) to (struct device *) */
#define stream_to_device(ptr) (&((((ptr)->tdev)->pdev)->dev))

/* ------------------------------------------------------------------------
 * Debugging, printing and logging
 */

#define LOG_TAG "VIN"

#define loge(dev, fmt, ...)                                                    \
	{                                                                      \
		dev_err(dev, "[ERROR][%s] %s - " fmt, LOG_TAG, __func__,       \
			##__VA_ARGS__);                                        \
	}

#define logw(dev, fmt, ...)                                                    \
	{                                                                      \
		dev_warn(dev, "[WARN][%s] %s - " fmt, LOG_TAG, __func__,       \
			 ##__VA_ARGS__);                                       \
	}

#define logd(dev, fmt, ...)                                                               \
	{ /* dev_info(dev, "[DEBUG][%s] %s - " fmt, LOG_TAG, __func__, ##__VA_ARGS__); */ \
	}

#define logi(dev, fmt, ...)                                                    \
	{                                                                      \
		dev_info(dev, "[INFO][%s] %s - " fmt, LOG_TAG, __func__,       \
			 ##__VA_ARGS__);                                       \
	}

#define trace_e(dev, fmt, ...)                                                 \
	{                                                                      \
		trace_printk("[%s][ERROR][%s] %s - " fmt, dev_name(dev),       \
			     LOG_TAG, __func__, ##__VA_ARGS__);                \
	}

#define trace_w(dev, fmt, ...)                                                 \
	{                                                                      \
		trace_printk("[%s][WARN][%s] %s - " fmt, dev_name(dev),        \
			     LOG_TAG, __func__, ##__VA_ARGS__);                \
	}

#define trace_d(dev, fmt, ...)                                                 \
	{                                                                      \
		trace_printk("[%s][DEBUG][%s] %s - " fmt, dev_name(dev),       \
			     LOG_TAG, __func__, ##__VA_ARGS__);                \
	}

#define trace_i(dev, fmt, ...)                                                 \
	{                                                                      \
		trace_printk("[%s][INFO][%s] %s - " fmt, dev_name(dev),        \
			     LOG_TAG, __func__, ##__VA_ARGS__);                \
	}

#endif
