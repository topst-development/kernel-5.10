/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef _VPU2_DIO_CSBLOB_H_
#define _VPU2_DIO_CSBLOB_H_

// work-log: TCC_VPU_D6.h
// work-log: TCC_VPU_C5_CODEC.h
// work-log: TCC_VPU_C7_CODEC.h
// work-log: TCC_VPU_4K_D2.h
// work-log: TCC_JPU_C6.h
// work-log: TCC_HEVCDEC.h
// work-log: TCC_VP9DEC.h (v2: deprecated)

/* -----------------------------------------------------------------------------
 * remark
 * -----------------------------------------------------------------------------
 * HEVC CSI: accessed by v1 struct: v2d_4kd2_userdata_t directly (VIOC)
 * -------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
 * macro: typing shortcut generator
 * -------------------------------------------------------------------------- */
#define V2DGEN_CSI(__level1, __level2, __level3) \
	V2D_CSI_##__level1##_##__level2##_##__level3

#define V2DGEN_CSISEQ(__level1, __level2, level3) \
	V2D_CSI_##__level1##_DECSEQ_##__level2##_##__level3

#define V2DGEN_CSIFRM(__level1, __level2, level3) \
	V2D_CSI_##__level1##_DECFRM_##__level2##_##__level3

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// IOCTL: v1: V_DEC_SEQ_HEADER

/* -----------------------------------------------------------------------------
 * Key/value index for v1 struct |dec_initial_info_t|
 * -------------------------------------------------------------------------- */
/*
 * V2 respose fleid key/index: ioctl suffix |DEC_SEQDATA|
 */
/* codec specific decoded header information from v1: dec_initial_info_t */
#if 0
#define V2D_KO_DECSEQ_CNT \
	((uint64_t)1ull << V2DGEN(FO, DECSEQ, CNT))  // |21| v2: CSI elements count
#define V2D_KO_DECSEQ_CSI \
	((uint64_t)1ull << V2DGEN(FO, DECSEQ, CSI))  // |22| v2: CSI array
#endif
/*
 * CSI blob map for DECSEQ: will contain the informations below
 */
// typedef union v2dec_header_t {
//     /* AVC: size of 4 bytes */
//     struct AvcHeaderCsiInfo_t {
//         int m_iAvcConstraintSetFlag_3; // used to make level in H.264 only
//     } m_AvcCsiInfo;
//
//     /* MPEG4: size of 32 bytes */
//     /* redundant info on a user-space side */
//     //
//     // struct Mpeg4HeaderCsiInfo_t {
//     //    int m_iM4vDataPartitionEnable;
//     //    int m_iM4vReversibleVlcEnable;
//     //    int m_iM4vShortVideoHeader; // 0: disable, 1: enable
//     //    int m_iM4vH263AnnexJEnable; // 0: disable, 1: enable
//     // } m_Mpeg4CsiInfo;
//
//     /* VC1: size of 4 bytes */
//     /* RFU: bcs of exact determination of interlace if required */
//     struct Vc1HeaderCsiInfo_t {
//         int m_iVc1Psf; // the value of "Progressive Segmented Frame"
//     } m_Vc1CsiInfo;
//
//     /* MJPEG: size of 48 bytes (VPU-C5, JPU-C6) */
//     struct MjpegHeaderCsiInfo_t {
//         /* chroma format (only for TCC891x/88xx/93XX)
//          * - 0: 4:2:0
//          * - 1: 4:2:2
//          * - 2: 4:2:2 vertical
//          * - 3: 4:4:4
//          * - 4: 4:0:0
//          */
//         int m_iMjpg_sourceFormat;
//         /* set if thumbnail image exists */
//         int m_iMjpg_ThumbnailEnable;
//         /* minimum frame buffer size
//          * - index 0: original size
//          * - index 1: 1/2 Scaling Down
//          * - index 2: 1/4 Scaling Down
//          * - index 3: 1/8 Scaling Down
//          */
//         int m_iMjpg_MinFrameBufferSize[4];
//     } m_MjpegCsiInfo;
//
//     /* RV: size of 4 bytes (VPU-C7 only) */
//     struct RvHeaderCsiInfo_t {
//         int m_iRvTimestamp; // reference pts in millisecond
//     } m_RvCsiInfo;
//
//     /* HEVC: size of 4 bytes (4K-D2 user-data) */
//     struct HevcHeaderCsiInfo_t {
//         struct v2d_hevc_userdata_t {
//             /* |hevc_vui_param_t| */
//             /* m_VuiParam: aspect ratio */
//             int aspect_ratio_idc;
//             int sar_width;
//             int sar_height;
//
//             /* m_VuiParam: color aspect */
//             int video_full_range_flag;
//             int colour_primaries;
//             int transfer_characteristics; // 16: HDR, 18:HLG
//             int matrix_coefficients;
//
//             /* |hevc_mastering_display_colour_volume_t| */
//             /* m_MasteringDisplayColorVolume */
//             int	display_primaries_x_0;
//             int	display_primaries_x_1;
//             int	display_primaries_x_2;
//             int	display_primaries_y_0;
//             int	display_primaries_y_1;
//             int	display_primaries_y_2;
//             int	white_point_x;
//             int	white_point_y;
//             int	max_display_mastering_luminance;
//             int	min_display_mastering_luminance;
//
//             /* |hevc_content_light_level_info_t| */
//             /* m_ContentLightLevelInfo */
//             int max_content_light_level;
//             int max_pic_average_light_level;
//
//             /* |hevc_alternative_transfer_characteristics_info_t| */
//             /* m_AlternativeTransferCharacteristicsInfo */
//             int	preferred_transfer_characteristics; // eotf helper
//         } m_UserDataInfo;
//     } m_HevcCsiInfo;
//
// } v2dec_cs_header_info_t; // sizeof(union max) = 48 bytes

