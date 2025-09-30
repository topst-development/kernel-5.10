// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Telechips DRM DP Driver
 *
 * Copyright (C) 2022 Telechips Inc.
 *
 * Authors:
 *	Jayden Kim
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#if defined(CONFIG_REFCODE_PRE_K510)
#include <drm/drmP.h>
#endif
#include <drm/drm_crtc_helper.h>
#include <drm/drm_panel.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_print.h>


#include <linux/of_graph.h>
#include <linux/regulator/consumer.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <video/of_videomode.h>
#include <video/of_display_timing.h>
#include <video/videomode.h>
#include <video/telechips/vioc_global.h>
#include <video/telechips/tcc_math.h>

#include <telechips_drm_types.h>
#include <telechips_drm_drv.h>
#include <telechips_drm_edid.h>
#include <telechips_drm_dp.h>
#include <telechips_drm_dp_helper.h>

#if defined(CONFIG_TELECHIPS_DPTX_AUDIO) || defined(CONFIG_TELECHIPS_DPTX_AUDIO_MODULE)
#include <linux/of_platform.h>
#include <linux/kernel.h>
#include <sound/hdmi-codec.h>
#include <tcc_dptx_api.h>
#endif

#define DRIVER_DATE "20240910"
#define DRIVER_MAJOR 2
#define DRIVER_MINOR 3
#define DRIVER_PATCH 6

#define CONFIG_DRM_TELECHIPS_SUPPORT_REAL_HPD

/* property */
enum tcc_prop_audio_freq {
	PROP_36_HZ = 0,
	PROP_44_1HZ,
	PROP_48HZ,
	PROP_88_2HZ,
	PROP_96HZ,
	PROP_192HZ,
};

static const struct drm_prop_enum_list tcc_prop_audio_freq_names[] = {
	{ (int)PROP_36_HZ, "36hz" },
	{ (int)PROP_44_1HZ, "44.1hz" },
	{ (int)PROP_48HZ, "48hz" },
	{ (int)PROP_88_2HZ, "88.2hz" },
	{ (int)PROP_96HZ, "96hz" },
	{ (int)PROP_192HZ, "192hz" },
};

enum tcc_prop_audio_type {
	PROP_TYPE_PCM = 0,
	PROP_TYPE_DD,
	PROP_TYPE_DDP,
	PROP_TYPE_DTS,
	PROP_TYPE_DTS_HD,
};

static const struct drm_prop_enum_list tcc_prop_audio_type_names[] = {
	{ (int)PROP_TYPE_PCM, "pcm" },
	{ (int)PROP_TYPE_DD, "dd" },
	{ (int)PROP_TYPE_DDP, "ddp" },
	{ (int)PROP_TYPE_DTS, "dts" },
	{ (int)PROP_TYPE_DTS_HD, "dts-hd" },
};

struct tccdrm_dptx_context {
	int dp_id;
	struct dptx_drm_helper_funcs *funcs;
#if defined(CONFIG_TELECHIPS_DPTX_AUDIO) || defined(CONFIG_TELECHIPS_DPTX_AUDIO_MODULE)
	struct tcc_dptx_audio_ops *aud_ops;
	struct platform_device *aud_dev;
	struct tcc_audio_params staudio_params;
#endif
};

struct tcc_dp_prop {
	struct drm_property *audio_freq;
	struct drm_property *audio_type;
};

struct tcc_dp_prop_data {
	enum tcc_prop_audio_freq audio_freq;
	enum tcc_prop_audio_type audio_type;
};

struct tccdrm_dp_context {
	struct drm_encoder encoder;
	struct drm_connector connector;
	struct device *dev;

	struct drm_panel *panel;
	struct display_timings *timings;

	#if defined(CONFIG_DRM_TELECHIPS_DP_PROC)
	struct proc_dir_entry *proc_dir;
	struct proc_dir_entry *proc_hpd;
	struct proc_dir_entry *proc_edid;
	enum drm_connector_status manual_hpd;
	#endif

	struct tccdrm_dptx_context *dp;
	struct tcc_dp_prop dp_prop;
	struct tcc_dp_prop_data dp_prop_data;

	bool binded;
};


#define connector_to_context(x) container_of((x), struct tccdrm_dp_context, connector)
#define encoder_to_context(x) container_of((x), struct tccdrm_dp_context, encoder)


#if defined(CONFIG_DRM_TELECHIPS_SUPPORT_REAL_HPD)
static int tccdrm_dp_get_hpd_state(struct tccdrm_dp_context *dev_context)
{
	unsigned char hpd_state = 0;
	bool internal_ok = (bool)true;
	int ret = 0;

	if (dev_context->dp == NULL) {
		internal_ok = (bool)false;
	}
	if (internal_ok && (dev_context->dp->funcs == NULL)) {
		internal_ok = (bool)false;
	}
	if (internal_ok && (dev_context->dp->funcs->get_hpd_state == NULL)) {
		internal_ok = (bool)false;
	}
	if (internal_ok &&
		(dev_context->dp->funcs->get_hpd_state(dev_context->dp->dp_id,
		&hpd_state) < 0)) {
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		if (hpd_state != 0) {
			ret = 1;
		}
	}

	return ret;
}
#endif

#if defined(CONFIG_TELECHIPS_DPTX_AUDIO) || defined(CONFIG_TELECHIPS_DPTX_AUDIO_MODULE)
static int tcc_dpv14_aud_get_ops(struct tccdrm_dp_context *dev_context)
{
	struct device *dev = dev_context->dev;
	struct device_node *dp_np;
	struct platform_device *ppdev;

	if (dev_context->dp == NULL) {
		DRM_DEV_ERROR(dev, "dp context is NULL\n");

		goto out;
	}

	dp_np = of_find_compatible_node(NULL, NULL, "telechips,dptx_audio");
	if (dp_np == NULL) {
		DRM_DEV_ERROR(dev, "can't get dp audio compatiable node\n");

		goto out;
	}

	ppdev = of_find_device_by_node(dp_np);
	if (ppdev == NULL) {
		DRM_DEV_ERROR(dev, "can't get dp audio device\n");

		goto out;
	}

	dev_context->dp->aud_ops = (struct tcc_dptx_audio_ops *)ppdev->dev.platform_data;

	DRM_DEV_INFO(dev, "DP %d: got dptx audio ops\r\n", dev_context->dp->dp_id);

out:
	return 0;
}

static int tcc_dpv14_aud_hw_params(struct device *dev,
											void *data,
											struct hdmi_codec_daifmt *daifmt,
											struct hdmi_codec_params *params)
{
	int ret = 0;
	struct tccdrm_dptx_context *dp_context;
	struct tccdrm_dp_context *dev_context;

	(void)dev;

	dev_context = (struct tccdrm_dp_context *)data;
	dp_context = dev_context->dp;

	if (dev_context->dp == NULL) {
		DRM_DEV_ERROR(dev, "dp context is NULL\n");

		goto out;
	}

	if (daifmt->fmt != HDMI_I2S) {
		DRM_DEV_ERROR(dev, "Invalid audio format %d\n", daifmt->fmt);

		goto out;
	}

	dp_context->staudio_params.sample_rate = params->sample_rate;
	dp_context->staudio_params.data_width = 16;//params->sample_width;
	dp_context->staudio_params.channels = params->channels;

