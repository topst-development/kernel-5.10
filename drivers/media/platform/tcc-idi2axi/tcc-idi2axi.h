/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_IDI2AXI_H
#define TCC_IDI2AXI_H

#include <linux/mutex.h>
#include <linux/platform_device.h>

#include <media/v4l2-common.h>
#include <media/videobuf2-v4l2.h>

#include "../tcc-camss/tcc-cap-media.h"

#define TCC_IDI2AXI_DRIVER_NAME "tcc-idi2axi"

struct tcc_idi2axi_buffer {
	/*
	 * struct vb2_v4l2_buffer must be the first element
	 * the videobuf2 framework will allocate this struct based on
	 * buf_struct_size and use the first sizeof(struct vb2_buffer) bytes of
	 * memory as a vb2_buffer
	 */
	struct vb2_v4l2_buffer vb2;
	struct list_head list;
};

struct tcc_idi2axi_clk {
	struct clk *axi;
	struct clk *pix;
	struct clk *apb;
};

struct tcc_idi2axi_intr {
	u32 num;
	bool is_registered;
};

struct tcc_idi2axi_prop {
	const char * const name;
	uint32_t *out;
	bool is_def;
	uint32_t def;
};

struct tcc_idi2axi_set {
	u32 sscnt;
	u32 i2xm;
};

struct tcc_idi2axi_state {
	struct platform_device *pdev;
	struct tcc_cap_media *tccmd;

	struct video_device vdev;
	struct mutex lock;
	struct media_pad pad;

	/* buffer related information */
	spinlock_t qlock;
	struct vb2_queue queue;
	struct list_head buf_list;
	struct tcc_idi2axi_buffer *buf_set_dma;
	struct v4l2_pix_format format;
	u32 sequence;

	/* device specific information */
	void __iomem *idi2axi_base;
	struct tcc_idi2axi_clk clk;
	struct tcc_idi2axi_intr intr;
	struct tcc_idi2axi_set sets;

	/*
	 * sub-device related information
	 * source device is implemented using sub-device
	 */
	struct v4l2_async_subdev asd;
	struct v4l2_async_notifier nf;
	struct v4l2_subdev *sd;
	struct v4l2_subdev_format fmt;
};

#endif