/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_SPDIF_H
#define TCC_SPDIF_H

#include <linux/io.h>
#include "tcc_audio_hw.h"
#include "tcc_audio_rule.h"

enum TCC_SPDIF__BLK_NO {
	TCC_SPDIF_BLK0 = 0,
	TCC_SPDIF_BLK1 = 1,
	TCC_SPDIF_BLK2 = 2,
	TCC_SPDIF_BLK3 = 3,
	TCC_SPDIF_BLK4 = 4,
	TCC_SPDIF_BLK5 = 5,
	TCC_SPDIF_BLK6 = 6,
	TCC_SPDIF_BLK7 = 7,
	TCC_SPDIF_BLK_MAX = 8
};

#define TCC_SPDIF0 "tcc-spdif-0"
#define TCC_SPDIF1 "tcc-spdif-1"
#define TCC_SPDIF2 "tcc-spdif-2"
#define TCC_SPDIF3 "tcc-spdif-3"
#define TCC_SPDIF4 "tcc-spdif-0"
#define TCC_SPDIF5 "tcc-spdif-1"
#define TCC_SPDIF6 "tcc-spdif-2"
#define TCC_SPDIF7 "tcc-spdif-3"

#define SPDIF0_PLAYBACK_WIDGET		"SPDIF0-Playback"
#define SPDIF0_CAPTURE_WIDGET		"SPDIF0-Capture"
#define SPDIF1_PLAYBACK_WIDGET		"SPDIF1-Playback"
#define SPDIF1_CAPTURE_WIDGET		"SPDIF1-Capture"
#define SPDIF2_PLAYBACK_WIDGET		"SPDIF2-Playback"
#define SPDIF2_CAPTURE_WIDGET		"SPDIF2-Capture"
#define SPDIF3_PLAYBACK_WIDGET		"SPDIF3-Playback"
#define SPDIF3_CAPTURE_WIDGET		"SPDIF3-Capture"
#define SPDIF4_PLAYBACK_WIDGET		"SPDIF4-Playback"
#define SPDIF4_CAPTURE_WIDGET		"SPDIF4-Capture"
#define SPDIF5_PLAYBACK_WIDGET		"SPDIF5-Playback"
#define SPDIF5_CAPTURE_WIDGET		"SPDIF5-Capture"
#define SPDIF6_PLAYBACK_WIDGET		"SPDIF6-Playback"
#define SPDIF6_CAPTURE_WIDGET		"SPDIF6-Capture"
#define SPDIF7_PLAYBACK_WIDGET		"SPDIF7-Playback"
#define SPDIF7_CAPTURE_WIDGET		"SPDIF7-Capture"


#define TCC_SPDIF_RX_BUF_MAX_CNT \
	(8)

enum TCC_SPDIF_TX_USERDATA_TYPE {
	// User Data A & B generated from TxChStat
	TCC_SPDIF_TX_USERDATA_TYPE0 = 0,
	// User Data A & B generated from ChStat bit7-0
	TCC_SPDIF_TX_USERDATA_TYPE1 = 1,
	// User Data A generated from ChStat bit7-0,
	// B generated from ChStat bit15-8
	TCC_SPDIF_TX_USERDATA_TYPE2 = 2,
};

enum TCC_SPDIF_TX_CHSTAT_TYPE {
	// Channel Status A & B generated from TxChStat
	TCC_SPDIF_TX_CHSTAT_TYPE0 = 0,
	// Channel Status A & B generated from ChStat bit7-0
	TCC_SPDIF_TX_CHSTAT_TYPE1 = 1,
	// Channel Status A generated from ChStat bit7-0,
	// B generated from ChStat bit15-8
	TCC_SPDIF_TX_CHSTAT_TYPE2 = 2,
};

enum TCC_SPDIF_TX_FORMAT {
	TCC_SPDIF_TX_FORMAT_AUDIO = 0,
	TCC_SPDIF_TX_FORMAT_DATA = 1,
};

enum TCC_SPDIF_TX_COPYRIGHT {
	TCC_SPDIF_TX_COPY_INHIBITED = 0,
	TCC_SPDIF_TX_COPY_PERMITTED = 1,
};

enum TCC_SPDIF_TX_PREEMPHASIS {
	TCC_SPDIF_TX_PREEMPHASIS_NONE = 0,
	TCC_SPDIF_TX_PREEMPHASIS_50_15us = 1,
};

