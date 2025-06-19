/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_WMIXER_IOCTRL_H
#define TCC_WMIXER_IOCTRL_H

#include "tcc_video_private.h"

#ifndef CONFIG_ARCH_TELECHIPS   /* for Android framework */

#ifndef ADDRESS_ALIGNED
#define ADDRESS_ALIGNED
#define ALIGN_BIT  (0x8-1)
#define BIT_0      (3)
#define GET_ADDR_YUV42X_spY(Base_addr) \
	(((((unsigned int)Base_addr) + ALIGN_BIT) >> BIT_0) << BIT_0)
#define GET_ADDR_YUV42X_spU(Yaddr, x, y) \
	(((((unsigned int)Yaddr+(x*y)) + ALIGN_BIT) >> BIT_0) << BIT_0)
#define GET_ADDR_YUV422_spV(Uaddr, x, y) \
	(((((unsigned int)Uaddr+(x*y/2)) + ALIGN_BIT) >> BIT_0) << BIT_0)
#define GET_ADDR_YUV420_spV(Uaddr, x, y) \
	(((((unsigned int)Uaddr+(x*y/4)) + ALIGN_BIT) >> BIT_0) << BIT_0)
#endif

/* RDMA/WDMA : RGB Swap */
#define VIOC_SWAP_RGB           (0)
#define VIOC_SWAP_RBG           (1)
#define VIOC_SWAP_GRB           (2)
#define VIOC_SWAP_GBR           (3)
#define VIOC_SWAP_BRG           (4)
#define VIOC_SWAP_BGR           (5)

/* RDMA/WDMA : Image Format */
#define VIOC_IMG_FMT_BPP1       (0)  // 1bit
#define VIOC_IMG_FMT_BPP2       (1)  // 2bits
#define VIOC_IMG_FMT_BPP4       (2)  // 4bits
#define VIOC_IMG_FMT_BPP8       (3)  // 1byte
#define VIOC_IMG_FMT_RGB332     (8)  // 1byte
#define VIOC_IMG_FMT_ARGB4444   (9)  // 2bytes
#define VIOC_IMG_FMT_RGB565     (10) // 2bytes
#define VIOC_IMG_FMT_ARGB1555   (11) // 2bytes
#define VIOC_IMG_FMT_ARGB8888   (12) // 4bytes
#define VIOC_IMG_FMT_ARGB6666_4 (13) // 4bytes
#define VIOC_IMG_FMT_RGB888     (14) // 3bytes
#define VIOC_IMG_FMT_ARGB6666_3 (15) // 3bytes
#define VIOC_IMG_FMT_COMP       (16) // 4bytes
#define VIOC_IMG_FMT_DECOMP     (VIOC_IMG_FMT_COMP)
#define VIOC_IMG_FMT_444SEP     (21) // 3bytes
#define VIOC_IMG_FMT_UYVY       (22) // 2bytes
#define VIOC_IMG_FMT_VYUY       (23) // 2bytes
#define VIOC_IMG_FMT_YUV420SEP  (24) // 1,1byte
#define VIOC_IMG_FMT_YUV422SEP  (25) // 1,1byte
#define VIOC_IMG_FMT_YUYV       (26) // 2bytes
#define VIOC_IMG_FMT_YVYU       (27) // 2bytes
#define VIOC_IMG_FMT_YUV420IL0  (28) // 1,2byte
#define VIOC_IMG_FMT_YUV420IL1  (29) // 1,2byte
#define VIOC_IMG_FMT_YUV422IL0  (30) // 1,2bytes
#define VIOC_IMG_FMT_YUV422IL1  (31) // 1,2bytes

#endif


typedef enum {
	WMIXER_POLLING,
	WMIXER_INTERRUPT,
	WMIXER_NOWAIT
} WMIXER_RESPONSE_TYPE;

