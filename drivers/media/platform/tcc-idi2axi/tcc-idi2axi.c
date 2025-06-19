// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/version.h>

#include <media/v4l2-ioctl.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/videobuf2-dma-contig.h>

#include "tcc-idi2axi.h"
#include "tcc-idi2axi-helper.h"

#ifdef CONFIG_ARCH_TCC750X
#include "750x/tcc-idi2axi-dev.h"
#endif

static int tcc_idi2axi_register_video_device_to_v4l2_device(
	struct tcc_idi2axi_state *state)
{
	const struct fwnode_handle *tccmd_fwnode = NULL;
	struct device_node *tccmd_of_node = NULL;
	int ret = 0;

	tccmd_of_node =
		of_parse_phandle(state->pdev->dev.of_node, "mediadev", 0);
	if (tccmd_of_node == NULL) {
		/* error */
		ret = -ENODEV;
		loge(&(state->pdev->dev),
		     "can not fine \"mediadev\". check device tree\n");
	}

	if (ret >= 0) {
		tccmd_fwnode = of_fwnode_handle(tccmd_of_node);
		if (tccmd_fwnode == NULL) {
			/* error */
			loge(&(state->pdev->dev),
			     "wrong fwnode. check device tree\n");
			of_node_put(tccmd_of_node);
			ret = -ENODEV;
		}
	}

	if (ret >= 0) {
		ret = tcc_cap_media_register_capture_dev(&(state->vdev),
							 tccmd_fwnode);
		if (ret != 0) {
			/* fail */
			loge(&(state->pdev->dev),
			     "tcc_cap_media_register_capture_dev, ret: %d\n",
			     ret);
		} else {
			state->tccmd =
				dev_get_drvdata(state->vdev.v4l2_dev->dev);
			logi(&(state->pdev->dev), "success register to the %s\n",
			     state->tccmd->v4l2_dev.name);
		}

		of_node_put(tccmd_of_node);
	}

	return ret;
}


/* ------------------------------------------------------------------------
 * media_entity_operations
 */
int tcc_idi2axi_link_validate(struct media_link *link)
{
	struct v4l2_subdev *sd =
		media_entity_to_v4l2_subdev(link->source->entity);
	struct video_device *vdev =
		container_of(link->sink->entity, struct video_device, entity);
	struct tcc_idi2axi_state *state = video_get_drvdata(vdev);
	struct v4l2_subdev_format sd_fmt;
	int ret;

	sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	sd_fmt.pad = link->source->index;

	ret = v4l2_subdev_call(sd, pad, get_fmt, NULL, &sd_fmt);
	if (ret) {
		/* error */
		loge(&(state->pdev->dev), "get_fmt returned %d\n", ret);
	} else {
		logd(&(state->pdev->dev),
		     "link validate: " "%s:src:%dx%d " "%s:snk:%dx%d\n",
		     /* src */
		     link->source->entity->name,
		     sd_fmt.format.width, sd_fmt.format.height,
		     /* sink */
		     link->sink->entity->name,
		     state->format.width, state->format.height);

		/* The width, height and pixelformat must match. */
		if ((sd_fmt.format.width != state->format.width) ||
		    (sd_fmt.format.height != state->format.height)) {
			loge(&(state->pdev->dev),
			     "resolution does not match\n");
			ret = -EPIPE;
		    }
	}

	return ret;
}

static const struct media_entity_operations tcc_idi2axi_mops = {
	.link_validate = tcc_idi2axi_link_validate,
};


/* ------------------------------------------------------------------------
 * v4l2_file_operations
 */
static const struct v4l2_file_operations tcc_idi2axi_fops = {
	.owner		= THIS_MODULE,
	.open		= v4l2_fh_open,
	.release	= vb2_fop_release,
	.read           = vb2_fop_read,
	.poll		= vb2_fop_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap           = vb2_fop_mmap,
};


/* ------------------------------------------------------------------------
 * v4l2_ioctl_ops
 */
static int tcc_idi2axi_querycap(struct file *file, void *fh,
				struct v4l2_capability *cap)
{
	const struct tcc_idi2axi_state *state = NULL;
	int ret = 0;

	state = (const struct tcc_idi2axi_state *)video_drvdata(file);

	strscpy(cap->driver, TCC_IDI2AXI_DRIVER_NAME, sizeof(cap->driver));
	strscpy(cap->card, KBUILD_MODNAME, sizeof(cap->card));
	strscpy(cap->bus_info, "platform:idi2axi", sizeof(cap->bus_info));

