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

#define LOG_TAG				"VSRC:imx424"

#define loge(fmt, ...)			\
	pr_err("[ERROR][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(fmt, ...)			\
	pr_warn("[WARN][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(fmt, ...)			\
	pr_debug("[DEBUG][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(fmt, ...)			\
	pr_info("[INFO][%s] %s - "	fmt, LOG_TAG, __func__, ##__VA_ARGS__)

#define	IMX424_DEFAULT_FRAMERATE (30)

#define IMX424_DEFAULT_WIDTH (2560U + 16U)
#define IMX424_DEFAULT_HEIGHT (1440U + 16U + 8U)

#define IMX424_PAD_SRC (0U)
#define IMX424_PAD_NUM (1U)

static const struct v4l2_mbus_framefmt imx424_mbus_frmfmt_default = {
	.width = IMX424_DEFAULT_WIDTH,
	.height	= IMX424_DEFAULT_HEIGHT,
	.code = MEDIA_BUS_FMT_SGBRG12_1X12,
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
struct imx424 {
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

const struct reg_sequence imx424_reg_init[] = {
	/* 30 fps */
	/* system parameter */
	{0x0090, 0x16, 0},
	{0x1021, 0x3E, 0},
	{0x1022, 0x0C, 0},
	{0x1040, 0xB0, 0},
	{0x1041, 0x77, 0},
	{0x1042, 0x0A, 0},
	{0x1044, 0x20, 0},
	{0x1045, 0xFA, 0},
	{0x1046, 0x05, 0},
	{0x1047, 0x01, 0},
	{0x1048, 0xD0, 0},
	{0x1049, 0x39, 0},
	{0x104A, 0xB8, 0},
	{0x104B, 0x5F, 0},
	{0x2005, 0x04, 0},
	{0x2006, 0x00, 0},
	{0x2008, 0x04, 0},
	{0x2009, 0x00, 0},
	{0x200A, 0x04, 0},
	{0x200B, 0x00, 0},
	{0x200C, 0x04, 0},
	{0x200D, 0x00, 0},
	{0x200E, 0x04, 0},
	{0x200F, 0x00, 0},
	{0x2010, 0x04, 0},
	{0x2011, 0x00, 0},
	{0x201D, 0xFF, 0},
	{0x201E, 0x1F, 0},
	{0x2020, 0xFF, 0},
	{0x2021, 0x1F, 0},
	{0x2022, 0xFF, 0},
	{0x2023, 0x1F, 0},
	{0x2024, 0xFF, 0},
	{0x2025, 0x1F, 0},
	{0x202B, 0x01, 0},
	{0x2047, 0x01, 0},
	{0x2056, 0x0B, 0},
	{0x2057, 0x0B, 0},
	{0x207A, 0x09, 0},
	{0x207C, 0x00, 0},
	{0x207D, 0x00, 0},
	{0x2080, 0x03, 0},
	{0x2087, 0xB9, 0},
	{0x2088, 0xDC, 0},
	{0x2089, 0x00, 0},
	{0x208A, 0x1F, 0},
	{0x20F9, 0x08, 0},
	{0x2108, 0x00, 0},
	{0x2109, 0x00, 0},
	{0x224A, 0x01, 0},
	{0x224C, 0x14, 0},
	{0x224D, 0x00, 0},
	{0x2254, 0x42, 0},
	{0x2255, 0x00, 0},
	{0x2258, 0x40, 0},
	{0x2259, 0x00, 0},
	{0x225E, 0x3E, 0},
	{0x225F, 0x00, 0},
	{0x2265, 0x00, 0},
	{0x2266, 0x08, 0},
	{0x2268, 0xFB, 0},
	{0x2269, 0x0B, 0},
	{0x226A, 0x77, 0},
	{0x226B, 0x0D, 0},
	{0x226C, 0x00, 0},
	{0x226E, 0x01, 0},
	{0x2270, 0x03, 0},
	{0x2290, 0x7E, 0},
	{0x2291, 0x0F, 0},
	{0x2292, 0x01, 0},
	{0x2294, 0x1E, 0},
	{0x2295, 0x1B, 0},
	{0x2299, 0x08, 0},
	{0x229A, 0x08, 0},
	{0x229B, 0x08, 0},
	{0x229C, 0x01, 0},
	{0x229D, 0x03, 0},
	{0x229E, 0x07, 0},
	{0x229F, 0x0A, 0},
	{0x22A0, 0x0C, 0},
	{0x22A1, 0x0E, 0},
	{0x22A2, 0x10, 0},
	{0x22EC, 0x0E, 0},
	{0x22ED, 0x01, 0},
	{0x2330, 0x48, 0},
	{0x2331, 0x00, 0},
	{0x2342, 0x48, 0},
	{0x2343, 0x00, 0},
	{0x2348, 0x4D, 0},
	{0x2349, 0x00, 0},
	{0x246F, 0x06, 0},
	{0x2470, 0x09, 0},
	{0x2471, 0x05, 0},
	{0x2475, 0x0A, 0},
	{0x2477, 0x05, 0},
	{0x249A, 0x04, 0},
	{0x249C, 0x05, 0},
	{0x24A0, 0x00, 0},
	{0x24A2, 0x00, 0},
	{0x24A6, 0x0C, 0},
	{0x24B0, 0x03, 0},
	{0x24B1, 0x03, 0},
	{0x24B4, 0x07, 0},
	{0x24B5, 0x07, 0},
	{0x3002, 0x03, 0},
	{0x3023, 0x0A, 0},
	{0x3024, 0xBF, 0},
	{0x3025, 0x07, 0},
	{0x3026, 0x87, 0},
	{0x3028, 0xBF, 0},
	{0x3029, 0x0B, 0},
	{0x302A, 0xAF, 0},
	{0x302C, 0x07, 0},
	{0x302D, 0x0D, 0},
	{0x3032, 0x0A, 0},
	{0x3034, 0xCC, 0},
	{0x3035, 0x07, 0},
	{0x3036, 0x87, 0},
	{0x3038, 0xBB, 0},
	{0x3039, 0x0B, 0},
	{0x303A, 0xAF, 0},
	{0x303C, 0xFD, 0},
	{0x303D, 0x0C, 0},
	{0x3044, 0xF5, 0},
	{0x3045, 0x03, 0},
	{0x3046, 0x7E, 0},
	{0x3047, 0x02, 0},
	{0x3048, 0x27, 0},
	{0x3049, 0x0C, 0},
	{0x304C, 0x4B, 0},
	{0x304D, 0x14, 0},
	{0x304E, 0xA0, 0},
	{0x304F, 0x00, 0},
	{0x3050, 0x68, 0},
	{0x3051, 0x16, 0},
	{0x3052, 0xFE, 0},
	{0x3053, 0x0F, 0},
	{0x3054, 0x42, 0},
	{0x3055, 0x04, 0},
	{0x3056, 0xE6, 0},
	{0x3057, 0x01, 0},
	{0x3058, 0x68, 0},
	{0x3059, 0x16, 0},
	{0x305A, 0xFA, 0},
	{0x305B, 0x0A, 0},
	{0x30CF, 0x00, 0},
	{0x405C, 0xAF, 0},
	{0x405D, 0x0A, 0},
	{0x406D, 0x2C, 0},
	{0x6203, 0x30, 0},
	{0x6204, 0x00, 0},
	{0x6603, 0x30, 0},
	{0x6604, 0x00, 0},
	{0x7004, 0x02, 0},
	{0x7007, 0x55, 0},
	{0x7008, 0xFF, 0},
	{0x7009, 0x0F, 0},
	{0x1004, 0x8C, 0},
	{0x1005, 0x0A, 0},
	{0x1006, 0x00, 0},
	/* WNIWV: Number of crop vertical lines [lines] + 12 */
	{0x1010, 0xBC, 0},
	{0x1011, 0x05, 0},
	/* WINPV: Vertical crop position [lines] */
	{0x1012, 0xF0, 0},
	{0x1013, 0x00, 0},
	/* CROPWV: Number of crop vertical lines [lines] +8 */
	{0x101A, 0xB8, 0},
	{0x101B, 0x05, 0},
	/* CROPPH: Horizontal crop position [pixels] */
	{0x101C, 0x78, 0},
	{0x101D, 0x02, 0},
	/* CROPWH: Output horizontal width [pixels] */
	{0x101E, 0x10, 0},
	{0x101F, 0x0A, 0},
	{0x104D, 0xA2, 0},
	{0x104E, 0x00, 0},
	{0x32E8, 0x0A, 0},
	{0x32E9, 0x78, 0},
	{0x32EA, 0x0A, 0},
	{0x32EB, 0x00, 0},
	{0x803A, 0x01, 0},
	{0x803B, 0x01, 0},
	{0x6202, 0x00, 0},
	{0x6203, 0x30, 0},
	{0x6602, 0x00, 0},
	{0x6603, 0x30, 0},
	{0x1050, 0xA5, 0},
	{0x1051, 0x00, 0},
	{0x102C, 0x5F, 0},
	{0x102D, 0x00, 0},
	{0x102E, 0x1F, 0},
	{0x102F, 0x00, 0},
	{0x1030, 0x1F, 0},
	{0x1031, 0x00, 0},
	{0x1032, 0x8F, 0},
	{0x1033, 0x00, 0},
	{0x1034, 0x27, 0},
	{0x1035, 0x00, 0},
	{0x1036, 0x3F, 0},
	{0x1037, 0x00, 0},
	{0x1038, 0x27, 0},
	{0x1039, 0x00, 0},
	{0x103A, 0x37, 0},
	{0x103B, 0x00, 0},
	{0x103C, 0x1F, 0},
	{0x103D, 0x00, 0},

	/* IQ parameter */
	{0x0014, 0x00, 0},
	{0x0015, 0x07, 0},
	{0x0016, 0x00, 0},
	{0x0018, 0x70, 0},
	{0x0019, 0x00, 0},
	{0x001A, 0x00, 0},
	{0x0022, 0x00, 0},
	{0x0023, 0x00, 0},
	{0x0024, 0x00, 0},
	{0x0025, 0x00, 0},
	{0x8010, 0x00, 0},
	{0x8011, 0x05, 0},
	{0x8012, 0x00, 0},
	{0x8013, 0x07, 0},
	{0x8014, 0x00, 0},
	{0x8015, 0x0A, 0},
	{0x8016, 0x00, 0},
	{0x8017, 0x0E, 0},
	{0x8018, 0x00, 0},
	{0x8019, 0x05, 0},
	{0x801A, 0x00, 0},
	{0x801B, 0x07, 0},
	{0x801C, 0x00, 0},
	{0x801D, 0x0F, 0},
	{0x801E, 0x00, 0},
	{0x801F, 0x15, 0},
	{0x8020, 0x00, 0},
	{0x8021, 0x05, 0},
	{0x8022, 0x00, 0},
	{0x8023, 0x07, 0},
	{0x8024, 0x00, 0},
	{0x8025, 0x0A, 0},
	{0x8026, 0x00, 0},
	{0x8027, 0x0E, 0},
	{0x8028, 0x00, 0},
	{0x8029, 0x05, 0},
	{0x802A, 0x00, 0},
	{0x802B, 0x07, 0},
	{0x802C, 0x00, 0},
	{0x802D, 0x0F, 0},
	{0x802E, 0x00, 0},
	{0x802F, 0x15, 0},
	{0x803D, 0x00, 0},
	{0x803E, 0x00, 0},

	/* PWL */
        /* 220607 BTREE setting*/
	{0x8040, 0x00, 0},
	{0x8041, 0x08, 0},
	{0x8042, 0x00, 0},
	{0x8043, 0x00, 0},
	{0x8044, 0x00, 0},
	{0x8045, 0x08, 0},
	{0x8046, 0x00, 0},
	{0x8047, 0x00, 0},
	{0x8048, 0x00, 0},
	{0x8049, 0x00, 0},
	{0x804A, 0x04, 0},
	{0x804B, 0x00, 0},
	{0x804C, 0xF8, 0},
	{0x804D, 0x0A, 0},
	{0x804E, 0x00, 0},
	{0x804F, 0x00, 0},
	{0x8050, 0x00, 0},
	{0x8051, 0x00, 0},
	{0x8052, 0x08, 0},
	{0x8053, 0x00, 0},
	{0x8054, 0xF8, 0},
	{0x8055, 0x0C, 0},
	{0x8056, 0x00, 0},
	{0x8057, 0x00, 0},
	{0x8058, 0xFF, 0},
	{0x8059, 0xFF, 0},
	{0x805A, 0x0F, 0},
	{0x805B, 0x00, 0},
	{0x805C, 0xF8, 0},
	{0x805D, 0x0E, 0},
	{0x805E, 0x00, 0},
	{0x805F, 0x00, 0},
	{0x8060, 0xFF, 0},
	{0x8061, 0xFF, 0},
	{0x8062, 0x0F, 0},
	{0x8063, 0x00, 0},
	{0x8064, 0xF8, 0},
	{0x8065, 0x0E, 0},
	{0x8066, 0x00, 0},
	{0x8067, 0x00, 0},
	{0x8068, 0xFF, 0},
	{0x8069, 0xFF, 0},
	{0x806A, 0x0F, 0},
	{0x806B, 0x00, 0},
	{0x806C, 0xF8, 0},
	{0x806D, 0x0E, 0},
	{0x806E, 0x00, 0},
	{0x806F, 0x00, 0},
	{0x8070, 0xFF, 0},
	{0x8071, 0xFF, 0},
	{0x8072, 0x0F, 0},
	{0x8073, 0x00, 0},
	{0x8074, 0xF8, 0},
	{0x8075, 0x0E, 0},
	{0x8076, 0x00, 0},
	{0x8077, 0x00, 0},
	{0x8078, 0xFF, 0},
	{0x8079, 0xFF, 0},
	{0x807A, 0x0F, 0},
	{0x807B, 0x00, 0},
	{0x807C, 0xF8, 0},
	{0x807D, 0x0E, 0},
	{0x807E, 0x00, 0},
	{0x807F, 0x00, 0},
	{0x8080, 0xFF, 0},
	{0x8081, 0xFF, 0},
	{0x8082, 0x0F, 0},
	{0x8083, 0x00, 0},
	{0x8084, 0xF8, 0},
	{0x8085, 0x0E, 0},
	{0x8086, 0x00, 0},
	{0x8087, 0x00, 0},
	{0x8088, 0xFF, 0},
	{0x8089, 0xFF, 0},
	{0x808A, 0x0F, 0},
	{0x808B, 0x00, 0},
	{0x808C, 0xF8, 0},
	{0x808D, 0x0E, 0},
	{0x808E, 0x00, 0},
	{0x808F, 0x00, 0},
	{0x8090, 0xFF, 0},
	{0x8091, 0xFF, 0},
	{0x8092, 0x0F, 0},
	{0x8093, 0x00, 0},
	{0x8094, 0xF8, 0},
	{0x8095, 0x0E, 0},
	{0x8096, 0x00, 0},
	{0x8097, 0x00, 0},
	{0x8098, 0xFF, 0},
	{0x8099, 0xFF, 0},
	{0x809A, 0x0F, 0},
	{0x809B, 0x00, 0},
	{0x809C, 0xF8, 0},
	{0x809D, 0x0E, 0},
	{0x809E, 0x00, 0},
	{0x809F, 0x00, 0},
	{0x80A0, 0xFF, 0},
	{0x80A1, 0xFF, 0},
	{0x80A2, 0x0F, 0},
	{0x80A3, 0x00, 0},
	{0x80A4, 0xF8, 0},
	{0x80A5, 0x0E, 0},
	{0x80A6, 0x00, 0},
	{0x80A7, 0x00, 0},
	{0x80A8, 0xFF, 0},
	{0x80A9, 0xFF, 0},
	{0x80AA, 0x0F, 0},
	{0x80AB, 0x00, 0},
	{0x80AC, 0xF8, 0},
	{0x80AD, 0x0E, 0},
	{0x80AE, 0x00, 0},
	{0x80AF, 0x00, 0},
	{0x80B0, 0xFF, 0},
	{0x80B1, 0xFF, 0},
	{0x80B2, 0x0F, 0},
	{0x80B3, 0x00, 0},
	{0x80B4, 0xF8, 0},
	{0x80B5, 0x0E, 0},
	{0x80B6, 0x00, 0},
	{0x80B7, 0x00, 0},
	{0x80B8, 0xFF, 0},
	{0x80B9, 0xFF, 0},
	{0x80BA, 0x0F, 0},
	{0x80BB, 0x00, 0},
	{0x80BC, 0xF8, 0},
	{0x80BD, 0x0E, 0},
	{0x80BE, 0x00, 0},
	{0x80BF, 0x00, 0},
	{0x80C0, 0xFF, 0},
	{0x80C1, 0xFF, 0},
	{0x80C2, 0x0F, 0},
	{0x80C3, 0x00, 0},
	{0x80C4, 0xF8, 0},
	{0x80C5, 0x0E, 0},
	{0x80C6, 0x00, 0},
	{0x80C7, 0x00, 0},
	{0x80C8, 0xFF, 0},
	{0x80C9, 0xFF, 0},
	{0x80CA, 0x0F, 0},
	{0x80CB, 0x00, 0},
	{0x80CC, 0xF8, 0},
	{0x80CD, 0x0E, 0},
	{0x80CE, 0x00, 0},
	{0x80CF, 0x00, 0},
	{0x80D0, 0xFF, 0},
	{0x80D1, 0xFF, 0},
	{0x80D2, 0x0F, 0},
	{0x80D3, 0x00, 0},
	{0x80D4, 0xF8, 0},
	{0x80D5, 0x0E, 0},
	{0x80D6, 0x00, 0},
	{0x80D7, 0x00, 0},
	{0x80D8, 0xFF, 0},
	{0x80D9, 0xFF, 0},
	{0x80DA, 0x0F, 0},
	{0x80DB, 0x00, 0},
	{0x80DC, 0xF8, 0},
	{0x80DD, 0x0E, 0},
	{0x80DE, 0x00, 0},
	{0x80DF, 0x00, 0},
	{0x80E0, 0xFF, 0},
	{0x80E1, 0xFF, 0},
	{0x80E2, 0x0F, 0},
	{0x80E3, 0x00, 0},
	{0x80E4, 0xF8, 0},
	{0x80E5, 0x0E, 0},
	{0x80E6, 0x00, 0},
	{0x80E7, 0x00, 0},
	{0x80E8, 0xFF, 0},
	{0x80E9, 0xFF, 0},
	{0x80EA, 0x0F, 0},
	{0x80EB, 0x00, 0},
	{0x80EC, 0xF8, 0},
	{0x80ED, 0x0E, 0},
	{0x80EE, 0x00, 0},
	{0x80EF, 0x00, 0},
	{0x80F0, 0xFF, 0},
	{0x80F1, 0xFF, 0},
	{0x80F2, 0x0F, 0},
	{0x80F3, 0x00, 0},
	{0x80F4, 0xF8, 0},
	{0x80F5, 0x0E, 0},
	{0x80F6, 0x00, 0},
	{0x80F7, 0x00, 0},
	{0x80F8, 0xFF, 0},
	{0x80F9, 0xFF, 0},
	{0x80FA, 0x0F, 0},
	{0x80FB, 0x00, 0},
	{0x80FC, 0xF8, 0},
	{0x80FD, 0x0E, 0},
	{0x80FE, 0x00, 0},
	{0x80FF, 0x00, 0},
	{0x8100, 0xFF, 0},
	{0x8101, 0xFF, 0},
	{0x8102, 0x0F, 0},
	{0x8103, 0x00, 0},
	{0x8104, 0xF8, 0},
	{0x8105, 0x0E, 0},
	{0x8106, 0x00, 0},
	{0x8107, 0x00, 0},
	{0x8108, 0xFF, 0},
	{0x8109, 0xFF, 0},
	{0x810A, 0x0F, 0},
	{0x810B, 0x00, 0},
	{0x810C, 0xF8, 0},
	{0x810D, 0x0E, 0},
	{0x810E, 0x00, 0},
	{0x810F, 0x00, 0},
	{0x8110, 0xFF, 0},
	{0x8111, 0xFF, 0},
	{0x8112, 0x0F, 0},
	{0x8113, 0x00, 0},
	{0x8114, 0xF8, 0},
	{0x8115, 0x0E, 0},
	{0x8116, 0x00, 0},
	{0x8117, 0x00, 0},
	{0x8118, 0xFF, 0},
	{0x8119, 0xFF, 0},
	{0x811A, 0x0F, 0},
	{0x811B, 0x00, 0},
	{0x811C, 0xF8, 0},
	{0x811D, 0x0E, 0},
	{0x811E, 0x00, 0},
	{0x811F, 0x00, 0},
	{0x8120, 0xFF, 0},
	{0x8121, 0xFF, 0},
	{0x8122, 0x0F, 0},
	{0x8123, 0x00, 0},
	{0x8124, 0xF8, 0},
	{0x8125, 0x0E, 0},
	{0x8126, 0x00, 0},
	{0x8127, 0x00, 0},
	{0x8128, 0xFF, 0},
	{0x8129, 0xFF, 0},
	{0x812A, 0x0F, 0},
	{0x812B, 0x00, 0},
	{0x812C, 0xF8, 0},
	{0x812D, 0x0E, 0},
	{0x812E, 0x00, 0},
	{0x812F, 0x00, 0},
	{0x8130, 0xFF, 0},
	{0x8131, 0xFF, 0},
	{0x8132, 0x0F, 0},
	{0x8133, 0x00, 0},
	{0x8134, 0xF8, 0},
	{0x8135, 0x0E, 0},
	{0x8136, 0x00, 0},
	{0x8137, 0x00, 0},
	{0x8138, 0xFF, 0},
	{0x8139, 0xFF, 0},
	{0x813A, 0x0F, 0},
	{0x813B, 0x00, 0},
	{0x813C, 0xF8, 0},
	{0x813D, 0x0E, 0},
	{0x813E, 0x00, 0},
	{0x813F, 0x00, 0},
	{0x8140, 0xFF, 0},
	{0x8141, 0xFF, 0},
	{0x8142, 0x0F, 0},
	{0x8143, 0x00, 0},
	{0x8144, 0xF8, 0},
	{0x8145, 0x0E, 0},
	{0x8146, 0x00, 0},
	{0x8147, 0x00, 0},
	{0x8148, 0x00, 0},
	{0x8149, 0x00, 0},
	{0x814A, 0xEF, 0},
	{0x814B, 0x03, 0},
	{0x814C, 0xDD, 0},
	{0x814D, 0x07, 0},
	{0x814E, 0xD8, 0},
	{0x814F, 0x0E, 0},
	{0x8150, 0x40, 0},
	{0x8151, 0x1F, 0},
	{0x8152, 0x6C, 0},
	{0x8153, 0x07, 0},
	{0x8154, 0xA0, 0},
	{0x8155, 0x0F, 0},
	{0x8156, 0x44, 0},
	{0x8157, 0x16, 0},
	{0x8158, 0xE0, 0},
	{0x8159, 0x2E, 0},
	{0x815A, 0x53, 0},
	{0x815B, 0x04, 0},
	{0x815C, 0xDD, 0},
	{0x815D, 0x07, 0},
	{0x815E, 0x30, 0},
	{0x815F, 0x11, 0},
	{0x8160, 0x40, 0},
	{0x8161, 0x1F, 0},
	{0x8162, 0x6C, 0},
	{0x8163, 0x07, 0},
	{0x8164, 0xA0, 0},
	{0x8165, 0x0F, 0},
	{0x8166, 0x44, 0},
	{0x8167, 0x16, 0},
	{0x8168, 0xE0, 0},
	{0x8169, 0x2E, 0},
	{0x8182, 0xFE, 0},
	{0x8183, 0x1F, 0},
	{0x818A, 0xFE, 0},
	{0x818B, 0x1F, 0},
	{0x8192, 0xFE, 0},
	{0x8193, 0x1F, 0},
	{0x8198, 0x08, 0},
	{0x8199, 0x07, 0},
	{0x819A, 0x10, 0},
	{0x819B, 0x0E, 0},
	{0x819C, 0x08, 0},
	{0x819D, 0x07, 0},
	{0x819E, 0x18, 0},
	{0x819F, 0x15, 0},
	{0x81A0, 0x6C, 0},
	{0x81A1, 0x07, 0},
	{0x81A2, 0xD8, 0},
	{0x81A3, 0x0E, 0},
	{0x81A4, 0x6C, 0},
	{0x81A5, 0x07, 0},
	{0x81A6, 0x44, 0},
	{0x81A7, 0x16, 0},
	{0x81A8, 0x98, 0},
	{0x81A9, 0x08, 0},
	{0x81AA, 0xA0, 0},
	{0x81AB, 0x0F, 0},
	{0x81AC, 0x30, 0},
	{0x81AD, 0x11, 0},
	{0x81AE, 0x40, 0},
	{0x81AF, 0x1F, 0},
	{0x81B0, 0x98, 0},
	{0x81B1, 0x08, 0},
	{0x81B2, 0xA0, 0},
	{0x81B3, 0x0F, 0},
	{0x81B4, 0xC8, 0},
	{0x81B5, 0x19, 0},
	{0x81B6, 0xE0, 0},
	{0x81B7, 0x2E, 0},
	{0x81B8, 0xFC, 0},
	{0x81B9, 0x08, 0},
	{0x81BA, 0xA0, 0},
	{0x81BB, 0x0F, 0},
	{0x81BC, 0xF8, 0},
	{0x81BD, 0x11, 0},
	{0x81BE, 0x40, 0},
	{0x81BF, 0x1F, 0},
	{0x81C0, 0xFC, 0},
	{0x81C1, 0x08, 0},
	{0x81C2, 0xA0, 0},
	{0x81C3, 0x0F, 0},
	{0x81C4, 0xF4, 0},
	{0x81C5, 0x1A, 0},
	{0x81C6, 0xE0, 0},
	{0x81C7, 0x2E, 0},
	{0x81C8, 0x00, 0},
	{0x81C9, 0x06, 0},
	{0x81CA, 0x00, 0},
	{0x81CB, 0x08, 0},
	{0x81CC, 0x00, 0},
	{0x81CD, 0x0C, 0},
	{0x81CE, 0x00, 0},
	{0x81CF, 0x10, 0},
	{0x81D0, 0x00, 0},
	{0x81D1, 0x06, 0},
	{0x81D2, 0x00, 0},
	{0x81D3, 0x08, 0},
	{0x81D4, 0x00, 0},
	{0x81D5, 0x12, 0},
	{0x81D6, 0x00, 0},
	{0x81D7, 0x18, 0},
	{0x002E, 0xD8, 0},
	{0x002F, 0x01, 0},
	{0x0030, 0x10, 0},
	{0x0031, 0x02, 0},
	{0x0034, 0x80, 0},
	{0x8000, 0x00, 0},
	{0x8001, 0x04, 0},
	{0x8002, 0x00, 0},
	{0x8003, 0x04, 0},
	{0x803C, 0x01, 0},	//HDR_BLD_MDET_ON (LED 광원내 블랙 개선)
	{0x7005, 0x00, 0},
	{0x3191, 0x00, 0},	//DGCLPTRCKHOLD (저조도 깜빡임 개선)
	{0x3194, 0x01, 0},	//DGCLPMTHD
	{0x3195, 0x00, 0},	//DGCLP_LSFOLTH
	{0x3199, 0x00, 0},	//DGCLPSTRCKNUM
	{0x31B8, 0x00, 0},	//DGCLPS2FTH
	{0x31B9, 0x09, 0},	//DGCLPSTRCKCTRL
	{0x31BA, 0x0A, 0},	//DGCLPASYMPSET

	/* streaming */
	{0x0000, 0x00, 0},
};

static const struct regmap_config imx424_regmap = {
	.reg_bits		= 16,
	.val_bits		= 8,

	.max_register		= 0xFFFF,
	.cache_type		= REGCACHE_NONE,
};

static struct frame_size imx424_framesizes[] = {
	{	IMX424_DEFAULT_WIDTH,	IMX424_DEFAULT_HEIGHT	},
};

static u32 imx424_framerates[] = {
	IMX424_DEFAULT_FRAMERATE,
};

/*
 * v4l2_ctrl_ops implementations
 */
static int imx424_s_ctrl(struct v4l2_ctrl *ctrl)
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

static int imx424_parse_device_tree(struct imx424 *dev, struct device_node *node)
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
static inline struct imx424 *to_dev(struct v4l2_subdev *sd)
{
	return container_of(sd, struct imx424, sd);
}

/*
 * v4l2_subdev_core_ops implementations
 */
static int imx424_init(struct v4l2_subdev *sd, u32 enable)
{
	struct imx424		*dev	= to_dev(sd);
	int			ret	= 0;

	mutex_lock(&dev->lock);

	if ((dev->i_cnt == 0) && (enable == 1)) {
		/* enable imx424 */
		ret = regmap_multi_reg_write(dev->regmap,
				imx424_reg_init,
				ARRAY_SIZE(imx424_reg_init));
		if (ret) {
			/* err status */
			loge("regmap_multi_reg_write returned %d\n", ret);
		}
	} else if ((dev->i_cnt == 1) && (enable == 0)) {
		/* disable imx424 */
	}

	if (enable)
		dev->i_cnt++;
	else
		dev->i_cnt--;

	mutex_unlock(&dev->lock);

	return ret;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int32_t imx424_g_register(struct v4l2_subdev *sd,
				 struct v4l2_dbg_register *reg)
{
	const struct imx424 *dev = to_dev(sd);
	uint32_t val = 0U;
	int32_t ret = 0;

	ret = regmap_read(dev->regmap, reg->reg, &val);
	if (ret < 0) {
		/* error */
		loge("imx424_core_get_reg returned %d\n", ret);
	} else {
		/* okay */
		reg->val = val;
	}

	return ret;
}

static int32_t imx424_s_register(struct v4l2_subdev *sd,
				 const struct v4l2_dbg_register *reg)
{
	const struct imx424 *dev = to_dev(sd);
	int32_t ret = 0;

	if (ret >= 0) {
		ret = regmap_write(dev->regmap, reg->reg, reg->val);
		if (ret < 0) {
			/* error */
			loge("imx424_core_set_reg returned %d\n", ret);
		}
	}

	return ret;
}
#endif

/*
 * v4l2_subdev_video_ops implementations
 */
static int imx424_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct imx424 *dev = to_dev(sd);
	int ret = 0;

	mutex_lock(&dev->lock);

	mutex_unlock(&dev->lock);

	return ret;
}

static int imx424_g_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct imx424		*dev	= NULL;

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

static int imx424_s_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_frame_interval *interval)
{
	struct imx424		*dev	= NULL;

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
static int32_t imx424_init_cfg(struct v4l2_subdev *sd,
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

static int imx424_enum_frame_size(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_size_enum *fse)
{
	struct frame_size	*size		= NULL;

	if (ARRAY_SIZE(imx424_framesizes) <= fse->index) {
		logd("index(%u) is wrong\n", fse->index);
		return -EINVAL;
	}

	size = &imx424_framesizes[fse->index];
	logd("size: %u * %u\n", size->width, size->height);

	fse->min_width = fse->max_width = size->width;
	fse->min_height	= fse->max_height = size->height;
	logd("max size: %u * %u\n", fse->max_width, fse->max_height);

	return 0;
}

static int imx424_enum_frame_interval(struct v4l2_subdev *sd,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_frame_interval_enum *fie)
{
	if (ARRAY_SIZE(imx424_framerates) <= fie->index) {
		logd("index(%u) is wrong\n", fie->index);
		return -EINVAL;
	}

	fie->interval.numerator = 1;
	fie->interval.denominator = imx424_framerates[fie->index];
	logd("framerate: %u / %u\n",
		fie->interval.numerator, fie->interval.denominator);

	return 0;
}

static int imx424_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *f)
{
	struct imx424 *dev = to_dev(sd);
	int ret = 0;

	mutex_lock(&dev->lock);

	if (f->pad >= IMX424_PAD_NUM) {
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

static int imx424_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *f)
{
	struct imx424 *dev = to_dev(sd);
	struct v4l2_mbus_framefmt *fmt = NULL;
	int ret = 0;

	mutex_lock(&dev->lock);

	/* check pad */
	if (f->pad >= IMX424_PAD_NUM) {
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
static int imx424_registered(struct v4l2_subdev *sd)
{
	int ret = 0;

	logd("registered with v4l2 dev\n");

	return ret;
}

/*
 * v4l2_subdev_ops implementations
 */
static const struct v4l2_ctrl_ops imx424_ctrl_ops = {
	.s_ctrl			= imx424_s_ctrl,
};

static const struct v4l2_subdev_core_ops imx424_core_ops = {
	.init			= imx424_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register		= imx424_g_register,
	.s_register		= imx424_s_register,
#endif
};

static const struct v4l2_subdev_video_ops imx424_video_ops = {
	.s_stream		= imx424_s_stream,
	.g_frame_interval	= imx424_g_frame_interval,
	.s_frame_interval	= imx424_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops imx424_pad_ops = {
	.init_cfg		= imx424_init_cfg,
	.enum_frame_size	= imx424_enum_frame_size,
	.enum_frame_interval	= imx424_enum_frame_interval,
	.get_fmt		= imx424_get_fmt,
	.set_fmt		= imx424_set_fmt,
};

static const struct v4l2_subdev_ops imx424_ops = {
	.core			= &imx424_core_ops,
	.video			= &imx424_video_ops,
	.pad			= &imx424_pad_ops,
};

static const struct v4l2_subdev_internal_ops imx424_internal_ops = {
	.registered = imx424_registered,
};

struct imx424 imx424_data = {
};

static const struct i2c_device_id imx424_id[] = {
	{ "imx424", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, imx424_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id imx424_of_match[] = {
	{
		.compatible	= "tcc-sony,imx424",
		.data		= &imx424_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, imx424_of_match);
#endif

int imx424_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct imx424			*dev	= NULL;
	const struct of_device_id	*dev_id	= NULL;
	int				ret	= 0;

	/* allocate and clear memory for a device */
	dev = devm_kzalloc(&client->dev, sizeof(struct imx424), GFP_KERNEL);
	if (dev == NULL) {
		loge("Allocate a device struct.\n");
		return -ENOMEM;
	}

	/* set the specific information */
	if (client->dev.of_node) {
		dev_id = of_match_node(imx424_of_match, client->dev.of_node);
		memcpy(dev, (const void *)dev_id->data, sizeof(*dev));
	}

	logd("name: %s, addr: 0x%x, client: 0x%p\n",
		client->name, (client->addr)<<1, client);

	mutex_init(&dev->lock);

	/* parse device tree */
	ret = imx424_parse_device_tree(dev, client->dev.of_node);
	if (ret < 0) {
		loge("cxd5700_parse_device_tree, ret: %d\n", ret);
		return ret;
	}

	/* regitster v4l2 control handlers */
	v4l2_ctrl_handler_init(&dev->hdl, 2);
	v4l2_ctrl_new_std(&dev->hdl, &imx424_ctrl_ops,
		V4L2_CID_BRIGHTNESS, 0, 255, 1, 128);
	v4l2_ctrl_new_std_menu(&dev->hdl,
		&imx424_ctrl_ops,
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
	v4l2_i2c_subdev_init(&dev->sd, client, &imx424_ops);
	dev->sd.internal_ops = &imx424_internal_ops;
	dev->sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->sd.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	dev->sd.entity.flags = MEDIA_ENT_FL_DEFAULT;

	if (ret >= 0) {
		/* init pads */
		dev->pad.index = 0U;
		dev->pad.flags = MEDIA_PAD_FL_SOURCE;
		dev->fmt = imx424_mbus_frmfmt_default;
		media_entity_pads_init(&dev->sd.entity, IMX424_PAD_NUM,
				       &dev->pad);
	}

	/* add async subdevs and register notifier */
	ret = v4l2_async_register_subdev_sensor_common(&dev->sd);
	if (ret < 0) {
		loge("v4l2_async_register_subdev returned %d\n", ret);
	}

	/* init framerate */
	dev->framerate = IMX424_DEFAULT_FRAMERATE;

	/* init regmap */
	dev->regmap = devm_regmap_init_i2c(client, &imx424_regmap);
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

int imx424_remove(struct i2c_client *client)
{
	struct v4l2_subdev	*sd	= i2c_get_clientdata(client);
	struct imx424		*dev	= to_dev(sd);

	/* release regmap */
	regmap_exit(dev->regmap);

	v4l2_ctrl_handler_free(&dev->hdl);

	v4l2_async_unregister_subdev(sd);

	kfree(dev);
	client = NULL;

	return 0;
}

static struct i2c_driver imx424_driver = {
	.probe		= imx424_probe,
	.remove		= imx424_remove,
	.driver		= {
		.name		= "imx424",
		.of_match_table	= of_match_ptr(imx424_of_match),
	},
	.id_table	= imx424_id,
};

module_i2c_driver(imx424_driver);
MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips IMX424 Driver");
MODULE_LICENSE("GPL");
