// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_comm.h"

#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC

#include "hevc_mgr_flexio.h"
#include "vpu_rm.h"

#include "vpu_buffer.h"
#include "vpu_devices.h"
#include "hevc_mgr_sys.h"


////////////////////////////////////////////////////////////////////////////
/* flex array converter for v1 struct: HEVC_INIT_t */
////////////////////////////////////////////////////////////////////////////
/* bitstream buffer */
static void v2hevcmgr_assign_hwbuf_bs(struct vpu_decoder_data *vdata,
									  struct v2hw_flex_io *fli)
{
	int32_t res = 0;
	MEM_ALLOC_INFO_t mai = {};

	/* [V2:KI:INI:F:08] */
	V2_FLEXIP_GET(fli, INI_BS_SIZE, res);
	if (res > 0) {
		mai.request_size = ALIGNED_BUFF((unsigned)res, 4096u);
	} else {
		mai.request_size = ALIGNED_BUFF(WAVE4_STREAM_BUF_SIZE, 4096u);
	}

	mai.buffer_type = BUFFER_STREAM;

	res = vmem_proc_alloc_memory(vdata->gsCodecType, &mai, vdata->gsDecType);
	if (res == 0) {
		/* v1 value for key|08|: bitstream buffer size */
		V2_FLEXIP_SET(fli, INI_BS_SIZE, mai.request_size);
		/* v1 value for key|12|13|: bitstream buffer addr (PA) */
		V2_FLEXIP_SET_ADDR(fli, INI_BS_PA, mai.phy_addr);
		/* v1 value for key|14|15|: bitstream buffer addr (VA) */
		V2_FLEXIP_SET_ADDR(fli, INI_BS_VA, (codec_addr_t)mai.kernel_remap_addr);
	}
}

/* userdata buffer */
static void v2hevcmgr_assign_hwbuf_ud(struct vpu_decoder_data *vdata,
									  struct v2hw_flex_io *fli)
{
	int32_t res = 0;
	MEM_ALLOC_INFO_t mai = {};

	/* [KI:INI:F:09] */
	V2_FLEXIP_GET(fli, INI_UD_SIZE, res);
	if (res > 0) {
		mai.request_size = ALIGNED_BUFF((unsigned)res, 4096u);
	} else {
		mai.request_size = ALIGNED_BUFF(WAVE4_USERDATA_BUF_SIZE, 4096u);
	}

	mai.buffer_type = BUFFER_USERDATA;

	res = vmem_proc_alloc_memory(vdata->gsCodecType, &mai, vdata->gsDecType);
	if (res == 0) {
		/* v1 value for key|09|: userdata buffer size */
		V2_FLEXIP_SET(fli, INI_UD_SIZE, mai.request_size);
		/* v1 value for key|16|17|: userdata buffer addr (PA) */
		V2_FLEXIP_SET_ADDR(fli, INI_UD_PA, mai.phy_addr);
		/* v1 value for key|18|19|: userdata buffer addr (VA) */
		V2_FLEXIP_SET_ADDR(fli, INI_UD_VA, (codec_addr_t)mai.kernel_remap_addr);
	}
}

/* bit work buffer */
static void v2hevcmgr_assign_hwbuf_bw(struct vpu_decoder_data *vdata,
									  struct v2hw_flex_io *fli)
{
	int ret;
	MEM_ALLOC_INFO_t mai = {};
	mai.request_size = ALIGNED_BUFF(WAVE4_WORK_CODE_BUF_SIZE, 4096u);
	mai.buffer_type = BUFFER_WORK;

	ret = vmem_proc_alloc_memory(vdata->gsCodecType, &mai, vdata->gsDecType);
	if (ret == 0) {
		/* v1 value for key|08|: bit-work buffer size */
		V2_FLEXIP_SET(fli, INI_BW_SIZE, mai.request_size);
		/* v1 value for key|17|18|: bit-work buffer addr (PA) */
		V2_FLEXIP_SET_ADDR(fli, INI_BW_PA, mai.phy_addr);
		/* v1 value for key|19|20|: bit-work buffer addr (VA) */
		V2_FLEXIP_SET_ADDR(fli, INI_BW_VA, (codec_addr_t)mai.kernel_remap_addr);
	}
}

