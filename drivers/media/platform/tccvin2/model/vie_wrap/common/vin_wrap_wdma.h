/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VIN_WRAP_WDMA_H
#define	VIN_WRAP_WDMA_H

/* RGB Swap */
#define VIN_WRAP_SWAP_RGB			(0U)
#define VIN_WRAP_SWAP_RBG			(1U)
#define VIN_WRAP_SWAP_GRB			(2U)
#define VIN_WRAP_SWAP_GBR			(3U)
#define VIN_WRAP_SWAP_BRG			(4U)
#define VIN_WRAP_SWAP_BGR			(5U)

/*
 *  WDMA YUV-to-RGB Converter Mode Register
 *
 *  0 - The Range for RGB is 16 ~ 235,"Studio Color". Normally SDTV
 *  1 - The Range for RGB is  0 ~ 255,"Conputer System Color". Normally SDTV
 *  2 - The Range for RGB is 16 ~ 235,"Studio Color". Normally HDTV
 *  3 - The Range for RGB is  0 ~ 255,"Conputer System Color". Normally HDTV
 *  4 - The Range for RGB is 16 ~ 235,"Studio Color". Normally UHDTV
 *  5 - The Range for RGB is  0 ~ 255,"Conputer System Color". Normally UHDTV
 */
#define R2YMD_SDTV_LR	0
#define R2YMD_SDTV_FR	1
#define R2YMD_HDTV_LR	2
#define R2YMD_HDTV_FR	3
#define R2YMD_UHDTV_LR	4
#define R2YMD_UHDTV_FR	5

#define VIE_WDMACTRL_FMT_1BPP		(0U) /* 1bpp indexed color */
#define VIE_WDMACTRL_FMT_2BPP		(1U) /* 2bpp indexed color */
#define VIE_WDMACTRL_FMT_4BPP		(2U) /* 4bpp indexed color */
#define VIE_WDMACTRL_FMT_8BPP		(3U) /* 8bpp indexed color */
#define VIE_WDMACTRL_FMT_RGB332		(8U) /* RGB332 - 1bytes aligned
					      * R[7:5],G[4:2],B[1:0] */
#define VIE_WDMACTRL_FMT_ARGB444	(9U) /* RGB444 - 2bytes aligned
					      * A[15:12],R[11:8],G[7:3],B[3:0] */
#define VIE_WDMACTRL_FMT_RGB565		(10U) /* RGB565 - 2bytes aligned
					       * R[15:11],G[10:5],B[4:0] */
#define VIE_WDMACTRL_FMT_ARGB1555	(11U) /* ARGB1555 - 2bytes aligned
					       * A[15],R[14:10],G[9:5],B[4:0] */
#define VIE_WDMACTRL_FMT_ARGB8888	(12U) /* ARGB888 - 4bytes aligned
					       * A[31:24],R[23:16],G[15:8],B[7:0] */
#define VIE_WDMACTRL_FMT_ARGB6666_4	(13U) /* ARGB666 - 4bytes aligned
					       * A[23:18],R[17:12],G[11:6],B[5:0] */
#define VIE_WDMACTRL_FMT_RGB888		(14U) /* RGB888 - 3 bytes aligned
					       * B1[31:24],R0[23:16],G0[15:8],B0[7:0] */
#define VIE_WDMACTRL_FMT_ARGB6666_3	(15U) /* ARGB6666 - 3 bytes aligned
					       * G1[1:0], B1[29:24], A0[23:18],
					       * R0[17:12], G0[11:6], B0[5:0]*/
#define VIN_WRAP_IMG_FMT_IR8		(20U)
#define VIE_WDMACTRL_FMT_444SEP		(21U) /* 4:4:4 separate format
					       * If Y2R is enabled,
					       * this can be used for YUV format,
					       * otherwise for RGB separated format.
					       */
#define VIE_WDMACTRL_FMT_UYVY		(22U) /* YCbCr 4:2:2 sequential format
					       * LSB [U/Y/V/Y] MSB */
#define VIE_WDMACTRL_FMT_VYUY		(23U) /* YCbCr 4:2:2 sequential format
                                               * LSB [V/Y/U/Y] MSB */
