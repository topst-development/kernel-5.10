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
#ifdef CONFIG_ARCH_TCC803X
#include "../../803x/tcc-mipi-csi2-cfg.h"
#include "../../803x/tcc-mipi-csi2-ckc.h"
#endif
#ifdef CONFIG_ARCH_TCC805X
#include "../../805x/tcc-mipi-csi2-cfg.h"
#include "../../805x/tcc-mipi-csi2-ckc.h"
#endif

//#define GENERIC_DATA_BUFFER_TEST

struct tcc_mipi_csi2_intr_src {
	const uint32_t mask;
	const char * const desc;
};

struct tcc_mipi_csi2_prop {
	const char * const name;
	uint32_t *out;
	bool is_def;
	uint32_t def;
};

static const struct tcc_mipi_csi2_intr_src intr_srcs0[] = {
	/* error state */
	{
		.mask = CIM_MSK_ERR_ID_MASK,
		.desc = "Unknown ID error",
	},
	{
		.mask = CIM_MSK_ERR_CRC_MASK,
		.desc = "CRC error",
	},
	{
		.mask = CIM_MSK_ERR_ECC_MASK,
		.desc = "ECC error",
	},
	{
		.mask = CIM_MSK_ERR_WRONG_CFG_MASK,
		.desc = "Wrong configuration",
	},
	{
		.mask = CIM_MSK_ERR_OVER_MASK,
		.desc = "Image FIFO overflow interrupt",
	},
	{
		.mask = CIM_MSK_ERR_LOST_FE_MASK,
		.desc = "Lost of Frame End packet",
	},
	{
		.mask = CIM_MSK_ERR_LOST_FS_MASK,
		.desc = "Lost of Frame Start packet",
	},
	{
		.mask = CIM_MSK_ERR_SOT_HS_MASK,
		.desc = "Start of transmission error",
	},
	/* normal state */
	{
		.mask = CIM_MSK_FRAMEEND_MASK,
		.desc = "FrameEnd packet is received",
	},
	{
		.mask = CIM_MSK_FRAMESTART_MASK,
		.desc = "FrameStart packet is received",
	},
	{
	},
};

static const struct tcc_mipi_csi2_intr_src intr_srcs1[] = {
	/* normal state */
	{
		.mask = CIM_MSK_LINE_END_MASK,
		.desc = "End of specific line",
	},
	{
	},
};

static const struct tcc_mipi_csi2_intr_src *intr_srcs[] = {
	intr_srcs0, /* page 0 */
	intr_srcs1, /* page 1 */
	NULL,
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
	case MEDIA_BUS_FMT_RGB565_1X16:
		dt = CSI_DT_RGB565;
		break;
	case MEDIA_BUS_FMT_RGB666_1X24_CPADHI:
		dt = CSI_DT_RGB666;
		break;
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
	default:
		dt = CSI_DT_YUV422_8BIT;
		break;
	}

	return dt;
}

static void
MIPI_CSIS_Set_ISP_CONFIG(const struct tcc_mipi_csi2_state *state, uint32_t ch)
{
	uint32_t val = 0U, pm = 0U, df = 0U, vc = 0U;
	const uint32_t offset[] = {
		ISP_CONFIG_CH0,
		ISP_CONFIG_CH1,
		ISP_CONFIG_CH2,
		ISP_CONFIG_CH3,
	};

	if (ch >= MAX_VC) {
		loge(&(state->pdev->dev), "invalid ch(%d)\n", ch);
	} else {
		val = tcc_mipi_csi2_readl(state->csi_base, offset[ch]);

		val &= ~(ICON_PIXEL_MODE_MASK |
			 ICON_PARALLEL_MASK |
			 ICON_RGB_SWAP_MASK |
			 ICON_DATAFORMAT_MASK |
			 ICON_VIRTUAL_CHANNEL_MASK);

		pm = state->isp_info[ch].pixel_mode;
		df = state->isp_info[ch].data_format;
		vc = state->isp_info[ch].virtual_channel;

		val |= ((pm << ICON_PIXEL_MODE_SHIFT) |
			/* Do not align 32bit data */
			(((uint32_t)0U) << ICON_PARALLEL_SHIFT) |
			/* MSB is R and LSB is B */
			(((uint32_t)0U) << ICON_RGB_SWAP_SHIFT) |
			(df << ICON_DATAFORMAT_SHIFT) |
			(vc << ICON_VIRTUAL_CHANNEL_SHIFT));

		tcc_mipi_csi2_writel(val, state->csi_base, offset[ch]);
	}
}

