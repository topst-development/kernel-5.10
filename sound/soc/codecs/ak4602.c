/*
 * ak4602.c  --  audio driver for AK4602
 *
 * Copyright (C) 2021 Asahi Kasei Microdevices Corporation
 * Modified by Copyright Telechips Inc.
 * Modified date :
 * Description :
 *
 *  Author             Date        Revision
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                   17/06/05	    1.0 	3.18.XX
 *                   17/06/08	    1.1
 *                   17/08/10       1.7
 *                   21/10/11       1.8		4.19.XX
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2 as
 *	published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include <linux/of_gpio.h>
#include <linux/regmap.h>

#include "ak4602.h"

//#define AK4602_DEBUG			//used at debug mode

#define AK4602_I2C_IF

#ifdef AK4602_DEBUG
#define akdbgprt pr_info
#else
#define akdbgprt(format, arg...) do {} while (0)
#endif

#define NUM_SYNCDOMAIN  5

#if IS_BUILTIN(CONFIG_SND_SOC_AK4602)
#define TCC_INITIAL_SETTING
#endif
#define TCC_ALWAYS_PMDAC_ON
#define TCC_USE_MUTE_GPIO
#define TCC_CODEC_SAMPLING_FREQ
#define TCC_EVB

struct ak4602_priv {
	struct spi_device *spi;
	struct i2c_client *i2c;
	struct regmap *regmap;
	int pdn_gpio;

	int SDfs[NUM_SYNCDOMAIN];     // 0:8kHz, 1:12kHz, 2:16kHz, 3:24kHz, 4:32kHz, 5:48kHz, 6:96kHz, 7:192kHz
	int SDBick[NUM_SYNCDOMAIN];   // 0:64fs, 1:48fs, 2:32fs, 3:128fs, 4:256fs, 5:No, 6:No, 7:512fs
	int Master[NUM_SYNCDOMAIN];   // 0 : Slave Mode, 1 : Master
	int SDCks[NUM_SYNCDOMAIN];    // 0 : Low,  1: PLLMCLK

	int tdmsdin[NUM_SYNCDOMAIN];     // 0 : Stereo mode,  1 : TDM mode
	int tdmsdout[NUM_SYNCDOMAIN];     // 0 : Stereo mode,  1 : TDM mode
	int SlotLenIn[NUM_SYNCDOMAIN];      // SDIN Slot Length  0 : 24bit, 1 : 20bit, 2 : 16 bit, 3 : 32bit
	int SlotLenOut[NUM_SYNCDOMAIN];     // SDOUT Slot Length  0 : 24bit, 1 : 20bit, 2 : 16 bit, 3 : 32bit

	int I2Smode2;    // Port2 I2S I/F  0 : Normal,  1 : Invert

#ifdef TCC_USE_MUTE_GPIO
	int32_t cmute_gpio;         // for CODEC_MUTE
	int32_t cmute_gpio_flags;
	int32_t amute_gpio;         // for AMP_MUTE
	int32_t amute_gpio_flags;
	int32_t stanby_gpio;        // for AMP_STANBY
	int32_t stanby_gpio_flags;
	int32_t playback_active;
#endif//TCC_USE_MUTE_GPIO
#ifdef TCC_CODEC_SAMPLING_FREQ
	bool adc2;
	uint32_t codec_freq;
	uint32_t adc1_freq;
#endif
#ifdef TCC_EVB
	bool is_updated;
	uint32_t dai_fmt;
#endif
};

static const struct reg_default ak4602_reg[] = {
	{ 0x0, 0x00 },  // AK4602_00_SYSTEM_CLOCK_1
	{ 0x1, 0x00 },  // AK4602_01_SYSTEM_CLOCK_2
	{ 0x2, 0x00 },  // AK4602_02_SYSTEM_CLOCK_3
	{ 0x3, 0x00 },  // AK4602_03_MIC_BIAS_POWER
	{ 0x4, 0x00 },  // AK4602_04_CLOCK_SD1_SET_1
	{ 0x5, 0x00 },  // AK4602_05_CLOCK_SD1_SET_2
	{ 0x6, 0x00 },  // AK4602_06_CLOCK_SD1_SET_3
	{ 0x7, 0x00 },  // AK4602_07_CLOCK_SD2_SET_1
	{ 0x8, 0x00 },  // AK4602_08_CLOCK_SD2_SET_2
	{ 0x9, 0x00 },  // AK4602_09_CLOCK_SD2_SET_3
	{ 0xA, 0x00 },  // AK4602_0A_CLOCK_SD3_SET_1
	{ 0xB, 0x00 },  // AK4602_0B_CLOCK_SD3_SET_2
	{ 0xC, 0x00 },  // AK4602_0C_CLOCK_SD3_SET_3
	{ 0xD, 0x00 },  // AK4602_0D_CLOCK_SD4_SET_1
	{ 0xE, 0x00 },  // AK4602_0E_CLOCK_SD4_SET_2
	{ 0xF, 0x00 },  // AK4602_0F_CLOCK_SD4_SET_3
	{ 0x10, 0x00 },  // AK4602_10_CLOCK_SD5_SET_1
	{ 0x11, 0x00 },  // AK4602_11_CLOCK_SD5_SET_2
	{ 0x12, 0x00 },  // AK4602_12_CLOCK_SD5_SET_3
	{ 0x13, 0x00 },  // AK4602_13_RESERVED
	{ 0x14, 0x00 },  // AK4602_14_RESERVED
	{ 0x15, 0x00 },  // AK4602_15_CLKO_OUTPUT
	{ 0x16, 0x00 },  // AK4602_16_RESERVED
	{ 0x17, 0x00 },  // AK4602_17_CLOCK_SD_SEL_1
	{ 0x18, 0x00 },  // AK4602_18_CLOCK_SD_SEL_2
	{ 0x19, 0x00 },  // AK4602_19_CLOCK_SD_SEL_3
	{ 0x1A, 0x00 },  // AK4602_1A_EXTCLOCK_1
	{ 0x1B, 0x00 },  // AK4602_1B_EXTCLOCK_2
	{ 0x1C, 0x00 },  // AK4602_1C_CLOCK_SD_SEL_4
	{ 0x1D, 0x00 },  // AK4602_1D_CLOCK_SD_SEL_5
	{ 0x1E, 0x00 },  // AK4602_1E_RESERVED
	{ 0x1F, 0x00 },  // AK4602_1F_CLOCK_SD_SEL_7
	{ 0x20, 0x00 },  // AK4602_20_CLOCK_SD_SEL_8
	{ 0x21, 0x00 },  // AK4602_21_CLOCK_SD_SEL_9
	{ 0x22, 0x00 },  // AK4602_22_CLOCK_SD_SEL_10
	{ 0x23, 0x00 },  // AK4602_23_CLOCK_SD_SEL_11
	{ 0x24, 0x00 },  // AK4602_24_CLOCK_SD_SEL_12
	{ 0x25, 0x00 },  // AK4602_25_CLOCK_SD_SEL_13
	{ 0x26, 0x00 },  // AK4602_26_CLOCK_SD_SEL_14
	{ 0x27, 0x00 },  // AK4602_27_CLOCK_SD_SEL_15
	{ 0x28, 0x00 },  // AK4602_28_CLOCK_SD_SEL_16
	{ 0x29, 0x00 },  // AK4602_29_CLOCK_SD_SEL_17
	{ 0x2A, 0x00 },  // AK4602_2A_SDOUT1_TDM_SLOT1_2
	{ 0x2B, 0x00 },  // AK4602_2B_SDOUT1_TDM_SLOT3_4
	{ 0x2C, 0x00 },  // AK4602_2C_SDOUT1_TDM_SLOT5_6
	{ 0x2D, 0x00 },  // AK4602_2D_SDOUT1_TDM_SLOT7_8
	{ 0x2E, 0x00 },  // AK4602_2E_SDOUT1_TDM_SLOT9_10
	{ 0x2F, 0x00 },  // AK4602_2F_SDOUT1_TDM_SLOT11_1
	{ 0x30, 0x00 },  // AK4602_30_SDOUT1_TDM_SLOT13_1
	{ 0x31, 0x00 },  // AK4602_31_SDOUT1_TDM_SLOT15_1
	{ 0x32, 0x00 },  // AK4602_32_SDOUT2_TDM_SLOT1_2
	{ 0x33, 0x00 },  // AK4602_33_SDOUT2_TDM_SLOT3_4
	{ 0x34, 0x00 },  // AK4602_34_SDOUT2_TDM_SLOT5_6
	{ 0x35, 0x00 },  // AK4602_35_SDOUT2_TDM_SLOT7_8
	{ 0x36, 0x00 },  // AK4602_36_SDOUT2_TDM_SLOT9_10
	{ 0x37, 0x00 },  // AK4602_37_SDOUT2_TDM_SLOT11_1
	{ 0x38, 0x00 },  // AK4602_38_SDOUT2_TDM_SLOT13_1
	{ 0x39, 0x00 },  // AK4602_39_SDOUT2_TDM_SLOT15_1
	{ 0x3A, 0x00 },  // AK4602_3A_SDOUT3_TDM_SLOT1_2
	{ 0x3B, 0x00 },  // AK4602_3B_SDOUT3_TDM_SLOT3_4
	{ 0x3C, 0x00 },  // AK4602_3C_SDOUT3_TDM_SLOT5_6
	{ 0x3D, 0x00 },  // AK4602_3D_SDOUT3_TDM_SLOT7_8
	{ 0x3E, 0x00 },  // AK4602_3E_SDOUT3_TDM_SLOT9_10
	{ 0x3F, 0x00 },  // AK4602_3F_SDOUT3_TDM_SLOT11_1
	{ 0x40, 0x00 },  // AK4602_40_SDOUT3_TDM_SLOT13_1
	{ 0x41, 0x00 },  // AK4602_41_SDOUT3_TDM_SLOT15_1
	{ 0x42, 0x00 },  // AK4602_42_SDOUT4_TDM_SLOT1_2
	{ 0x43, 0x00 },  // AK4602_43_SDOUT4_TDM_SLOT3_4
	{ 0x44, 0x00 },  // AK4602_44_SDOUT4_TDM_SLOT5_6
	{ 0x45, 0x00 },  // AK4602_45_SDOUT4_TDM_SLOT7_8
	{ 0x46, 0x00 },  // AK4602_46_SDOUT4_TDM_SLOT9_10
	{ 0x47, 0x00 },  // AK4602_47_SDOUT4_TDM_SLOT11_1
	{ 0x48, 0x00 },  // AK4602_48_SDOUT4_TDM_SLOT13_1
	{ 0x49, 0x00 },  // AK4602_49_SDOUT4_TDM_SLOT15_1
	{ 0x4A, 0x00 },  // AK4602_4A_SDOUT5_TDM_SLOT1_2
	{ 0x4B, 0x00 },  // AK4602_4B_SDOUT5_TDM_SLOT3_4
	{ 0x4C, 0x00 },  // AK4602_4C_SDOUT5_TDM_SLOT5_6
	{ 0x4D, 0x00 },  // AK4602_4D_SDOUT5_TDM_SLOT7_8
	{ 0x4E, 0x00 },  // AK4602_4E_SDOUT5_TDM_SLOT9_10
	{ 0x4F, 0x00 },  // AK4602_4F_SDOUT5_TDM_SLOT11_1
	{ 0x50, 0x00 },  // AK4602_50_SDOUT5_TDM_SLOT13_1
	{ 0x51, 0x00 },  // AK4602_51_SDOUT5_TDM_SLOT15_1
	{ 0x52, 0x00 },  // AK4602_52_DAC1_INPUT_DATA
	{ 0x53, 0x00 },  // AK4602_53_DAC2_INPUT_DATA
	{ 0x54, 0x00 },  // AK4602_54_DAC3_INPUT_DATA
	{ 0x55, 0x00 },  // AK4602_55_VOL1_INPUT_DATA
	{ 0x56, 0x00 },  // AK4602_56_VOL2_INPUT_DATA
	{ 0x57, 0x00 },  // AK4602_57_VOL3_INPUT_DATA
	{ 0x58, 0x00 },  // AK4602_58_VOL4_INPUT_DATA
	{ 0x59, 0x00 },  // AK4602_59_VOL5_INPUT_DATA
	{ 0x5A, 0x00 },  // AK4602_5A_SRC1_INPUT_DATA
	{ 0x5B, 0x00 },  // AK4602_5B_SRC2_INPUT_DATA
	{ 0x5C, 0x00 },  // AK4602_5C_SRC3_INPUT_DATA
	{ 0x5D, 0x00 },  // AK4602_5D_SRC4_INPUT_DATA
	{ 0x5E, 0x00 },  // AK4602_5E_RESERVED
	{ 0x5F, 0x00 },  // AK4602_5F_RESERVED
	{ 0x60, 0x00 },  // AK4602_60_RESERVED
	{ 0x61, 0x00 },  // AK4602_61_RESERVED
	{ 0x62, 0x00 },  // AK4602_62_MIXER_A_CH1_INPUT
	{ 0x63, 0x00 },  // AK4602_63_MIXER_A_CH2_INPUT
	{ 0x64, 0x00 },  // AK4602_64_MIXER_B_CH1_INPUT
	{ 0x65, 0x00 },  // AK4602_65_MIXER_B_CH2_INPUT
	{ 0x66, 0x00 },  // AK4602_66_DIT_INPUT_DATA
	{ 0x67, 0x00 },  // AK4602_67_SBO1_INPUT_DATA
	{ 0x68, 0x00 },  // AK4602_68_SBO2_INPUT_DATA
	{ 0x69, 0x00 },  // AK4602_69_CLOCK_FORMAT_1
	{ 0x6A, 0x00 },  // AK4602_6A_CLOCK_FORMAT_2
	{ 0x6B, 0x00 },  // AK4602_6B_CLOCK_FORMAT_3
	{ 0x6C, 0x00 },  // AK4602_6C_RESERVED
	{ 0x6D, 0x00 },  // AK4602_6D_SDIN1_DIGITAL_FORMAT
	{ 0x6E, 0x00 },  // AK4602_6E_SDIN2_DIGITAL_FORMAT
	{ 0x6F, 0x00 },  // AK4602_6F_SDIN3_DIGITAL_FORMAT
	{ 0x70, 0x00 },  // AK4602_70_SDIN4_DIGITAL_FORMAT
	{ 0x71, 0x00 },  // AK4602_71_SDIN5_DIGITAL_FORMAT
	{ 0x72, 0x00 },  // AK4602_72_SDOUT1_DIGITAL_FORMAT
	{ 0x73, 0x00 },  // AK4602_73_SDOUT2_DIGITAL_FORMAT
	{ 0x74, 0x00 },  // AK4602_74_SDOUT3_DIGITAL_FORMAT
	{ 0x75, 0x00 },  // AK4602_75_SDOUT4_DIGITAL_FORMAT
	{ 0x76, 0x00 },  // AK4602_76_SDOUT5_DIGITAL_FORMAT
	{ 0x77, 0x00 },  // AK4602_77_SDOUT_PHASE
	{ 0x78, 0x00 },  // AK4602_78_RESERVED
	{ 0x79, 0x00 },  // AK4602_79_RESERVED
	{ 0x7A, 0x00 },  // AK4602_7A_OUTPUT_PORT
	{ 0x7B, 0x00 },  // AK4602_7B_OUTPUT_PORT_ENABLE
	{ 0x7C, 0x00 },  // AK4602_7C_RESERVED
	{ 0x7D, 0x00 },  // AK4602_7D_MIXER_A
	{ 0x7E, 0x00 },  // AK4602_7E_MIXER_B
	{ 0x7F, 0x00 },  // AK4602_7F_MIC_AMP_GAIN
	{ 0x80, 0x00 },  // AK4602_80_MIC_AMP_GAIN_CONTRO
	{ 0x81, 0x30 },  // AK4602_81_ADC1_LCH_DIGITAL_VO
	{ 0x82, 0x30 },  // AK4602_82_ADC1_RCH_DIGITAL_VO
	{ 0x83, 0x30 },  // AK4602_83_ADC2_LCH_DIGITAL_VO
	{ 0x84, 0x30 },  // AK4602_84_ADC2_RCH_DIGITAL_VO
	{ 0x85, 0x30 },  // AK4602_85_ADCM_DIGITAL_VOL
	{ 0x86, 0x00 },  // AK4602_86_ANALOG_INPUT_SELECT
	{ 0x87, 0x00 },  // AK4602_87_ADC_MUTE_HPF_CONTRO
	{ 0x88, 0x18 },  // AK4602_88_DAC1_LCH_DIGITAL_VO
	{ 0x89, 0x18 },  // AK4602_89_DAC1_RCH_DIGITAL_VO
	{ 0x8A, 0x18 },  // AK4602_8A_DAC2_LCH_DIGITAL_VO
	{ 0x8B, 0x18 },  // AK4602_8B_DAC2_RCH_DIGITAL_VO
	{ 0x8C, 0x18 },  // AK4602_8C_DAC3_LCH_DIGITAL_VO
	{ 0x8D, 0x18 },  // AK4602_8D_DAC3_RCH_DIGITAL_VO
	{ 0x8E, 0x02 },  // AK4602_8E_DAC_MUTE_FILTER
	{ 0x8F, 0x15 },  // AK4602_8F_DAC_DEM
	{ 0x90, 0x18 },  // AK4602_90_VOL1_LCH_DIGITAL_VO
	{ 0x91, 0x18 },  // AK4602_91_VOL1_RCH_DIGITAL_VO
	{ 0x92, 0x18 },  // AK4602_92_VOL2_LCH_DIGITAL_VO
	{ 0x93, 0x18 },  // AK4602_93_VOL2_RCH_DIGITAL_VO
	{ 0x94, 0x18 },  // AK4602_94_VOL3_LCH_DIGITAL_VO
	{ 0x95, 0x18 },  // AK4602_95_VOL3_RCH_DIGITAL_VO
	{ 0x96, 0x18 },  // AK4602_96_VOL4_LCH_DIGITAL_VO
	{ 0x97, 0x18 },  // AK4602_97_VOL4_RCH_DIGITAL_VO
	{ 0x98, 0x18 },  // AK4602_98_VOL5_LCH_DIGITAL_VO
	{ 0x99, 0x18 },  // AK4602_99_VOL5_RCH_DIGITAL_VO
	{ 0x9A, 0x00 },  // AK4602_9A_VOL_SETTING
	{ 0x9B, 0x00 },  // AK4602_9B_SRC_FILTER
	{ 0x9C, 0x00 },  // AK4602_9C_SRC_PHASE_GROUP_1
	{ 0x9D, 0x00 },  // AK4602_9D_RESERVED
	{ 0x9E, 0x00 },  // AK4602_9E_SRC_MUTE_SETTING1
	{ 0x9F, 0x00 },  // AK4602_9F_SRC_MUTE_SETTING2
	{ 0xA0, 0x00 },  // AK4602_A0_STO_FLAG_1
	{ 0xA1, 0x00 },  // AK4602_A1_STO_FLAG_2
	{ 0xA2, 0x00 },  // AK4602_A2_DIT_STATUS_BIT_1
	{ 0xA3, 0x04 },  // AK4602_A3_DIT_STATUS_BIT_2
	{ 0xA4, 0x02 },  // AK4602_A4_DIT_STATUS_BIT_3
	{ 0xA5, 0x00 },  // AK4602_A5_DIT_STATUS_BIT_4
	{ 0xA6, 0x00 },  // AK4602_A6_RESERVED
	{ 0xA7, 0x00 },  // AK4602_A7_POWER_MANAGEMENT_1
	{ 0xA8, 0x00 },  // AK4602_A8_POWER_MANAGEMENT_2
	{ 0xA9, 0x20 },  // AK4602_A9_RESET_CONTROL
	{ 0xAA, 0x00 },  // RESERVED
	{ 0xAB, 0x00 },  // RESERVED
	{ 0xAC, 0x00 },  // RESERVED
	{ 0xAD, 0x00 },  // RESERVED
	{ 0xAE, 0x00 },  // RESERVED
	{ 0xAF, 0x00 },  // RESERVED
	{ 0xB0, 0x00 },  // RESERVED
	{ 0xB1, 0x00 },  // RESERVED
	{ 0xB2, 0x00 },  // RESERVED
	{ 0xB3, 0x00 },  // RESERVED
	{ 0xB4, 0x00 },  // RESERVED
	{ 0xB5, 0x00 },  // RESERVED
	{ 0xB6, 0x00 },  // RESERVED
	{ 0xB7, 0x00 },  // RESERVED
	{ 0xB8, 0x00 },  // RESERVED
	{ 0xB9, 0x00 },  // RESERVED
	{ 0xBA, 0x00 },  // RESERVED
	{ 0xBB, 0x00 },  // RESERVED
	{ 0xBC, 0x00 },  // RESERVED
	{ 0xBD, 0x00 },  // RESERVED
	{ 0xBE, 0x00 },  // RESERVED
	{ 0xBF, 0x00 },  // RESERVED
	{ 0xC0, 0x00 },  // RESERVED
	{ 0xC1, 0x00 },  // RESERVED
	{ 0xC2, 0x00 },  // RESERVED
	{ 0xC3, 0x00 },  // RESERVED
	{ 0xC4, 0x00 },  // RESERVED
	{ 0xC5, 0x00 },  // RESERVED
	{ 0xC6, 0x00 },  // RESERVED
	{ 0xC7, 0x00 },  // RESERVED
	{ 0xC8, 0x00 },  // RESERVED
	{ 0xC9, 0x00 },  // RESERVED
	{ 0xCA, 0x00 },  // RESERVED
	{ 0xCB, 0x00 },  // RESERVED
	{ 0xCC, 0x00 },  // RESERVED
	{ 0xCD, 0x00 },  // RESERVED
	{ 0xCE, 0x00 },  // RESERVED
	{ 0xCF, 0x00 },  // RESERVED
	{ 0xD0, 0x00 },  // RESERVED
	{ 0xD1, 0x00 },  // RESERVED
	{ 0xD2, 0x00 },  // RESERVED
	{ 0xD3, 0x00 },  // RESERVED
	{ 0xD4, 0x00 },  // RESERVED
	{ 0xD5, 0x00 },  // RESERVED
	{ 0xD6, 0x00 },  // RESERVED
	{ 0xD7, 0x00 },  // RESERVED
	{ 0xD8, 0x00 },  // RESERVED
	{ 0xD9, 0x00 },  // RESERVED
	{ 0xDA, 0x00 },  // RESERVED
	{ 0xDB, 0x00 },  // RESERVED
	{ 0xDC, 0x00 },  // RESERVED
	{ 0xDD, 0x00 },  // RESERVED
	{ 0xDE, 0x00 },  // RESERVED
	{ 0xDF, 0x00 },  // RESERVED
	{ 0xE0, 0x00 },  // RESERVED
	{ 0xE1, 0x00 },  // RESERVED
	{ 0xE2, 0x00 },  // RESERVED
	{ 0xE3, 0x00 },  // RESERVED
	{ 0xE4, 0x00 },  // RESERVED
	{ 0xE5, 0x00 },  // RESERVED
	{ 0xE6, 0x00 },  // RESERVED
	{ 0xE7, 0x00 },  // RESERVED
	{ 0xE8, 0x00 },  // RESERVED
	{ 0xE9, 0x00 },  // RESERVED
	{ 0xEA, 0x00 },  // RESERVED
	{ 0xEB, 0x00 },  // RESERVED
	{ 0xEC, 0x00 },  // RESERVED
	{ 0xED, 0x00 },  // RESERVED
	{ 0xEE, 0x00 },  // RESERVED
	{ 0xEF, 0x00 },  // RESERVED
	{ 0xF0, 0x00 },  // RESERVED
	{ 0xF1, 0x00 },  // RESERVED
	{ 0xF2, 0x00 },  // RESERVED
	{ 0xF3, 0x00 },  // RESERVED
	{ 0xF4, 0x00 },  // RESERVED
	{ 0xF5, 0x00 },  // RESERVED
	{ 0xF6, 0x00 },  // RESERVED
	{ 0xF7, 0x00 },  // RESERVED
	{ 0xF8, 0x00 },  // RESERVED
	{ 0xF9, 0x00 },  // RESERVED
	{ 0xFA, 0x00 },  // RESERVED
	{ 0xFB, 0x00 },  // RESERVED
	{ 0xFC, 0x00 },  // RESERVED
	{ 0xFD, 0x00 },  // RESERVED
	{ 0xFE, 0x00 },  // RESERVED
	{ 0xFF, 0x00 },  // RESERVED
	{ 0x100, 0x00 },  // RESERVED
	{ 0x101, 0x00 },  // RESERVED
	{ 0x102, 0x00 },  // AK4602_102_STATUS_READ
	{ 0x103, 0x00 }   // AK4602_103_SRC_STATUS_1
};

#define AK4602_XTI_FREQUECY   12288000  // 12.288MHz

enum {
	CLOCK_LOW = 0,
	CLOCK_SD1,
	CLOCK_SD2,
	CLOCK_SD3,
	CLOCK_SD4,
	CLOCK_SD5,
};

#define PORT1_CLOCK_SD   CLOCK_SD1
#define PORT2_CLOCK_SD   CLOCK_SD2
#define PORT3_CLOCK_SD   CLOCK_SD3
#define PORT4_CLOCK_SD   CLOCK_SD4
#define PORT5_CLOCK_SD   CLOCK_SD5

static int sdfstab[] = {
	8000, 12000, 16000, 24000,
	32000, 48000, 96000, 192000
};

static int sdbicktab[] = {
	64, 48, 32, 128, 256, 1, 1, 512
};

#define AK4602_RATES (SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT)

#define SLIM_CLOSE_TIMEOUT	1000

#define AK4602_FORMATS_AIF (SNDRV_PCM_FMTBIT_S16_LE | \
		SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FORMAT_S32_LE | \
		SNDRV_PCM_FORMAT_MU_LAW | SNDRV_PCM_FORMAT_A_LAW)

enum {
	AIF_PORT1 = 0,
	AIF_PORT2,
	AIF_PORT3,
	AIF_PORT4,
	AIF_PORT5,
	NUM_CODEC_DAIS,
};

static int ak4602_i2s_set_fmt(struct snd_soc_dai *dai, unsigned int fmt);

int ak4602_mclk_enable(struct snd_soc_component *component, int mclk_enable, bool dapm)
{
//	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	pr_debug("%s: mclk_enable = %u, dapm = %d\n", __func__, mclk_enable,
		 dapm);

	return 0;
}

static void ak4602_i2s_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;

	switch (dai->id) {
	case AIF_PORT1:
		snd_soc_component_update_bits(component, AK4602_04_CLOCK_SD1_SET_1, 0x80, 0x00);
		break;
	case AIF_PORT2:
		snd_soc_component_update_bits(component, AK4602_07_CLOCK_SD2_SET_1, 0x80, 0x00);
		break;
	case AIF_PORT3:
		snd_soc_component_update_bits(component, AK4602_0A_CLOCK_SD3_SET_1, 0x80, 0x00);
		break;
	case AIF_PORT4:
		snd_soc_component_update_bits(component, AK4602_0D_CLOCK_SD4_SET_1, 0x80, 0x00);
		break;
	case AIF_PORT5:
		snd_soc_component_update_bits(component, AK4602_10_CLOCK_SD5_SET_1, 0x80, 0x00);
		break;
	default:
		pr_err("%s: Invalid dai id %d\n", __func__, dai->id);
		break;
	}
}

static int setSDMaster(
struct snd_soc_component *component,
int nSDNo,
int nMaster)
{
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int addr;
	int value;

	if (nSDNo >=  NUM_SYNCDOMAIN)
		return 0;

	ak4602->Master[nSDNo] = nMaster;

	addr = AK4602_04_CLOCK_SD1_SET_1 + (3 * nSDNo);
	value = (nMaster << 7);
	snd_soc_component_update_bits(component, addr, 0x80, value);

	return 0;

}

#ifdef TCC_CODEC_SAMPLING_FREQ
//ADC2, ADCM, DIT DAC1/2/3 : ADC1
static int setCodecFSMode(struct snd_soc_component *component, uint32_t codec_freq, uint32_t adc1_freq)
{
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	unsigned int value = 0, cfreq = 0, afreq = 0;
	if ((codec_freq < 8000) || (codec_freq > 192000)) cfreq = 48000;
	else cfreq = codec_freq;

	if(ak4602->adc2 == true) { //SDOUT1 from ADC2
		afreq = cfreq;
	} else { //SDOUT1 from ADC1
		if ((adc1_freq < 8000) || (adc1_freq > 192000)) afreq = 48000;
		else afreq = adc1_freq;
	}

	switch (cfreq) {
	case 8000:
		value = 0;
		//"8kHz:8kHz", "7.35kHz:7.35kHz",
		break;
	case 11025:
		value = 1;
		//"12kHz:12kHz", "11.025kHz:11.025kHz",
		break;
	case 16000:
		value = 2;
		//"16kHz:16kHz", "14.7kHz:14.7kHz",
		break;
	case 22050:
	case 24000:
		value = 3;
		//"24kHz:24kHz", "22.05kHz:22.05kHz",
		break;
	case 32000:
		if (afreq == 8000) {
			value = 6;
		} else if ((afreq == 16000) || (afreq == 11025)) {
			value = 5;
		} else {
			value = 4;
		}
		//"32kHz:8kHz", "29.4kHz:7.35kHz",
		//"32kHz:16kHz", "29.4kHz:14.7kHz",
		//"32kHz:32kHz", "29.4kHz:29.4kHz",
		break;
	case 44100:
	case 48000:
		if (afreq == 8000) {
			value = 10;
		} else if ((afreq == 16000) || (afreq == 11025)) {
			value = 9;
		} else if ((afreq == 24000) || (afreq == 22050)) {
			value = 8;
		} else {
			value = 7;
		}
		//"48kHz:8kHz", "44.1kHz:7.35kHz",
		//"48kHz:16kHz", "44.1kHz:14.7kHz",
		//"48kHz:24kHz", "44.1kHz:22.05kHz",
		//"48kHz:48kHz", "44.1kHz:44.1kHz",

		break;
	case 88200:
	case 96000:
		if (afreq == 8000) {
			value = 16;
		} else if ((afreq == 16000) || (afreq == 11025)) {
			value = 15;
		} else if ((afreq == 24000) || (afreq == 22050)) {
			value = 14;
		} else if (afreq == 32000) {
			value = 13;
		} else if ((afreq == 48000) || (afreq == 44100)) {
			value = 12;
		} else {
			value = 11;
		}
		//"96kHz:8kHz", "88.2kHz:7.35kHz",
		//"96kHz:16kHz", "88.2kHz:14.7kHz",
		//"96kHz:24kHz", "88.2kHz:22.05kHz"
		//"96kHz:32kHz", "88.2kHz:29.4kHz",
		//"96kHz:48kHz", "88.2kHz:44.1kHz",
		//"96kHz:96kHz", "88.2kHz:88.2kHz",
		break;
	case 176400:
	case 192000:
		if ((afreq == 16000) || (afreq == 11025)) {
			value = 21;
		} else if (afreq == 32000) {
			value = 20;
		} else if ((afreq == 48000) || (afreq == 44100)) {
			value = 19;
		} else if ((afreq == 96000) || (afreq == 88200)) {
			value = 18;
		} else {
			value = 17;
		}
		//"192kHz:16kHz", "176.4kHz:14.7kHz"
		//"192kHz:32kHz", "176.4kHz:29.4kHz",
		//"192kHz:48kHz", "176.4kHz:44.1kHz",
		//"192kHz:96kHz", "176.4kHz:88.2kHz",
		//"192kHz:192kHz", "176.4kHz:176.4kHz",
		break;
	default:
		pr_err("%s: Invalid sampling rate %d\n", __func__,
				cfreq);
		return -EINVAL;
	}

	snd_soc_component_update_bits(component, AK4602_02_SYSTEM_CLOCK_3, 0x1f, value);

	return 0;
}
#endif//TCC_CODEC_SAMPLING_FREQ

static int setSDClock(
struct snd_soc_component *component,
int nSDNo)
{

	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int addr;
	int fs, bickfs;
	int sdv, bdv, bdvh;

	if (nSDNo >= NUM_SYNCDOMAIN)
		return 0;

	fs = sdfstab[ak4602->SDfs[nSDNo]];
	bickfs = sdbicktab[ak4602->SDBick[nSDNo]] * fs;

	addr = AK4602_04_CLOCK_SD1_SET_1 + (3 * nSDNo);

	if (ak4602->SDCks[nSDNo] == 1)
		bdv = 122880000 / bickfs;
	else if (ak4602->SDCks[nSDNo] < 3)
		bdv = AK4602_XTI_FREQUECY / bickfs;
	else if (ak4602->SDCks[nSDNo] < 7)
		bdv = 1;
	else if (ak4602->SDCks[nSDNo] == 7)
		return(-EINVAL);
	else if (ak4602->SDCks[nSDNo] < 18)
		bdv = (256 * fs) / bickfs;
	else
		return(-EINVAL);

	sdv = ak4602->SDBick[nSDNo];
	akdbgprt("\t[AK4602]%s, BDV=%d, SDV=%d\n", __func__, bdv, sdbicktab[sdv]);
	bdv--;
	if (bdv > 511) {
		pr_err("%s: BDV Error! SD No = %d, bdv bit = %d\n", __func__, nSDNo, bdv);
		return (-EINVAL);
	} else if (bdv > 255) {
		bdvh = 0x80;
		bdv &= 0xFF;
	} else {
		bdvh = 0x0;
	}

	bdvh += ak4602->SDCks[nSDNo];

	snd_soc_component_update_bits(component, addr, 0x07, sdv);
	addr++;
	snd_soc_component_update_bits(component, addr, 0x9F, bdvh);
	addr++;
	snd_soc_component_update_bits(component, addr, 0xFF, bdv);

	return 0;

}

static int ak4602_i2s_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(dai->component);
	int addr1, addr2, value, sdno;
#ifdef TCC_EVB
	int addr3, addr4;
#endif

	pr_debug("%s: dai_name = %s DAI-ID %x rate %d num_ch %d\n", __func__,
			dai->name, dai->id, params_rate(params),
			params_channels(params));

	akdbgprt("\t[AK4602] %s(%d)\n", __func__, __LINE__);


#ifdef TCC_EVB
	if (ak4602->is_updated == true) {
		ak4602->is_updated = false;
		ak4602_i2s_set_fmt(dai, ak4602->dai_fmt);
	}

	switch (dai->id) {
	case AIF_PORT1:
		addr1 = AK4602_6D_SDIN1_DIGITAL_FORMAT;
		addr2 = AK4602_6E_SDIN2_DIGITAL_FORMAT;
		addr3 = AK4602_6F_SDIN3_DIGITAL_FORMAT;
		addr4 = AK4602_70_SDIN4_DIGITAL_FORMAT;
		sdno = PORT1_CLOCK_SD;  // 15/10/28
		break;
	case AIF_PORT2:
		addr1 = AK4602_72_SDOUT1_DIGITAL_FORMAT;
		addr2 = AK4602_73_SDOUT2_DIGITAL_FORMAT;
		sdno = PORT2_CLOCK_SD;  // 15/10/28
		snd_soc_component_update_bits(component, AK4602_1F_CLOCK_SD_SEL_7, 0x70, (sdno << 4));
		snd_soc_component_update_bits(component, AK4602_1F_CLOCK_SD_SEL_7, 0x07, sdno);
		break;
	default:
		pr_err("%s: Invalid dai id %d\n", __func__, dai->id);
		return -EINVAL;
	}
#else //TCC_EVB
	switch (dai->id) {
	case AIF_PORT1:
		addr1 = AK4602_6D_SDIN1_DIGITAL_FORMAT;
		addr2 = AK4602_72_SDOUT1_DIGITAL_FORMAT;
		sdno = PORT1_CLOCK_SD;  // 15/10/28
		snd_soc_component_update_bits(component, AK4602_1F_CLOCK_SD_SEL_7, 0x70, (sdno << 4));
		break;
	case AIF_PORT2:
		addr1 = AK4602_6E_SDIN2_DIGITAL_FORMAT;
		addr2 = AK4602_73_SDOUT2_DIGITAL_FORMAT;
		sdno = PORT2_CLOCK_SD;  // 15/10/28
		snd_soc_component_update_bits(component, AK4602_1F_CLOCK_SD_SEL_7, 0x07, sdno);
		break;
	case AIF_PORT3:
		addr1 = AK4602_6F_SDIN3_DIGITAL_FORMAT;
		addr2 = AK4602_74_SDOUT3_DIGITAL_FORMAT;
		sdno = PORT3_CLOCK_SD;  // 15/10/28
		snd_soc_component_update_bits(component, AK4602_20_CLOCK_SD_SEL_8,  0x70, (sdno << 4));
		break;
	case AIF_PORT4:
		addr1 = AK4602_70_SDIN4_DIGITAL_FORMAT;
		addr2 = AK4602_75_SDOUT4_DIGITAL_FORMAT;
		sdno = PORT4_CLOCK_SD;  // 15/10/28
		snd_soc_component_update_bits(component, AK4602_20_CLOCK_SD_SEL_8, 0x07, sdno);
		break;
	case AIF_PORT5:
		addr1 = AK4602_71_SDIN5_DIGITAL_FORMAT;
		addr2 = AK4602_76_SDOUT5_DIGITAL_FORMAT;
		sdno = PORT5_CLOCK_SD;  // 15/10/28
		snd_soc_component_update_bits(component, AK4602_21_CLOCK_SD_SEL_9, 0x70, (sdno << 4));
		break;
	default:
		pr_err("%s: Invalid dai id %d\n", __func__, dai->id);
		return -EINVAL;
	}
#endif //TCC_EVB

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		value = 2;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		value = 0;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		value = 3;
		break;
	default:
		pr_err("%s: invalid RX format %u\n", __func__,
			params_format(params));
		return -EINVAL;
	}

	if (sdno == 0)
		return -EINVAL;
	sdno--;

#ifdef TCC_EVB
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		snd_soc_component_update_bits(component, addr1, 0x3, value);
		snd_soc_component_update_bits(component, addr2, 0x3, value);
#ifdef TCC_CODEC_SAMPLING_FREQ
		value = snd_soc_component_read(component, AK4602_2A_SDOUT1_TDM_SLOT1_2);

		if(value == 0x2F) ak4602->adc2 = true;
		else ak4602->adc2 = false;

		if (ak4602->adc2 == true) {
			ak4602->codec_freq = params_rate(params);
		} else {
			ak4602->adc1_freq = params_rate(params);
		}
#endif
	} else {
		snd_soc_component_update_bits(component, addr1, 0x3, value);
		snd_soc_component_update_bits(component, addr2, 0x3, value);
		snd_soc_component_update_bits(component, addr3, 0x3, value);
		snd_soc_component_update_bits(component, addr4, 0x3, value);
#ifdef TCC_CODEC_SAMPLING_FREQ
		ak4602->codec_freq = params_rate(params);
#endif
	}
#else //TCC_EVB
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		snd_soc_component_update_bits(component, addr2, 0x3, value);
	else
		snd_soc_component_update_bits(component, addr1, 0x3, value);
#endif //TCC_EVB
	switch (params_rate(params)) {
	case 8000:
		ak4602->SDfs[sdno] = 0;
		break;
	case 11025:
		ak4602->SDfs[sdno] = 1;
		break;
	case 16000:
		ak4602->SDfs[sdno] = 2;
		break;
	case 22050:
	case 24000:
		ak4602->SDfs[sdno] = 3;
		break;
	case 32000:
		ak4602->SDfs[sdno] = 4;
		break;
	case 44100:
	case 48000:
		ak4602->SDfs[sdno] = 5;
		break;
	case 88200:
	case 96000:
		ak4602->SDfs[sdno] = 6;
		break;
	case 176400:
	case 192000:
		ak4602->SDfs[sdno] = 7;
		break;
	default:
		pr_err("%s: Invalid sampling rate %d\n", __func__,
				params_rate(params));
		return -EINVAL;
	}

#ifdef TCC_CODEC_SAMPLING_FREQ
	setCodecFSMode(component, ak4602->codec_freq, ak4602->adc1_freq);
#endif
	setSDClock(component, sdno);

	return 0;
}

static int ak4602_i2s_set_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
#ifdef TCC_EVB
	unsigned int value = 0;
	if (dai->id == AIF_PORT1) {
		switch (freq) {
			case 256000:
			case 235200:
				value = 0;
				break;
			case 384000:
			case 352800:
				value = 1;
				break;
			case 512000:
			case 470400:
				value = 2;
				break;
			case 768000:
			case 705600:
				value = 3;
				break;
			case 1024000:
			case 940800:
				value = 4;
				break;
			case 1152000:
			case 1058400:
				value = 5;
				break;
			case 1536000:
			case 1411200:
				value = 6;
				break;
			case 2048000:
			case 1881600:
				value = 7;
				break;
			case 2204000:
			case 2116800:
				value = 8;
				break;
			case 3072000:
			case 2822400:
				value = 9;
				break;
			case 4096000:
			case 3763200:
				value = 10;
				break;
			case 4608000:
			case 4233600:
				value = 11;
				break;
			case 6144000:
			case 5644800:
				value = 12;
				break;
			case 8192000:
			case 7526400:
				value = 13;
				break;
			case 9216000:
			case 8467200:
				value = 14;
				break;
			case 12288000:
			case 11289600:
				value = 15;
				break;
			case 18432000:
			case 16934400:
				value = 16;
				break;
			case 24576000:
			case 22579200:
				value = 17;
				break;
			default:
				pr_err("%s: Invalid MCLK %d\n", __func__,
						freq);
				return -EINVAL;
		}
		snd_soc_component_update_bits(dai->component, AK4602_00_SYSTEM_CLOCK_1, 0x1f, value);
	} else if (dai->id == AIF_PORT2) {
		if(freq >= 12288000) {
			snd_soc_component_update_bits(dai->component, AK4602_77_SDOUT_PHASE, 0x1F, 0x1);
			//SDOUT1 High Speed Mode Enable
		} else {
			snd_soc_component_update_bits(dai->component, AK4602_77_SDOUT_PHASE, 0x1F, 0x0);
			//SDOUT1 High Speed Mode Disable
		}
	}
#endif
	pr_debug("%s\n", __func__);
	return 0;
}

static int ak4602_i2s_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(dai->component);
	int addr0, addr1, addr2, addr3;
#ifdef TCC_EVB
	int addr4, addr5;
#endif
	int value0, value1, value2;
	int sdno;
	int shift, shift3;
	int portno;

	akdbgprt("\t[AK4602] %s(%d)\n", __func__, __LINE__);

#ifdef TCC_EVB
	if (fmt != 0) {
		ak4602->dai_fmt = fmt;
	}
	if (ak4602->is_updated == true) {
		ak4602->is_updated = false;
	}
	switch (dai->id) {
	case AIF_PORT1:
		sdno = PORT1_CLOCK_SD;
		addr0 = AK4602_69_CLOCK_FORMAT_1;
		shift = 4;
		addr1 = AK4602_17_CLOCK_SD_SEL_1;
		shift3 = 4;
		addr2 = AK4602_6D_SDIN1_DIGITAL_FORMAT;
		addr3 = AK4602_6E_SDIN2_DIGITAL_FORMAT;
		addr4 = AK4602_6F_SDIN3_DIGITAL_FORMAT;
		addr5 = AK4602_70_SDIN4_DIGITAL_FORMAT;
		portno = 0;
		break;
	case AIF_PORT2:
		sdno = PORT2_CLOCK_SD;
		addr0 = AK4602_69_CLOCK_FORMAT_1;
		shift = 0;
		addr1 = AK4602_17_CLOCK_SD_SEL_1;
		shift3 = 0;
		addr2 = AK4602_72_SDOUT1_DIGITAL_FORMAT;
		addr3 = AK4602_73_SDOUT2_DIGITAL_FORMAT;
		portno = 0;
		break;
	default:
		pr_err("%s: Invalid dai id %d\n", __func__, dai->id);
		return -EINVAL;
	}
#else // TCC_EVB
	switch (dai->id) {
	case AIF_PORT1:
		sdno = PORT1_CLOCK_SD;
		addr0 = AK4602_69_CLOCK_FORMAT_1;
		shift = 4;
		addr1 = AK4602_6D_SDIN1_DIGITAL_FORMAT;
		addr2 = AK4602_72_SDOUT1_DIGITAL_FORMAT;
		addr3 = AK4602_17_CLOCK_SD_SEL_1;
		shift3 = 4;
		portno = 0;
		break;
	case AIF_PORT2:
		sdno = PORT2_CLOCK_SD;
		addr0 = AK4602_69_CLOCK_FORMAT_1;
		shift = 0;
		addr1 = AK4602_6E_SDIN2_DIGITAL_FORMAT;
		addr2 = AK4602_73_SDOUT2_DIGITAL_FORMAT;
		addr3 = AK4602_17_CLOCK_SD_SEL_1;
		shift3 = 0;
		portno = 1;
		break;
	case AIF_PORT3:
		sdno = PORT3_CLOCK_SD;
		addr0 = AK4602_6A_CLOCK_FORMAT_2;
		shift = 4;
		addr1 = AK4602_6F_SDIN3_DIGITAL_FORMAT;
		addr2 = AK4602_74_SDOUT3_DIGITAL_FORMAT;
		addr3 = AK4602_18_CLOCK_SD_SEL_2;
		shift3 = 4;
		portno = 2;
		break;
	case AIF_PORT4:
		sdno = PORT4_CLOCK_SD;
		addr0 = AK4602_6A_CLOCK_FORMAT_2;
		shift = 0;
		addr1 = AK4602_70_SDIN4_DIGITAL_FORMAT;
		addr2 = AK4602_75_SDOUT4_DIGITAL_FORMAT;
		addr3 = AK4602_18_CLOCK_SD_SEL_2;
		shift3 = 0;
		portno = 3;
		break;
	case AIF_PORT5:
		sdno = PORT5_CLOCK_SD;
		addr0 = AK4602_6B_CLOCK_FORMAT_3;
		shift = 0;
		addr1 = AK4602_71_SDIN5_DIGITAL_FORMAT;
		addr2 = AK4602_76_SDOUT5_DIGITAL_FORMAT;
		addr3 = AK4602_19_CLOCK_SD_SEL_3;
		shift3 = 4;
		portno = 4;
		break;
	default:
		pr_err("%s: Invalid dai id %d\n", __func__, dai->id);
		return -EINVAL;
	}
#endif // TCC_EVB

	if (sdno == 0)
		return -EINVAL;
	sdno--;

	pr_debug("%s\n", __func__);
#ifdef TCC_EVB
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		ak4602->Master[portno] = 1;
		snd_soc_component_update_bits(dai->component, addr1, (0x7 << shift3), ((sdno+1) << shift3));
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		ak4602->Master[portno] = 0;
		snd_soc_component_update_bits(dai->component, addr1, (0x7 << shift3), 0);
		break;
	default:
		return -EINVAL;
	}
#else // TCC_EVB
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		ak4602->Master[portno] = 1;
		snd_soc_component_update_bits(dai->component, addr3, (0x7 << shift3), ((sdno+1) << shift3));
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		ak4602->Master[portno] = 0;
		snd_soc_component_update_bits(dai->component, addr3, (0x7 << shift3), 0);
		break;
	default:
		return -EINVAL;
	}
#endif // TCC_EVB

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_B:
		value0 = 7;    // PCM Long Frame
		value1 = 0x80 + (ak4602->SlotLenIn[portno] << 4);
		value2 = 0x80 + (ak4602->SlotLenOut[portno] << 4);
		break;
	case SND_SOC_DAIFMT_DSP_A:
		value0 = 6;    // PCM Short Fram
		value1 = 0x80 + (ak4602->SlotLenIn[portno] << 4);
		value2 = 0x80 + (ak4602->SlotLenOut[portno] << 4);
		break;
	case SND_SOC_DAIFMT_I2S:
		if ((portno == 1) && (ak4602->I2Smode2 == 1))
			value0 = 4;
		else
			value0 = 0;
		if (ak4602->tdmsdin[portno] == 1) {
#ifdef TCC_EVB
			value1 = 0x80 + (ak4602->SlotLenIn[portno] << 4);
		} else {
			value1 = (ak4602->SlotLenIn[portno])<< 4;
		}
#else
			ak4602->SlotLenIn[portno] = 3;
			value1 = 0xB0;
		} else {
			value1 = 0;
		}
#endif
		if (ak4602->tdmsdout[portno] == 1) {
#ifdef TCC_EVB
			value2 = 0x80 + (ak4602->SlotLenOut[portno] << 4);
		} else {
			value2 = (ak4602->SlotLenIn[portno]) << 4;
		}
#else
			ak4602->SlotLenOut[portno] = 3;
			value2 = 0xB0;
		} else {
			value2 = 0;
		}
#endif
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		if ((ak4602->tdmsdin[portno] == 1) || (ak4602->tdmsdout[portno] == 1)) {
			value0 = 7;
			value1 = 0x80 + (ak4602->SlotLenIn[portno] << 4);
			value2 = 0x80 + (ak4602->SlotLenOut[portno] << 4);
		} else {
			value0 = 5;
			value1 = 0;
			value2 = 0;
		}
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
#ifdef TCC_EVB
	case SND_SOC_DAIFMT_NB_IF:
	case SND_SOC_DAIFMT_IB_IF:
		break;
#endif
	case SND_SOC_DAIFMT_IB_NF:
		if (value0 > 5)
			value0 |= 0x8;
		break;
	default:
		return -EINVAL;
	}

#ifdef TCC_EVB
	snd_soc_component_update_bits(dai->component, addr0, (0xF << shift), (value0 << shift));
	switch (dai->id) {
		case AIF_PORT1:
			snd_soc_component_update_bits(dai->component, addr2, 0xB0, value1);
			snd_soc_component_update_bits(dai->component, addr3, 0xB0, value1);
			snd_soc_component_update_bits(dai->component, addr4, 0xB0, value1);
			snd_soc_component_update_bits(dai->component, addr5, 0xB0, value1);
			break;
		case AIF_PORT2:
			snd_soc_component_update_bits(dai->component, addr2, 0xB0, value2);
			snd_soc_component_update_bits(dai->component, addr3, 0xB0, value2);
			break;
	default:
		pr_err("%s: Invalid dai id %d\n", __func__, dai->id);
		return -EINVAL;
	}

	setSDMaster(dai->component, sdno, ak4602->Master[sdno]);
#else //TCC_EVB
	snd_soc_component_update_bits(dai->component, addr0, (0xF << shift), (value0 << shift));
	snd_soc_component_update_bits(dai->component, addr1, 0xB0, value1);
	snd_soc_component_update_bits(dai->component, addr2, 0xB0, value2);

	setSDMaster(dai->component, portno, ak4602->Master[portno]);
#endif //TCC_EVB
	return 0;
}

static int ak4602_i2s_set_pll(struct snd_soc_dai *dai, int id, int src,
			  unsigned int freq_in, unsigned int freq_out)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int ak4602_i2s_digital_mute(struct snd_soc_dai *dai, int mute, int direction)
{

	struct snd_soc_component *component = dai->component;
	unsigned int value;
	if (direction != SNDRV_PCM_STREAM_PLAYBACK) {
		return 0;
	}

	value = snd_soc_component_read(component, AK4602_A7_POWER_MANAGEMENT_1);
	value = (0x7 & (~value));  // ~PMDA1-3 bits
	value <<= 4;               //  DA*MUTE bits

	if (mute == 0)
		snd_soc_component_update_bits(component, AK4602_8E_DAC_MUTE_FILTER, 0x70, value);

	return 0;

}

#ifdef TCC_USE_MUTE_GPIO
static int ak4602_i2s_mute_stream(struct snd_soc_dai *dai, int mute, int stream)
{
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(dai->component);
	akdbgprt("\t[AK4602] %s(%d)\n",__FUNCTION__,__LINE__);
	if(stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (mute) {
			if (ak4602->cmute_gpio >= 0) {
				gpio_set_value(ak4602->cmute_gpio, ak4602->cmute_gpio_flags); //cmute on
				akdbgprt("\t[AK4602] cmute_gpio mute(%d)\n",__LINE__);
			}
			if (ak4602->amute_gpio >= 0) {
				gpio_set_value(ak4602->amute_gpio, ak4602->amute_gpio_flags); //amute on
				akdbgprt("\t[AK4602] amute_gpio mute(%d)\n",__LINE__);
			}
			akdbgprt("\t[AK4602] Mute ON(%d)\n",__LINE__);
		} else {
			if (snd_soc_component_get_bias_level(dai->component) == SND_SOC_BIAS_ON) {
				mdelay(2);
				if (ak4602->cmute_gpio >= 0) {
					gpio_set_value(ak4602->cmute_gpio, !ak4602->cmute_gpio_flags); //cmute off
					akdbgprt("\t[AK4602] cmute_gpio unmute(%d)\n",__LINE__);
				}
				if (ak4602->amute_gpio >= 0) {
					gpio_set_value(ak4602->amute_gpio, !ak4602->amute_gpio_flags); //amute off
					akdbgprt("\t[AK4602] amute_gpio unmute(%d)\n",__LINE__);
				}
			}
			akdbgprt("\t[AK4602] Mute OFF(%d)\n",__LINE__);
		}
	}

	return 0;
}
#endif//TCC_USE_MUTE_GPIO

#ifdef TCC_USE_MUTE_GPIO
static int ak4602_i2s_trigger(struct snd_pcm_substream *substream, int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	switch (cmd) {
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				ak4602->playback_active = 0;
				akdbgprt("\t[AK4602] SNDRV_PCM_TRIGGER_STOP(%d)\n",__LINE__);
			}
			break;
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
				ak4602->playback_active = 1;
				akdbgprt("\t[AK4602] SNDRV_PCM_TRIGGER_START(%d)\n",__LINE__);
			}
			break;
		default:
			return -EINVAL;
	}
	return 0;
}
#endif//TCC_USE_MUTE_GPIO

static struct snd_soc_dai_ops ak4602_i2s_dai_ops = {
	.shutdown = ak4602_i2s_shutdown,
	.hw_params = ak4602_i2s_hw_params,
	.set_sysclk = ak4602_i2s_set_sysclk,
	.set_fmt = ak4602_i2s_set_fmt,
	.set_pll = ak4602_i2s_set_pll,
	.mute_stream = ak4602_i2s_digital_mute,
#ifdef TCC_USE_MUTE_GPIO
	.mute_stream	= ak4602_i2s_mute_stream,
	.trigger	= ak4602_i2s_trigger,
#endif
};

static struct snd_soc_dai_driver ak4602_dai[] = {
	{
		.name = "ak4602-aif1",
		.id = AIF_PORT1,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = AK4602_RATES,
			.formats = AK4602_FORMATS_AIF,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 16,
		},
		.capture = {
			.stream_name = "AIF1 Capture",
			.rates = AK4602_RATES,
			.formats = AK4602_FORMATS_AIF,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 16,
		},
		.ops = &ak4602_i2s_dai_ops,
	},
	{
		.name = "ak4602-aif2",
		.id = AIF_PORT2,
		.playback = {
			.stream_name = "AIF2 Playback",
			.rates = AK4602_RATES,
			.formats = AK4602_FORMATS_AIF,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 16,
		},
		.capture = {
			.stream_name = "AIF2 Capture",
			.rates = AK4602_RATES,
			.formats = AK4602_FORMATS_AIF,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 16,
		},
		.ops = &ak4602_i2s_dai_ops,
	},
	{
		.name = "ak4602-aif3",
		.id = AIF_PORT3,
		.playback = {
			.stream_name = "AIF3 Playback",
			.rates = AK4602_RATES,
			.formats = AK4602_FORMATS_AIF,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 16,
		},
		.capture = {
			.stream_name = "AIF3 Capture",
			.rates = AK4602_RATES,
			.formats = AK4602_FORMATS_AIF,
			.rate_max = 96000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 16,
		},
		.ops = &ak4602_i2s_dai_ops,
	},
	{
		.name = "ak4602-aif4",
		.id = AIF_PORT4,
		.playback = {
			.stream_name = "AIF4 Playback",
			.rates = AK4602_RATES,
			.formats = AK4602_FORMATS_AIF,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 16,
		},
		.capture = {
			.stream_name = "AIF4 Capture",
			.rates = AK4602_RATES,
			.formats = AK4602_FORMATS_AIF,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 16,
		},
		.ops = &ak4602_i2s_dai_ops,
	},
	{
		.name = "ak4602-aif5",
		.id = AIF_PORT5,
		.playback = {
			.stream_name = "AIF5 Playback",
			.rates = AK4602_RATES,
			.formats = AK4602_FORMATS_AIF,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 16,
		},
		.capture = {
			.stream_name = "AIF5 Capture",
			.rates = AK4602_RATES,
			.formats = AK4602_FORMATS_AIF,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 16,
		},
		.ops = &ak4602_i2s_dai_ops,
	},
};

// ******************************* Control Function ************************************************

static int get_sd1_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SDCks[0];

	return 0;
}

static int set_sd1_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 8) {
		if (ak4602->SDCks[0] != currMode) {
			ak4602->SDCks[0] = currMode;
			setSDClock(component, 0);
			setSDMaster(component, 0, ak4602->Master[0]);
		}
	} else {
		akdbgprt(" [AK4602] %s Invalid Value selected!\n", __func__);
	}

	return 0;
}

static int get_sd2_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SDCks[1];

	return 0;
}

static int set_sd2_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 8) {
		if (ak4602->SDCks[1] != currMode) {
			ak4602->SDCks[1] = currMode;
			setSDClock(component, 1);
			setSDMaster(component, 1, ak4602->Master[1]);
		}
	} else {
		akdbgprt(" [AK4602] %s Invalid Value selected!\n", __func__);
	}
	return 0;
}

static int get_sd3_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SDCks[2];

	return 0;
}

static int set_sd3_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 8) {
		if (ak4602->SDCks[2] != currMode) {
			ak4602->SDCks[2] = currMode;
			setSDClock(component, 2);
			setSDMaster(component, 2, ak4602->Master[2]);
		}
	} else {
		akdbgprt(" [AK4602] %s Invalid Value selected!\n", __func__);
	}
	return 0;
}

static int get_sd4_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SDCks[3];

	return 0;
}

static int set_sd4_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 8) {
		if (ak4602->SDCks[3] != currMode) {
			ak4602->SDCks[3] = currMode;
			setSDClock(component, 3);
			setSDMaster(component, 3, ak4602->Master[3]);
		}
	} else {
		akdbgprt(" [AK4602] %s Invalid Value selected!\n", __func__);
	}
	return 0;
}

static int get_sd5_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SDCks[4];

	return 0;
}

static int set_sd5_cks(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 8) {
		if (ak4602->SDCks[4] != currMode) {
			ak4602->SDCks[4] = currMode;
			setSDClock(component, 4);
			setSDMaster(component, 4, ak4602->Master[4]);
		}
	} else {
		akdbgprt(" [AK4602] %s Invalid Value selected!\n", __func__);
	}
	return 0;
}

static int get_sd1_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->Master[0];

	return 0;
}

static int set_sd1_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	int    currMode = ucontrol->value.enumerated.item[0];

	setSDMaster(component, 0, currMode);

	return 0;
}


static int get_sd2_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->Master[1];

	return 0;
}

static int set_sd2_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	int    currMode = ucontrol->value.enumerated.item[0];

	setSDMaster(component, 1, currMode);

	return 0;
}


static int get_sd3_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->Master[2];

	return 0;
}

static int set_sd3_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	int    currMode = ucontrol->value.enumerated.item[0];

	setSDMaster(component, 2, currMode);

	return 0;
}


static int get_sd4_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->Master[3];

	return 0;
}

static int set_sd4_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	int    currMode = ucontrol->value.enumerated.item[0];

	setSDMaster(component, 3, currMode);

	return 0;
}

static int get_sd5_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->Master[4];

	return 0;
}

static int set_sd5_ms(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	int    currMode = ucontrol->value.enumerated.item[0];

	setSDMaster(component, 4, currMode);

	return 0;
}

static int get_sd1_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SDfs[0];

	return 0;
}

static int set_sd1_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 8) {
		if (ak4602->SDfs[0] != currMode) {
			ak4602->SDfs[0] = currMode;
			setSDClock(component, 0);
			setSDMaster(component, 0, ak4602->Master[0]);
		}
	} else {
		akdbgprt(" [AK4602] %s Invalid Value selected!\n", __func__);
	}
	return 0;
}

static int get_sd2_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SDfs[1];

	return 0;
}

static int set_sd2_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 8) {
		if (ak4602->SDfs[1] != currMode) {
			ak4602->SDfs[1] = currMode;
			setSDClock(component, 1);
			setSDMaster(component, 1, ak4602->Master[1]);
		}
	} else {
		akdbgprt(" [AK4602] %s Invalid Value selected!\n", __func__);
	}
	return 0;

}

static int get_sd3_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SDfs[2];

	return 0;
}

static int set_sd3_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 8) {
		if (ak4602->SDfs[2] != currMode) {
			ak4602->SDfs[2] = currMode;
			setSDClock(component, 2);
			setSDMaster(component, 2,  ak4602->Master[2]);
		}
	} else {
		akdbgprt(" [AK4602] %s Invalid Value selected!\n", __func__);
	}
	return 0;

}

static int get_sd4_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SDfs[3];

	return 0;
}


static int set_sd4_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 8) {
		if (ak4602->SDfs[3] != currMode) {
			ak4602->SDfs[3] = currMode;
			setSDClock(component, 3);
			setSDMaster(component, 3, ak4602->Master[3]);
		}
	} else {
		akdbgprt(" [AK4602] %s Invalid Value selected!\n", __func__);
	}
	return 0;

}

static int get_sd5_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SDfs[4];

	return 0;
}


static int set_sd5_fs(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	if (currMode < 8) {
		if (ak4602->SDfs[4] != currMode) {
			ak4602->SDfs[4] = currMode;
			setSDClock(component, 4);
			setSDMaster(component, 4, ak4602->Master[4]);
		}
	} else {
		akdbgprt(" [AK4602] %s Invalid Value selected!\n", __func__);
	}
	return 0;

}


static int get_sd1_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SDBick[0];

	return 0;
}

static int set_sd1_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	if ((currMode < 5) || (currMode == 7)) {
		if (ak4602->SDBick[0] != currMode) {
			ak4602->SDBick[0] = currMode;
			setSDClock(component, 0);
		}
	} else {
		akdbgprt(" [AK4602] %s Invalid Value selected!\n", __func__);
	}
	return 0;
}

static int get_sd2_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SDBick[1];

	return 0;
}

static int set_sd2_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	if ((currMode < 5) || (currMode == 7)) {
		if (ak4602->SDBick[1] != currMode) {
			ak4602->SDBick[1] = currMode;
			setSDClock(component, 1);
		}
	} else {
		akdbgprt(" [AK4602] %s Invalid Value selected!\n", __func__);
	}
	return 0;
}

static int get_sd3_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SDBick[2];

	return 0;
}

static int set_sd3_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	if ((currMode < 5) || (currMode == 7)) {
		if (ak4602->SDBick[2] != currMode) {
			ak4602->SDBick[2] = currMode;
			setSDClock(component, 2);
		}
	} else {
		akdbgprt(" [AK4602] %s Invalid Value selected!\n", __func__);
	}
	return 0;
}

static int get_sd4_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SDBick[3];

	return 0;
}

static int set_sd4_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	if ((currMode < 5) || (currMode == 7)) {
		if (ak4602->SDBick[3] != currMode) {
			ak4602->SDBick[3] = currMode;
			setSDClock(component, 3);
		}
	} else {
		akdbgprt(" [AK4602] %s Invalid Value selected!\n", __func__);
	}
	return 0;
}

static int get_sd5_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SDBick[4];

	return 0;
}

static int set_sd5_bick(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	if ((currMode < 5) || (currMode == 7)) {
		if (ak4602->SDBick[4] != currMode) {
			ak4602->SDBick[4] = currMode;
			setSDClock(component, 4);
		}
	} else {
		akdbgprt(" [AK4602] %s Invalid Value selected!\n", __func__);
	}
	return 0;
}

static int get_tdmsdin_mode1(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->tdmsdin[0];

	return 0;

}

static int set_tdmsdin_mode1(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	ak4602->tdmsdin[0] = currMode;
#ifdef TCC_EVB
	ak4602->is_updated = true;
#endif

	return 0;
}

static int get_tdmsdin_mode2(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->tdmsdin[1];

	return 0;

}

static int set_tdmsdin_mode2(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	ak4602->tdmsdin[1] = currMode;

	return 0;
}

static int get_tdmsdin_mode3(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->tdmsdin[2];

	return 0;

}

static int set_tdmsdin_mode3(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	ak4602->tdmsdin[2] = currMode;

	return 0;
}

static int get_tdmsdin_mode4(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->tdmsdin[3];

	return 0;

}

static int set_tdmsdin_mode4(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	ak4602->tdmsdin[3] = currMode;

	return 0;
}

static int get_tdmsdin_mode5(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->tdmsdin[4];

	return 0;

}

static int set_tdmsdin_mode5(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	ak4602->tdmsdin[4] = currMode;

	return 0;
}

static int get_tdmsdout_mode1(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->tdmsdout[0];

	return 0;

}

static int set_tdmsdout_mode1(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	ak4602->tdmsdout[0] = currMode;
#ifdef TCC_EVB
	ak4602->is_updated = true;
#endif

	return 0;
}

static int get_tdmsdout_mode2(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->tdmsdout[1];

	return 0;

}

static int set_tdmsdout_mode2(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	ak4602->tdmsdout[1] = currMode;

	return 0;
}

static int get_tdmsdout_mode3(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->tdmsdout[2];

	return 0;

}

static int set_tdmsdout_mode3(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	ak4602->tdmsdout[2] = currMode;

	return 0;
}

static int get_tdmsdout_mode4(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->tdmsdout[3];

	return 0;

}

static int set_tdmsdout_mode4(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	ak4602->tdmsdout[3] = currMode;

	return 0;
}

static int get_tdmsdout_mode5(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->tdmsdout[4];

	return 0;

}

static int set_tdmsdout_mode5(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	ak4602->tdmsdout[4] = currMode;

	return 0;
}

static int get_slotin_len1(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SlotLenIn[0];

	return 0;

}

static int set_slotin_len1(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	ak4602->SlotLenIn[0] = currMode;

	return 0;
}

static int get_slotin_len2(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SlotLenIn[1];

	return 0;

}

static int set_slotin_len2(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	ak4602->SlotLenIn[1] = currMode;

	return 0;
}

static int get_slotin_len3(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SlotLenIn[2];

	return 0;

}

static int set_slotin_len3(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	ak4602->SlotLenIn[2] = currMode;

	return 0;
}

static int get_slotin_len4(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SlotLenIn[3];

	return 0;

}

static int set_slotin_len4(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	ak4602->SlotLenIn[3] = currMode;

	return 0;
}

static int get_slotin_len5(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SlotLenIn[4];

	return 0;

}

static int set_slotin_len5(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	ak4602->SlotLenIn[4] = currMode;

	return 0;
}

static int get_slotout_len1(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SlotLenOut[0];

	return 0;

}

static int set_slotout_len1(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	ak4602->SlotLenOut[0] = currMode;

	return 0;
}

static int get_slotout_len2(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SlotLenOut[1];

	return 0;

}

static int set_slotout_len2(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	ak4602->SlotLenOut[1] = currMode;

	return 0;
}

static int get_slotout_len3(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SlotLenOut[2];

	return 0;

}

static int set_slotout_len3(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	ak4602->SlotLenOut[2] = currMode;

	return 0;
}

static int get_slotout_len4(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SlotLenOut[3];

	return 0;

}

static int set_slotout_len4(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	ak4602->SlotLenOut[3] = currMode;

	return 0;
}

static int get_slotout_len5(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->SlotLenOut[4];

	return 0;

}

static int set_slotout_len5(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	ak4602->SlotLenOut[4] = currMode;

	return 0;
}

static int get_i2s_mode2(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = ak4602->I2Smode2;

	return 0;

}

static int set_i2s_mode2(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int    currMode = ucontrol->value.enumerated.item[0];

	ak4602->I2Smode2 = currMode;

	return 0;
}


// ******************************* Control ************************************************

static DECLARE_TLV_DB_MINMAX(mgain_tlv, 0, 3600);

// ADC, ADC2 Digital Volume control:
// from -103.5 to 24 dB in 0.5 dB steps (mute instead of -103.5 dB)
static DECLARE_TLV_DB_SCALE(voladc_tlv, -10350, 50, 0);

// DAC Digital Volume control:
// from -115.5 to 12 dB in 0.5 dB steps (mute instead of -115.5 dB)
static DECLARE_TLV_DB_SCALE(voldac_tlv, -11550, 50, 0);

// Volume Block :  Volume control:
// from -115.5 to 12 dB in 0.5 dB steps (mute instead of -115.5 dB)
static DECLARE_TLV_DB_SCALE(vol_tlv, -11550, 50, 0);


static const char * const adcvl_texts[] = {
	"2.3V", "2.83V"
};

static const struct soc_enum ak4602_adcvl_enum[] = {
	SOC_ENUM_SINGLE(AK4602_80_MIC_AMP_GAIN_CONTROL, 7, ARRAY_SIZE(adcvl_texts), adcvl_texts),
	SOC_ENUM_SINGLE(AK4602_80_MIC_AMP_GAIN_CONTROL, 6, ARRAY_SIZE(adcvl_texts), adcvl_texts),
	SOC_ENUM_SINGLE(AK4602_80_MIC_AMP_GAIN_CONTROL, 5, ARRAY_SIZE(adcvl_texts), adcvl_texts),
	SOC_ENUM_SINGLE(AK4602_80_MIC_AMP_GAIN_CONTROL, 4, ARRAY_SIZE(adcvl_texts), adcvl_texts),
	SOC_ENUM_SINGLE(AK4602_80_MIC_AMP_GAIN_CONTROL, 3, ARRAY_SIZE(adcvl_texts), adcvl_texts),
};

static const char * const digfil_texts[] = {
	"Sharp Roll-Off", "Show Roll-Off",
	"Short Delay Sharp Roll-Off", "Short Delay Show Roll-Off",
};

static const char * const adcinput_texts[] = {
	"Differential", "Single"
};

static const struct soc_enum ak4602_adcset_enum[] = {
	SOC_ENUM_SINGLE(AK4602_86_ANALOG_INPUT_SELECT, 6, ARRAY_SIZE(digfil_texts), digfil_texts),
	SOC_ENUM_SINGLE(AK4602_86_ANALOG_INPUT_SELECT, 4, ARRAY_SIZE(adcinput_texts), adcinput_texts),
	SOC_ENUM_SINGLE(AK4602_86_ANALOG_INPUT_SELECT, 3, ARRAY_SIZE(adcinput_texts), adcinput_texts),
	SOC_ENUM_SINGLE(AK4602_86_ANALOG_INPUT_SELECT, 2, ARRAY_SIZE(adcinput_texts), adcinput_texts),
};

static const char * const dacvoltime_texts[] = {
	"4/fs", "16/fs"
};

static const char * const dacclock_texts[] = {
	"Fixed", "fs based"
};

static const char * const dacdem_texts[] = {
	"44.1kHz", "Off", "48kHz", "32kHz",
};


static const struct soc_enum ak4602_dacset_enum[] = {
	SOC_ENUM_SINGLE(AK4602_8E_DAC_MUTE_FILTER, 7, ARRAY_SIZE(dacvoltime_texts), dacvoltime_texts),
	SOC_ENUM_SINGLE(AK4602_8E_DAC_MUTE_FILTER, 2, ARRAY_SIZE(dacclock_texts), dacclock_texts),
	SOC_ENUM_SINGLE(AK4602_8E_DAC_MUTE_FILTER, 0, ARRAY_SIZE(digfil_texts), digfil_texts),
	SOC_ENUM_SINGLE(AK4602_8F_DAC_DEM, 4, ARRAY_SIZE(dacdem_texts), dacdem_texts),
	SOC_ENUM_SINGLE(AK4602_8F_DAC_DEM, 2, ARRAY_SIZE(dacdem_texts), dacdem_texts),
	SOC_ENUM_SINGLE(AK4602_8F_DAC_DEM, 0, ARRAY_SIZE(dacdem_texts), dacdem_texts),
};


static const char * const mixer_level_adjst_texts[] = {
	"0dB", "-6dB", "Mute"
};

static const char * const mixer_data_change_texts[] = {
	"Through", "Lin->LRout", "Rin->LRout", "Swap"
};

static const struct soc_enum ak4602_mixer_setting_enum[] = {  //'15/10/28 Modified
	SOC_ENUM_SINGLE(AK4602_7D_MIXER_A, 4, ARRAY_SIZE(mixer_level_adjst_texts), mixer_level_adjst_texts),
	SOC_ENUM_SINGLE(AK4602_7D_MIXER_A, 6, ARRAY_SIZE(mixer_level_adjst_texts), mixer_level_adjst_texts),
	SOC_ENUM_SINGLE(AK4602_7D_MIXER_A, 0, ARRAY_SIZE(mixer_data_change_texts), mixer_data_change_texts),
	SOC_ENUM_SINGLE(AK4602_7D_MIXER_A, 2, ARRAY_SIZE(mixer_data_change_texts), mixer_data_change_texts),
	SOC_ENUM_SINGLE(AK4602_7E_MIXER_B, 4, ARRAY_SIZE(mixer_level_adjst_texts), mixer_level_adjst_texts),
	SOC_ENUM_SINGLE(AK4602_7E_MIXER_B, 6, ARRAY_SIZE(mixer_level_adjst_texts), mixer_level_adjst_texts),
	SOC_ENUM_SINGLE(AK4602_7E_MIXER_B, 0, ARRAY_SIZE(mixer_data_change_texts), mixer_data_change_texts),
	SOC_ENUM_SINGLE(AK4602_7E_MIXER_B, 2, ARRAY_SIZE(mixer_data_change_texts), mixer_data_change_texts),
};

static const char * const voltime_texts[] = {
	"1/fs", "4/fs"
};

static const struct soc_enum ak4602_volset_enum[] = {
	SOC_ENUM_SINGLE(AK4602_9A_VOL_SETTING, 7, ARRAY_SIZE(voltime_texts), voltime_texts),
};

static const char * const do1sel_texts[] = {
	"SDOUT1", "DIT"
};

static const char * const do4sel_texts[] = {
	"STO", "SDOUT4"
};

static const struct soc_enum ak4602_dosel_enum[] = {
	SOC_ENUM_SINGLE(AK4602_7A_OUTPUT_PORT, 7, ARRAY_SIZE(do1sel_texts), do1sel_texts),
	SOC_ENUM_SINGLE(AK4602_7A_OUTPUT_PORT, 4, ARRAY_SIZE(do4sel_texts), do4sel_texts),
};

static const char * const tdm_texts[] = {
	"Stereo Mode", "TDM Mode",
};

static const char * const aif_slot_texts[] = {
	"24bit", "20bit", "16bit", "32bit"
};

static const struct soc_enum ak4602_tdm_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdm_texts), tdm_texts),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(aif_slot_texts), aif_slot_texts),
};

static const char * const i2smode_texts[] = {
	"Normal", "Invert",
};

static const struct soc_enum ak4602_i2smode_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(i2smode_texts), i2smode_texts),
};


static const char * const dit_v_texts[] = {
	"Valid", "Invalid",
};

static const char * const dit_ditdth_texts[] = {
	"24bit", "16bit",
};

static const char * const dit_cs4140_texts[] = {
	"Copy", "One Generation", "Not Used",  "No Copy"
};

static const char * const dit_cs3_texts[] = {
	"Off", "On"
};

static const char * const dit_cs2_texts[] = {
	"Protected", "Not Protected"
};

static const char * const dit_cs1_texts[] = {
	"Audio", "Digital"
};

static const char * const dit_chn_texts[] = {
	"Valid", "Invalid"
};

static const char * const dit_fs_texts[] = {
	"44.1kHz", "Off", "48kHz", "32kHz",
	"22.05kHz", "384kHz", "24kHz", "16kHz",
	"88.2kHz", "8kHz", "96kHz", "64kHz",
	"176.4kHz", "Off", "192kHz", "Off",
};

static const char * const dit_accuracy_texts[] = {
	"Normal",
	"High-precision",
	"Variable Pitch",
	"Not Specified"
};

static const char * const dit_maxword_texts[] = {
	"20bit", "24bit",
};

static const char * const dit_wordlen_texts[] = {
	"Not Indicated", "16bit/20bit", "18bit/22bit", "N/A",
	"19bit/23bit", "20bit/24bit", "17bit/21bit"
};

static const char * const dit_orgfs_texts[] = {
	"Not Indicated", "192kHz", "12kHz", "176.4kHz",
	"64kHz", "96kHz", "8kHz", "88.2kHz",
	"16kHz",  "24kHz", "11.025kHz", "22.05kHz",
	"32kHz", "48kHz", "128kHz", "44.1kHz"
};

static const struct soc_enum ak4602_dit_enum[] = {
	SOC_ENUM_SINGLE(AK4602_A2_DIT_STATUS_BIT_1, 7, ARRAY_SIZE(dit_v_texts), dit_v_texts),
	SOC_ENUM_SINGLE(AK4602_A2_DIT_STATUS_BIT_1, 6, ARRAY_SIZE(dit_ditdth_texts), dit_ditdth_texts),
	SOC_ENUM_SINGLE(AK4602_A2_DIT_STATUS_BIT_1, 4, ARRAY_SIZE(dit_cs4140_texts), dit_cs4140_texts),
	SOC_ENUM_SINGLE(AK4602_A2_DIT_STATUS_BIT_1, 3, ARRAY_SIZE(dit_cs3_texts), dit_cs3_texts),
	SOC_ENUM_SINGLE(AK4602_A2_DIT_STATUS_BIT_1, 2, ARRAY_SIZE(dit_cs2_texts), dit_cs2_texts),
	SOC_ENUM_SINGLE(AK4602_A2_DIT_STATUS_BIT_1, 1, ARRAY_SIZE(dit_cs1_texts), dit_cs1_texts),
	SOC_ENUM_SINGLE(AK4602_A2_DIT_STATUS_BIT_1, 0, ARRAY_SIZE(dit_chn_texts), dit_chn_texts),
	SOC_ENUM_SINGLE(AK4602_A4_DIT_STATUS_BIT_3, 0, ARRAY_SIZE(dit_fs_texts), dit_fs_texts),
	SOC_ENUM_SINGLE(AK4602_A4_DIT_STATUS_BIT_3, 4, ARRAY_SIZE(dit_accuracy_texts), dit_accuracy_texts),
	SOC_ENUM_SINGLE(AK4602_A5_DIT_STATUS_BIT_4, 0, ARRAY_SIZE(dit_maxword_texts), dit_maxword_texts),
	SOC_ENUM_SINGLE(AK4602_A5_DIT_STATUS_BIT_4, 1, ARRAY_SIZE(dit_wordlen_texts), dit_wordlen_texts),
	SOC_ENUM_SINGLE(AK4602_A5_DIT_STATUS_BIT_4, 4, ARRAY_SIZE(dit_orgfs_texts), dit_orgfs_texts),
};

static const char * const refmode_texts[] = {
	"256kHz", "384kHz", "512kHz", "768kHz",
	"1.024MHz", "1.152MHz", "1.536MHz", "2.038MHz",
	"2.304MHz", "3.072MHz", "4.096MHz", "4.608MHz",
	"6.144MHz", "8.192MHz", "9.216MHz", "12.288MHz",
	"18.432MHz", "24.576MHz"
};

static const char * const refsel_texts[] = {
	"XTI", "BICK1",  "BICK2",  "BICK3",  "BICK4",  "BICK5"
};

static const char * const fsmode_texts[] = {
	"8kHz:8kHz", "12kHz:12kHz", "16kHz:16kHz", "24kHz:24kHz",
	"32kHz:32kHz", "32kHz:16kHz", "32kHz:8kHz", "48kHz:48kHz",
	"48kHz:24kHz", "48kHz:16kHz", "48kHz:8kHz", "96kHz:96kHz",
	"96kHz:48kHz", "96kHz:32kHz", "96kHz:24kHz", "96kHz:16kHz",
	"96kHz:8kHz", "192kHz:192kHz", "192kHz:96kHz", "192kHz:48kHz",
	"192kHz:32kHz", "192kHz:16kHz"
};

static const char * const clko_texts[] = {
	"12.288MHz", "24.576MHz", "8.192MHz", "6.144MHz",
	"4.096MHz", "2.048MHz", "INT_MCLK1", "INT_MCLK2"
};

static const struct soc_enum ak4602_clockset_enum[] = {
	SOC_ENUM_SINGLE(AK4602_00_SYSTEM_CLOCK_1, 0, ARRAY_SIZE(refmode_texts), refmode_texts),
	SOC_ENUM_SINGLE(AK4602_01_SYSTEM_CLOCK_2, 0, ARRAY_SIZE(refsel_texts), refsel_texts),
	SOC_ENUM_SINGLE(AK4602_02_SYSTEM_CLOCK_3, 0, ARRAY_SIZE(fsmode_texts), fsmode_texts),
	SOC_ENUM_SINGLE(AK4602_15_CLKO_OUTPUT, 0, ARRAY_SIZE(clko_texts), clko_texts),
};


static const char * const sd_fs_texts[] = {
	"8kHz", "12kHz",  "16kHz", "24kHz",
	"32kHz", "48kHz", "96kHz", "192kHz"
};

static const char * const sd_bick_texts[] = {
	"64fs", "48fs", "32fs",  "128fs", "256fs", "N/A_5", "N/A_6", "512fs"
};

static const struct soc_enum ak4602_sd_fs_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sd_fs_texts), sd_fs_texts),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(sd_bick_texts), sd_bick_texts),
};

static const char * const msn_texts[] = {
	"Slave", "Master",
};

static const struct soc_enum ak4602_msnbit_enum[] = {
	SOC_ENUM_SINGLE(AK4602_04_CLOCK_SD1_SET_1, 7, ARRAY_SIZE(msn_texts), msn_texts),
	SOC_ENUM_SINGLE(AK4602_07_CLOCK_SD2_SET_1, 7, ARRAY_SIZE(msn_texts), msn_texts),
	SOC_ENUM_SINGLE(AK4602_0A_CLOCK_SD3_SET_1, 7, ARRAY_SIZE(msn_texts), msn_texts),
	SOC_ENUM_SINGLE(AK4602_0D_CLOCK_SD4_SET_1, 7, ARRAY_SIZE(msn_texts), msn_texts),
	SOC_ENUM_SINGLE(AK4602_10_CLOCK_SD5_SET_1, 7, ARRAY_SIZE(msn_texts), msn_texts),
};

static const char * const sdcks_texts[] = {
	"Low", "PLLMCLK", "XTI", "BICK1",  "BICK2",
	"BICK3", "BICK4", "BICK5"
};

static const struct soc_enum ak4602_sdcks_enum[] = {
	SOC_ENUM_SINGLE(AK4602_05_CLOCK_SD1_SET_2, 0, ARRAY_SIZE(sdcks_texts), sdcks_texts),
	SOC_ENUM_SINGLE(AK4602_08_CLOCK_SD2_SET_2, 0, ARRAY_SIZE(sdcks_texts), sdcks_texts),
	SOC_ENUM_SINGLE(AK4602_0B_CLOCK_SD3_SET_2, 0, ARRAY_SIZE(sdcks_texts), sdcks_texts),
	SOC_ENUM_SINGLE(AK4602_0E_CLOCK_SD4_SET_2, 0, ARRAY_SIZE(sdcks_texts), sdcks_texts),
	SOC_ENUM_SINGLE(AK4602_11_CLOCK_SD5_SET_2, 0, ARRAY_SIZE(sdcks_texts), sdcks_texts),
};

static const char * const exbck_texts[] = {
	"Low", "BICK1/LRCK1", "BICK2/LRCK2", "BICK3/LRCK3", "BICK4/LRCK4", "BICK5/LRCK5"
};

static const struct soc_enum ak4602_exbck_enum[] = {
	SOC_ENUM_SINGLE(AK4602_19_CLOCK_SD_SEL_3, 0, ARRAY_SIZE(exbck_texts), exbck_texts),
	SOC_ENUM_SINGLE(AK4602_1A_EXTCLOCK_1, 4, ARRAY_SIZE(exbck_texts), exbck_texts),
	SOC_ENUM_SINGLE(AK4602_1A_EXTCLOCK_1, 0, ARRAY_SIZE(exbck_texts), exbck_texts),
	SOC_ENUM_SINGLE(AK4602_1B_EXTCLOCK_2, 4, ARRAY_SIZE(exbck_texts), exbck_texts),
	SOC_ENUM_SINGLE(AK4602_1B_EXTCLOCK_2, 0, ARRAY_SIZE(exbck_texts), exbck_texts),
};

static const char * const sdsel_texts[] = {
	"Low", "SD1", "SD2", "SD3", "SD4", "SD5"
};

static const struct soc_enum ak4602_sdsel_enum[] = {
	SOC_ENUM_SINGLE(AK4602_17_CLOCK_SD_SEL_1, 4, ARRAY_SIZE(sdsel_texts), sdsel_texts),
	SOC_ENUM_SINGLE(AK4602_17_CLOCK_SD_SEL_1, 0, ARRAY_SIZE(sdsel_texts), sdsel_texts),
	SOC_ENUM_SINGLE(AK4602_18_CLOCK_SD_SEL_2, 4, ARRAY_SIZE(sdsel_texts), sdsel_texts),
	SOC_ENUM_SINGLE(AK4602_18_CLOCK_SD_SEL_2, 0, ARRAY_SIZE(sdsel_texts), sdsel_texts),
	SOC_ENUM_SINGLE(AK4602_19_CLOCK_SD_SEL_3, 4, ARRAY_SIZE(sdsel_texts), sdsel_texts),

	SOC_ENUM_SINGLE(AK4602_1F_CLOCK_SD_SEL_7, 4, ARRAY_SIZE(sdsel_texts), sdsel_texts),
	SOC_ENUM_SINGLE(AK4602_1F_CLOCK_SD_SEL_7, 0, ARRAY_SIZE(sdsel_texts), sdsel_texts),
	SOC_ENUM_SINGLE(AK4602_20_CLOCK_SD_SEL_8, 4, ARRAY_SIZE(sdsel_texts), sdsel_texts),
	SOC_ENUM_SINGLE(AK4602_20_CLOCK_SD_SEL_8, 0, ARRAY_SIZE(sdsel_texts), sdsel_texts),
	SOC_ENUM_SINGLE(AK4602_21_CLOCK_SD_SEL_9, 4, ARRAY_SIZE(sdsel_texts), sdsel_texts),

	SOC_ENUM_SINGLE(AK4602_21_CLOCK_SD_SEL_9, 0, ARRAY_SIZE(sdsel_texts), sdsel_texts),
	SOC_ENUM_SINGLE(AK4602_22_CLOCK_SD_SEL_10, 4, ARRAY_SIZE(sdsel_texts), sdsel_texts),
	SOC_ENUM_SINGLE(AK4602_22_CLOCK_SD_SEL_10, 0, ARRAY_SIZE(sdsel_texts), sdsel_texts),
	SOC_ENUM_SINGLE(AK4602_23_CLOCK_SD_SEL_11, 4, ARRAY_SIZE(sdsel_texts), sdsel_texts),
	SOC_ENUM_SINGLE(AK4602_23_CLOCK_SD_SEL_11, 0, ARRAY_SIZE(sdsel_texts), sdsel_texts),

	SOC_ENUM_SINGLE(AK4602_24_CLOCK_SD_SEL_12, 4, ARRAY_SIZE(sdsel_texts), sdsel_texts),
	SOC_ENUM_SINGLE(AK4602_24_CLOCK_SD_SEL_12, 0, ARRAY_SIZE(sdsel_texts), sdsel_texts),
	SOC_ENUM_SINGLE(AK4602_25_CLOCK_SD_SEL_13, 4, ARRAY_SIZE(sdsel_texts), sdsel_texts),
	SOC_ENUM_SINGLE(AK4602_25_CLOCK_SD_SEL_13, 0, ARRAY_SIZE(sdsel_texts), sdsel_texts),
	SOC_ENUM_SINGLE(AK4602_28_CLOCK_SD_SEL_16, 4, ARRAY_SIZE(sdsel_texts), sdsel_texts),

	SOC_ENUM_SINGLE(AK4602_28_CLOCK_SD_SEL_16, 0, ARRAY_SIZE(sdsel_texts), sdsel_texts),
	SOC_ENUM_SINGLE(AK4602_29_CLOCK_SD_SEL_17, 4, ARRAY_SIZE(sdsel_texts), sdsel_texts),
	SOC_ENUM_SINGLE(AK4602_29_CLOCK_SD_SEL_17, 0, ARRAY_SIZE(sdsel_texts), sdsel_texts),
};

static const char * const sto_texts[] = {
	"Error", "Normal",
};

static const char * const lock_texts[] = {
	"Unlock", "Lock",
};

static const struct soc_enum ak4602_status_enum[] = {
	SOC_ENUM_SINGLE(AK4602_102_STATUS_READ, 6, ARRAY_SIZE(sto_texts), sto_texts),
	SOC_ENUM_SINGLE(AK4602_103_SRC_STATUS_1, 4, ARRAY_SIZE(lock_texts), lock_texts),
	SOC_ENUM_SINGLE(AK4602_103_SRC_STATUS_1, 5, ARRAY_SIZE(lock_texts), lock_texts),
	SOC_ENUM_SINGLE(AK4602_103_SRC_STATUS_1, 6, ARRAY_SIZE(lock_texts), lock_texts),
	SOC_ENUM_SINGLE(AK4602_103_SRC_STATUS_1, 7, ARRAY_SIZE(lock_texts), lock_texts)
};


//**********************

#if 1//def AK4602_DEBUG

static const char * const test_reg_select[] = {
	"read Reg 00:3F",
	"read Reg 40:7F",
	"read Reg 80:A9",
	"read Reg 102:103",
};


static const struct soc_enum ak4602_test_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(test_reg_select), test_reg_select),
};

static int nTestRegNo = 0;

static int get_test_reg(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	/* Get the current output routing */
	ucontrol->value.enumerated.item[0] = nTestRegNo;

	return 0;
}

