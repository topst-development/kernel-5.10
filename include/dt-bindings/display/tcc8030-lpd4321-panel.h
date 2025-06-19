// SPDX-License-Identifier: (GPL-2.0-or-later OR MIT)
/*
 * Copyright (C) 2023 Telechips Inc.
 */

#ifndef DT_BIND_TCC8030_LPD4321_DISP_H
#define DT_BIND_TCC8030_LPD4321_DISP_H

/* Definition for H/W LCD ports */
/* PORT1 for dual LVDS */
#define LCD_PORT1_PWR "gpb-17"
#define LCD_PORT1_RST "gpb-18"
#define LCD_PORT1_BLK "gpma-26"

/* PORT2 for HDMI */
#define LCD_PORT2_PWR "gpc-8"
#define LCD_PORT2_RST "gpc-9"
#define LCD_PORT2_BLK "gpma-27"

/* PORT3 for single LVDS (external I/C, not implemented)
#define LCD_PORT3_PWR ""
#define LCD_PORT3_RST ""
#define LCD_PORT3_BLK ""
*/

#endif
