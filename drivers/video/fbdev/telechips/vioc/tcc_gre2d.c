// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_dma.h>

#include <video/telechips/tcc_gre2d.h>

#define DO_NOTHING /* Do nothing to comply for MISRA C:2012 Rule 16.4 */

static void __iomem *pGre2D_reg;


void GRE_2D_SetInterrupt(uint32_t onoff)
{
	(void) onoff;
	//PPIC pHwPIC = (volatile PPIC)tcc_p2v(HwPIC_BASE);
	//
	//if (onoff) {
	//	BITSET(pHwPIC->CLR1, HwINT1_G2D);
	//	BITCLR(pHwPIC->POL1, HwINT1_G2D);
	//	BITSET(pHwPIC->SEL1, HwINT1_G2D);
	//	BITSET(pHwPIC->IEN1, HwINT1_G2D);
	//	BITSET(pHwPIC->MODE1, HwINT1_G2D);
	//} else {
	//	BITCLR(pHwPIC->IEN1, HwINT1_G2D);
	//}
}


void GRE_2D_SetFChAddress(
G2D_CHANNEL ch, uint32_t add0, uint32_t add1, uint32_t add2) {

	void __iomem *reg = GRE_2D_GetAddress();

	switch (ch) {
		case FCH0_CH:
			__raw_writel(add0, reg + 0x00);
			__raw_writel(add1, reg + 0x04);
			__raw_writel(add2, reg + 0x08);
			break;

		case FCH1_CH:
			__raw_writel(add0, reg + 0x20);
			__raw_writel(add1, reg + 0x24);
			__raw_writel(add2, reg + 0x28);
			break;

		default:
			DO_NOTHING /* Do nothing to comply for MISRA C:2012 Rule 16.4 */
			break;
	}
}


void GRE_2D_SetFChPosition(G2D_CHANNEL ch, G2D_FCH_TYPE src) {

	void __iomem *reg = GRE_2D_GetAddress();
	uint32_t value = 0x00u;

	switch (ch) {
	case FCH0_CH:
		value = (__raw_readl(reg + 0x0c) & ~(0x0FFF0FFFu));
		value |= ((src.frame_pix_sy<<16u) | src.frame_pix_sx);
		__raw_writel(value, reg + 0x0c);

		value = (__raw_readl(reg + 0x10) & ~(0x0FFF0FFFu));
		value |= ((src.src_off_sy<<16u) | src.src_off_sx);
		__raw_writel(value, reg + 0x10);

		value = (__raw_readl(reg + 0x14) & ~(0x0FFF0FFFu));
		value |= ((src.img_pix_sy<<16u) | src.img_pix_sx);
		__raw_writel(value, reg + 0x14);

		value = (__raw_readl(reg + 0x18) & ~(0x0FFF0FFFu));
		value |= ((src.win_off_sy<<16u) | src.win_off_sx);
		__raw_writel(value, reg + 0x18);
		break;

	case FCH1_CH:
		value = (__raw_readl(reg + 0x2c) & ~(0x0FFF0FFFu));
		value |= ((src.frame_pix_sy<<16u) | src.frame_pix_sx);
		__raw_writel(value, reg + 0x2c);

		value = (__raw_readl(reg + 0x30) & ~(0x0FFF0FFFu));
		value |= ((src.src_off_sy<<16u) | src.src_off_sx);
		__raw_writel(value, reg + 0x30);

		value = (__raw_readl(reg + 0x34) & ~(0x0FFF0FFFu));
		value |= ((src.img_pix_sy<<16u) | src.img_pix_sx);
		__raw_writel(value, reg + 0x34);

		value = (__raw_readl(reg + 0x38) & ~(0x0FFF0FFFu));
		value |= ((src.win_off_sy<<16u) | src.win_off_sx);
		__raw_writel(value, reg + 0x38);
		break;

	default:
		DO_NOTHING /* Do nothing to comply for MISRA C:2012 Rule 16.4 */
		break;
	}
}


