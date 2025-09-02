/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_SND_CARD_H
#define TCC_SND_CARD_H

struct tcc_hw_params {
	unsigned int rate;
	unsigned int channels;
	snd_pcm_format_t format;
};

#define MAX_NUM_MASRC	(6)
#define MAX_NUM_MIX		(6)
#define MAX_NUM_MARS	(6)
#define MAX_NUM_VOL		(8)

struct tcc_card_info_t {
	int32_t num_links;
	struct snd_soc_dai_link *dai_link;
	struct tcc_dai_info_t *dai_info;
	struct snd_soc_codec_conf *codec_conf;

	struct tcc_masrc_t *dev_masrc[MAX_NUM_MASRC];
	struct tcc_mixer_t *dev_mix[MAX_NUM_MIX];
	struct tcc_mars_t *dev_mars[MAX_NUM_MARS];
	struct tcc_volume_t *dev_vol[MAX_NUM_VOL];

	struct snd_pcm_substream *substream;
	struct tcc_hw_params fe_params;
	struct tcc_hw_params be_params;
};

#endif /*_TCC_SND_CARD_H*/
