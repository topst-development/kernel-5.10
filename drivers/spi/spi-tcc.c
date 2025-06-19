// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/io.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>
#include <linux/spi/spidev.h>

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/string.h>
#include <asm/dma.h>
#include <linux/dmaengine.h>
#include <linux/module.h>
#include <linux/reset.h>

#include <linux/bits.h>

#include "spi-tcc.h"

static void tcc_spi_hwinit(const struct tcc_spi *tccspi);

static ssize_t tcc_spi_sync_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct spi_master *tcc_master = dev_get_drvdata(dev);
	struct tcc_spi *tccspi = spi_master_get_devdata(tcc_master);
	uint32_t status;

	status = (readl(tccspi->base + TCC_GPSB_STAT) & TCC_GPSB_STAT_CMDSTAT) >> 10;

	return sprintf(buf, "%u\n",status);
}

static DEVICE_ATTR_RO(tcc_spi_sync);

/* Print values of GPSB registers */
static void tcc_spi_regs_dump(const struct tcc_spi *tccspi)
{
	const struct tcc_spi_pl_data *pd = tccspi->pd;

	if (pd != NULL) {
		dev_dbg(tccspi->dev,
			"[DEBUG][SPI] ##\tGPSB REGS DUMP [CH: %d]\t##\n",
			pd->gpsb_channel);
	}

	dev_dbg(tccspi->dev,
		"[DEBUG][SPI] STAT\t: 0x%08X\n",
		readl(tccspi->base + TCC_GPSB_STAT));
	dev_dbg(tccspi->dev,
		"[DEBUG][SPI] INTEN\t: 0x%08X\n",
		readl(tccspi->base + TCC_GPSB_INTEN));
	dev_dbg(tccspi->dev,
		"[DEBUG][SPI] MODE\t: 0x%08X\n",
		readl(tccspi->base + TCC_GPSB_MODE));
	dev_dbg(tccspi->dev,
		"[DEBUG][SPI] CTRL\t: 0x%08X\n",
		readl(tccspi->base + TCC_GPSB_CTRL));
	dev_dbg(tccspi->dev,
		"[DEBUG][SPI] EVTCTRL\t: 0x%08X\n",
		readl(tccspi->base + TCC_GPSB_EVTCTRL));
	dev_dbg(tccspi->dev,
		"[DEBUG][SPI] CCV\t: 0x%08X\n",
		readl(tccspi->base + TCC_GPSB_CCV));

	if (tccspi->pd->is_using_gdma == (bool)false) {
		dev_dbg(tccspi->dev,
			"[DEBUG][SPI] TXBASE\t: 0x%08X\n",
			readl(tccspi->base + TCC_GPSB_TXBASE));
		dev_dbg(tccspi->dev,
			"[DEBUG][SPI] RXBASE\t: 0x%08X\n",
			readl(tccspi->base + TCC_GPSB_RXBASE));
		dev_dbg(tccspi->dev,
			"[DEBUG][SPI] PACKET\t: 0x%08X\n",
			readl(tccspi->base + TCC_GPSB_PACKET));
		dev_dbg(tccspi->dev,
			"[DEBUG][SPI] DMACTR\t: 0x%08X\n",
			readl(tccspi->base + TCC_GPSB_DMACTR));
		dev_dbg(tccspi->dev,
			"[DEBUG][SPI] DMASTR\t: 0x%08X\n",
			readl(tccspi->base + TCC_GPSB_DMASTR));
		dev_dbg(tccspi->dev,
			"[DEBUG][SPI] DMAICR\t: 0x%08X\n",
			readl(tccspi->base + TCC_GPSB_DMAICR));
	}
}

static uint32_t tcc_circq_cnt(const struct tcc_circ *circ)
{
	uint32_t ret;

	if (circ->head >= circ->tail) {
		ret = circ->head - circ->tail;
	} else {
		ret = TCC_GPSB_SLAVE_QUEUE_MAX_SIZE - circ->tail + circ->head;
	}

	return ret;
}

static uint32_t tcc_circq_space(const struct tcc_circ *circ)
{
	uint32_t ret;

	if (circ->tail > circ->head) {
		ret = circ->tail - circ->head - 1U;
	} else {
		ret = TCC_GPSB_SLAVE_QUEUE_MAX_SIZE - circ->head + circ->tail;
	}

	return ret;
}

static uint32_t tcc_circq_cnt_to_end(const struct tcc_circ *circ)
{
	uint32_t ret;

	if (circ->head >= circ->tail) {
		ret = circ->head - circ->tail;
	} else {
		ret = TCC_GPSB_SLAVE_QUEUE_MAX_SIZE - circ->tail;
	}

	return ret;
}

static uint32_t tcc_circq_space_to_end(const struct tcc_circ *circ)
{
	uint32_t ret;

	if (circ->tail > circ->head) {
		ret = circ->tail - circ->head - 1U;
	} else {
		ret = TCC_GPSB_SLAVE_QUEUE_MAX_SIZE - circ->head;
	}

	return ret;
}

static void tcc_buf_to_circq(struct tcc_circ *circ, const void *buf, uint32_t len)
{
	uint32_t srx_queue_eob_cnt;

	if ((len <= tcc_circq_space(circ)) &&
	    (len <= TCC_GPSB_SLAVE_QUEUE_MAX_SIZE)) {
		srx_queue_eob_cnt = tcc_circq_space_to_end(circ);
		if (srx_queue_eob_cnt >= len) {
			(void)memcpy(&circ->buf[circ->head],
					buf,
					len);
			circ->head += len;
		} else {
			(void)memcpy(&circ->buf[circ->head],
					buf,
					srx_queue_eob_cnt);
			len -= srx_queue_eob_cnt;
			circ->head = 0;

			(void)memcpy(&circ->buf[circ->head],
					(buf + srx_queue_eob_cnt),
					len);
			circ->head = len;
		}

		if (circ->head == TCC_GPSB_SLAVE_QUEUE_MAX_SIZE) {
			circ->head = 0;
		}
	} else {
		(void)pr_warn("[WARN][SPI] Slave rx buffer is fulled, size: %d, head: %d, tail: %d\n",
			tcc_circq_cnt(circ),
			circ->head,
			circ->tail);
	}
}

static int32_t tcc_circq_to_buf(struct tcc_circ *circ, void *buf, uint32_t len)
{
	uint32_t srx_queue_eob_cnt;
	int32_t ret = 0;

	if (tcc_circq_cnt(circ) >= len) {
		srx_queue_eob_cnt = tcc_circq_cnt_to_end(circ);
		if (srx_queue_eob_cnt >= len) {
			(void)memcpy(buf,
				&circ->buf[circ->tail],
				len);
			circ->tail += len;
		} else {
			(void)memcpy(buf,
				&circ->buf[circ->tail],
				srx_queue_eob_cnt);
			len -= srx_queue_eob_cnt;
			circ->tail = 0U;

			(void)memcpy(buf + srx_queue_eob_cnt,
				&circ->buf[circ->tail],
				len);
			circ->tail = len;
		}

		pr_debug("[DEBUG][SPI] rx buffer data copy end, remain: %d, head: %d, tail: %d\n",
			tcc_circq_cnt(circ),
			circ->head,
			circ->tail);
	} else {
		ret = -ENOSPC;
		pr_debug("[DEBUG][SPI] Slave rx buffer data is lacking, request: %d, remain: %d\n",
			len,
			tcc_circq_cnt(circ));
	}

	return ret;
}

/* Set TCC GPSB DMA Packet counter */
static void tcc_spi_set_packet_size(const struct tcc_spi *tccspi, uint32_t size)
{
	if (tccspi->pd->is_using_gdma == (bool)false) {
		writel(TCC_GPSB_PACKET_SIZE(size),
			tccspi->base + TCC_GPSB_PACKET);
		dev_dbg(tccspi->dev,
			"[DEBUG][SPI] [%s] size: %d packet reg 0x%08X\n",
			__func__,
			size,
			readl(tccspi->base + TCC_GPSB_PACKET));
	}
}

/* Clear Tx and Rx FIFO counter */
static int32_t tcc_spi_clear_fifo(const struct tcc_spi *tccspi)
{
	uint32_t status;
	int32_t ret = 0;

	status = tcc_spi_readl(tccspi->base + TCC_GPSB_STAT);
	if ((status & TCC_GPSB_STAT_CNT) != 0U) {
		dev_dbg(tccspi->dev,
			"[DEBUG][SPI] FIFO is not empty. (STAT: 0x%08X)\n",
			status);
		/* Clear FIFO : SW reset */
		clk_disable_unprepare(tccspi->pd->hclk);
		if (tccspi->pd->soc_info->id != TCC803X) {
			reset_control_assert(tccspi->pd->rst);
			reset_control_deassert(tccspi->pd->rst);
		}
		ret = clk_prepare_enable(tccspi->pd->hclk);
		if (ret < 0) {
			dev_err(tccspi->dev,
				"[ERROR][SPI] Failed to release swreset\n");
		} else {
			/* re-init GPSB */
			tcc_spi_hwinit(tccspi);
		}
	}

	return ret;
}

static void tcc_gpsb_set_pcfg(const struct tcc_spi *tccspi,
				uint32_t ch, uint32_t port)
{
	uint32_t val, shift, offset;
	void __iomem *port_reg;

	if (tccspi->pd->soc_info->id == TCN100X) {
		offset = ch * 4U;
		port_reg = tccspi->pcfg + offset;
		port &= 0x1FU;
		writel(port, port_reg);
	} else {
		if (ch < 4U) {
			port_reg = tccspi->pcfg + TCC_GPSB_PCFG0;
			shift = ch << 3U;
		} else {
			ch %= 4U;
			port_reg = tccspi->pcfg + TCC_GPSB_PCFG1;
			shift = ch << 3U;
		}

		val = readl(port_reg);
		if (shift < (sizeof(val) * (u8)BITS_PER_BYTE)) {
			val &= (~((u32)0xFF << shift));
			val |= (port << shift);
			writel(val, port_reg);
		}
	}
}

/* Get the port configuration */
static uint32_t tcc_spi_get_port(const struct tcc_spi *tccspi, uint32_t ch)
{
	uint32_t port, shift, offset;
	const void __iomem *port_reg;

	if (tccspi->pd->soc_info->id == TCN100X) {
		offset = ch * 4U;
		port_reg = tccspi->pcfg + offset;
		port = readl( port_reg);
	} else {
		if (ch < 4U) {
			port_reg = tccspi->pcfg + TCC_GPSB_PCFG0;
			shift = ch << 3U;
		} else {
			ch %= 4U;
			port_reg = tccspi->pcfg + TCC_GPSB_PCFG1;
			shift = ch << 3U;
		}

		port = readl(port_reg);
		port >>= shift;
		port &= 0xFFU;

		dev_dbg(tccspi->dev,
			"[DEBUG][SPI] [%s] channel: %d port: %d\n",
		 	__func__,
			ch,
			port);
	}
	return port;
}

