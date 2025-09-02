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

#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "tcc_asrc_dai.h"
#include "tcc_asrc_pcm.h"
#include "tcc_asrc_drv.h"

#define unused(x) (void)(x)
#undef asrc_pcm_dbg
#if 0
#define asrc_pcm_dbg(a...)\
        (void) pr_info("[DEBUG][ASRC_PCM] " a)
#else
#define asrc_pcm_dbg(a...)
#endif
#define asrc_pcm_err(a...)\
        (void) pr_info("[ERROR][ASRC_PCM] " a)

#define MAX_BUFFER_BYTES\
	(262144U)

#define MIN_PERIOD_BYTES\
	(256U)
#define MIN_PERIOD_CNT\
	(2U)
#define MAX_PERIOD_CNT\
	(MAX_BUFFER_BYTES / MIN_PERIOD_BYTES)

//#define LLI_DEBUG

#ifdef LLI_DEBUG
static void tcc_pl080_dump_txbuf_lli(
	struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	int cnt)
{
	int i;

	for (i = 0; i < cnt; i++) {
		pr_info("pair[%d].txbuf.lli[%d].src_addr : 0x%08x\n",
			asrc_pair,
			i,
			asrc->pair[asrc_pair].txbuf.lli_virt[i].src_addr);
		pr_info("pair[%d].txbuf.lli[%d].dst_addr : 0x%08x\n",
			asrc_pair,
			i,
			asrc->pair[asrc_pair].txbuf.lli_virt[i].dst_addr);
		pr_info("pair[%d].txbuf.lli[%d].next_lli : 0x%08x\n",
			asrc_pair,
			i,
			asrc->pair[asrc_pair].txbuf.lli_virt[i].next_lli);
		pr_info("pair[%d].txbuf.lli[%d].control0 : 0x%08x\n",
			asrc_pair,
			i,
			asrc->pair[asrc_pair].txbuf.lli_virt[i].control0);
		pr_info("\n");
	}
}

static void tcc_pl080_dump_rxbuf_lli(
	struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	int cnt)
{
	int i;

	for (i = 0; i < cnt; i++) {
		pr_info("pair[%d].rxbuf.lli[%d].src_addr : 0x%08x\n",
			asrc_pair,
			i,
			asrc->pair[asrc_pair].rxbuf.lli_virt[i].src_addr);
		pr_info("pair[%d].rxbuf.lli[%d].dst_addr : 0x%08x\n",
			asrc_pair,
			i,
			asrc->pair[asrc_pair].rxbuf.lli_virt[i].dst_addr);
		pr_info("pair[%d].rxbuf.lli[%d].next_lli : 0x%08x\n",
			asrc_pair,
			i,
			asrc->pair[asrc_pair].rxbuf.lli_virt[i].next_lli);
		pr_info("pair[%d].rxbuf.lli[%d].control0 : 0x%08x\n",
			asrc_pair,
			i,
			asrc->pair[asrc_pair].rxbuf.lli_virt[i].control0);
		pr_info("\n");
	}
}
#endif

static int tcc_pl080_setup_tx_ring(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	uint32_t period_bytes,
	uint32_t buffer_bytes)
{
	uint32_t remain_bytes = buffer_bytes;
	uint32_t transfer_bytes;
	uint32_t idx = 0;
	struct tcc_pl080_ctl_info_t tcc_pl080_ctl_info;

	/*MISRA C-2012 Rule 2.2*/
	//tcc_pl080_ctl_info.transfer_size = 0U;
	tcc_pl080_ctl_info.src_width = TCC_PL080_WIDTH_32BIT; //src_width
	tcc_pl080_ctl_info.src_incr = (bool)true; //src_incr
	tcc_pl080_ctl_info.dst_width = TCC_PL080_WIDTH_32BIT; //dst_width
	tcc_pl080_ctl_info.dst_incr = (bool)false; //dst_incr
	tcc_pl080_ctl_info.src_bsize = TCC_PL080_BSIZE_1; //src_burst_size
	tcc_pl080_ctl_info.dst_bsize = TCC_PL080_BSIZE_1; //dst_burst_size
	tcc_pl080_ctl_info.irq_en = (bool)true; //irq_en

	while (remain_bytes > 0U) {
		transfer_bytes = (remain_bytes > period_bytes) ?
			period_bytes : remain_bytes;

		asrc->pair[asrc_pair].txbuf.lli_virt[idx].src_addr =
			ull_to_ui(asrc->pair[asrc_pair].txbuf.phys_addr) + ui_to_ui_mul(idx, period_bytes);
		asrc->pair[asrc_pair].txbuf.lli_virt[idx].dst_addr =
			get_fifo_in_phys_addr(asrc->asrc_reg_phys, asrc_pair);

		tcc_pl080_ctl_info.transfer_size = transfer_bytes / TRANSFER_UNIT_BYTES;
		asrc->pair[asrc_pair].txbuf.lli_virt[idx].control0 =
			tcc_pl080_lli_control_value(&tcc_pl080_ctl_info);
		asrc->pair[asrc_pair].txbuf.lli_virt[idx].next_lli =
			(remain_bytes > period_bytes) ?
				tcc_asrc_txbuf_lli_phys_address(
					asrc,
					asrc_pair,
					ui_add(idx, 1)) :
				tcc_asrc_txbuf_lli_phys_address(
					asrc,
					asrc_pair,
					0);

		remain_bytes -= transfer_bytes;
		idx = ui_add(idx, 1);
	}

#ifdef LLI_DEBUG
	tcc_pl080_dump_txbuf_lli(asrc, asrc_pair, idx);
#endif
	return 0;
}

