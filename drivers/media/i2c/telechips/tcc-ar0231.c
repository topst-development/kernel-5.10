// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */  

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of_graph.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <media/v4l2-async.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <linux/kdev_t.h>
#include <linux/of_gpio.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>

#define LOG_TAG				"VSRC:AR0231"

#define loge(fmt, ...) \
	pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(fmt, ...) \
	pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(fmt, ...) \
	pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(fmt, ...) \
	pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)

#define	AR0231_DEFAULT_FRAMERATE (30)

#if 1
#define AR0231_DEFAULT_WIDTH (1920U + 8U)
#define AR0231_DEFAULT_HEIGHT (1080U + 16U)
#else
#define AR0231_DEFAULT_WIDTH (1280U + 16U)
#define AR0231_DEFAULT_HEIGHT (720U + 16U)
#endif
#define AR0231_PAD_SRC (0U)
#define AR0231_PAD_NUM (1U)

static const struct v4l2_mbus_framefmt ar0231_mbus_frmfmt_default = {
	.width = AR0231_DEFAULT_WIDTH,
	.height	= AR0231_DEFAULT_HEIGHT,
	.code = MEDIA_BUS_FMT_SGRBG12_1X12,
	.field = V4L2_FIELD_NONE,
};

struct frame_size {
	u32 width;
	u32 height;
};

/*
 * This object contains essential v4l2 objects
 * such as sub-device and ctrl_handler
 */
struct ar0231 {
	struct i2c_client		*clinet;
	struct v4l2_subdev		sd;
	struct v4l2_ctrl_handler	hdl;

	struct media_pad		pad;
	struct v4l2_mbus_framefmt	fmt;
	int				framerate;

	/* Regmaps */
	struct regmap			*regmap;

	struct mutex lock;
	unsigned int p_cnt;
	unsigned int s_cnt;
	unsigned int i_cnt;
};

