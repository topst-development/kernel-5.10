/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_ADMA_H
#define TCC_ADMA_H

#include <linux/io.h>
#include "tcc_audio_hw.h"
#include "tcc_audio_rule.h"


enum TCC_ADMA_DATA_WIDTH {
	TCC_ADMA_DATA_WIDTH_16 = 0,
	TCC_ADMA_DATA_WIDTH_24 = 1,
	TCC_ADMA_DATA_WIDTH_32 = 2,
};

enum TCC_ADMA_WORD_SIZE { //2^n * 8
	TCC_ADMA_WORD_SIZE_8 = 0,
	TCC_ADMA_WORD_SIZE_16 = 1,
	TCC_ADMA_WORD_SIZE_32 = 2,
};

//TCC_ADMA_BURST_SIZE 2^n
#define TCC_ADMA_BURST_CYCLE_1 	0U
#define TCC_ADMA_BURST_CYCLE_2	1U
#define TCC_ADMA_BURST_CYCLE_4	2U
#define TCC_ADMA_BURST_CYCLE_8	3U

enum TCC_ADMA_MULTI_CH_MODE {
	TCC_ADMA_MULTI_CH_MODE_3_1 = 0,
	TCC_ADMA_MULTI_CH_MODE_5_1_012 = 1,
	TCC_ADMA_MULTI_CH_MODE_5_1_013 = 2,
	TCC_ADMA_MULTI_CH_MODE_7_1	= 3,
};

enum TCC_ADMA_REPEAT_MODE {
	TCC_ADMA_REPEAT_FROM_CUR_ADDR = 0,
	TCC_ADMA_REPEAT_FROM_START_ADDR = 1,
};

enum TCC_ADMA_SPDIF_CDIF_RX_SEL {
	TCC_ADMA_SPDIF_CDIF_SEL_CDIF = 0,
	TCC_ADMA_SPDIF_CDIF_SEL_SPDIF = 1,
};

#define unused(x) (void)(x)

#if 0 //DEBUG
#define adma_writel(v, c) \
	({pr_info("<ASoC> ADMA_REG(%p) = 0x%08x\n",\
	c, (unsigned int)v); writel(v, c); })
#else
#define adma_writel(v, c) \
	writel(v, c)
#endif

struct tcc_audio_dma_t{
	void __iomem *base_addr;
	dma_addr_t dma_addr;
	dma_addr_t mono_dma_addr;
	uint32_t buffer_bytes;
	uint32_t period_bytes;
};

struct adma_reg_t{
	unsigned int txdaddrcnt;
	unsigned int txdaparam;
	unsigned int txdatcnt;
	unsigned int txdasar;
	unsigned int txdasarl;
	unsigned int rxdaddrcnt;
	unsigned int rxdaparam;
	unsigned int rxdatcnt;
	unsigned int rxdadar;
	unsigned int rxdadarl;
	unsigned int txspadrcnt;
	unsigned int txsparam;
	unsigned int txsptcnt;
	unsigned int txspsar;
	unsigned int txspsarl;
	unsigned int rxspadrcnt;
	unsigned int rxspparam;
	unsigned int rxsptcnt;
	unsigned int rxspdar;
	unsigned int rxspdarl;
	unsigned int transctrl;
	unsigned int rptctrl;
	unsigned int chctrl;
};

static inline void tcc_adma_dump(const void __iomem *base_addr)
{
	ptrdiff_t offset;
	uint32_t value;

	for (offset = 0; offset <= TCC_ADMA_RESET_OFFSET; offset += 4) {
		value = readl(base_addr+offset);
		(void) pr_info("ADMA_REG(0x%03x) : 0x%08x\n",
			(uint32_t)offset,
			value);
	}
}

static inline void tcc_adma_tx_reset_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_RESET_OFFSET);

	value &= ~ADMA_RESET_DMA_TX_Msk;
	if (enable) {
		value |= ADMA_RESET_DMA_TX_RESET;
	} else {
		value |= ADMA_RESET_DMA_TX_RELEASE;
	}
	adma_writel(value, base_addr + TCC_ADMA_RESET_OFFSET);
}

static inline void tcc_adma_rx_reset_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_RESET_OFFSET);

	value &= ~ADMA_RESET_DMA_RX_Msk;
	if (enable) {
		value |= ADMA_RESET_DMA_RX_RESET;
	} else {
		value |= ADMA_RESET_DMA_RX_RELEASE;
	}
	adma_writel(value, base_addr + TCC_ADMA_RESET_OFFSET);
}

static inline void tcc_adma_dai_tx_reset_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_RESET_OFFSET);

	value &= ~ADMA_RESET_DMA_DAI_TX_Msk;
	if (enable) {
		value |= ADMA_RESET_DMA_DAI_TX_RESET;
	} else {
		value |= ADMA_RESET_DMA_DAI_TX_RELEASE;
	}
	adma_writel(value, base_addr + TCC_ADMA_RESET_OFFSET);
}
static inline void tcc_adma_dai_rx_reset_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_RESET_OFFSET);

	value &= ~ADMA_RESET_DMA_DAI_RX_Msk;
	if (enable) {
		value |= ADMA_RESET_DMA_DAI_RX_RESET;
	} else {
		value |= ADMA_RESET_DMA_DAI_RX_RELEASE;
	}
	adma_writel(value, base_addr + TCC_ADMA_RESET_OFFSET);
}

static inline void tcc_adma_spdif_tx_reset_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_RESET_OFFSET);

	value &= ~ADMA_RESET_DMA_SPDIF_TX_Msk;
	if (enable) {
		value |= ADMA_RESET_DMA_SPDIF_TX_RESET;
	} else {
		value |= ADMA_RESET_DMA_SPDIF_TX_RELEASE;
	}
	adma_writel(value, base_addr + TCC_ADMA_RESET_OFFSET);
}
static inline void tcc_adma_spdif_rx_reset_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_RESET_OFFSET);

	value &= ~ADMA_RESET_DMA_SPDIF_RX_Msk;
	if (enable) {
		value |= ADMA_RESET_DMA_SPDIF_RX_RESET;
	} else {
		value |= ADMA_RESET_DMA_SPDIF_RX_RELEASE;
	}
	adma_writel(value, base_addr + TCC_ADMA_RESET_OFFSET);
}

static inline void tcc_adma_dai_tx_irq_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_DAI_TX_IRQ_MODE_Msk;
	if (enable) {
		value |= ADMA_CHCTRL_DAI_TX_IRQ_ENABLE;
	} else {
		value |= ADMA_CHCTRL_DAI_TX_IRQ_DISABLE;
	}
	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_dai_tx_dma_enable(
	void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_DAI_TX_DMA_MODE_Msk;
	if (enable) {
		value |= ADMA_CHCTRL_DAI_TX_DMA_ENABLE;
	} else {
		value |= ADMA_CHCTRL_DAI_TX_DMA_DISABLE;
	}
	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline bool tcc_adma_dai_tx_dma_enable_check(
	const void __iomem *base_addr)
{
	uint32_t int_status = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);
	bool ret = FALSE;

	if ((int_status & (ADMA_CHCTRL_DAI_TX_DMA_MODE_Msk)) > (uint32_t) 0) {
		ret = TRUE;
	}

	return ret;
}

static inline bool tcc_adma_dai_tx_irq_check(
	const void __iomem *base_addr)
{
	uint32_t int_status = readl(base_addr + TCC_ADMA_INTSTATUS_OFFSET);
	bool ret = FALSE;

	if ((int_status & (ADMA_ISTAT_DAI_TX_MASKED_Msk |
		ADMA_ISTAT_DAI_TX_Msk)) > 0u) {
		ret = TRUE;
	}

	return ret;
}

