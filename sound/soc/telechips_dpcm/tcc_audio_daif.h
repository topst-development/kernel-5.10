/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_DAI_H
#define TCC_DAI_H

#include <linux/io.h>
#include "tcc_widgets.h"
#include "tcc_audio_hw.h"
#include "tcc_adma_pcm.h"
#if defined(CONFIG_SND_SOC_TELECHIPS_EXTENDED_TBD)
#include "tcc_audio_volume.h"
#include "tcc_audio_mixer.h"
#include "tcc_audio_masrc.h"
#include "tcc_audio_reorder_splitter.h"
#endif

#if 0//DEBUG
#define dai_writel(v, c)			\
	({pr_info("<ASoC> DAI_REG(0x%px) = 0x%08x\n", c, (unsigned int)v); \
	writel(v, c); })
#else
#define dai_writel(v, c)			writel(v, c)
#endif

enum TCC_DAI_BLK_NO {
	TCC_DAI_BLK_0 = 0,
	TCC_DAI_BLK_1 = 1,
	TCC_DAI_BLK_2 = 2,
	TCC_DAI_BLK_3 = 3,
	TCC_DAI_BLK_4 = 4,
	TCC_DAI_BLK_5 = 5,
	TCC_DAI_BLK_6 = 6,
	TCC_DAI_BLK_7 = 7,
	TCC_DAI_BLK_MAX = 8
};

enum TCC_DAI_FMT {
	TCC_DAI_LSB_16 = 0,
	TCC_DAI_LSB_24 = 1,
	TCC_DAI_LSB_32 = 2,
	TCC_DAI_MSB_16 = 3,
	TCC_DAI_MSB_24 = 4,
	TCC_DAI_MSB_32 = 5,
};

/* TCC_DAI_CLKDIV_ID */
#define TCC_DAI_CLKDIV_ID_MCLK_TO_BCLK	(0)

/* TCC_DAI_MCLK_DIV */
#define TCC_DAI_MCLK_TO_BCLK_DIV_1	(1)	// TDM_ONLY
#define TCC_DAI_MCLK_TO_BCLK_DIV_2	(2)	// TDM_ONLY
#define TCC_DAI_MCLK_TO_BCLK_DIV_4	(4)
#define TCC_DAI_MCLK_TO_BCLK_DIV_6	(6)
#define TCC_DAI_MCLK_TO_BCLK_DIV_8	(8)
#define TCC_DAI_MCLK_TO_BCLK_DIV_16	(16)
#define TCC_DAI_MCLK_TO_BCLK_DIV_24	(24)
#define TCC_DAI_MCLK_TO_BCLK_DIV_32	(32)
#define TCC_DAI_MCLK_TO_BCLK_DIV_48	(48)
#define TCC_DAI_MCLK_TO_BCLK_DIV_64	(64)

enum TCC_DAI_BCLK_RATIO {
	TCC_DAI_BCLK_RATIO_32 = 32u,
	TCC_DAI_BCLK_RATIO_48 = 48u,
	TCC_DAI_BCLK_RATIO_64 = 64u,
 	TCC_DAI_BCLK_RATIO_512 = 512u
};

enum TCC_DAI_PATH {
	TCC_DAI_PATH_ADMA = 0,
	TCC_DAI_PATH_ASRC = 1
};

enum TCC_CLOCK_ID {
	TCC_DAI_MCLK = 0,
};

struct dai_reg_t {
	uint32_t damr;
	uint32_t mccr0;
	uint32_t mccr1;
	uint32_t drmr;
	uint32_t dclkdiv;
};

 enum TCC_DAI_TDM_RX_CH {
	TCC_DAI_TDM_RX_2CH = 2,
	TCC_DAI_TDM_RX_4CH = 4,
	TCC_DAI_TDM_RX_8CH = 8,
	TCC_DAI_TDM_RX_16CH = 16,
	TCC_DAI_TDM_RX_32CH = 32
};

enum TCC_DAI_TDM_SLOT_SIZE {
	TCC_DAI_TDM_SLOT_32BIT = 32u,
	TCC_DAI_TDM_SLOT_16BIT = 16u,
	TCC_DAI_TDM_SLOT_24BIT = 24u
};

#define DAI_BLOCK_STEREO_TYPE 	(0u)
#define DAI_BLOCK_7_1CH_TYPE	(1u)
#define DAI_BLOCK_9_1CH_TYPE	(2u)
#define DAI_BLOCK_TYPE_MAX	(3u)

enum tcc_i2s_audio_filter_type {
	DAI_AUDIO_CLK_FILTER_TYPE = 1,
	DAI_AUDIO_DATA_FILTER_TYPE = 2
};

struct tcc_i2s_t {
	struct platform_device *pdev;

	// hw info
	int32_t blk_no;
	void __iomem *dai_reg;
	struct clk *dai_pclk;
#if !defined(CONFIG_ARCH_TCC807X)
	struct clk *dai_hclk;
#endif
	struct clk *dai_filter_clk;
	uint32_t have_fifo_clear_bit;
	uint32_t audio_filter_bit;
	uint32_t  block_type;

	// configurations
	uint32_t sample_rate;
	uint32_t channels;
	snd_pcm_format_t format;
	uint8_t mclk_div;
	uint16_t bclk_ratio;
	uint32_t dai_fmt;
	uint8_t tx_fifo_delay;
	uint8_t rx_fifo_delay;

	bool clk_continuous;
	bool is_pinctrl_export;

	bool tdm_mode;
	bool frame_invert; //for TDM I2S mode
	bool tdm_late_mode;
	bool tdm_multi_port;
 	bool rx_bclk_delay;
 	bool is_updated;
	uint8_t tdm_slots;
	uint8_t tdm_slot_width;

