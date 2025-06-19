/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * telechips npu driver
 *
 * Copyright (C) 2020 Telechips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef TELECHIPS_NPU_REG_H
#define TELECHIPS_NPU_REG_H

#define IADDR_DMA                   		(0x000U)
#define IADDR_MM                    		(0x100U)
#define IADDR_DW                    		(0x200U)
#define IADDR_MISC                  		(0x300U)
#define IADDR_MLX                   		(0x400U)

#define ADDR_NPU_CONTROL            		(0x000U)
#define ADDR_NPU_STATUS             		(0x004U)
#define ADDR_NPU_APB_COMMAND        		(0x008U)
#define ADDR_NPU_ID_CODE            		(0x00CU)

#define ADDR_NPU_IRQ_REASON         		(0x010U)
#define ADDR_NPU_IRQ_ENABLE         		(0x014U)
#define ADDR_NPU_IRQ_MASK           		(0x018U)
#define ADDR_NPU_IRQ_CLEAR          		(0x01CU)

#define ADDR_NPU_COLOR_CONV_0       		(0x020U)
#define ADDR_NPU_COLOR_CONV_1       		(0x024U)
#define ADDR_NPU_COLOR_CONV_2       		(0x028U)
#define ADDR_NPU_COLOR_CONV_BIAS    		(0x02CU)

#define ADDR_NPU_READ_INT_REG       		(0x030U)
#define ADDR_NPU_INT_REG_RDATA      		(0x034U)
#define ADDR_NPU_CMD_CNT            		(0x038U)

#define ADDR_NPU_BASE_ADDR0         		(0x040U)
#define ADDR_NPU_BASE_ADDR1         		(0x044U)
#define ADDR_NPU_BASE_ADDR2         		(0x048U)
#define ADDR_NPU_BASE_ADDR3         		(0x04CU)

#define ADDR_NPU_BASE_ADDR4         		(0x050U)
#define ADDR_NPU_BASE_ADDR5         		(0x054U)
#define ADDR_NPU_BASE_ADDR6         		(0x058U)
#define ADDR_NPU_BASE_ADDR7         		(0x05CU)

#define ADDR_NPU_PERF_DMA           		(0x060U)
#define ADDR_NPU_PERF_COMP          		(0x064U)
#define ADDR_NPU_PERF_ALL           		(0x068U)
#define ADDR_NPU_PERF_AXI_CONF      		(0x070U)

#define ADDR_NPU_INT_RST_CTRL       		(0x074U)
#define ADDR_NPU_CG_CTRL            		(0x078U)

#define ADDR_NPU_PERF_CNT_MM        		(0x080U)
#define ADDR_NPU_PERF_CNT_DW        		(0x084U)
#define ADDR_NPU_PERF_CNT_MISC      		(0x088U)
#define ADDR_NPU_PERF_CNT_MLX       		(0x08CU)

#define ADDR_NPU_ECC_CTRL                       (0x0C0U)
#define ADDR_NPU_ECC_TEST_CTRL                  (0x0C4U)
#define ADDR_NPU_ECC_RESERVED_3                 (0x0C8U)
#define ADDR_NPU_ECC_CBUF_ECC_CNT               (0x0CCU)

#define ADDR_NPU_ECC_GBUF_UE_CNT_C0             (0x0D0U)
#define ADDR_NPU_ECC_GBUF_UE_CNT_C1             (0x0D4U)
#define ADDR_NPU_ECC_GBUF_UE_CNT_C2             (0x0D8U)
#define ADDR_NPU_ECC_GBUF_UE_CNT_C3             (0x0DCU)

#define ADDR_NPU_ECC_GBUF_CE_CNT_C0             (0x0E0U)
#define ADDR_NPU_ECC_GBUF_CE_CNT_C1             (0x0E4U)
#define ADDR_NPU_ECC_GBUF_CE_CNT_C2             (0x0E8U)
#define ADDR_NPU_ECC_GBUF_CE_CNT_C3             (0x0ECU)

#define ADDR_NPU_ECC_CBUF_ADDR                  (0x0F0U)

#define ADDR_NPU_ECC_GBUF_UE_ADDR0_C0           (0x100U)
#define ADDR_NPU_ECC_GBUF_UE_ADDR1_C0           (0x104U)
#define ADDR_NPU_ECC_GBUF_UE_ADDR0_C1           (0x108U)
#define ADDR_NPU_ECC_GBUF_UE_ADDR1_C1           (0x10CU)
#define ADDR_NPU_ECC_GBUF_UE_ADDR0_C2           (0x110U)
#define ADDR_NPU_ECC_GBUF_UE_ADDR1_C2           (0x114U)
#define ADDR_NPU_ECC_GBUF_UE_ADDR0_C3           (0x118U)
#define ADDR_NPU_ECC_GBUF_UE_ADDR1_C3           (0x11CU)
#define ADDR_NPU_ECC_GBUF_CE_ADDR0_C0           (0x120U)
#define ADDR_NPU_ECC_GBUF_CE_ADDR1_C0           (0x124U)
#define ADDR_NPU_ECC_GBUF_CE_ADDR0_C1           (0x128U)
#define ADDR_NPU_ECC_GBUF_CE_ADDR1_C1           (0x12CU)
#define ADDR_NPU_ECC_GBUF_CE_ADDR0_C2           (0x130U)
#define ADDR_NPU_ECC_GBUF_CE_ADDR1_C2           (0x134U)
#define ADDR_NPU_ECC_GBUF_CE_ADDR0_C3           (0x138U)
#define ADDR_NPU_ECC_GBUF_CE_ADDR1_C3           (0x13CU)

// MLX_CORE_0
// Core0: 0x200 ~ 0x23C
// Core1: 0x240 ~ 0x27C
// Core2: 0x280 ~ 0x2BC
// Core2: 0x2C0 ~ 0x2FC
#define ADDR_NPU_MLX_C0_HCI_00                  (0x200U)
#define ADDR_NPU_MLX_C0_HCI_04                  (0x204U)
#define ADDR_NPU_MLX_C0_HCI_08                  (0x208U)
#define ADDR_NPU_MLX_C0_HCI_0C                  (0x20CU)
#define ADDR_NPU_MLX_C0_HCI_10                  (0x210U)
#define ADDR_NPU_MLX_C0_HCI_14                  (0x214U)
#define ADDR_NPU_MLX_C0_HCI_18                  (0x218U)
#define ADDR_NPU_MLX_C0_HCI_1C                  (0x21CU)

#define ADDR_NPU_MLX_C0_ECC_CTRL                (0x220U)
#define ADDR_NPU_MLX_C0_ECC_CNT                 (0x224U)
#define ADDR_NPU_MLX_CO_ECC_CE_ADDR             (0x228U)
#define ADDR_NPU_MLX_CO_ECC_CE_DATA             (0x22CU)
#define ADDR_NPU_MLX_CO_ECC_UE_ADDR             (0x230U)
#define ADDR_NPU_MLX_CO_ECC_UE_DATA             (0x234U)
#define ADDR_NPU_MLX_C0_ECC_MASK_DAT            (0x238U)
#define ADDR_NPU_MLX_C0_ECC_MASK_PAR            (0x23CU)

#endif //TELECHIPS_NPU_REG_H
