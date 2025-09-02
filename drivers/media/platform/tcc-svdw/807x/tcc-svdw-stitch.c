// SPDX-License-Identifier: GPL-2.0-or-later

#include "linux/of.h"
#include <linux/of_fdt.h>
#include <linux/types.h>
#include <linux/dev_printk.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/firmware.h>
#include "tcc-svdw-stitch.h"
#include "../tcc-svdw.h"

static void __iomem *pSVDW_reg;
static void __iomem *pCFG_reg;

int tcc_svdw_do_ioremap(struct tcc_svdw_device *p_svdw, struct device_node *node)
{
	pSVDW_reg = (void __iomem *)of_iomap(node, 0);
	pCFG_reg = (void __iomem *)of_iomap(node, 1);

	logd(&p_svdw->pdev->dev, "pSVDW_reg = %lx, pCFG_reg = %lx\n", (uintptr_t)pSVDW_reg,
	     (uintptr_t)pCFG_reg);

	return (pSVDW_reg == NULL) || (pCFG_reg == NULL);
}
EXPORT_SYMBOL(tcc_svdw_do_ioremap);

int svdw_get_ip_version(u32 *version)
{
	*version = __raw_readl(pSVDW_reg + SVDW_IP_VERSION);
	return 0;
}

int svdw_get_dummy_data_status(u32 id, u32 *data)
{
	u32 offset = 0U;
	int ret = 0;
	if (id < 4) {
		offset = SVDW_DUMMY_WRITE_0 + (0x4U * id);
		*data = (__raw_readl(pSVDW_reg + offset) & SVDW_DUMMY_WRITE_WREADY_MASK);
	} else {
		pr_err("Invalid id: %d\n", id);
		ret = -1;
	}
	return ret;
}

/**
 * svdw_set_recon_start - Set RECON_START register
 *
 * @r_ready:	[out]	ready status of surround view reconstruction
 * @r_start:	[in]	start surround view reconstruction
 *
 * Start SVDW reconstruction. 'RDONE_INTERRUPT' will arise after invoking this
 * function.
 */
void svdw_set_recon_start(u32 *r_ready, bool r_start)
{
	__raw_writel(r_start << SVDW_RECON_START_RSTART_SHIFT, pSVDW_reg + SVDW_RECON_START);
	if (r_ready) {
		*r_ready = (__raw_readl(pSVDW_reg + SVDW_RECON_START) >>
			    SVDW_RECON_START_RSTART_SHIFT) &
			   0x1U;
	}
}

/**
 * svdw_set_dummy_data - Set DUMMY_WRITE_[0,1,2,3] register
 *
 * @id:		[in]	the id of DUMMY_WRITE register interfaces
 * @w_ready	[out]	write status of dummy patch
 * @w_start:	[in]	start writing dummy patch data
 *
 * TODO describe the meaning of 'dummy' here and the usage of it.
 *
 * Returns 0 or -EINVAL (if 'id' is out of range)
 */
int svdw_set_dummy_data(u32 id, u32 *w_ready, u32 w_start)
{
	u32 offset = SVDW_DUMMY_WRITE_0 + (0x4U * id);
	u32 value = 0U;
	int ret = 0;

	if (id >= 4U) {
		pr_err("Out of range of id: %d\n", id);
		ret = -EINVAL;
	} else {
		value = (w_start << SVDW_DUMMY_WRITE_WSTART_SHIFT);
		__raw_writel(value, pSVDW_reg + offset);
		*w_ready = (__raw_readl(pSVDW_reg + offset) >> SVDW_DUMMY_WRITE_WREADY_SHIFT) & 0x1U;
	}
	return ret;
}

/**
 * svdw_enable_overlay - Enable overlay (a car image)
 * @enable	[in]	enable to show overlay image
 *
 * Enable overlay image. Before invoking the function, related image must be
 * loaded first.
 */
void svdw_enable_overlay(bool enable)
{
	u32 offset = SVDW_ENABLE;
	u32 regval;

	regval = __raw_readl(pSVDW_reg + offset);
	regval = regval & ~(0x1U << SVDW_ENABLE_OVERLAY_ENABLE_SHIFT);
	regval = regval | ((enable & 0x1U) << SVDW_ENABLE_OVERLAY_ENABLE_SHIFT);

	__raw_writel(regval, pSVDW_reg + offset);
}

/**
 * svdw_enable_recon_timer - Enable reconstruction timer
 * @enable	[in]	enable reconstruction timer
 *
 * When reconstruction is set, SVDW will automatically initiate reconstruction
 * every 'recon_timer_thres cycles'. The 'recon_timer_thres_cycles' can be read
 * by svdw_get_recon_timer_cnt().
 */
void svdw_enable_recon_timer(bool enable)
{
	u32 offset = SVDW_ENABLE;
	u32 regval;

	regval = __raw_readl(pSVDW_reg + offset);
	regval = regval & ~(0x1U << SVDW_ENABLE_RECON_TIMER_SHIFT);
	regval = regval | ((enable & 0x1U) << SVDW_ENABLE_RECON_TIMER_SHIFT);

	__raw_writel(regval, pSVDW_reg + offset);
}

/**
 * svdw_enable_patches - Enable patches
 * @patch0	[in]	enable 1st patch
 * @patch1	[in]	enable 2nd patch
 * @patch2	[in]	enable 3rd patch
 * @patch3 	[in]	enable 4th patch
 *
 * To make stitcher create a SVDW reconstructed image, it is necessary to enable
 * all patches.
 */
void svdw_enable_patches(bool patch0, bool patch1, bool patch2, bool patch3)
{
	u32 offset = SVDW_PATCH_ENABLE;
	u32 regval = ((patch0 & 0x1U) << SVDW_PATCH_ENABLE_0_SHIFT) |
		     ((patch1 & 0x1U) << SVDW_PATCH_ENABLE_1_SHIFT) |
		     ((patch2 & 0x1U) << SVDW_PATCH_ENABLE_2_SHIFT) |
		     ((patch3 & 0x1U) << SVDW_PATCH_ENABLE_3_SHIFT);
	__raw_writel(regval, pSVDW_reg + offset);
}

/**
 * svdw_enable_all_patches - Enable all patches
 */
void svdw_enable_all_patches(void)
{
	svdw_enable_patches(true, true, true, true);
}

