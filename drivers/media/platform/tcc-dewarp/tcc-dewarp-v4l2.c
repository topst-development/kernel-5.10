// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <video/tcc-svdw.h>
#include <linux/version.h>
#include <linux/compat.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/dma-mapping.h>
#include <linux/kconfig.h>

#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>

#include "tcc-dewarp-video.h"

static inline struct tcc_dewarp_stream *
queue_to_stream(struct tcc_dewarp_queue *queue)
{
	return container_of(queue, struct tcc_dewarp_stream, queue);
}

static inline struct tcc_dewarp_buffer *
vb2_v4l2_buf_to_buf(struct vb2_v4l2_buffer *buf)
{
	return container_of(buf, struct tcc_dewarp_buffer, buf);
}

/*
 * Return all queued buffers to videobuf2 in the requested state.
 *
 * This function must be called with the queue spinlock held.
 */

static void
tcc_dewarp_queue_return_buffers(struct tcc_dewarp_queue *queue,
				enum tcc_dewarp_buffer_state buf_state)
{
	enum vb2_buffer_state vb2_state;

	vb2_state = (buf_state == TCC_DEWARP_BUF_STATE_ERROR) ?
			    VB2_BUF_STATE_ERROR :
			    VB2_BUF_STATE_QUEUED;

	while (list_empty(&queue->buf_list) != 1) {
		struct tcc_dewarp_buffer *buf = NULL;

		buf = list_first_entry(&queue->buf_list,
				       struct tcc_dewarp_buffer, entry);
		list_del(&buf->entry);
		vb2_buffer_done(&buf->buf.vb2_buf, vb2_state);
	}
}

/* -----------------------------------------------------------------------------
 * videobuf2 queue operations
 */

static int tcc_dewarp_queue_setup(struct vb2_queue *vq, u32 *nbuffers,
				  u32 *nplanes, u32 sizes[],
				  struct device *alloc_devs[])
{
	struct tcc_dewarp_queue *queue = NULL;
	const struct tcc_dewarp_stream *vstream = NULL;
	const struct device *p_dev = NULL;
	struct v4l2_pix_format_mplane pix_mp = {
		0,
	};
	u32 idxPlane = 0;
	int ret = 0;

	queue = (struct tcc_dewarp_queue *)vb2_get_drv_priv(vq);
	vstream = queue_to_stream(queue);
	p_dev = stream_to_device(vstream);

	/* get current pixelformat, width and height */
	(void)memcpy(&pix_mp, &vstream->format.fmt.pix_mp, sizeof(pix_mp));

	/* fill other fileds */
	ret = v4l2_fill_pixfmt_mp(&pix_mp, pix_mp.pixelformat, pix_mp.width,
				  pix_mp.height);
	if (ret < 0) {
		loge(p_dev, "Failed on v4l2_fill_pixfmt_mp\n");
	} else {
		/* update num_planes */
		if (*nplanes == 0U) {
			/* The initial value of nplanes is 0,
			 * so set it as the initial format's num_planes.
			 */
			*nplanes = pix_mp.num_planes;
			logd(p_dev, "num_planes: %u\n", *nplanes);
		}
#if defined(CONFIG_ARCH_TCC750X)
		// support IR plane
		if (vstream->dewarp_wrap.odw_params.in.ir_enable == 1U) {
			if ((pix_mp.pixelformat != V4L2_PIX_FMT_RGB32) &&
			    (pix_mp.pixelformat != V4L2_PIX_FMT_BGR32) &&
			    (pix_mp.pixelformat != V4L2_PIX_FMT_ARGB32)) {
				*nplanes = (unsigned int)pix_mp.num_planes + 1U;
			}
		}
#endif //CONFIG_ARCH_TCC750X
		/* update sizeimage */
		for (idxPlane = 0; idxPlane < *nplanes; idxPlane++) {
			sizes[idxPlane] = pix_mp.plane_fmt[idxPlane].sizeimage;
			logd(p_dev, "plane[%u].sizeimage: %u\n", idxPlane,
			     sizes[idxPlane]);
		}
	}

	return ret;
}

static int tcc_dewarp_buf_prepare(struct vb2_buffer *vb)
{
	struct tcc_dewarp_queue *p_queue = NULL;
	struct tcc_dewarp_stream *vstream = NULL;
	struct vb2_v4l2_buffer *vbuf = NULL;
	struct v4l2_pix_format_mplane *format = NULL;
	int ret = 0;
	unsigned int i;

	p_queue = (struct tcc_dewarp_queue *)vb2_get_drv_priv(vb->vb2_queue);
	vstream = queue_to_stream(p_queue);
	vbuf = to_vb2_v4l2_buffer(vb);
	format = &vstream->format.fmt.pix_mp;

	for (i = 0; i < format->num_planes; i++) {
		if (format->plane_fmt[i].sizeimage <= vb2_plane_size(vb, i)) {
			vb2_set_plane_payload(vb, i, format->plane_fmt[i].sizeimage);
		} else {
			ret = -EINVAL;
			break;
		}
	}

	return ret;
}

static void tcc_dewarp_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = NULL;
	struct tcc_dewarp_queue *p_queue = NULL;
	struct tcc_dewarp_buffer *buf = NULL;
	unsigned long flags = 0;

	vbuf = to_vb2_v4l2_buffer(vb);

	p_queue = (struct tcc_dewarp_queue *)vb2_get_drv_priv(vb->vb2_queue);
	buf = vb2_v4l2_buf_to_buf(vbuf);

	spin_lock_irqsave(&p_queue->slock, flags);
	list_add_tail(&buf->entry, &p_queue->buf_list);
	spin_unlock_irqrestore(&p_queue->slock, flags);
}

