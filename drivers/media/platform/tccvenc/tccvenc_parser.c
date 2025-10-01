// SPDX-License-Identifier: MIT
#include <linux/slab.h>
#include <linux/string.h>
#include "tccvenc_parser.h"

#define NAL_SPS 7

/* ------------ Bitreader / Bitwriter (C90) ------------ */
struct bitreader {
	const u8 *buf;
	int size_bytes;
	int bitpos; /* bits */
};

static void br_init(struct bitreader *br, const u8 *buf, int size_bytes)
{
	br->buf = buf;
	br->size_bytes = size_bytes;
	br->bitpos = 0;
}

static int br_bits_left(const struct bitreader *br)
{
	return br->size_bytes * 8 - br->bitpos;
}

static u32 br_read_bit(struct bitreader *br)
{
	u32 byte, shift, v;
	if (br_bits_left(br) <= 0) return 0;
	byte  = br->buf[br->bitpos >> 3];
	shift = 7 - (br->bitpos & 7);
	v = (byte >> shift) & 1u;
	br->bitpos++;
	return v;
}

static u32 br_read_bits(struct bitreader *br, int n)
{
	u32 v;
	int k;
	v = 0;
	for (k = 0; k < n; ++k) v = (v << 1) | br_read_bit(br);
	return v;
}

/* ue(v) / se(v) */
static u32 br_read_ue(struct bitreader *br)
{
	int zeros;
	u32 info;
	u32 val;
	zeros = 0;
	info = 0;
	while (br_bits_left(br) > 0 && br_read_bit(br) == 0) zeros++;
	if (zeros > 31) return 0;
	if (zeros > 0) info = br_read_bits(br, zeros);
	val = ((1u << zeros) - 1u) + info;
	return val;
}

static int br_read_se(struct bitreader *br)
{
	u32 ue;
	ue = br_read_ue(br);
	return (ue & 1u) ? (int)((ue + 1u) >> 1) : -(int)(ue >> 1);
}

struct bitwriter {
	u8 *buf;
	int size_bytes;
	int bitpos;
};

static void bw_init(struct bitwriter *bw, u8 *buf, int size_bytes)
{
	bw->buf = buf;
	bw->size_bytes = size_bytes;
	bw->bitpos = 0;
	memset(buf, 0, size_bytes);
}

static int bw_bits_left(struct bitwriter *bw)
{
	return bw->size_bytes * 8 - bw->bitpos;
}

static void bw_write_bit(struct bitwriter *bw, u32 b)
{
	int byte_idx, shift;
	if (bw_bits_left(bw) <= 0) return;
	byte_idx = bw->bitpos >> 3;
	shift    = 7 - (bw->bitpos & 7);
	bw->buf[byte_idx] |= (b & 1u) << shift;
	bw->bitpos++;
}

static void bw_copy_bits(struct bitwriter *bw, const u8 *src, int bitcnt)
{
	struct bitreader br;
	int i;
	br_init(&br, src, (bitcnt + 7) >> 3);
	for (i = 0; i < bitcnt; ++i) bw_write_bit(bw, br_read_bit(&br));
}

/* ------------ EBSP <-> RBSP ------------ */
static int ebsp_to_rbsp(const u8 *ebsp, int ebsp_len, u8 *rbsp, int rbsp_max)
{
	int zero_cnt, i, w;
	zero_cnt = 0;
	w = 0;
	for (i = 0; i < ebsp_len; ++i) {
		u8 b;
		b = ebsp[i];
		if (zero_cnt == 2 && b == 0x03) { zero_cnt = 0; continue; }
		if (w < rbsp_max) rbsp[w++] = b;
		zero_cnt = (b == 0) ? (zero_cnt + 1) : 0;
	}
	return w;
}

static int rbsp_to_ebsp(const u8 *rbsp, int rbsp_len, u8 *ebsp, int ebsp_max)
{
	int zero_cnt, i, w;
	zero_cnt = 0;
	w = 0;
	for (i = 0; i < rbsp_len; ++i) {
		u8 b;
		b = rbsp[i];
		if (zero_cnt == 2 && b <= 0x03) {
			if (w < ebsp_max) ebsp[w++] = 0x03;
			zero_cnt = 0;
		}
		if (w < ebsp_max) ebsp[w++] = b;
		zero_cnt = (b == 0) ? (zero_cnt + 1) : 0;
	}
	return w;
}

