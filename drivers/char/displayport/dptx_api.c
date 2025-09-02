// SPDX-License-Identifier: GPL-2.0-or-later OR MIT
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/delay.h>
#include <linux/types.h>

#include <drm/drm_encoder.h>

#include <dptx_api.h>
#include <dptx_video.h>
#include <dptx_v14.h>
#include <dptx_reg.h>
#include <dptx_dbg.h>
#include <dptx_drm_dp_addition.h>

#if IS_ENABLED(CONFIG_DRM_TELECHIPS_DP)
#include <telechips_drm_dp_helper.h>
#endif

#if IS_ENABLED(CONFIG_DRM_PANEL_MAX968XX)
#include <panel-telechips-dpv14.h>
#endif

#if IS_ENABLED(CONFIG_TELECHIPS_FB_DPTXV14_PANEL)
/* Framebuffer PANEL */
#include <panel-dpv14.h>
#endif

#define MAX_CHECK_HPD_NUM				200

#define REG_PRINT_BUF_SIZE				1024
#define VIDEO_REG_DUMP_START_OFFSET			0x300
#define VIDEO_REG_DUMP_SIZE				0xD /* (0x34 / 4) */

#define VCP_REG_DUMP_START_OFFSET			0x200
#define VCP_REG_DUMP_SIZE				0xC /* (0x30 / 4) */

#define VCP_DPCD_DUMP_SIZE				64

#if IS_ENABLED(CONFIG_DRM_TELECHIPS_DP)
struct dptx_drm_context_t {
	bool dev_registered;
	struct drm_encoder *drm_dp_encoder;
	struct tcc_drm_dp_callback_funcs sttcc_drm_dp_callbacks;
};


static struct dptx_drm_context_t dptx_drm_context[PHY_INPUT_STREAM_MAX];
#endif

#if IS_ENABLED(CONFIG_DRM_TELECHIPS_DP) || IS_ENABLED(CONFIG_TELECHIPS_FB_DPTXV14_PANEL)
static bool dpv14_api_compare_dtb_params(const struct dptx_dtd_params *new_dtds, struct dptx_dtd_params *configured_dtds)
{
	bool same_dtds = (bool)false;

	if ((new_dtds == NULL) || (configured_dtds == NULL)) {
		/*For KCS*/
		dptx_err("Dtd pointer is null as 0x%p, 0x%p", new_dtds, configured_dtds);
	} else {
		/*
		 * remove h_sync_offset and v_sync_offset.
		 *  - Since Synopsys DP2.30a, h_sync_offset and v_sync_offset register were deprecated.
		 */
		if ((new_dtds->interlaced == configured_dtds->interlaced) &&
		    (new_dtds->h_sync_polarity == configured_dtds->h_sync_polarity) &&
		    (new_dtds->h_active == configured_dtds->h_active) &&
		    (new_dtds->h_blanking == configured_dtds->h_blanking) &&
		    (new_dtds->h_sync_pulse_width == configured_dtds->h_sync_pulse_width) &&
		    (new_dtds->v_sync_polarity == configured_dtds->v_sync_polarity) &&
		    (new_dtds->v_active == configured_dtds->v_active) &&
		    (new_dtds->v_blanking == configured_dtds->v_blanking) &&
		    (new_dtds->v_sync_pulse_width == configured_dtds->v_sync_pulse_width)) {
			/*For KCS*/
			same_dtds = (bool)true;
		}
	}

	return same_dtds;
}

