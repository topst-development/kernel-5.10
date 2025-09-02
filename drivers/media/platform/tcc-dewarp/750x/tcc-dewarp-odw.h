/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef DEWARP_ODW_H
#define DEWARP_ODW_H

/*
 * Register offset
 */
#define ODW_IP_VERSION					(0x000U)
#define ODW_DUMMY_WRITE_0				(0x008U)
#define ODW_DUMMY_WRITE_1				(0x00CU)
#define ODW_AXI_CONFIG					(0x010U)
#define ODW_FRAME_TIME_CYCLE				(0x014U)
#define ODW_FRAME_TIMEOUT_THRES				(0x018U)
#define ODW_FRAME_FAST_THRES				(0x01CU)
#define ODW_FRAME_SLOW_THRES				(0x020U)
#define ODW_FRAME_TOGGLE_THRES				(0x024U)
#define ODW_LINE_TOGGLE_THRES				(0x028U)
#define ODW_MAX_TOGGLE_CNT				(0x02CU)
#define ODW_ENABLE_0					(0x100U)
#define ODW_OUTPUT_ADDRESS_0_0				(0x104U)
#define ODW_OUTPUT_STRIDE_0_0				(0x108U)
#define ODW_OUTPUT_ADDRESS_1_0				(0x10CU)
#define ODW_OUTPUT_STRIDE_1_0				(0x110U)
#define ODW_OUTPUT_ADDRESS_2_0				(0x114U)
#define ODW_OUTPUT_STRIDE_2_0				(0x118U)
#define ODW_INPUT_SIZE_0				(0x11CU)
#define ODW_DEWARP_SIZE_0				(0x120U)
#define ODW_IMAGE_FORMAT_0				(0x124U)
#define ODW_BLOCK_CONFIG_0				(0x128U)
#define ODW_FILTER_KERNEL_0_0				(0x12CU)
#define ODW_FILTER_KERNEL_1_0				(0x130U)
#define ODW_DEFAULT_COLOR_0				(0x134U)
#define ODW_CAM_MAT_FX_0				(0x140U)
#define ODW_CAM_MAT_SX_0				(0x144U)
#define ODW_CAM_MAT_CX_0				(0x148U)
#define ODW_CAM_MAT_FY_0				(0x14CU)
#define ODW_CAM_MAT_CY_0				(0x150U)
#define ODW_DIST_COEFF_K1_FWD_0				(0x154U)
#define ODW_DIST_COEFF_K2_FWD_0				(0x158U)
#define ODW_DIST_COEFF_K3_FWD_0				(0x15CU)
#define ODW_DIST_COEFF_K4_FWD_0				(0x160U)
#define ODW_DIST_COEFF_K1_BWD_0				(0x164U)
#define ODW_DIST_COEFF_K2_BWD_0				(0x168U)
#define ODW_DIST_COEFF_K3_BWD_0				(0x16CU)
#define ODW_DIST_COEFF_K4_BWD_0				(0x170U)
#define ODW_HOMOGRAPHY_H11_FWD_0			(0x174U)
#define ODW_HOMOGRAPHY_H12_FWD_0			(0x178U)
#define ODW_HOMOGRAPHY_H13_FWD_0			(0x17CU)
#define ODW_HOMOGRAPHY_H21_FWD_0			(0x180U)
#define ODW_HOMOGRAPHY_H22_FWD_0			(0x184U)
#define ODW_HOMOGRAPHY_H23_FWD_0			(0x188U)
#define ODW_HOMOGRAPHY_H11_BWD_0			(0x194U)
#define ODW_HOMOGRAPHY_H12_BWD_0			(0x198U)
#define ODW_HOMOGRAPHY_H13_BWD_0			(0x19CU)
#define ODW_HOMOGRAPHY_H21_BWD_0			(0x1A0U)
#define ODW_HOMOGRAPHY_H22_BWD_0			(0x1A4U)
#define ODW_HOMOGRAPHY_H23_BWD_0			(0x1A8U)
#define ODW_IS_FISHEYE_0				(0x1B4U)
#define ODW_BACKWARD_SCAN_CONFIG_0			(0x1B8U)
#define ODW_POSITION_FIFO_TIMEOUT_THRES_0		(0x1C0U)
#define ODW_OUTPUT_FIFO_TIMEOUT_THRES_0			(0x1C4U)
#define ODW_OUTPUT_BUFFER_WAIT_TIME_0			(0x1C8U)
#define ODW_BUFFER_CAPACITY_0				(0x1CCU)
#define ODW_EOF_INTERRUPT_DELAY_0			(0x1D0U)
#define ODW_EOF_INTERRUPT_CLEAR_TIME_0			(0x1D4U)
#define ODW_CAM_0_STATUS				(0x1E0U)
#define ODW_CAM_0_CLEAR					(0x1E8U)
#define ODW_EOF_INTERRUPT_CLEAR_0			(0x1ECU)
#define ODW_CAM_0_UPDATE				(0x1F0U)
#define ODW_ENABLE_1					(0x200U)
#define ODW_OUTPUT_ADDRESS_0_1				(0x204U)
#define ODW_OUTPUT_STRIDE_0_1				(0x208U)
#define ODW_OUTPUT_ADDRESS_1_1				(0x20CU)
#define ODW_OUTPUT_STRIDE_1_1				(0x210U)
#define ODW_OUTPUT_ADDRESS_2_1				(0x214U)
#define ODW_OUTPUT_STRIDE_2_1				(0x218U)
#define ODW_INPUT_SIZE_1				(0x21CU)
#define ODW_DEWARP_SIZE_1				(0x220U)
#define ODW_IMAGE_FORMAT_1				(0x224U)
#define ODW_BLOCK_CONFIG_1				(0x228U)
#define ODW_FILTER_KERNEL_0_1				(0x22CU)
#define ODW_FILTER_KERNEL_1_1				(0x230U)
#define ODW_DEFAULT_COLOR_1				(0x234U)
#define ODW_CAM_MAT_FX_1				(0x240U)
#define ODW_CAM_MAT_SX_1				(0x244U)
#define ODW_CAM_MAT_CX_1				(0x248U)
#define ODW_CAM_MAT_FY_1				(0x24CU)
#define ODW_CAM_MAT_CY_1				(0x250U)
#define ODW_DIST_COEFF_K1_FWD_1				(0x254U)
#define ODW_DIST_COEFF_K2_FWD_1				(0x258U)
#define ODW_DIST_COEFF_K3_FWD_1				(0x25CU)
#define ODW_DIST_COEFF_K4_FWD_1				(0x260U)
#define ODW_DIST_COEFF_K1_BWD_1				(0x264U)
#define ODW_DIST_COEFF_K2_BWD_1				(0x268U)
#define ODW_DIST_COEFF_K3_BWD_1				(0x26CU)
#define ODW_DIST_COEFF_K4_BWD_1				(0x270U)
#define ODW_HOMOGRAPHY_H11_FWD_1			(0x274U)
#define ODW_HOMOGRAPHY_H12_FWD_1			(0x278U)
#define ODW_HOMOGRAPHY_H13_FWD_1			(0x27CU)
#define ODW_HOMOGRAPHY_H21_FWD_1			(0x280U)
#define ODW_HOMOGRAPHY_H22_FWD_1			(0x284U)
#define ODW_HOMOGRAPHY_H23_FWD_1			(0x288U)
#define ODW_HOMOGRAPHY_H11_BWD_1			(0x294U)
#define ODW_HOMOGRAPHY_H12_BWD_1			(0x298U)
#define ODW_HOMOGRAPHY_H13_BWD_1			(0x29CU)
#define ODW_HOMOGRAPHY_H21_BWD_1			(0x2A0U)
#define ODW_HOMOGRAPHY_H22_BWD_1			(0x2A4U)
#define ODW_HOMOGRAPHY_H23_BWD_1			(0x2A8U)
#define ODW_IS_FISHEYE_1				(0x2B4U)
#define ODW_BACKWARD_SCAN_CONFIG_1			(0x2B8U)
#define ODW_POSITION_FIFO_TIMEOUT_THRES_1		(0x2C0U)
#define ODW_OUTPUT_FIFO_TIMEOUT_THRES_1			(0x2C4U)
#define ODW_OUTPUT_BUFFER_WAIT_TIME_1			(0x2C8U)
#define ODW_BUFFER_CAPACITY_1				(0x2CCU)
#define ODW_EOF_INTERRUPT_DELAY_1			(0x2D0U)
#define ODW_EOF_INTERRUPT_CLEAR_TIME_1			(0x2D4U)
#define ODW_CAM_1_STATUS				(0x2E0U)
#define ODW_CAM_1_CLEAR					(0x2E8U)
#define ODW_EOF_INTERRUPT_CLEAR_1			(0x2ECU)
#define ODW_CAM_1_UPDATE				(0x2F0U)

