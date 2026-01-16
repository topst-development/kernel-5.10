/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#ifndef TCC_VPU_V3_DECODER_H
#define TCC_VPU_V3_DECODER_H

#include "tcc_vpu_v3_common.h"
#include "tcc_vpu_v3_decoder_metadata.h"

/**
 * @brief Flag indicating file skip in MPEG-4 GMC (Global Motion Compensation) mode.
 *
 * If set, indicates that file skipping occurs due to seq.init failure in MPEG-4 GMC mode.
 */
#define VDEC_V3_M4V_GMC_FILE_SKIP   (1<<0)

/**
 * @brief Flag indicating frame skip in MPEG-4 GMC (Global Motion Compensation) mode.
 *
 * If set, indicates that frame skipping occurs without decoding in MPEG-4 GMC mode.
 */
#define VDEC_V3_M4V_GMC_FRAME_SKIP  (1<<1)

/**
 * @brief Flag indicating field display for AVC (Advanced Video Coding) field input.
 *
 * If set, indicates that only a field is fed, and it should be displayed.
 */
#define VDEC_V3_AVC_FIELD_DISPLAY   (1<<2)

/**
 * @brief Flag enabling H.264 MVC (Multi-View Coding) decoding.
 *
 * If set, indicates the enabling of H.264 MVC decoding.
 */
#define VDEC_V3_MVC_DEC_ENABLE      (1<<3)

/**
 * @brief Flag for adjusting frame buffer size during resolution change.
 *
 * If set, indicates that the frame buffer size should be adjusted during streaming playback
 * when the resolution changes.
 */
#define VDEC_V3_USE_MAX_FRAMEBUFFER (1<<4)

/**
 * @brief ignore buffer-delay on VPU
 *
 * If set, this flag instructs the VPU to ignore buffer delay.
 */
#define VDEC_V3_NO_BUFFER_DELAY (1<<5)

/**
 * @brief Flag to avoid pending situations during interlace processing.
 *
 * If set, this flag enables a process to avoid pending situations during interlace processing.
 */

#define VDEC_V3_AVOID_PENDING (1<<6)


/**
 * @brief Maximum registered buffer count.
 *
 * The maximum buffer count is 32. When using a codec that supports compression
 * and requires linear output, the count needs to be doubled, so the maximum is set to 64.
 */
#define VDEC_V3_MAX_REG_BUFFER_COUNT			(64)



/**
 * @defgroup VPU_Decoder_Init VPU Decoder Initialization
 * @brief Initialization structures for the VPU decoder.
 *
 * This group contains the structures required for initializing the VPU decoder.
 * The VPU decoder initialization involves setting various configuration parameters,
 * enabling specific features, and defining buffer sizes for input and output data.
 */

/**
 * @addtogroup VPU_Decoder_Init
 * @{
 */

/**
 * @struct vdec_v3_init_in_t
 * @brief Structure for configuring VPU decoder initialization parameters.
 */
typedef struct vdec_v3_init_in_t {
	/**
	 * @brief The codec type to be used (e.g., VCODEC_ID_AVC).
	 */
	enum vpu_codec_id codec_id;

	/**
	 * @brief Maximum supported decoding width.
	 */
	unsigned int max_support_width;

	/**
	 * @brief Maximum supported decoding height.
	 */
	unsigned int max_support_height;

	/**
	 * @brief Output format specification.
	 */
	enum vpu_output_format output_format;

	/**
	 * @brief Additional frame count for allocating framebuffers.
	 */
	unsigned int additional_frame_count;

	/**
	 * @brief Whether to use a forced PMAP index.
	 *
	 * If set to a non-zero value, the decoder will use the specified PMAP index
	 * (forced_pmap_idx) for decoding.
	 * If not used, the decoder will automatically allocate PMAP indices.
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
	 * @brief Flags defined by preprocessor macros for vpu decoder options.
	 */
	unsigned int dec_opt_flags;


	/**
	 * @brief VPU driver configuration for operation.
	 */

	/**
	 * @brief Enable interlace processing for handling top and bottom fields together.
	 */
	unsigned int enable_interlace_processing;

	/**
	 * @brief Enable or disable user data processing.
	 */
	unsigned int enable_user_data;

	/**
	 * @brief Enable or disable the ringbuffer mode for VPU decoder.
	 *
	 * By enabling the ringbuffer mode, the VPU decoder operates in ringbuffer mode.
	 */
	unsigned int enable_ringbuffer_mode;

	/**
	 * @brief Enable or disable the use of a DMA buffer ID.
	 *
	 * When enable_dma_buf_id is set, it allows the utilization of a specific DMA buffer ID.
	 */
	unsigned int enable_dma_buf_id;

	/**
	 * @brief User-specified bitstream buffer size.
	 */
	int user_bitstream_buf_size;

	/**
	 * @brief User-specified userdata buffer size.
	 */
	int user_userdata_buf_size;

	/**
	 * @brief Determines whether to force the use of the VPU IP specified by force_vpu_ip_index.
	 */
	int enable_force_vpu_ip;

	/**
	 * @brief Forces the use of a specific VPU IP by specifying its index.
	 */
	int force_vpu_ip_index;

	int reserved[5];  /**< Reserved padding for future use. */
} vdec_v3_init_in_t;

