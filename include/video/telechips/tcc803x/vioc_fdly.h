/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef VIOC_FDLY_H
#define	VIOC_FDLY_H


/*
 * Frame Delay (Base Addr = 0x12003900)
 */
struct VIOC_FDLY_CTRL {
	unsigned int FMT :  1;
	unsigned int     : 15;
	unsigned int     : 16;
};

union VIOC_FDLY_CTRL_u {
	unsigned long nREG;
	struct VIOC_FDLY_CTRL bREG;
};

struct VIOC_FDLY_RATE {
	unsigned int     : 16;
	unsigned MAXRATE :  8;
	unsigned int     :  7;
	unsigned REN     :  1;
};

union VIOC_FDLY_RATE_u {
	unsigned long nREG;
	struct VIOC_FDLY_RATE bREG;
};

struct VIOC_FDLY_BG {
	unsigned int BG0 : 8;
	unsigned int BG1 : 8;
	unsigned int BG2 : 8;
	unsigned int BG3 : 8;
};

union VIOC_FDLY_BG_u {
	unsigned long nREG;
	struct VIOC_FDLY_BG bREG;
};

struct VIOC_FDLY {
	union VIOC_FDLY_CTRL_u uCTRL; // 0x00 R/W Frame Delay Control Reg.
	union VIOC_FDLY_RATE_u uRATE; // 0x04 R/W Frame Delay Rate Control Reg.
	unsigned int uBASE0; // 0x08 R/W Frame Delay Base Address 0 Reg.
	unsigned int uBASE1; // 0x0C R/W Frame Delay Base Address 1 Reg.
	union VIOC_FDLY_BG_u uBG; // 0x10 R/W Frame Delay Default Color Reg.
	unsigned int reserved0[3]; // 5,6,7
};

#endif /*__VIOC_FDLY_H__*/

