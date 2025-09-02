/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/of_reserved_mem.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <asm/unaligned.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-mem2mem.h>
#include <linux/dma-direct.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-common.h>
#include <linux/dma-direct.h>

#include "tccvdec.h"
#include "tccvdec_debug.h"
#include "tccvdec_core.h"
#include "tccvdec_ctrls.h"

#include <linux/fs.h>
#include <linux/uaccess.h>

#define to_tcvdec_buffer(ptr)	container_of(ptr, struct tcc_vdec_buffer, m2m_buf.vb)
static int tcc_vdec_open(struct file *file);
static int tcc_vdec_release(struct file *file);
static void tcc_vdec_m2m_job_abort(void *ctx);
static void tcc_vdec_m2m_device_run(void *priv);
static int tcc_vdec_probe(struct platform_device *pdev);
static int tcc_vdec_remove(struct platform_device *pdev);
static int tcc_vdec_queue_setup(struct vb2_queue *vq,
			unsigned int *num_buffers, unsigned int *num_planes,
			unsigned int sizes[], struct device *allocators[]);
static int tcc_vdec_start_streaming(struct vb2_queue *q, unsigned int count);
static void tcc_vdec_stop_streaming(struct vb2_queue *q);
static void tcc_vb2_buf_queue(struct vb2_buffer *vb);
static int tcc_vdec_vb2_buf_prepare(struct vb2_buffer *vb);
static int tcc_vdec_vb2_buf_init(struct vb2_buffer *vb);
static void tcc_vdec_buf_cleanup(struct vb2_buffer *vb);

static int tcc_vdec_querycap(struct file *file, void *priv, struct v4l2_capability *cap);
static int tcc_vdec_enum_fmt(struct file *file, void *pirv, struct v4l2_fmtdesc *f);
static int tcc_vdec_s_fmt(struct file *file, void *priv, struct v4l2_format *f);
static int tcc_vdec_g_fmt(struct file *file, void *fh, struct v4l2_format *f);
static int tcc_vdec_try_fmt(struct file *file, void *fh, struct v4l2_format *f);
static int tcc_vdec_g_selection(struct file *file, void *fh, struct v4l2_selection *s);
static int tcc_vdec_s_parm(struct file *file, void *fh, struct v4l2_streamparm *a);
static int tcc_vdec_g_parm(struct file *file, void *fh, struct v4l2_streamparm *a);
static int tcc_vdec_enum_framesizes(struct file *file, void *fh, struct v4l2_frmsizeenum *fsize);
static int tcc_vdec_subscribe_event(struct v4l2_fh *fh, const struct v4l2_event_subscription *sub);
static int tcc_vdec_decoder_cmd(struct file *file, void *fh, struct v4l2_decoder_cmd *cmd);

static uint32_t tcc_vdec_get_framesize_raw(struct tcc_vdec_ctx *ctx, uint32_t v4l2_fmt, uint32_t width,
	uint32_t height, uint32_t plane_idx, uint32_t planes);
static uint32_t tccvdec_get_output_size(uint32_t v4l2_fmt);
static void tcvdec_worker(struct work_struct *work);

/* util functions */
static void clear_used_buffers(struct tcc_vdec_ctx *ctx);

static inline struct tcc_vdec_ctx *to_ctx(struct file *filp)
{
	return container_of(filp->private_data, struct tcc_vdec_ctx, fh);
}

static const struct tcc_vdec_fmt tcc_vdec_formats[] = {
	{
        .pixfmt = V4L2_PIX_FMT_ABGR32,
        .num_planes = 1,
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
    },/* {
		.pixfmt = V4L2_PIX_FMT_H264,
		.num_planes = 1,
		.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.flags = V4L2_FMT_FLAG_DYN_RESOLUTION,
	},*/
	{
		.pixfmt = V4L2_PIX_FMT_HEVC,
		.num_planes = 1,
		.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.flags = V4L2_FMT_FLAG_DYN_RESOLUTION,
	},
};

static const struct tcc_vdec_framesizes tcc_vdec_framesizes[] = {
	/*
	{
		.pixfmt	= V4L2_PIX_FMT_H264,
		.stepwise = {
			TCC_VPU_DEC_H264_MIN_W, TCC_VPU_DEC_H264_MAX_W, TCC_VPU_DEC_H264_STEPSIZE_W,
			TCC_VPU_DEC_H264_MIN_H, TCC_VPU_DEC_H264_MAX_H, TCC_VPU_DEC_H264_STEPSIZE_H },
	},
	*/
	{
		.pixfmt = V4L2_PIX_FMT_HEVC,
		.stepwise = {
			TCC_VPU_DEC_HEVC_MIN_W, TCC_VPU_DEC_HEVC_MAX_W, TCC_VPU_DEC_HEVC_STEPSIZE_W,
			TCC_VPU_DEC_HEVC_MIN_H, TCC_VPU_DEC_HEVC_MAX_H, TCC_VPU_DEC_HEVC_STEPSIZE_H },
	},
};

static struct of_device_id tccvdec_of_match[] = {
	{ .compatible = "telechips,v4l2_vdec" },
	{}
};

static const struct v4l2_m2m_ops tcc_vdec_m2m_ops = {
	.device_run = tcc_vdec_m2m_device_run,
	.job_abort = tcc_vdec_m2m_job_abort,
};

static const struct v4l2_file_operations tcc_vdec_fops = {
	.owner = THIS_MODULE,
	.open = tcc_vdec_open,
	.release = tcc_vdec_release,
	.unlocked_ioctl = video_ioctl2,
	.poll = v4l2_m2m_fop_poll,
	.mmap = v4l2_m2m_fop_mmap,
};

static const struct v4l2_ioctl_ops tcc_vdec_ioctl_ops = {
	.vidioc_querycap = tcc_vdec_querycap,
	.vidioc_enum_fmt_vid_cap = tcc_vdec_enum_fmt,
	.vidioc_enum_fmt_vid_out = tcc_vdec_enum_fmt,
	.vidioc_s_fmt_vid_cap_mplane = tcc_vdec_s_fmt,
	.vidioc_s_fmt_vid_out_mplane = tcc_vdec_s_fmt,
	.vidioc_g_fmt_vid_cap_mplane = tcc_vdec_g_fmt,
	.vidioc_g_fmt_vid_out_mplane = tcc_vdec_g_fmt,
	.vidioc_try_fmt_vid_cap_mplane = tcc_vdec_try_fmt,
	.vidioc_try_fmt_vid_out_mplane = tcc_vdec_try_fmt,
	.vidioc_g_selection = tcc_vdec_g_selection,
	.vidioc_reqbufs = v4l2_m2m_ioctl_reqbufs,
	.vidioc_querybuf = v4l2_m2m_ioctl_querybuf,
	.vidioc_create_bufs = v4l2_m2m_ioctl_create_bufs,
	.vidioc_prepare_buf = v4l2_m2m_ioctl_prepare_buf,
	.vidioc_qbuf = v4l2_m2m_ioctl_qbuf,
	.vidioc_expbuf = v4l2_m2m_ioctl_expbuf,
	.vidioc_dqbuf = v4l2_m2m_ioctl_dqbuf,
	.vidioc_streamon = v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff = v4l2_m2m_ioctl_streamoff,
	.vidioc_s_parm = tcc_vdec_s_parm,
	.vidioc_g_parm = tcc_vdec_g_parm,
	.vidioc_enum_framesizes = tcc_vdec_enum_framesizes,
	.vidioc_subscribe_event = tcc_vdec_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
	.vidioc_try_decoder_cmd = v4l2_m2m_ioctl_try_decoder_cmd,
	.vidioc_decoder_cmd = tcc_vdec_decoder_cmd,
};

