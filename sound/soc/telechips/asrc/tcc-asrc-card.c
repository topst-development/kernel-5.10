// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/string.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "tcc_asrc_drv.h"
#include "tcc_asrc_dai.h"
#include "../tcc_dai.h"

#undef asrc_card_dbg
#if 0
#define asrc_card_dbg(a...)\
	(void) pr_info("[DEBUG][ASRC_CARD] " a)
#else
#define asrc_card_dbg(a...)
#endif
#define asrc_card_err(a...)\
	(void) pr_err("[ERROR][ASRC_CARD] " a)

#define DAI_LINK_MAX\
	(8U)	// ASRC_PAIR(4) + MCAUDIO(4)
#define ASRC_BE_HDR\
	"MCAudio"

struct tcc_asrc_dai_info_t {
	struct device_node *i2s_of_node;
	struct device_node *codec_of_node;
	const char *codec_dai_name;

	void __iomem *i2s_reg;

	uint32_t dai_fmt;
	uint32_t mclk_div;
	uint32_t bclk_ratio;

	uint32_t samplerate;
	uint32_t format;
	uint32_t channels;

	uint32_t num_of_m2p_pairs;
	uint32_t m2p_pairs[NUM_OF_ASRC_PAIR];
	uint32_t num_of_p2m_pairs;
	uint32_t p2m_pairs[NUM_OF_ASRC_PAIR];

	int peri_dai;
};

struct tcc_asrc_card_info_t {
	struct device_node *asrc_of_node;
	struct device_node *mcaudio_of_node[NUM_OF_ASRC_MCAUDIO];

	uint32_t asrc_path_type[NUM_OF_ASRC_PAIR];

	struct snd_soc_dai_link dai_link[DAI_LINK_MAX];
	uint32_t num_links;
	struct snd_soc_dapm_route dapm_routes[NUM_OF_ASRC_PAIR];
	uint32_t num_dapm_routes;

	uint32_t num_of_asrc_be;
	struct tcc_asrc_dai_info_t dai_info[NUM_OF_ASRC_MCAUDIO];
	struct snd_soc_codec_conf codec_conf[NUM_OF_ASRC_MCAUDIO];
};

static char const *mcaudio_play_widget[] = {
	ASRC_BE_HDR "0 I2S-Playback",
	ASRC_BE_HDR "1 I2S-Playback",
	ASRC_BE_HDR "2 I2S-Playback",
	ASRC_BE_HDR "3 I2S-Playback",
};

static char const *mcaudio_capture_widget[] = {
	ASRC_BE_HDR "0 I2S-Capture",
	ASRC_BE_HDR "1 I2S-Capture",
	ASRC_BE_HDR "2 I2S-Capture",
	ASRC_BE_HDR "3 I2S-Capture",
};

static char const *asrc_fe_play_widget[] = {
	"ASRC-PAIR0-Playback",
	"ASRC-PAIR1-Playback",
	"ASRC-PAIR2-Playback",
	"ASRC-PAIR3-Playback",
};

static char const *asrc_fe_capture_widget[] = {
	"ASRC-PAIR0-Capture",
	"ASRC-PAIR1-Capture",
	"ASRC-PAIR2-Capture",
	"ASRC-PAIR3-Capture",
};

static int mcaudio_hw_params_fixup(
	const struct snd_soc_pcm_runtime *rtd,
	struct snd_pcm_hw_params *params,
	uint32_t dai_be)
{
	const struct tcc_asrc_card_info_t *card_info =
	    snd_soc_card_get_drvdata(rtd->card);
	struct snd_interval *rate;
	struct snd_mask *mask;
	snd_pcm_format_t format;
	int ret = 0;

	asrc_card_dbg("%s\n", __func__);

	if (dai_be >= NUM_OF_ASRC_MCAUDIO) {
		asrc_card_err("%s - dai_be(%d) is greater than or",
			__func__,
			dai_be);
		asrc_card_err("equal to NUM_OF_ASRC_MCAUDIO(%d)\n",
		    NUM_OF_ASRC_MCAUDIO);
		ret = -EINVAL;
	} else {

		tcc_dai_set_dao_path_sel(
			card_info->dai_info[dai_be].i2s_reg,
			TCC_DAI_PATH_ASRC);
		tcc_dai_dma_threshold_enable(
			card_info->dai_info[dai_be].i2s_reg,
			FALSE);

		rate = hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);
		rate->min = card_info->dai_info[dai_be].samplerate;
		rate->max = rate->min;

		//rate = hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);
		//rate->min = card_info->dai_info[dai_be].channels;
		//rate->max = rate->min;

		mask = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);
		snd_mask_none(mask);
		format =
		    (card_info->dai_info[dai_be].format == 0U) ?
			SNDRV_PCM_FORMAT_S16_LE : SNDRV_PCM_FORMAT_S24_LE;
		snd_mask_set(mask, si_to_ui(format));

		ret = 0;
	}
	return ret;
}

