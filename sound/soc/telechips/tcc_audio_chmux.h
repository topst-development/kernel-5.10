/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_PORT_MUX_H
#define TCC_PORT_MUX_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/io.h>

#include "tcc_audio_hw.h"
#include "tcc_audio_rule.h"

#if 0				//DEBUG
#define chmux_writel(v, c) \
	({pr_info("<ASoC> IOCFG_REG(%p) = 0x%08x\n", c,\
	(unsigned int)v); writel(v, c); })
#else
#define chmux_writel(v, c) \
	writel(v, c)
#endif

#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || \
	defined(CONFIG_ARCH_TCC901X)

#define IOBUSCFG_DAI_PORT_MAX \
	(3)
#define IOBUSCFG_CDIF_PORT_MAX \
	(3)
#define IOBUSCFG_SPDIF_PORT_MAX \
	(3)

static inline void iobuscfg_dai_chmux(
	void __iomem *iobuscfg_reg,
	int32_t id,
	uint32_t port)
{
	ptrdiff_t offset =
	    (id == 0) ? IOBUS_CFG_DAI0_CHMUX : IOBUS_CFG_DAI1_CHMUX;

	if (port <= IOBUSCFG_DAI_PORT_MAX) {
		uint32_t value = 0;
		value = (1 << port);
		value = (value << IOBUS_CFG_CHMUX_SEL_Pos) &
				IOBUS_CFG_CHMUX_SEL_Msk;
		chmux_writel(
			value,
			iobuscfg_reg + offset);
	}
}



static inline void iobuscfg_spdif_chmux(
	void __iomem *iobuscfg_reg,
	int32_t id,
	uint32_t port)
{
	ptrdiff_t offset =
	    (id == 0) ? IOBUS_CFG_SPDIF0_CHMUX : IOBUS_CFG_SPDIF1_CHMUX;

	if (port <= IOBUSCFG_SPDIF_PORT_MAX) {
		uint32_t value = 0;
		value = (1 << port);
		value = (value << IOBUS_CFG_CHMUX_SEL_Pos) &
				IOBUS_CFG_CHMUX_SEL_Msk;
		chmux_writel(
			value,
			iobuscfg_reg + offset);
	}
}

#elif defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || \
	defined(CONFIG_ARCH_TCC806X)

#if defined(CONFIG_ARCH_TCC803X)
#define IOBUSCFG_DAI_71CH_PORT_MAX \
	(3)
#else
#define IOBUSCFG_DAI_71CH_PORT_MAX \
	(4)
#endif
#define IOBUSCFG_DAI_SRCH_PORT_MAX \
	(5)
#define IOBUSCFG_SPDIF_71CH_PORT_MAX \
	(6)
#define IOBUSCFG_SPDIF_SRCH_PORT_MAX \
	(6)

#if defined(CONFIG_ARCH_TCC803X)
static inline uint32_t iobuscfg_dai_chmux_sub_get_pos(int32_t id){
	uint32_t pos =
	    (id == 0) ?
		(uint32_t) IOBUS_CFGDAI_CHMUX71CH_0SEL_Pos : (id == 1) ?
		(uint32_t) IOBUS_CFGDAI_CHMUX71CH_1SEL_Pos : (id == 2) ?
		(uint32_t) IOBUS_CFGDAI_CHMUX71CH_2SEL_Pos : (id == 3) ?
		(uint32_t) IOBUS_CFGDAI_CHMUXSRCH_0SEL_Pos : (id == 4) ?
		(uint32_t) IOBUS_CFGDAI_CHMUXSRCH_1SEL_Pos : (id == 5) ?
		(uint32_t) IOBUS_CFGDAI_CHMUXSRCH_2SEL_Pos :
		(uint32_t) IOBUS_CFGDAI_CHMUXSRCH_3SEL_Pos;
	return pos;
}

static inline uint32_t iobuscfg_dai_chmux_sub_get_mask(int32_t id){
	uint32_t mask =
		(id == 0) ? IOBUS_CFGDAI_CHMUX71CH_0SEL_Msk :
		(id == 1) ? IOBUS_CFGDAI_CHMUX71CH_1SEL_Msk :
		(id == 2) ? IOBUS_CFGDAI_CHMUX71CH_2SEL_Msk :
		(id == 3) ? IOBUS_CFGDAI_CHMUXSRCH_0SEL_Msk :
		(id == 4) ? IOBUS_CFGDAI_CHMUXSRCH_1SEL_Msk :
		(id == 5) ? IOBUS_CFGDAI_CHMUXSRCH_2SEL_Msk :
		IOBUS_CFGDAI_CHMUXSRCH_3SEL_Msk;
	return mask;
}

