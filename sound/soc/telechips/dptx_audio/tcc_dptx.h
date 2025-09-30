/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_DPTX_AUDIO_H
#define TCC_DPTX_AUDIO_H
#include <linux/io.h>
#include "tcc_dptx_api.h"

#if 0	// Debug
#define dptx_writel(v, c)                        \
		({pr_info("<ASoC> DP_REG(%p) = 0x%08x\n", c, (unsigned int)v); \
		writel(v, c); })
#else
#define dptx_writel(v, c) writel(v, c)
#endif


/**
 * DP Audio I/F Selection Register Offset
 */
#define TCC_DP_AUDIO_SEL_OFFSET \
	(0x0058u)


/**
 * DP Tx Audio Registers Offset (DP Configration Base + offset)
 */
#define OFFSET_AUD_CONFIG1_N(n) \
	(0x400u + (0x10000u * (n)))
#define OFFSET_SDP_VERTICAL_CTRL_N(n) \
	(0x500u + (0x10000u * (n)))
#define OFFSET_SDP_HORIZONTAL_CTRL_N(n) \
	(0x504u + (0x10000u * (n)))
#define OFFSET_SDP_REGISTER_BANK0_N(n) \
	(0x600u + (0x10000u * (n)))
#define OFFSET_SDP_REGISTER_BANK1_N(n) \
	(0x604u + (0x10000u * (n)))
#define OFFSET_SDP_REGISTER_BANK2_N(n) \
	(0x608u + (0x10000u * (n)))


/**
 * DP Audio I/F Selection Register (DP_AUDIO_SEL),
 * Offset: IO Sub-system Configuration0 Base + 0x58)
 */
#define POSITION_I2S_0_DP_AUDIO_SEL \
	(0u)
#define MASK_I2S_0_DP_AUDIO_SEL \
	(0x7u << POSITION_I2S_0_DP_AUDIO_SEL)

#define POSITION_I2S_1_DP_AUDIO_SEL \
	(4u)
#define MASK_I2S_1_DP_AUDIO_SEL \
	(0x7u << POSITION_I2S_1_DP_AUDIO_SEL)

#define POSITION_I2S_2_DP_AUDIO_SEL \
	(8u)
#define MASK_I2S_2_DP_AUDIO_SEL \
	(0x7u << POSITION_I2S_2_DP_AUDIO_SEL)

#define POSITION_I2S_3_DP_AUDIO_SEL \
	(12u)
#define MASK_I2S_3_DP_AUDIO_SEL \
	(0x7u << POSITION_I2S_3_DP_AUDIO_SEL)


/**
 * DP TX Audio Configuration 1 (AUD_CONFIG1),
 * Offset: 0x400 + 0x10000 * (n)
 */
#define POSITION_INF_SEL_AUD_CONFIG1 \
	(0u)
#define MASK_INF_SEL_AUD_CONFIG1 \
	(0u << POSITION_INF_SEL_AUD_CONFIG1)
#define INF_SEL_I2S_AUD_CONFIG1 \
	(0u << POSITION_INF_SEL_AUD_CONFIG1)
#define INF_SEL_SPDIF_AUD_CONFIG1 \
	(1u << POSITION_INF_SEL_AUD_CONFIG1)

#define POSITION_DATA_IN_EN_AUD_CONFIG1 \
	(1u)
#define MASK_DATA_IN_EN_AUD_CONFIG1 \
	(15u << POSITION_DATA_IN_EN_AUD_CONFIG1)
#define DATA_IN_EN_0_AUD_CONFIG1 \
	(0x1u << POSITION_DATA_IN_EN_AUD_CONFIG1)
#define DATA_IN_EN_1_AUD_CONFIG1 \
	(0x2u << POSITION_DATA_IN_EN_AUD_CONFIG1)
#define DATA_IN_EN_2_AUD_CONFIG1 \
	(0x4u << POSITION_DATA_IN_EN_AUD_CONFIG1)
#define DATA_IN_EN_3_AUD_CONFIG1 \
	(0x8u << POSITION_DATA_IN_EN_AUD_CONFIG1)

#define POSITION_DATA_WIDTH_AUD_CONFIG1 \
	(5u)
#define MASK_DATA_WIDTH_AUD_CONFIG1 \
	(31u << POSITION_DATA_WIDTH_AUD_CONFIG1)
#define DATA_WIDTH_16_BIT_AUD_CONFIG1 \
	(0x10u << POSITION_DATA_WIDTH_AUD_CONFIG1)
#define DATA_WIDTH_17_BIT_AUD_CONFIG1 \
	(0x11u << POSITION_DATA_WIDTH_AUD_CONFIG1)
#define DATA_WIDTH_18_BIT_AUD_CONFIG1 \
	(0x12u << POSITION_DATA_WIDTH_AUD_CONFIG1)
#define DATA_WIDTH_19_BIT_AUD_CONFIG1 \
	(0x13u << POSITION_DATA_WIDTH_AUD_CONFIG1)
#define DATA_WIDTH_20_BIT_AUD_CONFIG1 \
	(0x14u << POSITION_DATA_WIDTH_AUD_CONFIG1)
