/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TSC_SERDES_H
#define TSC_SERDES_H

#include <linux/regmap.h>

#define SER_DES_REG_LEN		2
#define SER_DES_DATA_LEN	1

#define DP0_SER_ADDR		0xC0    /* 0xC0 >> 1 = 0x60 */
#define DP0_DES_ADDR		0x90    /* 0x90 >> 1 = 0x48 */
#define DP1_DES_ADDR		0x94    /* 0x94 >> 1 = 0x4A */
#define DP2_DES_ADDR		0x98    /* 0x98 >> 1 = 0x4C */
#define DP3_DES_ADDR		0xD0    /* 0xD0 >> 1 = 0x68 */

#define HDMI_SER_ADDR		0x44
#define HDMI_DES_ADDR		0xD4

#define SER_DES_W_LEN		3 /* byte */
#define DISPLAY_MAX_NUM 	5
#define DES_MAX_NUM		(DISPLAY_MAX_NUM)

#define TCC803XPE_EVB		(u32)(BIT(0U))
#define TCC8059_EVB		(u32)(BIT(1U))
#define TCC8050_53_EVB		(u32)(BIT(2U))
#define TCC8070_EVB		(u32)(BIT(3U))

#define TCC803X_EVB		((u32)BIT(16U) | (TCC803XPE_EVB))
#define TCC805X_EVB		((u32)BIT(18U) | (TCC8050_53_EVB) | (TCC8059_EVB))
#define TCC807X_EVB		((u32)BIT(19U) | (TCC8070_EVB))

/* serializer device revision */
#define REV_ALL			0x0
#define REV_ES4			0x3

struct serdes_info {
	struct i2c_client *client;
	struct regmap *tsc_regmap;
	unsigned int board_type;
	unsigned int max_disp;
	unsigned int act_disp;
	unsigned int lcd_touch_addr;
	struct device_link *dp_link;
};

struct i2c_data {
	unsigned short addr;
	unsigned short reg;
	unsigned int val;
	unsigned int board;
	unsigned int disp_idx;
	unsigned int revision;
};

int tcc_tsc_serdes_update(
	struct i2c_client *client,
	struct regmap *i2c_regmap,
	unsigned int disp_num,
	unsigned int board_type);

#endif /* TSC_SERDES_H */
