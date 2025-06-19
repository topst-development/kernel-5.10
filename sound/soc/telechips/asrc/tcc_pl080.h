/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_PL080_H
#define TCC_PL080_H

#include <linux/amba/pl080.h>

#define PL080_MAX_TRANSFER_SIZE\
	ui_lshift(1U,11)
#define TRANSFER_UNIT_BYTES\
	(4U)

enum tcc_peri_id_t {
	PERI_ID_P0_TX = 0,
	PERI_ID_P1_TX = 1,
	PERI_ID_P2_TX = 2,
	PERI_ID_P3_TX = 3,
	PERI_ID_P0_RX = 4,
	PERI_ID_P1_RX = 5,
	PERI_ID_P2_RX = 6,
	PERI_ID_P3_RX = 7,
};

enum tcc_pl080_width_t {
	TCC_PL080_WIDTH_8BIT  = PL080_WIDTH_8BIT,
	TCC_PL080_WIDTH_16BIT = PL080_WIDTH_16BIT,
	TCC_PL080_WIDTH_32BIT = PL080_WIDTH_32BIT,
};

enum tcc_pl080_bsize_t {
	TCC_PL080_BSIZE_1   = PL080_BSIZE_1,
	TCC_PL080_BSIZE_4   = PL080_BSIZE_4,
	TCC_PL080_BSIZE_8   = PL080_BSIZE_8,
	TCC_PL080_BSIZE_16  = PL080_BSIZE_16,
	TCC_PL080_BSIZE_32  = PL080_BSIZE_32,
	TCC_PL080_BSIZE_64  = PL080_BSIZE_64,
	TCC_PL080_BSIZE_128 = PL080_BSIZE_128,
	TCC_PL080_BSIZE_256 = PL080_BSIZE_256,
};

#undef PL080_Cx_BASE
#define PL080_Cx_BASE(x)                        ((0x100U + (x * 0x20U)))
#undef PL080_Cx_SRC_ADDR
#define PL080_Cx_SRC_ADDR(x)                    ((0x100U + (x * 0x20U)))
#undef PL080_Cx_DST_ADDR
#define PL080_Cx_DST_ADDR(x)                    ((0x104U + (x * 0x20U)))
#undef PL080_Cx_LLI
#define PL080_Cx_LLI(x)                         ((0x108U + (x * 0x20U)))
#undef PL080_Cx_CONTROL
#define PL080_Cx_CONTROL(x)                     ((0x10CU + (x * 0x20U)))
#undef PL080_Cx_CONFIG
#define PL080_Cx_CONFIG(x)                      ((0x110U + (x * 0x20U)))
#undef PL080S_Cx_CONTROL2
#define PL080S_Cx_CONTROL2(x)                   ((0x110U + (x * 0x20U)))
#undef PL080S_Cx_CONFIG
#define PL080S_Cx_CONFIG(x)                     ((0x114 + (x * 0x20)))

struct tcc_pl080_ctl_info_t{
	uint32_t transfer_size;
	enum tcc_pl080_width_t src_width;
	enum tcc_pl080_bsize_t src_bsize;
	bool src_incr;
	enum tcc_pl080_width_t dst_width;
	enum tcc_pl080_bsize_t dst_bsize;
	bool dst_incr;
	bool irq_en;
};

void tcc_pl080_dump_regs(const void __iomem *pl080_reg);
void tcc_pl080_enable(void __iomem *pl080_reg, bool enable);
uint32_t tcc_pl080_get_int_status(const void __iomem *pl080_reg);
void tcc_pl080_clear_int(
	void __iomem *pl080_reg,
	uint32_t bitmask);
void tcc_pl080_clear_err(
	void __iomem *pl080_reg,
	uint32_t bitmask);
void tcc_pl080_set_first_lli(
	void __iomem *pl080_reg,
	uint32_t dma_ch,
	const struct pl080_lli *lli);
void tcc_pl080_set_channel_mem2per(
	void __iomem *pl080_reg,
	uint32_t dma_ch,
	enum tcc_peri_id_t dst_peri,
	bool irq_en,
	bool err_en);
void tcc_pl080_set_channel_per2mem(
	void __iomem *pl080_reg,
	uint32_t dma_ch,
	enum tcc_peri_id_t src_peri,
	bool irq_en,
	bool err_en);
void tcc_pl080_channel_enable(
	void __iomem *pl080_reg,
	uint32_t dma_ch,
	bool enable);
void tcc_pl080_halt_enable(
	void __iomem *pl080_reg,
	uint32_t dma_ch,
	bool enable);
void tcc_pl080_channel_sync_mode(
	void __iomem *pl080_reg,
	uint32_t dma_ch,
	bool sync);
uint32_t tcc_pl080_lli_control_value(const struct tcc_pl080_ctl_info_t *tcc_pl080_ctl_info);

uint32_t tcc_pl080_get_cur_src_addr(
	const void __iomem *pl080_reg,
	uint32_t dma_ch);
uint32_t tcc_pl080_get_cur_dst_addr(
	const void __iomem *pl080_reg,
	uint32_t dma_ch);
uint32_t tcc_pl080_get_cur_lli_addr(
	const void __iomem *pl080_reg,
	uint32_t dma_ch);

#endif /*TCC_PL080_H*/
