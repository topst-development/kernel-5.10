/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */


#ifndef TCC_ISP_CORE_H
#define TCC_ISP_CORE_H
#include "../../tcc-isp.h"

void tcc_isp_core_padding(const struct tcc_isp_state *state, uint32_t code);
int32_t tcc_isp_core_load_setting(struct tcc_isp_state *state,
				  const struct firmware *fw);
void tcc_isp_core_set_io(const struct tcc_isp_state *state);
void tcc_isp_core_set_tune(const struct tcc_isp_state *state);
void tcc_isp_core_set_default(struct tcc_isp_state *state);
int32_t tcc_isp_core_get_reg(const struct tcc_isp_state *state,
			     const uint64_t reg, const uint64_t *val);
int32_t tcc_isp_core_set_reg(const struct tcc_isp_state *state,
			     const uint64_t reg, const uint64_t val);
int32_t tcc_isp_core_init(const struct tcc_isp_state *state, uint32_t enable);
void tcc_isp_core_set_deblank_on(const struct tcc_isp_state *state);
#endif
