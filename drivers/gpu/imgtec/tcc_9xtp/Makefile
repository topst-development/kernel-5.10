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

obj-$(CONFIG_POWERVR_TCC9XTP) += tcc_9xtp.o

tcc_9xtp-y += tcc_9xtp_init.o \
	    sysconfig.o \
	    ../pvrsrvkm/$(PVR_ARCH)/dma_support.o \
	    ../pvrsrvkm/interrupt_support.o

ccflags-y += -I$(srctree)/$(src) \
             -I$(srctree)/drivers/gpu/imgtec \
             -I$(srctree)/drivers/gpu/imgtec/pvrsrvkm \
             -I$(srctree)/drivers/gpu/imgtec/pvrsrvkm/$(PVR_ARCH) \
             -I$(srctree)/drivers/gpu/imgtec/pvrsrvkm/$(PVR_ARCH)/hwdefs \
             -I$(srctree)/drivers/gpu/imgtec/pvrsrvkm/$(PVR_ARCH)/hwdefs/km
tmp := $(addprefix -I,$(wildcard $(srctree)/drivers/gpu/imgtec/pvrsrvkm/$(PVR_ARCH)/generated/*))
$(warning $(tmp))
ccflags-y += $(tmp)
