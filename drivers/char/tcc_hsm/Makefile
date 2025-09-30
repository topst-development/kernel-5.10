# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2023 Telechips Inc.

ifeq ($(CONFIG_ARCH_TCC803X),y)
obj-$(CONFIG_TELECHIPS_HSM) += M4/tcc_hsm.o M4/tcc_hsm_sp_cmd.o
else ifneq ($(filter y,$(CONFIG_ARCH_TCC805X) $(CONFIG_ARCH_TCC807X)),)
obj-$(CONFIG_TELECHIPS_HSM) += SC000/tcc_hsm.o SC000/tcc_hsm_cmd.o
else ifeq ($(CONFIG_ARCH_TCC750X),y)
obj-$(CONFIG_TELECHIPS_HSM) += SC000/tcc_hsm.o SC000/tcc_hsm_cmd.o
else

endif
