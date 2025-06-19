// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-mediabus.h>
#include <linux/of_graph.h>

#include "tcc-mipi-csi2-csis-reg.h"
#include "../../tcc-mipi-csi2.h"
#include "../../tcc-mipi-csi2-helper.h"
#include "../../dphy_s/v1.3/tcc-mipi-csi2-dphys.h"
#ifdef CONFIG_ARCH_TCC750X
#include "../../750x/tcc-mipi-csi2-cfg.h"
#include "../../750x/tcc-mipi-csi2-ckc.h"
#endif

struct tcc_mipi_csi2_intr_src {
	const uint32_t mask;
	const char * const desc;
};

struct R_CSI2 {
	u16 VERSION;
	u16 N_LANES;
	u16 CTRL_RESETN;
	u16 INTERRUPT;
	u16 DATA_IDS_1;
	u16 DATA_IDS_2;
	u16 DATA_IDS_VC_1;
	u16 DATA_IDS_VC_2;
	u16 IPI_MODE;
	u16 IPI_VCID;
	u16 IPI_DATA_TYPE;
	u16 IPI_MEM_FLUSH;
	u16 IPI_HSA_TIME;
	u16 IPI_HBP_TIME;
	u16 IPI_HSD_TIME;
	u16 IPI_HLINE_TIME;
	u16 IPI_SOFTRSTN;
	u16 IPI_ADV_FEATURES;
	u16 IPI_VSA_LINES;
	u16 IPI_VBP_LINES;
	u16 IPI_VFP_LINES;
	u16 IPI_VACTIVE_LINES;
	u16 VC_EXTENSION;
	u16 INT_PHY_FATAL;
	u16 MASK_INT_PHY_FATAL;
	u16 FORCE_INT_PHY_FATAL;
	u16 INT_PKT_FATAL;
	u16 MASK_INT_PKT_FATAL;
	u16 FORCE_INT_PKT_FATAL;
	u16 INT_FRAME_FATAL;
	u16 MASK_INT_FRAME_FATAL;
	u16 FORCE_INT_FRAME_FATAL;
	u16 INT_PHY;
	u16 MASK_INT_PHY;
	u16 FORCE_INT_PHY;
	u16 INT_PKT;
	u16 MASK_INT_PKT;
	u16 FORCE_INT_PKT;
	u16 INT_LINE;
	u16 MASK_INT_LINE;
	u16 FORCE_INT_LINE;
	u16 INT_IPI;
	u16 MASK_INT_IPI;
	u16 FORCE_INT_IPI;
	u16 ST_BNDRY_FRAME_FATAL;
	u16 MSK_BNDRY_FRAME_FATAL;
	u16 FORCE_BNDRY_FRAME_FATAL;
	u16 ST_SEQ_FRAME_FATAL;
	u16 MSK_SEQ_FRAME_FATAL;
	u16 FORCE_SEQ_FRAME_FATAL;
	u16 ST_CRC_FRAME_FATAL;
	u16 MSK_CRC_FRAME_FATAL;
	u16 FORCE_CRC_FRAME_FATAL;
	u16 ST_PLD_CRC_FATAL;
	u16 MSK_PLD_CRC_FATAL;
	u16 FORCE_PLD_CRC_FATAL;
	u16 ST_DATA_ID;
	u16 MSK_DATA_ID;
	u16 FORCE_DATA_ID;
	u16 ST_ECC_CORRECT;
	u16 MSK_ECC_CORRECT;
	u16 FORCE_ECC_CORRECT;
};

/* Interrupt Masks */
struct interrupt_type {
	u32 PHY_FATAL;
	u32 PKT_FATAL;
	u32 FRAME_FATAL;
	u32 PHY;
	u32 PKT;
	u32 LINE;
	u32 IPI;
	u32 BNDRY_FRAME_FATAL;
	u32 SEQ_FRAME_FATAL;
	u32 CRC_FRAME_FATAL;
	u32 PLD_CRC_FATAL;
	u32 DATA_ID;
	u32 ECC_CORRECTED;
};

struct tcc_mipi_csi2_prop {
	const char * const name;
	uint32_t *out;
	bool is_def;
	uint32_t def;
};

