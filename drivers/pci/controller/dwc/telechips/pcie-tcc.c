// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of_pci.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/pci.h>
#include <linux/reset.h>
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/phy/phy.h>
#include <linux/debugfs.h>
#include <linux/irqdomain.h>
#include <linux/pinctrl/consumer.h>
#include <soc/telechips/dw_edma.h>

#include "../pcie-designware.h"

#ifdef CONFIG_ARCH_TCN100X
#include "pcie-axon.h"
#else
#include "pcie-dolphin.h"
#endif

#define to_tcc_pcie(x)	dev_get_drvdata((x)->dev)

#define to_phy_submode(r, m) \
	(((u32)(r) << 4) | (u32)(m))
#define phy_set_clk(phy, mode) \
	phy_set_mode_ext(phy, PHY_MODE_PCIE, mode)

#define to_atu_outb_reg(index, offset) \
	(u32)(PCIE_GET_ATU_OUTB_UNR_REG_OFFSET((index)) + (u32)(offset))

#define to_atu_inb_reg(index, offset) \
	(u32)(PCIE_GET_ATU_INB_UNR_REG_OFFSET((index)) + (u32)(offset))

/* BAR-match mode for Inbound iATU */
#ifndef PCIE_ATU_BAR_NUM
#define PCIE_ATU_BAR_NUM(bar)   (((u32)(bar) & 0x7U) << 8)
#endif

/*
 * PCIe controller wrapper DBI configuration registers
 */
#define PCIE_DBI2_OFFSET		(0x100000U)
#define DBI_PCIE_CAP_OFFSET		(0x70U)
#define DBI_PCIE_CAP_LINK_CONTROL		(DBI_PCIE_CAP_OFFSET + 0x10U)
#define DBI_PCIE_CAP_LINK_CONTROL2		(DBI_PCIE_CAP_OFFSET + 0x30U)
#define DBI_PCIE_PORT_LOGIC_OFFSET		(0x700U)
#define DBI_PCIE_PORT_LOGIC_GEN2_CTRL		(DBI_PCIE_PORT_LOGIC_OFFSET + 0x10CU)
#define DBI_PCIE_PORT_LOGIC_GEN3_RELATED		(DBI_PCIE_PORT_LOGIC_OFFSET + 0x190U)
#define DBI_PCIE_PORT_LOGIC_GEN3_EQ_CONTROL_OFF		(DBI_PCIE_PORT_LOGIC_OFFSET + 0x1A8U)
#define DBI_PCIE_PORT_LOGIC_GEN3_EQ_FB_MODE_DIR_CHANGE_OFF		(DBI_PCIE_PORT_LOGIC_OFFSET + 0x1ACU)
#define DBI_PCIE_PORT_LOGIC_PORT_LINK_CTRL		(DBI_PCIE_PORT_LOGIC_OFFSET + 0x10U)
#define DBI_PCIE_PORT_LOGIC_PIPE_LOOPBACK_CONTROL		(DBI_PCIE_PORT_LOGIC_OFFSET + 0x1B8U)
#define DBI_PCIE_PORT_LOGIC_MISC_CONTROL1		(DBI_PCIE_PORT_LOGIC_OFFSET + 0x1BCU)
#define DBI_PCIE_PORT_LOGIC_AMBA_ORDERING_CTRL		(DBI_PCIE_PORT_LOGIC_OFFSET + 0x1D8U)
#define DBI_PCIE_PORT_LOGIC_GEN4_LANE_MARGINING2		(DBI_PCIE_PORT_LOGIC_OFFSET + 0x480U)

#define DBI_PCIE_DMA_CAP_OFFSET		(0x380000U)
#define DBI_PCIE_DMA_CTRL_DATA_ARB_PRIOR		(DBI_PCIE_DMA_CAP_OFFSET + 0x000U)
#define DBI_PCIE_DMA_WRITE_ENGINE_EN		(DBI_PCIE_DMA_CAP_OFFSET + 0x00CU)
#define DBI_PCIE_DMA_WRITE_DOORBELL		(DBI_PCIE_DMA_CAP_OFFSET + 0x010U)
#define DBI_PCIE_DMA_WRITE_CHANNEL_ARB_WEIGHT_LOW		(DBI_PCIE_DMA_CAP_OFFSET + 0x018U)
#define DBI_PCIE_DMA_WRITE_INT_STATUS                (DBI_PCIE_DMA_CAP_OFFSET + 0x04CU)
#define DBI_PCIE_DMA_WRITE_INT_MASK		(DBI_PCIE_DMA_CAP_OFFSET + 0x054U)
#define DBI_PCIE_DMA_WRITE_INT_CLEAR		(DBI_PCIE_DMA_CAP_OFFSET + 0x58U)
#define DBI_PCIE_DMA_WRITE_DONE_IMWR_LOW		(DBI_PCIE_DMA_CAP_OFFSET + 0x060U)
#define DBI_PCIE_DMA_WRITE_DONE_IMWR_HIGH		(DBI_PCIE_DMA_CAP_OFFSET + 0x064U)
#define DBI_PCIE_DMA_WRITE_ABORT_IMWR_LOW		(DBI_PCIE_DMA_CAP_OFFSET + 0x068U)
#define DBI_PCIE_DMA_WRITE_ABORT_IMWR_HIGH		(DBI_PCIE_DMA_CAP_OFFSET + 0x06CU)
#define DBI_PCIE_DMA_WRITE_CH01_IMWR_DATA		(DBI_PCIE_DMA_CAP_OFFSET + 0x070U)
#define DBI_PCIE_DMA_WRITE_LINKED_LIST_ERR_EN		(DBI_PCIE_DMA_CAP_OFFSET + 0x090U)
#define DBI_PCIE_DMA_WR_CH_CONTROL1		(DBI_PCIE_DMA_CAP_OFFSET + 0x200U)
#define DBI_PCIE_DMA_WR_TRANSFER_SIZE		(DBI_PCIE_DMA_CAP_OFFSET + 0x208U)
#define DBI_PCIE_DMA_WR_SAR_LOW		(DBI_PCIE_DMA_CAP_OFFSET + 0x20CU)
#define DBI_PCIE_DMA_WR_SAR_HIGH		(DBI_PCIE_DMA_CAP_OFFSET + 0x210U)
#define DBI_PCIE_DMA_WR_DAR_LOW		(DBI_PCIE_DMA_CAP_OFFSET + 0x214U)
#define DBI_PCIE_DMA_WR_DAR_HIGH		(DBI_PCIE_DMA_CAP_OFFSET + 0x218U)

#define DBI_PCIE_DMA_READ_ENGINE_EN		(DBI_PCIE_DMA_CAP_OFFSET + 0x02CU)
#define DBI_PCIE_DMA_READ_DOORBELL		(DBI_PCIE_DMA_CAP_OFFSET + 0x030U)
#define DBI_PCIE_DMA_READ_CHANNEL_ARB_WEIGHT_LOW		(DBI_PCIE_DMA_CAP_OFFSET + 0x038U)
#define DBI_PCIE_DMA_READ_INT_STATUS                  (DBI_PCIE_DMA_CAP_OFFSET + 0x0A0U)
#define DBI_PCIE_DMA_READ_INT_MASK		(DBI_PCIE_DMA_CAP_OFFSET + 0x0A8U)
#define DBI_PCIE_DMA_READ_INT_CLEAR		(DBI_PCIE_DMA_CAP_OFFSET + 0x0ACU)
#define DBI_PCIE_DMA_READ_LINKED_LIST_ERR_EN		(DBI_PCIE_DMA_CAP_OFFSET + 0x0C4U)
#define DBI_PCIE_DMA_READ_DONE_IMWR_LOW		(DBI_PCIE_DMA_CAP_OFFSET + 0x0CCU)
#define DBI_PCIE_DMA_READ_DONE_IMWR_HIGH		(DBI_PCIE_DMA_CAP_OFFSET + 0x0D0U)
#define DBI_PCIE_DMA_READ_ABORT_IMWR_LOW		(DBI_PCIE_DMA_CAP_OFFSET + 0x0D4U)
#define DBI_PCIE_DMA_READ_ABORT_IMWR_HIGH		(DBI_PCIE_DMA_CAP_OFFSET + 0x0D8U)
#define DBI_PCIE_DMA_RD_CH_CONTROL1		(DBI_PCIE_DMA_CAP_OFFSET + 0x300U)
#define DBI_PCIE_DMA_RD_TRANSFER_SIZE		(DBI_PCIE_DMA_CAP_OFFSET + 0x308U)
#define DBI_PCIE_DMA_RD_SAR_LOW		(DBI_PCIE_DMA_CAP_OFFSET + 0x30CU)
#define DBI_PCIE_DMA_RD_SAR_HIGH		(DBI_PCIE_DMA_CAP_OFFSET + 0x310U)
#define DBI_PCIE_DMA_RD_DAR_LOW		(DBI_PCIE_DMA_CAP_OFFSET + 0x314U)
#define DBI_PCIE_DMA_RD_DAR_HIGH		(DBI_PCIE_DMA_CAP_OFFSET + 0x318U)

/*
 * Mask/shift bits in DBI configuration registers
 */
#define PCIE_DBI_LINK_SPEED_SHIFT		(16U)
#define PCIE_DBI_LINK_SPEED_MASK		((u32)0xFU << PCIE_DBI_LINK_SPEED_SHIFT)

#define PCIE_DBI_TARGET_LINK_SPEED_SHIFT		(0U)
#define PCIE_DBI_TARGET_LINK_SPEED_MASK		((u32)0xFU << PCIE_DBI_TARGET_LINK_SPEED_SHIFT)

#define PCIE_DBI_DIRECT_SPEED_CHANGE_SHIFT		(17U)
#define PCIE_DBI_DIRECT_SPEED_CHANGE_MASK		((u32)0x1U << PCIE_DBI_DIRECT_SPEED_CHANGE_SHIFT)

#define PCIE_DBI_EQ_REDO_DISABLE_SHIFT		(11U)
#define PCIE_DBI_EQ_REDO_DISABLE_MASK		((u32)0x1U << PCIE_DBI_EQ_REDO_DISABLE_SHIFT)

#define PCIE_DBI_LOOPBACK_ENABLE_SHIFT		(2U)
#define PCIE_DBI_LOOPBACK_ENABLE_MASK		((u32)0x1U << PCIE_DBI_LOOPBACK_ENABLE_SHIFT)

#define PCIE_DBI_PIPE_LOOPBACK_SHIFT		(31U)
#define PCIE_DBI_PIPE_LOOPBACK_MASK		((u32)0x1U << PCIE_DBI_PIPE_LOOPBACK_SHIFT)

#define PCIE_DBI_GEN3_EQUALIZATION_DISABLE_SHIFT (16U)
#define PCIE_DBI_GEN3_EQUALIZATION_DISABLE_MASK ((u32)0x1U << PCIE_DBI_GEN3_EQUALIZATION_DISABLE_SHIFT)

#define PCIE_DBI_GEN3_EQ_PSET_REQ_VEC_SHIFT		(8U)
#define PCIE_DBI_GEN3_EQ_PSET_REQ_VEC_MASK		((u32)0xFFFFU << PCIE_DBI_GEN3_EQ_PSET_REQ_VEC_SHIFT)

#define PCIE_DBI_AX_MSTR_ORDR_P_EVENT_SEL_SHIFT		(3)
#define PCIE_DBI_AX_MSTR_ORDR_P_EVENT_SEL_MASK		((u32)0x3U << PCIE_DBI_AX_MSTR_ORDR_P_EVENT_SEL_SHIFT)

#define PCIE_DBI_GEN4_RXMARGIN_MAX_VOLTAGE_OFFSET_SHIFT		(24U)
#define PCIE_DBI_GEN4_RXMARGIN_NUM_VOLTAGE_STEPS_SHIFT		(16U)
#define PCIE_DBI_GEN4_RXMARGIN_MAX_TIMING_OFFSET_SHIFT		(8U)
#define PCIE_DBI_GEN4_RXMARGIN_NUM_TIMING_STEPS_SHIFT		(0U)
#define PCIE_DBI_GEN4_RXMARGIN_MAX_VOLTAGE_OFFSET_MASK		((u32)0x3FU << PCIE_DBI_GEN4_RXMARGIN_MAX_VOLTAGE_OFFSET_SHIFT)
#define PCIE_DBI_GEN4_RXMARGIN_NUM_VOLTAGE_STEPS_MASK		((u32)0x7FU << PCIE_DBI_GEN4_RXMARGIN_NUM_VOLTAGE_STEPS_SHIFT)
#define PCIE_DBI_GEN4_RXMARGIN_MAX_TIMING_OFFSET_MASK		((u32)0x3FU << PCIE_DBI_GEN4_RXMARGIN_MAX_TIMING_OFFSET_SHIFT)
#define PCIE_DBI_GEN4_RXMARGIN_NUM_TIMING_STEPS_MASK		((u32)0x3FU << PCIE_DBI_GEN4_RXMARGIN_NUM_TIMING_STEPS_SHIFT)

