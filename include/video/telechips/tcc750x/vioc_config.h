/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef VIOC_CONFIG_H
#define	VIOC_CONFIG_H

/*
 * register offset
 */
#define RAWSTATUS0_OFFSET (0x000U)
#define RAWSTATUS1_OFFSET (0x004U)
#define RAWSTATUS2_OFFSET (0x008U)
#define RAWSTATUS3_OFFSET (0x00CU)
#define SYNCSTATUS0_OFFSET (0x010U)
#define SYNCSTATUS1_OFFSET (0x014U)
#define SYNCSTATUS2_OFFSET (0x018U)
#define SYNCSTATUS3_OFFSET (0x01CU)
#define VECTORID_OFFSET (0x028U)
#define TEST_LOOP_OFFSET (0x038U)
#define CFG_PATH_MC_OFFSET (0x03CU)
#define CFG_MISC0_OFFSET (0x040U)
#define CFG_PATH_SC0_OFFSET (0x044U)
#define CFG_PATH_SC1_OFFSET (0x048U)
#define CFG_PATH_SC2_OFFSET (0x04CU)
#define CFG_PATH_SC3_OFFSET (0x050U)
#define CFG_PATH_VIQE0_OFFSET (0x054U)
#define CFG_PATH_DEINTLS_OFFSET (0x058U)
#define CFG_PATH_FDLY_OFFSET (0x078U)
#define CFG_MISC1_OFFSET (0x084U)
#define CFG_DEV_SEL_OFFSET (0x0BCU)
#define ARID_OFFSET (0x0C0U)
#define AWID_OFFSET (0x0C4U)
#define PWR_AUTOPD_OFFSET (0x0C8U)
#define PWR_CLKCTRL_OFFSET (0x0CCU)
#define PWR_BLK_PWDN0_OFFSET (0x0D0U)
#define PWR_BLK_PWDN1_OFFSET (0x0D4U)
#define PWR_BLK_SWR0_OFFSET (0x0D8U)
#define PWR_BLK_SWR1_OFFSET (0x0DCU)
#define CFG_WMIX_PATH_SWR_OFFSET (0x0E8U)
#define PWR_BLK_PWDN2_OFFSET (0x0128U)
#define PWR_BLK_SWR3_OFFSET (0x012CU)
#define IRQSELECT0_0_OFFSET (0x400U)
#define IRQSELECT0_1_OFFSET (0x404U)
#define IRQSELECT0_2_OFFSET (0x408U)
#define IRQSELECT0_3_OFFSET (0x40CU)
#define IRQMASKSET0_0_OFFSET (0x410U)
#define IRQMASKSET0_1_OFFSET (0x414U)
#define IRQMASKSET0_2_OFFSET (0x418U)
#define IRQMASKSET0_3_OFFSET (0x41CU)
#define IRQMASKCLR0_0_OFFSET (0x420U)
#define IRQMASKCLR0_1_OFFSET (0x424U)
#define IRQMASKCLR0_2_OFFSET (0x428U)
#define IRQMASKCLR0_3_OFFSET (0x42CU)
#define IRQSELECT1_0_OFFSET (0x430U)
#define IRQSELECT1_1_OFFSET (0x434U)
#define IRQSELECT1_2_OFFSET (0x438U)
#define IRQSELECT1_3_OFFSET (0x43CU)
#define IRQMASKSET1_0_OFFSET (0x440U)
#define IRQMASKSET1_1_OFFSET (0x444U)
#define IRQMASKSET1_2_OFFSET (0x448U)
#define IRQMASKSET1_3_OFFSET (0x44CU)
#define IRQMASKCLR1_0_OFFSET (0x450U)
#define IRQMASKCLR1_1_OFFSET (0x454U)
#define IRQMASKCLR1_2_OFFSET (0x458U)
#define IRQMASKCLR1_3_OFFSET (0x45CU)
#define IRQSELECT2_0_OFFSET (0x460U)
#define IRQSELECT2_1_OFFSET (0x464U)
#define IRQSELECT2_2_OFFSET (0x468U)
#define IRQSELECT2_3_OFFSET (0x46CU)
#define IRQMASKSET2_0_OFFSET (0x470U)
#define IRQMASKSET2_1_OFFSET (0x474U)
#define IRQMASKSET2_2_OFFSET (0x478U)
#define IRQMASKSET2_3_OFFSET (0x47CU)
#define IRQMASKCLR2_0_OFFSET (0x480U)
#define IRQMASKCLR2_1_OFFSET (0x484U)
#define IRQMASKCLR2_2_OFFSET (0x488U)
#define IRQMASKCLR2_3_OFFSET (0x48CU)
#define IRQSELECT3_0_OFFSET (0x490U)
#define IRQSELECT3_1_OFFSET (0x494U)
#define IRQSELECT3_2_OFFSET (0x498U)
#define IRQSELECT3_3_OFFSET (0x49CU)
#define IRQMASKSET3_0_OFFSET (0x4A0U)
#define IRQMASKSET3_1_OFFSET (0x4A4U)
#define IRQMASKSET3_2_OFFSET (0x4A8U)
#define IRQMASKSET3_3_OFFSET (0x4ACU)
#define IRQMASKCLR3_0_OFFSET (0x4B0U)
#define IRQMASKCLR3_1_OFFSET (0x4B4U)
#define IRQMASKCLR3_2_OFFSET (0x4B8U)
#define IRQMASKCLR3_3_OFFSET (0x4BCU)

/*
 * Interrupt Status 0 Registers
 * @Description: 0 - Normal, 1 - Interrupt
 */
#define RAWSTATUS0_RD5_SHIFT (17U)	// RDMA05 interrupt Status
#define RAWSTATUS0_RD4_SHIFT (16U)	// RDMA04 interrupt Status
#define RAWSTATUS0_RD3_SHIFT (15U)	// RDMA03 interrupt Status
#define RAWSTATUS0_RD2_SHIFT (14U)	// RDMA02 interrupt Status
#define RAWSTATUS0_RD1_SHIFT (13U)	// RDMA01 interrupt Status
#define RAWSTATUS0_RD0_SHIFT (12U)	// RDMA00 interrupt Status
#define RAWSTATUS0_TCON0_SHIFT (0U) // Timing Controller 0 interrupt Status

