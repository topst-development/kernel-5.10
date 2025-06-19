// SPDX-License-Identifier: GPL-2.0-or-later OR MIT
/*
* Copyright (C) Telechips Inc.
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>

#if defined(CONFIG_PM)
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#endif

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/pm.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include "soc/telechips/chipinfo.h"

#include "dptx_v14.h"
#include "dptx_reg.h"
#include "dptx_dbg.h"
#include "dptx_drm_dp_addition.h"


static struct Dptx_Params *pstHandle;

static int32_t dptx_v14_register_callback(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	pstDptx->pvStr_Resume_CallBack = Str_Resume_CallBabck;

	(void)Dptx_Intr_Register_HPD_Callback(pstDptx, Hpd_Intr_CallBabck);
	(void)Dptx_Intr_Register_Panel_Callback(pstDptx, Panel_Topology_CallBabck);

	return iRetVal;
}

static int32_t dptx_init_resource(struct platform_device *pDev, struct Dptx_Params *pstDptx)
{
	uint32_t uiElement;
	void __iomem *pioVirAddr;
	const struct resource *pstResource;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	for(uiElement = 0; uiElement < 3U; uiElement++) {
		pstResource = platform_get_resource(pDev, IORESOURCE_MEM, uiElement);
		if (pstResource == NULL) {
			dptx_err("can't get %u device resource\n", uiElement);

			iRetVal = -ENOMEM;

			/* [FP] misra_c_2012_rule_15_1_violation*/
			goto return_funcs;
		}

		pioVirAddr = devm_ioremap(&pDev->dev, pstResource->start, pstResource->end - pstResource->start);
		if (pioVirAddr == NULL) {
			dptx_err("Failed to remap %u device resource\n", uiElement);

			iRetVal = -ENOMEM;

			/* [FP] misra_c_2012_rule_15_1_violation*/
			goto return_funcs;
		}

		if(uiElement == 0U) {
			/* For KCS */
			pstDptx->pioDPLink_BaseAddr = pioVirAddr;
		} else if(uiElement == 1U) {
			/* For KCS */
			pstDptx->pioMIC_SubSystem_BaseAddr = pioVirAddr;
		} else {
			/* For KCS */
			pstDptx->pioPMU_BaseAddr = pioVirAddr;
		}
	}

return_funcs:
	return iRetVal;

}