void svdw_set_view_address_l(u32 address_lsb)
{
	u32 offset = SVDW_VIEW_ADDRESS_L;
	__raw_writel(address_lsb, pSVDW_reg + offset);
}

void svdw_set_view_stride_l(u32 stride, u32 address_msb)
{
	u32 offset = SVDW_VIEW_STRIDE_L;
	u32 regval = (stride << SVDW_VIEW_STRIDE_L_SHIFT) | address_msb;
	__raw_writel(regval, pSVDW_reg + offset);
}

void svdw_set_view_address_c(u32 address_lsb)
{
	u32 offset = SVDW_VIEW_ADDRESS_C;
	__raw_writel(address_lsb, pSVDW_reg + offset);
}

void svdw_set_view_stride_c(u32 stride, u32 address_msb)
{
	u32 offset = SVDW_VIEW_STRIDE_C;
	u32 regval = (stride << SVDW_VIEW_STRIDE_C_SHIFT) | address_msb;
	__raw_writel(regval, pSVDW_reg + offset);
}

int svdw_set_view_size(u32 width, u32 height)
{
	int ret = 0;
	u32 offset = SVDW_VIEW_SIZE;
	u32 regval;
	if ((width == 0) && (height == 0) && (width > 0xFFFU) && (height > 0xFFFU)) {
		ret = -1;
	} else {
		regval = (height << SVDW_SIZE_HEIGHT_SHIFT) | width;
		__raw_writel(regval, pSVDW_reg + offset);
	}
	return ret;
}

int svdw_set_view_format(u32 fmt)
{
	int ret = 0;
	u32 offset = SVDW_VIEW_FORMAT;
	if (fmt > FMT_BGR) {
		ret = -1;
	} else {
		__raw_writel(fmt, pSVDW_reg + offset);
	}
	return ret;
}

void svdw_set_overlay_address(u32 address_lsb)
{
	u32 offset = SVDW_OVERLAY_ADDRESS;
	__raw_writel(address_lsb, pSVDW_reg + offset);
}

void svdw_set_overlay_stride(u32 stride, u32 address_msb)
{
	u32 offset = SVDW_OVERLAY_STRIDE;
	u32 regval = (stride << SVDW_OVERLAY_STRIDE_SHIFT) | address_msb;
	__raw_writel(regval, pSVDW_reg + offset);
}

void svdw_set_overlay_size(u32 width, u32 height)
{
	void *reg_addr = pSVDW_reg + SVDW_OVERLAY_SIZE;
	u32 value = (height << SVDW_SIZE_HEIGHT_SHIFT) | width;
	__raw_writel(value, reg_addr);
}

void svdw_set_overlay_offset(u32 left, u32 top)
{
	void *reg_addr = pSVDW_reg + SVDW_OVERLAY_OFFSET;
	u32 value = (top << SVDW_OVERLAY_OFFSET_HEIGHT_SHIFT) | left;
	__raw_writel(value, reg_addr);
}

int svdw_set_patch_offset(u32 id, u32 left, u32 top)
{
	int ret = 0;
	void *reg_addr = pSVDW_reg + (SVDW_PATCH_OFFSET_0 + (0x04U * id));
	u32 regval = (top << SVDW_PATCH_OFFSET_TOP_SHIFT) | left;

	if (id >= 0U && id < 4U) {
		__raw_writel(regval, reg_addr);
	} else {
		ret = -1;
	}
	return ret;
}

int svdw_set_blend_anchor_pos(u32 id, u32 left, u32 top)
{
	int ret = 0;
	void *addr = pSVDW_reg + (SVDW_BLEND_ANCHOR_POSITION_0 + (0x4U * id));
	u32 regval = (top << SVDW_POS_TOP_SHIFT) | left;

	if (id >= 0U && id < 4U) {
		__raw_writel(regval, addr);
	} else {
		ret = -1;
	}
	return ret;
}

void svdw_set_blend_angle(u32 id, u32 blend_angle, u32 cut_angle)
{
	void *addr = pSVDW_reg + (SVDW_BLEND_ANGLE_0 * (0x4U * id));
	u32 regval = (blend_angle << SVDW_BLEND_ANGLE_BLEND_ANGLE_SHFIT) |
		     (cut_angle << SVDW_BLEND_ANGLE_CUT_ANGLE_SHIFT);
	__raw_writel(regval, addr);
}

void svdw_set_patch_direction(u32 direct0, u32 direct1, u32 direct2, u32 direct3)
{
	void *addr = pSVDW_reg + SVDW_PATCH_DIRECTION;
	u32 regval = ((direct0 & 0x3U) << SVDW_PATCH_DIRECTION_DIR0_SHIFT) |
		     ((direct1 & 0x3U) << SVDW_PATCH_DIRECTION_DIR1_SHIFT) |
		     ((direct2 & 0x3U) << SVDW_PATCH_DIRECTION_DIR2_SHIFT) |
		     ((direct3 & 0x3U) << SVDW_PATCH_DIRECTION_DIR3_SHIFT);
	__raw_writel(regval, addr);
}

void svdw_set_default_color(u32 default_color)
{
	void *addr = pSVDW_reg + SVDW_DEFAULT_COLOR;
	__raw_writel(default_color, addr);
}

void svdw_set_buffer_mode(u32 frame_drop_enable, u32 buffer_mode)
{
	void *addr = pSVDW_reg + SVDW_BUFFER_MODE;
	u32 regval = (frame_drop_enable << SVDW_BUFFER_MODE_FRMDROP_ENBL_SHIFT) |
		     ((buffer_mode & 0x3U) << SVDW_BUFFER_MODE_BFR_MODE_SHIFT);
	__raw_writel(regval, addr);
}

void svdw_set_view_hole_filling(u32 vhf)
{
	void *addr = pSVDW_reg + SVDW_VIEW_HOLE_FILLING;
	u32 regval = vhf & 0x1U;

	__raw_writel(regval, addr);
}

void svdw_set_recon_timer_thres(u32 threshold)
{
	void *addr = pSVDW_reg + SVDW_RECON_TIMER_THRES;
	__raw_writel(threshold, addr);
}

