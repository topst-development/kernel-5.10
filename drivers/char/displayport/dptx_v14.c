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
#include <linux/of_graph.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include "soc/telechips/chipinfo.h"
#include <video/telechips/vioc_global.h>
#include <video/telechips/vioc_config.h>

#include <dptx_video.h>
#include <dptx_v14.h>
#include <dptx_reg.h>
#include <dptx_dbg.h>
#include <dptx_drm_dp_addition.h>

static struct Dptx_Params *dptx_dev_param;

static int32_t dptx_v14_register_callback(struct Dptx_Params *dev_param)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;

	#if IS_ENABLED(CONFIG_DRM_PANEL_MAX968XX) || IS_ENABLED(CONFIG_MAX968XX_DP_SERDES)
	dev_param->pvStr_Resume_CallBack = Str_Resume_CallBabck;
	#endif

	(void)Dptx_Intr_Register_HPD_Callback(dev_param, Hpd_Intr_CallBabck);
	(void)Dptx_Intr_Register_Panel_Callback(dev_param, Panel_Topology_CallBabck);

	return ret;
}

static int32_t dptx_init_resource(struct platform_device *pdev, struct Dptx_Params *dev_param)
{
	uint32_t elements;
	void __iomem *vaddr;
	const struct resource *pstResource;
	int32_t ret = DPTX_RETURN_NO_ERROR;

	for (elements = 0u; elements < 3u; elements++) {
		pstResource = platform_get_resource(pdev, IORESOURCE_MEM, elements);
		if (pstResource == NULL) {
			dptx_err("can't get %u device resource\n", elements);

			ret = -ENOMEM;
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			vaddr = devm_ioremap(&pdev->dev, pstResource->start,
					     pstResource->end - pstResource->start);
			if (vaddr == NULL) {
				dptx_err("Failed to remap %u device resource\n", elements);

				ret = -ENOMEM;
			}
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			if (elements == 0u) {
				/* For KCS */
				dev_param->pioDPLink_BaseAddr = vaddr;
			} else if (elements == 1u) {
				/* For KCS */
				dev_param->pioMIC_SubSystem_BaseAddr = vaddr;
			} else {
				/* For KCS */
				dev_param->pioPMU_BaseAddr = vaddr;
			}
		}
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			break;
		}
	}
	return ret;

}

static int32_t dptx_v14_init_os(struct platform_device *pdev, struct Dptx_Params *dev_param)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;

	mutex_init(&dev_param->Mutex);

	init_waitqueue_head(&dev_param->WaitQ);

	init_completion(&dev_param->hpd_plug_comp);

	atomic_set(&dev_param->Sink_request, 0);
	atomic_set(&dev_param->HPD_IRQ_State, 0);

	dev_param->iHPD_IRQ = platform_get_irq(pdev, 0);

	if (dev_param->iHPD_IRQ < 0) {
		dptx_err("No IRQ\n");

		ret = -ENODEV;
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		ret = devm_request_threaded_irq(dev_param->dev, (uint32_t)dev_param->iHPD_IRQ,
						Dptx_Intr_IRQ, Dptx_Intr_Threaded_IRQ,
						((unsigned long)IRQF_SHARED | (unsigned long)IRQ_LEVEL),
						"Dpv14_Tx", dev_param);
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			dptx_err("from devm_request_threaded_irq()");
		}
	}

	return ret;
}

static int32_t dptx_v14_init_params(struct Dptx_Params *dev_param, struct device *dev)
{
	uint8_t dp_idx;
	int32_t ret = DPTX_RETURN_NO_ERROR;

	dev_param->dev = dev;
	dev_param->uiHDCP22_RegAddr_Offset = DP_HDCP_OFFSET;
	dev_param->uiRegBank_RegAddr_Offset = DP_REGISTER_BANK_OFFSET;
	dev_param->uiCKC_RegAddr_Offset = DP_CKC_OFFSET;
	dev_param->uiProtect_RegAddr_Offset = DP_PROTECT_OFFSET;
	dev_param->uiSEC_PHY_Reg_Offset = DP_SEC_PHY_OFFSET;
	dev_param->eEstablished_Timing = DMT_NOT_SUPPORTED;

	#if defined(CONFIG_ARCH_TCC807X)
	dev_param->ePhy_Dev = PHY_DEVICE_SEC;
	#else
	dev_param->ePhy_Dev = PHY_DEVICE_SNPS;
	#endif

	dev_param->uiTCC80xx_Rev = get_chip_rev();

	dev_param->pucEdidBuf = (uint8_t *)devm_kzalloc(dev, (size_t)DPTX_EDID_BUFLEN, GFP_KERNEL);
	if (dev_param->pucEdidBuf == NULL) {
		/* For KCS */
		dptx_err("Failed to alloc base EDID Buf");
		ret = -DPTX_RETURN_ENOMEM;
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		for (dp_idx = 0u; dp_idx < (uint8_t)PHY_INPUT_STREAM_MAX; dp_idx++) {
			dev_param->paucEdidBuf_Entry[dp_idx] = (uint8_t *)devm_kzalloc(dev, (size_t)DPTX_EDID_BUFLEN, GFP_KERNEL);

			if (dev_param->paucEdidBuf_Entry[dp_idx] == NULL) {
				/* For KCS */
				dptx_err("DP %d : failed to alloc EDID Buf", dp_idx);
				ret = -DPTX_RETURN_ENOMEM;
				break;
			}
			/* For KCS */
			(void)memset(dev_param->paucEdidBuf_Entry[dp_idx], 0, (size_t)DPTX_EDID_BUFLEN);
		}
	}
	if (DPTX_RETURN_ERROR(ret)) {
		if (dev_param->pucEdidBuf != NULL) {
			devm_kfree(dev, dev_param->pucEdidBuf);
			dev_param->pucEdidBuf = NULL;

			for (dp_idx = 0u; dp_idx < (uint8_t)PHY_INPUT_STREAM_MAX; dp_idx++) {
				if (dev_param->paucEdidBuf_Entry[dp_idx] != NULL) {
					devm_kfree(dev, dev_param->paucEdidBuf_Entry[dp_idx]);
					dev_param->paucEdidBuf_Entry[dp_idx] = NULL;
				}
			}
		}
	}

	return ret;
}