/**
 * @struct vdec_v3_init_out_t
 * @brief Structure containing initialization output information for VPU decoder.
 */
typedef struct vdec_v3_init_out_t {
	/**
	 * @brief An array of VPU addresses representing the base addresses
	 *        for the bitstream buffers.
	 *
	 * The user needs to calculate the offset by using the next_bitstream_addr
	 * received from dec_init as the base address, along with the next input
	 * address obtained from the sequence header and decode results. They should
	 * then write the data to that position.
	 */
	vpu_addr_t next_bitstream_buf_addr[VPU_ADDR_MAX];

		/**
	 * @brief The size of the next_bitstream_buf_addr array.
	 */
	unsigned int next_bitstream_buf_size;

	/**
	 * @brief An array of VPU addresses representing the base addresses
	 *        for the user data buffers.
	 */
	vpu_addr_t userdata_buf_addr[VPU_ADDR_MAX];

		/**
	 * @brief The size of the userdata_buf_addr array.
	 */
	int userdata_buf_size;

	/**
	 * @brief The variable indicating whether the system is in ring buffer mode.
	 *
	 *	If the system is operating in ring buffer mode, this variable holds the value 1; otherwise, it holds 0.
	*/
	unsigned int is_ringbuffer_mode;

	int reserved[5];  /**< Reserved padding for future use. */
} vdec_v3_init_out_t;

/**
 * @struct vdec_v3_init_t
 * @brief Structure containing initialization parameters for VPU decoder.
 */
typedef struct vdec_v3_init_t {
	/**
	 * @brief The result of the sequence header processing.
	 *
	 * This member indicates the result of the vdec_v3 sequence header processing.
	 * If the processing is successful, it returns VPU_RETURN_SUCCESS defined in tcc_vpu_v3_common.h.
	 * If the processing fails, it returns an appropriate error code indicating the failure.
	 */
	enum vpu_return_code result;

	/**
	 * @brief The input parameters for the vdec_v3 initialization.
	 */
	vdec_v3_init_in_t input;

	/**
	 * @brief The output information from the vdec_v3 initialization.
	 */
	vdec_v3_init_out_t output;

	int reserved[4];  /**< Reserved padding for future use. */
} vdec_v3_init_t;

/**
 * @}
 */




/**
 * @defgroup VPU_Decoder_SeqHeader VPU Decoder Sequence Header Initialization
 * @brief Initialization structures for the VPU decoder's sequence header.
 *
 * This group contains the structures required for initializing the VPU decoder's sequence header.
 * The VPU decoder sequence header initialization involves setting various configuration parameters,
 * providing buffer addresses and sizes, and specifying profile, level, and tier information.
 */

/**
 * @addtogroup VPU_Decoder_SeqHeader
 * @{
 */

/**
* @struct vdec_v3_avc_vui_info_t
* @brief The following structures contain information used for initializing sequence headers.
*/

typedef struct vdec_v3_avc_vui_info_t {
	/**
	 * @brief Flag indicating whether AVC video full range is used.
	 */
	int avc_vui_video_full_range_flag;

	/**
	 * @brief Colour primaries information for AVC VUI.
	 */
	int avc_vui_colour_primaries;

	/**
	 * @brief Transfer characteristics information for AVC VUI.
	 */
	int avc_vui_transfer_characteristics;

	/**
	 * @brief Matrix coefficients information for AVC VUI.
	 */
	int avc_vui_matrix_coefficients;

	/**
	 * @brief Video format information for AVC VUI.
	 */
	int avc_vui_video_format;

	/**
	* @brief Video_signal_type_present_flag and color_description_present_flag for AVC VUI.
	*   Bit [0] video_signal_type_present_flag
	*    0 : If the 'video_signal_type_present_flag' equals zero, there's no VUI in SPS.
	*    1 : encode vui info.
	*   Bit [2] color_description_present_flag
	*    0 : If the 'color_description_present_flag' equals zero, there's no color description info.
	*        (color_primaries, transfer_characteristics, matrix_coeffs).
	*    1 : encode color description info.
	*/
	int avc_vui_video_signal_present_flags;

	int reserved[10];  /**< Reserved padding for future use. */
} vdec_v3_avc_vui_info_t;

/**
 * @struct vdec_v3_mpeg2_seq_display_ext_info_t
 * @brief Structure containing additional display information for MPEG-2 sequence.
 */
typedef struct vdec_v3_mpeg2_seq_display_ext_info_t {
	/**
	 * @brief Colour primaries information for MPEG-2 sequence display extension.
	 */
	int mp2_color_primaries;

	/**
	 * @brief Transfer characteristics information for MPEG-2 sequence display extension.
	 */
	int mp2_transfer_characteristics;

	/**
	 * @brief Matrix coefficients information for MPEG-2 sequence display extension.
	 */
	int mp2_matrix_coefficients;

	int reserved[13];  /**< Reserved padding for future use. */
} vdec_v3_mpeg2_seq_display_ext_info_t;

