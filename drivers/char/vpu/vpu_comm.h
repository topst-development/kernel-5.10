// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_COMM_H
#define VPU_COMM_H

#include <video/telechips/tcc_vpu_wbuffer.h>

/*****************************************************************************
 * VIDEO DECODER COUNT DEFINITION
 */
#if defined(CONFIG_VDEC_CNT_5)
#define DEFINED_CONFIG_VDEC_CNT_5 1
#else
#define DEFINED_CONFIG_VDEC_CNT_5 0
#endif

#if defined(CONFIG_VDEC_CNT_4)
#define DEFINED_CONFIG_VDEC_CNT_4 1
#else
#define DEFINED_CONFIG_VDEC_CNT_4 0
#endif

#if defined(CONFIG_VDEC_CNT_3)
#define DEFINED_CONFIG_VDEC_CNT_3 1
#else
#define DEFINED_CONFIG_VDEC_CNT_3 0
#endif

#if defined(CONFIG_VDEC_CNT_2)
#define DEFINED_CONFIG_VDEC_CNT_2 1
#else
#define DEFINED_CONFIG_VDEC_CNT_2 0
#endif

#if defined(CONFIG_VDEC_CNT_1)
#define DEFINED_CONFIG_VDEC_CNT_1 1
#else
#define DEFINED_CONFIG_VDEC_CNT_1 0
#endif

#define DEFINED_CONFIG_VDEC_CNT_45 \
	(DEFINED_CONFIG_VDEC_CNT_4 || DEFINED_CONFIG_VDEC_CNT_5)

#define DEFINED_CONFIG_VDEC_CNT_345 \
	(DEFINED_CONFIG_VDEC_CNT_3 || DEFINED_CONFIG_VDEC_CNT_45)

#define DEFINED_CONFIG_VDEC_CNT_2345 \
	(DEFINED_CONFIG_VDEC_CNT_2 || DEFINED_CONFIG_VDEC_CNT_345)

#define DEFINED_CONFIG_VDEC_CNT_12345 \
	(DEFINED_CONFIG_VDEC_CNT_1 || DEFINED_CONFIG_VDEC_CNT_2345)

/*****************************************************************************
 * VIDEO ENCODER COUNT DEFINITION
 */
#if defined(CONFIG_VENC_CNT_16)
#define DEFINED_CONFIG_VENC_CNT_16 1
#else
#define DEFINED_CONFIG_VENC_CNT_16 0
#endif

#if defined(CONFIG_VENC_CNT_15)
#define DEFINED_CONFIG_VENC_CNT_15 1
#else
#define DEFINED_CONFIG_VENC_CNT_15 0
#endif

#if defined(CONFIG_VENC_CNT_14)
#define DEFINED_CONFIG_VENC_CNT_14 1
#else
#define DEFINED_CONFIG_VENC_CNT_14 0
#endif

#if defined(CONFIG_VENC_CNT_13)
#define DEFINED_CONFIG_VENC_CNT_13 1
#else
#define DEFINED_CONFIG_VENC_CNT_13 0
#endif

#if defined(CONFIG_VENC_CNT_12)
#define DEFINED_CONFIG_VENC_CNT_12 1
#else
#define DEFINED_CONFIG_VENC_CNT_12 0
#endif

#if defined(CONFIG_VENC_CNT_11)
#define DEFINED_CONFIG_VENC_CNT_11 1
#else
#define DEFINED_CONFIG_VENC_CNT_11 0
#endif

#if defined(CONFIG_VENC_CNT_10)
#define DEFINED_CONFIG_VENC_CNT_10 1
#else
#define DEFINED_CONFIG_VENC_CNT_10 0
#endif

#if defined(CONFIG_VENC_CNT_9)
#define DEFINED_CONFIG_VENC_CNT_9 1
#else
#define DEFINED_CONFIG_VENC_CNT_9 0
#endif

#if defined(CONFIG_VENC_CNT_8)
#define DEFINED_CONFIG_VENC_CNT_8 1
#else
#define DEFINED_CONFIG_VENC_CNT_8 0
#endif

#if defined(CONFIG_VENC_CNT_7)
#define DEFINED_CONFIG_VENC_CNT_7 1
#else
#define DEFINED_CONFIG_VENC_CNT_7 0
#endif

#if defined(CONFIG_VENC_CNT_6)
#define DEFINED_CONFIG_VENC_CNT_6 1
#else
#define DEFINED_CONFIG_VENC_CNT_6 0
#endif

