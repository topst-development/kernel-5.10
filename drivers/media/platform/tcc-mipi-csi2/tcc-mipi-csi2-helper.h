/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_MIPI_CSI2_HELPER_H
#define TCC_MIPI_CSI2_HELPER_H

#define LOG_TAG		TCC_MIPI_CSI2_DRIVER_NAME

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

static inline uint32_t tcc_mipi_csi2_readl(const void __iomem *base,
					   uint32_t offset)
{
	return __raw_readl(base + offset);
}

static inline void tcc_mipi_csi2_writel(uint32_t val,
					void __iomem *base,
					uint32_t offset)
{
	__raw_writel(val, base + offset);
}


#endif
