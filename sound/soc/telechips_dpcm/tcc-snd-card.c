// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "tcc_audio_daif.h"
#include "tcc_audio_rule.h"
#include "tcc-snd-card.h"
#include "tcc_widgets.h"
#if defined(CONFIG_SND_SOC_TELECHIPS_EXTENDED_TBD)
#include "tcc_audio_fifo_ctl_pcm.h"
#endif

//#define TCC_SND_CARD_DEBUG

#undef snd_card_dbg
#if 0
#define snd_card_dbg(a...) \
	(void) pr_info("[DEBUG][SOUND_CARD] " a)
#else
#define snd_card_dbg(a...)
#endif
#define snd_card_err(a...) \
	(void) pr_err("[ERROR][SOUND_CARD] " a)

#define tcc_snd_card_sprintf(a...) \
	(void) pr_err("[ERROR][SOUND_CARD] " a)


#define DRIVER_NAME \
	("tcc-snd-card")
#define DAI_LINK_MAX \
	(20)
#define KCONTROL_HDR \
	"Device"

#define TCC_SND_SOC_DUMMY_NAME		"snd-soc-dummy"
#define TCC_SND_SOC_DUMMY_DAI_NAME	"snd-soc-dummy-dai"

struct tcc_dai_info_t {
	struct snd_soc_dai *dai;
	uint32_t mclk_div;
	uint32_t bclk_ratio;
	int32_t tdm_slots;
	int32_t tdm_width;
	uint32_t dai_fmt;
	bool is_updated;
	uint32_t fixup_samplerate;
	uint32_t fixup_format;
	uint32_t fixup_channels;
};

#if defined(CONFIG_SND_SOC_TELECHIPS_EXTENDED_TBD)
static const struct snd_soc_dapm_widget tcc_snd_card_widgets[] = {
	SND_SOC_DAPM_SRC_E(ASRC0_WIDGET, SND_SOC_NOPM, 0, 0,
					tcc_masrc0_event,
					SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |  SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_REG
					| SND_SOC_DAPM_POST_REG | SND_SOC_DAPM_WILL_PMU | SND_SOC_DAPM_WILL_PMD),

	SND_SOC_DAPM_SRC_E(ASRC1_WIDGET, SND_SOC_NOPM, 0, 0,
					tcc_masrc1_event,
					SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |  SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_REG
					| SND_SOC_DAPM_POST_REG | SND_SOC_DAPM_WILL_PMU | SND_SOC_DAPM_WILL_PMD),

	SND_SOC_DAPM_SRC_E(ASRC2_WIDGET, SND_SOC_NOPM, 0, 0,
					tcc_masrc2_event,
					SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |  SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_REG
					| SND_SOC_DAPM_POST_REG | SND_SOC_DAPM_WILL_PMU | SND_SOC_DAPM_WILL_PMD),

	SND_SOC_DAPM_PGA_E(VOLUME0_WIDGET, SND_SOC_NOPM, 0, 0,
					tcc_volume_controls0, ARRAY_SIZE(tcc_volume_controls0),
					tcc_volume0_event,
					SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_REG
					| SND_SOC_DAPM_POST_REG | SND_SOC_DAPM_WILL_PMU | SND_SOC_DAPM_WILL_PMD),
	SND_SOC_DAPM_PGA_E(VOLUME1_WIDGET, SND_SOC_NOPM, 0, 0,
					tcc_volume_controls1, ARRAY_SIZE(tcc_volume_controls1),
					tcc_volume1_event,
					SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_REG
					| SND_SOC_DAPM_POST_REG | SND_SOC_DAPM_WILL_PMU | SND_SOC_DAPM_WILL_PMD),
	SND_SOC_DAPM_PGA_E(VOLUME2_WIDGET, SND_SOC_NOPM, 0, 0,
					tcc_volume_controls2, ARRAY_SIZE(tcc_volume_controls2),
					tcc_volume2_event,
					SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_REG
					| SND_SOC_DAPM_POST_REG | SND_SOC_DAPM_WILL_PMU | SND_SOC_DAPM_WILL_PMD),
	SND_SOC_DAPM_PGA_E(VOLUME3_WIDGET, SND_SOC_NOPM, 0, 0,
					tcc_volume_controls3, ARRAY_SIZE(tcc_volume_controls3),
					tcc_volume3_event,
					SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_REG
					| SND_SOC_DAPM_POST_REG | SND_SOC_DAPM_WILL_PMU | SND_SOC_DAPM_WILL_PMD),
	SND_SOC_DAPM_PGA_E(VOLUME4_WIDGET, SND_SOC_NOPM, 0, 0,
					tcc_volume_controls4, ARRAY_SIZE(tcc_volume_controls4),
					tcc_volume4_event,
					SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_REG
					| SND_SOC_DAPM_POST_REG | SND_SOC_DAPM_WILL_PMU | SND_SOC_DAPM_WILL_PMD),
	SND_SOC_DAPM_PGA_E(VOLUME5_WIDGET, SND_SOC_NOPM, 0, 0,
					tcc_volume_controls5, ARRAY_SIZE(tcc_volume_controls5),
					tcc_volume5_event,
					SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_REG
					| SND_SOC_DAPM_POST_REG | SND_SOC_DAPM_WILL_PMU | SND_SOC_DAPM_WILL_PMD),
	SND_SOC_DAPM_PGA_E(VOLUME6_WIDGET, SND_SOC_NOPM, 0, 0,
					tcc_volume_controls6, ARRAY_SIZE(tcc_volume_controls6),
					tcc_volume6_event,
					SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_REG
					| SND_SOC_DAPM_POST_REG | SND_SOC_DAPM_WILL_PMU | SND_SOC_DAPM_WILL_PMD),
	SND_SOC_DAPM_PGA_E(VOLUME7_WIDGET, SND_SOC_NOPM, 0, 0,
					tcc_volume_controls7, ARRAY_SIZE(tcc_volume_controls7),
					tcc_volume7_event,
					SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_REG
					| SND_SOC_DAPM_POST_REG | SND_SOC_DAPM_WILL_PMU | SND_SOC_DAPM_WILL_PMD),
	SND_SOC_DAPM_MIXER_E(MIXER0_WIDGET, SND_SOC_NOPM, 0, 0,
				tcc_mixer0_controls, ARRAY_SIZE(tcc_mixer0_controls),
				tcc_mixer0_event,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |  SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_REG
				| SND_SOC_DAPM_POST_REG | SND_SOC_DAPM_WILL_PMU | SND_SOC_DAPM_WILL_PMD),
	SND_SOC_DAPM_MIXER_E(MIXER1_WIDGET, SND_SOC_NOPM, 0, 0,
			tcc_mixer1_controls, ARRAY_SIZE(tcc_mixer1_controls),
			tcc_mixer1_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |  SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_REG
			| SND_SOC_DAPM_POST_REG | SND_SOC_DAPM_WILL_PMU | SND_SOC_DAPM_WILL_PMD),
	SND_SOC_DAPM_MIXER_E(MIXER2_WIDGET, SND_SOC_NOPM, 0, 0,
			tcc_mixer2_controls, ARRAY_SIZE(tcc_mixer2_controls),
			tcc_mixer2_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |  SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_REG
			| SND_SOC_DAPM_POST_REG | SND_SOC_DAPM_WILL_PMU | SND_SOC_DAPM_WILL_PMD),
	SND_SOC_DAPM_MIXER_E(MIXER3_WIDGET, SND_SOC_NOPM, 0, 0,
			tcc_mixer3_controls, ARRAY_SIZE(tcc_mixer3_controls),
			tcc_mixer3_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |  SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_REG
			| SND_SOC_DAPM_POST_REG | SND_SOC_DAPM_WILL_PMU | SND_SOC_DAPM_WILL_PMD),
	SND_SOC_DAPM_MIXER_E(MIXER4_WIDGET, SND_SOC_NOPM, 0, 0,
			tcc_mixer4_controls, ARRAY_SIZE(tcc_mixer4_controls),
			tcc_mixer4_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |  SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_REG
			| SND_SOC_DAPM_POST_REG | SND_SOC_DAPM_WILL_PMU | SND_SOC_DAPM_WILL_PMD),

#if 0
	SND_SOC_DAPM_DEMUX_E(REORDERSPLITTER0_WIDGET, SND_SOC_NOPM, 0, 0,
					tcc_mars0_controls, ARRAY_SIZE(tcc_mars0_controls),
					tcc_mars0_event,
					SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |  SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_PRE_REG
					| SND_SOC_DAPM_POST_REG | SND_SOC_DAPM_WILL_PMU | SND_SOC_DAPM_WILL_PMD),
#endif
};
#endif

