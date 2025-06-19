// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *      tccvin_v4l2.c  --  Telechips Video-Input Path Driver
 *
 *      Copyright (C) 2005-2010
 *          Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 ******************************************************************************


 *   Modified by Telechips Inc.


 *   Modified date : 2020


 *   Description : v4l2 interface


 *****************************************************************************/

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

#include "tccvin_common.h"
#include "tccvin_video.h"

static inline struct tccvin_stream *queue_to_stream(struct tccvin_queue *queue)
{
	return container_of(queue, struct tccvin_stream, queue);
}

static inline struct tccvin_buffer *
vb2_v4l2_buf_to_buf(struct vb2_v4l2_buffer *buf)
{
	return container_of(buf, struct tccvin_buffer, buf);
}

/*
 * Return all queued buffers to videobuf2 in the requested state.
 *
 * This function must be called with the queue spinlock held.
 */

static void tccvin_queue_return_buffers(
	struct tccvin_queue *queue,
	enum tccvin_buffer_state buf_state)
{
	enum vb2_buffer_state vb2_state;

	vb2_state = (buf_state == TCCVIN_BUF_STATE_ERROR) ?
			    VB2_BUF_STATE_ERROR :
			    VB2_BUF_STATE_QUEUED;

	while (list_empty(&queue->buf_list) != 1) {
		struct tccvin_buffer *buf = NULL;

		buf = list_first_entry(&queue->buf_list, struct tccvin_buffer,
				       entry);
		list_del(&buf->entry);
		vb2_buffer_done(&buf->buf.vb2_buf, vb2_state);
	}
}

/* -----------------------------------------------------------------------------
 * videobuf2 queue operations
 */

static int tccvin_queue_setup(struct vb2_queue *vq,
			      u32 *nbuffers, u32 *nplanes,
			      u32 sizes[], struct device *alloc_devs[])
{
	struct tccvin_queue *queue = NULL;
	const struct tccvin_stream *vstream = NULL;
	const struct device *p_dev = NULL;
	struct v4l2_pix_format_mplane pix_mp = {
		0,
	};
	u32 idxPlane = 0;
	int ret = 0;

	queue = (struct tccvin_queue *)vb2_get_drv_priv(vq);
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

		/* update sizeimage */
		for (idxPlane = 0; idxPlane < *nplanes; idxPlane++) {
			sizes[idxPlane] = pix_mp.plane_fmt[idxPlane].sizeimage;
			logd(p_dev, "plane[%u].sizeimage: %u\n", idxPlane,
			     sizes[idxPlane]);
		}
	}

	return ret;
}

static int tccvin_buf_prepare(struct vb2_buffer *vb)
{
	struct tccvin_queue *p_queue = NULL;
	struct tccvin_stream *vstream = NULL;
	struct vb2_v4l2_buffer *vbuf = NULL;
	struct v4l2_pix_format_mplane *format = NULL;
	int ret = 0;
	unsigned int i;

	p_queue = (struct tccvin_queue *)vb2_get_drv_priv(vb->vb2_queue);
	vstream = queue_to_stream(p_queue);
	vbuf = to_vb2_v4l2_buffer(vb);
	format = &vstream->format.fmt.pix_mp;

	for (i = 0; i < format->num_planes; i++) {
		if (format->plane_fmt[i].sizeimage > vb2_plane_size(vb, i)) {
			ret = -EINVAL;
			break;
		} else {
			vb2_set_plane_payload(vb, i, format->plane_fmt[i].sizeimage);
		}
	}

	if ((ret == 0) && unlikely((p_queue->flags & (u32)TCCVIN_QUEUE_DISCONNECTED) != 0U)) {
		/* p_queue->flags is wrong */
		ret = -ENODEV;
	}

	return ret;
}

static void tccvin_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = NULL;
	struct tccvin_queue *p_queue = NULL;
	struct tccvin_buffer *buf = NULL;
	unsigned long flags = 0;

	vbuf = to_vb2_v4l2_buffer(vb);

	p_queue = (struct tccvin_queue *)vb2_get_drv_priv(vb->vb2_queue);
	buf = vb2_v4l2_buf_to_buf(vbuf);

	spin_lock_irqsave(&p_queue->slock, flags);
	if (likely((p_queue->flags & (u32)TCCVIN_QUEUE_DISCONNECTED) == 0U)) {
		list_add_tail(&buf->entry, &p_queue->buf_list);
	} else {
		/* If the device is disconnected return the buffer to userspace
		 * directly. The next QBUF call will fail with -ENODEV.
		 */
		vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
	}

	spin_unlock_irqrestore(&p_queue->slock, flags);
}

