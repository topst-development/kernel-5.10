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
#include <linux/spinlock.h>
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

#include "tcc_adma_pcm.h"
#include "tcc_audio_daif.h"

#undef adma_pcm_dbg
#if 0
#define adma_pcm_dbg(a...) \
	(void) pr_info("[DEBUG][AUDIO_DMA]" a)
#else
#define adma_pcm_dbg(a...)
#endif
 #define adma_pcm_err(a...) \
	(void) pr_err("[ERROR][AUDIO_DMA]" a)

#define CHECK_ADMA_HW_PARAM_ELAPSED_TIME \
	(0)

#define DRV_NAME "tcc-adma-pcm"

struct tcc_adma_pcm_t {
	struct platform_device *pdev;
	spinlock_t lock;
	int32_t blk_no;
#if !defined(CONFIG_ARCH_TCC807X)
	struct clk *adma_hclk;
#endif
	void __iomem *adma_reg;
	struct adma_reg_t regs_backup; // for suspend/resume
 	uint32_t adma_irq;
	uint32_t have_hopcnt_clear_bit;
	uint32_t adrcnt_mode;
	struct snd_dma_buffer mono_play, mono_capture; // for dummy buffers
	struct {
		struct snd_pcm_substream *playback_substream;
		struct snd_pcm_substream *capture_substream;
	} dev[TCC_ADMA_MAX];
};

#define CFG_DAI_BURST_CYCLE \
	(TCC_ADMA_BURST_CYCLE_8)
#define CFG_SPDIF_TX_BURST_CYCLE \
	(TCC_ADMA_BURST_CYCLE_8)
#define CFG_SPDIF_RX_BURST_CYCLE \
	(TCC_ADMA_BURST_CYCLE_8)
#define CFG_CDIF_RX_BURST_CYCLE \
	(TCC_ADMA_BURST_CYCLE_4)

#define MAX_BUFFER_BYTES \
	(65536 * 4)

#define MIN_PERIOD_BYTES \
	(256)
#define MIN_PERIOD_CNT \
	(2)

#define PLAYBACK_MAX_PERIOD_BYTES \
	(65536)
#define CAPTURE_MAX_PERIOD_BYTES \
	(16384)

struct tcc_adma_pcm_hw_t {
	struct snd_pcm_hardware play;
	struct snd_pcm_hardware capture;
};

static const struct tcc_adma_pcm_hw_t tcc_adma_hw[TCC_ADMA_MAX] = {
	[TCC_ADMA_I2S_STEREO] = {
		.play = {
			.info = ((uint32_t)SNDRV_PCM_INFO_MMAP
				| (uint32_t)SNDRV_PCM_INFO_MMAP_VALID
				| (uint32_t)SNDRV_PCM_INFO_INTERLEAVED
				| (uint32_t)SNDRV_PCM_INFO_BLOCK_TRANSFER
				| (uint32_t)SNDRV_PCM_INFO_PAUSE
				| (uint32_t)SNDRV_PCM_INFO_RESUME),

			.formats = SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE,
			.rates        = (SNDRV_PCM_RATE_8000_192000
					| SNDRV_PCM_RATE_KNOT),
			.rate_min     = 8000,
			.rate_max     = 192000,
			.channels_min = 1,
			.channels_max = 16,
			.period_bytes_min = MIN_PERIOD_BYTES,
			.period_bytes_max = PLAYBACK_MAX_PERIOD_BYTES,
			.periods_min      = MIN_PERIOD_CNT,
			.periods_max      = MAX_BUFFER_BYTES/MIN_PERIOD_BYTES,
			.buffer_bytes_max = MAX_BUFFER_BYTES,
			.fifo_size = 16,
		},
		.capture = {
			.info = ((uint32_t)SNDRV_PCM_INFO_MMAP
				| (uint32_t)SNDRV_PCM_INFO_MMAP_VALID
				| (uint32_t)SNDRV_PCM_INFO_INTERLEAVED
				| (uint32_t)SNDRV_PCM_INFO_BLOCK_TRANSFER
				| (uint32_t)SNDRV_PCM_INFO_PAUSE
				| (uint32_t)SNDRV_PCM_INFO_RESUME),

			.formats      = SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE,
			.rates        = (SNDRV_PCM_RATE_8000_192000
					| SNDRV_PCM_RATE_KNOT),
			.rate_min     = 8000,
			.rate_max     = 192000,
			.channels_min = 1,
			.channels_max = 16,
			.period_bytes_min = MIN_PERIOD_BYTES,
			.period_bytes_max = CAPTURE_MAX_PERIOD_BYTES,
			.periods_min      = MIN_PERIOD_CNT,
			.periods_max      = MAX_BUFFER_BYTES/MIN_PERIOD_BYTES,
			.buffer_bytes_max = MAX_BUFFER_BYTES,
			.fifo_size = 16,
		},
	},
	[TCC_ADMA_I2S_7_1CH] = {
		.play = {
			.info = ((uint32_t)SNDRV_PCM_INFO_MMAP
				| (uint32_t)SNDRV_PCM_INFO_MMAP_VALID
				| (uint32_t)SNDRV_PCM_INFO_INTERLEAVED
				| (uint32_t)SNDRV_PCM_INFO_BLOCK_TRANSFER
				| (uint32_t)SNDRV_PCM_INFO_PAUSE
				| (uint32_t)SNDRV_PCM_INFO_RESUME),

			.formats = SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE,
			.rates        = (SNDRV_PCM_RATE_8000_192000
					| SNDRV_PCM_RATE_KNOT),

			.rate_min     = 8000,
			.rate_max     = 192000,
			.channels_min = 1,
			.channels_max = 16,
			.period_bytes_min = MIN_PERIOD_BYTES,
			.period_bytes_max = PLAYBACK_MAX_PERIOD_BYTES,
			.periods_min      = MIN_PERIOD_CNT,
			.periods_max      = MAX_BUFFER_BYTES/MIN_PERIOD_BYTES,
			.buffer_bytes_max = MAX_BUFFER_BYTES,
			.fifo_size = 16,
		},
		.capture = {
			.info = ((uint32_t)SNDRV_PCM_INFO_MMAP
				| (uint32_t)SNDRV_PCM_INFO_MMAP_VALID
				| (uint32_t)SNDRV_PCM_INFO_INTERLEAVED
				| (uint32_t)SNDRV_PCM_INFO_BLOCK_TRANSFER
				| (uint32_t)SNDRV_PCM_INFO_PAUSE
				| (uint32_t)SNDRV_PCM_INFO_RESUME),

			.formats      = SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE,
			.rates        = (SNDRV_PCM_RATE_8000_192000
					| SNDRV_PCM_RATE_KNOT),
			.rate_min     = 8000,
			.rate_max     = 192000,
			.channels_min = 1,
			.channels_max = 16,
			.period_bytes_min = MIN_PERIOD_BYTES,
			.period_bytes_max = CAPTURE_MAX_PERIOD_BYTES,
			.periods_min      = MIN_PERIOD_CNT,
			.periods_max      = MAX_BUFFER_BYTES/MIN_PERIOD_BYTES,
			.buffer_bytes_max = MAX_BUFFER_BYTES,
			.fifo_size = 16,
		},
	},
	[TCC_ADMA_SPDIF] = {
		.play = {
			.info = ((uint32_t)SNDRV_PCM_INFO_MMAP
				| (uint32_t)SNDRV_PCM_INFO_MMAP_VALID
				| (uint32_t)SNDRV_PCM_INFO_INTERLEAVED
				| (uint32_t)SNDRV_PCM_INFO_BLOCK_TRANSFER
				| (uint32_t)SNDRV_PCM_INFO_PAUSE
				| (uint32_t)SNDRV_PCM_INFO_RESUME),

			.formats = SNDRV_PCM_FMTBIT_U16_LE
				| SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE,
			.rates        = SNDRV_PCM_RATE_8000_48000,
			.rate_min     = 8000,
			.rate_max     = 48000,
			.channels_min = 2,
			.channels_max = 8,
			.period_bytes_min = MIN_PERIOD_BYTES,
			.period_bytes_max = PLAYBACK_MAX_PERIOD_BYTES,
			.periods_min      = MIN_PERIOD_CNT,
			.periods_max      = MAX_BUFFER_BYTES/MIN_PERIOD_BYTES,
			.buffer_bytes_max = MAX_BUFFER_BYTES,
			.fifo_size = 16,
		},
		.capture = {
			.info = ((uint32_t)SNDRV_PCM_INFO_MMAP
				| (uint32_t)SNDRV_PCM_INFO_MMAP_VALID
				| (uint32_t)SNDRV_PCM_INFO_INTERLEAVED
				| (uint32_t)SNDRV_PCM_INFO_BLOCK_TRANSFER
				| (uint32_t)SNDRV_PCM_INFO_PAUSE
				| (uint32_t)SNDRV_PCM_INFO_RESUME),

			.formats      = SNDRV_PCM_FMTBIT_U16_LE
				| SNDRV_PCM_FMTBIT_S16_LE
				| SNDRV_PCM_FMTBIT_S24_LE
				| SNDRV_PCM_FMTBIT_S32_LE,
			.rates        = SNDRV_PCM_RATE_8000_48000,
			.rate_min     = 8000,
			.rate_max     = 48000,
			.channels_min = 2,
			.channels_max = 2,
			.period_bytes_min = MIN_PERIOD_BYTES,
			.period_bytes_max = CAPTURE_MAX_PERIOD_BYTES,
			.periods_min      = MIN_PERIOD_CNT,
			.periods_max      = MAX_BUFFER_BYTES/MIN_PERIOD_BYTES,
			.buffer_bytes_max = MAX_BUFFER_BYTES,
			.fifo_size = 16,
		},
	},
};

