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
#include <linux/limits.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/cpu_cooling.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include "thermal_common.h"
#include "../thermal_core.h"

#define DEFAULT_C_CODE	46
#define DEFAULT_C_TRIM	46

static int32_t legacy_c_to_t(const struct tcc_thermal_platform_data *pdata,
		int32_t temp_trim1, int32_t temp_code)
{
        int32_t temp;
        int32_t c_trim;
        int32_t c_code;

	if ((temp_code > (__INT_MAX__/2)) || (temp_code < (INT_MIN/2))) {
		c_code = DEFAULT_C_CODE;
	} else {
		c_code = temp_code;
	}

	if ((temp_trim1 > (__INT_MAX__/2)) ||
			(temp_trim1 < (INT_MIN/2))) {
		c_trim = DEFAULT_C_TRIM;
	} else {
		c_trim = temp_trim1;
	}

        switch (pdata->calib_sel) {
        case TYPE_ONE_POINT_TRIMMING:
                temp = (c_code - c_trim) + 25;
                break;
        case TYPE_NONE:
                temp = c_code - 21;
                break;
        default:
                temp = (c_code - c_trim) + 25;
                break;
        }

        return temp;
}

static int32_t tccxxxx_thermal_read(struct tcc_thermal_platform_data *pdata)
{
	int32_t celsius_temp = 0;
	uint32_t udigit = 0;
	int32_t digit = 0;

	udigit = readl_relaxed(pdata->base + TCCXXXX_MAIN_PROBE);

	if (udigit < 0x7FFFFFFFU) {
		digit = (int32_t)udigit;
	} else {
		digit = __INT_MAX__;
	}

	celsius_temp = legacy_c_to_t(pdata, pdata->temp_trim1[0], digit);

	return celsius_temp;
}


int32_t tccxxxx_get_temp(void *tz, int32_t *temp)
{
	struct tcc_thermal_platform_data *pdata = tz;

	*temp = tccxxxx_thermal_read(pdata);
	if (*temp < (__INT_MAX__ / MCELSIUS)) {
		*temp = *temp * MCELSIUS;
	}

	return 0;
}

int32_t tccxxxx_get_trend(void *tz, int32_t trip, enum thermal_trend *trend)
{
	struct tcc_thermal_platform_data *pdata = tz;
	int32_t cur_trip_temp;

	if (pdata->tz == NULL) {
		*trend = THERMAL_TREND_STABLE;
	} else {
		(void)pdata->tz->ops->get_trip_temp(pdata->tz,
						trip, &cur_trip_temp);

		if (pdata->tz->temperature > cur_trip_temp) {
			*trend = THERMAL_TREND_RAISING;
		} else if (pdata->tz->temperature < cur_trip_temp) {
			*trend = THERMAL_TREND_DROPPING;
		} else {
			*trend = THERMAL_TREND_STABLE;
		}
	}

	return 0;
}

//dummy
MODULE_AUTHOR("jay.kim@telechips.com");
MODULE_DESCRIPTION("Telechips thermal driver");
MODULE_LICENSE("GPL");
