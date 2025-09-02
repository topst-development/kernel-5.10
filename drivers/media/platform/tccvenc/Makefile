# 
# Makefile for Telechips VENC drivers.
#

obj-$(CONFIG_VIDEO_TELECHIPS_VPU_ENCODER) += tcc-vpu-enc.o

tcc-vpu-enc-objs := tccvenc.o

tcc-vpu-enc-objs += tccvenc_fmt.o tccvenc_queue.o tccvenc_ctrls.o

tcc-vpu-enc-objs += vpu_enc_v3.o

ccflags-y += -I$(srctree)/include/video/tcc
ccflags-y += -I$(srctree)/include/video/telechips
