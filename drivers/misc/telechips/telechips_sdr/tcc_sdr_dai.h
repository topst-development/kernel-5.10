/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef TCC_SDR_DAI_H
#define TCC_SDR_DAI_H

#include <linux/io.h>
#include "tcc_sdr_hw.h"

#define dai_writel(v, c, o)			writel(v, c+o)

enum TCC_SDR_DAI_BLK_NO {
	TCC_DAI_BLK_0 = 0u,
	TCC_DAI_BLK_1 = 1u,
	TCC_DAI_BLK_2 = 2u,
	TCC_DAI_BLK_3 = 3u,
	TCC_DAI_BLK_4 = 4u,
	TCC_DAI_BLK_5 = 5u,
	TCC_DAI_BLK_6 = 6u,
	TCC_DAI_BLK_7 = 7u,
	TCC_DAI_BLK_MAX = 8u
};

enum TCC_SDR_DAI_FMT {
	TCC_DAI_LSB_16 = 0u,
	TCC_DAI_LSB_24 = 1u,
	TCC_DAI_LSB_32 = 2u,
	TCC_DAI_MSB_16 = 3u,
	TCC_DAI_MSB_24 = 4u,
	TCC_DAI_MSB_32 = 5u,
};

enum TCC_SDR_DAI_CLKDIV_ID {
	TCC_DAI_CLKDIV_ID_MCLK_TO_BCLK = 0u
};

enum TCC_SDR_DAI_MCLK_DIV {
	TCC_DAI_MCLK_TO_BCLK_DIV_1 = 1u,	// TDM_ONLY
	TCC_DAI_MCLK_TO_BCLK_DIV_2 = 2u,	// TDM_ONLY
	TCC_DAI_MCLK_TO_BCLK_DIV_4 = 4u,
	TCC_DAI_MCLK_TO_BCLK_DIV_6 = 6u,
	TCC_DAI_MCLK_TO_BCLK_DIV_8 = 8u,
	TCC_DAI_MCLK_TO_BCLK_DIV_16 = 16u,
 	TCC_DAI_MCLK_TO_BCLK_DIV_24 = 24u,
	TCC_DAI_MCLK_TO_BCLK_DIV_32 = 32u,
	TCC_DAI_MCLK_TO_BCLK_DIV_48 = 48u,
	TCC_DAI_MCLK_TO_BCLK_DIV_64 = 64u
 };

enum TCC_SDR_DAI_BCLK_RATIO {
	TCC_DAI_BCLK_RATIO_32 = 32u,
	TCC_DAI_BCLK_RATIO_48 = 48u,
	TCC_DAI_BCLK_RATIO_64 = 64u,
#if !(defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X))
	TCC_DAI_BCLK_RATIO_512 = 512u
#endif
};

enum TCC_SDR_DAI_PATH {
	TCC_DAI_PATH_ADMA = 0u,
	TCC_DAI_PATH_ASRC = 1u
};

//TCC_IQ_FIFO_THRESH
#define TCC_IQ_FIFO_THRESH_64 (64u)
#define TCC_IQ_FIFO_THRESH_128 (128u)
#define TCC_IQ_FIFO_THRESH_256 (256u)


enum TCC_IQ_PORTSEL {
	TCC_IQ_PORTSEL_1 = 1u,
	TCC_IQ_PORTSEL_2 = 2u,
	TCC_IQ_PORTSEL_4 = 4u,
};

//TCC_IQ_BITMODE
#define	TCC_IQ_BITMODE_16 (16u)
#define	TCC_IQ_BITMODE_20 (20u)
#define	TCC_IQ_BITMODE_24 (24u)
#define	TCC_IQ_BITMODE_30 (30u)
#define	TCC_IQ_BITMODE_32 (32u)
#define	TCC_IQ_BITMODE_40 (40u)
#define	TCC_IQ_BITMODE_48 (48u)
#define	TCC_IQ_BITMODE_60 (60u)
#define	TCC_IQ_BITMODE_64 (64u)
#define	TCC_IQ_BITMODE_80 (80u)

enum TCC_SDR_CLOCK_ID {
	TCC_DAI_MCLK = 0u,
};