#define RAWSTATUS0_RD5_MASK ((u32)0x1U << RAWSTATUS0_RD5_SHIFT)
#define RAWSTATUS0_RD4_MASK ((u32)0x1U << RAWSTATUS0_RD4_SHIFT)
#define RAWSTATUS0_RD3_MASK ((u32)0x1U << RAWSTATUS0_RD3_SHIFT)
#define RAWSTATUS0_RD2_MASK ((u32)0x1U << RAWSTATUS0_RD2_SHIFT)
#define RAWSTATUS0_RD1_MASK ((u32)0x1U << RAWSTATUS0_RD1_SHIFT)
#define RAWSTATUS0_RD0_MASK ((u32)0x1U << RAWSTATUS0_RD0_SHIFT)
#define RAWSTATUS0_TCON0_MASK ((u32)0x1U << RAWSTATUS0_TCON0_SHIFT)

/*
 * Interrupt Status 1 Registers
 * @Description: 0 - Normal, 1 - Interrupt
 */
#define RAWSTATUS1_WMIX1_SHIFT (31U)	// WMIX1 Interrupt Status
#define RAWSTATUS1_WMIX0_SHIFT (30U)	// WMIX0 Interrupt Status
#define RAWSTATUS1_SC1_SHIFT (21U)	// Scaler1 Interrupt Status
#define RAWSTATUS1_SC0_SHIFT (20U)	// Scaler0 Interrupt Status

#define RAWSTATUS1_WMIX1_MASK ((u32)0x1U << RAWSTATUS1_WMIX1_SHIFT)
#define RAWSTATUS1_WMIX0_MASK ((u32)0x1U << RAWSTATUS1_WMIX0_SHIFT)
#define RAWSTATUS1_SC1_MASK ((u32)0x1U << RAWSTATUS1_SC1_SHIFT)
#define RAWSTATUS1_SC0_MASK ((u32)0x1U << RAWSTATUS1_SC0_SHIFT)

/*
 * Interrupt Status 2 Register
 */
#define RAWSTATUS2_WD2_SHIFT (10U)	// WD02 Interrupt Status
#define RAWSTATUS2_WD1_SHIFT (9U)	// WD01 Interrupt Status
#define RAWSTATUS2_WD0_SHIFT (8U)	// WD00 Interrupt Status

#define RAWSTATUS2_WD2_MASK ((u32)0x1U << RAWSTATUS2_WD2_SHIFT)
#define RAWSTATUS2_WD1_MASK ((u32)0x1U << RAWSTATUS2_WD1_SHIFT)
#define RAWSTATUS2_WD0_MASK ((u32)0x1U << RAWSTATUS2_WD0_SHIFT)

/*
 * Interrupt Status 3 Register
 */
#define RAWSTATUS3_FIFO1_SHIFT (7U)	// FIFO1 Interrupt Status
#define RAWSTATUS3_FIFO0_SHIFT (6U)	// FIFO0 Interrupt Status
#define RAWSTATUS3_TIMER_SHIFT (4U)	// TIMER Interrupt Status

#define RAWSTATUS3_FIFO1_MASK ((u32)0x1U << RAWSTATUS3_FIFO1_SHIFT)
#define RAWSTATUS3_FIFO0_MASK ((u32)0x1U << RAWSTATUS3_FIFO0_SHIFT)
#define RAWSTATUS3_TIMER_MASK ((u32)0x1U << RAWSTATUS3_TIMER_SHIFT)


/*
 * Sync Interrupt Status 0 Registers
 * @Description: 0 - Normal, 1 - Interrupt
 */
#define SYNCSTATUS0_RD5_SHIFT (17U)	// RDMA05 Sync Interrupt Status
#define SYNCSTATUS0_RD4_SHIFT (16U)	// RDMA04 Sync Interrupt Status
#define SYNCSTATUS0_RD3_SHIFT (15U)	// RDMA03 Sync Interrupt Status
#define SYNCSTATUS0_RD2_SHIFT (14U)	// RDMA02 Sync Interrupt Status
#define SYNCSTATUS0_RD1_SHIFT (13U)	// RDMA01 Sync Interrupt Status
#define SYNCSTATUS0_RD0_SHIFT (12U)	// RDMA00 Sync Interrupt Status
#define SYNCSTATUS0_TCON0_SHIFT (0U)	// TCON0 Sync Interrupt Status

#define SYNCSTATUS0_RD5_MASK ((u32)0x1U << SYNCSTATUS0_RD5_SHIFT)
#define SYNCSTATUS0_RD4_MASK ((u32)0x1U << SYNCSTATUS0_RD4_SHIFT)
#define SYNCSTATUS0_RD3_MASK ((u32)0x1U << SYNCSTATUS0_RD3_SHIFT)
#define SYNCSTATUS0_RD2_MASK ((u32)0x1U << SYNCSTATUS0_RD2_SHIFT)
#define SYNCSTATUS0_RD1_MASK ((u32)0x1U << SYNCSTATUS0_RD1_SHIFT)
#define SYNCSTATUS0_RD0_MASK ((u32)0x1U << SYNCSTATUS0_RD0_SHIFT)
#define SYNCSTATUS0_TCON0_MASK ((u32)0x1U << SYNCSTATUS0_TCON0_SHIFT)


/*
 * Sync Interrupt Status 1 Registers
 * @Description: 0 - Normal, 1 - Interrupt
 */

#define SYNCSTATUS1_WMIX1_SHIFT (31U)	// WMIX1 Sync Interrupt Status
#define SYNCSTATUS1_WMIX0_SHIFT (30U)	// WMIX0 Sync Interrupt Status
#define SYNCSTATUS1_SC1_SHIFT (21U)	// Scaler1 Sync Interrupt Status
#define SYNCSTATUS1_SC0_SHIFT (20U)	// Scaler0 Sync Interrupt Status

#define SYNCSTATUS1_WMIX1_MASK ((u32)0x1U << SYNCSTATUS1_WMIX1_SHIFT)
#define SYNCSTATUS1_WMIX0_MASK ((u32)0x1U << SYNCSTATUS1_WMIX0_SHIFT)
#define SYNCSTATUS1_SC1_MASK ((u32)0x1U << SYNCSTATUS1_SC1_SHIFT)
#define SYNCSTATUS1_SC0_MASK ((u32)0x1U << SYNCSTATUS1_SC0_SHIFT)

