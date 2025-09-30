/* SPDX-License-Identifier: GPL-2.0-or-later OR MIT */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef DPTX_FB_HELPER_H
#define DPTX_FB_HELPER_H

struct dptx_fb_helper_dp_funcs {
	int (*get_main_state)(void);
	int (*get_dtd_from_vic)(uint32_t, uint32_t, struct dptx_dtd_params *);
	int (*set_video)(int, struct dptx_dtd_params *);
	int (*set_enable_video)(int, unsigned char);
};

int dptx_register_fb_dp_ops(struct dptx_fb_helper_dp_funcs **dp_ofs);
#endif /* DPTX_DRM_H  */

