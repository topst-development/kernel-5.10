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

#include "tccvenc.h"
#include "tccvenc_fmt.h"
#include "tccvenc_queue.h"
#include "tccvenc_ctrls.h"
#include "tccvenc_debug.h"

#include <linux/fs.h>
#include <linux/uaccess.h>

static int tcc_venc_open    (struct file *file);
static int tcc_venc_release (struct file *file);
static int tcc_venc_probe   (struct platform_device *pdev);
static int tcc_venc_remove  (struct platform_device *pdev);
static int tcc_venc_querycap(struct file *file, void *priv, struct v4l2_capability *cap);

/*For M2M Workqueue*/
static void tcc_venc_device_run (void *priv);
static int  tcc_venc_job_ready	(void *priv);
static void tcc_venc_worker		(struct work_struct *work);



static struct of_device_id tccvenc_of_match[] = {
	{ .compatible = "telechips,v4l2_venc" },
	{}
};

static const struct v4l2_file_operations tcc_venc_fops = {
	.owner = THIS_MODULE,
	.open = tcc_venc_open,
	.release = tcc_venc_release,
	.unlocked_ioctl = video_ioctl2,
	.poll = v4l2_m2m_fop_poll,
	.mmap = v4l2_m2m_fop_mmap,
};

static int tccvenc_enum_framesizes(struct file *file, void *fh,
                                   struct v4l2_frmsizeenum *fsize)
{
	tcvenc_step("called: index=%u, pixel_format=0x%08x", fsize->index, fsize->pixel_format);

	if (fsize->index > 0) {
		tcvenc_step("invalid index: %u", fsize->index);
		return -EINVAL;
	}

	switch (fsize->pixel_format) {
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_YUV420:
		fsize->type = V4L2_FRMSIZE_TYPE_CONTINUOUS;
		fsize->stepwise.min_width = 64;
		fsize->stepwise.max_width = 3840;
		fsize->stepwise.min_height = 64;
		fsize->stepwise.max_height = 2160;
		fsize->stepwise.step_width = 2;
		fsize->stepwise.step_height = 2;
		tcvenc_step("supported format: set stepwise range (128x128) ~ (1920x1080)");
		break;
	case V4L2_PIX_FMT_H264:
			fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
			fsize->stepwise.min_width = 64;
			fsize->stepwise.max_width = 1920;
			fsize->stepwise.min_height = 64;
			fsize->stepwise.max_height = 1088;
			fsize->stepwise.step_width = 2;
			fsize->stepwise.step_height = 2;
			break;
	case V4L2_PIX_FMT_HEVC:
			fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
			fsize->stepwise.min_width = 256;
			fsize->stepwise.max_width = 3840;
			fsize->stepwise.min_height = 128;
			fsize->stepwise.max_height = 2160;
			fsize->stepwise.step_width = 2;
			fsize->stepwise.step_height = 2;
			break;
	default:
		tcvenc_step("unsupported pixel_format: 0x%08x", fsize->pixel_format);
		return -EINVAL;
	}

	return 0;
}
static const struct v4l2_ioctl_ops tcc_venc_ioctl_ops = {
	// Format Enumeration
	.vidioc_enum_fmt_vid_out  = tccvenc_enum_fmt,
	.vidioc_enum_fmt_vid_cap  = tccvenc_enum_fmt,

	// TRY_FMT
	.vidioc_try_fmt_vid_out_mplane = tccvenc_try_fmt,
	.vidioc_try_fmt_vid_cap_mplane = tccvenc_try_fmt,

	// S_FMT
	.vidioc_s_fmt_vid_out_mplane = tccvenc_s_fmt,
	.vidioc_s_fmt_vid_cap_mplane = tccvenc_s_fmt,

	.vidioc_s_parm				 = tccvenc_s_parm,

	// G_FMT
	.vidioc_g_fmt_vid_out_mplane = tccvenc_g_fmt,
	.vidioc_g_fmt_vid_cap_mplane = tccvenc_g_fmt,

	// Buffer handling via mem2mem helper
	.vidioc_reqbufs        = v4l2_m2m_ioctl_reqbufs,
	.vidioc_querybuf       = v4l2_m2m_ioctl_querybuf,
	.vidioc_qbuf           = v4l2_m2m_ioctl_qbuf,
	.vidioc_dqbuf          = v4l2_m2m_ioctl_dqbuf,
	.vidioc_create_bufs    = v4l2_m2m_ioctl_create_bufs,
	.vidioc_prepare_buf    = v4l2_m2m_ioctl_prepare_buf,
	.vidioc_expbuf         = v4l2_m2m_ioctl_expbuf,

	// Stream control
	.vidioc_streamon       = v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff      = v4l2_m2m_ioctl_streamoff,

	// Events
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,

	.vidioc_querycap 		= tcc_venc_querycap,
	.vidioc_enum_framesizes	= tccvenc_enum_framesizes,
};

