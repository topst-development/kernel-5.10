obj-$(CONFIG_POWERVR_ADF_FBDEV) += powervr_adf_fbdev.o
powervr_adf_fbdev-y += adf_common.o adf_fbdev.o
ifeq ($(CONFIG_ANDROID),y)
    ifeq ($(CONFIG_ARM64),y)
        include $(srctree)/drivers/gpu/imgtec/config_kernel_android64.mk
        ccflags-y += -include $(srctree)/drivers/gpu/imgtec/config_kernel_android64.h
    endif
else
    ifneq ($(CONFIG_POWERVR_DC_FBDEV),)
        ifeq ($(CONFIG_ARM64),y)
            include $(srctree)/drivers/gpu/imgtec/config_kernel_linux64_fb.mk
            ccflags-y += -include $(srctree)/drivers/gpu/imgtec/config_kernel_linux64_fb.h
		endif
    else
        ifeq ($(CONFIG_ARM64),y)
            include $(srctree)/drivers/gpu/imgtec/config_kernel_linux64_drm.mk
            ccflags-y += -include $(srctree)/drivers/gpu/imgtec/config_kernel_linux64_drm.h
		endif
    endif
endif

ccflags-y += \
	-I$(srctree)/$(src) \
	-I$(srctree)/drivers/staging/android

obj-y += pvrsrvkm/
#obj-$(CONFIG_POWERVR_TC:m=y) += tc/
#obj-$(CONFIG_POWERVR_RK3368) += rk3368/
#obj-$(CONFIG_POWERVR_MT8173) += mt8173/