void *v2hevcmgr_unmarshal_ip_inidata(void *args)
{
	struct v2hw_flex_io *fli;
	struct vpu_decoder_data *vdata;
	union { void *ptr; uint64_t off; } uniaddr;

	HEVC_INIT_t *init_handle;
	hevc_dec_init_t *ini_info;

	int32_t flx_field;
	int32_t bs_mode = 0, max_fb_mode = 0;

	fli = (struct v2hw_flex_io *)args;
	if (fli == NULL) {
		(void)pr_err("%s: flex io is nullified !!", __func__);
		return NULL;
	}

	uniaddr.off = fli->v1_strt;
	vdata = (struct vpu_decoder_data *)uniaddr.ptr;
	if (vdata == NULL) {
		(void)pr_err("%s: vdata is nullified !!", __func__);
		return NULL;
	}

	init_handle = &vdata->gsHevcDecInit_Info;
	ini_info = &init_handle->gsHevcDecInit;

	/* reset fields unused/unassigned by user space client */
	init_handle->result = 0;
	init_handle->gsHevcDecHandle = 0;

	/* -----------------------------------------------
	 * convert flex fields into legacy v1 structure
	 * ----------------------------------------------- */

	/* ==== set v1 basic field  ==== */
	/* [V2:KI:INI:F:00] */
	V2_FLEXIP_GET(fli, INI_FORMAT, ini_info->m_iBitstreamFormat);
	if (vdata->gsCodecType != ini_info->m_iBitstreamFormat) {
		(void)pr_err("%s: codec type mismatch (%d vs. %d)", __func__,
				vdata->gsCodecType, ini_info->m_iBitstreamFormat);

		vdata->gsCodecType = ini_info->m_iBitstreamFormat;
	}

	/* [V2:KI:INI:F:07] */
	V2_FLEXIP_GET(fli, INI_BSMOD, bs_mode);
	switch (bs_mode) {
	case V2_VAL_BSMOD_QUE:
		(void)pr_info("[%s] bs access: queue", __func__);
		vdata->gsIsDiminishedCopy = 1;
		ini_info->m_iFilePlayEnable = 1;
		break;
	case V2_VAL_BSMOD_RNG:
		(void)pr_info("[%s] bs access: cyclic", __func__);
		vdata->gsIsDiminishedCopy = 0;
		ini_info->m_iFilePlayEnable = 0;
		break;
	default:
		(void)pr_info("[%s] bs access: linear", __func__);
		vdata->gsIsDiminishedCopy = 0;
		ini_info->m_iFilePlayEnable = 1;
		break;
	}

	/* ==== set v1 boolean field  ==== */
	/* [V2:KI:INI:B:02] */
	V2_FLEX_B_GET(fli, INI_B_USRDAT, ini_info->m_bEnableUserData);
	/* [V2:KI:INI:B:03] */
	V2_FLEX_B_GET(fli, INI_B_MATRIX, ini_info->m_bCbCrInterleaveMode);


	/* ==== get and set v1 hw buffer field ==== */
	/* assign hw buffers at d/d side */
	v2hevcmgr_assign_hwbuf_bs(vdata, fli);
	v2hevcmgr_assign_hwbuf_bw(vdata, fli);
	if (V2_FLEXIP_B_CHECK(fli, INI_B_USRDAT)) {
		v2hevcmgr_assign_hwbuf_ud(vdata, fli);
	}

	/* ==== set v1 hw buffer field (should be deprecated) ==== */
	/* [V2:KI:INI:F:08] */
	V2_FLEXIP_GET(fli, INI_BS_SIZE, ini_info->m_iBitstreamBufSize);

	/* [V2:KI:INI:F:12|13] */
	V2_FLEXIP_GET_ADDR(fli, INI_BS_PA, ini_info->m_BitstreamBufAddr[PA]);
	/* [V2:KI:INI:F:14|15] */
	V2_FLEXIP_GET_ADDR(fli, INI_BS_VA, ini_info->m_BitstreamBufAddr[VA]);

	(void)pr_info("[%s][bs] PA = %#llx, VA = %#llx", __func__,
			(unsigned long long)ini_info->m_BitstreamBufAddr[PA],
			(unsigned long long)ini_info->m_BitstreamBufAddr[VA]);

	if (V2_FLEXIP_B_CHECK(fli, INI_B_USRDAT)) {
		/* [V2:KI:INI:F:09] */
		V2_FLEXIP_GET(fli, INI_UD_SIZE, ini_info->m_iUserDataBufferSize);

		/* [V2:KI:INI:F:16|17] */
		V2_FLEXIP_GET_ADDR(fli, INI_UD_PA, ini_info->m_UserDataAddr[PA]);
		/* [V2:KI:INI:F:18|19] */
		V2_FLEXIP_GET_ADDR(fli, INI_UD_VA, ini_info->m_UserDataAddr[VA]);

		(void)pr_info("[%s][ud] PA = %#llx, VA = %#llx", __func__,
				(unsigned long long)ini_info->m_UserDataAddr[PA],
				(unsigned long long)ini_info->m_UserDataAddr[VA]);
	}

	/* [V2:KI:INI:F:20|21] */
	V2_FLEXIP_GET_ADDR(fli, INI_BW_PA, ini_info->m_BitWorkAddr[PA]);
	/* [V2:KI:INI:F:22|23] */
	V2_FLEXIP_GET_ADDR(fli, INI_BW_VA, ini_info->m_BitWorkAddr[VA]);

	(void)pr_info("[%s][bw] PA = %#llx, VA = %#llx", __func__,
			(unsigned long long)ini_info->m_BitWorkAddr[PA],
			(unsigned long long)ini_info->m_BitWorkAddr[VA]);

	/* ==== set v1 option field ==== */
	ini_info->m_uiDecOptFlags = 0u;

	/* option: no compressed output */
	V2_FLEX_B_GET(fli, INI_B_MAPOFF, flx_field);
	if (flx_field > 0) {
		(void)pr_info("[%s] mapdata off", __func__);
		ini_info->m_uiDecOptFlags |= WAVE4_WTL_ENABLE; // (1 << 0u)
	}

	/* option: no buffer delay */
	V2_FLEX_B_GET(fli, INI_B_NBDELY, flx_field);
	if (flx_field > 0) {
		(void)pr_info("[%s] no display reordering", __func__);
		ini_info->m_uiDecOptFlags |= (1u << 2u);
	}

	/* option: fixed 8bit depth output */
	V2_FLEX_B_GET(fli, INI_B_10TO8, flx_field);
	if (flx_field > 0) {
		(void)pr_info("[%s] bit depth 10 to 8", __func__);
		ini_info->m_uiDecOptFlags |= WAVE4_10BITS_DISABLE; // (1u << 3u)
		/* reserved: max bit depth */
		ini_info->m_Reserved[5] = 8;
	} else {
		/* reserved: max bit depth */
		ini_info->m_Reserved[5] = 10;
	}
	(void)pr_info("[%s] bit depth %d", __func__, ini_info->m_Reserved[5]);

	/* option: virtual queue of linear bs */
	if (bs_mode == V2_VAL_BSMOD_QUE) {
		ini_info->m_uiDecOptFlags |= (unsigned int)(0x4000000); //1u << 26u;
	}

	/* option: max fb mode */
	V2_FLEX_B_GET(fli, INI_B_MAXFRM, max_fb_mode);
	if (max_fb_mode > 0) {
		//(void)pr_info("[%s] max fb mode", __func__);
		ini_info->m_uiDecOptFlags |= (unsigned int)(0x10000); //(1u << 16u); // HEVC D9 only
	}

	/* ==== set v1 reserved field ==== */
	/* reserved: max w x h */
	if (max_fb_mode > 0) {
		/* [V2:KI:INI:F:01] */
		V2_FLEXIP_GET(fli, INI_MFB_W, ini_info->m_Reserved[3]);
		/* [V2:KI:INI:F:02] */
		V2_FLEXIP_GET(fli, INI_MFB_H, ini_info->m_Reserved[4]);
		(void)pr_info("[%s] max fb mode (%d x %d)", __func__,
				ini_info->m_Reserved[3], ini_info->m_Reserved[4]);
	}

	return (void *)init_handle;
}