static int set_test_reg(
struct snd_kcontrol       *kcontrol,
struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	u32    currMode = ucontrol->value.enumerated.item[0];
	int    i;
	int	   regs, rege;
	unsigned int value;

	if (currMode > 3)
		return(-EINVAL);

	nTestRegNo = currMode;

	regs = 0x40 * currMode;
	rege = regs + 0x3F;
	if (currMode == 2) {
		rege = 0xA9;
	} else if (currMode == 3) {
		regs = 0x102;
		rege = 0x103;
	}

	for (i = regs; i <= rege; i++) {
		value = snd_soc_component_read(component, i);
		pr_err("***AK4602 Addr,Reg=(%03x, %02x)\n", i, value);
	}

	return 0;

}
#endif

static const struct snd_kcontrol_new ak4602_snd_controls[] = {

	SOC_SINGLE_TLV("AK4602 MIC Input Gain L", AK4602_7F_MIC_AMP_GAIN, 4, 0x0F, 0, mgain_tlv),
	SOC_SINGLE_TLV("AK4602 MIC Input Gain R", AK4602_7F_MIC_AMP_GAIN, 0, 0x0F, 0, mgain_tlv),

	SOC_SINGLE_TLV("AK4602 ADC1 Digital Volume L",	AK4602_81_ADC1_LCH_DIGITAL_VOL, 0, 0xFF, 1, voladc_tlv),
	SOC_SINGLE_TLV("AK4602 ADC1 Digital Volume R",	AK4602_82_ADC1_RCH_DIGITAL_VOL, 0, 0xFF, 1, voladc_tlv),
	SOC_SINGLE_TLV("AK4602 ADC2 Digital Volume L",	AK4602_83_ADC2_LCH_DIGITAL_VOL, 0, 0xFF, 1, voladc_tlv),
	SOC_SINGLE_TLV("AK4602 ADC2 Digital Volume R",	AK4602_84_ADC2_RCH_DIGITAL_VOL, 0, 0xFF, 1, voladc_tlv),
	SOC_SINGLE_TLV("AK4602 ADCM Digital Volume",	AK4602_85_ADCM_DIGITAL_VOL, 0, 0xFF, 1, voladc_tlv),

	SOC_SINGLE_TLV("AK4602 DAC1 Digital Volume L", AK4602_88_DAC1_LCH_DIGITAL_VOL, 0, 0xFF, 1, voldac_tlv),
	SOC_SINGLE_TLV("AK4602 DAC1 Digital Volume R", AK4602_89_DAC1_RCH_DIGITAL_VOL, 0, 0xFF, 1, voldac_tlv),
	SOC_SINGLE_TLV("AK4602 DAC2 Digital Volume L", AK4602_8A_DAC2_LCH_DIGITAL_VOL, 0, 0xFF, 1, voldac_tlv),
	SOC_SINGLE_TLV("AK4602 DAC2 Digital Volume R", AK4602_8B_DAC2_RCH_DIGITAL_VOL, 0, 0xFF, 1, voldac_tlv),
	SOC_SINGLE_TLV("AK4602 DAC3 Digital Volume L", AK4602_8C_DAC3_LCH_DIGITAL_VOL, 0, 0xFF, 1, voldac_tlv),
	SOC_SINGLE_TLV("AK4602 DAC3 Digital Volume R", AK4602_8D_DAC3_RCH_DIGITAL_VOL, 0, 0xFF, 1, voldac_tlv),

	SOC_SINGLE_TLV("AK4602 VOL1 Digital Volume L", AK4602_90_VOL1_LCH_DIGITAL_VOL, 0, 0xFF, 1, vol_tlv),
	SOC_SINGLE_TLV("AK4602 VOL1 Digital Volume R", AK4602_91_VOL1_RCH_DIGITAL_VOL, 0, 0xFF, 1, vol_tlv),
	SOC_SINGLE_TLV("AK4602 VOL2 Digital Volume L", AK4602_92_VOL2_LCH_DIGITAL_VOL, 0, 0xFF, 1, vol_tlv),
	SOC_SINGLE_TLV("AK4602 VOL2 Digital Volume R", AK4602_93_VOL2_RCH_DIGITAL_VOL, 0, 0xFF, 1, vol_tlv),
	SOC_SINGLE_TLV("AK4602 VOL3 Digital Volume L", AK4602_94_VOL3_LCH_DIGITAL_VOL, 0, 0xFF, 1, vol_tlv),
	SOC_SINGLE_TLV("AK4602 VOL3 Digital Volume R", AK4602_95_VOL3_RCH_DIGITAL_VOL, 0, 0xFF, 1, vol_tlv),
	SOC_SINGLE_TLV("AK4602 VOL4 Digital Volume L", AK4602_96_VOL4_LCH_DIGITAL_VOL, 0, 0xFF, 1, vol_tlv),
	SOC_SINGLE_TLV("AK4602 VOL4 Digital Volume R", AK4602_97_VOL4_RCH_DIGITAL_VOL, 0, 0xFF, 1, vol_tlv),
	SOC_SINGLE_TLV("AK4602 VOL5 Digital Volume L", AK4602_98_VOL5_LCH_DIGITAL_VOL, 0, 0xFF, 1, vol_tlv),
	SOC_SINGLE_TLV("AK4602 VOL5 Digital Volume R", AK4602_99_VOL5_RCH_DIGITAL_VOL, 0, 0xFF, 1, vol_tlv),

	SOC_SINGLE("AK4602 ADC1 Mute", AK4602_87_ADC_MUTE_HPF_CONTROL, 6, 1, 0),
	SOC_SINGLE("AK4602 ADC2 Mute", AK4602_87_ADC_MUTE_HPF_CONTROL, 5, 1, 0),
	SOC_SINGLE("AK4602 ADCM Mute", AK4602_87_ADC_MUTE_HPF_CONTROL, 4, 1, 0),

	SOC_SINGLE("AK4602 DAC1 Mute", AK4602_8E_DAC_MUTE_FILTER, 6, 1, 0),
	SOC_SINGLE("AK4602 DAC2 Mute", AK4602_8E_DAC_MUTE_FILTER, 5, 1, 0),
	SOC_SINGLE("AK4602 DAC3 Mute", AK4602_8E_DAC_MUTE_FILTER, 4, 1, 0),

	SOC_ENUM("AK4602 ADC1 Input Voltage L", ak4602_adcvl_enum[0]),
	SOC_ENUM("AK4602 ADC1 Input Voltage R", ak4602_adcvl_enum[1]),
	SOC_ENUM("AK4602 ADC2 Input Voltage L", ak4602_adcvl_enum[2]),
	SOC_ENUM("AK4602 ADC2 Input Voltage R", ak4602_adcvl_enum[3]),
	SOC_ENUM("AK4602 ADCM Input Voltage", ak4602_adcvl_enum[4]),

	SOC_SINGLE("AK4602 MIC Gain Zero Cross Enable L", AK4602_80_MIC_AMP_GAIN_CONTROL, 1, 1, 0),
	SOC_SINGLE("AK4602 MIC Gain Zero Cross Enable R", AK4602_80_MIC_AMP_GAIN_CONTROL, 0, 1, 0),

	SOC_ENUM("AK4602 ADC Digital Filter", ak4602_adcset_enum[0]),
	SOC_ENUM("AK4602 ADCM Input Select", ak4602_adcset_enum[1]),
	SOC_ENUM("AK4602 ADC1 Input Select L", ak4602_adcset_enum[2]),
	SOC_ENUM("AK4602 ADC1 Input Select R", ak4602_adcset_enum[3]),

	SOC_SINGLE("AK4602 ADC1 HPF Enable", AK4602_87_ADC_MUTE_HPF_CONTROL, 2, 1, 1),
	SOC_SINGLE("AK4602 ADC2 HPF Enable", AK4602_87_ADC_MUTE_HPF_CONTROL, 1, 1, 1),
	SOC_SINGLE("AK4602 ADCM HPF Enable", AK4602_87_ADC_MUTE_HPF_CONTROL, 0, 1, 1),

	SOC_ENUM("AK4602 DAC Volume Transition Time", ak4602_dacset_enum[0]),
	SOC_ENUM("AK4602 DAC Sampling Clock", ak4602_dacset_enum[1]),
	SOC_ENUM("AK4602 DAC Digital Filter", ak4602_dacset_enum[2]),
	SOC_ENUM("AK4602 DAC1 De-emphasis Filter", ak4602_dacset_enum[3]),
	SOC_ENUM("AK4602 DAC2 De-emphasis Filter", ak4602_dacset_enum[4]),
	SOC_ENUM("AK4602 DAC3 De-emphasis Filter", ak4602_dacset_enum[5]),

	SOC_ENUM("AK4602 MixerA Input1 Level Adjust", ak4602_mixer_setting_enum[0]),
	SOC_ENUM("AK4602 MixerA Input2 Level Adjust", ak4602_mixer_setting_enum[1]),
	SOC_ENUM("AK4602 MixerA Input1 Data Change", ak4602_mixer_setting_enum[2]),
	SOC_ENUM("AK4602 MixerA Input2 Data Change", ak4602_mixer_setting_enum[3]),

	SOC_ENUM("AK4602 MixerB Input1 Level Adjust", ak4602_mixer_setting_enum[4]),
	SOC_ENUM("AK4602 MixerB Input2 Level Adjust", ak4602_mixer_setting_enum[5]),
	SOC_ENUM("AK4602 MixerB Input1 Data Change", ak4602_mixer_setting_enum[6]),
	SOC_ENUM("AK4602 MixerB Input2 Data Change", ak4602_mixer_setting_enum[7]),

	SOC_ENUM("AK4602 VOL Volume Transition Time", ak4602_volset_enum[0]),

	SOC_SINGLE("AK4602 SRC1-4 Echo Cancel Filter Enable", AK4602_9B_SRC_FILTER, 5, 1, 0),
	SOC_SINGLE("AK4602 SRC1-4 Audio Filter Enable",  AK4602_9B_SRC_FILTER, 4, 1, 0),

	SOC_SINGLE("AK4602 SRC1 Group Delay Matching", AK4602_9C_SRC_PHASE_GROUP_1, 6, 2, 0),
	SOC_SINGLE("AK4602 SRC2 Group Delay Matching", AK4602_9C_SRC_PHASE_GROUP_1, 4, 2, 0),
	SOC_SINGLE("AK4602 SRC3 Group Delay Matching", AK4602_9C_SRC_PHASE_GROUP_1, 2, 2, 0),
	SOC_SINGLE("AK4602 SRC4 Group Delay Matching", AK4602_9C_SRC_PHASE_GROUP_1, 0, 2, 0),

	SOC_SINGLE("AK4602 SRC1 Soft Mute", AK4602_9E_SRC_MUTE_SETTING1, 7, 1, 0),
	SOC_SINGLE("AK4602 SRC2 Soft Mute", AK4602_9E_SRC_MUTE_SETTING1, 6, 1, 0),
	SOC_SINGLE("AK4602 SRC3 Soft Mute", AK4602_9E_SRC_MUTE_SETTING1, 5, 1, 0),
	SOC_SINGLE("AK4602 SRC4 Soft Mute", AK4602_9E_SRC_MUTE_SETTING1, 4, 1, 0),

	SOC_SINGLE("AK4602 SRC1 Soft Mute Auto Mode Enable", AK4602_9F_SRC_MUTE_SETTING2, 7, 1, 0),
	SOC_SINGLE("AK4602 SRC2 Soft Mute Auto Mode Enable", AK4602_9F_SRC_MUTE_SETTING2, 6, 1, 0),
	SOC_SINGLE("AK4602 SRC3 Soft Mute Auto Mode Enable", AK4602_9F_SRC_MUTE_SETTING2, 5, 1, 0),
	SOC_SINGLE("AK4602 SRC4 Soft Mute Auto Mode Enable", AK4602_9F_SRC_MUTE_SETTING2, 4, 1, 0),

	SOC_SINGLE("AK4602 STO SRC1 LOCK Enable", AK4602_A0_STO_FLAG_1, 7, 1, 0),
	SOC_SINGLE("AK4602 STO SRC2 LOCK Enable", AK4602_A0_STO_FLAG_1, 6, 1, 0),
	SOC_SINGLE("AK4602 STO SRC3 LOCK Enable", AK4602_A0_STO_FLAG_1, 5, 1, 0),
	SOC_SINGLE("AK4602 STO SRC4 LOCK Enable", AK4602_A0_STO_FLAG_1, 4, 1, 0),

	SOC_SINGLE("AK4602 STO PLL LOCK Enable", AK4602_A1_STO_FLAG_2, 6, 1, 0),

	SOC_ENUM("AK4602 SDOUT1/DIT pin Setting", ak4602_dosel_enum[0]),
	SOC_ENUM("AK4602 STO/SDOUT4 pin Setting", ak4602_dosel_enum[1]),

	SOC_ENUM_EXT("AK4602 SDIN1 TDM mode", ak4602_tdm_enum[0], get_tdmsdin_mode1, set_tdmsdin_mode1),
	SOC_ENUM_EXT("AK4602 SDIN2 TDM mode", ak4602_tdm_enum[0], get_tdmsdin_mode2, set_tdmsdin_mode2),
	SOC_ENUM_EXT("AK4602 SDIN3 TDM mode", ak4602_tdm_enum[0], get_tdmsdin_mode3, set_tdmsdin_mode3),
	SOC_ENUM_EXT("AK4602 SDIN4 TDM mode", ak4602_tdm_enum[0], get_tdmsdin_mode4, set_tdmsdin_mode4),
	SOC_ENUM_EXT("AK4602 SDIN5 TDM mode", ak4602_tdm_enum[0], get_tdmsdin_mode5, set_tdmsdin_mode5),

	SOC_ENUM_EXT("AK4602 SDOUT1 TDM mode", ak4602_tdm_enum[0], get_tdmsdout_mode1, set_tdmsdout_mode1),
	SOC_ENUM_EXT("AK4602 SDOUT2 TDM mode", ak4602_tdm_enum[0], get_tdmsdout_mode2, set_tdmsdout_mode2),
	SOC_ENUM_EXT("AK4602 SDOUT3 TDM mode", ak4602_tdm_enum[0], get_tdmsdout_mode3, set_tdmsdout_mode3),
	SOC_ENUM_EXT("AK4602 SDOUT4 TDM mode", ak4602_tdm_enum[0], get_tdmsdout_mode4, set_tdmsdout_mode4),
	SOC_ENUM_EXT("AK4602 SDOUT5 TDM mode", ak4602_tdm_enum[0], get_tdmsdout_mode5, set_tdmsdout_mode5),

	SOC_ENUM_EXT("AK4602 SDIN1 Slot Length", ak4602_tdm_enum[1], get_slotin_len1, set_slotin_len1),
	SOC_ENUM_EXT("AK4602 SDIN2 Slot Length", ak4602_tdm_enum[1], get_slotin_len2, set_slotin_len2),
	SOC_ENUM_EXT("AK4602 SDIN3 Slot Length", ak4602_tdm_enum[1], get_slotin_len3, set_slotin_len3),
	SOC_ENUM_EXT("AK4602 SDIN4 Slot Length", ak4602_tdm_enum[1], get_slotin_len4, set_slotin_len4),
	SOC_ENUM_EXT("AK4602 SDIN5 Slot Length", ak4602_tdm_enum[1], get_slotin_len5, set_slotin_len5),

	SOC_ENUM_EXT("AK4602 SDOUT1 Slot Length", ak4602_tdm_enum[1], get_slotout_len1, set_slotout_len1),
	SOC_ENUM_EXT("AK4602 SDOUT2 Slot Length", ak4602_tdm_enum[1], get_slotout_len2, set_slotout_len2),
	SOC_ENUM_EXT("AK4602 SDOUT3 Slot Length", ak4602_tdm_enum[1], get_slotout_len3, set_slotout_len3),
	SOC_ENUM_EXT("AK4602 SDOUT4 Slot Length", ak4602_tdm_enum[1], get_slotout_len4, set_slotout_len4),
	SOC_ENUM_EXT("AK4602 SDOUT5 Slot Length", ak4602_tdm_enum[1], get_slotout_len5, set_slotout_len5),

	SOC_ENUM_EXT("AK4602 Port2 I2S Mode", ak4602_i2smode_enum[0], get_i2s_mode2, set_i2s_mode2),

	SOC_ENUM("AK4602 DIT Validity Flag", ak4602_dit_enum[0]),
	SOC_ENUM("AK4602 DIT Dither", ak4602_dit_enum[1]),
	SOC_ENUM("AK4602 DIT CGMS-A", ak4602_dit_enum[2]),
	SOC_ENUM("AK4602 DIT Pre-emphasis", ak4602_dit_enum[3]),
	SOC_ENUM("AK4602 DIT Copyright", ak4602_dit_enum[4]),
	SOC_ENUM("AK4602 DIT Output Data", ak4602_dit_enum[5]),
	SOC_ENUM("AK4602 DIT Channel Number", ak4602_dit_enum[6]),

	SOC_SINGLE("AK4602 DIT Category Code(CS8-CS15)", AK4602_A3_DIT_STATUS_BIT_2, 0, 255, 0),

	SOC_ENUM("AK4602 DIT Sampling Frequency", ak4602_dit_enum[7]),
	SOC_ENUM("AK4602 DIT Clock Accuracy", ak4602_dit_enum[8]),
	SOC_ENUM("AK4602 DIT Max Word Length", ak4602_dit_enum[9]),
	SOC_ENUM("AK4602 DIT Word Length", ak4602_dit_enum[10]),
	SOC_ENUM("AK4602 DIT Original Sampling Frequency", ak4602_dit_enum[11]),

	SOC_ENUM("AK4602 PLL Reference Clock Frequency", ak4602_clockset_enum[0]),
	SOC_ENUM("AK4602 PLL Reference Clock Select", ak4602_clockset_enum[1]),
	SOC_ENUM("AK4602 CODEC Sampling Frequency", ak4602_clockset_enum[2]),

	SOC_SINGLE("AK4602 BICK1/LRCK1 Pulled Down Enable", AK4602_04_CLOCK_SD1_SET_1, 3, 1, 1),
	SOC_SINGLE("AK4602 BICK2/LRCK2 Pulled Down Enable", AK4602_07_CLOCK_SD2_SET_1, 3, 1, 1),
	SOC_SINGLE("AK4602 BICK3/LRCK3 Pulled Down Enable", AK4602_0A_CLOCK_SD3_SET_1, 3, 1, 1),
	SOC_SINGLE("AK4602 BICK4/LRCK4 Pulled Down Enable", AK4602_0D_CLOCK_SD4_SET_1, 3, 1, 1),
	SOC_SINGLE("AK4602 BICK5/LRCK5 Pulled Down Enable", AK4602_10_CLOCK_SD5_SET_1, 3, 1, 1),

	SOC_SINGLE("AK4602 SDOUT1 High Speed Mode Enable", AK4602_77_SDOUT_PHASE, 0, 1, 0),
	SOC_SINGLE("AK4602 SDOUT2 High Speed Mode Enable", AK4602_77_SDOUT_PHASE, 1, 1, 0),
	SOC_SINGLE("AK4602 SDOUT3 High Speed Mode Enable", AK4602_77_SDOUT_PHASE, 2, 1, 0),
	SOC_SINGLE("AK4602 SDOUT4 High Speed Mode Enable", AK4602_77_SDOUT_PHASE, 3, 1, 0),
	SOC_SINGLE("AK4602 SDOUT5 High Speed Mode Enable", AK4602_77_SDOUT_PHASE, 4, 1, 0),

	SOC_SINGLE("AK4602 CLKO Output Enable", AK4602_15_CLKO_OUTPUT, 3, 1, 0),
	SOC_ENUM("AK4602 CLKO Output Frequency", ak4602_clockset_enum[3]),

	SOC_ENUM_EXT("AK4602 BICK1/LRCK1 Master", ak4602_msnbit_enum[0], get_sd1_ms, set_sd1_ms),
	SOC_ENUM_EXT("AK4602 BICK2/LRCK2 Master", ak4602_msnbit_enum[1], get_sd2_ms, set_sd2_ms),
	SOC_ENUM_EXT("AK4602 BICK3/LRCK3 Master", ak4602_msnbit_enum[2], get_sd3_ms, set_sd3_ms),
	SOC_ENUM_EXT("AK4602 BICK4/LRCK4 Master", ak4602_msnbit_enum[3], get_sd4_ms, set_sd4_ms),
	SOC_ENUM_EXT("AK4602 BICK5/LRCK5 Master", ak4602_msnbit_enum[4], get_sd5_ms, set_sd5_ms),

	SOC_ENUM_EXT("AK4602 Sync Domain 1 Clock Source", ak4602_sdcks_enum[0], get_sd1_cks, set_sd1_cks),
	SOC_ENUM_EXT("AK4602 Sync Domain 2 Clock Source", ak4602_sdcks_enum[1], get_sd2_cks, set_sd2_cks),
	SOC_ENUM_EXT("AK4602 Sync Domain 3 Clock Source", ak4602_sdcks_enum[2], get_sd3_cks, set_sd3_cks),
	SOC_ENUM_EXT("AK4602 Sync Domain 4 Clock Source", ak4602_sdcks_enum[3], get_sd4_cks, set_sd4_cks),
	SOC_ENUM_EXT("AK4602 Sync Domain 5 Clock Source", ak4602_sdcks_enum[4], get_sd5_cks, set_sd5_cks),

	SOC_ENUM_EXT("AK4602 Sync Domain 1 fs", ak4602_sd_fs_enum[0], get_sd1_fs, set_sd1_fs),
	SOC_ENUM_EXT("AK4602 Sync Domain 2 fs", ak4602_sd_fs_enum[0], get_sd2_fs, set_sd2_fs),
	SOC_ENUM_EXT("AK4602 Sync Domain 3 fs", ak4602_sd_fs_enum[0], get_sd3_fs, set_sd3_fs),
	SOC_ENUM_EXT("AK4602 Sync Domain 4 fs", ak4602_sd_fs_enum[0], get_sd4_fs, set_sd4_fs),
	SOC_ENUM_EXT("AK4602 Sync Domain 5 fs", ak4602_sd_fs_enum[0], get_sd5_fs, set_sd5_fs),

	SOC_ENUM_EXT("AK4602 Sync Domain 1 BICK fs", ak4602_sd_fs_enum[1], get_sd1_bick, set_sd1_bick),
	SOC_ENUM_EXT("AK4602 Sync Domain 2 BICK fs", ak4602_sd_fs_enum[1], get_sd2_bick, set_sd2_bick),
	SOC_ENUM_EXT("AK4602 Sync Domain 3 BICK fs", ak4602_sd_fs_enum[1], get_sd3_bick, set_sd3_bick),
	SOC_ENUM_EXT("AK4602 Sync Domain 4 BICK fs", ak4602_sd_fs_enum[1], get_sd4_bick, set_sd4_bick),
	SOC_ENUM_EXT("AK4602 Sync Domain 5 BICK fs", ak4602_sd_fs_enum[1], get_sd5_bick, set_sd5_bick),

	SOC_ENUM("AK4602 SDIN1 BICK/LRCK", ak4602_exbck_enum[0]),
	SOC_ENUM("AK4602 SDIN2 BICK/LRCK", ak4602_exbck_enum[1]),
	SOC_ENUM("AK4602 SDIN3 BICK/LRCK", ak4602_exbck_enum[2]),
	SOC_ENUM("AK4602 SDIN4 BICK/LRCK", ak4602_exbck_enum[3]),
	SOC_ENUM("AK4602 SDIN5 BICK/LRCK", ak4602_exbck_enum[4]),

	SOC_ENUM("AK4602 BICK1 Sync Domain", ak4602_sdsel_enum[0]),
	SOC_ENUM("AK4602 BICK2 Sync Domain", ak4602_sdsel_enum[1]),
	SOC_ENUM("AK4602 BICK3 Sync Domain", ak4602_sdsel_enum[2]),
	SOC_ENUM("AK4602 BICK4 Sync Domain", ak4602_sdsel_enum[3]),
	SOC_ENUM("AK4602 BICK5 Sync Domain", ak4602_sdsel_enum[4]),

	SOC_ENUM("AK4602 SDOUT1 Sync Domain", ak4602_sdsel_enum[5]),
	SOC_ENUM("AK4602 SDOUT2 Sync Domain", ak4602_sdsel_enum[6]),
	SOC_ENUM("AK4602 SDOUT3 Sync Domain", ak4602_sdsel_enum[7]),
	SOC_ENUM("AK4602 SDOUT4 Sync Domain", ak4602_sdsel_enum[8]),
	SOC_ENUM("AK4602 SDOUT5 Sync Domain", ak4602_sdsel_enum[9]),

	SOC_ENUM("AK4602 VOL1 Sync Domain", ak4602_sdsel_enum[10]),
	SOC_ENUM("AK4602 VOL2 Sync Domain", ak4602_sdsel_enum[11]),
	SOC_ENUM("AK4602 VOL3 Sync Domain", ak4602_sdsel_enum[12]),
	SOC_ENUM("AK4602 VOL4 Sync Domain", ak4602_sdsel_enum[13]),
	SOC_ENUM("AK4602 VOL5 Sync Domain", ak4602_sdsel_enum[14]),

	SOC_ENUM("AK4602 SRCO1 Sync Domain", ak4602_sdsel_enum[15]),
	SOC_ENUM("AK4602 SRCO2 Sync Domain", ak4602_sdsel_enum[16]),
	SOC_ENUM("AK4602 SRCO3 Sync Domain", ak4602_sdsel_enum[17]),
	SOC_ENUM("AK4602 SRCO4 Sync Domain", ak4602_sdsel_enum[18]),

	SOC_ENUM("AK4602 MixerA Sync Domain", ak4602_sdsel_enum[19]),
	SOC_ENUM("AK4602 MixerB Sync Domain", ak4602_sdsel_enum[20]),

	SOC_ENUM("AK4602 ADC1 Sync Domain", ak4602_sdsel_enum[21]),
	SOC_ENUM("AK4602 CODEC Sync Domain", ak4602_sdsel_enum[22]),

	SOC_ENUM("AK4602 STO pin Status", ak4602_status_enum[0]),

	SOC_ENUM("AK4602 SRC1 Lock Status", ak4602_status_enum[1]),
	SOC_ENUM("AK4602 SRC2 Lock Status", ak4602_status_enum[2]),
	SOC_ENUM("AK4602 SRC3 Lock Status", ak4602_status_enum[3]),
	SOC_ENUM("AK4602 SRC4 Lock Status", ak4602_status_enum[4]),

#if 1//def AK4602_DEBUG
	SOC_ENUM_EXT("AK4602 Reg Read", ak4602_test_enum[0], get_test_reg, set_test_reg),
#endif

};