uint32_t code_to_csi_dt(uint32_t mbus_code)
{
	uint32_t dt = 0U;

	switch (mbus_code) {
	case MEDIA_BUS_FMT_UYVY8_2X8:
	case MEDIA_BUS_FMT_VYUY8_2X8:
	case MEDIA_BUS_FMT_YUYV8_2X8:
	case MEDIA_BUS_FMT_YVYU8_2X8:
	case MEDIA_BUS_FMT_UYVY8_1X16:
	case MEDIA_BUS_FMT_VYUY8_1X16:
	case MEDIA_BUS_FMT_YUYV8_1X16:
	case MEDIA_BUS_FMT_YVYU8_1X16:
		dt = CSI_DT_YUV422_8BIT;
		break;
	case MEDIA_BUS_FMT_RGB444_1X12:
	case MEDIA_BUS_FMT_RGB444_2X8_PADHI_BE:
	case MEDIA_BUS_FMT_RGB444_2X8_PADHI_LE:
		dt = CSI_DT_RGB444;
		break;
	case MEDIA_BUS_FMT_RGB555_2X8_PADHI_BE:
	case MEDIA_BUS_FMT_RGB555_2X8_PADHI_LE:
		dt = CSI_DT_RGB555;
		break;
	case MEDIA_BUS_FMT_RGB565_1X16:
	case MEDIA_BUS_FMT_BGR565_2X8_BE:
	case MEDIA_BUS_FMT_BGR565_2X8_LE:
	case MEDIA_BUS_FMT_RGB565_2X8_BE:
	case MEDIA_BUS_FMT_RGB565_2X8_LE:
		dt = CSI_DT_RGB565;
		break;
	case MEDIA_BUS_FMT_RGB666_1X18:
	case MEDIA_BUS_FMT_RGB666_1X24_CPADHI:
	case MEDIA_BUS_FMT_RGB666_1X7X3_SPWG:
		dt = CSI_DT_RGB666;
		break;
	case MEDIA_BUS_FMT_RGB888_2X12_BE:
	case MEDIA_BUS_FMT_RGB888_2X12_LE:
	case MEDIA_BUS_FMT_RGB888_1X7X4_SPWG:
	case MEDIA_BUS_FMT_RGB888_1X7X4_JEIDA:
	case MEDIA_BUS_FMT_RGB888_1X32_PADHI:
	case MEDIA_BUS_FMT_RBG888_1X24:
	case MEDIA_BUS_FMT_BGR888_1X24:
	case MEDIA_BUS_FMT_GBR888_1X24:
	case MEDIA_BUS_FMT_RGB888_1X24:
		dt = CSI_DT_RGB888;
		break;
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
	case MEDIA_BUS_FMT_Y8_1X8:
		dt = CSI_DT_RAW8;
		break;
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
	case MEDIA_BUS_FMT_Y10_1X10:
		dt = CSI_DT_RAW10;
		break;
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
	case MEDIA_BUS_FMT_Y12_1X12:
		dt = CSI_DT_RAW12;
		break;
	case MEDIA_BUS_FMT_SBGGR14_1X14:
	case MEDIA_BUS_FMT_SGBRG14_1X14:
	case MEDIA_BUS_FMT_SGRBG14_1X14:
	case MEDIA_BUS_FMT_SRGGB14_1X14:
		dt = CSI_DT_RAW14;
		break;
	case MEDIA_BUS_FMT_SBGGR16_1X16:
	case MEDIA_BUS_FMT_SGBRG16_1X16:
	case MEDIA_BUS_FMT_SGRBG16_1X16:
	case MEDIA_BUS_FMT_SRGGB16_1X16:
		dt = CSI_DT_RAW16;
		break;
	default:
		dt = CSI_DT_YUV422_8BIT;
		break;
	}

	return dt;
}

static struct R_CSI2 reg = {
	.VERSION = 0x00,
	.N_LANES = 0x04,
	.CTRL_RESETN = 0x08,
	.INTERRUPT = 0x0C,
	.DATA_IDS_1 = 0x10,
	.DATA_IDS_2 = 0x14,
	.IPI_MODE = 0x80,
	.IPI_VCID = 0x84,
	.IPI_DATA_TYPE = 0x88,
	.IPI_MEM_FLUSH = 0x8C,
	.IPI_HSA_TIME = 0x90,
	.IPI_HBP_TIME = 0x94,
	.IPI_HSD_TIME = 0x98,
	.IPI_HLINE_TIME = 0x9C,
	.IPI_SOFTRSTN = 0xA0,
	.IPI_ADV_FEATURES = 0xAC,
	.IPI_VSA_LINES = 0xB0,
	.IPI_VBP_LINES = 0xB4,
	.IPI_VFP_LINES = 0xB8,
	.IPI_VACTIVE_LINES = 0xBC,
	.INT_PHY_FATAL = 0xe0,
	.MASK_INT_PHY_FATAL = 0xe4,
	.FORCE_INT_PHY_FATAL = 0xe8,
	.INT_PKT_FATAL = 0xf0,
	.MASK_INT_PKT_FATAL = 0xf4,
	.FORCE_INT_PKT_FATAL = 0xf8,
	.INT_PHY = 0x110,
	.MASK_INT_PHY = 0x114,
	.FORCE_INT_PHY = 0x118,
	.INT_LINE = 0x130,
	.MASK_INT_LINE = 0x134,
	.FORCE_INT_LINE = 0x138,
	.INT_IPI = 0x140,
	.MASK_INT_IPI = 0x144,
	.FORCE_INT_IPI = 0x148,
	/*
	 * HW registers that were added
	 * to version 1.40
	 */
	.ST_BNDRY_FRAME_FATAL = 0x280,
	.MSK_BNDRY_FRAME_FATAL = 0x284,
	.FORCE_BNDRY_FRAME_FATAL = 0x288,
	.ST_SEQ_FRAME_FATAL = 0x290,
	.MSK_SEQ_FRAME_FATAL	= 0x294,
	.FORCE_SEQ_FRAME_FATAL = 0x298,
	.ST_CRC_FRAME_FATAL = 0x2a0,
	.MSK_CRC_FRAME_FATAL	= 0x2a4,
	.FORCE_CRC_FRAME_FATAL = 0x2a8,
	.ST_PLD_CRC_FATAL = 0x2b0,
	.MSK_PLD_CRC_FATAL = 0x2b4,
	.FORCE_PLD_CRC_FATAL = 0x2b8,
	.ST_DATA_ID = 0x2c0,
	.MSK_DATA_ID = 0x2c4,
	.FORCE_DATA_ID = 0x2c8,
	.ST_ECC_CORRECT = 0x2d0,
	.MSK_ECC_CORRECT = 0x2d4,
	.FORCE_ECC_CORRECT = 0x2d8,
	.DATA_IDS_VC_1 = 0x0,
	.DATA_IDS_VC_2 = 0x0,
	.VC_EXTENSION = 0x0,
};

