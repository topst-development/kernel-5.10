// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/platform_device.h>
#include <linux/dmaengine.h>
#include <linux/types.h>

#include "dmaengine.h"

#define TCC_MAX_DMA_CHANNELS	(5U)
#define TCC_DMA_MAX_XFER_CNT	(0xFFFFUL)
#define TCC_MAX_DMA_SLAVES	(64U)

#define DMA_CH_REG_OFF		(0x30U)

#define DMA_ST_SADR		(0x0)	/* Start Source Address */
#define DMA_SPARAM		(0x4)	/* Source Block Parameter */
  #define DMA_SPARAM_SMASK(x)	(((x) & 0xFFFFFFUL) << 8U)
  #define DMA_SPARAM_SINC(x)	((x) & 0xFFU)
#define DMA_C_SADR		(0xC)	/* Current Source Address */
#define DMA_ST_DADR		(0x10)	/* Start Destination Address */
#define DMA_DPARAM		(0x14)	/* Destination Block Parameter */
  #define DMA_DPARAM_DMASK(x)	(((x) & 0xFFFFFFU) << 8U)
  #define DMA_DPARAM_DINC(x)	((x) & 0xFFU)
#define DMA_C_DADR		(0x1C)	/* Current Destination Address */
#define DMA_HCOUNT		(0x20)	/* HOP Count */
  #define DMA_HCOUNT_ST_HCOUNT(x) ((x) & 0xFFFFU)
#define DMA_CHCTRL		(0x24)	/* Channel Control */
  #define DMA_CHCTRL_CONT	((u32)1U << 15U)
  #define DMA_CHCTRL_DTM	((u32)1U << 14U)
  #define DMA_CHCTRL_SYNC	((u32)1U << 13U)
  #define DMA_CHCTRL_HRD	((u32)1U << 12U)
  #define DMA_CHCTRL_LOCK	((u32)1U << 11U)
  #define DMA_CHCTRL_BST	((u32)1U << 10U)
  #define DMA_CHCTRL_TYPE(x)	((u32)(x) << 8U)
  #define DMA_CHCTRL_BSIZE(x)	((u32)(x) << 6U)
  #define DMA_CHCTRL_WSIZE(x)	((u32)(x) << 4U)
  #define DMA_CHCTRL_FLAG	((u32)1U << 3U)
  #define DMA_CHCTRL_IEN	((u32)1U << 2U)
  #define DMA_CHCTRL_REP	((u32)1U << 1U)
  #define DMA_CHCTRL_EN		((u32)1U << 0U)
#define DMA_RPTCTRL		(0x28)	/* Repeat Control */
#define DMA_EXTREQ		(0x2C)	/* External DMA Request */

#define DMA_CHCONFIG		(0x90)	/* Channel Configuration */
  #define DMA_CHCONFIG_IS(x, n)		(((x) >> 20U) & ((u32)1U << (n)))
  #define DMA_CHCONFIG_MIS(x, n)	(((x) >> 16U) & ((u32)1U << (n)))

#define DMA_AC0_START		(0x00)
#define DMA_AC0_LIMIT		(0x04)
#define DMA_AC1_START		(0x08)
#define DMA_AC1_LIMIT		(0x0C)
#define DMA_AC2_START		(0x10)
#define DMA_AC2_LIMIT		(0x14)
#define DMA_AC3_START		(0x18)
#define DMA_AC3_LIMIT		(0x1C)

/*
 * struct dma_chan *chan
 * &(chan->dev->device)
 */
#define chan2dev(x) (&(x)->dev->device)

struct tcc_dma_soc_data {
	const u64 sync;	/* hardware request synchronization for a peripheral */
};

struct tcc_dma_desc {
	struct list_head dma_node;
	struct list_head tx_list;
	struct dma_async_tx_descriptor txd;

	u32 src_addr;
	u32 src_inc;
	u32 dst_addr;
	u32 dst_inc;
	u32 len;

	enum dma_slave_buswidth width;
	u32 burst;
	u32 slave_id;
};

struct tcc_dma_chan {
	struct dma_chan chan;
	struct dma_pool *desc_pool;
	struct device *dev;
	struct tasklet_struct tasklet;

	struct dma_slave_config slave_config;
	const struct tcc_dma_soc_data *soc;

	struct list_head desc_submitted;
	struct list_head desc_issued;
	struct list_head desc_completed;

	spinlock_t lock;

	enum dma_transfer_direction direction;
	void __iomem *ch_base;	/* base address of a dma channel */
	u32 slave_id;
	u64 slave_bit;
};

struct tcc_dma {
	struct device *dev;
	struct dma_device dma;
	void __iomem *regs;	/* dma controller base address */
	void __iomem *req_reg;	/* dma request register address */
	void __iomem *ac_reg;	/* address access controler register address */
	struct clk *hclk;	/* iobus ahb clock */
	u32 ac_val[4][2];

	int irq;
	u32 dma_channels;
	const struct tcc_dma_soc_data *soc;

	struct tcc_dma_chan *chan;
};

struct tcc_dma_filter_data {
	struct tcc_dma *tdma;
	u32 chan_id;
	u32 slave_id;
	u64 slave_bit;
};

static inline struct tcc_dma_chan *to_tcc_dma_chan(struct dma_chan *chan)
{
	return container_of(chan, struct tcc_dma_chan, chan);
}

static inline struct tcc_dma_desc *to_tcc_dma_desc(
					struct dma_async_tx_descriptor *txd)
{
	return container_of(txd, struct tcc_dma_desc, txd);
}

static inline u32 tcc_readl(const void __iomem *addr, u32 off)
{
	u32 val;

	val = readl(addr + off);

	return val;
}

static inline void tcc_writel(void __iomem *addr, u32 off, u32 val)
{
	writel(val, addr + off);
}

static inline u32 ch_readl(const struct tcc_dma_chan *tdmac, u32 off)
{
	u32 val;

	val = tcc_readl(tdmac->ch_base, off);

	return val;
}

