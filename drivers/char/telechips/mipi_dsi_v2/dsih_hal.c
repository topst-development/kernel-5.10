// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (C) 2018 Synopsys, Inc.
 *
 * @file dsih_hal.c
 * @brief Hardware Abstraction Level of Synopsys MIPI DSI HOST controller
 * included as a part of Synopsys MIPI DSI Host controller driver
 *
 * @author Luis Oliveira <luis.oliveira@synopsys.com>
 * Modified by Telechips
 */

#include "dsih_hal.h"
#include "dsih_core.h"
#include "dsih_includes.h"

/**
 * Write a 32-bit word to the DSI Host core
 * @param dev pointer to structure holding the DSI Host core information
 * @param reg_address register offset in core
 * @param data 32-bit word to be written to register
 */
void mipi_dsih_write_word(struct mipi_dsi_dev *dev, unsigned int reg_address, unsigned int data)
{
	if(dev == NULL) {
		pr_err("%s:Device is null\n", __func__);
		return;
	}
	pr_debug("CORE: ADDR %X DATA %X\n",reg_address,data);

	__raw_writel(data,dev->core_addr + reg_address);
}
/**
 * Write a bit field o a 32-bit word to the DSI Host core
 * @param dev pointer to structure holding the DSI Host core information
 * @param reg_address register offset in core
 * @param data to be written to register
 * @param shift bit shift from the left (system is BIG ENDIAN)
 * @param width of bit field
 */
void mipi_dsih_write_part(struct mipi_dsi_dev *dev, unsigned int reg_address, unsigned int data, unsigned int shift, unsigned int width)
{
	unsigned int mask = (1 << width) - 1;
	unsigned int temp = mipi_dsih_read_word(dev, reg_address);

	temp &= ~(mask << shift);
	temp |= (data & mask) << shift;
	mipi_dsih_write_word(dev, reg_address, temp);
}
/**
 * Write a 32-bit word to the DSI Host core
 * @param dev pointer to structure holding the DSI Host core information
 * @param reg_address offset of register
 * @return 32-bit word value stored in register
 */
unsigned int mipi_dsih_read_word(struct mipi_dsi_dev *dev, unsigned int reg_address)
{
	unsigned int ret;

	if(dev == NULL){
		pr_err("%s:Device is null\n", __func__);
		return -ENODEV;
	}
	ret = __raw_readl(dev->core_addr + reg_address);

	return ret;
}
/**
 * Write a 32-bit word to the DSI Host core
 * @param dev pointer to structure holding the DSI Host core information
 * @param reg_address offset of register in core
 * @param shift bit shift from the left (system is BIG ENDIAN)
 * @param width of bit field
 * @return bit field read from register
 */
unsigned int mipi_dsih_read_part(struct mipi_dsi_dev *dev, unsigned int reg_address, unsigned int shift, unsigned int width)
{
	return (mipi_dsih_read_word(dev, reg_address) >> shift) & ((1 << width) - 1);
}

void mipi_dsih_main_pwr_up(struct mipi_dsi_dev *dev, unsigned int pwr)
{
	mipi_dsih_write_part(dev, PWR_UP, pwr, 0, 1);
}

unsigned int mipi_dsih_main_mode_ctrl(struct mipi_dsi_dev *dev, unsigned int mode)
{
	unsigned int value = 0;
	unsigned int count = 10;
	
	/* Step 1 - Mode Request */
	mipi_dsih_write_part(dev, MODE_CTRL, mode, 0, 3);
	do {
		value = mipi_dsih_read_part(dev, MODE_STATUS, 0, 3);
		count --;
		udelay(100);
	}while(!(value == mode) && (count != 0));

	return value;
}


void mipi_dsih_main_hs_tx_timeout(struct mipi_dsi_dev *dev, unsigned int time)
{
	mipi_dsih_write_part(dev, TO_HSTX_CFG, time, 0, 15);
}

void mipi_dsih_main_hs_tx_rdy_timeout(struct mipi_dsi_dev *dev, unsigned int time)
{
	mipi_dsih_write_part(dev, TO_HSTX_CFG, time, 0, 15);
}

void mipi_dsih_main_lp_rx_timeout(struct mipi_dsi_dev *dev, unsigned int time)
{
	mipi_dsih_write_part(dev, TO_LPRX_CFG, time, 0, 15);
}

