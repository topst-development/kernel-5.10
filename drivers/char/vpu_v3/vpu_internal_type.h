// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_INTERNAL_TYPE_H
#define VPU_INTERNAL_TYPE_H

#include <video/telechips/TCCxxxx_VPU_CODEC_COMMON.h>

#define VPU_DRV_ID_MAX			(32U)
#define INVALID_DRV_ID			(0xFF) //An ID value used to represent uninitialized driver IDs or invalid IDs.

//A feature to avoid pending issues with interlace frame
#define ENABLE_INTERLACE_DELAY_PROCESS //default enabled

//The VPU driver enables the processing of ring buffer mode internally
#define ENABLE_RINGBUFFER_MODE //default enabled

//The VPU driver enables automatic handling of frame skipping internally
#define ENABLE_AUTO_FRAMESKIP //default enabled

//The dec_input_t structure is used during sequence header initialization
#define ENABLE_SEQHEADER_BUFFER_CHANGE //default enabled


#ifndef phys_addr_t
#define phys_addr_t unsigned int
#endif

enum vpu_ip_type {
	VPU_IP_UNKNOWN = 0,
	VPU_IP_C7, //Coda960 => mpeg1/2, mpeg4, DivX, H.263, h264, VC-1, AVS, Boda950 => In vpu_c7, the encoder is excluded, and AVS is removed from the decoder
	VPU_IP_4KD2, //Wave512 => hevc, vp9 decode
	VPU_IP_HEVC_ENC, //Wave420L0 hevc encoder
	VPU_IP_HEVC_ENC2, //Wave420L1 hevc encoder2
	VPU_IP_JPU_C6, //CodaJ10 jpeg enc/dec
	VPU_IP_HEVC_DEC, //wave410
	//insert here new IP

	VPU_IP_MAX,
	VPU_IP_BOUND = 0x7FFFFFFF,
};

enum vpu_cq_type {
	VPU_CQ_LEGACY = 0,
	VPU_CQ_MULTI,

	VPU_CQ_MAX,
	VPU_CQ_BOUND = 0x7FFFFFFF,
};

enum vpu_op_type {
	VPU_OP_TYPE_DEC = 0,
	VPU_OP_TYPE_ENC,

	VPU_OP_TYPE_MAX,
	kVpuOpType_Bound = 0x7FFFFFFF,
};

enum vpu_cmd_type {
	VPU_CMD_DEC_INIT = 0,
	VPU_CMD_DEC_SEQ_HEADER,
	VPU_CMD_DEC_GET_INFO,
	VPU_CMD_DEC_REG_FRAME_BUFFER,
	VPU_CMD_DEC_REG_USER_FRAME_BUFFER,
	VPU_CMD_DEC_GET_OUTPUT_INFO,
	VPU_CMD_DEC_DECODE,
	VPU_CMD_DEC_BUF_FLAG_CLEAR,
	VPU_CMD_DEC_FLUSH,
	VPU_CMD_DEC_DRAIN,
	VPU_CMD_DEC_RING_GET_INFO,
	VPU_CMD_DEC_RING_SET_INFO,
	VPU_CMD_DEC_CLOSE,

	VPU_CMD_ENC_INIT,
	VPU_CMD_ENC_REG_FRAME_BUFFER,
	VPU_CMD_ENC_PUT_HEADER,
	VPU_CMD_ENC_ENCODE,
	VPU_CMD_ENC_CLOSE,

	VPU_CMD_TYPE_MAX,
	VPU_CMD_TYPE_BOUND = 0x7FFFFFFF,
};

enum vmgr_buffer_type {
	VMGR_BUF_BITSTREAM = 0,
	VMGR_BUF_NUM_OF_BITSTREAM,
	VMGR_BUF_BITWORK,
	VMGR_BUF_FRAMEBUF,
	VMGR_BUF_SPSPPS,
	VMGR_BUF_USERDATA,
	VMGR_BUF_SLICE,
	VMGR_BUF_MBDATA,
	VMGR_BUF_MESEARCH,
	VMGR_BUF_SLICEINFO,

	//add for user framebuffer register
	VMGR_BUF_Y, //linear
    VMGR_BUF_CB,
    VMGR_BUF_CR,
    VMGR_BUF_MVCOL,
    VMGR_BUF_FBCY,
    VMGR_BUF_FBCC,
    VMGR_BUF_COMP_Y, //compressed
    VMGR_BUF_COMP_C,

