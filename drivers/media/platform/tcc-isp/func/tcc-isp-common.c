// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include "../tcc-isp.h"
#include "../tcc-isp-helper.h"
#ifdef CONFIG_ARCH_TCC805X
#include "./805x/tcc-isp-reg.h"
#include "./805x/tcc-isp-mcu.h"
#endif
#if defined(CONFIG_ARCH_TCC807X)
#include "./807x/tcc-isp-reg.h"
#include "./807x/tcc-isp-mcu.h"
#endif
#if defined(CONFIG_ARCH_TCC750X)
#include "./750x/tcc-isp-reg.h"
#include "./750x/tcc-isp-mcu.h"
#endif

int32_t tcc_isp_cmm_read_file(struct tcc_isp_state *state,
			      const char *file_name,
			      int32_t (*load_func)(struct tcc_isp_state *state,
			      const struct firmware *fw))
{
	int32_t ret = 0;
	const struct firmware *fw = NULL;

	ret = request_firmware(&fw, file_name, &(state->pdev->dev));
	if (ret < 0) {
		/* error */
		loge(&(state->pdev->dev),
			"request_firmware(%s) returned error(%d)\n",
			file_name, ret);
	} else {
		/* okay */
		logi(&(state->pdev->dev),
			"load %s(size: %ld Byte) by %pS\n",
			file_name, fw->size, load_func);

		ret = load_func(state, fw);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
					"%pS returned error(%d)\n",
					load_func, ret);
		}

		release_firmware(fw);
	}

	return ret;
}

int32_t tcc_isp_cmm_add_drv_attr(struct platform_driver *drv)
{
	int32_t ret = 0;

	ret = tcc_isp_mcu_add_drv_attr(drv);
	if (ret < 0) {
		/* error */
		tccisp_loge("tcc_isp_mcu_add_drv_attr returned %d\n", ret);
	}

	return ret;
}

void tcc_isp_cmm_rm_drv_attr(struct platform_driver *drv)
{
	tcc_isp_mcu_rm_drv_attr(drv);
}

