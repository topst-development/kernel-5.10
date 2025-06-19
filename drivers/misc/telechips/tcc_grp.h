/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */
extern unsigned char gG2D_Dithering_en;

long g2d_drv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
int32_t g2d_drv_release(struct inode *p_inode, struct file *filp);
int32_t g2d_drv_open(struct inode *p_inode, struct file *filp);


