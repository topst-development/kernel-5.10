// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */
#include "vpu_drv_if.h"
#include "tccvdec_debug.h"
#include "tccvdec_core.h"

int tccvpudec_if_init(struct tcc_vpudec_ctx_t *ctx, u32 codec, void* priv, const struct vpu_drv_ops *ops)
{
	int ret = 0;
	vdec_v3_init_t vdec_init_info;
	vdec_handle_h vdec_handle;

	enum vpu_return_code vpu_ret = VPU_RETCODE_SUCCESS;

	ctx->ops = ops;

	mutex_init(&ctx->lock);
	vdec_handle = vdec_alloc_instance();
	if (vdec_handle != NULL) {
		ctx->vdec_handle = vdec_handle;

		memset(&vdec_init_info, 0x00, sizeof(vdec_v3_init_t));

		switch (codec) {
			case TCC_VIDEO_CODEC_H264:
				vdec_init_info.input.codec_id = VCODEC_ID_AVC;
				break;
			case TCC_VIDEO_CODEC_HEVC:
				vdec_init_info.input.codec_id = VCODEC_ID_HEVC;
				break;
			default:
				return -EINVAL;
		}
		
		vdec_init_info.input.output_format = VPU_OUTPUT_LINEAR_NV12;
		vdec_init_info.input.enable_ringbuffer_mode = 0U;
		vdec_init_info.input.enable_interlace_processing = 0U;
		vdec_init_info.input.enable_dma_buf_id = 0U;
		vdec_init_info.input.max_support_width = TCC_VPU_DEC_H264_MAX_W;
		vdec_init_info.input.max_support_height = TCC_VPU_DEC_H264_MAX_H;

		if (codec == TCC_VIDEO_CODEC_HEVC) {
			vdec_init_info.input.output_format = VPU_OUTPUT_LINEAR_10_TO_8_BIT_NV12;
			vdec_init_info.input.max_support_width = TCC_VPU_DEC_HEVC_MAX_W;
			vdec_init_info.input.max_support_height = TCC_VPU_DEC_HEVC_MAX_H;
		}

		//vdec_init_info.input.additional_frame_count = (int)8; // is GST + 8

		vpu_ret = vdec_init(vdec_handle, &vdec_init_info);
		
		if (vpu_ret == VPU_RETCODE_FAILURE) {
			ret = (int)VPU_RETCODE_FAILURE;
		} else {
			ret = (int)VPU_RETCODE_SUCCESS;
		}
		
		ctx->priv = priv;
	} else {
		ret = (int)VPU_RETCODE_FAILURE;
	}

	return ret;
}

void tccvpudec_if_deinit(struct tcc_vpudec_ctx_t *ctx)
{
	struct tcc_vdec_ctx *vdec_ctx = (struct tcc_vdec_ctx *)ctx->priv;

	if(ctx != NULL && ctx->vdec_handle != NULL) {
		mutex_lock(&ctx->lock);
		vdec_close(ctx->vdec_handle, vdec_ctx->tcc_dev->dev);
		vdec_release_instance(ctx->vdec_handle);
		mutex_unlock(&ctx->lock);
		mutex_destroy(&ctx->lock);
	}
}

int tccvpudec_if_register_fb(struct tcc_vpudec_ctx_t *ctx, struct tcc_codec_fb_t *fb_array, u32 number)
{
	struct tcc_vdec_ctx *vdec_ctx = (struct tcc_vdec_ctx *)ctx->priv;

	int ret = 0;

	if(ctx == NULL && ctx->vdec_handle == NULL) {
		tcvdec_err("[%s:%d] Invalid parameter.", __func__, __LINE__);
		return -EINVAL;
	}
	mutex_lock(&ctx->lock);
	ret = (int)vdec_register_framebuffer(ctx->vdec_handle, vdec_ctx->tcc_dev->dev, fb_array, number);
	mutex_unlock(&ctx->lock);

	if(ret != VPU_RETCODE_SUCCESS) {
		tcvdec_err("[%s:%d] Failed to register framebuffer. ret=%d", __func__, __LINE__, ret);
		return -EINVAL;
	} else {
		tcvdec_info("[%s:%d] Registered %d framebuffers.", __func__, __LINE__, number);
	}

	return ret;
}

int tccvpudec_if_parse_seq_header(struct tcc_vpudec_ctx_t *ctx, struct tcc_codec_bs_t *bs,
	struct tcc_codec_header_t *hdr)
{
	int ret = 0;

	if((bs == NULL) || (hdr == NULL)) {
		tcvdec_err("[%s:%d] Invalid parameter.", __func__, __LINE__);
		ret = -EINVAL;
	} else {
		mutex_lock(&ctx->lock);
		if(vdec_seq_header(ctx->vdec_handle, bs, hdr) != VPU_RETCODE_SUCCESS){
			ret = -EINVAL;
		}
		mutex_unlock(&ctx->lock);

		memset(&ctx->last_decode_out, 0x0, sizeof(struct tcc_vpudec_frame_t));
	}
	return ret;
}

