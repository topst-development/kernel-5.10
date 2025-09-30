/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#ifndef TCC_VPU_V3_DECODER_METADATA_H
#define TCC_VPU_V3_DECODER_METADATA_H

/**
 * @def VPU_METADATA_MAX_DPB_SIZE
 * @brief Maximum size for decoding picture buffer (DPB) in vpu_metadata.
 */
#define VPU_METADATA_MAX_DPB_SIZE 17

/**
 * @def VPU_METADATA_MAX_NUM_SUB_LAYER
 * @brief Maximum number of sub-layers in vpu_metadata.
 */
#define VPU_METADATA_MAX_NUM_SUB_LAYER 8

/**
 * @def VPU_METADATA_MAX_NUM_ST_RPS
 * @brief Maximum number of short-term reference picture sets (ST RPS) in vpu_metadata.
 */
#define VPU_METADATA_MAX_NUM_ST_RPS 64

/**
 * @def VPU_METADATA_MAX_CPB_CNT
 * @brief Maximum count of coded picture buffer (CPB) in vpu_metadata.
 */
#define VPU_METADATA_MAX_CPB_CNT 32

/**
 * @def VPU_METADATA_MAX_NUM_VERTICAL_FILTERS
 * @brief Maximum number of vertical filters in vpu_metadata.
 */
#define VPU_METADATA_MAX_NUM_VERTICAL_FILTERS 5

/**
 * @def VPU_METADATA_MAX_NUM_HORIZONTAL_FILTERS
 * @brief Maximum number of horizontal filters in vpu_metadata.
 */
#define VPU_METADATA_MAX_NUM_HORIZONTAL_FILTERS 3

/**
 * @def VPU_METADATA_MAX_TAP_LENGTH
 * @brief Maximum length for filter tap in vpu_metadata.
 */
#define VPU_METADATA_MAX_TAP_LENGTH 32

/**
 * @def VPU_METADATA_MAX_NUM_KNEE_POINT
 * @brief Maximum number of knee points in vpu_metadata.
 */
#define VPU_METADATA_MAX_NUM_KNEE_POINT 999

/**
 * @def VPU_METADATA_MAX_NUM_TONE_VALUE
 * @brief Maximum number of tone values in vpu_metadata.
 */
#define VPU_METADATA_MAX_NUM_TONE_VALUE 1024

/**
 * @def VPU_METADATA_MAX_NUM_FILM_GRAIN_COMPONENT
 * @brief Maximum number of film grain components in vpu_metadata.
 */
#define VPU_METADATA_MAX_NUM_FILM_GRAIN_COMPONENT 3

/**
 * @def VPU_METADATA_MAX_NUM_INTENSITY_INTERVALS
 * @brief Maximum number of intensity intervals in vpu_metadata.
 */
#define VPU_METADATA_MAX_NUM_INTENSITY_INTERVALS 256

/**
 * @def VPU_METADATA_MAX_NUM_MODEL_VALUES
 * @brief Maximum number of model values in vpu_metadata.
 */
#define VPU_METADATA_MAX_NUM_MODEL_VALUES 5

/**
 * @def VPU_METADATA_MAX_LUT_NUM_VAL
 * @brief Maximum number of LUT values in vpu_metadata.
 */
#define VPU_METADATA_MAX_LUT_NUM_VAL 3

/**
 * @def VPU_METADATA_MAX_LUT_NUM_VAL_MINUS1
 * @brief Maximum number of LUT values minus 1 in vpu_metadata.
 */
#define VPU_METADATA_MAX_LUT_NUM_VAL_MINUS1 33

/**
 * @def VPU_METADATA_MAX_COLOUR_REMAP_COEFFS
 * @brief Maximum number of colour remap coefficients in vpu_metadata.
 */
#define VPU_METADATA_MAX_COLOUR_REMAP_COEFFS 3



/**
 * @brief Structure for VPU metadata window.
 */