/*
 * Sync Interrupt Status 2 Register
 */
#define SYNCSTATUS2_WD2_SHIFT (10U) // WDMA02 Sync Interrupt Status
#define SYNCSTATUS2_WD1_SHIFT (9U) // WDMA01 Sync Interrupt Status
#define SYNCSTATUS2_WD0_SHIFT (8U) // WDMA00 Sync Interrupt Status

#define SYNCSTATUS2_WD2_MASK ((u32)0x1U << SYNCSTATUS2_WD2_SHIFT)
#define SYNCSTATUS2_WD1_MASK ((u32)0x1U << SYNCSTATUS2_WD1_SHIFT)
#define SYNCSTATUS2_WD0_MASK ((u32)0x1U << SYNCSTATUS2_WD0_SHIFT)

/*
 * Sync Interrupt Status 3 Register
 */
#define SYNCSTATUS3_FIFO1_SHIFT (7U) // FIFO1 Sync Interrupt Status
#define SYNCSTATUS3_FIFO0_SHIFT (6U) // FIFO0 Sync Interrupt Status
#define SYNCSTATUS3_TIMER_SHIFT (4U) // TIMER Sync Interrupt Status

#define SYNCSTATUS3_FIFO1_MASK ((u32)0x1U << SYNCSTATUS3_FIFO1_SHIFT)
#define SYNCSTATUS3_FIFO0_MASK ((u32)0x1U << SYNCSTATUS3_FIFO0_SHIFT)
#define SYNCSTATUS3_TIMER_MASK ((u32)0x1U << SYNCSTATUS3_TIMER_SHIFT)

/*
 * Vector ID Registers
 */
#define VECTORID_IVALID3_SHIFT (31U)	// Invalid Interrupt
#define VECTORID_IVALID2_SHIFT (30U)	// Invalid Interrupt
#define VECTORID_IVALID1_SHIFT (29U)	// Invalid Interrupt
#define VECTORID_IVALID0_SHIFT (28U)	// Invalid Interrupt
#define	VECTORID_INDEX3_SHIFT (21U)	// Interrupt Index
#define	VECTORID_INDEX2_SHIFT (14U)	// Interrupt Index
#define	VECTORID_INDEX1_SHIFT (7U)		// Interrupt Index
#define	VECTORID_INDEX0_SHIFT (0U)		// Interrupt Index

#define VECTORID_IVALID3_MASK ((u32)0x1U << VECTORID_IVALID3_SHIFT)
#define VECTORID_IVALID2_MASK ((u32)0x1U << VECTORID_IVALID2_SHIFT)
#define VECTORID_IVALID1_MASK ((u32)0x1U << VECTORID_IVALID1_SHIFT)
#define VECTORID_IVALID0_MASK ((u32)0x1U << VECTORID_IVALID0_SHIFT)
#define	VECTORID_INDEX3_MASK ((u32)0x7FU << VECTORID_INDEX3_SHIFT)
#define	VECTORID_INDEX2_MASK ((u32)0x7FU << VECTORID_INDEX2_SHIFT)
#define	VECTORID_INDEX1_MASK ((u32)0x7FU << VECTORID_INDEX1_SHIFT)
#define	VECTORID_INDEX0_MASK ((u32)0x7FU << VECTORID_INDEX0_SHIFT)

/*
 * Loop for Test Configuration Registers
 */
#define TEST_LOOP_LVIN7_SHIFT (14U)	// Loopback select for VIN7 (FOR DEBUG)
#define TEST_LOOP_LVIN6_SHIFT (12U)	// Loopback select for VIN6 (FOR DEBUG)
#define TEST_LOOP_LVIN5_SHIFT (10U)	// Loopback select for VIN5 (FOR DEBUG)
#define TEST_LOOP_LVIN4_SHIFT (8U)	// Loopback select for VIN4 (FOR DEBUG)
#define TEST_LOOP_LVIN3_SHIFT (6U) // Loopback select for VIN3 (FOR DEBUG)
#define TEST_LOOP_LVIN2_SHIFT (4U) // Loopback select for VIN2 (FOR DEBUG)
#define TEST_LOOP_LVIN1_SHIFT (2U) // Loopback select for VIN1 (FOR DEBUG)
#define TEST_LOOP_LVIN0_SHIFT (0U) // Loopback select for VIN0 (FOR DEBUG)

#define TEST_LOOP_LVIN7_MASK ((u32)0x3U << TEST_LOOP_LVIN7_SHIFT)
#define TEST_LOOP_LVIN6_MASK ((u32)0x3U << TEST_LOOP_LVIN6_SHIFT)
#define TEST_LOOP_LVIN5_MASK ((u32)0x3U << TEST_LOOP_LVIN5_SHIFT)
#define TEST_LOOP_LVIN4_MASK ((u32)0x3U << TEST_LOOP_LVIN4_SHIFT)
#define TEST_LOOP_LVIN3_MASK ((u32)0x3U << TEST_LOOP_LVIN3_SHIFT)
#define TEST_LOOP_LVIN2_MASK ((u32)0x3U << TEST_LOOP_LVIN2_SHIFT)
#define TEST_LOOP_LVIN1_MASK ((u32)0x3U << TEST_LOOP_LVIN1_SHIFT)
#define TEST_LOOP_LVIN0_MASK ((u32)0x3U << TEST_LOOP_LVIN0_SHIFT)

/*
 * Map Conv Path Configuration Register
 */
#define CFG_PATH_MC_MC1_SEL_SHIFT (16U)
#define CFG_PATH_MC_RD17_SHIFT (6U)
#define CFG_PATH_MC_RD16_SHIFT (5U)
#define CFG_PATH_MC_RD15_SHIFT (4U)
#define CFG_PATH_MC_RD13_SHIFT (3U)
#define CFG_PATH_MC_RD11_SHIFT (2U)
#define CFG_PATH_MC_RD07_SHIFT (1U)
#define CFG_PATH_MC_RD03_SHIFT (0U)

