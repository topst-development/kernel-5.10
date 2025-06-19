// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/errno.h>

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/io.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>

#include "tcc_asrc_dai.h"
#include "tcc_asrc_pcm.h"
#include "tcc_asrc_drv.h"

#define unused(x) (void)(x)
#undef asrc_dai_dbg
#if 0
#define asrc_dai_dbg(a...)\
        (void) pr_info("[DEBUG][ASRC_DAI] " a)
#else
#define asrc_dai_dbg(a...)
#endif
#define asrc_dai_err(a...)\
        (void) pr_err("[ERROR][ASRC_DAI] " a)

#define TCC_ASRC_SUPPORTED_FORMATS\
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE)
#define TCC_ASRC_SUPPORTED_RATES\
	(SNDRV_PCM_RATE_KNOT | si_to_ui(SNDRV_PCM_RATE_8000_192000))

static char const *asrc_dai_names[] = {
	"ASRC-PAIR0",
	"ASRC-PAIR1",
	"ASRC-PAIR2",
	"ASRC-PAIR3",
};

static char const *asrc_stream_name_play[] = {
	"ASRC-PAIR0-Playback",
	"ASRC-PAIR1-Playback",
	"ASRC-PAIR2-Playback",
	"ASRC-PAIR3-Playback",
};

static char const *asrc_stream_name_capture[] = {
	"ASRC-PAIR0-Capture",
	"ASRC-PAIR1-Capture",
	"ASRC-PAIR2-Capture",
	"ASRC-PAIR3-Capture",
};

void tcc_asrc_dai_driver_init(struct tcc_asrc_t *asrc);

static int tcc_asrc_dai_startup(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct tcc_asrc_t *asrc =
	    (struct tcc_asrc_t *)snd_soc_dai_get_drvdata(dai);
	uint32_t asrc_pair = si_to_ui(dai->id);

	unused(substream);
	asrc_dai_dbg("%s - active : %d\n", __func__, dai->active);
	asrc->pair[asrc_pair].m2m_stat.started = 1U;

	return 0;
}

static void tcc_asrc_dai_shutdown(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct tcc_asrc_t *asrc =
	    (struct tcc_asrc_t *)snd_soc_dai_get_drvdata(dai);
	uint32_t asrc_pair = si_to_ui(dai->id);

	unused(substream);
	asrc_dai_dbg("%s - active : %d\n", __func__, dai->active);
	asrc_dai_dbg("%s\n", __func__);

	(void)tcc_asrc_stop(asrc, asrc_pair);

	asrc->pair[asrc_pair].m2m_stat.started = 0U;
}

static int tcc_asrc_dai_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct tcc_asrc_t *asrc =
	    (struct tcc_asrc_t *)snd_soc_dai_get_drvdata(dai);

	uint32_t asrc_pair = si_to_ui(dai->id);
	uint32_t channels = params_channels(params);
	snd_pcm_format_t format = params_format(params);
	uint32_t rate = params_rate(params);
	struct tcc_asrc_hw_param_t tcc_asrc_hw_param;
	unused(substream);

	asrc_dai_dbg("%s\n", __func__);

	asrc_dai_dbg("dai->id : %d\n", dai->id);
	asrc_dai_dbg("format : 0x%08x\n", format);
	asrc_dai_dbg("rate : %d\n", rate);
	asrc_dai_dbg("channels : %d\n", channels);

	tcc_asrc_hw_param.bitwidth = (format == SNDRV_PCM_FORMAT_S24_LE) ? TCC_ASRC_24BIT
					: TCC_ASRC_16BIT;
	tcc_asrc_hw_param.channels = (channels == 8U) ? TCC_ASRC_NUM_OF_CH_8
					: (channels == 6U) ? TCC_ASRC_NUM_OF_CH_6
					: (channels == 4U) ? TCC_ASRC_NUM_OF_CH_4
					: TCC_ASRC_NUM_OF_CH_2;

	if (asrc->pair[asrc_pair].hw.asrc_path == TCC_ASRC_M2P_PATH) {
		tcc_asrc_hw_param.sync_mode = asrc->pair[asrc_pair].hw.sync_mode;
		tcc_asrc_hw_param.peri_dai = asrc->pair[asrc_pair].hw.peri_dai;
		tcc_asrc_hw_param.async_refclk = asrc->pair[asrc_pair].hw.async_refclk;
		tcc_asrc_hw_param.src_rate = rate;
		tcc_asrc_hw_param.dst_rate = asrc->pair[asrc_pair].hw.peri_dai_rate;
		(void)tcc_asrc_m2p_setup(
			asrc,
			asrc_pair,
			&tcc_asrc_hw_param);
	} else if (asrc->pair[asrc_pair].hw.asrc_path == TCC_ASRC_P2M_PATH) {
		tcc_asrc_hw_param.sync_mode = asrc->pair[asrc_pair].hw.sync_mode;
		tcc_asrc_hw_param.peri_dai = asrc->pair[asrc_pair].hw.peri_dai;
		tcc_asrc_hw_param.peri_dai_bitwidth = asrc->pair[asrc_pair].hw.peri_dai_format;
		tcc_asrc_hw_param.async_refclk = asrc->pair[asrc_pair].hw.async_refclk;
		tcc_asrc_hw_param.src_rate = asrc->pair[asrc_pair].hw.peri_dai_rate;
		tcc_asrc_hw_param.dst_rate = rate;
		(void)tcc_asrc_p2m_setup(
			asrc,
			asrc_pair,
			&tcc_asrc_hw_param);
	} else {
			asrc_dai_err("%s Not supported asrc path\n", __func__);
	}

	return 0;
}

