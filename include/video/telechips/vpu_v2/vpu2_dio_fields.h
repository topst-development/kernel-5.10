/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef _VPU2_DIO_FIELDS_H_
#define _VPU2_DIO_FIELDS_H_

//#include "vpu2_cio_bearer.h"

// work-log: TCC_VPU_D6.h
// work-log: TCC_VPU_C5_CODEC.h
// work-log: TCC_VPU_C7_CODEC.h
// work-log: TCC_VPU_4K_D2.h
// work-log: TCC_JPU_C6.h
// work-log: TCC_HEVCDEC.h
// work-log: TCC_VP9DEC.h (v2: deprecated)

/* -----------------------------------------------------------------------------
 * naming rule: acronyms
 * -----------------------------------------------------------------------------
 * [level 0] XXX_..._...
 * V2   : Vpu version 2 (top-level prefix)
 * V2D  : V2 Decoder driver
 *
 * [level 1] ..._XXX_...
 * KI   : key   for HW operation Input
 * KO   : key   for HW operation Output
 * FI   : Field for HW operation Input
 * FO   : Field for HW operation Output
 *
 * [level 2] ..._..._XXX_...
 * : represents ioctl keyword token
 *
 * [level 3] ..._..._..._XXX (bottom-level suffix)
 * : represents field per ioctl keyword token
 *
 * FB   : HW address of Frame Buffer
 * BS   : HW address of bitStream Buffer
 * UB   : HW address of Userdata Buffer
 *
 * CSI  : Codec Specific Information (v1: m_reserved[xx])
 *
 * vpu hw operation sequence
 * ioctl for request  -----> queue vpu hw cmd
 *                    <-----
 *      await poll      |
 * .... polling....     |    hw operation
 *      wake up poll
 * ioctl for response ----->
 *                    <----- hw op result
 *
 * [postfix] bit swiddling of 'codec_addr_t'
 * _H   : high bits
 * _L   : low bits
 *
 * -------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
 * macro: typing shortcut generator
 * -------------------------------------------------------------------------- */
#define V2DGEN(__level1, __level2, __level3) \
        V2D_##__level1##_##__level2##_##__level3

#define V2BGEN(__level1, __level2, __level3) \
        V2D_##__level1##_##__level2##_B_##__level3

#define V2ENUM(X) V2ENUM_##X

/* -----------------------------------------------------------------------------
 * macro: dummy signiture (for the purpose of readability increase)
 * -------------------------------------------------------------------------- */
#define v2step_(X) case X

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// IOCTL: v1: V_DEC_INIT        -> v2: V2D_IP_DRV_INI
// IOCTL: v1: V_DEC_INIT_RESULT -> v2: V2D_OP_DRV_INI

/* -----------------------------------------------------------------------------
 * Key|value pair - struct |dec_init_t|
 * : deprecated fields: m_iPicWidth, m_iPicHeight
 * : should dive into: expose the hidden usage of m_Reserved array
 * -------------------------------------------------------------------------- */
/* V2D meta field 'boolean' - ioctl INIT */
#define V2D_FI_INI_B_DPMEM     (0u) // dec preemptive mem alloc mode on/off
#define V2D_FI_INI_B_HWVER     (1u) // vpu lib. version access on/off
#define V2D_FI_INI_B_USRDAT    (2u) // userdata on/off
#define V2D_FI_INI_B_MATRIX    (3u) // pixel matrix, cb/cr interleave on/off
#define V2D_FI_INI_B_FWSACC    (4u) // vpu fw secure access on/off
#define V2D_FI_INI_B_MAPOFF    (5u) // map conv. Y/Chroma off (def:on)
#define V2D_FI_INI_B_MAXFRM    (6u) // max frame buffer mode
#define V2D_FI_INI_B_10TO8     (7u) // bit depth 10 to 8 shrink
#define V2D_FI_INI_B_IMGCUT    (8u) // jpeg still image
#define V2D_FI_INI_B_NBDELY    (9u) // no buffer delay
#define V2D_FI_INI_B_MAX      (10u) // should not exceed 64

/* V2D meta key - ioctl INIT */
#define V2D_KI_INI_B_DPMEM \
        ((uint32_t)1u << V2BGEN(FI, INI, DPMEM))   // v1: DEC_ONLY in Open_Type
#define V2D_KI_INI_B_HWVER \
        ((uint32_t)1u << V2BGEN(FI, INI, HWVER))   // v1: vpu lib. version info
#define V2D_KI_INI_B_USRDAT \
        ((uint32_t)1u << V2BGEN(FI, INI, USRDAT))  // v1: m_bEnableUserData
#define V2D_KI_INI_B_MATRIX \
        ((uint32_t)1u << V2BGEN(FI, INI, MATRIX))  // v1:!m_bCbCrInterleaveMode
#define V2D_KI_INI_B_FWSACC \
        ((uint32_t)1u << V2BGEN(FI, INI, SECFW))   // v1: m_uiDecOptFlags |= (1 << 7)
#define V2D_KI_INI_B_MAPOFF \
        ((uint32_t)1u << V2BGEN(FI, INI, MAPOFF))  // v1: map conv. off
#define V2D_KI_INI_B_MAXFRM \
        ((uint32_t)1u << V2BGEN(FI, INI, MAXFRM))  // v1: max.fb.mode on
#define V2D_KI_INI_B_10TO8 \
        ((uint32_t)1u << V2BGEN(FI, INI, 10TO8))   // v1: m_uiDecOptFlags |= (1 << 3)
#define V2D_KI_INI_B_IMGCUT \
        ((uint32_t)1u << V2BGEN(FI, INI, IMGCUT))  // v1: m_bStillJpeg
#define V2D_KI_INI_B_NBDELY \
        ((uint32_t)1u << V2BGEN(FI, INI, NBDELY))  // v1: m_uiDecOptFlags |= (1 << 2)