#define CFG_PATH_MC_MC1_SEL_MASK ((u32)0x7U << CFG_PATH_MC_MC1_SEL_SHIFT)
#define CFG_PATH_MC_RD17_MASK ((u32)0x1U << CFG_PATH_MC_RD17_SHIFT)
#define CFG_PATH_MC_RD16_MASK ((u32)0x1U << CFG_PATH_MC_RD16_SHIFT)
#define CFG_PATH_MC_RD15_MASK ((u32)0x1U << CFG_PATH_MC_RD15_SHIFT)
#define CFG_PATH_MC_RD13_MASK ((u32)0x1U << CFG_PATH_MC_RD13_SHIFT)
#define CFG_PATH_MC_RD11_MASK ((u32)0x1U << CFG_PATH_MC_RD11_SHIFT)
#define CFG_PATH_MC_RD07_MASK ((u32)0x1U << CFG_PATH_MC_RD07_SHIFT)
#define CFG_PATH_MC_RD03_MASK ((u32)0x1U << CFG_PATH_MC_RD03_SHIFT)

/*
 * Miscellaneous0 Configuration Registers
 */
#define CFG_MISC0_MIX10_SHIFT (18U)
// WMIX1 Path Control for 0��?th Input Channel
#define CFG_MISC0_MIX03_SHIFT (17U)
// WMIX0 Path Control for 3��?rd Input Channel
#define CFG_MISC0_MIX00_SHIFT (16U)
// WMIX0 Path Control for 0��?th Input Channel
#define CFG_MISC0_L0_EVS_SEL_SHIFT (0U)
// Select VS signal for Display Device Output Port 0

#define CFG_MISC0_MIX10_MASK ((u32)0x1U <<  CFG_MISC0_MIX10_SHIFT)
#define CFG_MISC0_MIX03_MASK ((u32)0x1U <<  CFG_MISC0_MIX03_SHIFT)
#define CFG_MISC0_MIX00_MASK ((u32)0x1U <<  CFG_MISC0_MIX00_SHIFT)
#define CFG_MISC0_L0_EVS_SEL_MASK ((u32)0xFU <<  CFG_MISC0_L0_EVS_SEL_SHIFT)

/* Scaler register fileds are all the same */
#define CFG_PATH_EN_SHIFT (31U)	// PATH Enable
#define CFG_PATH_ERR_SHIFT (18U)	// Device Error
#define CFG_PATH_STS_SHIFT (16U)	// Path Status
#define CFG_PATH_SEL_SHIFT (0U)	// Path Selection

#define CFG_PATH_EN_MASK ((u32)0x1U << CFG_PATH_EN_SHIFT)
#define CFG_PATH_ERR_MASK ((u32)0x1U << CFG_PATH_ERR_SHIFT)
#define CFG_PATH_STS_MASK ((u32)0x3U << CFG_PATH_STS_SHIFT)
#define CFG_PATH_SEL_MASK ((u32)0xFFU << CFG_PATH_SEL_SHIFT)

/*
 * Miscellaneous1 Configuration Registers
 */
#define CFG_MISC1_S_REQ_SHIFT (23U)	// Disable STOP Request
#define CFG_MISC1_AXIRD_M1_TR_SHIFT (14U)	// For Debug
#define CFG_MISC1_AXIRD_M0_TR_SHIFT (12U)	// For Debug
#define CFG_MISC1_AXIRD_SHIFT (8U)	// For Debug
#define CFG_MISC1_AXIWD_SHIFT (0U)	// For Debug

#define CFG_MISC1_S_REQ_MASK ((u32)0x1U << CFG_MISC1_S_REQ_SHIFT)
#define CFG_MISC1_AXIRD_M1_TR_MASK ((u32)0x3U << CFG_MISC1_AXIRD_M1_TR_SHIFT)
#define CFG_MISC1_AXIRD_M0_TR_MASK ((u32)0x3U << CFG_MISC1_AXIRD_M0_TR_SHIFT)
#define CFG_MISC1_AXIRD_MASK ((u32)0x1U << CFG_MISC1_AXIRD_SHIFT)
#define CFG_MISC1_AXIWD_MASK ((u32)0x1U << CFG_MISC1_AXIWD_SHIFT)

/*
 * Display Controller Configuration Registers
 */
#define CFG_DEV_SEL_P0_EN_SHIFT (7U)	// DISP0 Path Enable
#define CFG_DEV_SEL_DEV0_STAT_SHIFT (4U)	// Path Status
#define CFG_DEV_SEL_DEV0_PATH_SHIFT (0U)	// DISP0 Path Select

#define CFG_DEV_SEL_P0_EN_MASK ((u32)0x1U << CFG_DEV_SEL_P0_EN_SHIFT)
#define CFG_DEV_SEL_DEV0_STAT_MASK ((u32)0x7U << CFG_DEV_SEL_DEV0_STAT_SHIFT)
#define CFG_DEV_SEL_DEV0_PATH_MASK ((u32)0x3U << CFG_DEV_SEL_DEV0_PATH_SHIFT)

/*
 * ARID of DMA Registers
 */
#define ARID_SHIFT (0U)	// ARID of Master DMA

#define ARID_MASK ((u32)0x1FFFFFFFU << ARID_SHIFT)

/*
 * AWID of DMA Registers
 */
#define AWID_SHIFT (0U)	// AWID of Master DMA

#define AWID_MASK ((u32)0x7FFFFFFFU << AWID_SHIFT)

/*
 * Power Auto Power Down Registers
 * @Description: 0 - Normal, 1 - Auto PWDN
 */
#define PWR_AUTOPD_DEVMX_SHIFT (14U) // Mixer for DISP Auto Power Down Mode
#define PWR_AUTOPD_WDMA_SHIFT (3U) // WDMA Auto Power Down Mode
#define PWR_AUTOPD_MIX_SHIFT (2U) // WMIX Auto Power Down Mode
#define PWR_AUTOPD_SC_SHIFT (1U) // Scaler Auto Power Down Mode
#define PWR_AUTOPD_RDMA_SHIFT (0U) // RDMA Auto Power Down Mode

