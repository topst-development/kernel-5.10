/*
 * Copyright (c) 2016 Synopsys, Inc.
 *
 * Synopsys DP TX Linux Software Driver and documentation (hereinafter,
 * "Software") is an Unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto. You are permitted to use and
 * redistribute this Software in source and binary forms, with or without
 * modification, provided that redistributions of source code must retain this
 * notice. You may not view, use, disclose, copy or distribute this file or
 * any information contained herein except pursuant to this license grant from
 * Synopsys. If you do not agree with this notice, including the disclaimer
 * below, then you are not authorized to use the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
* Modified by Telechips Inc.
*/


#ifndef DRM_DP_ADDITIONS_H
#define DRM_DP_ADDITIONS_H

#include <asm/div64.h>
#include <linux/kernel.h>
#include <linux/bits.h>
#include <linux/math64.h>
#include <drm/drm_fixed.h>
#include <drm/drm_dp_helper.h>
#include <drm/drm_dp_mst_helper.h>

struct Dptx_Params;

uint8_t drm_addition_get_lane_status(const u8 link_status[DP_LINK_STATUS_SIZE], int iLane_Index);
bool drm_addition_clock_recovery_ok(const u8 link_status[DP_LINK_STATUS_SIZE], int lane_count);
bool drm_addition_channel_eq_ok(const u8 link_status[DP_LINK_STATUS_SIZE], int lane_count);

int  Drm_Addition_Calculate_PBN_mode(int clock, int bpp);
int32_t Drm_Addition_Parse_Sideband_Link_Address(struct drm_dp_sideband_msg_rx *raw, struct drm_dp_sideband_msg_reply_body *repmsg);
void Drm_Addition_Encode_Sideband_Msg_Hdr( struct drm_dp_sideband_msg_hdr *hdr, uint8_t *buf, int *len );
int32_t Drm_Addition_Decode_Sideband_Msg_Hdr( struct drm_dp_sideband_msg_hdr *hdr, uint8_t *buf, int buflen, uint8_t *hdrlen );
void Drm_Addition_Encode_SideBand_Msg_CRC(uint8_t *msg, uint8_t len);
void Drm_Addition_Parse_Sideband_Connection_Status_Notify(struct drm_dp_sideband_msg_rx *raw, struct drm_dp_sideband_msg_req_body *msg);

bool Drm_dp_tps3_supported(const uint8_t dpcd[DP_RECEIVER_CAP_SIZE]);
bool Drm_dp_tps4_supported(const uint8_t dpcd[DP_RECEIVER_CAP_SIZE]);
u8 Drm_dp_max_lane_count(const uint8_t dpcd[DP_RECEIVER_CAP_SIZE]);
bool Drm_dp_enhanced_frame_cap(const uint8_t dpcd[DP_RECEIVER_CAP_SIZE]);

void drm_addition_link_train_clock_recovery_delay(const uint8_t dpcd[DP_RECEIVER_CAP_SIZE]);
void drm_addition_link_train_channel_eq_delay(const u8 dpcd[DP_RECEIVER_CAP_SIZE]);

bool drm_addition_read_mst_cap(struct Dptx_Params *pstDptx);
int drm_addition_read_dpcd_caps(struct Dptx_Params *pstDptx);
int drm_addition_read_extended_dpcd_caps(struct Dptx_Params *pstDptx);
#endif
