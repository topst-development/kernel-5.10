# SPDX-License-Identifier: GPL-2.0
#
# Copyright 2020-2022 Telechips Inc.
#

TCC_SVDW_MODULE_NAME := tcc-svdw

ifeq ($(CONFIG_ARCH_TCC807X),y)
	TCC_SVDW_ARCH_DIR = 807x
endif
ifeq ($(CONFIG_ARCH_TCC750X),y)
	TCC_SVDW_ARCH_DIR = 750x
endif

$(TCC_SVDW_MODULE_NAME)-objs :=	\
	$(TCC_SVDW_MODULE_NAME).o \
	tcc-svdw-v4l2.o	\
	tcc-svdw-video.o \
	$(TCC_SVDW_ARCH_DIR)/tcc-svdw-stitch.o

obj-$(CONFIG_VIDEO_TCC_SVDW) +=	\
	$(TCC_SVDW_MODULE_NAME).o
