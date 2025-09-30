// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *      tcc-dewarp-odw.c  --  Telechips On-the-Fly ODW Path Driver
 *
 *      Copyright (C) 2005-2010
 *          Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 ******************************************************************************


 *   Modified by Telechips Inc.


 *   Modified date : 2020


 *   Description : Driver management


 *****************************************************************************/

#include <linux/module.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/firmware.h>
#include "../tcc-dewarp-video.h"
#include "tcc-dewarp-odw.h"

static void __iomem *pODW_reg;
static void __iomem *pCFG_reg;

u32 dewarp_odw_get_ip_version(u32 id)
{
	return __raw_readl(pODW_reg + ODW_IP_VERSION);
}

u32 dewarp_odw_get_dummy_data_status(u32 id)
{
	u32 offset = 0;

	switch (id) {
	case 0:
		offset = ODW_DUMMY_WRITE_0;
		break;
	case 1:
		offset = ODW_DUMMY_WRITE_1;
		break;
	case 2:
		offset = ODW_DUMMY_WRITE_2;
		break;
	case 3:
		offset = ODW_DUMMY_WRITE_3;
		break;
	default:
		break;
	}

	return (__raw_readl(pODW_reg + offset) & ODW_DUMMY_WRITE_WREADY_MASK);
}

void dewarp_odw_set_dummy_data(u32 id, u32 start)
{
	u32 offset = 0;
	u32 value = 0;

	switch (id) {
	case 0:
		offset = ODW_DUMMY_WRITE_0;
		break;
	case 1:
		offset = ODW_DUMMY_WRITE_1;
		break;
	case 2:
		offset = ODW_DUMMY_WRITE_2;
		break;
	case 3:
		offset = ODW_DUMMY_WRITE_3;
		break;
	default:
		break;
	}

	value = (__raw_readl(pODW_reg + offset) &
		 ~(ODW_DUMMY_WRITE_WSTART_MASK));
	value |= (start << ODW_DUMMY_WRITE_WSTART_SHIFT);

	__raw_writel(value, pODW_reg + offset);
}

void dewarp_odw_set_input_size(u32 id, u32 width, u32 height)
{
	u32 offset = 0;
	u32 value = 0;

	switch (id) {
	case 0:
		offset = ODW_INPUT_SIZE_0;
		break;
	case 1:
		offset = ODW_INPUT_SIZE_1;
		break;
	case 2:
		offset = ODW_INPUT_SIZE_2;
		break;
	case 3:
		offset = ODW_INPUT_SIZE_3;
		break;
	default:
		break;
	}

	value = (((height & 0xFFFU) << ODW_INPUT_SIZE_HEIGHT_SHIFT) |
		 ((width & 0xFFFU) << ODW_INPUT_SIZE_WIDTH_SHIFT));

	__raw_writel(value, pODW_reg + offset);
}
EXPORT_SYMBOL(dewarp_odw_set_input_size);

void dewarp_odw_set_output_size(u32 id, u32 width, u32 height)
{
	u32 offset = 0;
	u32 value = 0;

	switch (id) {
	case 0:
		offset = ODW_PATCH_SIZE_0;
		break;
	case 1:
		offset = ODW_PATCH_SIZE_1;
		break;
	case 2:
		offset = ODW_PATCH_SIZE_2;
		break;
	case 3:
		offset = ODW_PATCH_SIZE_3;
		break;
	default:
		break;
	}

	value = (((height & 0xFFFU) << ODW_DEWARP_SIZE_HEIGHT_SHIFT) |
		 ((width & 0xFFFU) << ODW_DEWARP_SIZE_WIDTH_SHIFT));

	__raw_writel(value, pODW_reg + offset);
}
EXPORT_SYMBOL(dewarp_odw_set_output_size);

void dewarp_odw_set_input_format(u32 id, u32 format, u32 ir_enable)
{
	u32 offset = 0;
	u32 value = 0;

	if (ir_enable != 0U) {
		/* error */
		(void)pr_err("[INF][ODW-%d] does not support ir channel\n", id);
	} else {
		switch (id) {
		case 0:
			offset = ODW_IMAGE_FORMAT_0;
			break;
		case 1:
			offset = ODW_IMAGE_FORMAT_1;
			break;
		case 2:
			offset = ODW_IMAGE_FORMAT_2;
			break;
		case 3:
			offset = ODW_IMAGE_FORMAT_3;
			break;
		default:
			break;
		}

		value = (__raw_readl(pODW_reg + offset) &
			 ~(ODW_IMAGE_FORMAT_INPUT_FORMAT_MASK));
		value |= (format << ODW_IMAGE_FORMAT_INPUT_FORMAT_SHIFT);
		__raw_writel(value, pODW_reg + offset);
	}
}
EXPORT_SYMBOL(dewarp_odw_set_input_format);

void dewarp_odw_set_input_fisheye(u32 id, u32 is_fisheye)
{
	u32 offset = 0;
	u32 value = 0;

	switch (id) {
	case 0:
		offset = ODW_IS_FISHEYE_0;
		break;
	case 1:
		offset = ODW_IS_FISHEYE_1;
		break;
	case 2:
		offset = ODW_IS_FISHEYE_2;
		break;
	case 3:
		offset = ODW_IS_FISHEYE_3;
		break;
	default:
		break;
	}

	value = is_fisheye & ODW_IS_FISHEYE_MASK;

	__raw_writel(value, pODW_reg + offset);
}
EXPORT_SYMBOL(dewarp_odw_set_input_fisheye);