static int tcc_dewarp_start_streaming(struct vb2_queue *vq, u32 count)
{
	struct tcc_dewarp_queue *p_queue = NULL;
	struct tcc_dewarp_stream *vstream = NULL;
	int ret = 0;

	p_queue = (struct tcc_dewarp_queue *)vb2_get_drv_priv(vq);
	vstream = queue_to_stream(p_queue);

	ret = media_pipeline_start(&vstream->tdev->vdev.entity,
				   &vstream->tdev->tccmd->pipe);
	if (ret < 0) {
		spin_lock_irq(&p_queue->slock);
		tcc_dewarp_queue_return_buffers(p_queue,
						TCC_DEWARP_BUF_STATE_QUEUED);
		spin_unlock_irq(&p_queue->slock);
	}

	if (ret >= 0) {
		ret = tcc_dewarp_video_streamon(vstream);
		if (ret != 0) {
			spin_lock_irq(&p_queue->slock);
			tcc_dewarp_queue_return_buffers(
				p_queue, TCC_DEWARP_BUF_STATE_QUEUED);
			spin_unlock_irq(&p_queue->slock);
		}
	}

	return ret;
}

static void tcc_dewarp_stop_streaming(struct vb2_queue *vq)
{
	struct tcc_dewarp_queue *p_queue = NULL;
	struct tcc_dewarp_stream *vstream = NULL;

	p_queue = (struct tcc_dewarp_queue *)vb2_get_drv_priv(vq);
	vstream = queue_to_stream(p_queue);

	(void)tcc_dewarp_video_streamoff(vstream);

	media_pipeline_stop(&vstream->tdev->vdev.entity);

	spin_lock_irq(&p_queue->slock);
	tcc_dewarp_queue_return_buffers(p_queue, TCC_DEWARP_BUF_STATE_ERROR);
	spin_unlock_irq(&p_queue->slock);
}

static const struct vb2_ops tcc_dewarp_qops = {
	.queue_setup = tcc_dewarp_queue_setup,
	.buf_prepare = tcc_dewarp_buf_prepare,
	.buf_queue = tcc_dewarp_buf_queue,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.start_streaming = tcc_dewarp_start_streaming,
	.stop_streaming = tcc_dewarp_stop_streaming,
};

