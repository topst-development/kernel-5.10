/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_SCRSHARE_H
#define TCC_SCRSHARE_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define TCC_SCRSHARE_SEND		0x1U
#define TCC_SCRSHARE_ACK		0x10U

struct tcc_scrshare_srcinfo {
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
};

struct tcc_scrshare_dstinfo {
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
	unsigned int img_num;
};

struct tcc_scrshare_info {
	struct tcc_scrshare_srcinfo *srcinfo;
	struct tcc_scrshare_dstinfo *dstinfo;

	unsigned int frm_w;
	unsigned int frm_h;
	unsigned int fmt;
	unsigned int src_addr;
	unsigned int share_enable;
	unsigned int rgb_swap;
};

/* control commands */
enum {
	SCRSHARE_CMD_NULL = 0,
	SCRSHARE_CMD_GET_DSTINFO,
	SCRSHARE_CMD_SET_SRCINFO,
	SCRSHARE_CMD_ON,
	SCRSHARE_CMD_OFF,
	SCRSHARE_CMD_READY,
	SCRSHARE_CMD_MAX,
};

/* driver status */
enum {
	SCRSHARE_STS_NULL = 0,
	SCRSHARE_STS_INIT,
	SCRSHARE_STS_READY,
	SCRSHARE_MAX_STS,
};

#define TCC_SCRSHARE_MAGIC 'S'
#define IOCTL_TCC_SCRSHARE_SET_DSTINFO		_IO(TCC_SCRSHARE_MAGIC, 1u)
#define IOCTL_TCC_SCRSHARE_GET_DSTINFO		_IO(TCC_SCRSHARE_MAGIC, 2u)
#define IOCTL_TCC_SCRSHARE_SET_SRCINFO		_IO(TCC_SCRSHARE_MAGIC, 3u)
#define IOCTL_TCC_SCRSHARE_ON			_IO(TCC_SCRSHARE_MAGIC, 4u)
#define IOCTL_TCC_SCRSHARE_OFF			_IO(TCC_SCRSHARE_MAGIC, 5u)

#endif