/* Set the port configuration and check the port setting conflict */
static int32_t tcc_spi_set_port(const struct tcc_spi *tccspi)
{
	int32_t ret = 0;
	uint32_t i, port, port_conflict;
	const struct tcc_spi_pl_data *pd = tccspi->pd;

	if (pd == NULL) {
		dev_err(tccspi->dev,
			"[ERROR][SPI][%s] tcc_pl_data is null\n",
			__func__);
		ret = -ENXIO;
	}

	if (ret == 0) {
		if ((tccspi->pd->soc_info->id == TCC897X) ||
			(tccspi->pd->soc_info->id == TCC803X) ||
			(tccspi->pd->soc_info->id == TCC805X) ||
			(tccspi->pd->soc_info->id == TCC807X) ||
			(tccspi->pd->soc_info->id == TCN100X)) {

			port = pd->port;
			if (port > 0xFFU) {
				dev_err(tccspi->dev,
					"[ERROR][SPI] Invalid port(%d)\n",
					port);
				ret = -EINVAL;
			} else {
				/* Set the port */
				tcc_gpsb_set_pcfg(tccspi, pd->gpsb_channel, port);

				/* Check the port setting conflict */
				for (i = 0U; i <= pd->soc_info->last_ch; i++) {
					if (i == pd->gpsb_channel) {
						continue;
					}
					port_conflict = tcc_spi_get_port(tccspi, i);
					if (port_conflict == port) {
						tcc_gpsb_set_pcfg(tccspi, i, 0xFFU);
						dev_warn(tccspi->dev,
							"[WARN][SPI] port conflict! [[ch %d[%d]]] : ch %d[%d]\n",
							pd->gpsb_channel,
							port,
							i,
							port_conflict);
					}
				}
			}
		} else if (tccspi->pd->soc_info->id == TCC750X) {
			port_conflict = readl(tccspi->pcfg + TCC_GPSB_SDM_RVC_CTRL);

			if (tccspi->pd->is_srvc) {
				port_conflict = TCC_GPSB_RVC_SEL_MASK(port_conflict);
				/* When using gpsb controller in SRVC */
				if ((port_conflict != pd->gpsb_channel) &&
				    (port_conflict != TCC_GPSB_SRVC_DEFAULT_CH)) {
					dev_warn(tccspi->dev,
						"[WARN][SPI] GPSB Controller for RVC is already in use on channel [%d]\n",
						port_conflict);

				}
				writel(TCC_GPSB_RVC_SEL(pd->gpsb_channel),
				       tccspi->pcfg + TCC_GPSB_SDM_RVC_CTRL);
			} else {
				if ((TCC_GPSB_TDM_SEL_MASK(port_conflict)) == pd->gpsb_channel) {
					port_conflict |= TCC_GPSB_TDM_SEL(0x7);
					writel(port_conflict,
						tccspi->pcfg + TCC_GPSB_SDM_RVC_CTRL);
				}
				ret = 0;
			}
		} else {
			ret = -EINVAL;
		}
	}

	return ret;
}

/* Set the bit width */
static void tcc_spi_set_bit_width(const struct tcc_spi *tccspi, uint32_t width)
{
	uint32_t val, tmp = 1;

	if (width < tmp) {
		dev_warn(tccspi->dev,
			"[WARN][SPI]%s: not supported bpw(%d)\n",
			__func__, width);
	} else {
		val = width - tmp;
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_BWS(0xFFUL));
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_BWS(val));

		if (tccspi->pd->is_using_gdma == (bool)false) {
			TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_INTEN,
					(TCC_GPSB_INTEN_SHT
					 | TCC_GPSB_INTEN_SBT
					 | TCC_GPSB_INTEN_SHR
					 | TCC_GPSB_INTEN_SBR));
			/* Set the endian mode according to the bit-width */
			if ((val & ((u32)1U << 4)) > 0U) {
				TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMACTR,
						TCC_GPSB_DMACTR_END);
			} else {
				TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_DMACTR,
						TCC_GPSB_DMACTR_END);
				if (width == 16U) {
					TCC_GPSB_BITSET(
						tccspi->base + TCC_GPSB_INTEN,
						(TCC_GPSB_INTEN_SBT |
						 TCC_GPSB_INTEN_SBR));
				}
			}
		}

		dev_dbg(tccspi->dev,
			"[DEBUG][SPI] [%s] bitwidth: %d endian: %u\n",
			__func__, width, (val & (u32)BIT(4)));
	}
}

/* Set the SCLK */
static int32_t tcc_spi_set_clk(const struct tcc_spi *tccspi, uint32_t sclk,
		uint32_t divldv)
{
	uint32_t pclk, clk_, tmp;
	int32_t ret = 0;
	const struct tcc_spi_pl_data *pd = tccspi->pd;

	if (sclk == 0U) {
		dev_err(tccspi->dev,
			"[ERROR][SPI] clk err (%u)\n",
			sclk);
		ret = -EINVAL;
	}

	if (pd == NULL) {
		dev_err(tccspi->dev,
			"[ERROR][SPI] pl data is null!\n");
		ret = -EINVAL;
	} else {
		if (pd->pclk == NULL) {
			dev_err(tccspi->dev,
				"[ERROR][SPI] pclk is null!!\n");
			ret = -EINVAL;
		}
	}

	if (ret == 0) {
		clk_ = sclk;

		/* Set the GPSB clock divider load value */
		TCC_GPSB_BITCSET(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_DIVLDV(0xFFUL),
				TCC_GPSB_MODE_DIVLDV(divldv));

		/* Calculate Peri Clock */
		tmp = 1U;
		if (((UINT_MAX - tmp) < divldv) ||
		   ((UINT_MAX / clk_) < (divldv + tmp))) {
			ret = -EIO;
		} else {
			pclk = (clk_ * (divldv + tmp)) << 1U;
			ret = clk_set_rate(pd->pclk, pclk);
			if (ret == 0) {
				dev_dbg(tccspi->dev,
					"[DEBUG][SPI][%s] sclk: %dHz divldv: %d pclk: %luHz\n",
					__func__,
					clk_,
					divldv,
					clk_get_rate(pd->pclk));
			}
		}
	}

	return ret;
}

/* Set SPI modes */
static void tcc_spi_set_mode(const struct tcc_spi *tccspi, uint32_t mode)
{
	if (spi_controller_is_slave(tccspi->tcc_master)) {
		/* slave mode */
		if ((mode == (u32)SPI_CPHA) || (mode == (u32)SPI_CPOL)) {
			/* SPI MODE 1, 2 */
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_PCK);
		} else {
			/* SPI MODE 0, 3 */
			TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_PCK);
		}
	} else {
		/* master mode */
		if ((mode & (u16)SPI_CPOL) != 0U) {
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_PCK);
		} else {
			TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_PCK);
		}
		if ((mode & (u16)SPI_CPHA) != 0U) {
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_PWD | TCC_GPSB_MODE_PRD);
		} else {
			TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_PWD | TCC_GPSB_MODE_PRD);
		}
		if (tccspi->pd->recovery_time) {
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_TRE);
		}
		if (tccspi->pd->hold_time) {
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_THL);
		}
		if (tccspi->pd->setup_time) {
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_TSU);
		}
	}
	if ((mode & (u16)SPI_CS_HIGH) != 0U) {
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_PCS | TCC_GPSB_MODE_PCD);
	} else {
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_PCS | TCC_GPSB_MODE_PCD);
	}
	if ((mode & (u16)SPI_LSB_FIRST) != 0U) {
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_SD);
	} else {
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_SD);
	}
	if ((mode & (u16)SPI_LOOP) != 0U) {
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_LB);
	} else {
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_LB);
	}
	if (tccspi->pd->prd) {
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_PRD);
	}

	dev_dbg(tccspi->dev,
		"[DEBUG][SPI] [%s] mode: 0x%X (mode reg: 0x%08X)\n",
		__func__,
		mode,
		tcc_spi_readl(tccspi->base + TCC_GPSB_MODE));
}

/* Set TCC GPSB DMA Tx and Rx base address and DMA request */
static void tcc_spi_set_dma_addr(const struct tcc_spi *tccspi,
		dma_addr_t tx, dma_addr_t rx)
{
	if (tccspi->pd->is_using_gdma == (bool)false) {
		/* Set Base address */
		writel((tx & 0xFFFFFFFFU),
			tccspi->base + TCC_GPSB_TXBASE);
		writel((rx & 0xFFFFFFFFU),
			tccspi->base + TCC_GPSB_RXBASE);

#ifdef CONFIG_ARCH_TCN100X
		if (tccspi->pd->soc_info->id == TCN100X) {
			writel(((tx & 0xF00000000) >> 32),
					tccspi->base + TCC_GPSB_TXBASE_H);
			writel(((rx & 0xF00000000) >> 32),
					tccspi->base + TCC_GPSB_RXBASE_H);
		}
#endif
	}

	/* Set DMA request */
	if (tx != 0U) {
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_INTEN,
				TCC_GPSB_INTEN_DW);
	} else {
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_INTEN,
				TCC_GPSB_INTEN_DW);
	}
	if (rx != 0U) {
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_INTEN,
				TCC_GPSB_INTEN_DR);
	} else {
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_INTEN,
				TCC_GPSB_INTEN_DR);
	}
}

/* Initialize GPSB register settings */
static void tcc_spi_hwinit(const struct tcc_spi *tccspi)
{
	/* Reset GPSB registers */
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_STAT, 0xFFFFFFFFU);
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_INTEN, 0xFFFFFFFFU);
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, 0xFFFFFFFFU);
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_CTRL, 0xFFFFFFFFU);
	if (tccspi->pd->is_using_gdma == (bool)false) {
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_TXBASE, 0xFFFFFFFFU);
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_RXBASE, 0xFFFFFFFFU);
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_PACKET, 0xFFFFFFFFU);
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMACTR, 0xFFFFFFFFU);
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMASTR, 0xFFFFFFFFU);
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMAICR, 0xFFFFFFFFU);
	}

	/* Check CONTM support */
	if (tccspi->pd->soc_info->id == TCC897X) {
		tccspi->pd->contm_support = (bool)false;
	} else {
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_EVTCTRL,
				TCC_GPSB_EVTCTRL_CONTM(0x3U));
		tccspi->pd->contm_support = (bool)true;
	}

	/* Disable operation */
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_EN);

	/* Set bitwidth (Default: 8) */
	tcc_spi_set_bit_width(tccspi, tccspi->bits);

	/* Set operation mode (SPI compatible) */
	TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_MD_MASK);

	if (tccspi->pd->ctf) {
		/* Set CTF Mode */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_CTF);
	}

	if (tccspi->pd->sdoe) {
		/* Set SDOE */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_EVTCTRL,
				TCC_GPSB_EVTCTRL_SDOE);
	}

	/* Set Tx and Rx FIFO threshold for interrupt/DMA request */
	TCC_GPSB_BITCSET(tccspi->base + TCC_GPSB_INTEN,
			TCC_GPSB_INTEN_CFGRTH_MASK,
			TCC_GPSB_INTEN_CFGRTH(tccspi->pd->cfgrth));
	TCC_GPSB_BITCSET(tccspi->base + TCC_GPSB_INTEN,
			TCC_GPSB_INTEN_CFGWTH_MASK,
			TCC_GPSB_INTEN_CFGWTH(tccspi->pd->cfgwth));

	if (spi_controller_is_slave(tccspi->tcc_master)) {
		/* Set SPI slave mode */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_SLV);
	} else {
		/* Set SPI master mode */
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_SLV);
	}

	tcc_spi_set_mode(tccspi, tccspi->mode);

	/* Set TXBASE and RXBASE registers */
	tcc_spi_set_dma_addr(tccspi,
			tccspi->tx_buf.dma_addr,
			tccspi->rx_buf.dma_addr);


	/* Enable operation */
	TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_EN);
}

/* Stop transfer */
static void tcc_spi_stop_dma(const struct tcc_spi *tccspi)
{
	if (tccspi->pd->is_using_gdma == (bool)false) {
		/* Clear DMA done and packet interrupt status */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_DMAICR,
				TCC_GPSB_DMAICR_ISD | TCC_GPSB_DMAICR_ISP);

		/* Disable GPSB DMA operation */
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMACTR,
				TCC_GPSB_DMACTR_EN);

		/* Disable DMA Tx and Rx request */
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMACTR,
				TCC_GPSB_DMACTR_DTE | TCC_GPSB_DMACTR_DRE);
	}
}

