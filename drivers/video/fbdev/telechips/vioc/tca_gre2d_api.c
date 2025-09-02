// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/div64.h>

#include <video/telechips/tcc_gre2d_api.h>
#include <video/telechips/tcc_gre2d.h>

#define DO_NOTHING /* Do nothing to comply for MISRA C:2012 Rule 16.4 */


// lookup table enalbe
static unsigned char gG2D_SRC0_LUTE;

extern G2D_DITHERING_TYPE gG2D_Dithering_type;
G2D_DITHERING_TYPE gG2D_Dithering_type = BIT_TOGGLE_OP;
EXPORT_SYMBOL(gG2D_Dithering_type);

extern unsigned char gG2D_Dithering_en;
unsigned char gG2D_Dithering_en = 0;
EXPORT_SYMBOL(gG2D_Dithering_en);

static uint32_t DitheringMatrix[16] = {
	0u,   8u,  2u, 10u,
	12u,  4u, 14u,  6u,
	3u,  11u,  1u,  9u,
	15u,  7u, 13u,  5u
};

static G2D_RSP_TYPE gre2d_rsp_type;


void gre2d_rsp_interrupt(G2D_RSP_TYPE rsp_type)
{
	gre2d_rsp_type = rsp_type;
}
EXPORT_SYMBOL(gre2d_rsp_interrupt);


void gre2d_set_dma_interrupt(uint32_t uiFlag)
{
	uint32_t enable  = (uint32_t) SET_G2D_DMA_INT_ENABLE;
	uint32_t disable = (uint32_t) SET_G2D_DMA_INT_DISABLE;

	if ((uiFlag & enable) == enable) {
		GRE_2D_SetInterrupt(1u);
		/* prevent KCS warning */
	}

	if ((uiFlag & disable) == disable) {
		GRE_2D_SetInterrupt(0u);
		/* prevent KCS warning */
	}
}
EXPORT_SYMBOL(gre2d_set_dma_interrupt);


void gre2d_waiting_result(G2D_EN grp_enalbe)
{
	G2D_INT_TYPE grp_isr = G2D_INT_NONE;
	uint8_t r_flg = (uint8_t) G2D_INT_R_FLG;

	if (gre2d_rsp_type == G2D_INTERRUPT_TYPE) {
		// regw(GE_IREQ, 0x10000)  // IREQ FLG Clear
		(void)GRE_2D_IntCtrl(1, G2D_INT_ALL, 0, 1);

		gre2d_set_dma_interrupt(SET_G2D_DMA_INT_ENABLE);

		/*-- interrupt type --*/
		// channel enable : Front End Channel 0, 1, 2 enable
		GRE_2D_Enable(grp_enalbe, 1);

	} else if (gre2d_rsp_type == G2D_CHECK_TYPE) {
		/*-- check  type --*/
		gre2d_set_dma_interrupt(SET_G2D_DMA_INT_ENABLE);

		// channel enable : Front End Channel 0, 1, 2 enable
		GRE_2D_Enable(grp_enalbe, 0);

	} else {
		/*-- polling type --*/
		gre2d_set_dma_interrupt(SET_G2D_DMA_INT_DISABLE);

		// channel enable : Front End Channel 0, 1, 2 enable
		GRE_2D_Enable(grp_enalbe, 0);

		// waiting for transfer
		while (r_flg != ((uint8_t)grp_isr & r_flg)) {
			//GE_IREQ
			grp_isr = GRE_2D_IntCtrl(0, G2D_INT_NONE, 0, 0);
		}

		// regw(GE_IREQ, 0x10000)  // IREQ FLG Clear
		(void) GRE_2D_IntCtrl(1, G2D_INT_ALL, 0, 1);
	}

	gre2d_rsp_type = G2D_POLLING_TYPE;
}
EXPORT_SYMBOL(gre2d_waiting_result);


/**
 * @brief Set DMA for 1 channel
 * 
 * @param gre2d_value Channel values
 */
