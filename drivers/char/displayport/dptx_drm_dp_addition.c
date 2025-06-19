// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright © 2014 Red Hat
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

/*
 * Modified by Telechips Inc.
 */

#include <linux/types.h>
#include <dptx_drm_dp_addition.h>
#include <dptx_v14.h>
#include <dptx_reg.h>
#include <dptx_dbg.h>

/* From DRM dp_link_status */
static uint8_t drm_addition_get_link_status(const u8 link_status[DP_LINK_STATUS_SIZE],
					    uint32_t dpcd_addr)
{
	uint32_t link_index;
	uint8_t status = 0u;

	if (dpcd_addr >= (uint32_t)DP_LANE0_1_STATUS) {
		link_index = dpcd_addr - (uint32_t)DP_LANE0_1_STATUS;
		if (link_index < (uint32_t)(DP_LINK_STATUS_SIZE - 1)) {
			/* For KCS */
			status = link_status[dpcd_addr - DP_LANE0_1_STATUS];
		}
	}
	return status;
}

/* From DRM dp_get_lane_status */
u8 drm_addition_get_lane_status(const u8 link_status[DP_LINK_STATUS_SIZE], int lane_index)
{
	uint32_t lane_offset, dpcd_address = DP_LANE0_1_STATUS;
	uint8_t dpce_lane_status;
	u8 lane_status = 0u;

	if (lane_index >= 0) {
		dpcd_address += ((uint32_t)lane_index >> 1);
		lane_offset = ((uint32_t)lane_index & 1u) << 2;

		dpce_lane_status = drm_addition_get_link_status(link_status, dpcd_address);
		lane_status =  ((uint32_t)dpce_lane_status >> lane_offset) & 0xFu;
	}
	return lane_status;
}

/* From DRM drm_dp_channel_eq_ok */
bool drm_addition_channel_eq_ok(const u8 link_status[DP_LINK_STATUS_SIZE],
				int lane_count)
{
	uint8_t lane_align, lane_status;
	bool status = (bool)true;
	int lane;

	lane_align = drm_addition_get_link_status(link_status,
				    DP_LANE_ALIGN_STATUS_UPDATED);
	if ((lane_align & DP_INTERLANE_ALIGN_DONE) == 0) {
		/* For KCS */
		status = (bool)false;
	}
	if (status) {
		for (lane = 0; lane < lane_count; lane++) {
			lane_status = drm_addition_get_lane_status(link_status, lane);
			if ((lane_status & DP_CHANNEL_EQ_BITS) != DP_CHANNEL_EQ_BITS) {
				status = (bool)false;
				break;
			}
		}
	}
	return status;
}

/* From DRM drm_dp_clock_recovery_ok */
bool drm_addition_clock_recovery_ok(const u8 link_status[DP_LINK_STATUS_SIZE],
				    int lane_count)
{
	bool status = (bool)true;
	u8 lane_status;
	int lane;

	for (lane = 0; lane < lane_count; lane++) {
		lane_status = drm_addition_get_lane_status(link_status, lane);
		if ((lane_status & DP_LANE_CR_DONE) == 0) {
			status = (bool)false;
			break;
		}
	}
	return status;
}

/* From DRM drm_dp_msg_data_crc4 */
static u8 drm_addition_get_msg_data_crc4(const uint8_t *data, u8 number_of_bytes)
{
	u8 bitmask = 0x80;
	u8 bitshift = 7;
	u8 array_index = 0;
	int number_of_bits = number_of_bytes * 8;
	u16 remainder = 0;

	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		remainder |= (data[array_index] & bitmask) >> bitshift;
		bitmask >>= 1;
		bitshift--;
		if (bitmask == 0) {
			bitmask = 0x80;
			bitshift = 7;
			array_index++;
		}
		if ((remainder & 0x100) == 0x100)
			remainder ^= 0xd5;
	}

	number_of_bits = 8;
	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		if ((remainder & 0x100) != 0)
			remainder ^= 0xd5;
	}

	return remainder & 0xff;
}