void svdw_set_axi_config(u32 max_ros, u32 max_wos, u32 max_rburst, u32 max_wburst)
{
	u32 value = 0;

	value |= (max_ros << SVDW_AXI_CONFIG_MAX_ROS_SHIFT) & SVDW_AXI_CONFIG_MAX_ROS_MASK;
	value |= (max_rburst << SVDW_AXI_CONFIG_MAX_RBURST_SHIFT) & SVDW_AXI_CONFIG_MAX_RBURST_MASK;

	value |= (max_wos << SVDW_AXI_CONFIG_MAX_WOS_SHIFT) & SVDW_AXI_CONFIG_MAX_WOS_MASK;
	value |= (max_wburst << SVDW_AXI_CONFIG_MAX_WBURST_SHIFT) & SVDW_AXI_CONFIG_MAX_WBURST_MASK;

	__raw_writel(value, pSVDW_reg + SVDW_AXI_CONFIG);
}
EXPORT_SYMBOL(svdw_set_axi_config);

void svdw_set_rdone_interrupt_delay(u32 rdone_delay)
{
	void *addr = pSVDW_reg + SVDW_RDONE_INTERRUPT_DELAY;
	__raw_writel((rdone_delay & 0xFFFFU), addr);
}

void svdw_set_rdone_interrupt_clear_time(u32 clr_time)
{
	void *addr = pSVDW_reg + SVDW_RDONE_INTERRUPT_CLEAR_TIME;
	__raw_writel(clr_time, addr);
}

void svdw_set_frame_time_cycle(u32 frame_time_cycle)
{
	__raw_writel(frame_time_cycle, pSVDW_reg + SVDW_FRAME_TIME_CYCLE);
}
EXPORT_SYMBOL(svdw_set_frame_time_cycle);

void svdw_set_frame_timeout_threshold(u32 frame_timeout_thres)
{
	__raw_writel(frame_timeout_thres, pSVDW_reg + SVDW_FRAME_TIMEOUT_THRES);
}
EXPORT_SYMBOL(svdw_set_frame_timeout_threshold);

void svdw_set_frame_fast_threshold(u32 frame_fast_thres)
{
	__raw_writel(frame_fast_thres, pSVDW_reg + SVDW_FRAME_FAST_THRES);
}
EXPORT_SYMBOL(svdw_set_frame_fast_threshold);

void svdw_set_frame_slow_threshold(u32 frame_slow_thres)
{
	__raw_writel(frame_slow_thres, pSVDW_reg + SVDW_FRAME_SLOW_THRES);
}
EXPORT_SYMBOL(svdw_set_frame_slow_threshold);

void svdw_set_frame_toggle_threshold(u32 frame_toggle_thres)
{
	__raw_writel((frame_toggle_thres & SVDW_FRAME_TOGGLE_THRES_MASK),
		     pSVDW_reg + SVDW_FRAME_TOGGLE_THRES);
}
EXPORT_SYMBOL(svdw_set_frame_toggle_threshold);

void svdw_set_line_toggle_threshold(u32 line_toggle_thres)
{
	__raw_writel((line_toggle_thres & SVDW_LINE_TOGGLE_THRES_MASK),
		     pSVDW_reg + SVDW_LINE_TOGGLE_THRES);
}
EXPORT_SYMBOL(svdw_set_line_toggle_threshold);

void svdw_set_max_toggle_threshold(u32 max_toggle_thres)
{
	__raw_writel((max_toggle_thres & SVDW_MAX_TOGGLE_CNT_MASK),
		     pSVDW_reg + SVDW_MAX_TOGGLE_CNT);
}
EXPORT_SYMBOL(svdw_set_max_toggle_threshold);

void svdw_get_recon_timer_cnt(u32 *recon_timer_cnt)
{
	*recon_timer_cnt = __raw_readl(pSVDW_reg + SVDW_RECON_TIMER_CNT);
}

void svdw_get_axi_status(u32 *axi_read_err, u32 *axi_write_err)
{
	u32 regval = __raw_readl(pSVDW_reg + SVDW_AXI_STATUS);
	*axi_read_err = (regval >> 8U) & 0x1U;
	*axi_write_err = regval & 0x1U;
}

void svdw_get_buffer_page(struct svdw_buf_page *buf_page)
{
	memcpy(buf_page, pSVDW_reg + SVDW_BUFFER_PAGE, sizeof(struct svdw_buf_page));
}

void svdw_clear_axi_error(u32 r_err_clr, u32 w_err_clr)
{
	void *addr = pSVDW_reg + SVDW_AXI_CLEAR;
	u32 regval = (r_err_clr << SVDW_AXI_CLEAR_RD_ERR_SHIFT) |
		     (w_err_clr << SVDW_AXI_CLEAR_WR_ERR_SHIFT);
	__raw_writel(regval, addr);
}

void svdw_clear_rdone_interrupt(void)
{
	void *addr = pSVDW_reg + SVDW_RDONE_INTERRUPT_CLEAR;
	u32 clr = 0x1U;
	__raw_writel((clr & 0x1U), addr);
}

void svdw_enable(u32 patch_id, u32 svdw_enable, u32 stream_enable, u32 bypass)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_ENABLE_0);
	u32 offset = 0;
	u32 value = 0;
	u32 chroma_interpolation = 0;
	u32 filter_enable = 0;
	u32 downsample_enable = 1;

	if ((svdw_enable == 1) && (bypass == 0)) {
		chroma_interpolation = 1;
		filter_enable = 1;
		downsample_enable = 0;
	}

	value = (__raw_readl(pSVDW_reg + offset) & ~(SVDW_ENABLE_DOWNSAMPLING_ENABLE_MASK) &
		 ~(SVDW_ENABLE_FILTER_ENABLE_MASK) & ~(SVDW_ENABLE_VERTICAL_FLIP_MASK) &
		 ~(SVDW_ENABLE_HORIZONTAL_FLIP_MASK) & ~(SVDW_ENABLE_CHROMA_INTERPOLATION_MASK) &
		 ~(SVDW_ENABLE_DEWARP_ENABLE_MASK) & ~(SVDW_ENABLE_BYPASS_ENABLE_MASK) &
		 ~(SVDW_ENABLE_STREAM_ENABLE_MASK));
	value |= (svdw_enable << SVDW_ENABLE_DEWARP_ENABLE_SHIFT);
	value |= (stream_enable << SVDW_ENABLE_STREAM_ENABLE_SHIFT);
	value |= (bypass << SVDW_ENABLE_BYPASS_ENABLE_SHIFT);
	value |= (chroma_interpolation << SVDW_ENABLE_CHROMA_INTERPOLATION_SHIFT);
	value |= (filter_enable << SVDW_ENABLE_FILTER_ENABLE_SHIFT);
	value |= (downsample_enable << SVDW_ENABLE_DOWNSAMPLING_ENABLE_SHIFT);

	__raw_writel(value, addr);
}
EXPORT_SYMBOL(svdw_enable);