struct interrupt_type csi_int = {
	.PHY_FATAL = BIT(0),
	.PKT_FATAL = BIT(1),
	.PHY = BIT(16),
	/* interrupts map were changed */
	.LINE = BIT(17),
	.IPI = BIT(18),
	.BNDRY_FRAME_FATAL = BIT(2),
	.SEQ_FRAME_FATAL	= BIT(3),
	.CRC_FRAME_FATAL = BIT(4),
	.PLD_CRC_FATAL = BIT(5),
	.DATA_ID = BIT(6),
	.ECC_CORRECTED = BIT(7),
};

#define tcc_mipi_csi2_print(DEV, BASE, OFFSET)                                 \
	dev_info(&DEV, "%s: 0x%x: %X\n", "##OFFSET##", OFFSET,                 \
		 tcc_mipi_csi2_readl(BASE, OFFSET))

void tcc_mipi_csi2_reset(const struct tcc_mipi_csi2_state *state)
{
	tcc_mipi_csi2_writel(0U, state->csi_base, reg.CTRL_RESETN);
	usleep_range(100, 200);
	tcc_mipi_csi2_writel(1U, state->csi_base, reg.CTRL_RESETN);
}

int tcc_mipi_csi2_mask_irq_power_off(const struct tcc_mipi_csi2_state *state)
{
	/* set only one lane (lane 0) as active (ON) */
	tcc_mipi_csi2_writel(0U, state->csi_base, reg.N_LANES);
	tcc_mipi_csi2_writel(0U, state->csi_base, reg.MASK_INT_PHY_FATAL);
	tcc_mipi_csi2_writel(0U, state->csi_base, reg.MASK_INT_PKT_FATAL);
	tcc_mipi_csi2_writel(0U, state->csi_base, reg.MASK_INT_PHY);
	tcc_mipi_csi2_writel(0U, state->csi_base, reg.MASK_INT_LINE);
	tcc_mipi_csi2_writel(0U, state->csi_base, reg.MASK_INT_IPI);

	tcc_mipi_csi2_writel(0U, state->csi_base, reg.CTRL_RESETN);

	/* only for version 1.40 */
	tcc_mipi_csi2_writel(0U, state->csi_base, reg.MSK_BNDRY_FRAME_FATAL);
	tcc_mipi_csi2_writel(0U, state->csi_base, reg.MSK_SEQ_FRAME_FATAL);
	tcc_mipi_csi2_writel(0U, state->csi_base, reg.MSK_CRC_FRAME_FATAL);
	tcc_mipi_csi2_writel(0U, state->csi_base, reg.MSK_PLD_CRC_FATAL);
	tcc_mipi_csi2_writel(0U, state->csi_base, reg.MSK_DATA_ID);
	tcc_mipi_csi2_writel(0U, state->csi_base, reg.MSK_ECC_CORRECT);

	return 0;
}

int tcc_mipi_csi2_hw_stdby(const struct tcc_mipi_csi2_state *state)
{
	tcc_mipi_csi2_reset(state);

	/* set only one lane (lane 0) as active (ON) */
	tcc_mipi_csi2_writel(0U, state->csi_base, reg.N_LANES);
	//phy_init(state->phy);

	/* common */
	tcc_mipi_csi2_writel(GENMASK(8, 0), state->csi_base, reg.MASK_INT_PHY_FATAL);
	tcc_mipi_csi2_writel(GENMASK(1, 0), state->csi_base, reg.MASK_INT_PKT_FATAL);
	tcc_mipi_csi2_writel(GENMASK(23, 0), state->csi_base, reg.MASK_INT_PHY);
	tcc_mipi_csi2_writel(GENMASK(23, 0), state->csi_base, reg.MASK_INT_LINE);
	tcc_mipi_csi2_writel(GENMASK(5, 0), state->csi_base, reg.MASK_INT_IPI);

	/* only for version 1.40 */
	tcc_mipi_csi2_writel(GENMASK(31, 0), state->csi_base, reg.MSK_BNDRY_FRAME_FATAL);
	tcc_mipi_csi2_writel(GENMASK(31, 0), state->csi_base, reg.MSK_SEQ_FRAME_FATAL);
	tcc_mipi_csi2_writel(GENMASK(31, 0), state->csi_base, reg.MSK_CRC_FRAME_FATAL);
	tcc_mipi_csi2_writel(GENMASK(31, 0), state->csi_base, reg.MSK_PLD_CRC_FATAL);
	tcc_mipi_csi2_writel(GENMASK(31, 0), state->csi_base, reg.MSK_DATA_ID);
	tcc_mipi_csi2_writel(GENMASK(31, 0), state->csi_base, reg.MSK_ECC_CORRECT);

	return 0;
}

