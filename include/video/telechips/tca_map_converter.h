/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCA_MAP_CONVERTER_H
#define TCA_MAP_CONVERTER_H

#include <video/telechips/vioc_mc.h>
#include <video/telechips/tccfb_ioctrl.h>

extern void tca_map_convter_set(unsigned int component_num,
				const struct tcc_lcdc_image_update *ImageInfo,
				int y2r);
extern void tca_map_convter_driver_set(
	unsigned int component_num, unsigned int Fwidth,
	unsigned int Fheight, unsigned int pos_x,
	unsigned int pos_y, unsigned int Cwidth,
	unsigned int Cheight, unsigned int y2r,
	const hevc_MapConv_info_t *mapConv_info);
extern void tca_map_convter_onoff(
	unsigned int component_num, unsigned int onoff, unsigned int wait_done);
extern void tca_map_convter_swreset(unsigned int component_num);
extern void tca_map_convter_wait_done(unsigned int component_num);

#endif