#define VIE_WDMACTRL_FMT_YUV420SP	(24U) /* YCbCr 4:2:0 separated format */
#define VIE_WDMACTRL_FMT_YUV422SP	(25U) /* YCbCr 4:2:2 separated format */
#define VIE_WDMACTRL_FMT_YUYV		(26U) /* YCbCr 4:2:2 sequential format
					       * LSB [Y/U/Y/V] MSB */
#define VIE_WDMACTRL_FMT_YVYU		(27U) /* YCbCr 4:2:2 sequential format
					       * LSB [Y/V/Y/U] MSB */
#define VIE_WDMACTRL_FMT_YUV420ITL0	(28U) /* YCbCr 4:2:0 interleaved type 0 format */
#define VIE_WDMACTRL_FMT_YUV420ITL1	(29U) /* YCbCr 4:2:0 interleaved type 1 format */
#define VIE_WDMACTRL_FMT_YUV422ITL0	(30U) /* YCbCr 4:2:2 interleaved type 0 format */
#define VIE_WDMACTRL_FMT_YUV422ITL1	(31U) /* YCbCr 4:2:2 interleaved type 1 format */

struct CSS_WDMA_IMAGE_INFO_Type {
	unsigned int ImgSizeWidth;
	unsigned int ImgSizeHeight;
	unsigned int TargetWidth;
	unsigned int TargetHeight;
	unsigned int ImgFormat;
	unsigned int BaseAddress;
	unsigned int BaseAddress1;
	unsigned int BaseAddress2;
	unsigned int Interlaced;
	unsigned int ContinuousMode;
	unsigned int SyncMode;
	unsigned int AlphaValue;
	unsigned int Hue;
	unsigned int Bright;
	unsigned int Contrast;
};

/* VIN WrapperWDMA : 0x20XX */
#define VIN_WRAP_WDMA				(0x2000U)
#define VIN_WRAP_WDMA0				(0x2000U)
#define VIN_WRAP_WDMA1				(0x2001U)
#define VIN_WRAP_WDMA2				(0x2002U)
#define VIN_WRAP_WDMA3				(0x2003U)
#define VIN_WRAP_WDMA_MAX			(0x0004U)

/*
 * register offset
 */
#define WDMACTRL_OFFSET				(0x00U)
#define WDMARATE_OFFSET				(0x04U)
#define WDMASIZE_OFFSET				(0x08U)
#define WDMABASE0_OFFSET			(0x0CU)
#define WDMACADDR_OFFSET			(0x10U)
#define WDMABASE1_OFFSET			(0x14U)
#define WDMABASE2_OFFSET			(0x18U)
#define WDMAOFFS_OFFSET				(0x1CU)
#define WDMABG_OFFSET				(0x24U)
#define WDMAPTS_OFFSET				(0x28U)
#define WDMADMAT0_OFFSET			(0x2CU)
#define WDMADMAT1_OFFSET			(0x30U)
#define WDMAROLL_OFFSET				(0x38U)
#define WDMASBASE_OFFSET			(0x3CU)
#define WDMAIRQSTS_OFFSET			(0x40U)
#define WDMAIRQMSK_OFFSET			(0x44U)

/*
 * WDMA Control Registers
 */
#define WDMACTRL_INTL_SHIFT			(31U) // Interlaced Image Indication Register
#define WDMACTRL_FU_SHIFT			(29U) // Field Update Enable
#define WDMACTRL_IEN_SHIFT			(28U) // Image Enable Register
#define WDMACTRL_DITHS_SHIFT			(27U) // Dither Select Register
#define WDMACTRL_DITHE_SHIFT			(24U) // Dither Enable Register
#define WDMACTRL_CONT_SHIFT			(23U) // Continuous Mode Enable Register
#define WDMACTRL_SREQ_SHIFT			(22U) // Stop Request Enable Register
#define WDMACTRL_Y2RMD_SHIFT			(18U) // YUV-to-RGB Converter Mode Register
#define WDMACTRL_Y2R_SHIFT			(17U) // YUV-to-RGB Converter Enable Register
#define WDMACTRL_UPD_SHIFT			(16U) // Information Update Register
#define WDMACTRL_SWAP_SHIFT			(12U) // RGB Swap Mode
#define WDMACTRL_R2YMD_SHIFT			(9U)  // RGB-toYUV Converter Mode Register
#define WDMACTRL_R2Y_SHIFT			(8U)  // RGB-toYUV Converter Enable Register
#define WDMACTRL_BR_SHIFT			(7U)  // Bit-Reverse in Byte
#define WDMACTRL_FMT_SHIFT			(0U)  // Image Format Register

