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
#include <linux/delay.h>

//#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_panel.h>

#include <video/display_timing.h>
#include <video/of_videomode.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>

#include <video/telechips/vioc_lvds.h>

#define LOG_TAG "DRMLVDS"

#define DRIVER_DATE	"20240229"
#define DRIVER_MAJOR	3
#define DRIVER_MINOR	0
#define DRIVER_PATCH	2

//#define LVDS_DEBUG
#ifdef LVDS_DEBUG
#define LVDS_DBG(fmt, args...) \
	dev_info(lvds->dev, "[INFO][DRMLVDS] " fmt, ## args)
#else
#define LVDS_DBG(fmt, args...) do {} while (false)
#endif //LVDS_DEBUG

static int panel_lvds_tcc_pin_select_state(struct pinctrl *p, struct pinctrl_state *s);

static int panel_lvds_tcc_pin_select_state(struct pinctrl *p, struct pinctrl_state *s)
{
	int ret = 0;

	if ((p == NULL) || (s == NULL)) {
		ret  = -EINVAL;
	} else {
		ret = pinctrl_select_state(p, s);
	}
	return ret;
}

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
	struct drm_panel panel;
	struct device *dev;

	const char *label;
	unsigned int width;
	unsigned int height;
	unsigned int bus_format;
	struct videomode video_mode;

	#if defined(CONFIG_DRM_TELECHIPS_CTRL_BACKLIGHT)
	struct backlight_device *backlight;
	#endif

	struct gpio_desc *enable_gpio;
	struct gpio_desc *reset_gpio;

	/* Device driver data */
	const struct lvds_match_data *data;

	struct lvds_pin lvds_pins;

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

static inline struct panel_lvds *to_panel_lvds(struct drm_panel *panel)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */ //
	/* coverity[misra_c_2012_rule_8_6] */ //
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	return container_of(panel, struct panel_lvds, panel);
}

static int panel_lvds_disable(struct drm_panel *panel)
{
	const struct panel_lvds *lvds = to_panel_lvds(panel);

	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[cert_dcl37_c] */
	dev_dbg(
		lvds->dev, "[DEBUG][%s] %s with %s \r\n", LOG_TAG, __func__,
		lvds->data->name);

#if defined(CONFIG_DRM_TELECHIPS_CTRL_BACKLIGHT)
	if (lvds->backlight) {
		lvds->backlight->props.power = FB_BLANK_POWERDOWN;
		lvds->backlight->props.state |= BL_CORE_FBBLANK;
		backlight_update_status(lvds->backlight);
	} else {
		dev_err(lvds->dev,
			"[ERROR][%s:%s] %s backlight driver not valid\n",
			LOG_TAG, lvds->data->name, __func__);
	}
#else
	if (panel_lvds_tcc_pin_select_state(lvds->lvds_pins.p,
				       lvds->lvds_pins.blk_off) < 0) {
		dev_warn(lvds->dev,
			 "[WARN][%s:%s] %s failed set pinctrl to blk_off\r\n",
			 LOG_TAG, lvds->data->name, __func__);
	}
#endif
	if (panel_lvds_tcc_pin_select_state(lvds->lvds_pins.p,
				       lvds->lvds_pins.pwr_off) < 0) {
		dev_warn(lvds->dev,
			 "[WARN][%s:%s] %s failed set pinctrl to pwr_off\r\n",
			 LOG_TAG, lvds->data->name, __func__);
	}

	return 0;
}

static int panel_lvds_unprepare(struct drm_panel *panel)
{
	const struct panel_lvds *lvds = to_panel_lvds(panel);

	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[cert_dcl37_c] */
	dev_dbg(
		lvds->dev, "[DEBUG][%s:%s] %s \r\n",
		LOG_TAG, lvds->data->name, __func__);

	return 0;
}