#define	ODW_INT						(0x140U)

/*
 * ODW Input Format
 */
#define ORDER_RGB				(0U)
#define ORDER_RBG				(1U)
#define ORDER_GRB				(2U)
#define ORDER_GBR				(3U)
#define ORDER_BRG				(4U)
#define ORDER_BGR				(5U)

#define FMT_YUV444				(0U)
#define FMT_YVU444				(1U)
#define FMT_UVY444				(2U)
#define FMT_VUY444				(3U)
#define FMT_YUV422_LSB_16BIT			(4U)
#define FMT_YVU422_LSB_16BIT			(5U)
#define FMT_UVY422_LSB_16BIT			(6U)
#define FMT_VUY422_LSB_16BIT			(7U)
#define FMT_YUV422_MSB_16BIT			(8U)
#define FMT_YVU422_MSB_16BIT			(9U)
#define FMT_UVY422_MSB_16BIT			(10U)
#define FMT_VUY422_MSB_16BIT			(11U)
#define FMT_RGB					(12U)
#define FMT_BGR					(13U)

/*
 * ODW Output Format
 */
#define DEWARP_COLOR_FMT_RGB			(0U)
#define DEWARP_COLOR_FMT_RGB_IR			(1U)
#define DEWARP_COLOR_FMT_VYUY			(3U)
#define	DEWARP_COLOR_FMT_UYVY			(4U)
#define	DEWARP_COLOR_FMT_YVYU			(5U)
#define	DEWARP_COLOR_FMT_YUYV			(6U)
#define	DEWARP_COLOR_FMT_YVU422IL		(7U)
#define	DEWARP_COLOR_FMT_YUV422IL		(8U)
#define	DEWARP_COLOR_FMT_YVU420IL_ODD		(9U)
#define	DEWARP_COLOR_FMT_YUV420IL_ODD		(10U)
#define	DEWARP_COLOR_FMT_YVU420IL_EVEN		(11U)
#define	DEWARP_COLOR_FMT_YUV420IL_EVEN		(12U)
#define DEWARP_COLOR_FMT_IR_ONLY			(13U)

