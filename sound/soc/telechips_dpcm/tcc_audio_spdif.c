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
#include <linux/pinctrl/consumer.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "tcc_adma_pcm.h"
#include "tcc_audio_spdif.h"
#include "tcc_audio_hw.h"

// subframe(32bit) * 2ch * 2(channel coding)
#define SPDIF_BITRATE \
	(128u)

#undef spdif_dai_dbg
#if 0
#define spdif_dai_dbg(f, a...) \
	(void) pr_info("[DEBUG][SPDIF] " a)
#else
#define spdif_dai_dbg(a...)
#endif
#define spdif_dai_err(a...) \
	(void) pr_err("[ERROR][SPDIF] " a)


#define CHECK_SPDIF_HW_PARAM_ELAPSED_TIME \
	(0)

struct tcc_spdif_t {
	struct platform_device *pdev;

	int blk_no;
	void __iomem *reg;
	struct clk *pclk;
#if !defined(CONFIG_ARCH_TCC807X)
	struct clk *hclk;
#endif
	uint32_t clock_rate;
	uint32_t clk_ratio;
 	bool data_format;
	bool copyright;
	//status
	struct tcc_adma_info dma_info;

	struct spdif_reg_t reg_backup;
};

static inline uint32_t calc_spdif_clk(
	const struct tcc_spdif_t *spdif,
	unsigned int sample_rate)
{
	uint32_t rate;
	uint32_t clk_ratio;
	uint32_t ret = 0;

	switch (sample_rate) {
	case 22000:
		rate = 22050;
		break;
	case 11000:
		rate = 11025;
		break;
	default:
		rate = (uint32_t)sample_rate;
		break;
	}

	if (spdif->clk_ratio == 0u) {
		clk_ratio = (rate < 24000u) ? 64u :
			(rate < 32000u) ? 32u :
			(rate < 96000u) ? 16u : 8u;
	} else {
		clk_ratio = spdif->clk_ratio;
	}

	tcc_spdif_tx_clk_ratio(spdif->reg, clk_ratio);

	//ret = rate * (uint32_t) SPDIF_BITRATE * clk_ratio;
	if(rate <= (UINT_MAX / SPDIF_BITRATE)){
		ret = rate * SPDIF_BITRATE;
		if (ret <= (UINT_MAX / clk_ratio)){
			ret *= clk_ratio;
		}
	}

	return ret;
}

static int tcc_spdif_set_clk(struct snd_soc_dai *dai,
			     const struct snd_pcm_substream *substream,
			     uint32_t sample_rate)
{
	struct tcc_spdif_t *spdif =
	    (struct tcc_spdif_t *)snd_soc_dai_get_drvdata(dai);
	uint32_t clock_rate;
	int ret = 0;

	clock_rate = calc_spdif_clk(spdif, sample_rate);
	if (clock_rate > (uint32_t) TCC_SPDIF_MAX_FREQ) {
		spdif_dai_err("%s - SPDIF peri max frequency is %dHz. but you try %dHz\n",
		     __func__,
			 TCC_SPDIF_MAX_FREQ,
			 clock_rate);
		ret = -ENOTSUPP;
	} else {
		spdif->clock_rate = clock_rate;

		if (spdif->pclk != NULL) {
			unsigned long cur_rate = 0;

			clk_disable_unprepare(spdif->pclk);
			(void)clk_set_rate(spdif->pclk, spdif->clock_rate);
			(void)clk_prepare_enable(spdif->pclk);

			cur_rate = clk_get_rate(spdif->pclk);
			spdif_dai_dbg("spdif set_rate : %d, get_rate : %lu\n",
					spdif->clock_rate,
					cur_rate);
		}

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			switch (sample_rate) {
				case 48000:
					tcc_spdif_tx_frequency(
							spdif->reg,
							TCC_SPDIF_TX_48000HZ);
					break;
				case 44100:
					tcc_spdif_tx_frequency(
							spdif->reg,
							TCC_SPDIF_TX_44100HZ);
					break;
				case 32000:
					tcc_spdif_tx_frequency(
							spdif->reg,
							TCC_SPDIF_TX_32000HZ);
					break;
				default:
					tcc_spdif_tx_frequency(spdif->reg, TCC_SPDIF_TX_SRC);
					break;
			}
		}
	}
	return ret;
}