static struct snd_soc_dai *tcc_adma_get_be_with_sink_path(const struct snd_soc_dapm_widget *tcc_dapm_widget)
{
	const struct snd_soc_dapm_path *tcc_dapm_path = NULL;
	struct snd_soc_dai *ret = NULL;

	snd_soc_dapm_widget_for_each_sink_path(tcc_dapm_widget, tcc_dapm_path) {
		if (tcc_dapm_path->connect == 0u){
			continue;
		}

		if (tcc_dapm_path->sink->id == snd_soc_dapm_dai_in){
			ret = (struct snd_soc_dai *)tcc_dapm_path->sink->priv;
		}
	}

	return ret;
}

static struct snd_soc_dai *tcc_adma_get_be_with_source_path(const struct snd_soc_dapm_widget *tcc_dapm_widget)
{
	const struct snd_soc_dapm_path *tcc_dapm_path = NULL;
	struct snd_soc_dai *ret = NULL;

	if(tcc_dapm_widget != NULL){
		snd_soc_dapm_widget_for_each_source_path(tcc_dapm_widget, tcc_dapm_path) {
			if (tcc_dapm_path->connect == 0u){
				continue;
			}

			if (tcc_dapm_path->source->id == snd_soc_dapm_dai_out){
				ret = (struct snd_soc_dai *)tcc_dapm_path->source->priv;
				break;
			}
		}
	}

	return ret;
}

static const unsigned int tcc_rates[] = {
	8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000, 64000,
	88200, 96000, 176400, 192000
};

static struct snd_pcm_hw_constraint_list tcc_constraints_rates = {
	.count = ARRAY_SIZE(tcc_rates),
	.list = tcc_rates,
	.mask = 0,
};

static int tcc_adma_pcm_open(
	struct snd_soc_component *component,
	struct snd_pcm_substream *substream)
{
	const struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)snd_soc_component_get_drvdata(component);
	int ret = 0;
	enum TCC_ADMA_DEV_TYPE dev_type;
	struct snd_soc_dai *dai = NULL;
	const struct tcc_dai_t *tcc_dai = NULL;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);

	adma_pcm_dbg("[%d] %s\n", adma_pcm->blk_no, __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dai = tcc_adma_get_be_with_sink_path(cpu_dai->playback_widget);
			if(dai == NULL){
				//adma_pcm_err("[%d] %s playback cannot find widget\n", adma_pcm->blk_no, __func__);
				ret = -EINVAL;
			}else{
				tcc_dai = (struct tcc_dai_t *)snd_soc_dai_get_drvdata(dai);
			}
	}else{
			dai =tcc_adma_get_be_with_source_path(cpu_dai->capture_widget);
			if(dai == NULL){
				//adma_pcm_err("[%d] %s capture cannot find widget\n", adma_pcm->blk_no, __func__);
				ret = -EINVAL;
			}else{
				tcc_dai = (struct tcc_dai_t *)snd_soc_dai_get_drvdata(dai);
			}
	}

	if(ret == 0){
		if(rtd->dai_link->id == 1){
			dev_type = TCC_ADMA_SPDIF;
		}else{
			dev_type = ((tcc_dai->i2s.block_type == DAI_BLOCK_7_1CH_TYPE)? TCC_ADMA_I2S_7_1CH : TCC_ADMA_I2S_STEREO);
		}

		(void) snd_pcm_hw_constraint_list(
				substream->runtime,
				0,
				SNDRV_PCM_HW_PARAM_RATE,
				&tcc_constraints_rates);

		if (adma_pcm->adrcnt_mode != 0u) {
			(void) snd_pcm_hw_constraint_step(
					substream->runtime,
					0,
					SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
					32);
		} else {
			(void) snd_pcm_hw_constraint_pow2(
					substream->runtime,
					0,
					SNDRV_PCM_HW_PARAM_BUFFER_BYTES);
		}
		(void) snd_pcm_hw_constraint_step(
				substream->runtime,
				0,
				SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
				32);

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			(void) snd_soc_set_runtime_hwparams(
					substream,
					&tcc_adma_hw[dev_type].play);
			adma_pcm->dev[dev_type].playback_substream =
				substream;
		} else {
			(void) snd_soc_set_runtime_hwparams(
					substream,
					&tcc_adma_hw[dev_type].capture);
			adma_pcm->dev[dev_type].capture_substream =
				substream;
		}
	}

	return ret;
}

static int tcc_adma_pcm_close(
	struct snd_soc_component *component,
	struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd =
		(struct snd_soc_pcm_runtime *)substream->private_data;
	//struct snd_soc_component *component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)snd_soc_component_get_drvdata(
		component);
	int ret = 0;
	enum TCC_ADMA_DEV_TYPE dev_type;
	struct snd_soc_dai *dai = NULL;
	const struct tcc_dai_t *tcc_dai = NULL;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);

	adma_pcm_dbg("[%d] %s\n", adma_pcm->blk_no, __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dai = tcc_adma_get_be_with_sink_path(cpu_dai->playback_widget);
			if(dai == NULL){
				//adma_pcm_err("[%d] %s playback cannot find widget\n", adma_pcm->blk_no, __func__);
				ret = -EINVAL;
			}else{
				tcc_dai = (struct tcc_dai_t *)snd_soc_dai_get_drvdata(dai);
			}
	}else{
			dai =tcc_adma_get_be_with_source_path(cpu_dai->capture_widget);
			if(dai == NULL){
				//adma_pcm_err("[%d] %s capture cannot find widget\n", adma_pcm->blk_no, __func__);
				ret = -EINVAL;
			}else{
				tcc_dai = (struct tcc_dai_t *)snd_soc_dai_get_drvdata(dai);
			}
	}

	if(ret == 0){
		if(rtd->dai_link->id == 1){
			dev_type = TCC_ADMA_SPDIF;
		}else{
			dev_type = ((tcc_dai->i2s.block_type == DAI_BLOCK_7_1CH_TYPE)? TCC_ADMA_I2S_7_1CH : TCC_ADMA_I2S_STEREO);
		}

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			switch (dev_type) {
				case TCC_ADMA_I2S_STEREO:
				case TCC_ADMA_I2S_7_1CH:
					tcc_adma_dai_tx_reset_enable(
							adma_pcm->adma_reg,
							FALSE);
					break;
				case TCC_ADMA_SPDIF:
				default:
					tcc_adma_spdif_tx_reset_enable(
							adma_pcm->adma_reg,
							FALSE);
					break;
#if 0
				default:
					ret = -EINVAL;
					break;
#endif
			}
		} else {
			switch (dev_type) {
				case TCC_ADMA_I2S_STEREO:
				case TCC_ADMA_I2S_7_1CH:
					tcc_adma_dai_rx_reset_enable(
							adma_pcm->adma_reg,
							FALSE);
					break;
				case TCC_ADMA_SPDIF:
				default:
					tcc_adma_spdif_rx_reset_enable(
							adma_pcm->adma_reg,
							FALSE);
					break;
#if 0
				default:
					ret = -EINVAL;
					break;
#endif
			}
		}

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			adma_pcm->dev[dev_type].playback_substream = NULL;
		} else {
			adma_pcm->dev[dev_type].capture_substream = NULL;
		}
	}
	return ret;
}

static int tcc_adma_pcm_mmap(
	struct snd_soc_component *component,
	struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	const struct snd_pcm_runtime *runtime = substream->runtime;
	
	unused(component);
	adma_pcm_dbg(" %s\n", __func__);

	return dma_mmap_wc(
		substream->pcm->card->dev,
		vma,
		runtime->dma_area,
		runtime->dma_addr,
		runtime->dma_bytes);
}