static void dptx_v14_init_clk(struct Dptx_Params *dev_param)
{
	uint8_t ucPLL_LockStatus;

	Dptx_Clk_Reset_PLL(dev_param);
	Dptx_Clk_Set_PLL_Divisor(dev_param);
	Dptx_Clk_Get_PLLLock_Status(dev_param, &ucPLL_LockStatus);
	Dptx_Clk_Set_PLL_ClkSrc(dev_param, (u8)CLKCTRL_PLL_DIVIDER_OUTPUT);
}

static void dptx_v14_init_protect(struct Dptx_Params *dev_param)
{
	(void)Dptx_Protect_Set_PW(dev_param);
	(void)Dptx_Protect_Set_CfgLock(dev_param, (bool)DP_PORTECT_CFG_UNLOCKED);
	(void)Dptx_Protect_Set_CfgAccess(dev_param, (bool)DP_PORTECT_CFG_ACCESSABLE);
}

static int32_t dptx_v14_set_reg_ap_access(const struct Dptx_Params *dev_param)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint32_t uiReg_Addr;
	uint32_t uiRegMap_R_ApbSel, uiRegMap_W_ApbSel;

	uiReg_Addr = (dev_param->ePhy_Dev == PHY_DEVICE_SEC) ? (uint32_t)DPTX_APB_SEL_SEC_PHY : (uint32_t)DPTX_APB_SEL_SNOP_PHY;

	uiRegMap_R_ApbSel = Dptx_MCU_DP_Reg_Read(dev_param, (uint32_t)uiReg_Addr);
	uiRegMap_W_ApbSel = (uint32_t)(uiRegMap_R_ApbSel | BIT(DPTX_APB_SEL_MASK));
	Dptx_MCU_DP_Reg_Write(dev_param, (uint32_t)uiReg_Addr, uiRegMap_W_ApbSel);

	dptx_debug("Register access to AP...Reg[0x%x]:0x%08x -> 0x%08x", uiReg_Addr, uiRegMap_R_ApbSel, uiRegMap_W_ApbSel);

	return ret;
}

static int32_t dptx_v14_release_coldrst_mask(const struct Dptx_Params *dev_param)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint32_t uiRegMap_R_HsmRst_Msk, uiRegMap_W_HsmRst_Msk;

	uiRegMap_R_HsmRst_Msk = Dptx_PMU_Reg_Read(dev_param, (uint32_t)PMU_HSM_RSTN_MASK);

	if ((uiRegMap_R_HsmRst_Msk & PMU_COLD_RESET_MASK) != 0U) {
		uiRegMap_W_HsmRst_Msk = (uint32_t)(uiRegMap_R_HsmRst_Msk & ~PMU_COLD_RESET_MASK);
		Dptx_PMU_Reg_Write(dev_param, (uint32_t)PMU_HSM_RSTN_MASK, uiRegMap_W_HsmRst_Msk);

		dptx_debug("DP Cold reset mask release...0x%08x -> 0x%08x", uiRegMap_R_HsmRst_Msk, uiRegMap_W_HsmRst_Msk);
	}

	return ret;
}

static int32_t dptx_reset_serdes(const struct Dptx_Params *dev_param)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;

	if (dev_param->pvStr_Resume_CallBack != NULL) {
		/* For KCS */
		ret = dev_param->pvStr_Resume_CallBack();
	}

	return ret;
}

static void dptx_v14_init_pre_condition(const struct Dptx_Params *dev_param)
{
	(void)dptx_v14_release_coldrst_mask(dev_param);
	(void)dptx_v14_set_reg_ap_access(dev_param);
}

static int32_t dptx_v14_deinit(struct Dptx_Params *dev_param)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;

	if (!dev_param->dp_slave_mode) {
		(void)Dptx_Core_Deinit(dev_param);
		(void)Dptx_Intr_Handle_HotUnplug(dev_param);
	}

	return ret;
}

/**
 * @brief Initialize SERDES and IP, and DP link training.
 *
 * This function is responsible for resetting the SERDES, configuring the IP,
 * and performing DisplayPort (DP) link training. It includes error handling
 * for SERDES reset, IP configuration, and Hot-Plug-Detect (HPD) status.
 * The function is designed to be called in the retry loop of the DP
 * initialization process.
 *
 * @param[in] dev_param Pointer to the device parameters structure.
 *
 * @return int32_t Returns DPTX_RETURN_NO_ERROR on success, or an error code on
 *                 failure.
 *
 */
static int32_t dptx_v14_init_and_link_training(struct Dptx_Params *dev_param)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;

	/* Attempt to reset SERDES and IP */
	ret = dptx_reset_serdes(dev_param);
	if (DPTX_RETURN_ERROR(ret)) {
		dptx_err("DP SER/DES reset failed. Retrying...");
		(void)dptx_reset_serdes(dev_param);
	}
	/**
	 * However, since the kernel cannot determine whether the DisplayPort
	 * is operating in panel mode, to ensure proper functionality when not
	 * in panel mode, the process has been modified to continue link training
	 * even if serializer configuration fails.
	 */
	ret = dptx_cfg_reset_ip(dev_param, dev_param->ucMax_Rate,
				dev_param->ucMax_Lanes);
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (!dptx_intr_check_hpd_and_wait_hpd_to_plugged(dev_param)) {
			dev_param->last_known_hpd_status = HPD_STATUS_UNPLUGGED;
			ret = -DPTX_RETURN_ENODEV;
			dptx_err("Hot unplug detected. Aborting initialize.");
		} else {
			dev_param->last_known_hpd_status = HPD_STATUS_PLUGGED;
			ret = dptx_probe_linktraining(dev_param);
		}
	}

	return ret;
}

/**
 * @brief Initializes the DisplayPort Source
 *
 * This function initializes the DisplayPort (DP) transmitter by setting up the
 * SERDES, IP configurations, and performing link training. It handles errors
 * such as MST_ACT timeouts and retries the initialization if necessary.
 * This function ensures the DP transmitter is fully operational by enabling
 * global interrupts after successful link training.
 *
 * @param[in] dev_param Pointer to the device parameters structure.
 *
 * @return int32_t Returns DPTX_RETURN_NO_ERROR on success, or an error code on
 *                 failure.
 *
 * @note
 * - This function internally calls `dptx_v14_init_and_link_training` for link
 *   training.
 */