void v2hevcmgr_marshal_op_inidata(void *args)
{
	struct v2hw_flex_io *flo;
	struct vpu_decoder_data *vdata;
	size_t index;

	union { void *ptr; uint64_t off; } uniaddr;

	int32_t      flxip_value = 0;
	codec_addr_t flxip_caddr = 0UL;

	struct v2hw_flex_io *fli = (struct v2hw_flex_io *)args;
	if (fli == NULL) {
		(void)pr_err("%s: fli is nullified", __func__);
		return;
	}

	uniaddr.off = fli->v1_strt;
	vdata = (struct vpu_decoder_data *)uniaddr.ptr;
	if (vdata == NULL) {
		(void)pr_err("%s: vdata is nullified !!", __func__);
		return;
	}

	/* assign flex array for 'inidata' op (output process) */
	index = V2D_FINDEX(V2D_OP_DRV_INI);
	V2_FLEXIO_ASSIGN(vdata->flx_list[index], V2D_FO_INI_MAX);
	flo = vdata->flx_list[index];
	if (flo == NULL) {
		(void)pr_err("[%s] failed to kzalloc flo (size %u)", __func__, V2D_FO_INI_MAX);
		return;
	}

	/* ==== convert v1 basic field  ==== */
	/* [KO:INI:F:00|01] */
	V2_FLEXIP_GET_ADDR(fli, INI_BS_PA, flxip_caddr);
	V2_FLEXOP_SET_ADDR(flo, INI_BS_PA, flxip_caddr);

	/* [KO:INI:F:02] */
	V2_FLEXIP_GET(fli, INI_BS_SIZE, flxip_value);
	V2_FLEXOP_SET(flo, INI_BS_SIZE, flxip_value);

	/* [KO:INI:F:03|04] */
	V2_FLEXIP_GET_ADDR(fli, INI_UD_PA, flxip_caddr);
	V2_FLEXOP_SET_ADDR(flo, INI_UD_PA, flxip_caddr);
	/* [KO:INI:F:05|06] */
	V2_FLEXIP_GET_ADDR(fli, INI_UD_VA, flxip_caddr);
	V2_FLEXOP_SET_ADDR(flo, INI_UD_VA, flxip_caddr);

	/* [KO:INI:F:07] */
	V2_FLEXIP_GET(fli, INI_UD_SIZE, flxip_value);
	V2_FLEXOP_SET(flo, INI_UD_SIZE, flxip_value);

	/* [KO:INI:F:08] */
	// [CHECK] HEVC D9 works with virtual dual bitstream.
	V2_FLEXOP_SET(flo, INI_CQ_CAPS, 2);

#if 0
	(void)printk(V2_FLXMETA_PRINTFMT(flo));
	for (index = 0; index < flo->maxfields; index++) {
		(void)printk("[%s][%zu] %#x", __func__, index, flo->fields[index]);
	}
#endif
}

////////////////////////////////////////////////////////////////////////////
/* flex array converter for v1 struct: HEVC_SEQ_HEADER_t */
////////////////////////////////////////////////////////////////////////////
void *v2hevcmgr_unmarshal_ip_seqdata(void *args)
{
	struct v2hw_flex_io *fli;
	struct vpu_decoder_data *vdata;
	union { void *ptr; uint64_t off; } uniaddr;

	HEVC_SEQ_HEADER_t *seq_handle;

	int32_t flx_field = 0;

	fli = (struct v2hw_flex_io *)args;
	if (fli == NULL) {
		(void)pr_err("%s: flex io is nullified !!", __func__);
		return NULL;
	}

	uniaddr.off = fli->v1_strt;
	vdata = (struct vpu_decoder_data *)uniaddr.ptr;
	if (vdata == NULL) {
		(void)pr_err("%s: vdata is nullified !!", __func__);
		return NULL;
	}

	seq_handle = &vdata->gsHevcDecSeqHeader_Info;

	/* reset unuset field from user-space */
	seq_handle->result = 0;

	/* -----------------------------------------------
	 * convert flex fields into legacy v1 structure
	 * -----------------------------------------------
	 */

	/* ==== set v1 basic field  ==== */
	/* [KI:DECSEQ:F:00] */
	V2_FLEXIP_GET(fli, DECSEQ_SIZE, seq_handle->stream_size);
	(void)pr_info("[%s] size: %d", __func__, seq_handle->stream_size);
	/* [KI:DECSEQ:F:01] */
	V2_FLEXIP_GET(fli, DECSEQ_FBMOD, flx_field);
	(void)pr_info("[%s] fb mode: %d", __func__, flx_field);
	/* [KI:DECSEQ:F:02] */
	V2_FLEXIP_GET(fli, DECSEQ_SCALE, flx_field);
	(void)pr_info("[%s] scale: %d", __func__, flx_field);

	return (void *)seq_handle;
}

