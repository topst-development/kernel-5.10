/*
 * ak4602.h  --  audio driver for ak4602
 *
 * Copyright (C) 2021 Asahi Kasei Microdevices Corporation
 * Modified by Copyright Telechips Inc.
 * Modified date :
 * Description :
 *
 *  Author                Date        Revision
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                      17/06/05	    1.0	3.18.XX
 *                      21/10/11	    1.8	4.19.XX
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef AK4602_CODEC_H
#define AK4602_CODEC_H

#define GPO_PDN_HIGH	1
#define GPO_PDN_LOW		0

#define  AK4602_00_SYSTEM_CLOCK_1          0x0
#define  AK4602_01_SYSTEM_CLOCK_2          0x1
#define  AK4602_02_SYSTEM_CLOCK_3          0x2
#define  AK4602_03_MIC_BIAS_POWER          0x3
#define  AK4602_04_CLOCK_SD1_SET_1         0x4
#define  AK4602_05_CLOCK_SD1_SET_2         0x5
#define  AK4602_06_CLOCK_SD1_SET_3         0x6
#define  AK4602_07_CLOCK_SD2_SET_1         0x7
#define  AK4602_08_CLOCK_SD2_SET_2         0x8
#define  AK4602_09_CLOCK_SD2_SET_3         0x9
#define  AK4602_0A_CLOCK_SD3_SET_1         0xA
#define  AK4602_0B_CLOCK_SD3_SET_2         0xB
#define  AK4602_0C_CLOCK_SD3_SET_3         0xC
#define  AK4602_0D_CLOCK_SD4_SET_1         0xD
#define  AK4602_0E_CLOCK_SD4_SET_2         0xE
#define  AK4602_0F_CLOCK_SD4_SET_3         0xF
#define  AK4602_10_CLOCK_SD5_SET_1         0x10
#define  AK4602_11_CLOCK_SD5_SET_2         0x11
#define  AK4602_12_CLOCK_SD5_SET_3         0x12
#define  AK4602_13_RESERVED                0x13
#define  AK4602_14_RESERVED                0x14
#define  AK4602_15_CLKO_OUTPUT             0x15
#define  AK4602_16_RESERVED                0x16
#define  AK4602_17_CLOCK_SD_SEL_1          0x17
#define  AK4602_18_CLOCK_SD_SEL_2          0x18
#define  AK4602_19_CLOCK_SD_SEL_3          0x19
#define  AK4602_1A_EXTCLOCK_1              0x1A
#define  AK4602_1B_EXTCLOCK_2              0x1B
#define  AK4602_1C_RESERVED      	       0x1C
#define  AK4602_1D_RESERVED                0x1D
#define  AK4602_1E_RESERVED                0x1E
#define  AK4602_1F_CLOCK_SD_SEL_7          0x1F
#define  AK4602_20_CLOCK_SD_SEL_8          0x20
#define  AK4602_21_CLOCK_SD_SEL_9          0x21
#define  AK4602_22_CLOCK_SD_SEL_10         0x22
#define  AK4602_23_CLOCK_SD_SEL_11         0x23
#define  AK4602_24_CLOCK_SD_SEL_12         0x24
#define  AK4602_25_CLOCK_SD_SEL_13         0x25
#define  AK4602_26_CLOCK_SD_SEL_14         0x26
#define  AK4602_27_CLOCK_SD_SEL_15         0x27
#define  AK4602_28_CLOCK_SD_SEL_16         0x28
#define  AK4602_29_CLOCK_SD_SEL_17         0x29
#define  AK4602_2A_SDOUT1_TDM_SLOT1_2      0x2A
#define  AK4602_2B_SDOUT1_TDM_SLOT3_4      0x2B
#define  AK4602_2C_SDOUT1_TDM_SLOT5_6      0x2C
#define  AK4602_2D_SDOUT1_TDM_SLOT7_8      0x2D
#define  AK4602_2E_SDOUT1_TDM_SLOT9_10     0x2E
#define  AK4602_2F_SDOUT1_TDM_SLOT11_12    0x2F
#define  AK4602_30_SDOUT1_TDM_SLOT13_14    0x30
#define  AK4602_31_SDOUT1_TDM_SLOT15_16    0x31
#define  AK4602_32_SDOUT2_TDM_SLOT1_2      0x32
#define  AK4602_33_SDOUT2_TDM_SLOT3_4      0x33
#define  AK4602_34_SDOUT2_TDM_SLOT5_6      0x34
#define  AK4602_35_SDOUT2_TDM_SLOT7_8      0x35
#define  AK4602_36_SDOUT2_TDM_SLOT9_10     0x36
#define  AK4602_37_SDOUT2_TDM_SLOT11_12    0x37
#define  AK4602_38_SDOUT2_TDM_SLOT13_14    0x38
#define  AK4602_39_SDOUT2_TDM_SLOT15_16    0x39
#define  AK4602_3A_SDOUT3_TDM_SLOT1_2      0x3A
#define  AK4602_3B_SDOUT3_TDM_SLOT3_4      0x3B
#define  AK4602_3C_SDOUT3_TDM_SLOT5_6      0x3C
#define  AK4602_3D_SDOUT3_TDM_SLOT7_8      0x3D
#define  AK4602_3E_SDOUT3_TDM_SLOT9_10     0x3E
#define  AK4602_3F_SDOUT3_TDM_SLOT11_12    0x3F
#define  AK4602_40_SDOUT3_TDM_SLOT13_14    0x40
#define  AK4602_41_SDOUT3_TDM_SLOT15_16    0x41
#define  AK4602_42_SDOUT4_TDM_SLOT1_2      0x42
#define  AK4602_43_SDOUT4_TDM_SLOT3_4      0x43
#define  AK4602_44_SDOUT4_TDM_SLOT5_6      0x44
#define  AK4602_45_SDOUT4_TDM_SLOT7_8      0x45
#define  AK4602_46_SDOUT4_TDM_SLOT9_10     0x46
#define  AK4602_47_SDOUT4_TDM_SLOT11_12    0x47
#define  AK4602_48_SDOUT4_TDM_SLOT13_14    0x48
#define  AK4602_49_SDOUT4_TDM_SLOT15_16    0x49
#define  AK4602_4A_SDOUT5_TDM_SLOT1_2      0x4A
#define  AK4602_4B_SDOUT5_TDM_SLOT3_4      0x4B
#define  AK4602_4C_SDOUT5_TDM_SLOT5_6      0x4C
#define  AK4602_4D_SDOUT5_TDM_SLOT7_8      0x4D
#define  AK4602_4E_SDOUT5_TDM_SLOT9_10     0x4E
#define  AK4602_4F_SDOUT5_TDM_SLOT11_12    0x4F
#define  AK4602_50_SDOUT5_TDM_SLOT13_14    0x50
#define  AK4602_51_SDOUT5_TDM_SLOT15_16    0x51
#define  AK4602_52_DAC1_INPUT_DATA         0x52
#define  AK4602_53_DAC2_INPUT_DATA         0x53
#define  AK4602_54_DAC3_INPUT_DATA         0x54
#define  AK4602_55_VOL1_INPUT_DATA         0x55
#define  AK4602_56_VOL2_INPUT_DATA         0x56
#define  AK4602_57_VOL3_INPUT_DATA         0x57
#define  AK4602_58_VOL4_INPUT_DATA         0x58
#define  AK4602_59_VOL5_INPUT_DATA         0x59
#define  AK4602_5A_SRC1_INPUT_DATA         0x5A
#define  AK4602_5B_SRC2_INPUT_DATA         0x5B
#define  AK4602_5C_SRC3_INPUT_DATA         0x5C
#define  AK4602_5D_SRC4_INPUT_DATA         0x5D
#define  AK4602_5E_RESERVED                0x5E
#define  AK4602_5F_RESERVED                0x5F
#define  AK4602_60_RESERVED                0x60
#define  AK4602_61_RESERVED                0x61
#define  AK4602_62_MIXER_A_CH1_INPUT       0x62
#define  AK4602_63_MIXER_A_CH2_INPUT       0x63
#define  AK4602_64_MIXER_B_CH1_INPUT       0x64
#define  AK4602_65_MIXER_B_CH2_INPUT       0x65
#define  AK4602_66_DIT_INPUT_DATA          0x66
#define  AK4602_67_RESERVED                0x67
#define  AK4602_68_RESERVED                0x68
#define  AK4602_69_CLOCK_FORMAT_1          0x69
#define  AK4602_6A_CLOCK_FORMAT_2          0x6A
#define  AK4602_6B_CLOCK_FORMAT_3          0x6B
#define  AK4602_6C_RESERVED                0x6C
#define  AK4602_6D_SDIN1_DIGITAL_FORMAT    0x6D
#define  AK4602_6E_SDIN2_DIGITAL_FORMAT    0x6E
#define  AK4602_6F_SDIN3_DIGITAL_FORMAT    0x6F
#define  AK4602_70_SDIN4_DIGITAL_FORMAT    0x70
#define  AK4602_71_SDIN5_DIGITAL_FORMAT    0x71
#define  AK4602_72_SDOUT1_DIGITAL_FORMAT   0x72
#define  AK4602_73_SDOUT2_DIGITAL_FORMAT   0x73
#define  AK4602_74_SDOUT3_DIGITAL_FORMAT   0x74
#define  AK4602_75_SDOUT4_DIGITAL_FORMAT   0x75
#define  AK4602_76_SDOUT5_DIGITAL_FORMAT   0x76
#define  AK4602_77_SDOUT_PHASE             0x77
#define  AK4602_78_RESERVED                0x78
#define  AK4602_79_RESERVED                0x79
#define  AK4602_7A_OUTPUT_PORT             0x7A
#define  AK4602_7B_OUTPUT_PORT_ENABLE      0x7B
#define  AK4602_7C_RESERVED                0x7C
#define  AK4602_7D_MIXER_A                 0x7D
#define  AK4602_7E_MIXER_B                 0x7E
#define  AK4602_7F_MIC_AMP_GAIN            0x7F
#define  AK4602_80_MIC_AMP_GAIN_CONTROL    0x80
#define  AK4602_81_ADC1_LCH_DIGITAL_VOL    0x81
#define  AK4602_82_ADC1_RCH_DIGITAL_VOL    0x82
#define  AK4602_83_ADC2_LCH_DIGITAL_VOL    0x83
#define  AK4602_84_ADC2_RCH_DIGITAL_VOL    0x84
#define  AK4602_85_ADCM_DIGITAL_VOL        0x85
#define  AK4602_86_ANALOG_INPUT_SELECT     0x86
#define  AK4602_87_ADC_MUTE_HPF_CONTROL    0x87
#define  AK4602_88_DAC1_LCH_DIGITAL_VOL    0x88
#define  AK4602_89_DAC1_RCH_DIGITAL_VOL    0x89
#define  AK4602_8A_DAC2_LCH_DIGITAL_VOL    0x8A
#define  AK4602_8B_DAC2_RCH_DIGITAL_VOL    0x8B
#define  AK4602_8C_DAC3_LCH_DIGITAL_VOL    0x8C
#define  AK4602_8D_DAC3_RCH_DIGITAL_VOL    0x8D
#define  AK4602_8E_DAC_MUTE_FILTER         0x8E
#define  AK4602_8F_DAC_DEM                 0x8F
#define  AK4602_90_VOL1_LCH_DIGITAL_VOL    0x90
#define  AK4602_91_VOL1_RCH_DIGITAL_VOL    0x91
#define  AK4602_92_VOL2_LCH_DIGITAL_VOL    0x92
#define  AK4602_93_VOL2_RCH_DIGITAL_VOL    0x93
#define  AK4602_94_VOL3_LCH_DIGITAL_VOL    0x94
#define  AK4602_95_VOL3_RCH_DIGITAL_VOL    0x95
#define  AK4602_96_VOL4_LCH_DIGITAL_VOL    0x96
#define  AK4602_97_VOL4_RCH_DIGITAL_VOL    0x97
#define  AK4602_98_VOL5_LCH_DIGITAL_VOL    0x98
#define  AK4602_99_VOL5_RCH_DIGITAL_VOL    0x99
#define  AK4602_9A_VOL_SETTING             0x9A
#define  AK4602_9B_SRC_FILTER              0x9B
#define  AK4602_9C_SRC_PHASE_GROUP_1       0x9C
#define  AK4602_9D_RESERVED                0x9D
#define  AK4602_9E_SRC_MUTE_SETTING1       0x9E
#define  AK4602_9F_SRC_MUTE_SETTING2       0x9F
#define  AK4602_A0_STO_FLAG_1              0xA0
#define  AK4602_A1_STO_FLAG_2              0xA1
#define  AK4602_A2_DIT_STATUS_BIT_1        0xA2
#define  AK4602_A3_DIT_STATUS_BIT_2        0xA3
#define  AK4602_A4_DIT_STATUS_BIT_3        0xA4
#define  AK4602_A5_DIT_STATUS_BIT_4        0xA5
#define  AK4602_A6_RESERVED                0xA6
#define  AK4602_A7_POWER_MANAGEMENT_1      0xA7
#define  AK4602_A8_POWER_MANAGEMENT_2      0xA8
#define  AK4602_A9_RESET_CONTROL           0xA9
#define  AK4602_102_STATUS_READ           0x102
#define  AK4602_103_SRC_STATUS_1          0x103

#define AK4602_MAX_REGISTER AK4602_103_SRC_STATUS_1

#endif
