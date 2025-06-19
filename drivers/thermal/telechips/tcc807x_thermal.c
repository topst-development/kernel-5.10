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
#include <linux/limits.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/cpu_cooling.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/arm-smccc.h>
#include "thermal_common.h"
#include "../thermal_core.h"

static void tcc807x_otp_data_check(struct tcc_thermal_platform_data *data)
{
	int32_t i;

	for (i = 0; i < 11; i++) {
		if (data->temp_trim1[i] == 0) {
			data->temp_trim1[i] = 285;
		}

		if (data->temp_trim2[i] == 0) {
			data->temp_trim2[i] = 345;
		}
	}
	if (data->calib_sel > 4) {
		data->calib_sel = 2;
	}

	if (data->d_otp_slope == 0) {
		data->d_otp_slope = 5;
	}

	if (data->ts_test_info_high == 0) {
		data->ts_test_info_high = 8500;
	}

	if (data->ts_test_info_low == 0) {
		data->ts_test_info_low = 2500;
	}
	if (data->core == 1) {
		(void)pr_info("[INFO][TSENSOR] %s. main_temp_trim1: %d\n",
				__func__, data->temp_trim1[0]);
		(void)pr_info("[INFO][TSENSOR] %s. main_temp_trim2: %d\n",
				__func__, data->temp_trim2[0]);

		for (i = 1; i < 11; i++) {
			(void)pr_info(
				"[INFO][TSENSOR] %s. probe%d_temp_trim1: %d\n",
					__func__, i, data->temp_trim1[i]);
			(void)pr_info(
				"[INFO][TSENSOR] %s. probe%d_temp_trim2: %d\n",
					__func__, i, data->temp_trim2[i]);
		}
		(void)pr_info("[INFO][TSENSOR] %s. cal_type: %d\n",
				__func__, data->calib_sel);
		(void)pr_info("[INFO][TSENSOR] %s. ts_test_info_high: %d\n",
				__func__, data->ts_test_info_high);
		(void)pr_info("[INFO][TSENSOR] %s. ts_test_info_low: %d\n",
				__func__, data->ts_test_info_low);
	}

}

static int32_t tcc_get_fuse_data(const struct platform_device *pdev)
{
	struct tcc_thermal_platform_data *data = platform_get_drvdata(pdev);

	tcc807x_otp_data_check(data);

	data->resolution = TCC807X_DEF_TEMP_RESOLUTION_VAL;
	data->temp_code_val = TCC807X_DEF_TEMP_CODE_VAL;

	return 0;
}

static int32_t tcc_thermal_init(const struct tcc_thermal_platform_data *data)
{
	u32 v_temp;
	int32_t threshold_low_temp[11];
	int32_t threshold_high_temp[11];
	int32_t ret = 0;
	uint32_t i;

	if (data == NULL) {
		(void)pr_err("[TSENSOR] %s: tcc_thermal_data error!!\n",
				__func__);
		ret = -EINVAL;
	}

	if ((data != NULL) && (data->init)) {
		if (data->base != NULL) {
			writel(0U, data->base + TCC807X_T_EN_REG);
			writel(0x7FFU, data->base + TCC807X_PROBE_SEL);
			if (data->tem0804->bjt_emitter_current >= 0) {
				writel((uint32_t)data->tem0804->bjt_emitter_current,
						data->base + TCC807X_THERMAL_BIT_REG);
			}
			if (data->tem0804->bgr_control >= 0) {
				writel((uint32_t)data->tem0804->bgr_control,
							data->base + TCC807X_BRT);
			}
			if (data->tem0804->vref_trimming_value >= 0) {
				writel((uint32_t)data->tem0804->vref_trimming_value,
							data->base + TCC807X_VT);
			}
			if (data->tem0804->vbe_current_trimming >= 0) {
				writel((uint32_t)data->tem0804->vbe_current_trimming,
							data->base + TCC807X_VIT);
			}
			if (data->tem0804->slope_setting >= 0) {
				writel((uint32_t)data->tem0804->slope_setting,
							data->base + TCC807X_BSS);
			}
			if (data->tem0804->reference_voltage >= 0) {
				writel((uint32_t)data->tem0804->reference_voltage,
							data->base + TCC807X_BVS);
			}
			if (data->tem0804->comp_current_trim >= 0) {
				writel((uint32_t)data->tem0804->comp_current_trim,
							data->base + TCC807X_CIT);
			}
		}
		for (i = 0U; i < 11U; i++) {
			threshold_high_temp[i] = tcc_temp_to_code(data,
					data->temp_trim1[i],
					data->temp_trim2[i],
					data->threshold_high_temp);
			if (threshold_high_temp[i] >= 0) {
				writel((uint32_t)threshold_high_temp[i],
						data->base +
						TCC807X_THRESHOLD_UP +
						(i*TCC807X_IRQ_INTV));
			} else {
				writel(345, data->base +
					TCC807X_THRESHOLD_UP +
					(i*TCC807X_IRQ_INTV));
			}
		}

		for (i = 0U; i < 11U; i++) {
			threshold_low_temp[i] = tcc_temp_to_code(data,
					data->temp_trim1[i],
					data->temp_trim2[i],
					data->threshold_low_temp);
			if (threshold_low_temp[i] >= 0) {
				writel((uint32_t)threshold_low_temp[i],
						data->base +
						TCC807X_THRESHOLD_DOWN +
						(i*TCC807X_IRQ_INTV));
			} else {
				writel(285, data->base +
					TCC807X_THRESHOLD_DOWN +
					(i*TCC807X_IRQ_INTV));
			}
		}

		(void)pr_info("%s.thermal maincore threshold high temp: %d\n",
				__func__, threshold_high_temp[1]);
		(void)pr_info("%s.thermal maincore threshold low temp: %d\n",
				__func__, threshold_low_temp[1]);
		(void)pr_info("%s.thermal subcore threshold high temp: %d\n",
				__func__, threshold_high_temp[0]);
		(void)pr_info("%s.thermal subcore threshold low temp: %d\n",
				__func__, threshold_low_temp[0]);
		writel(0, data->base + TCC807X_INT_EN0); // Default interrupt disable
		writel(0, data->base + TCC807X_INT_EN1); // Default interrupt disable

		writel(data->interval_time, data->base + TCC807X_T_TIME_INTV);
		writel((CAPTURE_IRQ | HIGH_TEMP_IRQ | LOW_TEMP_IRQ),
				data->base + TCC807X_INT_CLEAR1);
		writel((CAPTURE_IRQ | HIGH_TEMP_IRQ | LOW_TEMP_IRQ),
				data->base + TCC807X_INT_CLEAR0);
		writel(((~(TCC807X_MAIN_IRQ_EN | TCC807X_PROBE0_IRQ_EN)) &
					(LOW_TEMP_IRQ | HIGH_TEMP_IRQ)),
				data->base + TCC807X_INT_MASK1);
		writel(((~(TCC807X_MAIN_IRQ_EN | TCC807X_PROBE0_IRQ_EN)) &
					(LOW_TEMP_IRQ | HIGH_TEMP_IRQ)),
				data->base + TCC807X_INT_MASK0);
		writel((HIGH_TEMP_IRQ | LOW_TEMP_IRQ),
				data->base + TCC807X_INT_EN0);
		writel((HIGH_TEMP_IRQ | LOW_TEMP_IRQ),
				data->base + TCC807X_INT_EN1);
		v_temp = readl_relaxed(data->base + TCC807X_CONTROL_REG);
		v_temp = (v_temp & 0xEEU);
		v_temp |= ((uint32_t)0x1 << 4); // interval time enable
		v_temp |= ((uint32_t)0x1); // continuous mode
		writel(v_temp, data->base + TCC807X_CONTROL_REG);
		writel(1, data->base + TCC807X_T_EN_REG);
	} else {
		(void)pr_info("[TSENSOR] Not set configuration register\n");
	}

	return ret;
}