static int mcaudio_init(const struct snd_soc_pcm_runtime *rtd, uint32_t dai_be)
{
	const struct tcc_asrc_card_info_t *card_info =
	    snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);

	int ret = 0;

	asrc_card_dbg("%s\n", __func__);

	if (dai_be >= NUM_OF_ASRC_MCAUDIO) {
		asrc_card_err("%s - dai_be(%d) is greater than or",
			__func__,
			dai_be);
		asrc_card_err("equal to NUM_OF_ASRC_MCAUDIO(%d)\n",
		     NUM_OF_ASRC_MCAUDIO);
		ret = -EINVAL;
	} else if (card_info->dai_info[dai_be].mclk_div == 0U) {
		asrc_card_err("mclk_div is 0\n");
		ret = -EINVAL;

	} else if (card_info->dai_info[dai_be].bclk_ratio == 0U) {
		asrc_card_err("bclk_ratio is 0\n");
		ret = -EINVAL;
	} else {
		(void)snd_soc_dai_set_clkdiv(
			cpu_dai,
			(int)TCC_DAI_CLKDIV_ID_MCLK_TO_BCLK,
			ui_to_si(card_info->dai_info[dai_be].mclk_div));

		(void)snd_soc_dai_set_bclk_ratio(
			cpu_dai,
			card_info->dai_info[dai_be].bclk_ratio);

		ret = 0;
	}

	return ret;
}

static int mcaudio0_hw_params_fixup(
	struct snd_soc_pcm_runtime *rtd,
	struct snd_pcm_hw_params *params)
{
	return mcaudio_hw_params_fixup(rtd, params, 0);
}

static int mcaudio0_init(struct snd_soc_pcm_runtime *rtd)
{
	return mcaudio_init(rtd, 0);
}

static int mcaudio1_hw_params_fixup(
	struct snd_soc_pcm_runtime *rtd,
	struct snd_pcm_hw_params *params)
{
	return mcaudio_hw_params_fixup(rtd, params, 1);
}

static int mcaudio1_init(struct snd_soc_pcm_runtime *rtd)
{
	return mcaudio_init(rtd, 1);
}

static int mcaudio2_hw_params_fixup(
	struct snd_soc_pcm_runtime *rtd,
	struct snd_pcm_hw_params *params)
{
	return mcaudio_hw_params_fixup(rtd, params, 2);
}

static int mcaudio2_init(struct snd_soc_pcm_runtime *rtd)
{
	return mcaudio_init(rtd, 2);
}

static int mcaudio3_hw_params_fixup(
	struct snd_soc_pcm_runtime *rtd,
	struct snd_pcm_hw_params *params)
{
	return mcaudio_hw_params_fixup(rtd, params, 3);
}

static int mcaudio3_init(struct snd_soc_pcm_runtime *rtd)
{
	return mcaudio_init(rtd, 3);
}

static struct snd_soc_dai_link asrc_be_link[] = {
	{
	 .name = "MCAUDIO0",
	 .stream_name = "MCAUDIO0",
	 .be_hw_params_fixup = mcaudio0_hw_params_fixup,
	 .init = mcaudio0_init,
	 .dpcm_playback = 1,
	 .dpcm_capture = 1,
	 .no_pcm = 1,
	 },
	{
	 .name = "MCAUDIO1",
	 .stream_name = "MCAUDIO1",
	 .be_hw_params_fixup = mcaudio1_hw_params_fixup,
	 .init = mcaudio1_init,
	 .dpcm_playback = 1,
	 .dpcm_capture = 1,
	 .no_pcm = 1,
	 },
	{
	 .name = "MCAUDIO2",
	 .stream_name = "MCAUDIO2",
	 .be_hw_params_fixup = mcaudio2_hw_params_fixup,
	 .init = mcaudio2_init,
	 .dpcm_playback = 1,
	 .dpcm_capture = 1,
	 .no_pcm = 1,
	 },
	{
	 .name = "MCAUDIO3",
	 .stream_name = "MCAUDIO3",
	 .be_hw_params_fixup = mcaudio3_hw_params_fixup,
	 .init = mcaudio3_init,
	 .dpcm_playback = 1,
	 .dpcm_capture = 1,
	 .no_pcm = 1,
	 },
};

#if 0
static const struct snd_soc_dapm_route audio_map[] = {
	{"I2S-Playback", NULL, "ASRC-PAIR0-Playback"},
	{"I2S-Playback", NULL, "ASRC-PAIR1-Playback"},
	{"I2S-Playback", NULL, "ASRC-PAIR2-Playback"},
	{"ASRC-PAIR3-Capture", NULL, "I2S-Capture"},
};

static struct snd_soc_dai_link tcc_dai[] = {
	{
	 .name = "ASRC Pair 0",
	 .stream_name = "ASRC Pair0 stream",
	 .cpu_dai_name = "ASRC-PAIR0",
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 .dpcm_playback = 1,
	 .dynamic = 1,
	 },
	{
	 .name = "ASRC Pair 1",
	 .stream_name = "ASRC Pair1 stream",
	 .cpu_dai_name = "ASRC-PAIR1",
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 .dpcm_playback = 1,
	 .dynamic = 1,
	 },
	{
	 .name = "ASRC Pair 2",
	 .stream_name = "ASRC Pair2 stream",
	 .cpu_dai_name = "ASRC-PAIR2",
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 .dpcm_playback = 1,
	 .dynamic = 1,
	 },
	{
	 .name = "ASRC Pair 3",
	 .stream_name = "ASRC Pair3 stream",
	 .cpu_dai_name = "ASRC-PAIR3",
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 .dpcm_playback = 1,
	 .dynamic = 1,
	 },
	{
	 .name = "MCAUDIO",
	 .stream_name = "MCAUDIO Stream",
	 .platform_name = "snd-soc-dummy",
	 .be_hw_params_fixup = mcaudio_hw_params_fixup,
	 .dpcm_playback = 1,
	 .dpcm_capture = 1,
	 .no_pcm = 1,
	 },
};
#endif