const struct reg_sequence ar0231_reg_init[] = {
#if 0
	/* [AR0231AT_720p_P12_3exp_M27_20220707] */

	{ 0x301A, 0x10D8, 700 * 1000 }, // RESET_REGISTER

	{ 0x3092, 0x0C24, 0 }, // ROW_NOISE_CONTROL
	{ 0x337A, 0x0C80, 0 }, // DBLC_SCALE0
	{ 0x3520, 0x1288, 0 }, // DAC_LD_32_33
	{ 0x3522, 0x880C, 0 }, // DAC_LD_34_35
	{ 0x3524, 0x0C12, 0 }, // DAC_LD_36_37
	{ 0x352C, 0x1212, 0 }, // DAC_LD_44_45
	{ 0x354A, 0x007F, 0 }, // DAC_LD_74_75
	{ 0x350C, 0x055C, 0 }, // DAC_LD_12_13
	{ 0x3506, 0x3333, 0 }, // DAC_LD_6_7
	{ 0x3508, 0x3333, 0 }, // DAC_LD_8_9
	{ 0x3100, 0x4000, 0 }, // DLO_CONTROL0
	{ 0x3280, 0x0FA0, 0 }, // T1_BARRIER_C0
	{ 0x3282, 0x0FA0, 0 }, // T1_BARRIER_C1
	{ 0x3284, 0x0FA0, 0 }, // T1_BARRIER_C2
	{ 0x3286, 0x0FA0, 0 }, // T1_BARRIER_C3
	{ 0x3288, 0x0FA0, 0 }, // T2_BARRIER_C0
	{ 0x328A, 0x0FA0, 0 }, // T2_BARRIER_C1
	{ 0x328C, 0x0FA0, 0 }, // T2_BARRIER_C2
	{ 0x328E, 0x0FA0, 0 }, // T2_BARRIER_C3
	{ 0x3290, 0x0FA0, 0 }, // T3_BARRIER_C0
	{ 0x3292, 0x0FA0, 0 }, // T3_BARRIER_C1
	{ 0x3294, 0x0FA0, 0 }, // T3_BARRIER_C2
	{ 0x3296, 0x0FA0, 0 }, // T3_BARRIER_C3
	{ 0x3298, 0x0FA0, 0 }, // T4_BARRIER_C0
	{ 0x329A, 0x0FA0, 0 }, // T4_BARRIER_C1
	{ 0x329C, 0x0FA0, 0 }, // T4_BARRIER_C2
	{ 0x329E, 0x0FA0, 0 }, // T4_BARRIER_C3
	{ 0x301A, 0x10D8, 200 * 1000 }, // RESET_REGISTER

	{ 0x2512, 0x8000, 0 }, // SEQ_CTRL_PORT
	{ 0x2510, 0x0905, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x3350, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x2004, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1460, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1578, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0901, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x7B24, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xFF24, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xFF24, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xEA24, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1022, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x2410, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x155A, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0901, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1400, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x24FF, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x24FF, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x24EA, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x2324, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x647A, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x2404, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x052C, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x400A, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xFF0A, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xFF0A, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1008, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x3851, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1440, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0004, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0801, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0408, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1180, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x2652, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1518, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0906, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1348, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1002, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1016, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1181, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1189, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1056, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1210, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0901, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0D09, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1413, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x8809, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x2B15, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x8809, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0311, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xD909, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1214, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x4109, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0312, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1409, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0110, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xD612, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1012, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1212, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1011, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xDD11, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xD910, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x5609, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1511, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xDB09, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1511, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x9B09, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0F11, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xBB12, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1A12, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1014, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x6012, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x5010, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x7610, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xE609, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0812, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x4012, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x6009, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x290B, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0904, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1440, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0923, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x15C8, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x13C8, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x092C, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1588, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1388, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0C09, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0C14, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x4109, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1112, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x6212, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x6011, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xBF11, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xBB10, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x6611, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xFB09, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x3511, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xBB12, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x6312, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x6014, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0015, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0011, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xB812, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xA012, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0010, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x2610, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0013, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0011, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0008, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x3053, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x4215, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x4013, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x4010, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0210, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1611, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x8111, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x8910, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x5612, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1009, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x010D, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0815, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xC015, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xD013, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x5009, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1313, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xD009, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0215, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xC015, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xC813, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xC009, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0515, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x8813, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x8009, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0213, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x8809, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0411, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xC909, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0814, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0109, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0B11, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0xD908, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1400, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x091A, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1440, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0903, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1214, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0901, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x10D6, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1210, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1212, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1210, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x11DD, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x11D9, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1056, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0917, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x11DB, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0913, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x11FB, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0905, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x11BB, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x121A, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1210, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1460, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1250, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1076, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x10E6, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0901, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x15A8, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0901, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x13A8, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1240, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1260, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0925, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x13AD, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0902, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0907, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1588, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0901, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x138D, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0B09, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0914, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x4009, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0B13, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x8809, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1C0C, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0920, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1262, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1260, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x11BF, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x11BB, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1066, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x090A, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x11FB, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x093B, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x11BB, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1263, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1260, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1400, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1508, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x11B8, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x12A0, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1200, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1026, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1000, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1300, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x1100, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x437A, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0609, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0B05, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0708, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x4137, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x502C, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x2CFE, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x15FE, 0 }, // SEQ_DATA_PORT
	{ 0x2510, 0x0C2C, 0 }, // SEQ_DATA_PORT
	{ 0x32E6, 0x00E0, 0 }, // MIN_SUBROW
	{ 0x1008, 0x036F, 0 }, // FINE_INTEGRATION_TIME_MIN
	{ 0x100C, 0x058F, 0 }, // FINE_INTEGRATION_TIME2_MIN
	{ 0x100E, 0x07AF, 0 }, // FINE_INTEGRATION_TIME3_MIN
	{ 0x1010, 0x014F, 0 }, // FINE_INTEGRATION_TIME4_MIN
	{ 0x3230, 0x0312, 0 }, // FINE_CORRECTION
	{ 0x3232, 0x0532, 0 }, // FINE_CORRECTION2
	{ 0x3234, 0x0752, 0 }, // FINE_CORRECTION3
	{ 0x3236, 0x00F2, 0 }, // FINE_CORRECTION4
	{ 0x3566, 0x3328, 0 }, // DAC_LD_102_103
	{ 0x32D0, 0x3A02, 0 }, // SHUT_RST
	{ 0x32D2, 0x3508, 0 }, // SHUT_TX
	{ 0x32D4, 0x3702, 0 }, // SHUT_DCG
	{ 0x32D6, 0x3C04, 0 }, // SHUT_RST_BOOST
	{ 0x32DC, 0x370A, 0 }, // SHUT_TX_BOOST
	{ 0x30B0, 0x0800, 0 }, // DIGITAL_TEST
	{ 0x302A, 0x0008, 0 }, // VT_PIX_CLK_DIV
	{ 0x302C, 0x0001, 0 }, // VT_SYS_CLK_DIV
	{ 0x302E, 0x0009, 0 }, //PRE_PLL_CLK_DIV = 9
	{ 0x3030, 0x00E8, 0 }, //PLL_MULTIPLIER = 232
	{ 0x3036, 0x0008, 0 }, // OP_WORD_CLK_DIV
	{ 0x3038, 0x0001, 0 }, // OP_SYS_CLK_DIV
	{ 0x30B0, 0x0800, 0 }, // DIGITAL_TEST
	{ 0x30A2, 0x0001, 0 }, // X_ODD_INC_
	{ 0x30A6, 0x0001, 0 }, // Y_ODD_INC_
	{ 0x3040, 0x0000, 0 }, // READ_MODE
	{ 0x3040, 0x0000, 0 }, // READ_MODE
	{ 0x3082, 0x0008, 0 }, // OPERATION_MODE_CTRL
	{ 0x3082, 0x0008, 0 }, // OPERATION_MODE_CTRL
	{ 0x3082, 0x0008, 0 }, // OPERATION_MODE_CTRL
	{ 0x3082, 0x0008, 0 }, // OPERATION_MODE_CTRL
	{ 0x30BA, 0x11F2, 0 }, // DIGITAL_CTRL
	{ 0x30BA, 0x11F2, 0 }, // DIGITAL_CTRL
	{ 0x30BA, 0x11F2, 0 }, // DIGITAL_CTRL
	{ 0x3044, 0x0400, 0 }, // DARK_CONTROL
	{ 0x3044, 0x0400, 0 }, // DARK_CONTROL
	{ 0x3044, 0x0400, 0 }, // DARK_CONTROL
	{ 0x3044, 0x0400, 0 }, // DARK_CONTROL
	{ 0x3064, 0x1802, 0 }, // SMIA_TEST
	{ 0x3064, 0x1802, 0 }, // SMIA_TEST
	{ 0x3064, 0x1802, 0 }, // SMIA_TEST
	{ 0x3064, 0x1802, 0 }, // SMIA_TEST
	{ 0x33E0, 0x0C80, 0 }, // TEST_ASIL_ROWS
	{ 0x33E0, 0x0C80, 0 }, // TEST_ASIL_ROWS
	{ 0x3180, 0x0080, 0 }, // DELTA_DK_CONTROL
	{ 0x33E4, 0x0080, 0 }, // VERT_SHADING_CONTROL
	{ 0x33E0, 0x0C80, 0 }, // TEST_ASIL_ROWS
	{ 0x33E0, 0x0C80, 0 }, // TEST_ASIL_ROWS
	{ 0x3004, 0x013C, 0 }, // X_ADDR_START_
	{ 0x3008, 0x064B, 0 }, // X_ADDR_END_
	{ 0x3002, 0x00EC, 0 }, // Y_ADDR_START_
	{ 0x3006, 0x03CB, 0 }, // Y_ADDR_END_
	{ 0x3032, 0x0000, 0 }, // SCALING_MODE
	{ 0x3400, 0x0010, 0 }, // SCALE_M
	{ 0x3402, 0x0788, 0 }, // X_OUTPUT_CONTROL
	{ 0x3402, 0x0A20, 0 }, // X_OUTPUT_CONTROL
	{ 0x3404, 0x04B8, 0 }, // Y_OUTPUT_CONTROL
	{ 0x3404, 0x05C0, 0 }, // Y_OUTPUT_CONTROL
	{ 0x3082, 0x0008, 0 }, // OPERATION_MODE_CTRL
	{ 0x30BA, 0x11F2, 100 * 1000 }, // DIGITAL_CTRL

	{ 0x300C, 0x07BA, 0 }, // LINE_LENGTH_PCK_
	{ 0x300A, 0x05BA, 0 }, //FRAME_LENGTH_LINES = 1466
	{ 0x3042, 0x0000, 0 }, // EXTRA_DELAY
	{ 0x3238, 0x0222, 0 }, // EXPOSURE_RATIO
	{ 0x3238, 0x0222, 0 }, // EXPOSURE_RATIO
	{ 0x3238, 0x0222, 0 }, // EXPOSURE_RATIO
	{ 0x3238, 0x0222, 0 }, // EXPOSURE_RATIO
	{ 0x3012, 0x0163, 0 }, // COARSE_INTEGRATION_TIME_
	{ 0x3014, 0x0882, 0 }, // FINE_INTEGRATION_TIME_
	{ 0x321E, 0x0882, 0 }, // FINE_INTEGRATION_TIME2
	{ 0x3222, 0x0882, 0 }, // FINE_INTEGRATION_TIME3
	{ 0x30B0, 0x0800, 0 }, // DIGITAL_TEST
	{ 0x32EA, 0x3C0E, 0 }, // SHUT_CTRL
	{ 0x32EA, 0x3C0E, 0 }, // SHUT_CTRL
	{ 0x32EA, 0x3C0E, 0 }, // SHUT_CTRL
	{ 0x32EC, 0x72A1, 0 }, // SHUT_CTRL2
	{ 0x32EC, 0x72A1, 0 }, // SHUT_CTRL2
	{ 0x32EC, 0x72A1, 0 }, // SHUT_CTRL2
	{ 0x32EC, 0x72A1, 0 }, // SHUT_CTRL2
	{ 0x32EC, 0x72A1, 0 }, // SHUT_CTRL2
	{ 0x32EC, 0x72A1, 0 }, // SHUT_CTRL2
	{ 0x31D0, 0x0001, 0 }, // COMPANDING
	{ 0x31AE, 0x0001, 0 }, // SERIAL_FORMAT
	{ 0x31AE, 0x0001, 0 }, // SERIAL_FORMAT
	{ 0x31AC, 0x140C, 0 }, // DATA_FORMAT_BITS
	{ 0x301A, 0x10D8, 0 }, // RESET_REGISTER
	{ 0x301A, 0x10D8, 0 }, // RESET_REGISTER
	{ 0x301A, 0x10D8, 0 }, // RESET_REGISTER
	{ 0x301A, 0x10DC, 0 }, // RESET_REGISTER
#endif
#if 1
	/* [AR0231AT_1080p_P12_3exp_M27_20220707] */
	{ 0x301A, 0x10D8, 700 * 1000 }, //RESET_REGISTER

	{ 0x3092, 0x0C24, 0 }, //ROW_NOISE_CONTROL
	{ 0x337A, 0x0C80, 0 }, //DBLC_SCALE0
	{ 0x3520, 0x1288, 0 }, //DAC_LD_32_33
	{ 0x3522, 0x880C, 0 }, //DAC_LD_34_35
	{ 0x3524, 0x0C12, 0 }, //DAC_LD_36_37
	{ 0x352C, 0x1212, 0 }, //DAC_LD_44_45
	{ 0x354A, 0x007F, 0 }, //DAC_LD_74_75
	{ 0x350C, 0x055C, 0 }, //DAC_LD_12_13
	{ 0x3506, 0x3333, 0 }, //DAC_LD_6_7
	{ 0x3508, 0x3333, 0 }, //DAC_LD_8_9
	{ 0x3100, 0x4000, 0 }, //DLO_CONTROL0
	{ 0x3280, 0x0FA0, 0 }, //T1_BARRIER_C0
	{ 0x3282, 0x0FA0, 0 }, //T1_BARRIER_C1
	{ 0x3284, 0x0FA0, 0 }, //T1_BARRIER_C2
	{ 0x3286, 0x0FA0, 0 }, //T1_BARRIER_C3
	{ 0x3288, 0x0FA0, 0 }, //T2_BARRIER_C0
	{ 0x328A, 0x0FA0, 0 }, //T2_BARRIER_C1
	{ 0x328C, 0x0FA0, 0 }, //T2_BARRIER_C2
	{ 0x328E, 0x0FA0, 0 }, //T2_BARRIER_C3
	{ 0x3290, 0x0FA0, 0 }, //T3_BARRIER_C0
	{ 0x3292, 0x0FA0, 0 }, //T3_BARRIER_C1
	{ 0x3294, 0x0FA0, 0 }, //T3_BARRIER_C2
	{ 0x3296, 0x0FA0, 0 }, //T3_BARRIER_C3
	{ 0x3298, 0x0FA0, 0 }, //T4_BARRIER_C0
	{ 0x329A, 0x0FA0, 0 }, //T4_BARRIER_C1
	{ 0x329C, 0x0FA0, 0 }, //T4_BARRIER_C2
	{ 0x329E, 0x0FA0, 0 }, //T4_BARRIER_C3
	{ 0x301A, 0x10D8, 200 * 1000 }, //RESET_REGISTER

	{ 0x2512, 0x8000, 0 }, //SEQ_CTRL_PORT
	{ 0x2510, 0x0905, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x3350, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x2004, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1460, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1578, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0901, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x7B24, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xFF24, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xFF24, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xEA24, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1022, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x2410, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x155A, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0901, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1400, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x24FF, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x24FF, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x24EA, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x2324, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x647A, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x2404, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x052C, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x400A, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xFF0A, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xFF0A, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1008, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x3851, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1440, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0004, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0801, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0408, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1180, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x2652, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1518, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0906, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1348, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1002, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1016, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1181, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1189, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1056, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1210, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0901, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0D09, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1413, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x8809, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x2B15, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x8809, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0311, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xD909, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1214, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x4109, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0312, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1409, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0110, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xD612, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1012, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1212, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1011, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xDD11, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xD910, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x5609, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1511, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xDB09, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1511, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x9B09, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0F11, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xBB12, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1A12, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1014, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x6012, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x5010, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x7610, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xE609, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0812, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x4012, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x6009, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x290B, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0904, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1440, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0923, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x15C8, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x13C8, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x092C, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1588, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1388, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0C09, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0C14, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x4109, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1112, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x6212, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x6011, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xBF11, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xBB10, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x6611, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xFB09, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x3511, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xBB12, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x6312, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x6014, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0015, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0011, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xB812, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xA012, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0010, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x2610, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0013, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0011, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0008, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x3053, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x4215, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x4013, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x4010, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0210, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1611, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x8111, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x8910, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x5612, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1009, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x010D, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0815, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xC015, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xD013, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x5009, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1313, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xD009, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0215, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xC015, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xC813, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xC009, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0515, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x8813, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x8009, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0213, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x8809, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0411, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xC909, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0814, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0109, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0B11, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0xD908, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1400, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x091A, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1440, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0903, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1214, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0901, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x10D6, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1210, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1212, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1210, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x11DD, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x11D9, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1056, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0917, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x11DB, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0913, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x11FB, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0905, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x11BB, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x121A, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1210, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1460, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1250, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1076, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x10E6, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0901, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x15A8, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0901, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x13A8, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1240, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1260, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0925, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x13AD, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0902, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0907, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1588, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0901, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x138D, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0B09, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0914, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x4009, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0B13, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x8809, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1C0C, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0920, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1262, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1260, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x11BF, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x11BB, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1066, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x090A, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x11FB, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x093B, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x11BB, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1263, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1260, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1400, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1508, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x11B8, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x12A0, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1200, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1026, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1000, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1300, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x1100, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x437A, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0609, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0B05, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0708, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x4137, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x502C, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x2CFE, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x15FE, 0 }, //SEQ_DATA_PORT
	{ 0x2510, 0x0C2C, 0 }, //SEQ_DATA_PORT
	{ 0x32E6, 0x00E0, 0 }, //MIN_SUBROW
	{ 0x1008, 0x036F, 0 }, //FINE_INTEGRATION_TIME_MIN
	{ 0x100C, 0x058F, 0 }, //FINE_INTEGRATION_TIME2_MIN
	{ 0x100E, 0x07AF, 0 }, //FINE_INTEGRATION_TIME3_MIN
	{ 0x1010, 0x014F, 0 }, //FINE_INTEGRATION_TIME4_MIN
	{ 0x3230, 0x0312, 0 }, //FINE_CORRECTION
	{ 0x3232, 0x0532, 0 }, //FINE_CORRECTION2
	{ 0x3234, 0x0752, 0 }, //FINE_CORRECTION3
	{ 0x3236, 0x00F2, 0 }, //FINE_CORRECTION4
	{ 0x3566, 0x3328, 0 }, //DAC_LD_102_103
	{ 0x32D0, 0x3A02, 0 }, //SHUT_RST
	{ 0x32D2, 0x3508, 0 }, //SHUT_TX
	{ 0x32D4, 0x3702, 0 }, //SHUT_DCG
	{ 0x32D6, 0x3C04, 0 }, //SHUT_RST_BOOST
	{ 0x32DC, 0x370A, 0 }, //SHUT_TX_BOOST
	{ 0x30B0, 0x0800, 0 }, //DIGITAL_TEST
	{ 0x302A, 0x0008, 0 }, //VT_PIX_CLK_DIV
	{ 0x302C, 0x0001, 0 }, //VT_SYS_CLK_DIV
	{ 0x302E, 0x0009, 0 }, //PRE_PLL_CLK_DIV = 9
	{ 0x3030, 0x00E8, 0 }, //PLL_MULTIPLIER = 232
	{ 0x3036, 0x0008, 0 }, //OP_WORD_CLK_DIV
	{ 0x3038, 0x0001, 0 }, //OP_SYS_CLK_DIV
	{ 0x30B0, 0x0800, 0 }, //DIGITAL_TEST
	{ 0x30A2, 0x0001, 0 }, //X_ODD_INC_
	{ 0x30A6, 0x0001, 0 }, //Y_ODD_INC_
	{ 0x3040, 0x0000, 0 }, //READ_MODE
	{ 0x3040, 0x0000, 0 }, //READ_MODE
	{ 0x3082, 0x0008, 0 }, //OPERATION_MODE_CTRL
	{ 0x3082, 0x0008, 0 }, //OPERATION_MODE_CTRL
	{ 0x3082, 0x0008, 0 }, //OPERATION_MODE_CTRL
	{ 0x3082, 0x0008, 0 }, //OPERATION_MODE_CTRL
	{ 0x30BA, 0x11F2, 0 }, //DIGITAL_CTRL
	{ 0x30BA, 0x11F2, 0 }, //DIGITAL_CTRL
	{ 0x30BA, 0x11F2, 0 }, //DIGITAL_CTRL
	{ 0x3044, 0x0400, 0 }, //DARK_CONTROL
	{ 0x3044, 0x0400, 0 }, //DARK_CONTROL
	{ 0x3044, 0x0400, 0 }, //DARK_CONTROL
	{ 0x3044, 0x0400, 0 }, //DARK_CONTROL
	{ 0x3064, 0x1802, 0 }, //SMIA_TEST
	{ 0x3064, 0x1802, 0 }, //SMIA_TEST
	{ 0x3064, 0x1802, 0 }, //SMIA_TEST
	{ 0x3064, 0x1802, 0 }, //SMIA_TEST
	{ 0x33E0, 0x0C80, 0 }, //TEST_ASIL_ROWS
	{ 0x33E0, 0x0C80, 0 }, //TEST_ASIL_ROWS
	{ 0x3180, 0x0080, 0 }, //DELTA_DK_CONTROL
	{ 0x33E4, 0x0080, 0 }, //VERT_SHADING_CONTROL
	{ 0x33E0, 0x0C80, 0 }, //TEST_ASIL_ROWS
	{ 0x33E0, 0x0C80, 0 }, //TEST_ASIL_ROWS
	{ 0x3004, 0x0000, 0 }, //X_ADDR_START_
	{ 0x3008, 0x0787, 0 }, //X_ADDR_END_
	{ 0x3002, 0x0038, 0 }, //Y_ADDR_START_
	{ 0x3006, 0x047F, 0 }, //Y_ADDR_END_
	{ 0x3032, 0x0000, 0 }, //SCALING_MODE
	{ 0x3400, 0x0010, 0 }, //SCALE_M
	{ 0x3402, 0x0788, 0 }, //X_OUTPUT_CONTROL
	{ 0x3402, 0x0F10, 0 }, //X_OUTPUT_CONTROL
	{ 0x3404, 0x04B8, 0 }, //Y_OUTPUT_CONTROL
	{ 0x3404, 0x0894, 0 }, // Y_OUTPUT_CONTROL
	{ 0x3082, 0x0008, 0 }, //OPERATION_MODE_CTRL
	{ 0x30BA, 0x11F2, 100 * 1000 }, //DIGITAL_CTRL

	{ 0x300C, 0x07BA, 0 }, //LINE_LENGTH_PCK_
	{ 0x300A, 0x05BA, 0 }, //FRAME_LENGTH_LINES = 1466
	{ 0x3042, 0x0000, 0 }, //EXTRA_DELAY
	{ 0x3238, 0x0222, 0 }, //EXPOSURE_RATIO
	{ 0x3238, 0x0222, 0 }, //EXPOSURE_RATIO
	{ 0x3238, 0x0222, 0 }, //EXPOSURE_RATIO
	{ 0x3238, 0x0222, 0 }, //EXPOSURE_RATIO
	{ 0x3012, 0x0163, 0 }, //COARSE_INTEGRATION_TIME_
	{ 0x3014, 0x0882, 0 }, //FINE_INTEGRATION_TIME_
	{ 0x321E, 0x0882, 0 }, //FINE_INTEGRATION_TIME2
	{ 0x3222, 0x0882, 0 }, //FINE_INTEGRATION_TIME3
	{ 0x30B0, 0x0800, 0 }, //DIGITAL_TEST
	{ 0x32EA, 0x3C0E, 0 }, //SHUT_CTRL
	{ 0x32EA, 0x3C0E, 0 }, //SHUT_CTRL
	{ 0x32EA, 0x3C0E, 0 }, //SHUT_CTRL
	{ 0x32EC, 0x72A1, 0 }, //SHUT_CTRL2
	{ 0x32EC, 0x72A1, 0 }, //SHUT_CTRL2
	{ 0x32EC, 0x72A1, 0 }, //SHUT_CTRL2
	{ 0x32EC, 0x72A1, 0 }, //SHUT_CTRL2
	{ 0x32EC, 0x72A1, 0 }, //SHUT_CTRL2
	{ 0x32EC, 0x72A1, 0 }, //SHUT_CTRL2
	{ 0x31D0, 0x0001, 0 }, //COMPANDING
	{ 0x31AE, 0x0001, 0 }, //SERIAL_FORMAT
	{ 0x31AE, 0x0001, 0 }, //SERIAL_FORMAT
	{ 0x31AC, 0x140C, 0 }, //DATA_FORMAT_BITS
	{ 0x301A, 0x10D8, 0 }, //RESET_REGISTER
	{ 0x301A, 0x10D8, 0 }, //RESET_REGISTER
	{ 0x301A, 0x10D8, 0 }, //RESET_REGISTER
	{ 0x301A, 0x10DC, 0 }, //RESET_REGISTER
#endif
};

