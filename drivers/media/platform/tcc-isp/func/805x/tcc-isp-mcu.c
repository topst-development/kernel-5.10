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
uint32_t tcc_isp_tuning_mode;

struct tcc_isp_mcu_control {
	struct tcc_isp_state *isp_state;
	uint32_t is_probed;
	uint32_t is_started;
};
struct tcc_isp_mcu_control mcu_ctl[TCC_ISP_MAX_CORE];

void tcc_isp_mcu_set_tuning_mode(uint32_t tuning_mode)
{
	mutex_lock(&isp_lock);

	tcc_isp_tuning_mode = tuning_mode;

	mutex_unlock(&isp_lock);
}

uint32_t tcc_isp_mcu_get_tuning_mode(void)
{
	uint32_t ret;

	mutex_lock(&isp_lock);

	ret = tcc_isp_tuning_mode;

	mutex_unlock(&isp_lock);

	return ret;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static uint32_t tcc_isp_mcu_which_is_probed(void)
{
	uint32_t idx = 0U;
	uint32_t ret = 0U;

	mutex_lock(&isp_lock);

	for (idx = 0U; idx < TCC_ISP_MAX_CORE; idx++) {
		if (mcu_ctl[idx].is_probed == TCC_ISP_PROBED) {
			/* set which isp is probed */
			ret |= (((uint32_t)1U) << idx);
		}
	}

	mutex_unlock(&isp_lock);

	return ret;
}
#endif

static uint32_t tcc_isp_mcu_is_all_isp_started(void)
{
	uint32_t idx = 0U, is_tuning_mode = 0U;
	uint32_t ret = TCC_ISP_STARTED, skip_perm = 0U;

	is_tuning_mode = tcc_isp_mcu_get_tuning_mode();

	mutex_lock(&isp_lock);

	if (is_tuning_mode != 0U) {
		/* only one isp operates in isp tuning mode */
		skip_perm = 1U;
	}

	if (skip_perm == 0U) {
		for (idx = 0U; idx < TCC_ISP_MAX_CORE; idx++) {
			if ((mcu_ctl[idx].is_probed == TCC_ISP_PROBED) &&
			    (mcu_ctl[idx].is_started != TCC_ISP_STARTED)) {
				ret = TCC_ISP_STOPPED;
			}
		}
	}

	mutex_unlock(&isp_lock);

	return ret;
}

uint32_t tcc_isp_mcu_is_all_isp_stopped(void)
{
	uint32_t idx = 0U, is_tuning_mode = 0U;
	uint32_t ret = TCC_ISP_STOPPED, skip_perm = 0U;

	is_tuning_mode = tcc_isp_mcu_get_tuning_mode();

	mutex_lock(&isp_lock);

	if (is_tuning_mode != 0U) {
		/* only one isp operates in isp tuning mode */
		skip_perm = 1U;
	}

	if (skip_perm == 0U) {
		for (idx = 0U; idx < TCC_ISP_MAX_CORE; idx++) {
			if ((mcu_ctl[idx].is_probed == TCC_ISP_PROBED) &&
			    (mcu_ctl[idx].is_started != TCC_ISP_STOPPED)) {
				ret = TCC_ISP_STARTED;
			}
		}
	}

	mutex_unlock(&isp_lock);

	return ret;
}

static void tcc_isp_mcu_set_ctl_started(const struct tcc_isp_state *state,
					uint32_t is_started)
{
	mutex_lock(&isp_lock);
	if ((state->pdev->id >= 0) &&
	    (((uint32_t)state->pdev->id) < TCC_ISP_MAX_CORE)) {
		/* okay */
		mcu_ctl[state->pdev->id].is_started = is_started;
	} else {
		/* error */
		loge(&(state->pdev->dev), "invalid id(%d)\n", state->pdev->id);
	}
	mutex_unlock(&isp_lock);
}

void tcc_isp_mcu_set_ctl_probed(struct tcc_isp_state *state, uint32_t is_probed)
{
	if ((state->pdev->id >= 0) &&
	    (((uint32_t)state->pdev->id) < TCC_ISP_MAX_CORE)) {
		/* okay */
		if (is_probed == TCC_ISP_PROBED) {
			/* probed */
			mcu_ctl[state->pdev->id].isp_state = state;
		} else {
			/* not probed */
			mcu_ctl[state->pdev->id].isp_state = NULL;
		}
		mcu_ctl[state->pdev->id].is_probed = is_probed;
	} else {
		/* error */
		loge(&(state->pdev->dev), "invalid id(%d)\n", state->pdev->id);
	}
}

static void tcc_isp_mcu_enable(const struct tcc_isp_state *state, int onOff)
{
	uint32_t val = 0U;
	void __iomem *isp_base = state->isp_base;

	if (onOff == ON) {
		/* MCU enable */
		val = tcc_isp_readl(isp_base, REG_ISP_MCU_CTL);
		val &= ~(MCU_CTL_MCU_EN_MASK << MCU_CTL_MCU_EN_SHIFT);
		val |= (MCU_CTL_MCU_EN_ENABLE << MCU_CTL_MCU_EN_SHIFT);
		tcc_isp_writel(val, isp_base, REG_ISP_MCU_CTL);

	} else {
		/* MCU disable */
		val = tcc_isp_readl(isp_base, REG_ISP_MCU_CTL);
		val &= ~(MCU_CTL_MCU_EN_MASK << MCU_CTL_MCU_EN_SHIFT);
		val |= (MCU_CTL_MCU_EN_DISABLE << MCU_CTL_MCU_EN_SHIFT);
		tcc_isp_writel(val, isp_base, REG_ISP_MCU_CTL);
	}
}

static void tcc_isp_mcu_enable_all(const struct tcc_isp_state *state, int onOff)
{
	uint32_t idx = 0U;

	if (onOff == ON) {
		for (idx = 0U; idx < TCC_ISP_MAX_CORE; idx++) {
			if (mcu_ctl[idx].is_probed == TCC_ISP_PROBED) {
				/* enable mcu */
				tcc_isp_mcu_enable(mcu_ctl[idx].isp_state, ON);
			}
		}

		if (state->mdelay_to_out != 0UL) {
			logi(&(state->pdev->dev), "delay %llums\n",
			     state->mdelay_to_out);
			usleep_range(state->mdelay_to_out * 1000UL,
				     (state->mdelay_to_out * 1000UL) + 5000UL);
		}

		logi(&(state->pdev->dev), "all mcu are enabled\n");
	} else {
		for (idx = 0U; idx < TCC_ISP_MAX_CORE; idx++) {
			if (mcu_ctl[idx].is_probed == TCC_ISP_PROBED) {
				/* disable mcu */
				tcc_isp_mcu_enable(mcu_ctl[idx].isp_state, OFF);
			}
		}

		logi(&(state->pdev->dev), "all mcu are disabled\n");
	}
}

void tcc_isp_mcu_s_stream(const struct tcc_isp_state *state, int32_t enable)
{
	if (enable != 0) {
		tcc_isp_mcu_set_ctl_started(state, TCC_ISP_STARTED);

		if (tcc_isp_mcu_is_all_isp_started() == TCC_ISP_STARTED) {
			/* enable mcu */
			tcc_isp_mcu_enable_all(state, ON);
		} else {
			logi(&(state->pdev->dev), "skip enable mcu\n");
		}
	} else {
		/* disable mcu */
		tcc_isp_mcu_set_ctl_started(state, TCC_ISP_STOPPED);

		if (tcc_isp_mcu_is_all_isp_stopped() == TCC_ISP_STOPPED) {
			/* disable mcu */
			tcc_isp_mcu_enable_all(state, OFF);
		} else {
			logi(&(state->pdev->dev), "skip disable mcu\n");
		}
	}
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int32_t tcc_isp_mcu_set_stop_req(void)
{
	int32_t ret = 0;

	ret = tcc_i2c7_set_trfc(MCU_DISABLE_REQ);
	if (ret >= 0) {
		/* okay */
		/* wait ack */
		msleep(34U * 5U);
	}

	return ret;
}

static int32_t tcc_isp_mcu_clr_stop_req(void)
{
	int32_t ret = 0;

	ret = tcc_i2c7_set_trfc(MCU_DISABLE_CLR);

	return ret;
}

static int32_t tcc_isp_mcu_get_stop_ack(const struct tcc_isp_state *state,
					uint32_t *ack)
{
	int32_t ret = 0;

	ret = tcc_i2c7_get_trfc(ack);
	if (ret < 0) {
		ret = tcc_isp_mcu_clr_stop_req();
		if (ret < 0) {
			loge(&(state->pdev->dev),
			     "tcc_isp_mcu_clr_stop_req returned %d\n", ret);
		}
		ret = -EBUSY;
	} else {
		*ack &= MCU_DISABLE_ACK;
	}

	return ret;
}

static int32_t tcc_isp_mcu_check_stop_ack(const struct tcc_isp_state *state,
					  uint32_t ack)
{
	int32_t ret = 0;

	if (ack != tcc_isp_mcu_which_is_probed()) {
		/* error */
		loge(&(state->pdev->dev), "NOT recevied all ack(0x%x)\n", ack);

		ret = tcc_isp_mcu_clr_stop_req();
		if (ret < 0) {
			loge(&(state->pdev->dev),
			     "tcc_isp_mcu_clr_stop_req returned %d\n", ret);
		} else {
			/* enable mcu */
			tcc_isp_mcu_enable_all(state, ON);
			ret = -ETIMEDOUT;
		}
	} else {
		/* okay */
		/* wait mcu stop */
		usleep_range(5U, 10U);
	}

	return ret;
}

int32_t tcc_isp_mcu_get_access_perm(const struct tcc_isp_state *state)
{
	int32_t ret = 0;
	uint32_t ack = 0U, skip_perm = 0U;

	if (tcc_isp_mcu_get_tuning_mode() != 0U) {
		/* do not need to get access permission when isp tuning mode */
		skip_perm = 1U;
	}

	if (skip_perm == 0U) {
		/* request mcu stop */
		ret = tcc_isp_mcu_set_stop_req();
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
			     "tcc_isp_mcu_set_stop_req returned %d\n", ret);
		}

		/* receive ack */
		if (ret >= 0) {
			ret = tcc_isp_mcu_get_stop_ack(state, &ack);
			if (ret < 0) {
				/* error */
				loge(&(state->pdev->dev),
				     "tcc_isp_mcu_get_stop_ack returned %d\n",
				     ret);
			}
		}

		/* check ack */
		if (ret >= 0) {
			ret = tcc_isp_mcu_check_stop_ack(state, ack);
			if (ret < 0) {
				/* error */
				loge(&(state->pdev->dev),
				     "tcc_isp_mcu_check_stop_ack returned %d\n",
				     ret);
			}
		}
	}

	return ret;
}