void mipi_dsih_main_lp_rx_rdy_timeout(struct mipi_dsi_dev *dev, unsigned int time)
{
	mipi_dsih_write_part(dev, TO_LPTXRDY_CFG, time, 0, 15);
}

void mipi_dsih_main_lp_tx_trig_timeout(struct mipi_dsi_dev *dev, unsigned int time)
{
	mipi_dsih_write_part(dev, TO_LPTXTRIG_CFG, time, 0, 15);
}

void mipi_dsih_main_lp_tx_ulps_timeout(struct mipi_dsi_dev *dev, unsigned int time)
{
	mipi_dsih_write_part(dev, TOP_LPTXULPS_CFG, time, 0, 15);
}

void mipi_dsih_main_lp_bta_timeout(struct mipi_dsi_dev *dev, unsigned int time)
{
	mipi_dsih_write_part(dev, TO_BTA_CFG, time, 0, 15);
}

void mipi_dsih_main_mode_type(struct mipi_dsi_dev *dev, unsigned int enable)
{
	mipi_dsih_write_part(dev, MANUAL_MODE_CFG, enable, 0, 1);
}

void mipi_dsih_phy_hs_transfer_enable(struct mipi_dsi_dev *dev, unsigned int enable)
{
	mipi_dsih_write_part(dev, PHY_MODE_CFG, enable, 12, 1);
}

void mipi_dsih_phy_ppi_width(struct mipi_dsi_dev *dev, unsigned int width)
{
	mipi_dsih_write_part(dev, PHY_MODE_CFG, width, 8, 2);
}

void mipi_dsih_phy_lanes_active(struct mipi_dsi_dev *dev, unsigned int lanes)
{
	mipi_dsih_write_part(dev, PHY_MODE_CFG, lanes, 4, 2);
}

void mipi_dsih_phy_type(struct mipi_dsi_dev *dev, unsigned int type)
{
	mipi_dsih_write_part(dev, PHY_MODE_CFG, type, 0, 1);
}

void mipi_dsih_phy_lptx_clk_div(struct mipi_dsi_dev *dev, unsigned int div)
{
	mipi_dsih_write_part(dev, PHY_CLK_CFG, div, 8, 6);
}

void mipi_dsih_phy_clk_type(struct mipi_dsi_dev *dev, unsigned int type)
{
	mipi_dsih_write_part(dev, PHY_CLK_CFG, type, 0 ,1);
}

unsigned int mipi_dsih_main_get_pwr(struct mipi_dsi_dev *dev)
{
	return mipi_dsih_read_part(dev, PWR_UP, 0, 1);
}

void mipi_dsih_dsi_bta_en(struct mipi_dsi_dev *dev, unsigned int enable)
{
	mipi_dsih_write_part(dev, DSI_GENERAL_CFG, enable, 1 ,1);
}

void mipi_dsih_dsi_eotp_tx_en(struct mipi_dsi_dev *dev, unsigned int enable)
{
	mipi_dsih_write_part(dev, DSI_GENERAL_CFG, enable, 0 ,1);
}

void mipi_dsih_dsi_tx_vcid(struct mipi_dsi_dev *dev, unsigned int vcid)
{
	mipi_dsih_write_part(dev, DSI_VCID_CFG, vcid, 0 ,2);
}

void mipi_dsih_dsi_scrambling_seed(struct mipi_dsi_dev *dev, unsigned int seed)
{
	mipi_dsih_write_part(dev, DSI_SCRAMBLING_CFG, seed, 16, 16);
}

void mipi_dsih_dsi_scrambling_en(struct mipi_dsi_dev *dev, unsigned int enable)
{
	mipi_dsih_write_part(dev, DSI_SCRAMBLING_CFG, enable, 0, 1);
}

void mipi_dsih_dsi_vid_mode_type(struct mipi_dsi_dev *dev, unsigned int vid_mode)
{
	mipi_dsih_write_part(dev, DSI_VID_TX_CFG, vid_mode, 0, 2);
}

void mipi_dsih_dsi_vfp_hs_en(struct mipi_dsi_dev *dev, unsigned int enable)
{
	mipi_dsih_write_part(dev, DSI_VID_TX_CFG, enable, 14, 1);
}

