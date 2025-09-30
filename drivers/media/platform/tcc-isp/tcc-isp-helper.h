/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_ISP_HELPER_H
#define TCC_ISP_HELPER_H

#define LOG_TAG			TCC_ISP_DRIVER_NAME

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

#define tccisp_loge(fmt, ...)						\
do {									\
	int32_t len = 0;						\
	len = pr_err("[ERROR][%s] %s(%d) - "				\
		     fmt, LOG_TAG, __func__, __LINE__, ##__VA_ARGS__);	\
} while (false)

#define tccisp_logd(fmt, ...)						\
do {									\
	int32_t len = 0;						\
	len = pr_debug("[DEBUG][%s] %s(%d) - "				\
		       fmt, LOG_TAG, __func__, __LINE__, ##__VA_ARGS__);\
} while (false)

#define DEFAULT_WIDTH		0x8000
#define DEFAULT_HEIGHT		0x8000

/*
 * To use ISP UART,
 * enable "USE_ISP_UART" define and pinctl properties of isp device node in
 * device tree
 *
 * TCC8059 does not have an ball out of ISP UART.
 * TCC8050/53 has an ball out of ISP UART. Refer to the GPIO pins below.
 * UART TX - GPIO_MB[0] / GPIO_MB[6] / GPIO_MB[12] / GPIO_MB[24]
 * UART RX - GPIO_MB[1] / GPIO_MB[7] / GPIO_MB[13] / GPIO_MB[25]
 * But, GPIO pins above is used for other purpose.
 * So, H/W modification is needed to use ISP UART on the TCC8050/53 EVB.
 * Refer to the TCS(CV8050C-606)
 */
//#define USE_ISP_UART

/*
 * To print ISP setting value for debug,
 * enable define PRINT_SETTING_VALUE.
 */
//#define PRINT_SETTING_VALUE

/*
 * To disable ISP cropping,
 * enable define DISABLE_ISP_CROP
 */
//#define DISABLE_ISP_CROP

#define tcc_isp_readl(base, offset) __raw_readl((base) + (offset))
#define tcc_isp_writel(val, base, offset) __raw_writel(val, (base) + (offset))
#endif
