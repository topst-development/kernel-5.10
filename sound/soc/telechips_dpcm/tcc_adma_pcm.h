/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_ADMA_PCM_DT_H
#define TCC_ADMA_PCM_DT_H
#include "tcc_maic_adma.h"

#define TCC_ADMA0_DAI "tcc-adma0-dai"
#define TCC_ADMA1_DAI "tcc-adma1-dai"
#define TCC_ADMA2_DAI "tcc-adma2-dai"
#define TCC_ADMA3_DAI "tcc-adma3-dai"
#define TCC_ADMA4_DAI "tcc-adma4-dai"
#define TCC_ADMA5_DAI "tcc-adma5-dai"
#define TCC_ADMA6_DAI "tcc-adma6-dai"
#define TCC_ADMA7_DAI "tcc-adma7-dai"

#define TCC_ADMA0_SPDIF "tcc-adma0-spdif"
#define TCC_ADMA1_SPDIF "tcc-adma1-spdif"
#define TCC_ADMA2_SPDIF "tcc-adma2-spdif"
#define TCC_ADMA3_SPDIF "tcc-adma3-spdif"
#define TCC_ADMA4_SPDIF "tcc-adma4-spdif"
#define TCC_ADMA5_SPDIF "tcc-adma5-spdif"
#define TCC_ADMA6_SPDIF "tcc-adma6-spdif"
#define TCC_ADMA7_SPDIF "tcc-adma7-spdif"

#define ADMA0_PLAYBACK_WIDGET		"TCC-ADMA0-Playback"
#define ADMA0_CAPTURE_WIDGET		"TCC-ADMA0-Capture"
#define ADMA1_PLAYBACK_WIDGET		"TCC-ADMA1-Playback"
#define ADMA1_CAPTURE_WIDGET		"TCC-ADMA1-Capture"
#define ADMA2_PLAYBACK_WIDGET		"TCC-ADMA2-Playback"
#define ADMA2_CAPTURE_WIDGET		"TCC-ADMA2-Capture"
#define ADMA3_PLAYBACK_WIDGET		"TCC-ADMA3-Playback"
#define ADMA3_CAPTURE_WIDGET		"TCC-ADMA3-Capture"
#define ADMA4_PLAYBACK_WIDGET		"TCC-ADMA4-Playback"
#define ADMA4_CAPTURE_WIDGET		"TCC-ADMA4-Capture"
#define ADMA5_PLAYBACK_WIDGET		"TCC-ADMA5-Playback"
#define ADMA5_CAPTURE_WIDGET		"TCC-ADMA5-Capture"
#define ADMA6_PLAYBACK_WIDGET		"TCC-ADMA6-Playback"
#define ADMA6_CAPTURE_WIDGET		"TCC-ADMA6-Capture"
#define ADMA7_PLAYBACK_WIDGET		"TCC-ADMA7-Playback"
#define ADMA7_CAPTURE_WIDGET		"TCC-ADMA7-Capture"

#define ADMA0_SPDIF_PLAYBACK_WIDGET		"TCC-ADMA0-SPDIF-Playback"
#define ADMA0_SPDIF_CAPTURE_WIDGET		"TCC-ADMA0-SPDIF-Capture"
#define ADMA1_SPDIF_PLAYBACK_WIDGET		"TCC-ADMA1-SPDIF-Playback"
#define ADMA1_SPDIF_CAPTURE_WIDGET		"TCC-ADMA1-SPDIF-Capture"
#define ADMA2_SPDIF_PLAYBACK_WIDGET		"TCC-ADMA2-SPDIF-Playback"
#define ADMA2_SPDIF_CAPTURE_WIDGET		"TCC-ADMA2-SPDIF-Capture"
#define ADMA3_SPDIF_PLAYBACK_WIDGET		"TCC-ADMA3-SPDIF-Playback"
#define ADMA3_SPDIF_CAPTURE_WIDGET		"TCC-ADMA3-SPDIF-Capture"
#define ADMA4_SPDIF_PLAYBACK_WIDGET		"TCC-ADMA4-SPDIF-Playback"
#define ADMA4_SPDIF_CAPTURE_WIDGET		"TCC-ADMA4-SPDIF-Capture"
#define ADMA5_SPDIF_PLAYBACK_WIDGET		"TCC-ADMA5-SPDIF-Playback"
#define ADMA5_SPDIF_CAPTURE_WIDGET		"TCC-ADMA5-SPDIF-Capture"
#define ADMA6_SPDIF_PLAYBACK_WIDGET		"TCC-ADMA6-SPDIF-Playback"
#define ADMA6_SPDIF_CAPTURE_WIDGET		"TCC-ADMA6-SPDIF-Capture"
#define ADMA7_SPDIF_PLAYBACK_WIDGET		"TCC-ADMA7-SPDIF-Playback"
#define ADMA7_SPDIF_CAPTURE_WIDGET		"TCC-ADMA7-SPDIF-Capture"

enum TCC_ADMA_DEV_TYPE {
	TCC_ADMA_I2S_STEREO = 0,
	TCC_ADMA_I2S_7_1CH = 1,
	TCC_ADMA_SPDIF = 2,
	TCC_ADMA_MAX = 3
};

struct tcc_adma_info {
	enum TCC_ADMA_DEV_TYPE dev_type;
	bool tdm_mode;
};

#endif //TCC_ADMA_PCM_DT_H
