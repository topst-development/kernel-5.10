# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) Telechips Inc.
#

ccflags-y = -I$(src)/.. -Idrivers/video/fbdev/telechips/

#
# module name is tcc_v4l2_vout.ko
#
obj-$(CONFIG_VIDEO_TELECHIPS_VOUT) += tcc_v4l2_vout.o

tcc_v4l2_vout-$(CONFIG_VIDEO_TELECHIPS_VOUT) += tcc_vout_v4l2.o
tcc_v4l2_vout-$(CONFIG_VIDEO_TELECHIPS_VOUT) += tcc_vout_core.o
tcc_v4l2_vout-$(CONFIG_VIDEO_TELECHIPS_VOUT) += tcc_vout_attr.o
tcc_v4l2_vout-$(CONFIG_VIDEO_TELECHIPS_VOUT) += tcc_vout_dbg.o

tcc_v4l2_vout-$(CONFIG_TELECHIPS_VOUT_BOOTING_ANIMATION) += tcc_vout_booting_animation.o