/* Start transfer */
static void tcc_spi_start_dma(const struct tcc_spi *tccspi)
{
	if (tccspi->pd->is_using_gdma == (bool)false) {
		/* Set GPSB DMA address mode (Multiple address mode) */
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMACTR,
				(TCC_GPSB_DMACTR_RXAM_MASK |
				 TCC_GPSB_DMACTR_TXAM_MASK));

		/* Enable DMA Tx and Rx request */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_DMACTR,
				(TCC_GPSB_DMACTR_DTE |
				 TCC_GPSB_DMACTR_DRE));

		/* Enable DMA done interrupt */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_DMAICR,
				TCC_GPSB_DMAICR_IED);

		/* Disable DMA packet interrupt */
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMAICR,
				TCC_GPSB_DMAICR_IEP);

		/* Set DMA Rx interrupt */
		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_DMAICR,
				TCC_GPSB_DMAICR_IRQS);

		/* Enable GPSB DMA operation */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_DMACTR,
				TCC_GPSB_DMACTR_EN);
	}
}

/* Allocate dma buffer */
static int32_t tcc_spi_init_dma_buf(struct tcc_spi * const tccspi,
				    int32_t dma_to_mem)
{
	void *v_addr;
	dma_addr_t dma_addr;
	struct device *dev;
	int32_t ret = 0;

	dev = tccspi->dev;

	v_addr = dma_alloc_coherent(dev,
			tccspi->dma_buf_size,
			&dma_addr,
			GFP_KERNEL);
	if (v_addr == NULL) {
		dev_err(tccspi->dev,
			"[ERROR][SPI] Fail to allocate the dma buffer\n");
		ret = -ENOMEM;
	} else {
		if (dma_to_mem != 0) {
			tccspi->rx_buf.v_addr = v_addr;
			tccspi->rx_buf.dma_addr = dma_addr;
			tccspi->rx_buf.size = tccspi->dma_buf_size;
		} else {
			tccspi->tx_buf.v_addr = v_addr;
			tccspi->tx_buf.dma_addr = dma_addr;
			tccspi->tx_buf.size = tccspi->dma_buf_size;
		}

		dev_dbg(tccspi->dev,
			"[DEBUG][SPI] [%s] dma_to_mem: %d v_addr: %p dma_addr: 0x%08X size: %zu\n",
			__func__, dma_to_mem,
			v_addr,
			(u32)dma_addr,
			tccspi->dma_buf_size);
	}

	return ret;
}

/* De-allocate dma buffer */
static void tcc_spi_deinit_dma_buf(const struct tcc_spi *tccspi,
				   int32_t dma_to_mem)
{
	void *v_addr;
	dma_addr_t dma_addr;


	if (dma_to_mem != 0) {
		v_addr = tccspi->rx_buf.v_addr;
		dma_addr = tccspi->rx_buf.dma_addr;
	} else {
		v_addr = tccspi->tx_buf.v_addr;
		dma_addr = tccspi->tx_buf.dma_addr;
	}

	dma_free_coherent(tccspi->dev,
			tccspi->dma_buf_size,
			v_addr,
			dma_addr);

	dev_dbg(tccspi->dev,
		"[DEBUG][SPI] [%s] dma_to_mem: %d v_addr: %p dma_addr: 0x%08X size: %zu\n",
		__func__,
		dma_to_mem,
		v_addr,
		(u32)dma_addr,
		tccspi->dma_buf_size);
}

/* Copy client buf to spi bux (tx) */
static void tcc_spi_txbuf_copy_client_to_spi(struct tcc_spi *tccspi,
		const struct spi_transfer *xfer, uint32_t len)
{
	if (xfer->tx_buf == NULL) {
		(void)memset(tccspi->tx_buf.v_addr, 0, len);
	} else {
		(void)memcpy(tccspi->tx_buf.v_addr,
				xfer->tx_buf + tccspi->cur_tx_pos,
				len);
		if ((UINT_MAX - len) < tccspi->cur_tx_pos) {
			dev_warn(tccspi->dev,
				"[WARN][SPI] %s: len(%d) is too long\n",
				__func__, len);
		}
		tccspi->cur_tx_pos += len;

		dev_dbg(tccspi->dev,
			"[DEBUG][SPI][%s] tx - client_buf: %p offset: %#X spi_buf: %p\n",
			__func__,
			xfer->tx_buf,
			tccspi->cur_tx_pos,
			tccspi->tx_buf.v_addr);
	}
}

/* Copy client buf to spi bux (rx) */
static void tcc_spi_rxbuf_copy_client_to_spi(struct tcc_spi *tccspi,
		struct spi_transfer *xfer, uint32_t len)
{
	struct tcc_circ *circ = &(tccspi->srx_queue);
	unsigned long flags;

	if ((xfer->rx_buf != NULL)  &&
	    (len <= TCC_SPI_DMA_MAX_SIZE)) {
		if (tccspi->pd->srx_queue_enable) {
			spin_lock_irqsave(&tccspi->srx_lock, flags);
			if (tcc_circq_to_buf(circ, xfer->rx_buf, len) < 0) {
				xfer->len = 0;
			}

			spin_unlock_irqrestore(&tccspi->srx_lock, flags);
		} else {
			(void)memcpy(xfer->rx_buf + tccspi->cur_rx_pos,
					tccspi->rx_buf.v_addr,
					len);
			if ((UINT_MAX - len) < tccspi->cur_rx_pos) {
				dev_warn(tccspi->dev,
					"[WARN][SPI] %s: len(%d) is too long\n",
					__func__, len);
			}
			tccspi->cur_rx_pos += len;

			dev_dbg(tccspi->dev, "[DEBUG][SPI][%s] rx - client_buf: %p offset: %#X spi_buf: %p\n",
					__func__,
					xfer->rx_buf,
					tccspi->cur_rx_pos,
					tccspi->rx_buf.v_addr);
		}
	} else {
		dev_err(tccspi->dev,
			"[ERROR][SPI] Exceeded maximum data length [max: %d, req: %d]\n",
			TCC_SPI_DMA_MAX_SIZE,
			len);
	}
}

/* Check channel DMA IRQ status */
static uint32_t tcc_spi_check_dma_irq_status(const struct tcc_spi *tccspi)
{
	const struct tcc_spi_pl_data *pd = tccspi->pd;
	uint32_t val, offset;
	uint32_t ret;

	if (pd->gpsb_channel <= pd->soc_info->last_ch) {
		val = readl(tccspi->pcfg + TCC_GPSB_CIRQST);

		/* Check dma irq status */
		offset = (pd->gpsb_channel * 2U) + 1U;
		val >>= offset;

		/* DMA IRQ (TX/RX) */
		ret = val & 0x01U;

		dev_dbg(tccspi->dev,
			"[DEBUG][SPI] [%s] irq_status: %d\n",
			__func__, ret);
	} else {
		ret = 0;
	}

	return ret;
}

static void tcc_spi_set_sync(const struct tcc_spi *tccspi, int set)
{
	if (!IS_ERR_OR_NULL((void *)tccspi->pd->sync_gpio)) {
		gpiod_set_value(tccspi->pd->sync_gpio, set);
	}
}

/* TCC GPSB FIFO(RX)/DMA(TX) IRQ Handler */
static irqreturn_t tcc_spi_halfdup_irq(int32_t irq, void *data)
{
	struct spi_master *tcc_master = (struct spi_master *)data;
	struct tcc_spi *tccspi = (struct tcc_spi *)spi_master_get_devdata(tcc_master);
	struct tcc_circ *circ = &(tccspi->srx_queue);
	uint32_t dmaicr, irq_status;
	irqreturn_t ret = IRQ_HANDLED;

	if (tccspi == NULL) {
		(void)pr_err("[ERROR][SPI] [%s] tccspi is null (irq: %d)\n",
				__func__, irq);
		ret = IRQ_NONE;
	} else {
		if (tccspi->pd->soc_info->id == TCC750X) {
			irq_status = 1U;
		} else {
			irq_status = tcc_spi_check_dma_irq_status(tccspi);
		}

		/* Check dma irq status */
		if (irq_status == 0U) {
			ret = IRQ_NONE;
		} else {
			dmaicr = tcc_spi_readl(tccspi->base + TCC_GPSB_DMAICR);
			if ((dmaicr & TCC_GPSB_DMAICR_ISD) > 0U) {
				dev_dbg(tccspi->dev,
					"[DEBUG][SPI] [%s] irq %d - channel :%d\n",
					__func__,
					irq,
					tccspi->pd->gpsb_channel);

				/* Stop dma operation to handle buffer */
				tcc_spi_stop_dma(tccspi);

				if (tccspi->pd->is_xfer_tx == (bool)true) {
					complete(&tccspi->xfer_complete);
				} else {
					/* Copy spi rxbuf to client rxbuf */
					spin_lock(&tccspi->srx_lock);
					tcc_buf_to_circq(circ,
							tccspi->rx_buf.v_addr,
							tccspi->pd->packet_size);
					spin_unlock(&tccspi->srx_lock);

					/* Start Tx and Rx DMA operation */
					tcc_spi_start_dma(tccspi);
				}
			}
		}
	}

	return ret;
}

/* TCC GPSB DMA IRQ Handler */
static irqreturn_t tcc_spi_dma_irq(int32_t irq, void *data)
{
	struct tcc_spi *tccspi =
		(struct tcc_spi *)spi_master_get_devdata((struct spi_master *)data);
	uint32_t dmaicr, reg_data, irq_status;
	irqreturn_t ret = IRQ_HANDLED;

	if (tccspi == NULL) {
		(void)pr_err("[ERROR][SPI] [%s] tccspi is null (irq: %d)\n",
				__func__, irq);
		ret = IRQ_NONE;
	} else {
		if ((tccspi->pd->soc_info->id == TCC750X) ||
		   (tccspi->pd->soc_info->id == TCC807X) ||
		   (tccspi->pd->soc_info->id == TCN100X)) {
			irq_status = 1U;
		} else {
			irq_status = tcc_spi_check_dma_irq_status(tccspi);
		}

		/* Check dma irq status*/
		if (irq_status == 0U) {
			ret = IRQ_NONE;
		} else {
			/* Check GPSB error flag status */
			reg_data = tcc_spi_readl(tccspi->base + TCC_GPSB_STAT);
			if ((reg_data & TCC_GPSB_STAT_ERR) > 0U) {
				dev_warn(tccspi->dev,
					"[WARN][SPI] [%s] Slave/FIFO error flag (status: 0x%08X)\n",
					__func__, reg_data);
			}

			/* Handle DMA done interrupt */
			dmaicr = readl(tccspi->base + TCC_GPSB_DMAICR);
			dev_dbg(tccspi->dev, "[DEBUG][SPI] [%s] dmaicr: 0x%08X\n",
					__func__, dmaicr);
			if ((dmaicr & ((u32)TCC_GPSB_DMAICR_ISD)) > 0U) {
				dev_dbg(tccspi->dev,
					"[DEBUG][SPI] [%s] irq %d - channel :%d\n",
					__func__,
					irq,
					tccspi->pd->gpsb_channel);
				/* Stop dma operation to hanlde buffer */
				tcc_spi_stop_dma(tccspi);

				tcc_spi_set_sync(tccspi, 1);
				complete(&tccspi->xfer_complete);
			}
		}
	}

	return ret;
}