struct tcc_dai_reg_t {
	uint32_t damr;
	uint32_t mccr0;
	uint32_t mccr1;
	uint32_t davc;
	uint32_t diqmr;
	uint32_t dclkdiv;
#if defined(CONFIG_ARCH_TCC807X)
	uint32_t damr1;
#endif
};

enum tcc_i2s_block_type {
	DAI_BLOCK_STEREO_TYPE = 0u,
	DAI_BLOCK_7_1CH_TYPE = 1u,
	DAI_BLOCK_9_1CH_TYPE = 2u,
	DAI_BLOCK_TYPE_MAX  = 3u
};

enum tcc_i2s_audio_filter_type {
	DAI_AUDIO_CLK_FILTER_TYPE = 1u,
	DAI_AUDIO_DATA_FILTER_TYPE = 2u
};

static inline void tcc_sdr_daif_wr(
	uint32_t data, void __iomem *base_addr, uint32_t offset)
{
#if 0 // For debug
	if (offset == TCC_DAI_DAMR_OFFSET) {
		(void) pr_info("<ASoC> DAI_REG(0x%px) = 0x%08x [DAMR, %s]\n",
				(base_addr + offset), data, __func__);
	} else if (offset == TCC_DAI_DAVC_OFFSET) {
		(void) pr_info("<ASoC> DAI_REG(0x%px) = 0x%08x [DAVC, %s]\n",
				(base_addr + offset), data, __func__);
	} else if (offset == TCC_DAI_MCCR0_OFFSET) {
		(void) pr_info("<ASoC> DAI_REG(0x%px) = 0x%08x [MCCR0, %s]\n",
				(base_addr + offset), data, __func__);
	} else if (offset == TCC_DAI_MCCR1_OFFSET) {
		(void) pr_info("<ASoC> DAI_REG(0x%px) = 0x%08x [MCCR1, %s]\n",
				(base_addr + offset), data, __func__);
	} else if (offset == TCC_DAI_DIQMR_OFFSET) {
		(void) pr_info("<ASoC> DAI_REG(0x%px) = 0x%08x [DIQMR, %s]\n",
				(base_addr + offset), data, __func__);
	} else if (offset == TCC_DAI_DCLKDIV_OFFSET) {
		(void) pr_info("<ASoC> DAI_REG(0x%px) = 0x%08x [DCLKDIV, %s]\n",
				(base_addr + offset), data, __func__);
#if defined(CONFIG_ARCH_TCC807X)
	} else if (offset == TCC_MAIC_DAIF_DAMR1) {
		(void) pr_info("<ASoC> DAI_REG(0x%px) = 0x%08x [DAMR1, %s]\n",
				(base_addr + offset), data, __func__);
#endif
	} else {
		(void) pr_info("<ASoC> DAI_REG(0x%px) = 0x%08x [UnKnown, %s]\n",
				(base_addr + offset), data, __func__);
	}
#endif
	dai_writel(data, base_addr, offset);
}

static inline uint32_t tcc_sdr_daif_rd(
	const void __iomem *base_addr, uint32_t offset)
{
    uint32_t data = 0;
    data = readl(base_addr + offset);
    return data;
}

static inline void tcc_dai_dump(
	const void __iomem *base_addr)
{
	(void) pr_info("DAMR : 0x%08x\n",
					tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET));
	(void) pr_info("DAVC : 0x%08x\n",
					tcc_sdr_daif_rd(base_addr, TCC_DAI_DAVC_OFFSET));
	(void) pr_info("MCCR0: 0x%08x\n",
		     		tcc_sdr_daif_rd(base_addr, TCC_DAI_MCCR0_OFFSET));
	(void) pr_info("MCCR1: 0x%08x\n",
		     		tcc_sdr_daif_rd(base_addr, TCC_DAI_MCCR1_OFFSET));
	(void) pr_info("DIQMR : 0x%08x\n",
					tcc_sdr_daif_rd(base_addr, TCC_DAI_DIQMR_OFFSET));
 	(void) pr_info("DCLKDIV : 0x%08x\n",
		     		tcc_sdr_daif_rd(base_addr, TCC_DAI_DCLKDIV_OFFSET));
