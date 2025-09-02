// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (C) 2018 Synopsys, Inc.
 *
 * @file dsih_api.c
 * @brief Synopsys MIPI DSI driver API
 * included as a part of Synopsys MIPI DSI Host controller driver
 *
 * @author Luis Oliveira <luis.oliveira@synopsys.com>
 * Modified by Telechips
 */

#include "dsih_api.h"
#include "dsih_hal.h"
#include "dsih_dphy.h"

/**
 * @short Configure IPI video interface
 * @param dev pointer to structure holding the DSI Host core information
 * @param video_params pointer to video stream-to-send information
 * @return error code
 */
unsigned int mipi_dsih_dpi_get_hlbcc(unsigned int hcomponent, unsigned long pclk, unsigned int lane_mbps)
{
	unsigned long lbcc;
#if 1
	unsigned int frac;
	lbcc = hcomponent * lane_mbps * 1000 / 8;
	frac = lbcc % pclk;
	lbcc = lbcc / pclk;
	if(frac) {
		pr_err("round up\n");
			lbcc++;
	}
	pr_err("lbcc : %lu\n", lbcc);
#else
	unsigned long t_pix;
	unsigned long t_byte;
	unsigned long lbclk;

	lbclk = lane_mbps / 8 * 1000;

	t_pix = 1000000000000 / pclk;
	t_byte = 1000000000000 / lbclk;

	t_pix *= 100000;
	t_byte *= 10;
	pr_err("\n\nT_PIXE : %ldns\nT_BYTE : %ldns\n", t_pix, t_byte);
	lbcc = (hcomponent * (t_pix / t_byte));
	pr_err("lbcc : %ld, remainder : %ld \n", lbcc, lbcc % 10000);
	if(lbcc % 10000) {
		lbcc+= 10000;
	}
	lbcc /= 10000;
#endif	
	return lbcc;
}


static void mipi_dsih_main_config(struct mipi_dsi_dev *dev)
{
	/* Step 1 - Timeouts Configuration */
	/* Value zero turns the timeouts off */
	mipi_dsih_main_hs_tx_timeout(dev, 0); // set no-timeout
	mipi_dsih_main_hs_tx_rdy_timeout(dev, 0); // set no-timeout
	mipi_dsih_main_lp_rx_timeout(dev, 0);
	mipi_dsih_main_lp_rx_rdy_timeout(dev, 0);
	mipi_dsih_main_lp_tx_trig_timeout(dev, 0);
	mipi_dsih_main_lp_tx_ulps_timeout(dev, 0);
	mipi_dsih_main_lp_bta_timeout(dev, 0);

	/* Step 2 - Manual/Auto Mode Configuration*/
	mipi_dsih_main_mode_type(dev, dev->main_cfg.manual_mode_en);  //1: manual mode 0: automatic
}

static void mipi_dsih_phy_config(struct mipi_dsi_dev *dev)
{
	// unsigned int res = 0;

	/* Step 1 - PHY Type Selection */
	mipi_dsih_phy_type(dev, 0); //[0]   phy_type : DPHY

	/* Step 2 - Number of lanes Configuration */
	mipi_dsih_phy_lanes_active(dev, dev->phy_cfg.phy_lanes - 1); //[5:4] phy_lanes

	/* Step 3 - PPI width Configuration */
	mipi_dsih_phy_ppi_width(dev, dev->phy_cfg.ppi_width); //[9:8] ppi_width

	/* Step 4 - High Speed Transfer Enable Configuration */
	mipi_dsih_phy_hs_transfer_enable(dev, 0); //[12] hs_transferen_en

	/* Step 5 - Configure the Clock Mode */
	mipi_dsih_phy_clk_type(dev, 0); //[0]    clk_type // continuous

	/* Step 6 - Configuration the PHY Clock Low-Power */
	// phy_lptx_clk = sys_clk / 2 * div 
	// 65,625Khz = 292,000 Khz / 2 * dev
	// 2 * dev = 4.49
	// dev = 2.22....
	mipi_dsih_phy_lptx_clk_div(dev, 2);

	if(dev->main_cfg.manual_mode_en) {
		/* Manuual Mode */

		/* Step 7 - PHY/IPI Clock Ratio Configuration */
		//res = div_to_fixed_point(dev->phy_hstx_clk, dev->pclk, 16, 0x3FFFFF);
		// mipi_dsih_phy_ipi_ratio(dev, res);
		mipi_dsih_write_word(dev, 0x134, 0x0000c8e1);

		/* Step 8 - PHY/SYS Clock Ratio Configuration */
		// res = div_to_fixed_point(dev->phy_hstx_clk, dev->sys_clk, 16, 0x3FFFF);
		// mipi_dsih_phy_sys_ratio(dev, res);
		mipi_dsih_write_word(dev, 0x13c, 0x000038da);

		/* Step 9 - PHY Transition Timings Configuration */
		// mipi_dsih_phy_hs2lp_time(dev, hs2lp_time);
		// mipi_dsih_phy_lp2hs_time(dev, lp2hs_time);
		mipi_dsih_write_word(dev, 0x10c, 0x0013fa2e);
		mipi_dsih_write_word(dev, 0x114, 0x0011fac3);

		/* Step 10 - PHY Maximum Read Time Configuration */
		/* Should be computed according panel specification */
		/* cycles of phy_hstx_clk */
		// mipi_dsih_phy_sys_ratio(dev, field_phy_max_rd_time, 8 * dev->phy_hstx_clk / 1000);

		/* Step 11 - PHY ESC Command Time Configuration */
		mipi_dsih_write_word(dev, 0x124, 0x00171890);

		/* Step 12 - PHY Byte Transmission Time Configuration */
		mipi_dsih_write_word(dev, 0x12C, 0x000e3680);
	}
}