/* ------------ Annex-B start code scan ------------ */
static int is_sc3(const u8 *p)
{
	return p[0]==0x00 && p[1]==0x00 && p[2]==0x01;
}
static int is_sc4(const u8 *p)
{
	return p[0]==0x00 && p[1]==0x00 && p[2]==0x00 && p[3]==0x01;
}

static int find_start_code(const u8 *buf, int len, int *sc_size)
{
	int i;
	for (i = 0; i + 3 < len; ++i) {
		if (is_sc4(buf + i)) { *sc_size = 4; return i; }
		if (is_sc3(buf + i)) { *sc_size = 3; return i; }
	}
	return -1;
}

/* ------------ SPS parse (to VUI flag) ------------ */
static int profile_is_frext(int profile_idc)
{
	switch (profile_idc) {
	case 100: case 110: case 122: case 244:
	case 44:  case 83:  case 86:  case 118:
	case 128: case 138: case 139: case 134:
		return 1;
	default:
		return 0;
	}
}

/* 반환: RBSP 기준 vui_parameters_present_flag의 bit offset, 실패시 <0 */
static int sps_locate_vui_flag_bitpos(const u8 *rbsp, int rbsp_len)
{
	struct bitreader br;
	int profile_idc;
	u32 chroma_format_idc;
	u32 poc_type, n;
	u32 frame_mbs_only_flag;
	int vui_bitpos;
	int i, num_lists, size, j;
	int last, next;

	chroma_format_idc = 1;

	br_init(&br, rbsp, rbsp_len);
	if (br_bits_left(&br) < 24) return -1;

	profile_idc = (int)br_read_bits(&br, 8);
	(void)profile_idc;
	(void)br_read_bits(&br, 8); /* constraint flags + reserved */
	(void)br_read_bits(&br, 8); /* level_idc */
	(void)br_read_ue(&br);      /* sps id */

	if (profile_is_frext(profile_idc)) {
		chroma_format_idc = br_read_ue(&br);
		if (chroma_format_idc == 3) (void)br_read_bit(&br); /* separate_colour_plane_flag */
		(void)br_read_ue(&br); /* bit_depth_luma_minus8 */
		(void)br_read_ue(&br); /* bit_depth_chroma_minus8 */
		(void)br_read_bit(&br); /* qpprime_y_zero_transform_bypass_flag */
		if (br_read_bit(&br)) { /* seq_scaling_matrix_present_flag */
			num_lists = (chroma_format_idc == 3) ? 12 : 8;
			for (i = 0; i < num_lists; ++i) {
				if (br_read_bit(&br)) {
					size = (i < 6) ? 16 : 64;
					last = 8; next = 8;
					for (j = 0; j < size; ++j) {
						if (next) {
							int delta;
							delta = br_read_se(&br);
							next = (last + delta + 256) & 0xFF;
						}
						last = next ? next : last;
					}
				}
			}
		}
	}

	(void)br_read_ue(&br); /* log2_max_frame_num_minus4 */
	poc_type = br_read_ue(&br);
	if (poc_type == 0) {
		(void)br_read_ue(&br);
	} else if (poc_type == 1) {
		(void)br_read_bit(&br);
		(void)br_read_se(&br);
		(void)br_read_se(&br);
		n = br_read_ue(&br);
		for (i = 0; i < (int)n; ++i) (void)br_read_se(&br);
	}
	(void)br_read_ue(&br); /* max_num_ref_frames */
	(void)br_read_bit(&br); /* gaps_in_frame_num_value_allowed_flag */
	(void)br_read_ue(&br); /* pic_width_in_mbs_minus1 */
	(void)br_read_ue(&br); /* pic_height_in_map_units_minus1 */
	frame_mbs_only_flag = br_read_bit(&br);
	if (!frame_mbs_only_flag) (void)br_read_bit(&br); /* mb_adaptive_frame_field_flag */
	(void)br_read_bit(&br); /* direct_8x8_inference_flag */
	if (br_read_bit(&br)) { /* frame_cropping_flag */
		(void)br_read_ue(&br); (void)br_read_ue(&br);
		(void)br_read_ue(&br); (void)br_read_ue(&br);
	}

	/* 여기 위치가 vui_parameters_present_flag */
	vui_bitpos = br.bitpos;
	if (br_bits_left(&br) <= 0) return -1;
	return vui_bitpos;
}