static const struct regmap_config ar0231_regmap = {
	.reg_bits		= 16,
	.val_bits		= 16,

	.max_register		= 0xFFFF,
	.cache_type		= REGCACHE_NONE,
};

static struct frame_size ar0231_framesizes[] = {
	{	AR0231_DEFAULT_WIDTH,	AR0231_DEFAULT_HEIGHT	},
};

static u32 ar0231_framerates[] = {
	AR0231_DEFAULT_FRAMERATE,
};

/*
 * v4l2_ctrl_ops implementations
 */
static int ar0231_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int			ret	= 0;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
	case V4L2_CID_CONTRAST:
	case V4L2_CID_SATURATION:
	case V4L2_CID_HUE:
	case V4L2_CID_DO_WHITE_BALANCE:
	default:
		loge("V4L2_CID_BRIGHTNESS is not implemented yet.\n");
		ret = -EINVAL;
	}

	return ret;
}

static int ar0231_parse_device_tree(struct ar0231 *dev, struct device_node *node)
{
	int ret = 0;

	if (node == NULL) {
		loge("the device tree is empty\n");
		ret = -ENODEV;
	}


	return ret;
}

/*
 * Helper functions for reflection
 */
static inline struct ar0231 *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ar0231, sd);
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int ar0231_init(struct v4l2_subdev *sd, u32 enable)
{
	struct ar0231		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if ((dev->i_cnt == 0) && (enable == 1)) {
		/* enable ar0231 */
		ret = regmap_multi_reg_write(dev->regmap,
				ar0231_reg_init,
				ARRAY_SIZE(ar0231_reg_init));
		if (ret) {
			/* err status */
			loge("regmap_multi_reg_write returned %d\n", ret);
		}
	} else if ((dev->i_cnt == 1) && (enable == 0)) {
		/* disable ar0231 */
	}

	if (enable)
		dev->i_cnt++;
	else
		dev->i_cnt--;

	mutex_unlock(&dev->lock);

	return ret;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int32_t ar0231_g_register(struct v4l2_subdev *sd,
				 struct v4l2_dbg_register *reg)
{
	const struct ar0231 *dev = to_dev(sd);
	uint32_t val = 0U;
	int32_t ret = 0;

	ret = regmap_read(dev->regmap, reg->reg, &val);
	if (ret < 0) {
		/* error */
		loge("ar0231_core_get_reg returned %d\n", ret);
	} else {
		/* okay */
		reg->val = val;
	}

	return ret;
}

static int32_t ar0231_s_register(struct v4l2_subdev *sd,
				 const struct v4l2_dbg_register *reg)
{
	const struct ar0231 *dev = to_dev(sd);
	int32_t ret = 0;

	if (ret >= 0) {
		ret = regmap_write(dev->regmap, reg->reg, reg->val);
		if (ret < 0) {
			/* error */
			loge("ar0231_core_set_reg returned %d\n", ret);
		}
	}

	return ret;
}
#endif

/*
 * v4l2_subdev_video_ops implementations
 */
static int ar0231_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct ar0231 *dev = to_dev(sd);
	int ret = 0;

	mutex_lock(&dev->lock);

	mutex_unlock(&dev->lock);

	return ret;
}

