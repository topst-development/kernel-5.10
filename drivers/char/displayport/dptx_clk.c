// SPDX-License-Identifier: GPL-2.0-or-later OR MIT
/*
 * Copyright (C) Telechips Inc.
 */


#include <linux/io.h>
#include <linux/delay.h>

#include "dptx_v14.h"
#include "dptx_reg.h"
#include "dptx_dbg.h"

#define MAX_TRY_PLL_LOCK 10

#define CHECK_REG_OFFSET(x) (((x) < (uint32_t)DP_MAX_OFFSET) ? (bool)true : (bool)false)

void Dptx_Clk_Reset_PLL(struct Dptx_Params *dev_param)
{
	uint32_t uiRegMap_PLLPMS, reg_offset;

	if (!CHECK_REG_OFFSET(dev_param->uiCKC_RegAddr_Offset)) {
		dptx_err("Invalid reg offset as 0x%x", dev_param->uiCKC_RegAddr_Offset);
	} else {

		reg_offset = (uint32_t)(dev_param->uiCKC_RegAddr_Offset + (uint32_t)DPTX_CKC_CFG_CLKCTRL0);
		Dptx_Reg_Writel(dev_param, reg_offset, (u32)0x00);

		reg_offset = (uint32_t)(dev_param->uiCKC_RegAddr_Offset + (uint32_t)DPTX_CKC_CFG_CLKCTRL1);
		Dptx_Reg_Writel(dev_param, reg_offset, (u32)0x00);

		reg_offset = (uint32_t)(dev_param->uiCKC_RegAddr_Offset + (uint32_t)DPTX_CKC_CFG_CLKCTRL2);
		Dptx_Reg_Writel(dev_param, reg_offset, (u32)0x00);

		reg_offset = (uint32_t)(dev_param->uiCKC_RegAddr_Offset + (uint32_t)DPTX_CKC_CFG_CLKCTRL3);
		Dptx_Reg_Writel(dev_param, reg_offset, (u32)0x00);

		reg_offset = (uint32_t)(dev_param->uiCKC_RegAddr_Offset + (uint32_t)DPTX_CKC_CFG_PLLPMS);
		uiRegMap_PLLPMS = Dptx_Reg_Readl(dev_param, reg_offset);
		uiRegMap_PLLPMS = (uiRegMap_PLLPMS | (uint32_t)PLLPMS_BYPASS_MASK);
		Dptx_Reg_Writel(dev_param, reg_offset, uiRegMap_PLLPMS);

		reg_offset = (uint32_t)(dev_param->uiCKC_RegAddr_Offset + (uint32_t)DPTX_CKC_CFG_PLLPMS);
		uiRegMap_PLLPMS = Dptx_Reg_Readl(dev_param, reg_offset);
		uiRegMap_PLLPMS = (uiRegMap_PLLPMS & ~((uint32_t)PLLPMS_RESETB_MASK));
		Dptx_Reg_Writel(dev_param, reg_offset, uiRegMap_PLLPMS);

		dptx_debug("Restting Clk...");
	}
}

