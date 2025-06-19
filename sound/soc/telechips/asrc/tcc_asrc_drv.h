/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_ASRC_DRV_H
#define TCC_ASRC_DRV_H

#include <sound/soc.h>

#include "tcc_pl080.h"
#include "tcc_asrc.h"
#include "tcc_asrc_m2m.h"
#include "tcc_asrc_m2m_ioctl.h"

#define DRV_NAME "tcc-asrc-drv"

/* #define NUM_OF_ASRC_PAIR\
	(4U) */
#define NUM_OF_ASRC_MCAUDIO\
	(4U)
#define ASRC_RX_DMA_OFFSET\
	(4U)
#define NUM_OF_AUX_PERI_CLKS\
	(4)
#define TCC_ASRC_MIN_CHANNELS\
	(2)

#define DEFAULT_VOLUME_GAIN\
	(0x100000) // 0dB

#define DEFAULT_VOLUME_RAMP_TIME\
	(TCC_ASRC_RAMP_0_125DB_PER_1SAMPLE)
#define DEFAULT_VOLUME_RAMP_WAIT\
	(0)
#define DEFAULT_VOLUME_RAMP_GAIN\
	(0) // 0dB

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
//#define ASRC_M2M_INTERRUPT_MODE
#endif

enum tcc_asrc_drv_path_t {
	TCC_ASRC_M2M_PATH = 0,
	TCC_ASRC_M2P_PATH = 1,
	TCC_ASRC_P2M_PATH = 2,
};

enum tcc_asrc_async_refclk_t {
	TCC_ASRC_ASYNC_REFCLK_DAI0 = 0,
	TCC_ASRC_ASYNC_REFCLK_DAI1 = 1,
	TCC_ASRC_ASYNC_REFCLK_DAI2 = 2,
	TCC_ASRC_ASYNC_REFCLK_DAI3 = 3,
	TCC_ASRC_ASYNC_REFCLK_AUX  = 99,
};

enum tcc_asrc_drv_sync_mode_t {
	TCC_ASRC_SYNC_MODE	= 0,
	TCC_ASRC_ASYNC_MODE = 1,
};

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
enum tcc_asrc_drv_fifo_size_t {
	TCC_ASRC_FIFO_SIZE_256WORD = 256,
	TCC_ASRC_FIFO_SIZE_128WORD = 128,
	TCC_ASRC_FIFO_SIZE_64WORD = 64,
	TCC_ASRC_FIFO_SIZE_32WORD = 32,
	TCC_ASRC_FIFO_SIZE_16WORD = 16,
	TCC_ASRC_FIFO_SIZE_8WORD = 8,
	TCC_ASRC_FIFO_SIZE_4WORD = 4,
	TCC_ASRC_FIFO_SIZE_2WORD = 2,
};
#endif

struct tcc_asrc_hw_param_t{
	enum tcc_asrc_drv_sync_mode_t sync_mode;
	enum tcc_asrc_drv_bitwidth_t  bitwidth;
	enum tcc_asrc_drv_ch_t		  channels;
	enum tcc_asrc_peri_t		  peri_dai;
	enum tcc_asrc_drv_bitwidth_t  peri_dai_bitwidth;
	enum tcc_asrc_async_refclk_t  async_refclk;
	uint32_t					  src_rate;
	uint32_t					  dst_rate;
};

struct tcc_pl080_buf_t {
	void *virt;
	dma_addr_t phys_addr;

	struct pl080_lli *lli_virt;
	dma_addr_t lli_phys;
};

struct volume_ramp_t{
	uint32_t gain; // -0.125 * ramp_gain dB
	uint32_t up_time;
	uint32_t dn_time;
	uint32_t up_wait;
	uint32_t dn_wait;
};

struct tcc_asrc_t {
	struct platform_device *pdev;
	void __iomem *asrc_reg;
	uint32_t asrc_reg_phys;
	struct clk *aux_pclk[NUM_OF_AUX_PERI_CLKS];
	struct clk *asrc_hclk;
	uint32_t asrc_irq;

	void __iomem *pl080_reg;
	uint32_t pl080_irq;
	struct clk *pl080_hclk;

#ifdef CONFIG_ARCH_TCC802X
	bool chip_rev_xx;
#endif
	uint32_t m2m_open_cnt;
	struct mutex m2m_mlock;
	//dma buffers
	struct {
		struct tcc_pl080_buf_t txbuf;
		struct tcc_pl080_buf_t rxbuf;
		struct {
			uint32_t max_channel;
			enum tcc_asrc_drv_path_t asrc_path;
			enum tcc_asrc_drv_sync_mode_t sync_mode;
			enum tcc_asrc_async_refclk_t async_refclk;
			enum tcc_asrc_peri_t peri_dai;
			uint32_t peri_dai_rate;
			enum tcc_asrc_drv_bitwidth_t peri_dai_format;
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
			enum tcc_asrc_drv_fifo_size_t fifo_in_size;
#endif
		} hw;
		struct {
			enum tcc_asrc_drv_bitwidth_t tx_bitwidth;
			enum tcc_asrc_drv_bitwidth_t rx_bitwidth;
			enum tcc_asrc_drv_ch_t channels;
			uint32_t ratio_shift22; //|Int(31~22)|Fraction(21~0)|
		} cfg;
		struct {
			uint32_t started;
			uint32_t readable_size;
			uint32_t read_offset;
			uint32_t writable_size;
			uint32_t write_offset;
			struct snd_pcm_substream *substream;
		} m2m_stat; //m2m stat
		struct volume_ramp_t volume_ramp;
		uint32_t volume_gain;

		spinlock_t lock;
		struct mutex m;
		wait_queue_head_t wq;
		struct completion comp_asrc;
	} pair[NUM_OF_ASRC_PAIR];

