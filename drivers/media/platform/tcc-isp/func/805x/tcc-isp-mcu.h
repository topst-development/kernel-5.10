/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_ISP_MCU_H
#define TCC_ISP_MCU_H

#include "../../tcc-isp.h"
#include "tcc-isp-reg.h"

void tcc_isp_mcu_set_tuning_mode(uint32_t tuning_mode);
uint32_t tcc_isp_mcu_get_tuning_mode(void);
uint32_t tcc_isp_mcu_is_all_isp_stopped(void);
void tcc_isp_mcu_set_ctl_probed(struct tcc_isp_state *state,
				uint32_t is_probed);
void tcc_isp_mcu_s_stream(const struct tcc_isp_state *state, int32_t enable);
#ifdef CONFIG_VIDEO_ADV_DEBUG
int32_t tcc_isp_mcu_get_access_perm(const struct tcc_isp_state *state);
int32_t tcc_isp_mcu_rel_access_perm(const struct tcc_isp_state *state);
#endif

int32_t tcc_isp_mcu_load_firmware(struct tcc_isp_state *state,
				  const struct firmware *fw);
int32_t tcc_isp_mcu_add_drv_attr(struct platform_driver *drv);
void tcc_isp_mcu_rm_drv_attr(struct platform_driver *drv);

#endif