int tccvpudec_if_decode(struct tcc_vpudec_ctx_t *ctx, struct tcc_codec_bs_t *bs, struct tcc_vpudec_frame_t *frame)
{
	int ret = 0;
	if((ctx == NULL) || (bs == NULL) || (frame == NULL)) {
		tcvdec_err("[%s:%d] Invalid parameter.", __func__, __LINE__);
		ret = -EINVAL;
	} else if(ctx->ops == NULL) {
		tcvdec_err("[%s:%d] Invalid parameter.", __func__, __LINE__);
		ret = -EINVAL;
	} else if(ctx->ops->decode_done == NULL) {
		tcvdec_err("[%s:%d] Invalid parameter.", __func__, __LINE__);
		ret = -EINVAL;
	} else {
		struct tcc_codec_decode_output_t output;

		memset(&output, 0x0, sizeof(struct tcc_codec_decode_output_t));
		mutex_lock(&ctx->lock);
		ret = (int)vdec_decode(ctx->vdec_handle, bs, &output);
		mutex_unlock(&ctx->lock);

		if(ret != (int) VPU_RETCODE_SUCCESS) {
			tcvdec_err("Decode Fail(ret=%d)", ret);
			if(ret == VPU_RETCODE_CODEC_SPECOUT)
				ctx->ops->decode_finish(ctx);
			return ret;
		}

		frame->status |= output.status;
		if(output.status & TCC_VIDEO_CODEC_STATUS_DECODED) {
			if (output.decodedIndex >= 0) {
				ctx->dts[output.decodedIndex] = bs->timestamp;
			}
			frame->decodedIndex = output.decodedIndex; 
		} else if(output.status & TCC_VIDEO_CODEC_STATUS_BUF_FULL) {
			ctx->last_decode_out.status |= output.status;
		}

		if(output.status & TCC_VIDEO_CODEC_STATUS_DISPLAYABLE)
		{
			ctx->last_decode_out.width = output.width;
			ctx->last_decode_out.height = output.height;
			ctx->last_decode_out.displayIndex = output.displayIndex;
			ctx->last_decode_out.status |= output.status;
			ctx->last_decode_out.timestamp = ctx->dts[output.displayIndex];
			memcpy(&ctx->last_decode_out.fb, &output.fb, sizeof(output.fb));

			frame->width = output.width;
			frame->height = output.height;
			frame->displayIndex = output.displayIndex;
			frame->decodedIndex = output.decodedIndex; 
			memcpy(&frame->fb, &output.fb, sizeof(output.fb));
		}
	}
	return ret;
}

int tccvpudec_if_drain(struct tcc_vpudec_ctx_t *ctx)
{
	int ret = 0;
	if((ctx == NULL)) {
		ret = -EINVAL;
		tcvdec_err("[%s:%d] Invalid parameter.", __func__, __LINE__);
		return ret;
	} else if(ctx->ops == NULL) {
		ret = -EINVAL;
		tcvdec_err("[%s:%d] Invalid parameter.", __func__, __LINE__);
		return ret;
	} else {
		struct tcc_codec_decode_output_t output;
		memset(&output, 0x0, sizeof(struct tcc_codec_decode_output_t));
		mutex_lock(&ctx->lock);
		ret = (int)vdec_drain(ctx->vdec_handle, &output);
		mutex_unlock(&ctx->lock);
		if (ret >= 0) {
			ctx->last_decode_out.width = output.width;
			ctx->last_decode_out.height = output.height;
			ctx->last_decode_out.displayIndex = output.displayIndex;
			ctx->last_decode_out.status |= output.status;
			ctx->last_decode_out.timestamp = ctx->dts[output.displayIndex];
			memcpy(&ctx->last_decode_out.fb, &output.fb, sizeof(output.fb));

			ctx->ops->decode_done(ctx, &ctx->last_decode_out);
		}
	}
	return ret;
}

void tccvpudec_if_flush(struct tcc_vpudec_ctx_t *ctx)
{
	if (ctx == NULL) return; 
	
	mutex_lock(&ctx->lock);
	if(ctx->vdec_handle != NULL) {
		vdec_flush(ctx->vdec_handle);
	}
	memset(ctx->dts, 0x0, sizeof(ctx->dts));
	mutex_unlock(&ctx->lock);
}

int tccvpudec_if_buf_clear(struct tcc_vpudec_ctx_t *ctx, u32 clear_idx)
{
	int ret = 0;
	mutex_lock(&ctx->lock);
	ret=(int)vdec_buf_clear(ctx->vdec_handle, clear_idx);
	mutex_unlock(&ctx->lock);
	return ret;
}