static struct tcc_venc_variant venc_drvdata = {
	.version = 0x61,
	.port_num = 1,
};

static struct platform_device_id venc_driver_ids[] = {
	{
		.name = "tcc-encoder",
		.driver_data = (unsigned long)&venc_drvdata,
	},
	{},
};


static struct platform_driver tcc_venc_driver = {
	.probe = tcc_venc_probe,
	.remove = tcc_venc_remove,
	.id_table = venc_driver_ids,
	.driver = {
		.name = "tcc-venc",
		.owner = THIS_MODULE,
		.of_match_table = tccvenc_of_match,
	},
};

static int tcc_venc_querycap(struct file *file, void *priv, struct v4l2_capability *cap)
{
	strscpy(cap->driver, "tcc-venc", sizeof(cap->driver));
	strscpy(cap->card, "Telechips V4L2 Encoder", sizeof(cap->card));
	strscpy(cap->bus_info, "platform:tcc-venc", sizeof(cap->bus_info));

	cap->device_caps = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static int tcc_venc_job_ready(void *priv)
{
	struct tcc_venc_ctx *ctx = priv;
	int src_ready = v4l2_m2m_num_src_bufs_ready(ctx->m2m_ctx);
	int dst_ready = v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx);

	tcvenc_step("job_ready: src_ready=%d, dst_ready=%d", src_ready, dst_ready);

	return (src_ready > 0 && dst_ready > 0);
}

static void tcc_venc_device_run(void *priv)
{
	struct tcc_venc_ctx *ctx = priv;
	struct tcc_venc_dev *dev = ctx->venc_dev;

	queue_work(dev->workqueue, &ctx->encode_work);
}

static void tcc_venc_get_ycbcr_addr(struct tcc_venc_ctx *ctx,
                                    venc_input_t *input,
                                    struct vb2_buffer *src_vb)
{
	struct device *dev = ctx->venc_dev->dev;
	uint32_t y_size;
	char fourcc[5] = { 0 };

	memcpy(fourcc, &ctx->src_fmt.pixelformat, 4);
	tcvenc_step("get_ycbcr: fmt=%s planes=%u",
				fourcc, ctx->src_fmt.num_planes);

	input->input_y = (void *)dma_to_phys(dev, vb2_dma_contig_plane_dma_addr(src_vb, 0));

	if (ctx->src_fmt.pixelformat == V4L2_PIX_FMT_NV12) {
		if (ctx->src_fmt.num_planes == 1) {
			y_size = ctx->src_fmt.width * ctx->src_fmt.height;
			input->input_crcb[0] = input->input_y + y_size;

			tcvenc_step("NV12-1P  Y=0x%pad  CbCr=0x%pad", &input->input_y, &input->input_crcb[0]);
		} else {
			input->input_crcb[0] = (void *)dma_to_phys(dev, vb2_dma_contig_plane_dma_addr(src_vb, 1));
			tcvenc_step("NV12-2P  Y=0x%pad  CbCr=0x%pad", &input->input_y, &input->input_crcb[0]);
		}

	} else if (ctx->src_fmt.pixelformat == V4L2_PIX_FMT_YUV420) {
		if (ctx->src_fmt.num_planes == 1) {
			uint32_t uv_size;

			y_size  = ctx->src_fmt.width * ctx->src_fmt.height;
			uv_size = y_size / 4;

			input->input_crcb[0] = input->input_y       + y_size;
			input->input_crcb[1] = input->input_crcb[0] + uv_size;

			tcvenc_step("I420-1P  Y=0x%pad  U=0x%pad  V=0x%pad",
						&input->input_y,
						&input->input_crcb[0],
								&input->input_crcb[1]);
		} else {
				input->input_crcb[0] = (void *)dma_to_phys(dev, vb2_dma_contig_plane_dma_addr(src_vb, 1));
				input->input_crcb[1] = (void *)dma_to_phys(dev, vb2_dma_contig_plane_dma_addr(src_vb, 2));

				tcvenc_step("I420-3P  Y=0x%pad  U=0x%pad  V=0x%pad",
							&input->input_y,
							&input->input_crcb[0],
							&input->input_crcb[1]);
		}
	}
}