static inline int get_device_num_from_control_name(const char *str)
{
	const char sep[] = " ";
	const char *str_dev;
	char *str_tmp;
	char *tmp;
	int dev_num = 0;

	int32_t s_size = (int32_t) sizeof(KCONTROL_HDR);
	int ret = 0;

	//if(s_size != 0u ) { //always not  0
	s_size--;
	//}

	str_tmp = kstrdup(str, GFP_KERNEL);
	tmp = str_tmp;
	str_dev = strsep(&tmp, sep);

	if (str_dev != NULL) {
		ret = kstrtoint(&str_dev[s_size], 10, &dev_num);

		if (ret < 0) {
			snd_card_err("amixer %s failed\n", __func__);
			ret = -EINVAL;
		} else {
			snd_card_dbg("amixer %s success dev_num=%d\n", __func__, dev_num);
			ret = dev_num;
		}
	} else {
		snd_card_err("amixer %s failed\n", __func__);
		ret = -EINVAL;
	}

	if(str_tmp != NULL){
		kfree(str_tmp);
	}

	return ret;
}

#if 0
static inline struct snd_soc_pcm_runtime *get_rtd_from_card(
	struct snd_soc_card *card,
	int device_num)
{
	struct snd_soc_pcm_runtime *rtd;

	list_for_each_entry(rtd, &card->rtd_list, list) {
		if (rtd->num == device_num)
			break;
	}

	return rtd;
}
#endif

static inline struct tcc_dai_info_t *tcc_snd_card_get_dai_info(
	const struct tcc_card_info_t *card_info,
	const struct snd_soc_dai *dai)
{
	struct tcc_dai_info_t *ret = NULL;
	int32_t i;

	for (i = 0; i < card_info->num_links; i++) {
		if(card_info->dai_link[i].cpus->dai_name != NULL){
			if(strcmp(card_info->dai_link[i].cpus->dai_name, dai->name) == 0){
				if(card_info->dai_link[i].cpus->of_node == dai->dev->of_node){
					ret = &card_info->dai_info[i];
				}
			}
		}else{
			if (card_info->dai_link[i].cpus->of_node == dai->dev->of_node) {
				ret = &card_info->dai_info[i];
			}
		}

		if(ret != NULL){
			break;
		}
	}
	return ret;
}

static int tcc_snd_card_startup(struct snd_pcm_substream *substream)
{
	const struct snd_soc_pcm_runtime *rtd =
		(const struct snd_soc_pcm_runtime *)substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0u);
	struct snd_soc_dai *codec_dai = asoc_rtd_to_codec(rtd, 0u);
	const struct tcc_card_info_t *card_info =
		(const struct tcc_card_info_t *)snd_soc_card_get_drvdata(rtd->card);
	struct tcc_dai_info_t *dai_info;
	int32_t ret = 0;

	dai_info = tcc_snd_card_get_dai_info(card_info, cpu_dai);
	if (dai_info == NULL) {
		snd_card_err("%s : fail to get dai information\n", __func__);
		ret = -EINVAL;
	} else {
		if (dai_info->is_updated) {
			(void)snd_soc_dai_set_tdm_slot(
					cpu_dai,
					0u,
					0u,
					dai_info->tdm_slots,
					dai_info->tdm_width);
			(void)snd_soc_dai_set_tdm_slot(
					codec_dai,
					0u,
					0u,
					dai_info->tdm_slots,
					dai_info->tdm_width);

			snd_card_dbg("%s - dai_fmt : 0x%08x\n", __func__,
					dai_info->dai_fmt);

			(void)snd_soc_dai_set_fmt(cpu_dai, dai_info->dai_fmt);
			(void)snd_soc_dai_set_fmt(codec_dai, dai_info->dai_fmt);

			dai_info->is_updated = FALSE;
		}
	}

	return ret;
}

static void tcc_snd_card_shutdown(struct snd_pcm_substream *substream)
{

	snd_card_dbg("%s - Not support operation", __func__);

}

static int tcc_snd_card_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	const struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = asoc_rtd_to_codec(rtd, 0u);
	struct tcc_card_info_t *card_info = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct tcc_dai_info_t *dai_info
		= tcc_snd_card_get_dai_info(card_info, cpu_dai);
	unsigned int mclk_fs = 0, bclk_fs = 0;
	int ret = 0;

	snd_card_dbg("%s: channels: %d, rate: %d, format: %d\n", __func__,
					params_channels(params),
					params_rate(params),
					params_format(params));
	if(dai_info == NULL){
		ret = -EINVAL;
	}

	if(ret == 0){
#if defined(CONFIG_SND_SOC_TELECHIPS_EXTENDED_TBD)
		// card_info->dev_masrc[0]->iface_ops->set_be_hw_params(card_info->dev_masrc[0], substream, params);
#endif

		//For AK4602
		if(strncmp("ak4602", codec_dai->name, strlen("ak4602")) == 0){
			if (dai_info->tdm_slots != 0) { // the case of TDM
				mclk_fs = ui_to_ui_mul(si_to_ui(dai_info->tdm_slots), si_to_ui(dai_info->tdm_width));
				bclk_fs = mclk_fs;
			} else {// Not TDM
				mclk_fs = ui_to_ui_mul(dai_info->mclk_div, dai_info->bclk_ratio);
				bclk_fs = dai_info->bclk_ratio;
			}

			if (bclk_fs != 0u) {
				unsigned int bclk = params_rate(params) * bclk_fs;
				ret = snd_soc_dai_set_sysclk(codec_dai,
						0,
						bclk,
						SND_SOC_CLOCK_IN);
			}
		}
	}

	return ret;
}

static int tcc_snd_card_hw_free(struct snd_pcm_substream *substream)
{
	int ret = 0;

	snd_card_dbg("%s - Not support operation", __func__);

	return ret;
}

static int tcc_snd_card_prepare(struct snd_pcm_substream *substream)
{
#if defined(CONFIG_SND_SOC_TELECHIPS_EXTENDED_TBD)
	// struct snd_soc_pcm_runtime *rtd = substream->private_data;
	// struct tcc_card_info_t *card_info = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_card_info_t *card_info = snd_soc_card_get_drvdata(rtd->card);
#endif
	int ret = 0;

#if defined(CONFIG_SND_SOC_TELECHIPS_EXTENDED_TBD)
	// card_info->dev_masrc[0]->iface_ops->set_fe_hw_params(card_info->dev_masrc[0], substream);
	// card_info->dev_masrc[0]->iface_ops->configure(card_info->dev_masrc[0]);
	card_info->substream = substream;
	card_info->fe_params.rate = substream->runtime->rate;
	card_info->fe_params.channels = substream->runtime->channels;
	card_info->fe_params.format = substream->runtime->format;

	snd_card_dbg("%s: [FE] rate: %d, channels: %d, format: %d\n", __func__,
		card_info->fe_params.rate,
		card_info->fe_params.channels,
		card_info->fe_params.format);

	snd_card_dbg("%s: [BE] rate: %d, channels: %d, format: %d\n", __func__,
		card_info->be_params.rate,
		card_info->be_params.channels,
		card_info->be_params.format);

	snd_card_dbg("%s: substream->name: %s\n", __func__, substream->name);
	// card_info->dev_mars[0]->iface_ops->configure(card_info->dev_mars[0]);
#endif
	return ret;
}

