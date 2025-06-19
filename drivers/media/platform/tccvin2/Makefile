# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) Telechips Inc.
#
ifeq ($(CONFIG_ARCH_TCC807X),y)
	TCCVIN_ARCH_DIR = 807x
	TCCVIN_IP 	= vie_wrap
else ifeq ($(CONFIG_ARCH_TCC750X),y)
	TCCVIN_ARCH_DIR = 750x
	TCCVIN_IP 	= vie_wrap
else
	TCCVIN_ARCH_DIR = xxxx
	TCCVIN_IP 	= vioc_wrap
endif


ifeq ($(TCCVIN_IP),vie_wrap)
TCCVIN_DRIVER_OBJS	+= \
	model/$(TCCVIN_IP)/common/vin_wrap_vin.o	\
	model/$(TCCVIN_IP)/common/vin_wrap_wdma.o	\
	model/$(TCCVIN_IP)/$(TCCVIN_ARCH_DIR)/vin_wrap_intr.o	\
	model/$(TCCVIN_IP)/$(TCCVIN_ARCH_DIR)/vin_wrap_cfg.o	\
	model/$(TCCVIN_IP)/common/vin_wrap_video.o
else
TCCVIN_DRIVER_OBJS 	+= \
	model/$(TCCVIN_IP)/$(TCCVIN_ARCH_DIR)/tccxxxx_video.o
endif

TCCVIN_DRIVER_OBJS 	+= 	tccvin_subdev.o tccvin_v4l2.o tccvin_video.o tccvin_driver.o

tccvin-objs := $(TCCVIN_DRIVER_OBJS)

obj-$(CONFIG_VIDEO_TCCVIN2)	=  tccvin.o