static int tccvin_start_streaming(struct vb2_queue *vq, u32 count)
{
	struct tccvin_queue *p_queue = NULL;
	struct tccvin_stream *vstream = NULL;
	int ret = 0;

	p_queue = (struct tccvin_queue *)vb2_get_drv_priv(vq);
	vstream = queue_to_stream(p_queue);

	ret = media_pipeline_start(&vstream->tdev->vdev.entity,
				   &vstream->tdev->tccmd->pipe);
	if (ret < 0) {
		spin_lock_irq(&p_queue->slock);
		tccvin_queue_return_buffers(p_queue, TCCVIN_BUF_STATE_QUEUED);
		spin_unlock_irq(&p_queue->slock);
	}

	if (ret >= 0) {
		ret = tccvin_video_streamon(vstream);
		if (ret != 0) {
			spin_lock_irq(&p_queue->slock);
			tccvin_queue_return_buffers(p_queue,
						    TCCVIN_BUF_STATE_QUEUED);
			spin_unlock_irq(&p_queue->slock);
		}
	}

	return ret;
}

static void tccvin_stop_streaming(struct vb2_queue *vq)
{
	struct tccvin_queue *p_queue = NULL;
	struct tccvin_stream *vstream = NULL;

	p_queue = (struct tccvin_queue *)vb2_get_drv_priv(vq);
	vstream = queue_to_stream(p_queue);

	(void)tccvin_video_streamoff(vstream);

	media_pipeline_stop(&vstream->tdev->vdev.entity);

	spin_lock_irq(&p_queue->slock);
	tccvin_queue_return_buffers(p_queue, TCCVIN_BUF_STATE_ERROR);
	spin_unlock_irq(&p_queue->slock);
}

static const struct vb2_ops tccvin_qops = {
	.queue_setup = tccvin_queue_setup,
	.buf_prepare = tccvin_buf_prepare,
	.buf_queue = tccvin_buf_queue,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.start_streaming = tccvin_start_streaming,
	.stop_streaming = tccvin_stop_streaming,
};

int tccvin_v4l2_init_queue(struct tccvin_queue *queue, bool drop_corrupted)
{
	const struct tccvin_stream *vstream = NULL;
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
	q->is_output = (u8)DMA_TO_DEVICE;
	q->drv_priv = queue;
	q->buf_struct_size = (u32)sizeof(struct tccvin_buffer);
	q->ops = &tccvin_qops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->timestamp_flags = (u32)V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC |
			     (u32)V4L2_BUF_FLAG_TSTAMP_SRC_SOE;
	q->dev = &vstream->tdev->pdev->dev;
	q->lock = &vstream->tdev->pdev->dev.mutex;
	q->min_buffers_needed = 1;

	ret = vb2_queue_init(&queue->queue);
	if (ret != 0) {
		/* failure of queue init */
		ret = -1;
	} else {
		spin_lock_init(&queue->slock);
		INIT_LIST_HEAD(&queue->buf_list);
		queue->flags =
			drop_corrupted ? (u32)TCCVIN_QUEUE_DROP_CORRUPTED : 0U;
		logd(p_dev, "drop_corrupted: %d, queue->flags: 0x%08x",
		     drop_corrupted, queue->flags);
	}

	return ret;
}

EXPORT_SYMBOL_GPL(tccvin_v4l2_init_queue);