static inline void ch_writel(const struct tcc_dma_chan *tdmac,
		u32 off, u32 val)
{
	tcc_writel(tdmac->ch_base, off, val);
}

/*
 * Peak at the descriptor from the list.
 */
static struct tcc_dma_desc *tcc_dma_get_desc(const struct list_head *phead)
{
	struct tcc_dma_desc *ret;

	if (list_empty(phead) == 0) {
		/* Get a descriptor from phead list */
		ret = list_first_entry(phead, struct tcc_dma_desc, dma_node);
	} else {
		ret = NULL;
	}

	return ret;
}

static void tcc_dma_desc_free_list(const struct tcc_dma_chan *tdmac,
		struct list_head *head)
{
	struct tcc_dma_desc *desc;
	struct tcc_dma_desc *desc_tmp;

	list_for_each_entry_safe(desc, desc_tmp, head, dma_node) {
		list_del(&desc->dma_node);
		dma_pool_free(tdmac->desc_pool, (void *)desc, desc->txd.phys);
	}
	INIT_LIST_HEAD(head);
}

static void tcc_dma_cleanup_completed_desc(struct tcc_dma_chan *tdmac)
{
	struct tcc_dma_desc *desc;

	desc = tcc_dma_get_desc(&tdmac->desc_completed);
	if (desc != NULL) {
		dma_cookie_complete(&desc->txd);

		if (desc->txd.callback != NULL) {
			/* the callback is only called when dma xfer is done. */
			desc->txd.callback(desc->txd.callback_param);
		}

		tcc_dma_desc_free_list(tdmac, &tdmac->desc_completed);
	}
}

static u32 tcc_dma_width(const struct tcc_dma_chan *tdmac,
		enum dma_slave_buswidth width)
{
	u32 ret;

	switch (width) {
	case DMA_SLAVE_BUSWIDTH_1_BYTE:
		ret = DMA_CHCTRL_WSIZE(0U);
		break;
	case DMA_SLAVE_BUSWIDTH_2_BYTES:
		ret = DMA_CHCTRL_WSIZE(1U);
		break;
	case DMA_SLAVE_BUSWIDTH_4_BYTES:
		ret = DMA_CHCTRL_WSIZE(2U);
		break;
	default:
		dev_warn(chan2dev(&tdmac->chan),
				"[WARN][GDMA] %d width is not supported. Set 1 width.\n",
				width);
		ret = DMA_CHCTRL_WSIZE(0U);
		break;
	}

	return ret;
}

static u32 tcc_dma_burst(const struct tcc_dma_chan *tdmac, u32 burst)
{
	u32 ret;

	switch (burst) {
	case 1:
		ret = DMA_CHCTRL_BSIZE(0U);
		break;
	case 2:
		ret = DMA_CHCTRL_BSIZE(1U);
		break;
	case 4:
		ret = DMA_CHCTRL_BSIZE(2U);
		break;
	case 8:
		ret = DMA_CHCTRL_BSIZE(3U);
		break;
	default:
		dev_warn(chan2dev(&tdmac->chan),
				"[WARN][GDMA] %u burst is not supported. Set 1 burst.\n",
				burst);
		ret = DMA_CHCTRL_BSIZE(0U);
		break;
	}

	return ret;
}

static void tcc_dma_init_hop_cnt(const struct tcc_dma_chan *tdmac,
		const struct tcc_dma_desc *desc)
{
	/*
	 * Re-setting current count address
	 * In this time, dma read 1 data from source address but doesn't write
	 * data to destination address.
	 * So, source address must be memory address not peripheral register.
	 */
	ch_writel(tdmac, DMA_CHCTRL, 0);
	if (tdmac->direction == DMA_DEV_TO_MEM) {
		ch_writel(tdmac, DMA_ST_SADR, desc->dst_addr);
		ch_writel(tdmac, DMA_ST_DADR, desc->dst_addr);
	} else {
		ch_writel(tdmac, DMA_ST_SADR, desc->src_addr);
		ch_writel(tdmac, DMA_ST_DADR, desc->src_addr);
	}

	ch_writel(tdmac, DMA_HCOUNT, 0);
	ch_writel(tdmac, DMA_CHCTRL, 0x201);
	ch_writel(tdmac, DMA_CHCTRL, DMA_CHCTRL_FLAG);
}

static void tcc_dma_do_single_block(const struct tcc_dma_chan *tdmac,
		const struct tcc_dma_desc *desc)
{
	u32 chctrl;
	u32 hop_cnt;
	u32 slave_bit;

	dev_vdbg(chan2dev(&tdmac->chan),
			"[DEBUG][GDMA] src=%#08x, dest=%#08x, len=%d\n",
			desc->src_addr, desc->dst_addr, desc->len);

	tcc_dma_init_hop_cnt(tdmac, desc);

	/* Configure dma channel for current issue */
	ch_writel(tdmac, DMA_ST_SADR, desc->src_addr);
	ch_writel(tdmac, DMA_SPARAM, DMA_SPARAM_SINC(desc->src_inc));
	ch_writel(tdmac, DMA_ST_DADR, desc->dst_addr);
	ch_writel(tdmac, DMA_DPARAM, DMA_DPARAM_DINC(desc->dst_inc));

	hop_cnt = desc->len / desc->burst;
	ch_writel(tdmac, DMA_HCOUNT, DMA_HCOUNT_ST_HCOUNT(hop_cnt));

	slave_bit = (tdmac->slave_bit >= 0xFFFFFFFFU) ?
		(u32)(tdmac->slave_bit >> 32U) : (u32)tdmac->slave_bit;
	ch_writel(tdmac, DMA_EXTREQ, slave_bit);

	chctrl = DMA_CHCTRL_IEN | DMA_CHCTRL_WSIZE(0U) |
		DMA_CHCTRL_BSIZE(0U) | DMA_CHCTRL_FLAG |
		DMA_CHCTRL_TYPE(0U) | DMA_CHCTRL_EN;

	if (tdmac->slave_bit == 0U) {
		/* mem to mem copy */
		chctrl |= DMA_CHCTRL_TYPE(2U);
	} else {
		chctrl |= DMA_CHCTRL_TYPE(3U);
		if ((tdmac->soc->sync & tdmac->slave_bit) != 0U) {
			/* Some peripheral needs hardware synchronization. */
			chctrl |= DMA_CHCTRL_SYNC;
		}
	}

	chctrl |= tcc_dma_width(tdmac, desc->width);
	chctrl |= tcc_dma_burst(tdmac, desc->burst);

	ch_writel(tdmac, DMA_CHCTRL, chctrl);
}

