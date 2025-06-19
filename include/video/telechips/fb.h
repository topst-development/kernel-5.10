// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_SIMPLE_VIOC_FB_H
#define TCC_SIMPLE_VIOC_FB_H

#include <linux/types.h>
#ifdef CONFIG_DMA_SHARED_BUFFER
#include <linux/dma-buf.h>
#define FBIOGET_DMABUF		_IOR('F', 0x21, struct fb_dmabuf_export)
struct fb_dmabuf_export {
	int fd;
	int flags;
};
int fb_get_dmabuf(struct fb_info *info, int flags);
#endif
#define FBIOSET_NO_PIXELALPHABLEND _IOW('F', 0x30, unsigned int)

#if defined(CONFIG_TELECHIPS_SIMPLE_FB_HDMI_SUPPORT)
//HDMI support
#define FBIO_HDMI_TURN_OFF _IO('F', 0x31)
#define FBIO_HDMI_TURN_ON _IO('F', 0x32)
#define FBIO_HDMI_GET_CONFIG _IOWR('F', 0x33, struct tsvfb_hdmi_config)
#define FBIO_HDMI_SET_CONFIG _IOWR('F', 0x34, struct tsvfb_hdmi_config)

struct tsvfb_hdmi_config {
    unsigned int size;
    struct videomode vm;
    unsigned int pxdw;
    unsigned int swapbf;
    bool r2y;
    unsigned int r2ymd;
};
#endif

#endif