int32_t tcc_isp_mcu_rel_access_perm(const struct tcc_isp_state *state)
{
	int32_t ret = 0;
	uint32_t skip_perm = 0U;

	if (tcc_isp_mcu_get_tuning_mode() != 0U) {
		/* do not need to get access permission when isp tuning mode */
		skip_perm = 1U;
	}

	if (skip_perm == 0U) {
		/* clear disable request */
		ret = tcc_isp_mcu_clr_stop_req();
		if (ret < 0) {
			loge(&(state->pdev->dev),
			     "tcc_isp_mcu_clr_stop_req returned %d\n", ret);
		} else {
			/* mcu enable */
			tcc_isp_mcu_enable_all(state, ON);
		}
	}

	return ret;
}
#endif

int32_t tcc_isp_mcu_load_firmware(struct tcc_isp_state *state,
				  const struct firmware *fw)
{
	uint32_t val = 0U;
	void __iomem *isp_base = state->isp_base;
	void __iomem *mem_base = state->mem_base;
	int32_t ret = 0;

	/* MCU memory download enable */
	val = tcc_isp_readl(isp_base, REG_ISP_MCU_MEM_CTL);
	val &= ~(MCU_MEM_CTL_MCU_MEM_DL_EN_MASK
		 << MCU_MEM_CTL_MCU_MEM_DL_EN_SHIFT);
	val |= (MCU_MEM_CTL_MCU_MEM_DL_EN_ENABLE
		<< MCU_MEM_CTL_MCU_MEM_DL_EN_SHIFT);
	tcc_isp_writel(val, isp_base, REG_ISP_MCU_MEM_CTL);

	logi(&(state->pdev->dev), "COPY FW\n");

	/* copy firmware to the MCU memory(code + data) */
	if (memcpy(mem_base, fw->data, fw->size) != (mem_base)) {
		loge(&(state->pdev->dev), "Fail - memcpy\n");
		ret = -EINVAL;
	}

	/*
	 * after copying firmware,
	 * all related cache coherency should be ensured.
	 */
	mb();

	/* MCU memory download disable */
	val = tcc_isp_readl(isp_base, REG_ISP_MCU_MEM_CTL);
	val &= ~(MCU_MEM_CTL_MCU_MEM_DL_EN_MASK
		 << MCU_MEM_CTL_MCU_MEM_DL_EN_SHIFT);
	val |= (MCU_MEM_CTL_MCU_MEM_DL_EN_DISABLE
		<< MCU_MEM_CTL_MCU_MEM_DL_EN_SHIFT);
	tcc_isp_writel(val, isp_base, REG_ISP_MCU_MEM_CTL);

	if (ret >= 0) {
		/* okay */
		state->fw_load = 1;
	}

	return ret;
}