static void MIPI_CSIS_Set_ISP_RESOL(const struct tcc_mipi_csi2_state *state,
				    uint32_t ch,
				    uint32_t width,
				    uint32_t height)
{
	uint32_t val = 0U;
	const uint32_t offset[] = {
		ISP_RESOL_CH0,
		ISP_RESOL_CH1,
		ISP_RESOL_CH2,
		ISP_RESOL_CH3,
	};

	val = tcc_mipi_csi2_readl(state->csi_base, offset[ch]);

	val &= ~(IRES_VRESOL_MASK | IRES_HRESOL_MASK);

	val |= ((width << IRES_HRESOL_SHIFT) |
		(height << IRES_VRESOL_SHIFT));

	ch = ((ch >= MAX_VC) ? (0U) : (ch));

	logd(&(state->pdev->dev), "ch(%d), resol(%dx%d)\n", ch, width, height);

	tcc_mipi_csi2_writel(val, state->csi_base, offset[ch]);
}

static void
MIPI_CSIS_Set_CSIS_CMM_CTRL(const struct tcc_mipi_csi2_state *state)
{
	uint32_t val = 0U;
	uint32_t lane_number = 0U;

	/* for subtraction (-1)*/
	if (state->data_lane_num == 0U) {
		/* default value */
		lane_number = 1U;
	} else {
		/* actual data lane number */
		lane_number = state->data_lane_num;
	}

	val = tcc_mipi_csi2_readl(state->csi_base, CSIS_CMN_CTRL);

	val &= ~(CCTRL_UPDATE_SHADOW_MASK |
		CCTRL_DESKEW_LEVEL_MASK |
		CCTRL_DESKEW_ENABLE_MASK |
		CCTRL_INTERLEAVE_MODE_MASK |
		CCTRL_LANE_NUMBER_MASK |
		CCTRL_UPDATE_SHADOW_CTRL_MASK |
		CCTRL_SW_RESET_MASK |
		CCTRL_CSI_EN_MASK);

	val |= ((CCTRL_UPDATE_SHADOW_MASK) |
		(state->deskew_level << CCTRL_DESKEW_LEVEL_SHIFT) |
		(state->deskew_enable << CCTRL_DESKEW_ENABLE_SHIFT) |
		(state->interleave_mode << CCTRL_INTERLEAVE_MODE_SHIFT) |
		((lane_number - 1U) << CCTRL_LANE_NUMBER_SHIFT) |
		(state->update_shadow_ctrl << CCTRL_UPDATE_SHADOW_CTRL_SHIFT) |
		(0U << CCTRL_SW_RESET_SHIFT) |
		(1U << CCTRL_CSI_EN_SHIFT));

	tcc_mipi_csi2_writel(val, state->csi_base, CSIS_CMN_CTRL);
}

static void
MIPI_CSIS_Set_CLK_CTRL(const struct tcc_mipi_csi2_state *state,
		       uint32_t Clkgate_trail,
		       uint32_t Clkgate_en)
{
	uint32_t val = 0U;

	val = tcc_mipi_csi2_readl(state->csi_base, CSIS_CLK_CTRL);

	val &= ~(CCTRL_CLKGATE_TRAIL_MASK |
		CCTRL_CLKGATE_EN_MASK);

	val |= ((Clkgate_trail << CCTRL_CLKGATE_TRAIL_SHIFT) |
		(Clkgate_en << CCTRL_CLKGATE_EN_SHIFT));

	tcc_mipi_csi2_writel(val, state->csi_base, CSIS_CLK_CTRL);
}

static inline void MIPI_CSIS_Set_CSIS(const struct tcc_mipi_csi2_state *state)
{
	uint32_t idx = 0U;

	for (idx = 0U; idx < state->input_ch_num ; idx++) {
		MIPI_CSIS_Set_ISP_CONFIG(state, idx);
		MIPI_CSIS_Set_ISP_RESOL(state,
				idx,
				state->isp_info[idx].fmt.width,
				state->isp_info[idx].fmt.height);
	}

	MIPI_CSIS_Set_CLK_CTRL(state, 0x0U, 0xfU);
	MIPI_CSIS_Set_CSIS_CMM_CTRL(state);
}

