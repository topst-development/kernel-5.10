/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_DEWARP_VIDEO_H
#define TCC_DEWARP_VIDEO_H

#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/of_device.h>
#include <linux/videodev2.h>
#include <linux/version.h>
#include <video/tcc-svdw.h>
#include <media/media-device.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-fwnode.h>
#include <media/videobuf2-v4l2.h>
#include <media/media-entity.h>

#ifdef CONFIG_ARCH_TCC807X
#include "807x/tcc-dewarp-odw.h"
#endif
#ifdef CONFIG_ARCH_TCC750X
#include "750x/tcc-dewarp-odw.h"
#endif
#include "../tcc-camss/tcc-cap-media.h"

#include <video/telechips/tcc_cam_ioctrl.h>
/* reserved memory */
#include <linux/of_reserved_mem.h>

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/dma-mapping.h>

/* ------------------------------------------------------------------------
 * Driver specific constants.
 */
#define DRIVER_NAME "tcc-dewarp"
#define DRIVER_VERSION "2.0.0"

/* vioc path */
#define MAX_BUFFERS (4)
#define MAX_PLANES (3)

#define DEFAULT_FRAMEWIDTH (640U)
#define DEFAULT_FRAMEHEIGHT (480U)

#define MAX_FRAMEWIDTH (8192U)
#define MAX_FRAMEHEIGHT (8192U)

#define PGL_FORMAT (VIOC_IMG_FMT_ARGB8888)
#define PGL_BG_R (0xFFU)
#define PGL_BG_G (0xFFU)
#define PGL_BG_B (0xFFU)
#define PGL_BGM_R (PGL_BG_R & 0xF8U)
#define PGL_BGM_G (PGL_BG_G & 0xF8U)
#define PGL_BGM_B (PGL_BG_B & 0xF8U)

enum odw_device_parse {
	ODW_DEV_ID,
	ODW_DEV_BYPASS,
	ODW_DEV_IRQ,
	ODW_DEV_MAX,
};

enum reserved_memory {
	RESERVED_MEM_PGL,
	RESERVED_MEM_PREV,
	RESERVED_MEM_MAX,
};

enum tcc_dewarp_buffer_state {
	TCC_DEWARP_BUF_STATE_IDLE,
	TCC_DEWARP_BUF_STATE_QUEUED,
	TCC_DEWARP_BUF_STATE_ACTIVE,
	TCC_DEWARP_BUF_STATE_READY,
	TCC_DEWARP_BUF_STATE_DONE,
	TCC_DEWARP_BUF_STATE_ERROR,
};

/*
 * strucures for video source
 */
struct tcc_dewarp_vs_info {
	/* VIN_CTRL */
	u32 data_order; // [22:20]
	u32 data_format; // [19:16]

	/* VIN_SIZE */
	u32 height; // [31:16]
	u32 width; // [15: 0]
};

/* cam interrupt */
struct cam_intr {
	u32 reg;
	int num;
};

struct tcc_dewarp_device;

struct tcc_dewarp_wrap {
	u32 cam_mux;
	void __iomem *cam_mux_addr;
	u32 cam_ch;

	struct clk *cam_clk_axi;
	struct clk *cam_clk_pix;
	struct clk *cam_clk_apb;

	struct reserved_mem *rsvd_mem[RESERVED_MEM_MAX];

	struct dewarp_odw_params odw_params;

	/* cam interrupt */
	struct cam_intr intr;

	u32 recovery_trigger;
};

struct tcc_dewarp_buffer {
	struct vb2_v4l2_buffer buf;
	struct list_head entry;
};

struct tcc_dewarp_queue {
	struct vb2_queue queue;
	spinlock_t slock;
	struct list_head buf_list;
};

/* v4l2 structure */
struct tcc_dewarp_stream {
	struct tcc_dewarp_device *tdev;

	struct tcc_dewarp_vs_info vs_info;
	struct v4l2_subdev_mbus_code_enum mbus_code;
	struct v4l2_mbus_config mbus_config;
	struct v4l2_rect rect_crop;
	struct v4l2_rect rect_compose;
	struct v4l2_format format;
	struct mutex mlock;

	u32 handover_flags;
	u32 dewarp_flags;

	/* additional stabilization time */
	u32 vs_stabilization;

	/* dewarp path device */
	struct tcc_dewarp_wrap dewarp_wrap;

	int skip_frame_cnt;

	struct tcc_dewarp_queue queue;
	struct tcc_dewarp_buffer *curr_buf;
	struct tcc_dewarp_buffer *next_buf;

	u32 timestamp;
	struct timespec64 ts_prev;
	struct timespec64 ts_next;
	struct timespec64 ts_diff;

	u32 sequence;
	u32 dewarping;
};

/* v4l2 subdev structure */
struct tcc_dewarp_subdev {
	struct v4l2_async_subdev asd;
	struct v4l2_async_notifier notifier;
	struct v4l2_subdev *sd;
	struct v4l2_subdev_format fmt;

	struct tcc_dewarp_device *tdev;
};