enum TCC_SPDIF_TX_STATUS_GERNERATION {
	TCC_SPDIF_TX_NO_INDICATION = 0,
	TCC_SPDIF_TX_PRERECORDED_DATA = 1,
};

enum TCC_SPDIF_TX_FREQ {
	TCC_SPDIF_TX_44100HZ = 0,
	TCC_SPDIF_TX_48000HZ = 1,
	TCC_SPDIF_TX_32000HZ = 2,
	TCC_SPDIF_TX_SRC = 3,
};

enum TCC_SPDIF_TX_ADDRESS_MODE {
	TCC_SPDIF_TX_ADDRESS_MODE_DISABLE = 0,
	TCC_SPDIF_TX_ADDRESS_MODE_TYPE0 = 1,
	// 0, 2, 4, 8, 1, 3, 5, 7 sequence
	TCC_SPDIF_TX_ADDRESS_MODE_TYPE1 = 2,
};

enum TCC_SPDIF_TX_READADDR_MODE {
	TCC_SPDIF_TX_24BIT = 0,
	TCC_SPDIF_TX_24BIT_LR = 1,
	TCC_SPDIF_TX_16BIT = 2,
	TCC_SPDIF_TX_16BIT_LR = 3,
};

enum TCC_SPDIF_RX_HOLD_CH_TYPE {
	TCC_SPDIF_RX_HOLD_CH_A = 1,
	TCC_SPDIF_RX_HOLD_CH_B = 0,
};

enum TCC_SPDIF_RX_SAMPLEDATA_STORE_TYPE {
	TCC_SPDRX_SDATA_STORED_RGDLESS = 0,
	TCC_SPDRX_SDATA_STORED_ONLY_VBIT = 1,
};

struct spdif_reg_t {
	uint32_t tx_version;
	uint32_t tx_config;
	uint32_t tx_chstat;
	uint32_t tx_intmask;
	uint32_t tx_dmacfg;

	uint32_t rx_version;
	uint32_t rx_config;
	uint32_t rx_sigstat;
	uint32_t rx_intmask;
};

#if 0 //DEBUG
#define spdif_writel(v, c) \
	({pr_info("SPDIF_REG(0x%08x) = 0x%08x\n", c, v); writel(v, c); })
#else
#define spdif_writel(v, c) \
	writel(v, c)
#endif

static inline void tcc_spdif_tx_dump(const void __iomem *base_addr)
{
	(void) pr_info("TX_VERSION : 0x%08x\n",
		readl(base_addr + TCC_SPDIF_TX_VERSION_OFFSET));
	(void) pr_info("TX_CONFIG  : 0x%08x\n",
		readl(base_addr + TCC_SPDIF_TX_CONFIG_OFFSET));
	(void) pr_info("TX_CHSTAT  : 0x%08x\n",
		readl(base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET));
	(void) pr_info("TX_INTMASK : 0x%08x\n",
		readl(base_addr + TCC_SPDIF_TX_INTMASK_OFFSET));
	(void) pr_info("TX_INTSTAT : 0x%08x\n",
		readl(base_addr + TCC_SPDIF_TX_INTSTAT_OFFSET));
	(void) pr_info("TX_DMACFG  : 0x%08x\n",
		readl(base_addr + TCC_SPDIFTX_DMACFG_OFFSET));
}

static inline void tcc_spdif_tx_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);

	value &= ~(SPDIF_TX_CONFIG_TX_ENABLE_Msk);
	if (enable) {
		value |= SPDIF_TX_CONFIG_TX_ENABLE;
	} else {
		value |= SPDIF_TX_CONFIG_TX_DISABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);
}

static inline void tcc_spdif_tx_data_valid(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);

	value &= ~(SPDIF_TX_CONFIG_DATA_VALID_Msk);
	if (enable) {
		value |= SPDIF_TX_CONFIG_DATA_VALID_ENABLE;
	} else {
		value |= SPDIF_TX_CONFIG_DATA_VALID_DISABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);
}

static inline void tcc_spdif_tx_irq_enable(void __iomem *base_addr, bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);

	value &= ~(SPDIF_TX_CONFIG_IRQ_ENABLE_Msk);
	if (enable) {
		value |= SPDIF_TX_CONFIG_IRQ_ENABLE;
	} else {
		value |= SPDIF_TX_CONFIG_IRQ_DISABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);
}