void v2hevcmgr_marshal_op_seqdata(void *args)
{
	struct v2hw_flex_io *flo;
	struct vpu_decoder_data *vdata;
	union { void *ptr; uint64_t off; } uniaddr;
	size_t index;

	HEVC_SEQ_HEADER_t *seq_handle;
	hevc_dec_initial_info_t *seq_info;

	struct v2hw_flex_io *fli = (struct v2hw_flex_io *)args;
	if (fli == NULL) {
		(void)pr_err("%s: fli is nullified", __func__);
		return;
	}

	uniaddr.off = fli->v1_strt;
	vdata = (struct vpu_decoder_data *)uniaddr.ptr;
	if (vdata == NULL) {
		(void)pr_err("%s: vdata is nullified !!", __func__);
		return;
	}

	/* assign flex array for 'seqdata' output process */
	index = V2D_FINDEX(V2D_OP_DEC_SEQDATA);
	V2_FLEXIO_ASSIGN(vdata->flx_list[index], V2D_FO_DECSEQ_MAX);
	flo = vdata->flx_list[index];
	if (flo == NULL) {
		(void)pr_err("[%s] failed to kzalloc flo (size %u)", __func__, V2D_FO_DECSEQ_MAX);
		return;
	}

	seq_handle = &vdata->gsHevcDecSeqHeader_Info;
	seq_info = &seq_handle->gsHevcDecInitialInfo;

	/* ==== convert v1 basic field  ==== */
	/* [V2:KO:DECSEQ:F:00] */
	V2_FLEXOP_SET(flo, DECSEQ_ERRNO, seq_info->m_iReportErrorReason);

	/* [V2:KO:DECSEQ:F:01] */
	V2_FLEXOP_SET(flo, DECSEQ_PIC_W, seq_info->m_iPicWidth);

	/* [V2:KO:DECSEQ:F:02] */
	V2_FLEXOP_SET(flo, DECSEQ_PIC_H, seq_info->m_iPicHeight);

	/* [V2:KO:DECSEQ:F:03] frame-rate kilo */
	if ((seq_info->m_uiFrameRateDiv != 0u) &&
		(seq_info->m_uiFrameRateDiv < (0xFFFFFFFFu / 120u)) /* max. 120 fps */ &&
		(seq_info->m_uiFrameRateRes != 0u) &&
		(seq_info->m_uiFrameRateRes < (0xFFFFFFFFu / 1000u))) {
		uint32_t frame_rate = (seq_info->m_uiFrameRateRes * 1000)
			/ seq_info->m_uiFrameRateDiv;
		V2_FLEXOP_SET(flo, DECSEQ_FRM_R, frame_rate);
	}


	/* [V2:KO:DECSEQ:F:04] */
	V2_FLEXOP_SET(flo, DECSEQ_FRM_N, seq_info->m_iMinFrameBufferCount);

	/* [V2:KO:DECSEQ:F:05] */
	V2_FLEXOP_SET(flo, DECSEQ_FRM_X, 0); // [FIXME] how to mmap hw bufffer in kernel space

	/* [V2:KO:DECSEQ:F:06|07|08|09] window coordinate */
	V2_FLEXOP_SET(flo, DECSEQ_WIN_L, seq_info->m_PicCrop.m_iCropLeft);
	V2_FLEXOP_SET(flo, DECSEQ_WIN_T, seq_info->m_PicCrop.m_iCropTop);
	V2_FLEXOP_SET(flo, DECSEQ_WIN_R, seq_info->m_PicCrop.m_iCropRight);
	V2_FLEXOP_SET(flo, DECSEQ_WIN_B, seq_info->m_PicCrop.m_iCropBottom);

	/* [V2:KO:DECSEQ:F:10|11|12|13] color aspect */
	V2_FLEXOP_SET(flo, DECSEQ_CA_FR, seq_info->m_UserDataInfo.m_VuiParam.video_full_range_flag);
	V2_FLEXOP_SET(flo, DECSEQ_CA_PR, seq_info->m_UserDataInfo.m_VuiParam.colour_primaries);
	V2_FLEXOP_SET(flo, DECSEQ_CA_MC, seq_info->m_UserDataInfo.m_VuiParam.matrix_coefficients);
	V2_FLEXOP_SET(flo, DECSEQ_CA_TR, seq_info->m_UserDataInfo.m_VuiParam.transfer_characteristics);

	/* [V2:KO:DECSEQ:F:14] bit depth */
	//V2_FLEXOP_SET(flo, DECSEQ_BTDP, 0); // WAVE4 not support

	/* [V2:KO:DECSEQ:F:15] reordering depth */
	V2_FLEXOP_SET(flo, DECSEQ_RODP, seq_info->m_iFrameBufDelay);

	/* [V2:KO:DECSEQ:F:16] */
	V2_FLEXOP_SET(flo, DECSEQ_PRFL, seq_info->m_iProfile);

	/* [V2:KO:DECSEQ:F:17] */
	V2_FLEXOP_SET(flo, DECSEQ_LEVL, seq_info->m_iLevel);

	/* [V2:KO:DECSEQ:F:18] */
	V2_FLEXOP_SET(flo, DECSEQ_TIER, seq_info->m_iTier);

	/* [V2:KO:DECSEQ:F:19] */
	//V2_FLEXOP_SET(flo, DECSEQ_INTL, 0); // WAVE4 not support

	/* [V2:KO:DECSEQ:F:20] fb element size */
	V2_FLEXOP_SET(flo, DECSEQ_FRM_SZ, seq_info->m_iMinFrameBufferSize);

	/* [V2:KO:DECSEQ:F:21] CSI for AR */
	V2_FLEXOP_SET(flo, DECSEQ_CSAR, 0); // not used

	/* [V2:KO:DECSEQ:F:22|23] CSI */
	V2_FLEXOP_SET(flo, DECSEQ_CNT, 0); // not used
	V2_FLEXOP_SET(flo, DECSEQ_CSI, 0); // not used

#if 0
	(void)printk(V2_FLXMETA_PRINTFMT(flo));
	for (index = 0; index < flo->maxfields; index++) {
		(void)pr_info("[%s][%zu] %#x", __func__, index, flo->fields[index]);
	}
#endif
}

