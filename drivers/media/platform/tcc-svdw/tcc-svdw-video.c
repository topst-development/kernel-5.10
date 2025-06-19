/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <linux/compiler_attributes.h>
#include <linux/err.h>
#include <linux/limits.h>
#include <linux/list.h>
#include <linux/videodev2.h>
#include <media/videobuf2-core.h>
#include <linux/atomic.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/interrupt.h>
#include <linux/of_graph.h>
#include <linux/of_irq.h>
#include <linux/version.h>
#include <linux/videodev2.h>
#include <media/v4l2-common.h>
#include <media/videobuf2-dma-contig.h>
#include <video/tcc-svdw.h>

#include "tcc-svdw.h"

#ifdef CONFIG_ARCH_TCC807X
#include "807x/tcc-svdw-stitch.h"
#endif
#ifdef CONFIG_ARCH_TCC750X
//#include "750x/tcc-svdw-stitch.h"
#endif

#if defined(CONFIG_ARCH_TCC807X) || defined(CONFIG_ARCH_TCC750X)
extern void tcc_svdw_parse_cfg_and_set_regs(struct tcc_svdw_device *p_svdw,
					    const struct firmware *fw);
extern int tcc_svdw_do_ioremap(struct tcc_svdw_device *p_svdw, struct device_node *node);
#else
static inline void tcc_svdw_parse_cfg_and_set_regs(struct tcc_svdw_device *p_svdw,
						   const struct firmware *fw)
{
}
static inline int tcc_svdw_do_ioremap(struct tcc_svdw_device *p_svdw, struct device_node *node)
{
	return 0;
}
#endif

LIST_HEAD(tcc_svdw_dewarp_list);
EXPORT_SYMBOL(tcc_svdw_dewarp_list);

__maybe_unused atomic_t tcc_svdw_dewarp_cnt = ATOMIC_INIT(TCC_SVDW_DEWARP_MAX);
EXPORT_SYMBOL(tcc_svdw_dewarp_cnt);

struct tcc_svdw_mbus_format {
	u32 mbus_fmt;
	u32 data_format;
	u32 data_order;
};

struct tcc_svdw_format {
	u32 pixelformat;
	u32 guid;
};

#define IS_SET(value, mask) (((u32)(value) & (u32)(mask)) != 0U)

static struct tcc_svdw_mbus_format tcc_svdw_mbus_format_list[] = {
	{ .mbus_fmt = MEDIA_BUS_FMT_UYVY8_2X8,
	  .data_format = FMT_YUV422_LSB_16BIT,
	  .data_order = ORDER_RGB },
	{ .mbus_fmt = MEDIA_BUS_FMT_UYVY8_1X16,
	  .data_format = FMT_YUV422_LSB_16BIT,
	  .data_order = ORDER_RGB },
	{ .mbus_fmt = MEDIA_BUS_FMT_Y8_1X8,
	  .data_format = FMT_YUV422_LSB_16BIT,
	  .data_order = ORDER_RBG },
	{ .mbus_fmt = MEDIA_BUS_FMT_Y10_1X10,
	  .data_format = FMT_YUV422_LSB_16BIT,
	  .data_order = ORDER_RBG },
	{ .mbus_fmt = MEDIA_BUS_FMT_Y12_1X12,
	  .data_format = FMT_YUV422_LSB_16BIT,
	  .data_order = ORDER_RBG },
	{ .mbus_fmt = MEDIA_BUS_FMT_RGB888_1X24, .data_format = FMT_RGB, .data_order = ORDER_RBG },
	{ .mbus_fmt = MEDIA_BUS_FMT_YUV8_1X24, .data_format = FMT_YUV444, .data_order = ORDER_RBG },
};

/* supported color formats */
static struct tcc_svdw_format tcc_svdw_format_list[] = {
	{
		.pixelformat = V4L2_PIX_FMT_RGB32,
		.guid = SVDW_COLOR_FMT_RGB,
	},
	/* sequential (YUV packed) */
	{
		.pixelformat = V4L2_PIX_FMT_UYVY,
		.guid = SVDW_COLOR_FMT_UYVY,
	},
	{
		.pixelformat = V4L2_PIX_FMT_VYUY,
		.guid = SVDW_COLOR_FMT_VYUY,
	},
	{
		.pixelformat = V4L2_PIX_FMT_YUYV,
		.guid = SVDW_COLOR_FMT_YUYV,
	},
	{
		.pixelformat = V4L2_PIX_FMT_YVYU,
		.guid = SVDW_COLOR_FMT_YVYU,
	},

	/* two non contiguous planes - one Y, one Cr + Cb interleaved  */
	{
		.pixelformat = V4L2_PIX_FMT_NV12M,
		.guid = SVDW_COLOR_FMT_YUV420IL_ODD,
	},
	{
		.pixelformat = V4L2_PIX_FMT_NV21M,
		.guid = SVDW_COLOR_FMT_YVU420IL_ODD,
	},
	{
		.pixelformat = V4L2_PIX_FMT_NV16M,
		.guid = SVDW_COLOR_FMT_YUV422IL,
	},
	{
		.pixelformat = V4L2_PIX_FMT_NV61M,
		.guid = SVDW_COLOR_FMT_YVU422IL,
	},
};

static void __maybe_unused tcc_svdw_get_vs_info_format_by_mbus_format(
	struct tcc_svdw_device *p_svdw, u32 mbus_pixelcode, struct tcc_svdw_vs_info *vs_fmt)
{
	const struct device *p_dev = NULL;
	const struct tcc_svdw_mbus_format *format = NULL;
	u32 idxList = 0;
	u32 nList = 0;

	p_dev = svdw_to_dev(p_svdw);

	/* default format */
	vs_fmt->data_format = FMT_YUV422_LSB_16BIT;
	vs_fmt->data_order = ORDER_RGB;

	nList = ARRAY_SIZE(tcc_svdw_mbus_format_list);
	for (idxList = 0; idxList < nList; idxList++) {
		format = &tcc_svdw_mbus_format_list[idxList];
		if (mbus_pixelcode == format->mbus_fmt) {
			vs_fmt->data_format = format->data_format;
			vs_fmt->data_order = format->data_order;
		}
	}
}

static const struct tcc_svdw_format *tcc_svdw_get_fmt_by_pixfmt(u32 pixelformat)
{
	u32 idxList = 0;
	u32 nList = 0;
	const struct tcc_svdw_format *tformat = NULL;

	nList = ARRAY_SIZE(tcc_svdw_format_list);
	for (idxList = 0; idxList < nList; idxList++) {
		tformat = &tcc_svdw_format_list[idxList];
		if (pixelformat == tformat->pixelformat) {
			/* matched */
			break;
		}
	}

	if (idxList == nList) {
		/* When it failed to find a corresponding value from a list of
		supported format, return NULL  */
		tformat = NULL;
	}

	return tformat;
}