static int32_t dptx_v14_init_os(struct platform_device *pDev, struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	/* [FP] misra_c_2012_rule_14_4_violation*/
	/* [FP] cert_dcl37_c_violation*/
	mutex_init(&pstDptx->Mutex);

	/* [FP] misra_c_2012_rule_14_4_violation*/
	/* [FP] cert_dcl37_c_violation*/
	init_waitqueue_head(&pstDptx->WaitQ);

	init_completion(&pstDptx->hpd_plug_comp);

	atomic_set(&pstDptx->Sink_request, 0);
	atomic_set(&pstDptx->HPD_IRQ_State, 0);

	pstDptx->iHPD_IRQ = platform_get_irq(pDev, 0);

	if (pstDptx->iHPD_IRQ < 0) {
		dptx_err("No IRQ\n");

		iRetVal = -ENODEV;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	iRetVal = devm_request_threaded_irq(pstDptx->pstParentDev,
												(uint32_t)pstDptx->iHPD_IRQ,
												Dptx_Intr_IRQ,
												Dptx_Intr_Threaded_IRQ,
												((unsigned long)IRQF_SHARED | (unsigned long)IRQ_LEVEL),
												"Dpv14_Tx",
												pstDptx);
		if (iRetVal !=  DPTX_RETURN_NO_ERROR) {
			/* For KCS */
			dptx_err("from devm_request_threaded_irq()");
		}

return_funcs:
	return iRetVal;
}

static int32_t dptx_v14_init_params(struct Dptx_Params *pstDptx, struct device *pstParentDev)
{
	uint8_t ucDpIdx;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	pstDptx->pstParentDev				= pstParentDev;
	pstDptx->uiHDCP22_RegAddr_Offset	= DP_HDCP_OFFSET;
	pstDptx->uiRegBank_RegAddr_Offset	= DP_REGISTER_BANK_OFFSET;
	pstDptx->uiCKC_RegAddr_Offset		= DP_CKC_OFFSET;
	pstDptx->uiProtect_RegAddr_Offset	= DP_PROTECT_OFFSET;
	pstDptx->uiSEC_PHY_Reg_Offset		= DP_SEC_PHY_OFFSET;

	pstDptx->eEstablished_Timing		= DMT_NOT_SUPPORTED;

#if defined(CONFIG_ARCH_TCC807X)
	pstDptx->ePhy_Dev					= PHY_DEVICE_SEC;
#else
	pstDptx->ePhy_Dev					= PHY_DEVICE_SNPS;
#endif

	pstDptx->uiTCC80xx_Rev = get_chip_rev();

	pstDptx->pucEdidBuf = (uint8_t *)kzalloc((size_t)DPTX_EDID_BUFLEN, GFP_KERNEL);
	if (pstDptx->pucEdidBuf == NULL) {
		/* For KCS */
		dptx_err("Failed to alloc base EDID Buf");
	}

	for (ucDpIdx = 0; ucDpIdx < (uint8_t)PHY_INPUT_STREAM_MAX; ucDpIdx++) {
		/* [FP] misra_c_2012_rule_11_5_violation*/
		/* [FP] misra_c_2012_rule_10_8_violation*/
		pstDptx->paucEdidBuf_Entry[ucDpIdx] = (uint8_t *)kzalloc((size_t)DPTX_EDID_BUFLEN, GFP_KERNEL);

		if (pstDptx->paucEdidBuf_Entry[ucDpIdx] == NULL) {
			/* For KCS */
			dptx_err("DP %d : failed to alloc EDID Buf", ucDpIdx);
		} else {
			/* For KCS */
			(void)memset(pstDptx->paucEdidBuf_Entry[ucDpIdx], 0, (size_t)DPTX_EDID_BUFLEN);
		}
	}

	pstHandle = pstDptx;

	return iRetVal;
}


static int32_t dptx_v14_init_drv_params(struct platform_device *pDev, struct Dptx_Params *pstDptx, u32 uiPeri_PClk[PHY_INPUT_STREAM_MAX])
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	(void)dptx_v14_init_params(pstDptx, &pDev->dev);
	(void)Dptx_Core_Init_Params(pstDptx);
	(void)Dptx_Cfg_Init_Params(pstDptx);
	(void)Dptx_VidIn_Init_Params(pstDptx, uiPeri_PClk);
	(void)Dptx_Intr_Init_Params(pstDptx);
	(void)Dptx_Api_Init_Params();

	return iRetVal;
}