/* coverity[HIS_metric] */
static int panel_lvds_prepare(struct drm_panel *panel)
{
	const struct panel_lvds *lvds = to_panel_lvds(panel);

	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[cert_dcl37_c] */
	dev_dbg(
		lvds->dev, "[DEBUG][%s:%s] %s \r\n",
		LOG_TAG, lvds->data->name, __func__);

	if (panel_lvds_tcc_pin_select_state(lvds->lvds_pins.p,
				       lvds->lvds_pins.pwr_on_1) < 0) {
		dev_warn(lvds->dev,
			 "[WARN][%s:%s] %s failed set pinctrl to pwr_on_1\r\n",
			 LOG_TAG, lvds->data->name, __func__);
	}

	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_12_1] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[cert_dcl37_c] */
	/* coverity[cert_pre31_c] */
	udelay(20);

	if (panel_lvds_tcc_pin_select_state(lvds->lvds_pins.p,
				       lvds->lvds_pins.pwr_on_2) < 0) {
		dev_warn(lvds->dev,
			 "[WARN][%s:%s] %s failed set pinctrl to pwr_on_2\r\n",
			 LOG_TAG, lvds->data->name, __func__);
	}

	return 0;
}

static int panel_lvds_enable(struct drm_panel *panel)
{
	const struct panel_lvds *lvds = to_panel_lvds(panel);

	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[cert_dcl37_c] */
	dev_dbg(
		lvds->dev, "[DEBUG][%s:%s] %s \r\n",
		LOG_TAG, lvds->data->name, __func__);

#if defined(CONFIG_DRM_TELECHIPS_CTRL_BACKLIGHT)
	if (lvds->backlight != NULL) {
		/* coverity[cert_int31_c] */
		/* coverity[cert_int02_c] */
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_10_4] */
		lvds->backlight->props.state &= ~BL_CORE_FBBLANK;
		lvds->backlight->props.power = FB_BLANK_UNBLANK;
		(void)backlight_update_status(lvds->backlight);
	} else {
		(void)pr_err("[ERROR][%s:%s] %s backlight driver not valid\n",
			     LOG_TAG, lvds->data->name, __func__);
	}
#else
	if (panel_lvds_tcc_pin_select_state(lvds->lvds_pins.p,
				       lvds->lvds_pins.blk_on) < 0) {
		dev_warn(lvds->dev,
			 "[WARN][%s:%s] %s failed set pinctrl to blk_on\r\n",
			 LOG_TAG, lvds->data->name, __func__);
	}
#endif
	return 0;
}

static int panel_lvds_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	const struct panel_lvds *lvds;
	struct drm_display_mode *disp_mode;
	int ret = 1;

	if ((panel == NULL) || (connector == NULL)) {
		ret = 0;
	} else {
		lvds = to_panel_lvds(panel);
		disp_mode = drm_mode_create(connector->dev);
		if (disp_mode == NULL) {
			ret = 0;
		} else {
			if (lvds->video_mode.pixelclock != 0U) {
				drm_display_mode_from_videomode(
					&lvds->video_mode, disp_mode);
				/* coverity[misra_c_2012_rule_10_1] */
				/* coverity[misra_c_2012_rule_10_4] */
				disp_mode->type |= DRM_MODE_TYPE_DRIVER |
					      DRM_MODE_TYPE_PREFERRED;
				drm_mode_probed_add(connector, disp_mode);
			} else {
				ret = 0;
				drm_mode_destroy(connector->dev, disp_mode);
			}
		}
	}
	return ret;
}

static const struct drm_panel_funcs panel_lvds_funcs = {
	.disable = panel_lvds_disable,
	.unprepare = panel_lvds_unprepare,
	.prepare = panel_lvds_prepare,
	.enable = panel_lvds_enable,
	.get_modes = panel_lvds_get_modes,
};

