// SPDX-License-Identifier: GPL-2.0-or-later OR MIT
/*
* Copyright (C) Telechips Inc.
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>

#include <linux/list.h>

#include "dptx_v14.h"
#include "dptx_drm_dp_addition.h"
#include "dptx_reg.h"
#include "dptx_dbg.h"

#define DRIVER_DATE	"20221110"
#define DRIVER_MAJOR	3
#define DRIVER_MINOR	0
#define DRIVER_PATCH	0

#define SER_DES_I2C_REG_ADD_LEN			2
#define SER_DES_I2C_DATA_LEN			1

#define DP0_PANEL_SER_I2C_DEV_ADD		0xC0	/* 0xC0 >> 1 = 0x60 */
#define DP0_PANEL_DES_I2C_DEV_ADD		0x90	/* 0x90 >> 1 = 0x48 */
#define DP1_PANEL_DES_I2C_DEV_ADD		0x94	/* 0x94 >> 1 = 0x4A */
#define DP2_PANEL_DES_I2C_DEV_ADD		0x98	/* 0x98 >> 1 = 0x4C */
#define DP3_PANEL_DES_I2C_DEV_ADD		0xD0	/* 0xD0 >> 1 = 0x68 */
#define DP3_PANEL_DES_SHIFT_I2C_DEV_ADD		0x68	/* 0xD0 >> 1 = 0x68 */

#define	SER_DEV_REV						0x000E
#define SER_REV_ES2						0x01
#define SER_REV_ES4						0x03
#define SER_REV_ALL						0x0F

#define	SER_MISC_CONFIG_B1				0x7019
#define MST_FUNCTION_DISABLE		    0x00
#define MST_FUNCTION_ENABLE				0x01

#define	SER_LANE_REMAP_B0				0x7030
#define	SER_LANE_REMAP_B1				0x7031

#define	DES_DEV_REV						0x000E
#define DES_REV_ES2						0x01
#define DES_REV_ES3						0x02
#define DES_STREAM_SELECT				0x00A0
#define DES_DROP_VIDEO					0x0307

#define	DES_VIDEO_RX8					0x0108
#define DES_VID_LOCK					0x40
#define DES_VID_PKT_DET					0x20

#define DP_SER_DES_DELAY_DEV_ADDR			0xEFFF
#define DP_SER_DES_INVALID_REG_ADDR			0xFFFF


#define CHECK_DEV_ADDR(x) (((x) <= (uint16_t)DP3_PANEL_DES_SHIFT_I2C_DEV_ADD) ? true:false)


struct Max968xx_dev {
	uint8_t ucser_rev;
	uint8_t ucdes_rev;
	uint8_t ucevb_type;
	uint8_t ucpower_type;
	uint8_t ucevb_power_type;
	struct i2c_client *client;
	struct list_head list;
};
/* [FP] misra_c_2012_rule_8_4_violation*/
LIST_HEAD(max968xx_list);

struct Max968xx_Reg_Data {
	unsigned int	uidev_addr;
	unsigned int	uireg_addr;
	unsigned int	uireg_val;
	unsigned char	ucpower_type;
	unsigned char	ucser_rev;
};