#define WDMACTRL_INTL_MASK			((u32)0x1U << WDMACTRL_INTL_SHIFT)
#define WDMACTRL_FU_MASK			((u32)0x1U << WDMACTRL_FU_SHIFT)
#define WDMACTRL_IEN_MASK			((u32)0x1U << WDMACTRL_IEN_SHIFT)
#define WDMACTRL_DITHS_MASK			((u32)0x1U << WDMACTRL_DITHS_SHIFT)
#define WDMACTRL_DITHE_MASK			((u32)0x1U << WDMACTRL_DITHE_SHIFT)
#define WDMACTRL_CONT_MASK			((u32)0x1U << WDMACTRL_CONT_SHIFT)
#define WDMACTRL_SREQ_MASK			((u32)0x1U << WDMACTRL_SREQ_SHIFT)
#define WDMACTRL_Y2RMD_MASK			((u32)0x3U << WDMACTRL_Y2RMD_SHIFT)
#define WDMACTRL_Y2R_MASK			((u32)0x1U << WDMACTRL_Y2R_SHIFT)
#define WDMACTRL_UPD_MASK			((u32)0x1U << WDMACTRL_UPD_SHIFT)
#define WDMACTRL_SWAP_MASK			((u32)0x7U << WDMACTRL_SWAP_SHIFT)
#define WDMACTRL_R2YMD_MASK			((u32)0x3U << WDMACTRL_R2YMD_SHIFT)
#define WDMACTRL_R2Y_MASK			((u32)0x1U << WDMACTRL_R2Y_SHIFT)
#define WDMACTRL_BR_MASK			((u32)0x1U << WDMACTRL_BR_SHIFT)
#define WDMACTRL_FMT_MASK			((u32)0x1FU << WDMACTRL_FMT_SHIFT)

/*
 * WDMA Rate Control Registers
 */
#define WDMARATE_REN_SHIFT			(31U) // Rate Control Enable
#define WDMARATE_MAXRATE_SHIFT			(16U) // Maximum Pixel Rate (per micro second)
#define WDMARATE_SYNCMD_SHIFT			(9U)  // WDMA Sync Mode
#define WDMARATE_SEN_SHIFT			(8U)  // RDMA Sync Enable
#define WDMARATE_SYNCSEL_SHIFT			(0U)  // RDMA Select for Sync

#define WDMARATE_REN_MASK			((u32)0x1U << WDMARATE_REN_SHIFT)
#define WDMARATE_MAXRATE_MASK			((u32)0xFFU << WDMARATE_MAXRATE_SHIFT)
#define WDMARATE_SYNCMD_MASK			((u32)0x7U << WDMARATE_SYNCMD_SHIFT)
#define WDMARATE_SEN_MASK			((u32)0x1U << WDMARATE_SEN_SHIFT)
#define WDMARATE_SYNCSEL_MASK			((u32)0xFFU << WDMARATE_SYNCSEL_SHIFT)

/*
 * WDMA Size Registers
 */
#define WDMASIZE_HEIGHT_SHIFT			(16U) // Height Register
#define WDMASIZE_WIDTH_SHIFT			(0U)  // Width Register

#define WDMASIZE_HEIGHT_MASK			((u32)0x1FFFU << WDMASIZE_HEIGHT_SHIFT)
#define WDMASIZE_WIDTH_MASK			((u32)0x1FFFU << WDMASIZE_WIDTH_SHIFT)

/*
 * WDMA Base Address 0 Registers
 */
#define WDMABASE0_BASE0_SHIFT			(0U) // 1st Base Address for each image

#define WDMABASE0_BASE0_MASK			((u32)0xFFFFFFFFU << WDMABASE0_BASE0_SHIFT)

/*
 * WDMA Current Address 0 Registers
 */
#define WDMACADDR_CADDR_SHIFT			(0U) // The working address for base address

#define WDMACADDR_CADDR_MASK			((u32)0xFFFFFFFFU << WDMACADDR_CADDR_SHIFT)

/*
 * WDMA Base Address 1 Registers
 */
#define WDMABASE1_BASE1_SHIFT			(0U) // The 2nd base address for each image

#define WDMABASE1_BASE1_MASK			((u32)0xFFFFFFFFU << WDMABASE1_BASE1_SHIFT)