static void
gre2d_1ch_dma_main_func(G2d_1CH_FUNC gre2d_value) {

	G2D_SRC_CTRL src_ctrl;
	G2D_BCH_CTRL_TYPE dest_ctrl;
	struct g2d_op_ctrl op_params;
	struct g2d_ch_ctrl ch_params;
	// channel 0
	GRE_2D_SetFChAddress(FCH0_CH, gre2d_value.src0.add0, gre2d_value.src0.add1, gre2d_value.src0.add2);

	GRE_2D_SetFChPosition(FCH0_CH, gre2d_value.src0);

	ch_params.ch = FCH0_CH;
	ch_params.MABC = MABC_4KBYTE;
	ch_params.LUTE = gG2D_SRC0_LUTE;
	ch_params.SSUV = gre2d_value.src0.src_form.uv_order;
	ch_params.mode = gre2d_value.src0.op_mode;
	ch_params.ZF = ZF_HOB_FILL;
	ch_params.data_form = gre2d_value.src0.src_form;
	GRE_2D_SetFChControl(ch_params);

	GRE_2D_SetFChChromaKey(FCH0_CH,
		gre2d_value.src0.chroma_RY,
		gre2d_value.src0.chroma_GU,
		gre2d_value.src0.chroma_BV);

	GRE_2D_SetFChArithmeticPar(FCH0_CH,
		gre2d_value.src0.arith_RY,
		gre2d_value.src0.arith_GU,
		gre2d_value.src0.arith_BV);

//channel control
	src_ctrl.src0_arith = gre2d_value.src0.arith_mode;
	src_ctrl.src0_chroma_en = gre2d_value.src0.src_chroma_en;
	src_ctrl.src_sel_0 = FCH0_CH;
	src_ctrl.src0_y2r.src_y2r = gre2d_value.src0.src_y2r;
	src_ctrl.src0_y2r.src_y2r_type = gre2d_value.src0.src_y2r_type;

	src_ctrl.src1_arith = AR_NOOP;
	src_ctrl.src1_chroma_en = 0;
	src_ctrl.src_sel_1 = FCH1_CH;
	src_ctrl.src1_y2r.src_y2r = 0;
	src_ctrl.src1_y2r.src_y2r_type = (G2D_Y2R_TYPE)0;

	src_ctrl.src2_arith = AR_NOOP;
	src_ctrl.src2_chroma_en = 0;
	src_ctrl.src_sel_2 = FCH2_CH;
	src_ctrl.src2_y2r.src_y2r = 0;
	src_ctrl.src2_y2r.src_y2r_type = (G2D_Y2R_TYPE)0;
	src_ctrl.src_sel_3 = FCH3_CH;
	GRE_2D_SetSrcCtrl(src_ctrl);


	/* Set operator 0, 1,2  pattern */
	GRE_2D_SetOperator(OP_0, 0, 0, 0, 0);
	GRE_2D_SetOperator(OP_1, 0, 0, 0, 0);
	GRE_2D_SetOperator(OP_2, 0, 0, 0, 0);
	
	op_params.op_set = OP_0;
	op_params.ACON1 = ACON_2;
	op_params.ACON0 = ACON_2;
	op_params.CCON1 = CCON_4;
	op_params.CCON0 = CCON_4;
	op_params.ATUNE = Alpha_11;
	op_params.CSEL = CHROMA_OP0_NOOP;
	op_params.op = GE_ROP_SRC_COPY;
	GRE_2D_SetOperatorCtrl(op_params);

	op_params.op_set = OP_1;
	op_params.ACON1 = ACON_2;
	op_params.ACON0 = ACON_2;
	op_params.CCON1 = CCON_4;
	op_params.CCON0 = CCON_4;
	op_params.ATUNE = Alpha_11;
	op_params.CSEL = CHROMA_OP1_NOOP;
	op_params.op = GE_ROP_SRC_COPY;
	GRE_2D_SetOperatorCtrl(op_params);

	op_params.op_set = OP_2;
	op_params.ACON1 = ACON_2;
	op_params.ACON0 = ACON_2;
	op_params.CCON1 = CCON_4;
	op_params.CCON0 = CCON_4;
	op_params.ATUNE = Alpha_11;
	op_params.CSEL = CHROMA_OP1_NOOP;
	op_params.op = GE_ROP_SRC_COPY;
	GRE_2D_SetOperatorCtrl(op_params);


//back end channel
	GRE_2D_SetBChAddress(DEST_CH, gre2d_value.dest.add0, gre2d_value.dest.add1, gre2d_value.dest.add2);

	GRE_2D_SetBChPosition(DEST_CH,
		gre2d_value.dest.frame_pix_sx,
		gre2d_value.dest.frame_pix_sy,
		gre2d_value.dest.dest_off_sx,
		gre2d_value.dest.dest_off_sy);

	dest_ctrl.MABC = MABC_4KBYTE;
	dest_ctrl.ysel = gre2d_value.dest.ysel;
	dest_ctrl.xsel = gre2d_value.dest.xsel;
	dest_ctrl.converter_en = gre2d_value.dest.converter_en;
	dest_ctrl.converter_mode = gre2d_value.dest.converter_mode;
	dest_ctrl.DSUV = gre2d_value.dest.dest_form.uv_order;
	dest_ctrl.opmode = gre2d_value.dest.op_mode;
	dest_ctrl.dithering_type = gG2D_Dithering_type;
	dest_ctrl.dithering_en = gG2D_Dithering_en;
	dest_ctrl.data_form = gre2d_value.dest.dest_form;

	GRE_2D_SetBChControl(&dest_ctrl);

	if (gG2D_Dithering_en != 0u) {
		GRE_2D_SetDitheringMatrix(DitheringMatrix);
		/* prevent KCS warning */
	}

	//channel enable : Front End Channel 0 enable
	gre2d_waiting_result(GRP_F0);
}


