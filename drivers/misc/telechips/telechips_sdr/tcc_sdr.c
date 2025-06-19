/****************************************************************************
 * Copyright (C) 2016 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#include <linux/clk-provider.h>
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

#include <linux/timekeeping.h>
#include <linux/pinctrl/consumer.h>
#include "tcc_sdr.h"

#define unused(x) (void)(x)
//#define TCC_SDR_RX_ISR_DEBUG
//#define TCC_SDR_READ_DEBUG
//#define TCC_SDR_DEBUG

#ifdef TCC_SDR_DEBUG
#define SDR_DBG(a...) (void) pr_info("[DEBUG][SDR] " a)
#else
#define SDR_DBG(a...)
#endif

#ifdef TCC_SDR_RX_ISR_DEBUG
#define RX_ISR_DBG(a...) (void) pr_info("[DEBUG][SDR_RX_ISR] " a)
#else
#define RX_ISR_DBG(a...)
#endif

#ifdef TCC_SDR_READ_DEBUG
#define READ_DBG(a...) (void) pr_info("[DEBUG][SDR_READ] " a)
#else
#define READ_DBG(a...)
#endif

#define SDR_WARN(a...) (void) pr_info("[WARN][SDR] " a)
#define SDR_ERR(a...) (void) pr_info("[ERROR][SDR] " a)

#define PREALLOCATE_DMA_BUFFER_MODE

#define SDR_MAX_PORT_NUM	(4u)
#define SDR_READ_TIMEOUT	(1000u)
#define OVERRUN_NOTIFY_INTERVAL	(100u) //ms

#define SWRESET_OF_BUS
#define IQ_MODE_STOP_MIN_DELAY

// I/Q mode: Digital I/Q data receiver
// PCM mode: PCM data receiver

struct tcc_sdr_port_t {
	void *dma_vaddr;
	dma_addr_t dma_paddr;
	uint32_t dma_sz;

	uint32_t valid_sz;
	uint32_t read_pos;
	uint32_t write_pos;
	bool overrun;
};

struct tcc_sdr_t {
	int blk_no;
	struct platform_device *pdev;
	void __iomem *dai_reg;
	void __iomem *adma_reg;
#ifdef SWRESET_OF_BUS
	void __iomem *iocfg_reg;
	uint32_t hrsten_reg_offset;
	uint32_t hrsten_bit_offset;
#endif
	struct clk *dai_pclk;
	struct clk *dai_filter_clk;	//This is for PCM mode
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) ||\
	defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	struct clk *dai_hclk;
	struct clk *adma_hclk;
#endif
	uint32_t adma_irq;
	uint32_t adrcnt_mode;
	uint32_t dai_clk_rate[2];

	//cfg value
	bool iq_mode;	//TRUE: I/Q mode, FALSE: PCM mode
	uint32_t ports_num;
	uint32_t buffer_bytes;
	uint32_t bitmode;
	uint32_t bit_polarity;	//This is for PCM mode
	uint32_t channels;
	enum TCC_SDR_ADMA_I2S_TYPE dev_type;
	uint32_t bclk_ratio;
	uint32_t mclk_div;	//This is for PCM mode
#ifdef IQ_MODE_STOP_MIN_DELAY
	uint32_t nperiod;
	uint32_t nirq;
#endif
	uint32_t period_bytes;
	uint32_t dma_total_size;
	struct tcc_sdr_port_t port[SDR_MAX_PORT_NUM];

	spinlock_t lock;
	struct mutex m;
	wait_queue_head_t wq;

	bool opened;
	bool started;

	struct miscdevice *misc_dev;
#ifdef OVERRUN_NOTIFY_INTERVAL
	struct timespec64 pre_overrun;
	bool first_overrun;
#endif
};

static struct miscdevice *tcc_sdr_get_misc_device(
	void *file_sdr_data)
{
	struct miscdevice *sdr_misc_dev = NULL;

	if (file_sdr_data != NULL) {
		sdr_misc_dev = (struct miscdevice *)file_sdr_data;
	}

	return sdr_misc_dev;
}

static struct tcc_sdr_t *tcc_sdr_dev_get_drvdata(
	const struct device *sdr_misc_parent)
{
	struct tcc_sdr_t *sdr = NULL;

	if (sdr_misc_parent != NULL) {
		sdr = (struct tcc_sdr_t *)dev_get_drvdata(sdr_misc_parent);
	}

	return sdr;
}

static uint32_t tcc_sdr_get_current_dma_period_base_addr(
	const struct tcc_sdr_t *sdr,
	uint32_t sdr_port)
{
	uint32_t addr = 0u;
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	uint64_t addr_64 = 0u;

	if (sdr->iq_mode == TRUE) {
		addr_64 =
		    tcc_adma_dai_multiport_rx_get_cur_dma_addr(sdr->adma_reg, sdr_port);
	} else {
		addr_64 = tcc_adma_dai_rx_get_cur_dma_addr(sdr->adma_reg);
	}
	addr = ull_to_ui(addr_64);
#else
	if (sdr->iq_mode == TRUE) {
		addr =
		    tcc_adma_dai_multiport_rx_get_cur_dma_addr(sdr->adma_reg, sdr_port);
	} else {
		addr = tcc_adma_dai_rx_get_cur_dma_addr(sdr->adma_reg);
	}
#endif
	return addr;
}

static uint32_t calc_valid_sz_offset(
	uint32_t pre_offset,
	uint32_t cur_offset,
	uint32_t max_sz)
{
	uint32_t ret = 0u;
	if (cur_offset >= pre_offset) {
		ret = cur_offset - pre_offset;
	} else {
		if ((pre_offset < max_sz) &&
			((max_sz - pre_offset) < (UINT_MAX - cur_offset))) {
			ret = cur_offset + (max_sz - pre_offset);
		}
	}

	return ret;
}

static uint32_t tcc_adma_get_iq_mode_dbth_value(
	enum TCC_SDR_ADMA_I2S_TYPE dev_type,
	uint32_t fifo_thresh)
{
	//fifo_threshold: 64, 128, 256
	const uint32_t dbth_tbl_2ch[3] = { 0x07, 0x07, 0x07 };	// burst_16
	const uint32_t dbth_tbl_7_1ch[3] = { 0x03, 0x07, 0x0f };	// burst_16
	const uint32_t dbth_tbl_9_1ch[3] = { 0x07, 0x07, 0x07 };	// burst_16
	int thresh_idx = (fifo_thresh == TCC_IQ_FIFO_THRESH_64) ? 0 :
	    (fifo_thresh == TCC_IQ_FIFO_THRESH_128) ? 1 : 2;

	return (dev_type == TCC_ADMA_I2S_9_1CH) ?
		dbth_tbl_9_1ch[thresh_idx] :
		(dev_type == TCC_ADMA_I2S_7_1CH) ?
		dbth_tbl_7_1ch[thresh_idx] :
		dbth_tbl_2ch[thresh_idx];
}

static uint32_t tcc_adma_get_pcm_mode_dbth_value(
	enum TCC_SDR_ADMA_I2S_TYPE dev_type,
	uint32_t channels,
	uint32_t burst_size)
{
	//tdm, mono, 2ch, 4ch, 6ch, 8ch, 10ch
	const uint32_t dbth_tbl_2ch[2][7] = {
		{0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01},	// burst_4
		{0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01},	// burst_8
	};
	const uint32_t dbth_tbl_7_1ch[2][7] = {
		{0x0f, 0x07, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f},	// burst_4
		{0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07},	// burst_8
	};
	const uint32_t dbth_tbl_9_1ch[2][7] = {
		{0x01, 0x01, 0x01, 0x07, 0x0b, 0x0f, 0x13},	// burst_4
		{0x01, 0x01, 0x01, 0x03, 0x05, 0x07, 0x09},	// burst_8
	};
	uint32_t ch_idx, burst_idx;

	ch_idx = (channels / 2u) + 1u;
	burst_idx = (burst_size == TCC_SDR_ADMA_BURST_CYCLE_4) ? 0u : 1u;

	return (dev_type == TCC_ADMA_I2S_9_1CH) ?
		dbth_tbl_9_1ch[burst_idx][ch_idx] :
		(dev_type == TCC_ADMA_I2S_7_1CH) ?
		dbth_tbl_7_1ch[burst_idx][ch_idx] :
		dbth_tbl_2ch[burst_idx][ch_idx];
}

static int tcc_sdr_check_dma_addr (
	struct tcc_sdr_t *sdr,
	uint32_t length)
{
	uint32_t i;
	int ret = 0;
#ifndef PREALLOCATE_DMA_BUFFER_MODE
	for (i = 0u; i < sdr->ports_num; i++) {
		if (sdr->port[i].dma_vaddr == NULL) {
			sdr->port[i].dma_vaddr =
				dma_alloc_coherent(&sdr->pdev->dev, TCC_SDR_BUFFER_SZ_MAX,
						&sdr->port[i].dma_paddr, GFP_KERNEL);
		} else {
			SDR_DBG("[%d]th dma buffer is already allocated\n", i);
		}
	}
#endif
	for (i = 0u; i < sdr->ports_num; i++) {
		if (sdr->port[i].dma_vaddr == NULL) {
			SDR_DBG("[%d]th dma buffer is NULL\n", i);
			ret = -ENOMEM;
		} else {
			sdr->port[i].dma_sz = length;
			SDR_DBG("port(%d) vaddr: %p\n", i,
					(void *)sdr->port[i].dma_vaddr);
		}
	}

	return ret;
}

static void tcc_sdr_set_dma_inbuffer(
	const struct tcc_sdr_t *sdr,
	uint32_t length)
{
	uint32_t i = 0;
	int ret = 0;
	struct tcc_sdr_dma_t sdr_dma;

   //SDR_DBG("period_bytes : %d\n", sdr->period_bytes);

	sdr_dma.base_addr = sdr->adma_reg;
	sdr_dma.buffer_bytes = length;
	sdr_dma.period_bytes = sdr->period_bytes;
	if (sdr->iq_mode == TRUE) {
		if (sdr->bitmode == 16u) {
			ret = tcc_adma_set_dai_rx_dma_buffer(&sdr_dma,
					TCC_ADMA_DATA_WIDTH_16,
					(uint32_t)TCC_SDR_ADMA_BURST_CYCLE_16,
					FALSE);
		} else {
			ret = tcc_adma_set_dai_rx_dma_buffer(&sdr_dma,
					TCC_ADMA_DATA_WIDTH_24,
					(uint32_t)TCC_SDR_ADMA_BURST_CYCLE_16,
					FALSE);
		}
		if (ret < 0) {
			SDR_DBG("[%s][%d] It has something wrong. ret = %d\n",
			__func__, __LINE__, ret);
		}

		for (i = 0u; i < sdr->ports_num; i++) {
			ret =
				tcc_adma_dai_rx_set_dma_addr(sdr->adma_reg,
				sdr->port[i].dma_paddr,
				sdr->iq_mode, i);
			if (ret < 0) {
				SDR_DBG("[%s][%d] It has something wrong.",
				__func__, __LINE__);
				SDR_DBG("ret[%d] = %d\n", i, ret);
			}
		}

	} else {
		if (sdr->bitmode == 16u) {
			ret = tcc_adma_set_dai_rx_dma_buffer(&sdr_dma,
					TCC_ADMA_DATA_WIDTH_16,
					(uint32_t)TCC_SDR_ADMA_BURST_CYCLE_8,
					(sdr->adrcnt_mode != 0u)? TRUE:FALSE);
		} else {
			ret = tcc_adma_set_dai_rx_dma_buffer(&sdr_dma,
					TCC_ADMA_DATA_WIDTH_24,
					(uint32_t)TCC_SDR_ADMA_BURST_CYCLE_8,
					(sdr->adrcnt_mode != 0u)? TRUE:FALSE);
		}
		if (ret < 0) {
			SDR_DBG("[%s][%d] It has something wrong. ret = %d\n",
			__func__, __LINE__, ret);
		}

		for (i = 0u; i < sdr->ports_num; i++) {
			ret =
			tcc_adma_dai_rx_set_dma_addr(sdr->adma_reg,
				sdr->port[i].dma_paddr,
				sdr->iq_mode, i);
			if (ret < 0) {
				SDR_DBG("[%s][%d] It has something wrong.",
				__func__, __LINE__);
				SDR_DBG("ret[%d] = %d\n", i, ret);
			}
		}
	}
	tcc_adma_set_rx_dma_repeat_type(sdr->adma_reg,
		TCC_ADMA_REPEAT_FROM_CUR_ADDR);
	tcc_adma_repeat_infinite_mode(sdr->adma_reg);

	SDR_DBG("%s - HwRxDaParam [0x%X]\n", __func__,
		readl(sdr->adma_reg + TCC_ADMA_RXDAPARAM_OFFSET));
	SDR_DBG("%s - HwRxDaTCnt [%d]\n", __func__,
		readl(sdr->adma_reg + TCC_ADMA_RXDATCNT_OFFSET));
}

static int tcc_sdr_initialize(struct tcc_sdr_t *sdr)
{
	uint32_t dbth = 0u;
	int ret = 0;

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) ||\
	defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	if (sdr->dai_hclk != NULL) {
		ret = clk_prepare_enable(sdr->dai_hclk);
		if (ret < 0) {
			SDR_DBG("[%s] dai_hclk enable is wrong!!\n", __func__);
		}
	}

	if (sdr->adma_hclk != NULL) {
		ret = clk_prepare_enable(sdr->adma_hclk);
		if (ret < 0) {
			SDR_DBG("[%s] adma_hclk enable is wrong!!\n", __func__);
		}
	}
#endif
	tcc_dai_set_rx_mute(sdr->dai_reg, TRUE);

	if (sdr->dai_pclk != NULL) {
		ret = clk_set_rate(sdr->dai_filter_clk,
				TCC_DAI_FILTER_MAX_FREQ);
		if (ret < 0) {
			SDR_DBG("[%s] dai_filter_clk set is wrong!!\n", __func__);
		}
		ret = clk_prepare_enable(sdr->dai_filter_clk);
		if (ret < 0) {
			SDR_DBG("[%s] dai_filter_clk enable is wrong!!\n", __func__);
		}
		ret = clk_set_rate(sdr->dai_pclk, TCC_DAI_MAX_FREQ);
		if (ret < 0) {
			SDR_DBG("[%s] dai_pclk set is wrong!!\n", __func__);
		}
		ret = clk_prepare_enable(sdr->dai_pclk);
		if (ret < 0) {
			SDR_DBG("[%s] dai_pclk enable is wrong!!\n", __func__);
		}
	}
	//SLAVE MODE Setting mclk_mst, bclk_mst,
	//lrck_mst, tdm_mode, is_pinctrl_export
	tcc_dai_set_master_mode(sdr->dai_reg, FALSE, FALSE, FALSE, TRUE);

	ret = tcc_sdr_check_dma_addr(sdr, sdr->buffer_bytes);
	if (ret < 0) {
		SDR_DBG("[%s] dma_addr is wrong!!\n", __func__);
	} else {
		tcc_sdr_set_dma_inbuffer(sdr, sdr->buffer_bytes);
	}

	//Bit Mode Setting
	if (sdr->iq_mode == TRUE) {
		tcc_digital_iq_set_fifo_threshold(sdr->dai_reg,
					DIGITAL_IQ_FIFO_THRESHOLD);
		tcc_digital_iq_set_bitmode(sdr->dai_reg, sdr->bitmode);
		tcc_digital_iq_set_portsel(sdr->dai_reg, sdr->ports_num);
		tcc_dai_set_dao_mask(sdr->dai_reg, TRUE, TRUE, TRUE, TRUE,
				TRUE);

		dbth =
		tcc_adma_get_iq_mode_dbth_value(sdr->dev_type,
					 DIGITAL_IQ_FIFO_THRESHOLD);
		tcc_adma_dai_threshold(sdr->adma_reg, dbth);

		//audio filter enable
		tcc_dai_set_audio_filter_enable(sdr->dai_reg, FALSE);

		tcc_dai_dma_threshold_enable(sdr->dai_reg, TRUE);

	} else {
		if (sdr->bitmode == 16u) {
			tcc_dai_set_rx_format(sdr->dai_reg, TCC_DAI_LSB_16);
			tcc_adma_set_dai_rx_dma_width(sdr->adma_reg, TCC_ADMA_DATA_WIDTH_16);
		} else {    //24bitmode
			tcc_dai_set_rx_format(sdr->dai_reg, TCC_DAI_LSB_24);
			tcc_adma_set_dai_rx_dma_width(sdr->adma_reg, TCC_ADMA_DATA_WIDTH_24);
		}
		dbth =
		tcc_adma_get_pcm_mode_dbth_value(sdr->dev_type, sdr->channels,
				   TCC_SDR_ADMA_BURST_CYCLE_8);
		tcc_adma_dai_threshold(sdr->adma_reg, dbth);

		//audio filter enable
		tcc_dai_set_audio_filter_enable(sdr->dai_reg, FALSE);

		tcc_dai_dma_threshold_enable(sdr->dai_reg, TRUE);
		tcc_dai_set_dao_mask(sdr->dai_reg, TRUE, TRUE, TRUE, TRUE,
				TRUE);

		if (sdr->channels == 2u) {    //stereo mode
			tcc_dai_set_multiport_mode(sdr->dai_reg, FALSE);
			tcc_adma_set_dai_rx_multi_ch(sdr->adma_reg, FALSE,
						TCC_ADMA_MULTI_CH_MODE_7_1);
		} else if (sdr->channels == 8u) {  //8channel mode
			tcc_dai_set_multiport_mode(sdr->dai_reg, TRUE);
			tcc_adma_set_dai_rx_multi_ch(sdr->adma_reg, TRUE,
						TCC_ADMA_MULTI_CH_MODE_7_1);
		} else {
			SDR_DBG("PCM mode Can't support %d channel mode\n",
			sdr->channels);
			ret = -EINVAL;
		}
	}

	tcc_adma_set_dai_rx_dma_repeat_enable(sdr->adma_reg, TRUE);
	return ret;
}

static int tcc_sdr_check_param_channel(struct sdr_param *p)
{
	int ret = 0;

	if (p->sdr_iq_mode != 0u) {
		if ((p->sdr_channel > 4u) || (p->sdr_channel <= 0u)) {
			SDR_WARN("Channel[%d] should be 1/2/4.",
				p->sdr_channel);
			(void) pr_info("So, it is changed by default[%d]\n",
			    IQ_MODE_DEFAULT_CHANNEL);
			p->sdr_channel = IQ_MODE_DEFAULT_CHANNEL;
			ret = 1;
		}
	} else {		//PCM Mode
		if ((p->sdr_channel != 2u) && (p->sdr_channel != 8u)) {
			SDR_WARN("Channel[%d] should be 2 or 8.",
				p->sdr_channel);
			(void) pr_info("So, it is changed by default[%d]\n",
				PCM_MODE_DEFAULT_CHANNEL);
			p->sdr_channel = PCM_MODE_DEFAULT_CHANNEL;
			ret = 1;
		}
	}

	return ret;
}

static int tcc_sdr_check_param_bit_mode(struct sdr_param *p)
{
	int ret = 0;

	if (p->sdr_iq_mode != 0u) {
		if ((p->sdr_bit_mode != 16u) && (p->sdr_bit_mode != 20u) &&
		    (p->sdr_bit_mode != 24u) && (p->sdr_bit_mode != 30u) &&
		    (p->sdr_bit_mode != 32u) && (p->sdr_bit_mode != 40u) &&
		    (p->sdr_bit_mode != 48u) && (p->sdr_bit_mode != 60u) &&
		    (p->sdr_bit_mode != 64u) && (p->sdr_bit_mode != 80u)) {
			SDR_WARN("BitMode[%d] is wrong.",
				p->sdr_bit_mode);
			(void) pr_info("So, it is changed by default[%d]\n",
				IQ_MODE_DEFAULT_BITMODE);
			p->sdr_bit_mode = IQ_MODE_DEFAULT_BITMODE;
			ret = 1;
		}
	} else {		//PCM Mode
		if ((p->sdr_bit_mode != 16u) && (p->sdr_bit_mode != 24u)) {
			SDR_WARN("BitMode[%d] is wrong.",
				p->sdr_bit_mode);
			(void) pr_info("So, it is changed by default[%d]\n",
				PCM_MODE_DEFAULT_BITMODE);
			p->sdr_bit_mode = PCM_MODE_DEFAULT_BITMODE;
			ret = 1;
		}
	}

	return ret;
}

static int tcc_sdr_check_param_buffer_min(struct sdr_param *p)
{
	int ret = 0;

	if (p->sdr_bufferbytes < TCC_SDR_BUFFER_SZ_MIN) {
		SDR_WARN("Bufferbytes[0x%08x] is wrong.",
			p->sdr_bufferbytes);
		(void) pr_info("So, it is changed by min[0x%08x]\n",
			TCC_SDR_BUFFER_SZ_MIN);
		p->sdr_bufferbytes = TCC_SDR_BUFFER_SZ_MIN;
		ret = 1;
	}

	return ret;
}

static int tcc_sdr_check_param_period_min(struct sdr_param *p, uint32_t period_min)
{
	int ret = 0;

	if (p->sdr_periodbytes < period_min) {
		if (p->sdr_iq_mode != 0u) {
			if ((p->sdr_bufferbytes / IQ_MODE_DEFAULT_PERIOD_DIV) >
			    period_min) {
				period_min =
				    p->sdr_bufferbytes /
				    IQ_MODE_DEFAULT_PERIOD_DIV;
			}
		} else {
			if ((p->sdr_bufferbytes / PCM_MODE_DEFAULT_PERIOD_DIV) >
			    period_min) {
				period_min =
				    p->sdr_bufferbytes /
				    PCM_MODE_DEFAULT_PERIOD_DIV;
			}
		}
		SDR_WARN("Periodbytes[0x%08x] is wrong.",
			p->sdr_periodbytes);
		(void) pr_info("So, it is changed by min[0x%08x]\n",
			period_min);
		p->sdr_periodbytes = period_min;
		ret = 1;
	}

	return ret;
}

static int tcc_sdr_set_param(struct tcc_sdr_t *sdr, struct sdr_param *p)
{
	uint32_t period_min = 0u, sz_check = 0u, overall_sz = 0u;
	bool dab_en = FALSE;
	int ret = 0;

	SDR_DBG("%s\n", __func__);
	SDR_DBG("Channel : %d\n", p->sdr_channel);
	SDR_DBG("Bitmode : %d\n", p->sdr_bit_mode);
	SDR_DBG("Bitpolarity: %d\n", p->sdr_bit_polarity);
	SDR_DBG("Bufferbytes : %d\n", p->sdr_bufferbytes);
	SDR_DBG("Periodbytes : %d\n", p->sdr_periodbytes);

	if (p->sdr_sample_rate >= 512000u) {
		SDR_DBG("Samplerate : %d. Set for DAB.\n", p->sdr_sample_rate);
		dab_en = TRUE;
	}

	ret = tcc_sdr_check_param_channel(p);
	ret = tcc_sdr_check_param_bit_mode(p);

	period_min = (p->sdr_iq_mode != 0u)?
		TCC_SDR_PERIOD_SZ_IQ_MIN : TCC_SDR_PERIOD_SZ_PCM_MIN;

	ret = tcc_sdr_check_param_buffer_min(p);
	ret = tcc_sdr_check_param_period_min(p, period_min);

	sz_check = 31u;
	while (sz_check > 3u) {
		uint32_t uitemp = ui_lshift(1u, sz_check);
		if ((uitemp & p->sdr_bufferbytes) != 0u) {
			break;
		}

		sz_check--;
	};

	if ((p->sdr_bufferbytes > TCC_SDR_BUFFER_SZ_MAX)
	|| (p->sdr_periodbytes > TCC_SDR_PERIOD_SZ_MAX)
	|| (sz_check >= 31u)) {
		SDR_WARN("Periodbytes[0x%08x], Bufferbytes[0x%08x], sz_check[%d] is wrong.",
			p->sdr_periodbytes, p->sdr_bufferbytes, sz_check);
		ret = -EINVAL;
	} else {
		uint32_t i;
		if (p->sdr_iq_mode != 0u) {

			sdr->iq_mode = TRUE;
			sdr->ports_num = p->sdr_channel;
			sdr->bitmode = p->sdr_bit_mode;
			sdr->bit_polarity = p->sdr_bit_polarity;

#ifdef IQ_MODE_STOP_MIN_DELAY
			if (dab_en == TRUE) {
				overall_sz = 64u;
			} else {
				overall_sz = TCC_SDR_PERIOD_SZ_IQ_MIN;
			}
#else
			overall_sz = 64u;
			//2^(16BSIZE+32WSIZE)
#endif
		} else {		//PCM Mode
			sdr->iq_mode = FALSE;
			sdr->channels = p->sdr_channel;
			sdr->ports_num = 1;
			sdr->bclk_ratio = DEFAULT_BCLK_RATIO;
			sdr->mclk_div = DEFAULT_MCLK_DIV;
			sdr->bitmode = p->sdr_bit_mode;
			overall_sz = 32u;
			//2^(8BSIZE+32WSIZE)
		}

		for (i = 0u; i < SDR_MAX_PORT_NUM; i++) {
			sdr->port[i].overrun = FALSE;
		}

		if (p->sdr_bufferbytes != ui_lshift(1u, sz_check)) {
			uint32_t sz_2_pow = 0u;
			sz_2_pow = ui_lshift(1u, (sz_check + 1u));
			SDR_WARN("buffer_bytes[%u] should be 2^N[%u]\n",
					p->sdr_bufferbytes, sz_2_pow);
			SDR_WARN("buffer_bytes[%u] change to [%u]\n",
					p->sdr_bufferbytes, sz_2_pow);
			p->sdr_bufferbytes = sz_2_pow;
			sdr->buffer_bytes = sz_2_pow;
			ret = 1;
		} else {
			sdr->buffer_bytes = p->sdr_bufferbytes;
		}

		sz_check = p->sdr_periodbytes % overall_sz;
		if (sz_check != 0u) {
			sz_check = p->sdr_periodbytes / overall_sz;
			SDR_WARN("period_bytes[%u] should be multiple of %d[%u]\n",
					p->sdr_periodbytes, overall_sz, sz_check * overall_sz);
			SDR_WARN("period_bytes[%u] change to [%u]\n",
					p->sdr_periodbytes, sz_check * overall_sz);
			p->sdr_periodbytes = sz_check * overall_sz;
			sdr->period_bytes = p->sdr_periodbytes;
			ret = 1;
		} else {
			sdr->period_bytes = p->sdr_periodbytes;
		}

#ifdef IQ_MODE_STOP_MIN_DELAY
		if (p->sdr_iq_mode != 0u) {
			if (dab_en == TRUE) {
				sdr->nperiod = 1u;
			} else {
				sdr->nperiod = p->sdr_periodbytes / TCC_SDR_PERIOD_SZ_IQ_MIN;
				sdr->period_bytes = TCC_SDR_PERIOD_SZ_IQ_MIN;
			}
		} else {		//PCM Mode
			sdr->nperiod = 1u;
		}
#endif

#if defined(CONFIG_ARCH_TCC807X)
		tcc_audio_maic_select_interface(sdr->dai_reg, TCC_INTERFACE_ADMA);
#endif
		if (tcc_sdr_initialize(sdr) != 0) {
			SDR_WARN("[%s] tcc_sdr_initialize is fail!\n",
					__func__);
			ret = -EINVAL;
		}
	}

	return ret;
}

static void tcc_sdr_overrun_dbg_msg(struct tcc_sdr_t *sdr)
{
	uint32_t i;
#ifdef OVERRUN_NOTIFY_INTERVAL
	struct timespec64 pre, next;
	long overrun_ns = 0;
	uint32_t overrun_us = 0;
#endif

	for (i = 0u; i < sdr->ports_num; i++) {
		if (sdr->port[i].overrun == TRUE) {	// Overrun
			if ((sdr->opened == TRUE) && (sdr->started == TRUE)) {
#ifdef OVERRUN_NOTIFY_INTERVAL
				ktime_get_real_ts64(&next);
				pre = sdr->pre_overrun;

				if (next.tv_nsec >= pre.tv_nsec) {
					overrun_ns = next.tv_nsec - pre.tv_nsec;
				}

				overrun_us = sl_to_ui(overrun_ns / NSEC_PER_USEC);

				if ((sdr->first_overrun == FALSE) ||
					(overrun_us >=
					(OVERRUN_NOTIFY_INTERVAL*1000u))) {
					sdr->first_overrun = TRUE;
#ifdef TCC_SDR_DEBUG
					SDR_WARN("[dev-%d] %s - Overrun(%d),",
							sdr->blk_no, __func__, i);
					(void) pr_info("new_read_pos:0x%x, dma_sz:0x%x,",
							sdr->port[i].read_pos,
							sdr->port[i].dma_sz);
					(void) pr_info("valid_sz(%p):0x%x\n",
							&sdr->port[i].valid_sz,
							sdr->port[i].valid_sz);
#else
					SDR_WARN("[dev-%d] %s - Overrun(%d)\n",
							sdr->blk_no, __func__, i);
#endif
					sdr->pre_overrun.tv_sec = next.tv_sec;
					sdr->pre_overrun.tv_nsec = next.tv_nsec;
				}
#else
#ifdef TCC_SDR_DEBUG
				SDR_WARN("[dev-%d] %s - Overrun(%d),",
					sdr->blk_no, __func__, i);
				(void) pr_info("new_read_pos:0x%x, dma_sz:0x%x,",
					sdr->port[i].read_pos,
					sdr->port[i].dma_sz);
				(void) pr_info("valid_sz(%p):0x%x\n",
					&sdr->port[i].valid_sz,
					sdr->port[i].valid_sz);
#else
				SDR_WARN("[dev-%d] %s - Overrun(%d)\n",
					sdr->blk_no, __func__, i);
#endif
#endif
			}
		}
	}
}

static void tcc_sdr_rx_isr(struct tcc_sdr_t *sdr)
{
	uint32_t i, cur_period_offset = 0;
	unsigned long flags;

	spin_lock_irqsave(&sdr->lock, flags);

	for (i = 0u; i < sdr->ports_num; i++) {
		struct tcc_sdr_port_t *port = &sdr->port[i];
		uint32_t cur_addr, base_addr;

		cur_addr = tcc_sdr_get_current_dma_period_base_addr(sdr, i);
		base_addr = ull_to_ui(port->dma_paddr);
		if (cur_addr >= base_addr) {
			cur_period_offset =
				cur_addr - base_addr;
		}

		if ((sdr->opened == TRUE) && (sdr->started == TRUE)) {
			uint32_t calc_ret = 0;
			calc_ret = calc_valid_sz_offset(port->write_pos,
				cur_period_offset, port->dma_sz);
			if (port->valid_sz < (UINT_MAX - calc_ret)) {
				port->valid_sz += calc_ret;
			}

#ifdef TCC_SDR_RX_ISR_DEBUG
			RX_ISR_DBG("port(%d) valid_sz:0x%8x, read_pos:0x%8x,",
				i, port->valid_sz, port->read_pos);
			(void) pr_info("write_pos:0x%8x, new_write_pos:0x%8x\n",
				port->write_pos, cur_period_offset);
#endif

			if (port->valid_sz > port->dma_sz) {// Overrun
				port->valid_sz = 0;
				port->read_pos = cur_period_offset;

				port->overrun = TRUE;
			}
		}
		port->write_pos = cur_period_offset;

	}
	spin_unlock_irqrestore(&sdr->lock, flags);

	wake_up_interruptible(&sdr->wq);

	tcc_sdr_overrun_dbg_msg(sdr);
}

static irqreturn_t tcc_sdr_isr(int irq, void *dev)
{
	struct tcc_sdr_t *sdr = (struct tcc_sdr_t *)dev;
	bool adma_istatus;
	uint32_t clear_timeout = 0u, cur_pos = 0u, pre_pos = 0u;

	adma_istatus = tcc_adma_dai_rx_irq_check(sdr->adma_reg);
	cur_pos = tcc_sdr_get_current_dma_period_base_addr(sdr, 0);
	do {
#ifdef IQ_MODE_STOP_MIN_DELAY
		if (adma_istatus == TRUE) {
			if (cur_pos != pre_pos) {
				if (sdr->nirq <= (UINT_MAX -1u)) {
					sdr->nirq++;
				}
			}
			if (sdr->nirq >= sdr->nperiod) {
				tcc_sdr_rx_isr(sdr);
				sdr->nirq = 0;
			}
		}
#else
		if (adma_istatus == TRUE) {
			tcc_sdr_rx_isr(sdr);
		}
#endif
		pre_pos = cur_pos;
		tcc_adma_dai_rx_irq_clear(sdr->adma_reg);
		adma_istatus = tcc_adma_dai_rx_irq_check(sdr->adma_reg);
		cur_pos = tcc_sdr_get_current_dma_period_base_addr(sdr, 0);
		clear_timeout ++;
	} while((adma_istatus == TRUE) && (clear_timeout <= 100u));

	if (adma_istatus == TRUE) {
		SDR_WARN("dev-%d %s : DAI RX IRQ(%d) not clear!\n",
				sdr->blk_no, __func__, irq);
	}

	return IRQ_HANDLED;
}

static uint32_t tcc_sdr_get_read_pos_copy_size(
	struct tcc_sdr_t *sdr,
	uint32_t sdr_port,
	uint32_t *copy_size,
	unsigned int readcnt)
{
	struct tcc_sdr_port_t *port = &sdr->port[sdr_port];
	uint32_t first_sz = 0u, second_sz = 0u;
	uint32_t read_pos, new_read_pos;
	unsigned long flags = 0u;

	spin_lock_irqsave(&sdr->lock, flags);

	if (port->valid_sz < readcnt) {
		readcnt = port->valid_sz;
	}

#ifdef TCC_SDR_READ_DEBUG
	READ_DBG("<bf>(%d) base:0x%08x, len:0x%x\n",
			sdr_port, port->read_pos, readcnt);
#endif

	read_pos = port->read_pos;
	new_read_pos = port->read_pos + readcnt;
	if (new_read_pos > port->dma_sz) {
		first_sz = port->dma_sz - port->read_pos;
		if (readcnt > first_sz) {
			second_sz = readcnt - first_sz;
		} else {
			SDR_ERR("%s - dev-%d size is wrong[readcnt=%d, first_sz=%d.\n",
					__func__, sdr->blk_no, readcnt, first_sz);
		}
		port->read_pos = second_sz;
	} else {
		first_sz = readcnt;
		second_sz = 0u;
		port->read_pos =
			(new_read_pos == port->dma_sz) ? 0u : new_read_pos;
	}

	port->valid_sz -= readcnt;

#ifdef TCC_SDR_READ_DEBUG
	READ_DBG("<ft>-valid_sz:0x%x, read_pos:0x%x, readcnt:0x%x\n",
			port->valid_sz, port->read_pos, readcnt);
#endif

	spin_unlock_irqrestore(&sdr->lock, flags);

	copy_size[0] = first_sz;
	copy_size[1] = second_sz;

	return read_pos;
}

static int tcc_sdr_copy_from_dma(
	struct tcc_sdr_t *sdr,
	uint32_t sdr_port,
	char *buf,
	unsigned int readcnt)
{
	struct tcc_sdr_port_t *port = &sdr->port[sdr_port];
	int ret = 0;

	if (sdr->started != TRUE) {
		SDR_DBG("%s - not started\n", __func__);
		ret = -EPROTO;
	} else {
		if (port->overrun == TRUE) {
			SDR_DBG("dev-%d Overrun detected\n",
					sdr->blk_no);
			port->overrun = FALSE;
			ret = -EPIPE;
		}
	}

	if (ret >= 0) {
		long read_timeout, ret_wait_event = 0;

		read_timeout = ul_to_sl(msecs_to_jiffies(SDR_READ_TIMEOUT));

		ret_wait_event = wait_event_interruptible_timeout(sdr->wq,
									(port->valid_sz >= readcnt),
									read_timeout);

		if (ret_wait_event <= 0) {
			SDR_ERR("%s - dev-%d timeout[%d]sec, ret=[%ld].",
					__func__, sdr->blk_no, SDR_READ_TIMEOUT, ret_wait_event);
			SDR_ERR("Please check tuner status.\n");
			ret = -EIO;
		} else {
			uint32_t read_pos = 0u, copy_size[2] = {0u,};
			unsigned long copy_ret[2] = {0u,};
			read_pos = tcc_sdr_get_read_pos_copy_size(sdr, sdr_port, copy_size, readcnt);

			if (copy_size[0] > 0u) {
				//first copy
				copy_ret[0] = copy_to_user(buf,
										port->dma_vaddr + read_pos,
										copy_size[0]);
			}

			if (copy_size[1] > 0u) {
				//second copy
				copy_ret[1] = copy_to_user(buf + copy_size[0],
										port->dma_vaddr,
										copy_size[1]);
			}

			if ((copy_ret [0] != 0u) || (copy_ret [1] != 0u)) {
				SDR_ERR("[%s][%d]dev-%d copy_to_user failed\n", __func__,
						__LINE__, sdr->blk_no);
				ret = -EFAULT;
			} else {
				ret = ui_to_si(readcnt);
			}
		}
	}

	return ret;
}



#ifndef PREALLOCATE_DMA_BUFFER_MODE
static void tcc_sdr_deinitialize(struct tcc_sdr_t *sdr)
#else
static void tcc_sdr_deinitialize(const struct tcc_sdr_t *sdr)
#endif
{
#ifndef PREALLOCATE_DMA_BUFFER_MODE
	uint32_t i;

	SDR_DBG("Free pre-allocated dma buffer\n");

	for (i = 0u; i < sdr->ports_num; i++) {
		if (sdr->port[i].dma_vaddr != NULL) {
			dma_free_coherent(&sdr->pdev->dev, TCC_SDR_BUFFER_SZ_MAX,
					sdr->port[i].dma_vaddr, sdr->port[i].dma_paddr);
			sdr->port[i].dma_vaddr = NULL;
			sdr->port[i].dma_paddr = (dma_addr_t)NULL;
		}
	}
#endif

	if ((sdr->dai_pclk != NULL) &&
		(__clk_is_enabled(sdr->dai_pclk) == TRUE)) {
		clk_disable_unprepare(sdr->dai_pclk);
	}

	if ((sdr->dai_filter_clk != NULL) &&
		(__clk_is_enabled(sdr->dai_filter_clk) == TRUE)) {
		clk_disable_unprepare(sdr->dai_filter_clk);
	}

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) ||\
	defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	if ((sdr->dai_hclk != NULL) &&
		(__clk_is_enabled(sdr->dai_hclk) == TRUE)) {
		clk_disable_unprepare(sdr->dai_hclk);
	}

	if ((sdr->adma_hclk != NULL) &&
		(__clk_is_enabled(sdr->adma_hclk) == TRUE)) {
		clk_disable_unprepare(sdr->adma_hclk);
	}
#endif
}

static int tcc_sdr_start(struct tcc_sdr_t *sdr)
{
#ifdef OVERRUN_NOTIFY_INTERVAL
	struct timespec64 start;
#endif
	uint32_t i;
	int ret = 0;

	if (sdr->started == TRUE) {
		SDR_DBG("%s - already started\n",
		       __func__);
		ret = -EPROTO;
	} else {
#ifdef IQ_MODE_STOP_MIN_DELAY
		sdr->nirq = 0;
#endif
		for (i = 0u; i < sdr->ports_num; i++) {
			sdr->port[i].valid_sz = 0;
			sdr->port[i].read_pos = 0;
			sdr->port[i].write_pos = 0;
			sdr->port[i].overrun = FALSE;
		}
		tcc_adma_dai_rx_irq_enable(sdr->adma_reg, TRUE);
		tcc_adma_dai_rx_dma_enable(sdr->adma_reg, TRUE);

		tcc_dai_set_rx_mute(sdr->dai_reg, FALSE); //mute disable

#ifdef OVERRUN_NOTIFY_INTERVAL
		sdr->first_overrun = FALSE;
		ktime_get_real_ts64(&start);
		sdr->pre_overrun.tv_sec = start.tv_sec;
		sdr->pre_overrun.tv_nsec = start.tv_nsec;
#endif

		if (sdr->iq_mode == TRUE) {
			tcc_digital_iq_enable(sdr->dai_reg, TRUE);
		} else {
			tcc_dai_enable(sdr->dai_reg, TRUE); //dai enable for PCM mode
		}

		tcc_dai_rx_enable(sdr->dai_reg, TRUE); //dai rx enable

		//tcc_dai_dump(sdr->dai_reg);
		//tcc_adma_dump(sdr->adma_reg);
		sdr->started = TRUE;
	}

	return ret;
}

static int tcc_sdr_stop(struct tcc_sdr_t *sdr, bool rxdma_en)
{
	int32_t ret = 0;

	if ((sdr->started != TRUE) && (rxdma_en != TRUE)) {
		SDR_DBG("%s - not started\n", __func__);
		ret = -EPROTO;
	} else {
		SDR_DBG("%s\n", __func__);

		tcc_dai_set_rx_mute(sdr->dai_reg, TRUE);

		if (sdr->iq_mode == TRUE) {
			ret = tcc_adma_dai_rx_auto_dma_disable(sdr->adma_reg);
		} else {
			ret = 1; //PCM mode
		}

		if (ret != 0) {
			tcc_adma_dai_rx_dma_enable(sdr->adma_reg, FALSE);
		}

		tcc_adma_dai_rx_irq_enable(sdr->adma_reg, FALSE);

		/* HopCount Clear */
		tcc_adma_dai_rx_hopcnt_clear(sdr->adma_reg);

		tcc_adma_dai_rx_reset_enable(sdr->adma_reg, TRUE);

		//FIFO Clear
		tcc_dai_rx_fifo_clear(sdr->dai_reg);

		tcc_sdr_u_delay(1000u);

		tcc_dai_rx_enable(sdr->dai_reg, FALSE); //dai rx disable

		tcc_dai_rx_fifo_release(sdr->dai_reg);

		tcc_adma_dai_rx_reset_enable(sdr->adma_reg, FALSE);

		if (!rxdma_en) {
			if (sdr->iq_mode == TRUE) {
				tcc_digital_iq_enable(sdr->dai_reg, FALSE);
			} else {
				tcc_dai_enable(sdr->dai_reg, FALSE); //dai disable for PCM mode
			}
		} else {
			tcc_digital_iq_enable(sdr->dai_reg, FALSE);
			tcc_dai_enable(sdr->dai_reg, FALSE); //dai disable for PCM mode
		}

		//audio filter disable
		tcc_dai_set_audio_filter_enable(sdr->dai_reg, FALSE);
		//tcc_sdr_deinitialize(sdr);
		sdr->started = FALSE;
	}

	return ret;
}