#define PWR_AUTOPD_DEVMX_MASK ((u32)0x1U << PWR_AUTOPD_DEVMX_SHIFT)
#define PWR_AUTOPD_WDMA_MASK ((u32)0x1U << PWR_AUTOPD_WDMA_SHIFT)
#define PWR_AUTOPD_MIX_MASK ((u32)0x1U << PWR_AUTOPD_MIX_SHIFT)
#define PWR_AUTOPD_SC_MASK ((u32)0x1U << PWR_AUTOPD_SC_SHIFT)
#define PWR_AUTOPD_RDMA_MASK ((u32)0x1U << PWR_AUTOPD_RDMA_SHIFT)

/*
 * Power Clock Control Registers
 * @Description: 0 - Disable, 1 - Enable
 */
#define PWR_CLKCTRL_PFDATA_SHIFT (16U)	// Clock Profile Data
#define PWR_CLKCTRL_PFDONE_SHIFT (15U)	// Clock Profile Done
#define PWR_CLKCTRL_PFEN_SHIFT (9U)	// Clock Profile Enable
#define PWR_CLKCTRL_EN_SHIFT (8U)	// Clock Control (Clock Gating) Enable
#define PWR_CLKCTRL_MIN_SHIFT (0U)	// Clock Disable Minimum size

#define PWR_CLKCTRL_PFDATA_MASK ((u32)0xFFFFU << PWR_CLKCTRL_PFDATA_SHIFT)
#define PWR_CLKCTRL_PFDONE_MASK ((u32)0x1U << PWR_CLKCTRL_PFDONE_SHIFT)
#define PWR_CLKCTRL_PFEN_MASK ((u32)0x1U << PWR_CLKCTRL_PFEN_SHIFT)
#define PWR_CLKCTRL_EN_MASK ((u32)0x1U << PWR_CLKCTRL_EN_SHIFT)
#define PWR_CLKCTRL_MIN_MASK ((u32)0xFU << PWR_CLKCTRL_MIN_SHIFT)

/*
 * Power Block Power Down 0 Registers
 * @Description: 0 - Normal, 1 - PWDN
 */
#define PWR_BLK_PWDN0_SC_SHIFT (28U)	// Scaler Block Power Down
#define PWR_BLK_PWDN0_RDMA_SHIFT (0U)	// RDMA Power Down

#define PWR_BLK_PWDN0_SC_MASK ((u32)0xFU << PWR_BLK_PWDN0_SC_SHIFT)
#define PWR_BLK_PWDN0_RDMA_MASK ((u32)0x3FFFFU << PWR_BLK_PWDN0_RDMA_SHIFT)

/*
 * Power Block Power Down 1 Registers
 * @Description: 0 - Normal, 1 - PWDN
 */
#define PWR_BLK_PWDN1_FIFO_SHIFT (31U)	// Frame FIFO Power Down
#define PWR_BLK_PWDN1_TCON_SHIFT (20U)  // Timing Controller Power Down
#define PWR_BLK_PWDN1_WMIX_SHIFT (9U)	// WMIX Power Down
#define PWR_BLK_PWDN1_WDMA_SHIFT (0U)	// WDMA Power Down

#define PWR_BLK_PWDN1_FIFO_MASK ((u32)0x1U << PWR_BLK_PWDN1_FIFO_SHIFT)
#define PWR_BLK_PWDN1_TCON_MASK ((u32)0x7U << PWR_BLK_PWDN1_TCON_SHIFT)
#define PWR_BLK_PWDN1_WMIX_MASK ((u32)0x7FU << PWR_BLK_PWDN1_WMIX_SHIFT)
#define PWR_BLK_PWDN1_WDMA_MASK ((u32)0x1FFU << PWR_BLK_PWDN1_WDMA_SHIFT)

/*
 * Power Block SWRESET 0 Registers
 * @Description: 0 - Normal, 1 - Reset
 */
#define PWR_BLK_SWR0_SC_SHIFT (28U)	// Scaler Block Reset
#define PWR_BLK_SWR0_RDMA_SHIFT (0U)	// RDMA Reset

#define PWR_BLK_SWR0_SC_MASK ((u32)0xFU << PWR_BLK_SWR0_SC_SHIFT)
#define PWR_BLK_SWR0_RDMA_MASK ((u32)0x3FFFFU  << PWR_BLK_SWR0_RDMA_SHIFT)

/*
 * Power Block SWRESET 1 Registers
 * @Description: 0 - Normal, 1 - Reset
 */
#define PWR_BLK_SWR1_FIFO_SHIFT (31U)	// Frame FIFO Reset
#define PWR_BLK_SWR1_DEV_SHIFT  (20U)	// Dev Reset
#define PWR_BLK_SWR1_WMIX_SHIFT (9U)	// WMIX Reset
#define PWR_BLK_SWR1_WDMA_SHIFT (0U)	// WDMA Reset

#define PWR_BLK_SWR1_FIFO_MASK ((u32)0x1U << PWR_BLK_SWR1_FIFO_SHIFT)
#define PWR_BLK_SWR1_DEV_MASK  ((u32)0x7U << PWR_BLK_SWR1_DEV_SHIFT)
#define PWR_BLK_SWR1_WMIX_MASK ((u32)0x7FU << PWR_BLK_SWR1_WMIX_SHIFT)
#define PWR_BLK_SWR1_WDMA_MASK ((u32)0x1FFU << PWR_BLK_SWR1_WDMA_SHIFT)

/*
 * WMIX PATH SWRESET Register
 * @Description: 0 - Normal, 1 - Mixing PATH reset
 */
#define WMIX_PATH_SWR_MIX10_SHIFT (2U)  // MIX10
#define WMIX_PATH_SWR_MIX03_SHIFT (1U)  // MIX03
#define WMIX_PATH_SWR_MIX00_SHIFT (0U)  // MIX00

#define WMIX_PATH_SWR_MIX10_MASK ((u32)0x1U << WMIX_PATH_SWR_MIX10_SHIFT)
#define WMIX_PATH_SWR_MIX03_MASK ((u32)0x1U << WMIX_PATH_SWR_MIX03_SHIFT)
#define WMIX_PATH_SWR_MIX00_MASK ((u32)0x1U << WMIX_PATH_SWR_MIX00_SHIFT)

/*
 * Power Block Power Down 2 Registers
 * @Description: 0 - Normal, 1 - PWD
 */
#define PWR_BLK_PWDN2_SC_SHIFT (16U) // Scaler Block Power Down