static void tcc_venc_worker(struct work_struct *work)
{
	struct tcc_venc_ctx *ctx = container_of(work, struct tcc_venc_ctx, encode_work);
	struct vb2_v4l2_buffer *src, *dst;
	struct vb2_buffer *src_vb, *dst_vb;

	venc_input_t input = {0};
	venc_output_t output = {0};
	//venc_seq_header_t seq_header = {0};
	int ret;

	src = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	dst = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);

	if (!src || !dst) {
		tcvenc_err("Missing buffer\n");
		return;
	}

	src_vb = &src->vb2_buf;
	dst_vb = &dst->vb2_buf;

	//if (ctx->put_header) {
	//	ret = venc_put_seqheader(ctx->venc_handle, &seq_header);
	//	if (ret < 0) {
	//		tcvenc_err("Failed to put sequence header\n");
	//		goto error;
	//	}
	//	ctx->put_header = false;
	//	tcvenc_info("Sequence Header size = %d bytes", seq_header.seq_header_out_size);
	//}
//
	tcc_venc_get_ycbcr_addr(ctx, &input, src_vb);

	ret = venc_encode(ctx->venc_handle, &input, &output);
	if (ret < 0) {
		tcvenc_err("venc_encode failed");
		goto error;
	}

	if (output.bitstream_out_size == 0) {
		tcvenc_err("No encoded output. Possibly delayed or failed frame.");
		v4l2_m2m_buf_done(dst, VB2_BUF_STATE_ERROR);
		v4l2_m2m_buf_done(src, VB2_BUF_STATE_DONE);
		goto finish;
	}

	{
		void *dst_buf;
		size_t total_size = 0;

		dst_buf = vb2_plane_vaddr(dst_vb, 0);
		if (!dst_buf) {
			tcvenc_err("Failed to get destination plane vaddr");
			goto error;
		}

		//if (ctx->put_header) {
		//	memcpy(dst_buf, seq_header.seq_header_out, seq_header.seq_header_out_size);
		//	memcpy(dst_buf + seq_header.seq_header_out_size,
		//	       output.bitstream_out, output.bitstream_out_size);
		//	total_size = seq_header.seq_header_out_size + output.bitstream_out_size;
		//	tcvenc_info("Wrote SEQ+BITSTREAM (%zu bytes)", total_size);
		//	ctx->put_header = false;
		//} else 
		{
			memcpy(dst_buf, output.bitstream_out, output.bitstream_out_size);
			total_size = output.bitstream_out_size;
			tcvenc_dbg("Wrote BITSTREAM (%zu bytes)", total_size);
		}

		dst_vb->planes[0].bytesused = total_size;
		dst_vb->timestamp = src_vb->timestamp;
		vb2_set_plane_payload(dst_vb, 0, dst_vb->planes[0].bytesused);	
	}

	v4l2_m2m_src_buf_remove_by_buf(ctx->fh.m2m_ctx, src);
	v4l2_m2m_buf_done(dst, VB2_BUF_STATE_DONE);
	v4l2_m2m_dst_buf_remove_by_buf(ctx->fh.m2m_ctx, dst);
	v4l2_m2m_buf_done(src, VB2_BUF_STATE_DONE);
finish:
	v4l2_m2m_job_finish(ctx->m2m_dev, ctx->m2m_ctx);
	return;

error:
	v4l2_m2m_buf_done(dst, VB2_BUF_STATE_ERROR);
	v4l2_m2m_buf_done(src, VB2_BUF_STATE_DONE);
	v4l2_m2m_job_finish(ctx->m2m_dev, ctx->m2m_ctx);
}

const struct v4l2_m2m_ops tccvenc_m2m_ops = {
	.device_run = tcc_venc_device_run,
	.job_ready  = tcc_venc_job_ready,
	.job_abort = NULL,
};

