/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCCVIN_VIDEO_H
#define TCCVIN_VIDEO_H

#include <linux/kernel.h>
#include <linux/poll.h>
#include <linux/of_device.h>
#include <linux/videodev2.h>
#include <linux/version.h>
#include <media/media-device.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-fwnode.h>
#include <media/videobuf2-v4l2.h>
#include <media/media-entity.h>

#include "../tcc-camss/tcc-cap-media.h"
#include "tccvin_common.h"

#if defined(CONFIG_ARCH_TCC807X) || defined(CONFIG_ARCH_TCC750X)
#include "model/vie_wrap/common/vin_wrap_video.h"
#else
/* vioc path */
#include <video/telechips/vioc_global.h>
#include <video/telechips/vioc_config.h>
#include <video/telechips/vioc_rdma.h>
#include <video/telechips/vioc_vin.h>
#include <video/telechips/vioc_viqe.h>
#include <video/telechips/vioc_deintls.h>
#include <video/telechips/vioc_scaler.h>
#include <video/telechips/vioc_wmix.h>
#include <video/telechips/vioc_wdma.h>
#include <video/telechips/vioc_intr.h>
#include "model/vioc_wrap/xxxx/tccxxxx_video.h"
#endif

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
#define DRIVER_NAME "tccvin"
#define DRIVER_VERSION "1.0.0"

/* vioc path */
#define MAX_PLANES (3)

#define DEFAULT_FRAMEWIDTH (640U)
#define DEFAULT_FRAMEHEIGHT (480U)

#define MAX_FRAMEWIDTH (8192U)
#define MAX_FRAMEHEIGHT (8192U)

/* Image Format */
#define VIN_WRAP_IMG_FMT_BPP1 (0U) // 1bit
#define VIN_WRAP_IMG_FMT_BPP2 (1U) // 2bits
#define VIN_WRAP_IMG_FMT_BPP4 (2U) // 4bits
#define VIN_WRAP_IMG_FMT_BPP8 (3U) // 1byte
#define VIN_WRAP_IMG_FMT_RGB332 (8U) // 1byte
#define VIN_WRAP_IMG_FMT_ARGB4444 (9U) // 2bytes
#define VIN_WRAP_IMG_FMT_RGB565 (10U) // 2bytes
#define VIN_WRAP_IMG_FMT_ARGB1555 (11U) // 2bytes
#define VIN_WRAP_IMG_FMT_ARGB8888 (12U) // 4bytes
#define VIN_WRAP_IMG_FMT_ARGB6666_4 (13U) // 4bytes
#define VIN_WRAP_IMG_FMT_RGB888 (14U) // 3bytes
#define VIN_WRAP_IMG_FMT_ARGB6666_3 (15U) // 3bytes
#define VIN_WRAP_IMG_FMT_COMP (16U) // 4bytes
#define VIN_WRAP_IMG_FMT_DECOMP (VIN_WRAP_IMG_FMT_COMP)
#define VIN_WRAP_IMG_FMT_IR8 (20U) // 8bits
#define VIN_WRAP_IMG_FMT_444SEP (21U) // 3bytes
#define VIN_WRAP_IMG_FMT_UYVY (22U) // 2bytes
#define VIN_WRAP_IMG_FMT_VYUY (23U) // 2bytes
#define VIN_WRAP_IMG_FMT_YUV420SEP (24U) // 1,1byte
#define VIN_WRAP_IMG_FMT_YUV422SEP (25U) // 1,1byte
#define VIN_WRAP_IMG_FMT_YUYV (26U) // 2bytes
#define VIN_WRAP_IMG_FMT_YVYU (27U) // 2bytes
#define VIN_WRAP_IMG_FMT_YUV420IL0 (28U) // 1,2byte
#define VIN_WRAP_IMG_FMT_YUV420IL1 (29U) // 1,2byte
#define VIN_WRAP_IMG_FMT_YUV422IL0 (30U) // 1,2bytes
#define VIN_WRAP_IMG_FMT_YUV422IL1 (31U) // 1,2bytes