int32_t v2hevcmgr_register_hwbuf_fb(void *args, long handle, tccfp_vpu_proc_t tcc_hevc_dec)
{
	struct v2hw_flex_io *fli = (struct v2hw_flex_io *)args;
	struct v2hw_flex_io *flo;

	struct vpu_decoder_data *vdata;
	union { void *ptr; uint64_t off; } uniaddr;

	int32_t final_count;
	uint32_t total_size;

	int32_t max_count = 0, min_count = 0, ext_count = 0;

	int32_t min_size = 0;
	const int32_t max_frame_slot = 31;

	int32_t res;
	MEM_ALLOC_INFO_t mai = {};

	uniaddr.off = fli->v1_strt;
	vdata = (struct vpu_decoder_data *)uniaddr.ptr;
	if (vdata == NULL) {
		(void)pr_err("%s: vdata is nullified !!", __func__);
		return RETCODE_FAILURE;
	}

	flo = vdata->flx_list[V2D_FINDEX(V2D_OP_DEC_SEQDATA)];

	/* [KI:DECSEQ:F:02] ext frame count */
	V2_FLEXIP_GET(fli, DECSEQ_FBEXT, ext_count);

	/* [KO:DECSEQ:F:04] min frame count */
	V2_FLEXOP_CND_GET(flo, DECSEQ_FRM_N, min_count);

	/* [KO:DECSEQ:F:20] min frame size */
	V2_FLEXOP_GET(flo, DECSEQ_FRM_SZ, min_size);

	if (min_size == 0) {
		(void)pr_err("%s: min frame size is zero !!", __func__);
		return RETCODE_CODEC_EXIT;
	}

	final_count = min_count + ext_count;

	// limitation: do not exceed vpu memory capacity
	max_count = vmem_get_free_memory(vdata->gsDecType) / min_size;
	if (final_count > max_count) {
		final_count = max_count;
	}

	// limitation: do not exceed max vpu-fw slot count
	// [CHECK] all vpu fw apply this limitation ?
	if (final_count > max_frame_slot) {
		final_count = max_frame_slot;
	}

	if (min_size != 0 && final_count > UINT_MAX / min_size) {
		(void)pr_err("[%s:%d] overflow has occurred in totla_size", __func__, __LINE__);
		return RETCODE_CODEC_EXIT;
	} else {
		total_size = final_count * min_size;
	}

	mai.request_size = ALIGNED_BUFF(total_size, 4096u);
	mai.buffer_type = BUFFER_FRAMEBUFFER;

	(void)pr_info("[%s] count(ext: %d, min: %d, final: %d) size(min:%d, tot:%u)",
			__func__, ext_count, min_count, final_count, min_size, mai.request_size);

	res = vmem_proc_alloc_memory(vdata->gsCodecType, &mai, vdata->gsDecType);
	if (res == 0) {
		HEVC_SET_BUFFER_t *set_handle = &vdata->gsHevcDecBuffer_Info;
		hevc_dec_buffer_t *set_info   = &set_handle->gsHevcDecBuffer;

		set_info->m_iFrameBufferCount = final_count;
		set_info->m_FrameBufferStartAddr[PA] = mai.phy_addr;

		(void)pr_info("[%s][fb] PA = %#llx count = %d", __func__,
				(unsigned long long)set_info->m_FrameBufferStartAddr[PA],
				set_info->m_iFrameBufferCount);

		res = tcc_hevc_dec(VPU_DEC_REG_FRAME_BUFFER,
				(vcodec_handle_t *)&handle,
				(void *)set_info,
				(void *)NULL);

		(void)pr_info("[%s] op (reg-fb) result = %d", __func__, res);
	} else {
		(void)pr_err("[%s] failed to call: vmem_proc_alloc_memory", __func__);
		res = RETCODE_FAILURE;
	}
	return res;
}


////////////////////////////////////////////////////////////////////////////
/* flex array converter for v1 struct: HEVC_DECODE_t */
////////////////////////////////////////////////////////////////////////////
void *v2hevcmgr_unmarshal_ip_drndata(void *args)
{
	struct v2hw_flex_io *fli;
	struct vpu_decoder_data *vdata;
	union { void *ptr; uint64_t off; } uniaddr;

	HEVC_DECODE_t *frm_handle;
	hevc_dec_input_t *dec_input;

	fli = (struct v2hw_flex_io *)args;
	if (fli == NULL) {
		(void)pr_err("%s: flex io is nullified !!", __func__);
		return NULL;
	}

	uniaddr.off = fli->v1_strt;
	vdata = (struct vpu_decoder_data *)uniaddr.ptr;
	if (vdata == NULL) {
		(void)pr_err("%s: vdata is nullified !!", __func__);
		return NULL;
	}

	frm_handle = &vdata->gsHevcDecInOut_Info;
	dec_input = &frm_handle->gsHevcDecInput;

	/* reset unuset field from user-space */
	frm_handle->result = 0;

	/* -----------------------------------------------
	 * convert flex fields into legacy v1 structure
	 * -----------------------------------------------
	 */
	/* [V2:KI:DRAIN:F:00|01] */
	V2_FLEXIP_GET_ADDR(fli, DECFRM_UD_PA, dec_input->m_UserDataAddr[PA]);
	/* [V2:KI:DRAIN:F:02|03] */
	V2_FLEXIP_GET_ADDR(fli, DECFRM_UD_VA, dec_input->m_UserDataAddr[VA]);
	/* [V2:KI:DRAIN:F:04] */
	V2_FLEXIP_GET(fli, DECFRM_UD_SIZE, dec_input->m_iUserDataBufferSize);

	dec_input->m_iBitstreamDataSize = 0;
	dec_input->m_iSkipFrameMode = 0;

	return (void *)frm_handle;
}

