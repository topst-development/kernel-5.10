/* SPDX-License-Identifier: GPL-2.0-or-later */
/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef TCC_SDR_ADMA_H
#define TCC_SDR_ADMA_H

#include <linux/io.h>
#include "tcc_sdr_hw.h"
#include "tcc_sdr_rule.h"

enum TCC_SDR_ADMA_I2S_TYPE {
	TCC_ADMA_I2S_STEREO = 0,
	TCC_ADMA_I2S_7_1CH = 1,
	TCC_ADMA_I2S_9_1CH = 2,
	TCC_ADMA_I2S_TYPE_MAX,
};

enum TCC_SDR_ADMA_DATA_WIDTH {
	TCC_ADMA_DATA_WIDTH_16 = 0,
	TCC_ADMA_DATA_WIDTH_24 = 1,
	TCC_ADMA_DATA_WIDTH_32 = 2,
};

enum TCC_SDR_ADMA_WORD_SIZE { //2^n * 8
	TCC_ADMA_WORD_SIZE_8 = 0,
	TCC_ADMA_WORD_SIZE_16 = 1,
	TCC_ADMA_WORD_SIZE_32 = 2,
};

//TCC_SDR_ADMA_BURST_SIZE 2^n
#define TCC_SDR_ADMA_BURST_CYCLE_1 	0U
#define TCC_SDR_ADMA_BURST_CYCLE_2	1U
#define TCC_SDR_ADMA_BURST_CYCLE_4	2U
#define TCC_SDR_ADMA_BURST_CYCLE_8	3U
#define TCC_SDR_ADMA_BURST_CYCLE_16	4U

enum TCC_SDR_ADMA_MULTI_CH_MODE {
	TCC_ADMA_MULTI_CH_MODE_3_1 = 0,
	TCC_ADMA_MULTI_CH_MODE_5_1_012 = 1,
	TCC_ADMA_MULTI_CH_MODE_5_1_013 = 2,
	TCC_ADMA_MULTI_CH_MODE_7_1	= 3,
};

enum TCC_SDR_ADMA_REPEAT_MODE {
	TCC_ADMA_REPEAT_FROM_CUR_ADDR = 0,
	TCC_ADMA_REPEAT_FROM_START_ADDR = 1,
};


#define unused(x) (void)(x)

#define adma_writel(v, c, o) writel(v, c+o)

struct tcc_sdr_dma_t{
	void __iomem *base_addr;
	//dma_addr_t dma_addr;
	//dma_addr_t mono_dma_addr;
	uint32_t buffer_bytes;
	uint32_t period_bytes;
};