static uint32_t tcc_adma_get_dbth_value(
	uint32_t channels,
	enum TCC_ADMA_DEV_TYPE dev_type,
	uint32_t burst_size,
	bool tdm_mode)
{
	//tdm, mono, 2ch, 4ch, 6ch, 8ch, 10ch
	const uint32_t dbth_tbl_2ch[2][7] = {
		{0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01}, // burst_4
		{0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01}, // burst_8
	};
	const uint32_t dbth_tbl_7_1ch[2][7] = {
		{0x0f, 0x07, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f}, // burst_4
		{0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07}, // burst_8
	};
	uint32_t ch_idx, burst_idx;

	ch_idx = (tdm_mode) ? 0u : ((channels / 2u) + 1u);
	burst_idx = (burst_size == TCC_ADMA_BURST_CYCLE_4) ? 0u : 1u;

	return (dev_type == TCC_ADMA_I2S_7_1CH) ?
			dbth_tbl_7_1ch[burst_idx][ch_idx] :
			dbth_tbl_2ch[burst_idx][ch_idx];
}

static int tcc_adma_i2s_pcm_hw_params(
		struct snd_soc_component *component,
		const struct snd_pcm_substream *substream,
		const struct snd_pcm_hw_params *params,
		enum TCC_ADMA_DEV_TYPE dev_type,
		bool tdm_mode)
{
	const struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)snd_soc_component_get_drvdata(
		component);

	uint32_t channels = params_channels(params);
	uint32_t period_bytes = params_period_bytes(params);
	uint32_t buffer_bytes = params_buffer_bytes(params);
	snd_pcm_format_t format = params_format(params);
	bool mono_mode = (channels == 1u) ? TRUE : FALSE;
	bool multi_ch;
	uint32_t dbth;
	int ret = 0;
	bool adrcnt_mode;
	struct tcc_audio_dma_t tcc_audio_dma;

	enum TCC_ADMA_DATA_WIDTH data_width;
	enum TCC_ADMA_MULTI_CH_MODE multi_mode;

#if	(CHECK_ADMA_HW_PARAM_ELAPSED_TIME == 1)
	struct timeval start, end;
	u64 elapsed_usecs64;
	unsigned int elapsed_usecs;
#endif

	adma_pcm_dbg("[%d] %s\n", adma_pcm->blk_no, __func__);
	adma_pcm_dbg("[%d] format : 0x%08x\n", adma_pcm->blk_no, format);
	adma_pcm_dbg("[%d] channels : 0x%08x\n", adma_pcm->blk_no, channels);
	adma_pcm_dbg("[%d] period_bytes : %u\n", adma_pcm->blk_no, period_bytes);
	adma_pcm_dbg("[%d] buffer_bytes : %u\n", adma_pcm->blk_no, buffer_bytes);

#if	(CHECK_ADMA_HW_PARAM_ELAPSED_TIME == 1)
	do_gettimeofday(&start);
#endif

	data_width =
		(format == SNDRV_PCM_FORMAT_S32_LE) ? TCC_ADMA_DATA_WIDTH_32 :
		(format == SNDRV_PCM_FORMAT_S24_LE) ? TCC_ADMA_DATA_WIDTH_24 :
		TCC_ADMA_DATA_WIDTH_16;

	(void) memset(substream->dma_buffer.area, 0, buffer_bytes);

	adrcnt_mode = (adma_pcm->adrcnt_mode > 0u)? TRUE : FALSE;

	multi_ch = (channels > 2u) ? TRUE : FALSE;
	multi_mode = TCC_ADMA_MULTI_CH_MODE_7_1;

	dbth = tcc_adma_get_dbth_value(channels, dev_type,
		CFG_DAI_BURST_CYCLE, tdm_mode);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		tcc_audio_dma.base_addr = adma_pcm->adma_reg;
		tcc_audio_dma.dma_addr = substream->dma_buffer.addr;
		tcc_audio_dma.mono_dma_addr = adma_pcm->mono_play.addr;
		tcc_audio_dma.period_bytes = period_bytes;
		tcc_audio_dma.buffer_bytes = buffer_bytes;

		ret = tcc_adma_set_dai_tx_dma_buffer(
			&tcc_audio_dma,
			data_width,
			(uint32_t)CFG_DAI_BURST_CYCLE,
			mono_mode,
			adrcnt_mode);
	} else {
		tcc_audio_dma.base_addr = adma_pcm->adma_reg;
		tcc_audio_dma.dma_addr = substream->dma_buffer.addr;
		tcc_audio_dma.mono_dma_addr = adma_pcm->mono_capture.addr;
		tcc_audio_dma.period_bytes = period_bytes;
		tcc_audio_dma.buffer_bytes = buffer_bytes;

		ret = tcc_adma_set_dai_rx_dma_buffer(
			&tcc_audio_dma,
			data_width,
			(uint32_t)CFG_DAI_BURST_CYCLE,
			mono_mode,
			adrcnt_mode);
	}

	if (ret < 0) {
		adma_pcm_dbg("[%d] set dma buffer : fail\n", adma_pcm->blk_no);
	} else {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			tcc_adma_set_dai_tx_multi_ch(
					adma_pcm->adma_reg,
					multi_ch,
					multi_mode);
		} else {
			tcc_adma_set_dai_rx_multi_ch(
					adma_pcm->adma_reg,
					multi_ch,
					multi_mode);
		}

		tcc_adma_dai_threshold(adma_pcm->adma_reg, dbth);
	}
#if	(CHECK_ADMA_HW_PARAM_ELAPSED_TIME == 1)
	do_gettimeofday(&end);

	elapsed_usecs64 = timeval_to_ns(&end) - timeval_to_ns(&start);
	do_div(elapsed_usecs64, NSEC_PER_USEC);
	elapsed_usecs = elapsed_usecs64;

	adma_pcm_dbg("[%d] adma hw_params's elapsed time : %03d usec\n",
			adma_pcm->blk_no,
			elapsed_usecs);
#endif

	return ret;
}

static int tcc_adma_spdif_pcm_hw_params(
	struct snd_soc_component *component,
	const struct snd_pcm_substream *substream,
	const struct snd_pcm_hw_params *params)
{
	const struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)snd_soc_component_get_drvdata(
		component);

	uint32_t period_bytes = params_period_bytes(params);
	uint32_t buffer_bytes = params_buffer_bytes(params);
	snd_pcm_format_t format = params_format(params);
	enum TCC_ADMA_DATA_WIDTH data_width;
	int ret;
	bool adrcnt_mode;
	struct tcc_audio_dma_t  tcc_audio_dma;

	adma_pcm_dbg("[%d] %s\n", adma_pcm->blk_no, __func__);
	adma_pcm_dbg("[%d] format : 0x%08x\n", adma_pcm->blk_no, format);
	adma_pcm_dbg("[%d] period_bytes : %u\n", adma_pcm->blk_no, period_bytes);
	adma_pcm_dbg("[%d] buffer_bytes : %u\n", adma_pcm->blk_no, buffer_bytes);

	data_width =
		(format == SNDRV_PCM_FORMAT_S32_LE) ? TCC_ADMA_DATA_WIDTH_32 :
		(format == SNDRV_PCM_FORMAT_S24_LE) ? TCC_ADMA_DATA_WIDTH_24 :
		TCC_ADMA_DATA_WIDTH_16;

	(void) memset(substream->dma_buffer.area, 0, buffer_bytes);

	adrcnt_mode = (adma_pcm->adrcnt_mode > 0u)? TRUE : FALSE;

	tcc_audio_dma.base_addr = adma_pcm->adma_reg;
	tcc_audio_dma.dma_addr = substream->dma_buffer.addr;
	tcc_audio_dma.buffer_bytes = buffer_bytes;
	tcc_audio_dma.period_bytes = period_bytes;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = tcc_adma_set_spdif_tx_dma_buffer(
			&tcc_audio_dma,
			data_width,
			CFG_SPDIF_TX_BURST_CYCLE,
			adrcnt_mode);
	} else {
		tcc_adma_set_spdif_rx_path(
			adma_pcm->adma_reg,
			TCC_ADMA_SPDIF_CDIF_SEL_SPDIF);

		ret = tcc_adma_set_spdif_rx_dma_buffer(
			&tcc_audio_dma,
			data_width,
			(uint32_t)CFG_SPDIF_RX_BURST_CYCLE,
			adrcnt_mode);
	}

	return ret;
}