static int32_t tcc_parse_dt(const struct platform_device *pdev,
		struct tcc_thermal_platform_data *pdata)
{
	const struct device_node *np;
	const char *tmp_str;
	int32_t ret = 0;

	mutex_init(&pdata->lock);

	if (pdev->dev.of_node != NULL) {
		np = pdev->dev.of_node;
	} else {
		(void)pr_err(
				"[ERROR][TSENSOR]%s: failed to get device node\n",
				__func__);
		ret = -ENODEV;
		goto retval;
	}
	if (pdata == NULL) {
		(void)pr_err(
				"[ERROR][TSENSOR]%s: failed to get platform data\n",
				__func__);
		ret = -EINVAL;
		goto retval;
	}

	ret = of_property_read_string(np, "init", &tmp_str);
	if (ret != 0) {
		(void)pr_err(
				"[ERROR][TSENSOR]%s:failed to get init from dt\n",
				__func__);
		tmp_str = "false";
	}

	ret = strncmp(tmp_str, "okay", strnlen(tmp_str, 30));
	if (ret == 0) {
		pdata->init = (bool)true;
	} else {
		pdata->init = (bool)false;
	}

	ret = of_property_read_s32(np, "threshold_low_temp",
			&pdata->threshold_low_temp);
	if (ret != 0) {
		(void)pr_err(
				"%s:failed to get threshold_low_temp\n",
				__func__);
		pdata->threshold_low_temp = THRESHOLD_MIN_TEMP;
	}

	ret = of_property_read_s32(np, "threshold_high_temp",
			&pdata->threshold_high_temp);
	if (ret != 0) {
		(void)pr_err("%s:failed to get threshold_high_temp\n",
				__func__);
		pdata->threshold_high_temp = THRESHOLD_MAX_TEMP;
	}

	ret = of_property_read_s32(np, "core", &pdata->core);
	if (ret != 0) {
		(void)pr_err("%s:failed to get thermal probe number\n",
				__func__);
		pdata->core = 0;
		pdata->probe_num = TCC807X_MAIN_PROBE;
	} else {
		pdata->probe_num = (pdata->core * 4) + TCC807X_MAIN_PROBE;
	}

	ret = of_property_read_u32(np, "interval_time",
			&pdata->interval_time);
	if (ret != 0) {
		(void)pr_err(
				"%s:failed to get interval_time\n",
				__func__);
		pdata->interval_time = TCC807X_DEFAULT_INTERVAL;
	}
retval:
	return ret;
}

static struct tsens_reg_v2 tsens_reg;

static struct thermal_zone_of_device_ops tem_dev_ops = {
	.get_temp = tcc_get_temp,
	.get_trend = tcc_get_trend,
	.set_emul_temp = tcc_set_emul_temp,
};

static const struct thermal_ops tcc_tsens_ops = {
	.init			= tcc_thermal_init,
	.parse_dt		= tcc_parse_dt,
	.get_fuse_data		= tcc_get_fuse_data,
	.t_ops			= &tem_dev_ops,
};

static struct tcc_thermal_platform_data tcc_tsens_pdata = {
	.name		= "tcc_tsens",
	.ops		= &tcc_tsens_ops,
	.tem0804	= &tsens_reg,
};

struct tcc_thermal_data tcc_tsens_data = {
	.irq		= 40,
	.pdata		= &tcc_tsens_pdata,
};

MODULE_AUTHOR("jay.kim@telechips.com");
MODULE_DESCRIPTION("Telechips thermal driver");
MODULE_LICENSE("GPL");
