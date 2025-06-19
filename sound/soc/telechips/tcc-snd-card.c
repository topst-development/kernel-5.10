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

#include "tcc_dai.h"
#include "tcc_audio_rule.h"

#undef snd_card_func_dbg
#if 0
#define snd_card_func_dbg(a...) \
	(void) pr_info("[FUNC_DEBUG][SOUND_CARD] " a)
#else
#define snd_card_func_dbg(a...)
#endif
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
	(10)
#define KCONTROL_HDR \
	"Device"

struct tcc_dai_info_t {
	struct snd_soc_dai *dai;
	int32_t mclk_div;
	uint32_t bclk_ratio;
	int32_t tdm_slots;
	int32_t tdm_width;
	uint32_t dai_fmt;
	bool is_updated;
};

struct tcc_card_info_t {
	int32_t num_links;
	struct snd_soc_dai_link *dai_link;
	struct tcc_dai_info_t *dai_info;
	struct snd_soc_codec_conf *codec_conf;
};

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
			snd_card_dbg("amixer %s success\n", __func__);
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
		if (card_info->dai_link[i].cpus->of_node == dai->dev->of_node) {
			ret = &card_info->dai_info[i];
			break;
		}
	}

	return ret;
}

static int tcc_snd_card_startup(struct snd_pcm_substream *substream)
{
	const struct snd_soc_pcm_runtime *rtd =
		(const struct snd_soc_pcm_runtime *)substream->private_data;
	struct snd_soc_dai *cpu_dai = (struct snd_soc_dai *)asoc_rtd_to_cpu(rtd, (0U));
	struct snd_soc_dai *codec_dai = (struct snd_soc_dai *)asoc_rtd_to_codec(rtd, (0U));
	const struct tcc_card_info_t *card_info =
		(const struct tcc_card_info_t *)snd_soc_card_get_drvdata(rtd->card);
	struct tcc_dai_info_t *dai_info;
	int32_t ret = 0;

	snd_card_func_dbg("++ %s ++\n", __func__);

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

	snd_card_func_dbg("-- %s --\n", __func__);

	return ret;
}

static void tcc_snd_card_shutdown(struct snd_pcm_substream *substream)
{
	snd_card_func_dbg("%s - Not support operation", __func__);
}

static int tcc_snd_card_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	int ret = 0;

#ifdef CONFIG_SND_SOC_CS4265
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct tcc_card_info_t *card_info
		= snd_soc_card_get_drvdata(rtd->card);
	struct tcc_dai_info_t *dai_info
		= tcc_snd_card_get_dai_info(card_info, cpu_dai);
	unsigned int mclk = 0;
	unsigned int mclk_fs = 0;

	if (dai_info->tdm_slots != 0) // the case of TDM
		mclk_fs = dai_info->mclk_div
				* dai_info->tdm_slots
				* dai_info->tdm_width;
	else // Not TDM
		mclk_fs = dai_info->mclk_div
				* dai_info->bclk_ratio;

	if (mclk_fs) {
		mclk = params_rate(params) * mclk_fs;
		ret = snd_soc_dai_set_sysclk(codec_dai,
							0,
							mclk,
							SND_SOC_CLOCK_IN);
		if (ret && ret != -ENOTSUPP)
			goto hw_param_err;

		/* Set CPU DAI is not implemented */
		ret = snd_soc_dai_set_sysclk(cpu_dai,
							0,
							mclk,
							SND_SOC_CLOCK_OUT);
		if ((ret != 0) && (ret == -ENOTSUPP))
			ret = 0;
	}
#else
	unused(params);
#endif
	snd_card_func_dbg("+- %s -+\n", __func__);

	return ret;
}

static int tcc_snd_card_hw_free(struct snd_pcm_substream *substream)
{
	int ret = 0;

	snd_card_func_dbg("%s - Not support operation", __func__);

	return ret;
}

static int tcc_snd_card_prepare(struct snd_pcm_substream *substream)
{
	int ret = 0;

	snd_card_func_dbg("%s - Not support operation", __func__);

	return ret;
}