static int32_t dptx_v14_init_link(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	/* Wait 100ms for DP link ready to detect HPD */
	mdelay(100);

	if (dptx_intr_get_hotplug_status(pstDptx)) {
		pstDptx->eLast_HPDStatus = HPD_STATUS_PLUGGED;
	} else {
		pstDptx->eLast_HPDStatus = HPD_STATUS_UNPLUGGED;
		dptx_err("Hot unplugged...");

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	iRetVal = Dptx_Link_Perform_BringUp(pstDptx);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	iRetVal = Dptx_Link_Perform_Training(pstDptx, pstDptx->ucMax_Rate, pstDptx->ucMax_Lanes);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	iRetVal = (pstDptx->bMultStreamTransport) ? Dptx_Ext_Set_Topology_Configuration(pstDptx) : DPTX_RETURN_NO_ERROR;

return_funcs:
	return iRetVal;
}

static void dptx_v14_init_clk(struct Dptx_Params *pstDptx)
{
	uint8_t ucPLL_LockStatus;

	Dptx_Clk_Reset_PLL(pstDptx);
	Dptx_Clk_Set_PLL_Divisor(pstDptx);
	Dptx_Clk_Get_PLLLock_Status(pstDptx, &ucPLL_LockStatus);
	Dptx_Clk_Set_PLL_ClkSrc(pstDptx, (u8)CLKCTRL_PLL_DIVIDER_OUTPUT);
}

static void dptx_v14_init_protect(struct Dptx_Params *pstDptx)
{
	Dptx_Protect_Set_PW(pstDptx);
	Dptx_Protect_Set_CfgLock(pstDptx, (bool)DP_PORTECT_CFG_UNLOCKED);
	Dptx_Protect_Set_CfgAccess(pstDptx, (bool)DP_PORTECT_CFG_ACCESSABLE);
}

int32_t dptx_v14_set_reg_ap_access(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiReg_Addr;
	uint32_t uiRegMap_R_ApbSel, uiRegMap_W_ApbSel;

	uiReg_Addr = (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)DPTX_APB_SEL_SEC_PHY : (uint32_t)DPTX_APB_SEL_SNOP_PHY;

	uiRegMap_R_ApbSel = Dptx_MCU_DP_Reg_Read(pstDptx, (uint32_t)uiReg_Addr);
	uiRegMap_W_ApbSel = (uint32_t)(uiRegMap_R_ApbSel | BIT(DPTX_APB_SEL_MASK));
	Dptx_MCU_DP_Reg_Write(pstDptx, (uint32_t)uiReg_Addr, uiRegMap_W_ApbSel);

	dptx_debug("Register access to AP...Reg[0x%x]:0x%08x -> 0x%08x", uiReg_Addr, uiRegMap_R_ApbSel, uiRegMap_W_ApbSel);

	return iRetVal;
}

int32_t dptx_v14_release_coldrst_mask(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegMap_R_HsmRst_Msk, uiRegMap_W_HsmRst_Msk;

	uiRegMap_R_HsmRst_Msk = Dptx_PMU_Reg_Read(pstDptx, (uint32_t)PMU_HSM_RSTN_MASK);

	if ((uiRegMap_R_HsmRst_Msk & PMU_COLD_RESET_MASK) != 0U) {
		uiRegMap_W_HsmRst_Msk = (uint32_t)(uiRegMap_R_HsmRst_Msk & ~PMU_COLD_RESET_MASK);
		Dptx_PMU_Reg_Write(pstDptx, (uint32_t)PMU_HSM_RSTN_MASK, uiRegMap_W_HsmRst_Msk);

		dptx_debug("DP Cold reset mask release...0x%08x -> 0x%08x", uiRegMap_R_HsmRst_Msk, uiRegMap_W_HsmRst_Msk);
	}

	return iRetVal;
}

static int32_t dptx_reset_serdes(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	if (pstDptx->pvStr_Resume_CallBack != NULL) {
		/* For KCS */
		iRetVal = pstDptx->pvStr_Resume_CallBack();
	}

	return iRetVal;
}

static void dptx_v14_init_pre_condition(struct Dptx_Params *pstDptx)
{
	(void)dptx_reset_serdes(pstDptx);
	(void)dptx_v14_release_coldrst_mask(pstDptx);
	(void)dptx_v14_set_reg_ap_access(pstDptx);
}

static int32_t dptx_v14_init(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	/* Initialize max lanes and max rate */
	pstDptx->ucMax_Rate = DPTX_PHYIF_CTRL_RATE_HBR3;
	pstDptx->ucMax_Lanes = PHY_LANE_4;

	(void)dptx_v14_init_pre_condition(pstDptx);
	(void)dptx_v14_init_protect(pstDptx);
	(void)dptx_v14_init_clk(pstDptx);

	(void)Dptx_Cfg_Init(pstDptx, pstDptx->ucMax_Rate);
	(void)Dptx_Sec_PHY_Init(pstDptx, pstDptx->ucMax_Rate, pstDptx->ucMax_Lanes);
	(void)Dptx_Core_Init(pstDptx);

	iRetVal = dptx_v14_init_link(pstDptx);

	return iRetVal;
}

static int32_t dptx_v14_deinit(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	(void)Dptx_Core_Deinit(pstDptx);
	(void)Dptx_Intr_Handle_HotUnplug(pstDptx);

	return iRetVal;
}

static int32_t dptx_v14_probe_drv(struct platform_device *pDev, struct Dptx_Params *pstDptx, u32 uiPeri_PClk[PHY_INPUT_STREAM_MAX])
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	(void)dptx_v14_init_drv_params(pDev, pstDptx, uiPeri_PClk);
	(void)Dptx_Ext_Proc_Interface_Init(pstDptx);
	(void)dptx_v14_register_callback(pstDptx);

	Dptx_Core_Disable_Global_Intr(pstDptx);

	(void)dptx_v14_init_os(pDev, pstDptx);

	Dptx_Core_Enable_Global_Intr(pstDptx, (uint32_t)(DPTX_IEN_HPD | DPTX_IEN_HDCP | DPTX_IEN_SDP));

	return iRetVal;
}

