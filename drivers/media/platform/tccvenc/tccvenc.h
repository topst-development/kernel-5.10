/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef TCCVENC_DRIVER_H
#define TCCVENC_DRIVER_H

#include <linux/videodev2.h>

#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-core.h>

#include "vpu_enc_v3.h"

#define TCCVENC_DRIVER_NAME       "tccvenc"
#define TCCVENC_DRIVER_VERSION    "1.0.0"

struct tcc_venc_variant {
	unsigned int version;
	unsigned int port_num;
};

struct tcc_venc_dev {
	struct device *dev;
	struct v4l2_device	v4l2_dev;
	struct video_device	*vdev_dec;

	struct workqueue_struct *workqueue;

	struct platform_device	*plat_dev;

	struct mutex dev_mutex;
	atomic_t		busy;
};

struct tcc_venc_ctx {
	struct v4l2_fh fh;
	struct tcc_venc_dev *venc_dev;

	venc_handle_h venc_handle;

	struct v4l2_m2m_dev *m2m_dev;
	struct v4l2_m2m_ctx *m2m_ctx;

	struct v4l2_ctrl_handler ctrl_handler;

	struct v4l2_pix_format_mplane src_fmt;
	struct v4l2_pix_format_mplane dst_fmt;

	struct work_struct encode_work;

	u32 bitrate;
	u32 framerate; 
	u32 gop_size;
	u32 profile;
	u32 level;
	u32 hevc_profile;
	u32 hevc_level;

	bool put_header;
};


#endif //TCCVENC_DRIVER_H