static void tcc_dma_tasklet(unsigned long data)
{
	struct tcc_dma_chan *tdmac = (struct tcc_dma_chan *)data;
	struct tcc_dma_desc *desc;
	const struct tcc_dma_desc *desc_next;
	unsigned long flags;

	spin_lock_irqsave(&tdmac->lock, flags);

	desc = tcc_dma_get_desc(&tdmac->desc_issued);
	if (desc != NULL) {
		list_move_tail(&desc->dma_node, &tdmac->desc_completed);

		desc_next = tcc_dma_get_desc(&tdmac->desc_issued);
		if (desc_next != NULL) {
			tcc_dma_do_single_block(tdmac, desc_next);
			spin_unlock_irqrestore(&tdmac->lock, flags);
		} else {
			spin_unlock_irqrestore(&tdmac->lock, flags);
			tcc_dma_cleanup_completed_desc(tdmac);
		}
	}
}

static irqreturn_t tcc_dma_interrupt(int irq, void *data)
{
	const struct tcc_dma *tdma = (struct tcc_dma *)data;
	u32 ch_idx;
	irqreturn_t ret;

	if (tdma == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdma is NULL(irq: %d)\n",
				__func__, irq);
		ret = IRQ_NONE;
	} else {
		for (ch_idx = 0U; ch_idx < tdma->dma.chancnt; ch_idx++) {
			struct tcc_dma_chan *tdmac = &tdma->chan[ch_idx];
			u32 chconfig;

			chconfig = tcc_readl(tdma->regs, DMA_CHCONFIG);
			if (DMA_CHCONFIG_MIS(chconfig, ch_idx) != (u32)0UL) {
				ch_writel(tdmac, DMA_CHCTRL, DMA_CHCTRL_FLAG);
				tasklet_schedule(&tdmac->tasklet);
			}
		}

		ret = IRQ_HANDLED;
	}

	return ret;
}

static dma_cookie_t tcc_dma_tx_submit(struct dma_async_tx_descriptor *tx)
{
	struct tcc_dma_desc *desc = to_tcc_dma_desc(tx);
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(tx->chan);
	dma_cookie_t cookie;
	unsigned long flags;

	spin_lock_irqsave(&tdmac->lock, flags);
	cookie = dma_cookie_assign(tx);
	list_splice_tail(&desc->tx_list, &tdmac->desc_submitted);
	spin_unlock_irqrestore(&tdmac->lock, flags);

	return cookie;
}

static struct tcc_dma_desc *tcc_dma_alloc_descriptor(struct tcc_dma_chan *tdmac)
{
	struct tcc_dma_desc *desc;
	dma_addr_t phys;

	desc = dma_pool_alloc(tdmac->desc_pool, GFP_ATOMIC, &phys);
	if (desc == NULL) {
		dev_err(chan2dev(&tdmac->chan),
				"[ERROR][GDMA] failed to allocate descriptor pool\n");
	} else {
		(void)memset((void *)desc, 0, sizeof(struct tcc_dma_desc));

		INIT_LIST_HEAD(&desc->tx_list);
		INIT_LIST_HEAD(&desc->dma_node);
		dma_async_tx_descriptor_init(&desc->txd, &tdmac->chan);
		desc->txd.tx_submit = &tcc_dma_tx_submit;
		desc->txd.phys = phys;
	}

	return desc;
}

static int tcc_dma_terminate_all(struct dma_chan *chan)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
	ulong flags;
	int ret = 0;

	if (tdmac == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdmac is NULL\n", __func__);
		ret = -ENXIO;
	} else {
		/* Disable DMA channel */
		spin_lock_irqsave(&tdmac->lock, flags);
		ch_writel(tdmac, DMA_CHCTRL, 0);

		tcc_dma_desc_free_list(tdmac, &tdmac->desc_submitted);
		tcc_dma_desc_free_list(tdmac, &tdmac->desc_issued);
		tcc_dma_desc_free_list(tdmac, &tdmac->desc_completed);
		spin_unlock_irqrestore(&tdmac->lock, flags);
	}

	return ret;
}

/*
 * Allocate resources for DMA channel
 */
static int tcc_dma_alloc_chan_resources(struct dma_chan *chan)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
	int ret = 0;

	if (tdmac == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdmac is NULL\n", __func__);
		ret = -ENXIO;
	} else {
		dma_cookie_init(chan);

		tdmac->desc_pool = dma_pool_create(
				dev_name(chan2dev(chan)),
				tdmac->dev,
				sizeof(struct tcc_dma_desc),
				__alignof(struct tcc_dma_desc), 0);
		if (tdmac->desc_pool == NULL) {
			dev_err(chan2dev(chan),
					"[ERROR][GDMA] failed to create descriptor pool\n");
			ret = -ENOMEM;
		}
	}

	return ret;
}

/*
 * Free all resources of the channel
 */
static void tcc_dma_free_chan_resources(struct dma_chan *chan)
{
	const struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);

	if (tdmac == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdmac is NULL\n", __func__);
	} else {
		(void)tcc_dma_terminate_all(chan);
		dma_pool_destroy(tdmac->desc_pool);
	}
}