void dewarp_odw_set_output_format(u32 id, u32 format)
{
	u32 offset = 0;
	u32 value = 0;

	switch (id) {
	case 0:
		offset = ODW_IMAGE_FORMAT_0;
		break;
	case 1:
		offset = ODW_IMAGE_FORMAT_1;
		break;
	default:
		break;
	}

	value = (__raw_readl(pODW_reg + offset) &
		 (~(ODW_IMAGE_FORMAT_PATCH_FORMAT_MASK) &
		  ~(ODW_IMAGE_FORMAT_BYPASS_FORMAT_MASK)));

	value |= ((format << ODW_IMAGE_FORMAT_PATCH_FORMAT_SHIFT) |
		  (format << ODW_IMAGE_FORMAT_BYPASS_FORMAT_SHIFT));

	__raw_writel(value, pODW_reg + offset);
}
EXPORT_SYMBOL(dewarp_odw_set_output_format);

void dewarp_odw_set_output_address(u32 id, u32 addresses[], u32 ir_enable)
{
	if (ir_enable != 0U) {
		/* error */
		(void)pr_err("[INF][ODW-%d] does not support ir channel\n", id);
	} else {
		/* TODO: set patch_address for dewarping and blending */
		switch (id) {
		case 0:

			__raw_writel(addresses[0],
				     pODW_reg + ODW_BYPASS_ADDRESS_L_0);
			__raw_writel(addresses[1],
				     pODW_reg + ODW_BYPASS_ADDRESS_C_0);
			break;
		case 1:

			__raw_writel(addresses[0],
				     pODW_reg + ODW_BYPASS_ADDRESS_L_1);
			__raw_writel(addresses[1],
				     pODW_reg + ODW_BYPASS_ADDRESS_C_1);
			break;
		case 2:

			__raw_writel(addresses[0],
				     pODW_reg + ODW_BYPASS_ADDRESS_L_2);
			__raw_writel(addresses[1],
				     pODW_reg + ODW_BYPASS_ADDRESS_C_2);
			break;
		case 3:

			__raw_writel(addresses[0],
				     pODW_reg + ODW_BYPASS_ADDRESS_L_3);
			__raw_writel(addresses[1],
				     pODW_reg + ODW_BYPASS_ADDRESS_C_3);
			break;
		default:
			break;
		}
	}
}
EXPORT_SYMBOL(dewarp_odw_set_output_address);

