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
#include "tcc_dai.h"
#include "tcc_audio_chmux.h"

#define unused(x) (void)(x)

#undef i2s_dai_func_dbg
#if 0
#define i2s_dai_func_dbg_enable
#define i2s_dai_func_dbg(a...) \
	(void) pr_info("[FUNC_DEBUG][I2S]" a)
#else
#define i2s_dai_func_dbg(a...)
#endif
#undef i2s_dai_dbg
#if 0
#define i2s_dai_dbg(a...) \
	(void) pr_info("[DEBUG][I2S]" a)
#else
#define i2s_dai_dbg(a...)
#endif
#define i2s_dai_err(a...) \
	(void) pr_err("[ERROR][I2S] " a)
#define i2s_dai_warn(a...) \
	(void) pr_err("[WARN][I2S] " a)

#define CHECK_I2S_HW_PARAM_ELAPSED_TIME	(0)

#define DEFAULT_MCLK_DIV \
	(4) //4,6,8,16,24,32,48,64
#define DEFAULT_BCLK_RATIO	 \
	(64) //32,48,64
#define DEFAULT_DAI_SAMPLERATE \
	(32000) //for HDMI
#define DEFAULT_DAI_FILTER_CLK_RATE \
	(300000000)

enum tcc_i2s_block_type {
	DAI_BLOCK_STEREO_TYPE = 0,
	DAI_BLOCK_7_1CH_TYPE = 1,
	DAI_BLOCK_9_1CH_TYPE = 2,
	DAI_BLOCK_TYPE_MAX  = 3
};

enum tcc_i2s_audio_filter_type {
	DAI_AUDIO_CLK_FILTER_TYPE = 1,
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	DAI_AUDIO_DATA_FILTER_TYPE = 2
#endif//defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
};

struct tcc_i2s_t {
	struct platform_device *pdev;

	// hw info
	int32_t blk_no;
	void __iomem *dai_reg;
	struct clk *dai_pclk;
	struct clk *dai_hclk;
	struct clk *dai_filter_clk;
	uint32_t have_fifo_clear_bit;
	uint32_t audio_filter_bit;
	uint32_t block_type;

#if defined(CONFIG_ARCH_TCC802X)
	void __iomem *pcfg_reg;
	struct tcc_gfb_i2s_port portcfg;
#endif

	// configurations
	uint32_t sample_rate;
	uint8_t mclk_div;
	uint16_t bclk_ratio;
	uint32_t dai_fmt;

	bool clk_continuous;
	bool is_pinctrl_export;

	bool tdm_mode;
	bool frame_invert; //for TDM I2S mode
	bool tdm_late_mode;
	bool tdm_multi_port;
#if defined(CONFIG_ARCH_TCC805X)
	bool rx_bclk_delay;
#endif
	bool is_updated;
	uint8_t tdm_slots;
	uint8_t tdm_slot_width;

	//status
	unsigned long clock_rate;
	struct tcc_adma_info dma_info;
	struct dai_reg_t regs_backup; // for suspend/resume
};

static inline int32_t check_i2s_hw_params_sub_check_channels(
	const struct tcc_i2s_t *i2s,
	uint32_t channels)
{
	int32_t ret = 0;

	if(i2s->tdm_mode == FALSE){
		if ((i2s->block_type == (uint32_t)DAI_BLOCK_7_1CH_TYPE) &&
			!((channels == 1u) || (channels == 2u) || (channels == 8u))) {
			i2s_dai_err("%s - This DAI block only supports 1, 2 or 8 channels\n", __func__);
			ret = -ENOTSUPP;
		} else if ((i2s->block_type == (uint32_t)DAI_BLOCK_9_1CH_TYPE) 	&&
			!((channels == 1u) || (channels == 2u) || (channels == 8u) || (channels == 10u))) {
			i2s_dai_err("%s - This DAI block only supports 1, 2, 8 or 10 channels\n", __func__);
			ret = -ENOTSUPP;
		} else if ((i2s->block_type == (uint32_t)DAI_BLOCK_STEREO_TYPE) &&
			!((channels == 1u) || (channels == 2u))) {
			i2s_dai_err("%s - This DAI block only supports 1 or 2 channels\n", __func__);
			ret = -ENOTSUPP;
		} else {
			ret = 0;
		}
	} else { //i2s->tdm_mode == TRUE
#if defined(CONFIG_ARCH_TCC805X)
		if (!((channels == 2u) || (channels == 4u) || (channels == 8u) || (channels == 16u))) {
			i2s_dai_err("%s - TDM only supports 2, 4, 8, 16 channels\n", __func__);
			ret = -ENOTSUPP;
		}
#else
		if ((channels != 2u) && (channels != 4u) && (channels != 8u)) {
			i2s_dai_err("%s - TDM only supports 2, 4, 8 channels\n", __func__);
			ret = -ENOTSUPP;
		}
#endif
	}
	return ret;
}

static inline int32_t check_i2s_hw_params(
	const struct snd_pcm_substream *substream,
	const struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	int32_t ret = 0;
	const struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);
	int is_active = snd_soc_dai_active(dai);

	uint32_t channels = params_channels(params);

	ret = check_i2s_hw_params_sub_check_channels(i2s, channels);

	if ((i2s->tdm_mode == TRUE) && (i2s->tdm_multi_port == TRUE) && (channels != 8u) && (i2s->tdm_slots != 2u)) {
		uint32_t daifmt = i2s->dai_fmt & (uint32_t)SND_SOC_DAIFMT_FORMAT_MASK;

		if ((daifmt	!= (uint32_t)SND_SOC_DAIFMT_DSP_A)
			&& (daifmt	!= (uint32_t)SND_SOC_DAIFMT_DSP_B)) {
			i2s_dai_err("%s - TDM multi port mode support only 8 channels and 2 slots\n", __func__);
		} else {
			i2s_dai_err("%s - TDM multi port mode support in DSP A/B mode\n", __func__);
		}
		ret = -ENOTSUPP;
	}

#if defined(CONFIG_ARCH_TCC805X)
	if ((i2s->tdm_mode == TRUE) && (i2s->tdm_slot_width != 16u) && (i2s->tdm_slot_width != 24u) && (i2s->tdm_slot_width != 32u)) {
		i2s_dai_err("%s - TDM only supports 16, 24, 32 slot widths\n", __func__);
		ret = -ENOTSUPP;
	}
#else
	if ((i2s->tdm_mode == TRUE) && (i2s->tdm_slot_width != 32u)) {
		i2s_dai_err("%s - TDM only supports 32 slot width\n", __func__);
		ret = -ENOTSUPP;
	}
#endif

#if !defined(CONFIG_ARCH_TCC805X)
	if ((i2s->tdm_mode == TRUE) && (is_active > 1)) {
		i2s_dai_err("TDM Mode supports only uni-direction");
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			i2s_dai_err("%s ", __func__);
			i2s_dai_err("- CAPTURE is already using\n");
		} else {
			i2s_dai_err("%s ", __func__);
			i2s_dai_err("- PLAYBACK is already using\n");
		}
		ret = -ENOTSUPP;
	}
#else
	unused(is_active);
	unused(substream);
#endif

	return ret;
}

static inline unsigned long calc_mclk(
	const struct tcc_i2s_t *i2s,
	uint32_t sample_rate)
{
	uint32_t rate = 0;
	unsigned long ret;
	uint32_t tdm_bclk_ratio;

	rate = (sample_rate == 44100u) ? 44100u :
		(sample_rate == 22000u) ? 22050u :
		(sample_rate == 11000u) ? 11025u : sample_rate;

	if (i2s->tdm_mode) {
		tdm_bclk_ratio = ui_to_ui_mul(i2s->tdm_slots, i2s->tdm_slot_width);
		ret = ui_to_ull_mul(ui_to_ui_mul(rate, i2s->mclk_div),
				tdm_bclk_ratio);
	} else {
		ret = (unsigned long)rate * (unsigned long)i2s->mclk_div *
			(unsigned long)i2s->bclk_ratio;
	}

	return ret;
}

static inline uint32_t calc_bclk_from_mclk(
	const struct tcc_i2s_t *i2s,
	uint32_t mclk)
{
	uint32_t ret;

	ret = mclk % i2s->mclk_div;
	if (ret != 0u) {
		i2s_dai_warn("[%d] bclk is not multiple of ", i2s->blk_no);
		i2s_dai_warn("mclk_div[%d]\n", i2s->mclk_div);
	}
	ret = mclk / i2s->mclk_div;
	return ret;
}

static int tcc_i2s_set_dai_fmt_sub_set_bitclk_polarity(struct tcc_i2s_t *i2s, unsigned int fmt){
	int32_t ret = 0;

	switch (ui_to_si(fmt & (uint32_t)SND_SOC_DAIFMT_INV_MASK)) {
	case SND_SOC_DAIFMT_NB_NF:
		i2s_dai_dbg("[%d] CLK NB_NF\n", i2s->blk_no);
		tcc_dai_set_bitclk_polarity(i2s->dai_reg, TRUE);
		i2s->frame_invert = FALSE;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		i2s_dai_dbg("[%d] CLK IB_NF\n", i2s->blk_no);
		tcc_dai_set_bitclk_polarity(i2s->dai_reg, FALSE);
		i2s->frame_invert = FALSE;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		i2s_dai_dbg("[%d] CLK TDM NB_IF\n", i2s->blk_no);
		tcc_dai_set_bitclk_polarity(i2s->dai_reg, TRUE);
		i2s->frame_invert = TRUE;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		i2s_dai_dbg("[%d] CLK TDM IB_IF\n", i2s->blk_no);
		tcc_dai_set_bitclk_polarity(i2s->dai_reg, FALSE);
		i2s->frame_invert = TRUE;
		break;
	default:
		i2s_dai_err("[%d] does not supported\n",
			i2s->blk_no);
		ret = -ENOTSUPP;
		break;
	}

	return ret;
}

