// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_DEC_V3_H
#define VPU_DEC_V3_H

#include "vpu_codec_if.h"
#include "video/telechips/vpu_v3/tcc_vpu_v3_common.h"
#include "video/telechips/vpu_v3/tcc_vpu_v3_decoder.h"

typedef void* vdec_handle_h;

#define VDEC_MAX_CAP_STR  (64)
#define VDEC_MAX_CAP  (64)
#define VDEC_MAX_FRAMEBUFFER_COUNT			(32)

typedef struct vdec_framebuffer_t
{
	vpu_addr_t framebuffer[VPU_ADDR_MAX];

	unsigned int size;
	int aligned_width;
	int aligned_height;
	int format;
} vdec_framebuffer_t;

enum vpu_return_code vdec_init(vdec_handle_h handle, vdec_v3_init_t* p_init_param);

enum vpu_return_code vdec_seq_header(vdec_handle_h handle, struct tcc_codec_bs_t* bs, struct tcc_codec_header_t* hdr);

enum vpu_return_code vdec_register_framebuffer(vdec_handle_h handle, struct device *dev, struct tcc_codec_fb_t *fb_array, u32 number);

int vdec_decode(vdec_handle_h handle, struct tcc_codec_bs_t* bs, struct tcc_codec_decode_output_t* output);

enum vpu_return_code vdec_buf_clear(vdec_handle_h handle, u32 clear_idx);

enum vpu_return_code vdec_drain(vdec_handle_h handle, struct tcc_codec_decode_output_t* output);

enum vpu_return_code vdec_flush(vdec_handle_h handle);

enum vpu_return_code vdec_close(vdec_handle_h handle, struct device *dev);

vdec_handle_h vdec_alloc_instance(void);

void vdec_release_instance(vdec_handle_h handle);

#endif