static int ar0231_g_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct ar0231		*dev	= NULL;

	dev = to_dev(sd);
	if (!dev) {
		loge("Failed to get video source object by subdev\n");
		return -EINVAL;
	}

	interval->pad = 0;
	interval->interval.numerator = 1;
	interval->interval.denominator = dev->framerate;

	return 0;
}

static int ar0231_s_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct ar0231		*dev	= NULL;

	dev = to_dev(sd);
	if (!dev) {
		loge("Failed to get video source object by subdev\n");
		return -EINVAL;
	}

	/* set framerate with i2c setting if supported */

	dev->framerate = interval->interval.denominator;

	return 0;
}

/*
 * v4l2_subdev_pad_ops implementations
 */
static int32_t ar0231_init_cfg(struct v4l2_subdev *sd,
			       struct v4l2_subdev_pad_config *cfg)
{
	struct v4l2_mbus_framefmt *try;
	struct v4l2_subdev_format fmt;
	unsigned int pad;
	int32_t ret = 0;

	for (pad = 0; pad < sd->entity.num_pads; pad++) {
		memset(&fmt, 0, sizeof(fmt));

		fmt.pad = pad;
		fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		ret = v4l2_subdev_call(sd, pad, get_fmt, NULL, &fmt);
		if (ret < 0) {
			loge("get_fmt returned %d\n", ret);
			break;
		}

		try = v4l2_subdev_get_try_format(sd, cfg, pad);
		*try = fmt.format;
	}