/*
 * WDMA Base Address 1 Registers
 */
#define WDMABASE2_BASE2_SHIFT			(0U) // The 3rd base address for each image

#define WDMABASE2_BASE2_MASK			((u32)0xFFFFFFFFU << WDMABASE2_BASE2_SHIFT)

/*
 * WDMA Offset Registers
 */
#define WDMAOFFS_OFFSET1_SHIFT			(16U) // The 2nd offset for each image
#define WDMAOFFS_OFFSET0_SHIFT			(0U)  // The 1st offset for each image

#define WDMAOFFS_OFFSET1_MASK			((u32)0xFFFFU << WDMAOFFS_OFFSET1_SHIFT)
#define WDMAOFFS_OFFSET0_MASK			((u32)0xFFFFU << WDMAOFFS_OFFSET0_SHIFT)

/*
 * WDMA BackGround Color Registers
 */
#define WDMABG_BG3_SHIFT			(24U) // Background Color 3 (Alpha)
#define WDMABG_BG2_SHIFT			(16U) // Background Color 2 (Y/B)
#define WDMABG_BG1_SHIFT			(8U)  // Background Color 1 (Cb/G)
#define WDMABG_BG0_SHIFT			(0U)  // Background Color 0 (Cr/R)

#define WDMABG_BG3_MASK				((u32)0xFFU << WDMABG_BG3_SHIFT)
#define WDMABG_BG2_MASK				((u32)0xFFU << WDMABG_BG2_SHIFT)
#define WDMABG_BG1_MASK				((u32)0xFFU << WDMABG_BG1_SHIFT)
#define WDMABG_BG0_MASK				((u32)0xFFU << WDMABG_BG0_SHIFT)

/*
 * WDMA PTS Registers
 */
#define WDMAPTS_PTS_SHIFT			(0U) // Presentation Time Stamp Register

#define WDMAPTS_PTS_MASK			((u32)0xFFFFU << WDMAPTS_PTS_SHIFT)

/*
 * Dither Matrix 0
 */
#define WDMADMAT0_DITH13_SHIFT			(28U) // Dithering Pattern Matrix (1,3)
#define WDMADMAT0_DITH12_SHIFT			(24U) // Dithering Pattern Matrix (1,2)
#define WDMADMAT0_DITH11_SHIFT			(20U) // Dithering Pattern Matrix (1,1)
#define WDMADMAT0_DITH10_SHIFT			(16U) // Dithering Pattern Matrix (1,0)
#define WDMADMAT0_DITH03_SHIFT			(12U) // Dithering Pattern Matrix (0,3)
#define WDMADMAT0_DITH02_SHIFT			(8U)  // Dithering Pattern Matrix (0,2)
#define WDMADMAT0_DITH01_SHIFT			(4U)  // Dithering Pattern Matrix (0,1)
#define WDMADMAT0_DITH00_SHIFT			(0U)  // Dithering Pattern Matrix (0,0)

#define WDMADMAT0_DITH13_MASK			((u32)0x7U << WDMADMAT0_DITH13_SHIFT)
#define WDMADMAT0_DITH12_MASK			((u32)0x7U << WDMADMAT0_DITH12_SHIFT)
#define WDMADMAT0_DITH11_MASK			((u32)0x7U << WDMADMAT0_DITH11_SHIFT)
#define WDMADMAT0_DITH10_MASK			((u32)0x7U << WDMADMAT0_DITH10_SHIFT)
#define WDMADMAT0_DITH03_MASK			((u32)0x7U << WDMADMAT0_DITH03_SHIFT)
#define WDMADMAT0_DITH02_MASK			((u32)0x7U << WDMADMAT0_DITH02_SHIFT)
#define WDMADMAT0_DITH01_MASK			((u32)0x7U << WDMADMAT0_DITH01_SHIFT)
#define WDMADMAT0_DITH00_MASK			((u32)0x7U << WDMADMAT0_DITH00_SHIFT)

/*
 * Dither Matrix 1
 */
