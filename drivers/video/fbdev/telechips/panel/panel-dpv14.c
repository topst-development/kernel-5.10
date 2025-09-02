// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/backlight.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio.h>
#include <linux/clk.h>
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
#include <video/videomode.h>
#include <panel_helper.h>

#include "panel-tcc.h"
#include <include/dptx_api.h>
#include <include/dptx_video.h>
#include <include/dptx_fb.h>

#include <panel-dpv14.h>

#define DPV14_LOG_TAG "FB_PANEL_DPV14"

#define DPV14_DRIVER_DATE	"20240514"
#define DPV14_DRIVER_MAJOR	1
#define DPV14_DRIVER_MINOR	0
#define DPV14_DRIVER_PATCH	1

struct dpv14_match_data {
	const char *name;
};

static const struct dpv14_match_data dpv14_panel = {
	.name = "TCC_FB_DPV14",
};


struct panel_dpv14_params {
	uint8_t dp_idx;
	uint32_t dp_vic;
	unsigned long dp_pclk[DPTX_INPUT_STREAM_MAX];

	struct device *dev;
	struct fb_panel panel;
	const struct dpv14_match_data *data;/* Device driver data */

	/*pinctrl */
	struct pinctrl *pin;
	struct pinctrl_state *default0;
	struct pinctrl_state *pwr_on_1;
	struct pinctrl_state *pwr_on_2;
	struct pinctrl_state *blk_on;
	struct pinctrl_state *blk_off;
	struct pinctrl_state *pwr_off;

 	#if defined(CONFIG_TELECHIPS_FB_CTRL_BACKLIGHT)
	struct backlight_device *backlight;
 	#endif

	struct dptx_dtd_params dtd_params;

	struct dptx_fb_helper_dp_funcs *dp_ops;

 	/*
	 * Some panels, such as the AUO LVDS panel, must first transmit an
	 * LVDS signal when initializing the panel and then turn on the
	 * backlight after a certain period of time.
	 * The blk-wait-time is used to determine the waiting time until the
	 * backlight is turned on after a signal is transmitted to the
	 * LVDS panel.
	 */
	uint32_t blk_wait_time;

	/* Version ---------------------*/
	int major;	/** @major: driver major number */
	int minor;	/** @minor: driver minor number */
	int patchlevel;	/** @patchlevel: driver patch level */
	char *date;	/** @date: driver date */
};

static inline struct panel_dpv14_params *to_panel_dpv14(struct fb_panel *panel)
{
	return container_of(panel, struct panel_dpv14_params, panel);
}

//- DP HELPER-------------------------------------------------------------------
static int panel_dpv14_get_main_state(struct panel_dpv14_params *panel_params)
{
	int main_state = -1;

	if ((panel_params->dp_ops != NULL) &&
	    (panel_params->dp_ops->get_main_state != NULL)) {
		main_state = panel_params->dp_ops->get_main_state();
	}
	return main_state;
}

static int panel_dpv14_set_video(struct panel_dpv14_params *panel_params)
{
	int ret = -1;

	if ((panel_params->dp_ops != NULL) &&
	    (panel_params->dp_ops->set_video != NULL)) {
		ret = panel_params->dp_ops->set_video(panel_params->dp_idx, &panel_params->dtd_params);
	}
	return ret;
}

static int panel_dpv14_set_video_enable(struct panel_dpv14_params *panel_params)
{
	int ret = -1;

	if ((panel_params->dp_ops != NULL) &&
	    (panel_params->dp_ops->set_enable_video != NULL)) {
		ret = panel_params->dp_ops->set_enable_video(panel_params->dp_idx, 1u);
	}
	return ret;
}

static int panel_dpv14_set_video_disable(struct panel_dpv14_params *panel_params)
{
	int ret = -1;

	if ((panel_params->dp_ops != NULL) &&
	    (panel_params->dp_ops->set_enable_video != NULL)) {
		ret = panel_params->dp_ops->set_enable_video(panel_params->dp_idx, 0u);
	}
	return ret;
}

static int panel_dpv14_get_dtd_from_vic(struct panel_dpv14_params *panel_params)
{
	int ret = -1;

	if ((panel_params->dp_ops != NULL) &&
	    (panel_params->dp_ops->get_dtd_from_vic != NULL)) {
		ret = panel_params->dp_ops->get_dtd_from_vic(panel_params->dp_vic,
							     &panel_params->dtd_params);
	}
	return ret;
}
//------------------------------------------------------------------------------