static inline void iobuscfg_dai_chmux(void __iomem *iobuscfg_reg, int32_t id,
				      uint32_t port)
{
	ptrdiff_t offset = (id == 0) ? IOBUS_CFG_DAI_71CH_CHMUX :
	    (id == 1) ? IOBUS_CFG_DAI_71CH_CHMUX :
	    (id == 2) ? IOBUS_CFG_DAI_71CH_CHMUX : IOBUS_CFG_DAI_SRCH_CHMUX;

	uint32_t pos = iobuscfg_dai_chmux_sub_get_pos(id);
	uint32_t mask = iobuscfg_dai_chmux_sub_get_mask(id);
	uint32_t value;

	if (id <= 6) {
		value = readl(iobuscfg_reg + offset);
		value &= ~mask;

		if (((id < 3) && (port <= (uint32_t) IOBUSCFG_DAI_71CH_PORT_MAX)) ||
				((id >= 3) && (port <= (uint32_t) IOBUSCFG_DAI_SRCH_PORT_MAX))){
			value |= ui_lshift(ui_lshift(1u, port), pos);
		}

		chmux_writel(value, iobuscfg_reg + offset);
	}
}
#else
static inline uint32_t iobuscfg_dai_chmux_sub_get_pos(int32_t id){
	uint32_t pos =
		(id == 0) ? (uint32_t)IOBUS_CFGDAI_CHMUX71CH_0SEL_Pos :
		(id == 1) ? (uint32_t)IOBUS_CFGDAI_CHMUX71CH_1SEL_Pos :
		(id == 2) ? (uint32_t)IOBUS_CFGDAI_CHMUX71CH_2SEL_Pos :
		(id == 3) ? (uint32_t)IOBUS_CFGDAI_CHMUXSRCH_0SEL_Pos :
		(id == 4) ? (uint32_t)IOBUS_CFGDAI_CHMUXSRCH_1SEL_Pos :
		(id == 5) ? (uint32_t)IOBUS_CFGDAI_CHMUXSRCH_2SEL_Pos :
		(id == 6) ? (uint32_t)IOBUS_CFGDAI_CHMUXSRCH_3SEL_Pos :
		(uint32_t)IOBUS_CFGDAI_CHMUX71CH_3SEL_Pos;
	return pos;
}

static inline uint32_t iobuscfg_dai_chmux_sub_get_mask(int32_t id){
	uint32_t mask =
		(id == 0) ? IOBUS_CFGDAI_CHMUX71CH_0SEL_Msk :
		(id == 1) ? IOBUS_CFGDAI_CHMUX71CH_1SEL_Msk :
		(id == 2) ? IOBUS_CFGDAI_CHMUX71CH_2SEL_Msk :
		(id == 3) ? IOBUS_CFGDAI_CHMUXSRCH_0SEL_Msk :
		(id == 4) ? IOBUS_CFGDAI_CHMUXSRCH_1SEL_Msk :
		(id == 5) ? IOBUS_CFGDAI_CHMUXSRCH_2SEL_Msk :
		(id == 6) ? IOBUS_CFGDAI_CHMUXSRCH_3SEL_Msk :
		IOBUS_CFGDAI_CHMUX71CH_3SEL_Msk;
	return mask;
}

static inline void iobuscfg_dai_chmux(
	void __iomem *iobuscfg_reg,
	int32_t id,
	uint32_t port)
{
	ptrdiff_t offset =
		(id == 0) ? IOBUS_CFG_DAI_71CH_CHMUX :
		(id == 1) ? IOBUS_CFG_DAI_71CH_CHMUX :
		(id == 2) ? IOBUS_CFG_DAI_71CH_CHMUX :
		(id == 7) ? IOBUS_CFG_DAI_71CH_CHMUX :
		IOBUS_CFG_DAI_SRCH_CHMUX;

	uint32_t pos = iobuscfg_dai_chmux_sub_get_pos(id);
	uint32_t mask = iobuscfg_dai_chmux_sub_get_mask(id);
	uint32_t value;

	if (id <= 7) {
		value = readl(iobuscfg_reg + offset);
		value &= ~mask;

		if ((((id < 3) || (id == 7)) &&
					(port <= (uint32_t) IOBUSCFG_DAI_71CH_PORT_MAX)) ||
				((id >= 3) && (id != 7) &&
				 (port <= (uint32_t) IOBUSCFG_DAI_SRCH_PORT_MAX))) {
			value |= (((uint32_t) 1 << port) << pos);
		}

		chmux_writel(value, iobuscfg_reg + offset);
	}
}
#endif

