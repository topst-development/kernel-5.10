/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: shkim@telechips.com
*/

#ifndef TCC_VPU_CODEC_H_
#define TCC_VPU_CODEC_H_

#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC802X) || \
	defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC807X)
#include "TCC_VPU_C7_CODEC.h"
#elif defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC897X) || \
	defined(CONFIG_ARCH_TCC901X)
#include "TCC_VPU_D6.h"
#endif

#ifndef RETCODE_MULTI_CODEC_EXIT_TIMEOUT
#define RETCODE_MULTI_CODEC_EXIT_TIMEOUT    99
#endif

#ifndef SZ_1M
#define SZ_1M   (1024 * 1024)
#endif

#define VPU_ENC_PUT_HEADER_SIZE (16 * 1024)

#endif // TCC_VPU_CODEC_H_
