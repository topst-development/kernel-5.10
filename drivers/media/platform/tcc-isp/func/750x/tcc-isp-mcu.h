/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_ISP_MCU_H
#define TCC_ISP_MCU_H

#include "../../tcc-isp.h"
#include "tcc-isp-reg.h"

uint32_t tcc_isp_mcu_is_all_isp_stopped(void);
void tcc_isp_mcu_set_ctl_probed(struct tcc_isp_state *state,
				uint32_t is_probed);
void tcc_isp_mcu_s_stream(const struct tcc_isp_state *state, int32_t enable);
int32_t tcc_isp_mcu_load_firmware(struct tcc_isp_state *state,
				  const struct firmware *fw);
int32_t tcc_isp_mcu_add_drv_attr(struct platform_driver *drv);
void tcc_isp_mcu_rm_drv_attr(struct platform_driver *drv);

#endif