static const char * const ak4602_adc2_select_texts[] = {
	"AIN2", "AIN3", "AIN4", "AIN5"
};

static const struct soc_enum ak4602_adc2_mux_enum =
	SOC_ENUM_SINGLE(AK4602_86_ANALOG_INPUT_SELECT, 0,
			ARRAY_SIZE(ak4602_adc2_select_texts), ak4602_adc2_select_texts);

static const struct snd_kcontrol_new ak4602_adc2_mux_control =
	SOC_DAPM_ENUM("ADC2 Select", ak4602_adc2_mux_enum);

static const char * const ak4602_micbias_select_texts[] = {
	"LineIn", "MicBias"
};

static SOC_ENUM_SINGLE_VIRT_DECL(ak4602_micbias1_mux_enum, ak4602_micbias_select_texts);

static const struct snd_kcontrol_new ak4602_micbias1_mux_control =
	SOC_DAPM_ENUM("MicBias1 Select", ak4602_micbias1_mux_enum);


static SOC_ENUM_SINGLE_VIRT_DECL(ak4602_micbias2_mux_enum, ak4602_micbias_select_texts);

static const struct snd_kcontrol_new ak4602_micbias2_mux_control =
	SOC_DAPM_ENUM("MicBias2 Select", ak4602_micbias2_mux_enum);

