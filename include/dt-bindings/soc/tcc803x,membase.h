/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (C) 2023 Telechips Inc.
 */

#ifndef DT_BINDINGS_TCC803X_MEMBASE_H
#define DT_BINDINGS_TCC803X_MEMBASE_H

/*
 * Default size for EVB: 4GiB (Max: 4GiB)
 * Modify below according to the memory size on your machine.
 */

#define RSVD_MEM_BASE_32	(0x20000000U)
#define RSVD_MEM_SIZE_32	(0xA0000000U)

#define RSVD_MEM_BASE_64H	(0x1U)
#define RSVD_MEM_BASE_64L	(0xA0000000U)
#define RSVD_MEM_SIZE_64H	(0x00000000U)
#define RSVD_MEM_SIZE_64L	(0x60000000U)

#endif /* DT_BINDINGS_TCC803X_MEMBASE_H */