	ret = dp_context->aud_ops->set_audio_params(dp_context->dp_id, &dp_context->staudio_params);
	if (ret != 0) {
		DRM_DEV_ERROR(dev, "\n[ERR:%s]DP %d: failed to set codec params\r\n", __func__, dev_context->dp->dp_id);

		goto out;
	}

	ret = dp_context->aud_ops->enable_audio(dp_context->dp_id);
	if (ret != 0) {
		DRM_DEV_ERROR(dev, "\n[ERR:%s]DP %d: failed to set enable\r\n", __func__, dev_context->dp->dp_id);

		goto out;
	}

	DRM_DEV_INFO(dev, "DP %d\r\n", dp_context->dp_id);
	DRM_DEV_INFO(dev, "Format %d\r\n", daifmt->fmt);
	DRM_DEV_INFO(dev, "Sample rate: %u\r\n", dp_context->staudio_params.sample_rate);
	DRM_DEV_INFO(dev, "Data width: %u\r\n", dp_context->staudio_params.data_width);
	DRM_DEV_INFO(dev, "Channels: %u\r\n", dp_context->staudio_params.channels);

out:
	return ret;
}

static void tcc_dpv14_aud_shutdown(struct device *dev, void *data)
{
	struct tccdrm_dptx_context *dp_context;
	struct tccdrm_dp_context *dev_context;

	(void)dev;

	dev_context = (struct tccdrm_dp_context *)data;
	dp_context = dev_context->dp;

	if (dev_context->dp == NULL) {
		/*For KCS*/
		DRM_DEV_ERROR(dev, "dp context is NULL\n");
	} else {
		/*For KCS*/
		(void)dp_context->aud_ops->disable_audio(dp_context->dp_id);
	}

	DRM_DEV_INFO(dev, "shutdown to DP %d\r\n", dp_context->dp_id);
}

static int tcc_dpv14_aud_mute(struct device *dev, void *data, bool enable, int direction)
{
	int ret = 0;
	struct tccdrm_dptx_context *dp_context;
	struct tccdrm_dp_context *dev_context;

	(void)dev;
	(void)direction;

	dev_context = (struct tccdrm_dp_context *)data;
	dp_context = dev_context->dp;

	if (dev_context->dp == NULL) {
		DRM_DEV_ERROR(dev, "dp context is NULL\n");

		goto out;
	}

	ret = dp_context->aud_ops->set_audio_mute(dp_context->dp_id, enable);

	DRM_DEV_INFO(dev, "DP %d: audio %s\r\n",
							dp_context->dp_id,
							enable ? "mute":"unmute");

out:
	return ret;
}

static int tcc_dpv14_aud_get_eld(struct device *dev,
				 void *data, u8 *buf, size_t len)
{
	int ret = 0;
	size_t edid_max;
	struct edid *edid_buf;
	struct tccdrm_dptx_context *dp_context;
	struct tccdrm_dp_context *dev_context;

	dev_context = (struct tccdrm_dp_context *)data;
	dp_context = dev_context->dp;

	if (dev_context->dp == NULL) {
		DRM_DEV_ERROR(dev, "dp context is NULL\n");
		goto out;
	}
	#if defined(CONFIG_DRM_TELECHIPS_SUPPORT_REAL_HPD)
	if (tccdrm_dp_get_hpd_state(dev_context) == 0) {
		DRM_DEV_INFO(dev, "WARN: The DisplayPort is not connected yet\n");
		ret = -ENODEV;
		goto out;
	}
	#endif

	edid_max = (size_t)(sizeof(struct edid) * 4);

	edid_buf = (struct edid *)devm_kzalloc(dev_context->dev,
								edid_max,
								GFP_KERNEL);
	if (edid_buf == NULL) {
		DRM_DEV_ERROR(dev, "\n[ERR:%s]failed to edid buffer\r\n", __func__);
		goto out;
	}

	memset(edid_buf, 0, edid_max);

	ret = dev_context->dp->funcs->get_edid(dev_context->dp->dp_id,
							(unsigned char *)edid_buf,
							edid_max);
	if (ret != 0) {
		/*For KCS*/
		DRM_DEV_ERROR(dev, "\n[ERR:%s]DP %d: failed to get edid\r\n", __func__, dev_context->dp->dp_id);
	}

	memcpy(buf, edid_buf, min(edid_max, len));

	devm_kfree(dev_context->dev, edid_buf);

	DRM_DEV_INFO(dev, "DP %d: edid buf len as %lu\r\n", dp_context->dp_id, len);

out:
	return ret;
}


static const struct hdmi_codec_ops hdmi_audio_codec_ops = {
	.hw_params = tcc_dpv14_aud_hw_params,
	.audio_shutdown = tcc_dpv14_aud_shutdown,
	.mute_stream = tcc_dpv14_aud_mute,
	.get_eld = tcc_dpv14_aud_get_eld,
};

struct hdmi_codec_pdata hdmi_codec_data = {
	.i2s = 1,
	.spdif = 0,
	.ops = &hdmi_audio_codec_ops,
	.max_i2s_channels = 8,
};
#endif

static unsigned int tcc_dpv14_calc_pixelclock(const struct tccdrm_dp_context *dev_context,
						  struct drm_crtc_state *crtc_state)
{
	struct tcc_crtc_state *tcc_cstate = to_tcc_crtc_state(crtc_state);

	unsigned long div_val = 1000UL;
	unsigned long pixel_clock;

	bool internal_ok = (bool)true;

	/* DP pixel clock in kHz */
	if (!tcc_math_check_ulong_plus_ulong(tcc_cstate->pixel_clock, div_val)) {
		DRM_DEV_ERROR(dev_context->dev,
				  "The pixel clock is wrong in condition 1\r\n");
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		if (!tcc_math_check_ulong_minus_ulong(tcc_cstate->pixel_clock +
							  div_val, 1UL)) {
			DRM_DEV_ERROR(dev_context->dev,
					  "The pixel clock is wrong in condition 2\r\n");
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		pixel_clock = DIV_ROUND_UP(tcc_cstate->pixel_clock, div_val);

		if (tcc_math_ulong_gt_uintmax(pixel_clock)) {
			DRM_DEV_ERROR(dev_context->dev,
					  "The pixel clock is wrong in condition 3\r\n");
			internal_ok = (bool)false;
		}
	}
	if (!internal_ok) {
		pixel_clock = 0UL;
	}
	return (unsigned int)pixel_clock;
}

static int tcc_dpv14_calc_timing(const struct tccdrm_dp_context *dev_context,
				 struct drm_crtc_state *crtc_state,
				 const struct videomode *vm,
				 struct dptx_detailed_timing_t *dp_timing)
{
	unsigned int vm_flags, cmp_flags;
	bool internal_ok = (bool)true;
	int ret = 0;

	(void)memset(dp_timing, 0, sizeof(*dp_timing));
	vm_flags = (unsigned int)vm->flags;

	cmp_flags = (unsigned int)DISPLAY_FLAGS_INTERLACED;
	dp_timing->interlaced =
		((vm_flags & cmp_flags) != 0U) ? 1U : 0U;
	cmp_flags = (unsigned int)DISPLAY_FLAGS_DOUBLECLK;
	dp_timing->pixel_repetition_input =
		((vm_flags & cmp_flags) != 0U) ? 1U: 0U;
	dp_timing->h_active = vm->hactive;