void GRE_2D_SetFChControl(struct g2d_ch_ctrl ch_params) {

	void __iomem *reg = GRE_2D_GetAddress();
	uint32_t value = 0x00u;

	if (FCH0_CH == ch_params.ch) {
		value = (__raw_readl(reg + 0x1c) & ~(0x0F00FFFFu));
		value |= (	((uint32_t)ch_params.LUTE << 12u) |
					((uint32_t)ch_params.SSUV << 11u) |
					(((uint32_t)ch_params.mode << 8u) & (uint16_t)HwGE_FCHO_OPMODE) |
					((uint32_t)ch_params.ZF << 5u) |
					(ch_params.data_form.format & (uint8_t)HwGE_FCHO_SDFRM) |
					((ch_params.data_form.data_swap << 24u) & (uint32_t)HwGE_FCH_SSB));
		__raw_writel(value, reg + 0x1c);
	} else if (FCH1_CH == ch_params.ch) {
		value = (__raw_readl(reg + 0x3c) & ~(0x0F00FFFFu));
		value |= (((uint32_t)ch_params.LUTE << 12u) | ((uint32_t)ch_params.SSUV << 11u)
			| (((uint32_t)ch_params.mode << 8u) & HwGE_FCHO_OPMODE)
			| ((uint32_t)ch_params.ZF << 5u) | (ch_params.data_form.format & (uint8_t)HwGE_FCHO_SDFRM)
			| ((ch_params.data_form.data_swap << 24u) & (uint32_t)HwGE_FCH_SSB));
		__raw_writel(value, reg + 0x3c);
	} else {
		DO_NOTHING /* Do nothing to comply for MISRA C:2012 Rule 16.4 */
	}
}


void GRE_2D_SetFChChromaKey(
G2D_CHANNEL ch, unsigned char RY, unsigned char GU, unsigned char BV) {

	void __iomem *reg = GRE_2D_GetAddress();
	uint32_t value = 0x00u;

	if (FCH0_CH == ch) {
		value = (__raw_readl(reg + 0x80) & ~(0x00FFFFFFu));
		value |= ((((uint32_t)RY << 16u) & 0xFF0000u) |
				(((uint32_t)GU << 8u) & 0xFF00u) |
				((uint32_t)BV & 0xFFu));
			__raw_writel(value, reg + 0x80);
	}
}


void GRE_2D_SetFChArithmeticPar(
G2D_CHANNEL ch, unsigned char RY, unsigned char GU, unsigned char BV) {

	void __iomem *reg = GRE_2D_GetAddress();
	uint32_t value = 0x00u;

	if (FCH0_CH == ch) {
		value = (__raw_readl(reg + 0x84) & ~(0x00FFFFFFu));
		value |= ((((uint32_t)RY << 16u) & 0xFF0000u) | (((uint32_t)GU <<  8u) & 0xFF00u) | ( (uint32_t)BV & 0xFFu));
		__raw_writel(value, reg + 0x84);
	}
}


