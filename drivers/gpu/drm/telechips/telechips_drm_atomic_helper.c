// SPDX-License-Identifier: GPL-2.0-or-later

/* tcc_drm_crtc_plane_helper.c
 *
 * Copyright (C) 2022 Telechips Inc.
 * Authors:
 *	Jayden Kim
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#if defined(CONFIG_REFCODE_PRE_K510)
#include <drm/drmP.h>
#endif
#include <drm/drm_plane_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_encoder.h>
#include <drm/drm_vblank.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_print.h>
#include <drm/drm_bridge.h>

#include <telechips_drm_types.h>

static bool tccdrm_crtc_needs_disable(const struct drm_crtc_state *old_state,
		   const struct drm_crtc_state *new_state)
{
	bool ret = false;
	/*
	 * No new_state means the crtc is off, so the only criteria is whether
	 * it's currently active or in self refresh mode.
	 */
	if (new_state == NULL) {
		ret =  drm_atomic_crtc_effectively_active(old_state);
	} else {
		/*
		 * We need to run through the crtc_funcs->disable() function if the crtc
		 * is currently on, if it's transitioning to self refresh mode, or if
		 * it's in self refresh mode and needs to be fully disabled.
		 */
		ret = old_state->active ||
		      (old_state->self_refresh_active && !new_state->enable) ||
		      new_state->self_refresh_active;
	}

	return ret;
}

static void tccdrm_disable_encoders_stage(unsigned stage,
					  struct drm_encoder *encoder,
					  struct drm_bridge *bridge,
					  struct drm_atomic_state *old_state,
					  const struct tcc_crtc_state *tcc_cstate,
					  const struct drm_encoder_helper_funcs *funcs)
{
	switch (stage) {
	case 1:
		/*
		* Each encoder has at most one connector (since we always steal
		* it away), so we won't call disable hooks twice.
		*/
		#if defined(CONFIG_REFCODE_PRE_K510)
		drm_atomic_bridge_disable(encoder->bridge, old_state);
		#else
		bridge = drm_bridge_chain_get_first_bridge(encoder);
		drm_atomic_bridge_chain_disable(bridge, old_state);
		#endif
		/* Right function depends upon target state. */
		if (funcs != NULL) {
			if (funcs->atomic_disable != NULL) {
				funcs->atomic_disable(encoder, old_state);
			}
		}
		#if defined(CONFIG_REFCODE_PRE_K510)
		drm_atomic_bridge_post_disable(encoder->bridge, old_state);
		#else
		drm_atomic_bridge_chain_post_disable(bridge, old_state);
		#endif
		break;
	case 2:
		if (tcc_cstate->connector_type ==
		    DRM_MODE_CONNECTOR_DisplayPort) {
			/* Right function depends upon target state. */
			if (funcs != NULL) {
				if (funcs->disable != NULL) {
					funcs->disable(encoder);
				}
			}
		}
		break;
	default:
		(void)pr_info("%s: Unknown stage[%d]\n", __func__, stage);
#if 0
		DRM_DEV_DEBUG(dev->dev,
			"%s Unknown stage[%d]\n",
			__func__, stage);
#endif
		break;
	}
}

