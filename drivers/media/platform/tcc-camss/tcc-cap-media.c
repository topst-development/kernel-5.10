// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_graph.h>
#include <media/v4l2-async.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>

#include "tcc-cap-media.h"
#include "tcc-cap-media-helper.h"

static LIST_HEAD(tccmd_list);

static DEFINE_MUTEX(list_lock);
static inline struct tcc_cap_media *v4l2dev2tcccapmd(struct v4l2_device *v4l2d)
{
	return container_of(v4l2d, struct tcc_cap_media, v4l2_dev);
}

int tcc_cap_media_register_capture_dev(struct video_device *vdev,
				       const struct fwnode_handle *fwnode)
{
	struct tcc_cap_media *tccmd;
	int ret = -ENODEV;

	list_for_each_entry (tccmd, &tccmd_list, list) {
		if (tccmd->pdev->dev.fwnode == fwnode) {
			/* okay */
			vdev->v4l2_dev = &tccmd->v4l2_dev;
			ret = 0;
			break;
		}
	}

	if (ret < 0) {
		pr_err("[ERROR][%s] - %s(%d)\n", LOG_TAG, __func__, __LINE__);
	}

	return ret;
}

EXPORT_SYMBOL_GPL(tcc_cap_media_register_capture_dev);

static struct v4l2_subdev *
tcc_cap_media_find_subdev_by_fwnode(struct tcc_cap_media *tccmd,
				    struct fwnode_handle *fwnode)
{
	struct v4l2_subdev *sd;

	list_for_each_entry (sd, &tccmd->v4l2_dev.subdevs, list) {
		if (sd->fwnode == fwnode) {
			return sd;
		}
	}

	return NULL;
}

static int tcc_cap_media_add_link(struct tcc_cap_media *tccmd,
				  struct v4l2_fwnode_link *link,
				  struct media_entity *l_ent,
				  struct media_entity *r_ent)
{
	struct media_entity *src_ent, *sink_ent;
	u32 src_pad, sink_pad;
	int ret = 0;

	if (l_ent->pads[link->local_port].flags & MEDIA_PAD_FL_SINK) {
		src_ent = r_ent;
		src_pad = link->remote_port;
		sink_ent = l_ent;
		sink_pad = link->local_port;
	} else {
		src_ent = l_ent;
		src_pad = link->local_port;
		sink_ent = r_ent;
		sink_pad = link->remote_port;
	}

	/* make sure link doesn't already exist before creating */
	if (media_entity_find_link(&src_ent->pads[src_pad],
				   &sink_ent->pads[sink_pad]) == NULL) {
		logi(&tccmd->pdev->dev, "add link %s:%d -> %s:%d\n",
		     src_ent->name, src_pad, sink_ent->name, sink_pad);

		ret = media_create_pad_link(
			src_ent, src_pad, sink_ent, sink_pad,
			(MEDIA_LNK_FL_ENABLED | MEDIA_LNK_FL_IMMUTABLE));
		if (ret < 0) {
			loge(&tccmd->pdev->dev,
			     "media_create_pad_link returned %d\n", ret);
		}
	}

	return ret;
}

static int tcc_cap_media_create_of_link(struct tcc_cap_media *tccmd,
					struct v4l2_subdev *sd,
					struct video_device *vfd,
					struct v4l2_fwnode_link *link)
{
	struct v4l2_subdev *up_sd;
	struct media_entity *l_ent;
	int ret = 0;

	l_ent = ((sd != NULL) ? (&sd->entity) : (&vfd->entity));

	/* check port number */
	if (link->local_port >= l_ent->num_pads) {
		loge(&tccmd->pdev->dev, "invalid port number(%d - %d)\n",
		     link->local_port, l_ent->num_pads);
		ret = -EINVAL;
	}

	/* find upstream sub-device and add link */
	if (ret >= 0) {
		up_sd = tcc_cap_media_find_subdev_by_fwnode(tccmd,
							    link->remote_node);
		if (up_sd != NULL) {
			ret = tcc_cap_media_add_link(tccmd, link, l_ent,
						     &up_sd->entity);
			if (ret < 0) {
				loge(&tccmd->pdev->dev,
				     "tcc_cap_media_add_link returned %d\n",
				     ret);
			}
		}
	}