static int tcc_i2s_set_dai_fmt_sub_set_i2s(const struct tcc_i2s_t *i2s, unsigned int fmt){
	int32_t ret = 0;

	switch (ui_to_si(fmt & (uint32_t)SND_SOC_DAIFMT_FORMAT_MASK)) {
	case SND_SOC_DAIFMT_I2S:
		i2s_dai_dbg("[%d] I2S DAIFMT\n", i2s->blk_no);
		if (i2s->tdm_mode == TRUE) {
			if (i2s->frame_invert == TRUE) {

				tcc_dai_set_i2s_tdm_mode(
					i2s->dai_reg,
					(uint32_t) i2s->tdm_slots,
					(uint32_t) i2s->tdm_slot_width,
					(bool) i2s->tdm_late_mode);
			} else{
				tcc_dai_set_cirrus_tdm_mode(
					i2s->dai_reg,
					(uint32_t) i2s->tdm_slots,
					(uint32_t) i2s->tdm_slot_width,
					(bool) i2s->tdm_late_mode);
			}
		} else {
			tcc_dai_set_i2s_mode(i2s->dai_reg);
		}
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		i2s_dai_dbg("[%d] RIGHT_J DAIFMT\n", i2s->blk_no);
		if (i2s->tdm_mode == TRUE) {
			i2s_dai_err("[%d] RIGHT_J", i2s->blk_no);
			(void) pr_err("TDM does not supported\n");
			ret = -ENOTSUPP;
		} else {
			tcc_dai_set_right_j_mode(i2s->dai_reg,
				(uint32_t) i2s->bclk_ratio);
		}
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		i2s_dai_dbg("[%d] LEFT_J DAIFMT\n", i2s->blk_no);
		if (i2s->tdm_mode == TRUE) {
			i2s_dai_err("[%d] LEFT_J", i2s->blk_no);
			(void) pr_err("TDM does not supported\n");
			ret = -ENOTSUPP;
		} else {
			tcc_dai_set_left_j_mode(i2s->dai_reg);
			ret = 0;
		}
		break;
	case SND_SOC_DAIFMT_DSP_A:
		i2s_dai_dbg("[%d] DSP_A DAIFMT\n", i2s->blk_no);
		if (i2s->tdm_mode == TRUE) {
			tcc_dai_set_dsp_tdm_mode(
				i2s->dai_reg,
				(uint32_t) i2s->tdm_slots,
				(uint32_t) i2s->tdm_slot_width,
				FALSE);
		} else {
			i2s_dai_err("[%d] DSP_A", i2s->blk_no);
			(void) pr_err(" supports only TDM Mode\n");
			ret = -ENOTSUPP;
		}
		break;
	case SND_SOC_DAIFMT_DSP_B:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
		i2s_dai_dbg("[%d] DSP_B DAIFMT\n", i2s->blk_no);

		if (i2s->tdm_mode == TRUE) {
			tcc_dai_set_dsp_tdm_mode(i2s->dai_reg,
			i2s->tdm_slots,
			i2s->tdm_slot_width, TRUE);
		} else {
			i2s_dai_err("[%d] DSP_B supports only TDM Mode\n",
					i2s->blk_no);
			ret = -ENOTSUPP;
		}

		break;
#endif
	default:
		i2s_dai_err("[%d] does not supported\n", i2s->blk_no);
		ret = -ENOTSUPP;
		break;
	}

	return ret;
}

static int tcc_i2s_set_dai_fmt_sub_set_master(const struct tcc_i2s_t *i2s, unsigned int fmt){
	int32_t ret = 0;

	switch (ui_to_si(fmt & (uint32_t)SND_SOC_DAIFMT_MASTER_MASK)) {
	case SND_SOC_DAIFMT_CBM_CFM: /* codec clk & FRM master */
		i2s_dai_dbg("[%d] CBM_CFM\n", i2s->blk_no);
		tcc_dai_set_master_mode(
			i2s->dai_reg,
			TRUE,
			FALSE,
			FALSE,
			i2s->is_pinctrl_export);
		break;
	case SND_SOC_DAIFMT_CBS_CFM: /* codec clk slave & FRM master */
		i2s_dai_dbg("[%d] CBS_CFM\n", i2s->blk_no);
		tcc_dai_set_master_mode(
			i2s->dai_reg,
			TRUE,
			TRUE,
			FALSE,
			i2s->is_pinctrl_export);
		break;
	case SND_SOC_DAIFMT_CBM_CFS: /* codec clk master & frame slave */
		i2s_dai_dbg("[%d] CBM_CFS\n", i2s->blk_no);
		tcc_dai_set_master_mode(
			i2s->dai_reg,
			TRUE,
			FALSE,
			TRUE,
			i2s->is_pinctrl_export);
		break;
	case SND_SOC_DAIFMT_CBS_CFS: /* codec clk & FRM slave */
		i2s_dai_dbg("[%d] CBS_CFS\n", i2s->blk_no);
		tcc_dai_set_master_mode(
			i2s->dai_reg,
			TRUE,
			TRUE,
			TRUE,
			i2s->is_pinctrl_export);
		break;
	default:
		i2s_dai_err("[%d] does not supported\n", i2s->blk_no);
		ret = -ENOTSUPP;
		break;
	}

	return ret;
}

static int tcc_i2s_set_dai_fmt_sub_set_clk_mask(struct tcc_i2s_t *i2s, unsigned int fmt){
	int32_t ret = 0;
	switch (ui_to_si(fmt & (uint32_t)SND_SOC_DAIFMT_CLOCK_MASK)) {
	case SND_SOC_DAIFMT_CONT:
		i2s_dai_dbg("[%d] SND_SOC_DAIFMT_CONT\n", i2s->blk_no);
		i2s->clk_continuous = TRUE;
		break;
	case SND_SOC_DAIFMT_GATED:
		i2s_dai_dbg("[%d] SND_SOC_DAIFMT_GATED\n", i2s->blk_no);
		i2s->clk_continuous = FALSE;
		break;
	default:
		i2s_dai_err("[%d] does not supported\n", i2s->blk_no);
		ret = -ENOTSUPP;
		break;
	}

	return ret;
}