static int32_t dptx_v14_init(struct Dptx_Params *dev_param)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;

	bool need_retry_mst_act = (bool)false;

	uint32_t mst_act_retry_count = 0u;

	/* Initialize max lanes and max rate */
	dev_param->ucMax_Rate = (uint8_t)DPTX_PHYIF_CTRL_RATE_HBR3;
	dev_param->ucMax_Lanes = (uint8_t)PHY_LANE_MAX;

	if (!dev_param->dp_slave_mode) {
		dptx_v14_init_pre_condition(dev_param);
		dptx_v14_init_protect(dev_param);
		dptx_v14_init_clk(dev_param);

		for (mst_act_retry_count = 0u; mst_act_retry_count < 2u; mst_act_retry_count++) {
			ret = dptx_v14_init_and_link_training(dev_param);
			if (ret == -DPTX_RETURN_MST_ACT_TIMEOUT) {
				/* This condition checks if the MST_ACT
				 * failure is occurring for the first time
				 * after SERDES and IP reset, allowing for
				 * an additional retry if needed.
				 */
				if (mst_act_retry_count == 0u) {
					dptx_err("MST ACT timeout. Retrying DP Link initialization...");
					(void)dptx_v14_deinit(dev_param);
					need_retry_mst_act = (bool)true;
				} else {
					dptx_err("MST ACT timeout. All retries failed.");
				}
			}
			if (!need_retry_mst_act) {
				/* For KCS */
				break;
			}
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			dptx_core_enable_global_intr(dev_param, DPTX_IEN_ALL_INTR);
			dptx_info("Link training successful.");
		}
	}

	return ret;
}

static int32_t dptx_v14_probe_drv(struct platform_device *pdev, struct Dptx_Params *dev_param)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;

	if (!dev_param->dp_slave_mode) {
		(void)Dptx_Ext_Proc_Interface_Init(dev_param);
		(void)dptx_v14_register_callback(dev_param);

		dptx_core_disable_global_intr(dev_param, DPTX_IEN_ALL_INTR);

		(void)dptx_v14_init_os(pdev, dev_param);
	}

	return ret;
}

#if defined(DPTX_DUMP_PHY_EQ)
static void dptx_v14_dump_hw_config(struct dptx_hw_config *hw_config)
{
	const char * const link_rate_names[LINK_RATE_MAX] = {
		"rbr", "hbr", "hbr2", "hbr3"
	};
	uint32_t vsw_idx, link_rate;

	for (link_rate = (uint32_t)LINK_RATE_RBR; link_rate < (uint32_t)LINK_RATE_MAX; link_rate++) {
		pr_info("Main Equalization Value with link rate <%s>\n", link_rate_names[link_rate]);
		for (vsw_idx = 0u; vsw_idx < 16u; vsw_idx += 4u) {
			pr_info("0x%02x 0x%02x 0x%02x 0x%02x\n",
				hw_config->phy_eq[link_rate].main_eq[vsw_idx],
				hw_config->phy_eq[link_rate].main_eq[vsw_idx+1u],
				hw_config->phy_eq[link_rate].main_eq[vsw_idx+2u],
				hw_config->phy_eq[link_rate].main_eq[vsw_idx+3u]);
		}
		pr_info("Post Equalization Value with link rate <%s>\n", link_rate_names[link_rate]);
		for (vsw_idx = 0u; vsw_idx < 16u; vsw_idx += 4u) {
			pr_info("0x%02x 0x%02x 0x%02x 0x%02x\n",
				hw_config->phy_eq[link_rate].post_eq[vsw_idx],
				hw_config->phy_eq[link_rate].post_eq[vsw_idx+1u],
				hw_config->phy_eq[link_rate].post_eq[vsw_idx+2u],
				hw_config->phy_eq[link_rate].post_eq[vsw_idx+3u]);
		}
	}
}
#endif

static void tcc_dpv14_parse_hw_config(struct Dptx_Params *dev_param)
{
	struct device_node *hw_config_node = NULL;
	struct device_node *eq_sub_node = NULL;
	struct device_node *eq_node = NULL;

	int32_t ret = DPTX_RETURN_NO_ERROR;
	int32_t rd_counts;

	const char * const link_rate_names[LINK_RATE_MAX] = {
		"rbr", "hbr", "hbr2", "hbr3"
	};
	uint32_t link_rate;

	hw_config_node = of_get_child_by_name(dev_param->dev->of_node, "hw-config");
	if (hw_config_node != NULL) {
		eq_node = of_get_child_by_name(hw_config_node, "eq");
		if (eq_node == NULL) {
			dptx_warn("'eq' sub-node was not found");
			ret = -DPTX_RETURN_ENODEV;
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			if (!of_device_is_available(eq_node)) {
				dptx_warn("'eq' sub-node was not activated");
				ret = -DPTX_RETURN_ENODEV;
			}
		}
		for (link_rate = (uint32_t)LINK_RATE_RBR; link_rate < (uint32_t)LINK_RATE_MAX; link_rate++) {
			eq_sub_node = NULL;
			if (DPTX_RETURN_SUCCESS(ret)) {
				eq_sub_node = of_get_child_by_name(eq_node, link_rate_names[link_rate]);
				if (eq_sub_node == NULL) {
					dptx_warn("'%s' sub-node was not found", link_rate_names[link_rate]);
					ret = -DPTX_RETURN_ENODEV;
				}
			}
			if (DPTX_RETURN_SUCCESS(ret)) {
				rd_counts = of_property_read_variable_u32_array(eq_sub_node, "main",
										(uint32_t *)dev_param->hw_config.phy_eq[link_rate].main_eq, 16u, 0u);
				if (rd_counts != 16) {
					dptx_warn("failed to read main equali");
					ret = -DPTX_RETURN_ENODEV;
				}
			}
			if (DPTX_RETURN_SUCCESS(ret)) {
				rd_counts = of_property_read_variable_u32_array(eq_sub_node, "post",
											(uint32_t *)dev_param->hw_config.phy_eq[link_rate].post_eq, 16u, 0u);
				if (rd_counts != 16) {
					dptx_warn("failed to read post equali");
					ret = -DPTX_RETURN_ENODEV;
				}
			}
			if (eq_sub_node != NULL) {
				/* Release reference to eq_sub_node node*/
				of_node_put(eq_sub_node);
			}
			if (DPTX_RETURN_ERROR(ret)) {
				/* For KCS */
				break;
			}
		}
		if (eq_node != NULL) {
			/* Release reference to eq_node node*/
			of_node_put(eq_node);
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			/* For KCS */
			dev_param->hw_config.phy_eq_manual_mode = true;
		}
		if (!dev_param->hw_config.phy_eq_manual_mode) {
			dptx_info("phy equalization manulal mode is disabled");
			dptx_core_set_default_phy_eq(dev_param);
			if (dev_param->ePhy_Dev == PHY_DEVICE_SEC) {
				/* For KCS */
				dev_param->hw_config.phy_eq_manual_mode = true;
			}
		}
		#if defined(DPTX_DUMP_PHY_EQ)
		dptx_v14_dump_hw_config(&dev_param->hw_config);
		#endif
		of_node_put(hw_config_node);
	}
}