static int tcc_adma_pcm_hw_params(
	struct snd_soc_component *component,
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	const struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)snd_soc_component_get_drvdata(
		component);
	int ret = 0;
	unsigned long flags;
	enum TCC_ADMA_DEV_TYPE dev_type;
	bool tdm_mode = FALSE;
	struct snd_soc_dai *dai = NULL;
	const struct tcc_dai_t *tcc_dai = NULL;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);	

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dai = tcc_adma_get_be_with_sink_path(cpu_dai->playback_widget);
			if(dai == NULL){
				adma_pcm_err("[%d] %s playback cannot find widget\n", adma_pcm->blk_no, __func__);
				ret = -EINVAL;
			}else{
				tcc_dai = (struct tcc_dai_t *)snd_soc_dai_get_drvdata(dai);
			}
	}else{
			dai =tcc_adma_get_be_with_source_path(cpu_dai->capture_widget);
			if(dai == NULL){
				adma_pcm_err("[%d] %s capture cannot find widget\n", adma_pcm->blk_no, __func__);
				ret = -EINVAL;
			}else{
				tcc_dai = (struct tcc_dai_t *)snd_soc_dai_get_drvdata(dai);
			}
	}

	if(ret == 0){
		tdm_mode = tcc_dai->i2s.tdm_mode;
		if(rtd->dai_link->id == 1){
			dev_type = TCC_ADMA_SPDIF;
		}else{
			dev_type = ((tcc_dai->i2s.block_type == DAI_BLOCK_7_1CH_TYPE)? TCC_ADMA_I2S_7_1CH : TCC_ADMA_I2S_STEREO);
		}

		spin_lock_irqsave(&adma_pcm->lock, flags);

		switch (dev_type) {
			case TCC_ADMA_I2S_STEREO:
			case TCC_ADMA_I2S_7_1CH:
				ret = tcc_adma_i2s_pcm_hw_params(
						component,
						substream,
						params,
						dev_type,
						tdm_mode);
				break;
			case TCC_ADMA_SPDIF:
			default:
				ret = tcc_adma_spdif_pcm_hw_params(component, substream, params);
				break;
		}

		spin_unlock_irqrestore(&adma_pcm->lock, flags);

		tcc_adma_set_tx_dma_repeat_type(
				adma_pcm->adma_reg,
				TCC_ADMA_REPEAT_FROM_CUR_ADDR);
		tcc_adma_set_rx_dma_repeat_type(
				adma_pcm->adma_reg,
				TCC_ADMA_REPEAT_FROM_CUR_ADDR);

		tcc_adma_repeat_infinite_mode(adma_pcm->adma_reg);

		snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	}
	return ret;
}

static int tcc_adma_i2s_pcm_hw_free(
	struct snd_soc_component *component,
	const struct snd_pcm_substream *substream)
{
	const struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)snd_soc_component_get_drvdata(
		component);
	int ret = 0;

	adma_pcm_dbg("[%d] %s\n", adma_pcm->blk_no, __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		bool txdma_en = FALSE;
		adma_pcm_dbg("[%d] DAI_HW_FREE, PLAY\n", adma_pcm->blk_no);
		tcc_adma_dai_tx_irq_enable(adma_pcm->adma_reg, FALSE);
		tcc_adma_dai_tx_dma_enable(adma_pcm->adma_reg, FALSE);

		txdma_en = tcc_adma_dai_tx_dma_enable_check(adma_pcm->adma_reg);
		if (txdma_en == TRUE) {
			adma_pcm_err("[%d] %s : fail to hw_free (adma tx status : %d) \n", adma_pcm->blk_no, __func__, ret);
			ret = -EAGAIN;
		} else {
			if (adma_pcm->have_hopcnt_clear_bit != 0u) {
				tcc_adma_dai_tx_hopcnt_clear(adma_pcm->adma_reg);
			}

			tcc_adma_dai_tx_reset_enable(adma_pcm->adma_reg, TRUE);
		}
	} else {
		bool rxdma_en = FALSE;
		adma_pcm_dbg("[%d] DAI_HW_FREE, CAPTURE\n", adma_pcm->blk_no);
		tcc_adma_dai_rx_irq_enable(adma_pcm->adma_reg, FALSE);
		tcc_adma_dai_rx_dma_enable(adma_pcm->adma_reg, FALSE);

		rxdma_en = tcc_adma_dai_rx_dma_enable_check(adma_pcm->adma_reg);
		if (rxdma_en == TRUE) {
			adma_pcm_err("[%d] %s : fail to hw_free (adma rx status : %d) \n", adma_pcm->blk_no, __func__, ret);
			ret = -EAGAIN;
		} else {
			if (adma_pcm->have_hopcnt_clear_bit != 0u) {
				tcc_adma_dai_rx_hopcnt_clear(adma_pcm->adma_reg);
			}
			tcc_adma_dai_rx_reset_enable(adma_pcm->adma_reg, TRUE);
		}
	}

	return ret;
}

static int tcc_adma_spdif_pcm_hw_free(
	struct snd_soc_component *component,
	const struct snd_pcm_substream *substream)
{
	const struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)snd_soc_component_get_drvdata(
		component);
	int ret = 0;

	adma_pcm_dbg("[%d] %s\n", adma_pcm->blk_no, __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		bool txdma_en = FALSE;
		adma_pcm_dbg("[%d] SPDIF_HW_FREE, PLAY\n", adma_pcm->blk_no);
		tcc_adma_spdif_tx_irq_enable(adma_pcm->adma_reg, FALSE);
		tcc_adma_spdif_tx_dma_enable(adma_pcm->adma_reg, FALSE);

		txdma_en = tcc_adma_spdif_tx_dma_enable_check(adma_pcm->adma_reg);
		if (txdma_en == TRUE) {
			adma_pcm_err(
					"[%d] %s : fail to hw_free (adma tx status : %d) \n",
					adma_pcm->blk_no, __func__, ret);
			ret = -EAGAIN;
		} else {
			if (adma_pcm->have_hopcnt_clear_bit != 0u) {
				tcc_adma_spdif_tx_hopcnt_clear(adma_pcm->adma_reg);
			}
			tcc_adma_spdif_tx_reset_enable(adma_pcm->adma_reg, TRUE);
		}
	} else {
		bool rxdma_en = FALSE;

		adma_pcm_dbg("[%d] SPDIF_HW_FREE, CAPTURE\n", adma_pcm->blk_no);
		tcc_adma_spdif_rx_irq_enable(adma_pcm->adma_reg, FALSE);
		tcc_adma_spdif_rx_dma_en(adma_pcm->adma_reg, FALSE);

		rxdma_en = tcc_adma_spdif_rx_dma_en_check(adma_pcm->adma_reg);
		if (rxdma_en == TRUE) {
			adma_pcm_err(
				"[%d] %s : fail to hw_free (adma rx status : %d) \n",
				adma_pcm->blk_no, __func__, ret);
			ret = -EAGAIN;
		} else {
			if (adma_pcm->have_hopcnt_clear_bit != 0u) {
				tcc_adma_spdif_rx_hopcnt_clear(adma_pcm->adma_reg);
			}
			tcc_adma_spdif_rx_reset_enable(adma_pcm->adma_reg, TRUE);
		}
	}

	return ret;
}


static int tcc_adma_pcm_hw_free(
	struct snd_soc_component *component,
	struct snd_pcm_substream *substream)
{
	const struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)snd_soc_component_get_drvdata(
		component);
	int ret = 0;
	unsigned long flags;
	enum TCC_ADMA_DEV_TYPE dev_type;
	struct snd_soc_dai *dai = NULL;
	const struct tcc_dai_t *tcc_dai = NULL;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);

	adma_pcm_dbg("[%d] %s\n", adma_pcm->blk_no, __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dai = tcc_adma_get_be_with_sink_path(cpu_dai->playback_widget);
			if(dai == NULL){
				//adma_pcm_err("[%d] %s playback cannot find widget\n", adma_pcm->blk_no, __func__);
				ret = -EINVAL;
			}else{
				tcc_dai = (struct tcc_dai_t *)snd_soc_dai_get_drvdata(dai);
			}
	}else{
			dai =tcc_adma_get_be_with_source_path(cpu_dai->capture_widget);
			if(dai == NULL){
				//adma_pcm_err("[%d] %s capture cannot find widget\n", adma_pcm->blk_no, __func__);
				ret = -EINVAL;
			}else{
				tcc_dai = (struct tcc_dai_t *)snd_soc_dai_get_drvdata(dai);
			}
	}

	if(ret == 0){
		if(rtd->dai_link->id == 1){
			dev_type = TCC_ADMA_SPDIF;
		}else{
			dev_type = ((tcc_dai->i2s.block_type == DAI_BLOCK_7_1CH_TYPE)? TCC_ADMA_I2S_7_1CH : TCC_ADMA_I2S_STEREO);
		}

		(void) memset(substream->dma_buffer.area, 0,
			substream->dma_buffer.bytes);
		snd_pcm_set_runtime_buffer(substream, NULL);

		spin_lock_irqsave(&adma_pcm->lock, flags);

		switch (dev_type) {
		case TCC_ADMA_I2S_STEREO:
		case TCC_ADMA_I2S_7_1CH:
			ret = tcc_adma_i2s_pcm_hw_free(component, substream);
			break;
		case TCC_ADMA_SPDIF:
		default:
			ret = tcc_adma_spdif_pcm_hw_free(component, substream);
			break;
		}

		spin_unlock_irqrestore(&adma_pcm->lock, flags);
	}

	return ret;
}