static int panel_dpv14_disable(struct fb_panel *panel)
{
	struct panel_dpv14_params *panel_params = to_panel_dpv14(panel);
	int ret = 0;

	#if defined(CONFIG_TELECHIPS_FB_CTRL_BACKLIGHT)
	if (panel_params->backlight != NULL) {
		panel_params->backlight->props.power = FB_BLANK_POWERDOWN;
		panel_params->backlight->props.state |= BL_CORE_FBBLANK;
		(void)backlight_update_status(panel_params->backlight);
	} else {
		(void)pr_err("[ERROR][%s] : backlight driver not valid\n",
			__func__);
	}
	#else
	if ((panel_params->pin != NULL) && (panel_params->blk_off != NULL)) {
		ret = panel_tcc_pin_select_state(panel_params->pin, panel_params->blk_off);
	}
	#endif
	dev_info(panel_params->dev,
	         "[DEB][%s:%s]DP%u with %s\n", DPV14_LOG_TAG, __func__,
		 panel_params->dp_idx, panel_params->data->name);
	(void)panel_dpv14_set_video_disable(panel_params);
	return ret;
}

static int panel_dpv14_enable(struct fb_panel *panel)
{
	struct panel_dpv14_params *panel_params = to_panel_dpv14(panel);
	int ret = 0;

	dev_info(panel_params->dev, "****[DBG][%s:%s]DP%u with %s\n", DPV14_LOG_TAG, __func__, panel_params->dp_idx, panel_params->data->name);

	ret = panel_dpv14_get_main_state(panel_params);
	dev_info(panel_params->dev, "[DBG][%s:%s] Wait done\n", DPV14_LOG_TAG, __func__);
	if (ret < 0) {
		dev_err(panel_params->dev,
			"[DBG][%s:%s] can't enable dp due to failed initialization in sub-core\n",
			DPV14_LOG_TAG, __func__);
	} else {
		if (panel_params->dp_pclk[panel_params->dp_idx] < UINT_MAX) {
			panel_params->dtd_params.uiPixel_Clock =
				(uint32_t)panel_params->dp_pclk[panel_params->dp_idx];
			(void)panel_dpv14_set_video(panel_params);
			(void)panel_dpv14_set_video_enable(panel_params);
		} else {
			ret = -EINVAL;
		}

	}
	if (panel_params->blk_wait_time > 0u) {
		mdelay(panel_params->blk_wait_time);
	}
	#if defined(CONFIG_TELECHIPS_FB_CTRL_BACKLIGHT)
	if (panel_params->backlight != NULL) {
		panel_params->backlight->props.state &= ~BL_CORE_FBBLANK;
		panel_params->backlight->props.power = FB_BLANK_UNBLANK;
		(void)backlight_update_status(panel_params->backlight);
	} else {
		(void)pr_err("[ERROR][%s] : backlight driver not valid\n",
			__func__);
	}
	#else
	if ((panel_params->pin != NULL) && (panel_params->blk_on != NULL)) {
		ret = panel_tcc_pin_select_state(panel_params->pin, panel_params->blk_on);
		if (ret < 0) {
			/* For KCS */
			dev_warn(panel_params->dev,
				 "[WARN][%s:%s] failed set pinctrl to blk_on\r\n",
				 DPV14_LOG_TAG, __func__);
		}
	}
	#endif
	return ret;
}

static int panel_dpv14_unprepare(struct fb_panel *panel)
{
	const struct panel_dpv14_params *panel_params = to_panel_dpv14(panel);

	dev_info(panel_params->dev, "[DEB][%s:%s]DP%u with %s\n", DPV14_LOG_TAG, __func__, panel_params->dp_idx, panel_params->data->name);

	if ((panel_params->pin != NULL) && (panel_params->pwr_off != NULL)) {
		panel_tcc_pin_select_state(panel_params->pin, panel_params->pwr_off);
	}
	return 0;
}

static int panel_dpv14_prepare(struct fb_panel *panel)
{
	struct panel_dpv14_params *panel_params = to_panel_dpv14(panel);
	int ret = 0;

	dev_info(panel_params->dev,
		 "[DEB][%s:%s]DP%u with %s\n", DPV14_LOG_TAG, __func__,
		 panel_params->dp_idx, panel_params->data->name);

	if (panel_params->pin != NULL) {
		if (panel_params->pwr_on_1 != NULL) {
			ret = panel_tcc_pin_select_state(panel_params->pin, panel_params->pwr_on_1);
		}
		mdelay(1);
		if (panel_params->pwr_on_2 != NULL) {
			ret = panel_tcc_pin_select_state(panel_params->pin, panel_params->pwr_on_2);
		}
	}

	return ret;
}