void tcc_mipi_csi2_fill_timings(struct tcc_mipi_csi2_state *state)
{
	/* TODO: */
	state->hw.ipi_auto_flush = 1;
	state->hw.ipi_mode = CAMERA_TIMING;
	state->hw.ipi_cut_through = CTACTIVE;
	state->hw.ipi_adv_features = IPI_SYNC_EVENT_MODE(1) |
				     LINE_EVENT_SELECTION(EVSELPROG) |
				     EN_VIDEO;
	state->hw.hsa = 0x50;
	state->hw.hbp = 0x50;
	state->hw.hsd = 0x50;
}

void tcc_mipi_csi2_start(const struct tcc_mipi_csi2_state *state)
{
	uint32_t ch = 0U, val = 0U;
	uint32_t color_mode;
	uint32_t ipi_offset[4];

	ipi_offset[0] = TCC_DWC_MIPI_CSI2_IPI_MODE - TCC_DWC_MIPI_CSI2_IPI_MODE;
	ipi_offset[1] = TCC_DWC_MIPI_CSI2_IPI2_MODE - TCC_DWC_MIPI_CSI2_IPI_MODE;
	ipi_offset[2] = TCC_DWC_MIPI_CSI2_IPI3_MODE - TCC_DWC_MIPI_CSI2_IPI_MODE;
	ipi_offset[3] = TCC_DWC_MIPI_CSI2_IPI4_MODE - TCC_DWC_MIPI_CSI2_IPI_MODE;

	/* the number of data lanes */
	val = state->data_lane_num - 1;
	tcc_mipi_csi2_writel(val, state->csi_base, reg.N_LANES);

	for (ch = 0; ch < 4; ch++) {
		/* IPI Related Configuration */
		logi(&state->pdev->dev, "adv features: %x\n",
		     state->hw.ipi_adv_features);
		switch (ch) {
		case 0:
			tcc_mipi_csi2_writel(state->hw.ipi_adv_features,
					     state->csi_base,
					     reg.IPI_ADV_FEATURES);
			break;
		case 1:
			tcc_mipi_csi2_writel(state->hw.ipi_adv_features,
					     state->csi_base,
					     TCC_DWC_MIPI_CSI2_IPI2_ADV_FEATURES);
			break;
		case 2:
			tcc_mipi_csi2_writel(state->hw.ipi_adv_features,
					     state->csi_base,
					     TCC_DWC_MIPI_CSI2_IPI3_ADV_FEATURES);
			break;
		case 3:
			tcc_mipi_csi2_writel(state->hw.ipi_adv_features,
					     state->csi_base,
					     TCC_DWC_MIPI_CSI2_IPI4_ADV_FEATURES);
			break;
		}

		//tcc_mipi_csi2_write(state, reg.IPI_SOFTRSTN, 0x1);

		switch (state->isp_info[0U].data_format) {
		case CSI_DT_RGB444:
		case CSI_DT_RGB555:
		case CSI_DT_RGB565:
		case CSI_DT_RGB666:
		case CSI_DT_RGB888:
		case CSI_DT_YUV422_8BIT:
		case CSI_DT_YUV422_10BIT:
		case CSI_DT_RAW20:
		case CSI_DT_RAW24:
			color_mode = COLOR48;
			break;
		default:
			color_mode = COLOR16;
			break;
		}

		logi(&state->pdev->dev,
		     "video mode transmission type: %s timming\n",
		     state->hw.ipi_mode ? "controller" : "camera");
		logi(&state->pdev->dev, "Color Mode: %s\n",
		     color_mode ? "16 bits" : "48 bits");
		logi(&state->pdev->dev, "Cut Through Mode: %s\n",
		     state->hw.ipi_cut_through ? "enable" : "disable");
		tcc_mipi_csi2_writel(((state->hw.ipi_mode << TCC_MIPI_CSI2_IPI_MODE_TIMING_SHIFT) |
				      (color_mode << TCC_MIPI_CSI2_IPI_MODE_COLOR_COM_SHIFT) |
				      (state->hw.ipi_cut_through << TCC_MIPI_CSI2_IPI_MODE_CUT_THROUGH_SHIFT) |
				      (1U << TCC_MIPI_CSI2_IPI_MODE_ENABLE_SHIFT)),
				      state->csi_base,
				      reg.IPI_MODE + ipi_offset[ch]);
		tcc_mipi_csi2_writel((state->isp_info[ch].virtual_channel << TCC_MIPI_CSI2_IPI_VCID_VC_SHIFT),
				     state->csi_base,
				     reg.IPI_VCID + ipi_offset[ch]);
		tcc_mipi_csi2_writel((state->isp_info[0U].data_format << TCC_MIPI_CSI2_IPI_DATA_TYPE_DT_SHIFT),
				     state->csi_base,
				     reg.IPI_DATA_TYPE + ipi_offset[ch]);

		logi(&state->pdev->dev, "Auto-flush: %d\n",
		     state->hw.ipi_auto_flush);
		tcc_mipi_csi2_writel((state->hw.ipi_auto_flush << TCC_MIPI_CSI2_IPI_MEM_FLUSH_AUTO_SHIFT),
				     state->csi_base,
				     reg.IPI_MEM_FLUSH + ipi_offset[ch]);

		//tcc_mipi_csi2_write(state, reg.IPI_SOFTRSTN, 1);
#if 0
		if (state->hw.ipi_mode == AUTO_TIMING)
			phy_power_on(state->phy);
#endif

		logi(&state->pdev->dev,
		     "hsa: %x hbp: %x hsd: %x\n",
		     state->hw.hsa, state->hw.hbp, state->hw.hsd);
		tcc_mipi_csi2_writel(state->hw.hsa, state->csi_base,
				     reg.IPI_HSA_TIME + ipi_offset[ch]);
		tcc_mipi_csi2_writel(state->hw.hbp, state->csi_base,
				     reg.IPI_HBP_TIME + ipi_offset[ch]);
		tcc_mipi_csi2_writel(state->hw.hsd, state->csi_base,
				     reg.IPI_HSD_TIME + ipi_offset[ch]);
#if 0
	phy_power_on(state->phy);
#endif
	}
}

