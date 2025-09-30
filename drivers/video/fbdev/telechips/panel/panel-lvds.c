// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/backlight.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <uapi/linux/media-bus-format.h>

#include <video/display_timing.h>
#include <video/of_videomode.h>
#include <video/of_display_timing.h>
#include <video/telechips/vioc_lvds.h>
#include <video/videomode.h>
#include <panel_helper.h>

#define LOG_TAG "FBP_LVDS"

//#define LVDS_DEBUG
#ifdef LVDS_DEBUG
#define LVDS_DBG(fmt, args...) pr_info("[INFO][FBLVDS] " fmt, ## args)
#else
#define LVDS_DBG(fmt, args...)
#endif

#define TCC_LVDS_OUTPUT_VESA24 0
#define TCC_LVDS_OUTPUT_JEIDA24 1
#define TCC_LVDS_OUTPUT_MAX 2


#define DRIVER_DATE	"20240514"
#define DRIVER_MAJOR	1
#define DRIVER_MINOR	0
#define DRIVER_PATCH	1

struct lvds_match_data {
	const char *name;
};


static const struct lvds_match_data lvds_auoc123han06 = {
	.name = "AUOC123HAN06",
};

static const struct lvds_match_data lvds_tm123xdhp90 = {
	.name = "TM123XDHP90",
};

static const struct lvds_match_data lvds_fld0800 = {
	.name = "FLD0900",
};

static unsigned int
lvds_outformat[TCC_LVDS_OUTPUT_MAX][TXOUT_MAX_LINE][TXOUT_DATA_PER_LINE] = {
	/* LVDS vesa-24 format */
	{
		{TXOUT_G_D(0), TXOUT_R_D(5), TXOUT_R_D(4), TXOUT_R_D(3),
			TXOUT_R_D(2), TXOUT_R_D(1), TXOUT_R_D(0)},
		{TXOUT_B_D(1), TXOUT_B_D(0), TXOUT_G_D(5), TXOUT_G_D(4),
			TXOUT_G_D(3), TXOUT_G_D(2), TXOUT_G_D(1)},
		{TXOUT_DE, TXOUT_VS, TXOUT_HS, TXOUT_B_D(5),
			TXOUT_B_D(4), TXOUT_B_D(3), TXOUT_B_D(2)},
		{TXOUT_DUMMY, TXOUT_B_D(7), TXOUT_B_D(6), TXOUT_G_D(7),
			TXOUT_G_D(6), TXOUT_R_D(7), TXOUT_R_D(6)}
	},
	/* LVDS jeida-24 format , not implemented*/
};

struct lvds_pin {
	struct pinctrl *p;
	struct pinctrl_state *default0;
	struct pinctrl_state *pwr_on_1;
	struct pinctrl_state *pwr_on_2;
	struct pinctrl_state *blk_on;
	struct pinctrl_state *blk_off;
	struct pinctrl_state *pwr_off;
};

struct panel_lvds {
	struct fb_panel panel;
	struct device *dev;

	const char *label;
	unsigned int width;
	unsigned int height;
	unsigned int bus_format;
	struct videomode video_mode;

	#if defined(CONFIG_TELECHIPS_FB_CTRL_BACKLIGHT)
	struct backlight_device *backlight;
	#endif

	struct gpio_desc *enable_gpio;
	struct gpio_desc *reset_gpio;

	/* Device driver data */
	struct lvds_match_data *data;

	/* LVDS data from device tree */
	lvds_hw_info_t tcc_lvds_hw;

	struct lvds_pin lvds_pins;
	int enabled;

 	/*
	 * Some panels, such as the AUO LVDS panel, must first transmit an
	 * LVDS signal when initializing the panel and then turn on the
	 * backlight after a certain period of time.
	 * The blk-wait-time is used to determine the waiting time until the
	 * backlight is turned on after a signal is transmitted to the
	 * LVDS panel.
	 */
	uint32_t blk_wait_time;
};