	return ret;
}

static int ar0231_enum_frame_size(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_size_enum *fse)
{
	struct frame_size	*size		= NULL;

	if (ARRAY_SIZE(ar0231_framesizes) <= fse->index) {
		logd("index(%u) is wrong\n", fse->index);
		return -EINVAL;
	}

	size = &ar0231_framesizes[fse->index];
	logd("size: %u * %u\n", size->width, size->height);

	fse->min_width = fse->max_width = size->width;
	fse->min_height	= fse->max_height = size->height;
	logd("max size: %u * %u\n", fse->max_width, fse->max_height);

	return 0;
}

static int ar0231_enum_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_interval_enum *fie)
{
	if (ARRAY_SIZE(ar0231_framerates) <= fie->index) {
		logd("index(%u) is wrong\n", fie->index);
		return -EINVAL;
	}

	fie->interval.numerator = 1;
	fie->interval.denominator = ar0231_framerates[fie->index];
	logd("framerate: %u / %u\n",
		fie->interval.numerator, fie->interval.denominator);

	return 0;
}

static int ar0231_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *f)
{
	struct ar0231 *dev = to_dev(sd);
	int ret = 0;

	mutex_lock(&dev->lock);

	if (f->pad >= AR0231_PAD_NUM) {
		/* error */
		loge("invalid pad num(%d)\n", f->pad);
		ret = -EINVAL;
	} else {
		if (f->which == V4L2_SUBDEV_FORMAT_TRY) {
			/* get try format */
			f->format =
				*v4l2_subdev_get_try_format(sd, cfg, f->pad);
		} else {
			/* get active format */
			f->format = dev->fmt;
		}
	}

	mutex_unlock(&dev->lock);

	return ret;
}

