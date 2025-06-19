// SPDX-License-Identifier: GPL-2.0-or-later

/* telechips_drm_vioc.c
 *
 * Copyright (c) 2022 Telechips Inc.
 * Authors:
 *	Jayden Kim
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#if defined(CONFIG_REFCODE_PRE_K510)
#include <drm/drmP.h>
#endif
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/component.h>
#include <linux/regmap.h>
#include <linux/tcc_math.h>
#include <linux/videodev2.h>
#include <linux/ktime.h>
#include <video/of_display_timing.h>
#include <video/of_videomode.h>


#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic_state_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_vblank.h>
#include <drm/drm_print.h>
#include <drm/drm_panel.h>
#include <drm/telechips_drm.h>

#include <video/telechips/vioc_global.h>
#include <video/telechips/vioc_rdma.h>
#include <video/telechips/vioc_wmix.h>
#include <video/telechips/vioc_disp.h>
#include <video/telechips/tcc_types.h>
#include <video/telechips/vioc_intr.h>
#include <video/telechips/vioc_ddicfg.h>
#include <video/telechips/vioc_config.h>
#include <video/telechips/vioc_outcfg.h>

#include <telechips_drm_types.h>
#include <telechips_drm_drv.h>
#include <telechips_drm_fb.h>
#include <telechips_drm_edid.h>
#include <telechips_drm_vioc.h>
#include <telechips_drm_crtc_plane_helper.h>

#include <soc/telechips/timer_api.h>

#define TCCDRM_RDMA_MAX_NUM 4

#define DRIVER_DATE	"20240227"
#define DRIVER_MAJOR	2
#define DRIVER_MINOR	4
#define DRIVER_PATCH	2

/* DEFINES for PLANE ---------------------------------------------------------*/
#define to_tcc_plane_state(plane_state) \
	container_of((plane_state), struct tcc_drm_plane_state, base)

#define to_tccdrm_vioc_context(x) \
	container_of(x, struct tccdrm_vioc_context, crtc)


#if defined(CONFIG_DRM_TELECHIPS_CTRL_CHROMAKEY)
struct lcd_chromakeys {
	unsigned int chromakey_enable;
	struct drm_chromakey_t value;
	struct drm_chromakey_t mask;
};
#endif

struct vioc_fmt_parmas {
	struct vioc_fmt_t vioc_fmt;
	unsigned int width;
	unsigned int height;
};

struct tccdrm_vioc_context {
	struct device *dev;
	struct drm_device *drm;

	struct drm_crtc crtc;
	struct tcc_drm_plane planes[CRTC_WIN_NR_MAX];
	unsigned long crtc_flags;

	struct tccdrm_flip_state flip_state;
	/* support to fence */
	//atomic_t flip_status;
	//struct drm_pending_vblank_event *flip_event;
	//bool flip_state;

	/* Whether the CRTC enabled - false disabled, true enabled */
	bool crtc_enabled;

	/* Whether the dev binded - false not binded, true binded */
	bool dev_binded;

	/* Display Disable Done */
	wait_queue_head_t wait_display_done_queue;
	atomic_t wait_display_done_event;

	spinlock_t irq_lock;

	/*
	 * keep_logo
	 * If this flag is set to 1, DRM driver skips process that fills its
	 * plane with block colorwhen it is binded. In other words, If so any
	 * Logo image was displayed on screen at booting time, it will is keep
	 * dislayed on screen until the DRM application is executed.
	 */
	int keep_logo;

	#if defined(CONFIG_DRM_TELECHIPS_CTRL_CHROMAKEY)
	struct mutex chromakey_mutex;
	struct lcd_chromakeys chromakeys[3];
	#endif

	/* count for fifo-underrun */
	bool fifo_underrun;
	ktime_t time_underrun_start;

	/* H/W data */
	struct tcc_hw_device hw_data;
	#if defined(CONFIG_SMP)
	const struct cpumask *irq_cpumask;
	#endif

	/* vtimer - used in limited plane only mode */
	unsigned int vrefresh;
	struct tcc_timer *vtimer;
};

#if defined(CONFIG_VIOC_PVRIC_FBDC)
static const uint64_t tcc_povervr_rougue_plane_format_modifiers[3] = {
	DRM_FORMAT_MOD_PVR_FBCDC_8x8_V10,
	DRM_FORMAT_MOD_LINEAR,
	DRM_FORMAT_MOD_INVALID
};
#endif

/* FUNCTIONS for DEVICE-TREE--------------------------------------------------*/
static int
parse_vioc_address_of_display_controller(const struct platform_device *plat_dev,
				     struct tcc_hw_device *hw_data,
				     struct device_node *current_node)
{
	const struct device *dev = &plat_dev->dev;
	bool internal_ok = (bool)true;
	uint32_t vioc_index;
	int ret = 0;

	/* Set Default connector type */
	ret = of_property_read_u32_index(
		dev->of_node, "display_device", 1,
		&hw_data->display_device.blk_num);
	if (ret < 0) {
		internal_ok = (bool)false;
		ret = -ENODEV;
	}

	if (internal_ok) {
		hw_data->display_device.virt_addr =
			(void __iomem*)VIOC_DISP_GetAddress(hw_data->display_device.blk_num);

		vioc_index = get_vioc_index(hw_data->display_device.blk_num);
		hw_data->display_device.irq_num =
			irq_of_parse_and_map(current_node, (int)vioc_index);
		DRM_DEV_DEBUG(dev,
			      "[DEBUG] display device id is %d, irq_num = %d\r\n",
			      vioc_index,
			      hw_data->display_device.irq_num);
	}
	return ret;
}

static int parse_vioc_clocks_of_display_controller(const struct platform_device *plat_dev,
						   struct tcc_hw_device *hw_data,
						   struct device_node *current_node)
{
	const struct device *dev = &plat_dev->dev;
	bool internal_ok = (bool)true;
	int ret = 0;

	hw_data->vioc_clock =
		of_clk_get_by_name(current_node, "ddi-clk");
	/* coverity[misra_c_2012_rule_11_2] */
	if (IS_ERR(hw_data->vioc_clock)) {
		DRM_DEV_ERROR(dev, "could not find bus clk.\r\n");
		internal_ok = (bool)false;
		/* coverity[misra_c_2012_rule_10_3] */
		/* coverity[misra_c_2012_rule_11_2] */
		ret = PTR_ERR(hw_data->vioc_clock);
	}

	if (internal_ok) {
		switch (hw_data->display_device.blk_num) {
		case VIOC_DISP0:
			hw_data->ddc_clock =
				of_clk_get_by_name(current_node, "disp0-clk");
			hw_data->display_device.intr_num = VIOC_INTR_DEV0;
			break;
		case VIOC_DISP1:
			hw_data->ddc_clock =
				of_clk_get_by_name(current_node, "disp1-clk");
			hw_data->display_device.intr_num = VIOC_INTR_DEV1;
			break;
		#if defined(VIOC_DISP2)
		case VIOC_DISP2:
			hw_data->ddc_clock =
				of_clk_get_by_name(current_node, "disp2-clk");
			hw_data->display_device.intr_num = VIOC_INTR_DEV2;
			break;
		#endif
		#if defined(VIOC_DISP3)
		case VIOC_DISP3:
			hw_data->ddc_clock =
				of_clk_get_by_name(current_node, "disp3-clk");
			hw_data->display_device.intr_num = VIOC_INTR_DEV3;
			break;
		#endif
		#if defined(VIOC_DISP4)
		case VIOC_DISP4:
			hw_data->ddc_clock =
				of_clk_get_by_name(current_node, "disp4-clk");
			hw_data->display_device.intr_num = VIOC_INTR_DEV4;
			break;
		#endif
		default:
			/* coverity[misra_c_2012_rule_11_2] */
			hw_data->ddc_clock = ERR_PTR(-ENODEV);
			break;
		}
		/* coverity[misra_c_2012_rule_11_2] */
		if (IS_ERR(hw_data->ddc_clock)) {
			DRM_DEV_ERROR(dev, "could not find peri clk.\r\n");
			/* coverity[misra_c_2012_rule_10_3] */
			/* coverity[misra_c_2012_rule_11_2] */
			ret = PTR_ERR(hw_data->ddc_clock);
		}
	}

	return ret;
}

static int parse_vioc_display_controller(const struct platform_device *plat_dev,
					   struct tcc_hw_device *hw_data,
					   struct device_node *current_node)
{
	bool internal_ok = (bool)true;
	int ret = 0;

	ret = parse_vioc_address_of_display_controller(plat_dev, hw_data, current_node);
	if (ret < 0) {
		internal_ok = (bool)false;
		ret = -ENODEV;
	}
	if (internal_ok) {
		ret = parse_vioc_clocks_of_display_controller(plat_dev, hw_data, current_node);
	}
	return ret;
}

static int parse_vioc_wmix(const struct platform_device *plat_dev,
			   struct tcc_hw_device *hw_data)
{
	const struct device *dev = &plat_dev->dev;
	bool internal_ok = (bool)true;
	int ret = 0;

	ret = of_property_read_u32_index(dev->of_node, "wmixer", 1,
					 &hw_data->wmixer.blk_num);
	if (ret < 0) {
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		hw_data->wmixer.virt_addr =
			(void __iomem*)VIOC_WMIX_GetAddress(hw_data->wmixer.blk_num);
	}
	return ret;
}

#if defined(CONFIG_VIOC_PVRIC_FBDC)
static int parse_vioc_pvric_fbdc(const struct platform_device *plat_dev,
				 struct tcc_hw_device *hw_data)
{
	const struct device *dev = &plat_dev->dev;
	unsigned int u, vioc_index;
	int ret = 0;

	for (u = 0U; u < (uint32_t)TCCDRM_RDMA_MAX_NUM; u++) {
		if (of_property_read_u32_index(
			dev->of_node, "fbdc", u + 1U,
			&hw_data->fbdc[u].blk_num) != 0) {
			hw_data->fbdc[u].virt_addr = NULL;
		} else {
			if ((hw_data->fbdc[u].blk_num != VIOC_FBCDEC0) &&
				(hw_data->fbdc[u].blk_num != VIOC_FBCDEC1)) {
				hw_data->fbdc[u].virt_addr =
					NULL;
				continue;
			}
			hw_data->fbdc[u].virt_addr =
				(void __iomem*)VIOC_PVRIC_FBDC_GetAddress(
					hw_data->fbdc[u].blk_num);
			vioc_index = get_vioc_index(hw_data->fbdc[u].blk_num);
			DRM_DEV_INFO(dev,
					"[INFO] plane[%u] uses fbdc[%d]\r\n",
					u, vioc_index);
		}
	}
	return ret;
}
#endif

static int parse_vioc_rdmas(const struct platform_device *plat_dev,
				 struct tcc_hw_device *hw_data,
				 struct device_node *current_node)
{
	const struct device *dev = &plat_dev->dev;
	unsigned int u, vioc_index;
	int ret = 0;

	/* parse RDMA */
	for (u = 0U; u < (uint32_t)TCCDRM_RDMA_MAX_NUM; u++) {
		if (of_property_read_u32_index(dev->of_node,
						"rdma", u + 1U,
						&hw_data->rdma[u].blk_num) != 0) {
			hw_data->rdma[u].virt_addr = NULL;
		} else {
			hw_data->rdma[u].virt_addr =
				(void __iomem*)VIOC_RDMA_GetAddress(hw_data->rdma[u].blk_num);
			vioc_index = get_vioc_index(hw_data->rdma[u].blk_num);
			hw_data->rdma[u].irq_num =
				irq_of_parse_and_map(current_node,
						     (int)vioc_index);
		}
	}

	return ret;
}

static int parse_vioc_componets(const struct platform_device *plat_dev,
				       struct tcc_hw_device *hw_data)
{
	const struct device *dev = &plat_dev->dev;
	struct device_node *current_node;
	bool internal_ok = (bool)true;
	int ret = 0;

	current_node = of_parse_phandle(dev->of_node, "display_device", 0);
	if (current_node == NULL) {
		DRM_DEV_ERROR(
			dev,
			"could not find display_device node\n");
		internal_ok = (bool)false;
		ret = -ENODEV;
	}
	if (internal_ok) {
		ret = parse_vioc_display_controller((const struct platform_device *)plat_dev,
						    hw_data,
						    current_node);
		if (ret < 0) {
			internal_ok = (bool)false;
		}
		of_node_put(current_node);
	}
	if (internal_ok) {
		/* parse wmixer */
		current_node = of_parse_phandle(dev->of_node, "wmixer", 0);
		if (current_node == NULL) {
			DRM_DEV_ERROR(dev, "could not find wmixer node\n");
			internal_ok = (bool)false;
			ret = -ENODEV;
		}
	}
	if (internal_ok) {
		ret = parse_vioc_wmix((const struct platform_device *)plat_dev,
					    hw_data);
		if (ret < 0) {
			internal_ok = (bool)false;
		}
		of_node_put(current_node);
	}
	#if defined(CONFIG_VIOC_PVRIC_FBDC)
	if (internal_ok) {
		current_node = of_parse_phandle(dev->of_node, "fbdc", 0);
		if (current_node != NULL) {
			ret = parse_vioc_pvric_fbdc((const struct platform_device *)plat_dev,
						    hw_data);
			if (ret < 0) {
				internal_ok = (bool)false;
			}
			of_node_put(current_node);
		}
	}
	#endif
	/* parse RDMA */
	if (internal_ok) {
		current_node = of_parse_phandle(dev->of_node, "rdma", 0);
		if (current_node == NULL) {
			DRM_DEV_ERROR(dev, "could not find rdma node\n");
			internal_ok = (bool)false;
			ret = -ENODEV;
		}
	}
	if (internal_ok) {
		ret = parse_vioc_rdmas((const struct platform_device *)plat_dev,
					    hw_data, current_node);
		of_node_put(current_node);
	}
	return ret;
}

static int parse_vioc_planes(const struct platform_device *plat_dev,
				       struct tcc_hw_device *hw_data)
{
	const struct device *dev = &plat_dev->dev;
	bool internal_ok = (bool)true;
	int ret = 0;

	unsigned int u;

	int planles, read_planles;

	const char *planes_type[TCCDRM_RDMA_MAX_NUM] = {
		NULL, NULL, NULL, NULL
	};

	/* parse planes */
	planles = of_property_count_strings(dev->of_node, "planes");;
	if (planles > 0) {
		read_planles = of_property_read_string_array(dev->of_node,
							     "planes",
							     planes_type,
							     (size_t)planles);
		if (read_planles != planles) {
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	} else {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}

	if (internal_ok) {
		for (u = 0U; u < (uint32_t)planles;u++) {
			if (planes_type[u] == NULL) {
				continue;
			}
			if (strcmp(planes_type[u], "primary") == 0) {
				hw_data->rdma_plane_type[u] =
					(uint32_t)DRM_PLANE_TYPE_PRIMARY;
			} else if (strcmp(planes_type[u],
					  "primary_transparent") == 0) {
				hw_data->rdma_plane_type[u] =
					((uint32_t)DRM_PLANE_TYPE_PRIMARY) |
					((uint32_t)DRM_PLANE_FLAG_TRANSPARENT);
			} else if (strcmp(planes_type[u], "cursor") == 0) {
				hw_data->rdma_plane_type[u] =
					(uint32_t)DRM_PLANE_TYPE_CURSOR;
			} else if (strcmp(planes_type[u], "overlay") == 0) {
				hw_data->rdma_plane_type[u] =
					(uint32_t)DRM_PLANE_TYPE_OVERLAY;
			} else if (strcmp(planes_type[u],
					  "overlay_skip_yuv") == 0) {
				hw_data->rdma_plane_type[u] =
					(uint32_t)(DRM_PLANE_TYPE_OVERLAY) |
						DRM_PLANE_FLAG_SKIP_YUV_FORMAT;
			} else {
				hw_data->rdma_plane_type[u] =
					(uint32_t)DRM_PLANE_FLAG_NOT_DEFINED;
			}
		}

		/* mapping planes and rdma */
		hw_data->rdma_counts = 0;
		for (u = 0U; u < (uint32_t)TCCDRM_RDMA_MAX_NUM; u++) {
			if ((hw_data->rdma[u].virt_addr == NULL) ||
			    (DRM_PLANE_FLAG(hw_data->rdma_plane_type[u]) ==
			     DRM_PLANE_FLAG(DRM_PLANE_FLAG_NOT_DEFINED))) {
				hw_data->rdma[u].virt_addr = NULL;
				break;
			}
			hw_data->rdma_counts++;
		}

		for (; u < (uint32_t)TCCDRM_RDMA_MAX_NUM; u++) {
			hw_data->rdma[u].virt_addr = NULL;
			hw_data->rdma_plane_type[u]  =
				(uint32_t)DRM_PLANE_FLAG_NOT_DEFINED;
		}

		DRM_DEV_INFO(dev,
			     "[INFO] display_device:%d, wmixer:%d planes:%d\r\n",
			     get_vioc_index(hw_data->display_device.blk_num),
			     get_vioc_index(hw_data->wmixer.blk_num),
			     hw_data->rdma_counts);

		for (u = 0U; u < (uint32_t)TCCDRM_RDMA_MAX_NUM; u++) {
			if (hw_data->rdma[u].virt_addr != NULL) {
				DRM_DEV_INFO(dev,
					     "              rdma:%d - %s\r\n",
					     get_vioc_index(hw_data->rdma[u].blk_num),
					     (DRM_PLANE_TYPE(hw_data->rdma_plane_type[u]) ==
					     (uint32_t)DRM_PLANE_TYPE_CURSOR) ? "cursor" :
					     (DRM_PLANE_TYPE(hw_data->rdma_plane_type[u]) ==
					     (uint32_t)DRM_PLANE_TYPE_PRIMARY) ? "primary" :
					     "overlay");
			}
		}
	}
	return ret;
}

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC807X)
static int parse_vioc_display_mux(const struct platform_device *plat_dev,
				 struct tcc_hw_device *hw_data)
{
	const struct device *dev = &plat_dev->dev;
	bool internal_ok = (bool)true;
	int ret = 0;

	if (hw_data->limited_plane_only_mode) {
		/* plane only */
		DRM_DEV_INFO(dev, "[INFO] skip because this is plane only mode\r\n");
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		if (of_property_read_u32(
			dev->of_node, "lcdc-mux-select",
			&hw_data->lcdc_mux_select) == 0) {
			DRM_DEV_INFO(
				dev,
				"[DEBUG] lcdc-mux-select=%u\r\n",
				hw_data->lcdc_mux_select);
		} else {
			DRM_DEV_ERROR(dev,
					"can not found lcdc-mux-select\r\n");
			internal_ok =(bool)false;
			ret = -ENODEV;
		}
	}
	if (internal_ok) {
		if (of_property_read_u32(
			dev->of_node, "lcdc-mux-bypass",
			&hw_data->lcdc_mux_bypass) == 0) {
			DRM_DEV_INFO(
				dev,
				"[DEBUG] lcdc-mux-bypass=%u\r\n",
				hw_data->lcdc_mux_bypass);
		}
	}
	return ret;
}
#else
#define parse_vioc_display_mux(plat_dev, hw_data) (0)
#endif

static int tccdrm_vioc_dt_parse(
	/* coverity[misra_c_2012_rule_8_13] */
	struct platform_device *plat_dev, struct tcc_hw_device *hw_data)
{
	unsigned int limited_plane_only_mode;
	bool internal_ok = (bool)true;
	int ret = 0;