static int tcc_i2s_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);
	int32_t ret = 0;

	i2s_dai_func_dbg("[%d] ++ %s ++\n", i2s->blk_no, __func__);

	i2s->dai_fmt = 0;

	ret = tcc_i2s_set_dai_fmt_sub_set_bitclk_polarity(i2s, fmt);
	if (ret == 0) {

		ret = tcc_i2s_set_dai_fmt_sub_set_i2s(i2s, fmt);
		if (ret == 0) {
#if !defined(CONFIG_ARCH_TCC805X)
			if ((i2s->tdm_mode == TRUE) &&
				((fmt & (uint32_t)SND_SOC_DAIFMT_MASTER_MASK) !=
				(uint32_t)SND_SOC_DAIFMT_CBS_CFS)) {
				i2s_dai_err("TDM modes supports only CBS_CFS\n");
				ret = -ENOTSUPP;
			}else{
#endif
				ret = tcc_i2s_set_dai_fmt_sub_set_master(i2s, fmt);
				if (ret == 0) {
					ret = tcc_i2s_set_dai_fmt_sub_set_clk_mask(i2s, fmt);

					if (ret == 0) {
						i2s->dai_fmt = fmt;

						if (i2s->clk_continuous == TRUE) {
							/* Workaround Code for TCC803X, TCC899X and TCC901X
							 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
							 * So, we should always restore DCLKDIV value while write that
							 * value to register
							 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) ||\
							defined(CONFIG_ARCH_TCC901X)
							if ((i2s->block_type ==
									(uint32_t)DAI_BLOCK_STEREO_TYPE) ||
									(i2s->block_type ==
									(uint32_t)DAI_BLOCK_9_1CH_TYPE)) {
								i2s->regs_backup.dclkdiv =
									tcc_dai_set_clk_mode(i2s->dai_reg,
											i2s->mclk_div,
											i2s->bclk_ratio,
											i2s->tdm_mode);
							} else
#endif
							{
#if defined(CONFIG_ARCH_TCC805X)
								(void) tcc_dai_set_clk_mode(
										i2s->dai_reg,
										i2s->mclk_div,
										i2s->bclk_ratio,
										i2s->tdm_slot_width,
										i2s->tdm_mode);
#else
								(void) tcc_dai_set_clk_mode(i2s->dai_reg,
										i2s->mclk_div,
										i2s->bclk_ratio,
										i2s->tdm_mode);
#endif
							}
						} else {
							struct dai_reg_t regs = {0};

							tcc_dai_reg_backup(i2s->dai_reg, &regs);

							/* Workaround Code for TCC803X, TCC899X and TCC901X
							 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
							 * So, we should always restore DCLKDIV value while write that
							 * value to register
							 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) ||\
							defined(CONFIG_ARCH_TCC901X)
							if ((i2s->block_type ==
									(uint32_t)DAI_BLOCK_STEREO_TYPE) ||
									(i2s->block_type ==
									(uint32_t)DAI_BLOCK_9_1CH_TYPE)) {
								regs.dclkdiv = i2s->regs_backup.dclkdiv;
							}
#endif
							clk_disable_unprepare(i2s->dai_hclk);
							(void) clk_prepare_enable(i2s->dai_hclk);

							tcc_dai_reg_restore(i2s->dai_reg, &regs);
						}
					}
				}
#if !defined(CONFIG_ARCH_TCC805X)
			}
#endif
		}
	}

	i2s_dai_func_dbg("[%d] -- %s --\n", i2s->blk_no, __func__);

	return ret;
}

static int tcc_i2s_set_clkdiv(struct snd_soc_dai *dai, int div_id, int clk_div)
{
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	i2s_dai_func_dbg("[%d] ++ %s ++\n", i2s->blk_no, __func__);

	i2s_dai_dbg("[%d] %s - div_id:%d, div:%d\n",
		i2s->blk_no,
		__func__,
		div_id,
		clk_div);

	if ((enum TCC_DAI_CLKDIV_ID)div_id == TCC_DAI_CLKDIV_ID_MCLK_TO_BCLK) {
		switch ((enum TCC_DAI_MCLK_DIV)clk_div) {
		case TCC_DAI_MCLK_TO_BCLK_DIV_1:
		case TCC_DAI_MCLK_TO_BCLK_DIV_2:
			if (i2s->tdm_mode == FALSE) {
				ret = -EINVAL;
				break;
			}
			i2s->mclk_div = (uint8_t)(si_to_ui(clk_div) & 0xFFU);
			break;
		case TCC_DAI_MCLK_TO_BCLK_DIV_4:
		case TCC_DAI_MCLK_TO_BCLK_DIV_6:
		case TCC_DAI_MCLK_TO_BCLK_DIV_8:
		case TCC_DAI_MCLK_TO_BCLK_DIV_16:
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
		case TCC_DAI_MCLK_TO_BCLK_DIV_24:
		case TCC_DAI_MCLK_TO_BCLK_DIV_32:
		case TCC_DAI_MCLK_TO_BCLK_DIV_48:
		case TCC_DAI_MCLK_TO_BCLK_DIV_64:
#endif
			i2s->mclk_div = (uint8_t)(si_to_ui(clk_div) & 0xFFU);
			break;
		default:
			ret = -EINVAL;
			break;
		}
	}

	i2s_dai_func_dbg("[%d] -- %s --\n", i2s->blk_no, __func__);

	return ret;
}

static int tcc_i2s_set_bclk_ratio(struct snd_soc_dai *dai, unsigned int ratio)
{
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	i2s_dai_func_dbg("[%d] ++ %s ++\n", i2s->blk_no, __func__);

	i2s_dai_dbg(" %s - ratio:%u\n", __func__, ratio);

	// bclk_ratio don't care in tdm mode
	if (i2s->tdm_mode != TRUE) {
		switch (ratio) {
			case (unsigned int)TCC_DAI_BCLK_RATIO_32:
			case (unsigned int)TCC_DAI_BCLK_RATIO_48:
			case (unsigned int)TCC_DAI_BCLK_RATIO_64:
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
			case (unsigned int)TCC_DAI_BCLK_RATIO_512:
#endif
				i2s->bclk_ratio = (uint16_t)(ratio & 0xFFFFU);
				break;
			default:
				ret = -EINVAL;
				break;
		}
	}

	i2s_dai_func_dbg("[%d] -- %s --\n", i2s->blk_no, __func__);

	return ret;
}

static int tcc_i2s_startup(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
#ifdef i2s_dai_func_dbg_enable
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);
#else
	unused(dai);
#endif
	unused(substream);

	i2s_dai_func_dbg("[%d] +- %s - active : %d -+\n",
		i2s->blk_no, __func__, snd_soc_dai_active(dai));

	return 0;
}

static void tcc_i2s_shutdown(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	const struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);
	int is_active = snd_soc_dai_active(dai);

	i2s_dai_func_dbg("[%d] ++ %s - active : %d ++\n",
		i2s->blk_no,
		__func__,
		is_active);

	if ((i2s->clk_continuous == FALSE) && (is_active == 0)) {
		i2s_dai_dbg("[%d] %s - dai_disable\n",
				i2s->blk_no,
				__func__);
		tcc_dai_enable(i2s->dai_reg, FALSE);
	}

	i2s_dai_func_dbg("[%d] -- %s --\n", i2s->blk_no, __func__);

}

static int tcc_i2s_set_tdm_slot(
	struct snd_soc_dai *dai,
	unsigned int tx_mask,
	unsigned int rx_mask,
	int slots,
	int slot_width)
{
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);
//	struct dai_reg_t regs = {0};

	i2s_dai_func_dbg("[%d] ++ %s - slot:%d, slot_width:%d ++\n",
	i2s->blk_no, __func__, slots, slot_width);

	i2s->tdm_mode = ((slots != 0) && (slot_width != 0)) ? TRUE : FALSE;
	i2s->tdm_slots = (uint8_t) (si_to_ui(slots) & 0xFFU);
	i2s->tdm_slot_width = (uint8_t) (si_to_ui(slot_width) & 0xFFU);

	i2s->dma_info.tdm_mode = i2s->tdm_mode;

	i2s_dai_func_dbg("[%d] -- %s --\n", i2s->blk_no, __func__);

	return 0;
}

static int tcc_i2s_set_sysclk(struct snd_soc_dai *dai,
							int clk_id,
							unsigned int freq,
							int dir)
{
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);
	int ret = 0;
	unsigned long mclk;
	unsigned long cur_rate;

	i2s_dai_func_dbg("[%d] ++ %s ++\n", i2s->blk_no, __func__);

	switch (clk_id) {
	case (int)TCC_DAI_MCLK:
		tcc_dai_enable(i2s->dai_reg, FALSE);

		if (freq == 0u) {
			mclk = calc_mclk(i2s, i2s->sample_rate);
		} else {
			mclk = freq;
		}

		i2s->clock_rate = mclk;

		(void) clk_disable_unprepare(i2s->dai_pclk);
		(void) clk_set_rate(i2s->dai_pclk, i2s->clock_rate);
		(void) clk_prepare_enable(i2s->dai_pclk);

		cur_rate = clk_get_rate(i2s->dai_pclk);
		i2s_dai_dbg("[%d] %s - get_clk_rate:%lu, mclk_div:%u, bclk_ratio:%u, tdm_slots:%u, tdm_slot_width:%u\n",
			i2s->blk_no, __func__, cur_rate,
			i2s->mclk_div, i2s->bclk_ratio,
			i2s->tdm_slots, i2s->tdm_slot_width);

		tcc_dai_enable(i2s->dai_reg, TRUE);
		break;
	default:
		i2s_dai_err("%s - Not support clock ID\n", __func__);
		ret = -EINVAL;
		break;
	}

	i2s_dai_func_dbg("[%d] -- %s --\n", i2s->blk_no, __func__);

	return ret;
}

static int tcc_i2s_hw_params_sub_set_i2s_mode(
	const struct snd_pcm_substream *substream,
	const struct tcc_i2s_t *i2s,
	uint32_t channels,
	snd_pcm_format_t format)
{
	int32_t ret = 0;
	enum TCC_DAI_FMT tcc_fmt;

	do{
		if (i2s->tdm_mode == TRUE) {
			switch (i2s->dai_fmt & (uint32_t)SND_SOC_DAIFMT_FORMAT_MASK) {
			case SND_SOC_DAIFMT_I2S:
				if (channels != i2s->tdm_slots) {
					i2s_dai_err("I2S TDM Mode, slots(%d) != channels(%d)\n",
						i2s->tdm_slots,
						channels);
					ret = -EINVAL;
					break;
				}
				break;
			case SND_SOC_DAIFMT_DSP_A:
			case SND_SOC_DAIFMT_DSP_B:
			{
#if !defined(CONFIG_ARCH_TCC805X)
					uint32_t fmt_bitwidth =
						(format == SNDRV_PCM_FORMAT_S24_LE) ?
						24u : 16u;
					tcc_dai_set_dsp_tdm_word_len(
							i2s->dai_reg,
							fmt_bitwidth);
#endif
					if (channels < 2u) {
						i2s_dai_err("DSP A or B TDM Mode, channels must be greater than 1.\n");
						ret = -EINVAL;
						break;
					}

#if !defined(CONFIG_ARCH_TCC805X)
	/* After TCC805x CS,
	 *  audio IPs does not set the vaild data ends in MCCR1 register.
	 */
					if (i2s->tdm_multi_port) {
						tcc_dai_set_dsp_tdm_mode_valid_data(
								i2s->dai_reg,
								i2s->tdm_slots,
								i2s->tdm_slot_width);
					} else {
						tcc_dai_set_dsp_tdm_mode_valid_data(
								i2s->dai_reg,
								channels,
								i2s->tdm_slot_width);
					}
#endif
				}
				break;
			default:
				i2s_dai_warn("[%d] does not supported\n", i2s->blk_no);
				ret = -ENOTSUPP;
				break;
			}

			if (ret != 0) {
				continue;
			}

		} else {
			if ((i2s->dai_fmt & (uint32_t)SND_SOC_DAIFMT_FORMAT_MASK) ==
				(uint32_t)SND_SOC_DAIFMT_RIGHT_J) {
				tcc_dai_set_right_j_mode(
					i2s->dai_reg,
					(uint32_t)i2s->bclk_ratio);
			}
		}

		tcc_fmt = (format == SNDRV_PCM_FORMAT_S24_LE) ?
			TCC_DAI_LSB_24 : TCC_DAI_LSB_16;
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			i2s_dai_dbg("[%d] %s set_tx_format\n", i2s->blk_no, __func__);
			tcc_dai_set_tx_format(i2s->dai_reg, tcc_fmt);
		} else {
			i2s_dai_dbg("[%d] %s set_rx_format\n", i2s->blk_no, __func__);
			tcc_dai_set_rx_format(i2s->dai_reg, tcc_fmt);
		}

		if (i2s->tdm_mode == TRUE) {
			i2s_dai_dbg("[%d] %s set_single-port_mode\n", i2s->blk_no, __func__);
			tcc_dai_set_multiport_mode(i2s->dai_reg, i2s->tdm_multi_port);
		} else {
			if (channels > 2u) {
				i2s_dai_dbg("[%d] %s set_multiport_mode\n",
								i2s->blk_no, __func__);
				tcc_dai_set_multiport_mode(i2s->dai_reg, TRUE);
			} else {
				i2s_dai_dbg("[%d] %s set_single-port_mode\n",
								i2s->blk_no, __func__);
				tcc_dai_set_multiport_mode(i2s->dai_reg, FALSE);
			}
		}

#if defined(CONFIG_ARCH_TCC805X)
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			uint32_t dai_fmt_msk = 0;

			if (i2s->tdm_mode == TRUE) {
				tcc_dai_set_dsp_tdm_mode_rx_channel(
						i2s->dai_reg,
						(uint32_t)i2s->tdm_slots);
			} else {
				tcc_dai_set_dsp_tdm_mode_rx_channel(
						i2s->dai_reg,
						2);
			}

			dai_fmt_msk = i2s->dai_fmt;
			dai_fmt_msk &= (uint32_t)SND_SOC_DAIFMT_MASTER_MASK;

			switch (dai_fmt_msk) {
			case SND_SOC_DAIFMT_CBS_CFM:
				/* codec clk slave & FRM master */
			case SND_SOC_DAIFMT_CBS_CFS:
				/* codec clk & FRM slave */
					tcc_dai_set_rx_bclk_delay(
						i2s->dai_reg, i2s->rx_bclk_delay);

					if ((i2s->tdm_mode == TRUE)
						&& (i2s->rx_bclk_delay)) {
						tcc_dai_set_dsp_tdm_mode_rx_early(
								i2s->dai_reg,
								!(i2s->tdm_late_mode));
					} else {
						tcc_dai_set_dsp_tdm_mode_rx_early(
								i2s->dai_reg,
								FALSE);
					}
				break;
			default:
				tcc_dai_set_rx_bclk_delay(
						i2s->dai_reg, FALSE);
				tcc_dai_set_dsp_tdm_mode_rx_early(
						i2s->dai_reg, FALSE);
				break;
			}
		}
#endif//TCC805x
	}while (false);
	return ret;
}