int tcc_dewarp_v4l2_init_queue(struct tcc_dewarp_queue *queue)
{
	const struct tcc_dewarp_stream *vstream = NULL;
	const struct device *p_dev = NULL;
	struct vb2_queue *q = NULL;
	int ret = 0;

	vstream = queue_to_stream(queue);
	p_dev = stream_to_device(vstream);
	q = &queue->queue;

	/* init vb2_queue */
	q->type = (u32)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	q->io_modes = (u32)VB2_MMAP | (u32)VB2_DMABUF;
	q->bidirectional = 0;
	q->is_output = (unsigned char)DMA_TO_DEVICE;
	q->drv_priv = queue;
	q->buf_struct_size = (u32)sizeof(struct tcc_dewarp_buffer);
	q->ops = &tcc_dewarp_qops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->timestamp_flags = (u32)V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC |
			     (u32)V4L2_BUF_FLAG_TSTAMP_SRC_SOE;
	q->dev = &vstream->tdev->pdev->dev;
	q->lock = &vstream->tdev->pdev->dev.mutex;
	q->min_buffers_needed = 1;

	ret = vb2_queue_init(&queue->queue);
	if (ret != 0) {
		/* failure of queue init */
		loge(p_dev, "Failed to init vb2_queue: %d\n", ret);
		ret = -1;
	} else {
		spin_lock_init(&queue->slock);
		INIT_LIST_HEAD(&queue->buf_list);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(tcc_dewarp_v4l2_init_queue);

void tcc_dewarp_get_dma_addrs(const struct tcc_dewarp_stream *vstream,
			      struct vb2_buffer *vb, u32 addrs[])
{
	const struct device *p_dev = NULL;
	dma_addr_t dma_addrs[MAX_PLANES];
	u32 idxpln = 0;

	p_dev = stream_to_device(vstream);

	switch (vb->memory) {
	case (u32)VB2_MEMORY_MMAP:
		for (idxpln = 0; idxpln < vb->num_planes; idxpln++) {
			dma_addrs[idxpln] =
				vb2_dma_contig_plane_dma_addr(vb, idxpln);
#if defined(CONFIG_ARM64)
			addrs[idxpln] =
				clamp_t(u32, dma_addrs[idxpln], 0, UINT_MAX);
#else
			addrs[idxpln] = (u32)dma_addrs[idxpln];
#endif //defined(CONFIG_ARM64)
		}
		break;

	default:
		loge(p_dev, "memory(0x%08x) is not supported\n", vb->memory);
		break;
	}
}
EXPORT_SYMBOL_GPL(tcc_dewarp_get_dma_addrs);

void tcc_dewarp_print_dma_addrs(const struct tcc_dewarp_stream *vstream,
				const struct vb2_buffer *vb, const u32 addrs[])
{
	const struct device *p_dev = NULL;
	u32 idxpln = 0;

	p_dev = stream_to_device(vstream);

	(void)addrs;

	switch (vb->memory) {
	case (u32)VB2_MEMORY_MMAP:
		for (idxpln = 0; idxpln < vb->num_planes; idxpln++) {
			/* dma addr */
			logd(p_dev, "planes[%u]: 0x%08x\n", idxpln,
			     addrs[idxpln]);
		}
		break;

	default:
		loge(p_dev, "memory(0x%08x) is not supported\n", vb->memory);
		break;
	}
}
EXPORT_SYMBOL_GPL(tcc_dewarp_print_dma_addrs);

static u32 tcc_dewarp_get_mbus_code_by_pixelformat(u32 pixelformat)
{
	struct tcc_dewarp_mbus_fmt {
		u32 pixelformat;
		u32 mbus_fmt;
	};
	const struct tcc_dewarp_mbus_fmt tcc_dewarp_mbus_fmt_list[] = {
		{ .pixelformat = V4L2_PIX_FMT_RGB24,
		  .mbus_fmt = MEDIA_BUS_FMT_RGB888_1X24 },
		{ .pixelformat = V4L2_PIX_FMT_RGB32,
		  .mbus_fmt = MEDIA_BUS_FMT_ARGB8888_1X32 },
		{ .pixelformat = V4L2_PIX_FMT_UYVY,
		  .mbus_fmt = MEDIA_BUS_FMT_UYVY8_1X16 },
		{ .pixelformat = V4L2_PIX_FMT_VYUY,
		  .mbus_fmt = MEDIA_BUS_FMT_VYUY8_1X16 },
		{ .pixelformat = V4L2_PIX_FMT_YUYV,
		  .mbus_fmt = MEDIA_BUS_FMT_YUYV8_1X16 },
		{ .pixelformat = V4L2_PIX_FMT_YVYU,
		  .mbus_fmt = MEDIA_BUS_FMT_YVYU8_1X16 },
		{ .pixelformat = V4L2_PIX_FMT_YUV422P,
		  .mbus_fmt = MEDIA_BUS_FMT_YUYV8_2X8 },
		{ .pixelformat = V4L2_PIX_FMT_NV16,
		  .mbus_fmt = MEDIA_BUS_FMT_YVYU8_2X8 },
		{ .pixelformat = V4L2_PIX_FMT_NV61,
		  .mbus_fmt = MEDIA_BUS_FMT_YVYU8_2X8 },
		{ .pixelformat = V4L2_PIX_FMT_YVU420,
		  .mbus_fmt = MEDIA_BUS_FMT_YVYU8_2X8 },
		{ .pixelformat = V4L2_PIX_FMT_YUV420,
		  .mbus_fmt = MEDIA_BUS_FMT_YUYV8_2X8 },
		{ .pixelformat = V4L2_PIX_FMT_NV12,
		  .mbus_fmt = MEDIA_BUS_FMT_YVYU8_2X8 },
		{ .pixelformat = V4L2_PIX_FMT_NV21,
		  .mbus_fmt = MEDIA_BUS_FMT_YVYU8_2X8 },
	};
	const struct tcc_dewarp_mbus_fmt *fotmat = NULL;
	u32 idxList = 0;
	u32 nList = 0;
	u32 mbus_code = 0;

	nList = ARRAY_SIZE(tcc_dewarp_mbus_fmt_list);
	for (idxList = 0; idxList < nList; idxList++) {
		fotmat = &tcc_dewarp_mbus_fmt_list[idxList];
		if (pixelformat == fotmat->pixelformat) {
			mbus_code = fotmat->mbus_fmt;
			break;
		}
	}

	return mbus_code;
}

static struct v4l2_rect *
tcc_dewarp_get_rect_by_target(struct tcc_dewarp_stream *vstream, u32 target)
{
	const struct device *p_dev = NULL;
	struct v4l2_rect *p_rect = NULL;

	p_dev = stream_to_device(vstream);

	switch (target) {
	case V4L2_SEL_TGT_CROP:
	case V4L2_SEL_TGT_CROP_DEFAULT:
		p_rect = &vstream->rect_crop;
		break;
	case V4L2_SEL_TGT_COMPOSE:
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
		p_rect = &vstream->rect_compose;
		break;
	default:
		loge(p_dev, "target(0x%08x) is not supported\n", target);
		break;
	}

	return p_rect;
}

static void tcc_dewarp_print_v4l2_pix_format_mplane(
	const struct tcc_dewarp_stream *vstream,
	const struct v4l2_pix_format_mplane *format)
{
	const struct device *p_dev = NULL;
	char fcc[4] = {
		0,
	};
	u32 idxPlane = 0;

	p_dev = stream_to_device(vstream);

	/* convert pixelformat to fcc */
	(void)strscpy(fcc, (const char *)&format->pixelformat,
		      sizeof(format->pixelformat));

	logd(p_dev, "width: %u, height: %u\n", format->width, format->height);
	logd(p_dev, "pixelformat: %c%c%c%c\n", fcc[0], fcc[1], fcc[2], fcc[3]);
	logd(p_dev, "field: %u, colorspace: %u\n", format->field,
	     format->colorspace);
	logd(p_dev, "num_planes: %u\n", format->num_planes);
	for (idxPlane = 0; idxPlane < format->num_planes; idxPlane++) {
		logd(p_dev, " plane_fmt[%u].sizeimage: %u, .bytesperline: %u\n",
		     idxPlane, format->plane_fmt[idxPlane].sizeimage,
		     format->plane_fmt[idxPlane].bytesperline);
	}
}
#if defined(CONFIG_ARCH_TCC750X)
static void tcc_dewarp_add_ir_planes(struct tcc_dewarp_stream *vstream)
{
	struct v4l2_pix_format_mplane *pix_mp = NULL;

	pix_mp = &vstream->format.fmt.pix_mp;

	pix_mp->plane_fmt[pix_mp->num_planes].bytesperline = pix_mp->width;
	pix_mp->plane_fmt[pix_mp->num_planes].sizeimage =
		pix_mp->plane_fmt[pix_mp->num_planes].bytesperline *
		pix_mp->height;
}
#endif //CONFIG_ARCH_TCC750X
static void
tcc_dewarp_print_v4l2_format(const struct tcc_dewarp_stream *vstream,
			     const struct v4l2_format *format)
{
	const struct device *p_dev = NULL;

	p_dev = stream_to_device(vstream);

	switch (format->type) {
	case (u32)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		tcc_dewarp_print_v4l2_pix_format_mplane(vstream,
							&format->fmt.pix_mp);
		break;
	default:
		loge(p_dev, "type (0x%08x) is not supported\n", format->type);
		break;
	}
}

static void
tcc_dewarp_print_v4l2_buffer_mplane(const struct tcc_dewarp_stream *vstream,
				    const struct v4l2_buffer *p_buf)
{
	const struct device *p_dev = NULL;
	const struct v4l2_plane *plane = NULL;
	u32 idxpln = 0;

	p_dev = stream_to_device(vstream);

	logd(p_dev, "index;: %u\n", p_buf->index);
	logd(p_dev, "type: %u\n", p_buf->type);
	logd(p_dev, "bytesused: %u\n", p_buf->bytesused);
	logd(p_dev, "flags: 0x%08x\n", p_buf->flags);
	logd(p_dev, "field: 0x%08x\n", p_buf->field);
	logd(p_dev, "sequence: %u\n", p_buf->sequence);
	logd(p_dev, "memory: %u\n", p_buf->memory);
	logd(p_dev, "length: 0x%08x\n", p_buf->length);
	for (idxpln = 0; idxpln < p_buf->length; idxpln++) {
		plane = &p_buf->m.planes[idxpln];
		switch (p_buf->memory) {
		case (u32)V4L2_MEMORY_MMAP:
			logd(p_dev, "plane[%u]: 0x%08x\n", idxpln,
			     plane->m.mem_offset);
			break;
		default:
			loge(p_dev, "memory(0x%08x) is not supported\n",
			     p_buf->memory);
			break;
		}
	}
}

static void
tcc_dewarp_print_v4l2_buffer(const struct tcc_dewarp_stream *vstream,
			     const struct v4l2_buffer *buf)
{
	const struct device *p_dev = NULL;

	p_dev = stream_to_device(vstream);

	switch (buf->type) {
	case (u32)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		tcc_dewarp_print_v4l2_buffer_mplane(vstream, buf);
		break;
	default:
		loge(p_dev, "type(0x%08x) is not supported\n", buf->type);
		break;
	}
}

const struct v4l2_file_operations tcc_dewarp_fops = {
	.owner = THIS_MODULE,
	.open = v4l2_fh_open,
	.release = vb2_fop_release,
	.unlocked_ioctl = video_ioctl2,
	.mmap = vb2_fop_mmap,
	.poll = vb2_fop_poll,
};
EXPORT_SYMBOL_GPL(tcc_dewarp_fops);

/* ------------------------------------------------------------------------
 * v4l2_ioctl_ops
 */

static int tcc_dewarp_ioctl_querycap(struct file *pfile, void *fh,
				     struct v4l2_capability *cap)
{
	const struct device *p_dev = NULL;
	struct video_device *vdev = NULL;
	const struct tcc_dewarp_stream *vstream = NULL;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);
	vdev = &vstream->tdev->vdev;

	(void)scnprintf((char *)cap->driver, PAGE_SIZE, "%s", DRIVER_NAME);
	(void)scnprintf((char *)cap->card, PAGE_SIZE, "%s", vdev->name);
	(void)scnprintf((char *)cap->bus_info, PAGE_SIZE,
			"platform:videoinput:%d", vdev->num);

	cap->version = KERNEL_VERSION(5, 4, 00);
	cap->capabilities = V4L2_CAP_DEVICE_CAPS | cap->device_caps;

	logd(p_dev, "driver: %s\n", cap->driver);
	logd(p_dev, "card: %s\n", cap->card);
	logd(p_dev, "bus_info: %s\n", cap->bus_info);
	logd(p_dev, "version: %u.%u.%u\n", (cap->version >> 16) & 0xFFU,
	     (cap->version >> 8) & 0xFFU, (cap->version >> 0) & 0xFFU);
	logd(p_dev, "device_caps: 0x%08x\n", cap->device_caps);
	logd(p_dev, "capabilities: 0x%08x\n", cap->capabilities);

	return ret;
}

static int tcc_dewarp_ioctl_enum_fmt(struct file *pfile, void *fh,
				     struct v4l2_fmtdesc *fmt)
{
	const struct device *p_dev = NULL;
	const struct tcc_dewarp_stream *vstream = NULL;
	u32 index = 0;
	u32 pixelformat = 0;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);

	pixelformat =
		tcc_dewarp_video_get_pixelformat_by_index(vstream, fmt->index);
	if (pixelformat == 0U) {
		logd(p_dev, "format of index(%u) is not supported\n",
		     fmt->index);
		ret = -EINVAL;
	} else {
		index = fmt->index;

		(void)memset(fmt, 0, sizeof(*fmt));

		fmt->index = index;
		fmt->type = (u32)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		fmt->pixelformat = pixelformat;
		(void)strscpy((char *)fmt->description,
			      (const char *)&fmt->pixelformat,
			      sizeof(fmt->pixelformat));

		logd(p_dev, "index: %u\n", fmt->index);
		logd(p_dev, "type: 0x%08x\n", fmt->type);
		logd(p_dev, "flags: 0x%08x\n", fmt->flags);
		logd(p_dev, "description: %s\n", fmt->description);
	}

	return ret;
}