typedef struct {
	unsigned int rsp_type; // wmix response type

	unsigned int src_y_addr; // source y address
	unsigned int src_u_addr; // source u address
	unsigned int src_v_addr; // source v address
	unsigned int src_fmt;    // source image format
	unsigned int src_rgb_swap;
	unsigned int src_img_width; // source image width
	unsigned int src_img_height; // source image height
	unsigned int src_win_left;
	unsigned int src_win_top;
	unsigned int src_win_right;
	unsigned int src_win_bottom;

	// 0, 8: 8bit normal
	// 10: 10bit data type(16bit)
	// 11: 10bit data type(real 10bit)
	// 0x10: map converter
	// 0x20: dtrc converter
	unsigned int src_fmt_ext_info;
	hevc_MapConv_info_t mapConv_info;
	vp9_compressed_info_t dtrcConv_info;

	unsigned int dst_y_addr; // destination image address
	unsigned int dst_u_addr; // destination image address
	unsigned int dst_v_addr; // destination image address
	unsigned int dst_fmt;    // destination image format
	unsigned int dst_rgb_swap;
	unsigned int dst_img_width;  // destination image width
	unsigned int dst_img_height; // destination image height
	unsigned int dst_win_left;
	unsigned int dst_win_top;
	unsigned int dst_win_right;
	unsigned int dst_win_bottom;

	unsigned int dst_fmt_ext_info;
} WMIXER_INFO_TYPE;

typedef struct {
	unsigned int rsp_type; // wmix response type

	unsigned int src_y_addr; // source y address
	unsigned int src_u_addr; // source u address
	unsigned int src_v_addr; // source v address
	unsigned int src_fmt;    // source image format
	unsigned int src_img_width;
	unsigned int src_img_height;
	unsigned int src_win_left;
	unsigned int src_win_top;
	unsigned int src_win_right;
	unsigned int src_win_bottom;

	unsigned int src_fmt_ext_info;
	hevc_MapConv_info_t mapConv_info;
	vp9_compressed_info_t dtrcConv_info;

	unsigned int dst_y_addr; // destination image address
	unsigned int dst_u_addr; // destination image address
	unsigned int dst_v_addr; // destination image address
	unsigned int dst_fmt;    // destination image format
	unsigned int dst_img_width;
	unsigned int dst_img_height;
	unsigned int dst_win_left;
	unsigned int dst_win_top;
	unsigned int dst_win_right;
	unsigned int dst_win_bottom;

	unsigned int interlaced;
	// for only TCC898x
	unsigned int mc_num;
	unsigned int dst_fmt_ext_info;
} WMIXER_ALPHASCALERING_INFO_TYPE;

typedef struct {
	unsigned char rsp_type;
	unsigned char region;

	unsigned char src0_fmt;
	unsigned char src0_layer;
	unsigned short src0_acon0;
	unsigned short src0_acon1;
	unsigned short src0_ccon0;
	unsigned short src0_ccon1;
	unsigned short src0_rop_mode;
	unsigned short src0_asel;
	unsigned short src0_alpha0;
	unsigned short src0_alpha1;
	unsigned int src0_Yaddr;
	unsigned int src0_Uaddr;
	unsigned int src0_Vaddr;
	unsigned short src0_width;
	unsigned short src0_height;
	unsigned short src0_dst_width;
	unsigned short src0_dst_height;
	unsigned char src0_use_scaler;
	unsigned short src0_winLeft;
	unsigned short src0_winTop;
	unsigned short src0_winRight;
	unsigned short src0_winBottom;

	unsigned char src1_fmt;
	unsigned char src1_layer;
	unsigned short src1_acon0;
	unsigned short src1_acon1;
	unsigned short src1_ccon0;
	unsigned short src1_ccon1;
	unsigned short src1_rop_mode;
	unsigned short src1_asel;
	unsigned short src1_alpha0;
	unsigned short src1_alpha1;
	unsigned int src1_Yaddr;
	unsigned int src1_Uaddr;
	unsigned int src1_Vaddr;
	unsigned short src1_width;
	unsigned short src1_height;
	unsigned short src1_winLeft;
	unsigned short src1_winTop;
	unsigned short src1_winRight;
	unsigned short src1_winBottom;

	unsigned char dst_fmt;
	unsigned int dst_Yaddr;
	unsigned int dst_Uaddr;
	unsigned int dst_Vaddr;
	unsigned int dst_rgb_swap;
	unsigned short dst_width;
	unsigned short dst_height;
	unsigned short dst_winLeft;
	unsigned short dst_winTop;
	unsigned short dst_winRight;
	unsigned short dst_winBottom;
} WMIXER_ALPHABLENDING_TYPE;