static struct Max968xx_Reg_Data pstDP_Panel_VIC_1027_DesES3_RegVal[] = {
	{0xD0, 0x0010, 0x80, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x98, 0x0010, 0x80, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x94, 0x0010, 0x80, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0010, 0x80, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0010, 0x80, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
	10, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

/* AGC CR Init 8G1 */
	{0xC0, 0x60AA, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x61AA, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x62AA, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x63AA, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
/* BST CR Init 8G1 */
	{0xC0, 0x60B6, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x61B6, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x62B6, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x63B6, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
/* AGC CR Init 5G4 */
	{0xC0, 0x60A9, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x61A9, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x62A9, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x63A9, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
/* BST CR Init 5G4 */
	{0xC0, 0x60B5, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x61B5, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x62B5, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x63B5, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
/* AGC CR Init 2G7 */
	{0xC0, 0x60A8, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x61A8, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x62A8, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x63A8, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
/* BST CR Init 2G7 */
	{0xC0, 0x60B4, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x61B4, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x62B4, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x63B4, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
/* Set 8G1 Error Channel Phase */
	{0xC0, 0x6070, 0xA5, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x6071, 0x65, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x6170, 0xA5, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x6171, 0x65, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x6270, 0xA5, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x6271, 0x65, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x6370, 0xA5, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x6371, 0x65, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},

/***** MST Setting *****/
	{0xC0, 0x6420, 0x10, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7A14, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7000, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, SER_MISC_CONFIG_B1, MST_FUNCTION_ENABLE,
	TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
	100, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0xC0, 0x70A0, 0x04, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7074, 0x1E, TCC_EVB_LCD_POW_MAX, SER_REV_ES4},
	{0xC0, 0x7074, 0x0A, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x7070, 0x04, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0xC0, SER_LANE_REMAP_B0, 0x4E, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, SER_LANE_REMAP_B1, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
	1, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0xC0, 0x7000, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
	50, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0xC0, 0x7A14, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7B14, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7C14, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7D14, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0xC0, 0x7A14, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7A14, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x6420, 0x11, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x6420, 0x10, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x6420, 0x11, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
	100, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

/* VID_LINK_SEL_X, Y, Z, U of SER will be written 01 */
	{0xC0, 0x0100, 0x61, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0110, 0x61, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0120, 0x61, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0130, 0x61, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x04CF, 0xBF, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x05CF, 0xBF, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x06CF, 0xBF, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x07CF, 0xBF, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

/*****************************************/
/* Configure GM03 DP_RX Payload IDs      */
/*****************************************/
/*Sets the MST payload ID of the video stream for video output port 0, 1, 2, 3*/
	{0xC0, 0x7904, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7908, 0x02, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x790C, 0x03, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7910, 0x04, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7A10, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7A00, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
/* DMA mode enable */
	{0xC0, 0x7A18, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7A24, 0x1F, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7A26, 0x1F, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7A14, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7B10, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7B00, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7B18, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7B24, 0x1F, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7B26, 0x1F, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7B14, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7C10, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7C00, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7C18, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7C24, 0x1F, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7C26, 0x1F, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7C14, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7D10, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7D00, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7D18, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7D24, 0x1F, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7D26, 0x1F, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7D14, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0x90, 0x6184, 0x0F, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0x90, DES_DROP_VIDEO, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x94, DES_DROP_VIDEO, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x98, DES_DROP_VIDEO, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xD0, DES_DROP_VIDEO, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0x90, DES_STREAM_SELECT, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x94, DES_STREAM_SELECT, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x98, DES_STREAM_SELECT, 0x02, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xD0, DES_STREAM_SELECT, 0x03, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0xC0, 0x6420, 0x10, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x6420, 0x1F, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
	100, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0xC0, 0x6420, 0x10, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x6420, 0x1F, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

/********** Des & GPIO & I2C Setting *************/
	{0x90, 0x0005, 0x70, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x01CE, 0x4E, TCC_EVB_LCD_POW_MAX, SER_REV_ALL}, /* 1st LCD */
	{0x94, 0x0005, 0x70, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x94, 0x01CE, 0x4E, TCC_EVB_LCD_POW_MAX, SER_REV_ALL}, /* 2nd LCD */
	{0x98, 0x0005, 0x70, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x98, 0x01CE, 0x4E, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},/* 3rd LCD */
	{0xD0, 0x0005, 0x70, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xD0, 0x01CE, 0x4E, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},/* 4th LCD */

/* LCD Reset 1 : Ser GPIO #1 RX/TX RX ID 1  --> LCD Reset #1 */
	{0xC0, 0x0208, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0209, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0233, 0x84, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0234, 0xB1, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0235, 0x61, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

/* LCD Reset 2( TCC8059 ) : Ser GPIO #1 RX/TX RX ID 1  --> LCD Reset 1 */
	{0x94, 0x0233, 0x84, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	{0x94, 0x0234, 0xB1, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	{0x94, 0x0235, 0x61, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},

/* LCD Reset 3( TCC8059 ) : Ser GPIO #1 RX/TX RX ID 1  --> LCD Reset 3 */
	{0x98, 0x0233, 0x84, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	{0x98, 0x0234, 0xB1, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	{0x98, 0x0235, 0x61, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	{0xC0, 0x020B, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x020B, 0x21, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

/* LCD Reset 2( TCC8050 ) : Ser GPIO #11 RX/TX RX ID 11	--> LCD Reset #2 */
	{0xC0, 0x0258, 0x01, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x0259, 0x0B, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x94, 0x0233, 0x84, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x94, 0x0234, 0xB1, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x94, 0x0235, 0x6B, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x025B, 0x20, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x025B, 0x21, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},

/* LCD Reset 3 : Ser GPIO #15 RX/TX RX ID 15	--> LCD Reset 3 */
	{0xC0, 0x0278, 0x01, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x0279, 0x0F, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x98, 0x0233, 0x84, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x98, 0x0234, 0xB1, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x98, 0x0235, 0x6F, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x027B, 0x20, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x027B, 0x21, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},

/* LCD Reset 4 : Ser GPIO #22 RX/TX RX ID 22	--> LCD Reset #4 */
	{0xC0, 0x02B0, 0x01, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x02B1, 0x16, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xD0, 0x0233, 0x84, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xD0, 0x0234, 0xB1, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xD0, 0x0235, 0x6F, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x02B3, 0x20, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x02B3, 0x21, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},

/* LCD on : Ser GPIO #24 RX/TX RX ID 24	--> LCD On #1, 2, 3, 4 */
	{0xC0, 0x02C0, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x02C1, 0x18, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0236, 0x84, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0237, 0xB2, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0238, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x94, 0x0236, 0x84, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x94, 0x0237, 0xB2, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x94, 0x0238, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x98, 0x0236, 0x84, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x98, 0x0237, 0xB2, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x98, 0x0238, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xD0, 0x0236, 0x84, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xD0, 0x0237, 0xB2, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xD0, 0x0238, 0x78, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x02C3, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x02C3, 0x21, TCC_EVB_LCD_POW_MAX, SER_REV_ALL}, /* Toggle */

/* Backlight on 1 : Ser GPIO #0 RX/TX RX ID 0 --> Backlight On 1 */
	{0xC0, 0x0200, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0201, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0068, 0x48, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0069, 0x48, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0206, 0x84, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0207, 0xA2, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0208, 0x60, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0048, 0x08, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0049, 0x08, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

/* Backlight on 2( TCC8059 ) : Ser GPIO #0 RX/TX RX ID 0 --> Backlight On 2 */
	{0x94, 0x0206, 0x84, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	{0x94, 0x0207, 0xA2, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	{0x94, 0x0208, 0x60, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	{0x94, 0x0048, 0x08, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	{0x94, 0x0049, 0x08, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},

/* Backlight on 3( TCC8059 ) : Ser GPIO #0 RX/TX RX ID 0 --> Backlight On 2 */
	{0x98, 0x0206, 0x84, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	{0x98, 0x0207, 0xA2, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	{0x98, 0x0208, 0x60, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	{0x98, 0x0048, 0x08, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	{0x98, 0x0049, 0x08, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	{0xC0, 0x0203, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0203, 0x21, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

/* Backlight on 2 : Ser GPIO #0 RX/TX RX ID 5 --> Backlight On 2 */
	{0xC0, 0x0228, 0x01, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x0229, 0x05, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x94, 0x0206, 0x84, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x94, 0x0207, 0xA2, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x94, 0x0208, 0x65, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x94, 0x0048, 0x08, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x94, 0x0049, 0x08, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x022B, 0x20, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x022B, 0x21, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},

/* Backlight on 3 : Ser GPIO #0 RX/TX RX ID 14 --> Backlight On 3*/
	{0xC0, 0x0270, 0x01, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x0271, 0x0E, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x98, 0x0206, 0x84, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x98, 0x0207, 0xA2, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x98, 0x0208, 0x6E, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x98, 0x0048, 0x08, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x98, 0x0049, 0x08, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x0273, 0x20, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x0273, 0x21, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},

/* Backlight on 4 : Ser GPIO #0 RX/TX RX ID 21 --> Backlight On 4*/
	{0xC0, 0x02A8, 0x01, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x02A9, 0x15, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xD0, 0x0206, 0x84, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xD0, 0x0207, 0xA2, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xD0, 0x0208, 0x75, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xD0, 0x0048, 0x08, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x98, 0x0049, 0x08, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x0273, 0x20, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x0273, 0x21, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
/*****************************************/
/*	I2C Setting							 */
/*****************************************/
/* Des1, 2, 3 GPIO #14 I2C Driving */

	{0x90, 0x020C, 0x90, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x94, 0x020C, 0x90, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x98, 0x020C, 0x90, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xD0, 0x020C, 0x90, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},

	{0x0, 0x0, 0x0, 0, SER_REV_ALL}
};

static struct Max968xx_Reg_Data pstDP_Panel_VIC_1027_DesES2_RegVal[] = {
	{0x90, 0x0010, 0x80, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0010, 0x80, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
	100, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0x90, 0x0308, 0x03, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x03E0, 0x07, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x14A6, 0x0F, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x1460, 0x87, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x141F, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x1431, 0x08, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x141D, 0x02, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x14E1, 0x22, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0x90, 0x04D4, 0x43, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0423, 0x47, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x04E1, 0x22, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x03E0, 0x07, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0xC0, 0x0050, 0x66, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0x90, 0x001A, 0x10, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0022, 0x10, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0xC0, 0x0029, 0x02, TCC_EVB_LCD_POW_MAX},
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
	300, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0xC0, 0x6421, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x6420, 0x10, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7019, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7A14, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0xC0, 0x60AA, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x61AA, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x62AA, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x63AA, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x60B6, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x61B6, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x62B6, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x63B6, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x60A9, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x61A9, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x62A9, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x63A9, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x60B5, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x61B5, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x62B5, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x63B5, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x60A8, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x61A8, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x62A8, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x63A8, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x60B4, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x61B4, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x62B4, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x63B4, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x6070, 0xA5, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x6071, 0x65, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x6170, 0xA5, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x6171, 0x65, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x6270, 0xA5, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x6271, 0x65, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x6370, 0xA5, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	{0xC0, 0x6371, 0x65, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},

	{0xC0, 0x70A0, 0x04, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x6064, 0x06, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x6065, 0x06, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x6164, 0x06, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x6165, 0x06, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x6264, 0x06, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x6265, 0x06, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x6364, 0x06, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x6365, 0x06, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0xC0, 0x7000, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7054, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
	1, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0xC0, 0x7074, 0x0A, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7070, 0x04, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0xC0, SER_LANE_REMAP_B0, 0x4E, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, SER_LANE_REMAP_B1, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
	1, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0xC0, 0x7000, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
	1, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0xC0, 0x7A18, 0x05, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7A28, 0xFF, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7A2A, 0xFF, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7A24, 0xFF, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7A27, 0x0F, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7A14, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x6420, 0x11, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x6420, 0x10, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x6420, 0x11, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0x90, 0x0005, 0x70, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x01CE, 0x4E, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0xC0, 0x0210, 0x40, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0211, 0x40, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0212, 0x0F, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0213, 0x02, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0220, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0221, 0x04, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0223, 0x21, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0208, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0209, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x020B, 0x21, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x02C0, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x02C1, 0x18, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x02C3, 0x21, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0200, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0201, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0203, 0x21, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0068, 0x48, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0069, 0x48, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0x90, 0x022D, 0x43, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x022E, 0x6f, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x022F, 0x6f, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0230, 0x84, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0231, 0xb0, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0232, 0x44, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0233, 0x8c, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0234, 0xb1, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0235, 0x41, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0236, 0x84, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0237, 0xb2, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0238, 0x58, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0206, 0x84, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0207, 0xA2, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0208, 0x40, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0048, 0x08, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0049, 0x08, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0xC0, 0x009E, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0079, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0006, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0071, 0x02, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0210, 0x60, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0212, 0x4F, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0213, 0x02, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x022D, 0x63, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x022E, 0x6F, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x022F, 0x2F, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x022A, 0x18, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x020C, 0x80, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
	100, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0x90, 0x020C, 0x90, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
	100, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0x90, 0x020C, 0x80, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{DP_SER_DES_DELAY_DEV_ADDR, DP_SER_DES_INVALID_REG_ADDR,
	100, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0x90, 0x020C, 0x90, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x0, 0x0, 0x0, 0, SER_REV_ALL}
};

static int32_t dptx_max968xx_get_drv(uint8_t ucI2C_DevAdd, struct Max968xx_dev **ppstMax968xx_dev)
{
	bool bfound_drv = (bool)false;
	uint8_t ucdev_add;
	int32_t iret = 0;
	struct Max968xx_dev *pst_curr;

	/* [FP] misra_c_2012_rule_8_6_violation*/
	/* [FP] misra_c_2012_rule_8_13_violation*/
	/* [FP] misra_c_2012_rule_10_1_violation*/
	/* [FP] misra_c_2012_rule_11_5_violation*/
	/* [FP] misra_c_2012_rule_14_4_violation*/
	/* [FP] misra_c_2012_rule_15_6_violation*/
	/* [FP] misra_c_2012_rule_18_4_violation*/
	/* [FP] misra_c_2012_rule_20_7_violation*/
	/* [FP] misra_c_2012_rule_21_2_violation*/
	/* [FP] cert_dcl37_c_violation*/
	list_for_each_entry(pst_curr, &max968xx_list, list) {
		if (pst_curr->client == NULL) {
			/* For KCS */
			continue;
		}

		if (!CHECK_DEV_ADDR(pst_curr->client->addr)) {
			/* For KCS */
			continue;
		}

		ucdev_add = (uint8_t)(pst_curr->client->addr << 1);

		if (ucdev_add == ucI2C_DevAdd) {
			*ppstMax968xx_dev = pst_curr;
			bfound_drv = (bool)true;
			break;
		}
	}

	if (!bfound_drv) {
		dptx_debug("Failed to get client from dev add 0x%x", ucI2C_DevAdd);
		iret = -ENODEV;
	}

	return iret;
}

static int32_t dptx_max968xx_get_board_infor(uint8_t *pucser_rev, uint8_t *pucdes_rev, uint8_t *pucpower_type)
{
	int32_t iret = 0;
	struct Max968xx_dev *pstMax968xx_dev;

	iret = dptx_max968xx_get_drv((uint8_t)DP0_PANEL_SER_I2C_DEV_ADD, &pstMax968xx_dev);
	if (iret != 0) {
		dptx_err("Master Ser isn't connnected");

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	*pucser_rev = pstMax968xx_dev->ucser_rev;
	*pucpower_type = pstMax968xx_dev->ucevb_power_type;

	dptx_debug("Got Ser rev(%u) from device 0xC0", pstMax968xx_dev->ucser_rev);

	iret = dptx_max968xx_get_drv((uint8_t)DP0_PANEL_DES_I2C_DEV_ADD,
									&pstMax968xx_dev);
	if (iret != 0) {
		dptx_err("Master Des isn't connnected");

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	*pucdes_rev = pstMax968xx_dev->ucdes_rev;

	dptx_debug("Got Des rev(%u) from device 0x90", pstMax968xx_dev->ucdes_rev);

return_funcs:
	return iret;
}

static int dptx_max968xx_i2c_write(struct i2c_client *client, unsigned short usRegAdd, unsigned char ucValue)
{
	unsigned char aucWBuf[3] = {0,};
	int32_t iret_len, iw_len;
	int32_t iret = DPTX_RETURN_NO_ERROR;

	aucWBuf[0] = (uint8_t)(usRegAdd >> 8);
	aucWBuf[1] = (uint8_t)(usRegAdd & 0xFFU);
	aucWBuf[2] = ucValue;

	iw_len = (SER_DES_I2C_REG_ADD_LEN + SER_DES_I2C_DATA_LEN);

	iret_len = i2c_master_send((const struct i2c_client *)client, (const char *)aucWBuf, iw_len);
	if (iret_len != iw_len) {
		iret = DPTX_RETURN_ENODEV;

		dptx_err("i2c device %s: error to write address 0x%x.. len %d", client->name, client->addr, iret_len);
	}

	return iret;
}

static bool dptx_max968xx_check_continue(struct Max968xx_dev **ppstMax968xx_dev,
															bool blane_swap,
															bool bmst_mode,
															uint8_t ucpower_type,
															uint8_t ucser_rev,
															const struct Max968xx_Reg_Data *pstmax968xx_reg_data)
{
	bool bret = (bool)false;
	uint8_t uci2c_dev_add, uct_power_type, uct_ser_rev;
	int32_t iret = DPTX_RETURN_NO_ERROR;
	uint32_t uit_reg_add;
	struct Max968xx_dev *pstMax968xx_dev;

	uci2c_dev_add = (uint8_t)pstmax968xx_reg_data->uidev_addr;

	iret = dptx_max968xx_get_drv(uci2c_dev_add, &pstMax968xx_dev);
	if (iret != 0) {
		bret = (bool)true;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	uit_reg_add = pstmax968xx_reg_data->uireg_addr;
	uct_power_type = pstmax968xx_reg_data->ucpower_type;
	uct_ser_rev = pstmax968xx_reg_data->ucser_rev;

	*ppstMax968xx_dev = pstMax968xx_dev;

	if ((uct_power_type != (uint8_t)TCC_EVB_LCD_POW_MAX) && (ucpower_type != uct_power_type)) {
		bret = (bool)true;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	if ((uct_ser_rev != (uint8_t)SER_REV_ALL) && (ucser_rev != uct_ser_rev)) {
		bret = (bool)true;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

 	if ((!bmst_mode) && ((uit_reg_add == (uint32_t)DES_DROP_VIDEO) || (uit_reg_add == (uint32_t)DES_STREAM_SELECT))) {
			bret = (bool)true;

			/* [FP] misra_c_2012_rule_15_1_violation*/
			goto return_funcs;
	}

	if ((!blane_swap) && ((uit_reg_add == (uint32_t)SER_LANE_REMAP_B0) || (uit_reg_add == (uint32_t)SER_LANE_REMAP_B1))) {
			/* For KCS */
			bret = (bool)true;
	}

return_funcs:
	return bret;
}

static int32_t dptx_max968xx_set_wdata(bool bmst_mode,
													bool blane_swap,
													uint8_t *pucw_buf,
													const uint8_t aucvcp_id[PHY_INPUT_STREAM_MAX],
													struct Max968xx_Reg_Data *pstmax968xx_reg_data)
{
	uint8_t ucvcp_id = 0, ucdev_add = 0, ucdp_idx;
	uint8_t uclo_lane0, uclo_lane1, uclo_lane2, uclo_lane3;
	uint32_t uit_reg_add;

	uit_reg_add = pstmax968xx_reg_data->uireg_addr;
	ucdev_add = (uint8_t)(pstmax968xx_reg_data->uidev_addr & 0xFFU);

	if (!bmst_mode && (uit_reg_add == (uint32_t)SER_MISC_CONFIG_B1)) {
		dptx_debug("Setging to SST...");
		*pucw_buf = MST_FUNCTION_DISABLE;
	}

	if (blane_swap) {
		if (uit_reg_add == (uint32_t)SER_LANE_REMAP_B0) {
			uclo_lane0 = PHY_LANE_2;
			uclo_lane1 = PHY_LANE_3;
			uclo_lane1 = (uclo_lane1 << 2U);
			uclo_lane2 = PHY_LANE_0;
			uclo_lane2 = (uclo_lane1 << 4U);
			uclo_lane3 = PHY_LANE_1;
			uclo_lane3 = (uclo_lane1 << 6U);

			*pucw_buf = (uclo_lane0 | uclo_lane1 | uclo_lane2 | uclo_lane3);

			dptx_dbg("[%s:%d]Lane swap as 0x%x\n", __func__, __LINE__, *pucw_buf);
		}

		if (uit_reg_add == (uint32_t)SER_LANE_REMAP_B1) {
			/* For KCS */
			*pucw_buf = 0x01U;
		}
	}

	if (uit_reg_add == (uint32_t)DES_STREAM_SELECT) {
		switch (ucdev_add) {
		case (uint8_t)DP0_PANEL_DES_I2C_DEV_ADD:
			ucvcp_id = aucvcp_id[0];
			ucdp_idx = 0;
			break;
		case (uint8_t)DP1_PANEL_DES_I2C_DEV_ADD:
			ucvcp_id = aucvcp_id[1];
			ucdp_idx = 1U;
			break;
		case (uint8_t)DP2_PANEL_DES_I2C_DEV_ADD:
			ucvcp_id = aucvcp_id[2];
			ucdp_idx = 2U;
			break;
		case (uint8_t)DP3_PANEL_DES_I2C_DEV_ADD:
			ucvcp_id = aucvcp_id[3];
			ucdp_idx = 3U;
			break;
		default:
			dptx_warn("Invalid i2c dev add as 0x%x\n", ucdev_add);
			ucvcp_id = 1U;
			ucdp_idx = 0;
			break;
		}

		*pucw_buf = (ucvcp_id - 1U);

		dptx_info("Set VCP id %d to DP %u", *pucw_buf, ucdp_idx);
	}

	return DPTX_RETURN_NO_ERROR;
}

static int32_t of_parse_serdes_dt(struct Max968xx_dev *pstDev)
{
	int32_t iret = 0;
	uint32_t uievb_type;
	const struct device_node *dn;

	dn = of_find_compatible_node(NULL, NULL, "telechips,max968xx_configuration");
	if (dn == NULL) {
		/* For KCS */
		dptx_warn("Can't find SerDes node\n");
	}

	iret = of_property_read_u32(dn, "max968xx_evb_type", &uievb_type);
	if (iret < 0) {
		dptx_warn("Can't get EVB type.. set to 'TCC8050_SV_01' by default");
		uievb_type = (uint32_t)TCC8050_SV_01;
	}

	pstDev->ucevb_type = (uint8_t)(uievb_type & 0xFFU);

	switch (pstDev->ucevb_type) {
		case TCC8059_EVB_01:
			pstDev->ucevb_power_type = (uint8_t)TCC_EVB_LCD_ONE_POW;
			break;
		case TCC8050_SV_01:
		case TCC8050_SV_10:
		case TCC8070_SV_01:
		default:
			pstDev->ucevb_power_type = (uint8_t)TCC_EVB_LCD_FOUR_POW;
			break;
	}

	return DPTX_RETURN_NO_ERROR;
}

static int32_t panel_max968xx_init_list(uint8_t *pucnum_of_list,
							struct Max968xx_dev **ppstMax968xx_dev)
{
	uint8_t ucnum_of_list = 0;
	int32_t iret = 0;
	struct Max968xx_dev *pstMax968xx_dev;
	const struct Max968xx_dev *pst_curr;

	/* [FP] misra_c_2012_rule_10_8_violation*/
	/* [FP] misra_c_2012_rule_11_5_violation*/
	pstMax968xx_dev = (struct Max968xx_dev *)kzalloc(sizeof(struct Max968xx_dev), GFP_KERNEL);
	if (pstMax968xx_dev == NULL) {
		dptx_err("can't alloc device mem");

		iret = -ENOMEM;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	list_add(&pstMax968xx_dev->list, &max968xx_list);

	/* [FP] misra_c_2012_rule_8_6_violation*/
	/* [FP] misra_c_2012_rule_8_13_violation*/
	/* [FP] misra_c_2012_rule_10_1_violation*/
	/* [FP] misra_c_2012_rule_11_5_violation*/
	/* [FP] misra_c_2012_rule_14_4_violation*/
	/* [FP] misra_c_2012_rule_15_6_violation*/
	/* [FP] misra_c_2012_rule_18_4_violation*/
	/* [FP] misra_c_2012_rule_20_7_violation*/
	/* [FP] misra_c_2012_rule_21_2_violation*/
	/* [FP] cert_dcl37_c_violation*/
	list_for_each_entry(pst_curr, &max968xx_list, list) {
		/* For KCS */
		ucnum_of_list++;

		if (ucnum_of_list >= (uint8_t)SER_DES_INPUT_INDEX_MAX) {
			/* For KCS */
			break;
		}
	}

	*ppstMax968xx_dev = pstMax968xx_dev;
	*pucnum_of_list = ucnum_of_list;

return_funcs:
	return iret;
}

int32_t Dptx_Max968XX_Get_TopologyState(u8 *num_of_ports)
{
	uint8_t ucnum_of_ports = 0;
	int32_t iret = 0;
	struct Max968xx_dev *pst_curr;

	if (num_of_ports == NULL) {
		dptx_err("Err [%s:%d]num_of_ports == NULL", __func__, __LINE__);
		iret = -EINVAL;

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	/* [FP] misra_c_2012_rule_8_6_violation*/
	/* [FP] misra_c_2012_rule_8_13_violation*/
	/* [FP] misra_c_2012_rule_10_1_violation*/
	/* [FP] misra_c_2012_rule_11_5_violation*/
	/* [FP] misra_c_2012_rule_14_4_violation*/
	/* [FP] misra_c_2012_rule_15_6_violation*/
	/* [FP] misra_c_2012_rule_18_4_violation*/
	/* [FP] misra_c_2012_rule_20_7_violation*/
	/* [FP] misra_c_2012_rule_21_2_violation*/
	/* [FP] cert_dcl37_c_violation*/
	list_for_each_entry(pst_curr, &max968xx_list, list) {
		if (pst_curr->client == NULL) {
			/* For KCS */
			continue;
		}

		ucnum_of_ports++;

		if (ucnum_of_ports >= SER_DES_INPUT_INDEX_MAX) {
			/* For KCS */
			break;
		}
	}

	ucnum_of_ports = (ucnum_of_ports >= (uint8_t)DES_INPUT_INDEX_0) ? (ucnum_of_ports - 1U):0U;

	*num_of_ports = ucnum_of_ports;

return_funcs:
	return iret;
}
EXPORT_SYMBOL(Dptx_Max968XX_Get_TopologyState);

int32_t Dptx_Max968XX_Reset(const struct Dptx_Params *pstDptx)
{
	bool bcontinue, blane_swap;
	uint8_t ucser_rev  = 0, ucdes_rev = 0, ucpower_tpye = 0;
	uint8_t ucw_data;
	int32_t iret = DPTX_RETURN_NO_ERROR;
	uint32_t uit_idx, actual_cnt = 0;
	struct Max968xx_dev *pstMax968xx_dev;
	struct Max968xx_Reg_Data *pstmax968xx_reg_data;

	iret = dptx_max968xx_get_board_infor(&ucser_rev, &ucdes_rev, &ucpower_tpye);
	if (iret != DPTX_RETURN_NO_ERROR) {
		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	if (ucdes_rev == (uint8_t)DES_REV_ES2) {
		pstmax968xx_reg_data = pstDP_Panel_VIC_1027_DesES2_RegVal;

		dptx_dbg("Writing Des ES2 Tables, Ser Rev(%u)<->Des Rev(%u)", ucser_rev, ucdes_rev);
	} else {
		pstmax968xx_reg_data = pstDP_Panel_VIC_1027_DesES3_RegVal;

		dptx_dbg("Writing DES ES3 Tables, Ser Rev(%u)<->Des Rev(%u)", ucser_rev, ucdes_rev);
	}

	for (uit_idx = 0;
		!((pstmax968xx_reg_data[uit_idx].uidev_addr == 0U) &&
			(pstmax968xx_reg_data[uit_idx].uireg_addr == 0U) &&
			(pstmax968xx_reg_data[uit_idx].uireg_val == 0U));
			uit_idx++) {
		if (pstmax968xx_reg_data[uit_idx].uidev_addr == (uint32_t)DP_SER_DES_DELAY_DEV_ADDR) {
			/* [FP] misra_c_2012_rule_10_1_violation*/
			/* [FP] misra_c_2012_rule_10_4_violation*/
			/* [FP] misra_c_2012_rule_10_6_violation*/
			/* [FP] misra_c_2012_rule_10_7_violation*/
			/* [FP] misra_c_2012_rule_12_1_violation*/
			/* [FP] misra_c_2012_rule_14_4_violation*/
			/* [FP] misra_c_2012_rule_15_6_violation*/
			/* [FP] cert_int30_c_violation*/
			/* [FP] cert_dcl37_c_violation*/
			mdelay(pstmax968xx_reg_data[uit_idx].uireg_val);
			continue;
		}

		blane_swap = (pstDptx->bPhy_Lane_Std) ? (bool)false :(bool)true;

		bcontinue = dptx_max968xx_check_continue(&pstMax968xx_dev,
												blane_swap,
												pstDptx->bMultStreamTransport,
												ucpower_tpye,
												ucser_rev,
												&pstmax968xx_reg_data[uit_idx]);
		if (bcontinue) {
			/* For KCS */
			continue;
		}

		ucw_data = (uint8_t)pstmax968xx_reg_data[uit_idx].uireg_val;

		(void)dptx_max968xx_set_wdata(pstDptx->bMultStreamTransport,
									blane_swap,
									&ucw_data,
									pstDptx->aucVCP_Id,
									&pstmax968xx_reg_data[uit_idx]);

		iret = dptx_max968xx_i2c_write(pstMax968xx_dev->client,
											(uint16_t)pstmax968xx_reg_data[uit_idx].uireg_addr,
											ucw_data);
		if (iret != DPTX_RETURN_NO_ERROR) {
			/* For KCS */
			continue;
		}

		actual_cnt++;
	}

	dptx_dbg("%d SerDes Resisters are successfully done!!!\n", actual_cnt);
	dptx_dbg("  VCP Id: %u %u %u %u",
				pstDptx->aucVCP_Id[0], pstDptx->aucVCP_Id[1],
				pstDptx->aucVCP_Id[2], pstDptx->aucVCP_Id[3]);

return_funcs:
	return iret;
}
EXPORT_SYMBOL(Dptx_Max968XX_Reset);

static int32_t Dptx_Max968XX_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	bool bdev_connected = (bool)true;
	uint8_t ucaddr_buf[2] = {0,};
	uint8_t ucdev_add, ucdata_buf = 0, ucnum_of_list = 0, ucnum_of_ports;
	int32_t iret_len, iret = 0;
	struct Max968xx_dev *pstMax968xx_dev;

	iret = panel_max968xx_init_list(&ucnum_of_list, &pstMax968xx_dev);
	if (iret != 0) {
		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	if (!CHECK_DEV_ADDR(client->addr)) {
		dptx_err("Invalid dev addr as 0x%x", client->addr << 1);

		/* [FP] misra_c_2012_rule_15_1_violation*/
		goto return_funcs;
	}

	ucdev_add = (uint8_t)(client->addr << 1);

	ucaddr_buf[0] = (uint8_t)((uint16_t)SER_DEV_REV >> 8);
	ucaddr_buf[1] = (uint8_t)((uint16_t)SER_DEV_REV & 0xFFU);

	iret_len = i2c_master_send((const struct i2c_client *)client,
								(const char *)ucaddr_buf,
								(int)SER_DES_I2C_REG_ADD_LEN);
	if (iret_len != (int32_t)SER_DES_I2C_REG_ADD_LEN) {
		/* For KCS */
		bdev_connected = (bool)false;
	} else {
		iret_len = i2c_master_recv((const struct i2c_client *)client, (char *)&ucdata_buf, 1);
		if (iret_len != 1) {
			/* For KCS */
			bdev_connected = (bool)false;
		}
	}

	pstMax968xx_dev->client = bdev_connected ? client : NULL;
	pstMax968xx_dev->ucser_rev = (ucdev_add == (uint8_t)DP0_PANEL_SER_I2C_DEV_ADD) ? ucdata_buf : 0U;
	pstMax968xx_dev->ucdes_rev = (ucdev_add == (uint8_t)DP0_PANEL_SER_I2C_DEV_ADD) ? 0U : ucdata_buf;

	(void)of_parse_serdes_dt(pstMax968xx_dev);

	if (ucnum_of_list == (uint8_t)SER_DES_INPUT_INDEX_MAX) {
		(void)Dptx_Max968XX_Get_TopologyState(&ucnum_of_ports);
	}

	i2c_set_clientdata(client, pstMax968xx_dev);

	if (ucnum_of_list == (uint8_t)DES_INPUT_INDEX_0) {
		dptx_dbg("TCC-DP-SerDes-Ver %d.%d.%d", DRIVER_MAJOR, DRIVER_MINOR, DRIVER_PATCH);
	}
	dptx_dbg(" [%d]%s: %s I2C name(%s), Add(0x%x), Rev(%d) -> %s",
				ucnum_of_list,
				(pstMax968xx_dev->ucevb_type == (uint8_t)TCC8059_EVB_01) ? "TCC8059 EVB" :
				(pstMax968xx_dev->ucevb_type == (uint8_t)TCC8050_SV_01) ? "TCC8050/3 sv0.1" :
				(pstMax968xx_dev->ucevb_type == (uint8_t)TCC8050_SV_10) ? "TCC8050/3 sv1.0" :
				(pstMax968xx_dev->ucevb_type == (uint8_t)TCC8070_SV_01) ? "TCC8070 sv0.1" : "Unknown",
				(ucdev_add == (uint8_t)DP0_PANEL_SER_I2C_DEV_ADD) ? "Ser":"Des",
				client->name,
				ucdev_add,
				ucdata_buf,
				(bdev_connected) ? "connected":"not connected");
	if (ucnum_of_list == (uint8_t)SER_DES_INPUT_INDEX_MAX) {
		dptx_dbg(" Num of DPs: %u - %s mode\n", ucnum_of_ports, (ucnum_of_ports > 1U) ? "MST" : "SST");
	}

return_funcs:
	return DPTX_RETURN_NO_ERROR;
}

/* [FP] misra_c_2012_rule_2_7_violation*/
/* [FP] misra_c_2012_rule_8_13_violation*/
static int Dptx_Max968XX_remove(struct i2c_client *client)
{
	struct Max968xx_dev *pst_curr;

	/* [FP] misra_c_2012_rule_8_6_violation*/
	/* [FP] misra_c_2012_rule_8_13_violation*/
	/* [FP] misra_c_2012_rule_10_1_violation*/
	/* [FP] misra_c_2012_rule_11_5_violation*/
	/* [FP] misra_c_2012_rule_14_4_violation*/
	/* [FP] misra_c_2012_rule_15_6_violation*/
	/* [FP] misra_c_2012_rule_18_4_violation*/
	/* [FP] misra_c_2012_rule_20_7_violation*/
	/* [FP] misra_c_2012_rule_21_2_violation*/
	/* [FP] cert_dcl37_c_violation*/
	list_for_each_entry(pst_curr, &max968xx_list, list) {
		list_del(&pst_curr->list);
		kfree(pst_curr);
	}

	list_del_init(&max968xx_list);

	return DPTX_RETURN_NO_ERROR;
}

static const struct of_device_id max_96851_78_match[] = {
	{.compatible = "maxim,serdes"},
	{},
};
MODULE_DEVICE_TABLE(of, max_96851_78_match);


static const struct i2c_device_id max_96851_78_id[] = {
	{ "Max968XX", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, max_96851_78_id);


static struct i2c_driver stMax96851_78_drv = {
	.probe = Dptx_Max968XX_probe,
	.remove = Dptx_Max968XX_remove,
	.id_table = max_96851_78_id,
	.driver = {
			.name = "telechips,Max96851_78",
			.owner = THIS_MODULE,

#if defined(CONFIG_OF)
			.of_match_table = of_match_ptr(max_96851_78_match),
#endif
		},
};

static int __init Max968XX_Drv_init(void)
{
	int iret = DPTX_RETURN_NO_ERROR;

	INIT_LIST_HEAD(&max968xx_list);

	iret = i2c_add_driver(&stMax96851_78_drv);
	if (iret != 0) {
		dptx_err("Max96851_78 I2C registration failed");
	}

	return iret;
}
/* [FP] cert_dcl37_c_violation*/
/* [FP] misra_c_2012_rule_21_2_violation*/
/* [FP] misra_c_2012_rule_20_7_violation*/
module_init(Max968XX_Drv_init);

static void __exit Max968XX_Drv_exit(void)
{
	i2c_del_driver(&stMax96851_78_drv);
}
/* [FP] cert_dcl37_c_violation*/
/* [FP] misra_c_2012_rule_21_2_violation*/
/* [FP] misra_c_2012_rule_20_7_violation*/
module_exit(Max968XX_Drv_exit);