void dewarp_odw_set_output_stride(uint id, uint strides[])
{
	u32 values[3];

	switch (id) {
	case 0:
		values[0] |= (strides[0] << 8U);
		values[1] |= (strides[1] << 8U);

		__raw_writel(values[0], pODW_reg + ODW_BYPASS_STRIDE_L_0);
		__raw_writel(values[1], pODW_reg + ODW_BYPASS_STRIDE_C_0);
		break;
	case 1:
		values[0] |= (strides[0] << 8U);
		values[1] |= (strides[1] << 8U);

		__raw_writel(values[0], pODW_reg + ODW_BYPASS_STRIDE_L_1);
		__raw_writel(values[1], pODW_reg + ODW_BYPASS_STRIDE_C_1);
		break;
	case 2:
		values[0] |= (strides[0] << 8U);
		values[1] |= (strides[1] << 8U);

		__raw_writel(values[0], pODW_reg + ODW_BYPASS_STRIDE_L_2);
		__raw_writel(values[1], pODW_reg + ODW_BYPASS_STRIDE_C_2);
		break;
	case 3:
		values[0] |= (strides[0] << 8U);
		values[1] |= (strides[1] << 8U);

		__raw_writel(values[0], pODW_reg + ODW_BYPASS_STRIDE_L_3);
		__raw_writel(values[1], pODW_reg + ODW_BYPASS_STRIDE_C_3);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(dewarp_odw_set_output_stride);

void dewarp_odw_set_block_config(u32 id, u32 offset_x, u32 offset_y,
				 u32 interval_x, uint interval_y)
{
	u32 offset = 0;
	u32 value = 0;

	switch (id) {
	case 0:
		offset = ODW_BLOCK_CONFIG_0;
		break;
	case 1:
		offset = ODW_BLOCK_CONFIG_1;
		break;
	case 2:
		offset = ODW_BLOCK_CONFIG_2;
		break;
	case 3:
		offset = ODW_BLOCK_CONFIG_3;
		break;
	default:
		break;
	}

	value = (__raw_readl(pODW_reg + offset) &
		 ~(ODW_BLOCK_CONFIG_BLOCK_OFFSET_Y_MASK) &
		 ~(ODW_BLOCK_CONFIG_BLOCK_OFFSET_X_MASK) &
		 ~(ODW_BLOCK_CONFIG_BLOCK_INTERVAL_Y_MASK) &
		 ~(ODW_BLOCK_CONFIG_BLOCK_INTERVAL_X_MASK));
	value |= (offset_y << ODW_BLOCK_CONFIG_BLOCK_OFFSET_Y_SHIFT);
	value |= (offset_x << ODW_BLOCK_CONFIG_BLOCK_OFFSET_X_SHIFT);
	value |= (interval_y << ODW_BLOCK_CONFIG_BLOCK_INTERVAL_Y_SHIFT);
	value |= (interval_x << ODW_BLOCK_CONFIG_BLOCK_INTERVAL_X_SHIFT);

	__raw_writel(value, pODW_reg + offset);
}
EXPORT_SYMBOL(dewarp_odw_set_block_config);

void dewarp_odw_set_filter(u32 id, u32 firs[], u32 iirs[])
{
	u32 fir = 0;
	u32 iir = 0;

	fir = ((firs[3] << ODW_FILTER_KERNEL_FIR_3_SHIFT) |
	       (firs[2] << ODW_FILTER_KERNEL_FIR_2_SHIFT) |
	       (firs[1] << ODW_FILTER_KERNEL_FIR_1_SHIFT) |
	       (firs[0] << ODW_FILTER_KERNEL_FIR_0_SHIFT));

	iir = ((iirs[2] << ODW_FILTER_KERNEL_IIR_2_SHIFT) |
	       (iirs[1] << ODW_FILTER_KERNEL_IIR_1_SHIFT) |
	       (iirs[0] << ODW_FILTER_KERNEL_IIR_0_SHIFT) |
	       (firs[4] << ODW_FILTER_KERNEL_FIR_4_SHIFT));

	switch (id) {
	case 0:
		__raw_writel(fir, pODW_reg + ODW_FILTER_KERNEL_0_0);
		__raw_writel(iir, pODW_reg + ODW_FILTER_KERNEL_1_0);
		break;
	case 1:
		__raw_writel(fir, pODW_reg + ODW_FILTER_KERNEL_0_1);
		__raw_writel(iir, pODW_reg + ODW_FILTER_KERNEL_1_1);
		break;
	case 2:
		__raw_writel(fir, pODW_reg + ODW_FILTER_KERNEL_0_2);
		__raw_writel(iir, pODW_reg + ODW_FILTER_KERNEL_1_2);
		break;
	case 3:
		__raw_writel(fir, pODW_reg + ODW_FILTER_KERNEL_0_3);
		__raw_writel(iir, pODW_reg + ODW_FILTER_KERNEL_1_3);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(dewarp_odw_set_filter);

void dewarp_odw_set_background_color(u32 id, u32 color)
{
	__raw_writel(color, pODW_reg + ODW_DEFAULT_COLOR);
}
EXPORT_SYMBOL(dewarp_odw_set_background_color);

void dewarp_odw_set_cam_matrix(u32 id, u32 cam_mat[])
{
	switch (id) {
	case 0:

		__raw_writel((cam_mat[0] & ODW_CAM_MAT_MASK),
			     pODW_reg + ODW_CAM_MAT_FX_0);
		__raw_writel((cam_mat[1] & ODW_CAM_MAT_MASK),
			     pODW_reg + ODW_CAM_MAT_SX_0);
		__raw_writel((cam_mat[2] & ODW_CAM_MAT_MASK),
			     pODW_reg + ODW_CAM_MAT_CX_0);
		__raw_writel((cam_mat[3] & ODW_CAM_MAT_MASK),
			     pODW_reg + ODW_CAM_MAT_FY_0);
		__raw_writel((cam_mat[4] & ODW_CAM_MAT_MASK),
			     pODW_reg + ODW_CAM_MAT_CY_0);
		break;
	case 1:

		__raw_writel((cam_mat[0] & ODW_CAM_MAT_MASK),
			     pODW_reg + ODW_CAM_MAT_FX_1);
		__raw_writel((cam_mat[1] & ODW_CAM_MAT_MASK),
			     pODW_reg + ODW_CAM_MAT_SX_1);
		__raw_writel((cam_mat[2] & ODW_CAM_MAT_MASK),
			     pODW_reg + ODW_CAM_MAT_CX_1);
		__raw_writel((cam_mat[3] & ODW_CAM_MAT_MASK),
			     pODW_reg + ODW_CAM_MAT_FY_1);
		__raw_writel((cam_mat[4] & ODW_CAM_MAT_MASK),
			     pODW_reg + ODW_CAM_MAT_CY_1);
		break;
	case 2:

		__raw_writel((cam_mat[0] & ODW_CAM_MAT_MASK),
			     pODW_reg + ODW_CAM_MAT_FX_2);
		__raw_writel((cam_mat[1] & ODW_CAM_MAT_MASK),
			     pODW_reg + ODW_CAM_MAT_SX_2);
		__raw_writel((cam_mat[2] & ODW_CAM_MAT_MASK),
			     pODW_reg + ODW_CAM_MAT_CX_2);
		__raw_writel((cam_mat[3] & ODW_CAM_MAT_MASK),
			     pODW_reg + ODW_CAM_MAT_FY_2);
		__raw_writel((cam_mat[4] & ODW_CAM_MAT_MASK),
			     pODW_reg + ODW_CAM_MAT_CY_2);
		break;
	case 3:

		__raw_writel((cam_mat[0] & ODW_CAM_MAT_MASK),
			     pODW_reg + ODW_CAM_MAT_FX_3);
		__raw_writel((cam_mat[1] & ODW_CAM_MAT_MASK),
			     pODW_reg + ODW_CAM_MAT_SX_3);
		__raw_writel((cam_mat[2] & ODW_CAM_MAT_MASK),
			     pODW_reg + ODW_CAM_MAT_CX_3);
		__raw_writel((cam_mat[3] & ODW_CAM_MAT_MASK),
			     pODW_reg + ODW_CAM_MAT_FY_3);
		__raw_writel((cam_mat[4] & ODW_CAM_MAT_MASK),
			     pODW_reg + ODW_CAM_MAT_CY_3);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(dewarp_odw_set_cam_matrix);

void dewarp_odw_set_forward_dist_coeff(u32 id, u32 dist_coeff[])
{
	switch (id) {
	case 0:

		__raw_writel((dist_coeff[0] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K1_FWD_0);
		__raw_writel((dist_coeff[1] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K2_FWD_0);
		__raw_writel((dist_coeff[2] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K3_FWD_0);
		__raw_writel((dist_coeff[3] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K4_FWD_0);
		break;
	case 1:

		__raw_writel((dist_coeff[0] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K1_FWD_1);
		__raw_writel((dist_coeff[1] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K2_FWD_1);
		__raw_writel((dist_coeff[2] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K3_FWD_1);
		__raw_writel((dist_coeff[3] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K4_FWD_1);
		break;
	case 2:

		__raw_writel((dist_coeff[0] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K1_FWD_2);
		__raw_writel((dist_coeff[1] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K2_FWD_2);
		__raw_writel((dist_coeff[2] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K3_FWD_2);
		__raw_writel((dist_coeff[3] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K4_FWD_2);
		break;
	case 3:

		__raw_writel((dist_coeff[0] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K1_FWD_3);
		__raw_writel((dist_coeff[1] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K2_FWD_3);
		__raw_writel((dist_coeff[2] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K3_FWD_3);
		__raw_writel((dist_coeff[3] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K4_FWD_3);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(dewarp_odw_set_forward_dist_coeff);

void dewarp_odw_set_backward_dist_coeff(u32 id, u32 dist_coeff[])
{
	switch (id) {
	case 0:

		__raw_writel((dist_coeff[0] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K1_BWD_0);
		__raw_writel((dist_coeff[1] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K2_BWD_0);
		__raw_writel((dist_coeff[2] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K3_BWD_0);
		__raw_writel((dist_coeff[3] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K4_BWD_0);
		break;
	case 1:

		__raw_writel((dist_coeff[0] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K1_BWD_1);
		__raw_writel((dist_coeff[1] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K2_BWD_1);
		__raw_writel((dist_coeff[2] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K3_BWD_1);
		__raw_writel((dist_coeff[3] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K4_BWD_1);
		break;
	case 2:

		__raw_writel((dist_coeff[0] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K1_BWD_2);
		__raw_writel((dist_coeff[1] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K2_BWD_2);
		__raw_writel((dist_coeff[2] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K3_BWD_2);
		__raw_writel((dist_coeff[3] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K4_BWD_2);
		break;
	case 3:

		__raw_writel((dist_coeff[0] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K1_BWD_3);
		__raw_writel((dist_coeff[1] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K2_BWD_3);
		__raw_writel((dist_coeff[2] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K3_BWD_3);
		__raw_writel((dist_coeff[3] & ODW_DIST_COEFF_MASK),
			     pODW_reg + ODW_DIST_COEFF_K4_BWD_3);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(dewarp_odw_set_backward_dist_coeff);

void dewarp_odw_set_forward_homography(u32 id, u32 homography[])
{
	switch (id) {
	case 0:

		__raw_writel((homography[0] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H11_FWD_0);
		__raw_writel((homography[1] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H12_FWD_0);
		__raw_writel((homography[2] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H13_FWD_0);
		__raw_writel((homography[3] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H21_FWD_0);
		__raw_writel((homography[4] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H22_FWD_0);
		__raw_writel((homography[5] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H23_FWD_0);
		__raw_writel((homography[6] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H31_FWD_0);
		__raw_writel((homography[7] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H32_FWD_0);
		break;
	case 1:

		__raw_writel((homography[0] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H11_FWD_1);
		__raw_writel((homography[1] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H12_FWD_1);
		__raw_writel((homography[2] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H13_FWD_1);
		__raw_writel((homography[3] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H21_FWD_1);
		__raw_writel((homography[4] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H22_FWD_1);
		__raw_writel((homography[5] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H23_FWD_1);
		__raw_writel((homography[6] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H31_FWD_1);
		__raw_writel((homography[7] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H32_FWD_1);
		break;
	case 2:

		__raw_writel((homography[0] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H11_FWD_2);
		__raw_writel((homography[1] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H12_FWD_2);
		__raw_writel((homography[2] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H13_FWD_2);
		__raw_writel((homography[3] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H21_FWD_2);
		__raw_writel((homography[4] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H22_FWD_2);
		__raw_writel((homography[5] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H23_FWD_2);
		__raw_writel((homography[6] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H31_FWD_2);
		__raw_writel((homography[7] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H32_FWD_2);
		break;
	case 3:

		__raw_writel((homography[0] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H11_FWD_3);
		__raw_writel((homography[1] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H12_FWD_3);
		__raw_writel((homography[2] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H13_FWD_3);
		__raw_writel((homography[3] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H21_FWD_3);
		__raw_writel((homography[4] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H22_FWD_3);
		__raw_writel((homography[5] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H23_FWD_3);
		__raw_writel((homography[6] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H31_FWD_3);
		__raw_writel((homography[7] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H32_FWD_3);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(dewarp_odw_set_forward_homography);

void dewarp_odw_set_backward_homography(u32 id, u32 homography[])
{
	switch (id) {
	case 0:

		__raw_writel((homography[0] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H11_BWD_0);
		__raw_writel((homography[1] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H12_BWD_0);
		__raw_writel((homography[2] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H13_BWD_0);
		__raw_writel((homography[3] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H21_BWD_0);
		__raw_writel((homography[4] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H22_BWD_0);
		__raw_writel((homography[5] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H23_BWD_0);
		__raw_writel((homography[6] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H31_BWD_0);
		__raw_writel((homography[7] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H32_BWD_0);
		break;
	case 1:

		__raw_writel((homography[0] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H11_BWD_1);
		__raw_writel((homography[1] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H12_BWD_1);
		__raw_writel((homography[2] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H13_BWD_1);
		__raw_writel((homography[3] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H21_BWD_1);
		__raw_writel((homography[4] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H22_BWD_1);
		__raw_writel((homography[5] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H23_BWD_1);
		__raw_writel((homography[6] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H31_BWD_1);
		__raw_writel((homography[7] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H32_BWD_1);
		break;
	case 2:

		__raw_writel((homography[0] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H11_BWD_2);
		__raw_writel((homography[1] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H12_BWD_2);
		__raw_writel((homography[2] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H13_BWD_2);
		__raw_writel((homography[3] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H21_BWD_2);
		__raw_writel((homography[4] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H22_BWD_2);
		__raw_writel((homography[5] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H23_BWD_2);
		__raw_writel((homography[6] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H31_BWD_2);
		__raw_writel((homography[7] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H32_BWD_2);
		break;
	case 3:

		__raw_writel((homography[0] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H11_BWD_3);
		__raw_writel((homography[1] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H12_BWD_3);
		__raw_writel((homography[2] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H13_BWD_3);
		__raw_writel((homography[3] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H21_BWD_3);
		__raw_writel((homography[4] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H22_BWD_3);
		__raw_writel((homography[5] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H23_BWD_3);
		__raw_writel((homography[6] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H31_BWD_3);
		__raw_writel((homography[7] & ODW_HOMOGRAPHY_MASK),
			     pODW_reg + ODW_HOMOGRAPHY_H32_BWD_3);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(dewarp_odw_set_backward_homography);

void dewarp_odw_set_axi_config(u32 id, u32 max_ros, u32 max_wos, u32 max_rburst,
			       u32 max_wburst)
{
	u32 value = 0;

	value |= (max_ros << ODW_AXI_CONFIG_MAX_ROS_SHIFT) &
		 ODW_AXI_CONFIG_MAX_ROS_MASK;

	value |= (max_rburst << ODW_AXI_CONFIG_MAX_RBURST_SHIFT) &
		 ODW_AXI_CONFIG_MAX_RBURST_MASK;

	value |= (max_wos << ODW_AXI_CONFIG_MAX_WOS_SHIFT) &
		 ODW_AXI_CONFIG_MAX_WOS_MASK;

	value |= (max_wburst << ODW_AXI_CONFIG_MAX_WBURST_SHIFT) &
		 ODW_AXI_CONFIG_MAX_WBURST_MASK;

	__raw_writel(value, pODW_reg + ODW_AXI_CONFIG);
}
EXPORT_SYMBOL(dewarp_odw_set_axi_config);

void dewarp_odw_set_frame_time_cycle(u32 id, u32 frame_time_cycle)
{
	__raw_writel(frame_time_cycle, pODW_reg + ODW_FRAME_TIME_CYCLE);
}
EXPORT_SYMBOL(dewarp_odw_set_frame_time_cycle);

void dewarp_odw_set_frame_timeout_threshold(u32 id, u32 frame_timeout_thres)
{
	__raw_writel(frame_timeout_thres, pODW_reg + ODW_FRAME_TIMEOUT_THRES);
}
EXPORT_SYMBOL(dewarp_odw_set_frame_timeout_threshold);

void dewarp_odw_set_frame_fast_threshold(u32 id, u32 frame_fast_thres)
{
	__raw_writel(frame_fast_thres, pODW_reg + ODW_FRAME_FAST_THRES);
}
EXPORT_SYMBOL(dewarp_odw_set_frame_fast_threshold);

void dewarp_odw_set_frame_slow_threshold(u32 id, u32 frame_slow_thres)
{
	__raw_writel(frame_slow_thres, pODW_reg + ODW_FRAME_SLOW_THRES);
}
EXPORT_SYMBOL(dewarp_odw_set_frame_slow_threshold);

void dewarp_odw_set_frame_toggle_threshold(u32 id, u32 frame_toggle_thres)
{
	__raw_writel((frame_toggle_thres & ODW_FRAME_TOGGLE_THRES_MASK),
		     pODW_reg + ODW_FRAME_TOGGLE_THRES);
}
EXPORT_SYMBOL(dewarp_odw_set_frame_toggle_threshold);

void dewarp_odw_set_line_toggle_threshold(u32 id, u32 line_timeout_thres)
{
	__raw_writel((line_timeout_thres & ODW_LINE_TOGGLE_THRES_MASK),
		     pODW_reg + ODW_LINE_TOGGLE_THRES);
}
EXPORT_SYMBOL(dewarp_odw_set_line_toggle_threshold);

void dewarp_odw_set_max_toggle_threshold(u32 id, u32 max_toggle_thres)
{
	__raw_writel((max_toggle_thres & ODW_MAX_TOGGLE_CNT_MASK),
		     pODW_reg + ODW_MAX_TOGGLE_CNT);
}
EXPORT_SYMBOL(dewarp_odw_set_max_toggle_threshold);

void dewarp_odw_set_backward_scan_config(u32 id, u32 scan_xy_swap,
					 u32 scan_margin_x, uint scan_margin_y,
					 uint round_thres)
{
	u32 offset = 0;
	u32 value = 0;

	switch (id) {
	case 0:
		offset = ODW_BACKWARD_SCAN_CONFIG_0;
		break;
	case 1:
		offset = ODW_BACKWARD_SCAN_CONFIG_1;
		break;
	case 2:
		offset = ODW_BACKWARD_SCAN_CONFIG_2;
		break;
	case 3:
		offset = ODW_BACKWARD_SCAN_CONFIG_3;
		break;
	default:
		break;
	}

	value = (__raw_readl(pODW_reg + offset) &
		 ~(ODW_BACKWARD_SCAN_CONFIG_SCAN_XY_SWAP_MASK) &
		 ~(ODW_BACKWARD_SCAN_CONFIG_SCAN_MARGIN_Y_MASK) &
		 ~(ODW_BACKWARD_SCAN_CONFIG_SCAN_MARGIN_X_MASK) &
		 ~(ODW_BACKWARD_SCAN_CONFIG_ROUND_THRES_MASK));
	value |= (scan_xy_swap << ODW_BACKWARD_SCAN_CONFIG_SCAN_XY_SWAP_SHIFT);
	value |=
		(scan_margin_y << ODW_BACKWARD_SCAN_CONFIG_SCAN_MARGIN_Y_SHIFT);
	value |=
		(scan_margin_x << ODW_BACKWARD_SCAN_CONFIG_SCAN_MARGIN_X_SHIFT);
	value |= (round_thres << ODW_BACKWARD_SCAN_CONFIG_ROUND_THRES_SHIFT);

	__raw_writel(value, pODW_reg + offset);
}
EXPORT_SYMBOL(dewarp_odw_set_backward_scan_config);

void dewarp_odw_set_position_fifo_timeout_cnt(u32 id,
					      u32 position_fifo_timeout_cnt)
{
	u32 offset = 0;
	u32 value = 0;

	switch (id) {
	case 0:
		offset = ODW_POSITION_FIFO_TIMEOUT_THRES_0;
		break;
	case 1:
		offset = ODW_POSITION_FIFO_TIMEOUT_THRES_1;
		break;
	case 2:
		offset = ODW_POSITION_FIFO_TIMEOUT_THRES_2;
		break;
	case 3:
		offset = ODW_POSITION_FIFO_TIMEOUT_THRES_3;
		break;
	default:
		break;
	}

	value = (position_fifo_timeout_cnt &
		 ODW_POSITION_FIFO_TIMEOUT_THRES_MASK);

	__raw_writel(value, pODW_reg + offset);
}
EXPORT_SYMBOL(dewarp_odw_set_position_fifo_timeout_cnt);

void dewarp_odw_set_dewarp_fifo_timeout_cnt(u32 id, u32 dewarp_fifo_timeout_cnt)
{
	u32 bypass_offset = 0;
	u32 patch_offset = 0;
	u32 value = 0;

	switch (id) {
	case 0:
		bypass_offset = ODW_BYPASS_FIFO_TIMEOUT_THRES_0;
		patch_offset = ODW_PATCH_FIFO_TIMEOUT_THRES_0;
		break;
	case 1:
		bypass_offset = ODW_BYPASS_FIFO_TIMEOUT_THRES_1;
		patch_offset = ODW_PATCH_FIFO_TIMEOUT_THRES_1;
		break;
	case 2:
		bypass_offset = ODW_BYPASS_FIFO_TIMEOUT_THRES_2;
		patch_offset = ODW_PATCH_FIFO_TIMEOUT_THRES_2;
		break;
	case 3:
		bypass_offset = ODW_BYPASS_FIFO_TIMEOUT_THRES_3;
		patch_offset = ODW_PATCH_FIFO_TIMEOUT_THRES_3;
		break;
	default:
		break;
	}

	value = (dewarp_fifo_timeout_cnt & ODW_OUTPUT_FIFO_TIMEOUT_THRES_MASK);

	__raw_writel(value, pODW_reg + bypass_offset);
	__raw_writel(value, pODW_reg + patch_offset);
}
EXPORT_SYMBOL(dewarp_odw_set_dewarp_fifo_timeout_cnt);

void dewarp_odw_set_dewarp_buffer_wait_cnt(u32 id, u32 dewarp_buffer_wait_cnt)
{
	u32 bypass_offset = 0;
	u32 patch_offset = 0;
	u32 value = 0;

	switch (id) {
	case 0:
		bypass_offset = ODW_BYPASS_BUFFER_WAIT_TIME_0;
		patch_offset = ODW_PATCH_BUFFER_WAIT_TIME_0;
		break;
	case 1:
		bypass_offset = ODW_BYPASS_BUFFER_WAIT_TIME_1;
		patch_offset = ODW_PATCH_BUFFER_WAIT_TIME_1;
		break;
	case 2:
		bypass_offset = ODW_BYPASS_BUFFER_WAIT_TIME_2;
		patch_offset = ODW_PATCH_BUFFER_WAIT_TIME_2;
		break;
	case 3:
		bypass_offset = ODW_BYPASS_BUFFER_WAIT_TIME_3;
		patch_offset = ODW_PATCH_BUFFER_WAIT_TIME_3;
		break;
	default:
		break;
	}

	value = (dewarp_buffer_wait_cnt & ODW_OUTPUT_FIFO_TIMEOUT_THRES_MASK);

	__raw_writel(value, pODW_reg + bypass_offset);
	__raw_writel(value, pODW_reg + patch_offset);
}
EXPORT_SYMBOL(dewarp_odw_set_dewarp_buffer_wait_cnt);

void dewarp_odw_set_buffer_capacity(u32 id, u32 buffer_capacity)
{
	u32 offset = 0;
	u32 value = 0;

	switch (id) {
	case 0:
		offset = ODW_BUFFER_CAPACITY_0;
		break;
	case 1:
		offset = ODW_BUFFER_CAPACITY_1;
		break;
	case 2:
		offset = ODW_BUFFER_CAPACITY_2;
		break;
	case 3:
		offset = ODW_BUFFER_CAPACITY_3;
		break;
	default:
		break;
	}

	value = ((buffer_capacity
		  << ODW_BUFFER_CAPACITY_BYPASS_BUFFER_CAPACITY_SHIFT) |
		 (buffer_capacity
		  << ODW_BUFFER_CAPACITY_PATCH_BUFFER_CAPACITY_SHIFT));

	__raw_writel(value, pODW_reg + offset);
}
EXPORT_SYMBOL(dewarp_odw_set_buffer_capacity);

void dewarp_odw_set_eof_interrupt_delay(u32 id, u32 eof_interrupt_delay)
{
	u32 offset = 0;
	u32 value = 0;

	switch (id) {
	case 0:
		offset = ODW_EOF_INTERRUPT_DELAY_0;
		break;
	case 1:
		offset = ODW_EOF_INTERRUPT_DELAY_1;
		break;
	case 2:
		offset = ODW_EOF_INTERRUPT_DELAY_2;
		break;
	case 3:
		offset = ODW_EOF_INTERRUPT_DELAY_3;
		break;
	default:
		break;
	}

	value = (eof_interrupt_delay & ODW_EOF_INTERRUPT_DELAY_MASK);

	__raw_writel(value, pODW_reg + offset);
}
EXPORT_SYMBOL(dewarp_odw_set_eof_interrupt_delay);

void dewarp_odw_set_eof_interrupt_clear_time(u32 id,
					     u32 eof_interrupt_clear_time)
{
	u32 offset = 0;
	u32 value = 0;

	switch (id) {
	case 0:
		offset = ODW_EOF_INTERRUPT_CLEAR_TIME_0;
		break;
	case 1:
		offset = ODW_EOF_INTERRUPT_CLEAR_TIME_1;
		break;
	case 2:
		offset = ODW_EOF_INTERRUPT_CLEAR_TIME_2;
		break;
	case 3:
		offset = ODW_EOF_INTERRUPT_CLEAR_TIME_3;
		break;
	default:
		break;
	}

	value = (eof_interrupt_clear_time & ODW_EOF_INTERRUPT_CLEAR_TIME_MASK);

	__raw_writel(value, pODW_reg + offset);
}
EXPORT_SYMBOL(dewarp_odw_set_eof_interrupt_clear_time);

u32 dewarp_odw_get_processing_status(u32 id)
{
	u32 offset = 0;

	switch (id) {
	case 0:
		offset = ODW_CAM_0_STATUS;
		break;
	case 1:
		offset = ODW_CAM_1_STATUS;
		break;
	case 2:
		offset = ODW_CAM_2_STATUS;
		break;
	case 3:
		offset = ODW_CAM_3_STATUS;
		break;
	default:
		break;
	}

	return __raw_readl(pODW_reg + offset);
}
EXPORT_SYMBOL(dewarp_odw_get_processing_status);

void dewarp_odw_clear_processing_status(u32 id, u32 mask)
{
	u32 offset = 0;

	switch (id) {
	case 0:
		offset = ODW_CAM_0_CLEAR;
		break;
	case 1:
		offset = ODW_CAM_1_CLEAR;
		break;
	case 2:
		offset = ODW_CAM_2_CLEAR;
		break;
	case 3:
		offset = ODW_CAM_3_CLEAR;
		break;
	default:
		break;
	}

	__raw_writel(mask, pODW_reg + offset);
}
EXPORT_SYMBOL(dewarp_odw_clear_processing_status);

void dewarp_odw_clear_interrupt(u32 id)
{
	u32 offset = 0;
	u32 mask = 0;

	switch (id) {
	case 0:
		offset = ODW_EOF_INTERRUPT_CLEAR_0;
		break;
	case 1:
		offset = ODW_EOF_INTERRUPT_CLEAR_1;
		break;
	case 2:
		offset = ODW_EOF_INTERRUPT_CLEAR_2;
		break;
	case 3:
		offset = ODW_EOF_INTERRUPT_CLEAR_3;
		break;
	default:
		break;
	}

	mask = 0x1U;

	__raw_writel(mask, pODW_reg + offset);
}
EXPORT_SYMBOL(dewarp_odw_clear_interrupt);

void dewarp_odw_update(u32 id)
{
	u32 offset = 0;
	u32 value = 0;

	switch (id) {
	case 0:
		offset = ODW_CAM_0_UPDATE;
		break;
	case 1:
		offset = ODW_CAM_1_UPDATE;
		break;
	case 2:
		offset = ODW_CAM_2_UPDATE;
		break;
	case 3:
		offset = ODW_CAM_3_UPDATE;
		break;
	default:
		break;
	}

	value = 0x1U;

	__raw_writel(value, pODW_reg + offset);
}
EXPORT_SYMBOL(dewarp_odw_update);

u32 dewarp_odw_enable(u32 id, u32 dewarp_enable, u32 stream_enable, u32 bypass)
{
	u32 offset = 0;
	u32 value = 0;
	u32 chroma_interpolation = 0;
	u32 filter_enable = 0;
	u32 downsample_enable = 1;

	switch (id) {
	case 0:
		offset = ODW_ENABLE_0;
		break;
	case 1:
		offset = ODW_ENABLE_1;
		break;
	case 2:
		offset = ODW_ENABLE_2;
		break;
	case 3:
		offset = ODW_ENABLE_3;
		break;
	default:
		break;
	}

	if ((dewarp_enable == 1) && (bypass == 0)) {
		chroma_interpolation = 1;
		filter_enable = 1;
		downsample_enable = 0;
	}

	value = (__raw_readl(pODW_reg + offset) &
		 ~(ODW_ENABLE_DOWNSAMPLING_ENABLE_MASK) &
		 ~(ODW_ENABLE_FILTER_ENABLE_MASK) &
		 ~(ODW_ENABLE_VERTICAL_FLIP_MASK) &
		 ~(ODW_ENABLE_HORIZONTAL_FLIP_MASK) &
		 ~(ODW_ENABLE_CHROMA_INTERPOLATION_MASK) &
		 ~(ODW_ENABLE_DEWARP_ENABLE_MASK) &
		 ~(ODW_ENABLE_BYPASS_ENABLE_MASK) &
		 ~(ODW_ENABLE_STREAM_ENABLE_MASK));
	value |= (dewarp_enable << ODW_ENABLE_DEWARP_ENABLE_SHIFT);
	value |= (stream_enable << ODW_ENABLE_STREAM_ENABLE_SHIFT);
	value |= (bypass << ODW_ENABLE_BYPASS_ENABLE_SHIFT);
	value |=
		(chroma_interpolation << ODW_ENABLE_CHROMA_INTERPOLATION_SHIFT);
	value |= (filter_enable << ODW_ENABLE_FILTER_ENABLE_SHIFT);
	value |= (downsample_enable << ODW_ENABLE_DOWNSAMPLING_ENABLE_SHIFT);

	__raw_writel(value, pODW_reg + offset);

	return 0;
}
EXPORT_SYMBOL(dewarp_odw_enable);

s32 dewarp_set_ireq_mask(const struct device *pdev, u32 id, u32 set)
{
	/*
	 * set 1 : IREQ Masked(interrupt enable),
	 * set 0 : IREQ UnMasked(interrput disable)
	 */
	u32 value = 0U, mask = 0U;
	s32 ret = 0;

	switch (id) {
	case 0:
		mask = ODW_INT_0_MASK;
		break;
	case 1:
		mask = ODW_INT_1_MASK;
		break;
	case 2:
		mask = ODW_INT_2_MASK;
		break;
	case 3:
		mask = ODW_INT_3_MASK;
		break;
	default:
		loge(pdev, "invalid id(%d)\n", id);
		ret = -EINVAL;
		break;
	}

	if (ret == 0) {
		value = (__raw_readl(pCFG_reg + ODW_INT) & ~(mask));

		if (set == 1U) { /* Interrupt Enable*/
			/* Prevent KCS warning */
			value &= ~mask;
		} else {
			/* Prevent KCS warining */
			value |= mask;
		}

		__raw_writel(value, pCFG_reg + ODW_INT);
	}

	return ret;
}
EXPORT_SYMBOL(dewarp_set_ireq_mask);

void parse_cfg_and_set_regs(struct tcc_dewarp_stream *vstream,
			    const struct firmware *fw)
{
	struct device *p_dev = NULL;
	void *fw_data = NULL;
	char *p = NULL;
	u32 fw_size = 0U;
	u32 ch = 0U;
	u32 step = 0U;
	u32 offset = 0U;
	u32 regval = 0U;

	p_dev = stream_to_device(vstream);

	fw_size = max_t(uint, fw->size, 4096U);
	fw_data = kzalloc(fw_size, GFP_KERNEL);
	if (fw_data == NULL) {
		loge(p_dev,
		     "Failed to allocate memory to load the firmware.\n");
		return;
	}

	memcpy(fw_data, fw->data, fw->size);

	while ((p = strsep((char **)&fw_data, " \n")) != NULL) {
		if (!*p) {
			continue;
		}
		if (strncmp(p, "CH", 2U) == 0) {
			ch = (u32)simple_strtol(p + 2, NULL, 0);
			step = 1U;
		} else {
			if (step == 1U) {
				offset = (u32)simple_strtol(p, NULL, 0);
				step = 2U;
			} else if (step == 2U) {
				regval = (u32)simple_strtol(p, NULL, 0);
				writel(regval, pODW_reg + offset);
				logd(p_dev,
				     "regval = 0x%08x, offset = 0x%08x\n",
				     regval, offset);
			}
		}
	}

	kfree(fw_data);
}
EXPORT_SYMBOL(parse_cfg_and_set_regs);

/* VIN Get Interrupt Status
 *	- VIN_INT
 *		. return [ 3: 2]
 *	Not Used, Please use the alternative functions in vioc_intr.c
 */
void dewarp_get_status(u32 *status)
{
	// *status = __raw_readl(pCFG_reg + ODW_INT) & ODW_INT_0_MASK;
	// *status = __raw_readl(pODW_reg + ODW_CAM_0_STATUS) & ODW_CAM_STATUS_FRAME_ACTIVE_MASK;
	*status = __raw_readl(pODW_reg + ODW_CAM_0_STATUS);
}
EXPORT_SYMBOL(dewarp_get_status);

static int __init dewarp_odw_init(void)
{
	// u32 i;
	struct device_node *CamOdw_np;

	CamOdw_np = of_find_compatible_node(NULL, NULL, "telechips,cam_odw");
	if (CamOdw_np == NULL) {
		/* Prevent KCS warning */
		(void)pr_info("[INF][ODW] disabled\n");
	} else {
		pODW_reg = (void __iomem *)of_iomap(CamOdw_np, 0);
		if (pODW_reg != NULL) {
			/* Prevent KCS warning */
			(void)pr_info("[INF][ODW] cam-odw\n");
		}
	}

	CamOdw_np = of_find_compatible_node(NULL, NULL, "telechips,cam_cfg");
	if (CamOdw_np == NULL) {
		/* Prevent KCS warning */
		(void)pr_info("[INF][ODW] disabled\n");
	} else {
		pCFG_reg = (void __iomem *)of_iomap(CamOdw_np, 0);
		if (pCFG_reg != NULL) {
			/* Prevent KCS warning */
			(void)pr_info("[INF][ODW] cam-config\n");
		}
	}

	return 0;
}

module_init(dewarp_odw_init);
MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("SWITCH Driver");
MODULE_LICENSE("GPL");
