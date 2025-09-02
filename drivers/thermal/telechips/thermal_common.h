/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TELECHIPS_THERMAL_H
#define TELECHIPS_THERMAL_H


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
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/cpu_cooling.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>

#define MAX_PROBE_NUM		(11)

#define TYPE_ONE_POINT_TRIMMING 0
#define TYPE_TWO_POINT_TRIMMING 1
#define TYPE_NONE               2

#define DEBUG

#define TCC805X_MAIN_PROBE              (0xD0)
#define TCC805X_REMOTE_PROBE0           (0xD4)
#define TCC805X_REMOTE_PROBE1           (0xD8)
#define TCC805X_REMOTE_PROBE2           (0xDC)
#define TCC805X_REMOTE_PROBE3           (0xE0)
#define TCC805X_GPU_PROBE               (0xE4)
#define TCC805X_THRESHOLD_UP            (0x50U)
#define TCC805X_THRESHOLD_DOWN          (0x54U)
#define TCC805X_T_EN_REG                (0x0U)
#define TCC805X_CONTROL_REG             (0x4U)
#define TCC805X_PROBE_SEL               (0xCU)
#define TCC805X_TB                      (0x10U)
#define TCC805X_TV                      (0x14U)
#define TCC805X_TBC                     (0x18U)
#define TCC805X_BSS                     (0x1CU)
#define TCC805X_BVS                     (0x20U)
#define TCC805X_IRQ_INTV                (0x8U)
#define TCC805X_INT_EN                  (0x24U)
#define TCC805X_INT_MASK                (0x2CU)
#define TCC805X_INT_STATUS              (0x34U)
#define TCC805X_INT_CLEAR               (0x44U)
#define TCC805X_T_TIME_INTV             (0x154U)
#define TCC805X_CA53_IRQ_EN             (0x6UL)
#define TCC805X_CA72_IRQ_EN             (0x60UL)
#define TCC805X_DEFAULT_INTERVAL        (262143)
#define TCC805X_DEF_TEMP_CODE_VAL	(956)
#define TCC805X_DEF_TEMP_RESOLUTION_VAL	(625)

#define TCC807X_MAIN_PROBE              (0xD0)
#define TCC807X_REMOTE_PROBE0           (0xD4)
#define TCC807X_REMOTE_PROBE1           (0xD8)
#define TCC807X_REMOTE_PROBE2           (0xDC)
#define TCC807X_REMOTE_PROBE3           (0xE0)
#define TCC807X_REMOTE_PROBE4           (0xE4)
#define TCC807X_REMOTE_PROBE5           (0xE8)
#define TCC807X_REMOTE_PROBE6           (0xEC)
#define TCC807X_REMOTE_PROBE7           (0xF0)
#define TCC807X_REMOTE_PROBE8           (0xF4)
#define TCC807X_REMOTE_PROBE9           (0xF8)
#define TCC807X_THRESHOLD_UP            (0x50U)
#define TCC807X_THRESHOLD_DOWN          (0x54U)
#define TCC807X_T_EN_REG                (0x0U)
#define TCC807X_CONTROL_REG             (0x4U)
#define TCC807X_PROBE_SEL               (0x8U)
#define TCC807X_THERMAL_BIT_REG		(0xCU)
#define TCC807X_BRT			(0x10U)
#define TCC807X_VT			(0x14U)
#define TCC807X_VIT			(0x18U)
#define TCC807X_BSS			(0x1CU)
#define TCC807X_BVS			(0x20U)
#define TCC807X_CIT			(0x24U)

#define TCC807X_IRQ_INTV                (0x8U)
#define TCC807X_INT_EN0                 (0x28U)
#define TCC807X_INT_EN1                 (0x2CU)
#define TCC807X_INT_MASK0               (0x30U)
#define TCC807X_INT_MASK1               (0x34U)
#define TCC807X_INT_STATUS0             (0x38U)
#define TCC807X_INT_STATUS1             (0x3CU)
#define TCC807X_INT_CLEAR0              (0x48U)
#define TCC807X_INT_CLEAR1              (0x4CU)
#define TCC807X_T_TIME_INTV             (0x150U)
#define TCC807X_MAIN_IRQ_EN             (0x6UL)
#define TCC807X_PROBE0_IRQ_EN           (0x60UL)
#define TCC807X_DEFAULT_INTERVAL        (262143)
#define TCC807X_DEF_TEMP_CODE_VAL	(60)
#define TCC807X_DEF_TEMP_RESOLUTION_VAL	(10000)