typedef struct vpu_metadata_win_t {
	/** @brief Left coordinate of the window. */
	short sLeft;

	/** @brief Right coordinate of the window. */
	short sRight;

	/** @brief Top coordinate of the window. */
	short sTop;

	/** @brief Bottom coordinate of the window. */
	short sBottom;
} vpu_metadata_win_t;


/**
 * @brief Structure for VPU metadata HRD parameter information.
 */
typedef struct vpu_metadata_hrd_param_t {
	/** NAL HRD parameter present flag. */
	unsigned char nal_hrd_param_present_flag;

	/** VCL HRD parameter present flag. */
	unsigned char vcl_hrd_param_present_flag;

	/** Sub-picture HRD parameters present flag. */
	unsigned char sub_pic_hrd_params_present_flag;

	/** Tick divisor minus 2. */
	unsigned char tick_divisor_minus2;

	/** DU CPB removal delay increment length minus 1. */
	char du_cpb_removal_delay_inc_length_minus1;

	/** Sub-picture CPB parameters in picture timing SEI flag. */
	char sub_pic_cpb_params_in_pic_timing_sei_flag;

	/** DPB output delay DU length minus 1. */
	char dpb_output_delay_du_length_minus1;

	/** Bit rate scale. */
	char bit_rate_scale;

	/** CPB size scale. */
	char cpb_size_scale;

	/** Initial CPB removal delay length minus 1. */
	char initial_cpb_removal_delay_length_minus1;

	/** CPB removal delay length minus 1. */
	char cpb_removal_delay_length_minus1;

	/** DPB output delay length minus 1. */
	char dpb_output_delay_length_minus1;

	/** Fixed picture rate generation flags for each sub-layer. */
	unsigned char fixed_pic_rate_gen_flag[VPU_METADATA_MAX_NUM_SUB_LAYER];

	/** Fixed picture rate within CVS flags for each sub-layer. */
	unsigned char fixed_pic_rate_within_cvs_flag[VPU_METADATA_MAX_NUM_SUB_LAYER];

	/** Low delay HRD flags for each sub-layer. */
	unsigned char low_delay_hrd_flag[VPU_METADATA_MAX_NUM_SUB_LAYER];

	/** CPB count minus 1 for each sub-layer. */
	char cpb_cnt_minus1[VPU_METADATA_MAX_NUM_SUB_LAYER];

	/** Elemental duration in time code minus 1 for each sub-layer. */
	short elemental_duration_in_tc_minus1[VPU_METADATA_MAX_NUM_SUB_LAYER];

	/** NAL bit rate values minus 1 for each sub-layer and CPB count. */
	unsigned int nal_bit_rate_value_minus1[VPU_METADATA_MAX_NUM_SUB_LAYER][VPU_METADATA_MAX_CPB_CNT];

	/** NAL CPB size values minus 1 for each sub-layer and CPB count. */
	unsigned int nal_cpb_size_value_minus1[VPU_METADATA_MAX_NUM_SUB_LAYER][VPU_METADATA_MAX_CPB_CNT];

	/** NAL CPB size DU value minus 1 for each sub-layer. */
	unsigned int nal_cpb_size_du_value_minus1[VPU_METADATA_MAX_NUM_SUB_LAYER];

	/** NAL bit rate DU value minus 1 for each sub-layer. */
	unsigned int nal_bit_rate_du_value_minus1[VPU_METADATA_MAX_NUM_SUB_LAYER];

	/** NAL constant bit rate flags for each sub-layer and CPB count. */
	unsigned char nal_cbr_flag[VPU_METADATA_MAX_NUM_SUB_LAYER][VPU_METADATA_MAX_CPB_CNT];

	/** VCL bit rate values minus 1 for each sub-layer and CPB count. */
	unsigned int vcl_bit_rate_value_minus1[VPU_METADATA_MAX_NUM_SUB_LAYER][VPU_METADATA_MAX_CPB_CNT];

	/** VCL CPB size values minus 1 for each sub-layer and CPB count. */
	unsigned int vcl_cpb_size_value_minus1[VPU_METADATA_MAX_NUM_SUB_LAYER][VPU_METADATA_MAX_CPB_CNT];

	/** VCL CPB size DU value minus 1 for each sub-layer. */
	unsigned int vcl_cpb_size_du_value_minus1[VPU_METADATA_MAX_NUM_SUB_LAYER];

	/** VCL bit rate DU value minus 1 for each sub-layer. */
	unsigned int vcl_bit_rate_du_value_minus1[VPU_METADATA_MAX_NUM_SUB_LAYER];

	/** VCL constant bit rate flags for each sub-layer and CPB count. */
	unsigned char vcl_cbr_flag[VPU_METADATA_MAX_NUM_SUB_LAYER][VPU_METADATA_MAX_CPB_CNT];

	/** Reserved values (17 elements) for future use. */
	unsigned int m_Reserved[17];
} vpu_metadata_hrd_param_t;


