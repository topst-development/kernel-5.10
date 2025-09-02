// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *      tcc_dewarp_driver.c  --  Telechips On-the-Fly ODW Path Driver
 *
 *      Copyright (C) 2005-2010
 *          Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 ******************************************************************************


 *   Modified by Telechips Inc.


 *   Modified date : 2020


 *   Description : Driver management


 *****************************************************************************/

#include <linux/types.h>
#include <linux/videodev2.h>
#include <video/tcc-svdw.h>
#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <asm/unaligned.h>

#include <media/v4l2-common.h>

#include "tcc-dewarp-video.h"
#include "../tcc-camss/tcc-cap-media.h"

static inline struct platform_device *
device_to_platform_device(struct device *p_dev)
{
	return container_of(p_dev, struct platform_device, dev);
}

static inline struct tcc_dewarp_device *
device_to_tcc_dewarp_device(struct device *p_dev)
{
	const struct platform_device *pdev = NULL;

	pdev = device_to_platform_device(p_dev);

	return (struct tcc_dewarp_device *)platform_get_drvdata(pdev);
}

static inline struct tcc_dewarp_stream *
device_to_tcc_dewarp_stream(struct device *p_dev)
{
	struct tcc_dewarp_device *tdev = NULL;

	tdev = device_to_tcc_dewarp_device(p_dev);

	return &tdev->vstream;
}

/*
 * Delete the tcc_dewarp device.
 *
 * Called by the kernel when the last reference to the tcc_dewarp_device structure
 * is released.
 */
static void tcc_dewarp_delete(struct kref *p_ref)
{
	struct tcc_dewarp_device *tdev = NULL;
	const struct tcc_dewarp_stream *vstream = NULL;

	tdev = container_of(p_ref, struct tcc_dewarp_device, ref);
	vstream = &tdev->vstream;

	kfree(tdev);
}

static void tcc_dewarp_release(struct video_device *vdev)
{
	const struct device *p_dev = NULL;
	const struct tcc_dewarp_stream *vstream = NULL;
	struct tcc_dewarp_device *p_tdev = NULL;
	int ret = 0;

	vstream = (struct tcc_dewarp_stream *)video_get_drvdata(vdev);
	p_dev = stream_to_device(vstream);
	p_tdev = vstream->tdev;

	/* decrease reference count if device is unregistered */
	ret = kref_put(&p_tdev->ref, tcc_dewarp_delete);
	if (ret == 1) {
		/* the object was removed */
		logd(p_dev, "the object was removed\n");
	}
}

static int tcc_dewarp_init_stream_data(struct tcc_dewarp_stream *vstream,
				       const struct platform_device *p_pdev)
{
	const struct device *p_dev = NULL;
	int ret_call = 0;
	int ret = 0;

	p_dev = stream_to_device(vstream);

	ret_call = tcc_dewarp_video_init(vstream, p_pdev->dev.of_node);
	if (ret_call < 0) {
		loge(p_dev, "tcc_dewarp_video_init, ret: %d\n", ret_call);
		ret = -1;
	}

	mutex_init(&vstream->mlock);

	/* Set the driver data before calling video_register_device, otherwise
	 * tcc_dewarp_v4l2_open might race us.
	 */
	video_set_drvdata(&vstream->tdev->vdev, vstream);

	return ret;
}

static int tcc_dewarp_init_device_data(struct tcc_dewarp_device *p_tdev,
				       struct platform_device *p_pdev)
{
	const struct device *p_dev = NULL;
	struct tcc_dewarp_stream *vstream = NULL;
	int ret_call = 0;
	int ret = 0;

	p_dev = &p_pdev->dev;
	vstream = &p_tdev->vstream;

	p_tdev->pdev = p_pdev;
	p_tdev->id = of_alias_get_id(p_pdev->dev.of_node, "videoinput");
	(void)scnprintf((char *)p_tdev->name, PAGE_SIZE, p_pdev->name);
	logi(p_dev, "id: %d, name: %s\n", p_tdev->id, p_tdev->name);

	kref_init(&p_tdev->ref);