/* From DRM drm_dp_calc_pbn_mode */
int Drm_Addition_Calculate_PBN_mode(int clock, int bpp)
{
	/*
	 * margin 5300ppm + 300ppm ~ 0.6% as per spec, factor is 1.006
	 * The unit of 54/64Mbytes/sec is an arbitrary unit chosen based on
	 * common multiplier to render an integer PBN for all link rate/lane
	 * counts combinations
	 * calculate
	 * peak_kbps *= (1006/1000)
	 * peak_kbps *= (64/54)
	 * peak_kbps *= 8    convert to bytes
	 *
	 * If the bpp is in units of 1/16, further divide by 16. Put this
	 * factor in the numerator rather than the denominator to avoid
	 * integer overflow
	 */

	fixed20_12 pix_bw;
	fixed20_12 fbpp;
	fixed20_12 result;
	fixed20_12 margin, tmp;
	u32 res;

	pix_bw.full = dfixed_const(clock);
	fbpp.full = dfixed_const(bpp);
	tmp.full = dfixed_const(8);
	fbpp.full = dfixed_div(fbpp, tmp);

	result.full = dfixed_mul(pix_bw, fbpp);
	margin.full = dfixed_const(54);
	tmp.full = dfixed_const(64);
	margin.full = dfixed_div(margin, tmp);
	result.full = dfixed_div(result, margin);

	margin.full = dfixed_const(1006);
	tmp.full = dfixed_const(1000);
	margin.full = dfixed_div(margin, tmp);
	result.full = dfixed_mul(result, margin);

	result.full = dfixed_div(result, tmp);
	result.full = dfixed_ceil(result);

	res = dfixed_trunc(result);

	return res;
}
EXPORT_SYMBOL(Drm_Addition_Calculate_PBN_mode);

/* From DRM drm_dp_sideband_parse_link_address */
int32_t Drm_Addition_Parse_Sideband_Link_Address(struct drm_dp_sideband_msg_rx *raw,
						 struct drm_dp_sideband_msg_reply_body *repmsg)
{
	int idx = 1;
	int i;

	memcpy(repmsg->u.link_addr.guid, &raw->msg[idx], 16);
	idx += 16;
	repmsg->u.link_addr.nports = raw->msg[idx] & 0xf;
	idx++;
	if (idx > raw->curlen)
		goto fail_len;
	for (i = 0; i < repmsg->u.link_addr.nports; i++) {
		if (raw->msg[idx] & 0x80)
			repmsg->u.link_addr.ports[i].input_port = 1;

		repmsg->u.link_addr.ports[i].peer_device_type = (raw->msg[idx] >> 4) & 0x7;
		repmsg->u.link_addr.ports[i].port_number = (raw->msg[idx] & 0xf);

		idx++;
		if (idx > raw->curlen)
			goto fail_len;
		repmsg->u.link_addr.ports[i].mcs = (raw->msg[idx] >> 7) & 0x1;
		repmsg->u.link_addr.ports[i].ddps = (raw->msg[idx] >> 6) & 0x1;
		if (repmsg->u.link_addr.ports[i].input_port == 0)
			repmsg->u.link_addr.ports[i].legacy_device_plug_status = (raw->msg[idx] >> 5) & 0x1;
		idx++;
		if (idx > raw->curlen)
			goto fail_len;
		if (repmsg->u.link_addr.ports[i].input_port == 0) {
			repmsg->u.link_addr.ports[i].dpcd_revision = (raw->msg[idx]);
			idx++;
			if (idx > raw->curlen)
				goto fail_len;
			memcpy(repmsg->u.link_addr.ports[i].peer_guid, &raw->msg[idx], 16);
			idx += 16;
			if (idx > raw->curlen)
				goto fail_len;
			repmsg->u.link_addr.ports[i].num_sdp_streams = (raw->msg[idx] >> 4) & 0xf;
			repmsg->u.link_addr.ports[i].num_sdp_stream_sinks = (raw->msg[idx] & 0xf);
			idx++;

		}
		if (idx > raw->curlen)
			goto fail_len;
	}

	return DPTX_RETURN_NO_ERROR;
fail_len:
	dptx_err("link address reply parse length fail %d %d\n", idx, raw->curlen);

	return DPTX_RETURN_ENOENT;
}

/* From DRM drm_dp_msg_header_crc4 */
static u8 drm_addition_get_msg_header_crc4(const uint8_t *data, size_t num_nibbles)
{
	u8 bitmask = 0x80;
	u8 bitshift = 7;
	u8 array_index = 0;
	int number_of_bits = num_nibbles * 4;
	u8 remainder = 0;

	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		remainder |= (data[array_index] & bitmask) >> bitshift;
		bitmask >>= 1;
		bitshift--;
		if (bitmask == 0) {
			bitmask = 0x80;
			bitshift = 7;
			array_index++;
		}
		if ((remainder & 0x10) == 0x10)
			remainder ^= 0x13;
	}

	number_of_bits = 4;
	while (number_of_bits != 0) {
		number_of_bits--;
		remainder <<= 1;
		if ((remainder & 0x10) != 0)
			remainder ^= 0x13;
	}

	return remainder;
}

void Drm_Addition_Encode_SideBand_Msg_CRC(u8 *msg, u8 len)
{
	u8		crc4;

	crc4 = drm_addition_get_msg_data_crc4(msg, len);
	msg[len] = crc4;
}

