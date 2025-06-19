/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2023 Telechips Inc.
 */

#ifndef CLK_TCC805X__H
#define CLK_TCC805X__H

#include <dt-bindings/clock/telechips,tcc805x-clks.h>

/************************************
 * Need for pmu&ckc register control
 ************************************/

#define XIN_CLK_RATE    (24UL*1000UL*1000UL)  // 24MHz
#define XTIN_CLK_RATE   (32768UL)   // 32.768kHz
#define HDMI_CLK_RATE   (27UL*1000UL*1000UL)
#define HDMI_PCLK_RATE  (27UL)      // dummy value for set lcdc peri source to hdmi pclk
#define TCC_EXT0_CLK_RATE   (24UL*1000UL*1000UL)  // 24MHz
#define TCC_EXT1_CLK_RATE   (24UL*1000UL*1000UL)  // 24MHz

#define CKC_DISABLE     0UL
#define CKC_ENABLE      1UL
#define CKC_NOCHANGE    2UL

#define CKC_RESUME	0UL
#define CKC_SUSPEND	1UL

#define CKC_OK		0UL


/* IOBUS Peripheral Clock Enable */
enum {/* Peri. Name */
	IOBUS_PERI_UART0 = 8,
	IOBUS_PERI_UART1,
	IOBUS_PERI_UART2,		// 10
	IOBUS_PERI_UART3,
	IOBUS_PERI_UART4,
	IOBUS_PERI_UART5,
	IOBUS_PERI_UART6,
	IOBUS_PERI_UART7,		// 15
	IOBUS_PERI_SMARTCARD0,
	IOBUS_PERI_SMARTCARD1,
	IOBUS_PERI_SMARTCARD2,
	IOBUS_PERI_SMARTCARD3,
	IOBUS_PERI_SMARTCARD4,		// 20
	IOBUS_PERI_SMARTCARD5,
	IOBUS_PERI_SMARTCARD6,
	IOBUS_PERI_SMARTCARD7,
	IOBUS_PERI_I2C_MASTER0,
	IOBUS_PERI_I2C_MASTER1,		// 25
	IOBUS_PERI_I2C_MASTER2,
	IOBUS_PERI_I2C_MASTER3,
	IOBUS_PERI_I2C_MASTER4,
	IOBUS_PERI_I2C_MASTER5,
	IOBUS_PERI_I2C_MASTER6,		// 30
	IOBUS_PERI_I2C_MASTER7,
};

/* SWRESET Register (0x140000B4) */
enum {
	SWRST_CPUMP = 0,    // CPUMP
	SWRST_CPUAP,        // CPUAP
	SWRST_CPUBUS,       // CPU Bus
	SWRST_CMBUS,        // CM Bus
	SWRST_MEMBUS,       // Memory Bus
	SWRST_VBUS,         // Video Bus
	SWRST_HSIOBUS,      // HSIO Bus
	SWRST_SMUBUS,       // SMU Bus
	SWRST_GPU,          // 3D
	SWRST_DDIBUS,       // Display Bus
	SWRST_G2D,          // 2D
	SWRST_IOBUS,        // IO Bus
	SWRST_VCORE,        // Video core
	SWRST_CHEVC,        // CHEVC
	SWRST_VHEVC,        // VHEVC
	SWRST_BHEVC,        // BHEVC
	SWRST_PCIE,         // PCIE
	SWRST_U30,          // U30
	SWRST_SDMA,         // SDMA
};

enum {
	DCKC_GPU = 0,
	DCKC_G2D,
	DCKC_MAX,
};

enum {
	VBUS_VP9 = 1,
	VBUS_HEVC,
	VBUS_CODA
};

#endif /* __CLK_TCC805X__H__ */