static struct tcc_vdec_variant vdec_drvdata = {
	.version = 0x61,
	.port_num = 1,
};

static struct platform_device_id vdec_driver_ids[] = {
	{
		.name = "tcc-decoder",
		.driver_data = (unsigned long)&vdec_drvdata,
	},
	{},
};

static struct platform_driver tcc_vdec_driver = {
	.probe = tcc_vdec_probe,
	.remove = tcc_vdec_remove,
	.id_table = vdec_driver_ids,
	.driver = {
		.name = "tcc-vdec",
		.owner = THIS_MODULE,
		.of_match_table = tccvdec_of_match,
	},
};

static const struct vb2_ops tcc_vdec_vb2_ops = {
	.queue_setup = tcc_vdec_queue_setup,
	.buf_init = tcc_vdec_vb2_buf_init,
	.buf_cleanup = tcc_vdec_buf_cleanup,
	.buf_prepare = tcc_vdec_vb2_buf_prepare,
	.start_streaming = tcc_vdec_start_streaming,
	.stop_streaming = tcc_vdec_stop_streaming,
	.buf_queue = tcc_vb2_buf_queue,
};

static const char *v4l2_format_name(unsigned int fourcc)
{
    static char name[5];
    unsigned int i;

    for (i = 0; i < 4; ++i) {
        name[i] = fourcc & 0xff;
        fourcc >>= 8;
    }

    name[4] = '\0';
    return name;
}

#ifdef DUMP_Y_FRAME
static void dump_frame(struct tcc_vdec_ctx *ctx, struct vb2_v4l2_buffer *dst_buf) {
	void *vaddr;
	mm_segment_t old_fs;

	if (!ctx->dump_file || IS_ERR(ctx->dump_file)) {
		return;
	}

	vaddr = vb2_plane_vaddr(&dst_buf->vb2_buf, 0);
	if (!vaddr) {
		return;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	kernel_write(ctx->dump_file, vaddr, dst_buf->vb2_buf.planes[0].bytesused, &ctx->dump_file->f_pos);
	set_fs(old_fs);
}
#endif

#ifdef DUMP_BITSTREAM
static void dump_bitstream(struct tcc_vdec_ctx *ctx, struct vb2_v4l2_buffer *src_buf, bool insert_size_byte) {
	void *vaddr;
	int byte_check = 0;
	mm_segment_t old_fs;

	if (!ctx->dump_file_bs || IS_ERR(ctx->dump_file_bs)) {
		return;
	}

	vaddr = vb2_plane_vaddr(&src_buf->vb2_buf, 0);
	if (!vaddr) {
		return;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	tcvdec_info("input_bs.size: %d\n", (int)src_buf->vb2_buf.planes[0].bytesused);
	byte_check = (int)src_buf->vb2_buf.planes[0].bytesused;
	if (insert_size_byte) {
		kernel_write(ctx->dump_file_bs, &byte_check, sizeof(int), &ctx->dump_file_bs->f_pos);
	}
	kernel_write(ctx->dump_file_bs, vaddr, src_buf->vb2_buf.planes[0].bytesused, &ctx->dump_file_bs->f_pos);
	set_fs(old_fs);
}
#endif

static void tcc_vdec_ctx_init(struct tcc_vdec_ctx *ctx)
{
	ctx->fmt_out = &tcc_vdec_formats[1];
	ctx->fmt_cap = &tcc_vdec_formats[0];
	ctx->out_width = TCCVDEC_DEFAULT_WIDTH;
	ctx->out_height = TCCVDEC_DEFAULT_HEIGHT;
	ctx->width = TCCVDEC_DEFAULT_WIDTH;
	ctx->height = TCCVDEC_DEFAULT_HEIGHT;
	ctx->fps = 30;
	ctx->cap_buf_count = 0;
	ctx->num_output_bufs = 0;
	ctx->used_buf_cnt = 0;
	ctx->stopping = false;
	ctx->input_bs_buf_cnt = 0u;
	ctx->output_frame_buf_cnt = 0u;
	ctx->input_frame_buf_cnt = 0u;
	ctx->output_bs_buf_cnt = 0u;
	ctx->aborting = false;
	INIT_WORK(&ctx->decode_work, tcvdec_worker);
}

static const struct tcc_vdec_fmt *
find_format_by_index(struct tcc_vdec_ctx *ctx, unsigned int index, uint32_t type)
{
	const struct tcc_vdec_fmt *fmt = tcc_vdec_formats;
	unsigned int size = ARRAY_SIZE(tcc_vdec_formats);
	unsigned int i,k = 0;

	if (index > size)
		return NULL;

	for (i = 0; i < size; i++) {
		if (fmt[i].type != type)
			continue;
		if (k == index)
			break;
		k++;
	}

	if (i == size)
		return NULL;

	return &fmt[i];
}

static int tcc_vdec_enum_fmt(struct file *file, void *pirv, struct v4l2_fmtdesc *f)
{
	struct tcc_vdec_ctx *ctx = to_ctx(file);
	const struct tcc_vdec_fmt *fmt;
	memset(f->reserved, 0, sizeof(f->reserved));

	fmt = find_format_by_index(ctx, f->index, f->type);

	if (!fmt)
		return -EINVAL;

	f->pixelformat = fmt->pixfmt;
	f->flags = fmt->flags;

	return 0;
}

static const struct tcc_vdec_fmt *
find_format(uint32_t pixfmt, uint32_t type)
{
	const struct tcc_vdec_fmt *fmt = tcc_vdec_formats;
	unsigned int size = ARRAY_SIZE(tcc_vdec_formats);
	unsigned int i;

	for (i = 0; i < size; i++) {
		if (fmt[i].pixfmt == pixfmt)
			break;
	}

	if (i == size || fmt[i].type != type)
		return NULL;

	return &fmt[i];
}

static const struct tcc_vdec_fmt *tcc_vdec_try_fmt_common(struct tcc_vdec_ctx *ctx,
	struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *pixmp = &f->fmt.pix_mp;
	struct v4l2_plane_pix_format *pfmt = pixmp->plane_fmt;
	const struct tcc_vdec_fmt *fmt;
	uint32_t num_supported_framesize;
	uint32_t min_width;
	uint32_t max_width;
	uint32_t min_height;
	uint32_t max_height;
	uint32_t i;
	uint32_t pixelformat;

	tcvdec_dbg("try %s type=%d sizeimage=%d", v4l2_format_name(pixmp->pixelformat), f->type, pfmt[0].sizeimage);

	fmt = find_format(pixmp->pixelformat, f->type);
	if (!fmt) {
		if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
			pixmp->pixelformat = V4L2_PIX_FMT_ABGR32;
		} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
			pixmp->pixelformat = V4L2_PIX_FMT_HEVC;
		} else {
			tcvdec_err("Invalid type =%d ", f->type);
			return NULL;
		}
		fmt = find_format(pixmp->pixelformat, f->type);
	}

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		pixelformat = ctx->pix_format;
	} else {
		pixelformat = pixmp->pixelformat;
	}

	num_supported_framesize = ARRAY_SIZE(tcc_vdec_framesizes);

	for (i = 0u; i < num_supported_framesize; ++i) {
		if (pixelformat == tcc_vdec_framesizes[i].pixfmt)
			break;
	}
	min_width = tcc_vdec_framesizes[i].stepwise.min_width;
	max_width = tcc_vdec_framesizes[i].stepwise.max_width;
	min_height = tcc_vdec_framesizes[i].stepwise.min_height;
	max_height = tcc_vdec_framesizes[i].stepwise.max_height;

	pixmp->width = clamp(pixmp->width, min_width, max_width);
	pixmp->height = clamp(pixmp->height, min_height, max_height);

	// Only support progressive video
	pixmp->field = V4L2_FIELD_NONE;

	pixmp->num_planes = fmt->num_planes;

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {

		if (pixmp->pixelformat == V4L2_PIX_FMT_ABGR32)
		{
			pfmt[0].sizeimage = tcc_vdec_get_framesize_raw(ctx, pixmp->pixelformat, pixmp->width, pixmp->height, 0, 2);
			pfmt[0].bytesperline = ALIGN(pixmp->width * 4, ctx->align_width);
		}
	} else {
		pixmp->width = ctx->out_width;
		pixmp->height = ctx->out_height;
		pfmt[0].sizeimage = clamp_t(uint32_t, pfmt[0].sizeimage, 0, SZ_8M);
		pfmt[0].sizeimage = max(pfmt[0].sizeimage, tccvdec_get_output_size(pixmp->pixelformat));
		pfmt[0].bytesperline = 0;
	}
	// Always set 0 , it's not used
	for(i = 0; i < pixmp->num_planes; i++) {
		memset(pfmt[i].reserved, 0, sizeof(pfmt[i].reserved));
	}
	memset(pixmp->reserved, 0, sizeof(pixmp->reserved));
	pixmp->flags = 0;

	tcvdec_dbg("fmt=%s num_planes=%d %dx%d, sz=%d bytesperline=%d  ", 
		v4l2_format_name(fmt->pixfmt), pixmp->num_planes, pixmp->width, pixmp->height, pfmt[0].sizeimage, pfmt[0].bytesperline);

	return fmt;
}