#if defined(CONFIG_VENC_CNT_5)
#define DEFINED_CONFIG_VENC_CNT_5 1
#else
#define DEFINED_CONFIG_VENC_CNT_5 0
#endif

#if defined(CONFIG_VENC_CNT_4)
#define DEFINED_CONFIG_VENC_CNT_4 1
#else
#define DEFINED_CONFIG_VENC_CNT_4 0
#endif

#if defined(CONFIG_VENC_CNT_3)
#define DEFINED_CONFIG_VENC_CNT_3 1
#else
#define DEFINED_CONFIG_VENC_CNT_3 0
#endif

#if defined(CONFIG_VENC_CNT_2)
#define DEFINED_CONFIG_VENC_CNT_2 1
#else
#define DEFINED_CONFIG_VENC_CNT_2 0
#endif

#if defined(CONFIG_VENC_CNT_1)
#define DEFINED_CONFIG_VENC_CNT_1 1
#else
#define DEFINED_CONFIG_VENC_CNT_1 0
#endif

#define DEFINED_CONFIG_VENC_CNT_15to16 \
	(DEFINED_CONFIG_VENC_CNT_15 || DEFINED_CONFIG_VENC_CNT_16)

#define DEFINED_CONFIG_VENC_CNT_14to16 \
	(DEFINED_CONFIG_VENC_CNT_14 || DEFINED_CONFIG_VENC_CNT_15to16)

#define DEFINED_CONFIG_VENC_CNT_13to16 \
	(DEFINED_CONFIG_VENC_CNT_13 || DEFINED_CONFIG_VENC_CNT_14to16)

#define DEFINED_CONFIG_VENC_CNT_12to16 \
	(DEFINED_CONFIG_VENC_CNT_12 || DEFINED_CONFIG_VENC_CNT_13to16)

#define DEFINED_CONFIG_VENC_CNT_11to16 \
	(DEFINED_CONFIG_VENC_CNT_11 || DEFINED_CONFIG_VENC_CNT_12to16)

#define DEFINED_CONFIG_VENC_CNT_10to16 \
	(DEFINED_CONFIG_VENC_CNT_10 || DEFINED_CONFIG_VENC_CNT_11to16)

#define DEFINED_CONFIG_VENC_CNT_9to16 \
	(DEFINED_CONFIG_VENC_CNT_9 || DEFINED_CONFIG_VENC_CNT_10to16)

#define DEFINED_CONFIG_VENC_CNT_8to16 \
	(DEFINED_CONFIG_VENC_CNT_8 || DEFINED_CONFIG_VENC_CNT_9to16)

#define DEFINED_CONFIG_VENC_CNT_7to16 \
	(DEFINED_CONFIG_VENC_CNT_7 || DEFINED_CONFIG_VENC_CNT_8to16)

#define DEFINED_CONFIG_VENC_CNT_6to16 \
	(DEFINED_CONFIG_VENC_CNT_6 || DEFINED_CONFIG_VENC_CNT_7to16)

#define DEFINED_CONFIG_VENC_CNT_5to16 \
	(DEFINED_CONFIG_VENC_CNT_5 || DEFINED_CONFIG_VENC_CNT_6to16)

#define DEFINED_CONFIG_VENC_CNT_4to16 \
	(DEFINED_CONFIG_VENC_CNT_4 || DEFINED_CONFIG_VENC_CNT_5to16)

#define DEFINED_CONFIG_VENC_CNT_3to16 \
	(DEFINED_CONFIG_VENC_CNT_3 || DEFINED_CONFIG_VENC_CNT_4to16)

#define DEFINED_CONFIG_VENC_CNT_2to16 \
	(DEFINED_CONFIG_VENC_CNT_2 || DEFINED_CONFIG_VENC_CNT_3to16)

#define DEFINED_CONFIG_VENC_CNT_1to16 \
	(DEFINED_CONFIG_VENC_CNT_1 || DEFINED_CONFIG_VENC_CNT_2to16)

/*****************************************************************************/


#include "vpu_structure.h"
#include "vpu_etc.h"
#include "vpu_dbg.h"

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
#include "smp_driver.h"
#endif

/* vpu v2 */
#include <video/telechips/vpu_v2/vpu2_ioctl_cmds.h>
#include <video/telechips/vpu_v2/vpu2_cio_bearer.h>
#include <video/telechips/vpu_v2/vpu2_cio_consts.h>
#include <video/telechips/vpu_v2/vpu2_dio_fields.h>
#include <video/telechips/vpu_v2/vpu2_dio_csblob.h>