#define DATA_WIDTH_21_BIT_AUD_CONFIG1 \
	(0x15u << POSITION_DATA_WIDTH_AUD_CONFIG1)
#define DATA_WIDTH_22_BIT_AUD_CONFIG1 \
	(0x16u << POSITION_DATA_WIDTH_AUD_CONFIG1)
#define DATA_WIDTH_23_BIT_AUD_CONFIG1 \
	(0x17u << POSITION_DATA_WIDTH_AUD_CONFIG1)
#define DATA_WIDTH_24_BIT_AUD_CONFIG1 \
	(0x18u << POSITION_DATA_WIDTH_AUD_CONFIG1)

#define POSITION_HBR_MODE_ENABLE_AUD_CONFIG1 \
	(10u)
#define MASK_HBR_MODE_ENABLE_AUD_CONFIG1 \
	(1u << POSITION_HBR_MODE_ENABLE_AUD_CONFIG1)
#define HBR_MODE_ENABLE_AUD_CONFIG1 \
	(1u << POSITION_HBR_MODE_ENABLE_AUD_CONFIG1)
#define HBR_MODE_DISABLE_AUD_CONFIG1 \
	(0u << POSITION_HBR_MODE_ENABLE_AUD_CONFIG1)

#define POSITION_NUM_CHANNELS_AUD_CONFIG1 \
	(12u)
#define MASK_NUM_CHANNELS_AUD_CONFIG1 \
	(7u << POSITION_NUM_CHANNELS_AUD_CONFIG1)
#define NUM_CHANNELS_1_AUD_CONFIG1 \
	(0u << POSITION_NUM_CHANNELS_AUD_CONFIG1)
#define NUM_CHANNELS_2_AUD_CONFIG1 \
	(1u << POSITION_NUM_CHANNELS_AUD_CONFIG1)
#define NUM_CHANNELS_8_AUD_CONFIG1 \
	(7u << POSITION_NUM_CHANNELS_AUD_CONFIG1)

#define POSITION_AUDIO_MUTE_AUD_CONFIG1 \
	(15u)
#define MASK_AUDIO_MUTE_AUD_CONFIG1 \
	(1u << POSITION_AUDIO_MUTE_AUD_CONFIG1)
#define SET_AUDIO_MUTE_AUD_CONFIG1 \
	(1u << POSITION_AUDIO_MUTE_AUD_CONFIG1)
#define CLEAR_AUDIO_MUTE_AUD_CONFIG1 \
	(0u << POSITION_AUDIO_MUTE_AUD_CONFIG1)

#define POSITION_AUDIO_PACKET_ID_AUD_CONFIG1 \
	(16u)
#define MASK_AUDIO_PACKET_ID_AUD_CONFIG1 \
	(255u << POSITION_AUDIO_PACKET_ID_AUD_CONFIG1)

#define POSITION_AUDIO_TIMESTAMP_VER_NUM_AUD_CONFIG1 \
	(24u)
#define MASK_AUDIO_TIMESTAMP_VER_NUM_AUD_CONFIG1 \
	(63u << POSITION_AUDIO_TIMESTAMP_VER_NUM_AUD_CONFIG1)


/**
 * DP TX SDP VERTICAL Control (SDP_VERTICAL_CTRL),
 * Offset: 0x500 + 0x10000 * (n)
 */
#define POSITION_EN_AUDIO_TIME_STAMP_SDP_VERTICAL_CTRL \
	(0u)
#define MASK_EN_AUDIO_TIMESTAMP_SDP_VERTICAL_CTRL \
	(1u << POSITION_EN_AUDIO_TIME_STAMP_SDP_VERTICAL_CTRL)
#define SET_EN_AUDIO_TIMESTAMP_SDP_VERTICAL_CTRL \
	(1u << POSITION_EN_AUDIO_TIME_STAMP_SDP_VERTICAL_CTRL)
#define CLEAR_EN_AUDIO_TIMESTAMP_SDP_VERTICAL_CTRL \
	(0u << POSITION_EN_AUDIO_TIME_STAMP_SDP_VERTICAL_CTRL)

#define POSITION_EN_AUDIO_STREAM_SDP_VERTICAL_CTRL \
	(1u)
#define MASK_EN_AUDIO_STREAM_SDP_VERTICAL_CTRL \
	(1u << POSITION_EN_AUDIO_STREAM_SDP_VERTICAL_CTRL)
#define SET_EN_AUDIO_STREAM_SDP_VERTICAL_CTRL \
	(1u << POSITION_EN_AUDIO_STREAM_SDP_VERTICAL_CTRL)
#define CLEAR_EN_AUDIO_STREAM_SDP_VERTICAL_CTRL \
	(0u << POSITION_EN_AUDIO_STREAM_SDP_VERTICAL_CTRL)

#define POSITION_EN_VERTICAL_SDP_N_SDP_VERTICAL_CTRL \
	(2u)
#define MASK_EN_VERTICAL_SDP_N_SDP_VERTICAL_CTRL \
	(0x3FFFFu << POSITION_EN_VERTICAL_SDP_N_SDP_VERTICAL_CTRL)