/****** DMA-engine specific ******/
#define TCC_SPI_GDMA_WSIZE	1 /* Default word size */
#define TCC_SPI_GDMA_SG_LEN	1

/* Release dma-eninge channel */
static void tcc_spi_release_dma_engine(struct tcc_spi *tccspi)
{
	if (tccspi->dma.chan_tx != NULL) {
		dma_release_channel(tccspi->dma.chan_tx);
		tccspi->dma.chan_tx = NULL;
	}
	if (tccspi->dma.chan_rx != NULL) {
		dma_release_channel(tccspi->dma.chan_rx);
		tccspi->dma.chan_rx = NULL;
	}

}

/* dma-engine filter */
static bool tcc_spi_dma_engine_filter(struct dma_chan *chan, void *pdata)
{
	const struct tcc_spi_gdma *tccdma = pdata;
	struct device *dma_dev;
	bool ret = (bool)true;

	if (tccdma == NULL) {
		dev_err(chan->device->dev,
			"[ERROR][SPI] [%s] tcc_spi_gdma is NULL!!\n",
			__func__);
		ret = (bool)false;
	} else {
		dma_dev = tccdma->dma_dev;
		if (dma_dev == chan->device->dev) {
			chan->private = dma_dev;
			ret = (bool)true;
		} else {
			dev_err(chan->device->dev,
				"[ERROR][SPI] dma_dev(%p) != dev(%p)\n",
				dma_dev, chan->device->dev);
			ret = (bool)false;
		}
	}

	return ret;
}

/* Terminate all dma-engine channel */
static void tcc_spi_stop_dma_engine(const struct tcc_spi *tccspi)
{
	if (tccspi->dma.chan_tx != NULL) {
		(void)dmaengine_terminate_all(tccspi->dma.chan_tx);
	}
	if (tccspi->dma.chan_rx != NULL) {
		(void)dmaengine_terminate_all(tccspi->dma.chan_rx);
	}
}

/* dma-engine tx channel callback */
static void tcc_dma_engine_tx_callback(void *data)
{
	struct spi_master *tcc_master = data;
	const struct tcc_spi *tccspi = spi_master_get_devdata(tcc_master);
	uint32_t status;

	if (tccspi == NULL) {
		(void)pr_err("[ERROR][SPI] [%s] tcc_spi is NULL!!\n", __func__);
	} else {
		dev_dbg(tccspi->dev,
			"[DEBUG][SPI] [%s] gpsb channel %d\n",
			__func__, tccspi->pd->gpsb_channel);

		/* Check GPSB error flag status */
		status = tcc_spi_readl(tccspi->base + TCC_GPSB_STAT);
		if ((status & TCC_GPSB_STAT_ERR) > 0U) {
			dev_warn(tccspi->dev,
				"[WARN][SPI] [%s] Slave/FIFO error flag (status: 0x%08X)\n",
				__func__, status);
		}

		/* CS is de-active already */
		if (tccspi->pd->ctf) {
			TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_CTF);
		}
	}
}

/* dma-engine rx channel callback */
static void tcc_dma_engine_rx_callback(void *data)
{
	struct spi_master *tcc_master = data;
	struct tcc_spi *tccspi = spi_master_get_devdata(tcc_master);
	uint32_t status;

	if (tccspi == NULL) {
		(void)pr_err("[ERROR][SPI] [%s] tcc_spi is NULL!!\n", __func__);
	} else {
		dev_dbg(tccspi->dev,
			"[DEBUG][SPI] [%s] gpsb channel %d\n",
			__func__, tccspi->pd->gpsb_channel);

		/* Check GPSB error flag status */
		status = tcc_spi_readl(tccspi->base + TCC_GPSB_STAT);
		if ((status & TCC_GPSB_STAT_ERR) > 0U) {
			dev_warn(tccspi->dev,
				"[WARN][SPI] [%s] Slave/FIFO error flag (status: 0x%08X)\n",
				__func__, status);
		}

		/* Stop dma operation */
		tcc_spi_stop_dma(tccspi);
		complete(&tccspi->xfer_complete);
	}
}

/* Set dma-engine slave config word size */
static void tcc_spi_dma_engine_slv_cfg_addr_width
(struct dma_slave_config *slave_config, u8 bpw, int32_t src)
{
	// Set WSIZE
	if (src > 0) {
		slave_config->dst_addr_width = DMA_SLAVE_BUSWIDTH_UNDEFINED;
	} else {
		slave_config->src_addr_width = DMA_SLAVE_BUSWIDTH_UNDEFINED;
	}
	if (bpw == 4U) {
		if (src > 0) {
			slave_config->src_addr_width =
				DMA_SLAVE_BUSWIDTH_4_BYTES;
		} else {
			slave_config->dst_addr_width =
				DMA_SLAVE_BUSWIDTH_4_BYTES;
		}
	} else if (bpw == 2U) {
		if (src > 0) {
			slave_config->src_addr_width =
				DMA_SLAVE_BUSWIDTH_2_BYTES;
		} else {
			slave_config->dst_addr_width =
				DMA_SLAVE_BUSWIDTH_2_BYTES;
		}
	} else {
		if (src > 0) {
			slave_config->src_addr_width =
				DMA_SLAVE_BUSWIDTH_1_BYTE;
		} else {
			slave_config->dst_addr_width =
				DMA_SLAVE_BUSWIDTH_1_BYTE;
		}
	}
}

/* Configure dma-engine slaves */
static int32_t tcc_spi_dma_engine_slv_cfg
(const struct tcc_spi *tccspi, struct dma_slave_config *slave_config, u8 bpw)
{
	int32_t ret, error = 0;

	/* Set Busrt size(BSIZE) */
	slave_config->dst_maxburst = tccspi->pd->dma_bsize;
	slave_config->src_maxburst = tccspi->pd->dma_bsize;

	/* Set source and destincation address */
	slave_config->dst_addr = (dma_addr_t)(tccspi->pbase); /* GPSB PORT */
	slave_config->src_addr = (dma_addr_t)(tccspi->pbase); /* GPSB PORT */

	/* Set tx channel */
	slave_config->direction = DMA_MEM_TO_DEV;
	tcc_spi_dma_engine_slv_cfg_addr_width(slave_config, bpw, 1);
	ret = dmaengine_slave_config(tccspi->dma.chan_tx, slave_config);
	if (ret < 0) {
		dev_err(tccspi->dev,
			"[ERROR][SPI] Failed to configrue tx dma channel.\n");
		error = ret;
	}

	/* Set rx channel */
	slave_config->direction = DMA_DEV_TO_MEM;
	tcc_spi_dma_engine_slv_cfg_addr_width(slave_config, bpw, 0);
	ret = dmaengine_slave_config(tccspi->dma.chan_rx, slave_config);
	if (ret < 0) {
		dev_err(tccspi->dev,
			"[ERROR][SPI] Fail to configrue rx dma channel\n");
		error = ret;
	}

	return error;

}

/* Submit dma-engine descriptor */
static int32_t tcc_spi_dma_engine_submit(struct tcc_spi *tccspi, u32 flen)
{
	struct dma_chan *txchan = tccspi->dma.chan_tx;
	struct dma_chan *rxchan = tccspi->dma.chan_rx;
	struct dma_async_tx_descriptor *txdesc;
	struct dma_async_tx_descriptor *rxdesc;
	struct dma_slave_config slave_config;
	dma_cookie_t cookie;
	u32 len, bits_per_word;
	u8 bpw;
	int32_t ret = 0;

	if ((rxchan == NULL) || (txchan == NULL)) {
		dev_err(tccspi->dev,
			"[ERROR][SPI] rxchan(%p) or txchan(%p) are NULL\n",
			rxchan, txchan);
		ret = -ENODEV;
	}

	if (ret == 0) {
		bits_per_word = readl(tccspi->base + TCC_GPSB_MODE);
		bits_per_word = ((bits_per_word &
				TCC_GPSB_MODE_BWS_MASK) >> 8U) + 1U;
		bpw = ((u8)bits_per_word / 8U);
		len = flen / bpw;
		if (len == 0UL) {
			dev_err(tccspi->dev,
				"[ERROR][SPI] dma xfer len err, bpw %d len %d\n",
				bits_per_word, len);
			ret = -EINVAL;
		}
	}

	if (ret == 0) {
		/* Set CTF Mode */
		if (tccspi->pd->ctf) {
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_CTF);
		}
		/* Prepare the TX dma transfer */
		sg_init_table(&tccspi->dma.sgtx, TCC_SPI_GDMA_SG_LEN);
		sg_dma_len(&tccspi->dma.sgtx) = len;
		sg_dma_address(&tccspi->dma.sgtx) = tccspi->tx_buf.dma_addr;

		/* Prepare the RX dma transfer */
		sg_init_table(&tccspi->dma.sgrx, TCC_SPI_GDMA_SG_LEN);
		sg_dma_len(&tccspi->dma.sgrx) = len;
		sg_dma_address(&tccspi->dma.sgrx) = tccspi->rx_buf.dma_addr;

		/* Config dma slave */
		ret = tcc_spi_dma_engine_slv_cfg(tccspi, &slave_config, bpw);
		if (ret < 0) {
			dev_err(tccspi->dev,
				"[ERROR][SPI] Slave config failed.\n");
			ret = -EIO;
		}
	}

	if (ret == 0) {
		/* Send scatterlists */
		txdesc = dmaengine_prep_slave_sg(txchan, &tccspi->dma.sgtx,
				TCC_SPI_GDMA_SG_LEN,
				DMA_MEM_TO_DEV,
				(DMA_PREP_INTERRUPT | DMA_CTRL_ACK));
		if (txdesc == NULL) {
			dev_err(tccspi->dev,
				"[ERROR][SPI] Failed preparing TX DMA Desc.\n");
			ret = -ENOMEM;
		}
	}

	if (ret == 0) {
		txdesc->callback = tcc_dma_engine_tx_callback;
		txdesc->callback_param = tccspi->tcc_master;

		rxdesc = dmaengine_prep_slave_sg(rxchan, &tccspi->dma.sgrx,
				TCC_SPI_GDMA_SG_LEN,
				DMA_DEV_TO_MEM,
				DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
		if (rxdesc == NULL) {
			dev_err(tccspi->dev,
				"[ERROR][SPI] Failed preparing RX DMA Desc.\n");
			ret = -ENOMEM;
		}
	}

	if (ret == 0) {
		rxdesc->callback = tcc_dma_engine_rx_callback;
		rxdesc->callback_param = tccspi->tcc_master;

		/* GPSB half-word and byte swap settings */
		if (bpw == 4U) {
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_INTEN,
					(TCC_GPSB_INTEN_SHT |
					 TCC_GPSB_INTEN_SBT |
					 TCC_GPSB_INTEN_SHR |
					 TCC_GPSB_INTEN_SBR));
		} else if (bpw == 2U) {
			TCC_GPSB_BITCSET(tccspi->base + TCC_GPSB_INTEN,
					(TCC_GPSB_INTEN_SHT |
					 TCC_GPSB_INTEN_SHR),
					(TCC_GPSB_INTEN_SBT |
					 TCC_GPSB_INTEN_SBR));
		} else {
			TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_INTEN,
					(TCC_GPSB_INTEN_SHT |
					 TCC_GPSB_INTEN_SBT |
					 TCC_GPSB_INTEN_SHR |
					 TCC_GPSB_INTEN_SBR));
		}

		/* Submit desctriptors */
		cookie = dmaengine_submit(txdesc);
		ret = dma_submit_error(cookie);
		if (ret != 0) {
			dev_err(tccspi->dev,
				"[ERROR][SPI] TX Desc. submit error (cookie:%X)\n",
				cookie);
			ret = -ENOMEM;
		}
	}

	if (ret == 0) {
		cookie = dmaengine_submit(rxdesc);
		ret = dma_submit_error(cookie);
		if (ret != 0) {
			dev_err(tccspi->dev,
				"[ERROR][SPI] RX Desc. submit error (cookie:%X)\n",
				cookie);
			ret = -ENOMEM;
		}
	}

	if (ret == 0) {
		// Issue pendings
		dma_async_issue_pending(rxchan);
		dma_async_issue_pending(txchan);
	}

	if (ret == -ENOMEM) {
		/* Stop dma */
		tcc_spi_stop_dma_engine(tccspi);
		dev_err(tccspi->dev,
			"[ERROR][SPI] terminate dma-engine\n");
	}

	return ret;
}

