# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) Telechips Inc.
#
ccflags-y += -I$(srctree)/drivers/char/displayport 	\
	     -I$(srctree)/drivers/char/displayport/include

ifneq ($(filter y m,$(CONFIG_DRM_TELECHIPS)),)
ccflags-y += -I$(srctree)/drivers/gpu/drm/telechips
else
ifeq ($(CONFIG_TCC_DPTX_AUDIO),y)
ccflags-y += -I$(srctree)/sound/soc/tcc/dptx_audio
endif
endif

ifeq ($(CONFIG_DRM_PANEL_MAX968XX),y)
ccflags-y += -I$(srctree)/drivers/gpu/drm/panel
endif

obj-$(CONFIG_TELECHIPS_DP_DRIVER_V1_4) += telechips_dp_v14.o

ifneq ($(filter y m,$(CONFIG_TELECHIPS_DP_DRIVER_V1_4)),)
telechips_dp_v14-y += dptx_api.o dptx_v14.o dptx_reg.o dptx_aux.o dptx_vidin.o 	\
		      dptx_core.o dptx_intr.o dptx_link.o dptx_edid.o 		\
		      dptx_extension.o dptx_protect.o dptx_clk.o dptx_cfg.o 	\
		      dptx_sec_phy.o dptx_proc.o dptx_drm_dp_addition.o
endif
ifneq ($(filter y m,$(CONFIG_MAX968XX_DP_SERDES)),)
telechips_dp_v14-y += dptx_serdes.o
endif
ifneq ($(filter y m,$(CONFIG_DPTX_V14_AUDIO)),)
telechips_dp_v14-y += dptx_audin.o
endif


