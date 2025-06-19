// SPDX-License-Identifier: (GPL-2.0-or-later OR MIT)
/*
 * Copyright (C) 2023 Telechips Inc.
 */

#ifndef DT_BIND_TCC8070_EVB_SV0_1_H
#define DT_BIND_TCC8070_EVB_SV0_1_H

/* Definition for H/W LCD ports */
#define LCD_PORT1_PWR "gpa-15"
#define LCD_PORT1_RST "gpa-16"
#define LCD_PORT1_BLK "gpma-4"

#define LCD_PORT2_PWR "gpg-6"
#define LCD_PORT2_RST "gpg-7"
#define LCD_PORT2_BLK "gpma-5"

#define LCD_PORT3_PWR "gpb-14"
#define LCD_PORT3_RST "gpb-15"
#define LCD_PORT3_BLK "gpma-16"

#define LCD_PORT4_PWR "gpa-31"
#define LCD_PORT4_RST "gpb-16"
#define LCD_PORT4_BLK "gpma-17"

#define LCD_PORT5_PWR "gpmb-15"
#define LCD_PORT5_RST "gpmb-17"
#define LCD_PORT5_BLK "gpma-29"

// DSI
#define LCD_PORT_SER_PWDN "gpmb-10"

#define DP_HPD_GPIO "gpc-14"
#define DP_SERDES_INTB "gpa-18"
#define DP_SERDES_LOCK "gpa-19"

#endif