/* coverity[HIS_metric] */
static int panel_lvds_parse_dt(struct panel_lvds *lvds)
{
	struct device_node *dn = lvds->dev->of_node;
	struct device_node *np;
	int ret = 0;

	np = of_get_child_by_name(dn, "display-timings");
	if (np != NULL) {
		of_node_put(np);

		ret = of_get_videomode(dn, &lvds->video_mode,
				       OF_USE_NATIVE_MODE);
		if (ret < 0) {
			dev_err(lvds->dev,
				"[ERROR][%s:%s] %s failed to get of_get_videomode\r\n",
				LOG_TAG, lvds->data->name, __func__);
			ret = -ENODEV;
		}
	} else {
		dev_warn(
			 lvds->dev,
			 "[WARN][%s:%s] %s failed to get display-timings property\r\n",
			 LOG_TAG, lvds->data->name, __func__);
	}

	if (ret == 0) {
		/* pinctrl */
		lvds->lvds_pins.p = devm_pinctrl_get(lvds->dev);
		/* coverity[misra_c_2012_rule_11_2] */
		if (IS_ERR(lvds->lvds_pins.p)) {
			dev_err(
				lvds->dev,
				"[ERROR][%s:%s] %s failed to find pinctrl\r\n",
				LOG_TAG, lvds->data->name, __func__);
			lvds->lvds_pins.p = NULL;
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		lvds->lvds_pins.default0 =
			pinctrl_lookup_state(lvds->lvds_pins.p, "default");
		/* coverity[misra_c_2012_rule_11_2] */
		if (IS_ERR(lvds->lvds_pins.default0)) {
			dev_err(
				lvds->dev,
				"[ERROR][%s:%s] %s failed to find default\r\n",
				LOG_TAG, lvds->data->name, __func__);
			lvds->lvds_pins.default0 = NULL;
			ret = -ENODEV;
		}
	}

	if (ret == 0) {

		lvds->lvds_pins.pwr_on_1 =
			pinctrl_lookup_state(lvds->lvds_pins.p, "pwr_on_1");
		/* coverity[misra_c_2012_rule_11_2] */
		if (IS_ERR(lvds->lvds_pins.pwr_on_1)) {
			dev_warn(
				 lvds->dev,
				 "[WARN][%s:%s] %s failed to find pwr_on1\r\n",
				 LOG_TAG, lvds->data->name, __func__);
			lvds->lvds_pins.pwr_on_1 = NULL;
		}
		lvds->lvds_pins.pwr_on_2 =
			pinctrl_lookup_state(lvds->lvds_pins.p, "pwr_on_2");
		/* coverity[misra_c_2012_rule_11_2] */
		if (IS_ERR(lvds->lvds_pins.pwr_on_2)) {
			dev_warn(
				 lvds->dev,
				 "[WARN][%s:%s] %s failed to find pwr_on2\r\n",
				 LOG_TAG, lvds->data->name, __func__);
			lvds->lvds_pins.pwr_on_2 = NULL;
		}
		lvds->lvds_pins.blk_on =
			pinctrl_lookup_state(lvds->lvds_pins.p, "blk_on");
		/* coverity[misra_c_2012_rule_11_2] */
		if (IS_ERR(lvds->lvds_pins.blk_on)) {
			dev_warn(
				 lvds->dev,
				 "[WARN][%s:%s] %s failed to find blk_on\r\n",
				 LOG_TAG, lvds->data->name, __func__);
			lvds->lvds_pins.blk_on = NULL;
		}
		lvds->lvds_pins.blk_off =
			pinctrl_lookup_state(lvds->lvds_pins.p, "blk_off");
		/* coverity[misra_c_2012_rule_11_2] */
		if (IS_ERR(lvds->lvds_pins.blk_off)) {
			dev_warn(
				 lvds->dev,
				 "[WARN][%s:%s] %s failed to find blk_off\r\n",
				 LOG_TAG, lvds->data->name, __func__);
			lvds->lvds_pins.blk_off = NULL;
		}
		lvds->lvds_pins.pwr_off =
			pinctrl_lookup_state(lvds->lvds_pins.p, "power_off");
		/* coverity[misra_c_2012_rule_11_2] */
		if (IS_ERR(lvds->lvds_pins.pwr_off)) {
			dev_warn(
				 lvds->dev,
				 "[WARN][%s:%s] %s failed to find power_off\r\n",
				 LOG_TAG, lvds->data->name, __func__);
			lvds->lvds_pins.pwr_off = NULL;
		}

#if defined(CONFIG_DRM_TELECHIPS_CTRL_BACKLIGHT)
		np = of_parse_phandle(lvds->dev->of_node, "backlight", 0);
		if (np != NULL) {
			lvds->backlight = of_find_backlight_by_node(np);
			of_node_put(np);

			if (lvds->backlight == NULL) {//backlight node is not valid
				(void)pr_err("[ERROR][%s:%s] %s backlight driver not valid\n",
					LOG_TAG, lvds->data->name, __func__);
			} else {
				dev_info(
					 lvds->dev,
					 "[INFO][%s:%s] %s External backlight driver : max brightness[%d]\n",
					 LOG_TAG, lvds->data->name, __func__,
					 lvds->backlight->props.max_brightness);
			}
		} else {
			dev_info(
				 lvds->dev,
				 "[INFO][%s:%s] %s Use pinctrl backlight\n",
				 LOG_TAG, lvds->data->name, __func__);
		}
#else
		dev_info(
			 lvds->dev,
			 "[INFO][%s:%s] %s Use pinctrl backlight\n",
			 LOG_TAG, lvds->data->name, __func__);
#endif
	}
	return ret;
}

/* coverity[HIS_metric] */
static int panel_lvds_probe(struct platform_device *pdev)
{
	struct panel_lvds *lvds;
	int ret = 0;

	/* coverity[misra_c_2012_rule_10_8] */
	/* coverity[misra_c_2012_rule_11_5] */
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

		/* coverity[misra_c_2012_rule_11_5] */
		lvds->data = (const struct lvds_match_data *)
				of_device_get_match_data(&pdev->dev);
		if (lvds->data == NULL) {
			dev_err(
				lvds->dev,
				"[ERROR][%s] %s failed to find match_data from device tree\r\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
			devm_kfree(&pdev->dev, lvds);
		}
	}

	if (ret == 0) {
		ret = panel_lvds_parse_dt(lvds);
		if (ret < 0) {
			dev_err(
				lvds->dev,
				"[ERROR][%s:%s] %s failed to parse device tree\r\n",
				LOG_TAG, lvds->data->name, __func__);
			/* put dev */
			#if defined(CONFIG_DRM_TELECHIPS_CTRL_BACKLIGHT)
			put_device(&lvds->backlight->dev);
			#endif
			devm_kfree(&pdev->dev, lvds);
		}
	}

	if (ret == 0) {
		/* Register the panel. */
		drm_panel_init(&lvds->panel, lvds->dev, &panel_lvds_funcs, DRM_MODE_CONNECTOR_LVDS);
		lvds->panel.dev = lvds->dev;
		lvds->panel.funcs = &panel_lvds_funcs;

		drm_panel_add(&lvds->panel);

		dev_set_drvdata(lvds->dev, lvds);
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_10_3] */
		/* coverity[misra_c_2012_rule_10_4] */
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_15_6] */
		/* coverity[cert_dcl37_c] */
		dev_dbg(
			lvds->dev, "[DEBUG][%s] %s with [%s]\r\n",
			LOG_TAG, __func__, lvds->data->name);

		/* Version */
		lvds->major_version = DRIVER_MAJOR;
		lvds->minor_version = DRIVER_MINOR;
		lvds->patchlevel = DRIVER_PATCH;
		/* coverity[misra_c_2012_rule_10_8] */
		lvds->date =  kstrdup(DRIVER_DATE, GFP_KERNEL);

		dev_info(lvds->dev, "Initialized %s %d.%d.%d %s for %s\n",
			 lvds->data->name, lvds->major_version, lvds->minor_version,
			 lvds->patchlevel, lvds->date,
			 (lvds->dev != NULL) ?
			 dev_name(lvds->dev) : "unknown device");
	}
	return ret;
}