#define PWR_BLK_PWDN2_SC_MASK ((u32)0xFFU << PWR_BLK_PWDN2_SC_SHIFT)

/*
 * Power Block SWRESET 3 Registers
 * @Description: 0 - Normal, 1 - Reset
 */
#define PWR_BLK_SWR3_SC_SHIFT (16U) // Scaler Block Reset

#define PWR_BLK_SWR3_SC_MASK ((u32)0xFFU << PWR_BLK_SWR3_SC_SHIFT)

/*
 * Interrupt Select 0 Registers
 * @Description: 0 - Async Interrupt, 1 - Sync Interrupt
 */

#define IRQSELECT0_RD5_SHIFT (17U)	// RDMA05 Sync Interrupt Select
#define IRQSELECT0_RD4_SHIFT (16U)	// RDMA04 Sync Interrupt Select
#define IRQSELECT0_RD3_SHIFT (15U)	// RDMA03 Sync Interrupt Select
#define IRQSELECT0_RD2_SHIFT (14U)	// RDMA02 Sync Interrupt Select
#define IRQSELECT0_RD1_SHIFT (13U)	// RDMA01 Sync Interrupt Select
#define IRQSELECT0_RD0_SHIFT (12U)	// RDMA00 Sync Interrupt Select
#define IRQSELECT0_TCON0_SHIFT (0U) // Display Device 0 Sync Interrupt Select


#define IRQSELECT0_RD5_MASK ((u32)0x1U <<	IRQSELECT0_RD5_SHIFT)
#define IRQSELECT0_RD4_MASK ((u32)0x1U <<	IRQSELECT0_RD4_SHIFT)
#define IRQSELECT0_RD3_MASK ((u32)0x1U <<	IRQSELECT0_RD3_SHIFT)
#define IRQSELECT0_RD2_MASK ((u32)0x1U <<	IRQSELECT0_RD2_SHIFT)
#define IRQSELECT0_RD1_MASK ((u32)0x1U <<	IRQSELECT0_RD1_SHIFT)
#define IRQSELECT0_RD0_MASK ((u32)0x1U <<	IRQSELECT0_RD0_SHIFT)
#define IRQSELECT0_TCON0_MASK ((u32)0x1U << IRQSELECT0_TCON0_SHIFT)

/*
 * Interrupt Select 1 Registers
 * @Description: 0 - Normal, 1 - Interrupt
 */
#define IRQSELECT1_WMIX1_SHIFT (31U)	// WMIX1 Sync Interrupt Select
#define IRQSELECT1_WMIX0_SHIFT (30U)	// WMIX0 Sync Interrupt Select
#define IRQSELECT1_SC1_SHIFT (21U)	// Scaler1 Sync Interrupt Select
#define IRQSELECT1_SC0_SHIFT (20U)	// Scaler0 Sync Interrupt Select

#define IRQSELECT1_WMIX1_MASK ((u32)0x1U << IRQSELECT1_WMIX1_SHIFT)
#define IRQSELECT1_WMIX0_MASK ((u32)0x1U << IRQSELECT1_WMIX0_SHIFT)
#define IRQSELECT1_SC1_MASK ((u32)0x1U << IRQSELECT1_SC1_SHIFT)
#define IRQSELECT1_SC0_MASK ((u32)0x1U << IRQSELECT1_SC0_SHIFT)

/*
 * Interrupt Select k_2 Register
 */
#define IRQSELECT2_WD2_SHIFT (10U)	// WDMA2 Interrupt Select
#define IRQSELECT2_WD1_SHIFT (9U)	// WDMA1 Interrupt Select
#define IRQSELECT2_WD0_SHIFT (8U)	// WDMA0 Interrupt Select

#define IRQSELECT2_WD2_MASK ((u32)0x1U << IRQSELECT2_WD2_SHIFT)
#define IRQSELECT2_WD1_MASK ((u32)0x1U << IRQSELECT2_WD1_SHIFT)
#define IRQSELECT2_WD0_MASK ((u32)0x1U << IRQSELECT2_WD0_SHIFT)

/*
 * Interrupt Select k_3 Register
 */
#define IRQSELECT3_FIFO1_SHIFT (7U)	// FIFO1 Interrupt Select
#define IRQSELECT3_FIFO0_SHIFT (6U)	// FIFO0 Interrupt Select
#define IRQSELECT3_TIMER_SHIFT (4U)	// TIMER Interrupt Select

#define IRQSELECT3_FIFO1_MASK ((u32)0x1U << IRQSELECT3_FIFO1_SHIFT)
#define IRQSELECT3_FIFO0_MASK ((u32)0x1U << IRQSELECT3_FIFO0_SHIFT)
#define IRQSELECT3_TIMER_MASK ((u32)0x1U << IRQSELECT3_TIMER_SHIFT)

/*
 * Interrupt Mask Set 0 Registers
 * @Description: 0 - Enable, 1 - Disable
 */
#define IRQMASKSET0_RD5_SHIFT (17U)	// RDMA05 Interrupt Mask
#define IRQMASKSET0_RD4_SHIFT (16U)	// RDMA04 Interrupt Mask
#define IRQMASKSET0_RD3_SHIFT (15U)	// RDMA03 Interrupt Mask
#define IRQMASKSET0_RD2_SHIFT (14U)	// RDMA02 Interrupt Mask
#define IRQMASKSET0_RD1_SHIFT (13U)	// RDMA01 Interrupt Mask
#define IRQMASKSET0_RD0_SHIFT (12U)	// RDMA00 Interrupt Mask
#define IRQMASKSET0_TCON0_SHIFT (0U)	// TCON0 Interrupt Mask

#define IRQMASKSET0_RD5_MASK ((u32)0x1U <<	IRQMASKSET0_RD5_SHIFT)
#define IRQMASKSET0_RD4_MASK ((u32)0x1U <<	IRQMASKSET0_RD4_SHIFT)
#define IRQMASKSET0_RD3_MASK ((u32)0x1U <<	IRQMASKSET0_RD3_SHIFT)
#define IRQMASKSET0_RD2_MASK ((u32)0x1U <<	IRQMASKSET0_RD2_SHIFT)
#define IRQMASKSET0_RD1_MASK ((u32)0x1U <<	IRQMASKSET0_RD1_SHIFT)
#define IRQMASKSET0_RD0_MASK ((u32)0x1U <<	IRQMASKSET0_RD0_SHIFT)
#define IRQMASKSET0_TCON0_MASK ((u32)0x1U <<	IRQMASKSET0_TCON0_SHIFT)