/*
 * ODW_DUMMY_WRITE Register
 */
#define ODW_DUMMY_WRITE_WREADY_SHIFT			(8U)
#define ODW_DUMMY_WRITE_WSTART_SHIFT			(0U)

#define ODW_DUMMY_WRITE_WREADY_MASK			((u32)0x1U << ODW_DUMMY_WRITE_WREADY_SHIFT)
#define ODW_DUMMY_WRITE_WSTART_MASK			((u32)0x1U << ODW_DUMMY_WRITE_WSTART_SHIFT)

/*
 * ODW_AXI_CONFIG Register
 */
#define ODW_AXI_CONFIG_MAX_OS_SHIFT			(16U)
#define ODW_AXI_CONFIG_MAX_BURST_SHIFT			(0U)

#define ODW_AXI_CONFIG_MAX_OS_MASK			((u32)0x3FU << ODW_AXI_CONFIG_MAX_OS_SHIFT)
#define ODW_AXI_CONFIG_MAX_BURST_MASK			((u32)0xFFU << ODW_AXI_CONFIG_MAX_BURST_SHIFT)

/*
 * ODW_FRAME_TOGGLE_THRES Register
 */
#define ODW_FRAME_TOGGLE_THRES_MASK			((u32)0xFFFU)

/*
 * ODW_LINE_TOGGLE_THRES Register
 */
#define ODW_LINE_TOGGLE_THRES_MASK			((u32)0xFFFU)

/*
 * ODW_MAX_TOGGLE_CNT Register
 */
#define ODW_MAX_TOGGLE_CNT_MASK				((u32)0xFFU)

/*
 * ODW_INPUT_SIZE Register
 */
#define ODW_INPUT_SIZE_HEIGHT_SHIFT			(16U)
#define ODW_INPUT_SIZE_WIDTH_SHIFT			(0U)