	return ret;
}

static int tcc_cap_media_create_default_links(struct tcc_cap_media *tccmd,
					      struct v4l2_subdev *sd,
					      struct video_device *vfd)
{
	struct v4l2_fwnode_link flink;
	struct device *dev;
	struct device_node *ep;
	int ret;

	dev = ((sd != NULL) ? (sd->dev) : (vfd->dev_parent));

	for_each_endpoint_of_node (dev->of_node, ep) {
		ret = v4l2_fwnode_parse_link(of_fwnode_handle(ep), &flink);
		if (ret < 0) {
			/* skip */
			loge(&tccmd->pdev->dev, "skip %s node\n", ep->name);
			continue;
		}

		ret = tcc_cap_media_create_of_link(tccmd, sd, vfd, &flink);
		if (ret < 0) {
			/* error */
			loge(&tccmd->pdev->dev,
			     "tcc_cap_media_create_of_link returned %d\n", ret);
			v4l2_fwnode_put_link(&flink);
			break;
		}
		v4l2_fwnode_put_link(&flink);
	}

	return 0;
}

static inline u16 tcc_cap_media_get_num_pads(const struct v4l2_subdev *s)
{
	return s->entity.num_pads;
}

static inline struct v4l2_subdev *entity2sd(const struct media_entity *e)
{
	return container_of(e, struct v4l2_subdev, entity);
}

static int tcc_cap_media_set_pad(struct tcc_cap_media *tccmd,
				 struct media_link *l)
{
	struct v4l2_device *v4l2_dev;
	struct v4l2_subdev_format format;
	struct v4l2_subdev *src, *sink;
	int ret = 0;

	v4l2_dev = entity2sd(l->source->entity)->v4l2_dev;

	if (is_media_entity_v4l2_subdev(l->sink->entity)) {
		src = entity2sd(l->source->entity);
		sink = entity2sd(l->sink->entity);

		logd(&tccmd->pdev->dev,
		     "propagation of pad format %s:%d -> %s:%d\n", src->name,
		     l->source->index, sink->name, l->sink->index);

		/* get format of source pad */
		format.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		format.pad = l->source->index;

		ret = v4l2_subdev_call(src, pad, get_fmt, NULL, &format);
		if (ret < 0) {
			/* error */
			loge(&tccmd->pdev->dev, "%s - %s returned %d\n",
			     src->name, "get_fmt", ret);
		}

		/* set format of sink pad using linked source pad */
		if (ret >= 0) {
			format.pad = l->sink->index;

			ret = v4l2_subdev_call(sink, pad, set_fmt, NULL,
					       &format);
			if (ret < 0) {
				/* error */
				loge(&tccmd->pdev->dev, "%s - %s returned %d\n",
				     sink->name, "set_fmt", ret);
			}
		}

		/* find the link of source pad and call _config_pad */
		if (ret >= 0) {
			struct media_link *next_link;

			list_for_each_entry (next_link, &sink->entity.links,
					     list) {
				if (next_link->source->entity ==
				    &sink->entity) {
					ret = tcc_cap_media_set_pad(tccmd,
								    next_link);
					if (ret < 0) {
						/* error */
						loge(&tccmd->pdev->dev,
						     "recursive call(%s) returned %d\n",
						     next_link->sink->entity
							     ->name,
						     ret);
					}
				}
			}
		}
	}

	return ret;
}