	//status
	unsigned long clock_rate;
	struct tcc_adma_info dma_info;
	struct dai_reg_t regs_backup; // for suspend/resume
	uint32_t type; /* 0: adma, 1: fifo(MAFC) */
	uint64_t wdma; /* mafc fifo address for tx */
	uint64_t rdma; /* mafc fifo address for rx */
};

struct tcc_tdm_t {
	struct platform_device *pdev;

};

struct tcc_dai_t {
	struct tcc_i2s_t i2s;
	struct tcc_tdm_t tdm;
};


#define SND_SOC_DAPM_OUTPUT_E(wname, wevent, wflags) \
{   .id = snd_soc_dapm_output, .name = wname, \
	.reg = SND_SOC_NOPM, \
	.event = wevent, .event_flags = wflags }

/*
#define SND_SOC_DAPM_SRC_E(wname, stname, wslot, wreg, wshift, winvert, \
			     wevent, wflags)				\
{	.id = snd_soc_dapm_src, .name = wname, .sname = stname, \
	SND_SOC_DAPM_INIT_REG_VAL(wreg, wshift, winvert), \

	.event = wevent, .event_flags = wflags }
*/
// this widget is temparary for masrc. it will be changed to another widget @mk
#define SND_SOC_DAPM_SRC_E(wname, wreg, wshift, winvert, wevent, wflags) \
{	.id = snd_soc_dapm_src, .name = wname, \
	SND_SOC_DAPM_INIT_REG_VAL(wreg, wshift, winvert), \
	.event = wevent, .event_flags = wflags }

static inline void tcc_dai_dump(const void __iomem *base_addr)
{
	(void) pr_info("DAMR : 0x%08x\n", readl(base_addr + TCC_DAI_DAMR_OFFSET));
	(void) pr_info("MCCR0: 0x%08x\n",
		     readl(base_addr + TCC_DAI_MCCR0_OFFSET));
	(void) pr_info("MCCR1: 0x%08x\n",
		     readl(base_addr + TCC_DAI_MCCR1_OFFSET));
	(void) pr_info("DRMR : 0x%08x\n", readl(base_addr + TCC_DAI_DRMR_OFFSET));
 	(void) pr_info("DCLKDIV : 0x%08x\n",
		     readl(base_addr + TCC_DAI_DCLKDIV_OFFSET));
 }

