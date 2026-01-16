/*
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc.
* Contact: jayhouse@telechips.com
*/

#ifndef TCC_VPU_V3_COMMON_H
#define TCC_VPU_V3_COMMON_H

typedef long long vpu_addr_t;

/**
 * @brief Enum representing various return codes from VPU operations.
 */
enum vpu_return_code {
	VPU_RETCODE_SUCCESS = 0,					/**< (0) The operation was successful. */
	VPU_RETCODE_FAILURE,						/**< (1) The operation failed. */
	VPU_RETCODE_INSUFFICIENT_MEMORY,			/**< (2) Insufficient memory for the operation. */
	VPU_RETCODE_INVALID_PARAM,					/**< (3) Invalid parameters for the operation. */
	VPU_RETCODE_FRAME_NOT_COMPLETE,				/**< (4) Frame data is not complete for the operation. */
	VPU_RETCODE_INVALID_STRIDE,					/**< (5) Invalid stride value for the operation. */
	VPU_RETCODE_NOT_INITIALIZED,				/**< (6) The component is not initialized for the operation. */
	VPU_RETCODE_CODEC_FINISH,					/**< (7) The codec finished its operation (e.g., decoding). */
	VPU_RETCODE_CODEC_EXIT,						/**< (8) The codec exited its operation. */
	VPU_RETCODE_CODEC_SPECOUT,					/**< (9) An error code indicating a situation where an error occurred due to exceeding the specified specifications. */
	VPU_RETCODE_REPORT_NOT_READY,				/**< (10) The report is not ready for the operation. */
	VPU_RETCODE_MULTI_CODEC_EXIT_TIMEOUT,		/**< (11) Timeout occurred while waiting for multiple codecs to exit. */

	VPU_RETCODE_INFO_INSUFFICIENT_DATA,			/**< (12) Insufficient data for the operation. */

	VPU_RETCODE_MAX,							/**< Maximum value for vpu_codec_id enumeration. */
	VPU_RETCODE_BOUND = 0x7FFFFFFF				/**< Bound value for vpu_codec_id enumeration. */
};


/**
 * @enum vpu_codec_id
 * @brief Enumeration for various video codec IDs.
 * @details This enumeration lists the IDs for different video codecs supported by the VPU.
 */
enum vpu_codec_id {
	VCODEC_ID_NONE = 0,   /**< No video codec. */
	VCODEC_ID_AVC,        /**< Advanced Video Coding (H.264) codec. */
	VCODEC_ID_VC1,        /**< VC-1 video codec. */
	VCODEC_ID_MPEG2,      /**< MPEG-2 video codec. */
	VCODEC_ID_MPEG4,      /**< MPEG-4 video codec. */
	VCODEC_ID_H263,       /**< H.263 video codec. */
	VCODEC_ID_AVS,        /**< AVS video codec. */
	VCODEC_ID_MJPG,       /**< Motion JPEG codec. */
	VCODEC_ID_VP8,        /**< VP8 video codec. */
	VCODEC_ID_MVC,        /**< Multiview Video Coding codec. */
	VCODEC_ID_HEVC,       /**< High Efficiency Video Coding (H.265) codec. */
	VCODEC_ID_VP9,        /**< VP9 video codec. */

	VCODEC_ID_MAX,        /**< Maximum value for vpu_codec_id enumeration. */
	VCODEC_ID_BOUND = 0x7FFFFFFF /**< Bound value for vpu_codec_id enumeration. */
};

/**
 * @enum vpu_pmap_type
 * @brief Enumeration for different types of PMAP (Physical Memory Map) used by VPU.
 * @details This enumeration defines the types of PMAP (Physical Memory Map) used for decoding and encoding by VPU.
 */