static int tcc_i2s_hw_params_sub_set_clk_and_filter(
	struct snd_soc_dai *dai,
	struct tcc_i2s_t *i2s,
	uint32_t sample_rate)
{
	int32_t ret = 0;
	unsigned long mclk = 0;

	do{
	/* Workaround Code for TCC803X, TCC899X and TCC901X
	 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
	 * So, we should always restore DCLKDIV value while write that
	 * value to register
	 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) || \
		defined(CONFIG_ARCH_TCC901X)
		if ((i2s->block_type == (uint32_t)DAI_BLOCK_STEREO_TYPE) ||
			(i2s->block_type == (uint32_t)DAI_BLOCK_9_1CH_TYPE)) {
			i2s->regs_backup.dclkdiv =
				tcc_dai_set_clk_mode(
					i2s->dai_reg,
					i2s->mclk_div,
					i2s->bclk_ratio,
					i2s->tdm_mode);
		} else
#endif
		{
#if defined(CONFIG_ARCH_TCC805X)
			(void) tcc_dai_set_clk_mode(
				i2s->dai_reg,
				i2s->mclk_div,
				i2s->bclk_ratio,
				i2s->tdm_slot_width,
				i2s->tdm_mode);
#else
			(void) tcc_dai_set_clk_mode(
				i2s->dai_reg,
				i2s->mclk_div,
				i2s->bclk_ratio,
				i2s->tdm_mode);
#endif
		}

		mclk = calc_mclk(i2s, sample_rate);
		if (mclk <= 0u) {
			i2s_dai_err("%s - DAI peri max frequency is %uHz.\n",
				__func__,
				TCC_DAI_MAX_FREQ);
			ret = -ENOTSUPP;
			continue;
		}

		i2s_dai_dbg("[%d] %s - mclk : %ld\n", i2s->blk_no, __func__, mclk);
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
		if (i2s->bclk_ratio == (uint16_t)TCC_DAI_BCLK_RATIO_512) {
			if (mclk > (uint32_t)TCC_DAI_DP_LINK_MAX_FREQ) {
				i2s_dai_err(
					"%s - DAI peri for DP Link max frequency is %dHz. but you try %luHz\n",
					__func__,
					TCC_DAI_DP_LINK_MAX_FREQ,
					mclk);
				ret = -ENOTSUPP;
				continue;
			}

		} else {
#endif
			if (mclk > (uint32_t)TCC_DAI_MAX_FREQ) {
				i2s_dai_err("%s - DAI peri max frequency is %dHz. but you try %luHz\n",
					__func__,
					TCC_DAI_MAX_FREQ, mclk);
				ret = -ENOTSUPP;
				continue;
			}
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
		}
#endif

		if (i2s->clock_rate != mclk) {
			i2s->sample_rate = sample_rate;
			(void) tcc_i2s_set_sysclk(dai, (int)TCC_DAI_MCLK, (unsigned int)mclk, SND_SOC_CLOCK_OUT);
		}

		if (i2s->is_updated == TRUE) {
			struct dai_reg_t regs = {0};

			tcc_dai_reg_backup(i2s->dai_reg, &regs);
	/* Workaround Code for TCC803X, TCC899X and TCC901X
	 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
	 * So, we should always restore DCLKDIV value while write that
	 * value to register
	 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) ||\
		defined(CONFIG_ARCH_TCC901X)
			if ((i2s->block_type ==
				(uint32_t)DAI_BLOCK_STEREO_TYPE) ||
				(i2s->block_type ==
				(uint32_t)DAI_BLOCK_9_1CH_TYPE)) {
				regs.dclkdiv = i2s->regs_backup.dclkdiv;
			}
#endif
			clk_disable_unprepare(i2s->dai_hclk);
			(void) clk_prepare_enable(i2s->dai_hclk);
			tcc_dai_reg_restore(i2s->dai_reg, &regs);

			tcc_dai_enable(i2s->dai_reg, TRUE);
			i2s->is_updated = FALSE;
		}

		if (i2s->clk_continuous == FALSE) {
			i2s_dai_dbg("[%d] %s dai_enable\n", i2s->blk_no, __func__);
			tcc_dai_enable(i2s->dai_reg, TRUE);
		}

		if (((uint32_t)DAI_AUDIO_CLK_FILTER_TYPE & i2s->audio_filter_bit) != 0u) {
			i2s_dai_dbg("[%d] %s audio clk filter enable\n",
						i2s->blk_no, __func__);
			tcc_dai_set_audio_filter_enable(i2s->dai_reg, TRUE);
		} else {
			i2s_dai_dbg("[%d] %s audio clk filter disable\n",
						i2s->blk_no, __func__);
			tcc_dai_set_audio_filter_enable(i2s->dai_reg, FALSE);
		}
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
		if (((uint32_t)DAI_AUDIO_DATA_FILTER_TYPE & i2s->audio_filter_bit) != 0u) {
			i2s_dai_dbg("[%d] %s audio data filter enable\n",
						i2s->blk_no, __func__);
			tcc_dai_set_audio_data_filter_enable(i2s->dai_reg, TRUE);
		} else {
			i2s_dai_dbg("[%d] %s audio data filter enable\n",
						i2s->blk_no, __func__);
			tcc_dai_set_audio_data_filter_enable(i2s->dai_reg, FALSE);
		}
#endif
	}while (false);
	return ret;
}

static int tcc_i2s_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);

	uint32_t channels = params_channels(params);
	snd_pcm_format_t format = params_format(params);
	uint32_t sample_rate = params_rate(params);
	int32_t ret = 0;

#if	(CHECK_I2S_HW_PARAM_ELAPSED_TIME == 1)
	struct timeval start, end;
	u64 elapsed_usecs64;
	unsigned int elapsed_usecs;
#endif

	i2s_dai_func_dbg("[%d] ++ %s ++\n", i2s->blk_no, __func__);

	i2s_dai_dbg("[%d] %s - format : 0x%08x\n",
		i2s->blk_no,
		__func__,
		format);
	i2s_dai_dbg("[%d] %s - sample_rate : %d\n",
		i2s->blk_no,
		__func__,
		sample_rate);
	i2s_dai_dbg("[%d] %s - channels : %d\n",
		i2s->blk_no,
		__func__,
		channels);

#if	(CHECK_I2S_HW_PARAM_ELAPSED_TIME == 1)
	do_gettimeofday(&start);
#endif
	do{
		ret = check_i2s_hw_params(substream, params, dai);
		if (ret == -ENOTSUPP) {
			continue;
		}

		if (i2s->dai_fmt == 0u) {
			i2s_dai_err("audio%d can't use because tcc_i2s_set_dai_fmt failed\n",
				i2s->blk_no);
			ret = -EINVAL;
			continue;
		}

		/* Rx2Tx loopback Disable */
		i2s_dai_dbg("[%d] %s Rx2Tx mode Disable\n", i2s->blk_no, __func__);
		tcc_dai_rx2tx_loopback_enable(i2s->dai_reg, FALSE);

	    ret = tcc_i2s_hw_params_sub_set_i2s_mode(substream, i2s, channels, format);
		if (ret != 0) {
			continue;
		}

		ret = tcc_i2s_hw_params_sub_set_clk_and_filter(dai, i2s, sample_rate);
		if (ret != 0) {
			continue;
		}
	}while (false);

#if	(CHECK_I2S_HW_PARAM_ELAPSED_TIME == 1)
	do_gettimeofday(&end);

	elapsed_usecs64 = timeval_to_ns(&end) - timeval_to_ns(&start);
	do_div(elapsed_usecs64, NSEC_PER_USEC);
	elapsed_usecs = elapsed_usecs64;

	i2s_dai_dbg(" i2s hw_params's elapsed time : %03d usec\n",
		elapsed_usecs);
#endif

	i2s_dai_func_dbg("[%d] -- %s --\n", i2s->blk_no, __func__);

	return ret;
}

static void tcc_i2s_fifo_clear_delay(const gfp_t gfp)
{
	//FIFO clear delay = IOBUS clock period * FIFO size
	if (gfp == GFP_ATOMIC) {
		udelay(1000);
	} else  {
		msleep(1u);
	}
}

static int tcc_i2s_hw_free(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	const struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);

	i2s_dai_func_dbg("[%d] ++ %s - active:%d ++\n",
		i2s->blk_no,
		__func__,
		snd_soc_dai_active(dai));

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (tcc_dai_enable_check(i2s->dai_reg) > 0u) {
			if (i2s->have_fifo_clear_bit != 0u) {
				tcc_dai_tx_fifo_clear(i2s->dai_reg);
				tcc_i2s_fifo_clear_delay(GFP_ATOMIC);
				tcc_dai_tx_fifo_release(i2s->dai_reg);
				i2s_dai_dbg("%s - tx fifo clr\n", __func__);
			}
		}
	} else{ /* SNDRV_PCM_STREAM_CAPTURE */
		if (tcc_dai_enable_check(i2s->dai_reg) > 0u) {
			if (i2s->have_fifo_clear_bit != 0u) {
				tcc_dai_rx_fifo_clear(i2s->dai_reg);
				tcc_i2s_fifo_clear_delay(GFP_ATOMIC);
				tcc_dai_rx_fifo_release(i2s->dai_reg);
				i2s_dai_dbg("%s - rx fifo clr\n", __func__);
			}
		}
	}

