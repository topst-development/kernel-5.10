// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
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
#include <asm/div64.h>

#include <linux/timekeeping.h>

#include "tcc_asrc_drv.h"
#include "tcc_asrc_dai.h"
#include "tcc_asrc_pcm.h"
#include "tcc_asrc_m2m.h"
#include "tcc_asrc.h"
#include "tcc_pl080.h"

#define unused(x) (void)(x)
#undef asrc_drv_dbg
#if 0
#define asrc_drv_dbg(a...)\
	(void) pr_info("[DEBUG][ASRC_DRV] " a)
#else
#define asrc_drv_dbg(a...)
#endif
#define asrc_drv_err(a...)\
        (void) pr_err("[ERROR][ASRC_DRV] " a)

#define DEFAULT_PERI_DAI_RATE\
	(48000)
#define DEFAULT_PERI_DAI_FORMAT\
	(SNDRV_PCM_FORMAT_S16_LE)
#define DEFAULT_PERI_DAI\
	(TCC_ASRC_PERI_DAI0)

struct tcc_asrc_t *tcc_asrc_get_handle_by_node(struct device_node *np)
{
	const struct platform_device *pdev;
	struct tcc_asrc_t *ret = NULL;

	pdev = of_find_device_by_node(np);
	if (pdev == NULL) {
		ret = NULL;
	} else {
		ret = platform_get_drvdata(pdev);
	}

	return ret;
}
EXPORT_SYMBOL(tcc_asrc_get_handle_by_node);

uint32_t tcc_asrc_txbuf_lli_phys_address(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	uint32_t idx)
{
	return ui_add(ull_to_ui(asrc->pair[asrc_pair].txbuf.lli_phys),
			ui_to_ui_mul(idx, ul_to_ui(sizeof(struct pl080_lli))));
}

uint32_t tcc_asrc_rxbuf_lli_phys_address(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	uint32_t idx)
{
	return ui_add(ull_to_ui(asrc->pair[asrc_pair].rxbuf.lli_phys),
			ui_to_ui_mul(idx, ul_to_ui(sizeof(struct pl080_lli))));
}

int tcc_asrc_volume_gain(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair)
{
	tcc_asrc_set_volume_gain(
		asrc->asrc_reg, asrc_pair,
		asrc->pair[asrc_pair].volume_gain);
	tcc_asrc_volume_enable(asrc->asrc_reg, asrc_pair, (bool)true);

	return 0;
}

int tcc_asrc_volume_ramp(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair)
{
	void __iomem *asrc_reg = asrc->asrc_reg;
	uint32_t gain = asrc->pair[asrc_pair].volume_ramp.gain;
	uint32_t dn_time = asrc->pair[asrc_pair].volume_ramp.dn_time;
	uint32_t dn_wait = asrc->pair[asrc_pair].volume_ramp.dn_wait;
	uint32_t up_time = asrc->pair[asrc_pair].volume_ramp.up_time;
	uint32_t up_wait = asrc->pair[asrc_pair].volume_ramp.up_wait;

	tcc_asrc_set_volume_ramp_dn_time(asrc_reg, asrc_pair, dn_time, dn_wait);
	tcc_asrc_set_volume_ramp_up_time(asrc_reg, asrc_pair, up_time, up_wait);
	tcc_asrc_set_volume_ramp_gain(asrc_reg, asrc_pair, gain);	// 0 dB
	tcc_asrc_volume_ramp_enable(asrc_reg, asrc_pair, (bool)true);

	return 0;
}

int tcc_asrc_tx_dma_start(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair)
{
	uint32_t dma_tx_ch = asrc_pair;
	int ret = 0;

	if (asrc_pair >= NUM_OF_ASRC_PAIR) {
		ret = -1;
	} else {
		tcc_pl080_set_first_lli(
			asrc->pl080_reg, dma_tx_ch,
			&asrc->pair[asrc_pair].txbuf.lli_virt[0]);
		tcc_pl080_set_channel_mem2per(
			asrc->pl080_reg,
			dma_tx_ch,
			(enum tcc_peri_id_t)dma_tx_ch,
			(bool)true,
			(bool)true);
		tcc_pl080_channel_enable(asrc->pl080_reg, dma_tx_ch, (bool)true);
		tcc_pl080_channel_sync_mode(asrc->pl080_reg, dma_tx_ch, (bool)true);
	}

	return ret;
}

int tcc_asrc_tx_dma_stop(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair)
{
	uint32_t dma_tx_ch = asrc_pair;
	int ret = 0;

	if (asrc_pair > NUM_OF_ASRC_PAIR) {
		ret = -1;
	} else {
		tcc_pl080_channel_enable(asrc->pl080_reg, dma_tx_ch, (bool)false);
		ret = 0;
	}

	return ret;
}

int tcc_asrc_tx_dma_halt(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair)
{
	uint32_t dma_tx_ch = asrc_pair;
	int ret = 0;

	if (asrc_pair > NUM_OF_ASRC_PAIR) {
		ret = -1;
	} else {
		tcc_pl080_halt_enable(asrc->pl080_reg, dma_tx_ch, (bool)true);
		ret = 0;
	}

	return ret;
}

int tcc_asrc_tx_fifo_enable(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	bool enable)
{
	int ret = 0;

	if (asrc_pair > NUM_OF_ASRC_PAIR) {
		ret = -1;
	} else {
		tcc_asrc_fifo_in_dma_en(asrc->asrc_reg, asrc_pair, enable);
		ret = 0;
	}

	return ret;
}

int tcc_asrc_rx_dma_start(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair)
{
	uint32_t dma_rx_ch = ui_add(asrc_pair, ASRC_RX_DMA_OFFSET);
	int ret = 0;

	if (asrc_pair >= NUM_OF_ASRC_PAIR) {
		ret = -1;
	} else {
		tcc_pl080_set_first_lli(
			asrc->pl080_reg, dma_rx_ch,
			&asrc->pair[asrc_pair].rxbuf.lli_virt[0]);
		tcc_pl080_set_channel_per2mem(
			asrc->pl080_reg,
			dma_rx_ch,
			ui_to_enum_tcc_peri_id_t(dma_rx_ch),
			(bool)true,
			(bool)true);
		tcc_pl080_channel_enable(asrc->pl080_reg, dma_rx_ch, (bool)true);
		tcc_pl080_channel_sync_mode(asrc->pl080_reg, dma_rx_ch, (bool)true);

		ret = 0;
	}

	return ret;
}