static bool tcc_dma_filter(struct dma_chan *chan, void *param)
{
	const struct tcc_dma_filter_data *fdata = param;
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
	int ret = 0;

	if ((chan == NULL) || (param == NULL)) {
		(void)pr_err("[ERROR][GDMA] %s: chan or param is NULL\n",
				__func__);
		ret = -ENXIO;
	}

	if ((ret == 0) && (chan->device != &fdata->tdma->dma)) {
		/*
		 * dmaengine finds tdma->dma in all channels.
		 * So, this function almost returns -ENODEV.
		 */
		ret = -ENODEV;
	}

	if ((ret == 0) && (fdata->chan_id < TCC_MAX_DMA_CHANNELS)) {
		chan->chan_id = (int)fdata->chan_id;
		tdmac->slave_id = fdata->slave_id;
		tdmac->slave_bit = fdata->slave_bit;
	}

	return (ret == 0);
}

static int tcc_dma_set_req(struct tcc_dma_filter_data *fdata)
{
	const struct tcc_dma *tdma = fdata->tdma;
	u32 slave_id = fdata->slave_id;
	u32 reg_val;
	u32 dma_sel;
	int ret = 0;

	if (tdma->req_reg == NULL) {
		dev_err(tdma->dma.dev, "[ERROR][GDMA] request register is NULL\n");
		ret = -ENOMEM;
	}

	if (slave_id >= 64U) {
		dev_err(tdma->dma.dev, "[ERROR][GDMA] slave id is invalid(%u)\n",
				slave_id);
		ret = -EINVAL;
	}

	if (ret == 0) {
		reg_val = tcc_readl(tdma->req_reg, 0x0);
		if (slave_id > 31U) {
			dma_sel = ((u32)0x1UL << (slave_id - 32U));
			reg_val |= (dma_sel);
		} else {
			dma_sel = ((u32)0x1UL << slave_id);
			reg_val &= ~(dma_sel);
		}
		tcc_writel(tdma->req_reg, 0x0, reg_val);

		fdata->slave_bit = (u64)0X1ULL << slave_id;
	}

	return ret;
}

static struct dma_chan *tcc_dma_of_xlate(struct of_phandle_args *dma_spec,
		struct of_dma *ofdma)
{
	struct tcc_dma *tdma;
	struct tcc_dma_filter_data fdata;
	struct dma_chan *pdma_chan = NULL;
	s32 count;
	dma_cap_mask_t cap;
	int ret = 0;

	if ((dma_spec == NULL) || (ofdma == NULL)) {
		(void)pr_err("[ERROR][GDMA] dma_spec or ofdma is NULL\n");
		ret = -EINVAL;
	}

	if (ret == 0) {
		tdma = (struct tcc_dma *)ofdma->of_dma_data;
		count = dma_spec->args_count;
		if (count != 2) {
			dev_err(tdma->dma.dev,
					"[ERROR][GDMA] Invalid argument counter: %d\n",
					count);
			ret = -EINVAL;
		}
	}

	if (ret == 0) {
		if (dma_spec->args[0] >= TCC_MAX_DMA_CHANNELS) {
			dev_err(tdma->dma.dev,
					"[ERROR][GDMA] Invalid gdma channel %u\n",
					dma_spec->args[0]);
			ret = -EINVAL;
		} else {
			fdata.chan_id = dma_spec->args[0];
		}

		if (dma_spec->args[1] >= TCC_MAX_DMA_SLAVES) {
			dev_err(tdma->dma.dev,
					"[ERROR][GDMA] Invalid gdma channel %u\n",
					dma_spec->args[1]);
			ret = -EINVAL;
		} else {
			fdata.slave_id = dma_spec->args[1];
		}
	}

	if (ret == 0) {
		fdata.tdma = tdma;
		ret = tcc_dma_set_req(&fdata);
	}

	if (ret == 0) {
		dma_cap_zero(cap);
		dma_cap_set(DMA_SLAVE, cap);

		pdma_chan = dma_request_channel(cap, &tcc_dma_filter,
				(void *)&fdata);
	}

	return pdma_chan;
}

/*
 * Poll for transaction completion
 */
static enum dma_status tcc_dma_tx_status(struct dma_chan *chan,
		dma_cookie_t cookie,
		struct dma_tx_state *txstate)
{
	const struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
	enum dma_status ret = DMA_ERROR;
	int done = 0;
	u32 remain_bytes;
	u32 cur_hcount;
	u32 ctrl_val;
	u32 burst;
	u32 width;

	if (tdmac == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdmac is NULL\n", __func__);
		ret = DMA_ERROR;
		done = 1;
	}

	if (done == 0) {
		ret = dma_cookie_status(chan, cookie, txstate);
		if (ret == DMA_COMPLETE) {
			dev_vdbg(chan2dev(chan),
					"[DEBUG][GDMA] DMA complete transaction. cookie=%d\n",
					cookie);
			done = 1;
		}
	}

	if ((done == 0) &&
			((tdmac->direction == DMA_MEM_TO_DEV) ||
			 (tdmac->direction == DMA_DEV_TO_MEM) ||
			 (tdmac->direction == DMA_MEM_TO_MEM))) {
		cur_hcount = (ch_readl(tdmac, DMA_HCOUNT) >> 16U) & 0xFFFFU;
		if (cur_hcount != 0U) {
			ctrl_val = ch_readl(tdmac, DMA_CHCTRL);
			burst = (ctrl_val >> 6U) & 0x3U;
			width = (ctrl_val >> 4U) & 0x3U;
			if (width >= 2U) {
				/* WSIZE = 2, 3 means 32-bit data */
				width = 2;
			}
			remain_bytes = cur_hcount << (burst + width);
		} else {
			remain_bytes = 0;
		}
		dma_set_residue(txstate, remain_bytes);
	}

	return ret;
}

