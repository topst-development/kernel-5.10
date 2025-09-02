/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (C) 2018 Synopsys, Inc.
 *
 * @file dsih_api.h
 * @brief Synopsys MIPI DSI Host controller driver API
 * included as a part of Synopsys MIPI DSI Host controller driver
 *
 * @author Luis Oliveira <luis.oliveira@synopsys.com>
 * Modified by Telechips
 */

#ifndef MIPI_DSIH_API_H_
#define MIPI_DSIH_API_H_

#include "dsih_core.h"

int mipi_dsih_ipi_video(struct mipi_dsi_dev *dev);

#endif	/* MIPI_DSIH_API_H_ */