	logd(&(state->pdev->dev), "driver: %s\n", cap->driver);
	logd(&(state->pdev->dev), "card: %s\n", cap->card);
	logd(&(state->pdev->dev), "bus_info: %s\n", cap->bus_info);

	return ret;
}

static int tcc_idi2axi_enum_fmt_vid_cap(struct file *file, void *fh,
					struct v4l2_fmtdesc *f)
{
	const struct tcc_idi2axi_state *state = NULL;
	int ret = 0;

	state = (const struct tcc_idi2axi_state *)video_drvdata(file);

	/* TODO:
	 * make the supported format list and check request format with it.
	 */
	logw(&(state->pdev->dev), " NOT IMPLEMENTATION\n");

	return ret;
}

static int tcc_idi2axi_g_fmt_vid_cap(struct file *file, void *fh,
				     struct v4l2_format *f)
{
	const struct tcc_idi2axi_state *state = NULL;
	int ret = 0;

	state = (const struct tcc_idi2axi_state *)video_drvdata(file);

	f->fmt.pix = state->format;

	return ret;
}

static int tcc_idi2axi_try_fmt_vid_cap(struct file *file, void *fh,
				       struct v4l2_format *f)
{
	const struct tcc_idi2axi_state *state = NULL;
	int ret = 0;

	state = (const struct tcc_idi2axi_state *)video_drvdata(file);

	tcc_idi2axi_dev_try_fmt(state, f);

	return ret;
}

static int tcc_idi2axi_s_fmt_vid_cap(struct file *file, void *fh,
				     struct v4l2_format *f)
{
	struct tcc_idi2axi_state *state = NULL;
	int ret = 0;

	state = (struct tcc_idi2axi_state *)video_drvdata(file);

	/* Do not change the format while stream is on */
	if (vb2_is_busy(&(state->queue))) {
		/* busy status */
		ret = -EBUSY;
	}

	if (ret >= 0) {
		ret = tcc_idi2axi_try_fmt_vid_cap(file, fh, f);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
			     "tcc_idi2axi_try_fmt_vid_cap returned %d\n", ret);
		}
	}

	if (ret >= 0) {
		logd(&(state->pdev->dev),
		     "format update: "
		     "old:%dx%d (0x%x, %d, %d, %d, %d, %d) "
		     "new:%dx%d (0x%x, %d, %d, %d, %d, %d)\n",
		     /* old */
		     state->format.width, state->format.height,
		     state->format.pixelformat, state->format.colorspace,
		     state->format.quantization, state->format.xfer_func,
		     state->format.ycbcr_enc, state->format.bytesperline,
		     /* new */
		     f->fmt.pix.width, f->fmt.pix.height,
		     f->fmt.pix.pixelformat, f->fmt.pix.colorspace,
		     f->fmt.pix.quantization, f->fmt.pix.xfer_func,
		     f->fmt.pix.ycbcr_enc, f->fmt.pix.bytesperline);

		state->format = f->fmt.pix;
	}

	return ret;
}

static int tcc_idi2axi_enum_framesizes(struct file *file, void *fh,
				       struct v4l2_frmsizeenum *fsize)
{
	const struct tcc_idi2axi_state *state = NULL;
	int ret = 0;

	state = (const struct tcc_idi2axi_state *)video_drvdata(file);

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = state->format.width;
	fsize->discrete.height = state->format.height;

	return ret;
}

static const struct v4l2_ioctl_ops tcc_idi2axi_ioctl_ops = {
	/* VIDIOC_QUERYCAP handler */
	.vidioc_querycap = tcc_idi2axi_querycap,
	/* VIDIOC_ENUM_FMT handlers */
	.vidioc_enum_fmt_vid_cap = tcc_idi2axi_enum_fmt_vid_cap,
	/* VIDIOC_G_FMT handlers */
	.vidioc_g_fmt_vid_cap = tcc_idi2axi_g_fmt_vid_cap,
	/* VIDIOC_S_FMT handlers */
	.vidioc_s_fmt_vid_cap = tcc_idi2axi_s_fmt_vid_cap,
	/* VIDIOC_TRY_FMT handlers */
	.vidioc_try_fmt_vid_cap = tcc_idi2axi_try_fmt_vid_cap,
	/* Debugging ioctls */
	.vidioc_enum_framesizes = tcc_idi2axi_enum_framesizes,
	/* Buffer handlers */
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_expbuf = vb2_ioctl_expbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_create_bufs = vb2_ioctl_create_bufs,
	.vidioc_prepare_buf = vb2_ioctl_prepare_buf,
	/* Stream on/off */
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
};