int tcc_asrc_rx_dma_stop(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair)
{
	uint32_t dma_rx_ch = ui_add(asrc_pair, ASRC_RX_DMA_OFFSET);
	int ret = 0;

	if (asrc_pair > NUM_OF_ASRC_PAIR) {
		ret = -1;
	} else {
		tcc_pl080_channel_enable(asrc->pl080_reg, dma_rx_ch, (bool)false);
		ret = 0;
	}

	return ret;
}

int tcc_asrc_rx_dma_halt(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair)
{
	uint32_t dma_rx_ch = ui_add(asrc_pair, ASRC_RX_DMA_OFFSET);
	int ret = 0;

	if (asrc_pair > NUM_OF_ASRC_PAIR) {
		ret = -1;
	} else {
		tcc_pl080_halt_enable(asrc->pl080_reg, dma_rx_ch, (bool)true);
		ret = 0;
	}

	return ret;
}

int tcc_asrc_rx_fifo_enable(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	bool enable)
{
	int ret = 0;
	if (asrc_pair > NUM_OF_ASRC_PAIR) {
		ret = -1;
	} else {
		tcc_asrc_fifo_out_dma_en(asrc->asrc_reg, asrc_pair, enable);
		ret = 0;
	}

	return ret;
}

// M2M SYNC
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
static void tcc_sub_asrc_m2m_sync_setup(
	void __iomem *asrc_reg,
	uint32_t asrc_pair,
	const struct asrc_fifo_type_t *tx_fifo_type,
	const struct asrc_fifo_type_t *rx_fifo_type,
	uint32_t ratio_shift22,
	enum tcc_asrc_drv_fifo_size_t fifo_size)
#else
static void tcc_sub_asrc_m2m_sync_setup(
	void __iomem *asrc_reg,
	uint32_t asrc_pair,
	const struct asrc_fifo_type_t *tx_fifo_type,
	const struct asrc_fifo_type_t *rx_fifo_type,
	uint32_t ratio_shift22)
#endif
{
	enum tcc_asrc_component_t comp_asrc;
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	enum tcc_asrc_fifo_in_size_t fifo_in_size;
#endif//defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	comp_asrc = (asrc_pair == 0U) ? TCC_ASRC0
		: (asrc_pair == 1U) ? TCC_ASRC1
		: (asrc_pair == 2U) ? TCC_ASRC2
		: TCC_ASRC3;

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	fifo_in_size =
	(fifo_size == TCC_ASRC_FIFO_SIZE_256WORD) ?
				TCC_ASRC_FIFO_IN_SIZE_256WORD :
	(fifo_size == TCC_ASRC_FIFO_SIZE_128WORD) ?
				TCC_ASRC_FIFO_IN_SIZE_128WORD :
	(fifo_size == TCC_ASRC_FIFO_SIZE_64WORD) ?
				TCC_ASRC_FIFO_IN_SIZE_64WORD :
	(fifo_size == TCC_ASRC_FIFO_SIZE_32WORD) ?
				TCC_ASRC_FIFO_IN_SIZE_32WORD :
	(fifo_size == TCC_ASRC_FIFO_SIZE_16WORD) ?
				TCC_ASRC_FIFO_IN_SIZE_16WORD :
	(fifo_size == TCC_ASRC_FIFO_SIZE_8WORD) ?
				TCC_ASRC_FIFO_IN_SIZE_8WORD :
	(fifo_size == TCC_ASRC_FIFO_SIZE_4WORD) ?
				TCC_ASRC_FIFO_IN_SIZE_4WORD :
				TCC_ASRC_FIFO_IN_SIZE_2WORD;
#endif

	tcc_asrc_set_inport_path(asrc_reg, asrc_pair, TCC_ASRC_PATH_DMA);
	tcc_asrc_set_outport_path(asrc_reg, asrc_pair, TCC_ASRC_PATH_DMA);

	tcc_asrc_set_zero_init_val(asrc_reg, asrc_pair, ratio_shift22);
	tcc_asrc_set_ratio(
		asrc_reg,
		asrc_pair,
		TCC_ASRC_MODE_SYNC,
		ratio_shift22);
	tcc_asrc_component_reset(asrc_reg, comp_asrc);

	tcc_asrc_set_opt_buf_lvl(asrc_reg, asrc_pair, 0x10);
	tcc_asrc_set_period_sync_cnt(asrc_reg, asrc_pair, 0x1f);

	tcc_asrc_set_inport_timing(
		asrc_reg,
		asrc_pair,
		IP_OP_TIMING_ASRC_REQUEST);
	tcc_asrc_set_outport_timing(
		asrc_reg,
		asrc_pair,
		IP_OP_TIMING_ASRC_REQUEST);

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	tcc_asrc_fifo_in_config(
		asrc_reg,
		asrc_pair,
		tx_fifo_type->fifo_fmt,
		tx_fifo_type->fifo_mode,
		fifo_in_size,
		0);
#else
	tcc_asrc_fifo_in_config(asrc_reg, asrc_pair, tx_fifo_type->fifo_fmt, tx_fifo_type->fifo_mode, 0);
#endif//defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	tcc_asrc_fifo_out_config(asrc_reg, asrc_pair, rx_fifo_type->fifo_fmt, rx_fifo_type->fifo_mode, 0);

	tcc_asrc_component_enable(asrc_reg, comp_asrc, (bool)true);
	tcc_asrc_component_enable(asrc_reg, TCC_INPORT, (bool)true);
	tcc_asrc_component_enable(asrc_reg, TCC_OUTPORT, (bool)true);

	tcc_asrc_fifo_in_dma_en(asrc_reg, asrc_pair, (bool)true);
	tcc_asrc_fifo_out_dma_en(asrc_reg, asrc_pair, (bool)true);
}

