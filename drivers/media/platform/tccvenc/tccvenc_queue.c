#include "tccvenc_queue.h"

int tccvenc_queue_init(void *priv, struct vb2_queue *src_vq,
                                    struct vb2_queue *dst_vq)
{
	struct tcc_venc_ctx *ctx = priv;
	struct device *dev = &ctx->venc_dev->plat_dev->dev;
	int ret;

	tcvenc_step("queue init begin: ctx=%p, dev=%p", ctx, dev);

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	src_vq->drv_priv = ctx;
	src_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	src_vq->ops = &tccvenc_vb2_ops;
	src_vq->mem_ops = &vb2_dma_contig_memops;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &ctx->venc_dev->dev_mutex;
	src_vq->allow_zero_bytesused = 1;
	src_vq->min_buffers_needed = 0;
	src_vq->dev = dev;

	tcvenc_step("src_vq set: type=%u, io_modes=0x%x, buf_struct_size=%u",
		src_vq->type, src_vq->io_modes, src_vq->buf_struct_size);

	*dst_vq = *src_vq;
	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	tcvenc_step("dst_vq copied: type=%u", dst_vq->type);

	ret = vb2_queue_init(src_vq);
	if (ret) {
		tcvenc_err("vb2_queue_init(src_vq) failed: %d", ret);
		return ret;
	}

	ret = vb2_queue_init(dst_vq);
	if (ret) {
		tcvenc_err("vb2_queue_init(dst_vq) failed: %d", ret);
		return ret;
	}

	tcvenc_step("queue init done");

	return 0;
}

static int tccvenc_queue_setup(struct vb2_queue *vq,
		unsigned int *nbuffers, unsigned int *nplanes,
		unsigned int sizes[], struct device *alloc_devs[])
{
	struct tcc_venc_ctx *ctx = vb2_get_drv_priv(vq);
	struct v4l2_pix_format_mplane *fmt;

	unsigned int i;
    const char *qname = (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) ? "OUTPUT" : "CAPTURE";

	tcvenc_step("[%s] queue_setup: nbuffers=%u", qname, *nbuffers);

	if (!ctx || !ctx->src_fmt.num_planes) {
		tcvenc_err("ctx or format not properly initialized");
		return -EINVAL;
	}

	if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		fmt = &ctx->src_fmt;
	else
		fmt = &ctx->dst_fmt;

	if (*nplanes) {
		for (i = 0; i < *nplanes; i++) {
            tcvenc_step("[%s] plane%u: user_size=%u  expected=%u  bpl=%u",
                        qname, i, sizes[i],
                        fmt->plane_fmt[i].sizeimage,
                        fmt->plane_fmt[i].bytesperline);
			if (sizes[i] < fmt->plane_fmt[i].sizeimage)
				return -EINVAL;
		}
	} else {
		*nplanes = fmt->num_planes;
		for (i = 0; i < *nplanes; i++) {
			sizes[i] = fmt->plane_fmt[i].sizeimage;
            tcvenc_step("[%s] plane%u: alloc_size=%u  bpl=%u",
                        qname, i, sizes[i],
                        fmt->plane_fmt[i].bytesperline);
		}
	}

	return 0;
}

static int tccvenc_buf_prepare(struct vb2_buffer *vb)
{
	struct tcc_venc_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct v4l2_pix_format_mplane *fmt;

	if (!vb || !vb->planes) {
    	tcvenc_err("expbuf: invalid vb2_buffer or planes\n");
    	return -EINVAL;
	}

	if (!vb->vb2_queue || !vb->vb2_queue->bufs) {
		tcvenc_err("prepare: queue not initialized (REQBUFS missing?)\n");
		return -EINVAL;
	}

	fmt = (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) ?
			&ctx->src_fmt : &ctx->dst_fmt;

	if (vb2_plane_size(vb, 0) < fmt->plane_fmt[0].sizeimage)
		return -EINVAL;

	return 0;
}

static void tccvenc_buf_queue(struct vb2_buffer *vb)
{
	struct tcc_venc_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vbuf = container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
	enum v4l2_buf_type type = vb->vb2_queue->type;

	tcvenc_step("queue buffer: ctx=%p, type=%u, index=%u, bytesused=%u",
            ctx, type, vbuf->vb2_buf.index, vbuf->planes[0].bytesused);

	v4l2_m2m_buf_queue(ctx->m2m_ctx, vbuf);
}

static int tccvenc_start_streaming(struct vb2_queue *q, unsigned int count)
{
	tcvenc_step("");
	return 0;
}

static void tccvenc_stop_streaming(struct vb2_queue *q)
{
	struct tcc_venc_ctx *ctx = vb2_get_drv_priv(q);
	struct vb2_v4l2_buffer *buf;
	unsigned int i;

	if (ctx->venc_dev && ctx->venc_dev->workqueue)
		flush_workqueue(ctx->venc_dev->workqueue);

	if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		while ((buf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx)))
			v4l2_m2m_buf_done(buf, VB2_BUF_STATE_ERROR);
	} else if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		while ((buf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx)))
			v4l2_m2m_buf_done(buf, VB2_BUF_STATE_ERROR);
	}

	for (i = 0; i < q->num_buffers; i++) {
		struct vb2_buffer *vb = q->bufs[i];

		if (vb && vb->state == VB2_BUF_STATE_ACTIVE) {
			tcvenc_step("force done: index=%d state=ACTIVE", vb->index);
			vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
		}
	}
}

const struct vb2_ops tccvenc_vb2_ops = {
	.queue_setup		= tccvenc_queue_setup,
	.buf_prepare		= tccvenc_buf_prepare,
	.buf_queue		    = tccvenc_buf_queue,
	.start_streaming	= tccvenc_start_streaming,
	.stop_streaming		= tccvenc_stop_streaming,
	.wait_prepare		= vb2_ops_wait_prepare,
	.wait_finish		= vb2_ops_wait_finish,
};