void Drm_Addition_Encode_Sideband_Msg_Hdr(struct drm_dp_sideband_msg_hdr *hdr, u_int8_t *buf, int *len)
{
	int idx = 0;
	int i;
	u8 crc4;

	buf[idx++] = ((hdr->lct & 0xf) << 4) | (hdr->lcr & 0xf);
	for (i = 0; i < (hdr->lct / 2); i++)
		buf[idx++] = hdr->rad[i];
	buf[idx++] = (hdr->broadcast << 7) | (hdr->path_msg << 6) |
		(hdr->msg_len & 0x3f);
	buf[idx++] = (hdr->somt << 7) | (hdr->eomt << 6) | (hdr->seqno << 4);

	crc4 = drm_addition_get_msg_header_crc4(buf, (idx * 2) - 1);
	buf[idx - 1] |= (crc4 & 0xf);
	*len = idx;
}

int32_t Drm_Addition_Decode_Sideband_Msg_Hdr(struct drm_dp_sideband_msg_hdr *hdr, u_int8_t *buf, int buflen, u_int8_t *hdrlen)
{
	u8 crc4;
	u8 len;
	int i;
	u8 idx;

	if (buf[0] == 0)
		return DPTX_RETURN_ENOENT;
	len = 3;
	len += ((buf[0] & 0xf0) >> 4) / 2;
	if (len > buflen)
		return DPTX_RETURN_ENOENT;
	crc4 = drm_addition_get_msg_header_crc4(buf, (len * 2) - 1);

	if ((crc4 & 0xf) != (buf[len - 1] & 0xf)) {
		dptx_err("crc4 mismatch 0x%x 0x%x", crc4, buf[len - 1] & 0xfu);
		return DPTX_RETURN_ENOENT;
	}

	hdr->lct = (buf[0] & 0xf0) >> 4;
	hdr->lcr = (buf[0] & 0xf);
	idx = 1;
	for (i = 0; i < (hdr->lct / 2); i++)
		hdr->rad[i] = buf[idx++];
	hdr->broadcast = (buf[idx] >> 7) & 0x1;
	hdr->path_msg = (buf[idx] >> 6) & 0x1;
	hdr->msg_len = buf[idx] & 0x3f;
	idx++;
	hdr->somt = (buf[idx] >> 7) & 0x1;
	hdr->eomt = (buf[idx] >> 6) & 0x1;
	hdr->seqno = (buf[idx] >> 4) & 0x1;
	idx++;
	*hdrlen = idx;
	return DPTX_RETURN_NO_ERROR;
}

void Drm_Addition_Parse_Sideband_Connection_Status_Notify(struct drm_dp_sideband_msg_rx *raw, struct drm_dp_sideband_msg_req_body *msg)
{
	int idx = 1;

	msg->u.conn_stat.port_number = (raw->msg[idx] & 0xf0) >> 4;
	idx++;

	memcpy(msg->u.conn_stat.guid, &raw->msg[idx], 16);
	idx += 16;

	msg->u.conn_stat.legacy_device_plug_status = (raw->msg[idx] >> 6) & 0x1;
	msg->u.conn_stat.displayport_device_plug_status = (raw->msg[idx] >> 5) & 0x1;
	msg->u.conn_stat.message_capability_status = (raw->msg[idx] >> 4) & 0x1;
	msg->u.conn_stat.input_port = (raw->msg[idx] >> 3) & 0x1;
	msg->u.conn_stat.peer_device_type = (raw->msg[idx] & 0x7);
	idx++;
}

bool Drm_dp_tps3_supported(const u_int8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
	return (dpcd[DP_DPCD_REV] >= 0x12) && (dpcd[DP_MAX_LANE_COUNT] & DP_TPS3_SUPPORTED);
}

bool Drm_dp_tps4_supported(const u_int8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
	return (dpcd[DP_DPCD_REV] >= 0x14) && (dpcd[DP_MAX_DOWNSPREAD] & DP_TPS4_SUPPORTED);
}

u8	 Drm_dp_max_lane_count(const u_int8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
	return (dpcd[DP_MAX_LANE_COUNT] & DP_MAX_LANE_COUNT_MASK);
}

bool Drm_dp_enhanced_frame_cap(const u_int8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
	return (dpcd[DP_DPCD_REV] >= 0x11) && (dpcd[DP_MAX_LANE_COUNT] & DP_ENHANCED_FRAME_CAP);
}