/**
 * @struct vdec_v3_mjpeg_specific_info_t
 * @brief Structure containing specific information for MJPEG decoding.
 */
typedef struct vdec_v3_mjpeg_specific_info_t {
	/**
	 * @brief MJPEG source chroma format.
	 */
	enum vpu_yuv_source_format mjpg_source_format;

	/**
	 * @brief Flag indicating whether a thumbnail image exists.
	 *
	 * If a thumbnail image exists, this field is set.
	 */
	int mjpg_thumbnail_enable;

	/**
	 * @brief Minimum frame buffer size for JPEG only.
	 *
	 * The possible values are:
	 * - 0: Original Size
	 * - 1: 1/2 Scaling Down
	 * - 2: 1/4 Scaling Down
	 * - 3: 1/8 Scaling Down
	 */
	int mjpg_min_frameBufferSize[4];

	unsigned int reserved[10];  /**< Reserved padding for future use. */
} vdec_v3_mjpeg_specific_info_t;

/**
 * @struct vdec_v3_initial_info_t
 * @brief Structure containing initial information for VPU decoding.
 */
typedef struct vdec_v3_initial_info_t {
	/**
	 * @brief Picture width.
	 */
	int pic_width;

	/**
	 * @brief Picture height.
	 */
	int pic_height;

	/**
	 * @brief Numerator of the frame rate.
	 */
	unsigned int frame_rate_res;

	/**
	 * @brief Denominator of the frame rate.
	 */
	unsigned int frame_rate_div;

	/**
	 * @brief Minimum required frame buffer count.
	 */
	int min_frame_buffer_count;

	/**
	 * @brief Minimum required frame buffer size.
	 *
	 * If the format is MJPEG, refer to mjpeg_spec_info.mjpg_min_frameBufferSize.
	 */
	int min_frame_buffer_size;

	/**
	 * @brief Frame buffer format, where 10-bit format is used.
	 */
	int frame_buffer_format;

	/**
	 * @brief Picture cropping information.
	 */
	vpu_crop_t pic_crop;

	/**
	 * @brief Electro-Optical Transfer Function (EOTF) information.
	 */
	unsigned int eotf;

	/**
	 * @brief Frame buffer delay.
	 */
	int frame_buf_delay;

	/**
	 * @brief Profile information.
	 */
	int profile;

	/**
	 * @brief Level information.
	 */
	int level;

	/**
	 * @brief Tier information.
	 */
	int tier;

	/**
	 * @brief Interlace mode.
	 */
	int interlace;

	/**
	 * @brief Aspect ratio information.
	 */
	int aspectratio;

	/**
	 * @brief report error reasons.
	 */
	int report_error_reason;

	/**
	 * @brief Bit depth information.
	 */
	int bitdepth;

	/**
	 * @brief AVC VUI (Video Usability Information) related information.
	 */
	vdec_v3_avc_vui_info_t avc_vui_info;

	/**
	 * @brief MPEG-2 sequence display extension related information.
	 */
	vdec_v3_mpeg2_seq_display_ext_info_t mpeg2_disp_info;

	/**
	 * @brief MJPEG-specific information.
	 */
	vdec_v3_mjpeg_specific_info_t mjpg_spec_info;

	/**
	 * @brief Metadata flag.
	 *
	 * This flag indicates the presence of metadata associated with the video stream.
	 * If set to 1, it indicates that metadata is present. If set to 0, it indicates
	 * that no metadata is associated with the video stream.
	 */
	unsigned int metadata;

	/** @brief Metadata information. */
	vpu_metadata_info_t metadata_info;

	int reserved[4];  /**< Reserved padding for future use. */
} vdec_v3_initial_info_t ;

/**
 * @struct vdec_v3_seqheader_in_t
 * @brief Structure containing sequence header input information for VPU decoding.
 */
typedef struct vdec_v3_seqheader_in_t {
	/**
	 * @brief Array of VPU addresses representing the bitstream buffers.
	 *
	 * The bitstream data used for decoding is provided as an array of VPU addresses.
	 * These addresses point to the locations where the bitstream data is stored.
	 */
	vpu_addr_t bitstream_addr[VPU_ADDR_MAX];

	/**
	 * @brief Size of the bitstream data.
	 */
	unsigned int bitstream_size;

	/**
	 * @brief Enables user registration of framebuffers.
	 *
	 * When enabled, this field requires the user to allocate each framebuffer based on the output
	 * information provided by vdec_v3_seqheader_out_t after processing the VDEC_V3_SEQ_HEADER.
	 * Subsequently, the user must register these framebuffers using the VDEC_V3_REG_FRAMEBUFFER ioctl
	 * and the vdec_v3_reg_framebuffer_t structure before decoding can proceed.
	 */
	unsigned int enable_user_register_framebuffer;

	/**
	 * @brief Scale factor information for MJPEG.
	 *
	 * If the format is MJPEG, this scale factor is used to specify the scaling factor
	 * for decoding. This allows the decoder to perform downscaling on the MJPEG image.
	 */
	enum jpu_scale_factor scale_factor;

	int reserved[9];  /**< Reserved padding for future use. */
} vdec_v3_seqheader_in_t;