static int tccdrm_disable_encoders(const struct drm_device *dev,
				    struct drm_atomic_state *old_state,
				    unsigned int stage)
{
	const struct tcc_crtc_state *tcc_cstate;
	const struct drm_connector *connector;
	const struct drm_connector_state *old_conn_state, *new_conn_state;
	const struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	int i;

	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_9] */
	/* coverity[misra_c_2012_rule_13_4] */
	/* coverity[misra_c_2012_rule_13_5] */
	/* coverity[misra_c_2012_rule_13_6] */
	/* coverity[misra_c_2012_rule_15_6] */
	for_each_oldnew_connector_in_state(old_state, connector, old_conn_state, new_conn_state, i) {
		const struct drm_encoder_helper_funcs *funcs;
		struct drm_encoder *encoder;
		struct drm_bridge *bridge = NULL;

		/* Shut down everything that's in the changeset and currently
		 * still on. So need to check the old, saved state. */
		if (old_conn_state->crtc == NULL) {
			continue;
		}

		old_crtc_state = drm_atomic_get_old_crtc_state(old_state, old_conn_state->crtc);

		/* coverity[cert_dcl37_c] */
		/* coverity[cert_arr39_c] */
		/* coverity[cert_exp40_c] */
		/* coverity[misra_c_2012_rule_8_13] */
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_11_5] */
		/* coverity[misra_c_2012_rule_11_8] */
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_15_6] */
		/* coverity[misra_c_2012_rule_18_4] */
		/* coverity[misra_c_2012_rule_21_2] */
		tcc_cstate = to_tcc_crtc_state(old_crtc_state);

		if (new_conn_state->crtc != NULL) {
			new_crtc_state = drm_atomic_get_new_crtc_state(
						old_state,
						new_conn_state->crtc);
		} else {
			new_crtc_state = NULL;
		}

		if (!tccdrm_crtc_needs_disable(old_crtc_state, new_crtc_state) ||
		    !drm_atomic_crtc_needs_modeset(old_conn_state->crtc->state)) {
			continue;
		}

		encoder = old_conn_state->best_encoder;

		/* We shouldn't get this far if we didn't previously have
		 * an encoder.. but WARN_ON() rather than explode.
		 */
		if (encoder == NULL) {
			continue;
		}

		funcs = encoder->helper_private;

		DRM_DEV_DEBUG(dev->dev,
			      "%s disabling [ENCODER:%d:%s]\n",
			      __func__, encoder->base.id, encoder->name);

		tccdrm_disable_encoders_stage(stage, encoder, bridge, old_state, tcc_cstate, funcs);
	}
	return 0;
}

static void tccdrm_disable_outputs(const struct drm_device *dev, struct drm_atomic_state *old_state)
{
	struct drm_crtc_state *old_crtc_state;
	const struct drm_crtc_state *new_crtc_state;
	struct drm_crtc *crtc;
	int i;

	/* dieable encoders */
	(void)tccdrm_disable_encoders(dev, old_state, 1);

	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_9] */
	/* coverity[misra_c_2012_rule_13_4] */
	/* coverity[misra_c_2012_rule_13_5] */
	/* coverity[misra_c_2012_rule_15_6] */
	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state, new_crtc_state, i) {
		const struct drm_crtc_helper_funcs *funcs;
		int ret;

		/* Shut down everything that needs a full modeset. */
		if (!drm_atomic_crtc_needs_modeset(new_crtc_state)) {
			continue;
		}

		if (!tccdrm_crtc_needs_disable(old_crtc_state, new_crtc_state)) {
			continue;
		}

		funcs = crtc->helper_private;

		DRM_DEV_DEBUG(dev->dev,
			      "%s disabling [CRTC:%d:%s]\n",
			      __func__, crtc->base.id, crtc->name);


		/* Right function depends upon target state. */
		if (funcs->atomic_disable != NULL) {
			funcs->atomic_disable(crtc, old_crtc_state);
		}
		#if defined(CONFIG_REFCODE_PRE_K510)
		if (!(dev->irq_enabled && dev->num_crtcs)) {
			continue;
		}
		#else
		if (!drm_dev_has_vblank(dev)) {
			continue;
		}
		#endif
		ret = drm_crtc_vblank_get(crtc);
		if (ret == 0) {
			drm_crtc_vblank_put(crtc);
		}
	}

	(void)tccdrm_disable_encoders(dev, old_state, 2);
}