static int tcc_spdif_startup(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	unused(substream);

	spdif_dai_dbg("%s - active : %d\n", __func__, snd_soc_dai_active(dai));
	return 0;
}

static int tcc_spdif_open(
	struct snd_soc_component *component,
	struct snd_pcm_substream *substream)
{
	struct tcc_spdif_t *spdif =
	    (struct tcc_spdif_t *)snd_soc_component_get_drvdata(component);
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);

	spdif_dai_dbg("[%d] %s\n", spdif->blk_no, __func__);

	spdif->dma_info.dev_type = TCC_ADMA_SPDIF;

	snd_soc_dai_set_dma_data(cpu_dai, substream, &spdif->dma_info);

	return 0;
}

static void tcc_spdif_shutdown(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	//struct tcc_spdif_t *spdif =
	// (struct tcc_spdif_t*)snd_soc_dai_get_drvdata(dai);

	spdif_dai_dbg("%s - active : %d\n", __func__, snd_soc_dai_active(dai));
}

static int tcc_spdif_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	const struct tcc_spdif_t *spdif =
	    (struct tcc_spdif_t *)snd_soc_dai_get_drvdata(dai);
	snd_pcm_format_t format = params_format(params);
	uint32_t sample_rate = params_rate(params);
	int ret = 0;
	int i;

#if	(CHECK_SPDIF_HW_PARAM_ELAPSED_TIME == 1)
	struct timeval start, end;
	u64 elapsed_usecs64;
	unsigned int elapsed_usecs;
#endif

	spdif_dai_dbg("%s - format : 0x%08x\n", __func__, format);
	spdif_dai_dbg("%s - sample_rate : %d\n", __func__, sample_rate);

#if	(CHECK_SPDIF_HW_PARAM_ELAPSED_TIME == 1)
	do_gettimeofday(&start);
#endif
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		tcc_spdif_tx_buffers_clear(spdif->reg);

		tcc_spdif_tx_fifo_threshold(spdif->reg, 3);
		tcc_spdif_tx_fifo_clear(spdif->reg, FALSE);
		tcc_spdif_tx_addr_mode(
			spdif->reg,
			TCC_SPDIF_TX_ADDRESS_MODE_DISABLE);
		tcc_spdif_tx_userdata_dmareq_enable(spdif->reg, TRUE);
		tcc_spdif_tx_sampledata_dmareq_enable(spdif->reg, TRUE);
		tcc_spdif_tx_swap_enable(spdif->reg, FALSE);

		if ((params->reserved[0] & (unsigned char)0x01) !=
		    (unsigned char)0x00) {
			spdif_dai_dbg("%s - data format : DATA mode\n",
				      __func__);
			tcc_spdif_tx_format(
				spdif->reg,
				TCC_SPDIF_TX_FORMAT_DATA);
		} else {
			spdif_dai_dbg("%s - data format : %d\n", __func__,
				      spdif->data_format);
			tcc_spdif_tx_format(
				spdif->reg,
				(enum TCC_SPDIF_TX_FORMAT) spdif->data_format);
		}

		spdif_dai_dbg("%s - copyright : %d\n", __func__,
			      spdif->copyright);
		tcc_spdif_tx_copyright(
			spdif->reg,
			(enum TCC_SPDIF_TX_COPYRIGHT) spdif->copyright);

		switch (format) {
		case SNDRV_PCM_FORMAT_S16_LE:
			tcc_spdif_tx_readaddr_mode(
				spdif->reg,
				TCC_SPDIF_TX_16BIT);
			//tcc_spdif_tx_format(spdif->reg,
			// TCC_SPDIF_TX_FORMAT_AUDIO);
			tcc_spdif_tx_bitmode(spdif->reg, 16);
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			tcc_spdif_tx_readaddr_mode(
				spdif->reg,
				TCC_SPDIF_TX_24BIT);
			//tcc_spdif_tx_format(spdif->reg,
			//TCC_SPDIF_TX_FORMAT_AUDIO);
			tcc_spdif_tx_bitmode(spdif->reg, 24);
			break;
		default:
			ret = -EINVAL;
			break;
		}
	} else {
		for (i = 0; i < TCC_SPDIF_RX_BUF_MAX_CNT; i++) {
			tcc_spdif_rx_set_buf(
				spdif->reg,
				(uint32_t) i,
				(uint32_t) 0);
		}

		tcc_spdif_rx_stored_data_in_sample_buf(spdif->reg, TRUE);
		tcc_spdif_rx_hold_ch_type(spdif->reg, TCC_SPDIF_RX_HOLD_CH_A);
		tcc_spdif_rx_sampledata_store_type(spdif->reg,
			TCC_SPDRX_SDATA_STORED_ONLY_VBIT);

		tcc_spdif_rx_store_valid_bit(spdif->reg, FALSE);
		tcc_spdif_rx_store_userdata_bit(spdif->reg, FALSE);
		tcc_spdif_rx_store_channel_status_bit(spdif->reg, FALSE);
		tcc_spdif_rx_store_parity_bit(spdif->reg, FALSE);

		switch (format) {
		case SNDRV_PCM_FORMAT_S16_LE:
			tcc_spdif_rx_bitmode(spdif->reg, 16);
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			tcc_spdif_rx_bitmode(spdif->reg, 24);
			break;
		default:
			ret = -EINVAL;
			break;
		}

		if (ret != -EINVAL) {
			// fixed value. from SoC
			tcc_spdif_rx_set_phasedet(spdif->reg, 0x8C0C);
		}
	}

	if(ret < 0) {
		spdif_dai_dbg("%s format is invalid ret=%d\n", __func__, ret);
	} else {
		ret = tcc_spdif_set_clk(dai, substream, sample_rate);
	}