#if defined(CONFIG_ARCH_TCC807X)
	(void) pr_info("DAMR1 : 0x%08x\n",
					tcc_sdr_daif_rd(base_addr, TCC_MAIC_DAIF_DAMR1));
#endif
}

static inline void tcc_dai_damr_enable(
	void __iomem *base_addr, bool enable)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);

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

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_enable(
	void __iomem *base_addr, bool enable)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_ENABLE_MODE_Msk;
	if (enable) {
		value |= DAMR_DAI_ENABLE;
	} else {
		value |= DAMR_DAI_DISABLE;
	}

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DAMR_OFFSET);
}

static inline uint32_t tcc_dai_enable_check(
	const void __iomem *base_addr)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);
	uint32_t ret = 0;

	ret = (((value) & DAMR_DAI_ENABLE_MODE_Msk) >> DAMR_DAI_ENABLE_MODE_Pos);

	return ret;
}

static inline void tcc_dai_rx_enable(
	void __iomem *base_addr, bool enable)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_RECEIVER_MODE_Msk;
	if (enable) {
		value |= DAMR_DAI_RECEIVER_ENABLE;
	} else {
		value |= DAMR_DAI_RECEIVER_DISABLE;
	}

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DAMR_OFFSET);
}

static inline uint32_t tcc_dai_rx_check(
	const void __iomem *base_addr)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);
	uint32_t ret = 0;

	ret = (((value) & DAMR_DAI_RECEIVER_MODE_Msk) >> DAMR_DAI_RECEIVER_MODE_Pos);

	return ret;
}

static inline void tcc_dai_tx_enable(
	void __iomem *base_addr, bool enable)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_TRANSMITTER_MODE_Msk;
	if (enable) {
		value |= DAMR_DAI_TRANSMITTER_ENABLE;
	} else {
		value |= DAMR_DAI_TRANSMITTER_DISABLE;
	}

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DAMR_OFFSET);
}

static inline uint32_t tcc_dai_tx_check(
	const void __iomem *base_addr)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);
	uint32_t ret = 0;

	ret = (((value) & DAMR_DAI_TRANSMITTER_MODE_Msk) >> DAMR_DAI_TRANSMITTER_MODE_Pos);

	return ret;
}

static inline void tcc_dai_tx2rx_loopback_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_TX2RX_LOOPBACK_Msk;
	if (enable) {
		value |= DAMR_DAI_TX2RX_LOOPBACK_ENABLE;
	} else {
		value |= DAMR_DAI_TX2RX_LOOPBACK_DISABLE;
	}

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DAMR_OFFSET);
}

static inline uint32_t tcc_dai_tx2rx_loopback_check(
	const void __iomem *base_addr)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);
	uint32_t ret = 0;

	ret = (((value) & DAMR_DAI_TX2RX_LOOPBACK_Msk) >> DAMR_DAI_TX2RX_LOOPBACK_Pos);

	return ret;
}

static inline void tcc_dai_rx2tx_loopback_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_RX2TX_LOOPBACK_Msk;
	if (enable) {
		value |= DAMR_DAI_RX2TX_LOOPBACK_ENABLE;
	} else {
		value |= DAMR_DAI_RX2TX_LOOPBACK_DISABLE;
	}

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DAMR_OFFSET);
}

static inline uint32_t tcc_dai_rx2tx_loopback_check(
	const void __iomem *base_addr)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);
	uint32_t ret = 0;

	ret = (((value) & DAMR_DAI_RX2TX_LOOPBACK_Msk) >> DAMR_DAI_RX2TX_LOOPBACK_Pos);

	return ret;
}