static int tcc_pl080_setup_rx_ring(
	const struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	uint32_t period_bytes,
	uint32_t buffer_bytes)
{
	uint32_t remain_bytes = buffer_bytes;
	uint32_t transfer_bytes;
	uint32_t idx = 0;
	struct tcc_pl080_ctl_info_t tcc_pl080_ctl_info;

	/*MISRA C-2012 Rule 2.2*/
	//tcc_pl080_ctl_info.transfer_size = 0U;
	tcc_pl080_ctl_info.src_width = TCC_PL080_WIDTH_32BIT; //src_width
	tcc_pl080_ctl_info.src_incr = (bool)false; //src_incr
	tcc_pl080_ctl_info.dst_width = TCC_PL080_WIDTH_32BIT; //dst_width
	tcc_pl080_ctl_info.dst_incr = (bool)true; //dst_incr
	tcc_pl080_ctl_info.src_bsize = TCC_PL080_BSIZE_1; //src_burst_size
	tcc_pl080_ctl_info.dst_bsize = TCC_PL080_BSIZE_1; //dst_burst_size
	tcc_pl080_ctl_info.irq_en = (bool)true; //irq_en

	while (remain_bytes > 0U) {
		transfer_bytes = (remain_bytes > period_bytes) ?
			period_bytes : remain_bytes;

		asrc->pair[asrc_pair].rxbuf.lli_virt[idx].src_addr =
			get_fifo_out_phys_addr(asrc->asrc_reg_phys, asrc_pair);

		asrc->pair[asrc_pair].rxbuf.lli_virt[idx].dst_addr =
			ull_to_ui(asrc->pair[asrc_pair].rxbuf.phys_addr) + ui_to_ui_mul(idx, period_bytes);

		tcc_pl080_ctl_info.transfer_size = transfer_bytes / TRANSFER_UNIT_BYTES;
		asrc->pair[asrc_pair].rxbuf.lli_virt[idx].control0 =
			tcc_pl080_lli_control_value(&tcc_pl080_ctl_info);

		asrc->pair[asrc_pair].rxbuf.lli_virt[idx].next_lli =
			(remain_bytes > period_bytes) ?
			tcc_asrc_rxbuf_lli_phys_address(
				asrc,
				asrc_pair,
				ui_add(idx, 1)) :
			tcc_asrc_rxbuf_lli_phys_address(
				asrc,
				asrc_pair,
				0);

		remain_bytes -= transfer_bytes;
		idx = ui_add(idx, 1);
	}

#ifdef LLI_DEBUG
	tcc_pl080_dump_rxbuf_lli(asrc, asrc_pair, idx);
#endif
	return 0;
}

static const unsigned int tcc_rates[] = {
	8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000, 64000,
	88200, 96000, 176400, 192000
};

static struct snd_pcm_hw_constraint_list tcc_constraints_rates = {
	.count = ARRAY_SIZE(tcc_rates),
	.list = tcc_rates,
	.mask = 0,
};