void tcc_mipi_csi2_irq_handler(const struct tcc_mipi_csi2_state *state)
{
	u32 global_int_status, i_sts;
	//unsigned long flags;

	//spin_lock_irqsave(&csi_dev->slock, flags);
	global_int_status = tcc_mipi_csi2_readl(state->csi_base, reg.INTERRUPT);

	if (global_int_status & csi_int.PHY_FATAL) {
		i_sts = tcc_mipi_csi2_readl(state->csi_base, reg.INT_PHY_FATAL);
		loge_ratelimited(&(state->pdev->dev),
				 "int %08X: PHY FATAL: %08X\n",
				  reg.INT_PHY_FATAL, i_sts);
	}

	if (global_int_status & csi_int.PKT_FATAL) {
		i_sts = tcc_mipi_csi2_readl(state->csi_base, reg.INT_PKT_FATAL);
		loge_ratelimited(&(state->pdev->dev),
				 "int %08X: PKT FATAL: %08X\n",
				 reg.INT_PKT_FATAL, i_sts);
	}

	if (global_int_status & csi_int.PHY) {
		i_sts = tcc_mipi_csi2_readl(state->csi_base, reg.INT_PHY);
		loge_ratelimited(&(state->pdev->dev), "int %08X: PHY: %08X\n",
				 reg.INT_PHY, i_sts);
	}

	if (global_int_status & csi_int.LINE) {
		i_sts = tcc_mipi_csi2_readl(state->csi_base, reg.INT_LINE);
		loge_ratelimited(&(state->pdev->dev), "int %08X: LINE: %08X\n",
				 reg.INT_LINE, i_sts);
	}

	if (global_int_status & csi_int.IPI) {
		i_sts = tcc_mipi_csi2_readl(state->csi_base, reg.INT_IPI);
		loge_ratelimited(&(state->pdev->dev), "int %08X: IPI: %08X\n",
				 reg.INT_IPI, i_sts);
	}

	if (global_int_status & csi_int.BNDRY_FRAME_FATAL) {
		i_sts = tcc_mipi_csi2_readl(state->csi_base,
					    reg.ST_BNDRY_FRAME_FATAL);
		loge_ratelimited(&(state->pdev->dev),
				 "int %08X: ST_BNDRY_FRAME_FATAL: %08X\n",
				 reg.ST_BNDRY_FRAME_FATAL, i_sts);
	}

	if (global_int_status & csi_int.SEQ_FRAME_FATAL) {
		i_sts = tcc_mipi_csi2_readl(state->csi_base,
					    reg.ST_SEQ_FRAME_FATAL);
		loge_ratelimited(&(state->pdev->dev),
				 "int %08X: ST_SEQ_FRAME_FATAL: %08X\n",
				 reg.ST_SEQ_FRAME_FATAL, i_sts);
	}

	if (global_int_status & csi_int.CRC_FRAME_FATAL) {
		i_sts = tcc_mipi_csi2_readl(state->csi_base,
					    reg.ST_CRC_FRAME_FATAL);
		loge_ratelimited(&(state->pdev->dev),
				 "int %08X: ST_CRC_FRAME_FATAL: %08X\n",
				 reg.ST_CRC_FRAME_FATAL, i_sts);
	}

	if (global_int_status & csi_int.PLD_CRC_FATAL) {
		i_sts = tcc_mipi_csi2_readl(state->csi_base,
					    reg.ST_PLD_CRC_FATAL);
		loge_ratelimited(&(state->pdev->dev),
				 "int %08X: ST_PLD_CRC_FATAL: %08X\n",
				 reg.ST_PLD_CRC_FATAL, i_sts);
	}

	if (global_int_status & csi_int.DATA_ID) {
		i_sts = tcc_mipi_csi2_readl(state->csi_base, reg.ST_DATA_ID);
		loge_ratelimited(&(state->pdev->dev),
				 "int %08X: ST_DATA_ID: %08X\n", reg.ST_DATA_ID,
				 i_sts);
	}

	if (global_int_status & csi_int.ECC_CORRECTED) {
		i_sts = tcc_mipi_csi2_readl(state->csi_base,
					    reg.ST_ECC_CORRECT);
		loge_ratelimited(&(state->pdev->dev),
				 "int %08X: ST_ECC_CORRECT: %08X\n",
				 reg.ST_ECC_CORRECT, i_sts);
	}

	//spin_unlock_irqrestore(&csi_dev->slock, flags);
}

