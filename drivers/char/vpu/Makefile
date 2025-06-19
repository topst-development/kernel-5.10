

obj-$(CONFIG_TELECHIPS_VPU_DRV) += vpu.o

vpu-y += jpu_mgr.o
vpu-y += jpu_mgr_flexio.o
vpu-y += jpu_mgr_sys.o

obj-$(CONFIG_TELECHIPS_VPU_DRV) += jpu_dev.o
jpu_dev-y += jpu_devices.o

ifdef CONFIG_SUPPORT_TELECHIPS_G2V2_VP9
vpu-y += vp9_mgr.o
vpu-y += vp9_mgr_sys.o

obj-$(CONFIG_TELECHIPS_VPU_DRV) += vp9_dev.o
vp9_dev-y += vp9_devices.o
endif

vpu-y += hevc_mgr.o
vpu-y += hevc_mgr_flexio.o
vpu-y += hevc_mgr_sys.o

obj-$(CONFIG_TELECHIPS_VPU_DRV) += hevc_dev.o
hevc_dev-y += hevc_devices.o

vpu-y += vpu_4k_d2_mgr.o
vpu-y += vpu_4k_d2_mgr_flexio.o
vpu-y += vpu_4k_d2_mgr_sys.o

obj-$(CONFIG_TELECHIPS_VPU_DRV) += vpu_4k_d2_dev.o
vpu_4k_d2_dev-y += vpu_4k_d2_devices.o

vpu-y += vpu_hevc_enc_mgr.o
vpu-y += vpu_hevc_enc_mgr_sys.o

obj-$(CONFIG_TELECHIPS_VPU_DRV) += vpu_hevc_enc_dev.o
vpu_hevc_enc_dev-y += vpu_hevc_enc_devices.o

vpu-y += vpu_hevc_enc2_mgr.o
vpu-y += vpu_hevc_enc2_mgr_sys.o

obj-$(CONFIG_TELECHIPS_VPU_DRV) += vpu_hevc_enc2_dev.o
vpu_hevc_enc2_dev-y += vpu_hevc_enc2_devices.o

ifdef CONFIG_ARCH_TCC899X
vpu-y += smp_driver.o	###__FXXX__
endif

ifdef CONFIG_ARCH_TCC901X
vpu-y += smp_driver.o	###__FXXX__
endif

vpu-y += vpu_mgr.o vpu_mgr_flexio.o vpu_mem.o vpu_etc.o vpu_dbg.o vpu_rm.o
vpu-y += vpu_mgr_sys.o
vpu-y += vpu_devices.o
vpu-y += vpu_dec.o vpu_dec_flexio.o
vpu-y += vpu_enc.o