static int tcc_vdec_s_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	int ret = 0;
	struct tcc_vdec_ctx *ctx = to_ctx(file);
	struct v4l2_pix_format_mplane *pixmp = &f->fmt.pix_mp;
	struct v4l2_pix_format_mplane orig_pixmp;
	const struct tcc_vdec_fmt *fmt;
	struct v4l2_format format;
	uint32_t pixfmt_out = 0, pixfmt_cap = 0;

	tcvdec_step("pixelformat=0x%08x", f->fmt.pix_mp.pixelformat);

	orig_pixmp = *pixmp;

	fmt = tcc_vdec_try_fmt_common(ctx, f);

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		pixfmt_out = pixmp->pixelformat;
		pixfmt_cap = ctx->fmt_cap->pixfmt;
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		pixfmt_cap = pixmp->pixelformat;
		pixfmt_out = ctx->fmt_out->pixfmt;
	}

	memset(&format, 0, sizeof(format));

	format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	format.fmt.pix_mp.pixelformat = pixfmt_out;
	format.fmt.pix_mp.width = orig_pixmp.width;
	format.fmt.pix_mp.height = orig_pixmp.height;
	tcc_vdec_try_fmt_common(ctx, &format);

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ctx->out_width = format.fmt.pix_mp.width;
		ctx->out_height = format.fmt.pix_mp.height;
		ctx->colorspace = pixmp->colorspace;
		ctx->ycbcr_enc = pixmp->ycbcr_enc;
		ctx->quantization = pixmp->quantization;
		ctx->xfer_func = pixmp->xfer_func;
	}

	memset(&format, 0, sizeof(format));

	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	format.fmt.pix_mp.pixelformat = pixfmt_cap;
	format.fmt.pix_mp.width = orig_pixmp.width;
	format.fmt.pix_mp.height = orig_pixmp.height;
	tcc_vdec_try_fmt_common(ctx, &format);

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		ctx->fmt_out = fmt;
	else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		ctx->fmt_cap = fmt;

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		unsigned int codec;
		switch(fmt->pixfmt) {
/*
		case V4L2_PIX_FMT_H264:
			codec = TCC_VIDEO_CODEC_H264;
			ctx->align_width = 8;
			ctx->align_height = 8;
			break;
		case V4L2_PIX_FMT_VP8:
			codec = TCC_VIDEO_CODEC_VP8;
			break;
		case V4L2_PIX_FMT_VP9:
			codec = TCC_VIDEO_CODEC_VP9;
			break;
*/
		case V4L2_PIX_FMT_HEVC:
			codec = TCC_VIDEO_CODEC_HEVC;
			ctx->align_width = 8;
			ctx->align_height = 8;
			break;
		default:
			{
				tcvdec_err("Not supported codec. codec=%s ", v4l2_format_name(codec));
				ret = EINVAL;
				goto exit_dec_not_supported;
			}
			break;
		}
		if(ctx->state == VPU_STATE_FREE) {
			ret = tccvpudec_if_init(ctx->vpu_drv, codec, ctx);
			if (ret)
				return ret;
			ctx->state = VPU_STATE_INIT;
		}
		ctx->pix_format = fmt->pixfmt;
	}
exit_dec_not_supported:
	return ret;
}

static int tcc_vdec_g_fmt(struct file *file, void *fh, struct v4l2_format *f)
{
	struct tcc_vdec_ctx *ctx = to_ctx(file);
	const struct tcc_vdec_fmt *fmt = NULL;
	struct v4l2_pix_format_mplane *pixmp = &f->fmt.pix_mp;

	tcvdec_step("f->type=%d", f->type);
	
	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		fmt = ctx->fmt_cap;
	else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		fmt = ctx->fmt_out;

	pixmp->pixelformat = fmt->pixfmt;

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		pixmp->width = ctx->width;
		pixmp->height = ctx->height;
		pixmp->colorspace = ctx->colorspace;
		pixmp->ycbcr_enc = ctx->ycbcr_enc;
		pixmp->quantization = ctx->quantization;
		pixmp->xfer_func = ctx->xfer_func;
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		/*
		 * width and height have no meaning.
		 * It just for v4l2-compliance test.
		 */
		pixmp->width = ctx->out_width;
		pixmp->height = ctx->out_height;
	}

	tcc_vdec_try_fmt_common(ctx, f);

	return 0;
}

static int tcc_vdec_try_fmt(struct file *file, void *fh, struct v4l2_format *f)
{
	struct tcc_vdec_ctx *ctx = to_ctx(file);

	tcc_vdec_try_fmt_common(ctx, f);

	return 0;
}

