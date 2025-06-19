/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "linux/types.h"
#include "media/videobuf2-core.h"
#include "media/videobuf2-v4l2.h"
#include <linux/list.h>
#include <linux/videodev2.h>
#include <linux/atomic.h>
#include <linux/dma-map-ops.h>

#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>

#include <video/tcc-svdw.h>
#include "tcc-svdw.h"

static inline struct tcc_svdw_buffer *vb2_v4l2_buf_to_buf(struct vb2_v4l2_buffer *buf)
{
	return container_of(buf, struct tcc_svdw_buffer, buf);
}

static void tcc_svdw_queue_return_buffers(struct tcc_svdw_queue *queue)
{
	struct tcc_svdw_buffer *buf = NULL;

	while (!(list_empty(&queue->buf_list))) {
		buf = list_first_entry(&queue->buf_list, struct tcc_svdw_buffer, entry);
		list_del(&buf->entry);
		vb2_buffer_done(&buf->buf.vb2_buf, VB2_BUF_STATE_ERROR);
	}
}

static int tcc_svdw_queue_setup(struct vb2_queue *vq, u32 *nbuffers, u32 *nplanes, u32 sizes[],
				struct device *alloc_devs[])
{
	struct tcc_svdw_queue *queue = NULL;
	struct tcc_svdw_device *p_svdw = NULL;
	const struct device *p_dev = NULL;
	struct v4l2_pix_format_mplane pix_mp = {
		0,
	};
	u32 idxPlane = 0;
	int ret = 0;

	queue = (struct tcc_svdw_queue *)vb2_get_drv_priv(vq);
	p_svdw = (struct tcc_svdw_device *)container_of(queue, struct tcc_svdw_device, queue);
	p_dev = svdw_to_dev(p_svdw);

	/* get current pixelformat, width and height */
	(void)memcpy(&pix_mp, &p_svdw->format.fmt.pix_mp, sizeof(pix_mp));

	/* fill other fileds */
	ret = v4l2_fill_pixfmt_mp(&pix_mp, pix_mp.pixelformat, pix_mp.width, pix_mp.height);
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
			logd(p_dev, "plane[%u].sizeimage: %u\n", idxPlane, sizes[idxPlane]);
		}
	}

	return ret;
}

static void tcc_svdw_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = NULL;
	struct tcc_svdw_queue *p_queue = NULL;
	struct tcc_svdw_buffer *buf = NULL;
	unsigned long flags = 0;

	vbuf = to_vb2_v4l2_buffer(vb);
	p_queue = (struct tcc_svdw_queue *)vb2_get_drv_priv(vb->vb2_queue);
	buf = vb2_v4l2_buf_to_buf(vbuf);

	spin_lock_irqsave(&p_queue->slock, flags);
	list_add_tail(&buf->entry, &p_queue->buf_list);
	spin_unlock_irqrestore(&p_queue->slock, flags);
}

static int tcc_svdw_start_streaming(struct vb2_queue *vq, u32 count)
{
	struct tcc_svdw_queue *p_queue = NULL;
	struct tcc_svdw_device *p_svdw = NULL;
	int ret = 0;

	p_queue = (struct tcc_svdw_queue *)vb2_get_drv_priv(vq);
	p_svdw = q2svdw(p_queue);

	ret = media_pipeline_start(&p_svdw->vdev.entity, &p_svdw->tccmd->pipe);
	if (ret < 0) {
		goto error;
	}

	ret = tcc_svdw_video_streamon(p_svdw);
	if (ret != 0) {
		goto error;
	}

	goto ret;

error:
	spin_lock_irq(&p_queue->slock);
	tcc_svdw_queue_return_buffers(p_queue);
	spin_unlock_irq(&p_queue->slock);

ret:
	return ret;
}

static void tcc_svdw_stop_streaming(struct vb2_queue *vq)
{
	struct tcc_svdw_queue *p_queue = NULL;
	struct tcc_svdw_device *p_svdw = NULL;

	p_queue = (struct tcc_svdw_queue *)vb2_get_drv_priv(vq);
	p_svdw = q2svdw(p_queue);

	(void)tcc_svdw_video_streamoff(p_svdw);

	media_pipeline_stop(&p_svdw->vdev.entity);

	spin_lock_irq(&p_queue->slock);
	tcc_svdw_queue_return_buffers(p_queue);
	spin_unlock_irq(&p_queue->slock);
}

static const struct vb2_ops tcc_svdw_qops = {
	.queue_setup = tcc_svdw_queue_setup,
	.buf_queue = tcc_svdw_buf_queue,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.start_streaming = tcc_svdw_start_streaming,
	.stop_streaming = tcc_svdw_stop_streaming,
};