	if(of_property_read_u32(plat_dev->dev.of_node, "limited_plane_only_mode",
				 &limited_plane_only_mode) < 0) {
		/* No limitation */
		limited_plane_only_mode = 0U;
	}
	if (limited_plane_only_mode > 0U) {
		hw_data->limited_plane_only_mode = (bool)true;
	}
	ret = parse_vioc_componets((const struct platform_device *)plat_dev,
						hw_data);
	if (ret < 0) {
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		ret = parse_vioc_planes((const struct platform_device *)plat_dev,
					   hw_data);
		if (ret < 0) {
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		ret = parse_vioc_display_mux((const struct platform_device *)plat_dev,
					     hw_data);
	}

	return ret;
}

/* FUNCTIONS for PLANE -------------------------------------------------------*/
/* coverity[HIS_metric] - HIS_CCM */
static int tccdrm_plane_mode_set(struct tcc_drm_plane_state *tcc_pstate,
				 const u16 hdisplay, const u16 vdisplay)
{
	const struct drm_plane_state *drm_pstate =
		(const struct drm_plane_state *)&tcc_pstate->base;
	const struct drm_plane *plane = (const struct drm_plane *)drm_pstate->plane;
	const struct device *dev = NULL;
	unsigned int src_x, src_y, src_w, src_h;
	unsigned int crtc_w, crtc_h, utmp;
	int crtc_x, crtc_y, itmp;
	bool internal_ok = (bool)true;
	int ret = 0;


	if ((plane != NULL) && (plane->dev != NULL) && (plane->dev->dev != NULL)) {
		dev = plane->dev->dev;
	}

	/*
	 * The original src/dest coordinates are stored in tcc_pstate->base,
	 * but we want to keep another copy internal to our driver that we can
	 * clip/modify ourselves.
	 */
	crtc_x = drm_pstate->crtc_x;
	crtc_y = drm_pstate->crtc_y;
	crtc_w = drm_pstate->crtc_w;
	crtc_h = drm_pstate->crtc_h;

	/* Source parameters given in 16.16 fixed point, ignore fractional. */
	src_x = drm_pstate->src_x >> 16;
	src_y = drm_pstate->src_y >> 16;
	src_w = drm_pstate->src_w >> 16;
	src_h = drm_pstate->src_h >> 16;

	if (tcc_math_uint_gt_intmax(crtc_w)) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}

	if (internal_ok) {
		if (tcc_math_uint_gt_intmax(crtc_h)) {
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}

	if (internal_ok) {
		if (crtc_x < 0) {
			/* src_x += -crtc_x; */
			itmp = -crtc_x;
			utmp = (unsigned int)itmp;
			src_x += utmp;

			/* crtc_w += crtc_x; */
			if (crtc_w < utmp) {
				internal_ok = (bool)false;
				ret = -EINVAL;
			} else {
				crtc_w -= utmp;
				crtc_x = 0;
			}
		}
	}
	if (internal_ok) {
		if (crtc_y < 0) {
			/* src_y += -crtc_y; */
			itmp = -crtc_y;
			utmp = (unsigned int)itmp;
			src_y += utmp;

			/* crtc_h += crtc_y; */
			if (crtc_h < utmp) {
				internal_ok = (bool)false;
				ret = -EINVAL;
			} else {
				crtc_h -= utmp;
				crtc_y = 0;
			}
		}
	}

	/* set drm framebuffer data. */
	if (internal_ok) {
		tcc_pstate->src.x = src_x;
		tcc_pstate->src.y = src_y;
		tcc_pstate->src.w = src_w;
		tcc_pstate->src.h = src_h;
	}

	/* set plane range to be displayed. */
	if (internal_ok) {
		/* tcc_pstate->crtc.x = crtc_x; */
		utmp = (unsigned int)crtc_x;
		tcc_pstate->crtc.x = utmp;

		/* tcc_pstate->crtc.y = crtc_y; */
		utmp = (unsigned int)crtc_y;
		tcc_pstate->crtc.y = utmp;

		tcc_pstate->crtc.w = crtc_w;
		tcc_pstate->crtc.h = crtc_h;
	}

	if (internal_ok) {
		utmp = tcc_pstate->crtc.x + tcc_pstate->crtc.w;
		if (utmp > hdisplay) {
			crtc_w = utmp - hdisplay;
			if (tcc_pstate->crtc.w <= crtc_w) {
				DRM_DEV_ERROR(dev,
					"This has been calibrated to tcc_pstate->crtc.w(%u), crtc_w(%u)\r\n",
					tcc_pstate->crtc.w, crtc_w);
				internal_ok = (bool)false;
				ret = -EINVAL;
			} else {
				tcc_pstate->crtc.w -= crtc_w;
				DRM_DEV_INFO(dev,
					"This has been calibrated to vdisplay hdisplay(%u), crtc_x(%u) crtc_w(%u)\r\n",
					hdisplay, tcc_pstate->crtc.x,
					tcc_pstate->crtc.w);
			}
		}
	}
	if (internal_ok) {
		utmp = tcc_pstate->crtc.y + tcc_pstate->crtc.h;
		if (utmp > vdisplay) {
			crtc_h = utmp - vdisplay;
			if (tcc_pstate->crtc.h <= crtc_h) {
				DRM_DEV_ERROR(dev,
					"This has been calibrated to tcc_pstate->crtc.h(%u), crtc_h(%u)\r\n",
					tcc_pstate->crtc.h, crtc_h);
				ret = -EINVAL;
			} else {
				tcc_pstate->crtc.h -= crtc_h;
				DRM_DEV_INFO(dev,
					"This has been calibrated to vdisplay(%u), crtc_y(%u) crtc_h(%u)\r\n",
					vdisplay, tcc_pstate->crtc.y,
					tcc_pstate->crtc.h);
			}
		}
	}

	return ret;
}

static const struct drm_plane_funcs tccdrm_vioc_plane_funcs = {
	.update_plane	= drm_atomic_helper_update_plane,
	.disable_plane	= drm_atomic_helper_disable_plane,
	.destroy	= drm_plane_cleanup,
	.reset		= tccdrm_plane_reset,
	.atomic_duplicate_state = tccdrm_plane_duplicate_state,
	.atomic_destroy_state = tccdrm_plane_destory_state,
};

/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 2>
 * HIS metric violation (HIS_CCM)
 *  DR <case 3>
 */
/* coverity[HIS_metric] - HIS_CCM */
/* coverity[misra_c_2012_rule_8_13] */
static int tccdrm_vioc_plane_check(struct drm_plane *plane,
				   struct drm_plane_state *drm_pstate)
{
	struct tcc_drm_plane_state *tcc_pstate;
	const struct drm_crtc_state *drm_cstate;

	#if defined(CONFIG_VIOC_PVRIC_FBDC)
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tcc_drm_plane *tcc_plane = to_tcc_plane(plane);
	#endif
	bool internal_ok = (bool)true;
	int ret = 0;

	if (drm_pstate == NULL) {
		internal_ok = (bool)false;
		ret = -EINVAL;
		DRM_DEV_INFO(plane->dev->dev,
			     "[WARN] drm_pstate is NULL with err(%d)\r\n", ret);
	}

	if (internal_ok) {
		tcc_pstate =
			/* coverity[cert_arr39_c] */
			/* coverity[cert_dcl37_c] */
			/* coverity[misra_c_2012_rule_8_5] */	//
			/* coverity[misra_c_2012_rule_8_6] */	//
			/* coverity[misra_c_2012_rule_8_13] */
			/* coverity[misra_c_2012_rule_10_1] */
			/* coverity[misra_c_2012_rule_11_5] */
			/* coverity[misra_c_2012_rule_14_4] */
			/* coverity[misra_c_2012_rule_15_6] */
			/* coverity[misra_c_2012_rule_18_4] */
			/* coverity[misra_c_2012_rule_20_7] */
			/* coverity[misra_c_2012_rule_21_2] */
			(struct tcc_drm_plane_state *)to_tcc_plane_state(drm_pstate);
		if (drm_pstate->state == NULL) {
			DRM_DEV_INFO(plane->dev->dev,
				     "[WARN]drm_pstate->state is NULL with err(%d)\r\n",
				     ret);
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}

	if (internal_ok &&
		((drm_pstate->crtc == NULL) || (drm_pstate->fb == NULL))) {
		/*
		 * There is no need for further checks if the plane is
		 * being disabled
		 */
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		drm_cstate =
			drm_atomic_get_existing_crtc_state(drm_pstate->state,
							   drm_pstate->crtc);
		if (drm_cstate == NULL) {
			DRM_DEV_INFO(plane->dev->dev,
				     "[WARN] drm_cstate is NULL with err(%d)\r\n",
				     ret);
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}

	if (internal_ok) {
		switch (drm_pstate->fb->modifier) {
		#if defined(CONFIG_VIOC_PVRIC_FBDC)
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_10_4] */
		/* coverity[misra_c_2012_rule_20_7] */
		case DRM_FORMAT_MOD_PVR_FBCDC_8x8_V10:
			if (!test_bit(
				TCC_DRM_PLANE_CAP_FBDC,
				&tcc_plane->caps)) {
				DRM_DEV_INFO(plane->dev->dev,
					     "[WARN] crtc[%d] plane[%d] is not support PVR_FBCDC_8x8_V10\r\n",
					     drm_pstate->crtc->base.id,
					     tcc_plane->win);
				internal_ok = (bool)false;
				ret = -EINVAL;
			}
			if (internal_ok) {
				/* DP no.2 */
				/* coverity[misra_c_2012_rule_10_3] */
				/* coverity[misra_c_2012_rule_14_4] */
				/* coverity[misra_c_2012_rule_15_6] */
				/* coverity[cert_dcl37_c] */
				dev_dbg(plane->dev->dev,
					"[INFO] crtc[%d] plane[%d] is support PVR_FBCDC_8x8_V10\r\n",
					drm_pstate->crtc->base.id, tcc_plane->win);
			}
			break;
		#endif
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_10_4] */
		/* coverity[misra_c_2012_rule_10_7] */
		/* coverity[misra_c_2012_rule_20_7] */
		case DRM_FORMAT_MOD_LINEAR:
			//DRM_DEV_INFO(plane->dev->dev,
			//	     "[INFO] plane[%d] is support DRM_FORMAT_MOD_LINEAR\r\n",
			//	     tcc_plane->win);
			break;
		default:
			DRM_DEV_ERROR(plane->dev->dev,
				      "[%llx] is not supported\r\n",
				      drm_pstate->fb->modifier);
			internal_ok = (bool)false;
			ret = -EINVAL;
			break;
		}
	}

	if (internal_ok &&
		((drm_pstate->crtc == NULL) || (drm_pstate->fb == NULL))) {
		DRM_DEV_ERROR(plane->dev->dev, "Error crtc or fb is NULL\r\n");
		internal_ok = (bool)false;
		ret = -EINVAL;
	}