static int32_t tcc_spi_dma_chan_set(struct tcc_spi *tccspi)
{
	int32_t ret = 0;
	dma_cap_mask_t mask;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	tccspi->dma.chan_tx = dma_request_slave_channel_compat(mask,
			tcc_spi_dma_engine_filter,
			&tccspi->dma,
			tccspi->dev,
			"tx");
	if (tccspi->dma.chan_tx == NULL) {
		dev_err(tccspi->dev,
			"[ERROR][SPI] DMA TX channel request Error!(%p)\n",
			tccspi->dma.chan_tx);
		ret = -EBUSY;
	}

	if (ret == 0) {
		tccspi->dma.chan_rx = dma_request_slave_channel_compat(mask,
				tcc_spi_dma_engine_filter,
				&tccspi->dma,
				tccspi->dev,
				"rx");
		if (tccspi->dma.chan_rx == NULL) {
			dev_err(tccspi->dev,
				"[ERROR][SPI] DMA RX channel request Error!(%p)\n",
				tccspi->dma.chan_rx);
			ret = -EBUSY;
		}
	}

	return ret;
}

/* Probe dma-engine channel */
static int32_t tcc_spi_dma_engine_probe(struct platform_device *pdev,
		struct tcc_spi *tccspi)
{
	struct dma_slave_config slave_config;
	struct device *dev = &pdev->dev;
	int32_t ret = 0;

	tccspi->dev = dev;
	ret = tcc_spi_dma_chan_set(tccspi);
	if (ret == 0) {
		ret = tcc_spi_dma_engine_slv_cfg(tccspi,
				&slave_config,
				TCC_SPI_GDMA_WSIZE);
	}

	if (ret == 0) {
		dev_info(dev,
			"[INFO][SPI] DMA-engine Tx(%s) Rx(%s)\n",
			dma_chan_name(tccspi->dma.chan_tx),
			dma_chan_name(tccspi->dma.chan_rx));
	}

	if (ret != 0) {
		tcc_spi_release_dma_engine(tccspi);
	}

	return ret;
}

/*
 * SPI API
 */

/* SPI setsup */
static int32_t tcc_spi_setup(struct spi_device *spi)
{
	int32_t ret = 0;
	const struct tcc_spi *tccspi = spi_master_get_devdata(spi->master);

	if (tccspi == NULL) {
		(void)pr_err("[ERROR][SPI] tcc_spi data is not exist\n");
		ret = -ENXIO;
	}

	if (ret == 0) {
		/* Set the clock */
		ret = tcc_spi_set_clk(tccspi, spi->max_speed_hz, 0);
	}

	return ret;
}

/* Control CS by GPSB */
static void tcc_spi_set_cs(struct spi_device *spi, bool enable)
{
	const struct tcc_spi *tccspi =
		(struct tcc_spi *)spi_master_get_devdata(spi->master);
	bool cs_active;

	cs_active = enable;
	/* When do not use cs-gpios, SPI_CS_HIGH is set when call spi_setup() */
	if ((spi->mode & (u16)SPI_CS_HIGH) != 0U) {
		cs_active = !cs_active;
	}

	if (!cs_active) {
		if (tccspi->pd->ctf) {
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_CTF);
		}
		if (!tccspi->pd->contm_support) {
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_EN);
		}
	} else {
		if (tccspi->pd->ctf) {
			TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_CTF);
		}
		if (!tccspi->pd->contm_support) {
			TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_EN);
		}
	}

	dev_dbg(tccspi->dev,
		"[DEBUG][SPI] [%s] contm %d en %d\n",
		__func__, tccspi->pd->contm_support, cs_active);
}

/* Hanlde spi half duplex transfer for slave mode */
static int32_t tcc_spi_transfer_halfdup(struct spi_master *tcc_master,
		struct spi_device *spi,
		struct spi_transfer *xfer)
{
	struct tcc_spi *tccspi =
		(struct tcc_spi *)spi_master_get_devdata(tcc_master);
	struct tcc_circ *circ = &(tccspi->srx_queue);
	int32_t ret = 0;
	uint32_t timeout_ms;
	unsigned long timeout_ret;
	uint32_t status;

	if (tccspi == NULL) {
		(void)pr_err("[ERROR][SPI] [%s] tccspi is null!!\n", __func__);
		ret = -EINVAL;
	}

	if ((ret == 0) && (!spi_controller_is_slave(tcc_master))) {
		dev_err(tccspi->dev,
			"[ERROR][SPI] Half duplex xfer only supports slave mode\n");
		ret = -ENXIO;
	}

	if ((ret == 0) && (xfer->rx_buf != NULL)) {
		status = readl(tccspi->base + TCC_GPSB_STAT);
		if ((status & TCC_GPSB_STAT_ERR) > 0U) {
			dev_warn(tccspi->dev, "[WARN][SPI] [%s] rx_buf error (status: 0x%08X)\n", __func__, status);
			writel(status | TCC_GPSB_STAT_ERR, tccspi->base + TCC_GPSB_STAT);
		}
		tcc_spi_rxbuf_copy_client_to_spi(tccspi, xfer, xfer->len);
	} else if ((ret == 0) && (xfer->tx_buf != NULL)) {
		tccspi->pd->is_xfer_tx = (bool)true;
		tccspi->mode = spi->mode;

		tcc_spi_stop_dma(tccspi);
		tcc_spi_set_sync(tccspi, 0);

		/* Reset FIFO and Packet counter */
		ret = tcc_spi_clear_fifo(tccspi);
		if (ret == 0) {
			ret = tcc_spi_set_clk(tccspi, xfer->speed_hz, 0);
			if (ret != 0) {
				dev_err(tccspi->dev,
					"[ERROR][SPI] %s: fail to set clk\n",
					__func__);
			}
		} else {
			dev_err(tccspi->dev,
				"[ERROR][SPI] [%s] Failed to clear FIFO\n",
				__func__);
		}

		if (ret == 0) {
			/* Slave GSDO, GSCMD output enable */
			TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_EVTCTRL,
					TCC_GPSB_EVTCTRL_SDOE);
			TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_SDO);

			/* Set spi mode (clock and loopback)*/
			tcc_spi_set_mode(tccspi, spi->mode);

			/* Get Bit width */
			if (xfer->bits_per_word != spi->bits_per_word) {
				tccspi->bits = xfer->bits_per_word;
			} else {
				tccspi->bits = spi->bits_per_word;
			}
			tcc_spi_set_bit_width(tccspi, tccspi->bits);

			tccspi->cur_tx_pos = 0;

			reinit_completion(&tccspi->xfer_complete);

			/* Copy client txbuf to spi txbuf */
			tcc_spi_txbuf_copy_client_to_spi(tccspi, xfer, xfer->len);
			tcc_spi_set_packet_size(tccspi, xfer->len);

			/* calculate timeout */
			timeout_ms = ((xfer->len << 3U) * 1000U) / xfer->speed_hz;
			if (timeout_ms < (UINT_MAX - 1000U)) {
				timeout_ms += 2000U;
			} else {
				ret = EINVAL;
			}
		}

		if (ret == 0) {
			/* Start Tx and Rx DMA operation */
			tcc_spi_start_dma(tccspi);

			/* SPI transfer start request to master */
			tcc_spi_set_sync(tccspi, 1);
			udelay(1);
			tcc_spi_set_sync(tccspi, 0);

			/* Wait until transfer is finished */
			timeout_ret = wait_for_completion_timeout(
				&tccspi->xfer_complete,
				msecs_to_jiffies(timeout_ms));

			if (timeout_ret == 0UL) {
				status = readl(tccspi->base + TCC_GPSB_STAT);
				dev_err(tccspi->dev,
					"[ERROR][SPI] [%s] spi transfer timeout (%d ms), err %d, (status: 0x%08X)\n",
					__func__, timeout_ms, ret, status);
				if ((status & TCC_GPSB_STAT_ERR) > 0U) {
					writel(status | TCC_GPSB_STAT_ERR, tccspi->base + TCC_GPSB_STAT);
				}
				ret = -ETIME;
			}
		}

		if (ret == 0) {
			/* Copy spi rxbuf to client rxbuf */
			spin_lock(&tccspi->srx_lock);
			tcc_buf_to_circq(circ,
					tccspi->rx_buf.v_addr,
					xfer->len);
			spin_unlock(&tccspi->srx_lock);
		}

		ret = tcc_spi_clear_fifo(tccspi);
		if (ret < 0) {
			dev_err(tccspi->dev,
				"[ERROR][SPI] [%s] Failed to clear FIFO\n",
				__func__);
		}

		if (ret == 0) {
			/* Slave GSDO, GSCMD output disable */
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_EVTCTRL,
					TCC_GPSB_EVTCTRL_SDOE);
			TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
					TCC_GPSB_MODE_SDO);

			/* Set spi mode (clock and loopback)*/
			tcc_spi_set_mode(tccspi, spi->mode);
			tcc_spi_set_bit_width(tccspi, tccspi->bits);
		}

		/* Copy client txbuf to spi txbuf */
		tcc_spi_set_packet_size(tccspi, tccspi->pd->packet_size);

		/* Start Tx and Rx DMA operation */
		tcc_spi_start_dma(tccspi);

		tccspi->pd->is_xfer_tx = (bool)false;
	} else {
		/* Noting to do */
	}

	return ret;
}

/* Hanlde one spi_transfer */
static int32_t tcc_spi_transfer_one(struct spi_master *tcc_master,
		struct spi_device *spi,
		struct spi_transfer *xfer)
{
	struct tcc_spi *tccspi =
		(struct tcc_spi *)spi_master_get_devdata(spi->master);
	int32_t scret = 0, ret = 0;
	unsigned long mcret = 0U;
	uint32_t timeout_ms;

	if (tccspi == NULL) {
		(void)pr_err("[ERROR][SPI] [%s] tccspi is null!!\n", __func__);
		ret = -EINVAL;
	}

	if (ret == 0) {
		ret = tcc_spi_set_clk(tccspi, xfer->speed_hz, 0);
		if (ret != 0) {
			dev_err(tccspi->dev,
				"[ERROR][SPI] %s: fail to set clk\n",
				__func__);
		}
	}

	if (ret == 0) {
		/* Get Bit width */
		if (xfer->bits_per_word != spi->bits_per_word) {
			tccspi->bits = xfer->bits_per_word;
		} else {
			tccspi->bits = spi->bits_per_word;
		}
		tcc_spi_set_bit_width(tccspi, tccspi->bits);

		tccspi->cur_rx_pos = 0;
		tccspi->cur_tx_pos = 0;
		tccspi->current_xfer = xfer;
		tccspi->current_remaining_bytes = xfer->len;
		tccspi->mode = spi->mode;

		dev_dbg(tccspi->dev, "[DEBUG][SPI][%s] xfer length %d\n",
				__func__, xfer->len);
	}

