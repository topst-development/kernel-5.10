/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TCC_SVDW_VIDEO_H
#define TCC_SVDW_VIDEO_H

#include <linux/types.h>
#include <linux/firmware.h>
#include <linux/kernel.h>
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>
#include <linux/version.h>
#include <linux/videodev2.h>
#include <media/media-entity.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/videobuf2-v4l2.h>

#ifdef CONFIG_ARCH_TCC807X
#include "807x/tcc-svdw-stitch.h"
#endif
#ifdef CONFIG_ARCH_TCC750X
#include "750x/tcc-svdw-stitch.h"
#endif
#include "../tcc-camss/tcc-cap-media.h"

/* ------------------------------------------------------------------------
 * Driver specific constants.
 */
#define DRIVER_VERSION "0.0.1"

#define SVDW_MAX_DEWARP_CNT (4)

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

#define TCC_SVDW_PADS_MAX (4U)
#define TCC_SVDW_PADS_SINK_0 (0U)
#define TCC_SVDW_PADS_SINK_1 (1U)
#define TCC_SVDW_PADS_SINK_2 (2U)
#define TCC_SVDW_PADS_SINK_3 (3U)

#define TCC_SVDW_REQUIRED_DWRP_COUNT (4U)

#define SVDW_INTR_STR_SVM_0_EOF "svm_0_eof"
#define SVDW_INTR_STR_SVM_1_EOF "svm_1_eof"
#define SVDW_INTR_STR_SVM_2_EOF "svm_2_eof"
#define SVDW_INTR_STR_SVM_3_EOF "svm_3_eof"
#define SVDW_INTR_STR_SVM_RDONE "svm_rdone"

#define SVDW_INTR_IDX_SVM_0_EOF (0U)
#define SVDW_INTR_IDX_SVM_1_EOF (1U)
#define SVDW_INTR_IDX_SVM_2_EOF (2U)
#define SVDW_INTR_IDX_SVM_3_EOF (3U)
#define SVDW_INTR_IDX_SVM_RDONE (4U)
#define SVDW_INTR_IDX_MAX (5U)

enum odw_device_parse {
	ODW_DEV_ID,
	ODW_DEV_BYPASS,
	ODW_DEV_IRQ,
	ODW_DEV_MAX,
};

enum reserved_memory {
	SVDW_MEMBLOCK_PREV,
	SVDW_MEMBLOCK_OVERLAY,
	SVDW_MEMBLOCK_MAX,
};

enum tcc_svdw_buffer_state {
	TCC_SVDW_BUF_STATE_IDLE,
	TCC_SVDW_BUF_STATE_QUEUED,
	TCC_SVDW_BUF_STATE_ACTIVE,
	TCC_SVDW_BUF_STATE_READY,
	TCC_SVDW_BUF_STATE_DONE,
	TCC_SVDW_BUF_STATE_ERROR,
};

enum tcc_svdw_queue_state {
	TCC_SVDW_QUEUE_DISCONNECTED = 1, /* 1 << 0 */
	TCC_SVDW_QUEUE_DROP_CORRUPTED = 2, /* 1 << 1 */
};

enum tcc_svdw_handle_state {
	TCC_SVDW_HANDLE_PASSIVE,
	TCC_SVDW_HANDLE_ACTIVE,
};

/*
 * strucures for video source
 */
struct tcc_svdw_vs_info {
	/* VIN_CTRL */
	u32 data_order; // [22:20]
	u32 data_format; // [19:16]

	/* VIN_SIZE */
	u32 height; // [31:16]
	u32 width; // [15: 0]
};

/* cam interrupt */
struct svdw_intr {
	u32 registered;
	int num;
};

struct tcc_svdw_device;

struct tcc_svdw_wrap {
	u32 cam_mux;
	void __iomem *cam_mux_addr;
	u32 cam_ch;

	struct clk *cam_clk_axi;
	struct clk *cam_clk_pix;
	struct clk *cam_clk_apb;

	struct reserved_mem *rsvd_mem[SVDW_MEMBLOCK_PREV];

	struct svdw_stitch_params stitch_params;

	/* cam interrupt */
	struct svdw_intr intr[SVDW_INTR_IDX_MAX];

	u32 recovery_trigger;
};

struct tcc_svdw_buffer {
	struct vb2_v4l2_buffer buf;
	struct list_head entry;
};

struct tcc_svdw_queue {
	struct vb2_queue queue;
	u32 flags;
	spinlock_t slock;
	struct list_head buf_list;
};

struct tcc_svdw_device {
	struct platform_device *pdev;
	struct video_device vdev;
	struct v4l2_device v4ldev;
	struct tcc_cap_media *tccmd;
	struct media_pad pads[TCC_SVDW_PADS_MAX];

	phys_addr_t patch_addrs[SVDW_MAX_DEWARP_CNT];

	struct v4l2_fh vfh;

	struct tcc_svdw_vs_info vs_info;
	struct v4l2_subdev_mbus_code_enum mbus_code;
	struct v4l2_mbus_config mbus_config;
	struct v4l2_rect rect_crop;
	struct v4l2_rect rect_compose;
	struct v4l2_format format;
	struct mutex mlock;

	/* svdw wrap configuration */
	struct tcc_svdw_wrap svdw_wrap;

	struct tcc_svdw_queue queue;
	struct tcc_svdw_buffer *prev_buf;
	struct tcc_svdw_buffer *curr_buf;

	u32 timestamp;
	struct timespec64 ts_prev;
	struct timespec64 ts_next;
	struct timespec64 ts_diff;

	u32 sequence;
	bool is_streaming;
	atomic_t entity_cnt; /* the number of linked entities */

	struct device_node *cfg_node;
};

