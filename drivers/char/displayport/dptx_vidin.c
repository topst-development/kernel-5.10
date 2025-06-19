// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (c) 2016 Synopsys, Inc.
 *
 * Synopsys DP TX Linux Software Driver and documentation (hereinafter,
 * "Software") is an Unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto. You are permitted to use and
 * redistribute this Software in source and binary forms, with or without
 * modification, provided that redistributions of source code must retain this
 * notice. You may not view, use, disclose, copy or distribute this file or
 * any information contained herein except pursuant to this license grant from
 * Synopsys. If you do not agree with this notice, including the disclaimer
 * below, then you are not authorized to use the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 * Modified by Telechips Inc.
 */
#include <linux/kernel.h>
#include <video/telechips/tcc_math.h>

#include <dptx_v14.h>
#include <dptx_drm_dp_addition.h>
#include <dptx_reg.h>
#include <dptx_dbg.h>

//#define ENABLE_AVGEN_AUDIO_DEBUG

static uint32_t cea861_fractional_vblank_video_codes[] = {
	5u, 6u, 7u, 10, 11u, 20u, 21u, 22u, 25u, 26u,
	39u, 40u, 44u, 45u, 46u, 50u, 51u, 54u, 55u,
	58u, 59u,
	0u /* sentinel */
};

/*
 * u-boot codes..
static uint8_t dptx_bit_field(const uint16_t data, uint8_t shift, uint8_t width)
 */
/*
 * u-boot codes..
static uint16_t dptx_concat_bits(uint8_t bhi, uint8_t ohi, uint8_t nhi, uint8_t blo, uint8_t olo, uint8_t nlo)
 */
static int32_t dptx_vidin_get_pixel_encoding_type(const struct Dptx_Params *dev_param,
						  uint8_t dp_stream_id,
						  enum VIDEO_PIXEL_ENCODING_TYPE *pixel_encoding)
{
	uint32_t colorimetry_format_mapping = 0u;
	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint32_t reg_val = 0u;

	if (dev_param == NULL) {
		dptx_err("dev_param is NULL");
		ret = -DPTX_RETURN_EINVAL;
	} else if (dp_stream_id >= (uint32_t)PHY_INPUT_STREAM_MAX) {
		dptx_err("DisplayPort stream id %u is out of range ", dp_stream_id);
		ret = -DPTX_RETURN_EINVAL;
	} else {
		reg_val = Dptx_Reg_Readl(dev_param, DPTX_VIDEO_MSA2_N(dp_stream_id));

		colorimetry_format_mapping =
			((reg_val & DPTX_VIDEO_VMSA2_COL_MASK) >> DPTX_VIDEO_VMSA2_COL_SHIFT);
		if ((colorimetry_format_mapping == 0u) ||
		    (colorimetry_format_mapping == 4u)) {
			/*For KCS*/
			*pixel_encoding = PIXEL_ENCODING_TYPE_RGB;
		} else if ((colorimetry_format_mapping == 5u) ||
			   (colorimetry_format_mapping == 13u)) {
			/*For KCS*/
			*pixel_encoding = PIXEL_ENCODING_TYPE_YCBCR422;
		} else if ((colorimetry_format_mapping == 6u) ||
			   (colorimetry_format_mapping == 14u)) {
			/*For KCS*/
			*pixel_encoding = PIXEL_ENCODING_TYPE_YCBCR444;
		} else {
			dptx_warn("Invalid encoding type %u... set default to RGB ",
				   colorimetry_format_mapping);
			*pixel_encoding = PIXEL_ENCODING_TYPE_RGB;
		}
	}

	return ret;
}

static bool dptx_vidin_check_pixelclock_match(unsigned long ref_clk,
					      unsigned long cur_clk)
{
	unsigned long scale_factor, lower_bound_value, upper_bound_value;
	int32_t ret = DPTX_RETURN_NO_ERROR;
	bool match = (bool)false;

	if (ref_clk > 10UL) {
		if (!tcc_math_check_ulong_plus_ulong(ref_clk, 10UL)) {
			/*For KCS*/
			ret = -DPTX_RETURN_EINVAL;
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			scale_factor = DIV_ROUND_UP(ref_clk, 10UL);
			if (!tcc_math_check_ulong_plus_ulong(ref_clk, scale_factor)) {
				/*For KCS*/
				ret = -DPTX_RETURN_EINVAL;
			}
		}
		if (DPTX_RETURN_SUCCESS(ret)) {
			lower_bound_value = ref_clk - scale_factor;
			upper_bound_value = ref_clk + scale_factor;

			if ((cur_clk > lower_bound_value) && (cur_clk < upper_bound_value)) {
				/*For KCS*/
				match = (bool)true;
			}
		}
	}

	return match;
}

static int32_t dptx_vidin_get_num_of_streams(const struct Dptx_Params *dev_param,
					uint8_t *active_mst_stream_count)
{
	uint8_t dp_stream_id, mst_stream_count;
	int32_t ret = DPTX_RETURN_NO_ERROR;
	bool is_stream_enabled;

	if (dev_param == NULL) {
		dptx_err("dev_param is NULL");
		ret = -DPTX_RETURN_EINVAL;
	} else {
		mst_stream_count = 0u;
		for (dp_stream_id = 0u;
		     dp_stream_id < (uint8_t)PHY_INPUT_STREAM_MAX; dp_stream_id++) {
			(void)Dptx_VidIn_Get_Stream_Enable(dev_param,
							   &is_stream_enabled,
							   dp_stream_id);
			if (is_stream_enabled) {
				if (mst_stream_count <= dp_stream_id) {
					/*For KCS*/
					mst_stream_count++;
				}
			}
		}
		if (active_mst_stream_count != NULL) {
			/*For KCS*/
			*active_mst_stream_count  = mst_stream_count;
		}
	}

	return ret;
}