enum vpu_pmap_type {
	VPU_PMAP_DEC = 0,
	VPU_PMAP_DEC_EXT,
	VPU_PMAP_DEC_EXT2,
	VPU_PMAP_DEC_EXT3,
	VPU_PMAP_DEC_EXT4,
	VPU_PMAP_ENC,
	VPU_PMAP_ENC_EXT,
	VPU_PMAP_ENC_EXT2,
	VPU_PMAP_ENC_EXT3,
	VPU_PMAP_ENC_EXT4,
	VPU_PMAP_ENC_EXT5,
	VPU_PMAP_ENC_EXT6,
	VPU_PMAP_ENC_EXT7,
	VPU_PMAP_ENC_EXT8,
	VPU_PMAP_ENC_EXT9,
	VPU_PMAP_ENC_EXT10,
	VPU_PMAP_ENC_EXT11,
	VPU_PMAP_ENC_EXT12,
	VPU_PMAP_ENC_EXT13,
	VPU_PMAP_ENC_EXT14,
	VPU_PMAP_ENC_EXT15,

	VPU_PMAP_MAX,
	VPU_PMAP_BOUND = 0x7FFFFFFF
};

/**
 * @enum vpu_address
 * @brief Enumeration for different types of VPU memory addresses.
 * @details This enumeration defines different types of VPU memory addresses, such as physical address and kernel virtual address.
 */
enum vpu_address {
	VPU_PA = 0, /**< Physical address. */
	VPU_KVA = 1, /**< Kernel virtual address. */

	VPU_ADDR_MAX, /**< Maximum value for vpu_address enumeration. */
	VPU_ADDR_BOUND = 0x7FFFFFFF /**< Bound value for vpu_address enumeration. */
};

/**
 * @enum vpu_component
 * @brief Enumeration for different components in VPU.
 * @details This enumeration defines different components within the VPU, such as Y (luma), U (chroma), and V (chroma) components.
 */
enum vpu_component {
	VPU_COMP_Y = 0, /**< Luma (Y) component. */
	VPU_CH_0 = VPU_COMP_Y,
	VPU_COMP_U = 1, /**< Chroma (U) component. */
	VPU_CH_1 = VPU_COMP_U,
	VPU_COMP_V = 2, /**< Chroma (V) component. */
	VPU_CH_2 = VPU_COMP_V,
	VPU_CH_3 = 3,

	VPU_COMP_MAX, /**< Maximum value for vpu_component enumeration. */
	VPU_COMP_BOUND = 0x7FFFFFFF /**< Bound value for vpu_component enumeration. */
};

/**
 * @enum vpu_yuv_source_format
 * @brief Enumeration for different YUV source formats used by VPU.
 * @details This enumeration defines various YUV source formats that can be used as input by the VPU.
 */
enum vpu_yuv_source_format {
	VPU_SOURCE_YUV420 = 0, /**< YUV 4:2:0 source format. */
	VPU_SOURCE_YUV422,     /**< YUV 4:2:2 source format. */
	VPU_SOURCE_YUV224,     /**< YUV 2:2:4 source format. */
	VPU_SOURCE_YUV400,     /**< YUV 4:0:0 source format. */
	VPU_SOURCE_YUV444,     /**< YUV 4:4:4 source format. */

	VPU_SOURCE_MAX,        /**< Maximum value for vpu_yuv_source_format enumeration. */
	VPU_SOURCE_BOUND = 0x7FFFFFFF /**< Bound value for vpu_yuv_source_format enumeration. */
};


/**
 * @enum vpu_output_format
 * @brief Enumeration for different output formats of VPU decoder.
 * @details This enumeration defines various output formats that the VPU decoder can produce.
 */
enum vpu_output_format {
	VPU_OUTPUT_LINEAR_YUV420 = 0,   /**< Linear YUV420 output format. Supports 10-bit or 8-bit (original). */
	VPU_OUTPUT_LINEAR_NV12,          /**< Linear NV12 output format. */
	VPU_OUTPUT_LINEAR_10_TO_8_BIT_YUV420, /**< Linear YUV420 output format with 10-bit to 8-bit conversion. */
	VPU_OUTPUT_LINEAR_10_TO_8_BIT_NV12,   /**< Linear NV12 output format with 10-bit to 8-bit conversion. */
	VPU_OUTPUT_COMPRESSED_MAPCONV,   /**< Compressed output format using map convertor. */
	VPU_OUTPUT_COMPRESSED_AFBC,      /**< Compressed output format using ARM frame buffer compression (AFBC). */

	VPU_OUTPUT_MAX,                  /**< Maximum value for vpu_output_format enumeration. */
	VPU_OUTPUT_BOUND = 0x7FFFFFFF   /**< Bound value for vpu_output_format enumeration. */
};