static int tcc_dewarp_ioctl_g_fmt(struct file *pfile, void *fh,
				  struct v4l2_format *fmt)
{
	const struct tcc_dewarp_stream *vstream = NULL;
	int ret = 0;

	vstream = video_drvdata(pfile);

	*fmt = vstream->format;

	return ret;
}

static int tcc_dewarp_ioctl_s_fmt(struct file *pfile, void *fh,
				  struct v4l2_format *fmt)
{
	struct tcc_dewarp_stream *vstream = NULL;
	const struct device *p_dev = NULL;
	struct v4l2_pix_format_mplane *pix_mp = NULL;
	bool ret_bool = false;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);

	ret_bool = tcc_dewarp_video_is_format_supported(vstream, fmt);
	if (!ret_bool) {
		loge(p_dev, "format is not supported\n");
		ret = -EINVAL;
	} else {
		mutex_lock(&vstream->mlock);
		if (vb2_is_busy(&vstream->queue.queue)) {
			loge(p_dev, "vb2_is_busy\n");
		} else {
			(void)memcpy(&vstream->format, fmt, sizeof(*fmt));

			/* additional set */
			pix_mp = &vstream->format.fmt.pix_mp;

			ret = v4l2_fill_pixfmt_mp(pix_mp, pix_mp->pixelformat,
						  pix_mp->width,
						  pix_mp->height);
			if (ret < 0) {
				loge(p_dev, "Failed on v4l2_fill_pixfmt_mp\n");
			} else {
				pix_mp->field = (u32)V4L2_FIELD_NONE;
				pix_mp->colorspace = (u32)V4L2_COLORSPACE_SRGB;
#if defined(CONFIG_ARCH_TCC750X)
				if (vstream->dewarp_wrap.odw_params.in
					    .ir_enable) {
					if (pix_mp->pixelformat !=
						    V4L2_PIX_FMT_RGB32 &&
					    pix_mp->pixelformat !=
						    V4L2_PIX_FMT_BGR32 &&
					    pix_mp->pixelformat !=
						    V4L2_PIX_FMT_ARGB32) {
						tcc_dewarp_add_ir_planes(
							vstream);
					}
				}
#endif //CONFIG_ARCH_TCC750X
			}
		}
		mutex_unlock(&vstream->mlock);
	}

	return ret;
}