/**
 * @struct vdec_v3_buffer_size_info_t
 * @brief Structure containing buffer size information for VPU decoding.
 */
typedef struct vdec_v3_buffer_size_info_t {
	/**
	 * @brief Sizes of each framebuffer.
	 *
	 * This array represents the sizes of each framebuffer used in the decoding process.
	 * It specifies the amount of memory allocated for each framebuffer.
	 * The array size is defined by VPU_FRAMEBUFFER_MAX, indicating the maximum number of framebuffers.
	 */
	int framebuffer_size[VPU_FRAMEBUFFER_MAX];

	/**
	 * @brief Sizes of extended framebuffers.
	 *
	 * This array represents the sizes of extended framebuffers used in the decoding process.
	 * It specifies the amount of memory allocated for each extended framebuffer.
	 * The array size is defined by VPU_FRAMEBUFFER_EXT_MAX, indicating the maximum number of extended framebuffers.
	 */
	int framebuffer_ext_size[VPU_FRAMEBUFFER_EXT_MAX];

	/**
	 * @brief Reserved padding for 64-byte alignment.
	 *
	 * This field is used to pad the structure to ensure 64-byte alignment.
	 * This padding can be used for future extensions or reserved for alignment purposes.
	 */
	int reserved[5]; /**< Reserved padding for future use and 64-byte alignment. */

} vdec_v3_buffer_size_info_t;


/**
 * @struct vdec_v3_seqheader_out_t
 * @brief Structure containing sequence header output information for VPU decoding.
 */
typedef struct vdec_v3_seqheader_out_t {
	/**
	 * @brief Initial information obtained from the sequence header.
	 *
	 * This structure holds the initial information extracted from the sequence header,
	 * which is used to configure the decoder for subsequent decoding operations.
	 */
	vdec_v3_initial_info_t initial_info;

	/**
	 * @brief Buffer size information for VPU decoding.
	 *
	 * This structure contains the sizes of various buffers required for VPU decoding,
	 * including the size of each framebuffer, AVC slice, and VP8 macroblock data.
	 */
	vdec_v3_buffer_size_info_t buffer_size_info;

	/**
	 * @brief Array of VPU addresses representing the allocated framebuffers.
	 *
	 * The decoder allocates framebuffers to hold decoded frames. This array of VPU addresses
	 * points to the locations where these framebuffers are stored.
	 */
	vpu_addr_t framebuf_addr[VPU_ADDR_MAX];

	/**
	 * @brief Size of the allocated framebuffers.
	 */
	int framebuf_size;

	/**
	 * @brief Array of VPU addresses for the next bitstream buffer.
	 *
	 * The user needs to check the size of this address and copy the input stream
	 * to that address. This address is used as the next input bitstream for decoding.
	 */
	vpu_addr_t next_bitstream_buf_addr[VPU_ADDR_MAX];

	/**
	 * @brief Size of the next bitstream buffer.
	 */
	int next_bitstream_buf_size;

	int reserved[4];  /**< Reserved padding for future use. */
} vdec_v3_seqheader_out_t;

/**
 * @struct vdec_v3_seqheader_t
 * @brief Structure containing sequence header processing information for VPU decoding.
 */
typedef struct vdec_v3_seqheader_t {
	/**
	 * @brief The result of the sequence header processing.
	 *
	 * This member indicates the result of the vdec_v3 sequence header processing.
	 * If the processing is successful, it returns VPU_RETURN_SUCCESS defined in tcc_vpu_v3_common.h.
	 * If the processing fails, it returns an appropriate error code indicating the failure.
	 */
	enum vpu_return_code result;

	/**
	 * @brief The input parameters for the vdec_v3 sequence header processing.
	 *
	 * This structure holds the input parameters that are provided for the vdec_v3
	 * sequence header processing. These parameters determine how the decoder processes
	 * the sequence header data.
	 */
	vdec_v3_seqheader_in_t input;

	/**
	 * @brief The output information from the vdec_v3 sequence header processing.
	 *
	 * This structure contains the output information that is generated as a result
	 * of the vdec_v3 sequence header processing. It includes various configuration details,
	 * allocated framebuffers, and other relevant information.
	 */
	vdec_v3_seqheader_out_t output;

	int reserved[4];  /**< Reserved padding for future use. */
} vdec_v3_seqheader_t;

/**
 * @}
 */


/**
 * @struct vdec_v3_framebuffer_info_t
 * @brief Structure containing information about the framebuffers used in VPU decoding.
 *
 * This structure holds the addresses and sizes of the individual buffer types within the framebuffers
 * used in VPU decoding.
 */