/* coverity[misra_c_2012_rule_8_13] */
static int panel_lvds_remove(struct platform_device *pdev)
{
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_11_5] */
	struct panel_lvds *lvds = (struct panel_lvds*)dev_get_drvdata(&pdev->dev);

	devm_pinctrl_put(lvds->lvds_pins.p);

	drm_panel_remove(&lvds->panel);
	(void)drm_panel_disable(&lvds->panel);

	#if defined(CONFIG_DRM_TELECHIPS_CTRL_BACKLIGHT)
	if (lvds->backlight)
		put_device(&lvds->backlight->dev);
	#endif
	devm_kfree(&pdev->dev, lvds);
	return 0;
}

static const struct of_device_id panel_lvds_of_table[] = {
	{ .compatible = "telechips,drm-lvds-auoc123han06",
	  .data = &lvds_auoc123han06,
	},
	{ .compatible = "telechips,drm-lvds-tm123xdhp90",
	  .data = &lvds_tm123xdhp90,
	},
	{ .compatible = "telechips,drm-lvds-fld0800",
	  .data = &lvds_fld0800,
	},
	{ /* Sentinel */ },
};

MODULE_DEVICE_TABLE(of, panel_lvds_of_table);

#ifdef CONFIG_PM
/* coverity[misra_c_2012_rule_8_13] */
static int panel_lvds_suspend(struct device *dev)
{
	//struct panel_lvds *lvds = dev_get_drvdata(dev);
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[cert_dcl37_c] */
	dev_dbg(dev, "[DEBUG][%s] %s \r\n", LOG_TAG, __func__);

	return 0;
}