#if defined(CONFIG_ARCH_TCC802X) || defined(CONFIG_ARCH_TCC898X)
	if (i2s->tdm_mode == TRUE) {
		struct dai_reg_t regs = {0};

		tcc_dai_reg_backup(i2s->dai_reg, &regs);

		(void) clk_disable_unprepare(i2s->dai_hclk);
		(void) clk_prepare_enable(i2s->dai_hclk);

		tcc_dai_reg_restore(i2s->dai_reg, &regs);
	}
#endif

	i2s_dai_func_dbg("[%d] -- %s --\n", i2s->blk_no, __func__);

	return 0;
}


static int tcc_i2s_trigger(
	struct snd_pcm_substream *substream,
	int cmd,
	struct snd_soc_dai *dai)
{
	const struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	i2s_dai_func_dbg("[%d] ++ %s ++\n", i2s->blk_no, __func__);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			i2s_dai_dbg("[%d] %s ", i2s->blk_no, __func__);
			i2s_dai_dbg("- playback start.\n");
			if (i2s->have_fifo_clear_bit != 0u) {
				if(tcc_dai_enable_check(i2s->dai_reg) > 0u) {
					tcc_dai_tx_fifo_clear(i2s->dai_reg);
					tcc_dai_tx_enable(i2s->dai_reg, TRUE);
					tcc_i2s_fifo_clear_delay(GFP_ATOMIC);
					tcc_dai_tx_fifo_release(i2s->dai_reg);
					i2s_dai_dbg(" tx fifo clr done!!!\n");
				}
			} else {
				tcc_dai_tx_enable(i2s->dai_reg, TRUE);
				i2s_dai_dbg("- fifo_clear not support.\n");
			}
		} else {
			i2s_dai_dbg("[%d] %s ", i2s->blk_no, __func__);
			i2s_dai_dbg("- capture start.\n");
			if (i2s->have_fifo_clear_bit != 0u) {
				if(tcc_dai_enable_check(i2s->dai_reg) > 0u) {
					tcc_dai_rx_fifo_clear(i2s->dai_reg);
					tcc_dai_rx_enable(i2s->dai_reg, TRUE);
					tcc_i2s_fifo_clear_delay(GFP_ATOMIC);
					tcc_dai_rx_fifo_release(i2s->dai_reg);
					i2s_dai_dbg(" rx fifo clr done!!!\n");
				}
			} else{
				tcc_dai_rx_enable(i2s->dai_reg, TRUE);
				i2s_dai_dbg("- fifo_clear not support.\n");
			}
		}

		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			i2s_dai_dbg("[%d] %s ", i2s->blk_no, __func__);
			i2s_dai_dbg("- playback stop.\n");
			tcc_dai_tx_enable(i2s->dai_reg, FALSE);
		} else {
			i2s_dai_dbg("[%d] %s ", i2s->blk_no, __func__);
			i2s_dai_dbg("- capture stop.\n");
			tcc_dai_rx_enable(i2s->dai_reg, FALSE);
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	i2s_dai_func_dbg("[%d] -- %s --\n", i2s->blk_no, __func__);

	return ret;
}

static int tcc_i2s_mute_stream(struct snd_soc_dai *dai,
								int mute,
								int sndrv_pcm_stream)
{
	const struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	i2s_dai_func_dbg("[%d] ++ %s ++\n", i2s->blk_no, __func__);

	if (sndrv_pcm_stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (mute != 0) {
			tcc_dai_set_dao_mask(
				i2s->dai_reg,
				TRUE,
				TRUE,
				TRUE,
				TRUE,
				TRUE);
		} else {
			tcc_dai_set_dao_mask(
				i2s->dai_reg,
				FALSE,
				FALSE,
				FALSE,
				FALSE,
				FALSE);
		}
	} else {
		i2s_dai_dbg("[%d] %s - rx mute\n", i2s->blk_no, __func__);
		if(mute != 0) {
			tcc_dai_set_rx_mute(i2s->dai_reg, TRUE);
		} else {
			tcc_dai_set_rx_mute(i2s->dai_reg, FALSE);
		}
	}

	i2s_dai_func_dbg("[%d] -- %s --\n", i2s->blk_no, __func__);

	return ret;
}


static int tcc_i2s_set_pll
	(struct snd_soc_dai *dai,
	int pll_id,
	int source,
	unsigned int freq_in,
	unsigned int freq_out)
{
	int ret = 0;

	i2s_dai_func_dbg("%s - Not support operation", __func__);

	return ret;
}

static int tcc_i2s_xlate_tdm_slot_mask
	(unsigned int slots,
	unsigned int *tx_mask,
	unsigned int *rx_mask)
{
	int ret = 0;

	i2s_dai_func_dbg("%s - Not support operation", __func__);

	return ret;
}

static int tcc_i2s_set_channel_map
	(struct snd_soc_dai *dai,
	unsigned int tx_num,
	unsigned int *tx_slot,
	unsigned int rx_num,
	unsigned int *rx_slot)
{
	int ret = 0;

	i2s_dai_func_dbg("%s - Not support operation", __func__);

	return ret;
}

static int tcc_i2s_set_tristate
	(struct snd_soc_dai *dai,
	int tristate)
{
	int ret = 0;

	i2s_dai_func_dbg("%s - Not support operation", __func__);

	return ret;
}

static int tcc_i2s_prepare
	(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	int ret = 0;

	i2s_dai_func_dbg("%s - Not support operation", __func__);

	return ret;
}

static int tcc_i2s_bespoke_trigger
	(struct snd_pcm_substream *substream,
	int bespoke,
	struct snd_soc_dai *dai)
{
	int ret = 0;

	i2s_dai_func_dbg("%s - Not support operation", __func__);

	return ret;
}

#if 0
static snd_pcm_sframes_t tcc_i2s_delay
	(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	snd_pcm_sframes_t ret = 0;

	i2s_dai_dbg("%s - Not support operation", __func__);

	return ret;
}
#endif

static struct snd_soc_dai_ops tcc_i2s_ops = {
	.set_clkdiv     = tcc_i2s_set_clkdiv,
	.set_bclk_ratio = tcc_i2s_set_bclk_ratio,
	.set_fmt        = tcc_i2s_set_dai_fmt,
	.startup        = tcc_i2s_startup,
	.shutdown       = tcc_i2s_shutdown,
	.hw_params      = tcc_i2s_hw_params,
	.hw_free        = tcc_i2s_hw_free,
	.trigger        = tcc_i2s_trigger,
	.set_tdm_slot   = tcc_i2s_set_tdm_slot,
	.set_sysclk		= tcc_i2s_set_sysclk,
	.mute_stream	= tcc_i2s_mute_stream,

	/*Do not use below operations*/
	.set_pll				= tcc_i2s_set_pll,
	.xlate_tdm_slot_mask	= tcc_i2s_xlate_tdm_slot_mask,
	.set_channel_map		= tcc_i2s_set_channel_map,
	.set_tristate			= tcc_i2s_set_tristate,
	.prepare				= tcc_i2s_prepare,
	.bespoke_trigger		= tcc_i2s_bespoke_trigger,
	//.delay					= tcc_i2s_delay
};

static uint32_t tcc_i2s_is_active(
	struct snd_soc_component *comp,
	const char *set_change)
{
	const struct tcc_i2s_t *i2s =
		(struct tcc_i2s_t *)snd_soc_component_get_drvdata(comp);
	uint32_t is_active = 0;

	is_active = snd_soc_component_active(comp);

	if (is_active > 0U) {
		i2s_dai_err("%s doesn't change while I2S-%d is activated.",
			set_change,
			i2s->blk_no);
	}

	return is_active;
}

static int get_bclk_ratio(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *comp =
		snd_soc_kcontrol_component(kcontrol);
	const struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(comp);
	ucontrol->value.integer.value[0] =
		(i2s->bclk_ratio == 32u) ? 0 :
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
		(i2s->bclk_ratio == 48u) ? 1 :
		(i2s->bclk_ratio == 64u) ? 2 : 3;
#else
		(i2s->bclk_ratio == 48u) ? 1 : 2;
#endif

	return 0;
}

static int set_bclk_ratio(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *comp =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(comp);
	int ret = 0;

	if (tcc_i2s_is_active(comp, __func__)  > 0U) {
		i2s_dai_err("%s fail\n", __func__);
		ret = -EINVAL;
	} else {
	i2s->bclk_ratio =
		(ucontrol->value.integer.value[0] == 0) ? 32u :
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
		(ucontrol->value.integer.value[0] == 1) ? 48u :
		(ucontrol->value.integer.value[0] == 2) ? 64u : 512u;
#else
		(ucontrol->value.integer.value[0] == 1) ? 48u : 64u;
#endif
	}
	return ret;
}

static char const *bclk_ratio_texts[] = {
	"32fs",
	"48fs",
	"64fs",
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	"512fs for DP transfer",
#endif
};

static const struct soc_enum bclk_ratio_enum[] = {
	SOC_ENUM_SINGLE_EXT(
		(TCC_AUDIO_ARRAY_SIZE(bclk_ratio_texts)),
		(bclk_ratio_texts)),
};