static struct tcc_asrc_dai_info_t *asrc_get_dai_info_for_pair(
	struct tcc_asrc_card_info_t *card_info,
	uint32_t asrc_pair)
{
	uint32_t i, j;
	struct tcc_asrc_dai_info_t *ret = NULL;

	for (j = 0; j < card_info->num_of_asrc_be; j++) {
		for (i = 0; i < card_info->dai_info[j].num_of_m2p_pairs; i++) {
			if (asrc_pair == card_info->dai_info[j].m2p_pairs[i]) {
				ret = &card_info->dai_info[j];
			}
		}

		if(ret == NULL) {
			for (i = 0; i < card_info->dai_info[j].num_of_p2m_pairs; i++) {
				if (asrc_pair == card_info->dai_info[j].p2m_pairs[i]) {
					ret = &card_info->dai_info[j];
					break;
				}
			}
		}
		if(ret != NULL) { break; };
	}

	return ret;
}

static int asrc_fe_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct tcc_asrc_card_info_t *card_info =
	    snd_soc_card_get_drvdata(rtd->card);
	//struct tcc_asrc_t *asrc =
	//(struct tcc_asrc_t*)snd_soc_dai_get_drvdata(cpu_dai);
	const struct tcc_asrc_dai_info_t *dai_info;
	uint32_t asrc_pair = si_to_ui(cpu_dai->id);
	int ret = -EINVAL;

	asrc_card_dbg("%s\n", __func__);

	if (card_info != NULL) {
		dai_info = asrc_get_dai_info_for_pair(card_info, asrc_pair);
		if (dai_info == NULL) {
			asrc_card_err("dai_info is NULL\n");
			ret = -EINVAL;
		} else if (dai_info->peri_dai < 0) {
			ret = -EINVAL;
		} else {

			(void)snd_soc_dai_set_sysclk(
				cpu_dai,
				TCC_ASRC_CLKID_PERI_DAI_RATE,
				dai_info->samplerate,
				0);
			(void)snd_soc_dai_set_sysclk(
				cpu_dai,
				TCC_ASRC_CLKID_PERI_DAI_FORMAT,
				dai_info->format,
				0);
			(void)snd_soc_dai_set_sysclk(
				cpu_dai,
				TCC_ASRC_CLKID_PERI_DAI,
				(uint32_t)dai_info->peri_dai,
				0);

			ret = 0;
		}
	}

	return ret;
}

static struct snd_soc_dai_link asrc_fe_play_link[] = {
	{
	 .name = "ASRC Pair0",
	 .stream_name = "ASRC Pair0",
	 .dpcm_playback = 1,
	 .dynamic = 1,
	 .init = asrc_fe_init,
	 },
	{
	 .name = "ASRC Pair1",
	 .stream_name = "ASRC Pair1",
	 .dpcm_playback = 1,
	 .dynamic = 1,
	 .init = asrc_fe_init,
	 },
	{
	 .name = "ASRC Pair2",
	 .stream_name = "ASRC Pair2",
	 .dpcm_playback = 1,
	 .dynamic = 1,
	 .init = asrc_fe_init,
	 },
	{
	 .name = "ASRC Pair3",
	 .stream_name = "ASRC Pair3",
	 .dpcm_playback = 1,
	 .dynamic = 1,
	 .init = asrc_fe_init,
	 },
};

static struct snd_soc_dai_link asrc_fe_capture_link[] = {
	{
	 .name = "ASRC Pair0",
	 .stream_name = "ASRC Pair0",
	 .dpcm_capture = 1,
	 .dynamic = 1,
	 .init = asrc_fe_init,
	 },
	{
	 .name = "ASRC Pair1",
	 .stream_name = "ASRC Pair1",
	 .dpcm_capture = 1,
	 .dynamic = 1,
	 .init = asrc_fe_init,
	 },
	{
	 .name = "ASRC Pair2",
	 .stream_name = "ASRC Pair2",
	 .dpcm_capture = 1,
	 .dynamic = 1,
	 .init = asrc_fe_init,
	 },
	{
	 .name = "ASRC Pair3",
	 .stream_name = "ASRC Pair3",
	 .dpcm_capture = 1,
	 .dynamic = 1,
	 .init = asrc_fe_init,
	 },
};

static uint32_t check_pcm_count(const struct tcc_asrc_card_info_t *card_info)
{
	uint32_t i;
	uint32_t count = 0;

	for (i = 0; i < card_info->num_of_asrc_be; i++) {
		count = ui_add(count, card_info->dai_info[i].num_of_m2p_pairs);
		count = ui_add(count, card_info->dai_info[i].num_of_p2m_pairs);
	}

	count = ui_add(count, card_info->num_of_asrc_be);	// for MCAUDIO

	return count;
}