/* [MP4] v1: Mpeg4HeaderCsiInfo_t (not used) */
/* [VC1] v1: Vc1HeaderCsiInfo_t   (not used) */
/* [RV]  v1: RvHeaderCsiInfo_t    (not used) */

/*
 * [AVC] v1: AvcHeaderCsiInfo_t (used to make level)
 */
#define V2D_CSI_FO_DECSEQ_AVC_CONSTRAINT_SET3  (0u) // constraint set flag 3
#define V2D_CSI_FO_DECSEQ_AVC_MAX    (1u) // should not exceed 31

#define V2D_CSI_KO_DECSEQ_AVC_CONSTRAINT_SET3  \
	((uint64_t)1ull << V2DGEN_CSISEQ(FO, AVC, CONSTRAINT_SET3))
/*
 * [MJPG] v1: MjpegHeaderCsiInfo_t
 */
#define V2D_CSI_FO_DECSEQ_JPG_MATRIX  (0u)
#define V2D_CSI_FO_DECSEQ_JPG_MAX     (1u) // should not exceed 31

#define V2D_CSI_KO_DECSEQ_JPG_MATRIX \
	((uint64_t)1ull << V2DGEN_CSI(FO, DECSEQ_JPG, MATRIX)) // |00| v1: m_iSourceFormat

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// IOCTL: V_DEC_DECODE

/* -----------------------------------------------------------------------------
 * codec specific decoded frame information from v1: dec_output_info_t
 * -------------------------------------------------------------------------- */
// #define V2D_CSI_FO_DECFRM_CNT       (29u) // CSI element count
// #define V2D_CSI_FO_DECFRM_CSI       (30u)
/*
 * CSI for DECFRM: contain the informations below
 */
