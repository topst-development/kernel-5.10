// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_VPU_V3_VERSION_H
#define TCC_VPU_V3_VERSION_H

/*
 * VPU v3 Version
 * a.bb.ccc
 * a : major
 * b : minor
 * c : revision
 */
#define VPU_V3_VERSION_MAJOR 3  // major
#define VPU_V3_VERSION_MINOR 3  // minor
#define VPU_V3_VERSION_REV 21  // minor

#define str(s) #s
#define stringify(s) str(s)

#define VPU_V3_VERSION_STRING \
			"v" \
		stringify(VPU_V3_VERSION_MAJOR) "." \
		stringify(VPU_V3_VERSION_MINOR) "." \
		stringify(VPU_V3_VERSION_REV)

#define VPU_V3_DRIVER_VERSION VPU_V3_VERSION_STRING

#endif // TCC_VPU_V3_VERSION_H