#define ODW_INPUT_SIZE_HEIGHT_MASK			((u32)0xFFFU << ODW_INPUT_SIZE_HEIGHT_SHIFT)
#define ODW_INPUT_SIZE_WIDTH_MASK			((u32)0xFFFU << ODW_INPUT_SIZE_WIDTH_SHIFT)

/*
 * ODW_DEWARP_SIZE Register
 */
#define ODW_DEWARP_SIZE_HEIGHT_SHIFT			(16U)
#define ODW_DEWARP_SIZE_WIDTH_SHIFT			(0U)

#define ODW_DEWARP_SIZE_HEIGHT_MASK			((u32)0xFFFU << ODW_DEWARP_SIZE_HEIGHT_SHIFT)
#define ODW_DEWARP_SIZE_WIDTH_MASK			((u32)0xFFFU << ODW_DEWARP_SIZE_WIDTH_SHIFT)

/*
 * ODW_IS_FISHEYE Register
 */
#define ODW_IS_FISHEYE_MASK				((u32)0x1U)

/*
 * ODW_IMAGE_FORMAT Register
 */
#define ODW_IMAGE_FORMAT_IR_ENABLE_SHIFT		(16U)
#define ODW_IMAGE_FORMAT_OUTPUT_FORMAT_SHIFT		(8U)
#define ODW_IMAGE_FORMAT_INPUT_FORMAT_SHIFT		(0U)

#define ODW_IMAGE_FORMAT_IR_ENABLE_MASK			((u32)0x1U << ODW_IMAGE_FORMAT_IR_ENABLE_SHIFT)
#define ODW_IMAGE_FORMAT_OUTPUT_FORMAT_MASK		((u32)0xFU << ODW_IMAGE_FORMAT_OUTPUT_FORMAT_SHIFT)
#define ODW_IMAGE_FORMAT_INPUT_FORMAT_MASK		((u32)0xFU << ODW_IMAGE_FORMAT_INPUT_FORMAT_SHIFT)

/*
 * ODW_BLOCK_CONFIG Register
 */
#define ODW_BLOCK_CONFIG_BLOCK_OFFSET_Y_SHIFT		(24U)
#define ODW_BLOCK_CONFIG_BLOCK_OFFSET_X_SHIFT		(16U)
#define ODW_BLOCK_CONFIG_BLOCK_INTERVAL_Y_SHIFT		(8U)
#define ODW_BLOCK_CONFIG_BLOCK_INTERVAL_X_SHIFT		(0U)

#define ODW_BLOCK_CONFIG_BLOCK_OFFSET_Y_MASK		((u32)0x3U << ODW_BLOCK_CONFIG_BLOCK_OFFSET_Y_SHIFT)
#define ODW_BLOCK_CONFIG_BLOCK_OFFSET_X_MASK		((u32)0x3FU << ODW_BLOCK_CONFIG_BLOCK_OFFSET_X_SHIFT)
#define ODW_BLOCK_CONFIG_BLOCK_INTERVAL_Y_MASK		((u32)0x3U << ODW_BLOCK_CONFIG_BLOCK_INTERVAL_Y_SHIFT)
#define ODW_BLOCK_CONFIG_BLOCK_INTERVAL_X_MASK		((u32)0x3FU << ODW_BLOCK_CONFIG_BLOCK_INTERVAL_X_SHIFT)

/*
 * ODW_FILTER_KERNEL_0 Register
 */
#define ODW_FILTER_KERNEL_FIR_3_SHIFT			(24U)
#define ODW_FILTER_KERNEL_FIR_2_SHIFT			(16U)
#define ODW_FILTER_KERNEL_FIR_1_SHIFT			(8U)
#define ODW_FILTER_KERNEL_FIR_0_SHIFT			(0U)

#define ODW_FILTER_KERNEL_FIR_3_MASK			((u32)0xFFU << ODW_FILTER_KERNEL_FIR_3_SHIFT)
#define ODW_FILTER_KERNEL_FIR_2_MASK			((u32)0xFFU << ODW_FILTER_KERNEL_FIR_2_SHIFT)
#define ODW_FILTER_KERNEL_FIR_1_MASK			((u32)0xFFU << ODW_FILTER_KERNEL_FIR_1_SHIFT)
#define ODW_FILTER_KERNEL_FIR_0_MASK			((u32)0xFFU << ODW_FILTER_KERNEL_FIR_0_SHIFT)