void svdw_set_bypass_address_l(u32 id, u32 address_l_lsb)
{
	void *addr = pSVDW_reg + SVDW_BYPASS_ADDRESS_L_0 + (0x100U * id);
	__raw_writel(address_l_lsb, addr);
}

void svdw_set_bypass_stride_l(u32 id, u32 stride, u32 address_l_msb)
{
	void *addr = pSVDW_reg + SVDW_BYPASS_STRIDE_L_0 + (0x100U * id);
	u32 regval = (stride << SVDW_STRIDE_SHIFT) | address_l_msb;
	__raw_writel(regval, addr);
}

void svdw_set_bypass_address_c(u32 id, u32 address_c_lsb)
{
	void *addr = pSVDW_reg + SVDW_BYPASS_ADDRESS_C_0 + (0x100U * id);
	__raw_writel(address_c_lsb, addr);
}

void svdw_set_bypass_stride_c(u32 id, u32 stride, u32 address_c_msb)
{
	void *addr = pSVDW_reg + SVDW_BYPASS_STRIDE_C_0 + (0x100U * id);
	u32 regval = (stride << SVDW_STRIDE_SHIFT) | address_c_msb;
	__raw_writel(regval, addr);
}

void svdw_set_patch_address(u32 patch_id, u32 addr0, u32 addr1, u32 addr2)
{
	u32 reg_addr0 = SVDW_REG_OFFS(patch_id, SVDW_PATCH_ADDRESS_0_0);
	u32 reg_addr1 = SVDW_REG_OFFS(patch_id, SVDW_PATCH_ADDRESS_1_0);
	u32 reg_addr2 = SVDW_REG_OFFS(patch_id, SVDW_PATCH_ADDRESS_2_0);

	__raw_writel(addr0, pSVDW_reg + reg_addr0);
	__raw_writel(addr1, pSVDW_reg + reg_addr1);
	__raw_writel(addr2, pSVDW_reg + reg_addr2);
}

void svdw_set_patch_stride_l(u32 patch_id, u32 stride, u32 address_msb)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_PATCH_STRIDE_L_0);
	u32 regval = (stride << SVDW_STRIDE_SHIFT) | address_msb;
	__raw_writel(regval, addr);
}

void svdw_set_patch_chroma_offset(u32 patch_id, u32 offset)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_PATCH_CHROMA_OFFSET_0);
	__raw_writel(offset, addr);
}

void svdw_set_patch_stride_c(u32 patch_id, u32 stride, u32 address_msb)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_PATCH_STRIDE_C_0);
	u32 regval = (stride << SVDW_STRIDE_SHIFT) | address_msb;
	__raw_writel(regval, addr);
}

void svdw_set_input_size(u32 patch_id, u32 width, u32 height)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_INPUT_SIZE_0);
	u32 value = 0U;

	value = (((height & 0xFFFU) << SVDW_SIZE_HEIGHT_SHIFT) |
		 ((width & 0xFFFU) << SVDW_SIZE_WIDTH_SHIFT));

	__raw_writel(value, addr);
}
EXPORT_SYMBOL(svdw_set_input_size);

void svdw_set_patch_size(u32 patch_id, u32 width, u32 height)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_PATCH_SIZE_0);
	u32 value = 0U;

	value = (((height & 0xFFFU) << SVDW_SIZE_HEIGHT_SHIFT) |
		 ((width & 0xFFFU) << SVDW_SIZE_WIDTH_SHIFT));
	__raw_writel(value, addr);
}
EXPORT_SYMBOL(svdw_set_patch_size);

void svdw_set_input_format(u32 patch_id, u32 format, u32 ir_enable)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_IMAGE_FORMAT_0);
	u32 value = 0;

	if (ir_enable != 0U) {
		/* error */
		(void)pr_err("[INF][ODW-%d] does not support ir channel\n", patch_id);
	} else {
		value = (__raw_readl(addr) & ~(SVDW_IMAGE_FORMAT_INPUT_FORMAT_MASK));
		value |= (format << SVDW_IMAGE_FORMAT_INPUT_FORMAT_SHIFT);
		__raw_writel(value, addr);
	}
}
EXPORT_SYMBOL(svdw_set_input_format);

void svdw_set_bypass_format(u32 patch_id, u32 format, u32 ir_enable)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_IMAGE_FORMAT_0);
	u32 value = 0;

	if (ir_enable != 0U) {
		/* error */
		(void)pr_err("[INF][ODW-%d] does not support ir channel\n", patch_id);
	} else {
		value = (__raw_readl(addr) & ~(SVDW_IMAGE_FORMAT_BYPASS_FORMAT_MASK));
		value |= (format << SVDW_IMAGE_FORMAT_BYPASS_FORMAT_SHIFT);
		__raw_writel(value, addr);
	}
}
EXPORT_SYMBOL(svdw_set_bypass_format);

void svdw_set_patch_format(u32 patch_id, u32 format, u32 ir_enable)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_IMAGE_FORMAT_0);
	u32 value = 0;

	if (ir_enable != 0U) {
		/* error */
		(void)pr_err("[INF][ODW-%d] does not support ir channel\n", patch_id);
	} else {
		value = (__raw_readl(addr) & ~(SVDW_IMAGE_FORMAT_PATCH_FORMAT_MASK));
		value |= (format << SVDW_IMAGE_FORMAT_PATCH_FORMAT_SHIFT);
		__raw_writel(value, addr);
	}
}
EXPORT_SYMBOL(svdw_set_patch_format);