static int tcc_snd_card_trigger(struct snd_pcm_substream *substream, int cmd)
{
#if defined(CONFIG_SND_SOC_TELECHIPS_EXTENDED_TBD)
	// struct snd_soc_pcm_runtime *rtd = substream->private_data;
	// struct tcc_card_info_t *card_info = snd_soc_card_get_drvdata(rtd->card);
#endif
	int ret = 0;

	snd_card_dbg("%s - cmd: %d", __func__, cmd);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
#if defined(CONFIG_SND_SOC_TELECHIPS_EXTENDED_TBD)
		// card_info->dev_masrc[0]->iface_ops->start(card_info->dev_masrc[0]);
		// card_info->dev_mars[0]->iface_ops->start(card_info->dev_mars[0]);
		// card_info->dev_mars[0]->iface_ops->dump(card_info->dev_mars[0]);
#endif
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			snd_card_dbg("- playback start.\n");
		} else {
			snd_card_dbg("- capture start.\n");
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
#if defined(CONFIG_SND_SOC_TELECHIPS_EXTENDED_TBD)
		// card_info->dev_masrc[0]->iface_ops->stop(card_info->dev_masrc[0]);
		// card_info->dev_mars[0]->iface_ops->stop(card_info->dev_mars[0]);
#endif
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			snd_card_dbg("- playback stop.\n");
		} else {
			snd_card_dbg("- capture stop.\n");
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static struct snd_soc_ops tcc_snd_card_ops = {
	.startup = tcc_snd_card_startup,

	/*do not use below operations*/
	.shutdown = tcc_snd_card_shutdown,
	.hw_params = tcc_snd_card_hw_params,
	.hw_free = tcc_snd_card_hw_free,
	.prepare = tcc_snd_card_prepare,
	.trigger = tcc_snd_card_trigger,
};

static int get_tdm_width(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =
	    (struct snd_soc_card *)snd_kcontrol_chip(kcontrol);
	const struct tcc_card_info_t *card_info =
	    (struct tcc_card_info_t *)snd_soc_card_get_drvdata(card);
	int dev_num =
		get_device_num_from_control_name((char const *) kcontrol->id.name);
	const struct tcc_dai_info_t *dai_info = &card_info->dai_info[dev_num];
	int ret = 0;

	if (dev_num > card->num_rtd) {
		snd_card_dbg("%s - fail\n", __func__);
		ret = -EINVAL;
	} else {
		snd_card_dbg("%s - success\n", __func__);
		ucontrol->value.integer.value[0] = (dai_info->tdm_width == 16) ? 0 :
			(dai_info->tdm_width == 24) ? 1 : 2;
	}

	return ret;
}

static int set_tdm_width(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =
	    (struct snd_soc_card *)snd_kcontrol_chip(kcontrol);
	const struct tcc_card_info_t *card_info =
	    (struct tcc_card_info_t *)snd_soc_card_get_drvdata(card);
	int dev_num =
		get_device_num_from_control_name((char const *) kcontrol->id.name);
	struct tcc_dai_info_t *dai_info = &card_info->dai_info[dev_num];
	int ret = 0;
	bool active = FALSE;

	active = (snd_soc_dai_active(dai_info->dai) == 0)? FALSE : TRUE;

	if ((dev_num > card->num_rtd)
		||(active == TRUE)) {
		snd_card_dbg("%s - fail\n", __func__);
		ret = -EINVAL;
	} else {
		snd_card_dbg("%s - success\n", __func__);
		dai_info->tdm_width =
			(ucontrol->value.integer.value[0] == 0) ? 16 :
			(ucontrol->value.integer.value[0] == 1) ? 24 : 32;

		dai_info->is_updated = TRUE;
	}

	return ret;
}

static char const *tdm_width_texts[] = {
	"16bits",
	"24bits",
	"32bits",
};

static const struct soc_enum tdm_width_enum[] = {
	SOC_ENUM_SINGLE_EXT((TCC_AUDIO_ARRAY_SIZE(tdm_width_texts)),
			    (tdm_width_texts)),
};

static int get_tdm_slots(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =
	    (struct snd_soc_card *)snd_kcontrol_chip(kcontrol);
	const struct tcc_card_info_t *card_info =
	    (struct tcc_card_info_t *)snd_soc_card_get_drvdata(card);
	int dev_num =
		get_device_num_from_control_name((char const *) kcontrol->id.name);
	const struct tcc_dai_info_t *dai_info = &card_info->dai_info[dev_num];
	int ret = 0;

	if (dev_num > card->num_rtd) {
		snd_card_dbg("%s - fail\n", __func__);
		ret = -EINVAL;
	} else {
		snd_card_dbg("%s - success\n", __func__);
		ucontrol->value.integer.value[0] =
			(dai_info->tdm_slots == 0) ? 0 :
			(dai_info->tdm_slots == 2) ? 1 :
			(dai_info->tdm_slots == 4) ? 2 :
			(dai_info->tdm_slots == 8) ? 3 :
			(dai_info->tdm_slots == 16) ? 4 : 3;
	}
	return ret;
}

static int set_tdm_slots(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =
	    (struct snd_soc_card *)snd_kcontrol_chip(kcontrol);
	const struct tcc_card_info_t *card_info =
	    (struct tcc_card_info_t *)snd_soc_card_get_drvdata(card);
	int dev_num =
		get_device_num_from_control_name((char const *) kcontrol->id.name);
	struct tcc_dai_info_t *dai_info = &card_info->dai_info[dev_num];
	int ret = 0;
	bool active = FALSE;

	active = (snd_soc_dai_active(dai_info->dai) == 0)? FALSE : TRUE;

	if ((dev_num > card->num_rtd)
		||(active == TRUE)) {
		snd_card_dbg("%s - fail\n", __func__);
		ret = -EINVAL;
	} else {
		snd_card_dbg("%s - success\n", __func__);
		dai_info->tdm_slots =
			(ucontrol->value.integer.value[0] == 0) ? 0 :
			(ucontrol->value.integer.value[0] == 1) ? 2 :
			(ucontrol->value.integer.value[0] == 2) ? 4 :
			(ucontrol->value.integer.value[0] == 3) ? 8 :
			(ucontrol->value.integer.value[0] == 4) ? 16: 8;

		dai_info->is_updated = TRUE;
	}

	return ret;
}

static char const *tdm_slots_texts[] = {
	"Disable",
	"2slots",
	"4slots",
	"8slots",
	"16slots",
};

static const struct soc_enum tdm_slots_enum[] = {
	SOC_ENUM_SINGLE_EXT((TCC_AUDIO_ARRAY_SIZE(tdm_slots_texts)),
			    (tdm_slots_texts)),
};
#if defined(CONFIG_SND_SOC_TELECHIPS_EXTENDED_TBD)
#define MAX_NUM_FIFOCTL_PAIR	(4)
#define MAX_NUM_FIFOCTL_IP		(4)

const static char *mafc_cpu_dai_name[MAX_NUM_FIFOCTL_IP][MAX_NUM_FIFOCTL_PAIR] = {
	{
		TCC_FIFOCTRL0_PARI0,
		TCC_FIFOCTRL0_PARI1,
		TCC_FIFOCTRL0_PARI2,
		TCC_FIFOCTRL0_PARI3
	},
	{
		TCC_FIFOCTRL1_PARI0,
		TCC_FIFOCTRL1_PARI1,
		TCC_FIFOCTRL1_PARI2,
		TCC_FIFOCTRL1_PARI3
	},
	{
		TCC_FIFOCTRL2_PARI0,
		TCC_FIFOCTRL2_PARI1,
		TCC_FIFOCTRL2_PARI2,
		TCC_FIFOCTRL2_PARI3,
	},
	{
		TCC_FIFOCTRL3_PARI0,
		TCC_FIFOCTRL3_PARI1,
		TCC_FIFOCTRL3_PARI2,
		TCC_FIFOCTRL3_PARI3
	},
};
#endif
#define MAX_NUM_ADMA_DEV	(2)
#define MAX_NUM_ADMA_IP		(8)

const static char *adma_cpu_dai_name[MAX_NUM_ADMA_IP][MAX_NUM_ADMA_DEV] = {
	{ TCC_ADMA0_DAI, TCC_ADMA0_SPDIF },
	{ TCC_ADMA1_DAI, TCC_ADMA1_SPDIF },
	{ TCC_ADMA2_DAI, TCC_ADMA2_SPDIF },
	{ TCC_ADMA3_DAI, TCC_ADMA3_SPDIF },
	{ TCC_ADMA4_DAI, TCC_ADMA4_SPDIF },
	{ TCC_ADMA5_DAI, TCC_ADMA5_SPDIF },
	{ TCC_ADMA6_DAI, TCC_ADMA6_SPDIF },
	{ TCC_ADMA7_DAI, TCC_ADMA7_SPDIF }
};

static int get_dai_clkinv(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =
	    (struct snd_soc_card *)snd_kcontrol_chip(kcontrol);
	const struct tcc_card_info_t *card_info =
	    (struct tcc_card_info_t *)snd_soc_card_get_drvdata(card);
	int dev_num = get_device_num_from_control_name((char const *) kcontrol->id.name);
	uint32_t format;
	int ret = 0;

	if (dev_num > card->num_rtd) {
		snd_card_dbg("%s - fail\n", __func__);
		ret = -EINVAL;
	} else {
		int32_t sfmt = 0;
		snd_card_dbg("%s - success\n", __func__);
		format = card_info->dai_info[dev_num].dai_fmt &
			(uint32_t) SND_SOC_DAIFMT_INV_MASK;

		sfmt = (int32_t)format;

		ucontrol->value.integer.value[0] =
			(sfmt == SND_SOC_DAIFMT_NB_IF) ? 1 :
			(sfmt == SND_SOC_DAIFMT_IB_NF) ? 2 :
			(sfmt == SND_SOC_DAIFMT_IB_IF) ? 3 : 0;
	}

	return ret;
}

static int set_dai_clkinv(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =
	    (struct snd_soc_card *)snd_kcontrol_chip(kcontrol);
	const struct tcc_card_info_t *card_info =
	    (struct tcc_card_info_t *)snd_soc_card_get_drvdata(card);
	int dev_num = get_device_num_from_control_name((char const *) kcontrol->id.name);
	const struct tcc_dai_info_t *dai_info = &card_info->dai_info[dev_num];
	uint32_t format;
	int ret = 0;
	bool active = FALSE;

	active = (snd_soc_dai_active(dai_info->dai) == 0)? FALSE : TRUE;

	if ((dev_num > card->num_rtd)
		||(active == TRUE)) {
		snd_card_dbg("%s - fail\n", __func__);
		ret = -EINVAL;
	} else {
		int32_t sfmt = 0;
		snd_card_dbg("%s - success\n", __func__);

		sfmt = (ucontrol->value.integer.value[0] == 0) ?
			SND_SOC_DAIFMT_NB_NF :
			(ucontrol->value.integer.value[0] == 1) ?
			SND_SOC_DAIFMT_NB_IF :
			(ucontrol->value.integer.value[0] == 2) ?
			SND_SOC_DAIFMT_IB_NF :
			SND_SOC_DAIFMT_IB_IF;

		format = (uint32_t)sfmt;

		card_info->dai_info[dev_num].dai_fmt &=
			~(uint32_t)SND_SOC_DAIFMT_INV_MASK;
		card_info->dai_info[dev_num].dai_fmt |= format;

		card_info->dai_info[dev_num].is_updated = TRUE;
	}

	return ret;
}

static char const *dai_clkinv_texts[] = {
	"NB_NF",
	"NB_IF",
	"IB_NF",
	"IB_IF",
};

static const struct soc_enum dai_clkinv_enum[] = {
	SOC_ENUM_SINGLE_EXT((TCC_AUDIO_ARRAY_SIZE(dai_clkinv_texts)),
			    (dai_clkinv_texts)),
};

static int get_dai_format(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =
	    (struct snd_soc_card *)snd_kcontrol_chip(kcontrol);
	const struct tcc_card_info_t *card_info =
	    (struct tcc_card_info_t *)snd_soc_card_get_drvdata(card);
	int dev_num = get_device_num_from_control_name((char const *) kcontrol->id.name);
	uint32_t format;
	int ret = 0;

	if (dev_num > card->num_rtd) {
		snd_card_dbg("%s - fail\n", __func__);
		ret = -EINVAL;
	} else {
		snd_card_dbg("%s - success\n", __func__);
		format = card_info->dai_info[dev_num].dai_fmt &
			(uint32_t) SND_SOC_DAIFMT_FORMAT_MASK;

		ucontrol->value.integer.value[0] =
			(format == (uint32_t) SND_SOC_DAIFMT_DSP_B) ? 4 :
			(format == (uint32_t) SND_SOC_DAIFMT_DSP_A) ? 3 :
			(format == (uint32_t) SND_SOC_DAIFMT_RIGHT_J) ? 2 :
			(format == (uint32_t) SND_SOC_DAIFMT_LEFT_J) ? 1 :
			(format == (uint32_t) SND_SOC_DAIFMT_I2S) ? 0 : 0;
	}

	return ret;
}

static int set_dai_format(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =
	    (struct snd_soc_card *)snd_kcontrol_chip(kcontrol);
	const struct tcc_card_info_t *card_info =
	    (struct tcc_card_info_t *)snd_soc_card_get_drvdata(card);
	int dev_num = get_device_num_from_control_name((char const *) kcontrol->id.name);
	const struct tcc_dai_info_t *dai_info = &card_info->dai_info[dev_num];
	uint32_t format;
	int ret = 0;
	bool active = FALSE;

	active = (snd_soc_dai_active(dai_info->dai) == 0)? FALSE : TRUE;

	if ((dev_num > card->num_rtd)
		||(active == TRUE)) {
		snd_card_dbg("%s - fail\n", __func__);
		ret = -EINVAL;
	} else {
		snd_card_dbg("%s - success\n", __func__);
		format = (ucontrol->value.integer.value[0] == 4) ?
			(uint32_t) SND_SOC_DAIFMT_DSP_B :
			(ucontrol->value.integer.value[0] == 3) ?
			(uint32_t) SND_SOC_DAIFMT_DSP_A :
			(ucontrol->value.integer.value[0] == 2) ?
			(uint32_t) SND_SOC_DAIFMT_RIGHT_J :
			(ucontrol->value.integer.value[0] == 1) ?
			(uint32_t) SND_SOC_DAIFMT_LEFT_J :
			(uint32_t) SND_SOC_DAIFMT_I2S;

		card_info->dai_info[dev_num].dai_fmt &=
			~(uint32_t)SND_SOC_DAIFMT_FORMAT_MASK;
		card_info->dai_info[dev_num].dai_fmt |= format;

		card_info->dai_info[dev_num].is_updated = TRUE;
	}

	return ret;
}

static char const *dai_format_texts[] = {
	"I2S",
	"LEFT_J",
	"RIGHT_J",
	"DSP_A",		// TDM Only
	"DSP_B",		// TDM Only
};

static const struct soc_enum dai_format_enum[] = {
	SOC_ENUM_SINGLE_EXT((TCC_AUDIO_ARRAY_SIZE(dai_format_texts)),
			    (dai_format_texts)),
};

static int get_continuous_clk_mode(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =
	    (struct snd_soc_card *)snd_kcontrol_chip(kcontrol);
	const struct tcc_card_info_t *card_info =
	    (struct tcc_card_info_t *)snd_soc_card_get_drvdata(card);
	int dev_num = get_device_num_from_control_name((char const *) kcontrol->id.name);
	uint32_t format;
	int ret = 0;

	if (dev_num > card->num_rtd) {
		snd_card_dbg("%s - fail\n", __func__);
		ret = -EINVAL;
	} else {
		snd_card_dbg("%s - success\n", __func__);
		format = card_info->dai_info[dev_num].dai_fmt &
			(uint32_t) SND_SOC_DAIFMT_CLOCK_MASK;

		ucontrol->value.integer.value[0] =
			((int32_t) format == SND_SOC_DAIFMT_CONT) ? 1 : 0;
	}

	return ret;
}

static int set_continous_clk_mode(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card =
	    (struct snd_soc_card *)snd_kcontrol_chip(kcontrol);
	const struct tcc_card_info_t *card_info =
	    (struct tcc_card_info_t *)snd_soc_card_get_drvdata(card);
	int dev_num = get_device_num_from_control_name((char const *) kcontrol->id.name);
	const struct tcc_dai_info_t *dai_info = &card_info->dai_info[dev_num];
	uint32_t format;
	int ret = 0;
	bool active = FALSE;

	active = (snd_soc_dai_active(dai_info->dai) == 0)? FALSE : TRUE;

	if ((dev_num > card->num_rtd)
		||(active == TRUE)) {
		snd_card_dbg("%s - fail\n", __func__);
		ret = -EINVAL;
	} else {
		int32_t sformat = 0;
		snd_card_dbg("%s - success\n", __func__);
		sformat =
			(ucontrol->value.integer.value[0] == 1) ?
			SND_SOC_DAIFMT_CONT :
			SND_SOC_DAIFMT_GATED;

		format = (uint32_t) sformat;

		card_info->dai_info[dev_num].dai_fmt &=
			~(uint32_t)SND_SOC_DAIFMT_CLOCK_MASK;

		card_info->dai_info[dev_num].dai_fmt |= format;

		card_info->dai_info[dev_num].is_updated = TRUE;
	}

	return ret;
}

static const struct snd_kcontrol_new tcc_snd_i2s_controls[] = {
	SOC_ENUM_EXT(("DAI FORMAT"), (dai_format_enum[0]), (get_dai_format),
		     (set_dai_format)),
	SOC_ENUM_EXT(("DAI CLKINV"), (dai_clkinv_enum[0]), (get_dai_clkinv),
		     (set_dai_clkinv)),
	SOC_ENUM_EXT(("TDM Slots"), (tdm_slots_enum[0]), (get_tdm_slots),
		     (set_tdm_slots)),
	SOC_ENUM_EXT(("TDM Slot Width"), (tdm_width_enum[0]), (get_tdm_width),
		     (set_tdm_width)),
	SOC_SINGLE_BOOL_EXT(("Clock Continuous Mode"), (0),
			    (get_continuous_clk_mode),
			    (set_continous_clk_mode)),
};

static int tcc_snd_card_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct snd_soc_dai *codec_dai = asoc_rtd_to_codec(rtd, 0u);
	const struct tcc_card_info_t *card_info =
			snd_soc_card_get_drvdata(rtd->card);
	struct tcc_dai_info_t *dai_info;
	int ret = 0;

	dai_info = tcc_snd_card_get_dai_info(card_info, cpu_dai);
	if (dai_info == NULL) {
		snd_card_dbg("%s - fail\n", __func__);
		ret = -EINVAL;
	} else {
		if ((dai_info->tdm_slots != 0) && (dai_info->tdm_width != 0)) {
			(void)snd_soc_dai_set_tdm_slot(cpu_dai, 0, 0,
					dai_info->tdm_slots, dai_info->tdm_width);
			(void)snd_soc_dai_set_tdm_slot(codec_dai, 0, 0,
					dai_info->tdm_slots, dai_info->tdm_width);
		}

		if (dai_info->mclk_div != 0U) {
			snd_card_dbg("%s - mclk_div: %d\n", __func__, dai_info->mclk_div);
			(void)snd_soc_dai_set_clkdiv(cpu_dai,
					TCC_DAI_CLKDIV_ID_MCLK_TO_BCLK,
					ui_to_si(dai_info->mclk_div));
		}

		if (dai_info->bclk_ratio != 0u) {
			snd_card_dbg("%s - bclk_ratio: %d\n", __func__, dai_info->bclk_ratio);
			(void)snd_soc_dai_set_bclk_ratio(cpu_dai, dai_info->bclk_ratio);
		}
		dai_info->dai = cpu_dai;
	}
	return ret;
}

