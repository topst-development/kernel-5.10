// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 - present Telechips.co and/or its affiliates.
 */

#include <linux/backlight.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <drm/drm_crtc.h>
#include <drm/drm_panel.h>

#include <video/display_timing.h>
#include <video/of_videomode.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>
#include <video/telechips/vioc_lvds.h>

#include <telechips_drm_edid.h>

#include <linux/backlight.h>

#if defined(CONFIG_DRM_PANEL_MAX968XX)
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/list.h>

#include "dptx_video.h"
#include "dptx_drm.h"
#include "dptx_dbg.h"
#include "panel-telechips-dpv14.h"

#if defined(CONFIG_TOUCHSCREEN_INIT_SERDES)
#include <linux/input/tcc_tsc_serdes.h>
#endif

#endif

#define LOG_DPV14_TAG "DRM_DPV14"

#define DRIVER_DATE	"20250210"
#define DRIVER_MAJOR	3
#define DRIVER_MINOR	1
#define DRIVER_PATCH	0

#if defined(CONFIG_DRM_PANEL_MAX968XX)
#define SER_DES_I2C_REG_ADD_LEN			2
#define SER_DES_I2C_DATA_LEN			1

#define DP0_PANEL_SER_I2C_DEV_ADD		0xC0u	/* 0xC0 >> 1 = 0x60 */
#define DP0_PANEL_DES_I2C_DEV_ADD		0x90u	/* 0x90 >> 1 = 0x48 */
#define DP1_PANEL_DES_I2C_DEV_ADD		0x94u	/* 0x94 >> 1 = 0x4A */
#define DP2_PANEL_DES_I2C_DEV_ADD		0x98u	/* 0x98 >> 1 = 0x4C */
#define DP3_PANEL_DES_I2C_DEV_ADD		0xD0u	/* 0xD0 >> 1 = 0x68 */
#define DP3_PANEL_DES_SHIFT_I2C_DEV_ADD		0x68u	/* 0xD0 >> 1 = 0x68 */

#define	SER_DEV_REV				0x0Eu
#define SER_REV_ES2				0x01u
#define SER_REV_ES4				0x03u
#define SER_REV_CS				0x04u
#define SER_REV_ALL				0x0Fu

#define SER_LINK_RATE_RBR			0u
#define SER_LINK_RATE_HBR			1u
#define SER_LINK_RATE_HBR2			2u
#define SER_LINK_RATE_HBR3			3u

#define MST_FUNCTION_DISABLE			0x00u
#define MST_FUNCTION_ENABLE			0x01u

#define SER_PT_0				0x0079u
#define	SER_MISC_CONFIG_B1			0x7019u
#define	SER_LANE_REMAP_B0			0x7030u
#define	SER_LANE_REMAP_B1			0x7031u
#define SER_MAX_LINK_RATE			0x7074u

#define	DES_DEV_REV				0x000E
#define DES_REV_ES2				0x01
#define DES_REV_ES3				0x02
#define DES_CTRL3				0x13
#define DES_STREAM_SELECT			0x00A0u
#define DES_DROP_VIDEO				0x0307u

#define MAX968XX_DELAY_ADDR			0xEFFFu
#define MAX968XX_INVALID_REG_ADDR		0xFFFFu
#define MAX968XX_WATCH_LOCK_PIN_ADDR		0xDFFFu

#define PANEL_DP_MAX				4u
#define MAX_RETRY_I2C_RW			3u

#define CHECK_I2C_DEV_ADDR(x) (((x) <= DP3_PANEL_DES_SHIFT_I2C_DEV_ADD) ? (bool)true : (bool)false)

enum SERDES_INPUT_INDEX {
	SER_INPUT_INDEX_0		= 0,
	DES_INPUT_INDEX_0		= 1,
	DES_INPUT_INDEX_1		= 2,
	DES_INPUT_INDEX_2		= 3,
	DES_INPUT_INDEX_3		= 4,
	INPUT_INDEX_MAX			= 5
};

#define TCC8059_EVB_01		 0u
#define TCC8050_SV_01		 1u
#define TCC8050_SV_10		 2u
#define TCC8070_SV_01		 3u
#define TCC805X_EVB_UNKNOWN		 0xFEu

#define TCC_EVB_LCD_ONE_POW		0u
#define TCC_EVB_LCD_FOUR_POW		1u
#define TCC_EVB_LCD_POW_MAX		2u

enum PHY_INPUT_STREAM_IDX {
	PHY_INPUT_STREAM_0		= 0,
	PHY_INPUT_STREAM_1		= 1,
	PHY_INPUT_STREAM_2		= 2,
	PHY_INPUT_STREAM_3		= 3,
	PHY_INPUT_STREAM_MAX	= 4
};

#define PHY_LANE_IDX_0		0u
#define PHY_LANE_IDX_1		1u
#define PHY_LANE_IDX_2		2u
#define PHY_LANE_IDX_3		3u
#define PHY_LANE_IDX_4		4u
#define PHY_LANE_IDX_MAX	5u


struct panel_max968xx {
	uint8_t ucmst_mode;
	uint8_t ucser_rev;
	uint8_t ucdes_rev;
	uint8_t ucevb_type;
	uint8_t panel_power_type;
	uint8_t aucvcp_id[4];
	struct i2c_client *client;
	struct list_head list_;
};

LIST_HEAD(max968xx_list);

struct panel_max968xx_reg_data {
	unsigned int	uidev_addr;
	unsigned int	uireg_addr;
	unsigned int	uireg_val;
	unsigned char	panel_power_type;
	unsigned char	ucser_rev;
};

struct serdes_write_config  {
	const struct panel_max968xx_reg_data *panel_serdes_reg_data;
	struct panel_max968xx *panel_serdes;

	bool ser_swap_usb_to_dp_needed;
	bool serializer_reset_mode;

	uint8_t ser_rev;
	uint8_t des_rev;
	uint8_t panel_power_type;
	uint8_t num_of_ports;
	uint8_t link_rate;

	/* Respone */
	union {
		uint8_t wdata;         // I2C write data
		uint8_t action_flag;   // 0: Continue, 1: Stop or Skip
	};
};
#endif /* #if defined(CONFIG_DRM_PANEL_MAX968XX) */


struct dp_pins {
	struct pinctrl *p;
	struct pinctrl_state *pwr_port;
	struct pinctrl_state *pwr_on;		// LCD_ON
	struct pinctrl_state *reset_off;	// RESET OFF
	struct pinctrl_state *blk_on;		// BLK ON
	struct pinctrl_state *blk_off;		// BLK OFF
	struct pinctrl_state *pwr_off;		// LCD_ON &  RESET ON
};

struct panel_dp14 {
	struct drm_panel panel;
	struct device *dev;
	struct videomode video_mode;

#if defined(CONFIG_DRM_TELECHIPS_CTRL_BACKLIGHT)
	struct backlight_device *backlight;
#endif

	const struct dp_match_data *data;
	struct dp_pins st_dp_pins;

	/*
	 * Some panels, such as the AUO LVDS panel, must first transmit an
	 * LVDS signal when initializing the panel and then turn on the
	 * backlight after a certain period of time.
	 * The blk-wait-time is used to determine the waiting time until the
	 * backlight is turned on after a signal is transmitted to the
	 * LVDS panel.
	 */
	uint32_t blk_wait_time;
	ktime_t timestamp;

	/* Version ---------------------*/
	/** @major_version: driver major_version number */
	int major_version;
	/** @minor_version: driver minor_version number */
	int minor_version;
	/** @patchlevel: driver patch level */
	int patchlevel;
	/** @date: driver date */
	char *date;
};

struct dp_match_data {
	const char *name;
};

static const struct dp_match_data dpv14_panel_0 = {
	.name = "DP PANEL-0",
};

static const struct dp_match_data dpv14_panel_1 = {
	.name = "DP PANEL-1",
};

static const struct dp_match_data dpv14_panel_2 = {
	.name = "DP PANEL-2",
};

static const struct dp_match_data dpv14_panel_3 = {
	.name = "DP PANEL-3",
};

#if defined(CONFIG_DRM_PANEL_MAX968XX)
static struct panel_max968xx_reg_data stPanel_1027_DesES3_RegVal[] = {
	{0xD0, 0x0010, 0x80, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x98, 0x0010, 0x80, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x94, 0x0010, 0x80, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0010, 0x80, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0010, 0x80, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
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