/* V2D fiex field - ioctl INIT for hw op request */
#define V2D_FI_INI_FORMAT      (0u) // bitstream format
#define V2D_FI_INI_MFB_W       (1u) // max framebuffer width
#define V2D_FI_INI_MFB_H       (2u) // max framebuffer height
#define V2D_FI_INI_DMX_W       (3u) // demuxer width  (vp8 only)
#define V2D_FI_INI_DMX_H       (4u) // demuxer height (vp8 only)
#define V2D_FI_INI_BITDEP      (5u) // max bit depth (upto 10 bit)
#define V2D_FI_INI_CMDDEP      (6u) // cmd queue depth of vpu lib.
#define V2D_FI_INI_BSMOD       (7u) // bitstream buffer mode
#define V2D_FI_INI_BS_SIZE     (8u) // bitstream buffer size
#define V2D_FI_INI_UD_SIZE     (9u) // userdata buffer size
/* [NOTE][10 ~ 22] used by vpu d/d <-> vpu fw */
#define V2D_FI_INI_BW_SIZE    (10u) // bit-work buffer size
#define V2D_FI_INI_PS_SIZE    (11u) // sps-pps save buffer size
#define V2D_FI_INI_BS_PA_H    (12u)
#define V2D_FI_INI_BS_PA_L    (13u)
#define V2D_FI_INI_BS_VA_H    (14u)
#define V2D_FI_INI_BS_VA_L    (15u)
#define V2D_FI_INI_UD_PA_H    (16u)
#define V2D_FI_INI_UD_PA_L    (17u)
#define V2D_FI_INI_UD_VA_H    (18u)
#define V2D_FI_INI_UD_VA_L    (19u)
#define V2D_FI_INI_BW_PA_H    (20u)
#define V2D_FI_INI_BW_PA_L    (21u)
#define V2D_FI_INI_BW_VA_H    (22u)
#define V2D_FI_INI_BW_VA_L    (23u)
#define V2D_FI_INI_PS_PA_H    (24u)
#define V2D_FI_INI_PS_PA_L    (25u)
#define V2D_FI_INI_MAX        (26u) // should not exceed 64

/* V2D key - ioctl INIT for hw op request */
#define V2D_KI_INI_FORMAT \
        ((uint64_t)1ull << V2DGEN(FI, INI, FORMAT))  // [FI:00], v1: m_iBitstreamFormat
#define V2D_KI_INI_MFB_W \
        ((uint64_t)1ull << V2DGEN(FI, INI, MFB_W))   // [FI:01], v1: max fb width
#define V2D_KI_INI_MFB_H \
        ((uint64_t)1ull << V2DGEN(FI, INI, MFB_H))   // [FI:02], v1: max fb height
#define V2D_KI_INI_DMX_W \
        ((uint64_t)1ull << V2DGEN(FI, INI, DMX_W))   // [FI:03], v1: demuxer width  (vp8)
#define V2D_KI_INI_DMX_H \
        ((uint64_t)1ull << V2DGEN(FI, INI, DMX_H))   // [FI:04], v1: demuxer height (vp8)
#define V2D_KI_INI_BITDEP \
        ((uint64_t)1ull << V2DGEN(FI, INI, BITDEP))  // [FI:05], v1: m_Reserved[5] (8, 10, 12(N/A))
#define V2D_KI_INI_CMDDEP \
        ((uint64_t)1ull << V2DGEN(FI, INI, CMDDEP))  // [FI:06], v1: (2 << 17) and/or m_iCQCount
#define V2D_KI_INI_BSMOD \
        ((uint64_t)1ull << V2DGEN(FI, INI, BSMOD))   // [FI:07], v1: m_uiDecOptFlags |= (1 << 26)
#define V2D_KI_INI_BS_SIZE \
        ((uint64_t)1ull << V2DGEN(FI, INI, BS_SIZE)) // [FI:08], v1: m_iBitstreamBufSize
#define V2D_KI_INI_UD_SIZE \
        ((uint64_t)1ull << V2DGEN(FI, INI, UD_SIZE)) // [FI:09], v1: m_iUserDataBufferSize
#define V2D_KI_INI_BW_SIZE \
        ((uint64_t)1ull << V2DGEN(FI, INI, BW_SIZE)) // [FI:10], v1: not exists
#define V2D_KI_INI_PS_SIZE \
        ((uint64_t)1ull << V2DGEN(FI, INI, PS_SIZE)) // [FI:11], v1: m_iSpsPpsSaveBufferSize
#define V2D_KI_INI_BS_PA_H \
        ((uint64_t)1ull << V2DGEN(FI, INI, BS_PA_H)) // [FI:12], v1: m_BitstreamBufAddr[PA]
#define V2D_KI_INI_BS_PA_L \
        ((uint64_t)1ull << V2DGEN(FI, INI, BS_PA_L)) // [FI:13]
#define V2D_KI_INI_BS_VA_H \
        ((uint64_t)1ull << V2DGEN(FI, INI, BS_VA_H)) // [FI:14], v1: m_BitstreamBufAddr[VA]
#define V2D_KI_INI_BS_VA_L \
        ((uint64_t)1ull << V2DGEN(FI, INI, BS_VA_L)) // [FI:15]
#define V2D_KI_INI_UD_PA_H \
        ((uint64_t)1ull << V2DGEN(FI, INI, UD_PA_H)) // [FI:16], v1: m_UserDataAddr[PA]
#define V2D_KI_INI_UD_PA_L \
        ((uint64_t)1ull << V2DGEN(FI, INI, UD_PA_L)) // [FI:17]
#define V2D_KI_INI_UD_VA_H \
        ((uint64_t)1ull << V2DGEN(FI, INI, UD_VA_H)) // [FI:18], v1: m_UserDataAddr[VA]
#define V2D_KI_INI_UD_VA_L \
        ((uint64_t)1ull << V2DGEN(FI, INI, UD_VA_L)) // [FI:19]
#define V2D_KI_INI_BW_PA_H \
        ((uint64_t)1ull << V2DGEN(FI, INI, BW_PA_H)) // [FI:20], v1: m_BitWorkAddr[PA]
#define V2D_KI_INI_BW_PA_L \
        ((uint64_t)1ull << V2DGEN(FI, INI, BW_PA_L)) // [FI:21]
#define V2D_KI_INI_BW_VA_H \
        ((uint64_t)1ull << V2DGEN(FI, INI, BW_VA_H)) // [FI:22], v1: m_BitWorkAddr[VA]
#define V2D_KI_INI_BW_VA_L \
        ((uint64_t)1ull << V2DGEN(FI, INI, BW_VA_L)) // [FI:23]
#define V2D_KI_INI_PS_PA_H \
        ((uint64_t)1ull << V2DGEN(FI, INI, PS_PA_H)) // [FI:24], v1: m_pSpsPpsSaveBuffer (PA only)
#define V2D_KI_INI_PS_PA_L \
        ((uint64_t)1ull << V2DGEN(FI, INI, PS_PA_L)) // [FI:25]


