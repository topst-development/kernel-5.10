/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_MIPI_CSI2_CKC_H
#define TCC_MIPI_CSI2_CKC_H
#include "../tcc-mipi-csi2.h"

int tcc_mipi_csi2_ckc_enable(struct tcc_mipi_csi2_state *state);
void tcc_mipi_csi2_ckc_disable(struct tcc_mipi_csi2_state *state);

#endif