static inline void tcc_adma_dai_tx_irq_clear(
	void __iomem *base_addr)
{
	adma_writel(
		ADMA_ISTAT_DAI_TX_MASKED_Msk|ADMA_ISTAT_DAI_TX_Msk,
		base_addr + TCC_ADMA_INTSTATUS_OFFSET);
}

static inline void tcc_adma_dai_rx_irq_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_DAI_RX_IRQ_MODE_Msk;
	if (enable) {
		value |= ADMA_CHCTRL_DAI_RX_IRQ_ENABLE;
	} else {
		value |= ADMA_CHCTRL_DAI_RX_IRQ_DISABLE;
	}
	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_dai_rx_dma_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_DAI_RX_DMA_MODE_Msk;
	if (enable) {
		value |= ADMA_CHCTRL_DAI_RX_DMA_ENABLE;
	} else {
		value |= ADMA_CHCTRL_DAI_RX_DMA_DISABLE;
	}
	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline bool tcc_adma_dai_rx_dma_enable_check(
	const void __iomem *base_addr)
{
	uint32_t int_status = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);
	bool ret = FALSE;

	if ((int_status & (ADMA_CHCTRL_DAI_RX_DMA_MODE_Msk)) > (uint32_t) 0) {
		ret = TRUE;
	}

	return ret;
}

static inline bool tcc_adma_dai_rx_irq_check(
	const void __iomem *base_addr)
{
	uint32_t int_status = readl(base_addr + TCC_ADMA_INTSTATUS_OFFSET);
	bool ret = FALSE;

	if ((int_status & (ADMA_ISTAT_DAI_RX_MASKED_Msk |
			ADMA_ISTAT_DAI_RX_Msk)) > (uint32_t) 0) {
		ret = TRUE;
	}

	return ret;
}

static inline void tcc_adma_dai_rx_irq_clear(
	void __iomem *base_addr)
{
	adma_writel(
		ADMA_ISTAT_DAI_RX_MASKED_Msk|ADMA_ISTAT_DAI_RX_Msk,
		base_addr + TCC_ADMA_INTSTATUS_OFFSET);
}

static inline void tcc_adma_spdif_tx_irq_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_SPDIF_TX_IRQ_MODE_Msk;
	if (enable) {
		value |= ADMA_CHCTRL_SPDIF_TX_IRQ_ENABLE;
	} else {
		value |= ADMA_CHCTRL_SPDIF_TX_IRQ_DISABLE;
	}
	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_spdif_tx_dma_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~(ADMA_CHCTRL_SPDIF_TX_DMA_MODE_Msk);
	if (enable) {
		value |= ADMA_CHCTRL_SPDIF_TX_DMA_ENABLE;
	} else {
		value |= ADMA_CHCTRL_SPDIF_TX_DMA_DISABLE;
	}
	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline bool tcc_adma_spdif_tx_dma_enable_check(
	const void __iomem *base_addr)
{
	uint32_t int_status = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);
	bool ret = FALSE;

	if ((int_status & (ADMA_CHCTRL_SPDIF_TX_DMA_MODE_Msk)) > 0u) {
		ret = TRUE;
	}

	return ret;
}

static inline bool tcc_adma_spdif_tx_irq_check(
	const void __iomem *base_addr)
{
	uint32_t int_status = readl(base_addr + TCC_ADMA_INTSTATUS_OFFSET);
	bool ret = FALSE;

	if ((int_status &
		(ADMA_ISTAT_SPDIF_TX_MASKED_Msk | ADMA_ISTAT_SPDIF_TX_Msk))
		> (uint32_t) 0) {
		ret = TRUE;
	}

	return ret;
}

static inline void tcc_adma_spdif_tx_irq_clear(
	void __iomem *base_addr)
{
	adma_writel(
		ADMA_ISTAT_SPDIF_TX_MASKED_Msk|ADMA_ISTAT_SPDIF_TX_Msk,
		base_addr + TCC_ADMA_INTSTATUS_OFFSET);
}

static inline void tcc_adma_spdif_rx_irq_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_CDIF_RX_IRQ_MODE_Msk;

	if (enable) {
		value |= ADMA_CHCTRL_CDIF_RX_IRQ_ENABLE;
	} else {
		value |= ADMA_CHCTRL_CDIF_RX_IRQ_DISABLE;
	}

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_spdif_rx_dma_en(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_CDIF_RX_DMA_MODE_Msk;

	if (enable) {
		value |= ADMA_CHCTRL_CDIF_RX_DMA_ENABLE;
	} else {
		value |= ADMA_CHCTRL_CDIF_RX_DMA_DISABLE;
	}

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline bool tcc_adma_spdif_rx_dma_en_check(
	const void __iomem *base_addr)
{
	uint32_t int_status = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);
	bool ret = FALSE;

	if ((int_status & (ADMA_CHCTRL_CDIF_RX_DMA_MODE_Msk)) > 0u) {
		ret =  TRUE;
	}

	return ret;
}

static inline bool tcc_adma_spdif_rx_irq_check(
	const void __iomem *base_addr)
{
	uint32_t int_status = readl(base_addr + TCC_ADMA_INTSTATUS_OFFSET);
	bool ret = FALSE;

	if ((int_status &
		(ADMA_ISTAT_CDIF_RX_MASKED_Msk | ADMA_ISTAT_CDIF_RX_Msk))
		> (uint32_t) 0) {
		ret = TRUE;
	}

	return ret;
}

static inline void tcc_adma_spdif_rx_irq_clear(
	void __iomem *base_addr)
{
	adma_writel(
		ADMA_ISTAT_CDIF_RX_MASKED_Msk|ADMA_ISTAT_CDIF_RX_Msk,
		base_addr + TCC_ADMA_INTSTATUS_OFFSET);
}


static inline void tcc_adma_set_dai_tx_lrmode(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_DAI_TX_LR_MODE_Msk;

	if (enable) {
		value |= ADMA_CHCTRL_DAI_TX_LR_ENABLE;
	} else {
		value |= ADMA_CHCTRL_DAI_TX_LR_DISABLE;
	}

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_tx_dma_width(
	void __iomem *base_addr,
	enum TCC_ADMA_DATA_WIDTH width)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_DAI_TXD_WIDTH_MODE_Msk;
	value |=
		((width == TCC_ADMA_DATA_WIDTH_24) || (width == TCC_ADMA_DATA_WIDTH_32)) ?
		ADMA_CHCTRL_DAI_TXD_WIDTH_24BIT :
		ADMA_CHCTRL_DAI_TXD_WIDTH_16BIT;

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);

	if(width == TCC_ADMA_DATA_WIDTH_32) {
#if defined(CONFIG_ARCH_TCC807X)
		//TBD setting for 32bit
#endif
	}
}