static int tcc_snd_card_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	int ret = 0;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct tcc_card_info_t *card_info =
		(struct tcc_card_info_t *)snd_soc_card_get_drvdata(rtd->card);
	const struct tcc_dai_info_t *dai_info;

	struct snd_interval *rate = hw_param_interval(params,
	          SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
	                                  SNDRV_PCM_HW_PARAM_CHANNELS);
	snd_card_dbg("%s\n", __func__);
	snd_card_dbg("%s: enter - channels: %d, rate: %d, format: %d\n", __func__,
					params_channels(params),
					params_rate(params),
					params_format(params));
	dai_info = tcc_snd_card_get_dai_info(card_info, cpu_dai);
	if (dai_info == NULL) {
		snd_card_err("%s : fail to get dai information\n", __func__);
		ret = -EINVAL;
	} else {
		/* The DSP will covert the FE rate to 48k, stereo */
		rate->min = dai_info->fixup_samplerate;
		rate->max = dai_info->fixup_samplerate;
		channels->min = dai_info->fixup_channels;
		channels->max = dai_info->fixup_channels;
		/* set DAI0 to 16 bit */
		snd_mask_set(&params->masks[SNDRV_PCM_HW_PARAM_FORMAT -
							  SNDRV_PCM_HW_PARAM_FIRST_MASK],
							  dai_info->fixup_format);
	}

	card_info->be_params.rate = params_rate(params);
	card_info->be_params.channels = params_channels(params);
	card_info->be_params.format = params_format(params);

	snd_card_dbg("%s: [BE] rate: %d, channels: %d, format: %d\n", __func__,
		card_info->be_params.rate,
		card_info->be_params.channels,
		card_info->be_params.format);
	snd_card_dbg("%s: exit - channels: %d, rate: %d, format: %d\n", __func__,
					params_channels(params),
					params_rate(params),
					params_format(params));

	return ret;

}
static int tcc_snd_card_sub_dai_link(
	struct device_node *dnode,
	struct snd_soc_dai_link *dai_link,
	struct tcc_dai_info_t *dai_info)
{
	struct device_node *platform_of_node;
	struct device_node *dai_of_node;
	struct device_node *codec_of_node;
	const void *prop;
	bool enable = FALSE;
	int ret = 0;
	int pcm_mafc_id = -ENODEV, pcm_adma_id = -ENODEV;

