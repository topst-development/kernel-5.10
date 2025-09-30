/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2020 Telechips Inc.
 */
#ifndef __DT_BINDINGS_TCC750X_CSS_H
#define __DT_BINDINGS_TCC750X_CSS_H

// #include "../../../drivers/media/platform/tccvin2/750x/vin_wrap_vin.h"
// #include "../../../drivers/media/platform/tccvin2/750x/vin_wrap_wdma.h"
/* VIDEO IN : 0x10XX */
#define VIN_WRAP_VIN			(0x1000U)
#define VIN_WRAP_VIN00			(0x1000U)
#define VIN_WRAP_VIN01			(0x1001U)
#define VIN_WRAP_VIN10			(0x1002U)
#define VIN_WRAP_VIN11			(0x1003U)
#define VIN_WRAP_VIN_MAX		(0x0004U)

/* VIN WrapperWDMA : 0x20XX */
#define VIN_WRAP_WDMA			(0x2000U)
#define VIN_WRAP_WDMA0			(0x2000U)
#define VIN_WRAP_WDMA1			(0x2001U)
#define VIN_WRAP_WDMA_MAX		(0x0002U)
#endif /* __DT_BINDINGS_TCC750X_CSS_H */
