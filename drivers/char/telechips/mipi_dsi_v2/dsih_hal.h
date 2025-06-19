/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (C) 2018 Synopsys, Inc.
 *
 * @file dsih_hal.h
 * @brief Hardware Abstraction Level of DWC Synopsys MIPI DSI HOST controller
 * included as a part of Synopsys MIPI DSI Host controller driver
 *
 * @author Luis Oliveira <luis.oliveira@synopsys.com>
 * Modified by Telechips
 */

#ifndef MIPI_DSIH_HAL_H_
#define MIPI_DSIH_HAL_H_

#include "dsih_includes.h"

//DSI
#define PWR_UP                  0x00C
#define MODE_CTRL               0x018
#define MODE_STATUS             0x01C

#define MANUAL_MODE_CFG         0x024
#define TO_HSTX_CFG             0x048
#define TO_HSTXRDY_CFG          0x04C
#define TO_LPRX_CFG             0x050
#define TO_LPTXRDY_CFG          0x054
#define TO_LPTXTRIG_CFG         0x058
#define TOP_LPTXULPS_CFG        0x05C
#define TO_BTA_CFG              0x060

#define PHY_MODE_CFG            0x100
#define PHY_CLK_CFG             0x104

#define PRI_TX_CMD              0x1C0
#define PRI_CAL_CTRL            0x1C8

#define DSI_GENERAL_CFG         0x200
#define DSI_VCID_CFG            0x204
#define DSI_SCRAMBLING_CFG      0x208
#define DSI_VID_TX_CFG          0x20C
#define DSI_MAX_RPS_CFG         0x210

#define CRI_TX_HDR              0x2C0
#define CRI_TX_PLD              0x2C4
#define CRI_RX_HDR              0x2C8
#define CRI_RX_PLD              0x2CC

#define IPI_COLOR_MAN_CFG       0x300
#define IPI_VID_HSA_MAN_CFG		0x304
#define IPI_VID_HSA_AUTO		0x308
#define IPI_VID_HBP_MAN_CFG		0x30c
#define IPI_VID_HBP_AUTO		0x310
#define IPI_VID_HACT_MAN_CFG		0x314
#define IPI_VID_HACT_AUTO		0x318
#define IPI_VID_HLINE_MAN_CFG		0x31c
#define IPI_VID_HLINE_AUTO		0x320
#define IPI_VID_VSA_MAN_CFG		0x324
#define IPI_VID_VSA_AUTO		0x328
#define IPI_VID_VBP_MAN_CFG		0x32c
#define IPI_VID_VBP_AUTO		0x330
#define IPI_VID_VACT_MAN_CFG		0x334
#define IPI_VID_VACT_AUTO		0x338
#define IPI_VID_VFP_MAN_CFG		0x33c
#define IPI_VID_VFP_AUTO		0x340
#define IPI_PIX_PKT_CFG         0x344

/* bsp abstraction */
void mipi_dsih_write_word(struct mipi_dsi_dev *dev, unsigned int reg_address, unsigned int data);
void mipi_dsih_write_part(struct mipi_dsi_dev *dev, unsigned int reg_address, unsigned int data, unsigned int shift, unsigned int width);
unsigned int mipi_dsih_read_word(struct mipi_dsi_dev *dev, unsigned int reg_address);
unsigned int mipi_dsih_read_part(struct mipi_dsi_dev *dev, unsigned int reg_address, unsigned int shift, unsigned int width);