	if (internal_ok) {
		if ((tcc_math_uint_gt_intmax(drm_pstate->crtc_w))) {
			DRM_DEV_ERROR(plane->dev->dev,
				      "drm_pstate->crtc_w is out of integer range\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}

	if (internal_ok) {
		if ((tcc_math_uint_gt_intmax(drm_pstate->crtc_h))) {
			DRM_DEV_ERROR(plane->dev->dev,
				      "drm_pstate->crtc_h is out of integer range\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_12_2] */
		ret = drm_atomic_helper_check_plane_state(drm_pstate, drm_cstate,
                                                   DRM_PLANE_HELPER_NO_SCALING,
                                                   DRM_PLANE_HELPER_NO_SCALING,
                                                   (bool)true, (bool)false);
		if (ret < 0) {
			DRM_DEV_ERROR(plane->dev->dev,
				      "drm_atomic_helper_check_plane_state\r\n");
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		/* translate drm_pstate into tcc_pstate */
		ret = tccdrm_plane_mode_set(tcc_pstate,
					    drm_cstate->adjusted_mode.hdisplay,
					    drm_cstate->adjusted_mode.vdisplay);
	}

	return ret;
}

static void lcd_set_video_output(const struct tccdrm_vioc_context *dev_context,
				 unsigned int win, bool en)
{
	bool internal_ok = (bool)true;

	if (dev_context->hw_data.rdma_counts <= 0) {
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		if (win >= (unsigned int)dev_context->hw_data.rdma_counts) {
			DRM_DEV_DEBUG(dev_context->dev,
				"[DEBUG] win(%d) is out of range (%d)\r\n",
				win, dev_context->hw_data.rdma_counts);
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		if (dev_context->hw_data.rdma[win].virt_addr == NULL) {
			DRM_DEV_INFO(dev_context->dev,
				     "[WARN] virtual address of win(%d) is NULL\r\n",
				     win);
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		if (en) {
			VIOC_RDMA_SetImageEnable(dev_context->hw_data.rdma[win].virt_addr);
		} else {
			if (test_bit(CRTC_FLAGS_PCLK_BIT, &dev_context->crtc_flags) == 1) {
				VIOC_RDMA_SetImageDisable(
					dev_context->hw_data.rdma[win].virt_addr);
			} else {
				unsigned int enabled;

				VIOC_RDMA_GetImageEnable(
					dev_context->hw_data.rdma[win].virt_addr, &enabled);
				if (enabled == 1U) {
					DRM_DEV_INFO(
						dev_context->dev,
						"[INFO] win(%d) Disable RDMA with NoWait\r\n",
						win);
					VIOC_RDMA_SetImageDisableNW(
						dev_context->hw_data.rdma[win].virt_addr);
				}
			}
		}
	}
}

#if defined(CONFIG_VIOC_PVRIC_FBDC)
/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 2>
 */
static int lcd_connect_pvric_fbdc(
	struct tccdrm_vioc_context *dev_context, unsigned int win, unsigned int base_addr,
	unsigned int fmt, unsigned int width, unsigned int height)
{
	void __iomem *fbdc_base;
	unsigned int fbdc_id, rdma_id;
	unsigned int plugged_rdma;
	bool internal_ok = (bool)true;

	int ret = 0;

	fbdc_base = dev_context->hw_data.fbdc[win].virt_addr;
	fbdc_id = dev_context->hw_data.fbdc[win].blk_num;
	rdma_id = dev_context->hw_data.rdma[win].blk_num;

	if (
		test_bit(
			TCC_PLANE_FLAG_FBDC_PLUGGED,
			&dev_context->planes[win].plane_flags) == 1) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}

	if (internal_ok) {
		int plugged_dma = VIOC_CONFIG_GetFBDCPath(fbdc_id);
		if (plugged_dma < 0) {
			DRM_DEV_ERROR(dev_context->dev,
				"failed VIOC_CONFIG_GetFBDCPath\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		} else {
			plugged_rdma = (unsigned int)plugged_dma;
		}
	}

	if (internal_ok) {
		/* 0x1F means pvric_fbcd is not connected */
		if ((get_vioc_index(plugged_rdma) != 0x1FU) &&
		     (plugged_rdma != rdma_id)) {
			DRM_DEV_ERROR(dev_context->dev,
				"FBDC[%d] is already used by RDMA[%d]\r\n",
				get_vioc_index(fbdc_id),
				get_vioc_index(plugged_rdma));
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}

	if (internal_ok) {
		/* Disable RDMA */
		lcd_set_video_output((const struct tccdrm_vioc_context *)dev_context,
				     win, (bool)false);
		//lcd_disable_video_output((const struct tccdrm_vioc_context *)dev_context,
		//			 win);

		/* swreset fbdc and rdma */
		VIOC_CONFIG_SWReset(fbdc_id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(fbdc_id, VIOC_CONFIG_CLEAR);

		VIOC_CONFIG_SWReset(rdma_id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(rdma_id, VIOC_CONFIG_CLEAR);

		(void)VIOC_CONFIG_FBCDECPath(
			fbdc_id, dev_context->hw_data.rdma[win].blk_num, 1);
		(void)VIOC_PVRIC_FBDC_SetBasicConfiguration(
			dev_context->hw_data.fbdc[win].virt_addr,
			base_addr, fmt, width, height, 0);

		vioc_pvric_fbdc_set_val0_cr_ch0123(fbdc_base, 0, 0, 0, 0);
		vioc_pvric_fbdc_set_val1_cr_ch0123(fbdc_base, 0, 0, 0, 0);
		vioc_pvric_fbdc_set_cr_filter_enable(fbdc_base);

		set_bit(TCC_PLANE_FLAG_FBDC_PLUGGED,
			&dev_context->planes[win].plane_flags);
	}

	return ret;
}

static int lcd_disconnect_pvric_fbdc(
	struct tccdrm_vioc_context *dev_context, unsigned int win)
{
	unsigned int fbdc_id, rdma_id;
	bool internal_ok = (bool)true;
	int ret = 0;

	if (dev_context->hw_data.rdma_counts <= 0) {
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		if (win >= (unsigned int)dev_context->hw_data.rdma_counts) {
			DRM_DEV_DEBUG(dev_context->dev, "[DEBUG] win(%d) is out of range (%d)\r\n",
				win, dev_context->hw_data.rdma_counts);
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		if (test_bit(TCC_PLANE_FLAG_FBDC_PLUGGED,
			  &dev_context->planes[win].plane_flags) != 1) {
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {

		fbdc_id = dev_context->hw_data.fbdc[win].blk_num;
		rdma_id = dev_context->hw_data.rdma[win].blk_num;

		/* Disable RDMA */
		lcd_set_video_output((const struct tccdrm_vioc_context *)dev_context,
				     win, (bool)false);
		//lcd_disable_video_output((const struct tccdrm_vioc_context *)dev_context, win);

		/* Stop FBDC */
		VIOC_PVRIC_FBDC_TurnOFF(
			dev_context->hw_data.fbdc[win].virt_addr);
		VIOC_PVRIC_FBDC_SetUpdateInfo(
			dev_context->hw_data.fbdc[win].virt_addr, 1);
		(void)VIOC_CONFIG_FBCDECPath(fbdc_id, rdma_id, 0);

		clear_bit(TCC_PLANE_FLAG_FBDC_PLUGGED,
			  &dev_context->planes[win].plane_flags);
	}
	return ret;
}

static int lcd_check_pvric_fbdc(unsigned int fbdc_id)
{
	unsigned int fbdc_status, vioc_id;
	bool internal_ok = (bool)true;
	int fbdc_intr_id;
	int ret = 0;


	if ((fbdc_id != VIOC_FBCDEC0) &&
	    (fbdc_id != VIOC_FBCDEC1)) {
		DRM_DEV_ERROR(NULL, "invalid fbdc_id(%x)\r\n", fbdc_id);
		internal_ok = (bool)false;
		ret = -EINVAL;
	}

	if (internal_ok) {
		vioc_id = get_vioc_index(fbdc_id) +
			(unsigned int)VIOC_INTR_AFBCDEC0;
		fbdc_intr_id = (int)vioc_id;
	}

	if (internal_ok) {
		fbdc_status = vioc_intr_get_status(fbdc_intr_id);

		if ((fbdc_status & VIOC_PVRIC_FBDC_INT_MASK) != 0U) {
			(void)vioc_intr_clear(fbdc_intr_id,
					fbdc_status & VIOC_PVRIC_FBDC_INT_MASK);

			if ((fbdc_status & PVRICSTS_TILE_ERR_MASK) != 0U) {
				ret = -1;
				DRM_DEV_ERROR(NULL,
					      "FBDC[%d] PVRICSTS_TILE_ERR_MASK\r\n",
					      fbdc_id - VIOC_FBCDEC0);
			}
			if ((fbdc_status & PVRICSTS_ADDR_ERR_MASK) != 0U) {
				ret = -1;
				DRM_DEV_ERROR(NULL,
					      "FBDC[%d] PVRICSTS_ADDR_ERR_MASK\r\n",
					      fbdc_id -VIOC_FBCDEC0);
			}
			if ((fbdc_status & PVRICSTS_EOF_ERR_MASK) != 0U) {
				ret = -1;
				DRM_DEV_ERROR(NULL,
					      "FBDC[%d] PVRICSTS_EOF_ERR_MASK\r\n",
					      fbdc_id -VIOC_FBCDEC0);
			}
		}
	}
	return ret;
}

/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 2>
 */
static int lcd_set_pvric_fbdc(struct tccdrm_vioc_context *dev_context,
			      unsigned int win, unsigned int base_addr,
			      const struct drm_framebuffer *fb,
			      const struct vioc_fmt_parmas *fmt_params)
{
	void __iomem *fbdc_base;
	unsigned int fbdc_id;
	bool internal_ok = (bool)true;
	int ret = 0;

	fbdc_base = dev_context->hw_data.fbdc[win].virt_addr;
	if (fbdc_base == NULL) {
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_10_4] */
		/* coverity[misra_c_2012_rule_20_7] */
		if (fb->modifier == DRM_FORMAT_MOD_PVR_FBCDC_8x8_V10) {
			ret = -ENODEV;
			DRM_DEV_ERROR(dev_context->dev,
				      "TCC_PLANE_FLAG_MOD_FBDC bug virt_addr is NULL\r\n");
		}

		internal_ok = (bool)false;
	}

	if (internal_ok) {
		fbdc_id = dev_context->hw_data.fbdc[win].blk_num;

		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_10_4] */
		/* coverity[misra_c_2012_rule_20_7] */
		if (fb->modifier != DRM_FORMAT_MOD_PVR_FBCDC_8x8_V10) {
			(void)lcd_disconnect_pvric_fbdc(dev_context, win);
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}

	if (internal_ok) {
		if (lcd_check_pvric_fbdc(fbdc_id) < 0) {
			(void)lcd_disconnect_pvric_fbdc(dev_context, win);
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}

	if (internal_ok) {
		ret = lcd_connect_pvric_fbdc(dev_context, win, base_addr,
					     fmt_params->vioc_fmt.f_fmt,
					     fmt_params->width,
					     fmt_params->height);
		if (ret < 0) {
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}

	if (internal_ok) {
		VIOC_PVRIC_FBDC_SetRequestBase(fbdc_base, base_addr);
		VIOC_PVRIC_FBDC_SetOutBufBase(fbdc_base, base_addr);
		VIOC_PVRIC_FBDC_SetARGBSwizzMode(fbdc_base,
						 fmt_params->vioc_fmt.swizz);
		VIOC_PVRIC_FBDC_TurnOn(fbdc_base);
		VIOC_PVRIC_FBDC_SetUpdateInfo(fbdc_base, 1);
	}
	return ret;
}
#endif

static void tccdrm_vioc_plane_set_format(void __iomem *pRDMA,
			       const struct vioc_fmt_t *vioc_fmt, uint32_t width, uint32_t pitch)
{
	/* Default RGB SWAP */
	VIOC_RDMA_SetImageY2REnable(pRDMA, vioc_fmt->f_y2r);
	VIOC_RDMA_SetImageRGBSwapMode(pRDMA, vioc_fmt->f_swap);
	if (pitch != 0U) {
		VIOC_RDMA_SetImageOffset0Direct(pRDMA, pitch);
	} else {
		DRM_DEV_ERROR(NULL, "pitch value is zero\r\n");
		VIOC_RDMA_SetImageOffset(pRDMA, vioc_fmt->f_fmt, width);
	}
	VIOC_RDMA_SetImageFormat(pRDMA, vioc_fmt->f_fmt);
}

static int lcd_update_check_win(const struct tccdrm_vioc_context *dev_context,
				unsigned int win)
{
	bool internal_ok = (bool)true;
	int ret = 0;

	if (dev_context->hw_data.rdma_counts <= 0) {
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		if (win >= (unsigned int)dev_context->hw_data.rdma_counts) {
			DRM_DEV_ERROR(dev_context->dev,
				"win(%d) is out of range (%d)\r\n",
				win, dev_context->hw_data.rdma_counts);
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		if (dev_context->hw_data.rdma[win].virt_addr == NULL) {
			DRM_DEV_ERROR(dev_context->dev,
				"virtual address of win(%d) is NULL\r\n",
				win);
			ret = -EINVAL;
		}
	}
	return ret;
}

/*
 * HIS metric violation (HIS_CCM)
 *  DR <case 2>
 */
static unsigned int lcd_update_get_dma_address(const struct tccdrm_vioc_context *dev_context,
				      const struct tcc_drm_plane_state *plane_state,
				      unsigned int win)
{
	struct drm_framebuffer *fb = plane_state->base.fb;
	unsigned int cpp = fb->format->cpp[0];
	unsigned int pitch = fb->pitches[0];
	unsigned int utmp;

	bool internal_ok = (bool)true;
	unsigned int dma_addr32 = 0U;
	unsigned int dma_offset;
	dma_addr_t dma_addr;

	/*
	 * offset = state->src.x * cpp;
	 * offset += state->src.y * pitch;
	 * dma_addr = tccdrm_fb_dma_addr(fb, 0) + offset;
	 */

	/* offset = state->src.x * cpp; */
	if (!tcc_math_check_uint_mul_uint(plane_state->src.x, cpp)) {
		internal_ok = (bool)false;
	} else {
		dma_offset = plane_state->src.x * cpp;
	}
	/* -- */

	/* offset += state->src.y * pitch; */
	if (internal_ok) {
		if (!tcc_math_check_uint_mul_uint(plane_state->src.y, pitch)) {
			internal_ok = (bool)false;
		} else {
			utmp = plane_state->src.y * pitch;
		}
	}
	if (internal_ok) {
		if (!tcc_math_check_uint_plus_uint(dma_offset, utmp)) {
			internal_ok = (bool)false;
		} else {
			dma_offset += utmp;
		}
	}
	/* -- */

	/* dma_addr = tccdrm_fb_dma_addr(fb, 0) + offset; */
	if (internal_ok) {
		dma_addr = tccdrm_fb_dma_addr(fb, 0) + dma_offset;
		if (dma_addr == (dma_addr_t)0) {
			DRM_DEV_ERROR(dev_context->dev,
				"dma address of win(%d) is NULL\r\n", win);
			internal_ok = (bool)false;
		}
	}
	/* -- */

	if (internal_ok) {
		if (upper_32_bits(dma_addr) > 0U) {
			DRM_DEV_ERROR(dev_context->dev,
				      "dma address of win(%d) is out of range\r\n",
				      win);
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		/* buffer start address */
		dma_addr32 = lower_32_bits(dma_addr);
		#if defined(CONFIG_VIOC_PVRIC_FBDC)
		/* Skip Header */
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_10_4] */
		/* coverity[misra_c_2012_rule_20_7] */
		if (fb->modifier == DRM_FORMAT_MOD_PVR_FBCDC_8x8_V10) {
			/* dma_addr32 += ALIGN(DI	V_ROUND_UP(plane_state->src.w *
						plane_state->src.h, 64), 256); */
			if (!tcc_math_check_uint_mul_uint(plane_state->src.w,
			    plane_state->src.h)) {
				internal_ok = (bool)false;
				dma_addr32 = 0U;
			} else {
				utmp = (plane_state->src.w * plane_state->src.h);
			}
			if (internal_ok) {
				if (!tcc_math_check_uint_plus_uint(utmp, 64U)) {
					internal_ok = (bool)false;
					dma_addr32 = 0U;
				} else {
					/* coverity[misra_c_2012_rule_10_4] */
					utmp = DIV_ROUND_UP(utmp, 64U);
				}
			}
			if (internal_ok) {
				/* coverity[misra_c_2012_rule_10_4] */
				if (!tcc_math_check_uint_plus_uint(utmp, 255U)) {
					dma_addr32 = 0U;
				} else {
					dma_addr32 += ALIGN(utmp, 256U);
				}
			}
		}
		#endif
	}
	return dma_addr32;
}

#if defined(CONFIG_VIOC_PVRIC_FBDC)
static int lcd_update_prepare_phase0(struct tccdrm_vioc_context *dev_context,
				     const struct tcc_drm_plane_state *plane_state,
				     unsigned int win, unsigned int dma_addr32)
#else
static int lcd_update_prepare_phase0(const struct tccdrm_vioc_context *dev_context,
				     const struct tcc_drm_plane_state *plane_state,
				     unsigned int win)
#endif
{
	const struct drm_framebuffer *fb = plane_state->base.fb;
	struct vioc_fmt_parmas fmt_params;
	bool internal_ok = (bool)true;
	unsigned int enabled;
	void __iomem *pRDMA;
	unsigned int pitch = fb->pitches[0];
	int ret = 0;

	if (lcd_update_check_win(dev_context, win) < 0) {
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		pRDMA = (void __iomem *)dev_context->hw_data.rdma[win].virt_addr;

		/* Swreset RDMAs to prevent fifo-underrun if RDMA was disabled */
		VIOC_RDMA_GetImageEnable(pRDMA, &enabled);
		if (enabled == 0U) {
			if (
				get_vioc_type(
					dev_context->hw_data.rdma[win].blk_num) ==
					get_vioc_type(VIOC_RDMA)) {
				DRM_DEV_DEBUG(dev_context->dev,
					"[DEBUG] win(%d) swreset RDMA, because rdma was disabled\r\n",
					win);
				VIOC_CONFIG_SWReset(
					dev_context->hw_data.rdma[win].blk_num,
					VIOC_CONFIG_RESET);
				VIOC_CONFIG_SWReset(
					dev_context->hw_data.rdma[win].blk_num,
					VIOC_CONFIG_CLEAR);
			}
		}

		tccdrm_drmfmt_to_viocfmt(fb->format->format, &fmt_params.vioc_fmt);

		#if defined(CONFIG_VIOC_PVRIC_FBDC)
		fmt_params.width = plane_state->crtc.w;
		fmt_params.height = plane_state->crtc.h;

		/* Set PVRIC FBDC */
		ret = lcd_set_pvric_fbdc(dev_context, win, dma_addr32,
					(const struct drm_framebuffer *)fb,
					(const struct vioc_fmt_parmas *)&fmt_params);
		if (ret < 0) {
			internal_ok = (bool)false;
		}
		#endif
	}
	if (internal_ok) {
		tccdrm_vioc_plane_set_format(pRDMA, &fmt_params.vioc_fmt, plane_state->src.w, pitch);
	}
	return ret;
}

static int lcd_update_prepare_phase1(const struct tccdrm_vioc_context *dev_context,
				     const struct tcc_drm_plane_state *plane_state,
				     unsigned int win, unsigned int dma_addr32)
{
	const struct drm_framebuffer *fb = plane_state->base.fb;
	bool internal_ok = (bool)true;
	unsigned int dma_base1, dma_base2;
	void __iomem *pRDMA;
	int ret = 0;

	if (lcd_update_check_win(dev_context, win) < 0) {
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		pRDMA = (void __iomem *)dev_context->hw_data.rdma[win].virt_addr;

		/* Using the pixel alpha */
		VIOC_RDMA_SetImageAlphaSelect(pRDMA, 1);
		if (
			(fb->format->format == DRM_FORMAT_ARGB8888) ||
			(fb->format->format == DRM_FORMAT_ABGR8888)) {
			VIOC_RDMA_SetImageAlphaEnable(pRDMA, 1);
		} else {
			VIOC_RDMA_SetImageAlphaEnable(pRDMA, 0);
		}

		switch (fb->format->format) {
		case DRM_FORMAT_YVU420:
			if (!tcc_math_check_uint_plus_uint(dma_addr32, fb->offsets[2])) {
				internal_ok = (bool)false;
				ret = -EINVAL;
				break;
			} else {
				dma_base1 = dma_addr32+fb->offsets[2];
			}
			if (!tcc_math_check_uint_plus_uint(dma_addr32, fb->offsets[1])) {
				internal_ok = (bool)false;
				ret = -EINVAL;
				break;
			} else {
				dma_base2 = dma_addr32+fb->offsets[1];
			}
			break;
		case DRM_FORMAT_NV12:
		case DRM_FORMAT_NV21:
		case DRM_FORMAT_YUV420:
			if (!tcc_math_check_uint_plus_uint(dma_addr32, fb->offsets[1])) {
				internal_ok = (bool)false;
				ret = -EINVAL;
				break;
			} else {
				dma_base1 = dma_addr32+fb->offsets[1];
			}
			if (!tcc_math_check_uint_plus_uint(dma_addr32, fb->offsets[2])) {
				internal_ok = (bool)false;
				ret = -EINVAL;
				break;
			} else {
				dma_base2 = dma_addr32+fb->offsets[2];
			}
			break;
		default:
			dma_base1 = 0U;
			dma_base2 = 0U;
			break;
		}
	}

	if (internal_ok) {
		/* Update base address */
		VIOC_RDMA_SetImageBase(pRDMA, dma_addr32, dma_base1, dma_base2);
	}
	return ret;
}

static bool lcd_check_upd_rdma_first(const struct tccdrm_vioc_context *dev_context,
				     const struct tcc_drm_plane_state *plane_state,
				     unsigned int win)
{
	const void __iomem *pRDMA, *pWMIX, *pDISP;
	struct tcc_drm_rect old_crtc = {.x = 0u, .y = 0u, .w = 0u, .h = 0u};
	unsigned int out_w, out_h;
	bool upd_rdma_first = (bool)true;
	bool internal_ok = (bool)true;
	int wmix_img_num;

	if (dev_context == NULL) {
		internal_ok = (bool)false;
	} else {
		wmix_img_num = VIOC_RDMA_GetImageNum(dev_context->hw_data.rdma[win].blk_num);
	}
	if (plane_state == NULL) {
		internal_ok = (bool)false;
	}

	if( internal_ok )  {
		if ((wmix_img_num < 0) || (wmix_img_num > CRTC_WIN_NR_MAX)) {
			DRM_DEV_ERROR(dev_context->dev, "wmix image number %d is out of range\r\n", wmix_img_num);
			internal_ok = (bool)false;
		}
	}

	if (internal_ok) {
		pRDMA = (const void __iomem *)dev_context->hw_data.rdma[win].virt_addr;
		pWMIX = (const void __iomem *)dev_context->hw_data.wmixer.virt_addr;
		pDISP = (const void __iomem *)dev_context->hw_data.display_device.virt_addr;

		if (pWMIX != NULL) {
			VIOC_WMIX_GetPosition(pWMIX, (unsigned int)wmix_img_num, &old_crtc.x, &old_crtc.y);
			VIOC_WMIX_GetSize(pWMIX, &out_w, &out_h);
		} else {
			/* RDMA - DISP path */
			VIOC_DISP_GetPosition(pDISP, &old_crtc.x, &old_crtc.y);
			VIOC_DISP_GetSize(pDISP, &out_w, &out_h);
		}
		VIOC_RDMA_GetImageSize(pRDMA, &old_crtc.w, &old_crtc.h);

		if (!tcc_math_check_uint_plus_uint(old_crtc.x, plane_state->crtc.w)) {
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		if ((old_crtc.x + plane_state->crtc.w) >  out_w) {
			upd_rdma_first = (bool)false;
		}
		if (!tcc_math_check_uint_plus_uint(old_crtc.y, plane_state->crtc.h)) {
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		if ((old_crtc.y + plane_state->crtc.h) >  out_h) {
			upd_rdma_first = (bool)false;
		}
	}
	return upd_rdma_first;
}

static int lcd_update_prepare_phase2_get_wmix_image(const struct tccdrm_vioc_context *dev_context,
														 unsigned int win)
{
	bool internal_ok = (bool)true;
	int wmix_img_num = -1;
	int ret = -1;

	if (lcd_update_check_win(dev_context, win) < 0) {
			internal_ok = (bool)false;
	}
	if (internal_ok) {
		wmix_img_num = VIOC_RDMA_GetImageNum(dev_context->hw_data.rdma[win].blk_num);

		if ((wmix_img_num < 0) || (wmix_img_num > CRTC_WIN_NR_MAX)) {
			DRM_DEV_ERROR(dev_context->dev, "wmix image number %d is out of range\r\n", wmix_img_num);
			internal_ok = (bool)false;
		}
	}

	if(internal_ok) {
		ret = wmix_img_num;
	}

	return ret;
}

static void lcd_update_prepare_phase2_upd_RDMA_first(const struct tccdrm_vioc_context *dev_context,
														 const struct tcc_drm_plane_state *plane_state,
														 unsigned int win,
														 bool upd_rdma_first,
														 unsigned wmix_img_num)
{
	void __iomem *pDISP = dev_context->hw_data.display_device.virt_addr;
	void __iomem *pWMIX = dev_context->hw_data.wmixer.virt_addr;

	if (upd_rdma_first) {
		VIOC_RDMA_SetImageSize(dev_context->hw_data.rdma[win].virt_addr,
					   plane_state->crtc.w, plane_state->crtc.h);
		VIOC_RDMA_SetImageEnable(dev_context->hw_data.rdma[win].virt_addr);

		if (pWMIX != NULL) {
			VIOC_WMIX_SetPosition(pWMIX, wmix_img_num, plane_state->crtc.x, plane_state->crtc.y);
			VIOC_WMIX_SetUpdate(pWMIX);
		} else {
			/* RDMA - DISP path */
			VIOC_DISP_SetPosition(pDISP, plane_state->crtc.x, plane_state->crtc.y);
		}
	} else {
		DRM_DEV_INFO(dev_context->dev,
				"win(%d) should be update wmix first.\r\n", win);
		if (pWMIX != NULL) {
			VIOC_WMIX_SetPosition(pWMIX, (unsigned int)wmix_img_num, plane_state->crtc.x, plane_state->crtc.y);
			VIOC_WMIX_SetUpdate(pWMIX);
		} else {
			VIOC_DISP_SetPosition(pDISP, plane_state->crtc.x, plane_state->crtc.y);
		}
		VIOC_RDMA_SetImageSize(dev_context->hw_data.rdma[win].virt_addr,
					   plane_state->crtc.w, plane_state->crtc.h);
		VIOC_RDMA_SetImageEnable(dev_context->hw_data.rdma[win].virt_addr);
	}
}

#if defined(CONFIG_DRM_TELECHIPS_CTRL_CHROMAKEY)
static int lcd_update_prepare_phase2(struct tccdrm_vioc_context *dev_context,
				     const struct tcc_drm_plane_state *plane_state,
				     unsigned int win)
#else
static int lcd_update_prepare_phase2(const struct tccdrm_vioc_context *dev_context,
				     const struct tcc_drm_plane_state *plane_state,
				     unsigned int win)
#endif
{
	bool upd_rdma_first = (bool)true;
	int wmix_img_num;

	wmix_img_num = lcd_update_prepare_phase2_get_wmix_image(dev_context, win);
	if(wmix_img_num >= 0) {
		upd_rdma_first =
			lcd_check_upd_rdma_first((const struct tccdrm_vioc_context *)dev_context,
						 plane_state, win);
		#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
		mutex_lock(&dev_context->chromakey_mutex);
		#endif

		lcd_update_prepare_phase2_upd_RDMA_first(dev_context, plane_state, win, upd_rdma_first, (unsigned int)wmix_img_num);

		#if defined(CONFIG_DRM_TCC_CTRL_CHROMAKEY)
		mutex_unlock(&dev_context->chromakey_mutex);
		#endif
	}

	return 0;
}

/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 2>
 * HIS metric violation (HIS_CCM)
 *  DR <case 3>
 */
/* coverity[HIS_metric] - HIS_CALLS */
/* coverity[HIS_metric] - HIS_CCM */
/* coverity[misra_c_2012_rule_8_13] */
static void tccdrm_vioc_plane_update(struct drm_plane *plane,
				struct drm_plane_state *old_drm_pstate)
{
	struct drm_plane_state *drm_pstate =
		(struct drm_plane_state *)plane->state;
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_vioc_context *dev_context =
			(struct tccdrm_vioc_context *)to_tccdrm_vioc_context(drm_pstate->crtc);

	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tcc_drm_plane *tcc_plane = to_tcc_plane(plane);

	const struct tcc_drm_plane_state *tcc_pstate =
		/* coverity[cert_arr39_c] */
		/* coverity[cert_dcl37_c] */
		/* coverity[misra_c_2012_rule_8_5] */	//
		/* coverity[misra_c_2012_rule_8_6] */	//
		/* coverity[misra_c_2012_rule_8_13] */
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_11_5] */
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_15_6] */
		/* coverity[misra_c_2012_rule_18_4] */
		/* coverity[misra_c_2012_rule_20_7] */
		/* coverity[misra_c_2012_rule_21_2] */
		(const struct tcc_drm_plane_state *)to_tcc_plane_state(drm_pstate);

	const struct drm_framebuffer *fb = tcc_pstate->base.fb;
	unsigned int win = tcc_plane->win;
	bool internal_ok = (bool)true;
	unsigned int dma_addr32;
	int ret = 0;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter old_drm_pstate is not
	 * used in the function.
	 */
	(void)old_drm_pstate;

	if (drm_pstate->crtc != NULL) {
		#if defined(CONFIG_REFCODE_PRE_K54)
		plane->crtc = tcc_pstate->crtc;
		#endif

		if (dev_context->keep_logo > 0) {
			DRM_DEV_INFO(dev_context->dev,
				     "[INFO] skip logo (%d)\r\n",
				     dev_context->keep_logo);
			dev_context->keep_logo--;
			internal_ok = (bool)false;
		}

		if (lcd_update_check_win(dev_context, win) < 0) {
			internal_ok = (bool)false;
		}

		if (internal_ok) {
			/*
			* DRM v1.2.8
			* Add name "overlay_skip_yuv" of plane properties to support
			* Android hwrendere.
			* If this property is set on plane, This plane will be skip process
			* of atomic_update when input format is YUV and returns success.
			*/
			if (
				(DRM_PLANE_TYPE(dev_context->hw_data.rdma_plane_type[win]) ==
				(unsigned int)DRM_PLANE_TYPE_OVERLAY) &&
				(DRM_PLANE_FLAG(dev_context->hw_data.rdma_plane_type[win]) ==
				DRM_PLANE_FLAG(DRM_PLANE_FLAG_SKIP_YUV_FORMAT)) &&
				(
					(fb->format->format == DRM_FORMAT_NV12) ||
					(fb->format->format == DRM_FORMAT_NV21) ||
					(fb->format->format == DRM_FORMAT_YUV420) ||
					(fb->format->format == DRM_FORMAT_YVU420))) {
				internal_ok = (bool)false;
			}
		}

		if (internal_ok) {
			dma_addr32 = lcd_update_get_dma_address((const struct tccdrm_vioc_context *)dev_context, tcc_pstate, win);
			if (dma_addr32 == 0U) {
				internal_ok = (bool)false;
			}
		}
		if (internal_ok) {
			#if defined(CONFIG_VIOC_PVRIC_FBDC)
			ret = lcd_update_prepare_phase0(dev_context, tcc_pstate,
							win, dma_addr32);
			#else
			ret = lcd_update_prepare_phase0((const struct tccdrm_vioc_context *)dev_context, tcc_pstate,
							win);
			#endif
			if (ret < 0) {
				internal_ok = (bool)false;
			}
		}
		if (internal_ok) {
			ret = lcd_update_prepare_phase1((const struct tccdrm_vioc_context *)dev_context,
							tcc_pstate,
							win, dma_addr32);
			if (ret < 0) {
				internal_ok = (bool)false;
			}
		}
		if (internal_ok) {
			#if defined(CONFIG_DRM_TELECHIPS_CTRL_CHROMAKEY)
			ret = lcd_update_prepare_phase2(dev_context,tcc_pstate,
							win);
			#else
			ret = lcd_update_prepare_phase2((const struct tccdrm_vioc_context *)dev_context,
							tcc_pstate, win);
			#endif
		}
	}
}

/* coverity[misra_c_2012_rule_8_13] */
static void tccdrm_vioc_plane_disable(struct drm_plane *plane,
				 struct drm_plane_state *old_drm_pstate)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tcc_drm_plane *tcc_plane = to_tcc_plane(plane);

	if (old_drm_pstate->crtc != NULL) {
		#if defined(CONFIG_VIOC_PVRIC_FBDC)
		/* coverity[cert_arr39_c] */
		/* coverity[cert_dcl37_c] */
		/* coverity[misra_c_2012_rule_8_5] */
		/* coverity[misra_c_2012_rule_8_6] */
		/* coverity[misra_c_2012_rule_8_13] */
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_11_5] */
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_15_6] */
		/* coverity[misra_c_2012_rule_18_4] */
		/* coverity[misra_c_2012_rule_20_7] */
		/* coverity[misra_c_2012_rule_21_2] */
		struct tccdrm_vioc_context *dev_context =
				(struct tccdrm_vioc_context *)to_tccdrm_vioc_context(old_drm_pstate->crtc);
		#else
		/* coverity[cert_arr39_c] */
		/* coverity[cert_dcl37_c] */
		/* coverity[misra_c_2012_rule_8_5] */
		/* coverity[misra_c_2012_rule_8_6] */
		/* coverity[misra_c_2012_rule_8_13] */
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_11_5] */
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_15_6] */
		/* coverity[misra_c_2012_rule_18_4] */
		/* coverity[misra_c_2012_rule_20_7] */
		/* coverity[misra_c_2012_rule_21_2] */
		const struct tccdrm_vioc_context *dev_context =
				(const struct tccdrm_vioc_context *)to_tccdrm_vioc_context(old_drm_pstate->crtc);
		#endif

		lcd_set_video_output((const struct tccdrm_vioc_context *)dev_context,
				     tcc_plane->win, (bool)false);
		#if defined(CONFIG_VIOC_PVRIC_FBDC)
		(void)lcd_disconnect_pvric_fbdc(dev_context, tcc_plane->win);
		#endif
	}
}

/* plane helper funcs -------------------------------------------------------*/
static const struct drm_plane_helper_funcs tccdrm_vioc_plane_helper_funcs = {
	.prepare_fb =  drm_gem_fb_prepare_fb,
	.atomic_check = tccdrm_vioc_plane_check,
	.atomic_update = tccdrm_vioc_plane_update,
	.atomic_disable = tccdrm_vioc_plane_disable,
};

/* FUNCTIONS for CRTC --------------------------------------------------------*/
static int tccvioc_clk_disable(struct tccdrm_vioc_context *dev_context,
				unsigned int clk_nr_bit)
{
	bool internal_ok = (bool)true;
	bool keep_pclk = (bool)false;
	struct clk *target_clk;
	int ret = 0;

	switch (clk_nr_bit) {
		case CRTC_FLAGS_VCLK_BIT:
			target_clk = dev_context->hw_data.vioc_clock;
			break;
		case CRTC_FLAGS_PCLK_BIT:
			target_clk = dev_context->hw_data.ddc_clock;
			if (dev_context->hw_data.keep_pclk) {
				keep_pclk = (bool)true;
			}
			if (dev_context->hw_data.limited_plane_only_mode) {
				/* plane only */
				DRM_DEV_INFO(dev_context->dev, "[INFO] skip because this is plane only mode\r\n");
				internal_ok = (bool)false;
			}
			break;
		default:
			ret = -EINVAL;
			break;
	}

	if (internal_ok) {
		if (ret == 0) {
			if (test_and_clear_bit(clk_nr_bit, &dev_context->crtc_flags) == 1) {
				if (keep_pclk) {
					DRM_DEV_INFO(dev_context->dev,
						     "[WARN] skip disable PCLK for LVDS\r\n");
				} else {
					DRM_DEV_INFO(dev_context->dev,"[INFO] Disable %s\r\n",
						(clk_nr_bit ==
						(unsigned int)CRTC_FLAGS_VCLK_BIT) ? "VCLK" : "PCLK");
					clk_disable_unprepare(target_clk);
				}
			}
		}
	}
	return ret;
}

/* HIS metric violation (HIS_GOTO) caused by wait_event_interruptible_timeout */
/* coverity[HIS_metric] - HIS_GOTO */
static void tccvioc_wait_display_dev_disabled(struct tccdrm_vioc_context *dev_context, unsigned long wait_ms)
{
	long wait_ret;

	if (tcc_math_ulong_gt_longmax(wait_ms)) {
	DRM_DEV_INFO(dev_context->dev,
		"[WARN] msecs_to_jiffies is invalid\n");
	} else {
		/* coverity[cert_dcl37_c] */
		/* coverity[cert_pre31_c] */
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_10_3] */
		/* coverity[misra_c_2012_rule_10_4] */
		/* coverity[misra_c_2012_rule_12_1] */
		/* coverity[misra_c_2012_rule_14_3] */
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_15_1] */
		/* coverity[misra_c_2012_rule_15_4] */
		/* coverity[misra_c_2012_rule_15_6] */
		/* coverity[misra_c_2012_rule_16_4] */
		/* coverity[misra_c_2012_rule_20_7] */
		wait_ret = wait_event_interruptible_timeout(
			dev_context->wait_display_done_queue,
			/* coverity[cert_dcl37_c] */
			/* coverity[cert_pre31_c] */
			/* coverity[misra_c_2012_rule_10_3] */
			/* coverity[misra_c_2012_rule_14_4] */
			/* coverity[misra_c_2012_rule_15_6] */
			/* coverity[misra_c_2012_rule_21_2] */
			(atomic_read(
				&dev_context->wait_display_done_event) == 0),
			(long)wait_ms);
		if (wait_ret == 0) {
			DRM_DEV_INFO(dev_context->dev,
					"A timeout occurred while waiting for the display controller is shut down.\n");
		}
	}
}

static void tccvioc_disable_display_dev(struct tccdrm_vioc_context *dev_context)
{
	unsigned long wait_ms = msecs_to_jiffies(30U);

	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_21_2] */
	atomic_set(&dev_context->wait_display_done_event, 1);
	if (dev_context->hw_data.limited_plane_only_mode) {
		/* plane only */
		DRM_DEV_INFO(dev_context->dev, "[INFO] skip because this is plane only mode\r\n");
	} else {
		VIOC_DISP_TurnOff(dev_context->hw_data.display_device.virt_addr);
		if (test_bit(CRTC_FLAGS_IRQ_BIT, &dev_context->crtc_flags) != 1) {
			msleep(30);
			DRM_DEV_INFO(dev_context->dev,
					"[INFO] It will be wait 30ms because interrupt is not enabled.\n");
		} else {
			tccvioc_wait_display_dev_disabled(dev_context, wait_ms);
		}
	}
}

/* update pixel clock for encoder dev */
static int tccdrm_vioc_crtc_update_pixel_clock(const struct tccdrm_vioc_context *dev_context,
					       struct tcc_crtc_state *tcc_cstate)
{
	bool internal_ok = (bool)true;
	unsigned long pixel_clock;
	unsigned long ul_div_val;
	unsigned int div_val;
	int ret = 0;

	if (dev_context->hw_data.limited_plane_only_mode) {
		/* plane only */
		//DRM_DEV_INFO(dev_context->dev, "[INFO] skip because this is plane only mode\r\n");
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		pixel_clock = clk_get_rate(dev_context->hw_data.ddc_clock);
		div_val = vioc_disp_get_clkdiv(dev_context->hw_data.display_device.virt_addr);

		if (div_val > 0U) {
			ul_div_val = (unsigned long)div_val;

			if (!tcc_math_check_ulong_mul_ulong(ul_div_val, 2UL)) {
				DRM_DEV_ERROR(dev_context->dev,
						"The div value for refefence clock of display controller is out of range\r\n");
				internal_ok = (bool)false;
				ret = -EINVAL;
			} else {
				ul_div_val *= 2UL;

				if (pixel_clock > ul_div_val) {
					pixel_clock /= ul_div_val;
				} else {
					pixel_clock = 0UL;
				}
			}
		}
	}
	if (internal_ok) {
		tcc_cstate->pixel_clock = pixel_clock;
		//DRM_DEV_INFO(dev_context->dev,
		//	"[INFO] update pixel clocks %ldHz\r\n",
		//	tcc_cstate->pixel_clock);
	}
	return ret;
}

#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC807X)
static bool tccdrm_vioc_crtc_check_pixelclock_match(const struct device *dev,
						const struct tcc_hw_device *hw_data,
						unsigned long req_clk_rate)
{
	unsigned int clk_div =
		vioc_disp_get_clkdiv(hw_data->display_device.virt_addr);
	unsigned long curr_clk_rate = clk_get_rate(hw_data->ddc_clock);
	bool internal_ok = (bool)true;
	bool match = (bool)false;
	unsigned long bp, le, gt;

	if (clk_div > 0U) {
		unsigned long ul_clk_div = (unsigned long)clk_div;

		if (!tcc_math_check_ulong_mul_ulong(ul_clk_div, 2UL)) {
			internal_ok = (bool)false;
		} else {
			ul_clk_div *= 2UL;

			if (!tcc_math_check_ulong_mul_ulong(req_clk_rate, ul_clk_div)) {
				internal_ok = (bool)false;
			} else {
				req_clk_rate *= ul_clk_div;
			}
		}
	}
	if (internal_ok) {
		if (req_clk_rate < 10UL) {
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		if (!tcc_math_check_ulong_plus_ulong(req_clk_rate, 10UL)) {
			internal_ok = (bool)false;
		}
		if (internal_ok) {
			/* coverity[misra_c_2012_rule_10_4] */
			bp = DIV_ROUND_UP(req_clk_rate, 10UL);

			if (!tcc_math_check_ulong_plus_ulong(req_clk_rate, bp)) {
				internal_ok = (bool)false;
			}
		}
		if (internal_ok) {
			le = req_clk_rate - bp;
			gt = req_clk_rate + bp;

			if ((curr_clk_rate > le) && (curr_clk_rate < gt)) {
				match = (bool)true;
			}
		}
	}

	if (!match) {
		DRM_DEV_INFO(dev,
		"[INFO] display device(%d) clock is not match %ldHz : %luHz\r\n",
		get_vioc_index(hw_data->display_device.blk_num),
		curr_clk_rate, req_clk_rate);
	}

	return match;
}

static bool
tccdrm_vioc_check_turnoff_display_controller(struct tccdrm_vioc_context *dev_context,
					     const struct drm_crtc_state *drm_cstate)
{
	const struct tcc_hw_device *hw_data = &dev_context->hw_data;
	const struct drm_display_mode *adjusted_mode =
		(const struct drm_display_mode *)&drm_cstate->adjusted_mode;
	struct DisplayBlock_Info ddinfo;
	bool need_turnoff = (bool)false;
	bool internal_ok = (bool)true;

	unsigned int hdisplay, vdislay;

	(void)memset(&ddinfo, 0, sizeof(struct DisplayBlock_Info));

	if (dev_context->hw_data.limited_plane_only_mode) {
		/* plane only */
		DRM_DEV_INFO(dev_context->dev, "[INFO] skip because this is plane only mode\r\n");
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		VIOC_DISP_GetDisplayBlock_Info(
			hw_data->display_device.virt_addr, &ddinfo);

		/* Check turn on status of display device */
		if (ddinfo.enable != 0U) {
			if ((adjusted_mode->hdisplay < 1U) || (adjusted_mode->vdisplay < 1U) ||
			    (adjusted_mode->clock < 0)) {
				need_turnoff = (bool)true;
			}
			if (!need_turnoff) {
				hdisplay = (unsigned int)adjusted_mode->hdisplay;
				vdislay = (unsigned int)adjusted_mode->vdisplay;

				if ((ddinfo.width != hdisplay) ||
					(ddinfo.height != vdislay)) {
					DRM_DEV_INFO(dev_context->dev,
						"[INFO] display device(%d) size is not match %dx%d : %dx%d\r\n",
						get_vioc_index(hw_data->display_device.blk_num),
						ddinfo.width, ddinfo.height,
						adjusted_mode->hdisplay, vdislay);
					need_turnoff = (bool)true;
				}
			}
			if (!need_turnoff) {
				/* Check pixel clock */
				unsigned long mode_clocks = (unsigned long)adjusted_mode->clock;

				if (!tcc_math_check_ulong_mul_ulong(mode_clocks, 1000UL)) {
					need_turnoff = (bool)true;
				} else {
					if (!tccdrm_vioc_crtc_check_pixelclock_match(dev_context->dev,
						hw_data, mode_clocks * 1000UL)) {
						DRM_DEV_INFO(dev_context->dev,
							     "[INFO] display device(%d) pixelclock is not match\r\n",
							     get_vioc_index(hw_data->display_device.blk_num));
						need_turnoff = (bool)true;
					}
				}
			}
		} else {
			dev_context->keep_logo = 0;
			DRM_DEV_INFO(dev_context->dev, "Display device was turned of, keep_logo will be set zero\r\n");
		}
	}
	return need_turnoff;
}
#endif

/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 2>
 * HIS metric violation (HIS_CCM)
 *  DR <case 3>
 */
/* coverity[HIS_metric] - HIS_CALLS */
/* coverity[HIS_metric] - HIS_CCM */
static int tccdrm_vioc_crtc_atomic_check(struct drm_crtc *crtc,
				     struct drm_crtc_state *drm_cstate)
{
	struct tccdrm_vioc_context *dev_context =
			/* coverity[cert_arr39_c] */
			/* coverity[cert_dcl37_c] */
			/* coverity[misra_c_2012_rule_8_5] */
			/* coverity[misra_c_2012_rule_8_6] */
			/* coverity[misra_c_2012_rule_8_13] */
			/* coverity[misra_c_2012_rule_10_1] */
			/* coverity[misra_c_2012_rule_11_5] */
			/* coverity[misra_c_2012_rule_14_4] */
			/* coverity[misra_c_2012_rule_15_6] */
			/* coverity[misra_c_2012_rule_18_4] */
			/* coverity[misra_c_2012_rule_20_7] */
			/* coverity[misra_c_2012_rule_21_2] */
			(struct tccdrm_vioc_context *)to_tccdrm_vioc_context(crtc);
	const struct drm_display_mode *adjusted_mode =
		(const struct drm_display_mode *)&drm_cstate->adjusted_mode;
	unsigned long ideal_clk, lcd_rate;
	const void __iomem *ddi_config;

	bool pixel_clock_from_hdmi;
	bool internal_ok = (bool)true;

	unsigned long clkdiv;
	int vrefresh;
	int ret = 0;

	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tcc_crtc_state *tcc_cstate = to_tcc_crtc_state(drm_cstate);

	tcc_cstate->lcdc_num =
		get_vioc_index(dev_context->hw_data.display_device.blk_num);
	tcc_cstate->lcdc_mux_select =
		dev_context->hw_data.lcdc_mux_select;
	tcc_cstate->lcdc_mux_bypass =
		dev_context->hw_data.lcdc_mux_bypass;
	(void)tccdrm_vioc_crtc_update_pixel_clock(dev_context, tcc_cstate);

	#if defined(CONFIG_ARCH_TCC803X)
	if (tcc_cstate->connector_type == DRM_MODE_CONNECTOR_LVDS) {
		dev_context->hw_data.keep_pclk = (bool)true;
	}
	#endif

	#if defined(CONFIG_ARCH_TCC807X)
	if (tcc_cstate->connector_type == DRM_MODE_CONNECTOR_DSI) {
		dev_context->hw_data.keep_pclk = (bool)true;
	}
	#endif

	//DRM_DEV_INFO(dev_context->dev,
	//	     "[INFO] update lcdc_num = %d, 0x%px\r\n", tcc_cstate->lcdc_num, tcc_cstate);
	//DRM_DEV_INFO(dev_context->dev,
	//	     "[INFO] update connector_type = %d\r\n",
	//	     tcc_cstate->connector_type);

	if (drm_cstate->enable) {
		ddi_config = (const void __iomem *)VIOC_DDICONFIG_GetAddress();
		#if defined(CONFIG_ARCH_TCC803X)
		if (
			VIOC_DDICONFIG_GetPeriClock(
				ddi_config,
				get_vioc_index(dev_context->hw_data.display_device.blk_num)) != 0) {
			pixel_clock_from_hdmi = (bool)true;
		}
		#else
			pixel_clock_from_hdmi = (bool)false;
		#endif

		if (adjusted_mode->clock <= 0) {
			DRM_DEV_INFO(
				dev_context->dev,
				"[WARN] Mode has zero clock value.\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		}

		if (internal_ok) {
			vrefresh = drm_mode_vrefresh(adjusted_mode);
			if (vrefresh < 0) {
				ret = -EINVAL;
			} else {
				dev_context->vrefresh = (unsigned int)vrefresh;
			}

			if (dev_context->hw_data.limited_plane_only_mode) {
				/* plane only */
				DRM_DEV_INFO(dev_context->dev, "[INFO] skip because this is plane only mode\r\n");
			}
			else {
				/* coverity[misra_c_2012_rule_14_3] */
				if (pixel_clock_from_hdmi == false) {
					unsigned long mode_clock, khz;

					mode_clock = (unsigned long)adjusted_mode->clock;
					khz = 1000UL;

					if (!tcc_math_check_ulong_mul_ulong(mode_clock, khz)) {
						internal_ok = (bool)false;
						ret = -EINVAL;
					}
					if (internal_ok) {
						ideal_clk = mode_clock * khz;
						lcd_rate = clk_get_rate(dev_context->hw_data.ddc_clock);

						if (lcd_rate == 0UL) {
							DRM_DEV_INFO(dev_context->dev,
								"[WARN] The sclk_lcd clock is zero\n");
							internal_ok = (bool)false;
							ret = -EINVAL;
						}
					}
					if (internal_ok) {
						if (!tcc_math_check_ulong_mul_ulong(lcd_rate, 2UL)) {
							internal_ok = (bool)false;
							ret = -EINVAL;
						}
					}
					if (internal_ok) {
						if ((lcd_rate * 2UL) < ideal_clk) {
							DRM_DEV_INFO(
								dev_context->dev,
								"[WARN] The sclk_lcd clock too low(%lu) for requested pixel clock(%lu)\n",
								lcd_rate, ideal_clk);
							internal_ok = (bool)false;
						}
					}
					if (internal_ok) {
						/*
						* Find the clock divider value that gets us closest
						* to ideal_clk
						*/
						/* coverity[cert_dcl37_c] */
						/* coverity[cert_int02_c] */
						/* coverity[cert_int31_c] */
						/* coverity[misra_c_2012_rule_10_4] */
						/* coverity[misra_c_2012_rule_12_1] */
						/* coverity[misra_c_2012_rule_20_7] */
						clkdiv = DIV_ROUND_CLOSEST(lcd_rate, ideal_clk);
						if (clkdiv >= 0x200UL) {
							DRM_DEV_INFO(dev_context->dev,
								"[WARN] The Requested pixel clock(%lu) too low\n",
								ideal_clk);
						}
					}
				}
			}
		}
	}

	#if defined(CONFIG_ARCH_TCC803X)
	if (tcc_cstate->connector_type != DRM_MODE_CONNECTOR_HDMIA) {
		/* Currently, only HDMI supports display controller reset */
		clear_bit(CRTC_FLAGS_TIMING_CHECK_BIT, &dev_context->crtc_flags);
	}
	#endif

	#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC807X)
	if (test_and_clear_bit(CRTC_FLAGS_TIMING_CHECK_BIT, &dev_context->crtc_flags) == 1) {
		DRM_DEV_INFO(dev_context->dev,
			     "[INFO] Check display timing set by bootloader\r\n");
		if (tccdrm_vioc_check_turnoff_display_controller(dev_context,
								 (const struct drm_crtc_state *)drm_cstate)) {
			struct drm_atomic_state *drm_astate = drm_cstate->state;
			const struct drm_connector_state *new_connector_state;
			struct drm_connector_state *old_connector_state;
			struct drm_crtc_state *old_crtc_state;

			if (drm_astate != NULL ) {
				/* CRTC STATE */
				old_crtc_state =
					drm_atomic_get_old_crtc_state(drm_astate, crtc);

				/* FORCE DISABLE CRTC */
				old_crtc_state->active = (bool)true;
				drm_cstate->mode_changed = (bool)true;
				dev_context->keep_logo = 0;

				if (tcc_cstate->connector != NULL) {

					/* Increaser reference count for connector */
					drm_connector_get(tcc_cstate->connector);

					/* CONNECTOR STATE */
					old_connector_state =
						drm_atomic_get_old_connector_state(drm_astate,
										   tcc_cstate->connector);
					new_connector_state =
						drm_atomic_get_new_connector_state(drm_astate,
										   tcc_cstate->connector);

					if((old_connector_state != NULL) && (new_connector_state != NULL)) {
						/* FORCE DISABLE CONNECTOR */
						old_connector_state->crtc = crtc;
						old_connector_state->best_encoder = new_connector_state->best_encoder;

						#if defined(CONFIG_REFCODE_PRE_K510)
						#else
						if (new_connector_state->best_encoder != NULL) {
							old_connector_state->best_encoder->crtc = crtc;
						}
						/* inc vblank refcount */
						//drm_crtc_vblank_on(crtc);
						//drm_crtc_vblank_get(crtc);
						#endif
					}
				} else {
					DRM_DEV_INFO(dev_context->dev,
						     "[WARN] connector was NULL. It may be dummydev\r\n");
				}
				DRM_DEV_INFO(dev_context->dev,
				     "[INFO] display device(%d) will be disabled\r\n",
				     get_vioc_index(dev_context->hw_data.display_device.blk_num));
			}
		}
	}
	#endif

	return ret;
}

/* coverity[misra_c_2012_rule_8_13] */
static void tccdrm_vioc_crtc_atomic_begin(struct drm_crtc *crtc,
				  /* coverity[misra_c_2012_rule_8_13] */
				  struct drm_crtc_state *old_drm_cstate)
{
	int ret = 0;

	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_vioc_context *dev_context = (struct tccdrm_vioc_context *)to_tccdrm_vioc_context(crtc);

	if (crtc->state->active && !old_drm_cstate->active) {
		if (test_and_set_bit(CRTC_FLAGS_PCLK_BIT, &dev_context->crtc_flags) != 1) {
			ret = clk_prepare_enable(dev_context->hw_data.ddc_clock);
			if  (ret < 0) {
				DRM_DEV_INFO(
					dev_context->dev,
					"[WARN] It failed to enable the lcd clk\r\n");
			}
		}
	}

}

static void tccdrm_vioc_crtc_atomic_flush(struct drm_crtc *crtc,
				  /* coverity[misra_c_2012_rule_8_13] */
				  struct drm_crtc_state *old_drm_cstate)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_vioc_context *dev_context =
			(struct tccdrm_vioc_context *)to_tccdrm_vioc_context(crtc);

	if (old_drm_cstate->active) {
		tccdrm_crtc_handle_event(crtc, &dev_context->flip_state);
	} else {
		DRM_DEV_DEBUG(dev_context->dev,
			      "[DEBUG] old crtc state is not avtive\r\n");
	}
}

/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 2>
 */
/* coverity[HIS_metric] - HIS_CALLS */
/* coverity[misra_c_2012_rule_8_13] */
static void tcc_drm_crtc_atomic_enable(struct drm_crtc *crtc,
				       struct drm_crtc_state *old_drm_cstate)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_vioc_context *dev_context = (struct tccdrm_vioc_context *)to_tccdrm_vioc_context(crtc);

	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tcc_crtc_state *tcc_cstate = to_tcc_crtc_state(crtc->state);

	int ret;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter old_drm_cstate is not
	 * used in the function.
	 */
	(void)old_drm_cstate;

	DRM_DEV_INFO(dev_context->dev,
				"[INFO] connector_type = %d \r\n",
				tcc_cstate->connector_type);
	if (dev_context->hw_data.limited_plane_only_mode) {
		/* plane only */
		DRM_DEV_INFO(dev_context->dev, "[INFO] skip 1 because this is plane only mode\r\n");
	} else {
		if ((tcc_cstate->connector_type == DRM_MODE_CONNECTOR_LVDS) || (tcc_cstate->connector_type == DRM_MODE_CONNECTOR_DSI)) {
			if (test_and_set_bit(CRTC_FLAGS_PCLK_BIT, &dev_context->crtc_flags) != 1) {
				unsigned long lcd_rate =
					clk_get_rate(dev_context->hw_data.ddc_clock);

				if (dev_context->hw_data.keep_pclk) {
					DRM_DEV_INFO(dev_context->dev,
						     "[WARN] skip enable PCLK for LVDS\r\n");
				} else {
					DRM_DEV_INFO(dev_context->dev,
						     "[INFO] Enable LVDS-PCLK %ldHz \r\n",
						     lcd_rate);
					ret = clk_prepare_enable(dev_context->hw_data.ddc_clock);
					if  (ret < 0) {
						DRM_DEV_INFO(
							dev_context->dev,
							"[WARN] It failed to enable the lcd clk\r\n");
					}
				}
			}
		}
	}
	#if defined(CONFIG_ARCH_TCC803X)
	if (tcc_cstate->connector_type == DRM_MODE_CONNECTOR_HDMIA) {
		VIOC_OUTCFG_SetOutConfig(VIOC_OUTCFG_HDMI,
					 get_vioc_index(dev_context->hw_data.display_device.blk_num));
	}
	#endif
	if (!dev_context->crtc_enabled) {
		DRM_DEV_INFO(dev_context->dev, "[INFO] Turn on\r\n");
		#if defined(CONFIG_PM)
		(void)pm_runtime_get_sync(dev_context->dev);
		#endif
		dev_context->crtc_enabled = (bool)true;
		if (dev_context->hw_data.limited_plane_only_mode) {
			/* plane only */
			DRM_DEV_INFO(dev_context->dev, "[INFO] skip 2 because this is plane only mode\r\n");
		} else {
			VIOC_DISP_TurnOn(dev_context->hw_data.display_device.virt_addr);
		}
	}
	dev_context->fifo_underrun = (bool)false;

	drm_crtc_vblank_on(crtc);

	if (crtc->state->event != NULL) {
		/* coverity[misra_c_2012_rule_10_3] */
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_15_6] */
		WARN_ON(drm_crtc_vblank_get(crtc) != 0);
		tccdrm_crtc_update_flip_event(crtc, &dev_context->flip_state);
	}
}

/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 2>
 */
/* coverity[HIS_metric] - HIS_CALLS */
/* coverity[misra_c_2012_rule_8_13] */
static void tcc_drm_crtc_atomic_disable(struct drm_crtc *crtc,
					struct drm_crtc_state *old_drm_cstate)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_vioc_context *dev_context =
			(struct tccdrm_vioc_context *)to_tccdrm_vioc_context(crtc);
	unsigned int i;

	#if 0
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tcc_crtc_state *tcc_cstate = to_tcc_crtc_state(old_drm_cstate);


	DRM_DEV_INFO(dev_context->dev,
		     "[INFO] check connector_type = %d\r\n",
		     tcc_cstate->connector_type);
	#endif
	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter old_drm_cstate is not
	 * used in the function.
	 */
	(void)old_drm_cstate;


	for (i = 0U; i < (unsigned int)TCCDRM_RDMA_MAX_NUM; i++) {
		lcd_set_video_output((const struct tccdrm_vioc_context *)dev_context,
				     i, (bool)false);
		#if defined(CONFIG_VIOC_PVRIC_FBDC)
		(void)lcd_disconnect_pvric_fbdc(dev_context, i);
		#endif
	}

	if (dev_context->hw_data.limited_plane_only_mode) {
		/* plane only */
		DRM_DEV_INFO(dev_context->dev, "[INFO] skip because this is plane only mode\r\n");
	} else {
		DRM_DEV_INFO(dev_context->dev, "[INFO] Turn off\r\n");

		if (VIOC_DISP_Get_TurnOnOff(dev_context->hw_data.display_device.virt_addr) == 1U) {
			tccvioc_disable_display_dev(dev_context);
		}
	}
	(void)tccvioc_clk_disable(dev_context, CRTC_FLAGS_PCLK_BIT);

	#if defined(CONFIG_PM)
	if (dev_context->crtc_enabled) {
		(void)pm_runtime_put_sync(dev_context->dev);
	}
	#endif

	dev_context->crtc_enabled = (bool)false;
	drm_crtc_vblank_off(crtc);

	if (crtc->state->event != NULL) {
		unsigned long irq_flags;

		/* coverity[cert_dcl37_c] */
		/* coverity[misra_c_2012_rule_14_4] */
		spin_lock_irqsave(&crtc->dev->event_lock, irq_flags);
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		spin_unlock_irqrestore(&crtc->dev->event_lock, irq_flags);
		crtc->state->event = NULL;
	}
}
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC807X)
static bool tcc_drm_crtc_check_reset_display_controller(const struct tccdrm_vioc_context *dev_context)
{
	struct DisplayBlock_Info ddinfo;
	bool need_reset = (bool)false;

	(void)memset(&ddinfo, 0, sizeof(struct DisplayBlock_Info));

	VIOC_DISP_GetDisplayBlock_Info(dev_context->hw_data.display_device.virt_addr,
				       &ddinfo);

	/* Check turn on status of display device */
	if (ddinfo.enable == 0U) {
		DRM_DEV_INFO(dev_context->dev,
			     "[INFO] display device(%d) is disabled\r\n",
			     get_vioc_index(dev_context->hw_data.display_device.blk_num));
		need_reset = (bool)true;
	}
	return need_reset;
}

static void tcc_crtc_convert_timing_interlace(bool interlace,
					      const struct drm_display_mode *in_mode,
					      const struct videomode *vm,
					      stLTIMING *stTimingParam,
					      stLCDCTR *stCtrlParam)
{
	if (interlace) {
		stCtrlParam->tv = 1U;
		stCtrlParam->advi = 1U;
	} else {
		stCtrlParam->ni = 1U;
	}

	/* Calc vactive */
	stTimingParam->lpc = vm->hactive;
	stTimingParam->lewc = vm->hfront_porch;
	if(vm->hsync_len >= 1U)
	{
		stTimingParam->lpw = vm->hsync_len - 1U;
	}
	stTimingParam->lswc = vm->hback_porch;

	if (interlace) {
		stTimingParam->fpw = vm->vsync_len - 1U;
		stTimingParam->fswc = vm->vback_porch -2U;
		stTimingParam->fewc = vm->vfront_porch;
		if( (UINT_MAX-stTimingParam->fswc) >= 1U ) {
			stTimingParam->fswc2 = stTimingParam->fswc + 1U;
		}
		if( stTimingParam->fewc >= 1U ) {
			stTimingParam->fewc2 = stTimingParam->fewc - 1U;
		}
		if (
			(in_mode->vtotal == 1250U) &&
			(vm->hactive == 1920U) &&
			(vm->vactive == 1080U)) {
			/* VIC 1920x1080@50i 1250 vtotal */
			if( stTimingParam->fewc >= 2U ) {
				stTimingParam->fewc -= 2U;
			}
		}
	} else {
		if(vm->vsync_len >= 1U)
		{
			stTimingParam->fpw = vm->vsync_len - 1U;
		}
		if(vm->vback_porch >= 1U)
		{
			stTimingParam->fswc = vm->vback_porch - 1U;
		}
		if(vm->vfront_porch >= 1U)
		{
			stTimingParam->fewc = vm->vfront_porch - 1U;
		}
		stTimingParam->fswc2 = stTimingParam->fswc;
		stTimingParam->fewc2 = stTimingParam->fewc;
	}
}

static int tcc_crtc_convert_timing(const struct tccdrm_vioc_context *dev_context,
				    const struct drm_display_mode *in_mode,
				    struct videomode *vm,
				    stLTIMING *stTimingParam,
				    stLCDCTR *stCtrlParam)
{
	const struct device *dev = (const struct device *)dev_context->dev;
	unsigned int cmp_flags, vm_flags;
	bool interlace;
	int ret = 0;

	(void)memset(stCtrlParam, 0, sizeof(stLCDCTR));
	(void)memset(stTimingParam, 0, sizeof(stLTIMING));

	drm_display_mode_to_videomode(in_mode, vm);

	if (vm->vactive == 0U) {
		DRM_DEV_ERROR(dev, "vactive on crtc tmining is zero\r\n");
		ret = -EINVAL;
	} else if (vm->hactive == 0U) {
		DRM_DEV_ERROR(dev, "hactive on crtc tmining is zero\r\n");
		ret = -EINVAL;
	} else if (vm->hsync_len == 0U) {
		DRM_DEV_ERROR(dev, "hsync length on crtc tmining is zero\r\n");
		ret = -EINVAL;
	} else if (vm->hback_porch == 0U) {
		DRM_DEV_ERROR(dev, "hback porch on crtc tmining is zero\r\n");
		ret = -EINVAL;
	} else if (vm->hfront_porch == 0U) {
		DRM_DEV_ERROR(dev, "hfront porch on crtc tmining is zero\r\n");
		ret = -EINVAL;
	} else if (vm->vsync_len == 0U) {
		DRM_DEV_ERROR(dev, "vsync length on crtc tmining is zero\r\n");
		ret = -EINVAL;
	} else if (vm->vback_porch == 0U) {
		DRM_DEV_ERROR(dev, "vback porch on crtc tmining is zero\r\n");
		ret = -EINVAL;
	} else if (vm->vfront_porch == 0U) {
		DRM_DEV_ERROR(dev, "vfront porch on crtc tmining is zero\r\n");
		ret = -EINVAL;
	} else {
		vm_flags = (unsigned int)vm->flags;

		cmp_flags = (unsigned int)DISPLAY_FLAGS_INTERLACED;
		interlace = ((vm_flags & cmp_flags) != 0U) ? (bool)true : (bool)false;

		cmp_flags = (unsigned int)DISPLAY_FLAGS_VSYNC_LOW;
		stCtrlParam->iv = ((vm_flags & cmp_flags) != 0U) ? 1U : 0U;

		cmp_flags = (unsigned int)DISPLAY_FLAGS_HSYNC_LOW;
		stCtrlParam->ih = ((vm_flags & cmp_flags) != 0U) ? 1U : 0U;

		cmp_flags = (unsigned int)DISPLAY_FLAGS_DOUBLECLK;
		stCtrlParam->dp = ((vm_flags & cmp_flags) != 0U) ? 1U : 0U;

		tcc_crtc_convert_timing_interlace(interlace, in_mode, vm, stTimingParam, stCtrlParam);

		/* Common Timing Parameters */
		stTimingParam->flc = vm->vactive - 1U;
		stTimingParam->fpw2 = stTimingParam->fpw;
		stTimingParam->flc2 = stTimingParam->flc;
	}
	return ret;
}

/*
 * Resets wmix and display device on crtc path
 */
static u32 tcc_crtc_swreset_wmix_display_device(const struct tccdrm_vioc_context *dev_context)
{
	u32 ovp = VIOC_WMIX_POR_OVP;

	/* swreset display device */
	VIOC_CONFIG_SWReset(
		dev_context->hw_data.display_device.blk_num, VIOC_CONFIG_RESET);
	VIOC_CONFIG_SWReset(
		dev_context->hw_data.display_device.blk_num, VIOC_CONFIG_CLEAR);

	if (dev_context->hw_data.wmixer.virt_addr != NULL) {
		VIOC_WMIX_GetOverlayPriority(dev_context->hw_data.wmixer.virt_addr, &ovp);
		/* Restore ovp value of WMIX to drm default ovp value */
		if (ovp == VIOC_WMIX_POR_OVP) {
			ovp = VIOC_WMIX_DRM_DEFAULT_OVP;
			DRM_DEV_INFO(dev_context->dev,
				     "[INFO] It will be sets ovp of wmix to %u\r\n", ovp);
		}
		VIOC_CONFIG_SWReset(dev_context->hw_data.wmixer.blk_num, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(dev_context->hw_data.wmixer.blk_num, VIOC_CONFIG_CLEAR);
	}
	return ovp;
}

static void tcc_crtc_set_wmix_ovp_and_size(const struct tcc_hw_device *hw_data,
			      u32 ovp, u32 hactive, u32 vactive)
{
	if (hw_data->wmixer.virt_addr != NULL) {
		VIOC_WMIX_SetOverlayPriority(hw_data->wmixer.virt_addr, ovp);
		VIOC_WMIX_SetSize(hw_data->wmixer.virt_addr, hactive, vactive);
		VIOC_WMIX_SetUpdate(hw_data->wmixer.virt_addr);
	}
}

static void tcc_crtc_set_display_device(const struct tcc_hw_device *hw_data,
				   const stLTIMING *stTimingParam,
				   const stLCDCTR *stCtrlParam, u32 hactive, u32 vactive)
{
	VIOC_DISP_SetTimingParam(
		hw_data->display_device.virt_addr, stTimingParam);
	VIOC_DISP_SetControlConfigure(
		hw_data->display_device.virt_addr, stCtrlParam);

	/* PXDW
	* YCC420 with stb pxdw is 27
	* YCC422 with stb is pxdw 21, with out stb is 8
	* YCC444 && RGB with stb is 23, with out stb is 12
	* TCCDRM can only support RGB as format of the display device.
	*/
	VIOC_DISP_SetPXDW(hw_data->display_device.virt_addr, 12);

	VIOC_DISP_SetSize(
		hw_data->display_device.virt_addr, hactive, vactive);
	VIOC_DISP_SetBGColor(hw_data->display_device.virt_addr, 0, 0, 0, 0);
}


/*
 * Reset wmix and display of crtc path except rdma.
 */
static int tccdrm_vioc_crtc_reset_display_path(const struct tccdrm_vioc_context *dev_context,
					       const struct drm_display_mode *in_mode)
{
	stLTIMING stTimingParam;
	stLCDCTR stCtrlParam;
	struct videomode vm;
	u32 ovp;
	int ret = 0;

	ret = tcc_crtc_convert_timing(dev_context,
					  in_mode, &vm, &stTimingParam,
					  &stCtrlParam);
	if (ret < 0) {
		DRM_DEV_ERROR(dev_context->dev, "input timing is invalid\r\n");
	} else {
		ovp = tcc_crtc_swreset_wmix_display_device(dev_context);
		tcc_crtc_set_wmix_ovp_and_size(&dev_context->hw_data, ovp, vm.hactive, vm.vactive);
		tcc_crtc_set_display_device(&dev_context->hw_data,
					    (const stLTIMING *)&stTimingParam,
					    (const stLCDCTR *)&stCtrlParam,
					    vm.hactive, vm.vactive);
	}
	return ret;
}

/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 2>
 */
static int tcc_drm_crtc_set_display_timing(struct tccdrm_vioc_context *dev_context,
					   const struct drm_display_mode *in_mode)
{

	bool need_reset = (bool)false;
	bool internal_ok = (bool)true;
	struct videomode vm;
	int ret = 0;

	if (dev_context->hw_data.limited_plane_only_mode) {
		DRM_DEV_INFO(dev_context->dev, "[INFO] skip because this is plane only mode\r\n");
		internal_ok = (bool)false;
	}
	if (internal_ok && (in_mode == NULL)) {
		DRM_DEV_ERROR(dev_context->dev, "in_mode is NULL\r\n");
		internal_ok = (bool)false;
		ret = -EINVAL;
	}
	if (internal_ok) {
		drm_display_mode_to_videomode(in_mode, &vm);
	}
	if (internal_ok) {
		need_reset =
			tcc_drm_crtc_check_reset_display_controller((const struct tccdrm_vioc_context *)dev_context);
	}
	if (need_reset) {
		ret = tccdrm_vioc_crtc_reset_display_path((const struct tccdrm_vioc_context *)dev_context, in_mode);

		if (ret < 0) {
			internal_ok = (bool)false;
		}
		if (internal_ok) {
			dev_context->keep_logo = 0;

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC807X)
			/* Display MUX */
			(void)VIOC_CONFIG_LCDPath_Select(get_vioc_index(dev_context->hw_data.display_device.blk_num),
							 dev_context->hw_data.lcdc_mux_select);
			DRM_DEV_INFO(
				dev_context->dev,
				"[INFO] display device(%d) to connect mux(%d)\r\n",
				get_vioc_index(dev_context->hw_data.display_device.blk_num),
				dev_context->hw_data.lcdc_mux_select);
#endif
		}
	}
	if (internal_ok) {
		/* Set pixel clocks */
		if (
			!tccdrm_vioc_crtc_check_pixelclock_match(dev_context->dev,
				&dev_context->hw_data, vm.pixelclock)) {
			ret = clk_set_rate(dev_context->hw_data.ddc_clock, vm.pixelclock);
		}
	}
	return ret;
}
#endif

#if defined(CONFIG_ARCH_TCC803X)
static int tccdrm_vioc_crtc_set_pxdw_and_swapbf(struct tccdrm_vioc_context *dev_context,
						struct tcc_crtc_state *tcc_cstate)
{
        unsigned int pxdw_bit, swapbf_bit;
	unsigned int r2ymd, r2y;


	switch (tcc_cstate->bus_format) {
	case MEDIA_BUS_FMT_YUV8_1X24:
		DRM_DEV_INFO(dev_context->dev, "YCC444\r\n");
		/* YCC444 */
		pxdw_bit = 12;
		swapbf_bit = 2;
		r2y = 1;
		break;
	case MEDIA_BUS_FMT_UYVY8_1X16:
		DRM_DEV_INFO(dev_context->dev, "YCC422\r\n");
		/* YCC422 */
		pxdw_bit = 8;
		swapbf_bit = 5;
		r2y = 1;
		break;
	case MEDIA_BUS_FMT_RGB888_1X24:
		DRM_DEV_INFO(dev_context->dev, "RGB\r\n");
		pxdw_bit = 12;
		swapbf_bit = 0;
		r2y = 0;
		break;
	default:
		DRM_DEV_INFO(dev_context->dev, "RGB\r\n");
		pxdw_bit = 12;
		swapbf_bit = 0;
		r2y = 0;
		break;
	}
	r2ymd = 2; /* Test Value */
	#if 0
	if(videoParam->mEncodingOut != RGB) {
		tcc_fb_extra_data.r2y = 1;
		switch(videoParam->mColorimetry) {
		case ITU601:
		default:
			tcc_fb_extra_data.r2ymd = 0;
			break;
		case ITU709:
			tcc_fb_extra_data.r2ymd = 2;
			break;
		case EXTENDED_COLORIMETRY:
			switch(videoParam->mExtColorimetry) {
			case XV_YCC601:
			case S_YCC601:
			case ADOBE_YCC601:
			default:
				tcc_fb_extra_data.r2ymd = 0;
				break;
			case XV_YCC709:
				tcc_fb_extra_data.r2ymd = 2;
				break;
			case BT2020YCCBCR:
			case BT2020YCBCR:
				tcc_fb_extra_data.r2ymd = 5;
				break;
			}
			break;
		}
	}
	else {
		tcc_fb_extra_data.r2y = 0;
	}
	#endif

        VIOC_DISP_SetPXDW(dev_context->hw_data.display_device.virt_addr,
			  pxdw_bit);

        #if defined(CONFIG_ARCH_TCC803X)
        VIOC_DISP_SetSwapaf(dev_context->hw_data.display_device.virt_addr,
			    swapbf_bit);
        #endif

        VIOC_DISP_SetR2YMD(dev_context->hw_data.display_device.virt_addr,
			   r2ymd);
        VIOC_DISP_SetY2R(dev_context->hw_data.display_device.virt_addr, 0);
        VIOC_DISP_SetR2Y(dev_context->hw_data.display_device.virt_addr,
			 r2y);

	return 0;
}
#endif

/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 2>
 */
/* coverity[HIS_metric] - HIS_CALLS */
static void tccdrm_vioc_crtc_set_nofb(struct drm_crtc *crtc)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_vioc_context *dev_context = to_tccdrm_vioc_context(crtc);

	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tcc_crtc_state *tcc_cstate = to_tcc_crtc_state(crtc->state);

	#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC807X)
	const struct drm_crtc_state *crtc_state =
		(const struct drm_crtc_state *)crtc->state;
	#endif
	#if defined(CONFIG_DRM_TELECHIPS_CTRL_CHROMAKEY)
	unsigned int u;
	#endif

	//DRM_DEV_INFO(dev_context->dev,
	//	     "[INFO] update connector_type = %d\r\n",
	//	     tcc_cstate->connector_type);

	#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC807X)
	#if defined(CONFIG_ARCH_TCC803X)
	if (tcc_cstate->connector_type == DRM_MODE_CONNECTOR_HDMIA) {
	#endif
	(void)tcc_drm_crtc_set_display_timing(dev_context,
					      &crtc_state->adjusted_mode);
	#if defined(CONFIG_ARCH_TCC803X)
		DRM_DEV_INFO(dev_context->dev, "set pxdw and swapbf\r\n");
		tccdrm_vioc_crtc_set_pxdw_and_swapbf(dev_context, tcc_cstate);
	}
	#endif
	#endif

	#if defined(CONFIG_DRM_TELECHIPS_CTRL_CHROMAKEY)

	if (dev_context->hw_data.wmixer.virt_addr != NULL) {
		mutex_lock(&dev_context->chromakey_mutex);
		for (u = 0U; u < 3U; u++) {
			VIOC_WMIX_SetChromaKey(dev_context->hw_data.wmixer.virt_addr, u,
				dev_context->chromakeys[u].chromakey_enable,
				dev_context->chromakeys[u].value.red,
				dev_context->chromakeys[u].value.green,
				dev_context->chromakeys[u].value.blue,
				dev_context->chromakeys[u].mask.red,
				dev_context->chromakeys[u].mask.green,
				dev_context->chromakeys[u].mask.blue);
		}
		mutex_unlock(&dev_context->chromakey_mutex);
	}
	#endif
	//DRM_DEV_INFO(dev_context->dev,
	//		"[INFO] connector_type = %d \r\n",
	//		tcc_cstate->connector_type);
	if (dev_context->hw_data.limited_plane_only_mode) {
		/* plane only */
		DRM_DEV_INFO(dev_context->dev, "[INFO] skip because this is plane only mode\r\n");
	} else {
		if (tcc_cstate->connector_type != DRM_MODE_CONNECTOR_LVDS) {
			int ret;

			if (
				test_and_set_bit(
					CRTC_FLAGS_PCLK_BIT, &dev_context->crtc_flags) != 1) {
				unsigned long lcd_rate =
					clk_get_rate(dev_context->hw_data.ddc_clock);

				if (dev_context->hw_data.keep_pclk) {
					DRM_DEV_INFO(dev_context->dev,
							"[WARN] skip enable PCLK for LVDS\r\n");
				} else {
					DRM_DEV_INFO(dev_context->dev,
						"[INFO] Enable PCLK  %ldHz\r\n",
						lcd_rate);
					ret = clk_prepare_enable(dev_context->hw_data.ddc_clock);
					if  (ret < 0) {
						DRM_DEV_INFO(
							dev_context->dev,
							"[WARN] It failed to enable the lcd clk\r\n");
					}
				}
			}
		}
		(void)tccdrm_vioc_crtc_update_pixel_clock(dev_context, tcc_cstate);
	}
	#if defined(CONFIG_ARCH_TCC803X)
	if (tcc_cstate->connector_type == DRM_MODE_CONNECTOR_HDMIA) {
		void __iomem *ddi_config = VIOC_DDICONFIG_GetAddress();

		// from pclk.
		VIOC_DDICONFIG_SetPeriClock(ddi_config,
					    get_vioc_index(dev_context->hw_data.display_device.blk_num), 0);
	}
	#endif

}

/* crtc helper funcs --------------------------------------------------------*/
static const struct drm_crtc_helper_funcs tccdrm_vioc_crtc_helper_funcs = {
	.mode_set_nofb = tccdrm_vioc_crtc_set_nofb,
	.atomic_check	= tccdrm_vioc_crtc_atomic_check,
	.atomic_begin	= tccdrm_vioc_crtc_atomic_begin,
	.atomic_flush	= tccdrm_vioc_crtc_atomic_flush,
	.atomic_enable	= tcc_drm_crtc_atomic_enable,
	.atomic_disable	= tcc_drm_crtc_atomic_disable,
};

static void tcc_drm_crtc_destroy(struct drm_crtc *crtc)
{
	drm_crtc_cleanup(crtc);
}

static irqreturn_t lcd_vtimer_irq_handler(int irq, void *dev_id)
{
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_11_5] */
	struct tcc_timer *timer = (struct tcc_timer *)dev_id;
	struct tccdrm_vioc_context *dev_context;
	bool internal_ok = (bool)true;
	irqreturn_t ret = IRQ_HANDLED;

	(void)irq;

	if (timer == NULL) {
		DRM_DEV_ERROR(NULL, "timer is NULL\r\n");
		 internal_ok = (bool)false;
	}
	if (internal_ok) {
		if (timer->dev == NULL) {
			DRM_DEV_ERROR(NULL, "timer->dev is NULL\r\n");
			internal_ok = (bool)false;
		}
	}

	if (internal_ok) {
		/* coverity[misra_c_2012_rule_11_3] */
		/* coverity[misra_c_2012_rule_11_5] */
		dev_context = (struct tccdrm_vioc_context *)timer->dev;

		/* check the crtc is detached already from encoder */
		/* coverity[cert_exp39_c] */
		if (!dev_context->dev_binded) {
			DRM_DEV_ERROR(dev_context->dev,
				"[WARN] crtc was not bound yet.\r\n");
		} else {
			if (test_bit(CRTC_FLAGS_IRQ_BIT, &dev_context->crtc_flags) == 1) {
				tccdrm_crtc_vblank_handler(&dev_context->crtc,
							&dev_context->flip_state);
			}
		}
	}

	return ret;
}

static int tccvioc_enable_irq_of_display_dev(struct tccdrm_vioc_context *dev_context,
					     int irq_num, int vioc_intr_inum)
{
	unsigned long irqflags;

	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_14_4] */
	spin_lock_irqsave(&dev_context->irq_lock, irqflags);
	if (test_and_set_bit(CRTC_FLAGS_IRQ_BIT,
			     &dev_context->crtc_flags) != 1) {
		(void)vioc_intr_clear(
			vioc_intr_inum,
			VIOC_DISP_INTR_DISPLAY);
		#if defined(CONFIG_SMP)
		(void)irq_set_affinity_hint(
			dev_context->hw_data.display_device.irq_num,
			dev_context->irq_cpumask);
		#endif
		(void)vioc_intr_enable(irq_num, vioc_intr_inum,
					VIOC_DISP_INTR_DISPLAY);
		//DRM_DEV_DEBUG(dev_context->dev,
		//		"enable interrupt for display device irq(%d), blk(%d)\r\n",
		//		irq_num, vioc_intr_inum);
	}
	spin_unlock_irqrestore(&dev_context->irq_lock, irqflags);

	return 0;
}

static int tccvioc_disable_irq_of_display_dev(struct tccdrm_vioc_context *dev_context,
					      int irq_num, int vioc_intr_inum)
{
	unsigned long irqflags;

	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_14_4] */
	spin_lock_irqsave(&dev_context->irq_lock, irqflags);
	if (test_and_clear_bit(CRTC_FLAGS_IRQ_BIT,
			       &dev_context->crtc_flags) == 1) {
		DRM_DEV_DEBUG(dev_context->dev,
				"disable interrupt for display device irq(%d), blk(%d)\r\n",
				irq_num, vioc_intr_inum);
		(void)vioc_intr_disable(irq_num,
					vioc_intr_inum,
					VIOC_DISP_INTR_DISPLAY);
		(void)vioc_intr_clear(vioc_intr_inum,
					VIOC_DISP_INTR_DISPLAY);
	}
	spin_unlock_irqrestore(&dev_context->irq_lock, irqflags);

	return 0;
}


static int tcc_drm_crtc_enable_display_vblank(struct tccdrm_vioc_context *dev_context)
{
	bool internal_ok = (bool)true;
	unsigned int vioc_intr_unum;
	int vioc_intr_inum, irq_num;
	int ret = 0;

	vioc_intr_unum = dev_context->hw_data.display_device.intr_num;
	if (tcc_math_uint_gt_intmax(vioc_intr_unum)) {
		DRM_DEV_ERROR(dev_context->dev, "display device intr_num is out of range\r\n");
		internal_ok = (bool)false;
		ret = -EINVAL;
	} else {
		vioc_intr_inum = (int)vioc_intr_unum;
	}

	if (internal_ok) {
		if (tcc_math_uint_gt_intmax(dev_context->hw_data.display_device.irq_num)) {
			DRM_DEV_ERROR(dev_context->dev, "display device irq number is out of range\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		} else {
			irq_num = (int)dev_context->hw_data.display_device.irq_num;
		}
	}
	if (internal_ok) {
		ret = tccvioc_enable_irq_of_display_dev(dev_context,
							irq_num, vioc_intr_inum);
	}
	return ret;
}

static int tcc_drm_crtc_enable_timer(struct tccdrm_vioc_context *dev_context, u32 usec)
{
	int ret = 0;
	int err = 0;

	if (test_and_set_bit(CRTC_FLAGS_IRQ_BIT,
		 &dev_context->crtc_flags) != 1) {
		/* coverity[misra_c_2012_rule_11_5] */
		dev_context->vtimer =
			tcc_register_timer((struct device *)dev_context,
					   usec, lcd_vtimer_irq_handler);
		if (IS_ERR(dev_context->vtimer)) {
			ret = -ENODEV;
			DRM_DEV_ERROR(dev_context->dev,
				"failed to register vtimer using tcc_register_timer\r\n");
		} else {
			DRM_DEV_INFO(dev_context->dev, "Enable vblank timer\r\n");
			err = tcc_timer_enable(dev_context->vtimer);
			if(err < 0) {
				ret = -ENODEV;
				DRM_DEV_ERROR(dev_context->dev,
					"failed to tcc timer enable\r\n");
			}
		}
	}

	return ret;
}


static int tcc_drm_crtc_enable_timer_vblank(struct tccdrm_vioc_context *dev_context)
{
	bool internal_ok = (bool)true;
	unsigned long irqflags;
	int ret = 0;
	u32 usec;

	if (IS_ERR(dev_context->vtimer)) {
		if (!tcc_math_check_uint_plus_uint(1000000U - 1U, dev_context->vrefresh)) {
			DRM_DEV_ERROR(dev_context->dev, "out of range\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		}

		if (internal_ok) {
			/* coverity[misra_c_2012_rule_10_4] */
			usec = DIV_ROUND_UP(1000000U, dev_context->vrefresh);

			DRM_DEV_INFO(dev_context->dev, "refresh = %uHz usec = %uus\r\n",
				dev_context->vrefresh, usec);

			/* coverity[cert_dcl37_c] */
			/* coverity[misra_c_2012_rule_14_4] */
			spin_lock_irqsave(&dev_context->irq_lock, irqflags);

			ret = tcc_drm_crtc_enable_timer(dev_context, usec);

			spin_unlock_irqrestore(&dev_context->irq_lock, irqflags);
		}
	}
	return ret;
}

static int tcc_drm_crtc_enable_vblank(struct drm_crtc *crtc)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_vioc_context *dev_context =
			(struct tccdrm_vioc_context *)to_tccdrm_vioc_context(crtc);
	int ret = 0;

	if (dev_context->hw_data.limited_plane_only_mode) {
		/* plane only */
		ret = tcc_drm_crtc_enable_timer_vblank(dev_context);
	} else {
		/* no limitations */
		ret = tcc_drm_crtc_enable_display_vblank(dev_context);
	}

	return ret;
}

static void tcc_drm_crtc_disable_display_vblank(struct tccdrm_vioc_context *dev_context)
{
	bool internal_ok = (bool)true;
	unsigned int vioc_intr_unum;
	int vioc_intr_inum, irq_num;

	vioc_intr_unum = dev_context->hw_data.display_device.intr_num;
	if (tcc_math_uint_gt_intmax(vioc_intr_unum)) {
		DRM_DEV_ERROR(dev_context->dev, "display device intr_num is out of range\r\n");
		internal_ok = (bool)false;
	} else {
		vioc_intr_inum = (int)vioc_intr_unum;
	}
	if (internal_ok) {

		if (tcc_math_uint_gt_intmax(dev_context->hw_data.display_device.irq_num)) {
			DRM_DEV_ERROR(dev_context->dev, "display device irq number is out of range\r\n");
			internal_ok = (bool)false;
		} else {
			irq_num = (int)dev_context->hw_data.display_device.irq_num;
		}
	}
	if (internal_ok) {
		(void)tccvioc_disable_irq_of_display_dev(dev_context,
							     irq_num,
							     vioc_intr_inum);
	}
}

static void tcc_drm_crtc_disable_vtimer(struct tccdrm_vioc_context *dev_context)
{
	int err = 0;

	if (test_and_clear_bit(CRTC_FLAGS_IRQ_BIT,
			&dev_context->crtc_flags) == 1) {
		err = tcc_timer_disable(dev_context->vtimer);
		if(err < 0) {
			DRM_DEV_ERROR(dev_context->dev,
				"failed to tcc timer disable\r\n");
		}

		tcc_unregister_timer(dev_context->vtimer);

		/* coverity[misra_c_2012_rule_11_5] */
		dev_context->vtimer = ERR_PTR(-ENODEV);
	}
}

static void tcc_drm_crtc_disable_vtimer_vblank(struct tccdrm_vioc_context *dev_context)
{
	unsigned long irqflags;

	if (IS_ERR(dev_context->vtimer)) {
		/* Not registered */
		DRM_DEV_INFO(dev_context->dev, "vblank timer was not registred\r\n");
	} else {
		/* coverity[cert_dcl37_c] */
		/* coverity[misra_c_2012_rule_14_4] */
		spin_lock_irqsave(&dev_context->irq_lock, irqflags);

		tcc_drm_crtc_disable_vtimer(dev_context);

		spin_unlock_irqrestore(&dev_context->irq_lock, irqflags);
	}
}

static void tcc_drm_crtc_disable_vblank(struct drm_crtc *crtc)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_vioc_context *dev_context =
			(struct tccdrm_vioc_context *)to_tccdrm_vioc_context(crtc);

	if (dev_context->hw_data.limited_plane_only_mode) {
		/* plane only */
		tcc_drm_crtc_disable_vtimer_vblank(dev_context);
	} else {
		/* no limitations */
		tcc_drm_crtc_disable_display_vblank(dev_context);
	}
}

#if defined(CONFIG_DRM_TELECHIPS_CTRL_CHROMAKEY)
/* coverity[misra_c_2012_rule_8_13] */
static int lcd_set_chromakey(struct drm_crtc *crtc,
	unsigned int chromakey_layer, unsigned int chromakey_enable,
	const struct drm_chromakey_t *value, const struct drm_chromakey_t *mask)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_vioc_context *dev_context =
			(struct tccdrm_vioc_context *)to_tccdrm_vioc_context(crtc);
	int ret = 0;

	if (dev_context->hw_data.wmixer.virt_addr == NULL) {
		ret = -1;
	} else {
		mutex_lock(&dev_context->chromakey_mutex);
		if (dev_context->crtc_enabled) {
			VIOC_WMIX_SetChromaKey(
				dev_context->hw_data.wmixer.virt_addr, chromakey_layer,
				chromakey_enable,
				value->red, value->green, value->blue,
				mask->red, mask->green, mask->blue);
			VIOC_WMIX_SetUpdate(dev_context->hw_data.wmixer.virt_addr);
		}
		/* store information */
		dev_context->chromakeys[
			chromakey_layer].chromakey_enable = chromakey_enable;
		(void)memcpy(
			&dev_context->chromakeys[chromakey_layer].value,
			value, sizeof(*value));
		(void)memcpy(
			&dev_context->chromakeys[chromakey_layer].mask,
			mask, sizeof(*mask));
		mutex_unlock(&dev_context->chromakey_mutex);
	}
	return ret;
}

static int lcd_get_chromakey(struct drm_crtc *crtc,
	unsigned int chromakey_layer, unsigned int *chromakey_enable,
	struct drm_chromakey_t *value, struct drm_chromakey_t *mask)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct tccdrm_vioc_context *dev_context =
			(struct tccdrm_vioc_context *)to_tccdrm_vioc_context(crtc);
	int ret = -1;

	if (dev_context->hw_data.wmixer.virt_addr != NULL) {
		mutex_lock(&dev_context->chromakey_mutex);
		if (dev_context->crtc_enabled) {
			VIOC_WMIX_GetChromaKey(
				dev_context->hw_data.wmixer.virt_addr, chromakey_layer,
				chromakey_enable, &value->red,
				&value->green, &value->blue,
				&mask->red, &mask->green,
				&mask->blue);
			ret = 0;
		} else {
			*chromakey_enable =
			dev_context->chromakeys[chromakey_layer].chromakey_enable;
			(void)memcpy(
				value, &dev_context->chromakeys[chromakey_layer].value,
				sizeof(*value));
			(void)memcpy(
				mask, &dev_context->chromakeys[chromakey_layer].mask,
				sizeof(*mask));
		}
		mutex_unlock(&dev_context->chromakey_mutex);
	}
	return ret;
}
#endif
static struct drm_crtc_state *tccdrm_vioc_crtc_duplicate_state(struct drm_crtc *crtc)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	const struct tccdrm_vioc_context *dev_context =
			(const struct tccdrm_vioc_context *)to_tccdrm_vioc_context(crtc);
	struct tcc_crtc_state *tcc_cstate;
	struct drm_crtc_state *crtc_state = NULL;
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_10_8] */ //K5.4
	tcc_cstate = kzalloc(sizeof(*tcc_cstate), GFP_KERNEL);
	if (tcc_cstate != NULL) {
		__drm_atomic_helper_crtc_duplicate_state(crtc, &tcc_cstate->base);
		crtc_state = &tcc_cstate->base;
		#if defined(CONFIG_DRM_TELECHIPS_CTRL_CHROMAKEY)
		tcc_cstate->set_chromakey = lcd_set_chromakey;
		tcc_cstate->get_chromakey = lcd_get_chromakey;
		#endif
		tcc_cstate->dev = dev_context->dev;
	}