static int tcc_dewarp_ioctl_try_fmt(struct file *pfile, void *fh,
				    struct v4l2_format *fmt)
{
	const struct tcc_dewarp_stream *vstream = NULL;
	const struct device *p_dev = NULL;
	bool ret_bool = false;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);

	ret_bool = tcc_dewarp_video_is_format_supported(vstream, fmt);
	if (!ret_bool) {
		tcc_dewarp_print_v4l2_format(vstream, fmt);

		loge(p_dev, "format is not supported\n");
		ret = -EINVAL;
	}

	return ret;
}

static int tcc_dewarp_ioctl_reqbufs(struct file *pfile, void *fh,
				    struct v4l2_requestbuffers *rb)
{
	struct tcc_dewarp_stream *vstream = NULL;
	const struct device *p_dev = NULL;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);

	if (rb->type != (u32)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		loge(p_dev, "type (0x%08x) is not supported\n", rb->type);
		ret = -EINVAL;
	} else {
		ret = vb2_ioctl_reqbufs(pfile, &vstream->queue.queue, rb);
		if (ret >= 0) {
			logd(p_dev, "count: %d\n", rb->count);
			logd(p_dev, "type: 0x%08x\n", rb->type);
			logd(p_dev, "memory: 0x%08x\n", rb->memory);
		}
	}

	return ret;
}

static int tcc_dewarp_ioctl_querybuf(struct file *pfile, void *fh,
				     struct v4l2_buffer *p_buf)
{
	struct tcc_dewarp_stream *vstream = NULL;
	const struct device *p_dev = NULL;
	struct vb2_buffer *vb = NULL;
	u32 dma_addrs[MAX_PLANES];
	u32 idxpln = 0;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);

	ret = vb2_querybuf(&vstream->queue.queue, p_buf);
	if (ret < 0) {
		loge(p_dev, "vb2_querybuf, ret: %d\n", ret);
		ret = -EINVAL;
	} else {
		/* print for debug */
		tcc_dewarp_print_v4l2_buffer(vstream, p_buf);

		vb = vstream->queue.queue.bufs[p_buf->index];
		(void)memset(dma_addrs, 0, sizeof(dma_addrs));
		tcc_dewarp_get_dma_addrs(vstream, vb, dma_addrs);
		tcc_dewarp_print_dma_addrs(vstream, vb, dma_addrs);

		for (idxpln = 0; idxpln < p_buf->length; idxpln++) {
			/* provid dma addrs */
			p_buf->m.planes[idxpln].reserved[0] = dma_addrs[idxpln];
		}
	}

	return ret;
}

static int tcc_dewarp_ioctl_qbuf(struct file *pfile, void *fh,
				 struct v4l2_buffer *p_buf)
{
	struct tcc_dewarp_stream *vstream = NULL;
	const struct device *p_dev = NULL;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);

	/* device is available */
	ret = vb2_qbuf(&vstream->queue.queue,
		       vstream->tdev->vdev.v4l2_dev->mdev, p_buf);
	if (ret < 0) {
		loge(p_dev, "vb2_qbuf, ret: %d\n", ret);
		ret = -EINVAL;
	}

	return ret;
}

static int tcc_dewarp_ioctl_expbuf(struct file *pfile, void *fh,
				   struct v4l2_exportbuffer *p_buf)
{
	struct tcc_dewarp_stream *vstream = NULL;
	const struct device *p_dev = NULL;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);

	ret = vb2_expbuf(&vstream->queue.queue, p_buf);
	if (ret < 0) {
		loge(p_dev, "vb2_expbuf, ret: %d\n", ret);
		ret = -EINVAL;
	}

	return ret;
}

