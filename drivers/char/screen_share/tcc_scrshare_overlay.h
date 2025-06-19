// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_SCREEN_SHARED_OVERLAY_H
#define TCC_SCREEN_SHARED_OVERLAY_H


typedef struct {
	unsigned int src_addr;
	unsigned int src_x;
	unsigned int src_y;
	unsigned int src_w;
	unsigned int src_h;
	unsigned int dst_x;
	unsigned int dst_y;
	unsigned int dst_w;
	unsigned int dst_h;
	unsigned int frm_w;
	unsigned int frm_h;
	unsigned int fmt;
	unsigned int rgb_swap;
} shared_buffer_cfg_t;

struct tcc_scrshare_vioc {
	void __iomem *reg;
	unsigned int idx;
};

struct tcc_scrshare_overlay {
	struct tcc_scrshare_vioc rdma;
	struct tcc_scrshare_vioc wmix;
	unsigned int wmix_pos;
	unsigned int open_cnt;
};

extern int tcc_scrshare_overlay_display(
	const struct tcc_scrshare_overlay *overlay, shared_buffer_cfg_t cfg);
extern void tcc_scrshare_overlay_open(struct tcc_scrshare_overlay *overlay);
extern void tcc_scrshare_overlay_release(struct tcc_scrshare_overlay *overlay);
extern unsigned int tcc_scrshare_parse_overlay_node(
	struct tcc_scrshare_overlay *overlay, const struct platform_device *pdev);
#endif
/* end of file */