void tcc_mipi_csi2_dump(const struct tcc_mipi_csi2_state *state)
{
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.VERSION);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.N_LANES);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.CTRL_RESETN);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.INTERRUPT);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.DATA_IDS_1);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.DATA_IDS_2);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.IPI_MODE);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.IPI_VCID);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.IPI_DATA_TYPE);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.IPI_MEM_FLUSH);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.IPI_HSA_TIME);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.IPI_HBP_TIME);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.IPI_HSD_TIME);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.IPI_HLINE_TIME);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.IPI_SOFTRSTN);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.IPI_ADV_FEATURES);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.IPI_VSA_LINES);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.IPI_VBP_LINES);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.IPI_VFP_LINES);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.IPI_VACTIVE_LINES);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.IPI_DATA_TYPE);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.VERSION);
	tcc_mipi_csi2_print(state->pdev->dev, state->csi_base, reg.IPI_ADV_FEATURES);
}

static void tcc_mipi_csi2_set_interface(const struct tcc_mipi_csi2_state *state,
					uint32_t onOff)
{
	/*
	 * Synopsys MIPI CSI-2
	 **************************************************
	 * 1. Start up
	 *   - release preset(Peripheral reset)
	 *   - Config MIPI CSI-2
	 *   - Apply PHY Reset
	 *   - Init MIPI CSI-2
	 *   - Start the HS Reception Mode
	 * 2. Initialize
	 * 3. Configure IPI
	 * 4. Start the High-Speed Reception Mode
	 * 5. Detect Errors
	 * 6. Stop the High-Speed Reception Mode

	 * 1. Start up
	 * - Release MIPI CSI-2 Reset
	 * - Config MIPI CSI-2
	 */

	if (onOff == 1U) {
		tcc_mipi_csi2_cfg_reset(state, 0U);
		tcc_mipi_csi2_hw_stdby(state);
		tcc_mipi_csi2_start(state);
		tcc_mipi_csi2_dphys_set_dphys(state);
	} else {
		//phy_power_off(dev->phy);
		tcc_mipi_csi2_mask_irq_power_off(state);
		// S/W reset CSI2
		tcc_mipi_csi2_cfg_reset(state, 1U);
	}
}

void tcc_mipi_csi2_enable(const struct tcc_mipi_csi2_state *state,
			  uint32_t enable)
{
	tcc_mipi_csi2_set_interface(state, enable);
}

irqreturn_t tcc_mipi_csi2_isr(int irq, void *client_data)
{
	const struct tcc_mipi_csi2_state *state =
		(struct tcc_mipi_csi2_state *)client_data;
	irqreturn_t ret = IRQ_NONE;

	logd(&(state->pdev->dev), "irq number(%d)\n", irq);

	tcc_mipi_csi2_irq_handler(state);

	ret = IRQ_HANDLED;

	return ret;
}

irqreturn_t tcc_mipi_csi2_gdb_isr(int irq, void *client_data)
{
	const struct tcc_mipi_csi2_state *state =
		(struct tcc_mipi_csi2_state *)client_data;
	irqreturn_t ret = IRQ_NONE;

	logd(&(state->pdev->dev), "irq number(%d)\n", irq);

	ret = IRQ_HANDLED;

	return ret;
}

static int tcc_mipi_csi2_dt_read_u32(const struct tcc_mipi_csi2_state *state,
				     const struct device_node *nd,
				     const struct tcc_mipi_csi2_prop cfgs[])
{
	uint32_t idx = 0U;
	int ret = 0;

	for (idx = 0; cfgs[idx].name != NULL; idx++) {
		ret = of_property_read_u32(nd,
				cfgs[idx].name,
				cfgs[idx].out);
		if (ret < 0) {
			if (cfgs[idx].is_def) {
				/* default value */
				*(cfgs[idx].out) = cfgs[idx].def;
				ret = 0;
			} else {
				/* error */
				loge(&(state->pdev->dev),
						"invalid %s property(%d)\n",
						cfgs[idx].name,
						ret);
				break;
			}
		}
	}

	return ret;
}