static inline void tcc_sdr_adma_wr(
	uint32_t data, void __iomem *base_addr, uint32_t offset)
{
#if 0 // For debug
if (offset == TCC_ADMA_RXDADAR_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [RxDaDar0, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_RXDAPARAM_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [RxDaParam, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_RXDATCNT_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [RxDaTCnt, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_RXDACDAR_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [RxDaCdar0, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_RXCDDAR_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [RxCdDar, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_RXCDPARAM_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [RxCdParam, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_RXCDTCNT_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [RxCdTCnt, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_RXCDCDAR_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [RxCdCdar, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_RXDADARL_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [RxDaDarL0, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_RXDACDARL_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [RxDaCdarL0, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_RXCDDARL_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [RxCdDarL, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_RXCDCDARL_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [RxCdCdarL, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_TRANSCTRL_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [TransCtrl, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_RPTCTRL_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [RptCtrl, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_TXDASAR_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [TxDaSar0, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_TXDAPARAM_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [TxDaParam, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_TXDATCNT_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [TxDaTCnt, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_TXDACSAR_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [TxDaCsar0, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_TXSPSAR_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [TxSpSar, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_TXSPPARAM_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [TxSpParam, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_TXSPTCNT_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [TxSpTCnt, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_TXSPCSAR_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [TxSpCsar, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_TXDASARL_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [TxDaSarL0, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_TXDACSARL_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [TxDaCsarL0, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_TXSPSARL_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [TxSpSarL, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_TXSPCSARL_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [TxSpCsarL, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_CHCTRL_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [ChCtrl, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_INTSTATUS_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [IntStatus, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_GINTREQ_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [GIntReq, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_GINTSTATUS_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [GIntStatus, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_TXDAADRCNT_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [TxDaAdrCnt, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_RXDAADRCNT_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [RxDaAdrCnt, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_TXSPADRCNT_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [TxSpAdrCnt, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_RXSPADRCNT_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [RxSpAdrCnt, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_RXDADAR1_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [RxDaDar1, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_RXDADAR2_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [RxDaDar2, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_RXDADAR3_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [RxDaDar3, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_RXDACAR1_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [RxDaCar1, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_RXDACAR2_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [RxDaCar2, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_RXDACAR3_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [RxDaCar3, %s]\n",
		(base_addr+offset), data, __func__);
 } else if (offset == TCC_ADMA_RESET_OFFSET) {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [ADMARST, %s]\n",
		(base_addr+offset), data, __func__);
 } else {
	(void) pr_info("<ASoC> ADMA_REG(0x%px) = 0x%08x [UnKnown, %s]\n",
		(base_addr+offset), data, __func__);
}
#endif
	adma_writel(data, base_addr, offset);
}

static inline uint32_t tcc_sdr_adma_rd(
	const void __iomem *base_addr, uint32_t offset)
{
    uint32_t data = 0;
    data = readl(base_addr + offset);
    return data;
}

static inline void tcc_adma_dump(const void __iomem *base_addr)
{
	uint32_t value, offset;

	for (offset = 0u; offset <= si_to_ui(TCC_ADMA_RESET_OFFSET); offset += 4u) {
		value = tcc_sdr_adma_rd(base_addr, offset);
		(void) pr_info("ADMA_REG(0x%03x) : 0x%08x\n",
			(uint32_t)offset,
			value);
	}
}

static inline void tcc_audio_sw_reset_enable(
	void __iomem *base_addr,
	uint32_t reg_offset,
	uint32_t bit_offset,
	bool enable)
{
	uint32_t value = tcc_sdr_adma_rd(base_addr, reg_offset);
	uint32_t mask_bit = 0u;

	if (bit_offset < 32u) {
		mask_bit = ui_lshift(1u, bit_offset);

		if (enable) {
			value &= ~mask_bit;
		} else {
			value |= mask_bit;
		}

		tcc_sdr_adma_wr(value, base_addr, reg_offset);
	} else {
		(void) pr_info("[%s] bit_offset is wrong : %d\n",
			__func__,
			bit_offset);
	}

}

static inline void tcc_adma_tx_reset_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = tcc_sdr_adma_rd(base_addr, TCC_ADMA_RESET_OFFSET);

	value &= ~ADMA_RESET_DMA_TX_Msk;
	if (enable) {
		value |= ADMA_RESET_DMA_TX_RESET;
	} else {
		value |= ADMA_RESET_DMA_TX_RELEASE;
	}
	tcc_sdr_adma_wr(value, base_addr, TCC_ADMA_RESET_OFFSET);
}

static inline void tcc_adma_rx_reset_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = tcc_sdr_adma_rd(base_addr, TCC_ADMA_RESET_OFFSET);

	value &= ~ADMA_RESET_DMA_RX_Msk;
	if (enable) {
		value |= ADMA_RESET_DMA_RX_RESET;
	} else {
		value |= ADMA_RESET_DMA_RX_RELEASE;
	}
	tcc_sdr_adma_wr(value, base_addr, TCC_ADMA_RESET_OFFSET);
}

static inline void tcc_adma_dai_rx_reset_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = tcc_sdr_adma_rd(base_addr, TCC_ADMA_RESET_OFFSET);

	value &= ~ADMA_RESET_DMA_DAI_RX_Msk;
	if (enable) {
		value |= ADMA_RESET_DMA_DAI_RX_RESET;
	} else {
		value |= ADMA_RESET_DMA_DAI_RX_RELEASE;
	}
	tcc_sdr_adma_wr(value, base_addr, TCC_ADMA_RESET_OFFSET);
}

static inline void tcc_adma_dai_rx_irq_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = tcc_sdr_adma_rd(base_addr, TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_DAI_RX_IRQ_MODE_Msk;
	if (enable) {
		value |= ADMA_CHCTRL_DAI_RX_IRQ_ENABLE;
	} else {
		value |= ADMA_CHCTRL_DAI_RX_IRQ_DISABLE;
	}
	tcc_sdr_adma_wr(value, base_addr, TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_dai_rx_dma_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = tcc_sdr_adma_rd(base_addr, TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_DAI_RX_DMA_MODE_Msk;
	if (enable) {
		value |= ADMA_CHCTRL_DAI_RX_DMA_ENABLE;
	} else {
		value |= ADMA_CHCTRL_DAI_RX_DMA_DISABLE;
	}
	tcc_sdr_adma_wr(value, base_addr, TCC_ADMA_CHCTRL_OFFSET);
}

static inline bool tcc_adma_dai_rx_dma_enable_check(
	const void __iomem *base_addr)
{
	uint32_t int_status = tcc_sdr_adma_rd(base_addr, TCC_ADMA_CHCTRL_OFFSET);
	bool ret = FALSE;

	if ((int_status & (ADMA_CHCTRL_DAI_RX_DMA_MODE_Msk)) > (uint32_t) 0) {
		ret = TRUE;
	}

	return ret;
}

static inline bool tcc_adma_dai_rx_irq_check(
	const void __iomem *base_addr)
{
	uint32_t int_status = tcc_sdr_adma_rd(base_addr, TCC_ADMA_INTSTATUS_OFFSET);
	bool ret = FALSE;

	if ((int_status & (ADMA_ISTAT_DAI_RX_MASKED_Msk |
			ADMA_ISTAT_DAI_RX_Msk)) > (uint32_t) 0) {
		ret = TRUE;
	}

	return ret;
}

static inline bool tcc_adma_dai_rx_dma_adrcnt_mode_check(
	const void __iomem *base_addr)
{
	uint32_t int_status = tcc_sdr_adma_rd(base_addr, TCC_ADMA_RXDAADRCNT_OFFSET);
	bool ret = FALSE;

	if ((int_status & (ADMA_ADRCNT_MODE_Msk)) > (uint32_t) 0) {
		ret = TRUE;
	}

	return ret;
}

static inline void tcc_adma_dai_rx_irq_clear(
	void __iomem *base_addr)
{
	tcc_sdr_adma_wr(
		ADMA_ISTAT_DAI_RX_MASKED_Msk|ADMA_ISTAT_DAI_RX_Msk,
		base_addr, TCC_ADMA_INTSTATUS_OFFSET);
}

static inline void tcc_adma_set_dai_rx_lrmode(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = tcc_sdr_adma_rd(base_addr, TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_DAI_RX_LR_MODE_Msk;

	if (enable) {
		value |= ADMA_CHCTRL_DAI_RX_LR_ENABLE;
	} else {
		value |= ADMA_CHCTRL_DAI_RX_LR_DISABLE;
	}

	tcc_sdr_adma_wr(value, base_addr, TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_rx_dma_width(
	void __iomem *base_addr,
	enum TCC_SDR_ADMA_DATA_WIDTH width)
{
	uint32_t value = tcc_sdr_adma_rd(base_addr, TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_DAI_RXD_WIDTH_Msk;
	value |=
		((width == TCC_ADMA_DATA_WIDTH_24) || (width == TCC_ADMA_DATA_WIDTH_32)) ?
		ADMA_CHCTRL_DAI_RXD_WIDTH_24BIT :
		ADMA_CHCTRL_DAI_RXD_WIDTH_16BIT;

	tcc_sdr_adma_wr(value, base_addr, TCC_ADMA_CHCTRL_OFFSET);
	if(width == TCC_ADMA_DATA_WIDTH_32) {
#if defined(CONFIG_ARCH_TCC807X)
		//TBD setting for 32bit
#endif
	}
}

static inline void tcc_adma_set_dai_rx_transfer_size(
	void __iomem *base_addr,
	uint32_t wsize,
	uint32_t bsize)
{
	uint32_t value = tcc_sdr_adma_rd(base_addr, TCC_ADMA_TRANSCTRL_OFFSET);

	value &=
		~(ADMA_TRANCTRL_DAIRX_WSIZE_Msk |
		ADMA_TRANCTRL_DAIRX_BSIZE_Msk);

	value |=
		(wsize == (uint32_t)TCC_ADMA_WORD_SIZE_8) ?
		ADMA_TRANCTRL_DAIRX_WSIZE_8 :
		(wsize == (uint32_t)TCC_ADMA_WORD_SIZE_16) ?
		ADMA_TRANCTRL_DAIRX_WSIZE_16 :
		ADMA_TRANCTRL_DAIRX_WSIZE_32;

	value |=
		(bsize == (uint32_t)TCC_SDR_ADMA_BURST_CYCLE_1) ?
		ADMA_TRANCTRL_DAIRX_BCYCLE_1 :
		(bsize == (uint32_t)TCC_SDR_ADMA_BURST_CYCLE_2) ?
		ADMA_TRANCTRL_DAIRX_BCYCLE_2 :
		(bsize == (uint32_t)TCC_SDR_ADMA_BURST_CYCLE_4) ?
		ADMA_TRANCTRL_DAIRX_BCYCLE_4 :
		ADMA_TRANCTRL_DAIRX_BCYCLE_8;

	tcc_sdr_adma_wr(value, base_addr, TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_rx_dma_repeat_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = tcc_sdr_adma_rd(base_addr, TCC_ADMA_TRANSCTRL_OFFSET);

	if (enable) {
		value |= ADMA_TRANCTRL_DAIRX_REPEAT_EN;
	} else {
		value &= ~ADMA_TRANCTRL_DAIRX_REPEAT_EN;
	}

	tcc_sdr_adma_wr(value, base_addr, TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_rx_multi_ch(
	void __iomem *base_addr,
	bool enable,
	enum TCC_SDR_ADMA_MULTI_CH_MODE mode)
{
	uint32_t value = tcc_sdr_adma_rd(base_addr, TCC_ADMA_CHCTRL_OFFSET);

	value &=
		~(ADMA_CHCTRL_DAI_RX_MULTI_CH_ENABLE |
		ADMA_CHCTRL_RX_MULTI_CH_SEL_Msk);

	value |=
		(mode == TCC_ADMA_MULTI_CH_MODE_3_1) ?
		ADMA_CHCTRL_RX_MULTI_CH_3_1 :
		(mode == TCC_ADMA_MULTI_CH_MODE_5_1_012) ?
		ADMA_CHCTRL_RX_MULTI_CH_5_1_012 :
		(mode == TCC_ADMA_MULTI_CH_MODE_5_1_013) ?
		ADMA_CHCTRL_RX_MULTI_CH_5_1_013 :
		ADMA_CHCTRL_RX_MULTI_CH_7_1;

	if (enable) {
		value |= ADMA_CHCTRL_DAI_RX_MULTI_CH_ENABLE;
	} else {
		value |= ADMA_CHCTRL_DAI_RX_MULTI_CH_DISABLE;
	}

	tcc_sdr_adma_wr(value, base_addr, TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_rx_dma_trigger_type(
	void __iomem *base_addr,
	bool edge)
{
	uint32_t value = tcc_sdr_adma_rd(base_addr, TCC_ADMA_TRANSCTRL_OFFSET);

	value &= ~ADMA_TRANCTRL_DAIRX_TRIG_Msk;

	if (edge) {
		value |= ADMA_TRANCTRL_DAIRX_TRIG_EDGE;
	} else {
		value |= ADMA_TRANCTRL_DAIRX_TRIG_LEVEL;
	}

	tcc_sdr_adma_wr(value, base_addr, TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_rx_dma_repeat_type(
	void __iomem *base_addr,
	enum TCC_SDR_ADMA_REPEAT_MODE mode)
{
	uint32_t value = tcc_sdr_adma_rd(base_addr, TCC_ADMA_TRANSCTRL_OFFSET);

	value &= ~ADMA_TRANCTRL_RXCONTI_Msk;
	if (mode == TCC_ADMA_REPEAT_FROM_CUR_ADDR) {
		value |= ADMA_TRANCTRL_RXCONTIS_CURADDR;
	} else {
		value |= ADMA_TRANCTRL_RXCONTI_STARTADDR;
	}

	tcc_sdr_adma_wr(value, base_addr, TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_dai_threshold(
	void __iomem *base_addr,
	uint32_t dai_buf_threshold)
{
	uint32_t value;

	value = tcc_sdr_adma_rd(base_addr, TCC_ADMA_RPTCTRL_OFFSET);
	value &= ~ADMA_RPTCTRL_DAI_BUFTHRES_Msk;
	value |= ((dai_buf_threshold) << ADMA_RPTCTRL_DAI_BUFTHRES_Pos) &
			ADMA_RPTCTRL_DAI_BUFTHRES_Msk;

	tcc_sdr_adma_wr(value, base_addr, TCC_ADMA_RPTCTRL_OFFSET);
}

static inline void tcc_adma_repeat_infinite_mode(
	void __iomem *base_addr)
{
	uint32_t value;

	value = tcc_sdr_adma_rd(base_addr, TCC_ADMA_RPTCTRL_OFFSET);
	value |= (ADMA_RPTCTRL_IRQ_REQUEST_DMA | ADMA_RPTCTRL_RPTCNT_INFINITE);

	tcc_sdr_adma_wr(value, base_addr, TCC_ADMA_RPTCTRL_OFFSET);
}

static inline int tcc_adma_set_dai_rx_dma_buffer(
	const struct tcc_sdr_dma_t  *tcc_sdr_dma,
	enum TCC_SDR_ADMA_DATA_WIDTH data_width,
	uint32_t bsize,
#if 0
	bool mono_mode,
#endif
	bool adrcnt_mode)
{
	uint32_t wsize = (uint32_t)TCC_ADMA_WORD_SIZE_32;
	uint32_t wbytes = ui_lshift(1u, wsize);
	uint32_t dma_buffer = 0;
	uint32_t rxdaparam = 0;
	uint32_t rxdatcnt = 0;
	uint32_t rxdaadrcnt;
#if 0
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	uint32_t addr_lsb, addr_msb;
	uint32_t addr_lsb_mono, addr_msb_mono;
#endif
#endif
	int ret = 0;

	tcc_adma_dai_rx_dma_enable(tcc_sdr_dma->base_addr, FALSE);

	if (tcc_sdr_dma->buffer_bytes <= 0u) {
		(void) pr_err("buffer_bytes is zero!\n");
		ret = -EINVAL;
	} else if (wbytes == 0u) {
		ret = -EINVAL;
	} else {
		if (adrcnt_mode) {
			uint32_t adrcnt =
				tcc_sdr_dma->buffer_bytes / wbytes;

			if ((adrcnt % 8u) != 0u) {
				(void) pr_info("[WARN][DAI][%s][%d] Warning!!",
					__func__, __LINE__);
				(void) pr_info("adrcnt value should be 8n.\n");
			}

#if 0
			if (mono_mode == TRUE) {
				if((UINT_MAX / 2u) >= adrcnt){
					adrcnt = adrcnt * 2u;
				} else {
					adrcnt = UINT_MAX / 2u;
				}
			}
#endif
			if (adrcnt >= 1u) {
					adrcnt -= 1u;
			}
			rxdaadrcnt = (((uint32_t) adrcnt) << ADMA_ADRCNT_ADDR_COUNT_Pos) &
						ADMA_ADRCNT_ADDR_COUNT_Msk;
			rxdaadrcnt |= ADMA_ADRCNT_MODE_ADRCNT;
		} else {
			uint32_t value = 0u;

			value = ((0x7fffffffU) << ADMA_ADRCNT_ADDR_COUNT_Pos) &
						ADMA_ADRCNT_ADDR_COUNT_Msk;
			rxdaadrcnt = ADMA_ADRCNT_MODE_SMASK | value;

			value = ~((uint32_t) (tcc_sdr_dma->buffer_bytes - 1u)>>4);
			dma_buffer = ((value) << ADMA_PARAM_ADDR_MASK_Pos) &
						ADMA_PARAM_ADDR_MASK_Msk;
		}
		rxdaparam = dma_buffer | wbytes;
		if(wsize <= (UINT_MAX - bsize)){
			rxdatcnt = tcc_sdr_dma->period_bytes >> (wsize + bsize);
		}
		if (rxdatcnt >= 1u) {
			rxdatcnt -= 1u;
		}

		tcc_sdr_adma_wr(rxdaadrcnt, tcc_sdr_dma->base_addr, TCC_ADMA_RXDAADRCNT_OFFSET);
		tcc_sdr_adma_wr(rxdaparam, tcc_sdr_dma->base_addr, TCC_ADMA_RXDAPARAM_OFFSET);
		tcc_sdr_adma_wr(rxdatcnt, tcc_sdr_dma->base_addr, TCC_ADMA_RXDATCNT_OFFSET);
#if 0
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
		addr_lsb = ul_to_ui(tcc_sdr_dma->dma_addr & (uint32_t)0xFFFFFFFFU);
		addr_msb = ul_to_ui(tcc_sdr_dma->dma_addr >> 32) & (uint32_t)0xFFFFFFFFU;

		addr_lsb_mono = ul_to_ui(tcc_sdr_dma->mono_dma_addr & (uint32_t)0xFFFFFFFFU);
		addr_msb_mono = ul_to_ui(tcc_sdr_dma->mono_dma_addr >> 32) & (uint32_t)0xFFFFFFFFU;

		if (mono_mode) {
			tcc_sdr_adma_wr(addr_lsb_mono, tcc_sdr_dma->base_addr, TCC_ADMA_RXDADAR_OFFSET);
			//to do : write msb mono
			tcc_sdr_adma_wr(addr_lsb, tcc_sdr_dma->base_addr, TCC_ADMA_RXDADARL_OFFSET);
			//to do : write msb
		} else {
			tcc_sdr_adma_wr(addr_lsb, tcc_sdr_dma->base_addr, TCC_ADMA_RXDADAR_OFFSET);
			//to do : write msb
			tcc_sdr_adma_wr(0, tcc_sdr_dma->base_addr, TCC_ADMA_RXDADARL_OFFSET);
			//to do : reset msb mono
		}
#else
		if (mono_mode) {
			tcc_sdr_adma_wr(tcc_sdr_dma->mono_dma_addr, tcc_sdr_dma->base_addr, TCC_ADMA_RXDADAR_OFFSET);
			tcc_sdr_adma_wr(tcc_sdr_dma->dma_addr, tcc_sdr_dma->base_addr, TCC_ADMA_RXDADARL_OFFSET);
		} else {
			tcc_sdr_adma_wr(tcc_sdr_dma->dma_addr, tcc_sdr_dma->base_addr, TCC_ADMA_RXDADAR_OFFSET);
			tcc_sdr_adma_wr(0, tcc_sdr_dma->base_addr, TCC_ADMA_RXDADARL_OFFSET);
		}
#endif
#endif
		tcc_adma_set_dai_rx_lrmode(tcc_sdr_dma->base_addr, FALSE);
		tcc_adma_set_dai_rx_transfer_size(tcc_sdr_dma->base_addr, wsize, bsize);
		tcc_adma_set_dai_rx_dma_repeat_enable(tcc_sdr_dma->base_addr, TRUE);
		tcc_adma_set_dai_rx_dma_width(tcc_sdr_dma->base_addr, data_width);
	}

	return ret;
}

static inline int tcc_adma_dai_rx_set_dma_addr(
	void __iomem *base_addr,
	dma_addr_t dma_addr,
	bool iq_mode,
	uint32_t index)
{
	int ret = 0;
	if (dma_addr != 0u) {
		uint32_t addr_lsb;
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
		addr_lsb = ui64_to_ui(dma_addr & (uint32_t)0xFFFFFFFFU);
#else
		addr_lsb = dma_addr;
#endif
		if (iq_mode) {
			if (index == 0u) {
				tcc_sdr_adma_wr(addr_lsb,
						base_addr, TCC_ADMA_RXDADAR_OFFSET);
				tcc_sdr_adma_wr(0, base_addr, TCC_ADMA_RXDADARL_OFFSET);
			} else if (index == 1u) {
				tcc_sdr_adma_wr(addr_lsb,
						base_addr, TCC_ADMA_RXDADAR1_OFFSET);
				tcc_sdr_adma_wr(0, base_addr, TCC_ADMA_RXDADARL1_OFFSET);
			} else if (index == 2u) {
				tcc_sdr_adma_wr(addr_lsb,
						base_addr, TCC_ADMA_RXDADAR2_OFFSET);
				tcc_sdr_adma_wr(0, base_addr, TCC_ADMA_RXDADARL2_OFFSET);
			} else if (index == 3u) {
				tcc_sdr_adma_wr(addr_lsb,
						base_addr, TCC_ADMA_RXDADAR3_OFFSET);
				tcc_sdr_adma_wr(0, base_addr, TCC_ADMA_RXDADARL3_OFFSET);
			} else {
				ret = -EINVAL;
			}
		} else {
			tcc_sdr_adma_wr(addr_lsb,
					base_addr, TCC_ADMA_RXDADAR_OFFSET);
			tcc_sdr_adma_wr(0, base_addr, TCC_ADMA_RXDADARL_OFFSET);
		}
	} else {
		ret = -EINVAL;
	}

	return ret;
}

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
static inline uint64_t tcc_adma_dai_rx_get_cur_dma_addr(const void __iomem *base_addr)
{
	uint32_t addr_lsb, addr_msb;
	addr_lsb = tcc_sdr_adma_rd(base_addr, TCC_ADMA_RXDACDAR_OFFSET);
	addr_msb = 0x0u; //tcc_sdr_adma_rd(base_addr, TCC_XXXXXXXXXXXXX_OFFSET); //to do

	return (((((uint64_t)addr_msb) & 0xFFFFFFFFFFFFFFFFULL) << 32u)
		+ (((uint64_t)addr_lsb) & 0xFFFFFFFFFFFFFFFFULL));
}

static inline uint64_t tcc_adma_dai_multiport_rx_get_cur_dma_addr(
	const void __iomem *base_addr,
	uint32_t port)
{
	uint32_t addr_lsb, addr_msb;

	addr_lsb =
		(port == 0u) ? tcc_sdr_adma_rd(base_addr, TCC_ADMA_RXDACDAR_OFFSET) :
		(port == 1u) ? tcc_sdr_adma_rd(base_addr, TCC_ADMA_RXDACAR1_OFFSET) :
		(port == 2u) ? tcc_sdr_adma_rd(base_addr, TCC_ADMA_RXDACAR2_OFFSET) :
		tcc_sdr_adma_rd(base_addr, TCC_ADMA_RXDACAR3_OFFSET);
	addr_msb = 0x0u; //tcc_sdr_adma_rd(base_addr, TCC_XXXXXXXXXXXXX_OFFSET); //to do

	return (((((uint64_t)addr_msb) & 0xFFFFFFFFFFFFFFFFULL) << 32u)
		+ (((uint64_t)addr_lsb) & 0xFFFFFFFFFFFFFFFFULL));
}

static inline uint64_t tcc_adma_dai_rx_get_cur_mono_dma_addr(const void __iomem *base_addr)
{
	uint32_t addr_lsb, addr_msb;
	addr_lsb = tcc_sdr_adma_rd(base_addr, TCC_ADMA_RXDACDARL_OFFSET);
	addr_msb = 0x0u; //tcc_sdr_adma_rd(base_addr, TCC_XXXXXXXXXXXXX_OFFSET);  //to do
	return (((((uint64_t)addr_msb) & 0xFFFFFFFFFFFFFFFFULL) << 32u)
		+ (((uint64_t)addr_lsb) & 0xFFFFFFFFFFFFFFFFULL));
}

#else
static inline uint32_t tcc_adma_dai_rx_get_cur_dma_addr(const void __iomem *base_addr)
{
	return tcc_sdr_adma_rd(base_addr, TCC_ADMA_RXDACDAR_OFFSET);
}

static inline uint32_t tcc_adma_dai_multiport_rx_get_cur_dma_addr(
	const void __iomem *base_addr,
	uint32_t port)
{
	uint32_t cur_dma = 0u;

	cur_dma =
		(port == 0u) ? tcc_sdr_adma_rd(base_addr, TCC_ADMA_RXDACDAR_OFFSET) :
		(port == 1u) ? tcc_sdr_adma_rd(base_addr, TCC_ADMA_RXDACAR1_OFFSET) :
		(port == 2u) ? tcc_sdr_adma_rd(base_addr, TCC_ADMA_RXDACAR2_OFFSET) :
		tcc_sdr_adma_rd(base_addr, TCC_ADMA_RXDACAR3_OFFSET);
	return cur_dma;
}

static inline uint32_t tcc_adma_dai_rx_get_cur_mono_dma_addr(const void __iomem *base_addr)
{
	return tcc_sdr_adma_rd(base_addr, TCC_ADMA_RXDACDARL_OFFSET);
}
#endif

static inline void tcc_adma_dai_rx_hopcnt_clear(void __iomem *base_addr)
{
	uint32_t value = tcc_sdr_adma_rd(base_addr, TCC_ADMA_TRANSCTRL_OFFSET);
	uint32_t addr = tcc_sdr_adma_rd(base_addr, TCC_ADMA_RXDADAR_OFFSET);

	value &= ~ADMA_TRANSCTRL_HOP_COUNT_MODE_Msk;

	tcc_sdr_adma_wr(
		value | ADMA_TRANSCTRL_HOP_COUNT_CLEAR_MODE,
		base_addr, TCC_ADMA_TRANSCTRL_OFFSET);
	tcc_sdr_adma_wr(addr, base_addr, TCC_ADMA_RXDADAR_OFFSET);
	while ((tcc_sdr_adma_rd(base_addr, TCC_ADMA_RXDATCNT_OFFSET) &
		ADMA_COUNTER_CUR_COUNT_Msk) != 0u) {
	};
	tcc_sdr_adma_wr(
		value | ADMA_TRANSCTRL_HOP_COUNT_INCR_MODE,
		base_addr, TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_sdr_u_delay(uint32_t us)
{
	if ((us > 0u) && (us <= (si_to_ui(MAX_UDELAY_MS) * 1000u))) {
		udelay(us);
	} else {
		(void) pr_err("[%s] invalid input\n", __func__);
	}
}

static inline int tcc_adma_dai_rx_auto_dma_disable(void __iomem *base_addr)
{
	uint32_t transctrl = 0, tcnt = 0, chctrl = 0;
	uint32_t timeout = 0;
	int32_t ret = 0;

	// ADMA RX Repeat mode off
	transctrl = tcc_sdr_adma_rd(base_addr, TCC_ADMA_TRANSCTRL_OFFSET);
	transctrl &= ~ADMA_TRANCTRL_DAIRX_REPEAT_Msk;
	transctrl |= ADMA_TRANCTRL_DAIRX_REPEAT_DIS;
	tcc_sdr_adma_wr(
		transctrl,
		base_addr, TCC_ADMA_TRANSCTRL_OFFSET);

	// Wait until CurTCNT zero and ADMA disable
	while (timeout < 300u) {
		tcnt = tcc_sdr_adma_rd(base_addr, TCC_ADMA_RXDATCNT_OFFSET)
			& ADMA_COUNTER_CUR_COUNT_Msk;
		chctrl = tcc_sdr_adma_rd(base_addr, TCC_ADMA_CHCTRL_OFFSET)
			& ADMA_CHCTRL_DAI_RX_DMA_MODE_Msk;

		if ((tcnt == 0u) && (chctrl == 0u)) {
			break;
		}

		tcc_sdr_u_delay(1000u);
		timeout++;
	};

	transctrl = tcc_sdr_adma_rd(base_addr, TCC_ADMA_TRANSCTRL_OFFSET);
	tcnt = tcc_sdr_adma_rd(base_addr, TCC_ADMA_RXDATCNT_OFFSET);
	chctrl = tcc_sdr_adma_rd(base_addr, TCC_ADMA_CHCTRL_OFFSET);
	if ((timeout >= 300u) ||
		((chctrl & ADMA_CHCTRL_DAI_RX_DMA_MODE_Msk) != 0u)) {
		(void) pr_info("timeout: %d, tcnt: 0x%08x, chctrl: 0x%08x, trans: 0x%08x\n",
				timeout, tcnt, chctrl, transctrl);
		ret = -ETIME;
	}

	return ret;
}
#endif /*TCC_SDR_ADMA_H*/

