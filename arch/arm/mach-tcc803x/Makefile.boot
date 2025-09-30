# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2023 Telechips Inc.

ifeq ($(CONFIG_TCC803X_CA7S),y)
zreladdr-y	+= 0x80008000
params_phys-y	+= 0x80000100
initrd_phys-y	+= 0x80800000
endif