/*
 * Interrupt Mask Set 1 Registers
 * @Description: 0 - Normal, 1 - Interrupt
 */
#define IRQMASKSET1_WMIX1_SHIFT (31U) // WMIX1 Interrupt Mask
#define IRQMASKSET1_WMIX0_SHIFT (30U) // WMIX0 Interrupt Mask
#define IRQMASKSET1_SC1_SHIFT (21U) // Scaler1 Interrupt Mask
#define IRQMASKSET1_SC0_SHIFT (20U) // Scaler0 Interrupt Mask

#define IRQMASKSET1_WMIX1_MASK ((u32)0x1U << IRQMASKSET1_WMIX1_SHIFT)
#define IRQMASKSET1_WMIX0_MASK ((u32)0x1U << IRQMASKSET1_WMIX0_SHIFT)
#define IRQMASKSET1_SC1_MASK ((u32)0x1U << IRQMASKSET1_SC1_SHIFT)
#define IRQMASKSET1_SC0_MASK ((u32)0x1U << IRQMASKSET1_SC0_SHIFT)

/*
 * Interrupt Mask Set k_2 Register
 */
#define IRQMASKSET2_WD2_SHIFT (10U) // WDMA2 Interrupt Mask
#define IRQMASKSET2_WD1_SHIFT (9U) // WDMA1 Interrupt Mask
#define IRQMASKSET2_WD0_SHIFT (8U) // WDMA0 Interrupt Mask

#define IRQMASKSET2_WD2_MASK ((u32)0x1U << IRQMASKSET2_WD2_SHIFT)
#define IRQMASKSET2_WD1_MASK ((u32)0x1U << IRQMASKSET2_WD1_SHIFT)
#define IRQMASKSET2_WD0_MASK ((u32)0x1U << IRQMASKSET2_WD0_SHIFT)

/*
 * Interrupt Mask Set k_3 Register
 */
#define IRQMASKSET3_FIFO1_SHIFT (9U) // FIFO1 Interrupt Mask
#define IRQMASKSET3_FIFO0_SHIFT (8U) // FIFO0 Interrupt Mask
#define IRQMASKSET3_TIMER_SHIFT (6U) // TIMER Interrupt Mask

#define IRQMASKSET3_FIFO1_MASK ((u32)0x1U << IRQMASKSET3_FIFO1_SHIFT)
#define IRQMASKSET3_FIFO0_MASK ((u32)0x1U << IRQMASKSET3_FIFO0_SHIFT)
#define IRQMASKSET3_TIMER_MASK ((u32)0x1U << IRQMASKSET3_TIMER_SHIFT)

/*
 * Interrupt Mask Clear 0 Registers
 * @Description: 0 - Normal, 1 - Clear
 */
#define IRQMASKCLR0_RD5_SHIFT (17U)	// RDMA05 Interrupt Mask Clear
#define IRQMASKCLR0_RD4_SHIFT (16U)	// RDMA04 Interrupt Mask Clear
#define IRQMASKCLR0_RD3_SHIFT (15U)	// RDMA03 Interrupt Mask Clear
#define IRQMASKCLR0_RD2_SHIFT (14U)	// RDMA02 Interrupt Mask Clear
#define IRQMASKCLR0_RD1_SHIFT (13U)	// RDMA01 Interrupt Mask Clear
#define IRQMASKCLR0_RD0_SHIFT (12U)	// RDMA00 Interrupt Mask Clear
#define IRQMASKCLR0_TCON0_SHIFT (0U)	// TCON0 Interrupt Mask Clear

#define IRQMASKCLR0_RD5_MASK ((u32)0x1U << IRQMASKCLR0_RD5_SHIFT)
#define IRQMASKCLR0_RD4_MASK ((u32)0x1U << IRQMASKCLR0_RD4_SHIFT)
#define IRQMASKCLR0_RD3_MASK ((u32)0x1U << IRQMASKCLR0_RD3_SHIFT)
#define IRQMASKCLR0_RD2_MASK ((u32)0x1U << IRQMASKCLR0_RD2_SHIFT)
#define IRQMASKCLR0_RD1_MASK ((u32)0x1U << IRQMASKCLR0_RD1_SHIFT)
#define IRQMASKCLR0_RD0_MASK ((u32)0x1U << IRQMASKCLR0_RD0_SHIFT)
#define IRQMASKCLR0_TCON0_MASK ((u32)0x1U << IRQMASKCLR0_TCON0_SHIFT)

/*
 * Interrupt Mask Clear Set 1 Registers
 * @Description: 0 - Normal, 1 - Interrupt
 */
#define IRQMASKCLR1_WMIX1_SHIFT (31U) // WMIX1 Interrupt Mask Clear
#define IRQMASKCLR1_WMIX0_SHIFT (30U) // WMIX0 Interrupt Mask Clear
#define IRQMASKCLR1_SC1_SHIFT (21U) // Scaler1 Interrupt Mask Clear
#define IRQMASKCLR1_SC0_SHIFT (20U) // Scaler0 Interrupt Mask Clear

#define IRQMASKCLR1_WMIX1_MASK ((u32)0x1U << IRQMASKCLR1_WMIX1_SHIFT)
#define IRQMASKCLR1_WMIX0_MASK ((u32)0x1U << IRQMASKCLR1_WMIX0_SHIFT)
#define IRQMASKCLR1_SC1_MASK ((u32)0x1U << IRQMASKCLR1_SC1_SHIFT)
#define IRQMASKCLR1_SC0_MASK ((u32)0x1U << IRQMASKCLR1_SC0_SHIFT)

/*
 * Interrupt Mask Set k_2 Register
 */
#define IRQMASKCLR2_WD2_SHIFT (10U) // WDMA2 Interrupt Mask Clear
#define IRQMASKCLR2_WD1_SHIFT (9U) // WDMA1 Interrupt Mask Clear
#define IRQMASKCLR2_WD0_SHIFT (8U) // WDMA0 Interrupt Mask Clear