static int tcc_snd_card_trigger(struct snd_pcm_substream *substream,
								int trigger)
{
	int ret = 0;

	snd_card_func_dbg("%s - Not support operation", __func__);

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
	int active = 0;

	active = snd_soc_dai_active(dai_info->dai);

	if ((dev_num > card->num_rtd)
		|| (active > 0)) {
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
			(dai_info->tdm_slots == 16) ? 4 :
			(dai_info->tdm_slots == 32) ? 5 : 3;
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
	int active = 0;

	active = snd_soc_dai_active(dai_info->dai);

	if ((dev_num > card->num_rtd)
		||(active > 0)) {
		snd_card_dbg("%s - fail\n", __func__);
		ret = -EINVAL;
	} else {
		snd_card_dbg("%s - success\n", __func__);
		dai_info->tdm_slots =
			(ucontrol->value.integer.value[0] == 0) ? 0 :
			(ucontrol->value.integer.value[0] == 1) ? 2 :
			(ucontrol->value.integer.value[0] == 2) ? 4 :
			(ucontrol->value.integer.value[0] == 3) ? 8 :
			(ucontrol->value.integer.value[0] == 4) ? 16 :
			(ucontrol->value.integer.value[0] == 5) ? 32 : 8;

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
	"32slots",
};

static const struct soc_enum tdm_slots_enum[] = {
	SOC_ENUM_SINGLE_EXT((TCC_AUDIO_ARRAY_SIZE(tdm_slots_texts)),
			    (tdm_slots_texts)),
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
	int active = 0;

	active = snd_soc_dai_active(dai_info->dai);

	if ((dev_num > card->num_rtd)
		||(active > 0)) {
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
	int active = 0;

	active = snd_soc_dai_active(dai_info->dai);

	if ((dev_num > card->num_rtd)
		||(active > 0)) {
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
	int active = 0;

	active = snd_soc_dai_active(dai_info->dai);

	if ((dev_num > card->num_rtd)
		||(active > 0)) {
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
	struct snd_soc_dai *cpu_dai = (struct snd_soc_dai *)asoc_rtd_to_cpu(rtd, (0U));
	struct snd_soc_dai *codec_dai = (struct snd_soc_dai *)(asoc_rtd_to_codec(rtd, (0U)));
	const struct tcc_card_info_t *card_info =
			snd_soc_card_get_drvdata(rtd->card);
	struct tcc_dai_info_t *dai_info;
	int ret = 0;

	snd_card_func_dbg("++ %s ++\n", __func__);

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

		if (dai_info->mclk_div != 0) {
			snd_card_dbg("%s - mclk_div: %d\n", __func__, dai_info->mclk_div);
			(void)snd_soc_dai_set_clkdiv(cpu_dai,
					(int32_t) TCC_DAI_CLKDIV_ID_MCLK_TO_BCLK,
					dai_info->mclk_div);
		}

		if (dai_info->bclk_ratio != 0u) {
			snd_card_dbg("%s - bclk_ratio: %d\n", __func__, dai_info->bclk_ratio);
			(void)snd_soc_dai_set_bclk_ratio(cpu_dai, dai_info->bclk_ratio);
		}
		dai_info->dai = cpu_dai;
	}

	snd_card_func_dbg("-- %s --\n", __func__);

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
	bool enable = FALSE;
	int ret = 0;

	snd_card_func_dbg("++ %s ++\n", __func__);

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
				"codec,name",
				&dai_link->codecs->name);
		(void)of_property_read_string(
				dnode,
				"codec,dai-name",
				&dai_link->codecs->dai_name);

		snd_card_dbg("\t\tstream_name : %s\n", dai_link->stream_name);

	if (dai_of_node != NULL) {
		dai_link->cpus->of_node = dai_of_node;

		if (platform_of_node != NULL) {
			dai_link->platforms->of_node = platform_of_node;
		} else {
			dai_link->platforms->of_node = dai_of_node;
		}
	} else {
		dai_link->cpus->name = "snd-soc-dummy";
		dai_link->cpus->dai_name = "snd-soc-dummy-dai";
		dai_link->platforms->name = "snd-soc-dummy";
	}

	if ((codec_of_node != NULL) && (dai_link->codecs->name == NULL)) {
		dai_link->codecs->of_node = codec_of_node;
		snd_card_dbg("\t\tcodec_dai_name: %s\n", dai_link->codecs->dai_name);
	} else if ((codec_of_node == NULL) && (dai_link->codecs->name != NULL)) {
		dai_link->codecs->of_node = NULL;
		if (dai_link->codecs->dai_name == NULL) {
			dai_link->codecs->dai_name = "snd-soc-dummy-dai";
		}
		snd_card_dbg("\t\tcodec_name: %s\n", dai_link->codecs->name);
		snd_card_dbg("\t\tcodec_dai_name: %s\n", dai_link->codecs->dai_name);
	} else {
		dai_link->codecs->of_node = NULL;
		dai_link->codecs->name = "snd-soc-dummy";
		dai_link->codecs->dai_name = "snd-soc-dummy-dai";
		snd_card_dbg("\t\tcodec_dai_name: %s\n", dai_link->codecs->dai_name);
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
		(void)of_property_read_s32(dnode, "mclk_div", &dai_info->mclk_div);
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

		dai_info->is_updated = FALSE;
	}

	snd_card_func_dbg("-- %s --\n", __func__);

	return ret;
}

static int parse_tcc_snd_card_dt(const struct platform_device *pdev,
				 struct snd_soc_card *card)
{
	const struct device_node *dnode = pdev->dev.of_node;
	struct tcc_card_info_t *card_info = NULL;
	int i, not_failed_name_count;
	const struct device_node *dai_link;
	ssize_t alloc_size;
	int ret = 0;

	snd_card_func_dbg("++ %s ++\n", __func__);

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
			kfree(card_info);
			continue;
		}

		snd_card_dbg("num_links : %d\n", card_info->num_links);

		alloc_size =
			(ssize_t) sizeof(struct snd_soc_dai_link) * card_info->num_links;
		card_info->dai_link =
			(struct snd_soc_dai_link *)kzalloc((size_t) alloc_size, GFP_KERNEL);
		if (card_info->dai_link == NULL){
			ret = -ENOMEM;
			kfree(card_info);
			continue;
		} else {
			(void) memset(card_info->dai_link, 0, (size_t)alloc_size);
		}

		alloc_size =
			(ssize_t) sizeof(struct tcc_dai_info_t) * card_info->num_links;
		card_info->dai_info = kzalloc((size_t) alloc_size, GFP_KERNEL);
		if(card_info->dai_info == NULL){
			kfree(card_info->dai_link);
			kfree(card_info);
			ret = -ENOMEM;
			continue;
		} else {
			(void) memset(card_info->dai_info, 0, (size_t)alloc_size);
		}

		alloc_size =
			(ssize_t) sizeof(struct snd_soc_codec_conf) * card_info->num_links;
		card_info->codec_conf = kzalloc((size_t) alloc_size, GFP_KERNEL);
		if(card_info->codec_conf == NULL){
			kfree(card_info->dai_info);
			kfree(card_info->dai_link);
			kfree(card_info);
			ret = -ENOMEM;
			continue;
		} else {
			(void) memset(card_info->codec_conf, 0, (size_t)alloc_size);
		}

		dai_link = of_get_child_by_name(dnode, "telechips,dai-link");
		if (dai_link != NULL) {
			struct device_node *np;
			int cnt = 0;
			const char *overlay_dt_prop = of_get_property(dnode, "overlay_dt", NULL);

			if (overlay_dt_prop) {
				cnt = card_info->num_links - 1;
			}

			for_each_child_of_node((dnode), (np)) {
				snd_card_dbg("\tlink %d:\n", cnt);
				if (overlay_dt_prop) {
					alloc_size =
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
						continue;
					}

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
					if (cnt < card_info->num_links) {
						alloc_size =
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
							continue;
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
		}

		card->num_links = card_info->num_links;
		card->dai_link = card_info->dai_link;

		for (i = 0; i < card_info->num_links; i++) {
			char tmp_name[255];

			(void)scnprintf(tmp_name, (sizeof(tmp_name) - 1u), KCONTROL_HDR "%d", i);
			card_info->codec_conf[i].name_prefix =
				kstrdup(tmp_name, GFP_KERNEL);
			if (card_info->codec_conf[i].name_prefix == NULL) {
				int j;
				ret = -ENOMEM;
				not_failed_name_count = i;
				for (j = 0; j < not_failed_name_count; j++) {
					if(card_info->codec_conf[j].name_prefix != NULL) {
						snd_card_dbg("name_prefix(%d) free\n", j);
						kfree(card_info->codec_conf[j].name_prefix);
					}
				}
				break;
			} else {
				card_info->codec_conf[i].dlc.of_node =
					card_info->dai_link[i].cpus->of_node;
				snd_card_dbg("name_prefix(%d) : %s\n",
						i,
						card_info->codec_conf[i].name_prefix);
			}
		}

		if(ret < 0){
			kfree(card_info->dai_info);
			kfree(card_info->dai_link);
			kfree(card_info);
			continue;
		}

		card->codec_conf = card_info->codec_conf;
		card->num_configs = card_info->num_links;

		snd_soc_card_set_drvdata(card, card_info);

	} while (false);

	snd_card_func_dbg("-- %s --\n", __func__);

	return ret;
}


static void free_snd_kcontrol_new(const struct snd_kcontrol_new *controls, int cnt)
{
	int i;

	snd_card_func_dbg("++ %s ++\n", __func__);

	for (i = 0; i < cnt; i++) {
		if((controls != NULL) &&
			(controls[i].name != NULL)) {
			kfree(controls[i].name);
		}
	}

	if(controls != NULL) {
		kfree(controls);
	}

	snd_card_func_dbg("-- %s --\n", __func__);

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

	snd_card_func_dbg("++ %s ++\n", __func__);

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

	snd_card_func_dbg("-- %s --\n", __func__);

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

	snd_card_func_dbg("++ %s ++\n", __func__);

	i = 0;
	do {
		rtd = snd_soc_get_pcm_runtime(card, &card->dai_link[i]);
		cpu_dai = asoc_rtd_to_cpu(rtd, (0U));
		dai_info = tcc_snd_card_get_dai_info(card_info, cpu_dai);

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

	snd_card_func_dbg("-- %s --\n", __func__);

	return ret;
}

static int tcc_snd_card_platform_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = NULL;
	int ret = 0;

	snd_card_func_dbg("++ %s ++\n", __func__);

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

			ret = snd_soc_register_card(card);
			if(ret != 0) {
				dev_err(&pdev->dev,
					"snd_soc_register_card failed (%d)\n",
					ret);
			}
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

	snd_card_func_dbg("-- %s --\n", __func__);

	return ret;
}

static int tcc_snd_card_platform_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = NULL; //platform_get_drvdata(pdev);
	const struct tcc_card_info_t *card_info = NULL; //snd_soc_card_get_drvdata(card);
	int32_t num_controls = 0, num_links_i2s = 0;
	int i;

	snd_card_func_dbg("++ %s ++\n", __func__);

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

	snd_card_func_dbg("-- %s --\n", __func__);

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