static int32_t of_parse_dp_dt(struct Dptx_Params *pstDptx,
										struct device_node *pstOfNode,
										uint32_t *puiPeri0_PClk,
										uint32_t *puiPeri1_PClk,
										uint32_t *puiPeri2_PClk,
										uint32_t *puiPeri3_PClk)
{
	uint8_t ucDpIdx;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t uiVcp_Id[PHY_INPUT_STREAM_MAX];
	uint64_t ulDispX_Peri_Clk;
	struct clk *DISP0_Peri_Clk, *DISP1_Peri_Clk;
	struct clk *DISP2_Peri_Clk, *DISP3_Peri_Clk;

	DISP0_Peri_Clk = of_clk_get_by_name(pstOfNode, "disp0-clk");
	if (DISP0_Peri_Clk != NULL) {
		ulDispX_Peri_Clk = clk_get_rate(DISP0_Peri_Clk);
		*puiPeri0_PClk = (uint32_t)((ulDispX_Peri_Clk / 1000U) & 0xFFFFFFFFU);
	} else {
		dptx_err("Can't find DISP0 Peri clock\n");
		*puiPeri0_PClk = 0;
	}

	DISP1_Peri_Clk = of_clk_get_by_name(pstOfNode, "disp1-clk");
	if (DISP1_Peri_Clk != NULL) {
		ulDispX_Peri_Clk = clk_get_rate(DISP1_Peri_Clk);
		*puiPeri1_PClk = (uint32_t)((ulDispX_Peri_Clk / 1000U) & 0xFFFFFFFFU);
	} else {
		dptx_err("Can't find DISP1 Peri clock\n");
		*puiPeri1_PClk = 0;
	}

	DISP2_Peri_Clk = of_clk_get_by_name(pstOfNode, "disp2-clk");
	if (DISP0_Peri_Clk != NULL) {
		ulDispX_Peri_Clk = clk_get_rate(DISP2_Peri_Clk);
		*puiPeri2_PClk = (uint32_t)((ulDispX_Peri_Clk / 1000U) & 0xFFFFFFFFU);
	} else {
		dptx_err("Can't find DISP2 Peri clock\n");
		*puiPeri2_PClk = 0;
	}

	DISP3_Peri_Clk = of_clk_get_by_name(pstOfNode, "disp3-clk");
	if (DISP3_Peri_Clk != NULL) {
		ulDispX_Peri_Clk = clk_get_rate(DISP3_Peri_Clk);
		*puiPeri3_PClk = (uint32_t)((ulDispX_Peri_Clk / 1000U) & 0xFFFFFFFFU);
	} else {
		dptx_err("Can't find DISP3 Peri clock\n");
		*puiPeri3_PClk = 0;
	}

	iRetVal = of_property_read_u32_array(pstOfNode, "sink_vcp_id", uiVcp_Id, (size_t)PHY_INPUT_STREAM_MAX);
	if (iRetVal < 0) {
		dptx_err("Can't get Sink VCP Id.. set to default");

		for (ucDpIdx  = 0; ucDpIdx < (uint8_t)PHY_INPUT_STREAM_MAX; ucDpIdx++) {
			pstDptx->aucVCP_Id[ucDpIdx] = (ucDpIdx + 1U);
		}
	} else {
		for (ucDpIdx  = 0; ucDpIdx < (uint8_t)PHY_INPUT_STREAM_MAX; ucDpIdx++) {
			pstDptx->aucVCP_Id[ucDpIdx] = (uint8_t)uiVcp_Id[ucDpIdx];
		}
	}

	return DPTX_RETURN_NO_ERROR;
}

struct Dptx_Params *Dpv14_Tx_Get_Device_Handle(void)
{
	return pstHandle;
}


int tcc_device_match_of_node(struct device *dev, const void *np)
{
	return dev->of_node == np;
}

static struct device *
tcc_bus_find_device_by_of_node(struct bus_type *bus, const struct device_node *np)
{
	return bus_find_device(bus, NULL, np, tcc_device_match_of_node);
}


static void dptv_v14_add_i2c_link(struct device *pdev, struct Dptx_Params *pDptxParams) {
	struct device *pstI2cDev = NULL;
	struct device_node *pstDevNode = NULL;

	//Set device link for sorting a suspend/resume sequence
	pstDevNode = of_find_node_by_name(NULL, "i2c");
	if (pstDevNode != NULL) {
		pstI2cDev = tcc_bus_find_device_by_of_node(&platform_bus_type, pstDevNode);
		if (pstI2cDev != NULL) {
			pDptxParams->pstI2cLink = device_link_add(pdev, pstI2cDev, DL_FLAG_STATELESS);
		}
	}

	if (pDptxParams->pstI2cLink != NULL) {
		dev_info(pdev, "[INFO] Success to link i2c device!\n");
	}
	else {
		dev_err(pdev, " [ERR] Failed to link i2c device\n");
	}
}

static void dptv_v14_remove_i2c_link(struct Dptx_Params *pDptxParams) {
	device_link_del(pDptxParams->pstI2cLink);
}

