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

#include <dsih_serdes.h>

#define LOG_TAG "DRMDSI"

#define DRIVER_DATE	"20240229"
#define DRIVER_MAJOR	1
#define DRIVER_MINOR	0
#define DRIVER_PATCH	1

//#define DSI_DEBUG
#ifdef DSI_DEBUG
#define DSI_DBG(fmt, args...) \
	dev_info(dsi->dev, "[INFO][DRMDSI] " fmt, ## args)
#else
#define DSI_DBG(fmt, args...) do {} while (false)
#endif //DSI_DEBUG

static int panel_dsi_tcc_pin_select_state(struct pinctrl *p, struct pinctrl_state *s);

static int panel_dsi_tcc_pin_select_state(struct pinctrl *p, struct pinctrl_state *s)
{
	int ret = 0;

	if ((p == NULL) || (s == NULL)) {
		ret  = -EINVAL;
	} else {
		ret = pinctrl_select_state(p, s);
	}
	return ret;
}

struct dsi_match_data {
	const char *name;
};

static const struct dsi_match_data dsi_v2_panel_0 = {
	.name = "DSI Panel 0",
};

static const struct dsi_match_data dsi_v2_panel_1 = {
	.name = "DSI Panel 1",
};

struct dsi_pin {
	struct pinctrl *p;
	struct pinctrl_state *default0;
	struct pinctrl_state *pwr_on_1;
	struct pinctrl_state *pwr_on_2;
	struct pinctrl_state *blk_on;
	struct pinctrl_state *blk_off;
	struct pinctrl_state *pwr_off;
	struct pinctrl_state *ser_pwdn;
};
struct panel_dsi {
	struct drm_panel panel;
	struct device *dev;

	const char *label;
	unsigned int width;
	unsigned int height;
	unsigned int bus_format;
	unsigned int lanes;
	struct videomode video_mode;

	#if defined(CONFIG_DRM_TELECHIPS_CTRL_BACKLIGHT)
	struct backlight_device *backlight;
	#endif

	struct gpio_desc *enable_gpio;
	struct gpio_desc *reset_gpio;

	/* Device driver data */
	const struct dsi_match_data *data;

	struct dsi_pin dsi_pins;

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

static inline struct panel_dsi *to_panel_dsi(struct drm_panel *panel)
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
	return container_of(panel, struct panel_dsi, panel);
}

static int panel_dsi_disable(struct drm_panel *panel)
{
	const struct panel_dsi *dsi = to_panel_dsi(panel);

	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[cert_dcl37_c] */
	dev_err(
		dsi->dev, "[DEBUG][%s] %s with %s \r\n", LOG_TAG, __func__,
		dsi->data->name);

#if defined(CONFIG_DRM_TELECHIPS_CTRL_BACKLIGHT)
	if (dsi->backlight) {
		dsi->backlight->props.power = FB_BLANK_POWERDOWN;
		dsi->backlight->props.state |= BL_CORE_FBBLANK;
		backlight_update_status(dsi->backlight);
	} else {
		dev_err(dsi->dev,
			"[ERROR][%s:%s] %s backlight driver not valid\n",
			LOG_TAG, dsi->data->name, __func__);
	}
#else
	if (panel_dsi_tcc_pin_select_state(dsi->dsi_pins.p,
				       dsi->dsi_pins.blk_off) < 0) {
		dev_warn(dsi->dev,
			 "[WARN][%s:%s] %s failed set pinctrl to blk_off\r\n",
			 LOG_TAG, dsi->data->name, __func__);
	}
#endif
	// if (panel_dsi_tcc_pin_select_state(dsi->dsi_pins.p,
	// 			       dsi->dsi_pins.pwr_off) < 0) {
	// 	dev_warn(dsi->dev,
	// 		 "[WARN][%s:%s] %s failed set pinctrl to pwr_off\r\n",
	// 		 LOG_TAG, dsi->data->name, __func__);
	// }

	return 0;
}

static int panel_dsi_unprepare(struct drm_panel *panel)
{
	const struct panel_dsi *dsi = to_panel_dsi(panel);

	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[cert_dcl37_c] */
	dev_dbg(
		dsi->dev, "[DEBUG][%s:%s] %s \r\n",
		LOG_TAG, dsi->data->name, __func__);

	return 0;
}

/* coverity[HIS_metric] */
static int panel_dsi_prepare(struct drm_panel *panel)
{
	const struct panel_dsi *dsi = to_panel_dsi(panel);

	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[cert_dcl37_c] */
	dev_err(
		dsi->dev, "[DEBUG][%s:%s] %s \r\n",
		LOG_TAG, dsi->data->name, __func__);

	if (panel_dsi_tcc_pin_select_state(dsi->dsi_pins.p,
				       dsi->dsi_pins.ser_pwdn) < 0) {
		dev_err(dsi->dev,
			 "[WARN][%s:%s] %s failed set pinctrl to ser_pwdn\r\n",
			 LOG_TAG, dsi->data->name, __func__);
	}

	if (panel_dsi_tcc_pin_select_state(dsi->dsi_pins.p,
				       dsi->dsi_pins.pwr_on_1) < 0) {
		dev_warn(dsi->dev,
			 "[WARN][%s:%s] %s failed set pinctrl to pwr_on_1\r\n",
			 LOG_TAG, dsi->data->name, __func__);
	}

	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_12_1] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[cert_dcl37_c] */
	/* coverity[cert_pre31_c] */
	udelay(20);

	if (panel_dsi_tcc_pin_select_state(dsi->dsi_pins.p,
				       dsi->dsi_pins.pwr_on_2) < 0) {
		dev_warn(dsi->dev,
			 "[WARN][%s:%s] %s failed set pinctrl to pwr_on_2\r\n",
			 LOG_TAG, dsi->data->name, __func__);
	}

	return 0;
}