int tcc_asrc_m2m_sync_setup(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	const struct asrc_fifo_type_t *tx_fifo_type,
	const struct asrc_fifo_type_t *rx_fifo_type,
	uint32_t ratio_shift22)
{
	uint32_t dma_rx_ch = ui_add(asrc_pair, ASRC_RX_DMA_OFFSET);

	//disable rx dma channel
	tcc_pl080_channel_enable(asrc->pl080_reg, dma_rx_ch, (bool)false);

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	tcc_sub_asrc_m2m_sync_setup(
		asrc->asrc_reg,
		asrc_pair,
		tx_fifo_type,
		rx_fifo_type,
		ratio_shift22,
		asrc->pair[asrc_pair].hw.fifo_in_size);
#else
	tcc_sub_asrc_m2m_sync_setup(
		asrc->asrc_reg,
		asrc_pair,
		tx_fifo_type,
		rx_fifo_type,
		ratio_shift22);
#endif

	(void)tcc_asrc_volume_ramp(asrc, asrc_pair);
	(void)tcc_asrc_volume_gain(asrc, asrc_pair);

	return 0;
}

static int tcc_asrc_check_supported_channel(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	enum tcc_asrc_drv_ch_t channels)
{
	enum tcc_asrc_drv_ch_t max_channel;
	int ret = 0;

	max_channel =
	    (asrc->pair[asrc_pair].hw.max_channel == 2U) ? TCC_ASRC_NUM_OF_CH_2 :
	    (asrc->pair[asrc_pair].hw.max_channel == 4U) ? TCC_ASRC_NUM_OF_CH_4 :
	    (asrc->pair[asrc_pair].hw.max_channel == 6U) ? TCC_ASRC_NUM_OF_CH_6 :
		TCC_ASRC_NUM_OF_CH_8;

	if (max_channel < channels) {
		ret = -1;
	} else {
		ret = 0;
	}


	return ret;
}

// M2P
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
static void tcc_sub_asrc_m2p_setup(void __iomem *asrc_reg,
	uint32_t asrc_pair,
	const struct tcc_asrc_hw_param_t *tcc_asrc_hw_param,
	uint32_t ratio_shift22,
	enum tcc_asrc_drv_fifo_size_t fifo_size)
#else
static void tcc_sub_asrc_m2p_setup(
	void __iomem *asrc_reg,
	uint32_t asrc_pair,
	const struct tcc_asrc_hw_param_t *tcc_asrc_hw_param,
	uint32_t ratio_shift22)
#endif
{
	enum tcc_asrc_mode_t asrc_mode;
	enum tcc_asrc_component_t comp_asrc;
//      enum tcc_asrc_op_route_t op_route;
	enum tcc_asrc_clksel_t op_clksel, aux_sel, ip_clksel;
	enum tcc_asrc_fifo_fmt_t fifo_fmt;
	enum tcc_asrc_fifo_mode_t fifo_mode;
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	enum tcc_asrc_fifo_in_size_t fifo_in_size;
#endif//defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	asrc_mode =
	    (tcc_asrc_hw_param->sync_mode == TCC_ASRC_ASYNC_MODE) ? TCC_ASRC_MODE_ASYNC :
		TCC_ASRC_MODE_SYNC;

	comp_asrc = (asrc_pair == 0U) ? TCC_ASRC0 :
	    (asrc_pair == 1U) ? TCC_ASRC1 :
	    (asrc_pair == 2U) ? TCC_ASRC2 :
		TCC_ASRC3;

	aux_sel = tcc_asrc_get_aux_sel(asrc_pair);

	ip_clksel = tcc_asrc_get_clksel_with_async_refclk(tcc_asrc_hw_param->async_refclk, aux_sel);

	//op_route = (asrc_pair == 0U) ? TCC_ASRC_OP_ROUTE_ASRC_PAIR0_10:
	//(asrc_pair == 1U) ? TCC_ASRC_OP_ROUTE_ASRC_PAIR1 :
	//(asrc_pair == 2U) ? TCC_ASRC_OP_ROUTE_ASRC_PAIR2 :
	//TCC_ASRC_OP_ROUTE_ASRC_PAIR3;

	op_clksel = tcc_asrc_get_clksel_with_dai(tcc_asrc_hw_param->peri_dai);

	fifo_fmt = (tcc_asrc_hw_param->bitwidth == TCC_ASRC_16BIT) ? TCC_ASRC_FIFO_FMT_16BIT :
	    TCC_ASRC_FIFO_FMT_24BIT;

	fifo_mode = tcc_asrc_get_fifo_mode(tcc_asrc_hw_param->channels);

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	fifo_in_size =
		(fifo_size == TCC_ASRC_FIFO_SIZE_256WORD) ?
					TCC_ASRC_FIFO_IN_SIZE_256WORD :
		(fifo_size == TCC_ASRC_FIFO_SIZE_128WORD) ?
					TCC_ASRC_FIFO_IN_SIZE_128WORD :
		(fifo_size == TCC_ASRC_FIFO_SIZE_64WORD) ?
					TCC_ASRC_FIFO_IN_SIZE_64WORD :
		(fifo_size == TCC_ASRC_FIFO_SIZE_32WORD) ?
					TCC_ASRC_FIFO_IN_SIZE_32WORD :
		(fifo_size == TCC_ASRC_FIFO_SIZE_16WORD) ?
					TCC_ASRC_FIFO_IN_SIZE_16WORD :
		(fifo_size == TCC_ASRC_FIFO_SIZE_8WORD) ?
					TCC_ASRC_FIFO_IN_SIZE_8WORD :
		(fifo_size == TCC_ASRC_FIFO_SIZE_4WORD) ?
					TCC_ASRC_FIFO_IN_SIZE_4WORD :
					TCC_ASRC_FIFO_IN_SIZE_2WORD;
#endif

	tcc_asrc_set_inport_path(asrc_reg, asrc_pair, TCC_ASRC_PATH_DMA);
	tcc_asrc_set_outport_path(asrc_reg, (uint32_t)tcc_asrc_hw_param->peri_dai, TCC_ASRC_PATH_EXTIO);

	tcc_asrc_set_zero_init_val(asrc_reg, asrc_pair, ratio_shift22);
	tcc_asrc_set_ratio(asrc_reg, asrc_pair, asrc_mode, ratio_shift22);
	tcc_asrc_component_reset(asrc_reg, comp_asrc);

	tcc_asrc_set_opt_buf_lvl(asrc_reg, asrc_pair, 0x10);
	tcc_asrc_set_period_sync_cnt(asrc_reg, asrc_pair, 0x1f);

	tcc_asrc_set_inport_timing(asrc_reg, asrc_pair,
				   IP_OP_TIMING_ASRC_REQUEST);
	tcc_asrc_set_outport_timing(asrc_reg, asrc_pair,
				    IP_OP_TIMING_EXTERNEL_CLK);

	if (tcc_asrc_hw_param->sync_mode == TCC_ASRC_ASYNC_MODE) {
		tcc_asrc_set_inport_clksel(asrc_reg, asrc_pair, ip_clksel);
	}

	tcc_asrc_set_outport_clksel(asrc_reg, asrc_pair, op_clksel);
	//tcc_asrc_set_outport_route(asrc_reg, peri_target, op_route);
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	tcc_asrc_fifo_in_config(
		asrc_reg,
		asrc_pair,
		fifo_fmt,
		fifo_mode,
		fifo_in_size,
		0);
#else
	tcc_asrc_fifo_in_config(asrc_reg, asrc_pair, fifo_fmt, fifo_mode, 0);
#endif//defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)

	tcc_asrc_set_outport_format(
		asrc_reg,
		(uint32_t)tcc_asrc_hw_param->peri_dai,
		TCC_ASRC_FMT_NO_CHANGE,
		(bool)false);

	tcc_asrc_component_enable(asrc_reg, comp_asrc, (bool)true);
	tcc_asrc_component_enable(asrc_reg, TCC_INPORT, (bool)true);
	tcc_asrc_component_enable(asrc_reg, TCC_OUTPORT, (bool)true);
	tcc_asrc_component_enable(asrc_reg, TCC_EXTIO, (bool)true);
}