/**
 * @brief Structure for VPU metadata VUI parameter.
 */
typedef struct vpu_metadata_vui_param_t {
	/** @brief Aspect ratio information present flag. */
	unsigned char aspect_ratio_info_present_flag;

	/** @brief Aspect ratio IDC. */
	unsigned char aspect_ratio_idc;

	/** @brief Overscan information present flag. */
	unsigned char overscan_info_present_flag;

	/** @brief Overscan appropriate flag. */
	unsigned char overscan_appropriate_flag;

	/** @brief Video signal type present flag. */
	unsigned char video_signal_type_present_flag;

	/** @brief Video format. */
	char video_format;

	/** @brief Video full range flag. */
	unsigned char video_full_range_flag;

	/** @brief Colour description present flag. */
	unsigned char colour_description_present_flag;

	/** @brief Sample aspect ratio width. */
	unsigned short sar_width;

	/** @brief Sample aspect ratio height. */
	unsigned short sar_height;

	/** @brief Colour primaries. */
	unsigned char colour_primaries;

	/** @brief Transfer characteristics. */
	unsigned char transfer_characteristics;

	/** @brief Matrix coefficients. */
	unsigned char matrix_coefficients;

	/** @brief Chroma location information present flag. */
	unsigned char chroma_loc_info_present_flag;

	/** @brief Chroma sample location type for the top field. */
	char chroma_sample_loc_type_top_field;

	/** @brief Chroma sample location type for the bottom field. */
	char chroma_sample_loc_type_bottom_field;

	/** @brief Neutral chroma indication flag. */
	unsigned char neutral_chroma_indication_flag;

	/** @brief Field sequence flag. */
	unsigned char field_seq_flag;

	/** @brief Frame field information present flag. */
	unsigned char frame_field_info_present_flag;

	/** @brief Default display window flag. */
	unsigned char default_display_window_flag;

	/** @brief VUI timing information present flag. */
	unsigned char vui_timing_info_present_flag;

	/** @brief VUI POC proportional to timing flag. */
	unsigned char vui_poc_proportional_to_timing_flag;

	/** @brief VUI number of units in tick. */
	unsigned int vui_num_units_in_tick;

	/** @brief VUI time scale. */
	unsigned int vui_time_scale;

	/** @brief VUI HRD parameters present flag. */
	unsigned char vui_hrd_parameters_present_flag;

	/** @brief Bitstream restriction flag. */
	unsigned char bitstream_restriction_flag;

	/** @brief Tiles fixed structure flag. */
	unsigned char tiles_fixed_structure_flag;

	/** @brief Motion vectors over picture boundaries flag. */
	unsigned char motion_vectors_over_pic_boundaries_flag;

	/** @brief Restricted reference picture lists flag. */
	unsigned char restricted_ref_pic_lists_flag;

	/** @brief Minimum spatial segmentation IDC. */
	char min_spatial_segmentation_idc;

	/** @brief Maximum bytes per picture denominator. */
	char max_bytes_per_pic_denom;

	/** @brief Maximum bits per minCU denominator. */
	char max_bits_per_mincu_denom;

	/** @brief VUI number of ticks POC diff minus 1. */
	short vui_num_ticks_poc_diff_one_minus1;

	/** @brief Log2 max MV length horizontal. */
	char log2_max_mv_length_horizontal;

	/** @brief Log2 max MV length vertical. */
	char log2_max_mv_length_vertical;

	/** @brief Default display window. */
	vpu_metadata_win_t def_disp_win;

	/** @brief HRD parameter. */
	vpu_metadata_hrd_param_t hrd_param;

	/** @brief Reserved values (19 elements). */
	unsigned int m_Reserved[19];
} vpu_metadata_vui_param_t;