static int tcc_vdec_g_selection(struct file *file, void *fh, struct v4l2_selection *s)
{
	struct tcc_vdec_ctx *ctx = to_ctx(file);

	if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE &&
	    s->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return -EINVAL;

	switch (s->target) {
	case V4L2_SEL_TGT_CROP_BOUNDS:
	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP:
		if (s->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
			return -EINVAL;
		s->r.width = ctx->out_width;
		s->r.height = ctx->out_height;
		break;
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
	case V4L2_SEL_TGT_COMPOSE_PADDED:
		if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
			return -EINVAL;
		s->r.width = ctx->width;
		s->r.height = ctx->height;
		break;
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
	case V4L2_SEL_TGT_COMPOSE:
		if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
			return -EINVAL;
		s->r.width = ctx->out_width;
		s->r.height = ctx->out_height;
		break;
	default:
		return -EINVAL;
	}

	s->r.top = 0;
	s->r.left = 0;

	return 0;
}

static int tcc_vdec_enum_framesizes(struct file *file, void *fh, 
				struct v4l2_frmsizeenum *fsize)
{
	int i = 0;
	unsigned int num_supported_framesize = 0;

	/** VPU does not support V4L2_FRMSIZE_TYPE_DISCRETE */
	if (fsize->index != 0) {
		tcvdec_err("Invalid index=%d ", fsize->index);
		return -EINVAL;
	}

	tcvdec_step("enum_framesizes : pixel_format=0x%08x", fsize->pixel_format);
	
	num_supported_framesize = ARRAY_SIZE(tcc_vdec_framesizes);

	for (i = 0; i < num_supported_framesize; ++i) {
		if (fsize->pixel_format != tcc_vdec_framesizes[i].pixfmt)
			continue;

		fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
		fsize->stepwise = tcc_vdec_framesizes[i].stepwise;

		tcvdec_step(" fmt=%s min_w=%d max_h=%d step_w=%d min_h=%d max_h=%d step_h=%d", v4l2_format_name(fsize->pixel_format),
				fsize->stepwise.min_width, fsize->stepwise.max_width, fsize->stepwise.step_width,
				fsize->stepwise.min_height, fsize->stepwise.max_height, fsize->stepwise.step_height);
		return 0;
	}
	return -EINVAL;
}

static int tcc_vdec_subscribe_event(struct v4l2_fh *fh,
				const struct v4l2_event_subscription *sub)
{
	struct tcc_vdec_ctx *ctx = container_of(fh, struct tcc_vdec_ctx, fh);
	int ret;

	switch (sub->type) {
	case V4L2_EVENT_EOS:
		return v4l2_event_subscribe(fh, sub, 2, NULL);
	case V4L2_EVENT_SOURCE_CHANGE:
		ret = v4l2_src_change_event_subscribe(fh, sub);
		if (ret)
			return ret;
		ctx->subscriptions |= V4L2_EVENT_SOURCE_CHANGE;
		return 0;
	case V4L2_EVENT_CTRL:
		return v4l2_ctrl_subscribe_event(fh, sub);
	default:
		return -EINVAL;
	}
	return 0;
}

static int tcc_vdec_g_parm(struct file *file, void *fh, struct v4l2_streamparm *a)
{
	struct tcc_vdec_ctx *ctx = to_ctx(file);

	if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
	    a->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return -EINVAL;

	a->parm.output.capability |= V4L2_CAP_TIMEPERFRAME;
	a->parm.output.timeperframe = ctx->timeperframe;

	return 0;
}

static int tcc_vdec_s_parm(struct file *file, void *fh, struct v4l2_streamparm *a)
{
	struct tcc_vdec_ctx *ctx = to_ctx(file);
	struct v4l2_captureparm *cap = &a->parm.capture;
	struct v4l2_fract *timeperframe = &cap->timeperframe;
	uint64_t us_per_frame, fps;

	if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
	    a->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return -EINVAL;

	memset(cap->reserved, 0, sizeof(cap->reserved));
	if (!timeperframe->denominator)
		timeperframe->denominator = ctx->timeperframe.denominator;
	if (!timeperframe->numerator)
		timeperframe->numerator = ctx->timeperframe.numerator;
	cap->readbuffers = 0;
	cap->extendedmode = 0;
	cap->capability = V4L2_CAP_TIMEPERFRAME;
	us_per_frame = timeperframe->numerator * (uint64_t)USEC_PER_SEC;
	do_div(us_per_frame, timeperframe->denominator);

	if (!us_per_frame)
		return -EINVAL;

	fps = (uint64_t)USEC_PER_SEC;
	do_div(fps, us_per_frame);

	ctx->fps = fps;
	ctx->timeperframe = *timeperframe;

	return 0;
}

static int tcc_vdec_decoder_cmd(struct file *file, void *fh, struct v4l2_decoder_cmd *cmd)
{
	struct tcc_vdec_ctx *ctx = to_ctx(file);
	struct vb2_v4l2_buffer *src_buf;

	int ret = 0;

	tcvdec_step("decoder_cmd : cmd=%d", cmd->cmd);

	ret = v4l2_m2m_ioctl_try_decoder_cmd(file, fh, cmd);
	if (ret)
		return ret;

	if (cmd->cmd == V4L2_DEC_CMD_STOP) {
		if(ctx->vpu_drv != NULL) {
			src_buf = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
			v4l2_m2m_dst_buf_remove_by_buf(ctx->fh.m2m_ctx, src_buf);
			v4l2_m2m_buf_done(src_buf, VB2_BUF_STATE_DONE);
			mutex_lock(&ctx->lock);
			ctx->stopping = true;
			mutex_unlock(&ctx->lock);
		}
	}
	return 0;
}

static int tcc_vdec_querycap(struct file *file, void *priv, struct v4l2_capability *cap)
{
	tcvdec_step("querycap ");

	strscpy(cap->driver, "telechips-vdec", sizeof(cap->driver));
	strscpy(cap->card, "Telechips V4l2 video decoder", sizeof(cap->card));
	strscpy(cap->bus_info, "platform:tcc-vdec", sizeof(cap->bus_info));

	return 0;
}

static uint32_t get_framesize_raw_argb(struct tcc_vdec_ctx *ctx, uint32_t width, uint32_t height)
{
    uint32_t stride = ALIGN(width * 4, ctx->align_width);
    uint32_t size = stride * height;
    return ALIGN(size, SZ_4K);
}


static uint32_t tcc_vdec_get_framesize_raw(struct tcc_vdec_ctx *ctx, uint32_t v4l2_fmt, uint32_t width,
	uint32_t height, uint32_t plane_idx, uint32_t planes)
{
	switch (v4l2_fmt)
	{
		case V4L2_PIX_FMT_ABGR32:
			return get_framesize_raw_argb(ctx, width, height);
		default:
			return 0;
	}
}

static uint32_t tccvdec_get_output_size(uint32_t v4l2_fmt)
{
	uint32_t sz;

	switch (v4l2_fmt) {
	case V4L2_PIX_FMT_VP9:
	case V4L2_PIX_FMT_HEVC:
		sz = 6 * 1024 * 1024;
		break;
	case V4L2_PIX_FMT_MPEG:
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_H264_NO_SC:
	case V4L2_PIX_FMT_H264_MVC:
	case V4L2_PIX_FMT_H263:
	case V4L2_PIX_FMT_MPEG1:
	case V4L2_PIX_FMT_MPEG2:
	case V4L2_PIX_FMT_MPEG4:
	case V4L2_PIX_FMT_XVID:
	case V4L2_PIX_FMT_VC1_ANNEX_G:
	case V4L2_PIX_FMT_VC1_ANNEX_L:
	case V4L2_PIX_FMT_VP8:
	default:
		sz = 3 * 1024 * 1024;
		break;
	}

	return ALIGN(sz, SZ_4K);
}

static int tcc_vdec_start_streaming(struct vb2_queue *q, unsigned int count)
{
	int ret = 0;
	struct tcc_vdec_ctx *ctx = vb2_get_drv_priv(q);
#ifdef DUMP_Y_FRAME
	mm_segment_t old_fs;
#endif
	if((ctx == NULL) || (q == NULL)) {
		tcvdec_err(" Invalid parameter");
		return -EFAULT;
	}
#ifdef DUMP_Y_FRAME
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	ctx->dump_file = filp_open("/home/root/tcvdecdump.yuv", O_WRONLY | O_CREAT | O_APPEND | O_TRUNC, 0644);
	if (IS_ERR(ctx->dump_file)) {
		ctx->dump_file = NULL;
	}
	set_fs(old_fs);
#endif

	ctx->streaming = true;

	if(q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE && ctx->state < VPU_STATE_FRAMEBUFFER) {
		ctx->state = VPU_STATE_FRAMEBUFFER;
		ctx->input_bs_buf_cnt = 0u;
		ctx->output_frame_buf_cnt = 0u;
		ctx->input_frame_buf_cnt = 0u;
		ctx->output_bs_buf_cnt = 0u;
	}

	return ret;
}

static void tcc_vdec_stop_streaming(struct vb2_queue *q)
{
    struct tcc_vdec_ctx *ctx = vb2_get_drv_priv(q);
    struct vb2_v4l2_buffer *src_buf, *dst_buf;
	struct tcc_vdec_buffer *fb, *temp = NULL;

#ifdef DUMP_Y_FRAME
    if (ctx->dump_file && !IS_ERR(ctx->dump_file)) {
        tcvdec_info("[%s %d] dumpfile close\n", );
        filp_close(ctx->dump_file, NULL);
        ctx->dump_file = NULL;
    }
#endif

#ifdef DUMP_BITSTREAM
    if (ctx->dump_file_bs && !IS_ERR(ctx->dump_file)) {
        tcvdec_info("[%s %d] bitstream dumpfile close\n", );
        filp_close(ctx->dump_file_bs, NULL);
        ctx->dump_file_bs = NULL;
    }
#endif
	tcvdec_step("");

	mutex_lock(&ctx->lock);
	ctx->streaming = false;
	ctx->stopping = true;
	mutex_unlock(&ctx->lock);

	if (ctx->tcc_dev && ctx->tcc_dev->workqueue) {
		flush_workqueue(ctx->tcc_dev->workqueue);
	}

    if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		mutex_lock(&ctx->lock);
		while ((src_buf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx)))
			v4l2_m2m_buf_done(src_buf, VB2_BUF_STATE_ERROR);
		mutex_unlock(&ctx->lock);
	} else if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		tccvpudec_if_flush(ctx->vpu_drv);
		tccvdecqueue_reset(&ctx->timestamp_queue);

		mutex_lock(&ctx->lock);
		while ((dst_buf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx)))
			v4l2_m2m_buf_done(dst_buf, VB2_BUF_STATE_ERROR);
		list_for_each_entry_safe(fb, temp, &ctx->framebuffer_list, framebuffer_item) {
			if(fb->m2m_buf.vb.vb2_buf.state == VB2_BUF_STATE_ACTIVE)
				v4l2_m2m_buf_done(&fb->m2m_buf.vb, VB2_BUF_STATE_ERROR);
		}
		mutex_unlock(&ctx->lock);
	}

	mutex_lock(&ctx->lock);
	ctx->stopping = false;
	mutex_unlock(&ctx->lock);
}