static inline uint32_t iobuscfg_spdif_chmux_sub_get_pos(int32_t id){
	uint32_t pos =
	    (id == 0) ? (uint32_t) IOBUS_CFGSPDIF_CHMUX71CH_0SEL_Pos :
	    (id == 1) ? (uint32_t) IOBUS_CFGSPDIF_CHMUX71CH_1SEL_Pos :
		(id == 2) ? (uint32_t) IOBUS_CFGSPDIF_CHMUX71CH_2SEL_Pos :
		(id == 3) ? (uint32_t) IOBUS_CFGSPDIF_CHMUXSRCH_0SEL_Pos :
		(id == 4) ? (uint32_t) IOBUS_CFGSPDIF_CHMUXSRCH_1SEL_Pos :
		(id == 5) ? (uint32_t) IOBUS_CFGSPDIF_CHMUXSRCH_2SEL_Pos :
		(uint32_t) IOBUS_CFGSPDIF_CHMUXSRCH_3SEL_Pos;
	return pos;
}

static inline uint32_t iobuscfg_spdif_chmux_sub_get_mask(int32_t id){
	uint32_t mask =
		(id == 0) ? IOBUS_CFGSPDIF_CHMUX71CH_0SEL_Msk :
		(id == 1) ? IOBUS_CFGSPDIF_CHMUX71CH_1SEL_Msk :
		(id == 2) ? IOBUS_CFGSPDIF_CHMUX71CH_2SEL_Msk :
		(id == 3) ? IOBUS_CFGSPDIF_CHMUXSRCH_0SEL_Msk :
		(id == 4) ? IOBUS_CFGSPDIF_CHMUXSRCH_1SEL_Msk :
		(id == 5) ? IOBUS_CFGSPDIF_CHMUXSRCH_2SEL_Msk :
		IOBUS_CFGSPDIF_CHMUXSRCH_3SEL_Msk;
	return mask;
}

static inline void iobuscfg_spdif_chmux(
	void __iomem *iobuscfg_reg,
	int32_t id,
	uint32_t port)
{
	ptrdiff_t offset =
		(id == 0) ? IOBUS_CFG_SPDIF_71CH0_CHMUX :
		(id == 1) ? IOBUS_CFG_SPDIF_71CH0_CHMUX :
		(id == 2) ? IOBUS_CFG_SPDIF_71CH1_CHMUX :
		(id == 3) ? IOBUS_CFG_SPDIF_SRCH0_CHMUX :
		(id == 4) ? IOBUS_CFG_SPDIF_SRCH0_CHMUX :
		IOBUS_CFG_SPDIF_SRCH1_CHMUX;

 	uint32_t pos = iobuscfg_spdif_chmux_sub_get_pos(id);
	uint32_t mask = iobuscfg_spdif_chmux_sub_get_mask(id);
	uint32_t value;

	if (id <= 6) {
		value = readl(iobuscfg_reg + offset);
		value &= ~mask;

		if (((id < 3) &&
			(port <= (uint32_t) IOBUSCFG_SPDIF_71CH_PORT_MAX)) ||
			((id >= 3) &&
			(port <= (uint32_t) IOBUSCFG_SPDIF_SRCH_PORT_MAX))) {
			value |= (((uint32_t) 1 << port) << pos);
		}

		chmux_writel(value, iobuscfg_reg + offset);
	}
}

#elif defined(CONFIG_ARCH_TCC802X)
struct tcc_gfb_i2s_port {
	char clk[3];		//mclk, bclk, lrck
	char daout[4];		//dai0~3
	char dain[4];		//dao0~3
};

struct tcc_gfb_spdif_port {
	char port[2];		//tx, rx
};

struct tcc_gfb_cdif_port {
	char port[3];		//bclk, lrck, dai
};

static inline void tcc_gfb_dump_portcfg(void __iomem *pcfg_reg)
{
	pr_info("PCFG0 : 0x%08x\n", readl(pcfg_reg + TCC_AUDIO_PCFG0_OFFSET));
	pr_info("PCFG1 : 0x%08x\n", readl(pcfg_reg + TCC_AUDIO_PCFG1_OFFSET));
	pr_info("PCFG2 : 0x%08x\n", readl(pcfg_reg + TCC_AUDIO_PCFG2_OFFSET));
	pr_info("PCFG3 : 0x%08x\n", readl(pcfg_reg + TCC_AUDIO_PCFG3_OFFSET));
}