	/**********************
	 * MST Setting        *
	 **********************
	 */
	/* Turn off video-GM03 */
	{0xC0, 0x6420, 0x10, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* Disable MST_VS0_DTG_ENABLE */
	{0xC0, 0x7A14, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* Disable LINK_ENABLE */
	{0xC0, 0x7000, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* Enable MST */
	{0xC0, SER_MISC_CONFIG_B1, MST_FUNCTION_ENABLE,
	TCC_EVB_LCD_POW_MAX, SER_REV_ALL}, /* 100ms delay */
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	10, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	/* Set AUX_RD_INTERVAL to 16ms */
	{0xC0, 0x70A0, 0x04, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* Max rate : 1E -> 8.1Gbps, 14 -> 5.4Gbps, 0A -> 2.7Gbps */
	{0xC0, SER_MAX_LINK_RATE, 0x1E, TCC_EVB_LCD_POW_MAX, SER_REV_ES4},
	{0xC0, SER_MAX_LINK_RATE, 0x1E, TCC_EVB_LCD_POW_MAX, SER_REV_CS},
	{0xC0, SER_MAX_LINK_RATE, 0x0A, TCC_EVB_LCD_POW_MAX, SER_REV_ES2},
	/* Max lane count to 4 */
	{0xC0, 0x7070, 0x04, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0xC0, SER_LANE_REMAP_B0, 0x4E, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, SER_LANE_REMAP_B1, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	1, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	/* Enable LINK_ENABLE */
	{0xC0, 0x7000, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	10, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	/* Enable MST_VS0_DTG_ENABLE */
	{0xC0, 0x7A14, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* Enable MST_VS1_DTG_ENABLE */
	{0xC0, 0x7B14, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* Enable MST_VS2_DTG_ENABLE */
	{0xC0, 0x7C14, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* Enable MST_VS3_DTG_ENABLE */
	{0xC0, 0x7D14, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	/* Disable MST_VS0_DTG_ENABLE */
	{0xC0, 0x7A14, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* Enable MST_VS0_DTG_ENABLE */
	{0xC0, 0x7A14, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* Turn on video */
	{0xC0, 0x6420, 0x11, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* Turn off video */
	{0xC0, 0x6420, 0x10, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* Turn on video */
	{0xC0, 0x6420, 0x11, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	10, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	/* VID_LINK_SEL_X, Y, Z, U of SER will be written 01 */
	{0xC0, 0x0100, 0x61, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0110, 0x61, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0120, 0x61, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0130, 0x61, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x04CF, 0xBF, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x05CF, 0xBF, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x06CF, 0xBF, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x07CF, 0xBF, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	/*****************************************
	 *      Configure GM03 DP_RX Payload IDs *
	 *****************************************
	 */
	/*
	 * Sets the MST payload ID of the video stream for video output
	 * port 0, 1, 2, 3
	 */
	{0xC0, 0x7904, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7908, 0x02, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x790C, 0x03, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x7910, 0x04, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* Video FIFO Overflow Clear */
	{0xC0, 0x7A10, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* MST virtual sink device 0 enable */
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

	{MAX968XX_WATCH_LOCK_PIN_ADDR, MAX968XX_INVALID_REG_ADDR,
	30, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/*
	 * MAIN_STREAM_ENABLE_MAIN_STREAM_ENABLE will be written 0001
	 */
	{0x90, 0x6184, 0x0F, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0x90, DES_DROP_VIDEO, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x94, DES_DROP_VIDEO, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x98, DES_DROP_VIDEO, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xD0, DES_DROP_VIDEO, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0x90, DES_STREAM_SELECT, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x94, DES_STREAM_SELECT, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x98, DES_STREAM_SELECT, 0x02, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xD0, DES_STREAM_SELECT, 0x03, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	/* EDP_VIDEO_CTRL0_VIDEO_OUT_EN of SER will be written 0000  */
	{0xC0, 0x6420, 0x10, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* EDP_VIDEO_CTRL0_VIDEO_OUT_EN of SER will be written 1111  */
	{0xC0, 0x6420, 0x1F, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	10, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	/* EDP_VIDEO_CTRL0_VIDEO_OUT_EN of SER will be written 0000  */
	{0xC0, 0x6420, 0x10, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* EDP_VIDEO_CTRL0_VIDEO_OUT_EN of SER will be written 1111  */
	{0xC0, 0x6420, 0x1F, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	/*****************************************
	 *      Des & GPIO & I2C Setting         *
	 *****************************************
	 */
	/* Enable Displays on each of the GMSL3 OLDIdes */
	/* 1st LCD */
	{0x90, 0x0005, 0x70, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x01CE, 0x4E, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* 2nd LCD */
	{0x94, 0x0005, 0x70, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x94, 0x01CE, 0x4E, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* 3rd LCD */
	{0x98, 0x0005, 0x70, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x98, 0x01CE, 0x4E, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* 4th LCD */
	{0xD0, 0x0005, 0x70, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xD0, 0x01CE, 0x4E, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	/* LCD Reset 1 : Ser GPIO #1 RX/TX RX ID 1  --> LCD Reset #1 */
	{0xC0, 0x0208, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0209, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0233, 0x84, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0234, 0xB1, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* Des1 GPIO #17 <-- RX/TX RX ID 1 */
	{0x90, 0x0235, 0x61, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	/* LCD Reset 2(TCC8059) : Ser GPIO #1 RX/TX RX ID 1  --> LCD Reset 1 */
	{0x94, 0x0233, 0x84, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	{0x94, 0x0234, 0xB1, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	/* Des2 GPIO #17 <-- RX/TX RX ID 1 */
	{0x94, 0x0235, 0x61, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},

	/* LCD Reset 3(TCC8059) : Ser GPIO #1 RX/TX RX ID 1  --> LCD Reset 3 */
	{0x98, 0x0233, 0x84, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	{0x98, 0x0234, 0xB1, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	/* Des3 GPIO #17 <-- RX/TX RX ID 1 */
	{0x98, 0x0235, 0x61, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	{0xC0, 0x020B, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x020B, 0x21, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	/*
	 * LCD Reset 2(TCC8050) : Ser GPIO #11 RX/TX RX ID 11   -->
	 * LCD Reset #2
	 */
	{0xC0, 0x0258, 0x01, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x0259, 0x0B, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x94, 0x0233, 0x84, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x94, 0x0234, 0xB1, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	/* Des2 GPIO #17 <-- RX/TX RX ID 11 */
	{0x94, 0x0235, 0x6B, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x025B, 0x20, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	/* Toggle */
	{0xC0, 0x025B, 0x21, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},

	/* LCD Reset 3 : Ser GPIO #15 RX/TX RX ID 15    --> LCD Reset 3 */
	{0xC0, 0x0278, 0x01, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x0279, 0x0F, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x98, 0x0233, 0x84, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x98, 0x0234, 0xB1, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	/* Des3 GPIO #17 <-- RX/TX RX ID 15 */
	{0x98, 0x0235, 0x6F, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x027B, 0x20, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x027B, 0x21, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},

	/* LCD Reset 4 : Ser GPIO #22 RX/TX RX ID 22    --> LCD Reset #4 */
	{0xC0, 0x02B0, 0x01, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x02B1, 0x16, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xD0, 0x0233, 0x84, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xD0, 0x0234, 0xB1, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	/* Des1 GPIO #17 <-- RX/TX RX ID 15 */
	{0xD0, 0x0235, 0x6F, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x02B3, 0x20, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x02B3, 0x21, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},

	/* LCD on : Ser GPIO #24 RX/TX RX ID 24 --> LCD On #1, 2, 3, 4 */
	{0xC0, 0x02C0, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x02C1, 0x18, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0236, 0x84, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0237, 0xB2, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* Des1 GPIO #18 <-- RX/TX RX ID 24 */
	{0x90, 0x0238, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x94, 0x0236, 0x84, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x94, 0x0237, 0xB2, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* Des2 GPIO #18 <-- RX/TX RX ID 24 */
	{0x94, 0x0238, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x98, 0x0236, 0x84, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x98, 0x0237, 0xB2, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* Des3 GPIO #18 <-- RX/TX RX ID 24 */
	{0x98, 0x0238, 0x78, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xD0, 0x0236, 0x84, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xD0, 0x0237, 0xB2, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	/* Des4 GPIO #18 <-- RX/TX RX ID 24 */
	{0xD0, 0x0238, 0x78, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x02C3, 0x20, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* Toggle */
	{0xC0, 0x02C3, 0x21, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	/* Backlight on 1 : Ser GPIO #0 RX/TX RX ID 0 --> Backlight On 1 */
	{0xC0, 0x0200, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0201, 0x00, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0068, 0x48, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0069, 0x48, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0206, 0x84, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0207, 0xA2, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* Des1 GPIO #2 <-- RX/TX RX ID 0 */
	{0x90, 0x0208, 0x60, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0048, 0x08, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x90, 0x0049, 0x08, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	/*
	 * Backlight on 2(TCC8059) : Ser GPIO #0 RX/TX RX ID 0 -->
	 * Backlight On 2
	 */
	{0x94, 0x0206, 0x84, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	{0x94, 0x0207, 0xA2, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	/* Des1 GPIO #2 <-- RX/TX RX ID 0 */
	{0x94, 0x0208, 0x60, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	{0x94, 0x0048, 0x08, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	{0x94, 0x0049, 0x08, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},

	/*
	 * Backlight on 3(TCC8059) : Ser GPIO #0 RX/TX RX ID 0 -->
	 * Backlight On 2
	 */
	{0x98, 0x0206, 0x84, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	{0x98, 0x0207, 0xA2, TCC_EVB_LCD_ONE_POW, SER_REV_ALL},
	/* Des1 GPIO #2 <-- RX/TX RX ID 0 */
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
	/* Des2 GPIO #2 <-- RX/TX RX ID 5 */
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
	/* Des1 GPIO #2 <-- RX/TX RX ID 14 */
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
	/* Des1 GPIO #2 <-- RX/TX RX ID 21 */
	{0xD0, 0x0208, 0x75, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xD0, 0x0048, 0x08, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0x98, 0x0049, 0x08, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x0273, 0x20, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	{0xC0, 0x0273, 0x21, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},
	/*****************************************
	 *      I2C Setting                      *
	 *****************************************
	 */
	/* Des1, 2, 3 GPIO #14 I2C Driving */
	{0x90, 0x020C, 0x90, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x94, 0x020C, 0x90, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x98, 0x020C, 0x90, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xD0, 0x020C, 0x90, TCC_EVB_LCD_FOUR_POW, SER_REV_ALL},

	{0x0, 0x0, 0x0, 0, SER_REV_ALL}
};

static struct panel_max968xx_reg_data stPanel_1027_DesES2_RegVal[] = {
	{0x90, 0x0010, 0x80, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, 0x0010, 0x80, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
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

	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
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

	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	1, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	/* Max rate : 1E -> 8.1Gbps, 14 -> 5.4Gbps, 0A -> 2.7Gbps */
	{0xC0, SER_MAX_LINK_RATE, 0x0A, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	/* Max lane count to 4*/
	{0xC0, 0x7070, 0x04, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0xC0, SER_LANE_REMAP_B0, 0x4E, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0xC0, SER_LANE_REMAP_B1, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	1, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0xC0, 0x7000, 0x01, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
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

	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	100, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0x90, 0x020C, 0x90, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	100, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0x90, 0x020C, 0x80, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{MAX968XX_DELAY_ADDR, MAX968XX_INVALID_REG_ADDR,
	100, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},

	{0x90, 0x020C, 0x90, TCC_EVB_LCD_POW_MAX, SER_REV_ALL},
	{0x0, 0x0, 0x0, 0, SER_REV_ALL}
};
#endif /* #if defined(CONFIG_DRM_PANEL_MAX968XX) */


static int32_t panel_dpv14_pin_select_state(struct pinctrl *p, struct pinctrl_state *s)
{
	int ret = 0;

	if ((p == NULL) || (s == NULL)) {
		/* For KCS */
		ret  = -EINVAL;
	} else {
		ret = pinctrl_select_state(p, s);
	}
	return ret;
}

static inline struct panel_dp14 *to_dp_drv_panel(struct drm_panel *pdrm_panel)
{
	struct panel_dp14 *dp14;

	dp14 = (struct panel_dp14 *)container_of(pdrm_panel, struct panel_dp14, panel);

	return dp14;
}

static int panel_dpv14_set_edid_property(struct panel_dp14 *pdp14,
					 struct drm_connector *pconnector,
					 const struct drm_display_mode *pmode,
					 struct edid *pedid)
{
	int32_t ret = 0;

	ret = tcc_make_edid_from_display_mode(pedid, pmode);
	if (ret != 0) {
		dev_err(pdp14->dev, "[ERROR][%s:%d]%s\r\n", __func__, __LINE__, pdp14->data->name);
		kfree(pedid);
		ret = -ENODEV;
	} else {
		ret = drm_connector_update_edid_property(pconnector, pedid);
		if (ret != 0) {
			dev_err(pdp14->dev, "[ERROR][%s:%d]%s\r\n", __func__, __LINE__, pdp14->data->name);
			kfree(pedid);
			ret = -ENODEV;
		}
	}
	return ret;
}

static int panel_dpv14_disable(struct drm_panel *pdrm_panel)
{
#if defined(CONFIG_DRM_TELECHIPS_CTRL_BACKLIGHT)
	unsigned int bl_fbblank;
#endif

	const struct panel_dp14 *dp14;

	dp14 = (struct panel_dp14 *)to_dp_drv_panel(pdrm_panel);

	dev_info(dp14->dev, "[INFO][%s:%d]To %s..\r\n", __func__, __LINE__, dp14->data->name);

#if defined(CONFIG_DRM_TELECHIPS_CTRL_BACKLIGHT)
	if (dp14->backlight != NULL) {
		bl_fbblank = (unsigned int)BL_CORE_FBBLANK;

		dp14->backlight->props.power = FB_BLANK_POWERDOWN;
		dp14->backlight->props.state |= bl_fbblank;

		(void)backlight_update_status(dp14->backlight);
	}
#else
	if (panel_dpv14_pin_select_state(dp14->st_dp_pins.p, dp14->st_dp_pins.blk_off) < 0) {
		/* For KCS */
		dev_warn(dp14->dev, "[WARN][%s:%d]%s : failed set pinctrl to blk_off\r\n", __func__, __LINE__, dp14->data->name);
	}
#endif

	return 0;
}

static int panel_dpv14_enable(struct drm_panel *pdrm_panel)
{
#if defined(CONFIG_DRM_TELECHIPS_CTRL_BACKLIGHT)
	unsigned int bl_fbblank;
#endif
	uint32_t wait_ms;
	s64 ms_delta;

	const struct panel_dp14 *dp14;

	dp14 = (struct panel_dp14 *)to_dp_drv_panel(pdrm_panel);

	dev_info(dp14->dev, "[INFO][%s:%d]To %s..\r\n", __func__, __LINE__, dp14->data->name);

	if (dp14->blk_wait_time > 0u) {
		ms_delta = ktime_ms_delta(ktime_get(), dp14->timestamp);
		if ((ms_delta >= (s64)0) && (ms_delta < (s64)dp14->blk_wait_time)) {
			wait_ms = dp14->blk_wait_time - (uint32_t)ms_delta;
			mdelay(wait_ms);
		}
	}
#if defined(CONFIG_DRM_TELECHIPS_CTRL_BACKLIGHT)
	if (dp14->backlight != NULL) {
		bl_fbblank = (unsigned int)BL_CORE_FBBLANK;
		bl_fbblank = ~bl_fbblank;

		dp14->backlight->props.state &= bl_fbblank;
		dp14->backlight->props.power = FB_BLANK_UNBLANK;

		(void)backlight_update_status(dp14->backlight);
	}
#else
	if (panel_dpv14_pin_select_state(dp14->st_dp_pins.p, dp14->st_dp_pins.blk_on) < 0) {
		/* For KCS */
		dev_warn(dp14->dev, "[WARN][%s:%d]%s : failed set pinctrl to blk_on\r\n", __func__, __LINE__, dp14->data->name);
	}
#endif

	return 0;
}

static int panel_dpv14_unprepare(struct drm_panel *pdrm_panel)
{
	const struct panel_dp14 *dp14;

	dp14 = (struct panel_dp14 *)to_dp_drv_panel(pdrm_panel);

	dev_info(dp14->dev, "[INFO][%s:%d]To %s..\r\n", __func__, __LINE__, dp14->data->name);
	if (panel_dpv14_pin_select_state(dp14->st_dp_pins.p, dp14->st_dp_pins.pwr_off) < 0) {
		/* For KCS */
		dev_warn(dp14->dev, "[WARN][%s:%d]%s : failed set pinctrl to blk_off\r\n", __func__, __LINE__, dp14->data->name);
	}
	return 0;
}

static int panel_dpv14_prepare(struct drm_panel *pdrm_panel)
{
	struct panel_dp14 *dp14;

	dp14 = to_dp_drv_panel(pdrm_panel);

	dev_info(dp14->dev, "[INFO][%s:%d]To %s..\r\n", __func__, __LINE__, dp14->data->name);

	if (panel_dpv14_pin_select_state(dp14->st_dp_pins.p, dp14->st_dp_pins.pwr_on) < 0) {
		/* For KCS */
		dev_warn(dp14->dev, "[WARN][%s:%d]%s : failed set pinctrl to pwr_on\r\n", __func__, __LINE__, dp14->data->name);
	}
	mdelay(1);

	if (panel_dpv14_pin_select_state(dp14->st_dp_pins.p, dp14->st_dp_pins.reset_off) < 0) {
		/* For KCS */
		dev_warn(dp14->dev, "[WARN][%s:%d]%s : failed set pinctrl to reset_off\r\n", __func__, __LINE__, dp14->data->name);
	}
	if (dp14->blk_wait_time > 0u) {
		/* For KCS */
		dp14->timestamp = ktime_get();
	}
	return 0;
}

/* HIS metric violation (HIS_GOTO) */
static int panel_dpv14_get_modes(struct drm_panel *pdrm_panel, struct drm_connector *connector)
{
	int32_t mode_count = 0;
	int32_t ret = 0;
	struct panel_dp14 *dp14 = NULL;
	struct drm_display_mode *disp_mode = NULL;
	struct edid *pst_edid = NULL;

	if (connector != NULL) {
		dp14 = (struct panel_dp14 *)to_dp_drv_panel(pdrm_panel);

		disp_mode = drm_mode_create(connector->dev);
		if (disp_mode == NULL) {
			dev_err(dp14->dev, "[ERROR][%s:%d]%s\r\n", __func__, __LINE__, dp14->data->name);
		} else {
			drm_display_mode_from_videomode(&dp14->video_mode, disp_mode);

			pst_edid = (struct edid *)kzalloc(EDID_LENGTH, GFP_KERNEL);
			if (pst_edid != NULL) {
				ret = panel_dpv14_set_edid_property(dp14,
								    connector,
								    disp_mode,
								    pst_edid);
				if (ret == 0) {
					/* For KCS */
					mode_count = drm_add_edid_modes(connector, pst_edid);
				} else {
					kfree((void *)disp_mode);
				}
			} else {
				kfree((void *)disp_mode);
			}
		}
	}

	return mode_count;
}

static const struct drm_panel_funcs panel_dpv14_funcs = {
	.disable = panel_dpv14_disable,
	.unprepare = panel_dpv14_unprepare,
	.prepare = panel_dpv14_prepare,
	.enable = panel_dpv14_enable,
	.get_modes = panel_dpv14_get_modes,
};

static int panel_pinctrl_get_lookup_state(struct panel_dp14 *pdp14)
{
	int ret = 0;

	pdp14->st_dp_pins.p = devm_pinctrl_get(pdp14->dev);
	if (IS_ERR(pdp14->st_dp_pins.p)) {
		dev_info(pdp14->dev, "[INFO][%s:%d]%s : no pinctrl available\r\n", __func__, __LINE__, pdp14->data->name);

		pdp14->st_dp_pins.p = NULL;

		ret = -ENODEV;

		goto return_funcs;
	}

	pdp14->st_dp_pins.pwr_port = pinctrl_lookup_state(pdp14->st_dp_pins.p, "default");
	if (IS_ERR(pdp14->st_dp_pins.pwr_port)) {
		dev_warn(pdp14->dev, "[WARN][%s:%d]%s : failed to find default\r\n", __func__, __LINE__, pdp14->data->name);

		pdp14->st_dp_pins.pwr_port = NULL;

		ret = -ENODEV;

		goto return_funcs;
	}

	pdp14->st_dp_pins.pwr_on = pinctrl_lookup_state(pdp14->st_dp_pins.p, "power_on");
	if (IS_ERR(pdp14->st_dp_pins.pwr_on)) {
		dev_warn(pdp14->dev, "[WARN][%s:%d]%s : failed to find power_on\r\n", __func__, __LINE__, pdp14->data->name);
		pdp14->st_dp_pins.pwr_on = NULL;
	}

	pdp14->st_dp_pins.reset_off = pinctrl_lookup_state(pdp14->st_dp_pins.p, "reset_off");
	if (IS_ERR(pdp14->st_dp_pins.reset_off)) {
		dev_warn(pdp14->dev, "[WARN][%s:%d]%s : failed to find reset_off\r\n", __func__, __LINE__, pdp14->data->name);
		pdp14->st_dp_pins.reset_off = NULL;
	}

	pdp14->st_dp_pins.blk_on = pinctrl_lookup_state(pdp14->st_dp_pins.p, "blk_on");
	if (IS_ERR(pdp14->st_dp_pins.blk_on)) {
		dev_warn(pdp14->dev, "[WARN][%s:%d]%s : failed to find blk_on\r\n", __func__, __LINE__, pdp14->data->name);
		pdp14->st_dp_pins.blk_on = NULL;
	}

	pdp14->st_dp_pins.blk_off = pinctrl_lookup_state(pdp14->st_dp_pins.p, "blk_off");
	if (IS_ERR(pdp14->st_dp_pins.blk_off)) {
		dev_warn(pdp14->dev, "[WARN][%s:%d]%s : failed to find blk_off\r\n", __func__, __LINE__, pdp14->data->name);
		pdp14->st_dp_pins.blk_off = NULL;
	}

	pdp14->st_dp_pins.pwr_off = pinctrl_lookup_state(pdp14->st_dp_pins.p, "power_off");
	if (IS_ERR(pdp14->st_dp_pins.pwr_off)) {
		dev_warn(pdp14->dev, "[WARN][%s:%d]%s : failed to find power_off\r\n", __func__, __LINE__, pdp14->data->name);
		pdp14->st_dp_pins.pwr_off = NULL;
	}

return_funcs:
	return ret;
}

#if defined(CONFIG_DRM_TELECHIPS_CTRL_BACKLIGHT)
static int panel_pinctrl_get_backlight_node(struct panel_dp14 *pdp14)
{
	int ret = 0;
	struct device_node *np;

	np = of_parse_phandle(pdp14->dev->of_node, "backlight", 0);
	if (np != NULL) {
		pdp14->backlight = of_find_backlight_by_node(np);

		of_node_put(np);

		if (pdp14->backlight == NULL) {
			/* For KCS */
			dev_info(pdp14->dev, "[INFO][%s:%d]%s : backlight node isn't valid\r\n", __func__, __LINE__, pdp14->data->name);
		} else {
			dev_info(pdp14->dev, "[INFO][%s:%d]%s : max brightness[%d] from external backlight driver\r\n",
											__func__,
											__LINE__,
											pdp14->data->name,
											pdp14->backlight->props.max_brightness);
		}
	} else {
		dev_info(pdp14->dev, "[INFO][%s:%d]%s : TCC backlight ctrl isn't available -> use pinctrl backlight\r\n",
								__func__,
								__LINE__,
								pdp14->data->name);
	}

	return ret;
}
#endif

static void panel_pinctrl_init_drm_panel(struct panel_dp14 *pdp14)
{
	drm_panel_init(&pdp14->panel, pdp14->dev, &panel_dpv14_funcs, DRM_MODE_CONNECTOR_LVDS);

	pdp14->panel.dev = pdp14->dev;
	pdp14->panel.funcs = &panel_dpv14_funcs;

	drm_panel_add(&pdp14->panel);

	dev_set_drvdata(pdp14->dev, pdp14);
}

/* HIS metric violation (HIS_GOTO) */
static int panel_pinctrl_parse_dt(struct panel_dp14 *pdp14)
{
	struct device_node *dn;
	struct device_node *np;
	uint32_t blk_wait_time;
	int ret = 0;

	dn = pdp14->dev->of_node;

	if (of_property_read_u32(dn, "blk-wait-time", &blk_wait_time) == 0) {
		if (blk_wait_time > 0u) {
			/* For KCS */
			pdp14->blk_wait_time = blk_wait_time;
		}
	}

	np = of_get_child_by_name(dn, "display-timings");
	if (np == NULL) {
		/* For KCS */
		dev_warn(pdp14->dev, "[WARN][%s:%d]%s : failed to get display-timings property\r\n", __func__, __LINE__, pdp14->data->name);
	} else {
		of_node_put(np);

		ret = of_get_videomode(dn, &pdp14->video_mode, OF_USE_NATIVE_MODE);
		if (ret < 0) {
			/* For KCS */
			dev_warn(pdp14->dev, "[WARN][%s:%d]%s : failed to parse videomode\r\n", __func__, __LINE__, pdp14->data->name);
		}
	}

	ret = panel_pinctrl_get_lookup_state(pdp14);
	if (ret != 0) {
		/* For KCS */
		goto return_funcs;
	}

#if defined(CONFIG_DRM_TELECHIPS_CTRL_BACKLIGHT)
	(void)panel_pinctrl_get_backlight_node(pdp14);
#else
	dev_info(pdp14->dev, "[INFO][%s:%d]%s : using pinctrl backlight\r\n", __func__, __LINE__, pdp14->data->name);
#endif

return_funcs:
	return ret;
}

static int panel_pinctrl_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct panel_dp14 *dp14;

	dp14 = (struct panel_dp14 *)devm_kzalloc(&pdev->dev, sizeof(*dp14), GFP_KERNEL);
	if (dp14 == NULL) {
		ret = -ENODEV;
		goto return_funcs;
	}

	dp14->dev = &pdev->dev;

	dp14->data = (const struct dp_match_data *)of_device_get_match_data(&pdev->dev);
	if (dp14->data == NULL) {
		ret = -ENODEV;

		goto return_funcs;
	}

	ret = panel_pinctrl_parse_dt(dp14);
	if (ret < 0) {
		dev_err(dp14->dev, "[ERROR][%s:%d]%s : failed to parse device tree\r\n", __func__, __LINE__, dp14->data->name);

		goto return_funcs;
	}

	panel_pinctrl_init_drm_panel(dp14);

	/* Version */
	dp14->major_version = DRIVER_MAJOR;
	dp14->minor_version = DRIVER_MINOR;
	dp14->patchlevel = DRIVER_PATCH;

	dev_info(dp14->dev, "[INFO][%s:%d]%s : %d.%d.%d for %s\r\n", __func__, __LINE__,
				dp14->data->name,
				dp14->major_version, dp14->minor_version, dp14->patchlevel,
				(dp14->dev != NULL) ? dev_name(dp14->dev) : "unknown device");

return_funcs:
	return ret;
}

static int panel_pinctrl_remove(struct platform_device *pdev)
{
	int32_t ret = 0;
	struct panel_dp14 *dp14;

	dp14 = (struct panel_dp14 *)dev_get_drvdata(&pdev->dev);

	devm_pinctrl_put(dp14->st_dp_pins.p);

	drm_panel_remove(&dp14->panel);
	(void)drm_panel_disable(&dp14->panel);

	ret = panel_dpv14_disable(&dp14->panel);

#if defined(CONFIG_DRM_TELECHIPS_CTRL_BACKLIGHT)
	if (dp14->backlight != NULL) {
		/* For KCS */
		put_device(&dp14->backlight->dev);
	}
#endif

	devm_kfree(&pdev->dev, dp14);

	return ret;
}

#ifdef CONFIG_PM
static int panel_pinctrl_suspend(struct device *dev)
{
	const struct panel_dp14 *dp14;

	dp14 = (struct panel_dp14 *)dev_get_drvdata(dev);

	dev_info(dp14->dev, "[INFO][%s:%s] %s \r\n", LOG_DPV14_TAG, dp14->data->name, __func__);

	pinctrl_pm_select_sleep_state(dev);
	return 0;
}

static int panel_pinctrl_resume(struct device *dev)
{
	const struct panel_dp14 *dp14;

	dp14 = (struct panel_dp14 *)dev_get_drvdata(dev);

	dev_err(dp14->dev, "[INFO][%s:%s] %s \r\n", LOG_DPV14_TAG, dp14->data->name, __func__);

	if (panel_dpv14_pin_select_state(dp14->st_dp_pins.p, dp14->st_dp_pins.pwr_port) < 0) {
		/* For KCS */
		dev_warn(dp14->dev, "[WARN][%s:%s] %s failed set pinctrl to pwr_port\r\n", LOG_DPV14_TAG, dp14->data->name, __func__);
	}
	if (panel_dpv14_pin_select_state(dp14->st_dp_pins.p, dp14->st_dp_pins.pwr_off) < 0) {
		/* For KCS */
		dev_warn(dp14->dev, "[WARN][%s:%s] %s failed set pinctrl to pwr_off\r\n", LOG_DPV14_TAG, dp14->data->name, __func__);
	}
	if (panel_dpv14_pin_select_state(dp14->st_dp_pins.p, dp14->st_dp_pins.blk_off) < 0) {
		/* For KCS */
		dev_warn(dp14->dev, "[WARN][%s:%s] %s failed set pinctrl to blk_off\r\n", LOG_DPV14_TAG, dp14->data->name, __func__);
	}

	return 0;
}

static const struct dev_pm_ops panel_pinctrl_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(panel_pinctrl_suspend, panel_pinctrl_resume)
};
#endif


#if defined(CONFIG_DRM_PANEL_MAX968XX)
static int32_t panel_max968xx_get_drv(uint8_t ucI2C_DevAdd, struct panel_max968xx **ppstpanel_max968xx)
{
	bool bfound_drv = (bool)false;
	uint8_t ucdev_add;
	int32_t iret = 0;
	struct panel_max968xx *pst_curr;

	list_for_each_entry(pst_curr, &max968xx_list, list_) {
		if (pst_curr->client == NULL) {
			/* For KCS */
			continue;
		}

		if (!CHECK_I2C_DEV_ADDR(pst_curr->client->addr)) {
			/* For KCS */
			continue;
		}

		ucdev_add = (uint8_t)(pst_curr->client->addr << 1);

		if (ucdev_add == ucI2C_DevAdd) {
			*ppstpanel_max968xx = pst_curr;
			bfound_drv = (bool)true;
			break;
		}
	}

	if (!bfound_drv) {
		dptx_dbg("Failed to get client from dev add 0x%x", ucI2C_DevAdd);
		iret = -ENODEV;
	}

	return iret;
}

/* HIS metric violation (HIS_GOTO) */
static int32_t panel_max968xx_get_board_infor(uint8_t *pucser_rev,
																uint8_t *pucdes_rev,
																uint8_t *pucpower_type)
{
	int32_t iret = 0;
	struct panel_max968xx *pstpanel_max968xx;

	iret = panel_max968xx_get_drv((uint8_t)DP0_PANEL_SER_I2C_DEV_ADD, &pstpanel_max968xx);
	if (iret != 0) {
		dptx_err("Master Ser isn't connnected");

		goto return_funcs;
	}

	*pucser_rev = pstpanel_max968xx->ucser_rev;
	*pucpower_type = pstpanel_max968xx->panel_power_type;

	dptx_debug("Got Ser rev(%u) from device 0xC0", pstpanel_max968xx->ucser_rev);

	iret = panel_max968xx_get_drv((uint8_t)DP0_PANEL_DES_I2C_DEV_ADD, &pstpanel_max968xx);
	if (iret != 0) {
		dptx_err("Master Des isn't connnected");

		goto return_funcs;
	}

	*pucdes_rev = pstpanel_max968xx->ucdes_rev;

	dptx_debug("Got Des rev(%u) from device 0x90", pstpanel_max968xx->ucdes_rev);

return_funcs:
	return iret;
}

static int panel_max968xx_i2c_write(const struct i2c_client *client, unsigned short usRegAdd, unsigned char ucValue)
{
	uint8_t wbuf[3]	= {0,};
	int32_t iret_len, iw_len;
	int32_t iret = 0;

	wbuf[0] = (uint8_t)(usRegAdd >> 8);
	wbuf[1] = (uint8_t)(usRegAdd & 0xFFU);
	wbuf[2] = ucValue;

	iw_len = (SER_DES_I2C_REG_ADD_LEN + SER_DES_I2C_DATA_LEN);

	iret_len = i2c_master_send((const struct i2c_client *)client, (const char *)wbuf, iw_len);
	if (iret_len != iw_len) {
		dptx_err("failed to write dev addr(0x%x) reg addr(0x%x), len %d", (client->addr << 1), usRegAdd, iret_len);
		iret = -ENODEV;
	}

	return iret;
}

static int panel_max968xx_i2c_read(const struct i2c_client *client, uint16_t reg, uint8_t *buff)
{
	uint8_t reg_addr[SER_DES_I2C_REG_ADD_LEN];
	int ret = 0;

	reg_addr[0] = (uint8_t)((uint32_t)reg >> 8);
	reg_addr[1] = (uint8_t)(reg & 0xFFu);

	ret = i2c_master_send(client, (char const *)reg_addr, SER_DES_I2C_REG_ADD_LEN);
	if (ret != SER_DES_I2C_REG_ADD_LEN) {
		/* For KCS */
		ret = -ENODEV;
	} else {
		ret = i2c_master_recv((const struct i2c_client *)client, (char *)buff, 1);
	}

	return ret;
}

static bool panel_max968xx_des_is_locked(uint8_t num_of_ports, uint32_t des_lock_timeout)
{
	const uint8_t des_i2c_dev_add[PANEL_DP_MAX] = {
		DP0_PANEL_DES_I2C_DEV_ADD,
		DP1_PANEL_DES_I2C_DEV_ADD,
		DP2_PANEL_DES_I2C_DEV_ADD,
		DP3_PANEL_DES_I2C_DEV_ADD
	};
	struct panel_max968xx *panel_serdes = NULL;

	uint8_t dptx_daisy_chain_order, deserializer_reg_data;
	ktime_t start_time, ms_delta;

	uint8_t deserializer_check_status = 0;
	uint8_t deserializer_mask = 0;
	int32_t ret = 0;

	bool des_locked = (bool)false;

	if (num_of_ports >= (uint8_t)INPUT_INDEX_MAX) {
		dptx_err("num_of_ports %u is out of range", num_of_ports);
		ret = -EINVAL;
	}
	if (ret == 0) {
		start_time = ktime_get();

		deserializer_mask = (uint8_t)((((uint32_t)1u << num_of_ports) - 1u) & 0xFFu);

		/* Scan for disconnected deserializers.*/
		for (dptx_daisy_chain_order = 0; dptx_daisy_chain_order < num_of_ports; dptx_daisy_chain_order++) {
			ret = panel_max968xx_get_drv(des_i2c_dev_add[dptx_daisy_chain_order], &panel_serdes);
			if (ret < 0) {
				dptx_info("[%s:%d]Des(0x%02x) is not connected",
					   __func__, __LINE__, des_i2c_dev_add[dptx_daisy_chain_order]);
				/* Add disconnected deserializers to the mask to exclude them */
				deserializer_check_status |= (1u << dptx_daisy_chain_order);
			}
		}

		while (deserializer_check_status != deserializer_mask) {
			for (dptx_daisy_chain_order = 0; dptx_daisy_chain_order < num_of_ports; dptx_daisy_chain_order++) {
				if ((((uint32_t)deserializer_check_status >> dptx_daisy_chain_order) & 1u) == 1u) {
					/* For KCS */
					continue;
				}

				ret = panel_max968xx_get_drv(des_i2c_dev_add[dptx_daisy_chain_order], &panel_serdes);
				if (ret < 0) {
					dptx_warn("[%s:%d]Des(0x%02x) is not connected",
						  __func__, __LINE__, des_i2c_dev_add[dptx_daisy_chain_order]);
					break;
				}
				ret =  panel_max968xx_i2c_read(panel_serdes->client, DES_CTRL3, &deserializer_reg_data);
				if ((ret == 1) && ((deserializer_reg_data & 0x08u) != 0u)) {
					/* For KCS */
					deserializer_check_status |= (uint8_t)(((uint32_t)1u << dptx_daisy_chain_order) & 0xFFu);
				}
			}
			mdelay(1);
			/**
			 * Using ktime_get() provides nanosecond precision and avoids the
			 * Year 2038 problem associated with timeval structures.
			 * The s64 type used by ktime_get() can represent time durations
			 * of approximately 292 years, making overflow unlikely in
			 * typical scenarios.
			 */
			ms_delta = ktime_ms_delta(ktime_get(), start_time);
			if (ms_delta > (ktime_t)des_lock_timeout) {
				dptx_warn("ms_delta %lldums is timeout", ms_delta);
				break;
			}
		}
		if (deserializer_check_status == deserializer_mask) {
			/* For KCS */
			des_locked = (bool)true;
		}
	}

	return des_locked;
}

/**
 * Review whether I2C configuration is required based on the power type,
 * serializer revision, and deserializer revision included in the I2C data,
 * and assign the appropriate panel_serdes for the I2C device address to which
 * the data will be written.
 */
static int32_t panel_max968xx_get_panel_serdes(const struct panel_max968xx_reg_data *panel_serdes_reg_data,
					       struct panel_max968xx **panel_serdes)
{
	uint8_t i2c_dev_add;
	int32_t ret = 0;

	if ((panel_serdes_reg_data == NULL) ||
	    (panel_serdes == NULL)) {
		if (panel_serdes_reg_data == NULL) {
			/* For KCS */
			dptx_err("panel_serdes_reg_data is NULL");
		} else {
			dptx_err("panel_serdes is NULL");
		}
		ret = -EINVAL;
	}
	if (ret == 0) {
		if (panel_serdes_reg_data->uidev_addr >= 0xFFu) {
			dptx_err("i2c device address 0x%x is out of range",
				 panel_serdes_reg_data->uidev_addr);
			ret = -EINVAL;
		}
	}
	if (ret == 0) {
		i2c_dev_add = (uint8_t)(panel_serdes_reg_data->uidev_addr & 0xFFu);

		ret = panel_max968xx_get_drv(i2c_dev_add, panel_serdes);
	}

	return ret;
}

/**
 * Review whether I2C configuration is required based on the power type,
 * serializer revision, and deserializer revision included in the I2C data
 */
static int32_t panel_max968xx_check_delay_lock(struct serdes_write_config *write_config,
					       const struct panel_max968xx_reg_data *panel_serdes_reg_data)
{
	unsigned long wait_ms;
	int32_t ret = 0;

	if ((write_config == NULL) ||
	    (panel_serdes_reg_data == NULL)) {
		if (write_config == NULL) {
			/* For KCS */
			dptx_err("write_config is NULL");
		} else {
			dptx_err("panel_serdes_reg_data is NULL");
		}
		ret = -EINVAL;
	}
	if (ret == 0) {
		if (write_config->serializer_reset_mode) {
			if (panel_serdes_reg_data->uidev_addr !=
				(uint32_t)DP0_PANEL_SER_I2C_DEV_ADD) {
				/* For KCS */
				write_config->action_flag = 1u;
			}
		}
	}
	if ((ret == 0) && (write_config->action_flag == 0u)) {
		if (panel_serdes_reg_data->uidev_addr == (uint32_t)MAX968XX_DELAY_ADDR) {
			wait_ms = (unsigned long)panel_serdes_reg_data->uireg_val;
			mdelay(wait_ms);
			write_config->action_flag = 1;
		}
	}
	if ((ret == 0) && (write_config->action_flag == 0u)) {
		if (panel_serdes_reg_data->uidev_addr == (uint32_t)MAX968XX_WATCH_LOCK_PIN_ADDR) {
			if (!panel_max968xx_des_is_locked(write_config->num_of_ports,
							  panel_serdes_reg_data->uireg_val)) {
				dptx_err("Des Lock pin is not locked");
				/**
				 * Even though an error is logged, ret is set
				 * to 0 to allow the process to continue.
				 */
				ret = 0;
			}
			write_config->action_flag = 1;
		}
	}

	return ret;
}

static int32_t panel_max968xx_check_hardware(struct serdes_write_config *write_config,
					     const struct panel_max968xx_reg_data *panel_serdes_reg_data)
{
	int32_t ret = 0;

	if ((write_config == NULL) ||
	    (panel_serdes_reg_data == NULL)) {
		if (write_config == NULL) {
			/* For KCS */
			dptx_err("write_config is NULL");
		} else {
			dptx_err("panel_serdes_reg_data is NULL");
		}
		ret = -EINVAL;
	}
	if (ret == 0) {
		if ((panel_serdes_reg_data->panel_power_type != (uint8_t)TCC_EVB_LCD_POW_MAX) &&
		    (panel_serdes_reg_data->panel_power_type != write_config->panel_power_type)) {
			/* For KCS */
			write_config->action_flag = 1;
		}
	}
	if ((ret == 0) && (write_config->action_flag == 0u)) {
		if ((panel_serdes_reg_data->ucser_rev != (uint8_t)SER_REV_ALL) &&
		    (panel_serdes_reg_data->ucser_rev != write_config->ser_rev)) {
			/* For KCS */
			write_config->action_flag = 1;
		}
	}
	if ((ret == 0) && (write_config->action_flag == 0u)) {
		if (write_config->panel_serdes->ucmst_mode == 0u) {
			if ((panel_serdes_reg_data->uireg_addr == (uint32_t)DES_DROP_VIDEO) ||
			(panel_serdes_reg_data->uireg_addr == (uint32_t)DES_STREAM_SELECT)) {
				/* For KCS */
				write_config->action_flag = 1;
			}
		}
	}
	if ((ret == 0) && (write_config->action_flag == 0u)) {
		if (!write_config->ser_swap_usb_to_dp_needed) {
			if ((panel_serdes_reg_data->uireg_addr == (uint32_t)SER_LANE_REMAP_B0) ||
			    (panel_serdes_reg_data->uireg_addr == (uint32_t)SER_LANE_REMAP_B1)) {
				/* For KCS */
				write_config->action_flag = 1;
			}
		}
	}
	return ret;
}

static int32_t panel_max968xx_check_continue(struct serdes_write_config *write_config,
					     const struct panel_max968xx_reg_data *panel_serdes_reg_data)
{
	int32_t ret = 0;

	if ((write_config == NULL) || (write_config->panel_serdes_reg_data == NULL)) {
		if (write_config == NULL) {
			/* For KCS */
			dptx_err("write_config is NULL");
		} else {
			dptx_err("write_config->panel_serdes_reg_data is NULL");
		}
		ret = -EINVAL;
	} else {
		ret = panel_max968xx_check_delay_lock(write_config, panel_serdes_reg_data);
		if ((ret == 0) && (write_config->action_flag == 0u)) {
			ret = panel_max968xx_get_panel_serdes(panel_serdes_reg_data,
							      &write_config->panel_serdes);
			if (ret < 0) {
				/**
				 * The register table for initializing this SerDes
				 * includes configuration values for all possible
				 * connected deserializers. Therefore, if panel_max968xx_get_panel_serdes
				 * fails, it is considered that the deserializer
				 * is not connected and handled with a continue.
				 */
				write_config->action_flag = 1u;
				ret = 0;
			}
		}
		if ((ret == 0) && (write_config->action_flag == 0u)) {
			/* For KCS */
			ret = panel_max968xx_check_hardware(write_config,
							    panel_serdes_reg_data);
		}
	}
	return ret;
}

static int32_t panel_max968xx_check_wdata(struct serdes_write_config *write_config,
					 const struct panel_max968xx_reg_data *panel_serdes_reg_data)
{
	const struct panel_max968xx *panel_serdes;

	int32_t ret = 0;

	uint8_t panel_vcp_id, dptx_daisy_chain_order;

	bool write_data_updated = (bool)false;

	if ((write_config == NULL) ||
	    (write_config->panel_serdes == NULL) ||
	    (panel_serdes_reg_data == NULL)) {
		if (write_config == NULL) {
			/* For KCS */
			dptx_err("write_config is NULL");
		} else if (write_config->panel_serdes == NULL) {
			dptx_err("write_config->panel_serdes is NULL");
		} else {
			dptx_err("panel_serdes_reg_data is NULL");
		}
		ret = -EINVAL;
	} else {

		panel_serdes = write_config->panel_serdes;

		write_config->wdata = (uint8_t)(panel_serdes_reg_data->uireg_val & 0xFFu);
		if ((panel_serdes->ucmst_mode == 0U) &&
		    (panel_serdes_reg_data->uireg_addr == (uint32_t)SER_MISC_CONFIG_B1)) {
			dptx_debug("Seting to SST...");
			write_config->wdata = MST_FUNCTION_DISABLE;
			write_data_updated = (bool)true;
		}
		if (!write_data_updated) {
			if (panel_serdes_reg_data->uireg_addr == SER_MAX_LINK_RATE) {
				switch (write_config->link_rate) {
				case SER_LINK_RATE_HBR:
					write_config->wdata = 0x0A;
					break;
				case SER_LINK_RATE_HBR2:
					write_config->wdata = 0x14;
					break;
				case SER_LINK_RATE_HBR3:
					write_config->wdata = 0x1E;
					break;
				default:
					dptx_err("link_rate %d is out of range", write_config->link_rate);
					ret = -EINVAL;
					break;
				}
				write_data_updated = (bool)true;
			}
		}
	}
	if (ret == 0) {
		if (!write_data_updated) {
			if (write_config->ser_swap_usb_to_dp_needed) {
				if (panel_serdes_reg_data->uireg_addr == (uint32_t)SER_LANE_REMAP_B0) {
					write_config->wdata = PHY_LANE_IDX_2 & 0x3u;
					write_config->wdata |= (((uint32_t)PHY_LANE_IDX_3 & 0x3u) << 2);
					write_config->wdata |= (((uint32_t)PHY_LANE_IDX_0 & 0x3u) << 4);
					write_config->wdata |= (((uint32_t)PHY_LANE_IDX_1 & 0x3u) << 6);

					(void)dptx_info("[%s:%d]Lane swap as 0x%x\n",
						      __func__, __LINE__, write_config->wdata);
					write_data_updated = (bool)true;
				}
				if (panel_serdes_reg_data->uireg_addr == (uint32_t)SER_LANE_REMAP_B1) {
					/* For KCS */
					write_config->wdata = 0x01u;
					write_data_updated = (bool)true;
				}
			}
		}
		if (!write_data_updated) {
			if (panel_serdes_reg_data->uireg_addr == (uint32_t)DES_STREAM_SELECT) {
				switch (panel_serdes_reg_data->uidev_addr) {
				case DP0_PANEL_DES_I2C_DEV_ADD:
					panel_vcp_id = panel_serdes->aucvcp_id[0];
					dptx_daisy_chain_order = 0u;
					break;
				case DP1_PANEL_DES_I2C_DEV_ADD:
					panel_vcp_id = panel_serdes->aucvcp_id[1];
					dptx_daisy_chain_order = 1u;
					break;
				case DP2_PANEL_DES_I2C_DEV_ADD:
					panel_vcp_id = panel_serdes->aucvcp_id[2];
					dptx_daisy_chain_order = 2u;
					break;
				case DP3_PANEL_DES_I2C_DEV_ADD:
					panel_vcp_id = panel_serdes->aucvcp_id[3];
					dptx_daisy_chain_order = 3u;
					break;
				default:
					dptx_err("Invalid i2c dev add as 0x%x\n", panel_serdes_reg_data->uidev_addr);
					panel_vcp_id = 0U;
					dptx_daisy_chain_order = 0u;
					break;
				}
				if ((panel_vcp_id == 0u) || (panel_vcp_id > 4u)) {
					dptx_err("panel_vcp_id %u is out of range, please check a panel_vcp_id", panel_vcp_id);
					ret = -EINVAL;
				} else {
					write_config->wdata = (panel_vcp_id - 1u);
					dptx_info("Set VCP id %d to DP %u", write_config->wdata, dptx_daisy_chain_order);
					write_data_updated = (bool)true;
				}
			}
		}
	}
	if (ret == 0) {
		if (!write_data_updated) {
			if ((panel_serdes_reg_data->panel_power_type == (uint8_t)TCC_EVB_LCD_ONE_POW) &&
			    (panel_serdes_reg_data->uireg_addr == SER_PT_0)) {
				if (panel_serdes->ucmst_mode == 0u) {
					dptx_debug("\n[%s:%d] Serializer Enable only I2C PT 1(1Des)\n", __func__, __LINE__);

					/* TCC8059 SST - Enable PT1(1Des) */
					write_config->wdata = 0x01;
				} else {
					dptx_debug("\n[%s:%d]Serializer enables I2C PT1(1 Des), PT2(1Des)\n", __func__, __LINE__);

					/* TCC8050 SST/2MST - Enable PT1(1Des), PT2(1Des) */
					write_config->wdata = 0x03;
				}
			}
		}
	}
	return ret;
}

static int32_t panel_max968xx_parse_dt(struct panel_max968xx *pstpanel_max968xx)
{
	uint8_t dptx_daisy_chain_order;
	uint32_t uievb_type;
	uint32_t auivcp_id[PHY_INPUT_STREAM_MAX];
	int32_t iret = 0;
	const struct device_node *dn;

	dn = of_find_compatible_node(NULL, NULL, "telechips,max968xx_configuration");
	if (dn == NULL) {
		/* For KCS */
		dptx_warn("Can't find SerDes node\n");
	}

	iret = of_property_read_u32(dn, "max968xx_evb_type", &uievb_type);
	if (iret < 0) {
		/* For KCS */
		dptx_warn("Can't get EVB type.. set to TCC8050_SV_10(default)");
		uievb_type = (u32)TCC8050_SV_10;
	}

	pstpanel_max968xx->ucevb_type = (uint8_t)(uievb_type & 0xFFU);

	switch (pstpanel_max968xx->ucevb_type) {
	case TCC8059_EVB_01:
		pstpanel_max968xx->panel_power_type = (uint8_t)TCC_EVB_LCD_ONE_POW;
		break;
	case TCC8050_SV_01:
	case TCC8050_SV_10:
	case TCC8070_SV_01:
	default:
		pstpanel_max968xx->panel_power_type = (uint8_t)TCC_EVB_LCD_FOUR_POW;
		break;
	}

	dn = of_find_compatible_node(NULL, NULL, "telechips,dpv14-tx");
	if (dn == NULL) {
		/* For KCS */
		dptx_warn("Can't find dpv14-tx node\n");
	}

	iret = of_property_read_u32_array(dn, "sink_vcp_id", auivcp_id, (size_t)PHY_INPUT_STREAM_MAX);
	if (iret < 0) {
		dptx_err("Can't get Sink VCP Id.. set to default");
		for (dptx_daisy_chain_order = 0; dptx_daisy_chain_order < (uint8_t)PHY_INPUT_STREAM_MAX; dptx_daisy_chain_order++) {
			/* For KCS */
			pstpanel_max968xx->aucvcp_id[dptx_daisy_chain_order] = (dptx_daisy_chain_order + 1U);
		}
	} else {
		for (dptx_daisy_chain_order  = 0; dptx_daisy_chain_order < (uint8_t)PHY_INPUT_STREAM_MAX; dptx_daisy_chain_order++) {
			if (auivcp_id[dptx_daisy_chain_order] >= 255u) {
				pstpanel_max968xx->aucvcp_id[dptx_daisy_chain_order] = 255u;
				dptx_err("pstpanel_max968xx->aucvcp_id[dptx_daisy_chain_order] value over uint8_max");
			} else {
				pstpanel_max968xx->aucvcp_id[dptx_daisy_chain_order] = (uint8_t)auivcp_id[dptx_daisy_chain_order];
			}
		}
	}

	return 0;
}

/* HIS metric violation (HIS_GOTO) */
static int32_t panel_max968xx_init_list(uint8_t *pucnum_of_list,
							struct panel_max968xx **ppstpanel_max968xx)
{
	uint8_t ucnum_of_list = 0;
	int32_t iret = 0;
	struct panel_max968xx *pstpanel_max968xx;
	const struct panel_max968xx *pst_curr;

	pstpanel_max968xx = (struct panel_max968xx *)kzalloc(sizeof(struct panel_max968xx), GFP_KERNEL);
	if (pstpanel_max968xx == NULL) {
		dptx_err("can't alloc device mem");

		iret = -ENOMEM;

		goto return_funcs;
	}

	list_add(&pstpanel_max968xx->list_, &max968xx_list);

	list_for_each_entry(pst_curr, &max968xx_list, list_) {
		ucnum_of_list++;

		if (ucnum_of_list >= (uint8_t)INPUT_INDEX_MAX) {
			/* For KCS */
			break;
		}
	}

	*ppstpanel_max968xx = pstpanel_max968xx;
	*pucnum_of_list = ucnum_of_list;

return_funcs:
	return iret;
}

/* HIS metric violation (HIS_GOTO) */
int panel_max968xx_get_topology(uint8_t *num_of_ports)
{
	uint8_t ucnum_of_ports = 0;
	int32_t iret = 0;
	struct panel_max968xx *pst_curr;

	if (num_of_ports == NULL) {
		dptx_err("Err [%s:%d]num_of_ports == NULL", __func__, __LINE__);
		iret = -EINVAL;

		goto return_funcs;
	}

	list_for_each_entry(pst_curr, &max968xx_list, list_) {
		if (pst_curr->client == NULL) {
			/* For KCS */
			continue;
		}

		ucnum_of_ports++;

		if (ucnum_of_ports >= (uint8_t)INPUT_INDEX_MAX) {
			/* For KCS */
			break;
		}
	}

	ucnum_of_ports = (ucnum_of_ports >= (uint8_t)DES_INPUT_INDEX_0) ? (ucnum_of_ports - 1U):0U;

	list_for_each_entry(pst_curr, &max968xx_list, list_) {
		/* For KCS */
		pst_curr->ucmst_mode = (ucnum_of_ports > 1U) ? 1U:0U;
	}

	*num_of_ports = ucnum_of_ports;

return_funcs:
	return iret;
}
EXPORT_SYMBOL(panel_max968xx_get_topology);

static int panel_max968xx_check_and_write_regs(struct serdes_write_config *write_config)
{
	uint32_t reg_index, i2c_retry;
	int32_t ret = 0;

	if ((write_config == NULL) || (write_config->panel_serdes_reg_data == NULL)) {
		if (write_config == NULL) {
			/* For KCS */
			dptx_err("write_config is NULL");
		} else {
			dptx_err("write_config->panel_serdes_reg_data is NULL");
		}
		ret = -EINVAL;
	} else {
		for (reg_index = 0; ((write_config->panel_serdes_reg_data[reg_index].uidev_addr != 0U) &&
		     (write_config->panel_serdes_reg_data[reg_index].uireg_addr != 0U)); reg_index++) {
			/* reset action flag */
			write_config->action_flag = 0u;
			ret = panel_max968xx_check_continue(write_config,
							    &write_config->panel_serdes_reg_data[reg_index]);

			if (ret == 0) {
				if (write_config->action_flag == 1u) {
					/* For KCS */
					continue;
				}
			}
			if (ret == 0) {
				/* For KCS */
				ret = panel_max968xx_check_wdata(write_config,
								 &write_config->panel_serdes_reg_data[reg_index]);
			}
			if (ret == 0) {
				for (i2c_retry = 0u; i2c_retry < MAX_RETRY_I2C_RW; i2c_retry++) {
					ret = panel_max968xx_i2c_write(write_config->panel_serdes->client,
								(uint16_t)write_config->panel_serdes_reg_data[reg_index].uireg_addr,
								write_config->wdata);
					if (ret == 0) {
						/* For KCS */
						break;
					}
				}
			}
			if (ret < 0) {
				dptx_err("I2C writting fail on Dev(%d), Reg(%d), reg_index(%u)",
						write_config->panel_serdes_reg_data[reg_index].uidev_addr,
						write_config->panel_serdes_reg_data[reg_index].uireg_addr,
						reg_index);
				break;
			}
		}
	}
	if (ret == 0) {
		/* For KCS */
		dptx_info("SerDes Resisters are successfully done!!!");
	}
	return ret;
}

/**
 * @brief Resets the MAX968XX panel configuration.
 *
 * This function resets the MAX968XX serializer and optionally the deserializer,
 * based on the provided parameters. It also configures the serializer's link speed.
 *
 * @param serializer_reset_mode If `true`, resets only the serializer;
 *                              if `false`, resets both serializer and deserializer.
 * @param dp_phy_std_mode       If `true`, standard DisplayPort mode is used;
 *                              if `false`, indicates that DP PHY is in USB-C mode
 *                              Note: USB-C mode is not supported on this chipset
 * @param link_rate             The link speed for the serializer. The values correspond to:
 *                              - `0`: RBR (Reduced Bit Rate)
 *                              - `1`: HBR (High Bit Rate)
 *                              - `2`: HBR2 (High Bit Rate 2)
 *                              - `3`: HBR3 (High Bit Rate 3)
 *
 * @return 0 on success, negative error code on failure.
 */
int panel_max968xx_reset(bool serializer_reset_mode, bool dp_phy_std_mode, uint8_t link_rate)
{
	struct serdes_write_config write_config;
	int32_t ret = 0;

	if ((link_rate == SER_LINK_RATE_RBR) || (link_rate > SER_LINK_RATE_HBR3)) {
		dptx_err("max968x does not support link rate (%u)\n", link_rate);
		ret = -EINVAL;
	}
	if (ret == 0) {
		(void)memset(&write_config, 0, sizeof(write_config));

		write_config.serializer_reset_mode = serializer_reset_mode;
		if (!dp_phy_std_mode) {
			/**
			 * If DP PHY is not in standard mode, set the flag to indicate that
			 * the serializer needs to perform a USB-C to standard DP lane swap correction.
			 */
			write_config.ser_swap_usb_to_dp_needed = (bool)true;
		}
		ret = panel_max968xx_get_topology(&write_config.num_of_ports);
	}
	if (ret == 0) {
		if (write_config.num_of_ports == 0u) {
			dptx_warn("No panels connected, DisplayPort topology shows panel 0.\n");
			ret = -ENODEV;
		}
	}
	if (ret == 0) {
		/* For KCS */
		ret = panel_max968xx_get_board_infor(&write_config.ser_rev,
						     &write_config.des_rev,
						     &write_config.panel_power_type);
	}
	if (ret == 0) {
		/* Set the register table according to the deserializer version */
		if (write_config.des_rev == (uint8_t)DES_REV_ES2) {
			write_config.panel_serdes_reg_data = stPanel_1027_DesES2_RegVal;

			dptx_info("Writing DES ES2 Tables, Ser Rev(%u)<->Des Rev(%u), Lane swap(%d)",
				  write_config.ser_rev, write_config.des_rev, dp_phy_std_mode);
		} else {
			write_config.panel_serdes_reg_data = stPanel_1027_DesES3_RegVal;

			dptx_info("Writing DES ES3 Tables, Ser Rev(%u)<->Des Rev(%u), Lane swap(%d)",
				  write_config.ser_rev, write_config.des_rev, dp_phy_std_mode);
		}
		if ((write_config.ser_rev == SER_REV_ES2) && (link_rate > SER_LINK_RATE_HBR)) {
			/* The serializer ES2 only supports HBR. */
			link_rate = SER_LINK_RATE_HBR;
		}
		write_config.link_rate = link_rate;

		ret = panel_max968xx_check_and_write_regs(&write_config);
	}
	return ret;
}
EXPORT_SYMBOL(panel_max968xx_reset);

/* HIS metric violation (HIS_GOTO) */
static int panel_max968xx_i2c_probe(struct i2c_client *client, const struct i2c_device_id *dev_id)
{
	bool bdev_connected = (bool)true;
	uint8_t ucaddr_buf[2] = {0,};
	uint8_t ucdev_add, ucdata_buf = 0, ucnum_of_list = 0, ucnum_of_ports;
	int32_t iret_len, iret = 0;
	struct panel_max968xx *pstpanel_max968xx;

	(void)dev_id;

	iret = panel_max968xx_init_list(&ucnum_of_list, &pstpanel_max968xx);
	if (iret != 0) {
		/* For KCS */
		goto return_funcs;
	}

	if (!CHECK_I2C_DEV_ADDR(client->addr)) {
		/* For KCS */
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
		iret_len = i2c_master_recv((const struct i2c_client *)client,
									(char *)&ucdata_buf,
									1);
		if (iret_len != 1) {
			/* For KCS */
			bdev_connected = (bool)false;
		}
	}

	pstpanel_max968xx->client = bdev_connected ? client:NULL;

	pstpanel_max968xx->ucser_rev = (ucdev_add == (uint8_t)DP0_PANEL_SER_I2C_DEV_ADD) ?
										ucdata_buf:0U;
	pstpanel_max968xx->ucdes_rev = (ucdev_add == (uint8_t)DP0_PANEL_SER_I2C_DEV_ADD) ?
										0U:ucdata_buf;

	(void)panel_max968xx_parse_dt(pstpanel_max968xx);

	if (ucnum_of_list == (uint8_t)INPUT_INDEX_MAX) {
		/* For KCS */
		(void)panel_max968xx_get_topology(&ucnum_of_ports);
	}

	i2c_set_clientdata(client, pstpanel_max968xx);

	if (ucnum_of_list == (uint8_t)DES_INPUT_INDEX_0) {
		/* For KCS */
		dptx_info("TCC-DRM-DP-Panel Ver %d.%d.%d", DRIVER_MAJOR, DRIVER_MINOR, DRIVER_PATCH);
	}
	dptx_info(" [%d]%s: %s I2C name(%s), Add(0x%x), Rev(%d) -> %s",
		  ucnum_of_list,
		  (pstpanel_max968xx->ucevb_type == (uint8_t)TCC8059_EVB_01) ? "TCC8059 EVB" :
		  (pstpanel_max968xx->ucevb_type == (uint8_t)TCC8050_SV_01) ? "TCC8050/3 sv0.1" :
		  (pstpanel_max968xx->ucevb_type == (uint8_t)TCC8050_SV_10) ? "TCC8050/3 sv1.0" :
		  (pstpanel_max968xx->ucevb_type == (uint8_t)TCC8070_SV_01) ? "TCC8070 sv0.1" : "Unknown",
		  (ucdev_add == (uint8_t)DP0_PANEL_SER_I2C_DEV_ADD) ? "Ser" : "Des",
		  client->name, ucdev_add, ucdata_buf, (bdev_connected) ? "connected" : "not connected");
	if (ucnum_of_list == (uint8_t)INPUT_INDEX_MAX) {
		/* For KCS */
		dptx_info(" Num of DPs: %u - %s mode ", ucnum_of_ports, (pstpanel_max968xx->ucmst_mode != 0u) ? "MST" : "SST");
	}

return_funcs:
	return 0;
}

static int panel_max968xx_i2c_remove(struct i2c_client *client)
{
	struct panel_max968xx *pst_curr;

	list_for_each_entry(pst_curr, &max968xx_list, list_) {
		/* For KCS */
		list_del(&pst_curr->list_);
	}

	list_del_init(&max968xx_list);

	return 0;
}
#endif /* #if defined(CONFIG_DRM_PANEL_MAX968XX) */

static const struct of_device_id panel_pinctrl_of_table[] = {
	{ .compatible = "telechips,drm-lvds-dpv14-0",
	  .data = &dpv14_panel_0,
	},
	{ .compatible = "telechips,drm-lvds-dpv14-1",
	  .data = &dpv14_panel_1,
	},
	{ .compatible = "telechips,drm-lvds-dpv14-2",
	  .data = &dpv14_panel_2,
	},
	{ .compatible = "telechips,drm-lvds-dpv14-3",
	  .data = &dpv14_panel_3,
	},
	{ /* Sentinel */ },
};
MODULE_DEVICE_TABLE(of, panel_pinctrl_of_table);

static struct platform_driver pintctrl_driver = {
	.probe		= panel_pinctrl_probe,
	.remove		= panel_pinctrl_remove,
	.driver		= {
		.name	= "panel-dpv14",
#ifdef CONFIG_PM
		.pm	= &panel_pinctrl_pm_ops,
#endif
		.of_match_table = panel_pinctrl_of_table,
	},
};


#if defined(CONFIG_DRM_PANEL_MAX968XX)
static const struct of_device_id max968xx_match[] = {
	{.compatible = "maxim,serdes"},
	{},
};
MODULE_DEVICE_TABLE(of, max968xx_match);


static const struct i2c_device_id max968xx_id[] = {
	{ "Max968XX", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, max968xx_id);


static struct i2c_driver max968xx_i2c_drv = {
	.probe = panel_max968xx_i2c_probe,
	.remove = panel_max968xx_i2c_remove,
	.id_table = max968xx_id,
	.driver = {
		.name = "telechips,Max96851_78",
		.owner = THIS_MODULE,

#if defined(CONFIG_OF)
		.of_match_table = of_match_ptr(max968xx_match),
#endif
	},
};

static int __init panel_dpv14_init(void)
{
	int ret = 0;

	INIT_LIST_HEAD(&max968xx_list);

	ret = i2c_add_driver(&max968xx_i2c_drv);
	if (ret != 0) {
		/* For KCS */
		dptx_err("Max96851_78 I2C registration failed %d\n", ret);
	}

	ret = platform_driver_register(&pintctrl_driver);

	return ret;
}
module_init(panel_dpv14_init);

static void __exit panel_dpv14_exit(void)
{
	i2c_del_driver(&max968xx_i2c_drv);

	return platform_driver_unregister(&pintctrl_driver);
}
module_exit(panel_dpv14_exit);

#else
module_platform_driver(pintctrl_driver);

#endif /* #if defined(CONFIG_DRM_PANEL_MAX968XX) */

MODULE_DESCRIPTION("DRM DP V1.4 Panel Driver");
MODULE_LICENSE("GPL");
