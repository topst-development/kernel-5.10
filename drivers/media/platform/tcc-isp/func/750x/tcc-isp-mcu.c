// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include "../../tcc-isp.h"
#include "../../tcc-isp-helper.h"
#include "tcc-isp-reg.h"

DEFINE_MUTEX(isp_lock);
static uint32_t tcc_isp_mcu_enable_cnt;
static uint32_t tcc_isp_mcu_fw_load;

void tcc_isp_mcu_set_ctl_probed(struct tcc_isp_state *state,
				uint32_t is_probed)
{

}

static int32_t tcc_isp_mcu_reset(struct tcc_isp_state *state, int reset)
{
	void __iomem *cfg_base = state->cfg_base;
	uint32_t val = 0U;
	uint32_t swrst = CAM_SWRST_IRS_MASK;
	int32_t ret = 0;

	mutex_lock(&isp_lock);

	if (reset == ON) {
		if (tcc_isp_mcu_enable_cnt > 0U) {
			/* decrease count variable */
			tcc_isp_mcu_enable_cnt--;
		}

		if (tcc_isp_mcu_enable_cnt == 0U) {
			/* reset state */
			val = tcc_isp_readl(cfg_base, CAM_SWRST);
			val |= swrst;
			tcc_isp_writel(val, cfg_base, CAM_SWRST);

			logd(&(state->pdev->dev), "reset (cnt is %d)\n",
			     tcc_isp_mcu_enable_cnt);
		} else {
			/* skip reset state */
			logd(&(state->pdev->dev), "reset skip (cnt is %d)\n",
			     tcc_isp_mcu_enable_cnt);
		}
	} else {
		tcc_isp_mcu_enable_cnt++;

		if (tcc_isp_mcu_enable_cnt == 1U) {
			/* reset release state */
			val = tcc_isp_readl(cfg_base, CAM_SWRST);
			val &= ~(swrst);
			tcc_isp_writel(val, cfg_base, CAM_SWRST);

			logd(&(state->pdev->dev), "reset release (cnt is %d)\n",
			     tcc_isp_mcu_enable_cnt);
		} else {
			/* skip reset release state */
			logd(&(state->pdev->dev),
			     "reset release skip (cnt is %d)\n",
			     tcc_isp_mcu_enable_cnt);
		}
	}

	mutex_unlock(&isp_lock);

	return ret;
}

void tcc_isp_mcu_s_stream(struct tcc_isp_state *state, int32_t enable)
{
	if (enable != 0) {
		/* enable mcu */
		tcc_isp_mcu_reset(state, OFF);
	} else {
		/* disable mcu */
		tcc_isp_mcu_reset(state, ON);
	}
}

/**
 * tcc_isp_mcu_set_mem_protect - enable/disable program memory of mcu protection
 *
 * @state: pointer to struct tcc_isp_state
 * @onOff: protection or not
 *	1 = enable memory protection
 *	0 = disable memory protection
 *
 * If memory protection is enabled, program memory of mcu can not be accessed.
 */
static void tcc_isp_mcu_set_mem_protect(struct tcc_isp_state *state,
					uint32_t onOff)
{
	uint32_t val = 0U;

	val = tcc_isp_readl(state->cfg_base, ISP_CTRL0);
	if (onOff == 1U) {
		/* enable memory protection */
		val |= (ISP_CTRL_0_IMP_MASK);
	} else {
		/* disable memory protection */
		val &= ~(ISP_CTRL_0_IMP_MASK);
	}

	tcc_isp_writel(val, state->cfg_base, ISP_CTRL0);
}

int32_t tcc_isp_mcu_load_firmware(struct tcc_isp_state *state,
				  const struct firmware *fw)
{
	void __iomem *mem_base = state->mem_base;
	int32_t ret = 0;

	mutex_lock(&isp_lock);
	state->fw_load = tcc_isp_mcu_fw_load;
	mutex_unlock(&isp_lock);

	/* load f/w to the mcu memory */
	if (state->fw_load == 0) {
		/* disabled memory protection */
		tcc_isp_mcu_set_mem_protect(state, 0U);

		logd(&(state->pdev->dev), "COPY FW\n");

		/* copy firmware to the MCU memory(code + data) */
		if (memcpy(mem_base, fw->data, fw->size) != mem_base) {
			loge(&(state->pdev->dev), "Fail - memcpy\n");
			ret = -EINVAL;
		}

		if (ret >= 0) {
			/*
			 * after copying firmware,
			 * all related cache coherency should be ensured.
			 */
			mb();

			/* okay */
			state->fw_load = 1;

			mutex_lock(&isp_lock);
			tcc_isp_mcu_fw_load = 1;
			mutex_unlock(&isp_lock);
		}

		/* enable memory protection */
		tcc_isp_mcu_set_mem_protect(state, 1U);
	} else {
		/* skip loading */
		logd(&(state->pdev->dev), "ISP MCU F/W is already loaded\n");
	}

	return ret;
}

int32_t tcc_isp_mcu_add_drv_attr(struct platform_driver *drv)
{
	int32_t ret = 0;

	return ret;
}

void tcc_isp_mcu_rm_drv_attr(struct platform_driver *drv)
{

}
