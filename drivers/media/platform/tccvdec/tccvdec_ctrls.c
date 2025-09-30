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

int tccvdec_ctrl_init(struct tcc_vdec_ctx *ctx)
{
	struct v4l2_ctrl *ctrl;
	int ret;

	ret = v4l2_ctrl_handler_init(&ctx->ctrl_handler, 10);
	if (ret)
		return ret;

/*
	ctrl = v4l2_ctrl_new_std_menu(&ctx->ctrl_handler, &vdec_ctrl_ops,
		V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE,
		V4L2_MPEG_VIDEO_MPEG4_PROFILE_ADVANCED_CODING_EFFICIENCY,
		~((1 << V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE) |
		  (1 << V4L2_MPEG_VIDEO_MPEG4_PROFILE_ADVANCED_SIMPLE)),
		V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE);
	if(ctrl != NULL) {
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	}

	ctrl = v4l2_ctrl_new_std_menu(&ctx->ctrl_handler, &vdec_ctrl_ops,
				      V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL,
				      V4L2_MPEG_VIDEO_MPEG4_LEVEL_5,
				      0, V4L2_MPEG_VIDEO_MPEG4_LEVEL_0);
	if(ctrl != NULL) {
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	}
	ctrl = v4l2_ctrl_new_std_menu(&ctx->ctrl_handler, &vdec_ctrl_ops,
		V4L2_CID_MPEG_VIDEO_H264_PROFILE,
		V4L2_MPEG_VIDEO_H264_PROFILE_HIGH,
		0,
		V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE);
		if(ctrl != NULL) {
			ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
		}
	
	ctrl = v4l2_ctrl_new_std_menu(&ctx->ctrl_handler, &vdec_ctrl_ops,
		V4L2_CID_MPEG_VIDEO_H264_LEVEL,
		V4L2_MPEG_VIDEO_H264_LEVEL_5_1,
		0, V4L2_MPEG_VIDEO_H264_LEVEL_1_0);
	if(ctrl != NULL) {
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	}
*/
/*
	ctrl = v4l2_ctrl_new_std_menu(&ctx->ctrl_handler, &vdec_ctrl_ops,
				      V4L2_CID_MPEG_VIDEO_VP8_PROFILE,
				      V4L2_MPEG_VIDEO_VP8_PROFILE_3,
				      0, V4L2_MPEG_VIDEO_VP8_PROFILE_0);
	if(ctrl != NULL) {
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	}

	ctrl = v4l2_ctrl_new_std_menu(&ctx->ctrl_handler, &vdec_ctrl_ops,
				V4L2_CID_MPEG_VIDEO_VP9_PROFILE,
				V4L2_MPEG_VIDEO_VP9_PROFILE_0,
				0, V4L2_MPEG_VIDEO_VP9_PROFILE_0);
	if(ctrl != NULL) {
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	}
*/
	ctrl = v4l2_ctrl_new_std_menu(&ctx->ctrl_handler, &vdec_ctrl_ops,
		V4L2_CID_MPEG_VIDEO_HEVC_PROFILE,
		V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10,
		0,
		V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN);
	if(ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std_menu(&ctx->ctrl_handler, &vdec_ctrl_ops,
		V4L2_CID_MPEG_VIDEO_HEVC_LEVEL,
		V4L2_MPEG_VIDEO_HEVC_LEVEL_5_1,
		0,
		V4L2_MPEG_VIDEO_HEVC_LEVEL_1);
	if(ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_handler, &vdec_ctrl_ops,
		V4L2_CID_MIN_BUFFERS_FOR_CAPTURE, 1, 32, 1, 1);
		if(ctrl != NULL) {
			ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
		}

	ret = ctx->ctrl_handler.error;
	if (ret) {
		printk(KERN_ERR "Failed to initialize v4l2 ctrl. ret=0x%x \n", ret);
		v4l2_ctrl_handler_free(&ctx->ctrl_handler);
		return ret;
	}

	return 0;
}

void tccvdec_ctrl_deinit(struct tcc_vdec_ctx *ctx)
{
	v4l2_ctrl_handler_free(&ctx->ctrl_handler);
}
