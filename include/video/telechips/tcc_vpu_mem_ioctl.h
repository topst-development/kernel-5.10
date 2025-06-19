/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef VPU_MEM_IOCTL_H
#define VPU_MEM_IOCTL_H

#define TCC_LIMIT_PRESENTATION_WINDOW       0x10
#define TCC_DRAM_16BIT_USED                 0x11
#define TCC_VIDEO_SET_DISPLAYING_IDX        0x12
#define TCC_VIDEO_GET_DISPLAYING_IDX        0x13
#define TCC_VIDEO_SET_DISPLAYED_BUFFER_ID   0x14
#define TCC_VIDEO_GET_DISPLAYED_BUFFER_ID   0x15
#define TCC_VIDEO_CREATE_DMA_BUF            0x16
#define TCC_VIDEO_RELEASE_DMA_BUF           0x17
#define TCC_VIDEO_CREATE_DMA_BUF_2          0x18
#define TCC_8925S_IS_XX_CHIP                0x20

//for DolbyVision
#define TCC_GET_HDMI_INFO                   0x30
#define TCC_SET_HDMI_OUT_TYPE               0x31

#ifdef CONFIG_ANDROID
#define USE_UMP_RESERVED_SW_PMAP
#endif
#ifdef USE_UMP_RESERVED_SW_PMAP
#define TCC_REGISTER_UMP_SW_INFO            0x40
#define TCC_REGISTER_UMP_SW_INFO_KERNEL     0x41
#define TCC_DEREGISTER_UMP_SW_INFO          0x42
#define TCC_DEREGISTER_UMP_SW_INFO_KERNEL   0x43
#define TCC_AUTOFREE_DEREGISTER_UMP_SW_INFO_KERNEL 0x44
#endif

typedef struct {
	unsigned int istance_index;
	int index;
} vbuffer_manager;

typedef struct {
	unsigned int dv_path;
	unsigned int out_type;  // DOVI = 0, HDR10 = 1, SDR = 2
	unsigned int width;
	unsigned int height;
	unsigned int dv_vsvdb[12];
	unsigned int dv_vsvdb_size;
	unsigned int dv_ll_mode;
} vHdmi_info;

typedef struct {
	unsigned int phy_addr;
	unsigned int width;
	unsigned int height;
	unsigned int size;
} stIonBuff_info;

typedef struct{
	unsigned long phys;        // physical address
	unsigned long size;        // allocation size
	int fd;
} stVpuPhysInfo;

extern int set_displaying_index(unsigned long arg);
extern int get_displaying_index(int nInst);
extern int set_buff_id(unsigned long arg);
extern int get_buff_id(int nInst);

#endif //VPU_MEM_IOCTL_H