/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_WDMA_IOCTRL_H
#define TCC_WDMA_IOCTRL_H

#define WDMA_IOC_MAGIC  'w'


#define TCC_WDMA_IOCTRL 0x9003
#define TCC_WDMA_START  0x9002
#define TCC_WDMA_END    0x9001


struct vioc_wdma_frame_info {
	unsigned int buff_addr;
	unsigned int buff_size;
	unsigned int frame_fmt;
	unsigned int frame_x;
	unsigned int frame_y;
	unsigned int buffer_num; //buffer number
};

struct vioc_wdma_get_buffer {
	unsigned int buff_Yaddr;
	unsigned int buff_Uaddr;
	unsigned int buff_Vaddr;
	unsigned int frame_fmt;
	unsigned int frame_x;
	unsigned int frame_y;
	int buff_index; //if index >0  is success
};


#define TC_WDRV_COUNT_START			0x8000
#define TC_WDRV_COUNT_GET_DATA		0x8001
#define TC_WDRV_GET_CUR_DATA		0x8011
#define TC_WDRV_COUNT_END			0x8002

enum WDMA_RESPONSE_TYPE {
	WDMA_POLLING,
	WDMA_INTERRUPT,
	WDMA_NOWAIT
};

#endif