static int tcc_asrc_dai_trigger(
	struct snd_pcm_substream *substream, int cmd,
	struct snd_soc_dai *dai)
{
	const struct tcc_asrc_t *asrc =
	    (struct tcc_asrc_t *)snd_soc_dai_get_drvdata(dai);
	uint32_t asrc_pair = si_to_ui(dai->id);
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			asrc_dai_dbg("TRIGGER_START, PLAY\n");
			(void)tcc_asrc_tx_fifo_enable(asrc, asrc_pair, (bool)true);
		} else {
			asrc_dai_dbg("TRIGGER_START, CAPTURE\n");
			(void)tcc_asrc_rx_fifo_enable(asrc, asrc_pair, (bool)true);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			asrc_dai_dbg("TRIGGER_STOP, PLAY\n");
			(void)tcc_asrc_tx_fifo_enable(asrc, asrc_pair, (bool)false);
		} else {
			asrc_dai_dbg("TRIGGER_STOP, CAPTURE\n");
			(void)tcc_asrc_rx_fifo_enable(asrc, asrc_pair, (bool)false);
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int tcc_asrc_dai_set_sysclk(
	struct snd_soc_dai *dai, int clk_id,
	unsigned int freq, int dir)
{
	struct tcc_asrc_t *asrc =
	    (struct tcc_asrc_t *)snd_soc_dai_get_drvdata(dai);
	uint32_t asrc_pair = si_to_ui(dai->id);
	int ret = 0;

	asrc_dai_dbg("%s - asrc_pair : %d, clk_id :%d, freq:%d, dir :%d\n",
		__func__,
		asrc_pair,
		clk_id,
		freq,
		dir);

	unused(dir);
	switch (clk_id) {
	case TCC_ASRC_CLKID_PERI_DAI_RATE:	//sample rate
		asrc->pair[asrc_pair].hw.peri_dai_rate = freq;
		break;
	case TCC_ASRC_CLKID_PERI_DAI_FORMAT:	//freq:dai_format
		asrc->pair[asrc_pair].hw.peri_dai_format =
			ui_to_enum_tcc_asrc_drv_bitwidth_t(freq);
		break;
	case TCC_ASRC_CLKID_PERI_DAI:	//freq:peri_target
		asrc->pair[asrc_pair].hw.peri_dai =
			(freq == 0U) ? TCC_ASRC_PERI_DAI0
			: (freq == 1U) ? TCC_ASRC_PERI_DAI1
			: (freq == 2U) ? TCC_ASRC_PERI_DAI2
			: TCC_ASRC_PERI_DAI3;
		if ((asrc->mcaudio_m2p_mux[freq] < 0)
		    && (asrc->pair[asrc_pair].hw.asrc_path == TCC_ASRC_M2P_PATH)) {
			(void)tcc_asrc_set_m2p_mux_select(asrc, freq, asrc_pair);
		}
		break;
	default:
		asrc_dai_err("%s - clk_id is invalid\n", __func__);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static struct snd_soc_dai_ops tcc_asrc_ops = {
	.set_sysclk = tcc_asrc_dai_set_sysclk,
	.startup = tcc_asrc_dai_startup,
	.shutdown = tcc_asrc_dai_shutdown,
	.hw_params = tcc_asrc_dai_hw_params,
	.trigger = tcc_asrc_dai_trigger,
};

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X)

static int asrc_vol_ramp_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
	    snd_soc_kcontrol_component(kcontrol);
	struct tcc_asrc_t *asrc =
	    (struct tcc_asrc_t *)snd_soc_component_get_drvdata(component);
	const struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)ul_to_ptr_soc_mixer_control(
			    kcontrol->private_value);
	uint32_t asrc_pair = si_to_ui(mc->reg);
	uint32_t value;

	value = sl_to_ui(ucontrol->value.integer.value[0]);
	if (mc->invert != 0U) {
		value = ui_sub(si_to_ui(mc->max), value);
	}

	asrc->pair[asrc_pair].volume_ramp.gain = value * 2U;

	(void)tcc_asrc_volume_ramp(asrc, asrc_pair);

	return 0;
}