/**
 * @enum vpu_header_type
 * @brief Enumeration for different types of video codec headers.
 * @details This enumeration defines various header types used by different video codecs.
 */
enum vpu_header_type {
	VPU_HEADER_TYPE_0 = (1 << 0), /**< Header type 0. Used for H.264/AVC: SPS, H.265/HEVC: VPS, MPEG4: VOL. */
	VPU_HEADER_AVC_SPS = VPU_HEADER_TYPE_0, /**< AVC SPS header type. */
	VPU_HEADER_HEVC_VPS = VPU_HEADER_TYPE_0, /**< HEVC VPS header type. */
	VPU_HEADER_MPEG4_VOL = VPU_HEADER_TYPE_0, /**< MPEG4 VOL header type. */

	VPU_HEADER_TYPE_1 = (1 << 1), /**< Header type 1. Used for H.264/AVC: PPS, H.265/HEVC: SPS, MPEG4: VOS. */
	VPU_HEADER_AVC_PPS = VPU_HEADER_TYPE_1, /**< AVC PPS header type. */
	VPU_HEADER_HEVC_SPS = VPU_HEADER_TYPE_1, /**< HEVC SPS header type. */
	VPU_HEADER_MPEG4_VOS = VPU_HEADER_TYPE_1, /**< MPEG4 VOS header type. */

	VPU_HEADER_TYPE_2 = (1 << 2), /**< Header type 2. Used for H.264/AVC: (reserved), H.265/HEVC: PPS, MPEG4: VIS. */
	VPU_HEADER_HEVC_PPS = VPU_HEADER_TYPE_2, /**< HEVC PPS header type. */
	VPU_HEADER_MPEG4_VIS = VPU_HEADER_TYPE_2, /**< MPEG4 VIS header type. */

	VPU_HEADER_MAX,        /**< Maximum value for vpu_header_type enumeration. */
	VPU_HEADER_BOUND = 0x7FFFFFFF /**< Bound value for vpu_header_type enumeration. */
};

/**
 * @enum vpu_picture_type
 * @brief Enumeration for different types of video pictures.
 * @details This enumeration defines various types of video pictures, such as I-frames, P-frames, and B-frames.
 */
enum vpu_picture_type {
	VPU_PICTURE_I = 0, /**< I-frame picture type. */
	VPU_PICTURE_P,     /**< P-frame picture type. */
	VPU_PICTURE_B,     /**< B-frame picture type. */
	VPU_PICTURE_B_PB,  /**< B-frame picture type for MPEG-4 Packed PB-frame. */
	VPU_PICTURE_BI,    /**< BI-frame picture type for VC1. */
	VPU_PICTURE_IDR,   /**< IDR (Instantaneous Decoding Refresh) picture type. */
	VPU_PICTURE_SKIP,  /**< SKIP-frame picture type for VC1. */
	VPU_PICTURE_UNKNOWN,/**<Undefined picture type. */

	VPU_PICTURE_MAX,   /**< Maximum value for vpu_picture_type enumeration. */
	VPU_PICTURE_BOUND = 0x7FFFFFFF /**< Bound value for vpu_picture_type enumeration. */
};

/**
 * @enum vpu_frame_skip_mode
 * @brief Enumeration for different frame skip modes.
 * @details This enumeration defines different modes of frame skipping during video decoding.
 */
enum vpu_frame_skip_mode {
	VPU_FRAMESKIP_AUTO = 0,		/**< Automatic frame skipping. */
	VPU_FRAMESKIP_DISABLED, 	/**< Frame skipping disabled. */
	VPU_FRAMESKIP_NON_I,        /**< Frame skipping for non-I frames. */
	VPU_FRAMESKIP_B,            /**< Frame skipping for B-frames. */

	VPU_FRAMESKIP_MAX,          /**< Maximum value for vpu_frame_skip_mode enumeration. */
	VPU_FRAMESKIP_BOUND = 0x7FFFFFFF /**< Bound value for vpu_frame_skip_mode enumeration. */
};