	vstream->tdev = p_tdev;
	ret_call = tcc_dewarp_init_stream_data(vstream, p_pdev);
	if (ret_call < 0) {
		loge(p_dev, "tcc_dewarp_init_stream_data, ret: %d\n", ret_call);
		ret = -1;
	}

	return ret;
}

static int tcc_dewarp_register_video_device_to_v4l2_device(
	struct tcc_dewarp_device *p_tdev)
{
	const struct device *p_dev = NULL;
	const struct tcc_dewarp_stream *vstream = NULL;
	const struct fwnode_handle *tccmd_fwnode = NULL;
	struct device_node *tccmd_of_node = NULL;
	int ret = 0;

	vstream = &p_tdev->vstream;
	p_dev = stream_to_device(vstream);

	tccmd_of_node =
		of_parse_phandle(p_tdev->pdev->dev.of_node, "mediadev", 0);
	if (tccmd_of_node == NULL) {
		/* error */
		ret = -ENODEV;
		loge(p_dev, "can not fine \"mediadev\". check device tree\n");
	}

	if (ret >= 0) {
		tccmd_fwnode = of_fwnode_handle(tccmd_of_node);
		if (tccmd_fwnode == NULL) {
			/* error */
			loge(p_dev, "wrong fwnode. check device tree\n");
			of_node_put(tccmd_of_node);
			ret = -ENODEV;
		}
	}

	if (ret >= 0) {
		ret = tcc_cap_media_register_capture_dev(&p_tdev->vdev,
							 tccmd_fwnode);
		if (ret != 0) {
			/* fail */
			loge(p_dev,
			     "tcc_cap_media_register_capture_dev, ret: %d\n",
			     ret);
		} else {
			p_tdev->tccmd =
				dev_get_drvdata(p_tdev->vdev.v4l2_dev->dev);
			logi(p_dev, "success register to the %s\n",
			     p_tdev->tccmd->v4l2_dev.name);
		}

		of_node_put(tccmd_of_node);
	}

	return ret;
}

static int
tcc_dewarp_register_video_device(struct tcc_dewarp_device *p_tdev,
				 const struct platform_device *p_pdev)
{
	struct device *p_dev = NULL;
	struct tcc_dewarp_stream *vstream = NULL;
	struct video_device *vdev = NULL;

	bool drop_corrupted = false;
	int ret_call = 0;
	int ret = 0;

	(void)p_dev;

	vstream = &p_tdev->vstream;
	p_dev = stream_to_device(vstream);
	vdev = &vstream->tdev->vdev;

	ret_call = tcc_dewarp_register_video_device_to_v4l2_device(p_tdev);
	if (ret_call != 0) {
		loge(p_dev,
		     "tcc_dewarp_register_video_device_to_v4l2_device, ret: %d\n",
		     ret_call);
		ret = ret_call;
	}

	ret_call = tcc_dewarp_v4l2_init_queue(&vstream->queue, drop_corrupted);
	if (ret_call != 0) {
		/* fail */
		loge(p_dev, "tcc_dewarp_v4l2_init_queue, ret: %d\n", ret_call);
		ret = -1;
	}

	/* We already hold a reference to dev->udev. The video device will be
	 * unregistered before the reference is released, so we don't need to
	 * get another one.
	 */
	vdev->dev_parent = p_dev;
	vdev->fops = &tcc_dewarp_fops;
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
	vdev->vfl_type = VFL_TYPE_VIDEO;
#else
	vdev->vfl_type = VFL_TYPE_GRABBER;
#endif
	vdev->ioctl_ops = &tcc_dewarp_ioctl_ops;
	vdev->minor = p_tdev->id;
	vdev->release = tcc_dewarp_release;
	vdev->device_caps =
		(u32)V4L2_CAP_STREAMING | (u32)V4L2_CAP_VIDEO_CAPTURE_MPLANE;
	(void)strscpy(vdev->name, (const char *)&p_tdev->name[0],
		      sizeof(vdev->name));

	p_tdev->pads[0].flags = MEDIA_PAD_FL_SINK;
	p_tdev->pads[1].flags = MEDIA_PAD_FL_SOURCE;

	ret_call =
		media_entity_pads_init(&p_tdev->vdev.entity, 2U, p_tdev->pads);
	if (ret_call < 0) {
		loge(p_dev, "media_entity_pads_init, ret: %d\n", ret_call);
		ret = -1;
	}

	ret_call = video_register_device(vdev, vdev->vfl_type, vdev->minor);
	if (ret_call < 0) {
		loge(p_dev, "video_register_device, ret: %d\n", ret_call);
		ret = -1;
	} else {
		/* increase reference count if device is registered */
		kref_get(&p_tdev->ref);
	}

	return ret;
}