static void tcc_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct tcc_vdec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *src_buf;
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct tcc_vdec_ctx *vdec_ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct tcc_codec_bs_t input_bs;
	struct tcc_codec_header_t hdr;
	struct tcc_vdec_buffer *tdst_buf = NULL;

	mutex_lock(&ctx->lock);
	v4l2_m2m_buf_queue(vdec_ctx->fh.m2m_ctx, vbuf);
	if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
			tdst_buf = to_tcvdec_buffer(vbuf);
			tcvdec_step("Queue capture buffer: 0x%llx", (u64)tdst_buf->planes[0].phy_addr);

		 if ((tdst_buf->used) && (tdst_buf->vpu_idx >= 0)) {
			tccvpudec_if_buf_clear(ctx->vpu_drv, tdst_buf->vpu_idx);
			tdst_buf->used = false;
			tdst_buf->vpu_idx = -1;
			--ctx->used_buf_cnt;
		 }
		 if(ctx->stopping) {
			clear_used_buffers(ctx);
			v4l2_m2m_dst_buf_remove_by_buf(ctx->fh.m2m_ctx, vbuf);
			v4l2_m2m_buf_done(vbuf, VB2_BUF_STATE_DONE);
		}
		++ctx->input_frame_buf_cnt;
	} else {
#ifdef DUMP_BITSTREAM
		dump_bitstream(ctx, vbuf, true);
#endif
		if(ctx->stopping) {
			tcvdec_err("Queue bitstream buffer while stopping - application bug");
		}
		++ctx->input_bs_buf_cnt;
	}
	mutex_unlock(&ctx->lock);
	if (ctx->state == VPU_STATE_FRAMEBUFFER) {
		// already VPU driver is seqeunce header init
		return;
	}

	src_buf = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	if (!src_buf) {
		return;
	}

	if(ctx->state == VPU_STATE_INIT) {
		input_bs.size = (size_t)src_buf->vb2_buf.planes[0].bytesused;
		input_bs.va = vb2_plane_vaddr(&src_buf->vb2_buf, 0);
		input_bs.dma_addr = vb2_dma_contig_plane_dma_addr(&src_buf->vb2_buf, 0);
		input_bs.pa = (void*)dma_to_phys(ctx->tcc_dev->dev, input_bs.dma_addr);

		memset(&hdr, 0x0, sizeof(hdr));
		if (0 == tccvpudec_if_parse_seq_header(ctx->vpu_drv, &input_bs, &hdr)) {
			tcvdec_info("sequence header is parsed. %dx%d, min=%d", hdr.width, hdr.height, hdr.min_framebuffer_cnt);
			ctx->state = VPU_STATE_HEADER;
			ctx->min_framebuffer_cnt = hdr.min_framebuffer_cnt;
			ctx->user_framebuffer_cnt = 8;
			ctx->profile = hdr.profile;
			ctx->level = hdr.level;
			ctx->width = hdr.width;
			ctx->height = hdr.height;
			ctx->out_width = hdr.width;
			ctx->out_height = hdr.height;
		} else {
			tcvdec_err("Failed to parse sequence header. size=%ld va=%p dma=%p\n ", input_bs.size, input_bs.va, (void*)input_bs.dma_addr);
		}
	}
}