int tcc_asrc_pcm_open(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct tcc_asrc_t *asrc =
		(struct tcc_asrc_t *)snd_soc_component_get_drvdata(
				component);
	struct snd_pcm_hardware tcc_asrc_pcm = {
		.info = (SNDRV_PCM_INFO_MMAP
				| SNDRV_PCM_INFO_MMAP_VALID
				| SNDRV_PCM_INFO_INTERLEAVED
				| SNDRV_PCM_INFO_BLOCK_TRANSFER
				| SNDRV_PCM_INFO_PAUSE
				| SNDRV_PCM_INFO_RESUME),

		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		.rates        = si_to_ui(SNDRV_PCM_RATE_8000_192000),
		.rate_min     = 8000,
		.rate_max     = 192000,
		.channels_min = TCC_ASRC_MIN_CHANNELS,
		.channels_max = TCC_ASRC_MIN_CHANNELS,
		.period_bytes_min = MIN_PERIOD_BYTES,
		.period_bytes_max =
			ull_to_ui(ui_to_ull_mul(PL080_MAX_TRANSFER_SIZE,TRANSFER_UNIT_BYTES)),
		.periods_min      = MIN_PERIOD_CNT,
		.periods_max      = MAX_PERIOD_CNT,
		.buffer_bytes_max = MAX_BUFFER_BYTES,
		.fifo_size = 16,
	};
	uint32_t asrc_pair = si_to_ui(cpu_dai->id);

	asrc_pcm_dbg("%s\n", __func__);
	asrc_pcm_dbg("cpu_dai->id : %d\n", cpu_dai->id);

	(void)snd_pcm_hw_constraint_list(
			substream->runtime,
			0,
			SNDRV_PCM_HW_PARAM_RATE,
			&tcc_constraints_rates);

	(void)snd_pcm_hw_constraint_step(
		substream->runtime,
		0,
		SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
		TRANSFER_UNIT_BYTES);
	(void)snd_pcm_hw_constraint_step(
		substream->runtime,
		0,
		SNDRV_PCM_HW_PARAM_PERIOD_BYTES,
		TRANSFER_UNIT_BYTES);

	tcc_asrc_pcm.channels_max = asrc->pair[asrc_pair].hw.max_channel;

	(void)snd_soc_set_runtime_hwparams(substream, &tcc_asrc_pcm);
	asrc->pair[asrc_pair].m2m_stat.substream = substream;

	return 0;
}
EXPORT_SYMBOL(tcc_asrc_pcm_open);

int tcc_asrc_pcm_close(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct tcc_asrc_t *asrc =
		(struct tcc_asrc_t *)snd_soc_component_get_drvdata(
				component);

	uint32_t asrc_pair = si_to_ui(cpu_dai->id);
	asrc_pcm_dbg("%s\n", __func__);
	asrc->pair[asrc_pair].m2m_stat.substream = NULL;
	return 0;
}
EXPORT_SYMBOL(tcc_asrc_pcm_close);

int tcc_asrc_pcm_ioctl(struct snd_soc_component *component,
		struct snd_pcm_substream *substream,
		unsigned int cmd, void *arg)
{
	unused(component);
	asrc_pcm_dbg("%s\n", __func__);

	return snd_pcm_lib_ioctl(substream, cmd, arg);
}
EXPORT_SYMBOL(tcc_asrc_pcm_ioctl);

int tcc_asrc_pcm_mmap(
	struct snd_soc_component *component,
	struct snd_pcm_substream *substream,
	struct vm_area_struct *vma)
{
	const struct snd_pcm_runtime *runtime = substream->runtime;
	unused(component);

	asrc_pcm_dbg("%s\n", __func__);

	return dma_mmap_wc(
		substream->pcm->card->dev,
		vma,
		runtime->dma_area,
		runtime->dma_addr,
		runtime->dma_bytes);
}
EXPORT_SYMBOL(tcc_asrc_pcm_mmap);

int tcc_asrc_pcm_hw_params(
	struct snd_soc_component *component,
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	const struct tcc_asrc_t *asrc =
		(struct tcc_asrc_t *)snd_soc_component_get_drvdata(
				component);
	uint32_t asrc_pair = si_to_ui(cpu_dai->id);

	uint32_t period_bytes = params_period_bytes(params);
	uint32_t buffer_bytes = params_buffer_bytes(params);

	asrc_pcm_dbg("%s\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		(void)tcc_pl080_setup_tx_ring(
			asrc,
			asrc_pair,
			period_bytes,
			buffer_bytes);
	} else {
		(void)tcc_pl080_setup_rx_ring(
			asrc,
			asrc_pair,
			period_bytes,
			buffer_bytes);
	}

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	return 0;
}
EXPORT_SYMBOL(tcc_asrc_pcm_hw_params);