static void
tcc_dewarp_unregister_video_device(const struct tcc_dewarp_device *tdev)
{
	const struct device *p_dev = NULL;
	const struct tcc_dewarp_stream *vstream = NULL;

	vstream = &tdev->vstream;
	p_dev = stream_to_device(vstream);

	if (video_is_registered(&vstream->tdev->vdev) != 0) {
		/* unregister video device  */
		video_unregister_device(&vstream->tdev->vdev);
	}
}

static int tcc_dewarp_probe_device(struct tcc_dewarp_device *tdev,
				   struct platform_device *pdev)
{
	const struct device *p_dev = NULL;
	int ret_call = 0;
	int ret = 0;

	p_dev = &pdev->dev;

	ret_call = tcc_dewarp_init_device_data(tdev, pdev);
	if (ret_call != 0) {
		loge(p_dev, "tcc_dewarp_init_device_data, ret: %d\n", ret_call);
		ret = -1;
	} else {
		ret_call = tcc_dewarp_register_video_device(tdev, pdev);
		if (ret_call != 0) {
			loge(p_dev,
			     "tcc_dewarp_register_video_device, ret: %d\n",
			     ret_call);
			ret = -1;
		}
	}

	return ret;
}

static void tcc_dewarp_remove_device(struct tcc_dewarp_device *tdev)
{
	const struct device *p_dev = NULL;
	const struct tcc_dewarp_stream *vstream = NULL;
	int ret_call = 0;

	vstream = &tdev->vstream;
	p_dev = stream_to_device(vstream);

	tcc_dewarp_unregister_video_device(tdev);

	ret_call = tcc_dewarp_video_deinit(vstream);
	if (ret_call != 0) {
		/* error: tcc_dewarp_video_deinit */
		loge(p_dev, "tcc_dewarp_video_deinit, ret: %d\n", ret_call);
	}
}

static int tcc_dewarp_notifier_bound(struct v4l2_async_notifier *notifier,
				     struct v4l2_subdev *subdev,
				     struct v4l2_async_subdev *asd)
{
	const struct device *p_dev = NULL;
	struct tcc_dewarp_subdev *tsubdev = NULL;
	const struct tcc_dewarp_device *tdev = NULL;
	const struct tcc_dewarp_stream *vstream = NULL;
	int ret = 0;

	tsubdev = container_of(notifier, struct tcc_dewarp_subdev, notifier);
	tdev = tsubdev->tdev;
	vstream = &tdev->vstream;
	p_dev = stream_to_device(vstream);

	logi(p_dev, "v4l2-subdev %s is bounded\n", subdev->name);

	/* register subdevice here */
	tsubdev->sd = subdev;

	return ret;
}

static int tcc_dewarp_notifier_complete(struct v4l2_async_notifier *notifier)
{
	const struct device *p_dev = NULL;
	const struct tcc_dewarp_subdev *tsubdev = NULL;
	struct tcc_dewarp_device *tdev = NULL;
	const struct tcc_dewarp_stream *vstream = NULL;
	int ret_call = 0;
	int ret = 0;

	tsubdev = container_of(notifier, struct tcc_dewarp_subdev, notifier);
	tdev = tsubdev->tdev;
	vstream = &tdev->vstream;
	p_dev = stream_to_device(vstream);

	ret_call = tcc_cap_media_create_links(&tdev->vdev);
	if (ret_call != 0) {
		loge(p_dev, "tcc_cap_media_create_links, ret: %d\n", ret_call);
		ret = -1;
	}

	ret_call = v4l2_device_register_subdev_nodes(tdev->vdev.v4l2_dev);
	if (ret_call != 0) {
		loge(p_dev, "v4l2_device_register_subdev_nodes, ret: %d\n",
		     ret_call);
		ret = -1;
	}

	return ret;
}