/* V2D field/key - ioctl INIT for hw op respanse */
#define V2D_FO_INI_BS_PA_H      (0u)
#define V2D_FO_INI_BS_PA_L      (1u)
#define V2D_FO_INI_BS_SIZE      (2u)
#define V2D_FO_INI_UD_PA_H      (3u)
#define V2D_FO_INI_UD_PA_L      (4u)
#define V2D_FO_INI_UD_VA_H      (5u)
#define V2D_FO_INI_UD_VA_L      (6u)
#define V2D_FO_INI_UD_SIZE      (7u)
#define V2D_FO_INI_CQ_CAPS      (8u)
#define V2D_FO_INI_MAX          (9u) // should not exceed 64

#define V2D_KO_INI_BS_PA_H \
        ((uint64_t)1ull << V2DGEN(FO, INI, BS_PA_H)) // [FO:00] v1: m_BitstreamBufAddr[PA]
#define V2D_KO_INI_BS_PA_L \
        ((uint64_t)1ull << V2DGEN(FO, INI, BS_PA_L)) // [FO:01]
#define V2D_KO_INI_BS_SIZE \
        ((uint64_t)1ull << V2DGEN(FO, INI, BS_SIZE)) // [FO:02] v1: m_iBitstreamBufSize
#define V2D_KO_INI_UD_PA_H \
        ((uint64_t)1ull << V2DGEN(FO, INI, UD_PA_H)) // [FO:03] v1: m_UserDataAddr[PA]
#define V2D_KO_INI_UD_PA_L \
        ((uint64_t)1ull << V2DGEN(FO, INI, UD_PA_L)) // [FO:04]
#define V2D_KO_INI_UD_VA_H \
        ((uint64_t)1ull << V2DGEN(FO, INI, UD_VA_H)) // [FO:05] v1: m_UserDataAddr[VA]
#define V2D_KO_INI_UD_VA_L \
        ((uint64_t)1ull << V2DGEN(FO, INI, UD_VA_L)) // [FO:06]
#define V2D_KO_INI_UD_SIZE \
        ((uint64_t)1ull << V2DGEN(FO, INI, UD_SIZE)) // [FO:07] v1: m_iUserDataBufferSize
#define V2D_KO_INI_CQ_CAPS \
        ((uint64_t)1ull << V2DGEN(FO, INI, CQ_CAPS)) // [FO:08] v1: m_uiDecOptFlags (n << 17)


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// IOCTL: v1: V_DEC_SEQ_HEADER

/* -----------------------------------------------------------------------------
 * Key/value index for v1 struct |dec_initial_info_t|
 * : V_DEC_SEQ_HEADER will cover 'flex op' of V_DEC_REG_FRAME_BUFFER
 * -------------------------------------------------------------------------- */
/* V2D meta field/key - 'boolean' */
// REMARK: this boolean checker is deprecated (do not use it anymore)
#define V2D_FI_DECSEQ_B_REGFB    (0u) // reg. fb at the same time

#define V2D_KI_DECSEQ_B_REGFB \
        ((uint64_t)1ull << V2BGEN(FI, DECSEQ, REGFB))  // v1: DEC_ONLY in Open_Type


/* V2D field/key - ioctl DEC_SEQDATA for hw op request */
#define V2D_FI_DECSEQ_SIZE       (0u) // seq.data size
#define V2D_FI_DECSEQ_FBMOD      (1u) // frame buffer mode
#define V2D_FI_DECSEQ_SCALE      (2u) // scale ratio (jpu only)
#define V2D_FI_DECSEQ_FBEXT      (3u) // ext. frame buffer count
#define V2D_FI_DECSEQ_MAX        (4u) // should not exceed 64

#define V2D_KI_DECSEQ_SIZE \
        ((uint64_t)1ull << V2DGEN(FI, DECSEQ, SIZE))   // v1: stream_size
#define V2D_KI_DECSEQ_FBMOD \
        ((uint64_t)1ull << V2DGEN(FI, DECSEQ, FBMOD))  // v1: dec_buffer|n|_t
/* [FIXME] mjpeg scale ratio: always set to 0 */
#define V2D_KI_DECSEQ_SCALE \
        ((uint64_t)1ull << V2DGEN(FI, DECSEQ, SCALE))  // v1: m_iMJPGScaleRatio
#define V2D_KI_DECSEQ_FBEXT \
        ((uint64_t)1ull << V2DGEN(FI, DECSEQ, FBEXT))  // v1: added by min. fb count

/* V2 respose fleid key/index: ioctl suffix |DEC_SEQDATA| */
/* fields */
#define V2D_FO_DECSEQ_ERRNO      (0u) // error reason (ENOMEM(+))
#define V2D_FO_DECSEQ_PIC_W      (1u) // frame width
#define V2D_FO_DECSEQ_PIC_H      (2u) // frame height
/* frame rate info (meaning is diff. per codec) */
// FRM_R: TBD ( cal. by vpu lib. )
#define V2D_FO_DECSEQ_FRM_R      (3u) // frame rate multiplied by 1000
/* frame buffer count from seqhdr(min)/memory(max) */
#define V2D_FO_DECSEQ_FRM_N      (4u) // min. fb slot num from seqhdr
#define V2D_FO_DECSEQ_FRM_X      (5u) // max. fb slot num from memory
/* window coordinate */
#define V2D_FO_DECSEQ_WIN_L      (6u) // crop left
#define V2D_FO_DECSEQ_WIN_T      (7u) // crop top
#define V2D_FO_DECSEQ_WIN_R      (8u) // crop right
#define V2D_FO_DECSEQ_WIN_B      (9u) // crop bottom
/* color aspect */
#define V2D_FO_DECSEQ_CA_FR      (10u) // full range
#define V2D_FO_DECSEQ_CA_PR      (11u) // primaries
#define V2D_FO_DECSEQ_CA_MC      (12u) // matrix Coeffs
#define V2D_FO_DECSEQ_CA_TR      (13u) // transfer
/* codec traits */
#define V2D_FO_DECSEQ_BTDP       (14u) // bit depth
#define V2D_FO_DECSEQ_RODP       (15u) // ReOrdering DePth
#define V2D_FO_DECSEQ_PRFL       (16u) // profile
#define V2D_FO_DECSEQ_LEVL       (17u) // level
#define V2D_FO_DECSEQ_TIER       (18u) // tier (not used)
#define V2D_FO_DECSEQ_INTL       (19u) // INTerLaced
#define V2D_FO_DECSEQ_CSAR       (20u) // aspect ratio csi
/* fb reg. information */
#define V2D_FO_DECSEQ_FRM_SZ     (21u) // min. fb slot size
#define V2D_FO_DECSEQ_FB_PA_H    (22u) // frame buffer start offset
#define V2D_FO_DECSEQ_FB_PA_L    (23u) //
/* codec specific information */
#define V2D_FO_DECSEQ_CNT        (24u) // num.  of csi
#define V2D_FO_DECSEQ_CSI        (25u) // array of csi
#define V2D_FO_DECSEQ_MAX        (26u) // should not exceed 64

