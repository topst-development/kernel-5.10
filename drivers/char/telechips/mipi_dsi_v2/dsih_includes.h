/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (C) 2018 Synopsys, Inc.
 *
 * @file includes.h
 * @brief includes file
 * included as a part of Synopsys MIPI DSI Host controller driver
 *
 * @author Luis Oliveira <luis.oliveira@synopsys.com>
 * Modified by Telechips
 */

#ifndef __INCLUDES_H__
#define __INCLUDES_H__

//#include "dsih_core.h"
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/io.h>

#define DSI_PHY_OFFSET 0x4000

/*
 * Video stream type
 */
typedef enum {
	VIDEO_NON_BURST_WITH_SYNC_PULSES = 0,
	VIDEO_NON_BURST_WITH_SYNC_EVENTS,
	VIDEO_BURST_WITH_SYNC_PULSES
} dsih_video_mode_t;

enum operation_mode {
	IDLE_MODE,
	AUTO_CALC_MODE,
	COMMAND_MODE,
	VIDEO_MODE,
	DATA_STREAM_MODE,
};

struct dsih_core_main_t {
	unsigned int manual_mode_en;
	unsigned int to_hs_tx_timeout;
	unsigned int to_hs_tx_rdy_timeout;
	unsigned int to_lp_rx_timeout;
	unsigned int to_lp_rx_rdy_timeout;
	unsigned int to_lp_tx_trig_timeout;
	unsigned int to_lp_tx_ulps_timeout;
};

struct dsih_core_phy_t
{
	/* PHY interface */
	unsigned int phy_type;
	/* number of lanes used - from device tree*/
	unsigned int phy_lanes;
	/* number of bytes in the PPI interface */
	unsigned int ppi_width;
	/* Continuous clock or Non-continuous clock */
	unsigned int clk_type;
	/* Division factor for TX Escape clock to be generated from sys_clk */
	unsigned int phy_lptx_clk_div;
	/* Configures PHY transition time from low-power to high-speed transmission
	 * used in mannual mode
	 */
	unsigned int phy_lp2hs_time;
	/* Configures PHY transition time from high-speed to low-power transmission
	 * used in mannual mode
	 */
	unsigned int phy_hs2lp_time;
	/* Configures byte time for low-power data transmission */
	unsigned int phy_esc_byte_time;
	/*  Configures ratio between frequencies of HSTX clock and IPI clock */
	unsigned int phy_ipi_ratio;
	/* Ratio of frequencies phy_hstx_clk / sys_clk (manual) */
	unsigned int phy_sys_ratio;
	/* Time needed to complete deskew calibration, given in cycles of sys_clk */
	unsigned int phy_cal_time;
	/* Configures the PHY wakeup time that controller will consider when performing an ULPS exit request */
	unsigned int phy_wakeup_time;
};

struct dsih_core_dsi_t
{
	/* Enables Bus Turn Around (BTA) procedures.   */
	unsigned int bta_en;
	/* Enables the EoTp transmission in high-speed. */
	unsigned int eotp_tx_en;
	/* Scrambling enable.   */
	unsigned int scrambling_en;
	/* Configures the video mode transmission type. */
	unsigned int vid_mode_type;
};

struct dsih_core_ipi_t
{
	/* Configures the IPI color depth. */
	unsigned int ipi_depth;
	/* Configures the IPI pixel format. */
	unsigned int ipi_format;
	/* Configures the Horizontal Sync Active period measured in cycles of phy_hstx_clk */
	unsigned int vid_hsa_time;
	/* Configures the Horizontal Back Porch period measured in cycles of phy_hstx_clk. */
	unsigned int vid_hbp_time;
	/* Configures the Horizontal Front Porch period measured in cycles of phy_hstx_clk. */
	unsigned int vid_hfp_time;
	/* Configures the Horizontal Active period measured in cycles of phy_hstx_clk. */
	unsigned int vid_hact_time;
	/* Configures the total line time (HSA+HBP+HACT+HFP) measured in cycles of phy_hstx_clk. */
	unsigned int vid_hline_time;

	/* Configures the Vertical Sync Active period measured in lines.  */
	unsigned int vid_vsa_lines;
	/* Configures the Vertical Back Porch period measured in lines.  */
	unsigned int vid_vbp_lines;
	/* Configures the Vertical Front Active region period measured in lines.  */
	unsigned int vid_vact_lines;
	/* Configures the Vertical Front Porch period measured in lines.   */
	unsigned int vid_vfp_lines;
	/* Configures the number of pixels in a single video packet, 
	 * used in Video mode (for non-burst modes) and Data Stream mode.
	 */
	unsigned int max_pix_pkt;
};


/**
 * Main structures to instantiate the driver
 */
struct mipi_dsi_dev {

    /** HW version */
    unsigned int hw_version;
	unsigned int port;
	unsigned int automode;
    /* MIPI DSI Controller */
    void __iomem *core_addr; // dsi register
    void __iomem *phy_addr; // cfg register
    void __iomem *cfg_addr; // cfg register

	// data_rate [Mhz]
    unsigned long data_rate;
	// pixel_clock (Khz)
    unsigned long pclk;
	// phy_hxtx_clk [Khz]
	unsigned int phy_hstx_clk;
	// sys_clk [Khz]
	unsigned int sys_clk;

	struct dsih_core_main_t main_cfg;
	struct dsih_core_phy_t phy_cfg;
	struct dsih_core_dsi_t dsi_cfg;
	struct dsih_core_ipi_t ipi_cfg;
};

#endif /* __INCLUDES_H__ */
