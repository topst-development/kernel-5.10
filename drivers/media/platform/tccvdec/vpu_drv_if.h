// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_VPUDRV_IF_H
#define TCC_VPUDRV_IF_H

#include <linux/printk.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>

#include "vpu_codec_if.h"
#include "vpu_supported_mediaforamt.h"
#include "vpu_dec_v3.h"

#define USER_FRAMEBUFFEER_CNT (8)

#define MAX_RESOLUTION_WIDTH	(3840)
#define MAX_RESOLUTION_HEIGHT	(2160)

#define DEFAULT_FRAME_BUF_CNT		(6)

typedef struct tcc_vpudec_ctx_t tcc_vpudec_ctx_t;

struct tcc_vpudec_frame_t {
	struct tcc_codec_fb_t fb;
	u64 timestamp;
	u32 displayIndex;
	u32 decodedIndex;
	u32 width;
	u32 height;
	u32 status;
	u32 flags;
};

struct tcc_vpudec_ctx_t {
	struct mutex lock;
	u64 dts[VIDEO_MAX_FRAME];
	vdec_handle_h vdec_handle;
	void* priv;
	const struct codec_if *dec_if;
};

int tccvpudec_if_init(struct tcc_vpudec_ctx_t *ctx, u32 codec, void* priv);

void tccvpudec_if_deinit(struct tcc_vpudec_ctx_t *ctx);

int tccvpudec_if_register_fb(struct tcc_vpudec_ctx_t *ctx, struct tcc_codec_fb_t *fb_array, u32 number);

int tccvpudec_if_parse_seq_header(struct tcc_vpudec_ctx_t *ctx, struct tcc_codec_bs_t *bs, struct tcc_codec_header_t *hdr);

int tccvpudec_if_decode(struct tcc_vpudec_ctx_t *ctx, struct tcc_codec_bs_t *bs, struct tcc_codec_decode_output_t *output);

void tccvpudec_if_flush(struct tcc_vpudec_ctx_t *ctx);

int tccvpudec_if_drain(struct tcc_vpudec_ctx_t *ctx);

int tccvpudec_if_buf_clear(struct tcc_vpudec_ctx_t *ctx, u32 clear_idx);

#endif