static inline void tcc_spdif_tx_userdata_type(
	void __iomem *base_addr,
	enum TCC_SPDIF_TX_USERDATA_TYPE type)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);

	value &= ~SPDIF_TX_CONFIG_UDATA_EN_Msk;
	value |= (((uint32_t) type) << SPDIF_TX_CONFIG_UDATA_EN_Pos) &
			SPDIF_TX_CONFIG_UDATA_EN_Msk;

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);
}

static inline void tcc_spdif_tx_chstat_type(
	void __iomem *base_addr,
	enum TCC_SPDIF_TX_CHSTAT_TYPE type)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);

	value &= ~SPDIF_TX_CONFIG_CHSTAT_EN_Msk;
	value |= (((uint32_t) type) << SPDIF_TX_CONFIG_CHSTAT_EN_Pos) &
			SPDIF_TX_CONFIG_CHSTAT_EN_Msk;

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);
}

static inline void tcc_spdif_tx_clk_ratio(
	void __iomem *base_addr,
	uint32_t ratio)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);
	uint32_t div_ratio = (ratio > 1u)? (ratio - 1u) : 0u;

	value &= ~SPDIF_TX_CONFIG_RATIO_Msk;
	value |= (div_ratio << SPDIF_TX_CONFIG_RATIO_Pos) &
			SPDIF_TX_CONFIG_RATIO_Msk;

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);
}

static inline void tcc_spdif_tx_bitmode(
	void __iomem *base_addr,
	uint32_t bitmode)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);

	if (bitmode >= 16u) {
		bitmode = bitmode - 16u;
	} else {
		bitmode = 0;
	}

	value &= ~SPDIF_TX_CONFIG_MODE_Msk;
	value |= (bitmode << SPDIF_TX_CONFIG_MODE_Pos) &
			SPDIF_TX_CONFIG_MODE_Msk;

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);
}

static inline void tcc_spdif_tx_format(
	void __iomem *base_addr,
	enum TCC_SPDIF_TX_FORMAT format)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);

	value &= ~(SPDIF_TX_CHSTAT_FORMAT_Msk);
	if (format == TCC_SPDIF_TX_FORMAT_DATA) {
		value |= SPDIF_TX_CHSTAT_FORMAT_DATA;
	} else {
		value |= SPDIF_TX_CHSTAT_FORMAT_AUDIO;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);
}

static inline void tcc_spdif_tx_copyright(
	void __iomem *base_addr,
	enum TCC_SPDIF_TX_COPYRIGHT copyright)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);

	value &= ~(SPDIF_TX_CHSTAT_COPYRIGHT_Msk);
	if (copyright == TCC_SPDIF_TX_COPY_PERMITTED) {
		value |= SPDIF_TX_CHSTAT_COPY_PERMITTED;
	} else {
		value |= SPDIF_TX_CHSTAT_COPY_INHIBITED;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);
}

static inline void tcc_spdif_tx_preemphasis(
	void __iomem *base_addr,
	enum TCC_SPDIF_TX_PREEMPHASIS preemphasis)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);

	value &= ~(SPDIF_TX_CHSTAT_PREEMPHASIS_Msk);
	if (preemphasis == TCC_SPDIF_TX_PREEMPHASIS_50_15us) {
		value |= SPDIF_TX_CHSTAT_COPY_PERMITTED;
	} else {
		value |= SPDIF_TX_CHSTAT_COPY_INHIBITED;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);
}

static inline void tcc_spdif_tx_status_generation(
	void __iomem *base_addr,
	enum TCC_SPDIF_TX_STATUS_GERNERATION gen)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);

	value &= ~(SPDIF_TX_CHSTAT_STATUS_GEN_Msk);
	if (gen == TCC_SPDIF_TX_PRERECORDED_DATA) {
		value |= SPDIF_TX_CHSTAT_STATUS_PRE_RECORDED;
	} else {
		value |= SPDIF_TX_CHSTAT_STATUS_NO_INDICATION;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);
}

static inline void tcc_spdif_tx_frequency(
	void __iomem *base_addr,
	enum TCC_SPDIF_TX_FREQ freq)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);

	value &= SPDIF_TX_CHSTAT_FREQ_Msk;
	value |= (((uint32_t) freq) << SPDIF_TX_CHSTAT_FREQ_Pos) &
			SPDIF_TX_CHSTAT_FREQ_Msk;

	spdif_writel(value, base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);
}

