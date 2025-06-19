// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_VPU_V3_ENCODER_H
#define TCC_VPU_V3_ENCODER_H

#include "tcc_vpu_v3_common.h"

/**
 * @defgroup VPU_Encoder_Init VPU Encoder Initialization
 * @brief Structures for initializing the VPU encoder.
 *
 * This group contains the structures used for initializing the VPU encoder with the required parameters.
 * These structures include options for rate control, picture quality, and other encoder settings.
 */

/**
 * @addtogroup VPU_Encoder_Init
 * @{
 */

/**
 * @struct venc_v3_rc_init_t
 * @brief Structure for initializing the VPU encoder's rate control settings.
 * @details This structure defines the initialization parameters for configuring the rate control settings
 *          of the Video Processing Unit (VPU) encoder.
 */
typedef struct venc_v3_rc_init_t
{
	int deblk_disable;           /**< Disable deblocking filter. */
	int deblk_alpha;             /**< Deblocking filter alpha value. */
	int deblk_beta;              /**< Deblocking filter beta value. */
	int deblk_ch_qp_offset;      /**< Deblocking filter chroma QP offset. */
	int avc_fast_encoding;       /**< Enable fast AVC encoding. */
	int constrained_intra;       /**< Enable constrained intra prediction. */
	int pic_qp_y;                /**< Picture quantization parameter for luminance. */
	int vbv_buffer_size;         /**< Video buffer verifier (VBV) buffer size. */
	int search_range;            /**< Search range for motion estimation. */
	int pvm_disable;             /**< Disable predictive motion vector (PVM). */
	int weight_intra_cost;       /**< Weighted intra prediction cost. */
	int rc_interval_mode;        /**< Rate control interval mode. */
	int rc_interval_mbnum;       /**< Rate control interval in macroblocks. */
	int intra_mb_refresh;        /**< Intra macroblock refresh interval. */
	int slice_mode;              /**< Slice mode for encoding. */
	int slice_size_mode;         /**< Slice size mode. */
	int slice_size;              /**< Size of each slice. */
	int enc_quality_level;       /**< Encoding quality level. */

	int initial_qp;              /**< Initial quantization parameter. */
	int intra_qp_min;            /**< Minimum quantization parameter for intra frames. */
	int intra_qp_max;            /**< Maximum quantization parameter for intra frames. */
	int inter_qp_min;            /**< Minimum quantization parameter for inter frames. */
	int inter_qp_max;            /**< Maximum quantization parameter for inter frames. */

	int reserved[16];            /**< Reserved padding for future use. */
} venc_v3_rc_init_t;


/**
 * @brief Option to insert Access Unit Delimiter (AUD) in the encoded bitstream.
 */
#define VENC_V3_OPTION_INSERT_AUD		(1<<0) //insert AUD

/**
 * @brief Structure for providing input parameters for VPU encoder initialization.
 */
typedef struct venc_v3_init_in_t
{
	/**
	 * @brief Codec ID to be used for encoding.
	 */
	enum vpu_codec_id codec_id;

	/**
	 * @brief YUV source format.
	 */
	enum vpu_yuv_source_format yuv_format;

	/**
	 * @brief Width of the input picture.
	 */
	int pic_width;

	/**
	 * @brief Height of the input picture.
	 */
	int pic_height;

	/**
	 * @brief Quality setting for JPEG encoding.
	 */
	int enc_quality; //jpeg

	/**
	 * @brief Frame rate for encoding.
	 */
	int frame_rate;

	/**
	 * @brief Target bitrate for encoding.
	 */
	int target_kbps;

	/**
	 * @brief Keyframe interval.
	 */
	int key_interval;

	/**
	 * @brief Chroma format interleave mode.
	 *
	 * If set to 1, chroma (Cb and Cr) components are interleaved in the bitstream.
	 * If set to 0, chroma components are stored in separate planes (separate format).
	 */
	unsigned int cbcr_interleave_mode;

	/**
	 * @brief Encoding options flags.
	 *
	 * Use VENC_OPTION_INSERT_AUD to enable inserting AUD.
	 */
	unsigned int enc_opt_flags;

	/**
	 * @brief Whether to use a forced PMAP index.
	 *
	 * If set to a non-zero value, the encoder will use the specified PMAP index
	 * (forced_pmap_idx) for encoding.
	 * If not used, the encoder will automatically allocate PMAP indices.
	 */
	unsigned int use_forced_pmap_idx;

	/**
	 * @brief Index of the forced PMAP to be used.
	 *
	 * User can modify PMAP indices, so this index information is provided.
	 * To modify PMAP, set the value to match the desired vpu_pmap_type enum.
	 */
	enum vpu_pmap_type forced_pmap_idx;

	/**
	 * @brief Whether to use specific rate control options.
	 */
	int use_specific_rc_option;

	/**
	 * @brief Rate control initialization parameters.
	 */
	venc_v3_rc_init_t rc;

	/**
	 * @brief User-specified bitstream buffer size.
	 */
	int user_bitstream_buf_size;

	int reserved[26];
} venc_v3_init_in_t;

