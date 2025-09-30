# SPDX-License-Identifier: GPL-2.0
#
# Copyright 2020-2022 Telechips Inc.
#

ifeq ($(CONFIG_ARCH_TCC807X),y)
	TCC_DEWARP_ARCH_DIR = 807x
endif
ifeq ($(CONFIG_ARCH_TCC750X),y)
	TCC_DEWARP_ARCH_DIR = 750x
endif

obj-$(CONFIG_VIDEO_TCC_ODW) +=	\
	tcc-dewarp-driver.o	\
	tcc-dewarp-v4l2.o	\
	tcc-dewarp-subdev.o	\
	tcc-dewarp-video.o	\
	$(TCC_DEWARP_ARCH_DIR)/tcc-dewarp-odw.o