#define TCC750X_MAIN_PROBE              (0x70)
#define TCC750X_REMOTE_PROBE0           (0x74)
#define TCC750X_REMOTE_PROBE1           (0x78)
#define TCC750X_REMOTE_PROBE2           (0x7C)
#define TCC750X_THRESHOLD_UP            (0x50U)
#define TCC750X_THRESHOLD_DOWN          (0x54U)
#define TCC750X_T_EN_REG                (0x0U)
#define TCC750X_CONTROL_REG             (0x4U)
#define TCC750X_PROBE_SEL               (0xCU)
#define TCC750X_TB                      (0x10U)
#define TCC750X_TV                      (0x14U)
#define TCC750X_TBC                     (0x18U)
#define TCC750X_BSS                     (0x1CU)
#define TCC750X_BVS                     (0x20U)
#define TCC750X_IRQ_INTV                (0x8U)
#define TCC750X_INT_EN                  (0x24U)
#define TCC750X_INT_MASK                (0x2CU)
#define TCC750X_INT_STATUS              (0x34U)
#define TCC750X_INT_CLEAR               (0x44U)
#define TCC750X_T_TIME_INTV             (0x94U)
#define TCC750X_CA53_IRQ_EN             (0x6UL)
#define TCC750X_CA72_IRQ_EN             (0x60UL)
#define TCC750X_DEFAULT_INTERVAL        (262143)
#define TCC750X_DEF_TEMP_CODE_VAL	(956)
#define TCC750X_DEF_TEMP_RESOLUTION_VAL	(625)

#define TCCXXXX_MAIN_PROBE      (0x30U)
#define MAX_TEMP_CODE           (0x10010010)

#define MCELSIUS                (1000)
#define THRESHOLD_MIN_TEMP      (-40000)
#define THRESHOLD_MAX_TEMP      (125000)
#define CAPTURE_IRQ             (0x111111UL)
#define HIGH_TEMP_IRQ           (0x222222UL)
#define LOW_TEMP_IRQ            (0x444444UL)

struct tcc_thermal_data {
	struct tcc_thermal_platform_data *pdata;
	int32_t irq;
};

struct thermal_ops {
	int32_t (*init)(const struct tcc_thermal_platform_data *pdata);
	int32_t (*parse_dt)(const struct platform_device *pdev,
			struct tcc_thermal_platform_data *pdata);
	int32_t (*get_fuse_data)(const struct platform_device *pdev);
	struct thermal_zone_of_device_ops *t_ops;
};

struct tsens_reg_v1 {
	int32_t buf_slope_sel_ts;
	int32_t buf_vref_sel_ts;
	int32_t trim_bgr_ts;
	int32_t trim_vlsb_ts;
	int32_t trim_bjt_cur_ts;
};

struct tsens_reg_v2 {
	int32_t bjt_emitter_current;
	int32_t bgr_control;
	int32_t vref_trimming_value;
	int32_t vbe_current_trimming;
	int32_t slope_setting;
	int32_t reference_voltage;
	int32_t comp_current_trim;
};

struct tcc_thermal_platform_data {
	char name[16];
	struct device *dev;
	bool init;
	struct thermal_zone_device *tz;
	struct resource *res;
	struct mutex lock;
	void __iomem *base;
	const struct thermal_ops *ops;
	struct thermal_zone_params *tcc_gov;
	int32_t core;
	int32_t threshold_low_temp;
	int32_t threshold_high_temp;
	int32_t resolution;
	uint32_t interval_time;
	int32_t probe_num;
	int32_t temp_trim1[MAX_PROBE_NUM];
	int32_t temp_trim2[MAX_PROBE_NUM];
	int32_t ts_test_info_high;
	int32_t ts_test_info_low;
	int32_t d_otp_slope;
	int32_t calib_sel;
	int32_t temp_code_val;
	struct tsens_reg_v1 *tem1461;
	struct tsens_reg_v2 *tem0804;
};

#if defined (CONFIG_TELECHIPS_GPU_THERMAL_SUPPORT)
extern const struct tcc_thermal_data tcc805x_gpu_data;
#endif

#if defined (CONFIG_TELECHIPS_TEM1459)
// for leagcy t-sensor
int32_t tccxxxx_get_temp(void *tz, int32_t *temp);
int32_t tccxxxx_get_trend(void *tz, int32_t trip, enum thermal_trend *trend);
#else
int32_t tcc_get_temp(void *tz, int32_t *temp);
int32_t tcc_get_trend(void *tz, int32_t trip, enum thermal_trend *trend);
int32_t tcc_set_emul_temp(void *tz, int32_t temp);
int32_t tcc_code_to_temp(const struct tcc_thermal_platform_data *pdata,
		int32_t temp_trim1, int32_t temp_trim2, int32_t temp_code);
int32_t tcc_temp_to_code(const struct tcc_thermal_platform_data *pdata,
		int32_t temp_trim1, int32_t temp_trim2, int32_t temp);
#endif
extern struct tcc_thermal_data tcc_tsens_data;
#endif
