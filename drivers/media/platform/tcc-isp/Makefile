# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) Telechips Inc.
#

tcc-cap-isp-objs := \
	tcc-isp.o \
	func/tcc-isp-common.o

ifeq ($(CONFIG_ARCH_TCC805X), y)
	TCC-CAP-ISP-ARCH = 805x
endif
ifeq ($(CONFIG_ARCH_TCC750X), y)
	TCC-CAP-ISP-ARCH = 750x
endif
ifeq ($(CONFIG_ARCH_TCC807X), y)
	TCC-CAP-ISP-ARCH = 807x
endif

tcc-cap-isp-objs += \
	func/$(TCC-CAP-ISP-ARCH)/tcc-isp-core.o \
	func/$(TCC-CAP-ISP-ARCH)/tcc-isp-mcu.o

obj-$(CONFIG_TCC_ISP) += tcc-cap-isp.o
