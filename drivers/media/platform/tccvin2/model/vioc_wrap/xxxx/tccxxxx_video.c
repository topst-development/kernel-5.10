// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *      tccvin_video.c  --  Telechips Video-Input Path Driver
 *
 *      Copyright (C) 2005-2010
 *          Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 ******************************************************************************


 *   Modified by Telechips Inc.


 *   Modified date : 2020


 *   Description : Video handling


 *****************************************************************************/

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <linux/clk.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_graph.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/version.h>

#include <media/v4l2-common.h>
#include <media/videobuf2-dma-contig.h>

#include <asm/unaligned.h>
#include "../../../tccvin_common.h"
#include "tccxxxx_video.h"
#include "../../../tccvin_video.h"

/* this spinlock is defined in the tccvin2 driver */
extern spinlock_t cam_mux_cfg_lock;

/* ------------------------------------------------------------------------
 *  * helper macro
 *   */

#define IS_SET(value, mask) (((u32)(value) & (u32)(mask)) != 0U)

/* supported color formats */
struct tccvin_format tccvin_format_list[] = {
	/* RGB formats */
	{
		.pixelformat = V4L2_PIX_FMT_RGB24,
		.guid = VIOC_IMG_FMT_RGB888,
	},
	{
		.pixelformat = V4L2_PIX_FMT_RGB32,
		.guid = VIOC_IMG_FMT_ARGB8888,
	},
	/* sequential (YUV packed) */
	{
		.pixelformat = V4L2_PIX_FMT_UYVY,
		.guid = VIOC_IMG_FMT_UYVY,
	},
	{
		.pixelformat = V4L2_PIX_FMT_VYUY,
		.guid = VIOC_IMG_FMT_VYUY,
	},
	{
		.pixelformat = V4L2_PIX_FMT_YUYV,
		.guid = VIOC_IMG_FMT_YUYV,
	},
	{
		.pixelformat = V4L2_PIX_FMT_YVYU,
		.guid = VIOC_IMG_FMT_YVYU,
	},

	/* two non contiguous planes - one Y, one Cr + Cb interleaved  */
	{
		.pixelformat = V4L2_PIX_FMT_NV12M,
		.guid = VIOC_IMG_FMT_YUV420IL0,
	},
	{
		.pixelformat = V4L2_PIX_FMT_NV21M,
		.guid = VIOC_IMG_FMT_YUV420IL1,
	},
	{
		.pixelformat = V4L2_PIX_FMT_NV16M,
		.guid = VIOC_IMG_FMT_YUV422IL0,
	},
	{
		.pixelformat = V4L2_PIX_FMT_NV61M,
		.guid = VIOC_IMG_FMT_YUV422IL1,
	},

	/* three planes - Y Cb, Cr */
	{
		.pixelformat = V4L2_PIX_FMT_YUV420,
		.guid = VIOC_IMG_FMT_YUV420SEP,
	},
	{
		.pixelformat = V4L2_PIX_FMT_YUV422P,
		.guid = VIOC_IMG_FMT_YUV422SEP,
	},

	/* three non contiguous planes - Y, Cb, Cr */
	{
		.pixelformat = V4L2_PIX_FMT_YUV420M,
		.guid = VIOC_IMG_FMT_YUV420SEP,
	},
	{
		.pixelformat = V4L2_PIX_FMT_YUV422M,
		.guid = VIOC_IMG_FMT_YUV422SEP,
	},
};

struct tccvin_format *tccvin_get_tccvin_format_by_pixelformat(u32 pixelformat)
{
	u32 idxList = 0;
	u32 nList = 0;
	struct tccvin_format *tformat = NULL;