#define POSITION_EN_128_BYTES_SDP_1_SDP_VERTICAL_CTRL \
	(24u)
#define MASK_EN_128_BYTES_SDP_1_SDP_VERTICAL_CTRL \
	(0x1u << POSITION_EN_128_BYTES_SDP_1_SDP_VERTICAL_CTRL)
#define SET_EN_128_BYTES_SDP_1_SDP_VERTICAL_CTRL \
	(1u << POSITION_EN_128_BYTES_SDP_1_SDP_VERTICAL_CTRL)
#define CLEAR_EN_128_BYTES_SDP_1_SDP_VERTICAL_CTRL \
	(0u << POSITION_EN_128_BYTES_SDP_1_SDP_VERTICAL_CTRL)

#define POSITION_DISABLE_EXT_SDP_SDP_VERTICAL_CTRL \
	(30u)
#define MASK_DISABLE_EXT_SDP_SDP_VERTICAL_CTRL \
	(1u << POSITION_DISABLE_EXT_SDP_SDP_VERTICAL_CTRL)
#define SET_DISABLE_EXT_SDP_SDP_VERTICAL_CTRL \
	(1u << POSITION_DISABLE_EXT_SDP_SDP_VERTICAL_CTRL)
#define CLEAR_DISABLE_EXT_SDP_SDP_VERTICAL_CTRL \
	(0u << POSITION_DISABLE_EXT_SDP_SDP_VERTICAL_CTRL)

#define POSITION_FIXED_PRIO_ARBI_SDP_VERTICAL_CTRL \
	(31u)
#define MASK_FIXED_PRIO_ARBI_SDP_VERTICAL_CTRL \
	(1u << POSITION_FIXED_PRIO_ARBI_SDP_VERTICAL_CTRL)
#define SET_FIXED_PRIO_ARBI_SDP_VERTICAL_CTRL \
	(1u << POSITION_FIXED_PRIO_ARBI_SDP_VERTICAL_CTRL)
#define CLEAR_FIXED_PRIO_ARBI_SDP_VERTICAL_CTRL \
	(0u << POSITION_FIXED_PRIO_ARBI_SDP_VERTICAL_CTRL)


/**
 * DP TX SDP Horizontal Control (SDP_HORIZONTAL_CTRL),
 * Offset: 0x504 + 0x10000 * (n)
 */
#define POSITION_EN_AUDIO_TIMESTAMP_SDP_HORIZONTAL_CTRL \
	(0u)
#define MASK_EN_AUDIO_TIMESTAMP_SDP_HORIZONTAL_CTRL \
	(1u << POSITION_EN_AUDIO_TIMESTAMP_SDP_HORIZONTAL_CTRL)
#define SET_EN_AUDIO_TIMESTAMP_SDP_HORIZONTAL_CTRL \
	(1u << POSITION_EN_AUDIO_TIMESTAMP_SDP_HORIZONTAL_CTRL)
#define CLEAR_EN_AUDIO_TIMESTAMP_SDP_HORIZONTAL_CTRL \
	(0u << POSITION_EN_AUDIO_TIMESTAMP_SDP_HORIZONTAL_CTRL)

#define POSITION_EN_AUDIO_STREAM_SDP_HORIZONTAL_CTRL \
	(1u)
#define MASK_EN_AUDIO_STREAM_SDP_HORIZONTAL_CTRL \
	(1u << POSITION_EN_AUDIO_STREAM_SDP_HORIZONTAL_CTRL)
#define SET_EN_AUDIO_STREAM_SDP_HORIZONTAL_CTRL \
	(1u << POSITION_EN_AUDIO_STREAM_SDP_HORIZONTAL_CTRL)
#define CLEAR_EN_AUDIO_STREAM_SDP_HORIZONTAL_CTRL \
	(0u << POSITION_EN_AUDIO_STREAM_SDP_HORIZONTAL_CTRL)

#define POSITION_EN_HORIZONTAL_SDP_N_SDP_HORIZONTAL_CTRL \
	(2u)
#define MASK_EN_HORIZONTAL_SDP_N_SDP_HORIZONTAL_CTRL \
	(0x3FFFFu << POSITION_EN_HORIZONTAL_SDP_N_SDP_HORIZONTAL_CTRL)

#define POSITION_FIXED_PRIO_ARBI_SDP_HORIZONTAL_CTRL \
	(31u)
#define MASK_FIXED_PRIO_ARBI_SDP_HORIZONTAL_CTRL \
	(1u << POSITION_FIXED_PRIO_ARBI_SDP_HORIZONTAL_CTRL)
#define SET_FIXED_PRIO_ARBI_SDP_HORIZONTAL_CTRL \
	(1u << POSITION_FIXED_PRIO_ARBI_SDP_HORIZONTAL_CTRL)
#define CLEAR_FIXED_PRIO_ARBI_SDP_HORIZONTAL_CTRL \
	(0u << POSITION_FIXED_PRIO_ARBI_SDP_HORIZONTAL_CTRL)