#ifdef CONFIG_SUPPORT_TCC_VPU
#if defined(CONFIG_TYPE_C5)
#include <video/telechips/TCC_VPUs_CODEC.h>
#else
#include <video/telechips/TCC_VPU_CODEC.h>
#endif
#include <video/telechips/tcc_vpu_ioctl.h>
#endif

#ifdef CONFIG_SUPPORT_TCC_JPU
#if defined(JPU_C5)
#include <video/telechips/TCC_JPU_CODEC.h>
#else
#include <video/telechips/TCC_JPU_C6.h>
#endif
#include <video/telechips/tcc_jpu_ioctl.h>
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
#include <video/telechips/TCC_HEVC_CODEC.h>
#include <video/telechips/tcc_hevc_ioctl.h>
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2	// HEVC/VP9
#include <video/telechips/TCC_VPU_4K_D2_CODEC.h>
#include <video/telechips/tcc_4k_d2_ioctl.h>
#endif

#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
#include <video/telechips/TCC_VP9_CODEC.h>
#include <video/telechips/tcc_vp9_ioctl.h>
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
#include <video/telechips/TCC_VPU_HEVC_ENC_CODEC.h>
#include <video/telechips/tcc_vpu_hevc_enc_ioctl.h>
#endif

#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
#	define USE_ACCESS_POINT
#endif

#if defined(USE_ACCESS_POINT)
#	define vpu_check_fourcc(a, b, c, d)\
	(((__u32)(a) | ((__u32)(b) << 8U)) | (((__u32)(c) << 16U) | ((__u32)(d) << 24U)))
#	define GET_FOURCC_1(a) (((__u32)(a)) & (0x00FFU))
#	define GET_FOURCC_2(a) (((__u32)(a) >>  8U) & (0x00FFU))
#	define GET_FOURCC_3(a) (((__u32)(a) >> 16U) & (0x00FFU))
#	define GET_FOURCC_4(a) (((__u32)(a) >> 24U) & (0x00FFU))

#	define CHECK_CODE_01 vpu_check_fourcc('T', 'e', 'l', 'e')
#	define CHECK_CODE_02 vpu_check_fourcc('c', 'h', 'i', 'p')
#	define CHECK_CODE_03 vpu_check_fourcc('s', 'V', 'i', 'd')
#	define CHECK_CODE_04 vpu_check_fourcc('e', 'o', 0xFF, 0xFF)

# 	if defined(CONFIG_TCC805X_CA53Q) || defined(CONFIG_TCC807X_CA55_SUB)
#   	define SHARE_POINT_ADDR 0x50000000
#	elif defined(CONFIG_ARCH_TCC897X)
#   	define SHARE_POINT_ADDR 0x90000000
# 	else
#   	define SHARE_POINT_ADDR 0x30000000
# 	endif
# 	define SHARD_POINT_GAP  128U
#endif

#ifdef CONFIG_CFI_CLANG
#   if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
#       define VPU_NO_SANITIZE_CFI __attribute__((no_sanitize("kcfi")))
#   else
#       define VPU_NO_SANITIZE_CFI __attribute__((no_sanitize("cfi")))
#   endif
#else
#	define VPU_NO_SANITIZE_CFI
#endif

//In case of kernel operation (open/close in the kernel level)
// like TMS, open/close works asynchronosly.
#if 0
#define USE_DEV_OPEN_CLOSE_IOCTL
#endif

/* COMMON */
#define VPU_LIMIT_PICWIDTH  (8*1024)
#define VPU_LIMIT_PICHEIGHT (VPU_LIMIT_PICWIDTH)

#define IRQ_INT_TYPE    (((unsigned long)IRQ_TYPE_EDGE_RISING) | ((unsigned long)IRQF_SHARED))

#define LIST_MAX 10

#define RET4_WAIT   0X00010000
#define RET3        0x00008000
#define RET2        0x00004000
#define RET1        0x00002000
#define RET0        0x00001000

#define INT_MAX_U		(~0U >> 1)
#define INT_MAX_S 		(0x7fffffff)

#define VPU_BUG_ON(x)	\
{ \
	void *tTmpPtr = NULL; \
	VPU_CAST_PT(tTmpPtr, x); \
	if (IS_ERR(tTmpPtr)) { \
		BUG(); \
	} \
}