// typedef union v2dec_output_t {
//     /* VC1: size of 8 bytes */
//     struct Vc1FrameCsiInfo_t {
//         int m_iVc1HScaleFlag;
//         int m_iVc1VScaleFlag;
//     } m_Vc1FrameCsiInfo;
//
//     /* MPEG-2: size of 20 bytes */
//     struct Mpeg2FrameCsiInfo_t {
//         int m_iM2vProgressiveFrame;
//         int m_iM2vProgressiveSequence;
//         int m_iM2vFieldSequence;
//         int m_iM2vAspectRatio;
//         int m_iM2vFrameRate;
//     } m_Mpeg2FrameCsiInfo;
//
//     /* VP8: size: 36 bytes */
//     struct Vp8FrameCsiInfo_t {
//         struct Vp8DecScaleInfo_t {
//             int m_iHscaleFactor;
//             int m_iVscaleFactor;
//             int m_iPicWidth;
//             int m_iPicHeight;
//         } m_Vp8ScaleInfo;
//
//         struct Vp8DecPicInfo_t {
//             int m_iShowFrame;
//         } m_Vp8PicInfo;
//     } m_Vp8FrameCsiInfo;
//
//     /* MVC: size of 80 bytes */
//     struct MvcFrameCsiInfo_t {
//         struct MvcPictureInfo_t {
//             int m_iViewIdxDisplay;
//             int m_iViewIdxDecoded;
//         } m_MvcPicInfo;
//
//         /* frame packing arrangement information from SEI (VPU-D6 only) */
//         struct MvcAvcFpaSei_t {
//             int m_iexist;
//             int m_iframe_packing_arrangement_id;
//             int m_iframe_packing_arrangement_cancel_flag;
//             int m_iquincunx_sampling_flag;
//             int m_ispatial_flipping_flag;
//             int m_iframe0_flipped_flag;
//             int m_ifield_views_flag;
//             int m_icurrent_frame_is_frame0_flag;
//             int m_iframe0_self_contained_flag;
//             int m_iframe1_self_contained_flag;
//             int m_iframe_packing_arrangement_extension_flag;
//             int m_iframe_packing_arrangement_type;
//             int m_icontent_interpretation_type;
//             int m_iframe0_grid_position_x;
//             int m_iframe0_grid_position_y;
//             int m_iframe1_grid_position_x;
//             int m_iframe1_grid_position_y;
//             int m_iframe_packing_arrangement_repetition_period;
//         } m_MvcAvcFpaSei;
//     } m_MvcFrameCsiInfo;
//
//     /* AVC: size of 72 bytes */
//     struct AvcFrameCsiInfo_t {
//         /* frame packing arrangement information from SEI (VPU-D6 only) */
//         struct MvcAvcFpaSei_t {
//             int m_iexist;
//             int m_iframe_packing_arrangement_id;
//             int m_iframe_packing_arrangement_cancel_flag;
//             int m_iquincunx_sampling_flag;
//             int m_ispatial_flipping_flag;
//             int m_iframe0_flipped_flag;
//             int m_ifield_views_flag;
//             int m_icurrent_frame_is_frame0_flag;
//             int m_iframe0_self_contained_flag;
//             int m_iframe1_self_contained_flag;
//             int m_iframe_packing_arrangement_extension_flag;
//             int m_iframe_packing_arrangement_type;
//             int m_icontent_interpretation_type;
//             int m_iframe0_grid_position_x;
//             int m_iframe0_grid_position_y;
//             int m_iframe1_grid_position_x;
//             int m_iframe1_grid_position_y;
//             int m_iframe_packing_arrangement_repetition_period;
//         } m_MvcAvcFpaSei;
//     } m_AvcFrameCsiInfo;
//
//     /* HEVC: size of 124 (0|44|80|124) bytes (4K-D2 user-data + map-data) */
//     struct HevcFrameCsiInfo_t {
//         // [TODO] consider tcc_video_private.h
//         /* map-data (map-converted info): 44 bytes */
//         struct v2d_cprs_mapdata_t {
//             // l: luma, c: chroma
//             uintptr_t cmpr_l_pa;     // v1: m_CompressedY[PA]
//             uintptr_t cmpr_l_va;     // v1: m_CompressedY[VA]
//
//             uintptr_t cmpr_c_pa;     // v1: m_CompressedCb[PA]
//             uintptr_t cmpr_c_va;     // v1: m_CompressedCb[VA]
//
//             uintptr_t fbc_l_off_pa;  // v1: m_FbcYOffsetAddr[PA]
//             uintptr_t fbc_l_off_va;  // v1: m_FbcYOffsetAddr[VA]
//
//             uintptr_t fbc_c_off_pa;  // v1: m_FbcCOffsetAddr[PA]
//             uintptr_t fbc_c_off_va;  // v1: m_FbcCOffsetAddr[VA]
//
//             int32_t cmpr_tbl_l_size; // v1: m_uiCompressionTableLumaSize
//             int32_t cmpr_tbl_c_size; // v1: m_uiCompressionTableChromaSize
//             int32_t l_stride;        // v1: m_uiLumaStride
//             int32_t c_stride;        // v1: m_uiChromaStride
//
//             int32_t l_bitdep;        // v1: m_uiLumaBitDepth
//             int32_t c_bitdep;        // v1: m_uiChromaBitDepth
//             int32_t endian;          // v1: m_uiFrameEndian
//
//             int32_t codec_type;      // v1: m_Reserved[0]: HEVC(15), VP9(16)
//         } m_CprsMapData;
//
//         /* user-data: 80 bytes */
//         struct v2d_hevc_userdata_t {
//             /* |hevc_vui_param_t| */
//             /* m_VuiParam: aspect ratio */
//             int aspect_ratio_idc;
//             int sar_width;
//             int sar_height;
//
//             /* m_VuiParam: color aspect */
//             int video_full_range_flag;
//             int colour_primaries;
//             int transfer_characteristics;
//             int matrix_coefficients;
//
//             /* |hevc_mastering_display_colour_volume_t| */
//             /* m_MasteringDisplayColorVolume */
//             int display_primaries_x_0;
//             int display_primaries_x_1;
//             int display_primaries_x_2;
//             int display_primaries_y_0;
//             int display_primaries_y_1;
//             int display_primaries_y_2;
//             int white_point_x;
//             int white_point_y;
//             int max_display_mastering_luminance;
//             int min_display_mastering_luminance;
//
//             /* |hevc_content_light_level_info_t| */
//             /* m_ContentLightLevelInfo */
//             int max_content_light_level;
//             int max_pic_average_light_level;
//
//             /* |hevc_alternative_transfer_characteristics_info_t| */
//             /* m_AlternativeTransferCharacteristicsInfo */
//             int	preferred_transfer_characteristics; // eotf helper
//         } m_UserDataInfo;
//     } m_HevcFrameCsiInfo;
//
//     /* VP9: size of (0|92) bytes (4K-D2 map-data) */
//     struct Vp9FrameCsiInfo_t {
//         /* map-data (map-converted info): 92 bytes */
//         struct v2d_cprs_mapdata_t {
//             // l: luma, c: chroma
//             uintptr_t cmpr_l_pa;     // v1: m_CompressedY[PA]
//             uintptr_t cmpr_l_va;     // v1: m_CompressedY[VA]
//
//             uintptr_t cmpr_c_pa;     // v1: m_CompressedCb[PA]
//             uintptr_t cmpr_c_va;     // v1: m_CompressedCb[VA]
//
//             uintptr_t fbc_l_off_pa;  // v1: m_FbcYOffsetAddr[PA]
//             uintptr_t fbc_l_off_va;  // v1: m_FbcYOffsetAddr[VA]
//
//             uintptr_t fbc_c_off_pa;  // v1: m_FbcCOffsetAddr[PA]
//             uintptr_t fbc_c_off_va;  // v1: m_FbcCOffsetAddr[VA]
//
//             int32_t cmpr_tbl_l_size; // v1: m_uiCompressionTableLumaSize
//             int32_t cmpr_tbl_c_size; // v1: m_uiCompressionTableChromaSize
//             int32_t l_stride;        // v1: m_uiLumaStride
//             int32_t c_stride;        // v1: m_uiChromaStride
//
//             int32_t l_bitdep;        // v1: m_uiLumaBitDepth
//             int32_t c_bitdep;        // v1: m_uiChromaBitDepth
//             int32_t endian;          // v1: m_uiFrameEndian
//
//             int32_t codec_type;      // v1: m_Reserved[0]: HEVC(15), VP9(16)
//         } m_CprsMapData;
//     } m_Vp9FrameCsiInfo;
//
// } v2dec_cs_output_info_t; // sizeof(union max) = 80 bytes