static __poll_t tcc_sdr_poll(
	struct file *file_sdr,
	struct poll_table_struct *wait_sdr)
{
	const struct miscdevice *misc = tcc_sdr_get_misc_device(file_sdr->private_data);
	struct tcc_sdr_t *sdr = tcc_sdr_dev_get_drvdata(misc->parent);
	__poll_t ret = 0u;
	uint32_t i;

	//SDR_DBG("%s\n", __func__);
	if (sdr != NULL) {
		for (i = 0u; i < sdr->ports_num; i++) {
			if (sdr->port[i].valid_sz > 0u) {
				ret = si_to_ui(POLLIN);
				break;
			}
		}

		if (ret == 0u) {
			poll_wait(file_sdr, &sdr->wq, wait_sdr);

			for (i = 0u; i < sdr->ports_num; i++) {
				if (sdr->port[i].valid_sz > 0u) {
					ret = si_to_ui(POLLIN);
					break;
				}
			}
		}
	} else {
		SDR_ERR("[%s] sdr is NULL.\n", __func__);
		ret = si_to_ui(POLLERR);
	}

	return ret;
}

static long tcc_sdr_ioctl(
	struct file *file_sdr,
	unsigned int cmd, unsigned long arg)
{
	const struct miscdevice *misc = tcc_sdr_get_misc_device(file_sdr->private_data);
	struct tcc_sdr_t *sdr = tcc_sdr_dev_get_drvdata(misc->parent);
	int ret = 0;

	unused(file_sdr);
	if (sdr != NULL) {
		mutex_lock(&sdr->m);
		switch (cmd) {
			case SDR_IQ_MODE_RX_DAI:
				{
					struct sdr_rx_buf_param param;
					param.sdr_buf = NULL;
					param.read_count = 0u;
					param.sdr_port_index = 0u;

					if (copy_from_user((void *)&param,
								(const void __user *)arg,
								sizeof(struct sdr_rx_buf_param)) == 0u) {
						uint32_t port_index = 0u;

						if (param.sdr_port_index < SDR_MAX_PORT_NUM) {
							port_index = param.sdr_port_index;
						} else {
							port_index = 0u;
						}

						ret = tcc_sdr_copy_from_dma(sdr,
								port_index,
								param.sdr_buf,
								param.read_count);
					} else {
						SDR_DBG("SDR_IQ_MODE_RX_DAI From User Fail!!\n");
						ret = -EFAULT;
					}

					if (ret < 0) {
						param.read_count = 0;
						if (copy_to_user
								((void __user *)arg, (const void *)&param,
								 sizeof(param)) != 0u) {
							SDR_DBG("SDR_IQ_MODE_RX_DAI To User Fail!!\n");
							ret = -EFAULT;
						}
					}
				}
				break;
			case SDR_PCM_MODE_RX_DAI:
				{
					struct sdr_rx_buf_param param;
					param.sdr_buf = NULL;
					param.read_count = 0u;
					param.sdr_port_index = 0u;

					if (copy_from_user((void *)&param, (const void __user *)arg,
								sizeof(struct sdr_rx_buf_param)) == 0u) {
						param.sdr_port_index = 0u;
						ret =
							tcc_sdr_copy_from_dma(sdr,
									param.sdr_port_index,
									param.sdr_buf,
									param.read_count);
					} else {
						SDR_DBG("SDR_PCM_MODE_RX_DAI Fail!!\n");
						ret = -EFAULT;
					}

					if (ret < 0) {
						param.read_count = 0;
						if (copy_to_user
								((void __user *)arg, (const void *)&param,
								 sizeof(param)) != 0u) {
							SDR_DBG("SDR_PCM_MODE_RX_DAI Fail!!\n");
							ret = -EFAULT;
						}
					}
				}
				break;
			case SDR_RX_START:
				SDR_DBG("SDR_RX_START\n");
				ret = tcc_sdr_start(sdr);
				break;
			case SDR_RX_STOP:
				SDR_DBG("SDR_RX_STOP\n");
				ret = tcc_sdr_stop(sdr, FALSE);
				break;
			case SDR_SET_PARAMS:
				SDR_DBG("SDR_SET_PARAMS\n");
				{
					struct sdr_param param;
					(void) memset(&param, 0, sizeof(struct sdr_param));

					if (copy_from_user((void *)&param,
								(const void __user *)arg,
								sizeof(struct sdr_param)) == 0u) {
						ret = tcc_sdr_set_param(sdr, &param);
					} else {
						SDR_DBG("SDR_SET_PARAMS From User Fail!!\n");
						ret = -EFAULT;
					}

					if (ret >= 0) {
						if (copy_to_user
								((void __user *)arg, (const void *)&param,
								 sizeof(struct sdr_param)) != 0u) {
							SDR_DBG("SDR_SET_PARAMS To User Fail!!\n");
							ret = -EFAULT;
						}
					}
				}
				break;
			case SDR_GET_VALID_BYTES:
				{
					uint32_t i, valid[SDR_MAX_PORT_NUM] = { 0, };

					for (i = 0u; i < sdr->ports_num; i++) {
						valid[i] = sdr->port[i].valid_sz;
						//SDR_DBG("[dev-%d][%d] valid=%d\n",
						//sdr->blk_no, i, valid[i]);
					}

					if (copy_to_user
							((void __user *)arg, (const void *)valid,
							 sizeof(uint32_t) * SDR_MAX_PORT_NUM) != 0u) {
						SDR_DBG("SDR_GET_VALID_BYTES Fail!!\n");
						ret = -EFAULT;
					} else {
						ret = 0;
					}
				}
				break;
			default:
				SDR_DBG("CMD(0x%x)is not supported\n",
						cmd);
				ret = 0;
				break;
		}
		mutex_unlock(&sdr->m);
	} else {
		SDR_ERR("[%s] sdr is NULL.\n", __func__);
		ret = -EINVAL;
	}

	return (long)ret;
}

