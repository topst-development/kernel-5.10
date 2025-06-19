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
#include <linux/proc_fs.h>
#include <uapi/linux/media-bus-format.h>

#include <video/display_timing.h>
#include <video/of_videomode.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>
#include <panel_helper.h>

#include "dsih_includes.h"
#include "dsih_core.h"
#include "dsih_hal.h"
#include "dsih_dphy.h"
#include "dsih_serdes.h"

#define LOG_TAG "FB_DSI"

#define DRIVER_DATE	"20230330"
#define DRIVER_MAJOR	1
#define DRIVER_MINOR	1
#define DRIVER_PATCH	0

//#define DSI_DEBUG
#ifdef DSI_DEBUG
#define DSI_DEBUG(fmt, args...) pr_info("[INFO][FBDSI] " fmt, ## args)
#else
#define DSI_DEBUG(fmt, args...)
#endif

struct dsi_match_data {
	const char *name;
};

static const struct dsi_match_data dsi_panel = {
	.name = "TCC_DSI",
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
	struct fb_panel panel;
	struct device *dev;

	const char *label;
	unsigned int width;
	unsigned int height;
	struct videomode video_mode;

    struct mipi_dsi_dev dsi_info;

	#if defined(CONFIG_FB_TCC_CTRL_BACKLIGHT)
	struct backlight_device *backlight;
	#endif

	struct gpio_desc *enable_gpio;
	struct gpio_desc *reset_gpio;

	/* Device driver data */
	const struct dsi_match_data *data;

	/* DSI data from device tree */
	unsigned int sdm_bypass;
	unsigned int trvc_bypass;
	unsigned int display_ch;
	unsigned int lcdc_mux_select;
	unsigned long disp_pclk;

	struct dsi_pin dsi_pins;
	int enabled;

	/* Version ---------------------*/
	/** @major: driver major number */
	int major;
	/** @minor: driver minor number */
	int minor;
	/** @patchlevel: driver patch level */
	int patchlevel;
	/** @date: driver date */
	char *date;

	struct proc_dir_entry *dsi_proc_dir;
	struct proc_dir_entry *dsi_proc_str;
};

int dsi_proc_open(struct inode *pinode, struct file *filp);
int dsi_proc_close(struct inode *pinode, struct file *filp);
ssize_t dsi_ext_proc_read_str_status(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set);

