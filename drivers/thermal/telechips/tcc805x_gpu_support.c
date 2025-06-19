// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/cpu_cooling.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/arm-smccc.h>
#include <soc/telechips/smc.h>
#include <linux/soc/telechips/tcc_sc_protocol.h>
#include <trace/events/thermal.h>
#include "thermal_common.h"
#include "../thermal_core.h"

#define GPU_UNUSED(x)	((void)(x))
#define DEBUG

#if defined(CONFIG_PVR_TCC9XTP_DFS_SUPPORT)
extern int tcc_9xtp_mode_is_host(void);
extern int tcc_9xtp_set_clk_rate(unsigned long trip);
#endif

struct tcc805x_gpu_governor_data {
	uint32_t trip_level;
};

static int32_t find_trip_level(struct thermal_zone_device *tz)
{
	int32_t index = 0, trip_level = 0;

	for (index = 0; index < tz->trips; index++) {
		int32_t temp_level = 0;
		tz->ops->get_trip_temp(tz, index, &temp_level);
		if (tz->temperature > temp_level) {
			trip_level++;
		}
	}

	return trip_level;
}

static int32_t tcc805x_gpu_allocator_bind(struct thermal_zone_device *tz)
{
	struct tcc805x_gpu_governor_data *data = NULL;
	int32_t ret = 0;

	if(tz == NULL) {
		ret = -ENODEV;
		goto err_bind;
	} else {
		tz->governor_data = NULL;
		data = kzalloc(sizeof(struct tcc805x_gpu_governor_data), GFP_KERNEL);
		if(data == NULL) {
			(void)pr_err("[TSENSOR] %s: Failed to allocate governor_data!!\n",
					__func__);
			ret = -ENOMEM;
			goto err_bind;
		}

		tz->governor_data = data;
	}

err_bind:
	if(ret < 0) {
		if(tz->governor_data != NULL) {
			kfree(tz->governor_data);
			tz->governor_data = NULL;
		}
	}

	return ret;
}

static void tcc805x_gpu_allocator_unbind(struct thermal_zone_device *tz)
{
	if(tz == NULL) {
		/**/
		/**/
	} else {
		if(tz->governor_data) {
			kfree(tz->governor_data);
			tz->governor_data = NULL;
		}
	}
}
 
static int32_t tcc805x_gpu_throttle(struct thermal_zone_device *tz, int trip)
{
	int32_t ret = 0;

	GPU_UNUSED(trip);

	if(tz == NULL) {
		ret = -ENODEV;
		goto err_throttle;
	} else {
		struct tcc805x_gpu_governor_data *pdata = tz->governor_data;
		if(pdata == NULL) {
			ret = -ENOMEM;
			goto err_throttle;
		} else {
			int32_t cur_trip_level = find_trip_level(tz);
			if (pdata->trip_level != cur_trip_level) {
#if defined(CONFIG_PVR_TCC9XTP_DFS_SUPPORT)
				if(tcc_9xtp_mode_is_host()) {
					if(tcc_9xtp_set_clk_rate(cur_trip_level) < 0) {
						(void)pr_err("[TSENSOR] %s: Failed to scale gpu clock",
								__func__);
						ret = -EAGAIN;
						goto err_throttle;
					} else {
						pdata->trip_level = cur_trip_level;
					}
				}
#endif
			}
		}
	}

err_throttle:
	return ret;
}

struct thermal_governor tcc805x_gpu_cooling = {
	.name		= "tcc805x_gpu_throttle",
	.bind_to_tz	= tcc805x_gpu_allocator_bind,
	.unbind_from_tz	= tcc805x_gpu_allocator_unbind,
	.throttle	= tcc805x_gpu_throttle,
};

THERMAL_GOVERNOR_DECLARE(tcc805x_gpu_cooling);

static int32_t tcc805x_gpu_tinit(const struct tcc_thermal_platform_data *data)
{
	int32_t ret = 0;

	if (data == NULL) {
		(void)pr_err("[TSENSOR] %s: tcc_thermal_data error!!\n",
				__func__);
		ret = -EINVAL;
		goto err_init;
	}
/*
	ret = thermal_register_governor(&tcc805x_gpu_cooling);

	if (ret != 0) {
		(void)pr_info("[TSENSOR] gpu governor registration failure\n");
	}*/
	(void)pr_info("[TSENSOR] GPU does not set configuration register\n");

err_init:
	return ret;
}

static struct thermal_zone_of_device_ops _tcc805x_gpu_tops = {
	.get_temp = tem1461_get_temp,
	.get_trend = tem1461_get_trend,
	.set_emul_temp = tem1461_set_emul_temp,
};
const struct thermal_ops tcc805x_gpu_tops = {
	.init			= tcc805x_gpu_tinit,
	.parse_dt		= tem1461_parse_dt,
	.get_fuse_data		= tcc805x_get_fuse_data,
	.t_ops			= &_tcc805x_gpu_tops,
};

struct thermal_zone_params tcc805x_gpu_tparam = {
	.governor_name		= "tcc805x_gpu_throttle",
};

struct tcc_thermal_platform_data tcc805x_gpu_pdata = {
	.name		= "tcc805x-gpu",
	.ops		= &tcc805x_gpu_tops,
	.tcc_gov        = &tcc805x_gpu_tparam,
};

const struct tcc_thermal_data tcc805x_gpu_data = {
	.irq		= 40,
	.pdata		= &tcc805x_gpu_pdata,
};

MODULE_AUTHOR("android_ce@telechips.com");
MODULE_DESCRIPTION("Telechips thermal driver");
MODULE_LICENSE("GPL");