struct tcc_dewarp_device {
	struct platform_device *pdev;
	struct tcc_cap_media *tccmd;

	int id;
	char name[32];

	struct video_device vdev;
	struct media_pad pads[2];
	struct v4l2_device v4ldev;

	struct tcc_dewarp_subdev tsubdev;
	struct tcc_dewarp_stream vstream;
	struct kref ref;

	struct tcc_svdw_dewarp_unit svdw_dwp_unit;
};

/* Macros */
/* (struct tcc_dewarp_stream *) to (struct device *) */
#define stream_to_device(ptr) (&((((ptr)->tdev)->pdev)->dev))

/* ------------------------------------------------------------------------
 * Debugging, printing and logging
 */

#define LOG_TAG "DEWARP"

#define loge(dev, fmt, ...)                                                    \
	{                                                                      \
		dev_err(dev, "[ERROR][%s] %s - " fmt, LOG_TAG, __func__,       \
			##__VA_ARGS__);                                        \
	}

#define logw(dev, fmt, ...)                                                    \
	{                                                                      \
		dev_warn(dev, "[WARN][%s] %s - " fmt, LOG_TAG, __func__,       \
			 ##__VA_ARGS__);                                       \
	}

#define logd(dev, fmt, ...)                                                    \
	{								       \
		dev_dbg(dev, "[DEBUG][%s] %s - " fmt, LOG_TAG, __func__,       \
			##__VA_ARGS__);					       \
	}

#define logi(dev, fmt, ...)                                                    \
	{                                                                      \
		dev_info(dev, "[INFO][%s] %s - " fmt, LOG_TAG, __func__,       \
			 ##__VA_ARGS__);                                       \
	}

/* v4l2 subdev interfaces */
extern int tcc_dewarp_subdev_get_src_sd(const struct tcc_dewarp_device *tdev,
					const struct media_pad *local_pad,
					struct v4l2_subdev **src_sd);
extern int tcc_dewarp_subdev_core_init(struct v4l2_subdev *sd, u32 val);
extern int tcc_dewarp_subdev_core_load_fw(struct v4l2_subdev *sd);
extern int tcc_dewarp_subdev_core_s_power(struct v4l2_subdev *sd, int on);

extern int tcc_dewarp_subdev_video_g_input_status(struct v4l2_subdev *sd,
						  u32 *status);
extern int tcc_dewarp_subdev_video_s_stream(struct v4l2_subdev *sd, int enable);
extern int
tcc_dewarp_subdev_video_g_dv_timings(struct v4l2_subdev *sd,
				     struct v4l2_dv_timings *timings);
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
extern int tcc_dewarp_subdev_video_g_mbus_config(struct v4l2_subdev *sd,
						 unsigned int idx,
						 struct v4l2_mbus_config *cfg);
#else
extern int tcc_dewarp_subdev_video_g_mbus_config(struct v4l2_subdev *sd,
						 struct v4l2_mbus_config *cfg);
#endif
extern int tcc_dewarp_subdev_pad_get_fmt(struct v4l2_subdev *sd,
					 struct v4l2_subdev_format *format);
extern int tcc_dewarp_subdev_pad_set_fmt(struct v4l2_subdev *sd,
					 struct v4l2_subdev_pad_config *cfg,
					 struct v4l2_subdev_format *format);

/* v4l2 interface */
extern const struct v4l2_ioctl_ops tcc_dewarp_ioctl_ops;
extern const struct v4l2_file_operations tcc_dewarp_fops;
extern int tcc_dewarp_v4l2_init_queue(struct tcc_dewarp_queue *queue);
extern int tcc_dewarp_v4l2_init_format(struct tcc_dewarp_device *tdev);
extern void tcc_dewarp_get_dma_addrs(const struct tcc_dewarp_stream *vstream,
				     struct vb2_buffer *vb, u32 addrs[]);
extern void tcc_dewarp_print_dma_addrs(const struct tcc_dewarp_stream *vstream,
				       const struct vb2_buffer *vb,
				       const u32 addrs[]);

/* capture */
extern int tcc_dewarp_video_init(struct tcc_dewarp_stream *vstream,
				 struct device_node *dev_node);
extern int tcc_dewarp_video_deinit(const struct tcc_dewarp_stream *vstream);
extern bool tcc_dewarp_video_is_pixelformat_supported(
	const struct tcc_dewarp_stream *vstream, u32 pixelformat);
extern u32 tcc_dewarp_video_get_pixelformat_by_index(
	const struct tcc_dewarp_stream *vstream, u32 index);
extern bool
tcc_dewarp_video_is_format_supported(const struct tcc_dewarp_stream *vstream,
				     const struct v4l2_format *format);
extern int tcc_dewarp_video_declare_coherent_dma_memory(
	const struct tcc_dewarp_stream *vstream);
extern int tcc_dewarp_video_streamon(struct tcc_dewarp_stream *vstream);
extern int tcc_dewarp_video_streamoff(struct tcc_dewarp_stream *vstream);
extern int tcc_dewarp_video_s_dewarp_params(struct tcc_dewarp_stream *vstream,
						struct dewarp_params *params);

#endif