void Dptx_Clk_Set_PLL_Divisor(struct Dptx_Params *dev_param)
{
	uint32_t cfg_axi, cfg_aux, cfg_apb;
	uint32_t reg_offset;

	#if defined(CONFIG_ARCH_TCC807X)
	/*
	 * 807x
	 * APX 400MHz, AUX 160MHz, APB 200MHz
	 */
	cfg_axi = (uint32_t)DIV_CFG_CLK_400HMZ;
	cfg_aux = (uint32_t)DIV_CFG_CLK_160HMZ;
	cfg_apb = (uint32_t)DIV_CFG_CLK_200HMZ;
	#else
	/*
	 * 805x
	 * APX 200MHz, AUX 160MHz, APB 40MHz
	 */
	cfg_axi = (uint32_t)DIV_CFG_CLK_200HMZ;
	cfg_aux = (uint32_t)DIV_CFG_CLK_160HMZ;
	cfg_apb = (uint32_t)DIV_CFG_CLK_100HMZ;
	#endif

	if (!CHECK_REG_OFFSET(dev_param->uiCKC_RegAddr_Offset)) {
		dptx_err("Invalid reg offset as 0x%x", dev_param->uiCKC_RegAddr_Offset);
	} else {

		reg_offset = (dev_param->uiCKC_RegAddr_Offset + (uint32_t)DPTX_CKC_CFG_PLLCON);
		Dptx_Reg_Writel(dev_param, reg_offset, 0x00000FC0U);

		reg_offset = (dev_param->uiCKC_RegAddr_Offset + (uint32_t)DPTX_CKC_CFG_PLLMON);
		Dptx_Reg_Writel(dev_param, reg_offset, 0x00008800U);

		reg_offset = (dev_param->uiCKC_RegAddr_Offset + (uint32_t)DPTX_CKC_CFG_CLKDIVC0);
		Dptx_Reg_Writel(dev_param, reg_offset, cfg_axi);

		reg_offset = (dev_param->uiCKC_RegAddr_Offset + (uint32_t)DPTX_CKC_CFG_CLKDIVC1);
		Dptx_Reg_Writel(dev_param, reg_offset, cfg_aux);

		reg_offset = (dev_param->uiCKC_RegAddr_Offset + (uint32_t)DPTX_CKC_CFG_CLKDIVC2);
		Dptx_Reg_Writel(dev_param, reg_offset, cfg_apb);

		reg_offset = (dev_param->uiCKC_RegAddr_Offset + (uint32_t)DPTX_CKC_CFG_PLLPMS);
		Dptx_Reg_Writel(dev_param, reg_offset, 0x05026403U);

		reg_offset = (dev_param->uiCKC_RegAddr_Offset + (uint32_t)DPTX_CKC_CFG_PLLPMS);
		Dptx_Reg_Writel(dev_param, reg_offset, 0x85026403U);

		dptx_debug("Set Clk divisor...");
	}
}

void Dptx_Clk_Set_PLL_ClkSrc(struct Dptx_Params *dev_param, uint8_t clk_source)
{
	uint32_t reg_val = 0, reg_offset;

	if (!CHECK_REG_OFFSET(dev_param->uiCKC_RegAddr_Offset)) {
		dptx_err("Invalid reg offset as 0x%x", dev_param->uiCKC_RegAddr_Offset);
	} else {
		reg_val |= clk_source;

		reg_offset = (dev_param->uiCKC_RegAddr_Offset + (uint32_t)DPTX_CKC_CFG_CLKCTRL0);
		Dptx_Reg_Writel(dev_param, reg_offset, reg_val);

		reg_offset = (dev_param->uiCKC_RegAddr_Offset + (uint32_t)DPTX_CKC_CFG_CLKCTRL1);
		Dptx_Reg_Writel(dev_param, reg_offset, reg_val);

		reg_offset = (dev_param->uiCKC_RegAddr_Offset + (uint32_t)DPTX_CKC_CFG_CLKCTRL2);
		Dptx_Reg_Writel(dev_param, reg_offset, reg_val);

		dptx_debug("Set PLL Clk source...");
	}
}

void Dptx_Clk_Get_PLLLock_Status(struct Dptx_Params *dev_param, uint8_t *pll_locked)
{
	uint32_t reg_val, loop;

	*pll_locked = 0u;
	if (!CHECK_REG_OFFSET(dev_param->uiCKC_RegAddr_Offset)) {
		dptx_err("Invalid reg offset as 0x%x", dev_param->uiCKC_RegAddr_Offset);
	} else {
		for (loop = 0u; loop < (uint32_t)MAX_TRY_PLL_LOCK; loop++) {
			reg_val = Dptx_Reg_Readl(dev_param,
						 (dev_param->uiCKC_RegAddr_Offset + (uint32_t)DPTX_CKC_CFG_PLLPMS));

			if ((reg_val & (uint32_t)DPTX_PLLPMS_LOCK_MASK) != 0U) {
				*pll_locked = 1u;
				break;
			}
			mdelay(1);
		}
	}
	if (*pll_locked == 1u) {
		/* For KCS */
		dptx_debug("Success to get PLL Locking after %u ms", loop);
	} else {
		dptx_err("Fail to get PLL Locking");
	}
}


