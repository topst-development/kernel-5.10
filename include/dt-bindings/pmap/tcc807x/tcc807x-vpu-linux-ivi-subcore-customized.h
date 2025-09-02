/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#ifndef DT_PMAP_TCC807X_LINUX_SUBCORE_CUSTOMIZED_H
#define DT_PMAP_TCC807X_LINUX_SUBCORE_CUSTOMIZED_H

/*
 * Temporary chip definition before sending
 * VPU memory calculation code to dt-bindings
 * completely
 */
#define ARCH_TCC805X_LINUX

/*
 * vpu's configuration for TCC807x
 */
//#define CONFIG_SUPPORT_TCC_VPU
//#define CONFIG_SUPPORT_TCC_JPU
//#define CONFIG_SUPPORT_TCC_WAVE512_4K_D2
//#define CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC

//########################################################################
/**************** Customer edit from the following line. ****************/
//########################################################################

// Enable in case supporting the display-device
// or video-contents rotation (90/180/270)
// have to have rotation h/w block
//#define SUPPORT_ROTATION
#define SUPPORT_TCC_HEVC_4K             (0)

//***************************** VPU/HEVC ******************************/
//Should sync with hardware/telechips/omx/omx_videodec_interface/include/vdec.h
#if SUPPORT_TCC_HEVC_4K
#define SUPPORT_VIDEO_MAX_WIDTH         (4096)
#define SUPPORT_VIDEO_MAX_HEIGHT        (2176)
#else
#define SUPPORT_VIDEO_MAX_WIDTH         (1920)
#define SUPPORT_VIDEO_MAX_HEIGHT        (1088)
#endif

/*
   allows the VPU decoder to use user-registered contiguous physical memory as its framebuffer;
   in this case, the framebuffer size is excluded from the physical memory used by the VPU decoder.
   note: user framebuffers must be registered for all VPU decoders; individual control per decoder is not supported
*/
#define USE_EXTERNAL_FRAMEBUFFER_IN_DECODER		(0)

/*
 * DECODER on VPU
 *
 * INST_XXX_USE : Decide whether to use an instance or not. 1 means to use.
 * INST_XXX_IS_HEVC_TYPE : Decide whether or not to set the HEVC codec type.
 *                  1 means to set to HEVC
 * INST_XXX_VIDEO_WIDTH : Set the width of the video (HEVC: 32x alignment,
 *                  Other: 16x alignment)
 * INST_XXX_VIDEO_HEIGHT : Set the height of the video (HEVC: 32x alignment,
 *                  Other: 16x alignment)
 * INST_XXX_MAX_FRAMEBUFFER : Set the number of frame buffers (HEVC-4K: 7,
 *                  Others: 24)
 */
#define INST_1ST_USE                    (0)
#define INST_1ST_IS_HEVC_TYPE           (0)
#define INST_1ST_VIDEO_WIDTH            (SUPPORT_VIDEO_MAX_WIDTH)
#define INST_1ST_VIDEO_HEIGHT           (SUPPORT_VIDEO_MAX_HEIGHT)
#if SUPPORT_TCC_HEVC_4K
#define INST_1ST_MAX_FRAMEBUFFERS       (7)
#else
#define INST_1ST_MAX_FRAMEBUFFERS       (10)
#endif

#define INST_2ND_USE                    (0)
#define INST_2ND_IS_HEVC_TYPE           (0)
#define INST_2ND_VIDEO_WIDTH            (1920)
#define INST_2ND_VIDEO_HEIGHT           (1088)
#define INST_2ND_MAX_FRAMEBUFFERS       (24)

#define INST_3RD_USE                    (0)
#define INST_3RD_IS_HEVC_TYPE           (0)
#define INST_3RD_VIDEO_WIDTH            (1920)
#define INST_3RD_VIDEO_HEIGHT           (1088)
#define INST_3RD_MAX_FRAMEBUFFERS       (24)

#define INST_4TH_USE                    (0)
#define INST_4TH_IS_HEVC_TYPE           (0)
#define INST_4TH_VIDEO_WIDTH            (1920)
#define INST_4TH_VIDEO_HEIGHT           (1088)
#define INST_4TH_MAX_FRAMEBUFFERS       (24)

#define INST_5TH_USE                    (0)
#define INST_5TH_IS_HEVC_TYPE           (0)
#define INST_5TH_VIDEO_WIDTH            (1920)
#define INST_5TH_VIDEO_HEIGHT           (1088)
#define INST_5TH_MAX_FRAMEBUFFERS       (24)


