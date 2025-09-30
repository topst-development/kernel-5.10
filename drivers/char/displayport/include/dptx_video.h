/* SPDX-License-Identifier: GPL-2.0-or-later OR MIT */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef DPTX_VIDEO_PARAM_H
#define DPTX_VIDEO_PARAM_H

/* display detailed timing */
struct dptx_detailed_timing_t {
	unsigned char interlaced;
	unsigned char pixel_repetition_input;
	unsigned int  h_active;
	unsigned int  h_blanking;
	unsigned int  h_sync_offset;
	unsigned int  h_sync_pulse_width;
	unsigned int  h_sync_polarity;
	unsigned int  v_active;
	unsigned int  v_blanking;
	unsigned int  v_sync_offset;
	unsigned int  v_sync_pulse_width;
	unsigned int  v_sync_polarity;
	unsigned int  pixel_clock;
};

/* DP DTD data */
struct dptx_dtd_params {
	uint8_t interlaced;
	uint8_t h_sync_polarity;
	uint8_t	v_sync_polarity;
	uint16_t pixel_repetition_input;
	uint16_t h_active;
	uint16_t h_blanking;
	uint16_t h_image_size;
	uint16_t h_sync_offset;
	uint16_t h_sync_pulse_width;
	uint16_t v_active;
	uint16_t v_blanking;
	uint16_t v_image_size;
	uint16_t v_sync_offset;
	uint16_t v_sync_pulse_width;
	uint32_t uiPixel_Clock;
};

#endif