static int32_t panel_dpv14_get_videomode(struct fb_panel *panel, struct videomode *vm)
{
	struct panel_dpv14_params *panel_params = to_panel_dpv14(panel);

	(void)panel_dpv14_get_dtd_from_vic(panel_params);

	vm->pixelclock = (unsigned long)panel_params->dtd_params.uiPixel_Clock * (unsigned long)1000u;
	vm->hactive = panel_params->dtd_params.h_active;
	vm->hfront_porch = panel_params->dtd_params.h_sync_offset;
	vm->hback_porch = (panel_params->dtd_params.h_blanking - (panel_params->dtd_params.h_sync_offset + panel_params->dtd_params.h_sync_pulse_width));
	vm->hsync_len = panel_params->dtd_params.h_sync_pulse_width;

	vm->vactive = (panel_params->dtd_params.interlaced != 0U) ? (panel_params->dtd_params.v_active << 1) : panel_params->dtd_params.v_active;
	vm->vfront_porch = (panel_params->dtd_params.interlaced != 0U) ? ((uint32_t)panel_params->dtd_params.v_sync_offset << 1) : panel_params->dtd_params.v_sync_offset;
	if ((panel_params->dp_vic == 39U) && (vm->vfront_porch > 2U)) {
		/*For KCS*/
		vm->vfront_porch -= 2U;
	}
	vm->vback_porch = (panel_params->dtd_params.v_blanking - (panel_params->dtd_params.v_sync_offset + panel_params->dtd_params.v_sync_pulse_width));
	vm->vback_porch = (panel_params->dtd_params.interlaced != 0U) ? (vm->vback_porch << 1) : vm->vback_porch;
	vm->vsync_len = (panel_params->dtd_params.interlaced != 0U) ? (panel_params->dtd_params.v_sync_pulse_width << 1) : panel_params->dtd_params.v_sync_pulse_width;

	vm->flags = (panel_params->dtd_params.h_sync_polarity == 0U) ? DISPLAY_FLAGS_HSYNC_LOW : DISPLAY_FLAGS_HSYNC_HIGH;
	vm->flags |= (panel_params->dtd_params.v_sync_polarity == 0U) ? DISPLAY_FLAGS_VSYNC_LOW : DISPLAY_FLAGS_VSYNC_HIGH;
	if (panel_params->dtd_params.interlaced != 0U) {
		/*For KCS*/
		vm->flags |= DISPLAY_FLAGS_INTERLACED;
	}

	//For testing
	//panel_params->dtd_params.uiPixel_Clock = panel_params->dp_pclk[panel_params->dp_idx];
	//panel_params->dtd_params.uiPixel_Clock = panel_params->dp_pclk[2];

	dev_info(panel_params->dev, "[Detailed timing to FB] DP%u: ", panel_params->dp_idx);
	dev_info(panel_params->dev, "	Pixel clk = %u", (u32)vm->pixelclock);
	dev_info(panel_params->dev, "	%s", (panel_params->dtd_params.interlaced == 0U) ? "Progressive" : "Interlace");
	dev_info(panel_params->dev, "	H Active(%u), V Active(%u)", vm->hactive, vm->vactive);
	dev_info(panel_params->dev, "	H Front porch(%u), H Back porch(%u)", vm->hfront_porch, vm->hback_porch);
	dev_info(panel_params->dev, "	V Front porch(%u), V Back porch(%u)", vm->vfront_porch, vm->vback_porch);
	dev_info(panel_params->dev, "	Flag(0x%x)", vm->flags);
	dev_info(panel_params->dev, "	H Blanking(%d), V Blanking(%d)", (u32)panel_params->dtd_params.h_blanking, (u32)panel_params->dtd_params.v_blanking);
	dev_info(panel_params->dev, "	H Sync offset(%d), V Sync offset(%d) ", (u32)panel_params->dtd_params.h_sync_offset, (u32)panel_params->dtd_params.v_sync_offset);
	dev_info(panel_params->dev, "	H Sync plus W(%d), V Sync plus W(%d) ", (u32)panel_params->dtd_params.h_sync_pulse_width, (u32)panel_params->dtd_params.v_sync_pulse_width);
	dev_info(panel_params->dev, "	H Sync Polarity(%d), V Sync Polarity(%d)", (u32)panel_params->dtd_params.h_sync_polarity, (u32)panel_params->dtd_params.v_sync_polarity);

	return 0;
}