static int32_t dptx_vidin_set_sampler(const struct Dptx_Params *dev_param, uint8_t dp_stream_id)
{
	const struct dptx_video_params *video_params = NULL;
	uint32_t reg_val, video_mapping = 0u;

	int32_t ret = DPTX_RETURN_NO_ERROR;

	if (dev_param == NULL) {
		dptx_err("dev_param is NULL");
		ret = -DPTX_RETURN_EINVAL;
	} else if (dp_stream_id >= (uint32_t)PHY_INPUT_STREAM_MAX) {
		dptx_err("DisplayPort stream id %u is out of range ", dp_stream_id);
		ret = -DPTX_RETURN_EINVAL;
	} else {
		video_params = &dev_param->video_params[dp_stream_id];
		if (video_params->bit_per_component != (uint8_t)PIXEL_BPC_8_BITS) {
			dptx_err("Invalid bpc = %d ", video_params->bit_per_component);
			ret = -DPTX_RETURN_EINVAL;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		reg_val = Dptx_Reg_Readl(dev_param, DPTX_VSAMPLE_CTRL_N(dp_stream_id));
		reg_val &= ~(DPTX_VSAMPLE_CTRL_VMAP_BPC_MASK | DPTX_VSAMPLE_CTRL_MULTI_PIXEL_MASK);

		switch (video_params->pixel_encoding) {
		case PIXEL_ENCODING_TYPE_RGB:
			video_mapping = 1u;
			break;
		case PIXEL_ENCODING_TYPE_YCBCR422:
			video_mapping = 9u;
			break;
		case PIXEL_ENCODING_TYPE_YCBCR444:
			video_mapping = 5u;
			break;
		default:
			dptx_err("Invalid encoding type = %u ", video_params->pixel_encoding);
			ret = -DPTX_RETURN_EINVAL;
			break;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		reg_val |= (video_mapping << DPTX_VSAMPLE_CTRL_VMAP_BPC_SHIFT);
		reg_val |= ((uint32_t)DPTX_MP_SINGLE_PIXEL << DPTX_VSAMPLE_CTRL_MULTI_PIXEL_SHIFT);

		Dptx_Reg_Writel(dev_param, DPTX_VSAMPLE_CTRL_N(dp_stream_id), reg_val);
	}

	return ret;
}

static int32_t dptx_vidin_set_config(const struct Dptx_Params *dev_param, uint8_t dp_stream_id)
{
	const struct dptx_video_params *video_params = NULL;
	const struct dptx_dtd_params *dtd_param = NULL;
	uint32_t video_code, reg_val = 0u;

	int32_t ret = DPTX_RETURN_NO_ERROR;

	if (dev_param == NULL) {
		dptx_err("dev_param is NULL");
		ret = -DPTX_RETURN_EINVAL;
	} else if (dp_stream_id >= (uint32_t)PHY_INPUT_STREAM_MAX) {
		dptx_err("DisplayPort stream id %u is out of range ", dp_stream_id);
		ret = -DPTX_RETURN_EINVAL;
	} else {
		video_params = &dev_param->video_params[dp_stream_id];
		dtd_param = &video_params->dtd_param;

		video_code = video_params->video_code;
		if (video_params->video_format_standard == VIDEO_FORMAT_CEA_861) {
			uint32_t video_code_loop;

			for (video_code_loop = 0u;
			     cea861_fractional_vblank_video_codes[video_code_loop] > 0u;
			     video_code_loop++) {
				if (video_code == cea861_fractional_vblank_video_codes[video_code_loop]) {
					reg_val |= DPTX_VIDEO_CONFIG1_IN_OSC_EN;
					break;
				}
			}
		}
		if (dtd_param->interlaced == 1u) {
			/*For KCS*/
			reg_val |= DPTX_VIDEO_CONFIG1_O_IP_EN;
		}
		reg_val |= ((uint32_t)dtd_param->h_active << DPTX_VIDEO_H_ACTIVE_SHIFT);
		reg_val |= ((uint32_t)dtd_param->h_blanking << DPTX_VIDEO_H_BLANK_SHIFT);
		Dptx_Reg_Writel(dev_param, DPTX_VIDEO_CONFIG1_N(dp_stream_id), reg_val);

		reg_val = ((uint32_t)dtd_param->v_active << DPTX_VIDEO_V_ACTIVE_SHIFT);
		reg_val |= ((uint32_t)dtd_param->v_blanking << DPTX_VIDEO_V_BLANK_SHIFT);
		Dptx_Reg_Writel(dev_param, DPTX_VIDEO_CONFIG2_N(dp_stream_id), reg_val);

		reg_val = ((uint32_t)dtd_param->h_sync_offset << DPTX_VIDEO_H_FRONT_PORCH_SHIFT);
		reg_val |= ((uint32_t)dtd_param->h_sync_pulse_width << DPTX_VIDEO_H_SYNC_WIDTH_SHIFT);
		Dptx_Reg_Writel(dev_param, DPTX_VIDEO_CONFIG3_N(dp_stream_id), reg_val);

		reg_val = ((uint32_t)dtd_param->v_sync_offset << DPTX_VIDEO_V_FRONT_PORCH_SHIFT);
		reg_val |= ((uint32_t)dtd_param->v_sync_pulse_width << DPTX_VIDEO_V_SYNC_WIDTH_SHIFT);

		Dptx_Reg_Writel(dev_param, DPTX_VIDEO_CONFIG4_N(dp_stream_id), reg_val);

		reg_val = Dptx_Reg_Readl(dev_param, DPTX_VIDEO_CONFIG5_N(dp_stream_id));

		reg_val &= (~DPTX_VIDEO_CONFIG5_TU_MASK);
		reg_val |= ((uint32_t)video_params->average_bytes_per_tu << DPTX_VIDEO_CONFIG5_TU_SHIFT);

		if (dev_param->bMultStreamTransport) {
			reg_val &= (~DPTX_VIDEO_CONFIG5_TU_FRAC_MASK_MST);
			reg_val |= ((uint32_t)video_params->average_bytes_per_tu_frac << DPTX_VIDEO_CONFIG5_TU_FRAC_SHIFT_MST);
		} else {
			reg_val &= (~DPTX_VIDEO_CONFIG5_TU_FRAC_MASK_SST);
			reg_val |= ((uint32_t)video_params->average_bytes_per_tu_frac << DPTX_VIDEO_CONFIG5_TU_FRAC_SHIFT_SST);
		}

		reg_val &= (~DPTX_VIDEO_CONFIG5_INIT_THRESHOLD_MASK);
		reg_val |= ((uint32_t)video_params->fifo_threshold << DPTX_VIDEO_CONFIG5_INIT_THRESHOLD_SHIFT);

		Dptx_Reg_Writel(dev_param, DPTX_VIDEO_CONFIG5_N(dp_stream_id), reg_val);
	}

	return ret;
}

static int32_t dptx_vidin_set_msa(const struct Dptx_Params *dev_param, uint8_t dp_stream_id)
{
	const struct dptx_video_params *video_params = NULL;
	const struct dptx_dtd_params *dtd_param = NULL;

	uint32_t reg_val, colorimetry_indicator = 0u;
	const uint32_t bits_per_component = 1u; /* 8bpc for RGB and YCbCr */

	int32_t ret = DPTX_RETURN_NO_ERROR;

	if (dev_param == NULL) {
		dptx_err("dev_param is NULL");
		ret = -DPTX_RETURN_EINVAL;
	} else if (dp_stream_id >= (uint32_t)PHY_INPUT_STREAM_MAX) {
		dptx_err("DisplayPort stream id %u is out of range ", dp_stream_id);
		ret = -DPTX_RETURN_EINVAL;
	} else {
		video_params = &dev_param->video_params[dp_stream_id];
		dtd_param = &video_params->dtd_param;

		if (dtd_param->h_blanking <= dtd_param->h_sync_offset) {
			dptx_err("detailed timinmg is not valid hblank %u and hsync offset %u",
				 dtd_param->h_blanking, dtd_param->h_sync_offset);
			ret = -DPTX_RETURN_EINVAL;
		}
		if (dtd_param->v_blanking <= dtd_param->v_sync_offset) {
			dptx_err("detailed timinmg is not valid vblank %u and vsync offset %u",
				 dtd_param->v_blanking, dtd_param->v_sync_offset);
			ret = -DPTX_RETURN_EINVAL;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		reg_val = (((uint32_t)dtd_param->h_blanking - (uint32_t)dtd_param->h_sync_offset) << DPTX_VIDEO_MSA1_H_START_SHIFT);
		reg_val |= (((uint32_t)dtd_param->v_blanking - (uint32_t)dtd_param->v_sync_offset) << DPTX_VIDEO_MSA1_V_START_SHIFT);
		Dptx_Reg_Writel(dev_param, DPTX_VIDEO_MSA1_N(dp_stream_id), reg_val);

		switch (video_params->pixel_encoding) {
		case PIXEL_ENCODING_TYPE_RGB:
			if (video_params->colorimetry_format == COLORIMETRY_SRGB) {
				/*For KCS*/
				colorimetry_indicator = 4u;
			} else {
				if (video_params->colorimetry_format == COLORIMETRY_RGB) {
					/*For KCS*/
					colorimetry_indicator = 0u;
				}
			}
			break;
		case PIXEL_ENCODING_TYPE_YCBCR422:
			if (video_params->colorimetry_format == COLORIMETRY_YCBCR_601) {
				/*For KCS*/
				colorimetry_indicator = 5u;
			} else {
				if (video_params->colorimetry_format == COLORIMETRY_YCBCR_709) {
					/*For KCS*/
					colorimetry_indicator = 13u;
				}
			}
			break;
		case PIXEL_ENCODING_TYPE_YCBCR444:
			if (video_params->colorimetry_format == COLORIMETRY_YCBCR_601) {
				/*For KCS*/
				colorimetry_indicator = 6u;
			} else {
				if (video_params->colorimetry_format == COLORIMETRY_YCBCR_709) {
					/*For KCS*/
					colorimetry_indicator = 14u;
				}
			}
			break;
		default:
			dptx_err("Invalid encoding type = 0x%x", video_params->pixel_encoding);
			ret = -DPTX_RETURN_EINVAL;
			break;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (video_params->bit_per_component != (uint8_t)PIXEL_BPC_8_BITS) {
			dptx_err("Invalid bits per component %d, it should be set 8",
				 video_params->bit_per_component);
			ret = -DPTX_RETURN_EINVAL;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		reg_val = Dptx_Reg_Readl(dev_param, DPTX_VIDEO_MSA2_N(dp_stream_id));
		reg_val &= ~(DPTX_VIDEO_VMSA2_BPC_MASK | DPTX_VIDEO_VMSA2_COL_MASK);
		reg_val |= (colorimetry_indicator << DPTX_VIDEO_VMSA2_COL_SHIFT);
		reg_val |= (bits_per_component << DPTX_VIDEO_VMSA2_BPC_SHIFT);

		Dptx_Reg_Writel(dev_param, DPTX_VIDEO_MSA2_N(dp_stream_id), reg_val);

		reg_val = Dptx_Reg_Readl(dev_param, DPTX_VIDEO_MSA3_N(dp_stream_id));
		reg_val &= ~DPTX_VIDEO_VMSA3_PIX_ENC_MASK;
		Dptx_Reg_Writel(dev_param, DPTX_VIDEO_MSA3_N(dp_stream_id), reg_val);
	}

	return ret;
}

static int32_t dptx_vidin_set_hblank_interval(const struct Dptx_Params *dev_param, uint8_t dp_stream_id)
{
	const struct dptx_video_params *video_params = NULL;
	uint32_t reg_val, reg_tmp, link_clocks;
	const struct dptx_dtd_params *dtd_param;

	int32_t ret = DPTX_RETURN_NO_ERROR;

	if (dev_param == NULL) {
		dptx_err("dev_param is NULL");
		ret = -DPTX_RETURN_EINVAL;
	} else if (dp_stream_id >= (uint32_t)PHY_INPUT_STREAM_MAX) {
		dptx_err("DisplayPort stream id %u is out of range ", dp_stream_id);
		ret = -DPTX_RETURN_EINVAL;
	} else {
		video_params = &dev_param->video_params[dp_stream_id];
		dtd_param = &video_params->dtd_param;

		switch (dev_param->stDptxLink.ucLinkRate) {
		case DPTX_PHYIF_CTRL_RATE_RBR:
			link_clocks = 40500u;
			break;
		case DPTX_PHYIF_CTRL_RATE_HBR:
			link_clocks = 67500u;
			break;
		case DPTX_PHYIF_CTRL_RATE_HBR2:
			link_clocks = 135000u;
			break;
		case DPTX_PHYIF_CTRL_RATE_HBR3:
			link_clocks = 202500u;
			break;
		default:
			dptx_err("Invalid rate 0x%x", dev_param->stDptxLink.ucLinkRate);
			ret = -DPTX_RETURN_EINVAL;
			break;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (dev_param->bMultStreamTransport) {
			/*
			 * Resolutions with an HBLANK value smaller than 16 are
			 * rare in typical displays. It is generally possible only
			 * in resolutions smaller than 640x480.
			 */
			if (dtd_param->h_blanking < 16u) {
				dptx_err("hblank %u smaller then 16", dtd_param->h_blanking);
				ret = -DPTX_RETURN_EINVAL;
			}
			if (dtd_param->h_blanking > 4096u) {
				/*
				 * 65536 (U16MAX) / 16 * 64 * 202500(HBR3) = over 32bits
				 * 4096 / 16 * 64 * 202500(HBR3)
				 * hblank limitation is 4096
				 */
				dptx_err("hblank %u larger then 4096", dtd_param->h_blanking);
				ret = -DPTX_RETURN_EINVAL;
			}
			if (video_params->average_bytes_per_tu > (uint8_t)DPTX_MAX_LINK_SYMBOLS) {
				dptx_err("average bytes per tu %u out of range", video_params->average_bytes_per_tu);
				ret = -DPTX_RETURN_EINVAL;
			}
			if (DPTX_RETURN_SUCCESS(ret)) {
				reg_tmp = ((uint32_t)dtd_param->h_blanking / 16u);
				reg_val = (uint32_t)video_params->average_bytes_per_tu * link_clocks;
				reg_tmp *= reg_val;
				reg_val = reg_tmp / dtd_param->uiPixel_Clock;
			}
		} else {
			reg_tmp = dtd_param->h_blanking * link_clocks;
			reg_val = reg_tmp / dtd_param->uiPixel_Clock;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		reg_val |= ((uint32_t)DPTX_VIDEO_HBLANK_INTERVAL_ENABLE << DPTX_VIDEO_HBLANK_INTERVAL_SHIFT);
		Dptx_Reg_Writel(dev_param, DPTX_VIDEO_HBLANK_INTERVAL_N(dp_stream_id), reg_val);
	}

	return ret;
}

static int32_t dptx_vidin_config_video_input(const struct Dptx_Params *dev_param, uint8_t dp_stream_id)
{
	const struct dptx_video_params *video_params = NULL;
	const struct dptx_dtd_params *dtd_param;

	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint32_t reg_val;

	if (dev_param == NULL) {
		dptx_err("dev_param is NULL");
		ret = -DPTX_RETURN_EINVAL;
	} else if (dp_stream_id >= (uint32_t)PHY_INPUT_STREAM_MAX) {
		dptx_err("DisplayPort stream id %u is out of range ", dp_stream_id);
		ret = -DPTX_RETURN_EINVAL;
	} else {
		video_params = &dev_param->video_params[dp_stream_id];
		dtd_param = &video_params->dtd_param;

		ret = dptx_vidin_set_sampler(dev_param, dp_stream_id);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		reg_val = 0u;

		if (dtd_param->h_sync_polarity == 1u) {
			/*For KCS*/
			reg_val |= DPTX_POL_CTRL_H_SYNC_POL_EN;
		}
		if (dtd_param->v_sync_polarity == 1u) {
			/*For KCS*/
			reg_val |= DPTX_POL_CTRL_V_SYNC_POL_EN;
		}
		Dptx_Reg_Writel(dev_param, DPTX_VSAMPLE_POLARITY_CTRL_N(dp_stream_id), reg_val);

		ret = dptx_vidin_set_config(dev_param, dp_stream_id);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		/*For KCS*/
		ret = dptx_vidin_set_msa(dev_param, dp_stream_id);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		/*For KCS*/
		ret = dptx_vidin_set_hblank_interval(dev_param, dp_stream_id);
	}
	return ret;
}

/*
 * u-boot
int32_t Dptx_Vidin_Init(struct Dptx_Params *dev_param, uint8_t pixel_encoding)
 */


/*
 * u-boot
int32_t Dptx_Vidin_Set_Video_PPClk(struct Dptx_Params *dev_param,
				   uint8_t num_of_streams,
				   const uint32_t pixel_clocks[PHY_INPUT_STREAM_MAX])
 */
static int32_t dptx_vidin_get_vic_from_dtb(struct Dptx_Params *dev_param, uint8_t dp_stream_id)
{
	struct dptx_dtd_params configured_dtd_params = {0, };
	struct dptx_dtd_params new_dtd_params = {0, };
	struct dptx_video_params *video_params = NULL;

	int32_t ret = DPTX_RETURN_NO_ERROR;

	uint32_t vic = DP_CUSTOM_MAX_DTD_VIC;
	uint8_t video_format;

	if (dev_param == NULL) {
		dptx_err("dev_param is NULL");
		ret = -DPTX_RETURN_EINVAL;
	} else if (dp_stream_id >= (uint32_t)PHY_INPUT_STREAM_MAX) {
		dptx_err("DisplayPort stream id %u is out of range ", dp_stream_id);
		ret = -DPTX_RETURN_EINVAL;
	} else {
		ret = Dptx_VidIn_Get_Configured_Timing(dev_param, dp_stream_id, &configured_dtd_params);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		video_params = &dev_param->video_params[dp_stream_id];

		ret = -DPTX_RETURN_EINVAL;

		for (video_format = (uint8_t)VIDEO_FORMAT_CEA_861;
		     video_format < (uint8_t)VIDEO_FORMAT_MAX; video_format++) {
			for (vic = 0; vic < (uint32_t)DP_CUSTOM_MAX_DTD_VIC;  vic++) {
				if (Dptx_VidIn_Fill_Dtd(&new_dtd_params,
							vic,
							60000u,
							(enum VIDEO_FORMAT_STANDARD_TYPE)video_format) != DPTX_RETURN_NO_ERROR) {
					continue;
				}
				/*
				 * remove h_sync_offset and v_sync_offset.
				 *  - Since Synopsys DP2.30a, h_sync_offset and v_sync_offset register were deprecated.
				 */
				if ((new_dtd_params.interlaced != configured_dtd_params.interlaced) ||
					(new_dtd_params.h_sync_polarity != configured_dtd_params.h_sync_polarity) ||
					(new_dtd_params.h_active != configured_dtd_params.h_active) ||
					(new_dtd_params.h_blanking != configured_dtd_params.h_blanking) ||
					(new_dtd_params.h_sync_pulse_width != configured_dtd_params.h_sync_pulse_width) ||
					(new_dtd_params.v_sync_polarity != configured_dtd_params.v_sync_polarity) ||
					(new_dtd_params.v_active != configured_dtd_params.v_active) ||
					(new_dtd_params.v_blanking != configured_dtd_params.v_blanking) ||
					(new_dtd_params.v_sync_pulse_width != configured_dtd_params.v_sync_pulse_width)) {
					continue;
				}

				#if IS_ENABLED(CONFIG_DUMP_VIC_FROM_DTB)
				pr_err(" %d  - %d\n", new_dtd_params.h_sync_polarity, configured_dtd_params.h_sync_polarity);
				pr_err(" %d  - %d\n", new_dtd_params.h_active, configured_dtd_params.h_active);
				pr_err(" %d  - %d\n", new_dtd_params.h_blanking, configured_dtd_params.h_blanking);
				pr_err(" %d  - %d\n", new_dtd_params.h_sync_pulse_width, configured_dtd_params.h_sync_pulse_width);
				pr_err(" %d  - %d\n", new_dtd_params.v_sync_polarity, configured_dtd_params.v_sync_polarity);
				pr_err(" %d  - %d\n", new_dtd_params.v_active, configured_dtd_params.v_active);
				pr_err(" %d  - %d\n", new_dtd_params.v_blanking, configured_dtd_params.v_blanking);
				pr_err(" %d  - %d\n", new_dtd_params.v_sync_pulse_width, configured_dtd_params.v_sync_pulse_width);
				pr_err(" new_pclk = %u  - %u\n", new_dtd_params.uiPixel_Clock, video_params->pclks[stream_idx]);
				#endif

				if (dptx_vidin_check_pixelclock_match(new_dtd_params.uiPixel_Clock,
								      video_params->pixel_clock)) {
					video_params->video_code = vic;
					video_params->video_format_standard = video_format;

					dptx_info("DisplayPort stream[%u] enabled with VIC[%u]",
						  dp_stream_id, video_params->video_code);

					(void)memcpy(&video_params->dtd_param, &new_dtd_params,
						     sizeof(struct dptx_dtd_params));
					ret = DPTX_RETURN_NO_ERROR;
					break;
				}
			}
			if (DPTX_RETURN_SUCCESS(ret)) {
				/*For KCS*/
				break;
			}
		}
	}
	if (DPTX_RETURN_ERROR(ret)) {
		/*For KCS*/
		dptx_err("DP[%u] not found VIC from dtd", dp_stream_id);
	}
	return ret;
}

int32_t Dptx_VidIn_Init_Params(struct Dptx_Params *dev_param,
			       const uint32_t pixel_clocks[PHY_INPUT_STREAM_MAX])
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint8_t num_of_ports, dp_stream_id;

	if (dev_param == NULL) {
		dptx_err("dev_param is NULL");
		ret = -DPTX_RETURN_EINVAL;
	} else {
		ret = dptx_vidin_get_num_of_streams(dev_param, &num_of_ports);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (num_of_ports == 0u) {
			dptx_warn("DP was not enabled");
			ret = -DPTX_RETURN_ENODEV;
		} else if (num_of_ports > (uint8_t)PHY_INPUT_STREAM_MAX) {
			dptx_err("Number of ports are out of range %d", num_of_ports);
			ret = -DPTX_RETURN_ENODEV;
		} else {
			dptx_info("Readback %u streams but hw_confg %u streams\n",
				  num_of_ports, dev_param->hw_config.num_of_ports);

			if (num_of_ports > dev_param->hw_config.num_of_ports) {
				uint32_t valid_stream_id = num_of_ports - 1u;

				for (dp_stream_id = valid_stream_id;
				     dp_stream_id < (uint8_t)dev_param->hw_config.num_of_ports;
				     dp_stream_id++) {
					dptx_info("DP %u: Disable video streams\n", dp_stream_id);
					(void)Dptx_VidIn_Set_Stream_Enable(dev_param, false, dp_stream_id);
				}
				if (dev_param->hw_config.num_of_ports == 1u) {
					dptx_info("DP %u: Disable video streams\n", PHY_INPUT_STREAM_0);
					/* Disable video stream as the topology has changed from MST to SST */
					(void)Dptx_VidIn_Set_Stream_Enable(dev_param, false, PHY_INPUT_STREAM_0);
				}
			}
			for (dp_stream_id = 0u;
			     dp_stream_id < (uint8_t)dev_param->hw_config.num_of_ports;
			     dp_stream_id++) {
				dev_param->video_params[dp_stream_id].bit_per_component = (uint8_t)PIXEL_BPC_8_BITS;
				dev_param->video_params[dp_stream_id].video_format_standard = VIDEO_FORMAT_CEA_861;
				dev_param->video_params[dp_stream_id].video_refresh_rate = (u32)VIDEO_REFRESH_RATE_60_00HZ;

				if (dp_stream_id >= dev_param->ucNumOfPorts) {
					/*For KCS*/
					continue;
				}
				ret = dptx_vidin_get_pixel_encoding_type(dev_param,
									 dp_stream_id,
									 &dev_param->video_params[dp_stream_id].pixel_encoding);
				if (dev_param->video_params[dp_stream_id].pixel_encoding == PIXEL_ENCODING_TYPE_RGB) {
					/*For KCS*/
					dev_param->video_params[dp_stream_id].colorimetry_format = COLORIMETRY_RGB;
				} else {
					/*For KCS*/
					dev_param->video_params[dp_stream_id].colorimetry_format = COLORIMETRY_YCBCR_709;
				}
				dev_param->video_params[dp_stream_id].pixel_clock = pixel_clocks[dp_stream_id];
				ret = dptx_vidin_get_vic_from_dtb(dev_param, dp_stream_id);
				if (DPTX_RETURN_ERROR(ret)) {
					/*For KCS*/
					break;
				}
			}
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		for (dp_stream_id = 0u; dp_stream_id < dev_param->ucNumOfPorts;
		     dp_stream_id++) {
			dptx_info(" DP[%u] Pixel encoding = %s, video_code = %u, pixel_clock = %uKhz",
				  dp_stream_id,
				  (dev_param->video_params[dp_stream_id].pixel_encoding == PIXEL_ENCODING_TYPE_RGB) ? "RGB" :
				  (dev_param->video_params[dp_stream_id].pixel_encoding == PIXEL_ENCODING_TYPE_YCBCR422) ? "YCbCr422" : "YCbCr444",
				  dev_param->video_params[dp_stream_id].video_code,
				  dev_param->video_params[dp_stream_id].pixel_clock);
		}
	}

	return DPTX_RETURN_NO_ERROR;
}

/*
 * u-boot
int32_t Dptx_Vidin_Set_Video_TimingChange_FromVIC(struct Dptx_Params *dev_param,
						  uint32_t video_code, uint8_t dp_stream_id)
 */

int32_t Dptx_VidIn_Set_Detailed_Timing(struct Dptx_Params *dev_param,
				       uint8_t dp_stream_id,
				       const struct dptx_dtd_params *dtd_param)
{
	struct dptx_video_params *video_params = NULL;
	int32_t ret = DPTX_RETURN_NO_ERROR;

	if (dev_param == NULL) {
		dptx_err("dev_param is NULL");
		ret = -DPTX_RETURN_EINVAL;
	} else if (dtd_param == NULL) {
		dptx_err("dtd_param is NULL");
		ret = -DPTX_RETURN_EINVAL;
	} else if (dp_stream_id >= (uint32_t)PHY_INPUT_STREAM_MAX) {
		dptx_err("DisplayPort stream id %u is out of range ", dp_stream_id);
		ret = -DPTX_RETURN_EINVAL;
	} else {
		if ((!dev_param->bMultStreamTransport) && (dp_stream_id >= (uint8_t)PHY_INPUT_STREAM_1)) {
			dptx_err("stream index %u is out of range with in SST", dp_stream_id);
			ret = -DPTX_RETURN_EINVAL;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		video_params = &dev_param->video_params[dp_stream_id];

		(void)memcpy(&video_params->dtd_param,
			     dtd_param, sizeof(struct dptx_dtd_params));

		video_params->pixel_clock = video_params->dtd_param.uiPixel_Clock;

		dptx_debug("Stream %d :  ", dp_stream_id);
		dptx_debug("Refresh rate: %d", video_params->video_refresh_rate);
		dptx_debug("DTD format: %s",
			   (video_params->video_format_standard ==
			    (uint8_t)VIDEO_FORMAT_CEA_861) ? "CEA_861":"Others");
		dptx_debug("Pixel Clk: %d",
			   video_params->pixel_clock);

		ret = dptx_vidin_calculate_average_tu_symbols(dev_param, video_params);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		/*For KCS*/
		ret = Dptx_VidIn_Set_Stream_Enable(dev_param, (bool)false, dp_stream_id);
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		/*For KCS*/
		ret = dptx_vidin_config_video_input(dev_param, (uint8_t)dp_stream_id);
	}
	return ret;
}

int32_t Dptx_VidIn_Set_Stream_Enable(const struct Dptx_Params *dev_param,
				     bool enable_stream, uint8_t dp_stream_id)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint32_t reg_val;

	if (dev_param == NULL) {
		dptx_err("dev_param is NULL");
		ret = -DPTX_RETURN_EINVAL;
	} else if (dp_stream_id >= (uint32_t)PHY_INPUT_STREAM_MAX) {
		dptx_err("DisplayPort stream id %u is out of range ", dp_stream_id);
		ret = -DPTX_RETURN_EINVAL;
	} else {
		reg_val = Dptx_Reg_Readl(dev_param, DPTX_VSAMPLE_CTRL_N(dp_stream_id));

		if (enable_stream) {
			/*For KCS*/
			reg_val |= DPTX_VSAMPLE_CTRL_STREAM_EN;
		} else {
			/*For KCS*/
			reg_val &= ~DPTX_VSAMPLE_CTRL_STREAM_EN;
		}
		Dptx_Reg_Writel(dev_param, DPTX_VSAMPLE_CTRL_N(dp_stream_id), reg_val);
	}

	return ret;
}

int32_t Dptx_VidIn_Get_Stream_Enable(const struct Dptx_Params *dev_param,
				     bool *is_stream_enabled, uint8_t dp_stream_id)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint32_t reg_val;

	if (dev_param == NULL) {
		dptx_err("dev_param is NULL");
		ret = -DPTX_RETURN_EINVAL;
	} else if (dp_stream_id >= (uint32_t)PHY_INPUT_STREAM_MAX) {
		dptx_err("DisplayPort stream id %u is out of range ", dp_stream_id);
		ret = -DPTX_RETURN_EINVAL;
	} else {
		reg_val = Dptx_Reg_Readl(dev_param, DPTX_VSAMPLE_CTRL_N(dp_stream_id));

		if (is_stream_enabled != NULL) {
			if ((reg_val & DPTX_VSAMPLE_CTRL_STREAM_EN) != 0u) {
				/*For KCS*/
				*is_stream_enabled = (bool)true;
			} else {
				/*For KCS*/
				*is_stream_enabled = (bool)false;
			}
		}
	}
	return ret;
}

int32_t Dptx_VidIn_Set_Timing(const struct Dptx_Params *dev_param, uint8_t dp_stream_id)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;

	if (dev_param == NULL) {
		dptx_err("dev_param is NULL");
		ret = -DPTX_RETURN_EINVAL;
	} else if (dp_stream_id >= (uint32_t)PHY_INPUT_STREAM_MAX) {
		dptx_err("DisplayPort stream id %u is out of range ", dp_stream_id);
		ret = -DPTX_RETURN_EINVAL;
	} else {
		ret = Dptx_VidIn_Set_Stream_Enable(dev_param, false, dp_stream_id);
		if (DPTX_RETURN_SUCCESS(ret)) {
			/*For KCS*/
			ret = dptx_vidin_config_video_input(dev_param, (uint8_t)dp_stream_id);
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		/*For KCS*/
		ret = Dptx_VidIn_Set_Stream_Enable(dev_param, true, dp_stream_id);
	}

	return ret;
}

int32_t Dptx_VidIn_Get_Configured_Timing(const struct Dptx_Params *dev_param,
					 uint8_t dp_stream_id,
					 struct dptx_dtd_params *dtd_param)
{
	uint32_t reg_video_config1, reg_video_config2, reg_video_config3,
		 reg_video_config4, reg_video_polarity;
	uint32_t reg_val;
	int32_t	ret = DPTX_RETURN_NO_ERROR;

	if (dev_param == NULL) {
		dptx_err("dev_param is NULL");
		ret = -DPTX_RETURN_EINVAL;
	} else if (dtd_param == NULL) {
		dptx_err("dtd_param is NULL");
		ret = -DPTX_RETURN_EINVAL;
	} else if (dp_stream_id >= (uint32_t)PHY_INPUT_STREAM_MAX) {
		dptx_err("DisplayPort stream id %u is out of range ", dp_stream_id);
		ret = -DPTX_RETURN_EINVAL;
	} else {
		(void)memset(dtd_param, 0, sizeof(struct dptx_dtd_params));

		reg_video_polarity = Dptx_Reg_Readl(dev_param, DPTX_VSAMPLE_POLARITY_CTRL_N(dp_stream_id));
		reg_video_config1 = Dptx_Reg_Readl(dev_param, DPTX_VIDEO_CONFIG1_N(dp_stream_id));
		reg_video_config2 = Dptx_Reg_Readl(dev_param, DPTX_VIDEO_CONFIG2_N(dp_stream_id));
		reg_video_config3 = Dptx_Reg_Readl(dev_param, DPTX_VIDEO_CONFIG3_N(dp_stream_id));
		reg_video_config4 = Dptx_Reg_Readl(dev_param, DPTX_VIDEO_CONFIG4_N(dp_stream_id));

		if ((reg_video_polarity &  DPTX_POL_CTRL_H_SYNC_POL_EN) != 0u) {
			/*For KCS*/
			dtd_param->h_sync_polarity = 1u;
		}
		if ((reg_video_polarity &  DPTX_POL_CTRL_V_SYNC_POL_EN) != 0u) {
			/*For KCS*/
			dtd_param->v_sync_polarity = 1u;

		}
		if ((reg_video_config1 &  DPTX_VIDEO_CONFIG1_O_IP_EN) != 0u) {
			/*For KCS*/
			dtd_param->interlaced = 1u;
		}
		reg_val = (reg_video_config1 & DPTX_VIDEO_H_ACTIVE_MASK) >> DPTX_VIDEO_H_ACTIVE_SHIFT;
		if (reg_val > 0xffffu) {
			dptx_err("h_active %u is out of range ", reg_val);
			ret = -DPTX_RETURN_EINVAL;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dtd_param->h_active = (uint16_t)reg_val;

		reg_val = (reg_video_config1 & DPTX_VIDEO_H_BLANK_MASK) >> DPTX_VIDEO_H_BLANK_SHIFT;
		if (reg_val > 0xffffu) {
			dptx_err("h_active %u is out of range ", reg_val);
			ret = -DPTX_RETURN_EINVAL;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dtd_param->h_blanking = (uint16_t)reg_val;

		reg_val = (reg_video_config2 & DPTX_VIDEO_V_ACTIVE_MASK) >> DPTX_VIDEO_V_ACTIVE_SHIFT;
		if (reg_val > 0xffffu) {
			dptx_err("v_active %u is out of range ", reg_val);
			ret = -DPTX_RETURN_EINVAL;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dtd_param->v_active = (uint16_t)reg_val;

		reg_val = (reg_video_config2 & DPTX_VIDEO_V_BLANK_MASK) >> DPTX_VIDEO_V_BLANK_SHIFT;
		if (reg_val > 0xffffu) {
			dptx_err("v_blanking %u is out of range ", reg_val);
			ret = -DPTX_RETURN_EINVAL;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dtd_param->v_blanking = (uint16_t)reg_val;

		reg_val = (reg_video_config3 & DPTX_VIDEO_H_FRONTE_PORCH_MASK) >> DPTX_VIDEO_H_FRONT_PORCH_SHIFT;
		if (reg_val > 0xffffu) {
			dptx_err("h_sync_offset %u is out of range ", reg_val);
			ret = -DPTX_RETURN_EINVAL;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dtd_param->h_sync_offset = (uint16_t)reg_val;

		reg_val = (reg_video_config3 & DPTX_VIDEO_H_SYNC_WIDTH_MASK) >> DPTX_VIDEO_H_SYNC_WIDTH_SHIFT;
		if (reg_val > 0xffffu) {
			dptx_err("h_sync_pulse_width %u is out of range ", reg_val);
			ret = -DPTX_RETURN_EINVAL;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dtd_param->h_sync_pulse_width = (uint16_t)reg_val;

		reg_val = (reg_video_config4 & DPTX_VIDEO_V_FRONTE_PORCH_MASK) >> DPTX_VIDEO_V_FRONT_PORCH_SHIFT;
		if (reg_val > 0xffffu) {
			dptx_err("v_sync_offset %u is out of range ", reg_val);
			ret = -DPTX_RETURN_EINVAL;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dtd_param->v_sync_offset = (uint16_t)reg_val;

		reg_val = (reg_video_config4 & DPTX_VIDEO_V_SYNC_WIDTH_MASK) >> DPTX_VIDEO_V_SYNC_WIDTH_SHIFT;
		if (reg_val > 0xffffu) {
			dptx_err("v_sync_pulse_width %u is out of range ", reg_val);
			ret = -DPTX_RETURN_EINVAL;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		/*For KCS*/
		dtd_param->v_sync_pulse_width = (uint16_t)reg_val;
	}

	return ret;
}

uint32_t dptx_vidin_get_configured_tu(const struct Dptx_Params *dev_param,
				      uint8_t dp_stream_id)
{
	uint32_t reg_val = 0u;

	if (dev_param == NULL) {
		dptx_err("dev_param is NULL");
	} else if (dp_stream_id >= (uint32_t)PHY_INPUT_STREAM_MAX) {
		dptx_err("DisplayPort stream id %u is out of range ", dp_stream_id);
	} else {
		reg_val = Dptx_Reg_Readl(dev_param, DPTX_VIDEO_CONFIG5_N(dp_stream_id));

		reg_val &= DPTX_VIDEO_CONFIG5_TU_MASK;
		reg_val >>= DPTX_VIDEO_CONFIG5_TU_SHIFT;
	}
	return reg_val;
}

int32_t dptx_vidin_calculate_average_tu_symbols(struct Dptx_Params *dev_param,
						struct dptx_video_params *video_param)
{
	uint32_t average_valid_symbols_per_tu_k, average_valid_symbols_per_tu;
	uint32_t link_symbol_clock_mhz, bits_per_pixel;
	uint32_t transfer_unit_fraction, transfer_unit_fraction_tmp;

	const struct dptx_dtd_params *dtd_param = NULL;

	int32_t ret = DPTX_RETURN_NO_ERROR;

	if (video_param == NULL) {
		dptx_err("video_param is NULL");
		ret = -DPTX_RETURN_EINVAL;
	} else {
		dtd_param = &video_param->dtd_param;
		if (dev_param->stDptxLink.ucNumOfLanes > (uint8_t)DPTX_MAX_LINK_LANES) {
			dptx_err("num_of_lane %u is out of range", dev_param->stDptxLink.ucNumOfLanes);
			ret = -DPTX_RETURN_EINVAL;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		switch (dev_param->stDptxLink.ucLinkRate) {
		case DPTX_PHYIF_CTRL_RATE_RBR:
			/* 162MHz for 1.62Gbps/lane */
			link_symbol_clock_mhz = 162;
			break;
		case DPTX_PHYIF_CTRL_RATE_HBR:
			/* 270MHz for 2.7Gbps/lane */
			link_symbol_clock_mhz = 270;
			break;
		case DPTX_PHYIF_CTRL_RATE_HBR2:
			/* 540MHz for 5.4Gbps/lane */
			link_symbol_clock_mhz = 540;
			break;
		case DPTX_PHYIF_CTRL_RATE_HBR3:
			/* 810MHz for 8.1Gbps/lane */
			link_symbol_clock_mhz = 810;
			break;
		default:
			dptx_err("Invalid rate param = %u", dev_param->stDptxLink.ucLinkRate);
			ret = -DPTX_RETURN_EINVAL;
			break;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (video_param->bit_per_component == (uint8_t)PIXEL_BPC_8_BITS) {
			if ((video_param->pixel_encoding == PIXEL_ENCODING_TYPE_RGB) ||
			    (video_param->pixel_encoding == PIXEL_ENCODING_TYPE_YCBCR444)) {
				/*For KCS*/
				bits_per_pixel  = ((uint32_t)PIXEL_BPC_8_BITS * (uint32_t)VIDEO_LINK_BPP_RGB_YCbCr444);
			} else if (video_param->pixel_encoding == PIXEL_ENCODING_TYPE_YCBCR422) {
				/*For KCS*/
				bits_per_pixel = ((uint32_t)PIXEL_BPC_8_BITS * (uint32_t)VIDEO_LINK_BPP_YCbCr422);
			} else {
				dptx_err("Invalid encoding type(%u)", video_param->pixel_encoding);
				ret = -DPTX_RETURN_EINVAL;
			}
		} else {
			dptx_err("Invalid bits per component(%d)", video_param->bit_per_component);
			ret = -DPTX_RETURN_EINVAL;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		/*
		 * pakced_data_rate_over_lanes = bits_or_pixel * pixel_clocks /
		 *                               dev_param->stDptxLink.ucNumOfLanes;
		 * symbols_of_pakced_data_rate_over_lane = pakced_data_rate_over_lanes / 8u;
		 * Average_valid_symbols_per_tu = symbols_of_pakced_data_rate_over_lane /
		 *                                link_symbol_clock * TU size ;
		 * Note: TU is 64
		 *
		 * Average_valid_symbols_per_tu = bits_or_pixel * pixel_clocks /
		 *                                dev_param->stDptxLink.ucNumOfLanes /
		 *                                8u / link_symbol_clock * 64u;
		 * Average_valid_symbols_per_tu = ((bits_or_pixel * pixel_clocks) /
		 *                                 (dev_param->stDptxLink.ucNumOfLanes *
		 *                                 8u * link_symbol_clock)) * 64u;
		 * ->
		 * Average_valid_symbols_per_tu = ((8u * bits_or_pixel * pixel_clocks) /
		 *                                 (dev_param->stDptxLink.ucNumOfLanes *
		 *                                 link_symbol_clock))
		 */
		if (dtd_param->uiPixel_Clock > DPTX_PIXEL_CLOCK_KHZ_MAX) {
			/* 600MHz is maximum PCLK for DDI BUS */
			dptx_err("pixel_clock_khz %uKHz is out of range", dtd_param->uiPixel_Clock);
			ret = -DPTX_RETURN_EINVAL;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		/* 8 * 24 * 600,000 fits within the range of 32-bit values. */
		average_valid_symbols_per_tu_k = (8u * bits_per_pixel * dtd_param->uiPixel_Clock) /
						 (dev_param->stDptxLink.ucNumOfLanes * link_symbol_clock_mhz);
		average_valid_symbols_per_tu = average_valid_symbols_per_tu_k / 1000u;
		if (average_valid_symbols_per_tu > (uint32_t)DPTX_MAX_LINK_SYMBOLS) {
			dptx_err("average_valid_symbols_per_tu is %d out of range",
				 average_valid_symbols_per_tu);
			ret = -DPTX_RETURN_ENODEV;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (!dev_param->bMultStreamTransport) {
			transfer_unit_fraction = (average_valid_symbols_per_tu_k / 100u);
			transfer_unit_fraction_tmp = (average_valid_symbols_per_tu * 10u);

			if (transfer_unit_fraction < transfer_unit_fraction_tmp) {
				dptx_err("transfer_unit_fraction is not valid");
				ret = -DPTX_RETURN_EINVAL;
			} else {
				transfer_unit_fraction -= transfer_unit_fraction_tmp;
			}
		} else  {
			/*
			 * This is the fractional portion of the value calculated
			 * in the average bytes per TU. The first decimal value
			 * in the fractional part should be programmed here.
			 * For example, if the average bytes per TU is 24.34, then
			 * this field should be programmed to a value of 3.
			 * In SST mode only bits 19:16 is used. In MST mode, this
			 * field should be programmed with the fractional value
			 * of the VC payload size calculated by the Payload bandwidth
			 * manager. as specified in Section 2.6.4.3 of DisplayPort 1.4
			 * Spec. The fractional value calculated times 64 should
			 * be programmed in this field.
			 * Note: The frac function returns the fractional part
			 * of a real number. For example, frac(3.14) yields 0.14.
			 * This function is useful for extracting the decimal
			 * portion of a number, excluding its integer part.
			 *
			 * Peak_stream_bandwidth:
			 *  pixel_clocks * bits_or_pixel
			 * Link Bandwidth:
			 *  dev_param->stDptxLink.ucNumOfLanes * link_symbol_clock
			 *
			 * Average_bytes_per_tu_frac = frac((peak_stream_bandwidth / link_bandwidth) * 64) * 64
			 */
			transfer_unit_fraction = average_valid_symbols_per_tu_k;
			transfer_unit_fraction_tmp = (average_valid_symbols_per_tu * 1000u);

			if (transfer_unit_fraction < transfer_unit_fraction_tmp) {
				dptx_err("transfer_unit_fraction is not valid");
				ret = -DPTX_RETURN_EINVAL;
			} else {
				transfer_unit_fraction -= transfer_unit_fraction_tmp;
				transfer_unit_fraction *= 64u;
				transfer_unit_fraction /= 1000u;
			}
		}
		if (transfer_unit_fraction > 0x3fu) {
			/*For KCS*/
			dptx_err("transfer_unit_fraction is %d out of range",
				transfer_unit_fraction);
				ret = -DPTX_RETURN_ENODEV;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		video_param->average_bytes_per_tu = (uint8_t)average_valid_symbols_per_tu;
		video_param->average_bytes_per_tu_frac = (uint8_t)transfer_unit_fraction;

		/*
		 * If the calculated average bytes per tu value is less than 6
		 * then the threshold value should be programmed to 32.
		 * For RGB or YCbCr444 modes if the hblank is less than or equal
		 * to 80pixels, then a value of 12 should be used.
		 * In all other cases the default value of 16 is sufficient.
		 */
		if (average_valid_symbols_per_tu < 6u) {
			/*For KCS*/
			video_param->fifo_threshold = 32u;
		} else if (dtd_param->h_blanking <= 80u) {
			/*For KCS*/
			video_param->fifo_threshold = 12u;
		} else {
			/*For KCS*/
			video_param->fifo_threshold = 16u;
		}
	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		dptx_notice("Calculating DP sample TU");
		dptx_notice("%s: ", dev_param->bMultStreamTransport ? "MST" : "SST");
		dptx_notice(" -.Pixel clock: %d", dtd_param->uiPixel_Clock);
		dptx_notice(" -.Num of Lanes: %d", dev_param->stDptxLink.ucNumOfLanes);
		dptx_notice(" -.Link rate: %s",
			    (dev_param->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_RBR) ? "RBR" :
			    (dev_param->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR) ? "HBR" :
			    (dev_param->stDptxLink.ucLinkRate == DPTX_PHYIF_CTRL_RATE_HBR2) ? "HBR2" : "HBR3");
		dptx_notice(" -.Pixel clock: %d", dtd_param->uiPixel_Clock);
		dptx_notice(" -.Hblanking: %d", dtd_param->h_blanking);
		dptx_notice(" -.Link Bpc: %u", video_param->bit_per_component);
		dptx_notice(" -.Average Symbols Per TU: %d ", video_param->average_bytes_per_tu);
		dptx_notice(" -.Fraction Per TU: %d", video_param->average_bytes_per_tu_frac);
		dptx_notice(" -.Init thresh: %d",	video_param->fifo_threshold);
	}

	return ret;
}


int32_t Dptx_VidIn_Fill_Dtd(struct dptx_dtd_params *dtd_param,
			    uint32_t video_code, uint32_t refresh_rate,
			    enum VIDEO_FORMAT_STANDARD_TYPE video_format_standard)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;

	(void)memset(dtd_param, 0, sizeof(*dtd_param));

	dtd_param->h_image_size = 16;
	dtd_param->v_image_size = 9;

	if (video_format_standard == VIDEO_FORMAT_CEA_861) {
		switch (video_code) {
		case DP_CUSTOM_1025_DTD_VIC:
			dtd_param->h_active = 1024;
			dtd_param->v_active = 600;
			dtd_param->h_blanking = 320;
			dtd_param->v_blanking = 35;
			dtd_param->h_sync_offset = 150;
			dtd_param->v_sync_offset = 15;
			dtd_param->h_sync_pulse_width = 20;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 51200;
			break;
		case DP_CUSTOM_1026_DTD_VIC:
			dtd_param->h_active = 5760;
			dtd_param->v_active = 900;
			dtd_param->h_blanking = 160;
			dtd_param->v_blanking = 26;
			dtd_param->h_sync_offset = 48;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 32;
			dtd_param->v_sync_pulse_width = 10;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 297000;
			break;
		case DP_CUSTOM_1027_DTD_VIC:
			dtd_param->h_active = 1920;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 64;
			dtd_param->v_blanking = 21;
			dtd_param->h_sync_offset = 30;
			dtd_param->v_sync_offset = 10;
			dtd_param->h_sync_pulse_width = 4;
			dtd_param->v_sync_pulse_width = 2;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 88200;
			break;
		case DP_CUSTOM_1028_DTD_VIC:
			dtd_param->h_active = 1920;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 460;	/* 356 + 96 + 8 */
			dtd_param->v_blanking = 40;	/* 36 + 3 + 1 */
			dtd_param->h_sync_offset = 356;
			dtd_param->v_sync_offset = 36;
			dtd_param->h_sync_pulse_width = 8;
			dtd_param->v_sync_pulse_width = 1;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 108520;
			break;
		case 1: /* 640x480p @ 59.94/60Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 640;
			dtd_param->v_active = 480;
			dtd_param->h_blanking = 160;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 16;
			dtd_param->v_sync_offset = 10;
			dtd_param->h_sync_pulse_width = 96;
			dtd_param->v_sync_pulse_width = 2;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 25175;
			break;
		case 2: /* 720x480p @ 59.94/60Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 720;
			dtd_param->v_active = 480;
			dtd_param->h_blanking = 138;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 16;
			dtd_param->v_sync_offset = 9;
			dtd_param->h_sync_pulse_width = 62;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 27000;
			break;
		case 3: /* 720x480p @ 59.94/60Hz 16:9 */
			dtd_param->h_active = 720;
			dtd_param->v_active = 480;
			dtd_param->h_blanking = 138;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 16;
			dtd_param->v_sync_offset = 9;
			dtd_param->h_sync_pulse_width = 62;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 27000;
			break;
		case 69:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1280;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 370;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 110;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 40;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 74250;
			break;
		case 4: /* 1280x720p @ 59.94/60Hz 16:9 */
			dtd_param->h_active = 1280;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 370;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 110;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 40;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 74250;
			break;
		case 5: /* 1920x1080i @ 59.94/60Hz 16:9 */
			dtd_param->h_active = 1920;
			dtd_param->v_active = 540;
			dtd_param->h_blanking = 280;
			dtd_param->v_blanking = 22;
			dtd_param->h_sync_offset = 88;
			dtd_param->v_sync_offset = 2;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 1;
			dtd_param->uiPixel_Clock = 74250;
			break;
		case 6: /* 720(1440)x480i @ 59.94/60Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1440;
			dtd_param->v_active = 240;
			dtd_param->h_blanking = 276;
			dtd_param->v_blanking = 22;
			dtd_param->h_sync_offset = 38;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 124;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 1;
			dtd_param->uiPixel_Clock = 27000;
			break;
		case 7: /* 720(1440)x480i @ 59.94/60Hz 16:9 */
			dtd_param->h_active = 1440;
			dtd_param->v_active = 240;
			dtd_param->h_blanking = 276;
			dtd_param->v_blanking = 22;
			dtd_param->h_sync_offset = 38;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 124;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 1;
			dtd_param->uiPixel_Clock = 27000;
			break;
		case 8: /* 720(1440)x240p @ 59.826/60.054/59.886/60.115Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1440;
			dtd_param->v_active = 240;
			dtd_param->h_blanking = 276;
			dtd_param->v_blanking = (refresh_rate == 59940u) ? 22u : 23u;
			dtd_param->h_sync_offset = 38;
			dtd_param->v_sync_offset = (refresh_rate == 59940u) ? 4u : 5u;
			dtd_param->h_sync_pulse_width = 124;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 27000;
			break;
		case 9: /* 720(1440)x240p @59.826/60.054/59.886/60.115Hz 16:9 */
			dtd_param->h_active = 1440;
			dtd_param->v_active = 240;
			dtd_param->h_blanking = 276;
			dtd_param->v_blanking = (refresh_rate == 59940u) ? 22u : 23u;
			dtd_param->h_sync_offset = 38;
			dtd_param->v_sync_offset = (refresh_rate == 59940u) ? 4u : 5u;
			dtd_param->h_sync_pulse_width = 124;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 27000;
			break;
		case 10: /* 2880x480i @ 59.94/60Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 2880;
			dtd_param->v_active = 240;
			dtd_param->h_blanking = 552;
			dtd_param->v_blanking = 22;
			dtd_param->h_sync_offset = 76;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 248;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 1;
			dtd_param->uiPixel_Clock = 54000;
			break;
		case 11: /* 2880x480i @ 59.94/60Hz 16:9 */
			dtd_param->h_active = 2880;
			dtd_param->v_active = 240;
			dtd_param->h_blanking = 552;
			dtd_param->v_blanking = 22;
			dtd_param->h_sync_offset = 76;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 248;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 1;
			dtd_param->uiPixel_Clock = 54000;
			break;
		case 12: /* 2880x240p @ 59.826/60.054/59.886/60.115Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 2880;
			dtd_param->v_active = 240;
			dtd_param->h_blanking = 552;
			dtd_param->v_blanking = (refresh_rate == 60054u) ? 22u : 23u;
			dtd_param->h_sync_offset = 76;
			dtd_param->v_sync_offset = (refresh_rate == 60054u) ? 4u : 5u;
			dtd_param->h_sync_pulse_width = 248;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 54000;
			break;
		case 13: /* 2880x240p @ 59.826/60.054/59.886/60.115Hz 16:9 */
			dtd_param->h_active = 2880;
			dtd_param->v_active = 240;
			dtd_param->h_blanking = 552;
			dtd_param->v_blanking = (refresh_rate == 60054u) ? 22u : 23u;
			dtd_param->h_sync_offset = 76;
			dtd_param->v_sync_offset = (refresh_rate == 60054u) ? 4u : 5u;
			dtd_param->h_sync_pulse_width = 248;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 54000;
			break;
		case 14: /* 1440x480p @ 59.94/60Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1440;
			dtd_param->v_active = 480;
			dtd_param->h_blanking = 276;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 32;
			dtd_param->v_sync_offset = 9;
			dtd_param->h_sync_pulse_width = 124;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 54000;
			break;
		case 15: /* 1440x480p @ 59.94/60Hz 16:9 */
			dtd_param->h_active = 1440;
			dtd_param->v_active = 480;
			dtd_param->h_blanking = 276;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 32;
			dtd_param->v_sync_offset = 9;
			dtd_param->h_sync_pulse_width = 124;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 54000;
			break;
		case 76:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1920;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 280;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 88;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 148500;
			break;
		case 16: /* 1920x1080p @ 59.94/60Hz 16:9 */
			dtd_param->h_active = 1920;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 280;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 88;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 148500;
			break;
		case 17: /* 720x576p @ 50Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 720;
			dtd_param->v_active = 576;
			dtd_param->h_blanking = 144;
			dtd_param->v_blanking = 49;
			dtd_param->h_sync_offset = 12;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 64;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 27000;
			break;
		case 18: /* 720x576p @ 50Hz 16:9 */
			dtd_param->h_active = 720;
			dtd_param->v_active = 576;
			dtd_param->h_blanking = 144;
			dtd_param->v_blanking = 49;
			dtd_param->h_sync_offset = 12;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 64;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 27000;
			break;
		case 68:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1280;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 700;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 440;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 40;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 74250;
			break;
		case 19: /* 1280x720p @ 50Hz 16:9 */
			dtd_param->h_active = 1280;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 700;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 440;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 40;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 74250;
			break;
		case 20: /* 1920x1080i @ 50Hz 16:9 */
			dtd_param->h_active = 1920;
			dtd_param->v_active = 540;
			dtd_param->h_blanking = 720;
			dtd_param->v_blanking = 22;
			dtd_param->h_sync_offset = 528;
			dtd_param->v_sync_offset = 2;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 1;
			dtd_param->uiPixel_Clock = 74250;
			break;
		case 21: /* 720(1440)x576i @ 50Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1440;
			dtd_param->v_active = 288;
			dtd_param->h_blanking = 288;
			dtd_param->v_blanking = 24;
			dtd_param->h_sync_offset = 24;
			dtd_param->v_sync_offset = 2;
			dtd_param->h_sync_pulse_width = 126;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 1;
			dtd_param->uiPixel_Clock = 27000;
			break;
		case 22: /* 720(1440)x576i @ 50Hz 16:9 */
			dtd_param->h_active = 1440;
			dtd_param->v_active = 288;
			dtd_param->h_blanking = 288;
			dtd_param->v_blanking = 24;
			dtd_param->h_sync_offset = 24;
			dtd_param->v_sync_offset = 2;
			dtd_param->h_sync_pulse_width = 126;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 1;
			dtd_param->uiPixel_Clock = 27000;
			break;
		case 23: /* 720(1440)x288p @ 50Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1440;
			dtd_param->v_active = 288;
			dtd_param->h_blanking = 288;
			dtd_param->v_blanking = (refresh_rate == 50080u) ? 24u : ((refresh_rate == 49920u) ? 25u : 26u);
			dtd_param->h_sync_offset = 24;
			dtd_param->v_sync_offset = (refresh_rate == 50080u) ? 2u : ((refresh_rate == 49920u) ? 3u : 4u);
			dtd_param->h_sync_pulse_width = 126;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 27000;
			break;
		case 24: /* 720(1440)x288p @ 50Hz 16:9 */
			dtd_param->h_active = 1440;
			dtd_param->v_active = 288;
			dtd_param->h_blanking = 288;
			dtd_param->v_blanking = (refresh_rate == 50080u) ? 24u : ((refresh_rate == 49920u) ? 25u : 26u);
			dtd_param->h_sync_offset = 24;
			dtd_param->v_sync_offset = (refresh_rate == 50080u) ? 2u : ((refresh_rate == 49920u) ? 3u : 4u);
			dtd_param->h_sync_pulse_width = 126;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 27000;
			break;
		case 25: /* 2880x576i @ 50Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 2880;
			dtd_param->v_active = 288;
			dtd_param->h_blanking = 576;
			dtd_param->v_blanking = 24;
			dtd_param->h_sync_offset = 48;
			dtd_param->v_sync_offset = 2;
			dtd_param->h_sync_pulse_width = 252;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 1;
			dtd_param->uiPixel_Clock = 54000;
			break;
		case 26: /* 2880x576i @ 50Hz 16:9 */
			dtd_param->h_active = 2880;
			dtd_param->v_active = 288;
			dtd_param->h_blanking = 576;
			dtd_param->v_blanking = 24;
			dtd_param->h_sync_offset = 48;
			dtd_param->v_sync_offset = 2;
			dtd_param->h_sync_pulse_width = 252;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 1;
			dtd_param->uiPixel_Clock = 54000;
			break;
		case 27: /* 2880x288p @ 50Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 2880;
			dtd_param->v_active = 288;
			dtd_param->h_blanking = 576;
			dtd_param->v_blanking = (refresh_rate == 50080u) ? 24u : ((refresh_rate == 49920u) ? 25u : 26u);
			dtd_param->h_sync_offset = 48;
			dtd_param->v_sync_offset = (refresh_rate == 50080u) ? 2u : ((refresh_rate == 49920u) ? 3u : 4u);
			dtd_param->h_sync_pulse_width = 252;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 54000;
			break;
		case 28: /* 2880x288p @ 50Hz 16:9 */
			dtd_param->h_active = 2880;
			dtd_param->v_active = 288;
			dtd_param->h_blanking = 576;
			dtd_param->v_blanking = (refresh_rate == 50080u) ? 24u : ((refresh_rate == 49920u) ? 25u : 26u);
			dtd_param->h_sync_offset = 48;
			dtd_param->v_sync_offset = (refresh_rate == 50080u) ? 2u : ((refresh_rate == 49920u) ? 3u : 4u);
			dtd_param->h_sync_pulse_width = 252;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 54000;
			break;
		case 29: /* 1440x576p @ 50Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1440;
			dtd_param->v_active = 576;
			dtd_param->h_blanking = 288;
			dtd_param->v_blanking = 49;
			dtd_param->h_sync_offset = 24;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 128;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 54000;
			break;
		case 30: /* 1440x576p @ 50Hz 16:9 */
			dtd_param->h_active = 1440;
			dtd_param->v_active = 576;
			dtd_param->h_blanking = 288;
			dtd_param->v_blanking = 49;
			dtd_param->h_sync_offset = 24;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 128;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 54000;
			break;
		case 75:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1920;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 720;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 528;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 148500;
			break;
		case 31: /* 1920x1080p @ 50Hz 16:9 */
			dtd_param->h_active = 1920;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 720;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 528;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 148500;
			break;
		case 72:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1920;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 830;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 638;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 74250;
			break;
		case 32: /* 1920x1080p @ 23.976/24Hz 16:9 */
			dtd_param->h_active = 1920;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 830;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 638;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 74250;
			break;
		case 73:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1920;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 720;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 528;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 74250;
			break;
		case 33: /* 1920x1080p @ 25Hz 16:9 */
			dtd_param->h_active = 1920;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 720;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 528;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 74250;
			break;
		case 74:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1920;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 280;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 88;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 74250;
			break;
		case 34: /* 1920x1080p @ 29.97/30Hz 16:9 */
			dtd_param->h_active = 1920;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 280;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 88;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 74250;
			break;
		case 35: /* 2880x480p @ 60Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 2880;
			dtd_param->v_active = 480;
			dtd_param->h_blanking = 552;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 64;
			dtd_param->v_sync_offset = 9;
			dtd_param->h_sync_pulse_width = 248;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 108000;
			break;
		case 36: /* 2880x480p @ 60Hz 16:9 */
			dtd_param->h_active = 2880;
			dtd_param->v_active = 480;
			dtd_param->h_blanking = 552;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 64;
			dtd_param->v_sync_offset = 9;
			dtd_param->h_sync_pulse_width = 248;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 108000;
			break;
		case 37: /* 2880x576p @ 50Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 2880;
			dtd_param->v_active = 576;
			dtd_param->h_blanking = 576;
			dtd_param->v_blanking = 49;
			dtd_param->h_sync_offset = 48;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 256;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 108000;
			break;
		case 38: /* 2880x576p @ 50Hz 16:9 */
			dtd_param->h_active = 2880;
			dtd_param->v_active = 576;
			dtd_param->h_blanking = 576;
			dtd_param->v_blanking = 49;
			dtd_param->h_sync_offset = 48;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 256;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 108000;
			break;
		case 39: /* 1920x1080i (1250 total) @ 50Hz 16:9 */
			dtd_param->h_active = 1920;
			dtd_param->v_active = 540;
			dtd_param->h_blanking = 384;
			dtd_param->v_blanking = 85;
			dtd_param->h_sync_offset = 32;
			dtd_param->v_sync_offset = 23;
			dtd_param->h_sync_pulse_width = 168;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 1;
			dtd_param->uiPixel_Clock = 72000;
			break;
		case 40: /* 1920x1080i @ 100Hz 16:9 */
			dtd_param->h_active = 1920;
			dtd_param->v_active = 540;
			dtd_param->h_blanking = 720;
			dtd_param->v_blanking = 22;
			dtd_param->h_sync_offset = 528;
			dtd_param->v_sync_offset = 2;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 1;
			dtd_param->uiPixel_Clock = 148500;
			break;
		case 70:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1280;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 700;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 440;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 40;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 148500;
			break;
		case 41: /* 1280x720p @ 100Hz 16:9 */
			dtd_param->h_active = 1280;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 700;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 440;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 40;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 148500;
			break;
		case 42: /* 720x576p @ 100Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 720;
			dtd_param->v_active = 576;
			dtd_param->h_blanking = 144;
			dtd_param->v_blanking = 49;
			dtd_param->h_sync_offset = 12;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 64;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 54000;
			break;
		case 43: /* 720x576p @ 100Hz 16:9 */
			dtd_param->h_active = 720;
			dtd_param->v_active = 576;
			dtd_param->h_blanking = 144;
			dtd_param->v_blanking = 49;
			dtd_param->h_sync_offset = 12;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 64;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 54000;
			break;
		case 44: /* 720(1440)x576i @ 100Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1440;
			dtd_param->v_active = 288;
			dtd_param->h_blanking = 288;
			dtd_param->v_blanking = 24;
			dtd_param->h_sync_offset = 24;
			dtd_param->v_sync_offset = 2;
			dtd_param->h_sync_pulse_width = 126;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 1;
			dtd_param->uiPixel_Clock = 54000;
			break;
		case 45: /* 720(1440)x576i @ 100Hz 16:9 */
			dtd_param->h_active = 1440;
			dtd_param->v_active = 288;
			dtd_param->h_blanking = 288;
			dtd_param->v_blanking = 24;
			dtd_param->h_sync_offset = 24;
			dtd_param->v_sync_offset = 2;
			dtd_param->h_sync_pulse_width = 126;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 1;
			dtd_param->uiPixel_Clock = 54000;
			break;
		case 46: /* 1920x1080i @ 119.88/120Hz 16:9 */
			dtd_param->h_active = 1920;
			dtd_param->v_active = 540;
			dtd_param->h_blanking = 288;
			dtd_param->v_blanking = 22;
			dtd_param->h_sync_offset = 88;
			dtd_param->v_sync_offset = 2;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 1;
			dtd_param->uiPixel_Clock = 148500;
			break;
		case 71:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1280;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 370;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 110;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 40;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 148500;
			break;
		case 47: /* 1280x720p @ 119.88/120Hz 16:9 */
			dtd_param->h_active = 1280;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 370;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 110;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 40;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 148500;
			break;
		case 48: /* 720x480p @ 119.88/120Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 720;
			dtd_param->v_active = 480;
			dtd_param->h_blanking = 138;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 16;
			dtd_param->v_sync_offset = 9;
			dtd_param->h_sync_pulse_width = 62;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 54000;
			break;
		case 49: /* 720x480p @ 119.88/120Hz 16:9 */
			dtd_param->h_active = 720;
			dtd_param->v_active = 480;
			dtd_param->h_blanking = 138;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 16;
			dtd_param->v_sync_offset = 9;
			dtd_param->h_sync_pulse_width = 62;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 54000;
			break;
		case 50: /* 720(1440)x480i @ 119.88/120Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1440;
			dtd_param->v_active = 240;
			dtd_param->h_blanking = 276;
			dtd_param->v_blanking = 22;
			dtd_param->h_sync_offset = 38;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 124;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 1;
			dtd_param->uiPixel_Clock = 54000;
			break;
		case 51: /* 720(1440)x480i @ 119.88/120Hz 16:9 */
			dtd_param->h_active = 1440;
			dtd_param->v_active = 240;
			dtd_param->h_blanking = 276;
			dtd_param->v_blanking = 22;
			dtd_param->h_sync_offset = 38;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 124;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 1;
			dtd_param->uiPixel_Clock = 54000;
			break;
		case 52: /* 720X576p @ 200Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 720;
			dtd_param->v_active = 576;
			dtd_param->h_blanking = 144;
			dtd_param->v_blanking = 49;
			dtd_param->h_sync_offset = 12;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 64;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 108000;
			break;
		case 53: /* 720X576p @ 200Hz 16:9 */
			dtd_param->h_active = 720;
			dtd_param->v_active = 576;
			dtd_param->h_blanking = 144;
			dtd_param->v_blanking = 49;
			dtd_param->h_sync_offset = 12;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 64;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 108000;
			break;
		case 54: /* 720(1440)x576i @ 200Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1440;
			dtd_param->v_active = 288;
			dtd_param->h_blanking = 288;
			dtd_param->v_blanking = 24;
			dtd_param->h_sync_offset = 24;
			dtd_param->v_sync_offset = 2;
			dtd_param->h_sync_pulse_width = 126;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 1;
			dtd_param->uiPixel_Clock = 108000;
			break;
		case 55: /* 720(1440)x576i @ 200Hz 16:9 */
			dtd_param->h_active = 1440;
			dtd_param->v_active = 288;
			dtd_param->h_blanking = 288;
			dtd_param->v_blanking = 24;
			dtd_param->h_sync_offset = 24;
			dtd_param->v_sync_offset = 2;
			dtd_param->h_sync_pulse_width = 126;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 1;
			dtd_param->uiPixel_Clock = 108000;
			break;
		case 56: /* 720x480p @ 239.76/240Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 720;
			dtd_param->v_active = 480;
			dtd_param->h_blanking = 138;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 16;
			dtd_param->v_sync_offset = 9;
			dtd_param->h_sync_pulse_width = 62;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 108000;
			break;
		case 57: /* 720x480p @ 239.76/240Hz 16:9 */
			dtd_param->h_active = 720;
			dtd_param->v_active = 480;
			dtd_param->h_blanking = 138;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 16;
			dtd_param->v_sync_offset = 9;
			dtd_param->h_sync_pulse_width = 62;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 108000;
			break;
		case 58: /* 720(1440)x480i @ 239.76/240Hz 4:3 */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1440;
			dtd_param->v_active = 240;
			dtd_param->h_blanking = 276;
			dtd_param->v_blanking = 22;
			dtd_param->h_sync_offset = 38;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 124;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 1;
			dtd_param->uiPixel_Clock = 108000;
			break;
		case 59: /* 720(1440)x480i @ 239.76/240Hz 16:9 */
			dtd_param->h_active = 1440;
			dtd_param->v_active = 240;
			dtd_param->h_blanking = 276;
			dtd_param->v_blanking = 22;
			dtd_param->h_sync_offset = 38;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 124;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 1;
			dtd_param->uiPixel_Clock = 108000;
			break;
		case 65:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1280;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 2020;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 1760;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 40;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 594000;
			break;
		case 60: /* 1280x720p @ 23.97/24Hz 16:9 */
			dtd_param->h_active = 1280;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 2020;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 1760;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 40;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 594000;
			break;
		case 66:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1280;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 2680;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 2420;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 40;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 74250;
			break;
		case 61: /* 1280x720p @ 25Hz 16:9 */
			dtd_param->h_active = 1280;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 2680;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 2420;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 40;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 74250;
			break;
		case 67:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1280;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 2020;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 1760;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 40;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 74250;
			break;
		case 62: /* 1280x720p @ 29.97/30Hz  16:9 */
			dtd_param->h_active = 1280;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 2020;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 1760;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 40;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 74250;
			break;
		case 78:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1920;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 280;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 88;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 297000;
			break;
		case 63: /* 1920x1080p @ 119.88/120Hz 16:9 */
			dtd_param->h_active = 1920;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 280;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 88;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 297000;
			break;
		case 77:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1920;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 720;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 528;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 297000;
			break;
		case 64: /* 1920x1080p @ 100Hz 16:9 */
			dtd_param->h_active = 1920;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 720;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 528;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 297000;
			break;
		case 79:
			dtd_param->h_active = 1680;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 1620;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 1360;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 40;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 594000;
			break;
		case 80:
			dtd_param->h_active = 1680;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 1488;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 1228;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 40;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 594000;
			break;
		case 81:
			dtd_param->h_active = 1680;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 960;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 700;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 40;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 594000;
			break;
		case 82:
			dtd_param->h_active = 1680;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 520;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 260;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 40;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 82500;
			break;
		case 83:
			dtd_param->h_active = 1680;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 520;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 260;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 40;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 99000;
			break;
		case 84:
			dtd_param->h_active = 1680;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 320;
			dtd_param->v_blanking = 105;
			dtd_param->h_sync_offset = 60;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 40;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 165000;
			break;
		case 85:
			dtd_param->h_active = 1680;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 320;
			dtd_param->v_blanking = 105;
			dtd_param->h_sync_offset = 60;
			dtd_param->v_sync_offset = 5;
			dtd_param->h_sync_pulse_width = 40;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 198000;
			break;
		case 86:
			dtd_param->h_active = 2560;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 1190;
			dtd_param->v_blanking = 20;
			dtd_param->h_sync_offset = 998;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 99000;
			break;
		case 87:
			dtd_param->h_active = 2560;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 640;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 448;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 90000;
			break;
		case 88:
			dtd_param->h_active = 2560;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 960;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 768;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 118800;
			break;
		case 89:
			dtd_param->h_active = 2560;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 740;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 548;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 185625;
			break;
		case 90:
			dtd_param->h_active = 2560;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 440;
			dtd_param->v_blanking = 20;
			dtd_param->h_sync_offset = 248;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 198000;
			break;
		case 91:
			dtd_param->h_active = 2560;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 410;
			dtd_param->v_blanking = 170;
			dtd_param->h_sync_offset = 218;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 371250;
			break;
		case 92:
			dtd_param->h_active = 2560;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 740;
			dtd_param->v_blanking = 170;
			dtd_param->h_sync_offset = 548;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 495000;
			break;
		case 101:
			dtd_param->h_active = 4096;
			dtd_param->v_active = 2160;
			dtd_param->h_blanking = 1184;
			dtd_param->v_blanking = 90;
			dtd_param->h_sync_offset = 968;
			dtd_param->v_sync_offset = 8;
			dtd_param->h_sync_pulse_width = 88;
			dtd_param->v_sync_pulse_width = 10;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 59400;
			break;
		case 100:
			dtd_param->h_active = 4096;
			dtd_param->v_active = 2160;
			dtd_param->h_blanking = 304;
			dtd_param->v_blanking = 90;
			dtd_param->h_sync_offset = 88;
			dtd_param->v_sync_offset = 8;
			dtd_param->h_sync_pulse_width = 88;
			dtd_param->v_sync_pulse_width = 10;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 297000;
			break;
		case 99:
			dtd_param->h_active = 4096;
			dtd_param->v_active = 2160;
			dtd_param->h_blanking = 1184;
			dtd_param->v_blanking = 90;
			dtd_param->h_sync_offset = 968;
			dtd_param->v_sync_offset = 8;
			dtd_param->h_sync_pulse_width = 88;
			dtd_param->v_sync_pulse_width = 10;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 297000;
			break;
		case 102:
			dtd_param->h_active = 4096;
			dtd_param->v_active = 2160;
			dtd_param->h_blanking = 304;
			dtd_param->v_blanking = 90;
			dtd_param->h_sync_offset = 88;
			dtd_param->v_sync_offset = 8;
			dtd_param->h_sync_pulse_width = 88;
			dtd_param->v_sync_pulse_width = 10;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 594000;
			break;
		case 103:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 3840;
			dtd_param->v_active = 2160;
			dtd_param->h_blanking = 1660;
			dtd_param->v_blanking = 90;
			dtd_param->h_sync_offset = 1276;
			dtd_param->v_sync_offset = 8;
			dtd_param->h_sync_pulse_width = 88;
			dtd_param->v_sync_pulse_width = 10;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 297000;
			break;
		case 93:		/* 4k x 2k, 30Hz */
			dtd_param->h_active = 3840;
			dtd_param->v_active = 2160;
			dtd_param->h_blanking = 1660;
			dtd_param->v_blanking = 90;
			dtd_param->h_sync_offset = 1276;
			dtd_param->v_sync_offset = 8;
			dtd_param->h_sync_pulse_width = 88;
			dtd_param->v_sync_pulse_width = 10;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 297000;
			break;
		case 104:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 3840;
			dtd_param->v_active = 2160;
			dtd_param->h_blanking = 1440;
			dtd_param->v_blanking = 90;
			dtd_param->h_sync_offset = 1056;
			dtd_param->v_sync_offset = 8;
			dtd_param->h_sync_pulse_width = 88;
			dtd_param->v_sync_pulse_width = 10;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 297000;
			break;
		case 94:
			dtd_param->h_active = 3840;
			dtd_param->v_active = 2160;
			dtd_param->h_blanking = 1440;
			dtd_param->v_blanking = 90;
			dtd_param->h_sync_offset = 1056;
			dtd_param->v_sync_offset = 8;
			dtd_param->h_sync_pulse_width = 88;
			dtd_param->v_sync_pulse_width = 10;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 297000;
			break;
		case 105:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 3840;
			dtd_param->v_active = 2160;
			dtd_param->h_blanking = 560;
			dtd_param->v_blanking = 90;
			dtd_param->h_sync_offset = 176;
			dtd_param->v_sync_offset = 8;
			dtd_param->h_sync_pulse_width = 88;
			dtd_param->v_sync_pulse_width = 10;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 297000;
			break;
		case 95:
			dtd_param->h_active = 3840;
			dtd_param->v_active = 2160;
			dtd_param->h_blanking = 560;
			dtd_param->v_blanking = 90;
			dtd_param->h_sync_offset = 176;
			dtd_param->v_sync_offset = 8;
			dtd_param->h_sync_pulse_width = 88;
			dtd_param->v_sync_pulse_width = 10;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 297000;
			break;
		case 106:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 3840;
			dtd_param->v_active = 2160;
			dtd_param->h_blanking = 1440;
			dtd_param->v_blanking = 90;
			dtd_param->h_sync_offset = 1056;
			dtd_param->v_sync_offset = 8;
			dtd_param->h_sync_pulse_width = 88;
			dtd_param->v_sync_pulse_width = 10;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 594000;
			break;
		case 96:
			dtd_param->h_active = 3840;
			dtd_param->v_active = 2160;
			dtd_param->h_blanking = 1440;
			dtd_param->v_blanking = 90;
			dtd_param->h_sync_offset = 1056;
			dtd_param->v_sync_offset = 8;
			dtd_param->h_sync_pulse_width = 88;
			dtd_param->v_sync_pulse_width = 10;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 594000;
			break;
		case 107:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 3840;
			dtd_param->v_active = 2160;
			dtd_param->h_blanking = 560;
			dtd_param->v_blanking = 90;
			dtd_param->h_sync_offset = 176;
			dtd_param->v_sync_offset = 8;
			dtd_param->h_sync_pulse_width = 88;
			dtd_param->v_sync_pulse_width = 10;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 594000;
			break;
		case 97:
			dtd_param->h_active = 3840;
			dtd_param->v_active = 2160;
			dtd_param->h_blanking = 560;
			dtd_param->v_blanking = 90;
			dtd_param->h_sync_offset = 176;
			dtd_param->v_sync_offset = 8;
			dtd_param->h_sync_pulse_width = 88;
			dtd_param->v_sync_pulse_width = 10;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 594000;
			break;
		case 98:
			dtd_param->h_active = 4096;
			dtd_param->v_active = 2160;
			dtd_param->h_blanking = 1404;
			dtd_param->v_blanking = 90;
			dtd_param->h_sync_offset = 1020;
			dtd_param->v_sync_offset = 8;
			dtd_param->h_sync_pulse_width = 88;
			dtd_param->v_sync_pulse_width = 10;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;
			dtd_param->uiPixel_Clock = 297000;
			break;
		default:
			ret = -DPTX_RETURN_EINVAL;
			break;
		}
	} else if (video_format_standard == VIDEO_FORMAT_VESA_CVT) {
		switch (video_code) {
		case 1:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 640;
			dtd_param->v_active = 480;
			dtd_param->h_blanking = 160;
			dtd_param->v_blanking = 20;
			dtd_param->h_sync_offset = 8;
			dtd_param->v_sync_offset = 1;
			dtd_param->h_sync_pulse_width = 32;
			dtd_param->v_sync_pulse_width = 8;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 23750;
			break;
		case 2:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 800;
			dtd_param->v_active = 600;
			dtd_param->h_blanking = 224;
			dtd_param->v_blanking = 24;
			dtd_param->h_sync_offset = 31;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 81;
			dtd_param->v_sync_pulse_width = 4;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 38250;
			break;
		case 3:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1024;
			dtd_param->v_active = 768;
			dtd_param->h_blanking = 304;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 48;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 104;
			dtd_param->v_sync_pulse_width = 4;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 63500;
			break;
		case 4:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1280;
			dtd_param->v_active = 960;
			dtd_param->h_blanking = 416;
			dtd_param->v_blanking = 36;
			dtd_param->h_sync_offset = 80;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 128;
			dtd_param->v_sync_pulse_width = 4;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 101250;
			break;
		case 5:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1400;
			dtd_param->v_active = 1050;
			dtd_param->h_blanking = 464;
			dtd_param->v_blanking = 39;
			dtd_param->h_sync_offset = 88;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 144;
			dtd_param->v_sync_pulse_width = 4;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 121750;
			break;
		case 6:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1600;
			dtd_param->v_active = 1200;
			dtd_param->h_blanking = 560;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 112;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 68;
			dtd_param->v_sync_pulse_width = 4;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 161000;
			break;
		case 12:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1280;
			dtd_param->v_active = 1024;
			dtd_param->h_blanking = 432;
			dtd_param->v_blanking = 39;
			dtd_param->h_sync_offset = 80;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 136;
			dtd_param->v_sync_pulse_width = 7;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 109000;
			break;
		case 13:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1280;
			dtd_param->v_active = 768;
			dtd_param->h_blanking = 384;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 64;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 128;
			dtd_param->v_sync_pulse_width = 7;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 79500;
			break;
		case 16:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1280;
			dtd_param->v_active = 720;
			dtd_param->h_blanking = 384;
			dtd_param->v_blanking = 28;
			dtd_param->h_sync_offset = 64;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 128;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 74500;
			break;
		case 17:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1360;
			dtd_param->v_active = 768;
			dtd_param->h_blanking = 416;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 72;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 136;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 84750;
			break;
		case 20:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1920;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 656;
			dtd_param->v_blanking = 40;
			dtd_param->h_sync_offset = 128;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 200;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 173000;
			break;
		case 22:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 2560;
			dtd_param->v_active = 1440;
			dtd_param->h_blanking = 928;
			dtd_param->v_blanking = 53;
			dtd_param->h_sync_offset = 192;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 272;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 312250;
			break;
		case 28:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1280;
			dtd_param->v_active = 800;
			dtd_param->h_blanking = 400;
			dtd_param->v_blanking = 31;
			dtd_param->h_sync_offset = 72;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 128;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 83500;
			break;
		case 34:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1920;
			dtd_param->v_active = 1200;
			dtd_param->h_blanking = 672;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 136;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 200;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 193250;
			break;
		case 38:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 3840;
			dtd_param->v_active = 2400;
			dtd_param->h_blanking = 80;
			dtd_param->v_blanking = 69;
			dtd_param->h_sync_offset = 320;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 424;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 580128;
			break;
		case 40:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1600;
			dtd_param->v_active = 1200;
			dtd_param->h_blanking = 160;
			dtd_param->v_blanking = 35;
			dtd_param->h_sync_offset = 48;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 32;
			dtd_param->v_sync_pulse_width = 4;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 124076;
			break;
		case 41:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 2048;
			dtd_param->v_active = 1536;
			dtd_param->h_blanking = 160;
			dtd_param->v_blanking = 44;
			dtd_param->h_sync_offset = 48;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 32;
			dtd_param->v_sync_pulse_width = 4;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 208000;
			break;
		default:
			ret = -DPTX_RETURN_EINVAL;
			break;
		}
	}  else if (video_format_standard == VIDEO_FORMAT_VESA_DMT) {
		switch (video_code) {
		case 1: // HISilicon timing
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 3600;
			dtd_param->v_active = 1800;
			dtd_param->h_blanking = 120;
			dtd_param->v_blanking = 128;
			dtd_param->h_sync_offset = 20;
			dtd_param->v_sync_offset = 2;
			dtd_param->h_sync_pulse_width = 20;
			dtd_param->v_sync_pulse_width = 2;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 645500;
			break;
		case 2:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 3840;
			dtd_param->v_active = 2160;
			dtd_param->h_blanking = 160;
			dtd_param->v_blanking = 62;
			dtd_param->h_sync_offset = 48;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 32;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 533000;
			break;
		case 4:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 640;
			dtd_param->v_active = 480;
			dtd_param->h_blanking = 144;
			dtd_param->v_blanking = 29;
			dtd_param->h_sync_offset = 8;
			dtd_param->v_sync_offset = 2;
			dtd_param->h_sync_pulse_width = 96;
			dtd_param->v_sync_pulse_width = 2;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 25175;
			break;
		case 13:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 800;
			dtd_param->v_active = 600;
			dtd_param->h_blanking = 160;
			dtd_param->v_blanking = 36;
			dtd_param->h_sync_offset = 48;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 32;
			dtd_param->v_sync_pulse_width = 4;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 73250;
			break;
		case 14: /* 848x480p@60Hz */
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 848;
			dtd_param->v_active = 480;
			dtd_param->h_blanking = 240;
			dtd_param->v_blanking = 37;
			dtd_param->h_sync_offset = 16;
			dtd_param->v_sync_offset = 6;
			dtd_param->h_sync_pulse_width = 112;
			dtd_param->v_sync_pulse_width = 8;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI)  */;
			dtd_param->uiPixel_Clock = 33750;
			break;
		case 22:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1280;
			dtd_param->v_active = 768;
			dtd_param->h_blanking = 160;
			dtd_param->v_blanking = 22;
			dtd_param->h_sync_offset = 48;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 32;
			dtd_param->v_sync_pulse_width = 7;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 68250;
			break;
		case 35:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1280;
			dtd_param->v_active = 1024;
			dtd_param->h_blanking = 408;
			dtd_param->v_blanking = 42;
			dtd_param->h_sync_offset = 48;
			dtd_param->v_sync_offset = 1;
			dtd_param->h_sync_pulse_width = 112;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 108000;
			break;
		case 39:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1360;
			dtd_param->v_active = 768;
			dtd_param->h_blanking = 432;
			dtd_param->v_blanking = 27;
			dtd_param->h_sync_offset = 64;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 112;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 85500;
			break;
		case 40:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1360;
			dtd_param->v_active = 768;
			dtd_param->h_blanking = 160;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 48;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 32;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 148250;
			break;
		case 81:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1366;
			dtd_param->v_active = 768;
			dtd_param->h_blanking = 426;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 70;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 142;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 85500;
			break;
		case 86:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1366;
			dtd_param->v_active = 768;
			dtd_param->h_blanking = 134;
			dtd_param->v_blanking = 32;
			dtd_param->h_sync_offset = 14;
			dtd_param->v_sync_offset = 1;
			dtd_param->h_sync_pulse_width = 56;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 72000;
			break;
		case 87:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 4096;
			dtd_param->v_active = 2160;
			dtd_param->h_blanking = 80;
			dtd_param->v_blanking = 62;
			dtd_param->h_sync_offset = 8;
			dtd_param->v_sync_offset = 48;
			dtd_param->h_sync_pulse_width = 32;
			dtd_param->v_sync_pulse_width = 8;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 556744;
			break;
		case 88:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 4096;
			dtd_param->v_active = 2160;
			dtd_param->h_blanking = 80;
			dtd_param->v_blanking = 62;
			dtd_param->h_sync_offset = 8;
			dtd_param->v_sync_offset = 48;
			dtd_param->h_sync_pulse_width = 32;
			dtd_param->v_sync_pulse_width = 8;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 556188;
			break;
		case 41:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1400;
			dtd_param->v_active = 1050;
			dtd_param->h_blanking = 160;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 48;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 32;
			dtd_param->v_sync_pulse_width = 4;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 101000;
			break;
		case 42:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1400;
			dtd_param->v_active = 1050;
			dtd_param->h_blanking = 464;
			dtd_param->v_blanking = 39;
			dtd_param->h_sync_offset = 88;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 144;
			dtd_param->v_sync_pulse_width = 4;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 121750;
			break;
		case 46:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1440;
			dtd_param->v_active = 900;
			dtd_param->h_blanking = 160;
			dtd_param->v_blanking = 26;
			dtd_param->h_sync_offset = 48;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 32;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 88750;
			break;
		case 47:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1440;
			dtd_param->v_active = 900;
			dtd_param->h_blanking = 464;
			dtd_param->v_blanking = 34;
			dtd_param->h_sync_offset = 80;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 152;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 106500;
			break;
		case 51:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1600;
			dtd_param->v_active = 1200;
			dtd_param->h_blanking = 560;
			dtd_param->v_blanking = 50;
			dtd_param->h_sync_offset = 64;
			dtd_param->v_sync_offset = 1;
			dtd_param->h_sync_pulse_width = 192;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 162000;
			break;
		case 57:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1680;
			dtd_param->v_active = 1050;
			dtd_param->h_blanking = 160;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 48;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 32;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 119000;
			break;
		case 58:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1680;
			dtd_param->v_active = 1050;
			dtd_param->h_blanking = 560;
			dtd_param->v_blanking = 39;
			dtd_param->h_sync_offset = 104;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 176;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 146250;
			break;
		case 68:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1920;
			dtd_param->v_active = 1200;
			dtd_param->h_blanking = 160;
			dtd_param->v_blanking = 35;
			dtd_param->h_sync_offset = 48;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 32;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 154000;
			break;
		case 69:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1920;
			dtd_param->v_active = 1200;
			dtd_param->h_blanking = 672;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 136;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 200;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 193250;
			break;
		case 82:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1920;
			dtd_param->v_active = 1080;
			dtd_param->h_blanking = 280;
			dtd_param->v_blanking = 45;
			dtd_param->h_sync_offset = 88;
			dtd_param->v_sync_offset = 4;
			dtd_param->h_sync_pulse_width = 44;
			dtd_param->v_sync_pulse_width = 5;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 148500;
			break;
		case 83:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1600;
			dtd_param->v_active = 900;
			dtd_param->h_blanking = 200;
			dtd_param->v_blanking = 100;
			dtd_param->h_sync_offset = 24;
			dtd_param->v_sync_offset = 1;
			dtd_param->h_sync_pulse_width = 80;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 108000;
			break;
		case 9:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 800;
			dtd_param->v_active = 600;
			dtd_param->h_blanking = 256;
			dtd_param->v_blanking = 28;
			dtd_param->h_sync_offset = 40;
			dtd_param->v_sync_offset = 1;
			dtd_param->h_sync_pulse_width = 128;
			dtd_param->v_sync_pulse_width = 4;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 40000;
			break;
		case 16:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1024;
			dtd_param->v_active = 768;
			dtd_param->h_blanking = 320;
			dtd_param->v_blanking = 38;
			dtd_param->h_sync_offset = 24;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 136;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 65000;
			break;
		case 23:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1280;
			dtd_param->v_active = 768;
			dtd_param->h_blanking = 384;
			dtd_param->v_blanking = 30;
			dtd_param->h_sync_offset = 64;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 128;
			dtd_param->v_sync_pulse_width = 7;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 79500;
			break;
		case 62:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active =  1792;
			dtd_param->v_active = 1344;
			dtd_param->h_blanking = 656;
			dtd_param->v_blanking =  50;
			dtd_param->h_sync_offset = 128;
			dtd_param->v_sync_offset = 1;
			dtd_param->h_sync_pulse_width = 200;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0; /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 204750;
			break;
		case 32:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1280;
			dtd_param->v_active = 960;
			dtd_param->h_blanking = 520;
			dtd_param->v_blanking = 40;
			dtd_param->h_sync_offset = 96;
			dtd_param->v_sync_offset = 1;
			dtd_param->h_sync_pulse_width = 112;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;  /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 108000;
			break;
		case 73:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1920;
			dtd_param->v_active = 1440;
			dtd_param->h_blanking = 680;
			dtd_param->v_blanking = 60;
			dtd_param->h_sync_offset = 128;
			dtd_param->v_sync_offset = 1;
			dtd_param->h_sync_pulse_width = 208;
			dtd_param->v_sync_pulse_width = 3;
			dtd_param->h_sync_polarity = 0;
			dtd_param->v_sync_polarity = 1;
			dtd_param->interlaced = 0;  /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 234000;
			break;
		case 27:
			dtd_param->h_image_size = 4;
			dtd_param->v_image_size = 3;
			dtd_param->h_active = 1280;
			dtd_param->v_active = 800;
			dtd_param->h_blanking = 160;
			dtd_param->v_blanking = 23;
			dtd_param->h_sync_offset = 48;
			dtd_param->v_sync_offset = 3;
			dtd_param->h_sync_pulse_width = 32;
			dtd_param->v_sync_pulse_width = 6;
			dtd_param->h_sync_polarity = 1;
			dtd_param->v_sync_polarity = 0;
			dtd_param->interlaced = 0;  /* (progressive_nI) */
			dtd_param->uiPixel_Clock = 71000;
			break;
		default:
			ret = -DPTX_RETURN_EINVAL;
			break;
		}
	} else {
		ret = -DPTX_RETURN_EINVAL;
	}

	return ret;
}
