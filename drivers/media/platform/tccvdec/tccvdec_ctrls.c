#include <linux/types.h>
#include <media/v4l2-ctrls.h>
#include "tccvdec_core.h"

static int vdec_op_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct tcc_vdec_ctx *ctx = ctrl_to_ctx(ctrl);
	struct vdec_controls *ctr = &ctx->controls;

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
	case V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE:
	case V4L2_CID_MPEG_VIDEO_VP8_PROFILE:
	case V4L2_CID_MPEG_VIDEO_VP9_PROFILE:
	case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
		ctr->profile = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
	case V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL:
	case V4L2_CID_MPEG_VIDEO_HEVC_LEVEL:
		ctr->level = ctrl->val;
		break;
	default:
		printk(KERN_ERR "[%s:%d] Unsupported id=%d\n", __func__, __LINE__, ctrl->id);
		return -EINVAL;
	}
	return 0;
}

static int vdec_op_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	struct tcc_vdec_ctx *ctx = ctrl_to_ctx(ctrl);
	struct vdec_controls *ctr = &ctx->controls;

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
	case V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE:
	case V4L2_CID_MPEG_VIDEO_VP8_PROFILE:
	case V4L2_CID_MPEG_VIDEO_VP9_PROFILE:
	case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
		if (ctx->state >= VPU_STATE_HEADER) {
			ctr->profile = ctx->profile;
		}
		ctrl->val = ctr->profile;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
	case V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL:
	case V4L2_CID_MPEG_VIDEO_HEVC_LEVEL:
		if (ctx->state >= VPU_STATE_HEADER) {
			ctr->level = ctx->level;
		}
		ctrl->val = ctr->level;
		break;
	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
		if (ctx->state >= VPU_STATE_HEADER) {
			ctrl->val = ctx->min_framebuffer_cnt;
		} else {
			ctrl->val = 1;
		}
		break;
	default:
		printk(KERN_ERR "[%s:%d] Unsupported id=%d\n", __func__, __LINE__, ctrl->id);
		return -EINVAL;
	}
	printk("[%s:%d] id=%d val=%d \n", __func__, __LINE__, ctrl->id, ctrl->val);
	return 0;
}

static const struct v4l2_ctrl_ops vdec_ctrl_ops = {
	.s_ctrl = vdec_op_s_ctrl,
	.g_volatile_ctrl = vdec_op_g_volatile_ctrl,
};

/* Only VP9 / H.264(AVC) / H.265(HEVC) are exposed.
 * Profiles/levels are limited to the HW spec:
 *  - H.264: Baseline/Main/High up to Level 4.2
 *  - VP9 : Profile 0 and Profile 2 (HBD)
 *  - HEVC: Main/Main10 up to Level 5.1
 */
int tccvdec_ctrl_init(struct tcc_vdec_ctx *ctx)
{
	struct v4l2_ctrl *ctrl;
	struct v4l2_ctrl *c;
	int ret;

	ret = v4l2_ctrl_handler_init(&ctx->ctrl_handler, 10);
	if (ret)
		return ret;

	/* --- H.264 (AVC) --- */
	ctrl = v4l2_ctrl_new_std_menu(&ctx->ctrl_handler, &vdec_ctrl_ops,
		V4L2_CID_MPEG_VIDEO_H264_PROFILE,
		V4L2_MPEG_VIDEO_H264_PROFILE_HIGH,
		~((1 << V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE) |
		  (1 << V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE) |
		  (1 << V4L2_MPEG_VIDEO_H264_PROFILE_EXTENDED) |
		  (1 << V4L2_MPEG_VIDEO_H264_PROFILE_MAIN) |
		  (1 << V4L2_MPEG_VIDEO_H264_PROFILE_HIGH)),
		V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE);

	ctrl = v4l2_ctrl_new_std_menu(&ctx->ctrl_handler, &vdec_ctrl_ops,
		V4L2_CID_MPEG_VIDEO_H264_LEVEL,
		V4L2_MPEG_VIDEO_H264_LEVEL_4_2,
		0,
		V4L2_MPEG_VIDEO_H264_LEVEL_1_0);

	/* --- VP9 --- (no level control in V4L2) */
	ctrl = v4l2_ctrl_new_std_menu(&ctx->ctrl_handler, &vdec_ctrl_ops,
		V4L2_CID_MPEG_VIDEO_VP9_PROFILE,
		V4L2_MPEG_VIDEO_VP9_PROFILE_3,
		~((1 << V4L2_MPEG_VIDEO_VP9_PROFILE_0) |
		  (1 << V4L2_MPEG_VIDEO_VP9_PROFILE_2)),
		V4L2_MPEG_VIDEO_VP9_PROFILE_0);

	/* --- HEVC (H.265) --- */
	ctrl = v4l2_ctrl_new_std_menu(&ctx->ctrl_handler, &vdec_ctrl_ops,
		V4L2_CID_MPEG_VIDEO_HEVC_PROFILE,
		V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10,
		~((1 << V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN) |
		  (1 << V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10)),
		V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN);

	ctrl = v4l2_ctrl_new_std_menu(&ctx->ctrl_handler, &vdec_ctrl_ops,
		V4L2_CID_MPEG_VIDEO_HEVC_LEVEL,
		V4L2_MPEG_VIDEO_HEVC_LEVEL_5_1,
		0,
		V4L2_MPEG_VIDEO_HEVC_LEVEL_1);

	/* Min capture buffers */
	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_handler, &vdec_ctrl_ops,
		V4L2_CID_MIN_BUFFERS_FOR_CAPTURE, 1, 32, 1, 1);

	/* Mark all controls as volatile once here.
	 * Note: 'ctrls' is a list, not an array.
	 */
	list_for_each_entry(c, &ctx->ctrl_handler.ctrls, node)
		c->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ret = ctx->ctrl_handler.error;
	if (ret) {
		printk(KERN_ERR "Failed to initialize v4l2 ctrl. ret=0x%x\n", ret);
		v4l2_ctrl_handler_free(&ctx->ctrl_handler);
		return ret;
	}
	return 0;
}

void tccvdec_ctrl_deinit(struct tcc_vdec_ctx *ctx)
{
	v4l2_ctrl_handler_free(&ctx->ctrl_handler);
}