void GRE_2D_SetSrcCtrl(G2D_SRC_CTRL g2d_ctrl) {

	uint32_t sf_ctrl_reg = 0u;
	uint32_t sa_ctrl_reg = 0u;
	uint32_t value = 0x00u;
	void __iomem *reg = GRE_2D_GetAddress();
	
	uint32_t src0_y2r = (uint32_t) g2d_ctrl.src0_y2r.src_y2r;
	uint32_t src1_y2r = (uint32_t) g2d_ctrl.src1_y2r.src_y2r;
	uint32_t src2_y2r = (uint32_t) g2d_ctrl.src2_y2r.src_y2r;
	uint32_t src0_y2r_type = (uint32_t) g2d_ctrl.src0_y2r.src_y2r_type;
	uint32_t src1_y2r_type = (uint32_t) g2d_ctrl.src1_y2r.src_y2r_type;
	uint32_t src2_y2r_type = (uint32_t) g2d_ctrl.src2_y2r.src_y2r_type;
	uint8_t src_sel0 = (uint8_t) g2d_ctrl.src_sel_0;
	uint8_t src_sel1 = (uint8_t) g2d_ctrl.src_sel_1;
	uint8_t src_sel2 = (uint8_t) g2d_ctrl.src_sel_2;
	uint8_t src_sel3 = (uint8_t) g2d_ctrl.src_sel_3;
	uint32_t src0_arith = (uint32_t) g2d_ctrl.src0_arith;
	uint32_t src1_arith = (uint32_t) g2d_ctrl.src1_arith;
	uint32_t src2_arith = (uint32_t) g2d_ctrl.src2_arith;
	uint32_t src0_chroma_en = (uint32_t) g2d_ctrl.src0_chroma_en;
	uint32_t src1_chroma_en = (uint32_t) g2d_ctrl.src1_chroma_en;
	uint32_t src2_chroma_en = (uint32_t) g2d_ctrl.src2_chroma_en;

	uint32_t s0_y2ren = (uint32_t) Hw2D_SFCTRL_S0_Y2REN;
	uint32_t s1_y2ren = (uint32_t) Hw2D_SFCTRL_S1_Y2REN;
	uint32_t s2_y2ren = (uint32_t) Hw2D_SFCTRL_S2_Y2REN;
	uint32_t s0_y2rmode = (uint32_t) Hw2D_SFCTRL_S0_Y2RMODE;
	uint32_t s1_y2rmode = (uint32_t) Hw2D_SFCTRL_S1_Y2RMODE;
	uint32_t s2_y2rmode = (uint32_t) Hw2D_SFCTRL_S2_Y2RMODE;

	uint8_t s0_sel = (uint8_t) Hw2D_SFCTRL_S0_SEL;
	uint8_t s1_sel = (uint8_t) Hw2D_SFCTRL_S1_SEL;
	uint8_t s2_sel = (uint8_t) Hw2D_SFCTRL_S2_SEL;
	uint8_t s3_sel = (uint8_t) Hw2D_SFCTRL_S3_SEL;
	uint8_t s0_arithmode = (uint8_t) Hw2D_SACTRL_S0_ARITHMODE;
	uint8_t s1_arithmode = (uint8_t) Hw2D_SACTRL_S1_ARITHMODE;
	uint16_t s2_arithmode = (uint16_t) Hw2D_SACTRL_S2_ARITHMODE;
	uint32_t s0_chromaen = (uint32_t) Hw2D_SACTRL_S0_CHROMAEN;
	uint32_t s1_chromaen = (uint32_t) Hw2D_SACTRL_S1_CHROMAEN;
	uint32_t s2_chromaen = (uint32_t) Hw2D_SACTRL_S2_CHROMAEN;

	// source YUV to RGB converter enable sf_ctrl
	sf_ctrl_reg |= (((src0_y2r << 24u) & s0_y2ren) |
					((src1_y2r << 25u) & s1_y2ren) |
					((src2_y2r << 26u) & s2_y2ren) );

	// source YUV to RGB coverter type sf_ctrl
	sf_ctrl_reg |= (((src0_y2r_type << 16u) & s0_y2rmode) |
					((src1_y2r_type << 18u) & s1_y2rmode) |
					((src2_y2r_type << 20u) & s2_y2rmode));

	// source select  sf_ctrl
	sf_ctrl_reg |=  (((src_sel0) & s0_sel) |
					((src_sel1 << 2u) & s1_sel) |
					((src_sel2 << 4u) & s2_sel) |
					((src_sel3 << 6u) & s3_sel));

	// source arithmetic mode sa_ctrl
	sa_ctrl_reg |= (((src0_arith) & s0_arithmode) |
					((src1_arith << 4u) & s1_arithmode) |
					((src2_arith << 8u) & s2_arithmode));

	// source chroma key enable : for arithmetic	sa_ctrl
	sa_ctrl_reg |= (((src0_chroma_en << 16u) & s0_chromaen) |
					((src1_chroma_en << 17u) & s1_chromaen) |
					((src2_chroma_en << 18u) & s2_chromaen));

	value = sf_ctrl_reg;
	__raw_writel(value, reg + 0xa0);

	value = sa_ctrl_reg;
	__raw_writel(value, reg + 0xa4);
}


void GRE_2D_SetOperator(
G2D_OP_TYPE op_set, unsigned short alpha,
unsigned char RY, unsigned char GU, unsigned char BV) {

	void __iomem *reg = GRE_2D_GetAddress();
	uint32_t value = 0x00u;

	if (OP_0 == op_set) {
		value = (__raw_readl(reg + 0xc0) & ~(0x00FFFFFFu));
		value |= ( (((uint32_t)RY << 16u) & (uint32_t)HwGE_PAT_RY) |
				   (((uint16_t)GU <<  8u) & (uint16_t)HwGE_PAT_GU) | 
				   (BV & (uint8_t)HwGE_PAT_BV) );

		__raw_writel(value, reg + 0xc0);

		value = (__raw_readl(reg + 0xb0) & ~(0x0000FFFFu));
		value |= (uint32_t)((alpha) & (uint16_t)HwGE_ALPHA);
		__raw_writel(value, reg + 0xb0);
	}
}