/* [VC1] v2: Vc1FrameCsiInfo_t (not used) */
/* [MVC] v2: MvcFrameCsiInfo_t (not used) */
/* [AVC] v2: AvcFrameCsiInfo_t (not used) */

/*
 * [MP2] v2: Mpeg2FrameCsiInfo_t
 */
#define V2D_CSI_FO_DECFRM_MP2_PRGRSSV  (0u)  // v1: m_iM2vProgressiveFrame
#define V2D_CSI_FO_DECFRM_MP2_PRGRSEQ  (1u)  // v1: m_iM2vProgressiveSequence
#define V2D_CSI_FO_DECFRM_MP2_AR       (2u)  // v1: m_iM2vAspectRatio
#define V2D_CSI_FO_DECFRM_MP2_FIELDSEQ (3u)  // v1: m_iM2vFieldSequence
#define V2D_CSI_FO_DECFRM_MP2_FRMRATE  (4u)  // v1: m_iM2vFrameRate
#define V2D_CSI_FO_DECFRM_MP2_MAX      (5u)  // should not exceed 31

#define V2D_CSI_KO_DECFRM_MP2_PRGRSSV  \
	((uint64_t)1ull << V2DGEN_CSI(FO, DECFRM_MP2, PRGRSSV))  // |00|
