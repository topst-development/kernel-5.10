# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) Telechips Inc.

ifneq (,$(filter y, $(CONFIG_TELECHIPS_FB_LVDS_PANEL)\
		    $(CONFIG_TELECHIPS_FB_DSI_PANEL)\
		    $(CONFIG_TELECHIPS_FB_DPTXV14_PANEL)))
EXTRA_CFLAGS += -I$(srctree)/drivers/video/fbdev/telechips/panel
obj-y += panel_helper.o
endif

ifneq (,$(filter y,$(CONFIG_TELECHIPS_FB_LVDS_PANEL)))
obj-y += panel-lvds.o
endif

ifneq (,$(filter y,$(CONFIG_TELECHIPS_FB_DSI_PANEL)))
ccflags-y += -I$(srctree)/drivers/char/telechips/mipi_dsi_v2
obj-y += panel-dsi.o
endif

ifneq (,$(filter y,$(CONFIG_TELECHIPS_FB_DPTXV14_PANEL)))
ccflags-y += -I$(srctree)/drivers/char/displayport
obj-y += panel-dpv14.o
endif

