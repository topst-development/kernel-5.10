/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_DRM_DP_HELPER_HEADER
#define TCC_DRM_DP_HELPER_HEADER

#if defined(CONFIG_TELECHIPS_DP_DRIVER_V1_4) || defined(CONFIG_TELECHIPS_DP_DRIVER_V1_4_MODULE)
#include <include/dptx_video.h>
#include <include/dptx_drm.h>

/* coverity[misra_c_2012_rule_5_7] */
struct tcc_drm_dp_callback_funcs {
	int (*attach)(struct drm_encoder *encoder, int dp_id, int flags);
	int (*detach)(struct drm_encoder *encoder, int dp_id, int flags);
	int (*register_helper_funcs)(
		struct drm_encoder *encoder,
		struct dptx_drm_helper_funcs *dptx_ops);
};
#endif
#endif
