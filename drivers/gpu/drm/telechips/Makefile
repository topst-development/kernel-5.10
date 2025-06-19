#
# Makefile for the drm device driver.  This driver provides support for the
# Direct Rendering Infrastructure (DRI) in XFree86 4.1.0 and higher.
ccflags-y += -I$(srctree)/drivers/gpu/drm/telechips
ifneq ($(filter y m,$(CONFIG_TELECHIPS_DP_DRIVER_V1_4)),)
ccflags-y += -I$(srctree)/drivers/char/displayport
endif
ifneq ($(filter y m,$(CONFIG_TELECHIPS_SCREEN_SHARE)),)
ccflags-y += -I$(srctree)/drivers/char/screen_share
ccflags-y += -I$(srctree)/drivers/clk/telechips
endif
ifeq ($(CONFIG_TELECHIPS_DPTX_AUDIO),y)
ccflags-y += -I$(srctree)/sound/soc/telechips/dptx_audio
endif
ifneq ($(filter y m,$(CONFIG_TELECHIPS_DSI_DRIVER_V2)),)
ccflags-y += -I$(srctree)/drivers/char/telechips/mipi_dsi_v2
endif

ifneq ($(filter y m,$(CONFIG_DRM_TELECHIPS)),)
telechips_drm_common-y := telechips_drm_edid.o telechips_drm_crtc_plane_helper.o telechips_drm_fb.o \
                   telechips_drm_atomic_helper.o
telechips_drm-y := telechips_drm_drv.o telechips_drm_gem.o
endif


telechips_drm-$(CONFIG_DRM_FBDEV_EMULATION) += telechips_drm_fbdev.o
ifneq ($(filter y,$(CONFIG_DRM_TELECHIPS_LCD) $(CONFIG_DRM_TELECHIPS_EXT)\
       $(CONFIG_DRM_TELECHIPS_THIRD) $(CONFIG_DRM_TELECHIPS_FOURTH)),)
obj-$(CONFIG_DRM_TELECHIPS_VIOC) += telechips_drm_vioc.o
endif
obj-$(CONFIG_DRM_TELECHIPS_SCREEN_SHARE) += telechips_drm_screen_share.o
obj-$(CONFIG_DRM_TELECHIPS_DUMMY_DEV) += telechips_drm_dummy_dev.o
obj-$(CONFIG_DRM_TELECHIPS_LVDS) += telechips_drm_lvds.o
obj-$(CONFIG_DRM_TELECHIPS_DSI) += telechips_drm_dsi.o
obj-$(CONFIG_DRM_TELECHIPS_DP) += telechips_drm_dp.o
obj-$(CONFIG_DRM_TELECHIPS) += telechips_drm.o  telechips_drm_common.o