	while ((ret == 0) && (tccspi->current_remaining_bytes > 0U)) {
		uint32_t len;

		reinit_completion(&tccspi->xfer_complete);

		/* Set Packet size (packet count = 0) */
		if (tccspi->current_remaining_bytes > tccspi->dma_buf_size) {
			len = (u32)tccspi->dma_buf_size;
			tccspi->tx_packet_remain = 1;
			dev_dbg(tccspi->dev,
				"[DEBUG][SPI] remain %d xfer %d\n",
				tccspi->current_remaining_bytes,
				len);
		} else {
			len = tccspi->current_remaining_bytes;
			tccspi->tx_packet_remain = 0;
			dev_dbg(tccspi->dev,
				"[DEBUG][SPI] xfer %d\n",
				len);
		}

		/* Reset FIFO and Packet counter */
		ret = tcc_spi_clear_fifo(tccspi);
		if (ret == 0) {
			/* Set TXBASE and RXBASE registers */
			tcc_spi_set_dma_addr(tccspi,
					tccspi->tx_buf.dma_addr,
					tccspi->rx_buf.dma_addr);

			/* Copy client txbuf to spi txbuf */
			tcc_spi_txbuf_copy_client_to_spi(tccspi, xfer, len);
			tcc_spi_set_packet_size(tccspi, len);

			/* Set spi mode (clock and loopback)*/
			tcc_spi_set_mode(tccspi, spi->mode);

			/* Setup GDMA, if use */
			if (tccspi->pd->is_using_gdma == (bool)true) {
				/* Submit dma engine descriptor */
				ret = tcc_spi_dma_engine_submit(tccspi, len);
				if (ret < 0) {
					dev_err(tccspi->dev,
						"[ERROR][SPI] spi dma transfer err %d\n",
						ret);
					tcc_spi_stop_dma_engine(tccspi);
				}
			}
		}

		if (ret == 0) {
			tcc_spi_regs_dump(tccspi);

			/* Start Tx and Rx DMA operation */
			tcc_spi_start_dma(tccspi);

			/* calculate timeout */
			timeout_ms = ((len << 3U) * 1000U) / xfer->speed_hz;
			if (timeout_ms < (UINT_MAX - 100U)) {
				timeout_ms += 100U; /* some tolerance */
			} else {
				dev_warn(tccspi->dev,
					"[WARN][SPI] Data transfer may not be completed within the timeout\n");
			}

			/* Wait until transfer is finished */
			if (spi_controller_is_slave(tcc_master)) {
				tcc_spi_set_sync(tccspi, 0);

				scret = wait_for_completion_interruptible(
						&tccspi->xfer_complete);
				if (scret == 0) {
					scret = 1;
				}
			} else {
				mcret = wait_for_completion_timeout(
						&tccspi->xfer_complete,
						msecs_to_jiffies(timeout_ms));
			}

			if ((mcret == 0U) &&
			   (scret == 0)) {
				dev_err(tccspi->dev,
					"[ERROR][SPI] [%s] spi interrupted or trasfer timeout (%d ms), err %d\n",
					__func__, timeout_ms, ret);

				tcc_spi_regs_dump(tccspi);

				if (tccspi->pd->is_using_gdma == (bool)true) {
					tcc_spi_stop_dma_engine(tccspi);
				}
				tcc_spi_stop_dma(tccspi);

				ret = -ETIME;
			}
		}

		if (ret == 0) {
			/* Copy spi rxbuf to client rxbuf */
			tcc_spi_rxbuf_copy_client_to_spi(tccspi, xfer, len);

			if (tccspi->current_remaining_bytes < len) {
				dev_err(tccspi->dev,
					"[ERROR][SPI]%s: remaining bytes (%d) < xfered len (%d)\n",
					__func__,
					tccspi->current_remaining_bytes,
					len);
				ret = -EIO;
			}
		}

		if (ret == 0) {
			tccspi->current_remaining_bytes -= len;
			dev_dbg(tccspi->dev,
				"[DEBUG][SPI] completed remain %d xfered %d\n",
				tccspi->current_remaining_bytes, len);
		}
	}

	return ret;
}

static int32_t tcc_spi_init(const struct tcc_spi *tccspi)
{
	int32_t ret = 0;
	uint32_t ac_val[2] = {0,};
	const struct device_node *np = tccspi->dev->of_node;

	if (tccspi->pd->hclk == NULL) {
		ret = -ENOMEM;
		dev_err(tccspi->dev,
			"[ERROR][SPI] [%s] hclk is null!!\n",
			__func__);
	}

	if (tccspi->pd->pclk == NULL) {
		ret = -ENOMEM;
		dev_err(tccspi->dev,
			"[ERROR][SPI] [%s] pclk is null!!\n",
			__func__);
	}

	if (ret == 0) {
		/* Enable clock */
		ret = clk_prepare_enable(tccspi->pd->hclk);
		if (ret < 0) {
			dev_err(tccspi->dev,
				"[ERROR][SPI] Failed to enable hclk\n");
		}
	}

	if (ret == 0) {
		ret = clk_prepare_enable(tccspi->pd->pclk);
		if (ret < 0) {
			dev_err(tccspi->dev,
				"[ERROR][SPI] Failed to enable pclk\n");
			clk_disable_unprepare(tccspi->pd->hclk);
		}
	}

	if (ret == 0) {
		/* access control */
		if (!IS_ERR(tccspi->ac)) {
			if (of_property_read_u32_array(np,
						"access-control0",
						ac_val,	2) == 0) {
				dev_dbg(tccspi->dev,
					"[DEBUG][SPI] access-control0 start:0x%x limit:0x%x\n",
					ac_val[0], ac_val[1]);
				writel(ac_val[0],
					tccspi->ac + TCC_GPSB_AC0_START);
				writel(ac_val[1],
					tccspi->ac + TCC_GPSB_AC0_LIMIT);
			}
			if (of_property_read_u32_array(np,
						"access-control1",
						ac_val, 2) == 0) {
				dev_dbg(tccspi->dev,
					"[DEBUG][SPI] access-control1 start:0x%x limit:0x%x\n",
					ac_val[0], ac_val[1]);
				writel(ac_val[0],
					tccspi->ac + TCC_GPSB_AC1_START);
				writel(ac_val[1],
					tccspi->ac + TCC_GPSB_AC1_LIMIT);
			}
			if (of_property_read_u32_array(np,
						"access-control2",
						ac_val, 2) == 0) {
				dev_dbg(tccspi->dev,
					"[DEBUG][SPI] access-control2 start:0x%x limit:0x%x\n",
					ac_val[0], ac_val[1]);
				writel(ac_val[0],
					tccspi->ac + TCC_GPSB_AC2_START);
				writel(ac_val[1],
					tccspi->ac + TCC_GPSB_AC2_LIMIT);
			}
			if (of_property_read_u32_array(np,
						"access-control3",
						ac_val, 2) == 0) {
				dev_dbg(tccspi->dev,
					"[DEBUG][SPI] access-control3 start:0x%x limit:0x%x\n",
					ac_val[0], ac_val[1]);
				writel(ac_val[0],
					tccspi->ac + TCC_GPSB_AC3_START);
				writel(ac_val[1],
					tccspi->ac + TCC_GPSB_AC3_LIMIT);
			}
		}

		/* Set port configuration */
		ret = tcc_spi_set_port(tccspi);
	}

	if (ret == 0) {
		/* Initialize GPSB registers */
		tcc_spi_stop_dma(tccspi);
		tcc_spi_hwinit(tccspi);
	}

	return ret;
}

static int32_t tcc_spi_get_gpsb_ch(const struct tcc_spi_pl_data *pd,
				struct device_node *np)
{
	int32_t ret = of_alias_get_id(np, "gpsb");

	if (ret >= 0) {
		if ((uint32_t)ret > pd->soc_info->last_ch) {
			ret = -EINVAL;
		}
	}

	return ret;
}