/**
 * DP TX SDP Register Bank 0 (SDP_REGISTER_BANK0),
 * Offset: 0x600 + 0x10000 * (n)
 */
#define POSITION_SDP_REGISTER_BANK0 \
	(0u)
#define MASK_SDP_REGISTER_BANK0 \
	(0xFFFFFFFFu << POSITION_SDP_REGISTER_BANK0)


/**
 * DP TX SDP Register Bank 1 (SDP_REGISTER_BANK1),
 * Offset: 0x604 + 0x10000 * (n)
 */
#define POSITION_SDP_REGISTER_BANK1 \
	(0u)
#define MASK_SDP_REGISTER_BANK1 \
	(0xFFFFFFFFu << POSITION_SDP_REGISTER_BANK1)
#define SAMPLE_RATE_32000_HZ_SDP_REGISTER_BANK1 \
	(0x00000501u << POSITION_SDP_REGISTER_BANK1)
#define SAMPLE_RATE_44100_HZ_SDP_REGISTER_BANK1 \
	(0x00000901u << POSITION_SDP_REGISTER_BANK1)
#define SAMPLE_RATE_48000_HZ_SDP_REGISTER_BANK1 \
	(0x00000D01u << POSITION_SDP_REGISTER_BANK1)
#define SAMPLE_RATE_88200_HZ_SDP_REGISTER_BANK1 \
	(0x00001101u << POSITION_SDP_REGISTER_BANK1)
#define SAMPLE_RATE_96000_HZ_SDP_REGISTER_BANK1 \
	(0x00001501u << POSITION_SDP_REGISTER_BANK1)
#define SAMPLE_RATE_176400_HZ_SDP_REGISTER_BANK1 \
	(0x00001901u << POSITION_SDP_REGISTER_BANK1)
#define SAMPLE_RATE_192000_HZ_SDP_REGISTER_BANK1 \
	(0x00001D01u << POSITION_SDP_REGISTER_BANK1)


/**
 * DP TX SDP Register Bank 2 (SDP_REGISTER_BANK2),
 * Offset: 0x608 + 0x10000 * (n)
 */
#define POSITION_SDP_REGISTER_BANK2 \
	(0u)
#define MASK_SDP_REGISTER_BANK2 \
	(0xFFFFFFFFu << POSITION_SDP_REGISTER_BANK2)


/**
 * ETC
 */
#define TRUE \
	((bool) true)
#define FALSE \
	((bool) false)
#define TCC_AUDIO_INPUT_MAX \
	(7u)
#define MAX_NUM_DPTX_STREAM \
	(4u)


/**
 * Character Device
 */
#define MINOR_BASE \
	0
#define DEVICE_NAME \
	"tcc_dptx_audio"

#define MAGIC_NO 'k'
#define IOCTL_CMD_DPTX_AUDIO_SET_APARAMS \
	_IOWR(MAGIC_NO, 0x50u, struct tcc_audio_stream)
#define IOCTL_CMD_DPTX_AUDIO_ENABLE_AUDIO \
	_IOWR(MAGIC_NO, 0x51u, struct tcc_audio_stream)
#define IOCTL_CMD_DPTX_AUDIO_DISABLE_AUDIO \
	_IOWR(MAGIC_NO, 0x52u, struct tcc_audio_stream)
#define IOCTL_CMD_DPTX_AUDIO_MUTE \
	_IOWR(MAGIC_NO, 0x53u, struct tcc_audio_stream)
#define IOCTL_CMD_DPTX_AUDIO_UNMUTE \
	_IOWR(MAGIC_NO, 0x54u, struct tcc_audio_stream)
#define IOCTL_CMD_DPTX_AUDIO_DUMP \
	_IOWR(MAGIC_NO, 0x55u, struct tcc_audio_stream)


enum TCC_AUDIO_INPUT_VALUE {
	TCC_AUDIO_0_INPUT_VALUE 		= 0x4u,
	TCC_AUDIO_1_INPUT_VALUE 		= 0x5u,
	TCC_AUDIO_2_INPUT_VALUE 		= 0x6u,
	TCC_AUDIO_3_INPUT_VALUE 		= 0x0u,
	TCC_AUDIO_4_INPUT_VALUE 		= 0x1u,
	TCC_AUDIO_5_INPUT_VALUE 		= 0x2u,
	TCC_AUDIO_6_INPUT_VALUE 		= 0x3u,
	TCC_AUDIO_7_INPUT_VALUE 		= 0x7u,
};

enum DPTX_AUDIO_STREAM_STATUS {
	AUDIO_STREAM_STATUS_DISABLED 	= 0u,
	AUDIO_STREAM_STATUS_ENABLED 	= 1u,
	AUDIO_STREAM_STATUS_UNMUTE		= AUDIO_STREAM_STATUS_ENABLED,
	AUDIO_STREAM_STATUS_MUTE 		= 2u,
};

struct tcc_audio_stream {
	uint8_t id;
	uint32_t audio_input;
	struct tcc_audio_params aparams;
	enum DPTX_AUDIO_STREAM_STATUS status;
};