int tcc_mipi_csi2_dt_res(struct tcc_mipi_csi2_state *state)
{
	const struct resource *res = NULL;
	int ret = 0;

	/* Get MIPI CSI-2 base address */
	res = platform_get_resource_byname(state->pdev, IORESOURCE_MEM, "csi");
	state->csi_base = devm_ioremap_resource(&state->pdev->dev, res);
	if (IS_ERR((const void *)state->csi_base)) {
		/* error */
		ret = (int)PTR_ERR((const void *)state->csi_base);
		loge(&(state->pdev->dev),
				"Invalid MIPI CSI2 base addr(%d)\n",
				ret);
	}

	/* Get CFG base address */
	if (ret == 0) {
		res = platform_get_resource_byname(state->pdev, IORESOURCE_MEM,
						   "cfg");
		state->cfg_base = ioremap(res->start, resource_size(res));
		if (IS_ERR((const void *)state->cfg_base)) {
			/* error */
			ret = (int)PTR_ERR((const void *)state->cfg_base);
			loge(&(state->pdev->dev),
					"Invalid CFG base addr(%d)\n",
					ret);
		}
	}

	/* get interrupt number */
	if (ret == 0) {
		ret = platform_get_irq_byname(state->pdev, "csi");
		if (ret <= 0) {
			/* error */
			loge(&(state->pdev->dev),
					"Invalid IRQ(%d)\n",
					ret);
		} else {
			/* okay */
			state->irq = (uint32_t)ret;
			ret = 0;
		}
	}

	return ret;
}

static int tcc_mipi_csi2_dt_path_cfg(struct tcc_mipi_csi2_state *state)
{
	const struct device_node *nd = NULL;
	int ret = 0;

	const struct tcc_mipi_csi2_prop path_cfgs[] = {
		{
			.name = "isp0-bypass",
			.out = &state->isp_bypass[0],
			.is_def = (bool)false,
		},
		{
			.name = "isp1-bypass",
			.out = &state->isp_bypass[1],
			.is_def = (bool)false,
		},
		{
			.name = "isp2-bypass",
			.out = &state->isp_bypass[2],
			.is_def = (bool)false,
		},
		{
			.name = "isp3-bypass",
			.out = &state->isp_bypass[3],
			.is_def = (bool)false,
		},
		{
		},
	};

	nd = of_get_parent(state->pdev->dev.of_node);
	if (IS_ERR((const void *)nd)) {
		/* error */
		ret = (int)PTR_ERR((const void *)nd);
		loge(&(state->pdev->dev),
		     "invalid mipi_wrap node(%d)\n",
		     ret);
	}

	if (ret == 0) {
		ret = tcc_mipi_csi2_dt_read_u32(state, nd, path_cfgs);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
				"tcc_mipi_csi2_dt_read_u32 returned %d\n",
				ret);
		}
	}

	return ret;
}

static int tcc_mipi_csi2_dt_csi_in(struct tcc_mipi_csi2_state *state)
{
	struct device_node *ep = NULL;
	struct v4l2_fwnode_endpoint epdata = {0, };
	int ret = 0;

	const struct tcc_mipi_csi2_prop cmm_cfgs[] = {
		{
			.name = "num-channel",
			.out = &state->input_ch_num,
			.is_def = (bool)false,
		},
		/* CSIS common control parameter */
		{
			.name = "deskew-level",
			.out = &state->deskew_level,
			.is_def = (bool)true,
			.def = 2U,
		},
		{
			.name = "deskew-enable",
			.out = &state->deskew_enable,
			.is_def = (bool)true,
			.def = 1U,
		},
		{
			.name = "interleave-mode",
			.out = &state->interleave_mode,
			.is_def = (bool)false,
		},
		{
			.name = "update-shadow-ctrl",
			.out = &state->update_shadow_ctrl,
			.is_def = (bool)true,
			.def = 0U,
		},
		{
		},
	};

	ep = of_graph_get_endpoint_by_regs(state->pdev->dev.of_node, 0, 0);
	if (IS_ERR_OR_NULL(ep)) {
		/* error */
		loge(&(state->pdev->dev), "invalid input endpoint\n");
		ret = -ENXIO;
	}

	if (ret == 0) {
		ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(ep),
						 &epdata);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
					"cannot parse endpoint\n");
		} else {
			/* okay */
			state->data_lane_num =
				epdata.bus.mipi_csi2.num_data_lanes;
		}
	}

	if (ret == 0) {
		ret = tcc_mipi_csi2_dt_read_u32(state, ep, cmm_cfgs);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
			     "tcc_mipi_csi2_dt_read_u32 returned %d\n",
			     ret);
		}
	}

	of_node_put(ep);

	return ret;
}