typedef struct vdec_v3_framebuffer_info_t {
	/**
	 * @brief Array of addresses for different types of framebuffers.
	 *
	 * An array storing the addresses for various types of buffers used in the VPU decoding process.
	 * The size of the array is defined by VPU_ADDR_MAX.
	 */
	vpu_addr_t framebuffer[VPU_ADDR_MAX];

	/**
	 * @brief Size of the framebuffer.
	 *
	 * This member indicates the size of the individual buffer types within the framebuffer.
	 */
	unsigned int size;

	/**
	 * @brief Aligned width of the framebuffer.
	 *
	 * This member indicates the width of the framebuffer, aligned to the required boundary.
	 */
	int aligned_width;

	/**
	 * @brief Aligned height of the framebuffer.
	 *
	 * This member indicates the height of the framebuffer, aligned to the required boundary.
	 */
	int aligned_height;

	/**
	 * @brief Framebuffer format.
	 *
	 * This member specifies the format of the framebuffer.
	 */
	int format;

	/**
	 * @brief Reserved padding for future use.
	 *
	 * This member is reserved for future use and ensures the structure is 64-byte aligned.
	 */
	int reserved[5];  /**< Reserved padding to ensure 64-byte alignment. */

} vdec_v3_framebuffer_info_t;

typedef struct vdec_v3_reg_framebuffer_in_t {
	/**
	 * @brief The count of framebuffers to be registered.
	 *
	 * Specifies the number of framebuffers that are registered for the VPU decoding process.
	 * This count should not exceed the maximum buffer count specified by VDEC_V3_MAX_REG_BUFFER_COUNT.
	 */
	unsigned int frame_buffer_count;

	/**
	 * @brief Addresses of the framebuffers.
	 *
	 * A multi-dimensional array that stores the addresses for different types of data within the framebuffers.
	 * The dimensions are organized as follows:
	 * [Buffer Count][Buffer Type].
	 * The types of buffers are defined by the vpu_framebuffer_type enumeration, and the size of each type of buffer
	 * within a framebuffer is specified. The types include Y, Cr, Cb, MVCol, CompressedY, CompressedCb, FbcYOffsetAddr,
	 * and FbcCOffsetAddr.
	 */
	vdec_v3_framebuffer_info_t frameBuffer[VDEC_V3_MAX_REG_BUFFER_COUNT][VPU_FRAMEBUFFER_MAX];

	/**
	 * @brief Additional buffer information.
	 *
	 * This member holds additional buffer information that may be required for specific use cases or future extensions.
	 * The types of additional buffers are defined by the vpu_framebuffer_ext_type enumeration, including
	 * AvcSliceSaveBuffer and Vp8MbDataSaveBuffer.
	 */
	vdec_v3_framebuffer_info_t framebuffer_ext[VPU_FRAMEBUFFER_EXT_MAX];
} vdec_v3_reg_framebuffer_in_t;

/**
 * @struct vdec_v3_reg_framebuffer_t
 * @brief Structure containing framebuffer registration information for VPU decoding.
 *
 * This structure is used to register framebuffers with the VPU decoder, specifying
 * the count and types of buffers, along with their sizes and addresses.
 */
typedef struct vdec_v3_reg_framebuffer_t {
	/**
	 * @brief The result of the framebuffer registration process.
	 *
	 * This member indicates the result of the framebuffer registration. It returns VPU_RETURN_SUCCESS
	 * if the registration is successful. If the registration fails, it returns an appropriate error code
	 * defined in tcc_vpu_v3_common.h.
	 */
	enum vpu_return_code result;

	vdec_v3_reg_framebuffer_in_t input;

	int reserved[2];   /**< Reserved padding for future use. */
} vdec_v3_reg_framebuffer_t;


/**
 * @defgroup VPU_Decoder_Decoding VPU Decoder Decoding Information
 * @brief Structures for decoding information used by the VPU decoder.
 *
 * This group contains the structures used for decoding video frames using the VPU decoder.
 * These structures include input and output information, codec-specific data, and frame information.
 */

/**
 * @addtogroup VPU_Decoder_Decoding
 * @{
 */

/**
* @struct vdec_v3_decode_in_t
* @brief The following structures contain information used for decoding.
*/