static inline const char *get_asrc_dai_name(uint32_t asrc_pair){
	const char *dai_name;

	switch(asrc_pair) {
		case 0:
			dai_name = "ASRC-PAIR0";
			break;
		case 1:
			dai_name = "ASRC-PAIR1";
			break;
		case 2:
			dai_name = "ASRC-PAIR2";
			break;
		case 3:
			dai_name = "ASRC-PAIR3";
			break;
		default:
			asrc_card_err("asrc_pair%d is not ASRC Pair\n", asrc_pair);
			dai_name = NULL;
			break;
	}
	return dai_name;
}

static int setup_dai_link_sub_alloc_dai_link(struct tcc_asrc_card_info_t *card_info, size_t  alloc_size){
	uint32_t j;
	int ret = 0;

	do {
		card_info->num_links = check_pcm_count(card_info);
		if (card_info->num_links > DAI_LINK_MAX) {
			asrc_card_err("num_links(%d) is greater than", card_info->num_links);
			asrc_card_err("DAI_LINK_MAX(%d)\n", DAI_LINK_MAX);
			ret = -EINVAL;
			continue;
		}

		asrc_card_dbg("num_links : %d\n", card_info->num_links);

		for(j = 0; j < card_info->num_links; j++) {
			card_info->dai_link[j].cpus =
				kzalloc((size_t) alloc_size, GFP_KERNEL);
			card_info->dai_link[j].platforms =
				kzalloc((size_t) alloc_size, GFP_KERNEL);
			card_info->dai_link[j].codecs =
				kzalloc((size_t) alloc_size, GFP_KERNEL);
			if ((card_info->dai_link[j].cpus == NULL)
					||(card_info->dai_link[j].platforms == NULL)
					||(card_info->dai_link[j].codecs == NULL)){
				asrc_card_err("DAI_LINK alloc fail!! dai_link[%d]\n", j);
				ret = -ENOMEM;
				break;
			}
		}
	} while (false);
	return ret;
}

static int setup_dai_link_sub_alloc_fe_link(struct tcc_asrc_card_info_t *card_info, struct snd_soc_dai_link *dai_link, size_t  alloc_size){
	struct snd_soc_dapm_route *dapm_routes = NULL;
	uint32_t asrc_pair;
	uint32_t asrc_mcaudio;
	uint32_t asrc_dapm;
	uint32_t i, j;
	int ret = 0;

	for (j = 0; j < card_info->num_of_asrc_be; j++) {
		const struct tcc_asrc_dai_info_t *dai_info = &card_info->dai_info[j];
		uint32_t *num_dapm_routes = &card_info->num_dapm_routes;

		for (i = 0; i < dai_info->num_of_m2p_pairs; i++) {
			asrc_pair = dai_info->m2p_pairs[i];
			if(card_info->asrc_path_type[asrc_pair] == (uint32_t)TCC_ASRC_M2P_PATH) {
				asrc_fe_play_link[asrc_pair].cpus =
					kzalloc((size_t) alloc_size, GFP_KERNEL);
				asrc_fe_play_link[asrc_pair].platforms =
					kzalloc((size_t) alloc_size, GFP_KERNEL);
				asrc_fe_play_link[asrc_pair].codecs =
					kzalloc((size_t) alloc_size, GFP_KERNEL);
				if ((asrc_fe_play_link[asrc_pair].cpus == NULL)
						||(asrc_fe_play_link[asrc_pair].platforms == NULL)
						||(asrc_fe_play_link[asrc_pair].codecs == NULL)){
					asrc_card_err("asrc_fe_play_link[%d] alloc fail!!\n"
								, asrc_pair);
					ret = -ENOMEM;
				} else {
					asrc_fe_play_link[asrc_pair].cpus->dai_name = get_asrc_dai_name(asrc_pair);
					asrc_fe_play_link[asrc_pair].num_cpus = 1;

					asrc_fe_play_link[asrc_pair].codecs->name = "snd-soc-dummy";
					asrc_fe_play_link[asrc_pair].codecs->dai_name = "snd-soc-dummy-dai";
					asrc_fe_play_link[asrc_pair].num_codecs = 1;
					(void)memcpy(dai_link, &asrc_fe_play_link[asrc_pair],
					       sizeof(struct snd_soc_dai_link));

					dai_link->cpus->of_node = card_info->asrc_of_node;
					dai_link->platforms->of_node =
						card_info->asrc_of_node;
					dai_link++;

					asrc_dapm = (uint32_t) *num_dapm_routes;
					if (asrc_dapm < NUM_OF_ASRC_PAIR) {
						dapm_routes =
							&card_info->dapm_routes[*num_dapm_routes];
						(*num_dapm_routes)++;
					} else {
						dapm_routes =
						&card_info->dapm_routes[asrc_dapm - 1U];
					}
					asrc_mcaudio = si_to_ui(dai_info->peri_dai);
					dapm_routes->sink =
						mcaudio_play_widget[asrc_mcaudio];
					dapm_routes->control = NULL;
					dapm_routes->source =
						asrc_fe_play_widget[asrc_pair];
					dapm_routes->connected = NULL;
				}
			}else {
				asrc_card_err("asrc_pair%d is not M2P path\n", asrc_pair);
				ret = -EINVAL;
			}
			if (ret < 0) { break; }
		}
		if (ret < 0) { break; }

		for (i = 0; i < dai_info->num_of_p2m_pairs; i++) {
			asrc_pair = dai_info->p2m_pairs[i];
			if(card_info->asrc_path_type[asrc_pair] == (unsigned)TCC_ASRC_P2M_PATH){
				asrc_fe_capture_link[asrc_pair].cpus =
					kzalloc((size_t) alloc_size, GFP_KERNEL);
				asrc_fe_capture_link[asrc_pair].platforms =
					kzalloc((size_t) alloc_size, GFP_KERNEL);
				asrc_fe_capture_link[asrc_pair].codecs =
					kzalloc((size_t) alloc_size, GFP_KERNEL);
				if ((asrc_fe_capture_link[asrc_pair].cpus == NULL)
						||(asrc_fe_capture_link[asrc_pair].platforms == NULL)
						||(asrc_fe_capture_link[asrc_pair].codecs == NULL)){
					asrc_card_err("asrc_fe_capture_link[%d] alloc fail!!\n"
								, asrc_pair);
					ret = -ENOMEM;
				} else {
					asrc_fe_capture_link[asrc_pair].cpus->dai_name = get_asrc_dai_name(asrc_pair);
					asrc_fe_capture_link[asrc_pair].num_cpus = 1;

					asrc_fe_capture_link[asrc_pair].codecs->name = "snd-soc-dummy";
					asrc_fe_capture_link[asrc_pair].codecs->dai_name = "snd-soc-dummy-dai";
					asrc_fe_capture_link[asrc_pair].num_codecs = 1;
					(void)memcpy(dai_link,
					       &asrc_fe_capture_link[asrc_pair],
					       sizeof(struct snd_soc_dai_link));

					dai_link->cpus->of_node = card_info->asrc_of_node;
					dai_link->platforms->of_node =
					    card_info->asrc_of_node;
					dai_link++;

					asrc_dapm = (uint32_t) *num_dapm_routes;
					if (asrc_dapm < NUM_OF_ASRC_PAIR) {
						dapm_routes =
							&card_info->dapm_routes[*num_dapm_routes];
						(*num_dapm_routes)++;
					} else {
						dapm_routes =
						&card_info->dapm_routes[asrc_dapm - 1U];
					}

					dapm_routes->sink =
					    asrc_fe_capture_widget[asrc_pair];
					dapm_routes->control = NULL;

					asrc_mcaudio = si_to_ui(dai_info->peri_dai);
					dapm_routes->source =
					    mcaudio_capture_widget[asrc_mcaudio];
					dapm_routes->connected = NULL;
				}
			} else {
				asrc_card_err("asrc_pair%d is not P2M path\n",
				     asrc_pair);
				ret = -EINVAL;
			}
			if (ret < 0) { break; }
		}
	}

	return ret;
}