/**
 * @enum jpu_scale_factor
 * @brief Enumeration for different scale factors in JPU (JPEG Processing Unit).
 * @details This enumeration defines various scale factors that can be applied to images
 *          processed by the JPEG Processing Unit (JPU).
 */
enum jpu_scale_factor {
	JPU_SCALE_FACTOR_1 = 0,        /**< 100% original scale factor. */
	JPU_SCALE_FACTOR_1_DIV_2,      /**< 50% scale factor. */
	JPU_SCALE_FACTOR_1_DIV_4,      /**< 25% scale factor. */
	JPU_SCALE_FACTOR_1_DIV_8,      /**< 12.5% scale factor. */

	JPU_SCALE_FACTOR_MAX,          /**< Maximum value for jpu_scale_factor enumeration. */
	JPU_SCALE_FACTOR_BOUND = 0x7FFFFFFF /**< Bound value for jpu_scale_factor enumeration. */
};

/**
 * @enum vpu_display_status
 * @brief Enumeration for display status after video decoding.
 * @details This enumeration defines different display status values that indicate
 *          the outcome of video decoding and its success or failure in terms of display.
 */
enum vpu_display_status {
	VPU_DISP_STAT_FAIL = 0,  /**< Display status indicating failure. */
	VPU_DISP_STAT_SUCCESS,    /**< Display status indicating success. */

	VPU_DISPLAY_MAX,          /**< Maximum value for vpu_display_status enumeration. */
	VPU_DISPLAY_BOUND = 0x7FFFFFFF /**< Bound value for vpu_display_status enumeration. */
};

/**
 * @enum vpu_decoded_status
 * @brief Enumeration for decoded frame status after video decoding.
 * @details This enumeration defines different decoded frame status values that provide
 *          information about the outcome of video decoding for a specific frame.
 */
enum vpu_decoded_status {
	VPU_DEC_STAT_NONE = 0, /**< (0) No specific status for the decoded frame. */
	VPU_DEC_STAT_SUCCESS, /**< (1) Decoding success for the frame. */
	VPU_DEC_STAT_INFO_NOT_SUFFICIENT_SPS_PPS_BUFF, /**< (2) Insufficient SPS/PPS buffer for decoding. */
	VPU_DEC_STAT_INFO_NOT_SUFFICIENT_SLICE_BUFF, /**< (3) Insufficient slice buffer for decoding. */
	VPU_DEC_STAT_BUF_FULL, /**< (4) Decoder buffer full during decoding. */
	VPU_DEC_STAT_SUCCESS_FIELD_PICTURE, /**< (5) Success for a field picture. */
	VPU_DEC_STAT_DETECT_RESOLUTION_CHANGE, /**< (6) Detected resolution change during decoding. */
	VPU_DEC_STAT_INVALID_INSTANCE, /**< (7) Invalid decoder instance for the frame. */
	VPU_DEC_STAT_DETECT_DPB_CHANGE, /**< (8) Detected DPB (Decoded Picture Buffer) change. */
	VPU_DEC_STAT_QUEUEING_FAIL, /**< (9) Failed to queue the frame for decoding. */
	VPU_DEC_STAT_VP9_SUPER_FRAME, /**< (10) Decoded VP9 super frame. */
	VPU_DEC_STAT_CQ_EMPTY, /**< (11) Command Queue (CQ) is empty. */
	VPU_DEC_STAT_REPORT_NOT_READY, /**< (12) Decode report not ready. */

	VPU_DEC_STAT_MAX, /**< (13) Maximum value for vpu_decoded_status enumeration. */
	VPU_DEC_STAT_BOUND = 0x7FFFFFFF /**< Bound value for vpu_decoded_status enumeration. */
};


/**
 * @enum vpu_bs_buffer_mode
 * @brief Enumeration for bitstream buffer modes in VPU (Video Processing Unit).
 * @details This enumeration defines different modes for handling bitstream buffers
 *          during video processing using the Video Processing Unit (VPU).
 */
enum vpu_bs_buffer_mode {
	VPU_BS_MODE_RINGBUFFER = (1 << 0), /**< Bitstream buffer mode: Ringbuffer. */
	VPU_BS_MODE_LINEARBUFFR = (1 << 1), /**< Bitstream buffer mode: Linear buffer. */