	if ((dnode == NULL) || (dai_link == NULL) || (dai_info == NULL)) {
		snd_card_dbg("%s - fail\n", __func__);
		ret = -EINVAL;
	} else {
		platform_of_node = of_parse_phandle(dnode, "pcm", 0);
		dai_of_node = of_parse_phandle(dnode, "dai", 0);
		codec_of_node = of_parse_phandle(dnode, "codec", 0);

		(void)of_property_read_string(
				dnode,
				"stream-name",
				&dai_link->name);
		(void)of_property_read_string(
				dnode,
				"stream-name",
				&dai_link->stream_name);
		(void)of_property_read_string(
				dnode,
				"codec,dai-name",
				&dai_link->codecs->dai_name);

		if(of_property_read_s32(dnode, "pcm,id", &dai_link->id) != 0){
			dai_link->id = 0;
			snd_card_dbg("\t\tstream_name : %s\n", dai_link->stream_name);
		}else{
			pcm_mafc_id = of_alias_get_id(platform_of_node, "fifo_controller");
			pcm_adma_id = of_alias_get_id(platform_of_node, "adma");
			snd_card_dbg("\t\tstream_name : %s, mafc dev id=%d, dai_link id=%d\n", dai_link->stream_name, pcm_mafc_id, dai_link->id);
		}

		if(dai_of_node == NULL){
			if(platform_of_node == NULL){
				dai_link->cpus->name = TCC_SND_SOC_DUMMY_NAME;
				dai_link->cpus->dai_name = TCC_SND_SOC_DUMMY_DAI_NAME;
			}
		} else {
			dai_link->cpus->of_node = dai_of_node;
			snd_card_dbg("\t\tcpus->of_node->name: %s\n", dai_link->cpus->of_node->name);
		}

		if(platform_of_node == NULL){
			dai_link->platforms->name = "snd-soc-dummy";
		} else {
#if defined(CONFIG_SND_SOC_TELECHIPS_EXTENDED_TBD)
			if((pcm_mafc_id < MAX_NUM_FIFOCTL_IP) && (pcm_mafc_id >= 0)){
				dai_link->cpus->dai_name = mafc_cpu_dai_name[pcm_mafc_id][dai_link->id];
			}else if((pcm_adma_id < MAX_NUM_ADMA_IP) && (pcm_adma_id >= 0)){
				dai_link->cpus->dai_name = adma_cpu_dai_name[pcm_adma_id][dai_link->id];
			}
#else
			if((pcm_adma_id < MAX_NUM_ADMA_IP) && (pcm_adma_id >= 0)){
				dai_link->cpus->dai_name = adma_cpu_dai_name[pcm_adma_id][dai_link->id];
			}
#endif

			dai_link->cpus->of_node = platform_of_node;
			dai_link->platforms->of_node = platform_of_node;
			dai_link->dynamic = 1;
			snd_card_dbg("\t\tcpus->name: %s\n", dai_link->cpus->of_node->name);
			snd_card_dbg("\t\tcpus->dai_name: %s\n", dai_link->cpus->dai_name);
			snd_card_dbg("\t\tplatforms->name: %s\n", dai_link->platforms->of_node->name);
			snd_card_dbg("\t\tdai_link->dynamic: %d\n", dai_link->dynamic);
		}

		if(codec_of_node == NULL){
			dai_link->codecs->of_node = NULL;
			dai_link->codecs->name = TCC_SND_SOC_DUMMY_NAME;
			dai_link->codecs->dai_name = TCC_SND_SOC_DUMMY_DAI_NAME;
			snd_card_dbg("\t\tcodecs->name: %s\n", dai_link->codecs->name);

			if(platform_of_node == NULL){
				dai_link->no_pcm = 1;
				snd_card_dbg("\t\tdai_link->no_pcm: %d\n", dai_link->no_pcm);

				if(of_property_read_bool(dnode, "fixupdai") == TRUE){
					dai_link->be_hw_params_fixup = tcc_snd_card_be_hw_params_fixup;
					(void)of_property_read_u32(dnode, "fixupdai,channels", &dai_info->fixup_channels);
					snd_card_dbg("\t\tfixup_channels : %d\n", dai_info->fixup_channels);
					(void)of_property_read_u32(dnode, "fixupdai,format", &dai_info->fixup_format);
					snd_card_dbg("\t\tfixup_format : %d\n", dai_info->fixup_format);
					(void)of_property_read_u32(dnode, "fixupdai,samplerate", &dai_info->fixup_samplerate);
					snd_card_dbg("\t\tfixup_samplerate : %d\n", dai_info->fixup_samplerate);
					snd_card_dbg("\t\tbe_hw_params_fixup: yes\n");
				}else{
					snd_card_dbg("\t\tbe_hw_params_fixup: no\n");
				}
			}
		} else {
			dai_link->codecs->of_node = codec_of_node;
			snd_card_dbg("\t\tcodecs->of_node->name: %s\n", dai_link->codecs->of_node->name);

			dai_link->no_pcm = 1;
			snd_card_dbg("\t\tdai_link->no_pcm: %d\n", dai_link->no_pcm);

			if(of_property_read_bool(dnode, "fixupdai") == TRUE){
				dai_link->be_hw_params_fixup = tcc_snd_card_be_hw_params_fixup;
				(void)of_property_read_u32(dnode, "fixupdai,channels", &dai_info->fixup_channels);
				snd_card_dbg("\t\tfixup_channels : %d\n", dai_info->fixup_channels);
				(void)of_property_read_u32(dnode, "fixupdai,format", &dai_info->fixup_format);
				snd_card_dbg("\t\tfixup_format : %d\n", dai_info->fixup_format);
				(void)of_property_read_u32(dnode, "fixupdai,samplerate", &dai_info->fixup_samplerate);
				snd_card_dbg("\t\tfixup_samplerate : %d\n", dai_info->fixup_samplerate);
				snd_card_dbg("\t\tbe_hw_params_fixup: yes\n");
			}else{
					snd_card_dbg("\t\tbe_hw_params_fixup: no\n");
			}
		}


		dai_link->ops = &tcc_snd_card_ops;
		dai_link->init = tcc_snd_card_dai_init;

		enable = of_property_read_bool(dnode, "playback-only");
		if (enable == TRUE) {
			snd_card_dbg("\t\tDAI link playback_only!\n");
			dai_link->playback_only = 1u;
		}

		enable = of_property_read_bool(dnode, "capture-only");
		if (enable == TRUE) {
			snd_card_dbg("\t\tDAI link capture only!\n");
			dai_link->capture_only = 1u;
		}

		if ((dai_link->playback_only != 0u)
			&& (dai_link->capture_only != 0u)) {
			snd_card_err("no enabled DAI link");
			(void)pr_err("This will activate both.");
			dai_link->playback_only = 0u;
			dai_link->capture_only = 0u;
		}

		dai_link->dai_fmt = snd_soc_of_parse_daifmt(dnode, "codec,", NULL, NULL);
		dai_info->dai_fmt = dai_link->dai_fmt;
		snd_card_dbg("\t\tdai_fmt : 0x%08x\n", dai_link->dai_fmt);

		// parse configrations
		(void)of_property_read_u32(dnode, "mclk_div", &dai_info->mclk_div);
		snd_card_dbg("\t\tmclk_div : %d\n", dai_info->mclk_div);

		(void)of_property_read_u32(dnode, "bclk_ratio", &dai_info->bclk_ratio);
		snd_card_dbg("\t\tbclk_ratio: %d\n", dai_info->bclk_ratio);

		(void)of_property_read_s32(
				dnode,
				"dai-tdm-slot-num",
				&dai_info->tdm_slots);
		snd_card_dbg("\t\tdai-tdm-slot-num : %d\n", dai_info->tdm_slots);

		(void)of_property_read_s32(
				dnode,
				"dai-tdm-slot-width",
				&dai_info->tdm_width);
		snd_card_dbg("\t\tdai-tdm-slot-width : %d\n", dai_info->tdm_width);

		prop = of_get_property(dnode, "dpcm_playback", NULL);
		dai_link->dpcm_playback = (prop != NULL)? 1u : 0u;
		snd_card_dbg("\t\tdpcm-playback : %d\n", dai_link->dpcm_playback);

		prop = of_get_property(dnode, "dpcm_capture", NULL);
		dai_link->dpcm_capture = (prop != NULL)? 1u : 0u;
		snd_card_dbg("\t\tdpcm-capture : %d\n", dai_link->dpcm_capture);

		dai_info->is_updated = FALSE;
	}