static int panel_tcc_pin_select_state(struct pinctrl *p, struct pinctrl_state *s)
{
	int ret = 0;

	if ((p == NULL) || (s == NULL)) {
		ret  = -EINVAL;
	} else {
		ret = pinctrl_select_state(p, s);
	}
	return ret;
}

static inline struct panel_lvds *to_panel_lvds(struct fb_panel *panel)
{
	return container_of(panel, struct panel_lvds, panel);
}

static int panel_lvds_disable(struct fb_panel *panel)
{
	struct panel_lvds *lvds = to_panel_lvds(panel);
	unsigned int ts_mux_id = 0;

	dev_dbg(lvds->dev, "[DEBUG][%s] %s with %s\n",
		LOG_TAG, __func__, lvds->data->name);

	if (lvds->enabled != 0) {
		#if defined(CONFIG_TELECHIPS_FB_CTRL_BACKLIGHT)
		if (lvds->backlight != NULL) {
			lvds->backlight->props.power = FB_BLANK_POWERDOWN;
			lvds->backlight->props.state |= BL_CORE_FBBLANK;
			(void)backlight_update_status(lvds->backlight);
		} else {
			(void)pr_err("[ERROR][%s] : backlight driver not valid\n",
			       __func__);
		}
		#else
		if (
		    panel_tcc_pin_select_state(
					       lvds->lvds_pins.p,
					       lvds->lvds_pins.blk_off) < 0) {
			dev_warn(lvds->dev,
				 "[WARN][%s:%s] %s failed set pinctrl to blk_off\r\n",
				 LOG_TAG, lvds->data->name, __func__);
		}
		#endif

		if (lvds->tcc_lvds_hw.lvds_type ==
				(unsigned int)PANEL_LVDS_DUAL) {
			ts_mux_id = (unsigned int)TS_MUX_IDX0;
		} else {
			if (lvds->tcc_lvds_hw.ts_mux_id >= 0) {
				ts_mux_id =
					(unsigned int)lvds->
					tcc_lvds_hw.ts_mux_id;
			}
		}

		LVDS_WRAP_ResetPHY(ts_mux_id, 1);

		lvds->enabled = 0;
	} else {
		dev_dbg(lvds->dev, "[DEBUG][%s] %s with %s - already disabled\n",
			LOG_TAG, __func__, lvds->data->name);
	}

	return 0;
}

static int panel_lvds_unprepare(struct fb_panel *panel)
{
	const struct panel_lvds *lvds = to_panel_lvds(panel);

	dev_dbg(lvds->dev, "[DEBUG][%s] %s with %s\n",
		LOG_TAG, __func__, lvds->data->name);
	if (panel_tcc_pin_select_state(lvds->lvds_pins.p,
				       lvds->lvds_pins.pwr_off) < 0) {
		dev_warn(lvds->dev,
				"[WARN][%s:%s] %s failed set pinctrl to pwr_off\r\n",
				LOG_TAG, lvds->data->name, __func__);
	}
	return 0;
}