/*
 * strucures for video source
 */
struct tccvin_vs_info {
	/* VIN_CTRL */
	u32 data_order; // [22:20]
	u32 data_format; // [19:16]
	u32 stream_enable; // [14]
	u32 gen_field_en; // [13]
	u32 de_low; // [12]
	u32 field_low; // [11]
	u32 vs_low; // [10]
	u32 hs_low; // [ 9]
	u32 pclk_polarity; // [ 8]
	u32 vs_mask; // [ 6]
	u32 hsde_connect_en; // [ 4]
	u32 intpl_en; // [ 3]
	u32 interlaced; // [ 2]
	u32 conv_en; // [ 1]

	/* VIN_MISC */
	u32 flush_vsync; // [16]

	/* VIN_SIZE */
	u32 height; // [31:16]
	u32 width; // [15: 0]
};

struct tccvin_mbus_vin_format {
	u32 mbus_fmt;
	u32 data_format;
	u32 data_order;
};

struct tccvin_format {
	u32 pixelformat;
	u32 guid;
};

struct tccvin_device;

struct tccvin_buffer {
	struct vb2_v4l2_buffer buf;
	struct list_head entry;
};

struct tccvin_queue {
	struct vb2_queue queue;
	u32 flags;
	spinlock_t slock;
	struct list_head buf_list;
};

/* v4l2 structure */
struct tccvin_stream {
	struct tccvin_device *tdev;

	struct tccvin_vs_info vs_info;
	struct v4l2_subdev_mbus_code_enum mbus_code;
	struct v4l2_mbus_config mbus_config;
	struct v4l2_rect rect_crop;
	struct v4l2_rect rect_compose;
	struct v4l2_format format;

	u32 handover_flags;

	/* additional stabilization time */
	u32 vs_stabilization;

	/* video-input path device */
	struct tccvin_cif cif;

	int skip_frame_cnt;

	struct tccvin_queue queue;
	struct tccvin_buffer *prev_buf;
	struct tccvin_buffer *next_buf;

	u32 timestamp;
	struct timespec64 ts_prev;
	struct timespec64 ts_next;
	struct timespec64 ts_diff;

	u32 sequence;
};

/* v4l2 subdev structure */
struct tccvin_subdev {
	struct v4l2_async_subdev asd;
	struct v4l2_async_notifier notifier;
	struct v4l2_subdev *sd;
	struct v4l2_subdev_format fmt;

	struct tccvin_device *tdev;
};

struct tccvin_device {
	struct platform_device *pdev;
	struct tcc_cap_media *tccmd;

	int id;
	char name[32];

	struct video_device vdev;
	struct media_pad pad;

	struct tccvin_subdev tsubdev;
	struct tccvin_stream vstream;
	struct kref ref;
};

/* v4l2 subdev interfaces */
int tccvin_subdev_get_src_sd(const struct tccvin_device *tdev,
			     const struct media_pad *local_pad,
			     struct v4l2_subdev **src_sd);
int tccvin_subdev_core_init(struct v4l2_subdev *sd, u32 val);
int tccvin_subdev_core_load_fw(struct v4l2_subdev *sd);
int tccvin_subdev_core_s_power(struct v4l2_subdev *sd, int on);

int tccvin_subdev_video_g_input_status(struct v4l2_subdev *sd, u32 *status);
int tccvin_subdev_video_s_stream(struct v4l2_subdev *sd, int enable);
int tccvin_subdev_video_g_dv_timings(struct v4l2_subdev *sd,
				     struct v4l2_dv_timings *timings);
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
int tccvin_subdev_video_g_mbus_config(struct v4l2_subdev *sd, unsigned int idx,
				      struct v4l2_mbus_config *cfg);
