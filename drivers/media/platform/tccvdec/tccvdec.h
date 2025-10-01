/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef TCCVDEC_DRIVER_H
#define TCCVDEC_DRIVER_H

#include <linux/videodev2.h>

#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-fh.h>

#include <media/videobuf2-core.h>

#define TCCVDEC_DRIVER_NAME       "tccvdec"
#define TCCVDEC_DRIVER_VERSION    "1.0.0"
#define TCCVDEC_DEFAULT_WIDTH     (1920)
#define TCCVDEC_DEFAULT_HEIGHT    (1080)
#define TCCVDEC_DEFAULT_OUTNUMPLANE  (1)
#define TCCVDEC_DEFAULT_CAPNUMPLANE  (2)

#define TCCVDEC_WORKQUEUE_THROTTLE_MS   10
#define TCCVDEC_MAX_CAPTURE_BUFFER_NUM   28

#define SLEEP_TIME_MS   500

struct tcc_vdec_variant {
	unsigned int version;
	unsigned int port_num;
};


#endif //TCCVDEC_DRIVER_H
