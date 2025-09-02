/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef GRE2D_API_H
#define GRE2D_API_H

#include <video/telechips/tcc_gre2d_type.h>

/*------------ Graphice engine special application function --------------*/

#define SET_G2D_DMA_INT_ENABLE         0x00000001
#define SET_G2D_DMA_INT_DISABLE        0x00000002

typedef enum {
	G2D_POLLING_TYPE = 0,
	G2D_INTERRUPT_TYPE,
	G2D_CHECK_TYPE,
	G2D_RSP_MAX
} G2D_RSP_TYPE;

extern void gre2d_set_dma_interrupt(uint32_t uiFlag);
extern void gre2d_rsp_interrupt(G2D_RSP_TYPE rsp_type);


/**
 * @brief Control graphic engine interrupt
 * 
 * @param wr      1:write | 0:read
 * @param flag    interrupt type
 * @param int_irq interrupt request
 * @param int_flg flag bit
 *
 * @return interrupt type
 */
G2D_INT_TYPE gre2d_interrupt_ctrl(
unsigned char wr, G2D_INT_TYPE flag, unsigned char int_irq, unsigned char int_flg);


struct rot_src_params {
	uint32_t src0, src1, src2;
	G2D_FMT_CTRL srcfm;
	uint32_t src_imgx, src_imgy;
	uint32_t offset_x, offset_y;
	uint32_t Rimg_x, Rimg_y;
};

struct rot_dst_params {
	uint32_t tgt0, tgt1, tgt2;
	G2D_FMT_CTRL tgtfm;
	uint32_t des_imgx, des_imgy;
	uint32_t offset_x, offset_y;
};

/**
 * @brief Rotate image
 * @param src_params        Source info
 * @param dst_params        Output info
 * @param ch_mode			G2D_OP_MODE
 * @param parallel_ch_mode 	G2D_OP_MODE
 */
extern void gre2d_ImgRotate_Ex(
struct rot_src_params src_params, struct rot_dst_params dst_params,
G2D_OP_MODE ch_mode, G2D_OP_MODE parallel_ch_mode);


extern void gre2d_waiting_result(G2D_EN grp_enalbe);

#endif // GRE2D_API_H