static inline void tcc_spdif_tx_fifo_threshold(
	void __iomem *base_addr,
	uint32_t threshold)
{
	uint32_t value = readl(base_addr + TCC_SPDIFTX_DMACFG_OFFSET);

	value &= SPDIFTX_DMACFG_FIFO_THRES_Msk;
	value |= (threshold << SPDIFTX_DMACFG_FIFO_THRES_Pos) &
			SPDIFTX_DMACFG_FIFO_THRES_Msk;

	spdif_writel(value, base_addr + TCC_SPDIFTX_DMACFG_OFFSET);
}

static inline void tcc_spdif_tx_fifo_clear(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIFTX_DMACFG_OFFSET);

	value &= ~(SPDIFTX_DMACFG_FIFO_CLEAR_Msk);
	if (enable) {
		value |= SPDIFTX_DMACFG_FIFO_CLEAR_ENABLE;
	} else {
		value |= SPDIFTX_DMACFG_FIFO_CLEAR_DISABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIFTX_DMACFG_OFFSET);
}

static inline void tcc_spdif_tx_addr_mode(
	void __iomem *base_addr,
	enum TCC_SPDIF_TX_ADDRESS_MODE mode)
{
	uint32_t value = readl(base_addr + TCC_SPDIFTX_DMACFG_OFFSET);

	value &= ~(SPDIFTX_DMACFG_AMODE0_Msk|SPDIFTX_DMACFG_AMODE1_Msk);

	if (mode == TCC_SPDIF_TX_ADDRESS_MODE_DISABLE) {
		value |= SPDIFTX_DMACFG_AMODE0_DISABLE;
		value |= SPDIFTX_DMACFG_AMODE1_DISABLE;
	} else if (mode == TCC_SPDIF_TX_ADDRESS_MODE_TYPE0) {
		value |= SPDIFTX_DMACFG_AMODE0_ENABLE;
		value |= SPDIFTX_DMACFG_AMODE1_DISABLE;
	} else { /*if (mode == TCC_SPDIF_TX_ADDRESS_MODE_TYPE1) {*/
		value |= SPDIFTX_DMACFG_AMODE0_ENABLE;
		value |= SPDIFTX_DMACFG_AMODE1_ENABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIFTX_DMACFG_OFFSET);
}

static inline void tcc_spdif_tx_userdata_dmareq_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIFTX_DMACFG_OFFSET);

	value &= ~(SPDIFTX_DMACFG_UDATA_DMAREQ_Msk);
	if (enable) {
		value |= SPDIFTX_DMACFG_UDATA_DMAREQ_EN;
	} else {
		value |= SPDIFTX_DMACFG_UDATA_DMAREQ_DIS;
	}

	spdif_writel(value, base_addr + TCC_SPDIFTX_DMACFG_OFFSET);
}

static inline void tcc_spdif_tx_sampledata_dmareq_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIFTX_DMACFG_OFFSET);

	value &= ~(SPDIFTX_DMACFG_SDATA_DMAREQ_Msk);
	if (enable) {
		value |= SPDIFTX_DMACFG_SDATA_DMAREQ_EN;
	} else {
		value |= SPDIFTX_DMACFG_SDATA_DMAREQ_DIS;
	}

	spdif_writel(value, base_addr + TCC_SPDIFTX_DMACFG_OFFSET);
}

static inline void tcc_spdif_tx_readaddr_mode(
	void __iomem *base_addr,
	enum TCC_SPDIF_TX_READADDR_MODE mode)
{
	uint32_t value = readl(base_addr + TCC_SPDIFTX_DMACFG_OFFSET);

	value &= ~SPDIFTX_DMACFG_READADDR_MODE_Msk;
	value |= (((uint32_t) mode) << SPDIFTX_DMACFG_READADDR_MODE_Pos) &
			SPDIFTX_DMACFG_READADDR_MODE_Msk;

	spdif_writel(value, base_addr + TCC_SPDIFTX_DMACFG_OFFSET);
}

static inline void tcc_spdif_tx_swap_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIFTX_DMACFG_OFFSET);

	value &= ~(SPDIFTX_DMACFG_SWAP_Msk);
	if (enable) {
		value |= SPDIFTX_DMACFG_SWAP_ENABLE;
	} else {
		value |= SPDIFTX_DMACFG_SWAP_DISABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIFTX_DMACFG_OFFSET);
}

