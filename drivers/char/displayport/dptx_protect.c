// SPDX-License-Identifier: GPL-2.0-or-later OR MIT
/*
* Copyright (C) Telechips Inc.
*/



#include <asm/io.h>

#include "dptx_v14.h"
#include "dptx_reg.h"
#include "dptx_dbg.h"

#define CHECK_REG_OFFSET(x) (((x) < (uint32_t)DP_MAX_OFFSET) ? (bool)true : (bool)false)

int32_t Dptx_Protect_Set_PW(struct Dptx_Params *pstDptx)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiPW, uiReg_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(pstDptx->uiProtect_RegAddr_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", pstDptx->uiProtect_RegAddr_Offset);

		iRetVal = DPTX_RETURN_EACCES;

		goto return_funcs;
	}

	uiPW = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)DP_PORTECT_CFG_SEC_PW_VAL : (uint32_t)DP_PORTECT_CFG_SNPS_PW_VAL;
	uiReg_Offset = (uint32_t)(pstDptx->uiProtect_RegAddr_Offset + DP_PORTECT_CFG_PW_OK);

	Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiPW);

	dptx_debug("RegisterBank_PW[0x%x] to 0x%x", uiReg_Offset, (uint32_t)uiPW);

return_funcs:
	return iRetVal;
}

int32_t Dptx_Protect_Set_CfgLock(struct Dptx_Params *pstDptx, bool bLocked)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiW_RegVal, uiReg_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(pstDptx->uiProtect_RegAddr_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", pstDptx->uiProtect_RegAddr_Offset);

		iRetVal = DPTX_RETURN_EACCES;

		goto return_funcs;
	}

	uiW_RegVal = bLocked ? (uint32_t)DP_PORTECT_CFG_LOCKED : (uint32_t)DP_PORTECT_CFG_UNLOCKED;
	uiReg_Offset = (uint32_t)(pstDptx->uiProtect_RegAddr_Offset + DP_PORTECT_CFG_LOCK);

	Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiW_RegVal);

	dptx_debug("RegisterBank_CfgLock[0x%x] to %u, w data : 0x%x", uiReg_Offset, (uint32_t)bLocked, uiW_RegVal);

return_funcs:
	return iRetVal;
}


int32_t Dptx_Protect_Set_CfgAccess(struct Dptx_Params *pstDptx, bool bAccessible)
{
	bool bOffsetInRange;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiW_RegVal, uiReg_Offset;

	bOffsetInRange = CHECK_REG_OFFSET(pstDptx->uiProtect_RegAddr_Offset);
	if (!bOffsetInRange) {
		dptx_err("Invalid reg offset as 0x%x", pstDptx->uiProtect_RegAddr_Offset);

		iRetVal = DPTX_RETURN_EACCES;

		goto return_funcs;
	}

	uiW_RegVal = bAccessible ? (uint32_t)DP_PORTECT_CFG_ACCESSABLE : (uint32_t)DP_PORTECT_CFG_NOT_ACCESSABLE;
	uiReg_Offset = (uint32_t)(pstDptx->uiProtect_RegAddr_Offset + DP_PORTECT_CFG_ACCESS);

	Dptx_Reg_Writel(pstDptx, uiReg_Offset, uiW_RegVal);

	dptx_debug("Cfg access[0x%x] to %u, w data : 0x%x", uiReg_Offset, (uint32_t)bAccessible, uiW_RegVal);

return_funcs:
	return iRetVal;
}