static int dpv14_api_set_video_timing(int dp_id, struct dptx_dtd_params *dtd_parmas)
{
	struct dptx_dtd_params configured_dtds;
	struct dptx_video_params video_param;

	struct Dptx_Params *dptx_context = NULL;
	int ret = DPTX_RETURN_NO_ERROR;

	bool stream_enabled = (bool)false;
	bool same_timings = (bool)false;

	uint32_t vidin_tu;

	if ((dp_id >= (int)PHY_INPUT_STREAM_MAX) || (dp_id < 0)) {
		dptx_err("Invalid dp id as %d", dp_id);

		ret = -DPTX_RETURN_EINVAL;
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (dtd_parmas == NULL) {
			dptx_err("dtd_parmas ptr is NULL");
			ret = -DPTX_RETURN_EINVAL;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dptx_context = Dpv14_Tx_Get_Device_Handle();
		if (dptx_context == NULL) {
			dptx_err("Failed to get handle");
			ret = -DPTX_RETURN_ENODEV;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		/* For KCS */
		ret = Dptx_VidIn_Get_Stream_Enable(dptx_context, &stream_enabled, (u8)dp_id);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (stream_enabled) {
			ret = Dptx_VidIn_Get_Configured_Timing(dptx_context, (u8)dp_id, &configured_dtds);
			if (DPTX_RETURN_SUCCESS(ret)) {
				same_timings = dpv14_api_compare_dtb_params((const struct dptx_dtd_params *)dtd_parmas, &configured_dtds);
				if (same_timings) {
					(void)memcpy(&video_param, &dptx_context->video_params[dp_id], sizeof(video_param));
					ret = dptx_vidin_calculate_average_tu_symbols(dptx_context, &video_param);
					if (DPTX_RETURN_SUCCESS(ret)) {
						vidin_tu = dptx_vidin_get_configured_tu(dptx_context, dp_id);
						if (vidin_tu != video_param.average_bytes_per_tu) {
							dptx_err("required tu is %u but configured tu is %u",
								 video_param.average_bytes_per_tu, vidin_tu);
							same_timings = (bool)false;
						}
					}
				}
				if (same_timings) {
					dptx_notice("[Detailed timing from DRM] : ");
					dptx_notice("Video timing of DP %d was already configured --> Skip", dp_id);
					dptx_notice("		Pixel clk = %d ", (u32)dtd_parmas->uiPixel_Clock);
					dptx_notice("		%s", (dtd_parmas->interlaced == 0U) ? "Progressive" : "Interlace");
					dptx_notice("		H Active(%d), V Active(%d)", (u32)dtd_parmas->h_active, (u32)dtd_parmas->v_active);
					dptx_notice("		H Blanking(%d), V Blanking(%d)", (u32)dtd_parmas->h_blanking, (u32)dtd_parmas->v_blanking);
					dptx_notice("		H Sync offset(%d), V Sync offset(%d) ", (u32)dtd_parmas->h_sync_offset, (u32)dtd_parmas->v_sync_offset);
					dptx_notice("		H Sync plus W(%d), V Sync plus W(%d) ", (u32)dtd_parmas->h_sync_pulse_width, (u32)dtd_parmas->v_sync_pulse_width);
					dptx_notice("		H Sync Polarity(%d), V Sync Polarity(%d)", (u32)dtd_parmas->h_sync_polarity, (u32)dtd_parmas->v_sync_polarity);
				}
			}
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (!same_timings) {
			ret = Dptx_VidIn_Set_Detailed_Timing(dptx_context, (u8)dp_id, dtd_parmas);
			if (DPTX_RETURN_SUCCESS(ret)) {
				dptx_notice("[Detailed timing from DRM] : Video timing of DP %d is being aconfigured", dp_id);
				dptx_notice("		Pixel clk = %d ", (u32)dtd_parmas->uiPixel_Clock);
				dptx_notice("		%s", (dtd_parmas->interlaced == 0U) ? "Progressive" : "Interlace");
				dptx_notice("		H Active(%d), V Active(%d)", (u32)dtd_parmas->h_active, (u32)dtd_parmas->v_active);
				dptx_notice("		H Blanking(%d), V Blanking(%d)", (u32)dtd_parmas->h_blanking, (u32)dtd_parmas->v_blanking);
				dptx_notice("		H Sync offset(%d), V Sync offset(%d) ", (u32)dtd_parmas->h_sync_offset, (u32)dtd_parmas->v_sync_offset);
				dptx_notice("		H Sync plus W(%d), V Sync plus W(%d) ", (u32)dtd_parmas->h_sync_pulse_width, (u32)dtd_parmas->v_sync_pulse_width);
				dptx_notice("		H Sync Polarity(%d), V Sync Polarity(%d)", (u32)dtd_parmas->h_sync_polarity, (u32)dtd_parmas->v_sync_polarity);
			}
		}
	}
	return ret;
}

static int dpv14_api_set_video_stream_enable(int dp_id, unsigned char enable)
{
	const struct Dptx_Params *dptx_context;
	int ret = DPTX_RETURN_NO_ERROR;

	if ((dp_id >= (int)PHY_INPUT_STREAM_MAX) || (dp_id < 0)) {
		dptx_err("Invalid dp id as %d", dp_id);
		ret = -DPTX_RETURN_EINVAL;
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dptx_context = Dpv14_Tx_Get_Device_Handle();
		if (dptx_context == NULL) {
			dptx_err("Failed to get handle");
			ret = -DPTX_RETURN_ENODEV;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		ret = Dptx_VidIn_Set_Stream_Enable(dptx_context, (bool)enable, (u8)dp_id);
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			dptx_err("DP %d fails to enable stream", dp_id);
		}
	}
	//dptx_dbg("DP %d : set to %s video", dp_id, (enable == 0U) ? "disable":"enable");
	return ret;
}
#endif

#if IS_ENABLED(CONFIG_DRM_TELECHIPS_DP)
static int dpv14_api_attach_drm(u8 dp_idx)
{
	const struct dptx_drm_context_t *drm_context;
	struct drm_encoder *drm_dp_encoder = NULL;
	int ret = DPTX_RETURN_NO_ERROR;

	if (dp_idx >= (uint8_t)PHY_INPUT_STREAM_MAX) {
		dptx_err("Invalid dp id as %d", dp_idx);

		ret = -DPTX_RETURN_EINVAL;
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		drm_context = &dptx_drm_context[dp_idx];

		if (drm_context->dev_registered == (bool)false) {
			dptx_warn("Port %d is not registered", dp_idx);
			ret = -DPTX_RETURN_ENODEV;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		drm_dp_encoder = drm_context->drm_dp_encoder;
		if ((drm_dp_encoder != NULL) && (drm_context->sttcc_drm_dp_callbacks.attach != NULL)) {
			ret = drm_context->sttcc_drm_dp_callbacks.attach(drm_dp_encoder, (int)dp_idx, 0);
			if (ret != 0) {
				/* For KCS */
				dptx_warn("DP %d: error returned from drm attach callback", dp_idx);
			}
		}
	}
	return ret;
}

static int dpv14_api_detach_drm(uint8_t dp_idx)
{
	const struct dptx_drm_context_t *drm_context;
	struct drm_encoder *drm_dp_encoder = NULL;
	int ret = DPTX_RETURN_NO_ERROR;

	if (dp_idx >= (uint8_t)PHY_INPUT_STREAM_MAX) {
		dptx_err("Invalid dp id as %d", dp_idx);

		ret = -DPTX_RETURN_ENODEV;
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		drm_context = &dptx_drm_context[dp_idx];

		if (drm_context->dev_registered == (bool)false) {
			dptx_dbg("Port %d is not registered", dp_idx);
		} else {
			drm_dp_encoder = drm_context->drm_dp_encoder;
			if ((drm_dp_encoder != NULL) && (drm_context->sttcc_drm_dp_callbacks.detach != NULL)) {
				ret = drm_context->sttcc_drm_dp_callbacks.detach(drm_dp_encoder, (int)dp_idx, 0);
				if (DPTX_RETURN_ERROR(ret)) {
					/* For KCS */
					dptx_err("DP %d: error returned from drm detach callback", dp_idx);
				}
			}
		}
	}
	return ret;
}

static int dpv14_api_get_edid(int dp_id, unsigned char *edid_buf, unsigned int buf_length)
{
	const struct Dptx_Params *dptx_context;
	int ret = DPTX_RETURN_NO_ERROR;
	uint8_t *dp_edid = NULL;

	if ((dp_id >= (int)PHY_INPUT_STREAM_MAX) || (dp_id < 0)) {
		dptx_err("Invalid dp id as %d", dp_id);
		ret = -DPTX_RETURN_EINVAL;
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (edid_buf == NULL) {
			dptx_err("drm edid buffer ptr is NULL");
			ret = -DPTX_RETURN_EINVAL;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (buf_length > (uint32_t)DPTX_EDID_BUFLEN) {
			dptx_err("EDID Buffer Len. is larger than one EDID buf size(%d)",
				  DPTX_EDID_BUFLEN);
			ret = -DPTX_RETURN_EINVAL;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		(void)memset(edid_buf, 0, (size_t)buf_length);

		dptx_context = Dpv14_Tx_Get_Device_Handle();
		if (dptx_context == NULL) {
			dptx_err("Failed to get handle");
			ret = -DPTX_RETURN_ENODEV;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dp_edid = dptx_context->paucEdidBuf_Entry[dp_id];
		if (dp_edid == NULL) {
			dptx_err("DP %d EDID buffer is not available", dp_id);
			ret = -DPTX_RETURN_ENODEV;
		} else {
			ret  = Dptx_Edid_Verify_EDID(dp_edid);
			if (DPTX_RETURN_ERROR(ret)) {
				/* For KCS */
				dptx_err("DP %d EDID data is not valid", dp_id);
			} else {
				(void)memcpy(edid_buf, dp_edid, (size_t)buf_length);
			}
		}
	}
	return ret;
}

static int dpv14_api_detailed_timing_to_dtd_param(const struct dptx_detailed_timing_t *dptx_detailed_timing,
						   struct dptx_dtd_params *dtd_params)
{
	int ret = 0;

	if ((dptx_detailed_timing != NULL) && (dtd_params != NULL)) {
		dtd_params->pixel_repetition_input =
			(uint16_t)dptx_detailed_timing->pixel_repetition_input;
		dtd_params->interlaced = dptx_detailed_timing->interlaced;
		dtd_params->h_active =
			(uint16_t)(dptx_detailed_timing->h_active & 0xFFFFU);
		dtd_params->h_blanking =
			(uint16_t)(dptx_detailed_timing->h_blanking & 0xFFFFU);
		dtd_params->h_sync_offset =
			(uint16_t)(dptx_detailed_timing->h_sync_offset & 0xFFFFU);
		dtd_params->h_sync_pulse_width =
			(uint16_t)(dptx_detailed_timing->h_sync_pulse_width & 0xFFFFU);
		dtd_params->h_sync_polarity =
			(uint8_t)(dptx_detailed_timing->h_sync_polarity & 0xFFU);
		dtd_params->v_active =
			(uint16_t)(dptx_detailed_timing->v_active & 0xFFFFU);
		dtd_params->v_blanking =
			(uint16_t)(dptx_detailed_timing->v_blanking & 0xFFFFU);
		dtd_params->v_sync_offset =
			(uint16_t)(dptx_detailed_timing->v_sync_offset & 0xFFFFU);
		dtd_params->v_sync_pulse_width =
			(uint16_t)(dptx_detailed_timing->v_sync_pulse_width & 0xFFFFU);
		dtd_params->v_sync_polarity =
			(uint8_t)(dptx_detailed_timing->v_sync_polarity & 0xFFU);
		dtd_params->uiPixel_Clock = dptx_detailed_timing->pixel_clock;
		dtd_params->h_image_size = 16;
		dtd_params->v_image_size = 9;
	} else {
		ret = -EINVAL;
	}
	return ret;
}

static int dpv14_api_set_dp_video_timing(int dp_id, const struct dptx_detailed_timing_t *dptx_detailed_timing)
{
	struct dptx_dtd_params dtd_parmas;
	int ret;

	ret = dpv14_api_detailed_timing_to_dtd_param(dptx_detailed_timing, &dtd_parmas);
	if (ret == 0) {
		/* For KCS */
		ret = dpv14_api_set_video_timing(dp_id, &dtd_parmas);
	}
	return ret;
}

static int dpv14_api_set_audio_stream_enable(int dp_id, unsigned char enable)
{
	dptx_err("DP %d : Feature(%u) isn't supported", dp_id, enable);

	return -ENXIO;
}
static int dpv14_api_get_hpd_state(int dp_id, unsigned char *hpd_state)
{
	int ret = DPTX_RETURN_NO_ERROR;
	struct Dptx_Params *dptx_context;

	if ((dp_id >= (int)PHY_INPUT_STREAM_MAX) ||
	    (dp_id < (int)PHY_INPUT_STREAM_0)) {
		dptx_err("Invalid dp id as %d", dp_id);
		ret = DPTX_RETURN_EINVAL;
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (hpd_state == NULL) {
			dptx_err("drm hpd buffer ptr is NULL");
			ret = -DPTX_RETURN_EINVAL;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dptx_context = Dpv14_Tx_Get_Device_Handle();
		if (dptx_context == NULL) {
			dptx_err("Failed to get handle");
			ret = -ENODEV;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (dptx_intr_get_hotplug_status(dptx_context) != HPD_STATUS_UNPLUGGED) {
			if ((uint8_t)dp_id >= dptx_context->ucNumOfPorts) {
				dptx_dbg("DP index %d is larger than the number of ports -> set to unplugged",
					 dp_id);
				*hpd_state = (unsigned char)0u;
			} else {
				dptx_dbg("DP %d is plugged", dp_id);
				*hpd_state = (unsigned char)1u;
			}
		} else {
			dptx_dbg("DP %d is not plugged", dp_id);
			*hpd_state = (unsigned char)0u;
		}
	}

	return ret;
}
#endif

#if IS_ENABLED(CONFIG_DRM_TELECHIPS_DP)
static struct dptx_drm_helper_funcs dptx_drm_ops = {
	.get_hpd_state = dpv14_api_get_hpd_state,
	.get_edid = dpv14_api_get_edid,
	.set_video = dpv14_api_set_dp_video_timing,
	.set_enable_video = dpv14_api_set_video_stream_enable,
	.set_enable_audio = dpv14_api_set_audio_stream_enable,
};

int tcc_dp_register_drm(struct drm_encoder *encoder, const struct tcc_drm_dp_callback_funcs *callbacks)
{
	struct dptx_drm_context_t *drm_context =  NULL;
	int ret = DPTX_RETURN_NO_ERROR;
	int ret_dp_idx = -1;
	uint32_t dp_idx;

	if ((encoder == NULL) || (callbacks == NULL)) {
		if (encoder == NULL) {
			/* For KCS */
			dptx_err("drm encoder ptr is NULL");
		}
		if (callbacks == NULL) {
			/* For KCS */
			dptx_err("drm callback ptr is NULL");
		}
		ret = -DPTX_RETURN_EINVAL;
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if ((callbacks->attach == NULL) || (callbacks->detach == NULL)) {
			if (callbacks->attach == NULL) {
				/* For KCS */
				dptx_err("drm attach callback ptr is NULL");
			}
			if (callbacks->detach == NULL) {
				/* For KCS */
				dptx_err("drm detach callback ptr is NULL");
			}
			ret = -DPTX_RETURN_EINVAL;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		for (dp_idx = 0u; dp_idx < (uint32_t)PHY_INPUT_STREAM_MAX; dp_idx++) {
			drm_context = &dptx_drm_context[dp_idx];

			if (drm_context->dev_registered == (bool)false) {
				drm_context->dev_registered = (bool)true;
				break;
			}
		}

		if (dp_idx >= (uint32_t)PHY_INPUT_STREAM_MAX) {
			dptx_err("There is no more available port\n");

			ret = -DPTX_RETURN_ENODEV;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		drm_context->drm_dp_encoder = encoder;
		drm_context->sttcc_drm_dp_callbacks.attach = callbacks->attach;
		drm_context->sttcc_drm_dp_callbacks.detach = callbacks->detach;

		ret = callbacks->register_helper_funcs(drm_context->drm_dp_encoder,
						       &dptx_drm_ops);
		if (ret == 0) {
			/* For KCS */
			dptx_dbg("DP %u is registered to DRM", dp_idx);
			ret_dp_idx = (int)dp_idx;
		} else {
			dptx_dbg("DP %u fails to register to DRM", dp_idx);
		}
	}
	return ret_dp_idx;
}
EXPORT_SYMBOL(tcc_dp_register_drm);

int tcc_dp_unregister_drm(const struct drm_encoder *encoder)
{
	uint32_t dp_idx;

	if (encoder != NULL) {
		for (dp_idx = 0u; dp_idx < (uint32_t)PHY_INPUT_STREAM_MAX; dp_idx++) {
			if (dptx_drm_context[dp_idx].drm_dp_encoder == encoder) {
				dptx_drm_context[dp_idx].dev_registered = (bool)false;
				dptx_drm_context[dp_idx].drm_dp_encoder = NULL;
				dptx_drm_context[dp_idx].sttcc_drm_dp_callbacks.attach = NULL;
				dptx_drm_context[dp_idx].sttcc_drm_dp_callbacks.detach = NULL;
				break;
			}
		}
	}

	return DPTX_RETURN_NO_ERROR;
}
EXPORT_SYMBOL(tcc_dp_unregister_drm);
#endif

#if defined(CONFIG_TELECHIPS_FB_DPTXV14_PANEL) || defined(CONFIG_TELECHIPS_FB_DPTXV14_PANEL_MODULE)
static int dpv14_api_get_main_status(void)
{
	uint32_t wait_cnt;
	uint32_t reg_val, reg_addr;

	struct Dptx_Params *dev_handle;
	int32_t ret = 0;

	dev_handle = Dpv14_Tx_Get_Device_Handle();
	if (dev_handle == NULL) {
		dptx_err("Failed to get handle");

		ret = -ENODEV;
	}
	if (ret == 0) {
		reg_addr = MAIN_SUB_CORE_DP_SYNC;
		reg_val = Dptx_Reg_Readl(dev_handle, reg_addr);

		switch (reg_val) {
		case MAIN_DP_COMPLETED_INIT:
			dptx_info("DP Init. in main core was completed already");
			break;
		case MAIN_DP_DOING_INIT:
		case MAIN_DP_RESET_VALUE:
			mdelay(100);
			for (wait_cnt = 0u; wait_cnt < MAX_SYNC_WAIT_CNT; wait_cnt++) {
				reg_val = Dptx_Reg_Readl(dev_handle, reg_addr);
				if (reg_val == MAIN_DP_COMPLETED_INIT) {
					dptx_info("DP Init. in main core is completed after %ums", (wait_cnt * 100u));
					//dev_handle->main_initialized = (bool)true;
					break;
				}
				mdelay(100);
			}

			if (reg_val != MAIN_DP_COMPLETED_INIT) {
				/* For KCS */
				dptx_err("DP Init. in main core is not completed for %ums", (wait_cnt * 100u));
				ret = -ENODEV;
			}
			break;
		default:
			dptx_err("Unknown sync value as %u", reg_val);
			ret = -ENODEV;
			break;
		}
	}

	if (ret == 0) {
		if (dptx_link_get_linktraining_status(dev_handle)) {
			/* For KCS */
			dptx_info("\n****Link training was successful in Main core\n");
		} else {
			dptx_err("\n****Link training was failed in Main core\n");
		}
	}

	return ret;
}

static int32_t dpv14_api_get_dtd_from_vic(uint32_t video_code, uint32_t video_format,
					  struct dptx_dtd_params *dptx_dtd_params)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	struct dptx_dtd_params dtd;

	if (dptx_dtd_params == NULL) {
		dptx_err("Ptr. of Dtd params is NULL");
		ret = DPTX_RETURN_EINVAL;
	} else {
		ret = Dptx_VidIn_Fill_Dtd(&dtd, video_code, 60000, video_format);
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			dptx_err("Unable to map the provided VIC (%u) with Video Format (%u) to a valid Detailed Display Timing (DTD).",
				 video_code, video_format);
		}
	}

	if (DPTX_RETURN_SUCCESS(ret)) {
		/* For KCS */
		(void)memcpy(dptx_dtd_params, &dtd, sizeof(struct dptx_dtd_params));
	}

	return ret;
}

static struct dptx_fb_helper_dp_funcs dptx_fb_dp_ops = {
	.get_dtd_from_vic = dpv14_api_get_dtd_from_vic,
	.get_main_state = dpv14_api_get_main_status,
	.set_video = dpv14_api_set_video_timing,
	.set_enable_video = dpv14_api_set_video_stream_enable,
};

int dptx_register_fb_dp_ops(struct dptx_fb_helper_dp_funcs **dp_ofs)
{
	if (dp_ofs != NULL) {
		/* For KCS */
		*dp_ofs = &dptx_fb_dp_ops;
	}
	return 0;
}
EXPORT_SYMBOL(dptx_register_fb_dp_ops);
#endif

int32_t tcc_dp_identify_lcd_mux_configuration(uint32_t dp_id, uint8_t lcd_mux_index)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	struct Dptx_Params *dptx_context;
	uint8_t ucmux_id;

	if (dp_id >= (uint32_t)PHY_INPUT_STREAM_MAX) {
		dptx_err("Invalid dp id as %d", dp_id);

		ret = -DPTX_RETURN_EINVAL;
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dptx_context = Dpv14_Tx_Get_Device_Handle();
		if (dptx_context == NULL) {
			dptx_err("Failed to get handle");
			ret = -DPTX_RETURN_ENODEV;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		ret = Dptx_Cfg_Get_MuxSelect(dptx_context, (uint8_t)dp_id, &ucmux_id);
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			dptx_err("DP %d fails to get Mux configuration", dp_id);
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (lcd_mux_index != ucmux_id) {
			dptx_err("The mux index(%u) of DRM is NOT matched with mux index(%u) of DP%u configured in u-boot",
				  lcd_mux_index, ucmux_id, dp_id);
		}
	}
	return ret;
}

void Hpd_Intr_CallBabck(u8 ucDP_Index, bool bHPD_State)
{
	dptx_dbg("Callback called with DP %d, HPD %s",
		  ucDP_Index, bHPD_State ? "Plugged" : "Unplugged");

	#if defined(CONFIG_DRM_TELECHIPS) ||\
	    defined(CONFIG_DRM_TELECHIPS_MODULE)
	if (bHPD_State == (bool)HPD_STATUS_PLUGGED) {
		if (DPTX_RETURN_ERROR(dpv14_api_attach_drm(ucDP_Index))) {
			/* For KCS */
			dptx_dbg("Error from dpv14_api_attach_drm()");
		}
	} else {
		if (DPTX_RETURN_ERROR(dpv14_api_detach_drm(ucDP_Index))) {
			/* For KCS */
			dptx_dbg("Error from dpv14_api_detach_drm()");
		}
	}
	#endif
}

int Str_Resume_CallBabck(void)
{
	int ret = DPTX_RETURN_NO_ERROR;

	#if IS_ENABLED(CONFIG_DRM_PANEL_MAX968XX) || IS_ENABLED(CONFIG_MAX968XX_DP_SERDES)
	const struct Dptx_Params *dptx_context;

	dptx_dbg("Str Resume Callback is called");

	dptx_context = Dpv14_Tx_Get_Device_Handle();

	if (dptx_context == NULL) {
		dptx_err("Failed to get handle");
		ret = -DPTX_RETURN_ENODEV;
	} else {
		#if IS_ENABLED(CONFIG_DRM_PANEL_MAX968XX)
		ret = panel_max968xx_reset((bool)false, dptx_context->bPhy_Lane_Std, DPTX_PHYIF_CTRL_RATE_HBR3);
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			dptx_err("from panel_max968xx_reset()");
		}
		#elif IS_ENABLED(CONFIG_MAX968XX_DP_SERDES)
		ret = Dptx_Max968XX_Reset(dptx_context);
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			dptx_err("from Dptx_Max968XX_Reset()");
		}
		#endif
	}
	#endif

	return ret;
}

int panel_serializer_reset_callback(void)
{
	int ret = DPTX_RETURN_NO_ERROR;

	#if IS_ENABLED(CONFIG_DRM_PANEL_MAX968XX) || IS_ENABLED(CONFIG_MAX968XX_DP_SERDES)
	const struct Dptx_Params *dptx_context;

	dptx_dbg("Str Resume Callback is called");

	dptx_context = Dpv14_Tx_Get_Device_Handle();

	if (dptx_context == NULL) {
		dptx_err("Failed to get handle");
		ret = -DPTX_RETURN_ENODEV;
	} else {
		#if IS_ENABLED(CONFIG_DRM_PANEL_MAX968XX)
		ret = panel_max968xx_reset((bool)true,
					   dptx_context->bPhy_Lane_Std,
					   dptx_context->stDptxLink.ucLinkRate);
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			dptx_err("from panel_max968xx_reset()");
		}
		#elif IS_ENABLED(CONFIG_MAX968XX_DP_SERDES)
		ret = Dptx_Max968XX_Reset(dptx_context);
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			dptx_err("from Dptx_Max968XX_Reset()");
		}
		#endif
	}
	#endif
	return ret;
}

int Panel_Topology_CallBabck(uint8_t *pucNumOfPorts)
{
	int ret = DPTX_RETURN_NO_ERROR;

	#if IS_ENABLED(CONFIG_DRM_PANEL_MAX968XX) || IS_ENABLED(CONFIG_MAX968XX_DP_SERDES)
	if (pucNumOfPorts == NULL) {
		dptx_err("pucNumOfPorts == NULL");
		ret = -DPTX_RETURN_ENODEV;
	} else {
		#if IS_ENABLED(CONFIG_DRM_PANEL_MAX968XX)
		ret = panel_max968xx_get_topology(pucNumOfPorts);
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			dptx_err("Error from panel_max968xx_get_topology()");
		}
		#elif IS_ENABLED(CONFIG_MAX968XX_DP_SERDES)
		ret = Dptx_Max968XX_Get_TopologyState(pucNumOfPorts);
		if (DPTX_RETURN_ERROR(ret)) {
			/* For KCS */
			dptx_err("Error from Dptx_Max968XX_Get_TopologyState()");
		}
		#endif
	}
	#endif
	return ret;
}

int32_t Dptx_Api_Init_Params(void)
{
	#if IS_ENABLED(CONFIG_DRM_TELECHIPS)
	(void)memset(&dptx_drm_context[PHY_INPUT_STREAM_0], 0,
		     sizeof(struct dptx_drm_context_t) * (size_t)PHY_INPUT_STREAM_MAX);
	#endif

	return 0;
}

int32_t Dptx_Api_Register_Aud(void)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;

	#if !IS_ENABLED(CONFIG_DRM_TELECHIPS) && IS_ENABLED(CONFIG_TCC_DPTX_AUDIO)
	struct Dptx_Params *dptx_context = Dpv14_Tx_Get_Device_Handle();

	if (dptx_context == NULL) {
		dptx_err("Failed to get handle");

		ret = -DPTX_RETURN_ENODEV;
	} else {
		ret = Dptx_AudIn_Register(dptx_context);
	}
	#else
	dptx_info("DP audio is NOT activated in DRM");
	#endif

	return ret;
}
EXPORT_SYMBOL(Dptx_Api_Register_Aud);