typedef struct {
	unsigned char rsp_type;
	unsigned char region;

	unsigned char src0_fmt;
	unsigned char src0_layer;
	unsigned short src0_acon0;
	unsigned short src0_acon1;
	unsigned short src0_ccon0;
	unsigned short src0_ccon1;
	unsigned short src0_rop_mode;
	unsigned short src0_asel;
	unsigned short src0_alpha0;
	unsigned short src0_alpha1;
	unsigned int src0_Yaddr;
	unsigned int src0_Uaddr;
	unsigned int src0_Vaddr;
	unsigned short src0_width;
	unsigned short src0_height;
	unsigned short src0_dst_width;
	unsigned short src0_dst_height;
	unsigned char src0_use_scaler;
	unsigned short src0_winLeft;
	unsigned short src0_winTop;
	unsigned short src0_winRight;
	unsigned short src0_winBottom;

	unsigned char src1_fmt;
	unsigned char src1_layer;
	unsigned short src1_acon0;
	unsigned short src1_acon1;
	unsigned short src1_ccon0;
	unsigned short src1_ccon1;
	unsigned short src1_rop_mode;
	unsigned short src1_asel;
	unsigned short src1_alpha0;
	unsigned short src1_alpha1;
	unsigned int src1_Yaddr;
	unsigned int src1_Uaddr;
	unsigned int src1_Vaddr;
	unsigned short src1_width;
	unsigned short src1_height;
	unsigned short src1_dst_width;
	unsigned short src1_dst_height;
	unsigned char src1_use_scaler;
	unsigned short src1_winLeft;
	unsigned short src1_winTop;
	unsigned short src1_winRight;
	unsigned short src1_winBottom;

	unsigned char dst_fmt;
	unsigned int dst_Yaddr;
	unsigned int dst_Uaddr;
	unsigned int dst_Vaddr;
	unsigned int dst_rgb_swap;
	unsigned short dst_width;
	unsigned short dst_height;
	unsigned short dst_winLeft;
	unsigned short dst_winTop;
	unsigned short dst_winRight;
	unsigned short dst_winBottom;
} WMIXER_ALPHABLENDING_EX_TYPE;

//VIOC Block ID infomation
typedef struct {
	unsigned char rdma[4];
	unsigned char wmixer;
	unsigned char wdma;
} WMIXER_VIOC_INFO;

#define WMIXER 'w'
#define TCC_WMIXER_IOCTRL               0x01U
#define TCC_WMIXER_IOCTRL_KERNEL        0x02U
#define TCC_WMIXER_ALPHA_SCALING        0x04U
#define TCC_WMIXER_ALPHA_SCALING_KERNEL 0x08U
#define TCC_WMIXER_ALPHA_MIXING         0x10U
#define TCC_WMIXER_ALPHA_MIXING_KERNEL  0x12U
#define TCC_WMIXER_ALPHA_MIXING_EX      0x13U

#define TCC_WMIXER_VIOC_INFO        _IOR(WMIXER, 0x20, WMIXER_VIOC_INFO)
#define TCC_WMIXER_VIOC_INFO_KERNEL _IOR(WMIXER, 0x40, WMIXER_VIOC_INFO)

#endif