static const char * const ak4602_source_select_texts[] = {
	"ZERO",
	"SDIN1A", "SDIN1B", "SDIN1C", "SDIN1D", "SDIN1E", "SDIN1F", "SDIN1G", "SDIN1H",
	"SDIN2A", "SDIN2B", "SDIN2C", "SDIN2D", "SDIN2E", "SDIN2F", "SDIN2G", "SDIN2H",
	"SDIN3A", "SDIN3B", "SDIN3C", "SDIN3D", "SDIN3E", "SDIN3F", "SDIN3G", "SDIN3H",
	"SDIN4A", "SDIN4B", "SDIN4C", "SDIN4D", "SDIN4E", "SDIN4F", "SDIN4G", "SDIN4H",
	"SDIN5A", "SDIN5B", "SDIN5C", "SDIN5D", "SDIN5E", "SDIN5F", "SDIN5G", "SDIN5H",
	"VOLO1",  "VOLO2", "VOLO3", "VOLO4", "VOLO5", "ADC1", "ADC2", "ADCM",
	"MixerA", "MixerB", "SRCO1", "SRCO2", "SRCO3", "SRCO4"
};

static const struct soc_enum ak4602_sout1a_mux_enum =
	SOC_ENUM_SINGLE(AK4602_2A_SDOUT1_TDM_SLOT1_2, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout1a_mux_control =
	SOC_DAPM_ENUM("SOUT1A Select", ak4602_sout1a_mux_enum);

static const struct soc_enum ak4602_sout1b_mux_enum =
	SOC_ENUM_SINGLE(AK4602_2B_SDOUT1_TDM_SLOT3_4, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout1b_mux_control =
	SOC_DAPM_ENUM("SOUT1B Select", ak4602_sout1b_mux_enum);

static const struct soc_enum ak4602_sout1c_mux_enum =
	SOC_ENUM_SINGLE(AK4602_2C_SDOUT1_TDM_SLOT5_6, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout1c_mux_control =
	SOC_DAPM_ENUM("SOUT1C Select", ak4602_sout1c_mux_enum);

static const struct soc_enum ak4602_sout1d_mux_enum =
	SOC_ENUM_SINGLE(AK4602_2D_SDOUT1_TDM_SLOT7_8, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout1d_mux_control =
	SOC_DAPM_ENUM("SOUT1D Select", ak4602_sout1d_mux_enum);


static const struct soc_enum ak4602_sout1e_mux_enum =
	SOC_ENUM_SINGLE(AK4602_2E_SDOUT1_TDM_SLOT9_10, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout1e_mux_control =
	SOC_DAPM_ENUM("SOUT1E Select", ak4602_sout1e_mux_enum);

static const struct soc_enum ak4602_sout1f_mux_enum =
	SOC_ENUM_SINGLE(AK4602_2F_SDOUT1_TDM_SLOT11_12, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout1f_mux_control =
	SOC_DAPM_ENUM("SOUT1F Select", ak4602_sout1f_mux_enum);

static const struct soc_enum ak4602_sout1g_mux_enum =
	SOC_ENUM_SINGLE(AK4602_30_SDOUT1_TDM_SLOT13_14, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout1g_mux_control =
	SOC_DAPM_ENUM("SOUT1G Select", ak4602_sout1g_mux_enum);

static const struct soc_enum ak4602_sout1h_mux_enum =
	SOC_ENUM_SINGLE(AK4602_31_SDOUT1_TDM_SLOT15_16, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout1h_mux_control =
	SOC_DAPM_ENUM("SOUT1H Select", ak4602_sout1h_mux_enum);

static const struct soc_enum ak4602_sout2a_mux_enum =
	SOC_ENUM_SINGLE(AK4602_32_SDOUT2_TDM_SLOT1_2, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout2a_mux_control =
	SOC_DAPM_ENUM("SOUT2A Select", ak4602_sout2a_mux_enum);

static const struct soc_enum ak4602_sout2b_mux_enum =
	SOC_ENUM_SINGLE(AK4602_33_SDOUT2_TDM_SLOT3_4, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout2b_mux_control =
	SOC_DAPM_ENUM("SOUT2B Select", ak4602_sout2b_mux_enum);

static const struct soc_enum ak4602_sout2c_mux_enum =
	SOC_ENUM_SINGLE(AK4602_34_SDOUT2_TDM_SLOT5_6, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout2c_mux_control =
	SOC_DAPM_ENUM("SOUT2C Select", ak4602_sout2c_mux_enum);

static const struct soc_enum ak4602_sout2d_mux_enum =
	SOC_ENUM_SINGLE(AK4602_35_SDOUT2_TDM_SLOT7_8, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout2d_mux_control =
	SOC_DAPM_ENUM("SOUT2D Select", ak4602_sout2d_mux_enum);


static const struct soc_enum ak4602_sout2e_mux_enum =
	SOC_ENUM_SINGLE(AK4602_36_SDOUT2_TDM_SLOT9_10, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout2e_mux_control =
	SOC_DAPM_ENUM("SOUT2E Select", ak4602_sout2e_mux_enum);

static const struct soc_enum ak4602_sout2f_mux_enum =
	SOC_ENUM_SINGLE(AK4602_37_SDOUT2_TDM_SLOT11_12, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout2f_mux_control =
	SOC_DAPM_ENUM("SOUT2F Select", ak4602_sout2f_mux_enum);

static const struct soc_enum ak4602_sout2g_mux_enum =
	SOC_ENUM_SINGLE(AK4602_38_SDOUT2_TDM_SLOT13_14, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout2g_mux_control =
	SOC_DAPM_ENUM("SOUT2G Select", ak4602_sout2g_mux_enum);

static const struct soc_enum ak4602_sout2h_mux_enum =
	SOC_ENUM_SINGLE(AK4602_39_SDOUT2_TDM_SLOT15_16, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout2h_mux_control =
	SOC_DAPM_ENUM("SOUT2H Select", ak4602_sout2h_mux_enum);

static const struct soc_enum ak4602_sout3a_mux_enum =
	SOC_ENUM_SINGLE(AK4602_3A_SDOUT3_TDM_SLOT1_2, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout3a_mux_control =
	SOC_DAPM_ENUM("SOUT3A Select", ak4602_sout3a_mux_enum);

static const struct soc_enum ak4602_sout3b_mux_enum =
	SOC_ENUM_SINGLE(AK4602_3B_SDOUT3_TDM_SLOT3_4, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout3b_mux_control =
	SOC_DAPM_ENUM("SOUT3B Select", ak4602_sout3b_mux_enum);

static const struct soc_enum ak4602_sout3c_mux_enum =
	SOC_ENUM_SINGLE(AK4602_3C_SDOUT3_TDM_SLOT5_6, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout3c_mux_control =
	SOC_DAPM_ENUM("SOUT3C Select", ak4602_sout3c_mux_enum);

static const struct soc_enum ak4602_sout3d_mux_enum =
	SOC_ENUM_SINGLE(AK4602_3D_SDOUT3_TDM_SLOT7_8, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout3d_mux_control =
	SOC_DAPM_ENUM("SOUT3D Select", ak4602_sout3d_mux_enum);


static const struct soc_enum ak4602_sout3e_mux_enum =
	SOC_ENUM_SINGLE(AK4602_3E_SDOUT3_TDM_SLOT9_10, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout3e_mux_control =
	SOC_DAPM_ENUM("SOUT3E Select", ak4602_sout3e_mux_enum);

static const struct soc_enum ak4602_sout3f_mux_enum =
	SOC_ENUM_SINGLE(AK4602_3F_SDOUT3_TDM_SLOT11_12, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout3f_mux_control =
	SOC_DAPM_ENUM("SOUT3F Select", ak4602_sout3f_mux_enum);

static const struct soc_enum ak4602_sout3g_mux_enum =
	SOC_ENUM_SINGLE(AK4602_40_SDOUT3_TDM_SLOT13_14, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout3g_mux_control =
	SOC_DAPM_ENUM("SOUT3G Select", ak4602_sout3g_mux_enum);

static const struct soc_enum ak4602_sout3h_mux_enum =
	SOC_ENUM_SINGLE(AK4602_41_SDOUT3_TDM_SLOT15_16, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout3h_mux_control =
	SOC_DAPM_ENUM("SOUT3H Select", ak4602_sout3h_mux_enum);

static const struct soc_enum ak4602_sout4a_mux_enum =
	SOC_ENUM_SINGLE(AK4602_42_SDOUT4_TDM_SLOT1_2, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout4a_mux_control =
	SOC_DAPM_ENUM("SOUT4A Select", ak4602_sout4a_mux_enum);

static const struct soc_enum ak4602_sout4b_mux_enum =
	SOC_ENUM_SINGLE(AK4602_43_SDOUT4_TDM_SLOT3_4, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout4b_mux_control =
	SOC_DAPM_ENUM("SOUT4B Select", ak4602_sout4b_mux_enum);

static const struct soc_enum ak4602_sout4c_mux_enum =
	SOC_ENUM_SINGLE(AK4602_44_SDOUT4_TDM_SLOT5_6, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout4c_mux_control =
	SOC_DAPM_ENUM("SOUT4C Select", ak4602_sout4c_mux_enum);

static const struct soc_enum ak4602_sout4d_mux_enum =
	SOC_ENUM_SINGLE(AK4602_45_SDOUT4_TDM_SLOT7_8, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout4d_mux_control =
	SOC_DAPM_ENUM("SOUT4D Select", ak4602_sout4d_mux_enum);

static const struct soc_enum ak4602_sout4e_mux_enum =
	SOC_ENUM_SINGLE(AK4602_46_SDOUT4_TDM_SLOT9_10, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout4e_mux_control =
	SOC_DAPM_ENUM("SOUT4E Select", ak4602_sout4e_mux_enum);

static const struct soc_enum ak4602_sout4f_mux_enum =
	SOC_ENUM_SINGLE(AK4602_47_SDOUT4_TDM_SLOT11_12, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout4f_mux_control =
	SOC_DAPM_ENUM("SOUT4F Select", ak4602_sout4f_mux_enum);

static const struct soc_enum ak4602_sout4g_mux_enum =
	SOC_ENUM_SINGLE(AK4602_48_SDOUT4_TDM_SLOT13_14, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout4g_mux_control =
	SOC_DAPM_ENUM("SOUT4G Select", ak4602_sout4g_mux_enum);

static const struct soc_enum ak4602_sout4h_mux_enum =
	SOC_ENUM_SINGLE(AK4602_49_SDOUT4_TDM_SLOT15_16, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout4h_mux_control =
	SOC_DAPM_ENUM("SOUT4H Select", ak4602_sout4h_mux_enum);

static const struct soc_enum ak4602_sout5a_mux_enum =
	SOC_ENUM_SINGLE(AK4602_4A_SDOUT5_TDM_SLOT1_2, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout5a_mux_control =
	SOC_DAPM_ENUM("SOUT5A Select", ak4602_sout5a_mux_enum);

static const struct soc_enum ak4602_sout5b_mux_enum =
	SOC_ENUM_SINGLE(AK4602_4B_SDOUT5_TDM_SLOT3_4, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout5b_mux_control =
	SOC_DAPM_ENUM("SOUT5B Select", ak4602_sout5b_mux_enum);

static const struct soc_enum ak4602_sout5c_mux_enum =
	SOC_ENUM_SINGLE(AK4602_4C_SDOUT5_TDM_SLOT5_6, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout5c_mux_control =
	SOC_DAPM_ENUM("SOUT5C Select", ak4602_sout5c_mux_enum);

static const struct soc_enum ak4602_sout5d_mux_enum =
	SOC_ENUM_SINGLE(AK4602_4D_SDOUT5_TDM_SLOT7_8, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout5d_mux_control =
	SOC_DAPM_ENUM("SOUT5D Select", ak4602_sout5d_mux_enum);

static const struct soc_enum ak4602_sout5e_mux_enum =
	SOC_ENUM_SINGLE(AK4602_4E_SDOUT5_TDM_SLOT9_10, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout5e_mux_control =
	SOC_DAPM_ENUM("SOUT5E Select", ak4602_sout5e_mux_enum);

static const struct soc_enum ak4602_sout5f_mux_enum =
	SOC_ENUM_SINGLE(AK4602_4F_SDOUT5_TDM_SLOT11_12, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout5f_mux_control =
	SOC_DAPM_ENUM("SOUT5F Select", ak4602_sout5f_mux_enum);

static const struct soc_enum ak4602_sout5g_mux_enum =
	SOC_ENUM_SINGLE(AK4602_50_SDOUT5_TDM_SLOT13_14, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout5g_mux_control =
	SOC_DAPM_ENUM("SOUT5G Select", ak4602_sout5g_mux_enum);

static const struct soc_enum ak4602_sout5h_mux_enum =
	SOC_ENUM_SINGLE(AK4602_51_SDOUT5_TDM_SLOT15_16, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_sout5h_mux_control =
	SOC_DAPM_ENUM("SOUT5H Select", ak4602_sout5h_mux_enum);


static const struct soc_enum ak4602_dac1_mux_enum =
	SOC_ENUM_SINGLE(AK4602_52_DAC1_INPUT_DATA, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_dac1_mux_control =
	SOC_DAPM_ENUM("DAC1 Select", ak4602_dac1_mux_enum);

static const struct soc_enum ak4602_dac2_mux_enum =
	SOC_ENUM_SINGLE(AK4602_53_DAC2_INPUT_DATA, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_dac2_mux_control =
	SOC_DAPM_ENUM("DAC2 Select", ak4602_dac2_mux_enum);

static const struct soc_enum ak4602_dac3_mux_enum =
	SOC_ENUM_SINGLE(AK4602_54_DAC3_INPUT_DATA, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_dac3_mux_control =
	SOC_DAPM_ENUM("DAC3 Select", ak4602_dac3_mux_enum);

static const struct soc_enum ak4602_vol1_mux_enum =
	SOC_ENUM_SINGLE(AK4602_55_VOL1_INPUT_DATA, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_vol1_mux_control =
	SOC_DAPM_ENUM("VOL1 Select", ak4602_vol1_mux_enum);

static const struct soc_enum ak4602_vol2_mux_enum =
	SOC_ENUM_SINGLE(AK4602_56_VOL2_INPUT_DATA, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_vol2_mux_control =
	SOC_DAPM_ENUM("VOL2 Select", ak4602_vol2_mux_enum);

static const struct soc_enum ak4602_vol3_mux_enum =
	SOC_ENUM_SINGLE(AK4602_57_VOL3_INPUT_DATA, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_vol3_mux_control =
	SOC_DAPM_ENUM("VOL3 Select", ak4602_vol3_mux_enum);

static const struct soc_enum ak4602_vol4_mux_enum =
	SOC_ENUM_SINGLE(AK4602_58_VOL4_INPUT_DATA, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_vol4_mux_control =
	SOC_DAPM_ENUM("VOL4 Select", ak4602_vol4_mux_enum);

static const struct soc_enum ak4602_vol5_mux_enum =
	SOC_ENUM_SINGLE(AK4602_59_VOL5_INPUT_DATA, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_vol5_mux_control =
	SOC_DAPM_ENUM("VOL5 Select", ak4602_vol5_mux_enum);

static const struct soc_enum ak4602_src1_mux_enum =
	SOC_ENUM_SINGLE(AK4602_5A_SRC1_INPUT_DATA, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_src1_mux_control =
	SOC_DAPM_ENUM("SRC1 Select", ak4602_src1_mux_enum);

static const struct soc_enum ak4602_src2_mux_enum =
	SOC_ENUM_SINGLE(AK4602_5B_SRC2_INPUT_DATA, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_src2_mux_control =
	SOC_DAPM_ENUM("SRC2 Select", ak4602_src2_mux_enum);

static const struct soc_enum ak4602_src3_mux_enum =
	SOC_ENUM_SINGLE(AK4602_5C_SRC3_INPUT_DATA, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_src3_mux_control =
	SOC_DAPM_ENUM("SRC3 Select", ak4602_src3_mux_enum);

static const struct soc_enum ak4602_src4_mux_enum =
	SOC_ENUM_SINGLE(AK4602_5D_SRC4_INPUT_DATA, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_src4_mux_control =
	SOC_DAPM_ENUM("SRC4 Select", ak4602_src4_mux_enum);

static const struct soc_enum ak4602_mixera1_mux_enum =
	SOC_ENUM_SINGLE(AK4602_62_MIXER_A_CH1_INPUT, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_mixera1_mux_control =
	SOC_DAPM_ENUM("MixerA1 Select", ak4602_mixera1_mux_enum);

static const struct soc_enum ak4602_mixera2_mux_enum =
	SOC_ENUM_SINGLE(AK4602_63_MIXER_A_CH2_INPUT, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_mixera2_mux_control =
	SOC_DAPM_ENUM("MixerA2 Select", ak4602_mixera2_mux_enum);

static const struct soc_enum ak4602_mixerb1_mux_enum =
	SOC_ENUM_SINGLE(AK4602_64_MIXER_B_CH1_INPUT, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_mixerb1_mux_control =
	SOC_DAPM_ENUM("MixerB1 Select", ak4602_mixerb1_mux_enum);

static const struct soc_enum ak4602_mixerb2_mux_enum =
	SOC_ENUM_SINGLE(AK4602_65_MIXER_B_CH2_INPUT, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_mixerb2_mux_control =
	SOC_DAPM_ENUM("MixerB2 Select", ak4602_mixerb2_mux_enum);

static const struct soc_enum ak4602_dit_mux_enum =
	SOC_ENUM_SINGLE(AK4602_66_DIT_INPUT_DATA, 0,
			ARRAY_SIZE(ak4602_source_select_texts), ak4602_source_select_texts);

static const struct snd_kcontrol_new ak4602_dit_mux_control =
	SOC_DAPM_ENUM("DIT Select", ak4602_dit_mux_enum);


// 170719
static int ak4602_ckreset_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:	/* after widget power up */
		mdelay(10);
		akdbgprt("\t[AK4602] %s 10msec wait\n", __func__);
		break;
	}

	return 0;
}

// ******************************************************************

static const struct snd_soc_dapm_widget ak4602_dapm_widgets[] = {

/* Power Supply */
	SND_SOC_DAPM_SUPPLY_S("AK4602 Clock Power", 1, AK4602_01_SYSTEM_CLOCK_2, 7, 0, ak4602_ckreset_event, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_SUPPLY_S("AK4602 HUB Power", 2, AK4602_A9_RESET_CONTROL, 0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY_S("AK4602 CODEC Power", 4, AK4602_A9_RESET_CONTROL, 4, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY_S("AK4602 SDOUT1E", 5, AK4602_7B_OUTPUT_PORT_ENABLE, 5, 0, NULL, 0),

// Digital Input/Output
	SND_SOC_DAPM_AIF_IN("AK4602 SDIN1", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AK4602 SDIN2", "AIF2 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AK4602 SDIN3", "AIF3 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AK4602 SDIN4", "AIF4 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AK4602 SDIN5", "AIF5 Playback", 0, SND_SOC_NOPM, 0, 0),
#ifdef TCC_EVB
	SND_SOC_DAPM_AIF_OUT("AK4602 SDOUT1", "AIF2 Capture", 0, AK4602_7B_OUTPUT_PORT_ENABLE, 5, 0),
	SND_SOC_DAPM_AIF_OUT("AK4602 SDOUT2", "AIF2 Capture", 0, AK4602_7B_OUTPUT_PORT_ENABLE, 4, 0),
#else
	SND_SOC_DAPM_AIF_OUT("AK4602 SDOUT1", "AIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AK4602 SDOUT2", "AIF2 Capture", 0, AK4602_7B_OUTPUT_PORT_ENABLE, 4, 0),
#endif
	SND_SOC_DAPM_AIF_OUT("AK4602 SDOUT3", "AIF3 Capture", 0, AK4602_7B_OUTPUT_PORT_ENABLE, 3, 0),
	SND_SOC_DAPM_AIF_OUT("AK4602 SDOUT4", "AIF4 Capture", 0, AK4602_7B_OUTPUT_PORT_ENABLE, 2, 0),
	SND_SOC_DAPM_AIF_OUT("AK4602 SDOUT5", "AIF5 Capture", 0, AK4602_7B_OUTPUT_PORT_ENABLE, 1, 0),
// CODEC
	SND_SOC_DAPM_ADC("AK4602 ADC1", NULL, AK4602_A7_POWER_MANAGEMENT_1, 5, 0),
	SND_SOC_DAPM_ADC("AK4602 ADC2", NULL, AK4602_A7_POWER_MANAGEMENT_1, 4, 0),
	SND_SOC_DAPM_ADC("AK4602 ADCM", NULL, AK4602_A7_POWER_MANAGEMENT_1, 3, 0),
#ifdef TCC_ALWAYS_PMDAC_ON
	SND_SOC_DAPM_DAC("AK4602 DAC1", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("AK4602 DAC2", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC("AK4602 DAC3", NULL, SND_SOC_NOPM, 0, 0),
#else
	SND_SOC_DAPM_DAC("AK4602 DAC1", NULL, AK4602_A7_POWER_MANAGEMENT_1, 2, 0),
	SND_SOC_DAPM_DAC("AK4602 DAC2", NULL, AK4602_A7_POWER_MANAGEMENT_1, 1, 0),
	SND_SOC_DAPM_DAC("AK4602 DAC3", NULL, AK4602_A7_POWER_MANAGEMENT_1, 0, 0),
#endif //TCC_ALWAYS_PMDAC_ON

//  VOL,MixerA,SRC,DIT
	SND_SOC_DAPM_PGA("AK4602 VOL1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AK4602 VOL2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AK4602 VOL3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AK4602 VOL4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AK4602 VOL5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AK4602 MixerA", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AK4602 MixerB", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AK4602 SRC1", AK4602_A8_POWER_MANAGEMENT_2, 7, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AK4602 SRC2", AK4602_A8_POWER_MANAGEMENT_2, 6, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AK4602 SRC3", AK4602_A8_POWER_MANAGEMENT_2, 5, 0, NULL, 0),
	SND_SOC_DAPM_PGA("AK4602 SRC4", AK4602_A8_POWER_MANAGEMENT_2, 4, 0, NULL, 0),

	SND_SOC_DAPM_PGA("AK4602 DIT", AK4602_A7_POWER_MANAGEMENT_1, 6, 0, NULL, 0),
	SND_SOC_DAPM_OUTPUT("AK4602 DITOUT"),
// *****

// Analog Input
	SND_SOC_DAPM_INPUT("AK4602 AIN1L"),
	SND_SOC_DAPM_INPUT("AK4602 AIN1R"),
	SND_SOC_DAPM_INPUT("AK4602 AIN2"),
	SND_SOC_DAPM_INPUT("AK4602 AIN3"),
	SND_SOC_DAPM_INPUT("AK4602 AIN4"),
	SND_SOC_DAPM_INPUT("AK4602 AIN5"),
	SND_SOC_DAPM_INPUT("AK4602 AINM"),

	SND_SOC_DAPM_MICBIAS("AK4602 MicBias1", AK4602_03_MIC_BIAS_POWER, 1, 0),
	SND_SOC_DAPM_MUX("AK4602 MicBias1 MUX", SND_SOC_NOPM, 0, 0, &ak4602_micbias1_mux_control),

	SND_SOC_DAPM_MICBIAS("AK4602 MicBias2", AK4602_03_MIC_BIAS_POWER, 0, 0),
	SND_SOC_DAPM_MUX("AK4602 MicBias2 MUX", SND_SOC_NOPM, 0, 0, &ak4602_micbias2_mux_control),

	SND_SOC_DAPM_MUX("AK4602 ADC2 MUX", SND_SOC_NOPM, 0, 0, &ak4602_adc2_mux_control),

// Analog Output
	SND_SOC_DAPM_OUTPUT("AK4602 AOUT1"),
	SND_SOC_DAPM_OUTPUT("AK4602 AOUT2"),
	SND_SOC_DAPM_OUTPUT("AK4602 AOUT3"),

// Source Selector
	SND_SOC_DAPM_MUX("AK4602 SDOUT1A Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout1a_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT1B Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout1b_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT1C Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout1c_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT1D Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout1d_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT1E Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout1e_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT1F Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout1f_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT1G Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout1g_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT1H Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout1h_mux_control),

	SND_SOC_DAPM_MUX("AK4602 SDOUT2A Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout2a_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT2B Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout2b_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT2C Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout2c_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT2D Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout2d_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT2E Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout2e_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT2F Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout2f_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT2G Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout2g_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT2H Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout2h_mux_control),

	SND_SOC_DAPM_MUX("AK4602 SDOUT3A Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout3a_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT3B Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout3b_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT3C Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout3c_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT3D Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout3d_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT3E Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout3e_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT3F Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout3f_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT3G Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout3g_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT3H Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout3h_mux_control),

	SND_SOC_DAPM_MUX("AK4602 SDOUT4A Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout4a_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT4B Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout4b_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT4C Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout4c_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT4D Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout4d_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT4E Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout4e_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT4F Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout4f_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT4G Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout4g_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT4H Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout4h_mux_control),

	SND_SOC_DAPM_MUX("AK4602 SDOUT5A Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout5a_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT5B Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout5b_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT5C Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout5c_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT5D Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout5d_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT5E Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout5e_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT5F Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout5f_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT5G Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout5g_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SDOUT5H Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_sout5h_mux_control),


	SND_SOC_DAPM_MUX("AK4602 DAC1 Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_dac1_mux_control),
	SND_SOC_DAPM_MUX("AK4602 DAC2 Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_dac2_mux_control),
	SND_SOC_DAPM_MUX("AK4602 DAC3 Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_dac3_mux_control),

	SND_SOC_DAPM_MUX("AK4602 VOL1 Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_vol1_mux_control),
	SND_SOC_DAPM_MUX("AK4602 VOL2 Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_vol2_mux_control),
	SND_SOC_DAPM_MUX("AK4602 VOL3 Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_vol3_mux_control),
	SND_SOC_DAPM_MUX("AK4602 VOL4 Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_vol4_mux_control),
	SND_SOC_DAPM_MUX("AK4602 VOL5 Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_vol5_mux_control),

	SND_SOC_DAPM_MUX("AK4602 SRC1 Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_src1_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SRC2 Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_src2_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SRC3 Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_src3_mux_control),
	SND_SOC_DAPM_MUX("AK4602 SRC4 Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_src4_mux_control),

	SND_SOC_DAPM_MUX("AK4602 MixerA1 Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_mixera1_mux_control),
	SND_SOC_DAPM_MUX("AK4602 MixerA2 Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_mixera2_mux_control),
	SND_SOC_DAPM_MUX("AK4602 MixerB1 Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_mixerb1_mux_control),
	SND_SOC_DAPM_MUX("AK4602 MixerB2 Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_mixerb2_mux_control),

	SND_SOC_DAPM_MUX("AK4602 DIT Source Selector", SND_SOC_NOPM, 0, 0, &ak4602_dit_mux_control),

};

static const struct snd_soc_dapm_route ak4602_intercon[] = {

// Power Setting
	{"AK4602 HUB Power", NULL, "AK4602 Clock Power"},
	{"AK4602 CODEC Power", NULL, "AK4602 HUB Power"},

	{"AK4602 ADC1", NULL, "AK4602 CODEC Power"},
	{"AK4602 ADC2", NULL, "AK4602 CODEC Power"},
	{"AK4602 ADCM", NULL, "AK4602 CODEC Power"},
	{"AK4602 DAC1", NULL, "AK4602 CODEC Power"},
	{"AK4602 DAC2", NULL, "AK4602 CODEC Power"},
	{"AK4602 DAC3", NULL, "AK4602 CODEC Power"},

	{"AK4602 SDIN1", NULL, "AK4602 HUB Power"},
	{"AK4602 SDIN2", NULL, "AK4602 HUB Power"},
	{"AK4602 SDIN3", NULL, "AK4602 HUB Power"},
	{"AK4602 SDIN4", NULL, "AK4602 HUB Power"},


	{"AK4602 DITOUT", NULL, "AK4602 DIT"},
	{"AK4602 SDOUT1", NULL, "AK4602 SDOUT1E"},
	{"AK4602 DIT", NULL, "AK4602 SDOUT1E"},

// ADC Input
	{"AK4602 MicBias1", NULL, "AK4602 AIN1L"},
	{"AK4602 MicBias1 MUX", "LineIn", "AK4602 AIN1L"},
	{"AK4602 MicBias1 MUX", "MicBias", "AK4602 MicBias1"},

	{"AK4602 MicBias2", NULL, "AK4602 AIN1R"},
	{"AK4602 MicBias2 MUX", "LineIn", "AK4602 AIN1R"},
	{"AK4602 MicBias2 MUX", "MicBias", "AK4602 MicBias2"},

	{"AK4602 ADC1", NULL, "AK4602 MicBias1 MUX"},
	{"AK4602 ADC1", NULL, "AK4602 MicBias2 MUX"},

	{"AK4602 ADC2 MUX", "AIN2", "AK4602 AIN2"},
	{"AK4602 ADC2 MUX", "AIN3", "AK4602 AIN3"},
	{"AK4602 ADC2 MUX", "AIN4", "AK4602 AIN4"},
	{"AK4602 ADC2 MUX", "AIN5", "AK4602 AIN5"},
	{"AK4602 ADC2", NULL, "AK4602 ADC2 MUX"},

	{"AK4602 ADCM", NULL, "AK4602 AINM"},

// DAC Output
	{"AK4602 DAC1", NULL, "AK4602 DAC1 Source Selector"},
	{"AK4602 AOUT1", NULL, "AK4602 DAC1"},
	{"AK4602 DAC2", NULL, "AK4602 DAC2 Source Selector"},
	{"AK4602 AOUT2", NULL, "AK4602 DAC2"},
	{"AK4602 DAC3", NULL, "AK4602 DAC3 Source Selector"},
	{"AK4602 AOUT3", NULL, "AK4602 DAC3"},

// SDOUT
	{"AK4602 SDOUT1", NULL, "AK4602 SDOUT1A Source Selector"},
	{"AK4602 SDOUT1", NULL, "AK4602 SDOUT1B Source Selector"},
	{"AK4602 SDOUT1", NULL, "AK4602 SDOUT1C Source Selector"},
	{"AK4602 SDOUT1", NULL, "AK4602 SDOUT1D Source Selector"},
	{"AK4602 SDOUT1", NULL, "AK4602 SDOUT1E Source Selector"},
	{"AK4602 SDOUT1", NULL, "AK4602 SDOUT1F Source Selector"},
	{"AK4602 SDOUT1", NULL, "AK4602 SDOUT1G Source Selector"},
	{"AK4602 SDOUT1", NULL, "AK4602 SDOUT1H Source Selector"},
	{"AK4602 SDOUT2", NULL, "AK4602 SDOUT2A Source Selector"},
	{"AK4602 SDOUT2", NULL, "AK4602 SDOUT2B Source Selector"},
	{"AK4602 SDOUT2", NULL, "AK4602 SDOUT2C Source Selector"},
	{"AK4602 SDOUT2", NULL, "AK4602 SDOUT2D Source Selector"},
	{"AK4602 SDOUT2", NULL, "AK4602 SDOUT2E Source Selector"},
	{"AK4602 SDOUT2", NULL, "AK4602 SDOUT2F Source Selector"},
	{"AK4602 SDOUT2", NULL, "AK4602 SDOUT2G Source Selector"},
	{"AK4602 SDOUT2", NULL, "AK4602 SDOUT2H Source Selector"},
	{"AK4602 SDOUT3", NULL, "AK4602 SDOUT3A Source Selector"},
	{"AK4602 SDOUT3", NULL, "AK4602 SDOUT3B Source Selector"},
	{"AK4602 SDOUT3", NULL, "AK4602 SDOUT3C Source Selector"},
	{"AK4602 SDOUT3", NULL, "AK4602 SDOUT3D Source Selector"},
	{"AK4602 SDOUT3", NULL, "AK4602 SDOUT3E Source Selector"},
	{"AK4602 SDOUT3", NULL, "AK4602 SDOUT3F Source Selector"},
	{"AK4602 SDOUT3", NULL, "AK4602 SDOUT3G Source Selector"},
	{"AK4602 SDOUT3", NULL, "AK4602 SDOUT3H Source Selector"},
	{"AK4602 SDOUT4", NULL, "AK4602 SDOUT4A Source Selector"},
	{"AK4602 SDOUT4", NULL, "AK4602 SDOUT4B Source Selector"},
	{"AK4602 SDOUT4", NULL, "AK4602 SDOUT4C Source Selector"},
	{"AK4602 SDOUT4", NULL, "AK4602 SDOUT4D Source Selector"},
	{"AK4602 SDOUT4", NULL, "AK4602 SDOUT4E Source Selector"},
	{"AK4602 SDOUT4", NULL, "AK4602 SDOUT4F Source Selector"},
	{"AK4602 SDOUT4", NULL, "AK4602 SDOUT4G Source Selector"},
	{"AK4602 SDOUT4", NULL, "AK4602 SDOUT4H Source Selector"},
	{"AK4602 SDOUT5", NULL, "AK4602 SDOUT5A Source Selector"},
	{"AK4602 SDOUT5", NULL, "AK4602 SDOUT5B Source Selector"},
	{"AK4602 SDOUT5", NULL, "AK4602 SDOUT5C Source Selector"},
	{"AK4602 SDOUT5", NULL, "AK4602 SDOUT5D Source Selector"},
	{"AK4602 SDOUT5", NULL, "AK4602 SDOUT5E Source Selector"},
	{"AK4602 SDOUT5", NULL, "AK4602 SDOUT5F Source Selector"},
	{"AK4602 SDOUT5", NULL, "AK4602 SDOUT5G Source Selector"},
	{"AK4602 SDOUT5", NULL, "AK4602 SDOUT5H Source Selector"},

//  SRC,VOL,Mixer,DIT
	{"AK4602 VOL1", NULL, "AK4602 VOL1 Source Selector"},
	{"AK4602 VOL2", NULL, "AK4602 VOL2 Source Selector"},
	{"AK4602 VOL3", NULL, "AK4602 VOL3 Source Selector"},
	{"AK4602 VOL4", NULL, "AK4602 VOL4 Source Selector"},
	{"AK4602 VOL5", NULL, "AK4602 VOL5 Source Selector"},
	{"AK4602 MixerA", NULL, "AK4602 MixerA1 Source Selector"},
	{"AK4602 MixerA", NULL, "AK4602 MixerA2 Source Selector"},
	{"AK4602 MixerB", NULL, "AK4602 MixerB1 Source Selector"},
	{"AK4602 MixerB", NULL, "AK4602 MixerB2 Source Selector"},
	{"AK4602 SRC1", NULL, "AK4602 SRC1 Source Selector"},
	{"AK4602 SRC2", NULL, "AK4602 SRC2 Source Selector"},
	{"AK4602 SRC3", NULL, "AK4602 SRC3 Source Selector"},
	{"AK4602 SRC4", NULL, "AK4602 SRC4 Source Selector"},
	{"AK4602 DIT", NULL, "AK4602 DIT Source Selector"},


// Selector
	{"AK4602 SDOUT1A Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT1A Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT1A Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT1A Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT1A Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT1A Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT1A Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT1A Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT1A Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT1A Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT1A Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT1A Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT1A Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT1A Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT1A Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT1A Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT1A Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT1A Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT1A Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT1A Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT1A Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT1A Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT1A Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT1A Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT1A Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT1A Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT1A Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT1A Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT1A Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT1A Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT1A Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT1A Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT1A Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT1A Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT1A Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT1A Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT1A Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT1A Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT1A Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT1A Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT1A Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT1A Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT1A Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT1A Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT1A Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT1A Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT1A Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT1A Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT1A Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT1A Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT1A Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT1A Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT1A Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT1A Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT1B Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT1B Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT1B Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT1B Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT1B Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT1B Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT1B Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT1B Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT1B Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT1B Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT1B Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT1B Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT1B Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT1B Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT1B Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT1B Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT1B Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT1B Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT1B Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT1B Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT1B Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT1B Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT1B Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT1B Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT1B Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT1B Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT1B Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT1B Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT1B Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT1B Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT1B Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT1B Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT1B Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT1B Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT1B Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT1B Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT1B Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT1B Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT1B Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT1B Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT1B Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT1B Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT1B Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT1B Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT1B Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT1B Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT1B Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT1B Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT1B Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT1B Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT1B Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT1B Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT1B Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT1B Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT1C Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT1C Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT1C Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT1C Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT1C Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT1C Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT1C Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT1C Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT1C Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT1C Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT1C Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT1C Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT1C Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT1C Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT1C Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT1C Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT1C Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT1C Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT1C Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT1C Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT1C Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT1C Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT1C Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT1C Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT1C Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT1C Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT1C Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT1C Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT1C Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT1C Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT1C Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT1C Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT1C Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT1C Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT1C Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT1C Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT1C Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT1C Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT1C Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT1C Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT1C Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT1C Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT1C Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT1C Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT1C Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT1C Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT1C Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT1C Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT1C Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT1C Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT1C Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT1C Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT1C Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT1C Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT1D Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT1D Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT1D Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT1D Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT1D Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT1D Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT1D Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT1D Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT1D Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT1D Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT1D Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT1D Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT1D Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT1D Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT1D Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT1D Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT1D Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT1D Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT1D Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT1D Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT1D Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT1D Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT1D Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT1D Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT1D Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT1D Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT1D Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT1D Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT1D Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT1D Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT1D Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT1D Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT1D Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT1D Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT1D Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT1D Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT1D Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT1D Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT1D Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT1D Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT1D Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT1D Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT1D Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT1D Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT1D Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT1D Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT1D Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT1D Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT1D Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT1D Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT1D Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT1D Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT1D Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT1D Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT1E Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT1E Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT1E Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT1E Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT1E Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT1E Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT1E Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT1E Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT1E Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT1E Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT1E Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT1E Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT1E Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT1E Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT1E Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT1E Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT1E Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT1E Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT1E Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT1E Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT1E Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT1E Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT1E Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT1E Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT1E Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT1E Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT1E Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT1E Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT1E Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT1E Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT1E Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT1E Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT1E Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT1E Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT1E Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT1E Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT1E Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT1E Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT1E Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT1E Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT1E Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT1E Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT1E Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT1E Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT1E Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT1E Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT1E Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT1E Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT1E Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT1E Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT1E Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT1E Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT1E Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT1E Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT1F Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT1F Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT1F Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT1F Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT1F Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT1F Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT1F Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT1F Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT1F Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT1F Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT1F Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT1F Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT1F Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT1F Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT1F Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT1F Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT1F Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT1F Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT1F Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT1F Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT1F Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT1F Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT1F Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT1F Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT1F Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT1F Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT1F Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT1F Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT1F Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT1F Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT1F Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT1F Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT1F Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT1F Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT1F Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT1F Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT1F Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT1F Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT1F Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT1F Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT1F Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT1F Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT1F Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT1F Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT1F Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT1F Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT1F Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT1F Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT1F Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT1F Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT1F Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT1F Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT1F Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT1F Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT1G Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT1G Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT1G Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT1G Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT1G Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT1G Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT1G Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT1G Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT1G Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT1G Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT1G Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT1G Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT1G Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT1G Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT1G Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT1G Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT1G Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT1G Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT1G Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT1G Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT1G Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT1G Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT1G Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT1G Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT1G Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT1G Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT1G Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT1G Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT1G Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT1G Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT1G Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT1G Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT1G Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT1G Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT1G Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT1G Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT1G Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT1G Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT1G Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT1G Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT1G Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT1G Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT1G Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT1G Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT1G Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT1G Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT1G Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT1G Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT1G Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT1G Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT1G Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT1G Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT1G Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT1G Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT1H Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT1H Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT1H Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT1H Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT1H Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT1H Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT1H Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT1H Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT1H Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT1H Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT1H Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT1H Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT1H Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT1H Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT1H Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT1H Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT1H Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT1H Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT1H Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT1H Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT1H Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT1H Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT1H Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT1H Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT1H Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT1H Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT1H Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT1H Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT1H Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT1H Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT1H Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT1H Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT1H Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT1H Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT1H Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT1H Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT1H Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT1H Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT1H Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT1H Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT1H Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT1H Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT1H Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT1H Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT1H Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT1H Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT1H Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT1H Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT1H Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT1H Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT1H Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT1H Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT1H Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT1H Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT2A Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT2A Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT2A Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT2A Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT2A Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT2A Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT2A Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT2A Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT2A Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT2A Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT2A Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT2A Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT2A Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT2A Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT2A Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT2A Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT2A Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT2A Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT2A Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT2A Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT2A Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT2A Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT2A Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT2A Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT2A Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT2A Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT2A Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT2A Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT2A Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT2A Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT2A Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT2A Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT2A Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT2A Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT2A Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT2A Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT2A Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT2A Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT2A Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT2A Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT2A Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT2A Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT2A Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT2A Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT2A Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT2A Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT2A Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT2A Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT2A Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT2A Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT2A Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT2A Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT2A Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT2A Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT2B Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT2B Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT2B Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT2B Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT2B Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT2B Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT2B Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT2B Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT2B Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT2B Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT2B Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT2B Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT2B Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT2B Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT2B Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT2B Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT2B Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT2B Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT2B Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT2B Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT2B Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT2B Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT2B Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT2B Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT2B Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT2B Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT2B Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT2B Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT2B Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT2B Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT2B Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT2B Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT2B Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT2B Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT2B Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT2B Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT2B Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT2B Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT2B Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT2B Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT2B Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT2B Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT2B Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT2B Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT2B Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT2B Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT2B Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT2B Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT2B Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT2B Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT2B Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT2B Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT2B Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT2B Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT2C Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT2C Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT2C Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT2C Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT2C Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT2C Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT2C Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT2C Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT2C Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT2C Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT2C Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT2C Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT2C Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT2C Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT2C Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT2C Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT2C Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT2C Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT2C Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT2C Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT2C Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT2C Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT2C Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT2C Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT2C Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT2C Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT2C Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT2C Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT2C Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT2C Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT2C Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT2C Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT2C Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT2C Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT2C Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT2C Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT2C Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT2C Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT2C Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT2C Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT2C Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT2C Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT2C Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT2C Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT2C Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT2C Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT2C Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT2C Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT2C Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT2C Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT2C Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT2C Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT2C Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT2C Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT2D Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT2D Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT2D Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT2D Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT2D Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT2D Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT2D Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT2D Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT2D Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT2D Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT2D Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT2D Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT2D Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT2D Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT2D Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT2D Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT2D Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT2D Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT2D Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT2D Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT2D Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT2D Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT2D Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT2D Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT2D Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT2D Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT2D Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT2D Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT2D Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT2D Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT2D Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT2D Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT2D Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT2D Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT2D Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT2D Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT2D Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT2D Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT2D Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT2D Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT2D Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT2D Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT2D Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT2D Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT2D Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT2D Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT2D Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT2D Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT2D Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT2D Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT2D Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT2D Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT2D Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT2D Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT2E Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT2E Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT2E Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT2E Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT2E Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT2E Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT2E Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT2E Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT2E Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT2E Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT2E Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT2E Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT2E Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT2E Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT2E Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT2E Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT2E Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT2E Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT2E Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT2E Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT2E Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT2E Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT2E Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT2E Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT2E Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT2E Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT2E Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT2E Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT2E Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT2E Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT2E Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT2E Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT2E Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT2E Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT2E Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT2E Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT2E Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT2E Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT2E Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT2E Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT2E Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT2E Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT2E Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT2E Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT2E Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT2E Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT2E Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT2E Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT2E Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT2E Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT2E Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT2E Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT2E Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT2E Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT2F Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT2F Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT2F Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT2F Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT2F Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT2F Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT2F Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT2F Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT2F Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT2F Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT2F Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT2F Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT2F Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT2F Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT2F Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT2F Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT2F Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT2F Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT2F Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT2F Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT2F Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT2F Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT2F Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT2F Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT2F Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT2F Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT2F Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT2F Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT2F Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT2F Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT2F Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT2F Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT2F Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT2F Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT2F Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT2F Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT2F Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT2F Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT2F Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT2F Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT2F Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT2F Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT2F Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT2F Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT2F Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT2F Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT2F Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT2F Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT2F Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT2F Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT2F Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT2F Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT2F Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT2F Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT2G Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT2G Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT2G Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT2G Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT2G Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT2G Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT2G Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT2G Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT2G Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT2G Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT2G Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT2G Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT2G Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT2G Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT2G Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT2G Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT2G Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT2G Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT2G Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT2G Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT2G Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT2G Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT2G Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT2G Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT2G Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT2G Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT2G Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT2G Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT2G Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT2G Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT2G Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT2G Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT2G Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT2G Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT2G Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT2G Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT2G Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT2G Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT2G Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT2G Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT2G Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT2G Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT2G Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT2G Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT2G Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT2G Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT2G Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT2G Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT2G Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT2G Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT2G Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT2G Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT2G Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT2G Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT2H Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT2H Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT2H Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT2H Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT2H Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT2H Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT2H Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT2H Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT2H Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT2H Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT2H Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT2H Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT2H Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT2H Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT2H Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT2H Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT2H Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT2H Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT2H Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT2H Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT2H Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT2H Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT2H Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT2H Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT2H Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT2H Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT2H Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT2H Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT2H Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT2H Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT2H Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT2H Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT2H Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT2H Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT2H Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT2H Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT2H Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT2H Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT2H Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT2H Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT2H Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT2H Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT2H Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT2H Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT2H Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT2H Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT2H Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT2H Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT2H Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT2H Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT2H Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT2H Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT2H Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT2H Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT3A Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT3A Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT3A Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT3A Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT3A Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT3A Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT3A Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT3A Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT3A Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT3A Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT3A Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT3A Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT3A Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT3A Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT3A Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT3A Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT3A Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT3A Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT3A Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT3A Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT3A Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT3A Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT3A Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT3A Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT3A Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT3A Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT3A Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT3A Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT3A Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT3A Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT3A Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT3A Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT3A Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT3A Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT3A Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT3A Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT3A Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT3A Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT3A Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT3A Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT3A Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT3A Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT3A Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT3A Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT3A Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT3A Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT3A Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT3A Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT3A Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT3A Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT3A Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT3A Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT3A Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT3A Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT3B Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT3B Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT3B Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT3B Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT3B Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT3B Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT3B Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT3B Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT3B Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT3B Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT3B Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT3B Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT3B Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT3B Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT3B Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT3B Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT3B Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT3B Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT3B Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT3B Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT3B Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT3B Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT3B Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT3B Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT3B Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT3B Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT3B Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT3B Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT3B Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT3B Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT3B Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT3B Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT3B Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT3B Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT3B Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT3B Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT3B Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT3B Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT3B Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT3B Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT3B Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT3B Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT3B Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT3B Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT3B Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT3B Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT3B Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT3B Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT3B Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT3B Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT3B Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT3B Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT3B Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT3B Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT3C Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT3C Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT3C Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT3C Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT3C Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT3C Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT3C Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT3C Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT3C Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT3C Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT3C Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT3C Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT3C Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT3C Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT3C Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT3C Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT3C Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT3C Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT3C Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT3C Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT3C Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT3C Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT3C Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT3C Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT3C Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT3C Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT3C Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT3C Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT3C Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT3C Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT3C Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT3C Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT3C Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT3C Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT3C Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT3C Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT3C Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT3C Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT3C Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT3C Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT3C Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT3C Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT3C Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT3C Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT3C Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT3C Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT3C Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT3C Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT3C Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT3C Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT3C Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT3C Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT3C Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT3C Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT3D Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT3D Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT3D Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT3D Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT3D Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT3D Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT3D Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT3D Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT3D Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT3D Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT3D Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT3D Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT3D Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT3D Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT3D Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT3D Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT3D Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT3D Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT3D Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT3D Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT3D Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT3D Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT3D Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT3D Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT3D Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT3D Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT3D Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT3D Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT3D Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT3D Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT3D Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT3D Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT3D Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT3D Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT3D Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT3D Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT3D Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT3D Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT3D Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT3D Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT3D Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT3D Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT3D Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT3D Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT3D Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT3D Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT3D Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT3D Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT3D Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT3D Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT3D Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT3D Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT3D Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT3E Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT3E Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT3E Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT3E Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT3E Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT3E Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT3E Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT3E Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT3E Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT3E Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT3E Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT3E Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT3E Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT3E Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT3E Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT3E Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT3E Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT3E Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT3E Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT3E Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT3E Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT3E Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT3E Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT3E Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT3E Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT3E Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT3E Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT3E Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT3E Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT3E Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT3E Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT3E Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT3E Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT3E Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT3E Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT3E Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT3E Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT3E Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT3E Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT3E Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT3E Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT3E Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT3E Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT3E Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT3E Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT3E Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT3E Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT3E Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT3E Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT3E Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT3E Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT3E Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT3E Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT3E Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT3F Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT3F Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT3F Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT3F Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT3F Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT3F Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT3F Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT3F Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT3F Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT3F Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT3F Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT3F Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT3F Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT3F Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT3F Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT3F Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT3F Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT3F Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT3F Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT3F Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT3F Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT3F Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT3F Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT3F Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT3F Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT3F Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT3F Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT3F Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT3F Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT3F Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT3F Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT3F Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT3F Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT3F Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT3F Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT3F Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT3F Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT3F Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT3F Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT3F Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT3F Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT3F Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT3F Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT3F Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT3F Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT3F Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT3F Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT3F Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT3F Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT3F Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT3F Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT3F Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT3F Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT3F Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT3G Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT3G Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT3G Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT3G Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT3G Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT3G Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT3G Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT3G Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT3G Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT3G Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT3G Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT3G Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT3G Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT3G Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT3G Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT3G Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT3G Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT3G Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT3G Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT3G Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT3G Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT3G Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT3G Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT3G Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT3G Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT3G Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT3G Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT3G Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT3G Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT3G Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT3G Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT3G Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT3G Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT3G Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT3G Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT3G Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT3G Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT3G Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT3G Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT3G Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT3G Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT3G Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT3G Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT3G Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT3G Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT3G Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT3G Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT3G Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT3G Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT3G Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT3G Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT3G Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT3G Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT3G Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT3H Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT3H Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT3H Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT3H Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT3H Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT3H Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT3H Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT3H Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT3H Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT3H Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT3H Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT3H Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT3H Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT3H Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT3H Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT3H Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT3H Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT3H Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT3H Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT3H Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT3H Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT3H Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT3H Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT3H Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT3H Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT3H Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT3H Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT3H Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT3H Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT3H Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT3H Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT3H Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT3H Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT3H Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT3H Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT3H Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT3H Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT3H Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT3H Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT3H Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT3H Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT3H Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT3H Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT3H Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT3H Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT3H Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT3H Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT3H Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT3H Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT3H Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT3H Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT3H Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT3H Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT3H Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT4A Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT4A Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT4A Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT4A Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT4A Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT4A Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT4A Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT4A Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT4A Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT4A Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT4A Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT4A Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT4A Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT4A Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT4A Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT4A Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT4A Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT4A Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT4A Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT4A Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT4A Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT4A Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT4A Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT4A Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT4A Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT4A Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT4A Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT4A Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT4A Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT4A Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT4A Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT4A Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT4A Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT4A Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT4A Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT4A Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT4A Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT4A Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT4A Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT4A Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT4A Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT4A Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT4A Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT4A Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT4A Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT4A Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT4A Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT4A Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT4A Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT4A Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT4A Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT4A Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT4A Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT4A Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT4B Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT4B Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT4B Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT4B Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT4B Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT4B Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT4B Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT4B Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT4B Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT4B Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT4B Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT4B Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT4B Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT4B Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT4B Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT4B Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT4B Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT4B Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT4B Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT4B Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT4B Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT4B Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT4B Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT4B Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT4B Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT4B Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT4B Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT4B Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT4B Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT4B Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT4B Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT4B Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT4B Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT4B Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT4B Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT4B Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT4B Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT4B Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT4B Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT4B Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT4B Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT4B Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT4B Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT4B Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT4B Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT4B Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT4B Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT4B Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT4B Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT4B Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT4B Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT4B Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT4B Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT4B Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT4C Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT4C Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT4C Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT4C Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT4C Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT4C Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT4C Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT4C Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT4C Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT4C Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT4C Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT4C Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT4C Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT4C Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT4C Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT4C Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT4C Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT4C Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT4C Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT4C Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT4C Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT4C Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT4C Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT4C Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT4C Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT4C Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT4C Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT4C Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT4C Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT4C Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT4C Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT4C Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT4C Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT4C Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT4C Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT4C Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT4C Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT4C Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT4C Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT4C Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT4C Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT4C Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT4C Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT4C Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT4C Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT4C Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT4C Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT4C Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT4C Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT4C Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT4C Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT4C Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT4C Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT4C Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT4D Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT4D Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT4D Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT4D Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT4D Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT4D Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT4D Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT4D Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT4D Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT4D Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT4D Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT4D Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT4D Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT4D Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT4D Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT4D Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT4D Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT4D Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT4D Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT4D Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT4D Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT4D Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT4D Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT4D Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT4D Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT4D Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT4D Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT4D Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT4D Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT4D Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT4D Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT4D Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT4D Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT4D Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT4D Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT4D Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT4D Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT4D Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT4D Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT4D Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT4D Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT4D Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT4D Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT4D Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT4D Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT4D Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT4D Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT4D Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT4D Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT4D Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT4D Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT4D Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT4D Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT4D Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT4E Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT4E Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT4E Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT4E Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT4E Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT4E Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT4E Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT4E Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT4E Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT4E Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT4E Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT4E Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT4E Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT4E Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT4E Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT4E Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT4E Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT4E Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT4E Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT4E Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT4E Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT4E Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT4E Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT4E Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT4E Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT4E Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT4E Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT4E Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT4E Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT4E Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT4E Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT4E Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT4E Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT4E Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT4E Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT4E Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT4E Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT4E Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT4E Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT4E Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT4E Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT4E Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT4E Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT4E Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT4E Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT4E Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT4E Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT4E Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT4E Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT4E Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT4E Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT4E Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT4E Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT4E Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT4F Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT4F Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT4F Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT4F Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT4F Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT4F Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT4F Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT4F Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT4F Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT4F Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT4F Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT4F Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT4F Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT4F Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT4F Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT4F Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT4F Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT4F Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT4F Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT4F Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT4F Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT4F Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT4F Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT4F Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT4F Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT4F Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT4F Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT4F Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT4F Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT4F Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT4F Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT4F Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT4F Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT4F Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT4F Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT4F Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT4F Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT4F Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT4F Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT4F Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT4F Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT4F Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT4F Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT4F Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT4F Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT4F Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT4F Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT4F Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT4F Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT4F Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT4F Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT4F Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT4F Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT4F Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT4G Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT4G Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT4G Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT4G Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT4G Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT4G Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT4G Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT4G Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT4G Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT4G Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT4G Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT4G Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT4G Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT4G Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT4G Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT4G Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT4G Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT4G Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT4G Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT4G Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT4G Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT4G Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT4G Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT4G Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT4G Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT4G Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT4G Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT4G Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT4G Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT4G Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT4G Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT4G Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT4G Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT4G Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT4G Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT4G Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT4G Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT4G Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT4G Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT4G Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT4G Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT4G Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT4G Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT4G Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT4G Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT4G Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT4G Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT4G Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT4G Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT4G Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT4G Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT4G Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT4G Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT4G Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT4H Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT4H Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT4H Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT4H Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT4H Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT4H Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT4H Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT4H Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT4H Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT4H Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT4H Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT4H Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT4H Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT4H Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT4H Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT4H Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT4H Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT4H Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT4H Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT4H Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT4H Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT4H Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT4H Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT4H Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT4H Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT4H Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT4H Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT4H Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT4H Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT4H Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT4H Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT4H Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT4H Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT4H Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT4H Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT4H Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT4H Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT4H Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT4H Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT4H Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT4H Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT4H Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT4H Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT4H Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT4H Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT4H Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT4H Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT4H Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT4H Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT4H Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT4H Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT4H Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT4H Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT4H Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT5A Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT5A Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT5A Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT5A Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT5A Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT5A Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT5A Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT5A Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT5A Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT5A Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT5A Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT5A Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT5A Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT5A Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT5A Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT5A Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT5A Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT5A Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT5A Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT5A Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT5A Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT5A Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT5A Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT5A Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT5A Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT5A Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT5A Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT5A Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT5A Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT5A Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT5A Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT5A Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT5A Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SDOUT5A Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT5A Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT5A Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT5A Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT5A Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT5A Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT5A Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT5A Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT5A Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT5A Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT5A Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT5A Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT5A Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT5A Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT5A Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT5A Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT5A Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT5A Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT5A Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT5A Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT5A Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT5B Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT5B Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT5B Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT5B Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT5B Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT5B Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT5B Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT5B Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT5B Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT5B Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT5B Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT5B Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT5B Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT5B Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT5B Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT5B Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT5B Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT5B Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT5B Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT5B Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT5B Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT5B Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT5B Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT5B Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT5B Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT5B Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT5B Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT5B Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT5B Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT5B Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT5B Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT5B Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT5B Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT5B Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SDOUT5B Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT5B Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT5B Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT5B Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT5B Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT5B Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT5B Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT5B Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT5B Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT5B Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT5B Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT5B Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT5B Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT5B Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT5B Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT5B Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT5B Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT5B Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT5B Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT5B Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT5C Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT5C Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT5C Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT5C Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT5C Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT5C Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT5C Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT5C Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT5C Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT5C Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT5C Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT5C Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT5C Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT5C Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT5C Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT5C Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT5C Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT5C Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT5C Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT5C Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT5C Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT5C Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT5C Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT5C Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT5C Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT5C Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT5C Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT5C Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT5C Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT5C Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT5C Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT5C Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT5C Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT5C Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT5C Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SDOUT5C Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT5C Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT5C Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT5C Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT5C Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT5C Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT5C Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT5C Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT5C Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT5C Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT5C Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT5C Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT5C Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT5C Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT5C Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT5C Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT5C Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT5C Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT5C Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT5D Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT5D Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT5D Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT5D Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT5D Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT5D Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT5D Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT5D Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT5D Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT5D Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT5D Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT5D Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT5D Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT5D Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT5D Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT5D Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT5D Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT5D Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT5D Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT5D Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT5D Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT5D Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT5D Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT5D Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT5D Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT5D Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT5D Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT5D Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT5D Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT5D Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT5D Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT5D Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT5D Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT5D Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT5D Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT5D Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SDOUT5D Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT5D Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT5D Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT5D Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT5D Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT5D Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT5D Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT5D Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT5D Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT5D Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT5D Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT5D Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT5D Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT5D Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT5D Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT5D Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT5D Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT5D Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT5E Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT5E Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT5E Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT5E Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT5E Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT5E Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT5E Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT5E Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT5E Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT5E Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT5E Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT5E Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT5E Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT5E Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT5E Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT5E Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT5E Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT5E Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT5E Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT5E Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT5E Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT5E Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT5E Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT5E Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT5E Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT5E Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT5E Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT5E Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT5E Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT5E Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT5E Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT5E Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT5E Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT5E Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT5E Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT5E Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT5E Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SDOUT5E Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT5E Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT5E Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT5E Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT5E Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT5E Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT5E Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT5E Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT5E Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT5E Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT5E Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT5E Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT5E Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT5E Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT5E Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT5E Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT5E Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT5F Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT5F Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT5F Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT5F Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT5F Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT5F Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT5F Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT5F Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT5F Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT5F Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT5F Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT5F Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT5F Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT5F Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT5F Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT5F Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT5F Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT5F Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT5F Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT5F Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT5F Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT5F Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT5F Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT5F Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT5F Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT5F Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT5F Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT5F Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT5F Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT5F Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT5F Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT5F Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT5F Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT5F Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT5F Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT5F Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT5F Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT5F Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SDOUT5F Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT5F Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT5F Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT5F Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT5F Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT5F Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT5F Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT5F Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT5F Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT5F Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT5F Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT5F Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT5F Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT5F Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT5F Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT5F Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT5G Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT5G Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT5G Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT5G Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT5G Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT5G Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT5G Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT5G Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT5G Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT5G Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT5G Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT5G Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT5G Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT5G Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT5G Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT5G Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT5G Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT5G Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT5G Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT5G Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT5G Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT5G Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT5G Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT5G Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT5G Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT5G Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT5G Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT5G Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT5G Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT5G Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT5G Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT5G Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT5G Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT5G Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT5G Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT5G Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT5G Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT5G Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT5G Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SDOUT5G Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT5G Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT5G Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT5G Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT5G Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT5G Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT5G Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT5G Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT5G Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT5G Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT5G Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT5G Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT5G Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT5G Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT5G Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SDOUT5H Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SDOUT5H Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SDOUT5H Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SDOUT5H Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SDOUT5H Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SDOUT5H Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SDOUT5H Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SDOUT5H Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SDOUT5H Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SDOUT5H Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SDOUT5H Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SDOUT5H Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SDOUT5H Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SDOUT5H Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SDOUT5H Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SDOUT5H Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SDOUT5H Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SDOUT5H Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SDOUT5H Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SDOUT5H Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SDOUT5H Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SDOUT5H Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SDOUT5H Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SDOUT5H Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SDOUT5H Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SDOUT5H Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SDOUT5H Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SDOUT5H Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SDOUT5H Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SDOUT5H Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SDOUT5H Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SDOUT5H Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SDOUT5H Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT5H Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT5H Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT5H Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT5H Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT5H Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT5H Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT5H Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SDOUT5H Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SDOUT5H Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SDOUT5H Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SDOUT5H Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SDOUT5H Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SDOUT5H Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SDOUT5H Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SDOUT5H Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SDOUT5H Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SDOUT5H Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SDOUT5H Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SDOUT5H Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SDOUT5H Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SDOUT5H Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 DAC1 Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 DAC1 Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 DAC1 Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 DAC1 Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 DAC1 Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 DAC1 Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 DAC1 Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 DAC1 Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 DAC1 Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 DAC1 Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 DAC1 Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 DAC1 Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 DAC1 Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 DAC1 Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 DAC1 Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 DAC1 Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 DAC1 Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 DAC1 Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 DAC1 Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 DAC1 Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 DAC1 Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 DAC1 Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 DAC1 Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 DAC1 Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 DAC1 Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 DAC1 Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 DAC1 Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 DAC1 Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 DAC1 Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 DAC1 Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 DAC1 Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 DAC1 Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 DAC1 Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 DAC1 Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 DAC1 Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 DAC1 Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 DAC1 Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 DAC1 Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 DAC1 Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 DAC1 Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 DAC1 Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 DAC1 Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 DAC1 Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 DAC1 Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 DAC1 Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 DAC1 Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 DAC1 Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 DAC1 Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 DAC1 Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 DAC1 Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 DAC1 Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 DAC1 Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 DAC1 Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 DAC1 Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 DAC2 Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 DAC2 Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 DAC2 Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 DAC2 Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 DAC2 Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 DAC2 Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 DAC2 Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 DAC2 Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 DAC2 Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 DAC2 Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 DAC2 Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 DAC2 Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 DAC2 Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 DAC2 Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 DAC2 Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 DAC2 Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 DAC2 Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 DAC2 Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 DAC2 Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 DAC2 Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 DAC2 Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 DAC2 Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 DAC2 Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 DAC2 Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 DAC2 Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 DAC2 Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 DAC2 Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 DAC2 Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 DAC2 Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 DAC2 Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 DAC2 Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 DAC2 Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 DAC2 Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 DAC2 Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 DAC2 Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 DAC2 Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 DAC2 Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 DAC2 Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 DAC2 Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 DAC2 Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 DAC2 Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 DAC2 Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 DAC2 Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 DAC2 Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 DAC2 Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 DAC2 Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 DAC2 Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 DAC2 Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 DAC2 Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 DAC2 Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 DAC2 Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 DAC2 Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 DAC2 Source Selector", "SRCO3", "AK4602 SRC3"},

	{"AK4602 DAC3 Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 DAC3 Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 DAC3 Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 DAC3 Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 DAC3 Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 DAC3 Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 DAC3 Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 DAC3 Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 DAC3 Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 DAC3 Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 DAC3 Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 DAC3 Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 DAC3 Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 DAC3 Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 DAC3 Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 DAC3 Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 DAC3 Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 DAC3 Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 DAC3 Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 DAC3 Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 DAC3 Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 DAC3 Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 DAC3 Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 DAC3 Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 DAC3 Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 DAC3 Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 DAC3 Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 DAC3 Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 DAC3 Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 DAC3 Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 DAC3 Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 DAC3 Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 DAC3 Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 DAC3 Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 DAC3 Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 DAC3 Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 DAC3 Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 DAC3 Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 DAC3 Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 DAC3 Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 DAC3 Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 DAC3 Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 DAC3 Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 DAC3 Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 DAC3 Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 DAC3 Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 DAC3 Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 DAC3 Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 DAC3 Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 DAC3 Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 DAC3 Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 DAC3 Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 DAC3 Source Selector", "SRCO3", "AK4602 SRC3"},

	{"AK4602 VOL1 Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 VOL1 Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 VOL1 Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 VOL1 Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 VOL1 Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 VOL1 Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 VOL1 Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 VOL1 Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 VOL1 Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 VOL1 Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 VOL1 Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 VOL1 Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 VOL1 Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 VOL1 Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 VOL1 Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 VOL1 Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 VOL1 Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 VOL1 Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 VOL1 Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 VOL1 Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 VOL1 Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 VOL1 Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 VOL1 Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 VOL1 Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 VOL1 Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 VOL1 Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 VOL1 Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 VOL1 Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 VOL1 Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 VOL1 Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 VOL1 Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 VOL1 Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 VOL1 Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 VOL1 Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 VOL1 Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 VOL1 Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 VOL1 Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 VOL1 Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 VOL1 Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 VOL1 Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 VOL1 Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 VOL1 Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 VOL1 Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 VOL1 Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 VOL1 Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 VOL1 Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 VOL1 Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 VOL1 Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 VOL1 Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 VOL1 Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 VOL1 Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 VOL1 Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 VOL1 Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 VOL1 Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 VOL2 Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 VOL2 Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 VOL2 Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 VOL2 Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 VOL2 Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 VOL2 Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 VOL2 Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 VOL2 Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 VOL2 Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 VOL2 Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 VOL2 Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 VOL2 Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 VOL2 Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 VOL2 Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 VOL2 Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 VOL2 Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 VOL2 Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 VOL2 Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 VOL2 Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 VOL2 Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 VOL2 Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 VOL2 Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 VOL2 Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 VOL2 Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 VOL2 Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 VOL2 Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 VOL2 Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 VOL2 Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 VOL2 Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 VOL2 Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 VOL2 Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 VOL2 Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 VOL2 Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 VOL2 Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 VOL2 Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 VOL2 Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 VOL2 Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 VOL2 Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 VOL2 Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 VOL2 Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 VOL2 Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 VOL2 Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 VOL2 Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 VOL2 Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 VOL2 Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 VOL2 Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 VOL2 Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 VOL2 Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 VOL2 Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 VOL2 Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 VOL2 Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 VOL2 Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 VOL2 Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 VOL2 Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 VOL3 Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 VOL3 Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 VOL3 Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 VOL3 Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 VOL3 Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 VOL3 Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 VOL3 Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 VOL3 Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 VOL3 Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 VOL3 Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 VOL3 Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 VOL3 Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 VOL3 Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 VOL3 Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 VOL3 Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 VOL3 Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 VOL3 Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 VOL3 Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 VOL3 Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 VOL3 Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 VOL3 Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 VOL3 Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 VOL3 Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 VOL3 Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 VOL3 Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 VOL3 Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 VOL3 Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 VOL3 Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 VOL3 Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 VOL3 Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 VOL3 Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 VOL3 Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 VOL3 Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 VOL3 Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 VOL3 Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 VOL3 Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 VOL3 Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 VOL3 Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 VOL3 Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 VOL3 Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 VOL3 Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 VOL3 Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 VOL3 Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 VOL3 Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 VOL3 Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 VOL3 Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 VOL3 Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 VOL3 Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 VOL3 Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 VOL3 Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 VOL3 Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 VOL3 Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 VOL3 Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 VOL3 Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 VOL4 Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 VOL4 Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 VOL4 Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 VOL4 Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 VOL4 Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 VOL4 Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 VOL4 Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 VOL4 Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 VOL4 Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 VOL4 Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 VOL4 Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 VOL4 Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 VOL4 Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 VOL4 Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 VOL4 Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 VOL4 Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 VOL4 Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 VOL4 Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 VOL4 Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 VOL4 Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 VOL4 Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 VOL4 Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 VOL4 Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 VOL4 Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 VOL4 Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 VOL4 Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 VOL4 Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 VOL4 Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 VOL4 Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 VOL4 Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 VOL4 Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 VOL4 Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 VOL4 Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 VOL4 Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 VOL4 Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 VOL4 Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 VOL4 Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 VOL4 Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 VOL4 Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 VOL4 Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 VOL4 Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 VOL4 Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 VOL4 Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 VOL4 Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 VOL4 Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 VOL4 Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 VOL4 Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 VOL4 Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 VOL4 Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 VOL4 Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 VOL4 Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 VOL4 Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 VOL4 Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 VOL4 Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 VOL5 Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 VOL5 Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 VOL5 Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 VOL5 Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 VOL5 Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 VOL5 Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 VOL5 Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 VOL5 Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 VOL5 Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 VOL5 Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 VOL5 Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 VOL5 Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 VOL5 Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 VOL5 Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 VOL5 Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 VOL5 Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 VOL5 Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 VOL5 Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 VOL5 Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 VOL5 Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 VOL5 Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 VOL5 Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 VOL5 Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 VOL5 Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 VOL5 Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 VOL5 Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 VOL5 Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 VOL5 Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 VOL5 Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 VOL5 Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 VOL5 Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 VOL5 Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 VOL5 Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 VOL5 Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 VOL5 Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 VOL5 Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 VOL5 Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 VOL5 Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 VOL5 Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 VOL5 Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 VOL5 Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 VOL5 Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 VOL5 Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 VOL5 Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 VOL5 Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 VOL5 Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 VOL5 Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 VOL5 Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 VOL5 Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 VOL5 Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 VOL5 Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 VOL5 Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 VOL5 Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 VOL5 Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 MixerA1 Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 MixerA1 Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 MixerA1 Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 MixerA1 Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 MixerA1 Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 MixerA1 Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 MixerA1 Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 MixerA1 Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 MixerA1 Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 MixerA1 Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 MixerA1 Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 MixerA1 Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 MixerA1 Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 MixerA1 Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 MixerA1 Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 MixerA1 Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 MixerA1 Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 MixerA1 Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 MixerA1 Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 MixerA1 Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 MixerA1 Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 MixerA1 Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 MixerA1 Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 MixerA1 Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 MixerA1 Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 MixerA1 Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 MixerA1 Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 MixerA1 Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 MixerA1 Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 MixerA1 Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 MixerA1 Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 MixerA1 Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 MixerA1 Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 MixerA1 Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 MixerA1 Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 MixerA1 Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 MixerA1 Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 MixerA1 Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 MixerA1 Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 MixerA1 Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 MixerA1 Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 MixerA1 Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 MixerA1 Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 MixerA1 Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 MixerA1 Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 MixerA1 Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 MixerA1 Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 MixerA1 Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 MixerA1 Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 MixerA1 Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 MixerA1 Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 MixerA1 Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 MixerA1 Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 MixerA1 Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 MixerA2 Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 MixerA2 Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 MixerA2 Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 MixerA2 Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 MixerA2 Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 MixerA2 Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 MixerA2 Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 MixerA2 Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 MixerA2 Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 MixerA2 Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 MixerA2 Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 MixerA2 Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 MixerA2 Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 MixerA2 Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 MixerA2 Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 MixerA2 Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 MixerA2 Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 MixerA2 Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 MixerA2 Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 MixerA2 Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 MixerA2 Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 MixerA2 Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 MixerA2 Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 MixerA2 Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 MixerA2 Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 MixerA2 Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 MixerA2 Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 MixerA2 Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 MixerA2 Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 MixerA2 Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 MixerA2 Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 MixerA2 Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 MixerA2 Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 MixerA2 Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 MixerA2 Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 MixerA2 Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 MixerA2 Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 MixerA2 Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 MixerA2 Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 MixerA2 Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 MixerA2 Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 MixerA2 Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 MixerA2 Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 MixerA2 Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 MixerA2 Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 MixerA2 Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 MixerA2 Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 MixerA2 Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 MixerA2 Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 MixerA2 Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 MixerA2 Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 MixerA2 Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 MixerA2 Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 MixerA2 Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 MixerB1 Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 MixerB1 Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 MixerB1 Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 MixerB1 Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 MixerB1 Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 MixerB1 Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 MixerB1 Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 MixerB1 Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 MixerB1 Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 MixerB1 Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 MixerB1 Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 MixerB1 Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 MixerB1 Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 MixerB1 Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 MixerB1 Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 MixerB1 Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 MixerB1 Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 MixerB1 Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 MixerB1 Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 MixerB1 Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 MixerB1 Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 MixerB1 Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 MixerB1 Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 MixerB1 Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 MixerB1 Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 MixerB1 Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 MixerB1 Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 MixerB1 Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 MixerB1 Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 MixerB1 Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 MixerB1 Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 MixerB1 Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 MixerB1 Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 MixerB1 Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 MixerB1 Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 MixerB1 Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 MixerB1 Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 MixerB1 Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 MixerB1 Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 MixerB1 Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 MixerB1 Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 MixerB1 Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 MixerB1 Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 MixerB1 Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 MixerB1 Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 MixerB1 Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 MixerB1 Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 MixerB1 Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 MixerB1 Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 MixerB1 Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 MixerB1 Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 MixerB1 Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 MixerB1 Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 MixerB1 Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 MixerB2 Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 MixerB2 Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 MixerB2 Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 MixerB2 Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 MixerB2 Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 MixerB2 Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 MixerB2 Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 MixerB2 Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 MixerB2 Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 MixerB2 Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 MixerB2 Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 MixerB2 Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 MixerB2 Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 MixerB2 Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 MixerB2 Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 MixerB2 Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 MixerB2 Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 MixerB2 Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 MixerB2 Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 MixerB2 Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 MixerB2 Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 MixerB2 Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 MixerB2 Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 MixerB2 Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 MixerB2 Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 MixerB2 Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 MixerB2 Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 MixerB2 Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 MixerB2 Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 MixerB2 Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 MixerB2 Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 MixerB2 Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 MixerB2 Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 MixerB2 Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 MixerB2 Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 MixerB2 Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 MixerB2 Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 MixerB2 Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 MixerB2 Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 MixerB2 Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 MixerB2 Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 MixerB2 Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 MixerB2 Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 MixerB2 Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 MixerB2 Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 MixerB2 Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 MixerB2 Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 MixerB2 Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 MixerB2 Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 MixerB2 Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 MixerB2 Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 MixerB2 Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 MixerB2 Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 MixerB2 Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SRC1 Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SRC1 Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SRC1 Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SRC1 Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SRC1 Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SRC1 Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SRC1 Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SRC1 Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SRC1 Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SRC1 Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SRC1 Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SRC1 Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SRC1 Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SRC1 Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SRC1 Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SRC1 Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SRC1 Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SRC1 Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SRC1 Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SRC1 Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SRC1 Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SRC1 Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SRC1 Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SRC1 Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SRC1 Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SRC1 Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SRC1 Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SRC1 Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SRC1 Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SRC1 Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SRC1 Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SRC1 Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SRC1 Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SRC1 Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SRC1 Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SRC1 Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SRC1 Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SRC1 Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SRC1 Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SRC1 Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SRC1 Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SRC1 Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SRC1 Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SRC1 Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SRC1 Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SRC1 Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SRC1 Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SRC1 Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SRC1 Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SRC1 Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SRC1 Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SRC1 Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SRC1 Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SRC1 Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SRC2 Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SRC2 Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SRC2 Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SRC2 Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SRC2 Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SRC2 Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SRC2 Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SRC2 Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SRC2 Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SRC2 Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SRC2 Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SRC2 Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SRC2 Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SRC2 Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SRC2 Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SRC2 Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SRC2 Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SRC2 Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SRC2 Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SRC2 Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SRC2 Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SRC2 Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SRC2 Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SRC2 Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SRC2 Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SRC2 Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SRC2 Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SRC2 Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SRC2 Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SRC2 Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SRC2 Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SRC2 Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SRC2 Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 SRC2 Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 SRC2 Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 SRC2 Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 SRC2 Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 SRC2 Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 SRC2 Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 SRC2 Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 SRC2 Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SRC2 Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SRC2 Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SRC2 Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SRC2 Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SRC2 Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SRC2 Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SRC2 Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SRC2 Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SRC2 Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SRC2 Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SRC2 Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SRC2 Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SRC2 Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SRC3 Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SRC3 Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SRC3 Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SRC3 Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SRC3 Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SRC3 Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SRC3 Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SRC3 Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SRC3 Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SRC3 Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SRC3 Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SRC3 Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SRC3 Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SRC3 Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SRC3 Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SRC3 Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SRC3 Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SRC3 Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SRC3 Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SRC3 Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SRC3 Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SRC3 Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SRC3 Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SRC3 Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SRC3 Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SRC3 Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SRC3 Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SRC3 Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SRC3 Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SRC3 Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SRC3 Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SRC3 Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SRC3 Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SRC3 Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SRC3 Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SRC3 Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SRC3 Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SRC3 Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SRC3 Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SRC3 Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SRC3 Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SRC3 Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SRC3 Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SRC3 Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SRC3 Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SRC3 Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SRC3 Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SRC3 Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SRC3 Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SRC3 Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SRC3 Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SRC3 Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SRC3 Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SRC3 Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 SRC4 Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 SRC4 Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 SRC4 Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 SRC4 Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 SRC4 Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 SRC4 Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 SRC4 Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 SRC4 Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 SRC4 Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 SRC4 Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 SRC4 Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 SRC4 Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 SRC4 Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 SRC4 Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 SRC4 Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 SRC4 Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 SRC4 Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 SRC4 Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 SRC4 Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 SRC4 Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 SRC4 Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 SRC4 Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 SRC4 Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 SRC4 Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 SRC4 Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SRC4 Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SRC4 Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SRC4 Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SRC4 Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SRC4 Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SRC4 Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SRC4 Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SRC4 Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 SRC4 Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 SRC4 Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 SRC4 Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 SRC4 Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 SRC4 Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 SRC4 Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 SRC4 Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 SRC4 Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 SRC4 Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 SRC4 Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 SRC4 Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 SRC4 Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 SRC4 Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 SRC4 Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 SRC4 Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 SRC4 Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 SRC4 Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 SRC4 Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 SRC4 Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 SRC4 Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 SRC4 Source Selector", "SRCO4", "AK4602 SRC4"},

	{"AK4602 DIT Source Selector", "SDIN1A", "AK4602 SDIN1"},
	{"AK4602 DIT Source Selector", "SDIN1B", "AK4602 SDIN1"},
	{"AK4602 DIT Source Selector", "SDIN1C", "AK4602 SDIN1"},
	{"AK4602 DIT Source Selector", "SDIN1D", "AK4602 SDIN1"},
	{"AK4602 DIT Source Selector", "SDIN1E", "AK4602 SDIN1"},
	{"AK4602 DIT Source Selector", "SDIN1F", "AK4602 SDIN1"},
	{"AK4602 DIT Source Selector", "SDIN1G", "AK4602 SDIN1"},
	{"AK4602 DIT Source Selector", "SDIN1H", "AK4602 SDIN1"},
	{"AK4602 DIT Source Selector", "SDIN2A", "AK4602 SDIN2"},
	{"AK4602 DIT Source Selector", "SDIN2B", "AK4602 SDIN2"},
	{"AK4602 DIT Source Selector", "SDIN2C", "AK4602 SDIN2"},
	{"AK4602 DIT Source Selector", "SDIN2D", "AK4602 SDIN2"},
	{"AK4602 DIT Source Selector", "SDIN2E", "AK4602 SDIN2"},
	{"AK4602 DIT Source Selector", "SDIN2F", "AK4602 SDIN2"},
	{"AK4602 DIT Source Selector", "SDIN2G", "AK4602 SDIN2"},
	{"AK4602 DIT Source Selector", "SDIN2H", "AK4602 SDIN2"},
	{"AK4602 DIT Source Selector", "SDIN3A", "AK4602 SDIN3"},
	{"AK4602 DIT Source Selector", "SDIN3B", "AK4602 SDIN3"},
	{"AK4602 DIT Source Selector", "SDIN3C", "AK4602 SDIN3"},
	{"AK4602 DIT Source Selector", "SDIN3D", "AK4602 SDIN3"},
	{"AK4602 DIT Source Selector", "SDIN3E", "AK4602 SDIN3"},
	{"AK4602 DIT Source Selector", "SDIN3F", "AK4602 SDIN3"},
	{"AK4602 DIT Source Selector", "SDIN3G", "AK4602 SDIN3"},
	{"AK4602 DIT Source Selector", "SDIN3H", "AK4602 SDIN3"},
	{"AK4602 DIT Source Selector", "SDIN4A", "AK4602 SDIN4"},
	{"AK4602 DIT Source Selector", "SDIN4B", "AK4602 SDIN4"},
	{"AK4602 DIT Source Selector", "SDIN4C", "AK4602 SDIN4"},
	{"AK4602 DIT Source Selector", "SDIN4D", "AK4602 SDIN4"},
	{"AK4602 DIT Source Selector", "SDIN4E", "AK4602 SDIN4"},
	{"AK4602 DIT Source Selector", "SDIN4F", "AK4602 SDIN4"},
	{"AK4602 DIT Source Selector", "SDIN4G", "AK4602 SDIN4"},
	{"AK4602 DIT Source Selector", "SDIN4H", "AK4602 SDIN4"},
	{"AK4602 DIT Source Selector", "SDIN5A", "AK4602 SDIN5"},
	{"AK4602 DIT Source Selector", "SDIN5B", "AK4602 SDIN5"},
	{"AK4602 DIT Source Selector", "SDIN5C", "AK4602 SDIN5"},
	{"AK4602 DIT Source Selector", "SDIN5D", "AK4602 SDIN5"},
	{"AK4602 DIT Source Selector", "SDIN5E", "AK4602 SDIN5"},
	{"AK4602 DIT Source Selector", "SDIN5F", "AK4602 SDIN5"},
	{"AK4602 DIT Source Selector", "SDIN5G", "AK4602 SDIN5"},
	{"AK4602 DIT Source Selector", "SDIN5H", "AK4602 SDIN5"},
	{"AK4602 DIT Source Selector", "VOLO1", "AK4602 VOL1"},
	{"AK4602 DIT Source Selector", "VOLO2", "AK4602 VOL2"},
	{"AK4602 DIT Source Selector", "VOLO3", "AK4602 VOL3"},
	{"AK4602 DIT Source Selector", "VOLO4", "AK4602 VOL4"},
	{"AK4602 DIT Source Selector", "VOLO5", "AK4602 VOL5"},
	{"AK4602 DIT Source Selector", "ADC1", "AK4602 ADC1"},
	{"AK4602 DIT Source Selector", "ADC2", "AK4602 ADC2"},
	{"AK4602 DIT Source Selector", "ADCM", "AK4602 ADCM"},
	{"AK4602 DIT Source Selector", "MixerA", "AK4602 MixerA"},
	{"AK4602 DIT Source Selector", "MixerB", "AK4602 MixerB"},
	{"AK4602 DIT Source Selector", "SRCO1", "AK4602 SRC1"},
	{"AK4602 DIT Source Selector", "SRCO2", "AK4602 SRC2"},
	{"AK4602 DIT Source Selector", "SRCO3", "AK4602 SRC3"},
	{"AK4602 DIT Source Selector", "SRCO4", "AK4602 SRC4"},
};

static bool ak4602_readable(struct device *dev, unsigned int reg)
{
	bool ret;

	if (reg <= AK4602_MAX_REGISTER)
		ret = true;
	else
		ret = false;

	return ret;

}


static bool ak4602_volatile(struct device *dev, unsigned int reg)
{
	bool	ret;

#ifdef AK4602_DEBUG
	if (reg <= AK4602_MAX_REGISTER)
		ret = true;
	else
		ret = false;
#else
	switch (reg) {
	case AK4602_102_STATUS_READ:
	case AK4602_103_SRC_STATUS_1:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}
#endif
	return(ret);
}

static bool ak4602_writeable(struct device *dev, unsigned int reg)
{
	bool ret;

	if ((reg >= AK4602_13_RESERVED) &&  (reg <= AK4602_14_RESERVED))
		ret = false;
	else if ((reg >= AK4602_1C_RESERVED) &&  (reg <= AK4602_1E_RESERVED))
		ret = false;
	else if ((reg >= AK4602_5E_RESERVED) &&  (reg <= AK4602_61_RESERVED))
		ret = false;
	else if ((reg >= AK4602_67_RESERVED) &&  (reg <= AK4602_68_RESERVED))
		ret = false;
	else if (reg == AK4602_6C_RESERVED)
		ret = false;
	else if ((reg >= AK4602_78_RESERVED) &&  (reg <= AK4602_79_RESERVED))
		ret = false;
	else if (reg == AK4602_7C_RESERVED)
		ret = false;
	else if (reg == AK4602_9D_RESERVED)
		ret = false;
	else if (reg == AK4602_A6_RESERVED)
		ret = false;
	else if (reg > AK4602_A9_RESET_CONTROL)
		ret = false;
	else
		ret = true;

	return ret;
}

#ifdef AK4602_I2C_IF
static int ak4602_i2c_read(
struct i2c_client *client,
u8 *reg,
int reglen,
u8 *data,
int datalen)
{
	struct i2c_msg xfer[2];
	int ret;

	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = reglen;
	xfer[0].buf = reg;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = datalen;
	xfer[1].buf = data;

	ret = i2c_transfer(client->adapter, xfer, 2);

	if (ret == 2)
		return 0;
	else
		return ret;
}
#endif

static int ak4602_read(void *context, unsigned int reg, unsigned int *val)
{
	struct device *dev = context;
	struct ak4602_priv *ak4602;
	unsigned char tx[3], rx[1];
	int	wlen, rlen;
	int ret;

	if (reg == SND_SOC_NOPM)
		return 0;

	ak4602 = dev_get_drvdata(dev);

	tx[0] = 0x40;
	tx[1] = (unsigned char)(0xFF & (reg >> 8));
	tx[2] = (unsigned char)(0xFF & reg);
	wlen = 3;
	rlen = 1;

#ifdef AK4602_I2C_IF
	ret = ak4602_i2c_read(ak4602->i2c, tx, wlen, rx, rlen);
#else
	ret = spi_write_then_read(ak4602->spi, tx, wlen, rx, rlen);
#endif

	if (ret < 0) {
		pr_err("\t[AK4602] %s error ret = %d\n", __func__, ret);
		rx[0] = 0;
	}

	*val = (unsigned int)rx[0];
	akdbgprt("[AK4602] %s addr, read data =(%x, %x)\n", __func__, reg, *val);

	return 0;

}

static int ak4602_write(void *context, unsigned int reg, unsigned int value)
{
	struct device *dev = context;
	struct ak4602_priv *ak4602;
	unsigned char tx[4];
	int wlen;
	int ret;


	if (reg == SND_SOC_NOPM)
		return 0;

	ak4602 = dev_get_drvdata(dev);

	wlen = 4;
	tx[0] = 0xC0;
	tx[1] = (unsigned char)(0xFF & (reg >> 8));
	tx[2] = (unsigned char)(0xFF & reg);
	tx[3] = (unsigned char)value;

#ifdef AK4602_I2C_IF
	ret = i2c_master_send(ak4602->i2c, tx, wlen);
	if (ret == wlen) {
		ret = 0;
	} else {
		akdbgprt("\t[AK4602] %s error ret != wlen\n", __func__);
		return -EIO;
	}
#else
	ret = spi_write(ak4602->spi, tx, wlen);
#endif
	akdbgprt("\t[AK4602] %s error ret = %d\n", __func__, ret);

	if (ret < 0) {
		akdbgprt("\t[AK4602] %s error ret = %d\n", __func__, ret);
		return ret;
	}

	return 0;

}

static int ak4602_set_bias_level(struct snd_soc_component *component,
				 enum snd_soc_bias_level level)
{
//	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
//	struct ak460x *control = codec->control_data;
#ifdef TCC_USE_MUTE_GPIO
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
#endif	//TCC_USE_MUTE_GPIO

	switch (level) {
	case SND_SOC_BIAS_ON:
#ifdef TCC_USE_MUTE_GPIO
		if (ak4602->playback_active) {
			mdelay(2);
			if (ak4602->cmute_gpio >= 0) {
				gpio_set_value(ak4602->cmute_gpio, !ak4602->cmute_gpio_flags); //cmute off
				akdbgprt("\t[AK4602] SND_SOC_BIAS_ON cmute_gpio unmute\n");
			}
			if (ak4602->amute_gpio >= 0) {
				gpio_set_value(ak4602->amute_gpio, !ak4602->amute_gpio_flags); //amute off
				akdbgprt("\t[AK4602] SND_SOC_BIAS_ON amute_gpio unmute\n");
			}
		}
#endif//TCC_USE_MUTE_GPIO
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
#ifdef TCC_USE_MUTE_GPIO
		if (snd_soc_component_get_bias_level(component) == SND_SOC_BIAS_OFF) {
			if (ak4602->stanby_gpio >= 0) {
				gpio_set_value(ak4602->stanby_gpio, ak4602->stanby_gpio_flags); //stanby on
				akdbgprt("\t[AK4602] SND_SOC_BIAS_PREPARE stanby on\n");
			}
		}
#endif//TCC_USE_MUTE_GPIO
		break;

	case SND_SOC_BIAS_OFF:
#ifdef TCC_USE_MUTE_GPIO
		if (ak4602->stanby_gpio >= 0) {
			gpio_set_value(ak4602->stanby_gpio, !ak4602->stanby_gpio_flags); //stanby off
			akdbgprt("\t[AK4602] SND_SOC_BIAS_OFF stanby off\n");
		}
#endif//TCC_USE_MUTE_GPIO
		break;
	}
	component->dapm.bias_level = level;

	return 0;
}

#ifndef AK4602_I2C_IF
static int ak4602_spi_dummy_command(struct snd_soc_component *component)
{
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	unsigned char tx[4];
	int wlen;
	int rd;

	rd = 0;
	wlen = 4;
	tx[0] = 0xDE;
	tx[1] = 0xAD;
	tx[2] = 0xDA;
	tx[3] = 0x7A;

	rd = spi_write(ak4602->spi, tx, wlen);

	return rd;
}
#endif

static int ak4602_init_reg(struct snd_soc_component *component)
{
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int i;

	akdbgprt("\t[AK4602] %s\n", __func__);

	if (ak4602->pdn_gpio > 0) {    // 170719
		gpio_set_value(ak4602->pdn_gpio, GPO_PDN_LOW);
		mdelay(1);
		gpio_set_value(ak4602->pdn_gpio, GPO_PDN_HIGH);
		mdelay(1);
	}

#ifndef AK4602_I2C_IF
	ak4602_spi_dummy_command(component);
#endif

	for (i = 0; i < NUM_SYNCDOMAIN; i++) {
		ak4602->Master[i] = 0; //slave
		ak4602->tdmsdin[i] = 0; //SDIN 1/2/3/4/5 Stereo mode
		ak4602->tdmsdout[i] = 0; //SDOUT 1/2/3/4/5 Stereo Mode
		ak4602->SlotLenIn[i] = 3; //SDIN 1/2/3/4/5 slot 32bit
		ak4602->SlotLenOut[i] = 3; //SDOUT 1/2/3/4/5 slot 32bit
	}
#ifndef TCC_INITIAL_SETTING
	ak4602->Master[NUM_SYNCDOMAIN-1] = 1;
#endif
	for (i = 0; i < NUM_SYNCDOMAIN; i++) {
		ak4602->SDBick[i]  = 0; //64fs
		ak4602->SDfs[i] = 5;  // 48kHz
		ak4602->SDCks[i] = 0;  // Low
	}

	ak4602->I2Smode2 = 0;

	snd_soc_component_update_bits(component, AK4602_00_SYSTEM_CLOCK_1, 0x1F, 0xF);	// 12.288MHz
	snd_soc_component_update_bits(component, AK4602_19_CLOCK_SD_SEL_3, 0x7, 0x1);	// SDIN1_BICK : BICK1
	snd_soc_component_update_bits(component, AK4602_1A_EXTCLOCK_1, 0x77, 0x23);		// SDIN2_BICK : BICK2, SDIN3_BICK : BICK3
	snd_soc_component_update_bits(component, AK4602_1B_EXTCLOCK_2, 0x70,  0x40);	// SDIN4_BICK : BICK4
#ifdef TCC_INITIAL_SETTING
	snd_soc_component_update_bits(component, AK4602_00_SYSTEM_CLOCK_1, 0x1F, 0x9);
	// 3.072MHz
	snd_soc_component_update_bits(component, AK4602_01_SYSTEM_CLOCK_2, 0xF, 0x1);
	// Ref. CLK BICK1
	snd_soc_component_update_bits(component, AK4602_02_SYSTEM_CLOCK_3, 0x1F, 0x7);
	// Codec Sampling Freq. 48Khz:48Khz
	//snd_soc_component_update_bits(component, AK4602_05_CLOCK_SD1_SET_2, 0x1F, 0x4);
	ak4602->SDCks[0] = 3;	//BICK1
	// Sync Domain 1 Clock SRC. : BICK1
	ak4602->SDCks[1] = 4;	//BICK2
	// Sync Domain 2 Clock SRC. : BICK2
	ak4602->SDfs[0] = 5;	//48kHz
	// Sync Domain 1 fs : 48kHz
	ak4602->SDfs[1] = 5;	//48kHz
	// Sync Domain 2 fs : 48kHz
	snd_soc_component_update_bits(component, AK4602_04_CLOCK_SD1_SET_1, 0xFF, 0x88);
	// BCLK1/LRCK1 Master : Slave
	// BCLK1/LRCK1 Pulled Down Enable : off
	// Sync Domain 1 BICK fs : 64fs
	snd_soc_component_update_bits(component, AK4602_07_CLOCK_SD2_SET_1, 0xFF, 0x88);
	// BCLK2/LRCK2 Master : Slave
	// BCLK2/LRCK2 Pulled Down Enable : off
	// Sync Domain 2 BICK fs : 64fs

	snd_soc_component_update_bits(component, AK4602_19_CLOCK_SD_SEL_3, 0x7, 0x1);
	// SDIN1_BICK/LRCK : BICK1/LRCK1
	snd_soc_component_update_bits(component, AK4602_1A_EXTCLOCK_1, 0x77, 0x11);
	// SDIN2_BICK/LRCK : BICK1/LRCK1
	// SDIN3_BICK/LRCK : BICK1/LRCK1
	snd_soc_component_update_bits(component, AK4602_17_CLOCK_SD_SEL_1, 0x77, 0x12);
	// BICK1 Sync Domain : SD1
	// BICK2 Sync Domain : SD2

	snd_soc_component_update_bits(component, AK4602_1F_CLOCK_SD_SEL_7, 0x77, 0x22);
	//SDOUT1 Sync Domain : SD2
	//SDOUT2 Sync Domain : SD2
	snd_soc_component_update_bits(component, AK4602_29_CLOCK_SD_SEL_17, 0x77, 0x21);
	//ADC1 Sync Domain : SD2 for MIC
	//CODEC(ADC2, ADCM) Sync Domain : SD1 for AUX (If SD2, SD1&2 shoud be same sample rate)

	snd_soc_component_update_bits(component, AK4602_52_DAC1_INPUT_DATA, 0x3F, 0x1);
	//DAC1 SRC Sel : SDIN1A
	snd_soc_component_update_bits(component, AK4602_53_DAC2_INPUT_DATA, 0x3F, 0x9);
	//DAC2 SRC Sel : SDIN2A
	snd_soc_component_update_bits(component, AK4602_54_DAC3_INPUT_DATA, 0x3F, 0x11);
	//DAC3 SRC Sel : SDIN3A
	snd_soc_component_update_bits(component, AK4602_8E_DAC_MUTE_FILTER, 0x70, 0x0);
	//DAC1/2/3 Mute : off
	snd_soc_component_update_bits(component, AK4602_88_DAC1_LCH_DIGITAL_VOL, 0xFF, 0x18);
	//DAC1 Digital Vol L: 231 0dB
	snd_soc_component_update_bits(component, AK4602_89_DAC1_RCH_DIGITAL_VOL, 0xFF, 0x18);
	//DAC1 Digital Vol R: 231 0dB
	snd_soc_component_update_bits(component, AK4602_8A_DAC2_LCH_DIGITAL_VOL, 0xFF, 0x18);
	//DAC2 Digital Vol L: 231 0dB
	snd_soc_component_update_bits(component, AK4602_8B_DAC2_RCH_DIGITAL_VOL, 0xFF, 0x18);
	//DAC2 Digital Vol R: 231 0dB
	snd_soc_component_update_bits(component, AK4602_8C_DAC3_LCH_DIGITAL_VOL, 0xFF, 0x18);
	//DAC3 Digital Vol L: 231 0dB
	snd_soc_component_update_bits(component, AK4602_8D_DAC3_RCH_DIGITAL_VOL, 0xFF, 0x18);
	//DAC3 Digital Vol R: 231 0dB

	snd_soc_component_update_bits(component, AK4602_7A_OUTPUT_PORT, 0x80, 0x0);
	//SDOUT1/DIT pin Setting : SDOUT1
	snd_soc_component_update_bits(component, AK4602_86_ANALOG_INPUT_SELECT, 0x0F, 0xD);
	//ADC1 Lch Input Pin Select : AIN1L Single
	//ADC1 Rch Input Pin Select : AIN1R Single
	//ADC2 MUX : AIN3
	snd_soc_component_update_bits(component, AK4602_87_ADC_MUTE_HPF_CONTROL, 0x60, 0x0);
	//ADC1/2 Mute : off
	snd_soc_component_update_bits(component, AK4602_2A_SDOUT1_TDM_SLOT1_2, 0x3F, 0x2F);
	//SDOUT1A Source Selector : ADC2 for AUX
	snd_soc_component_update_bits(component, AK4602_32_SDOUT2_TDM_SLOT1_2, 0x3F, 0x2E);
	//SDOUT2A Source Selector : ADC1 for MainCluster MIC (MC_MIC path)
#endif
#ifdef TCC_ALWAYS_PMDAC_ON
	snd_soc_component_update_bits(component, AK4602_A7_POWER_MANAGEMENT_1, 0x7, 0x7);
	//DAC1,2,3 On
#endif
#ifdef TCC_USE_MUTE_GPIO
	ak4602->playback_active = 0;
#endif//TCC_USE_MUTE_GPIO
#ifdef TCC_CODEC_SAMPLING_FREQ
	ak4602->codec_freq = 48000;
	ak4602->adc1_freq = 48000;
	ak4602->adc2 = false;
#endif
	return 0;
}

static int ak4602_parse_dt(struct ak4602_priv *ak4602)
{
	struct device *dev;
	struct device_node *np;
#ifdef TCC_USE_MUTE_GPIO
	enum of_gpio_flags gpio_flags;
#endif//TCC_USE_MUTE_GPIO

#ifdef AK4602_I2C_IF
	dev = &(ak4602->i2c->dev);
#else
	dev = &(ak4602->spi->dev);
#endif

	np = dev->of_node;

	if (!np)
		return 0;

#ifdef TCC_USE_MUTE_GPIO
	ak4602->cmute_gpio = of_get_named_gpio_flags(np, "cmute-gpios", 0, &gpio_flags);
	if(ak4602->cmute_gpio < 0) {
		ak4602->cmute_gpio = -1;
	} else {
		if(gpio_is_valid(ak4602->cmute_gpio)) {
			ak4602->cmute_gpio_flags = (gpio_flags & OF_GPIO_ACTIVE_LOW)? 0 : 1;
			akdbgprt("[AK4602] use cmute gpio(%d)\n", ak4602->cmute_gpio);
			if (devm_gpio_request_one(dev,
						ak4602->cmute_gpio,
						(ak4602->cmute_gpio_flags ? GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW),
						"cmute-gpios")) {
				ak4602->cmute_gpio = -1;
				akdbgprt("[AK4602] request cmute gpio(%d) failed\n", ak4602->cmute_gpio);
			}
		} else {
			ak4602->cmute_gpio = -1;
			akdbgprt("[AK4602] cmute gpio(%d) is invalid\n", ak4602->cmute_gpio);
		}
	}
	ak4602->amute_gpio = of_get_named_gpio_flags(np, "amute-gpios", 0, &gpio_flags);
	if(ak4602->amute_gpio < 0) {
		ak4602->amute_gpio = -1;
	} else {
		if(gpio_is_valid(ak4602->amute_gpio)) {
			ak4602->amute_gpio_flags = (gpio_flags & OF_GPIO_ACTIVE_LOW)? 0 : 1;
			akdbgprt("[AK4602] use amute gpio(%d)\n", ak4602->amute_gpio);
			if (devm_gpio_request_one(dev,
						ak4602->amute_gpio,
						(ak4602->amute_gpio_flags ? GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW),
						"amute-gpios")) {
				ak4602->amute_gpio = -1;
				akdbgprt("[AK4602] request amute gpio(%d) failed\n", ak4602->amute_gpio);
			}
		} else {
			ak4602->amute_gpio = -1;
			akdbgprt("[AK4602] amute gpio(%d) is invalid\n", ak4602->amute_gpio);
		}
	}
	ak4602->stanby_gpio = of_get_named_gpio_flags(np, "stanby-gpios", 0, &gpio_flags);
	if(ak4602->stanby_gpio < 0) {
		ak4602->stanby_gpio = -1;
	} else {
		if(gpio_is_valid(ak4602->stanby_gpio)) {
			ak4602->stanby_gpio_flags = (gpio_flags & OF_GPIO_ACTIVE_LOW)? 0 : 1;
			akdbgprt("[AK4602] use stanby gpio(%d)\n", ak4602->stanby_gpio);
			if (devm_gpio_request_one(dev,
						ak4602->stanby_gpio,
						(ak4602->stanby_gpio_flags ? GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW),
						"stanby-gpios")) {
				ak4602->stanby_gpio = -1;
				akdbgprt("[AK4602] request stanby gpio(%d) failed\n", ak4602->stanby_gpio);
			}
		} else {
			ak4602->stanby_gpio = -1;
			akdbgprt("[AK4602] stanby gpio(%d) is invalid\n", ak4602->stanby_gpio);
		}
	}
#endif//TCC_USE_MUTE_GPIO
	akdbgprt("Read PDN pin from device tree\n");

	ak4602->pdn_gpio = of_get_named_gpio(np, "ak4602,pdn-gpio", 0);
	if (ak4602->pdn_gpio < 0) {
		ak4602->pdn_gpio = -1;
		return 0;
	}

	if (!gpio_is_valid(ak4602->pdn_gpio)) {
		akdbgprt(KERN_ERR "ak4602 pdn pin(%u) is invalid\n", ak4602->pdn_gpio);
		return 0;
	}
	ak4602->is_updated = false;
	ak4602->dai_fmt = 0;

	return 0;
}

static int ak4602_probe(struct snd_soc_component *component)
{
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);
	int ret = 0;

	akdbgprt("\t[AK4602] %s(%d)\n", __func__, __LINE__);

	ret = ak4602_parse_dt(ak4602);
	if (ak4602->pdn_gpio != -1) {
		ret = gpio_request(ak4602->pdn_gpio, "ak4602 pdn");
		akdbgprt("\t[AK4602] %s : gpio_request ret = %d\n", __func__, ret);
		gpio_direction_output(ak4602->pdn_gpio, 0);
	}

	ak4602_init_reg(component);

	return ret;
}

static void ak4602_remove(struct snd_soc_component *component)
{
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	akdbgprt("\t[AK4602] %s(%d)\n", __func__, __LINE__);

	ak4602_set_bias_level(component, SND_SOC_BIAS_OFF);
	if (ak4602->pdn_gpio > 0) {
		gpio_set_value(ak4602->pdn_gpio, GPO_PDN_LOW);
		mdelay(1);
		gpio_free(ak4602->pdn_gpio);
		mdelay(1);
	}
}

static int ak4602_suspend(struct snd_soc_component *component)
{
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	akdbgprt("\t[AK4602] %s(%d)\n", __func__, __LINE__);

#ifdef TCC_USE_MUTE_GPIO
	if (ak4602->cmute_gpio >= 0) {
		gpio_set_value(ak4602->cmute_gpio, ak4602->cmute_gpio_flags); //cmute on
		akdbgprt("\t[AK4602] cmute_gpio mute(%d)\n",__LINE__);
		gpio_free(ak4602->cmute_gpio);
	}
	if (ak4602->amute_gpio >= 0) {
		gpio_set_value(ak4602->amute_gpio, ak4602->amute_gpio_flags); //amute on
		akdbgprt("\t[AK4602] amute_gpio mute(%d)\n",__LINE__);
		gpio_free(ak4602->amute_gpio);
	}
#endif//TCC_USE_MUTE_GPIO

	ak4602_set_bias_level(component, SND_SOC_BIAS_OFF);

	regcache_cache_only(ak4602->regmap, true);
	regcache_mark_dirty(ak4602->regmap);

	if (ak4602->pdn_gpio > 0) {
		gpio_direction_output(ak4602->pdn_gpio, 0);
		gpio_set_value(ak4602->pdn_gpio, GPO_PDN_LOW);
		mdelay(1);
	}

	return 0;
}

static int ak4602_resume(struct snd_soc_component *component)
{
	struct ak4602_priv *ak4602 = snd_soc_component_get_drvdata(component);

	akdbgprt("\t[AK4602] %s(%d)\n", __func__, __LINE__);

	if (ak4602->pdn_gpio != -1) {
		gpio_direction_output(ak4602->pdn_gpio, 0);
		gpio_set_value(ak4602->pdn_gpio, GPO_PDN_LOW);
		akdbgprt("\t[AK4602] %s External PDN[OFF]\n", __func__);
		mdelay(1);
		gpio_set_value(ak4602->pdn_gpio, GPO_PDN_HIGH);
		akdbgprt("\t[AK4602] %s External PDN[ON]\n", __func__);
		mdelay(1);
	}

#ifndef AK4602_I2C_IF
	ak4602_spi_dummy_command(component);
#endif

	regcache_cache_only(ak4602->regmap, false);
	regcache_sync(ak4602->regmap);

#ifdef TCC_USE_MUTE_GPIO
	if (ak4602->cmute_gpio >= 0) {
		if (gpio_request_one(ak4602->cmute_gpio,
					(ak4602->cmute_gpio_flags ? GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW),
					"cmute-gpios")) {
			ak4602->cmute_gpio = -1;
			akdbgprt("[AK4602] request cmute gpio(%d) failed\n", ak4602->cmute_gpio);
		}
		gpio_direction_output(ak4602->cmute_gpio, 0);
		gpio_set_value(ak4602->cmute_gpio, ak4602->cmute_gpio_flags); //cmute on
		akdbgprt("\t[AK4602] SND_SOC_BIAS_ON cmute_gpio unmute\n");
	}
	if (ak4602->amute_gpio >= 0) {
		if (gpio_request_one(ak4602->amute_gpio,
					(ak4602->amute_gpio_flags ? GPIOF_OUT_INIT_HIGH : GPIOF_OUT_INIT_LOW),
					"amute-gpios")) {
			ak4602->amute_gpio = -1;
			akdbgprt("[AK4602] request amute gpio(%d) failed\n", ak4602->amute_gpio);
		}
		gpio_direction_output(ak4602->amute_gpio, 0);
		gpio_set_value(ak4602->amute_gpio, ak4602->amute_gpio_flags); //amute on
		akdbgprt("\t[AK4602] SND_SOC_BIAS_ON amute_gpio unmute\n");
	}
#endif//TCC_USE_MUTE_GPIO

	return 0;
}

static const struct snd_soc_component_driver soc_codec_dev_ak4602 = {
	.probe = ak4602_probe,
	.remove = ak4602_remove,
	.suspend =	ak4602_suspend,
	.resume =	ak4602_resume,

	.set_bias_level = ak4602_set_bias_level,

	.controls = ak4602_snd_controls,
	.num_controls = ARRAY_SIZE(ak4602_snd_controls),
	.dapm_widgets = ak4602_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(ak4602_dapm_widgets),
	.dapm_routes = ak4602_intercon,
	.num_dapm_routes = ARRAY_SIZE(ak4602_intercon),

	.idle_bias_on = 1,
	.endianness = 1,
	.non_legacy_dai_naming = 1,
};
//EXPORT_SYMBOL_GPL(soc_codec_dev_ak4602);

static const struct regmap_config ak4602_regmap = {
	.reg_bits = 16,
	.val_bits = 8,

	.max_register = AK4602_MAX_REGISTER,
	.readable_reg = ak4602_readable,
	.volatile_reg = ak4602_volatile,
	.writeable_reg = ak4602_writeable,

	.reg_write = ak4602_write,
	.reg_read = ak4602_read,

	.reg_defaults = ak4602_reg,
	.num_reg_defaults = ARRAY_SIZE(ak4602_reg),
	.cache_type = REGCACHE_RBTREE,
};

static const struct of_device_id ak4602_of_match[] = {
	{ .compatible = "akm,ak4602", },
	{ }
};
MODULE_DEVICE_TABLE(of, ak4602_of_match);


#ifdef AK4602_I2C_IF

static int ak4602_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct ak4602_priv *ak4602;
	int ret = 0;

	akdbgprt("\t[AK4602] %s(%d)\n", __func__, __LINE__);

	ak4602 = devm_kzalloc(&i2c->dev, sizeof(struct ak4602_priv), GFP_KERNEL);
	if (ak4602 == NULL)
		return -ENOMEM;

	ak4602->regmap = devm_regmap_init(&i2c->dev, NULL, &i2c->dev, &ak4602_regmap);
	if (IS_ERR(ak4602->regmap)) {
		devm_kfree(&i2c->dev, ak4602);
		return PTR_ERR(ak4602->regmap);
	}

	i2c_set_clientdata(i2c, ak4602);

	ak4602->i2c = i2c;

	ret = devm_snd_soc_register_component(&i2c->dev,
			&soc_codec_dev_ak4602, &ak4602_dai[0], ARRAY_SIZE(ak4602_dai));
	if (ret < 0) {
		devm_kfree(&i2c->dev, ak4602);
		akdbgprt("\t[AK4602 Error!] %s(%d)\n", __func__, __LINE__);
	}
	return ret;
}

static int ak4602_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_component(&client->dev);
	return 0;
}

static const struct i2c_device_id ak4602_i2c_id[] = {
	{ "ak4602", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ak4602_i2c_id);

static struct i2c_driver ak4602_i2c_driver = {
	.driver = {
		.name = "ak4602",
		.owner = THIS_MODULE,
		.of_match_table = ak4602_of_match,
	},
	.probe = ak4602_i2c_probe,
	.remove = ak4602_i2c_remove,
	.id_table = ak4602_i2c_id,
};

#else

static int ak4602_spi_probe(struct spi_device *spi)
{
	struct ak4602_priv *ak4602;
	int ret;

	akdbgprt("\t[AK4602] %s(%d)\n", __func__, __LINE__);

	ak4602 = devm_kzalloc(&spi->dev, sizeof(struct ak4602_priv),
			      GFP_KERNEL);
	if (ak4602 == NULL)
		return -ENOMEM;

	ak4602->regmap = devm_regmap_init(&spi->dev, NULL, &spi->dev, &ak4602_regmap);
	if (IS_ERR(ak4602->regmap)) {
		ret = PTR_ERR(ak4602->regmap);
		dev_err(&spi->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	spi_set_drvdata(spi, ak4602);

	ak4602->spi = spi;

	ret = snd_soc_register_component(&spi->dev,
			&soc_codec_dev_ak4602, &ak4602_dai[0], ARRAY_SIZE(ak4602_dai));

	if (ret != 0) {
		dev_err(&spi->dev, "Failed to register CODEC: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ak4602_spi_remove(struct spi_device *spi)
{
	snd_soc_unregister_component(&spi->dev);
	return 0;
}

static struct spi_driver ak4602_spi_driver = {
	.driver = {
		.name = "ak4602",
		.owner = THIS_MODULE,
		.of_match_table = ak4602_of_match,
	},
	.probe = ak4602_spi_probe,
	.remove = ak4602_spi_remove,
};
#endif

static int __init ak4602_modinit(void)
{
	int ret = 0;

	akdbgprt("\t[AK4602] %s(%d)\n", __func__, __LINE__);

#ifdef AK4602_I2C_IF
	ret = i2c_add_driver(&ak4602_i2c_driver);
	if (ret != 0)
		akdbgprt(KERN_ERR "Failed to register AK4602 I2C driver: %d\n", ret);
#else
	ret = spi_register_driver(&ak4602_spi_driver);
	if (ret != 0)
		akdbgprt(KERN_ERR "Failed to register AK4602 SPI driver: %d\n", ret);
#endif

	return ret;
}

module_init(ak4602_modinit);

static void __exit ak4602_exit(void)
{
#ifdef AK4602_I2C_IF
	i2c_del_driver(&ak4602_i2c_driver);
#else
	spi_unregister_driver(&ak4602_spi_driver);
#endif

}
module_exit(ak4602_exit);

MODULE_DESCRIPTION("ak4602 codec driver");
MODULE_VERSION("1.8");
MODULE_LICENSE("GPL v2");