static int get_mclk_div(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *comp =
		snd_soc_kcontrol_component(kcontrol);
	const struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(comp);

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
	ucontrol->value.integer.value[0] =
		(i2s->mclk_div == 4u)  ? 0 :
		(i2s->mclk_div == 6u)  ? 1 :
		(i2s->mclk_div == 8u)  ? 2 :
		(i2s->mclk_div == 16u) ? 3 :
		(i2s->mclk_div == 1u)  ? 4 :
		(i2s->mclk_div == 2u)  ? 5 :
		(i2s->mclk_div == 24u) ? 6 :
		(i2s->mclk_div == 32u) ? 7 :
		(i2s->mclk_div == 48u) ? 8 : 9;
#else
	ucontrol->value.integer.value[0] =
		(i2s->mclk_div == 4u)  ? 0 :
		(i2s->mclk_div == 6u)  ? 1 :
		(i2s->mclk_div == 8u)  ? 2 :
		(i2s->mclk_div == 16u) ? 3 :
		(i2s->mclk_div == 1u)  ? 4 : 5;
#endif

	return 0;
}

static int set_mclk_div(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *compo =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		(struct tcc_i2s_t *)snd_soc_component_get_drvdata(compo);
	int ret = 0;

	if (tcc_i2s_is_active(compo, __func__) > 0U) {
		i2s_dai_err("%s fail\n", __func__);
		ret = -EINVAL;
	} else {
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
		defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
		defined(CONFIG_ARCH_TCC901X)
		i2s->mclk_div =
			(ucontrol->value.integer.value[0] == 0) ? 4u :
			(ucontrol->value.integer.value[0] == 1) ? 6u :
			(ucontrol->value.integer.value[0] == 2) ? 8u :
			(ucontrol->value.integer.value[0] == 3) ? 16u :
			(ucontrol->value.integer.value[0] == 4) ? 1u :
			(ucontrol->value.integer.value[0] == 5) ? 2u :
			(ucontrol->value.integer.value[0] == 6) ? 24u :
			(ucontrol->value.integer.value[0] == 7) ? 32u :
			(ucontrol->value.integer.value[0] == 8) ? 48u : 64u;
#else
		i2s->mclk_div =
			(ucontrol->value.integer.value[0] == 0) ? 4u :
			(ucontrol->value.integer.value[0] == 1) ? 6u :
			(ucontrol->value.integer.value[0] == 2) ? 8u :
			(ucontrol->value.integer.value[0] == 3) ? 16u :
			(ucontrol->value.integer.value[0] == 4) ? 1u : 2u;
#endif
	}
	return ret;
}

static char const *mclk_div_texts[] = {
	"4div",
	"6div",
	"8div",
	"16div",
	"1div_TDM",
	"2div_TDM",
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
	"24div",
	"32div",
	"48div",
	"64div",
#endif
};

static const struct soc_enum mclk_div_enum[] = {
	SOC_ENUM_SINGLE_EXT((TCC_AUDIO_ARRAY_SIZE(mclk_div_texts)),
		(mclk_div_texts)),
};

#if defined(CONFIG_ARCH_TCC805X)
static int get_rx_bclk_delay(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *comp =
		snd_soc_kcontrol_component(kcontrol);
	const struct tcc_i2s_t *i2s =
		(struct tcc_i2s_t *)snd_soc_component_get_drvdata(comp);

	ucontrol->value.integer.value[0] =
		(i2s->rx_bclk_delay == TRUE)? 1:0;

	return 0;
}

static int set_rx_bclk_delay(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *compo =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		(struct tcc_i2s_t *)snd_soc_component_get_drvdata(compo);
	int ret = 0;

	if (tcc_i2s_is_active(compo, __func__) > 0U) {
		i2s_dai_err("%s fail\n", __func__);
		ret = -EINVAL;
	} else {
		i2s_dai_dbg("%s success\n", __func__);
		i2s->rx_bclk_delay =
			(ucontrol->value.integer.value[0] == 1)? TRUE:FALSE;
	}
	return ret;
}

static char const *rx_bclk_delay_texts[] = {
	"Disable",
	"Enable",
};

static const struct soc_enum rx_bclk_delay_enum[] = {
	SOC_ENUM_SINGLE_EXT(
		(TCC_AUDIO_ARRAY_SIZE(rx_bclk_delay_texts)),
		(rx_bclk_delay_texts)),
};
#endif

static int get_audio_filter(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *comp =
		snd_soc_kcontrol_component(kcontrol);
	const struct tcc_i2s_t *i2s =
		(struct tcc_i2s_t *)snd_soc_component_get_drvdata(comp);

	ucontrol->value.integer.value[0] = (long)i2s->audio_filter_bit;

	return 0;
}
static int set_audio_filter(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value  *ucontrol)
{
	struct snd_soc_component *comp =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		(struct tcc_i2s_t *)snd_soc_component_get_drvdata(comp);
	long control_value;
	int ret = 0;

	if (tcc_i2s_is_active(comp, __func__) > 0U) {
		i2s_dai_err("%s fail\n", __func__);
		ret = -EINVAL;
	} else {
		control_value = ucontrol->value.integer.value[0];

		i2s->audio_filter_bit = (uint32_t) (si_to_ui(sl_to_si(control_value)) & 0xFFU);

		i2s->is_updated = TRUE;
	}
	return ret;
}

static char const *audio_filter_texts[] = {
	"Disable",
	"Clock Filter Only Enable",
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	"Data Filter Only Enable",
	"Clock and Data Filter Enable",
#endif
};

static const struct soc_enum audio_filter_enum[] = {
	SOC_ENUM_SINGLE_EXT(
		(TCC_AUDIO_ARRAY_SIZE(audio_filter_texts)),
		(audio_filter_texts)),
};

static int get_tx2rx_loopback_mode(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp =
		snd_soc_kcontrol_component(kcontrol);
	const struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(comp);

	ucontrol->value.integer.value[0] =
		(long)tcc_dai_tx2rx_loopback_check(i2s->dai_reg);

	return 0;
}

static int set_tx2rx_loopback_mode(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(comp);
	int ret = 0;

	if (tcc_i2s_is_active(comp, __func__) > 0U) {
		i2s_dai_err("%s fail\n", __func__);
		ret = -EINVAL;
	} else {
		tcc_dai_tx2rx_loopback_enable(
				i2s->dai_reg,
				(bool)ucontrol->value.integer.value[0]);

		i2s->is_updated = TRUE;
	}
	return ret;
}

static int get_rx2tx_loopback_mode(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp =
		snd_soc_kcontrol_component(kcontrol);
	const struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(comp);

	ucontrol->value.integer.value[0] =
		(long)tcc_dai_rx2tx_loopback_check(i2s->dai_reg);

	return 0;
}

static int set_rx2tx_loopback_mode(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *compo =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(compo);
	int ret = 0;

	if (tcc_i2s_is_active(compo, __func__) > 0U) {
		i2s_dai_err("%s fail\n", __func__);
		ret = -EINVAL;
	} else {
		tcc_dai_rx2tx_loopback_enable(
				i2s->dai_reg,
				(bool)ucontrol->value.integer.value[0]);

		i2s->is_updated = TRUE;
	}
	return ret;
}

static int get_tdm_late_mode(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp =
		snd_soc_kcontrol_component(kcontrol);
	const struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(comp);

	ucontrol->value.integer.value[0] = (long)i2s->tdm_late_mode;

	return 0;
}

static int set_tdm_late_mode(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(comp);
	int ret = 0;

	if (tcc_i2s_is_active(comp, __func__) > 0U) {
		i2s_dai_err("%s fail\n", __func__);
		ret = -EINVAL;
	} else {
		i2s->tdm_late_mode = (bool)ucontrol->value.integer.value[0];

		i2s->is_updated = TRUE;
	}
	return ret;
}

static int get_tdm_multi_port_mode(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp =
		snd_soc_kcontrol_component(kcontrol);
	const struct tcc_i2s_t *i2s =
		(struct tcc_i2s_t *)snd_soc_component_get_drvdata(comp);

	ucontrol->value.integer.value[0] = (long)i2s->tdm_multi_port;

	return 0;
}

static int set_tdm_multi_port_mode(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp =
		snd_soc_kcontrol_component(kcontrol);
	struct tcc_i2s_t *i2s =
		(struct tcc_i2s_t *)snd_soc_component_get_drvdata(comp);

	i2s->tdm_multi_port = (bool)ucontrol->value.integer.value[0];

	i2s->is_updated = TRUE;

	return 0;
}

static const struct snd_kcontrol_new tcc_i2s_snd_controls[] = {
	SOC_ENUM_EXT(("BCLK RATIO"), (bclk_ratio_enum[0]),
		(get_bclk_ratio), (set_bclk_ratio)),
	SOC_ENUM_EXT(("MCLK DIV"),   (mclk_div_enum[0]),
		(get_mclk_div),   (set_mclk_div)),
	SOC_ENUM_EXT(("Audio Filter"),   (audio_filter_enum[0]),
		(get_audio_filter),   (set_audio_filter)),
	SOC_SINGLE_BOOL_EXT(("Tx to Rx Internal Loopback Mode"), (0),
		(get_tx2rx_loopback_mode), (set_tx2rx_loopback_mode)),
	SOC_SINGLE_BOOL_EXT(("Rx to Tx Internal Loopback Mode"), (0),
		(get_rx2tx_loopback_mode), (set_rx2tx_loopback_mode)),
	SOC_SINGLE_BOOL_EXT(("TDM Late Mode"), (0),
		(get_tdm_late_mode), (set_tdm_late_mode)),
	SOC_SINGLE_BOOL_EXT(("TDM Multi Port Mode"), (0),
		(get_tdm_multi_port_mode), (set_tdm_multi_port_mode)),
#if defined(CONFIG_ARCH_TCC805X)
	SOC_ENUM_EXT(("Rx BCLK Delay in Master Mode"),
		(rx_bclk_delay_enum[0]), (get_rx_bclk_delay), (set_rx_bclk_delay)),
#endif
};

static int tcc_i2s_suspend
	(struct snd_soc_component *component)
{
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) ||\
		defined(CONFIG_ARCH_TCC901X)
	uint32_t dclkdiv_backup = 0;