/* keys */
#define V2D_KO_DECSEQ_ERRNO \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, ERRNO)) // |00| v1: m_iReportErrorReason
#define V2D_KO_DECSEQ_PIC_W \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, PIC_W)) // |01| v1: m_iPicWidth
#define V2D_KO_DECSEQ_PIC_H \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, PIC_H)) // |02| v1: m_iPicHeight
#define V2D_KO_DECSEQ_FRM_R \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, FRM_R)) // |03| v1: m_uiFrameRate|Res|Div
#define V2D_KO_DECSEQ_FRM_N \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, FRM_N)) // |04| v1: m_iMinFrameBufferCount
#define V2D_KO_DECSEQ_FRM_X \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, FRM_X)) // |05| v2: max. frm slot num
#define V2D_KO_DECSEQ_WIN_L \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, WIN_L)) // |06| v1: m_iCropLeft
#define V2D_KO_DECSEQ_WIN_T \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, WIN_T)) // |07| v1: m_iCropTop
#define V2D_KO_DECSEQ_WIN_R \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, WIN_R)) // |08| v1: m_iCropRight
#define V2D_KO_DECSEQ_WIN_B \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, WIN_B)) // |09| v1: m_iCropBottom

// CA: AVC: m_AvcVuiInfo, MPEG2: m_Mp2SeqDisplayExt, HEVC: Userdata
// - mpeg2 has no full range info (set to 0 externally)
#define V2D_KO_DECSEQ_CA_FR \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, CA_FR)) // |10| v1: m_VideoFullRangeFlag
#define V2D_KO_DECSEQ_CA_PR \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, CA_PR)) // |11| v1: m_ColourPrimaries
#define V2D_KO_DECSEQ_CA_MC \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, CA_MC)) // |12| v1: m_MatrixCoefficients
#define V2D_KO_DECSEQ_CA_TR \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, CA_TR)) // |13| v1: m_TransferCharacteristics

/* NOTE: m_iBitDepth is not used in U/S.
 *       replace m_iFrameBufferFormat with m_iBitDepth */
/* NOTE: m_iFrameBufferFormat is not used in U/S so far. */
/* [FIXME] should consider 12 bit cases (0: 8bit, other: 10 bit) */
#define V2D_KO_DECSEQ_BTDP \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, BTDP))  // |14| v1: m_iBitDepth
#define V2D_KO_DECSEQ_RODP \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, RODP))  // |15| v1: m_iFrameBufDelay
#define V2D_KO_DECSEQ_PRFL \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, PRFL))  // |16| v1: m_iProfile
#define V2D_KO_DECSEQ_LEVL \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, LEVL))  // |17| v1: m_iLevel
#define V2D_KO_DECSEQ_TIER \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, TIER))  // |18| v1: m_iTier (hevc)
#define V2D_KO_DECSEQ_INTL \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, INTL))  // |19| v1: m_iInterlace
#define V2D_KO_DECSEQ_CSAR \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, CSAR))  // |20| v1: m_iAspectRateInfo

// [FIXME] [21] below should be deprecated
#define V2D_KO_DECSEQ_FRM_SZ \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, FRM_SZ))  // |21| v1: m_iMinFrameBufferSize
#define V2D_KO_DECSEQ_FB_PA_H \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, FB_PA_H)) // |22| v1: m_FrameBufferStartAddr[PA]
#define V2D_KO_DECSEQ_FB_PA_L \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, FB_PA_L)) // |23|

/* codec specific decoded header information from v1: dec_initial_info_t */
#define V2D_KO_DECSEQ_CNT \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, CNT))  // |24| v2: CSI elements count
#define V2D_KO_DECSEQ_CSI \
        ((uint64_t)1ull << V2DGEN(FO, DECSEQ, CSI))  // |25| v2: CSI array


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// IOCTL: v1: V_DEC_REG_FRAME_BUFFER
//
// Remark: v2 now combines seq.header and reg fb ioctls into a single one
//         (do not use it anymore)

/* -----------------------------------------------------------------------------
 * Key/value index for v1 struct |{xxx_}dec_buffer_t|
 * : Remark: only flex ip is used (flex op is deprepated)
 * : D/D will set/sent it but result is set to flexio 'seqdata op'
 * -------------------------------------------------------------------------- */
/* V2D field/key - V2D_IP_DRV_REG */
/* fields */
/* [NOTE] all fields are used by vpu d/d <-> vpu fw */
#define V2D_FI_REG_FB_CNT        (0u) // frame buffer count
#define V2D_FI_REG_FB_PA_H       (1u) // frame buffer PA
#define V2D_FI_REG_FB_PA_L       (2u) //

#define V2D_FI_REG_SB_SIZE       (3u) // slice buffer size
#define V2D_FI_REG_SB_PA_H       (4u) // slice buffer PA
#define V2D_FI_REG_SB_PA_L       (5u) //

#define V2D_FI_REG_MB_SIZE       (6u) // macro block buffer size
#define V2D_FI_REG_MB_PA_H       (7u) // macro block buffer PA
#define V2D_FI_REG_MB_PA_L       (8u) //

#define V2D_FI_REG_MAX           (9u) // should not exceed 64

/* keys */
#define V2D_KI_REG_FB_CNT \
        ((uint64_t)1ull << V2DGEN(FI, REG, FB_CNT))  // |00| v1: m_iFrameBufferCount
#define V2D_KI_REG_FB_PA_H \
        ((uint64_t)1ull << V2DGEN(FI, REG, FB_PA_H)) // |01| v1: m_FrameBufferStartAddr[PA]
#define V2D_KI_REG_FB_PA_L \
        ((uint64_t)1ull << V2DGEN(FI, REG, FB_PA_L)) // |02|

#define V2D_KI_REG_SB_SIZE \
        ((uint64_t)1ull << V2DGEN(FI, REG, SB_SIZE)) // |03| v1: m_iAvcSliceSaveBufferSize
#define V2D_KI_REG_SB_PA_H \
        ((uint64_t)1ull << V2DGEN(FI, REG, SB_PA_H)) // |04| v1: m_AvcSliceSaveBufferAddr