static int32_t panel_dpv14_set_pclk(struct fb_panel *panel, unsigned long pclk)
{
	struct panel_dpv14_params *panel_params = to_panel_dpv14(panel);

	if (pclk >= 1000u) {
		panel_params->dp_pclk[panel_params->dp_idx] = pclk / 1000u;
	} else {
		panel_params->dp_pclk[panel_params->dp_idx] = 0u;
	}

	return 0;
}


static const struct fb_panel_funcs panel_dpv14_funcs = {
	.disable = panel_dpv14_disable,
	.unprepare = panel_dpv14_unprepare,
	.prepare = panel_dpv14_prepare,
	.enable = panel_dpv14_enable,
	.get_videomode = panel_dpv14_get_videomode,
	.set_pclk = panel_dpv14_set_pclk,
};

static int32_t panel_dpv14_parse_dt(struct platform_device *pdev, struct panel_dpv14_params *panel_params)
{
	struct device_node *dn;
	uint32_t blk_wait_time;
	uint32_t dp_idx;
	int32_t ret = 0;

	dn = panel_params->dev->of_node;

	if (of_property_read_u32(dn, "blk-wait-time", &blk_wait_time) == 0u) {
		if (blk_wait_time > 0u) {
			panel_params->blk_wait_time = blk_wait_time;
		}
	}

	ret = of_property_read_u32_index(dn, "vic", 0, &panel_params->dp_vic);
	if (ret < 0) {
		dev_warn(panel_params->dev, "[%s:%s]Warn: failed to get vic, default set to 1027\n",
			 DPV14_LOG_TAG, __func__);
		panel_params->dp_vic = 1027U;
	}

	ret = of_property_read_u32_index(dn, "dp-idx", 0, &dp_idx);
	if (ret < 0) {
		dev_warn(panel_params->dev, "[%s:%s]Warn: failed to get dp_idx, default set to 1\n",
			 DPV14_LOG_TAG, __func__);
		panel_params->dp_idx = 1U;
	} else {
		if (dp_idx < 4u) {
			/* For KCS */
			panel_params->dp_idx = (uint8_t)dp_idx;
		} else {
			dev_warn(panel_params->dev, "[%s:%s]Warn: dp_idx (%u) is out of range\n",
				 DPV14_LOG_TAG, __func__, dp_idx);
			ret = -EINVAL;
		}
	}

	return ret;
}