#define V2D_CSI_KO_DECFRM_MP2_PRGRSEQ  \
	((uint64_t)1ull << V2DGEN_CSI(FO, DECFRM_MP2, PRGRSEQ))  // |01|
#define V2D_CSI_KO_DECFRM_MP2_AR  \
	((uint64_t)1ull << V2DGEN_CSI(FO, DECFRM_MP2, AR))       // |02|
#define V2D_CSI_KO_DECFRM_MP2_FIELDSEQ \
	((uint64_t)1ull << V2DGEN_CSI(FO, DECFRM_MP2, FIELDSEQ)) // |03|
#define V2D_CSI_KO_DECFRM_MP2_FRMRATE  \
	((uint64_t)1ull << V2DGEN_CSI(FO, DECFRM_MP2, FRMRATE))  // |04|
/*
 * [VP8] v2: Vp8FrameCsiInfo_t
 */
// Vp8DecScaleInfo_t
#define V2D_CSI_FO_DECFRM_VP8_SCALE_H  (0u)  // v1: m_iHscaleFactor
#define V2D_CSI_FO_DECFRM_VP8_SCALE_V  (1u)  // v1: m_iVscaleFactor
#define V2D_CSI_FO_DECFRM_VP8_SCPIC_W  (2u)  // v1: m_iPicWidth
#define V2D_CSI_FO_DECFRM_VP8_SCPIC_H  (3u)  // v1: m_iPicHeight
											 // Vp8DecPicInfo_t
#define V2D_CSI_FO_DECFRM_VP8_SHOWFRM  (4u)  // v1: m_iShowFrame
#define V2D_CSI_FO_DECFRM_VP8_MAX      (5u)  // should not exceed 31

#define V2D_CSI_KO_DECFRM_VP8_SCALE_H  \
	((uint64_t)1ull << V2DGEN_CSIFRM(FO, VP8, SCALE_H))  // |00|
#define V2D_CSI_KO_DECFRM_VP8_SCALE_V  \
	((uint64_t)1ull << V2DGEN_CSIFRM(FO, VP8, SCALE_V))  // |01|
#define V2D_CSI_KO_DECFRM_VP8_SCPIC_W  \
	((uint64_t)1ull << V2DGEN_CSIFRM(FO, VP8, SCPIC_W))  // |02|
#define V2D_CSI_KO_DECFRM_VP8_SCPIC_H  \
	((uint64_t)1ull << V2DGEN_CSIFRM(FO, VP8, SCPIC_H))  // |03|
#define V2D_CSI_KO_DECFRM_VP8_SHOWFRM  \
	((uint64_t)1ull << V2DGEN_CSIFRM(FO, VP8, SHOWFRM))  // |04|

/*
 * [HEVC|VP9] v2: map conpress info: |Hevc|Vp9|FrameCsiInfo_t
 */
