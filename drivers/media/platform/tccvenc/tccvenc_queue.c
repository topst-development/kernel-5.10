#include "tccvenc_queue.h"

struct res_bitrate_entry {
	u32 pixels;      // width * height
	u32 bitrate_kbps;
};

static const struct res_bitrate_entry bitrate_table[] = {
	{  720 *  480,  2 * 1024 * 1024 },  // 2 Mbps
	{ 1280 *  720,  6 * 1024 * 1024 },  // 6 Mbps
	{ 1920 * 1080, 10 * 1024 * 1024 },  // 10 Mbps
	{ 7680 * 4320, 10 * 1024 * 1024 },  // 10 Mbps
};

static u32 get_default_bitrate_kbps(u32 width, u32 height);

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
	int ret;
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

	if (ctx->venc_handle &&
	    ctx->src_fmt.width && ctx->src_fmt.height &&
	    ctx->dst_fmt.pixelformat && 
		!ctx->initialied_enc) {
		u32 bandwidth = 0;

		venc_init_t init = { 0 };
		
		switch (ctx->dst_fmt.pixelformat) {
		case V4L2_PIX_FMT_H264:
			init.codec = VCODEC_ID_AVC;
			break;
		case V4L2_PIX_FMT_HEVC:
			init.codec = VCODEC_ID_HEVC;
			break;
		default:
			tcvenc_err("Unsupported codec format: 0x%08x\n", ctx->dst_fmt.pixelformat);
			return -EINVAL;
		}

		switch (ctx->src_fmt.pixelformat) {
		case V4L2_PIX_FMT_NV12:
			init.source_format = VENC_SOURCE_NV12;
			break;
		case V4L2_PIX_FMT_YUV420:
			init.source_format = VENC_SOURCE_YUV420P;
			break;
		default:
			tcvenc_err("Unsupported input format: 0x%08x\n", ctx->src_fmt.pixelformat);
			return -EINVAL;
		}

		init.pic_width 		= ctx->src_fmt.width;
		init.pic_height 	= ctx->src_fmt.height;
		
		init.framerate 		= ctx->framerate ? ctx->framerate : 30; 

		init.bitrateKbps = ctx->bitrate ?
                   ctx->bitrate :
                   get_default_bitrate_kbps(ctx->src_fmt.width, ctx->src_fmt.height);
		init.bitrateKbps = init.bitrateKbps / 1024; //Convert bps to Kbps
		init.key_interval 	= ctx->framerate;

		bandwidth = init.pic_width * init.pic_height * init.framerate;

		//tcvenc_info("width: %u, height: %u, framerate: %u, bandwidth: %u", init.pic_width, init.pic_height, init.framerate, bandwidth);
		
		switch (ctx->dst_fmt.pixelformat) {
			case V4L2_PIX_FMT_H264:
				init.slice_mode = 0;
				init.slice_size_mode = 0;
				init.slice_size = 1024 * 4;
				if (bandwidth > MAX_BANDWIDTH_H264) {
					tcvenc_err("bandwidth is too high for H264 Max resolution: %ux%u (fps: %u)"
						, MAX_WIDTH_H264, MAX_HEIGHT_H264, MAX_FRAMERATE_H264);
					return -EINVAL;
				}
				break;
			case V4L2_PIX_FMT_HEVC:
				init.slice_mode = 0;
				init.slice_size_mode = 0;
				init.slice_size = 0;
				if (bandwidth > MAX_BANDWIDTH_HEVC) {
					tcvenc_err("bandwidth is too high for HEVC Max resolution: %ux%u (fps: %u)"
						, MAX_WIDTH_HEVC, MAX_HEIGHT_HEVC, MAX_FRAMERATE_HEVC);
					return -EINVAL;
				}
				break;
			default:
				tcvenc_err("Unsupported input format: 0x%08x\n", ctx->src_fmt.pixelformat);
				return -EINVAL;
		}
		
		ret = venc_init(ctx->venc_handle, &init);

		if (ret < 0) {
			tcvenc_err("venc_init failed\n");
			return -EINVAL;
		}
		tcvenc_info("venc_init_t config:");
		tcvenc_info("  codec         = %d", init.codec);
		tcvenc_info("  source_format = %d", init.source_format);
		tcvenc_info("  width x height= %u x %u", init.pic_width, init.pic_height);
		tcvenc_info("  framerate     = %u", init.framerate);
		tcvenc_info("  bitrateKbps   = %u", init.bitrateKbps);
		tcvenc_info("  key_interval  = %u", init.key_interval);
		tcvenc_info("  slice_mode    = %u", init.slice_mode);
		tcvenc_info("  slice_size_mode = %u", init.slice_size_mode);
		tcvenc_info("  slice_size    = %u", init.slice_size);

		ctx->initialied_enc = true;
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

static u32 get_default_bitrate_kbps(u32 width, u32 height)
{
	u32 pixels = width * height;
	u32 best_diff = ~0;
	u32 selected_bps = 4 * 1000000;  // fallback = 4 Mbps
	int i;

	for (i = 0; i < ARRAY_SIZE(bitrate_table); i++) {
		u32 diff = abs((int)pixels - (int)bitrate_table[i].pixels);
		if (diff < best_diff) {
			best_diff = diff;
			selected_bps = bitrate_table[i].bitrate_kbps;
		}
	}

	return selected_bps;
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