/*
 * ENCODER on VPU
 *
 * INST_ENC_XXX_USE : Decide whether to use an instance or not. 1 means to use.
 * INST_ENC_XXX_IS_HEVC_TYPE : Decide whether or not to set the HEVC codec type.
 *                  1 means to set HEVC (please set the defconfig file to
 *                  CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC=y"
 *                  )
 * INST_ENC_XXX_VIDEO_WIDTH : Set the width of the video (16x alignment)
 * INST_ENC_XXX_VIDEO_HEIGHT : Set the height of the video (16x alignment)
 * INST_ENC_XXX_MAX_FRAMEBUFFERS : 8 :: fixed value. (Please, ask the right
 *                  engineerif you want to change it.)
 */
#define INST_ENC_1ST_USE                (0)
#define INST_ENC_1ST_IS_HEVC_TYPE       (0)
#define INST_ENC_1ST_VIDEO_WIDTH        (1920)
#define INST_ENC_1ST_VIDEO_HEIGHT       (1088)
#define INST_ENC_1ST_MAX_FRAMEBUFFERS   (8)

#define INST_ENC_2ND_USE                (0)
#define INST_ENC_2ND_IS_HEVC_TYPE       (0)
#define INST_ENC_2ND_VIDEO_WIDTH        (1920)
#define INST_ENC_2ND_VIDEO_HEIGHT       (1088)
#define INST_ENC_2ND_MAX_FRAMEBUFFERS   (8)

#define INST_ENC_3RD_USE                (0)
#define INST_ENC_3RD_IS_HEVC_TYPE       (0)
#define INST_ENC_3RD_VIDEO_WIDTH        (1920)
#define INST_ENC_3RD_VIDEO_HEIGHT       (1088)
#define INST_ENC_3RD_MAX_FRAMEBUFFERS   (8)

#define INST_ENC_4TH_USE                (0)
#define INST_ENC_4TH_IS_HEVC_TYPE       (0)
#define INST_ENC_4TH_VIDEO_WIDTH        (1920)
#define INST_ENC_4TH_VIDEO_HEIGHT       (1088)
#define INST_ENC_4TH_MAX_FRAMEBUFFERS   (8)

#define INST_ENC_5TH_USE                (0)
#define INST_ENC_5TH_IS_HEVC_TYPE       (0)
#define INST_ENC_5TH_VIDEO_WIDTH        (1920)
#define INST_ENC_5TH_VIDEO_HEIGHT       (1088)
#define INST_ENC_5TH_MAX_FRAMEBUFFERS   (8)

#define INST_ENC_6TH_USE                (0)
#define INST_ENC_6TH_IS_HEVC_TYPE       (0)
#define INST_ENC_6TH_VIDEO_WIDTH        (1920)
#define INST_ENC_6TH_VIDEO_HEIGHT       (1088)
#define INST_ENC_6TH_MAX_FRAMEBUFFERS   (8)

#define INST_ENC_7TH_USE                (0)
#define INST_ENC_7TH_IS_HEVC_TYPE       (0)
#define INST_ENC_7TH_VIDEO_WIDTH        (1920)
#define INST_ENC_7TH_VIDEO_HEIGHT       (1088)
#define INST_ENC_7TH_MAX_FRAMEBUFFERS   (8)

#define INST_ENC_8TH_USE                (0)
#define INST_ENC_8TH_IS_HEVC_TYPE       (0)
#define INST_ENC_8TH_VIDEO_WIDTH        (1920)
#define INST_ENC_8TH_VIDEO_HEIGHT       (1088)
#define INST_ENC_8TH_MAX_FRAMEBUFFERS   (8)

#define INST_ENC_9TH_USE                (0)
#define INST_ENC_9TH_IS_HEVC_TYPE       (0)
#define INST_ENC_9TH_VIDEO_WIDTH        (1920)
#define INST_ENC_9TH_VIDEO_HEIGHT       (1088)
#define INST_ENC_9TH_MAX_FRAMEBUFFERS   (8)

#define INST_ENC_10TH_USE               (0)
#define INST_ENC_10TH_IS_HEVC_TYPE      (0)
#define INST_ENC_10TH_VIDEO_WIDTH       (1920)
#define INST_ENC_10TH_VIDEO_HEIGHT      (1088)
#define INST_ENC_10TH_MAX_FRAMEBUFFERS  (8)

#define INST_ENC_11TH_USE               (0)
#define INST_ENC_11TH_IS_HEVC_TYPE      (0)
#define INST_ENC_11TH_VIDEO_WIDTH       (1920)
#define INST_ENC_11TH_VIDEO_HEIGHT      (1088)
#define INST_ENC_11TH_MAX_FRAMEBUFFERS  (8)

#define INST_ENC_12TH_USE               (0)
#define INST_ENC_12TH_IS_HEVC_TYPE      (0)
#define INST_ENC_12TH_VIDEO_WIDTH       (1920)
#define INST_ENC_12TH_VIDEO_HEIGHT      (1088)
#define INST_ENC_12TH_MAX_FRAMEBUFFERS  (8)

