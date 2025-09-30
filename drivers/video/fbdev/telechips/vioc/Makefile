# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) Telechips Inc.
#

ifneq (,$(filter y,$(CONFIG_ARCH_TCC805X)))
ccflags-y += -I$(srctree)/drivers/clk/telechips
endif

#
# module name is vioc_api.ko
#
obj-$(CONFIG_TELECHIPS_VIOC_LOW_API) += vioc_api.o

vioc_api-$(CONFIG_TELECHIPS_VIOC_LOW_API) += vioc.o
vioc_api-$(CONFIG_TELECHIPS_VIOC_LOW_API) += vioc_ddicfg.o
vioc_api-$(CONFIG_TELECHIPS_VIOC_LOW_API) += vioc_config.o
vioc_api-$(CONFIG_TELECHIPS_VIOC_LOW_API) += vioc_intr.o

vioc_api-$(CONFIG_TELECHIPS_VIOC_LOW_API) += vioc_rdma.o
vioc_api-$(CONFIG_TELECHIPS_VIOC_LOW_API) += vioc_wdma.o
vioc_api-$(CONFIG_TELECHIPS_VIOC_LOW_API) += vioc_wmix.o
vioc_api-$(CONFIG_TELECHIPS_VIOC_LOW_API) += vioc_disp.o
vioc_api-$(CONFIG_TELECHIPS_VIOC_LOW_API) += vioc_scaler.o
vioc_api-$(CONFIG_TELECHIPS_VIOC_LOW_API) += vioc_lut.o
vioc_api-$(CONFIG_TELECHIPS_VIOC_LOW_API) += vioc_fifo.o
vioc_api-$(CONFIG_TELECHIPS_VIOC_LOW_API) += vioc_timer.o
ifneq ($(CONFIG_ARCH_TCC750X),y)
vioc_api-$(CONFIG_TELECHIPS_VIOC_LOW_API) += vioc_vin.o
endif

vioc_api-$(CONFIG_VIOC_DEINTERLACER) += vioc_viqe.o vioc_deintls.o
vioc_api-$(CONFIG_VIOC_G2D) += tca_gre2d_api.o tcc_gre2d.o
vioc_api-$(CONFIG_VIOC_OUTPUT_CONFIG) += vioc_outcfg.o

vioc_api-$(CONFIG_VIOC_MAP_DECOMP) += vioc_mc.o tca_map_converter.o
vioc_api-$(CONFIG_VIOC_AFBCDEC) += vioc_afbcdec.o
vioc_api-$(CONFIG_VIOC_PVRIC_FBDC) += vioc_pvric_fbdc.o

vioc_api-$(CONFIG_ARCH_TCC897X) += vioc_lvds_t.o
vioc_api-$(CONFIG_ARCH_TCC803X) += vioc_pxdemux.o vioc_lvds.o
vioc_api-$(CONFIG_ARCH_TCC805X) += vioc_pxdemux.o vioc_lvds.o vioc_lut_3d.o
