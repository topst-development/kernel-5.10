/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */


#ifndef _VPU2_CIO_CONSTS_H_
#define _VPU2_CIO_CONSTS_H_

// v1 work-log: TCCxxxx_VPU_CODEC_COMMON.h
// v1 work-log: TCC_VPU_D6.h
// v1 work-log: TCC_VPU_C7_CODEC.h
// v1 work-log: TCC_VPU_4K_D2.h
// v1 work-log: TCC_HEVCDEC.h
// v1 work-log: TCC_VP9DEC.h
// v1 work-log: TCC_JPU_C6.h

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// TYPE DEFINITION
typedef uint64_t v2buf_addr_t; // v1: codec_addr_t
typedef uint64_t v2buf_size_t; // v1: size_t

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// ENUMERATION
/* V2D fiex field value: bitstream buffering mode */
enum V2ENUM_BSMOD
#if defined(__cplusplus)
    : int32_t
#endif
{
    V2_VAL_BSMOD_LNR   = 0, // linear with zero offset
    V2_VAL_BSMOD_QUE   = 1, // input element queue // (1 << 26)
    V2_VAL_BSMOD_RNG   = 2, // ring mode (managed by vpu)
    V2_VAL_BSMOD_LOF   = 3, // linear with offset (managed by hal)
};

/* V2D fiex field value: random access mode */
// [NOTE] do not change enum value
enum V2ENUM_RAMOD
#if defined(__cplusplus)
    : int32_t
#endif
{
    V2_VAL_RA_NONE 	   =  0,
    V2_VAL_RA_PICK_KEY =  1,
    V2_VAL_RA_SKIP_B   =  2,
    V2_VAL_RA_SKIP_P_B =  3, // v1: skip except I(IDR) picture
    V2_VAL_RA_MAX,
};

/* V2D fiex field value: sub-mode for RA key (H.264 only) */
enum V2ENUM_KEYTYPE
#if defined(__cplusplus)
    : int32_t
#endif
{
    V2_VAL_KEY_IDR     = 0x001, // the first picture of each coded video sequence
    V2_VAL_KEY_I_IDR   = 0x201, // IDR or I (Non IDR)
};

/* V2D fiex csi value: YUV format (MJPEG only) */
enum V2ENUM_YUVTYPE
#if defined(__cplusplus)
    : int32_t
#endif
{
    V2_VAL_YUV_420   = 0,
    V2_VAL_YUV_422   = 1,
    V2_VAL_YUV_422_V = 2, // 4:2:2 vertical
    V2_VAL_YUV_444   = 3,
    V2_VAL_YUV_400   = 4,
};

enum V2ENUM_HWBUF
#if defined(__cplusplus)
    : int32_t