static struct tcc_dma_desc *tcc_dma_mem_desc_conf(struct tcc_dma_chan *tdmac,
		u32 xfer_count, u32 src_addr, u32 dst_addr)
{
	struct tcc_dma_desc *desc;

	desc = tcc_dma_alloc_descriptor(tdmac);
	if (desc != NULL) {
		desc->src_addr = src_addr;
		desc->src_inc = 1;

		desc->dst_addr = dst_addr;
		desc->dst_inc = 1;

		desc->len = xfer_count;
		desc->width = DMA_SLAVE_BUSWIDTH_1_BYTE;
		desc->burst = 1;
		desc->slave_id = 0;
	}

	return desc;
}

/*
 * Prepare a memcpy operation
 */
static struct dma_async_tx_descriptor *tcc_dma_prep_dma_memcpy(
		struct dma_chan *chan,
		dma_addr_t dest,
		dma_addr_t src,
		size_t len,
		unsigned long flags)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
	struct tcc_dma_desc *desc;
	struct tcc_dma_desc *first = NULL;
	struct dma_async_tx_descriptor *ret_desc;
	int ret = 0;
	u64 remain_len = 0;
	u32 xfer_count;
	u32 dest32;
	u32 src32;

	if (len == 0U) {
		dev_err(chan2dev(chan), "[ERROR][GDMA] %s: len is zero\n",
				__func__);
		ret = -EINVAL;
	}

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	if ((dest > 0xFFFFFFFFU) || (src > 0xFFFFFFFFU)) {
		dev_err(chan2dev(chan),
				"[ERROR][GDMA] Not support 64-bit address.\n");
		ret = -EINVAL;
	}
#endif

	if (ret == 0) {
		dest32 = (u32)dest;
		src32 = (u32)src;

		tdmac->direction = DMA_MEM_TO_MEM;
		remain_len = len;
	}

	while ((ret == 0) && (remain_len != 0U)) {
		xfer_count = (remain_len > TCC_DMA_MAX_XFER_CNT) ?
			(u32)TCC_DMA_MAX_XFER_CNT : (u32)remain_len;
		desc = tcc_dma_mem_desc_conf(tdmac, xfer_count, src32, dest32);
		if (desc == NULL) {
			ret = -ENOMEM;
			break;
		}

		if (first == NULL) {
			/* First descriptor is added to desc->tx_list */
			first = desc;
		}
		list_add_tail(&desc->dma_node, &first->tx_list);

		if ((src32 > (0xFFFFFFFFU - xfer_count)) ||
				(dest32 > (0xFFFFFFFFU - xfer_count))) {
			dev_err(chan2dev(chan),
					"[ERROR][GDMA] address is out of range.\n");
			ret = -ENOMEM;
		} else {
			src32 += xfer_count;
			dest32 += xfer_count;
			remain_len -= xfer_count;
		}
	}

	if (ret < 0) {
		dev_err(chan2dev(chan),
				"[ERROR][GDMA] Failed to ready descriptor for mem copy\n");
		if ((ret == -ENOMEM) && (first != NULL)) {
			/* Remove all generated descriptors */
			tcc_dma_desc_free_list(tdmac, &first->tx_list);
		}
		ret_desc = NULL;
	} else {
		ret_desc = &first->txd;
	}

	return ret_desc;
}

static struct tcc_dma_desc *tcc_dma_slv_desc_conf(struct tcc_dma_chan *tdmac,
		const struct scatterlist *sg)
{
	const struct dma_slave_config *config = &tdmac->slave_config;
	struct tcc_dma_desc *ret_desc;
	struct tcc_dma_desc *desc = NULL;
	int ret = 0;

	/* Check to valid sg data */
	if (sg_dma_len(sg) > TCC_DMA_MAX_XFER_CNT) {
		dev_err(chan2dev(&tdmac->chan),
				"[ERROR][GDMA] sg dma length is too large.\n");
		ret = -EINVAL;
	}
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
	if ((ret == 0) && (sg_dma_address(sg) > 0xFFFFFFFFU)) {
		dev_err(chan2dev(&tdmac->chan),
				"[ERROR][GDMA] sg dma address is out of 32 bit address.\n");
		ret = -EINVAL;
	}
#endif

	if (ret == 0) {
		desc = tcc_dma_alloc_descriptor(tdmac);
		if (desc == NULL) {
			dev_err(chan2dev(&tdmac->chan),
					"[ERROR][GDMA] Failed to allocate descriptor for slave.\n");
			ret = -ENOMEM;
		}
	}

	if (ret == 0) {
		desc->len = sg_dma_len(sg);
		desc->slave_id = tdmac->slave_id;
		if (tdmac->direction == DMA_MEM_TO_DEV) {
			desc->src_addr = (u32)sg_dma_address(sg);
			desc->src_inc = (u32)config->src_addr_width;
			desc->dst_addr = (u32)(config->dst_addr & 0xFFFFFFFFU);
			desc->dst_inc = 0;
			desc->width = config->src_addr_width;
			desc->burst = (u32)config->src_maxburst;
		} else { /* DMA_DEV_TO_MEM */
			desc->src_addr = (u32)(config->src_addr & 0xFFFFFFFFU);
			desc->src_inc = 0;
			desc->dst_addr = (u32)sg_dma_address(sg);
			desc->dst_inc = (u32)config->dst_addr_width;
			desc->width = config->dst_addr_width;
			desc->burst = (u32)config->dst_maxburst;
		}
		if (desc->burst == 0U) {
			dev_err(chan2dev(&tdmac->chan),
					"[ERROR][GDMA] dma burst is zero\n");
			ret = -EINVAL;
		}
		if ((ret == 0) && ((desc->len % desc->burst) != 0U)) {
			dev_err(chan2dev(&tdmac->chan),
					"[ERROR][GDMA] len(%d) should be a multiple of burst(%d)\n",
					desc->len, desc->burst);
			ret = -EINVAL;
		}
	}

	if (ret < 0) {
		if (desc != NULL) {
			dma_pool_free(tdmac->desc_pool,
					(void *)desc, desc->txd.phys);
		}
		ret_desc = NULL;
	} else {
		ret_desc = desc;
	}

	return ret_desc;
}