	return ret;
}

static int tcc_of_parse_audio_routing(struct snd_soc_card *card, const char *propname)
{
	const struct device_node *np = card->dev->of_node;
	int num_routes;
	struct snd_soc_dapm_route *routes;
	char *blk_no_char;
	int i, blk_no, ret = 0;
	unsigned int base = 10;
	int mixer_source_cnt[MAX_NUM_MIX] = { 0 };

	blk_no_char = kzalloc(1, GFP_KERNEL);
	if(blk_no_char == NULL){
		ret = -ENOMEM;
	}

	if(propname == NULL){
		ret = -EINVAL;
	}

	if(ret == 0){
		do {
			num_routes = of_property_count_strings(np, propname);
			if ((num_routes < 0) || ((si_to_ui(num_routes) & 1u) != 0u)) {
				snd_card_err(
					"%s : Property '%s' does not exist or its length is not even\n",
					__func__, propname);
				ret = -EINVAL;
				continue;
			}
			num_routes /= 2;
			if (!num_routes) {
				snd_card_err("%s : Property '%s's length is zero\n",
					__func__, propname);
				ret = -EINVAL;
				continue;
			}

			routes = devm_kcalloc(card->dev, si_to_ul(num_routes), sizeof(*routes),
					      GFP_KERNEL);
			if (routes == NULL) {
				snd_card_err("%s : Could not allocate DAPM route table\n", __func__);
				ret = -ENOMEM;
				continue;
			}

			i = 0;
			do{
        			ret = of_property_read_string_index(np, propname,
        				2 * i, &routes[i].sink);
        			if (ret != 0) {
        				snd_card_err("%s : Property '%s' index %d could not be read: %d\n",
        					__func__, propname, 2 * i, ret);
        				ret = -EINVAL;
        				continue;
        			}
        			ret = of_property_read_string_index(np, propname,
        				(2 * i) + 1, &routes[i].source);
        			if (ret != 0) {
        				snd_card_err("%s : Property '%s' index %d could not be read: %d\n",
        					__func__, propname, (2 * i) + 1, ret);
        				ret = -EINVAL;
        				continue;
        			}

        			if(strncmp(routes[i].sink, "tcc-mixer", 9) == 0){
        				(void)strncpy(blk_no_char, &(routes[i].sink[9]), 1);

        				ret = kstrtoint(blk_no_char, base, &blk_no);
        				if (ret < 0) {
        					snd_card_err("%s : fail to kstrtoint\n", __func__);
        					ret = -EINVAL;
        				}else{
							if((blk_no < 0) || (blk_no >= MAX_NUM_MIX)){
								ret = -EINVAL;
							}else{
								if (mixer_source_cnt[blk_no] == 0)  {
									routes[i].control = kstrdup("First Source", GFP_KERNEL);
									mixer_source_cnt[blk_no]++;
								} else if (mixer_source_cnt[blk_no] == 1) {
									routes[i].control = kstrdup("Second Source", GFP_KERNEL);
									mixer_source_cnt[blk_no]++;
								} else {
									snd_card_err("%s : tcc mixer only support 2 sources\n", __func__);
									ret = -ENOTSUPP;
								}
							}
						}

						if(ret < 0){
							break;
						}
        			}
        			snd_card_dbg("%s : sink(%s) source(%s) control(%s)\n", __func__, routes[i].sink, routes[i].source, routes[i].control);
        		}while(++i < num_routes);


				if(ret < 0){
					continue;
				}
        		card->num_of_dapm_routes = num_routes;
        		card->of_dapm_routes = routes;
        	} while (false);
	}

	if(blk_no_char != NULL){
		kfree(blk_no_char);
	}

	return ret;
}

static inline int tcc_alloc_dai_link(const struct tcc_card_info_t *card_info, unsigned int cnt)
{
	int ret = 0;
	ssize_t alloc_size =
	        (ssize_t) sizeof(struct snd_soc_dai_link_component);
	card_info->dai_link[cnt].cpus =
	        kzalloc((size_t) alloc_size, GFP_KERNEL);
	card_info->dai_link[cnt].platforms =
	        kzalloc((size_t) alloc_size, GFP_KERNEL);
	card_info->dai_link[cnt].codecs =
	        kzalloc((size_t) alloc_size, GFP_KERNEL);
	if ((card_info->dai_link[cnt].cpus == NULL)
	                || (card_info->dai_link[cnt].platforms == NULL)
	                || (card_info->dai_link[cnt].codecs == NULL)) {
	        ret = -ENOMEM;
	}
	return ret;
}

static inline void tcc_free_dai_link(const struct tcc_card_info_t *card_info, unsigned int cnt)
{
	if(card_info->dai_link[cnt].cpus != NULL){
	        kfree(card_info->dai_link[cnt].cpus);
	}
	if(card_info->dai_link[cnt].platforms != NULL){
	        kfree(card_info->dai_link[cnt].platforms);
	}
	if(card_info->dai_link[cnt].codecs != NULL){
	        kfree(card_info->dai_link[cnt].codecs);
	}
}

static inline int tcc_parse_widget_dev_info(struct device_node *of_node, struct tcc_card_info_t *card_info)
{
	#define IDX_MASRC	(0u)
	#define IDX_MIXER	(1u)
	#define IDX_MARS	(2u)
	#define IDX_VOL		(3u)
	int blk, ret = 0;
	const char *const widget_name[] = {"masrc", "mixer", "mars", "volume"};
	const int max_blk[] = {MAX_NUM_MASRC, MAX_NUM_MIX, MAX_NUM_MARS, MAX_NUM_VOL};
	unsigned int idx = 0;

	for (idx = 0; idx < TCC_AUDIO_ARRAY_SIZE(max_blk); idx++) {
		for_each_node_by_name(of_node, widget_name[idx]){
			const struct platform_device *pdev = of_find_device_by_node(of_node);
			if(pdev == NULL){
			        continue;
			}
			snd_card_dbg("%s pdev(%s)\n", widget_name[idx], pdev->name);

			blk = of_alias_get_id(of_node, widget_name[idx]);
			snd_card_dbg("%s blk(%d)\n", widget_name[idx], blk);

			if((blk >= 0) && (blk < max_blk[idx])){
				if(idx == IDX_MASRC){
			        	card_info->dev_masrc[blk] = platform_get_drvdata(pdev);
				}else if(idx == IDX_MIXER){
			        	card_info->dev_mix[blk] = platform_get_drvdata(pdev);
				}else if(idx == IDX_MARS){
			        	card_info->dev_mars[blk] = platform_get_drvdata(pdev);
				}else if(idx == IDX_VOL){
			        	card_info->dev_vol[blk] = platform_get_drvdata(pdev);
				}else{
			        	ret = -EINVAL;
				        continue;
				}
			}else{
			        ret = -EINVAL;
			        continue;
			}
		}
	}
	return ret;
}

