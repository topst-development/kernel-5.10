/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_IDI2AXI_HELPER_H
#define TCC_IDI2AXI_HELPER_H

#define LOG_TAG TCC_IDI2AXI_DRIVER_NAME

#define loge(dev_ptr, fmt, ...)						\
do {									\
	dev_err(dev_ptr, "[ERROR][%s] %s(%d) - "			\
		fmt, LOG_TAG, __func__, __LINE__, ##__VA_ARGS__);	\
} while (false)

#define logw(dev_ptr, fmt, ...)						\
do {									\
	dev_warn(dev_ptr, "[WARN][%s] %s(%d) - "			\
		fmt, LOG_TAG, __func__, __LINE__, ##__VA_ARGS__);	\
} while (false)

#define logi(dev_ptr, fmt, ...)						\
do {									\
	dev_info(dev_ptr, "[INFO][%s] %s(%d) - " fmt,			\
		LOG_TAG, __func__, __LINE__, ##__VA_ARGS__);		\
} while (false)

#define logd(dev_ptr, fmt, ...)						\
do {									\
	dev_dbg(dev_ptr, "[DEBUG][%s] %s(%d) - "			\
		fmt, LOG_TAG, __func__, __LINE__, ##__VA_ARGS__);	\
} while (false)

#endif