static int tcc_adma_i2s_pcm_prepare(
	struct snd_soc_component *component,
	const struct snd_pcm_substream *substream)
{
	const struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)snd_soc_component_get_drvdata(
		component);
	bool dma_enable = false;
	int ret = 0;

	adma_pcm_dbg("[%d] %s\n", adma_pcm->blk_no, __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		adma_pcm_dbg("[%d] DAI_PREPARE, PLAY\n", adma_pcm->blk_no);
		dma_enable = tcc_adma_dai_tx_dma_enable_check(adma_pcm->adma_reg);
		if (dma_enable == TRUE) {
			tcc_adma_dai_tx_irq_enable(adma_pcm->adma_reg, FALSE);
			tcc_adma_dai_tx_dma_enable(adma_pcm->adma_reg, FALSE);

			dma_enable = tcc_adma_dai_tx_dma_enable_check(adma_pcm->adma_reg);
			if (dma_enable == TRUE) {
				adma_pcm_err("[%d] %s : fail to Audio DMA stop (adma tx status : %d) \n", adma_pcm->blk_no, __func__, dma_enable);
				ret = -EAGAIN;
			} else {
				if (adma_pcm->have_hopcnt_clear_bit != 0u) {
					tcc_adma_dai_tx_hopcnt_clear(adma_pcm->adma_reg);
				}
			}
		}
	} else {
		adma_pcm_dbg("[%d] DAI_PREPARE, CAPTURE\n", adma_pcm->blk_no);
		dma_enable = tcc_adma_dai_rx_dma_enable_check(adma_pcm->adma_reg);
		if (dma_enable == TRUE) {
			tcc_adma_dai_rx_irq_enable(adma_pcm->adma_reg, FALSE);
			tcc_adma_dai_rx_dma_enable(adma_pcm->adma_reg, FALSE);

			dma_enable = tcc_adma_dai_rx_dma_enable_check(adma_pcm->adma_reg);
			if (dma_enable == TRUE) {
				adma_pcm_err("[%d] %s : fail to Audio DMA stop (adma rx status : %d) \n", adma_pcm->blk_no, __func__, dma_enable);
				ret = -EAGAIN;
			} else {
				if (adma_pcm->have_hopcnt_clear_bit != 0u) {
					tcc_adma_dai_rx_hopcnt_clear(adma_pcm->adma_reg);
				}
			}
		}
	}

	return ret;
}

static int tcc_adma_spdif_pcm_prepare(
	struct snd_soc_component *component,
	const struct snd_pcm_substream *substream)
{
	const struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)snd_soc_component_get_drvdata(
		component);
	bool dma_enable = false;
	int ret = 0;

	adma_pcm_dbg("[%d] %s\n", adma_pcm->blk_no, __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		adma_pcm_dbg("[%d] SPDIF_PREPARE, PLAY\n", adma_pcm->blk_no);
		dma_enable = tcc_adma_spdif_tx_dma_enable_check(adma_pcm->adma_reg);
		if (dma_enable == TRUE) {
			tcc_adma_spdif_tx_irq_enable(adma_pcm->adma_reg, FALSE);
			tcc_adma_spdif_tx_dma_enable(adma_pcm->adma_reg, FALSE);

			dma_enable = tcc_adma_spdif_tx_dma_enable_check(adma_pcm->adma_reg);
			if (dma_enable == TRUE) {
				adma_pcm_err(
				"[%d] %s : fail to Audio DMA stop (adma tx status : %d) \n",
				adma_pcm->blk_no, __func__, dma_enable);
				ret = -EAGAIN;
			} else {
				if (adma_pcm->have_hopcnt_clear_bit != 0u) {
					tcc_adma_spdif_tx_hopcnt_clear(adma_pcm->adma_reg);
				}
			}
		}

	} else {
		adma_pcm_dbg("[%d] SPDIF_PREPARE, CAPTURE\n", adma_pcm->blk_no);
		dma_enable = tcc_adma_spdif_rx_dma_en_check(adma_pcm->adma_reg);
		if (dma_enable == TRUE) {
			tcc_adma_spdif_rx_irq_enable(adma_pcm->adma_reg, FALSE);
			tcc_adma_spdif_rx_dma_en(adma_pcm->adma_reg, FALSE);

			dma_enable = tcc_adma_spdif_rx_dma_en_check(adma_pcm->adma_reg);
			if (dma_enable == TRUE) {
				adma_pcm_err(
				"[%d] %s : fail to Audio DMA stop (adma rx status : %d) \n",
				adma_pcm->blk_no, __func__, dma_enable);
				ret = -EAGAIN;
			} else {
				if (adma_pcm->have_hopcnt_clear_bit != 0u) {
					tcc_adma_spdif_rx_hopcnt_clear(adma_pcm->adma_reg);
				}
			}
		}
	}

	return ret;
}


static int tcc_adma_pcm_prepare(
	struct snd_soc_component *component,
	struct snd_pcm_substream *substream)
{
	const struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)snd_soc_component_get_drvdata(
		component);
	int ret = 0;
	unsigned long flags;
	enum TCC_ADMA_DEV_TYPE dev_type;
	struct snd_soc_dai *dai = NULL;
	const struct tcc_dai_t *tcc_dai = NULL;
	const struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);

	adma_pcm_dbg("[%d] %s\n", adma_pcm->blk_no, __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dai = tcc_adma_get_be_with_sink_path(cpu_dai->playback_widget);
			if(dai == NULL){
				//adma_pcm_err("[%d] %s playback cannot find widget\n", adma_pcm->blk_no, __func__);
				ret = -EINVAL;
			}else{
				tcc_dai = (struct tcc_dai_t *)snd_soc_dai_get_drvdata(dai);
			}
	}else{
			dai =tcc_adma_get_be_with_source_path(cpu_dai->capture_widget);
			if(dai == NULL){
				//adma_pcm_err("[%d] %s capture cannot find widget\n", adma_pcm->blk_no, __func__);
				ret = -EINVAL;
			}else{
				tcc_dai = (struct tcc_dai_t *)snd_soc_dai_get_drvdata(dai);
			}
	}

	if(ret == 0){
		if(rtd->dai_link->id == 1){
			dev_type = TCC_ADMA_SPDIF;
		}else{
			dev_type = ((tcc_dai->i2s.block_type == DAI_BLOCK_7_1CH_TYPE)? TCC_ADMA_I2S_7_1CH : TCC_ADMA_I2S_STEREO);
		}

		spin_lock_irqsave(&adma_pcm->lock, flags);

		adma_pcm_dbg("[%d] %s\n", adma_pcm->blk_no, __func__);
		switch (dev_type) {
		case TCC_ADMA_I2S_STEREO:
		case TCC_ADMA_I2S_7_1CH:
			ret = tcc_adma_i2s_pcm_prepare(component, substream);
			break;
		case TCC_ADMA_SPDIF:
		default:
			ret = tcc_adma_spdif_pcm_prepare(component, substream);
			break;
		}

		spin_unlock_irqrestore(&adma_pcm->lock, flags);
	}
	return ret;
}