static int panel_dpv14_init_pinctrl(struct panel_dpv14_params *panel_params)
{
	#if defined(CONFIG_TELECHIPS_FB_CTRL_BACKLIGHT)
	struct device_node *np;
	#endif
	int ret = 0;

	panel_params->pin = devm_pinctrl_get(panel_params->dev);
	if (IS_ERR(panel_params->pin)) {
		dev_info(panel_params->dev,
			"[%s:%s]INFO: pinctrl is not provided\n", DPV14_LOG_TAG, __func__);

		panel_params->pin = NULL;
	}

	if (panel_params->pin != NULL) {
		panel_params->default0 = pinctrl_lookup_state(panel_params->pin, "default");
		if (IS_ERR(panel_params->default0)) {
			dev_warn(panel_params->dev,
				 "[%s:%s]Warn: failed to find default\n", DPV14_LOG_TAG, __func__);
			panel_params->default0 = NULL;
		}

		panel_params->pwr_on_1 = pinctrl_lookup_state(panel_params->pin, "pwr_on_1");
		if (IS_ERR(panel_params->pwr_on_1)) {
			dev_warn(panel_params->dev,
				 "[%s:%s]Warn: failed to find pwr_on1\n", DPV14_LOG_TAG, __func__);
			panel_params->pwr_on_1 = NULL;
		}

		panel_params->pwr_on_2 = pinctrl_lookup_state(panel_params->pin, "pwr_on_2");
		if (IS_ERR(panel_params->pwr_on_2)) {
			dev_warn(panel_params->dev,
				 "[%s:%s]Warn: failed to find pwr_on2\n", DPV14_LOG_TAG, __func__);
			panel_params->pwr_on_2 = NULL;
		}

		panel_params->blk_on = pinctrl_lookup_state(panel_params->pin, "blk_on");
		if (IS_ERR(panel_params->blk_on)) {
			dev_warn(panel_params->dev,
				 "[%s:%s]Warn: failed to find blk_on\n", DPV14_LOG_TAG, __func__);
			panel_params->blk_on = NULL;
		}

		panel_params->blk_off = pinctrl_lookup_state(panel_params->pin, "blk_off");
		if (IS_ERR(panel_params->blk_off)) {
			dev_warn(panel_params->dev,
				 "[%s:%s]Warn: failed to find blk_off\n", DPV14_LOG_TAG, __func__);
			panel_params->blk_off = NULL;
		}

		panel_params->pwr_off = pinctrl_lookup_state(panel_params->pin, "power_off");
		if (IS_ERR(panel_params->pwr_off)) {
			dev_warn(panel_params->dev,
				 "[%s:%s]Warn: failed to find power_off\n", DPV14_LOG_TAG, __func__);
			panel_params->pwr_off = NULL;
		}
	}
	#if defined(CONFIG_TELECHIPS_FB_CTRL_BACKLIGHT)
	np = of_parse_phandle(dpdev->of_node, "backlight", 0);
	if (np != NULL) {
		panel_params->backlight = of_find_backlight_by_node(np);
		of_node_put(np);

		if (panel_params->backlight == NULL) { //backlight node is not valid
			dev_err(panel_params->dev, "[%s:%s]Err: backlight driver not valid\n", DPV14_LOG_TAG, __func__);
		} else {
			dev_info(panel_params->dev, "[%s:%s]Info: External backlight driver : max brightness[%d]\n", DPV14_LOG_TAG, __func__, panel_params->backlight->props.max_brightness);
		}
	} else {
		dev_warn(panel_params->dev, "[%s:%s]Warn: kernel backlight not valid\n", DPV14_LOG_TAG, __func__);
	}
	#else
	dev_info(panel_params->dev, "[%s:%s] Info: pinctrl backlight\n", DPV14_LOG_TAG, __func__);
	#endif

	return ret;
}

static int panel_dpv14_probe(struct platform_device *pdev)
{
	int32_t ret = 0;
	struct panel_dpv14_params *panel_params;

	panel_params = devm_kzalloc(&pdev->dev, sizeof(*panel_params), GFP_KERNEL);
	if (panel_params == NULL) {
		pr_err("[%s:%s]Err: failed to alloc device context\n", DPV14_LOG_TAG, __func__);

		ret = -ENODEV;
	} else {
		panel_params->dev = &pdev->dev;

		panel_params->data = (const struct dpv14_match_data *)of_device_get_match_data(&pdev->dev);
		if (panel_params->data == NULL) {
			dev_err(panel_params->dev, "[%s:%s]Err: failed to find match_data from device tree\n", DPV14_LOG_TAG, __func__);
			ret = -ENODEV;
		}
	}
	if (ret == 0) {
		ret = panel_dpv14_parse_dt(pdev, panel_params);
	}
	if (ret == 0) {
		(void)panel_dpv14_init_pinctrl(panel_params);

		fb_panel_init(&panel_params->panel); /* Register the panel. */

		panel_params->panel.dev = panel_params->dev;
		panel_params->panel.funcs = &panel_dpv14_funcs;

		ret = fb_panel_add(&panel_params->panel);
		if (ret < 0) {
			dev_err(panel_params->dev, "[%s:%s]Err: failed to panel with %s to FB\n", DPV14_LOG_TAG, __func__, panel_params->data->name);
		}
	}
	if (ret == 0) {
		dev_set_drvdata(panel_params->dev, panel_params);
		dptx_register_fb_dp_ops(&panel_params->dp_ops);

		panel_params->major = DPV14_DRIVER_MAJOR;
		panel_params->minor = DPV14_DRIVER_MINOR;
		panel_params->patchlevel = DPV14_DRIVER_PATCH;
		panel_params->date = kstrdup(DPV14_DRIVER_DATE, GFP_KERNEL);

		dev_info(panel_params->dev, "\n[%s:%s]Info: initialization is done\n", DPV14_LOG_TAG, __func__);
		dev_info(panel_params->dev, "	Version: %d.%d.%d - built %s)\n", panel_params->major, panel_params->minor, panel_params->patchlevel, panel_params->date);
		dev_info(panel_params->dev, "	Name: %s\n", panel_params->data->name);
		dev_info(panel_params->dev, "	DP Idx: %u\n", panel_params->dp_idx);
		dev_info(panel_params->dev, "	VIC: %u\n", panel_params->dp_vic);
	}

	if (ret < 0) {
		if (panel_params != NULL) {
			#if defined(CONFIG_TELECHIPS_FB_CTRL_BACKLIGHT)
			if (panel_params->backlight != NULL) {
				put_device(&panel_params->backlight->dev);
			}
			#endif
			devm_kfree(&pdev->dev, panel_params);
		}
	}

	return ret;
}