static int tcc_dewarp_ioctl_dqbuf(struct file *pfile, void *fh,
				  struct v4l2_buffer *p_buf)
{
	struct tcc_dewarp_stream *vstream = NULL;
	const struct device *p_dev = NULL;
	bool non_block = false;
	u32 idxpln = 0;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);

	non_block = ((pfile->f_flags & (u32)O_NONBLOCK) != 0U);
	ret = vb2_dqbuf(&vstream->queue.queue, p_buf, non_block);
	if (ret < 0) {
		loge(p_dev, "vb2_dqbuf, ret: %d\n", ret);
		ret = -EINVAL;
	} else {
		for (idxpln = 0; idxpln < p_buf->length; idxpln++) {
			/* set bytesused to inform the data size */
			p_buf->m.planes[idxpln].bytesused =
				vstream->format.fmt.pix_mp.plane_fmt[idxpln]
					.sizeimage;
		}

		/* print for debug */
		tcc_dewarp_print_v4l2_buffer(vstream, p_buf);
	}

	return ret;
}

static int tcc_dewarp_ioctl_streamon(struct file *pfile, void *fh,
				     enum v4l2_buf_type type)
{
	struct tcc_dewarp_stream *vstream = NULL;
	const struct device *p_dev = NULL;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);

	mutex_lock(&vstream->mlock);
	ret = vb2_streamon(&vstream->queue.queue, type);
	mutex_unlock(&vstream->mlock);
	if (ret < 0) {
		loge(p_dev, "vb2_streamon, ret: %d\n", ret);
		ret = -EINVAL;
	}

	return ret;
}

static int tcc_dewarp_ioctl_streamoff(struct file *pfile, void *fh,
				      enum v4l2_buf_type type)
{
	struct tcc_dewarp_stream *vstream = NULL;
	const struct device *p_dev = NULL;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);

	mutex_lock(&vstream->mlock);
	ret = vb2_streamoff(&vstream->queue.queue, type);
	mutex_unlock(&vstream->mlock);
	if (ret < 0) {
		loge(p_dev, "vb2_streamoff, ret: %d\n", ret);
		ret = -EINVAL;
	}

	return ret;
}

static int tcc_dewarp_ioctl_enum_input(struct file *pfile, void *fh,
				       struct v4l2_input *input)
{
	const struct tcc_dewarp_stream *vstream = NULL;
	struct v4l2_subdev *subdev = NULL;
	const struct device *p_dev = NULL;
	u32 index = 0;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);

	index = input->index;
	if (index != 0U) {
		loge(p_dev, "input %d is not supported\n", index);
		ret = -EINVAL;
	} else {
		(void)memset(input, 0, sizeof(*input));

		input->index = index;
		(void)scnprintf((char *)input->name, PAGE_SIZE,
				"v4l2 subdev[%u]", index);
		input->type = V4L2_INPUT_TYPE_CAMERA;

		subdev = vstream->tdev->tsubdev.sd;
		ret = tcc_dewarp_subdev_video_g_input_status(subdev,
							     &input->status);
		switch (ret) {
		case -ENODEV:
			logd(p_dev, "%s - subdev is null\n", input->name);
			break;
		case -ENOIOCTLCMD:
			logd(p_dev,
			     "%s - video.g_input_status is not supported\n",
			     input->name);
			ret = -ENOTTY;
			break;
		default:
			logd(p_dev, "%s - status: 0x%08x\n", input->name,
			     input->status);
			break;
		}
	}

	return ret;
}

static int tcc_dewarp_ioctl_g_input(struct file *pfile, void *fh, u32 *input)
{
	int ret = 0;

	/* support 0th input only */
	*input = 0;

	return ret;
}

static int tcc_dewarp_ioctl_s_input(struct file *pfile, void *fh, u32 input)
{
	const struct tcc_dewarp_stream *vstream = NULL;
	const struct device *p_dev = NULL;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);

	/* support 0th input only */
	if (input != 0U) {
		loge(p_dev, "input %d is not supported\n", input);
		ret = -EINVAL;
	}

	return ret;
}

static int tcc_dewarp_ioctl_g_pixelaspect(struct file *pfile, void *fh,
					  int buf_type,
					  struct v4l2_fract *aspect)
{
	const struct tcc_dewarp_stream *vstream = NULL;
	const struct device *p_dev = NULL;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);

	/* Note
	 * Unfortunately in the case of multiplanar buffer types
	 * (V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE and
	 * V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) this API was messed up with
	 * regards to how the v4l2_cropcap type field should be filled in.
	 * Some drivers only accepted the _MPLANE buffer type while other
	 * drivers only accepted a non-multiplanar buffer type
	 * (i.e. without the _MPLANE at the end).
	 */
	if ((buf_type != (int)V4L2_BUF_TYPE_VIDEO_CAPTURE) &&
	    (buf_type != (int)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)) {
		loge(p_dev, "type: 0x%08x, is not supported\n", buf_type);
		ret = -EINVAL;
	}

	if (ret >= 0) {
		aspect->numerator = 1;
		aspect->denominator = 1;
	}

	return ret;
}

static int tcc_dewarp_ioctl_g_selection(struct file *pfile, void *fh,
					struct v4l2_selection *s)
{
	struct tcc_dewarp_stream *vstream = NULL;
	const struct v4l2_rect *t_rect = NULL;
	int ret = 0;

	vstream = video_drvdata(pfile);

	t_rect = tcc_dewarp_get_rect_by_target(vstream, s->target);
	if (t_rect == NULL) {
		/* error: tcc_dewarp_get_rect_by_target */
		ret = -EINVAL;
	} else {
		/* get target's rect info */
		(void)memcpy(&s->r, t_rect, sizeof(*t_rect));
	}

	return ret;
}

