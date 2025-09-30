/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_MIPI_CSI2_DPHYS_H
#define TCC_MIPI_CSI2_DPHYS_H

#include "../../tcc-mipi-csi2.h"

void tcc_mipi_csi2_dphys_set_clock_lane(const struct tcc_mipi_csi2_state *state);
void tcc_mipi_csi2_dphys_set_data_lane(const struct tcc_mipi_csi2_state *state,
				       uint32_t lane);
void tcc_mipi_csi2_dphys_enable_clock_lane(
	const struct tcc_mipi_csi2_state *state, bool on);
void tcc_mipi_csi2_dphys_enable_data_lane(
	const struct tcc_mipi_csi2_state *state, uint32_t lane, bool on);
#endif