#define VPU_UNUSED_PARAMETER(x) 	do { (void)(x); } while (0)
#define VPU_NO_OP	((void)0)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
#	define VREMOVE_RET_TYPE void
#	define VREMOVE_RETURN()	VPU_NO_OP
#else
static inline int vremove_return(void) { return 0; }
#	define VREMOVE_RET_TYPE int
#	define VREMOVE_RETURN()  do { return vremove_return(); } while (0)
#endif

typedef union {
	codec_handle_t *pcodec_handle;
	vcodec_handle_t *pvcodec_handle;
} union_codec_handle_t;

enum list_cmd_type {
	LIST_ADD = 0,
	LIST_DEL,
	LIST_IS_EMPTY,
	LIST_GET_ENTRY
};

struct vpu_dec_data_t {
	wait_queue_head_t wq;
	spinlock_t lock;
	unsigned int count;
	unsigned char dev_opened;
#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	unsigned char dev_file_opened;
#endif
};

struct VpuList {
	struct list_head list;

	unsigned int type;	//encode or decode ?
	int cmd_type;		//vpu command
	long handle;
	void *args;		//vpu argument!!
	struct vpu_dec_data_t *comm_data;
	int *vpu_result;
};

struct MgrCommData {
	struct list_head main_list;
	struct list_head wait_list;
	struct mutex list_mutex;
	struct mutex io_mutex;
	struct mutex file_mutex;
	struct mutex process_mutex;
	unsigned int thread_intr;
	wait_queue_head_t thread_wq;
};

#ifdef CONFIG_VPU_TIME_MEASUREMENT
struct TimeInfo {
	unsigned int print_out_index;
	unsigned int proc_time[30];
	unsigned int proc_base_cnt;	// 0~29
	unsigned int proc_time_30frames;
	// between 1st frame and last one.
	unsigned int accumulated_proc_time;
	unsigned int accumulated_frame_cnt;
};
#endif

struct mgr_data_t {
//IRQ number and IP base
	unsigned int irq;
	void __iomem *base_addr;
	int check_interrupt_detection;
#define VPU_CLOSED 1
	int closed[VPU_MAX];
	long handle[VPU_MAX];
	int fileplay_mode[VPU_MAX];

#if defined(CONFIG_PM)
	struct VpuList vList[VPU_MAX];
#endif

	struct MgrCommData comm_data;

	atomic_t oper_intr;
	wait_queue_head_t oper_wq;

	atomic_t opened;

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	atomic_t dev_file_opened;
#endif
	unsigned char irq_reged;

	unsigned char cmd_processing;
	unsigned char only_decmode;
	unsigned char clk_limitation;

	unsigned char external_proc;

	int current_cmd;
	unsigned int szFrame_Len;
	unsigned int nDecode_Cmd;
	unsigned int nOpened_Count;
	unsigned int current_resolution;

#ifdef CONFIG_SUPPORT_TCC_VPU
	VDEC_RENDERED_BUFFER_t gsRender_fb_info;
#endif
	unsigned int cmd_queued;

	bool bDiminishInputCopy;
#ifdef CONFIG_VPU_TIME_MEASUREMENT
	struct TimeInfo iTime[VPU_MAX];
#endif

	bool bVpu_already_proc_force_closed;

	struct {
		int interlaced[VPU_MAX];
		int interlaced_video_total; // total in IP
		struct VpuList *last_vlist[VPU_MAX];
		unsigned long last_timestamp;
		int pending_vtype; //pending instance
		int avoid_pending;
		int pending_count[VPU_MAX];
		int isFlexible[VPU_MAX];
		struct vpu_decoder_data *vdata[VPU_MAX];
	} vpu_ctrl;

	int timeout_exit;
	int timeout_vpu_type;
};

struct vpu_decoder_data {
	struct miscdevice *misc;
	struct vpu_dec_data_t vComm_data;
	int gsDecType;
	int gsCodecType;