#define V2D_KI_REG_SB_PA_L \
        ((uint64_t)1ull << V2DGEN(FI, REG, SB_PA_L)) // |05|

#define V2D_KI_REG_MB_SIZE \
        ((uint64_t)1ull << V2DGEN(FI, REG, MB_SIZE)) // |06| v1: m_iVp8MbDataSaveBufferSize
#define V2D_KI_REG_MB_PA_H \
        ((uint64_t)1ull << V2DGEN(FI, REG, MB_PA_H)) // |07| v1: m_Vp8MbDataSaveBufferAddr
#define V2D_KI_REG_MB_PA_L \
        ((uint64_t)1ull << V2DGEN(FI, REG, MB_PA_L)) // |08|

/*
 *  v1: ext.frame + min.frame = m_iFrameBufferCount
 *  v1: dec_buffer3_t (VPU-C5 only)
 */
// /* structural view-point (v2 i/f legacy candidate before v2 flex io) */
// /* v2d_framebuffer_iop_t: bearer for dec_buffer|x|_t  */
// struct v2d_framebuffer_iop_t {
//     /* 0: dec_buffer_t, 1: dec_buffer2_t, 2: dec_buffer3_t */
// 	   /* IN */ int m_FrameBufferType;
// 	   /* IN */int m_FrameBufferCount; // frame buffer count allocated
//     /* OUT */
//     union {
//         phys_addr_t m_FrameBuffers1;
//         phys_addr_t m_FrameBuffers2[32];
//         phys_addr_t m_FrameBuffers3[32][2];
//     };
// };

/* V2D field/key - V2D_OP_DRV_REG (none) */


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// IOCTL: V_DEC_DECODE

/* -----------------------------------------------------------------------------
 * Key/value index for struct |dec_input_t|
 * : buffer address remark: consider only PA field - 32 bit
 * -------------------------------------------------------------------------- */
/* fields */
#define V2D_FI_DECFRM_BS_PA_H    (0u)
#define V2D_FI_DECFRM_BS_PA_L    (1u)
#define V2D_FI_DECFRM_BS_SIZE    (2u)
// TODO: user data: R&R repositioning (user space -> vpu lib.)
#define V2D_FI_DECFRM_UD_PA_H    (3u)
#define V2D_FI_DECFRM_UD_PA_L    (4u)
#define V2D_FI_DECFRM_UD_VA_H    (5u) // [TODO] check if vpu really use it
#define V2D_FI_DECFRM_UD_VA_L    (6u)
#define V2D_FI_DECFRM_UD_SIZE    (7u)
#define V2D_FI_DECFRM_RA_MODE    (8u) // random access of decoding input frame
#define V2D_FI_DECFRM_MAX        (9u) // should not exceed 64

/* keys */
#define V2D_KI_DECFRM_BS_PA_H \
        ((uint64_t)1ull << V2DGEN(FI, DECFRM, BS_PA_H)) // |00| v1: m_BitstreamDataAddr[PA]
#define V2D_KI_DECFRM_BS_PA_L \
        ((uint64_t)1ull << V2DGEN(FI, DECFRM, BS_PA_L)) // |01|
#define V2D_KI_DECFRM_BS_SIZE \
        ((uint64_t)1ull << V2DGEN(FI, DECFRM, BS_SIZE)) // |02| v1: m_iBitstreamDataSize
#define V2D_KI_DECFRM_UD_PA_H  \
        ((uint64_t)1ull << V2DGEN(FI, DECFRM, UD_PA_H)) // |03| v1: m_UserDataAddr[PA]
#define V2D_KI_DECFRM_UD_PA_L  \
        ((uint64_t)1ull << V2DGEN(FI, DECFRM, UD_PA_L)) // |04|
#define V2D_KI_DECFRM_UD_VA_H  \
        ((uint64_t)1ull << V2DGEN(FI, DECFRM, UD_VA_H)) // |05| v1: m_UserDataAddr[K_VA]
#define V2D_KI_DECFRM_UD_VA_L  \
        ((uint64_t)1ull << V2DGEN(FI, DECFRM, UD_VA_L)) // |06|
#define V2D_KI_DECFRM_UD_SIZE  \
        ((uint64_t)1ull << V2DGEN(FI, DECFRM, UD_SIZE)) // |07| v1: m_iUserDataBufferSize
#define V2D_KI_DECFRM_RA_MODE  \
        ((uint64_t)1ull << V2DGEN(FI, DECFRM, RA_MODE)) // |08| v1: m_iFrameSearchEnable

/* boolean fields */
#define V2D_FI_DECFRM_B_RA_RORDR  (0u) // RA_KEY output reordering option
#define V2D_FI_DECFRM_B_SPRFRM_X  (1u) // superframe decoding repetition on d/d
#define V2D_FI_DECFRM_B_SKPFRM_O  (2u) // skip one frame iff skippable
#define V2D_FI_DECFRM_B_RNGFED_O  (3u) // update input size in vpu rng mode
#define V2D_FI_DECFRM_B_MAPDAT_O  (4u) // map-converted yuv output mode
#define V2D_FI_DECFRM_B_DELAYFRM  (5u) // delay decoding (mimic cq2)
#define V2D_FI_DECFRM_B_MAX       (6u) // should not exceed 64

/* boolean  keys (32bit field) */
#define V2D_KI_DECFRM_B_RA_RORDR \
        ((uint32_t)1u << V2BGEN(FI, DECFRM, RA_RORDR)) // v1: m_iSkipFrameMode |= ((uint64_t)1ull << 4u)
#define V2D_KI_DECFRM_B_SPRFRM_X \
        ((uint32_t)1u << V2BGEN(FI, DECFRM, SPRFRM_X)) // v1: m_Reserved[20] = 20
#define V2D_KI_DECFRM_B_SKPFRM_O \
        ((uint32_t)1u << V2BGEN(FI, DECFRM, SKPFRM_O)) // v1: m_iSkipFrameNum = 1
#define V2D_KI_DECFRM_B_RNGFED_O \
        ((uint32_t)1u << V2BGEN(FI, DECFRM, RNGFED_O))
#define V2D_KI_DECFRM_B_MAPDAT_O \
        ((uint32_t)1u << V2BGEN(FI, DECFRM, MAPDAT_O))
#define V2D_KI_DECFRM_B_DELAYFRM \
        ((uint32_t)1u << V2BGEN(FI, DECFRM, DELAYFRM))

/* -----------------------------------------------------------------------------
 * Key/value index for struct |dec_output_t|
 * : buffer address remark: consider only PA field - 32 bit
 * -------------------------------------------------------------------------- */
