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
#include <soc/telechips/smc.h>
#include "thermal_common.h"
#include "../thermal_core.h"

static void tcc750x_otp_data_check(struct tcc_thermal_platform_data *data)
{
	int32_t i;

	for (i = 0; i < 4; i++) {
		if (data->temp_trim1[i] == 0) {
			data->temp_trim1[i] = 1596;
		}

		if (data->temp_trim2[i] == 0) {
			data->temp_trim2[i] = 2552;
		}
	}

	if (data->calib_sel > 4) {
		data->calib_sel = 2;
	}

	if (data->d_otp_slope == 0) {
		data->d_otp_slope = 6;
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

		for (i = 1; i < 4; i++) {
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
	struct arm_smccc_res res;
	uint32_t buf[1] = {0};

	arm_smccc_smc(
		SIP_READ_OTP, (unsigned long)0xe0c, (unsigned long)32,
		(unsigned long)&buf, 0, 0, 0, 0, &res);
	data->temp_trim1[0] = (int32_t)((uint16_t)(buf[0] & 0xFFFU));
	data->temp_trim2[0] = (int32_t)((uint16_t)((buf[0] >> 16U) & 0xFFFU));
//	pr_info("[TSENSOR] OTP READ : 0x%x\n", buf[0]);

	arm_smccc_smc(
		SIP_READ_OTP, (unsigned long)0xe10, (unsigned long)32,
		(unsigned long)&buf, 0, 0, 0, 0, &res);
	data->temp_trim1[1] = (int32_t)((uint16_t)(buf[0] & 0xFFFU));
	data->temp_trim2[1] = (int32_t)((uint16_t)((buf[0] >> 16U) & 0xFFFU));
//	pr_info("[TSENSOR] OTP READ : 0x%x\n", buf[0]);

	arm_smccc_smc(
		SIP_READ_OTP, (unsigned long)0xe14, (unsigned long)32,
		(unsigned long)&buf, 0, 0, 0, 0, &res);
	data->temp_trim1[2] = (int32_t)((uint16_t)(buf[0] & 0xFFFU));
	data->temp_trim2[2] = (int32_t)((uint16_t)((buf[0] >> 16U) & 0xFFFU));
//	pr_info("[TSENSOR] OTP READ : 0x%x\n", buf[0]);

	arm_smccc_smc(
		SIP_READ_OTP, (unsigned long)0xe18, (unsigned long)32,
		(unsigned long)&buf, 0, 0, 0, 0, &res);
	data->temp_trim1[3] = (int32_t)((uint16_t)(buf[0] & 0xFFFU));
	data->temp_trim2[3] = (int32_t)((uint16_t)((buf[0] >> 16U) & 0xFFFU));
//	pr_info("[TSENSOR] OTP READ : 0x%x\n", buf[0]);

	arm_smccc_smc(
		SIP_READ_OTP, (unsigned long)0xe1c, (unsigned long)32,
		(unsigned long)&buf, 0, 0, 0, 0, &res);
	data->ts_test_info_high =
		(int32_t)((uint16_t)((buf[0] & 0xFFF000U) >> 12U));
	data->ts_test_info_high =
		(int32_t)(uint16_t)
		(((((uint32_t)data->ts_test_info_high &
		    0xFF0U) >> 4U) * 100U) +
		 (((uint32_t)data->ts_test_info_high & 0xFU) * 10U));

	data->ts_test_info_low = (int32_t)((uint16_t)(buf[0] & 0xFFFU));
	data->ts_test_info_low =
		(int32_t)(uint16_t)
		(((((uint32_t)data->ts_test_info_low &
		    0xFF0U) >> 4U) * 100U) +
		 (((uint32_t)data->ts_test_info_low & 0xFU) * 10U));
//	pr_info("[TSENSOR] OTP READ : 0x%x\n", buf[0]);

	arm_smccc_smc(
		SIP_READ_OTP, (unsigned long)0xe20, (unsigned long)32,
		(unsigned long)&buf, 0, 0, 0, 0, &res);
	data->tem1461->buf_slope_sel_ts  =
		(int32_t)(uint16_t)((buf[0] >> 28U) & 0xFU);
	data->d_otp_slope	=
		(int32_t)(uint16_t)((buf[0] >> 24U) & 0xFU);
	data->calib_sel		=
		(int32_t)(uint16_t)((buf[0] >> 14U) & 0x3U);
	data->tem1461->buf_vref_sel_ts	=
		(int32_t)(uint16_t)((buf[0] >> 12U) & 0x3U);
	data->tem1461->trim_bgr_ts	=
		(int32_t)(uint16_t)((buf[0] >> 8U) & 0xFU);
	data->tem1461->trim_vlsb_ts	=
		(int32_t)(uint16_t)((buf[0] >> 4U) & 0xFU);
	data->tem1461->trim_bjt_cur_ts	=
		(int32_t)(uint16_t)(buf[0] & 0xFU);
//	pr_info("[TSENSOR] OTP READ : 0x%x\n", buf[0]);
	tcc750x_otp_data_check(data);

	data->resolution = TCC750X_DEF_TEMP_RESOLUTION_VAL;
	data->temp_code_val = TCC750X_DEF_TEMP_CODE_VAL;

	return 0;
}

static int32_t tcc_thermal_init(const struct tcc_thermal_platform_data *data)
{
	u32 v_temp;
	int32_t threshold_low_temp[4];
	int32_t threshold_high_temp[4];
	int32_t ret = 0;
	uint32_t i;

	if (data == NULL) {
		(void)pr_err("[TSENSOR] %s: tcc_thermal_data error!!\n",
				__func__);
		ret = -EINVAL;
	}

	if ((data != NULL) && (data->init)) {
		if (data->base != NULL) {
			writel(0U, data->base + TCC750X_T_EN_REG);
			writel(0x3FU, data->base + TCC750X_PROBE_SEL);
			if (data->tem1461->trim_bgr_ts >= 0) {
				writel((uint32_t)data->tem1461->trim_bgr_ts,
						data->base + TCC750X_TB);
			}
			if (data->tem1461->trim_vlsb_ts >= 0) {
				writel((uint32_t)data->tem1461->trim_vlsb_ts,
						data->base + TCC750X_TV);
			}
			if (data->tem1461->trim_bjt_cur_ts >= 0) {
				writel((uint32_t)data->tem1461->trim_bjt_cur_ts,
						data->base + TCC750X_TBC);
			}
			if (data->tem1461->buf_slope_sel_ts >= 0) {
				writel((uint32_t)data->tem1461->buf_slope_sel_ts,
						data->base + TCC750X_BSS);
			}
			if (data->tem1461->buf_vref_sel_ts >= 0) {
				writel((uint32_t)data->tem1461->buf_vref_sel_ts,
						data->base + TCC750X_BVS);
			}
		}
		for (i = 0U; i < 4U; i++) {
			threshold_high_temp[i] = tcc_temp_to_code(data,
					data->temp_trim1[i],
					data->temp_trim2[i],
					data->threshold_high_temp);
			if (threshold_high_temp[i] >= 0) {
				writel((uint32_t)threshold_high_temp[i],
						data->base +
						TCC750X_THRESHOLD_UP +
						(i*TCC750X_IRQ_INTV));
			} else {
				writel(2552, data->base +
					TCC750X_THRESHOLD_UP +
					(i*TCC750X_IRQ_INTV));
			}
		}

		for (i = 0U; i < 4U; i++) {
			threshold_low_temp[i] = tcc_temp_to_code(data,
					data->temp_trim1[i],
					data->temp_trim2[i],
					data->threshold_low_temp);
			if (threshold_low_temp[i] >= 0) {
				writel((uint32_t)threshold_low_temp[i],
						data->base +
						TCC750X_THRESHOLD_DOWN +
						(i*TCC750X_IRQ_INTV));
			} else {
				writel(1596, data->base +
					TCC750X_THRESHOLD_DOWN +
					(i*TCC750X_IRQ_INTV));
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
		writel(0, data->base + TCC750X_INT_EN); // Default interrupt disable

		writel(data->interval_time, data->base + TCC750X_T_TIME_INTV);
		writel((CAPTURE_IRQ | HIGH_TEMP_IRQ | LOW_TEMP_IRQ),
				data->base + TCC750X_INT_CLEAR);
		writel(((~(TCC750X_CA53_IRQ_EN | TCC750X_CA72_IRQ_EN)) &
					(LOW_TEMP_IRQ | HIGH_TEMP_IRQ)),
				data->base + TCC750X_INT_MASK);
		writel((HIGH_TEMP_IRQ | LOW_TEMP_IRQ),
				data->base + TCC750X_INT_EN);
		v_temp = readl_relaxed(data->base + TCC750X_CONTROL_REG);
		v_temp = (v_temp & 0x1EU);
		v_temp |= (uint32_t)0x1; // Default mode : Continuous mode
		v_temp |= ((uint32_t)0x1 << 5); // interval time enable

		writel(v_temp, data->base + TCC750X_CONTROL_REG);
		writel(1, data->base + TCC750X_T_EN_REG);
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
		pdata->probe_num = TCC750X_MAIN_PROBE;
	} else {
		pdata->probe_num = (pdata->core * 4) + TCC750X_MAIN_PROBE;
	}

	ret = of_property_read_u32(np, "interval_time",
			&pdata->interval_time);
	if (ret != 0) {
		(void)pr_err(
				"%s:failed to get interval_time\n",
				__func__);
		pdata->interval_time = TCC750X_DEFAULT_INTERVAL;
	}
retval:
	return ret;
}

static struct tsens_reg_v1 tsens_reg;

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
	.tem1461	= &tsens_reg,
};

struct tcc_thermal_data tcc_tsens_data = {
	.irq		= 25,
	.pdata		= &tcc_tsens_pdata,
};


MODULE_AUTHOR("jay.kim@telechips.com");
MODULE_DESCRIPTION("Telechips thermal driver");
MODULE_LICENSE("GPL");