	int gsIsDiminishedCopy;

#ifdef CONFIG_SUPPORT_TCC_VPU
	VDEC_INIT_t gsVpuDecInit_Info;
	VDEC_SEQ_HEADER_t gsVpuDecSeqHeader_Info;
	VDEC_SET_BUFFER_t gsVpuDecBuffer_Info;
	VDEC_SET_BUFFER3_t gsVpuDecBuffer3_Info;
	VDEC_DECODE_t gsVpuDecInOut_Info;
	VDEC_RINGBUF_GETINFO_t gsVpuDecBufStatus;
	VDEC_RINGBUF_SETBUF_t gsVpuDecBufFill;
	VDEC_RINGBUF_SETBUF_PTRONLY_t gsVpuDecUpdateWP;
	VDEC_GET_VERSION_t gsVpuDecVersion;
#endif

#ifdef CONFIG_SUPPORT_TCC_JPU
	JDEC_INIT_t gsJpuDecInit_Info;
#if defined(JPU_C6)
	JDEC_SEQ_HEADER_t gsJpuDecSeqHeader_Info;
#endif
	JPU_SET_BUFFER_t gsJpuDecBuffer_Info;
	JPU_SET_BUFFER3_t gsJpuDecBuffer3_Info;
	JPU_DECODE_t gsJpuDecInOut_Info;
	JPU_GET_VERSION_t gsJpuDecVersion;
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	VPU_4K_D2_INIT_t gsV4kd2DecInit_Info;
	VPU_4K_D2_SEQ_HEADER_t gsV4kd2DecSeqHeader_Info;
	VPU_4K_D2_SET_BUFFER_t gsV4kd2DecBuffer_Info;
	VPU_4K_D2_SET_BUFFER3_t gsV4kd2DecBuffer3_Info;
	VPU_4K_D2_DECODE_t gsV4kd2DecInOut_Info;
	VPU_4K_D2_RINGBUF_GETINFO_t gsV4kd2DecBufStatus;
	VPU_4K_D2_RINGBUF_SETBUF_t gsV4kd2DecBufFill;
	VPU_4K_D2_RINGBUF_SETBUF_PTRONLY_t gsV4kd2DecUpdateWP;
	VPU_4K_D2_GET_VERSION_t gsV4kd2DecVersion;
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	HEVC_INIT_t gsHevcDecInit_Info;
	HEVC_SEQ_HEADER_t gsHevcDecSeqHeader_Info;
	HEVC_SET_BUFFER_t gsHevcDecBuffer_Info;
	HEVC_DECODE_t gsHevcDecInOut_Info;
	HEVC_RINGBUF_GETINFO_t gsHevcDecBufStatus;
	HEVC_RINGBUF_SETBUF_t gsHevcDecBufFill;
	HEVC_RINGBUF_SETBUF_PTRONLY_t gsHevcDecUpdateWP;
	HEVC_GET_VERSION_t gsHevcDecVersion;
#endif

#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	VP9_INIT_t gsVp9DecInit_Info;
	VP9_SEQ_HEADER_t gsVp9DecSeqHeader_Info;
	VP9_SET_BUFFER_t gsVp9DecBuffer_Info;
	VP9_DECODE_t gsVp9DecInOut_Info;
	VP9_GET_VERSION_t gsVp9DecVersion;
#endif

	// VPU V2 flex array
	struct v2hw_flex_io *flx_list[V2D_IOP_MAX];
	struct v2hw_io_delay *flx_io_delay;

	unsigned int gsDecClearBuffer_index;
	int gsCommDecResult;
	unsigned char list_idx;
	bool list_inited;

	struct VpuList vdec_list[LIST_MAX];
};

#if DEFINED_CONFIG_VENC_CNT_1to16
struct vpu_encoder_data {
	struct miscdevice *misc;
	struct vpu_dec_data_t vComm_data;
	int gsEncType;
	int gsCodecType;

#ifdef CONFIG_SUPPORT_TCC_VPU
	VENC_INIT_t gsVpuEncInit_Info;
	VENC_PUT_HEADER_t gsVpuEncPutHeader_Info;
	VENC_SET_BUFFER_t gsVpuEncBuffer_Info;
	VENC_ENCODE_t gsVpuEncInOut_Info;
#endif

#ifdef CONFIG_SUPPORT_TCC_JPU
	JENC_INIT_t gsJpuEncInit_Info;
	JPU_ENCODE_t gsJpuEncInOut_Info;
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
	VENC_HEVC_INIT_t gsVpuHevcEncInit_Info;
	VENC_HEVC_SET_BUFFER_t gsVpuHevcEncBuffer_Info;
	VENC_HEVC_PUT_HEADER_t gsVpuHevcEncPutHeader_Info;
	VENC_HEVC_ENCODE_t gsVpuHevcEncInOut_Info;
#endif

	int gsCommEncResult;
	unsigned char list_idx;

	struct VpuList venc_list[LIST_MAX];
};
#endif

#endif /*VPU_COMM_H*/
