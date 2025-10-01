#include "tccvenc_fmt.h"
#include "vpu_enc_v3.h"

static const struct tccvenc_fmt tccvenc_output_formats[] = {
	{ V4L2_PIX_FMT_NV12, "YUV420 Semi-Planar (NV12)" },
	{ V4L2_PIX_FMT_YUV420, "YUV420 Planar (I420)" },
};

static const struct tccvenc_fmt tccvenc_capture_formats[] = {
	{ V4L2_PIX_FMT_H264, "H.264 Encoded" },
	{ V4L2_PIX_FMT_HEVC, "H.265 Encoded" },
};


int tccvenc_enum_fmt(struct file *file, void *priv, struct v4l2_fmtdesc *f)
{
	tcvenc_step("called: type=%u, index=%u", f->type, f->index);

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (f->index >= ARRAY_SIZE(tccvenc_output_formats)) {
			tcvenc_step("invalid output index: %u", f->index);
			return -EINVAL;
		}
		f->pixelformat = tccvenc_output_formats[f->index].fourcc;
		strscpy(f->description, tccvenc_output_formats[f->index].desc, sizeof(f->description));
		tcvenc_step("output format: %s (0x%08x)", f->description, f->pixelformat);
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (f->index >= ARRAY_SIZE(tccvenc_capture_formats)) {
			tcvenc_step("invalid capture index: %u", f->index);
			return -EINVAL;
		}
		f->pixelformat = tccvenc_capture_formats[f->index].fourcc;
		f->flags = V4L2_FMT_FLAG_DYN_RESOLUTION;
		strscpy(f->description, tccvenc_capture_formats[f->index].desc, sizeof(f->description));
		tcvenc_step("capture format: %s (0x%08x)", f->description, f->pixelformat);
	} else {
		tcvenc_step("invalid type: %u", f->type);
		return -EINVAL;
	}
	return 0;
}

int tccvenc_try_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *pix = &f->fmt.pix_mp;

	tcvenc_step("called: type=%u, width=%u, height=%u, fourcc=0x%08x",
		    f->type, pix->width, pix->height, pix->pixelformat);

	if (pix->width == 0) pix->width = 1280;
	if (pix->height == 0) pix->height = 720;

	if (pix->width > MAX_WIDTH || pix->height > MAX_HEIGHT) {
		tcvenc_step("resolution too large: %ux%u", pix->width, pix->height);
		return -EINVAL;
	}

	pix->field = V4L2_FIELD_NONE;

	switch (f->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		switch (pix->pixelformat) {
		case V4L2_PIX_FMT_NV12:
			/* Plane 0 : Y */
			pix->plane_fmt[0].bytesperline = pix->width;
			pix->plane_fmt[0].sizeimage = pix->width * pix->height;

			/* Plane 1 : UV */
			pix->plane_fmt[1].bytesperline = pix->width;
			pix->plane_fmt[1].sizeimage =
				pix->width * pix->height / 2;

			tcvenc_step(
				"NV12 OUT: Y bpl=%u size=%u, UV bpl=%u size=%u",
				pix->plane_fmt[0].bytesperline,
				pix->plane_fmt[0].sizeimage,
				pix->plane_fmt[1].bytesperline,
				pix->plane_fmt[1].sizeimage);
			break;

		case V4L2_PIX_FMT_YUV420: /* I420 */
			/* Plane 0 : Y */
			pix->plane_fmt[0].bytesperline = pix->width;
			pix->plane_fmt[0].sizeimage = pix->width * pix->height;

			/* Plane 1 : U */
			pix->plane_fmt[1].bytesperline = pix->width / 2;
			pix->plane_fmt[1].sizeimage =
				pix->width * pix->height / 4;

			/* Plane 2 : V */
			pix->plane_fmt[2].bytesperline = pix->width / 2;
			pix->plane_fmt[2].sizeimage =
				pix->width * pix->height / 4;

			tcvenc_step(
				"I420 OUT: Y bpl=%u size=%u, U bpl=%u size=%u, V bpl=%u size=%u",
				pix->plane_fmt[0].bytesperline,
				pix->plane_fmt[0].sizeimage,
				pix->plane_fmt[1].bytesperline,
				pix->plane_fmt[1].sizeimage,
				pix->plane_fmt[2].bytesperline,
				pix->plane_fmt[2].sizeimage);
			break;
		default:
			tcvenc_step("unsupported output format: 0x%08x",
				    pix->pixelformat);
			return -EINVAL;
		}
		if (pix->num_planes == 1) {
			pix->plane_fmt[0].bytesperline = pix->width;
			pix->plane_fmt[0].sizeimage   = pix->width * pix->height * 3 / 2;
		}
		
		break;

	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		switch (pix->pixelformat) {
			case V4L2_PIX_FMT_H264:
			case V4L2_PIX_FMT_HEVC:
				pix->plane_fmt[0].bytesperline = 0;
				pix->plane_fmt[0].sizeimage = ALIGN(6 * 1024 * 1024, SZ_4K);
				tcvenc_step("capture: sizeimage=%u", pix->plane_fmt[0].sizeimage);
				break;
			default:
				tcvenc_step("unsupported capture format: 0x%08x", pix->pixelformat);
				return -EINVAL;
		}
		break;

	default:
		tcvenc_step("invalid type: %u", f->type);
		return -EINVAL;
	}

	return 0;
}

