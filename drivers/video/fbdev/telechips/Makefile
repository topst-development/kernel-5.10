# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) Telechips Inc.
#
ifeq ($(CONFIG_ARCH_TCC807X),y)
obj-y += vioc_tcc807x/
else
obj-y += vioc/
endif

obj-$(CONFIG_TELECHIPS_SIMPLE_FB) += panel/ fb.o

obj-$(CONFIG_VIDEO_DISPLAY_DEINTERLACE_MODE) += viqe/

obj-$(CONFIG_TELECHIPS_OVERLAY) += tcc_overlay.o