	/* dp_timing->h_blanking = vm->hfront_porch +
				vm->hsync_len + vm->hback_porch; */
	if (!tcc_math_check_uint_plus_uint(vm->hfront_porch,
						vm->hsync_len)) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	} else {
		dp_timing->h_blanking = vm->hfront_porch + vm->hsync_len;
	}
	if (internal_ok) {
		if (!tcc_math_check_uint_plus_uint(dp_timing->h_blanking,
						vm->hback_porch)) {
			internal_ok = (bool)false;
			ret = -EINVAL;
		} else {
			dp_timing->h_blanking += vm->hback_porch;
		}
	}
	/* -- */

	if (internal_ok) {
		dp_timing->h_sync_offset = vm->hfront_porch;
		dp_timing->h_sync_pulse_width = vm->hsync_len;
		cmp_flags = (unsigned int)DISPLAY_FLAGS_HSYNC_LOW;
		dp_timing->h_sync_polarity =
			((vm_flags & cmp_flags) != 0U) ? 0U : 1U;
		dp_timing->v_active = vm->vactive;
	}

	if (internal_ok) {
		/* dp_timing->v_blanking = vm->vfront_porch +
					   vm->vsync_len + vm->vback_porch; */
		if (!tcc_math_check_uint_plus_uint(vm->vfront_porch,
						   vm->vsync_len)) {
			internal_ok = (bool)false;
			ret = -EINVAL;
		} else {
			dp_timing->v_blanking = vm->vfront_porch + vm->vsync_len;
		}
		if (internal_ok) {
			if (!tcc_math_check_uint_plus_uint(dp_timing->v_blanking,
						   vm->vback_porch)) {
				internal_ok = (bool)false;
				ret = -EINVAL;
			} else {
				dp_timing->v_blanking += vm->vback_porch;
			}
		}
		/* -- */
	}

	if (internal_ok) {
		dp_timing->v_sync_offset = vm->vfront_porch;
		dp_timing->v_sync_pulse_width = vm->vsync_len;
		cmp_flags = (unsigned int)DISPLAY_FLAGS_VSYNC_LOW;
		dp_timing->v_sync_polarity =
			((vm_flags & cmp_flags) != 0U) ? 0U : 1U;
	}

	if (internal_ok) {
		unsigned int pixel_clock =
			tcc_dpv14_calc_pixelclock(dev_context, crtc_state);

		if (pixel_clock == 0UL) {
			ret = -EINVAL;
		} else {
			dp_timing->pixel_clock = pixel_clock;
			DRM_DEV_INFO(dev_context->dev,
					 "[INFO] find pixel clocks %dHz\r\n",
					 dp_timing->pixel_clock);
		}
	}

	return ret;
}

static int tcc_dpv14_set_video(
	const struct tccdrm_dp_context *dev_context,
	struct drm_crtc_state *crtc_state)
{
	struct dptx_detailed_timing_t dp_timing;
	//unsigned int vm_flags, cmp_flags;
	bool internal_ok = (bool)true;
	struct videomode vm;
	int ret = 0;

	if (dev_context->dp == NULL) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}

	if (internal_ok) {
		if (dev_context->dp->funcs == NULL) {
			internal_ok = (bool)false;
			ret = -EINVAL;
		} else {
			if (dev_context->dp->funcs->set_video == NULL) {
				internal_ok = (bool)false;
				ret = -EINVAL;
			}
		}
	}
	if (internal_ok) {
		drm_display_mode_to_videomode(&crtc_state->adjusted_mode, &vm);

		ret = tcc_dpv14_calc_timing(dev_context, crtc_state, &vm, &dp_timing);
		if (ret < 0) {
			internal_ok = (bool)false;
		}
	}

	if (internal_ok) {
		if (dev_context->dp->funcs->set_video(dev_context->dp->dp_id, &dp_timing) < 0) {
			ret = -ENODEV;
		}
	}

	return ret;
}

static int tcc_dpv14_enable_video(
	const struct tccdrm_dp_context *dev_context, unsigned char en)
{
	bool internal_ok = (bool)true;
	int ret = -EINVAL;

	if (dev_context->dp == NULL) {
		internal_ok = (bool)false;
	}
	if (internal_ok && (dev_context->dp->funcs == NULL)) {
		internal_ok = (bool)false;
	}
	if (internal_ok && (dev_context->dp->funcs->set_enable_video == NULL)) {
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		ret = dev_context->dp->funcs->set_enable_video(dev_context->dp->dp_id,
							   en);
	}
	return ret;
}

static enum drm_connector_status tccdrm_dp_detect(struct drm_connector *connector,
						   bool force)
{
	enum drm_connector_status connector_status =
				connector_status_connected;
	#if defined(CONFIG_DRM_TELECHIPS_SUPPORT_REAL_HPD)
	struct tccdrm_dp_context *dev_context = connector_to_context(connector);
	#else
	(void)connector;
	#endif

	(void)force;

	#if defined(CONFIG_DRM_TELECHIPS_SUPPORT_REAL_HPD)
	/**
	 * The HAL(Hardware Abstraction layer) must be able to
	 * handle HPD events provided by DRM.
	 * However, the HAL provided by Telechips can not
	 * handle this HPD events, so until the HAL is ready,
	 * DRM always returns HPD status to true.
	 */
	if (dev_context->dp != NULL) {
		if (tccdrm_dp_get_hpd_state(dev_context) == 0) {
			connector_status =
				connector_status_disconnected;
		}
	}
	#endif
	#if defined(CONFIG_DRM_TELECHIPS_DP_PROC)
	if (dev_context->manual_hpd != connector_status_unknown) {
		connector_status = dev_context->manual_hpd;
	}
	DRM_DEV_DEBUG(dev_context->dev,
			"[WARN] status = %s\r\n",
			(connector_status == connector_status_connected) ?
			"connected" : "disconnected");
	#endif

	return connector_status;
}

static void tccdrm_dp_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

#if defined(CONFIG_TELECHIPS_DP_DRIVER_V1_4) || defined(CONFIG_TELECHIPS_DP_DRIVER_V1_4_MODULE)
static int tccdrm_dp_connector_atomic_get_property(
	struct drm_connector *connector,
	const struct drm_connector_state *connector_state,
	struct drm_property *out_property,
	uint64_t *val)
{
	const struct tccdrm_dp_context *dev_context = connector_to_context(connector);

	/*
	 * FIX
	 * is not used in the function.
	 */
	(void)connector_state;

	if (out_property == dev_context->dp_prop.audio_freq) {
		uint64_t audio_freq;

		//audio_freq = dev_context->dp->funcs->get_audio_freq();
		audio_freq = (uint64_t)dev_context->dp_prop_data.audio_freq;
		*val = audio_freq;
	} else {
		if (out_property == dev_context->dp_prop.audio_type) {
			uint64_t audio_type;

			/* *val = dev_context->dp->funcs->get_audio_type(); */
			audio_type = (uint64_t)dev_context->dp_prop_data.audio_type;
			*val = audio_type;
		}
	}
	return 0;
}