static int tcc_dewarp_ioctl_s_selection(struct file *pfile, void *fh,
					struct v4l2_selection *s)
{
	struct tcc_dewarp_stream *vstream = NULL;
	struct v4l2_rect *t_rect = NULL;
	int ret = 0;

	vstream = video_drvdata(pfile);

	t_rect = tcc_dewarp_get_rect_by_target(vstream, s->target);
	if (t_rect == NULL) {
		/* error: tcc_dewarp_get_rect_by_target */
		ret = -EINVAL;
	} else {
		/* set target's rect info */
		(void)memcpy(t_rect, &s->r, sizeof(*t_rect));
	}

	return ret;
}

static int tcc_dewarp_ioctl_g_parm(struct file *pfile, void *fh,
				   struct v4l2_streamparm *a)
{
	const struct device *p_dev = NULL;
	const struct tcc_dewarp_stream *vstream = NULL;
	struct v4l2_captureparm *capture = NULL;
	struct v4l2_subdev *subdev = NULL;
	struct v4l2_subdev_frame_interval t_interval = {
		0,
	};
	const struct v4l2_fract *timeperframe = NULL;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);
	subdev = vstream->tdev->tsubdev.sd;

	(void)memset(&t_interval, 0, sizeof(t_interval));
	t_interval.interval.numerator = 1;
	t_interval.interval.denominator = 1;

	/* if subdev exists, then get its interval */
	if (subdev != NULL) {
		ret = v4l2_subdev_call(subdev, video, g_frame_interval,
				       &t_interval);
		if (ret != 0) {
			/* failure */
			logw(p_dev, "video.g_frame_interval, ret: %d\n", ret);
		}
	}

	switch (a->type) {
	case (u32)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		capture = &a->parm.capture;
		capture->timeperframe = t_interval.interval;
		timeperframe = &capture->timeperframe;
		break;
	default:
		timeperframe = &t_interval.interval;
		break;
	}

	logd(p_dev, "framerate: %u / %u\n", timeperframe->numerator,
	     timeperframe->denominator);

	return 0;
}

static int tcc_dewarp_ioctl_s_parm(struct file *pfile, void *fh,
				   struct v4l2_streamparm *a)
{
	const struct device *p_dev = NULL;
	const struct tcc_dewarp_stream *vstream = NULL;
	struct v4l2_captureparm *capture = NULL;
	struct v4l2_subdev *subdev = NULL;
	struct v4l2_subdev_frame_interval t_interval = {
		0,
	};
	struct v4l2_fract *timeperframe = NULL;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);
	subdev = vstream->tdev->tsubdev.sd;

	(void)memset(&t_interval, 0, sizeof(t_interval));

	switch (a->type) {
	case (u32)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		capture = &a->parm.capture;
		timeperframe = &capture->timeperframe;
		break;
	default:
		timeperframe = &t_interval.interval;
		break;
	}

	if ((timeperframe->numerator == 0U) ||
	    (timeperframe->denominator == 0U)) {
		timeperframe->numerator = 1;
		timeperframe->denominator = 1;
	}

	logd(p_dev, "framerate: %u / %u\n", timeperframe->numerator,
	     timeperframe->denominator);

	/* if subdev exists, then set interval to its one */
	if (subdev != NULL) {
		ret = v4l2_subdev_call(subdev, video, s_frame_interval,
				       &t_interval);
		if (ret != 0) {
			logd(p_dev, "video.s_frame_interval, ret: %d\n", ret);
		}
	}

	return 0;
}

/*
 * framesize depends on capabilities of video-input path, not video sources
 * because wdma can even save video data with black video data
 * framesize is bigger than video source's supported one and video-input path doesn't have any scaler.
 */

static int tcc_dewarp_ioctl_enum_framesizes(struct file *pfile, void *fh,
					    struct v4l2_frmsizeenum *fsize)
{
	const struct tcc_dewarp_stream *vstream = NULL;
	const struct device *p_dev = NULL;
	bool ret_bool = true;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);

	if (fsize->index != 0U) {
		loge(p_dev, "index(%u) is not 0\n", fsize->index);
		ret = -EINVAL;
	}

	ret_bool = tcc_dewarp_video_is_pixelformat_supported(
		vstream, fsize->pixel_format);
	if (!ret_bool) {
		loge(p_dev, "tcc_dewarp_video_is_pixelformat_supported\n");
		ret = -EINVAL;
	}

	if (ret == 0) {
		fsize->type = (u32)V4L2_FRMSIZE_TYPE_STEPWISE;
		fsize->stepwise.min_width = 4;
		fsize->stepwise.max_width = MAX_FRAMEWIDTH;
		fsize->stepwise.step_width = 4;
		fsize->stepwise.min_height = 4;
		fsize->stepwise.max_height = MAX_FRAMEHEIGHT;
		fsize->stepwise.step_height = 2;

		logd(p_dev, "framesize: %u * %u ~ %u * %u (step: %u, %u)\n",
		     fsize->stepwise.min_width, fsize->stepwise.min_height,
		     fsize->stepwise.max_width, fsize->stepwise.max_height,
		     fsize->stepwise.step_width, fsize->stepwise.step_height);
	}

	return ret;
}

/*
 * framesize depends on capabilities of video sources, not video-input path
 * because video-input path can get video data from video source as much as possible.
 */