void *v2hevcmgr_unmarshal_ip_frmdata(void *args)
{
	struct v2hw_flex_io *fli;
	struct vpu_decoder_data *vdata;
	union { void *ptr; uint64_t off; } uniaddr;

	HEVC_DECODE_t *frm_handle;
	hevc_dec_input_t *dec_input;

	fli = (struct v2hw_flex_io *)args;
	if (fli == NULL) {
		(void)pr_err("%s: flex io is nullified !!", __func__);
		return NULL;
	}

	uniaddr.off = fli->v1_strt;
	vdata = (struct vpu_decoder_data *)uniaddr.ptr;
	if (vdata == NULL) {
		(void)pr_err("%s: vdata is nullified !!", __func__);
		return NULL;
	}

	frm_handle = &vdata->gsHevcDecInOut_Info;
	dec_input = &frm_handle->gsHevcDecInput;

	/* reset unuset field from user-space */
	frm_handle->result = 0;

	/* -----------------------------------------------
	 * convert flex fields into legacy v1 structure
	 * -----------------------------------------------
	 */
	/* ==== set v1 basic field  ==== */
	/* [V2:KI:DECFRM:F:00|01] */ // [CHECK] why va is unnecessary ??
	V2_FLEXIP_GET_ADDR(fli, DECFRM_BS_PA, dec_input->m_BitstreamDataAddr[PA]);
	/* [V2:KI:DECFRM:F:02] */
	V2_FLEXIP_GET(fli, DECFRM_BS_SIZE, dec_input->m_iBitstreamDataSize);

	/* [V2:KI:DECFRM:F:03|04] */
	V2_FLEXIP_GET_ADDR(fli, DECFRM_UD_PA, dec_input->m_UserDataAddr[PA]);
	/* [V2:KI:DECFRM:F:05|06] */
	V2_FLEXIP_GET_ADDR(fli, DECFRM_UD_VA, dec_input->m_UserDataAddr[VA]);
	/* [V2:KI:DECFRM:F:07] */
	V2_FLEXIP_GET(fli, DECFRM_UD_SIZE, dec_input->m_iUserDataBufferSize);

	/* [V2:KI:DECFRM:F:08] */
	V2_FLEXIP_GET(fli, DECFRM_RA_MODE, dec_input->m_iSkipFrameMode);

	return (void *)frm_handle;
}