static inline void tcc_spdif_tx_buffers_clear(
	void __iomem *base_addr)
{
	uint32_t i;
	uint32_t buffer_size = 0;

	for (i = 0; i < SPDIFTX_UDATA_BUF_COUNT; i++) {
		buffer_size = ui_to_ui_mul(i, ul_to_ui(sizeof(uint32_t)));;
		spdif_writel(0, base_addr + TCC_SPDIF_TX_USERDATA_BUF_OFFSET +
		buffer_size);
	}

	for (i = 0; i < SPDIF_TX_CHSTAT_BUF_COUNT; i++) {
		buffer_size = ui_to_ui_mul(i, ul_to_ui(sizeof(uint32_t)));;
		spdif_writel(0, base_addr + TCC_SPDIF_TX_CHSTAT_BUF_OFFSET +
		buffer_size);
	}

	for (i = 0; i < SPDIFTX_SDATA_BUF_COUNT; i++) {
		buffer_size = ui_to_ui_mul(i, ul_to_ui(sizeof(uint32_t)));;
		spdif_writel(0, base_addr + TCC_SPDIF_TX_SAMPLEDATA_BUF_OFFSET +
		buffer_size);
	}
}

///////////////////////

static inline void tcc_spdif_rx_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	value &= ~(SPDIF_RX_CONFIG_RX_ENABLE_Msk);
	if (enable) {
		value |= SPDIF_RX_CONFIG_RX_ENABLE;
	} else {
		value |= SPDIF_RX_CONFIG_RX_DISABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
}