static int tcc_adma_i2s_pcm_trigger(
	struct snd_soc_component *component,
	const struct snd_pcm_substream *substream,
	int cmd)
{
	const struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)snd_soc_component_get_drvdata(
		component);
	int ret = 0;

	adma_pcm_dbg("[%d] %s\n", adma_pcm->blk_no, __func__);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			adma_pcm_dbg("[%d] ADMA_TRIGGER_START, PLAY\n", adma_pcm->blk_no);
			tcc_adma_dai_tx_irq_enable(adma_pcm->adma_reg, TRUE);
			tcc_adma_dai_tx_dma_enable(adma_pcm->adma_reg, TRUE);
		} else {
			adma_pcm_dbg("[%d] ADMA_TRIGGER_START, CAPTURE\n", adma_pcm->blk_no);
			tcc_adma_dai_rx_irq_enable(adma_pcm->adma_reg, TRUE);
			tcc_adma_dai_rx_dma_enable(adma_pcm->adma_reg, TRUE);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			adma_pcm_dbg("[%d] ADMA_TRIGGER_STOP, PLAY\n", adma_pcm->blk_no);
		} else {
			adma_pcm_dbg("[%d] ADMA_TRIGGER_STOP, CAPTURE\n", adma_pcm->blk_no);
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int tcc_adma_spdif_pcm_trigger(
	struct snd_soc_component *component,
	const struct snd_pcm_substream *substream,
	int cmd)
{
	const struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)snd_soc_component_get_drvdata(
		component);
	int ret = 0;

	adma_pcm_dbg("[%d] %s\n", adma_pcm->blk_no, __func__);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			adma_pcm_dbg("[%d] SPDIF_TRIGGER_START, PLAY\n", adma_pcm->blk_no);
			tcc_adma_spdif_tx_irq_enable(adma_pcm->adma_reg, TRUE);
			tcc_adma_spdif_tx_dma_enable(adma_pcm->adma_reg, TRUE);
		} else {
			adma_pcm_dbg("[%d] SPDIF_TRIGGER_START, CAPTURE\n", adma_pcm->blk_no);
			tcc_adma_spdif_rx_irq_enable(
				adma_pcm->adma_reg,
				TRUE);
			tcc_adma_spdif_rx_dma_en(
				adma_pcm->adma_reg,
				TRUE);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int tcc_adma_pcm_trigger(
	struct snd_soc_component *component,
	struct snd_pcm_substream *substream,
	int cmd)
{
	const struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)snd_soc_component_get_drvdata(
		component);
	unsigned long flags;
	int ret = 0;
	enum TCC_ADMA_DEV_TYPE dev_type;
	struct snd_soc_dai *dai = NULL;
	const struct tcc_dai_t *tcc_dai = NULL;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);

	adma_pcm_dbg("[%d] %s\n", adma_pcm->blk_no, __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dai = tcc_adma_get_be_with_sink_path(cpu_dai->playback_widget);
			if(dai == NULL){
				//adma_pcm_err("[%d] %s playback cannot find widget\n", adma_pcm->blk_no, __func__);
				ret = -EINVAL;
			}else{
				tcc_dai = (struct tcc_dai_t *)snd_soc_dai_get_drvdata(dai);
			}
	}else{
			dai =tcc_adma_get_be_with_source_path(cpu_dai->capture_widget);
			if(dai == NULL){
				//adma_pcm_err("[%d] %s capture cannot find widget\n", adma_pcm->blk_no, __func__);
				ret = -EINVAL;
			}else{
				tcc_dai = (struct tcc_dai_t *)snd_soc_dai_get_drvdata(dai);
			}
	}

	if(ret == 0){
		if(rtd->dai_link->id == 1){
			dev_type = TCC_ADMA_SPDIF;
		}else{
			dev_type = ((tcc_dai->i2s.block_type == DAI_BLOCK_7_1CH_TYPE)? TCC_ADMA_I2S_7_1CH : TCC_ADMA_I2S_STEREO);
		}

		spin_lock_irqsave(&adma_pcm->lock, flags);

		switch (dev_type) {
			case TCC_ADMA_I2S_STEREO:
			case TCC_ADMA_I2S_7_1CH:
				ret = tcc_adma_i2s_pcm_trigger(component, substream, cmd);
				break;
			case TCC_ADMA_SPDIF:
			default:
				ret = tcc_adma_spdif_pcm_trigger(component, substream, cmd);
				break;

		}

		spin_unlock_irqrestore(&adma_pcm->lock, flags);
	}

	return ret;
}


static snd_pcm_uframes_t tcc_adma_i2s_pcm_pointer(
	struct snd_soc_component *component,
	const struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	const struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)snd_soc_component_get_drvdata(
		component);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	uint64_t dma_cur, base_cur;
#else
	uint32_t dma_cur, base_cur;
#endif
	snd_pcm_sframes_t frames = 0;
	snd_pcm_uframes_t hw_point = 0;

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	base_cur =  (uint64_t)runtime->dma_addr;
#else
	base_cur =  (uint32_t)(runtime->dma_addr & 0xffffffffu);
#endif

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (runtime->channels == 1u) {
			dma_cur = tcc_adma_dai_tx_get_cur_mono_dma_addr(adma_pcm->adma_reg);
		} else {
			dma_cur = tcc_adma_dai_tx_get_cur_dma_addr(adma_pcm->adma_reg);
		}
	} else {
		if (runtime->channels == 1u) {
			dma_cur = tcc_adma_dai_rx_get_cur_mono_dma_addr(adma_pcm->adma_reg);
		} else {
			dma_cur = tcc_adma_dai_rx_get_cur_dma_addr(adma_pcm->adma_reg);
		}
	}

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	frames = bytes_to_frames(runtime, ull_to_si(ull_sub(dma_cur, base_cur)));
#else
	frames = bytes_to_frames(runtime, ui_to_si(ui_sub(dma_cur, base_cur)));
#endif

	if (frames >= 0) {
		hw_point = (snd_pcm_uframes_t)frames;
	}

	return hw_point;
}

static snd_pcm_uframes_t tcc_adma_spdif_pcm_pointer(
	struct snd_soc_component *component,
	const struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	const struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)snd_soc_component_get_drvdata(
		component);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	uint64_t dma_cur, base_cur;
#else
	uint32_t dma_cur, base_cur;
#endif
	snd_pcm_sframes_t frames = 0;
	snd_pcm_uframes_t hw_point = 0;

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	base_cur =  (uint64_t)runtime->dma_addr;
#else
	base_cur =  (uint32_t)(runtime->dma_addr & 0xffffffffu);
#endif

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		dma_cur = tcc_adma_spdif_tx_get_cur_dma_addr(
			adma_pcm->adma_reg);
	} else {
		dma_cur = tcc_adma_spdif_rx_get_cur_dma_addr(
			adma_pcm->adma_reg);
	}
	
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	frames = bytes_to_frames(runtime, ull_to_si(ull_sub(dma_cur, base_cur)));
#else
	frames = bytes_to_frames(runtime, ui_to_si(ui_sub(dma_cur, base_cur)));
#endif

	if (frames >= 0) {
		hw_point = (snd_pcm_uframes_t)frames;
	}

	return hw_point;
}
#if 0
static snd_pcm_uframes_t tcc_adma_pcm_pointer(
	struct snd_soc_component *component,
	const struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	const struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)snd_soc_component_get_drvdata(
		component);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	uint64_t dma_cur, base_cur;
#else
	uint32_t dma_cur, base_cur;
#endif
	snd_pcm_sframes_t frames = 0;
	snd_pcm_uframes_t hw_point = 0;

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	base_cur =  (uint64_t)runtime->dma_addr;
#else
	base_cur =  (uint32_t)(runtime->dma_addr & 0xffffffffu);
#endif

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		dma_cur = 0u;
	} else {
		dma_cur = tcc_adma_spdif_rx_get_cur_dma_addr(adma_pcm->adma_reg);
	}

	frames = bytes_to_frames(runtime, (long)(dma_cur - base_cur));
	if (frames >= 0) {
		hw_point = (snd_pcm_uframes_t)frames;
	}

	return hw_point;
}
#endif

static snd_pcm_uframes_t tcc_adma_pcm_pointer(
	struct snd_soc_component *component,
	struct snd_pcm_substream *substream)
{
	const struct snd_soc_pcm_runtime *rtd = substream->private_data;
	snd_pcm_uframes_t ret = 0;
	enum TCC_ADMA_DEV_TYPE dev_type;
	int err = 0;
	struct snd_soc_dai *dai = NULL;
	const struct tcc_dai_t *tcc_dai = NULL;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	//const struct tcc_adma_pcm_t *adma_pcm =
	//	(struct tcc_adma_pcm_t *)snd_soc_component_get_drvdata(component);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			dai = tcc_adma_get_be_with_sink_path(cpu_dai->playback_widget);
			if(dai == NULL){
				//adma_pcm_err("[%d] %s playback cannot find widget\n", adma_pcm->blk_no, __func__);
				err = -EINVAL;
			}else{
				tcc_dai = (struct tcc_dai_t *)snd_soc_dai_get_drvdata(dai);
			}
	}else{
			dai =tcc_adma_get_be_with_source_path(cpu_dai->capture_widget);
			if(dai == NULL){
				//adma_pcm_err("[%d] %s capture cannot find widget\n", adma_pcm->blk_no, __func__);
				err = -EINVAL;
			}else{
				tcc_dai = (struct tcc_dai_t *)snd_soc_dai_get_drvdata(dai);
			}
	}

	if(err == 0){
		if(rtd->dai_link->id == 1){
			dev_type = TCC_ADMA_SPDIF;
		}else{
			dev_type = ((tcc_dai->i2s.block_type == DAI_BLOCK_7_1CH_TYPE)? TCC_ADMA_I2S_7_1CH : TCC_ADMA_I2S_STEREO);
		}

		switch (dev_type) {
			case TCC_ADMA_I2S_STEREO:
			case TCC_ADMA_I2S_7_1CH:
				ret = tcc_adma_i2s_pcm_pointer(component, substream);
				break;
			case TCC_ADMA_SPDIF:
			default:
				ret = tcc_adma_spdif_pcm_pointer(component, substream);
				break;
		}
	}
	return ret;
}