/* fields */
/* dec frame information */
#define V2D_FO_DECFRM_PICTY       (0u)

#define V2D_FO_DECFRM_OSLOT       (1u)
#define V2D_FO_DECFRM_ISLOT       (2u)
#define V2D_FO_DECFRM_OSTAT       (3u)
#define V2D_FO_DECFRM_ISTAT       (4u)

#define V2D_FO_DECFRM_INTL        (5u)
#define V2D_FO_DECFRM_ERRMB       (6u)

#define V2D_FO_DECFRM_CA_FR       (7u)
#define V2D_FO_DECFRM_CA_PR       (8u)
#define V2D_FO_DECFRM_CA_MC       (9u)
#define V2D_FO_DECFRM_CA_TR      (10u)

#define V2D_FO_DECFRM_FRM_W      (11u)
#define V2D_FO_DECFRM_FRM_H      (12u)

#define V2D_FO_DECFRM_WIN_L      (13u)
#define V2D_FO_DECFRM_WIN_T      (14u)
#define V2D_FO_DECFRM_WIN_R      (15u)
#define V2D_FO_DECFRM_WIN_B      (16u)

#define V2D_FO_DECFRM_PICSTRT    (17u)
#define V2D_FO_DECFRM_INTL_TFF   (18u) // top field first
#define V2D_FO_DECFRM_INTL_RFF   (19u) // repeat first field

#define V2D_FO_DECFRM_CSAR       (20u)
#define V2D_FO_DECFRM_BS_READ    (21u) // bitstream consumed byte

/* phy buffer access points */
#define V2D_FO_DECFRM_OUT_Y_PA_H     (22u)
#define V2D_FO_DECFRM_OUT_Y_PA_L     (23u)
#define V2D_FO_DECFRM_OUT_Y_VA_H     (24u)
#define V2D_FO_DECFRM_OUT_Y_VA_L     (25u)

#define V2D_FO_DECFRM_OUT_U_PA_H     (26u)
#define V2D_FO_DECFRM_OUT_U_PA_L     (27u)
#define V2D_FO_DECFRM_OUT_U_VA_H     (28u)
#define V2D_FO_DECFRM_OUT_U_VA_L     (29u)

#define V2D_FO_DECFRM_OUT_V_PA_H     (30u)
#define V2D_FO_DECFRM_OUT_V_PA_L     (31u)
#define V2D_FO_DECFRM_OUT_V_VA_H     (32u)
#define V2D_FO_DECFRM_OUT_V_VA_L     (33u)

#define V2D_FO_DECFRM_DEC_Y_PA_H     (34u)
#define V2D_FO_DECFRM_DEC_Y_PA_L     (35u)
#define V2D_FO_DECFRM_DEC_Y_VA_H     (36u)
#define V2D_FO_DECFRM_DEC_Y_VA_L     (37u)

#define V2D_FO_DECFRM_DEC_U_PA_H     (38u)
#define V2D_FO_DECFRM_DEC_U_PA_L     (39u)
#define V2D_FO_DECFRM_DEC_U_VA_H     (40u)
#define V2D_FO_DECFRM_DEC_U_VA_L     (41u)

#define V2D_FO_DECFRM_DEC_V_PA_H     (42u)
#define V2D_FO_DECFRM_DEC_V_PA_L     (43u)
#define V2D_FO_DECFRM_DEC_V_VA_H     (44u)
#define V2D_FO_DECFRM_DEC_V_VA_L     (45u)

#define V2D_FO_DECFRM_UD_PA_H        (46u) // userdata buffer r offset
#define V2D_FO_DECFRM_UD_PA_L        (47u)
#define V2D_FO_DECFRM_UD_VA_H        (48u)
#define V2D_FO_DECFRM_UD_VA_L        (49u)

#define V2D_FO_DECFRM_CNT       (50u) // CSI element count
#define V2D_FO_DECFRM_CSI       (51u)

#define V2D_FO_DECFRM_MAX       (52u) // should not exceed 64

/* keys */
#define V2D_KO_DECFRM_PICTY  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, PICTY))   // |00| v1: m_iPicType
#define V2D_KO_DECFRM_OSLOT  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, OSLOT))   // |01| v1: m_iDispOutIdx
#define V2D_KO_DECFRM_ISLOT  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, ISLOT))   // |02| v1: m_iDecodedIdx
#define V2D_KO_DECFRM_OSTAT  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, OSTAT))   // |03| v1: m_iOutputStatus
#define V2D_KO_DECFRM_ISTAT  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, ISTAT))   // |04| v1: m_iDecodingStatus
#define V2D_KO_DECFRM_INTL  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, INTL))    // |05| v1: m_iInterlacedFrame
#define V2D_KO_DECFRM_ERRMB \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, ERRMB))   // |06| v1: m_iNumOfErrMBs

// v1: m_AvcVuiInfo | m_Mp2SeqDisplayExt
#define V2D_KO_DECFRM_CA_FR  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, CA_FR))   // |07| v1: 0 in mpeg2
#define V2D_KO_DECFRM_CA_PR  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, CA_PR))   // |08|
#define V2D_KO_DECFRM_CA_MC  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, CA_MC))   // |09|
#define V2D_KO_DECFRM_CA_TR  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, CA_TR))   // |10|

#define V2D_KO_DECFRM_FRM_W  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, FRM_W))   // |11| v1: m_iWidth
#define V2D_KO_DECFRM_FRM_H  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, FRM_H))   // |12| v1: m_iHeight

// v1: pic_crop_t
#define V2D_KO_DECFRM_WIN_L  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, WIN_L))   // |13|
#define V2D_KO_DECFRM_WIN_T  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, WIN_T))   // |14|
#define V2D_KO_DECFRM_WIN_R  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, WIN_R))   // |15|
#define V2D_KO_DECFRM_WIN_B  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, WIN_B))   // |16|

// v1: pic sub type & interlace field info
#define V2D_KO_DECFRM_PICSTRT  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, PICSTRT))  // |17| v1: m_iPictureStructure
#define V2D_KO_DECFRM_INTL_TFF \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, INTL_TFF)) // |18| v1: m_iTopFieldFirst
#define V2D_KO_DECFRM_INTL_RFF \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, INTL_RFF)) // |19| v1: m_iRepeatFirstField

#define V2D_KO_DECFRM_CSAR  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, CSAR))     // |20| v1: m_iAspectRateInfo
#define V2D_KO_DECFRM_BS_READ  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, BS_READ))  // |21| v1: m_iConsumedBytes

