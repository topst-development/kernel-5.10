/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_AUDIO_HW_H
#define TCC_AUDIO_HW_H

#define unused(x) (void)(x)

#define TRUE	((bool) true)
#define FALSE	((bool) false)

#define TCC_AUDIO_ARRAY_SIZE(a) (uint32_t)(sizeof(a) / sizeof((a)[0]))

//----------------------------------------------------------------------------------------------------------------------------
// Data format
//----------------------------------------------------------------------------------------------------------------------------
enum DataFormat {
	STEREO      = 0b0001    ,
	TDM4        = 0b0011    ,
	TDM8        = 0b0111    ,
	TDM16       = 0b1111
};

//----------------------------------------------------------------------------------------------------------------------------
// Data width
//----------------------------------------------------------------------------------------------------------------------------
enum DataWidth {
	BIT16		= 0U,
	BIT24_LSB	= 1U,
	BIT24_MSB	= 2U,
	BIT32		= 3U
};

//----------------------------------------------------------------------------------------------------------------------------
// Data signed/unsigned
//----------------------------------------------------------------------------------------------------------------------------
enum DataSign {
	DATA_UNSIGNED	= 0U,
	DATA_SIGNED		= 1U
};

//----------------------------------------------------------------------------------------------------------------------------
// Bit-converting mode
//----------------------------------------------------------------------------------------------------------------------------
enum BitConvert {
	MSB_PADDING = 0U,
	ZERO_PADDING = 1U
};

#define FMT_SIGNED   1
#define FMT_UNSIGNED 0

#define PADDING_MSB 0
#define PADDING_LSB 1

#define DATA_WIDTH_16B  (0u)
#define DATA_WIDTH_24LB (1u)
#define DATA_WIDTH_24MB (2u)
#define DATA_WIDTH_32B  (3u)


#include "tcc_audio_rule.h"
#include "tcc_ccu_hw.h"
#include "tcc_maic_hw.h"
#if defined(CONFIG_SND_SOC_TELECHIPS_EXTENDED_TBD)
#include "tcc_mavc_hw.h"
#include "tcc_mars_hw.h"
#include "tcc_masm_hw.h"
#include "tcc_masrc_hw.h"
#include "tcc_mafc_hw.h"
#endif
#include "tcc_audio_cfg_hw.h"

#endif /*T CC_AUDIO_HW_H */