static inline void tcc_adma_set_dai_tx_transfer_size(
	void __iomem *base_addr,
	uint32_t wsize,
	uint32_t bsize)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	value &=
		~(ADMA_TRANCTRL_DAITX_WSIZE_Msk |
		ADMA_TRANCTRL_DAITX_BSIZE_Msk);

	value |=
		(wsize == (uint32_t)TCC_ADMA_WORD_SIZE_8) ?
		ADMA_TRANCTRL_DAITX_WSIZE_8 :
		(wsize == (uint32_t)TCC_ADMA_WORD_SIZE_16) ?
		ADMA_TRANCTRL_DAITX_WSIZE_16 :
		ADMA_TRANCTRL_DAITX_WSIZE_32;

	value |=
		(bsize == (uint32_t)TCC_ADMA_BURST_CYCLE_1) ?
		ADMA_TRANCTRL_DAITX_BCYCLE_1 :
		(bsize == (uint32_t)TCC_ADMA_BURST_CYCLE_2) ?
		ADMA_TRANCTRL_DAITX_BCYCLE_2 :
		(bsize == (uint32_t)TCC_ADMA_BURST_CYCLE_4) ?
		ADMA_TRANCTRL_DAITX_BCYCLE_4 :
		ADMA_TRANCTRL_DAITX_BCYCLE_8;

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_tx_dma_repeat_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	if (enable) {
		value |= ADMA_TRANCTRL_DAITX_REPEAT_ENABLE;
	} else {
		value &= ~ADMA_TRANCTRL_DAITX_REPEAT_ENABLE;
	}

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_rx_lrmode(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_DAI_RX_LR_MODE_Msk;

	if (enable) {
		value |= ADMA_CHCTRL_DAI_RX_LR_ENABLE;
	} else {
		value |= ADMA_CHCTRL_DAI_RX_LR_DISABLE;
	}

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_rx_dma_width(
	void __iomem *base_addr,
	enum TCC_ADMA_DATA_WIDTH width)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_DAI_RXD_WIDTH_Msk;
	value |=
		((width == TCC_ADMA_DATA_WIDTH_24) || (width == TCC_ADMA_DATA_WIDTH_32)) ?
		ADMA_CHCTRL_DAI_RXD_WIDTH_24BIT :
		ADMA_CHCTRL_DAI_RXD_WIDTH_16BIT;

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
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
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

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
		(bsize == (uint32_t)TCC_ADMA_BURST_CYCLE_1) ?
		ADMA_TRANCTRL_DAIRX_BCYCLE_1 :
		(bsize == (uint32_t)TCC_ADMA_BURST_CYCLE_2) ?
		ADMA_TRANCTRL_DAIRX_BCYCLE_2 :
		(bsize == (uint32_t)TCC_ADMA_BURST_CYCLE_4) ?
		ADMA_TRANCTRL_DAIRX_BCYCLE_4 :
		ADMA_TRANCTRL_DAIRX_BCYCLE_8;

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_rx_dma_repeat_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	if (enable) {
		value |= ADMA_TRANCTRL_DAIRX_REPEAT_EN;
	} else {
		value &= ~ADMA_TRANCTRL_DAIRX_REPEAT_EN;
	}

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_spdif_tx_dma_width(
	void __iomem *base_addr,
	enum TCC_ADMA_DATA_WIDTH width)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_SPDIF_TXD_WIDTH_MODE_Msk;
	value |=
		(width == TCC_ADMA_DATA_WIDTH_24) ?
		ADMA_CHCTRL_SPDIF_TXD_WIDTH_24BIT :
		ADMA_CHCTRL_SPDIF_TXD_WIDTH_16BIT;

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_set_spdif_tx_transfer_size(
	void __iomem *base_addr,
	uint32_t wsize,
	uint32_t bsize)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	value &=
		~(ADMA_TRANCTRL_SPDIFTX_WSIZE_Msk |
		ADMA_TRANCTRL_SPDIFTX_BSIZE_Msk);

	value |=
		(wsize == (uint32_t)TCC_ADMA_WORD_SIZE_8) ?
		ADMA_TRANCTRL_SPDIFTX_WSIZE_8 :
		(wsize == (uint32_t)TCC_ADMA_WORD_SIZE_16) ?
		ADMA_TRANCTRL_SPDIFTX_WSIZE_16 :
		ADMA_TRANCTRL_SPDIFTX_WSIZE_32;

	value |=
		(bsize == (uint32_t)TCC_ADMA_BURST_CYCLE_1) ?
		ADMA_TRANCTRL_SPDIFTX_BCYCLE_1 :
		(bsize == (uint32_t)TCC_ADMA_BURST_CYCLE_2) ?
		ADMA_TRANCTRL_SPDIFTX_BCYCLE_2 :
		(bsize == (uint32_t)TCC_ADMA_BURST_CYCLE_4) ?
		ADMA_TRANCTRL_SPDIFTX_BCYCLE_4 :
		ADMA_TRANCTRL_SPDIFTX_BCYCLE_8;

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_spdif_tx_dma_repeat_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	if (enable) {
		value |= ADMA_TRANCTRL_SPDIFTXREPEAT_EN;
	} else {
		value &= ~ADMA_TRANCTRL_SPDIFTXREPEAT_EN;
	}

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_spdif_rx_path(
	void __iomem *base_addr,
	enum TCC_ADMA_SPDIF_CDIF_RX_SEL sel)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_CDIF_SPDIF_SEL_Msk;
	value |=
		(sel == TCC_ADMA_SPDIF_CDIF_SEL_CDIF) ?
		ADMA_CHCTRL_CDIF_SPDIF_SEL_CDIF :
		ADMA_CHCTRL_CDIF_SPDIF_SEL_SPDIF;

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}


static inline void tcc_adma_set_spdif_rx_width(
	void __iomem *base_addr,
	enum TCC_ADMA_DATA_WIDTH width)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~ADMA_CHCTRL_CDIF_RXD_WIDTH_MODE_Msk;
	value |=
		(width == TCC_ADMA_DATA_WIDTH_24) ?
		ADMA_CHCTRL_CDIF_RXD_WIDTH_24BIT :
		ADMA_CHCTRL_CDIF_RXD_WIDTH_16BIT;

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_set_spdif_rx_transfer_size(
	void __iomem *base_addr,
	uint32_t wsize,
	uint32_t bsize)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	value &=
		~(ADMA_TRANCTRL_CDIFRX_WSIZE_Msk |
		ADMA_TRANCTRL_CDIFRX_BSIZE_Msk);

	value |=
		(wsize == (uint32_t)TCC_ADMA_WORD_SIZE_8) ?
		ADMA_TRANCTRL_CDIF_RX_WSIZE_8 :
		(wsize == (uint32_t)TCC_ADMA_WORD_SIZE_16) ?
		ADMA_TRANCTRL_CDIFRX_WSIZE_16 :
		ADMA_TRANCTRL_CDIFRX_WSIZE_32;

	value |=
		(bsize == (uint32_t)TCC_ADMA_BURST_CYCLE_1) ?
		ADMA_TRANCTRL_CDIFRX_BCYCLE_1 :
		(bsize == (uint32_t)TCC_ADMA_BURST_CYCLE_2) ?
		ADMA_TRANCTRL_CDIFRX_BCYCLE_2 :
		(bsize == (uint32_t)TCC_ADMA_BURST_CYCLE_4) ?
		ADMA_TRANCTRL_CDIFRX_BCYCLE_4 :
		ADMA_TRANCTRL_CDIFRX_BCYCLE_8;

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_spdif_rx_rept_en(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	if (enable) {
		value |= ADMA_TRANCTRL_CDIF_RX_REPEAT_EN;
	} else {
		value &= ~ADMA_TRANCTRL_CDIF_RX_REPEAT_EN;
	}

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}