static inline void tcc_dai_dma_threshold_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_BUF_THRESHOLD_MODE_Msk;
	if (enable) {
		value |= DAMR_DAI_BUF_THRESHOLD_ENABLE;
	} else {
		value |= DAMR_DAI_BUF_THRESHOLD_DISABLE;
	}

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_bitclk_polarity(
	void __iomem *base_addr,
	bool positive)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_DAI_BIT_CLK_POLARITY_Msk;
	if (positive) {
		value |= DAMR_DAI_BIT_CLK_POSITIVE;
	} else {
		value |= DAMR_DAI_BIT_CLK_NAGATIVE;
	}

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_i2s_mode(
	void __iomem *base_addr)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);

	value &=
		~(DAMR_RX_JUSTIFIED_MODE_Msk
		| DAMR_TX_JUSTIFIED_MODE_Msk
		| DAMR_DAI_SYNC_MODE_Msk
		| DAMR_DSP_MODE_Msk);

	value |= (DAMR_DAI_SYNC_IIS_DSP_TDM | DAMR_DSP_IIS_MODE);

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_right_j_mode(
	void __iomem *base_addr,
	uint32_t bclk_ratio)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);

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

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_left_j_mode(
	void __iomem *base_addr)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);

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

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DAMR_OFFSET);
}

static inline void tcc_dai_set_master_mode(
	void __iomem *base_addr,
	bool mclk_master, bool bclk_master,
	bool lrck_master, bool is_pinctrl_export)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);

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

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DAMR_OFFSET);
}