#define WDMADMAT1_DITH33_SHIFT			(28U) // Dithering Pattern Matrix (3,3)
#define WDMADMAT1_DITH32_SHIFT			(24U) // Dithering Pattern Matrix (3,2)
#define WDMADMAT1_DITH31_SHIFT			(20U) // Dithering Pattern Matrix (3,1)
#define WDMADMAT1_DITH30_SHIFT			(16U) // Dithering Pattern Matrix (3,0)
#define WDMADMAT1_DITH23_SHIFT			(12U) // Dithering Pattern Matrix (2,3)
#define WDMADMAT1_DITH22_SHIFT			(8U)  // Dithering Pattern Matrix (2,2)
#define WDMADMAT1_DITH21_SHIFT			(4U)  // Dithering Pattern Matrix (2,1)
#define WDMADMAT1_DITH20_SHIFT			(0U)  // Dithering Pattern Matrix (2,0)

#define WDMADMAT1_DITH33_MASK			((u32)0x7U << WDMADMAT1_DITH33_SHIFT)
#define WDMADMAT1_DITH32_MASK			((u32)0x7U << WDMADMAT1_DITH32_SHIFT)
#define WDMADMAT1_DITH31_MASK			((u32)0x7U << WDMADMAT1_DITH31_SHIFT)
#define WDMADMAT1_DITH30_MASK			((u32)0x7U << WDMADMAT1_DITH30_SHIFT)
#define WDMADMAT1_DITH23_MASK			((u32)0x7U << WDMADMAT1_DITH23_SHIFT)
#define WDMADMAT1_DITH22_MASK			((u32)0x7U << WDMADMAT1_DITH22_SHIFT)
#define WDMADMAT1_DITH21_MASK			((u32)0x7U << WDMADMAT1_DITH21_SHIFT)
#define WDMADMAT1_DITH20_MASK			((u32)0x7U << WDMADMAT1_DITH20_SHIFT)

/*
 * WDMA Rolling Control Register
 */
#define WDMAROLL_ROL_SHIFT			(31U) // Rolling Enable Register
#define WDMAROLL_ROLLCNT_SHIFT			(0U)  // Rolling Count Register

#define WDMAROLL_ROL_MASK			((u32)0x1U << WDMAROLL_ROL_SHIFT)
#define WDMAROLL_ROLLCNT_MASK			((u32)0xFFFFU << WDMAROLL_ROLLCNT_SHIFT)

/*
 * WDMA Synchronized Base Address
 */
#define WDMASBASE_SBASE0_SHIFT			(0U) // Synchronized Base Address

#define WDMABASE_SBASE0_MASK			((u32)0xFFFFFFFFU << WDMABASE_SBASE0_MASK)

/*
 * WDMA Interrupt Status Register
 */
#define WDMAIRQSTS_ST_EOF_SHIFT			(31U) // Status of EOF
#define WDMAIRQSTS_ST_BF_SHIFT			(30U) // Status of Bottom Field
#define WDMAIRQSTS_ST_SEN_SHIFT			(29U) // Status of Synchronized Enabled
#define WDMAIRQSTS_SEOFF_SHIFT			(8U)  // Falling the Sync EOF
#define WDMAIRQSTS_SEOFR_SHIFT			(7U)  // Rising the Sync EOF
#define WDMAIRQSTS_EOFF_SHIFT			(6U)  // Falling the EOF
#define WDMAIRQSTS_EOFR_SHIFT			(5U)  // Rising the EOF
#define WDMAIRQSTS_ENF_SHIFT			(4U)  // Falling the Frame Synchronized Enable
#define WDMAIRQSTS_ENR_SHIFT			(3U)  // Rising the Frame Synchronized Enable
#define WDMAIRQSTS_ROLL_SHIFT			(2U)  // Roll Interrupt
#define WDMAIRQSTS_SREQ_SHIFT			(1U)  // STOP Request
#define WDMAIRQSTS_UPD_SHIFT			(0U)  // Register Update Done