int tcc_asrc_pcm_hw_free( struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	unused(component);
	asrc_pcm_dbg("%s\n", __func__);

	(void)memset(substream->dma_buffer.area, 0, substream->dma_buffer.bytes);
	snd_pcm_set_runtime_buffer(substream, NULL);
	return 0;
}
EXPORT_SYMBOL(tcc_asrc_pcm_hw_free);

int tcc_asrc_pcm_prepare(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	unused(component);
        unused(substream);
	asrc_pcm_dbg("%s\n", __func__);
	return 0;
}
EXPORT_SYMBOL(tcc_asrc_pcm_prepare);

int tcc_asrc_pcm_trigger(struct snd_soc_component *component,
		struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	const struct tcc_asrc_t *asrc =
		(struct tcc_asrc_t *)snd_soc_component_get_drvdata(
				component);
	uint32_t asrc_pair = si_to_ui(cpu_dai->id);
	int ret = 0;

	asrc_pcm_dbg("%s(%d)\n", __func__, cpu_dai->id);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			asrc_pcm_dbg("TRIGGER_START, PLAY\n");
			(void)tcc_asrc_tx_dma_start(asrc, asrc_pair);
		} else {
			asrc_pcm_dbg("TRIGGER_START, CAPTURE\n");
			(void)tcc_asrc_rx_dma_start(asrc, asrc_pair);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			asrc_pcm_dbg("TRIGGER_STOP, PLAY\n");
			(void)tcc_asrc_tx_dma_stop(asrc, asrc_pair);
		} else {
			asrc_pcm_dbg("TRIGGER_STOP, CAPTURE\n");
			(void)tcc_asrc_rx_dma_stop(asrc, asrc_pair);
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(tcc_asrc_pcm_trigger);

snd_pcm_uframes_t tcc_asrc_pcm_pointer(
	struct snd_soc_component *component,
	struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct snd_pcm_runtime *runtime = substream->runtime;
	const struct tcc_asrc_t *asrc =
		(struct tcc_asrc_t *)snd_soc_component_get_drvdata(
				component);
	uint32_t asrc_pair = si_to_ui(cpu_dai->id);
	uint32_t dma_tx_ch = asrc_pair;
	uint32_t dma_rx_ch = ui_add(asrc_pair, ASRC_RX_DMA_OFFSET);
	uint32_t dma_cur;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		dma_cur = tcc_pl080_get_cur_src_addr(
			asrc->pl080_reg,
			dma_tx_ch);
	} else {
		dma_cur = tcc_pl080_get_cur_dst_addr(
			asrc->pl080_reg,
			dma_rx_ch);
	}

//	asrc_pcm_dbg("%s - dma_addr : 0x%08x, dma_cur : 0x%08x\n",
//	__func__, runtime->dma_addr, dma_cur);

	return sl_to_ul(bytes_to_frames(runtime, ui_to_si(ui_sub(dma_cur, ull_to_ui(runtime->dma_addr)))));
}
EXPORT_SYMBOL(tcc_asrc_pcm_pointer);