	return crtc_state;
}

static void tcc_drm_crtc_reset(struct drm_crtc *crtc)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	const struct tccdrm_vioc_context *dev_context =
			(const struct tccdrm_vioc_context *)to_tccdrm_vioc_context(crtc);
	struct tcc_crtc_state *tcc_cstate;

	if (crtc->state != NULL) {
		tccdrm_crtc_destroy_state(crtc, crtc->state);
	}
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_10_8] */ //K5.4
	tcc_cstate = kzalloc(sizeof(*tcc_cstate), GFP_KERNEL);
	if (tcc_cstate != NULL) {
	        __drm_atomic_helper_crtc_reset(crtc, &tcc_cstate->base);
		#if defined(CONFIG_DRM_TELECHIPS_CTRL_CHROMAKEY)
		tcc_cstate->set_chromakey = lcd_set_chromakey;
		tcc_cstate->get_chromakey = lcd_get_chromakey;
		#endif
		tcc_cstate->dev = dev_context->dev;

		//tcc_cstate->bus_format = ;
		//tcc_cstate->enc_out_encoding = ;
	}
}

static const struct drm_crtc_funcs tccdrm_vioc_crtc_funcs = {
	.set_config = drm_atomic_helper_set_config,
	.page_flip = drm_atomic_helper_page_flip,
	.destroy = tcc_drm_crtc_destroy,
	.reset = tcc_drm_crtc_reset,
	.atomic_duplicate_state = tccdrm_vioc_crtc_duplicate_state,
	.atomic_destroy_state = tccdrm_crtc_destroy_state,
	.enable_vblank = tcc_drm_crtc_enable_vblank,
	.disable_vblank = tcc_drm_crtc_disable_vblank,
};