struct tcc_dptx_t {
	struct platform_device *pdev;
	uint32_t max_num_stream;

	void __iomem *link_reg;
	void __iomem *audio_sel_reg;

	struct tcc_audio_stream astream[MAX_NUM_DPTX_STREAM];
	/* char devices */
	dev_t dev_num;
	struct cdev char_dev;
	struct class *char_class;
	struct device *char_device;

};

/*######################################################################*/
/*#               Fucntions for DP Audio driver                        #*/
/*######################################################################*/
static inline void dptx_audio_dump(const struct tcc_dptx_t *dptx)
{
	uint32_t i;
	const struct tcc_audio_stream *astream;
	const struct tcc_audio_params *aparams;

	(void) pr_info("DPTX Audio Register\n");
	(void) pr_info("    DP_AUDIO_SEL: 0x%08x\n",
		readl(dptx->audio_sel_reg + TCC_DP_AUDIO_SEL_OFFSET));
	for (i = 0; i < dptx->max_num_stream; i++) {
		(void) pr_info("    AUD_CONFIG1[%d]: 0x%08x\n",
			i,
			readl(dptx->link_reg + OFFSET_AUD_CONFIG1_N(i)));
		(void) pr_info("    SDP_REG_BANK0[%d]: 0x%08x\n",
			i,
			readl(dptx->link_reg + OFFSET_SDP_REGISTER_BANK0_N(i)));
		(void) pr_info("    SDP_REG_BANK1[%d]: 0x%08x\n",
			i,
			readl(dptx->link_reg + OFFSET_SDP_REGISTER_BANK1_N(i)));
		(void) pr_info("    SDP_REG_BANK2[%d]: 0x%08x\n",
			i,
			readl(dptx->link_reg + OFFSET_SDP_REGISTER_BANK2_N(i)));
		(void) pr_info("    SDP_VERTIAL_CTRL[%d]: 0x%08x\n",
			i,
			readl(dptx->link_reg + OFFSET_SDP_VERTICAL_CTRL_N(i)));
		(void) pr_info("    SDP_HORIZONTAL_CTRL[%d]: 0x%08x\n",
			i,
			readl(dptx->link_reg + OFFSET_SDP_HORIZONTAL_CTRL_N(i)));
	}


	(void) pr_info("DPTX Audio Stream Context\n");
	for (i = 0; i < dptx->max_num_stream; ++i) {
		astream = &dptx->astream[i];
		aparams = &astream->aparams;
		(void) pr_info("    DPTX_AUDIO_STREAM[%d]\n", i);
		(void) pr_info("        id: %d\n", astream->id);
		(void) pr_info("        audio_input: %d\n", astream->audio_input);
		(void) pr_info("        status: %d\n", astream->status);
		(void) pr_info("        data_width: %d\n", aparams->data_width);
		(void) pr_info("        channels: %d\n", aparams->channels);
		(void) pr_info("        sample_rate: %d\n", aparams->sample_rate);
	}
}

static inline void dptx_link_audio_input(
	void __iomem *base_addr,
	const struct tcc_audio_stream *astream)
{
	uint32_t value;
	uint32_t input_value;
	const struct tcc_audio_params *aparams;

	aparams = &astream->aparams;

	value = readl(base_addr + TCC_DP_AUDIO_SEL_OFFSET);

	switch (astream->audio_input) {
		case 0u:
			input_value = (uint32_t)TCC_AUDIO_0_INPUT_VALUE;
			break;
		case 1u:
			input_value = (uint32_t)TCC_AUDIO_1_INPUT_VALUE;
			break;
		case 2u:
			input_value = (uint32_t)TCC_AUDIO_2_INPUT_VALUE;
			break;
		case 3u:
			input_value = (uint32_t)TCC_AUDIO_3_INPUT_VALUE;
			break;
		case 4u:
			input_value = (uint32_t)TCC_AUDIO_4_INPUT_VALUE;
			break;
		case 5u:
			input_value = (uint32_t)TCC_AUDIO_5_INPUT_VALUE;
			break;
		case 6u:
			input_value = (uint32_t)TCC_AUDIO_6_INPUT_VALUE;
			break;
		case 7u:
			input_value = (uint32_t)TCC_AUDIO_7_INPUT_VALUE;
			break;
		default:
			input_value = (uint32_t)TCC_AUDIO_0_INPUT_VALUE;
			break;
	}

	switch (astream->id) {
		case 0u:
			value &= ~MASK_I2S_0_DP_AUDIO_SEL;
			value |= input_value << POSITION_I2S_0_DP_AUDIO_SEL;
			break;
		case 1u:
			value &= ~MASK_I2S_1_DP_AUDIO_SEL;
			value |= input_value << POSITION_I2S_1_DP_AUDIO_SEL;
			break;
		case 2u:
			value &= ~MASK_I2S_2_DP_AUDIO_SEL;
			value |= input_value << POSITION_I2S_2_DP_AUDIO_SEL;
			break;
		case 3u:
			value &= ~MASK_I2S_3_DP_AUDIO_SEL;
			value |= input_value << POSITION_I2S_3_DP_AUDIO_SEL;
			break;
		default:
			value &= ~MASK_I2S_0_DP_AUDIO_SEL;
			value |= input_value << POSITION_I2S_0_DP_AUDIO_SEL;
			break;
	}

	dptx_writel(value, base_addr + TCC_DP_AUDIO_SEL_OFFSET);
}