	//m2p or p2m
	struct snd_soc_dai_driver dai_drv[NUM_OF_ASRC_PAIR];
	int mcaudio_m2p_mux[NUM_OF_ASRC_MCAUDIO];

	struct asrc_reg_t asrc_regs_backup;
};

extern uint32_t tcc_asrc_txbuf_lli_phys_address(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	uint32_t idx);
extern uint32_t tcc_asrc_rxbuf_lli_phys_address(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	uint32_t idx);
extern int tcc_asrc_volume_gain(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair);
extern int tcc_asrc_volume_ramp(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair);

extern int tcc_asrc_tx_dma_start(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair);
extern int tcc_asrc_tx_dma_stop(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair);
extern int tcc_asrc_tx_dma_halt(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair);
extern int tcc_asrc_tx_fifo_enable(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	bool enable);

extern int tcc_asrc_rx_dma_start(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair);
extern int tcc_asrc_rx_dma_stop(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair);
extern int tcc_asrc_rx_dma_halt(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair);
extern int tcc_asrc_rx_fifo_enable(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	bool enable);

extern int tcc_asrc_m2m_sync_setup(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	const struct asrc_fifo_type_t *tx_fifo_type,
	const struct asrc_fifo_type_t *rx_fifo_type,
	uint32_t ratio_shift22);

extern int tcc_asrc_set_m2p_mux_select(
	struct tcc_asrc_t *asrc,
	uint32_t peri_target,
	uint32_t asrc_pair);
extern int tcc_asrc_m2p_setup(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	const struct tcc_asrc_hw_param_t *tcc_asrc_hw_param
	);

extern int tcc_asrc_p2m_setup(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	const struct tcc_asrc_hw_param_t *tcc_asrc_hw_param);

extern int tcc_asrc_stop(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair);
extern struct tcc_asrc_t *tcc_asrc_get_handle_by_node(struct device_node *np);

static inline enum tcc_asrc_clksel_t tcc_asrc_get_aux_sel(uint32_t asrc_pair){
	enum tcc_asrc_clksel_t aux_sel = (asrc_pair == 0U) ? TCC_ASRC_CLKSEL_AUXCLK0 :
		    (asrc_pair == 1U) ? TCC_ASRC_CLKSEL_AUXCLK1 :
		    (asrc_pair == 2U) ? TCC_ASRC_CLKSEL_AUXCLK2 :
			TCC_ASRC_CLKSEL_AUXCLK3;
	return aux_sel;
}

static inline enum tcc_asrc_fifo_mode_t tcc_asrc_get_fifo_mode(enum tcc_asrc_drv_ch_t channels){
	enum tcc_asrc_fifo_mode_t fifo_mode;
	fifo_mode = (channels == TCC_ASRC_NUM_OF_CH_8) ? TCC_ASRC_FIFO_MODE_8CH :
		(channels == TCC_ASRC_NUM_OF_CH_6) ? TCC_ASRC_FIFO_MODE_6CH :
		(channels == TCC_ASRC_NUM_OF_CH_4) ? TCC_ASRC_FIFO_MODE_4CH :
		TCC_ASRC_FIFO_MODE_2CH;
	return fifo_mode;
}

static inline enum tcc_asrc_ip_route_t tcc_asrc_get_ip_route(enum tcc_asrc_peri_t peri_dai){
	enum tcc_asrc_ip_route_t route;
	route =
		(peri_dai == TCC_ASRC_PERI_DAI0) ? TCC_ASRC_IP_ROUTE_MCAUDIO0_10 :
		(peri_dai == TCC_ASRC_PERI_DAI1) ? TCC_ASRC_IP_ROUTE_MCAUDIO1 :
		(peri_dai == TCC_ASRC_PERI_DAI2) ? TCC_ASRC_IP_ROUTE_MCAUDIO2 :
		TCC_ASRC_IP_ROUTE_MCAUDIO3;
	return route;
}

static inline enum tcc_asrc_clksel_t tcc_asrc_get_clksel_with_dai(enum tcc_asrc_peri_t peri_dai){
	enum tcc_asrc_clksel_t clksel =
	(peri_dai == TCC_ASRC_PERI_DAI0) ? TCC_ASRC_CLKSEL_MCAUDIO0_LRCK :
	(peri_dai == TCC_ASRC_PERI_DAI1) ? TCC_ASRC_CLKSEL_MCAUDIO1_LRCK :
	(peri_dai == TCC_ASRC_PERI_DAI2) ? TCC_ASRC_CLKSEL_MCAUDIO2_LRCK :
	TCC_ASRC_CLKSEL_MCAUDIO3_LRCK;
	return clksel;
}

static inline enum tcc_asrc_clksel_t tcc_asrc_get_clksel_with_async_refclk(enum tcc_asrc_async_refclk_t async_refclk, enum tcc_asrc_clksel_t aux_sel){
	enum tcc_asrc_clksel_t clksel =
	(async_refclk == TCC_ASRC_ASYNC_REFCLK_DAI0) ?
	TCC_ASRC_CLKSEL_MCAUDIO0_LRCK :
	(async_refclk == TCC_ASRC_ASYNC_REFCLK_DAI1) ?
	TCC_ASRC_CLKSEL_MCAUDIO1_LRCK :
	(async_refclk == TCC_ASRC_ASYNC_REFCLK_DAI2) ?
	TCC_ASRC_CLKSEL_MCAUDIO2_LRCK :
	(async_refclk == TCC_ASRC_ASYNC_REFCLK_DAI3) ?
	TCC_ASRC_CLKSEL_MCAUDIO3_LRCK : aux_sel;
	return clksel;
}

#endif /*TCC_ASRC_DRV_H*/
