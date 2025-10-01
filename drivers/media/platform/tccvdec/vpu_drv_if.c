// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */
#include "vpu_drv_if.h"
#include "tccvdec_debug.h"
#include "tccvdec_core.h"

int tccvpudec_if_init(struct tcc_vpudec_ctx_t *ctx, u32 codec, void* priv)
{
	int ret = 0;
	vdec_v3_init_t vdec_init_info;
	vdec_handle_h vdec_handle;

	enum vpu_return_code vpu_ret = VPU_RETCODE_SUCCESS;

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
			case TCC_VIDEO_CODEC_VP9:
				vdec_init_info.input.codec_id = VCODEC_ID_VP9;
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

		if (codec == TCC_VIDEO_CODEC_HEVC || codec == TCC_VIDEO_CODEC_VP9) {
			vdec_init_info.input.output_format = VPU_OUTPUT_COMPRESSED_MAPCONV;
			vdec_init_info.input.max_support_width = TCC_VPU_DEC_HEVC_MAX_W;
			vdec_init_info.input.max_support_height = TCC_VPU_DEC_HEVC_MAX_H;
		}

		vdec_init_info.input.additional_frame_count = 11;
		vpu_ret = vdec_init(vdec_handle, &vdec_init_info);
		
		if (vpu_ret != VPU_RETCODE_SUCCESS) {
			tcvdec_err("vdec_init failed. codec_id=%d, ret=%d", 
				vdec_init_info.input.codec_id, vpu_ret);
			/* Clean up on failure */
			vdec_release_instance(vdec_handle);
			ctx->vdec_handle = NULL;
			ret = (int)vpu_ret;
		} else {
			ret = (int)VPU_RETCODE_SUCCESS;
			ctx->priv = priv;
		}
	} else {
		tcvdec_err("vdec_alloc_instance failed");
		ret = (int)VPU_RETCODE_FAILURE;
	}

	return ret;
}

void tccvpudec_if_deinit(struct tcc_vpudec_ctx_t *ctx)
{
	struct tcc_vdec_ctx *vdec_ctx = NULL;

	if (ctx == NULL) {
		tcvdec_err("[%s:%d] ctx is NULL.", __func__, __LINE__);
		return;
	}

	if (ctx->vdec_handle == NULL) {
		tcvdec_dbg("[%s:%d] vdec_handle is NULL, nothing to deinit.", __func__, __LINE__);
		return;
	}

	vdec_ctx = (struct tcc_vdec_ctx *)ctx->priv;
	
	if (vdec_ctx == NULL) {
		tcvdec_dbg("[%s:%d] vdec_ctx is NULL, skipping vdec_close.", __func__, __LINE__);
		mutex_lock(&ctx->lock);
		vdec_release_instance(ctx->vdec_handle);
		mutex_unlock(&ctx->lock);
		mutex_destroy(&ctx->lock);
		return;
	}

	mutex_lock(&ctx->lock);
	
	if (vdec_ctx->tcc_dev != NULL && vdec_ctx->tcc_dev->dev != NULL) {
		vdec_close(ctx->vdec_handle, vdec_ctx->tcc_dev->dev);
	} else {
		tcvdec_dbg("[%s:%d] tcc_dev or dev is NULL, skipping vdec_close.", __func__, __LINE__);
	}
	
	vdec_release_instance(ctx->vdec_handle);
	mutex_unlock(&ctx->lock);
	mutex_destroy(&ctx->lock);
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
	}
	return ret;
}

int tccvpudec_if_decode(struct tcc_vpudec_ctx_t *ctx, struct tcc_codec_bs_t *bs, struct tcc_codec_decode_output_t *output)
{
	int ret = 0;
	if((ctx == NULL) || (bs == NULL) || (output == NULL)) {
		tcvdec_err("[%s:%d] Invalid parameter.", __func__, __LINE__);
		ret = -EINVAL;
	} else {
		mutex_lock(&ctx->lock);
		ret = (int)vdec_decode(ctx->vdec_handle, bs, output);
		mutex_unlock(&ctx->lock);

		if(ret != (int) VPU_RETCODE_SUCCESS) {
			tcvdec_err("Decode Fail(ret=%d)", ret);
			return ret;
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
	} else {
		struct tcc_codec_decode_output_t output;
		memset(&output, 0x0, sizeof(struct tcc_codec_decode_output_t));
		mutex_lock(&ctx->lock);
		ret = (int)vdec_drain(ctx->vdec_handle, &output);
		mutex_unlock(&ctx->lock);
	}
	return ret;
}

void tccvpudec_if_flush(struct tcc_vpudec_ctx_t *ctx)
{
	if (ctx == NULL) return; 

	tcvdec_dbg("flush");
	
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
