/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#ifndef VPU_WBUFFER_H
#define VPU_WBUFFER_H

#if defined(__ANDROID_COMMON_KERNEL__) || defined(CONFIG_ANDROID)
#define ANDROID_VPU_KERNEL
#endif

#if defined(ANDROID_VPU_KERNEL)
#if defined(CONFIG_ARCH_TCC803X)
# 	include <dt-bindings/pmap/tcc803x/tcc803x-vpu-android-ivi-customized.h>
#elif defined(CONFIG_ARCH_TCC805X)
#   if defined(CONFIG_TOPST)
#       include <dt-bindings/pmap/tcc805x/tcc805x-vpu-android-topst-customized.h>
#   else
# 	    include <dt-bindings/pmap/tcc805x/tcc805x-vpu-android-ivi-customized.h>
#   endif
#elif defined(CONFIG_ARCH_TCC807X)
# 	include <dt-bindings/pmap/tcc807x/tcc807x-vpu-android-ivi-customized.h>
#elif defined(CONFIG_ARCH_TCC897X)
# 	include <dt-bindings/pmap/tcc897x/tcc897x-vpu-android-ivi-customized.h>
#endif

#else  // ifdef ANDROID_VPU_KERNEL

#if defined(CONFIG_TELECHIPS_VPU_DVRS)
#if defined(CONFIG_ARCH_TCC805X)
# 	include <dt-bindings/pmap/tcc805x/tcc805x-vpu-linux-dvrs-customized.h>
#elif defined(CONFIG_ARCH_TCC807X)
# 	include <dt-bindings/pmap/tcc807x/tcc807x-vpu-linux-dvrs-customized.h>
#endif

#elif defined(CONFIG_TELECHIPS_VPU_CONSOLIDATION) //defined(CONFIG_TELECHIPS_VPU_DVRS)

#if defined(CONFIG_ARCH_TCC805X)
#if defined(CONFIG_TCC805X_CA53Q)
#	include <dt-bindings/pmap/tcc805x/tcc805x-vpu-linux-consolidation-subcore-customized.h>
#else
# 	include <dt-bindings/pmap/tcc805x/tcc805x-vpu-linux-consolidation-customized.h>
#endif
#endif

#elif defined(CONFIG_TELECHIPS_VPU_CLUSTER) //defined(CONFIG_TELECHIPS_VPU_DVRS)
#if defined(CONFIG_ARCH_TCC897X)
# 	include <dt-bindings/pmap/tcc897x/tcc897x-vpu-linux-cluster-customized.h>
#endif

#else //defined(CONFIG_TELECHIPS_VPU_DVRS)

#if defined(CONFIG_ARCH_TCC803X)
# 	include <dt-bindings/pmap/tcc803x/tcc803x-vpu-linux-ivi-customized.h>
#elif defined(CONFIG_ARCH_TCC805X)
# 	if defined(CONFIG_TCC805X_CA53Q)
#		if defined(CONFIG_TOPST)
#			include <dt-bindings/pmap/tcc805x/tcc805x-vpu-linux-topst-subcore-customized.h>
#		else
#			include <dt-bindings/pmap/tcc805x/tcc805x-vpu-linux-ivi-subcore-customized.h>
#		endif
#	else
#		if defined(CONFIG_TOPST)
#			include <dt-bindings/pmap/tcc805x/tcc805x-vpu-linux-topst-customized.h>
#		else
#			include <dt-bindings/pmap/tcc805x/tcc805x-vpu-linux-ivi-customized.h>
#		endif
# 	endif
#elif defined(CONFIG_ARCH_TCC807X)
#	if defined(CONFIG_TCC807X_CA55_SUB)
#		include <dt-bindings/pmap/tcc807x/tcc807x-vpu-linux-ivi-subcore-customized.h>
#	else
#		include <dt-bindings/pmap/tcc807x/tcc807x-vpu-linux-ivi-customized.h>
#	endif
#elif defined(CONFIG_ARCH_TCC750X)
# 	include <dt-bindings/pmap/tcc750x/tcc750x-vpu-customized.h>
#elif defined(CONFIG_ARCH_TCC897X)
# 	include <dt-bindings/pmap/tcc897x/tcc897x-vpu-linux-ivi-customized.h>
#endif

#endif // defined(CONFIG_TELECHIPS_VPU_DVRS)
#endif // ANDROID_VPU_KERNEL

#endif // VPU_WBUFFER_H