G2D_INT_TYPE gre2d_interrupt_ctrl(
unsigned char wr, G2D_INT_TYPE flag,
unsigned char int_irq, unsigned char int_flg) {
	return GRE_2D_IntCtrl(wr, flag, int_irq, int_flg);
}
EXPORT_SYMBOL(gre2d_interrupt_ctrl);


void gre2d_ImgRotate_Ex(
struct rot_src_params src_params, struct rot_dst_params dst_params,
G2D_OP_MODE ch_mode, G2D_OP_MODE parallel_ch_mode) {

	G2d_1CH_FUNC gre2d_value;
	IMGFMT_CONV_TYPE change_type = NONE;

	if ((src_params.srcfm.format >= (uint32_t) GE_RGB444) &&
		(dst_params.tgtfm.format <  (uint32_t) GE_RGB444)) {
		change_type = R2Y_TYPE;
		/* prevent KCS warning */
	} else if ((src_params.srcfm.format < (uint32_t) GE_RGB444) &&
				(dst_params.tgtfm.format >= (uint32_t) GE_RGB444)) {
		change_type = Y2R_TYPE;
	} else {
		DO_NOTHING /* Do nothing to comply for MISRA C:2012 Rule 16.4 */
	}

	(void)memset(&gre2d_value, 0x00, sizeof(G2d_1CH_FUNC));

// front channel setting
	gre2d_value.src0.add0 = src_params.src0;
	gre2d_value.src0.add1 = src_params.src1;
	gre2d_value.src0.add2 = src_params.src2;
	gre2d_value.src0.frame_pix_sx = src_params.src_imgx;
	gre2d_value.src0.frame_pix_sy = src_params.src_imgy;
	gre2d_value.src0.src_off_sx = src_params.offset_x;
	gre2d_value.src0.src_off_sy = src_params.offset_y;
	gre2d_value.src0.img_pix_sx = src_params.Rimg_x;
	gre2d_value.src0.img_pix_sy = src_params.Rimg_y;
	gre2d_value.src0.win_off_sx = 0;
	gre2d_value.src0.win_off_sy = 0;

// rotate option ����
	gre2d_value.src0.op_mode = ch_mode;

	gre2d_value.src0.src_form = src_params.srcfm;

	gre2d_value.src0.chroma_RY = 0;
	gre2d_value.src0.chroma_GU = 0;
	gre2d_value.src0.chroma_BV = 0;

	gre2d_value.src0.src_chroma_en = 0;
	gre2d_value.src0.arith_mode = AR_NOOP;
	gre2d_value.src0.arith_RY = 0;
	gre2d_value.src0.arith_GU = 0;
	gre2d_value.src0.arith_BV = 0;


// YUV to RGB �� front image format coverter
	if (change_type == Y2R_TYPE) {
		gre2d_value.src0.src_y2r = 1;
		gre2d_value.src0.src_y2r_type = Y2R_TYP0;
	}

// back end channel setting
	gre2d_value.dest.add0 = dst_params.tgt0;
	gre2d_value.dest.add1 = dst_params.tgt1;
	gre2d_value.dest.add2 = dst_params.tgt2;
	gre2d_value.dest.frame_pix_sx = dst_params.des_imgx;
	gre2d_value.dest.frame_pix_sy = dst_params.des_imgy;
	gre2d_value.dest.dest_off_sx = dst_params.offset_x;
	gre2d_value.dest.dest_off_sy = dst_params.offset_y;
	gre2d_value.dest.ysel = 0;
	gre2d_value.dest.xsel = 0;

// RGB to YUV �� back end image format coverter
	if (change_type == R2Y_TYPE) {
		gre2d_value.dest.converter_en = 1;
		gre2d_value.dest.converter_mode = R2Y_TYP0;
	}

	if ((uint8_t)parallel_ch_mode > (uint8_t)ROTATE_270) {
		gre2d_value.dest.op_mode = NOOP;
	} else {
		gre2d_value.dest.op_mode = parallel_ch_mode;
	}

	gre2d_value.dest.dest_form = dst_params.tgtfm;

	gre2d_1ch_dma_main_func(gre2d_value);
}
EXPORT_SYMBOL(gre2d_ImgRotate_Ex);