typedef struct vdec_v3_decode_in_t {
	/**
	 * @brief Array of VPU addresses representing the primary bitstream buffer.
	 *
	 * The primary bitstream data used for decoding is provided as an array of VPU addresses.
	 * These addresses point to the locations where the bitstream data is stored.
	 */
	vpu_addr_t bitstream_addr[VPU_ADDR_MAX];

	/**
	 * @brief Size of the primary bitstream data.
	 */
	int bitstream_size;

	/**
	 * @brief Array of VPU addresses representing a secondary bitstream buffer.
	 *
	 * An alternative bitstream buffer can be provided for continuous decoding.
	 */
	vpu_addr_t bitstream_addr_2[VPU_ADDR_MAX];

	/**
	 * @brief Size of the secondary bitstream data.
	 */
	unsigned int bitstream_size_2;

	/**
	 * @brief frame skip mode for decoding.
	 *
	 * If set to VPU_FRAMESKIP_AUTO, the driver automatically controls the skip mode.
	 * When a flush is called, the auto skip mode is reactivated.
	 * If set to any other value, the user must manually control the skip mode.
	 *
	 * The basic operation of the auto skip mode is as follows:
	 * After encountering a non-I-frame (VPU_FRAMESKIP_NON_I),
	 * when an I-frame is detected, the mode transitions to VPU_FRAME_SKIP_B and
	 * subsequently to VPU_FRAMESKIP_DISABLED.
	 */
	enum vpu_frame_skip_mode skip_mode;

	/**
	 * @brief Flags indicating various options for decoding.
	 *
	 * The option_flags field allows users to set different options for the decoding process.
	 * Users can manipulate these flags to enable or disable specific features or behaviors during decoding.
	 *
	 */
	unsigned int option_flags;

	int reserved[4];  /**< Reserved padding for future use. */
} vdec_v3_decode_in_t;

/**
 * @struct vdec_v3_specific_info_t
 * @brief Structure containing specific information for VPU decoding.
 */
typedef struct vdec_v3_specific_info_t {
	/**
	 * @brief MPEG-2 specific field sequence information.
	 */
	int m2v_field_sequence;

	/**
	 * @brief MPEG-2 specific framerate information.
	 */
	int m2v_framerate;

	/**
	 * @brief Picture structure information.
	 */
	int picture_structure;

	/**
	 * @brief Top field first information.
	 */
	int top_field_first;

	int reserved[12];  /**< Reserved padding for future use. */
} vdec_v3_specific_info_t;

/**
 * @struct vdec_v3_mapconv_info_t
 * @brief Structure containing specific information for map convertor in VPU decoding.
 */
typedef struct vdec_v3_mapconv_info_t {
	/**
	 *  @brief Array of addresses for compressed Y data (2 planes).
	 */
	vpu_addr_t compressed_y[VPU_ADDR_MAX];

	/**
	 * @brief Array of addresses for compressed Cb data (2 planes).
	 */
	vpu_addr_t compressed_cb[VPU_ADDR_MAX];

	/**
	 * @brief Array of addresses for FBC Y offset data (2 planes).
	 */
	vpu_addr_t fbc_y_offset_addr[VPU_ADDR_MAX];

	/**
	 * @brief Array of addresses for FBC C offset data (2 planes).
	 */
	vpu_addr_t fbc_c_offset_addr[VPU_ADDR_MAX];

	/**
	 * @brief Size of compression table for luma data.
	 */
	unsigned int compression_table_luma_size;

	/**
	 * @brief Size of compression table for chroma data.
	 */
	unsigned int compression_table_chroma_size;

	/**
	 * @brief Stride of luma data.
	 */
	unsigned int luma_stride;

	/**
	 * @brief Stride of chroma data.
	 */
	unsigned int chroma_stride;

	/**
	 * @brief Bit depth of luma data.
	 */
	unsigned int luma_bit_depth;

	/**
	 * @brief Bit depth of chroma data.
	 */
	unsigned int chroma_bit_depth;

	 /**
	  * @brief Endianness of the frame.
	  */
	unsigned int frame_endian;

	/**
	 * @brief Reserved padding for future use.
	 */
	unsigned int reserved[17];
} vdec_v3_mapconv_info_t;

/**
 * @struct vdec_v3_output_info_t
 * @brief Structure containing output information for vdec_v3 decoding.
 */
typedef struct vdec_v3_output_info_t {
	/**
	 * @brief Type of the picture.
	 * @details Refers to the values of enum vpu_picture_type.
	 *          For an interlaced frame, the upper 3 bits represent the type of the top field,
	 *          while the lower 3 bits represent the type of the bottom field.
	 *          The format is | 3 bits (top) | 3 bits (bottom) |.
	 */
	int pic_type;

	/**
	 * @brief Index of the displayed frame.
	 */
	int display_idx;

	/**
	 * @brief Index of the decoded frame.
	 */
	int decoded_idx;

	/**
	 * @brief Display status of the frame.
	 */
	enum vpu_display_status display_status;

	/**
	 * @brief Decoded status of the frame.
	 */
	enum vpu_decoded_status decoded_status;

	/**
	 * @brief Flag indicating whether the frame is interlaced.
	 */
	int interlaced_frame;

	/**
	 * @brief Number of error macroblocks in the decoded frame.
	 */
	int num_of_err_mbs;

	/**
	 * @brief Width of the decoded frame.
	 */
	int decoded_width;

	/**
	 * @brief Height of the decoded frame.
	 */
	int decoded_height;

	/**
	 * @brief Width of the displayed frame.
	 */
	int display_width;

	/**
	 * @brief Height of the displayed frame.
	 */
	int display_height;

	/**
	 * @brief Cropping information for the decoded frame.
	 */
	vpu_crop_t decoded_crop;

	/**
	 * @brief Cropping information for the displayed frame.
	 */
	vpu_crop_t display_crop;

	/**
	 * @brief DMA buffer alignment width adjusted according to DMA buf align.
	 *        This width is calculated to meet the requirements of DMA buffer alignment.
	 */
	unsigned int dma_buf_align_width;

	/**
	 * @brief DMA buffer alignment height adjusted according to DMA buf align.
	 *        This width is calculated to meet the requirements of DMA buffer alignment.
	 */
	unsigned int dma_buf_align_height;

	/**
	 * @brief Array of VPU addresses representing the user data buffer.
	 */
	vpu_addr_t userdata_buf_addr[VPU_ADDR_MAX];

	/**
	 * @brief Size of the user data buffer.
	 */
	int userdata_buffer_size;

	/**
	 * @brief Codec-specific information related to decoding.
	 */
	vdec_v3_specific_info_t specific_info;

	/**
	 * @brief Structure containing information about compressed frames for display in map converter.
	 */
	vdec_v3_mapconv_info_t disp_map_conv_info;

	int reserved[4];  /**< Reserved padding for future use. */
} vdec_v3_output_info_t;