static int tcc_dewarp_ioctl_enum_frameintervals(struct file *pfile, void *fh,
						struct v4l2_frmivalenum *fival)
{
	const struct tcc_dewarp_stream *vstream = NULL;
	const struct device *p_dev = NULL;
	struct v4l2_subdev *subdev = NULL;
	bool ret_bool = true;
	int ret_call = 0;
	struct v4l2_subdev_frame_interval_enum fie = {
		0,
	};
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);
	subdev = vstream->tdev->tsubdev.sd;

	ret_bool = tcc_dewarp_video_is_pixelformat_supported(
		vstream, fival->pixel_format);
	if (!ret_bool) {
		loge(p_dev, "tcc_dewarp_video_is_pixelformat_supported\n");
		ret = -EINVAL;
	}

	if (subdev != NULL) {
		fie.index = fival->index;
		fie.pad = 0;
		fie.code = tcc_dewarp_get_mbus_code_by_pixelformat(
			fival->pixel_format);
		fie.width = fival->width;
		fie.height = fival->height;
		fie.which = (u32)V4L2_SUBDEV_FORMAT_ACTIVE;

		ret_call = v4l2_subdev_call(subdev, pad, enum_frame_interval,
					    NULL, &fie);
		if (ret_call != 0) {
			logw(p_dev, "pad.enum_frame_interval, ret: %d\n",
			     ret_call);
			ret = ret_call;
		} else {
			fival->type = (u32)V4L2_FRMIVAL_TYPE_DISCRETE;
			(void)memcpy(&fival->discrete, &fie.interval,
				     sizeof(fie.interval));
		}
	}

	if (ret == 0) {
		logd(p_dev, "index: %u, width: %u, height: %u\n", fival->index,
		     fival->width, fival->height);
		logd(p_dev, " . numerator: %u, denominator: %u\n",
		     fival->discrete.numerator, fival->discrete.denominator);
	}

	return ret;
}

static long tcc_dewarp_ioctl_default(struct file *pfile, void *fh, bool valid_prio,
				u32 cmd, void *arg)
{
	struct tcc_dewarp_stream *vstream = NULL;
	struct device *p_dev = NULL;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);

	switch (cmd) {
	case (u32)VIDIOC_S_DEWARP_PARAMS:
		ret = tcc_dewarp_video_s_dewarp_params(vstream, (struct dewarp_params *)arg);
		break;
	default:
		ret = - ENOTTY;
		break;
	}

	return ret;
}

const struct v4l2_ioctl_ops tcc_dewarp_ioctl_ops = {
	.vidioc_querycap = tcc_dewarp_ioctl_querycap,
	.vidioc_enum_fmt_vid_cap = tcc_dewarp_ioctl_enum_fmt,
	.vidioc_g_fmt_vid_cap_mplane = tcc_dewarp_ioctl_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane = tcc_dewarp_ioctl_s_fmt,
	.vidioc_try_fmt_vid_cap_mplane = tcc_dewarp_ioctl_try_fmt,
	.vidioc_reqbufs = tcc_dewarp_ioctl_reqbufs,
	.vidioc_querybuf = tcc_dewarp_ioctl_querybuf,
	.vidioc_qbuf = tcc_dewarp_ioctl_qbuf,
	.vidioc_expbuf = tcc_dewarp_ioctl_expbuf,
	.vidioc_dqbuf = tcc_dewarp_ioctl_dqbuf,
	.vidioc_streamon = tcc_dewarp_ioctl_streamon,
	.vidioc_streamoff = tcc_dewarp_ioctl_streamoff,
	.vidioc_enum_input = tcc_dewarp_ioctl_enum_input,
	.vidioc_g_input = tcc_dewarp_ioctl_g_input,
	.vidioc_s_input = tcc_dewarp_ioctl_s_input,
	.vidioc_g_pixelaspect = tcc_dewarp_ioctl_g_pixelaspect,
	.vidioc_g_selection = tcc_dewarp_ioctl_g_selection,
	.vidioc_s_selection = tcc_dewarp_ioctl_s_selection,
	.vidioc_g_parm = tcc_dewarp_ioctl_g_parm,
	.vidioc_s_parm = tcc_dewarp_ioctl_s_parm,
	.vidioc_enum_framesizes = tcc_dewarp_ioctl_enum_framesizes,
	.vidioc_enum_frameintervals = tcc_dewarp_ioctl_enum_frameintervals,
	.vidioc_default = tcc_dewarp_ioctl_default,
};
EXPORT_SYMBOL_GPL(tcc_dewarp_ioctl_ops);

int tcc_dewarp_v4l2_init_format(struct tcc_dewarp_device *tdev)
{
	const struct device *p_dev = NULL;
	struct tcc_dewarp_stream *vstream = NULL;
	struct v4l2_format *format = NULL;
	struct v4l2_pix_format_mplane *pix_mp = NULL;
	int ret = 0;

	vstream = &tdev->vstream;
	p_dev = stream_to_device(vstream);

	format = &vstream->format;
	format->type = (u32)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	pix_mp = &format->fmt.pix_mp;
	pix_mp->pixelformat = (u32)V4L2_PIX_FMT_RGB24;
	pix_mp->width = DEFAULT_FRAMEWIDTH;
	pix_mp->height = DEFAULT_FRAMEHEIGHT;
	pix_mp->field = (u32)V4L2_FIELD_NONE;
	pix_mp->colorspace = (u32)V4L2_COLORSPACE_SRGB;

	ret = v4l2_fill_pixfmt_mp(pix_mp, pix_mp->pixelformat, pix_mp->width,
				  pix_mp->height);
	if (ret < 0) {
		loge(p_dev, "Failed on v4l2_fill_pixfmt_mp\n");
	} else {
		logd(p_dev, "Succeed on v4l2_fill_pixfmt_mp\n");
	}

	tcc_dewarp_print_v4l2_format(vstream, &vstream->format);

	return ret;
}
EXPORT_SYMBOL_GPL(tcc_dewarp_v4l2_init_format);
MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips Dewarp Driver");
MODULE_LICENSE("GPL");