/* Workaround Code for TCC803X, TCC899X and TCC901X
 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
 * So, we should always restore DCLKDIV value while write that
 * value to register
 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
static inline uint32_t tcc_dai_get_mclk_div(
	const void __iomem *base_addr,
	uint32_t backup_dclkdiv,
	bool tdm_mode,
	bool workaround)
#else
static inline uint32_t tcc_dai_get_mclk_div(
	const void __iomem *base_addr,
	bool tdm_mode)
#endif
{
	uint32_t mccr0 = tcc_sdr_daif_rd(base_addr, TCC_DAI_MCCR0_OFFSET);
	uint32_t dclkdiv = tcc_sdr_daif_rd(base_addr, TCC_DAI_DCLKDIV_OFFSET);
	uint32_t mclk_div = 0, ret = 0;

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
	if (workaround == TRUE) {
		dclkdiv = backup_dclkdiv;
	}
#endif
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

/* Workaround Code for TCC803X, TCC899X and TCC901X
 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
 * So, we should always restore DCLKDIV value while write that
 * value to register
 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
static inline uint32_t tcc_dai_get_bclk_ratio(
	const void __iomem *base_addr,
	uint32_t backup_dclkdiv,
	bool tdm_mode,
	bool workaround)
#else
static inline uint32_t tcc_dai_get_bclk_ratio(
	const void __iomem *base_addr,
	bool tdm_mode)
#endif
{
	uint32_t dclkdiv = tcc_sdr_daif_rd(base_addr, TCC_DAI_DCLKDIV_OFFSET);
	uint32_t bclk_ratio = 0, ret = 0;

	if (tdm_mode == FALSE) {
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
		if (workaround == TRUE) {
			dclkdiv = backup_dclkdiv;
		}

		bclk_ratio = dclkdiv & DCLKDIV_DAI_FRAME_CLK_DIV_Msk;
		ret =
			(bclk_ratio == DCLKDIV_DAI_FRAME_CLK_DIV_32) ?
			(uint32_t)TCC_DAI_BCLK_RATIO_32 :
			(bclk_ratio ==  DCLKDIV_DAI_FRAME_CLK_DIV_48) ?
			(uint32_t)TCC_DAI_BCLK_RATIO_48 :
			(uint32_t)TCC_DAI_BCLK_RATIO_64;
#else
		bclk_ratio = dclkdiv & DCLKDIV_DAI_FRAME_CLK_DIV_Msk;
		ret =
			(bclk_ratio == DCLKDIV_DAI_FRAME_CLK_DIV_32) ?
			(uint32_t)TCC_DAI_BCLK_RATIO_32 :
			(bclk_ratio == DCLKDIV_DAI_FRAME_CLK_DIV_48) ?
			(uint32_t)TCC_DAI_BCLK_RATIO_48 :
			(bclk_ratio == DCLKDIV_DAI_FRAME_CLK_DIV_64) ?
			(uint32_t)TCC_DAI_BCLK_RATIO_64 :
			(uint32_t)TCC_DAI_BCLK_RATIO_512;
#endif
	}
	return ret;
}
/* Workaround Code for TCC803X, TCC899X and TCC901X
 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
 * So, we should always restore DCLKDIV value while write that
 * value to register
 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) ||\
	defined(CONFIG_ARCH_TCC901X)
static inline uint32_t tcc_dai_set_clk_mode(
		void __iomem *base_addr,
		uint32_t mclk_div,
		uint32_t bclk_ratio,
		bool tdm_mode)
#else
static inline void tcc_dai_set_clk_mode(
		void __iomem *base_addr,
		uint32_t mclk_div,
		uint32_t bclk_ratio,
		bool tdm_mode)
#endif
{
	uint32_t mccr0 = tcc_sdr_daif_rd(base_addr, TCC_DAI_MCCR0_OFFSET);
	uint32_t dclkdiv = tcc_sdr_daif_rd(base_addr, TCC_DAI_DCLKDIV_OFFSET);

	dclkdiv &=
			~(DCLKDIV_DAI_BIT_CLK_DIV_Msk
		     |DCLKDIV_DAI_FRAME_CLK_DIV_Msk);

	mccr0 &= ~MCCR0_TDM_BIT_CLK_DIV_Msk;

	if (tdm_mode) {
		dclkdiv |= DCLKDIV_DAI_FRAME_CLK_DIV_X;	// xfs->fs
		dclkdiv |= DCLKDIV_DAI_BIT_CLK_DIV_64; //TDM mode is don't care.

		mccr0 |=
		    (mclk_div == (uint32_t)TCC_DAI_MCLK_TO_BCLK_DIV_1) ?
			MCCR0_TDM_BIT_CLK_DIV_1 :
			(mclk_div == (uint32_t)TCC_DAI_MCLK_TO_BCLK_DIV_2) ?
			MCCR0_TDM_BIT_CLK_DIV_2 :
			MCCR0_TDM_BIT_CLK_DIV_DISABLE;
	} else {
		dclkdiv |=
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

		dclkdiv |=
		    (bclk_ratio == (uint32_t)TCC_DAI_BCLK_RATIO_32) ?
			DCLKDIV_DAI_FRAME_CLK_DIV_32 :
#if !(defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X))
		    (bclk_ratio == (uint32_t)TCC_DAI_BCLK_RATIO_48) ?
			DCLKDIV_DAI_FRAME_CLK_DIV_48 :
		    (bclk_ratio == (uint32_t)TCC_DAI_BCLK_RATIO_64) ?
			DCLKDIV_DAI_FRAME_CLK_DIV_64 :
			DCLKDIV_DAI_FRAME_CLK_DIV_512;
#else
		    (bclk_ratio == (uint32_t)TCC_DAI_BCLK_RATIO_48) ?
			DCLKDIV_DAI_FRAME_CLK_DIV_48 :
			DCLKDIV_DAI_FRAME_CLK_DIV_64;
#endif

		mccr0 |= MCCR0_TDM_BIT_CLK_DIV_DISABLE;

	}

	tcc_sdr_daif_wr(dclkdiv, base_addr, TCC_DAI_DCLKDIV_OFFSET);
	tcc_sdr_daif_wr(mccr0, base_addr, TCC_DAI_MCCR0_OFFSET);

/* Workaround Code for TCC803X, TCC899X and TCC901X
 * Stereo & 9.1ch Audio IPs cannot read DCLKDIV register (0x54)
 * So, we should always restore DCLKDIV value while write that
 * value to register
 */
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)
	return dclkdiv;
#endif

}

static inline void tcc_dai_set_tx_format(
	void __iomem *base_addr,
	enum TCC_SDR_DAI_FMT fmt)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);

	value &= ~(DAMR_DAI_TX_SHIFT_Msk);

	value |=
		((fmt == TCC_DAI_MSB_24) || (fmt == TCC_DAI_MSB_32)) ? (DAMR_DAI_TX_SHIFT_MSB_24) :
	    (fmt == TCC_DAI_MSB_16) ? (DAMR_DAI_TX_SHIFT_MSB_16) :
	    ((fmt == TCC_DAI_LSB_24) || (fmt == TCC_DAI_LSB_32)) ? (DAMR_DAI_TX_SHIFT_LSB_24) :
		(DAMR_DAI_TX_SHIFT_LSB_16);

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DAMR_OFFSET);
	if((fmt == TCC_DAI_MSB_32) || (fmt == TCC_DAI_LSB_32)) {
#if defined(CONFIG_ARCH_TCC807X)
		//TBD setting for 32bit
#endif
	}
}