#define V2D_CSI_FO_MC_MAP_Y_PA_H    (0u)  // v1: m_CompressedY[PA]
#define V2D_CSI_FO_MC_MAP_Y_PA_L    (1u)
#define V2D_CSI_FO_MC_MAP_Y_VA_H    (2u)  // v1: m_CompressedY[VA]
#define V2D_CSI_FO_MC_MAP_Y_VA_L    (3u)
#define V2D_CSI_FO_MC_MAP_C_PA_H    (4u)  // v1: m_CompressedCb[PA]
#define V2D_CSI_FO_MC_MAP_C_PA_L    (5u)
#define V2D_CSI_FO_MC_MAP_C_VA_H    (6u)  // v1: m_CompressedCb[VA]
#define V2D_CSI_FO_MC_MAP_C_VA_L    (7u)
#define V2D_CSI_FO_MC_FBC_Y_PA_H    (8u)  // v1: m_FbcYOffsetAddr[PA]
#define V2D_CSI_FO_MC_FBC_Y_PA_L    (9u)
#define V2D_CSI_FO_MC_FBC_Y_VA_H   (10u)  // v1: m_FbcYOffsetAddr[VA]
#define V2D_CSI_FO_MC_FBC_Y_VA_L   (11u)
#define V2D_CSI_FO_MC_FBC_C_PA_H   (12u)  // v1: m_FbcCOffsetAddr[PA]
#define V2D_CSI_FO_MC_FBC_C_PA_L   (13u)
#define V2D_CSI_FO_MC_FBC_C_VA_H   (14u)  // v1: m_FbcCOffsetAddr[VA]
#define V2D_CSI_FO_MC_FBC_C_VA_L   (15u)

#define V2D_CSI_FO_MC_LUMA_LEN     (16u)  // v1: m_uiCompressTableLumaSize
#define V2D_CSI_FO_MC_CRMA_LEN     (17u)  // v1: m_uiCompressTableChromaSize
#define V2D_CSI_FO_MC_LUMA_STR     (18u)  // v1: m_uiLumaStride
#define V2D_CSI_FO_MC_CRMA_STR     (19u)  // v1: m_uiChromaStride
#define V2D_CSI_FO_MC_LUMA_DEP     (20u)  // v1: m_uiLumaBitDepth
#define V2D_CSI_FO_MC_CRMA_DEP     (21u)  // v1: m_uiChromaBitDepth
#define V2D_CSI_FO_MC_ENDIAN       (22u)  // v1: m_uiFrameEndian
#define V2D_CSI_FO_MC_CODEC        (23u)  // v1: m_Reserved[0] (HEVC:15, VP9:16)
#define V2D_CSI_FO_MC_MAX          (24u)  // should not exceed 31

#define V2D_CSI_KO_MC_MAP_Y_PA_H  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, MAP_Y_PA_H)) // |00|
#define V2D_CSI_KO_MC_MAP_Y_PA_L  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, MAP_Y_PA_L)) // |01|
#define V2D_CSI_KO_MC_MAP_Y_VA_H  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, MAP_Y_VA_H)) // |02|
#define V2D_CSI_KO_MC_MAP_Y_VA_L  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, MAP_Y_VA_L)) // |03|

#define V2D_CSI_KO_MC_MAP_C_PA_H  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, MAP_C_PA_H)) // |04|
#define V2D_CSI_KO_MC_MAP_C_PA_L  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, MAP_C_PA_L)) // |05|
#define V2D_CSI_KO_MC_MAP_C_VA_H  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, MAP_C_VA_H)) // |06|
#define V2D_CSI_KO_MC_MAP_C_VA_L  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, MAP_C_VA_L)) // |07|

#define V2D_CSI_KO_MC_FBC_Y_PA_H  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, FBC_Y_PA_H)) // |08|
#define V2D_CSI_KO_MC_FBC_Y_PA_L  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, FBC_Y_PA_L)) // |09|
#define V2D_CSI_KO_MC_FBC_Y_VA_H  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, FBC_Y_VA_H)) // |10|
#define V2D_CSI_KO_MC_FBC_Y_VA_L  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, FBC_Y_VA_L)) // |11|