static void mipi_dsih_dsi_config(struct mipi_dsi_dev *dev)
{
	/* Step 1 - DSI2 General Configuration */
	mipi_dsih_dsi_bta_en(dev, 0);
	mipi_dsih_dsi_eotp_tx_en(dev , 1);

	/* Step 2 - DSI2 Virtual Channel Configuration */
	mipi_dsih_dsi_tx_vcid(dev, 0);

	/* Step 3 - DSI2 Scrambling Configuration */
	mipi_dsih_dsi_scrambling_seed(dev, 0);
	mipi_dsih_dsi_scrambling_en(dev, 0);

	/* Step 4 - DSI2 Video Transmission Configuration */
	mipi_dsih_dsi_vid_mode_type(dev, VIDEO_NON_BURST_WITH_SYNC_PULSES);
	mipi_dsih_dsi_vfp_hs_en(dev, 0);
	mipi_dsih_dsi_vbp_hs_en(dev, 0);
	mipi_dsih_dsi_vsa_hs_en(dev, 0);
	mipi_dsih_dsi_hfp_hs_en(dev, 0);
	mipi_dsih_dsi_hbp_hs_en(dev, 0);
	mipi_dsih_dsi_hsa_hs_en(dev, 0);
	mipi_dsih_dsi_lpdt_display_cmd_en(dev, 0);

	/* Step 5 - Maximum Return Packet Size Configuration */
	mipi_dsih_dsi_max_rt_pkt_size(dev, 1000); //500

}
#if 1
static void mipi_dsih_ipi_config(struct mipi_dsi_dev *dev)
{
	unsigned int res;
	/* Step 1 - Pixel Packet Size Configuration */
	mipi_dsih_ipi_max_pix_pkt(dev, 0);

	if(!dev->main_cfg.manual_mode_en)
		return;
	
	/* Step 2 - Color Coding Configuration */
	mipi_dsih_ipi_color_fmt(dev, 0);
	mipi_dsih_ipi_color_depth(dev, 5);

	//Set Timing
	/* Step 3 - Horizontal Sync Active Configuration */
	res = mipi_dsih_dpi_get_hlbcc(dev->ipi_cfg.vid_hsa_time, dev->pclk, dev->phy_hstx_clk);
	mipi_dsih_ipi_hsa_time(dev, res);
	// mipi_dsih_write_word(dev, IPI_VID_HSA_MAN_CFG, 0x0003f5be);

	/* Step 4 - Horizontal Back Porch Configuration */
	res = mipi_dsih_dpi_get_hlbcc(dev->ipi_cfg.vid_hbp_time, dev->pclk, dev->phy_hstx_clk);
	mipi_dsih_ipi_hbp_time(dev, res);
	// mipi_dsih_write_word(dev, IPI_VID_HBP_MAN_CFG, 0x000ddc19);

	/* Step 5 - Horizontal Active Time Configuration */
	res = mipi_dsih_dpi_get_hlbcc(dev->ipi_cfg.vid_hact_time, dev->pclk, dev->phy_hstx_clk);
	mipi_dsih_ipi_hact_time(dev, res);
	// mipi_dsih_write_word(dev, IPI_VID_HACT_MAN_CFG, 0x03b66220);

	/* Step 6 - Horizontal Line Time Configuration */
	res = mipi_dsih_dpi_get_hlbcc(dev->ipi_cfg.vid_hline_time, dev->pclk, dev->phy_hstx_clk);
	mipi_dsih_ipi_hline_time(dev, res);
	// mipi_dsih_write_word(dev, IPI_VID_HLINE_MAN_CFG, 0x03d61010);

	/* Step 7 - Vertical Sync Active Configuration */
	mipi_dsih_ipi_vsa_time(dev, dev->ipi_cfg.vid_vsa_lines);

	/* Step 8 - Vertical Back Porch Configuration */
	mipi_dsih_ipi_vbp_time(dev, dev->ipi_cfg.vid_vbp_lines);

	/* Step 9 - Vertical Active Configuration */
	mipi_dsih_ipi_vact_time(dev, dev->ipi_cfg.vid_vact_lines);

	/* Step 10 - Vertical Front Porch Configuration */
	mipi_dsih_ipi_vfp_time(dev, dev->ipi_cfg.vid_vfp_lines);
}
#endif