#else
int tccvin_subdev_video_g_mbus_config(struct v4l2_subdev *sd,
				      struct v4l2_mbus_config *cfg);
#endif

int tccvin_subdev_pad_get_fmt(struct v4l2_subdev *sd,
			      struct v4l2_subdev_format *format);
int tccvin_subdev_pad_set_fmt(struct v4l2_subdev *sd,
			      struct v4l2_subdev_pad_config *cfg,
			      struct v4l2_subdev_format *format);

/* v4l2 interface */
extern const struct v4l2_ioctl_ops tccvin_ioctl_ops;
extern const struct v4l2_file_operations tccvin_fops;
extern int tccvin_v4l2_init_queue(struct tccvin_queue *queue,
				  bool drop_corrupted);
extern int tccvin_v4l2_init_format(struct tccvin_device *tdev);
extern void tccvin_get_dma_addrs(const struct tccvin_stream *vstream,
				 struct vb2_buffer *vb, u32 addrs[]);
extern void tccvin_print_dma_addrs(const struct tccvin_stream *vstream,
				   const struct vb2_buffer *vb,
				   const u32 addrs[]);

/* capture */
int tccvin_video_init(struct tccvin_stream *vstream,
		      struct device_node *dev_node);
int tccvin_video_deinit(const struct tccvin_stream *vstream);
bool tccvin_video_is_pixelformat_supported(const struct tccvin_stream *vstream,
					   u32 pixelformat);
bool tccvin_video_is_format_supported(const struct tccvin_stream *vstream,
				      const struct v4l2_format *format);
int tccvin_video_declare_coherent_dma_memory(
	const struct tccvin_stream *vstream);
int tccvin_video_streamon(struct tccvin_stream *vstream);
int tccvin_video_streamoff(struct tccvin_stream *vstream);

void tccvin_video_check_path_status(const struct tccvin_stream *vstream,
				    u32 *status);
int tccvin_video_get_lastframe_addrs(const struct tccvin_stream *vstream,
				     u32 *addrs);
int tccvin_video_create_lastframe(const struct tccvin_stream *vstream,
				  u32 *addrs);
int tccvin_video_s_handover(struct tccvin_stream *vstream, const u32 *flag);
int tccvin_video_s_lut(struct tccvin_stream *vstream,
		       const struct vin_lut *plut);
int tccvin_video_trigger_recovery(const struct tccvin_stream *vstream,
				  u32 index);

void tccvin_get_vs_info_format_by_mbus_format(
	const struct tccvin_stream *vstream, u32 mbus_pixelcode,
	struct tccvin_vs_info *vs_fmt);
int tccvin_set_pgl(const struct tccvin_stream *vstream);
int tccvin_set_vin(const struct tccvin_stream *vstream);
int tccvin_set_deinterlacer(const struct tccvin_stream *vstream);
int tccvin_set_scaler(const struct tccvin_stream *vstream);
int tccvin_set_wmixer(const struct tccvin_stream *vstream);
int tccvin_set_wdma(struct tccvin_stream *vstream);
void tccvin_switch_lastframe(void __iomem *wdma, u32 *addrsLframe,
			     u32 *addrsPrev);

/* tccvin_video.c */
void tccvin_get_and_update_time(const struct tccvin_stream *vstream,
				struct timespec64 *ts_prev,
				struct timespec64 *ts_next, u32 debug);
bool tccvin_handover_flagged(u32 flags, u32 mask);

void tccvin_put_clock(const struct tccvin_stream *vstream);
int tccvin_get_clock(struct tccvin_stream *vstream,
		     struct device_node *dev_node);
int tccvin_parse_ddibus(struct tccvin_stream *vstream,
			const struct device_node *dev_node);
int tccvin_stop_subdevs(const struct tccvin_stream *vstream);
int tccvin_start_subdevs(struct tccvin_stream *vstream);
void tccvin_print_handover_flags(const struct tccvin_stream *vstream,
				 u32 flags);

#endif