static int setup_dai_link_sub_alloc_be_link(struct tcc_asrc_card_info_t *card_info, struct snd_soc_dai_link *dai_link, size_t  alloc_size){
	uint32_t i, j;
	int ret = 0;

	for (i = 0; i < card_info->num_of_asrc_be; i++) {
		const struct tcc_asrc_dai_info_t *dai_info = &card_info->dai_info[i];
		char tmp_name[255];

		if (dai_info->i2s_of_node == NULL) {
			ret = -EINVAL;
		} else {

			asrc_be_link[i].cpus =
				kzalloc((size_t) alloc_size, GFP_KERNEL);
			asrc_be_link[i].platforms =
				kzalloc((size_t) alloc_size, GFP_KERNEL);
			asrc_be_link[i].codecs =
				kzalloc((size_t) alloc_size, GFP_KERNEL);

			if ((asrc_be_link[i].cpus == NULL)
					||(asrc_be_link[i].platforms == NULL)
					||(asrc_be_link[i].codecs == NULL)){
				asrc_card_err("asrc_be_link[%d] alloc fail!!\n"
						, i);
				ret = -ENOMEM;
			} else {
				asrc_be_link[i].cpus->name = NULL;
				asrc_be_link[i].cpus->dai_name = NULL;
				asrc_be_link[i].num_cpus = 1;
				asrc_be_link[i].platforms->name = "snd-soc-dummy";
				asrc_be_link[i].num_platforms = 1;
				(void)memcpy(dai_link, &asrc_be_link[i],
						sizeof(struct snd_soc_dai_link));

				dai_link->cpus->of_node = dai_info->i2s_of_node;
				if (dai_info->codec_of_node != NULL) {
					dai_link->codecs->of_node = dai_info->codec_of_node;
					dai_link->codecs->name = NULL;
					dai_link->codecs->dai_name = dai_info->codec_dai_name;
				} else {
					dai_link->codecs->of_node = NULL;
					dai_link->codecs->name = "snd-soc-dummy";
					dai_link->codecs->dai_name = "snd-soc-dummy-dai";
				}

				asrc_card_dbg("codec name %s, dai_name %s\n", dai_link->codecs->name, dai_link->codecs->dai_name);

				dai_link->num_codecs = 1;
				dai_link->dai_fmt = dai_info->dai_fmt;

				ret = scnprintf(tmp_name, sizeof(tmp_name), ASRC_BE_HDR "%d", dai_info->peri_dai);

				if (ret < 0) {
					asrc_card_err("sprintf is error : %d\n", ret);
				} else {
					card_info->codec_conf[i].name_prefix =
						kstrdup(tmp_name, GFP_KERNEL);
					if (card_info->codec_conf[i].name_prefix == NULL) {
						uint32_t not_failed_name_count = i;

						for (j = 0; j < not_failed_name_count; j++){
							kfree(card_info->codec_conf[j].name_prefix);
						}

						asrc_card_err("kstrdup is error\n");
						ret = -ENOMEM;
					} else {
						card_info->codec_conf[i].dlc.of_node = dai_info->i2s_of_node;
						asrc_card_dbg("name_prefix(%d) : %s\n", i,
							card_info->codec_conf[i].name_prefix);

						dai_link++;
					}
				}
			}
		}
		if (ret < 0) { break; }
	}

	return ret;
}