/*
 * Prepare a slave DMA operation
 */
static struct dma_async_tx_descriptor *tcc_dma_prep_slave_sg(
		struct dma_chan *chan,
		struct scatterlist *sgl,
		unsigned int sg_len,
		enum dma_transfer_direction direction,
		unsigned long flags,
		void *context)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
	const struct dma_slave_config *config;
	struct tcc_dma_desc *desc;
	struct tcc_dma_desc *first = NULL;
	struct scatterlist *sg;
	struct dma_async_tx_descriptor *ret_desc;
	u32 sg_idx;
	int ret = 0;

	if (sg_len == 0U) {
		dev_err(chan2dev(chan), "[ERROR][GDMA] sg_len is zero\n");
		ret = -EINVAL;
	}

	if ((ret == 0) && !is_slave_direction(direction)) {
		dev_err(chan2dev(chan), "[ERROR][GDMA] Invalid slave direction\n");
		ret = -EINVAL;
	}

	if (ret == 0) {
		tdmac->direction = direction;
		config = &tdmac->slave_config;

		sg = sgl;
		for (sg_idx = 0U; sg_idx < sg_len; sg_idx++) {
			desc = tcc_dma_slv_desc_conf(tdmac, sg);
			if (desc == NULL) {
				ret = -ENOMEM;
				break;
			}

			if (first == NULL) {
				/* First descriptor is added to desc->tx_list */
				first = desc;
			}
			list_add_tail(&desc->dma_node, &first->tx_list);

			sg = sg_next(sg);
		}
	}

	if (ret < 0) {
		dev_err(chan2dev(chan), "[ERROR][GDMA] Failed to ready dma descriptor for slave\n");
		if ((ret == -ENOMEM) && (first != NULL)) {
			/* Remove all generated descriptors */
			tcc_dma_desc_free_list(tdmac, &first->tx_list);
		}
		ret_desc = NULL;
	} else {
		ret_desc = &first->txd;
	}

	return ret_desc;
}

static int tcc_dma_slave_config(struct dma_chan *chan,
		struct dma_slave_config *cfg)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
	int ret = 0;

	if (tdmac == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdmac is NULL\n", __func__);
		ret = -ENXIO;
	}

	if (ret == 0) {
		dev_vdbg(chan2dev(chan), "[DEBUG][GDMA] src_addr=0x%x, dst_addr=0x%x, dir=%d\n",
				(uint)cfg->src_addr, (uint)cfg->dst_addr,
				cfg->direction);

		(void)memcpy(&tdmac->slave_config, cfg,
				sizeof(struct dma_slave_config));
	}

	return ret;
}

/*
 * Push pending transactions to hardware
 */
static void tcc_dma_issue_pending(struct dma_chan *chan)
{
	struct tcc_dma_chan *tdmac = to_tcc_dma_chan(chan);
	const struct tcc_dma_desc *desc;

	list_splice_init(&tdmac->desc_submitted, &tdmac->desc_issued);

	desc = tcc_dma_get_desc(&tdmac->desc_issued);
	if (desc == NULL) {
		dev_err(chan2dev(chan),
				"[ERROR][GDMA] There is no descriptor\n");
	} else {
		tcc_dma_do_single_block(tdmac, desc);
	}
}

static void tcc_dma_set_access_control(const struct tcc_dma *tdma)
{
	if (tdma->ac_reg != NULL) {
		tcc_writel(tdma->ac_reg, DMA_AC0_START, tdma->ac_val[0][0]);
		tcc_writel(tdma->ac_reg, DMA_AC0_LIMIT, tdma->ac_val[0][1]);
		tcc_writel(tdma->ac_reg, DMA_AC1_START, tdma->ac_val[1][0]);
		tcc_writel(tdma->ac_reg, DMA_AC1_LIMIT, tdma->ac_val[1][1]);
		tcc_writel(tdma->ac_reg, DMA_AC2_START, tdma->ac_val[2][0]);
		tcc_writel(tdma->ac_reg, DMA_AC2_LIMIT, tdma->ac_val[2][1]);
		tcc_writel(tdma->ac_reg, DMA_AC3_START, tdma->ac_val[3][0]);
		tcc_writel(tdma->ac_reg, DMA_AC3_LIMIT, tdma->ac_val[3][1]);
	}
}

static void tcc_dma_parse_access_control(struct tcc_dma *tdma)
{
	struct device_node *ac_np;
	const char ac_name[4][16] = {
		"access-control0", "access-control1",
		"access-control2", "access-control3" };
	u32 ac_val[2] = { 0 };
	int ac_idx;

	ac_np = of_parse_phandle(tdma->dev->of_node, "access-control", 0);
	if (ac_np != NULL) {
		tdma->ac_reg = of_iomap(ac_np, 0);
		if (IS_ERR(tdma->ac_reg)) {
			dev_err(tdma->dev, "[ERROR][GDMA] failed ac_reg ioremap, err: %ld\n",
					PTR_ERR(tdma->ac_reg));
			tdma->ac_reg = NULL;
		}
	} else {
		tdma->ac_reg = NULL;
	}

	if (tdma->ac_reg != NULL) {
		for (ac_idx = 0; ac_idx < 4; ac_idx++) {
			if (of_property_read_u32_array(
						ac_np, ac_name[ac_idx],
						ac_val, 2) == 0) {
				dev_vdbg(tdma->dev, "[DEBUG][GDMA] access-control%d start:0x%08x limit:0x%08x\n",
						ac_idx, ac_val[0], ac_val[1]);
				tdma->ac_val[ac_idx][0] = ac_val[0];
				tdma->ac_val[ac_idx][1] = ac_val[1];
			}
		}
	}
}