static inline void tcc_adma_set_dai_tx_multi_ch(
	void __iomem *base_addr,
	bool enable,
	enum TCC_ADMA_MULTI_CH_MODE mode)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

	value &= ~(ADMA_CHCTRL_DAI_TX_MULTI_CH_ENABLE |
		ADMA_CHCTRL_TX_MULTI_CH_SEL_Msk);

	value |=
		(mode == TCC_ADMA_MULTI_CH_MODE_3_1) ?
		ADMA_CHCTRL_TX_MULTI_CH_3_1 :
		(mode == TCC_ADMA_MULTI_CH_MODE_5_1_012) ?
		ADMA_CHCTRL_TX_MULTI_CH_5_1_012 :
		(mode == TCC_ADMA_MULTI_CH_MODE_5_1_013) ?
		ADMA_CHCTRL_TX_MULTI_CH_5_1_013 :
		ADMA_CHCTRL_TX_MULTI_CH_7_1;

	if (enable) {
		value |= ADMA_CHCTRL_DAI_TX_MULTI_CH_ENABLE;
	} else {
		value |= ADMA_CHCTRL_DAI_TX_MULTI_CH_DISABLE;
	}

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_rx_multi_ch(
	void __iomem *base_addr,
	bool enable,
	enum TCC_ADMA_MULTI_CH_MODE mode)
{
	uint32_t value = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);

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

	adma_writel(value, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_tx_dma_trigger_type(
	void __iomem *base_addr,
	bool edge)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	value &= ~ADMA_TRANCTRL_DAITX_TRIG_Msk;

	if (edge) {
		value |= ADMA_TRANCTRL_DAITX_TRIG_EDGE;
	} else {
		value |= ADMA_TRANCTRL_DAITX_TRIG_LEVEL;
	}

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_dai_rx_dma_trigger_type(
	void __iomem *base_addr,
	bool edge)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	value &= ~ADMA_TRANCTRL_DAIRX_TRIG_Msk;

	if (edge) {
		value |= ADMA_TRANCTRL_DAIRX_TRIG_EDGE;
	} else {
		value |= ADMA_TRANCTRL_DAIRX_TRIG_LEVEL;
	}

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_spdif_tx_dma_trigger_type(
	void __iomem *base_addr,
	bool edge)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	value &= ~ADMA_TRANCTRL_SPDIFTX_TRIG_Msk;
	if (edge) {
		value |= ADMA_TRANCTRL_SPDIFTX_TRIG_EDGE;
	} else {
		value |= ADMA_TRANCTRL_SPDIFTX_TRIG_LEV;
	}

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_rx_dma_trigger_type(
	void __iomem *base_addr,
	bool edge)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	value &= ~ADMA_TRANCTRL_CDIFRX_TRIG_Msk;

	if (edge) {
		value |= ADMA_TRANCTRL_CDIFRX_TRIG_EDGE;
	} else {
		value |= ADMA_TRANCTRL_CDIFRX_TRIG_LEVEL;
	}

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_tx_dma_repeat_type(
	void __iomem *base_addr,
	enum TCC_ADMA_REPEAT_MODE mode)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	value &= ~ADMA_TRANCTRL_TX_DMA_CONTI_Msk;
	if (mode == TCC_ADMA_REPEAT_FROM_CUR_ADDR) {
		value |= ADMA_TRANCTRL_TXCONTI_CURADDR;
	} else {
		value |= ADMA_TRANCTRL_TXCONTI_STARTADDR;
	}

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_set_rx_dma_repeat_type(
	void __iomem *base_addr,
	enum TCC_ADMA_REPEAT_MODE mode)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);

	value &= ~ADMA_TRANCTRL_RXCONTI_Msk;
	if (mode == TCC_ADMA_REPEAT_FROM_CUR_ADDR) {
		value |= ADMA_TRANCTRL_RXCONTIS_CURADDR;
	} else {
		value |= ADMA_TRANCTRL_RXCONTI_STARTADDR;
	}

	adma_writel(value, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_dai_threshold(
	void __iomem *base_addr,
	uint32_t dai_buf_threshold)
{
	uint32_t value;

	value = readl(base_addr + TCC_ADMA_RPTCTRL_OFFSET);
	value &= ~ADMA_RPTCTRL_DAI_BUFTHRES_Msk;
	value |= ((dai_buf_threshold) << ADMA_RPTCTRL_DAI_BUFTHRES_Pos) &
			ADMA_RPTCTRL_DAI_BUFTHRES_Msk;

	adma_writel(value, base_addr + TCC_ADMA_RPTCTRL_OFFSET);
}

static inline void tcc_adma_repeat_infinite_mode(
	void __iomem *base_addr)
{
	uint32_t value;

	value = readl(base_addr + TCC_ADMA_RPTCTRL_OFFSET);
	value |= (ADMA_RPTCTRL_IRQ_REQUEST_DMA | ADMA_RPTCTRL_RPTCNT_INFINITE);

	adma_writel(value, base_addr + TCC_ADMA_RPTCTRL_OFFSET);
}

static inline int tcc_adma_set_dai_tx_dma_buffer(
	const struct tcc_audio_dma_t  *tcc_audio_dma,
	enum TCC_ADMA_DATA_WIDTH data_width,
	uint32_t bsize,
	bool mono_mode,
	bool adrcnt_mode)
{
	uint32_t wsize = (uint32_t)TCC_ADMA_WORD_SIZE_32;
	uint32_t wbytes = ui_lshift(1u, wsize);
	uint32_t dma_buffer = 0;
	uint32_t txdaparam = 0;
	uint32_t txdatcnt = 0;
	uint32_t txdaadrcnt;
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	uint32_t addr_lsb, addr_lsb_mono;
	#if 0//for the future
	uint32_t addr_msb, addr_msb_mono;
	#endif
#endif
	int ret = 0;

	tcc_adma_dai_tx_dma_enable(tcc_audio_dma->base_addr, FALSE);

	if ((tcc_audio_dma->buffer_bytes <= 0u) ||
		(tcc_audio_dma->dma_addr > UINT_MAX) ||
		(tcc_audio_dma->mono_dma_addr > UINT_MAX)) {
		(void) pr_err("buffer_bytes is zero!\n");
		ret = -EINVAL;
	} else if (wbytes == 0u) {
		ret = -EINVAL;
	} else {
		if (adrcnt_mode) {
			uint32_t adrcnt =
				(tcc_audio_dma->buffer_bytes / wbytes);

			if ((adrcnt % 8u) != 0u) {
				(void) pr_info("[WARN][DAI][%s][%d] Warning!! adrcnt",
					__func__, __LINE__);
				(void) pr_info("value should be 8n.\n");
			}

			if (mono_mode == TRUE) {
				if((UINT_MAX / 2u) >= adrcnt){
					adrcnt = adrcnt * 2u;
				} else {
					adrcnt = (UINT_MAX / 2u);
				}
			}

			if (adrcnt >= 1u) {
				adrcnt -= 1u;
			}
			txdaadrcnt = (((uint32_t) adrcnt) << ADMA_ADRCNT_ADDR_COUNT_Pos) &
						ADMA_ADRCNT_ADDR_COUNT_Msk;
			txdaadrcnt |= ADMA_ADRCNT_MODE_ADRCNT;
		} else {
			uint32_t value = 0u;

			value = ((0x7fffffffU) << ADMA_ADRCNT_ADDR_COUNT_Pos) &
						ADMA_ADRCNT_ADDR_COUNT_Msk;
			txdaadrcnt = ADMA_ADRCNT_MODE_SMASK | value;

			value = ~((uint32_t) (tcc_audio_dma->buffer_bytes - 1u)>>4);
			dma_buffer = ((value) << ADMA_PARAM_ADDR_MASK_Pos) &
						ADMA_PARAM_ADDR_MASK_Msk;
		}
		txdaparam = dma_buffer | ((uint32_t) 1 << (uint32_t) wsize);
		if(wsize <= (UINT_MAX - bsize)){
			txdatcnt = (uint32_t) tcc_audio_dma->period_bytes >> (uint32_t) (wsize + bsize);
		}
		if(txdatcnt >= 1u){
			txdatcnt -= 1u;
		}


		adma_writel(txdaadrcnt, tcc_audio_dma->base_addr + TCC_ADMA_TXDAADRCNT_OFFSET);
		adma_writel(txdaparam, tcc_audio_dma->base_addr + TCC_ADMA_TXDAPARAM_OFFSET);
		adma_writel(txdatcnt, tcc_audio_dma->base_addr + TCC_ADMA_TXDATCNT_OFFSET);

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
		addr_lsb = ul_to_ui(tcc_audio_dma->dma_addr & (uint32_t)0xFFFFFFFFU);
		#if 0//for the future
		addr_msb = ul_to_ui(tcc_audio_dma->dma_addr >> 32) & (uint32_t)0xFFFFFFFFU;
		#endif

		addr_lsb_mono = ul_to_ui(tcc_audio_dma->mono_dma_addr & (uint32_t)0xFFFFFFFFU);
		#if 0//for the future
		addr_msb_mono = ul_to_ui(tcc_audio_dma->mono_dma_addr >> 32) & (uint32_t)0xFFFFFFFFU;
		#endif

		if (mono_mode) {
			adma_writel(addr_lsb_mono, tcc_audio_dma->base_addr + TCC_ADMA_TXDASAR_OFFSET);
			//to do : write msb mono
			adma_writel(addr_lsb, tcc_audio_dma->base_addr + TCC_ADMA_TXDASARL_OFFSET);
			//to do : write msb
		} else {
			adma_writel(addr_lsb, tcc_audio_dma->base_addr + TCC_ADMA_TXDASAR_OFFSET);
			//to do : write msb mono
			adma_writel(0, tcc_audio_dma->base_addr + TCC_ADMA_TXDASARL_OFFSET);
		}

#else
		if (mono_mode) {
			adma_writel(tcc_audio_dma->mono_dma_addr, tcc_audio_dma->base_addr + TCC_ADMA_TXDASAR_OFFSET);
			adma_writel(tcc_audio_dma->dma_addr, tcc_audio_dma->base_addr + TCC_ADMA_TXDASARL_OFFSET);
		} else {
			adma_writel(tcc_audio_dma->dma_addr, tcc_audio_dma->base_addr + TCC_ADMA_TXDASAR_OFFSET);
			adma_writel(0, tcc_audio_dma->base_addr + TCC_ADMA_TXDASARL_OFFSET);
		}
#endif
		tcc_adma_set_dai_tx_lrmode(tcc_audio_dma->base_addr, mono_mode);
		tcc_adma_set_dai_tx_transfer_size(tcc_audio_dma->base_addr, wsize, bsize);
		tcc_adma_set_dai_tx_dma_repeat_enable(tcc_audio_dma->base_addr, TRUE);
		tcc_adma_set_dai_tx_dma_width(tcc_audio_dma->base_addr, data_width);
	}
	return ret;
}

static inline int tcc_adma_set_dai_rx_dma_buffer(
	const struct tcc_audio_dma_t  *tcc_audio_dma,
	enum TCC_ADMA_DATA_WIDTH data_width,
	uint32_t bsize,
	bool mono_mode,
	bool adrcnt_mode)
{
	uint32_t wsize = (uint32_t)TCC_ADMA_WORD_SIZE_32;
	uint32_t wbytes = ui_lshift(1u, wsize);
	uint32_t dma_buffer = 0;
	uint32_t rxdaparam = 0;
	uint32_t rxdatcnt = 0;
	uint32_t rxdaadrcnt;
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	uint32_t addr_lsb;
	//to do : uint32_t addr_msb;
	uint32_t addr_lsb_mono;
	//to do : uint32_t addr_msb_mono;
#endif
	int ret = 0;

	tcc_adma_dai_rx_dma_enable(tcc_audio_dma->base_addr, FALSE);

	if ((tcc_audio_dma->buffer_bytes <= 0u) ||
		(tcc_audio_dma->dma_addr > UINT_MAX) ||
		(tcc_audio_dma->mono_dma_addr > UINT_MAX)) {
		(void) pr_err("buffer_bytes is zero!\n");
		ret = -EINVAL;
	} else if (wbytes == 0u) {
		ret = -EINVAL;
	} else {
		if (adrcnt_mode) {
			uint32_t adrcnt =
				tcc_audio_dma->buffer_bytes / wbytes;

			if ((adrcnt % 8u) != 0u) {
				(void) pr_info("[WARN][DAI][%s][%d] Warning!!",
					__func__, __LINE__);
				(void) pr_info("adrcnt value should be 8n.\n");
			}

			if (mono_mode == TRUE) {
				if((UINT_MAX / 2u) >= adrcnt){
					adrcnt = adrcnt * 2u;
				} else {
					adrcnt = UINT_MAX / 2u;
				}
			}

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

			value = ~((uint32_t) (tcc_audio_dma->buffer_bytes - 1u)>>4);
			dma_buffer = ((value) << ADMA_PARAM_ADDR_MASK_Pos) &
						ADMA_PARAM_ADDR_MASK_Msk;
		}
		rxdaparam = dma_buffer | wbytes;
		if(wsize <= (UINT_MAX - bsize)){
			rxdatcnt = tcc_audio_dma->period_bytes >> (wsize + bsize);
		}
		if (rxdatcnt >= 1u) {
			rxdatcnt -= 1u;
		}

		adma_writel(rxdaadrcnt, tcc_audio_dma->base_addr + TCC_ADMA_RXDAADRCNT_OFFSET);
		adma_writel(rxdaparam, tcc_audio_dma->base_addr + TCC_ADMA_RXDAPARAM_OFFSET);
		adma_writel(rxdatcnt, tcc_audio_dma->base_addr + TCC_ADMA_RXDATCNT_OFFSET);

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
		addr_lsb = ul_to_ui(tcc_audio_dma->dma_addr & (uint32_t)0xFFFFFFFFU);
		//to do : addr_msb = ul_to_ui(tcc_audio_dma->dma_addr >> 32) & (uint32_t)0xFFFFFFFFU;

		addr_lsb_mono = ul_to_ui(tcc_audio_dma->mono_dma_addr & (uint32_t)0xFFFFFFFFU);
		//to do : addr_msb_mono = ul_to_ui(tcc_audio_dma->mono_dma_addr >> 32) & (uint32_t)0xFFFFFFFFU;

		if (mono_mode) {
			adma_writel(addr_lsb_mono, tcc_audio_dma->base_addr + TCC_ADMA_RXDADAR_OFFSET);
			//to do : write msb mono
			adma_writel(addr_lsb, tcc_audio_dma->base_addr + TCC_ADMA_RXDADARL_OFFSET);
			//to do : write msb
		} else {
			adma_writel(addr_lsb, tcc_audio_dma->base_addr + TCC_ADMA_RXDADAR_OFFSET);
			//to do : write msb
			adma_writel(0, tcc_audio_dma->base_addr + TCC_ADMA_RXDADARL_OFFSET);
			//to do : reset msb mono
		}
#else
		if (mono_mode) {
			adma_writel(tcc_audio_dma->mono_dma_addr, tcc_audio_dma->base_addr + TCC_ADMA_RXDADAR_OFFSET);
			adma_writel(tcc_audio_dma->dma_addr, tcc_audio_dma->base_addr + TCC_ADMA_RXDADARL_OFFSET);
		} else {
			adma_writel(tcc_audio_dma->dma_addr, tcc_audio_dma->base_addr + TCC_ADMA_RXDADAR_OFFSET);
			adma_writel(0, tcc_audio_dma->base_addr + TCC_ADMA_RXDADARL_OFFSET);
		}
#endif
		tcc_adma_set_dai_rx_lrmode(tcc_audio_dma->base_addr, mono_mode);
		tcc_adma_set_dai_rx_transfer_size(tcc_audio_dma->base_addr, wsize, bsize);
		tcc_adma_set_dai_rx_dma_repeat_enable(tcc_audio_dma->base_addr, TRUE);
		tcc_adma_set_dai_rx_dma_width(tcc_audio_dma->base_addr, data_width);
	}

	return ret;
}

static inline int tcc_adma_set_spdif_tx_dma_buffer(
	const struct tcc_audio_dma_t  *tcc_audio_dma,
	enum TCC_ADMA_DATA_WIDTH data_width,
	uint32_t bsize,
	bool adrcnt_mode)
{
	uint32_t wsize = (uint32_t)TCC_ADMA_WORD_SIZE_32;
	uint32_t wbytes = ui_lshift(1u, wsize);
	uint32_t dma_buffer = 0;
	uint32_t txspparam = 0;
	uint32_t txsptcnt = 0;
 	uint32_t txspadrcnt;
 	int ret = 0;
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	uint32_t addr_lsb;
	//to do : uint32_t addr_msb;
#endif


	tcc_adma_spdif_tx_dma_enable(tcc_audio_dma->base_addr, FALSE);

	if ((tcc_audio_dma->buffer_bytes <= 0u) || (tcc_audio_dma->dma_addr > UINT_MAX)) {
		(void) pr_err("buffer_bytes is zero!\n");
		ret = -EINVAL;
	} else if (wbytes == 0u) {
		ret = -EINVAL;
	} else {
 		if (adrcnt_mode) {
			uint32_t adrcnt = tcc_audio_dma->buffer_bytes / wbytes;

			if ((adrcnt % 8u) != 0u) {
				(void) pr_info("[WARN][DAI][%s][%d] Warning!!",
						__func__,
						__LINE__);
				(void) pr_info("adrcnt value should be 8n.\n");
			}

			if(adrcnt >= 1u){
				adrcnt -= 1u;
			} else {
				adrcnt = 0u;
			}

			txspadrcnt = ((adrcnt) << ADMA_ADRCNT_ADDR_COUNT_Pos) &
						ADMA_ADRCNT_ADDR_COUNT_Msk;
			txspadrcnt |= ADMA_ADRCNT_MODE_ADRCNT;
		} else {
			uint32_t value = 0u;

			value = ((0x7fffffffU) << ADMA_ADRCNT_ADDR_COUNT_Pos) &
						ADMA_ADRCNT_ADDR_COUNT_Msk;
			txspadrcnt = ADMA_ADRCNT_MODE_SMASK | value;

			value = ~((uint32_t) (tcc_audio_dma->buffer_bytes - 1u)>>4);
			dma_buffer = ((value) << ADMA_PARAM_ADDR_MASK_Pos) &
						ADMA_PARAM_ADDR_MASK_Msk;
		}
 		txspparam = dma_buffer | wbytes;
		if(wsize <= (UINT_MAX - bsize)){
			txsptcnt = tcc_audio_dma->period_bytes >> (wsize + bsize);
		}
		if(txsptcnt >= 1u){
			txsptcnt -= 1u;
		}
 		adma_writel(txspadrcnt, tcc_audio_dma->base_addr + TCC_ADMA_TXSPADRCNT_OFFSET);
 		adma_writel(txspparam, tcc_audio_dma->base_addr + TCC_ADMA_TXSPPARAM_OFFSET);
		adma_writel(txsptcnt, tcc_audio_dma->base_addr + TCC_ADMA_TXSPTCNT_OFFSET);

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
		addr_lsb = ul_to_ui(tcc_audio_dma->dma_addr & (uint32_t)0xFFFFFFFFU);
		//to do : addr_msb = ul_to_ui(tcc_audio_dma->dma_addr >> 32) & (uint32_t)0xFFFFFFFFU;

		if (adrcnt_mode) {
			adma_writel(addr_lsb, tcc_audio_dma->base_addr + TCC_ADMA_TXSPSAR_OFFSET);
			//to do : write msb
			adma_writel(addr_lsb, tcc_audio_dma->base_addr + TCC_ADMA_TXSPSARL_OFFSET);
			//to do : write msb
		} else {
			adma_writel(addr_lsb, tcc_audio_dma->base_addr + TCC_ADMA_TXSPSAR_OFFSET);
			//to do : reset msb
			adma_writel(0, tcc_audio_dma->base_addr + TCC_ADMA_TXSPSARL_OFFSET);
			//to do : reset msb
		}

#else
		if (adrcnt_mode) {
			adma_writel(tcc_audio_dma->dma_addr, tcc_audio_dma->base_addr + TCC_ADMA_TXSPSAR_OFFSET);
			adma_writel(tcc_audio_dma->dma_addr, tcc_audio_dma->base_addr + TCC_ADMA_TXSPSARL_OFFSET);
		} else {
			adma_writel(tcc_audio_dma->dma_addr, tcc_audio_dma->base_addr + TCC_ADMA_TXSPSAR_OFFSET);
			adma_writel(0, tcc_audio_dma->base_addr + TCC_ADMA_TXSPSARL_OFFSET);
		}
#endif
		tcc_adma_set_spdif_tx_transfer_size(tcc_audio_dma->base_addr, wsize, bsize);
		tcc_adma_set_spdif_tx_dma_repeat_enable(tcc_audio_dma->base_addr, TRUE);
		tcc_adma_set_spdif_tx_dma_width(tcc_audio_dma->base_addr, data_width);
	}

	return ret;
}

static inline int tcc_adma_set_spdif_rx_dma_buffer(
	const struct tcc_audio_dma_t  *tcc_audio_dma,
	enum TCC_ADMA_DATA_WIDTH data_width,
	uint32_t bsize,
	bool adrcnt_mode)
{
	uint32_t wsize = (uint32_t)TCC_ADMA_WORD_SIZE_32;
	uint32_t wbytes = ui_lshift(1u, wsize);
	uint32_t dma_buffer = 0;
	uint32_t rxspparam = 0;
	uint32_t rxsptcnt = 0;
 	uint32_t rxspadrcnt;
 	int ret = 0;
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	uint32_t addr_lsb;
	#if 0//for the future
	uint32_t addr_msb;
	#endif
#endif

	tcc_adma_spdif_rx_dma_en(tcc_audio_dma->base_addr, FALSE);

	if ((tcc_audio_dma->buffer_bytes <= 0u) || (tcc_audio_dma->dma_addr > UINT_MAX)){
		(void) pr_err("buffer_bytes is zero!\n");
		ret = -EINVAL;
	} else if (wbytes == 0u) {
		ret = -EINVAL;
	} else {
 		if (adrcnt_mode) {
			uint32_t adrcnt = tcc_audio_dma->buffer_bytes / wbytes;

			if ((adrcnt % 8u) != 0u) {
				(void) pr_info("[WARN][DAI][%s][%d] Warning!!",
					__func__, __LINE__);
				(void) pr_info("adrcnt value should be 8n.\n");
			}

			if (adrcnt >= 1u) {
				adrcnt -= 1u;
			}
			rxspadrcnt = ((adrcnt) << ADMA_ADRCNT_ADDR_COUNT_Pos) &
						ADMA_ADRCNT_ADDR_COUNT_Msk;
			rxspadrcnt |= ADMA_ADRCNT_MODE_ADRCNT;
		} else {
			uint32_t value = 0u;
			value = ((0x7fffffffU) << ADMA_ADRCNT_ADDR_COUNT_Pos) &
						ADMA_ADRCNT_ADDR_COUNT_Msk;
			rxspadrcnt = ADMA_ADRCNT_MODE_SMASK | value;

			value = ~((uint32_t) (tcc_audio_dma->buffer_bytes - 1u)>>4);
			dma_buffer = ((value) << ADMA_PARAM_ADDR_MASK_Pos) &
						ADMA_PARAM_ADDR_MASK_Msk;
		}
 		rxspparam = dma_buffer | ((uint32_t) 1 << (uint32_t) wsize);

		if(wsize <= (UINT_MAX - bsize)){
			rxsptcnt = (tcc_audio_dma->period_bytes >> (wsize + bsize));
		}

		if (rxsptcnt >= 1u) {
			rxsptcnt  -= 1u;
		}
 		adma_writel(rxspadrcnt, tcc_audio_dma->base_addr + TCC_ADMA_RXSPADRCNT_OFFSET);
		adma_writel(rxspparam, tcc_audio_dma->base_addr + TCC_ADMA_RXCDPARAM_OFFSET);
		adma_writel(rxsptcnt, tcc_audio_dma->base_addr + TCC_ADMA_RXCDTCNT_OFFSET);

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
		addr_lsb = ul_to_ui(tcc_audio_dma->dma_addr & (uint32_t)0xFFFFFFFFU);
		#if 0//for the future
		addr_msb = ul_to_ui(tcc_audio_dma->dma_addr >> 32) & (uint32_t)0xFFFFFFFFU;
		#endif

		adma_writel(addr_lsb, tcc_audio_dma->base_addr + TCC_ADMA_RXCDDAR_OFFSET);
		//to do : write msb
		adma_writel(0, tcc_audio_dma->base_addr + TCC_ADMA_RXCDDARL_OFFSET);
		//to do : reset msb

#else
		adma_writel(tcc_audio_dma->dma_addr, tcc_audio_dma->base_addr + TCC_ADMA_RXCDDAR_OFFSET);
		adma_writel(0, tcc_audio_dma->base_addr + TCC_ADMA_RXCDDARL_OFFSET);
#endif

		tcc_adma_set_spdif_rx_transfer_size(tcc_audio_dma->base_addr, wsize, bsize);
		tcc_adma_set_spdif_rx_rept_en(tcc_audio_dma->base_addr, TRUE);
		tcc_adma_set_spdif_rx_width(tcc_audio_dma->base_addr, data_width);
	}

	return ret;
}

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
static inline uint64_t tcc_adma_dai_tx_get_cur_dma_addr(const void __iomem *base_addr)
{
	uint32_t addr_lsb, addr_msb;
	addr_lsb = readl(base_addr + TCC_ADMA_TXDACSAR_OFFSET);
	addr_msb = 0x0; //readl(base_addr + TCC_XXXXXXXXXXXXX_OFFSET); //to do
	return (((uint64_t)addr_msb << 32) + addr_lsb);
}

static inline uint64_t tcc_adma_dai_rx_get_cur_dma_addr(const void __iomem *base_addr)
{
	uint32_t addr_lsb, addr_msb;
	addr_lsb = readl(base_addr + TCC_ADMA_RXDACDAR_OFFSET);
	addr_msb = 0x0; //readl(base_addr + TCC_XXXXXXXXXXXXX_OFFSET); //to do
	return (((uint64_t)addr_msb << 32) + addr_lsb);
}

static inline uint64_t tcc_adma_spdif_tx_get_cur_dma_addr(const void __iomem *base_addr)
{
	uint32_t addr_lsb, addr_msb;
	addr_lsb = readl(base_addr + TCC_ADMA_TXSPCSAR_OFFSET);
	addr_msb = 0x0; //readl(base_addr + TCC_XXXXXXXXXXXXX_OFFSET);  //to do
	return (((uint64_t)addr_msb << 32) + addr_lsb);
}

static inline uint64_t tcc_adma_spdif_rx_get_cur_dma_addr(const void __iomem *base_addr)
{
	uint32_t addr_lsb, addr_msb;
	addr_lsb = readl(base_addr + TCC_ADMA_RXCDCDAR_OFFSET);
	addr_msb = 0x0; //readl(base_addr + TCC_XXXXXXXXXXXXX_OFFSET);  //to do
	return (((uint64_t)addr_msb << 32) + addr_lsb);
}

static inline uint64_t tcc_adma_dai_tx_get_cur_mono_dma_addr(const void __iomem *base_addr)
{
	uint32_t addr_lsb, addr_msb;
	addr_lsb = readl(base_addr + TCC_ADMA_TXDACSARL_OFFSET);
	addr_msb = 0x0; //readl(base_addr + TCC_XXXXXXXXXXXXX_OFFSET);  //to do
	return (((uint64_t)addr_msb << 32) + addr_lsb);
}

static inline uint64_t tcc_adma_dai_rx_get_cur_mono_dma_addr(const void __iomem *base_addr)
{
	uint32_t addr_lsb, addr_msb;
	addr_lsb = readl(base_addr + TCC_ADMA_RXDACDARL_OFFSET);
	addr_msb = 0x0; //readl(base_addr + TCC_XXXXXXXXXXXXX_OFFSET);  //to do
	return (((uint64_t)addr_msb << 32) + addr_lsb);
}
#else
static inline uint32_t tcc_adma_dai_tx_get_cur_dma_addr(const void __iomem *base_addr)
{
	return readl(base_addr + TCC_ADMA_TXDACSAR_OFFSET);
}

static inline uint32_t tcc_adma_dai_rx_get_cur_dma_addr(const void __iomem *base_addr)
{
	return readl(base_addr + TCC_ADMA_RXDACDAR_OFFSET);
}

static inline uint32_t tcc_adma_spdif_tx_get_cur_dma_addr(const void __iomem *base_addr)
{
	return readl(base_addr + TCC_ADMA_TXSPCSAR_OFFSET);
}

static inline uint32_t tcc_adma_spdif_rx_get_cur_dma_addr(const void __iomem *base_addr)
{
	return readl(base_addr + TCC_ADMA_RXCDCDAR_OFFSET);
}

static inline uint32_t tcc_adma_dai_tx_get_cur_mono_dma_addr(const void __iomem *base_addr)
{
	return readl(base_addr + TCC_ADMA_TXDACSARL_OFFSET);
}

static inline uint32_t tcc_adma_dai_rx_get_cur_mono_dma_addr(const void __iomem *base_addr)
{
	return readl(base_addr + TCC_ADMA_RXDACDARL_OFFSET);
}
#endif

static inline void tcc_adma_dai_tx_hopcnt_clear(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
	uint32_t addr = readl(base_addr + TCC_ADMA_TXDASAR_OFFSET);

	value &= ~ADMA_TRANSCTRL_HOP_COUNT_MODE_Msk;

	adma_writel(
		value | ADMA_TRANSCTRL_HOP_COUNT_CLEAR_MODE,
		(base_addr + TCC_ADMA_TRANSCTRL_OFFSET));
	adma_writel(addr, base_addr + TCC_ADMA_TXDASAR_OFFSET);
	while ((readl(base_addr + TCC_ADMA_TXDATCNT_OFFSET) &
		ADMA_COUNTER_CUR_COUNT_Msk) != 0u) {
	};

	adma_writel(
		value | ADMA_TRANSCTRL_HOP_COUNT_INCR_MODE,
		base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_dai_rx_hopcnt_clear(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
	uint32_t addr = readl(base_addr + TCC_ADMA_RXDADAR_OFFSET);

	value &= ~ADMA_TRANSCTRL_HOP_COUNT_MODE_Msk;

	adma_writel(
		value | ADMA_TRANSCTRL_HOP_COUNT_CLEAR_MODE,
		base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
	adma_writel(addr, base_addr + TCC_ADMA_RXDADAR_OFFSET);
	while ((readl(base_addr + TCC_ADMA_RXDATCNT_OFFSET) &
		ADMA_COUNTER_CUR_COUNT_Msk) != 0u) {
	};
	adma_writel(
		value | ADMA_TRANSCTRL_HOP_COUNT_INCR_MODE,
		base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_spdif_tx_hopcnt_clear(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
	uint32_t addr = readl(base_addr + TCC_ADMA_TXSPSAR_OFFSET);

	value &= ~ADMA_TRANSCTRL_HOP_COUNT_MODE_Msk;

	adma_writel(
		value | ADMA_TRANSCTRL_HOP_COUNT_CLEAR_MODE,
		base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
	adma_writel(addr, base_addr + TCC_ADMA_TXSPSAR_OFFSET);
	while ((readl(base_addr + TCC_ADMA_TXSPTCNT_OFFSET) &
		ADMA_COUNTER_CUR_COUNT_Msk) != 0u) {
	};
	adma_writel(
		value | ADMA_TRANSCTRL_HOP_COUNT_INCR_MODE,
		base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_spdif_rx_hopcnt_clear(void __iomem *base_addr)
{
	uint32_t value = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
	uint32_t addr = readl(base_addr + TCC_ADMA_RXCDDAR_OFFSET);

	value &= ~ADMA_TRANSCTRL_HOP_COUNT_MODE_Msk;

	adma_writel(
		value | ADMA_TRANSCTRL_HOP_COUNT_CLEAR_MODE,
		base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
	adma_writel(addr, base_addr + TCC_ADMA_RXCDDAR_OFFSET);
	while ((readl(base_addr + TCC_ADMA_RXCDTCNT_OFFSET) &
		ADMA_COUNTER_CUR_COUNT_Msk) != 0u) {
	};
	adma_writel(
		value | ADMA_TRANSCTRL_HOP_COUNT_INCR_MODE,
		base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
}

static inline void tcc_adma_reg_backup(const void __iomem *base_addr,
	struct adma_reg_t *reg)
{
	reg->txdaddrcnt = readl(base_addr + TCC_ADMA_TXDAADRCNTR_OFFSET);
	reg->txdaparam = readl(base_addr + TCC_ADMA_TXDAPARAM_OFFSET);
	reg->txdatcnt=  readl(base_addr + TCC_ADMA_TXDATCNT_OFFSET);
	reg->txdasar = readl(base_addr + TCC_ADMA_TXDASAR_OFFSET);
	reg->txdasarl = readl(base_addr + TCC_ADMA_TXDASARL_OFFSET);
	reg->rxdaddrcnt = readl(base_addr + TCC_ADMA_RXDAADRCNTR_OFFSET);
	reg->rxdaparam = readl(base_addr + TCC_ADMA_RXDAPARAM_OFFSET);
	reg->rxdatcnt = readl(base_addr + TCC_ADMA_RXDATCNT_OFFSET);
	reg->rxdadar = readl(base_addr + TCC_ADMA_RXDADAR_OFFSET);
	reg->rxdadarl = readl(base_addr + TCC_ADMA_RXDADARL_OFFSET);
	reg->txspadrcnt = readl(base_addr + TCC_ADMA_TXSPADRCNTR_OFFSET);
	reg->txsparam = readl(base_addr + TCC_ADMA_TXSPPARAM_OFFSET);
	reg->txsptcnt = readl(base_addr + TCC_ADMA_TXSPTCNT_OFFSET);
	reg->txspsar = readl(base_addr + TCC_ADMA_TXSPSAR_OFFSET);
	reg->txspsarl = readl(base_addr + TCC_ADMA_TXSPSARL_OFFSET);
	reg->rxspadrcnt = readl(base_addr + TCC_ADMA_RXSPADRCNTR_OFFSET);
	reg->rxspparam = readl(base_addr + TCC_ADMA_RXCDPARAM_OFFSET);
	reg->rxsptcnt = readl(base_addr + TCC_ADMA_RXCDTCNT_OFFSET);
	reg->rxspdar = readl(base_addr + TCC_ADMA_RXCDDAR_OFFSET);
	reg->rxspdarl = readl(base_addr + TCC_ADMA_RXCDDARL_OFFSET);
	reg->transctrl = readl(base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
	reg->rptctrl = readl(base_addr + TCC_ADMA_RPTCTRL_OFFSET);
	reg->chctrl = readl(base_addr + TCC_ADMA_CHCTRL_OFFSET);
}

static inline void tcc_adma_reg_restore(void __iomem *base_addr,
	struct adma_reg_t *reg)
{
	adma_writel(reg->txdaddrcnt, base_addr + TCC_ADMA_TXDAADRCNT_OFFSET);
	adma_writel(reg->txdaparam, base_addr + TCC_ADMA_TXDAPARAM_OFFSET);
	adma_writel(reg->txdatcnt, base_addr + TCC_ADMA_TXDATCNT_OFFSET);
	adma_writel(reg->txdasar, base_addr + TCC_ADMA_TXDASAR_OFFSET);
	adma_writel(reg->txdasarl, base_addr + TCC_ADMA_TXDASARL_OFFSET);
	adma_writel(reg->rxdaddrcnt, base_addr + TCC_ADMA_RXDAADRCNT_OFFSET);
	adma_writel(reg->rxdaparam, base_addr + TCC_ADMA_RXDAPARAM_OFFSET);
	adma_writel(reg->rxdatcnt, base_addr + TCC_ADMA_RXDATCNT_OFFSET);
	adma_writel(reg->rxdadar, base_addr + TCC_ADMA_RXDADAR_OFFSET);
	adma_writel(reg->rxdadarl, base_addr + TCC_ADMA_RXDADARL_OFFSET);
	adma_writel(reg->txspadrcnt, base_addr + TCC_ADMA_TXSPADRCNT_OFFSET);
	adma_writel(reg->txsparam, base_addr + TCC_ADMA_TXSPPARAM_OFFSET);
	adma_writel(reg->txsptcnt, base_addr + TCC_ADMA_TXSPTCNT_OFFSET);
	adma_writel(reg->txspsar, base_addr + TCC_ADMA_TXSPSAR_OFFSET);
	adma_writel(reg->txspsarl, base_addr + TCC_ADMA_TXSPSARL_OFFSET);
	adma_writel(reg->rxspadrcnt, base_addr + TCC_ADMA_RXSPADRCNT_OFFSET);
	adma_writel(reg->rxspparam, base_addr + TCC_ADMA_RXCDPARAM_OFFSET);
	adma_writel(reg->rxsptcnt, base_addr + TCC_ADMA_RXCDTCNT_OFFSET);
	adma_writel(reg->rxspdar, base_addr + TCC_ADMA_RXCDDAR_OFFSET);
	adma_writel(reg->rxspdarl, base_addr + TCC_ADMA_RXCDDARL_OFFSET);
	adma_writel(reg->transctrl, base_addr + TCC_ADMA_TRANSCTRL_OFFSET);
	adma_writel(reg->rptctrl, base_addr + TCC_ADMA_RPTCTRL_OFFSET);
	adma_writel(reg->chctrl, base_addr + TCC_ADMA_CHCTRL_OFFSET);
}
#endif /*TCC_ADMA_H*/