/* FUNCTIONS for LCDC --------------------------------------------------------*/
/*
 * LCD stands for Fully Interactive Various Display and
 * as a display controller, it transfers contents drawn on memory
 * to a LCD Panel through Display Interfaces such as RGB or
 * CPU Interface.
 */
static const uint32_t lcd_formats[] = {
	DRM_FORMAT_BGR565,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_BGR888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888,
};

static const uint32_t lcd_formats_vrdma[] = {
	DRM_FORMAT_BGR565,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_BGR888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_NV12,
	DRM_FORMAT_NV21,
	DRM_FORMAT_YUV420,
	DRM_FORMAT_YVU420,
};

static void lcd_ru_handler(struct tccdrm_vioc_context *dev_context, int vioc_intr_inum)
{
	(void)vioc_intr_clear(
		vioc_intr_inum,
		((unsigned int)1U << VIOC_DISP_INTR_RU));

	/* check the crtc is detached already from encoder */
	if (!dev_context->dev_binded) {
		DRM_DEV_DEBUG(dev_context->dev,
			      "[WARN] crtc was not bound yet.\r\n");
	} else {
		if (test_bit(CRTC_FLAGS_IRQ_BIT, &dev_context->crtc_flags) == 1) {
			tccdrm_crtc_vblank_handler(&dev_context->crtc,
						   &dev_context->flip_state);
		}
	}
}

