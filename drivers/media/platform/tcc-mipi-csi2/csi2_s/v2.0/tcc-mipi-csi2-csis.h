/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_MIPI_CSI2_CSIS_H
#define TCC_MIPI_CSI2_CSIS_H

#include "../../tcc-mipi-csi2.h"

void tcc_mipi_csi2_reset(const struct tcc_mipi_csi2_state *state);
int tcc_mipi_csi2_mask_irq_power_off(const struct tcc_mipi_csi2_state *state);
int tcc_mipi_csi2_hw_stdby(const struct tcc_mipi_csi2_state *state);
void tcc_mipi_csi2_start(const struct tcc_mipi_csi2_state *state);
void tcc_mipi_csi2_irq_handler(const struct tcc_mipi_csi2_state *state);
void tcc_mipi_csi2_fill_timings(struct tcc_mipi_csi2_state *state);
void tcc_mipi_csi2_dump(const struct tcc_mipi_csi2_state *state);

uint32_t code_to_csi_dt(uint32_t mbus_code);
void MIPI_CSIS_Set_DPHY(const struct tcc_mipi_csi2_state *state);
void tcc_mipi_csi2_enable(const struct tcc_mipi_csi2_state *state,
			  uint32_t enable);
irqreturn_t tcc_mipi_csi2_isr(int irq, void *client_data);
irqreturn_t tcc_mipi_csi2_gdb_isr(int irq, void *client_data);
int tcc_mipi_csi2_dt_res(struct tcc_mipi_csi2_state *state);
int tcc_mipi_csi2_parse_dt(struct tcc_mipi_csi2_state *state);

#endif