/**
 * @brief Structure for getting output information after VPU encoder initialization.
 */
typedef struct venc_v3_init_out_t
{
	/**
	 * @brief Minimum required frame buffer count.
	 */
	int min_frame_buffer_count;

	/**
	 * @brief Minimum required frame buffer size.
	 */
	int min_frame_buffer_size;

	/**
	 * @brief Array of bitstream output addresses.
	 */
	vpu_addr_t bitstream_out[VPU_ADDR_MAX];

	/**
	 * @brief Size of the bitstream output.
	 */
	unsigned int bitstream_outsize;

	int reserved[13];
} venc_v3_init_out_t;

/**
 * @brief Overall structure for VPU encoder initialization.
 */
typedef struct venc_v3_init_t
{
	/**
	 * @brief The result of the encoder initialization.
	 *
	 * This member indicates the result of the venc_v3 initialization.
	 * If the initialization is successful, it returns VPU_RETURN_SUCCESS defined in tcc_vpu_v3_common.h.
	 * If the initialization fails, it returns an appropriate error code indicating the failure.
	 */
	enum vpu_return_code result;

	/**
	 * @brief Input parameters for the encoder initialization.
	 */
	venc_v3_init_in_t input;

	/**
	 * @brief Output information from the encoder initialization.
	 */
	venc_v3_init_out_t output;

	int reserved[7];
} venc_v3_init_t;

/**
 * @}
 */


/**
 * @defgroup VPU_Encoder_Put_Header VPU Encoder Put Header
 * @brief Structures for putting headers in the VPU encoder.
 *
 * This group contains the structures used for putting headers in the VPU encoder.
 * Headers can be used for storing sequence headers and other metadata.
 */

/**
 * @addtogroup VPU_Encoder_Put_Header
 * @{
 */

/**
 * @brief Structure for putting headers in the VPU encoder.
 */
typedef struct venc_v3_putheader_t
{
	/**
	 * @brief The result of the header insertion.
	 *
	 * This member indicates the result of the venc_v3_putheader function.
	 * If the header insertion is successful, it returns VPU_RETURN_SUCCESS defined in tcc_vpu_v3_common.h.
	 * If the insertion fails, it returns an appropriate error code indicating the failure.
	 */
	enum vpu_return_code result;

	/**
	 * @brief The type of the header being inserted.
	 *
	 * This member specifies the type of header being inserted using the vpu_header_type enum.
	 * Multiple header types can be combined using bitwise OR to put multiple headers at once.
	 * For example, you can use (VPU_HEADER_AVC_SPS | VPU_HEADER_AVC_PPS) to insert both SPS and PPS headers.
	 */
	enum vpu_header_type header_type;

	/**
	 * @brief Buffer addresses for storing the generated header data.
	 *
	 * This array holds VPU addresses where the header data, generated based on the user's configuration, will be stored.
	 */
	vpu_addr_t bitstream_buffer_addr[VPU_ADDR_MAX];

	/**
	 * @brief Size of the generated header data
	 */
	unsigned int bitstream_buffer_size;

	int reserved[15]; /**< Reserved padding for future use. */
} venc_v3_putheader_t;


/**
 * @}
 */


/**
 * @defgroup VPU_Encoder_Encoding VPU Encoder Encoding
 * @brief Structures for encoding with the VPU encoder.
 *
 * This group contains the structures used for encoding with the VPU encoder.
 * These structures include input and output information for the encoding process,
 * as well as rate control flags and information about the encoded stream.
 */

/**
 * @addtogroup VPU_Encoder_Encoding
 * @{
 */

/**
 * @brief Flag indicating the enabling of rate control in the VPU encoder.
 */
#define VENC_V3_RC_FLAG_ENABLE         (1 << 0)

/**
 * @brief Flag indicating the specification of bitrate in rate control settings.
 */
#define VENC_V3_RC_FLAG_BITRATE        (1 << 1)

/**
 * @brief Flag indicating the specification of framerate in rate control settings.
 */
#define VENC_V3_RC_FLAG_FRAMERATE      (1 << 2)

/**
 * @brief Flag indicating the specification of key frame interval in rate control settings.
 */
#define VENC_V3_RC_FLAG_KEY_INTERVAL   (1 << 3)


/**
 * @brief Structure for providing input parameters for VPU encoder encoding.
 */