/*
 * ODW_FILTER_KERNEL_1 Register
 */
#define ODW_FILTER_KERNEL_IIR_2_SHIFT			(24U)
#define ODW_FILTER_KERNEL_IIR_1_SHIFT			(16U)
#define ODW_FILTER_KERNEL_IIR_0_SHIFT			(8U)
#define ODW_FILTER_KERNEL_FIR_4_SHIFT			(0U)

#define ODW_FILTER_KERNEL_IIR_2_MASK			((u32)0xFFU << ODW_FILTER_KERNEL_IIR_2_SHIFT)
#define ODW_FILTER_KERNEL_IIR_1_MASK			((u32)0xFFU << ODW_FILTER_KERNEL_IIR_1_SHIFT)
#define ODW_FILTER_KERNEL_IIR_0_MASK			((u32)0xFFU << ODW_FILTER_KERNEL_IIR_0_SHIFT)
#define ODW_FILTER_KERNEL_FIR_4_MASK			((u32)0xFFU << ODW_FILTER_KERNEL_FIR_4_SHIFT)

/*
 * ODW_DEFAULT_COLOR_0 Register
 */
#define ODW_DEFAULT_COLOR_0_SHIFT			(0U)

#define ODW_DEFAULT_COLOR_0_MASK			((u32)0xFFFFFFFFU << ODW_DEFAULT_COLOR_0_SHIFT)

/*
 * ODW_DEFAULT_COLOR_1 Register
 */
#define ODW_DEFAULT_COLOR_1_SHIFT			(0U)

#define ODW_DEFAULT_COLOR_1_MASK			((u32)0xFFFFFFU << ODW_DEFAULT_COLOR_0_SHIFT)

/*
 * ODW_CAM_MAT Register
 */
#define ODW_CAM_MAT_MASK				((u32)0xFFFFFFU)

/*
 * ODW_DIST_COEFF Register
 */
#define ODW_DIST_COEFF_MASK				((u32)0xFFFFFFU)

/*
 * ODW_HOMOGRAPHY Register
 */
#define ODW_HOMOGRAPHY_MASK				((u32)0xFFFFFFU)

/*
 * ODW_BACKWARD_SCAN_CONFIG Register
 */
#define ODW_BACKWARD_SCAN_CONFIG_SCAN_XY_SWAP_SHIFT	(24U)
#define ODW_BACKWARD_SCAN_CONFIG_SCAN_MARGIN_Y_SHIFT	(16U)
#define ODW_BACKWARD_SCAN_CONFIG_SCAN_MARGIN_X_SHIFT	(8U)
#define ODW_BACKWARD_SCAN_CONFIG_ROUND_THRES_SHIFT	(0U)

#define ODW_BACKWARD_SCAN_CONFIG_SCAN_XY_SWAP_MASK	((u32)0x1U << ODW_BACKWARD_SCAN_CONFIG_SCAN_XY_SWAP_SHIFT)
#define ODW_BACKWARD_SCAN_CONFIG_SCAN_MARGIN_Y_MASK	((u32)0x3U << ODW_BACKWARD_SCAN_CONFIG_SCAN_MARGIN_Y_SHIFT)
#define ODW_BACKWARD_SCAN_CONFIG_SCAN_MARGIN_X_MASK	((u32)0x3U << ODW_BACKWARD_SCAN_CONFIG_SCAN_MARGIN_X_SHIFT)
#define ODW_BACKWARD_SCAN_CONFIG_ROUND_THRES_MASK	((u32)0xFU << ODW_BACKWARD_SCAN_CONFIG_ROUND_THRES_SHIFT)

/*
 * ODW_POSITION_FIFO_TIMEOUT_THRES Register
 */
#define ODW_POSITION_FIFO_TIMEOUT_THRES_MASK		((u32)0xFFFFU)

/*
 * ODW_OUTPUT_FIFO_TIMEOUT_THRES Register
 */
#define ODW_OUTPUT_FIFO_TIMEOUT_THRES_MASK		((u32)0xFFFFU)

/*
 * ODW_OUTPUT_BUFFER_WAIT_TIME Register
 */
#define ODW_OUTPUT_BUFFER_WAIT_TIME_MASK		((u32)0xFFFFU)