#ifdef CONFIG_OF
static int tcc_dma_parse_dt(struct tcc_dma *tdma)
{
	const struct device_node *np = tdma->dev->of_node;
	int ret;

	ret = of_property_read_u32(np, "dma-channels", &tdma->dma_channels);
	if ((ret < 0) || (tdma->dma_channels > TCC_MAX_DMA_CHANNELS)) {
		dev_err(tdma->dev, "[ERROR][GDMA] dma-channels(%u) is wrong.\n",
				tdma->dma_channels);
		tdma->dma_channels = 0;
	}

	/* Get GDMA Access Control base address and set access control */
	tcc_dma_parse_access_control(tdma);
	tcc_dma_set_access_control(tdma);

	return ret;
}
#else
static int tcc_dma_parse_dt(struct tcc_dma *tdma)
{
	return -EINVAL;
}
#endif

static int tcc_dma_set_reg(struct platform_device *pdev, struct tcc_dma *tdma)
{
	const struct resource *res;
	int ret = 0;

	/* base address register */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	tdma->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(tdma->regs)) {
		dev_err(&pdev->dev,
				"[ERROR][GDMA] Failed to get base address, err: %ld\n",
				PTR_ERR(tdma->regs));
		ret = (int)PTR_ERR(tdma->regs);
	}

	/* request register */
	if (ret == 0) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (IS_ERR_OR_NULL(res)) {
			/* some dma controller doesn't have request register */
			dev_info(&pdev->dev, "[INFO][GDMA] No request register\n");
		} else {
			tdma->req_reg = devm_ioremap_resource(&pdev->dev, res);
			if (IS_ERR(tdma->req_reg)) {
				dev_err(&pdev->dev,
						"[ERROR][GDMA] Failed to get request register, err: %ld\n",
						PTR_ERR(tdma->req_reg));
				ret = (int)PTR_ERR(tdma->req_reg);
			}
		}
	}

	return ret;
}

static int tcc_dma_set_irq(struct platform_device *pdev, struct tcc_dma *tdma)
{
	int ret = 0;

	tdma->irq = platform_get_irq(pdev, 0);
	if (tdma->irq < 0) {
		dev_err(&pdev->dev, "[ERROR][GDMA] Failed to get irq number\n");
		ret = tdma->irq;
	} else {
		ret = devm_request_irq(&(pdev->dev),
				(uint)tdma->irq, &tcc_dma_interrupt,
				IRQF_SHARED, "tcc_dma", (void *)tdma);
		if (ret != 0) {
			dev_err(&pdev->dev,
					"[ERROR][GDMA] Failed to set %d irq handler (%d)\n",
					tdma->irq, ret);
		}
	}

	return ret;
}

static int tcc_dma_set_clk(struct tcc_dma *tdma)
{
	int ret = 0;

	tdma->hclk = devm_clk_get(tdma->dev, NULL);
	if (IS_ERR(tdma->hclk)) {
		dev_err(tdma->dev,
				"[ERROR][GDMA] Failed to get hclk info, err: %ld\n",
				PTR_ERR(tdma->hclk));
		ret = (int)PTR_ERR(tdma->hclk);
	}

	if (ret >= 0) {
		ret = clk_prepare_enable(tdma->hclk);
		if (ret != 0) {
			dev_err(tdma->dev,
					"[ERROR][GDMA] Failed to enable hclk\n");
		}
	}

	return ret;
}

static int tcc_dma_set_ctrl(struct platform_device *pdev, struct tcc_dma *tdma)
{
	int ret;

	ret = dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (ret != 0) {
		dev_err(&pdev->dev,
				"[ERROR][GDMA] Unable to set DMA mask, ret: %d\n",
				ret);
	}

	if (ret == 0) {
		/* Set register address for gdma */
		ret = tcc_dma_set_reg(pdev, tdma);
	}

	if (ret == 0) {
		/* Set the interrupt */
		ret = tcc_dma_set_irq(pdev, tdma);
	}

	if (ret == 0) {
		/* Configure gdma clock */
		ret = tcc_dma_set_clk(tdma);
	}

	if (ret >= 0) {
		/* Parse dtb */
		ret = tcc_dma_parse_dt(tdma);
	}

	tdma->soc = of_device_get_match_data(tdma->dev);
	if (tdma->soc == NULL) {
		dev_err(&pdev->dev, "[ERROR][GDMA] No soc data\n");
		ret = -ENODEV;
	}

	return ret;
}

static int tcc_dma_set_chan(struct tcc_dma *tdma)
{
	int ret = 0;
	u32 ch_idx;

	tdma->chan = devm_kcalloc(tdma->dev, (size_t)tdma->dma_channels,
			sizeof(struct tcc_dma_chan), GFP_KERNEL);
	if (tdma->chan == NULL) {
		/* Please check memory size */
		ret = -ENOMEM;
	}

	if (ret >= 0) {
		INIT_LIST_HEAD(&tdma->dma.channels);
		for (ch_idx = 0U; ch_idx < tdma->dma_channels; ch_idx++) {
			struct tcc_dma_chan *tdmac = &tdma->chan[ch_idx];

			tasklet_init(&tdmac->tasklet, &tcc_dma_tasklet,
					(ulong)tdmac);

			tdmac->chan.device = &tdma->dma;
			tdmac->dev = tdma->dev;
			tdmac->direction = DMA_TRANS_NONE;
			tdmac->ch_base = tdma->regs + (ch_idx * DMA_CH_REG_OFF);
			tdmac->soc = tdma->soc;
			dma_cookie_init(&tdmac->chan);

			INIT_LIST_HEAD(&tdmac->desc_submitted);
			INIT_LIST_HEAD(&tdmac->desc_issued);
			INIT_LIST_HEAD(&tdmac->desc_completed);

			spin_lock_init(&tdmac->lock);

			list_add_tail(&tdmac->chan.device_node,
					&tdma->dma.channels);
		}
	}

	return ret;
}