#endif
{
    V2_VAL_HWBUF_BS = 0,  // bitstream buffer
    V2_VAL_HWBUF_FB = 1,  // frame buffer
    V2_VAL_HWBUF_UD = 2,  // userdata buffer
    V2_VAL_HWBUF_SQ = 3,  // seq.header (sps-pps) save buffer (avc only)
    V2_VAL_HWBUF_MB = 4,  // mb slice save buffer (avc, vp8 only)
    V2_VAL_HWBUF_WB = 5,  // work buffer
    V2_VAL_HWBUF_MAX,
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// MACRO CONSTANTS: VPU lib. specific magic number
#define V2_RNG_DEC_DUMMY_SIZE          (1)

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// MACRO CONSTANTS: RETCODE
/* [|V|J|PU-COMMON] --------------------------------------------------------- */
#define V2_ERR_NONE     (0)

#define V2_ERR_SEQDATA_NOT_FOUND       (31)
#define V2_ERR_STRIDE0_OR_ALIGN8       (100)
#define V2_ERR_MIN_RESOLUTION          (101)
#define V2_ERR_MAX_RESOLUTION          (102)
#define V2_ERR_PROFILE                 (110)

// Remark: add 2 more user-specific errors due to R&R repositioning
// v1: VPU_ENV_INIT_ERROR: error from vpu_env_open
#define V2_ERR_ENV_SETUP               (-10000)
// v1: VPU_NOT_ENOUGH_MEM: error from buffer reg setting
#define V2_ERR_OOM                     (-20000)

#define V2_ERR_FIO_VERSION             (-25000)

#define V2_ERR_SYSCALL                 (-30000)
#define V2_ERR_IOCTL                   (-30001)
#define V2_ERR_POLL                    (-30002)
#define V2_ERR_MMAP                    (-30003)
#define V2_ERR_IOREMAP                 (-30004)

#define V2_ERR_HW_HANGUP               (-40000)
#define V2_ERR_HW_HANGUP_IOCTL         (-40001)
#define V2_ERR_HW_HANGUP_POLL          (-40002)

/* [|V|PU Specific] --------------------------------------------------------- */
/* prefix change: v1:RETCODE_ -> v2: V2_ERR_ */
#define V2_ERR_HW                          (1)
#define V2_ERR_INVALID_HANDLE              (2)
#define V2_ERR_INVALID_PARAM               (3)
#define V2_ERR_INVALID_COMMAND             (4)
#define V2_ERR_ROTATOR_OUTPUT_NOT_SET      (5)
#define V2_ERR_ROTATOR_STRIDE_NOT_SET      (6)
#define V2_ERR_FRAME_NOT_COMPLETE          (7)
#define V2_ERR_INVALID_FRAME_BUFFER        (8)
#define V2_ERR_INSUFFICIENT_FRAME_BUFFERS  (9)
#define V2_ERR_INVALID_STRIDE              (10)
#define V2_ERR_WRONG_CALL_SEQUENCE         (11)
#define V2_ERR_CALLED_BEFORE               (12)
#define V2_ERR_NOT_INITIALIZED             (13)
#define V2_ERR_USERDATA_BUF_NOT_SET        (14)
#define V2_ERR_CODEC_FINISH                (15)
#define V2_ERR_CODEC_EXIT                  (16)
#define V2_ERR_CODEC_SPECOUT               (17)
#define V2_ERR_MEM_ACCESS_VIOLATION        (18)
#define V2_ERR_INPUT_UNDERRUN              (20)
#define V2_ERR_INSUFFICIENT_BITSTREAM_BUF  (21)
#define V2_ERR_INSUFFICIENT_PS_BUF         (22)
#define V2_ERR_HW_ACCESS_VIOLATION         (23)
#define V2_ERR_2ND_AXI_BUF_UNDERRUN        (24)
#define V2_ERR_QUEUEING_FAILURE            (25)
#define V2_ERR_VPU_STILL_RUNNING           (26)
#define V2_ERR_REPORT_NOT_READY            (27)
#define V2_ERR_VPU_FW_PENDING              (30)

#define V2_ERR_MULTI_EXIT_TIMEOUT          (99)

#define V2_ERR_WRAP_AROUND                 (-10) // [NOTE] negative is right

/* [VPU-|D6|C7| Specific] --------------------------------------------------- */
/* prefix change: v1:RETCODE_ -> v2: V2_ERR_ */
#define V2_ERR_SEQ_INIT_HANGUP            (103)
#define V2_ERR_CHROMA_FORMAT              (104)

#define V2_ERR_VC1_COMPLEX_PROFILE        (120)
#define V2_ERR_H263_ANNEX_D               (130)
#define V2_ERR_H263_ANNEX_EFG             (131)
#define V2_ERR_H263_UFEP                  (132)
#define V2_ERR_H263_ANNEX_D_PLUSPTYPE     (133)
#define V2_ERR_H263_ANNEX_EF_PLUSPTYPE    (134)
#define V2_ERR_H263_ANNEX_NRS_PLUSPTYPE   (135)
#define V2_ERR_H263_ANNEX_PQ_MPPTYPE      (136)
#define V2_ERR_H263_UUI                   (137)
#define V2_ERR_H263_SSS                   (138)
#define V2_ERR_H263_PIC_SIZE              (139)
#define V2_ERR_MPEG4_OBMC_DISABLE         (140)
#define V2_ERR_MPEG4_SPRITE_ENABLE        (141)
#define V2_ERR_MPEG4_SCALABILITY          (142)
#define V2_ERR_MPEG4_SPK_FORMAT           (144)
#define V2_ERR_MPEG4_SPK_VERSION          (145)
#define V2_ERR_MPEG4_SPK_RESOLUTION       (146)
#define V2_ERR_MPEG4_PACKEDPB             (147)
#define V2_ERR_MPEG4_PIC_SIZE             (148)
#define V2_ERR_MPEG2_CHROMA_FORMAT        (150)
#define V2_ERR_MPEG2_PROFILE              (151)
#define V2_ERR_MPEG2_SEQ_PIC_W_SPEC_OVER  (152) // over 1920
#define V2_ERR_MPEG2_SEQ_PIC_H_SPEC_OVER  (153) // over 1152

/* [VPU-C7 Specific] -------------------------------------------------------- */
#define V2_ERR_AVS_PROFILE                (170) // AVS

/* [VPU-C5 Specific] -------------------------------------------------------- */
#define V2_ERR_RV_SUB_MOF_FLAG31TO16      (160) // RV
#define V2_ERR_RV_SUB_MOF_FLAG16TO0       (161) // RV8, RV9, RV89combo

/* [VPU-4KD2 Specific] ------------------------------------------------------ */
#define V2_ERR_4KD2_SEQ_INIT_HANGUP       (103)
#define V2_ERR_4KD2_CHROMA_FORMAT         (104)

/* [VPU-HEVC Specific] ------------------------------------------------------ */
#define V2_ERR_HEVC_SEQ_INIT_HANGUP       (103)
#define V2_ERR_HEVC_CHROMA_FORMAT         (104)

/* [VPU-VP9 Specific] ------------------------------------------------------- */
#define V2_ERR_VP9_BITDEPTH               (103)
#define V2_ERR_VP9_RGB_FORMAT             (104)


/* [|J|PU Specific] --------------------------------------------------------- */
/* [JPU-C6 Specific] -------------------------------------------------------- */
/* prefix change: v1:JPG_RET_ -> v2: JPU2_ERR_ */
#define J2_ERR_BIT_EMPTY                   (2)
#define J2_ERR_EOS                         (3)
#define J2_ERR_INVALID_HANDLE              (4)
#define J2_ERR_INVALID_PARAM               (5)
#define J2_ERR_INVALID_COMMAND             (6)
#define J2_ERR_ROTATOR_OUTPUT_NOT_SET      (7)
#define J2_ERR_ROTATOR_STRIDE_NOT_SET      (8)
#define J2_ERR_FRAME_NOT_COMPLETE          (9)
#define J2_ERR_INVALID_FRAME_BUFFER        (10)
#define J2_ERR_INSUFFICIENT_FRAME_BUFFERS  (11)
#define J2_ERR_INVALID_STRIDE              (12)
#define J2_ERR_WRONG_CALL_SEQUENCE         (13)
#define J2_ERR_CALLED_BEFORE               (14)
#define J2_ERR_NOT_INITIALIZED             (15)
#define J2_ERR_CODEC_EXIT                  (16)
#define J2_ERR_SPEC_OUT                    (17)
#define J2_ERR_INSUFFICIENT_BITSTREAM_BUF  (18)
#define J2_RET_CODEC_FINISH                (19) // [FIXME] JPG_RET_NOBASELINE

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// MACRO CONSTANTS: FIELD VALUE

#define V2_PICTYPE_I    (0x000)
#define V2_PICTYPE_P    (0x001)
#define V2_PICTYPE_B    (0x002)
#define V2_PICTYPE_SKIP (0x004)
#define V2_PICTYPE_IDR  (0x005)
#define V2_PICTYPE_P_B  (0x102) // mpeg4 packed B

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// MACRO CONSTANTS: CODEC TYPE (v1: STD: standard target Decoder)
#define V2CODEC_AVC         (0)
#define V2CODEC_VC1         (1)
#define V2CODEC_MPEG2       (2)
#define V2CODEC_MPEG4       (3)
#define V2CODEC_H263        (4)
#define V2CODEC_DIV3        (5)
#define V2CODEC_WMV78       (8)
#define V2CODEC_MJPG       (10)
#define V2CODEC_VP8        (11)
#define V2CODEC_HEVC       (15)
#define V2CODEC_VP9        (16)
#define V2CODEC_HEVC_ENC   (17)

// MACRO CONSTANTS: 0 based linear ENUM TYPE (v1: Y|U|V, PA|VA)
#define V2IMG_Y   (0u)
#define V2IMG_U   (1u)
#define V2IMG_V   (2u)

#define V2MEM_PA  (0u)
#define V2MEM_VA  (1u)

#endif  // _VPU2_CIO_CONSTS_H_