/*
 * ODW_BUFFER_CAPACITY Register
 */
#define ODW_BUFFER_CAPACITY_MASK			((u32)0xFU)

/*
 * ODW_EOF_INTERRUPT_DELAY Register
 */
#define ODW_EOF_INTERRUPT_DELAY_MASK			((u32)0xFFFFU)

/*
 * ODW_EOF_INTERRUPT_CLEAR_TIME Register
 */
#define ODW_EOF_INTERRUPT_CLEAR_TIME_MASK		((u32)0xFFFFU)

/*
 * ODW_CAM_STATUS Register
 */
#define ODW_CAM_STATUS_FRAME_ACTIVE_SHIFT		(24U)
#define ODW_CAM_STATUS_OUTPUT_TIMEOUT_SHIFT		(16U)
#define ODW_CAM_STATUS_POSITION_TIMEOUT_SHIFT		(8U)
#define ODW_CAM_STATUS_FRAME_TIMEOUT_SHIFT		(7U)
#define ODW_CAM_STATUS_TOGGLE_SHIFT			(6U)
#define ODW_CAM_STATUS_FRAME_SLOW_SHIFT			(5U)
#define ODW_CAM_STATUS_FRAME_FAST_SHIFT			(4U)
#define ODW_CAM_STATUS_LINE_OVERFLOW_SHIFT		(3U)
#define ODW_CAM_STATUS_LINE_INSUFFICIENT_SHIFT		(2U)
#define ODW_CAM_STATUS_PIXEL_OVERFLOW_SHIFT		(1U)
#define ODW_CAM_STATUS_PIXEL_INSUFFICIENT_SHIFT		(0U)

#define ODW_CAM_STATUS_FRAME_ACTIVE_MASK		\
		(((u32)0x1U) << ODW_CAM_STATUS_FRAME_ACTIVE_SHIFT)
#define ODW_CAM_STATUS_OUTPUT_TIMEOUT_MASK		\
		(((u32)0x1U) << ODW_CAM_STATUS_OUTPUT_TIMEOUT_SHIFT)
#define ODW_CAM_STATUS_POSITION_TIMEOUT_MASK		\
		(((u32)0x1U) << ODW_CAM_STATUS_POSITION_TIMEOUT_SHIFT)
#define ODW_CAM_STATUS_FRAME_TIMEOUT_MASK		\
		(((u32)0x1U) << ODW_CAM_STATUS_FRAME_TIMEOUT_SHIFT)
#define ODW_CAM_STATUS_TOGGLE_MASK			\
		(((u32)0x1U) << ODW_CAM_STATUS_TOGGLE_SHIFT)
#define ODW_CAM_STATUS_FRAME_SLOW_MASK			\
		(((u32)0x1U) << ODW_CAM_STATUS_FRAME_SLOW_SHIFT)
#define ODW_CAM_STATUS_FRAME_FAST_MASK			\
		(((u32)0x1U) << ODW_CAM_STATUS_FRAME_FAST_SHIFT)
#define ODW_CAM_STATUS_LINE_OVERFLOW_MASK		\
		(((u32)0x1U) << ODW_CAM_STATUS_LINE_OVERFLOW_SHIFT)
#define ODW_CAM_STATUS_LINE_INSUFFICIENT_MASK		\
		(((u32)0x1U) << ODW_CAM_STATUS_LINE_INSUFFICIENT_SHIFT)
#define ODW_CAM_STATUS_PIXEL_OVERFLOW_MASK		\
		(((u32)0x1U) << ODW_CAM_STATUS_PIXEL_OVERFLOW_SHIFT)
#define ODW_CAM_STATUS_PIXEL_INSUFFICIENT_MASK		\
		(((u32)0x1U) << ODW_CAM_STATUS_PIXEL_INSUFFICIENT_SHIFT)

#define ODW_CAM_STATUS_ALL_MASK				\
		(ODW_CAM_STATUS_FRAME_ACTIVE_MASK | \
		ODW_CAM_STATUS_OUTPUT_TIMEOUT_MASK | \
		ODW_CAM_STATUS_POSITION_TIMEOUT_MASK | \
		ODW_CAM_STATUS_FRAME_TIMEOUT_MASK | \
		ODW_CAM_STATUS_TOGGLE_MASK | \
		ODW_CAM_STATUS_FRAME_SLOW_MASK | \
		ODW_CAM_STATUS_FRAME_FAST_MASK | \
		ODW_CAM_STATUS_LINE_OVERFLOW_MASK | \
		ODW_CAM_STATUS_LINE_INSUFFICIENT_MASK | \
		ODW_CAM_STATUS_PIXEL_OVERFLOW_MASK | \
		ODW_CAM_STATUS_PIXEL_INSUFFICIENT_MASK)