static int asrc_vol_ramp_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
	    snd_soc_kcontrol_component(kcontrol);
	const struct tcc_asrc_t *asrc =
	    (struct tcc_asrc_t *)snd_soc_component_get_drvdata(component);
	const struct soc_mixer_control *mc =
	    (struct soc_mixer_control *)ul_to_ptr_soc_mixer_control(
			    kcontrol->private_value);
	uint32_t asrc_pair = si_to_ui(mc->reg);
	uint32_t value;

	value = (asrc->pair[asrc_pair].volume_ramp.gain / 2U);
	if (mc->invert != 0U) {
		value = ui_sub(si_to_ui(mc->max), value);
	}

	ucontrol->value.integer.value[0] = ui_to_sl(value);

	return 0;
}

#define RAMP_VOL_MAX_STEP\
	(TCC_ASRC_VOL_RAMP_GAIN_MAX/2)// 0.25dB = 2
static const DECLARE_TLV_DB_SCALE(asrc_vol_ramp_tlv, 0xFFFFDCBFU /*-9025*/, 25, 0);

static char const *fade_in_out_texts[] = {
	"64dB per Sample",
	"32dB per Sample",
	"16dB per Sample",
	"8dB per Sample",
	"4dB per Sample",
	"2dB per Sample",
	"1dB per Sample",
	"0.5dB per Sample",
	"0.25dB per Sample",
	"0.125dB per 1 Sample",
	"0.125dB per 2 Sample",
	"0.125dB per 4 Sample",
	"0.125dB per 8 Sample",
	"0.125dB per 16 Sample",
	"0.125dB per 32 Sample",
	"0.125dB per 64 Sample",
};

static const struct soc_enum fade_in_out_enum[] = {
	SOC_ENUM_SINGLE_EXT(TCC_ASRC_ARRAY_SIZE(fade_in_out_texts), (fade_in_out_texts)),
};

static int fade_out_time_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol,
	uint32_t asrc_pair)
{
	struct snd_soc_component *component =
	    snd_soc_kcontrol_component(kcontrol);
	struct tcc_asrc_t *asrc =
	    (struct tcc_asrc_t *)snd_soc_component_get_drvdata(component);

	asrc->pair[asrc_pair].volume_ramp.dn_time =
	    ui_add(sl_to_ui(ucontrol->value.integer.value[0]), 1U);

	return 0;
}

static int fade_out_time_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol,
	uint32_t asrc_pair)
{
	struct snd_soc_component *component =
	    snd_soc_kcontrol_component(kcontrol);
	const struct tcc_asrc_t *asrc =
	    (struct tcc_asrc_t *)snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] =
	    ui_to_si(ui_sub(asrc->pair[asrc_pair].volume_ramp.dn_time, 1U));

	return 0;
}

int pair0_fade_out_time_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fade_out_time_put(kcontrol, ucontrol, 0);
}

int pair0_fade_out_time_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fade_out_time_get(kcontrol, ucontrol, 0);
}

int pair1_fade_out_time_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fade_out_time_put(kcontrol, ucontrol, 1);
}

int pair1_fade_out_time_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fade_out_time_get(kcontrol, ucontrol, 1);
}

int pair2_fade_out_time_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fade_out_time_put(kcontrol, ucontrol, 2);
}

int pair2_fade_out_time_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fade_out_time_get(kcontrol, ucontrol, 2);
}

int pair3_fade_out_time_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fade_out_time_put(kcontrol, ucontrol, 3);
}

