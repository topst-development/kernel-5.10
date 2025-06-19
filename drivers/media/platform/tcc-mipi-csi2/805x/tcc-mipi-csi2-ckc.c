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

static void tcc_mipi_csi2_ckc_set_div(const struct tcc_mipi_csi2_state *state,
				      uint32_t onOff,
				      uint32_t pdiv)
{
	uint32_t val = 0U, target = 0U;

	target = (((onOff) << CLKDIVC_PE_SHIFT) |
		  ((pdiv) << CLKDIVC_PDIV_SHIFT));

	val = tcc_mipi_csi2_readl(state->ckc_base, CLKDIVC);

	if (val != target) {
		val &= ~(CLKDIVC_PE_MASK | CLKDIVC_PDIV_MASK);
		val |= target;

		tcc_mipi_csi2_writel(val, state->ckc_base, CLKDIVC);
	} else {
		/* pll divisor is already set */
		logi(&(state->pdev->dev), "skip setting divisor\n");
	}
}

static int tcc_mipi_csi2_ckc_set_pms(const struct tcc_mipi_csi2_state *state,
				 uint32_t p,
				 uint32_t m,
				 uint32_t s)
{
	uint32_t val = 0U, target;
	int retval = 0;

	target = ((((uint32_t)1U) << PLLPMS_RESETB_SHIFT) |
		  (PLLPMS_LOCK_MASK) |
		  ((p) << PLLPMS_P_SHIFT) |
		  ((m) << PLLPMS_M_SHIFT) |
		  ((s) << PLLPMS_S_SHIFT));
	val = tcc_mipi_csi2_readl(state->ckc_base, PLLPMS);

	if ((val & target) != target) {
		val &= ~(PLLPMS_S_MASK | PLLPMS_M_MASK | PLLPMS_P_MASK);

		val |= (((p) << PLLPMS_P_SHIFT) |
			((m) << PLLPMS_M_SHIFT) |
			((s) << PLLPMS_S_SHIFT));

		tcc_mipi_csi2_writel(val, state->ckc_base, PLLPMS);

		usleep_range(1000U, 2000U);

		val |= (((uint32_t)1U) << PLLPMS_RESETB_SHIFT);

		tcc_mipi_csi2_writel(val, state->ckc_base, PLLPMS);

		usleep_range(1000U, 2000U);

		val = (tcc_mipi_csi2_readl(state->ckc_base, PLLPMS) &
		       PLLPMS_LOCK_MASK);

		if (val == PLLPMS_LOCK_MASK) {
			/* success lock */
			retval = 0;
		} else {
			/* fail lock */
			retval = -EBUSY;
		}
	} else {
		/* PMS is already set */
		logi(&(state->pdev->dev), "skip setting PMS\n");
		retval = 0;
	}

	return retval;
}

static void tcc_mipi_csi2_ckc_set_clksrc(const struct tcc_mipi_csi2_state *state,
				 uint32_t src,
				 uint32_t offset)
{
	uint32_t val = 0, target = 0;

	target = (src << CLKCTRL_SEL_SHIFT);

	val = tcc_mipi_csi2_readl(state->ckc_base, offset);

	if ((val & target) != target) {
		val &= ~(CLKCTRL_CHGRQ_MASK | CLKCTRL_SEL_MASK);
		val |= (src << CLKCTRL_SEL_SHIFT);
		tcc_mipi_csi2_writel(val, state->ckc_base, offset);
	} else {
		/* CLKSRC selection is already set */
		logi(&(state->pdev->dev),
				"skip setting %s SRC\n",
				(offset == MIPI_BUS_CLK) ?
				"MIPI_BUS_CLK" : "MIPI_PIXEL_CLK");
	}
}

int tcc_mipi_csi2_ckc_enable(const struct tcc_mipi_csi2_state *state)
{
	int ret = 0;
	uint32_t pll_div = 5, pll_p = 2, pll_m = 200, pll_s = 2;
	uint32_t sel_pclk = CLKCTRL_SEL_PLL_DIRECT;
	uint32_t sel_bclk = CLKCTRL_SEL_PLL_DIVIDER;
	const char * const clksrc_sel[] = {
		"XIN", "PLL_DIRECT_OUTPUT", "PLL_DIVIDER_OUTPUT"};

	/*
	 * XIN is 24Mhz
	 * set pixel clock 600MHz
	 * set bus clock 100MHz
	 */
	logi(&(state->pdev->dev), "DIV(%d), PMS(%d %d %d)\n",
		pll_div, pll_p, pll_m, pll_s);
	tcc_mipi_csi2_ckc_set_div(state, ON, pll_div);
	ret = tcc_mipi_csi2_ckc_set_pms(state, pll_p, pll_m, pll_s);
	if (ret < 0) {
		loge(&(state->pdev->dev), "FAIL - MIPI WRAP PLL SETTING\n");
	} else {
		/*
		 * set source clock to PLL
		 */
		logi(&(state->pdev->dev), "BUSCLK SRC(%s), PIXELCLK SRC(%s)\n",
				clksrc_sel[sel_bclk], clksrc_sel[sel_pclk]);
		tcc_mipi_csi2_ckc_set_clksrc(state, sel_bclk, MIPI_BUS_CLK);
		tcc_mipi_csi2_ckc_set_clksrc(state, sel_pclk, MIPI_PIXEL_CLK);
	}

	return ret;
}

void tcc_mipi_csi2_ckc_disable(const struct tcc_mipi_csi2_state *state)
{

}

