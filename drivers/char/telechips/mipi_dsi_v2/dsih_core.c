// SPDX-License-Identifier: GPL-2.0
/**
 * Copyright (C) 2018 Synopsys, Inc.
 *
 * @file dsih_core.c
 * @brief Synopsys MIPI DSI driver API
 * included as a part of Synopsys MIPI DSI Host controller driver
 *
 * @author Luis Oliveira <luis.oliveira@synopsys.com>
 * Modified by Telechips
 */

#include "dsih_core.h"
#include "dsih_api.h"
#include "dsih_hal.h"
/**
* @short Start DSI platform
* @param[in] dev MIPI DSI device
* @param[in] display Type of display
* @param[in] video_mode Video mode
* @return none
*/

void tcc_dsi_platform_init(struct mipi_dsi_dev *dev, int video_mode)
{
	pr_info("%s:DSI initialization\n", __func__);
	pr_info("%s:DSI Open\n", __func__);
	
	
	switch (video_mode)
	{
	case VIDEO_MODE:
		/* video mode */
		if (mipi_dsih_ipi_video(dev))
			pr_err("error configuring video\n");

		break;
	case 1:
	pr_err("Closeing video\n");
		mipi_dsih_main_pwr_up(dev, 0);
		mipi_dsih_write_part(dev, 0x10, 0, 0, 0);
		mipi_dsih_write_part(dev, 0x10, 1, 0, 0);
		mipi_dsih_write_part(dev, 0x10, 2, 0, 0);
		break;
	default:
		pr_err( "Invalid mode\n");
		break;
	}

}