int mipi_dsih_ipi_video(struct mipi_dsi_dev *dev)
{

	//phy_hstx_clk [Khz]
	unsigned int phy_hstx_clk;
	// unsigned int apb_data = 0;
	unsigned int r_mode = 0;
	unsigned int count = 100;

	dev->phy_cfg.ppi_width = DSI2_DPHY_16_BITS;

	phy_hstx_clk = dev->data_rate * 1000;
	if (dev->phy_cfg.ppi_width == DSI2_DPHY_32_BITS)
		phy_hstx_clk /= 32;
	else if (dev->phy_cfg.ppi_width == DSI2_DPHY_16_BITS)
		phy_hstx_clk /= 16;
	else
		phy_hstx_clk /= 8;
	dev->phy_hstx_clk = phy_hstx_clk;

	pr_err("[%s] DSI Port %d enabling\n",
		 __func__,dev->port);
	// --------------------------------------------------------------------------------------------------------
	// 1. Reset APB Interface
	// 2. MAIN Configuration
	// 3. PHY Configuration
	// 4. DSI Configuration
	// 5. IPI Configuration
	// 6. Interrupt Mask
	// 7. PHY Initialization
	// 8. Wake-Up Core
	// Automatic Mode
	// -> 9. Auto-Calculation
	// -> 10. Operating Mode Selection
	// 9. Operating Mode Selection
	// --------------------------------------------------------------------------------------------------------

	/* Step 2 - MAIN Configuration */
	mipi_dsih_main_config(dev);

	/* Step 3 - PHY Configuration */
	mipi_dsih_phy_config(dev);

	/* Step 4 - DSI Configuration */
	mipi_dsih_dsi_config(dev);
	
	/* Step 5 - IPI Configuration */
	mipi_dsih_ipi_config(dev);


	/* Step 6 - Interrupt Masks Configuration */
	mipi_dsih_write_word(dev, 0x404, 0x3001f);
	mipi_dsih_write_word(dev, 0x414, 0x7f);
	mipi_dsih_write_word(dev, 0x424, 0xffff);
	mipi_dsih_write_word(dev, 0x434, 0xf010f);
	mipi_dsih_write_word(dev, 0x444, 0x3);
	mipi_dsih_write_word(dev, 0x454, 0x3f007f);

	/* Step 7 - PHY Initialization */
	// Done

	/* Step 8 - Wake-Up Core */
	mipi_dsih_main_pwr_up(dev, 1);
	
	// 9. Operating Mode Selection
	mipi_dsih_write_word(dev, 0x018   , 0x00000001);//auto-mode
	do {
		// auto mode to AutoCalculation mode
		r_mode = mipi_dsih_read_part(dev, 0x1c, 0, 3);
		count --;
	}while(!(r_mode == 1) && (count != 0));
	if(count == 0) {
		pr_err("[%s][%d] failed to switch to AutoCalculation mode\n",
			__func__, __LINE__);
	} else {
		pr_info("AutoCalculation mode verified\n");
	}
	count = 100;

	mipi_dsih_write_word(dev, 0x018   , 0x00000003);//videomode
	do {
		// videomode
		r_mode = mipi_dsih_read_part(dev, 0x1c, 0, 3);
		ndelay(400);
		count --;
	}while(!(r_mode == 3) && (count != 0));
	if(count == 0) {
		pr_info("[%s] failed to switch to videomode, No DSI Data\n",
		__func__);
	} else {
		pr_info("Video mode verified\n");
	}

	return 0;
}