#endif
	struct pinctrl *tcc_pctrl;
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(component);

	i2s_dai_func_dbg("[%d] ++ %s ++\n", i2s->blk_no, __func__);

	tcc_pctrl = pinctrl_get_select(component->dev, "idle");
	if (IS_ERR(tcc_pctrl)) {
		i2s_dai_err("%s : pinctrl suspend error[0x%p]\n",
			__func__,
			tcc_pctrl);
	}


/* Workaround Code for TCC803X, TCC899X and TCC901X
 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
 * So, we should always restore DCLKDIV value while write that
 * value to register
 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) ||\
	defined(CONFIG_ARCH_TCC901X)
	if ((i2s->block_type == (uint32_t)DAI_BLOCK_STEREO_TYPE) ||
		(i2s->block_type == (uint32_t)DAI_BLOCK_9_1CH_TYPE)) {
		dclkdiv_backup = i2s->regs_backup.dclkdiv;
	}
#endif
	tcc_dai_reg_backup(i2s->dai_reg, &i2s->regs_backup);

/* Workaround Code for TCC803X, TCC899X and TCC901X
 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
 * So, we should always restore DCLKDIV value while write that
 * value to register
 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) ||\
	defined(CONFIG_ARCH_TCC901X)
	if ((i2s->block_type == (uint32_t)DAI_BLOCK_STEREO_TYPE) ||
		(i2s->block_type == (uint32_t)DAI_BLOCK_9_1CH_TYPE)) {
		i2s->regs_backup.dclkdiv = dclkdiv_backup;
	}
#endif
	i2s_dai_func_dbg("[%d] -- %s --\n", i2s->blk_no, __func__);

	return 0;
}

static int tcc_i2s_resume
	(struct snd_soc_component *component)
{
	const struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(component);
	struct pinctrl *tcc_pctrl;

	i2s_dai_func_dbg("[%d] ++ %s ++\n", i2s->blk_no, __func__);

	tcc_pctrl = pinctrl_get_select(component->dev, "default");
	if (IS_ERR(tcc_pctrl)) {
		i2s_dai_err("%s : pinctrl resume error[0x%p]\n",
			__func__,
			tcc_pctrl);
	}

#if defined(CONFIG_ARCH_TCC802X)
	tcc_gfb_i2s_portcfg(i2s->pcfg_reg, &i2s->portcfg);
#endif

	tcc_dai_reg_restore(i2s->dai_reg, &i2s->regs_backup);

	return 0;
}

static int tcc_i2s_open(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	const struct snd_soc_pcm_runtime *rtd = (struct snd_soc_pcm_runtime *)substream->private_data;
	struct snd_soc_dai *cpu_dai = (struct snd_soc_dai *)asoc_rtd_to_cpu(rtd, (0U));
	struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)snd_soc_component_get_drvdata(component);

	i2s_dai_func_dbg("[%d] -- %s --\n", i2s->blk_no, __func__);

	i2s->dma_info.dev_type =
		(i2s->block_type == (uint32_t)DAI_BLOCK_STEREO_TYPE) ?
		TCC_ADMA_I2S_STEREO :
		(i2s->block_type == (uint32_t)DAI_BLOCK_7_1CH_TYPE) ?
		TCC_ADMA_I2S_7_1CH : TCC_ADMA_I2S_9_1CH;

	snd_soc_dai_set_dma_data(cpu_dai, substream, &i2s->dma_info);

	return 0;
}

static const struct snd_soc_component_driver tcc_i2s_component_drv = {
	.name = "tcc-i2s",
	.controls = tcc_i2s_snd_controls,
	.num_controls = TCC_AUDIO_ARRAY_SIZE(tcc_i2s_snd_controls),
	.open = tcc_i2s_open,
	.suspend = tcc_i2s_suspend,
	.resume	= tcc_i2s_resume,
};

static struct snd_soc_dai_driver tcc_i2s_dai_drv[DAI_BLOCK_TYPE_MAX] = {
	[DAI_BLOCK_STEREO_TYPE] = {
		.name = "tcc-i2s",
		.playback = {
			.stream_name = "I2S-Playback",
			.channels_min = 1,
			.channels_max = 16,
			.rate_min = 8000,
			.rate_max = 192000,
			.rates = (SNDRV_PCM_RATE_8000_192000
				| SNDRV_PCM_RATE_KNOT),
			.formats =
				(SNDRV_PCM_FMTBIT_S16_LE
				|SNDRV_PCM_FMTBIT_S24_LE),
		},
		.capture = {
			.stream_name = "I2S-Capture",
			.channels_min = 1,
			.channels_max = 16,
			.rate_min = 8000,
			.rate_max = 192000,
			.rates = (SNDRV_PCM_RATE_8000_192000
				| SNDRV_PCM_RATE_KNOT),
			.formats =
				(SNDRV_PCM_FMTBIT_S16_LE
				|SNDRV_PCM_FMTBIT_S24_LE),
		},
		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.ops = &tcc_i2s_ops,
	},
	[DAI_BLOCK_7_1CH_TYPE] = {
		.name = "tcc-i2s",
		.playback = {
			.stream_name = "I2S-Playback",
			.channels_min = 1,
			.channels_max = 16,
			.rate_min = 8000,
			.rate_max = 192000,
			.rates = (SNDRV_PCM_RATE_8000_192000
				| SNDRV_PCM_RATE_KNOT),
			.formats =
				(SNDRV_PCM_FMTBIT_S16_LE
				|SNDRV_PCM_FMTBIT_S24_LE),
		},
		.capture = {
			.stream_name = "I2S-Capture",
			.channels_min = 1,
			.channels_max = 16,
			.rate_min = 8000,
			.rate_max = 192000,
			.rates = (SNDRV_PCM_RATE_8000_192000
				| SNDRV_PCM_RATE_KNOT),
			.formats =
				(SNDRV_PCM_FMTBIT_S16_LE
				|SNDRV_PCM_FMTBIT_S24_LE),

		},
		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.ops = &tcc_i2s_ops,
	},
	[DAI_BLOCK_9_1CH_TYPE] = {
		.name = "tcc-i2s",
		.playback = {
			.stream_name = "I2S-Playback",
			.channels_min = 1,
			.channels_max = 10,
			.rate_min = 8000,
			.rate_max = 192000,
			.rates = (SNDRV_PCM_RATE_8000_192000
				| SNDRV_PCM_RATE_KNOT),
			.formats =
				(SNDRV_PCM_FMTBIT_S16_LE
				|SNDRV_PCM_FMTBIT_S24_LE),
		},
		.capture = {
			.stream_name = "I2S-Capture",
			.channels_min = 1,
			.channels_max = 10,
			.rate_min = 8000,
			.rate_max = 192000,
			.rates = (SNDRV_PCM_RATE_8000_192000
				| SNDRV_PCM_RATE_KNOT),
			.formats =
				(SNDRV_PCM_FMTBIT_S16_LE
				|SNDRV_PCM_FMTBIT_S24_LE),
		},
		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.ops = &tcc_i2s_ops,
	},
};

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) ||\
	defined(CONFIG_ARCH_TCC901X)
static void i2s_initialize(struct tcc_i2s_t *i2s)
#else
static void i2s_initialize(const struct tcc_i2s_t *i2s)
#endif
{
	i2s_dai_func_dbg("[%d] ++ %s ++\n", i2s->blk_no, __func__);

	(void) clk_prepare_enable(i2s->dai_hclk);

	(void) clk_set_rate(i2s->dai_pclk, i2s->clock_rate);
	(void) clk_prepare_enable(i2s->dai_pclk);

	i2s_dai_dbg("[%d] %s - i2s->clock_rate:%ld, mclk_div:%u, bclk_ratio:%u tdm_mode:%d\n",
		i2s->blk_no,
		 __func__,
		i2s->clock_rate,
		i2s->mclk_div,
		i2s->bclk_ratio,
		i2s->tdm_mode);

	(void) clk_set_rate(i2s->dai_filter_clk, DEFAULT_DAI_FILTER_CLK_RATE);
	(void) clk_prepare_enable(i2s->dai_filter_clk);

#if defined(CONFIG_ARCH_TCC802X)
	tcc_gfb_i2s_portcfg(i2s->pcfg_reg, &i2s->portcfg);
#endif

	tcc_dai_set_tx_mute(i2s->dai_reg, FALSE);
	tcc_dai_set_rx_mute(i2s->dai_reg, FALSE);

	tcc_dai_damr_enable(i2s->dai_reg, FALSE);
	tcc_dai_set_dao_mask(i2s->dai_reg, TRUE, TRUE, TRUE, TRUE, TRUE);

/* Workaround Code for TCC803X, TCC899X and TCC901X
 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
 * So, we should always restore DCLKDIV value while write that
 * value to register
 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) ||\
	defined(CONFIG_ARCH_TCC901X)
	if ((i2s->block_type == (uint32_t)DAI_BLOCK_STEREO_TYPE) ||
		(i2s->block_type == (uint32_t)DAI_BLOCK_9_1CH_TYPE)) {
		i2s->regs_backup.dclkdiv =
			tcc_dai_set_clk_mode(
				i2s->dai_reg,
				i2s->mclk_div,
				i2s->bclk_ratio,
				i2s->tdm_mode);
	} else
#endif
	{
#if defined(CONFIG_ARCH_TCC805X)
		(void) tcc_dai_set_clk_mode(
			i2s->dai_reg,
			i2s->mclk_div,
			i2s->bclk_ratio,
			i2s->tdm_slot_width,
			i2s->tdm_mode);
#else
		(void) tcc_dai_set_clk_mode(
			i2s->dai_reg,
			i2s->mclk_div,
			i2s->bclk_ratio,
			i2s->tdm_mode);
#endif
	}
	tcc_dai_set_tx_format(i2s->dai_reg, TCC_DAI_LSB_16);
	tcc_dai_set_rx_format(i2s->dai_reg, TCC_DAI_LSB_16);

	tcc_dai_set_multiport_mode(i2s->dai_reg, FALSE);

	tcc_dai_dma_threshold_enable(i2s->dai_reg, TRUE);
	tcc_dai_set_dao_path_sel(i2s->dai_reg, TCC_DAI_PATH_ADMA);

	if ((i2s->audio_filter_bit & (uint32_t)DAI_AUDIO_CLK_FILTER_TYPE) != 0u) {
		tcc_dai_set_audio_filter_enable(i2s->dai_reg, TRUE);
	} else {
		tcc_dai_set_audio_filter_enable(i2s->dai_reg, FALSE);
	}

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	if ((i2s->audio_filter_bit & (uint32_t)DAI_AUDIO_DATA_FILTER_TYPE) != 0u) {
		tcc_dai_set_audio_data_filter_enable(i2s->dai_reg, TRUE);
	} else {
		tcc_dai_set_audio_data_filter_enable(i2s->dai_reg, FALSE);
	}
#endif

	i2s_dai_func_dbg("[%d] -- %s --\n", i2s->blk_no, __func__);
}

static void set_default_configrations(struct tcc_i2s_t *i2s)
{
	i2s->mclk_div = DEFAULT_MCLK_DIV;
	i2s->bclk_ratio = DEFAULT_BCLK_RATIO;

	i2s->clock_rate = ui_to_ull_mul( si_to_ui(DEFAULT_DAI_SAMPLERATE),
		ui_to_ui_mul(si_to_ui(DEFAULT_MCLK_DIV), si_to_ui(DEFAULT_BCLK_RATIO)));
	// these values will be updated by device tree or another functions

	i2s->dai_fmt =
		((uint32_t) SND_SOC_DAIFMT_I2S
		|(uint32_t) SND_SOC_DAIFMT_NB_NF
		|(uint32_t) SND_SOC_DAIFMT_CBS_CFS
		|(uint32_t) SND_SOC_DAIFMT_GATED);

	i2s->clk_continuous = FALSE;

	i2s->tdm_mode = FALSE;
	i2s->frame_invert = FALSE;
	i2s->tdm_late_mode = FALSE;
	i2s->is_updated = FALSE;
	i2s->tdm_slots = 0;
	i2s->tdm_slot_width = 0;
	i2s->tdm_multi_port = FALSE;
}

static int parse_i2s_dt(struct platform_device *pdev, struct tcc_i2s_t *i2s)
{
	uint32_t sample_rate = 0;
	const void *prop;
	int32_t ret = 0;

	i2s->pdev = pdev;

	i2s->blk_no = of_alias_get_id(pdev->dev.of_node, "i2s");

	i2s_dai_dbg(" blk_no : %d\n", i2s->blk_no);
	i2s_dai_func_dbg("[%d] ++ %s ++\n", i2s->blk_no, __func__);

	/* get dai info. */
	i2s->dai_reg = of_iomap(pdev->dev.of_node, 0);
	if (IS_ERR(i2s->dai_reg)) {
		i2s->dai_reg = NULL;
		i2s_dai_err("dai_reg is NULL\n");
		ret = -EINVAL;
	} else {
		i2s_dai_dbg("[%d] dai_reg = %px\n", i2s->blk_no, i2s->dai_reg);

		i2s->dai_pclk = of_clk_get(pdev->dev.of_node, 0);
		i2s->dai_hclk = of_clk_get(pdev->dev.of_node, 1);
		i2s->dai_filter_clk = of_clk_get(pdev->dev.of_node, 2);

		if ((IS_ERR(i2s->dai_pclk)) ||
				(IS_ERR(i2s->dai_hclk)) ||
				(IS_ERR(i2s->dai_filter_clk))){
			i2s_dai_err("dai_pclk or dai_hclk is NULL\n");
			ret = -EINVAL;
		}
#if defined(CONFIG_ARCH_TCC802X)
		i2s->pcfg_reg = of_iomap(pdev->dev.of_node, 1);
		if (IS_ERR((void *)i2s->pcfg_reg)) {
			i2s->pcfg_reg = NULL;
			i2s_dai_err("pcfg_reg is NULL\n");
			ret = -EINVAL;
		}
#endif
	}

	if (ret == 0) {
		prop = of_get_property(pdev->dev.of_node, "pinctrl-0", NULL);
		i2s->is_pinctrl_export = (prop != NULL)? TRUE : FALSE;

		i2s_dai_dbg("[%d] is_pinctrl_export: %d\n",
				i2s->blk_no, i2s->is_pinctrl_export);

		(void) of_property_read_u32(
				pdev->dev.of_node,
				"have-fifo-clear",
				&i2s->have_fifo_clear_bit);

		i2s_dai_dbg("[%d] have_fifo_clear_bit : %u\n",
				i2s->blk_no, i2s->have_fifo_clear_bit);

		ret = of_property_read_u32(
				pdev->dev.of_node,
				"audio-filter",
				&i2s->audio_filter_bit);
		if (ret != 0) {
			i2s->audio_filter_bit = 0;
		}

		i2s_dai_dbg("[%d] audio_filter_bit : %u\n",
				i2s->blk_no,
				i2s->audio_filter_bit);

		(void) of_property_read_u32(
				pdev->dev.of_node,
				"block-type",
				&i2s->block_type);

		i2s_dai_dbg("[%d] block-type : %u\n", i2s->blk_no,
				i2s->block_type);

#if defined(CONFIG_ARCH_TCC802X)
		i2s_dai_dbg("[%d] pcfg_reg=%p\n", i2s->blk_no, i2s->pcfg_reg);
		of_property_read_u8_array(
				pdev->dev.of_node,
				"clk-mux",
				i2s->portcfg.clk,
				of_property_count_elems_of_size(
					pdev->dev.of_node,
					"clk-mux",
					sizeof(char)));
		of_property_read_u8_array(
				pdev->dev.of_node,
				"daout-mux",
				i2s->portcfg.daout,
				of_property_count_elems_of_size(
					pdev->dev.of_node,
					"daout-mux",
					sizeof(char)));
		of_property_read_u8_array(
				pdev->dev.of_node,
				"dain-mux",
				i2s->portcfg.dain,
				of_property_count_elems_of_size(
					pdev->dev.of_node,
					"dain-mux",
					sizeof(char)));
#endif

		ret = of_property_read_u32(
				pdev->dev.of_node,
				"clock-frequency",
				&sample_rate);
		if (ret == 0) {
			i2s->sample_rate = sample_rate;

			//i2s->clock_rate =
			//		sample_rate * i2s->mclk_div * i2s->bclk_ratio;
			if(sample_rate <= (ULONG_MAX / i2s->mclk_div)){
				i2s->clock_rate = (unsigned long)sample_rate *
								(unsigned long)i2s->mclk_div;
				if(i2s->clock_rate <= (ULONG_MAX / i2s->bclk_ratio)){
					i2s->clock_rate *= i2s->bclk_ratio;
				}
			}

			i2s_dai_dbg("[%d] clk_rate=%lu\n", i2s->blk_no, i2s->clock_rate);
		}

		prop = of_get_property(pdev->dev.of_node, "tdm-late-mode", NULL);
		i2s->tdm_late_mode = (prop != NULL)? TRUE : FALSE;

		prop = of_get_property(pdev->dev.of_node, "tdm-multi-port", NULL);
		i2s->tdm_multi_port = (prop != NULL)? TRUE : FALSE;

#if defined(CONFIG_ARCH_TCC805X)
		prop = of_get_property(pdev->dev.of_node, "rx-bclk-delay", NULL);
		i2s->rx_bclk_delay = (prop != NULL)? TRUE : FALSE;
#endif
	}

	i2s_dai_func_dbg("[%d] -- %s --\n", i2s->blk_no, __func__);

	return ret;
}