static int
tccdrm_dp_connector_atomic_set_property(struct drm_connector *connector,
					  /* DP no.6 */
					  struct drm_connector_state *conn_state,
					  /* DP no.6 */
					 struct drm_property *in_property,
					 uint64_t val)
{
	struct tccdrm_dp_context *dev_context = connector_to_context(connector);
	bool internal_ok = (bool)true;
	int ival;
	int ret = 0;

	(void)conn_state;

	if (tcc_math_u64_gt_intmax(val)) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	} else {
		ival = (int)val;
	}

	if (internal_ok) {
		if (in_property == dev_context->dp_prop.audio_freq) {
			if (ival <= (int)PROP_192HZ) {
				//dev_context->dp->funcs->set_audio_freq(val);
				dev_context->dp_prop_data.audio_freq =
					(enum tcc_prop_audio_freq)ival;
			}
		} else if (in_property == dev_context->dp_prop.audio_type) {
			if (ival <= (int)PROP_TYPE_DTS_HD) {
				//dev_context->dp->funcs->set_audio_freq(val);
				dev_context->dp_prop_data.audio_type =
					(enum tcc_prop_audio_type)ival;
			}
		} else {
			ret = -EINVAL;
		}
	}
	return ret;
}
#endif
static const struct drm_connector_funcs tccdrm_dp_connector_funcs = {
	.detect = tccdrm_dp_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = tccdrm_dp_connector_destroy,
	.reset = drm_atomic_helper_connector_reset,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
	#if defined(CONFIG_TELECHIPS_DP_DRIVER_V1_4) || defined(CONFIG_TELECHIPS_DP_DRIVER_V1_4_MODULE)
	.atomic_get_property = tccdrm_dp_connector_atomic_get_property,
	.atomic_set_property = tccdrm_dp_connector_atomic_set_property,
	#endif
};