void svdw_set_block_config(u32 patch_id, u32 offset_x, u32 offset_y, u32 interval_x,
			   uint interval_y)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_BLOCK_CONFIG_0);
	u32 value = 0;

	value = (__raw_readl(addr) & ~(SVDW_BLOCK_CONFIG_BLOCK_OFFSET_Y_MASK) &
		 ~(SVDW_BLOCK_CONFIG_BLOCK_OFFSET_X_MASK) &
		 ~(SVDW_BLOCK_CONFIG_BLOCK_INTERVAL_Y_MASK) &
		 ~(SVDW_BLOCK_CONFIG_BLOCK_INTERVAL_X_MASK));
	value |= (offset_y << SVDW_BLOCK_CONFIG_BLOCK_OFFSET_Y_SHIFT);
	value |= (offset_x << SVDW_BLOCK_CONFIG_BLOCK_OFFSET_X_SHIFT);
	value |= (interval_y << SVDW_BLOCK_CONFIG_BLOCK_INTERVAL_Y_SHIFT);
	value |= (interval_x << SVDW_BLOCK_CONFIG_BLOCK_INTERVAL_X_SHIFT);

	__raw_writel(value, addr);
}
EXPORT_SYMBOL(svdw_set_block_config);

void svdw_set_filter(u32 patch_id, u32 firs[], u32 iirs[])
{
	void *addr_fir = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_FILTER_KERNEL_0_0);
	void *addr_iir = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_FILTER_KERNEL_1_0);
	u32 fir = 0;
	u32 iir = 0;

	fir = ((firs[3] << SVDW_FILTER_KERNEL_FIR_3_SHIFT) |
	       (firs[2] << SVDW_FILTER_KERNEL_FIR_2_SHIFT) |
	       (firs[1] << SVDW_FILTER_KERNEL_FIR_1_SHIFT) |
	       (firs[0] << SVDW_FILTER_KERNEL_FIR_0_SHIFT));

	iir = ((iirs[2] << SVDW_FILTER_KERNEL_IIR_2_SHIFT) |
	       (iirs[1] << SVDW_FILTER_KERNEL_IIR_1_SHIFT) |
	       (iirs[0] << SVDW_FILTER_KERNEL_IIR_0_SHIFT) |
	       (firs[4] << SVDW_FILTER_KERNEL_FIR_4_SHIFT));

	__raw_writel(fir, addr_fir);
	__raw_writel(iir, addr_iir);
}
EXPORT_SYMBOL(svdw_set_filter);

void svdw_set_input_fisheye(u32 patch_id, u32 is_fisheye)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_IS_FISHEYE_0);
	u32 value = 0;

	value = is_fisheye & SVDW_IS_FISHEYE_MASK;
	__raw_writel(value, addr);
}
EXPORT_SYMBOL(svdw_set_input_fisheye);

void svdw_set_backward_scan_config(u32 patch_id, u32 scan_xy_swap, u32 scan_margin_x,
				   uint scan_margin_y, uint round_thres)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_BACKWARD_SCAN_CONFIG_0);
	u32 value = 0;

	value = (__raw_readl(addr) & ~(SVDW_BACKWARD_SCAN_CONFIG_SCAN_XY_SWAP_MASK) &
		 ~(SVDW_BACKWARD_SCAN_CONFIG_SCAN_MARGIN_Y_MASK) &
		 ~(SVDW_BACKWARD_SCAN_CONFIG_SCAN_MARGIN_X_MASK) &
		 ~(SVDW_BACKWARD_SCAN_CONFIG_ROUND_THRES_MASK));
	value |= (scan_xy_swap << SVDW_BACKWARD_SCAN_CONFIG_SCAN_XY_SWAP_SHIFT);
	value |= (scan_margin_y << SVDW_BACKWARD_SCAN_CONFIG_SCAN_MARGIN_Y_SHIFT);
	value |= (scan_margin_x << SVDW_BACKWARD_SCAN_CONFIG_SCAN_MARGIN_X_SHIFT);
	value |= (round_thres << SVDW_BACKWARD_SCAN_CONFIG_ROUND_THRES_SHIFT);

	__raw_writel(value, addr);
}
EXPORT_SYMBOL(svdw_set_backward_scan_config);

void svdw_set_position_fifo_timeout_thres(u32 patch_id, u32 position_fifo_timeout_thres)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_POSITION_FIFO_TIMEOUT_THRES_0);
	u32 value = 0;

	value = (position_fifo_timeout_thres & SVDW_POSITION_FIFO_TIMEOUT_THRES_MASK);
	__raw_writel(value, addr);
}
EXPORT_SYMBOL(svdw_set_position_fifo_timeout_thres);

void svdw_set_bypass_fifo_timeout_thres(u32 patch_id, u32 bypass_fifo_timeout_thres)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_BYPASS_FIFO_TIMEOUT_THRES_0);
	u32 value = 0;
	value = bypass_fifo_timeout_thres & 0xFFFFU;
	__raw_writel(value, addr);
}
EXPORT_SYMBOL(svdw_set_bypass_fifo_timeout_thres);

void svdw_set_patch_fifo_timeout_thres(u32 patch_id, u32 patch_fifo_timeout_thres)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_PATCH_FIFO_TIMEOUT_THRES_0);
	u32 value = 0;
	value = patch_fifo_timeout_thres & 0xFFFFU;
	__raw_writel(value, addr);
}
EXPORT_SYMBOL(svdw_set_patch_fifo_timeout_thres);

void svdw_set_svdw_fifo_timeout_cnt(u32 id, u32 svdw_fifo_timeout_cnt)
{
	u32 value = 0;
	value = (svdw_fifo_timeout_cnt & SVDW_OUTPUT_FIFO_TIMEOUT_THRES_MASK);

	svdw_set_bypass_fifo_timeout_thres(id, value);
	svdw_set_patch_fifo_timeout_thres(id, value);
}
EXPORT_SYMBOL(svdw_set_svdw_fifo_timeout_cnt);

void svdw_set_bypass_buffer_wait_time(u32 patch_id, u32 wait_time)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_BYPASS_BUFFER_WAIT_TIME_0);
	u32 value = 0;
	value = wait_time & 0xFFFFU;
	__raw_writel(value, addr);
}
EXPORT_SYMBOL(svdw_set_bypass_buffer_wait_time);