static int tcc_mipi_csi2_dt_csi_out(struct tcc_mipi_csi2_state *state,
				    const uint32_t ch)
{
	struct device_node *ep = NULL;
	int ret = 0;

	if (ch >= MAX_VC) {
		/* error */
		loge(&(state->pdev->dev), "invalid ch(%d)\n", ch);
		ret = -EINVAL;
	}


	if (ret == 0) {
		ep = of_graph_get_endpoint_by_regs(state->pdev->dev.of_node,
						   (int)ch + 1,
						   0);
		if (IS_ERR_OR_NULL(ep)) {
			/* error */
			loge(&(state->pdev->dev),
					"invalid output endpoint(%d)\n",
					(int)ch + 1);
			ret = -ENXIO;
		}
	}

	if (ret == 0) {
		const struct tcc_mipi_csi2_prop isp_cfgs[] = {
			{
				.name = "channel",
				.out = &state->isp_info[ch].virtual_channel,
				.is_def = (bool)false,
			},
			{
				.name = "pixel-mode",
				.out = &state->isp_info[ch].pixel_mode,
				.is_def = (bool)false,
			},
			{
			},
		};

		ret = tcc_mipi_csi2_dt_read_u32(state, ep, isp_cfgs);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
				"tcc_mipi_csi2_dt_read_u32 returned %d\n",
				ret);
		}
	}

	of_node_put(ep);

	return ret;
}

static int tcc_mipi_csi2_dt_csi_cfg(struct tcc_mipi_csi2_state *state)
{
	uint32_t idx = 0U;
	int ret = 0;

	/* parse input port csi cfg */
	ret = tcc_mipi_csi2_dt_csi_in(state);
	if (ret < 0) {
		/* error */
		loge(&(state->pdev->dev),
				"tcc_mipi_csi2_dt_csi_in returned %d\n",
				ret);
	}

	/* parse output port csi cfg */
	if (ret == 0) {
		for (idx = 0U; idx < MAX_VC; idx++) {
			if (ret < 0) {
				/* error */
				loge(&(state->pdev->dev),
						"break EP parsing\n");
				break;
			}

			ret = tcc_mipi_csi2_dt_csi_out(state, idx);
			if (ret < 0) {
				/* error */
				loge(&(state->pdev->dev),
				     "tcc_mipi_csi2_dt_csi_out returned %d\n",
				     ret);
				continue;
			}
		}
	}

	return ret;
}

static int tcc_mipi_csi2_dt_dphy_cfg(struct tcc_mipi_csi2_state *state)
{
	const struct device_node *ep = NULL;
	int ret = 0;

	const struct tcc_mipi_csi2_prop dphy_cfgs[] = {
		/* DPHY common control parameter */
		{
			.name = "hs-settle",
			.out = &state->hssettle,
			.is_def = (bool)false,
		},
		{
			.name = "s-clksettlectl",
			.out = &state->s_clksettlectl,
			.is_def = (bool)true,
			/* depends on the DPHY sepecification version */
			.def = 0U,
		},
		{
			.name = "s-dpdn-swap-clk",
			.out = &state->s_dpdn_swap_clk,
			.is_def = (bool)true,
			.def = 0U,
		},
		{
			.name = "s-dpdn-swap-dat",
			.out = &state->s_dpdn_swap_dat,
			.is_def = (bool)true,
			.def = 0U,
		},
		{
		},
	};

	ep = of_graph_get_next_endpoint(state->pdev->dev.of_node, NULL);
	if (IS_ERR_OR_NULL(ep)) {
		/* error */
		ret = -ENXIO;
		loge(&(state->pdev->dev), "invalid input endpoint(%d)", ret);
	}

	if (ret == 0) {
		ret = tcc_mipi_csi2_dt_read_u32(state, ep, dphy_cfgs);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
				"tcc_mipi_csi2_dt_read_u32 returned %d\n",
				ret);
		}
	}

	return ret;
}

int tcc_mipi_csi2_parse_dt(struct tcc_mipi_csi2_state *state)
{
	int ret = 0;

	ret = of_alias_get_id(state->pdev->dev.of_node, "mipi-csi2-");
	if (ret < 0) {
		/* error */
		loge(&(state->pdev->dev), "of_alias_get_id returned NULL\n");
	} else {
		/* okay */
		state->pdev->id = ret;
		ret = 0;
	}

	/* Parsing used resource */
	if (ret == 0) {
		ret = tcc_mipi_csi2_dt_res(state);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
			     "tcc_mipi_csi2_dt_res returned %d",
			     ret);
		}
	}

	/* Parsing MIPI_WRAP path configure */
	if (ret == 0) {
		ret = tcc_mipi_csi2_dt_path_cfg(state);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
			     "tcc_mipi_csi2_dt_path_cfg returned %d",
			     ret);
		}
	}

	/* Get input MIPI CSI2 bus info */
	if (ret == 0) {
		ret = tcc_mipi_csi2_dt_csi_cfg(state);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
				"tcc_mipi_csi2_dt_csi_cfg returned %d",
				ret);
		}
	}

	if (ret == 0) {
		ret = tcc_mipi_csi2_dt_dphy_cfg(state);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
				"tcc_mipi_csi2_dt_dphy_cfg returned %d",
				ret);
		}
	}

	if (ret == 0) {
		tcc_mipi_csi2_fill_timings(state);
	}

	return ret;
}