static inline void tcc_dai_set_rx_format(
	void __iomem *base_addr,
	enum TCC_SDR_DAI_FMT fmt)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);

	value &= ~(DAMR_DAI_RX_SHIFT_Msk);

	value |=
		((fmt == TCC_DAI_MSB_24) || (fmt == TCC_DAI_MSB_32)) ? (DAMR_DAI_RX_SHIFT_MSB_24) :
	    (fmt == TCC_DAI_MSB_16) ? (DAMR_DAI_RX_SHIFT_MSB_16) :
	    ((fmt == TCC_DAI_LSB_24) || (fmt == TCC_DAI_LSB_32)) ? (DAMR_DAI_RX_SHIFT_LSB_24) :
		(DAMR_DAI_RX_SHIFT_LSB_16);

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DAMR_OFFSET);
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
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_AUDIO_FILTER_MODE_Msk;

	if (enable) {
		value |= DAMR_AUDIO_FILTER_ENABLE;
	} else {
		value |= DAMR_AUDIO_FILTER_DISABLE;
	}

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DAMR_OFFSET);
}

#if !(defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X))
static inline void tcc_dai_set_audio_data_filter_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_AUDIO_DATA_FILTER_MODE_Msk;

	if (enable) {
		value |= DAMR_AUDIO_DATA_FILTER_ENABLE;
	} else {
		value |= DAMR_AUDIO_DATA_FILTER_DISABLE;
	}

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DAMR_OFFSET);
}
#endif
static inline void tcc_dai_set_dsp_tdm_frame_invert(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_MCCR0_OFFSET);

	value &= ~MCCR0_FRAME_INVERT_Msk;

	value |=
	    (enable) ? MCCR0_FRAME_INVERT_ENABLE : MCCR0_FRAME_INVERT_DISABLE;

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_MCCR0_OFFSET);
}

static inline void tcc_dai_set_dao_mask(
	void __iomem *base_addr,
	bool dao0,
	bool dao1,
	bool dao2,
	bool dao3,
	bool dao4)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_MCCR0_OFFSET);

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

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_MCCR0_OFFSET);
}

static inline void tcc_dai_set_dao_path_sel_inner(
	void __iomem *base_addr,
	enum TCC_SDR_DAI_PATH dao0,
	enum TCC_SDR_DAI_PATH dao1,
	enum TCC_SDR_DAI_PATH dao2,
	enum TCC_SDR_DAI_PATH dao3)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_MCCR1_OFFSET);

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

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_MCCR1_OFFSET);
}

static inline void tcc_dai_set_dao_path_sel(
	void __iomem *base_addr,
	enum TCC_SDR_DAI_PATH dao_path)
{
	tcc_dai_set_dao_path_sel_inner(base_addr,
			dao_path, dao_path, dao_path, dao_path);
}

static inline void tcc_dai_set_multiport_mode(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);

	value &= ~DAMR_MULTIPORT_MODE_Msk;

	if (enable) {
		value |= DAMR_MULTIPORT_ENABLE;
	} else {
		value |= DAMR_MULTIPORT_DISABLE;
	}

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DAMR_OFFSET);
}

static inline uint32_t tcc_dai_multiport_mode_check(
	const void __iomem *base_addr)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);
	uint32_t ret = 0;

	ret = (((value) & DAMR_MULTIPORT_MODE_Msk) >> DAMR_MULTIPORT_MODE_Pos);

	return ret;
}

static inline void tcc_dai_set_tx_mute(
	void __iomem *base_addr, bool enable)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAVC_OFFSET);

	value &= ~DAVC_DAI_TX_VOLUME_CONTROL_Msk;

	if (enable) {
		value |= DAVC_DAI_TX_VOLUME_MINUS_96DB;
	} else {
		value |= DAVC_DAI_TX_VOLUME_0DB;
	}

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DAVC_OFFSET);
}