static int setup_dai_link(struct tcc_asrc_card_info_t *card_info)
{
	struct snd_soc_dai_link *dai_link = NULL;
	uint32_t i;
	int ret = 0;
	size_t alloc_size = (size_t)(sizeof(struct snd_soc_dai_link_component));

	do {
		ret = setup_dai_link_sub_alloc_dai_link(card_info, alloc_size);
		if(ret < 0) { continue; }

		dai_link = card_info->dai_link;

		if (card_info->asrc_of_node == NULL) {
			ret = -EINVAL;
			continue;
		}

		card_info->num_dapm_routes = 0;

		ret = setup_dai_link_sub_alloc_fe_link(card_info, dai_link, alloc_size);
		if (ret < 0) { continue; }

		i = ui_sub(card_info->num_links, card_info->num_of_asrc_be);
		ret = setup_dai_link_sub_alloc_be_link(card_info, &dai_link[i], alloc_size);
		if (ret < 0) { continue; }

		for (i = 0; i < DAI_LINK_MAX; i++) {
			asrc_card_dbg("dai_link[%d].dai_fmt : 0x%08x\n", i,
				      card_info->dai_link[i].dai_fmt);
		}

		for (i = 0; i < card_info->num_dapm_routes; i++) {
			asrc_card_dbg("sink : %s, source : %s\n",
				      card_info->dapm_routes[i].sink,
				      card_info->dapm_routes[i].source);
		}

		ret = 0;
	} while (false);

	return ret;
}

static int parse_tcc_asrc_be_dai(
	struct device_node *np,
	const struct tcc_asrc_card_info_t *card_info,
	struct tcc_asrc_dai_info_t *dai_info)
{
	uint32_t asrc_pair;
	uint32_t i;
	int ret = 0;
    int prop;