/* HIS_CALLS */
static int panel_lvds_prepare(struct fb_panel *panel)
{
	struct panel_lvds *lvds = to_panel_lvds(panel);
	unsigned int ts_mux_id = 0;

	dev_dbg(lvds->dev, "[DEBUG][%s] %s with %s\n",
		LOG_TAG, __func__, lvds->data->name);

	if (lvds->enabled == 0) {
		if (
			panel_tcc_pin_select_state(
				lvds->lvds_pins.p,
				lvds->lvds_pins.pwr_on_1) < 0) {
			dev_warn(lvds->dev,
				"[WARN][%s:%s] %s failed set pinctrl to pwr_on_1\r\n",
				LOG_TAG, lvds->data->name, __func__);
		}

		mdelay(1);

		if (
			panel_tcc_pin_select_state(
				lvds->lvds_pins.p,
				lvds->lvds_pins.pwr_on_2) < 0) {
			dev_warn(lvds->dev,
				"[WARN][%s:%s] %s failed set pinctrl to pwr_on_2\r\n",
				LOG_TAG, lvds->data->name, __func__);
		}

		if (lvds->tcc_lvds_hw.lvds_type ==
				(unsigned int)PANEL_LVDS_DUAL) {
			ts_mux_id = (unsigned int)TS_MUX_IDX0;
		} else {
			if (lvds->tcc_lvds_hw.ts_mux_id >= 0) {
				ts_mux_id =
					(unsigned int)lvds->
					tcc_lvds_hw.ts_mux_id;
			}
		}

		LVDS_WRAP_ResetPHY(ts_mux_id, 1);

		lvds_splitter_init(&lvds->tcc_lvds_hw);

		LVDS_WRAP_SM_Bypass(lvds->tcc_lvds_hw.lcdc_mux_id,
				    (lvds->tcc_lvds_hw.lcdc_mux_bypass != 0u) ? 1u : 0u);
	} else {
		dev_dbg(lvds->dev, "[DEBUG][%s] %s with %s - already enabled\n",
			LOG_TAG, __func__, lvds->data->name);
	}

	return 0;
}

static int panel_lvds_enable(struct fb_panel *panel)
{
	struct panel_lvds *lvds = to_panel_lvds(panel);

	dev_dbg(lvds->dev, "[DEBUG][%s] %s with %s\n",
		LOG_TAG, __func__, lvds->data->name);

	if (lvds->enabled == 0) {
		lvds_phy_init(&lvds->tcc_lvds_hw);

		if (lvds->blk_wait_time > 0u) {
			mdelay(lvds->blk_wait_time);
		}
		#if defined(CONFIG_TELECHIPS_FB_CTRL_BACKLIGHT)
		if (lvds->backlight != NULL) {
			lvds->backlight->props.state &= ~BL_CORE_FBBLANK;
			lvds->backlight->props.power = FB_BLANK_UNBLANK;
			(void)backlight_update_status(lvds->backlight);
		} else {
			(void)pr_err("[ERROR][%s] : backlight driver not valid\n",
			       __func__);
		}
		#else
		if (
			panel_tcc_pin_select_state(
				lvds->lvds_pins.p,
				lvds->lvds_pins.blk_on) < 0) {
			dev_warn(lvds->dev,
				"[WARN][%s:%s] %s failed set pinctrl to blk_on\r\n",
				LOG_TAG, lvds->data->name, __func__);
		}
		#endif

		lvds->enabled = 1;
	} else {
		dev_dbg(lvds->dev, "[DEBUG][%s] %s with %s - already enabled\n",
			LOG_TAG, __func__, lvds->data->name);
	}

	return 0;
}

static int panel_lvds_get_videomode(
	struct fb_panel *panel,
	struct videomode *vm)
{
	const struct panel_lvds *lvds = to_panel_lvds(panel);

	(void)memcpy(vm, &lvds->video_mode, sizeof(*vm));

	return 0;
}

static const struct fb_panel_funcs panel_lvds_funcs = {
	.disable = panel_lvds_disable,
	.unprepare = panel_lvds_unprepare,
	.prepare = panel_lvds_prepare,
	.enable = panel_lvds_enable,
	.get_videomode = panel_lvds_get_videomode,
};

