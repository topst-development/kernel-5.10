# SPDX-License-Identifier: GPL-2.0
#
# Copyright 2020-2022 Telechips Inc.
#

ifeq ($(CONFIG_ARCH_TCC750X), y)
	TCC-IDI2AXI-ARCH = 750x
endif

tcc-cap-idi2axi-objs += \
	tcc-idi2axi.o \
	$(TCC-IDI2AXI-ARCH)/tcc-idi2axi-dev.o

obj-$(CONFIG_VIDEO_TCC_IDI2AXI) += tcc-cap-idi2axi.o