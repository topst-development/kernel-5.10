/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_ADMA_PCM_DT_H
#define TCC_ADMA_PCM_DT_H

#include "tcc_adma.h"

enum TCC_ADMA_DEV_TYPE {
	TCC_ADMA_I2S_STEREO = 0,
	TCC_ADMA_I2S_7_1CH = 1,
	TCC_ADMA_I2S_9_1CH = 2,
	TCC_ADMA_SPDIF = 3,
	TCC_ADMA_MAX = 4
};

struct tcc_adma_info {
	enum TCC_ADMA_DEV_TYPE dev_type;
	bool tdm_mode;
};

#endif //TCC_ADMA_PCM_DT_H