static inline void MIPI_CSIS_Set_GDB(const struct tcc_mipi_csi2_state *state)
{
	uint32_t val = 0U;

	val = ((uint32_t)0xFFFFFFFFU);

	tcc_mipi_csi2_writel(val, state->gdb_base, VC0_DT_EN0);
	tcc_mipi_csi2_writel(val, state->gdb_base, VC0_DT_EN1);
	tcc_mipi_csi2_writel(val, state->gdb_base, VC1_DT_EN0);
	tcc_mipi_csi2_writel(val, state->gdb_base, VC1_DT_EN1);
	tcc_mipi_csi2_writel(val, state->gdb_base, VC2_DT_EN0);
	tcc_mipi_csi2_writel(val, state->gdb_base, VC2_DT_EN1);
	tcc_mipi_csi2_writel(val, state->gdb_base, VC3_DT_EN0);
	tcc_mipi_csi2_writel(val, state->gdb_base, VC3_DT_EN1);
}

static void MIPI_CSIS_Set_CSIS_Reset(const struct tcc_mipi_csi2_state *state,
				     uint32_t reset)
{
	uint32_t val = 0U, count = 0U;

	val = tcc_mipi_csi2_readl(state->csi_base, CSIS_CMN_CTRL);

	val &= ~(CCTRL_SW_RESET_MASK);

	if (reset == TCC_MIPI_RESET_RESET) {
		/* 0x1 : reset, 0x0 : release */
		val |= (1U << CCTRL_SW_RESET_SHIFT);
	}
	tcc_mipi_csi2_writel(val, state->csi_base, CSIS_CMN_CTRL);

	while ((tcc_mipi_csi2_readl(state->csi_base, CSIS_CMN_CTRL) &
	       CCTRL_SW_RESET_MASK) != 0U) {
		if (count > 50U) {
			loge(&(state->pdev->dev), "fail - MIPI_CSI2 reset\n");
			break;
		}
		usleep_range(1U, 2U);
		count++;
	}
}

static void MIPI_CSIS_Set_Enable(const struct tcc_mipi_csi2_state *state,
		uint32_t enable)
{
	uint32_t val = 0U;

	val = tcc_mipi_csi2_readl(state->csi_base, CSIS_CMN_CTRL);

	val &= ~(CCTRL_CSI_EN_MASK);

	if (enable == 1U) {
		/* enable csi */
		val |= (1U << CCTRL_CSI_EN_SHIFT);
	}

	tcc_mipi_csi2_writel(val, state->csi_base, CSIS_CMN_CTRL);
}

/**
 * MIPI_CSIS_Set_CIM() - Get CSIS Interrupt Mask register
 *
 * @state: pointer to &struct tcc_mipi_csi2_state
 * @pg: page number(0 ~ 1)
 * @mask: target interrupt
 * @enable: CIM_INTR_ENABLE, CIM_INTR_DISABLE
 */
static void
MIPI_CSIS_Set_CIM(const struct tcc_mipi_csi2_state *state,
		  uint32_t pg,
		  uint32_t mask,
		  uint32_t enable)
{
	uint32_t val = 0U, offset = 0U;

	if (pg == 0U) {
		/* page 0 */
		offset = CSIS_INT_MSK0;
	} else {
		/* page 1 */
		offset = CSIS_INT_MSK1;
	}

	val = (tcc_mipi_csi2_readl(state->csi_base, offset) & ~(mask));

	if (enable == CIM_INTR_ENABLE) {
		/* Interrupt enable*/
		val |= mask;
	}

	tcc_mipi_csi2_writel(val, state->csi_base, offset);
}

/**
 * MIPI_CSIS_Get_CIM() - Get CSIS Interrupt Mask register
 *
 * @state: pointer to &struct tcc_mipi_csi2_state
 * @pg: page number(0 ~ 1)
 */
static uint32_t
MIPI_CSIS_Get_CIM(const struct tcc_mipi_csi2_state *state, uint32_t pg)
{
	uint32_t val = 0U, offset = 0U;

	if (pg == 0U) {
		/* page 0 */
		offset = CSIS_INT_MSK0;
	} else {
		/* page 1 */
		offset = CSIS_INT_MSK1;
	}

	val = tcc_mipi_csi2_readl(state->csi_base, offset);

	return val;
}