static int panel_lvds_parse_pins(struct panel_lvds *lvds)
{
	int ret = 0;
#if defined(CONFIG_TELECHIPS_FB_CTRL_BACKLIGHT)
	struct device_node *np;
#endif

	if (IS_ERR(lvds->lvds_pins.p)) {
		dev_err(lvds->dev, "[ERROR][%s] %s failed to find pinctrl\n",
			LOG_TAG, __func__);
		lvds->lvds_pins.p = NULL;
		ret = -ENODEV;
	}

	if (ret == 0) {
		lvds->lvds_pins.default0 =
			pinctrl_lookup_state(lvds->lvds_pins.p, "default");
		if (IS_ERR(lvds->lvds_pins.default0)) {
			dev_err(lvds->dev, "[ERROR][%s] %s failed to find default\n",
				LOG_TAG, __func__);
			lvds->lvds_pins.default0 = NULL;
			ret = -ENODEV;
		}
	}

	if (ret == 0) {

		lvds->lvds_pins.pwr_on_1 =
			pinctrl_lookup_state(lvds->lvds_pins.p, "pwr_on_1");
		if (IS_ERR(lvds->lvds_pins.pwr_on_1)) {
			dev_warn(lvds->dev, "[WARN][%s] %s failed to find pwr_on1\n",
				 LOG_TAG, __func__);
			lvds->lvds_pins.pwr_on_1 = NULL;
		}

		lvds->lvds_pins.pwr_on_2 =
			pinctrl_lookup_state(lvds->lvds_pins.p, "pwr_on_2");
		if (IS_ERR(lvds->lvds_pins.pwr_on_2)) {
			dev_warn(lvds->dev, "[WARN][%s] %s failed to find pwr_on2\n",
				 LOG_TAG, __func__);
			lvds->lvds_pins.pwr_on_2 = NULL;
		}

		lvds->lvds_pins.blk_on =
			pinctrl_lookup_state(lvds->lvds_pins.p, "blk_on");
		if (IS_ERR(lvds->lvds_pins.blk_on)) {
			dev_warn(lvds->dev, "[WARN][%s] %s failed to find blk_on\n",
				 LOG_TAG, __func__);
			lvds->lvds_pins.blk_on = NULL;
		}

		lvds->lvds_pins.blk_off =
			pinctrl_lookup_state(lvds->lvds_pins.p, "blk_off");
		if (IS_ERR(lvds->lvds_pins.blk_off)) {
			dev_warn(lvds->dev, "[WARN][%s] %s failed to find blk_off\n",
				 LOG_TAG, __func__);
			lvds->lvds_pins.blk_off = NULL;
		}

		lvds->lvds_pins.pwr_off =
			pinctrl_lookup_state(lvds->lvds_pins.p, "power_off");
		if (IS_ERR(lvds->lvds_pins.pwr_off)) {
			dev_warn(lvds->dev, "[WARN][%s] %s failed to find power_off\n",
				 LOG_TAG, __func__);
			lvds->lvds_pins.pwr_off = NULL;
		}

#if defined(CONFIG_TELECHIPS_FB_CTRL_BACKLIGHT)
		np = of_parse_phandle(lvds->dev->of_node, "backlight", 0);
		if (np != NULL) {
			lvds->backlight = of_find_backlight_by_node(np);
			of_node_put(np);

			if (lvds->backlight == NULL) { //backlight node is not valid
				dev_err(lvds->dev, "[ERROR][%s] %s: backlight driver not valid\n",
					     LOG_TAG, __func__);
			} else {
				dev_warn(lvds->dev, "[INFO][%s] %s: External backlight driver : max brightness[%d]\n",
					LOG_TAG, __func__,
					lvds->backlight->props.max_brightness);
			}
		} else {
			dev_warn(lvds->dev, "[INFO][%s] %s: Use pinctrl backlight\n",
				 LOG_TAG, __func__);
		}
#else
		dev_warn(lvds->dev, "[INFO][%s] %s: Use pinctrl backlight\n",
			 LOG_TAG, __func__);
#endif
	}

	return ret;
}