void mipi_dsih_main_pwr_up(struct mipi_dsi_dev *dev, unsigned int pwr);
unsigned int mipi_dsih_main_mode_ctrl(struct mipi_dsi_dev *dev, unsigned int mode);
void mipi_dsih_main_hs_tx_timeout(struct mipi_dsi_dev *dev, unsigned int time);
void mipi_dsih_main_hs_tx_rdy_timeout(struct mipi_dsi_dev *dev, unsigned int time);
void mipi_dsih_main_lp_rx_timeout(struct mipi_dsi_dev *dev, unsigned int time);
void mipi_dsih_main_lp_rx_rdy_timeout(struct mipi_dsi_dev *dev, unsigned int time);
void mipi_dsih_main_lp_tx_trig_timeout(struct mipi_dsi_dev *dev, unsigned int time);
void mipi_dsih_main_lp_tx_ulps_timeout(struct mipi_dsi_dev *dev, unsigned int time);
void mipi_dsih_main_lp_bta_timeout(struct mipi_dsi_dev *dev, unsigned int time);
void mipi_dsih_main_mode_type(struct mipi_dsi_dev *dev, unsigned int enable);
void mipi_dsih_phy_hs_transfer_enable(struct mipi_dsi_dev *dev, unsigned int enable);
void mipi_dsih_phy_ppi_width(struct mipi_dsi_dev *dev, unsigned int width);
void mipi_dsih_phy_lanes_active(struct mipi_dsi_dev *dev, unsigned int lanes);
void mipi_dsih_phy_type(struct mipi_dsi_dev *dev, unsigned int type);
void mipi_dsih_phy_lptx_clk_div(struct mipi_dsi_dev *dev, unsigned int div);
void mipi_dsih_phy_clk_type(struct mipi_dsi_dev *dev, unsigned int type);
void mipi_dsih_dsi_bta_en(struct mipi_dsi_dev *dev, unsigned int enable);
unsigned int mipi_dsih_main_get_pwr(struct mipi_dsi_dev *dev);
void mipi_dsih_dsi_eotp_tx_en(struct mipi_dsi_dev *dev, unsigned int enable);
void mipi_dsih_dsi_tx_vcid(struct mipi_dsi_dev *dev, unsigned int vcid);
void mipi_dsih_dsi_scrambling_seed(struct mipi_dsi_dev *dev, unsigned int seed);
void mipi_dsih_dsi_scrambling_en(struct mipi_dsi_dev *dev, unsigned int enable);
void mipi_dsih_dsi_vid_mode_type(struct mipi_dsi_dev *dev, unsigned int vid_mode);
void mipi_dsih_dsi_vfp_hs_en(struct mipi_dsi_dev *dev, unsigned int enable);
void mipi_dsih_dsi_vbp_hs_en(struct mipi_dsi_dev *dev, unsigned int enable);
void mipi_dsih_dsi_vsa_hs_en(struct mipi_dsi_dev *dev, unsigned int enable);
void mipi_dsih_dsi_hfp_hs_en(struct mipi_dsi_dev *dev, unsigned int enable);
void mipi_dsih_dsi_hbp_hs_en(struct mipi_dsi_dev *dev, unsigned int enable);
void mipi_dsih_dsi_hsa_hs_en(struct mipi_dsi_dev *dev, unsigned int enable);
void mipi_dsih_dsi_lpdt_display_cmd_en(struct mipi_dsi_dev *dev, unsigned int enable);
void mipi_dsih_dsi_max_rt_pkt_size(struct mipi_dsi_dev *dev,  unsigned int size);
void mipi_dsih_ipi_max_pix_pkt(struct mipi_dsi_dev *dev, unsigned int size);
void mipi_dsih_ipi_color_fmt(struct mipi_dsi_dev *dev, unsigned int fmt);
void mipi_dsih_ipi_color_depth(struct mipi_dsi_dev *dev, unsigned int depth);
void mipi_dsih_ipi_hsa_time(struct mipi_dsi_dev *dev, unsigned int time);
void mipi_dsih_ipi_hbp_time(struct mipi_dsi_dev *dev, unsigned int time);
void mipi_dsih_ipi_hact_time(struct mipi_dsi_dev *dev, unsigned int time);
void mipi_dsih_ipi_hline_time(struct mipi_dsi_dev *dev, unsigned int time);
void mipi_dsih_ipi_vsa_time(struct mipi_dsi_dev *dev, unsigned int time);
void mipi_dsih_ipi_vbp_time(struct mipi_dsi_dev *dev, unsigned int time);
void mipi_dsih_ipi_vact_time(struct mipi_dsi_dev *dev, unsigned int time);
void mipi_dsih_ipi_vfp_time(struct mipi_dsi_dev *dev, unsigned int time);
#endif	/* MIPI_DSI_API_H_ */