int tcc_cap_media_create_links(struct video_device *vfd)
{
	struct tcc_cap_media *tccmd = v4l2dev2tcccapmd(vfd->v4l2_dev);
	struct media_link *link;
	struct v4l2_subdev *sd;
	int ret = 0;

	/* link between bridge device and sub-device */
	ret = tcc_cap_media_create_default_links(tccmd, NULL, vfd);
	if (ret < 0) {
		/* error */
		loge(&tccmd->pdev->dev,
		     "tcc_cap_media_create_default_links returned %d\n", ret);
	}

	/* link between sub-devices */
	if (ret >= 0) {
		list_for_each_entry (sd, &tccmd->v4l2_dev.subdevs, list) {
			ret = tcc_cap_media_create_default_links(tccmd, sd,
								 NULL);
			if (ret < 0) {
				/* error */
				loge(&tccmd->pdev->dev,
				     "tcc_cap_media_create_default_links returned %d\n",
				     ret);
				break;
			}
		}
	}

	/* find sensor sub-device and set pad format of linked sub-device */
	if (ret >= 0) {
		list_for_each_entry (sd, &tccmd->v4l2_dev.subdevs, list) {
			if (tcc_cap_media_get_num_pads(sd) == 1U) {
				logi(&tccmd->pdev->dev,
				     "video stream source device is %s\n",
				     sd->name);

				link = container_of(sd->entity.links.next,
						    struct media_link, list);

				ret = tcc_cap_media_set_pad(tccmd, link);
				if (ret < 0) {
					/* error */
					loge(&tccmd->pdev->dev,
					     "tcc_cap_media_set_pad returned %d\n",
					     ret);
				}
			}
		}
	}

	return ret;
}

EXPORT_SYMBOL_GPL(tcc_cap_media_create_links);

int tcc_cap_media_call_sd_s_power(struct v4l2_subdev *sd, u32 onOff)
{
	struct tcc_cap_media *tccmd;
	struct media_link *flink;
	struct v4l2_subdev *fsd;
	int ret = 0;

	tccmd = v4l2dev2tcccapmd(sd->v4l2_dev);

	if (onOff != 0U && onOff != 1U) {
		loge(&tccmd->pdev->dev, "Unexpected onOff: %u\n", onOff);
		return -EINVAL;
	}

	/* call current s_power call of sub-device */
	logi(&tccmd->pdev->dev, "call %s s_power\n", sd->name);

	ret = v4l2_subdev_call(sd, core, s_power, onOff);
	if (ret < 0) {
		if (ret == -ENOIOCTLCMD) {
			logd(&tccmd->pdev->dev,
			     "s_power of %s is not created\n", sd->name);
			ret = 0;
		} else {
			/* error */
			loge(&tccmd->pdev->dev, "s_power of %s returned %d\n",
			     sd->name, ret);
		}
	}

	/* find sub-device which is linked to the sink pad and call s_power */

	list_for_each_entry (flink, &sd->entity.links, list) {
		if (flink->sink->entity == &sd->entity) {
			fsd = media_entity_to_v4l2_subdev(
				flink->source->entity);
			tcc_cap_media_call_sd_s_power(fsd, onOff);
			if (ret < 0) {
				/* error */
				loge(&tccmd->pdev->dev,
				     "recursive call(%s) returned %d\n",
				     fsd->name, ret);
			}
		}
	}

	return ret;
}

EXPORT_SYMBOL_GPL(tcc_cap_media_call_sd_s_power);

int tcc_cap_media_call_sd_init(struct v4l2_subdev *sd, u32 onOff)
{
	struct tcc_cap_media *tccmd;
	struct media_link *flink;
	struct v4l2_subdev *fsd;
	int ret = 0;

	tccmd = v4l2dev2tcccapmd(sd->v4l2_dev);

	/* call current init call of sub-device */
	logi(&tccmd->pdev->dev, "call %s init\n", sd->name);

	ret = v4l2_subdev_call(sd, core, init, onOff);
	if (ret < 0) {
		if (ret == -ENOIOCTLCMD) {
			logd(&tccmd->pdev->dev, "init of %s is not created\n",
			     sd->name);
			ret = 0;
		} else {
			/* error */
			loge(&tccmd->pdev->dev, "init of %s returned %d\n",
			     sd->name, ret);
		}
	}

	/* find sub-device which is linked to the sink pad and call init */

	list_for_each_entry (flink, &sd->entity.links, list) {
		if (flink->sink->entity == &sd->entity) {
			fsd = media_entity_to_v4l2_subdev(
				flink->source->entity);
			ret = tcc_cap_media_call_sd_init(fsd, onOff);
			if (ret < 0) {
				/* error */
				loge(&tccmd->pdev->dev,
				     "recursive call(%s) returned %d\n",
				     fsd->name, ret);
			}
		}
	}

	return ret;
}