static int parse_tcc_snd_card_dt(const struct platform_device *pdev,
				 struct snd_soc_card *card)
{
	const struct device_node *dnode = pdev->dev.of_node;
	struct tcc_card_info_t *card_info = NULL;
	struct device_node *tcc_of_node = NULL;
	int i;
	const struct device_node *dai_link;
	ssize_t alloc_size;
	int ret = 0;

	do {
		card_info = kzalloc(sizeof(struct tcc_card_info_t), GFP_KERNEL);
		if (card_info == NULL) {
			ret = -ENOMEM;
			continue;
		}

		card_info->num_links = of_get_child_count(dnode);
		if ((card_info->num_links <= 0) || (card_info->num_links > DAI_LINK_MAX)) {
			snd_card_dbg("num_links[%d] is invalid.\n", card_info->num_links);
			ret = -EINVAL;
			continue;
		}

		snd_card_dbg("num_links : %d\n", card_info->num_links);

		alloc_size =
			(ssize_t) sizeof(struct snd_soc_dai_link) * card_info->num_links;
		card_info->dai_link =
			(struct snd_soc_dai_link *)kzalloc((size_t) alloc_size, GFP_KERNEL);
		if (card_info->dai_link == NULL){
			ret = -ENOMEM;
			continue;
		} else {
			(void) memset(card_info->dai_link, 0, (size_t)alloc_size);
		}

		alloc_size =
			(ssize_t) sizeof(struct tcc_dai_info_t) * card_info->num_links;
		card_info->dai_info = kzalloc((size_t) alloc_size, GFP_KERNEL);
		if(card_info->dai_info == NULL){
			ret = -ENOMEM;
			continue;
		} else {
			(void) memset(card_info->dai_info, 0, (size_t)alloc_size);
		}

		alloc_size =
			(ssize_t) sizeof(struct snd_soc_codec_conf) * card_info->num_links;
		card_info->codec_conf = kzalloc((size_t) alloc_size, GFP_KERNEL);
		if(card_info->codec_conf == NULL){
			ret = -ENOMEM;
			continue;
		} else {
			(void) memset(card_info->codec_conf, 0, (size_t)alloc_size);
		}

		dai_link = of_get_child_by_name(dnode, "telechips,dai-link");
		if (dai_link != NULL) {
			struct device_node *np;
			unsigned int cnt = 0;
			const char *overlay_dt_prop = of_get_property(dnode, "overlay_dt", NULL);

			if (overlay_dt_prop) {
				cnt = card_info->num_links - 1;
			}

			for_each_child_of_node((dnode), (np)) {
				snd_card_dbg("\tlink %d:\n", cnt);
				if (overlay_dt_prop) {
					ret = tcc_alloc_dai_link(card_info, cnt);
					card_info->dai_link[cnt].num_cpus = 1;
					card_info->dai_link[cnt].num_platforms = 1;
					card_info->dai_link[cnt].num_codecs = 1;

					(void)tcc_snd_card_sub_dai_link(np,
							&card_info->dai_link[cnt],
							&card_info->dai_info[cnt]);
					if(cnt == 0u){
						break;
					}
					cnt--;
				}else{
					if (cnt < si_to_ui(card_info->num_links)){
						ret = tcc_alloc_dai_link(card_info, cnt);
						if(ret < 0){
							break;
						}

						card_info->dai_link[cnt].num_cpus = 1;
						card_info->dai_link[cnt].num_platforms = 1;
						card_info->dai_link[cnt].num_codecs = 1;

						(void)tcc_snd_card_sub_dai_link(np,
								&card_info->dai_link[cnt],
								&card_info->dai_info[cnt]);
						cnt++;
					} else {
						break;
					}
				}
			}

			if(ret < 0){
				//free
				for_each_child_of_node((dnode), (np)) {
					snd_card_dbg("\tlink %d:\n", cnt);
					if (cnt < si_to_ui(card_info->num_links)){
						tcc_free_dai_link(card_info, cnt);
						cnt++;
					} else {
						break;
					}
				}

				continue;
			}
		}


		card->num_links = card_info->num_links;
		card->dai_link = card_info->dai_link;

		for (i = 0; i < card_info->num_links; i++) {
			{
				card_info->codec_conf[i].dlc.of_node = card_info->dai_link[i].cpus->of_node;
				snd_card_dbg("name_prefix(%d) : %s\n", i, card_info->codec_conf[i].name_prefix);
			}
		}

		ret = tcc_of_parse_audio_routing(card, "routing");
		if (ret != 0) {
			snd_card_err("%s: failed to parse audio-routing: %d\n", __func__, ret);
			ret = -EINVAL;
			continue;
		} else {
			snd_card_dbg("%s : register %d routes\n", __func__, card->num_of_dapm_routes);
		}

		card->codec_conf = card_info->codec_conf;
		card->num_configs = card_info->num_links;

		ret = tcc_parse_widget_dev_info(tcc_of_node, card_info);

		snd_soc_card_set_drvdata(card, card_info);
	} while (false);

	if(ret < 0){
		if(card_info != NULL){
			if(card_info->dai_info != NULL){
				kfree(card_info->dai_info);
			}
			if(card_info->dai_link != NULL){
				kfree(card_info->dai_link);
			}
			kfree(card_info);
		}
	}

	return ret;
}


static void free_snd_kcontrol_new(const struct snd_kcontrol_new *controls, int cnt)
{
	int i;

	for (i = 0; i < cnt; i++) {
		if((controls != NULL) &&
			(controls[i].name != NULL)) {
			kfree(controls[i].name);
		}
	}

	if(controls != NULL) {
		kfree(controls);
	}

}

static int tcc_snd_card_kcontrol_init(struct snd_soc_card *card)
{
	const struct tcc_card_info_t *card_info = snd_soc_card_get_drvdata(card);
	int32_t num_controls = 0, num_links_i2s = 0;
	struct snd_kcontrol_new *controls;
	int not_failed_name_count = 0, offset_controls = 0;
	int i, j;
	ssize_t alloc_size;
	int ret = 0;

	for (i = 0; i < card_info->num_links; i++) {
		if ((strcmp(
				card_info->dai_link[i].cpus->of_node->name,
				"i2s") == 0)
				&& (num_links_i2s < INT_MAX)) {
			num_links_i2s++;
		}
	}

	num_controls =
		(int32_t) TCC_AUDIO_ARRAY_SIZE(tcc_snd_i2s_controls) *
		(int32_t) num_links_i2s;

	if (num_controls > 0) {
		alloc_size = (ssize_t) sizeof(struct snd_kcontrol_new)
						* num_controls;
		controls = (struct snd_kcontrol_new *) kzalloc((size_t) alloc_size, GFP_KERNEL);
		if (controls == NULL) {
			snd_card_err("amixer controls allocation failed\n");
			ret = -ENOMEM;
		} else {
			for (i = 0; i < card_info->num_links; i++) {
				if ((strcmp(card_info->dai_link[i].cpus->of_node->name, "i2s")) == 0) {
					alloc_size = ((ssize_t) sizeof(struct snd_kcontrol_new))
						*((ssize_t) TCC_AUDIO_ARRAY_SIZE(
									tcc_snd_i2s_controls));
					(void) memcpy(
							&controls[offset_controls],
							tcc_snd_i2s_controls,
							(size_t) alloc_size);

					for (j = 0;
						j < (int32_t)TCC_AUDIO_ARRAY_SIZE(tcc_snd_i2s_controls);
						j++) {
						char tmp_name[255];
						if (controls[offset_controls+j].name == NULL) {
							snd_card_err(
									"name allocation failed : %d\n",
									i);
							not_failed_name_count =
								offset_controls + j;
							ret = -ENOMEM;
							break;
						} else {
							(void) scnprintf(tmp_name,
									(sizeof(tmp_name) - 1u),
									KCONTROL_HDR"%d %s",
									i,
									controls[offset_controls+j].name);
							controls[offset_controls+j].name =
								kstrdup(tmp_name, GFP_KERNEL);
						}
					}

					if(ret != 0) {
						break;
					} else {
						uint32_t array_sz = TCC_AUDIO_ARRAY_SIZE(
								tcc_snd_i2s_controls);
						offset_controls += (int)array_sz;
					}
				}
			}
		}

		if((ret < 0) && (controls != NULL)) {
			(void)free_snd_kcontrol_new(controls, not_failed_name_count);
		} else {
			card->controls = controls;
		}
	} else {
		snd_card_dbg("There is no amixer controls\n");
	}
	card->num_controls = num_controls;

	return ret;
}

