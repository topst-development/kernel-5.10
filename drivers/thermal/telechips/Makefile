# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2023 Telechips Inc.

ifneq ($(filter y,$(CONFIG_ARCH_TCC805X) $(CONFIG_ARCH_TCN100X)),)
tcc_tsens_module-y			+= tem_v1.o tcc805x_thermal.o \
						thermal_common.o
obj-$(CONFIG_TELECHIPS_THERMAL)		+= tcc_tsens_module.o
else ifneq ($(filter y,$(CONFIG_ARCH_TCC750X)),)
tcc_tsens_module-y			+= tem_v1.o tcc750x_thermal.o \
						thermal_common.o
obj-$(CONFIG_TELECHIPS_THERMAL)		+= tcc_tsens_module.o
else ifneq ($(filter y,$(CONFIG_ARCH_TCC807X)),)
tcc_tsens_module-y			+= tem_v1.o tcc807x_thermal.o \
						thermal_common.o
obj-$(CONFIG_TELECHIPS_THERMAL)		+= tcc_tsens_module.o
else ifneq ($(filter y,$(CONFIG_ARCH_TCC803X) $(CONFIG_ARCH_TCC802X) $(CONFIG_ARCH_TCC897X)),)
tcc_tsens_module-y			+= tem1459.o tccxxxx_thermal.o \
						thermal_common.o
obj-$(CONFIG_TELECHIPS_THERMAL)		+= tcc_tsens_module.o
endif
#obj-$(CONFIG_TELECHIPS_GPU_THERMAL_SUPPORT)   += tcc805x_gpu_support.o