	VPU_BS_MODE_MAX,         /**< Maximum value for vpu_bs_buffer_mode enumeration. */
	VPU_BS_MODE_BOUND = 0x7FFFFFFF /**< Bound value for vpu_bs_buffer_mode enumeration. */
};


/**
 * @enum vpu_buffer_type
 * @brief Enumeration for different types of buffers used in VPU (Video Processing Unit).
 * @details This enumeration defines the various types of buffers that are utilized
 *          during the operation of the Video Processing Unit (VPU) for video processing tasks.
 */
enum vpu_buffer_type {
	VPU_BUFFER_ELSE,        /**< Generic buffer type */
	VPU_BUFFER_WORK,        /**< Work buffer */
	VPU_BUFFER_STREAM,      /**< Stream buffer */
	VPU_BUFFER_SEQHEADER,   /**< Sequence header buffer */
	VPU_BUFFER_FRAMEBUFFER, /**< Frame buffer */
	VPU_BUFFER_PS,          /**< Parameter set buffer */
	VPU_BUFFER_SLICE,       /**< Slice buffer */
	VPU_BUFFER_USERDATA,    /**< User data buffer */

	VPU_BUFFER_MAX,         /**< Maximum value for vpu_buffer_type enumeration. */
	VPU_BUFFER_BOUND = 0x7FFFFFFF /**< Bound value for vpu_buffer_type enumeration. */
};


/**
 * @enum vpu_framebuffer_type
 * @brief Enumeration for different types of framebuffers used in VPU (Video Processing Unit).
 * @details This enumeration defines the various types of framebuffers that are utilized
 *          during the operation of the Video Processing Unit (VPU) for video processing tasks.
 */
enum vpu_framebuffer_type {
	VPU_FRAMEBUFFER_Y = 0,       /**< Luma (Y) framebuffer */
	VPU_FRAMEBUFFER_CB,          /**< Chroma (Cb) framebuffer */
	VPU_FRAMEBUFFER_CR,          /**< Chroma (Cr) framebuffer */
	VPU_FRAMEBUFFER_MVCOL,       /**< Motion vector collation buffer */
	VPU_FRAMEBUFFER_FBCY,        /**< Framebuffer compression Y buffer */
	VPU_FRAMEBUFFER_FBCC,        /**< Framebuffer compression Cb/Cr buffer */
    VPU_FRAMEBUFFER_COMP_Y,      /**< compressed luma (Y) framebuffer */
    VPU_FRAMEBUFFER_COMP_C,  	 /**< compressed chroma (CbCr) framebuffer */
	VPU_FRAMEBUFFER_RESERVED_1,  /**< Reserved for future use */
	VPU_FRAMEBUFFER_RESERVED_2,  /**< Reserved for future use */
	VPU_FRAMEBUFFER_RESERVED_3,  /**< Reserved for future use */
	VPU_FRAMEBUFFER_RESERVED_4,  /**< Reserved for future use */

	VPU_FRAMEBUFFER_MAX,         /**< Maximum value for vpu_framebuffer_type enumeration. */
	VPU_FRAMEBUFFER_BOUND = 0x7FFFFFFF /**< Bound value for vpu_framebuffer_type enumeration. */
};


/**
 * @enum vpu_framebuffer_ext_type
 * @brief Enumeration for different types of extended framebuffers used in VPU (Video Processing Unit).
 * @details This enumeration defines the various types of extended framebuffers that are utilized
 *          during the operation of the Video Processing Unit (VPU) for handling specific video codecs.
 */
enum vpu_framebuffer_ext_type {
	VPU_FRAMEBUFFER_EXT_AVC_SLICE = 0, /**< AVC (H.264) slice buffer */
	VPU_FRAMEBUFFER_EXT_VP8_MBDATA,    /**< VP8 macroblock data buffer */

	VPU_FRAMEBUFFER_EXT_MAX,            /**< Maximum value for vpu_framebuffer_ext_type enumeration. */
	VPU_FRAMEBUFFER_EXT_BOUND = 0x7FFFFFFF /**< Bound value for vpu_framebuffer_ext_type enumeration. */
};




