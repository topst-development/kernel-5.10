/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef TCC_CAP_MEDIA_H
#define TCC_CAP_MEDIA_H

#include <linux/platform_device.h>
#include <media/media-device.h>
#include <media/v4l2-device.h>

struct tcc_cap_media {
	struct list_head list;
	struct platform_device *pdev;

	struct media_device md;
	struct v4l2_device v4l2_dev;

	struct media_pipeline pipe;
};

int tcc_cap_media_register_capture_dev(struct video_device *vdev,
				       const struct fwnode_handle *fwnode);
int tcc_cap_media_create_links(struct video_device *vfd);
int tcc_cap_media_call_sd_s_power(struct v4l2_subdev *sd, u32 onOff);
int tcc_cap_media_call_sd_init(struct v4l2_subdev *sd, u32 onOff);

#endif
