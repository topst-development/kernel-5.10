// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <video/telechips/vioc_ddicfg.h>

#include "tcc-mipi-csi2-cfg-reg.h"
#include "../tcc-mipi-csi2.h"
#include "../tcc-mipi-csi2-helper.h"


void tcc_mipi_csi2_cfg_reset(const struct tcc_mipi_csi2_state *state,
			     uint32_t reset)
{
	if (reset == 1U) {
		/* reset state */
		// S/W reset D-PHY
		VIOC_DDICONFIG_MIPI_Reset_DPHY(state->ddicfg_base, 1U);
		// S/W reset Generic buffer interface
		VIOC_DDICONFIG_MIPI_Reset_GEN(state->ddicfg_base, 1U);
	} else {
		/* release state */
		// S/W reset D-PHY
		VIOC_DDICONFIG_MIPI_Reset_DPHY(state->ddicfg_base, 0U);
		// S/W reset Generic buffer interface
		VIOC_DDICONFIG_MIPI_Reset_GEN(state->ddicfg_base, 0U);
	}
}