/**
 * @struct vpu_crop_t
 * @brief Structure defining cropping parameters for video processing.
 * @details This structure defines cropping parameters that can be applied to
 *          video frames during video processing operations.
 */
typedef struct vpu_crop_t {
	int left;    /**< Left cropping boundary. */
	int right;   /**< Right cropping boundary. */
	int top;     /**< Top cropping boundary. */
	int bottom;  /**< Bottom cropping boundary. */
} vpu_crop_t;

/**
 * @struct vpu_drv_version_t
 * @brief Structure for storing VPU driver version information.
 * @details This structure holds the version information of the VPU (Video Processing Unit) driver.
 */
typedef struct vpu_drv_version_t {
	unsigned int major;     /**< Major version number of the VPU driver. */
	unsigned int minor;     /**< Minor version number of the VPU driver. */
	unsigned int revision;  /**< Revision number of the VPU driver. */

	int reserved[13];       /**< Reserved padding for future use. */
} vpu_drv_version_t;


#define VPU_V3_MAX_CAP_STR  (64) /**< Maximum length for strings in vpu_codec_cap_t structure. */
#define VPU_V3_MAX_CAP  (64) /**< Maximum number of codec capabilities in vpu_capability_t structure. */

/**
 * @struct vpu_codec_cap_t
 * @brief Structure defining codec capabilities for the VPU.
 * @details This structure defines the capabilities of different video codecs supported by the
 *          Video Processing Unit (VPU).
 */
typedef struct vpu_codec_cap_t {
	enum vpu_codec_id codec_id;              /**< Identifier of the codec. */
	char support_codec[VPU_V3_MAX_CAP_STR];  /**< String representing supported codecs. */
	char support_profile[VPU_V3_MAX_CAP_STR]; /**< String representing supported profiles. */
	char support_level[VPU_V3_MAX_CAP_STR];   /**< String representing supported levels. */
	unsigned int max_width;                  /**< Maximum supported decoding width. */
	unsigned int max_height;                 /**< Maximum supported decoding height. */
	unsigned int max_fps;                    /**< Maximum supported frames per second. */

	enum vpu_bs_buffer_mode support_buffer_mode;        /**< Supported buffer modes. */

	int reserved[8];                        /**< Reserved padding for future use. */
} vpu_codec_cap_t;

/**
 * @struct vpu_capability_t
 * @brief Structure containing overall VPU capabilities.
 * @details This structure holds the overall capabilities of the Video Processing Unit (VPU),
 *          including codec capabilities and instance counts.
 */
typedef struct vpu_capability_t {
	unsigned int num_of_codec_cap;           /**< Number of codec capabilities. */
	vpu_codec_cap_t codec_caps[VPU_V3_MAX_CAP]; /**< Array of codec capabilities. */

	unsigned int max_supported_instance;     /**< Maximum supported instance count. */
	unsigned int available_instance;         /**< Available instance count. */

	unsigned int drv_id;                      /**< encoder/decoder driver id */

	int reserved[17];                         /**< Reserved padding for future use. */
} vpu_capability_t;


/**
 * @struct vpu_mem_alloc_t
 * @brief Structure for memory allocation within the VPU.
 * @details This structure is used for requesting and managing memory resources in the Video Processing Unit (VPU).
 *          It is used to allocate physical and kernel-remapped memory specific to buffer types defined by the VPU.
 * @warning When using this structure for allocation, please note that the VPU instance cannot be used for decoding purposes.
 *          If you intend to use the instance for decoding, you must close it and reinitialize it.
 */
typedef struct vpu_mem_alloc_t {
	enum vpu_codec_id codec;             /**< Codec type for which the memory is being allocated. */

	unsigned int request_size;           /**< The size of the memory request in bytes. */
	vpu_addr_t phy_addr;                 /**< Physical address of the allocated memory. */
	void *kernel_remap_addr;             /**< Kernel virtual address where the physical memory is mapped. */
	enum vpu_buffer_type buffer_type;    /**< Type of buffer for which memory is allocated, defined by vpu_buffer_type. */

	int reserved[10];                    /**< Reserved padding for future use, ensuring structure size alignment. */
} vpu_mem_alloc_t;


#endif //TCC_VPU_V3_COMMON_H
