// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_VERSION_H
#define VPU_VERSION_H

/*
 * VPU Version
 * a.bb.ccc
 * a : structure version
 * b : library header version
 * c : driver update version
 */
#define VPU_MAJOR 2  // Structure Version
#define VPU_MINOR 4  // Library header version
#define VPU_PATCH 21 // Driver update version

#define ver_str(s) #s
#define ver_stringify(s) ver_str(s)

#define VPU_VERSION_STRING \
			"v" \
			ver_stringify(VPU_MAJOR) "." \
			ver_stringify(VPU_MINOR) "." \
			ver_stringify(VPU_PATCH)

#define VPU_DRIVER_VERSION VPU_VERSION_STRING

#endif // VPU_VERSION_H
