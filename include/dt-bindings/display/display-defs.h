// SPDX-License-Identifier: (GPL-2.0-or-later OR MIT)
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef DT_BIND_DISPLAY_DEFS_H
#define DT_BIND_DISPLAY_DEFS_H

/*** COMMON DEFINES **********************************/
#define DEF_TO_STR(pinctrl) #pinctrl
#define MIX_GPIO_NUM_TO_STR(pinctrl) DEF_TO_STR(pinctrl)
#define PANEL_GPIO_TO_PINCTRL(gpio, num) MIX_GPIO_NUM_TO_STR(gpio-num)

/*** DRM DRIVERS *************************************/
/* Dual LDI LVDS */
#define DT_DISPDEV_LVDS0 	0x10

/* Single LDI0 LVDS */
#define DT_DISPDEV_LVDS1	0x11

/* Single LDI1 LVDS */
#define DT_DISPDEV_LVDS2	0x12

#define DT_DISPDEV_HDMI		0x20

#define DT_DISPDEV_DP0		0x30
#define DT_DISPDEV_DP1		0x31
#define DT_DISPDEV_DP2		0x32
#define DT_DISPDEV_DP3		0x33

#define DT_DISPDEV_DUMMY0	0x40
#define DT_DISPDEV_DUMMY1	0x41
#define DT_DISPDEV_DUMMY2	0x42
#define DT_DISPDEV_DUMMY3	0x43
#define DT_DISPDEV_DUMMY4	0x44

#define DT_DISPDEV_DSI0		0x50
#define DT_DISPDEV_DSI1		0x51

/*** FB DRIVERS **************************************/
/* FB Dual LDI LVDS */
#define DT_DISPDEV_FB_LVDS0 	0x10000

/* Single LDI0 LVDS */
#define DT_DISPDEV_FB_LVDS1	0x10011

/* Single LDI1 LVDS */
#define DT_DISPDEV_FB_LVDS2	0x10012

#define DT_DISPDEV_FB_DSI0	0x10050
#define DT_DISPDEV_FB_DSI1	0x10051

#define DT_DISPDEV_FB_DUMMY0	0x10040
#define DT_DISPDEV_FB_DUMMY1	0x10041
#define DT_DISPDEV_FB_DUMMY2	0x10042
#define DT_DISPDEV_FB_DUMMY3	0x10043
#define DT_DISPDEV_FB_DUMMY4	0x10044

#define DT_DISPDEV_IS_FB_DEV(x) ((x) & DT_DISPDEV_FB_LVDS0)

#define DT_DISPDEV_NONE		0x00000

#define LVDS_PANEL_BOE		0
#define LVDS_PANEL_AUO		1

#endif