int tcc_asrc_m2p_setup(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	const struct tcc_asrc_hw_param_t *tcc_asrc_hw_param)
{
	uint64_t ratio_shift22 = ui_lshift(1U, 22);
	int ret = 0;

	if (tcc_asrc_check_supported_channel(asrc, asrc_pair, tcc_asrc_hw_param->channels) < 0) {
		asrc_drv_err("%s - asrc_pair %d supports",
			__func__, asrc_pair);
		asrc_drv_err("max channel(%u)\n",
			asrc->pair[asrc_pair].hw.max_channel);
		ret = -1;
	} else {

		ratio_shift22 = ui_to_ull_mul(0x400000U, tcc_asrc_hw_param->dst_rate);
		ratio_shift22 = div64_ul(ratio_shift22, tcc_asrc_hw_param->src_rate);

		//set aux pclk
		if ((tcc_asrc_hw_param->sync_mode == TCC_ASRC_ASYNC_MODE) &&
			(tcc_asrc_hw_param->async_refclk == TCC_ASRC_ASYNC_REFCLK_AUX)) {
			(void)clk_set_rate(asrc->aux_pclk[asrc_pair], tcc_asrc_hw_param->src_rate);
			(void)clk_prepare_enable(asrc->aux_pclk[asrc_pair]);
		}

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
		tcc_sub_asrc_m2p_setup(
			asrc->asrc_reg,
			asrc_pair,
			tcc_asrc_hw_param,
			ull_to_ui(ratio_shift22),
			asrc->pair[asrc_pair].hw.fifo_in_size);
#else
		tcc_sub_asrc_m2p_setup(
			asrc->asrc_reg,
			asrc_pair,
			tcc_asrc_hw_param,
			ull_to_ui(ratio_shift22));
#endif

		(void)tcc_asrc_volume_ramp(asrc, asrc_pair);
		(void)tcc_asrc_volume_gain(asrc, asrc_pair);

		ret = 0;
	}

	return ret;
}

int tcc_asrc_set_m2p_mux_select(
	struct tcc_asrc_t *asrc,
	uint32_t peri_target,
	uint32_t asrc_pair)
{
	enum tcc_asrc_op_route_t op_route;
	int ret = 0;

	if (asrc_pair >= NUM_OF_ASRC_PAIR) {
		ret = -EINVAL;
	} else if (peri_target >= NUM_OF_ASRC_MCAUDIO) {
		ret = -EINVAL;
	} else {
		asrc->mcaudio_m2p_mux[peri_target] = ui_to_si(asrc_pair);
		op_route =
			(asrc_pair == 0U) ? TCC_ASRC_OP_ROUTE_ASRC_PAIR0_10 :
		    (asrc_pair == 1U) ? TCC_ASRC_OP_ROUTE_ASRC_PAIR1 :
		    (asrc_pair == 2U) ? TCC_ASRC_OP_ROUTE_ASRC_PAIR2 :
			TCC_ASRC_OP_ROUTE_ASRC_PAIR3;

		tcc_asrc_set_outport_route(asrc->asrc_reg, peri_target, op_route);
	}

	return ret;
}