/**
 * @brief Structure for VPU metadata mastering display color volume.
 */
typedef struct vpu_metadata_mastering_display_colour_volume_t {
	/** @brief Display primaries x-coordinates for R, G, and B. */
	unsigned int display_primaries_x[3];

	/** @brief Display primaries y-coordinates for R, G, and B. */
	unsigned int display_primaries_y[3];

	/** @brief White point x-coordinate. */
	unsigned int white_point_x : 16;

	/** @brief White point y-coordinate. */
	unsigned int white_point_y : 16;

	/** @brief Maximum display mastering luminance. */
	unsigned int max_display_mastering_luminance : 32;

	/** @brief Minimum display mastering luminance. */
	unsigned int min_display_mastering_luminance : 32;

	/** @brief Reserved values (22 elements). */
	unsigned int m_Reserved[22];
} vpu_metadata_mastering_display_colour_volume_t;

/**
 * @brief Structure for VPU metadata chroma resampling filter hint.
 */
typedef struct vpu_metadata_chroma_resampling_filter_hint_t {
	/** @brief Vertical chroma filter IDC. */
	unsigned int ver_chroma_filter_idc : 8;

	/** @brief Horizontal chroma filter IDC. */
	unsigned int hor_chroma_filter_idc : 8;

	/** @brief Vertical filtering field processing flag. */
	unsigned int ver_filtering_field_processing_flag : 1;

	/** @brief Target format IDC. */
	unsigned int target_format_idc : 2;

	/** @brief Number of vertical filters. */
	unsigned int num_vertical_filters : 3;

	/** @brief Number of horizontal filters. */
	unsigned int num_horizontal_filters : 3;

	/** @brief Vertical tap length minus 1 for each filter. */
	unsigned char ver_tap_length_minus1[VPU_METADATA_MAX_NUM_VERTICAL_FILTERS];

	/** @brief Horizontal tap length minus 1 for each filter. */
	unsigned char hor_tap_length_minus1[VPU_METADATA_MAX_NUM_HORIZONTAL_FILTERS];

	/** @brief Vertical filter coefficients for each filter and tap. */
	int ver_filter_coeff[VPU_METADATA_MAX_NUM_VERTICAL_FILTERS][VPU_METADATA_MAX_TAP_LENGTH];

	/** @brief Horizontal filter coefficients for each filter and tap. */
	int hor_filter_coeff[VPU_METADATA_MAX_NUM_HORIZONTAL_FILTERS][VPU_METADATA_MAX_TAP_LENGTH];

	/** @brief Reserved values (24 elements). */
	unsigned int m_Reserved[24];
} vpu_metadata_chroma_resampling_filter_hint_t;


/**
 * @brief Structure for VPU metadata knee function information.
 */
typedef struct vpu_metadata_knee_function_info_t {
	/** @brief Knee function ID. */
	unsigned int knee_function_id;

	/** @brief Knee function cancel flag. */
	unsigned char knee_function_cancel_flag;

	/** @brief Knee function persistence flag. */
	unsigned char knee_function_persistence_flag;

	/** @brief Reserved values (2 elements). */
	unsigned char m_Reserved[2];

	/** @brief Input display luminance. */
	unsigned int input_disp_luminance;

	/** @brief Input dynamic range. */
	unsigned int input_d_range;

	/** @brief Output dynamic range. */
	unsigned int output_d_range;

	/** @brief Output display luminance. */
	unsigned int output_disp_luminance;

	/** @brief Number of knee points minus 1. */
	unsigned short num_knee_points_minus1;

	/** @brief Input knee points for each point. */
	unsigned short input_knee_point[VPU_METADATA_MAX_NUM_KNEE_POINT];

	/** @brief Output knee points for each point. */
	unsigned short output_knee_point[VPU_METADATA_MAX_NUM_KNEE_POINT];
} vpu_metadata_knee_function_info_t;