	nList = ARRAY_SIZE(tccvin_format_list);
	for (idxList = 0; idxList < nList; idxList++) {
		tformat = &tccvin_format_list[idxList];
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

u32 tccvin_video_get_pixelformat_by_index(u32 index)
{
	u32 nList = 0;
	u32 pixelformat = 0;

	nList = ARRAY_SIZE(tccvin_format_list);
	if (index < nList) {
		/* get pixelformat */
		pixelformat = tccvin_format_list[index].pixelformat;
	}

	return pixelformat;
}

int tccvin_get_clock(struct tccvin_stream *vstream,
		     struct device_node *dev_node)
{
	const struct device *p_dev = NULL;
	int ret = 0;

	p_dev = stream_to_device(vstream);

	/* get the ddi clock */
	vstream->cif.vioc_clk = of_clk_get(dev_node, 0);
	if (vstream->cif.vioc_clk == NULL) {
		loge(p_dev, "of_clk_get\n");
		ret = -ENODEV;
	}

	return ret;
}

void tccvin_put_clock(const struct tccvin_stream *vstream)
{
	clk_put(vstream->cif.vioc_clk);
}

static int tccvin_enable_clock(const struct tccvin_stream *vstream)
{
	const struct device *p_dev = NULL;
	int ret = 0;

	p_dev = stream_to_device(vstream);

	ret = clk_prepare_enable(vstream->cif.vioc_clk);
	if (ret != 0) {
		/* failure of clk_prepare_enable */
		loge(p_dev, "clk_prepare_enable, ret: %d\n", ret);
	}

	return ret;
}

static void tccvin_disable_clock(const struct tccvin_stream *vstream)
{
	clk_disable_unprepare(vstream->cif.vioc_clk);
}

/*
 * tccvin_parse_ddibus
 *
 * - DESCRIPTION:
 *	Parse video-input path's device tree
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 *	-ENODEV:	a certain device node is not found.
 */

int tccvin_parse_ddibus(struct tccvin_stream *vstream,
			const struct device_node *dev_node)
{
	const struct device *p_dev = NULL;
	struct device_node *vioc_node = NULL;
	struct vioc_comp *vin_path = NULL;
	struct vioc_comp *pvioc = NULL;
	const char *const name_list[VIN_COMP_MAX] = {
		[VIN_COMP_VIN] = "vin",		[VIN_COMP_VIQE] = "viqe",
		[VIN_COMP_SDEINTL] = "deintls", [VIN_COMP_SCALER] = "scaler",
		[VIN_COMP_PGL] = "rdma",	[VIN_COMP_WMIX] = "wmixer",
		[VIN_COMP_WDMA] = "wdma",
	};
	const char *name = NULL;
	u32 idxVioc = 0;
	u32 prop_val = 0;
	u32 wmix_bypass = 0;
	u32 vioc_id = 0;
	int ret = 0;

	p_dev = stream_to_device(vstream);
	vin_path = vstream->cif.vin_path;

	/* cif port */
	vioc_node = of_parse_phandle(dev_node, "cifport", 0);
	if (!IS_ERR_OR_NULL(vioc_node)) {
		(void)of_property_read_u32_index(dev_node, "cifport", 1,
						 &prop_val);
		vstream->cif.cif_port = prop_val;
		vstream->cif.cifport_addr = of_iomap(vioc_node, 0);
		logd(p_dev, "%10s: %u\n", "CIF Port", vstream->cif.cif_port);
	} else {
		loge(p_dev, "\"cifport\" node is not found.\n");
		ret = -ENODEV;
	}

	/* wmix bypass parsing */
	(void)of_property_read_u32_index(dev_node, "wmix-bypass", 0,
					 &wmix_bypass);
	vstream->cif.wmix_bypass = wmix_bypass;
	logd(p_dev, "%12s: %u\n", "WMIX bypass", vstream->cif.wmix_bypass);

	for (idxVioc = 0; idxVioc < (u32)VIN_COMP_MAX; idxVioc++) {
		name = name_list[idxVioc];
		vioc_node = of_parse_phandle(dev_node, name, 0);
		if (!IS_ERR_OR_NULL(vioc_node)) {
			pvioc = &vin_path[idxVioc];

			/* vioc node */
			pvioc->np = vioc_node;

			/* vioc index */
			(void)of_property_read_u32_index(dev_node, name, 1,
							 &prop_val);
			pvioc->index = prop_val;

			/*get vioc id */
			if (idxVioc == (u32)VIN_COMP_VIN) {
				/* if vin, index / 2 */
				vioc_id = get_vioc_index(pvioc->index) / 2U;
			} else {
				/* if not vin, index as it is */
				vioc_id = get_vioc_index(pvioc->index);
			}
			logd(p_dev, "%10s: %u\n", name, vioc_id);

			/* extra property */
			if (idxVioc == (u32)VIN_COMP_PGL) {
				/* Parking Guide Line */
				(void)of_property_read_u32_index(
					dev_node, "use_pgl", 0, &prop_val);
				vstream->cif.use_pgl = prop_val;
				logd(p_dev, "%10s: %u\n", "use_pgl",
				     vstream->cif.use_pgl);
			}

			/* vioc interrupt */
			if ((idxVioc == (u32)VIN_COMP_VIN) ||
			    (idxVioc == (u32)VIN_COMP_WDMA)) {
				prop_val = irq_of_parse_and_map(vioc_node,
								(int)vioc_id);
				pvioc->intr.num =
					clamp_t(s32, prop_val, 0, 255);
				logd(p_dev, "%6s irq: %u\n", name,
				     pvioc->intr.num);
			}
		} else {
			if ((idxVioc == (u32)VIN_COMP_VIN) ||
			    (idxVioc == (u32)VIN_COMP_WDMA)) {
				loge(p_dev, "\"%s\" node is not found.\n",
				     name);
				ret = -ENODEV;
			}
		}
	}

	return ret;
}

/*
 * tccvin_reset_vioc_path
 *
 * - DESCRIPTION:
 *	Reset or clear a certain vioc component.
 *	If vioc manager is enabled, try to ask to main core to reset or clear.
 *	If no response comes from main core, do it directly.
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 *	-1:		The vioc manager device is not found.
 */

int tccvin_reset_vioc_path(const struct tccvin_stream *vstream)
{
	const struct vioc_comp *vin_path = NULL;
	int idxVioc = 0;
	int ret = 0;

	vin_path = vstream->cif.vin_path;

	/* vioc reset */
	for (idxVioc = (int)VIN_COMP_MAX - 1; idxVioc >= 0; idxVioc--) {
		if (!IS_ERR_OR_NULL(vin_path[idxVioc].np)) {
			/* vioc reset */
			if ((vstream->cif.wmix_bypass == BYPASS_MODE) &&
			    (idxVioc == VIN_COMP_WMIX)) {
				logd(p_dev, "%12s\n", "WMIX Not Reset");
			} else {
				VIOC_CONFIG_SWReset(vin_path[idxVioc].index,
						    VIOC_CONFIG_RESET);
				logd(p_dev, "%12s\n", "WMIX Reset");
			}
		}
	}

	/* vioc reset clear */
	for (idxVioc = 0; idxVioc < (int)VIN_COMP_MAX; idxVioc++) {
		if (!IS_ERR_OR_NULL(vin_path[idxVioc].np)) {
			/* vioc reset clear */
			if ((vstream->cif.wmix_bypass == BYPASS_MODE) &&
			    (idxVioc == VIN_COMP_WMIX)) {
				logd(p_dev, "%12s\n", "WMIX Not Reset Clear");
			} else {
				VIOC_CONFIG_SWReset(vin_path[idxVioc].index,
						    VIOC_CONFIG_CLEAR);
				logd(p_dev, "%12s\n", "WMIX Reset Clear");
			}
		}
	}

	return ret;
}

/*
 * tccvin_map_cif_port
 *
 * - DESCRIPTION:
 *	Map cif port to receive video data
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */
static int tccvin_map_cif_port(const struct tccvin_stream *vstream)
{
	const struct device *p_dev = NULL;
	void __iomem *addr = NULL;
	u32 mask = 0xF;
	u32 vin_index = 0;
	u32 value = 0;

	p_dev = stream_to_device(vstream);
	addr = vstream->cif.cifport_addr;

	vin_index = (get_vioc_index(vstream->cif.vin_path[VIN_COMP_VIN].index) /
		     2U);

	spin_lock_irq(&cam_mux_cfg_lock);
	value = ((__raw_readl(addr) & ~(mask << (vin_index * 4U))) |
		 (vstream->cif.cif_port << (vin_index * 4U)));
	__raw_writel(value, addr);

	value = __raw_readl(addr);
	spin_unlock_irq(&cam_mux_cfg_lock);

	logd(p_dev, "CIF Port: %d, VIN Index: %d, Register Value: 0x%08x\n",
	     vstream->cif.cif_port, vin_index, value);

	return 0;
}

static irqreturn_t tccvin_vin_isr(int irq, void *p_data)
{
	struct tccvin_stream *vstream = NULL;
	const struct device *p_dev = NULL;
	struct tccvin_queue *p_queue = NULL;
	const struct vioc_comp *pvioc = NULL;
	const void __iomem *vin = NULL;
	void __iomem *wdma = NULL;
	const struct vioc_intr *intr = NULL;
	const struct vioc_intr_type *intr_src = NULL;
	unsigned long flags = 0;
	u32 status = 0;
	struct vb2_buffer *vb = NULL;
	u32 dma_addrs[MAX_PLANES];

	bool ret_bool = false;
	int ret_call = 0;
	irqreturn_t ret = IRQ_NONE;

	vstream = (struct tccvin_stream *)p_data;
	p_dev = stream_to_device(vstream);
	p_queue = &vstream->queue;

	pvioc = &vstream->cif.vin_path[VIN_COMP_WDMA];
	wdma = VIOC_WDMA_GetAddress(pvioc->index);
	pvioc = &vstream->cif.vin_path[VIN_COMP_VIN];
	vin = VIOC_VIN_GetAddress(pvioc->index);

	intr = &pvioc->intr;
	intr_src = &intr->source;

	ret_bool = is_vioc_intr_activatied(intr_src->id, intr_src->bits);
	if (ret_bool) {
		VIOC_VIN_GetStatus(vin, &status);
		if ((status & VIN_INT_VS_MASK) != 0U) {
			ret_call =
				vioc_intr_clear(intr_src->id, VIN_INT_VS_MASK);
			if (ret_call != 0) {
				/* failure of vioc_intr_clear */
				trace_e(p_dev, "interrupt is not cleared\n");
			}

			spin_lock_irqsave(&p_queue->slock, flags);

			vstream->prev_buf = NULL;
			vstream->next_buf = NULL;

			/* check if frameskip is needed */
			if (vstream->skip_frame_cnt > 0) {
				trace_d(p_dev, "skip frame count: 0x%08x\n",
					vstream->skip_frame_cnt);
				vstream->skip_frame_cnt--;

				goto wdma_update;
			}

			/* check if the incoming buffer list is empty */

			if (list_empty(&vstream->queue.buf_list)) {
				trace_e(p_dev,
					"The incoming buffer list is empty\n");

				goto wdma_update;
			}

			vstream->prev_buf =
				list_first_entry(&p_queue->buf_list,
						 struct tccvin_buffer, entry);

			ret_call = ((p_queue->flags &
				     (u32)TCCVIN_QUEUE_DROP_CORRUPTED) != 0U) ?
					   1 :
					   0;
			if (ret_call != 0) {
				trace_e(p_dev, "The buffer is corrupted\n");
				vstream->prev_buf = NULL;

				goto wdma_update;
			}

			/* check if the incoming buffer list has only one entry */

			if (list_is_last(&vstream->prev_buf->entry,
					 &p_queue->buf_list)) {
				trace_d(p_dev, "driver has only one buffer\n");
				vstream->prev_buf = NULL;

				goto wdma_update;
			}

			/* The incoming buffer list has two or more entries. */

			vstream->next_buf =
				list_next_entry(vstream->prev_buf, entry);

			if (IS_ERR_OR_NULL(vstream->prev_buf) ||
			    IS_ERR_OR_NULL(vstream->next_buf)) {
				trace_e(p_dev,
					"prev(0x%p) or next(0x%p) is wrong\n",
					vstream->prev_buf, vstream->next_buf);

				goto wdma_update;
			}

			trace_d(p_dev,
				"bufidx: %d, type: 0x%08x, memory: 0x%08x\n",
				vstream->next_buf->buf.vb2_buf.index,
				vstream->next_buf->buf.vb2_buf.type,
				vstream->next_buf->buf.vb2_buf.memory);

			vb = &vstream->next_buf->buf.vb2_buf;
			(void)memset(dma_addrs, 0, sizeof(dma_addrs));
			tccvin_get_dma_addrs(vstream, vb, dma_addrs);
			tccvin_print_dma_addrs(vstream, vb, dma_addrs);
			VIOC_WDMA_SetImageBase(wdma, dma_addrs[0], dma_addrs[1],
					       dma_addrs[2]);
		wdma_update:
			spin_unlock_irqrestore(&p_queue->slock, flags);

			VIOC_WDMA_SetImageEnable(wdma, OFF);
		}

		ret = IRQ_HANDLED;
	}

	return ret;
}

static irqreturn_t tccvin_wdma_isr(int irq, void *p_data)
{
	struct tccvin_stream *vstream = NULL;
	const struct device *p_dev = NULL;
	struct tccvin_queue *p_queue = NULL;
	const struct vioc_comp *pvioc = NULL;
	const void __iomem *wdma = NULL;
	const struct vioc_intr *intr = NULL;
	const struct vioc_intr_type *intr_src = NULL;
	unsigned long flags = 0;
	u32 status = 0;

	bool ret_bool = false;
	int ret_call = 0;
	irqreturn_t ret = IRQ_NONE;

	vstream = (struct tccvin_stream *)p_data;
	p_dev = stream_to_device(vstream);
	p_queue = &vstream->queue;

	pvioc = &vstream->cif.vin_path[VIN_COMP_WDMA];
	wdma = VIOC_WDMA_GetAddress(pvioc->index);
	intr = &pvioc->intr;
	intr_src = &intr->source;

	ret_bool = is_vioc_intr_activatied(intr_src->id, intr_src->bits);
	if (ret_bool) {
		VIOC_WDMA_GetStatus(wdma, &status);
		if ((status & VIOC_WDMA_IREQ_EOFR_MASK) != 0U) {
			ret_call = vioc_intr_clear(intr_src->id,
						   VIOC_WDMA_IREQ_EOFR_MASK);
			if (ret_call != 0) {
				/* failure of vioc_intr_clear */
				trace_e(p_dev, "interrupt is not cleared\n");
			}

			tccvin_get_and_update_time(vstream, &vstream->ts_prev,
						   &vstream->ts_next,
						   vstream->timestamp);

			spin_lock_irqsave(&p_queue->slock, flags);

			if (!IS_ERR_OR_NULL(vstream->prev_buf) &&
			    !IS_ERR_OR_NULL(vstream->next_buf)) {
				/* fill the buffer info */
				vstream->prev_buf->buf.vb2_buf.timestamp =
					clamp_t(u64,
						timespec64_to_ns(
							&vstream->ts_next),
						0UL, ULONG_MAX);
				vstream->prev_buf->buf.field =
					(u32)V4L2_FIELD_NONE;
				vstream->sequence = clamp(
					vstream->sequence + 1U, 0U, UINT_MAX);
				vstream->prev_buf->buf.sequence =
					vstream->sequence;

				/* dequeue prev_buf from the incoming buffer list */
				list_del(&vstream->prev_buf->entry);

				/* queue the prev_buf to the outgoing buffer list */
				vb2_buffer_done(&vstream->prev_buf->buf.vb2_buf,
						VB2_BUF_STATE_DONE);

				vstream->prev_buf = NULL;
				vstream->next_buf = NULL;
			}

			spin_unlock_irqrestore(&p_queue->slock, flags);
		}

		ret = IRQ_HANDLED;
	}

	return ret;
}

#if defined(CONFIG_OVERLAY_PGL)
/*
 * tccvin_set_pgl
 *
 * - DESCRIPTION:
 *	Set rdma component to read parking guideline image
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */

int tccvin_set_pgl(const struct tccvin_stream *vstream)
{
	const struct device *p_dev = NULL;
	void __iomem *p_rdma = NULL;
	const struct v4l2_pix_format_mplane *pix_mp = NULL;
	u32 width = 0;
	u32 height = 0;
	u32 format = 0;
	u32 buf_addr = 0;

	p_dev = stream_to_device(vstream);
	p_rdma =
		VIOC_RDMA_GetAddress(vstream->cif.vin_path[VIN_COMP_PGL].index);

	pix_mp = &vstream->format.fmt.pix_mp;

	width = pix_mp->width;
	height = pix_mp->height;
	format = PGL_FORMAT;

#if defined(CONFIG_ARM64)
	buf_addr = clamp_t(u32, vstream->cif.rsvd_mem[RESERVED_MEM_PGL]->base,
			   0, UINT_MAX);
#else
	buf_addr = vstream->cif.rsvd_mem[RESERVED_MEM_PGL]->base;
#endif //defined(CONFIG_ARM64)

	logd(p_dev, "RDMA: 0x%08x, size[%d x %d], format[%d]\n",
	     (u32)(uintptr_t)p_rdma, width, height, format);

	VIOC_RDMA_SetImageFormat(p_rdma, format);
	VIOC_RDMA_SetImageSize(p_rdma, width, height);
	VIOC_RDMA_SetImageOffset(p_rdma, format, width);
	VIOC_RDMA_SetImageBase(p_rdma, buf_addr, 0, 0);
	if (vstream->cif.use_pgl == 1U) {
		VIOC_RDMA_SetImageEnable(p_rdma);
		VIOC_RDMA_SetImageUpdate(p_rdma);
	} else {
		VIOC_RDMA_SetImageDisable(p_rdma);
	}

	return 0;
}
#endif /* defined(CONFIG_OVERLAY_PGL) */

static int tccvin_start_stream(struct tccvin_stream *vstream)
{
	const struct device *p_dev = NULL;
	const struct tccvin_cif *cif = NULL;
	const struct vioc_comp *vin_path = NULL;
	const struct tccvin_vs_info *vs_info = NULL;
	const struct v4l2_pix_format_mplane *pix_mp = NULL;
	int ret_call = 0;
	int ret = 0;

	p_dev = stream_to_device(vstream);
	cif = &vstream->cif;
	vin_path = cif->vin_path;
	vs_info = &vstream->vs_info;

	pix_mp = &vstream->format.fmt.pix_mp;

	/* size info */
	logd(p_dev, "preview size: %d * %d\n", pix_mp->width, pix_mp->height);

	/* map cif-port */
	ret_call = tccvin_map_cif_port(vstream);
	if (ret_call < 0) {
		loge(p_dev, "tccvin_map_cif_port, ret: %d\n", ret_call);
		ret = -1;
	}

#if defined(CONFIG_OVERLAY_PGL)
	/* set rdma for Parking Guide Line */
	if (vstream->cif.use_pgl == 1U) {
		/* enable rdma */
		ret_call = tccvin_set_pgl(vstream);
		if (ret_call < 0) {
			loge(p_dev, "tccvin_set_pgl, ret: %d\n", ret_call);
			ret = -1;
		}
	}
#endif /* defined(CONFIG_OVERLAY_PGL) */

	/* set vin */
	ret_call = tccvin_set_vin(vstream);
	if (ret_call < 0) {
		loge(p_dev, "tccvin_set_vin, ret: %d\n", ret_call);
		ret = -1;
	}

	/* set deinterlacer */
	if (vs_info->interlaced == (u32)V4L2_DV_INTERLACED) {
		/* set Deinterlacer */
		ret_call = tccvin_set_deinterlacer(vstream);
		if (ret_call < 0) {
			loge(p_dev, "tccvin_set_deinterlacer, ret: %d\n",
			     ret_call);
			ret = -1;
		}

		if (!IS_ERR_OR_NULL(vin_path[VIN_COMP_VIQE].np)) {
			/*
			 * set skip 1 frame when use wdma frame by frame mode
			 * set skip 3 frames in case of viqe 3d mode
			 */
			vstream->skip_frame_cnt = 4;
		}
	} else {
		/* set skip 1 frame when use wdma frame by frame mode */
		vstream->skip_frame_cnt = 1;
	}

	/* set scaler */
	if (!IS_ERR_OR_NULL(vin_path[VIN_COMP_SCALER].np)) {
		/* scaler exists */
		ret_call = tccvin_set_scaler(vstream);
		if (ret_call < 0) {
			loge(p_dev, "tccvin_set_scaler, ret: %d\n", ret_call);
			ret = -1;
		}
	}

	/* set wmixer */
	if (!IS_ERR_OR_NULL(vin_path[VIN_COMP_WMIX].np)) {
		/* wmix exists */
		ret_call = tccvin_set_wmixer(vstream);
		if (ret_call < 0) {
			loge(p_dev, "tccvin_set_wmixer, ret: %d\n", ret_call);
			ret = -1;
		}
	}

	/* set wdma */
	ret_call = tccvin_set_wdma(vstream);
	if (ret_call < 0) {
		loge(p_dev, "tccvin_set_wdma, ret: %d\n", ret_call);
		ret = -1;
	}

	return ret;
}

static int tccvin_stop_stream(const struct tccvin_stream *vstream)
{
	const struct device *p_dev = NULL;
	const struct vioc_comp *vin_path = NULL;
	const struct vioc_comp *vioc = NULL;
	const struct tccvin_vs_info *vs_info = NULL;

#if defined(CONFIG_OVERLAY_PGL)
	void __iomem *pgl = NULL;
#endif /* defined(CONFIG_OVERLAY_PGL) */
	void __iomem *wdma = NULL;
	VIOC_PlugInOutCheck plugstate = {
		0,
	};

	p_dev = stream_to_device(vstream);
	vin_path = vstream->cif.vin_path;
	vs_info = &vstream->vs_info;

#if defined(CONFIG_OVERLAY_PGL)
	pgl = VIOC_RDMA_GetAddress(vin_path[VIN_COMP_PGL].index);
#endif /* defined(CONFIG_OVERLAY_PGL) */

	wdma = VIOC_WDMA_GetAddress(vin_path[VIN_COMP_WDMA].index);
	VIOC_WDMA_SetIreqMask(wdma, VIOC_WDMA_IREQ_ALL_MASK, 0x1);
	VIOC_WDMA_SetImageDisable(wdma);

	if (VIOC_WDMA_Get_CAddress(wdma) > 0UL) {
		int idxLoop;
		u32 status;

		/* We set a criteria for a worst-case within 10-fps and
		 * time-out to disable WDMA as 200ms. To be specific, in the
		 * 10-fps environment, the worst case we assume, it has to
		 * wait 200ms at least for 2 frame.
		 */
		for (idxLoop = 0; idxLoop < 10; idxLoop++) {
			VIOC_WDMA_GetStatus(wdma, &status);
			if (IS_SET(status, VIOC_WDMA_IREQ_EOFR_MASK)) {
				/* eof occurs */
				break;
			}
			/* 20msec is minimum in msleep() */
			msleep(20);
		}
	}

	vioc = &vin_path[VIN_COMP_SCALER];
	if (!IS_ERR_OR_NULL(vioc->np)) {
		(void)VIOC_CONFIG_Device_PlugState(vioc->index, &plugstate);
		if ((plugstate.enable == 1U) &&
		    (plugstate.connect_statue == (u32)VIOC_PATH_CONNECTED)) {
			(void)VIOC_CONFIG_PlugOut(vioc->index);
		}
	}

	if (vs_info->interlaced == (u32)V4L2_DV_INTERLACED) {
		vioc = &vin_path[VIN_COMP_VIQE];
		if (!IS_ERR_OR_NULL(vioc->np)) {
			(void)VIOC_CONFIG_Device_PlugState(vioc->index,
							   &plugstate);
			if ((plugstate.enable == 1U) &&
			    (plugstate.connect_statue == VIOC_PATH_CONNECTED)) {
				(void)VIOC_CONFIG_PlugOut(vioc->index);
			}
		} else {
			vioc = &vin_path[VIN_COMP_SDEINTL];
			if (!IS_ERR_OR_NULL(vioc->np)) {
				(void)VIOC_CONFIG_Device_PlugState(vioc->index,
								   &plugstate);
				if ((plugstate.enable == 1U) &&
				    (plugstate.connect_statue ==
				     VIOC_PATH_CONNECTED)) {
					(void)VIOC_CONFIG_PlugOut(vioc->index);
				}
			} else {
				/* no available de-interlacer */
				loge(p_dev,
				     "There is no available deinterlacer\n");
			}
		}
	}

	VIOC_VIN_SetEnable(VIOC_VIN_GetAddress(vin_path[VIN_COMP_VIN].index),
			   OFF);

#if defined(CONFIG_OVERLAY_PGL)
	/* disable pgl */
	if (vstream->cif.use_pgl == 1U) {
		/* disable rdma */
		VIOC_RDMA_SetImageDisable(pgl);
	}
#endif /* defined(CONFIG_OVERLAY_PGL) */

	return 0;
}

static int tccvin_request_irq(struct tccvin_stream *vstream)
{
	const struct device *p_dev = NULL;
	struct vioc_comp *vin_path = NULL;
	struct vioc_intr *intr = NULL;
	struct vioc_intr_type *intr_src = NULL;
	u32 intr_base_id = 0;
	u32 intr_src_id = 0;
	u32 vioc_base_id = 0;
	u32 vioc_vin_id = 0;
	u32 vioc_wdma_id = 0;
	int ret_call = 0;
	int ret = 0;

	p_dev = stream_to_device(vstream);

	/* vin interrupt */
	vin_path = &vstream->cif.vin_path[VIN_COMP_VIN];
	intr = &vin_path->intr;
	intr_src = &intr->source;
	if (intr->reg == 0U) {
		intr_src->id = 0;
		intr_src->bits = 0;
		vioc_base_id = VIOC_VIN00;
		intr_base_id = VIOC_INTR_VIN0;

		if (vin_path->index >= VIOC_VIN40) {
			vioc_base_id = VIOC_VIN40;
			intr_base_id = VIOC_INTR_VIN4;
		}

		/* convert to raw index */
		vioc_base_id = get_vioc_index(vioc_base_id) / 2U;
		vioc_vin_id = get_vioc_index(vin_path->index) / 2U;

		intr_src_id =
			clamp_t(u32,
				intr_base_id + (vioc_vin_id - vioc_base_id), 0,
				UINT_MAX);
		intr_src->id = clamp_t(s32, intr_src_id, 0, INT_MAX);
		intr_src->bits = VIN_INT_VS_MASK;
		logd(p_dev, "vin - vioc_base_id: %u, vioc_vin_id: %u\n",
		     vioc_base_id, vioc_vin_id);
		logd(p_dev, "vin irq num: %u, src.id: %u, src.bits: 0x%08x\n",
		     intr->num, intr_src->id, intr_src->bits);

		(void)vioc_intr_disable(intr->num, intr_src->id, 0xFFFFFFFFU);

		(void)vioc_intr_clear(intr_src->id, 0xFFFFFFFFU);

		ret_call = request_irq(clamp_t(u32, intr->num, 0, 255),
				       tccvin_vin_isr, IRQF_SHARED,
				       vstream->tdev->vdev.name, vstream);
		if (ret_call < 0) {
			loge(p_dev, "vin - request_irq, ret: %d\n", ret_call);
			ret = -1;
		}

		(void)vioc_intr_enable(intr->num, intr_src->id, intr_src->bits);
		intr->reg = 1;
	} else {
		loge(p_dev, "The irq(%d) is already registered.\n", intr->num);
		ret = -1;
	}

	/* wdma interrupt */
	vin_path = &vstream->cif.vin_path[VIN_COMP_WDMA];
	intr = &vin_path->intr;
	intr_src = &intr->source;
	if (intr->reg == 0U) {
		intr_src->id = 0;
		intr_src->bits = 0;

		vioc_base_id = VIOC_WDMA00;
		intr_base_id = VIOC_INTR_WD0;

		if (vin_path->index >= VIOC_WDMA09) {
			vioc_base_id = VIOC_WDMA09;
			intr_base_id = VIOC_INTR_WD9;
		}

		/* convert to raw index */
		vioc_base_id = get_vioc_index(vioc_base_id);
		vioc_wdma_id = get_vioc_index(vin_path->index);

		intr_src_id =
			clamp_t(u32,
				intr_base_id + (vioc_wdma_id - vioc_base_id), 0,
				UINT_MAX);
		intr_src->id = clamp_t(s32, intr_src_id, 0, INT_MAX);
		intr_src->bits = VIOC_WDMA_IREQ_EOFR_MASK;

		logd(p_dev, "wdma - vioc_base_id: %u, vioc_wdma_id: %u\n",
		     vioc_base_id, vioc_wdma_id);
		logd(p_dev, "wdma irq num: %u, src.id: %d, src.bits: 0x%08x\n",
		     intr->num, intr_src->id, intr_src->bits);

		(void)vioc_intr_disable(intr->num, intr_src->id,
					VIOC_WDMA_IREQ_ALL_MASK);

		(void)vioc_intr_clear(intr_src->id, VIOC_WDMA_IREQ_ALL_MASK);

		ret_call = request_irq(clamp_t(s32, intr->num, 0, INT_MAX),
				       tccvin_wdma_isr, IRQF_SHARED,
				       vstream->tdev->vdev.name, vstream);
		if (ret_call < 0) {
			loge(p_dev, "wdma - request_irq, ret: %d\n", ret_call);
			ret = -1;
		}

		(void)vioc_intr_enable(intr->num, intr_src->id, intr_src->bits);
		intr->reg = 1;
	} else {
		loge(p_dev, "The irq(%d) is already registered.\n", intr->num);
		ret = -1;
	}

	return ret;
}

static int tccvin_free_irq(struct tccvin_stream *vstream)
{
	const struct device *p_dev = NULL;
	struct vioc_comp *vin_path = NULL;
	struct vioc_intr *intr = NULL;
	const struct vioc_intr_type *intr_src = NULL;
	int idxVioc = 0;
	int ret = 0;

	p_dev = stream_to_device(vstream);

	for (idxVioc = (int)VIN_COMP_MAX - 1; idxVioc >= 0; idxVioc--) {
		vin_path = &vstream->cif.vin_path[idxVioc];
		intr = &vin_path->intr;
		intr_src = &intr->source;

		if (intr->reg == 1U) {
			(void)vioc_intr_clear(intr_src->id, intr_src->bits);

			(void)vioc_intr_disable(intr->num, intr_src->id,
						intr_src->bits);
#if defined(CONFIG_SMP)
			(void)irq_set_affinity_hint(intr->num, NULL);
#endif
			(void)free_irq(clamp_t(u32, intr->num, 0, INT_MAX),
				       vstream);
			intr->reg = 0;
		}
	}

	return ret;
}

int tccvin_video_streamon(struct tccvin_stream *vstream)
{
	const struct device *p_dev = NULL;
	u32 flags = 0;
	int ret_call = 0;
	int ret = 0;

	p_dev = stream_to_device(vstream);
	flags = vstream->handover_flags;

	/* print handover flags */
	tccvin_print_handover_flags(vstream, flags);

	/* skip to handle video source */
	if (!tccvin_handover_flagged(flags, V4L2_CAP_CTRL_SKIP_SUBDEV)) {
		/* start v4l2-subdev */
		(void)tccvin_start_subdevs(vstream);
	}

	ret_call = tccvin_enable_clock(vstream);
	if (ret_call < 0) {
		loge(p_dev, "tccvin_enable_clock, ret: %d\n", ret_call);
		ret = -1;
	}

	/* skip to handle video-capture */
	if (!tccvin_handover_flagged(flags, V4L2_CAP_CTRL_SKIP_DEV)) {
		/* reset vioc path */
		(void)tccvin_reset_vioc_path(vstream);
	}

	/* IMPORTANT: VIOC Interrupt MUST BE Requested after VIOC RESET Sequence */
	ret_call = tccvin_request_irq(vstream);
	if (ret_call < 0) {
		loge(p_dev, "tccvin_request_irq, ret: %d\n", ret_call);
		ret = -1;
	}

	/* skip to handle video-capture */
	if (!tccvin_handover_flagged(flags, V4L2_CAP_CTRL_SKIP_DEV)) {
		ret_call = tccvin_start_stream(vstream);
		if (ret_call < 0) {
			loge(p_dev, "tccvin_start_stream, ret: %d\n", ret_call);
			ret = -1;
		}
	}

	return ret;
}

int tccvin_video_streamoff(struct tccvin_stream *vstream)
{
	const struct device *p_dev = NULL;
	u32 flags = 0;
	int ret_call = 0;
	int ret = 0;

	p_dev = stream_to_device(vstream);
	flags = vstream->handover_flags;

	/* print handover flags */
	tccvin_print_handover_flags(vstream, flags);

	/* skip to handle video-capture */
	if (!tccvin_handover_flagged(flags, V4L2_CAP_CTRL_SKIP_DEV)) {
		ret_call = tccvin_stop_stream(vstream);
		if (ret_call < 0) {
			loge(p_dev, "tccvin_stop_stream, ret: %d\n", ret_call);
			ret = -1;
		}
	}

	ret_call = tccvin_free_irq(vstream);
	if (ret_call < 0) {
		loge(p_dev, "tccvin_free_irq, ret: %d\n", ret_call);
		ret = -1;
	}

	/* skip to handle video-capture */
	if (!tccvin_handover_flagged(flags, V4L2_CAP_CTRL_SKIP_DEV)) {
		/* reset vioc path */
		(void)tccvin_reset_vioc_path(vstream);
	}

	tccvin_disable_clock(vstream);

	/* skip to handle video source */
	if (!tccvin_handover_flagged(flags, V4L2_CAP_CTRL_SKIP_SUBDEV)) {
		/* stop v4l2-subdev */
		(void)tccvin_stop_subdevs(vstream);
	}

	return ret;
}

void tccvin_video_check_path_status(const struct tccvin_stream *vstream,
				    u32 *status)
{
	const struct device *p_dev = NULL;
	const struct vioc_comp *vin_path = NULL;
	const void __iomem *pWDMA = NULL;
	u32 prev_addr = 0;
	u32 curr_addr = 0;
	u32 nCheck = 0;
	u32 idxCheck = 0;
	u32 delay = 20;

	p_dev = stream_to_device(vstream);
	vin_path = vstream->cif.vin_path;
	pWDMA = VIOC_WDMA_GetAddress(vin_path[VIN_COMP_WDMA].index);
	curr_addr = VIOC_WDMA_Get_CAddress(pWDMA);
	msleep(delay);

	nCheck = 4;
	for (idxCheck = 0; idxCheck < nCheck; idxCheck++) {
		prev_addr = curr_addr;
		msleep(delay);
		curr_addr = VIOC_WDMA_Get_CAddress(pWDMA);

		if (prev_addr != curr_addr) {
			/* path status is okay */
			*status = V4L2_CAP_PATH_WORKING;
		} else {
			*status = V4L2_CAP_PATH_NOT_WORKING;
			logd(p_dev,
			     "[%d] prev_addr: 0x%08x, curr_addr: 0x%08x\n",
			     idxCheck, prev_addr, curr_addr);
		}
	}
}

int tccvin_video_create_lastframe(const struct tccvin_stream *vstream,
				  u32 *addrs)
{
	const struct device *p_dev = NULL;
	void __iomem *wdma = NULL;
	u32 addrsLframe[3] = { 0, 0, 0 };
	u32 addrsPrev[3] = { 0, 0, 0 };
	int ret = 0;

	p_dev = stream_to_device(vstream);
	wdma = VIOC_WDMA_GetAddress(vstream->cif.vin_path[VIN_COMP_WDMA].index);

	(void)memset(addrsLframe, 0, sizeof(addrsLframe));
	(void)memset(addrsPrev, 0, sizeof(addrsPrev));

	if ((vstream->cif.rsvd_mem[RESERVED_MEM_LFRAME] == (struct reserved_mem *)NULL) ||
	    (vstream->cif.rsvd_mem[RESERVED_MEM_PREV] == (struct reserved_mem *)NULL)) {
		logw(p_dev, "lastframe memory is not reserved for this device.");
		ret = -ENOMEM;
	} else {
#if defined(CONFIG_ARM64)
		addrsLframe[0] =
			clamp_t(u32, vstream->cif.rsvd_mem[RESERVED_MEM_LFRAME]->base,
				0, UINT_MAX);
		addrsPrev[0] =
			clamp_t(u32, vstream->cif.rsvd_mem[RESERVED_MEM_PREV]->base, 0,
				UINT_MAX);
#else
		addrsLframe[0] = vstream->cif.rsvd_mem[RESERVED_MEM_LFRAME]->base;
		addrsPrev[0] = vstream->cif.rsvd_mem[RESERVED_MEM_PREV]->base;
#endif //defined(CONFIG_ARM64)

		logd(p_dev, "addrs of lastframe: 0x%08x, 0x%08x, 0x%08x\n",
		addrsLframe[0], addrsLframe[1], addrsLframe[2]);
		logd(p_dev, "addfs of  preview: 0x%08x, 0x%08x, 0x%08x\n", addrsPrev[0],
		addrsPrev[1], addrsPrev[2]);

		if (addrsLframe[0] == 0U) {
			loge(p_dev, "addrs of lastframe is wrong\n");
			ret = -1;
		} else {
			tccvin_switch_lastframe(wdma, addrsLframe, addrsPrev);

			/* get address of lastframe */
			*addrs = addrsLframe[0];
		}
	}

	return ret;
}

int tccvin_video_trigger_recovery(const struct tccvin_stream *vstream,
				  u32 index)
{
	const struct device *p_dev = NULL;
	const struct tccvin_cif *cif = NULL;
	void __iomem *wdma = NULL;
	int ret = 0;

	p_dev = stream_to_device(vstream);
	cif = &vstream->cif;
	wdma = VIOC_WDMA_GetAddress(cif->vin_path[VIN_COMP_WDMA].index);

	switch (index) {
	case (u32)TCCVIN_TEST_STOP_SUBDEV:
		break;
	case (u32)TCCVIN_TEST_STOP_WDMA:
		/* force to disable wdma */
		VIOC_WDMA_SetImageSize(wdma, 0, 0);
		VIOC_WDMA_SetImageEnable(wdma, OFF);
		break;
	default:
		loge(p_dev, "recovery case (%u) is not supported\n", index);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*
 * tccvin_set_vin
 *
 * - DESCRIPTION:
 *	Set vin component to receive video data via cif port
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */

int tccvin_set_vin(const struct tccvin_stream *vstream)
{
	const struct device *p_dev = NULL;
	void __iomem *vin = NULL;
	const struct tccvin_vs_info *vs_info = NULL;
	const struct v4l2_rect *crop_rect = NULL;
	u32 data_order = 0;
	u32 data_format = 0;
	u32 gen_field_en = 0;
	u32 de_low = 0;
	u32 field_low = 0;
	u32 vs_low = 0;
	u32 hs_low = 0;
	u32 pxclk_pol = 0;
	u32 vs_mask = 0;
	u32 hsde_connect_en = 0;
	u32 intpl_en = 0;
	u32 conv_en = 0;
	u32 interlaced = 0;
	u32 width = 0;
	u32 height = 0;
	u32 offset_x = 0;
	u32 offset_y = 0;
	u32 offset_y_intl = 0;
	u32 crop_width = 0;
	u32 crop_height = 0;
	u32 crop_offset_x = 0;
	u32 crop_offset_y = 0;
	u32 stream_enable = 0;
	u32 flush_vsync = 0;

	p_dev = stream_to_device(vstream);
	vin = VIOC_VIN_GetAddress(vstream->cif.vin_path[VIN_COMP_VIN].index);

	vs_info = &vstream->vs_info;
	crop_rect = &vstream->rect_crop;

	data_order = vs_info->data_order;
	data_format = vs_info->data_format;
	gen_field_en = vs_info->gen_field_en;
	de_low = vs_info->de_low;
	field_low = vs_info->field_low;
	vs_low = vs_info->vs_low;
	hs_low = vs_info->hs_low;
	pxclk_pol = vs_info->pclk_polarity;
	vs_mask = vs_info->vs_mask;
	hsde_connect_en = vs_info->hsde_connect_en;
	intpl_en = vs_info->intpl_en;
	interlaced = vs_info->interlaced;
	conv_en = vs_info->conv_en;

	width = vs_info->width;
	height = ror32(vs_info->height, interlaced);

	if ((crop_rect->width == 0U) && (crop_rect->height == 0U)) {
		crop_width = width;
		crop_height = height;
		crop_offset_x = 0;
		crop_offset_y = 0;
	} else {
		crop_width = crop_rect->width;
		crop_height = ror32(crop_rect->height, interlaced);
		crop_offset_x = clamp_t(u32, crop_rect->left, 0, INT_MAX);
		crop_offset_y = ror32(clamp_t(u32, crop_rect->top, 0, INT_MAX),
				      interlaced);
	}

	stream_enable = vs_info->stream_enable;
	flush_vsync = vs_info->flush_vsync;

	logd(p_dev,
	     "VIN: 0x%08x, Size - (%u, %u) %u * %u -> (%u, %u) %u * %u\n",
	     (u32)(uintptr_t)vin, offset_x, offset_y, width, height,
	     crop_offset_x, crop_offset_y, crop_width, crop_height);

	logd(p_dev, "%20s: %u\n", "data_order", data_order);
	logd(p_dev, "%20s: %u\n", "data_format", data_format);
	logd(p_dev, "%20s: %u\n", "gen_field_en", gen_field_en);
	logd(p_dev, "%20s: %u\n", "de_low", de_low);
	logd(p_dev, "%20s: %u\n", "vs_mask", vs_mask);
	logd(p_dev, "%20s: %u\n", "hsde_connect_en", hsde_connect_en);
	logd(p_dev, "%20s: %u\n", "intpl_en", intpl_en);
	logd(p_dev, "%20s: %u\n", "interlaced", interlaced);
	logd(p_dev, "%20s: %u\n", "conv_en", conv_en);
	logd(p_dev, "%20s: %u\n", "stream_enable", stream_enable);
	logd(p_dev, "%20s: %u\n", "flush_vsync", flush_vsync);

	VIOC_VIN_SetSyncPolarity(vin, hs_low, vs_low, field_low, de_low,
				 gen_field_en, pxclk_pol);
	VIOC_VIN_SetCtrl(vin, conv_en, hsde_connect_en, vs_mask, data_format,
			 data_order);
	VIOC_VIN_SetInterlaceMode(vin, interlaced, intpl_en);
	VIOC_VIN_SetImageSize(vin, width, height);
	VIOC_VIN_SetImageOffset(vin, offset_x, offset_y, offset_y_intl);
	VIOC_VIN_SetImageCropSize(vin, crop_width, crop_height);
	VIOC_VIN_SetImageCropOffset(vin, crop_offset_x, crop_offset_y);
	VIOC_VIN_SetY2RMode(vin, 2);

	VIOC_VIN_SetSEEnable(vin, stream_enable);
	VIOC_VIN_SetFlushBufferEnable(vin, flush_vsync);

	logd(p_dev, "v4l2 preview format(pixelformat): 0x%08x\n",
	     vstream->format.fmt.pix_mp.pixelformat);
	if (((vstream->tdev->tsubdev.fmt.format.code & 0x3000U) == 0x2000U) &&
	    ((vstream->format.fmt.pix_mp.pixelformat ==
	      (u32)V4L2_PIX_FMT_RGB24) ||
	     (vstream->format.fmt.pix_mp.pixelformat ==
	      (u32)V4L2_PIX_FMT_RGB32))) {
		/*
		 * code
		 * 0x1xxx: RGB, 0x2xxx: YUV, 0x3xxx: Bayer
		 */
		if (!((interlaced == 1U) &&
		      !IS_ERR_OR_NULL(
			      vstream->cif.vin_path[VIN_COMP_VIQE].np))) {
			logd(p_dev, "y2r is ENABLED\n");
			VIOC_VIN_SetY2REnable(vin, ON);
		}
	} else {
		logd(p_dev, "y2r is DISABLED\n");
		VIOC_VIN_SetY2REnable(vin, OFF);
	}

	/* set vin lut */
	if ((vstream->cif.vin_internal_lut.lut0_en == 1U) ||
	    (vstream->cif.vin_internal_lut.lut1_en == 1U) ||
	    (vstream->cif.vin_internal_lut.lut2_en == 1U)) {
		VIOC_VIN_SetLUT(vin, vstream->cif.vin_internal_lut.lut);
		VIOC_VIN_SetLUTEnable(vin,
				      vstream->cif.vin_internal_lut.lut0_en,
				      vstream->cif.vin_internal_lut.lut1_en,
				      vstream->cif.vin_internal_lut.lut2_en);
	}
	VIOC_VIN_SetEnable(vin, ON);

	return 0;
}

/*
 * tccvin_set_deinterlacer
 *
 * - DESCRIPTION:
 *	Set viqe component to deinterlace interlaced video data
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */

int tccvin_set_deinterlacer(const struct tccvin_stream *vstream)
{
	const struct device *p_dev = NULL;
	const struct vioc_comp *vin_path = NULL;
	void __iomem *viqe = NULL;
	const struct tccvin_vs_info *vs_info = NULL;
	u32 interlaced = 0;
	u32 width = 0;
	u32 height = 0;
	u32 viqe_width = 0;
	u32 viqe_height = 0;
	u32 format = 0;
	VIOC_VIQE_DEINTL_MODE bypass_deintl;
	u32 v_offset = 0;
	u32 deintl_base0 = 0;
	u32 deintl_base1 = 0;
	u32 deintl_base2 = 0;
	u32 deintl_base3 = 0;
	u32 cdf_lut_en = 0;
	u32 his_en = 0;
	u32 gamut_en = 0;
	u32 d3d_en = 0;
	u32 deintl_en = 0;
	u32 vioc_format = 0;
	u32 pixelformat = 0;
	int ret = 0;

	bypass_deintl = (VIOC_VIQE_DEINTL_MODE)0;
	p_dev = stream_to_device(vstream);
	vin_path = vstream->cif.vin_path;

	if (!IS_ERR_OR_NULL(vin_path[VIN_COMP_VIQE].np)) {
		viqe = VIOC_VIQE_GetAddress(vin_path[VIN_COMP_VIQE].index);
		vs_info = &vstream->vs_info;

		interlaced = vs_info->interlaced;
		width = (vstream->rect_crop.width != 0U) ?
				vstream->rect_crop.width :
				vs_info->width;
		height = ror32(((vstream->rect_crop.height != 0U) ?
					vstream->rect_crop.height :
					vs_info->height),
			       interlaced);

		viqe_width = 0;
		viqe_height = 0;
		format = (u32)VIOC_VIQE_FMT_YUV422;
		bypass_deintl = VIOC_VIQE_DEINTL_MODE_3D;
		v_offset = clamp_t(u32, width * height * 4U * 3U / 2U, 0,
				   UINT_MAX);
		deintl_base0 =
			clamp_t(u32,
				vstream->cif.rsvd_mem[RESERVED_MEM_VIQE]->base,
				0L, UINT_MAX);
		deintl_base1 = clamp(deintl_base0 + v_offset, 0U, UINT_MAX);
		deintl_base2 = clamp(deintl_base1 + v_offset, 0U, UINT_MAX);
		deintl_base3 = clamp(deintl_base2 + v_offset, 0U, UINT_MAX);

		cdf_lut_en = OFF;
		his_en = OFF;
		gamut_en = OFF;
		d3d_en = OFF;
		deintl_en = ON;

		vioc_format = vstream->vs_info.data_format;
		pixelformat = vstream->format.fmt.pix_mp.pixelformat;

		logd(p_dev,
		     "VIQE: 0x%08x, Source Size - width: %d, height: %d\n",
		     (u32)(uintptr_t)viqe, width, height);

		ret = VIOC_CONFIG_PlugIn(vin_path[VIN_COMP_VIQE].index,
					 vin_path[VIN_COMP_VIN].index);

		if (((vioc_format == (u32)FMT_YUV422_16BIT) ||
		     (vioc_format == (u32)FMT_YUV422_8BIT) ||
		     (vioc_format == (u32)FMT_YUVK4444_16BIT) ||
		     (vioc_format == (u32)FMT_YUVK4224_24BIT)) &&
		    ((pixelformat == V4L2_PIX_FMT_RGB24) ||
		     (pixelformat == V4L2_PIX_FMT_RGB32))) {
			VIOC_VIQE_SetImageY2RMode(viqe, 2);
			VIOC_VIQE_SetImageY2REnable(viqe, ON);
		}
		VIOC_VIQE_SetControlRegister(viqe, viqe_width, viqe_height,
					     format);
		VIOC_VIQE_SetDeintlRegister(viqe, format, OFF, viqe_width,
					    viqe_height, bypass_deintl,
					    deintl_base0, deintl_base1,
					    deintl_base2, deintl_base3);
		VIOC_VIQE_SetControlEnable(viqe, cdf_lut_en, his_en, gamut_en,
					   d3d_en, deintl_en);
		VIOC_VIQE_SetDeintlModeWeave(viqe);
		VIOC_VIQE_IgnoreDecError(viqe, ON, ON, ON);
	} else if (!IS_ERR_OR_NULL(vin_path[VIN_COMP_SDEINTL].np)) {
		/* can be pluged in */
		ret = VIOC_CONFIG_PlugIn(vin_path[VIN_COMP_SDEINTL].index,
					 vin_path[VIN_COMP_VIN].index);
	} else {
		logi(p_dev, "There is no available deinterlacer\n");
		ret = -1;
	}

	return ret;
}

/*
 * tccvin_set_scaler
 *
 * - DESCRIPTION:
 *	Set sclaer component to scale up or down
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */

int tccvin_set_scaler(const struct tccvin_stream *vstream)
{
	const struct device *p_dev = NULL;
	const struct tccvin_cif *cif = NULL;
	void __iomem *sc = NULL;
	const struct v4l2_pix_format_mplane *pix_mp = NULL;
	u32 width = 0;
	u32 height = 0;
	int ret = 0;

	p_dev = stream_to_device(vstream);
	cif = &vstream->cif;
	sc = VIOC_SC_GetAddress(cif->vin_path[VIN_COMP_SCALER].index);

	pix_mp = &vstream->format.fmt.pix_mp;

	if ((vstream->rect_compose.width == 0U) &&
	    (vstream->rect_compose.height == 0U)) {
		width = pix_mp->width;
		height = pix_mp->height;
	} else {
		width = vstream->rect_compose.width;
		height = vstream->rect_compose.height;
	}

	logd(p_dev, "SCaler: 0x%08x, Size - width: %d, height: %d\n",
	     (u32)(uintptr_t)sc, width, height);

	/* Plug the scaler in */
	ret = VIOC_CONFIG_PlugIn(cif->vin_path[VIN_COMP_SCALER].index,
				 cif->vin_path[VIN_COMP_VIN].index);

	/* Configure the scaler */
	VIOC_SC_SetBypass(sc, OFF);
	VIOC_SC_SetDstSize(sc, width, height);
	VIOC_SC_SetOutPosition(sc, 0U, 0U);
	/* workaround: scaler margin */
	VIOC_SC_SetOutSize(sc, width, clamp(height + 1, 0U, UINT_MAX));
	VIOC_SC_SetUpdate(sc);

	return 0;
}

/*
 * tccvin_set_wmixer
 *
 * - DESCRIPTION:
 *	Set video-input wmixer component to mix two or more video data
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */

int tccvin_set_wmixer(const struct tccvin_stream *vstream)
{
	void __iomem *wmixer = NULL;
	const struct v4l2_pix_format_mplane *pix_mp = NULL;
	u32 width = 0;
	u32 height = 0;
	u32 out_posx = 0;
	u32 out_posy = 0;
	u32 ovp = 5;
	u32 vs_ch = 0;
	const struct device *p_dev = NULL;
#if defined(CONFIG_OVERLAY_PGL)
	u32 pgl_ch = 1;
	u32 chrom_layer = 0;
	u32 chroma_r = PGL_BG_R;
	u32 chroma_g = PGL_BG_G;
	u32 chroma_b = PGL_BG_B;
	u32 masked_chroma_r = PGL_BGM_R;
	u32 masked_chroma_g = PGL_BGM_G;
	u32 masked_chroma_b = PGL_BGM_B;
#endif /* defined(CONFIG_OVERLAY_PGL) */
	int ret = 0;

	p_dev = stream_to_device(vstream);
	wmixer = VIOC_WMIX_GetAddress(
		vstream->cif.vin_path[VIN_COMP_WMIX].index);

	pix_mp = &vstream->format.fmt.pix_mp;

	width = pix_mp->width;
	height = pix_mp->height;

	if ((vstream->rect_compose.left != 0) &&
	    (vstream->rect_compose.top != 0)) {
		out_posx = (u32)vstream->rect_compose.left;
		out_posy = clamp_t(u32, vstream->rect_compose.top, 0, INT_MAX);
	}

	if (vstream->cif.wmix_bypass == BYPASS_MODE) {
		ret = VIOC_CONFIG_WMIXPath(
			vstream->cif.vin_path[VIN_COMP_VIN].index, OFF);
	} else {
		logd(p_dev, "WMIXer: 0x%08x, Size - width: %d, height: %d\n",
		     (u32)(uintptr_t)(wmixer), width, height);
		logd(p_dev, " . CH0: (%d, %d)\n", out_posx, out_posy);

		/* Configure the wmixer */
		VIOC_WMIX_SetSize(wmixer, width, height);
		VIOC_WMIX_SetOverlayPriority(wmixer, ovp);
		VIOC_WMIX_SetPosition(wmixer, vs_ch, out_posx, out_posy);
#if defined(CONFIG_OVERLAY_PGL)
		VIOC_WMIX_SetPosition(wmixer, pgl_ch, 0, 0);
		VIOC_WMIX_SetChromaKey(wmixer, chrom_layer, ON, chroma_r,
				       chroma_g, chroma_b, masked_chroma_r,
				       masked_chroma_g, masked_chroma_b);
#endif /*defined(CONFIG_OVERLAY_PGL)*/
		VIOC_WMIX_SetUpdate(wmixer);
		ret = VIOC_CONFIG_WMIXPath(
			vstream->cif.vin_path[VIN_COMP_VIN].index, ON);
	}

	return ret;
}

/*
 * tccvin_set_wdma
 *
 * - DESCRIPTION:
 *	Set wdma component to write video data
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */

int tccvin_set_wdma(struct tccvin_stream *vstream)
{
	const struct device *p_dev = NULL;
	struct tccvin_queue *p_queue = NULL;
	const struct tccvin_cif *cif = NULL;
	struct tccvin_buffer *p_buf = NULL;
	unsigned long flags = 0;
	void __iomem *wdma = NULL;
	const struct v4l2_pix_format_mplane *pix_mp = NULL;
	u32 width = 0;
	u32 height = 0;
	const struct tccvin_format *pformat = NULL;
	u32 format = 0;
	struct vb2_buffer *vb = NULL;
	u32 dma_addrs[MAX_PLANES];

	p_dev = stream_to_device(vstream);
	p_queue = &vstream->queue;
	cif = &vstream->cif;
	wdma = VIOC_WDMA_GetAddress(vstream->cif.vin_path[VIN_COMP_WDMA].index);

	pix_mp = &vstream->format.fmt.pix_mp;

	width = pix_mp->width;
	height = pix_mp->height;
	pformat = tccvin_get_tccvin_format_by_pixelformat(pix_mp->pixelformat);
	if (pformat == NULL) {
		loge(p_dev, "Failed to set WDMA: unsupported format\n");

		return -EINVAL;
	}

	format = pformat->guid;

	logd(p_dev, "WDMA: 0x%08x, size[%d x %d], format[%d]\n",
	     (u32)(uintptr_t)(wdma), width, height, format);

	VIOC_WDMA_SetImageFormat(wdma, format);
	VIOC_WDMA_SetImageSize(wdma, width, height);
	VIOC_WDMA_SetImageOffset(wdma, format, width);

	spin_lock_irqsave(&p_queue->slock, flags);

	if (!list_empty(&p_queue->buf_list)) {
		p_buf = list_first_entry(&p_queue->buf_list,
					 struct tccvin_buffer, entry);
	}
	spin_unlock_irqrestore(&p_queue->slock, flags);

	if (p_buf != NULL) {
		vb = &p_buf->buf.vb2_buf;
		(void)memset(dma_addrs, 0, sizeof(dma_addrs));
		tccvin_get_dma_addrs(vstream, vb, dma_addrs);
		tccvin_print_dma_addrs(vstream, vb, dma_addrs);

		VIOC_WDMA_SetImageBase(wdma, dma_addrs[0], dma_addrs[1],
				       dma_addrs[2]);
		VIOC_WDMA_SetImageEnable(wdma, OFF);

	} else {
		loge(p_dev, "Buffer is NOT initialized\n");
	}

	return 0;
}

void tccvin_switch_lastframe(void __iomem *wdma, u32 *addrsLframe,
			     u32 *addrsPrev)
{
	/* switch to write to lastframe buffer */
	VIOC_WDMA_SetImageBase(wdma, addrsLframe[0], addrsLframe[1],
			       addrsLframe[2]);
	VIOC_WDMA_SetImageUpdate(wdma);

	/* wait until writing is done */
	msleep(60);

	/* switch to write to preview buffer */
	VIOC_WDMA_SetImageBase(wdma, addrsPrev[0], addrsPrev[1], addrsPrev[2]);
	VIOC_WDMA_SetImageUpdate(wdma);
}