/* ------------ Core: strip VUI on one SPS EBSP ------------ */
static int strip_vui_from_sps_ebsp(const u8 *in_ebsp, int in_len,
                                   u8 *out_ebsp, int out_max)
{
	u8 *rbsp;
	u8 *rbsp_out;
	int rbsp_len;
	int rbsp_out_len;
	int ret_len;
	struct bitreader br_chk;
	struct bitwriter bw;
	int vui_bitpos;
	u32 vui_present;

	rbsp      = kmalloc(in_len, GFP_KERNEL);
	rbsp_out  = kmalloc(in_len, GFP_KERNEL);
	rbsp_out_len = 0;
	ret_len   = -1;

	if (!rbsp || !rbsp_out) goto out;

	rbsp_len = ebsp_to_rbsp(in_ebsp, in_len, rbsp, in_len);

	vui_bitpos = sps_locate_vui_flag_bitpos(rbsp, rbsp_len);
	if (vui_bitpos < 0) {
		if (in_len <= out_max) memcpy(out_ebsp, in_ebsp, in_len);
		ret_len = in_len;
		goto out;
	}

	br_init(&br_chk, rbsp, rbsp_len);
	br_chk.bitpos = vui_bitpos;
	vui_present = br_read_bit(&br_chk);

	if (!vui_present) {
		if (in_len <= out_max) memcpy(out_ebsp, in_ebsp, in_len);
		ret_len = in_len;
		goto out;
	}

	bw_init(&bw, rbsp_out, in_len);

	/* VUI 플래그 이전 비트 복사 */
	if (vui_bitpos > 0) bw_copy_bits(&bw, rbsp, vui_bitpos);

	/* VUI 플래그 = 0 */
	bw_write_bit(&bw, 0);

	/* rbsp_trailing_bits: stop_one_bit=1 + 바이트 경계까지 0 */
	bw_write_bit(&bw, 1);
	while (bw.bitpos & 7) bw_write_bit(&bw, 0);

	rbsp_out_len = (bw.bitpos + 7) >> 3;
	ret_len = rbsp_to_ebsp(rbsp_out, rbsp_out_len, out_ebsp, out_max);

out:
	if (rbsp) kfree(rbsp);
	if (rbsp_out) kfree(rbsp_out);
	return ret_len; /* EBSP length (payload after NAL header) */
}

/* ------------ Top: iterate Annex-B, rewrite SPS only ------------ */
int tccvenc_h264_strip_vui_in_place_annexb(u8 *buf, int *len_inout)
{
	int in_len;
	u8 *out;
	int rd, wr;
	int sc_size, sc_off;
	int next_search_from, next_sc_size, next_sc_off;
	int nal_start, nal_end;
	u8 nal_hdr, nal_type;
	const u8 *in_ebsp;
	int in_ebsp_len;
	int new_len;

	in_len = *len_inout;
	out = kmalloc(in_len + 16, GFP_KERNEL);
	if (!out) return -ENOMEM;

	rd = 0;
	wr = 0;

	while (rd < in_len) {
		sc_off = find_start_code(buf + rd, in_len - rd, &sc_size);
		if (sc_off < 0) break;

		nal_start = rd + sc_off + sc_size;

		next_search_from = nal_start;
		next_sc_off = find_start_code(buf + next_search_from, in_len - next_search_from, &next_sc_size);
		nal_end = (next_sc_off >= 0) ? (next_search_from + next_sc_off) : in_len;

		/* copy start code */
		memcpy(out + wr, buf + rd + sc_off, sc_size);
		wr += sc_size;

		if (nal_start >= nal_end) {
			rd = nal_end;
			continue;
		}

		/* NAL header */
		nal_hdr  = buf[nal_start];
		nal_type = nal_hdr & 0x1F;
		out[wr++] = nal_hdr;

		in_ebsp     = buf + nal_start + 1;
		in_ebsp_len = nal_end - (nal_start + 1);

		if (nal_type == NAL_SPS) {
			new_len = strip_vui_from_sps_ebsp(in_ebsp, in_ebsp_len, out + wr, (in_len + 16) - wr);
			if (new_len < 0) {
				memcpy(out + wr, in_ebsp, in_ebsp_len);
				wr += in_ebsp_len;
			} else {
				wr += new_len;
			}
		} else {
			memcpy(out + wr, in_ebsp, in_ebsp_len);
			wr += in_ebsp_len;
		}

		rd = nal_end;
	}

	if (rd < in_len) {
		memcpy(out + wr, buf + rd, in_len - rd);
		wr += (in_len - rd);
	}

	memcpy(buf, out, wr);
	*len_inout = wr;

	kfree(out);
	return 0;
}