/*
 * ODW_ENABLE Register
 */
#define ODW_ENABLE_DOWNSAMPLING_ENABLE_SHIFT		(28U)
#define ODW_ENABLE_FILTER_ENABLE_SHIFT			(24U)
#define ODW_ENABLE_VERTICAL_FLIP_SHIFT			(20U)
#define ODW_ENABLE_HORIZONTAL_FLIP_SHIFT		(16U)
#define ODW_ENABLE_CHROMA_INTERPOLATION_SHIFT		(12U)
#define ODW_ENABLE_DEWARP_ENABLE_SHIFT			(8U)
#define ODW_ENABLE_BYPASS_ENABLE_SHIFT			(4U)
#define ODW_ENABLE_STREAM_ENABLE_SHIFT			(0U)

#define ODW_ENABLE_DOWNSAMPLING_ENABLE_MASK		((u32)0x1U << ODW_ENABLE_DOWNSAMPLING_ENABLE_SHIFT)
#define ODW_ENABLE_FILTER_ENABLE_MASK			((u32)0x1U << ODW_ENABLE_FILTER_ENABLE_SHIFT)
#define ODW_ENABLE_VERTICAL_FLIP_MASK			((u32)0x1U << ODW_ENABLE_VERTICAL_FLIP_SHIFT)
#define ODW_ENABLE_HORIZONTAL_FLIP_MASK			((u32)0x1U << ODW_ENABLE_HORIZONTAL_FLIP_SHIFT)
#define ODW_ENABLE_CHROMA_INTERPOLATION_MASK		((u32)0x1U << ODW_ENABLE_CHROMA_INTERPOLATION_SHIFT)
#define ODW_ENABLE_DEWARP_ENABLE_MASK			((u32)0x1U << ODW_ENABLE_DEWARP_ENABLE_SHIFT)
#define ODW_ENABLE_BYPASS_ENABLE_MASK			((u32)0x1U << ODW_ENABLE_BYPASS_ENABLE_SHIFT)
#define ODW_ENABLE_STREAM_ENABLE_MASK			((u32)0x1U << ODW_ENABLE_STREAM_ENABLE_SHIFT)

#define ODW_INT_0_SHIFT			(8U)
#define ODW_INT_1_SHIFT			(9U)

#define ODW_INT_0_MASK			((u32)0x1U << ODW_INT_0_SHIFT)
#define ODW_INT_1_MASK			((u32)0x1U << ODW_INT_1_SHIFT)

struct dewarp_odw_input_params {
	u32			width;
	u32			height;
	u32			ir_enable;
	u32			format;
	u32			is_fisheye;
	u32			is_dewarp;
	u32			is_bypass;
};

struct dewarp_odw_output_params {
	u32			width;
	u32			height;
	u32			format;
	u32			address[3];
	u32			strides[3];
};

struct dewarp_odw_params {
	u32			id;
	u32			stream_enable;

	struct dewarp_odw_input_params	in;
	struct dewarp_odw_output_params	out;

	u32			max_ros;
	u32			max_wos;
	u32			max_rburst;
	u32			max_wburst;
	u32			frame_time_cycle;
	u32			frame_timeout_thres;
	u32			frame_fast_thres;
	u32			frame_slow_thres;
	u32			frame_toggle_thres;
	u32			line_toggle_thres;
	u32			max_toggle_thres;
	u32			offset_x;
	u32			offset_y;
	u32			interval_x;
	u32			interval_y;
	u32			firs[5];
	u32			iirs[3];
	u32			colors[4];
	u32			cam_mat[5];
	u32			dist_coeff_fwds[4];
	u32			dist_coeff_bwds[4];
	u32			homography_fwd[6];
	u32			homography_bwd[6];
	u32			scan_xy_swap;
	u32			scan_margin_x;
	u32			scan_margin_y;
	u32			round_thres;
	u32			position_fifo_timeout_cnt;
	u32			dewarp_fifo_timeout_cnt;
	u32			dewarp_buffer_wait_cnt;
	u32			buffer_capacity;
	u32			eof_interrupt_delay;
	u32			eof_interrupt_clear_time;
};

