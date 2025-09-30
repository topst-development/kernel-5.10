/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef GRE2D_H
#define GRE2D_H

#include <video/telechips/tcc_types.h>
#include "tcc_gre2d_reg.h"
#include <video/telechips/tcc_gre2d_type.h>

/**
 * @brief Setting graphic engine interrupt
 * 
 * @param onoff Whether to enable channel
 */
extern void GRE_2D_SetInterrupt(uint32_t onoff);


/**
 * @brief Setting front-end channel address 0, 1, 2
 * 
 * @param ch   Channel number
 * @param add0 address 0
 * @param add1 address 1
 * @param add2 address 2
 */
extern void GRE_2D_SetFChAddress(
G2D_CHANNEL ch, uint32_t add0, uint32_t add1, uint32_t add2);


/**
 * @brief Set position front channel
 * 
 * @param ch  channel number
 * @param src position data 
 */
extern void GRE_2D_SetFChPosition(G2D_CHANNEL ch, G2D_FCH_TYPE src);


struct g2d_ch_ctrl {
	G2D_CHANNEL ch;
	G2D_MABC_TYPE MABC;
	unsigned char LUTE;
	unsigned char SSUV;
	G2D_OP_MODE mode;
	G2D_ZF_TYPE ZF;
	G2D_FMT_CTRL data_form;
};

/**
 * @brief Setting front-end channel control
 * 
 * @param ch_params 
 */
extern void GRE_2D_SetFChControl(struct g2d_ch_ctrl ch_params);


/**
 * @brief Setting front-end channel chroma key
 * 
 * @param ch Channel number
 * @param RY Red | Luminance
 * @param GU Green | Chrominance blue
 * @param BV Blue | Chrominance red
 */
extern void GRE_2D_SetFChChromaKey(
G2D_CHANNEL ch, unsigned char RY, unsigned char GU, unsigned char BV);


/**
 * @brief Setting front-end channel arithmetic parameters
 * 
 * @param ch Channel number
 * @param RY Red | Luminance
 * @param GU Green | Chrominance blue
 * @param BV Blue | Chrominance red
 */
extern void GRE_2D_SetFChArithmeticPar(
G2D_CHANNEL ch, unsigned char RY, unsigned char GU, unsigned char BV);


/**
 * @brief Control graphic engine sources
 * 
 * @param g2d_ctrl source parameters
 */
extern void GRE_2D_SetSrcCtrl(G2D_SRC_CTRL g2d_ctrl);


/**
 * @brief Setting graphic engine operator
 * 
 * @param op_set operator type
 * @param alpha  alpha value
 * @param RY Red | Luminance
 * @param GU Green | Chrominance blue
 * @param BV Blue | Chrominance red
 */
extern void GRE_2D_SetOperator(
G2D_OP_TYPE op_set, unsigned short alpha,
unsigned char RY, unsigned char GU, unsigned char BV);


struct g2d_op_ctrl {
	G2D_OP_TYPE op_set;
	G2D_OP_ACON ACON1;
	G2D_OP_ACON ACON0;
	G2D_OP_CCON CCON1;
	G2D_OP_CCON CCON0;
	G2D_OP_ATUNE ATUNE;
	G2D_OP_CHROMA CSEL;
	GE_ROP_TYPE op;
};

/**
 * @brief Setting operator control register
 *
 * @param op_params operator control parameters
 */
extern void GRE_2D_SetOperatorCtrl(struct g2d_op_ctrl op_params);


/**
 * @brief Setting back-end channel address 0, 1, 2
 * 
 * @param ch   Channel number
 * @param add0 address 0
 * @param add1 address 1
 * @param add2 address 2
 */
extern void GRE_2D_SetBChAddress(
G2D_CHANNEL ch, uint32_t add0, uint32_t add1, uint32_t add2);


/**
 * @brief Setting back-end channel position
 *
 * @param ch 
 * @param frameps_x frame width size
 * @param frameps_y frame height size
 * @param poffset_x frame offset x
 * @param poffset_y frame offset y
 */
extern void GRE_2D_SetBChPosition(
G2D_CHANNEL ch, uint32_t frameps_x, uint32_t frameps_y, uint32_t poffset_x, uint32_t poffset_y);


/**
 * @brief Setting back-end channel control
 * 
 * @param g2d_bch_ctrl back-end control parameters
 */
extern void GRE_2D_SetBChControl(const G2D_BCH_CTRL_TYPE *g2d_bch_ctrl);

/**
 * @brief Setting graphic engine dithering matrix
 * 
 * @param Matrix Ditering matrix
 */
extern void GRE_2D_SetDitheringMatrix(const uint32_t *Matrix);


/**
 * @brief Control graphic engine channel enable
 * 
 * @param grp_enalbe Channel number
 * @param int_en     Whether to enable channel
 */
extern void GRE_2D_Enable(G2D_EN grp_enalbe, unsigned char int_en);


/**
 * @brief Control graphic engine interrupt
 *
 * @param wr      write / read
 * @param flag    interrupt type
 * @param int_irq interrupt request
 * @param int_flg flag bit
 *
 * @return interrupt type
 */
extern G2D_INT_TYPE GRE_2D_IntCtrl(
unsigned char wr, G2D_INT_TYPE flag, unsigned char int_irq, unsigned char int_flg);

extern void __iomem *GRE_2D_GetAddress(void);

#endif//GRE2D_H