static void lcd_fu_handler(struct tccdrm_vioc_context *dev_context, int vioc_intr_inum)
{
	ktime_t time_underrun_isr;
	s64 ms_delta;

	(void)vioc_intr_clear(
		vioc_intr_inum,
		((unsigned int)1U << VIOC_DISP_INTR_FU));
	if (
		VIOC_DISP_Get_TurnOnOff(
			dev_context->hw_data.display_device.virt_addr) == 1U) {

		time_underrun_isr = ktime_get();
 		ms_delta = ktime_ms_delta(time_underrun_isr, dev_context->time_underrun_start);

		if ((!dev_context->fifo_underrun) || (ms_delta >= (s64)1000)) {
			DRM_DEV_ERROR(dev_context->dev, "FIFO UNDERRUN\r\n");
			/* Update time */
			dev_context->time_underrun_start = ktime_get();
		}
		dev_context->fifo_underrun = (bool)true;
	}
}

static void lcd_etc_handler(struct tccdrm_vioc_context *dev_context, int vioc_intr_inum,
			    u32 dispblock_status)
{
	if ((dispblock_status &
		((unsigned int)1U << VIOC_DISP_INTR_DD)) != 0U) {
		/* coverity[cert_dcl37_c] */
		/* coverity[cert_pre31_c] */
		/* coverity[misra_c_2012_rule_10_3] */
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_15_6] */
		/* coverity[misra_c_2012_rule_21_2] */
		if (atomic_read(&dev_context->wait_display_done_event)) {
			/* coverity[cert_dcl37_c] */
			/* coverity[misra_c_2012_rule_10_3] */
			/* coverity[misra_c_2012_rule_21_2] */
			atomic_set(&dev_context->wait_display_done_event, 0);
			/* coverity[misra_c_2012_rule_10_1] */
			wake_up_all(&dev_context->wait_display_done_queue);
		}
		(void)vioc_intr_clear(
			vioc_intr_inum,
			((unsigned int)1U << VIOC_DISP_INTR_DD));
	}

	if ((dispblock_status &
		((unsigned int)1U << VIOC_DISP_INTR_SREQ)) != 0U) {
		(void)vioc_intr_clear(
			vioc_intr_inum,
			((unsigned int)1U << VIOC_DISP_INTR_SREQ));
	}
}