void mipi_dsih_dsi_vbp_hs_en(struct mipi_dsi_dev *dev, unsigned int enable)
{
	mipi_dsih_write_part(dev, DSI_VID_TX_CFG, enable, 13, 1);
}

void mipi_dsih_dsi_vsa_hs_en(struct mipi_dsi_dev *dev, unsigned int enable)
{
	mipi_dsih_write_part(dev, DSI_VID_TX_CFG, enable, 12, 1);
}

void mipi_dsih_dsi_hfp_hs_en(struct mipi_dsi_dev *dev, unsigned int enable)
{
	mipi_dsih_write_part(dev, DSI_VID_TX_CFG, enable, 6, 1);
}

void mipi_dsih_dsi_hbp_hs_en(struct mipi_dsi_dev *dev, unsigned int enable)
{
	mipi_dsih_write_part(dev, DSI_VID_TX_CFG, enable, 5, 1);
}

void mipi_dsih_dsi_hsa_hs_en(struct mipi_dsi_dev *dev, unsigned int enable)
{
	mipi_dsih_write_part(dev, DSI_VID_TX_CFG, enable, 4, 1);
}

void mipi_dsih_dsi_lpdt_display_cmd_en(struct mipi_dsi_dev *dev, unsigned int enable)
{
	mipi_dsih_write_part(dev, DSI_VID_TX_CFG, enable, 20, 1);
}

void mipi_dsih_dsi_max_rt_pkt_size(struct mipi_dsi_dev *dev,  unsigned int size)
{
	mipi_dsih_write_part(dev, DSI_MAX_RPS_CFG, size, 0, 16);
}

void mipi_dsih_ipi_max_pix_pkt(struct mipi_dsi_dev *dev, unsigned int size)
{
	mipi_dsih_write_part(dev, IPI_PIX_PKT_CFG, size, 0, 16);
}

void mipi_dsih_ipi_color_fmt(struct mipi_dsi_dev *dev, unsigned int fmt)
{
	mipi_dsih_write_part(dev, IPI_COLOR_MAN_CFG, fmt, 0, 4);
}

void mipi_dsih_ipi_color_depth(struct mipi_dsi_dev *dev, unsigned int depth)
{
	mipi_dsih_write_part(dev, IPI_COLOR_MAN_CFG, depth, 4, 4);
}

//TODO:
void mipi_dsih_ipi_hsa_time(struct mipi_dsi_dev *dev, unsigned int time)
{
	mipi_dsih_write_part(dev, IPI_VID_HSA_MAN_CFG, (time << 16), 0, 30);	
}

void mipi_dsih_ipi_hbp_time(struct mipi_dsi_dev *dev, unsigned int time)
{
	mipi_dsih_write_part(dev, IPI_VID_HBP_MAN_CFG, (time << 16), 0, 30);	
}

void mipi_dsih_ipi_hact_time(struct mipi_dsi_dev *dev, unsigned int time)
{
	mipi_dsih_write_part(dev, IPI_VID_HACT_MAN_CFG, (time << 16), 0, 30);	
}

void mipi_dsih_ipi_hline_time(struct mipi_dsi_dev *dev, unsigned int time)
{
	mipi_dsih_write_part(dev, IPI_VID_HLINE_MAN_CFG, (time << 16), 0, 30);	
}

void mipi_dsih_ipi_vsa_time(struct mipi_dsi_dev *dev, unsigned int time)
{
	mipi_dsih_write_part(dev, IPI_VID_VSA_MAN_CFG, time, 0, 10);	
}

void mipi_dsih_ipi_vbp_time(struct mipi_dsi_dev *dev, unsigned int time)
{
	mipi_dsih_write_part(dev, IPI_VID_VBP_MAN_CFG, time, 0, 10);	
}

void mipi_dsih_ipi_vact_time(struct mipi_dsi_dev *dev, unsigned int time)
{
	mipi_dsih_write_part(dev, IPI_VID_VACT_MAN_CFG, time, 0, 14);	
}

void mipi_dsih_ipi_vfp_time(struct mipi_dsi_dev *dev, unsigned int time)
{
	mipi_dsih_write_part(dev, IPI_VID_VFP_MAN_CFG, time, 0, 10);	
}