static const struct v4l2_async_notifier_operations tcc_dewarp_notifier_ops = {
	.bound = tcc_dewarp_notifier_bound,
	.complete = tcc_dewarp_notifier_complete,
};

static int tcc_dewarp_add_asd(struct tcc_dewarp_device *tdev)
{
	struct device *p_dev = NULL;
	const struct tcc_dewarp_stream *vstream = NULL;
	struct tcc_dewarp_subdev *tsubdev = NULL;
	int ret_call = 0;
	int ret = 0;

	vstream = &tdev->vstream;
	p_dev = stream_to_device(vstream);
	tsubdev = &tdev->tsubdev;

	v4l2_async_notifier_init(&tsubdev->notifier);

	tsubdev->notifier.ops = &tcc_dewarp_notifier_ops;
	tsubdev->tdev = tdev;

	/* add a v4l2-subdev to a notifier */
	ret_call = v4l2_async_notifier_parse_fwnode_endpoints_by_port(
		p_dev, &tsubdev->notifier, sizeof(struct v4l2_async_subdev), 0,
		NULL);
	if (ret_call != 0) {
		loge(p_dev,
		     "v4l2_async_notifier_parse_fwnode_endpoints, ret: %d\n",
		     ret_call);
		ret = -1;
	}

	return ret;
}

static int tcc_dewarp_register_asd_nf(struct tcc_dewarp_device *tdev)
{
	const struct device *p_dev = NULL;
	const struct tcc_dewarp_stream *vstream = NULL;
	struct tcc_dewarp_subdev *tsubdev = NULL;
	int ret_call = 0;
	int ret = 0;

	vstream = &tdev->vstream;
	p_dev = stream_to_device(vstream);
	tsubdev = &tdev->tsubdev;

	/* register a notifier */
	ret_call = v4l2_async_notifier_register(vstream->tdev->vdev.v4l2_dev,
						&tsubdev->notifier);
	if (ret_call != 0) {
		loge(p_dev, "v4l2_async_notifier_register, ret: %d\n",
		     ret_call);
		v4l2_async_notifier_cleanup(&tsubdev->notifier);
		ret = -1;
	}

	return ret;
}

static void tcc_dewarp_unregister_subdevs(struct tcc_dewarp_device *tdev)
{
	struct tcc_dewarp_subdev *tsubdev = NULL;

	tsubdev = &tdev->tsubdev;

	v4l2_async_notifier_unregister(&tsubdev->notifier);

	v4l2_async_notifier_cleanup(&tsubdev->notifier);
}

static int tcc_dewarp_probe_subdevs(struct tcc_dewarp_device *tdev,
				    const struct device_node *root_node)
{
	const struct device *p_dev = NULL;
	const struct tcc_dewarp_stream *vstream = NULL;
	int ret_call = 0;
	int ret = 0;

	vstream = &tdev->vstream;
	p_dev = stream_to_device(vstream);

	ret_call = tcc_dewarp_add_asd(tdev);
	if (ret_call != 0) {
		loge(p_dev, "tcc_dewarp_add_asd, ret: %d\n", ret_call);
		ret = -1;
	} else {
		ret_call = tcc_dewarp_register_asd_nf(tdev);
		if (ret_call != 0) {
			loge(p_dev, "tcc_dewarp_register_asd_nf, ret: %d\n",
			     ret_call);
			ret = -1;
		}
	}

	return ret;
}

static void tcc_dewarp_remove_subdevs(struct tcc_dewarp_device *tdev)
{
	tcc_dewarp_unregister_subdevs(tdev);
}

/* ------------------------------------------------------------------------
 * sysfs
 */

static ssize_t vs_stabilization_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	const struct tcc_dewarp_stream *vstream = NULL;

	vstream = device_to_tcc_dewarp_stream(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", vstream->vs_stabilization);
}

