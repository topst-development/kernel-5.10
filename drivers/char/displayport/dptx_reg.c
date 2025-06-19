// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 Synopsys, Inc.
 *
 * Synopsys DP TX Linux Software Driver and documentation (hereinafter,
 * "Software") is an Unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto. You are permitted to use and
 * redistribute this Software in source and binary forms, with or without
 * modification, provided that redistributions of source code must retain this
 * notice. You may not view, use, disclose, copy or distribute this file or
 * any information contained herein except pursuant to this license grant from
 * Synopsys. If you do not agree with this notice, including the disclaimer
 * below, then you are not authorized to use the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 * Modified by Telechips Inc.
 */

#include <linux/io.h>

#include "dptx_v14.h"
#include "dptx_reg.h"
#include "dptx_dbg.h"


static uint32_t dptx_internal_readl(const void __iomem *reg_base_addr,
				    uint32_t reg_offset)
{
	void const *alighed_reg_target_addr;
	uintptr_t reg_target_addr;
	uint32_t reg_value = 0u;

	reg_target_addr = (uintptr_t)reg_base_addr + (uintptr_t)reg_offset;

	if ((reg_target_addr & 3u) == 0u) {
		alighed_reg_target_addr = (void const *)reg_target_addr;
		reg_value = __raw_readl(alighed_reg_target_addr);
	}

	return reg_value;
}

static void dptx_internal_writel(const void __iomem *reg_base_addr,
				 uint32_t reg_offset,
				 uint32_t reg_value)
{
	void *alighed_reg_target_addr;
	uintptr_t reg_target_addr;

	reg_target_addr = (uintptr_t)reg_base_addr + (uintptr_t)reg_offset;

	if ((reg_target_addr & 3u) == 0u) {
		alighed_reg_target_addr = (void *)reg_target_addr;
		__raw_writel(reg_value, alighed_reg_target_addr);
	}
}

uint32_t Dptx_Reg_Readl(const struct Dptx_Params *dev_param, uint32_t reg_offset)
{
	uint32_t reg_value;

	reg_value = dptx_internal_readl(dev_param->pioDPLink_BaseAddr,
					reg_offset);

	return reg_value;
}

void Dptx_Reg_Writel(const struct Dptx_Params *dev_param, uint32_t reg_offset, uint32_t reg_value)
{
	dptx_internal_writel(dev_param->pioDPLink_BaseAddr,
			     reg_offset, reg_value);
}

uint32_t Dptx_MCU_DP_Reg_Read(const struct Dptx_Params *dev_param, uint32_t reg_offset)
{
	uint32_t reg_value;

	reg_value = dptx_internal_readl(dev_param->pioMIC_SubSystem_BaseAddr,
					reg_offset);
	return reg_value;
}

void Dptx_MCU_DP_Reg_Write(const struct Dptx_Params *dev_param, uint32_t reg_offset, uint32_t reg_value)
{
	dptx_internal_writel(dev_param->pioMIC_SubSystem_BaseAddr,
			     reg_offset, reg_value);
}

uint32_t Dptx_PMU_Reg_Read(const struct Dptx_Params *dev_param, uint32_t reg_offset)
{
	uint32_t reg_value;

	reg_value = dptx_internal_readl(dev_param->pioPMU_BaseAddr,
					reg_offset);
	return reg_value;
}

void Dptx_PMU_Reg_Write(const struct Dptx_Params *dev_param, uint32_t reg_offset, uint32_t reg_value)
{
	dptx_internal_writel(dev_param->pioPMU_BaseAddr,
			     reg_offset, reg_value);
}

uint32_t dptx_phy_read(const struct Dptx_Params *dev_param, uint32_t address)
{
	const uint32_t phy_read_bit = BIT(PHYREG_CMDADDR_PHY_READ_SHIFT);
	uint32_t phy_cmd = phy_read_bit |
			      (address & PHYREG_CMDADDR_PHY_ADDRESS_MASK);
	uint32_t phy_data;
	uint32_t loop;

	Dptx_Reg_Writel(dev_param, DPTX_PHYREG_CMDADDR, phy_cmd);


	for (loop = 0u; loop < 10u; loop++) {
		phy_cmd = Dptx_Reg_Readl(dev_param, DPTX_PHYREG_CMDADDR);
		if ((phy_cmd & phy_read_bit) == 0u) {
			/* The for loop exits as the PHY becomes ready. */
			break;
		}
		udelay(1);
	}
	phy_data = Dptx_Reg_Readl(dev_param, DPTX_PHYREG_DATA);
	phy_data &= PHYREG_DATA_PHY_DATA_DATA_MAS;

	return phy_data;
}



int32_t dptx_phy_write(const struct Dptx_Params *dev_param, uint32_t address,
		       uint32_t phy_data)
{
	const uint32_t phy_write_bit = BIT(PHYREG_CMDADDR_PHY_WRITE_SHIFT);
	uint32_t phy_cmd = phy_write_bit |
			      (address & PHYREG_CMDADDR_PHY_ADDRESS_MASK);
	uint32_t loop;
	int32_t ret = 0;

	Dptx_Reg_Writel(dev_param, DPTX_PHYREG_DATA, (phy_data & 0xFFFFu));
	Dptx_Reg_Writel(dev_param, DPTX_PHYREG_CMDADDR, phy_cmd);

	for (loop = 0u; loop < 10u; loop++) {
		phy_cmd = Dptx_Reg_Readl(dev_param, DPTX_PHYREG_CMDADDR);
		if ((phy_cmd & phy_write_bit) == 0u) {
			/* The for loop exits as the PHY becomes write. */
			break;
		}
		udelay(1);
	}
	return ret;
}