static inline struct platform_device *dev_to_pdev(struct device *p_dev)
{
	return container_of(p_dev, struct platform_device, dev);
}

static inline struct tcc_svdw_device *dev_to_svdw(struct device *p_dev)
{
	const struct platform_device *pdev = NULL;
	pdev = dev_to_pdev(p_dev);
	return (struct tcc_svdw_device *)platform_get_drvdata(pdev);
}

static inline struct device *svdw_to_dev(struct tcc_svdw_device *p_svdw)
{
	return &p_svdw->pdev->dev;
}

static inline struct tcc_svdw_device *q2svdw(struct tcc_svdw_queue *p_queue)
{
	return (struct tcc_svdw_device *)container_of(p_queue, struct tcc_svdw_device, queue);
}
#define LOG_TAG "SVDW"
#define loge(dev, fmt, ...)                                                                        \
	{                                                                                          \
		dev_err(dev, "[ERROR][%s] %s - " fmt, LOG_TAG, __func__, ##__VA_ARGS__);           \
	}
#define logw(dev, fmt, ...)                                                                        \
	{                                                                                          \
		dev_warn(dev, "[WARN][%s] %s - " fmt, LOG_TAG, __func__, ##__VA_ARGS__);           \
	}
#define logd(dev, fmt, ...)                                                                        \
	{                                                                                          \
		dev_dbg(dev, "[DEBUG][%s] %s - " fmt, LOG_TAG, __func__, ##__VA_ARGS__);           \
	}
#define logi(dev, fmt, ...)                                                                        \
	{                                                                                          \
		dev_info(dev, "[INFO][%s] %s - " fmt, LOG_TAG, __func__, ##__VA_ARGS__);           \
	}

/* v4l2 subdev interfaces */
extern int tcc_svdw_subdev_get_src_sd(const struct tcc_svdw_device *tdev,
				      const struct media_pad *local_pad,
				      struct v4l2_subdev **src_sd);
extern int tcc_svdw_subdev_core_init(struct v4l2_subdev *sd, u32 val);
extern int tcc_svdw_subdev_core_load_fw(struct v4l2_subdev *sd);
extern int tcc_svdw_subdev_core_s_power(struct v4l2_subdev *sd, int on);

extern int tcc_svdw_subdev_video_g_input_status(struct v4l2_subdev *sd, u32 *status);
extern int tcc_svdw_subdev_video_s_stream(struct v4l2_subdev *sd, int enable);
extern int tcc_svdw_subdev_video_g_dv_timings(struct v4l2_subdev *sd,
					      struct v4l2_dv_timings *timings);
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
extern int tcc_svdw_subdev_video_g_mbus_config(struct v4l2_subdev *sd, unsigned int idx,
					       struct v4l2_mbus_config *cfg);
#else
extern int tcc_svdw_subdev_video_g_mbus_config(struct v4l2_subdev *sd,
					       struct v4l2_mbus_config *cfg);
#endif
extern int tcc_svdw_subdev_pad_get_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_format *format);
extern int tcc_svdw_subdev_pad_set_fmt(struct v4l2_subdev *sd, struct v4l2_subdev_pad_config *cfg,
				       struct v4l2_subdev_format *format);

/* v4l2 interface */
extern const struct v4l2_ioctl_ops tcc_svdw_ioctl_ops;
extern const struct v4l2_file_operations tcc_svdw_fops;
extern int tcc_svdw_init_vb2_queue(struct tcc_svdw_device *p_svdw, bool drop_corrupted);
extern int tcc_svdw_init_v4l2_fmt(struct tcc_svdw_device *tdev);
extern void tcc_svdw_get_dma_addrs(struct tcc_svdw_device *vstream, struct vb2_buffer *vb,
				   u32 addrs[]);
extern void tcc_svdw_print_dma_addrs(struct tcc_svdw_device *vstream, const struct vb2_buffer *vb,
				     const u32 addrs[]);

/* capture */
extern int tcc_svdw_init_parse_dt(struct tcc_svdw_device *p_svdw, struct device_node *dev_node);
extern void tcc_svdw_init(struct tcc_svdw_device *p_svdw, struct platform_device *p_pdev);
extern void __maybe_unused parse_cfg_and_set_regs(struct tcc_svdw_device *p_svdw,
						  const struct firmware *fw);
extern void tcc_svdw_video_deinit(struct tcc_svdw_device *p_svdw);
extern bool tcc_svdw_video_is_pixelformat_supported(struct tcc_svdw_device *p_svdw,
						    u32 pixelformat);
extern u32 tcc_svdw_video_get_pixelformat_by_index(struct tcc_svdw_device *p_svdw, u32 index);
extern bool tcc_svdw_video_is_format_supported(struct tcc_svdw_device *p_svdw,
					       const struct v4l2_format *format);
extern int tcc_svdw_init_coherent_dma_memory(struct tcc_svdw_device *p_svdw);
extern int tcc_svdw_video_streamon(struct tcc_svdw_device *vstream);
extern int tcc_svdw_video_streamoff(struct tcc_svdw_device *vstream);

extern void tcc_svdw_video_check_path_status(struct tcc_svdw_device *p_svdw, u32 *status);
extern int tcc_svdw_video_s_handover(struct tcc_svdw_device *vstream, const u32 *flag);
extern void tcc_svdw_unregister_video_device(struct tcc_svdw_device *p_svdw);
extern int tcc_svdw_g_patch_addresses(struct tcc_svdw_device *p_svdw);
extern void tcc_svdw_free_irq(struct tcc_svdw_device *p_svdw);
extern int tcc_svdw_s_fmt_dewarpers(struct tcc_svdw_device *p_svdw);
#endif