#define WDMAIRQSTS_ST_EOF_MASK			((u32)0x1U << WDMAIRQSTS_ST_EOF_SHIFT)
#define WDMAIRQSTS_ST_BF_MASK			((u32)0x1U << WDMAIRQSTS_ST_BF_SHIFT)
#define WDMAIRQSTS_ST_SEN_MASK			((u32)0x1U << WDMAIRQSTS_ST_SEN_SHIFT)
#define WDMAIRQSTS_SEOFF_MASK			((u32)0x1U << WDMAIRQSTS_SEOFF_SHIFT)
#define WDMAIRQSTS_SEOFR_MASK			((u32)0x1U << WDMAIRQSTS_SEOFR_SHIFT)
#define WDMAIRQSTS_EOFF_MASK			((u32)0x1U << WDMAIRQSTS_EOFF_SHIFT)
#define WDMAIRQSTS_EOFR_MASK			((u32)0x1U << WDMAIRQSTS_EOFR_SHIFT)
#define WDMAIRQSTS_ENF_MASK			((u32)0x1U << WDMAIRQSTS_ENF_SHIFT)
#define WDMAIRQSTS_ENR_MASK			((u32)0x1U << WDMAIRQSTS_ENR_SHIFT)
#define WDMAIRQSTS_ROLL_MASK			((u32)0x1U << WDMAIRQSTS_ROLL_SHIFT)
#define WDMAIRQSTS_SREQ_MASK			((u32)0x1U << WDMAIRQSTS_SREQ_SHIFT)
#define WDMAIRQSTS_UPD_MASK			((u32)0x1U << WDMAIRQSTS_UPD_SHIFT)

/*
 * WDMA Interrupt Mask Register
 */
#define WDMAIRQMSK_SEOFF_SHIFT			(8U) // Sync EOF Falling Intr. Masked
#define WDMAIRQMSK_SEOFR_SHIFT			(7U) // Sync EOF Rising Intr. Masked
#define WDMAIRQMSK_EOFF_SHIFT			(6U) // EOF Falling Intr. Masked
#define WDMAIRQMSK_EOFR_SHIFT			(5U) // EOF Rising Intr. Masked
#define WDMAIRQMSK_ENF_SHIFT			(4U) // Synchronized Enable Falling Intr. Masked
#define WDMAIRQMSK_ENR_SHIFT			(3U) // Synchronized Enable Rising Intr. Masked
#define WDMAIRQMSK_ROL_SHIFT			(2U) // Rolling Intr. Masked
#define WDMAIRQMSK_SREQ_SHIFT			(1U) // Stop Request Intr. Masked
#define WDMAIRQMSK_UPD_SHIFT			(0U) // Register Update Intr. Masked

#define WDMAIRQMSK_SEOFF_MASK			((u32)0x1U << WDMAIRQMSK_SEOFF_SHIFT)
#define WDMAIRQMSK_SEOFR_MASK			((u32)0x1U << WDMAIRQMSK_SEOFR_SHIFT)
#define WDMAIRQMSK_EOFF_MASK			((u32)0x1U << WDMAIRQMSK_EOFF_SHIFT)
#define WDMAIRQMSK_EOFR_MASK			((u32)0x1U << WDMAIRQMSK_EOFR_SHIFT)
#define WDMAIRQMSK_ENF_MASK			((u32)0x1U << WDMAIRQMSK_ENF_SHIFT)
#define WDMAIRQMSK_ENR_MASK			((u32)0x1U << WDMAIRQMSK_ENR_SHIFT)
#define WDMAIRQMSK_ROL_MASK			((u32)0x1U << WDMAIRQMSK_ROL_SHIFT)
#define WDMAIRQMSK_SREQ_MASK			((u32)0x1U << WDMAIRQMSK_SREQ_SHIFT)
#define WDMAIRQMSK_UPD_MASK			((u32)0x1U << WDMAIRQMSK_UPD_SHIFT)