static int tcc_vdec_vb2_buf_init(struct vb2_buffer *vb)
{
	struct tcc_vdec_ctx *ctx = NULL;
	struct vb2_v4l2_buffer *vbuf = NULL;
	struct tcc_vdec_buffer *buf = NULL;
	int i;

	if(vb == NULL) {
		tcvdec_err("Invalid parameter.");
		return -EINVAL;
	}
	ctx = vb2_get_drv_priv(vb->vb2_queue);
	vbuf = to_vb2_v4l2_buffer(vb);
	buf = to_tcvdec_buffer(vbuf);

	if(ctx == NULL || buf == NULL) {
		tcvdec_err("Invalid parameter.");
		return -EINVAL;
	}

	if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		ctx->cap_buf_count++;

		for(i = 0; i < vbuf->vb2_buf.num_planes; i++) {
			buf->planes[i].dma_addr = vb2_dma_contig_plane_dma_addr(&vbuf->vb2_buf, i);
			buf->planes[i].phy_addr = (void*)dma_to_phys(ctx->tcc_dev->dev, buf->planes[i].dma_addr);
			buf->planes[i].size = tcc_vdec_get_framesize_raw(ctx, ctx->fmt_cap->pixfmt, ctx->width, ctx->height, i, vbuf->vb2_buf.num_planes);
			tcvdec_step("phy[%d] : 0x%llx size : %d",i , (u64)buf->planes[i].phy_addr, buf->planes[i].size);
		}

		mutex_lock(&ctx->lock);
		buf->vpu_idx = 0;
		buf->used = false;
		mutex_unlock(&ctx->lock);

		list_add_tail(&buf->framebuffer_item, &ctx->framebuffer_list);
	}
	return 0;
}

static void tcc_vdec_buf_cleanup(struct vb2_buffer *vb)
{
	struct tcc_vdec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	tcvdec_step("");

	if(ctx == NULL) {
		tcvdec_err("%s Invalid parameter.", __func__);
		return;
	}
	if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		ctx->cap_buf_count--;
	}
}


static int tcc_vdec_vb2_buf_prepare(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	tcvdec_step("");


	if(vbuf == NULL) {
		tcvdec_err("Invalid parameter.");
		return -EINVAL;
	}

	if (V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		if (vbuf->field == V4L2_FIELD_ANY)
			vbuf->field = V4L2_FIELD_NONE;
		if (vbuf->field != V4L2_FIELD_NONE) {
			tcvdec_err("field isn't supported. only support progressive video");
			return -EINVAL;
		}
	}
	return 0;
}

static int tcc_vdec_queue_setup(struct vb2_queue *vq,
			unsigned int *num_buffers, unsigned int *num_planes,
			unsigned int sizes[], struct device *allocators[])
{
	struct tcc_vdec_ctx* ctx = vb2_get_drv_priv(vq);
	int ret = 0;
	int i = 0;
#ifdef DUMP_BITSTREAM
	mm_segment_t old_fs_bs;
#endif
	if(ctx == NULL) {
		tcvdec_err("Invalid parameter.");
		return -EINVAL;
	}
	tcvdec_step("type=%d num_buffers=%d num_planes=%d size=%d/%d", vq->type, *num_buffers, *num_planes, sizes[0], sizes[1]);

	if (*num_planes) {
		if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE &&
			*num_planes != ctx->fmt_out->num_planes)
			return -EINVAL;

		if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
			*num_planes != ctx->fmt_cap->num_planes)
			return -EINVAL;

		if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE &&
			sizes[0] < ctx->src_psize[0])
			return -EINVAL;

		if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
			sizes[0] < ctx->dst_psize[0])
			return -EINVAL;

		return 0;
	}

	switch (vq->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		*num_planes = ctx->fmt_out->num_planes;//min(, 1);
		sizes[0] = tccvdec_get_output_size(ctx->fmt_out->pixfmt);
		sizes[0] = max(sizes[0], ctx->input_buf_size);
		ctx->input_buf_size = sizes[0];
		ctx->num_input_bufs = *num_buffers;
#ifdef DUMP_BITSTREAM
		old_fs_bs = get_fs();
		set_fs(KERNEL_DS);
		ctx->dump_file_bs = filp_open("/home/root/tcvdecdump_bs.bin", O_WRONLY | O_CREAT | O_APPEND | O_TRUNC, 0644);
		if (IS_ERR(ctx->dump_file_bs)) {
			ctx->dump_file_bs = NULL;
		}
		set_fs(old_fs_bs);
#endif
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		*num_planes = ctx->fmt_cap->num_planes;
		for(i = 0; i < *num_planes; i++) {
			sizes[i] = tcc_vdec_get_framesize_raw(ctx, ctx->fmt_cap->pixfmt,
							ctx->width, ctx->height, i, *num_planes);
		}
		//*num_buffers = 5;//ctx->min_framebuffer_cnt + ctx->user_framebuffer_cnt;//TCCVDEC_MAX_CAPTURE_BUFFER_NUM;
		ctx->num_output_bufs = *num_buffers;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	tcvdec_dbg("[Out] type=%d num_buffers=%d num_planes=%d size=%d/%d", vq->type, *num_buffers, *num_planes, sizes[0], sizes[1]);

	return ret;
}

static void tcc_vdec_m2m_job_abort(void *ctx)
{
	struct tcc_vdec_ctx *vdec_ctx = ctx;
	tcvdec_step("abort");
	
	mutex_lock(&vdec_ctx->lock);
	vdec_ctx->aborting = true;
	mutex_unlock(&vdec_ctx->lock);
}


static void clear_used_buffers(struct tcc_vdec_ctx *ctx)
{
	int idx = 0;
	for(; idx < ctx->num_output_bufs; idx++)
	{
		tccvpudec_if_buf_clear(ctx->vpu_drv, idx);
	}
}