/**
 * @brief Structure for VPU metadata content light level information.
 */
typedef struct vpu_metadata_content_light_level_info_t {
	/** @brief Maximum content light level. */
	unsigned short max_content_light_level;

	/** @brief Maximum picture average light level. */
	unsigned short max_pic_average_light_level;
} vpu_metadata_content_light_level_info_t;


/**
 * @brief Structure for VPU metadata color remapping information.
 */
typedef struct vpu_metadata_colour_remapping_info_t {
	/** @brief Color remap ID. */
	unsigned int colour_remap_id;

	/** @brief Color remap cancel flag. */
	unsigned char colour_remap_cancel_flag;

	/** @brief Color remap persistence flag. */
	unsigned char colour_remap_persistence_flag;

	/** @brief Color remap video signal information present flag. */
	unsigned char colour_remap_video_signal_info_present_flag;

	/** @brief Color remap full range flag. */
	unsigned char colour_remap_full_range_flag;

	/** @brief Color remap primaries. */
	unsigned char colour_remap_primaries;

	/** @brief Color remap transfer function. */
	unsigned char colour_remap_transfer_function;

	/** @brief Color remap matrix coefficients. */
	unsigned char colour_remap_matrix_coefficients;

	/** @brief Color remap input bit depth. */
	unsigned char colour_remap_input_bit_depth;

	/** @brief Color remap bit depth. */
	unsigned char colour_remap_bit_depth;

	/** @brief Pre-LUT number of values minus 1 for each entry. */
	unsigned char pre_lut_num_val_minus1[VPU_METADATA_MAX_LUT_NUM_VAL];

	/** @brief Pre-LUT coded values for each entry. */
	unsigned short pre_lut_coded_value[VPU_METADATA_MAX_LUT_NUM_VAL][VPU_METADATA_MAX_LUT_NUM_VAL_MINUS1];

	/** @brief Pre-LUT target values for each entry. */
	unsigned short pre_lut_target_value[VPU_METADATA_MAX_LUT_NUM_VAL][VPU_METADATA_MAX_LUT_NUM_VAL_MINUS1];

	/** @brief Color remap matrix present flag. */
	unsigned char colour_remap_matrix_present_flag;

	/** @brief Log2 matrix denominator. */
	unsigned char log2_matrix_denom;

	/** @brief Color remap coefficients for each entry. */
	unsigned char colour_remap_coeffs[VPU_METADATA_MAX_COLOUR_REMAP_COEFFS][VPU_METADATA_MAX_COLOUR_REMAP_COEFFS];

	/** @brief Post-LUT number of values minus 1 for each entry. */
	unsigned char post_lut_num_val_minus1[VPU_METADATA_MAX_LUT_NUM_VAL];

	/** @brief Post-LUT coded values for each entry. */
	unsigned short post_lut_coded_value[VPU_METADATA_MAX_LUT_NUM_VAL][VPU_METADATA_MAX_LUT_NUM_VAL_MINUS1];

	/** @brief Post-LUT target values for each entry. */
	unsigned short post_lut_target_value[VPU_METADATA_MAX_LUT_NUM_VAL][VPU_METADATA_MAX_LUT_NUM_VAL_MINUS1];
} vpu_metadata_colour_remapping_info_t;


/**
 * @brief Structure for VPU metadata film grain characteristics information.
 */