typedef struct venc_v3_encode_in_t
{
	/**
	 * @brief Base address of the Y component of the input picture.
	 */
	unsigned int pic_y_addr;

	/**
	 * @brief Base address of the Cb component of the input picture.
	 */
	unsigned int pic_cb_addr;

	/**
	 * @brief Base address of the Cr component of the input picture.
	 */
	unsigned int pic_cr_addr;

	/**
	 * @brief Flag to force encoding an I-picture.
	 */
	int force_i_picture;

	/**
	 * @brief Flag to skip encoding the picture.
	 */
	int skip_picture;

	/**
	 * @brief Quantization parameter for the encoding.
	 */
	int quant_param;

	/**
	 * @brief Flag indicating changes in rate control parameters.
	 *
	 * This flag is used to indicate changes in rate control parameters for the encoding process.
	 */
	int change_rc_param_flag;

	/**
	 * @brief New target bitrate for rate control.
	 *
	 * This member is used when the change_rc_param_flag is set with (VENC_V3_RC_FLAG_ENABLE | VENC_V3_RC_FLAG_BITRATE).
	 */
	int change_target_kbps;

	/**
	 * @brief New target framerate for rate control.
	 *
	 * This member is used when the change_rc_param_flag is set with (VENC_V3_RC_FLAG_ENABLE | VENC_V3_RC_FLAG_FRAMERATE).
	 */
	int change_framerate;

	/**
	 * @brief New key frame interval for rate control.
	 *
	 * This member is used when the change_rc_param_flag is set with (VENC_V3_RC_FLAG_ENABLE | VENC_V3_RC_FLAG_KEY_INTERVAL).
	 */
	int change_key_interval;

	/**
	 * @brief Array of buffer addresses for storing the encoded bitstream.
	 */
	vpu_addr_t bitstream_buffer_addr[VPU_ADDR_MAX];

	/**
	 * @brief Size of the encoded bitstream buffer.
	 */
	unsigned int bitstream_buffer_size;

	int reserved[27]; /**< Reserved padding for future use. */
}venc_v3_encode_in_t;


/**
 * @brief Structure for getting information about the encoded stream.
 */
typedef struct venc_v3_encoded_info_t
{
	/**
	 * @brief Average quantization parameter used for encoding.
	 */
	int avg_qp;

	int reserved[15]; /**< Reserved padding for future use. */
}venc_v3_encoded_info_t;

/**
 * @brief Structure for getting output information after VPU encoder encoding.
 */
typedef struct venc_v3_encode_out_t
{
	/**
	 * @brief Type of the encoded picture (I-picture, P-picture, B-picture, etc.).
	 */
	enum vpu_picture_type pic_type;

	/**
	 * @brief Array of buffer addresses for storing the encoded stream.
	 */
	vpu_addr_t encoded_stream_addr[VPU_ADDR_MAX];

	/**
	 * @brief Size of the encoded stream.
	 */
	unsigned int encoded_stream_size;

	/**
	 * @brief Information about the encoded stream.
	 */
	venc_v3_encoded_info_t info;

	int reserved[14]; /**< Reserved padding for future use. */
} venc_v3_encode_out_t;

/**
 * @brief Overall structure for VPU encoder encoding.
 */
typedef struct venc_v3_encode_t
{
	/**
	 * @brief The result of the encoding process.
	 *
	 * This member indicates the result of the venc_v3_encode function.
	 * If the encoding is successful, it returns VPU_RETURN_SUCCESS defined in tcc_vpu_v3_common.h.
	 * If the encoding fails, it returns an appropriate error code indicating the failure.
	 */
	enum vpu_return_code result;

	/**
	 * @brief Input parameters for the encoding process.
	 */
	venc_v3_encode_in_t input;

	/**
	 * @brief Output information from the encoding process.
	 */
	venc_v3_encode_out_t output;

	int reserved[15]; /**< Reserved padding for future use. */
} venc_v3_encode_t;

/**
 * @}
 */


/**
 * @defgroup VPU_Encoder_Close VPU Encoder Close
 * @brief Structures for closing the VPU encoder.
 *
 * This group contains the structures used for closing the VPU encoder.
 * These structures include the result of the close operation and optional close options.
 */

/**
 * @addtogroup VPU_Encoder_Close
 * @{
 */

/**
 * @brief Structure for closing the VPU encoder.
 */
typedef struct venc_v3_close_t
{
	/**
	 * @brief The result of closing the VPU encoder.
	 *
	 * This member indicates the result of the venc_v3_close function.
	 * If the closing is successful, it returns VPU_RETURN_SUCCESS defined in tcc_vpu_v3_common.h.
	 * If the closing fails, it returns an appropriate error code indicating the failure.
	 */
	enum vpu_return_code result;

	/**
	 * @brief Option for closing the VPU encoder.
	 */
	unsigned int option;

	int reserved[14]; /**< Reserved padding for future use. */
} venc_v3_close_t;

/**
 * @}
 */

#endif //TCC_VPU_V3_ENCODER_H