#define V2D_CSI_KO_MC_FBC_C_PA_H  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, FBC_C_PA_H)) // |12|
#define V2D_CSI_KO_MC_FBC_C_PA_L  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, FBC_C_PA_L)) // |13|
#define V2D_CSI_KO_MC_FBC_C_VA_H  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, FBC_C_VA_H)) // |14|
#define V2D_CSI_KO_MC_FBC_C_VA_L  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, FBC_C_VA_L)) // |15|

#define V2D_CSI_KO_MC_LUMA_LEN  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, LUMA_LEN))   // |16|
#define V2D_CSI_KO_MC_CRMA_LEN  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, CRMA_LEN))   // |17|
#define V2D_CSI_KO_MC_LUMA_STR  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, LUMA_STR))   // |18|
#define V2D_CSI_KO_MC_CRMA_STR  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, CRMA_STR))   // |19|
#define V2D_CSI_KO_MC_LUMA_DEP  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, LUMA_DEP))   // |20|
#define V2D_CSI_KO_MC_CRMA_DEP  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, CRMA_DEP))   // |21|
#define V2D_CSI_KO_MC_ENDIAN  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, ENDIAN))     // |22|
#define V2D_CSI_KO_MC_CODEC  \
	((uint64_t)1ull << V2DGEN_CSI(FO, MC, CODEC))      // |23|

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// IOCTL: CSI for |V_DEC_SEQ_HEADER|V_DEC_DECODE|
/*
 * [HEVC] v2: v2d_hevc_userdata_t for hevc|4kd2
 *       SEQ: HevcHeaderCsiInfo_t
 *       DEC: HevcFrameCsiInfo_t
 */
/* field: aspect ratio */
#define V2D_CSI_FO_UD_HEVC_AR_IDC    (0u) // v1: aspect_ratio_idc
#define V2D_CSI_FO_UD_HEVC_SAR_W     (1u) // v1: sar_width
#define V2D_CSI_FO_UD_HEVC_SAR_H     (2u) // v1: sar_height
/* field: color aspects */
#define V2D_CSI_FO_UD_HEVC_CA_FR     (3u) // v1: video_full_range_flag
#define V2D_CSI_FO_UD_HEVC_CA_PR     (4u) // v1: colour_primaries
#define V2D_CSI_FO_UD_HEVC_CA_TR     (5u) // v1: transfer_characteristics
#define V2D_CSI_FO_UD_HEVC_CA_MC     (6u) // v1: matrix_coefficients
/* field: mastering_display_colour_volume_t */
#define V2D_CSI_FO_UD_HEVC_DP_X0     (7u) // v1: display_primaries_x_0
#define V2D_CSI_FO_UD_HEVC_DP_X1     (8u) // v1: display_primaries_x_1
#define V2D_CSI_FO_UD_HEVC_DP_X2     (9u) // v1: display_primaries_x_2
#define V2D_CSI_FO_UD_HEVC_DP_Y0    (10u) // v1: display_primaries_y_0
#define V2D_CSI_FO_UD_HEVC_DP_Y1    (11u) // v1: display_primaries_y_1
#define V2D_CSI_FO_UD_HEVC_DP_Y2    (12u) // v1: display_primaries_y_2
#define V2D_CSI_FO_UD_HEVC_WHITE_X  (13u) // v1: white_point_x
#define V2D_CSI_FO_UD_HEVC_WHITE_Y  (14u) // v1: white_point_y
										  // v1: max_display_mastering_luminance
#define V2D_CSI_FO_UD_HEVC_LUM_MAX  (15u)
// v1: min_display_mastering_luminance
#define V2D_CSI_FO_UD_HEVC_LUM_MIN  (16u)
/* hevc_content_light_level_info_t */
#define V2D_CSI_FO_UD_HEVC_MCLL     (17u) // v1: max_content_light_level
#define V2D_CSI_FO_UD_HEVC_MPAL     (18u) // v1: max_pic_average_light_level
/* hevc_alternative_transfer_characteristics_info_t */
// v1: preferred_transfer_characteristics
#define V2D_CSI_FO_UD_HEVC_PRFR_TR  (19u)
#define V2D_CSI_FO_UD_HEVC_MAX      (20u) // should not exceed 31