static void tcvdec_worker(struct work_struct *work)
{
	struct tcc_vdec_ctx *ctx = container_of(work, struct tcc_vdec_ctx, decode_work);
	struct tcc_vdec_buffer *tdst_buf;
	struct tcc_codec_bs_t input_bs;
	struct tcc_codec_decode_output_t output;

	struct vb2_v4l2_buffer *src_buf, *dst_buf;
	struct vb2_buffer *dst_vb;
	struct v4l2_event ev;
 
	int i,ret = 0;
 
	src_buf = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);

	if (src_buf == NULL) {
		v4l2_m2m_job_finish(ctx->m2m_dev, ctx->fh.m2m_ctx);
		tcvdec_dbg("src_buf empty!!");
		return;
	}

	if(!ctx->streaming) {
		v4l2_m2m_src_buf_remove_by_buf(ctx->fh.m2m_ctx, src_buf);
		v4l2_m2m_buf_done(src_buf, VB2_BUF_STATE_DONE);
		return;
	}

	memset(&input_bs, 0x0, sizeof(input_bs));
	input_bs.size = (size_t)src_buf->vb2_buf.planes[0].bytesused;
	input_bs.va = vb2_plane_vaddr(&src_buf->vb2_buf, 0);
	input_bs.dma_addr = vb2_dma_contig_plane_dma_addr(&src_buf->vb2_buf, 0);
	input_bs.pa = (void*)dma_to_phys(ctx->tcc_dev->dev, input_bs.dma_addr);
	input_bs.timestamp = src_buf->vb2_buf.timestamp;

	tcvdec_step("[%llu]input : 0x%llx (size : 0x%lx)", input_bs.timestamp, (u64)input_bs.pa, input_bs.size);

	memset(&output, 0x0, sizeof(output));

	dst_buf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
	tdst_buf = to_tcvdec_buffer(dst_buf);

	for( i = 0; i < ctx->fmt_cap->num_planes; i++) {
		output.fb.pa[i] = tdst_buf->planes[i].phy_addr;
		output.fb.size[i] = tdst_buf->planes[i].size;
		output.fb.dma_addr[i] = tdst_buf->planes[i].dma_addr;
	}

	mutex_lock(&ctx->lock);
	ret = tccvpudec_if_decode(ctx->vpu_drv, &input_bs, &output);
	mutex_unlock(&ctx->lock);

	if (ret != VPU_RETCODE_SUCCESS) {
		tcvdec_err("Failed to decode. ret=%d", ret);
		if(ctx->streaming) {
			++ctx->output_bs_buf_cnt;
			v4l2_m2m_src_buf_remove_by_buf(ctx->fh.m2m_ctx, src_buf);
			v4l2_m2m_buf_done(src_buf, VB2_BUF_STATE_DONE);
		} else {
			v4l2_m2m_job_finish(ctx->m2m_dev, ctx->fh.m2m_ctx);
			return;
		}
		if (ret == VPU_RETCODE_CODEC_SPECOUT) {
			tcvdec_err("SPECOUT detected. Exiting workqueue and flushing buffers.");
			dst_buf->flags |= V4L2_BUF_FLAG_LAST;
		}
	}
	
	if (output.status & TCC_VIDEO_CODEC_STATUS_BUF_FULL) {
		mutex_lock(&ctx->lock);
		clear_used_buffers(ctx);
		mutex_unlock(&ctx->lock);
	} else if (output.status & TCC_VIDEO_CODEC_STATUS_DECODED){
		if (output.decodedIndex >= 0) {
			tccvdecqueue_enqueue(&ctx->timestamp_queue, input_bs.timestamp, output.decodedIndex, NULL);
		}

		v4l2_m2m_src_buf_remove_by_buf(ctx->fh.m2m_ctx, src_buf);
		v4l2_m2m_buf_done(src_buf, VB2_BUF_STATE_DONE);
	} 
	
	if (output.status & TCC_VIDEO_CODEC_STATUS_DISPLAYABLE) {
		struct tccvdecqueue_entry *ts_entry;

		ts_entry = tccvdecqueue_dequeue(&ctx->timestamp_queue);
		if (ts_entry) {
			dst_buf->vb2_buf.timestamp = ts_entry->timestamp;
			kfree(ts_entry);
		} else {
			dst_buf->vb2_buf.timestamp = input_bs.timestamp;
		}
		tdst_buf->vpu_idx = output.displayIndex;
		tdst_buf->used = true;
		dst_buf->flags |= src_buf->flags & V4L2_BUF_FLAG_LAST;
		dst_vb = &dst_buf->vb2_buf;

		for( i = 0; i < ctx->fmt_cap->num_planes; i++) {
			vb2_set_plane_payload(dst_vb, i, tdst_buf->planes[i].size);			
			dst_vb->planes[i].data_offset = i;
		}

		tcvdec_step("Dequeue capture buffer: 0x%llx (%lld) idx : %d, size : %u", (u64)tdst_buf->planes[0].phy_addr, dst_buf->vb2_buf.timestamp, tdst_buf->vpu_idx, tdst_buf->planes[0].size);

		if(ctx->streaming) {
			v4l2_m2m_buf_done(dst_buf, VB2_BUF_STATE_DONE);
		} else {
			tccvpudec_if_buf_clear(ctx->vpu_drv, output.displayIndex);
			v4l2_m2m_job_finish(ctx->m2m_dev, ctx->fh.m2m_ctx);
			return;
		}
	}

	if (dst_buf->flags & V4L2_BUF_FLAG_LAST) {
		tcvdec_err("Detect V4L2_BUF_FLAG_LAST");
		ev.type = V4L2_EVENT_EOS;
		v4l2_event_queue_fh(&ctx->fh, &ev);
		v4l2_m2m_buf_done(dst_buf, VB2_BUF_STATE_ERROR);
	}
	 v4l2_m2m_job_finish(ctx->m2m_dev, ctx->fh.m2m_ctx);
}

static void tcc_vdec_m2m_device_run(void *priv)
{
	struct tcc_vdec_ctx *ctx = priv;
	struct tcc_vdec_dev *dev = ctx->tcc_dev;

	queue_work(dev->workqueue, &ctx->decode_work);
}

static int m2m_queue_init(void *priv, struct vb2_queue *src_vq, struct vb2_queue *dst_vq)
{
	struct tcc_vdec_ctx *ctx = priv;
	int ret;
	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->ops = &tcc_vdec_vb2_ops;
	src_vq->mem_ops = &vb2_dma_contig_memops;
	src_vq->drv_priv = ctx;
	src_vq->buf_struct_size = sizeof(struct tcc_vdec_buffer);
	src_vq->allow_zero_bytesused = 1;
	src_vq->min_buffers_needed = 0;
	src_vq->dev = &ctx->tcc_dev->plat_dev->dev;
	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_MMAP | VB2_DMABUF;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->ops = &tcc_vdec_vb2_ops;
	dst_vq->mem_ops = &vb2_dma_contig_memops;
	dst_vq->drv_priv = ctx;
	dst_vq->buf_struct_size = sizeof(struct tcc_vdec_buffer);
	dst_vq->allow_zero_bytesused = 1;
	dst_vq->min_buffers_needed = 0;
	dst_vq->dev = &ctx->tcc_dev->plat_dev->dev;
	dst_vq->dma_attrs = DMA_ATTR_WRITE_COMBINE;

	ret = vb2_queue_init(dst_vq);
	if (ret) {
		vb2_queue_release(src_vq);
		return ret;
	}

	return 0;
}