static int32_t Dpv14_Tx_Probe(struct platform_device *pdev)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint32_t auiPeri_Pixel_Clock[PHY_INPUT_STREAM_MAX] = { 0, };
	struct Dptx_Params *pstDptx;
	const struct Dptx_Video_Params *pstVideoParams;

	if (pdev->dev.of_node == NULL) {
		dptx_err("Node wasn't found");

		iRetVal = -ENODEV;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	/* [FP] misra_c_2012_rule_10_8_violation*/
	/* [FP] misra_c_2012_rule_11_5_violation*/
	pstDptx = (struct Dptx_Params *)devm_kzalloc(&pdev->dev, sizeof(*pstDptx), GFP_KERNEL);
	if (pstDptx == NULL) {
		dptx_err("Can't alloc Dev memory");

		iRetVal = -ENOMEM;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	(void)memset(pstDptx, 0, sizeof(struct Dptx_Params));

	iRetVal = dptx_init_resource(pdev, pstDptx);
	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

#ifdef CONFIG_OF
	iRetVal = of_parse_dp_dt(pstDptx, pdev->dev.of_node,
							&auiPeri_Pixel_Clock[PHY_INPUT_STREAM_0],
							&auiPeri_Pixel_Clock[PHY_INPUT_STREAM_1],
							&auiPeri_Pixel_Clock[PHY_INPUT_STREAM_2],
							&auiPeri_Pixel_Clock[PHY_INPUT_STREAM_3]);

	if (iRetVal != DPTX_RETURN_NO_ERROR) {
		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}
#endif

	platform_set_drvdata(pdev, pstDptx);

	iRetVal = dptx_v14_probe_drv(pdev, pstDptx, auiPeri_Pixel_Clock);

	if (iRetVal == DPTX_RETURN_NO_ERROR) {
		pstVideoParams = &pstDptx->stVideoParams;

		dptx_dbg("TCC-DPTX-Ver: %d.%d.%d", TCC_DPTX_DRV_MAJOR_VER, TCC_DPTX_DRV_MINOR_VER, TCC_DPTX_DRV_SUBTITLE_VER);
#if defined(CONFIG_ARCH_TCC807X)
		dptx_dbg("  TCC807X %s", (pstDptx->uiTCC80xx_Rev == (uint32_t)TCC80XX_REVISION_ES) ? "ES" :
									  (pstDptx->uiTCC80xx_Rev == (uint32_t)TCC80XX_REVISION_CS) ? "CS" :"BX");
#elif defined(CONFIG_ARCH_TCC805X)
		dptx_dbg("  TCC805X %s", (pstDptx->uiTCC80xx_Rev == (uint32_t)TCC80XX_REVISION_ES) ? "ES" :
									  (pstDptx->uiTCC80xx_Rev == (uint32_t)TCC80XX_REVISION_CS) ? "CS" :"BX");
#endif
		dptx_dbg("  Hot %s ", (pstDptx->eLast_HPDStatus == HPD_STATUS_PLUGGED) ? "Plugged" : "Unplugged");
		dptx_dbg("  Sink device: %s ", (pstDptx->bSideBand_MSG_Supported) ? "exernal monitor" : "lcd panel");
		dptx_dbg("  Num of DPs: %u - %s mode ",
								(pstDptx->ucNumOfPorts),
								(pstDptx->bMultStreamTransport) ? "MST" : "SST");
		dptx_dbg("  Max rate: %s",
						(pstDptx->ucMax_Rate == (uint8_t)DPTX_PHYIF_CTRL_RATE_RBR) ? "RBR" :
						(pstDptx->ucMax_Rate == (uint8_t)DPTX_PHYIF_CTRL_RATE_HBR) ? "HBR" :
						(pstDptx->ucMax_Rate == (uint8_t)DPTX_PHYIF_CTRL_RATE_HBR2) ? "HB2" : "HBR3");
		dptx_dbg("  Max lane: %s",
						(pstDptx->ucMax_Lanes == (uint8_t)1U) ? "1 lane" :
						(pstDptx->ucMax_Lanes == (uint8_t)2U) ? "2 lanes" :"4 lanes");
		dptx_dbg("  Mux: %u %u %u %u", pstDptx->aucMux_Idx[0], pstDptx->aucMux_Idx[1], pstDptx->aucMux_Idx[2], pstDptx->aucMux_Idx[3]);
		dptx_dbg("  PHY Lanes cfg: %s", (pstDptx->bPhy_Lane_Std) ? "standard" : "swap");
		dptx_dbg("  SDM Path: %s", (pstDptx->bSDM_Bypass) ? "bypass" : "use video data");
		dptx_dbg("  TRVC Path: %s", (pstDptx->bTRVC_Bypass) ? "bypass" : "use video data");
		dptx_dbg("  VCP Id: %u %u %u %u", pstDptx->aucVCP_Id[0], pstDptx->aucVCP_Id[1], pstDptx->aucVCP_Id[2], pstDptx->aucVCP_Id[3]);
		dptx_dbg("  VIC: %u %u %u %u",
					pstVideoParams->auiVideo_Code[0], pstVideoParams->auiVideo_Code[1],
					pstVideoParams->auiVideo_Code[2], pstVideoParams->auiVideo_Code[3]);
		dptx_dbg("  PClk: %u %u %u %u",
					pstVideoParams->uiPeri_Pixel_Clock[0], pstVideoParams->uiPeri_Pixel_Clock[1],
					pstVideoParams->uiPeri_Pixel_Clock[2], pstVideoParams->uiPeri_Pixel_Clock[3]);
		dptx_dbg("  %d streams enable -> Pixel encoding = %s, Pixel mode = %s",
					pstDptx->ucNumOfPorts,
					(pstVideoParams->ucPixel_Encoding == (uint8_t)PIXEL_ENCODING_TYPE_RGB) ? "RGB" :
					(pstVideoParams->ucPixel_Encoding == (uint8_t)PIXEL_ENCODING_TYPE_YCBCR422) ? "YCbCr422" : "YCbCr444",
					(pstVideoParams->ucMultiPixel == 0) ? "Single" : (pstVideoParams->ucMultiPixel == 1U) ? "Dual" : "Quad");
	}

	dptv_v14_add_i2c_link(&pdev->dev, pstDptx);

return_funcs:
	return iRetVal;
}

/* [FP] misra_c_2012_rule_8_13_violation*/
static int Dpv14_Tx_Remove(struct platform_device *pstDev)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	struct Dptx_Params *pstDptx;

	dptx_dbg("Remove: DP V1.4 driver ");

	/* [FP] misra_c_2012_rule_11_5_violation*/
	pstDptx = (struct Dptx_Params *)platform_get_drvdata(pstDev);

	iRetVal = dptx_v14_deinit(pstDptx);

	dptv_v14_remove_i2c_link(pstDptx);

	mutex_destroy(&pstDptx->Mutex);

	return iRetVal;
}