static int tcc_dma_alloc(struct tcc_dma *tdma)
{
	int ret;

	dma_cap_zero(tdma->dma.cap_mask);
	dma_cap_set(DMA_MEMCPY, tdma->dma.cap_mask);
	dma_cap_set(DMA_SLAVE, tdma->dma.cap_mask);

	tdma->dma.dev = tdma->dev;

	tdma->dma.device_alloc_chan_resources = &tcc_dma_alloc_chan_resources;
	tdma->dma.device_free_chan_resources = &tcc_dma_free_chan_resources;
	tdma->dma.device_tx_status = &tcc_dma_tx_status;
	tdma->dma.device_prep_dma_memcpy = &tcc_dma_prep_dma_memcpy;
	tdma->dma.device_prep_slave_sg = &tcc_dma_prep_slave_sg;
	tdma->dma.device_issue_pending = &tcc_dma_issue_pending;
	tdma->dma.device_config = &tcc_dma_slave_config;
	tdma->dma.device_terminate_all = &tcc_dma_terminate_all;

	ret = dma_async_device_register(&tdma->dma);
	if (ret != 0) {
		dev_err(tdma->dev, "[ERROR][GDMA] failed to register DMA device. ret: %d\n",
				ret);
	}

	if (ret == 0) {
		ret = of_dma_controller_register(tdma->dev->of_node,
				&tcc_dma_of_xlate, (void *)tdma);
		if (ret != 0) {
			dev_err(tdma->dev, "[ERROR][GDMA] failed to register of_dma_controller\n");
			dma_async_device_unregister(&tdma->dma);
		} else {
			dev_info(tdma->dev, "[INFO][GDMA] Telechips DMA controller initialized (irq=%d)\n",
					tdma->irq);
		}
	}

	return ret;
}

static int tcc_dma_probe(struct platform_device *pdev)
{
	struct tcc_dma *tdma;
	int ret = 0;

	/* configure dma controller */
	tdma = devm_kzalloc(&pdev->dev, sizeof(struct tcc_dma), GFP_KERNEL);
	if (tdma == NULL) {
		ret = -ENOMEM;
	} else {
		tdma->dev = &pdev->dev;
		platform_set_drvdata(pdev, (void *)tdma);
	}

	if (ret == 0) {
		/* Configure gdma controller */
		ret = tcc_dma_set_ctrl(pdev, tdma);
	}

	if (ret >= 0) {
		/* Configure dma channel */
		ret = tcc_dma_set_chan(tdma);
	}

	if (ret >= 0) {
		/* dma allocation */
		ret = tcc_dma_alloc(tdma);
	}

	return ret;
}

static int tcc_dma_remove(struct platform_device *pdev)
{
	struct tcc_dma *tdma = (struct tcc_dma *)platform_get_drvdata(pdev);
	int ret = 0;
	u32 ch_idx;

	if (tdma != NULL) {
		of_dma_controller_free(pdev->dev.of_node);
		dma_async_device_unregister(&tdma->dma);
		if (tdma->irq > 0) {
			devm_free_irq(&pdev->dev,
					(uint)tdma->irq, (void *)tdma);
		}

		for (ch_idx = 0U; ch_idx < tdma->dma_channels; ch_idx++) {
			struct tcc_dma_chan *tdmac = &tdma->chan[ch_idx];

			(void)tcc_dma_terminate_all(&tdmac->chan);
			tasklet_kill(&tdmac->tasklet);
		}

		clk_disable_unprepare(tdma->hclk);
	} else {
		ret = -ENXIO;
	}

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int tcc_dma_suspend(struct device *dev)
{
	const struct tcc_dma *tdma = (struct tcc_dma *)dev_get_drvdata(dev);

	clk_disable_unprepare(tdma->hclk);

	return 0;
}

static int tcc_dma_resume(struct device *dev)
{
	const struct tcc_dma *tdma = (struct tcc_dma *)dev_get_drvdata(dev);
	int ret = 0;

	if (tdma == NULL) {
		(void)pr_err("[ERROR][GDMA] %s: tdma is NULL\n", __func__);
		ret = -ENXIO;
	}

	if (ret >= 0) {
		ret = clk_prepare_enable(tdma->hclk);
		if (ret != 0) {
			/* Please check clk driver in resume process */
			dev_err(dev, "[ERROR][DMA] Failed to enable dma hclk\n");
		}
	}

	if (ret >= 0) {
		/* Re-configure access control */
		tcc_dma_set_access_control(tdma);
	}

	return ret;
}

static const struct dev_pm_ops tcc_dma_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tcc_dma_suspend, tcc_dma_resume)
};
#endif

static const struct tcc_dma_soc_data soc_data_tcc897x = {
	.sync = 0x6C000F006C000F00,
};

static const struct tcc_dma_soc_data soc_data_tcc = {
	.sync = 0x0,
};

#ifdef CONFIG_OF
static const struct of_device_id tcc_dma_of_id_table[5] = {
	{.compatible = "telechips,tcc897x-dma", .data = &soc_data_tcc897x},
	{.compatible = "telechips,tcc803x-dma", .data = &soc_data_tcc},
	{.compatible = "telechips,tcc805x-dma", .data = &soc_data_tcc},
	{.compatible = "telechips,tcc807x-dma", .data = &soc_data_tcc},
	{}
};

MODULE_DEVICE_TABLE(of, tcc_dma_of_id_table);
#endif

static struct platform_driver tcc_dma_driver = {
	.driver = {
		   .name = "tcc-dma",
		   .owner = THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
		   .pm = &tcc_dma_pm_ops,
#endif
		   .of_match_table = of_match_ptr(tcc_dma_of_id_table),
		   },
	.probe = tcc_dma_probe,
	.remove = tcc_dma_remove,
};

static int tcc_dma_init(void)
{
	return platform_driver_register(&tcc_dma_driver);
}

subsys_initcall(tcc_dma_init);

static void __exit tcc_dma_exit(void)
{
	platform_driver_unregister(&tcc_dma_driver);
}

module_exit(tcc_dma_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips DMA driver");
MODULE_LICENSE("GPL");