typedef struct vpu_metadata_film_grain_characteristics_t {
	/** @brief Film grain characteristics cancel flag. */
	unsigned char film_grain_characteristics_cancel_flag;

	/** @brief Film grain model ID. */
	unsigned char film_grain_model_id;

	/** @brief Separate colour description present flag. */
	unsigned char separate_colour_description_present_flag;

	/** @brief Film grain bit depth luma minus 8. */
	unsigned char film_grain_bit_depth_luma_minus8;

	/** @brief Film grain bit depth chroma minus 8. */
	unsigned char film_grain_bit_depth_chroma_minus8;

	/** @brief Film grain full range flag. */
	unsigned char film_grain_full_range_flag;

	/** @brief Film grain colour primaries. */
	unsigned char film_grain_colour_primaries;

	/** @brief Film grain transfer characteristics. */
	unsigned char film_grain_transfer_characteristics;

	/** @brief Film grain matrix coefficients. */
	unsigned char film_grain_matrix_coeffs;

	/** @brief Blending mode ID. */
	unsigned char blending_mode_id;

	/** @brief Log2 scale factor. */
	unsigned char log2_scale_factor;

	/** @brief Component model present flag for each component. */
	unsigned char comp_model_present_flag[VPU_METADATA_MAX_NUM_FILM_GRAIN_COMPONENT];

	/** @brief Number of intensity intervals minus 1 for each component. */
	unsigned char num_intensity_intervals_minus1[VPU_METADATA_MAX_NUM_FILM_GRAIN_COMPONENT];

	/** @brief Number of model values minus 1 for each component. */
	unsigned char num_model_values_minus1[VPU_METADATA_MAX_NUM_FILM_GRAIN_COMPONENT];

	/** @brief Intensity interval lower bound for each component and interval. */
	unsigned char intensity_interval_lower_bound[VPU_METADATA_MAX_NUM_FILM_GRAIN_COMPONENT][VPU_METADATA_MAX_NUM_INTENSITY_INTERVALS];

	/** @brief Intensity interval upper bound for each component and interval. */
	unsigned char intensity_interval_upper_bound[VPU_METADATA_MAX_NUM_FILM_GRAIN_COMPONENT][VPU_METADATA_MAX_NUM_INTENSITY_INTERVALS];

	/** @brief Component model value for each component, interval, and model. */
	unsigned int comp_model_value[VPU_METADATA_MAX_NUM_FILM_GRAIN_COMPONENT][VPU_METADATA_MAX_NUM_INTENSITY_INTERVALS][VPU_METADATA_MAX_NUM_MODEL_VALUES];

	/** @brief Film grain characteristics persistence flag. */
	unsigned char film_grain_characteristics_persistence_flag;
} vpu_metadata_film_grain_characteristics_t;


/**
 * @brief Structure for VPU metadata tone mapping information.
 */