int32_t Dpv14_Tx_Suspend_T(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;

	dptx_dbg("*** PM Suspend Test ***");

	iRetVal = dptx_v14_deinit(pstDptx);

	dptx_dbg("Suspend: Hot %s, Ports = %d\n",
				(pstDptx->eLast_HPDStatus == HPD_STATUS_PLUGGED) ? "Plugged":"Unplugged",
				pstDptx->ucNumOfPorts);

	return iRetVal;
}

int32_t Dpv14_Tx_Resume_T(struct Dptx_Params *pstDptx)
{
	uint8_t ucRetry_MSTAct = 0;
	int32_t iPinRet, iRetVal = DPTX_RETURN_NO_ERROR;
	const struct Dptx_Video_Params *pstVideoParams;

	dptx_dbg("*** PM Resume Test ***");

	pstVideoParams = &pstDptx->stVideoParams;

	iPinRet = pinctrl_pm_select_default_state(pstDptx->pstParentDev);
	if (iPinRet != 0) {
		/* For KCS */
		dptx_err("from pinctrl_pm_select_default_state()");
	}

	dptx_dbg("Resume start: Hot %s, Ports = %d, PClk %d %d %d %d",
				(pstDptx->eLast_HPDStatus == HPD_STATUS_PLUGGED) ? "Plugged":"Unplugged",
				pstDptx->ucNumOfPorts,
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_0],
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_1],
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_2],
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_3]);

	do {
		iRetVal = dptx_v14_init(pstDptx);
		if ((iRetVal != DPTX_RETURN_NO_ERROR) && (iRetVal != DPTX_RETURN_MST_ACT_TIMEOUT)) {
			/* [FP] misra_c_2012_rule_15_1_violation*/
			goto return_funcs;
		}

		if (iRetVal == DPTX_RETURN_MST_ACT_TIMEOUT) {
			if (ucRetry_MSTAct == 0U) {
				dptx_info("\n==== Re-initialize DP Link ===\n");

				ucRetry_MSTAct = 1U;

				(void)dptx_v14_deinit(pstDptx);
			} else {
				dptx_err("MST ACT timeout...");
				ucRetry_MSTAct = 0U;
			}
		}
	} while (ucRetry_MSTAct == 1U);

	dptx_dbg("Resume end: Hot %s, Ports = %d, PClk %d %d %d %d",
				(pstDptx->eLast_HPDStatus == HPD_STATUS_PLUGGED) ? "Plugged":"Unplugged",
				pstDptx->ucNumOfPorts,
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_0],
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_1],
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_2],
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_3]);