static const struct proc_ops proc_fops_str_data = {
	.proc_open    = dsi_proc_open,
	.proc_release = dsi_proc_close,
	.proc_read    = dsi_ext_proc_read_str_status,
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

static inline struct panel_dsi *to_panel_dsi(struct fb_panel *panel)
{
	return container_of(panel, struct panel_dsi, panel);
}

static int panel_dsi_disable(struct fb_panel *panel)
{
	struct panel_dsi *dsi = to_panel_dsi(panel);

	dev_dbg(dsi->dev, "[DEBUG][%s] %s with %s\n",
		LOG_TAG, __func__, dsi->data->name);

	if (dsi->enabled != 0) {
		#if defined(CONFIG_FB_TCC_CTRL_BACKLIGHT)
		if (dsi->backlight != NULL) {
			dsi->backlight->props.power = FB_BLANK_POWERDOWN;
			dsi->backlight->props.state |= BL_CORE_FBBLANK;
			(void)backlight_update_status(dsi->backlight);
		} else {
			(void)pr_err("[ERROR][%s] : backlight driver not valid\n",
			       __func__);
		}
		#else
		if (
		    panel_tcc_pin_select_state(
					       dsi->dsi_pins.p,
					       dsi->dsi_pins.blk_off) < 0) {
			dev_warn(dsi->dev,
				 "[WARN][%s:%s] %s failed set pinctrl to pwr_off\r\n",
				 LOG_TAG, dsi->data->name, __func__);
		}
		#endif
		tcc_dsi_platform_init(&dsi->dsi_info , 1);
		dsi->enabled = 0;
	} else {
		dev_dbg(dsi->dev, "[DEBUG][%s] %s with %s - already disabled\n",
			LOG_TAG, __func__, dsi->data->name);
	}

	return 0;
}

static int panel_dsi_unprepare(struct fb_panel *panel)
{
	const struct panel_dsi *dsi = to_panel_dsi(panel);

	dev_dbg(dsi->dev, "[DEBUG][%s] %s with %s\n",
		LOG_TAG, __func__, dsi->data->name);

	return 0;
}

static int panel_dsi_prepare(struct fb_panel *panel)
{
	const struct panel_dsi *dsi = to_panel_dsi(panel);

	dev_dbg(dsi->dev, "[DEBUG][%s] %s with %s\n",
		LOG_TAG, __func__, dsi->data->name);

	if (dsi->enabled == 0) {
		if (
			panel_tcc_pin_select_state(
				dsi->dsi_pins.p,
				dsi->dsi_pins.pwr_on_1) < 0) {
			dev_warn(dsi->dev,
				"[WARN][%s:%s] %s failed set pinctrl to pwr_on_1\r\n",
				LOG_TAG, dsi->data->name, __func__);
		}

		udelay(20);

		if (
			panel_tcc_pin_select_state(
				dsi->dsi_pins.p,
				dsi->dsi_pins.pwr_on_2) < 0) {
			dev_warn(dsi->dev,
				"[WARN][%s:%s] %s failed set pinctrl to pwr_on_2\r\n",
				LOG_TAG, dsi->data->name, __func__);
		}

	} else {
		dev_dbg(dsi->dev, "[DEBUG][%s] %s with %s - already enabled\n",
			LOG_TAG, __func__, dsi->data->name);
	}

	return 0;
}

static void tcc_dsi_ch_select(const struct panel_dsi *dsi)
{
	void __iomem *reg =
		(void __iomem *)(dsi->dsi_info.cfg_addr + 0x10);
	unsigned int val;

	if(dsi->dsi_info.port == 0U){
		val = __raw_readl(reg) & ~(0x7U << 0x0);
		val |= dsi->lcdc_mux_select << (0x0);
	} else {
		val = __raw_readl(reg) & ~(0x7U << 0x3);
		val |= dsi->lcdc_mux_select << (0x3);
	}

	val &= ~((unsigned int)0x3U << 0x8);
	val |= (dsi->sdm_bypass << 0x9) | (dsi->trvc_bypass << 0x8);

	__raw_writel(val, reg);
}

static int panel_dsi_enable(struct fb_panel *panel)
{
	struct panel_dsi *dsi = to_panel_dsi(panel);

	dev_dbg(dsi->dev, "[DEBUG][%s] %s with %s\n",
		LOG_TAG, __func__, dsi->data->name);

	if (dsi->enabled == 0) {
		#if defined(CONFIG_FB_TCC_CTRL_BACKLIGHT)
		if (dsi->backlight != NULL) {
			dsi->backlight->props.state &= ~BL_CORE_FBBLANK;
			dsi->backlight->props.power = FB_BLANK_UNBLANK;
			(void)backlight_update_status(dsi->backlight);
		} else {
			(void)pr_err("[ERROR][%s] : backlight driver not valid\n",
			       __func__);
		}
		#else
		if (
			panel_tcc_pin_select_state(
				dsi->dsi_pins.p,
				dsi->dsi_pins.blk_on) < 0) {
			dev_warn(dsi->dev,
				"[WARN][%s:%s] %s failed set pinctrl to blk_on\r\n",
				LOG_TAG, dsi->data->name, __func__);
		}
		#endif
		tcc_dsi_ch_select(dsi);
		(void)max9xxxx_reg_set(dsi->dsi_info.phy_cfg.phy_lanes, dsi->video_mode);
        tcc_dsi_phy_init(&dsi->dsi_info);
        mdelay(200);
        tcc_dsi_platform_init(&dsi->dsi_info, 0);
		dsi->enabled = 1;
	} else {
		dev_dbg(dsi->dev, "[DEBUG][%s] %s with %s - already enabled\n",
			LOG_TAG, __func__, dsi->data->name);
	}

	return 0;
}

static int panel_dsi_get_videomode(
	struct fb_panel *panel,
	struct videomode *vm)
{
	const struct panel_dsi *dsi = to_panel_dsi(panel);

	(void)memcpy(vm, &dsi->video_mode, sizeof(*vm));

	return 0;
}

static const struct fb_panel_funcs panel_dsi_funcs = {
	.disable = panel_dsi_disable,
	.unprepare = panel_dsi_unprepare,
	.prepare = panel_dsi_prepare,
	.enable = panel_dsi_enable,
	.get_videomode = panel_dsi_get_videomode,
};

#if 1
static int panel_dsi_parse_pins(struct panel_dsi *dsi)
{
	int ret = 0;
#if defined(CONFIG_FB_TCC_CTRL_BACKLIGHT)
	struct device_node *np;
#endif

	if (IS_ERR(dsi->dsi_pins.p)) {
		dev_err(dsi->dev, "[ERROR][%s] %s failed to find pinctrl\n",
			LOG_TAG, __func__);
		dsi->dsi_pins.p = NULL;
		ret = -ENODEV;
	}

	if (ret == 0) {
		dsi->dsi_pins.default0 =
			pinctrl_lookup_state(dsi->dsi_pins.p, "default");
		if (IS_ERR(dsi->dsi_pins.default0)) {
			dev_err(dsi->dev, "[ERROR][%s] %s failed to find default\n",
				LOG_TAG, __func__);
			dsi->dsi_pins.default0 = NULL;
			ret = -ENODEV;
		}
	}

	if (ret == 0) {

		dsi->dsi_pins.pwr_on_1 =
			pinctrl_lookup_state(dsi->dsi_pins.p, "pwr_on_1");
		if (IS_ERR(dsi->dsi_pins.pwr_on_1)) {
			dev_warn(dsi->dev, "[WARN][%s] %s failed to find pwr_on1\n",
				 LOG_TAG, __func__);
			dsi->dsi_pins.pwr_on_1 = NULL;
		}

		dsi->dsi_pins.pwr_on_2 =
			pinctrl_lookup_state(dsi->dsi_pins.p, "pwr_on_2");
		if (IS_ERR(dsi->dsi_pins.pwr_on_2)) {
			dev_warn(dsi->dev, "[WARN][%s] %s failed to find pwr_on2\n",
				 LOG_TAG, __func__);
			dsi->dsi_pins.pwr_on_2 = NULL;
		}

		dsi->dsi_pins.blk_on =
			pinctrl_lookup_state(dsi->dsi_pins.p, "blk_on");
		if (IS_ERR(dsi->dsi_pins.blk_on)) {
			dev_warn(dsi->dev, "[WARN][%s] %s failed to find blk_on\n",
				 LOG_TAG, __func__);
			dsi->dsi_pins.blk_on = NULL;
		}

		dsi->dsi_pins.blk_off =
			pinctrl_lookup_state(dsi->dsi_pins.p, "blk_off");
		if (IS_ERR(dsi->dsi_pins.blk_off)) {
			dev_warn(dsi->dev, "[WARN][%s] %s failed to find blk_off\n",
				 LOG_TAG, __func__);
			dsi->dsi_pins.blk_off = NULL;
		}

		dsi->dsi_pins.pwr_off =
			pinctrl_lookup_state(dsi->dsi_pins.p, "power_off");
		if (IS_ERR(dsi->dsi_pins.pwr_off)) {
			dev_warn(dsi->dev, "[WARN][%s] %s failed to find power_off\n",
				 LOG_TAG, __func__);
			dsi->dsi_pins.pwr_off = NULL;
		}

		dsi->dsi_pins.ser_pwdn =
			pinctrl_lookup_state(dsi->dsi_pins.p, "ser_pwdn");
		if (IS_ERR(dsi->dsi_pins.pwr_off)) {
			dev_warn(dsi->dev, "[WARN][%s] %s failed to find ser_pwdn\n",
				 LOG_TAG, __func__);
			dsi->dsi_pins.pwr_off = NULL;
		}
#if defined(CONFIG_FB_TCC_CTRL_BACKLIGHT)
		np = of_parse_phandle(dsi->dev->of_node, "backlight", 0);
		if (np != NULL) {
			dsi->backlight = of_find_backlight_by_node(np);
			of_node_put(np);
			dev_err(dsi->dev, "[INFO][%s] %s: backlight node found\n",
				 LOG_TAG, __func__);
			if (dsi->backlight == NULL) { //backlight node is not valid
				dev_err(dsi->dev, "[ERROR][%s] %s: backlight driver not valid\n",
					     LOG_TAG, __func__);
			} else {
				dev_err(dsi->dev, "[INFO][%s] %s: External backlight driver : max brightness[%d]\n",
					LOG_TAG, __func__,
					dsi->backlight->props.max_brightness);
			}
		} else {
			dev_err(dsi->dev, "[INFO][%s] %s: Use fb pinctrl backlight\n",
				 LOG_TAG, __func__);
		}
#else
		 dev_err(dsi->dev, "[INFO][%s] %s: Use gpio pinctrl backlight\n",
		  LOG_TAG, __func__);
#endif
	}

	return ret;
}
#endif

static int panel_dsi_parse_dt(struct platform_device *pDev, struct panel_dsi *dsi)
{
	struct device_node *dn = dsi->dev->of_node;
	struct device_node *np;
	const struct clk *disp_peri_clk = NULL;
    const struct resource *pstresource;
    void __iomem *pioaddr;
    struct mipi_dsi_dev *dsi_info = &dsi->dsi_info;

	int ret = 0;

	//todo dsi timing function
	if (of_property_read_u32_index(dn, "dsi-port", 0, &dsi_info->port) < 0) {
		dev_err(dsi->dev,
			"[ERROR][%s] %s failed to get dsi-port property\n",
			LOG_TAG, __func__);
		ret = -ENODEV;
	}
    dev_info(dsi->dev, "port %d \n", dsi_info->port);

	/* Get Resource DSI Core */
    pstresource = platform_get_resource(pDev, IORESOURCE_MEM, dsi_info->port);
    if (pstresource == NULL) {
		dev_err(dsi->dev,
                "[ERROR][%s]can't get %u device resource\n", 
                    LOG_TAG, dsi_info->port);
		ret = -ENODEV;
	}
	if(ret == 0) {
	    pioaddr = devm_ioremap(&pDev->dev, pstresource->start, pstresource->end - pstresource->start);
	    if (pioaddr == NULL) {
			dev_err(dsi->dev,
	                "[ERROR][%s]Failed to remap device resource\n", 
	                    LOG_TAG);
			ret = -ENODEV;
		}
	    dsi_info->core_addr = pioaddr;
	    dsi_info->phy_addr = pioaddr + DSI_PHY_OFFSET;
	}

	/* Get Resouces CAM CFG */
	pstresource = platform_get_resource(pDev, IORESOURCE_MEM, 2);
    if (pstresource == NULL) {
		dev_err(dsi->dev,
                "[ERROR][%s]can't get CAM CFG device resource\n", 
                    LOG_TAG);
		ret = -ENODEV;
	}
	if(ret == 0) {
	    pioaddr = devm_ioremap(&pDev->dev, pstresource->start, pstresource->end - pstresource->start);
	    if (pioaddr == NULL) {
			dev_err(dsi->dev,
	                "[ERROR][%s]Failed to remap device resource\n", 
	                    LOG_TAG);
			ret = -ENODEV;
		}
		dsi_info->cfg_addr = pioaddr;
	}

    if (ret == 0) {
		if (of_property_read_u32_index(dn, "auto-mode", 0, &dsi_info->automode) < 0) {
			dev_err(dsi->dev,
				"[ERROR][%s] %s failed to get automode property\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
		}
	}
    dev_info(dsi->dev, "automode %d \n", dsi_info->automode);

    if (ret == 0) {
		if (of_property_read_u32_index(dn, "no_of_lane", 0, &dsi_info->phy_cfg.phy_lanes) < 0) {
			dev_err(dsi->dev,
				"[ERROR][%s] %s failed to get no_of_lane property\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
		}
	}
    dev_info(dsi->dev, "lane %d \n", dsi_info->phy_cfg.phy_lanes);

    if (ret == 0) {
		if (of_property_read_u32_index(dn, "dsi-sdm-bypass", 0, &dsi->sdm_bypass) < 0) {
			dev_err(dsi->dev,
				"[ERROR][%s] %s failed to get dsi-sdm-bypass property\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
		}
	}
    dev_info(dsi->dev, "dsi-sdm-bypass %d \n", dsi->sdm_bypass);

	
    if (ret == 0) {
		if (of_property_read_u32_index(dn, "dsi-trvc-bypass", 0, &dsi->trvc_bypass) < 0) {
			dev_err(dsi->dev,
				"[ERROR][%s] %s failed to get dsi-trvc-bypass property\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
		}
	}
    dev_info(dsi->dev, "trvc_bypass %d \n", dsi->trvc_bypass);

    if (ret == 0) {
		if (of_property_read_u32_index(dn, "dsi-display-source", 0, &dsi->display_ch) < 0) {
			dev_err(dsi->dev,
				"[ERROR][%s] %s failed to get dsi-display-source property\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
		}
	}
    dev_info(dsi->dev, "display_ch %d \n", dsi->display_ch);

	switch(dsi->display_ch) {
		case 0 :
			disp_peri_clk = of_clk_get_by_name(dn, "disp0-clk");
			break;
		case 1 :
			disp_peri_clk = of_clk_get_by_name(dn, "disp1-clk");
			break;
		case 2 :
			disp_peri_clk = of_clk_get_by_name(dn, "disp2-clk");
			break;
		case 3 :
			disp_peri_clk = of_clk_get_by_name(dn, "disp3-clk");
			break;
		case 4 :
			disp_peri_clk = of_clk_get_by_name(dn, "disp4-clk");
			break;
		default :
			(void)pr_err(
					"[ERROR][DSI] error in %s: can not get ddc clock\n",
					__func__);
			break;
	}

    if (ret == 0) {
		if (of_property_read_u32_index(dn, "lcdc-mux-select", 0, &dsi->lcdc_mux_select) < 0) {
			dev_err(dsi->dev,
				"[ERROR][%s] %s failed to get lcdc-mux-select property\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
		}
	}
    dev_info(dsi->dev, "lcdc_mux_select %d \n", dsi->lcdc_mux_select);

	if (ret == 0) {
		np = of_get_child_by_name(dn, "display-timings");
		if (np != NULL) {
			of_node_put(np);

			ret = of_get_videomode(dn, &dsi->video_mode, OF_USE_NATIVE_MODE);
			if (ret < 0) {
				dev_err(dsi->dev,
					"[ERROR][%s] %s failed to get of_get_videomode\n",
					LOG_TAG, __func__);
				ret = -ENODEV;
			}
		} else {
			dev_err(dsi->dev,
				"[ERROR][%s] %s failed to get display-timings property\n",
				LOG_TAG, __func__);
		}
	}

	dsi->disp_pclk = dsi->video_mode.pixelclock / 1000UL;
	dsi_info->pclk = dsi->disp_pclk;

	dsi_info->data_rate = (dsi->disp_pclk) * 24U / dsi_info->phy_cfg.phy_lanes;
	dsi_info->data_rate /= 1000U;
	if((dsi_info->data_rate % 10U) != 0U) {
		(void)pr_err("[DSI]Request D-PHY Data rate not supported!\n Please set as multiple of 10mbps\n");
		dsi_info->data_rate /= 10U;
		dsi_info->data_rate += 1U;
		dsi_info->data_rate *= 10U;
	}
	if((dsi_info->data_rate > 2500U) || (dsi_info->data_rate < 100U)) {
		(void)pr_err("D-PHY Bandwidth out of range\n");
	}
	dev_info(dsi->dev, "data_rate %ld Mhz \n", dsi_info->data_rate);

	if (ret == 0) {
		dsi->dsi_pins.p = devm_pinctrl_get(dsi->dev);
		ret = panel_dsi_parse_pins(dsi);
	}

	return ret;
}

int dsi_proc_open(struct inode *pinode, struct file *filp)
{
	int ret = 0;

	(void)pinode;
	(void)filp;

	if(try_module_get(THIS_MODULE) == (bool)false) {
		/*For KCS*/
		ret = -ENODEV;
	}

	return ret;
}

int dsi_proc_close(struct inode *pinode, struct file *filp)
{
	(void)pinode;
	(void)filp;

	module_put(THIS_MODULE);

	return 0;
}

ssize_t dsi_ext_proc_read_str_status(struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set)
{
	ssize_t size = 0;
	struct panel_dsi *dsi;

	(void)cnt;
	(void)usr_buf;
	(void)off_set;

	dsi = (struct panel_dsi *)PDE_DATA(file_inode(filp));

	(void)pr_info("*** DSI Suspend Test\n");
	(void)panel_dsi_disable(&dsi->panel);
	(void)pr_info("*** DSI Suspend Done\n waiting 5s...");
	mdelay(5000);

	(void)pr_info("*** DSI Resume Test\n");
	(void)panel_dsi_enable(&dsi->panel);

	return size;
}


static int panel_dsi_init_proc(struct panel_dsi *dsi)
{
	int ret = 0;	
	dsi->dsi_proc_dir = proc_mkdir("tcc_dsi_v2", NULL);
	if(dsi->dsi_proc_dir == NULL) {
		/* KCS */
		dev_err(dsi->dev, "[ERROR][%s] Failed to create @ /proc/tcc_dsi_v2", __func__);
		ret = -1;
	}

	if(ret == 0) {
		dsi->dsi_proc_str = proc_create_data("str", ((umode_t)S_IFREG | (umode_t)292), dsi->dsi_proc_dir, &proc_fops_str_data, dsi);
		if(dsi->dsi_proc_str == NULL) {
			dev_err(dsi->dev, "[ERROR][%s] Failed to create @ /proc/tcc_dsi_v2/str", __func__);
			ret = -1;
		}
	}

	return ret;
}

static int panel_dsi_probe(struct platform_device *pdev)
{
	struct panel_dsi *dsi;
	int ret = 0;

	dsi = devm_kzalloc(&pdev->dev, sizeof(*dsi), GFP_KERNEL);

	if (dsi != NULL) {
		ret = 0;
	} else {
		(void)pr_err("[ERROR][%s] %s failed to alloc device context\n",
			LOG_TAG, __func__);
		ret = -ENODEV;
	}

	if (ret == 0) {
		dsi->dev = &pdev->dev;
		dsi->data = (const struct dsi_match_data *)
				of_device_get_match_data(&pdev->dev);
		if (dsi->data == NULL) {
			dev_err(dsi->dev,
				"[ERROR][%s] %s failed to find match_data from device tree\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
			devm_kfree(&pdev->dev, dsi);
		}
	}

	if (ret == 0) {
		ret = panel_dsi_parse_dt(pdev, dsi);
		if (ret < 0) {
			dev_err(dsi->dev,
				"[ERROR][%s] %s failed to parse device tree\n",
				LOG_TAG, __func__);
			ret = -ENODEV;
			devm_kfree(&pdev->dev, dsi);
		}
	}

	if (ret == 0) {
		/* Register the panel. */
		fb_panel_init(&dsi->panel);
		dsi->panel.dev = dsi->dev;
		dsi->panel.funcs = &panel_dsi_funcs;

		ret = fb_panel_add(&dsi->panel);
		if (ret < 0) {
			dev_err(dsi->dev,
				"[ERROR][%s] %s with [%s] failed to fb_panel_init\n",
				LOG_TAG, __func__, dsi->data->name);
			/* put dev */
			#if defined(CONFIG_FB_TCC_CTRL_BACKLIGHT)
			put_device(&dsi->backlight->dev);
			#endif
			devm_kfree(&pdev->dev, dsi);
		}
	}

	if (ret == 0) {
		dev_set_drvdata(dsi->dev, dsi);

		if(mipi_dsih_main_get_pwr(&dsi->dsi_info) != 0U) {
			dsi->enabled = 1;
		} else {
			dsi->enabled = 0;
		}

		// Proc 
		(void)panel_dsi_init_proc(dsi);
		/* Version */
		dsi->major = DRIVER_MAJOR;
		dsi->minor = DRIVER_MINOR;
		dsi->patchlevel = DRIVER_PATCH;
		dsi->date =  kstrdup(DRIVER_DATE, GFP_KERNEL);

		dev_err(dsi->dev, "[INFO][%s] %s with [%s], enabled : %d\n",
			 LOG_TAG, __func__, dsi->data->name, dsi->enabled);
	}

	return ret;
}

static int panel_dsi_remove(struct platform_device *pdev)
{
	struct panel_dsi *dsi =
		(struct panel_dsi*)dev_get_drvdata(&pdev->dev);

	devm_pinctrl_put(dsi->dsi_pins.p);

	fb_panel_remove(&dsi->panel);

	(void)panel_dsi_disable(&dsi->panel);

	#if defined(CONFIG_FB_TCC_CTRL_BACKLIGHT)
	if (dsi->backlight != NULL) {
		put_device(&dsi->backlight->dev);
	}
	#endif

	devm_kfree(&pdev->dev, dsi);

	return 0;
}

static const struct of_device_id panel_dsi_of_table[] = {
	{ .compatible = "telechips,fb-dsi-panel",
	  .data = &dsi_panel,
	},
	{ /* Sentinel */ },
};

MODULE_DEVICE_TABLE(of, panel_dsi_of_table);

#ifdef CONFIG_PM
static int panel_dsi_suspend(struct device *dev)
{
	const struct panel_dsi *dsi = (struct panel_dsi*)dev_get_drvdata(dev);

	dev_dbg(dsi->dev, "[DEBUG][%s] %s\n", LOG_TAG, __func__);

	return 0;
}

static int panel_dsi_resume(struct device *dev)
{
	const struct panel_dsi *dsi = (struct panel_dsi*)dev_get_drvdata(dev);

	dev_dbg(dev, "[DEBUG][%s] %s\n", LOG_TAG, __func__);

	/* Set pin status to defualt */
	if (
		panel_tcc_pin_select_state(
			dsi->dsi_pins.p,
			dsi->dsi_pins.default0) < 0) {
		dev_warn(dsi->dev,
			"[WARN][%s:%s] %s failed set pinctrl to default0\r\n",
			LOG_TAG, dsi->data->name, __func__);
	}
	if (
		panel_tcc_pin_select_state(
			dsi->dsi_pins.p,
			dsi->dsi_pins.pwr_off) < 0) {
		dev_warn(dsi->dev,
			"[WARN][%s:%s] %s failed set pinctrl to pwr_off\r\n",
			LOG_TAG, dsi->data->name, __func__);
	}
	if (
		panel_tcc_pin_select_state(
			dsi->dsi_pins.p,
			dsi->dsi_pins.blk_off) < 0) {
		dev_warn(dsi->dev,
			"[WARN][%s:%s] %s failed set pinctrl to blk_off\r\n",
			LOG_TAG, dsi->data->name, __func__);
	}

	return 0;
}
static const struct dev_pm_ops panel_dsi_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(panel_dsi_suspend, panel_dsi_resume)
};
#endif

static struct platform_driver fb_panel_dsi_driver = {
	.probe		= panel_dsi_probe,
	.remove		= panel_dsi_remove,
	.driver		= {
		.name	= "fb-panel-dsi",
		#ifdef CONFIG_PM
		.pm	= &panel_dsi_pm_ops,
		#endif
		.of_match_table = panel_dsi_of_table,
	},
};

module_platform_driver(fb_panel_dsi_driver);

MODULE_DESCRIPTION("Telechips DSI Panel Driver");
MODULE_LICENSE("GPL");