static void MIPI_CSIS_Set_GDB_INTM(const struct tcc_mipi_csi2_state *state,
				   int32_t mask, uint32_t enable)
{
	uint32_t val = 0U;

	val = (tcc_mipi_csi2_readl(state->gdb_base, GDB_INTM) & ~(mask));

	if (enable == GDB_INTM_INTR_ENABLE) {
		/* Interrupt disable */
		val |= mask;
	}

	tcc_mipi_csi2_writel(val, state->gdb_base, GDB_INTM);
}

static uint32_t
MIPI_CSIS_Get_GDB_INTM(const struct tcc_mipi_csi2_state *state)
{
	uint32_t val = 0U;

	val = tcc_mipi_csi2_readl(state->gdb_base, GDB_INTM);

	return val;
}

/**
 * MIPI_CSIS_Set_CIS - Set CSIS Interrupt Source register
 *
 * @state: pointer to &struct tcc_mipi_csi2_state
 * @pg: page number(0 ~ 1)
 * @mask: target interrupt
 */
static void
MIPI_CSIS_Set_CIS(const struct tcc_mipi_csi2_state *state,
		  uint32_t pg,
		  uint32_t mask)
{
	uint32_t val = 0U, offset = 0U;

	if (pg == 0U) {
		/* page 0 */
		offset = CSIS_INT_SRC0;
	} else {
		/* page 1 */
		offset = CSIS_INT_SRC1;
	}

	val = tcc_mipi_csi2_readl(state->csi_base, offset);

	val |= mask;

	tcc_mipi_csi2_writel(val, state->csi_base, offset);
}

/**
 * MIPI_CSIS_Get_CIS - Get CSIS Interrupt Source register
 *
 * @state: pointer to &struct tcc_mipi_csi2_state
 * @pg: page number(0 ~ 1)
 */
static uint32_t
MIPI_CSIS_Get_CIS(const struct tcc_mipi_csi2_state *state, uint32_t pg)
{
	uint32_t val = 0U, offset = 0U;

	if (pg == 0U) {
		/* page 0 */
		offset = CSIS_INT_SRC0;
	} else {
		/* page 1 */
		offset = CSIS_INT_SRC1;
	}

	val = tcc_mipi_csi2_readl(state->csi_base, offset);

	return val;
}

static void MIPI_CSIS_Set_GDB_INT(const struct tcc_mipi_csi2_state *state,
				  uint32_t mask)
{
	uint32_t val = 0U;

	val = tcc_mipi_csi2_readl(state->gdb_base, GDB_INT);

	val |= mask;

	tcc_mipi_csi2_writel(val, state->gdb_base, GDB_INT);
}

static uint32_t
MIPI_CSIS_Get_GDB_INT(const struct tcc_mipi_csi2_state *state)
{
	uint32_t val = 0U;

	val = tcc_mipi_csi2_readl(state->gdb_base, GDB_INT);

	return val;
}

static void tcc_mipi_csi2_set_interface(const struct tcc_mipi_csi2_state *state,
					uint32_t onOff)
{
	if (onOff == 1U) {
		tcc_mipi_csi2_cfg_reset(state, 0U);
		// S/W reset CSI2
		MIPI_CSIS_Set_CSIS_Reset(state, TCC_MIPI_RESET_RELEASE);

		tcc_mipi_csi2_dphys_set_dphys(state);
		MIPI_CSIS_Set_CSIS(state);
		if (state->gdb_irq != 0U) {
			/* set generic data type */
			MIPI_CSIS_Set_GDB(state);
		}
	} else {
		MIPI_CSIS_Set_Enable(state, OFF);

		// S/W reset CSI2
		MIPI_CSIS_Set_CSIS_Reset(state, TCC_MIPI_RESET_RESET);
		tcc_mipi_csi2_cfg_reset(state, 1U);
	}
}

