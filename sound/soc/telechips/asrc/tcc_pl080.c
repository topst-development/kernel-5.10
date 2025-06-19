// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/io.h>

#include "tcc_pl080.h"
#include "tcc_asrc_rule.h"

void tcc_pl080_dump_regs(const void __iomem *pl080_reg)
{
	asrc_info("PL080 Regs(virts:0x%px)\n", pl080_reg);
	asrc_info("IntStatus      : 0x%08x\n",
		readl(pl080_reg + PL080_INT_STATUS));
	asrc_info("IntTCStatus    : 0x%08x\n",
		readl(pl080_reg + PL080_TC_STATUS));
	asrc_info("IntErrorStatus : 0x%08x\n",
		readl(pl080_reg + PL080_ERR_STATUS));
	asrc_info("RawIntTCStatus : 0x%08x\n",
		readl(pl080_reg + PL080_RAW_TC_STATUS));
	asrc_info("RawIntErrStatus: 0x%08x\n",
		readl(pl080_reg + PL080_RAW_ERR_STATUS));
	asrc_info("EnbldChns      : 0x%08x\n",
		readl(pl080_reg + PL080_EN_CHAN));
	asrc_info("SoftBreq       : 0x%08x\n",
		readl(pl080_reg + PL080_SOFT_BREQ));
	asrc_info("SoftSreq       : 0x%08x\n",
		readl(pl080_reg + PL080_SOFT_SREQ));
	asrc_info("SoftLBRreq     : 0x%08x\n",
		readl(pl080_reg + PL080_SOFT_LBREQ));
	asrc_info("SoftLSReq      : 0x%08x\n",
		readl(pl080_reg + PL080_SOFT_LSREQ));
	asrc_info("Configuration  : 0x%08x\n",
		readl(pl080_reg + PL080_CONFIG));
	asrc_info("Sync           : 0x%08x\n",
		readl(pl080_reg + PL080_SYNC));

	asrc_info("C0_SrcAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_SRC_ADDR(0U)));
	asrc_info("C0_DstAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_DST_ADDR(0U)));
	asrc_info("C0_NextLLI     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_LLI(0U)));
	asrc_info("C0_Control     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONTROL(0U)));
	asrc_info("C0_Config      : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONFIG(0U)));

	asrc_info("C1_SrcAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_SRC_ADDR(1U)));
	asrc_info("C1_DstAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_DST_ADDR(1U)));
	asrc_info("C1_NextLLI     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_LLI(1U)));
	asrc_info("C1_Control     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONTROL(1U)));
	asrc_info("C1_Config      : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONFIG(1U)));

	asrc_info("C2_SrcAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_SRC_ADDR(2U)));
	asrc_info("C2_DstAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_DST_ADDR(2U)));
	asrc_info("C2_NextLLI     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_LLI(2U)));
	asrc_info("C2_Control     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONTROL(2U)));
	asrc_info("C2_Config      : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONFIG(2U)));

	asrc_info("C3_SrcAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_SRC_ADDR(3U)));
	asrc_info("C3_DstAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_DST_ADDR(3U)));
	asrc_info("C3_NextLLI     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_LLI(3U)));
	asrc_info("C3_Control     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONTROL(3U)));
	asrc_info("C3_Config      : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONFIG(3U)));

	asrc_info("C4_SrcAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_SRC_ADDR(4U)));
	asrc_info("C4_DstAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_DST_ADDR(4U)));
	asrc_info("C4_NextLLI     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_LLI(4U)));
	asrc_info("C4_Control     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONTROL(4U)));
	asrc_info("C4_Config      : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONFIG(4U)));

	asrc_info("C5_SrcAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_SRC_ADDR(5U)));
	asrc_info("C5_DstAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_DST_ADDR(5U)));
	asrc_info("C5_NextLLI     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_LLI(5U)));
	asrc_info("C5_Control     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONTROL(5U)));
	asrc_info("C5_Config      : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONFIG(5U)));

	asrc_info("C6_SrcAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_SRC_ADDR(6U)));
	asrc_info("C6_DstAddr     : 0x%08x\n"
		, readl(pl080_reg + PL080_Cx_DST_ADDR(6U)));
	asrc_info("C6_NextLLI     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_LLI(6U)));
	asrc_info("C6_Control     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONTROL(6U)));
	asrc_info("C6_Config      : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONFIG(6U)));

	asrc_info("C7_SrcAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_SRC_ADDR(7U)));
	asrc_info("C7_DstAddr     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_DST_ADDR(7U)));
	asrc_info("C7_NextLLI     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_LLI(7U)));
	asrc_info("C7_Control     : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONTROL(7U)));
	asrc_info("C7_Config      : 0x%08x\n",
		readl(pl080_reg + PL080_Cx_CONFIG(7U)));
}

void tcc_pl080_enable(void __iomem *pl080_reg, bool enable)
{
	if (enable) {
		writel(PL080_CONFIG_ENABLE, pl080_reg+PL080_CONFIG);
	} else {
		writel(0, pl080_reg+PL080_CONFIG);
	}
}

uint32_t tcc_pl080_get_int_status(const void __iomem *pl080_reg)
{
	return readl(pl080_reg+PL080_INT_STATUS);
}

void tcc_pl080_clear_int(void __iomem *pl080_reg, uint32_t bitmask)
{
	writel(bitmask, pl080_reg+PL080_TC_CLEAR);
}

void tcc_pl080_clear_err(void __iomem *pl080_reg, uint32_t bitmask)
{
	writel(bitmask, pl080_reg+PL080_TC_CLEAR);
}

void tcc_pl080_set_first_lli(
	void __iomem *pl080_reg,
	uint32_t dma_ch,
	const struct pl080_lli *lli)
{
	writel(lli->src_addr, pl080_reg+PL080_Cx_SRC_ADDR(dma_ch));
	writel(lli->dst_addr, pl080_reg+PL080_Cx_DST_ADDR(dma_ch));
	writel(lli->next_lli, pl080_reg+PL080_Cx_LLI(dma_ch));
	writel(lli->control0, pl080_reg+PL080_Cx_CONTROL(dma_ch));
}

void tcc_pl080_set_channel_mem2per(
	void __iomem *pl080_reg,
	uint32_t dma_ch,
	enum tcc_peri_id_t dst_peri,
	bool irq_en,
	bool err_en)
{
	uint32_t val = 0;

	val = ((uint32_t)PL080_FLOW_MEM2PER << (uint32_t)PL080_CONFIG_FLOW_CONTROL_SHIFT)
		| ((uint32_t)dst_peri << (uint32_t)PL080_CONFIG_DST_SEL_SHIFT);

	if (irq_en) {
		val |= (uint32_t)PL080_CONFIG_TC_IRQ_MASK;
	}

	if (err_en) {
		val |= (uint32_t)PL080_CONFIG_ERR_IRQ_MASK;
	}

	writel(val, pl080_reg + PL080_Cx_CONFIG(dma_ch));
}

void tcc_pl080_set_channel_per2mem(
	void __iomem *pl080_reg,
	uint32_t dma_ch,
	enum tcc_peri_id_t src_peri,
	bool irq_en,
	bool err_en)
{
	uint32_t val = 0;

	val = ((uint32_t)PL080_FLOW_PER2MEM << PL080_CONFIG_FLOW_CONTROL_SHIFT)
		| ((uint32_t)src_peri << PL080_CONFIG_SRC_SEL_SHIFT);

	if (irq_en) {
		val |= (uint32_t)PL080_CONFIG_TC_IRQ_MASK;
	}

	if (err_en) {
		val |= (uint32_t)PL080_CONFIG_ERR_IRQ_MASK;
	}

	writel(val, pl080_reg + PL080_Cx_CONFIG(dma_ch));
}


void tcc_pl080_channel_enable(void __iomem *pl080_reg, uint32_t dma_ch, bool enable)
{
	uint32_t val;

	val = readl(pl080_reg+PL080_Cx_CONFIG(dma_ch));

	if (enable) {
		val |= (uint32_t)PL080_CONFIG_ENABLE;
	} else {
		val &= ~(ul_to_ui(PL080_CONFIG_ENABLE));
	}

	writel(val, pl080_reg+PL080_Cx_CONFIG(dma_ch));
}

void tcc_pl080_halt_enable(void __iomem *pl080_reg, uint32_t dma_ch, bool enable)
{
	uint32_t val;

	val = readl(pl080_reg+PL080_Cx_CONFIG(dma_ch));

	if (enable) {
		val |= (uint32_t)PL080_CONFIG_HALT;
	} else {
		val &= ~(ul_to_ui(PL080_CONFIG_HALT));
	}

	writel(val, pl080_reg+PL080_Cx_CONFIG(dma_ch));
}

void tcc_pl080_channel_sync_mode(void __iomem *pl080_reg, uint32_t dma_ch, bool sync)
{
	uint32_t val;

	val = readl(pl080_reg+PL080_SYNC);

	if (sync) {
		val |= ui_lshift(1U, dma_ch);
	} else {
		val &= ~(ui_lshift(1U, dma_ch));
	}

	writel(val, pl080_reg+PL080_SYNC);
}

uint32_t tcc_pl080_lli_control_value(const struct tcc_pl080_ctl_info_t *tcc_pl080_ctl_info)
{
	uint32_t val = 0;

	val = ((uint32_t)tcc_pl080_ctl_info->src_width << (uint32_t)PL080_CONTROL_SWIDTH_SHIFT)
		| ((uint32_t)tcc_pl080_ctl_info->dst_width << (uint32_t)PL080_CONTROL_DWIDTH_SHIFT)
		| ((uint32_t)tcc_pl080_ctl_info->src_bsize << (uint32_t)PL080_CONTROL_SB_SIZE_SHIFT)
		| ((uint32_t)tcc_pl080_ctl_info->dst_bsize << (uint32_t)PL080_CONTROL_DB_SIZE_SHIFT);

	val	|= (ui_lshift(
			ul_to_ui(tcc_pl080_ctl_info->transfer_size&PL080_CONTROL_TRANSFER_SIZE_MASK),
			(uint32_t)PL080_CONTROL_TRANSFER_SIZE_SHIFT));

	if (tcc_pl080_ctl_info->src_incr) {
		val |= (uint32_t)PL080_CONTROL_SRC_INCR;
	}

	if (tcc_pl080_ctl_info->dst_incr) {
		val |= (uint32_t)PL080_CONTROL_DST_INCR;
	}

	if (tcc_pl080_ctl_info->irq_en) {
		val |= (uint32_t)PL080_CONTROL_TC_IRQ_EN;
	}

	return val;
}

uint32_t tcc_pl080_get_cur_src_addr(const void __iomem *pl080_reg, uint32_t dma_ch)
{
	return readl(pl080_reg + PL080_Cx_SRC_ADDR(dma_ch));
}

uint32_t tcc_pl080_get_cur_dst_addr(const void __iomem *pl080_reg, uint32_t dma_ch)
{
	return readl(pl080_reg + PL080_Cx_DST_ADDR(dma_ch));
}

uint32_t tcc_pl080_get_cur_lli_addr(const void __iomem *pl080_reg, uint32_t dma_ch)
{
	return readl(pl080_reg + PL080_Cx_LLI(dma_ch));
}
