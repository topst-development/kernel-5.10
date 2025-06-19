/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_ISP_COMMON_H
#define TCC_ISP_COMMON_H

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include "../tcc-isp.h"

int32_t tcc_isp_cmm_read_file(struct tcc_isp_state *state,
			      const char *file_name,
			      int32_t (*load_func)(struct tcc_isp_state *state,
						   const struct firmware *fw));
int32_t tcc_isp_cmm_add_drv_attr(struct platform_driver *drv);
void tcc_isp_cmm_rm_drv_attr(struct platform_driver *drv);
int32_t tcc_isp_cmm_add_dev_attr(const struct tcc_isp_state *state);
#endif
