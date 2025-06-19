# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) Telechips Inc.

ifneq (,$(filter y, $(CONFIG_TELECHIPS_FB_LVDS_PANEL) $(CONFIG_TELECHIPS_FB_DSI_PANEL)))
obj-y += panel_helper.o
endif

ifneq (,$(filter y,$(CONFIG_TELECHIPS_FB_LVDS_PANEL)))
EXTRA_CFLAGS += -I$(srctree)/drivers/video/fbdev/telechips/panel
obj-y += panel-lvds.o
endif

ifneq (,$(filter y,$(CONFIG_TELECHIPS_FB_DSI_PANEL)))
EXTRA_CFLAGS += -I$(srctree)/drivers/video/fbdev/telechips/panel
ccflags-y += -I$(srctree)/drivers/char/telechips/mipi_dsi_v2
obj-y += panel-dsi.o
endif
