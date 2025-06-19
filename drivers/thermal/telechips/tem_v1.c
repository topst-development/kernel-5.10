// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/delay.h>
#include <linux/cpu_cooling.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/limits.h>
#include "thermal_common.h"
#include "../thermal_core.h"

// decimal value => register value
int32_t tcc_temp_to_code(const struct tcc_thermal_platform_data *pdata,
		int32_t temp_trim1, int32_t temp_trim2, int32_t temp)
{
	/*Always apply two point calibration*/
	int32_t temp_code;
	int64_t l_pre_fuse;
	int32_t pre_fuse;
	int64_t res;
	int64_t l_trim_val;
	int32_t trim_val;

	l_pre_fuse = (int64_t)pdata->ts_test_info_high -
			(int64_t)pdata->ts_test_info_low;

	if ((l_pre_fuse > __INT_MAX__) || (l_pre_fuse < INT_MIN)) {
		pre_fuse = 6000;
	} else {
		pre_fuse = (int32_t)l_pre_fuse;
	}

	l_trim_val = ((int64_t)temp_trim2 - (int64_t)temp_trim1);
	if ((l_trim_val > __INT_MAX__) || (l_trim_val < INT_MIN)) {
		trim_val = pdata->temp_code_val;
	} else {
		trim_val = (int32_t)l_trim_val;
	}

	if ((pdata->calib_sel == 0) ||
			(pdata->calib_sel == 1)) {
		if (temp >= 25) {
			temp_code = temp - 25;
			temp_code = temp_code * (10000/pdata->resolution);
		} else {
			temp_code = (temp - 25);
			temp_code = temp_code *	(57 + pdata->d_otp_slope);
			temp_code = temp_code / 65;
			temp_code = temp_code * (10000/pdata->resolution);
		}
	} else {
		if (temp >= 25) {
			temp_code = temp - 25;
			temp_code = temp_code * trim_val;
			temp_code = temp_code / (pre_fuse / 100);
		} else {
			temp_code = (temp - 25);
			temp_code = temp_code *	(57 + pdata->d_otp_slope);
			temp_code = temp_code / 65;
			temp_code = temp_code * trim_val;
			temp_code = temp_code / (pre_fuse / 100);
		}
	}

	res = ((int64_t)temp_code + (int64_t)temp_trim1);
	if ((res > __INT_MAX__) || (res < INT_MIN)) {
		temp_code = 2500;
	} else {
		temp_code = (int32_t)res;
	}

	return temp_code;
}

// register value > decimal value
int32_t tcc_code_to_temp(const struct tcc_thermal_platform_data *pdata,
		int32_t temp_trim1, int32_t temp_trim2, int32_t temp_code)
{
	/*Always apply two point calibration*/
	int32_t temp;
	int64_t code;
	int64_t l_d_otp_temp;
	int32_t d_otp_temp;
	int64_t l_pre_fuse;
	int32_t pre_fuse;

	code = (int64_t)temp_code - (int64_t)temp_trim1;
	if ((code > __INT_MAX__) || (code < INT_MIN)) {
		temp = 200;
	} else {
		temp = (int32_t)code;
	}
	l_d_otp_temp = (int64_t)temp_trim2 - (int64_t)temp_trim1;
	if ((l_d_otp_temp > __INT_MAX__) || (l_d_otp_temp < INT_MIN)) {
		d_otp_temp = pdata->temp_code_val;
	} else {
		d_otp_temp = (int32_t)l_d_otp_temp;
	}

	l_pre_fuse = (int64_t)pdata->ts_test_info_high -
			(int64_t)pdata->ts_test_info_low;

	if ((l_pre_fuse > __INT_MAX__) || (l_pre_fuse < INT_MIN)) {
		pre_fuse = 6000;
	} else {
		pre_fuse = (int32_t)l_pre_fuse;
	}

	if ((pdata->calib_sel == 1) || (pdata->calib_sel == 0)) {
		if (temp_code >= temp_trim1) {
			temp = temp * pdata->resolution;
			temp = temp / 100;
		} else {
			temp = temp * pdata->resolution;
			temp = temp / 100;
			temp = temp * (6500 / (57 + pdata->d_otp_slope));
			temp = temp / 100;
		}
	} else {
		if (temp_code >= temp_trim1) {
			temp = temp  * pre_fuse;
			temp = temp / d_otp_temp;
		} else {
			temp = temp  * pre_fuse;
			temp = temp / d_otp_temp;
			temp = temp * (6500 / (57 + pdata->d_otp_slope));
			temp = temp / 100;
		}
	}

	if (temp < (__INT_MAX__ - 2500)) {
		temp = temp + 2500;
	}

	return temp;
}

int32_t tcc_set_emul_temp(void *tz, int32_t temp)
{
	const struct tcc_thermal_platform_data *t_pdata = tz;

	t_pdata->tz->emul_temperature = temp;
	(void)pr_info("emul_temp is set to %d\n",
			t_pdata->tz->emul_temperature);

	return t_pdata->tz->temperature;
}

static int32_t tcc_thermal_read(
			const struct tcc_thermal_platform_data *pdata)
{
	int32_t celsius_temp = 0;
	int32_t i = 0;
	uint32_t udigit = 0;
	int32_t digit = 0;

	if (pdata->probe_num >= 0xD0) {
		i = ((pdata->probe_num - 0xD0) / 4);
	} else {
		i = 0;
	}

	udigit = readl_relaxed(pdata->base + pdata->probe_num);
	if (udigit < 0x7FFFFFFFU) {
		digit = (int32_t)udigit;
	} else {
		digit = __INT_MAX__;
	}

	celsius_temp = tcc_code_to_temp(pdata,
			pdata->temp_trim1[i],
			pdata->temp_trim2[i],
			digit);

	return celsius_temp;
}


int32_t tcc_get_temp(void *tz, int32_t *temp)
{
	const struct tcc_thermal_platform_data *pdata = tz;

	*temp = tcc_thermal_read(pdata);
	if (*temp < (__INT_MAX__ / 10)) {
		*temp = *temp * 10;
	}

	return 0;
}

int32_t tcc_get_trend(void *tz, int32_t trip, enum thermal_trend *trend)
{
	const struct tcc_thermal_platform_data *pdata = tz;
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

MODULE_AUTHOR("jay.kim@telechips.com");
MODULE_DESCRIPTION("Telechips thermal driver");
MODULE_LICENSE("GPL");
