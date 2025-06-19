/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2023 Telechips Inc.
 */

#ifndef F_ISP_H		/* 016.11.14 */
#define F_ISP_H

extern void isp_cleanup(void);
extern int isp_bind_config(struct usb_configuration *c);
unsigned char data_buf[8];

#endif /* F_ISP_H */