/**
 * @struct vdec_v3_decode_out_t
 * @brief Structure containing output information for vdec_v3 decoding.
 */

typedef struct vdec_v3_decode_out_t {
	/**
	 * @brief Array of pointers to the display output buffers.
	 *
	 * This array holds pointers to the display output buffers for each component.
	 */
	unsigned char *display_out[VPU_ADDR_MAX][VPU_COMP_MAX];

	/**
	 * @brief Array of pointers to the decoded output buffers.
	 *
	 * This array holds pointers to the decoded output buffers for each component.
	 */
	unsigned char *decoded_out[VPU_ADDR_MAX][VPU_COMP_MAX];

	/**
	 * @brief Identifier of the DMA buffer.
	 *        It should reference and use out_info's dma_buf_align_width and dma_buf_align_height values.
	 */
	int dma_buf_id;

	/**
	 * @brief Array of VPU addresses for the next bitstream buffer.
	 *
	 * The user needs to check the size of this address and copy the input stream
	 * to that address. This address is used as the next input bitstream for decoding.
	 */
	vpu_addr_t next_bitstream_buf_addr[VPU_ADDR_MAX];

	/**
	 * @brief Size of the next bitstream buffer.
	 */
	int next_bitstream_buf_size;

	/**
	 * @brief Output information related to the decoded frame.
	 */
	vdec_v3_output_info_t out_info;

	int reserved[4];  /**< Reserved padding for future use. */
} vdec_v3_decode_out_t;

/**
 * @struct vdec_v3_decode_t
 * @brief Structure containing decode result, input, and output for vdec_v3.
 */
typedef struct vdec_v3_decode_t {
	/**
	 * @brief The result of the decoding operation.
	 *
	 * This member indicates the result of the vdec_v3 decoding operation.
	 * If the processing is successful, it returns VPU_RETURN_SUCCESS defined in tcc_vpu_v3_common.h.
	 * If the processing fails, it returns an appropriate error code indicating the failure.
	 */
	enum vpu_return_code result;

	/**
	 * @brief The input parameters for the vdec_v3 decoding operation.
	 *
	 * This structure holds the input parameters that are provided for the vdec_v3
	 * decoding operation. These parameters determine how the decoder processes the input data.
	 */
	vdec_v3_decode_in_t input;

	/**
	 * @brief The output information from the vdec_v3 decoding operation.
	 *
	 * This structure contains the output information that is generated as a result
	 * of the vdec_v3 decoding operation. It includes various frame data, bitstream buffers,
	 * and other relevant information related to the decoding process.
	 */
	vdec_v3_decode_out_t output;

	int reserved[8];  /**< Reserved padding for future use. */
} vdec_v3_decode_t;

/**
 * @}
 */



/**
 * @defgroup VPU_Buffer_Clear VPU Buffer Clear
 * @brief Structures for clearing VPU buffers.
 *
 * This group contains the structures used for clearing VPU buffers during certain operations.
 * The VPU buffer clearing process involves setting specific parameters for clearing buffers.
 */

/**
 * @addtogroup VPU_Buffer_Clear
 * @{
 */

/**
 * @struct vdec_v3_buf_clear_t
 * @brief Structure for buffer clearing information in VPU decoding.
 */
typedef struct vdec_v3_buf_clear_t {
	/**
	 * @brief The result of the buffer clearing operation.
	 *
	 * This member indicates the result of the buffer clearing operation in vdec_v3.
	 * If the clearing is successful, it returns VPU_RETURN_SUCCESS defined in tcc_vpu_v3_common.h.
	 * If the clearing fails, it returns an appropriate error code indicating the failure.
	 */
	enum vpu_return_code result;

	/**
	 * @brief The index of the buffer to be cleared.
	 *
	 * This member indicates the index of the buffer that is intended to be cleared.
	 */
	int index;

	/**
	 * @brief Valid DMA buffer identifier to release.
	 */
	int dma_buf_id;

	int reserved[13];  /**< Reserved padding for future use. */
} vdec_v3_buf_clear_t;

