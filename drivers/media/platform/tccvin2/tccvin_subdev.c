// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *      tccvin_subdev.c  --  sub video device
 *
 *      Copyright (C) 2005-2010
 *          Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 ******************************************************************************


 *   Modified by Telechips Inc.


 *   Modified date : 2020


 *   Description : Sub Video Device handling


 *****************************************************************************/

#include <media/v4l2-subdev.h>
#include <linux/module.h>
#include <linux/version.h>

#include "tccvin_common.h"
#include "tccvin_video.h"
#include "../tcc-camss/tcc-cap-media.h"

int tccvin_subdev_get_src_sd(const struct tccvin_device *tdev,
			     const struct media_pad *local_pad,
			     struct v4l2_subdev **src_sd)
{
	const struct media_pad *src_pad = NULL;
	int ret = 0;

	src_pad = media_entity_remote_pad(local_pad);
	if (src_pad == (struct media_pad *)0) {
		/* error */
		loge(&(tdev->pdev->dev), "Failed to find remote source pad\n");
		ret = -ENOLINK;
	} else if (!is_media_entity_v4l2_subdev(src_pad->entity)) {
		/* error */
		loge(&(tdev->pdev->dev),
		     "Upstream entity is not a v4l2 subdev\n");
		ret = -ENODEV;
	} else {
		*src_sd = media_entity_to_v4l2_subdev(src_pad->entity);
	}

	return ret;
}

/*
 * v4l2_subdev_core_ops
 */
int tccvin_subdev_core_init(struct v4l2_subdev *sd, u32 val)
{
	return tcc_cap_media_call_sd_init(sd, val);
}

int tccvin_subdev_core_load_fw(struct v4l2_subdev *sd)
{
	return v4l2_subdev_call(sd, core, load_fw);
}

int tccvin_subdev_core_s_power(struct v4l2_subdev *sd, int on)
{
	int res = -EINVAL;

	if ((on == 0) || (on == 1)) {
		res = tcc_cap_media_call_sd_s_power(sd, (u32)on);
	}
	return res;
}

/*
 * v4l2_subdev_video_ops
 */
int tccvin_subdev_video_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	return v4l2_subdev_call(sd, video, g_input_status, status);
}

int tccvin_subdev_video_s_stream(struct v4l2_subdev *sd, int enable)
{
	return v4l2_subdev_call(sd, video, s_stream, enable);
}

int tccvin_subdev_video_g_dv_timings(struct v4l2_subdev *sd,
				     struct v4l2_dv_timings *timings)
{
	return v4l2_subdev_call(sd, video, g_dv_timings, timings);
}

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
int tccvin_subdev_video_g_mbus_config(struct v4l2_subdev *sd, unsigned int idx,
				      struct v4l2_mbus_config *cfg)
{
	return v4l2_subdev_call(sd, pad, get_mbus_config, idx, cfg);
}
#else
int tccvin_subdev_video_g_mbus_config(struct v4l2_subdev *sd,
				      struct v4l2_mbus_config *cfg)
{
	return v4l2_subdev_call(sd, video, g_mbus_config, cfg);
}
#endif

/*
 * v4l2_subdev_pad_ops
 */
int tccvin_subdev_pad_get_fmt(struct v4l2_subdev *sd,
			      struct v4l2_subdev_format *format)
{
	int ret = 0;

	if (!PTR_ERR_OR_ZERO(sd) && !PTR_ERR_OR_ZERO(format)) {
		ret = v4l2_subdev_call(sd, pad, get_fmt, NULL, format);
	} else {
		WARN_ON(sd == NULL);
		WARN_ON(format == NULL);
	}

	return ret;
}

int tccvin_subdev_pad_set_fmt(struct v4l2_subdev *sd,
			      struct v4l2_subdev_pad_config *cfg,
			      struct v4l2_subdev_format *format)
{
	return v4l2_subdev_call(sd, pad, set_fmt, cfg, format);
}