	VMGR_BUF_MAX,
	VMRR_BUF_BOUND = 0x7FFFFFFF
};

typedef enum {
	VPU_DEC = 0,
	VPU_DEC_EXT,
	VPU_DEC_EXT2,
	VPU_DEC_EXT3,
	VPU_DEC_EXT4,
	VPU_ENC,
	VPU_ENC_EXT,
	VPU_ENC_EXT2,
	VPU_ENC_EXT3,
	VPU_ENC_EXT4,
	VPU_ENC_EXT5,
	VPU_ENC_EXT6,
	VPU_ENC_EXT7,
	VPU_ENC_EXT8,
	VPU_ENC_EXT9,
	VPU_ENC_EXT10,
	VPU_ENC_EXT11,
	VPU_ENC_EXT12,
	VPU_ENC_EXT13,
	VPU_ENC_EXT14,
	VPU_ENC_EXT15,
	VPU_MAX
} vputype;

typedef struct vpu_drv_poll_t {
	wait_queue_head_t wq;
	atomic_t count;
} vpu_drv_poll_t;

//vmgr command managing structure
typedef struct vmgr_comm_t {
	struct mutex list_mutex;

	unsigned int thread_intr;
	wait_queue_head_t thread_wq;
	unsigned int cmd_queued;

	vpu_dllist_t *cmd_pool;
	vpu_dllist_t *cmd_q;
	vpu_dllist_t *wait_q[VPU_OP_TYPE_MAX][VPU_DRV_ID_MAX]; //each en/decoder for wait result only for cq2
	vpu_dllist_t *result_q[VPU_OP_TYPE_MAX][VPU_DRV_ID_MAX]; //each en/decoder result
} vmgr_comm_t;

typedef struct vpu_drv_shared_t {
	struct miscdevice misc;
	atomic_t reference_count; //how many en/decoders are opened
	struct mutex shared_mutex;
} vpu_drv_shared_t;

typedef struct vpu_pmap_info_t {
	int size;
	vpu_addr_t addr[VPU_ADDR_MAX];
} vpu_pmap_info_t;

typedef struct vpu_pmap_alloc_info_t {
	//from init
	vpu_pmap_info_t bitstream_buf;
	int num_of_bitstream_buffers;
	int bitstream_buffer_index;
	int size_of_bitstream_buffer;
	int bitstream_safearea_size; //safe area size in ringbuffer

	vpu_pmap_info_t bitwork_buf;

	//from register framebuffer
	//be cautious as the following addresses become invalid when the user framebuffer is enabled
	int additional_frame_buffer_count;
	int framebuffer_count;
	vpu_pmap_info_t frame_buf;

	//decoder only
	vpu_pmap_info_t spspps_buf; //avc, mvc only
	vpu_pmap_info_t userdata_buf;
	vpu_pmap_info_t slice_buf; //avc, mvc only
	vpu_pmap_info_t mbdata_buf; //for vp8
} vpu_pmap_alloc_info_t;

