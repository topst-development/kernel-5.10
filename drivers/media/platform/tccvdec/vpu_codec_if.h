#ifndef TCC_CODEC_IF_H
#define TCC_CODEC_IF_H

#include <stddef.h>
#include <linux/types.h>
#include <linux/dma-mapping.h>

#define TCC_VIDEO_CODEC_H264		(0x0001)
#define TCC_VIDEO_CODEC_MPEG1		(0x0002)
#define TCC_VIDEO_CODEC_MPEG2		(0x0004)
#define TCC_VIDEO_CODEC_MPEG4		(0x0008)
#define TCC_VIDEO_CODEC_VP8			(0x0010)
#define TCC_VIDEO_CODEC_HEVC		(0x0020)
#define TCC_VIDEO_CODEC_VP9			(0x0040)

#define TCC_VIDEO_CODEC_STATUS_FAIL 				(0x0000)
#define TCC_VIDEO_CODEC_STATUS_DISPLAYABLE 			(0x0001)
#define TCC_VIDEO_CODEC_STATUS_DECODED 				(0x0002)
#define TCC_VIDEO_CODEC_STATUS_BUF_FULL 			(0x0004)
#define TCC_VIDEO_CODEC_STATUS_MORE_DATA 			(0x0008)
#define TCC_VIDEO_CODEC_STATUS_BUFFER_NOT_CONSUMED 	(0x0010)
#define TCC_VIDEO_CODEC_STATUS_CODEC_FINISH 		(0x0020)

struct tcc_codec_bs_t {
	size_t size;
	void *pa;
	void *va;
	u64 timestamp;
	dma_addr_t dma_addr;
};

struct tcc_codec_fb_t {
	size_t size[3];
	void* pa[3];
	dma_addr_t dma_addr[3];
};

struct tcc_codec_decode_output_t {
	u32 status;
	int displayIndex;
	int decodedIndex;
	int pic_type;
	int picture_structure;
	int top_field_first;
	u32 width;
	u32 height;
	struct tcc_codec_fb_t fb;
};

struct tcc_codec_header_t {
	u32 width;
	u32 height;
	u32 min_framebuffer_cnt;
	u32 profile;
	u32 level;
};

struct codec_if {
	int (*init)(void **h_codec);
	int (*decode)(void *h_codec, struct tcc_codec_bs_t *bs,
		struct tcc_codec_decode_output_t *output);
	int (*parse_seq_header)(void *h_codec, struct tcc_codec_bs_t *bs,
			struct tcc_codec_header_t *hdr);
	int (*register_fb)(void *h_codec, struct tcc_codec_fb_t *fb_array, u32 number);
	int (*clear_displayed_idx)(void *h_codec, u32 displayIndex);
	void (*deinit)(void *h_codec);
	void (*flush)(void *h_codec);
	int (*flush_each) (void *h_codec, struct tcc_codec_decode_output_t *output);
};

#endif