#define PCIE_DBI_DMA_ABORT_INT_STATUS_SHIFT		(16U)
#define PCIE_DBI_DMA_DONE_INT_STATUS_SHIFT		(0U)
#define PCIE_DBI_DMA_ABORT_INT_STATUS_MASK		((u32)0xFU << PCIE_DBI_DMA_ABORT_INT_STATUS_SHIFT)
#define PCIE_DBI_DMA_DONE_INT_STATUS_MASK		((u32)0xFU << PCIE_DBI_DMA_DONE_INT_STATUS_SHIFT)

#define PCIE_DBI_DMA_CS_SHIFT		(0x5U)
#define PCIE_DBI_DMA_CS_RUN_MASK		(0x1U << PCIE_DBI_DMA_CS_SHIFT)
#define PCIE_DBI_DMA_CS_HALT_MASK		(0x2U << PCIE_DBI_DMA_CS_SHIFT)
#define PCIE_DBI_DMA_CS_STOP_MASK		(0x3U << PCIE_DBI_DMA_CS_SHIFT)
#define PCIE_DBI_DMA_CS_MASK		(PCIE_DBI_DMA_CS_RUN_MASK |  \
	PCIE_DBI_DMA_CS_HALT_MASK | \
	PCIE_DBI_DMA_CS_STOP_MASK)

#define PCIE_LINK_UP_TIMEOUT		(120LL)
#define PCIE_LINK_UP_DELAY		(1U)
#define PCIE_VDM_MSG_GRANT_TIMEOUT		(1000LL)
#define PCIE_VDM_MSG_GRANT_DELAY		(1U)
#define PCIE_REG_BACKUP_SIZE			(0x90U)

enum tcc_pcie_gen {
	PCIE_GEN1 = 1,
	PCIE_GEN2,
	PCIE_GEN3,
	PCIE_GEN4,
};

enum tcc_pcie_variants {
	TCC803X,
	TCC805X,
	TCC807X,
	TCC750X,
	TCN100X,
};

enum tcc_pcie_refclk_src {
	REFCLK_SRC_NA = 0,
	REFCLK_SRC_XO,
	REFCLK_SRC_IO,
	REFCLK_SRC_PLL,
};

enum tcc_pcie_link_state {
	PCIE_LINK_DOWN = 0,
	PCIE_LINK_UP,
};

struct tcc_pcie_of_data {
	enum tcc_pcie_variants variant;
	enum dw_pcie_device_mode mode;
};

enum tcc_pcie_loopback_mode {
	LOOPBACK_MODE_NONE,
	LOOPBACK_MODE_LOCAL_DIGITAL,
	LOOPBACK_MODE_LOCAL_ANALOG,
	LOOPBACK_MODE_REMOTE_DIGITAL,
};

enum tcc_pcie_msg_route {
	DP_PCIE_MSG_ROUTE_TO_RC = 0,
	DP_PCIE_MSG_ROUTE_BY_ADDR,
	DP_PCIE_MSG_ROUTE_BY_ID,
	DP_PCIE_MSG_BROADCAST_DOWNSTREAM,
	DP_PCIE_MSG_TERMINATE_AT_RC,
	DP_PCIE_MSG_GNR_TO_RC,
	DP_PCIE_MSG_RESERVED1,
	DP_PCIE_MSG_RESERVED2,
};

struct tcc_pcie_edma {
	atomic_t cond;
	s32 irq;
};

struct tcc_pcie {
	struct dw_pcie		*pci;
	void __iomem		*link_base;
	void __iomem		*suspend_regs;
	struct reset_control		*reset;
	struct phy		*phy;
	u32		refclk_type;
	s32		irq;
	u32		max_link_speed;

	enum tcc_pcie_variants		variant;
	enum dw_pcie_device_mode		mode;
	struct tcc_pcie_edma		*edma;
#ifdef CONFIG_DEBUG_FS
	enum tcc_pcie_loopback_mode		loopback_mode;
#endif
};