/* -----------------------------------------------------------------------------
 * v4l2_subdev
 */
static int tcc_idi2axi_nf_bound(struct v4l2_async_notifier *notifier,
				struct v4l2_subdev *subdev,
				struct v4l2_async_subdev *asd)
{
	struct tcc_idi2axi_state *state = NULL;
	int ret = 0;

	state = container_of(notifier, struct tcc_idi2axi_state, nf);

	logi(&(state->pdev->dev),
	     "v4l2-subdev %s is bounded\n", subdev->name);

	/* register subdevice here */
	state->sd = subdev;

	return ret;
}

static int tcc_idi2axi_nf_complete(struct v4l2_async_notifier *notifier)
{
	struct tcc_idi2axi_state *state = NULL;
	int ret = 0;

	state = container_of(notifier, struct tcc_idi2axi_state, nf);

	ret = tcc_cap_media_create_links(&(state->vdev));
	if (ret != 0) {
		/* error */
		loge(&(state->pdev->dev),
		     "tcc_cap_media_create_links returned %d\n", ret);
	}

	ret = v4l2_device_register_subdev_nodes(state->vdev.v4l2_dev);
	if (ret != 0) {
		/* error */
		loge(&(state->pdev->dev),
		     "v4l2_device_register_subdev_nodes returned %d\n",
		     ret);
	}

	return ret;
}

static const struct v4l2_async_notifier_operations tcc_idi2axi_nf_ops = {
	.bound = tcc_idi2axi_nf_bound,
	.complete = tcc_idi2axi_nf_complete,
};

static int tcc_idi2axi_add_asd_to_nf(struct tcc_idi2axi_state *state)
{
	int ret = 0;

	state->nf.ops = &tcc_idi2axi_nf_ops;
	v4l2_async_notifier_init(&(state->nf));

	/* add a v4l2-subdev to a notifier */
	ret = v4l2_async_notifier_parse_fwnode_endpoints(
		&(state->pdev->dev), &(state->nf),
		sizeof(struct v4l2_async_subdev), NULL);
	if (ret != 0) {
		/* error */
		loge(&(state->pdev->dev),
		     "v4l2_async_notifier_parse_fwnode_endpoints returned %d\n",
		     ret);
	}

	return ret;
}

static int tcc_idi2axi_register_asd_nf(struct tcc_idi2axi_state *state)
{
	int ret = 0;

	/* register a notifier */
	ret = v4l2_async_notifier_register(state->vdev.v4l2_dev,
					   &(state->nf));
	if (ret != 0) {
		/* error */
		loge(&(state->pdev->dev),
		     "v4l2_async_notifier_register returned %d\n",
		     ret);
		v4l2_async_notifier_cleanup(&(state->nf));
	}

	return ret;
}

static int tcc_idi2axi_attach_subdevs(struct tcc_idi2axi_state *state)
{
	int ret = 0;

	ret = tcc_idi2axi_add_asd_to_nf(state);
	if (ret != 0) {
		/* error */
		loge(&(state->pdev->dev),
		     "tcc_idi2axi_add_asd returned %d\n", ret);
	} else {
		ret = tcc_idi2axi_register_asd_nf(state);
		if (ret != 0) {
			/* error */
			loge(&(state->pdev->dev),
			     "tcc_idi2axi_register_asd_nf returned %d\n", ret);
		}
	}

	return ret;
}
int tcc_idi2axi_subdev_core_s_power(struct v4l2_subdev *sd, int on)
{
	int res = -EINVAL;

	if ((on == 0) || (on == 1)) {
		res = tcc_cap_media_call_sd_s_power(sd, (u32)on);
	}
	return res;
}

int tcc_idi2axi_subdev_core_init(struct v4l2_subdev *sd, u32 val)
{
	return tcc_cap_media_call_sd_init(sd, val);
}

int tcc_idi2axi_subdev_video_s_stream(struct v4l2_subdev *sd, int enable)
{
	return v4l2_subdev_call(sd, video, s_stream, enable);
}