int pair3_fade_out_time_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fade_out_time_get(kcontrol, ucontrol, 3);
}
static int fade_in_time_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol,
	uint32_t asrc_pair)
{
	struct snd_soc_component *component =
	    snd_soc_kcontrol_component(kcontrol);
	struct tcc_asrc_t *asrc =
	    (struct tcc_asrc_t *)snd_soc_component_get_drvdata(component);

	asrc->pair[asrc_pair].volume_ramp.up_time =
	    ui_add(sl_to_ui(ucontrol->value.integer.value[0]), 1U);

	return 0;
}

static int fade_in_time_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol,
	uint32_t asrc_pair)
{
	struct snd_soc_component *component =
	    snd_soc_kcontrol_component(kcontrol);
	const struct tcc_asrc_t *asrc =
	    (struct tcc_asrc_t *)snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] =
	    ui_to_si(ui_sub(asrc->pair[asrc_pair].volume_ramp.up_time, 1U));

	return 0;
}

int pair0_fade_in_time_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fade_in_time_put(kcontrol, ucontrol, 0);
}

int pair0_fade_in_time_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fade_in_time_get(kcontrol, ucontrol, 0);
}

int pair1_fade_in_time_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fade_in_time_put(kcontrol, ucontrol, 1);
}

int pair1_fade_in_time_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fade_in_time_get(kcontrol, ucontrol, 1);
}

int pair2_fade_in_time_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fade_in_time_put(kcontrol, ucontrol, 2);
}

int pair2_fade_in_time_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fade_in_time_get(kcontrol, ucontrol, 2);
}

int pair3_fade_in_time_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fade_in_time_put(kcontrol, ucontrol, 3);
}

int pair3_fade_in_time_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fade_in_time_get(kcontrol, ucontrol, 3);
}
#endif

static char const *mcaudio_mux_texts[] = {
	"ASRC Pair0",
	"ASRC Pair1",
	"ASRC Pair2",
	"ASRC Pair3",
	"There isn't M2P Pair",
};

static const struct soc_enum mcaudio_mux_enum[] = {
	SOC_ENUM_SINGLE_EXT(TCC_ASRC_ARRAY_SIZE(mcaudio_mux_texts), (mcaudio_mux_texts)),
};

static int mcaudio_mux_put(
	struct snd_kcontrol *kcontrol,
	const struct snd_ctl_elem_value *ucontrol,
	uint32_t mcaudio_ch)
{
	struct snd_soc_component *component =
	    snd_soc_kcontrol_component(kcontrol);
	struct tcc_asrc_t *asrc =
	    (struct tcc_asrc_t *)snd_soc_component_get_drvdata(component);
	uint32_t asrc_pair;

	asrc_pair = sl_to_ui(ucontrol->value.integer.value[0]);
	if ((asrc->pair[asrc_pair].hw.asrc_path == TCC_ASRC_M2P_PATH)
	    && ((uint32_t)asrc->pair[asrc_pair].hw.peri_dai == mcaudio_ch)) {
		(void)tcc_asrc_set_m2p_mux_select(asrc, mcaudio_ch, asrc_pair);
	} else {
		asrc_dai_err("ASRC Pair%d isn't M2P path or",
			asrc_pair);
		asrc_dai_err("its target(%d) isn't MCAudio%d\n",
		     asrc->pair[asrc_pair].hw.peri_dai,
			 mcaudio_ch);
	}
	return 0;
}

static int mcaudio_mux_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol,
	uint32_t mcaudio_ch)
{
	struct snd_soc_component *component =
	    snd_soc_kcontrol_component(kcontrol);
	const struct tcc_asrc_t *asrc =
	    (struct tcc_asrc_t *)snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] =
		(asrc->mcaudio_m2p_mux[mcaudio_ch] < 0) ? 4
		: asrc->mcaudio_m2p_mux[mcaudio_ch];

	return 0;
}

int mcaudio0_mux_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return mcaudio_mux_put(kcontrol, ucontrol, 0);
}

int mcaudio0_mux_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return mcaudio_mux_get(kcontrol, ucontrol, 0);
}

int mcaudio1_mux_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return mcaudio_mux_put(kcontrol, ucontrol, 1);
}

int mcaudio1_mux_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return mcaudio_mux_get(kcontrol, ucontrol, 1);
}

int mcaudio2_mux_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return mcaudio_mux_put(kcontrol, ucontrol, 2);
}