void GRE_2D_SetOperatorCtrl(struct g2d_op_ctrl op_params) {

	void __iomem *reg = GRE_2D_GetAddress();
	uint32_t value = 0x00u;

	if (OP_0 == op_params.op_set) {
		value = (__raw_readl(reg + 0xd0) & ~(0xFFFFFFFFu));
		value |= ((((uint32_t)op_params.ACON1 << 28u) & (uint32_t)HwGE_OP_CTRL_ACON1) |
				(((uint32_t)op_params.ACON0 << 24u) & (uint32_t)HwGE_OP_CTRL_ACON0) |
				(((uint32_t)op_params.CCON1 << 21u) & (uint32_t)HwGE_OP_CTRL_CCON1) |
				(((uint32_t)op_params.CCON0 << 16u) & (uint32_t)HwGE_OP_CTRL_CCON0) |
				(((uint32_t)op_params.ATUNE << 12u) & (uint16_t)HwGE_OP_CTRL_ATUNE) |
				(((uint32_t)op_params.CSEL  <<  8u) & (uint16_t)HwGE_OP_CTRL_CSEL)  |
				((uint32_t)op_params.op & (uint8_t)HwGE_OP_CTRL_OPMODE));

		__raw_writel(value, reg + 0xd0);
	}
}


void GRE_2D_SetBChAddress(
G2D_CHANNEL ch, uint32_t add0, uint32_t add1, uint32_t add2) {

	void __iomem *reg = GRE_2D_GetAddress();

	if (ch == DEST_CH) {
		__raw_writel(add0, reg + 0xe0);
		__raw_writel(add1, reg + 0xe4);
		__raw_writel(add2, reg + 0xe8);
	}
}
EXPORT_SYMBOL(GRE_2D_SetBChAddress);


void GRE_2D_SetBChPosition(
G2D_CHANNEL ch, uint32_t frameps_x, uint32_t frameps_y,
uint32_t poffset_x, uint32_t poffset_y) {

	void __iomem *reg = GRE_2D_GetAddress();
	uint32_t value = 0x00u;

	if (ch == DEST_CH) {
		value = (__raw_readl(reg + 0xec) & ~(0x0FFF0FFFu));
		value |= ((frameps_y << 16u) | frameps_x);
		__raw_writel(value, reg + 0xec);

		value = (__raw_readl(reg + 0xf0) & ~(0x0FFF0FFFu));
		value |= ((poffset_y << 16u) | poffset_x);
		__raw_writel(value, reg + 0xf0);
	}
}


void GRE_2D_SetBChControl(
const G2D_BCH_CTRL_TYPE *g2d_bch_ctrl) {

	uint32_t BCH_ctrl_reg = 0u;
	void __iomem *reg = GRE_2D_GetAddress();

	BCH_ctrl_reg |= (((uint32_t)g2d_bch_ctrl->MABC << 21u) & (uint32_t) HwGE_BCH_DCTRL_MABC);
	BCH_ctrl_reg |= (((uint32_t)g2d_bch_ctrl->ysel << 18u) & (uint32_t) HwGE_BCH_DCTRL_YSEL);
	BCH_ctrl_reg |= (((uint32_t)g2d_bch_ctrl->xsel << 16u) & (uint32_t) HwGE_BCH_DCTRL_XSEL);
	BCH_ctrl_reg |= (((uint32_t)g2d_bch_ctrl->converter_en << 15u) & (uint32_t) HwGE_BCH_DCTRL_CEN);
	BCH_ctrl_reg |= (((uint32_t)g2d_bch_ctrl->converter_mode << 13u) & (uint16_t) HwGE_BCH_DCTRL_CMODE);
	BCH_ctrl_reg |= (((uint32_t)g2d_bch_ctrl->DSUV << 11u) & (uint32_t) HwGE_BCH_DCTRL_DSUV);
	BCH_ctrl_reg |= (((uint32_t)g2d_bch_ctrl->opmode << 8u) & (uint16_t) HwGE_BCH_DCTRL_OPMODE);
	BCH_ctrl_reg |= (((uint32_t)g2d_bch_ctrl->dithering_type<<6u) & (uint32_t) HwGE_BCH_DCTRL_DOP);
	BCH_ctrl_reg |= (((uint32_t)g2d_bch_ctrl->dithering_en << 5u) & (uint32_t) HwGE_BCH_DCTRL_DEN);
	BCH_ctrl_reg |= ( (uint32_t)g2d_bch_ctrl->data_form.format & (uint8_t) HwGE_BCH_DCTRL_DDFRM);
	BCH_ctrl_reg |= (((uint32_t)g2d_bch_ctrl->data_form.data_swap << 24u) & (uint32_t) HwGE_DCH_SSB);

	__raw_writel(BCH_ctrl_reg, reg + 0xf4);
}


