/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_DPTX_AUDIO_API_H
#define TCC_DPTX_AUDIO_API_H

struct tcc_audio_params {
	uint32_t data_width;
	uint32_t channels;
	uint32_t sample_rate;
};

struct tcc_dptx_audio_ops {
	int32_t (*set_audio_mute)(
			uint8_t stream_id,
			bool mute);
	int32_t (*set_audio_params)(
			uint8_t stream_id,
			const struct tcc_audio_params *cli_aparams);
	int32_t (*enable_audio)(
			uint8_t stream_id);
	int32_t (*disable_audio)(
			uint8_t stream_id);
	void (*dump)(void);
};

#endif /* TCC_DPTX_AUDIO_API_H */