static inline void tcc_gfb_i2s_portcfg(
	void __iomem *pcfg_reg,
	struct tcc_gfb_i2s_port *port)
{
	uint32_t pcfg0 = readl(pcfg_reg + TCC_AUDIO_PCFG0_OFFSET);
	uint32_t pcfg1 = readl(pcfg_reg + TCC_AUDIO_PCFG1_OFFSET);
	uint32_t pcfg2 = readl(pcfg_reg + TCC_AUDIO_PCFG2_OFFSET);
	uint32_t val0 = 0, val1 = 0, val2 = 0, val3 = 0;

	pcfg0 &=
	    ~(PCFG0_DAI_DI0_Msk
		|PCFG0_DAI_DI1_Msk
		|PCFG0_DAI_DI2_Msk
		|PCFG0_DAI_DI3_Msk);
	pcfg1 &=
	    ~(PCFG1_DAI_LRCK_Msk
		|PCFG1_DAI_BCLK_Msk
		|PCFG1_DAI_DO0_Msk
		|PCFG1_DAI_DO1_Msk);
	pcfg2 &= ~(PCFG2_DAI_DO2_Msk | PCFG2_DAI_DO3_Msk | PCFG2_DAI_MCLK_Msk);

	val0 = ((port->dain[0] << PCFG0_DAI_DI0_Pos) & PCFG0_DAI_DI0_Msk);
	val1 = ((port->dain[1] << PCFG0_DAI_DI1_Pos) & PCFG0_DAI_DI1_Msk);
	val2 = ((port->dain[2] << PCFG0_DAI_DI2_Pos) & PCFG0_DAI_DI2_Msk);
	val3 = ((port->dain[3] << PCFG0_DAI_DI3_Pos) & PCFG0_DAI_DI3_Msk);
	pcfg0 |= (val0 | val1 | val2 | val3);

	val0 = ((port->daout[0] << PCFG1_DAI_DO0_Pos) & PCFG1_DAI_DO0_Msk);
	val1 = ((port->daout[1] << PCFG1_DAI_DO1_Pos) & PCFG1_DAI_DO1_Msk);
	val2 = ((port->clk[1] << PCFG1_DAI_BCLK_Pos) & PCFG1_DAI_BCLK_Msk);
	val3 = ((port->clk[2] << PCFG1_DAI_LRCK_Pos) & PCFG1_DAI_LRCK_Msk);
	pcfg1 |= (val0 | val1 | val2 | val3);

	val0 = ((port->daout[2] << PCFG2_DAI_DO2_Pos) & PCFG2_DAI_DO2_Msk);
	val1 = ((port->daout[3] << PCFG2_DAI_DO3_Pos) & PCFG2_DAI_DO3_Msk);
	val2 = ((port->clk[0] << PCFG2_DAI_MCLK_Pos) & PCFG2_DAI_MCLK_Msk);
	pcfg2 |= (val0 | val1 | val2);

	chmux_writel(pcfg0, pcfg_reg + TCC_AUDIO_PCFG0_OFFSET);
	chmux_writel(pcfg1, pcfg_reg + TCC_AUDIO_PCFG1_OFFSET);
	chmux_writel(pcfg2, pcfg_reg + TCC_AUDIO_PCFG2_OFFSET);
}

static inline void tcc_gfb_spdif_portcfg(
	void __iomem *pcfg_reg,
	struct tcc_gfb_spdif_port *port)
{
	uint32_t pcfg3 = readl(pcfg_reg + TCC_AUDIO_PCFG3_OFFSET);
	uint32_t val0 = 0, val1 = 0;

	pcfg3 &= ~(PCFG3_SPDIF_TX_Msk | PCFG3_SPDIF_RX_Msk);

	val0 = ((port->port[0] << PCFG3_SPDIF_TX_Pos) & PCFG3_SPDIF_TX_Msk);
	val1 = ((port->port[1] << PCFG3_SPDIF_RX_Pos) & PCFG3_SPDIF_RX_Msk);
	pcfg3 |= (val0 | val1);

	chmux_writel(pcfg3, pcfg_reg + TCC_AUDIO_PCFG3_OFFSET);
}

#endif /* CONFIG_ARCH_TCC802X */

#endif /*TCC_PORT_MUX_H */