static irqreturn_t tcc_adma_pcm_handler(int irq, void *dev_id)
{
	const struct tcc_adma_pcm_t *adma_pcm = (struct tcc_adma_pcm_t *)dev_id;
	bool ret;
	//adma_pcm_dbg("%s\n", __func__);

	unused(irq);

	ret = tcc_adma_dai_tx_irq_check(adma_pcm->adma_reg);
	if (ret) {
		tcc_adma_dai_tx_irq_clear(adma_pcm->adma_reg);
		ret = tcc_adma_dai_tx_irq_check(adma_pcm->adma_reg);
		if (ret) {
			adma_pcm_err("[%d] %s:DAI TX IRQ NOT CLEAR!\n", adma_pcm->blk_no, __func__);
		}

		if (adma_pcm->dev[TCC_ADMA_I2S_STEREO].playback_substream
				!= NULL) {
			snd_pcm_period_elapsed(
			adma_pcm->dev[TCC_ADMA_I2S_STEREO].playback_substream);
		}

		if (adma_pcm->dev[TCC_ADMA_I2S_7_1CH].playback_substream
				!= NULL) {
			snd_pcm_period_elapsed(
			adma_pcm->dev[TCC_ADMA_I2S_7_1CH].playback_substream);
		}
	}
	ret = tcc_adma_dai_rx_irq_check(adma_pcm->adma_reg);
	if (ret) {
		tcc_adma_dai_rx_irq_clear(adma_pcm->adma_reg);
		ret = tcc_adma_dai_rx_irq_check(adma_pcm->adma_reg);
		if (ret) {
			adma_pcm_err("[%d] %s : DAI RX IRQ NOT CLEAR!\n", adma_pcm->blk_no, __func__);
		}

		if (adma_pcm->dev[TCC_ADMA_I2S_STEREO].capture_substream
				!= NULL) {
			snd_pcm_period_elapsed(
			adma_pcm->dev[TCC_ADMA_I2S_STEREO].capture_substream);
		}

		if (adma_pcm->dev[TCC_ADMA_I2S_7_1CH].capture_substream
				!= NULL) {
			snd_pcm_period_elapsed(
			adma_pcm->dev[TCC_ADMA_I2S_7_1CH].capture_substream);
		}
	}

	ret = tcc_adma_spdif_tx_irq_check(adma_pcm->adma_reg);
	if (ret) {
		tcc_adma_spdif_tx_irq_clear(adma_pcm->adma_reg);
		ret = tcc_adma_spdif_tx_irq_check(adma_pcm->adma_reg);
		if (ret) {
			adma_pcm_err("[%d] %s : SPDIF TX IRQ NOT CLEAR!\n",
				adma_pcm->blk_no,
				__func__);
		}
		if (adma_pcm->dev[TCC_ADMA_SPDIF].playback_substream != NULL) {
			snd_pcm_period_elapsed(
			adma_pcm->dev[TCC_ADMA_SPDIF].playback_substream);
		}
	}

	ret = tcc_adma_spdif_rx_irq_check(adma_pcm->adma_reg);
	if (ret) {
		tcc_adma_spdif_rx_irq_clear(adma_pcm->adma_reg);
		ret = tcc_adma_spdif_rx_irq_check(adma_pcm->adma_reg);
		if (adma_pcm->dev[TCC_ADMA_SPDIF].capture_substream != NULL) {
			if (ret) {
				adma_pcm_err("[%d] %s : SPDIF RX IRQ NOT	CLEAR!\n",
					adma_pcm->blk_no,
					__func__);
			}
			snd_pcm_period_elapsed(
			adma_pcm->dev[TCC_ADMA_SPDIF].capture_substream);
		}
	}

	return IRQ_HANDLED;
}

static int tcc_adma_pcm_new(
	struct snd_soc_component *component,
	struct snd_soc_pcm_runtime *rtd)
{
	const struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm	= rtd->pcm;
	struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)snd_soc_component_get_drvdata(
		component);
	u64 bit_mask =	(1ULL<<32)-1ULL;
	int ret = 0;

	ret = dma_coerce_mask_and_coherent(card->dev, bit_mask);
	if (ret >= 0) {
		ret = snd_dma_alloc_pages(
				SNDRV_DMA_TYPE_DEV,
				card->dev,
				MAX_BUFFER_BYTES,
				&adma_pcm->mono_play);
	}

	if (ret >= 0) {
		ret = snd_dma_alloc_pages(
				SNDRV_DMA_TYPE_DEV,
				card->dev,
				MAX_BUFFER_BYTES,
				&adma_pcm->mono_capture);

		if (ret != 0) {
			snd_dma_free_pages(&adma_pcm->mono_play);
		} else {
			(void) snd_pcm_lib_preallocate_pages_for_all(
					pcm,
					SNDRV_DMA_TYPE_DEV,
					card->dev,
					MAX_BUFFER_BYTES,
					MAX_BUFFER_BYTES);
		}
	}

	return ret;
}

static void tcc_adma_pcm_free_dma_buffers(
	struct snd_soc_component *component,
	struct snd_pcm *pcm)
{
	struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)snd_soc_component_get_drvdata(
		component);

	(void) snd_pcm_lib_preallocate_free_for_all(pcm);

	snd_dma_free_pages(&adma_pcm->mono_play);
	snd_dma_free_pages(&adma_pcm->mono_capture);
}

int tcc_adma_pcm_ioctl(
	struct snd_soc_component *component,
	struct snd_pcm_substream *substream,
	unsigned int cmd, void *arg)
{
	int ret;
	unused(component);
	ret = snd_pcm_lib_ioctl(substream, cmd, arg);

	return ret;
}

static const struct snd_soc_component_driver tcc_adma_pcm_component = {
	.name	  = DRV_NAME,
	.pcm_construct = tcc_adma_pcm_new,
	.pcm_destruct = tcc_adma_pcm_free_dma_buffers,

	.open  = tcc_adma_pcm_open,
	.close  = tcc_adma_pcm_close,
	.ioctl  = tcc_adma_pcm_ioctl,
	.hw_params = tcc_adma_pcm_hw_params,
	.hw_free = tcc_adma_pcm_hw_free,
	.prepare = tcc_adma_pcm_prepare,
	.trigger = tcc_adma_pcm_trigger,
	.pointer = tcc_adma_pcm_pointer,
	.mmap = tcc_adma_pcm_mmap,
};

static int parse_pcm_dt(
	struct platform_device *pdev,
	struct tcc_adma_pcm_t *adma_pcm)
{
	int32_t ret = 0;

	adma_pcm->pdev = pdev;

	adma_pcm->blk_no = of_alias_get_id(pdev->dev.of_node, "adma");

	adma_pcm->adma_reg = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR(adma_pcm->adma_reg)) {
		adma_pcm->adma_reg = NULL;
	} else {
		adma_pcm_dbg("[%d] adma_reg=%p\n", adma_pcm->blk_no, adma_pcm->adma_reg);
	}

	ret = platform_get_irq(pdev, 0);
	if(ret >= 0){
		adma_pcm->adma_irq = (uint32_t) ret;
		ret = 0;
	}

#if !defined(CONFIG_ARCH_TCC807X)
	adma_pcm->adma_hclk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(adma_pcm->adma_hclk)) {
		ret = -EINVAL;
	} else
#endif
	{
		(void) of_property_read_u32(
				pdev->dev.of_node,
				"have-hopcnt-clear",
				&adma_pcm->have_hopcnt_clear_bit);
		adma_pcm_dbg("[%d] have_hopcnt_clear_bit : %u\n",
				adma_pcm->blk_no,
				adma_pcm->have_hopcnt_clear_bit);

		(void) of_property_read_u32(
				pdev->dev.of_node,
				"adrcnt-mode",
				&adma_pcm->adrcnt_mode);
		adma_pcm_dbg("[%d] adrcnt_mode : %u\n",
				adma_pcm->blk_no, adma_pcm->adrcnt_mode);
	}

	return ret;
}