	do {
		dai_info->i2s_of_node = of_parse_phandle(np, "i2s", 0);
		if (dai_info->i2s_of_node == NULL) {
			asrc_card_err("i2s node is not exist\n");
			ret = -EINVAL;
			continue;
		}
		dai_info->codec_of_node = of_parse_phandle(np, "codec", 0);

		asrc_card_dbg("\ti2s_of_node=%px\n", dai_info->i2s_of_node);

		(void)of_property_read_string(np, "codec,dai-name",
					&dai_info->codec_dai_name);

		dai_info->peri_dai = ui_to_si(NUM_OF_ASRC_MCAUDIO);
		for (i = 0; i < NUM_OF_ASRC_MCAUDIO; i++) {
			if (card_info->mcaudio_of_node[i] == dai_info->i2s_of_node) {
				dai_info->peri_dai = (int)i;
			}
		}
		if (dai_info->peri_dai >= ui_to_si(NUM_OF_ASRC_MCAUDIO)) {
			asrc_card_err("ASRC can't connect the i2s block\n");
			ret = -EINVAL;
			continue;
		}
		asrc_card_dbg("\tperi_dai=%d\n", dai_info->peri_dai);

		dai_info->i2s_reg = of_iomap(dai_info->i2s_of_node, 0);
		if (IS_ERR((void *)dai_info->i2s_reg)) {
			asrc_card_err("i2s_reg is NULL\n");
			ret = -EINVAL;
			continue;
		}
		asrc_card_dbg("\ti2s_reg=%px\n", dai_info->i2s_reg);

		prop = of_property_count_elems_of_size(
				np,
				"asrc-m2p-pairs",
				ul_to_si(sizeof(uint32_t)));
		dai_info->num_of_m2p_pairs = si_to_ui((prop < 0) ? 0 : prop);
		(void)of_property_read_u32_array(
			np,
			"asrc-m2p-pairs",
			dai_info->m2p_pairs,
			dai_info->num_of_m2p_pairs);

		prop = of_property_count_elems_of_size(
				np,
				"asrc-p2m-pairs",
				ul_to_si(sizeof(uint32_t)));
		dai_info->num_of_p2m_pairs = si_to_ui((prop < 0) ? 0 : prop);
		(void)of_property_read_u32_array(
			np,
			"asrc-p2m-pairs",
			dai_info->p2m_pairs,
			dai_info->num_of_p2m_pairs);

		asrc_card_dbg("\tnum_of_m2p_pairs: %d\n", dai_info->num_of_m2p_pairs);
		for (i = 0; i < dai_info->num_of_m2p_pairs; i++) {
			asrc_pair = dai_info->m2p_pairs[i];

			if (asrc_pair >= NUM_OF_ASRC_PAIR) {
				asrc_card_err("ASRC Pair%d is bigger than",
					asrc_pair);
				asrc_card_err("NUM_OF_ASRC_PAIR(%d)\n",
				     NUM_OF_ASRC_PAIR);
				ret = -EINVAL;
			} else if (card_info->asrc_path_type[asrc_pair] != (uint32_t)TCC_ASRC_M2P_PATH) {
				asrc_card_err("ASRC Pair%d is not M2P path type\n",
				     asrc_pair);
				ret = -EINVAL;
			} else {
				asrc_card_dbg("\tm2p_pairs[%d]: %d\n", i,
					      dai_info->m2p_pairs[i]);
			}

			if (ret < 0) { break; }
		}

		if (ret < 0) { continue; }

		asrc_card_dbg("\tnum_of_p2m_pairs: %d\n", dai_info->num_of_p2m_pairs);
		for (i = 0; i < dai_info->num_of_p2m_pairs; i++) {
			asrc_pair = dai_info->p2m_pairs[i];

			if (asrc_pair >= NUM_OF_ASRC_PAIR) {
				asrc_card_err("ASRC Pair%d is bigger than NUM_OF_ASRC_PAIR(%d)\n",
				     asrc_pair, NUM_OF_ASRC_PAIR);
				ret = -EINVAL;
			} else if (card_info->asrc_path_type[asrc_pair] != (unsigned)TCC_ASRC_P2M_PATH) {
				asrc_card_err("ASRC Pair%d is not P2M path type\n",
				     asrc_pair);
				ret = -EINVAL;
			} else {
				asrc_card_dbg("\tp2m_pairs[%d]: %d\n", i,
						dai_info->p2m_pairs[i]);
			}

			if (ret < 0) { break; }
		}

		if (ret < 0) { continue; }

		// parse configrations
		dai_info->dai_fmt = snd_soc_of_parse_daifmt(np, "codec,", NULL, NULL);
		asrc_card_dbg("\tdai_fmt : 0x%08x\n", dai_info->dai_fmt);

		(void)of_property_read_u32(np, "mclk_div", &dai_info->mclk_div);
		asrc_card_dbg("\tmclk_div : %d\n", dai_info->mclk_div);

		(void)of_property_read_u32(np, "bclk_ratio", &dai_info->bclk_ratio);
		asrc_card_dbg("\tbclk_ratio: %d\n", dai_info->bclk_ratio);

		(void)of_property_read_u32(np, "samplerate", &dai_info->samplerate);
		dai_info->samplerate =
		    (dai_info->samplerate == 0U) ? 48000U : dai_info->samplerate;
		asrc_card_dbg("\tsamplerate: %d\n", dai_info->samplerate);

		(void)of_property_read_u32(np, "format", &dai_info->format);
		asrc_card_dbg("\tformat: %d\n", dai_info->format);

		(void)of_property_read_u32(np, "channels", &dai_info->channels);
		dai_info->channels = (dai_info->channels == 0U) ? 2U : dai_info->channels;
		asrc_card_dbg("\tchannels: %d\n", dai_info->channels);

		ret = 0;
	} while (false);

	return ret;
}

static int parse_tcc_asrc_card_dt(
	const struct platform_device *pdev,
	struct tcc_asrc_card_info_t *card_info)
{
	const struct device_node *np = pdev->dev.of_node;
	int ret = 0;
	uint32_t i;

	(void)memset(card_info, 0, sizeof(struct tcc_asrc_card_info_t));
	card_info->asrc_of_node = of_parse_phandle(np, "asrc", 0);
	if (card_info->asrc_of_node == NULL) {
		asrc_card_err("asrc node is not exist\n");
		ret = -EINVAL;
	} else {
		(void)of_property_read_u32_array(
			card_info->asrc_of_node,
			"path-type",
			card_info->asrc_path_type,
			NUM_OF_ASRC_PAIR);

		for (i = 0; i < NUM_OF_ASRC_MCAUDIO; i++) {
			card_info->mcaudio_of_node[i] =
			    of_parse_phandle(card_info->asrc_of_node, "mcaudio", (int)i);
			asrc_card_dbg("of_node_mcaudio[%d] : %px\n",
				i,
				card_info->mcaudio_of_node[i]);
		}

		card_info->num_of_asrc_be = (uint32_t)of_get_child_count(np);

		if (of_get_child_by_name(np, "telechips,dai-link") != NULL) {
			struct device_node *be_np = NULL;
			i = 0;

			for_each_child_of_node((np), (be_np)) {
				asrc_card_dbg("link %d:\n", i);
				if (i < NUM_OF_ASRC_MCAUDIO) {
					ret = parse_tcc_asrc_be_dai(
						be_np,
						card_info,
						&card_info->dai_info[i]);
					i++;
				} else {
					ret = -1;
				}

				if (ret < 0) { break; }
			}
		}
	}

	return ret;
}