return_funcs:
	return iRetVal;
}

#if defined(CONFIG_PM)
/* [FP] misra_c_2012_rule_8_13_violation*/
static int Dpv14_Tx_Device_Driver_Suspend(struct device *dev)
{
	int32_t iRetVal  = DPTX_RETURN_NO_ERROR;
	int32_t iPinRet;
	struct Dptx_Params *pstDptx;

	dptx_err("dp v14 suspend ");
	/* [FP] misra_c_2012_rule_11_5_violation*/
	pstDptx = (struct Dptx_Params *)dev_get_drvdata(dev);

	iPinRet = pinctrl_pm_select_sleep_state(dev);
	if (iPinRet != 0) {
		/* For KCS */
		dptx_err("from pinctrl_pm_select_sleep_state()");
	}

	iRetVal = dptx_v14_deinit(pstDptx);

	return iRetVal;
}

static int Dpv14_Tx_Device_Driver_Resume(struct device *dev)
{
	uint8_t ucRetry_MSTAct = 0;
	int32_t iPinRet, iRetVal = DPTX_RETURN_NO_ERROR;
	struct Dptx_Params *pstDptx;
	const struct Dptx_Video_Params *pstVideoParams;

	dptx_err("dp v14 resume ");
	/* [FP] misra_c_2012_rule_11_5_violation*/
	pstDptx = (struct Dptx_Params *)dev_get_drvdata(dev);

	pstVideoParams = &pstDptx->stVideoParams;

	iPinRet = pinctrl_pm_select_default_state(dev);
	if (iPinRet != 0) {
		/* For KCS */
		dptx_err("from pinctrl_pm_select_default_state()");
	}

	dptx_dbg("Resume: Hot %s, Ports = %d, PClk %d %d %d %d\n",
				(pstDptx->eLast_HPDStatus == HPD_STATUS_PLUGGED) ? "Plugged":"Unplugged",
				pstDptx->ucNumOfPorts,
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_0],
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_1],
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_2],
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_3]);

	do {
		iRetVal = dptx_v14_init(pstDptx);
		if ((iRetVal != DPTX_RETURN_NO_ERROR) && (iRetVal != DPTX_RETURN_MST_ACT_TIMEOUT)) {
			/* [FP] misra_c_2012_rule_15_1_violation*/
			goto return_funcs;
		}

		if (iRetVal == DPTX_RETURN_MST_ACT_TIMEOUT) {
			if (ucRetry_MSTAct == 0U) {
				dptx_notice("\n==== Re-initialize DP Link ===\n");

				ucRetry_MSTAct = 1U;

				(void)dptx_v14_deinit(pstDptx);
			} else {
				dptx_err("MST ACT timeout...");
				ucRetry_MSTAct = 0U;
			}
		}
	} while (ucRetry_MSTAct == 1U);

return_funcs:
	return iRetVal;
}
/* [FP] misra_c_2012_rule_8_13_violation*/
static int Dpv14_Tx_Suspend(struct platform_device *pdev, pm_message_t state)
{
	int32_t iRetVal  = DPTX_RETURN_NO_ERROR;
	int32_t iPinRet;
	struct Dptx_Params *pstDptx;

	(void)state.event;

	/* [FP] misra_c_2012_rule_11_5_violation*/
	pstDptx = (struct Dptx_Params *)platform_get_drvdata(pdev);

	iPinRet = pinctrl_pm_select_sleep_state(&pdev->dev);
	if (iPinRet != 0) {
		/* For KCS */
		dptx_err("from pinctrl_pm_select_sleep_state()");
	}

	iRetVal = dptx_v14_deinit(pstDptx);

	return iRetVal;
}