static ssize_t tuning_mode_show(struct device_driver *d, char *buf)
{
	ssize_t ret = 0;

	ret = scnprintf(buf, PAGE_SIZE, "%u\n", tcc_isp_mcu_get_tuning_mode());
	if (!((ret > 0) && (((uint32_t)ret) < PAGE_SIZE))) {
		/* error */
		tccisp_loge("scnprintf returned %ld\n", ret);
		ret = -EINVAL;
	}

	return ret;
}

static ssize_t tuning_mode_store(struct device_driver *d, const char *buf,
				 size_t count)
{
	uint32_t val;
	ssize_t ret = 0;

	ret = kstrtouint(buf, 0, &val);
	if (ret < 0) {
		/* error */
		tccisp_loge("Fail - read data from attribute\n");
	}

	if (ret >= 0) {
		/* print value */
		if (count <= (SIZE_MAX >> 1U)) {
			tcc_isp_mcu_set_tuning_mode(val);
			ret = (ssize_t)count;
		} else {
			ret = -EINVAL;
		}
	}

	return ret;
}
static DRIVER_ATTR_RW(tuning_mode);

int32_t tcc_isp_mcu_add_drv_attr(struct platform_driver *drv)
{
	int32_t ret = 0;

	ret = driver_create_file(&drv->driver, &driver_attr_tuning_mode);
	if (ret < 0) {
		/* error */
		tccisp_loge("driver_create_file returned %d\n", ret);
	}

	return ret;
}

void tcc_isp_mcu_rm_drv_attr(struct platform_driver *drv)
{
	driver_remove_file(&drv->driver, &driver_attr_tuning_mode);
}