static int panel_dpv14_remove(struct platform_device *pdev)
{
	struct panel_dpv14_params *panel_params;

	panel_params = (struct panel_dpv14_params *)dev_get_drvdata(&pdev->dev);

	fb_panel_remove(&panel_params->panel);

	(void)panel_dpv14_disable(&panel_params->panel);

	if (panel_params->pin != NULL) {
		devm_pinctrl_put(panel_params->pin);
	}
	devm_kfree(&pdev->dev, panel_params);

	return 0;
}

#ifdef CONFIG_PM
static int panel_dpv14_suspend(struct device *dev)
{
	struct panel_dpv14_params *panel_params;

	panel_params = (struct panel_dpv14_params *)dev_get_drvdata(dev);

	dev_info(panel_params->dev, "\n****[INFO][%s:%s]DP%u with %s\n", DPV14_LOG_TAG, __func__, panel_params->dp_idx, panel_params->data->name);

	return 0;
}

/* coverity[misra_c_2012_rule_8_13_violation : FALSE] */
static int panel_dpv14_resume(struct device *dev)
{
	const struct panel_dpv14_params *panel_params;
	int ret;
	panel_params = (const struct panel_dpv14_params *)dev_get_drvdata(dev);

	dev_info(panel_params->dev,
		 "\n****[INFO][%s:%s]DP%u with %s\n", DPV14_LOG_TAG,
		 __func__, panel_params->dp_idx, panel_params->data->name);
	ret = pinctrl_pm_select_default_state(dev);
	if (ret != 0) {
		/* For KCS */
		dev_err(panel_params->dev, "from pinctrl_pm_select_default_state()");
	}

	if (panel_params->pin != NULL) {
		if (panel_params->default0 != NULL) {
			ret = panel_tcc_pin_select_state(panel_params->pin, panel_params->default0);
			if (ret < 0) {
				/* For KCS */
				dev_warn(panel_params->dev,
					 "[WARN][%s:%s] failed set pinctrl to default0\r\n",
					 DPV14_LOG_TAG, __func__);
			}
		}
		if (panel_params->pwr_off != NULL) {
			ret = panel_tcc_pin_select_state(panel_params->pin, panel_params->pwr_off);
			if (ret < 0) {
				/* For KCS */
				dev_warn(panel_params->dev,
					 "[WARN][%s:%s] failed set pinctrl to pwr_off\r\n",
					 DPV14_LOG_TAG, __func__);
			}
		}
		if (panel_params->blk_off != NULL) {
			ret = panel_tcc_pin_select_state(panel_params->pin, panel_params->blk_off);
			if (ret < 0) {
				/* For KCS */
				dev_warn(panel_params->dev,
					 "[WARN][%s:%s] failed set pinctrl to blk_off\r\n",
					 DPV14_LOG_TAG, __func__);
			}
		}
	} else {
		dev_err(panel_params->dev,
			"[%s:%s]Err: no pinctrl valid\n", DPV14_LOG_TAG, __func__);
	}

	return 0;
}
static const struct dev_pm_ops panel_dpv14_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(panel_dpv14_suspend, panel_dpv14_resume)
};
#endif

static const struct of_device_id panel_dpv14_params_of_table[] = {
	{
		.compatible = "telechips,fb-dpv14-panel",
		.data = &dpv14_panel,
	},
	{ /* Sentinel */ },
};
MODULE_DEVICE_TABLE(of, panel_dpv14_params_of_table);

static struct platform_driver fb_panel_dpv14_params_driver = {
	.probe		= panel_dpv14_probe,
	.remove		= panel_dpv14_remove,
	.driver		= {
		.name	= "fb-panel-dpv14",
#ifdef CONFIG_PM
		.pm	= &panel_dpv14_pm_ops,
#endif
		.of_match_table = panel_dpv14_params_of_table,
	},
};
module_platform_driver(fb_panel_dpv14_params_driver);

MODULE_DESCRIPTION("Telechips DPV14 Panel Driver");
MODULE_LICENSE("GPL");