static inline void dptx_audio_sample_rate_change(
	void __iomem *base_addr,
	const struct tcc_audio_stream *astream)
{
	uint32_t value;
	const struct tcc_audio_params *aparams;

	aparams = &astream->aparams;

	value =
		readl(base_addr + OFFSET_SDP_REGISTER_BANK1_N(astream->id));
	value &= ~MASK_SDP_REGISTER_BANK1;
	switch (aparams->sample_rate) {
		case 32000u:
			value |= SAMPLE_RATE_32000_HZ_SDP_REGISTER_BANK1;
			break;
		case 44100u:
			value |= SAMPLE_RATE_44100_HZ_SDP_REGISTER_BANK1;
			break;
		case 48000u:
			value |= SAMPLE_RATE_48000_HZ_SDP_REGISTER_BANK1;
			break;
		case 88200u:
			value |= SAMPLE_RATE_88200_HZ_SDP_REGISTER_BANK1;
			break;
		case 96000u:
			value |= SAMPLE_RATE_96000_HZ_SDP_REGISTER_BANK1;
			break;
		case 176400u:
			value |= SAMPLE_RATE_176400_HZ_SDP_REGISTER_BANK1;
			break;
		case 192000u:
			value |= SAMPLE_RATE_192000_HZ_SDP_REGISTER_BANK1;
			break;
		default:
			value |= SAMPLE_RATE_32000_HZ_SDP_REGISTER_BANK1;
			break;
	}
	dptx_writel(
		value,
		base_addr + OFFSET_SDP_REGISTER_BANK1_N(astream->id));
}

static inline void dptx_audio_change_inf_type(
	void __iomem *base_addr,
	const struct tcc_audio_stream *astream)
{
	uint32_t value;

	value = readl(base_addr + OFFSET_AUD_CONFIG1_N(astream->id));
	value &= ~MASK_INF_SEL_AUD_CONFIG1;
	value |= INF_SEL_I2S_AUD_CONFIG1;

	dptx_writel(
		value,
		base_addr + OFFSET_AUD_CONFIG1_N(astream->id));
}

static inline void dptx_audio_change_num_ch(
	void __iomem *base_addr,
	const struct tcc_audio_stream *astream)
{
	uint32_t value;
	const struct tcc_audio_params *aparams;

	aparams = &astream->aparams;

	value = readl(base_addr + OFFSET_AUD_CONFIG1_N(astream->id));
	value &= ~MASK_NUM_CHANNELS_AUD_CONFIG1;

	switch (aparams->channels) {
		case 1u:
			value |= NUM_CHANNELS_1_AUD_CONFIG1;
			break;
		case 2u:
			value |= NUM_CHANNELS_2_AUD_CONFIG1;
			break;
		case 8u:
			value |= NUM_CHANNELS_8_AUD_CONFIG1;
			break;
		default:
			value |= NUM_CHANNELS_2_AUD_CONFIG1;
			break;
	}

	dptx_writel(
		value,
		base_addr + OFFSET_AUD_CONFIG1_N(astream->id));
}

static inline void dptx_audio_change_data_width(
	void __iomem *base_addr,
	const struct tcc_audio_stream *astream)
{
	uint32_t value;
	const struct tcc_audio_params *aparams;

	aparams = &astream->aparams;

	value = readl(base_addr + OFFSET_AUD_CONFIG1_N(astream->id));
	value &= ~MASK_DATA_WIDTH_AUD_CONFIG1;

	switch (aparams->data_width) {
		case 16u:
			value |= DATA_WIDTH_16_BIT_AUD_CONFIG1;
			break;
		case 17u:
			value |= DATA_WIDTH_17_BIT_AUD_CONFIG1;
			break;
		case 18u:
			value |= DATA_WIDTH_18_BIT_AUD_CONFIG1;
			break;
		case 19u:
			value |= DATA_WIDTH_19_BIT_AUD_CONFIG1;
			break;
		case 20u:
			value |= DATA_WIDTH_20_BIT_AUD_CONFIG1;
			break;
		case 21u:
			value |= DATA_WIDTH_21_BIT_AUD_CONFIG1;
			break;
		case 22u:
			value |= DATA_WIDTH_22_BIT_AUD_CONFIG1;
			break;
		case 23u:
			value |= DATA_WIDTH_23_BIT_AUD_CONFIG1;
			break;
		case 24u:
			value |= DATA_WIDTH_24_BIT_AUD_CONFIG1;
			break;
		default:
			value |= DATA_WIDTH_16_BIT_AUD_CONFIG1;
			break;
	}

	dptx_writel(
		value,
		base_addr + OFFSET_AUD_CONFIG1_N(astream->id));
}

