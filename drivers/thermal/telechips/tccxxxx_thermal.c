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
#include <linux/interrupt.h>
#include <linux/cpu_cooling.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include "thermal_common.h"
#include "../thermal_core.h"

#if defined (CONFIG_ARCH_TCC897X)
#define TCC_ECID_CON_REG		(0x74200290U)
#else
#define TCC_ECID_CON_REG		(0x14200290U)
#endif
#define TCC_E_USR_REG0		(0x8U)
#define TCC_E_USR_REG1		(0xCU)
#define SLOPE_SEL		(0xCU)
#define VREF_SEL		(0x10U)

static int32_t tccxxxx_thermal_init(const struct tcc_thermal_platform_data *data)
{
	uint32_t v_temp = 0;

	v_temp = readl_relaxed(data->base);

	v_temp = (v_temp | 0x3U);

	writel(v_temp, data->base);

	if (data->tem1461->buf_vref_sel_ts > 0) {
		v_temp = (uint32_t)data->tem1461->buf_vref_sel_ts;
		writel(v_temp, data->base + VREF_SEL);
	}

	if (data->tem1461->buf_slope_sel_ts > 0) {
		v_temp = (uint32_t)data->tem1461->buf_slope_sel_ts;
		writel(v_temp, data->base + SLOPE_SEL);
	}

	return 0;
}

static int32_t tccxxxx_get_efuse(const struct platform_device *pdev)
{
	struct tcc_thermal_platform_data *data = platform_get_drvdata(pdev);
	uint32_t reg_temp = 0;
	uint32_t ecid_reg1_temp;
	uint32_t ecid_reg0_temp;
	uint32_t i;
	void __iomem *ecid_conf;

	ecid_conf = ioremap(TCC_ECID_CON_REG, 0x10);
	// Read ECID - USER0
	reg_temp = (reg_temp | (1UL << 31));
	writel(reg_temp, ecid_conf); // enable
	reg_temp = (reg_temp | (1UL << 30));
	writel(reg_temp, ecid_conf); // CS:1, Sel : 0

	for (i = 0 ; i < 8U; i++) {
		reg_temp = (reg_temp & ~(0x3FUL << 17));
		writel(reg_temp, ecid_conf);
		reg_temp = (reg_temp | (i << 17));
		writel(reg_temp, ecid_conf);
		reg_temp = (reg_temp | (0x1UL << 23));
		writel(reg_temp, ecid_conf);
		reg_temp = (reg_temp | (0x1UL << 27));
		writel(reg_temp, ecid_conf);
		reg_temp = (reg_temp | (0x1UL << 29));
		writel(reg_temp, ecid_conf);
		reg_temp = (reg_temp & ~(0x1UL << 23));
		writel(reg_temp, ecid_conf);
		reg_temp = (reg_temp & ~(0x1UL << 27));
		writel(reg_temp, ecid_conf);
		reg_temp = (reg_temp & ~(0x1UL << 29));
		writel(reg_temp, ecid_conf);
	}

	reg_temp = (reg_temp & ~(1UL << 30));
	writel(reg_temp, ecid_conf);
	data->temp_trim1[0] =
		(int32_t)(uint16_t)((readl(ecid_conf + TCC_E_USR_REG1) &
			0x0000FF00UL) >> 8);
#if defined(CONFIG_ARCH_TCC803X) && defined(CONFIG_ARCH_TCC899X)
	data->tem1461->buf_slope_sel_ts =
		(int32_t)(uint16_t)((readl(ecid_conf + TCC_E_USR_REG1) &
			0x000000F0UL) >> 4);
	data->tem1461->buf_vref_sel_ts =
		(int32_t)(uint16_t)(((readl(ecid_conf + TCC_E_USR_REG1) &
		  0x0000000FUL) << 1UL) |
		((readl(ecid_conf + TCC_E_USR_REG0) &
		  0x80000000UL) >> 31UL));
#endif
	ecid_reg1_temp = (readl(ecid_conf + TCC_E_USR_REG1));
	ecid_reg0_temp = (readl(ecid_conf + TCC_E_USR_REG0));
	reg_temp = (reg_temp & ~(0x1UL << 31));
	writel(reg_temp, ecid_conf);
	(void)pr_info("%s.--ecid register read --\n", __func__);
	(void)pr_info("%s.ecid reg1 : %08x\n", __func__, ecid_reg1_temp);
	(void)pr_info("%s.ecid reg0 : %08x\n", __func__, ecid_reg0_temp);
	(void)pr_info("%s.data->temp_trim1 : %08x\n", __func__,
			data->temp_trim1[0]);
	(void)pr_info("%s.data->buf_vref_sel_ts1 : %08x\n", __func__,
			data->tem1461->buf_vref_sel_ts);
	(void)pr_info("%s.data->slope_trim1 : %08x\n", __func__,
			data->tem1461->buf_slope_sel_ts);

	// ~Read ECID - USER0
	(void)pr_info("%s. cal_type: %d\n", __func__, data->calib_sel);

	iounmap(ecid_conf);

	return 0;
}