/* key: aspect ratio */
#define V2D_CSI_KO_UD_HEVC_AR_IDC  \
	((uint64_t)1ull << V2DGEN_CSISEQ(FO, HEVC, AR_IDC))  // |00|
#define V2D_CSI_KO_UD_HEVC_SAR_W   \
	((uint64_t)1ull << V2DGEN_CSISEQ(FO, HEVC, SAR_W))   // |01|
#define V2D_CSI_KO_UD_HEVC_SAR_H   \
	((uint64_t)1ull << V2DGEN_CSISEQ(FO, HEVC, SAR_H))   // |02|
/* key: color aspects */
#define V2D_CSI_KO_UD_HEVC_CA_FR   \
	((uint64_t)1ull << V2DGEN_CSISEQ(FO, HEVC, CA_FR))   // |03|
#define V2D_CSI_KO_UD_HEVC_CA_PR   \
	((uint64_t)1ull << V2DGEN_CSISEQ(FO, HEVC, CA_PR))   // |04|
#define V2D_CSI_KO_UD_HEVC_CA_TR   \
	((uint64_t)1ull << V2DGEN_CSISEQ(FO, HEVC, CA_TR))   // |05| 16: HDR, 18:HLG
#define V2D_CSI_KO_UD_HEVC_CA_MC   \
	((uint64_t)1ull << V2DGEN_CSISEQ(FO, HEVC, CA_MC))   // |06|
/* key: mastering_display_colour_volume_t */
#define V2D_CSI_KO_UD_HEVC_DP_X0   \
	((uint64_t)1ull << V2DGEN_CSISEQ(FO, HEVC, DP_X0))   // |07|
#define V2D_CSI_KO_UD_HEVC_DP_X1   \
	((uint64_t)1ull << V2DGEN_CSISEQ(FO, HEVC, DP_X1))   // |08|
#define V2D_CSI_KO_UD_HEVC_DP_X2   \
	((uint64_t)1ull << V2DGEN_CSISEQ(FO, HEVC, DP_X2))   // |09|
#define V2D_CSI_KO_UD_HEVC_DP_Y0   \
	((uint64_t)1ull << V2DGEN_CSISEQ(FO, HEVC, DP_Y0))   // |10|
#define V2D_CSI_KO_UD_HEVC_DP_Y1   \
	((uint64_t)1ull << V2DGEN_CSISEQ(FO, HEVC, DP_Y1))   // |11|
#define V2D_CSI_KO_UD_HEVC_DP_Y2   \
	((uint64_t)1ull << V2DGEN_CSISEQ(FO, HEVC, DP_Y2))   // |12|
#define V2D_CSI_KO_UD_HEVC_WHITE_X   \
	((uint64_t)1ull << V2DGEN_CSISEQ(FO, HEVC, WHITE_X)) // |13|
#define V2D_CSI_KO_UD_HEVC_WHITE_Y   \
	((uint64_t)1ull << V2DGEN_CSISEQ(FO, HEVC, WHITE_Y)) // |14|
#define V2D_CSI_KO_UD_HEVC_LUMI_MAX   \
	((uint64_t)1ull << V2DGEN_CSISEQ(FO, HEVC, LUM_MAX)) // |15|
#define V2D_CSI_KO_UD_HEVC_LUMI_MIN   \
	((uint64_t)1ull << V2DGEN_CSISEQ(FO, HEVC, LUM_MIN)) // |16|
/* key: content_light_level_info_t */
#define V2D_CSI_KO_UD_HEVC_MCLL    \
	((uint64_t)1ull << V2DGEN_CSISEQ(FO, HEVC, MCLL))    // |17|
#define V2D_CSI_KO_UD_HEVC_MPAL    \
	((uint64_t)1ull << V2DGEN_CSISEQ(FO, HEVC, MPAL))    // |18|
/* key: alternative_transfer_characteristics_info_t */
#define V2D_CSI_KO_UD_HEVC_PRFR_TR \
	((uint64_t)1ull << V2DGEN_CSISEQ(FO, HEVC, PRFR_TR)) // |19| EOTF decision

#endif  // _VPU2_DIO_CSBLOB_H_