static int panel_dsi_enable(struct drm_panel *panel)
{
	const struct panel_dsi *dsi = to_panel_dsi(panel);

	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[cert_dcl37_c] */
	dev_err(
		dsi->dev, "[DEBUG][%s:%s] %s \r\n",
		LOG_TAG, dsi->data->name, __func__);

#if defined(CONFIG_DRM_TELECHIPS_CTRL_BACKLIGHT)
	if (dsi->backlight != NULL) {
		/* coverity[cert_int31_c] */
		/* coverity[cert_int02_c] */
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_10_4] */
		dsi->backlight->props.state &= ~BL_CORE_FBBLANK;
		dsi->backlight->props.power = FB_BLANK_UNBLANK;
		(void)backlight_update_status(dsi->backlight);
	} else {
		(void)pr_err("[ERROR][%s:%s] %s backlight driver not valid\n",
			     LOG_TAG, dsi->data->name, __func__);
	}
#else
	if (panel_dsi_tcc_pin_select_state(dsi->dsi_pins.p,
				       dsi->dsi_pins.blk_on) < 0) {
		dev_err(dsi->dev,
			 "[WARN][%s:%s] %s failed set pinctrl to blk_on\r\n",
			 LOG_TAG, dsi->data->name, __func__);
	}
#endif
	return 0;
}

