// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_SCREEN_SHARED_BUFFER_H
#define TCC_SCREEN_SHARED_BUFFER_H

void tcc_scrshare_set_sharedBuffer(
	unsigned int addr, unsigned int frameWidth,
	unsigned int frameHeight, unsigned int fmt,
	unsigned int rgb_swap);

#endif