static int tcc_vdec_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tcc_vdec_dev *tcc_dev;
	struct video_device *vdev;
	int ret = -ENOENT;

	tcvdec_step("probe : pdev=%p", pdev);

	if (!dev->parent) {
		return -EPROBE_DEFER;
	}

	tcc_dev = devm_kzalloc(dev, sizeof(*tcc_dev), GFP_KERNEL);
	if (!tcc_dev)
		return -ENOMEM;
	tcc_dev->plat_dev = pdev;
	tcc_dev->vdev_dec = NULL;
	tcc_dev->dev = dev;
	tcc_dev->workqueue =
		alloc_ordered_workqueue("tcvdec",
			WQ_MEM_RECLAIM | WQ_FREEZABLE | WQ_HIGHPRI);
	if (!tcc_dev->workqueue) {
		tcvdec_err("Failed to create decode workqueue");
		ret = -ENOMEM;
		goto exit_destroy_tcc_dev;
	}

	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64));
	if (ret) {
		goto exit_destroy_workqueue;
	}

	if (!dev->dma_parms) {
		dev->dma_parms = devm_kzalloc(dev, sizeof(*dev->dma_parms), GFP_KERNEL);
		if (!dev->dma_parms)
			ret = -ENOMEM;
		goto exit_destroy_workqueue;
	}

	//ret = of_reserved_mem_device_init(dev);
	//if (ret) {
	//	dev_info(dev, "init reserved memory failed\n");
	//	goto exit_destroy_workqueue;
	//}

	//dma_set_max_seg_size(dev, DMA_BIT_MASK(32));

	mutex_init(&tcc_dev->dev_mutex);

	ret = v4l2_device_register(&pdev->dev, &tcc_dev->v4l2_dev);
	if (ret) {
		goto exit_free_dev;
	}

	/* decoder */
	vdev = video_device_alloc();
	if (!vdev) {
		v4l2_err(&tcc_dev->v4l2_dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto exit_free_dev;
	}

	strscpy(vdev->name, "tcc-video-decoder", sizeof(vdev->name));
	vdev->fops = &tcc_vdec_fops,
	vdev->ioctl_ops = &tcc_vdec_ioctl_ops;
	vdev->release = video_device_release;
	vdev->lock = &tcc_dev->dev_mutex;
	vdev->vfl_dir = VFL_DIR_M2M;
	vdev->minor = -1;
	vdev->device_caps	= V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;

	vdev->v4l2_dev = &tcc_dev->v4l2_dev;

	vdev->minor = 9;
	tcc_dev->vdev_dec = vdev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
	ret = video_register_device(vdev, VFL_TYPE_VIDEO, vdev->minor);
#else
	ret = video_register_device(vdev, VFL_TYPE_GRABBER, vdev->minor);
#endif

	if (ret) {
		tcvdec_err("Failed to register video device\n ");
		goto exit_dec_reg;
	}
	v4l2_info(&tcc_dev->v4l2_dev, "decoder registered as /dev/video%d\n", vdev->num);
	video_set_drvdata(vdev, tcc_dev);
	platform_set_drvdata(pdev, tcc_dev);

	return 0;
exit_dec_reg:
	video_device_release(tcc_dev->vdev_dec);
	v4l2_device_unregister(&tcc_dev->v4l2_dev);
exit_free_dev:
	of_reserved_mem_device_release(dev);
exit_destroy_workqueue:
	destroy_workqueue(tcc_dev->workqueue);
exit_destroy_tcc_dev:
	kfree(tcc_dev);
	tcvdec_err("with error ");
	return ret;
}

static int tcc_vdec_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tcc_vdec_dev *tcc_dev = platform_get_drvdata(pdev);

	v4l2_info(&tcc_dev->v4l2_dev, "Removing %s\n", pdev->name);
	tcvdec_step("remove : pdev=%p(%s)", pdev, pdev->name);

	flush_workqueue(tcc_dev->workqueue);
	destroy_workqueue(tcc_dev->workqueue);
	video_unregister_device(tcc_dev->vdev_dec);
	v4l2_device_unregister(&tcc_dev->v4l2_dev);

	of_reserved_mem_device_release(dev);

	kfree(tcc_dev);

	return 0;
}

static int tcc_vdec_open(struct file *file)
{
	int ret = 0;
	struct tcc_vdec_dev *tcc_dev = video_drvdata(file);
	struct tcc_vdec_ctx *ctx = NULL;
	if_null_print_return_value(file, -EINVAL);

	tcvdec_step("Entering: (file=%p)", file);

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);

	if (!ctx) {
		tcvdec_err("Failed to allocate memory for ctx\n");
		return -ENOMEM;
	}

	tcc_vdec_ctx_init(ctx);

	if(0 != tccvdec_ctrl_init(ctx)) {
		tcvdec_err("Failed to initialize v4l2 controls \n");
		goto err_core_destory;
	}

	INIT_LIST_HEAD(&ctx->framebuffer_list);

	mutex_init(&ctx->lock);
	ctx->tcc_dev = tcc_dev;
	ctx->m2m_dev = v4l2_m2m_init(&tcc_vdec_m2m_ops);

	if (IS_ERR(ctx->m2m_dev)) {
		ret = PTR_ERR(ctx->m2m_dev);
		tcvdec_err("Failed to initialize m2m framework \n");
		goto err_core_destory;
	}

	v4l2_fh_init(&ctx->fh, tcc_dev->vdev_dec);
	ctx->m2m_ctx = v4l2_m2m_ctx_init(ctx->m2m_dev, ctx, m2m_queue_init);

	if (IS_ERR(ctx->m2m_ctx)) {
		ret = PTR_ERR(ctx->m2m_ctx);
		tcvdec_err("Failed to initialize m2m context \n");
		goto err_m2m_release;
	}
	ctx->fh.ctrl_handler = &ctx->ctrl_handler;
	v4l2_fh_add(&ctx->fh);
	ctx->fh.m2m_ctx = ctx->m2m_ctx;
	file->private_data = &ctx->fh;

	tccvdecqueue_init(&ctx->timestamp_queue);

	ctx->vpu_drv = kzalloc(sizeof(*ctx->vpu_drv), GFP_KERNEL);
	if (!ctx->vpu_drv) {
		tcvdec_err("Failed to allocate memory for vpu drv interface \n");
		goto err_destroy;
	}

	ctx->pix_format = V4L2_PIX_FMT_HEVC;

	return 0;

err_m2m_release:
	v4l2_fh_exit(&ctx->fh);
	v4l2_m2m_release(ctx->m2m_dev);
err_core_destory:
	tccvdec_ctrl_init(ctx);
	kfree(ctx->vpu_drv);
err_destroy:
	kfree(ctx);
	return ret;
}

static int tcc_vdec_release(struct file *file)
{
	struct tcc_vdec_ctx *ctx = NULL;

    if (file == NULL || file->private_data == NULL) {
        tcvdec_err("Invalid file or private_data\n");
        return -EINVAL;
    }

	tcvdec_step("Entering: (file=%p)", file);

    ctx = to_ctx(file);
    if (ctx != NULL) {
		if (ctx->tcc_dev && ctx->tcc_dev->workqueue) {
			flush_workqueue(ctx->tcc_dev->workqueue);
		}	
        tccvpudec_if_deinit(ctx->vpu_drv);
        tccvdec_ctrl_deinit(ctx);
        mutex_destroy(&ctx->lock);
        v4l2_fh_del(&ctx->fh);
        v4l2_fh_exit(&ctx->fh);
        v4l2_m2m_ctx_release(ctx->m2m_ctx);
        v4l2_m2m_release(ctx->m2m_dev);
        kfree(ctx->vpu_drv);
        kfree(ctx);
    }
    return 0;
}

MODULE_DEVICE_TABLE(of, tccvdec_of_match);

module_platform_driver(tcc_vdec_driver);

MODULE_AUTHOR("Telechips.Co.Ltd");

MODULE_DESCRIPTION("Telechips Video Decoder Driver");

MODULE_LICENSE("GPL");

MODULE_VERSION(TCCVDEC_DRIVER_VERSION);