void v2hevcmgr_marshal_op_frmdata(void *args, bool updated)
{
	struct v2hw_flex_io *flo;
	struct vpu_decoder_data *vdata;
	union { void *ptr; uint64_t off; } uniaddr;
	size_t index;
	size_t numElements;

	HEVC_DECODE_t *frm_handle;
	hevc_dec_output_t *dec_output;
	hevc_dec_output_info_t *info;
	hevc_dec_MapConv_info_t *mc;

	struct v2hw_flex_io *fli = (struct v2hw_flex_io *)args;
	if (fli == NULL) {
		(void)pr_err("%s: fli is nullified", __func__);
		return;
	}

	uniaddr.off = fli->v1_strt;
	vdata = (struct vpu_decoder_data *)uniaddr.ptr;
	if (vdata == NULL) {
		(void)pr_err("%s: vdata is nullified !!", __func__);
		return;
	}

	/* ==== set v1 boolean field  ==== */
	/* [V2:KI:DECFRM:B:04] skip frame */
	if (V2_FLEXIP_B_CHECK(fli, DECFRM_B_MAPDAT_O)) {
		numElements = V2D_FO_DECFRM_CSI + V2D_CSI_FO_MC_MAX;
	} else {
		numElements = V2D_FO_DECFRM_MAX;
	}

	/* assign flex array for 'frmdata' output process */
	index = V2D_FINDEX(V2D_OP_DEC_FRMDATA);
	V2_FLEXIO_ASSIGN(vdata->flx_list[index], numElements);
	flo = vdata->flx_list[index];
	if (flo == NULL) {
		(void)pr_err("[%s] failed to kzalloc flo (size %zu)", __func__, numElements);
		return;
	}

	if (updated) {
		frm_handle = &vdata->gsHevcDecInOut_Info;
		dec_output = &frm_handle->gsHevcDecOutput;
		info = &dec_output->m_DecOutInfo;
		mc = &info->m_DispMapConvInfo;

		/* ==== convert v1 basic field  ==== */
		/* [V2:KO:DECFRM:F:22|23] Y PA of display output */
		V2_FLEXOP_SET_ADDR(flo, DECFRM_OUT_Y_PA,
				(uintptr_t)dec_output->m_pDispOut[PA][Y]);
		/* [V2:KO:DECFRM:F:24|25] Y VA of display output */
		V2_FLEXOP_SET_ADDR(flo, DECFRM_OUT_Y_VA,
				(uintptr_t)dec_output->m_pDispOut[VA][Y]);
		/* [V2:KO:DECFRM:F:26|27] U PA of display output */
		V2_FLEXOP_SET_ADDR(flo, DECFRM_OUT_U_PA,
				(uintptr_t)dec_output->m_pDispOut[PA][U]);
		/* [V2:KO:DECFRM:F:28|29] U VA of display output */
		V2_FLEXOP_SET_ADDR(flo, DECFRM_OUT_U_VA,
				(uintptr_t)dec_output->m_pDispOut[VA][U]);
		/* [V2:KO:DECFRM:F:30|31] V PA of display output */
		V2_FLEXOP_SET_ADDR(flo, DECFRM_OUT_V_PA,
				(uintptr_t)dec_output->m_pDispOut[PA][V]);
		/* [V2:KO:DECFRM:F:32|33] V VA of display output */
		V2_FLEXOP_SET_ADDR(flo, DECFRM_OUT_V_VA,
				(uintptr_t)dec_output->m_pDispOut[VA][V]);

		/* [V2:KO:DECFRM:F:00] */
		V2_FLEXOP_SET(flo, DECFRM_PICTY, info->m_iPicType);
		/* [V2:KO:DECFRM:F:01] */
		V2_FLEXOP_SET(flo, DECFRM_OSLOT, info->m_iDispOutIdx);
		/* [V2:KO:DECFRM:F:02] */
		V2_FLEXOP_SET(flo, DECFRM_ISLOT, info->m_iDecodedIdx);
		/* [V2:KO:DECFRM:F:03] */
		V2_FLEXOP_SET(flo, DECFRM_OSTAT, info->m_iOutputStatus);
		/* [V2:KO:DECFRM:F:04] */
		V2_FLEXOP_SET(flo, DECFRM_ISTAT, info->m_iDecodingStatus);
		/* [V2:KO:DECFRM:F:05]
		 * [FIXME] m_iInterlace is located in hevc_dec_initial_info_t (DECSEQ) */
		V2_FLEXOP_SET(flo, DECFRM_INTL, 0);
		/* [V2:KO:DECFRM:F:06] */
		V2_FLEXOP_SET(flo, DECFRM_ERRMB, info->m_iNumOfErrMBs);

		/* [V2:KO:DECFRM:F:07|08|09|10] color aspects */
		V2_FLEXOP_SET(flo, DECFRM_CA_FR, info->m_UserDataInfo.m_VuiParam.video_full_range_flag);
		V2_FLEXOP_SET(flo, DECFRM_CA_PR, info->m_UserDataInfo.m_VuiParam.colour_primaries);
		V2_FLEXOP_SET(flo, DECFRM_CA_MC, info->m_UserDataInfo.m_VuiParam.matrix_coefficients);
		V2_FLEXOP_SET(flo, DECFRM_CA_TR, info->m_UserDataInfo.m_VuiParam.transfer_characteristics);

		/* [V2:KO:DECFRM:F:11|12] frame width, height */
		/* [V2:KO:DECFRM:F:13|14|15|16] crop info */
		V2_FLEXOP_SET(flo, DECFRM_FRM_W, info->m_iDecodedWidth);
		V2_FLEXOP_SET(flo, DECFRM_FRM_H, info->m_iDecodedHeight);

		V2_FLEXOP_SET(flo, DECFRM_WIN_L, info->m_DecodedCropInfo.m_iCropLeft);
		V2_FLEXOP_SET(flo, DECFRM_WIN_T, info->m_DecodedCropInfo.m_iCropTop);
		V2_FLEXOP_SET(flo, DECFRM_WIN_R, info->m_DecodedCropInfo.m_iCropRight);
		V2_FLEXOP_SET(flo, DECFRM_WIN_B, info->m_DecodedCropInfo.m_iCropBottom);

		if (V2_FLEXIP_B_CHECK(fli, DECFRM_B_MAPDAT_O)) {
			/* ==== convert v1 csi field (map converter) ==== */
			/* [V2:KO:DECFRM:F:29] */
			V2_FLEXOP_SET(flo, DECFRM_CNT, V2D_CSI_FO_MC_MAX);
			V2_FLEXOP_CSI_OFFSET(flo, V2D_FO_DECFRM_CSI);

			/* [V2:KO:DECFRM:CSI:00|01] compressed Y PA */
			V2_FLEXOP_CSI_SET_ADDR(flo, MC_MAP_Y_PA, mc->m_CompressedY[PA]);
			/* [V2:KO:DECFRM:CSI:02|03] compressed Y VA */
			V2_FLEXOP_CSI_SET_ADDR(flo, MC_MAP_Y_VA, mc->m_CompressedY[VA]);

			/* [V2:KO:DECFRM:CSI:04|05] compressed C PA */
			V2_FLEXOP_CSI_SET_ADDR(flo, MC_MAP_C_PA, mc->m_CompressedCb[PA]);
			/* [V2:KO:DECFRM:CSI:06|07] compressed C VA */
			V2_FLEXOP_CSI_SET_ADDR(flo, MC_MAP_C_VA, mc->m_CompressedCb[VA]);

			/* [V2:KO:DECFRM:CSI:08|09] fbc Y PA */
			V2_FLEXOP_CSI_SET_ADDR(flo, MC_FBC_Y_PA, mc->m_FbcYOffsetAddr[PA]);
			/* [V2:KO:DECFRM:CSI:10|11] fbc Y VA */
			V2_FLEXOP_CSI_SET_ADDR(flo, MC_FBC_Y_VA, mc->m_FbcYOffsetAddr[VA]);

			/* [V2:KO:DECFRM:CSI:12|13] fbc C PA */
			V2_FLEXOP_CSI_SET_ADDR(flo, MC_FBC_C_PA, mc->m_FbcCOffsetAddr[PA]);
			/* [V2:KO:DECFRM:CSI:14|15] fbc C VA */
			V2_FLEXOP_CSI_SET_ADDR(flo, MC_FBC_C_VA, mc->m_FbcCOffsetAddr[VA]);

			/* [V2:KO:DECFRM:CSI:16] */
			V2_FLEXOP_CSI_SET(flo, MC_LUMA_LEN, mc->m_uiCompressionTableLumaSize);
			/* [V2:KO:DECFRM:CSI:17] */
			V2_FLEXOP_CSI_SET(flo, MC_CRMA_LEN, mc->m_uiCompressionTableChromaSize);
			/* [V2:KO:DECFRM:CSI:18] */
			V2_FLEXOP_CSI_SET(flo, MC_LUMA_STR, mc->m_uiLumaStride);
			/* [V2:KO:DECFRM:CSI:19] */
			V2_FLEXOP_CSI_SET(flo, MC_CRMA_STR, mc->m_uiChromaStride);
			/* [V2:KO:DECFRM:CSI:20] */
			V2_FLEXOP_CSI_SET(flo, MC_LUMA_DEP, mc->m_uiLumaBitDepth);
			/* [V2:KO:DECFRM:CSI:21] */
			V2_FLEXOP_CSI_SET(flo, MC_CRMA_DEP, mc->m_uiChromaBitDepth);
			/* [V2:KO:DECFRM:CSI:22] */
			V2_FLEXOP_CSI_SET(flo, MC_ENDIAN, mc->m_uiFrameEndian);
			/* [V2:KO:DECFRM:CSI:23] */
			V2_FLEXOP_CSI_SET(flo, MC_CODEC, mc->m_Reserved[0]);
		}
	}

#if 0
	printk(V2_FLXMETA_PRINTFMT(flo));
	for (index = 0; i < flo->maxfields; index++) {
		(void)pr_info("[%s][%zu] %#x", __func__, index, flo->fields[index]);
	}
#endif
}