int tcc_idi2axi_streamon_subdevs(struct tcc_idi2axi_state *state)
{
	int ret = 0;

	ret = tcc_idi2axi_subdev_core_s_power(state->sd, 1);
	if (ret != 0) {
		/* error */
		logd(&(state->pdev->dev),
		     "tcc_idi2axi_subdev_core_s_power returned %d\n", ret);
	}
	ret = tcc_idi2axi_subdev_core_init(state->sd, 1);
	if (ret != 0) {
		/* error */
		logd(&(state->pdev->dev),
		     "tcc_idi2axi_subdev_core_init returned %d\n", ret);
	}
	ret = tcc_idi2axi_subdev_video_s_stream(state->sd, 1);
	if (ret != 0) {
		/* error */
		logd(&(state->pdev->dev),
		     "tcc_idi2axi_subdev_video_s_stream returned %d\n", ret);
	}

	return ret;
}

void tcc_idi2axi_streamoff_subdevs(struct tcc_idi2axi_state *state)
{
	int ret = 0;

	ret = tcc_idi2axi_subdev_video_s_stream(state->sd, 0);
	if (ret != 0) {
		/* error */
		logd(&(state->pdev->dev),
		     "tcc_idi2axi_subdev_video_s_stream returned %d\n", ret);
	}

	ret = tcc_idi2axi_subdev_core_init(state->sd, 0);
	if (ret != 0) {
		/* error */
		logd(&(state->pdev->dev),
		     "tcc_idi2axi_subdev_core_init returned %d\n", ret);
	}

	ret = tcc_idi2axi_subdev_core_s_power(state->sd, 0);
	if (ret != 0) {
		/* error */
		logd(&(state->pdev->dev),
		     "tcc_idi2axi_subdev_core_s_power returned %d\n", ret);
	}
}


/* -----------------------------------------------------------------------------
 * vb2_ops
 */
static int tcc_idi2axi_queue_setup(struct vb2_queue *vq, u32 *nbuffers,
				   u32 *nplanes, u32 sizes[],
				   struct device *alloc_devs[])
{
	struct tcc_idi2axi_state *state;
	int ret = 0;

	state = (struct tcc_idi2axi_state *)vb2_get_drv_priv(vq);

	/* update num_planes */
	if (*nplanes == 0U) {
		/* Support only single plane */
		*nplanes = 1U;
		sizes[0U] = state->format.sizeimage;
	}

	return ret;
}

static int tcc_idi2axi_buffer_prepare(struct vb2_buffer *vb)
{
	struct tcc_idi2axi_state *state;
	unsigned long size;
	int ret = 0;

	state = (struct tcc_idi2axi_state *)vb2_get_drv_priv(vb->vb2_queue);

	size = state->format.sizeimage;
	if (vb2_plane_size(vb, 0) < size) {
		loge(&(state->pdev->dev), "buffer too small (%lu < %lu)\n",
		     vb2_plane_size(vb, 0), size);
		ret = -EINVAL;
	}

	return ret;
}

static void tcc_idi2axi_return_all_buffers(struct tcc_idi2axi_state *state,
					   enum vb2_buffer_state buffer_state)
{
	struct tcc_idi2axi_buffer *vbuf, *node;

	spin_lock(&(state->qlock));

	list_for_each_entry_safe(vbuf, node, &(state->buf_list), list) {
		list_del(&vbuf->list);
		vb2_buffer_done(&(vbuf->vb2.vb2_buf), buffer_state);
	}

	if (state->buf_set_dma != NULL) {
		vb2_buffer_done(&(state->buf_set_dma->vb2.vb2_buf),
				buffer_state);
		state->buf_set_dma = NULL;
	}

	spin_unlock(&(state->qlock));
}

static int tcc_idi2axi_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct tcc_idi2axi_state *state;
	int ret = 0;

	state = (struct tcc_idi2axi_state *)vb2_get_drv_priv(vq);

	state->sequence = 0;

	/* Start the media pipeline */
	ret = media_pipeline_start(&(state->vdev.entity),
				   &(state->tccmd->pipe));
	if (ret < 0) {
		/* error */
		loge(&(state->pdev->dev),
		     "media_pipeline_start returned %d\n", ret);
		tcc_idi2axi_return_all_buffers(state, VB2_BUF_STATE_QUEUED);
	}

	if (ret >= 0) {
		ret = tcc_idi2axi_dev_streamon(state);
		if (ret < 0) {
			loge(&(state->pdev->dev),
			     "tcc_idi2axi_dev_streamon returned %d\n", ret);
			media_pipeline_stop(&(state->vdev.entity));
			tcc_idi2axi_return_all_buffers(state,
						       VB2_BUF_STATE_QUEUED);
		}
	}

	if (ret >= 0) {
		ret = tcc_idi2axi_streamon_subdevs(state);
		if (ret < 0) {
			loge(&(state->pdev->dev),
			     "tcc_idi2axi_streamon_subdevs returned %d\n", ret);
			tcc_idi2axi_dev_streamoff(state);
			media_pipeline_stop(&(state->vdev.entity));
			tcc_idi2axi_return_all_buffers(state,
						       VB2_BUF_STATE_QUEUED);
		}
	}

	return ret;
}