static int tcc_snd_card_late_probe(struct snd_soc_card *card)
{
	const struct tcc_card_info_t *card_info = snd_soc_card_get_drvdata(card);
	const struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_dai *cpu_dai = NULL;
	const struct tcc_dai_info_t *dai_info = NULL;
	unsigned int mclk = 0;
	int32_t ret = 0;
	int32_t i;

	snd_card_dbg("%s\n", __func__);

	i = 0;
	do {
		rtd = snd_soc_get_pcm_runtime(card, &card->dai_link[i]);
		cpu_dai = asoc_rtd_to_cpu(rtd, 0);
		dai_info = tcc_snd_card_get_dai_info(card_info, cpu_dai);

		if(card->dai_link[i].dynamic == 1u){
			i++;
			continue;
		}

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
			snd_card_err("%s : fail to get dai information in dai-link %d.\n", __func__, i);
		}

		i++;
	} while ((dai_info != NULL) && (i < card->num_rtd));

	return ret;
}

#if defined(TCC_SND_CARD_DEBUG)//for debug
static struct snd_soc_dai *tcc_get_be_with_sink_path(const struct snd_soc_dapm_widget *tcc_dapm_widget)
{
	const struct snd_soc_dapm_path *tcc_dapm_path = NULL;
	struct snd_soc_dai *be;
	struct snd_soc_dai *ret = NULL;


	snd_soc_dapm_widget_for_each_sink_path(tcc_dapm_widget, tcc_dapm_path) {
		if (tcc_dapm_path->connect == 0u){
			continue;
		}

		snd_card_dbg("source id = %d, name=%s, sname=%s", tcc_dapm_path->source->id,
			tcc_dapm_path->source->name, tcc_dapm_path->source->sname);
		snd_card_dbg("sink id = %d, name=%s, sname=%s", tcc_dapm_path->sink->id,
			tcc_dapm_path->sink->name, tcc_dapm_path->sink->sname);

		if (tcc_dapm_path->sink->id == snd_soc_dapm_dai_in){
			ret = (struct snd_soc_dai *)tcc_dapm_path->sink->priv;
		}

		if(ret == NULL){
			be = tcc_get_be_with_sink_path(tcc_dapm_path->sink);
			if (be != NULL){
				ret = be;
			}
		}

		if(ret != NULL){
			break;
		}
	}

	return ret;
}

static struct snd_soc_dai *tcc_get_be_with_source_path(const struct snd_soc_dapm_widget *tcc_dapm_widget)
{
	const struct snd_soc_dapm_path *tcc_dapm_path = NULL;
	struct snd_soc_dai *be;
	struct snd_soc_dai *ret = NULL;

	if(tcc_dapm_widget != NULL){
		snd_soc_dapm_widget_for_each_source_path(tcc_dapm_widget, tcc_dapm_path) {
			if (tcc_dapm_path->connect == 0u){
				continue;
			}

			snd_card_dbg("source id = %d, name=%s, sname=%s", tcc_dapm_path->source->id,
				tcc_dapm_path->source->name, tcc_dapm_path->source->sname);
			snd_card_dbg("sink id = %d, name=%s, sname=%s", tcc_dapm_path->sink->id,
				tcc_dapm_path->sink->name, tcc_dapm_path->sink->sname);

			if (tcc_dapm_path->source->id == snd_soc_dapm_dai_out){
				ret = (struct snd_soc_dai *)tcc_dapm_path->source->priv;
				break;
			}

			be = tcc_get_be_with_source_path(tcc_dapm_path->source);
			if (be != NULL){
				ret = be;
				break;
			}
		}
	}

	return ret;
}

static ssize_t tcc_dai_dapm_path_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	int len,i;
	struct snd_soc_card *card = (struct snd_soc_card *)dev_get_drvdata(dev);
	const struct tcc_card_info_t *card_info = snd_soc_card_get_drvdata(card);

	unused(attr);
	unused(buf);

	snd_card_dbg("%s\n", __func__);

	len = 0;

	snd_card_dbg("%s num of links is %d\n", __func__, card_info->num_links);
	for (i = 0; i < card_info->num_links; i++) {
		snd_card_dbg("%s dai info[%d]\n", __func__, i);
		snd_card_dbg("%s dai info[%d] name=%s, stream name=%s\n", __func__, i, card_info->dai_link[i].name, card_info->dai_link[i].stream_name);
		snd_card_dbg("%s dai info[%d] cpus dai name=%s\n", __func__, i, card_info->dai_link[i].cpus->dai_name);
		snd_card_dbg("%s dai info[%d] codecs dai name=%s\n", __func__, i, card_info->dai_link[i].codecs->dai_name);

		if(card_info->dai_info[i].dai!= NULL){
			snd_card_dbg("%s, dai name =%s, id=%d", __func__, card_info->dai_info[i].dai->name, card_info->dai_info[i].dai->id);
			(void)tcc_get_be_with_sink_path(card_info->dai_info[i].dai->capture_widget);
			(void)tcc_get_be_with_source_path(card_info->dai_info[i].dai->playback_widget);
		}else{
			snd_card_dbg("card_info->dai_info[%d].dai does not exist!!!!%s\n", i, __func__);
		}
	}
	return len;
}

static DEVICE_ATTR(
	tcc_dai_dapm_path,
	(S_IRUSR|S_IRGRP|S_IROTH),
	tcc_dai_dapm_path_show,
	NULL);

static const struct attribute *tcc_audio_card_attributes[] = {
	&dev_attr_tcc_dai_dapm_path.attr,
	NULL,
};
#endif

static int tcc_snd_card_platform_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = NULL;
	int ret = 0;

	if (pdev == NULL) {
		ret = -EINVAL;
	} else {
		card = (struct snd_soc_card *) kzalloc(sizeof(struct snd_soc_card), GFP_KERNEL);
		if (card == NULL) {
			ret = -ENOMEM;
		}
	}

	if (card != NULL) {
		card->dev = &pdev->dev;
		card->late_probe = tcc_snd_card_late_probe;
		platform_set_drvdata(pdev, card);

		ret = snd_soc_of_parse_card_name(card, "card-name");
		if (ret == 0) {
			snd_card_dbg("%s %s\n", __func__, card->name);

			(void)parse_tcc_snd_card_dt(pdev, card);
			(void)tcc_snd_card_kcontrol_init(card);

			card->driver_name = DRIVER_NAME;
#if defined(CONFIG_SND_SOC_TELECHIPS_EXTENDED_TBD)
			card->dapm_widgets = tcc_snd_card_widgets;
			card->num_dapm_widgets = ARRAY_SIZE(tcc_snd_card_widgets);
#endif

			ret = snd_soc_register_card(card);
			if(ret != 0) {
				dev_err(&pdev->dev,
					"snd_soc_register_card failed (%d)\n",
					ret);
			}
#if defined(TCC_SND_CARD_DEBUG)//for debug
			else{
				ret = sysfs_create_files(&pdev->dev.kobj, tcc_audio_card_attributes);
				if (ret != 0) {
					snd_card_err("failed create sysfs\r\n");
				} else {
					snd_card_dbg("success create sysfs\r\n");
				}
			}
#endif
		} else {
			dev_err(&pdev->dev,
				"snd_soc_of_parse_card_name failed (%d)\n",
				ret);
		}
	} else {
		(void) pr_err("%s - failed (%d)\n", __func__, ret);
	}

	if ((ret != 0) && (card != NULL)) {
		kfree(card);
	}

	return ret;
}

static int tcc_snd_card_platform_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = NULL; //platform_get_drvdata(pdev);
	const struct tcc_card_info_t *card_info = NULL; //snd_soc_card_get_drvdata(card);
	int32_t num_controls = 0, num_links_i2s = 0;
	int i;

	if(pdev != NULL) {
		card = platform_get_drvdata(pdev);
		if(card != NULL) {
			card_info = snd_soc_card_get_drvdata(card);
		}
	}

	if (card_info != NULL) {
		for (i = 0; i < card_info->num_links; i++) {
				if (((strcmp(card_info->dai_link[i].cpus->of_node->name, "i2s")) == 0)
					&& (num_links_i2s < INT_MAX)){
				num_links_i2s++;
			}
		}

		num_controls =
			(int32_t) TCC_AUDIO_ARRAY_SIZE(tcc_snd_i2s_controls)
			* num_links_i2s;

		(void) snd_soc_unregister_card(card);

		for (i = 0; i < num_controls; i++) {
			if (card->controls[i].name != NULL) {
				kfree(card->controls[i].name);
			}
		}

		kfree(card->controls);
		kfree(card);

		kfree(card_info->dai_link);
		kfree(card_info->dai_info);
		kfree(card_info);
	}
	return 0;
}

static const struct of_device_id tcc_snd_card_of_match[] = {
	{.compatible = "telechips,snd-card",},
	{.compatible = "",},
};

MODULE_DEVICE_TABLE(of, tcc_snd_card_of_match);

static struct platform_driver tcc_snd_card_driver = {
	.driver = {
		.name = "tcc-soc-card",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = tcc_snd_card_of_match,
	},
	.probe = tcc_snd_card_platform_probe,
	.remove = tcc_snd_card_platform_remove,
};

module_platform_driver(tcc_snd_card_driver);

MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips Sound Card");
MODULE_LICENSE("GPL");