static inline void dptx_audio_change_ats_ver(
	void __iomem *base_addr,
	const struct tcc_audio_stream *astream)
{
	uint32_t value;
	const struct tcc_audio_params *aparams;

	aparams = &astream->aparams;

	value = readl(base_addr + OFFSET_AUD_CONFIG1_N(astream->id));
	value &= ~MASK_AUDIO_TIMESTAMP_VER_NUM_AUD_CONFIG1;
	// Todo. implementation after soc guide (ats = 0x12)
	value |= 0x12u << POSITION_AUDIO_TIMESTAMP_VER_NUM_AUD_CONFIG1;

	dptx_writel(
		value,
		base_addr + OFFSET_AUD_CONFIG1_N(astream->id));
}

static inline void dptx_audio_enable_channel(
	void __iomem *base_addr,
	const struct tcc_audio_stream *astream)
{
	uint32_t value;
	const struct tcc_audio_params *aparams;

	aparams = &astream->aparams;

	value = readl(base_addr + OFFSET_AUD_CONFIG1_N(astream->id));
	value &= ~MASK_DATA_IN_EN_AUD_CONFIG1;

	switch (aparams->channels) {
		case 1u:
		case 2u:
			value |= DATA_IN_EN_0_AUD_CONFIG1;
			break;
		case 3u:
		case 4u:
			value |= DATA_IN_EN_1_AUD_CONFIG1;
			break;
		case 5u:
		case 6u:
			value |= DATA_IN_EN_2_AUD_CONFIG1;
			break;
		case 7u:
		case 8u:
			value |= DATA_IN_EN_3_AUD_CONFIG1;
			break;
		default:
			value |= DATA_IN_EN_0_AUD_CONFIG1;
			break;
	}

	dptx_writel(
		value,
		base_addr + OFFSET_AUD_CONFIG1_N(astream->id));
}

static inline void dptx_audio_enable_sdp(
	void __iomem *base_addr,
	const struct tcc_audio_stream *astream)
{
	uint32_t value;
	const struct tcc_audio_params *aparams;

	aparams = &astream->aparams;

	value = readl(
		base_addr + OFFSET_SDP_VERTICAL_CTRL_N(astream->id));
	value &= ~MASK_EN_AUDIO_STREAM_SDP_VERTICAL_CTRL;
	value |= SET_EN_AUDIO_STREAM_SDP_VERTICAL_CTRL;

	dptx_writel(
		value,
		base_addr + OFFSET_SDP_VERTICAL_CTRL_N(astream->id));

	value = readl(
		base_addr + OFFSET_SDP_HORIZONTAL_CTRL_N(astream->id));
	value &= ~MASK_EN_AUDIO_STREAM_SDP_HORIZONTAL_CTRL;
	value |= SET_EN_AUDIO_STREAM_SDP_HORIZONTAL_CTRL;

	dptx_writel(
		value,
		base_addr + OFFSET_SDP_HORIZONTAL_CTRL_N(astream->id));
}

static inline void dptx_audio_disable_sdp(
	void __iomem *base_addr,
	const struct tcc_audio_stream *astream)
{
	uint32_t value;
	const struct tcc_audio_params *aparams;

	aparams = &astream->aparams;

	value = readl(
		base_addr + OFFSET_SDP_VERTICAL_CTRL_N(astream->id));
	value &= ~MASK_EN_AUDIO_STREAM_SDP_VERTICAL_CTRL;
	value |= CLEAR_EN_AUDIO_STREAM_SDP_VERTICAL_CTRL;

	dptx_writel(
		value,
		base_addr + OFFSET_SDP_VERTICAL_CTRL_N(astream->id));

	value = readl(
		base_addr + OFFSET_SDP_HORIZONTAL_CTRL_N(astream->id));
	value &= ~MASK_EN_AUDIO_STREAM_SDP_HORIZONTAL_CTRL;
	value |= CLEAR_EN_AUDIO_STREAM_SDP_HORIZONTAL_CTRL;

	dptx_writel(
		value,
		base_addr + OFFSET_SDP_HORIZONTAL_CTRL_N(astream->id));
}

static inline void dptx_audio_enable_timestamp_sdp(
	void __iomem *base_addr,
	const struct tcc_audio_stream *astream)
{
	uint32_t value;
	const struct tcc_audio_params *aparams;

	aparams = &astream->aparams;

	value = readl(
		base_addr + OFFSET_SDP_VERTICAL_CTRL_N(astream->id));
	value &= ~MASK_EN_AUDIO_TIMESTAMP_SDP_VERTICAL_CTRL;
	value |= SET_EN_AUDIO_TIMESTAMP_SDP_VERTICAL_CTRL;

	dptx_writel(
		value,
		base_addr + OFFSET_SDP_VERTICAL_CTRL_N(astream->id));
}