void svdw_set_patch_buffer_wait_time(u32 patch_id, u32 wait_time)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_BYPASS_BUFFER_WAIT_TIME_0);
	u32 value = 0;
	value = wait_time & 0xFFFFU;
	__raw_writel(value, addr);
}
EXPORT_SYMBOL(svdw_set_patch_buffer_wait_time);

void svdw_set_svdw_buffer_wait_cnt(u32 id, u32 svdw_buffer_wait_cnt)
{
	u32 value = 0;
	value = (svdw_buffer_wait_cnt & SVDW_OUTPUT_FIFO_TIMEOUT_THRES_MASK);

	svdw_set_bypass_buffer_wait_time(id, value);
	svdw_set_patch_buffer_wait_time(id, value);
}
EXPORT_SYMBOL(svdw_set_svdw_buffer_wait_cnt);

void svdw_set_buffer_capacity(u32 patch_id, u32 buffer_capacity)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_BUFFER_CAPACITY_0);
	u32 value = 0;

	value = ((buffer_capacity << SVDW_BUFFER_CAPACITY_BYPASS_BUFFER_CAPACITY_SHIFT) |
		 (buffer_capacity << SVDW_BUFFER_CAPACITY_PATCH_BUFFER_CAPACITY_SHIFT));

	__raw_writel(value, addr);
}
EXPORT_SYMBOL(svdw_set_buffer_capacity);

void svdw_set_output_format(u32 id, u32 format)
{
	u32 offset = 0;
	u32 value = 0;

	switch (id) {
	case 0:
		offset = SVDW_IMAGE_FORMAT_0;
		break;
	case 1:
		offset = SVDW_IMAGE_FORMAT_1;
		break;
	default:
		break;
	}

	value = (__raw_readl(pSVDW_reg + offset) & (~(SVDW_IMAGE_FORMAT_PATCH_FORMAT_MASK) &
						    ~(SVDW_IMAGE_FORMAT_BYPASS_FORMAT_MASK)));

	value |= ((format << SVDW_IMAGE_FORMAT_PATCH_FORMAT_SHIFT) |
		  (format << SVDW_IMAGE_FORMAT_BYPASS_FORMAT_SHIFT));

	__raw_writel(value, pSVDW_reg + offset);
}
EXPORT_SYMBOL(svdw_set_output_format);

void svdw_set_view_address(u32 addresses[], u32 fmt)
{
	if (fmt > FMT_BGR) {
		pr_err("Invalid format as the SVM view format.");
		return;
	}
	if (fmt >= FMT_RGB) {
		/* RGB format */
		__raw_writel(addresses[0], pSVDW_reg + SVDW_VIEW_ADDRESS_L);
	} else {
		/* YUV format */
		__raw_writel(addresses[0], pSVDW_reg + SVDW_VIEW_ADDRESS_L);
		__raw_writel(addresses[1], pSVDW_reg + SVDW_VIEW_ADDRESS_C);
	}
}
EXPORT_SYMBOL(svdw_set_view_address);

void svdw_set_anchor_position(u32 anchor_idx, u32 left, u32 top)
{
	u32 offset = (anchor_idx * 0x4U) + SVDW_BLEND_ANCHOR_POSITION_0;
	u32 value = (top << SVDW_ANCHOR_TOP_SHIFT) | left;

	__raw_writel(value, pSVDW_reg + offset);
}

void svdw_set_background_color(u32 id, u32 color)
{
	__raw_writel(color, pSVDW_reg + SVDW_DEFAULT_COLOR);
}
EXPORT_SYMBOL(svdw_set_background_color);

