/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef VOUT_IOCTL_H
#define VOUT_IOCTL_H

#include <linux/videodev2.h>

#define VIDIOC_USER_CLEAR_FRAME _IOW('V', BASE_VIDIOC_PRIVATE+1, int)
#define VIDIOC_USER_DISPLAY_LASTFRAME _IOW('V', BASE_VIDIOC_PRIVATE+2, int)
#define VIDIOC_USER_CAPTURE_LASTFRAME \
		_IOW('V', BASE_VIDIOC_PRIVATE+3, struct v4l2_buffer)
#define VIDIOC_USER_SET_OUTPUT_MODE _IOW('V', BASE_VIDIOC_PRIVATE+4, int)

#endif