static void tccdrm_crtc_set_mode(const struct drm_device *dev, const struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	const struct drm_connector *connector;
	struct drm_connector_state *new_conn_state;
	int i;

	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_9] */
	/* coverity[misra_c_2012_rule_13_4] */
	/* coverity[misra_c_2012_rule_13_5] */
	/* coverity[misra_c_2012_rule_15_6] */
	for_each_new_crtc_in_state(old_state, crtc, new_crtc_state, i) {
		const struct drm_crtc_helper_funcs *funcs;

		if (!drm_atomic_crtc_needs_modeset(new_crtc_state)) {
			continue;
		}

		if (!new_crtc_state->active || !new_crtc_state->enable) {
			continue;
		}

		funcs = crtc->helper_private;

		if (funcs->mode_set_nofb != NULL) {
			funcs->mode_set_nofb(crtc);
		}
	}

	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_9] */
	/* coverity[misra_c_2012_rule_13_4] */
	/* coverity[misra_c_2012_rule_13_5] */
	/* coverity[misra_c_2012_rule_15_6] */
	for_each_new_connector_in_state(old_state, connector, new_conn_state, i) {
		const struct drm_encoder_helper_funcs *funcs;
		struct drm_encoder *encoder;
		struct drm_display_mode *disp_mode, *adjusted_mode;
		#if defined(CONFIG_REFCODE_PRE_K510)
		#else
		struct drm_bridge *bridge;
		#endif

		if (new_conn_state->best_encoder == NULL) {
			continue;
		}

		encoder = new_conn_state->best_encoder;
		funcs = encoder->helper_private;
		new_crtc_state = new_conn_state->crtc->state;
		disp_mode = &new_crtc_state->mode;
		adjusted_mode = &new_crtc_state->adjusted_mode;

		if (!drm_atomic_crtc_needs_modeset(new_crtc_state)) {
			continue;
		}

		if (!new_crtc_state->active || !new_crtc_state->enable) {
			continue;
		}

		/*
		 * Each encoder has at most one connector (since we always steal
		 * it away), so we won't call mode_set hooks twice.
		 */
		if(funcs != NULL) {
			if (funcs->atomic_mode_set != NULL) {
				funcs->atomic_mode_set(encoder, new_crtc_state,
							   new_conn_state);
			} else if (funcs->mode_set != NULL) {
				funcs->mode_set(encoder, disp_mode, adjusted_mode);
			} else {
				DRM_DEV_INFO(dev->dev,
					"%s: mode_set func is NULL pointer\r\n", __func__);
			}
		}

		#if defined(CONFIG_REFCODE_PRE_K510)
		drm_bridge_mode_set(encoder->bridge, disp_mode, adjusted_mode);
		#else
		bridge = drm_bridge_chain_get_first_bridge(encoder);
		drm_bridge_chain_mode_set(bridge, disp_mode, adjusted_mode);
		#endif
	}
}

/**
 * tccdrm_drm_atomic_helper_commit_modeset_disables - modeset commit to disable
 * outputs
 * @dev: DRM device
 * @old_state: atomic state object with old state structures
 *
 * This function shuts down all the outputs that need to be shut down and
 * prepares them (if required) with the new mode.
 *
 * For compatibility with legacy CRTC helpers this should be called before
 * drm_atomic_helper_commit_planes(), which is what the default commit function
 * does. But drivers with different needs can group the modeset commits together
 * and do the plane commits at the end. This is useful for drivers doing runtime
 * PM since planes updates then only happen when the CRTC is actually enabled.
 */
static void tccdrm_drm_atomic_helper_commit_modeset_disables(struct drm_device *dev,
					       struct drm_atomic_state *old_state)
{
	tccdrm_disable_outputs(dev, old_state);

	drm_atomic_helper_update_legacy_modeset_state(dev, old_state);
	drm_atomic_helper_calc_timestamping_constants(old_state);

	tccdrm_crtc_set_mode(dev, old_state);
}

static unsigned int wfb_wait_vblank_phase1(const struct drm_atomic_state *old_state)
{
	const struct drm_crtc_state *old_crtc_state;
	const struct drm_crtc_state *new_crtc_state;
	unsigned int crtc_mask = 0U;
	struct drm_crtc *crtc;
	int i, ret;

	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_9] */
	/* coverity[misra_c_2012_rule_12_1] */
	/* coverity[misra_c_2012_rule_13_4] */
	/* coverity[misra_c_2012_rule_13_5] */
	/* coverity[misra_c_2012_rule_14_2] */
	/* coverity[misra_c_2012_rule_15_6] */
	for_each_oldnew_crtc_in_state(old_state, crtc, old_crtc_state,
				new_crtc_state, i) {
		#if defined(CONFIG_REFCODE_PRE_K54)
		if (!new_crtc_state->active ||
			!new_crtc_state->planes_changed) {
		#else
		if (!new_crtc_state->active) {
		#endif
			continue;
		}
		ret = drm_crtc_vblank_get(crtc);
		if (ret != 0) {
			continue;
		}
		crtc_mask |= drm_crtc_mask(crtc);
		old_state->crtcs[i].last_vblank_count =
			drm_crtc_vblank_count(crtc);
	}
	return crtc_mask;
}