/**
 * @}
 */


/**
 * @defgroup VPU_Decoder_Buffer_Drain VPU Decoder Buffer Drain
 * @brief Structures for draining the VPU decoder's buffer.
 *
 * This group contains the structures used for draining the VPU decoder's buffer during certain operations.
 * The buffer draining process involves obtaining the output information after decoding.
 */

/**
 * @addtogroup VPU_Decoder_Buffer_Drain
 * @{
 */

/**
 * @struct vdec_v3_drain_t
 * @brief Structure for draining the VPU decoder's buffer.
 */
typedef struct vdec_v3_drain_t {
	/**
	 * @brief The result of the draining operation.
	 *
	 * This member indicates the result of the vdec_v3 draining operation.
	 * If the operation is successful, it returns VPU_RETURN_SUCCESS defined in tcc_vpu_v3_common.h.
	 * If the operation fails, it returns an appropriate error code indicating the failure.
	 */
	enum vpu_return_code result;

	/**
	 * @brief The output information from the decoding operation.
	 *
	 * This structure holds the output information generated as a result of the decoding operation.
	 * It includes details about the decoded frames and related information.
	 */
	vdec_v3_decode_out_t output;

	/**
	 * @brief Option for the draining operation.
	 *
	 * This member specifies an option for the draining operation, allowing for additional control.
	 */
	unsigned int option;

	int reserved[7];  /**< Reserved padding for future use. */
} vdec_v3_drain_t;
/**
 * @}
 */


/**
 * @defgroup VPU_Decoder_Buffer_Flush VPU Decoder Buffer Flush
 * @brief Structures for flushing the VPU decoder's buffer.
 *
 * This group contains the structures used for flushing the VPU decoder's buffer during certain operations.
 * The buffer flushing process involves setting specific parameters for flushing the buffer.
 */

/**
 * @addtogroup VPU_Decoder_Buffer_Flush
 * @{
 */

/**
 * @struct vdec_v3_flush_t
 * @brief Structure for flushing the VPU decoder's buffer.
 */
typedef struct vdec_v3_flush_t {
	/**
	 * @brief The result of the flushing operation.
	 *
	 * This member indicates the result of the vdec_v3 flushing operation.
	 * If the operation is successful, it returns VPU_RETURN_SUCCESS defined in tcc_vpu_v3_common.h.
	 * If the operation fails, it returns an appropriate error code indicating the failure.
	 */
	enum vpu_return_code result;

	/**
	 * @brief Option for the flushing operation.
	 *
	 * This member specifies an option for the flushing operation, allowing for additional control.
	 */
	unsigned int option;

	int reserved[14];  /**< Reserved padding for future use. */
} vdec_v3_flush_t;
/**
 * @}
 */


/**
 * @defgroup VPU_Decoder_Close VPU Decoder Close
 * @brief Structures for closing the VPU decoder.
 *
 * This group contains the structures used for closing the VPU decoder during certain operations.
 * The VPU decoder closing process involves setting specific parameters for the closing operation.
 */

/**
 * @addtogroup VPU_Decoder_Close
 * @{
 */

/**
 * @struct vdec_v3_close_t
 * @brief Structure for closing VPU decoding.
 */
typedef struct vdec_v3_close_t {
	/**
	 * @brief The result of the closing operation.
	 *
	 * This member indicates the result of the vdec_v3 closing operation.
	 * If the operation is successful, it returns VPU_RETURN_SUCCESS defined in tcc_vpu_v3_common.h.
	 * If the operation fails, it returns an appropriate error code indicating the failure.
	 */
	enum vpu_return_code result;

	/**
	 * @brief Option for the closing operation.
	 *
	 * This member specifies an option for the closing operation, allowing for additional control.
	 */
	unsigned int option;

	int reserved[14];  /**< Reserved padding for future use. */
} vdec_v3_close_t;
/**
 * @}
 */


/**
 * @addtogroup VPU_Decoder_Get_Next_Result
 * @{
 */

/**
 * @struct vdec_v3_get_next_result_t
 * @brief Structure containing information about the result of getting the next output from VPU decoder.
 */
typedef struct vdec_v3_get_next_result_t {
	/**
	 * @brief The result of the "get next output" operation.
	 *
	 * Possible error codes are defined in tcc_vpu_v3_common.h using VPU_RETURN_XXXX macros.
	 */
	enum vpu_return_code result;

	/**
	 * @brief Command to retrieve the next result.
	 *
	 * This field specifies the command to be used to retrieve the next output result
	 * from the VPU decoder.
	 */
	int next_result_cmd;

	/**
	 * @brief Number of remaining output results.
	 *
	 * This field indicates the number of output results
	 */
	int remain_result;

	int reserved[13];  /**< Reserved padding for future use. */
} vdec_v3_get_next_result_t;
/**
 * @}
 */


#endif //TCC_VPU_V3_DECODER_H