static inline void tcc_dai_set_rx_mute(
	void __iomem *base_addr, bool enable)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAVC_OFFSET);

	value &= ~DAVC_DAI_RX_MUTE_CTRL_Msk;

	if (enable) {
		value |= DAVC_DAI_RX_MUTE_ENABLE;
	} else {
		value |= DAVC_DAI_RX_MUTE_DISABLE;
	}

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DAVC_OFFSET);
}

static inline void tcc_dai_tx_fifo_clear(
	void __iomem *base_addr)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DIQMR_OFFSET);

	value &= ~DIQMR_TX_FIFO_CLEAR_Msk;

	tcc_sdr_daif_wr(value | DIQMR_TX_FIFO_CLEAR,
						base_addr, TCC_DAI_DIQMR_OFFSET);
}

static inline void tcc_dai_tx_fifo_release(
	void __iomem *base_addr)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DIQMR_OFFSET);

	value &= ~DIQMR_TX_FIFO_CLEAR_Msk;

	tcc_sdr_daif_wr(
		value | DIQMR_TX_FIFO_RELEASE,
		base_addr, TCC_DAI_DIQMR_OFFSET);
}

static inline void tcc_dai_rx_fifo_clear(
	void __iomem *base_addr)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DIQMR_OFFSET);

	value &= ~DIQMR_RX_FIFO_CLEAR_Msk;

	tcc_sdr_daif_wr(value | DIQMR_RX_FIFO_CLEAR,
						base_addr, TCC_DAI_DIQMR_OFFSET);
}

static inline void tcc_dai_rx_fifo_release(
	void __iomem *base_addr)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DIQMR_OFFSET);

	value &= ~DIQMR_RX_FIFO_CLEAR_Msk;

	tcc_sdr_daif_wr(
		value | DIQMR_RX_FIFO_RELEASE,
		base_addr, TCC_DAI_DIQMR_OFFSET);
}

static inline void tcc_digital_iq_enable(
	void __iomem *base_addr, bool enable)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DIQMR_OFFSET);

	value &= ~DIQMR_IQ_SLV_ENABLE_MODE_Msk;
	if (enable) {
		value |= DIQMR_IQ_SLV_ENABLE;
	} else {
		value |= DIQMR_IQ_SLV_DISABLE;
	}

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DIQMR_OFFSET);
}

static inline void tcc_digital_iq_set_fifo_threshold(
	void __iomem *base_addr,
	uint32_t threshold)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DIQMR_OFFSET);

	value &= ~(DIQMR_FIFO_THRESH_Msk);

	value |= (threshold == TCC_IQ_FIFO_THRESH_64) ?
		(DIQMR_FIFO_THRESH_64) :
		(threshold == TCC_IQ_FIFO_THRESH_128) ?
		(DIQMR_FIFO_THRESH_128) : (DIQMR_FIFO_THRESH_256);

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DIQMR_OFFSET);
}

static inline void tcc_digital_iq_set_portsel(
	void __iomem *base_addr,
	uint32_t portsel)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DIQMR_OFFSET);

	value &= ~(DIQMR_PORTSEL_Msk);

	value |= (portsel == (uint32_t)TCC_IQ_PORTSEL_1) ? (DIQMR_PORTSEL_1) :
		(portsel == (uint32_t)TCC_IQ_PORTSEL_2) ?
		(DIQMR_PORTSEL_2) : (DIQMR_PORTSEL_4);

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DIQMR_OFFSET);
}

static inline void tcc_digital_iq_set_bitmode(
	void __iomem *base_addr,
	uint32_t bitmode)
{
	uint32_t value = tcc_sdr_daif_rd(base_addr, TCC_DAI_DIQMR_OFFSET);

	value &= ~(DIQMR_BITMODE_Msk);

	value |= (bitmode == TCC_IQ_BITMODE_16) ?
			(DIQMR_BITMODE_16) :
		(bitmode == TCC_IQ_BITMODE_20) ?
			(DIQMR_BITMODE_20) :
		(bitmode == TCC_IQ_BITMODE_24) ?
			(DIQMR_BITMODE_24) :
		(bitmode == TCC_IQ_BITMODE_30) ?
			(DIQMR_BITMODE_30) :
		(bitmode == TCC_IQ_BITMODE_32) ?
			(DIQMR_BITMODE_32) :
		(bitmode == TCC_IQ_BITMODE_40) ?
			(DIQMR_BITMODE_40) :
		(bitmode == TCC_IQ_BITMODE_48) ?
			(DIQMR_BITMODE_48) :
		(bitmode == TCC_IQ_BITMODE_60) ?
			(DIQMR_BITMODE_60) :
		(bitmode == TCC_IQ_BITMODE_64) ?
			(DIQMR_BITMODE_64) : (DIQMR_BITMODE_80);

	tcc_sdr_daif_wr(value, base_addr, TCC_DAI_DIQMR_OFFSET);
}