static ssize_t vs_stabilization_store(struct device *dev,

				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct tcc_dewarp_stream *vstream = NULL;
	u32 val = 0;
	int ret = 0;

	vstream = device_to_tcc_dewarp_stream(dev);

	ret = kstrtou32(buf, 0, &val);
	if (ret == 0) {
		/* update */
		vstream->vs_stabilization = val;
	}

	return (ret > 0) ? (ssize_t)ret : clamp_t(s64, count, 0, INT_MAX);
}

static DEVICE_ATTR_RW(vs_stabilization);

static ssize_t timestamp_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	const struct tcc_dewarp_stream *vstream = NULL;

	vstream = device_to_tcc_dewarp_stream(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", vstream->timestamp);
}

static ssize_t timestamp_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct tcc_dewarp_stream *vstream = NULL;
	u32 val = 0;
	int ret = 0;

	vstream = device_to_tcc_dewarp_stream(dev);

	ret = kstrtou32(buf, 0, &val);
	if (ret == 0) {
		/* update */
		vstream->timestamp = val;
	}

	return (ret > 0) ? (ssize_t)ret : clamp_t(s64, count, 0, INT_MAX);
}

static DEVICE_ATTR_RW(timestamp);

static struct attribute *tcc_dewarp_attrs[] = {
	&dev_attr_vs_stabilization.attr,
	&dev_attr_timestamp.attr,
	NULL,
};
ATTRIBUTE_GROUPS(tcc_dewarp);

/* ------------------------------------------------------------------------
 * Driver initialization and cleanup
 */

#ifdef CONFIG_VIDEO_TCC_SVDW
static int tcc_dewarp_svdw_start_streaming(void *p_data)
{
	int ret = 0;
	struct video_device *vdev = (struct video_device *)p_data;
	struct tcc_dewarp_device *dwrp_dev =
		(struct tcc_dewarp_device *)container_of(
			vdev, struct tcc_dewarp_device, vdev);

	dwrp_dev->svdw_dwp_unit.svdw_mode = true;

	ret = tcc_dewarp_video_streamon(&dwrp_dev->vstream);
	if (ret < 0) {
		loge(&dwrp_dev->pdev->dev,
		     "Failed on dewarp streamon with svdw mode (%d)\n", ret);
	}

	return ret;
}

static int tcc_dewarp_svdw_stop_streaming(void *p_data)
{
	int ret = 0;
	struct video_device *vdev = (struct video_device *)p_data;
	struct tcc_dewarp_device *dwrp_dev =
		(struct tcc_dewarp_device *)container_of(
			vdev, struct tcc_dewarp_device, vdev);

	ret = tcc_dewarp_video_streamoff(&dwrp_dev->vstream);
	if (ret < 0) {
		loge(&dwrp_dev->pdev->dev,
		     "Failed on dewarp streamon with svdw mode (%d)\n", ret);
	}

	dwrp_dev->svdw_dwp_unit.svdw_mode = false;

	return ret;
}

static int tcc_dewarp_svdw_g_phys_addr(void *p_data, phys_addr_t *p_addr)
{
	struct tcc_dewarp_stream *stream;
	struct tcc_dewarp_device *dwrp_dev;
	struct video_device *vdev;

	*p_addr = 0U;

	vdev = (struct video_device *)p_data;
	dwrp_dev = (struct tcc_dewarp_device *)container_of(
		vdev, struct tcc_dewarp_device, vdev);
	stream = &dwrp_dev->vstream;

	*p_addr = stream->dewarp_wrap.rsvd_mem[RESERVED_MEM_PREV]->base;
	return (*p_addr == 0) ? -ENOMEM : 0;
}

static int tcc_dewarp_svdw_s_fmt(void *p_data,
				 struct v4l2_pix_format_mplane *pix_mp)
{
	int ret = 0;
	struct tcc_dewarp_stream *stream;
	struct tcc_dewarp_device *dwrp_dev;
	struct video_device *vdev;