static int ar0231_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *f)
{
	struct ar0231 *dev = to_dev(sd);
	struct v4l2_mbus_framefmt *fmt = NULL;
	int ret = 0;

	mutex_lock(&dev->lock);

	/* check pad */
	if (f->pad >= AR0231_PAD_NUM) {
		/* error */
		loge("invalid pad num(%d)\n", f->pad);
		ret = -EINVAL;
	}

	/* get try or active mbus framefmt pointer */
	if (ret >= 0) {
		if (f->which == V4L2_SUBDEV_FORMAT_TRY) {
			/* get try format */
			fmt = v4l2_subdev_get_try_format(sd, cfg, f->pad);
		} else {
			/* get active format */
			fmt = &dev->fmt;
		}
	}

	*fmt = f->format;

	mutex_unlock(&dev->lock);

	return ret;
}

/*
 * v4l2_subdev_internal_ops implementations
 */
static int ar0231_registered(struct v4l2_subdev *sd)
{
	int ret = 0;

	logd("registered with v4l2 dev\n");

	return ret;
}

/*
 * v4l2_subdev_ops implementations
 */
static const struct v4l2_ctrl_ops ar0231_ctrl_ops = {
	.s_ctrl			= ar0231_s_ctrl,
};

static const struct v4l2_subdev_core_ops ar0231_core_ops = {
	.init			= ar0231_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register		= ar0231_g_register,
	.s_register		= ar0231_s_register,
#endif
};

