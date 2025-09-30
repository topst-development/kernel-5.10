/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_MIPI_CSI2_CFG_H
#define TCC_MIPI_CSI2_CFG_H

#include "../tcc-mipi-csi2.h"

void tcc_mipi_csi2_cfg_reset(const struct tcc_mipi_csi2_state *state,
			     uint32_t reset);

#endif