// P2M
static void tcc_sub_asrc_p2m_setup(
	void __iomem *asrc_reg,
	uint32_t asrc_pair,
	const struct tcc_asrc_hw_param_t *tcc_asrc_hw_param,
	uint32_t ratio_shift22)
{
	enum tcc_asrc_mode_t asrc_mode;
	enum tcc_asrc_component_t comp_asrc;
	enum tcc_asrc_ip_route_t ip_route;
	enum tcc_asrc_clksel_t aux_sel, ip_clksel, op_clksel;
	enum tcc_asrc_fifo_fmt_t fifo_fmt;
	enum tcc_asrc_fifo_mode_t fifo_mode;

	asrc_mode =
		(tcc_asrc_hw_param->sync_mode == TCC_ASRC_ASYNC_MODE) ? TCC_ASRC_MODE_ASYNC :
		TCC_ASRC_MODE_SYNC;

	comp_asrc =
		(asrc_pair == 0U) ? TCC_ASRC0 :
	    (asrc_pair == 1U) ? TCC_ASRC1 :
	    (asrc_pair == 2U) ? TCC_ASRC2 :
		TCC_ASRC3;

	aux_sel = tcc_asrc_get_aux_sel(asrc_pair);

	op_clksel = tcc_asrc_get_clksel_with_async_refclk(tcc_asrc_hw_param->async_refclk, aux_sel);

	ip_route = tcc_asrc_get_ip_route(tcc_asrc_hw_param->peri_dai);

	ip_clksel = tcc_asrc_get_clksel_with_dai(tcc_asrc_hw_param->peri_dai);

	fifo_fmt = (tcc_asrc_hw_param->bitwidth == TCC_ASRC_16BIT) ? TCC_ASRC_FIFO_FMT_16BIT :
		TCC_ASRC_FIFO_FMT_24BIT;

	fifo_mode = tcc_asrc_get_fifo_mode(tcc_asrc_hw_param->channels);

	tcc_asrc_set_inport_path(asrc_reg, asrc_pair, TCC_ASRC_PATH_EXTIO);
	tcc_asrc_set_outport_path(asrc_reg, asrc_pair, TCC_ASRC_PATH_DMA);

	tcc_asrc_set_zero_init_val(asrc_reg, asrc_pair, ratio_shift22);
	tcc_asrc_set_ratio(asrc_reg, asrc_pair, asrc_mode, ratio_shift22);
	tcc_asrc_component_reset(asrc_reg, comp_asrc);

	tcc_asrc_set_opt_buf_lvl(asrc_reg, asrc_pair, 0x10);
	tcc_asrc_set_period_sync_cnt(asrc_reg, asrc_pair, 0x1f);

	tcc_asrc_set_inport_timing(
		asrc_reg,
		asrc_pair,
		IP_OP_TIMING_EXTERNEL_CLK);
	tcc_asrc_set_outport_timing(
		asrc_reg,
		asrc_pair,
		IP_OP_TIMING_ASRC_REQUEST);

	if (tcc_asrc_hw_param->sync_mode == TCC_ASRC_ASYNC_MODE) {
		tcc_asrc_set_outport_clksel(asrc_reg, asrc_pair, op_clksel);
	}

	tcc_asrc_set_inport_clksel(asrc_reg, asrc_pair, ip_clksel);
	tcc_asrc_set_inport_route(asrc_reg, asrc_pair, ip_route);

	tcc_asrc_fifo_out_config(asrc_reg, asrc_pair, fifo_fmt, fifo_mode, 0);

	if (tcc_asrc_hw_param->peri_dai_bitwidth == TCC_ASRC_16BIT) {
		tcc_asrc_set_inport_format(
			asrc_reg,
			(uint32_t)tcc_asrc_hw_param->peri_dai,
			TCC_ASRC_16BIT_LEFT_8BIT,
			(bool)true);
	} else {
		tcc_asrc_set_inport_format(
			asrc_reg,
			(uint32_t)tcc_asrc_hw_param->peri_dai,
			TCC_ASRC_FMT_NO_CHANGE, (bool)true);
	}

	tcc_asrc_component_enable(asrc_reg, comp_asrc, (bool)true);
	tcc_asrc_component_enable(asrc_reg, TCC_INPORT, (bool)true);
	tcc_asrc_component_enable(asrc_reg, TCC_OUTPORT, (bool)true);
	tcc_asrc_component_enable(asrc_reg, TCC_EXTIO, (bool)true);
}

int tcc_asrc_p2m_setup(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	const struct tcc_asrc_hw_param_t *tcc_asrc_hw_param)
{
	uint64_t ratio_shift22 = ui_lshift(1U,22);
	int ret = 0;

	if (tcc_asrc_check_supported_channel(asrc, asrc_pair, tcc_asrc_hw_param->channels) < 0) {
		asrc_drv_err("[ERROR][ASRC_DRV] %s - asrc_pair %d supports max",
			 __func__, asrc_pair);
		asrc_drv_err("channel(%u)\n",
		     asrc->pair[asrc_pair].hw.max_channel);
		ret = -1;
	} else {

		ratio_shift22 = ui_to_ull_mul(0x400000U, tcc_asrc_hw_param->dst_rate);
		ratio_shift22 = div64_ul(ratio_shift22, tcc_asrc_hw_param->src_rate);

		//set aux pclk
		if ((tcc_asrc_hw_param->sync_mode == TCC_ASRC_ASYNC_MODE) &&
			(tcc_asrc_hw_param->async_refclk == TCC_ASRC_ASYNC_REFCLK_AUX)) {
			(void)clk_set_rate(asrc->aux_pclk[asrc_pair], tcc_asrc_hw_param->dst_rate);
			(void)clk_prepare_enable(asrc->aux_pclk[asrc_pair]);
		}

		tcc_sub_asrc_p2m_setup(
			asrc->asrc_reg,
			asrc_pair,
			tcc_asrc_hw_param,
			ull_to_ui(ratio_shift22));

		(void)tcc_asrc_volume_ramp(asrc, asrc_pair);
		(void)tcc_asrc_volume_gain(asrc, asrc_pair);
	}

	return ret;
}