int tcc_svdw_init_vb2_queue(struct tcc_svdw_device *p_svdw, bool drop_corrupted)
{
	struct tcc_svdw_queue *queue;
	struct device *p_dev = NULL;
	struct vb2_queue *q = NULL;
	int ret = 0;

	queue = &p_svdw->queue;
	p_dev = svdw_to_dev(p_svdw);
	q = &queue->queue;

	/* init vb2_queue */
	q->type = (u32)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	q->io_modes = (u32)VB2_MMAP | (u32)VB2_DMABUF;
	q->bidirectional = 0;
	q->is_output = (unsigned char)DMA_TO_DEVICE;
	q->drv_priv = queue;
	q->buf_struct_size = (u32)sizeof(struct tcc_svdw_buffer);
	q->ops = &tcc_svdw_qops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->timestamp_flags =
		(u32)V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC | (u32)V4L2_BUF_FLAG_TSTAMP_SRC_SOE;
	q->dev = &p_svdw->pdev->dev;
	q->lock = &p_svdw->pdev->dev.mutex;
	q->min_buffers_needed = 3;

	ret = vb2_queue_init(q);
	if (ret == 0) {
		spin_lock_init(&queue->slock);
		INIT_LIST_HEAD(&queue->buf_list);
		queue->flags = drop_corrupted ? (u32)TCC_SVDW_QUEUE_DROP_CORRUPTED : 0U;
		logd(p_dev, "drop_corrupted: %d, queue->flags: 0x%08x", drop_corrupted,
		     queue->flags);
	} else {
		loge(p_dev, "Failed to init vb2_queue: %d\n", ret);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(tcc_svdw_init_vb2_queue);

void tcc_svdw_get_dma_addrs(struct tcc_svdw_device *p_svdw, struct vb2_buffer *vb, u32 addrs[])
{
	struct device *p_dev = NULL;
	dma_addr_t dma_addrs[MAX_PLANES];
	u32 idxpln = 0;

	p_dev = svdw_to_dev(p_svdw);

	switch (vb->memory) {
	case (u32)VB2_MEMORY_MMAP:
		for (idxpln = 0; idxpln < vb->num_planes; idxpln++) {
			dma_addrs[idxpln] = vb2_dma_contig_plane_dma_addr(vb, idxpln);
#if defined(CONFIG_ARM64)
			addrs[idxpln] = clamp_t(u32, dma_addrs[idxpln], 0, UINT_MAX);
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
EXPORT_SYMBOL_GPL(tcc_svdw_get_dma_addrs);

void tcc_svdw_print_dma_addrs(struct tcc_svdw_device *p_svdw, const struct vb2_buffer *vb,
			      const u32 addrs[])
{
	const struct device *p_dev = NULL;
	u32 idxpln = 0;

	p_dev = svdw_to_dev(p_svdw);

	(void)addrs;

	switch (vb->memory) {
	case (u32)VB2_MEMORY_MMAP:
		for (idxpln = 0; idxpln < vb->num_planes; idxpln++) {
			/* dma addr */
			trace_printk("planes[%u]: 0x%08x\n", idxpln, addrs[idxpln]);
		}
		break;

	default:
		trace_printk("memory(0x%08x) is not supported\n", vb->memory);
		break;
	}
}
EXPORT_SYMBOL_GPL(tcc_svdw_print_dma_addrs);

static u32 __maybe_unused tcc_svdw_get_mbus_code_by_pixelformat(u32 pixelformat)
{
	struct tcc_svdw_mbus_fmt {
		u32 pixelformat;
		u32 mbus_fmt;
	};
	const struct tcc_svdw_mbus_fmt tcc_svdw_mbus_fmt_list[] = {
		{ .pixelformat = V4L2_PIX_FMT_RGB24, .mbus_fmt = MEDIA_BUS_FMT_RGB888_1X24 },
		{ .pixelformat = V4L2_PIX_FMT_RGB32, .mbus_fmt = MEDIA_BUS_FMT_ARGB8888_1X32 },
		{ .pixelformat = V4L2_PIX_FMT_UYVY, .mbus_fmt = MEDIA_BUS_FMT_UYVY8_1X16 },
		{ .pixelformat = V4L2_PIX_FMT_VYUY, .mbus_fmt = MEDIA_BUS_FMT_VYUY8_1X16 },
		{ .pixelformat = V4L2_PIX_FMT_YUYV, .mbus_fmt = MEDIA_BUS_FMT_YUYV8_1X16 },
		{ .pixelformat = V4L2_PIX_FMT_YVYU, .mbus_fmt = MEDIA_BUS_FMT_YVYU8_1X16 },
		{ .pixelformat = V4L2_PIX_FMT_YUV422P, .mbus_fmt = MEDIA_BUS_FMT_YUYV8_2X8 },
		{ .pixelformat = V4L2_PIX_FMT_NV16, .mbus_fmt = MEDIA_BUS_FMT_YVYU8_2X8 },
		{ .pixelformat = V4L2_PIX_FMT_NV61, .mbus_fmt = MEDIA_BUS_FMT_YVYU8_2X8 },
		{ .pixelformat = V4L2_PIX_FMT_YVU420, .mbus_fmt = MEDIA_BUS_FMT_YVYU8_2X8 },
		{ .pixelformat = V4L2_PIX_FMT_YUV420, .mbus_fmt = MEDIA_BUS_FMT_YUYV8_2X8 },
		{ .pixelformat = V4L2_PIX_FMT_NV12, .mbus_fmt = MEDIA_BUS_FMT_YVYU8_2X8 },
		{ .pixelformat = V4L2_PIX_FMT_NV21, .mbus_fmt = MEDIA_BUS_FMT_YVYU8_2X8 },
	};
	const struct tcc_svdw_mbus_fmt *fotmat = NULL;
	u32 idxList = 0;
	u32 nList = 0;
	u32 mbus_code = 0;

	nList = ARRAY_SIZE(tcc_svdw_mbus_fmt_list);
	for (idxList = 0; idxList < nList; idxList++) {
		fotmat = &tcc_svdw_mbus_fmt_list[idxList];
		if (pixelformat == fotmat->pixelformat) {
			mbus_code = fotmat->mbus_fmt;
			break;
		}
	}

	return mbus_code;
}

static struct v4l2_rect *tcc_svdw_get_rect_by_target(struct tcc_svdw_device *p_svdw, u32 target)
{
	const struct device *p_dev = NULL;
	struct v4l2_rect *p_rect = NULL;

	p_dev = svdw_to_dev(p_svdw);

	switch (target) {
	case V4L2_SEL_TGT_CROP:
	case V4L2_SEL_TGT_CROP_DEFAULT:
		p_rect = &p_svdw->rect_crop;
		break;
	case V4L2_SEL_TGT_COMPOSE:
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
		p_rect = &p_svdw->rect_compose;
		break;
	default:
		loge(p_dev, "target(0x%08x) is not supported\n", target);
		break;
	}

	return p_rect;
}

static void tcc_svdw_print_v4l2_pix_format_mplane(struct tcc_svdw_device *p_svdw,
						  const struct v4l2_pix_format_mplane *format)
{
	const struct device *p_dev = NULL;
	char fcc[4] = {
		0,
	};
	u32 idxPlane = 0;

	p_dev = svdw_to_dev(p_svdw);

	/* convert pixelformat to fcc */
	(void)strscpy(fcc, (const char *)&format->pixelformat, sizeof(format->pixelformat));

	logd(p_dev, "width: %u, height: %u\n", format->width, format->height);
	logd(p_dev, "pixelformat: %c%c%c%c\n", fcc[0], fcc[1], fcc[2], fcc[3]);
	logd(p_dev, "field: %u, colorspace: %u\n", format->field, format->colorspace);
	logd(p_dev, "num_planes: %u\n", format->num_planes);
	for (idxPlane = 0; idxPlane < format->num_planes; idxPlane++) {
		logd(p_dev, " plane_fmt[%u].sizeimage: %u, .bytesperline: %u\n", idxPlane,
		     format->plane_fmt[idxPlane].sizeimage,
		     format->plane_fmt[idxPlane].bytesperline);
	}
}

static void tcc_svdw_print_v4l2_format(struct tcc_svdw_device *p_svdw,
				       const struct v4l2_format *format)
{
	const struct device *p_dev = NULL;

	p_dev = svdw_to_dev(p_svdw);

	switch (format->type) {
	case (u32)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		tcc_svdw_print_v4l2_pix_format_mplane(p_svdw, &format->fmt.pix_mp);
		break;
	default:
		loge(p_dev, "type (0x%08x) is not supported\n", format->type);
		break;
	}
}

static void tcc_svdw_print_v4l2_buffer_mplane(struct tcc_svdw_device *p_svdw,
					      const struct v4l2_buffer *p_buf)
{
	const struct device *p_dev = NULL;
	const struct v4l2_plane *plane = NULL;
	u32 idxpln = 0;

	p_dev = svdw_to_dev(p_svdw);

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
			logd(p_dev, "plane[%u]: 0x%08x\n", idxpln, plane->m.mem_offset);
			break;
		default:
			loge(p_dev, "memory(0x%08x) is not supported\n", p_buf->memory);
			break;
		}
	}
}

static void tcc_svdw_print_v4l2_buffer(struct tcc_svdw_device *p_svdw,
				       const struct v4l2_buffer *buf)
{
	const struct device *p_dev = NULL;

	p_dev = svdw_to_dev(p_svdw);

	switch (buf->type) {
	case (u32)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		tcc_svdw_print_v4l2_buffer_mplane(p_svdw, buf);
		break;
	default:
		loge(p_dev, "type(0x%08x) is not supported\n", buf->type);
		break;
	}
}

static int tcc_svdw_fop_open(struct file *filp)
{
	int ret = 0;
	/* struct video_device *vdev = video_devdata(filp); */

	atomic_add(-1 * TCC_SVDW_DEWARP_MAX, &tcc_svdw_dewarp_cnt);
	ret = v4l2_fh_open(filp);
	// filp->private_data = vdev->queue->owner;

	return ret;
}

static int tcc_svdw_fop_release(struct file *file)
{
	struct video_device *vdev = video_devdata(file);
	struct tcc_svdw_device *p_svdw =
		(struct tcc_svdw_device *)container_of(vdev, struct tcc_svdw_device, vdev);

	atomic_add(TCC_SVDW_DEWARP_MAX, &tcc_svdw_dewarp_cnt);

	/* mask interrupts for unexpected close */
	svdw_set_ireq_mask(&p_svdw->pdev->dev, 1);
	vb2_queue_release(&p_svdw->queue.queue);

	return vb2_fop_release(file);
}

const struct v4l2_file_operations tcc_svdw_fops = {
	.owner = THIS_MODULE,
	.open = tcc_svdw_fop_open,
	.release = tcc_svdw_fop_release,
	.unlocked_ioctl = video_ioctl2,
	.mmap = vb2_fop_mmap,
	.poll = vb2_fop_poll,
};
EXPORT_SYMBOL_GPL(tcc_svdw_fops);

static int tcc_svdw_ioctl_querycap(struct file *pfile, void *fh, struct v4l2_capability *cap)
{
	const struct device *p_dev = NULL;
	struct video_device *vdev = NULL;
	struct tcc_svdw_device *p_svdw = NULL;
	int ret = 0;
	int dewarp_cnt = 0;

	p_svdw = video_drvdata(pfile);
	p_dev = svdw_to_dev(p_svdw);
	vdev = &p_svdw->vdev;

	(void)scnprintf((char *)cap->driver, PAGE_SIZE, "%s", KBUILD_MODNAME);
	(void)scnprintf((char *)cap->card, PAGE_SIZE, "%s", vdev->name);
	(void)scnprintf((char *)cap->bus_info, PAGE_SIZE, "platform:videoinput:%d", vdev->num);
	cap->version = LINUX_VERSION_CODE;
	cap->capabilities = V4L2_CAP_DEVICE_CAPS | cap->device_caps;

	logd(p_dev, "driver: %s\n", cap->driver);
	logd(p_dev, "card: %s\n", cap->card);
	logd(p_dev, "bus_info: %s\n", cap->bus_info);
	logd(p_dev, "version: %u.%u.%u\n", (cap->version >> 16) & 0xFFU,
	     (cap->version >> 8) & 0xFFU, (cap->version >> 0) & 0xFFU);
	logd(p_dev, "device_caps: 0x%08x\n", cap->device_caps);
	logd(p_dev, "capabilities: 0x%08x\n", cap->capabilities);
	dewarp_cnt = atomic_read(&tcc_svdw_dewarp_cnt);

	if (dewarp_cnt != 0) {
		loge(p_dev, "Other devices are using dewarp resources: %d\n", dewarp_cnt);
		ret = -EBUSY;
	}

	return ret;
}

static int tcc_svdw_ioctl_enum_fmt(struct file *pfile, void *fh, struct v4l2_fmtdesc *fmt)
{
	const struct device *p_dev = NULL;
	struct tcc_svdw_device *p_svdw = NULL;
	u32 index = 0;
	u32 pixelformat = 0;
	int ret = 0;

	p_svdw = video_drvdata(pfile);
	p_dev = svdw_to_dev(p_svdw);

	pixelformat = tcc_svdw_video_get_pixelformat_by_index(p_svdw, fmt->index);
	if (pixelformat == 0U) {
		logd(p_dev, "format of index(%u) is not supported\n", fmt->index);
		ret = -EINVAL;
	} else {
		index = fmt->index;

		(void)memset(fmt, 0, sizeof(*fmt));

		fmt->index = index;
		fmt->type = (u32)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		fmt->pixelformat = pixelformat;
		(void)strscpy((char *)fmt->description, (const char *)&fmt->pixelformat,
			      sizeof(fmt->pixelformat));

		logd(p_dev, "index: %u\n", fmt->index);
		logd(p_dev, "type: 0x%08x\n", fmt->type);
		logd(p_dev, "flags: 0x%08x\n", fmt->flags);
		logd(p_dev, "description: %s\n", fmt->description);
	}

	return ret;
}

static int tcc_svdw_ioctl_g_fmt(struct file *pfile, void *fh, struct v4l2_format *fmt)
{
	struct tcc_svdw_device *p_svdw = NULL;
	int ret = 0;

	p_svdw = video_drvdata(pfile);

	*fmt = p_svdw->format;

	return ret;
}

static int tcc_svdw_ioctl_s_fmt(struct file *pfile, void *fh, struct v4l2_format *fmt)
{
	struct tcc_svdw_device *p_svdw = NULL;
	const struct device *p_dev = NULL;
	struct v4l2_pix_format_mplane *pix_mp = NULL;
	bool ret_bool = false;
	int ret = 0;

	p_svdw = video_drvdata(pfile);
	p_dev = svdw_to_dev(p_svdw);

	ret_bool = tcc_svdw_video_is_format_supported(p_svdw, fmt);
	if (!ret_bool) {
		loge(p_dev, "format is not supported\n");
		ret = -EINVAL;
	} else {
		mutex_lock(&p_svdw->mlock);
		if (vb2_is_busy(&p_svdw->queue.queue)) {
			loge(p_dev, "vb2_is_busy\n");
		} else {
			(void)memcpy(&p_svdw->format, fmt, sizeof(*fmt));

			/* additional set */
			pix_mp = &p_svdw->format.fmt.pix_mp;

			ret = v4l2_fill_pixfmt_mp(pix_mp, pix_mp->pixelformat, pix_mp->width,
						  pix_mp->height);
			if (ret < 0) {
				loge(p_dev, "Failed on v4l2_fill_pixfmt_mp\n");
			} else {
				pix_mp->field = (u32)V4L2_FIELD_NONE;
				pix_mp->colorspace = (u32)V4L2_COLORSPACE_SRGB;
			}
		}
		mutex_unlock(&p_svdw->mlock);

		/* Broadcasting s_fmt configuration to odw */
		tcc_svdw_s_fmt_dewarpers(p_svdw);
	}

	return ret;
}

static int tcc_svdw_ioctl_try_fmt(struct file *pfile, void *fh, struct v4l2_format *fmt)
{
	struct tcc_svdw_device *p_svdw = NULL;
	const struct device *p_dev = NULL;
	bool ret_bool = false;
	int ret = 0;

	p_svdw = video_drvdata(pfile);
	p_dev = svdw_to_dev(p_svdw);

	ret_bool = tcc_svdw_video_is_format_supported(p_svdw, fmt);
	if (!ret_bool) {
		tcc_svdw_print_v4l2_format(p_svdw, fmt);

		loge(p_dev, "format is not supported\n");
		ret = -EINVAL;
	}

	return ret;
}

static int tcc_svdw_ioctl_reqbufs(struct file *pfile, void *fh, struct v4l2_requestbuffers *rb)
{
	struct tcc_svdw_device *p_svdw = NULL;
	const struct device *p_dev = NULL;
	phys_addr_t addr = 0U;
	int ret = 0;
	int idx = 0;
	p_svdw = video_drvdata(pfile);
	p_dev = svdw_to_dev(p_svdw);

	ret = vb2_reqbufs(&p_svdw->queue.queue, rb);
	if (ret >= 0) {
		logd(p_dev, "count: %d\n", rb->count);
		logd(p_dev, "type: 0x%08x\n", rb->type);
		logd(p_dev, "memory: 0x%08x\n", rb->memory);
	} else {
		loge(p_dev, "Failed on vb2_reqbufs: %d\n", ret);
		goto done;
	}

	ret = tcc_svdw_g_patch_addresses(p_svdw);
	if (ret < 0) {
		loge(p_dev, "Failed on tcc_svdw_g_patch_addresses: %d\n", ret);
		goto done;
	}

	for (idx = 0; idx < TCC_SVDW_PADS_MAX; idx++) {
		logd(p_dev, "patch address[%d] = 0x%llx\n", idx, p_svdw->patch_addrs[idx]);

		if (addr == p_svdw->patch_addrs[idx]) {
			loge(p_dev, "The wrong configuration with memblock (pmap_*)!");
			ret = -EINVAL;
			break;
		}
		addr = p_svdw->patch_addrs[idx];
	}
done:
	return ret;
}

static int tcc_svdw_ioctl_querybuf(struct file *pfile, void *fh, struct v4l2_buffer *p_buf)
{
	struct tcc_svdw_device *p_svdw = NULL;
	const struct device *p_dev = NULL;
	struct vb2_buffer *vb = NULL;
	u32 dma_addrs[MAX_PLANES];
	u32 idxpln = 0;
	int ret = 0;

	p_svdw = video_drvdata(pfile);
	p_dev = svdw_to_dev(p_svdw);

	ret = vb2_querybuf(&p_svdw->queue.queue, p_buf);
	if (ret < 0) {
		loge(p_dev, "vb2_querybuf, ret: %d\n", ret);
		ret = -EINVAL;
	} else {
		/* print for debug */
		tcc_svdw_print_v4l2_buffer(p_svdw, p_buf);

		vb = p_svdw->queue.queue.bufs[p_buf->index];
		(void)memset(dma_addrs, 0, sizeof(dma_addrs));
		tcc_svdw_get_dma_addrs(p_svdw, vb, dma_addrs);
		tcc_svdw_print_dma_addrs(p_svdw, vb, dma_addrs);

		for (idxpln = 0; idxpln < p_buf->length; idxpln++) {
			/* provid dma addrs */
			p_buf->m.planes[idxpln].reserved[0] = dma_addrs[idxpln];
		}
	}

	return ret;
}

static int tcc_svdw_ioctl_qbuf(struct file *pfile, void *fh, struct v4l2_buffer *p_buf)
{
	struct tcc_svdw_device *p_svdw = NULL;
	const struct device *p_dev = NULL;
	int ret = 0;

	p_svdw = video_drvdata(pfile);
	p_dev = svdw_to_dev(p_svdw);

	/* device is available */
	ret = vb2_qbuf(&p_svdw->queue.queue, p_svdw->vdev.v4l2_dev->mdev, p_buf);
	if (ret < 0) {
		loge(p_dev, "vb2_qbuf, ret: %d\n", ret);
		ret = -EINVAL;
	}

	return ret;
}

static int tcc_svdw_ioctl_expbuf(struct file *pfile, void *fh, struct v4l2_exportbuffer *p_buf)
{
	struct tcc_svdw_device *p_svdw = NULL;
	const struct device *p_dev = NULL;
	int ret = 0;

	p_svdw = video_drvdata(pfile);
	p_dev = svdw_to_dev(p_svdw);

	ret = vb2_expbuf(&p_svdw->queue.queue, p_buf);
	if (ret < 0) {
		loge(p_dev, "vb2_expbuf, ret: %d\n", ret);
		ret = -EINVAL;
	}

	return ret;
}

static int tcc_svdw_ioctl_dqbuf(struct file *pfile, void *fh, struct v4l2_buffer *p_buf)
{
	struct tcc_svdw_device *p_svdw = NULL;
	const struct device *p_dev = NULL;
	bool non_block = false;
	u32 idxpln = 0;
	int ret = 0;

	p_svdw = video_drvdata(pfile);
	p_dev = svdw_to_dev(p_svdw);

	non_block = ((pfile->f_flags & (u32)O_NONBLOCK) != 0U);
	ret = vb2_dqbuf(&p_svdw->queue.queue, p_buf, non_block);
	if (ret < 0) {
		loge(p_dev, "vb2_dqbuf, ret: %d\n", ret);
		ret = -EINVAL;
	} else {
		for (idxpln = 0; idxpln < p_buf->length; idxpln++) {
			/* set bytesused to inform the data size */
			p_buf->m.planes[idxpln].bytesused =
				p_svdw->format.fmt.pix_mp.plane_fmt[idxpln].sizeimage;
		}

		/* print for debug */
		tcc_svdw_print_v4l2_buffer(p_svdw, p_buf);
	}

	return ret;
}

static int tcc_svdw_ioctl_streamon(struct file *pfile, void *fh, enum v4l2_buf_type type)
{
	struct tcc_svdw_device *p_svdw = NULL;
	const struct device *p_dev = NULL;
	int ret = 0;

	p_svdw = video_drvdata(pfile);
	p_dev = svdw_to_dev(p_svdw);

	mutex_lock(&p_svdw->mlock);
	ret = vb2_streamon(&p_svdw->queue.queue, type);
	mutex_unlock(&p_svdw->mlock);
	if (ret < 0) {
		loge(p_dev, "vb2_streamon, ret: %d\n", ret);
		ret = -EINVAL;
	}

	return ret;
}

static int tcc_svdw_ioctl_streamoff(struct file *pfile, void *fh, enum v4l2_buf_type type)
{
	struct tcc_svdw_device *p_svdw = NULL;
	const struct device *p_dev = NULL;
	int ret = 0;

	p_svdw = video_drvdata(pfile);
	p_dev = svdw_to_dev(p_svdw);

	mutex_lock(&p_svdw->mlock);
	ret = vb2_streamoff(&p_svdw->queue.queue, type);
	mutex_unlock(&p_svdw->mlock);
	if (ret < 0) {
		loge(p_dev, "vb2_streamoff, ret: %d\n", ret);
		ret = -EINVAL;
	}

	return ret;
}

static int tcc_svdw_ioctl_enum_input(struct file *pfile, void *fh, struct v4l2_input *input)
{
	struct tcc_svdw_device *p_svdw = NULL;
	const struct device *p_dev = NULL;
	u32 index = 0;
	int ret = 0;

	p_svdw = video_drvdata(pfile);
	p_dev = svdw_to_dev(p_svdw);

	index = input->index;
	if (index != 0U) {
		loge(p_dev, "input %d is not supported\n", index);
		ret = -EINVAL;
	} else {
		(void)memset(input, 0, sizeof(*input));

		input->index = index;
		(void)scnprintf((char *)input->name, PAGE_SIZE, "v4l2 subdev[%u]", index);
		input->type = V4L2_INPUT_TYPE_CAMERA;

		// TODO enum_input

		switch (ret) {
		case -ENODEV:
			loge(p_dev, "%s - subdev is null\n", input->name);
			break;
		case -ENOIOCTLCMD:
			logd(p_dev, "%s - video.g_input_status is not supported\n", input->name);
			ret = -ENOTTY;
			break;
		default:
			logd(p_dev, "%s - status: 0x%08x\n", input->name, input->status);
			break;
		}
	}

	return ret;
}

static int tcc_svdw_ioctl_g_input(struct file *pfile, void *fh, u32 *input)
{
	int ret = 0;

	/* support 0th input only */
	*input = 0;

	return ret;
}

static int tcc_svdw_ioctl_s_input(struct file *pfile, void *fh, u32 input)
{
	struct tcc_svdw_device *p_svdw = NULL;
	const struct device *p_dev = NULL;
	int ret = 0;

	p_svdw = video_drvdata(pfile);
	p_dev = svdw_to_dev(p_svdw);

	/* support 0th input only */
	if (input != 0U) {
		loge(p_dev, "input %d is not supported\n", input);
		ret = -EINVAL;
	}

	return ret;
}

static int tcc_svdw_ioctl_g_pixelaspect(struct file *pfile, void *fh, int buf_type,
					struct v4l2_fract *aspect)
{
	struct tcc_svdw_device *p_svdw = NULL;
	const struct device *p_dev = NULL;
	int ret = 0;

	p_svdw = video_drvdata(pfile);
	p_dev = svdw_to_dev(p_svdw);

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

static int tcc_svdw_ioctl_g_selection(struct file *pfile, void *fh, struct v4l2_selection *s)
{
	struct tcc_svdw_device *p_svdw = NULL;
	const struct v4l2_rect *t_rect = NULL;
	int ret = 0;

	p_svdw = video_drvdata(pfile);

	t_rect = tcc_svdw_get_rect_by_target(p_svdw, s->target);
	if (t_rect == NULL) {
		/* error: tcc_svdw_get_rect_by_target */
		ret = -EINVAL;
	} else {
		/* get target's rect info */
		(void)memcpy(&s->r, t_rect, sizeof(*t_rect));
	}

	return ret;
}

static int tcc_svdw_ioctl_s_selection(struct file *pfile, void *fh, struct v4l2_selection *s)
{
	struct tcc_svdw_device *p_svdw = NULL;
	struct v4l2_rect *t_rect = NULL;
	int ret = 0;

	p_svdw = video_drvdata(pfile);

	t_rect = tcc_svdw_get_rect_by_target(p_svdw, s->target);
	if (t_rect == NULL) {
		/* error: tcc_svdw_get_rect_by_target */
		ret = -EINVAL;
	} else {
		/* set target's rect info */
		(void)memcpy(t_rect, &s->r, sizeof(*t_rect));
	}

	return ret;
}

static int tcc_svdw_ioctl_g_parm(struct file *pfile, void *fh, struct v4l2_streamparm *a)
{
	const struct device *p_dev = NULL;
	struct tcc_svdw_device *p_svdw = NULL;

	p_svdw = video_drvdata(pfile);
	p_dev = svdw_to_dev(p_svdw);

	// TODO

	return 0;
}

static int tcc_svdw_ioctl_s_parm(struct file *pfile, void *fh, struct v4l2_streamparm *a)
{
	const struct device *p_dev = NULL;
	struct tcc_svdw_device *p_svdw = NULL;
	struct v4l2_captureparm *capture = NULL;
	struct v4l2_subdev_frame_interval t_interval = {
		0,
	};
	struct v4l2_fract *timeperframe = NULL;

	p_svdw = video_drvdata(pfile);
	p_dev = svdw_to_dev(p_svdw);

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

	if ((timeperframe->numerator == 0U) || (timeperframe->denominator == 0U)) {
		timeperframe->numerator = 1;
		timeperframe->denominator = 1;
	}

	logd(p_dev, "framerate: %u / %u\n", timeperframe->numerator, timeperframe->denominator);

	return 0;
}

/*
 * framesize depends on capabilities of video-input path, not video sources
 * because wdma can even save video data with black video data
 * framesize is bigger than video source's supported one and video-input path doesn't have any scaler.
 */
static int tcc_svdw_ioctl_enum_framesizes(struct file *pfile, void *fh,
					  struct v4l2_frmsizeenum *fsize)
{
	struct tcc_svdw_device *p_svdw = NULL;
	const struct device *p_dev = NULL;
	bool ret_bool = true;
	int ret = 0;

	p_svdw = video_drvdata(pfile);
	p_dev = svdw_to_dev(p_svdw);

	if (fsize->index != 0U) {
		loge(p_dev, "index(%u) is not 0\n", fsize->index);
		ret = -EINVAL;
	}

	ret_bool = tcc_svdw_video_is_pixelformat_supported(p_svdw, fsize->pixel_format);
	if (!ret_bool) {
		loge(p_dev, "tcc_svdw_video_is_pixelformat_supported\n");
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

static long tcc_svdw_ioctl_default(struct file *pfile, void *fh, bool valid_prio, u32 cmd,
				   void *arg)
{
	struct tcc_svdw_device *p_svdw = NULL;
	int ret = 0;

	p_svdw = video_drvdata(pfile);
	return ret;
}

const struct v4l2_ioctl_ops tcc_svdw_ioctl_ops = {
	.vidioc_querycap = tcc_svdw_ioctl_querycap,
	.vidioc_enum_fmt_vid_cap = tcc_svdw_ioctl_enum_fmt,
	.vidioc_g_fmt_vid_cap_mplane = tcc_svdw_ioctl_g_fmt,
	.vidioc_s_fmt_vid_cap_mplane = tcc_svdw_ioctl_s_fmt,
	.vidioc_try_fmt_vid_cap_mplane = tcc_svdw_ioctl_try_fmt,
	.vidioc_reqbufs = tcc_svdw_ioctl_reqbufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	.vidioc_querybuf = tcc_svdw_ioctl_querybuf,
	.vidioc_qbuf = tcc_svdw_ioctl_qbuf,
	.vidioc_expbuf = tcc_svdw_ioctl_expbuf,
	.vidioc_dqbuf = tcc_svdw_ioctl_dqbuf,
	.vidioc_streamon = tcc_svdw_ioctl_streamon,
	.vidioc_streamoff = tcc_svdw_ioctl_streamoff,
	.vidioc_enum_input = tcc_svdw_ioctl_enum_input,
	.vidioc_g_input = tcc_svdw_ioctl_g_input,
	.vidioc_s_input = tcc_svdw_ioctl_s_input,
	.vidioc_g_pixelaspect = tcc_svdw_ioctl_g_pixelaspect,
	.vidioc_g_selection = tcc_svdw_ioctl_g_selection,
	.vidioc_s_selection = tcc_svdw_ioctl_s_selection,
	.vidioc_g_parm = tcc_svdw_ioctl_g_parm,
	.vidioc_s_parm = tcc_svdw_ioctl_s_parm,
	.vidioc_enum_framesizes = tcc_svdw_ioctl_enum_framesizes,
	.vidioc_default = tcc_svdw_ioctl_default,
};
EXPORT_SYMBOL_GPL(tcc_svdw_ioctl_ops);

int tcc_svdw_init_v4l2_fmt(struct tcc_svdw_device *p_svdw)
{
	const struct device *p_dev = NULL;
	struct v4l2_format *format = NULL;
	struct v4l2_pix_format_mplane *pix_mp = NULL;
	int ret = 0;

	p_dev = svdw_to_dev(p_svdw);

	format = &p_svdw->format;
	format->type = (u32)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	pix_mp = &format->fmt.pix_mp;
	pix_mp->pixelformat = (u32)V4L2_PIX_FMT_RGB24;
	pix_mp->width = DEFAULT_FRAMEWIDTH;
	pix_mp->height = DEFAULT_FRAMEHEIGHT;
	pix_mp->field = (u32)V4L2_FIELD_NONE;
	pix_mp->colorspace = (u32)V4L2_COLORSPACE_SRGB;

	ret = v4l2_fill_pixfmt_mp(pix_mp, pix_mp->pixelformat, pix_mp->width, pix_mp->height);
	if (ret < 0) {
		loge(p_dev, "Failed on v4l2_fill_pixfmt_mp\n");
	} else {
		logd(p_dev, "Succeed on v4l2_fill_pixfmt_mp\n");
	}

	tcc_svdw_print_v4l2_format(p_svdw, &p_svdw->format);

	return ret;
}
EXPORT_SYMBOL_GPL(tcc_svdw_init_v4l2_fmt);

void tcc_svdw_unregister_video_device(struct tcc_svdw_device *p_svdw)
{
	if (video_is_registered(&p_svdw->vdev) != 0) {
		/* unregister video device  */
		video_unregister_device(&p_svdw->vdev);
	}
}
EXPORT_SYMBOL(tcc_svdw_unregister_video_device);