static int tcc_sdr_open(struct inode *inode_sdr, struct file *file_sdr)
{
	const struct miscdevice *misc = tcc_sdr_get_misc_device(file_sdr->private_data);
	struct tcc_sdr_t *sdr = tcc_sdr_dev_get_drvdata(misc->parent);
	int ret = 0;

	unused(inode_sdr);
	unused(file_sdr);

	if (sdr != NULL) {
		ret =
			request_irq(sdr->adma_irq, tcc_sdr_isr, 0x0, "tcc-sdr",
					(void *)sdr);

		if (sdr->opened == TRUE) {
			SDR_DBG("it is already opend.\n");
			ret = -EMFILE;
		} else {
			sdr->opened = TRUE;
		}
	} else {
		SDR_ERR("[%s] sdr is NULL.\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

static int tcc_sdr_release(struct inode *inode_sdr, struct file *file_sdr)
{
	const struct miscdevice *misc = tcc_sdr_get_misc_device(file_sdr->private_data);
	struct tcc_sdr_t *sdr = tcc_sdr_dev_get_drvdata(misc->parent);
	int ret = 0;
	const void *pret = NULL;

	unused(inode_sdr);
	unused(file_sdr);

	if (sdr != NULL) {
		pret = free_irq(sdr->adma_irq, sdr);
		if (pret == NULL) {
			SDR_DBG("%s: free_irq has wrong.\n", __func__);
		}

		ret = tcc_sdr_stop(sdr, FALSE);
		if (ret < 0) {
			SDR_DBG("%s: stop seq. has wrong.\n", __func__);
		}
		tcc_sdr_deinitialize(sdr);

		sdr->opened = FALSE;
		sdr->started = FALSE;

#ifdef SWRESET_OF_BUS
		if (sdr->iocfg_reg != NULL) {
			tcc_audio_sw_reset_enable(sdr->iocfg_reg, sdr->hrsten_reg_offset, sdr->hrsten_bit_offset, TRUE);
			tcc_sdr_u_delay(2000u);
			tcc_audio_sw_reset_enable(sdr->iocfg_reg, sdr->hrsten_reg_offset, sdr->hrsten_bit_offset, FALSE);
		}
#endif
	} else {
		SDR_ERR("[%s] sdr is NULL.\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

static const struct file_operations tcc_sdr_fops = {
	.owner = THIS_MODULE,
	.poll = tcc_sdr_poll,
	.unlocked_ioctl = tcc_sdr_ioctl,
	.open = tcc_sdr_open,
	.release = tcc_sdr_release,
};

static int parse_sdr_dt(const struct platform_device *pdev, struct tcc_sdr_t *sdr)
{
	struct device_node *of_node_adma;
	const char *devname = NULL;
	uint32_t block_type;
	int ret = 0;
	/* get dai info. */
	sdr->blk_no = of_alias_get_id(pdev->dev.of_node, "i2s");
	sdr->dai_reg = of_iomap(pdev->dev.of_node, 0);
	of_node_adma = of_parse_phandle(pdev->dev.of_node, "adma", 0);
	if ((IS_ERR((void *) sdr->dai_reg)) || (of_node_adma == NULL)) {
		sdr->dai_reg = NULL;
		SDR_DBG("dai_reg or of_node_adma is NULL\n");
		ret = -EINVAL;
	} else {
		SDR_DBG("dai_reg=%p\n", sdr->dai_reg);

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) ||\
	defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
		sdr->dai_pclk = of_clk_get(pdev->dev.of_node, 0);
		sdr->dai_hclk = of_clk_get(pdev->dev.of_node, 1);
		sdr->dai_filter_clk = of_clk_get(pdev->dev.of_node, 2);
		if ((sdr->dai_pclk == NULL) ||
			(sdr->dai_hclk == NULL) ||
			(sdr->dai_filter_clk == NULL)) {
			SDR_ERR("One of dai_pclk/hclk/filter_clk value is not exist.\n");
		}
#else
		sdr->dai_pclk = of_clk_get(pdev->dev.of_node, 0);
		sdr->dai_filter_clk = of_clk_get(pdev->dev.of_node, 1);
		if ((sdr->dai_pclk == NULL) ||
			(sdr->dai_filter_clk == NULL)) {
			SDR_ERR("One of dai_pclk/filter_clk value is not exist.\n");
		}
#endif

		if (of_property_read_u32
				(pdev->dev.of_node, "clock-frequency", &sdr->dai_clk_rate[1]) < 0) {
			SDR_DBG("clock-frequency value is not exist.");
			SDR_DBG(" So, default value is set.\n");
			sdr->dai_clk_rate[1] = 48000u;
		}
		sdr->dai_clk_rate[0] =
			(sdr->dai_clk_rate[1] > 48000u) ? 48000u : sdr->dai_clk_rate[1];
		sdr->dai_clk_rate[1] = 1u;
		SDR_DBG("clk_rate=%u\n", sdr->dai_clk_rate[0]);

		/* get adma info */
		sdr->adma_reg = of_iomap(of_node_adma, 0);
		if (IS_ERR((void *) sdr->adma_reg)) {
			sdr->adma_reg = NULL;
		} else {
			SDR_DBG("adma_reg=%p\n", sdr->adma_reg);
		}

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) ||\
	defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
		sdr->adma_hclk = of_clk_get(of_node_adma, 0);
		if (sdr->adma_hclk == NULL) {
			SDR_ERR("adma_hclk value is not exist.\n");
		}
#endif

		sdr->adma_irq = si_to_ui(of_irq_get(of_node_adma, 0));
#ifdef SWRESET_OF_BUS
		sdr->iocfg_reg = of_iomap(of_node_adma, 1);
		if (IS_ERR((void *) sdr->iocfg_reg)) {
			sdr->iocfg_reg = NULL;
			SDR_DBG("iocfg_reg is NULL\n");
		} else {
			SDR_DBG("iocfg_reg=%p\n", sdr->iocfg_reg);
			sdr->hrsten_reg_offset = 0u;
			ret = of_property_read_u32(of_node_adma, "hrsten-reg-offset", &sdr->hrsten_reg_offset);
			ret = of_property_read_u32(of_node_adma, "hrsten-bit-offset", &sdr->hrsten_bit_offset);
			if ((ret == 0) &&
				(sdr->hrsten_bit_offset != 0u) &&
				(sdr->hrsten_reg_offset != 0u)) {
				SDR_DBG("hrsten_reg_offset : 0x%08x\n", sdr->hrsten_reg_offset);
				SDR_DBG("hrsten_bit_offset : %u\n", sdr->hrsten_bit_offset);
			}
		}
#endif
		ret = of_property_read_u32(of_node_adma, "adrcnt-mode", &sdr->adrcnt_mode);
		if (ret < 0) {
			sdr->adrcnt_mode = 0u;
		}

		if (of_property_read_string(pdev->dev.of_node, "dev-name", &devname) ==
				0) {
			sdr->misc_dev->name = devname;
			SDR_DBG("dev-name : %s\n", devname);
		} else {
			SDR_DBG("default dev-name\n");
		}

		ret = of_property_read_u32(pdev->dev.of_node, "block-type", &block_type);
		if (ret >= 0) {
			sdr->dev_type =
				(block_type == (uint32_t)DAI_BLOCK_STEREO_TYPE) ?
				TCC_ADMA_I2S_STEREO :
				(block_type == (uint32_t)DAI_BLOCK_7_1CH_TYPE) ?
				TCC_ADMA_I2S_7_1CH : TCC_ADMA_I2S_9_1CH;
		}
	}

	return ret;
}

static int tcc_sdr_probe(struct platform_device *pdev)
{
	int ret = 0;
	bool rxdma_en = FALSE;
	struct tcc_sdr_t *sdr = NULL;

	SDR_DBG("%s\n", __func__);

	sdr = (struct tcc_sdr_t *) kzalloc(sizeof(struct tcc_sdr_t), GFP_KERNEL);
	if (sdr != NULL) {
		(void) memset(sdr, 0, sizeof(struct tcc_sdr_t));

		sdr->misc_dev = (struct miscdevice *) kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
		if (sdr->misc_dev != NULL) {
			sdr->misc_dev->parent = &pdev->dev;
			sdr->misc_dev->minor = MISC_DYNAMIC_MINOR;
			sdr->misc_dev->name = "tcc-sdr";
			sdr->misc_dev->fops = &tcc_sdr_fops;
			ret = parse_sdr_dt(pdev, sdr);
		} else {
			SDR_WARN("%s : Fail to alloc misc_dev\n",
					__func__);

			ret = -ENOMEM;
		}

		if (ret < 0) {
			SDR_WARN("%s : Fail to parse sdr dt\n",
					__func__);
		} else {
#ifdef PREALLOCATE_DMA_BUFFER_MODE
			uint32_t i;
#endif
			//setup default cfg value
			sdr->ports_num = IQ_MODE_DEFAULT_CHANNEL;
			sdr->bitmode = (uint32_t) IQ_MODE_DEFAULT_BITMODE;
			sdr->buffer_bytes = TCC_SDR_BUFFER_SZ_MAX;

			platform_set_drvdata(pdev, sdr);
			sdr->pdev = pdev;

			init_waitqueue_head(&sdr->wq);
			mutex_init(&sdr->m);
			spin_lock_init(&sdr->lock);

#ifdef PREALLOCATE_DMA_BUFFER_MODE
			for (i = 0u; i < SDR_MAX_PORT_NUM; i++) {
				sdr->port[i].dma_vaddr =
					dma_alloc_coherent(&sdr->pdev->dev, TCC_SDR_BUFFER_SZ_MAX,
							&sdr->port[i].dma_paddr, GFP_KERNEL);
				if (sdr->port[i].dma_vaddr == NULL) {
					SDR_DBG("[%d]th dma memory allocation failed\n", i);
					ret = -ENOMEM;
				} else {
					SDR_DBG("dma_vaddr[%d] : %p\n",
							i, (void *)sdr->port[i].dma_vaddr);
				}
			}
#endif
			rxdma_en = tcc_adma_dai_rx_dma_enable_check(sdr->adma_reg);
			if (rxdma_en) {
				bool adrcnt_en = tcc_adma_dai_rx_dma_adrcnt_mode_check(sdr->adma_reg);

				SDR_WARN("This is for robust booting.\n");

				sdr->iq_mode = (adrcnt_en == FALSE)? TRUE:FALSE;

				ret = tcc_sdr_stop(sdr, rxdma_en);
				if (ret < 0) {
					SDR_WARN("%s: stop seq. has wrong.\n", __func__);
				} else {
					SDR_DBG("after tcc_sdr_stop ret=%d.\n", ret);

					sdr->iq_mode = FALSE;
					ret = 0;
				}
			}
			sdr->opened = FALSE;

			if (misc_register(sdr->misc_dev) != 0) {
				SDR_WARN("Couldn't register misc device.\n");
				ret = -EBUSY;
			}
		}
	} else {
		SDR_ERR("[%s] sdr is NULL.\n", __func__);
		ret = -ENOMEM;
	}

	if (ret < 0) {
		if ((sdr != NULL) && (sdr->misc_dev != NULL)) {
			kfree(sdr->misc_dev);
		}
		if (sdr != NULL) {
			kfree(sdr);
		}
	}
	return ret;
}

static int tcc_sdr_remove(struct platform_device *pdev)
{
	struct tcc_sdr_t *sdr = (struct tcc_sdr_t *)platform_get_drvdata(pdev);
	uint32_t i;

	SDR_DBG("%s\n", __func__);

	for (i = 0u; i < sdr->ports_num; i++) {
		if (sdr->port[i].dma_vaddr != NULL) {
			dma_free_coherent(&pdev->dev, TCC_SDR_BUFFER_SZ_MAX,
					sdr->port[i].dma_vaddr, sdr->port[i].dma_paddr);
		}
	}

	kfree(sdr->misc_dev);
	sdr->misc_dev = NULL;

	kfree(sdr);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int tcc_sdr_suspend(struct platform_device *pdev, pm_message_t state)
{
	//struct tcc_sdr_t *sdr =
	//	(struct tcc_sdr_t*)platform_get_drvdata(pdev);
	struct pinctrl *sdr_pinctrl = NULL;

	unused(state);

	SDR_DBG("%s\n", __func__);

	sdr_pinctrl = pinctrl_get_select(&pdev->dev, "idle");
	if (sdr_pinctrl == NULL) {
		SDR_ERR("%s : pinctrl suspend error[0x%p]\n",
			__func__, sdr_pinctrl);
	}

	//sdr->started = FALSE;
	return 0;
}

static int tcc_sdr_resume(struct platform_device *pdev)
{
	//struct tcc_sdr_t *sdr =
	//	(struct tcc_sdr_t*)platform_get_drvdata(pdev);

	struct pinctrl *sdr_pinctrl = NULL;

	SDR_DBG("%s\n", __func__);
	//tcc_sdr_start(sdr, GFP_ATOMIC);

	sdr_pinctrl = pinctrl_get_select(&pdev->dev, "default");
	if (sdr_pinctrl == NULL) {
		SDR_ERR("%s : pinctrl resume error[0x%p]\n",
			__func__, sdr_pinctrl);
	}

	return 0;
}

static const struct of_device_id tcc_sdr_of_match[] = {
	{.compatible = "telechips,sdr"},
	{}
};

MODULE_DEVICE_TABLE(of, tcc_sdr_of_match);

static struct platform_driver tcc_sdr_driver = {
	.probe = tcc_sdr_probe,
	.remove = tcc_sdr_remove,
	.suspend = tcc_sdr_suspend,
	.resume = tcc_sdr_resume,
	.driver = {
		.name = "tcc_sdr_drv",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(tcc_sdr_of_match),
#endif
	},
};

static int __init tcc_sdr_init(void)
{
	SDR_DBG("%s\n", __func__);
	return platform_driver_register(&tcc_sdr_driver);
}

static void __exit tcc_sdr_exit(void)
{
	SDR_DBG("%s\n", __func__);
	platform_driver_unregister(&tcc_sdr_driver);
}

//------------------------------------------------------

module_init(tcc_sdr_init);
module_exit(tcc_sdr_exit);

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("Telechips SDR Driver");
MODULE_LICENSE("GPL");