	vdev = (struct video_device *)p_data;
	dwrp_dev = (struct tcc_dewarp_device *)container_of(
		vdev, struct tcc_dewarp_device, vdev);
	stream = &dwrp_dev->vstream;

	memcpy(&stream->format.fmt.pix_mp, pix_mp, sizeof(*pix_mp));
	return ret;
}

const struct tcc_svdw_ops svdw_ops_tbl = {
	.start_streaming = tcc_dewarp_svdw_start_streaming,
	.stop_streaming = tcc_dewarp_svdw_stop_streaming,
	.g_phys_addr = tcc_dewarp_svdw_g_phys_addr,
	.s_fmt = tcc_dewarp_svdw_s_fmt,
};

static void add_dewarp_to_svdw_list(struct platform_device *pdev,
				    struct tcc_dewarp_device *tdev)
{
	INIT_LIST_HEAD(&tdev->svdw_dwp_unit.anchor);
	tdev->svdw_dwp_unit.vdev = &tdev->vdev;
	tdev->svdw_dwp_unit.dev = &pdev->dev;
	tdev->svdw_dwp_unit.ops = &svdw_ops_tbl;
	tdev->svdw_dwp_unit.svdw_mode = false;
	list_add_tail(&tdev->svdw_dwp_unit.anchor, &tcc_svdw_dewarp_list);
}
#endif

static int tcc_dewarp_probe(struct platform_device *pdev)
{
	const struct device *p_dev = NULL;
	struct tcc_dewarp_device *tdev = NULL;
	int ret_call = 0;
	int ret = 0;

	p_dev = &pdev->dev;

	tdev = kzalloc(sizeof(*tdev), GFP_KERNEL);
	if (tdev == NULL) {
		loge(p_dev, "kzalloc(tdev)\n");
		ret = -ENOMEM;
	} else {
		ret_call = tcc_dewarp_probe_device(tdev, pdev);
		if (ret_call != 0) {
			loge(p_dev, "tcc_dewarp_probe_device, ret: %d\n",
			     ret_call);
			ret = -1;
		} else {
			(void)tcc_dewarp_v4l2_init_format(tdev);

			ret_call = tcc_dewarp_probe_subdevs(tdev,
							    pdev->dev.of_node);
			if (ret_call != 0) {
				loge(p_dev,
				     "tcc_dewarp_probe_subdevs, ret: %d\n",
				     ret_call);
				ret = -1;
			}
		}

		platform_set_drvdata(pdev, tdev);

		tcc_dewarp_video_declare_coherent_dma_memory(&tdev->vstream);
#ifdef CONFIG_VIDEO_TCC_SVDW
		add_dewarp_to_svdw_list(pdev, tdev);
#endif
	}

	return ret;
}

static int tcc_dewarp_remove(struct platform_device *pdev)
{
	const struct device *p_dev = NULL;
	struct tcc_dewarp_device *tdev = NULL;
	int ret_call = 0;
	int ret = 0;

	p_dev = &pdev->dev;

	tdev = (struct tcc_dewarp_device *)platform_get_drvdata(pdev);

	tcc_dewarp_remove_subdevs(tdev);

	tcc_dewarp_remove_device(tdev);

	ret_call = kref_put(&tdev->ref, tcc_dewarp_delete);
	if (ret_call != 0) {
		loge(p_dev, "kref_put, ret: %d\n", ret);
		ret = -1;
	}

	return ret;
}

const static struct of_device_id tcc_dewarp_of_match[] = {
	{ .compatible = "telechips,dewarp" },
	{}
};

MODULE_DEVICE_TABLE(of, tcc_dewarp_of_match);

static struct platform_driver tcc_dewarp_driver = {
	.probe		= tcc_dewarp_probe,
	.remove		= tcc_dewarp_remove,
	.driver		= {
		.name		= DRIVER_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(tcc_dewarp_of_match),
		.dev_groups	= tcc_dewarp_groups,
	},
};

module_platform_driver(tcc_dewarp_driver);
MODULE_AUTHOR("Telechips.Co.Ltd");
MODULE_DESCRIPTION("Telechips Video-Input Path(V4L2-Capture) Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