int tcc_asrc_stop(const struct tcc_asrc_t *asrc, uint32_t asrc_pair)
{
	enum tcc_asrc_component_t comp_asrc;

	comp_asrc =
		(asrc_pair == 0U) ? TCC_ASRC0 :
	    (asrc_pair == 1U) ? TCC_ASRC1 :
	    (asrc_pair == 2U) ? TCC_ASRC2 :
		TCC_ASRC3;

	(void)tcc_asrc_rx_dma_stop(asrc, asrc_pair);
	(void)tcc_asrc_tx_dma_stop(asrc, asrc_pair);

	tcc_asrc_fifo_in_dma_en(asrc->asrc_reg, asrc_pair, (bool)false);
	tcc_asrc_fifo_out_dma_en(asrc->asrc_reg, asrc_pair, (bool)false);

	tcc_asrc_component_enable(asrc->asrc_reg, comp_asrc, (bool)false);
	tcc_asrc_component_reset(asrc->asrc_reg, comp_asrc);

	return 0;
}

static irqreturn_t tcc_pl080_isr(int irq, void *dev)
{
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t *)dev;
	uint32_t int_status = readl(asrc->pl080_reg + PL080_INT_STATUS);
	uint32_t i;

        unused(irq);
	asrc_drv_dbg("%s(0x%08x)\n", __func__, int_status);
	for (i = 0; i < NUM_OF_ASRC_PAIR; i++) {
		if ((int_status & ui_lshift(1U, i)) != 0U) {
			writel(ui_lshift(1U, i), asrc->pl080_reg + PL080_TC_CLEAR);
			writel(ui_lshift(1U, i), asrc->pl080_reg + PL080_ERR_CLEAR);
			switch (asrc->pair[i].hw.asrc_path) {
#ifdef ASRC_M2M_INTERRUPT_MODE
			case TCC_ASRC_M2P_PATH:
				(void)tcc_pl080_asrc_pcm_isr_ch(asrc, i);
				break;
#else
			case TCC_ASRC_M2M_PATH:
				(void)tcc_pl080_asrc_m2m_txisr_ch(asrc, i);
				break;
			case TCC_ASRC_M2P_PATH:
				(void)tcc_pl080_asrc_pcm_isr_ch(asrc, i);
				break;
#endif
			default:
			        asrc_drv_err("Not supported asrc path!!\n");
                                break;
			}

		}
	}

	for (i = 0; i < NUM_OF_ASRC_PAIR; i++) {
		if ((int_status & ui_lshift(1U, (i + NUM_OF_ASRC_PAIR))) != 0U) {
			writel(ui_lshift(1U,(i + NUM_OF_ASRC_PAIR)),
			       asrc->pl080_reg + PL080_TC_CLEAR);
			writel(ui_lshift(1U, (i + NUM_OF_ASRC_PAIR)),
			       asrc->pl080_reg + PL080_ERR_CLEAR);

			if (asrc->pair[i].hw.asrc_path == TCC_ASRC_P2M_PATH) {
				(void)tcc_pl080_asrc_pcm_isr_ch(asrc, i);
			}

		}
	}

	return IRQ_HANDLED;
}

#ifdef ASRC_M2M_INTERRUPT_MODE
static irqreturn_t tcc_asrc_isr(int irq, void *dev)
{
	struct tcc_asrc_t *asrc = (struct tcc_asrc_t *)dev;
	uint32_t int_status =
			readl(asrc->asrc_reg+TCC_ASRC_IRQ_RAW_STATUS1_OFFSET);
	uint32_t i;

	asrc_drv_dbg(" %s(0x%08x)\n", __func__, int_status);
	for (i = 0; i < NUM_OF_ASRC_PAIR; i++) {
		if (int_status & (1<<i)) {
			if (asrc->pair[i].hw.asrc_path == TCC_ASRC_M2M_PATH) {
				tcc_asrc_m2m_txisr_ch(asrc, i);
			}
		}
	}

	return IRQ_HANDLED;
}
#endif


static int parse_asrc_dt_sub(const struct platform_device *pdev, struct tcc_asrc_t *asrc){
	int ret = 0;
	struct device_node *of_node_asrc = pdev->dev.of_node;
	struct device_node *of_node_dma = NULL;
	struct platform_device *pdev_asrc = NULL;
	struct platform_device *pdev_dma = NULL;
	struct resource res;
	int prop;
	uint32_t i;

	of_node_dma = of_parse_phandle(pdev->dev.of_node, "dma", 0);

	do{
		if (of_node_dma == NULL) {
			asrc_drv_err("of_node_dma is NULL\n");
			ret = -EINVAL;
			continue;
		}

		pdev_asrc = of_find_device_by_node(of_node_asrc);
		pdev_dma = of_find_device_by_node(of_node_dma);

		asrc->asrc_reg = of_iomap(of_node_asrc, 0);
		if (asrc->asrc_reg == NULL) {
			asrc_drv_err("asrc_reg is NULL\n");
			ret = -EINVAL;
			continue;
		}

		if (of_address_to_resource(of_node_asrc, 0, &res) < 0) {
			asrc_drv_err("asrc_reg_phys is error\n");
			ret = -EINVAL;
			continue;
		}
		asrc->asrc_reg_phys = ull_to_ui(res.start);

		asrc->pl080_reg = of_iomap(of_node_dma, 0);
		if (asrc->pl080_reg == NULL) {
			asrc_drv_err("pl080_reg is NULL\n");
			ret = -EINVAL;
			continue;
		}

		for (i = 0; i < (unsigned)NUM_OF_AUX_PERI_CLKS; i++) {
			asrc->aux_pclk[i] = of_clk_get(of_node_asrc, ui_to_si(i));
			if (IS_ERR(asrc->aux_pclk[i])) {
				asrc_drv_err("aux%d_pclk is invalid\n", i);
				ret = -EINVAL;
				continue;
			}
		}

		asrc->asrc_hclk = of_clk_get_by_name(of_node_asrc, "asrc_hclk");
		if (IS_ERR(asrc->asrc_hclk)) {
			asrc_drv_err("asrc_hclk is invalid\n");
			ret = -EINVAL;
			continue;
		}

		asrc->pl080_hclk = of_clk_get_by_name(of_node_dma, "pl080_hclk");
		if (IS_ERR(asrc->pl080_hclk)) {
			asrc_drv_err("asrc_hclk is invalid\n");
			ret = -EINVAL;
			continue;
		}

		prop = platform_get_irq(pdev_asrc, 0);
		if (prop < 0) {
			asrc_drv_err("asrc_irq is invalid\n");
			ret = prop;
			continue;
		}else {
			asrc->asrc_irq = si_to_ui(prop);
		}

		if (pdev_dma != NULL) {
			prop = platform_get_irq(pdev_dma, 0);
			asrc->pl080_irq = si_to_ui(prop);
		} else {
			prop = 0;
			asrc->pl080_irq = irq_of_parse_and_map(of_node_dma, 0);
		}
		if (prop < 0) {
			asrc_drv_err("asrc_irq is invalid\n");
			ret = prop;
			continue;
		}
		ret = 0;
	} while (false);

	return ret;
}