static void tcc_mipi_csi2_set_interrupt(const struct tcc_mipi_csi2_state *state,
					uint32_t onOff)
{
	/* page 0 */
	const uint32_t msk[] = {
		(CIM_MSK_ALL_MASK &
		 (~(CIM_MSK_FRAMESTART_MASK | CIM_MSK_FRAMEEND_MASK))),
		CIM_MSK_LINE_END_MASK,
	};

	if (onOff == 1U) {
		/*
		 * clear interrupt
		 */
		MIPI_CSIS_Set_CIS(state, 0U, CIS_SRC_ALL_MASK);
		MIPI_CSIS_Set_CIS(state, 1U, CIS_SRC_LINE_END_MASK);
		if (state->gdb_irq != 0U) {
			/* clear gdb interrupt */
			MIPI_CSIS_Set_GDB_INT(state, (uint32_t)0x30100U);
		}

		/*
		 * unmask interrupt
		 */
		MIPI_CSIS_Set_CIM(state, 0U, msk[0], CIM_INTR_ENABLE);
		MIPI_CSIS_Set_CIM(state, 1U, msk[1], CIM_INTR_ENABLE);
		if (state->gdb_irq != 0U) {
			/* enable gdb count interrupt */
			MIPI_CSIS_Set_GDB_INTM(state, (uint32_t)0x101U,
					       GDB_INTM_INTR_ENABLE);
		}
	} else {
		/*
		 * mask interrupt
		 */
		MIPI_CSIS_Set_CIM(state, 0U, msk[0], CIM_INTR_DISABLE);
		MIPI_CSIS_Set_CIM(state, 1U, msk[1], CIM_INTR_DISABLE);
		if (state->gdb_irq != 0U) {
			/* disabled gdb count interrupt */
			MIPI_CSIS_Set_GDB_INTM(state, (uint32_t)0x101U,
					       GDB_INTM_INTR_DISABLE);
		}
	}
}

void tcc_mipi_csi2_enable(const struct tcc_mipi_csi2_state *state,
			  uint32_t enable)
{
	tcc_mipi_csi2_set_interface(state, enable);

	tcc_mipi_csi2_set_interrupt(state, enable);
}

static bool is_interrupt(uint32_t sts, uint32_t mask)
{
	return ((sts & mask) != 0U);
}

static void which_interrupt(const struct tcc_mipi_csi2_state *state,
			    const struct tcc_mipi_csi2_intr_src src[],
			    uint32_t check)
{
	uint32_t idx = 0U;

	while (true) {
		if (src[idx].mask == 0U) {
			/* end checking interrupt */
			break;
		}

		if (is_interrupt(check, src[idx].mask)) {
			/* print which interrupt has been occurred */
			loge_ratelimited(&(state->pdev->dev), "%s\n", src[idx].desc);
		}
		idx++;
	}
}

static uint32_t
tcc_mipi_csi2_get_intr(const struct tcc_mipi_csi2_state *state, uint32_t pg)
{
	uint32_t ret = 0U, src = 0U, msk = 0U;

	/* get interrupt */
	src = MIPI_CSIS_Get_CIS(state, pg);
	msk = MIPI_CSIS_Get_CIM(state, pg);

	ret = (src & msk);

	/* clear interrupt */
	MIPI_CSIS_Set_CIS(state, pg, ret);

	return ret;
}

irqreturn_t tcc_mipi_csi2_isr(int irq, void *client_data)
{
	const struct tcc_mipi_csi2_state *state =
		(struct tcc_mipi_csi2_state *)client_data;
	uint32_t pg = 0U;
	uint32_t intr_status[] = {0U, 0U};
	irqreturn_t ret = IRQ_NONE;

	logd(&(state->pdev->dev), "irq number(%d)\n", irq);

	for (pg = 0U; pg < 2U; pg++) {
		intr_status[pg] = tcc_mipi_csi2_get_intr(state, pg);
		which_interrupt(state, intr_srcs[pg], intr_status[pg]);
	}

	ret = IRQ_HANDLED;

	return ret;
}

