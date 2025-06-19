/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (C) 2023 Telechips Inc.
 */

#ifndef DT_BINDINGS_TCN100X_MEMBASE_H
#define DT_BINDINGS_TCN100X_MEMBASE_H

/*
 * Default size for EVB: 8 GiB (6 GiB available for AP-side)
 * Modify below according to the memory size on your machine.
 */

#define RSVD_MEM_BASE_64H	(0x00000001U)
#define RSVD_MEM_BASE_64L	(0x80000000U)
#define RSVD_MEM_SIZE_64H	(0x00000001U)
#define RSVD_MEM_SIZE_64L	(0x80000000U)

#endif /* DT_BINDINGS_TCN100X_MEMBASE_H */