static struct clk *get_disp_clks(const struct Dptx_Params *dev_param, uint32_t ddc_id)
{
	const char * const disp_clk_names[VIOC_DISP_MAX] = {
		"disp0-clk",
		"disp1-clk",
		"disp2-clk",
		#if defined(VIOC_DISP3)
		"disp3-clk",
		#endif
		#if defined(VIOC_DISP4)
		"disp3-clk",
		#endif
	};
	struct clk *disp_clk = NULL;

	if (ddc_id < VIOC_DISP_MAX) {
		disp_clk = of_clk_get_by_name(dev_param->dev->of_node, disp_clk_names[ddc_id]);
		if (IS_ERR(disp_clk)) {
			dptx_err("Can't find pixel clock of display controller %u", ddc_id);
			disp_clk = NULL;
		}
	}
	return disp_clk;
}

static int32_t parse_dp_params(struct Dptx_Params *dev_param, const struct device_node *np)
{
	uint32_t vcp_id[PHY_INPUT_STREAM_MAX];
	uint32_t dt_val;
	uint8_t dp_idx;


	int32_t ret = DPTX_RETURN_NO_ERROR;

	if (np == NULL) {
		dptx_err("np is NULL");
		ret = -DPTX_RETURN_ENODEV;
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		/* For KCS */
		ret = of_property_read_u32_array(np, "sink_vcp_id", vcp_id,
						 (size_t)PHY_INPUT_STREAM_MAX);
		if (DPTX_RETURN_SUCCESS(ret)) {
			for (dp_idx  = 0u; dp_idx < (uint8_t)PHY_INPUT_STREAM_MAX; dp_idx++) {
				if ((vcp_id[dp_idx] >= 1u) && (vcp_id[dp_idx] <= 4u)) {
					dev_param->aucVCP_Id[dp_idx] = (uint8_t)vcp_id[dp_idx];
				} else {
					dptx_err("vcp id were out of range");
					ret = -DPTX_RETURN_EINVAL;
					break;
				}
			}
		} else {
			dptx_err("Can't get Sink VCP Id.. set to default");
			for (dp_idx  = 0u; dp_idx < (uint8_t)PHY_INPUT_STREAM_MAX; dp_idx++) {
				/* For KCS */
				dev_param->aucVCP_Id[dp_idx] = (dp_idx + 1u);
			}
			ret = DPTX_RETURN_NO_ERROR;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (of_property_read_u32(np, "dp-slave-mode", &dt_val) < 0) {
			/* For KCS */
			dt_val = 0u;
		}
		if (dt_val != 0u) {
			/* For KCS */
			dev_param->dp_slave_mode = (bool)true;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		/* Parse dp-max-lane */
		ret = of_property_read_u32(np, "dp-max-lane", &dt_val);
		if (DPTX_RETURN_ERROR(ret)) {
			dptx_err("The dp-max-lane property was not found");
		} else {
			if ((dt_val != 1u) &&
				(dt_val != 2u) &&
				(dt_val != 4u)) {
				dptx_err("The dp-max-lane property was out of range (%u)",
						dt_val);
				ret = -DPTX_RETURN_EINVAL;
			}
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dev_param->ucMax_Lanes = (uint8_t)dt_val;
		dev_param->stDptxLink.ucNumOfLanes = dev_param->ucMax_Lanes;

		/* Parse dp-max-rate */
		ret = of_property_read_u32(np, "dp-max-rate", &dt_val);
		if (DPTX_RETURN_ERROR(ret)) {
			dptx_err("The dp-max-rate property was not found");
		} else {
			if (dt_val >= (uint32_t)LINK_RATE_MAX) {
				dptx_err("The dp-max-rate property was out of range (%u)",
					dt_val);
				ret = -DPTX_RETURN_EINVAL;
			}
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dev_param->ucMax_Rate = (uint8_t)dt_val;
		dev_param->stDptxLink.ucLinkRate  = (uint8_t)dev_param->ucMax_Rate;

		/* Parse dp-ssc */
		ret = of_property_read_u32(np, "dp-spread-spectrum", &dt_val);
		if (DPTX_RETURN_ERROR(ret)) {
			/* Failed to find spread specturm property */
			dptx_err("The dp-spread-spectrum property was not found");
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (dt_val != 0u) {
			/* Spread specturm clock will be activated */
			dev_param->bSpreadSpectrum_Clock = (bool)true;
		}
		if (IS_ENABLED(CONFIG_ARCH_TCC805X)) {
			/* Parse dp-phy-lane-swap */
			ret = of_property_read_u32(np, "dp-phy-lane-swap", &dt_val);
			if (DPTX_RETURN_ERROR(ret)) {
				dptx_err("The dp-phy-lane-swap property was not found");
			}
		} else {
			/**
			 * The dp-phy-lane-swap property is supported only on
			 * TCC805x. For other chipsets, the default configuration
			 * should be applied.
			 */
			dt_val = 1u;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (dt_val != 0u) {
			/* Not ALT Mode */
			dev_param->bPhy_Lane_Std = (bool)true;
		}
	}

	return ret;
}

static int32_t parse_video_param(struct Dptx_Params *dev_param, const struct device_node *np)
{
	uint32_t video_codes[PHY_INPUT_STREAM_MAX * 2];
	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint32_t video_stream_count;
	int32_t video_elems_of_size;
	uint32_t dp_idx;


	if (np == NULL) {
		ret = -DPTX_RETURN_ENODEV;
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		/* Parse dp-video-codes */
		video_elems_of_size = of_property_count_elems_of_size(np, "dp-video-codes", 4);
		if (video_elems_of_size < 0) {
			dptx_err("The dp-video-codes property can not counted in device-tree");
			ret = -DPTX_RETURN_ENODEV;
		} else {
			video_stream_count = (uint32_t)video_elems_of_size;

			if ((video_stream_count < 2u) || ((video_stream_count & 1u) != 0u) ||
			    (video_stream_count > ((uint32_t)PHY_INPUT_STREAM_MAX * 2u))) {
				dptx_err("The dp-video-codes count was out of range (%u)",
					 video_stream_count);
				ret = -DPTX_RETURN_EINVAL;
			}
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dev_param->hw_config.num_of_ports = (uint32_t)video_stream_count >> 1;
		ret = of_property_read_u32_array(np, "dp-video-codes",
						 video_codes,
						 video_stream_count);
		if (DPTX_RETURN_ERROR(ret)) {
			/* Failed to find video identification codes property */
			dptx_err("The dp-video-codes property was not found in device-tree");
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dptx_info("DTB %u streams\n", dev_param->hw_config.num_of_ports);
		for (dp_idx = 0u; dp_idx < dev_param->hw_config.num_of_ports; dp_idx++) {
			dev_param->video_params[dp_idx].bit_per_component = (uint8_t)PIXEL_BPC_8_BITS;
			dev_param->video_params[dp_idx].video_refresh_rate = (u32)VIDEO_REFRESH_RATE_60_00HZ;
			dev_param->video_params[dp_idx].pixel_encoding = PIXEL_ENCODING_TYPE_RGB;
			dev_param->video_params[dp_idx].colorimetry_format = COLORIMETRY_RGB;
			if (video_codes[dp_idx << 1] >= (uint32_t)VIDEO_FORMAT_MAX) {
				dptx_err("DP %u: The video format standard is out of range, set to default CEA-861.", dp_idx);
				video_codes[dp_idx << 1] = (uint32_t)VIDEO_FORMAT_CEA_861;
			}
			dev_param->video_params[dp_idx].video_format_standard =
				(enum VIDEO_FORMAT_STANDARD_TYPE)video_codes[dp_idx << 1];
			dev_param->video_params[dp_idx].video_code = video_codes[(dp_idx << 1) + 1u];
		}
	}
	return ret;
}

static int parse_drm_mux(struct Dptx_Params *dev_param, const struct device_node *np_vioc, uint32_t dp_idx)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;

	uint32_t lcdc_mux_select, lcdc_mux_bypass;

	ret = of_property_read_u32(np_vioc, "lcdc-mux-select", &lcdc_mux_select);
	if (DPTX_RETURN_ERROR(ret)) {
		/* Failed to find lcdc mux selection property */
		dptx_err("DP %u: can not found lcdc-mux-select\r\n", dp_idx);
	} else {
		dptx_info("[DEBUG] lcdc-mux-select=%u\r\n", lcdc_mux_select);

		ret = of_property_read_u32(np_vioc, "lcdc-mux-bypass", &lcdc_mux_bypass);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (lcdc_mux_select > 4u) {
			/* Failed to find lcdc mux selection property */
			dptx_err("DP %u: lcdc-mux-select %u is out of range\n", dp_idx, lcdc_mux_select);
			ret = -DPTX_RETURN_EINVAL;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dev_param->aucMux_Idx[dp_idx] = (uint8_t)lcdc_mux_select;
		if (lcdc_mux_bypass != 0u) {
			if (lcdc_mux_select == 2u) {
				dev_param->bSDM_Bypass = (bool)true;
				dptx_info("[INFO] sdm-bypass");
			}
			if (lcdc_mux_select == 3u) {
				dev_param->bTRVC_Bypass = (bool)true;
				dptx_info("[INFO] trvc-bypass");
			}
		}
	}
	return ret;
}

static int32_t parse_drm_params(struct Dptx_Params *dev_param, uint32_t ddc_ids[PHY_INPUT_STREAM_MAX])
{
	struct platform_device *drm_dp_device, *drm_vioc_device;
	struct device_node *np_vioc = NULL;
	struct device_node *np = NULL;

	uint32_t dp_idx, dt_val, num_of_ports = 0u;

	int32_t ret = DPTX_RETURN_NO_ERROR;

	for (dp_idx = 0u; dp_idx < (uint32_t)PHY_INPUT_STREAM_MAX; dp_idx++) {
		drm_dp_device = NULL;
		drm_vioc_device = NULL;
		np = of_find_compatible_node(np, NULL, "telechips,drm-dp");
		if (np == NULL) {
			/* Failed to find DRM DP node */
			ret = -DPTX_RETURN_ENODEV;
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			drm_dp_device = of_find_device_by_node(np);
			if (drm_dp_device == NULL) {
				/* Failed to find DRM DP device */
				ret = -DPTX_RETURN_ENODEV;
			}
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			if (!of_graph_is_present(np)) {
				ret = -DPTX_RETURN_ENODEV;
			}
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			np_vioc = of_graph_get_remote_node(np, 0, 0);
			if (np_vioc == NULL) {
				dptx_err("no vioc node found for %s\n", dev_name(&drm_dp_device->dev));
				ret = -ENOENT;
			}
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			drm_vioc_device = of_find_device_by_node(np_vioc);
			if (drm_vioc_device == NULL) {
				/* Failed to find DRM DP device */
				ret = -DPTX_RETURN_ENODEV;
			}
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			ret = parse_drm_mux(dev_param, np_vioc, dp_idx);
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			ret = of_property_read_u32_index(np_vioc, "display_device", 1, &dt_val);
			/* Release reference to np_vioc node*/
			of_node_put(np_vioc);
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			/* Get display controller id */
			ddc_ids[dp_idx] = get_vioc_index(dt_val);
		}
		if (np != NULL) {
			/* Release reference to np node*/
			of_node_put(np);
		}
		if (drm_dp_device != NULL) {
			/* Release reference to drm_dp_device */
			put_device(&drm_dp_device->dev);
		}
		if (drm_vioc_device != NULL) {
			/* Release reference to drm_vioc_device */
			put_device(&drm_vioc_device->dev);
		}
		if (DPTX_RETURN_ERROR(ret)) {
			break;
		}
		num_of_ports++;
	}
	dev_param->hw_config.num_of_ports = min_t(uint32_t,
						  dev_param->hw_config.num_of_ports,
						  num_of_ports);
	dev_param->ucNumOfPorts = (uint8_t)dev_param->hw_config.num_of_ports;
	dptx_info("DRM %u streams\n", dev_param->hw_config.num_of_ports);
	return 0;
}

static int32_t readback_device_mux_pcks(struct Dptx_Params *dev_param,
					uint32_t pclks[PHY_INPUT_STREAM_MAX])
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint8_t dp_idx, mux_id;

	unsigned long disp_pclk;
	uint32_t ddc_id;

	struct clk *disp_clk;

	for (dp_idx  = 0u; dp_idx < (uint8_t)PHY_INPUT_STREAM_MAX; dp_idx++) {
		ret = Dptx_Cfg_Get_MuxSelect(dev_param, dp_idx, &mux_id);
		if (DPTX_RETURN_ERROR(ret)) {
			dptx_err("Failed get display mux id for dp %u", dp_idx);
		} else {
			ret = VIOC_CONFIG_get_ddc_id_from_mux(mux_id, &ddc_id);
			if (DPTX_RETURN_ERROR(ret)) {
				dptx_err("Failed get display device controller id for mux %u", mux_id);
			}
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			disp_clk = get_disp_clks(dev_param, ddc_id);
			if (disp_clk == NULL) {
				dptx_err("Failed get clk for display device controller %u", ddc_id);
				ret = -DPTX_RETURN_ENODEV;
			}
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			disp_pclk = clk_get_rate(disp_clk);
			dptx_info("DISP[%u] - MUX[%u] - DP[%u] - pclk (%lu)",
				ddc_id, mux_id, dp_idx, disp_pclk);
			if (disp_pclk > 1000u) {
				/* For KCS */
				disp_pclk /= 1000u;
			} else {
				disp_pclk = 0u;
			}
			if (disp_pclk > (unsigned long)U32_MAX) {
				dptx_err("Pixel clock of display controller %u is out of range", ddc_id);
				ret = -DPTX_RETURN_EINVAL;
			}
		}
		if (DPTX_RETURN_ERROR(ret)) {
			break;
		}
		/* For KCS */
		pclks[dp_idx] = (uint32_t)disp_pclk;
	}

	return 0;
}

static int32_t parse_video_pclks(struct Dptx_Params *dev_param,
				 const uint32_t ddc_ids[PHY_INPUT_STREAM_MAX])
{
	struct dptx_dtd_params dp_dtd_param;
	int32_t ret = DPTX_RETURN_NO_ERROR;
	unsigned long disp_pclk;
	struct clk *disp_clk;

	uint32_t dp_idx;

	for (dp_idx  = 0u; dp_idx < dev_param->hw_config.num_of_ports; dp_idx++) {
		disp_clk = get_disp_clks(dev_param, ddc_ids[dp_idx]);
		if (disp_clk == NULL) {
			dptx_err("Failed get clk for display device controller %u", ddc_ids[dp_idx]);
			ret = -DPTX_RETURN_ENODEV;
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			ret = Dptx_VidIn_Fill_Dtd(&dp_dtd_param,
						dev_param->video_params[dp_idx].video_code,
						60000u,
						dev_param->video_params[dp_idx].video_format_standard);
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			ret = clk_set_rate(disp_clk, (ulong)dp_dtd_param.uiPixel_Clock * 1000u);
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			disp_pclk = clk_get_rate(disp_clk);

			dptx_info("DISP[%u] - MUX[%u] - DP[%u] - pclk (%u --> %lu)",
				  ddc_ids[dp_idx], dev_param->aucMux_Idx[dp_idx],
				  dp_idx, dp_dtd_param.uiPixel_Clock, disp_pclk);
			if (disp_pclk > 1000u) {
				disp_pclk /= 1000u;
			} else {
				disp_pclk = 0u;
			}
			if (disp_pclk > (unsigned long)U32_MAX) {
				dptx_err("Pixel clock of display controller %u is out of range", ddc_ids[dp_idx]);
				ret = -DPTX_RETURN_EINVAL;
			}
		}
		if (DPTX_RETURN_ERROR(ret)) {
			break;
		}
		dev_param->video_params[dp_idx].pixel_clock = (uint32_t)disp_pclk;
	}
	return ret;
}

static int32_t of_parse_dp_dt(struct Dptx_Params *dev_param,
			      uint32_t pclks[PHY_INPUT_STREAM_MAX],
			      bool dp_initialized_by_bootloader)
{
	uint32_t ddc_ids[PHY_INPUT_STREAM_MAX];
	int32_t ret = DPTX_RETURN_NO_ERROR;
	struct device_node *np;

	np = dev_param->dev->of_node;

	(void)memset(pclks, 0, sizeof(*pclks));
	ret = parse_dp_params(dev_param, np);
	if (DPTX_RETURN_SUCCESS(ret)) {
		ret = parse_video_param(dev_param, np);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		ret = parse_drm_params(dev_param, ddc_ids);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		tcc_dpv14_parse_hw_config(dev_param);

		if (dp_initialized_by_bootloader) {
			ret = readback_device_mux_pcks(dev_param, pclks);
		} else {
			ret = parse_video_pclks(dev_param, ddc_ids);
			if (DPTX_RETURN_SUCCESS(ret)) {
				if (dev_param->hw_config.num_of_ports > 1u) {
					dev_param->bMultStreamTransport = (bool)true;
				}
			}
		}
	}
	return ret;
}

struct Dptx_Params *Dpv14_Tx_Get_Device_Handle(void)
{
	return dptx_dev_param;
}

static int tcc_device_match_of_node(struct device *dev, const void *np)
{
	return (dev->of_node == np) ? 1 : 0;
}

static struct device *
tcc_bus_find_device_by_of_node(struct bus_type *bus, const struct device_node *np)
{
	return bus_find_device(bus, NULL, np, tcc_device_match_of_node);
}

static void dptv_v14_add_i2c_link(struct device *pdev, struct Dptx_Params *pDptxParams)
{
	struct device *i2c_dev = NULL;
	const struct device_node *dev_node = NULL;

	//Set device link for sorting a suspend/resume sequence
	dev_node = of_find_node_by_name(NULL, "i2c");
	if (dev_node != NULL) {
		i2c_dev = tcc_bus_find_device_by_of_node(&platform_bus_type, dev_node);
		if (i2c_dev != NULL) {
			pDptxParams->pstI2cLink =
				device_link_add(pdev, i2c_dev, DL_FLAG_STATELESS);
		}
	}

	if (pDptxParams->pstI2cLink != NULL) {
		/* For KCS */
		dev_info(pdev, "[INFO] Success to link i2c device!\n");
	} else {
		dev_err(pdev, " [ERR] Failed to link i2c device\n");
	}
}

static void dptv_v14_remove_i2c_link(const struct Dptx_Params *pDptxParams)
{
	device_link_del(pDptxParams->pstI2cLink);
}

static int32_t Dpv14_Tx_Probe(struct platform_device *pdev)
{
	uint32_t pclks[PHY_INPUT_STREAM_MAX] = {0, };
	int32_t ret = DPTX_RETURN_NO_ERROR;

	bool dp_initialized_by_bootloader = (bool)true;
	struct Dptx_Params *dev_param = NULL;

	if (pdev->dev.of_node == NULL) {
		dptx_err("Node wasn't found");

		ret = -ENODEV;
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dev_param = (struct Dptx_Params *)devm_kzalloc(&pdev->dev, sizeof(*dev_param), GFP_KERNEL);
		if (dev_param == NULL) {
			/* For KCS */
			ret = -ENOMEM;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		(void)memset(dev_param, 0, sizeof(struct Dptx_Params));

		ret = dptx_v14_init_params(dev_param, &pdev->dev);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		/* For KCS */
		ret = dptx_init_resource(pdev, dev_param);
	}
	#ifdef CONFIG_OF
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (dptx_core_get_phy_xmit(dev_param) == 0u) {
			dp_initialized_by_bootloader = (bool)false;
		}
		ret = of_parse_dp_dt(dev_param, pclks, dp_initialized_by_bootloader);
		if (DPTX_RETURN_SUCCESS(ret)) {
			if (dp_initialized_by_bootloader) {
				(void)Dptx_Core_Init_Params(dev_param);
				(void)Dptx_Cfg_Init_Params(dev_param);
				(void)Dptx_VidIn_Init_Params(dev_param, pclks);
			}
			(void)Dptx_Intr_Init_Params(dev_param);
			(void)Dptx_Api_Init_Params();
		}
	}
	#endif
	if (DPTX_RETURN_SUCCESS(ret)) {
		platform_set_drvdata(pdev, dev_param);

		ret = dptx_v14_probe_drv(pdev, dev_param);

		if (DPTX_RETURN_SUCCESS(ret)) {
			if (dev_param->dp_slave_mode) {
				dptx_dbg("TCC-DPTX-Slave-Ver:%d.%d.%d",
					 TCC_DPTX_DRV_MAJOR_VER, TCC_DPTX_DRV_MINOR_VER, TCC_DPTX_DRV_SUBTITLE_VER);
			} else {
				dptx_dbg("TCC-DPTX-Ver: %d.%d.%d",
					 TCC_DPTX_DRV_MAJOR_VER, TCC_DPTX_DRV_MINOR_VER, TCC_DPTX_DRV_SUBTITLE_VER);
			}
	#if defined(CONFIG_ARCH_TCC807X)
			dptx_dbg("  TCC807X %s", (dev_param->uiTCC80xx_Rev == (uint32_t)TCC80XX_REVISION_ES) ? "ES" :
										(dev_param->uiTCC80xx_Rev == (uint32_t)TCC80XX_REVISION_CS) ? "CS" : "BX");
	#elif defined(CONFIG_ARCH_TCC805X)
			dptx_dbg("  TCC805X %s", (dev_param->uiTCC80xx_Rev == (uint32_t)TCC80XX_REVISION_ES) ? "ES" :
										(dev_param->uiTCC80xx_Rev == (uint32_t)TCC80XX_REVISION_CS) ? "CS" : "BX");
	#endif
			dptx_dbg("  Hot %s ", (dev_param->last_known_hpd_status == HPD_STATUS_PLUGGED) ? "Plugged" : "Unplugged");
			dptx_dbg("  Sink device: %s ", (dev_param->bSideBand_MSG_Supported) ? "exernal monitor" : "lcd panel");
			dptx_dbg("  Num of DPs: %u - %s mode ",
									(dev_param->ucNumOfPorts),
									(dev_param->bMultStreamTransport) ? "MST" : "SST");
			dptx_dbg("  Max rate: %s",
							(dev_param->ucMax_Rate == (uint8_t)DPTX_PHYIF_CTRL_RATE_RBR) ? "RBR" :
							(dev_param->ucMax_Rate == (uint8_t)DPTX_PHYIF_CTRL_RATE_HBR) ? "HBR" :
							(dev_param->ucMax_Rate == (uint8_t)DPTX_PHYIF_CTRL_RATE_HBR2) ? "HB2" : "HBR3");
			dptx_dbg("  Max lane: %s",
							(dev_param->ucMax_Lanes == (uint8_t)1U) ? "1 lane" :
							(dev_param->ucMax_Lanes == (uint8_t)2U) ? "2 lanes" : "4 lanes");
			dptx_dbg("  Mux: %u %u %u %u", dev_param->aucMux_Idx[0], dev_param->aucMux_Idx[1], dev_param->aucMux_Idx[2], dev_param->aucMux_Idx[3]);
			dptx_dbg("  PHY Lanes cfg: %s", (dev_param->bPhy_Lane_Std) ? "standard" : "swap");
			dptx_dbg("  SDM Path: %s", (dev_param->bSDM_Bypass) ? "bypass" : "use video data");
			dptx_dbg("  TRVC Path: %s", (dev_param->bTRVC_Bypass) ? "bypass" : "use video data");
			dptx_dbg("  VCP Id: %u %u %u %u", dev_param->aucVCP_Id[0], dev_param->aucVCP_Id[1], dev_param->aucVCP_Id[2], dev_param->aucVCP_Id[3]);
			dptx_dbg("  VIC: %u %u %u %u", dev_param->video_params[0].video_code,
						       dev_param->video_params[1].video_code,
						       dev_param->video_params[2].video_code,
						       dev_param->video_params[3].video_code);
			dptx_dbg("  PClk: %u %u %u %u", dev_param->video_params[0].pixel_clock,
							dev_param->video_params[1].pixel_clock,
							dev_param->video_params[2].pixel_clock,
							dev_param->video_params[3].pixel_clock);
			dptx_dbg("  %d streams enabled", dev_param->ucNumOfPorts);
			dptx_dbg("   stream[0] pixel_encoding = %s",
				(dev_param->video_params[0].pixel_encoding == PIXEL_ENCODING_TYPE_RGB) ? "RGB" :
				 (dev_param->video_params[0].pixel_encoding == PIXEL_ENCODING_TYPE_YCBCR422) ? "YCbCr422" : "YCbCr444");
			dptx_dbg("   stream[1] pixel_encoding = %s",
				(dev_param->video_params[1].pixel_encoding == PIXEL_ENCODING_TYPE_RGB) ? "RGB" :
				 (dev_param->video_params[1].pixel_encoding == PIXEL_ENCODING_TYPE_YCBCR422) ? "YCbCr422" : "YCbCr444");
			dptx_dbg("   stream[2] pixel_encoding = %s",
				(dev_param->video_params[2].pixel_encoding == PIXEL_ENCODING_TYPE_RGB) ? "RGB" :
				 (dev_param->video_params[2].pixel_encoding == PIXEL_ENCODING_TYPE_YCBCR422) ? "YCbCr422" : "YCbCr444");
			dptx_dbg("   stream[3] pixel_encoding = %s",
				(dev_param->video_params[3].pixel_encoding == PIXEL_ENCODING_TYPE_RGB) ? "RGB" :
				 (dev_param->video_params[3].pixel_encoding == PIXEL_ENCODING_TYPE_YCBCR422) ? "YCbCr422" : "YCbCr444");
		}
		if (!dev_param->dp_slave_mode) {
			dptv_v14_add_i2c_link(&pdev->dev, dev_param);
			/* Supports slave mode */
			#if defined(CONFIG_TELECHIPS_DP_VZ)
			Dptx_Reg_Writel(dev_param, DPTX_CUSTOMPAT0, MAIN_DP_COMPLETED_INIT);
			#endif
		}
		/* Update global device param */
		dptx_dev_param = dev_param;

		if (!dev_param->dp_slave_mode) {
			if (dptx_intr_get_hotplug_status(dev_param) != HPD_STATUS_UNPLUGGED) {
				mutex_lock(&dev_param->Mutex);
				ret = dptx_probe_linktraining(dev_param);
				mutex_unlock(&dev_param->Mutex);
			}
			dptx_core_enable_global_intr(dev_param,
						((uint32_t)DPTX_IEN_HPD |
						(uint32_t)DPTX_IEN_HDCP |
						(uint32_t)DPTX_IEN_SDP));
		}
	}
	if (DPTX_RETURN_ERROR(ret)) {
		if (dev_param != NULL) {
			platform_set_drvdata(pdev, NULL);
			devm_kfree(&pdev->dev, dev_param);
		}
	}
	return ret;
}

static int Dpv14_Tx_Remove(struct platform_device *pstDev)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	struct Dptx_Params *dev_param;

	dptx_dbg("Remove: DP V1.4 driver ");

	dev_param = (struct Dptx_Params *)platform_get_drvdata(pstDev);

	if (!dev_param->dp_slave_mode) {
		ret = dptx_v14_deinit(dev_param);

		dptv_v14_remove_i2c_link(dev_param);
	}

	mutex_destroy(&dev_param->Mutex);

	return ret;
}

int32_t dptx_driver_suspend_core(struct Dptx_Params *dev_param)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;

	if (!dev_param->dp_slave_mode) {
		(void)pinctrl_pm_select_sleep_state(dev_param->dev);

		dptx_info("*** PM Suspend ***");
		/* Supports slave mode */
		#if defined(CONFIG_TELECHIPS_DP_VZ)
		Dptx_Reg_Writel(dev_param, DPTX_CUSTOMPAT0, MAIN_DP_RESET_VALUE);
		#endif

		ret = dptx_v14_deinit(dev_param);

		dptx_info("Suspend: Hot %s, Ports = %u\n",
					(dev_param->last_known_hpd_status == HPD_STATUS_PLUGGED) ? "Plugged":"Unplugged",
					dev_param->ucNumOfPorts);
	}

	return ret;
}

int32_t dptx_driver_resume_core(struct Dptx_Params *dev_param)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;

	dptx_info("*** PM Resume ***");

	if (!dev_param->dp_slave_mode) {
		/* Supports slave mode */
		#if defined(CONFIG_TELECHIPS_DP_VZ)
		Dptx_Reg_Writel(dev_param, DPTX_CUSTOMPAT0, MAIN_DP_DOING_INIT);
		#endif

		ret = pinctrl_pm_select_default_state(dev_param->dev);
		if (ret != 0) {
			/* For KCS */
			dptx_err("Failed pinctrl_pm_select_default_state()");
		} else {

			dptx_info("Resume start: Hot %s, Ports = %u, PClk %d %d %d %d",
				 (dev_param->last_known_hpd_status == HPD_STATUS_PLUGGED) ? "Plugged":"Unplugged",
				 dev_param->ucNumOfPorts,
				 dev_param->video_params[PHY_INPUT_STREAM_0].pixel_clock,
				 dev_param->video_params[PHY_INPUT_STREAM_1].pixel_clock,
				 dev_param->video_params[PHY_INPUT_STREAM_2].pixel_clock,
				 dev_param->video_params[PHY_INPUT_STREAM_3].pixel_clock);
			ret = dptx_v14_init(dev_param);

			/* Supports slave mode */
			#if defined(CONFIG_TELECHIPS_DP_VZ)
			Dptx_Reg_Writel(dev_param, DPTX_CUSTOMPAT0, MAIN_DP_COMPLETED_INIT);
			#endif

			dptx_info("Resume end: Hot %s, Ports = %u, PClk %d %d %d %d",
				 (dev_param->last_known_hpd_status == HPD_STATUS_PLUGGED) ? "Plugged":"Unplugged",
				 dev_param->ucNumOfPorts,
				 dev_param->video_params[PHY_INPUT_STREAM_0].pixel_clock,
				 dev_param->video_params[PHY_INPUT_STREAM_1].pixel_clock,
				 dev_param->video_params[PHY_INPUT_STREAM_2].pixel_clock,
				 dev_param->video_params[PHY_INPUT_STREAM_3].pixel_clock);
		}
	}
	return ret;
}

#if defined(CONFIG_PM)
static int dptx_driver_suspend(struct device *dev)
{
	struct Dptx_Params *dev_param;

	dev_param = (struct Dptx_Params *)dev_get_drvdata(dev);
	return dptx_driver_suspend_core(dev_param);
}

static int dptx_driver_resume(struct device *dev)
{
	struct Dptx_Params *dev_param;

	dev_param = (struct Dptx_Params *)dev_get_drvdata(dev);
	return dptx_driver_resume_core(dev_param);
}

static const struct dev_pm_ops dp_pinctrl_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(dptx_driver_suspend, dptx_driver_resume)
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
};

static __init int Dpv14_Tx_init(void)
{
	return platform_driver_register(&stDpv14_Tx_pdrv);
}
module_init(Dpv14_Tx_init);

static __exit void Dpv14_Tx_exit(void)
{
	return platform_driver_unregister(&stDpv14_Tx_pdrv);
}
module_exit(Dpv14_Tx_exit);

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("DP Tx Driver");
MODULE_LICENSE("GPL");

MODULE_VERSION(__stringify(TCC_DPTX_DRV_MAJOR_VER) "."
	__stringify(TCC_DPTX_DRV_MINOR_VER) "."
	__stringify(TCC_DPTX_DRV_SUBTITLE_VER));