static irqreturn_t lcd_irq_handler(int irq, void *dev_id)
{
	unsigned int vioc_intr_unum;
	int vioc_intr_inum, blk_num;

	/* coverity[misra_c_2012_rule_11_5] */
	struct tccdrm_vioc_context *dev_context = (struct tccdrm_vioc_context *)dev_id;
	bool internal_ok = (bool)true;
	u32 dispblock_status = 0;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter irq is not used in
	 * the function.
	 */
	(void)irq;

	if (dev_context == NULL) {
		DRM_DEV_INFO(NULL, "[WARN]dev_context is NULL\r\n");
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		if (tcc_math_uint_gt_intmax(dev_context->hw_data.display_device.blk_num)) {
			DRM_DEV_ERROR(dev_context->dev, "display device blk number is out of range\r\n");
			internal_ok = (bool)false;
		} else {
			blk_num = (int)dev_context->hw_data.display_device.blk_num;
		}
	}
	if (internal_ok) {
		vioc_intr_unum = dev_context->hw_data.display_device.intr_num;
		if (tcc_math_uint_gt_intmax(vioc_intr_unum)) {
			DRM_DEV_ERROR(dev_context->dev, "display device intr_num is out of range\r\n");
			internal_ok = (bool)false;
		} else {
			vioc_intr_inum = (int)vioc_intr_unum;
		}
	}
	if (internal_ok) {
		if (
			is_vioc_display_device_intr_masked(
				blk_num,
				VIOC_DISP_INTR_DISPLAY)) {
			DRM_DEV_DEBUG(dev_context->dev,
					"[DEBUG]interrupt was not enabled\r\n");
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		/* Get TCC VIOC block status register */
		dispblock_status = vioc_intr_get_status(vioc_intr_inum);
	}
	if (internal_ok) {
		if ((dispblock_status &
		    ((unsigned int)1U << VIOC_DISP_INTR_RU)) != 0U) {
			lcd_ru_handler(dev_context, vioc_intr_inum);
		}
	}
	if (internal_ok) {
		/* Check FIFO underrun. */
		if ((dispblock_status &
		    ((unsigned int)1U << VIOC_DISP_INTR_FU)) != 0U)  {
			lcd_fu_handler(dev_context, vioc_intr_inum);
		} else {
			if (dev_context->fifo_underrun) {
				DRM_DEV_INFO(dev_context->dev, "Reset fifo underrun state\r\n");
				dev_context->fifo_underrun = (bool)false;
			}
		}
	}
	if (internal_ok) {
		lcd_etc_handler(dev_context, vioc_intr_inum, dispblock_status);
	}

	return IRQ_HANDLED;
}

static int tccdrm_vioc_bind_init_variables(struct tccdrm_vioc_context *dev_context)
{
	int ret = 0;

	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_17_7] */
	spin_lock_init(&dev_context->irq_lock);

	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_14_4] */
	init_waitqueue_head(&dev_context->wait_display_done_queue);
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_21_2] */
	atomic_set(&dev_context->wait_display_done_event, 0);

	#if defined(CONFIG_DRM_TELECHIPS_CTRL_CHROMAKEY)
	/* coverity[misra_c_2012_rule_14_4] */
	mutex_init(&dev_context->chromakey_mutex);
	#endif

	#if defined(CONFIG_DRM_TELECHIPS_KEEP_LOGO)
	#if defined(CONFIG_DRM_FBDEV_EMULATION)
	/* If fbdev emulation was selected then FB core calls set_par */
	dev_context->keep_logo = 1;

	#if defined(CONFIG_LOGO) || defined(CONFIG_ARCH_TCC807X)
	/* If logo was selected then FB core calls set_par twice */
	dev_context->keep_logo++;
	#endif // CONFIG_LOGO
	#endif // CONFIG_DRM_FBDEV_EMULATION
	#endif // CONFIG_DRM_TELECHIPS_KEEP_LOGO

	if (dev_context->hw_data.limited_plane_only_mode) {
		/* plane only */
		dev_context->keep_logo = 0;
	}

	return ret;
}