extern u32 dewarp_odw_get_ip_version(u32 id);
extern u32 dewarp_odw_get_dummy_data_status(u32 id);
extern void dewarp_odw_set_dummy_data(u32 id, u32 start);
extern void dewarp_odw_set_input_size(u32 id, u32 width, u32 height);
extern void dewarp_odw_set_output_size(u32 id, u32 width, u32 height);
extern void dewarp_odw_set_input_format(u32 id, u32 format, u32 ir_enable);
extern void dewarp_odw_set_input_fisheye(u32 id, u32 is_fisheye);
extern void dewarp_odw_set_output_format(u32 id, u32 format);
extern void dewarp_odw_set_output_address(u32 id, u32 addresses[], u32 ir_enable);
extern void dewarp_odw_set_output_stride(u32 id, u32 strides[]);
extern void dewarp_odw_set_block_config(u32 id, u32 offset_x, u32 offset_y,
					u32 interval_x, u32 interval_y);
extern void dewarp_odw_set_filter(u32 id, u32 firs[], u32 iirs[]);
extern void dewarp_odw_set_background_color(u32 id, u32 color);
extern void dewarp_odw_set_cam_matrix(u32 id, u32 cam_mat[]);
extern void dewarp_odw_set_forward_dist_coeff(u32 id, u32 dist_coeff[]);
extern void dewarp_odw_set_backward_dist_coeff(u32 id, u32 dist_coeff[]);
extern void dewarp_odw_set_forward_homography(u32 id, u32 homography[]);
extern void dewarp_odw_set_backward_homography(u32 id, u32 homography[]);
extern void dewarp_odw_set_axi_config(u32 id, u32 max_ros, u32 max_wos,
				      u32 max_rbust, u32 max_wburst);
extern void dewarp_odw_set_frame_time_cycle(u32 id, u32 frame_time_cycle);
extern void dewarp_odw_set_frame_timeout_threshold(u32 id, u32 frame_timeout_thres);
extern void dewarp_odw_set_frame_fast_threshold(u32 id, u32 frame_fast_thres);
extern void dewarp_odw_set_frame_slow_threshold(u32 id, u32 frame_slow_thres);
extern void dewarp_odw_set_frame_toggle_threshold(u32 id, u32 frame_toggle_thres);
extern void dewarp_odw_set_line_toggle_threshold(u32 id, u32 line_timeout_thres);
extern void dewarp_odw_set_max_toggle_threshold(u32 id, u32 max_toggle_thres);
extern void dewarp_odw_set_backward_scan_config(u32 id, u32 scan_xy_swap,
						u32 scan_margin_x, u32 scan_margin_y,
						u32 round_thres);
extern void dewarp_odw_set_position_fifo_timeout_cnt(u32 id, u32 position_fifo_timeout_cnt);
extern void dewarp_odw_set_dewarp_fifo_timeout_cnt(u32 id, u32 dewarp_fifo_timeout_cnt);
extern void dewarp_odw_set_dewarp_buffer_wait_cnt(u32 id, u32 dewarp_buffer_wait_cnt);
extern void dewarp_odw_set_buffer_capacity(u32 id, u32 buffer_capacity);
extern void dewarp_odw_set_eof_interrupt_delay(u32 id, u32 eof_interrupt_delay);
extern void dewarp_odw_set_eof_interrupt_clear_time(u32 id, u32 eof_interrupt_clear_time);
extern u32 dewarp_odw_get_processing_status(u32 id);
extern void dewarp_odw_clear_processing_status(u32 id, u32 mask);
extern void dewarp_odw_clear_interrupt(u32 id);
extern void dewarp_odw_update(u32 id);
extern u32 dewarp_odw_enable(u32 id, u32 dewarp_enable,
				      u32 stream_enable, u32 bypass);
extern u32 dewarp_odw_disable(u32 id);
extern s32 dewarp_set_ireq_mask(const struct device *pdev, u32 id, u32 set);
extern void dewarp_get_status(u32 *status);
#endif /*DEWARP_ODW_H*/