//a structure in which VPU enc/dec, vpu_mgr, and each vpu's IP managers share information
typedef struct vpu_drv_info_t {
	/**
	 * @brief Structure representing a node in the VPU linked list.
	 */
	vpu_dllist_node_t list;

	/**
	 * @brief Enumeration for the operation type of VPU.
	 */
	enum vpu_op_type op_type;

	/**
	 * @brief Enumeration for the codec ID of VPU.
	 */
	enum vpu_codec_id codec_id;

	/**
	 * @brief Enumeration for the IP type of VPU.
	 */
	enum vpu_ip_type ip_type;

	/**
	 * @brief Enumeration for the pmap type of VPU.
	 */
	enum vpu_pmap_type pmap_type;

	/**
	 * @brief Enc/dec driver ID, ranging from 0 to N, incrementing.
	 */
	unsigned int drv_id;

	/**
	 * @brief Indicates whether the VPU handle is open or not.
	 */
	bool opened;

	/**
	 * @brief Indicates whether the open request originated from kernel space.
	 *
	 * This variable is set to true if the open request was made from kernel space,
	 * and false if the request was made from user space.
	 */
	bool is_kernel_call;

	/**
	 * @brief Handle for the codec.
	 */
	codec_handle_t handle;

	/**
	 * @brief Allocation information for pmap.
	 */
	vpu_pmap_alloc_info_t pmap_alloc_info;

	/**
	 * @brief Initial information for each encoder/decoder.
	 */
	vdec_v3_init_in_t dec_init_info;
	venc_v3_init_in_t enc_init_info;

	/**
	 * @brief Initial information for vdec_v3.
	 */
	vdec_v3_initial_info_t initial_info;

	/**
	 * @brief When the ring buffer mode is enabled, it is set to 1.
	 */
	unsigned int enabled_ringbuffer_mode;

	/**
	 * @brief When the user register framebuffer is enabled, it is set to 1.
	 */
	unsigned int enable_user_register_framebuffer;

	/**
	 * @brief Represents the driver allocated in each IP's manager, allocated upon opening and released upon closing.
	 */
	void *ip_param;

	/**
	 * @brief pixel_product
	 *
	 * This variable stores the result of the pixel product calculated from width, height, and fps values.
	 * It is used to approximate the resources currently being utilized by the encoder or decoder.
	 * This value helps in assessing and monitoring the utilization of the Video Processing Unit (VPU).
	 */
	unsigned long pixel_product;

	/**
	 * @brief Enum defining the internal frame skip mode used for managing skip modes within the VPU.
	 *
	 * This enumeration represents the different internal frame skip modes used within the VPU to manage skipping of frames.
	 * The mode selected from this enumeration determines how frames are skipped during VPU processing.
	 */
	enum vpu_frame_skip_mode internal_skip_mode;

	/**
	 * @brief Width of the display used to detect resolution changes.
	 */
	int display_width;

	/**
	 * @brief Height of the display used to detect resolution changes.
	 */
	int display_height;

#if defined(ENABLE_INTERLACE_DELAY_PROCESS)
	unsigned int enable_interlace_delay_proc; //Whether delay processing for interlace handling is enabled or disabled is determined based on the user's choice.
	unsigned int detected_interlace; //when interlace decoding is detected, it is set to 1.
	unsigned int enable_avoid_pending; //If detected_interlace is 1, the process for avoid_pending starts from the first field of the next interlace.
	unsigned int avoid_pending; //if forced decoding is initiated to resolve the pending state, it is set to 1

	//At the initiation of the initial delay processing, the first input triggers a wake-up poll
	//the conditions for the value to become 1 are: before first decode, after flush, and after avoid_pending.
	unsigned int initial_wakeup_poll;

	vpu_dllist_t *delay_queue;
	void *temp_decoding_result; //Temporary storage of results after the pending state is resolved.
#endif

	/**
	 * @brief For debugging purposes: last processing command in manager.
	 */
	int last_cmd;

	/**
	 * @brief For debugging purposes: number of processed commands.
	 */
	long count_cmd;

	/**
	 * @brief For debugging purposes: start time of processing command.
	 */
	unsigned long cmd_start_time;

	/**
	 * @brief For debugging purposes: command processing time.
	 */
	long cmd_proc_time_us;
} vpu_drv_info_t;

//command structure from enc/dec driver to vpu_mgr
typedef struct vpu_cmd_t {
	vpu_dllist_node_t list;

	enum vpu_op_type op_type;
	unsigned int drv_id;	//enc/dec driver id
	vputype pmap_type;
	unsigned int cmd_id; //An unique ID that increments by 1 and cycles up to MAX_COMMAND_ID to differentiate commands

	enum vpu_cmd_type cmd_type;

	long handle;
	void *args;		//vpu argument!!
	int result;

	vpu_drv_poll_t *poll_data;
	vpu_drv_info_t *drv_info; //each driver's information
} vpu_cmd_t;

typedef struct vdec_v3_ringbuff_get_info_t {
	int result;

	unsigned int available_space;
	unsigned int read_physical_addr;
	unsigned int write_physical_addr;

	int reserved[32];
} vdec_v3_ringbuff_get_info_t;

typedef struct vdec_v3_ringbuff_set_info_t {
	int result;

	int written_byte;
	int is_flush;

	int reserved[31];
} vdec_v3_ringbuff_set_info_t;

#endif //VPU_INTERNAL_TYPE_H