static u32 tccvin_get_mbus_code_by_pixelformat(u32 pixelformat)
{
	struct tccvin_mbus_fmt {
		u32 pixelformat;
		u32 mbus_fmt;
	};
	const struct tccvin_mbus_fmt tccvin_mbus_fmt_list[] = {
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
	const struct tccvin_mbus_fmt *fotmat = NULL;
	u32 idxList = 0;
	u32 nList = 0;
	u32 mbus_code = 0;

	nList = ARRAY_SIZE(tccvin_mbus_fmt_list);
	for (idxList = 0; idxList < nList; idxList++) {
		fotmat = &tccvin_mbus_fmt_list[idxList];
		if (pixelformat == fotmat->pixelformat) {
			mbus_code = fotmat->mbus_fmt;
			break;
		}
	}

	return mbus_code;
}

static struct v4l2_rect *
tccvin_get_rect_by_target(struct tccvin_stream *vstream, u32 target)
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

static void
tccvin_print_v4l2_pix_format_mplane(const struct tccvin_stream *vstream,
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

static void tccvin_print_v4l2_format(const struct tccvin_stream *vstream,
				     const struct v4l2_format *format)
{
	const struct device *p_dev = NULL;

	p_dev = stream_to_device(vstream);

	switch (format->type) {
	case (u32)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		tccvin_print_v4l2_pix_format_mplane(vstream,
						    &format->fmt.pix_mp);
		break;
	default:
		loge(p_dev, "type (0x%08x) is not supported\n", format->type);
		break;
	}
}

static void tccvin_print_v4l2_buffer_mplane(const struct tccvin_stream *vstream,
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

static void tccvin_print_v4l2_buffer(const struct tccvin_stream *vstream,
				     const struct v4l2_buffer *buf)
{
	const struct device *p_dev = NULL;

	p_dev = stream_to_device(vstream);

	switch (buf->type) {
	case (u32)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		tccvin_print_v4l2_buffer_mplane(vstream, buf);
		break;
	default:
		loge(p_dev, "type(0x%08x) is not supported\n", buf->type);
		break;
	}
}

const struct v4l2_file_operations tccvin_fops = {
	.owner = THIS_MODULE,
	.open = v4l2_fh_open,
	.release = vb2_fop_release,
	.unlocked_ioctl = video_ioctl2,
	.mmap = vb2_fop_mmap,
	.poll = vb2_fop_poll,
};

EXPORT_SYMBOL_GPL(tccvin_fops);

/* ------------------------------------------------------------------------
 * v4l2_ioctl_ops
 */

static int tccvin_ioctl_querycap(struct file *pfile, void *fh,
				 struct v4l2_capability *cap)
{
	const struct device *p_dev = NULL;
	struct video_device *vdev = NULL;
	const struct tccvin_stream *vstream = NULL;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);
	vdev = &vstream->tdev->vdev;

	(void)scnprintf((char *)cap->driver, 16, "%s", DRIVER_NAME);
	(void)scnprintf((char *)cap->card, 32, "%s", vdev->name);
	(void)scnprintf((char *)cap->bus_info, 32, "platform:videoinput:%d",
			vdev->num);

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

static int tccvin_ioctl_enum_fmt(struct file *pfile, void *fh,
				 struct v4l2_fmtdesc *fmt)
{
	const struct device *p_dev = NULL;
	const struct tccvin_stream *vstream = NULL;
	u32 index = 0;
	u32 pixelformat = 0;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);

	pixelformat = tccvin_video_get_pixelformat_by_index(fmt->index);
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

static int tccvin_ioctl_g_fmt(struct file *pfile, void *fh,
			      struct v4l2_format *fmt)
{
	const struct tccvin_stream *vstream = NULL;
	int ret = 0;

	vstream = video_drvdata(pfile);

	*fmt = vstream->format;

	return ret;
}

static int tccvin_ioctl_s_fmt(struct file *pfile, void *fh,
			      struct v4l2_format *fmt)
{
	struct tccvin_stream *vstream = NULL;
	const struct device *p_dev = NULL;
	struct v4l2_pix_format_mplane *pix_mp = NULL;

	bool ret_bool = false;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);

	ret_bool = tccvin_video_is_format_supported(vstream, fmt);
	if (!ret_bool) {
		loge(p_dev, "format is not supported\n");
		ret = -EINVAL;
	} else {
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
			}
		}
	}

	return ret;
}

static int tccvin_ioctl_try_fmt(struct file *pfile, void *fh,
				struct v4l2_format *fmt)
{
	const struct tccvin_stream *vstream = NULL;
	const struct device *p_dev = NULL;

