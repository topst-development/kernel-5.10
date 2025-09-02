// SPDX-License-Identifier: (GPL-2.0-or-later OR MIT)
/*
 * Copyright (C) 2023 Telechips Inc.
 */

#ifndef DT_BIND_TCC8059_LPD4X322_DISP_H
#define DT_BIND_TCC8059_LPD4X322_DISP_H

/* Definition for H/W LCD ports */
#define LCD_PORT1_PWR "gpb-17"
#define LCD_PORT1_RST "gpb-18"
#define LCD_PORT1_BLK_GPIO gpma
#define LCD_PORT1_BLK_NUM  26
#define LCD_PORT1_BLK PANEL_GPIO_TO_PINCTRL(LCD_PORT1_BLK_GPIO, LCD_PORT1_BLK_NUM)

#define LCD_PORT2_PWR "gpc-8"
#define LCD_PORT2_RST "gpc-9"
#define LCD_PORT2_BLK_GPIO gpma
#define LCD_PORT2_BLK_NUM  27
#define LCD_PORT2_BLK PANEL_GPIO_TO_PINCTRL(LCD_PORT2_BLK_GPIO, LCD_PORT2_BLK_NUM)

#define DP_HPD_GPIO "gpc-14"
#define DP_SERDES_INTB "gpe-19"

#endif