int tccvenc_s_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct tcc_venc_ctx *ctx = priv;
	int ret = tccvenc_try_fmt(file, priv, f);

	tcvenc_step("called: type=%u", f->type);

	if (ret) {
		tcvenc_step("try_fmt failed: %d", ret);
		return ret;
	}

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ctx->src_fmt = f->fmt.pix_mp;
		tcvenc_step("src_fmt set: width=%u, height=%u, fourcc=0x%08x, num_planes=%d",
			    ctx->src_fmt.width, ctx->src_fmt.height,
			    ctx->src_fmt.pixelformat, ctx->src_fmt.num_planes);
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		ctx->dst_fmt = f->fmt.pix_mp;
		tcvenc_step("dst_fmt set: width=%u, height=%u, fourcc=0x%08x, num_planes=%d",
			    ctx->dst_fmt.width, ctx->dst_fmt.height,
			    ctx->dst_fmt.pixelformat, ctx->dst_fmt.num_planes);
	} else {
		tcvenc_step("invalid type: %u", f->type);
		return -EINVAL;
	}

	return 0;
}

int tccvenc_g_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct tcc_venc_ctx *ctx = priv;

	tcvenc_step("called: type=%u", f->type);

	switch (f->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		f->fmt.pix_mp = ctx->src_fmt;
		tcvenc_step(
			"return src_fmt: width=%u, height=%u, fourcc=0x%08x",
			ctx->src_fmt.width, ctx->src_fmt.height,
			ctx->src_fmt.pixelformat);
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		f->fmt.pix_mp = ctx->dst_fmt;
		tcvenc_step(
			"return dst_fmt: width=%u, height=%u, fourcc=0x%08x",
			ctx->dst_fmt.width, ctx->dst_fmt.height,
			ctx->dst_fmt.pixelformat);
		break;
	default:
		tcvenc_step("invalid type: %u", f->type);
		return -EINVAL;
	}

	return 0;
}

int tccvenc_g_parm(struct file *file, void *priv,
			      struct v4l2_streamparm *a)
{
	tcvenc_step("get parm");

	if (a->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return -EINVAL;

	a->parm.output.capability = V4L2_CAP_TIMEPERFRAME;
	a->parm.output.timeperframe.denominator = 30;
	a->parm.output.timeperframe.numerator = 1;

	return 0;
}


int tccvenc_s_parm(struct file *file, void *fh, struct v4l2_streamparm *a)
{
	struct tcc_venc_ctx *ctx = fh;

	if (a->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return -EINVAL;

	ctx->framerate = 0;
	if (a->parm.output.timeperframe.denominator &&
	    a->parm.output.timeperframe.numerator) {
		ctx->framerate = a->parm.output.timeperframe.denominator /
		                 a->parm.output.timeperframe.numerator;
	}

	tcvenc_info("set framerate = %d", ctx->framerate);
	return 0;
}