/* From DRM drm_dp_link_train_clock_recovery_delay */
void drm_addition_link_train_clock_recovery_delay(const u_int8_t dpcd[DP_RECEIVER_CAP_SIZE])
{
	unsigned long rd_interval = dpcd[DP_TRAINING_AUX_RD_INTERVAL] &
				    DP_TRAINING_AUX_RD_MASK;

	if (rd_interval > 4u) {
		pr_debug("AUX interval %lu, out of range (max 4)\n", rd_interval);
		rd_interval = 4u;
	}

	if ((rd_interval == 0u) || (dpcd[DP_DPCD_REV] >= 0x14u)) {
		/* For KCS */
		rd_interval = 100u;
	} else {
		rd_interval *= 4000u;
	}
	udelay(rd_interval);
}


/* From DRM drm_dp_link_train_channel_eq_delay */
void drm_addition_link_train_channel_eq_delay(const u8 dpcd[DP_RECEIVER_CAP_SIZE])
{
	unsigned long rd_interval = dpcd[DP_TRAINING_AUX_RD_INTERVAL] &
					 DP_TRAINING_AUX_RD_MASK;

	if (rd_interval > 4u) {
		pr_debug("AUX interval %lu, out of range (max 4)\n", rd_interval);
		rd_interval = 4u;
	}

	if (rd_interval == 0u) {
		/* For KCS */
		rd_interval = 400u;
	} else {
		rd_interval *= 4000u;
	}
	udelay(rd_interval);
}


/**
 * drm_dp_read_mst_cap() - check whether or not a sink supports MST
 * @aux: The DP AUX channel to use
 * @dpcd: A cached copy of the DPCD capabilities for this sink
 *
 * Returns: %True if the sink supports MST, %false otherwise
 */
bool drm_addition_read_mst_cap(struct Dptx_Params *pstDptx)
{
	bool mst_support = (bool)true;
	u8 mstm_cap = 0;

	if (pstDptx->aucDPCD_Caps[DP_DPCD_REV] < DP_DPCD_REV_12) {
		/* For KCS */
		mst_support = (bool)false;
	}
	if (mst_support) {
		if (Dptx_Aux_Read_DPCD(pstDptx, DP_MSTM_CAP, &mstm_cap) !=
		    DPTX_RETURN_NO_ERROR) {
			mst_support = (bool)false;
		}
	}
	if (mst_support) {
		if ((mstm_cap & DP_MST_CAP) == 0u) {
			/* For KCS */
			mst_support = (bool)false;
		}
	}
	return mst_support;
}

/* From DRM drm_dp_read_dpcd_caps */
int drm_addition_read_dpcd_caps(struct Dptx_Params *pstDptx)
{
	int ret;

	ret = Dptx_Aux_Read_Bytes_From_DPCD(pstDptx, DP_DPCD_REV,
					    pstDptx->aucDPCD_Caps,
					    DP_RECEIVER_CAP_SIZE);
	if (DPTX_RETURN_SUCCESS(ret)) {
		if (pstDptx->aucDPCD_Caps[DP_DPCD_REV] == 0u) {
			dptx_err("DPCD_REV is zero");
			ret = -EIO;
		}

	}
	if (DPTX_RETURN_SUCCESS(ret)) {
		/* For KCS */
		ret = drm_addition_read_extended_dpcd_caps(pstDptx);
	}
	return ret;
}

/* From DRM drm_dp_read_extended_dpcd_caps */
int drm_addition_read_extended_dpcd_caps(struct Dptx_Params *pstDptx)
{
	uint8_t dpcd_ext[DP_RECEIVER_CAP_SIZE];
	int ret = DPTX_RETURN_NO_ERROR;

	/*
	 * Prior to DP1.3 the bit represented by
	 * DP_EXTENDED_RECEIVER_CAP_FIELD_PRESENT was reserved.
	 * If it is set DP_DPCD_REV at 0000h could be at a value less than
	 * the true capability of the panel. The only way to check is to
	 * then compare 0000h and 2200h.
	 */
	if ((pstDptx->aucDPCD_Caps[DP_TRAINING_AUX_RD_INTERVAL] &
		DP_EXTENDED_RECEIVER_CAP_FIELD_PRESENT) != 0u) {
		ret = Dptx_Aux_Read_Bytes_From_DPCD(pstDptx, DP_DP13_DPCD_REV, dpcd_ext, sizeof(dpcd_ext));
		if (DPTX_RETURN_SUCCESS(ret)) {
			if (pstDptx->aucDPCD_Caps[DP_DPCD_REV] > dpcd_ext[DP_DPCD_REV]) {
				dptx_dbg("Extended DPCD rev less than base DPCD rev (%d > %d)",
					  pstDptx->aucDPCD_Caps[DP_DPCD_REV], dpcd_ext[DP_DPCD_REV]);
			} else {
				(void)memcpy(pstDptx->aucDPCD_Caps,
					     dpcd_ext, sizeof(dpcd_ext));
			}
		}
	}

	return ret;
}