static inline void tcc_dai_reg_backup(
	const void __iomem *base_addr,
	struct tcc_dai_reg_t *regs)
{
	regs->damr = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAMR_OFFSET);
	regs->mccr0 = tcc_sdr_daif_rd(base_addr, TCC_DAI_MCCR0_OFFSET);
	regs->mccr1 = tcc_sdr_daif_rd(base_addr, TCC_DAI_MCCR1_OFFSET);
	regs->davc = tcc_sdr_daif_rd(base_addr, TCC_DAI_DAVC_OFFSET);
	regs->diqmr = tcc_sdr_daif_rd(base_addr, TCC_DAI_DIQMR_OFFSET);
 	regs->dclkdiv = tcc_sdr_daif_rd(base_addr, TCC_DAI_DCLKDIV_OFFSET);
}

static inline void tcc_dai_reg_restore(
	void __iomem *base_addr,
	const struct tcc_dai_reg_t *regs)
{
	tcc_sdr_daif_wr(regs->damr, base_addr, TCC_DAI_DAMR_OFFSET);
	tcc_sdr_daif_wr(regs->mccr0, base_addr, TCC_DAI_MCCR0_OFFSET);
	tcc_sdr_daif_wr(regs->mccr1, base_addr, TCC_DAI_MCCR1_OFFSET);
	tcc_sdr_daif_wr(regs->davc, base_addr, TCC_DAI_DAVC_OFFSET);
	tcc_sdr_daif_wr(regs->diqmr, base_addr, TCC_DAI_DIQMR_OFFSET);
 	tcc_sdr_daif_wr(regs->dclkdiv, base_addr, TCC_DAI_DCLKDIV_OFFSET);
#if defined(CONFIG_ARCH_TCC807X)
 	tcc_sdr_daif_wr(regs->damr1, base_addr, TCC_MAIC_DAIF_DAMR1);
#endif
}

static inline void tcc_dai_tx_gint_enable(
	void __iomem *girq_base, bool enable)
{
	uint32_t value = tcc_sdr_daif_rd(girq_base, TCC_GINT_REQ_OFFSET);

	if (enable) {
		value |= ADMA_GINT_DAI_TX_Msk;
	} else {
		value &= ~ADMA_GINT_DAI_TX_Msk;
	}

	tcc_sdr_daif_wr(value, girq_base, TCC_GINT_REQ_OFFSET);
}

static inline uint32_t tcc_get_gint_status(
	const void __iomem *girq_base)
{
	return tcc_sdr_daif_rd(girq_base, TCC_GINT_STATUS_OFFSET);
}

#if defined(CONFIG_ARCH_TCC807X)

#define TCC_INTERFACE_ADMA	(0x0U)
#define TCC_INTERFACE_TAS	(0x1U)

static inline void tcc_audio_maic_select_interface(
	void __iomem *base_addr, uint32_t interface)
{

	uint32_t rdata = tcc_sdr_daif_rd(base_addr, TCC_MAIC_DAIF_DAMR1);

	rdata = rdata & ~(TCC_MAIC_DAMR1_TAS_SEL_MsK);
	if(interface == TCC_INTERFACE_ADMA){
		rdata |= TCC_MAIC_DAMR1_TAS_SEL_ADMA;
	}else{
		rdata |= TCC_MAIC_DAMR1_TAS_SEL_TAS;
	}

	tcc_sdr_daif_wr(rdata, base_addr, TCC_MAIC_DAIF_DAMR1);
}
#endif //#if defined(CONFIG_ARCH_TCC807X)

#endif /*TCC_SDR_DAI_H*/