#define VIN_WRAP_WDMA_IREQ_UPD_MASK		(WDMAIRQSTS_UPD_MASK)
#define VIN_WRAP_WDMA_IREQ_SREQ_MASK		(WDMAIRQSTS_SREQ_MASK)
#define VIN_WRAP_WDMA_IREQ_ROLL_MASK		(WDMAIRQSTS_ROLL_MASK)
#define VIN_WRAP_WDMA_IREQ_ENR_MASK		(WDMAIRQSTS_ENR_MASK)
#define VIN_WRAP_WDMA_IREQ_ENF_MASK		(WDMAIRQSTS_ENF_MASK)
#define VIN_WRAP_WDMA_IREQ_EOFR_MASK		(WDMAIRQSTS_EOFR_MASK)
#define VIN_WRAP_WDMA_IREQ_EOFF_MASK		(WDMAIRQSTS_EOFF_MASK)
#define VIN_WRAP_WDMA_IREQ_SEOFR_MASK		(WDMAIRQSTS_SEOFR_MASK)
#define VIN_WRAP_WDMA_IREQ_SEOFF_MASK		(WDMAIRQSTS_SEOFF_MASK)
#define VIN_WRAP_WDMA_IREQ_STSEN_MASK		(WDMAIRQSTS_ST_SEN_MASK)
#define VIN_WRAP_WDMA_IREQ_STBF_MASK		(WDMAIRQSTS_ST_BF_MASK)
#define VIN_WRAP_WDMA_IREQ_STEOF_MASK		(WDMAIRQSTS_ST_EOF_MASK)
#define VIN_WRAP_WDMA_IREQ_ALL_MASK			( \
						WDMAIRQSTS_UPD_MASK \
						| WDMAIRQSTS_SREQ_MASK \
						| WDMAIRQSTS_ROLL_MASK \
						| WDMAIRQSTS_ENR_MASK \
						| WDMAIRQSTS_ENF_MASK \
						| WDMAIRQSTS_EOFR_MASK \
						| WDMAIRQSTS_EOFF_MASK \
						| WDMAIRQSTS_SEOFR_MASK \
						| WDMAIRQSTS_SEOFF_MASK \
						| WDMAIRQSTS_ST_SEN_MASK \
						| WDMAIRQSTS_ST_BF_MASK \
						| WDMAIRQSTS_ST_EOF_MASK)

/* Interface APIs. */
void VIN_WRAP_WDMA_SetImageEnable(void __iomem *reg, unsigned int nContinuous);
void VIN_WRAP_WDMA_GetImageEnable(const void __iomem *reg, unsigned int *enable);
void VIN_WRAP_WDMA_SetImageDisable(void __iomem *reg);
void VIN_WRAP_WDMA_SetImageUpdate(void __iomem *reg);
void VIN_WRAP_WDMA_SetContinuousMode(void __iomem *reg,	unsigned int enable);
void VIN_WRAP_WDMA_SetImageFormat(void __iomem *reg, unsigned int nFormat);
void VIN_WRAP_WDMA_SetImageRGBSwapMode(void __iomem *reg, unsigned int rgb_mode);
void VIN_WRAP_WDMA_SetImageInterlaced(void __iomem *reg, unsigned int intl);
void VIN_WRAP_WDMA_SetImageR2YMode(void __iomem *reg, unsigned int r2y_mode);
void VIN_WRAP_WDMA_SetImageR2YEnable(void __iomem *reg, unsigned int enable);
void VIN_WRAP_WDMA_SetImageY2RMode(void __iomem *reg, unsigned int y2r_mode);
void VIN_WRAP_WDMA_SetImageY2REnable(void __iomem *reg, unsigned int enable);
void VIN_WRAP_WDMA_SetImageSize(void __iomem *reg, unsigned int sw, unsigned int sh);
void VIN_WRAP_WDMA_SetImageBase(void __iomem *reg, unsigned int nBase0,
				unsigned int nBase1, unsigned int nBase2);
void VIN_WRAP_WDMA_SetImageOffset(void __iomem *reg, unsigned int imgFmt, unsigned int imgWidth);
#ifdef L_STRIDE_ALIGN
void VIN_WRAP_WDMA_SetImageOffset_withYV12(void __iomem *reg, unsigned int imgWidth);
#endif
void VIN_WRAP_WDMA_SetIreqMask(void __iomem *reg, unsigned int mask, unsigned int set);
void VIN_WRAP_WDMA_SetIreqStatus(void __iomem *reg, unsigned int mask);
void VIN_WRAP_WDMA_ClearEOFR(void __iomem *reg);
void VIN_WRAP_WDMA_ClearEOFF(void __iomem *reg);

void VIN_WRAP_WDMA_GetStatus(const void __iomem *reg, unsigned int *status);
bool VIN_WRAP_WDMA_IsImageEnable(const void __iomem *reg);
bool VIN_WRAP_WDMA_IsContinuousMode(const void __iomem *reg);
unsigned int VIN_WRAP_WDMA_Get_CAddress(const void __iomem *reg);

void __iomem *VIN_WRAP_WDMA_GetAddress(unsigned int wdma_id);
void __iomem *VIN_WRAP_WDMA_VIN_GetAddress(unsigned int wdma_id);
void VIN_WRAPs_WDMA_DUMP(const void __iomem *reg, unsigned int wdma_id);

void vin_wrap_wdma_init(void);
#endif