#define IRQMASKCLR2_WD2_MASK ((u32)0x1U << IRQMASKCLR2_WD2_SHIFT)
#define IRQMASKCLR2_WD1_MASK ((u32)0x1U << IRQMASKCLR2_WD1_SHIFT)
#define IRQMASKCLR2_WD0_MASK ((u32)0x1U << IRQMASKCLR2_WD0_SHIFT)

/*
 * Interrupt Mask Set k_3 Register
 */
#define IRQMASKCLR3_FIFO1_SHIFT (7U) // FIFO1 Interrupt Mask Clear
#define IRQMASKCLR3_FIFO0_SHIFT (6U) // FIFO0 Interrupt Mask Clear
#define IRQMASKCLR3_TIMER_SHIFT (4U) // TIMER Interrupt Mask Clear

#define IRQMASKCLR3_FIFO1_MASK ((u32)0x1U << IRQMASKCLR3_FIFO1_SHIFT)
#define IRQMASKCLR3_FIFO0_MASK ((u32)0x1U << IRQMASKCLR3_FIFO0_SHIFT)
#define IRQMASKCLR3_TIMER_MASK ((u32)0x1U << IRQMASKCLR3_TIMER_SHIFT)

/* define for only config & interrupt register */
#define VIOC_CONFIG_RESET 0x1U
#define VIOC_CONFIG_CLEAR 0x0U

//typedef enum {
//	WMIX00 = 0,
//	WMIX03,
//	WMIX10,
//	WMIX13,
//	WMIX30,
//	WMIX40,
//	WMIX50,
//	WMIX60,
//	WMIX_MAX
//} VIOC_CONFIG_WMIX_PATH;

typedef enum {
	FBCDEC0 = 0,
	FBCDEC1,
	FBCDEC_MAX
} VIOC_CONFIG_FBCDEC_PATH;

typedef enum {
	VIOC_CONFIG_DEV	= 0,
	VIOC_CONFIG_WMIXER,
	VIOC_CONFIG_WDMA,
	VIOC_CONFIG_RDMA,
	VIOC_CONFIG_SCALER,
	VIOC_CONFIG_VIQE,
	VIOC_CONFIG_DEINTS,
	VIOC_CONFIG_VIN,
	VIOC_CONFIG_MC,
	VIOC_CONFIG_FCENC,
	VIOC_CONFIG_FCDEC,
	VIOC_CONFIG_DTRC
} VIOC_SWRESET_Component;

typedef struct {
	unsigned int enable;
	unsigned int connect_statue;
	unsigned int connect_device;
} VIOC_PlugInOutCheck;

// Power Down
#define HwDDIC_PWDN_HDMI	Hw2
#define HwDDIC_PWDN_NTSC	Hw1	// NTSC/PAL
#define HwDDIC_PWDN_LCDC	Hw0

// Soft Reset
//#define HwDDIC_SWRESET_HDMI Hw2 // HDMI
//#define HwDDIC_SWRESET_NTSC Hw1 // NTSL/PAL
#define HwDDIC_SWRESET_ISP_AXI Hw2
#define HwDDIC_SWRESET_DP_AXI Hw1 // NTSC/PAL
#define HwDDIC_SWRESET_LCDC	Hw0

/* Interface APIs */
extern int VIOC_AUTOPWR_Enalbe(unsigned int component, unsigned int onoff);
extern int VIOC_CONFIG_PlugIn(
	unsigned int component,
	unsigned int select);
extern int VIOC_CONFIG_PlugOut(unsigned int component);
extern int VIOC_CONFIG_WMIXPath(
	unsigned int component_num,
	unsigned int wmixmode);
extern int VIOC_CONFIG_GetWMIXPath(unsigned int component_num);

#if defined(CONFIG_VIOC_PVRIC_FBDC)
extern int VIOC_CONFIG_FBCDECPath(
	unsigned int FBCDecPath,
	unsigned int rdmaPath,
	unsigned int on);
extern int VIOC_CONFIG_GetFBDCPath(unsigned int fbcdec_id);
#endif

extern int VIOC_CONFIG_MCPath(
	unsigned int component,
	unsigned int mc);
extern int VIOC_CONFIG_Device_PlugState(
	unsigned int component,
	VIOC_PlugInOutCheck *VIOC_PlugIn);
extern int VIOC_CONFIG_GetScaler_PluginToRDMA(
	unsigned int RdmaNum);
extern int VIOC_CONFIG_GetScaler_PluginToWDMA(
	unsigned int WdmaNum);

extern int VIOC_CONFIG_GetRdma_PluginToComponent(
	unsigned int ComponentNum);
extern void VIOC_CONFIG_StopRequest(unsigned int en);

extern int VIOC_CONFIG_LCDPath_Select(
	unsigned int lcdx_sel,
	unsigned int lcdx_if);
extern void __iomem *VIOC_IREQConfig_GetAddress(void);
extern void VIOC_IREQConfig_DUMP(
	unsigned int offset,
	unsigned int size);
extern int vioc_config_init(void);

/*
 * The WMIX is divided into the mix and the bypass path.
 * VIOC_CONFIG_SWReset() is only used to reset the mix path of WMIX.
 * VIOC_CONFIG_WMIXPathReset() is only used to reset the bypass path of WMIX.
 *
 * VIOC_CONFIG_SWReset(component, resetmode)
 *    - Reset the mix path and other VIOC components
 *    - PWR_BLK_SWR1 register
 *    - component: WMIX VIOC ID (ex. VIOC_WMIX0, VIOC_WMIX1 etc...)
 *    - resetmode: 1 is reset mode, 0 is normal mode
 *
 * VIOC_CONFIG_WMIXPathReset(component, resetmode)
 *    - Reset the bypass path
 *    - WMIX_PATH_SWR register
 *    - component: Input Layer VIOC ID of WMIX (ex. VIOC_RDMA03, VIOC_VIN10 etc...)
 *    - resetmode: 1 is reset mode, 0 is normal mode
 */
extern void VIOC_CONFIG_SWReset(unsigned int component, unsigned int resetmode);
extern void VIOC_CONFIG_WMIXPathReset(unsigned int component, unsigned int resetmode);

#endif
