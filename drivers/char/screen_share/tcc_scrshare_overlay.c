// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/platform_device.h>

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/of.h>

#include <video/telechips/vioc_rdma.h>
#include <video/telechips/vioc_wmix.h>
#include <video/telechips/vioc_global.h>
#include <video/telechips/tcc_types.h>

#include <tcc_scrshare_overlay.h>

static int tcc_scrshare_overlay_check_vrdma(
unsigned int rdma, shared_buffer_cfg_t cfg) {

	int ret = 0;

	if (VIOC_RDMA_IsVRDMA(rdma) == 0) {
		if (cfg.fmt == (unsigned int)TCC_LCDC_IMG_FMT_444SEP) {
			ret = -1;
		}
		else if (cfg.fmt == (unsigned int)TCC_LCDC_IMG_FMT_YUV420SP) {
			ret = -1;
		}
		else if (cfg.fmt == (unsigned int)TCC_LCDC_IMG_FMT_YUV422SP) {
			ret = -1;
		}
		else if	((cfg.fmt >= (unsigned int)TCC_LCDC_IMG_FMT_YUV420ITL0) &&
			(cfg.fmt <= (unsigned int)TCC_LCDC_IMG_FMT_YUV422ITL1)) {
			ret = -1;
		}
		else {
			// misra_c_2012_rule_15_7_violation:
			// No non-empty terminating "else" statement
		}
	}

	return ret;
}

static int tcc_scrshare_overlay_set_rdma(
const struct tcc_scrshare_overlay *overlay, shared_buffer_cfg_t cfg) {

	int ret = 0;

	if(overlay->rdma.reg != NULL) {
		VIOC_RDMA_SetImageSize(overlay->rdma.reg, cfg.dst_w, cfg.dst_h);
		VIOC_RDMA_SetImageFormat(overlay->rdma.reg, cfg.fmt);
		VIOC_RDMA_SetImageOffset(overlay->rdma.reg, cfg.fmt, cfg.frm_w);
		VIOC_RDMA_SetImageRGBSwapMode(overlay->rdma.reg, cfg.rgb_swap);
		VIOC_RDMA_SetImageBase(overlay->rdma.reg, cfg.src_addr, cfg.src_addr, cfg.src_addr);
	}
	else {
		ret = -1;
	}

	return ret;
}

static int tcc_scrshare_overlay_set_wmix(
const struct tcc_scrshare_overlay *overlay, shared_buffer_cfg_t cfg) {

	int ret = 0;

	if(overlay->wmix.reg != NULL) {
		VIOC_WMIX_SetPosition(overlay->wmix.reg, overlay->wmix_pos, cfg.dst_x, cfg.dst_y);
		VIOC_WMIX_SetUpdate(overlay->wmix.reg);
	}
	else {
		ret = -1;
	}

	return ret;
}

static int tcc_scrshare_overlay_update_image(
const struct tcc_scrshare_overlay *overlay) {

	int ret = 0;

	if(overlay->rdma.reg != NULL) {
		VIOC_RDMA_SetImageEnable(overlay->rdma.reg);
	}
	else {
		ret = -1;
	}

	return ret;
}

int tcc_scrshare_overlay_display(
const struct tcc_scrshare_overlay *overlay, shared_buffer_cfg_t cfg) {

	int ret = -1;

	if(overlay != NULL) {

		ret = tcc_scrshare_overlay_check_vrdma(overlay->rdma.idx, cfg);

		if(ret == 0) {
			ret = tcc_scrshare_overlay_set_rdma(overlay, cfg);
		}

		if(ret == 0) {
			ret = tcc_scrshare_overlay_set_wmix(overlay, cfg);
		}

		if(ret == 0) {
			ret = tcc_scrshare_overlay_update_image(overlay);
		}
	}

    return ret;
}

void tcc_scrshare_overlay_open(
struct tcc_scrshare_overlay *overlay) {

	if(overlay != NULL) {
		if(overlay->open_cnt < (UINT_MAX -1u)) {
			overlay->open_cnt++;
		}
	}
}

void tcc_scrshare_overlay_release(
struct tcc_scrshare_overlay *overlay) {

	if(overlay != NULL) {
		if(overlay->open_cnt == 1u) {
			if(overlay->rdma.reg != NULL) {
				VIOC_RDMA_SetImageDisable(overlay->rdma.reg);
			}
			if(overlay->wmix.reg != NULL) {
				VIOC_WMIX_SetPosition(overlay->wmix.reg, overlay->wmix_pos, 0, 0);
				VIOC_WMIX_SetUpdate(overlay->wmix.reg);
			}
		}

		if(overlay->open_cnt > 0u) {
			overlay->open_cnt--;
		}
	}
}

static int tcc_scrshare_overlay_parse_rdma(
struct tcc_scrshare_overlay *overlay, const struct platform_device *pdev) {

	const struct device_node *tmp_node = of_parse_phandle(pdev->dev.of_node, "rdma", 0);
	int ret = -1;

	if(tmp_node != NULL) {
		ret = of_property_read_u32_index(pdev->dev.of_node, "rdma", 1, &overlay->rdma.idx);
		if(ret == 0) {
			overlay->rdma.reg = VIOC_RDMA_GetAddress(overlay->rdma.idx);
			if(IS_ERR(overlay->rdma.reg)) {
				overlay->rdma.reg = NULL;
				ret = -1;
			}
			else {
				VIOC_RDMA_SetImageDisable(overlay->rdma.reg);
			}
		}
	}
	else {
		overlay->rdma.reg = NULL;
	}

	return ret;
}

static int tcc_scrshare_overlay_parse_wmix(
struct tcc_scrshare_overlay *overlay, const struct platform_device *pdev) {

	const struct device_node *tmp_node = of_parse_phandle(pdev->dev.of_node, "wmix", 0);
	int ret = -1;

	if(tmp_node != NULL) {
		ret = of_property_read_u32_index(pdev->dev.of_node, "wmix", 1, &overlay->wmix.idx);
		if(ret == 0) {
			overlay->wmix.reg = VIOC_WMIX_GetAddress(overlay->wmix.idx);
			if(IS_ERR(overlay->wmix.reg)) {
				overlay->wmix.reg = NULL;
				ret = -1;
			}
			else {
				if(get_vioc_index(overlay->wmix.idx) == 0u) {
					overlay->wmix_pos = get_vioc_index(overlay->rdma.idx);
				}
				else {
					overlay->wmix_pos = get_vioc_index(overlay->rdma.idx) - (0x4u * get_vioc_index(overlay->wmix.idx));
				}
			}
		}
	}
	else {
		overlay->wmix.reg = NULL;
	}

	return ret;
}

unsigned int tcc_scrshare_parse_overlay_node(
struct tcc_scrshare_overlay *overlay, const struct platform_device *pdev) {

	unsigned int screen_mode = 0u;

	if(overlay != NULL) {
		int ret = tcc_scrshare_overlay_parse_rdma(overlay, pdev);
		if(ret == 0) {
			ret = tcc_scrshare_overlay_parse_wmix(overlay, pdev);
		}

		if(ret == 0) {
			screen_mode = 1u;
			overlay->open_cnt = 0u;
		}
	}

	return screen_mode;
}
/* end of file */