static int Dpv14_Tx_Resume(struct platform_device *pdev)
{
	uint8_t ucRetry_MSTAct = 0;
	int32_t iPinRet, iRetVal = DPTX_RETURN_NO_ERROR;
	struct Dptx_Params *pstDptx;
	const struct Dptx_Video_Params *pstVideoParams;

	/* [FP] misra_c_2012_rule_11_5_violation*/
	pstDptx = (struct Dptx_Params *)platform_get_drvdata(pdev);

	pstVideoParams = &pstDptx->stVideoParams;

	iPinRet = pinctrl_pm_select_default_state(&pdev->dev);
	if (iPinRet != 0) {
		/* For KCS */
		dptx_err("from pinctrl_pm_select_default_state()");
	}

	dptx_dbg("Resume: Hot %s, Ports = %d, PClk %d %d %d %d\n",
				(pstDptx->eLast_HPDStatus == HPD_STATUS_PLUGGED) ? "Plugged":"Unplugged",
				pstDptx->ucNumOfPorts,
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_0],
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_1],
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_2],
				pstVideoParams->uiPeri_Pixel_Clock[PHY_INPUT_STREAM_3]);

	do {
		iRetVal = dptx_v14_init(pstDptx);
		if ((iRetVal != DPTX_RETURN_NO_ERROR) && (iRetVal != DPTX_RETURN_MST_ACT_TIMEOUT)) {
			/* [FP] misra_c_2012_rule_15_1_violation*/
			goto return_funcs;
		}

		if (iRetVal == DPTX_RETURN_MST_ACT_TIMEOUT) {
			if (ucRetry_MSTAct == 0U) {
				dptx_notice("\n==== Re-initialize DP Link ===\n");

				ucRetry_MSTAct = 1U;

				(void)dptx_v14_deinit(pstDptx);
			} else {
				dptx_err("MST ACT timeout...");
				ucRetry_MSTAct = 0U;
			}
		}
	} while (ucRetry_MSTAct == 1U);

return_funcs:
	return iRetVal;
}


static const struct dev_pm_ops dp_pinctrl_pm_ops = {
	/* [FP] misra_c_2012_rule_20_7_violation*/
	SET_LATE_SYSTEM_SLEEP_PM_OPS(Dpv14_Tx_Device_Driver_Suspend, Dpv14_Tx_Device_Driver_Resume)
};
#endif


static const struct of_device_id dpv14_tx[] = {
		{ .compatible =	"telechips,dpv14-tx" },
		{ }
};

static struct platform_driver __refdata stDpv14_Tx_pdrv = {
		.probe = Dpv14_Tx_Probe,
		.remove = Dpv14_Tx_Remove,
		.driver = {
				.name = "telechips,dpv14-tx",
				.owner = THIS_MODULE,

#if defined(CONFIG_OF)
				.of_match_table = dpv14_tx,
#endif
#if defined(CONFIG_PM)
				.pm = &dp_pinctrl_pm_ops,
#endif
		},

#if defined(CONFIG_PM)
		.suspend = Dpv14_Tx_Suspend,
		.resume = Dpv14_Tx_Resume,
#endif
};

static __init int Dpv14_Tx_init(void)
{
	return platform_driver_register(&stDpv14_Tx_pdrv);
}
/* [FP] cert_dcl37_c_violation*/
/* [FP] misra_c_2012_rule_21_2_violation*/
/* [FP] misra_c_2012_rule_20_7_violation*/
module_init(Dpv14_Tx_init);

static __exit void Dpv14_Tx_exit(void)
{
	return platform_driver_unregister(&stDpv14_Tx_pdrv);
}
/* [FP] cert_dcl37_c_violation*/
/* [FP] misra_c_2012_rule_21_2_violation*/
/* [FP] misra_c_2012_rule_20_7_violation*/
module_exit(Dpv14_Tx_exit);

/* [FP] cert_dcl37_c_violation*/
/* [FP] misra_c_2012_rule_21_2_violation*/ // K5.4
MODULE_AUTHOR("Telechips.");
/* [FP] cert_dcl37_c_violation*/
/* [FP] misra_c_2012_rule_21_2_violation*/ // K5.4
MODULE_DESCRIPTION("DP Tx Driver");
/* [FP] cert_dcl37_c_violation*/
/* [FP] misra_c_2012_rule_21_2_violation*/ // K5.4
MODULE_LICENSE("GPL");

/* [FP] cert_dcl37_c_violation*/
/* [FP] misra_c_2012_rule_5_9_violation*/ //K5.4
/* [FP] misra_c_2012_rule_21_2_violation*/ //K5.4
MODULE_VERSION(__stringify(TCC_DPTX_DRV_MAJOR_VER) "."
               __stringify(TCC_DPTX_DRV_MINOR_VER) "."
               __stringify(TCC_DPTX_DRV_SUBTITLE_VER));