typedef struct vpu_metadata_tone_mapping_info_t {
	/** @brief Tone mapping ID. */
	unsigned int tone_map_id;

	/** @brief Tone mapping cancel flag. */
	unsigned char tone_map_cancel_flag;

	/** @brief Tone mapping persistence flag. */
	unsigned char tone_map_persistence_flag;

	/** @brief Coded data bit depth. */
	unsigned int coded_data_bit_depth;

	/** @brief Target bit depth. */
	unsigned int target_bit_depth;

	/** @brief Tone mapping model ID. */
	unsigned char tone_map_model_id;

	/** @brief Minimum value for tone mapping. */
	unsigned int min_value;

	/** @brief Maximum value for tone mapping. */
	unsigned int max_value;

	/** @brief Sigmoid midpoint for tone mapping. */
	unsigned int sigmoid_midpoint;

	/** @brief Sigmoid width for tone mapping. */
	unsigned int sigmoid_width;

	/** @brief Start of coded interval for tone mapping. */
	unsigned short start_of_coded_interval[VPU_METADATA_MAX_NUM_TONE_VALUE];

	/** @brief Number of pivots for tone mapping. */
	unsigned short num_pivots;

	/** @brief Coded pivot values for tone mapping. */
	unsigned short coded_pivot_value[VPU_METADATA_MAX_NUM_TONE_VALUE];

	/** @brief Target pivot values for tone mapping. */
	unsigned short target_pivot_value[VPU_METADATA_MAX_NUM_TONE_VALUE];

	/** @brief Camera ISO speed IDC for tone mapping. */
	unsigned char camera_iso_speed_idc;

	/** @brief Camera ISO speed value for tone mapping. */
	unsigned int camera_iso_speed_value;

	/** @brief Exposure index IDC for tone mapping. */
	unsigned char exposure_index_idc;

	/** @brief Exposure index value for tone mapping. */
	unsigned int exposure_index_value;

	/** @brief Exposure compensation value sign flag for tone mapping. */
	unsigned char exposure_compensation_value_sign_flag;

	/** @brief Exposure compensation value numerator for tone mapping. */
	unsigned short exposure_compensation_value_numerator;

	/** @brief Exposure compensation value denominator IDC for tone mapping. */
	unsigned short exposure_compensation_value_denom_idc;

	/** @brief Reference screen luminance white for tone mapping. */
	unsigned int ref_screen_luminance_white;

	/** @brief Extended range white level for tone mapping. */
	unsigned int extended_range_white_level;

	/** @brief Nominal black level code value for tone mapping. */
	unsigned short nominal_black_level_code_value;

	/** @brief Nominal white level code value for tone mapping. */
	unsigned short nominal_white_level_code_value;

	/** @brief Extended white level code value for tone mapping. */
	unsigned short extended_white_level_code_value;
} vpu_metadata_tone_mapping_info_t;


/**
 * @brief Structure for VPU metadata SEI picture timing.
 */
typedef struct vpu_metadata_sei_pic_timing_t {
	/** @brief Status for picture timing SEI. */
	char status;

	/** @brief Picture structure for picture timing SEI. */
	char pic_struct;

	/** @brief Source scan type for picture timing SEI. */
	char source_scan_type;

	/** @brief Duplicate flag for picture timing SEI. */
	char duplicate_flag;
} vpu_metadata_sei_pic_timing_t;


/**
 * @brief Structure for VPU metadata alternative transfer characteristics information.
 */
 typedef struct vpu_metadata_alternative_transfer_characteristics_info_t {
	/** @brief Preferred transfer characteristics for alternative transfer characteristics. */
	unsigned int preferred_transfer_characteristics;
} vpu_metadata_alternative_transfer_characteristics_info_t;


/**
 * @brief Structure for VPU metadata decoder user data information.
 */
typedef struct vpu_metadata_info_t {
	/** @brief SEI picture timing information. */
	vpu_metadata_sei_pic_timing_t sei_pic_timing;

	/** @brief VUI parameter information. */
	vpu_metadata_vui_param_t vui_param;

	/** @brief Mastering display color volume information. */
	vpu_metadata_mastering_display_colour_volume_t mastering_display_color_volume;

	/** @brief Content light level information. */
	vpu_metadata_content_light_level_info_t m_ContentLightLevelInfo;

	/** @brief Chroma resampling filter hint information. */
	vpu_metadata_chroma_resampling_filter_hint_t chroma_resampling_filter_hint;

	/** @brief Knee function information. */
	vpu_metadata_knee_function_info_t knee_function_info;

	/** @brief Tone mapping information. */
	vpu_metadata_tone_mapping_info_t tone_mapping_info;

	/** @brief Color remapping information. */
	vpu_metadata_colour_remapping_info_t colour_remapping_info;

	/** @brief Film grain characteristics information. */
	vpu_metadata_film_grain_characteristics_t film_grain_char_info;

	/** @brief Alternative transfer characteristics information. */
	vpu_metadata_alternative_transfer_characteristics_info_t alternative_transfer_characteristics_info;

	/** @brief Reserved values for future use. */
	unsigned int m_Reserved[22];
} vpu_metadata_info_t;

#endif //TCC_VPU_V3_DECODER_METADATA_H