////////////////////////////////////////////////////////////////////////////
/* flex array converter for v1 struct: HEVC_RINGBUF_SETBUF_PTRONLY_t */
////////////////////////////////////////////////////////////////////////////
void *v2hevcmgr_unmarshal_ip_setpos(void *args)
{
	struct v2hw_flex_io *fli;
	struct vpu_decoder_data *vdata;
	union { void *ptr; uint64_t off; } uniaddr;

	HEVC_RINGBUF_SETBUF_PTRONLY_t *setpos_handle;

	fli = (struct v2hw_flex_io *)args;
	if (fli == NULL) {
		(void)pr_err("%s: flex io is nullified !!", __func__);
		return NULL;
	}

	uniaddr.off = fli->v1_strt;
	vdata = (struct vpu_decoder_data *)uniaddr.ptr;
	if (vdata == NULL) {
		(void)pr_err("%s: vdata is nullified !!", __func__);
		return NULL;
	}

	setpos_handle = &vdata->gsHevcDecUpdateWP;

	/* reset unuset field from user-space */
	setpos_handle->result = 0;

	/* -----------------------------------------------
	 * convert flex fields into legacy v1 structure
	 * -----------------------------------------------
	 */
	/* [V2:KI:SETPOS:F:00] */
	V2_FLEXIP_GET(fli, RNG_WRITTEN, setpos_handle->iCopiedSize);
	/* [V2:KI:SETPOS:F:01] */
	V2_FLEXIP_GET(fli, RNG_REWIND, setpos_handle->iFlushBuf);

	(void)pr_info("%s: written %d, flush %d", __func__,
		   setpos_handle->iCopiedSize, setpos_handle->iFlushBuf);

	return (void *)setpos_handle;
}

////////////////////////////////////////////////////////////////////////////
/* flex array converter for v1 struct: HEVC_RINGBUF_GETINFO_t */
////////////////////////////////////////////////////////////////////////////
void *v2hevcmgr_unmarshal_ip_getpos(void *args)
{
	struct v2hw_flex_io *fli;
	struct vpu_decoder_data *vdata;
	union { void *ptr; uint64_t off; } uniaddr;

	HEVC_RINGBUF_GETINFO_t *getpos_handle;

	fli = (struct v2hw_flex_io *)args;
	if (fli == NULL) {
		(void)pr_err("%s: flex io is nullified !!", __func__);
		return NULL;
	}

	uniaddr.off = fli->v1_strt;
	vdata = (struct vpu_decoder_data *)uniaddr.ptr;
	if (vdata == NULL) {
		(void)pr_err("%s: vdata is nullified !!", __func__);
		return NULL;
	}

	getpos_handle = &vdata->gsHevcDecBufStatus;

	/* reset unuset field from user-space */
	getpos_handle->result = 0;

	return (void *)getpos_handle;
}

void v2hevcmgr_marshal_op_getpos(void *args)
{
	struct v2hw_flex_io *flo;
	struct vpu_decoder_data *vdata;
	union { void *ptr; uint64_t off; } uniaddr;
	size_t index;

	HEVC_RINGBUF_GETINFO_t *getpos_handle;
	hevc_dec_ring_buffer_status_out_t *getpos_info;

	struct v2hw_flex_io *fli = (struct v2hw_flex_io *)args;
	if (fli == NULL) {
		(void)pr_err("%s: fli is nullified", __func__);
		return;
	}

	uniaddr.off = fli->v1_strt;
	vdata = (struct vpu_decoder_data *)uniaddr.ptr;
	if (vdata == NULL) {
		(void)pr_err("%s: vdata is nullified !!", __func__);
		return;
	}

	/* assign flex array for 'seqdata' output process */
	index = V2D_FINDEX(V2D_OP_RNG_GETPOS);
	V2_FLEXIO_ASSIGN(vdata->flx_list[index], V2D_FO_RNG_GETPOS_MAX);
	flo = vdata->flx_list[index];
	if (flo == NULL) {
		(void)pr_err("[%s] failed to kzalloc flo (size %u)",
				__func__, V2D_FO_RNG_GETPOS_MAX);
		return;
	}

	getpos_handle = &vdata->gsHevcDecBufStatus;
	getpos_info = &getpos_handle->gsHevcDecRingStatus;

	/* ==== convert v1 field ==== */
	/* [V2:KO:GETPOS:F:00] */
	V2_FLEXOP_SET(flo, RNG_AVAIL, getpos_info->m_ulAvailableSpaceInRingBuffer);
	/* [V2:KO:GETPOS:F:01] */
	V2_FLEXOP_SET(flo, RNG_RPOS, getpos_info->m_ptrReadAddr_PA);
	/* [V2:KO:GETPOS:F:02] */
	V2_FLEXOP_SET(flo, RNG_WPOS, getpos_info->m_ptrWriteAddr_PA);


#if 0
	(void)printk(V2_FLXMETA_PRINTFMT(flo));
	for (index = 0; index < flo->maxfields; index++) {
		(void)printk("[%s][%zu] %d", __func__, index, flo->fields[index]);
	}
#endif
}

#endif //#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