#if	(CHECK_SPDIF_HW_PARAM_ELAPSED_TIME == 1)
	do_gettimeofday(&end);

	elapsed_usecs64 = timeval_to_ns(&end) - timeval_to_ns(&start);
	do_div(elapsed_usecs64, NSEC_PER_USEC);
	elapsed_usecs = elapsed_usecs64;

	spdif_dai_dbg("spdif hw_params's elapsed time : %03d usec\n",
		elapsed_usecs);
#endif

	return ret;
}

static int tcc_spdif_hw_free(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	const struct tcc_spdif_t *spdif =
	    (struct tcc_spdif_t *)snd_soc_dai_get_drvdata(dai);

	spdif_dai_dbg("%s - active:%d\n", __func__, snd_soc_dai_active(dai));

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		spdif_dai_dbg("%s - PLAY\n", __func__);
		tcc_spdif_tx_fifo_clear(spdif->reg, TRUE);
		tcc_spdif_tx_fifo_clear(spdif->reg, FALSE);
	} else {
		spdif_dai_dbg("%s - CAPTURE\n", __func__);
	}

	return 0;
}

static int tcc_spdif_trigger(
	struct snd_pcm_substream *substream,
	int cmd,
	struct snd_soc_dai *dai)
{
	const struct tcc_spdif_t *spdif =
	    (struct tcc_spdif_t *)snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	spdif_dai_dbg("%s\n", __func__);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			spdif_dai_dbg("TRIGGER_START, PLAY\n");
			tcc_spdif_tx_irq_enable(spdif->reg, TRUE);
			tcc_spdif_tx_data_valid(spdif->reg, TRUE);
			tcc_spdif_tx_enable(spdif->reg, TRUE);
		} else {
			spdif_dai_dbg("TRIGGER_START, CAPTURE\n");
			tcc_spdif_rx_irq_enable(spdif->reg, TRUE);
			tcc_spdif_rx_enable(spdif->reg, TRUE);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			spdif_dai_dbg("TRIGGER_STOP, PLAY\n");
			tcc_spdif_tx_enable(spdif->reg, FALSE);
			tcc_spdif_tx_data_valid(spdif->reg, FALSE);
			tcc_spdif_tx_irq_enable(spdif->reg, FALSE);
		} else {
			spdif_dai_dbg("TRIGGER_STOP, CAPTURE\n");
			tcc_spdif_rx_enable(spdif->reg, FALSE);
			tcc_spdif_rx_irq_enable(spdif->reg, FALSE);
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int tcc_spdif_set_clkdiv(
	struct snd_soc_dai *dai,
	int div_id,
	int clk_div)
{
	struct tcc_spdif_t *spdif =
	    (struct tcc_spdif_t *)snd_soc_dai_get_drvdata(dai);
	int ret;

	spdif_dai_dbg("%s - div_id:%d, div:%d\n", __func__, div_id, clk_div);

	if (clk_div >= 0) {
		spdif->clk_ratio = si_to_ui(clk_div);
		ret = 0;
	} else {
		spdif_dai_dbg("%s - div:%d is negative\n", __func__, clk_div);
		ret = -EINVAL;
	}

	return ret;
}

static struct snd_soc_dai_ops tcc_spdif_ops = {
	.set_clkdiv = tcc_spdif_set_clkdiv,
	.startup = tcc_spdif_startup,
	.shutdown = tcc_spdif_shutdown,
	.hw_params = tcc_spdif_hw_params,
	.hw_free = tcc_spdif_hw_free,
	.trigger = tcc_spdif_trigger,
};

static bool tcc_spdif_is_active(
	struct snd_soc_component *component,
	const char *set_change)
{
	const struct tcc_spdif_t *spdif =
		(struct tcc_spdif_t *)snd_soc_component_get_drvdata(component);
	bool is_active = FALSE;

	is_active = (snd_soc_component_active(component) == 0u) ? FALSE : TRUE;

	if (is_active) {
		spdif_dai_err("%s doesn't change while SPDIF-%d is activated.",
			set_change,
			spdif->blk_no);
	}

	return is_active;
}

static char const *spdif_copyright_texts[] = {
	"Inhibited",
	"Permitted",
};

static const struct soc_enum spdif_copyright_enum[] = {
	SOC_ENUM_SINGLE_EXT((TCC_AUDIO_ARRAY_SIZE(spdif_copyright_texts)),
			    (spdif_copyright_texts)),
};

static char const *spdif_data_format_texts[] = {
	"Audio",
	"Data",
};

static const struct soc_enum spdif_data_format_enum[] = {
	SOC_ENUM_SINGLE_EXT((TCC_AUDIO_ARRAY_SIZE(spdif_data_format_texts)),
			    (spdif_data_format_texts)),
};

static int get_spdif_copyright(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
	    snd_soc_kcontrol_component(kcontrol);
	const struct tcc_spdif_t *spdif =
	    (struct tcc_spdif_t *)snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = (long)spdif->copyright;

	return 0;
}

static int set_spdif_copyright(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
	    snd_soc_kcontrol_component(kcontrol);
	struct tcc_spdif_t *spdif =
	    (struct tcc_spdif_t *)snd_soc_component_get_drvdata(component);
	int ret = 0;

	if (tcc_spdif_is_active(component, __func__) == TRUE) {
		ret = -EINVAL;
	} else {
		spdif->copyright = (bool) ucontrol->value.integer.value[0];
	}

	return ret;
}

static int get_spdif_data_format(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
	    snd_soc_kcontrol_component(kcontrol);
	const struct tcc_spdif_t *spdif =
	    (struct tcc_spdif_t *)snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = (long)spdif->data_format;

	return 0;
}

static int set_spdif_data_format(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
	    snd_soc_kcontrol_component(kcontrol);
	struct tcc_spdif_t *spdif =
	    (struct tcc_spdif_t *)snd_soc_component_get_drvdata(component);
	int ret = 0;

	if (tcc_spdif_is_active(component, __func__) == TRUE) {
		ret = -EINVAL;
	} else {
		spdif->data_format = (bool) ucontrol->value.integer.value[0];
	}

	return ret;
}


static int get_spdif_ratio(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
	    snd_soc_kcontrol_component(kcontrol);
	const struct tcc_spdif_t *spdif =
	    (struct tcc_spdif_t *)snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = (long) spdif->clk_ratio;

	return 0;
}

static int set_spdif_ratio(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
	    snd_soc_kcontrol_component(kcontrol);
	struct tcc_spdif_t *spdif =
	    (struct tcc_spdif_t *)snd_soc_component_get_drvdata(component);
	int32_t value = sl_to_si(ucontrol->value.integer.value[0]);

	spdif->clk_ratio = si_to_ui(value);

	return 0;
}

static int tcc_spdif_suspend(struct snd_soc_component *component)
{
	struct tcc_spdif_t *spdif =
	    (struct tcc_spdif_t *)snd_soc_component_get_drvdata(component);
	struct pinctrl *tcc_pctrl;

	spdif_dai_dbg("%s\n", __func__);

	tcc_pctrl = pinctrl_get_select(component->dev, "idle");
	if (IS_ERR(tcc_pctrl)) {
		spdif_dai_err("%s : pinctrl suspend error[0x%p]\n",
			   __func__, tcc_pctrl);
	}

	tcc_spdif_reg_backup(spdif->reg, &spdif->reg_backup);

	return 0;
}

static int tcc_spdif_resume(struct snd_soc_component *component)
{
	const struct tcc_spdif_t *spdif =
	    (struct tcc_spdif_t *)snd_soc_component_get_drvdata(component);
	struct pinctrl *tcc_pctrl;

	spdif_dai_dbg("%s\n", __func__);

	tcc_pctrl = pinctrl_get_select(component->dev, "default");
	if (IS_ERR(tcc_pctrl)) {
		spdif_dai_err("%s : pinctrl resume error[0x%p]\n",
			__func__,
			tcc_pctrl);
	}

	tcc_spdif_reg_restore(spdif->reg, &spdif->reg_backup);

	return 0;
}

#define TCC_KCONROLS_SPDIF(BLK_NUM)	\
	{	\
		SOC_ENUM_EXT((#BLK_NUM "Data Format"), (spdif_data_format_enum[0]), (get_spdif_data_format), (set_spdif_data_format)),	\
		SOC_ENUM_EXT((#BLK_NUM "Copyright"), (spdif_copyright_enum[0]),  (get_spdif_copyright), (set_spdif_copyright)),	\
		SOC_SINGLE_EXT((#BLK_NUM "MCLK DIV"), 0, 0, 255, 0, (get_spdif_ratio), (set_spdif_ratio))	\
	}

static const struct snd_kcontrol_new tcc_spdif_snd_controls[TCC_SPDIF_BLK_MAX][3] = {
	TCC_KCONROLS_SPDIF(TCC_SPDIF_BLK0),
	TCC_KCONROLS_SPDIF(TCC_SPDIF_BLK1),
	TCC_KCONROLS_SPDIF(TCC_SPDIF_BLK2),
	TCC_KCONROLS_SPDIF(TCC_SPDIF_BLK3),
	TCC_KCONROLS_SPDIF(TCC_SPDIF_BLK4),
	TCC_KCONROLS_SPDIF(TCC_SPDIF_BLK5),
	TCC_KCONROLS_SPDIF(TCC_SPDIF_BLK6),
	TCC_KCONROLS_SPDIF(TCC_SPDIF_BLK7)
};

#define TCC_SOC_COMP_DRV_SPDIF(NAME, BLK_NUM)	\
	{	\
		.name = NAME,	\
		.controls = tcc_spdif_snd_controls[BLK_NUM],	\
		.num_controls = TCC_AUDIO_ARRAY_SIZE(tcc_spdif_snd_controls[BLK_NUM]),	\
		.open = tcc_spdif_open, \
		.suspend = tcc_spdif_suspend,	\
		.resume = tcc_spdif_resume,		\
	}

static const struct snd_soc_component_driver tcc_spdif_component_drv[TCC_SPDIF_BLK_MAX] = {
	TCC_SOC_COMP_DRV_SPDIF("tcc-spdif0", TCC_SPDIF_BLK0),
	TCC_SOC_COMP_DRV_SPDIF("tcc-spdif1", TCC_SPDIF_BLK1),
	TCC_SOC_COMP_DRV_SPDIF("tcc-spdif2", TCC_SPDIF_BLK2),
	TCC_SOC_COMP_DRV_SPDIF("tcc-spdif3", TCC_SPDIF_BLK3),
	TCC_SOC_COMP_DRV_SPDIF("tcc-spdif4", TCC_SPDIF_BLK4),
	TCC_SOC_COMP_DRV_SPDIF("tcc-spdif5", TCC_SPDIF_BLK5),
	TCC_SOC_COMP_DRV_SPDIF("tcc-spdif6", TCC_SPDIF_BLK6),
	TCC_SOC_COMP_DRV_SPDIF("tcc-spdif7", TCC_SPDIF_BLK7)
};

#define TCC_SOC_ADMA_SPD_DAI_DRV(NAME, SNAME_PLAYBACK, SNAME_CAPTURE) \
	{	\
		.name = NAME,	\
		.playback = {	\
			.stream_name = SNAME_PLAYBACK,	\
			.channels_min = 2,	\
			.channels_max = 2,	\
			.rates = SNDRV_PCM_RATE_8000_48000,	\
			.formats = (SNDRV_PCM_FMTBIT_U16_LE |SNDRV_PCM_FMTBIT_S16_LE |SNDRV_PCM_FMTBIT_S24_LE),	\
		},	\
		.capture = {	\
			.stream_name = SNAME_CAPTURE,	\
			.channels_min = 2,	\
			.channels_max = 2,	\
			.rates = SNDRV_PCM_RATE_8000_48000,	\
			.formats = (SNDRV_PCM_FMTBIT_U16_LE |SNDRV_PCM_FMTBIT_S16_LE |SNDRV_PCM_FMTBIT_S24_LE),	\
		},	\
		.symmetric_rates = 1,	\
		.ops = &tcc_spdif_ops,	\
	}

static struct snd_soc_dai_driver tcc_spdif_dai_drv[TCC_SPDIF_BLK_MAX] = {
	TCC_SOC_ADMA_SPD_DAI_DRV(TCC_SPDIF0, SPDIF0_PLAYBACK_WIDGET, SPDIF0_CAPTURE_WIDGET),
	TCC_SOC_ADMA_SPD_DAI_DRV(TCC_SPDIF1, SPDIF1_PLAYBACK_WIDGET, SPDIF1_CAPTURE_WIDGET),
	TCC_SOC_ADMA_SPD_DAI_DRV(TCC_SPDIF2, SPDIF2_PLAYBACK_WIDGET, SPDIF2_CAPTURE_WIDGET),
	TCC_SOC_ADMA_SPD_DAI_DRV(TCC_SPDIF3, SPDIF3_PLAYBACK_WIDGET, SPDIF3_CAPTURE_WIDGET),
	TCC_SOC_ADMA_SPD_DAI_DRV(TCC_SPDIF4, SPDIF4_PLAYBACK_WIDGET, SPDIF4_CAPTURE_WIDGET),
	TCC_SOC_ADMA_SPD_DAI_DRV(TCC_SPDIF5, SPDIF5_PLAYBACK_WIDGET, SPDIF5_CAPTURE_WIDGET),
	TCC_SOC_ADMA_SPD_DAI_DRV(TCC_SPDIF6, SPDIF6_PLAYBACK_WIDGET, SPDIF6_CAPTURE_WIDGET),
	TCC_SOC_ADMA_SPD_DAI_DRV(TCC_SPDIF7, SPDIF7_PLAYBACK_WIDGET, SPDIF7_CAPTURE_WIDGET),
};

static void spdif_initialize(const struct tcc_spdif_t *spdif)
{
#if !defined(CONFIG_ARCH_TCC807X)
	(void)clk_prepare_enable(spdif->hclk);
#endif
	(void)clk_set_rate(spdif->pclk, spdif->clock_rate);
	(void)clk_prepare_enable(spdif->pclk);

	spdif_dai_dbg("%s - spdif->clk_rate:%d\n", __func__, spdif->clock_rate);

}

static void spdif_set_default_configrations(struct tcc_spdif_t *spdif)
{
	// these values will be updated by device tree or another functions
	spdif->data_format = FALSE;	//Copy inhibited
	spdif->copyright = FALSE;	// Audio Format
}

static int parse_spdif_dt(
	struct platform_device *pdev,
	struct tcc_spdif_t *spdif)
{
	int32_t ret = 0;

	spdif->pdev = pdev;

	spdif->blk_no = of_alias_get_id(pdev->dev.of_node, "spdif");
	spdif_dai_dbg("blk_no : %d\n", spdif->blk_no);

	/* get dai info. */
	spdif->reg = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR((void *)spdif->reg)) {
		spdif->reg = NULL;
		spdif_dai_err("spdif_reg is NULL\n");
		ret = -EINVAL;
	} else {
		spdif_dai_dbg("spdif_reg=%p\n", spdif->reg);

		spdif->pclk = of_clk_get(pdev->dev.of_node, 0);
#if !defined(CONFIG_ARCH_TCC807X)
		spdif->hclk = of_clk_get(pdev->dev.of_node, 1);

		if ((IS_ERR(spdif->pclk)) ||
			(IS_ERR(spdif->hclk))) {
			ret = -EINVAL;
		}
#else
		if (IS_ERR(spdif->pclk)) {
			ret = -EINVAL;
		}
#endif
		else {
			ret =
				of_property_read_u32(pdev->dev.of_node, "clock-frequency",
						&spdif->clock_rate);
			if (ret < 0) {
				spdif_dai_err("clock-frequency value is not exist\n");
				ret = -EINVAL;
			} else {
				spdif_dai_dbg("clk_rate=%u\n", spdif->clock_rate);
			}
 		}
	}

	return ret;
}

static int tcc_spdif_probe(struct platform_device *pdev)
{
	struct tcc_spdif_t *spdif;
	int ret = 0;

	spdif_dai_dbg("%s\n", __func__);

	spdif =
	    (struct tcc_spdif_t *)devm_kzalloc(
			&pdev->dev,
			sizeof(struct tcc_spdif_t),
			GFP_KERNEL);
	if (spdif == NULL) {
		spdif_dai_dbg("%s alloc fail.\n", __func__);
		ret = -ENOMEM;
	} else {
		spdif_dai_dbg("%s - spdif : %p\n", __func__, spdif);

		spdif_set_default_configrations(spdif);

		ret = parse_spdif_dt(pdev, spdif);
		if (ret < 0) {
			spdif_dai_err("%s : Fail to parse spdif dt\n",
					__func__);
		} else {
			platform_set_drvdata(pdev, spdif);

			spdif_initialize(spdif);
#if 0//test
			ret = devm_snd_soc_register_component(
						&pdev->dev,
						&tcc_spdif_component_drv,
						&tcc_spdif_dai_drv[spdif->blk_no],
						1);
#else
			ret = devm_snd_soc_register_component(
						&pdev->dev,
						&tcc_spdif_component_drv[spdif->blk_no],
						&tcc_spdif_dai_drv[spdif->blk_no],
						1);
#endif
		}
	}

	if (ret < 0) {
		spdif_dai_err("devm_snd_soc_register_component failed\n");
		if (spdif != NULL) {
			kfree(spdif);
		}
	} else {
		spdif_dai_dbg("devm_snd_soc_register_component success\n");
	}
	return ret;
}

static int tcc_spdif_remove(struct platform_device *pdev)
{
//	struct tcc_spdif_t *spdif =
//		(struct tcc_spdif_t*)platform_get_drvdata(pdev);

	spdif_dai_dbg("%s\n", __func__);

	return 0;
}

static const struct of_device_id tcc_spdif_of_match[] = {
	{.compatible = "telechips,spdif"},
	{.compatible = ""}
};

MODULE_DEVICE_TABLE(of, tcc_spdif_of_match);

static struct platform_driver tcc_spdif_driver = {
	.probe = tcc_spdif_probe,
	.remove = tcc_spdif_remove,
	.driver = {
		.name = "tcc_spdif_drv",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(tcc_spdif_of_match),
#endif
	},
};

module_platform_driver(tcc_spdif_driver);

MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips SPDIF Driver");
MODULE_LICENSE("GPL");