int mcaudio2_mux_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return mcaudio_mux_get(kcontrol, ucontrol, 2);
}

int mcaudio3_mux_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return mcaudio_mux_put(kcontrol, ucontrol, 3);
}

int mcaudio3_mux_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return mcaudio_mux_get(kcontrol, ucontrol, 3);
}

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
static const char * const fifo_in_size_texts[] = {
	"256 word",
	"128 word",
	"64 word",
	"32 word",
	"16 word",
	"8 word",
	"4 word",
	"2 word",
};

static const struct soc_enum fifo_in_size_enum[] = {
	SOC_ENUM_SINGLE_EXT(TCC_ASRC_ARRAY_SIZE(fifo_in_size_texts), (fifo_in_size_texts)),
};

static int fifo_in_size_set(struct snd_kcontrol *kcontrol,
	const struct snd_ctl_elem_value *ucontrol,
	uint32_t asrc_pair)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct tcc_asrc_t *asrc =
	(struct tcc_asrc_t *)snd_soc_component_get_drvdata(component);
	uint32_t val = sl_to_ui(ucontrol->value.integer.value[0]);

	asrc->pair[asrc_pair].hw.fifo_in_size =
		(val == 0U) ? TCC_ASRC_FIFO_SIZE_256WORD :
		(val == 1U) ? TCC_ASRC_FIFO_SIZE_128WORD :
		(val == 2U) ? TCC_ASRC_FIFO_SIZE_64WORD :
		(val == 3U) ? TCC_ASRC_FIFO_SIZE_32WORD :
		(val == 4U) ? TCC_ASRC_FIFO_SIZE_16WORD :
		(val == 5U) ? TCC_ASRC_FIFO_SIZE_8WORD :
		(val == 6U) ? TCC_ASRC_FIFO_SIZE_4WORD :
		TCC_ASRC_FIFO_SIZE_2WORD;

	return 0;
}

static int fifo_in_size_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol,
			uint32_t asrc_pair)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	const struct tcc_asrc_t *asrc =
	(struct tcc_asrc_t *)snd_soc_component_get_drvdata(component);
	enum tcc_asrc_drv_fifo_size_t size =
			asrc->pair[asrc_pair].hw.fifo_in_size;

	ucontrol->value.integer.value[0] =
		(size == TCC_ASRC_FIFO_SIZE_256WORD) ? 0 :
		(size == TCC_ASRC_FIFO_SIZE_128WORD) ? 1 :
		(size == TCC_ASRC_FIFO_SIZE_64WORD) ? 2 :
		(size == TCC_ASRC_FIFO_SIZE_32WORD) ? 3 :
		(size == TCC_ASRC_FIFO_SIZE_16WORD) ? 4 :
		(size == TCC_ASRC_FIFO_SIZE_8WORD) ? 5 :
		(size == TCC_ASRC_FIFO_SIZE_4WORD) ? 6 : 7;

	return 0;
}


int pair0_fifo_in_size_set(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fifo_in_size_set(kcontrol, ucontrol, 0);
}

int pair0_fifo_in_size_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fifo_in_size_get(kcontrol, ucontrol, 0);
}

int pair1_fifo_in_size_set(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fifo_in_size_set(kcontrol, ucontrol, 1);
}

int pair1_fifo_in_size_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fifo_in_size_get(kcontrol, ucontrol, 1);
}

int pair2_fifo_in_size_set(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fifo_in_size_set(kcontrol, ucontrol, 2);
}

int pair2_fifo_in_size_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fifo_in_size_get(kcontrol, ucontrol, 2);
}

int pair3_fifo_in_size_set(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fifo_in_size_set(kcontrol, ucontrol, 3);
}

int pair3_fifo_in_size_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return fifo_in_size_get(kcontrol, ucontrol, 3);
}
#endif

