/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (C) 2018 Synopsys, Inc.
 *
 * @file dsih_core.h
 * @brief Synopsys MIPI DSI Host controller library
 * included as a part of Synopsys MIPI DSI Host controller driver.
 *
 * @author Luis Oliveira <luis.oliveira@synopsys.com>
 * Modified by Telechips
 */

#ifndef __INCLUDES_DSI_H__
#define __INCLUDES_DSI_H__

#include "dsih_includes.h"

#define VIDEO_MODE 0

void tcc_dsi_platform_init(struct mipi_dsi_dev *dev, int video_mode);

#endif

