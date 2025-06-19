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

#include "tcc-mipi-csi2-ckc-reg.h"
#include "../tcc-mipi-csi2.h"
#include "../tcc-mipi-csi2-helper.h"


int tcc_mipi_csi2_ckc_enable(struct tcc_mipi_csi2_state *state)
{
	int ret;
	struct device *dev = &state->pdev->dev;
	struct device_node *node = dev->of_node;

	ret = of_property_read_u32(node, "clock-frequency", &state->csi_pclk);
	if (ret < 0) {
		loge(&(state->pdev->dev),
				"invalid clock-frequency property (%d)\n",
				ret);
	}

	if (ret == 0) {
		state->pixel_clock = clk_get(dev, "camb-pclk");
		if (IS_ERR_OR_NULL(state->pixel_clock)) {
			loge(&(state->pdev->dev),
					"clk_get returend %ld\n",
					PTR_ERR(state->pixel_clock));
			ret = (int)PTR_ERR(state->pixel_clock);
		}
	}

	if (ret == 0) {
		ret = clk_prepare(state->pixel_clock);
		if (ret < 0) {
			loge(&(state->pdev->dev),
					"clk_prepare returned %d\n",
					ret);
			clk_put(state->pixel_clock);
			state->pixel_clock = ERR_PTR(-EINVAL);
		}
	}

	if (ret == 0) {
		ret = clk_set_rate(state->pixel_clock, state->csi_pclk);
		if (ret < 0) {
			loge(&(state->pdev->dev),
					"clk_set_rate returned %d\n",
					ret);
			clk_unprepare(state->pixel_clock);
			clk_put(state->pixel_clock);
		}
	}

	if (ret == 0) {
		ret = clk_enable(state->pixel_clock);
		if (ret < 0) {
			loge(&(state->pdev->dev),
					"clk_enable returned %d\n",
					ret);
			clk_unprepare(state->pixel_clock);
			clk_put(state->pixel_clock);
		}
	}

	if (ret == 0) {
		logi(&(state->pdev->dev),
				"csi pixel_clock %dMHz\n",
				state->csi_pclk);
	}

	return ret;
}

void tcc_mipi_csi2_ckc_disable(struct tcc_mipi_csi2_state *state)
{
	if (!IS_ERR(state->pixel_clock)) {
		clk_disable(state->pixel_clock);
		clk_unprepare(state->pixel_clock);
		clk_put(state->pixel_clock);
		state->pixel_clock = ERR_PTR(-EINVAL);
	}
}
