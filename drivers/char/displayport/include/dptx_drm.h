/* SPDX-License-Identifier: GPL-2.0-or-later OR MIT */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef DPTX_DRM_H
#define DPTX_DRM_H
struct dptx_drm_helper_funcs {
	int (*get_hpd_state)(int dp_id, unsigned char *hpd_state);
	int (*get_edid)(int dp_id, unsigned char *edid_buf, unsigned int buf_length);
	int (*set_video)(
			int dp_id,
			const struct dptx_detailed_timing_t *dptx_detailed_timing);
	int (*set_enable_video)(int dp_id, unsigned char enable);
	int (*set_enable_audio)(int dp_id, unsigned char enable);
};

struct tcc_drm_dp_callback_funcs;

int tcc_dp_register_drm(struct drm_encoder *encoder, const struct tcc_drm_dp_callback_funcs *callbacks);
int tcc_dp_unregister_drm(const struct drm_encoder *encoder);
int32_t tcc_dp_identify_lcd_mux_configuration(uint32_t dp_id, uint8_t lcd_mux_index);
#endif /* DPTX_DRM_H  */