#define INST_ENC_13TH_USE               (0)
#define INST_ENC_13TH_IS_HEVC_TYPE      (0)
#define INST_ENC_13TH_VIDEO_WIDTH       (1920)
#define INST_ENC_13TH_VIDEO_HEIGHT      (1088)
#define INST_ENC_13TH_MAX_FRAMEBUFFERS  (8)

#define INST_ENC_14TH_USE               (0)
#define INST_ENC_14TH_IS_HEVC_TYPE      (0)
#define INST_ENC_14TH_VIDEO_WIDTH       (1920)
#define INST_ENC_14TH_VIDEO_HEIGHT      (1088)
#define INST_ENC_14TH_MAX_FRAMEBUFFERS  (8)

#define INST_ENC_15TH_USE               (0)
#define INST_ENC_15TH_IS_HEVC_TYPE      (0)
#define INST_ENC_15TH_VIDEO_WIDTH       (1920)
#define INST_ENC_15TH_VIDEO_HEIGHT      (1088)
#define INST_ENC_15TH_MAX_FRAMEBUFFERS  (8)

#define INST_ENC_16TH_USE               (0)
#define INST_ENC_16TH_IS_HEVC_TYPE      (0)
#define INST_ENC_16TH_VIDEO_WIDTH       (1920)
#define INST_ENC_16TH_VIDEO_HEIGHT      (1088)
#define INST_ENC_16TH_MAX_FRAMEBUFFERS  (8)

//########################################################################
/****************           Customer edit - end!          ****************/
//########################################################################

#if INST_5TH_USE
#define VPU_INST_MAX 5
#define CONFIG_VDEC_CNT_5
#elif INST_4TH_USE
#define VPU_INST_MAX 4
#define CONFIG_VDEC_CNT_4
#elif INST_3RD_USE
#define VPU_INST_MAX 3
#define CONFIG_VDEC_CNT_3
#elif INST_2ND_USE
#define VPU_INST_MAX 2
#define CONFIG_VDEC_CNT_2
#elif INST_1ST_USE
#define VPU_INST_MAX 1
#define CONFIG_VDEC_CNT_1
#else
#define VPU_INST_MAX 0
#endif

#if INST_ENC_16TH_USE
#define VPU_ENC_MAX_CNT 16
#define CONFIG_VENC_CNT_16
#elif INST_ENC_15TH_USE
#define VPU_ENC_MAX_CNT 15
#define CONFIG_VENC_CNT_15
#elif INST_ENC_14TH_USE
#define VPU_ENC_MAX_CNT 14
#define CONFIG_VENC_CNT_14
#elif INST_ENC_13TH_USE
#define VPU_ENC_MAX_CNT 13
#define CONFIG_VENC_CNT_13
#elif INST_ENC_12TH_USE
#define VPU_ENC_MAX_CNT 12
#define CONFIG_VENC_CNT_12
#elif INST_ENC_11TH_USE
#define VPU_ENC_MAX_CNT 11
#define CONFIG_VENC_CNT_11
#elif INST_ENC_10TH_USE
#define VPU_ENC_MAX_CNT 10
#define CONFIG_VENC_CNT_10
#elif INST_ENC_9TH_USE
#define VPU_ENC_MAX_CNT 9
#define CONFIG_VENC_CNT_9
#elif INST_ENC_8TH_USE
#define VPU_ENC_MAX_CNT 8
#define CONFIG_VENC_CNT_8
#elif INST_ENC_7TH_USE
#define VPU_ENC_MAX_CNT 7
#define CONFIG_VENC_CNT_7
#elif INST_ENC_6TH_USE
#define VPU_ENC_MAX_CNT 6
#define CONFIG_VENC_CNT_6
#elif INST_ENC_5TH_USE
#define VPU_ENC_MAX_CNT 5
#define CONFIG_VENC_CNT_5
#elif INST_ENC_4TH_USE
#define VPU_ENC_MAX_CNT 4
#define CONFIG_VENC_CNT_4
#elif INST_ENC_3RD_USE
#define VPU_ENC_MAX_CNT 3
#define CONFIG_VENC_CNT_3
#elif INST_ENC_2ND_USE
#define VPU_ENC_MAX_CNT 2
#define CONFIG_VENC_CNT_2
#elif INST_ENC_1ST_USE
#define VPU_ENC_MAX_CNT 1
#define CONFIG_VENC_CNT_1
#else
#define VPU_ENC_MAX_CNT 0
#endif


#endif //DT_PMAP_TCC807X_LINUX_SUBCORE_CUSTOMIZED_H