int tcc_asrc_pcm_new(struct snd_soc_component *component,
		struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct tcc_asrc_t *asrc =
		(struct tcc_asrc_t *)snd_soc_component_get_drvdata(
				component);
	uint32_t asrc_pair = si_to_ui(cpu_dai->id);

	struct snd_pcm_substream *play_substream =
		rtd->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	struct snd_pcm_substream *capture_substream =
		rtd->pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
	struct snd_dma_buffer *play_buf = &play_substream->dma_buffer;
	struct snd_dma_buffer *capture_buf = &capture_substream->dma_buffer;
	int ret = 0;

	do {
		asrc_pcm_dbg("%s\n", __func__);
		asrc_pcm_dbg("pair : %d\n", asrc_pair);

		if ((play_substream != NULL) && (play_buf != NULL)) {
			ret = snd_dma_alloc_pages(
				SNDRV_DMA_TYPE_DEV,
				rtd->card->dev,
				MAX_BUFFER_BYTES,
				play_buf);
			if (ret != 0) {
				continue;
			}

			(void)memset(play_buf->area, 0, play_buf->bytes);

			asrc->pair[asrc_pair].txbuf.phys_addr = play_buf->addr;
			asrc->pair[asrc_pair].txbuf.virt = play_buf->area;
			asrc->pair[asrc_pair].txbuf.lli_virt =
				dma_alloc_wc(
					rtd->card->dev,
					sizeof(struct pl080_lli)*MAX_PERIOD_CNT,
					&asrc->pair[asrc_pair].txbuf.lli_phys,
					GFP_KERNEL);
		}

		if ((capture_substream != NULL) && (capture_buf != NULL)) {
			ret = snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV,
				rtd->card->dev, MAX_BUFFER_BYTES, capture_buf);
			if (ret != 0) {
				continue;
			}

			(void)memset(capture_buf->area, 0, capture_buf->bytes);

			asrc->pair[asrc_pair].rxbuf.phys_addr = capture_buf->addr;
			asrc->pair[asrc_pair].rxbuf.virt = capture_buf->area;
			asrc->pair[asrc_pair].rxbuf.lli_virt =
				dma_alloc_wc(
					rtd->card->dev,
					sizeof(struct pl080_lli)*MAX_PERIOD_CNT,
					&asrc->pair[asrc_pair].rxbuf.lli_phys,
					GFP_KERNEL);
		}

		ret = 0;
	} while (false);

	return ret;
}
EXPORT_SYMBOL(tcc_asrc_pcm_new);

void tcc_asrc_pcm_free_dma_buffers(struct snd_soc_component *component,
		struct snd_pcm *pcm)
{
	struct snd_pcm_substream *play_substream =
		pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	struct snd_pcm_substream *capture_substream =
		pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;

	struct snd_dma_buffer *play_buf = &play_substream->dma_buffer;
	struct snd_dma_buffer *capture_buf = &capture_substream->dma_buffer;
	unused(component);

	asrc_pcm_dbg("%s\n", __func__);

	if ((play_substream != NULL) && (play_buf != NULL)) {
		snd_dma_free_pages(play_buf);
	}

	if ((capture_substream != NULL) && (capture_buf != NULL)) {
		snd_dma_free_pages(capture_buf);
	}

}
EXPORT_SYMBOL(tcc_asrc_pcm_free_dma_buffers);

int tcc_pl080_asrc_pcm_isr_ch(const struct tcc_asrc_t *asrc, uint32_t asrc_pair)
{
//	asrc_pcm_dbg("%s - pair:%d\n", __func__, asrc_pair);
	snd_pcm_period_elapsed(asrc->pair[asrc_pair].m2m_stat.substream);

	return 0;
}
EXPORT_SYMBOL(tcc_pl080_asrc_pcm_isr_ch);

#if 0
static struct snd_soc_component_driver tcc_asrc_pcm_component = {
	.name           = DRV_NAME,
	.open           = tcc_asrc_pcm_open,
	.close          = tcc_asrc_pcm_close,
	.ioctl          = tcc_asrc_pcm_ioctl,
	.hw_params      = tcc_asrc_pcm_hw_params,
	.hw_free        = tcc_asrc_pcm_hw_free,
	.prepare        = tcc_asrc_pcm_prepare,
	.trigger        = tcc_asrc_pcm_trigger,
	.pointer        = tcc_asrc_pcm_pointer,
	.mmap           = tcc_asrc_pcm_mmap,
	.pcm_construct  = tcc_asrc_pcm_new,
	.pcm_destruct   = tcc_asrc_pcm_free_dma_buffers,
};

static struct snd_soc_dai_driver tcc_asrc_pcm_dai_drv[] = {
	{
		.name = "tcc-asrc-pcm",
		.playback = {
			.stream_name = "TCC-ASRC-Playback",
			.channels_min = 2,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats =
				(SNDRV_PCM_FMTBIT_S16_LE
				 |SNDRV_PCM_FMTBIT_S24_LE),
		},
		.capture = {
			.stream_name = "TCC-ASRC-Capture",
			.channels_min = 2,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_192000,
			.formats =
				(SNDRV_PCM_FMTBIT_S16_LE
				 |SNDRV_PCM_FMTBIT_S24_LE),
		},
	},
};

int tcc_asrc_pcm_drvinit(struct platform_device *pdev)
{
	return snd_soc_register_component(&pdev->dev, &tcc_asrc_pcm_component,
					tcc_asrc_pcm_dai_drv, ui_to_si(TCC_ASRC_ARRAY_SIZE(tcc_asrc_pcm_dai_drv)));
}
#endif