static inline void tcc_spdif_rx_stored_data_in_sample_buf(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	value &= ~(SPDIF_RX_CONFIG_STORED_DATA_Msk);
	if (enable) {
		value |= SPDIF_RX_CONFIG_STORED_DATA;
	} else {
		value |= SPDIF_RX_CONFIG_STORED_NO_DATA;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
}

static inline void tcc_spdif_rx_irq_enable(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	value &= ~(SPDIF_RX_CONFIG_IRQ_ENABLE_Msk);
	if (enable) {
		value |= SPDIF_RX_CONFIG_IRQ_ENABLE;
	} else {
		value |= SPDIF_RX_CONFIG_IRQ_DISABLE;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
}

static inline void tcc_spdif_rx_hold_ch_type(
	void __iomem *base_addr,
	enum TCC_SPDIF_RX_HOLD_CH_TYPE type)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	value &= ~(SPDIF_RX_CONFIG_IRQ_ENABLE_Msk);
	if (type == TCC_SPDIF_RX_HOLD_CH_A) {
		value |= SPDIF_RX_CONFIG_HOLD_CH_A;
	} else {
		value |= SPDIF_RX_CONFIG_HOLD_CH_B;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
}

static inline void tcc_spdif_rx_sampledata_store_type(
	void __iomem *base_addr,
	enum TCC_SPDIF_RX_SAMPLEDATA_STORE_TYPE type)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	value &= ~(SPDIFRX_CONF_STR_VALID_TYPE_Msk);
	if (type == TCC_SPDRX_SDATA_STORED_ONLY_VBIT) {
		value |= SPDIFRX_CONF_STR_VALID_ONLY;
	} else {
		value |= SPDIFRX_CONF_STR_VALID_REGARDLESS;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
}

static inline void tcc_spdif_rx_store_valid_bit(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	value &= ~(SPDIFRX_CONF_STR_VALID_BIT_Msk);
	if (enable) {
		value |= SPDIFRX_CONF_STR_VALID_BIT_EN;
	} else {
		value |= SPDIFRX_CONF_STR_VALID_BIT_DIS;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
}

static inline void tcc_spdif_rx_store_userdata_bit(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	value &= ~(SPDIFRX_CONF_STR_USERDATA_Msk);
	if (enable) {
		value |= SPDIFRX_CONF_STR_USERDATA_EN;
	} else {
		value |= SPDIFRX_CONF_STR_USERDATA_DIS;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
}

static inline void tcc_spdif_rx_store_channel_status_bit(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	value &= ~(SPDIFRX_CONF_STR_STATUS_BIT_Msk);
	if (enable) {
		value |= SPDIFRX_CONF_STR_STATUS_BIT_EN;
	} else {
		value |= SPDIFRX_CONF_STR_STATUS_BIT_DIS;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
}

static inline void tcc_spdif_rx_store_parity_bit(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	value &= ~(SPDIFRX_CONF_STR_PARITY_BIT_Msk);
	if (enable) {
		value |= SPDIFRX_CONF_STR_PARITY_BIT_EN;
	} else {
		value |= SPDIFRX_CONF_STR_PARITY_BIT_DIS;
	}

	spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
}

static inline void tcc_spdif_rx_bitmode(
	void __iomem *base_addr,
	uint32_t bitwidth)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	if ((bitwidth >= 16u) && (bitwidth <= 24u)) {
		value &= ~SPDIF_RX_CONFIG_MODE_Msk;
		value |= ((bitwidth - 16u) << SPDIF_RX_CONFIG_MODE_Pos) &
				SPDIF_RX_CONFIG_MODE_Msk;

		spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
	}
}

static inline void tcc_spdif_rx_block_marking(
	void __iomem *base_addr,
	bool enable)
{
	uint32_t value = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);

	value &= ~(SPDIF_RX_CONFIG_BLK_MARK_Msk);
	/*
	if (enable) {
		value |= SPDIF_RX_CONFIG_BLK_MARK_EN;
	} else {
		value |= SPDIF_RX_CONFIG_BLK_MARK_DIS;
	}
	*/
	value |= (enable)? SPDIF_RX_CONFIG_BLK_MARK_EN :
			SPDIF_RX_CONFIG_BLK_MARK_DIS;

	spdif_writel(value, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
}

static inline void tcc_spdif_rx_set_phasedet(
	void __iomem *base_addr,
	uint32_t value)
{
	spdif_writel(value, base_addr + TCC_SPDIF_RX_PHASEDET_OFFSET);
}

static inline void tcc_spdif_rx_set_buf(void __iomem *base_addr,
	uint32_t idx, uint32_t value)
{
	uint32_t buffer_size = 0;

	if(idx <= (UINT_MAX / sizeof(uint32_t))){
		buffer_size = (uint32_t) (idx * sizeof(uint32_t));
	}

	spdif_writel(value, base_addr + TCC_SPDIF_RX_BUF_OFFSET +
		buffer_size);
}

static inline void tcc_spdif_reg_backup(
	const void __iomem *base_addr,
	struct spdif_reg_t *regs)
{
	regs->tx_version = readl(base_addr + TCC_SPDIF_TX_VERSION_OFFSET);
	regs->tx_config = readl(base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);
	regs->tx_chstat = readl(base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);
	regs->tx_intmask = readl(base_addr + TCC_SPDIF_TX_INTMASK_OFFSET);
	regs->tx_dmacfg = readl(base_addr + TCC_SPDIFTX_DMACFG_OFFSET);

	regs->rx_version = readl(base_addr + TCC_SPDIF_RX_VER_OFFSET);
	regs->rx_config = readl(base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
	regs->rx_sigstat = readl(base_addr + TCC_SPDIF_RX_SIGSTAT_OFFSET);
	regs->rx_intmask = readl(base_addr + TCC_SPDIF_RX_INTMASK_OFFSET);
}

static inline void tcc_spdif_reg_restore(
	void __iomem *base_addr,
	const struct spdif_reg_t *regs)
{
	spdif_writel(regs->tx_version, base_addr + TCC_SPDIF_TX_VERSION_OFFSET);
	spdif_writel(regs->tx_config, base_addr + TCC_SPDIF_TX_CONFIG_OFFSET);
	spdif_writel(regs->tx_chstat, base_addr + TCC_SPDIF_TX_CHSTAT_OFFSET);
	spdif_writel(regs->tx_intmask, base_addr + TCC_SPDIF_TX_INTMASK_OFFSET);
	spdif_writel(regs->tx_dmacfg, base_addr + TCC_SPDIFTX_DMACFG_OFFSET);

	spdif_writel(regs->rx_version, base_addr + TCC_SPDIF_RX_VER_OFFSET);
	spdif_writel(regs->rx_config, base_addr + TCC_SPDIF_RX_CONFIG_OFFSET);
	spdif_writel(regs->rx_sigstat, base_addr + TCC_SPDIF_RX_SIGSTAT_OFFSET);
	spdif_writel(regs->rx_intmask, base_addr + TCC_SPDIF_RX_INTMASK_OFFSET);
}

#endif /*TCC_SPDIF_H*/