static s32 tcc_pcie_backup_reg(const struct tcc_pcie *tp)
{
	s32 err = 0;

	if (tp != NULL) {
		const struct dw_pcie *pci = tp->pci;

		if (tp->suspend_regs != NULL) {
			(void)memcpy(tp->suspend_regs,
					pci->dbi_base,
					PCIE_REG_BACKUP_SIZE);
		} else {
			err = -ENOMEM;
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_restore_reg(const struct tcc_pcie *tp)
{
	s32 err = 0;

	if (tp != NULL) {
		const struct dw_pcie *pci = tp->pci;

		if (tp->suspend_regs != NULL) {
			(void)memcpy(pci->dbi_base,
					tp->suspend_regs,
					PCIE_REG_BACKUP_SIZE);
		} else {
			err = -ENOMEM;
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static inline u32 tcc_pcie_readl(void __iomem *base, u32 offset)
{
	u32 ret = 0;

	if (base != NULL) {
		ret = ioread32(base + offset);
	}

	return ret;
}

static inline void tcc_pcie_writel(void __iomem *base, u32 offset, u32 value,
		u32 mask)
{
	if (base != NULL) {
		iowrite32((ioread32(base + offset) & ~mask)|value,
				base + offset);
	}
}

#ifdef CONFIG_DEBUG_FS
static s32 tcc_pcie_grant_is_raised(const struct tcc_pcie *tp)
{
	s32 err = 0;

	if (tp != NULL) {
		struct timespec64 start, end, gap;
		u32 mask;

		mask = PCIE_LINK_CFG_VEN_MSG_GRANT_MASK;
		ktime_get_ts64(&start);
		while ((tcc_pcie_readl(tp->link_base, PCIE_VDM_CTRL) & mask) == 0x0U) {
			ktime_get_ts64(&end);
			gap = timespec64_sub(end, start);
			if ((timespec64_to_ns(&gap)/NSEC_PER_MSEC) > PCIE_VDM_MSG_GRANT_TIMEOUT) {
				err = -EBUSY;
				break;
			} else {
				mdelay(PCIE_VDM_MSG_GRANT_DELAY);
			}
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static int tcc_pcie_send_vdm(void *data, u64 msg)
{
	s32 err = 0;

	if (data != NULL) {
		const struct tcc_pcie *tp = (const struct tcc_pcie *)data;
		const struct dw_pcie *pci = tp->pci;
		u32 val, mask;

		/* Clear VEN_MSG_REQ */
		val = 0x0U;
		mask = PCIE_LINK_CFG_VEN_MSG_REQ_MASK;
		tcc_pcie_writel(tp->link_base, PCIE_VDM_CTRL, val, mask);

		/* Set VEN_MSG_FMT (Should be set to 0x1) */
		val = ((u32)0x1U << PCIE_LINK_CFG_VEN_MSG_FMT_SHIFT);
		mask = PCIE_LINK_CFG_VEN_MSG_FMT_MASK;
		tcc_pcie_writel(tp->link_base, PCIE_VDM_CTRL, val, mask);

		/* Set VEN_MSG_LEN (Should be set to 0x0) */
		val = 0x0U;
		mask = PCIE_LINK_CFG_VEN_MSG_LEN_MASK;
		tcc_pcie_writel(tp->link_base, PCIE_VDM_CTRL, val, mask);

		/* Set VEN_MSG_TYPE (Routing: Route to RC)*/
		val = PCIE_LINK_CFG_VEN_MSG_TYPE(DP_PCIE_MSG_ROUTE_TO_RC);
		mask = PCIE_LINK_CFG_VEN_MSG_TYPE_MASK;
		tcc_pcie_writel(tp->link_base, PCIE_VDM_CTRL, val, mask);

		/* Set VEN_MSG_CODE (Vendor Defined Message 0) */
		val = ((u32)0x7EU << PCIE_LINK_CFG_VEN_MSG_CODE_SHIFT);
		mask = PCIE_LINK_CFG_VEN_MSG_CODE_MASK;
		tcc_pcie_writel(tp->link_base, PCIE_VDM_CODE, val, mask);

		/* Set Vendor ID */
		val = ((u32)tp->variant << PCIE_LINK_CFG_VENDOR_ID_SHIFT);
		mask = PCIE_LINK_CFG_VENDOR_ID_MASK;
		tcc_pcie_writel(tp->link_base, PCIE_VDM_DATA_UPPER, val, mask);

		/* Set Vendor defined data */
		val = lower_32_bits(msg);
		mask = PCIE_LINK_CFG_VDM_DATA_MASK;
		tcc_pcie_writel(tp->link_base, PCIE_VDM_DATA_LOWER, val, mask);

		/* Set VEN_MSG_REQ */
		val = ((u32)0x1U << PCIE_LINK_CFG_VEN_MSG_REQ_SHIFT);
		mask = PCIE_LINK_CFG_VEN_MSG_REQ_MASK;
		tcc_pcie_writel(tp->link_base, PCIE_VDM_CTRL, val, mask);

		/* wait for VEN_MSG_GRANT */
		err = tcc_pcie_grant_is_raised(tp);
		if (err != 0) {
			dev_err(pci->dev, "Failed to write vdm(%d)\n", err);
		}

		/* Clear VEN_MSG_REQ */
		val = ((u32)0x0U << PCIE_LINK_CFG_VEN_MSG_REQ_SHIFT);
		mask = PCIE_LINK_CFG_VEN_MSG_REQ_MASK;
		tcc_pcie_writel(tp->link_base, PCIE_VDM_CTRL, val, mask);

		/* Clear VEN_MSG_GRANT */
		val = ((u32)0x1U << PCIE_LINK_CFG_VEN_MSG_GRANT_SHIFT);
		mask = PCIE_LINK_CFG_VEN_MSG_GRANT_MASK;
		tcc_pcie_writel(tp->link_base, PCIE_VDM_CTRL, val, mask);
	} else {
		err = -EINVAL;
	}

	return err;
}

static int tcc_pcie_recv_vdm(void *data, u64 *msg)
{
	s32 err = 0;

	if ((data != NULL) && (msg != NULL)) {
		const struct tcc_pcie *tp = (const struct tcc_pcie *)data;
		u32 val, mask;

		mask = PCIE_LINK_CFG_RADM_VENDOR_MSG_MASK;
		val = tcc_pcie_readl(tp->link_base, PCIE_RADM_MSG_STS) & mask;
		if (val != 0x0U) {
			tcc_pcie_writel(tp->link_base, PCIE_RADM_MSG_STS, val, mask);
			*msg = (u64)tcc_pcie_readl(tp->link_base, PCIE_RADM_MSG_DATA);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_disable_loopback(const struct tcc_pcie *tp)
{
	s32 err = 0;

	if (tp != NULL) {
		struct dw_pcie *pci = tp->pci;
		u32 val, mask;

		dw_pcie_dbi_ro_wr_en(pci);

		switch (tp->loopback_mode) {
		case LOOPBACK_MODE_LOCAL_ANALOG:
		case LOOPBACK_MODE_REMOTE_DIGITAL:
			mask = PCIE_DBI_LOOPBACK_ENABLE_MASK;
			val = dw_pcie_readl_dbi(pci, DBI_PCIE_PORT_LOGIC_PORT_LINK_CTRL);
			val &= ~mask;
			dw_pcie_writel_dbi(pci, DBI_PCIE_PORT_LOGIC_PORT_LINK_CTRL, val);
			break;
		case LOOPBACK_MODE_LOCAL_DIGITAL:
			mask = PCIE_DBI_PIPE_LOOPBACK_MASK;
			/* 1. Clear PIPE_LOOPBACK in PIPE_LOOPBACK_CONTROL_OFF */
			val = dw_pcie_readl_dbi(pci, DBI_PCIE_PORT_LOGIC_PIPE_LOOPBACK_CONTROL);
			val &= ~mask;
			dw_pcie_writel_dbi(pci, DBI_PCIE_PORT_LOGIC_PIPE_LOOPBACK_CONTROL, val);

			/* 2. Clear LOOPBACK_ENABLE in PORT_LINK_CTRL_OFF */
			mask = PCIE_DBI_LOOPBACK_ENABLE_MASK;
			val = dw_pcie_readl_dbi(pci, DBI_PCIE_PORT_LOGIC_PORT_LINK_CTRL);
			val &= ~mask;
			dw_pcie_writel_dbi(pci, DBI_PCIE_PORT_LOGIC_PORT_LINK_CTRL, val);
			break;
		case LOOPBACK_MODE_NONE:
		default:
			err = -EINVAL;
			break;
		}

		dw_pcie_dbi_ro_wr_dis(pci);
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_enable_loopback(const struct tcc_pcie *tp, enum tcc_pcie_loopback_mode mode)
{
	s32 err = 0;

	if (tp != NULL) {
		struct dw_pcie *pci = tp->pci;
		u32 val, mask;

		dw_pcie_dbi_ro_wr_en(pci);

		switch (mode) {
		case LOOPBACK_MODE_LOCAL_ANALOG:
		case LOOPBACK_MODE_REMOTE_DIGITAL:
			mask = PCIE_DBI_LOOPBACK_ENABLE_MASK;
			val = dw_pcie_readl_dbi(pci, DBI_PCIE_PORT_LOGIC_PORT_LINK_CTRL);
			val |= mask;
			dw_pcie_writel_dbi(pci, DBI_PCIE_PORT_LOGIC_PORT_LINK_CTRL, val);
			break;
		case LOOPBACK_MODE_LOCAL_DIGITAL:
			/* 1. Set GEN3_EQUALIZATION_DISABLE in the GEN3_RELATED_OFF */
			mask = PCIE_DBI_GEN3_EQUALIZATION_DISABLE_MASK;
			val = dw_pcie_readl_dbi(pci, DBI_PCIE_PORT_LOGIC_GEN3_RELATED);
			val |= mask;
			dw_pcie_writel_dbi(pci, DBI_PCIE_PORT_LOGIC_GEN3_RELATED, val);

			/* 2. Set the PIPE_LOOPBACK in the PIPE_LOOPBACK_CONTROL_OFF register.*/
			mask = PCIE_DBI_PIPE_LOOPBACK_MASK;
			val = dw_pcie_readl_dbi(pci, DBI_PCIE_PORT_LOGIC_PIPE_LOOPBACK_CONTROL);
			val |= mask;
			dw_pcie_writel_dbi(pci, DBI_PCIE_PORT_LOGIC_PIPE_LOOPBACK_CONTROL, val);

			/* 3. Set the LOOPBACK_ENABLE in the PORT_LINK_CTRL_OFF register */
			mask = PCIE_DBI_LOOPBACK_ENABLE_MASK;
			val = dw_pcie_readl_dbi(pci, DBI_PCIE_PORT_LOGIC_PORT_LINK_CTRL);
			val |= mask;
			dw_pcie_writel_dbi(pci, DBI_PCIE_PORT_LOGIC_PORT_LINK_CTRL, val);
			break;
		case LOOPBACK_MODE_NONE:
		default:
			err = -EINVAL;
			break;
		}

		dw_pcie_dbi_ro_wr_dis(pci);
	} else {
		err = -EINVAL;
	}

	return err;
}

static int tcc_pcie_loopback_get_mode(void *data, u64 *val)
{
	s32 err = 0;

	if ((data != NULL) && (val != NULL)) {
		const struct tcc_pcie *tp =
			(const struct tcc_pcie *)data;

		*val = (u64)tp->loopback_mode;
	} else {
		err = -EINVAL;
	}

	return err;
}

static int tcc_pcie_loopback_set_mode(void *data, u64 val)
{
	s32 err = 0;

	if (data != NULL) {
		struct tcc_pcie *tp = (struct tcc_pcie *)data;
		enum tcc_pcie_loopback_mode mode;

		if (val <= (u64)LOOPBACK_MODE_REMOTE_DIGITAL) {
			mode = (enum tcc_pcie_loopback_mode)val;

			switch (mode) {
			case LOOPBACK_MODE_NONE:
				err = tcc_pcie_disable_loopback((const struct tcc_pcie *)tp);
				break;
			case LOOPBACK_MODE_LOCAL_DIGITAL:
			case LOOPBACK_MODE_LOCAL_ANALOG:
			case LOOPBACK_MODE_REMOTE_DIGITAL:
				err = tcc_pcie_enable_loopback((const struct tcc_pcie *)tp, mode);
				break;
			default:
				err = -EINVAL;
				break;
			}
		} else {
			err = -EINVAL;
		}

		if (err == 0) {
			tp->loopback_mode = mode;
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static int tcc_pcie_get_link_speed(void *data, u64 *speed)
{
	s32 err = 0;

	if ((data != NULL) && (speed != NULL)) {
		const struct tcc_pcie *tp = (const struct tcc_pcie *)data;
		struct dw_pcie *pci = tp->pci;
		u32 val, mask;

		mask = PCIE_DBI_LINK_SPEED_MASK;
		val = dw_pcie_readl_dbi(pci, DBI_PCIE_CAP_LINK_CONTROL) & mask;
		val = val >> PCIE_DBI_LINK_SPEED_SHIFT;
		*speed = (u64)val;
	} else {
		err = -EINVAL;
	}

	return err;
}

static int tcc_pcie_set_link_speed(void *data, u64 speed)
{
	s32 err = 0;

	if (data != NULL) {
		const struct tcc_pcie *tp = (const struct tcc_pcie *)data;
		struct dw_pcie *pci = tp->pci;

		if (speed > (u64)PCIE_GEN4) {
			err = -EINVAL;
		} else {
			u32 val, mask;

			dw_pcie_dbi_ro_wr_en(pci);

			mask = PCIE_DBI_TARGET_LINK_SPEED_MASK;
			val = dw_pcie_readl_dbi(pci, DBI_PCIE_CAP_LINK_CONTROL2) & ~mask;
			val |= ((u32)speed << PCIE_DBI_TARGET_LINK_SPEED_SHIFT);
			dw_pcie_writel_dbi(pci, DBI_PCIE_CAP_LINK_CONTROL2, val);

			mask = PCIE_DBI_DIRECT_SPEED_CHANGE_MASK;
			val = dw_pcie_readl_dbi(pci, DBI_PCIE_PORT_LOGIC_GEN2_CTRL) & ~mask;
			dw_pcie_writel_dbi(pci, DBI_PCIE_PORT_LOGIC_GEN2_CTRL, val);

			val |= ((u32)0x1U << PCIE_DBI_DIRECT_SPEED_CHANGE_SHIFT);
			dw_pcie_writel_dbi(pci, DBI_PCIE_PORT_LOGIC_GEN2_CTRL, val);

			dw_pcie_dbi_ro_wr_dis(pci);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

DEFINE_SIMPLE_ATTRIBUTE(tcc_pcie_link_speed_fops, tcc_pcie_get_link_speed,
		tcc_pcie_set_link_speed, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(tcc_pcie_loopback_fops, tcc_pcie_loopback_get_mode,
		tcc_pcie_loopback_set_mode, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(tcc_pcie_vdm_fops, tcc_pcie_recv_vdm, tcc_pcie_send_vdm,
		"%llu\n");

static s32 tcc_pcie_debugfs_init(struct tcc_pcie *tp)
{
	s32 err = 0;

	if (tp != NULL) {
		struct dw_pcie *pci = tp->pci;
		struct device *dev = pci->dev;
		const char *name;

		name = devm_kasprintf(dev, GFP_KERNEL, "%pOFP", dev->of_node);
		if (name != NULL) {
			struct dentry *dbgfs_dir;

			dbgfs_dir = debugfs_create_dir(name, NULL);
			(void)debugfs_create_file("link_speed", ((uint16_t)0x180),
					dbgfs_dir, (void *)tp,
					&tcc_pcie_link_speed_fops);
			(void)debugfs_create_file("loopback_mode", ((uint16_t)0x180),
					dbgfs_dir, (void *)tp,
					&tcc_pcie_loopback_fops);
			(void)debugfs_create_file("vdm", ((uint16_t)0x180), dbgfs_dir,
					(void *)tp, &tcc_pcie_vdm_fops);
		} else {
			err = -ENOMEM;
		}
	} else {
		err = -EINVAL;
	}

	return err;
}
#endif

static s32 tcc_pcie_reset_control(const struct tcc_pcie *tp)
{
	s32 err = 0;

	if (tp != NULL) {
		if (IS_ENABLED(CONFIG_RESET_TELECHIPS) != 0) {
			if (tp->reset != NULL) {
				err = reset_control_assert(tp->reset);
				if (err == 0) {
					mdelay(10);
					err = reset_control_deassert(tp->reset);
				}
			}
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_disable_irq(const struct tcc_pcie *tp)
{
	s32 err = 0;

	if (tp != NULL) {
		const struct dw_pcie *pci = tp->pci;
		const struct pcie_port *pp = &pci->pp;
		s32 irq;

		irq = (tp->mode == DW_PCIE_RC_TYPE) ? pp->irq : tp->irq;
		if (irq >= 0) {
			disable_irq((u32)irq);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_enable_irq(const struct tcc_pcie *tp)
{
	s32 err = 0;

	if (tp != NULL) {
		const struct dw_pcie *pci = tp->pci;
		const struct pcie_port *pp = &pci->pp;
		s32 irq;

		irq = (tp->mode == DW_PCIE_RC_TYPE) ? pp->irq : tp->irq;
		if (irq > 0) {
			enable_irq((u32)irq);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_gen4_init(const struct tcc_pcie *tp)
{
	s32 err = 0;

	if (tp != NULL) {
		struct dw_pcie *pci = tp->pci;
		u32 val, mask;

		dw_pcie_dbi_ro_wr_en(pci);

		if (tp->max_link_speed >= (u32)PCIE_GEN3) {
			mask = PCIE_DBI_GEN3_EQ_PSET_REQ_VEC_MASK;
			val = dw_pcie_readl_dbi(pci, DBI_PCIE_PORT_LOGIC_GEN3_EQ_CONTROL_OFF) & ~mask;
			val |= ((u32)0x59FU << PCIE_DBI_GEN3_EQ_PSET_REQ_VEC_SHIFT);
			dw_pcie_writel_dbi(pci, DBI_PCIE_PORT_LOGIC_GEN3_EQ_CONTROL_OFF, val);
		}

		if (tp->max_link_speed == (u32)PCIE_GEN4) {
			mask = PCIE_DBI_GEN4_RXMARGIN_MAX_VOLTAGE_OFFSET_MASK |
				PCIE_DBI_GEN4_RXMARGIN_NUM_VOLTAGE_STEPS_MASK |
				PCIE_DBI_GEN4_RXMARGIN_MAX_TIMING_OFFSET_MASK |
				PCIE_DBI_GEN4_RXMARGIN_NUM_TIMING_STEPS_MASK;
			val = dw_pcie_readl_dbi(pci, DBI_PCIE_PORT_LOGIC_GEN4_LANE_MARGINING2) & ~mask;
			val |= ((u32)0x1FU << PCIE_DBI_GEN4_RXMARGIN_NUM_TIMING_STEPS_SHIFT);
			val |= ((u32)0x32U << PCIE_DBI_GEN4_RXMARGIN_MAX_TIMING_OFFSET_SHIFT);
			val |= ((u32)0x7FU << PCIE_DBI_GEN4_RXMARGIN_NUM_VOLTAGE_STEPS_SHIFT);
			val |= ((u32)0x28U << PCIE_DBI_GEN4_RXMARGIN_MAX_VOLTAGE_OFFSET_SHIFT);
			dw_pcie_writel_dbi(pci, DBI_PCIE_PORT_LOGIC_GEN4_LANE_MARGINING2, val);
		}

		mask = PCIE_DBI_AX_MSTR_ORDR_P_EVENT_SEL_MASK;
		val = dw_pcie_readl_dbi(pci, DBI_PCIE_PORT_LOGIC_AMBA_ORDERING_CTRL) & ~mask;
		val |= ((u32)0x1U << PCIE_DBI_AX_MSTR_ORDR_P_EVENT_SEL_SHIFT);
		dw_pcie_writel_dbi(pci, DBI_PCIE_PORT_LOGIC_AMBA_ORDERING_CTRL, val);

		dw_pcie_dbi_ro_wr_dis(pci);
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_set_dev_type(const struct tcc_pcie *tp)
{
	s32 err = 0;

	if (tp != NULL) {
		u32 val;

		switch (tp->mode) {
		case DW_PCIE_RC_TYPE:
			val = ((u32)0x4U  << PCIE_LINK_CFG_DEVICE_TYPE_SHIFT);
			break;
		case DW_PCIE_EP_TYPE:
			val = 0x0U;
			break;
		case DW_PCIE_LEG_EP_TYPE:
		case DW_PCIE_UNKNOWN_TYPE:
		default:
			err = -ENODEV;
			break;
		}

		if (err == 0) {
			u32 mask;

			mask = PCIE_LINK_CFG_DEVICE_TYPE_MASK;
			tcc_pcie_writel(tp->link_base, PCIE_APP_CTRL,
					val, mask);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_set_max_link_speed(struct dw_pcie *pci)
{
	s32 err = 0;

	if (pci != NULL) {
		struct tcc_pcie *tp = (struct tcc_pcie *)to_tcc_pcie(pci);
		u32 val, mask;

		dw_pcie_dbi_ro_wr_en(pci);
		mask = PCIE_DBI_TARGET_LINK_SPEED_MASK;
		val = dw_pcie_readl_dbi(pci, DBI_PCIE_CAP_LINK_CONTROL2) & ~mask;
		val |= ((u32)tp->max_link_speed << PCIE_DBI_TARGET_LINK_SPEED_SHIFT);
		dw_pcie_writel_dbi(pci, DBI_PCIE_CAP_LINK_CONTROL2, val);
		dw_pcie_dbi_ro_wr_dis(pci);
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_set_layer(struct dw_pcie *pci)
{
	s32 err = 0;

	if (pci != NULL) {
		struct tcc_pcie *tp = (struct tcc_pcie *)to_tcc_pcie(pci);
		u32 val, mask;

		dw_pcie_dbi_ro_wr_en(pci);
		if (tp->max_link_speed >= (u32)PCIE_GEN2) {
			mask = PCIE_DBI_DIRECT_SPEED_CHANGE_MASK;
			val = dw_pcie_readl_dbi(pci, DBI_PCIE_PORT_LOGIC_GEN2_CTRL) & ~mask;
			val |= ((u32)0x1U << PCIE_DBI_DIRECT_SPEED_CHANGE_SHIFT);
			dw_pcie_writel_dbi(pci, DBI_PCIE_PORT_LOGIC_GEN2_CTRL, val);
		}

		if (tp->max_link_speed >= (u32)PCIE_GEN3) {
			mask = PCIE_DBI_EQ_REDO_DISABLE_MASK;
			val = dw_pcie_readl_dbi(pci, DBI_PCIE_PORT_LOGIC_GEN3_RELATED) & ~mask;
			val |= ((u32)0x1U << PCIE_DBI_EQ_REDO_DISABLE_SHIFT);
			dw_pcie_writel_dbi(pci, DBI_PCIE_PORT_LOGIC_GEN3_RELATED, val);
		}
		dw_pcie_dbi_ro_wr_dis(pci);
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_init_phy(const struct tcc_pcie *tp)
{
	s32 err = 0;

	if (tp != NULL) {
		err = phy_init(tp->phy);
		if (err == 0) {
			if (tp->variant == TCC807X) {
				err = tcc_pcie_gen4_init(tp);
			}
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_set_defaults(const struct tcc_pcie *tp)
{
	s32 err = 0;

	if (tp != NULL) {
		struct dw_pcie *pci = tp->pci;

		err = tcc_pcie_init_phy(tp);
		if (err == 0) {
			err = tcc_pcie_set_dev_type(tp);
		}

		if (err == 0) {
			err = tcc_pcie_set_max_link_speed(pci);
		}

		if (err == 0) {
			err = tcc_pcie_set_layer(pci);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_enable_ltssm(const struct tcc_pcie *tp, bool enable)
{
	s32 err = 0;

	if (tp != NULL) {
		u32 val, mask;

		val = enable ? ((u32)0x1U << PCIE_LINK_CFG_LTSSM_ENABLE_SHIFT) : 0x0U;
		mask = PCIE_LINK_CFG_LTSSM_ENABLE_MASK;
		tcc_pcie_writel(tp->link_base, PCIE_LTSSM_CTRL,
				val, mask);
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_clear_cactive(const struct tcc_pcie *tp)
{
	s32 err = 0;

	if (tp != NULL) {
		u32 val, mask;

		val = 0x0U;
		mask = 0xFFFFFFFFU;
		tcc_pcie_writel(tp->link_base, PCIE_AXI_CTRL,
				val, mask);
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_clear_interrupts(const struct tcc_pcie *tp)
{
	s32 err = 0;

	if (tp != NULL) {
		u32 val, mask;

		val = 0xFFFFFFFFU;
		mask = val;
		tcc_pcie_writel(tp->link_base, PCIE_LINK_STS_CLR,
				val, mask);
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_enable_interrupt(const struct tcc_pcie *tp, bool enable)
{
	s32 err = 0;

	if (tp != NULL) {
		u32 val, mask;

		mask = PCIE_LINK_CFG_INTR_EN_MASK;
		val = enable ? mask : 0x0U;
		tcc_pcie_writel(tp->link_base, PCIE_LINK_INTR, val, mask);
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_set_interrupt(const struct tcc_pcie *tp, bool enable)
{
	s32 err = 0;

	if (tp != NULL) {
		err = tcc_pcie_clear_interrupts(tp);
		if (err == 0) {
			err = tcc_pcie_enable_interrupt(tp, enable);
		}

		if (err == 0) {
			if ((IS_ENABLED(CONFIG_PCI_MSI) != 0) &&
					(tp->mode == DW_PCIE_RC_TYPE)) {
				struct dw_pcie *pci = tp->pci;
				struct pcie_port *pp = &pci->pp;

				dw_pcie_msi_init(pp);
			}
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static void tcc_pcie_write_dbi(struct dw_pcie *pci, void __iomem *base,
		u32 reg, size_t size, u32 val)
{
	if (pci != NULL) {
		const struct tcc_pcie *tp = (const struct tcc_pcie *)to_tcc_pcie(pci);
		u32 mask, write_val = val;
		s32 sz;

		if (tp->variant != TCC803X) {
			mask = 0x3FFFFFFFU;
			if ((reg == to_atu_outb_reg(0, PCIE_ATU_UNR_LOWER_BASE)) ||
					(reg == to_atu_outb_reg(1, PCIE_ATU_UNR_LOWER_BASE)) ||
					(reg == to_atu_outb_reg(2, PCIE_ATU_UNR_LOWER_BASE)) ||
					(reg == to_atu_outb_reg(0, PCIE_ATU_UNR_LOWER_LIMIT)) ||
					(reg == to_atu_outb_reg(1, PCIE_ATU_UNR_LOWER_LIMIT)) ||
					(reg == to_atu_outb_reg(2, PCIE_ATU_UNR_LOWER_LIMIT))) {
				write_val &= mask;
			}

			/*
			 * REMOVED: Forcing UPPER_BASE to 0 breaks >4GB addresses
			 * (e.g., addr_space at 0x500000000). Keep UPPER registers as-is.
			 *
			 * if ((reg == to_atu_outb_reg(0, PCIE_ATU_UNR_UPPER_BASE)) || ...
			 *     write_val = 0x0U;
			 */
		} else {
			mask = 0x00FFFFFFU;
			if ((reg == (u32)PCIE_ATU_LOWER_BASE) ||
					(reg == (u32)PCIE_ATU_LIMIT)) {
				write_val &= mask;
			}
		}

		if (!__builtin_add_overflow(size, 0, &sz)) {
			if (dw_pcie_write(base + reg, sz, write_val) != 0) {
				dev_err(pci->dev, "failed to write dbi register(0x%08x)\n", reg);
			}
		}
	}
}

static void tcc_pcie_set_dbi2_mode(const struct tcc_pcie *tp, bool enable)
{
	if (tp != NULL) {
		if ((tp->variant == TCC805X) || (tp->variant == TCC807X)) {
			u32 val, mask;

			if (enable) {
				val = PCIE_LINK_CFG_INDIRECT_ADDR_MASK;
				mask = 0x0U;
				tcc_pcie_writel(tp->link_base, PCIE_DBI_INDIRECT_ADDR_MASK,
						val, mask);
			} else {
				val = 0x0U;
				mask = PCIE_LINK_CFG_INDIRECT_ADDR_MASK;
				tcc_pcie_writel(tp->link_base, PCIE_DBI_INDIRECT_ADDR_MASK,
						val, mask);
			}
		}
	}
}

static void tcc_pcie_write_dbi2(struct dw_pcie *pci, void __iomem *base, u32 reg,
		size_t size, u32 val)
{
	if (pci != NULL) {
		const struct tcc_pcie *tp = (const struct tcc_pcie *)to_tcc_pcie(pci);
		s32 sz, err;

		err = 0;
		if (!__builtin_add_overflow(size, 0, &sz)) {
			if ((tp->variant == TCC805X) || (tp->variant == TCC807X)) {
				reg |= PCIE_LINK_CFG_INDIRECT_ADDR_MASK;
				tcc_pcie_set_dbi2_mode(tp, true);
				err = dw_pcie_write(base + reg, sz, val);
				tcc_pcie_set_dbi2_mode(tp, false);
			} else {
				err = dw_pcie_write(base + reg, sz, val);
			}
		}

		if (err != 0) {
			dev_err(pci->dev, "failed to write dbi2 register(%d)\n", err);
		}
	}
}

static s32 tcc_pcie_link_up(struct dw_pcie *pci)
{
	s32 err = (s32)PCIE_LINK_DOWN;

	if (pci != NULL) {
		const struct tcc_pcie *tp = (const struct tcc_pcie *)to_tcc_pcie(pci);
		u32 val;

		val = tcc_pcie_readl(tp->link_base, PCIE_RDLH_LINK_STS);
		if ((val & PCIE_LINK_CFG_RDLH_LINK_UP_MASK) != 0x0U) {
			val = tcc_pcie_readl(tp->link_base, PCIE_SMLH_LINK_STS);
			if ((val & PCIE_LINK_CFG_SMLH_LINK_UP_MASK) != 0x0U) {
				err = (s32)PCIE_LINK_UP;
			}
		}
	}

	return err;
}

static s32 tcc_pcie_check_link_up(const struct tcc_pcie *tp)
{
	s32 err = 0;

	if (tp != NULL) {
		struct dw_pcie *pci = tp->pci;
		struct timespec64 start, end, gap;

		ktime_get_ts64(&start);
		do {
			err = tcc_pcie_link_up(pci);
			ktime_get_ts64(&end);
			gap = timespec64_sub(end, start);
			if ((timespec64_to_ns(&gap)/NSEC_PER_MSEC) > PCIE_LINK_UP_TIMEOUT) {
				err = -EAGAIN;
				break;
			} else {
				mdelay(PCIE_LINK_UP_DELAY);
			}
		} while (err != (s32)PCIE_LINK_UP);
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_establish_link(struct dw_pcie *pci)
{
	s32 err = 0;

	if (pci != NULL) {
		const struct tcc_pcie *tp = (const struct tcc_pcie *)to_tcc_pcie(pci);

		err = tcc_pcie_enable_ltssm(tp, true);
		if (err == 0) {
			if (tcc_pcie_check_link_up(tp) == (s32)PCIE_LINK_UP) {
				dev_err(pci->dev, "Link up\n");
			} else {
				dev_err(pci->dev, "timout waiting for link_up\n");
			}
		}

		if (err == 0) {
			err = tcc_pcie_set_interrupt(tp, true);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static void	tcc_pcie_stop_link(struct dw_pcie *pci)
{
	if (pci != NULL) {
		const struct tcc_pcie *tp = (const struct tcc_pcie *)to_tcc_pcie(pci);

		if (tcc_pcie_enable_ltssm(tp, false) != 0) {
			dev_err(pci->dev, "failed to disable ltssm\n");
		}
	}
}

static const struct dw_pcie_ops tcc_pcie_host_ops = {
	.write_dbi = tcc_pcie_write_dbi,
	.write_dbi2 = tcc_pcie_write_dbi2,
	.start_link = tcc_pcie_establish_link,
	.stop_link = tcc_pcie_stop_link,
	.link_up = tcc_pcie_link_up,
};

static s32 tcc_pcie_host_init(struct pcie_port *pp)
{
	s32 err = 0;

	if (pp != NULL) {
		struct dw_pcie *pci = to_dw_pcie_from_pp(pp);

		dw_pcie_setup_rc(pp);

		err = pinctrl_pm_select_default_state(pci->dev);
		if (err == 0) {
			err = tcc_pcie_establish_link(pci);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static void tcc_pcie_set_num_vectors(struct pcie_port *pp)
{
	if (pp != NULL) {
		pp->num_vectors = MAX_MSI_IRQS;
	}
}

static const struct dw_pcie_host_ops tcc_pcie_rc_ops = {
	.host_init = tcc_pcie_host_init,
	.set_num_vectors = tcc_pcie_set_num_vectors,
};

/* MSI int handler */
static irqreturn_t tcc_pcie_handle_msi_irq(struct pcie_port *pp)
{
	irqreturn_t ret = IRQ_NONE;

	if (pp != NULL) {
		struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
		unsigned long status, pos;
		irq_hw_number_t hwirq;
		u32 idx;

		pos = (unsigned long)0;
		for (idx = 0; idx < (pp->num_vectors/(u32)MAX_MSI_IRQS_PER_CTRL); idx++) {
			status = dw_pcie_readl_dbi(pci,
					(u32)PCIE_MSI_INTR0_STATUS + (idx * (u32)MSI_REG_CTRL_BLOCK_SIZE));
			if (status != 0x0UL) {
				ret = IRQ_HANDLED;

				do {
					pos = find_next_bit(&status, (unsigned long)MAX_MSI_IRQS_PER_CTRL, pos);
					if (pos != (unsigned long)MAX_MSI_IRQS_PER_CTRL) {
						if (!__builtin_mul_overflow(idx, MAX_MSI_IRQS_PER_CTRL, &hwirq)) {
							if (!__builtin_add_overflow(hwirq, pos, &hwirq)) {
								unsigned int irq = irq_find_mapping(pp->irq_domain, hwirq);

								(void)generic_handle_irq(irq);
							}
						}
						pos++;
					}
				} while (pos != (unsigned long)MAX_MSI_IRQS_PER_CTRL);
			}
		}
	}
	return ret;
}

static irqreturn_t tcc_pcie_irq_handler(s32 irq, void *arg)
{
	const struct tcc_pcie *tp = (const struct tcc_pcie *)arg;
	irqreturn_t ret = IRQ_HANDLED;

	(void)irq;

	if (tp != NULL) {
		struct dw_pcie *pci = tp->pci;
		struct pcie_port *pp = &pci->pp;
		u32 val, mask;

		mask = PCIE_LINK_CFG_INTX_MASK | PCIE_LINK_CFG_MSI_INT_MASK;
		val = tcc_pcie_readl(tp->link_base, PCIE_LINK_STS) & mask;
		if (val != 0x0U) {
			if (IS_ENABLED(CONFIG_PCI_MSI) != 0) {
				if ((tp->mode == DW_PCIE_RC_TYPE) &&
						((val & PCIE_LINK_CFG_MSI_INT_MASK) != 0x0U)) {
					ret = tcc_pcie_handle_msi_irq(pp);
				}
			}

			tcc_pcie_writel(tp->link_base, PCIE_LINK_STS_CLR,
					val, mask);
		} else {
			ret = IRQ_NONE;
		}
	}

	return ret;
}

static s32 tcc_pcie_prepare_rc(struct tcc_pcie *tp, struct platform_device *pdev,
		struct pcie_port *pp)
{
	s32 err = 0;

	if ((tp != NULL) &&
			(pdev != NULL) &&
			(pp != NULL)) {
		pp->ops = &tcc_pcie_rc_ops;
		pp->irq = platform_get_irq(pdev, 0U);
		if (pp->irq > 0) {
			err = devm_request_irq(&pdev->dev,
					(u32)pp->irq, tcc_pcie_irq_handler,
					IRQF_SHARED, "telechips-pcie-rc",
					tp);
		} else {
			err = -ENODEV;
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_deassert_pwrup_reset(const struct tcc_pcie *tp)
{
	s32 err = 0;

	if (tp != NULL) {
		if (tp->variant != TCC803X) {
			u32 val, mask;

			/* deassert power up reset */
			mask = PCIE_LINK_CFG_POWER_UP_RST_MASK;
			val = mask;
			tcc_pcie_writel(tp->link_base, PCIE_PHY_RST_CTRL, val, mask);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_wake_up_phy(const struct tcc_pcie *tp)
{
	s32 err = 0;

	if (tp != NULL) {
		const struct dw_pcie *pci = (const struct dw_pcie *)tp->pci;

		err = phy_power_on(tp->phy);
		if (err == 0) {
			err = pinctrl_pm_select_idle_state(pci->dev);
		}

		if (err == 0) {
			err = tcc_pcie_clear_interrupts(tp);
		}

		if (err == 0) {
			err = tcc_pcie_deassert_pwrup_reset(tp);
		}

		if (err == 0) {
			err = phy_reset(tp->phy);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_set_clksrc(const struct tcc_pcie *tp)
{
	s32 err = 0;

	if (tp != NULL) {
		if ((tp->variant == TCC803X) || (tp->variant == TCC807X)) {
			u32 val, mask;

			mask = PCIE_LINK_CFG_REFCLK_EXT_EN_MASK | PCIE_LINK_CFG_PHY_REFCLK_SEL_MASK;
			val = (tp->refclk_type == 0x0U) ? PCIE_LINK_CFG_REFCLK_EXT_EN_MASK : 0x0U;
			val |= ((u32)((tp->refclk_type == 0x0U) ? REFCLK_SRC_PLL : REFCLK_SRC_IO)
					<< PCIE_LINK_CFG_PHY_REFCLK_SEL_SHIFT);
			tcc_pcie_writel(tp->link_base, PCIE_PHY_CTRL, val, mask);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_set_clk(const struct tcc_pcie *tp)
{
	s32 err = 0;

	if (tp != NULL) {
		err = tcc_pcie_set_clksrc(tp);
		if (err == 0) {
			s32 submode;

			if (!__builtin_add_overflow(to_phy_submode(tp->refclk_type, tp->mode), 0, &submode)) {
				err = phy_set_clk(tp->phy, submode);
			}
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

#ifdef CONFIG_PCIE_DW_EP
static irqreturn_t tcc_pcie_edma_irq_handler(s32 irq, void *arg)
{
	struct tcc_pcie *tp = (struct tcc_pcie *)arg;
	irqreturn_t ret = (irqreturn_t)IRQ_NONE;

	(void)irq;

	if (tp != NULL) {
		struct dw_pcie *pci = tp->pci;
		u32 reg, val, status;

		status = tcc_pcie_readl(tp->link_base, PCIE_EDMA_INTR_STS);
		if((status & PCIE_LINK_CFG_EDMA_INTR_STS_MASK) != 0x0U) {
			reg = ((status & PCIE_LINK_CFG_EDMA_WR_INTR_STS_MASK) != 0x0U) ?
				DBI_PCIE_DMA_WRITE_INT_STATUS : DBI_PCIE_DMA_READ_INT_STATUS;

			val = dw_pcie_readl_dbi(pci, reg);
			reg = ((status & PCIE_LINK_CFG_EDMA_WR_INTR_STS_MASK) != 0x0U) ?
				DBI_PCIE_DMA_WRITE_INT_CLEAR : DBI_PCIE_DMA_READ_INT_CLEAR;
			if ((val & PCIE_DBI_DMA_DONE_INT_STATUS_MASK) != 0x0U) {
				if (atomic_read(&tp->edma->cond) == 0) {
					atomic_set(&tp->edma->cond, 1);
				}
			}
			dw_pcie_writel_dbi(pci, reg, val);

			ret = (irqreturn_t)IRQ_HANDLED;
		}
	}

	return ret;
}

static s32 tcc_pcie_init_edma(struct tcc_pcie *tp, struct platform_device *pdev)
{
	s32 err = 0;

	if ((tp != NULL) && (pdev != NULL)) {
		if (tp->variant != TCC803X) {
			tp->edma = devm_kzalloc(&pdev->dev,
					sizeof(struct tcc_pcie_edma), GFP_KERNEL);
			if (tp->edma != NULL) {
				tp->edma->irq = platform_get_irq(pdev, 1);
				if (tp->edma->irq > 0) {
					err = devm_request_irq(&pdev->dev, (u32)tp->edma->irq,
							tcc_pcie_edma_irq_handler, IRQF_SHARED,
							"telechips-pcie-edma", tp);
				}
			}
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 dw_edma_async_transfer_program(struct dw_pcie *pci, enum edma_data_direction dir, u64 src, u64 dst, u32 size)
{
	s32 err = 0;

	if (pci != NULL) {
		struct tcc_pcie *tp = to_tcc_pcie(pci);
		u32 reg, val, mask;

		mask = PCIE_DBI_DMA_CS_MASK;
		reg = (dir == EDMA_TO_PNR) ?
			DBI_PCIE_DMA_WR_CH_CONTROL1 : DBI_PCIE_DMA_RD_CH_CONTROL1;
		val = dw_pcie_readl_dbi(pci, reg) & mask;
		if (val != PCIE_DBI_DMA_CS_RUN_MASK) {
			mask = PCIE_LINK_CFG_EDMA_INTR_EN_MASK;
			val = tcc_pcie_readl(tp->link_base, PCIE_EDMA_INTR_EN) & ~mask;
			val |= (dir == EDMA_TO_PNR) ?
				PCIE_LINK_CFG_EDMA_WR_INTR_EN_MASK : PCIE_LINK_CFG_EDMA_RD_INTR_EN_MASK;
			tcc_pcie_writel(tp->link_base, PCIE_EDMA_INTR_EN, val, mask);

			atomic_set(&tp->edma->cond, 0);
			dw_pcie_writel_dbi(pci, DBI_PCIE_DMA_CTRL_DATA_ARB_PRIOR, 0x608U);
			if (dir == EDMA_TO_PNR) {
				/* Write Channel0 */
				dw_pcie_writel_dbi(pci, DBI_PCIE_DMA_WRITE_ENGINE_EN, 0x1U);
				dw_pcie_writel_dbi(pci, DBI_PCIE_DMA_WRITE_CHANNEL_ARB_WEIGHT_LOW, 0x20C41U);
				dw_pcie_writel_dbi(pci, DBI_PCIE_DMA_WRITE_INT_MASK, 0x0U);
				dw_pcie_writel_dbi(pci, DBI_PCIE_DMA_WR_CH_CONTROL1, 0x04000018U);
				dw_pcie_writel_dbi(pci, DBI_PCIE_DMA_WR_TRANSFER_SIZE, size);
				dw_pcie_writel_dbi(pci, DBI_PCIE_DMA_WR_SAR_LOW, lower_32_bits(src));
				dw_pcie_writel_dbi(pci, DBI_PCIE_DMA_WR_SAR_HIGH, upper_32_bits(src));
				dw_pcie_writel_dbi(pci, DBI_PCIE_DMA_WR_DAR_LOW, lower_32_bits(dst));
				dw_pcie_writel_dbi(pci, DBI_PCIE_DMA_WR_DAR_HIGH, upper_32_bits(dst));
				dw_pcie_writel_dbi(pci, DBI_PCIE_DMA_WRITE_DOORBELL, 0x0U);
			} else {
				/* Read Channel0 */
				dw_pcie_writel_dbi(pci, DBI_PCIE_DMA_READ_ENGINE_EN, 0x1U);
				dw_pcie_writel_dbi(pci, DBI_PCIE_DMA_READ_CHANNEL_ARB_WEIGHT_LOW, 0x20C41U);
				dw_pcie_writel_dbi(pci, DBI_PCIE_DMA_READ_INT_MASK, 0x0U);
				dw_pcie_writel_dbi(pci, DBI_PCIE_DMA_RD_CH_CONTROL1, 0x04000018U);
				dw_pcie_writel_dbi(pci, DBI_PCIE_DMA_RD_TRANSFER_SIZE, size);
				dw_pcie_writel_dbi(pci, DBI_PCIE_DMA_RD_SAR_LOW, lower_32_bits(src));
				dw_pcie_writel_dbi(pci, DBI_PCIE_DMA_RD_SAR_HIGH, upper_32_bits(src));
				dw_pcie_writel_dbi(pci, DBI_PCIE_DMA_RD_DAR_LOW, lower_32_bits(dst));
				dw_pcie_writel_dbi(pci, DBI_PCIE_DMA_RD_DAR_HIGH, upper_32_bits(dst));
				dw_pcie_writel_dbi(pci, DBI_PCIE_DMA_READ_DOORBELL, 0x0U);
			}
		} else {
			err = -EAGAIN;
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 dw_edma_async_transfer_complete(struct tcc_pcie *tp)
{
	s32 err = 0;

	if (tp != NULL) {
		struct timespec64 start, end, gap;

		ktime_get_ts64(&start);
		while(atomic_read(&tp->edma->cond) == 0) {
			ktime_get_ts64(&end);
			gap = timespec64_sub(end, start);
			if ((timespec64_to_ns(&gap)/NSEC_PER_MSEC) > 1000LL) {
				err = -EAGAIN;
				break;
			} else {
				udelay(1);
			}
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

s32 dw_edma_async_transfer(void *arg, enum edma_data_direction dir,
		u64 src, u64 dst, u32 sz, bool pending)
{
	s32 err = 0;

	if (arg != NULL) {
		struct dw_pcie_ep *ep = epc_get_drvdata((struct pci_epc *)arg);
		struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
		struct tcc_pcie *tp = to_tcc_pcie(pci);

		if (valid_edma_direction(dir) != 0) {
			err = dw_edma_async_transfer_program(pci, dir, src, dst, sz);
			if ((err == 0) && pending) {
				err = dw_edma_async_transfer_complete(tp);
			}
		} else {
			err = -EINVAL;
		}
	} else {
		err = -EINVAL;
	}

	return err;
}
EXPORT_SYMBOL_GPL(dw_edma_async_transfer);

static s32 tcc_pcie_ep_reset_bar(struct dw_pcie *pci, s32 bar)
{
	s32 err = 0;

	if ((pci != NULL) &&
			(bar <= (s32)BAR_5)) {
		u32 reg;

		if (!__builtin_add_overflow(PCI_BASE_ADDRESS_0, (0x4 * bar), &reg)) {
			dw_pcie_dbi_ro_wr_en(pci);
			tcc_pcie_write_dbi2(pci, pci->dbi_base2, reg, SZ_4, 0x0U);
			dw_pcie_writel_dbi(pci, reg, 0x0U);
			dw_pcie_dbi_ro_wr_dis(pci);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_ep_get_iatu_unroll_support(struct dw_pcie *pci)
{
	s32 err = 0;

	if (pci != NULL) {
		u32 val;

		val = dw_pcie_readl_dbi(pci, PCIE_ATU_VIEWPORT);
		if (val == 0xFFFFFFFFU) {
			pci->iatu_unroll_enabled = 1U;
		} else {
			pci->iatu_unroll_enabled = 0U;
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static const struct pci_epc_features tcc_pcie_epc_features = {
	.linkup_notifier = 0x0U,
	.msi_capable = 0x1U,
	.msix_capable = 0x1U,
	.align = SZ_64K,
};

static const struct pci_epc_features* tcc_pcie_get_features(struct dw_pcie_ep *ep)
{
	(void)ep;

	return &tcc_pcie_epc_features;
}

static void tcc_pcie_ep_init(struct dw_pcie_ep *ep)
{
	if (ep != NULL) {
		struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
		s32 bar, err;

		for (bar = (s32)BAR_0; bar <= (s32)BAR_5; bar++) {
			err = tcc_pcie_ep_reset_bar(pci, bar);
			if (err != 0) {
				break;
			}
		}

		if (err == 0) {
			err = pinctrl_pm_select_default_state(pci->dev);
		}
	}
}

/* migrated, need to remove */
#define CALC_PCIE_REG_OFFSET(b, o, e)	((u32)(b) + (u32)(o) + (u32)(e))
#define CALC_LIMIT_ADDR(b, o)		((u64)(b) + (u64)(o))
static u32 tcc_pcie_readl_atu(struct dw_pcie *pci, u32 reg)
{
	u32 val = 0;

	if (pci != NULL) {
		if (pci->ops->read_dbi != NULL) {
			val = pci->ops->read_dbi(pci, pci->atu_base, reg, 4);
		} else {
			if (dw_pcie_read(pci->atu_base + reg, 4, &val) != 0) {
				dev_err(pci->dev, "Read ATU address failed\n");
			}
		}
	}

	return val;
}

static void tcc_pcie_writel_atu(struct dw_pcie *pci, u32 reg, u32 val)
{
	if (pci != NULL) {
		if (pci->ops->write_dbi != NULL) {
			pci->ops->write_dbi(pci, pci->atu_base, reg, 4, val);
		} else {
			if (dw_pcie_write(pci->atu_base + reg, 4, val) != 0) {
				dev_err(pci->dev, "Write ATU address failed\n");
			}
		}
	}
}

static void tcc_pcie_prog_outbound_atu_unroll(struct dw_pcie *pci, u8 func_no,
		u32 index, u32 type,
		u64 cpu_addr, u64 pci_addr,
		size_t size)
{
	if (pci != NULL) {
		const struct dw_pcie_ep *ep = (const struct dw_pcie_ep *)&pci->ep;

		if (index < ep->num_ob_windows) {
			u32 retries, val;
			u32 offset = PCIE_GET_ATU_OUTB_UNR_REG_OFFSET(index);

			if (!__builtin_sub_overflow(size, 1, &size)) {
				u64 limit_addr = CALC_LIMIT_ADDR(cpu_addr, size);

				tcc_pcie_writel_atu(pci,
						CALC_PCIE_REG_OFFSET(offset, PCIE_ATU_UNR_LOWER_BASE, 0),
						lower_32_bits(cpu_addr));
				tcc_pcie_writel_atu(pci,
						CALC_PCIE_REG_OFFSET(offset, PCIE_ATU_UNR_UPPER_BASE, 0),
						upper_32_bits(cpu_addr));
				tcc_pcie_writel_atu(pci,
						CALC_PCIE_REG_OFFSET(offset, PCIE_ATU_UNR_LOWER_LIMIT, 0),
						lower_32_bits(limit_addr));
				tcc_pcie_writel_atu(pci,
						CALC_PCIE_REG_OFFSET(offset, PCIE_ATU_UNR_UPPER_LIMIT, 0),
						upper_32_bits(limit_addr));
				tcc_pcie_writel_atu(pci,
						CALC_PCIE_REG_OFFSET(offset, PCIE_ATU_UNR_LOWER_TARGET, 0),
						lower_32_bits(pci_addr));
				tcc_pcie_writel_atu(pci,
						CALC_PCIE_REG_OFFSET(offset, PCIE_ATU_UNR_UPPER_TARGET, 0),
						upper_32_bits(pci_addr));
				tcc_pcie_writel_atu(pci,
						CALC_PCIE_REG_OFFSET(offset, PCIE_ATU_UNR_REGION_CTRL1, 0),
						type | PCIE_ATU_FUNC_NUM(func_no));
				tcc_pcie_writel_atu(pci,
						CALC_PCIE_REG_OFFSET(offset, PCIE_ATU_UNR_REGION_CTRL2, 0),
						PCIE_ATU_ENABLE);

				/*
				 * Make sure ATU enable takes effect before any subsequent config
				 * and I/O accesses.
				 */
				for (retries = 0; retries < (u32)LINK_WAIT_MAX_IATU_RETRIES; retries++) {
					val = tcc_pcie_readl_atu(pci,
							CALC_PCIE_REG_OFFSET(offset, PCIE_ATU_UNR_REGION_CTRL2, 0));
					if ((val & PCIE_ATU_ENABLE) != 0x0U) {
						break;
					} else {
						mdelay(LINK_WAIT_IATU);
					}
				}

				if (retries == (u32)LINK_WAIT_MAX_IATU_RETRIES) {
					dev_err(pci->dev, "Outbound iATU is not being enabled\n");
				}
			} else {
				dev_err(pci->dev, "Invalid Size\n");
			}
		}
	}
}

static void tcc_pcie_prog_outbound_atu_no_unroll(struct dw_pcie *pci, u8 func_no, u32 index,
		u32 type, u64 cpu_addr, u64 pci_addr,
		size_t size)
{
	if (pci != NULL) {
		const struct dw_pcie_ep *ep = (const struct dw_pcie_ep *)&pci->ep;

		if (index < ep->num_ob_windows) {
			if (!__builtin_sub_overflow(size, 1, &size)) {
				u64 limit_addr = CALC_LIMIT_ADDR(cpu_addr, size);
				u32 retries, val;

				val = ((u32)PCIE_ATU_REGION_OUTBOUND | index);
				dw_pcie_writel_dbi(pci, PCIE_ATU_VIEWPORT, val);
				dw_pcie_writel_dbi(pci, PCIE_ATU_LOWER_BASE, lower_32_bits(cpu_addr));
				dw_pcie_writel_dbi(pci, PCIE_ATU_UPPER_BASE, upper_32_bits(cpu_addr));
				dw_pcie_writel_dbi(pci, PCIE_ATU_LIMIT, lower_32_bits(limit_addr));
				dw_pcie_writel_dbi(pci, PCIE_ATU_LOWER_TARGET, lower_32_bits(pci_addr));
				dw_pcie_writel_dbi(pci, PCIE_ATU_UPPER_TARGET, upper_32_bits(pci_addr));
				dw_pcie_writel_dbi(pci, PCIE_ATU_CR1, type | PCIE_ATU_FUNC_NUM((u32)func_no));
				dw_pcie_writel_dbi(pci, PCIE_ATU_CR2, (u32)PCIE_ATU_ENABLE);

				/*
				 * Make sure ATU enable takes effect before any subsequent config
				 * and I/O accesses.
				 */
				for (retries = 0; retries < (u32)LINK_WAIT_MAX_IATU_RETRIES; retries++) {
					val = dw_pcie_readl_dbi(pci, (u32)PCIE_ATU_CR2);
					if ((val & (u32)PCIE_ATU_ENABLE) != 0x0U) {
						break;
					}

					mdelay(LINK_WAIT_IATU);
				}

				if (retries == (u32)LINK_WAIT_MAX_IATU_RETRIES) {
					dev_err(pci->dev, "Outbound iATU is not being enabled\n");
				}
			} else {
				dev_err(pci->dev, "Invalid Size\n");
			}
		}
	}
}

static void tcc_pcie_prog_outbound_atu(struct dw_pcie *pci, u8 func_no, u32 index,
		u32 type, u64 cpu_addr, u64 pci_addr,
		size_t size)
{
	if (pci != NULL) {
		if (pci->iatu_unroll_enabled != 0U) {
			tcc_pcie_prog_outbound_atu_unroll(pci, func_no, index, type,
					cpu_addr, pci_addr, size);
		} else {
			tcc_pcie_prog_outbound_atu_no_unroll(pci, func_no, index, type,
					cpu_addr, pci_addr, size);
		}
	}
}

static int tcc_pcie_ep_outbound_atu(struct dw_pcie_ep *ep, u8 func_no,
		phys_addr_t phys_addr,
		u64 pci_addr, size_t size)
{
	s32 err = 0;

	if (ep != NULL) {
		struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
		unsigned long free_win;

		free_win = find_first_zero_bit(ep->ob_window_map, ep->num_ob_windows);
		if (free_win >= ep->num_ob_windows) {
			dev_err(pci->dev, "No free outbound window\n");
			err = -EINVAL;
		} else {
			tcc_pcie_prog_outbound_atu(pci, func_no, (u32)free_win, PCIE_ATU_TYPE_MEM,
					phys_addr, pci_addr, size);

			set_bit((unsigned int)free_win, ep->ob_window_map);
			ep->outbound_addr[free_win] = phys_addr;
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_ep_map_addr(struct pci_epc *epc, u8 func_no,
		phys_addr_t addr,
		u64 pci_addr, size_t size)
{
	s32 err = 0;

	if (epc != NULL) {
		struct dw_pcie_ep *ep = (struct dw_pcie_ep *)epc_get_drvdata(epc);
		struct dw_pcie *pci = to_dw_pcie_from_ep(ep);

		err = tcc_pcie_ep_outbound_atu(ep, func_no, addr, pci_addr, size);
		if (err != 0) {
			dev_err(pci->dev, "Failed to enable address\n");
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static void tcc_pcie_disable_atu(struct dw_pcie *pci, u32 index,
		enum dw_pcie_region_type type)
{
	if (pci != NULL) {
		if ((type == DW_PCIE_REGION_INBOUND) ||
				(type == DW_PCIE_REGION_OUTBOUND)) {
			u32 region = (type == DW_PCIE_REGION_INBOUND) ?
				PCIE_ATU_REGION_INBOUND : PCIE_ATU_REGION_OUTBOUND;

			dw_pcie_writel_dbi(pci, PCIE_ATU_VIEWPORT, region | index);
			dw_pcie_writel_dbi(pci, PCIE_ATU_CR2, ~(u32)PCIE_ATU_ENABLE);
		}
	}
}

static int tcc_pcie_find_index(const struct dw_pcie_ep *ep, phys_addr_t addr,
		u32 *atu_index)
{
	s32 err = 0;

	if (ep != NULL) {
		u32 idx;

		for (idx = 0; idx < ep->num_ob_windows; idx++) {
			if (ep->outbound_addr[idx] != addr) {
				continue;
			} else {
				*atu_index = idx;
				break;
			}
		}

		if (idx == ep->num_ob_windows) {
			err = -EINVAL;
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static void tcc_pcie_ep_unmap_addr(struct pci_epc *epc, u8 func_no,
		phys_addr_t addr)
{
	(void)func_no;

	if (epc != NULL) {
		const struct dw_pcie_ep *ep =
			(const struct dw_pcie_ep *)epc_get_drvdata(epc);
		struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
		u32 idx = 0U;

		if (tcc_pcie_find_index(ep, addr, &idx) == 0) {
			tcc_pcie_disable_atu(pci, idx, DW_PCIE_REGION_OUTBOUND);
			clear_bit(idx, ep->ob_window_map);
		}
	}
}

static unsigned int tcc_pcie_ep_func_select(struct dw_pcie_ep *ep, u8 func_no)
{
	unsigned int func_offset = (unsigned int)0;

	if (ep->ops->func_conf_select != NULL) {
		func_offset = ep->ops->func_conf_select(ep, func_no);
	}

	return func_offset;
}

static struct dw_pcie_ep_func *tcc_pcie_ep_get_func_from_ep(const struct dw_pcie_ep *ep, u8 func_no)
{
	struct dw_pcie_ep_func *ep_func = NULL;

	if (ep != NULL) {
		struct dw_pcie_ep_func *tmp;

		list_for_each_entry(tmp, &ep->func_list, list) {
			if (tmp->func_no == func_no) {
				ep_func = tmp;
				break;
			}
		}
	}

	return ep_func;
}

static s32 tcc_pcie_ep_get_msi_addr(struct dw_pcie_ep *ep, u8 func_no,
		u64 *msg_addr, u16 *msg_data, unsigned int *aligned_offset)
{
	s32 err = 0;

	if (ep != NULL) {
		struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
		const struct dw_pcie_ep_func *ep_func =
			(const struct dw_pcie_ep_func *)tcc_pcie_ep_get_func_from_ep(
					(const struct dw_pcie_ep *)ep, func_no);
		const struct pci_epc *epc = (const struct pci_epc *)ep->epc;

		if ((ep_func == NULL) || (ep_func->msi_cap == (u8)0)) {
			err = -EINVAL;
		} else {
			unsigned int func_offset;
			u32 msg_addr_lower, msg_addr_upper, reg;
			u32 offset, mask;
			u16 msg_ctrl;
			bool has_upper;

			func_offset = tcc_pcie_ep_func_select(ep, func_no);
			if (!__builtin_add_overflow(func_offset, 0, &offset)) {
				/* Raise MSI per the PCI Local Bus Specification Revision 3.0, 6.8.1. */
				reg = CALC_PCIE_REG_OFFSET(ep_func->msi_cap, offset, PCI_MSI_FLAGS);
				msg_ctrl = dw_pcie_readw_dbi(pci, reg);
				has_upper = ((msg_ctrl & (u16)PCI_MSI_FLAGS_64BIT) != 0x0U) ? true : false;

				reg = CALC_PCIE_REG_OFFSET(ep_func->msi_cap, offset, PCI_MSI_ADDRESS_LO);
				msg_addr_lower = dw_pcie_readl_dbi(pci, reg);
				if (has_upper) {
					reg = CALC_PCIE_REG_OFFSET(ep_func->msi_cap, offset, PCI_MSI_ADDRESS_HI);
					msg_addr_upper = dw_pcie_readl_dbi(pci, reg);
					reg = CALC_PCIE_REG_OFFSET(ep_func->msi_cap, offset, PCI_MSI_DATA_64);
					*msg_data = dw_pcie_readw_dbi(pci, reg);
				} else {
					msg_addr_upper = (u32)0;
					reg = CALC_PCIE_REG_OFFSET(ep_func->msi_cap, offset, PCI_MSI_DATA_32);
					*msg_data = dw_pcie_readw_dbi(pci, reg);
				}

				if (!__builtin_sub_overflow(epc->mem->window.page_size, 1, &mask)) {
					*aligned_offset = msg_addr_lower & mask;
					*msg_addr = ((u64)msg_addr_upper << 32) | (msg_addr_lower & ~(*aligned_offset));
				} else {
					err = -EINVAL;
				}
			} else {
				err = -EINVAL;
			}
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_ep_raise_legacy_irq(struct dw_pcie_ep *ep, u8 func_no)
{
	s32 err = 0;

	(void)func_no;

	if (ep != NULL) {
		struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
		const struct tcc_pcie *tp = (const struct tcc_pcie *)to_tcc_pcie(pci);
		u32 val, mask;

		mask = PCIE_LINK_CFG_SYS_INT_MASK;
		val = mask;
		tcc_pcie_writel(tp->link_base, PCIE_SYS_INT, val, mask);

		val = 0x0U;
		tcc_pcie_writel(tp->link_base, PCIE_SYS_INT, val, mask);
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_ep_raise_msi_irq(struct dw_pcie_ep *ep, u8 func_no,
		u8 interrupt_num)
{
	s32 err = 0;

	if (ep != NULL) {
		struct pci_epc *epc = ep->epc;
		unsigned int aligned_offset;
		u64 msg_addr;
		u16 msg_data;

		err = tcc_pcie_ep_get_msi_addr(ep, func_no,
				&msg_addr, &msg_data, &aligned_offset);
		if (err == 0) {
			u32 intr_num;

			err = tcc_pcie_ep_map_addr(epc, func_no, ep->msi_mem_phys, msg_addr,
					epc->mem->window.page_size);
			if (err == 0) {
				if (!__builtin_sub_overflow(interrupt_num, 1, &intr_num)) {
					writel(((u32)msg_data | intr_num), ep->msi_mem + aligned_offset);
					tcc_pcie_ep_unmap_addr(epc, func_no, ep->msi_mem_phys);
				} else {
					err = -EINVAL;
				}
			}
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static struct pci_epf_msix_tbl *tcc_pcie_ep_get_msix_table(struct dw_pcie_ep *ep, u8 func_no)
{
	struct pci_epf_msix_tbl *msix_tbl = NULL;

	if (ep != NULL) {
		struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
		const struct dw_pcie_ep_func *ep_func;

		ep_func = (const struct dw_pcie_ep_func *)tcc_pcie_ep_get_func_from_ep(ep, func_no);
		if ((ep_func != NULL) && (ep_func->msix_cap != (u8)0)) {
			unsigned int func_offset;
			u32 reg, tbl_offset, offset;

			func_offset = tcc_pcie_ep_func_select(ep, func_no);
			if (!__builtin_add_overflow(func_offset, PCI_MSIX_TABLE, &offset)) {
				reg = CALC_PCIE_REG_OFFSET(ep_func->msix_cap, offset, 0);
				tbl_offset = dw_pcie_readl_dbi(pci, reg);
				tbl_offset &= (u32)PCI_MSIX_TABLE_OFFSET;
				msix_tbl = ep->epf_bar[tbl_offset & (u32)PCI_MSIX_TABLE_BIR]->addr + tbl_offset;
			}
		}
	}

	return msix_tbl;
}

static s32 tcc_pcie_ep_raise_msix_irq(struct dw_pcie_ep *ep, u8 func_no,
		u16 interrupt_num)
{
	s32 err = 0;

	if (ep != NULL) {
		const struct pci_epf_msix_tbl *msix_tbl;

		msix_tbl = (const struct pci_epf_msix_tbl *)tcc_pcie_ep_get_msix_table(ep, func_no);
		if (msix_tbl != NULL) {
			u32 msg_data, vec_ctrl, intr_num;
			u64 msg_addr;

			if (!__builtin_sub_overflow(interrupt_num, 1, &intr_num)) {
				msg_addr = msix_tbl[intr_num].msg_addr;
				msg_data = msix_tbl[intr_num].msg_data;
				vec_ctrl = msix_tbl[intr_num].vector_ctrl;

				if ((vec_ctrl & (u32)PCI_MSIX_ENTRY_CTRL_MASKBIT) != 0x0U) {
					err = -EPERM;
				} else {
					struct pci_epc *epc = ep->epc;

					err = tcc_pcie_ep_map_addr(epc,
							func_no,
							ep->msi_mem_phys,
							msg_addr,
							epc->mem->window.page_size);
					if (err == 0) {
						u32 aligned_offset;
						u64 mask;

						if (!__builtin_sub_overflow(epc->mem->window.page_size, 1, &mask)) {
							aligned_offset = lower_32_bits(msg_addr & mask);
							writel(msg_data, ep->msi_mem + aligned_offset);

							tcc_pcie_ep_unmap_addr(epc, func_no, ep->msi_mem_phys);
						} else {
							err = -EINVAL;
						}
					}
				}
			} else {
				err = -EINVAL;
			}
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_ep_raise_irq(struct dw_pcie_ep *ep, u8 func_no,
		enum pci_epc_irq_type type, u16 interrupt_num)
{
	s32 err = 0;

	if (ep != NULL) {
		struct dw_pcie *pci = to_dw_pcie_from_ep(ep);
		const struct pci_epc_features *epc_features = tcc_pcie_get_features(ep);
		u8 intr_num;

		if (!__builtin_add_overflow(interrupt_num, 0, &intr_num)) {
			switch (type) {
			case PCI_EPC_IRQ_MSI:
				if (epc_features->msi_capable != 0U) {
					err = tcc_pcie_ep_raise_msi_irq(ep, func_no, intr_num);
				}
				break;
			case PCI_EPC_IRQ_MSIX:
				if (epc_features->msix_capable != 0U) {
					err = tcc_pcie_ep_raise_msix_irq(ep, func_no, intr_num);
				}
				break;
			case PCI_EPC_IRQ_LEGACY:
				err = tcc_pcie_ep_raise_legacy_irq(ep, func_no);
				break;
			case PCI_EPC_IRQ_UNKNOWN:
			default:
				dev_err(pci->dev, "INVALID DEV type : %d\n", type);
				err = -ENODEV;
				break;
			}
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static struct dw_pcie_ep_ops tcc_pcie_ep_ops = {
	.get_features = tcc_pcie_get_features,
	.ep_init = tcc_pcie_ep_init,
	.raise_irq = tcc_pcie_ep_raise_irq,
};

static s32 tcc_pcie_prepare_ep(struct tcc_pcie *tp, struct platform_device *pdev,
		struct dw_pcie_ep *ep)
{
	s32 err = 0;

	if ((tp != NULL) &&
			(pdev != NULL) &&
			(ep != NULL)) {
		const struct resource *res =
			(const struct resource *)platform_get_resource_byname(pdev, IORESOURCE_MEM, "addr_space");

		if (res != NULL) {
			ep->phys_base = res->start;
			ep->addr_size = resource_size(res);
			ep->page_size = SZ_64K;		/* 64K alignment */

			ep->ops = &tcc_pcie_ep_ops;
			tp->irq = platform_get_irq(pdev, 0);
			if (tp->irq < 0) {
				err = -ENODEV;
			} else {
				err = devm_request_irq(&pdev->dev,
						(u32)tp->irq, tcc_pcie_irq_handler,
						IRQF_SHARED, "telechips-pcie-ep",
						tp);
			}
		} else {
			err = -ENODEV;
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static void tcc_pcie_prog_inbound_atu_bar(struct dw_pcie *pci, u8 func_no,
					  u32 index, u32 bar_num,
					  u64 target_addr)
{
	u32 offset, val, retries;

	if (!pci)
		return;

	if (pci->iatu_unroll_enabled) {
		offset = PCIE_GET_ATU_INB_UNR_REG_OFFSET(index);

		/* target = EP local phys */
		tcc_pcie_writel_atu(pci, offset + PCIE_ATU_UNR_LOWER_TARGET,
				    lower_32_bits(target_addr));
		tcc_pcie_writel_atu(pci, offset + PCIE_ATU_UNR_UPPER_TARGET,
				    upper_32_bits(target_addr));

		tcc_pcie_writel_atu(pci, offset + PCIE_ATU_UNR_REGION_CTRL1,
				    PCIE_ATU_TYPE_MEM | PCIE_ATU_FUNC_NUM(func_no));

		tcc_pcie_writel_atu(pci, offset + PCIE_ATU_UNR_REGION_CTRL2,
				    PCIE_ATU_ENABLE | PCIE_ATU_BAR_MODE_ENABLE |
				    PCIE_ATU_BAR_NUM(bar_num));

		for (retries = 0; retries < (u32)LINK_WAIT_MAX_IATU_RETRIES; retries++) {
			val = tcc_pcie_readl_atu(pci, offset + PCIE_ATU_UNR_REGION_CTRL2);
			if (val & PCIE_ATU_ENABLE) {
				dev_info(pci->dev, "Inbound iATU[%u] BAR%u -> 0x%llx enabled\n",
					 index, bar_num, target_addr);
				return;
			}
			mdelay(LINK_WAIT_IATU);
		}
		dev_err(pci->dev, "Inbound(BAR) iATU enable failed idx=%u bar=%u\n",
			index, bar_num);
	} else {
		/* non-unroll path */
		dw_pcie_writel_dbi(pci, PCIE_ATU_VIEWPORT,
				   PCIE_ATU_REGION_INBOUND | index);

		dw_pcie_writel_dbi(pci, PCIE_ATU_LOWER_TARGET,
				   lower_32_bits(target_addr));
		dw_pcie_writel_dbi(pci, PCIE_ATU_UPPER_TARGET,
				   upper_32_bits(target_addr));

		dw_pcie_writel_dbi(pci, PCIE_ATU_CR1,
				   PCIE_ATU_TYPE_MEM | PCIE_ATU_FUNC_NUM((u32)func_no));
		dw_pcie_writel_dbi(pci, PCIE_ATU_CR2,
				   (u32)PCIE_ATU_ENABLE | PCIE_ATU_BAR_MODE_ENABLE |
				   PCIE_ATU_BAR_NUM(bar_num));

		for (retries = 0; retries < (u32)LINK_WAIT_MAX_IATU_RETRIES; retries++) {
			val = dw_pcie_readl_dbi(pci, PCIE_ATU_CR2);
			if (val & (u32)PCIE_ATU_ENABLE) {
				dev_info(pci->dev, "Inbound iATU[%u] BAR%u -> 0x%llx enabled\n",
					 index, bar_num, target_addr);
				return;
			}
			mdelay(LINK_WAIT_IATU);
		}
		dev_err(pci->dev, "Inbound(BAR) iATU enable failed idx=%u bar=%u\n",
			index, bar_num);
	}
}

static s32 tcc_pcie_add_pcie_ep(struct tcc_pcie *tp, struct platform_device *pdev)
{
	s32 err = 0;

	if ((tp != NULL) && (pdev != NULL)) {
		struct dw_pcie *pci = tp->pci;
		struct dw_pcie_ep *ep = &pci->ep;

		err = tcc_pcie_set_clk((const struct tcc_pcie *)tp);
		if (err == 0) {
			err = tcc_pcie_prepare_ep(tp, pdev, ep);
		}

		if (err == 0) {
			err = tcc_pcie_set_defaults((const struct tcc_pcie *)tp);
		}

		if (err == 0) {
			err = tcc_pcie_ep_get_iatu_unroll_support(pci);
		}

		if (err == 0) {
			pci->dbi_base2 = pci->dbi_base + PCIE_DBI2_OFFSET;
			err = dw_pcie_ep_init(ep);
		}

		if (err == 0) {
			/* Program Inbound iATU (BAR match) */
			tcc_pcie_prog_inbound_atu_bar(pci, 0, 0, 0,
				0x500010000ULL); /* ib0 -> BAR0 (4KB) */
			tcc_pcie_prog_inbound_atu_bar(pci, 0, 1, 1,
				0x500020000ULL); /* ib1 -> BAR1 (64KB) */
		}

		if (err == 0) {
			err = tcc_pcie_init_edma(tp, pdev);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}
#endif

static s32 tcc_pcie_add_pcie_port(struct tcc_pcie *tp, struct platform_device *pdev)
{
	s32 err = 0;

	if ((tp != NULL) && (pdev != NULL)) {
		struct dw_pcie *pci = tp->pci;
		struct pcie_port *pp = &pci->pp;

		err = tcc_pcie_set_clk((const struct tcc_pcie *)tp);
		if (err == 0) {
			err = tcc_pcie_prepare_rc(tp, pdev, pp);
		}

		if (err == 0) {
			err = tcc_pcie_set_defaults((const struct tcc_pcie *)tp);
		}

		if (err == 0) {
			if (tp->variant != TCC803X) {
				err = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
			} else {
				err = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
			}
		}

		if (err == 0) {
			err = dw_pcie_host_init(pp);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static int tcc_pcie_add_port(struct tcc_pcie *tp,
		struct platform_device *pdev)
{
	s32 err = 0;

	if ((tp != NULL) && (pdev != NULL)) {
		err = tcc_pcie_wake_up_phy((const struct tcc_pcie *)tp);
		if (err == 0) {
			switch (tp->mode) {
			case DW_PCIE_RC_TYPE:
				err = tcc_pcie_add_pcie_port(tp, pdev);
				break;
			case DW_PCIE_EP_TYPE:
#ifdef CONFIG_PCIE_DW_EP
				err = tcc_pcie_add_pcie_ep(tp, pdev);
				break;
#endif
			case DW_PCIE_UNKNOWN_TYPE:
			case DW_PCIE_LEG_EP_TYPE:
			default:
				err = -ENODEV;
				break;
			}
		}

		if (err != 0) {
			dev_err(&pdev->dev, "failed to init controller as %s mode\n",
					(tp->mode == DW_PCIE_EP_TYPE) ? "EP" : "RC");
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_parse_regs(struct platform_device *pdev,
		struct tcc_pcie *tp)
{
	s32 err = 0;

	if ((tp != NULL) && (pdev != NULL)) {
		struct resource *res;

		res = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "link");
		if (res != NULL) {
			tp->link_base = devm_ioremap(&pdev->dev,
					res->start, resource_size(res));
			if (IS_ERR(tp->link_base)) {
				err = (s32)PTR_ERR(tp->link_base);
			}
		} else {
			err = -ENODEV;
		}

		if (err == 0) {
			res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "dbi");
			if (res != NULL) {
				struct dw_pcie *pci = tp->pci;

				pci->dbi_base = devm_ioremap(&pdev->dev,
						res->start, resource_size(res));
				if (IS_ERR(pci->dbi_base)) {
					err = (s32)PTR_ERR(pci->dbi_base);
				}
			} else {
				err = -ENODEV;
			}
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static const struct	tcc_pcie_of_data tcc_pcie_tcc803x_rc_of_data = {
	.mode = DW_PCIE_RC_TYPE,
	.variant = TCC803X,
};

static const struct	tcc_pcie_of_data tcc_pcie_tcc807x_rc_of_data = {
	.mode = DW_PCIE_RC_TYPE,
	.variant = TCC807X,
};
static const struct	tcc_pcie_of_data tcc_pcie_tcc807x_ep_of_data = {
	.mode = DW_PCIE_EP_TYPE,
	.variant = TCC807X,
};

static const struct	tcc_pcie_of_data tcc_pcie_tcc805x_rc_of_data = {
	.mode = DW_PCIE_RC_TYPE,
	.variant = TCC805X,
};
static const struct	tcc_pcie_of_data tcc_pcie_tcc805x_ep_of_data = {
	.mode = DW_PCIE_EP_TYPE,
	.variant = TCC805X,
};

static const struct	tcc_pcie_of_data tcc_pcie_tcc750x_rc_of_data = {
	.mode = DW_PCIE_RC_TYPE,
	.variant = TCC750X,
};
static const struct	tcc_pcie_of_data tcc_pcie_tcc750x_ep_of_data = {
	.mode = DW_PCIE_EP_TYPE,
	.variant = TCC750X,
};

static const struct	tcc_pcie_of_data tcc_pcie_tcn100x_rc_of_data = {
	.mode = DW_PCIE_RC_TYPE,
	.variant = TCN100X,
};
static const struct	tcc_pcie_of_data tcc_pcie_tcn100x_ep_of_data = {
	.mode = DW_PCIE_EP_TYPE,
	.variant = TCN100X,
};

static const struct of_device_id tcc_pcie_of_match[] = {
	{
		.compatible = "telechips,tcc803x-pcie",
		.data = &tcc_pcie_tcc803x_rc_of_data,
	},
	{
		.compatible = "telechips,tcc805x-pcie",
		.data = &tcc_pcie_tcc805x_rc_of_data,
	},
	{
		.compatible = "telechips,tcc805x-pcie-ep",
		.data = &tcc_pcie_tcc805x_ep_of_data,
	},
	{
		.compatible = "telechips,tcc750x-pcie",
		.data = &tcc_pcie_tcc750x_rc_of_data,
	},
	{
		.compatible = "telechips,tcc750x-pcie-ep",
		.data = &tcc_pcie_tcc750x_ep_of_data,
	},
	{
		.compatible = "telechips,tcc807x-pcie",
		.data = &tcc_pcie_tcc807x_rc_of_data,
	},
	{
		.compatible = "telechips,tcc807x-pcie-ep",
		.data = &tcc_pcie_tcc807x_ep_of_data,
	},
	{
		.compatible = "telechips,tcn100x-pcie",
		.data = &tcc_pcie_tcn100x_rc_of_data,
	},
	{
		.compatible = "telechips,tcn100x-pcie-ep",
		.data = &tcc_pcie_tcn100x_ep_of_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, tcc_pcie_of_match);

static s32 tcc_pcie_get_of_data(const struct platform_device *pdev,
		struct tcc_pcie *tp)
{
	s32 err = 0;

	if ((pdev != NULL) && (tp != NULL)) {
		const struct of_device_id *match = (const struct of_device_id *)of_match_device(
				of_match_ptr(tcc_pcie_of_match),
				&pdev->dev);

		if (match != NULL) {
			const struct tcc_pcie_of_data *data =
				(const struct tcc_pcie_of_data *)match->data;

			tp->mode = data->mode;
			tp->variant = data->variant;
		} else {
			err = -ENODEV;
		}

		if (err != 0) {
			dev_err(&pdev->dev, "failed to get of_data\n");
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_parse_phy(struct platform_device *pdev,
		struct tcc_pcie *tp)
{
	s32 err = 0;

	if ((tp != NULL) && (pdev != NULL)) {
		tp->phy = devm_phy_get(&pdev->dev, "pcie-phy");
		if (IS_ERR(tp->phy)) {
			if (tp->variant != TCN100X) {
				dev_err(&pdev->dev, "Failed to get pcie phy. Check kernel configuration.\n");
				err = (s32)PTR_ERR(tp->phy);
			} else {
				dev_warn(&pdev->dev, "Failed to get pcie phy. Check if intended\n");
				tp->phy = NULL;
			}
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_parse_reset(struct platform_device *pdev,
		struct tcc_pcie *tp)
{
	s32 err = 0;

	if ((tp != NULL) && (pdev != NULL)) {
		if (IS_ENABLED(CONFIG_RESET_TELECHIPS) != 0) {
			tp->reset = devm_reset_control_get(&pdev->dev, NULL);
			if (IS_ERR(tp->reset)) {
				tp->reset = NULL;
			}
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_parse_props(struct platform_device *pdev,
		struct tcc_pcie *tp)
{
	s32 err = 0;

	if ((tp != NULL) && (pdev != NULL)) {
		err = of_property_read_u32(pdev->dev.of_node,
				"refclk_type", &tp->refclk_type);

		if (err == 0) {
			err = of_property_read_u32(pdev->dev.of_node,
					"max-link-speed", &tp->max_link_speed);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_parse_dt(struct platform_device *pdev,
		struct tcc_pcie *tp)
{
	s32 err = 0;

	if ((tp != NULL) && (pdev != NULL)) {
		err = tcc_pcie_parse_regs(pdev, tp);

		if (err == 0) {
			err = tcc_pcie_parse_phy(pdev, tp);
		}

		if (err == 0) {
			err = tcc_pcie_parse_reset(pdev, tp);
		}

		if (err == 0) {
			err = tcc_pcie_parse_props(pdev, tp);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static struct tcc_pcie *tcc_pcie_alloc_drvdata(struct platform_device *pdev)
{
	struct tcc_pcie *tp = NULL;

	if (pdev != NULL) {
		tp = devm_kzalloc(&pdev->dev,
				sizeof(struct tcc_pcie), GFP_KERNEL);
		if (tp != NULL) {
			tp->pci = devm_kzalloc(&pdev->dev,
					sizeof(struct dw_pcie), GFP_KERNEL);
			if (tp->pci != NULL) {
				tp->suspend_regs = kzalloc(
						PCIE_REG_BACKUP_SIZE, GFP_KERNEL);
				if (IS_ERR_OR_NULL(tp->suspend_regs)) {
					devm_kfree(&pdev->dev, tp->pci);
				}
			}

			if (IS_ERR_OR_NULL(tp->pci)) {
				devm_kfree(&pdev->dev, tp);
			}
		}
	}

	return tp;
}

static int tcc_pcie_probe(struct platform_device *pdev)
{
	struct tcc_pcie *tp;
	s32 err = 0;

	if (pdev != NULL) {
		tp = tcc_pcie_alloc_drvdata(pdev);
		if (tp != NULL) {
			err = tcc_pcie_get_of_data(pdev, tp);
			if (err == 0) {
				err = tcc_pcie_parse_dt(pdev, tp);
			}

			if (err == 0) {
				tp->pci->ops = &tcc_pcie_host_ops;
				tp->pci->dev = &pdev->dev;
				platform_set_drvdata(pdev, tp);
				err = tcc_pcie_add_port(tp, pdev);
			}

#ifdef CONFIG_DEBUG_FS
			if (err == 0) {
				err = tcc_pcie_debugfs_init(tp);
			}
#endif
		} else {
			err = -ENOMEM;
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_remove(struct platform_device *pdev)
{
	s32 err = 0;

	if (pdev != NULL) {
		const struct tcc_pcie *tp =
			(const struct tcc_pcie *)platform_get_drvdata(pdev);
		const struct dw_pcie *pci = (const struct dw_pcie *)tp->pci;

		err = tcc_pcie_disable_irq(tp);
		if (err == 0) {
			err = pinctrl_pm_select_idle_state(pci->dev);
		}

		if (err == 0) {
			mdelay(1);
			err = pinctrl_pm_select_default_state(pci->dev);
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static int tcc_pcie_suspend(struct device *dev)
{
	s32 err = 0;

	if (dev != NULL) {
		const struct tcc_pcie *tp =
			(const struct tcc_pcie *)dev_get_drvdata(dev);
		struct dw_pcie *pci = tp->pci;

		if (tp->mode == DW_PCIE_RC_TYPE) {
			u32 val;

			val = dw_pcie_readl_dbi(pci, PCI_COMMAND);
			val &= ~(u32)PCI_COMMAND_MEMORY;
			dw_pcie_writel_dbi(pci, PCI_COMMAND, val);
		}
	} else {
		err = -ENODEV;
	}

	return err;
}

static int tcc_pcie_resume(struct device *dev)
{
	s32 err = 0;

	if (dev != NULL) {
		const struct tcc_pcie *tp =
			(const struct tcc_pcie *)dev_get_drvdata(dev);
		struct dw_pcie *pci = tp->pci;

		if (tp->mode == DW_PCIE_RC_TYPE) {
			u32 val;

			val = dw_pcie_readl_dbi(pci, PCI_COMMAND);
			val |= (u32)PCI_COMMAND_MEMORY;
			dw_pcie_writel_dbi(pci, PCI_COMMAND, val);
		}
	} else {
		err = -ENODEV;
	}

	return err;
}

static s32 tcc_pcie_reestablish_link(const struct tcc_pcie *tp)
{
	s32 err = 0;

	if (tp != NULL) {
		struct dw_pcie *pci = tp->pci;

		err = tcc_pcie_reset_control(tp);
		if (err == 0) {
			err = tcc_pcie_wake_up_phy(tp);
		}

		if (err == 0) {
			err = tcc_pcie_set_clk(tp);
		}

		if (err == 0) {
			err = tcc_pcie_set_defaults(tp);
		}

		if (err == 0) {
			if (tp->mode == DW_PCIE_RC_TYPE) {
				err = tcc_pcie_host_init(&pci->pp);
			} else if (tp->mode == DW_PCIE_EP_TYPE) {
#ifdef CONFIG_PCIE_DW_EP
				err = dw_pcie_ep_init(&pci->ep);
#endif
			} else {
				err = -ENODEV;
			}
		}
	} else {
		err = -EINVAL;
	}

	return err;
}

static s32 tcc_pcie_resume_early(struct device *dev)
{
	s32 err = 0;

	if (dev != NULL) {
		const struct tcc_pcie *tp =
			(const struct tcc_pcie *)dev_get_drvdata(dev);

		err = tcc_pcie_reestablish_link(tp);
		if (err == 0) {
			err = tcc_pcie_restore_reg(tp);
		}

		if (err == 0) {
			err = tcc_pcie_enable_irq(tp);
		}
	} else {
		err = -ENODEV;
	}

	return err;
}

static s32 tcc_pcie_suspend_late(struct device *dev)
{
	s32 err = 0;

	if (dev != NULL) {
		const struct tcc_pcie *tp =
			(const struct tcc_pcie *)dev_get_drvdata(dev);

		err = tcc_pcie_disable_irq(tp);
		if (err == 0) {
			err = tcc_pcie_backup_reg(tp);
		}

		if (err == 0) {
			err = phy_power_off(tp->phy);
		}

		if (err == 0) {
			err = tcc_pcie_clear_cactive(tp);
		}
		if (err == 0) {
			err = pinctrl_pm_select_sleep_state(dev);
		}
	} else {
		err = -ENODEV;
	}

	return err;
}

static const struct dev_pm_ops tcc_pcie_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tcc_pcie_suspend, tcc_pcie_resume)
	SET_LATE_SYSTEM_SLEEP_PM_OPS(tcc_pcie_suspend_late,
			tcc_pcie_resume_early)
};

static struct platform_driver tcc_pcie_driver = {
	.probe = tcc_pcie_probe,
	.remove = tcc_pcie_remove,
	.driver = {
		.name = "telechips-pcie",
		.pm = &tcc_pcie_pm_ops,
		.of_match_table = tcc_pcie_of_match,
	},
};
module_platform_driver(tcc_pcie_driver);

MODULE_DESCRIPTION("Telechips PCIe controller driver");
MODULE_LICENSE("GPL v2");