static const struct v4l2_subdev_video_ops ar0231_video_ops = {
	.s_stream		= ar0231_s_stream,
	.g_frame_interval	= ar0231_g_frame_interval,
	.s_frame_interval	= ar0231_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops ar0231_pad_ops = {
	.init_cfg		= ar0231_init_cfg,
	.enum_frame_size	= ar0231_enum_frame_size,
	.enum_frame_interval	= ar0231_enum_frame_interval,
	.get_fmt		= ar0231_get_fmt,
	.set_fmt		= ar0231_set_fmt,
};

static const struct v4l2_subdev_ops ar0231_ops = {
	.core			= &ar0231_core_ops,
	.video			= &ar0231_video_ops,
	.pad			= &ar0231_pad_ops,
};

static const struct v4l2_subdev_internal_ops ar0231_internal_ops = {
	.registered = ar0231_registered,
};

struct ar0231 ar0231_data = {
};

static const struct i2c_device_id ar0231_id[] = {
	{ "ar0231", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ar0231_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id ar0231_of_match[] = {
	{
		.compatible	= "tcc-onnn,ar0231",
		.data		= &ar0231_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, ar0231_of_match);
#endif

int ar0231_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct ar0231			*dev	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct ar0231), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(ar0231_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	mutex_init(&dev->lock);

	/* parse device tree */
	ret = ar0231_parse_device_tree(dev, client->dev.of_node);
	if (ret < 0) {
		loge("cxd5700_parse_device_tree, ret: %d\n", ret);
		return ret;
	}

	/* regitster v4l2 control handlers */
	v4l2_ctrl_handler_init(&dev->hdl, 2);
	v4l2_ctrl_new_std(&dev->hdl, &ar0231_ctrl_ops,
		V4L2_CID_BRIGHTNESS, 0, 255, 1, 128);
	v4l2_ctrl_new_std_menu(&dev->hdl,
		&ar0231_ctrl_ops,
		V4L2_CID_DV_RX_IT_CONTENT_TYPE,
		V4L2_DV_IT_CONTENT_TYPE_NO_ITC,
		0,
		V4L2_DV_IT_CONTENT_TYPE_NO_ITC);
	dev->sd.ctrl_handler = &dev->hdl;
	if (dev->hdl.error) {
		loge("v4l2_ctrl_handler_init is wrong\n");
		ret = dev->hdl.error;
		goto goto_free_device_data;
	}

	/* Register with V4L2 layer as a slave device */
	v4l2_i2c_subdev_init(&dev->sd, client, &ar0231_ops);
	dev->sd.internal_ops = &ar0231_internal_ops;
	dev->sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	dev->sd.entity.flags = MEDIA_ENT_FL_DEFAULT;

	if (ret >= 0) {
		/* init pads */
		dev->pad.index = 0U;
		dev->pad.flags = MEDIA_PAD_FL_SOURCE;
		dev->fmt = ar0231_mbus_frmfmt_default;
		media_entity_pads_init(&dev->sd.entity, AR0231_PAD_NUM,
				       &dev->pad);
	}

	/* add async subdevs and register notifier */
	ret = v4l2_async_register_subdev_sensor_common(&dev->sd);
	if (ret < 0) {
		loge("v4l2_async_register_subdev returned %d\n", ret);
	}

	/* init framerate */
	dev->framerate = AR0231_DEFAULT_FRAMERATE;

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &ar0231_regmap);
	if (IS_ERR(dev->regmap)) {
		loge("devm_regmap_init_i2c is wrong\n");
		ret = -1;
		goto goto_free_device_data;
	}

	goto goto_end;

goto_free_device_data:
	/* free the videosource data */
	kfree(dev);

goto_end:
	return ret;
}

int ar0231_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct ar0231		*dev	= to_dev(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	v4l2_ctrl_handler_free(&dev->hdl);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver ar0231_driver = {
	.probe		= ar0231_probe,
	.remove		= ar0231_remove,
	.driver		= {
		.name		= "ar0231",
		.of_match_table	= of_match_ptr(ar0231_of_match),
	},
	.id_table	= ar0231_id,
};

module_i2c_driver(ar0231_driver);
MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips AR0231 Driver");
MODULE_LICENSE("GPL");