static int parse_asrc_dt(const struct platform_device *pdev, struct tcc_asrc_t *asrc)
{
	const struct device_node *of_node_asrc = pdev->dev.of_node;
	uint32_t max_channel[NUM_OF_ASRC_PAIR];
	uint32_t path_type[NUM_OF_ASRC_PAIR];
	uint32_t sync_mode[NUM_OF_ASRC_PAIR];
	uint32_t async_refclk[NUM_OF_ASRC_PAIR];
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
	uint32_t fifo_in_size[NUM_OF_ASRC_PAIR];
#endif
	uint32_t i;
	int ret = 0;

	do {
		asrc_drv_dbg("%s\n", __func__);

		ret = parse_asrc_dt_sub(pdev, asrc);
		if(ret < 0) {
			asrc_drv_err(" is invalid\n");
			continue;
		}

		(void)of_property_read_u32_array(
			of_node_asrc,
			"max-ch-per-pair",
			max_channel,
			NUM_OF_ASRC_PAIR);
		(void)of_property_read_u32_array(
			of_node_asrc,
			"path-type",
			path_type,
			NUM_OF_ASRC_PAIR);
		(void)of_property_read_u32_array(
			of_node_asrc,
			"sync-mode",
			sync_mode,
			NUM_OF_ASRC_PAIR);
		(void)of_property_read_u32_array(
			of_node_asrc,
			"async-refclk",
			async_refclk,
			NUM_OF_ASRC_PAIR);
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
		(void)of_property_read_u32_array(
			of_node_asrc,
			"fifo_in-size",
			fifo_in_size,
			NUM_OF_ASRC_PAIR);
#endif

		for (i = 0; i < NUM_OF_ASRC_PAIR; i++) {
			asrc->pair[i].hw.asrc_path =
			    (path_type[i] == 2U) ? TCC_ASRC_P2M_PATH :
				(path_type[i] == 1U) ? TCC_ASRC_M2P_PATH :
				TCC_ASRC_M2M_PATH;
			asrc->pair[i].hw.max_channel = max_channel[i];
			asrc->pair[i].hw.sync_mode =
			    (sync_mode[i] == 1U) ? TCC_ASRC_SYNC_MODE :
				TCC_ASRC_ASYNC_MODE;
			asrc->pair[i].hw.async_refclk =
			    (async_refclk[i] == 0U) ? TCC_ASRC_ASYNC_REFCLK_DAI0 :
				(async_refclk[i] == 1U) ? TCC_ASRC_ASYNC_REFCLK_DAI1 :
				(async_refclk[i] == 2U) ? TCC_ASRC_ASYNC_REFCLK_DAI2 :
				(async_refclk[i] == 3U) ? TCC_ASRC_ASYNC_REFCLK_DAI3 :
				TCC_ASRC_ASYNC_REFCLK_AUX;
			asrc->pair[i].hw.peri_dai = TCC_ASRC_PERI_DAI0;
			asrc->pair[i].hw.peri_dai_rate = (uint32_t)DEFAULT_PERI_DAI_RATE;
			asrc->pair[i].hw.peri_dai_format = TCC_ASRC_16BIT;
#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
			asrc->pair[i].hw.fifo_in_size =
				(fifo_in_size[i] == 256U) ? TCC_ASRC_FIFO_SIZE_256WORD :
				(fifo_in_size[i] == 128U) ? TCC_ASRC_FIFO_SIZE_128WORD :
				(fifo_in_size[i] == 64U) ? TCC_ASRC_FIFO_SIZE_64WORD :
				(fifo_in_size[i] == 32U) ? TCC_ASRC_FIFO_SIZE_32WORD :
				(fifo_in_size[i] == 16U) ? TCC_ASRC_FIFO_SIZE_16WORD :
				(fifo_in_size[i] == 8U) ? TCC_ASRC_FIFO_SIZE_8WORD :
				(fifo_in_size[i] == 4U) ? TCC_ASRC_FIFO_SIZE_4WORD :
				TCC_ASRC_FIFO_SIZE_2WORD;
#endif
		}

		for (i = 0; i < NUM_OF_ASRC_PAIR; i++) {
			asrc->pair[i].volume_ramp.gain = DEFAULT_VOLUME_RAMP_GAIN;
			asrc->pair[i].volume_ramp.dn_time = (uint32_t)DEFAULT_VOLUME_RAMP_TIME;
			asrc->pair[i].volume_ramp.dn_wait = (uint32_t)DEFAULT_VOLUME_RAMP_WAIT;
			asrc->pair[i].volume_ramp.up_time = (uint32_t)DEFAULT_VOLUME_RAMP_TIME;
			asrc->pair[i].volume_ramp.up_wait = (uint32_t)DEFAULT_VOLUME_RAMP_WAIT;

			asrc->pair[i].volume_gain = DEFAULT_VOLUME_GAIN;
		}

		for (i = 0; i < NUM_OF_ASRC_MCAUDIO; i++) {
			asrc->mcaudio_m2p_mux[i] = -1;
		}

		asrc_drv_dbg("asrc_reg : %px\n", asrc->asrc_reg);
		asrc_drv_dbg("asrc_irq: %u\n", asrc->asrc_irq);
		asrc_drv_dbg("pl080_reg : %px\n", asrc->pl080_reg);
		asrc_drv_dbg("pl080_irq : %u\n", asrc->pl080_irq);

		for (i = 0; i < NUM_OF_ASRC_PAIR; i++) {
			asrc_drv_dbg("max-ch-per-pair(%d) : %u\n", i, max_channel[i]);
			asrc_drv_dbg("path_type(%d) : %u\n", i, path_type[i]);
			asrc_drv_dbg("sync-mode(%d) : %u\n", i, sync_mode[i]);
			asrc_drv_dbg("async-refclk(%d) : %u\n", i, async_refclk[i]);
		}

		ret = 0;
	} while (false);

	return ret;
}