#define V2D_KO_DECFRM_OUT_Y_PA_H  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, OUT_Y_PA_H))    // |22| v1: m_pDispOut[PA][Y]
#define V2D_KO_DECFRM_OUT_Y_PA_L  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, OUT_Y_PA_L))    // |23|
#define V2D_KO_DECFRM_OUT_Y_VA_H  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, OUT_Y_VA_H))    // |24| v1: m_pDispOut[VA][Y]
#define V2D_KO_DECFRM_OUT_Y_VA_L  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, OUT_Y_VA_L))    // |25|

#define V2D_KO_DECFRM_OUT_U_PA_H  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, OUT_U_PA_H))    // |26| v1: m_pDispOut[PA][U]
#define V2D_KO_DECFRM_OUT_U_PA_L  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, OUT_U_PA_L))    // |27|
#define V2D_KO_DECFRM_OUT_U_VA_H  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, OUT_U_VA_H))    // |28| v1: m_pDispOut[VA][U]
#define V2D_KO_DECFRM_OUT_U_VA_L  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, OUT_U_VA_L))    // |29|

#define V2D_KO_DECFRM_OUT_V_PA_H  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, OUT_V_PA_H))    // |30| v1: m_pDispOut[PA][V]
#define V2D_KO_DECFRM_OUT_V_PA_L  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, OUT_V_PA_L))    // |31|
#define V2D_KO_DECFRM_OUT_V_VA_H  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, OUT_V_VA_H))  // |32| v1: m_pDispOut[VA][V]
#define V2D_KO_DECFRM_OUT_V_VA_L  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, OUT_V_VA_L))  // |33|

#define V2D_KO_DECFRM_DEC_Y_PA_H  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, DEC_Y_PA_H))  // |34| v1: m_pCurrOut[PA][Y]
#define V2D_KO_DECFRM_DEC_Y_PA_L  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, DEC_Y_PA_L))  // |35|
#define V2D_KO_DECFRM_DEC_Y_VA_H  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, DEC_Y_VA_H))  // |36| v1: m_pCurrOut[VA][Y]
#define V2D_KO_DECFRM_DEC_Y_VA_L  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, DEC_Y_VA_L))  // |37|

#define V2D_KO_DECFRM_DEC_U_PA_H  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, DEC_U_PA_H))  // |38| v1: m_pCurrOut[PA][U]
#define V2D_KO_DECFRM_DEC_U_PA_L  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, DEC_U_PA_L))  // |39|
#define V2D_KO_DECFRM_DEC_U_VA_H  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, DEC_U_VA_H))  // |40| v1: m_pCurrOut[VA][U]
#define V2D_KO_DECFRM_DEC_U_VA_L  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, DEC_U_VA_L))  // |41|

#define V2D_KO_DECFRM_DEC_V_PA_H  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, DEC_V_PA_H))  // |42| v1: m_pCurrOut[PA][V]
#define V2D_KO_DECFRM_DEC_V_PA_L  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, DEC_V_PA_L))  // |43|
#define V2D_KO_DECFRM_DEC_V_VA_H  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, DEC_V_VA_H))  // |44| v1: m_pCurrOut[VA][V]
#define V2D_KO_DECFRM_DEC_V_VA_L  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, DEC_V_VA_L))  // |45|

#define V2D_KO_DECFRM_UD_PA_H  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, UD_PA_H)) // |46| v1: m_UserDataAddress[PA]
#define V2D_KO_DECFRM_UD_PA_L  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, UD_PA_L)) // |47|
#define V2D_KO_DECFRM_UD_VA_H  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, UD_VA_H)) // |48| v1: m_UserDataAddress[VA]
#define V2D_KO_DECFRM_UD_VA_L  \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, UD_VA_L)) // |49|

/* codec specific decoded frame information from v1: dec_output_t */
#define V2D_KO_DECFRM_CNT \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, CNT))      // |50| v2: CSI elements count
#define V2D_KO_DECFRM_CSI \
        ((uint64_t)1ull << V2DGEN(FO, DECFRM, CSI))      // |51| v2: CSI array

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// IOCTL: V2D_IP_FRM_DRAIN (v1: V_DEC_FLUSH_OUTPUT)

/* -----------------------------------------------------------------------------
 * Key/value index for struct |dec_input_t|
 * : buffer address remark: consider only PA field - 32 bit
 * -------------------------------------------------------------------------- */
/* FI fields */
#define V2D_FI_DRAIN_UD_PA_H    (0u)
#define V2D_FI_DRAIN_UD_PA_L    (1u)
#define V2D_FI_DRAIN_UD_VA_H    (2u) // [TODO] check if vpu really use it
#define V2D_FI_DRAIN_UD_VA_L    (3u)
#define V2D_FI_DRAIN_UD_SIZE    (4u)
#define V2D_FI_DRAIN_MAX        (5u) // should not exceed 64

/* keys */
#define V2D_KI_DRAIN_UD_PA_H  \
        ((uint64_t)1ull << V2DGEN(FI, DRAIN, UD_PA_H)) // |00| v1: m_UserDataAddr[PA]
#define V2D_KI_DRAIN_UD_PA_L  \
        ((uint64_t)1ull << V2DGEN(FI, DRAIN, UD_PA_L)) // |01|
#define V2D_KI_DRAIN_UD_VA_H  \
        ((uint64_t)1ull << V2DGEN(FI, DRAIN, UD_VA_H)) // |02| v1: m_UserDataAddr[K_VA]
#define V2D_KI_DRAIN_UD_VA_L  \
        ((uint64_t)1ull << V2DGEN(FI, DRAIN, UD_VA_L)) // |03|
#define V2D_KI_DRAIN_UD_SIZE  \
        ((uint64_t)1ull << V2DGEN(FI, DRAIN, UD_SIZE)) // |04| v1: m_iUserDataBufferSize

/* FO fields : same as the FO of DECFRM */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// IOCTL: V2D_IP_FRM_CLEAR (v1: op of V_DEC_BUF_FLAG_CLEAR)
/* FI fields */
#define V2D_FI_CLEAR_FB_IDX    (0u) // should not exceed 31 [TODO] double-check
#define V2D_FI_CLEAR_MAX       (1u) // should not exceed 64

