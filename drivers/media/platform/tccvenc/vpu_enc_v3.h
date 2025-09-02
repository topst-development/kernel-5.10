// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef _VPU_ENC_V3_H_
#define _VPU_ENC_V3_H_

#include "tccvenc_debug.h"
#include "vpu_v3/tcc_vpu_v3_ioctl.h"
#include "vpu_v3/tcc_vpu_v3_version.h"
#include "video/telechips/vpu_v3/tcc_vpu_v3_common.h"
#include "video/telechips/vpu_v3/tcc_vpu_v3_encoder.h"

#define VENC_MAX_CAP_STR  (64)
#define VENC_MAX_CAP  (64)

typedef void* venc_handle_h;
typedef long long venc_addr_t;

enum venc_source_format
{
	VENC_SOURCE_NONE = 0,
    VENC_SOURCE_YUV420P, /* YUV420 planar (full planer) : Y field + U field + V field */
    VENC_SOURCE_NV12, /* NV12, YUV420SP(semi planer), YUV420 interleaved : Y field + UV field. */
	VENC_SOURCE_YUV422,
	VENC_SOURCE_YUV444,
	VENC_SOURCE_YUV400,
	VENC_SOURCE_MAX,
};

typedef struct venc_init_t
{
    enum vpu_codec_id codec;
	enum venc_source_format source_format;

    int pic_width;                    //!< Width  : multiple of 16
    int pic_height;                   //!< Height : multiple of 16
    int framerate;                   //!< Frame rate
    int bitrateKbps;                  //!< Target bit rate in Kbps. if 0, there will be no rate control,
                                        //!< and pictures will be encoded with a quantization parameter equal to quantParam

    int key_interval;                 //!< Key interval : max 32767
    int avc_fast_encoding;             //!< fast encoding for AVC( 0: default, 1: encode intra 16x16 only )

    //! Options
    int slice_mode;
    int slice_size_mode;
    int slice_size;

    int encoding_quality;                  //!< jpeg encoding quality

    int deblk_disable;                //!< 0 : Enable, 1 : Disable, 2 Disable at slice boundary
    int vbv_buffer_size;               //!< Reference decoder buffer size in bits(0 : ignore)

    int initial_qp;
    int max_i_qp;
    int max_p_qp;
    int min_i_qp;
    int min_p_qp;
    int idr_frame_encoding;            //!< 1 : Enable, 0 : Disable, default is 0 (H.264 only)

	int enable_force_vpu_ip;
    int force_vpu_ip_index;
} venc_init_t;

typedef struct venc_seq_header_t
{
    unsigned char* seq_header_out;     //!< [out] Seqence header pointer
    int seq_header_out_size;       	    //!< [out] Seqence header size
} venc_seq_header_t;

typedef struct venc_input_t
{
    unsigned char* input_y;
    unsigned char* input_crcb[2];

    int change_rc_param_flag;   //0: disable, 3:enable(change a bitrate), 5: enable(change a framerate), 7:enable(change bitrate and framerate)
    int change_target_bitrate_kbps;
    int change_framerate;
    int quant_param;

    unsigned char request_IntraFrame;
} venc_input_t;

typedef struct venc_output_t
{
    unsigned char* bitstream_out;
    int bitstream_out_size;

    enum vpu_picture_type pic_type;
} venc_output_t;

int venc_init(venc_handle_h handle, venc_init_t* p_init);

int venc_put_seqheader(venc_handle_h handle, venc_seq_header_t* p_seqhead);

int venc_encode(venc_handle_h handle, venc_input_t* p_input, venc_output_t* p_output);

int venc_close(venc_handle_h handle);

venc_handle_h venc_alloc_instance(void);

void venc_release_instance(venc_handle_h handle);

#endif