static int tcc_svdw_get_clock(struct tcc_svdw_device *p_svdw, struct device_node *dev_node)
{
	const struct device *p_dev = NULL;
	int ret = 0;

	p_dev = svdw_to_dev(p_svdw);

	/* get the axi clock */
	p_svdw->svdw_wrap.cam_clk_axi = of_clk_get(dev_node, 0);
	if (p_svdw->svdw_wrap.cam_clk_axi == NULL) {
		loge(p_dev, "of_clk_get_axi\n");
		ret = -ENODEV;
	}

	/* get the pix clock */
	if (ret == 0) {
		p_svdw->svdw_wrap.cam_clk_pix = of_clk_get(dev_node, 1);
		if (p_svdw->svdw_wrap.cam_clk_pix == NULL) {
			loge(p_dev, "of_clk_get_pix\n");
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		/* get the apb clock */
		p_svdw->svdw_wrap.cam_clk_apb = of_clk_get(dev_node, 2);
		if (p_svdw->svdw_wrap.cam_clk_apb == NULL) {
			loge(p_dev, "of_clk_get_apb\n");
			ret = -ENODEV;
		}
	}

	return ret;
}

static void tcc_svdw_put_clock(struct tcc_svdw_device *p_svdw)
{
	clk_put(p_svdw->svdw_wrap.cam_clk_axi);
	clk_put(p_svdw->svdw_wrap.cam_clk_pix);
	clk_put(p_svdw->svdw_wrap.cam_clk_apb);
}

static int tcc_svdw_enable_clock(struct tcc_svdw_device *p_svdw)
{
	const struct device *p_dev = NULL;
	int ret = 0;

	p_dev = svdw_to_dev(p_svdw);

	ret = clk_prepare_enable(p_svdw->svdw_wrap.cam_clk_axi);
	if (ret != 0) {
		/* failure of clk_prepare_enable */
		loge(p_dev, "clk_prepare_enable axi, ret: %d\n", ret);
	}

	if (ret == 0) {
		ret = clk_prepare_enable(p_svdw->svdw_wrap.cam_clk_pix);
		if (ret != 0) {
			/* failure of clk_prepare_enable */
			loge(p_dev, "clk_prepare_enable pix, ret: %d\n", ret);
		}
	}

	if (ret == 0) {
		ret = clk_prepare_enable(p_svdw->svdw_wrap.cam_clk_apb);
		if (ret != 0) {
			/* failure of clk_prepare_enable */
			loge(p_dev, "clk_prepare_enable apb, ret: %d\n", ret);
		}
	}

	return ret;
}

static void tcc_svdw_disable_clock(struct tcc_svdw_device *p_svdw)
{
	clk_disable_unprepare(p_svdw->svdw_wrap.cam_clk_axi);
	clk_disable_unprepare(p_svdw->svdw_wrap.cam_clk_pix);
	clk_disable_unprepare(p_svdw->svdw_wrap.cam_clk_apb);
}

/**
 * tcc_svdw_parse_props() - parse svdw's device tree
 * @p_svdw:	a pointer of svdw instance
 *
 * 0:		Success
 * -ENODEV:	a certain device node is not found.
 */
static int tcc_svdw_parse_props(struct tcc_svdw_device *p_svdw, const struct device_node *dev_node)
{
	const struct device *p_dev = NULL;
	struct device_node *p_dtnode = NULL;
	struct tcc_svdw_wrap *svdw_wrap = NULL;
	int ret = 0;
	u32 idx = 0;

	const char *const intr_list[] = {
		[SVDW_INTR_IDX_SVM_0_EOF] = SVDW_INTR_STR_SVM_0_EOF,
		[SVDW_INTR_IDX_SVM_1_EOF] = SVDW_INTR_STR_SVM_1_EOF,
		[SVDW_INTR_IDX_SVM_2_EOF] = SVDW_INTR_STR_SVM_2_EOF,
		[SVDW_INTR_IDX_SVM_3_EOF] = SVDW_INTR_STR_SVM_3_EOF,
		[SVDW_INTR_IDX_SVM_RDONE] = SVDW_INTR_STR_SVM_RDONE,
	};

	p_dev = svdw_to_dev(p_svdw);
	p_dtnode = of_get_parent(dev_node);
	svdw_wrap = &p_svdw->svdw_wrap;

	for (idx = 0U; idx < SVDW_INTR_IDX_MAX; idx++) {
		ret = platform_get_irq_byname(p_svdw->pdev, intr_list[idx]);
		if (ret <= 0) {
			loge(p_dev, "Failed to parse %s: Invalid IRQ(%d)\n", intr_list[idx], ret);
			goto fin;
		} else {
			svdw_wrap->intr[idx].num = clamp_t(s32, ret, 0, INT_MAX);
			logd(p_dev, "%s : %u\n", intr_list[idx], svdw_wrap->intr[idx].num);
		}
	}
	/* Make sure that the return value is zeroized if parsing succeeds. */
	ret = 0;
fin:
	return ret;
}

static int __maybe_unused tcc_svdw_load_fw_cfg(struct tcc_svdw_device *p_svdw)
{
	int ret = 0;
	const struct firmware *fw = NULL;
	struct device *p_dev = NULL;
	const char *fname = "svdw_cfg";

	p_dev = svdw_to_dev(p_svdw);

	ret = request_firmware(&fw, fname, p_dev);
	if (ret < 0) {
		dev_err(p_dev, "request_firmware(%s) returned an error: %d. Skip to load a fw\n",
			fname, ret);
	} else {
		dev_info(p_dev, "load %s(size: %ld Byte)\n", fname, fw->size);
		tcc_svdw_parse_cfg_and_set_regs(p_svdw, fw);
		release_firmware(fw);
	}

	return ret;
}

static int tcc_svdw_parse_reserved_memory(struct tcc_svdw_device *p_svdw,
					  const struct device_node *dev_node)
{
	const struct device *p_dev = NULL;
	struct device_node *mmblk_node = NULL;
	const char *const name_list[SVDW_MEMBLOCK_MAX] = {
		[SVDW_MEMBLOCK_PREV] = "pmap_svdw",
		[SVDW_MEMBLOCK_OVERLAY] = "pmap_svdw_overlay",
	};
	const char *name = NULL;
	u32 mmblk_idx = 0;
	int ret = 0;

	p_dev = svdw_to_dev(p_svdw);

	for (mmblk_idx = 0; mmblk_idx < (u32)SVDW_MEMBLOCK_MAX; mmblk_idx++) {
		mmblk_node = of_parse_phandle(dev_node, "memory-region",
					      clamp_t(s32, mmblk_idx, 0, INT_MAX));
		name = name_list[mmblk_idx];
		if (!IS_ERR_OR_NULL(mmblk_node)) {
			p_svdw->svdw_wrap.rsvd_mem[mmblk_idx] = of_reserved_mem_lookup(mmblk_node);
			if (!IS_ERR_OR_NULL(p_svdw->svdw_wrap.rsvd_mem[mmblk_idx])) {
				logd(p_dev, "%20s: 0x%llx ~ 0x%llx (0x%llx)\n", name,
				     p_svdw->svdw_wrap.rsvd_mem[mmblk_idx]->base,
				     p_svdw->svdw_wrap.rsvd_mem[mmblk_idx]->base +
					     p_svdw->svdw_wrap.rsvd_mem[mmblk_idx]->size,
				     p_svdw->svdw_wrap.rsvd_mem[mmblk_idx]->size);
			}
		} else {
			loge(p_dev, "\"%s\" node is not found.\n", name);
			ret = -1;
		}
		of_node_put(mmblk_node);
	}

	return ret;
}

static int tcc_svdw_parse_fwnode(struct tcc_svdw_device *p_svdw, const struct device_node *dev_node)
{
	const struct device *p_dev = NULL;
	struct device_node *loc_ep = NULL;
	struct fwnode_handle *fwnode = NULL;
	struct v4l2_fwnode_endpoint fw_ep = {
		0,
	};
	int ret_call = 0;
	int ret = 0;

	p_dev = svdw_to_dev(p_svdw);

	/* get phandle of subdev */
	loc_ep = of_graph_get_endpoint_by_regs(dev_node, -1, -1);
	if (loc_ep == NULL) {
		loge(p_dev, "No subdev is found\n");
		ret = -EINVAL;
	} else {
		fwnode = of_fwnode_handle(loc_ep);

		fw_ep.bus_type = V4L2_MBUS_UNKNOWN;
		ret_call = v4l2_fwnode_endpoint_parse(fwnode, &fw_ep);
		if (ret_call != 0) {
			loge(p_dev, "v4l2_fwnode_endpoint_parse, ret: %d\n", ret_call);
		}
	}

	return ret;
}

static bool tcc_svdw_video_is_framesize_supported(struct tcc_svdw_device *p_svdw, u32 width,
						  u32 height)
{
	const struct device *p_dev = NULL;
	bool ret = true;

	p_dev = svdw_to_dev(p_svdw);

	logd(p_dev, "frmaesize(%u * %u)\n", width, height);

	if ((width == 0U) || (height == 0U)) {
		loge(p_dev, "width or height is 0\n");
		ret = false;
	}

	if ((width * height) >= (MAX_FRAMEWIDTH * MAX_FRAMEHEIGHT)) {
		loge(p_dev, "frmaesize(%u * %u) exceeds the maximum size(%u * %u)\n", width, height,
		     MAX_FRAMEWIDTH, MAX_FRAMEHEIGHT);
		ret = false;
	}

	return ret;
}

static int __maybe_unused tcc_svdw_draw_overlay_img(struct tcc_svdw_device *p_svdw)
{
	int ret = 0;
	struct device *p_dev = NULL;
	const struct firmware *fw = NULL;
	const char *fname = "svdw_overlay";
	void *iomem;

	svdw_set_overlay_stride(0x0898, 0x0);
	svdw_set_overlay_size(0x226, 0x0dc);
	svdw_set_overlay_offset(0x16c, 0x0f9);

	p_dev = svdw_to_dev(p_svdw);
	ret = request_firmware(&fw, fname, p_dev);
	if (ret < 0) {
		dev_err(p_dev, "request_firmware(%s) returned an error: %d. Skip to load a fw\n",
			fname, ret);
	} else {
		dev_info(p_dev, "load %s(size: %ld Byte)\n", fname, fw->size);
		iomem = ioremap_cache(p_svdw->svdw_wrap.rsvd_mem[SVDW_MEMBLOCK_OVERLAY]->base,
				      p_svdw->svdw_wrap.rsvd_mem[SVDW_MEMBLOCK_OVERLAY]->size);
		memcpy(iomem + 2, fw->data, fw->size);
		release_firmware(fw);

		svdw_set_overlay_address(p_svdw->svdw_wrap.rsvd_mem[SVDW_MEMBLOCK_OVERLAY]->base);
		svdw_enable_overlay(true);
	}
	return ret;
}

static void tcc_svdw_get_and_update_time(struct tcc_svdw_device *p_svdw, struct timespec64 *ts_prev,
					 struct timespec64 *ts_next, u32 debug)
{
	const struct device *p_dev = NULL;
	struct timespec64 ts_diff = {
		0,
	};

	p_dev = svdw_to_dev(p_svdw);

	ktime_get_raw_ts64(ts_next);

	if (debug == 1U) {
		ts_diff.tv_sec = ts_next->tv_sec - ts_prev->tv_sec;
		ts_diff.tv_nsec = ts_next->tv_nsec - ts_prev->tv_nsec;
		if (ts_diff.tv_nsec < 0) {
			ts_diff.tv_sec -= 1;
			ts_diff.tv_nsec += 1000000000;
		}

		trace_printk("timestamp curr: %9lld.%09ld, diff: %9lld.%09ld\n", ts_next->tv_sec,
			     ts_next->tv_nsec, ts_diff.tv_sec, ts_diff.tv_nsec);
	}

	*ts_prev = *ts_next;
}

static u32 __maybe_unused tcc_svdw_list_get_entry_count(struct tcc_svdw_device *p_svdw,
							struct list_head *head)
{
	struct list_head *list = NULL;
	struct tcc_svdw_buffer *p_buf = NULL;
	const struct device *p_dev = NULL;
	u32 count = 0;

	p_dev = svdw_to_dev(p_svdw);
	list_for_each (list, head) {
		p_buf = list_entry(list, struct tcc_svdw_buffer, entry);
		count++;
	}

	logd(p_dev, "tcc_svdw_list_get_entry_count: %d\n", count);

	return count;
}

static __maybe_unused struct vb2_buffer *tcc_svdw_find_next_buffer(struct vb2_queue *q)
{
	struct vb2_buffer *buf;
	list_for_each_entry (buf, &q->queued_list, queued_entry) {
		if (buf->state == VB2_BUF_STATE_ACTIVE) {
			return buf;
		}
	}

	return NULL;
}

static irqreturn_t tcc_svdw_isr_rdone(int irq, void *data)
{
	const struct device *p_dev = NULL;
	struct svdw_intr *intr = NULL;
	struct svdw_stitch_input_params *in = NULL;
	struct svdw_stitch_output_params *out = NULL;
	struct svdw_stitch_params *stitch_params = NULL;
	struct tcc_svdw_device *p_svdw = NULL;
	struct tcc_svdw_queue *p_queue = NULL;
	struct tcc_svdw_wrap *svdw_wrap = NULL;
	struct vb2_buffer *vb = NULL;
	u32 dma_addrs[MAX_PLANES];
	u32 idxpln = 0;
	unsigned long flags = 0;

	p_svdw = (struct tcc_svdw_device *)data;
	p_dev = svdw_to_dev(p_svdw);
	p_queue = &p_svdw->queue;

	svdw_wrap = &p_svdw->svdw_wrap;
	intr = &svdw_wrap->intr[SVDW_INTR_IDX_SVM_RDONE];

	stitch_params = &svdw_wrap->stitch_params;
	in = &stitch_params->in;
	out = &stitch_params->out;

	svdw_clear_rdone_interrupt();
	svdw_set_view_address_l(0x0);

	spin_lock_irqsave(&p_queue->slock, flags);
	if (list_empty(&p_svdw->queue.buf_list)) {
		goto odw_update;
	}

	p_svdw->prev_buf = list_first_entry(&p_queue->buf_list, struct tcc_svdw_buffer, entry);
	if (list_is_last(&p_svdw->prev_buf->entry, &p_queue->buf_list)) {
		trace_printk("driver has only one buffer\n");

		vb = &p_svdw->prev_buf->buf.vb2_buf;
		(void)memset(dma_addrs, 0, sizeof(dma_addrs));
		tcc_svdw_get_dma_addrs(p_svdw, vb, dma_addrs);
		tcc_svdw_print_dma_addrs(p_svdw, vb, dma_addrs);
		for (idxpln = 0; idxpln < vb->num_planes; idxpln++) {
			out->address[idxpln] = dma_addrs[idxpln];
		}

		svdw_set_view_address_l(out->address[0]);
		p_svdw->prev_buf->buf.sequence = p_svdw->sequence;
		goto odw_update;
	}

	/* The incoming buffer list has two or more entries. */
	p_svdw->curr_buf = list_next_entry(p_svdw->prev_buf, entry);
	if (IS_ERR_OR_NULL(p_svdw->prev_buf) || IS_ERR_OR_NULL(p_svdw->curr_buf)) {
		trace_printk("prev(0x%p) or curr(0x%p) is wrong\n", p_svdw->prev_buf,
			     p_svdw->curr_buf);
		goto odw_update;
	}

	vb = &p_svdw->curr_buf->buf.vb2_buf;
	(void)memset(dma_addrs, 0, sizeof(dma_addrs));
	tcc_svdw_get_dma_addrs(p_svdw, vb, dma_addrs);
	tcc_svdw_print_dma_addrs(p_svdw, vb, dma_addrs);
	for (idxpln = 0; idxpln < vb->num_planes; idxpln++) {
		out->address[idxpln] = dma_addrs[idxpln];
	}

	tcc_svdw_get_and_update_time(p_svdw, &p_svdw->ts_prev, &p_svdw->ts_next, 1);

	if (!IS_ERR_OR_NULL(p_svdw->prev_buf) && !IS_ERR_OR_NULL(p_svdw->curr_buf)) {
		/* fill the buffer info */
		p_svdw->prev_buf->buf.vb2_buf.timestamp =
			clamp_t(u64, timespec64_to_ns(&p_svdw->ts_next), 0UL, S64_MAX);
		p_svdw->prev_buf->buf.field = (u32)V4L2_FIELD_NONE;

		/* dequeue prev_buf from the incoming buffer list */
		list_del(&p_svdw->prev_buf->entry);

		/* queue the prev_buf to the outgoing buffer list */
		vb2_buffer_done(&p_svdw->prev_buf->buf.vb2_buf, VB2_BUF_STATE_DONE);
		trace_printk("buffer_done sequence: %d\n", p_svdw->prev_buf->buf.sequence);
	}

	trace_printk("[buffer >= 3] svdw_set_view_address_l: 0x%08x\n", out->address[0]);
	svdw_set_view_address_l(out->address[0]);
	p_svdw->curr_buf->buf.sequence = p_svdw->sequence;

odw_update:
	p_svdw->sequence = clamp(p_svdw->sequence + 1U, 0U, UINT_MAX);
	spin_unlock_irqrestore(&p_queue->slock, flags);

	return IRQ_HANDLED;
}

struct tcc_svdw_dewarp_unit *tcc_svdw_g_dewarp_unit_by_vdev(struct tcc_cap_media *tcc_md,
							    struct video_device *p_vdev)
{
	struct list_head *cursor;
	struct tcc_svdw_dewarp_unit *dwrp_unit;

	list_for_each (cursor, &tcc_svdw_dewarp_list) {
		dwrp_unit = container_of(cursor, struct tcc_svdw_dewarp_unit, anchor);
		if (dwrp_unit->vdev == p_vdev) {
			return dwrp_unit;
		}
	}
	return NULL;
}

int tcc_svdw_s_fmt_dewarpers(struct tcc_svdw_device *p_svdw)
{
	int ret = 0;
	struct media_pad *r_pad;
	struct media_entity *src;
	struct video_device *vdev;
	struct tcc_svdw_dewarp_unit *dwrp_unit;
	const struct tcc_svdw_ops *dwrp_ops;

	u32 pad_idx;

	for (pad_idx = 0U; pad_idx < TCC_SVDW_PADS_MAX; pad_idx++) {
		r_pad = media_entity_remote_pad(&p_svdw->pads[pad_idx]);
		if (r_pad) {
			src = r_pad->entity;
			vdev = (struct video_device *)container_of(src, struct video_device,
								   entity);
			dwrp_unit = tcc_svdw_g_dewarp_unit_by_vdev(p_svdw->tccmd, vdev);
			dwrp_ops = dwrp_unit->ops;
			if (dwrp_ops) {
				ret = dwrp_ops->s_fmt(dwrp_unit->vdev, &p_svdw->format.fmt.pix_mp);
				if (ret < 0) {
					loge(&p_svdw->pdev->dev,
					     "Failed to get patch address (idx: %d)\n", pad_idx);
				}
			} else {
				loge(&p_svdw->pdev->dev, "dwrp_ops is not initialized");
			}
		} else {
			loge(&p_svdw->pdev->dev, "Failed to find remote pad\n");
			ret = -EINVAL;
			break;
		}
	}
	return ret;
}

int tcc_svdw_g_patch_addresses(struct tcc_svdw_device *p_svdw)
{
	int ret = 0;
	struct media_pad *r_pad;
	struct media_entity *src;
	struct video_device *vdev;
	struct tcc_svdw_dewarp_unit *dwrp_unit;
	const struct tcc_svdw_ops *dwrp_ops;

	u32 pad_idx;
	phys_addr_t p_addr;

	for (pad_idx = 0U; pad_idx < TCC_SVDW_PADS_MAX; pad_idx++) {
		r_pad = media_entity_remote_pad(&p_svdw->pads[pad_idx]);
		if (r_pad) {
			src = r_pad->entity;
			vdev = (struct video_device *)container_of(src, struct video_device,
								   entity);
			dwrp_unit = tcc_svdw_g_dewarp_unit_by_vdev(p_svdw->tccmd, vdev);
			dwrp_ops = dwrp_unit->ops;
			if (dwrp_ops) {
				ret = dwrp_ops->g_phys_addr(dwrp_unit->vdev, &p_addr);
				if (ret < 0) {
					loge(&p_svdw->pdev->dev,
					     "Failed to get patch address (idx: %d)\n", pad_idx);
					p_svdw->patch_addrs[pad_idx] = 0U;
				} else {
					p_svdw->patch_addrs[pad_idx] = p_addr;
				}
			} else {
				loge(&p_svdw->pdev->dev, "dwrp_ops is not initialized");
			}
		} else {
			loge(&p_svdw->pdev->dev, "Failed to find remote pad\n");
			ret = -EINVAL;
			break;
		}
	}
	return ret;
}

static int tcc_svdw_start_dewarps(struct tcc_svdw_device *p_svdw)
{
	int ret = 0;
	u32 pad_idx;
	struct media_entity *src;
	struct media_pad *r_pad;
	struct video_device *vdev;
	struct tcc_svdw_dewarp_unit *dwrp_unit;
	const struct tcc_svdw_ops *dwrp_ops;

	for (pad_idx = 0; pad_idx < TCC_SVDW_PADS_MAX; pad_idx++) {
		r_pad = media_entity_remote_pad(&p_svdw->pads[pad_idx]);
		if (r_pad) {
			src = r_pad->entity;
			vdev = (struct video_device *)container_of(src, struct video_device,
								   entity);
			dwrp_unit = tcc_svdw_g_dewarp_unit_by_vdev(p_svdw->tccmd, vdev);
			dwrp_ops = dwrp_unit->ops;
			if (dwrp_ops) {
				ret = dwrp_ops->start_streaming(dwrp_unit->vdev);
			} else {
				loge(&p_svdw->pdev->dev, "dwrp_ops is not initialized");
			}
		} else {
			loge(&p_svdw->pdev->dev, "Failed to find remote pad\n");
			ret = -EINVAL;
			break;
		}
	}

	return ret;
}

static int tcc_svdw_start_svdw(struct tcc_svdw_device *p_svdw)
{
	int ret = 0;
	int idx = 0;
	u32 offset;
	u32 width, height;
	u32 rmem_base;
	struct tcc_svdw_queue *p_queue;
	struct v4l2_pix_format_mplane *pix_mp;

	p_queue = &p_svdw->queue;

	/* view address - buffer address for svdw */
	/* TODO format must be set with fmt mapping table */
	if (!IS_ERR_OR_NULL(p_svdw->svdw_wrap.rsvd_mem[SVDW_MEMBLOCK_PREV])) {
		rmem_base = clamp_t(u32, p_svdw->svdw_wrap.rsvd_mem[SVDW_MEMBLOCK_PREV]->base, 0,
				    U32_MAX);
		svdw_set_view_address_l(rmem_base);
	} else {
		loge(&p_svdw->pdev->dev, "Wrong reserved memory!\n");
	}

	pix_mp = &p_svdw->format.fmt.pix_mp;
	width = pix_mp->width;
	height = pix_mp->height;

	svdw_enable_all_patches();

	svdw_set_view_stride_l(width * 4, 0);
	svdw_set_view_stride_c(0, 0);
	svdw_set_view_size(width, height);
	svdw_set_view_format(FMT_RGB);

	svdw_set_default_color(0x404040);
	svdw_set_buffer_mode(0x0, 0x3);
	svdw_set_view_hole_filling(0x0);

	svdw_set_recon_timer_thres(0x0cb7355);
	svdw_enable_recon_timer(true);

	svdw_set_axi_config(0x3f, 0x3f, 0x0f, 0x0f);
	/* svdw_set_rdone_interrupt_delay(0x0); */

	svdw_set_frame_time_cycle(0x0ba2840);
	svdw_set_frame_timeout_threshold(0x0ba2840);
	svdw_set_frame_fast_threshold(0x1000);
	svdw_set_frame_slow_threshold(0x1000);
	svdw_set_frame_toggle_threshold(0x200);
	svdw_set_line_toggle_threshold(0x300);
	svdw_set_max_toggle_threshold(0x4);

	offset = width * height * 4;
	for (idx = 0; idx < TCC_SVDW_DEWARP_MAX; idx++) {
		svdw_set_patch_address(idx, p_svdw->patch_addrs[idx],
				       p_svdw->patch_addrs[idx] + offset,
				       p_svdw->patch_addrs[idx] + (offset * 2));
	}

	/* patch #0 */
	svdw_enable(0, 0x1, 0x1, 0x0);
	svdw_set_bypass_stride_l(0, 0x1400, 0x0);
	svdw_set_bypass_stride_c(0, 0x5, 0x0);
	svdw_set_bypass_format(0, 0x0, 0);
	svdw_set_input_size(0, 0x500, 0x2d0);
	svdw_set_input_format(0, 0x4, 0);
	svdw_set_patch_stride_l(0, 0x1400, 0x0);
	svdw_set_patch_stride_c(0, 0x0, 0x0);
	svdw_set_patch_chroma_offset(0, 0x280000);
	svdw_set_patch_size(0, 0x500, 0x104);
	svdw_set_patch_offset(0, 0, 0);
	svdw_set_patch_format(0, 0x0, 0);

	/* patch #1 */
	svdw_enable(1, 0x1, 0x1, 0x0);
	svdw_set_bypass_stride_l(1, 0x1400, 0x0);
	svdw_set_bypass_stride_c(1, 0x5, 0x0);
	svdw_set_bypass_format(1, 0x0, 0);
	svdw_set_input_size(1, 0x500, 0x2d0);
	svdw_set_input_format(1, 0x4, 0);
	svdw_set_patch_stride_l(1, 0x0b40, 0x0);
	svdw_set_patch_stride_c(1, 0x0, 0x0);
	svdw_set_patch_chroma_offset(1, 0x280000);
	svdw_set_patch_size(1, 0x2d0, 0x186);
	svdw_set_patch_offset(1, 0x37a, 0);
	svdw_set_patch_format(1, 0x0, 0);

	/* patch #2 */
	svdw_enable(2, 0x1, 0x1, 0x0);
	svdw_set_bypass_stride_l(2, 0x1400, 0x0);
	svdw_set_bypass_stride_c(2, 0x5, 0x0);
	svdw_set_bypass_format(2, 0x0, 0);
	svdw_set_input_size(2, 0x500, 0x2d0);
	svdw_set_input_format(2, 0x4, 0);
	svdw_set_patch_stride_l(2, 0x1400, 0x0);
	svdw_set_patch_stride_c(2, 0x0, 0x0);
	svdw_set_patch_chroma_offset(2, 0x280000);
	svdw_set_patch_size(2, 0x500, 0x104);
	svdw_set_patch_offset(2, 0x0, 0x1cc);
	svdw_set_patch_format(2, 0x0, 0);

	/* patch #3 */
	svdw_enable(3, 0x1, 0x1, 0x0);
	svdw_set_bypass_stride_l(3, 0x1400, 0x0);
	svdw_set_bypass_stride_c(3, 0x5, 0x0);
	svdw_set_bypass_format(3, 0x0, 0);
	svdw_set_input_size(3, 0x500, 0x2d0);
	svdw_set_input_format(3, 0x4, 0);
	svdw_set_patch_stride_l(3, 0x0b40, 0x0);
	svdw_set_patch_stride_c(3, 0x0, 0x0);
	svdw_set_patch_chroma_offset(3, 0x280000);
	svdw_set_patch_size(3, 0x2d0, 0x186);
	svdw_set_patch_offset(3, 0x0, 0x0);
	svdw_set_patch_format(3, 0x0, 0);

	/* disable bypass address */
	for (idx = 0; idx < TCC_SVDW_DEWARP_MAX; idx++) {
		svdw_set_bypass_address_l(idx, 0);
		svdw_set_bypass_address_c(idx, 0);

		/* update CAM */
		svdw_update(idx);
	}

	svdw_set_recon_start(NULL, 0x1);

	return ret;
}

static int tcc_svdw_start_stream(struct tcc_svdw_device *p_svdw)
{
	const struct device *p_dev = NULL;
	const struct v4l2_pix_format_mplane *pix_mp = NULL;
	int ret = 0;

	p_dev = svdw_to_dev(p_svdw);
	pix_mp = &p_svdw->format.fmt.pix_mp;

	p_svdw->sequence = 0;

	logd(p_dev, "preview size: %d * %d\n", pix_mp->width, pix_mp->height);

	ret = tcc_svdw_start_dewarps(p_svdw);
	if (ret < 0) {
		loge(p_dev, "tcc_svdw_start_dewarps, ret: %d\n", ret);
	}

	ret = tcc_svdw_load_fw_cfg(p_svdw);
	if (ret < 0) {
		loge(p_dev, "tcc_svdw_load_fw_cfg: skip the config error(%d)\n", ret);
	}

	ret = tcc_svdw_draw_overlay_img(p_svdw);
	if (ret < 0) {
		loge(p_dev, "tcc_svdw_draw_overlay_img, ret: %d\n", ret);
	}

	ret = tcc_svdw_start_svdw(p_svdw);
	if (ret < 0) {
		loge(p_dev, "tcc_svdw_start_svdw, ret: %d\n", ret);
	}

	/* Enable svdw_rdone interupt */
	svdw_set_ireq_mask(p_dev, 0);

	return ret;
}

static void tcc_svdw_stop_dewarps(struct tcc_svdw_device *p_svdw)
{
	u32 pad_idx;
	int ret;
	struct media_entity *src;
	struct media_pad *r_pad;
	struct video_device *vdev;
	struct tcc_svdw_dewarp_unit *dwrp_unit;
	const struct tcc_svdw_ops *dwrp_ops;

	for (pad_idx = 0; pad_idx < TCC_SVDW_PADS_MAX; pad_idx++) {
		r_pad = media_entity_remote_pad(&p_svdw->pads[pad_idx]);
		if (r_pad) {
			src = r_pad->entity;
			vdev = (struct video_device *)container_of(src, struct video_device,
								   entity);
			dwrp_unit = tcc_svdw_g_dewarp_unit_by_vdev(p_svdw->tccmd, vdev);
			dwrp_ops = dwrp_unit->ops;
			if (dwrp_ops) {
				ret = dwrp_ops->stop_streaming(dwrp_unit->vdev);
			} else {
				loge(&p_svdw->pdev->dev, "dwrp_ops is not initialized");
			}
		} else {
			loge(&p_svdw->pdev->dev, "Failed to find remote pad\n");
			ret = -EINVAL;
			break;
		}
	}
}

static void tcc_svdw_stop_svdw(struct tcc_svdw_device *p_svdw)
{
	int idx = 0;

	for (idx = 0; idx < TCC_SVDW_DEWARP_MAX; idx++) {
		svdw_enable(idx, 0, 0, 0);
	}
}

static void tcc_svdw_stop_stream(struct tcc_svdw_device *p_svdw)
{
	tcc_svdw_stop_svdw(p_svdw);
	tcc_svdw_stop_dewarps(p_svdw);
}

static int tcc_svdw_request_irq(struct tcc_svdw_device *p_svdw)
{
	const struct device *p_dev = NULL;
	struct tcc_svdw_wrap *svdw_wrap = NULL;
	struct svdw_intr *intr = NULL;
	int ret_call = 0;
	int ret = 0;

	p_dev = svdw_to_dev(p_svdw);
	svdw_wrap = &p_svdw->svdw_wrap;

	intr = &svdw_wrap->intr[SVDW_INTR_IDX_SVM_RDONE];
	ret_call = request_threaded_irq(intr->num, NULL, tcc_svdw_isr_rdone, IRQF_ONESHOT,
					p_svdw->vdev.name, p_svdw);
	if (ret_call < 0) {
		loge(p_dev, "svdw(odw) - request_irq, ret: %d\n", ret_call);
		ret = -EINVAL;
	}

	return ret;
}

void tcc_svdw_free_irq(struct tcc_svdw_device *p_svdw)
{
	const struct device *p_dev = NULL;
	struct tcc_svdw_wrap *svdw_wrap = NULL;
	struct svdw_stitch_params *stitch_params = NULL;
	struct svdw_intr *intr = NULL;

	p_dev = svdw_to_dev(p_svdw);

	svdw_wrap = &p_svdw->svdw_wrap;
	intr = &svdw_wrap->intr[SVDW_INTR_IDX_SVM_RDONE];
	stitch_params = &svdw_wrap->stitch_params;

	(void)free_irq(clamp_t(u32, intr->num, 0, INT_MAX), p_svdw);
	svdw_set_ireq_mask(p_dev, 1);
}

int tcc_svdw_add_link(struct platform_device *pdev, struct media_entity *l_ent,
		      struct media_entity *r_ent, struct v4l2_fwnode_link *flink)
{
	int ret = 0;
	u16 port_to_alloc;

	struct tcc_svdw_device *p_svdw;

	p_svdw = (struct tcc_svdw_device *)platform_get_drvdata(pdev);
	if (r_ent == NULL || l_ent == NULL) {
		loge(&p_svdw->pdev->dev, "Entity is NULL\n");
		return -EINVAL;
	}

	port_to_alloc = atomic_read(&p_svdw->entity_cnt);

	logd(&pdev->dev, "source_pad: %d, source->num_pads: %d, sink_pad: %d, sink->num_pads: %d\n",
	     flink->remote_port, r_ent->num_pads, flink->local_port, l_ent->num_pads);

	if (media_entity_find_link(&r_ent->pads[1], &l_ent->pads[port_to_alloc]) == NULL) {
		ret = media_create_pad_link(r_ent, 1U, l_ent, port_to_alloc,
					    (MEDIA_LNK_FL_ENABLED | MEDIA_LNK_FL_IMMUTABLE));
		if (ret < 0) {
			loge(&pdev->dev, "Failed to create pad link, rp: %d, lp: %d\n",
			     flink->remote_port, port_to_alloc);
		} else {
			logd(&pdev->dev, "Success to create pad link, rp: %d, lp: %d\n",
			     flink->remote_port, port_to_alloc);
			atomic_add(1, &p_svdw->entity_cnt);
		}
	} else {
		loge(&pdev->dev, "The link already exists: %d(port_to_alloc)\n", port_to_alloc);
	}

	return ret;
}

struct media_entity *tcc_svdw_g_entity_by_fwnode(struct platform_device *pdev,
						 struct fwnode_handle *handle)
{
	struct device *dev;
	struct media_entity *ret = NULL;
	struct list_head *cursor;
	struct tcc_svdw_dewarp_unit *dwrp_unit;

	dev = get_dev_from_fwnode(handle);
	if (dev == NULL) {
		loge(&pdev->dev, "Failed to get dev from fwnode_handle\n");
		goto done;
	}

	list_for_each (cursor, &tcc_svdw_dewarp_list) {
		dwrp_unit = container_of(cursor, struct tcc_svdw_dewarp_unit, anchor);
		if (dwrp_unit->dev == dev) {
			return &dwrp_unit->vdev->entity;
		}
	}

done:
	return ret;
}

int tcc_svdw_init_links(struct tcc_svdw_device *p_svdw)
{
	struct platform_device *p_pdev;
	struct video_device *l_vdev;
	struct device_node *lep, *rep;
	struct v4l2_fwnode_link flink;
	struct media_entity *ent_lep, *ent_rep;
	struct fwnode_handle *fw_handle;

	int ret = 0;

	p_pdev = p_svdw->pdev;
	l_vdev = &p_svdw->vdev;
	logi(&p_pdev->dev, "Find endpoints and init links: \n");

	atomic_set(&p_svdw->entity_cnt, 0);

	for_each_endpoint_of_node (p_pdev->dev.of_node, lep) {
		if (lep == NULL) {
			loge(&p_pdev->dev, "lep is NULL\n");
			ret = -EINVAL;
			goto done;
		}
		logd(&p_pdev->dev, "Found local ep: %s\n", lep->name);

		rep = of_graph_get_remote_endpoint(lep);
		if (rep == NULL) {
			loge(&p_pdev->dev, "Failed to get remote endpoint: "
					   "no remote ep specified\n");
			ret = -EINVAL;
			goto done;
		}
		logd(&p_pdev->dev, "Found remote ep: %s\n", rep->name);

		fw_handle = of_fwnode_handle(lep);
		if (fw_handle == NULL) {
			loge(&p_pdev->dev, "Failed to get fwnode from device_node\n");
			ret = -EINVAL;
			goto done;
		}
		ret = v4l2_fwnode_parse_link(fw_handle, &flink);
		if (ret < 0) {
			loge(&p_pdev->dev, "Failed to parse lep: %d\n", ret);
			goto done;
		}
		logd(&p_pdev->dev, "local(%s) port: %u, remote(%s) port: %u\n",
		     dev_name(flink.local_node->dev), flink.local_port,
		     dev_name(flink.remote_node->dev), flink.remote_port);

		ent_lep = &l_vdev->entity;
		ent_rep = tcc_svdw_g_entity_by_fwnode(p_pdev, flink.remote_node);

		if (IS_ERR_OR_NULL(ent_lep->graph_obj.mdev)) {
			/* Register the local media entity explicitly since the
			 * bridge device cannot be handled automatically by the
			 * framework */
			ret = media_device_register_entity(&p_svdw->tccmd->md, ent_lep);
			if (ret < 0) {
				loge(&p_pdev->dev, "Failed to register ent_lep\n");
			}
		}

		ret = tcc_svdw_add_link(p_pdev, ent_lep, ent_rep, &flink);
		if (ret < 0) {
			loge(&p_pdev->dev, "Failed to establish links: %d\n", ret);
			goto done;
		}

		v4l2_fwnode_put_link(&flink);
	}
done:
	return ret;
}

static int tcc_svdw_init_pads(struct tcc_svdw_device *p_svdw)
{
	struct device *p_dev = NULL;
	struct video_device *p_vdev = NULL;
	int sink_idx = 0;
	int ret = 0;

	p_dev = svdw_to_dev(p_svdw);
	p_vdev = &p_svdw->vdev;

	for (sink_idx = 0; sink_idx < TCC_SVDW_PADS_MAX; sink_idx++) {
		p_svdw->pads[sink_idx].flags = MEDIA_PAD_FL_SINK;
		if (ret < 0) {
			loge(p_dev, "media_entity_pads_init, ret: %d\n", ret);
			goto fin;
		}
	}

	p_vdev->entity.function = MEDIA_ENT_F_IO_V4L;
	ret = media_entity_pads_init(&p_svdw->vdev.entity, TCC_SVDW_PADS_MAX, p_svdw->pads);
	if (ret < 0) {
		loge(p_dev, "Failed to init media_entity pads of svdw\n");
		goto fin;
	}

fin:
	return ret;
}

static int tcc_svdw_init_vdev(struct tcc_svdw_device *p_svdw)
{
	struct device *p_dev = NULL;
	struct video_device *p_vdev = NULL;
	int ret = 0;

	p_dev = svdw_to_dev(p_svdw);
	p_vdev = &p_svdw->vdev;

	p_vdev->dev_parent = p_dev;
	p_vdev->fops = &tcc_svdw_fops;
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
	p_vdev->vfl_type = VFL_TYPE_VIDEO;
#else
	vdev->vfl_type = VFL_TYPE_GRABBER;
#endif
	p_vdev->ioctl_ops = &tcc_svdw_ioctl_ops;
	p_vdev->minor = 0;
	p_vdev->release = video_device_release_empty;
	p_vdev->device_caps = (u32)V4L2_CAP_STREAMING | (u32)V4L2_CAP_VIDEO_CAPTURE |
			      (u32)V4L2_CAP_VIDEO_CAPTURE_MPLANE;
	p_vdev->queue = &p_svdw->queue.queue;

	(void)strscpy(p_vdev->name, KBUILD_MODNAME, sizeof(KBUILD_MODNAME));

	return ret;
}

static int tcc_svdw_register_to_mdev(struct tcc_svdw_device *p_svdw)
{
	const struct device *p_dev = NULL;
	const struct fwnode_handle *tccmd_fwnode = NULL;
	struct device_node *tccmd_of_node = NULL;
	int ret = 0;

	p_dev = svdw_to_dev(p_svdw);

	tccmd_of_node = of_parse_phandle(p_svdw->pdev->dev.of_node, "mediadev", 0);
	if (tccmd_of_node == NULL) {
		ret = -ENODEV;
		loge(p_dev, "can not fine \"mediadev\". check device tree\n");
	}

	if (ret >= 0) {
		tccmd_fwnode = of_fwnode_handle(tccmd_of_node);
		if (tccmd_fwnode == NULL) {
			loge(p_dev, "wrong fwnode. check device tree\n");
			of_node_put(tccmd_of_node);
			ret = -ENODEV;
		}
	}

	if (ret >= 0) {
		ret = tcc_cap_media_register_capture_dev(&p_svdw->vdev, tccmd_fwnode);
		if (ret != 0) {
			loge(p_dev, "tcc_cap_media_register_capture_dev, ret: %d\n", ret);
		} else {
			p_svdw->tccmd = dev_get_drvdata(p_svdw->vdev.v4l2_dev->dev);
			logi(p_dev, "success register to the %s\n", p_svdw->tccmd->v4l2_dev.name);
		}

		of_node_put(tccmd_of_node);
		atomic_set(&tcc_svdw_dewarp_cnt, SVDW_MAX_DEWARP_CNT);
	}

	return ret;
}

void tcc_svdw_video_deinit(struct tcc_svdw_device *p_svdw)
{
	tcc_svdw_put_clock(p_svdw);
}
EXPORT_SYMBOL_GPL(tcc_svdw_video_deinit);

bool tcc_svdw_video_is_pixelformat_supported(struct tcc_svdw_device *p_svdw, u32 pixelformat)
{
	const struct device *p_dev = NULL;
	const struct tcc_svdw_format *tformat = NULL;
	bool ret = true;

	p_dev = svdw_to_dev(p_svdw);

	tformat = tcc_svdw_get_fmt_by_pixfmt(pixelformat);
	if (tformat == NULL) {
		loge(p_dev, "pixelformat(0x%08x) is not supported\n", pixelformat);
		ret = false;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(tcc_svdw_video_is_pixelformat_supported);

u32 tcc_svdw_video_get_pixelformat_by_index(struct tcc_svdw_device *p_svdw, u32 index)
{
	const struct device *p_dev = NULL;
	u32 nList = 0;
	u32 pixelformat = 0;

	p_dev = svdw_to_dev(p_svdw);

	nList = ARRAY_SIZE(tcc_svdw_format_list);
	if (index < nList) {
		/* get pixelformat */
		pixelformat = tcc_svdw_format_list[index].pixelformat;
	}

	return pixelformat;
}
EXPORT_SYMBOL_GPL(tcc_svdw_video_get_pixelformat_by_index);

bool tcc_svdw_video_is_format_supported(struct tcc_svdw_device *p_svdw,
					const struct v4l2_format *format)
{
	const struct device *p_dev = NULL;
	const struct v4l2_pix_format_mplane *pix_mp = NULL;
	bool ret_bool = true;
	bool ret = true;

	p_dev = svdw_to_dev(p_svdw);
	pix_mp = &format->fmt.pix_mp;

	if (format->type != (u32)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		/* the format is supported */
		ret = false;
	}

	ret_bool = tcc_svdw_video_is_framesize_supported(p_svdw, pix_mp->width, pix_mp->height);
	if (!ret_bool) {
		loge(p_dev, "tcc_svdw_video_is_framesize_supported\n");
		ret = false;
	}

	ret_bool = tcc_svdw_video_is_pixelformat_supported(p_svdw, pix_mp->pixelformat);
	if (!ret_bool) {
		loge(p_dev, "tcc_svdw_video_is_pixelformat_supported\n");
		ret = false;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(tcc_svdw_video_is_format_supported);

int tcc_svdw_init_coherent_dma_memory(struct tcc_svdw_device *p_svdw)
{
	struct device *p_dev = NULL;
	int ret = 0;

	p_dev = svdw_to_dev(p_svdw);

	ret = of_reserved_mem_device_init_by_idx(p_dev, p_dev->of_node, 0);
	if (ret < 0) {
		loge(p_dev, "Failed to init per-device memory: %d\n", ret);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(tcc_svdw_init_coherent_dma_memory);

int tcc_svdw_video_streamon(struct tcc_svdw_device *p_svdw)
{
	const struct device *p_dev = NULL;
	int ret = 0;

	p_dev = svdw_to_dev(p_svdw);

	ret = tcc_svdw_enable_clock(p_svdw);
	if (ret < 0) {
		loge(p_dev, "tcc_svdw_enable_clock, ret: %d\n", ret);
		goto done;
	}

	ret = tcc_svdw_start_stream(p_svdw);
	if (ret < 0) {
		loge(p_dev, "tcc_svdw_start_stream, ret: %d\n", ret);
		goto done;
	}

done:
	return ret;
}
EXPORT_SYMBOL_GPL(tcc_svdw_video_streamon);

int tcc_svdw_video_streamoff(struct tcc_svdw_device *p_svdw)
{
	const struct device *p_dev = NULL;
	int ret = 0;

	p_dev = svdw_to_dev(p_svdw);
	tcc_svdw_stop_stream(p_svdw);

	/* Disable svdw rdone interrupt */
	svdw_set_ireq_mask(p_dev, 1);
	tcc_svdw_disable_clock(p_svdw);

	return ret;
}
EXPORT_SYMBOL_GPL(tcc_svdw_video_streamoff);

int tcc_svdw_init_parse_dt(struct tcc_svdw_device *p_svdw, struct device_node *dev_node)
{
	const struct device *p_dev = NULL;
	int ret_call = 0;
	int ret = 0;

	p_dev = svdw_to_dev(p_svdw);

	ret_call = tcc_svdw_get_clock(p_svdw, dev_node);
	if (ret_call != 0) {
		loge(p_dev, "tcc_svdw_get_clock, ret: %d\n", ret_call);
		ret = -ENODEV;
	}

	ret_call = tcc_svdw_parse_props(p_svdw, dev_node);
	if (ret_call != 0) {
		loge(p_dev, "tcc_svdw_parse_props, ret: %d\n", ret_call);
		ret = -ENODEV;
	}

	ret_call = tcc_svdw_parse_reserved_memory(p_svdw, dev_node);
	if (ret_call != 0) {
		loge(p_dev, "tcc_svdw_parse_reserved_memory, ret: %d\n", ret_call);
		ret = -ENODEV;
	}

	ret_call = tcc_svdw_parse_fwnode(p_svdw, dev_node);
	if (ret_call != 0) {
		logd(p_dev, "tcc_svdw_parse_fwnode, ret: %d\n", ret_call);
		ret = -ENODEV;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(tcc_svdw_init_parse_dt);

static int tcc_svdw_init_v4l2(struct tcc_svdw_device *p_svdw)
{
	struct device *p_dev = NULL;
	struct video_device *p_vdev = NULL;
	int ret = 0;

	p_dev = svdw_to_dev(p_svdw);
	p_vdev = &p_svdw->vdev;

	ret = tcc_svdw_register_to_mdev(p_svdw);
	if (ret != 0) {
		loge(p_dev, "tcc_svdw_register_vdev, ret: %d\n", ret);
		goto fin;
	}

	ret = tcc_svdw_init_vb2_queue(p_svdw, false);
	if (ret != 0) {
		loge(p_dev, "tcc_svdw_v4l2_init_queue, ret: %d\n", ret);
		goto fin;
	}

	ret = tcc_svdw_init_vdev(p_svdw);
	if (ret < 0) {
		loge(p_dev, "tcc_svdw_init_vdev, ret: %d\n", ret);
		goto fin;
	}

	ret = tcc_svdw_init_pads(p_svdw);
	if (ret < 0) {
		loge(p_dev, "tcc_svdw_init_pads, ret: %d\n", ret);
		goto fin;
	}

	ret = video_register_device(p_vdev, p_vdev->vfl_type, p_vdev->minor);
	if (ret < 0) {
		loge(p_dev, "video_register_device, ret: %d\n", ret);
		goto fin;
	}

	ret = tcc_svdw_init_links(p_svdw);
	if (ret < 0) {
		loge(p_dev, "tcc_svdw_init_links, ret: %d\n", ret);
		goto fin;
	}

	ret = tcc_svdw_init_v4l2_fmt(p_svdw);
	if (ret != 0) {
		loge(p_dev, "tcc_svdw_init_v4l2_fmt, ret: %d\n", ret);
		goto fin;
	}
fin:
	return ret;
}

void tcc_svdw_init(struct tcc_svdw_device *p_svdw, struct platform_device *p_pdev)
{
	const struct device *p_dev = NULL;
	int ret = 0;

	p_dev = &p_pdev->dev;
	mutex_init(&p_svdw->mlock);
	video_set_drvdata(&p_svdw->vdev, p_svdw);
	p_svdw->pdev = p_pdev;

	ret = tcc_svdw_init_parse_dt(p_svdw, p_pdev->dev.of_node);
	if (ret < 0) {
		loge(p_dev, "tcc_svdw_video_init, ret: %d\n", ret);
		goto fin;
	}

	ret = tcc_svdw_init_v4l2(p_svdw);
	if (ret != 0) {
		loge(p_dev, "tcc_svdw_register, ret: %d\n", ret);
		goto fin;
	}

	ret = tcc_svdw_do_ioremap(p_svdw, p_pdev->dev.of_node);
	if (ret != 0) {
		loge(p_dev, "tcc_svdw_do_ioremap, ret: %d\n", ret);
		goto fin;
	}

	ret = tcc_svdw_request_irq(p_svdw);
	if (ret < 0) {
		loge(p_dev, "Failed to request irq\n");
		goto fin;
	}

	ret = tcc_svdw_init_coherent_dma_memory(p_svdw);
	if (ret != 0) {
		loge(&p_svdw->pdev->dev, "tcc_svdw_video_declare_coherent_dma_memory, ret: %d\n",
		     ret);
	}

fin:
	return;
}
EXPORT_SYMBOL(tcc_svdw_init);