static int connector_get_modes_from_dp(struct tccdrm_dp_context *dev_context)
{
	struct drm_connector *connector = &dev_context->connector;
	bool internal_ok = (bool)true;
	int mode_count = 0;

	int edid_max = (int)sizeof(struct edid) * 4;
	struct edid *edid_from_sink = NULL;

	if (dev_context->dp == NULL) {
		DRM_ERROR("dp is NULL\n");
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		if (dev_context->dp->funcs == NULL) {
			DRM_ERROR("funcs is NULL\n");
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		if (dev_context->dp->funcs->get_edid == NULL) {
			DRM_ERROR("get_edid is NULL\n");
			internal_ok = (bool)false;
		}
	}

	if (internal_ok) {
		edid_from_sink =
			(struct edid *)devm_kzalloc(dev_context->dev,
							(unsigned int)edid_max,
							GFP_KERNEL);
		if (edid_from_sink != NULL) {
			if(dev_context->dp->funcs->get_edid(dev_context->dp->dp_id,
								(unsigned char *)edid_from_sink,
								edid_max) == 0) {
				#if defined(CONFIG_REFCODE_PRE_K54)
				if (drm_mode_connector_update_edid_property(connector,
										edid_from_sink) == 0)
				#else
				if (drm_connector_update_edid_property(connector,
									edid_from_sink) == 0)
				#endif
				{

				mode_count = drm_add_edid_modes(connector, edid_from_sink);

				#if defined(CONFIG_REFCODE_PRE_K54)
				drm_edid_to_eld(connector, edid_from_sink);
				#endif
				}
			}
		}
	}

	if (edid_from_sink != NULL) {
		devm_kfree(dev_context->dev, edid_from_sink);
	}

	return mode_count;
}

static int tccdrm_dp_register_mode(struct drm_connector *connector,
								   struct drm_display_mode *in_mode)
{
	const struct tccdrm_dp_context *dev_context;
	bool internal_ok = (bool)true;
	int tmp_val;
	int ret = 0;

	if (connector == NULL) {
			(void)DRM_DEV_ERROR(NULL, "[ERROR] connector is NULL\r\n");
			ret = -EINVAL;
			internal_ok = (bool)false;
	}

	if (internal_ok) {
			dev_context = (const struct tccdrm_dp_context *)connector_to_context(connector);
			if (dev_context == NULL) {
					DRM_DEV_ERROR(NULL, "dev_context is NULL\r\n");
					ret = -EINVAL;
					internal_ok = (bool)false;
			}
	}

	if (internal_ok) {
			/*
				* Physical size as value that display
				* resolution divided by 10.
				*/
			if (in_mode->hdisplay > 10u) {
					/* DP no.14 */
					tmp_val = DIV_ROUND_UP(in_mode->hdisplay, 10);
			} else {
					tmp_val = (int)in_mode->hdisplay;
			}
			if (tmp_val > 0) {
					connector->display_info.width_mm = (unsigned int)tmp_val;
			} else {
					internal_ok = (bool)false;
					ret = -EINVAL;
			}
	}
	if (internal_ok) {
			/*
				* Physical size as value that display
				* resolution divided by 10.
				*/
			if (in_mode->vdisplay > 10u) {
					/* DP no.14 */
					tmp_val = DIV_ROUND_UP(in_mode->vdisplay, 10);
			} else {
					tmp_val = (int)in_mode->vdisplay;
			}
			if (tmp_val > 0) {
					connector->display_info.height_mm = (unsigned int)tmp_val;
					drm_mode_probed_add(connector, in_mode);
			} else {
					ret = -EINVAL;
			}
	}

	return ret;
}

static int tccdrm_dp_get_dev_node_modes(struct tccdrm_dp_context *dev_context)
{
	struct drm_connector *connector = &dev_context->connector;
	struct drm_display_mode *modes = NULL;
	bool internal_ok = (bool)true;
	int i, mode_count = 0;

	const struct display_timings *timings;
	struct videomode vm;
	int num_timings = 0;

	if (dev_context->timings == NULL) {
			internal_ok = (bool)false;
	}
	if (internal_ok) {
			timings = dev_context->timings;
			if (!tcc_math_uint_gt_intmax(timings->num_timings)) {
					num_timings = (int)timings->num_timings;
			}
	}
	for (i = 0; i < num_timings; i++) {
		if (videomode_from_timings(timings, &vm,
									(unsigned int)i) < 0) {
				continue;
		}

		modes = drm_mode_create(connector->dev);
		if (modes == NULL) {
				DRM_DEV_ERROR(dev_context->dev,
								"failed to create drm_mode\r\n");
				continue;
		}

		drm_display_mode_from_videomode(&vm, modes);

		modes->type = DRM_MODE_TYPE_DRIVER;
		drm_mode_set_name(modes);
		if (timings->native_mode == (unsigned int)i) {
				DRM_DEV_INFO(dev_context->dev,
								"[INFO] Native mode is detected at index [%d] name [%s]\r\n",
						mode_count, modes->name);
				modes->type |= DRM_MODE_TYPE_PREFERRED;
		}
		if (tccdrm_dp_register_mode(connector, modes) < 0) {
				drm_mode_destroy(connector->dev, modes);
				continue;
		}

		if (mode_count <= (DRM_INT_MAX -1)) {
			mode_count++;
		}
	}
	return mode_count;
}

/*
 * This function called in drm_helper_probe_single_connector_modes
 * return is mode_count
 */
static int tccdrm_dp_get_modes(struct drm_connector *connector)
{
	struct tccdrm_dp_context *dev_context;
	int mode_count = 0;

	if (connector != NULL) {
		dev_context = (struct tccdrm_dp_context *)connector_to_context(connector);
		if (dev_context != NULL) {
			/* step1: Check panels */
			if (dev_context->panel != NULL) {
				#if defined(CONFIG_REFCODE_PRE_K510)
				mode_count = drm_panel_get_modes(dev_context->panel);
				#else
				mode_count = drm_panel_get_modes(dev_context->panel, connector);
				#endif
				if (mode_count == 0) {
					DRM_DEV_INFO(dev_context->dev,
							"[WARN] It has a panel node, but there is no detailed-display-timing information in panel node.\r\n");
				}
			}

			/* step2: Check drm crtc */
			if (mode_count == 0) {
				mode_count = tccdrm_dp_get_dev_node_modes(dev_context);
			}
			if (mode_count == 0) {
				DRM_DEV_INFO(dev_context->dev,
						"There is no detailed-display-timing information in device node.\r\n");
			}

			/* step3: Read EDID from SINK */
			if (mode_count == 0) {
				/* clearn edid property */
				#if defined(CONFIG_REFCODE_PRE_K54)
				if (drm_mode_connector_update_edid_property(connector, NULL) < 0) {
					DRM_DEV_ERROR(dev_context->dev,
						"drm_mode_connector_update_edid_property\r\n");
				#else
				//if edid is null set 0 edid in connector edid property
				if (drm_connector_update_edid_property(connector, NULL) < 0) {
					DRM_DEV_INFO(dev_context->dev,
						"drm_connector_update_edid_property\r\n");
				#endif
				} else {
					mode_count = connector_get_modes_from_dp(dev_context);
				}

				if (mode_count == 0) {
					DRM_DEV_ERROR(dev_context->dev,
							"There is no detailed-display-timing information from EDID.\r\n");
				}
			} else {
				/*
				* avoid a coverity issues
				*/
			}
		} else {
			DRM_DEV_ERROR(NULL, "dev_context is NULL\r\n");
		}
	} else {
		DRM_DEV_ERROR(NULL, "connector is NULL\r\n");
	}

	return mode_count;
}

#if defined(CONFIG_REFCODE_PRE_K54)
#else
static struct drm_encoder *tccdrm_dp_best_single_encoder(
	struct drm_connector *connector)
{
	struct tccdrm_dp_context *dev_context = connector_to_context(connector);

	return &dev_context->encoder;
}
#endif

static const struct
drm_connector_helper_funcs tccdrm_dp_connector_helper_funcs = {
	.get_modes = tccdrm_dp_get_modes,
	#if defined(CONFIG_REFCODE_PRE_K54)
	.best_encoder = drm_atomic_helper_best_encoder,
	#else
	.best_encoder = tccdrm_dp_best_single_encoder,
	#endif
};

static int tccdrm_dp_set_connector(struct tccdrm_dp_context *dev_context)
{
	struct drm_connector *connector = &dev_context->connector;
	struct drm_encoder *encoder = &dev_context->encoder;
	bool internal_ok = (bool)true;
	int ret = 0;

	connector->polled = DRM_CONNECTOR_POLL_HPD;

	ret = drm_connector_init(encoder->dev,
				 connector, &tccdrm_dp_connector_funcs,
				 DRM_MODE_CONNECTOR_DisplayPort);
	if (ret < 0) {
		DRM_DEV_ERROR(dev_context->dev,
				  "failed to initialize connector with drm\n");
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		int num_values;

		/* audio_freq */
		num_values = ARRAY_SIZE(tcc_prop_audio_freq_names);
		dev_context->dp_prop.audio_freq =
			drm_property_create_enum(connector->dev, 0,
				"audio_freq",
				tcc_prop_audio_freq_names,
				num_values);

		if(dev_context->dp_prop.audio_freq != NULL) {
			drm_object_attach_property(&connector->base,
				dev_context->dp_prop.audio_freq, 0);
		}

		num_values = ARRAY_SIZE(tcc_prop_audio_type_names);
		dev_context->dp_prop.audio_type =
			drm_property_create_enum(connector->dev, 0,
				"audio_type",
				tcc_prop_audio_type_names,
				num_values);

		if(dev_context->dp_prop.audio_type != NULL) {
			drm_object_attach_property(&connector->base,
				dev_context->dp_prop.audio_type, 0);
		}

		drm_connector_helper_add(connector,
					 &tccdrm_dp_connector_helper_funcs);
		#if defined(CONFIG_REFCODE_PRE_K54)
		(void)drm_mode_connector_attach_encoder(connector, encoder);
		#else
		(void)drm_connector_attach_encoder(connector, encoder);
		#endif
		#if defined(CONFIG_REFCODE_PRE_K510)
		if ((dev_context->panel != NULL) &&
			(dev_context->panel->connector == NULL)) {
			(void)drm_panel_attach(dev_context->panel,
			&dev_context->connector);
		}
		#endif
	}

	return ret;
}

static void
tccdrm_dp_mode_set(struct drm_encoder *encoder,
			struct drm_crtc_state *crtc_state,
			struct drm_connector_state *connector_state)
{
	const struct tccdrm_dp_context *dev_context = encoder_to_context(encoder);

	/*
	 * FIX
	 * not used in the function.
	 */
	(void)connector_state;

	if (crtc_state->active) {
		if (dev_context->panel != NULL) {
			(void)drm_panel_prepare(dev_context->panel);
			/*  panel->funcs->prepare(panel) */
		}
		(void)tcc_dpv14_set_video(dev_context,
					crtc_state);
		(void)tcc_dpv14_enable_video(dev_context, 1);
	}
}

static void tccdrm_dp_enable(struct drm_encoder *encoder,
				 struct drm_atomic_state *drm_astate)
{
	const struct tccdrm_dp_context *dev_context = encoder_to_context(encoder);

	(void)drm_astate;

	if (encoder->crtc == NULL) {
		DRM_DEV_INFO(dev_context->dev, "[INFO] encoder is not ready\r\n");
	} else {
		struct tcc_crtc_state *tcc_cstate = to_tcc_crtc_state(encoder->crtc->state);

		DRM_DEV_INFO(dev_context->dev,
			"[INFO] tccdrm_dp is connectd to lcdc_num %u\r\n",
			tcc_cstate->lcdc_num);

		if (dev_context->panel != NULL) {
			(void)drm_panel_enable(dev_context->panel);
			/* panel->funcs->enable(panel); */
		}
	}
}

static void tccdrm_dp_disable(struct drm_encoder *encoder,
			  struct drm_atomic_state *drm_astate, unsigned int stage)
{
	const struct tccdrm_dp_context *dev_context =
		(const struct tccdrm_dp_context *)encoder_to_context(encoder);

	(void)drm_astate;

	switch (stage) {
	case 1:
		if (dev_context->panel != NULL) {
			(void)drm_panel_disable(dev_context->panel);
			/* panel->funcs->disable(panel); */
		}
		break;
	case 2:
		(void)tcc_dpv14_enable_video(dev_context, 0);
		if (dev_context->panel != NULL) {
			mdelay(30);
			(void)drm_panel_unprepare(dev_context->panel);
			/* panel->funcs->unprepare(panel); */
		}
		break;
	default:
		pr_debug("[DEBUG][DRM_DP] Unknown stage\r\n");
		break;
	}
}

static void tccdrm_dp_disable_stage1(struct drm_encoder *encoder,
			  struct drm_atomic_state *drm_astate)
{
	tccdrm_dp_disable(encoder, drm_astate, 1);
}

static void tccdrm_dp_disable_stage2(struct drm_encoder *encoder)
{
	tccdrm_dp_disable(encoder, NULL, 2);
}

static int tccdrm_dp_check(struct drm_encoder *encoder,
					  struct drm_crtc_state *crtc_state,
					  struct drm_connector_state *conn_state)
{
	struct tcc_crtc_state *tcc_cstate = to_tcc_crtc_state(crtc_state);

	struct tccdrm_dp_context *dev_context = encoder_to_context(encoder);

	(void)conn_state;

	/* Update connector */
	tcc_cstate->connector = &dev_context->connector;
	tcc_cstate->connector_type = DRM_MODE_CONNECTOR_DisplayPort;
	//DRM_DEV_INFO(dev_context->dev,
	//		 "[INFO] update connector_type = %d\r\n",
	//		 tcc_cstate->connector_type);

	//DRM_DEV_INFO(dev_context->dev,
	//		 "[INFO] check pixel clocks %ldHz\r\n",
	//		 tcc_cstate->pixel_clock);

	return 0;
}

static const struct drm_encoder_helper_funcs tccdrm_dp_encoder_helper_funcs = {
	.atomic_mode_set = tccdrm_dp_mode_set,
	.atomic_enable = tccdrm_dp_enable,
	.atomic_disable = tccdrm_dp_disable_stage1,
	.atomic_check = tccdrm_dp_check,
	.disable = tccdrm_dp_disable_stage2,
};

static const struct drm_encoder_funcs tccdrm_dp_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static int tccdrm_dp_parse_dt(struct tccdrm_dp_context *dev_context)
{
	const struct device *dev = dev_context->dev;
	const struct device_node *dn = dev->of_node;
	struct device_node *np;
	int ret = 0;

	np = of_get_child_by_name(dn, "display-timings");
	if (np != NULL) {
		of_node_put(np);

		dev_context->timings = of_get_display_timings(dn);
		if (dev_context->timings == NULL) {
			DRM_DEV_INFO(dev,
					 "[WARN] failed to of_get_display_timings\n");
			ret = -ENODEV;
		}
	} else {
		DRM_DEV_DEBUG(dev,
			"[DEBUG] cannot find display-timings node\n");
	}

	return ret;
}

static int tccdrm_dp_attach(struct drm_encoder *encoder, int dp_id,
				 int in_flags)
{
	const struct tccdrm_dp_context *dev_context = encoder_to_context(encoder);

	(void)dp_id;

	/*
	 * FIX
	 * in the function.
	 */
	(void)in_flags;

	(void)drm_helper_hpd_irq_event(dev_context->encoder.dev);

	/*
	 * In normal circumstances, when an HPD (Hot Plug Detect) unplug event occurs,
	 * the upper layers (userspace or DRM core) should stop rendering by disabling
	 * the CRTC and related pipelines.
	 *
	 * However, there are cases where this deactivation does not happen properly.
	 * As a result, even when HPD replug occurs later, rendering is not re-enabled.
	 *
	 *
	 * To avoid a blank screen in such scenarios, we explicitly trigger
	 * DisplayPort video output activation inside the attach callback.
	 *
	 * This ensures that, regardless of upper layer behavior,
	 * video output is resumed correctly when the connector is re-attached.
	 */
	if ((encoder->crtc != NULL) && (encoder->crtc->state != NULL)) {
		if (encoder->crtc->state->enable) {
			(void)tcc_dpv14_set_video(dev_context,
						  encoder->crtc->state);
			(void)tcc_dpv14_enable_video(dev_context, 1);
		}
	}
	DRM_DEV_INFO(dev_context->dev, "[INFO] display port is attached\r\n");
	return 0;
}

static int tccdrm_dp_detach(struct drm_encoder *encoder, int dp_id,
				 int in_flags)
{
	const struct tccdrm_dp_context *dev_context = encoder_to_context(encoder);

	(void)dp_id;
	(void)in_flags;

	(void)drm_helper_hpd_irq_event(dev_context->encoder.dev);
	DRM_DEV_INFO(dev_context->dev, "[INFO] display port is detached\r\n");
	return 0;
}

static int tccdrm_dp_register_helper_funcs(struct drm_encoder *encoder,
				struct dptx_drm_helper_funcs *helper_funcs)
{
	const struct tccdrm_dp_context *dev_context =
		(const struct tccdrm_dp_context *)encoder_to_context(encoder);
	int ret = 0;

	if (dev_context->dp == NULL) {
		ret = -EINVAL;
	} else {
		dev_context->dp->funcs = helper_funcs;
	}
	return ret;
}

static struct tcc_drm_dp_callback_funcs dp_callback_funcs = {
	.attach = tccdrm_dp_attach,
	.detach = tccdrm_dp_detach,
	.register_helper_funcs = tccdrm_dp_register_helper_funcs,
};

#if defined(CONFIG_DRM_TELECHIPS_DP_PROC)
static int tccdrm_proc_open(struct inode *finode, struct file *filp)
{
	(void)finode;

	(void)filp;

	(void)try_module_get(THIS_MODULE);

	return 0;
}

static int tccdrm_proc_close(struct inode *finode, struct file *filp)
{
	(void)finode;

	(void)filp;

	module_put(THIS_MODULE);

	return 0;
}

static ssize_t tccdrm_proc_write_hpd(struct file *filp, const char __user *buffer,
						size_t cnt, loff_t *off_set)
{
	bool internal_ok = (bool)true;
	ssize_t writed = 0;
	int ret, hpd = 0;
	char *hpd_buff = NULL;

	struct tccdrm_dp_context *dev_context = PDE_DATA(file_inode(filp));

	if (!tcc_math_check_ulong_plus_ulong(cnt, 1UL)) {
		internal_ok = (bool)false;
		writed = -ENOMEM;
	}
	if (internal_ok) {
		hpd_buff = devm_kzalloc(dev_context->dev, cnt + 1UL, GFP_KERNEL);

		if (hpd_buff == NULL) {
			writed = -ENOMEM;
			internal_ok = (bool)false;
		}
	}

	if (internal_ok) {
		writed = simple_write_to_buffer(hpd_buff, cnt, off_set, buffer, cnt);
		writed = (writed < 0) ? 0 : writed;
		if ((size_t)writed != cnt) {
			devm_kfree(dev_context->dev, hpd_buff);
			writed = -EIO;
			internal_ok = (bool)false;
		}
	}

	if (internal_ok) {
		hpd_buff[cnt] = '\0';
		ret = kstrtoint(hpd_buff, 10, &hpd);
		devm_kfree(dev_context->dev, hpd_buff);
		if (ret < 0) {
			writed = -EINVAL;
			internal_ok = (bool)false;
		}
	}

	if (internal_ok) {
		switch (hpd) {
		case 0:
			dev_context->manual_hpd = connector_status_disconnected;
			break;
		case 1:
			dev_context->manual_hpd = connector_status_connected;
			break;
		case -1:
		default:
			dev_context->manual_hpd = connector_status_unknown;
			break;
		}
		(void)drm_helper_hpd_irq_event(dev_context->encoder.dev);
	}
	return writed;
}

static struct drm_connector *tccdrm_dp_find_connector_from_crtc(const struct drm_crtc *crtc)
{
	struct drm_connector *connector = NULL;
	struct drm_encoder *encoder;
	struct tccdrm_dp_context *dev_context;

	/* DP no.11 */
	drm_for_each_encoder(encoder, crtc->dev)
		if (encoder->crtc == crtc) {
			dev_context = encoder_to_context(encoder);
			connector = &dev_context->connector;
		}
	return connector;
}

static int tccdrm_proc_check_nullptr(const struct drm_connector *connector)
{
	int ret = 1;

	if(connector != NULL) {
		if(connector->edid_blob_ptr != NULL) {
			if(connector->edid_blob_ptr->data != NULL) {
				ret = 0;
			}
		}
	}

	return ret;
}

static ssize_t tccdrm_proc_read_edid(
	struct file *filp, char __user *usr_buf, size_t cnt, loff_t *off_set)
{
	const struct tccdrm_dp_context *dev_context = (const struct tccdrm_dp_context *)PDE_DATA(file_inode(filp));
	const struct drm_crtc *crtc = (dev_context != NULL) ? dev_context->encoder.crtc : NULL;
	bool internal_ok = (bool)true;
	ssize_t bytes_read = 0;
	unsigned int u;

	(void)cnt;
	(void)off_set;

	(void)usr_buf;

	if (crtc != NULL) {
		const struct drm_property_blob *edid_blob = NULL;
		const struct drm_connector *connector =
			(const struct drm_connector *)tccdrm_dp_find_connector_from_crtc(
				(const struct drm_crtc *)crtc);
		const unsigned char *data;

		if(tccdrm_proc_check_nullptr(connector) == 0) {
			edid_blob = (const struct drm_property_blob *)connector->edid_blob_ptr;
			data = edid_blob->data;

			(void)pr_info(
				"[INFO] CRTC_ID[%ud] length = %zu",
				drm_crtc_index(crtc),
				edid_blob->length);
			for (u = 0U; u < edid_blob->length; u += 8U) {
				(void)pr_info(
					"%02x %02x %02x %02x %02x %02x %02x %02x\r\n",
					data[u+0U],
					data[u+1U],
					data[u+2U],
					data[u+3U],
					data[u+4U],
					data[u+5U],
					data[u+6U],
					data[u+7U]);
			}

			bytes_read = (ssize_t)edid_blob->length;
		}
	}

	return bytes_read;
}

static const struct file_operations proc_fops_hpd = {
	.owner   = THIS_MODULE,
	.open	= tccdrm_proc_open,
	.release = tccdrm_proc_close,
	.write   = tccdrm_proc_write_hpd,
};

static const struct file_operations proc_fops_edid = {
	.owner   = THIS_MODULE,
	.open	= tccdrm_proc_open,
	.release = tccdrm_proc_close,
	.read	 = tccdrm_proc_read_edid,
};
#endif


#if (defined(CONFIG_DRM_PANEL_DPV14_TELECHIPS) || defined(CONFIG_DRM_PANEL_DPV14_TELECHIPS_MODULE)) && \
	(defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC807X))
static int tccdrm_dp_parse_panel(const struct device *dev, struct drm_panel **panel)
{
	const struct device_node *np = dev->of_node;
	const struct device_node *remote = NULL;
	int ret = 0;

	/*
	* of_graph_get_remote_node() produces a noisy error message if port
	* node isn't found and the absence of the port is a legit case here,
	* so at first we silently check whether graph presents in the
	* device-tree node.
	*/
	if (of_graph_is_present(np)) {
		remote = of_graph_get_remote_node(np, 1, 0);
		if (remote != NULL) {
			*panel = of_drm_find_panel(remote);
			if (IS_ERR(*panel)) {
				ret = PTR_ERR(*panel);
				if (ret == -EPROBE_DEFER) {
					DRM_DEV_INFO(dev, "[INFO] DRM DP panel is not ready\r\n");
				}

				*panel = NULL;
			}
		} else {
			ret = -ENODEV;
		}
	} else {
		ret = -ENODEV;
	}

	if (*panel != NULL) {
		DRM_DEV_INFO(dev, "[INFO] has DRM DP panel\r\n");
	}
	else {
		DRM_DEV_INFO(dev, "[INFO] has no DRM DP panel\r\n");
	}

	return ret;
}
#endif

#if defined(CONFIG_DRM_TELECHIPS_DP_PROC)
static void tccdrm_dp_make_proc(struct tccdrm_dp_context *dev_context)
{
	const struct device *dev = dev_context->dev;
	const char *proc_name = dev_name(dev);

	dev_context->proc_dir = proc_mkdir(proc_name, NULL);
	if (dev_context->proc_dir != NULL) {
		dev_context->proc_hpd = proc_create_data(
			"hpd", (unsigned int)(S_IFREG | 0444U),
			dev_context->proc_dir, &proc_fops_hpd, dev_context);
		if (dev_context->proc_hpd == NULL) {
			DRM_DEV_INFO(
				dev,
				"[WARN] Could not create file system @ /%s/%s/hpd\r\n",
				proc_name,
				dev_name(dev));
		}
		/* DP no.23B */
		dev_context->proc_edid = proc_create_data(
			"edid", (unsigned int)(S_IFREG | 0555U),
			dev_context->proc_dir, &proc_fops_edid, dev_context);
		if (dev_context->proc_edid == NULL) {
			DRM_DEV_INFO(
				dev,
				"[WARN] Could not create file system @ /%s/%s/edid\r\n",
				proc_name,
				dev_name(dev));
		}
	} else {
		DRM_DEV_INFO(dev,
				"[WARN] Could not create file system @ %s\r\n",
				proc_name);
	}
}
#endif

#if 0
struct drm_encoder *tccdrm_dp_find_encoder_from_crtc(const struct drm_crtc *crtc)
{
	struct drm_encoder *encoder = NULL;

	/* DP no.11 */
	drm_for_each_encoder(encoder, crtc->dev)
		if (encoder->crtc == crtc) {
			break;
		}
	return encoder;
}
#endif

static int tccdrm_dp_bind(struct device *dev, struct device *master_dev, void *data)
{
	struct tccdrm_dp_context *dev_context;
	struct drm_device *drm_dev = (struct drm_device *)data;
	bool cleanup_encoder = (bool)false;
	bool internal_ok = (bool)true;
	struct drm_encoder *encoder;
	int ret = 0;

	(void)master_dev;

	dev_context = dev_get_drvdata(dev);
	if (dev_context == NULL) {
		internal_ok = (bool)false;
		ret = -ENOMEM;
	}
	if (internal_ok) {
		encoder = &dev_context->encoder;

		encoder->possible_crtcs =
			drm_of_find_possible_crtcs(drm_dev, dev->of_node);
		if (encoder->possible_crtcs == 0U) {
			if (dev_context->dp != NULL) {
				(void)tcc_dp_unregister_drm(&dev_context->encoder);
				devm_kfree(dev, dev_context->dp);
				dev_context->dp = NULL;
			}

			internal_ok = (bool)false;
			/*
			 * Even if possible_crtcs is 0, no error is returned.
			 * The tccdrm determines whether crtc is used or not
			 * through device tree and kconfig settings.
			 * If this driver returns an error when the crtc is set
			 * to enabled in the device tree and disabled in
			 * Kconfig, the tccdrm will be failed to bind.
			 * Therefore, it does not return an error to operate
			 * normally even in this exception condition.
			 */
			DRM_DEV_INFO(dev,
					 "This encoder will also be deactivated because the crtc connected with this encoder is probably in a deactivated state.\r\n");
		}
	}
	if (internal_ok) {
		ret = drm_encoder_init(drm_dev,
					   encoder,
					   &tccdrm_dp_encoder_funcs,
					   DRM_MODE_ENCODER_TMDS, NULL);
		if (ret != 0) {
			DRM_DEV_ERROR(dev, "failed to initialize encoder with drm\n");
			internal_ok = (bool)false;
		} else {
			cleanup_encoder = (bool)true;
		}
	}
	if (internal_ok) {
		drm_encoder_helper_add(encoder, &tccdrm_dp_encoder_helper_funcs);
		ret = tccdrm_dp_set_connector(dev_context);
		if (ret < 0) {
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		dev_context->binded = (bool)true;
	} else {
		if (cleanup_encoder) {
			drm_encoder_cleanup(encoder);
		}
	}
	return ret;
}

static void tccdrm_dp_unbind(struct device *dev,
				 struct device *master_dev,
				 void *data)
{
	struct tccdrm_dp_context *dev_context = dev_get_drvdata(dev);
	struct drm_connector *connector = &dev_context->connector;
	struct drm_encoder *encoder = &dev_context->encoder;

	(void)master_dev;

	(void)data;

	#if defined(CONFIG_REFCODE_PRE_K510)
	if (dev_context->panel != NULL) {
		(void)drm_panel_detach(dev_context->panel);
	}
	#endif

	if (dev_context->binded) {
		if (encoder->funcs->destroy != NULL) {
			encoder->funcs->destroy(encoder);
		}
		if (connector->funcs->destroy != NULL) {
			connector->funcs->destroy(connector);
		}
	}
}

static const struct component_ops tcc_dp_component_ops = {
	.bind = tccdrm_dp_bind,
	.unbind = tccdrm_dp_unbind,
};

static int tccdrm_dp_probe_base(struct platform_device *plat_dev, struct drm_panel *pdrm_panel)
{
	struct tccdrm_dp_context *dev_context;
	struct device *dev = &plat_dev->dev;
	int ret = -EPROBE_DEFER;

	dev_context = devm_kzalloc(dev, sizeof(*dev_context), GFP_KERNEL);
	if (dev_context != NULL) {
		dev_context->dev = dev;
		dev_context->timings = NULL;

		#if defined(CONFIG_DRM_TELECHIPS_DP_PROC)
		dev_context->manual_hpd = connector_status_unknown;
		#endif

		(void)tccdrm_dp_parse_dt(dev_context);
		dev_context->panel = pdrm_panel;

		dev_context->dp = devm_kzalloc(dev, sizeof(*dev_context->dp), GFP_KERNEL);
		if (dev_context->dp != NULL) {
			dev_context->dp->dp_id = tcc_dp_register_drm(
									&dev_context->encoder,
									&dp_callback_funcs);
			if (dev_context->dp->dp_id < 0) {
				ret = -ENODEV;
				DRM_DEV_ERROR(dev, "Displayport ID is out of range\r\n");
				devm_kfree(dev, dev_context->dp);
				devm_kfree(dev,dev_context);
			} else {
				#if defined(CONFIG_TELECHIPS_DPTX_AUDIO) || defined(CONFIG_TELECHIPS_DPTX_AUDIO_MODULE)
				(void)tcc_dpv14_aud_get_ops(dev_context);

				hdmi_codec_data.data = (void *)dev_context;

				dev_context->dp->aud_dev =
					platform_device_register_data(dev,
								      HDMI_CODEC_DRV_NAME,
								      PLATFORM_DEVID_AUTO,
								      &hdmi_codec_data,
								      sizeof(hdmi_codec_data));

				DRM_DEV_INFO(dev, "\r\n");
				DRM_DEV_INFO(dev, "===DP %d : data to %p\r\n", dev_context->dp->dp_id, dev_context);
				#endif

				#if defined(CONFIG_DRM_TELECHIPS_DP_PROC)
				tccdrm_dp_make_proc(dev_context);
				#endif

				(void)DRM_INFO("Initialized %s %d.%d.%d %s\r\n",
						plat_dev->name,
						DRIVER_MAJOR,
						DRIVER_MINOR,
						DRIVER_PATCH,
						DRIVER_DATE);
				platform_set_drvdata(plat_dev, dev_context);
				ret = component_add(dev, &tcc_dp_component_ops);
				if (ret <0) {
					devm_kfree(dev, dev_context->dp);
					devm_kfree(dev,dev_context);
				}
			}
		}
		else {
			devm_kfree(dev,dev_context);
		}
	}

	return ret;
}

#if (defined(CONFIG_DRM_PANEL_DPV14_TELECHIPS) || defined(CONFIG_DRM_PANEL_DPV14_TELECHIPS_MODULE)) && \
	(defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC807X))
static int tccdrm_dp_probe_with_dp_panel(struct platform_device *plat_dev)
{
	const struct device *dev = &plat_dev->dev;
	struct drm_panel *panel = NULL;
	int ret = 0;

	ret = tccdrm_dp_parse_panel(dev, &panel);
	if (ret != -EPROBE_DEFER) {
		ret = tccdrm_dp_probe_base(plat_dev, panel);
	}

	return ret;
}
#endif

static int tccdrm_dp_probe(struct platform_device *plat_dev)
{
	int ret = 0;

#if (defined(CONFIG_DRM_PANEL_DPV14_TELECHIPS) || defined(CONFIG_DRM_PANEL_DPV14_TELECHIPS_MODULE)) && \
	(defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC807X))
	ret = tccdrm_dp_probe_with_dp_panel(plat_dev);
#else
	ret = tccdrm_dp_probe_base(plat_dev, NULL);
#endif

	return ret;
}