	bool ret_bool = false;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);

	ret_bool = tccvin_video_is_format_supported(vstream, fmt);
	if (!ret_bool) {
		tccvin_print_v4l2_format(vstream, fmt);

		loge(p_dev, "format is not supported\n");
		ret = -EINVAL;
	}

	return ret;
}

static int tccvin_ioctl_querybuf(struct file *pfile, void *fh,
				 struct v4l2_buffer *p_buf)
{
	struct tccvin_stream *vstream = NULL;
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
		tccvin_print_v4l2_buffer(vstream, p_buf);

		vb = vstream->queue.queue.bufs[p_buf->index];
		(void)memset(dma_addrs, 0, sizeof(dma_addrs));
		tccvin_get_dma_addrs(vstream, vb, dma_addrs);
		tccvin_print_dma_addrs(vstream, vb, dma_addrs);

		for (idxpln = 0; idxpln < p_buf->length; idxpln++) {
			/* provid dma addrs */
			p_buf->m.planes[idxpln].reserved[0] = dma_addrs[idxpln];
		}
	}

	return ret;
}

static int tccvin_ioctl_enum_input(struct file *pfile, void *fh,
				   struct v4l2_input *input)
{
	const struct tccvin_stream *vstream = NULL;
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
		(void)scnprintf((char *)input->name, 32, "v4l2 subdev[%u]",
				index);
		input->type = V4L2_INPUT_TYPE_CAMERA;