void svdw_set_cam_matrix(u32 id, u32 cam_mat[])
{
	switch (id) {
	case 0:
		__raw_writel((cam_mat[0] & SVDW_CAM_MAT_MASK), pSVDW_reg + SVDW_CAM_MAT_FX_0);
		__raw_writel((cam_mat[1] & SVDW_CAM_MAT_MASK), pSVDW_reg + SVDW_CAM_MAT_SX_0);
		__raw_writel((cam_mat[2] & SVDW_CAM_MAT_MASK), pSVDW_reg + SVDW_CAM_MAT_CX_0);
		__raw_writel((cam_mat[3] & SVDW_CAM_MAT_MASK), pSVDW_reg + SVDW_CAM_MAT_FY_0);
		__raw_writel((cam_mat[4] & SVDW_CAM_MAT_MASK), pSVDW_reg + SVDW_CAM_MAT_CY_0);
		break;
	case 1:
		__raw_writel((cam_mat[0] & SVDW_CAM_MAT_MASK), pSVDW_reg + SVDW_CAM_MAT_FX_1);
		__raw_writel((cam_mat[1] & SVDW_CAM_MAT_MASK), pSVDW_reg + SVDW_CAM_MAT_SX_1);
		__raw_writel((cam_mat[2] & SVDW_CAM_MAT_MASK), pSVDW_reg + SVDW_CAM_MAT_CX_1);
		__raw_writel((cam_mat[3] & SVDW_CAM_MAT_MASK), pSVDW_reg + SVDW_CAM_MAT_FY_1);
		__raw_writel((cam_mat[4] & SVDW_CAM_MAT_MASK), pSVDW_reg + SVDW_CAM_MAT_CY_1);
		break;
	case 2:
		__raw_writel((cam_mat[0] & SVDW_CAM_MAT_MASK), pSVDW_reg + SVDW_CAM_MAT_FX_2);
		__raw_writel((cam_mat[1] & SVDW_CAM_MAT_MASK), pSVDW_reg + SVDW_CAM_MAT_SX_2);
		__raw_writel((cam_mat[2] & SVDW_CAM_MAT_MASK), pSVDW_reg + SVDW_CAM_MAT_CX_2);
		__raw_writel((cam_mat[3] & SVDW_CAM_MAT_MASK), pSVDW_reg + SVDW_CAM_MAT_FY_2);
		__raw_writel((cam_mat[4] & SVDW_CAM_MAT_MASK), pSVDW_reg + SVDW_CAM_MAT_CY_2);
		break;
	case 3:
		__raw_writel((cam_mat[0] & SVDW_CAM_MAT_MASK), pSVDW_reg + SVDW_CAM_MAT_FX_3);
		__raw_writel((cam_mat[1] & SVDW_CAM_MAT_MASK), pSVDW_reg + SVDW_CAM_MAT_SX_3);
		__raw_writel((cam_mat[2] & SVDW_CAM_MAT_MASK), pSVDW_reg + SVDW_CAM_MAT_CX_3);
		__raw_writel((cam_mat[3] & SVDW_CAM_MAT_MASK), pSVDW_reg + SVDW_CAM_MAT_FY_3);
		__raw_writel((cam_mat[4] & SVDW_CAM_MAT_MASK), pSVDW_reg + SVDW_CAM_MAT_CY_3);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(svdw_set_cam_matrix);

void svdw_set_forward_dist_coeff(u32 id, u32 dist_coeff[])
{
	switch (id) {
	case 0:
		__raw_writel((dist_coeff[0] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K1_FWD_0);
		__raw_writel((dist_coeff[1] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K2_FWD_0);
		__raw_writel((dist_coeff[2] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K3_FWD_0);
		__raw_writel((dist_coeff[3] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K4_FWD_0);
		break;
	case 1:
		__raw_writel((dist_coeff[0] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K1_FWD_1);
		__raw_writel((dist_coeff[1] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K2_FWD_1);
		__raw_writel((dist_coeff[2] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K3_FWD_1);
		__raw_writel((dist_coeff[3] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K4_FWD_1);
		break;
	case 2:
		__raw_writel((dist_coeff[0] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K1_FWD_2);
		__raw_writel((dist_coeff[1] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K2_FWD_2);
		__raw_writel((dist_coeff[2] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K3_FWD_2);
		__raw_writel((dist_coeff[3] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K4_FWD_2);
		break;
	case 3:
		__raw_writel((dist_coeff[0] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K1_FWD_3);
		__raw_writel((dist_coeff[1] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K2_FWD_3);
		__raw_writel((dist_coeff[2] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K3_FWD_3);
		__raw_writel((dist_coeff[3] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K4_FWD_3);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(svdw_set_forward_dist_coeff);

void svdw_set_backward_dist_coeff(u32 id, u32 dist_coeff[])
{
	switch (id) {
	case 0:
		__raw_writel((dist_coeff[0] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K1_BWD_0);
		__raw_writel((dist_coeff[1] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K2_BWD_0);
		__raw_writel((dist_coeff[2] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K3_BWD_0);
		__raw_writel((dist_coeff[3] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K4_BWD_0);
		break;
	case 1:
		__raw_writel((dist_coeff[0] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K1_BWD_1);
		__raw_writel((dist_coeff[1] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K2_BWD_1);
		__raw_writel((dist_coeff[2] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K3_BWD_1);
		__raw_writel((dist_coeff[3] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K4_BWD_1);
		break;
	case 2:
		__raw_writel((dist_coeff[0] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K1_BWD_2);
		__raw_writel((dist_coeff[1] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K2_BWD_2);
		__raw_writel((dist_coeff[2] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K3_BWD_2);
		__raw_writel((dist_coeff[3] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K4_BWD_2);
		break;
	case 3:
		__raw_writel((dist_coeff[0] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K1_BWD_3);
		__raw_writel((dist_coeff[1] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K2_BWD_3);
		__raw_writel((dist_coeff[2] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K3_BWD_3);
		__raw_writel((dist_coeff[3] & SVDW_DIST_COEFF_MASK),
			     pSVDW_reg + SVDW_DIST_COEFF_K4_BWD_3);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(svdw_set_backward_dist_coeff);

void svdw_set_forward_homography(u32 id, u32 homography[])
{
	switch (id) {
	case 0:
		__raw_writel((homography[0] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H11_FWD_0);
		__raw_writel((homography[1] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H12_FWD_0);
		__raw_writel((homography[2] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H13_FWD_0);
		__raw_writel((homography[3] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H21_FWD_0);
		__raw_writel((homography[4] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H22_FWD_0);
		__raw_writel((homography[5] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H23_FWD_0);
		__raw_writel((homography[6] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H31_FWD_0);
		__raw_writel((homography[7] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H32_FWD_0);
		break;
	case 1:
		__raw_writel((homography[0] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H11_FWD_1);
		__raw_writel((homography[1] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H12_FWD_1);
		__raw_writel((homography[2] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H13_FWD_1);
		__raw_writel((homography[3] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H21_FWD_1);
		__raw_writel((homography[4] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H22_FWD_1);
		__raw_writel((homography[5] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H23_FWD_1);
		__raw_writel((homography[6] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H31_FWD_1);
		__raw_writel((homography[7] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H32_FWD_1);
		break;
	case 2:
		__raw_writel((homography[0] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H11_FWD_2);
		__raw_writel((homography[1] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H12_FWD_2);
		__raw_writel((homography[2] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H13_FWD_2);
		__raw_writel((homography[3] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H21_FWD_2);
		__raw_writel((homography[4] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H22_FWD_2);
		__raw_writel((homography[5] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H23_FWD_2);
		__raw_writel((homography[6] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H31_FWD_2);
		__raw_writel((homography[7] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H32_FWD_2);
		break;
	case 3:
		__raw_writel((homography[0] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H11_FWD_3);
		__raw_writel((homography[1] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H12_FWD_3);
		__raw_writel((homography[2] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H13_FWD_3);
		__raw_writel((homography[3] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H21_FWD_3);
		__raw_writel((homography[4] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H22_FWD_3);
		__raw_writel((homography[5] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H23_FWD_3);
		__raw_writel((homography[6] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H31_FWD_3);
		__raw_writel((homography[7] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H32_FWD_3);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(svdw_set_forward_homography);

void svdw_set_backward_homography(u32 id, u32 homography[])
{
	switch (id) {
	case 0:
		__raw_writel((homography[0] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H11_BWD_0);
		__raw_writel((homography[1] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H12_BWD_0);
		__raw_writel((homography[2] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H13_BWD_0);
		__raw_writel((homography[3] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H21_BWD_0);
		__raw_writel((homography[4] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H22_BWD_0);
		__raw_writel((homography[5] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H23_BWD_0);
		__raw_writel((homography[6] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H31_BWD_0);
		__raw_writel((homography[7] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H32_BWD_0);
		break;
	case 1:
		__raw_writel((homography[0] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H11_BWD_1);
		__raw_writel((homography[1] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H12_BWD_1);
		__raw_writel((homography[2] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H13_BWD_1);
		__raw_writel((homography[3] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H21_BWD_1);
		__raw_writel((homography[4] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H22_BWD_1);
		__raw_writel((homography[5] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H23_BWD_1);
		__raw_writel((homography[6] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H31_BWD_1);
		__raw_writel((homography[7] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H32_BWD_1);
		break;
	case 2:
		__raw_writel((homography[0] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H11_BWD_2);
		__raw_writel((homography[1] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H12_BWD_2);
		__raw_writel((homography[2] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H13_BWD_2);
		__raw_writel((homography[3] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H21_BWD_2);
		__raw_writel((homography[4] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H22_BWD_2);
		__raw_writel((homography[5] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H23_BWD_2);
		__raw_writel((homography[6] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H31_BWD_2);
		__raw_writel((homography[7] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H32_BWD_2);
		break;
	case 3:
		__raw_writel((homography[0] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H11_BWD_3);
		__raw_writel((homography[1] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H12_BWD_3);
		__raw_writel((homography[2] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H13_BWD_3);
		__raw_writel((homography[3] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H21_BWD_3);
		__raw_writel((homography[4] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H22_BWD_3);
		__raw_writel((homography[5] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H23_BWD_3);
		__raw_writel((homography[6] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H31_BWD_3);
		__raw_writel((homography[7] & SVDW_HOMOGRAPHY_MASK),
			     pSVDW_reg + SVDW_HOMOGRAPHY_H32_BWD_3);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(svdw_set_backward_homography);

void svdw_set_eof_interrupt_delay(u32 patch_id, u32 eof_interrupt_delay)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_EOF_INTERRUPT_DELAY_0);
	u32 value = value = (eof_interrupt_delay & SVDW_EOF_INTERRUPT_DELAY_MASK);
	__raw_writel(value, addr);
}
EXPORT_SYMBOL(svdw_set_eof_interrupt_delay);

void svdw_set_eof_interrupt_clear_time(u32 patch_id, u32 eof_interrupt_clear_time)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_EOF_INTERRUPT_CLEAR_TIME_0);
	u32 value = value = (eof_interrupt_clear_time & SVDW_EOF_INTERRUPT_CLEAR_TIME_MASK);
	__raw_writel(value, addr);
}
EXPORT_SYMBOL(svdw_set_eof_interrupt_clear_time);

void svdw_get_processing_status(u32 patch_id, u32 *status)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_CAM_0_STATUS);
	*status = __raw_readl(addr);
}
EXPORT_SYMBOL(svdw_get_processing_status);

void svdw_get_cam_active(u32 patch_id, u32 *is_active)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_CAM_0_ACTIVE);
	*is_active = __raw_readl(addr);
}
EXPORT_SYMBOL(svdw_get_cam_active);

void svdw_clear_processing_status(u32 patch_id, u32 mask)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_CAM_0_CLEAR);
	__raw_writel(mask, addr);
}
EXPORT_SYMBOL(svdw_clear_processing_status);

void svdw_clear_eof_interrupt(u32 patch_id, u32 mask)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_EOF_INTERRUPT_CLEAR_0);
	__raw_writel(mask, addr);
}
EXPORT_SYMBOL(svdw_clear_eof_interrupt);

void svdw_update(u32 patch_id)
{
	void *addr = pSVDW_reg + SVDW_REG_OFFS(patch_id, SVDW_CAM_0_UPDATE);
	__raw_writel(0x1U, addr);
}
EXPORT_SYMBOL(svdw_update);

int svdw_set_ireq_mask(const struct device *pdev, u32 set)
{
	/*
	 * set 1 : IREQ Masked(interrupt disable),
	 * set 0 : IREQ UnMasked(interrput enable)
	 */
	int ret = 0;
	u32 value = 0U;
	u32 mask = SVDW_INT_RDONE_MASK;

	value = (__raw_readl(pCFG_reg + CAM_IREQ_MSK) & ~(mask));

	if (set == 0U) {
		/* Prevent KCS warning */
		value &= ~mask;
	} else {
		/* Prevent KCS warining */
		value |= mask;
	}

	__raw_writel(value, pCFG_reg + CAM_IREQ_MSK);

	return ret;
}
EXPORT_SYMBOL(svdw_set_ireq_mask);

/* VIN Get Interrupt Status
 *	- VIN_INT
 *		. return [ 3: 2]
 *	Not Used, Please use the alternative functions in vioc_intr.c
 */
void svdw_get_status(u32 *status)
{
	// *status = __raw_readl(pCFG_reg + SVDW_INT) & SVDW_INT_0_MASK;
	// *status = __raw_readl(pSVDW_reg + SVDW_CAM_0_STATUS) & SVDW_CAM_STATUS_FRAME_ACTIVE_MASK;
	*status = __raw_readl(pSVDW_reg + SVDW_CAM_0_STATUS);
}
EXPORT_SYMBOL(svdw_get_status);

void svdw_set_register(u32 offset, u32 val)
{
	writel(val, pSVDW_reg + offset);
}
EXPORT_SYMBOL(svdw_set_register);

void tcc_svdw_parse_cfg_and_set_regs(struct tcc_svdw_device *p_svdw, const struct firmware *fw)
{
	struct device *p_dev = NULL;
	struct device_node *config_node;
	void *fw_data = NULL;
	u32 fw_size = 0U;
	struct property *prop;
	const __be32 *p;
	int v;
	int offset = -1;

	p_dev = &p_svdw->pdev->dev;

	fw_size = max_t(uint, fw->size, 4096U);
	fw_data = kzalloc(fw_size, GFP_KERNEL);
	if (fw_data == NULL) {
		dev_err(p_dev, "Failed to allocate memory to load the firmware.\n");
		return;
	}
	memcpy(fw_data, fw->data, fw->size);
	of_fdt_unflatten_tree(fw_data, NULL, &p_svdw->cfg_node);
	kfree(fw_data);

	for_each_child_of_node(p_svdw->cfg_node, config_node) {
		of_property_for_each_u32 (config_node, "params", prop, p, v) {
			if (offset == -1) {
				offset = v;
			} else {
				__raw_writel(v, pSVDW_reg + offset);
				offset = -1;
			}
		}
	}
}
EXPORT_SYMBOL(tcc_svdw_parse_cfg_and_set_regs);

MODULE_LICENSE("GPL");