static void tcc_idi2axi_stop_streaming(struct vb2_queue *vq)
{
	struct tcc_idi2axi_state *state;

	state = (struct tcc_idi2axi_state *)vb2_get_drv_priv(vq);

	tcc_idi2axi_streamoff_subdevs(state);

	tcc_idi2axi_dev_streamoff(state);

	/* Stop the media pipeline */
	media_pipeline_stop(&(state->vdev.entity));

	/* Release all active buffers */
	tcc_idi2axi_return_all_buffers(state, VB2_BUF_STATE_ERROR);
}

static void tcc_idi2axi_buf_queue(struct vb2_buffer *vb2)
{
	struct tcc_idi2axi_state *state;
	struct tcc_idi2axi_buffer *buf;

	state = (struct tcc_idi2axi_state *)vb2_get_drv_priv(vb2->vb2_queue);
	buf = container_of(vb2, struct tcc_idi2axi_buffer, vb2.vb2_buf);

	spin_lock(&(state->qlock));
	list_add_tail(&(buf->list), &(state->buf_list));
	spin_unlock(&(state->qlock));

	logd(&(state->pdev->dev), "queued buffer 0x%llx\n",
	     vb2_dma_contig_plane_dma_addr(vb2, 0U));
}

static const struct vb2_ops tcc_idi2axi_qops = {
	.queue_setup		= tcc_idi2axi_queue_setup,
	/*
	 * Since q->lock is set we can use the standard
	 * vb2_ops_wait_prepare/finish helper functions.
	 */
	.wait_prepare		= vb2_ops_wait_prepare,
	.wait_finish		= vb2_ops_wait_finish,
	.buf_prepare		= tcc_idi2axi_buffer_prepare,
	.start_streaming	= tcc_idi2axi_start_streaming,
	.stop_streaming		= tcc_idi2axi_stop_streaming,
	.buf_queue		= tcc_idi2axi_buf_queue,
};


/* ------------------------------------------------------------------------
 * Driver initialization and cleanup
 */
