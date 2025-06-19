// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 * FileName   : tcc_vpu_wbuffer.h
 * Description: TCC VPU h/w block
 */
#ifndef VPU_WBUFFER_H
#define VPU_WBUFFER_H

#if defined(CONFIG_ANDROID)
#if defined(CONFIG_ARCH_TCC803X)
# 	include <dt-bindings/pmap/tcc803x/tcc803x-vpu-android-ivi-customized.h>
#elif defined(CONFIG_ARCH_TCC805X)
# 	include <dt-bindings/pmap/tcc805x/tcc805x-vpu-android-ivi-customized.h>
#elif defined(CONFIG_ARCH_TCC807X)
# 	include <dt-bindings/pmap/tcc807x/tcc807x-vpu-android-ivi-customized.h>
#endif

#else  // ifdef CONFIG_ANDROID

#if defined(CONFIG_TELECHIPS_VPU_DVRS)
#if defined(CONFIG_ARCH_TCC805X)
# 	include <dt-bindings/pmap/tcc805x/tcc805x-vpu-linux-dvrs-customized.h>
#elif defined(CONFIG_ARCH_TCC807X)
# 	include <dt-bindings/pmap/tcc807x/tcc807x-vpu-linux-dvrs-customized.h>
#endif

#elif defined(CONFIG_TELECHIPS_VPU_CONSOLIDATION) //defined(CONFIG_TELECHIPS_VPU_DVRS)

#if defined(CONFIG_ARCH_TCC805X)
#if defined(CONFIG_TCC805X_CA53Q)
# 	include <dt-bindings/pmap/tcc805x/tcc805x-vpu-linux-consolidation-subcore-customized.h>
#else
# 	include <dt-bindings/pmap/tcc805x/tcc805x-vpu-linux-consolidation-customized.h>
#endif
#endif

#else //defined(CONFIG_TELECHIPS_VPU_DVRS)

#if defined(CONFIG_ARCH_TCC803X)
# 	include <dt-bindings/pmap/tcc803x/tcc803x-vpu-linux-ivi-customized.h>
#elif defined(CONFIG_ARCH_TCC805X)
# 	if defined(CONFIG_TCC805X_CA53Q)
# 		include <dt-bindings/pmap/tcc805x/tcc805x-vpu-linux-ivi-subcore-customized.h>
# 	else
# 		include <dt-bindings/pmap/tcc805x/tcc805x-vpu-linux-ivi-customized.h>
# 	endif
#elif defined(CONFIG_ARCH_TCC807X)
# 	if defined(CONFIG_TCC807X_CA55_SUB)
# 		include <dt-bindings/pmap/tcc807x/tcc807x-vpu-linux-ivi-subcore-customized.h>
# 	else
# 		include <dt-bindings/pmap/tcc807x/tcc807x-vpu-linux-ivi-customized.h>
# 	endif
#elif defined(CONFIG_ARCH_TCC750X)
# 	include <dt-bindings/pmap/tcc750x/tcc750x-vpu-customized.h>
#endif

#endif // defined(CONFIG_TELECHIPS_VPU_DVRS)
#endif // CONFIG_ANDROID

#endif // VPU_WBUFFER_H