static inline void tcc_dai_damr_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &=
	    ~(DAMR_DAI_ENABLE_MODE_Msk
		| DAMR_DAI_TRANSMITTER_MODE_Msk
		| DAMR_DAI_RECEIVER_MODE_Msk);
	if (enable) {
		value |=
		    (DAMR_DAI_ENABLE
			|DAMR_DAI_TRANSMITTER_ENABLE
			|DAMR_DAI_RECEIVER_ENABLE);
	} else {
		value |=
		    (DAMR_DAI_DISABLE
			|DAMR_DAI_TRANSMITTER_DISABLE
			|DAMR_DAI_RECEIVER_DISABLE);
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_ENABLE_MODE_Msk;
	if (enable) {
		value |= DAMR_DAI_ENABLE;
	} else {
		value |= DAMR_DAI_DISABLE;
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline uint32_t tcc_dai_enable_check(const void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);
	uint32_t ret = 0;

	ret = (((value) & DAMR_DAI_ENABLE_MODE_Msk) >> DAMR_DAI_ENABLE_MODE_Pos);

	return ret;
}

static inline void tcc_dai_rx_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_RECEIVER_MODE_Msk;
	if (enable) {
		value |= DAMR_DAI_RECEIVER_ENABLE;
	} else {
		value |= DAMR_DAI_RECEIVER_DISABLE;
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline uint32_t tcc_dai_rx_check(const void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);
	uint32_t ret = 0;

	ret = (((value) & DAMR_DAI_RECEIVER_MODE_Msk) >> DAMR_DAI_RECEIVER_MODE_Pos);

	return ret;
}

static inline void tcc_dai_tx_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_TRANSMITTER_MODE_Msk;
	if (enable) {
		value |= DAMR_DAI_TRANSMITTER_ENABLE;
	} else {
		value |= DAMR_DAI_TRANSMITTER_DISABLE;
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline uint32_t tcc_dai_tx_check(const void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);
	uint32_t ret = 0;

	ret = (((value) & DAMR_DAI_TRANSMITTER_MODE_Msk) >> DAMR_DAI_TRANSMITTER_MODE_Pos);

	return ret;
}

static inline void tcc_dai_tx2rx_loopback_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_TX2RX_LOOPBACK_Msk;
	if (enable) {
		value |= DAMR_DAI_TX2RX_LOOPBACK_ENABLE;
	} else {
		value |= DAMR_DAI_TX2RX_LOOPBACK_DISABLE;
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline uint32_t tcc_dai_tx2rx_loopback_check(const void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);
	uint32_t ret = 0;

	ret = (((value) & DAMR_DAI_TX2RX_LOOPBACK_Msk) >> DAMR_DAI_TX2RX_LOOPBACK_Pos);

	return ret;
}

static inline void tcc_dai_rx2tx_loopback_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_RX2TX_LOOPBACK_Msk;
	if (enable) {
		value |= DAMR_DAI_RX2TX_LOOPBACK_ENABLE;
	} else {
		value |= DAMR_DAI_RX2TX_LOOPBACK_DISABLE;
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline uint32_t tcc_dai_rx2tx_loopback_check(const void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);
	uint32_t ret = 0;

	ret = (((value) & DAMR_DAI_RX2TX_LOOPBACK_Msk) >> DAMR_DAI_RX2TX_LOOPBACK_Pos);

	return ret;
}

static inline void tcc_dai_dma_threshold_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_BUF_THRESHOLD_MODE_Msk;
	if (enable) {
		value |= DAMR_DAI_BUF_THRESHOLD_ENABLE;
	} else {
		value |= DAMR_DAI_BUF_THRESHOLD_DISABLE;
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_bitclk_polarity(
	void __iomem *base_addr,
	bool positive)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_BIT_CLK_POLARITY_Msk;
	if (positive) {
		value |= DAMR_DAI_BIT_CLK_POSITIVE;
	} else {
		value |= DAMR_DAI_BIT_CLK_NAGATIVE;
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_i2s_mode(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &=
		~(DAMR_RX_JUSTIFIED_MODE_Msk
		| DAMR_TX_JUSTIFIED_MODE_Msk
		| DAMR_DAI_SYNC_MODE_Msk
		| DAMR_DSP_MODE_Msk);

	value |= (DAMR_DAI_SYNC_IIS_DSP_TDM | DAMR_DSP_IIS_MODE);

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_right_j_mode(
	void __iomem *base_addr,
	uint32_t bclk_ratio)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &=
		~(DAMR_RX_JUSTIFIED_MODE_Msk
		| DAMR_TX_JUSTIFIED_MODE_Msk
		| DAMR_DAI_SYNC_MODE_Msk
		| DAMR_DSP_MODE_Msk);

	// Right_j rx doesn't work well when bclk_ratio is 32fs.
	// but, the waveform is the same as left_j when bclk_ratio is 32fs
	if (bclk_ratio == (uint32_t) TCC_DAI_BCLK_RATIO_32) {
		value |=
		    (DAMR_DAI_SYNC_LR_JUSTIFIED
			|DAMR_TX_JUSTIFIED_RIGHT
			|DAMR_RX_JUSTIFIED_LEFT
			|DAMR_DSP_IIS_MODE);
	} else {
		value |=
		    (DAMR_DAI_SYNC_LR_JUSTIFIED
			|DAMR_TX_JUSTIFIED_RIGHT
			|DAMR_RX_JUSTIFIED_RIGHT
			|DAMR_DSP_IIS_MODE);
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_left_j_mode(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &=
		~(DAMR_RX_JUSTIFIED_MODE_Msk
		| DAMR_TX_JUSTIFIED_MODE_Msk
		| DAMR_DAI_SYNC_MODE_Msk
		| DAMR_DSP_MODE_Msk);

	value |=
		(DAMR_DAI_SYNC_LR_JUSTIFIED
		|DAMR_TX_JUSTIFIED_LEFT
		|DAMR_RX_JUSTIFIED_LEFT
		|DAMR_DSP_IIS_MODE);

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_i2s_tdm_mode(
	void __iomem *base_addr,
	uint32_t tdm_slots,
	uint32_t slot_width,
	bool lateMode)
{
	uint32_t damr = readl(base_addr + TCC_DAI_DAMR_OFFSET);
	uint32_t mccr0 = readl(base_addr + TCC_DAI_MCCR0_OFFSET);

	uint32_t frame_len = 0;
	uint32_t half_frame_len = 0;

	if ((slot_width != 0u) && (tdm_slots <= (UINT_MAX / slot_width))) {
		frame_len = tdm_slots * slot_width;
		half_frame_len = frame_len / 2u;
	}

	damr &=
		~(DAMR_RX_JUSTIFIED_MODE_Msk
		| DAMR_TX_JUSTIFIED_MODE_Msk
		| DAMR_DAI_SYNC_MODE_Msk
		| DAMR_DSP_MODE_Msk);

	mccr0 &=
		~(MCCR0_FRAME_SIZE_Msk
		| MCCR0_FRAME_CLK_DIV_Msk
		| MCCR0_TDM_MODE_Msk
		| MCCR0_CIRRUS_LATE_Msk
		| MCCR0_MODE_SELECT_Msk
		| MCCR0_FRAME_INVERT_Msk
		| MCCR0_FRAME_BEGIN_POSITION_Msk
		| MCCR0_FRAME_END_POSTION_Msk);

	damr |= (DAMR_DAI_SYNC_IIS_DSP_TDM | DAMR_DSP_OR_TDM_MODE);

	if(frame_len < 1u){
		frame_len = 1u;
	}

	if(half_frame_len < 1u){
		half_frame_len = 1u;
	}

	if ((tdm_slots == 32u) && (slot_width == 32u)) {
		mccr0 |= MCCR0_FRAME_SIZE_32TDM_32BITSLOT;
	} else if ((tdm_slots == 32u) && (slot_width == 24u)) {
		mccr0 |= MCCR0_FRAME_SIZE_32TDM_24BITSLOT;
	} else {
		mccr0 |= ((frame_len - 1u)
				<< (uint32_t) MCCR0_FRAME_SIZE_Pos);
	}

	mccr0 |=
	    ((half_frame_len - 1u) <<
	    (uint32_t) MCCR0_FRAME_END_POSTION_Pos);
	mccr0 |= MCCR0_FRAME_CLK_DIV_USE;
	mccr0 |= MCCR0_TDM_MODE_0;

	mccr0 |= MCCR0_FRAME_INVERT_ENABLE;
	mccr0 |= MCCR0_FRAME_BEGIN_EARLY_MODE;

	if (lateMode == TRUE) {
		mccr0 |= MCCR0_MODE_SELECT_ENABLE;
	}

	dai_writel(damr, base_addr + TCC_DAI_DAMR_OFFSET);
	dai_writel(mccr0, base_addr + TCC_DAI_MCCR0_OFFSET);
}


static inline void tcc_dai_set_dsp_tdm_word_len(
	void __iomem *base_addr,
	uint32_t bit_width)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~(DAMR_DSP_WORD_LEN_Msk);

	value |= (bit_width == (uint32_t)24) ?
		((uint32_t) DAMR_DSP_WORD_LEN_24BIT) :
		((uint32_t) DAMR_DSP_WORD_LEN_16BIT);

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_cirrus_tdm_mode(
	void __iomem *base_addr,
	uint32_t slots,
	uint32_t slot_width,
	bool late)
{
	uint32_t damr = readl(base_addr + TCC_DAI_DAMR_OFFSET);
	uint32_t mccr0 = readl(base_addr + TCC_DAI_MCCR0_OFFSET);

	uint32_t frame_len = 0;
	uint32_t half_frame_len = 0;

	if ((slot_width != 0u) && (slots <= (UINT_MAX / slot_width))) {
		frame_len = slots * slot_width;
		half_frame_len = frame_len / 2u;
	}

	damr &=
		~(DAMR_RX_JUSTIFIED_MODE_Msk
		| DAMR_TX_JUSTIFIED_MODE_Msk
		| DAMR_DAI_SYNC_MODE_Msk
		| DAMR_DSP_MODE_Msk);

 	mccr0 &=
		~(MCCR0_FRAME_SIZE_Msk
				| MCCR0_FRAME_CLK_DIV_Msk
				| MCCR0_TDM_MODE_Msk
				| MCCR0_MODE_SELECT_Msk
				| MCCR0_FRAME_INVERT_Msk
				| MCCR0_FRAME_BEGIN_POSITION_Msk
				| MCCR0_FRAME_END_POSTION_Msk);

	damr |= (DAMR_DAI_SYNC_IIS_DSP_TDM | DAMR_DSP_OR_TDM_MODE);

	if(frame_len < 1u){
		frame_len = 1u;
	}

	if(half_frame_len < 1u){
		half_frame_len = 1u;
	}

	if ((slots == 32u) && (slot_width == 32u)) {
		mccr0 |= MCCR0_FRAME_SIZE_32TDM_32BITSLOT;
	} else if ((slots == 32u) && (slot_width == 24u)) {
		mccr0 |= MCCR0_FRAME_SIZE_32TDM_24BITSLOT;
	} else {
		mccr0 |= ((frame_len - 1u)
				<< (uint32_t) MCCR0_FRAME_SIZE_Pos);
	}

	mccr0 |=
	    ((half_frame_len - 1u) <<
	    (uint32_t) MCCR0_FRAME_END_POSTION_Pos);
	mccr0 |= MCCR0_FRAME_CLK_DIV_USE;
	mccr0 |= MCCR0_TDM_MODE_0;

	mccr0 |= MCCR0_FRAME_INVERT_DISABLE;

	mccr0 |= MCCR0_FRAME_BEGIN_EARLY_MODE;

 	if (late == TRUE) {
		mccr0 |= MCCR0_MODE_SELECT_ENABLE;
	}

	dai_writel(damr, base_addr + TCC_DAI_DAMR_OFFSET);
	dai_writel(mccr0, base_addr + TCC_DAI_MCCR0_OFFSET);
}

static inline void tcc_dai_set_dsp_tdm_mode(
	void __iomem *base_addr,
	uint32_t tdm_slots,
	uint32_t slot_width,
	bool late)
{
	uint32_t damr = readl(base_addr + TCC_DAI_DAMR_OFFSET);
	uint32_t mccr0 = readl(base_addr + TCC_DAI_MCCR0_OFFSET);
	uint32_t frame_len = 0;

	if((slot_width != 0u) && (tdm_slots <= (UINT_MAX / slot_width))){
		frame_len = tdm_slots * slot_width;
	}

	damr &=
		~(DAMR_RX_JUSTIFIED_MODE_Msk
		| DAMR_TX_JUSTIFIED_MODE_Msk
		| DAMR_DAI_SYNC_MODE_Msk
		| DAMR_DSP_MODE_Msk
		| DAMR_DSP_WORD_LEN_Msk);

	mccr0 &=
		~(MCCR0_FRAME_SIZE_Msk
		| MCCR0_FRAME_CLK_DIV_Msk
		| MCCR0_TDM_MODE_Msk
		| MCCR0_CIRRUS_LATE_Msk
		| MCCR0_MODE_SELECT_Msk
		| MCCR0_FRAME_INVERT_Msk
		| MCCR0_FRAME_BEGIN_POSITION_Msk
		| MCCR0_FRAME_END_POSTION_Msk);

	damr |= (DAMR_DAI_SYNC_IIS_DSP_TDM | DAMR_DSP_OR_TDM_MODE);

	if(frame_len < 1u){
		frame_len = 1u;
	}

	if ((tdm_slots == 32u) && (slot_width == 32u)) {
		mccr0 |= MCCR0_FRAME_SIZE_32TDM_32BITSLOT;
	} else if ((tdm_slots == 32u) && (slot_width == 24u)) {
		mccr0 |= MCCR0_FRAME_SIZE_32TDM_24BITSLOT;
	} else {
		mccr0 |= ((frame_len - 1u) <<
			(uint32_t) MCCR0_FRAME_SIZE_Pos);
	}

	mccr0 |= ((uint32_t) 0 << (uint32_t) MCCR0_FRAME_END_POSTION_Pos);
	mccr0 |= MCCR0_FRAME_CLK_DIV_USE;

	mccr0 |= MCCR0_FRAME_INVERT_DISABLE;

	mccr0 |= MCCR0_TDM_MODE_0;
	if (late == TRUE) {
		mccr0 |= MCCR0_MODE_SELECT_ENABLE;	//DSP-B
	}

	dai_writel(damr, base_addr + TCC_DAI_DAMR_OFFSET);
	dai_writel(mccr0, base_addr + TCC_DAI_MCCR0_OFFSET);
}

static inline void tcc_dai_set_dsp_tdm_mode_valid_data(
	void __iomem *base_addr,
	uint32_t channels,
	uint32_t slot_width)
{
	uint32_t mccr1 = readl(base_addr + TCC_DAI_MCCR1_OFFSET);
	uint32_t value = 0;

	//value = ((channels - 1) * slot_width) - 1;
	if (channels >= 1u) {
		value = channels -1u;
		if((slot_width != 0u) && (value <= (UINT_MAX / slot_width))){
			value = value * slot_width;
			if(value >= 1u){
				value -= 1u;
			}
		}
	}

	mccr1 &= ~MCCR1_VALID_END_Msk;

	mccr1 |= ((value) << MCCR1_VALID_END_Pos) & MCCR1_VALID_END_Msk;

	dai_writel(mccr1, base_addr + TCC_DAI_MCCR1_OFFSET);
}

static inline void tcc_dai_set_dsp_tdm_mode_rx_channel(
	void __iomem *base_addr,
	uint32_t channels)
{
	uint32_t mccr1 = readl(base_addr + TCC_DAI_MCCR1_OFFSET);
	uint32_t value =
		(channels == (uint32_t)TCC_DAI_TDM_RX_2CH) ?
			MCCR1_TDM_RX_CH_2CH :
		(channels == (uint32_t)TCC_DAI_TDM_RX_4CH) ?
			MCCR1_TDM_RX_CH_4CH :
		(channels == (uint32_t)TCC_DAI_TDM_RX_8CH) ?
			MCCR1_TDM_RX_CH_8CH :
		(channels == (uint32_t)TCC_DAI_TDM_RX_16CH) ?
			MCCR1_TDM_RX_CH_16CH :
		(channels == (uint32_t)TCC_DAI_TDM_RX_32CH) ?
			MCCR1_TDM_RX_CH_32CH :
			MCCR1_TDM_RX_CH_2CH;

	mccr1 &= ~MCCR1_TDM_RX_CH_Msk;

	mccr1 |= value;

	dai_writel(mccr1, base_addr + TCC_DAI_MCCR1_OFFSET);
}

static inline void tcc_dai_set_dsp_tdm_mode_rx_early(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t mccr1 = readl(base_addr + TCC_DAI_MCCR1_OFFSET);

	mccr1 &= ~MCCR1_TDM_RX_EARLY_Msk;

	if (enable) {
		mccr1 |= MCCR1_TDM_RX_EARLY_ENABLE;
	} else {
		mccr1 |= MCCR1_TDM_RX_EARLY_DISABLE;
	}

	dai_writel(mccr1, base_addr + TCC_DAI_MCCR1_OFFSET);
}

#define RX_BCLK_DELAY_MUST_SET_BCLK (17000000)
static inline void tcc_dai_set_rx_bclk_delay(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t mccr1 = readl(base_addr + TCC_DAI_MCCR1_OFFSET);

	mccr1 &= ~MCCR1_TDM_RX_FDBK_Msk;

	if (enable) {
		mccr1 |= MCCR1_TDM_RX_FDBK_ENABLE;
	} else {
		mccr1 |= MCCR1_TDM_RX_FDBK_DISABLE;
	}

	dai_writel(mccr1, base_addr + TCC_DAI_MCCR1_OFFSET);
}

static inline void tcc_dai_set_master_mode(
	void __iomem *base_addr,
	bool mclk_master, bool bclk_master,
	bool lrck_master, bool is_pinctrl_export)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &=
		~(DAMR_BCLK_SRC_MODE_Msk
		| DAMR_LRCK_SRC_MODE_Msk
		| DAMR_DAI_SYS_CLK_MASTER_Msk
		| DAMR_DAI_BIT_CLK_MASTER_Msk
		| DAMR_DAI_FRAME_CLK_MASTER_Msk);

	if (!is_pinctrl_export) {
		value |=
		    (DAMR_BCLK_SRC_DIRECT_MASTER | DAMR_LRCK_SRC_DIRECT_MASTER);
	} else {
		value |= (DAMR_BCLK_SRC_BCLK_PAD | DAMR_LRCK_SRC_LRCK_PAD);
	}

	if (mclk_master) {
		value |= DAMR_DAI_SYS_CLK_MASTER_SYS;
	} else {
		value |= DAMR_DAI_SYS_CLK_MASTER_EXT;
	}

	if (bclk_master) {
		value |= (DAMR_BCLK_SRC_DIRECT_MASTER);
		value |= (DAMR_DAI_BIT_CLK_MASTER_SYS);
	} else {
		value |= (DAMR_DAI_BIT_CLK_MASTER_EXT);
	}

	if (lrck_master) {
		value |= (DAMR_LRCK_SRC_DIRECT_MASTER);
		value |= (DAMR_DAI_FRAME_CLK_MASTER_SYS);
	} else {
		value |= (DAMR_DAI_FRAME_CLK_MASTER_EXT);
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline uint32_t tcc_dai_get_mclk_div(
	const void __iomem *base_addr,
	bool tdm_mode)
{
	uint32_t mccr0 = readl(base_addr + TCC_DAI_MCCR0_OFFSET);
 	uint32_t dclkdiv = readl(base_addr + TCC_DAI_DCLKDIV_OFFSET);
 	uint32_t mclk_div = 0, ret = 0;

 	mclk_div = dclkdiv & DCLKDIV_DAI_BIT_CLK_DIV_Msk;

	ret =
		(mclk_div == DCLKDIV_DAI_BIT_CLK_DIV_4) ?
		(uint32_t)TCC_DAI_MCLK_TO_BCLK_DIV_4 :
		(mclk_div == DCLKDIV_DAI_BIT_CLK_DIV_6) ?
		(uint32_t)TCC_DAI_MCLK_TO_BCLK_DIV_6 :
		(mclk_div == DCLKDIV_DAI_BIT_CLK_DIV_8) ?
		(uint32_t)TCC_DAI_MCLK_TO_BCLK_DIV_8 :
		(mclk_div == DCLKDIV_DAI_BIT_CLK_DIV_16) ?
		(uint32_t)TCC_DAI_MCLK_TO_BCLK_DIV_16 :
		(mclk_div == DCLKDIV_DAI_BIT_CLK_DIV_24) ?
		(uint32_t)TCC_DAI_MCLK_TO_BCLK_DIV_24 :
		(mclk_div == DCLKDIV_DAI_BIT_CLK_DIV_32) ?
		(uint32_t)TCC_DAI_MCLK_TO_BCLK_DIV_32 :
		(mclk_div == DCLKDIV_DAI_BIT_CLK_DIV_48) ?
		(uint32_t)TCC_DAI_MCLK_TO_BCLK_DIV_48 :
		(uint32_t)TCC_DAI_MCLK_TO_BCLK_DIV_64;


	if (tdm_mode) {
		mclk_div = mccr0 & MCCR0_TDM_BIT_CLK_DIV_Msk;
		ret =
			(mclk_div == MCCR0_TDM_BIT_CLK_DIV_1) ?
			(uint32_t)TCC_DAI_MCLK_TO_BCLK_DIV_1 :
			(mclk_div == MCCR0_TDM_BIT_CLK_DIV_2) ?
			(uint32_t)TCC_DAI_MCLK_TO_BCLK_DIV_2 :
			ret;
	}

	return ret;
}

 static inline uint32_t tcc_dai_get_bclk_ratio(
	const void __iomem *base_addr,
	bool tdm_mode)
 {
 	uint32_t dclkdiv = readl(base_addr + TCC_DAI_DCLKDIV_OFFSET);
 	uint32_t bclk_ratio = 0, ret = 0;

	if (tdm_mode == FALSE) {
		bclk_ratio = dclkdiv & DCLKDIV_DAI_FRAME_CLK_DIV_Msk;
		ret =
			(bclk_ratio == DCLKDIV_DAI_FRAME_CLK_DIV_32) ?
			(uint32_t)TCC_DAI_BCLK_RATIO_32 :
			(bclk_ratio == DCLKDIV_DAI_FRAME_CLK_DIV_48) ?
			(uint32_t)TCC_DAI_BCLK_RATIO_48 :
			(bclk_ratio == DCLKDIV_DAI_FRAME_CLK_DIV_64) ?
			(uint32_t)TCC_DAI_BCLK_RATIO_64 :
			(uint32_t)TCC_DAI_BCLK_RATIO_512;
 	}
	return ret;
}

 static inline uint32_t tcc_dai_set_clk_mode_sub_get_bclkdiv(uint32_t mclk_div){
	uint32_t bclkdiv =
		    (mclk_div == (uint32_t)TCC_DAI_MCLK_TO_BCLK_DIV_4) ?
			DCLKDIV_DAI_BIT_CLK_DIV_4 :
			(mclk_div == (uint32_t)TCC_DAI_MCLK_TO_BCLK_DIV_6) ?
			DCLKDIV_DAI_BIT_CLK_DIV_6 :
			(mclk_div == (uint32_t)TCC_DAI_MCLK_TO_BCLK_DIV_8) ?
			DCLKDIV_DAI_BIT_CLK_DIV_8 :
			(mclk_div == (uint32_t)TCC_DAI_MCLK_TO_BCLK_DIV_16) ?
			DCLKDIV_DAI_BIT_CLK_DIV_16 :
			(mclk_div == (uint32_t)TCC_DAI_MCLK_TO_BCLK_DIV_24) ?
			DCLKDIV_DAI_BIT_CLK_DIV_24 :
			(mclk_div == (uint32_t)TCC_DAI_MCLK_TO_BCLK_DIV_32) ?
			DCLKDIV_DAI_BIT_CLK_DIV_32 :
			(mclk_div == (uint32_t)TCC_DAI_MCLK_TO_BCLK_DIV_48) ?
			DCLKDIV_DAI_BIT_CLK_DIV_48 :
			DCLKDIV_DAI_BIT_CLK_DIV_64;
	return bclkdiv;
}



 static inline void tcc_dai_set_clk_mode(
	void __iomem *base_addr,
	uint32_t mclk_div,
	uint32_t bclk_ratio,
	uint32_t slot_size,
	bool tdm_mode)
{
	uint32_t mccr0 = readl(base_addr + TCC_DAI_MCCR0_OFFSET);
	uint32_t dclkdiv = readl(base_addr + TCC_DAI_DCLKDIV_OFFSET);

 	dclkdiv &=
			~(DCLKDIV_DAI_BIT_CLK_DIV_Msk
		     |DCLKDIV_DAI_FRAME_CLK_DIV_Msk);

	mccr0 &= ~MCCR0_TDM_BIT_CLK_DIV_Msk;

	if (tdm_mode) {
  		dclkdiv |=
		    (slot_size == (uint32_t)TCC_DAI_TDM_SLOT_16BIT) ?
			DCLKDIV_DAI_TDM_SLOT_SIZE_16 :
		    (slot_size == (uint32_t)TCC_DAI_TDM_SLOT_24BIT) ?
			DCLKDIV_DAI_TDM_SLOT_SIZE_24 :
			DCLKDIV_DAI_TDM_SLOT_SIZE_32;
 		dclkdiv |= tcc_dai_set_clk_mode_sub_get_bclkdiv(mclk_div);


		mccr0 |=
		    (mclk_div == (uint32_t)TCC_DAI_MCLK_TO_BCLK_DIV_1) ?
			MCCR0_TDM_BIT_CLK_DIV_1 :
			(mclk_div == (uint32_t)TCC_DAI_MCLK_TO_BCLK_DIV_2) ?
			MCCR0_TDM_BIT_CLK_DIV_2 :
			MCCR0_TDM_BIT_CLK_DIV_DISABLE;
	} else {
 		dclkdiv |= tcc_dai_set_clk_mode_sub_get_bclkdiv(mclk_div);
		dclkdiv |=
		    (bclk_ratio == (uint32_t)TCC_DAI_BCLK_RATIO_32) ?
			DCLKDIV_DAI_FRAME_CLK_DIV_32 :
 		    (bclk_ratio == (uint32_t)TCC_DAI_BCLK_RATIO_48) ?
			DCLKDIV_DAI_FRAME_CLK_DIV_48 :
		    (bclk_ratio == (uint32_t)TCC_DAI_BCLK_RATIO_64) ?
			DCLKDIV_DAI_FRAME_CLK_DIV_64 :
			DCLKDIV_DAI_FRAME_CLK_DIV_512;


		mccr0 |= MCCR0_TDM_BIT_CLK_DIV_DISABLE;
	}

 	dai_writel(dclkdiv, base_addr + TCC_DAI_DCLKDIV_OFFSET);
 	dai_writel(mccr0, base_addr + TCC_DAI_MCCR0_OFFSET);

}

static inline uint32_t tcc_dai_get_tx_format(
	const void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= DAMR_DAI_TX_SHIFT_Msk;
	return (value >> DAMR_DAI_TX_SHIFT_Pos);
}

static inline void tcc_dai_set_tx_format(
	void __iomem *base_addr,
	enum TCC_DAI_FMT fmt)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~(DAMR_DAI_TX_SHIFT_Msk);

	value |=
		((fmt == TCC_DAI_MSB_24) || (fmt == TCC_DAI_MSB_32)) ? (DAMR_DAI_TX_SHIFT_MSB_24) :
	    (fmt == TCC_DAI_MSB_16) ? (DAMR_DAI_TX_SHIFT_MSB_16) :
	    ((fmt == TCC_DAI_LSB_24) || (fmt == TCC_DAI_LSB_32)) ? (DAMR_DAI_TX_SHIFT_LSB_24) :
		(DAMR_DAI_TX_SHIFT_LSB_16);

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
	if((fmt == TCC_DAI_MSB_32) || (fmt == TCC_DAI_LSB_32)) {
#if defined(CONFIG_ARCH_TCC807X)
		//TBD setting for 32bit
#endif
	}
}

static inline void tcc_dai_set_rx_format(
	void __iomem *base_addr,
	enum TCC_DAI_FMT fmt)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~(DAMR_DAI_RX_SHIFT_Msk);

	value |=
		((fmt == TCC_DAI_MSB_24) || (fmt == TCC_DAI_MSB_32)) ? (DAMR_DAI_RX_SHIFT_MSB_24) :
	    (fmt == TCC_DAI_MSB_16) ? (DAMR_DAI_RX_SHIFT_MSB_16) :
	    ((fmt == TCC_DAI_LSB_24) || (fmt == TCC_DAI_LSB_32)) ? (DAMR_DAI_RX_SHIFT_LSB_24) :
		(DAMR_DAI_RX_SHIFT_LSB_16);

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
	if((fmt == TCC_DAI_MSB_32) || (fmt == TCC_DAI_LSB_32)) {
#if defined(CONFIG_ARCH_TCC807X)
		//TBD setting for 32bit
#endif
	}
}

static inline void tcc_dai_set_audio_filter_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_AUDIO_FILTER_MODE_Msk;

	if (enable) {
		value |= DAMR_AUDIO_FILTER_ENABLE;
	} else {
		value |= DAMR_AUDIO_FILTER_DISABLE;
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_audio_data_filter_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_AUDIO_DATA_FILTER_MODE_Msk;

	if (enable) {
		value |= DAMR_AUDIO_DATA_FILTER_ENABLE;
	} else {
		value |= DAMR_AUDIO_DATA_FILTER_DISABLE;
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_dsp_tdm_frame_invert(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_MCCR0_OFFSET);

	value &= ~MCCR0_FRAME_INVERT_Msk;

	value |=
	    (enable) ? MCCR0_FRAME_INVERT_ENABLE : MCCR0_FRAME_INVERT_DISABLE;

	dai_writel(value, base_addr + TCC_DAI_MCCR0_OFFSET);
}

static inline void tcc_dai_set_dao_mask(
	void __iomem *base_addr,
	bool dao0,
	bool dao1,
	bool dao2,
	bool dao3,
	bool dao4)
{
	uint32_t value = readl(base_addr + TCC_DAI_MCCR0_OFFSET);

	value &=
		~(MCCR0_DAO0_MASK_Msk
		|MCCR0_DAO1_MASK_Msk
		|MCCR0_DAO2_MASK_Msk
		|MCCR0_DAO3_MASK_Msk);

	value |= (dao0) ? MCCR0_DAO0_MASK_ENABLE : MCCR0_DAO0_MASK_DISABLE;
	value |= (dao1) ? MCCR0_DAO1_MASK_ENABLE : MCCR0_DAO1_MASK_DISABLE;
	value |= (dao2) ? MCCR0_DAO2_MASK_ENABLE : MCCR0_DAO2_MASK_DISABLE;
	value |= (dao3) ? MCCR0_DAO3_MASK_ENABLE : MCCR0_DAO3_MASK_DISABLE;
	value |= (dao4) ? MCCR0_DAO4_MASK_ENABLE : MCCR0_DAO4_MASK_DISABLE;

	dai_writel(value, base_addr + TCC_DAI_MCCR0_OFFSET);
}

static inline void tcc_dai_set_dao_path_sel_inner(
	void __iomem *base_addr,
	enum TCC_DAI_PATH dao0,
	enum TCC_DAI_PATH dao1,
	enum TCC_DAI_PATH dao2,
	enum TCC_DAI_PATH dao3)
{
	uint32_t value = readl(base_addr + TCC_DAI_MCCR1_OFFSET);

	value &=
			~(MCCR1_DAO0_PATH_Msk
			|MCCR1_DAO1_PATH_Msk
			|MCCR1_DAO2_PATH_Msk
			|MCCR1_DAO3_PATH_Msk);

	value |=
		(dao0 == TCC_DAI_PATH_ADMA) ? MCCR1_DAO0_PATH_ADMA :
			MCCR1_DAO0_PATH_ASRC;
	value |=
		(dao1 == TCC_DAI_PATH_ADMA) ? MCCR1_DAO1_PATH_ADMA :
		MCCR1_DAO1_PATH_ASRC;
	value |=
	    (dao2 == TCC_DAI_PATH_ADMA) ? MCCR1_DAO2_PATH_ADMA :
		MCCR1_DAO2_PATH_ASRC;
	value |=
		(dao3 == TCC_DAI_PATH_ADMA) ? MCCR1_DAO3_PATH_ADMA :
		MCCR1_DAO3_PATH_ASRC;

	dai_writel(value, base_addr + TCC_DAI_MCCR1_OFFSET);
}

static inline void tcc_dai_set_dao_path_sel(
	void __iomem *base_addr,
	enum TCC_DAI_PATH dao_path)
{
	tcc_dai_set_dao_path_sel_inner(base_addr, dao_path, dao_path, dao_path, dao_path);
}

static inline void tcc_dai_set_multiport_mode(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_MULTIPORT_MODE_Msk;

	if (enable) {
		value |= DAMR_MULTIPORT_ENABLE;
	} else {
		value |= DAMR_MULTIPORT_DISABLE;
	}

	dai_writel(value, base_addr + TCC_DAI_DAMR_OFFSET);
}

static inline uint32_t tcc_dai_multiport_mode_check(const void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DAMR_OFFSET);
	uint32_t ret = 0;

	ret = (((value) & DAMR_MULTIPORT_MODE_Msk) >> DAMR_MULTIPORT_MODE_Pos);

	return ret;
}

static inline void tcc_dai_tx_fifo_clear(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DRMR_OFFSET);

	value &= ~DRMR_TX_FIFO_CLEAR_Msk;

	dai_writel(value | DRMR_TX_FIFO_CLEAR, base_addr + TCC_DAI_DRMR_OFFSET);
}

static inline void tcc_dai_tx_fifo_release(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DRMR_OFFSET);

	value &= ~DRMR_TX_FIFO_CLEAR_Msk;

	dai_writel(
		value | DRMR_TX_FIFO_RELEASE,
		base_addr + TCC_DAI_DRMR_OFFSET);
}

static inline void tcc_dai_rx_fifo_clear(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DRMR_OFFSET);

	value &= ~DRMR_RX_FIFO_CLEAR_Msk;

	dai_writel(value | DRMR_RX_FIFO_CLEAR, base_addr + TCC_DAI_DRMR_OFFSET);
}

static inline void tcc_dai_rx_fifo_release(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_DAI_DRMR_OFFSET);

	value &= ~DRMR_RX_FIFO_CLEAR_Msk;

	dai_writel(
		value | DRMR_RX_FIFO_RELEASE,
		base_addr + TCC_DAI_DRMR_OFFSET);
}

static inline void tcc_dai_reg_backup(
	const void __iomem *base_addr,
	struct dai_reg_t *regs)
{
	regs->damr = readl(base_addr + TCC_DAI_DAMR_OFFSET);
	regs->mccr0 = readl(base_addr + TCC_DAI_MCCR0_OFFSET);
	regs->mccr1 = readl(base_addr + TCC_DAI_MCCR1_OFFSET);
	regs->drmr = readl(base_addr + TCC_DAI_DRMR_OFFSET);
 	regs->dclkdiv = readl(base_addr + TCC_DAI_DCLKDIV_OFFSET);
 }

static inline void tcc_dai_reg_restore(
	void __iomem *base_addr,
	const struct dai_reg_t *regs)
{
	dai_writel(regs->damr, base_addr + TCC_DAI_DAMR_OFFSET);
	dai_writel(regs->mccr0, base_addr + TCC_DAI_MCCR0_OFFSET);
	dai_writel(regs->mccr1, base_addr + TCC_DAI_MCCR1_OFFSET);
	dai_writel(regs->drmr, base_addr + TCC_DAI_DRMR_OFFSET);
 	dai_writel(regs->dclkdiv, base_addr + TCC_DAI_DCLKDIV_OFFSET);
}

static inline void tcc_dai_tx_gint_enable(void __iomem *girq_base, bool enable)
{
	uint32_t value = readl(girq_base + TCC_GINT_REQ_OFFSET);

	if (enable) {
		value |= ADMA_GINT_DAI_TX_Msk;
	} else {
		value &= ~ADMA_GINT_DAI_TX_Msk;
	}

	dai_writel(value, girq_base + TCC_GINT_REQ_OFFSET);
}

static inline uint32_t tcc_get_gint_status(const void __iomem *girq_base)
{
	return readl(girq_base + TCC_GINT_STATUS_OFFSET);
}

#endif /*_TCC_DAI_H*/