static int32_t tcc_spi_parse_port(struct tcc_spi_pl_data *pd,
				const struct device_node *np)
{
	int32_t ret = 0;

	if ((pd->soc_info->id == TCC897X) ||
		(pd->soc_info->id == TCC803X) ||
		(pd->soc_info->id == TCC805X) ||
		(pd->soc_info->id == TCC807X) ||
		(pd->soc_info->id == TCN100X)) {
		ret = of_property_read_u32(np, "gpsb-port", &pd->port);
	} else if (pd->soc_info->id == TCC750X) {
		pd->is_srvc = of_property_read_bool(np, "spi-srvc");
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static int32_t tcc_spi_parse_dma(struct tcc_spi_pl_data *pd,
				const struct device_node *np)
{
	int32_t ret = 0;

	switch (pd->soc_info->id) {
		case TCC897X:
		case TCC807X:
			pd->is_using_gdma = of_property_read_bool(np, "dmas");
			break;

		case TCC803X:
		case TCC805X:
			/*
			 * TCC GPSB CH 3-5 don't have dedicated dma
			 * GPSB CH 3-5 should use gdma(dma-engine)
			 */
			if ((pd->gpsb_channel > 2U && pd->gpsb_channel < 6U) ||
				of_property_read_bool(np, "dmas")) {
				pd->is_using_gdma = (bool)true;
			} else {
				pd->is_using_gdma = (bool)false;
			}
			break;

		case TCC750X:
		case TCN100X:
			/* uses dedicated dma on all channels */
			pd->is_using_gdma = (bool)false;
			break;

		default:
			ret = -EINVAL;
			break;
	}

	if ((ret == 0) && (pd->is_using_gdma)) {
		ret = of_property_read_u32(np, "dma-burst", &pd->dma_bsize);
		if (ret != 0) {
			pd->dma_bsize = 1;
			ret = 0;
		}
		if (pd->dma_bsize > 4U) {
			pr_warn("[WARN][SPI] Exceeded maximum dma burst size [req:%d]\n",
					pd->dma_bsize);
			pd->dma_bsize = 4;
		}
	}

	return ret;
}

/* Get the hclk and pclk */
static int32_t tcc_spi_parse_clk(struct tcc_spi_pl_data *pd,
				struct device_node *np)
{
	int32_t ret = 0;

	pd->pclk = of_clk_get(np, 0);
	if (IS_ERR_OR_NULL((void *)pd->pclk)) {
		ret = -EIO;
	}

	if (ret == 0) {
		pd->hclk = of_clk_get(np, 1);
		if (IS_ERR_OR_NULL((void *)pd->hclk)) {
			ret = -EIO;
		}
	}

	return ret;
}

static struct tcc_spi_pl_data *tcc_spi_parse_dt(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np;
	struct tcc_spi_pl_data *pd;
	int32_t ret = 0;

	np = dev->of_node;

	if (np == NULL) {
		dev_err(dev, "[ERROR][SPI] no dt node defined\n");
		ret = -EINVAL;
	}

	if (ret == 0) {
		pd = devm_kzalloc(dev,
				sizeof(struct tcc_spi_pl_data),
				GFP_KERNEL);
		if (pd == NULL) {
			ret = -ENOMEM;
		}
	}

	if (ret == 0) {
		pd->soc_info = (const struct tcc_spi_soc_info *)of_device_get_match_data(&pdev->dev);
		if (pd->soc_info == NULL) {
			ret = -EINVAL;
		}
	}

	if (ret == 0) {
		/* Get the driver name */
		pd->name = np->name;
		ret = tcc_spi_get_gpsb_ch(pd, np);
		if (ret < 0) {
			dev_err(dev,
				"[ERROR][SPI] wrong spi driver name (%s)\n",
				np->name);
		} else {
			pd->gpsb_channel = (uint32_t)ret;

			ret = tcc_spi_parse_dma(pd, np);
			if (ret != 0) {
				dev_err(dev,
					"[ERROR][SPI] Failed to GDMA configuration\n");
			}
		}
	}

	/* Get the port number */
	if (ret == 0) {
		ret = tcc_spi_parse_port(pd, np);
		if (ret != 0) {
			dev_err(dev, "[ERROR][SPI] No SPI port info\n");
		}
	}

	if (ret == 0) {
		if (of_property_read_bool(np, "ctf-mode-disable")) {
			pd->ctf = (bool)false;
		} else {
			pd->ctf = (bool)true;
		}

		if (of_property_read_bool(np, "prd-enable")) {
			pd->prd = (bool)true;
		} else {
			pd->prd = (bool)false;
		}

		if (of_property_read_bool(np, "sdoe-enable")) {
			pd->sdoe = (bool)true;
		} else {
			pd->sdoe = (bool)false;
		}

		if (of_property_read_bool(np, "recovery-time")) {
			pd->recovery_time = (bool)true;
		} else {
			pd->recovery_time = (bool)false;
		}

		if (of_property_read_bool(np, "hold-time")) {
			pd->hold_time = (bool)true;
		} else {
			pd->hold_time = (bool)false;
		}

		if (of_property_read_bool(np, "setup-time")) {
			pd->setup_time = (bool)true;
		} else {
			pd->setup_time = (bool)false;
		}

		pd->is_slave = of_property_read_bool(np, "spi-slave");

		if (of_property_read_bool(np, "slave-queue-enable") &&
		   (pd->is_slave == (bool)true)) {
			ret = of_property_read_u32(np, "rx-packet-size", &pd->packet_size);
			if (ret != 0) {
				pd->srx_queue_enable = (bool)false;
			} else {
				pd->srx_queue_enable = (bool)true;
			}
		} else {
			pd->srx_queue_enable = (bool)false;
		}

		/* optional value */
		ret = of_property_read_u32(np,
				"write-threshold",
				&pd->cfgwth);
		if (ret != 0) {
			if (pd->is_slave) {
				pd->cfgwth = 6U; /* slave (default value) */
			} else {
				pd->cfgwth = 0U; /* master (default value) */
			}
			ret = 0;
		}

		if (pd->cfgwth > 15U) {
			pd->cfgwth = 15U; /* max value */
		}

		/* optional value */
		ret = of_property_read_u32(np,
				"read-threshold",
				&pd->cfgrth);
		if (ret != 0) {
			pd->cfgrth = 0U; /* default value */
			ret = 0;
		}
		if (pd->cfgrth > 15U) {
			pd->cfgrth = 15U; /* max value */
		}

		if (pd->is_slave) {
			pd->sync_gpio = devm_gpiod_get_optional(dev, "sync", GPIOD_OUT_LOW);
			if (!IS_ERR_OR_NULL((void *)pd->sync_gpio)) {
				dev_info(dev,
					"[INFO][SPI] Success to init sync-gpio\n");
			} else {
				pd->sync_gpio = NULL;
			}
		} else {
			pd->sync_gpio = NULL;
		}

		dev_info(dev,
			"[INFO][SPI] GPSB %s CTF mode: %d, PRD: %d, SDOE: %d, Tx threshold: %d, Rx threshold: %d\n",
			pd->is_slave ? "Slave":"Master",
			pd->ctf,
			pd->prd,
			pd->sdoe,
			pd->cfgwth,
			pd->cfgrth);
		if (!pd->is_slave) {
			dev_info(dev,
				"[INFO][SPI] recovery-time: %d, hold-time: %d, setup-time: %d\n",
				pd->recovery_time,
				pd->hold_time,
				pd->setup_time);
		} else {
			if (pd->srx_queue_enable) {
				dev_info(dev, "[INFO][SPI] slave rx buffer enable: %d\n", pd->packet_size);
			}
		}

		ret = tcc_spi_parse_clk(pd, np);
		if (ret < 0) {
			dev_err(dev, "[ERROR][SPI] No SPI clock info.\n");
		}
	}

	if (ret == 0) {
		if (pd->soc_info->id != TCC803X) {
			pd->rst = devm_reset_control_get_shared_by_index(dev, 0);
			if (!IS_ERR_OR_NULL(pd->rst)) {
				reset_control_deassert(pd->rst);
			}
		}
	}

	if (ret != 0) {
		pd = NULL;
	}

	return pd;
}

/* Probe spi master driver */
static int32_t tcc_spi_probe(struct platform_device *pdev)
{
	int32_t ret = 0;
	uint16_t dma_mask_bits;
	struct device *dev = &pdev->dev;
	const struct resource *regs;
	struct spi_master *tcc_master;
	struct tcc_spi *tccspi;
	struct tcc_spi_pl_data *pd = NULL;

	/* Get TCC GPSB SPI master platform data */
	pd = tcc_spi_parse_dt(pdev);
	if (pd == NULL) {
		dev_err(dev,
			"[ERROR][SPI] No TCC SPI master platform data.\n");
		ret = -EINVAL;
		goto out;
	}

	/* allocate master or slave controller */
	if (pd->is_slave) {
		tcc_master = spi_alloc_slave(dev, (u32)sizeof(struct tcc_spi));
	} else {
		tcc_master = spi_alloc_master(dev, (u32)sizeof(struct tcc_spi));
	}
	if (tcc_master == NULL) {
		dev_err(dev,
			"[ERROR][SPI] SPI memory allocation failed.\n");
		ret = -ENOMEM;
		goto out;
	}

	/* the spi->mode bits understood by this driver: */
	if (pd->gpsb_channel <= pd->soc_info->last_ch) {
		tcc_master->bus_num = (s16)pd->gpsb_channel;
	} else {
		dev_err(dev,
			"[ERROR][SPI] Invalid SPI id.\n");
		goto out_free_master;
	}

	/* tcc_master->num_chipselect = 1. */
	tcc_master->mode_bits = TCC_SPI_MODE_BITS;
	tcc_master->bits_per_word_mask =
		(u32)SPI_BPW_MASK(32) |
		(u32)SPI_BPW_MASK(16) |
		(u32)SPI_BPW_MASK(8);
	tcc_master->dev.of_node = dev->of_node;
	tcc_master->max_speed_hz = TCC_GPSB_MAX_FREQ;
	tcc_master->setup = tcc_spi_setup;
	tcc_master->set_cs = tcc_spi_set_cs;
	tcc_master->use_gpio_descriptors = (bool)true;
	if (pd->soc_info->id == TCN100X) {
		dma_mask_bits = (u16)(sizeof(uint64_t) * (u8)BITS_PER_BYTE);
	} else {
		dma_mask_bits = (u16)(sizeof(uint32_t) * (u8)BITS_PER_BYTE);
	}

	platform_set_drvdata(pdev, tcc_master);

	tccspi = spi_master_get_devdata(tcc_master);

	/* Initialize tcc_spi */
	tccspi->dev = dev;
	tccspi->tcc_master = tcc_master;
	tccspi->pd = pd;
	tccspi->bits = 8;
	tccspi->dma_buf_size = TCC_SPI_DMA_MAX_SIZE;

	/* Set device dma coherent mask */
	ret = dma_set_coherent_mask(dev, GPSB_DMA_BIT_MASK(dma_mask_bits));
	if (ret != 0) {
		dev_err(dev,
			"[ERROR][SPI] Failed to set dma mask\n");
		ret = -ENXIO;
		goto out_free_master;
	}

	if (dev->dma_mask == NULL) {
		dev->dma_mask = &dev->coherent_dma_mask;
	} else {
		(void)dma_set_mask(dev, GPSB_DMA_BIT_MASK(dma_mask_bits));
	}
	/* Get TCC GPSB SPI master base address */
	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (regs == NULL) {
		dev_err(dev,
			"[ERROR][SPI] No SPI base register addr.\n");
		ret = -ENXIO;
		goto out_free_master;
	}

	tccspi->pbase = regs->start;
	tccspi->base = devm_ioremap_resource(dev, regs);

	/* Get TCC GPSB SPI master port configuration reg. address */
	regs = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (regs == NULL) {
		dev_err(dev,
			"[ERROR][SPI] No SPI PCFG register addr.\n");
		ret = -ENXIO;
		goto out_free_master;
	}

	tccspi->pcfg = of_iomap(dev->of_node, 1);
	/* Configure the AHB Access Filter */
	tccspi->ac = of_iomap(dev->of_node, 2);

	/* initialize GPSB */
	ret = tcc_spi_init(tccspi);
	if (ret < 0) {
		dev_err(dev,
			"[ERROR][SPI] %s: failed to init GPSB\n",
			__func__);
		ret = -ENXIO;
		goto out_free_master;
	}

	/* Get pin control (active state)*/
	tccspi->pinctl = devm_pinctrl_get_select(tccspi->dev, "active");
	if (IS_ERR_OR_NULL((void *)tccspi->pinctl)) {
		dev_err(tccspi->dev,
			"[ERROR][SPI] Failed to get pinctrl (active state)\n");
		ret = -ENXIO;
		goto out_unprepare_clk;
	} else {
		tccspi->pinctl_active 	= pinctrl_lookup_state(tccspi->pinctl, "active");
		tccspi->pinctl_idle 	= pinctrl_lookup_state(tccspi->pinctl, "idle");
		tccspi->pinctl_cs 	= pinctrl_lookup_state(tccspi->pinctl, "cs");

		if (IS_ERR_OR_NULL((void *)tccspi->pinctl_active) ||
		   (IS_ERR_OR_NULL((void *)tccspi->pinctl_idle))) {
			goto out_unprepare_clk;
		}
	}

	/* Get TCC GPSB IRQ number */
	tccspi->irq = platform_get_irq(pdev, 0);
	if (tccspi->irq < 0) {
		dev_err(dev,
			"[ERROR][SPI] No SPI IRQ Number.\n");
		ret = -ENXIO;
		goto out_unprepare_clk;
	}

	if (tccspi->pd->is_using_gdma == (bool)true) {
		ret = tcc_spi_dma_engine_probe(pdev, tccspi);
		if (ret < 0) {
			dev_err(dev,
				"[ERROR][SPI] Failed to get dma-engine\n");
			ret = -ENXIO;
			goto out_unprepare_clk;
		}
	} else {
		if (!pd->srx_queue_enable) {
			ret = devm_request_irq(dev,
				(u32)tccspi->irq,
				tcc_spi_dma_irq,
				IRQF_SHARED,
				dev_name(dev),
				tcc_master);
		} else {
			ret = devm_request_irq(dev,
				(u32)tccspi->irq,
				tcc_spi_halfdup_irq,
				IRQF_SHARED,
				dev_name(dev),
				tcc_master);
		}
		if (ret < 0) {
			dev_err(dev,
				"[ERROR][SPI] Failed to enter irq handler\n");
			ret = -ENXIO;
			goto out_unprepare_clk;
		}
	}

	/* Allocate SPI master dma buffer (rx) */
	ret = tcc_spi_init_dma_buf(tccspi, 1);
	if (ret < 0) {
		dev_err(dev,
			"[ERROR][SPI] Failed to allocate rx dma buffer\n");
		ret = -ENOMEM;
		goto out_unprepare_clk;
	}

	/* Allocate SPI master dma buffer (tx) */
	ret = tcc_spi_init_dma_buf(tccspi, 0);
	if (ret < 0) {
		dev_err(dev,
			"[ERROR][SPI] Failed to allocate tx dma buffer\n");
		ret = -ENOMEM;
		goto out_rx_dma_free;
	}

	if (pd->srx_queue_enable) {
		tcc_master->transfer_one = tcc_spi_transfer_halfdup;
		spin_lock_init(&tccspi->srx_lock);

		tccspi->srx_queue.buf = (uint8_t *)devm_kzalloc(dev,
						TCC_GPSB_SLAVE_QUEUE_MAX_SIZE,
						GFP_KERNEL);
		if (tccspi->srx_queue.buf == NULL) {
			dev_err(dev,
				"[ERROR][SPI] Failed to allocate slave queue memory\n");
			ret = -ENOMEM;
			goto out_unprepare_clk;
		}

		(void)tcc_spi_clear_fifo(tccspi);
		tcc_spi_stop_dma(tccspi);

		/* Slave GSDO, GSCMD output enable */
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_EVTCTRL,
				TCC_GPSB_EVTCTRL_SDOE);
		TCC_GPSB_BITSET(tccspi->base + TCC_GPSB_MODE,
				TCC_GPSB_MODE_SDO);
	} else {
		tcc_master->transfer_one = tcc_spi_transfer_one;
	}

	/* Initialize xfer completion */
	init_completion(&tccspi->xfer_complete);

	platform_set_drvdata(pdev, tcc_master);

	ret = devm_spi_register_master(dev, tcc_master);
	if (ret < 0) {
		dev_err(dev,
			"[ERROR][SPI] Failed to spi driver register.\n");
		ret = -ENOMEM;
		goto out_tx_dma_free;
	}

	dev_info(dev,
		"[INFO][SPI] TCC SPI [%s] Id: %d [ch:%d][port: %d][irq: %d][CONTM: %d][gdma: %d][gdma burst: %d][cs_num: %d]\n",
		pd->name,
		tcc_master->bus_num,
		pd->gpsb_channel,
		pd->port,
		tccspi->irq,
		tccspi->pd->contm_support,
		tccspi->pd->is_using_gdma,
		tccspi->pd->dma_bsize,
		tcc_master->num_chipselect);


	if (pd->srx_queue_enable) {
		/* Reset FIFO and Packet counter */
		ret = tcc_spi_clear_fifo(tccspi);
		if (ret == 0) {
			ret = tcc_spi_set_clk(tccspi, tcc_master->max_speed_hz, 0);
			if (ret != 0) {
				dev_err(tccspi->dev,
					"[ERROR][SPI] %s: fail to set clk\n",
					__func__);
			}
		} else {
			dev_err(tccspi->dev,
				"[ERROR][SPI] [%s] Failed to clear FIFO\n",
				__func__);
		}

		if (ret == 0) {
			/* Set spi mode (clock and loopback)*/
			tcc_spi_set_mode(tccspi, 0);

			/* Set TXBASE and RXBASE registers */
			tcc_spi_set_dma_addr(tccspi,
				tccspi->tx_buf.dma_addr,
				tccspi->rx_buf.dma_addr);

			/* Get Bit width */
			tcc_spi_set_bit_width(tccspi, tccspi->bits);

			/* Copy client txbuf to spi txbuf */
			tcc_spi_set_packet_size(tccspi, tccspi->pd->packet_size);

			/* Start Tx and Rx DMA operation */
			tcc_spi_start_dma(tccspi);
		}
	}

	if (tccspi->pd->soc_info->id == TCC807X) {
		if (pd->is_slave) {
			ret = device_create_file(dev, &dev_attr_tcc_spi_sync);
		}
	}

	if (ret != 0) {
out_tx_dma_free:
		tcc_spi_deinit_dma_buf(tccspi, 0);
out_rx_dma_free:
		tcc_spi_deinit_dma_buf(tccspi, 1);
out_unprepare_clk:
		clk_disable_unprepare(pd->pclk);
		clk_disable_unprepare(pd->hclk);
out_free_master:
		spi_master_put(tcc_master);
	}
out:
	return ret;
}

static int32_t tcc_spi_remove(struct platform_device *pdev)
{
	struct spi_master *tcc_master = platform_get_drvdata(pdev);
	struct tcc_spi *tccspi =
		(struct tcc_spi *)spi_master_get_devdata(tcc_master);
	int32_t ret = 0;

	if (tccspi == NULL) {
		(void)pr_err("[ERROR][SPI] [%s] tccspi is null!!\n", __func__);
		ret = -EINVAL;
	} else {
		/* Get pin control (idle state)*/
		ret = pinctrl_select_state(tccspi->pinctl, tccspi->pinctl_idle);
		if (ret < 0) {
			dev_err(tccspi->dev,
				"[ERROR][SPI] Failed to get pinctrl (idle state)\n");
			ret = -ENODEV;
		}

		if (!IS_ERR_OR_NULL(tccspi->pd->sync_gpio)) {
			gpiod_put(tccspi->pd->sync_gpio);
		}

		if (ret == 0) {
			/* Disable clock */
			if (tccspi->pd->pclk != NULL) {
				clk_disable_unprepare(tccspi->pd->pclk);
			}
			if (tccspi->pd->hclk != NULL) {
				clk_disable_unprepare(tccspi->pd->hclk);
			}

			/* Release DMA buffers */
			if (tccspi->rx_buf.v_addr != NULL) {
				tcc_spi_deinit_dma_buf(tccspi, 1);
			}
			if (tccspi->tx_buf.v_addr != NULL) {
				tcc_spi_deinit_dma_buf(tccspi, 0);
			}

			if (tccspi->pd->is_using_gdma == (bool)true) {
				tcc_spi_release_dma_engine(tccspi);
			}

			if (tccspi->pd->soc_info->id == TCC807X) {
				if (tccspi->pd->is_slave) {
					device_remove_file(&pdev->dev, &dev_attr_tcc_spi_sync);
				}
			}
		}
	}

	/* controller unreginster가 없다?? */

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int32_t tcc_spi_suspend(struct device *dev)
{
	struct spi_master *tcc_master = dev_get_drvdata(dev);
	const struct tcc_spi *tccspi = (struct tcc_spi *)spi_master_get_devdata(tcc_master);
	const struct tcc_spi_pl_data *pd;
	int32_t ret = 0;

	if (tccspi == NULL) {
		(void)pr_err("[ERROR][SPI] [%s] tccspi is null!!\n", __func__);
		ret = -EINVAL;
	} else {
		pd = tccspi->pd;

		TCC_GPSB_BITCLR(tccspi->base + TCC_GPSB_MODE, TCC_GPSB_MODE_EN);
		tcc_spi_stop_dma(tccspi);

		if (pd->pclk != NULL) {
			clk_disable_unprepare(pd->pclk);
		}
		if (pd->hclk != NULL) {
			clk_disable_unprepare(pd->hclk);
		}

		/* Get pin control (idle state)*/
		ret = pinctrl_select_state(tccspi->pinctl, tccspi->pinctl_idle);
		if (ret < 0) {
			dev_err(dev,
				"[ERROR][SPI] Failed to get pinctrl (idle state)\n");
			ret = -ENXIO;
		} else {
			ret = spi_master_suspend(tcc_master);
		}
	}

	return ret;
}

static int32_t tcc_spi_resume(struct device *dev)
{
	struct spi_master *tcc_master = dev_get_drvdata(dev);
	const struct tcc_spi *tccspi = spi_master_get_devdata(tcc_master);
	int32_t ret = 0;
	uint16_t i;

	if (tccspi == NULL) {
		(void)pr_err("[ERROR][SPI] [%s] tccspi is null!!\n", __func__);
		ret = -EINVAL;
	} else {
		/* Initialize GPSB */
		ret = tcc_spi_init(tccspi);
		if (ret == 0) {
			ret = pinctrl_select_state(tccspi->pinctl, tccspi->pinctl_active);
			if (ret < 0) {
				dev_err(dev,
					"[ERROR][SPI] Failed to get pinctrl (active state)\n");
				ret = -ENXIO;
			}
		} else {
			dev_err(dev,
				"[ERROR][SPI] %s: failed to init GPSB\n",
				__func__);
		}

		if (!tccspi->pd->is_slave) {
			if (!IS_ERR_OR_NULL((void *)tccspi->pinctl_cs)) {
				ret = pinctrl_select_state(tccspi->pinctl,
							   tccspi->pinctl_cs);
			}

			for (i = 0; i < tcc_master->num_chipselect; i++) {
				(void)gpiod_set_value(tcc_master->cs_gpiods[i], 0);
			}
		}

		if ((ret == 0) &&
		    (!IS_ERR_OR_NULL((void *)tccspi->pd->sync_gpio))) {
			ret = gpiod_direction_output(tccspi->pd->sync_gpio, 0);
			if (ret == 0) {
				dev_info(tccspi->dev,
					"[INFO][SPI] Success to init sync-gpio\n");
			} else {
				dev_err(tccspi->dev,
					"[ERROR][SPI] Failed to init sync-gpio\n");
			}
		}

		if (ret == 0) {
			ret = spi_master_resume(tcc_master);
		}
	}

	return ret;
}

static const struct dev_pm_ops tcc_spi_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(tcc_spi_suspend, tcc_spi_resume)
};

#define TCC_SPI_PM	(&tcc_spi_pmops)
#else
#define TCC_SPI_PM	(NULL)
#endif

static const struct tcc_spi_soc_info tcc897x_gpsb_info = {
	.id = TCC897X,
	.last_ch = 2,
};

static const struct tcc_spi_soc_info tcc803x_gpsb_info = {
	.id = TCC803X,
	.last_ch = 5,
};

static const struct tcc_spi_soc_info tcc805x_gpsb_info = {
	.id = TCC805X,
	.last_ch = 5,
};

static const struct tcc_spi_soc_info tcc750x_gpsb_info = {
	.id = TCC750X,
	.last_ch = 4,
};

static const struct tcc_spi_soc_info tcc807x_gpsb_info = {
	.id = TCC807X,
	.last_ch = 6,
};

static const struct tcc_spi_soc_info tcn100x_gpsb_info = {
	.id = TCN100X,
	.last_ch = 6
};

static struct of_device_id tcc_spi_of_match[] = {
	{ .compatible = "telechips,tcc-spi",
	  .data	      = &tcc897x_gpsb_info},
	{ .compatible = "telechips,tcc897x-spi",
	  .data       = &tcc897x_gpsb_info},
	{ .compatible = "telechips,tcc803x-spi",
	  .data       = &tcc803x_gpsb_info},
	{ .compatible = "telechips,tcc805x-spi",
	  .data       = &tcc805x_gpsb_info},
	{ .compatible = "telechips,tcc750x-spi",
	  .data	      = &tcc750x_gpsb_info},
	{ .compatible = "telechips,tcc807x-spi",
	  .data	      = &tcc807x_gpsb_info},
	{ .compatible = "telechips,tcn100x-spi",
	  .data       = &tcn100x_gpsb_info },
	{},
};
MODULE_DEVICE_TABLE(of, tcc_spi_of_match);

static struct platform_driver tcc_spidrv = {
	.probe		= tcc_spi_probe,
	.remove		= tcc_spi_remove,
	.driver		= {
		.name		= "tcc-spi",
		.owner		= THIS_MODULE,
		.pm = TCC_SPI_PM,
		.of_match_table = of_match_ptr(tcc_spi_of_match)
	},
};

module_platform_driver(tcc_spidrv);

MODULE_AUTHOR("Telechips Inc. linux@telechips.com");
MODULE_DESCRIPTION("Telechips GPSB SPI Driver");
MODULE_LICENSE("GPL");