EXPORT_SYMBOL_GPL(tcc_cap_media_call_sd_init);

static int tcc_cap_media_init(struct tcc_cap_media *tccmd)
{
	int ret = 0;

	INIT_LIST_HEAD(&tccmd->list);

	/* step 1: init media device */
	strscpy(tccmd->md.model, "Telechips capture subsystem",
		sizeof(tccmd->md.model));
	snprintf(tccmd->md.bus_info, sizeof(tccmd->md.bus_info), "platform:%s",
		 "camera sub-system");
	tccmd->md.dev = &tccmd->pdev->dev;
	media_device_init(&tccmd->md);

	/* step 2: register v4l2 device */
	tccmd->v4l2_dev.mdev = &tccmd->md;
	strscpy(tccmd->v4l2_dev.name, tccmd->pdev->name,
		sizeof(tccmd->v4l2_dev.name));
	ret = v4l2_device_register(&tccmd->pdev->dev, &tccmd->v4l2_dev);
	if (ret < 0) {
		/* error */
		loge(&tccmd->pdev->dev, "v4l2_device_register returned %d\n",
		     ret);
		media_device_cleanup(&tccmd->md);
	}

	/* step 3: register media device */
	if (ret >= 0) {
		ret = media_device_register(&tccmd->md);
		if (ret < 0) {
			/* error */
			loge(&tccmd->pdev->dev,
			     "error - media_device_register returned %d\n",
			     ret);
			v4l2_device_unregister(&tccmd->v4l2_dev);
			media_device_cleanup(&tccmd->md);
		}
	}

	return ret;
}

static void tcc_cap_media_deinit(struct tcc_cap_media *tccmd)
{
	media_device_unregister(&tccmd->md);
	v4l2_device_unregister(&tccmd->v4l2_dev);
	media_device_cleanup(&tccmd->md);
}

static int tcc_cap_media_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	//struct device_node *node = dev->of_node;
	struct tcc_cap_media *tccmd = NULL;
	int ret = 0;

	tccmd = devm_kzalloc(dev, sizeof(*tccmd), GFP_KERNEL);
	if (tccmd == NULL) {
		/* error */
		ret = -ENOMEM;
	} else {
		/* okay */
		tccmd->pdev = pdev;
		platform_set_drvdata(pdev, tccmd);
	}

	if (ret >= 0) {
		ret = tcc_cap_media_init(tccmd);
		if (ret < 0) {
			/* error */
			loge(&tccmd->pdev->dev,
			     "tcc_cap_media_init returned %d\n", ret);
		} else {
			mutex_lock(&list_lock);
			list_add_tail(&tccmd->list, &tccmd_list);
			mutex_unlock(&list_lock);
		}
	}

	logi(&tccmd->pdev->dev, "%s probe of driver: %s device: %s\n",
	     ((ret >= 0) ? "success" : "fail"), pdev->dev.driver->name,
	     pdev->name);

	return ret;
}

static int tcc_cap_media_remove(struct platform_device *pdev)
{
	struct tcc_cap_media *tccmd =
		(struct tcc_cap_media *)platform_get_drvdata(pdev);

	logi(&tccmd->pdev->dev, "Removing %s-%s\n", pdev->dev.driver->name,
	     pdev->name);

	tcc_cap_media_deinit(tccmd);
	list_del(&tccmd->list);
	devm_kfree(&tccmd->pdev->dev, tccmd);

	return 0;
}

static const struct of_device_id tcc_cap_media_dt_ids[] = {
	{ .compatible = "telechips,tcc-cap-media" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, tcc_cap_media_dt_ids);

static struct platform_driver tcc_cap_media_driver = {
	.probe		= tcc_cap_media_probe,
	.remove		= tcc_cap_media_remove,
	.driver		= {
		.name	= TCC_CAP_MEDIA_DRIVER_NAME,
		.of_match_table	= tcc_cap_media_dt_ids,
	},
};

module_platform_driver(tcc_cap_media_driver);
MODULE_DESCRIPTION("Telechips media controller driver");
MODULE_LICENSE("GPL");