static int panel_dsi_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	const struct panel_dsi *dsi;
	struct drm_display_mode *disp_mode;
	int ret = 1;

	if ((panel == NULL) || (connector == NULL)) {
		ret = 0;
	} else {
		dsi = to_panel_dsi(panel);
		disp_mode = drm_mode_create(connector->dev);
		if (disp_mode == NULL) {
			ret = 0;
		} else {
			if (dsi->video_mode.pixelclock != 0U) {
				drm_display_mode_from_videomode(
					&dsi->video_mode, disp_mode);
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

static const struct drm_panel_funcs panel_dsi_funcs = {
	.disable = panel_dsi_disable,
	.unprepare = panel_dsi_unprepare,
	.prepare = panel_dsi_prepare,
	.enable = panel_dsi_enable,
	.get_modes = panel_dsi_get_modes,
};

/* coverity[HIS_metric] */
static int panel_dsi_parse_dt(struct panel_dsi *dsi)
{
	struct device_node *dn = dsi->dev->of_node;
	struct device_node *np;
	int ret = 0;

	np = of_get_child_by_name(dn, "display-timings");
	if (np != NULL) {
		of_node_put(np);

		ret = of_get_videomode(dn, &dsi->video_mode,
				       OF_USE_NATIVE_MODE);
		if (ret < 0) {
			dev_err(dsi->dev,
				"[ERROR][%s:%s] %s failed to get of_get_videomode\r\n",
				LOG_TAG, dsi->data->name, __func__);
			ret = -ENODEV;
		}
	} else {
		dev_warn(
			 dsi->dev,
			 "[WARN][%s:%s] %s failed to get display-timings property\r\n",
			 LOG_TAG, dsi->data->name, __func__);
	}

	if (ret == 0) {
		/* pinctrl */
		dsi->dsi_pins.p = devm_pinctrl_get(dsi->dev);
		/* coverity[misra_c_2012_rule_11_2] */
		if (IS_ERR(dsi->dsi_pins.p)) {
			dev_err(
				dsi->dev,
				"[ERROR][%s:%s] %s failed to find pinctrl\r\n",
				LOG_TAG, dsi->data->name, __func__);
			dsi->dsi_pins.p = NULL;
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		dsi->dsi_pins.default0 =
			pinctrl_lookup_state(dsi->dsi_pins.p, "default");
		/* coverity[misra_c_2012_rule_11_2] */
		if (IS_ERR(dsi->dsi_pins.default0)) {
			dev_err(
				dsi->dev,
				"[ERROR][%s:%s] %s failed to find default\r\n",
				LOG_TAG, dsi->data->name, __func__);
			dsi->dsi_pins.default0 = NULL;
			ret = -ENODEV;
		}
	}

	if (ret == 0) {

		dsi->dsi_pins.pwr_on_1 =
			pinctrl_lookup_state(dsi->dsi_pins.p, "pwr_on_1");
		/* coverity[misra_c_2012_rule_11_2] */
		if (IS_ERR(dsi->dsi_pins.pwr_on_1)) {
			dev_err(
				 dsi->dev,
				 "[WARN][%s:%s] %s failed to find pwr_on1\r\n",
				 LOG_TAG, dsi->data->name, __func__);
			dsi->dsi_pins.pwr_on_1 = NULL;
		}
		dsi->dsi_pins.pwr_on_2 =
			pinctrl_lookup_state(dsi->dsi_pins.p, "pwr_on_2");
		/* coverity[misra_c_2012_rule_11_2] */
		if (IS_ERR(dsi->dsi_pins.pwr_on_2)) {
			dev_err(
				 dsi->dev,
				 "[WARN][%s:%s] %s failed to find pwr_on2\r\n",
				 LOG_TAG, dsi->data->name, __func__);
			dsi->dsi_pins.pwr_on_2 = NULL;
		}
		dsi->dsi_pins.blk_on =
			pinctrl_lookup_state(dsi->dsi_pins.p, "blk_on");
		/* coverity[misra_c_2012_rule_11_2] */
		if (IS_ERR(dsi->dsi_pins.blk_on)) {
			dev_err(
				 dsi->dev,
				 "[WARN][%s:%s] %s failed to find blk_on\r\n",
				 LOG_TAG, dsi->data->name, __func__);
			dsi->dsi_pins.blk_on = NULL;
		}
		dsi->dsi_pins.blk_off =
			pinctrl_lookup_state(dsi->dsi_pins.p, "blk_off");
		/* coverity[misra_c_2012_rule_11_2] */
		if (IS_ERR(dsi->dsi_pins.blk_off)) {
			dev_err(
				 dsi->dev,
				 "[WARN][%s:%s] %s failed to find blk_off\r\n",
				 LOG_TAG, dsi->data->name, __func__);
			dsi->dsi_pins.blk_off = NULL;
		}
		dsi->dsi_pins.pwr_off =
			pinctrl_lookup_state(dsi->dsi_pins.p, "power_off");
		/* coverity[misra_c_2012_rule_11_2] */
		if (IS_ERR(dsi->dsi_pins.pwr_off)) {
			dev_err(
				 dsi->dev,
				 "[WARN][%s:%s] %s failed to find power_off\r\n",
				 LOG_TAG, dsi->data->name, __func__);
			dsi->dsi_pins.pwr_off = NULL;
		}

		dsi->dsi_pins.ser_pwdn =
			pinctrl_lookup_state(dsi->dsi_pins.p, "ser_pwdn");
		/* coverity[misra_c_2012_rule_11_2] */
		if (IS_ERR(dsi->dsi_pins.ser_pwdn)) {
			dev_err(dsi->dev, "[WARN][%s] %s failed to find ser_pwdn\n",
				 LOG_TAG, __func__);
			dsi->dsi_pins.ser_pwdn = NULL;
		}

#if defined(CONFIG_DRM_TELECHIPS_CTRL_BACKLIGHT)
		np = of_parse_phandle(dsi->dev->of_node, "backlight", 0);
		if (np != NULL) {
			dsi->backlight = of_find_backlight_by_node(np);
			of_node_put(np);

			if (dsi->backlight == NULL) {//backlight node is not valid
				(void)pr_err("[ERROR][%s:%s] %s backlight driver not valid\n",
					LOG_TAG, dsi->data->name, __func__);
			} else {
				dev_info(
					 dsi->dev,
					 "[INFO][%s:%s] %s External backlight driver : max brightness[%d]\n",
					 LOG_TAG, dsi->data->name, __func__,
					 dsi->backlight->props.max_brightness);
			}
		} else {
			dev_info(
				 dsi->dev,
				 "[INFO][%s:%s] %s Use DRM ctrl backlight\n",
				 LOG_TAG, dsi->data->name, __func__);
		}
#else
		dev_info(
			 dsi->dev,
			 "[INFO][%s:%s] %s Use pinctrl backlight\n",
			 LOG_TAG, dsi->data->name, __func__);
#endif
	}

	if (ret == 0) {
		if (of_property_read_u32_index(dn, "serdes-lanes", 0, &dsi->lanes) < 0) {
			dev_err(dsi->dev,
				"[ERROR][%s] %s failed to get dsi panel lanes\n",
				LOG_TAG, __func__);
			dsi->lanes = 2;
			ret = -ENODEV;
		}
	}
	return ret;
}

/* coverity[HIS_metric] */
static int panel_dsi_probe(struct platform_device *pdev)
{
	struct panel_dsi *dsi;
	int ret = 0;

	/* coverity[misra_c_2012_rule_10_8] */
	/* coverity[misra_c_2012_rule_11_5] */
	dsi = devm_kzalloc(&pdev->dev, sizeof(*dsi), GFP_KERNEL);
	if (dsi != NULL) {
		dsi->dev = &pdev->dev;
		dev_err(
			dsi->dev," probe dsi \n");

		/* coverity[misra_c_2012_rule_11_5] */
		dsi->data = (const struct dsi_match_data *)
				of_device_get_match_data(&pdev->dev);
		if (dsi->data == NULL) {
			dev_err(
				dsi->dev,
				"[ERROR][%s] %s failed to find match_data from device tree\r\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
			devm_kfree(&pdev->dev, dsi);
		}

		if (ret == 0) {
			ret = panel_dsi_parse_dt(dsi);
			if (ret < 0) {
				dev_err(
					dsi->dev,
					"[ERROR][%s:%s] %s failed to parse device tree\r\n",
					LOG_TAG, dsi->data->name, __func__);
				/* put dev */
				#if defined(CONFIG_DRM_TELECHIPS_CTRL_BACKLIGHT)
				put_device(&dsi->backlight->dev);
				#endif
				devm_kfree(&pdev->dev, dsi);
			}
		}

		if (ret == 0) {
			/* Register the panel. */
			drm_panel_init(&dsi->panel, dsi->dev, &panel_dsi_funcs, DRM_MODE_CONNECTOR_DSI);
			dsi->panel.dev = dsi->dev;
			dsi->panel.funcs = &panel_dsi_funcs;

			drm_panel_add(&dsi->panel);

			dev_set_drvdata(dsi->dev, dsi);
			/* coverity[misra_c_2012_rule_10_1] */
			/* coverity[misra_c_2012_rule_10_3] */
			/* coverity[misra_c_2012_rule_10_4] */
			/* coverity[misra_c_2012_rule_14_4] */
			/* coverity[misra_c_2012_rule_15_6] */
			/* coverity[cert_dcl37_c] */
			dev_dbg(
				dsi->dev, "[DEBUG][%s] %s with [%s]\r\n",
				LOG_TAG, __func__, dsi->data->name);

			/* Version */
			dsi->major_version = DRIVER_MAJOR;
			dsi->minor_version = DRIVER_MINOR;
			dsi->patchlevel = DRIVER_PATCH;
			/* coverity[misra_c_2012_rule_10_8] */
			dsi->date =  kstrdup(DRIVER_DATE, GFP_KERNEL);

			dev_info(dsi->dev, "Initialized %s %d.%d.%d %s for %s\n",
				dsi->data->name, dsi->major_version, dsi->minor_version,
				dsi->patchlevel, dsi->date,
				(dsi->dev != NULL) ?
				dev_name(dsi->dev) : "unknown device");
		}
	} else {
		(void)pr_err("[ERROR][%s] %s failed to alloc device context\n",
			LOG_TAG, __func__);

		ret = -ENODEV;
	}

	return ret;
}

/* coverity[misra_c_2012_rule_8_13] */
static int panel_dsi_remove(struct platform_device *pdev)
{
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_11_5] */
	struct panel_dsi *dsi = (struct panel_dsi*)dev_get_drvdata(&pdev->dev);

	devm_pinctrl_put(dsi->dsi_pins.p);

	drm_panel_remove(&dsi->panel);
	(void)drm_panel_disable(&dsi->panel);

	#if defined(CONFIG_DRM_TELECHIPS_CTRL_BACKLIGHT)
	if (dsi->backlight)
		put_device(&dsi->backlight->dev);
	#endif
	devm_kfree(&pdev->dev, dsi);

	return 0;
}

static const struct of_device_id panel_dsi_of_table[] = {
	{ .compatible = "telechips,drm-dsi-0",
	  .data = &dsi_v2_panel_0,
	},
	{ .compatible = "telechips,drm-dsi-1",
	  .data = &dsi_v2_panel_1,
	},
	{ /* Sentinel */ },
};

MODULE_DEVICE_TABLE(of, panel_dsi_of_table);

#ifdef CONFIG_PM
/* coverity[misra_c_2012_rule_8_13] */
static int panel_dsi_suspend(struct device *dev)
{
	//struct panel_dsi *dsi = dev_get_drvdata(dev);
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
static int panel_dsi_resume(struct device *dev)
{
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_11_5] */
	struct panel_dsi *dsi = (struct panel_dsi*)dev_get_drvdata(dev);

	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[cert_dcl37_c] */
	dev_err(dev, "[DEBUG][%s] %s \r\n", LOG_TAG, __func__);

#ifdef CONFIG_TCC807X_CA55_SUB
	max9xxxx_reg_set(dsi->lanes, dsi->video_mode);
#endif
	/* Set pin status to defualt */
	if (
		panel_dsi_tcc_pin_select_state(
			dsi->dsi_pins.p,
			dsi->dsi_pins.default0) < 0) {
		dev_warn(dsi->dev,
			"[WARN][%s:%s] %s failed set pinctrl to default0\r\n",
			LOG_TAG, dsi->data->name, __func__);
	}
	if (
		panel_dsi_tcc_pin_select_state(
			dsi->dsi_pins.p,
			dsi->dsi_pins.pwr_off) < 0) {
		dev_warn(dsi->dev,
			"[WARN][%s:%s] %s failed set pinctrl to pwr_off\r\n",
			LOG_TAG, dsi->data->name, __func__);
	}
	if (
		panel_dsi_tcc_pin_select_state(
			dsi->dsi_pins.p,
			dsi->dsi_pins.blk_off) < 0) {
		dev_warn(dsi->dev,
			"[WARN][%s:%s] %s failed set pinctrl to blk_off\r\n",
			LOG_TAG, dsi->data->name, __func__);
	}

	return 0;
}
static const struct dev_pm_ops panel_dsi_pm_ops = {
	/* coverity[misra_c_2012_rule_20_7] */
	SET_SYSTEM_SLEEP_PM_OPS(panel_dsi_suspend, panel_dsi_resume)
};
#endif

static struct platform_driver panel_dsi_driver = {
	.probe		= panel_dsi_probe,
	.remove		= panel_dsi_remove,
	.driver		= {
		.name	= "panel-dsi",
		#ifdef CONFIG_PM
		.pm	= &panel_dsi_pm_ops,
		#endif
		.of_match_table = panel_dsi_of_table,
	},
};
/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_21_2] */
module_platform_driver(panel_dsi_driver);

/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_21_2] */
MODULE_DESCRIPTION("DSI Panel Driver");
/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_21_2] */
MODULE_LICENSE("GPL");