/* HIS CALLS, HIS_CCM */
static int panel_lvds_parse_dt(struct panel_lvds *lvds)
{
	struct device_node *dn = lvds->dev->of_node;
	struct device_node *np;
	const char *mapping;
	unsigned int lane_idx;
	unsigned int lvds_format;
	unsigned int p_clk;
	lvds_hw_info_t lvds_info;
	const lvds_hw_info_t *lvds_ret;
	uint32_t blk_wait_time;
	int ret = 0;

	if (of_property_read_u32(dn, "blk-wait-time", &blk_wait_time) == 0u) {
		if (blk_wait_time > 0u) {
			lvds->blk_wait_time = blk_wait_time;
		}
	}

	//todo LVDS timing function
	ret = of_property_read_string(dn, "data-mapping", &mapping);
	if (ret < 0) {
		dev_err(lvds->dev,
			"[ERROR][%s] %s failed to get data-mapping property\n",
			LOG_TAG, __func__);
		ret = -ENODEV;
	}

	if (ret == 0) {
		if (strcmp(mapping, "vesa-24") == 0) {
			lvds->bus_format = MEDIA_BUS_FMT_RGB888_1X7X4_SPWG;
			lvds_format = TCC_LVDS_OUTPUT_VESA24;
		} else {
			/* need to developed if vesa-16 and jeida-24 are needed. */
			dev_err(lvds->dev,
				"[ERROR][%s] %s invalid or missing data-mapping property\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		if (of_property_read_u32_index(dn, "mode", 0,
					       &lvds_info.lvds_type)
		    < 0) {
			dev_err(lvds->dev,
				"[ERROR][%s] %s failed to get mode property\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		LVDS_DBG("%s lvds - main_port: %d\n",
			 __func__, lvds_info.lvds_type);

		if (of_property_read_u32_index(dn, "phy-ports", 0,
							&lvds_info.port_main)
		    < 0) {
			dev_err(lvds->dev,
				"[ERROR][%s] %s failed to get phy-ports property\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		LVDS_DBG("%s lvds - main_port: %d\n",
						__func__, lvds_info.port_main);

		if (lvds_info.lvds_type == (unsigned int)PANEL_LVDS_DUAL) {
			if (of_property_read_u32_index(dn,
				"phy-ports", 1, &lvds_info.port_sub) < 0) {
				dev_err(lvds->dev,
					"[ERROR][%s] %s failed to get phy-ports for sub property\n",
					LOG_TAG, __func__);
				ret = -ENODEV;
			}

			LVDS_DBG("%s lvds - sub_port: %d\n",
				 __func__, lvds_info.port_sub);
		} else if (lvds_info.lvds_type ==
					(unsigned int)PANEL_LVDS_SINGLE) {
			lvds_info.port_sub = LVDS_PHY_PORT_MAX;
		} else {
			dev_err(lvds->dev,
				"[ERROR][%s] %s wrong port number\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		if (of_property_read_u32_index(dn, "lcdc-mux-select", 0,
					       &lvds_info.lcdc_mux_id) < 0) {
			dev_err(lvds->dev,
				"[ERROR][%s] %s failed to get lcdc-mux-select property\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		LVDS_DBG("%s lvds - lcdc-mux-select: %d\n", __func__,
			 lvds_info.lcdc_mux_id);
		if (of_property_read_bool(dn, "lcdc-mux-bypass")) {
			lvds_info.lcdc_mux_bypass = 1;
		}
		LVDS_DBG("%s lvds - lcdc-mux-bypass=%u\r\n",
			  __func__, lvds_info.lcdc_mux_bypass);
	}
	if (ret == 0) {

		for (lane_idx = 0 ; lane_idx < (unsigned int)LVDS_PHY_LANE_MAX; lane_idx++) {
			if (of_property_read_u32_index(dn, "lane-main",
				lane_idx, &lvds_info.lane_main[lane_idx]) < 0) {
				dev_err(lvds->dev,
					"[ERROR][%s] %s failed to get lane-main property\n",
					LOG_TAG, __func__);
				ret = -ENODEV;
			}

			LVDS_DBG("%s lvds - lane main[%d] : %d\n", __func__,
				 lane_idx, lvds_info.lane_main[lane_idx]);
		}
	}

	if (ret == 0) {
		if (lvds_info.lvds_type == (unsigned int)PANEL_LVDS_DUAL) {
			for (lane_idx = 0 ; lane_idx < (unsigned int)LVDS_PHY_LANE_MAX;
				lane_idx++) {
				if (of_property_read_u32_index(dn, "lane-sub",
				lane_idx, &lvds_info.lane_sub[lane_idx]) < 0) {
					dev_err(lvds->dev,
						"[ERROR][%s] %s failed to get lane-sib property\n",
						LOG_TAG, __func__);
					ret = -ENODEV;
				}

				LVDS_DBG("%s lvds - lane sub[%d] : %d\n",
					 __func__,
					 lane_idx,
					 lvds_info.lane_sub[lane_idx]);
			}
		}
	}

	if (ret == 0) {
		if (of_property_read_u32_index(dn, "vcm", 0, &lvds_info.vcm) < 0) {
			dev_err(lvds->dev,
				"[ERROR][%s] %s failed to get vcm property\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		LVDS_DBG("%s lvds - vcm: %d\n", __func__, lvds_info.vcm);

		if (of_property_read_u32_index(dn, "vsw", 0,
					       &lvds_info.vsw) < 0) {
			dev_err(lvds->dev,
				"[ERROR][%s] %s failed to get vsw property\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		LVDS_DBG("%s lvds - vsw: %d\n", __func__, lvds_info.vsw);

		np = of_get_child_by_name(dn, "display-timings");
		if (np != NULL) {
			of_node_put(np);

			ret = of_get_videomode(dn, &lvds->video_mode, OF_USE_NATIVE_MODE);
			if (ret < 0) {
				dev_err(lvds->dev,
					"[ERROR][%s] %s failed to get of_get_videomode\n",
					LOG_TAG, __func__);
				ret = -ENODEV;
			}
		} else {
			dev_err(lvds->dev,
				"[ERROR][%s] %s failed to get display-timings property\n",
				LOG_TAG, __func__);
		}
	}

	if (ret == 0) {
		/* lvds hw_info registration */
		(void)memcpy(lvds->tcc_lvds_hw.txout_main,
			     lvds_outformat[lvds_format],
			     sizeof(lvds->tcc_lvds_hw.txout_main));

		if (lvds_info.lvds_type == (unsigned int)PANEL_LVDS_DUAL) {
			(void)memcpy(lvds->tcc_lvds_hw.txout_sub,
				     lvds_outformat[lvds_format],
				     sizeof(lvds->tcc_lvds_hw.txout_sub));
		}

		(void)memcpy(lvds->tcc_lvds_hw.lane_main, lvds_info.lane_main,
			     sizeof(lvds->tcc_lvds_hw.lane_main));

		if (lvds_info.lvds_type == (unsigned int)PANEL_LVDS_DUAL) {
			(void)memcpy(lvds->tcc_lvds_hw.lane_sub,
				     lvds_info.lane_sub,
				     sizeof(lvds->tcc_lvds_hw.lane_sub));
		}

		lvds->tcc_lvds_hw.vcm = lvds_info.vcm;
		lvds->tcc_lvds_hw.vsw = lvds_info.vsw;
		p_clk = (unsigned int)lvds->video_mode.pixelclock;

		lvds_ret = lvds_register_hw_info(&lvds->tcc_lvds_hw,
						 lvds_info.lvds_type,
						 lvds_info.port_main,
						 lvds_info.port_sub,
						 p_clk,
						 lvds_info.lcdc_mux_id,
						 lvds_info.lcdc_mux_bypass,
						 lvds->video_mode.hactive);

		if (lvds_ret == NULL) {
			(void)pr_err("%s : invalid lcdc_hw ptr\n", __func__);
			dev_err(lvds->dev,
				"[ERROR][%s] %s failed to get hardware information for lcd\n",
				LOG_TAG, __func__);
			ret = -EINVAL;
		}
	}

	if (ret == 0) {
		/* pinctrl */
		lvds->lvds_pins.p = devm_pinctrl_get(lvds->dev);
		ret = panel_lvds_parse_pins(lvds);
	}

	return ret;
}

/* HIS CALLS */
static int panel_lvds_probe(struct platform_device *pdev)
{
	struct panel_lvds *lvds;
	int ret = 0;
	unsigned int lvds_status;

	lvds = devm_kzalloc(&pdev->dev, sizeof(*lvds), GFP_KERNEL);

	if (lvds != NULL) {
		ret = 0;
	} else {
		(void)pr_err("[ERROR][%s] %s failed to alloc device context\n",
			LOG_TAG, __func__);
		ret = -ENODEV;
	}

	if (ret == 0) {
		lvds->dev = &pdev->dev;
		lvds->data = (struct lvds_match_data *)
				of_device_get_match_data(&pdev->dev);
		if (lvds->data == NULL) {
			dev_err(lvds->dev,
				"[ERROR][%s] %s failed to find match_data from device tree\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
			devm_kfree(&pdev->dev, lvds);
		}
	}

	if (ret == 0) {
		ret = panel_lvds_parse_dt(lvds);
		if (ret < 0) {
			dev_err(lvds->dev,
				"[ERROR][%s] %s failed to parse device tree\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
			devm_kfree(&pdev->dev, lvds);
		}
	}

	if (ret == 0) {
		/* Register the panel. */
		fb_panel_init(&lvds->panel);
		lvds->panel.dev = lvds->dev;
		lvds->panel.funcs = &panel_lvds_funcs;

		ret = fb_panel_add(&lvds->panel);
		if (ret < 0) {
			dev_err(lvds->dev,
				"[ERROR][%s] %s with [%s] failed to fb_panel_init\n",
				LOG_TAG, __func__, lvds->data->name);

			/* put dev */
			#if defined(CONFIG_TELECHIPS_FB_CTRL_BACKLIGHT)
			put_device(&lvds->backlight->dev);
			#endif
			devm_kfree(&pdev->dev, lvds);
		}
	}

	if (ret == 0) {
		dev_set_drvdata(lvds->dev, lvds);

		dev_info(lvds->dev, "[INFO][%s] %s with [%s]\n",
			 LOG_TAG, __func__, lvds->data->name);

		lvds_status = LVDS_PHY_CheckStatus(
						   lvds->tcc_lvds_hw.port_main,
						   lvds->tcc_lvds_hw.port_sub);

		if ((lvds_status & 0x1U) == 0U) {
			dev_info(lvds->dev,
				 "[INFO][%s] %s with [%s] Primary port(%d) is in death\n",
				 LOG_TAG, __func__, lvds->data->name,
				 lvds->tcc_lvds_hw.port_main);
		} else {
			if (lvds->tcc_lvds_hw.lvds_type ==
			    (unsigned int)PANEL_LVDS_SINGLE) {
				lvds->enabled = 1;
			}
			dev_info(
				 lvds->dev,
				 "[INFO][%s] %s with [%s] Primary port(%d) is in alive\r\n",
				 LOG_TAG, __func__, lvds->data->name,
				 lvds->tcc_lvds_hw.port_main);
		}

		if (lvds->tcc_lvds_hw.lvds_type ==
						(unsigned int)PANEL_LVDS_DUAL) {
			if ((lvds_status & 0x2U) == 0U) {
				dev_info(lvds->dev,
					 "[INFO][%s] %s with [%s] Secondary port(%d) is in death\n",
					 LOG_TAG, __func__, lvds->data->name,
					 lvds->tcc_lvds_hw.port_sub);
			} else {
				lvds->enabled = 1;
				dev_info(lvds->dev,
					 "[INFO][%s] %s with [%s] Secondary port(%d) is in alive\n",
					 LOG_TAG, __func__, lvds->data->name,
					 lvds->tcc_lvds_hw.port_sub);
			}
		}

		dev_info(lvds->dev,
			 "[INFO][%s] %s with [%s] lvds - lcdc-mux-select: %d\n",
			 LOG_TAG, __func__, lvds->data->name,
			 lvds->tcc_lvds_hw.lcdc_mux_id);
		dev_info(lvds->dev,
			 "[INFO][%s] %s LVDS driver v%d.%d.%d\n",
			 LOG_TAG, __func__,
			 DRIVER_MAJOR,
			 DRIVER_MINOR,
			 DRIVER_PATCH);
	}

	return ret;
}

static int panel_lvds_remove(struct platform_device *pdev)
{
	struct panel_lvds *lvds =
		(struct panel_lvds*)dev_get_drvdata(&pdev->dev);

	devm_pinctrl_put(lvds->lvds_pins.p);

	fb_panel_remove(&lvds->panel);

	(void)panel_lvds_disable(&lvds->panel);

	#if defined(CONFIG_TELECHIPS_FB_CTRL_BACKLIGHT)
	if (lvds->backlight != NULL) {
		put_device(&lvds->backlight->dev);
	}
	#endif

	devm_kfree(&pdev->dev, lvds);

	return 0;
}

static const struct of_device_id panel_lvds_of_table[] = {
	{ .compatible = "telechips,fb-lvds-auoc123han06",
	  .data = &lvds_auoc123han06,
	},
	{ .compatible = "telechips,fb-lvds-tm123xdhp90",
	  .data = &lvds_tm123xdhp90,
	},
	{ .compatible = "telechips,fb-lvds-fld0800",
	  .data = &lvds_fld0800,
	},
	{ /* Sentinel */ },
};

MODULE_DEVICE_TABLE(of, panel_lvds_of_table);

#ifdef CONFIG_PM
static int panel_lvds_suspend(struct device *dev)
{
	struct panel_lvds *lvds = (struct panel_lvds*)dev_get_drvdata(dev);

	dev_dbg(lvds->dev, "[DEBUG][%s] %s\n", LOG_TAG, __func__);

	return 0;
}

static int panel_lvds_resume(struct device *dev)
{
	struct panel_lvds *lvds = (struct panel_lvds*)dev_get_drvdata(dev);

	dev_dbg(dev, "[DEBUG][%s] %s\n", LOG_TAG, __func__);

	/* Set pin status to defualt */
	if (
		panel_tcc_pin_select_state(
			lvds->lvds_pins.p,
			lvds->lvds_pins.default0) < 0) {
		dev_warn(lvds->dev,
			"[WARN][%s:%s] %s failed set pinctrl to default0\r\n",
			LOG_TAG, lvds->data->name, __func__);
	}
	if (
		panel_tcc_pin_select_state(
			lvds->lvds_pins.p,
			lvds->lvds_pins.pwr_off) < 0) {
		dev_warn(lvds->dev,
			"[WARN][%s:%s] %s failed set pinctrl to pwr_off\r\n",
			LOG_TAG, lvds->data->name, __func__);
	}
	if (
		panel_tcc_pin_select_state(
			lvds->lvds_pins.p,
			lvds->lvds_pins.blk_off) < 0) {
		dev_warn(lvds->dev,
			"[WARN][%s:%s] %s failed set pinctrl to blk_off\r\n",
			LOG_TAG, lvds->data->name, __func__);
	}

	return 0;
}
static const struct dev_pm_ops panel_lvds_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(panel_lvds_suspend, panel_lvds_resume)
};
#endif

static struct platform_driver fb_panel_lvds_driver = {
	.probe		= panel_lvds_probe,
	.remove		= panel_lvds_remove,
	.driver		= {
		.name	= "fb-panel-lvds",
		#ifdef CONFIG_PM
		.pm	= &panel_lvds_pm_ops,
		#endif
		.of_match_table = panel_lvds_of_table,
	},
};
module_platform_driver(fb_panel_lvds_driver);

MODULE_DESCRIPTION("Telechips LVDS Panel Driver");
MODULE_LICENSE("GPL");