static inline struct tcc_asrc_dai_info_t *tcc_asrc_card_get_dai_info(
	struct tcc_asrc_card_info_t *card_info,
	struct snd_soc_dai *dai)
{
	struct tcc_asrc_dai_info_t *ret = NULL;
	uint32_t i, j;

	j = ui_sub(card_info->num_links, card_info->num_of_asrc_be);
	if(j != 0U) {
		for (i = 0; i < card_info->num_links; i++) {
			if (card_info->dai_link[i].cpus->of_node == dai->dev->of_node) {
				ret = &card_info->dai_info[i % j];
				break;
			}
		}
	}

	return ret;
}

static int tcc_asrc_card_late_probe(struct snd_soc_card *card)
{
	struct tcc_asrc_card_info_t *card_info = snd_soc_card_get_drvdata(card);
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_dai *cpu_dai = NULL;
	struct tcc_asrc_dai_info_t *dai_info = NULL;
	unsigned int mclk = 0;
	int32_t ret = 0;
	uint32_t i = 0;

	asrc_card_dbg("%s\n", __func__);

	/* backend dai_link only probe */
	i = ui_sub(card_info->num_links, card_info->num_of_asrc_be);
	do {
		rtd = snd_soc_get_pcm_runtime(card, &card->dai_link[i]);
		cpu_dai = asoc_rtd_to_cpu(rtd, 0);
		dai_info = tcc_asrc_card_get_dai_info(card_info, cpu_dai);

		if(dai_info != NULL) {
			if ((dai_info->dai_fmt & si_to_ui(SND_SOC_DAIFMT_CLOCK_MASK))
					== si_to_ui(SND_SOC_DAIFMT_CONT)){

				ret = snd_soc_dai_set_sysclk(cpu_dai,
						(int)TCC_DAI_MCLK,
						mclk,
						SND_SOC_CLOCK_OUT);
				if ((ret != 0) && (ret != -ENOTSUPP)) {
					break;
				}
			}
		} else {
			ret = -ENOLINK;
			asrc_card_err("%s : fail to get dai information in dai-link %d.\n", __func__, i);
		}

		i = ui_add(i, 1);
	} while ((dai_info != NULL) && (ui_to_si(i) < card->num_rtd));

	return ret;
}

static int tcc_asrc_card_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = NULL;
	struct tcc_asrc_card_info_t *card_info = NULL;
	int ret = 0;

	do {
		card = kzalloc(sizeof(struct snd_soc_card), GFP_KERNEL);
		if (card == NULL) {
			ret = -ENOMEM;
			continue;
		}

		card_info = kzalloc(
			sizeof(struct tcc_asrc_card_info_t),
			GFP_KERNEL);

		if (card_info  == NULL) {
			ret = -ENOMEM;
			kfree(card);
			continue;
		}

		card->dev = &pdev->dev;
		card->late_probe = tcc_asrc_card_late_probe;
		platform_set_drvdata(pdev, card);
		snd_soc_card_set_drvdata(card, card_info);
		ret = snd_soc_of_parse_card_name(card, "card-name");
		if (ret < 0) {
			ret = -EINVAL;
			kfree(card_info);
			continue;
		}

		asrc_card_dbg("%s %s\n", __func__, card->name);
		ret = parse_tcc_asrc_card_dt(pdev, card_info);
		if (ret < 0) {
			asrc_card_err("%s: device tree parsing error\n", __func__);
			kfree(card_info);
			continue;
		}

		ret = setup_dai_link(card_info);
		if (ret < 0) {
			asrc_card_err("%s: setup dai failed\n", __func__);
			kfree(card_info);
			continue;
		}

		card->driver_name = "tcc-asrc-card";
		card->dai_link = card_info->dai_link;
		card->num_links = ui_to_si(card_info->num_links);


		card->dapm_routes = card_info->dapm_routes;
		card->num_dapm_routes = ui_to_si(card_info->num_dapm_routes);


		card->codec_conf = card_info->codec_conf;
		card->num_configs = ui_to_si(card_info->num_of_asrc_be);


		ret = snd_soc_register_card(card);
		if (ret < 0) {
			dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
			kfree(card_info);
			continue;
		}

		ret = 0;
	} while (false);

	return ret;
}

static int tcc_asrc_card_remove(struct platform_device *pdev)
{
	const struct tcc_asrc_card_info_t *card_info = NULL;
	struct snd_soc_card *card = NULL;

	card = platform_get_drvdata(pdev);

	if (card != NULL) {
		card_info = snd_soc_card_get_drvdata(card);
		kfree(card_info);
		kfree(card);
	}

	return 0;
}

static const struct of_device_id tcc_asrc_card_of_match[] = {
	{.compatible = "telechips,asrc-card",},
	{},
};

MODULE_DEVICE_TABLE(of, tcc_asrc_card_of_match);

static struct platform_driver tcc_asrc_card_driver = {
	.driver = {
		   .name = "tcc-asrc-card",
		   .owner = THIS_MODULE,
		   .pm = &snd_soc_pm_ops,
		   .of_match_table = tcc_asrc_card_of_match,
		   },
	.probe = tcc_asrc_card_probe,
	.remove = tcc_asrc_card_remove,
};

module_platform_driver(tcc_asrc_card_driver);

MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips ASRC Card");
MODULE_LICENSE("GPL");
