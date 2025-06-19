# 
# Makefile for Telechips VDEC drivers.
#

obj-$(CONFIG_VIDEO_TELECHIPS_VPU_DECODER) += tcc-vpu-dec.o

tcc-vpu-dec-objs := tccvdec.o
tcc-vpu-dec-objs += tccvdec_ctrls.o tccvdec_core.o 
tcc-vpu-dec-objs +=  vpu_dec_v3.o vpu_drv_if.o tccvdecqueue.o 

#tcc-vpu-dec-objs += vpu_dec_ip_vpu.o vpu_dec_ip_common.o
ccflags-y += -I$(srctree)/include/video/tcc
ccflags-y += -I$(srctree)/include/video/telechips