#if defined(CONFIG_PM)
static int tcc_adma_pcm_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)platform_get_drvdata(pdev);

	adma_pcm_dbg("[%d] %s\n", adma_pcm->blk_no, __func__);

	tcc_adma_reg_backup(adma_pcm->adma_reg, &adma_pcm->regs_backup);
	return 0;
}

static int tcc_adma_pcm_resume(struct platform_device *pdev)
{
	struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)platform_get_drvdata(pdev);

	adma_pcm_dbg("[%d] %s\n", adma_pcm->blk_no, __func__);

	tcc_adma_reg_restore(adma_pcm->adma_reg, &adma_pcm->regs_backup);
	return 0;
}
#endif

#define TCC_SOC_ADMA_DAI_DRV(NAME, SNAME_PLAYBACK, SNAME_CAPTURE) \
	{	\
		.name = NAME,	\
		.playback = {	\
			.stream_name = SNAME_PLAYBACK,	\
			.channels_min = 1,	\
			.channels_max = 16,	\
			.rate_min = 8000, \
			.rate_max = 192000, \
			.rates = (SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT),	\
			.formats = (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE),	\
		},	\
		.capture = {	\
			.stream_name = SNAME_CAPTURE,	\
			.channels_min = 1,	\
			.channels_max = 16,	\
			.rate_min = 8000, \
			.rate_max = 192000, \
			.rates = (SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT),	\
			.formats = (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE),	\
		},	\
	}

static struct snd_soc_dai_driver tcc_adma_pcm_dai_drv[8][2] = {
	{
		TCC_SOC_ADMA_DAI_DRV(TCC_ADMA0_DAI, ADMA0_PLAYBACK_WIDGET, ADMA0_CAPTURE_WIDGET),
		TCC_SOC_ADMA_DAI_DRV(TCC_ADMA0_SPDIF, ADMA0_SPDIF_PLAYBACK_WIDGET, ADMA0_SPDIF_CAPTURE_WIDGET)
	},
	{
		TCC_SOC_ADMA_DAI_DRV(TCC_ADMA1_DAI, ADMA1_PLAYBACK_WIDGET, ADMA1_CAPTURE_WIDGET),
		TCC_SOC_ADMA_DAI_DRV(TCC_ADMA1_SPDIF, ADMA1_SPDIF_PLAYBACK_WIDGET, ADMA1_SPDIF_CAPTURE_WIDGET),
	},
	{
		TCC_SOC_ADMA_DAI_DRV(TCC_ADMA2_DAI, ADMA2_PLAYBACK_WIDGET, ADMA2_CAPTURE_WIDGET),
		TCC_SOC_ADMA_DAI_DRV(TCC_ADMA2_SPDIF, ADMA2_SPDIF_PLAYBACK_WIDGET, ADMA2_SPDIF_CAPTURE_WIDGET),
	},
	{
		TCC_SOC_ADMA_DAI_DRV(TCC_ADMA3_DAI, ADMA3_PLAYBACK_WIDGET, ADMA3_CAPTURE_WIDGET),
		TCC_SOC_ADMA_DAI_DRV(TCC_ADMA3_SPDIF, ADMA3_SPDIF_PLAYBACK_WIDGET, ADMA3_SPDIF_CAPTURE_WIDGET),
	},
	{
		TCC_SOC_ADMA_DAI_DRV(TCC_ADMA4_DAI, ADMA4_PLAYBACK_WIDGET, ADMA4_CAPTURE_WIDGET),
		TCC_SOC_ADMA_DAI_DRV(TCC_ADMA4_SPDIF, ADMA4_SPDIF_PLAYBACK_WIDGET, ADMA4_SPDIF_CAPTURE_WIDGET)
	},
	{
		TCC_SOC_ADMA_DAI_DRV(TCC_ADMA5_DAI, ADMA5_PLAYBACK_WIDGET, ADMA5_CAPTURE_WIDGET),
		TCC_SOC_ADMA_DAI_DRV(TCC_ADMA5_SPDIF, ADMA5_SPDIF_PLAYBACK_WIDGET, ADMA5_SPDIF_CAPTURE_WIDGET),
	},
	{
		TCC_SOC_ADMA_DAI_DRV(TCC_ADMA6_DAI, ADMA6_PLAYBACK_WIDGET, ADMA6_CAPTURE_WIDGET),
		TCC_SOC_ADMA_DAI_DRV(TCC_ADMA6_SPDIF, ADMA6_SPDIF_PLAYBACK_WIDGET, ADMA6_SPDIF_CAPTURE_WIDGET),
	},
	{
		TCC_SOC_ADMA_DAI_DRV(TCC_ADMA7_DAI, ADMA7_PLAYBACK_WIDGET, ADMA7_CAPTURE_WIDGET),
		TCC_SOC_ADMA_DAI_DRV(TCC_ADMA7_SPDIF, ADMA7_SPDIF_PLAYBACK_WIDGET, ADMA7_SPDIF_CAPTURE_WIDGET),
	}
};

static int tcc_adma_pcm_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)devm_kzalloc(
			&pdev->dev,
			sizeof(struct tcc_adma_pcm_t),
			GFP_KERNEL);

	adma_pcm_dbg(" %s\n", __func__);

	if (adma_pcm == NULL) {
		(void) adma_pcm_err(" %s : Fail to alloc adma_pcm dt\n",
			__func__);
		ret = -ENOMEM;
	} else {
		(void) memset(adma_pcm, 0, sizeof(struct tcc_adma_pcm_t));
		ret = parse_pcm_dt(pdev, adma_pcm);

		if (ret != 0) {
			(void) adma_pcm_err("[%d] %s : Fail to parse adma_pcm dt\n",
					adma_pcm->blk_no,
					__func__);
		} else {
			spin_lock_init(&adma_pcm->lock);
#if !defined(CONFIG_ARCH_TCC807X)
			(void) clk_prepare_enable(adma_pcm->adma_hclk);
#endif
			platform_set_drvdata(pdev, adma_pcm);

			ret = devm_request_irq(&pdev->dev, (uint32_t) adma_pcm->adma_irq,
					tcc_adma_pcm_handler, IRQF_TRIGGER_HIGH, "adma-pcm", adma_pcm);
			if (ret != 0) {
				adma_pcm_err("[%d] %s - devm_request_irq failed\n", adma_pcm->blk_no, __func__);
			} else {
				ret = snd_soc_register_component(&pdev->dev, &tcc_adma_pcm_component,
								tcc_adma_pcm_dai_drv[adma_pcm->blk_no], (int)TCC_AUDIO_ARRAY_SIZE(tcc_adma_pcm_dai_drv[adma_pcm->blk_no]));
				if (ret < 0) {
					adma_pcm_err("[%d] %s : tcc_adma_pcm_platform_register failed\n",
							adma_pcm->blk_no, __func__);
				} else {
					adma_pcm_dbg("[%d] %s : tcc_adma_pcm_platform_register success\n",
							adma_pcm->blk_no, __func__);
				}
			}
		}
	}
	return ret;
}

static int tcc_adma_pcm_remove(struct platform_device *pdev)
{
	struct tcc_adma_pcm_t *adma_pcm =
		(struct tcc_adma_pcm_t *)platform_get_drvdata(pdev);

	adma_pcm_dbg("[%d] %s\n", adma_pcm->blk_no, __func__);

	devm_free_irq(&pdev->dev, (uint32_t) adma_pcm->adma_irq, adma_pcm);

	devm_kfree(&pdev->dev, adma_pcm);

	return 0;
}

static struct of_device_id const tcc_adma_pcm_of_match[] = {
	{ .compatible = "telechips,adma" },
	{ .compatible = ""}
};
MODULE_DEVICE_TABLE(of, tcc_adma_pcm_of_match);

static struct platform_driver tcc_adma_pcm_driver = {
	.probe		= tcc_adma_pcm_probe,
	.remove		= tcc_adma_pcm_remove,
#if defined(CONFIG_PM)
	.suspend	= tcc_adma_pcm_suspend,
	.resume		= tcc_adma_pcm_resume,
#endif
	.driver		= {
		.name	= "tcc_adma_pcm_drv",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(tcc_adma_pcm_of_match),
#endif
	},
};

module_platform_driver(tcc_adma_pcm_driver);

MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips ADMA PCM Driver");
MODULE_LICENSE("GPL");
