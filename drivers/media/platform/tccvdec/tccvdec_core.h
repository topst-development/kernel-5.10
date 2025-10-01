// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef TCCVDEC_CORE_H
#define TCCVDEC_CORE_H

#include <linux/list.h>
#include <media/videobuf2-v4l2.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>

#include "vpu_drv_if.h"

#include "tccvdecqueue.h"

/**
 * enum tcc_vpudrv_state - The state of an VPU Driver instance.
 * @VPU_STATE_FREE - default state when instance is created
 * @VPU_STATE_INIT - vcodec instance is initialized
 * @VPU_STATE_HEADER - vdec had sps/pps header parsed or venc
 *			had sps/pps header encoded
 * @VPU_STATE_FRAMEBUFFER - vdec had registered frame buffer for decoding
 */
enum tcc_vpudrv_state {
	VPU_STATE_FREE = 0,
	VPU_STATE_INIT = 1,
	VPU_STATE_HEADER = 2,
	VPU_STATE_FRAMEBUFFER = 3,
	VPU_STATE_FLUSHED = 4,
};

struct tcc_vdec_plane {
	dma_addr_t dma_addr;
	void* phy_addr;
	void* virt_addr;
	uint32_t size;
	uint32_t flags;
};

/* NOTE: [2024-04-16] [YoungJun Shim]
 * V4L2 custom buffer structure.
 * The first and the second members are templates from the V4L2 M2M framework.
 * Do not change the order of the members in this structure.
 * If adding a new member, insert it below 'used'.
 */
struct tcc_vdec_buffer {
	struct v4l2_m2m_buffer m2m_buf;
	struct tcc_vdec_plane planes[3];
	int vpu_idx;
	uint32_t used;
	struct list_head framebuffer_item;
};

struct tcc_vdec_dev {
	struct device *dev;
	struct v4l2_device	v4l2_dev;
	struct video_device	*vdev_dec;

	struct workqueue_struct *workqueue;

	struct platform_device	*plat_dev;

	struct mutex dev_mutex;
	atomic_t		busy;
};

struct tcc_vdec_fmt {
	uint32_t pixfmt;
	uint32_t num_planes;
	uint32_t type;
	uint32_t flags;
};

struct tcc_vdec_framesizes {
	uint32_t pixfmt;
	struct v4l2_frmsize_stepwise stepwise;
};

struct vdec_controls {
	u32 profile;
	u32 level;
};

struct tcc_vdec_ctx {
	struct tcc_vdec_dev *tcc_dev;
	struct v4l2_fh fh;
	struct mutex lock;

	struct tcc_vpudec_ctx_t *vpu_drv;
	enum tcc_vpudrv_state state;

	struct v4l2_ctrl_handler ctrl_handler;
	struct vdec_controls controls;

	unsigned int input_buf_size;
	unsigned int output_buf_size;

	unsigned int num_input_bufs;
	unsigned int num_output_bufs;

	struct v4l2_fract timeperframe;

	int cap_buf_count;

	unsigned int pix_format;

	const struct tcc_vdec_fmt *fmt_out;
	const struct tcc_vdec_fmt *fmt_cap;

	struct v4l2_m2m_dev *m2m_dev;
	struct v4l2_m2m_ctx *m2m_ctx;
	
	struct vb2_queue out_vq;
	struct vb2_queue cap_vq;
	
	unsigned int streamon_cap, streamon_out;

	uint32_t out_width;
	uint32_t out_height;

	uint32_t width;
	uint32_t height;

	uint32_t align_width;
	uint32_t align_height;

	uint32_t fps;

	uint32_t min_framebuffer_cnt;
	uint32_t user_framebuffer_cnt;
	uint32_t profile;
	uint32_t level;

	int aborting;

	int src_psize[2];
	int dst_psize[2];

	u32 colorspace;
	u8 ycbcr_enc;
	u8 quantization;
	u8 xfer_func;

	bool stopping;
	bool streaming;

	u32 used_buf_cnt;

	u32 input_bs_buf_cnt;
	u32 output_bs_buf_cnt;

	u32 input_frame_buf_cnt;
	u32 output_frame_buf_cnt;

	u32 sequnce_fail_cnt;

	u32 subscriptions;

	struct file *dump_file;
	struct file *dump_file_bs;

	struct work_struct decode_work;

	struct list_head framebuffer_list;

    struct tccvdecqueue timestamp_queue;
};

#define ctrl_to_ctx(ctrl)	\
	container_of((ctrl)->handler, struct tcc_vdec_ctx, ctrl_handler)

#endif