static int tccdrm_dp_remove(struct platform_device *plat_dev)
{
	const struct tccdrm_dp_context *dev_context = platform_get_drvdata(plat_dev);
	struct device *dev = &plat_dev->dev;

	component_del(dev, &tcc_dp_component_ops);

	#if defined(CONFIG_DRM_TELECHIPS_DP_PROC)
	if (dev_context->proc_edid != NULL) {
		proc_remove(dev_context->proc_edid);
	}
	if (dev_context->proc_hpd != NULL) {
		proc_remove(dev_context->proc_hpd);
	}
	if (dev_context->proc_dir != NULL) {
		proc_remove(dev_context->proc_dir);
	}
	#endif

	devm_kfree(dev, dev_context);

	return 0;
}

static const struct of_device_id tccdrm_dp_dt_match[] = {
	{
		.compatible = "telechips,drm-dp",
	},
	{
		/* sentinel */
	},
};
MODULE_DEVICE_TABLE(of, tccdrm_dp_dt_match);

struct platform_driver tccdrm_dp_driver = {
	.probe		= tccdrm_dp_probe,
	.remove		= tccdrm_dp_remove,
	.driver		= {
		.name	= "tccdrm-dp",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tccdrm_dp_dt_match),
	},
};
EXPORT_SYMBOL(tccdrm_dp_driver);

MODULE_DESCRIPTION("Telechips DRM Display Port v1.4");

MODULE_LICENSE("GPL");

MODULE_VERSION(__stringify(DRIVER_MAJOR) "."
			   __stringify(DRIVER_MINOR) "."
			   __stringify(DRIVER_PATCH));
