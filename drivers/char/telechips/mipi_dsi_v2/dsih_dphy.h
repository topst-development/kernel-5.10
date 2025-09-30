/* SPDX-License-Identifier: GPL-2.0 */
/**
 * Copyright (C) 2018 Synopsys, Inc.
 *
 * @file dsih_dphy.h
 * @brief Synopsys MIPI D-PHY functions
 * included as a part of Synopsys MIPI DSI Host controller driver
 *
 * @author Luis Oliveira <luis.oliveira@synopsys.com>
 * Modified by Telechips
 */

#ifndef MIPI_DSIH_DPHY_H_
#define MIPI_DSIH_DPHY_H_

#include "dsih_includes.h"

#define CAMB_PERF_PHY0_ADDR         0x1C014000
#define CAMB_PERF_PHY1_ADDR         0x1C024000

#define DSI2_DPHY_8_BITS 0
#define DSI2_DPHY_16_BITS 1
#define DSI2_DPHY_32_BITS 2

#define BIAS_CON0_OFFSET    0x0
#define BIAS_CON1_OFFSET    0x4
#define BIAS_CON2_OFFSET    0x8
#define BIAS_CON3_OFFSET    0xC
#define BIAS_CON4_OFFSET    0x10

#define PLL_CON0_OFFSET     0x100
#define PLL_CON1_OFFSET     0x104
#define PLL_CON2_OFFSET     0x108
#define PLL_CON3_OFFSET     0x10C
#define PLL_CON4_OFFSET     0x110
#define PLL_CON5_OFFSET     0x114
#define PLL_CON6_OFFSET     0x118
#define PLL_CON7_OFFSET     0x11C
#define PLL_CON8_OFFSET     0x120

#define MASTER0_CLK_GNR_CON0_OFFSET       0x300
#define MASTER0_CLK_GNR_CON1_OFFSET       0x304
#define MASTER0_CLK_ANA_CON0_OFFSET       0x308
#define MASTER0_CLK_ANA_CON1_OFFSET       0x30C
#define MASTER0_CLK_TIME_CON0_OFFSET       0x330
#define MASTER0_CLK_TIME_CON1_OFFSET       0x334
#define MASTER0_CLK_TIME_CON2_OFFSET       0x338
#define MASTER0_CLK_TIME_CON3_OFFSET       0x33C
#define MASTER0_CLK_TIME_CON4_OFFSET       0x340
#define MASTER0_CLK_DESKEW_OFFSET       0x350

#define MASTER0_DATA0_GNR_CON0_OFFSET     0x400
#define MASTER0_DATA0_GNR_CON1_OFFSET     0x404
#define MASTER0_DATA0_ANA_CON0_OFFSET     0x408
#define MASTER0_DATA0_ANA_CON1_OFFSET     0x40C
#define MASTER0_DATA0_ANA_CON2_OFFSET     0x410
#define MASTER0_DATA0_ANA_CON3_OFFSET     0x414
#define MASTER0_DATA0_TIME_CON0_OFFSET     0x430
#define MASTER0_DATA0_TIME_CON1_OFFSET     0x434
#define MASTER0_DATA0_TIME_CON2_OFFSET     0x438
#define MASTER0_DATA0_TIME_CON3_OFFSET     0x43C
#define MASTER0_DATA0_TIME_CON4_OFFSET     0x440
#define MASTER0_DATA0_CON0_OFFSET           0x444

#define MASTER0_DATA1_OFFSET     0x500
#define MASTER0_DATA2_OFFSET     0x600
#define MASTER0_DATA3_OFFSET     0x700

#define PLL_EN_SHIFT             12
#define PLL_S_SHIFT             8
#define PLL_P_SHIFT              0
#define PLL_M_SHIFT              0

#define PLL_K_SHIFT              0
#define PLL_FOUT_MASK_SHIFT     13
#define FEED_EN_SHIFT           12

#define MRR_SHIFT       8
#define MFR_SHIFT       0
#define RSEL_SHIFT      12
#define SSCG_EN_SHIFT       11
#define FSEL_SHIFT      10
#define BYPASS_SHIFT        9
#define AFC_ENB_SHIFT       8
#define EXTAFC_SHIFT        0
#define RESET_N_SEL_SHIFT   10
#define PLL_EN_SEL_SHIFT    8
#define ICP_SHIFT       4
#define SEL_PF_SHIFT    0
#define WCKJ_BUF_SFT_CNT_SHIFT  8

/* master clock lane timing */
#define HSTX_CLK_SEL_SHIFT   12
#define T_LPX_SHIFT          4
#define T_CLK_ZERO_SHIFT     8
#define T_CLK_PREPARE_SHIFT    0
#define T_HS_EXIT_SHIFT         8
#define T_CLK_TRAIL_SHIFT       0
#define T_CLK_POST_SHIFT        0
#define T_ULPS_EXIT_SHIFT       0

#define SKEW_CAL_RUN_TIME_SHIFT             12
#define SKEW_CAL_INIT_RUN_TIME_SHIFT        8
#define SKEW_CAL_INIT_WAIT_TIME_SHIFT       4
#define SKEW_CAL_EN_SHIFT                   0

/* master data lane timing */
#define DATA_HSTX_CLK_SEL_SHIFT         12
#define DATA_T_LPX_SHIFT                4
#define DATA_T_LP_EXIT_SKEW_SHIFT       2
#define DATA_T_LP_ENTRY_SKEW_SHIFT      0
#define DATA_T_HS_ZERO_SHIFT            8
#define DATA_T_HS_PREPARE_SHIFT         0
#define DATA_T_HS_EXIT_SHIFT            8
#define DATA_T_HS_TRAIL_SHIFT           0
#define DATA_T_TA_GET_SHIFT             4
#define DATA_T_TA_GO_SHIFT              0
#define DATA_T_ULPS_EXIT_SHIFT          0


void 	mipi_dsih_dphy_write(struct mipi_dsi_dev *dev, unsigned int address,
                             unsigned int * data, unsigned int data_length);

void 	mipi_dsih_dphy_write_word(struct mipi_dsi_dev *dev,
                                  unsigned int reg_address, unsigned int data);

void 	mipi_dsih_dphy_write_part(struct mipi_dsi_dev *dev,
                                  unsigned int reg_address, unsigned int data,
                                  unsigned int shift, unsigned int width);

unsigned int	 mipi_dsih_dphy_read_word(struct mipi_dsi_dev *dev,
                                      unsigned int reg_address);

unsigned int 	mipi_dsih_dphy_read_part(struct mipi_dsi_dev *dev,
                                     unsigned int reg_address,
                                     unsigned int shift,
                                     unsigned int width);

void tcc_dsi_phy_init(struct mipi_dsi_dev *dev);
#endif	/* MIPI_DSIH_DPHY_H_ */