static int tcc_asrc_init(const struct tcc_asrc_t *asrc)
{
	(void)clk_prepare_enable(asrc->pl080_hclk);
	(void)clk_prepare_enable(asrc->asrc_hclk);

	tcc_asrc_reset(asrc->asrc_reg);
	tcc_asrc_dma_arbitration(asrc->asrc_reg, (bool)false);

	tcc_pl080_enable(asrc->pl080_reg, (bool)true);

	tcc_pl080_clear_int(asrc->pl080_reg, 0xff);
	tcc_pl080_clear_err(asrc->pl080_reg, 0xff);

	return 0;
}

static int tcc_asrc_deinit(const struct tcc_asrc_t *asrc)
{
	clk_disable_unprepare(asrc->pl080_hclk);
	clk_disable_unprepare(asrc->asrc_hclk);

	return 0;
}

#ifdef CONFIG_ARCH_TCC802X
void check_tcc802x_rev_xx(struct tcc_asrc_t *asrc)
{
#define CHIP_ID_ADDR0\
	(0xE0003C10U)
#define CHIP_ID_ADDR1\
	(0xF400001CU)

#define CHIP_ID_REV_XX_ADDR\
	(0x16042200)

	uint32_t *p0 = ioremap_nocache(CHIP_ID_ADDR0, 4);
	uint32_t *p1 = ioremap_nocache(CHIP_ID_ADDR1, 4);
	uint32_t val;

	val = (*p0 >> 8) & 0x0f;

	if ((val == 0) && (*p1 == CHIP_ID_REV_XX_ADDR)) {
		asrc->chip_rev_xx = true;
	} else {
		asrc->chip_rev_xx = false;

	iounmap(p0);
	iounmap(p1);
}
#endif

static int tcc_asrc_probe(struct platform_device *pdev)
{
	struct tcc_asrc_t *asrc =
	    kzalloc(sizeof(struct tcc_asrc_t), GFP_KERNEL);
	int ret = 0;

	if (asrc == NULL) {
		asrc_drv_err("%s - kzalloc failed.\n", __func__);
		ret = -ENOMEM;
	} else {
		 ret = parse_asrc_dt(pdev, asrc);
		 if (ret < 0) {
			asrc_drv_err("%s : Fail to parse asrc dt\n", __func__);
			kfree(asrc);
		} else {

			platform_set_drvdata(pdev, asrc);
			asrc->pdev = pdev;

#ifdef CONFIG_ARCH_TCC802X
			check_tcc802x_rev_xx(asrc);
#endif

			(void)tcc_asrc_init(asrc);

			(void)tcc_asrc_m2m_drvinit(pdev);
			(void)tcc_asrc_dai_drvinit(pdev);
			//(void)tcc_asrc_pcm_drvinit(pdev);

			ret = request_irq(
					asrc->pl080_irq,
					tcc_pl080_isr,
					IRQF_TRIGGER_HIGH,
					"tcc-asrc-pl080",
					(void *)asrc);
			if (ret < 0) {
				asrc_drv_err("pl080 request_irq(%u) failed\n",
				       asrc->pl080_irq);
				kfree(asrc);
			}

#ifdef ASRC_M2M_INTERRUPT_MODE
			else {
				ret = request_irq(asrc->asrc_irq, tcc_asrc_isr,
					IRQF_TRIGGER_HIGH, "tcc-asrc", (void *)asrc);
				if (ret < 0) {
					asrc_drv_err("ASRC request_irq(%u) failed\n",
							asrc->asrc_irq);
					kfree(asrc);
				}
			}
#endif
		}
	}

	return ret;
}

static int tcc_asrc_remove(struct platform_device *pdev)
{
	struct tcc_asrc_t *asrc = platform_get_drvdata(pdev);

	(void)free_irq(asrc->pl080_irq, (void *)asrc);

	(void)tcc_asrc_deinit(asrc);

	return 0;
}

static int tcc_asrc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct tcc_asrc_t *asrc = platform_get_drvdata(pdev);

	unused(state);
	tcc_asrc_reg_backup(asrc->asrc_reg, &asrc->asrc_regs_backup);

	(void)tcc_asrc_deinit(asrc);

	return 0;
}

static int tcc_asrc_resume(struct platform_device *pdev)
{
	const struct tcc_asrc_t *asrc = platform_get_drvdata(pdev);
	uint32_t i;

	(void)tcc_asrc_init(asrc);

	tcc_asrc_reg_restore(asrc->asrc_reg, &asrc->asrc_regs_backup);

	for (i = 0; i < NUM_OF_ASRC_PAIR; i++) {
		tcc_asrc_volume_enable(asrc->asrc_reg, i, (bool)true);
		tcc_asrc_volume_ramp_enable(asrc->asrc_reg, i, (bool)true);
	}

	return 0;
}

static struct of_device_id const tcc_asrc_of_match[] = {
	{.compatible = "telechips,asrc"},
	{}
};

MODULE_DEVICE_TABLE(of, tcc_asrc_of_match);

static struct platform_driver tcc_asrc_driver = {
	.probe = tcc_asrc_probe,
	.remove = tcc_asrc_remove,
	.suspend = tcc_asrc_suspend,
	.resume = tcc_asrc_resume,
	.driver = {
		.name = "tcc_asrc_drv",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
	   .of_match_table = of_match_ptr(tcc_asrc_of_match),
#endif
	},
};

module_platform_driver(tcc_asrc_driver);

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("Telechips ASRC Driver");
MODULE_LICENSE("GPL");