void GRE_2D_SetDitheringMatrix(const uint32_t *Matrix) {

	void __iomem *reg = GRE_2D_GetAddress();
	uint32_t value = 0x00u;

	value = (__raw_readl(reg + 0x100) & ~(0x1F1F1F1Fu));
	value |= (Matrix[0] | (Matrix[1] << 8u) | (Matrix[2] << 16u) | (Matrix[3] << 24u));
	__raw_writel(value, reg + 0x100);

	value = (__raw_readl(reg + 0x104) & ~(0x1F1F1F1Fu));
	value |= (Matrix[4] | (Matrix[5] << 8u) | (Matrix[6] << 16u) | (Matrix[7] << 24u));
	__raw_writel(value, reg + 0x104);

	value = (__raw_readl(reg + 0x108) & ~(0x1F1F1F1Fu));
	value |= (Matrix[8] | (Matrix[9] << 8u) | (Matrix[10] << 16u) | (Matrix[11] << 24u));
	__raw_writel(value, reg + 0x108);

	value = (__raw_readl(reg + 0x10c) & ~(0x1F1F1F1Fu));
	value |= (Matrix[12] | (Matrix[13] << 8u) | (Matrix[14] << 16u) | (Matrix[15] << 24u));
	__raw_writel(value, reg + 0x10c);
}


void GRE_2D_Enable(
G2D_EN grp_enalbe, unsigned char int_en) {

	void __iomem *reg = GRE_2D_GetAddress();
	uint32_t value = 0x00u;

	value = (__raw_readl(reg + 0x110) & ~(HwGE_GE_CTRL_EN|HwGE_GE_INT_EN));
	value |= (((uint32_t)int_en << 16u) | (uint32_t)grp_enalbe);
	__raw_writel(value, reg + 0x110);
}


G2D_INT_TYPE GRE_2D_IntCtrl(
unsigned char wr, G2D_INT_TYPE flag,
unsigned char int_irq, unsigned char int_flg) {

	void __iomem *reg = GRE_2D_GetAddress();

	uint32_t value = 0u;
	uint32_t temp_irq = 0u;
	uint32_t temp_flg = 0u;
	uint32_t ireq_irq = (uint32_t) HwGE_GE_IREQ_IRQ;
	uint32_t ireq_flg = (uint32_t) HwGE_GE_IREQ_FLG;

	uint8_t res    = (uint8_t) G2D_INT_NONE;
	uint8_t l_flag = (uint8_t) flag;
	uint8_t r_irq  = (uint8_t) G2D_INT_R_IRQ;
	uint8_t r_flg  = (uint8_t) G2D_INT_R_FLG;

	if (0u != int_irq) {
		temp_irq = ireq_irq;
	}

	if (0u != int_flg) {
		temp_flg = ireq_flg;
	}

	if (0u != wr) {
		if ((l_flag & r_irq) == r_irq) {
			value = (__raw_readl(reg + 0x114) & ~(HwGE_GE_IREQ_IRQ));
			value |= temp_irq;
			__raw_writel(value, reg + 0x114);
		}

		if ((l_flag & r_flg) == r_flg) {
			value = (__raw_readl(reg + 0x114) & ~(HwGE_GE_IREQ_FLG));
			value |= temp_flg;
			__raw_writel(value, reg + 0x114);
		}
	} else {
		value = __raw_readl(reg + 0x114);
		if ((value & ireq_irq) == ireq_irq) {
			res |= r_irq;
			/* prevent KCS warning */
		}

		if ((value & ireq_flg) == ireq_flg) {
			// pHwOVERLAYMIXER->OM_IREQ
			res |= r_flg;
		}
	}

	return (G2D_INT_TYPE)res;
}


void __iomem *GRE_2D_GetAddress(void) {

	void __iomem *res = NULL;

	if (NULL == pGre2D_reg) {
		(void)pr_err("[ERR][G2D] %s\n", __func__);
	} else {
		res = pGre2D_reg;
	}

	return res;
}


int32_t gre2d_init(void) {

	struct device_node *pGre2D_np;

	pGre2D_np = of_find_compatible_node(NULL, NULL, "telechips,graphic.2d");
	if (NULL == pGre2D_np) {
		(void) pr_info("[INF][G2D] vioc-g2d: disabled\n");
		/* prevent KCS warning */
	}

	pGre2D_reg = of_iomap(pGre2D_np, 0);
	if (NULL != pGre2D_reg) {
		(void) pr_info("[INF][G2D] vioc-g2d: 0x%p\n", pGre2D_reg);
		/* prevent KCS warning */
	}

	return 0;
}

EXPORT_SYMBOL(gre2d_init);

