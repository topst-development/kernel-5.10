#include "tccvenc_ctrls.h"

static int tccvenc_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct tcc_venc_ctx *ctx = container_of(ctrl->handler, struct tcc_venc_ctx, ctrl_handler);

	tcvenc_step("s_ctrl: id=0x%x, val=%d", ctrl->id, ctrl->val);

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_BITRATE:
		tcvenc_step("Set bitrate: %d", ctrl->val);
		ctx->bitrate = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_GOP_SIZE:
		tcvenc_step("Set GOP size: %d", ctrl->val);
		ctx->gop_size = ctrl->val;
		break;

	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		tcvenc_step("Set H.264 profile: %d", ctrl->val);
		ctx->profile = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		tcvenc_step("Set H.264 level: %d", ctrl->val);
		ctx->level = ctrl->val;
		break;

	case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
		tcvenc_info("Set HEVC profile: %d", ctrl->val);
		if (ctrl->val != V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN)
			return -EINVAL;
		ctx->hevc_profile = ctrl->val;
		return 0;
	case V4L2_CID_MPEG_VIDEO_HEVC_LEVEL:
		tcvenc_info("Set HEVC level: %d", ctrl->val);
		ctx->hevc_level = ctrl->val;
		return 0;
	default:
		tcvenc_step("Unsupported ctrl id: 0x%x", ctrl->id);
		return -EINVAL;
	}

	return 0;
}

static const struct v4l2_ctrl_ops tccvenc_ctrl_ops = {
	.s_ctrl = tccvenc_s_ctrl,
};

int tccvenc_ctrls_init(struct tcc_venc_ctx *ctx)
{
	tcvenc_step("Initializing encoder controls");

	v4l2_ctrl_handler_init(&ctx->ctrl_handler, 4);

	v4l2_ctrl_new_std(&ctx->ctrl_handler, &tccvenc_ctrl_ops,
		V4L2_CID_MPEG_VIDEO_BITRATE, 2000000, 10000000, 1000, 2000000);

	v4l2_ctrl_new_std(&ctx->ctrl_handler, &tccvenc_ctrl_ops,
		V4L2_CID_MPEG_VIDEO_GOP_SIZE, 1, 300, 1, 30);
	
	v4l2_ctrl_new_std_menu(&ctx->ctrl_handler, &tccvenc_ctrl_ops,
		V4L2_CID_MPEG_VIDEO_H264_PROFILE,
		V4L2_MPEG_VIDEO_H264_PROFILE_HIGH,
		0,
		V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE);

	v4l2_ctrl_new_std_menu(&ctx->ctrl_handler, &tccvenc_ctrl_ops,
		V4L2_CID_MPEG_VIDEO_H264_LEVEL,
		V4L2_MPEG_VIDEO_H264_LEVEL_5_2,
		0,
		V4L2_MPEG_VIDEO_H264_LEVEL_4_1);

	v4l2_ctrl_new_std_menu(&ctx->ctrl_handler, &tccvenc_ctrl_ops,
		V4L2_CID_MPEG_VIDEO_HEVC_PROFILE,
		V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN, 
		0,
		V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN);

	v4l2_ctrl_new_std_menu(&ctx->ctrl_handler, &tccvenc_ctrl_ops,
		V4L2_CID_MPEG_VIDEO_HEVC_LEVEL,
		V4L2_MPEG_VIDEO_HEVC_LEVEL_5_1,
		0,
		V4L2_MPEG_VIDEO_HEVC_LEVEL_5);

	if (ctx->ctrl_handler.error) {
		tcvenc_err("ctrl handler error: %d", ctx->ctrl_handler.error);
		return ctx->ctrl_handler.error;
	}

	ctx->fh.ctrl_handler = &ctx->ctrl_handler;
	tcvenc_step("Encoder controls initialized successfully");
	return 0;
}