irqreturn_t tcc_mipi_csi2_gdb_isr(int irq, void *client_data)
{
	const struct tcc_mipi_csi2_state *state =
		(struct tcc_mipi_csi2_state *)client_data;
	irqreturn_t ret = IRQ_NONE;
	uint32_t loop = 0U, cnt = 0U;

	logd(&(state->pdev->dev), "GDB INT(0x%x), GDB INTM(0x%x)\n",
	     MIPI_CSIS_Get_GDB_INT(state), MIPI_CSIS_Get_GDB_INTM(state));

	cnt = tcc_mipi_csi2_readl(state->gdb_base, GDB_CNT);
	for (loop = 0U; loop < cnt; loop++) {
		logi(&(state->pdev->dev), "GDB DATA: 0x%x\n",
		     tcc_mipi_csi2_readl(state->gdb_base, GDB_RDATA));
	}

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
#if IS_ENABLED(CONFIG_ARCH_TCC803X)
	struct device_node *ddicfg_node = NULL;
#endif
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

#if defined(GENERIC_DATA_BUFFER_TEST)
	/* Get MIPI CSI-2 gdb base addr */
	res = platform_get_resource_byname(state->pdev, IORESOURCE_MEM, "gdb");
	state->gdb_base = devm_ioremap_resource(&state->pdev->dev, res);
	if (IS_ERR((const void *)state->gdb_base)) {
		/* error */
		ret = (int)PTR_ERR((const void *)state->gdb_base);
		loge(&(state->pdev->dev),
				"Invalid MIPI CSI2 gdb base addr(%d)\n",
				ret);
	}
#endif

#if defined(CONFIG_ARCH_TCC805X)
	/* Get CKC base address */
	if (ret == 0) {
		res = platform_get_resource_byname(state->pdev, IORESOURCE_MEM,
						   "ckc");
		state->ckc_base = ioremap(res->start, resource_size(res));
		if (IS_ERR((const void *)state->ckc_base)) {
			/* error */
			ret = (int)PTR_ERR((const void *)state->ckc_base);
			loge(&(state->pdev->dev),
					"Invalid CKC base addr(%d)\n",
					ret);
		}
	}
#endif
#if defined(CONFIG_ARCH_TCC805X)
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
#endif

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

#if defined(GENERIC_DATA_BUFFER_TEST)
	if (ret == 0) {
		ret = platform_get_irq_byname(state->pdev, "gdb");
		if (ret <= 0) {
			/* error */
			loge(&(state->pdev->dev),
					"Invalid IRQ(%d)\n",
					ret);
		} else {
			/* okay */
			state->gdb_irq = (uint32_t)ret;
			ret = 0;
		}
	}
#endif

#if IS_ENABLED(CONFIG_ARCH_TCC803X)
	/* DDI config base address */
	if (ret == 0) {
		ddicfg_node =
			of_find_compatible_node(NULL, NULL,
					"telechips,ddi_config");
		if (IS_ERR_OR_NULL(ddicfg_node)) {
			/* error */
			ret = -ENODEV;
			loge(&(state->pdev->dev),
					"invalid DDI Config node(%d)\n",
					ret);
		} else {
			/* okay */
			state->ddicfg_base =
				(void __iomem *)of_iomap(ddicfg_node, 0);
		}
	}
#endif
	return ret;
}

#if defined(CONFIG_ARCH_TCC805X)
static int tcc_mipi_csi2_dt_path_cfg(struct tcc_mipi_csi2_state *state)
{
	const struct device_node *nd = NULL;
	int ret = 0;

	const struct tcc_mipi_csi2_prop path_cfgs[] = {
		{
			.name = "mipi-chmux-0",
			.out = &state->mipi_chmux[0],
			.is_def = (bool)false,
		},
		{
			.name = "mipi-chmux-1",
			.out = &state->mipi_chmux[1],
			.is_def = (bool)false,
		},
		{
			.name = "mipi-chmux-2",
			.out = &state->mipi_chmux[2],
			.is_def = (bool)false,
		},
		{
			.name = "mipi-chmux-3",
			.out = &state->mipi_chmux[3],
			.is_def = (bool)false,
		},
		{
			.name = "mipi-chmux-4",
			.out = &state->mipi_chmux[4],
			.is_def = (bool)false,
		},
		{
			.name = "mipi-chmux-5",
			.out = &state->mipi_chmux[5],
			.is_def = (bool)false,
		},
		{
			.name = "mipi-chmux-6",
			.out = &state->mipi_chmux[6],
			.is_def = (bool)false,
		},
		{
			.name = "mipi-chmux-7",
			.out = &state->mipi_chmux[7],
			.is_def = (bool)false,
		},
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
#endif

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
#if defined(CONFIG_ARCH_TCC805X)
	if (ret == 0) {
		ret = tcc_mipi_csi2_dt_path_cfg(state);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
			     "tcc_mipi_csi2_dt_path_cfg returned %d",
			     ret);
		}
	}
#endif

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

	return ret;
}