		subdev = vstream->tdev->tsubdev.sd;
		ret = tccvin_subdev_video_g_input_status(subdev,
							 &input->status);
		switch (ret) {
		case -ENODEV:
			loge(p_dev, "%s - subdev is null\n", input->name);
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

static int tccvin_ioctl_g_input(struct file *pfile, void *fh, u32 *input)
{
	int ret = 0;

	/* support 0th input only */
	*input = 0;

	return ret;
}

static int tccvin_ioctl_s_input(struct file *pfile, void *fh, u32 input)
{
	const struct tccvin_stream *vstream = NULL;
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

static int tccvin_ioctl_g_pixelaspect(struct file *pfile, void *fh,
				      int buf_type, struct v4l2_fract *aspect)
{
	const struct tccvin_stream *vstream = NULL;
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

static int tccvin_ioctl_g_selection(struct file *pfile, void *fh,
				    struct v4l2_selection *s)
{
	struct tccvin_stream *vstream = NULL;
	const struct v4l2_rect *t_rect = NULL;
	int ret = 0;

	vstream = video_drvdata(pfile);

	t_rect = tccvin_get_rect_by_target(vstream, s->target);
	if (t_rect == NULL) {
		/* error: tccvin_get_rect_by_target */
		ret = -EINVAL;
	} else {
		/* get target's rect info */
		(void)memcpy(&s->r, t_rect, sizeof(*t_rect));
	}

	return ret;
}

static int tccvin_ioctl_s_selection(struct file *pfile, void *fh,
				    struct v4l2_selection *s)
{
	struct tccvin_stream *vstream = NULL;
	struct v4l2_rect *t_rect = NULL;
	int ret = 0;

	vstream = video_drvdata(pfile);

	t_rect = tccvin_get_rect_by_target(vstream, s->target);
	if (t_rect == NULL) {
		/* error: tccvin_get_rect_by_target */
		ret = -EINVAL;
	} else {
		/* set target's rect info */
		(void)memcpy(t_rect, &s->r, sizeof(*t_rect));
	}

	return ret;
}

static int tccvin_ioctl_g_parm(struct file *pfile, void *fh,
			       struct v4l2_streamparm *a)
{
	const struct device *p_dev = NULL;
	const struct tccvin_stream *vstream = NULL;
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

static int tccvin_ioctl_s_parm(struct file *pfile, void *fh,
			       struct v4l2_streamparm *a)
{
	const struct device *p_dev = NULL;
	const struct tccvin_stream *vstream = NULL;
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
			/* error */
			loge(p_dev, "video.s_frame_interval, ret: %d\n", ret);
		}
	}

	return 0;
}

/*
 * framesize depends on capabilities of video-input path, not video sources
 * because wdma can even save video data with black video data
 * framesize is bigger than video source's supported one and video-input path doesn't have any scaler.
 */

static int tccvin_ioctl_enum_framesizes(struct file *pfile, void *fh,
					struct v4l2_frmsizeenum *fsize)
{
	const struct tccvin_stream *vstream = NULL;
	const struct device *p_dev = NULL;

	bool ret_bool = true;
	int ret = 0;

	vstream = video_drvdata(pfile);
	p_dev = stream_to_device(vstream);

	if (fsize->index != 0U) {
		loge(p_dev, "index(%u) is not 0\n", fsize->index);
		ret = -EINVAL;
	}

	ret_bool = tccvin_video_is_pixelformat_supported(vstream,
							 fsize->pixel_format);
	if (!ret_bool) {
		loge(p_dev, "tccvin_video_is_pixelformat_supported\n");
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

static int tccvin_ioctl_enum_frameintervals(struct file *pfile, void *fh,
					    struct v4l2_frmivalenum *fival)
{
	const struct tccvin_stream *vstream = NULL;
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

	ret_bool = tccvin_video_is_pixelformat_supported(vstream,
							 fival->pixel_format);
	if (!ret_bool) {
		loge(p_dev, "tccvin_video_is_pixelformat_supported\n");
		ret = -EINVAL;
	}

	if (subdev != NULL) {
		fie.index = fival->index;
		fie.pad = 0;
		fie.code = tccvin_get_mbus_code_by_pixelformat(
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

static long tccvin_ioctl_default(struct file *pfile, void *fh, bool valid_prio,
				 u32 cmd, void *arg)
{
	struct tccvin_stream *vstream = NULL;
	int ret = 0;

	vstream = video_drvdata(pfile);

	switch (cmd) {
	case (u32)VIDIOC_CHECK_PATH_STATUS:

		tccvin_video_check_path_status(vstream, (u32 *)arg);
		break;

	case (u32)VIDIOC_G_LASTFRAME_ADDRS:

		ret = tccvin_video_get_lastframe_addrs(vstream, (u32 *)arg);
		break;

	case (u32)VIDIOC_CREATE_LASTFRAME:

		ret = tccvin_video_create_lastframe(vstream, (u32 *)arg);
		break;

	case (u32)VIDIOC_S_HANDOVER:

		ret = tccvin_video_s_handover(vstream, (u32 *)arg);
		break;

	case (u32)VIDIOC_S_LUT:

		ret = tccvin_video_s_lut(vstream, (struct vin_lut *)arg);
		break;

	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}

const struct v4l2_ioctl_ops tccvin_ioctl_ops = {
	.vidioc_querycap = tccvin_ioctl_querycap,
	.vidioc_enum_fmt_vid_cap = tccvin_ioctl_enum_fmt,
	.vidioc_g_fmt_vid_cap_mplane = tccvin_ioctl_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane = tccvin_ioctl_s_fmt,
	.vidioc_try_fmt_vid_cap_mplane = tccvin_ioctl_try_fmt,

	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	/* TODO:
	 * To give physical address to the userspace,
	 * customized querybuf function is used.
	 * Someday, it should be replaced by vb2_ioctl_querybuf.
	 */
	.vidioc_querybuf = tccvin_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,

	.vidioc_enum_input = tccvin_ioctl_enum_input,
	.vidioc_g_input = tccvin_ioctl_g_input,
	.vidioc_s_input = tccvin_ioctl_s_input,
	.vidioc_g_pixelaspect = tccvin_ioctl_g_pixelaspect,
	.vidioc_g_selection = tccvin_ioctl_g_selection,
	.vidioc_s_selection = tccvin_ioctl_s_selection,
	.vidioc_g_parm = tccvin_ioctl_g_parm,
	.vidioc_s_parm = tccvin_ioctl_s_parm,
	.vidioc_enum_framesizes = tccvin_ioctl_enum_framesizes,
	.vidioc_enum_frameintervals = tccvin_ioctl_enum_frameintervals,
	.vidioc_default = tccvin_ioctl_default,
};

int tccvin_v4l2_init_format(struct tccvin_device *tdev)
{
	const struct device *p_dev = NULL;
	struct tccvin_stream *vstream = NULL;
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

	tccvin_print_v4l2_format(vstream, &vstream->format);

	return ret;
}