static inline void dptx_audio_disable_timestamp_sdp(
	void __iomem *base_addr,
	const struct tcc_audio_stream *astream)
{
	uint32_t value;
	const struct tcc_audio_params *aparams;

	aparams = &astream->aparams;

	value = readl(
		base_addr + OFFSET_SDP_VERTICAL_CTRL_N(astream->id));
	value &= ~MASK_EN_AUDIO_TIMESTAMP_SDP_VERTICAL_CTRL;
	value |= CLEAR_EN_AUDIO_TIMESTAMP_SDP_VERTICAL_CTRL;

	dptx_writel(
		value,
		base_addr + OFFSET_SDP_VERTICAL_CTRL_N(astream->id));
}

static inline void dptx_audio_send_infoframe_sdp(
	void __iomem *base_addr,
	const struct tcc_audio_stream *astream)
{
	uint32_t value;
	const struct tcc_audio_params *aparams;

	aparams = &astream->aparams;

	/* SDP_REGISTER_BANK_0 */
	// Todo. implementation after soc guide
	// value = readl(
	// 	base_addr + OFFSET_SDP_REGISTER_BANK0_N(astream->id));
	// value &= ~MASK_SDP_REGISTER_BANK0;
	value = 0x481B8400;
	dptx_writel(
		value,
		base_addr + OFFSET_SDP_REGISTER_BANK0_N(astream->id));

	/* SDP_REGISTER_BANK_1 */
	value = readl(
		base_addr + OFFSET_SDP_REGISTER_BANK1_N(astream->id));
	value &= ~MASK_SDP_REGISTER_BANK1;
	switch (aparams->sample_rate) {
		case 32000u:
			value |= SAMPLE_RATE_32000_HZ_SDP_REGISTER_BANK1;
			break;
		case 44100u:
			value |= SAMPLE_RATE_44100_HZ_SDP_REGISTER_BANK1;
			break;
		case 48000u:
			value |= SAMPLE_RATE_48000_HZ_SDP_REGISTER_BANK1;
			break;
		case 88200u:
			value |= SAMPLE_RATE_88200_HZ_SDP_REGISTER_BANK1;
			break;
		case 96000u:
			value |= SAMPLE_RATE_96000_HZ_SDP_REGISTER_BANK1;
			break;
		case 176400u:
			value |= SAMPLE_RATE_176400_HZ_SDP_REGISTER_BANK1;
			break;
		case 192000u:
			value |= SAMPLE_RATE_192000_HZ_SDP_REGISTER_BANK1;
			break;
		default:
			value |= SAMPLE_RATE_32000_HZ_SDP_REGISTER_BANK1;
			break;
	}
	dptx_writel(
		value,
		base_addr + OFFSET_SDP_REGISTER_BANK1_N(astream->id));

	/* SDP_REGISTER_BANK_2 */
	// Todo. implementation after soc guide
}

// Todo. SoC Guide Check
static inline void dptx_audio_reset_infoframe_sdp(
	void __iomem *base_addr,
	const struct tcc_audio_stream *astream)
{
	uint32_t value;
	const struct tcc_audio_params *aparams;

	aparams = &astream->aparams;

	/* SDP_REGISTER_BANK_0 */
	// Todo. implementation after soc guide
	value = readl(
		base_addr + OFFSET_SDP_REGISTER_BANK0_N(astream->id));
	value &= ~MASK_SDP_REGISTER_BANK0;
	value |= ~MASK_SDP_REGISTER_BANK0;
	dptx_writel(
		value,
		base_addr + OFFSET_SDP_REGISTER_BANK0_N(astream->id));

	/* SDP_REGISTER_BANK_1: Sample Rate Setting */
	value = readl(
		base_addr + OFFSET_SDP_REGISTER_BANK1_N(astream->id));
	value &= ~MASK_SDP_REGISTER_BANK1;
	value |= ~MASK_SDP_REGISTER_BANK1;
	dptx_writel(
		value,
		base_addr + OFFSET_SDP_REGISTER_BANK1_N(astream->id));

	/* SDP_REGISTER_BANK_2 */
	// Todo. implementation after soc guide
}

static inline void dptx_audio_enable_mute(
	void __iomem *base_addr,
	const struct tcc_audio_stream *astream)
{
	uint32_t value;
	const struct tcc_audio_params *aparams;

	aparams = &astream->aparams;

	value = readl(
		base_addr + OFFSET_AUD_CONFIG1_N(astream->id));
	value &= ~MASK_AUDIO_MUTE_AUD_CONFIG1;
	value |= SET_AUDIO_MUTE_AUD_CONFIG1;

	dptx_writel(
		value,
		base_addr + OFFSET_AUD_CONFIG1_N(astream->id));
}

static inline void dptx_audio_disable_mute(
	void __iomem *base_addr,
	const struct tcc_audio_stream *astream)
{
	uint32_t value;
	const struct tcc_audio_params *aparams;

	aparams = &astream->aparams;

	value = readl(
		base_addr + OFFSET_AUD_CONFIG1_N(astream->id));
	value &= ~MASK_AUDIO_MUTE_AUD_CONFIG1;
	value |= CLEAR_AUDIO_MUTE_AUD_CONFIG1;

	dptx_writel(
		value,
		base_addr + OFFSET_AUD_CONFIG1_N(astream->id));
}

#endif /* TCC_DPTX_AUDIO_H */