static int tcc_venc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tcc_venc_dev *tcc_dev;
	struct video_device *vdev;
	int ret = -ENOENT;

	tcvenc_step("probe : pdev=%p", pdev);

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
		alloc_ordered_workqueue("tcvenc",
			WQ_MEM_RECLAIM | WQ_FREEZABLE | WQ_HIGHPRI);
	if (!tcc_dev->workqueue) {
		tcvenc_err("Failed to create decode workqueue");
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
	mutex_init(&tcc_dev->dev_mutex);

	ret = v4l2_device_register(&pdev->dev, &tcc_dev->v4l2_dev);
	if (ret) {
		goto exit_free_dev;
	}

	/* encoder */
	vdev = video_device_alloc();
	if (!vdev) {
		v4l2_err(&tcc_dev->v4l2_dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto exit_free_dev;
	}

	strscpy(vdev->name, "tcc-video-encoder", sizeof(vdev->name));
	vdev->fops = &tcc_venc_fops,
	vdev->ioctl_ops = &tcc_venc_ioctl_ops;
	vdev->release = video_device_release;
	vdev->lock = &tcc_dev->dev_mutex;
	vdev->vfl_dir = VFL_DIR_M2M;
	vdev->device_caps	= V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;

	vdev->v4l2_dev = &tcc_dev->v4l2_dev;

	vdev->minor = 8;
	tcc_dev->vdev_dec = vdev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
	ret = video_register_device(vdev, VFL_TYPE_VIDEO, vdev->minor);
#else
	ret = video_register_device(vdev, VFL_TYPE_GRABBER, vdev->minor);
#endif

	if (ret) {
		tcvenc_err("Failed to register video device\n ");
		goto exit_dec_reg;
	}
	v4l2_info(&tcc_dev->v4l2_dev, "encoder registered as /dev/video%d\n", vdev->num);
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
	tcvenc_err("with error ");
	return ret;
}

static int tcc_venc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tcc_venc_dev *tcc_dev = platform_get_drvdata(pdev);

	v4l2_info(&tcc_dev->v4l2_dev, "Removing %s\n", pdev->name);
	tcvenc_step("remove : pdev=%p(%s)", pdev, pdev->name);

	flush_workqueue(tcc_dev->workqueue);
	destroy_workqueue(tcc_dev->workqueue);
	video_unregister_device(tcc_dev->vdev_dec);
	v4l2_device_unregister(&tcc_dev->v4l2_dev);

	of_reserved_mem_device_release(dev);

	kfree(tcc_dev);

	return 0;
}

static int tcc_venc_open(struct file *file)
{
	struct tcc_venc_dev *dev = video_drvdata(file);
	struct tcc_venc_ctx *ctx;

	tcvenc_step("open: dev=%p", dev);

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->venc_dev = dev;

	v4l2_fh_init(&ctx->fh, video_devdata(file));
	file->private_data = &ctx->fh;
	ctx->fh.ctrl_handler = &ctx->ctrl_handler;

	v4l2_ctrl_handler_init(&ctx->ctrl_handler, 0);
	v4l2_fh_add(&ctx->fh);
	
	tccvenc_ctrls_init(ctx);

	ctx->m2m_dev = v4l2_m2m_init(&tccvenc_m2m_ops);
	if (IS_ERR(ctx->m2m_dev)) {
		int ret = PTR_ERR(ctx->m2m_dev);
		tcvenc_err("m2m init fail: %d\n", ret);
		goto err_free;
	}

	ctx->m2m_ctx = v4l2_m2m_ctx_init(ctx->m2m_dev, ctx, tccvenc_queue_init);
	if (IS_ERR(ctx->m2m_ctx)) {
		int ret = PTR_ERR(ctx->m2m_ctx);
		v4l2_m2m_release(ctx->m2m_dev);
		tcvenc_err("ctx init fail: %d\n", ret);
		goto err_free;
	}

	ctx->fh.m2m_ctx = ctx->m2m_ctx;

	INIT_WORK(&ctx->encode_work, tcc_venc_worker);

	ctx->venc_handle = venc_alloc_instance();
	ctx->put_header  = true; 

	return 0;

err_free:
	v4l2_ctrl_handler_free(&ctx->ctrl_handler);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx);
	return -ENOMEM;
}

static int tcc_venc_release(struct file *file)
{
	struct v4l2_fh *fh = file->private_data;
	struct tcc_venc_ctx *ctx = container_of(fh, struct tcc_venc_ctx, fh);

	tcvenc_step("release");

	v4l2_m2m_ctx_release(ctx->m2m_ctx);
	v4l2_m2m_release(ctx->m2m_dev);

	v4l2_ctrl_handler_free(&ctx->ctrl_handler);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);

	venc_release_instance(ctx->venc_handle);

	kfree(ctx);
	return 0;
}

MODULE_DEVICE_TABLE(of, tccvenc_of_match);

module_platform_driver(tcc_venc_driver);

MODULE_AUTHOR("Telechips.Co.Ltd");

MODULE_DESCRIPTION("Telechips Video Encoder Driver");

MODULE_LICENSE("GPL");

MODULE_VERSION(TCCVENC_DRIVER_VERSION);