/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 2>
 * HIS metric violation (HIS_GOTO)
 *  caused by wait_event_interruptible_timeout
 */
static void wfb_wait_vblank_phase2(const struct drm_device *dev,
			    const struct drm_atomic_state *old_state,
			    unsigned int crtc_mask,
			    unsigned long wait_ms)
{
	const struct drm_crtc_state *old_crtc_state;
	//const struct drm_crtc_state *new_crtc_state;
	struct drm_crtc *crtc;
	int i, ret;

	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_9] */
	/* coverity[misra_c_2012_rule_12_1] */
	/* coverity[misra_c_2012_rule_13_4] */
	/* coverity[misra_c_2012_rule_13_5] */
	/* coverity[misra_c_2012_rule_14_2] */
	/* coverity[misra_c_2012_rule_15_6] */
	for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		if ((crtc_mask & drm_crtc_mask(crtc)) ==0u) {
			continue;
		}
		/* coverity[cert_int02_c] */
		/* coverity[cert_int31_c] */
		/* coverity[cert_dcl37_c] */
		/* coverity[cert_pre31_c] */
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_10_3] */
		/* coverity[misra_c_2012_rule_12_1] */
		/* coverity[misra_c_2012_rule_14_3] */
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_15_4] */
		/* coverity[misra_c_2012_rule_15_1] */
		/* coverity[misra_c_2012_rule_15_6] */
		/* coverity[misra_c_2012_rule_20_7] */
		ret = wait_event_timeout(dev->vblank[i].queue,
					 (old_state->crtcs[i].last_vblank_count != drm_crtc_vblank_count(crtc)),
					 wait_ms);
		if (ret == 0) {
			DRM_DEV_INFO(dev->dev,
					"[CRTC:%d:%s] vblank\r\n",
					crtc->base.id,
					crtc->name);
		}
		drm_crtc_vblank_put(crtc);
	}
}

/* HIS metric violation (HIS_GOTO) caused by wait_event_interruptible_timeout */
static void tccdrm_wait_for_vblanks(const struct drm_device *dev,
		const struct drm_atomic_state *old_state)
{
	bool internal_ok = (bool)true;
	unsigned int crtc_mask = 0U;
	unsigned long wait_ms = msecs_to_jiffies(100U);

	/*
	 * Legacy cursor ioctls are completely unsynced, and userspace
	 * relies on that (by doing tons of cursor updates).
	 */
	if (old_state->legacy_cursor_update) {
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		crtc_mask = wfb_wait_vblank_phase1(old_state);
		wfb_wait_vblank_phase2(dev, old_state, crtc_mask, wait_ms);
	}
}

/**
 * tccdrm_commit_tail - commit atomic update to hardware
 * @old_state: atomic state object with old state structures
 *
 * This is the default implementation for the
 * &drm_mode_config_helper_funcs.atomic_commit_tail hook, for drivers
 * that do not support runtime_pm or do not need the CRTC to be
 * enabled to perform a commit. Otherwise, see
 * drm_atomic_helper_commit_tail_rpm().
 *
 * Note that the default ordering of how the various stages are called is to
 * match the legacy modeset helper library closest.
 */
/* coverity[misra_c_2012_rule_8_4] */
void tccdrm_commit_tail(struct drm_atomic_state *old_state)
{
	struct drm_device *dev = old_state->dev;

	tccdrm_drm_atomic_helper_commit_modeset_disables(dev, old_state);

	drm_atomic_helper_commit_planes(dev, old_state, 0);

	drm_atomic_helper_commit_modeset_enables(dev, old_state);

	#if defined(CONFIG_REFCODE_PRE_K54)
	#else
	drm_atomic_helper_fake_vblank(old_state);
	#endif

	drm_atomic_helper_commit_hw_done(old_state);

	tccdrm_wait_for_vblanks((const struct drm_device *)dev,
				(const struct drm_atomic_state *)old_state);

	drm_atomic_helper_cleanup_planes(dev, old_state);
}
/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_8_3] */
/* coverity[misra_c_2012_rule_8_5] */
/* coverity[misra_c_2012_rule_8_6] */
/* coverity[misra_c_2012_rule_8_11] */
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
EXPORT_SYMBOL(tccdrm_commit_tail);