static const struct snd_kcontrol_new tcc_asrc_snd_controls[] = {
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X)
	SOC_SINGLE_EXT_TLV(
		"ASRC Pair0 Volume Ramp Gain",
		0,
		0,
		RAMP_VOL_MAX_STEP,
		1,
		asrc_vol_ramp_get,
		asrc_vol_ramp_put,
		asrc_vol_ramp_tlv),
	SOC_SINGLE_EXT_TLV(
		"ASRC Pair1 Volume Ramp Gain",
		1,
		0,
		RAMP_VOL_MAX_STEP,
		1,
		asrc_vol_ramp_get,
		asrc_vol_ramp_put,
		asrc_vol_ramp_tlv),
	SOC_SINGLE_EXT_TLV(
		"ASRC Pair2 Volume Ramp Gain",
		2,
		0,
		RAMP_VOL_MAX_STEP,
		1,
		asrc_vol_ramp_get,
		asrc_vol_ramp_put,
		asrc_vol_ramp_tlv),
	SOC_SINGLE_EXT_TLV(
		"ASRC Pair3 Volume Ramp Gain",
		3,
		0,
		RAMP_VOL_MAX_STEP,
		1,
		asrc_vol_ramp_get,
		asrc_vol_ramp_put,
		asrc_vol_ramp_tlv),

	SOC_ENUM_EXT(
		"ASRC Pair0 Fade Out Time",
		(fade_in_out_enum[0]),
		(pair0_fade_out_time_get),
		(pair0_fade_out_time_put)),
	SOC_ENUM_EXT(
		"ASRC Pair1 Fade Out Time",
		(fade_in_out_enum[0]),
		(pair1_fade_out_time_get),
		(pair1_fade_out_time_put)),
	SOC_ENUM_EXT(
		"ASRC Pair2 Fade Out Time",
		(fade_in_out_enum[0]),
		(pair2_fade_out_time_get),
		(pair2_fade_out_time_put)),
	SOC_ENUM_EXT(
		"ASRC Pair3 Fade Out Time",
		(fade_in_out_enum[0]),
		(pair3_fade_out_time_get),
		(pair3_fade_out_time_put)),

	SOC_ENUM_EXT(
		"ASRC Pair0 Fade In Time",
		(fade_in_out_enum[0]),
		(pair0_fade_in_time_get),
		(pair0_fade_in_time_put)),
	SOC_ENUM_EXT(
		"ASRC Pair1 Fade In Time",
		(fade_in_out_enum[0]),
		(pair1_fade_in_time_get),
		(pair1_fade_in_time_put)),
	SOC_ENUM_EXT(
		"ASRC Pair2 Fade In Time",
		(fade_in_out_enum[0]),
		(pair2_fade_in_time_get),
		(pair2_fade_in_time_put)),
	SOC_ENUM_EXT(
		"ASRC Pair3 Fade In Time",
		(fade_in_out_enum[0]),
		(pair3_fade_in_time_get),
		(pair3_fade_in_time_put)),
#endif

	SOC_ENUM_EXT(
		"ASRC MCAudio0 Out Mux",
		(mcaudio_mux_enum[0]),
		(mcaudio0_mux_get),
		(mcaudio0_mux_put)),
	SOC_ENUM_EXT(
		"ASRC MCAudio1 Out Mux",
		(mcaudio_mux_enum[0]),
		(mcaudio1_mux_get),
		(mcaudio1_mux_put)),
	SOC_ENUM_EXT(
		"ASRC MCAudio2 Out Mux",
		(mcaudio_mux_enum[0]),
		(mcaudio2_mux_get),
		(mcaudio2_mux_put)),
	SOC_ENUM_EXT(
		"ASRC MCAudio3 Out Mux",
		(mcaudio_mux_enum[0]),
		(mcaudio3_mux_get),
		(mcaudio3_mux_put)),

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	SOC_ENUM_EXT("ASRC Pair0 FIFO In Size", (fifo_in_size_enum[0]),
				(pair0_fifo_in_size_get), (pair0_fifo_in_size_set)),
	SOC_ENUM_EXT("ASRC Pair1 FIFO In Size", (fifo_in_size_enum[0]),
				(pair1_fifo_in_size_get), (pair1_fifo_in_size_set)),
	SOC_ENUM_EXT("ASRC Pair2 FIFO In Size", (fifo_in_size_enum[0]),
				(pair2_fifo_in_size_get), (pair2_fifo_in_size_set)),
	SOC_ENUM_EXT("ASRC Pair3 FIFO In Size", (fifo_in_size_enum[0]),
				(pair3_fifo_in_size_get), (pair3_fifo_in_size_set)),
#endif
};