static int tcc_i2s_probe(struct platform_device *pdev)
{
	struct tcc_i2s_t *i2s;
	int ret;

	i2s_dai_func_dbg("++ %s ++\n", __func__);

	i2s = (struct tcc_i2s_t *)devm_kzalloc(
		&pdev->dev,
		sizeof(struct tcc_i2s_t),
		GFP_KERNEL);

	if (i2s == NULL) {
		i2s_dai_dbg(" %s - i2s is null\n", __func__);
		ret = -ENOMEM;
	} else {
		i2s_dai_dbg(" %s - i2s is not null\n", __func__);

		set_default_configrations(i2s);

		ret = parse_i2s_dt(pdev, i2s);
		if (ret < 0) {
			i2s_dai_err("%s : Fail to parse i2s dt\n", __func__);
		} else {
			platform_set_drvdata(pdev, i2s);

			i2s_initialize(i2s);

			ret = devm_snd_soc_register_component(
					&pdev->dev,
					&tcc_i2s_component_drv,
					&tcc_i2s_dai_drv[i2s->block_type],
					1);
		}
	}

	if (ret < 0) {
		i2s_dai_err("devm_snd_soc_register_component failed\n");
		if (i2s != NULL) {
			kfree(i2s);
		}
	} else {
		i2s_dai_dbg(" devm_snd_soc_register_component success\n");
	}

	i2s_dai_func_dbg("-- %s --\n", __func__);

	return ret;
}

static int tcc_i2s_remove(struct platform_device *pdev)
{
	const struct tcc_i2s_t *i2s =
		 (struct tcc_i2s_t *)platform_get_drvdata(pdev);

	i2s_dai_func_dbg("[%d] ++ %s ++\n", i2s->blk_no, __func__);

	devm_kfree(&pdev->dev, i2s);

	clk_disable_unprepare(i2s->dai_hclk);

	i2s_dai_func_dbg("-- %s --\n", __func__);

	return 0;
}

static const struct of_device_id tcc_i2s_of_match[] = {
	{ .compatible = "telechips,i2s" },
	{ .compatible = "" }
};
MODULE_DEVICE_TABLE(of, tcc_i2s_of_match);

static struct platform_driver tcc_i2s_driver = {
	.probe		= tcc_i2s_probe,
	.remove		= tcc_i2s_remove,
	.driver		= {
		.name	= "tcc_i2s_drv",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(tcc_i2s_of_match),
#endif
	},
};

module_platform_driver(tcc_i2s_driver);

MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips I2S Driver");
MODULE_LICENSE("GPL");