/* coverity[misra_c_2012_rule_8_13] */
static int panel_lvds_resume(struct device *dev)
{
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_11_5] */
	struct panel_lvds *lvds = (struct panel_lvds*)dev_get_drvdata(dev);

	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[cert_dcl37_c] */
	dev_dbg(dev, "[DEBUG][%s] %s \r\n", LOG_TAG, __func__);

	/* Set pin status to defualt */
	if (
		panel_lvds_tcc_pin_select_state(
			lvds->lvds_pins.p,
			lvds->lvds_pins.default0) < 0) {
		dev_warn(lvds->dev,
			"[WARN][%s:%s] %s failed set pinctrl to default0\r\n",
			LOG_TAG, lvds->data->name, __func__);
	}
	if (
		panel_lvds_tcc_pin_select_state(
			lvds->lvds_pins.p,
			lvds->lvds_pins.pwr_off) < 0) {
		dev_warn(lvds->dev,
			"[WARN][%s:%s] %s failed set pinctrl to pwr_off\r\n",
			LOG_TAG, lvds->data->name, __func__);
	}
	if (
		panel_lvds_tcc_pin_select_state(
			lvds->lvds_pins.p,
			lvds->lvds_pins.blk_off) < 0) {
		dev_warn(lvds->dev,
			"[WARN][%s:%s] %s failed set pinctrl to blk_off\r\n",
			LOG_TAG, lvds->data->name, __func__);
	}
	return 0;
}
static const struct dev_pm_ops panel_lvds_pm_ops = {
	/* coverity[misra_c_2012_rule_20_7] */
	SET_SYSTEM_SLEEP_PM_OPS(panel_lvds_suspend, panel_lvds_resume)
};
#endif

static struct platform_driver panel_lvds_driver = {
	.probe		= panel_lvds_probe,
	.remove		= panel_lvds_remove,
	.driver		= {
		.name	= "panel-lvds",
		#ifdef CONFIG_PM
		.pm	= &panel_lvds_pm_ops,
		#endif
		.of_match_table = panel_lvds_of_table,
	},
};
/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_21_2] */
module_platform_driver(panel_lvds_driver);

/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_21_2] */
MODULE_DESCRIPTION("LVDS Panel Driver");
/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_5_9] */
/* coverity[misra_c_2012_rule_21_2] */
MODULE_LICENSE("GPL");