static int32_t tccxxxx_parse_dt(const struct platform_device *pdev,
		struct tcc_thermal_platform_data *pdata)
{
	struct device_node *np;
	const char *tmp_str;
	int32_t ret = 0;

	if (pdev->dev.of_node != NULL) {
		np = pdev->dev.of_node;
	} else {
		(void)pr_err(
				"[ERROR][TSENSOR]%s: failed to get device node\n",
				__func__);
		ret = -ENODEV;
		goto retval;
	}

	/*tcc803x, tcc802x, tcc897x, tcc899x has only one porbe for t-sensor*/
        pdata->core = 0;

	ret = of_property_read_string(np, "cal_type", &tmp_str);
	if (ret != 0) {
		(void)pr_err(
				"[ERROR][TSENSOR]%s:failed to get cal_type from dt\n",
				__func__);
	}

	ret = strncmp(tmp_str, "TYPE_ONE_POINT_TRIMMING", strnlen(tmp_str, 30));
	if (ret == 0) {
		pdata->calib_sel = TYPE_ONE_POINT_TRIMMING;
	} else {
		ret = strncmp(tmp_str, "TYPE_TWO_POINT_TRIMMING",
				strnlen(tmp_str, 30));
		if (ret == 0) {
			/**/
			pdata->calib_sel = TYPE_TWO_POINT_TRIMMING;
		} else {
			/**/
			pdata->calib_sel = TYPE_NONE;
		}
	}

	ret = of_property_read_s32(np, "threshold_temp",
			&pdata->threshold_high_temp);
	if (ret != 0) {
		(void)pr_err("%s:failed to get threshold_temp\n", __func__);
		pdata->threshold_high_temp = THRESHOLD_MAX_TEMP;
	}
retval:
	return ret;
}

static struct tsens_reg_v1 tsens_reg;

static struct thermal_zone_of_device_ops tem_dev_ops = {
	.get_temp = tccxxxx_get_temp,
	.get_trend = tccxxxx_get_trend,
};

static const struct thermal_ops tcc_tsens_ops = {
	.init			= tccxxxx_thermal_init,
	.parse_dt		= tccxxxx_parse_dt,
	.get_fuse_data		= tccxxxx_get_efuse,
	.t_ops			= &tem_dev_ops,
};

static struct tcc_thermal_platform_data tcc_tsens_pdata = {
	.name		= "tcc_tsens",
	.ops		= &tcc_tsens_ops,
	.tem1461	= &tsens_reg,
};

struct tcc_thermal_data tcc_tsens_data = {
	.irq		= 40,
	.pdata		= &tcc_tsens_pdata,
};

MODULE_AUTHOR("jay.kim@telechips.com");
MODULE_DESCRIPTION("Telechips thermal driver");
MODULE_LICENSE("GPL");