/* keys */
#define V2D_KI_CLEAR_FB_IDX \
        ((uint64_t)1ull << V2DGEN(FI, CLEAR, FB_IDX)) // |00| v1: fb slot index

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// IOCTL: V2D_IP_FRM_FLUSH (v1: op of a bunch of V_DEC_BUF_FLAG_CLEAR)
/* FI fields */
#define V2D_FI_FLUSH_FB_MAX    (0u) // should not exceed 31 [TODO] double-check
#define V2D_FI_FLUSH_MAX       (1u) // should not exceed 64

/* keys */
#define V2D_KI_FLUSH_FB_MAX \
        ((uint64_t)1ull << V2DGEN(FI, FLUSH, FB_MAX)) // |00| v1: total num of fb slots

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// IOCTL: V2D_IP_DRV_RST (v1: V_DEC_CLOSE)
/* FI fields: none */
/* FO fields: none */


/* -------------------------------------------------------------------------- */
/* For Ring-Buffer Mode Only ------------------------------------------------ */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// IOCTL: (v1: V_FILL_RING_BUFFER_AUTO) (deprecated)
// - v1: dec_ring_buffer_setting_in_t
// - vdec_k: vdec_ring_buffer_set_t
/* -----------------------------------------------------------------------------
 * Key/value index for v1 struct |dec_ring_buffer_setting_in_t| : deprecated
 * -------------------------------------------------------------------------- */
// /* keys */
// #define V2D_FO_RNG_FILL_KVA    (0u)
// #define V2D_FO_RNG_FILL_SIZE   (1u)
// #define V2D_FO_RNG_FILL_MAX    (2u) // should not exceed 64
// #endif

// /* values */
// #define V2D_KO_RNG_FILL_KVA
//         ((uint64_t)1ull << V2DGEN(FO, RNG, FILL_KVA))  // |00| v1: m_OnePacketBufferAddr
// #define V2D_KO_RNG_FILL_SIZE
//         ((uint64_t)1ull << V2DGEN(FO, RNG, FILL_SIZE)) // |01| v1: m_iOnePacketBufferSize

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// IOCTL: V2D_OP_RNG_GETPOS (v1: V_GET_RING_BUFFER_STATUS_RESULT)

/* -----------------------------------------------------------------------------
 * Key/value index for v1 struct |dec_ring_buffer_status_out_t|
 * -------------------------------------------------------------------------- */
/* fields */
#define V2D_FO_RNG_AVAIL       (0u)
#define V2D_FO_RNG_RPOS        (1u)
#define V2D_FO_RNG_WPOS        (2u)
#define V2D_FO_RNG_GETPOS_MAX  (3u) // should not exceed 64

/* keys */
#define V2D_KO_RNG_AVAIL  \
        ((uint64_t)1ull << V2DGEN(FO, RNG, AVAIL)) // |0| v1: m_ulAvailableSpaceInRingBuffer
#define V2D_KO_RNG_RPOS  \
        ((uint64_t)1ull << V2DGEN(FO, RNG, RPOS))  // |01| v1: m_ptrReadAddr_PA
#define V2D_KO_RNG_WPOS  \
        ((uint64_t)1ull << V2DGEN(FO, RNG, WPOS))  // |02| v1: m_ptrWriteAddr_PA

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// IOCTL: V2D_IP_RNG_SETPOS (v1: V_DEC_UPDATE_RINGBUF_WP)
/* -----------------------------------------------------------------------------
 * Key/value index for v1 struct |VDEC_RINGBUF_SETBUF_PTRONLY_t|
 * -------------------------------------------------------------------------- */
/* fields */
#define V2D_FI_RNG_WRITTEN    (0u)
#define V2D_FI_RNG_REWIND     (1u)
#define V2D_FI_RNG_SETPOS_MAX (2u) // should not exceed 64

/* keys */
#define V2D_KI_RNG_WRITTEN  \
        ((uint64_t)1ull << V2DGEN(FI, RNG, WRITTEN)) // |00| v1: iCopiedSize
#define V2D_KI_RNG_REWIND   \
        ((uint64_t)1ull << V2DGEN(FI, RNG, REWIND))  // |01| v1: iFlushBuf


/* -------------------------------------------------------------------------- */
/* VD hw memory buffer (for s/w decoder only) ------------------------------- */

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// IOCTL: V2D_IP_MEM_ASGN (v1: V_DEC_ALLOC_MEMORY)
/* -----------------------------------------------------------------------------
 * Key/value index for v1 struct |MEM_ALLOC_INFO_t|
 * -------------------------------------------------------------------------- */
/* V2D field/key - ioctl MEM_ASGN for hw ip request */
#define V2D_FI_HWBUF_TYPE    (0u)
#define V2D_FI_HWBUF_SIZE    (1u)
#define V2D_FI_HWBUF_PA_H    (2u)
#define V2D_FI_HWBUF_PA_L    (3u)
#define V2D_FI_HWBUF_MAX     (4u) // should not exceed 64

/* keys */
#define V2D_KI_HWBUF_TYPE \
        ((uint64_t)1ull << V2DGEN(FI, HWBUF, TYPE)) // |00| v1: buffer_type
#define V2D_KI_HWBUF_SIZE \
        ((uint64_t)1ull << V2DGEN(FI, HWBUF, SIZE)) // |01| v1: request_size
#define V2D_KI_HWBUF_PA_H \
        ((uint64_t)1ull << V2DGEN(FI, HWBUF, PA_H)) // |02| v1: phy_addr
#define V2D_KI_HWBUF_PA_L \
        ((uint64_t)1ull << V2DGEN(FI, HWBUF, PA_L)) // |03|

/* V2D field/key - ioctl MEM_ASGN for hw op response */
#define V2D_FO_HWBUF_TYPE    (0u)
#define V2D_FO_HWBUF_SIZE    (1u)
#define V2D_FO_HWBUF_PA_H    (2u)
#define V2D_FO_HWBUF_PA_L    (3u)
#define V2D_FO_HWBUF_MAX     (4u) // should not exceed 64

/* keys */
#define V2D_KO_HWBUF_TYPE \
        ((uint64_t)1ull << V2DGEN(FO, HWBUF, TYPE)) // |00| v1: buffer_type
#define V2D_KO_HWBUF_SIZE \
        ((uint64_t)1ull << V2DGEN(FO, HWBUF, SIZE)) // |01| v1: request_size
#define V2D_KO_HWBUF_PA_H \
        ((uint64_t)1ull << V2DGEN(FO, HWBUF, PA_H)) // |02| v1: phy_addr
#define V2D_KO_HWBUF_PA_L \
        ((uint64_t)1ull << V2DGEN(FO, HWBUF, PA_L)) // |03|

#endif  // _VPU2_DIO_FIELDS_H_