static const struct snd_soc_component_driver tcc_asrc_component_drv = {
	.name = DRV_NAME,
	.controls = tcc_asrc_snd_controls,
	.num_controls = TCC_ASRC_ARRAY_SIZE(tcc_asrc_snd_controls),
	.open           = tcc_asrc_pcm_open,
	.close          = tcc_asrc_pcm_close,
	.ioctl          = tcc_asrc_pcm_ioctl,
	.hw_params      = tcc_asrc_pcm_hw_params,
	.hw_free        = tcc_asrc_pcm_hw_free,
	.prepare        = tcc_asrc_pcm_prepare,
	.trigger        = tcc_asrc_pcm_trigger,
	.pointer        = tcc_asrc_pcm_pointer,
	.mmap           = tcc_asrc_pcm_mmap,
	.pcm_construct  = tcc_asrc_pcm_new,
	.pcm_destruct   = tcc_asrc_pcm_free_dma_buffers,
};

#if 0
struct snd_soc_dai_driver tcc_asrc_dai_drv[] = {
	{
	 .name = "ASRC-PAIR0",
	 .playback = {
	      .stream_name = "ASRC-PAIR0-Playback",
	      .channels_min = 1,
	      .channels_max = 8,
	      .rates = SNDRV_PCM_RATE_8000_192000,
	      .formats =
	      SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	      },
	 .ops = &tcc_asrc_ops,
	 },
	{
	 .name = "ASRC-PAIR1",
	 .playback = {
	      .stream_name = "ASRC-PAIR1-Playback",
	      .channels_min = 1,
	      .channels_max = 8,
	      .rates = SNDRV_PCM_RATE_8000_192000,
	      .formats =
	      SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	      },
	 .ops = &tcc_asrc_ops,
	 },
	{
	 .name = "ASRC-PAIR2",
	 .playback = {
	      .stream_name = "ASRC-PAIR2-Playback",
	      .channels_min = 1,
	      .channels_max = 8,
	      .rates = SNDRV_PCM_RATE_8000_192000,
	      .formats =
	      SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	      },
	 .ops = &tcc_asrc_ops,
	 },
	{
	 .name = "ASRC-PAIR3",
	 .capture = {
	     .stream_name = "ASRC-PAIR3-Capture",
	     .channels_min = 1,
	     .channels_max = 8,
	     .rates = SNDRV_PCM_RATE_8000_192000,
	     .formats =
	     SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	     },
	 .ops = &tcc_asrc_ops,
	 },
};
#endif

void tcc_asrc_dai_driver_init(struct tcc_asrc_t *asrc)
{
	uint32_t i;

	(void)memset(asrc->dai_drv, 0,
	       sizeof(struct snd_soc_dai_driver) * NUM_OF_ASRC_PAIR);

	for (i = 0; i < NUM_OF_ASRC_PAIR; i++) {
		struct snd_soc_dai_driver *dai_drv = &asrc->dai_drv[i];

		dai_drv->name = asrc_dai_names[i];
		switch (asrc->pair[i].hw.asrc_path) {
		case TCC_ASRC_M2P_PATH:
			dai_drv->playback.stream_name =
			    asrc_stream_name_play[i];
			dai_drv->playback.channels_min = TCC_ASRC_MIN_CHANNELS;
			dai_drv->playback.channels_max =
			    asrc->pair[i].hw.max_channel;
			dai_drv->playback.rates = TCC_ASRC_SUPPORTED_RATES;
			dai_drv->playback.rate_min = 8000;
			dai_drv->playback.rate_max = 192000;
			dai_drv->playback.formats = TCC_ASRC_SUPPORTED_FORMATS;
			break;
		case TCC_ASRC_P2M_PATH:
			dai_drv->capture.stream_name =
			    asrc_stream_name_capture[i];
			dai_drv->capture.channels_min = TCC_ASRC_MIN_CHANNELS;
			dai_drv->capture.channels_max =
			    asrc->pair[i].hw.max_channel;
			dai_drv->capture.rates = TCC_ASRC_SUPPORTED_RATES;
			dai_drv->capture.rate_min = 8000;
			dai_drv->capture.rate_max = 192000;
			dai_drv->capture.formats = TCC_ASRC_SUPPORTED_FORMATS;
			break;
		default:
			asrc_dai_err("%s Not supported asrc path\n", __func__);
			break;
		}
		dai_drv->ops = &tcc_asrc_ops;
	}
}

int tcc_asrc_dai_drvinit(struct platform_device *pdev)
{
	struct tcc_asrc_t *asrc = platform_get_drvdata(pdev);

	tcc_asrc_dai_driver_init(asrc);

	return snd_soc_register_component(&pdev->dev, &tcc_asrc_component_drv,
					  asrc->dai_drv, (int)NUM_OF_ASRC_PAIR);
}