static struct tccdrm_vioc_context *tccdrm_vioc_bind_create_context(struct device *dev,
						   const struct device *master_dev)
{
	/* coverity[misra_c_2012_rule_11_5] */
	struct drm_device *drm = (struct drm_device *)dev_get_drvdata(master_dev);
	struct tccdrm_vioc_context *dev_context = NULL;
	bool internal_ok = (bool)true;

	if (dev->of_node == NULL) {
		DRM_DEV_ERROR(
			dev,
			"failed to get the device node\n");
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		/* Create device context */
		/* coverity[misra_c_2012_rule_11_5] */
		dev_context = devm_kzalloc(dev, sizeof(*dev_context),
					   /* coverity[misra_c_2012_rule_10_8] */ //K5.4
					   GFP_KERNEL);
		if (dev_context == NULL) {
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		dev_context->dev = dev;
		dev_context->drm = drm;
		/* coverity[misra_c_2012_rule_11_5] */
		dev_context->vtimer = ERR_PTR(-ENODEV);
	}

	return dev_context;
}

static int tccdrm_vioc_bind_parse_context(struct tccdrm_vioc_context *dev_context)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */	//
	/* coverity[misra_c_2012_rule_8_6] */	//
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	struct platform_device *plat_dev = to_platform_device(dev_context->dev);
	#if defined(CONFIG_SMP)
	unsigned int u, irq_cpumask;
	#endif
	int ret = 0;

	platform_set_drvdata(plat_dev, dev_context);

	#if defined(CONFIG_SMP)
	/* coverity[cert_int02_c] */
	/* coverity[cert_int31_c] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_12_1] */
	/* coverity[misra_c_2012_rule_13_4] */
	/* coverity[misra_c_2012_rule_14_2] */
	for_each_online_cpu(u) {
		irq_cpumask = u;
	}
	dev_context->irq_cpumask = cpumask_of(irq_cpumask);
	DRM_DEV_INFO(dev_context->dev,
		     "[INFO] crtc interrupts will be handled by cpu (%u).\r\n",
		     irq_cpumask);
	#endif

	ret = tccdrm_vioc_dt_parse(plat_dev, &dev_context->hw_data);
	if (ret < 0) {
		DRM_DEV_ERROR(dev_context->dev, "failed to parse device tree\n");
	}

	return ret;
}

static int tccdrm_vioc_bind_init_interrupts(struct tccdrm_vioc_context *dev_context)
{
	bool internal_ok = (bool)true;
	unsigned int vioc_intr_unum;
	int irq_num, vioc_intr_inum;
	int ret = 0;

	if (dev_context->hw_data.limited_plane_only_mode) {
		/* plane only */
		DRM_DEV_INFO(dev_context->dev, "[INFO] skip because this is plane only mode\r\n");
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		if (tcc_math_uint_gt_intmax(dev_context->hw_data.display_device.blk_num)) {
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		if (tcc_math_uint_gt_intmax(dev_context->hw_data.display_device.irq_num)) {
			internal_ok = (bool)false;
			ret = -EINVAL;
		} else {
			irq_num = (int)dev_context->hw_data.display_device.irq_num;
		}
	}
	if (internal_ok) {
		vioc_intr_unum = dev_context->hw_data.display_device.intr_num;
		if (tcc_math_uint_gt_intmax(vioc_intr_unum)) {
			DRM_DEV_ERROR(dev_context->dev, "display device intr_num is out of range\r\n");
			internal_ok = (bool)false;
			ret = -EINVAL;
		} else {
			vioc_intr_inum = (int)vioc_intr_unum;
		}
	}
	if (internal_ok) {
		/* Disable & clean interrupt */
		(void)vioc_intr_disable(irq_num,
					vioc_intr_inum,
					VIOC_DISP_INTR_DISPLAY);
		(void)vioc_intr_clear(vioc_intr_inum, VIOC_DISP_INTR_DISPLAY);

		#if defined(CONFIG_SMP)
		(void)irq_set_affinity_hint(dev_context->hw_data.display_device.irq_num,
				       dev_context->irq_cpumask);
		#endif
		ret = devm_request_irq(
			dev_context->dev,
			dev_context->hw_data.display_device.irq_num,
			lcd_irq_handler, IRQF_SHARED, dev_name(dev_context->dev),
			dev_context);
		if (ret < 0) {
			DRM_DEV_ERROR(dev_context->dev,
				      "failed to request irq\r\n");
		}
	}
	return ret;
}

static int lcd_bin_init_clocks(struct tccdrm_vioc_context *dev_context)
{
	bool internal_ok = (bool)true;
	int ret = 0;

	/* Activate CRTC clocks */
	if (test_and_set_bit(CRTC_FLAGS_VCLK_BIT,
			     &dev_context->crtc_flags) != 1) {
		DRM_DEV_INFO(dev_context->dev,
			     "[INFO] enable vioc_clock\r\n");
		ret = clk_prepare_enable(dev_context->hw_data.vioc_clock);
		if (ret < 0) {
			DRM_DEV_ERROR(dev_context->dev,
				"failed to enable the bus clk\n");
			internal_ok = (bool)false;
		}
	}
	if (dev_context->hw_data.limited_plane_only_mode) {
		/* plane only */
		DRM_DEV_INFO(dev_context->dev, "[INFO] skip because this is plane only mode\r\n");
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		if (test_and_set_bit(CRTC_FLAGS_PCLK_BIT,
				     &dev_context->crtc_flags) != 1) {
			if (dev_context->hw_data.keep_pclk) {
				DRM_DEV_INFO(dev_context->dev,
					     "[WARN] skip enable PCLK for LVDS\r\n");
			} else {
				DRM_DEV_INFO(dev_context->dev,
					"[INFO] enable ddc_clock\r\n");
				ret = clk_prepare_enable(dev_context->hw_data.ddc_clock);
				if (ret < 0) {
					DRM_DEV_ERROR(dev_context->dev,
						"failed to enable the ddc clk\n");
				}
			}
		}
	}
	return ret;
}

static int tccdrm_vioc_bind_init_planes_and_crtcs(struct tccdrm_vioc_context *dev_context)
{
	struct tccdrm_universal_plane_data universal_plane_data;
	enum drm_plane_type plane_type;
	struct drm_plane *primary = NULL;
	struct drm_plane *cursor = NULL;
	bool internal_ok = (bool)true;
	unsigned int formats_list_size;
	const uint32_t *formats_list;
	unsigned int u, utmp;
	int ret = 0;

	if (dev_context->hw_data.rdma_counts <= 0) {
		DRM_DEV_ERROR(dev_context->dev,
				"%s have invalid rdma_count %d\r\n",
				dev_name(dev_context->dev),
				dev_context->hw_data.rdma_counts);
		internal_ok = (bool)false;
		ret = -ENODEV;
	}
	if (internal_ok) {
		for (u = 0U; u < (unsigned int)dev_context->hw_data.rdma_counts; u++) {
			/* initizlise universal_plane_data */
			(void)memset(&universal_plane_data, 0,
			       sizeof(universal_plane_data));
			#if defined(CONFIG_VIOC_PVRIC_FBDC)
			if (dev_context->hw_data.fbdc[u].virt_addr != NULL) {
				set_bit(TCC_DRM_PLANE_CAP_FBDC,
					&dev_context->planes[u].caps);
				universal_plane_data.format_modifiers =
					tcc_povervr_rougue_plane_format_modifiers;
			}
			#endif
			if (VIOC_RDMA_IsVRDMA(dev_context->hw_data.rdma[u].blk_num) != 0) {
				formats_list = lcd_formats_vrdma;
				/* coverity[misra_c_2012_rule_6_1] */
				/* coverity[misra_c_2012_rule_10_3] */
				/* coverity[misra_c_2012_rule_10_4] */
				/* coverity[misra_c_2012_rule_12_1] */
				formats_list_size = ARRAY_SIZE(lcd_formats_vrdma);
			} else {
				formats_list = lcd_formats;
				/* coverity[misra_c_2012_rule_6_1] */
				/* coverity[misra_c_2012_rule_10_3] */
				/* coverity[misra_c_2012_rule_10_4] */
				/* coverity[misra_c_2012_rule_12_1] */
				formats_list_size = ARRAY_SIZE(lcd_formats);
			}
			utmp = DRM_PLANE_TYPE(dev_context->hw_data.rdma_plane_type[u]);
			if (utmp > (unsigned int)DRM_PLANE_TYPE_CURSOR) {
				internal_ok = (bool)false;
				ret = -EINVAL;
			} else {
				plane_type = (enum drm_plane_type )utmp;
			}
			if (internal_ok) {
				universal_plane_data.plane =
					&dev_context->planes[u].base;
				universal_plane_data.plane_type = plane_type;
				universal_plane_data.pixel_formats = formats_list;
				universal_plane_data.num_pixel_formats =
					formats_list_size;
				universal_plane_data.plane_funcs =
					&tccdrm_vioc_plane_funcs;
				universal_plane_data.plane_helper_funcs =
					&tccdrm_vioc_plane_helper_funcs;

				ret = tcc_prepare_universal_plane(
					dev_context->drm,
					&universal_plane_data);
				if (ret != 0) {
					DRM_DEV_ERROR(
						dev_context->dev,
						"failed to initizliaed the planes\n");
					internal_ok = (bool)false;
				}
			}
			if (!internal_ok) {
				break;
			}
			dev_context->planes[u].win = u;
			if (
				DRM_PLANE_TYPE(dev_context->hw_data.rdma_plane_type[u]) ==
				(unsigned int)DRM_PLANE_TYPE_PRIMARY) {
				primary = &dev_context->planes[u].base;
			}
			if (
				DRM_PLANE_TYPE(dev_context->hw_data.rdma_plane_type[u]) ==
				(unsigned int)DRM_PLANE_TYPE_CURSOR) {
				cursor = &dev_context->planes[u].base;
			}
		}
	}
	if (internal_ok) {
		struct tccdrm_crtc_create_data crtc_create_data = {
			.crtc = &dev_context->crtc,
			.crtc_name = NULL,
			.primary = primary,
			.cursor = cursor,
			.crtc_funcs = &tccdrm_vioc_crtc_funcs,
			.crtc_helper_funcs = &tccdrm_vioc_crtc_helper_funcs
		};

		ret = tccdrm_crtc_create((const struct device *)dev_context->dev,
					 dev_context->drm,
					 (const struct tccdrm_crtc_create_data *)&crtc_create_data);
		if (ret == 0) {
			/* coverity[cert_dcl37_c] */
			/* coverity[misra_c_2012_rule_10_3] */
			/* coverity[misra_c_2012_rule_21_2] */
			atomic_set(&dev_context->flip_state.flipstatus,
				TCC_DRM_CRTC_FLIP_STATUS_NONE);
		}
	}
	return ret;
}

static int tccdrm_vioc_bind_init_phase_0(struct tccdrm_vioc_context *dev_context)
{
	bool internal_ok = (bool)true;
	int ret = 0;

	ret = tccdrm_vioc_bind_init_variables(dev_context);
	if (ret < 0) {
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		ret = lcd_bin_init_clocks(dev_context);
		if (ret < 0) {
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		ret = tccdrm_vioc_bind_init_interrupts(dev_context);
	}
	return ret;
}

static int tccdrm_vioc_bind_init_phase_1(struct tccdrm_vioc_context *dev_context)
{
	struct device *dev = dev_context->dev;
	bool internal_ok = (bool)true;
	int ret = 0;

	ret = tccdrm_vioc_bind_init_planes_and_crtcs(dev_context);
	if (ret < 0) {
		DRM_DEV_ERROR(dev, "failedinit planes and crtcs\r\n");
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		#if defined(CONFIG_PM)
		pm_runtime_enable(dev);
		#endif
	}

	return ret;
}

/* coverity[misra_c_2012_rule_8_13] */
static int tccdrm_vioc_bind(struct device *dev, struct device *master_dev,
		    void *data)
{
	struct tccdrm_vioc_context *dev_context = NULL;
	bool free_context = (bool)false;
	bool internal_ok = (bool)true;
	int ret = 0;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter data is not
	 * used in the function.
	 */
	(void)data;

	dev_context = tccdrm_vioc_bind_create_context(dev, (const struct device *)master_dev);
	if (dev_context == NULL) {
		internal_ok = (bool)false;
		ret = -ENOMEM;
	} else {
		free_context = (bool)true;
	}
	if (internal_ok) {
		ret = tccdrm_vioc_bind_parse_context(dev_context);
		if (ret < 0) {
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		ret = tccdrm_vioc_bind_init_phase_0(dev_context);
		if (ret < 0) {
			internal_ok = (bool)false;
		}
	}

	if (internal_ok) {
		ret = tccdrm_vioc_bind_init_phase_1(dev_context);
		if (ret < 0) {
			internal_ok = (bool)false;
		}
	}

	if (internal_ok) {
		set_bit(CRTC_FLAGS_TIMING_CHECK_BIT, &dev_context->crtc_flags);
		dev_context->dev_binded = (bool)true;
	} else {
		if (free_context) {
			devm_kfree(dev, dev_context);
			dev_context = NULL;
		}
	}
	return ret;
}

/* coverity[misra_c_2012_rule_8_13] */
static bool tccdrm_vioc_unbind_phase_0(struct tccdrm_vioc_context *dev_context)
{
	bool internal_ok = (bool)true;
	unsigned int vioc_intr_unum;
	int irq_num, vioc_intr_inum;

	if (dev_context == NULL) {
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		if (tcc_math_uint_gt_intmax(dev_context->hw_data.display_device.irq_num)) {
			internal_ok = (bool)false;
		} else {
			irq_num = (int)dev_context->hw_data.display_device.irq_num;
		}
	}
	if (internal_ok) {
		if (tcc_math_uint_gt_intmax(dev_context->hw_data.display_device.blk_num)) {
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		vioc_intr_unum = dev_context->hw_data.display_device.intr_num;
		if (tcc_math_uint_gt_intmax(vioc_intr_unum)) {
			DRM_DEV_ERROR(dev_context->dev, "display device intr_num is out of range\r\n");
			internal_ok = (bool)false;
		} else {
			vioc_intr_inum = (int)vioc_intr_unum;
		}
	}
	if (internal_ok) {
		(void)tccvioc_disable_irq_of_display_dev(dev_context,
							     irq_num,
							     vioc_intr_inum);
		(void)irq_set_affinity_hint(dev_context->hw_data.display_device.irq_num, NULL);
		devm_free_irq(dev_context->dev,
			      dev_context->hw_data.display_device.irq_num,
			      dev_context);
	}

	return internal_ok;
}

/* coverity[misra_c_2012_rule_8_13] */
static void tccdrm_vioc_unbind(struct device *dev, struct device *master_dev,
			void *data)
{
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_11_5] */
	struct tccdrm_vioc_context *dev_context = dev_get_drvdata(dev);
	bool internal_ok = (bool)true;
	//unsigned long irqflags;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter master_dev is not
	 * used in the function.
	 */
	(void)master_dev;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter dev is not used in
	 * the function.
	 */
	(void)data;

	internal_ok = tccdrm_vioc_unbind_phase_0(dev_context);

	if (internal_ok) {
		/* Deactivate CRTC clock */
		(void)tccvioc_clk_disable(dev_context, CRTC_FLAGS_VCLK_BIT);

		#if defined(CONFIG_PM)
		pm_runtime_disable(dev);
		#endif

		devm_kfree(dev, dev_context);
	}
}

static const struct component_ops tccdrm_vioc_component_ops = {
	.bind	= tccdrm_vioc_bind,
	.unbind = tccdrm_vioc_unbind,
};

static int tccdrm_vioc_probe(struct platform_device *plat_dev)
{
	(void)DRM_INFO("Initialized %s %d.%d.%d %s\r\n",
			plat_dev->name,
			DRIVER_MAJOR,
			DRIVER_MINOR,
			DRIVER_PATCH,
			DRIVER_DATE);
	return component_add(&plat_dev->dev, &tccdrm_vioc_component_ops);
}

static int tccdrm_vioc_remove(struct platform_device *plat_dev)
{
	component_del(&plat_dev->dev, &tccdrm_vioc_component_ops);
	return 0;
}

#ifdef CONFIG_PM
/* coverity[misra_c_2012_rule_8_13] */
static int tcc_drm_vioc_suspend(struct device *dev)
{
	/* coverity[misra_c_2012_rule_11_5] */
	const struct tccdrm_vioc_context *dev_context =
		(const struct tccdrm_vioc_context *)dev_get_drvdata(dev);

	DRM_DEV_INFO(dev, "[INFO] \r\n");
	if (test_bit(CRTC_FLAGS_VCLK_BIT, &dev_context->crtc_flags) == 1) {
		DRM_DEV_INFO(dev, "[INFO] Disable vclk\r\n");
		clk_disable_unprepare(dev_context->hw_data.vioc_clock);
	}
	return 0;
}

/* coverity[misra_c_2012_rule_8_13] */
static int tcc_drm_vioc_resume(struct device *dev)
{
	/* coverity[misra_c_2012_rule_11_5] */
	const struct tccdrm_vioc_context *dev_context =
		(const struct tccdrm_vioc_context *)dev_get_drvdata(dev);
	int ret = 0;

	DRM_DEV_INFO(dev, "[INFO] \r\n");
	if (test_bit(CRTC_FLAGS_VCLK_BIT, &dev_context->crtc_flags) == 1) {
		DRM_DEV_INFO(dev, "[INFO] Enable vclk\r\n");
		ret = clk_prepare_enable(dev_context->hw_data.vioc_clock);
	}
	return ret;
}
#endif

static const struct dev_pm_ops tcc_drm_vioc_pm_ops = {
	/* coverity[misra_c_2012_rule_20_7] */
	SET_SYSTEM_SLEEP_PM_OPS(tcc_drm_vioc_suspend, tcc_drm_vioc_resume)
};

static const struct of_device_id drm_vioc_dt_match[] = {
	#if defined(CONFIG_DRM_TELECHIPS_LCD)
	{
		.compatible = "telechips,drm-vioc-0",
	},
	#endif
	#if defined(CONFIG_DRM_TELECHIPS_EXT)
	{
		.compatible = "telechips,drm-vioc-1",
	},
	#endif
	#if defined(CONFIG_DRM_TELECHIPS_THIRD)
	{
		.compatible = "telechips,drm-vioc-2",
	},
	#endif
	#if defined(CONFIG_DRM_TELECHIPS_FOURTH)
	{
		.compatible = "telechips,drm-vioc-3",
	},
	#endif
	#if defined(CONFIG_DRM_TELECHIPS_FIFTH)
	{
		.compatible = "telechips,drm-vioc-4",
	},
	#endif
	{
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, drm_vioc_dt_match);

struct platform_driver tccdrm_vioc_driver = {
	.probe		= tccdrm_vioc_probe,
	.remove		= tccdrm_vioc_remove,
	.driver		= {
		.name	= "tccdrm-vioc",
		.owner	= THIS_MODULE,
		.pm	= &tcc_drm_vioc_pm_ops,
		.of_match_table = of_match_ptr(drm_vioc_dt_match),
	},
};

/* coverity[misra_c_2012_rule_8_3] */
/* coverity[misra_c_2012_rule_8_5] */
/* coverity[misra_c_2012_rule_8_6] */
/* coverity[misra_c_2012_rule_8_11] */
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
EXPORT_SYMBOL(tccdrm_vioc_driver);

/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_21_2] */ //K5.4
MODULE_DESCRIPTION("Telechips DRM VIOC Driver");
/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_5_9] */ //K5.4
/* coverity[misra_c_2012_rule_21_2] */ //K5.4
MODULE_LICENSE("GPL");

/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_5_9] */ //K5.4
/* coverity[misra_c_2012_rule_21_2] */ //K5.4
MODULE_VERSION(__stringify(DRIVER_MAJOR) "."
               __stringify(DRIVER_MINOR) "."
               __stringify(DRIVER_PATCH));