static int tcc_idi2axi_probe(struct platform_device *pdev)
{
	struct tcc_idi2axi_state *state = NULL;
	int ret = 0;

	/* allocate and clear memory for a device */
	state = devm_kzalloc(&(pdev->dev), sizeof(*state), GFP_KERNEL);
	if (state == NULL) {
		/* error */
		loge(&(pdev->dev), "devm_kzalloc returned NULL\n");
		ret = -ENOMEM;
	}

	/* parse device tree */
	if (ret >= 0) {
		state->pdev = pdev;
		platform_set_drvdata(pdev, state);

		ret = tcc_idi2axi_dev_parse_dt(state);
		if (ret < 0) {
			loge(&(state->pdev->dev),
			     "tcc_idi2axi_dev_parse_dt returned %d\n", ret);
		}
	}

	/* Assigne reserved memory to this device */
	if (ret >= 0) {
		ret = of_reserved_mem_device_init(&(state->pdev->dev));
		if (ret < 0) {
			loge(&(state->pdev->dev),
			     "of_reserved_mem_device_init returned %d\n", ret);
		}
	}

	/* Register this device to the tcc-cap-media device */
	if (ret >= 0) {
		ret = tcc_idi2axi_register_video_device_to_v4l2_device(state);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
			     "tcc_idi2axi_register_video_device_to_v4l2_device returned %d\n",
			     ret);
		}
	}

	/* Initialize the media entity */
	if (ret >= 0) {
		state->vdev.entity.name = "IDI2AXI";
		state->vdev.entity.function = MEDIA_ENT_F_IO_V4L;
		//state->vdev.entity.ops = &tcc_idi2axi_mops;
		state->pad.flags = MEDIA_PAD_FL_SINK;

		ret = media_entity_pads_init(&(state->vdev.entity), 1U,
					     &(state->pad));
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
			     "media_entity_pads_init returned %d\n", ret);
		}
	}

	/* Initialize the vb2 queue */
	if (ret >= 0) {
		state->queue.dev = &(state->pdev->dev);
		state->queue.type = (u32)V4L2_BUF_TYPE_VIDEO_CAPTURE;
		state->queue.io_modes =
			(u32)VB2_MMAP | (u32)VB2_DMABUF | (u32)VB2_USERPTR;
		state->queue.drv_priv = state;
		state->queue.buf_struct_size =
			(u32)sizeof(struct tcc_idi2axi_buffer);
		state->queue.ops = &tcc_idi2axi_qops;
		state->queue.mem_ops = &vb2_dma_contig_memops;
		state->queue.timestamp_flags =
			(u32)V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
		state->queue.lock = &(state->lock);
		state->queue.min_buffers_needed = 1U;

		/* Initialize buffer list and its lock */
		INIT_LIST_HEAD(&(state->buf_list));
		spin_lock_init(&(state->qlock));

		ret = vb2_queue_init(&(state->queue));
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev), "vb2_queue_init returned %d\n", ret);
		}
	}

	/* Initialize the video_device struct */
	if (ret >= 0) {
		mutex_init(&(state->lock));

		state->vdev.minor =
			of_alias_get_id(state->pdev->dev.of_node, "videoinput");
		(void)strscpy(state->vdev.name, state->pdev->name,
			      sizeof(state->vdev.name));
		state->vdev.dev_parent = &(state->pdev->dev);
		state->vdev.vfl_type = VFL_TYPE_VIDEO;
		state->vdev.device_caps = (u32)V4L2_CAP_VIDEO_CAPTURE | 
					  (u32)V4L2_CAP_STREAMING |
					  (u32)V4L2_CAP_IO_MC;
		state->vdev.release = video_device_release_empty;
		state->vdev.fops = &tcc_idi2axi_fops;
		state->vdev.ioctl_ops = &tcc_idi2axi_ioctl_ops;
		state->vdev.lock = &(state->lock);
		state->vdev.queue = &(state->queue);
		//state->vdev.v4l2_dev = ;	
		state->vdev.vfl_dir = VFL_DIR_RX;
		video_set_drvdata(&(state->vdev), state);

		ret = video_register_device(&(state->vdev),
					    state->vdev.vfl_type,
					    state->vdev.minor);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
			     "video_register_device returned %d\n", ret);
		}
	}

	/* find and attach sub-devices */
	if (ret >= 0) {
		ret = tcc_idi2axi_attach_subdevs(state);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
			     "tcc_idi2axi_attach_subdevs returned %d",
			      ret);
		}
	}

	/* request irq */
	if (ret >= 0) {
		tcc_idi2axi_dev_disable(state);

		ret = devm_request_irq(&(state->pdev->dev), state->intr.num,
				       tcc_idi2axi_dev_isr, 0U,
				       dev_name(&(state->pdev->dev)), state);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
					"devm_request_irq returned %d",
					ret);
		}
	}

	if (ret >= 0) {
		logi(&(state->pdev->dev), "Success probe IDI2AXI\n");
	}

	return ret;
}

static int tcc_idi2axi_remove(struct platform_device *pdev)
{
	struct tcc_idi2axi_state *state = NULL;
	int ret = 0;

	state = (struct tcc_idi2axi_state *)platform_get_drvdata(pdev);
	if (state == NULL) {
		/* error */
		loge(&(pdev->dev), "platform_get_drvdata returned NULL\n");
		ret = -ENOMEM;
	}

	return ret;
}

const static struct of_device_id tcc_idi2axi_of_match[] = {
	{ .compatible = "telechips,idi2axi" },
	{}
};

MODULE_DEVICE_TABLE(of, tcc_idi2axi_of_match);

static struct platform_driver tcc_idi2axi_driver = {
	.probe		= tcc_idi2axi_probe,
	.remove		= tcc_idi2axi_remove,
	.driver		= {
		.name		= TCC_IDI2AXI_DRIVER_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(tcc_idi2axi_of_match),
	},
};

module_platform_driver(tcc_idi2axi_driver);
MODULE_AUTHOR("Telechips.Co.Ltd");
MODULE_DESCRIPTION("Telechips IDI2AXI Driver");
MODULE_LICENSE("GPL");
