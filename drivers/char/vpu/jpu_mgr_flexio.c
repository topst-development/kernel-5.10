/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_comm.h"

#ifdef CONFIG_SUPPORT_TCC_JPU

#include "jpu_mgr_flexio.h"
#include "vpu_buffer.h"
#include "vpu_devices.h"
#include "jpu_mgr_sys.h"


////////////////////////////////////////////////////////////////////////////
/* flex array converter for v1 struct: JDEC_INIT_t */
////////////////////////////////////////////////////////////////////////////
/* bitstream buffer */
static void v2jpgmgr_assign_hwbuf_bs(const struct vpu_decoder_data *vdata,
									 struct v2hw_flex_io *fli)
{
	int32_t res = 0;
	MEM_ALLOC_INFO_t mai = {};

	/* [V2:KI:INI:F:08] */
	V2_FLEXIP_GET(fli, INI_BS_SIZE, res);
	if (res > 0) {
		mai.request_size = ALIGNED_BUFF((unsigned)res, 1024u);
	} else {
		mai.request_size = ALIGNED_BUFF(LARGE_STREAM_BUF_SIZE, 1024u);
	}

	mai.buffer_type = BUFFER_STREAM;

	res = vmem_proc_alloc_memory(vdata->gsCodecType, &mai, (vputype)vdata->gsDecType);
	if (res == 0) {
		/* v1 value for key|08|: bitstream buffer size */
		V2_FLEXIP_SET(fli, INI_BS_SIZE, mai.request_size);
		/* v1 value for key|12|13|: bitstream buffer addr (PA) */
		V2_FLEXIP_SET_ADDR(fli, INI_BS_PA, mai.phy_addr);
		/* v1 value for key|14|15|: bitstream buffer addr (VA) */
		V2_FLEXIP_SET_ADDR(fli, INI_BS_VA, (codec_addr_t)mai.kernel_remap_addr);
	}
}

void *v2jpgmgr_unmarshal_ip_inidata(void *args)
{
	struct v2hw_flex_io *fli = NULL;
	struct vpu_decoder_data *vdata;
	union { void *ptr; uint64_t off; } uniaddr;

	JDEC_INIT_t *init_handle;
	jpu_dec_init_t *ini_info;

	int32_t bs_mode = 0;

	VPU_CAST_PT(fli, args);
	if (fli == NULL) {
		(void)pr_err("%s: flex io is nullified !!", __func__);
		return NULL;
	}

	uniaddr.off = fli->v1_strt;
	VPU_CAST_PT(vdata, uniaddr.ptr);
	if (vdata == NULL) {
		(void)pr_err("%s: vdata is nullified !!", __func__);
		return NULL;
	}

	init_handle = &vdata->gsJpuDecInit_Info;
	ini_info = &init_handle->gsJpuDecInit;

	/* reset fields unused/unassigned by user space client */
	init_handle->result = 0;
	init_handle->gsJpuDecHandle = 0;

	/* -----------------------------------------------
	 * convert flex fields into legacy v1 structure
	 * ----------------------------------------------- */

	/* ==== set v1 basic field  ==== */
	/* [V2:KI:INI:F:07] */
	V2_FLEXIP_GET(fli, INI_BSMOD, bs_mode);
	switch (bs_mode) {
		case (int32_t)V2_VAL_BSMOD_QUE:
			(void)pr_info("[%s] bs access: queue", __func__);
			vdata->gsIsDiminishedCopy = 1;
			break;
		default:
			(void)pr_info("[%s] bs access: linear", __func__);
			vdata->gsIsDiminishedCopy = 0;
			break;
	}

	/* ==== set v1 boolean field  ==== */
	/* [V2:KI:INI:B:03] */
	V2_FLEX_B_GET(fli, INI_B_MATRIX, ini_info->m_iCbCrInterleaveMode);

	/* ==== get and set v1 hw buffer field ==== */
	/* assign hw buffers at d/d side */
	v2jpgmgr_assign_hwbuf_bs(vdata, fli);

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

	/* ==== set v1 option field ==== */
	ini_info->m_uiDecOptFlags = 0u;

	if (bs_mode == (int32_t)V2_VAL_BSMOD_QUE) {
		ini_info->m_uiDecOptFlags |= (unsigned int)(0x4000000); //1u << 26u;
	}

	return (void *)init_handle;
}

void v2jpgmgr_marshal_op_inidata(void *args)
{
	struct v2hw_flex_io *flo = NULL;
	struct v2hw_flex_io *fli = NULL;
	struct vpu_decoder_data *vdata = NULL;
	union { void *ptr; uint64_t off; } uniaddr;
	size_t index;

	int32_t      flxip_value = 0;
	codec_addr_t flxip_caddr;

	VPU_CAST_PT(fli, args);
	if (fli == NULL) {
		(void)pr_err("%s: fli is nullified", __func__);
		return;
	}

	uniaddr.off = fli->v1_strt;
	VPU_CAST_PT(vdata, uniaddr.ptr);
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

#if 0
	(void)printk(V2_FLXMETA_PRINTFMT(flo));
	for (index = 0; index < flo->maxfields; index++) {
		(void)pr_info("[%s][%zu] %#x", __func__, index, flo->fields[index]);
	}
#endif
}

////////////////////////////////////////////////////////////////////////////
/* flex array converter for v1 struct: JDEC_SEQ_HEADER_t */
////////////////////////////////////////////////////////////////////////////
void *v2jpgmgr_unmarshal_ip_seqdata(void *args)
{
	struct v2hw_flex_io *fli;
	struct vpu_decoder_data *vdata;
	union { void *ptr; uint64_t off; } uniaddr;

	JDEC_SEQ_HEADER_t *seq_handle;

	VPU_CAST_PT(fli, args);
	if (fli == NULL) {
		(void)pr_err("%s: flex io is nullified !!", __func__);
		return NULL;
	}

	uniaddr.off = fli->v1_strt;
	VPU_CAST_PT(vdata, uniaddr.ptr);
	if (vdata == NULL) {
		(void)pr_err("%s: vdata is nullified !!", __func__);
		return NULL;
	}

	seq_handle = &vdata->gsJpuDecSeqHeader_Info;

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

	return (void *)seq_handle;
}

void v2jpgmgr_marshal_op_seqdata(void *args)
{
	struct v2hw_flex_io *flo;
	struct v2hw_flex_io *fli = NULL;
	struct vpu_decoder_data *vdata;
	union { void *ptr; uint64_t off; } uniaddr;
	size_t index;
	size_t numElements;

	JDEC_SEQ_HEADER_t *seq_handle;
	const jpu_dec_initial_info_t *seq_info;

	VPU_CAST_PT(fli, args);
	if (fli == NULL) {
		(void)pr_err("%s: fli is nullified", __func__);
		return;
	}

	uniaddr.off = fli->v1_strt;
	VPU_CAST_PT(vdata, uniaddr.ptr);
	if (vdata == NULL) {
		(void)pr_err("%s: vdata is nullified !!", __func__);
		return;
	}

	/* assign flex array for 'seqdata' output process */
	switch (vdata->gsCodecType) {
		case STD_MJPG:
			numElements = V2D_FO_DECSEQ_CSI + V2D_CSI_FO_DECSEQ_JPG_MAX;
			break;

		default:
			numElements = V2D_FO_DECSEQ_MAX;
			break;
	}

	index = V2D_FINDEX(V2D_OP_DEC_SEQDATA);
	if (vdata->flx_list[index] != NULL) {
		(void)pr_err("[%s] prev. flex io exits (size %u)",
				__func__, vdata->flx_list[index]->fx_size);
	}

	V2_FLEXIO_ASSIGN(vdata->flx_list[index], numElements);
	flo = vdata->flx_list[index];
	if (flo == NULL) {
		(void)pr_err("[%s] failed to kzalloc flo (size %zu)", __func__, numElements);
		return;
	}

	seq_handle = &vdata->gsJpuDecSeqHeader_Info;
	seq_info = &seq_handle->gsJpuDecInitialInfo;

	/* ==== convert v1 basic field  ==== */
	/* [V2:KO:DECSEQ:F:00] */
	V2_FLEXOP_SET(flo, DECSEQ_ERRNO, seq_info->m_iErrorReason);

	/* [V2:KO:DECSEQ:F:01|02] picture resolution */
	(void)pr_info("%s: %d x %d", __func__,
			seq_info->m_iPicWidth, seq_info->m_iPicHeight);

	V2_FLEXOP_SET(flo, DECSEQ_PIC_W, seq_info->m_iPicWidth);
	V2_FLEXOP_SET(flo, DECSEQ_PIC_H, seq_info->m_iPicHeight);

	/* [V2:KO:DECSEQ:F:04] */
	V2_FLEXOP_SET(flo, DECSEQ_FRM_N, seq_info->m_iMinFrameBufferCount);

	/* [V2:KO:DECSEQ:F:05] */
	V2_FLEXOP_SET(flo, DECSEQ_FRM_X, 0); // [FIXME] consider m_iSourceFormat in csi

	/* [V2:KO:DECSEQ:F:21] fb element size */
	V2_FLEXOP_SET(flo, DECSEQ_FRM_SZ, seq_info->m_iMinFrameBufferSize[0]);

	/* [V2:KO:DECSEQ:F:22|23] fb start addr */
	// this will be set by 'v2jpgmgr_register_hwbuf'

	/* [V2:KO:DECSEQ:F:24|25] CSI */
	V2_FLEXOP_SET(flo, DECSEQ_CNT, V2D_CSI_FO_DECSEQ_JPG_MAX);
	V2_FLEXOP_CSI_OFFSET(flo, V2D_FO_DECSEQ_CSI);

	/* [V2:KO:DECFRM:CSI:00] */
	V2_FLEXOP_CSI_SET(flo, DECSEQ_JPG_MATRIX, seq_info->m_iSourceFormat);

#if 0
	(void)printk(V2_FLXMETA_PRINTFMT(flo));
	for (index = 0; index < flo->maxfields; index++) {
		(void)pr_infoprintk("[%s][%zu] %d", __func__, index, flo->fields[index]);
	}
#endif
}

static int32_t v2jpgmgr_assign_hwbuf_fb(struct vpu_decoder_data *vdata)
{
	const struct v2hw_flex_io *fli = vdata->flx_list[V2D_FINDEX(V2D_IP_DEC_SEQDATA)];
	struct v2hw_flex_io *flo = vdata->flx_list[V2D_FINDEX(V2D_OP_DEC_SEQDATA)];

	uint32_t final_count;
	uint32_t total_size;

	uint32_t max_count = 0UL, min_count = 0UL, ext_count = 0UL;

	uint32_t min_size = 0UL;
	const uint32_t max_frame_slot = 31UL;

	int32_t res;
	MEM_ALLOC_INFO_t mai = {};

	/* [KI:DECSEQ:F:02] ext frame count */
	V2_FLEXIP_GET(fli, DECSEQ_FBEXT, ext_count);

	/* [KO:DECSEQ:F:04] min frame count */
	V2_FLEXOP_CND_GET(flo, DECSEQ_FRM_N, min_count);

	/* [KO:DECSEQ:F:20] min frame size */
	V2_FLEXOP_CND_GET(flo, DECSEQ_FRM_SZ, min_size);

	if (min_size == 0) {
		(void)pr_err("%s: min frame size is zero !!", __func__);
		return RETCODE_CODEC_EXIT;
	}

	final_count = min_count + ext_count;

	// limitation: do not exceed vpu memory capacity
	max_count = vmem_get_free_memory((vputype)vdata->gsDecType) / min_size;
	if (final_count > max_count) {
		final_count = max_count;
	}

	// limitation: do not exceed max vpu-fw slot count
	// [CHECK] all vpu fw apply this limitation ?
	if (final_count > max_frame_slot) {
		final_count = max_frame_slot;
	}

	if (min_size != 0 && final_count > UINT_MAX / min_size)
	{
		(void)pr_err("[%s:%d] overflow has occurred in totla_size", __func__, __LINE__);
		return RETCODE_CODEC_EXIT;
	} else {
		total_size = final_count * min_size;
	}
	mai.request_size = ALIGNED_BUFF(total_size, 4096u);
	mai.buffer_type = BUFFER_FRAMEBUFFER;

	(void)pr_info("[%s] count(ext: %d, min: %d, final: %d) size(min:%d, tot:%u)",
			__func__, ext_count, min_count, final_count, min_size, mai.request_size);

	res = vmem_proc_alloc_memory(vdata->gsCodecType, &mai, (vputype)vdata->gsDecType);
	if (res == 0) {
		JPU_SET_BUFFER_t *set_handle = &vdata->gsJpuDecBuffer_Info;
		jpu_dec_buffer_t *set_info = &set_handle->gsJpuDecBuffer;

		set_info->m_iFrameBufferCount = final_count;
		set_info->m_FrameBufferStartAddr[PA] = mai.phy_addr;
		set_info->m_FrameBufferStartAddr[VA] = 0u;
		set_info->m_iJPGScaleRatio = 0; // no scale

		(void)pr_info("[%s][fb] PA = %#llx count = %d, scale = %d", __func__,
				(unsigned long long)set_info->m_FrameBufferStartAddr[PA],
				set_info->m_iFrameBufferCount, set_info->m_iJPGScaleRatio);

		//
		// NOTE: u/s probably wants to know this (to dump yuv on debugging)
		//
		/* [V2:KO:DECSEQ:F:22|23] fb start addr */
		V2_FLEXOP_SET_ADDR(flo, DECSEQ_FB_PA, set_info->m_FrameBufferStartAddr[PA]);
	} else {
		res = RETCODE_FAILURE;
	}

	return res;
}

int32_t v2jpgmgr_register_hwbuf(void *args, long handle, tccfp_vpu_proc_t TccJpuDec)
{
	struct v2hw_flex_io *fli = NULL;
	int32_t ret;

	struct vpu_decoder_data *vdata;
	union { void *ptr; uint64_t off; } uniaddr;
	VPU_CAST_PT(fli, args);

	uniaddr.off = fli->v1_strt;
	vdata = (struct vpu_decoder_data *)uniaddr.ptr;
	if (vdata == NULL) {
		(void)pr_err("%s: vdata is nullified !!", __func__);
		return RETCODE_FAILURE;
	}

	ret = v2jpgmgr_assign_hwbuf_fb(vdata);
	if (ret != 0) {
		(void)pr_err("[%s] failed to call: alloc frame buffer", __func__);
	} else {
		JPU_SET_BUFFER_t *set_handle = &vdata->gsJpuDecBuffer_Info;
		jpu_dec_buffer_t *set_info = &set_handle->gsJpuDecBuffer;

		ret = TccJpuDec(JPU_DEC_REG_FRAME_BUFFER,
							 (vcodec_handle_t *)&handle,
							 (void *)set_info,
							 (void *)NULL);
		(void)pr_info("[%s] ip (reg-fb) result = %d", __func__, ret);
	}

	return ret;
}

////////////////////////////////////////////////////////////////////////////
/* flex array converter for v1 struct: VDEC_DECODE_t */
////////////////////////////////////////////////////////////////////////////
void *v2jpgmgr_unmarshal_ip_frmdata(void *args)
{
	struct v2hw_flex_io *fli = NULL;
	struct vpu_decoder_data *vdata = NULL;
	union { void *ptr; uint64_t off; } uniaddr;

	JPU_DECODE_t *frm_handle;
	jpu_dec_input_t *dec_input;

	VPU_CAST_PT(fli, args);
	if (fli == NULL) {
		(void)pr_err("%s: flex io is nullified !!", __func__);
		return NULL;
	}

	uniaddr.off = fli->v1_strt;
	vdata = (struct vpu_decoder_data *)uniaddr.ptr;
	VPU_CAST_PT(vdata, uniaddr.ptr);
	if (vdata == NULL) {
		(void)pr_err("%s: vdata is nullified !!", __func__);
		return NULL;
	}

	frm_handle = &vdata->gsJpuDecInOut_Info;
	dec_input = &frm_handle->gsJpuDecInput;

	/* reset unuset field from user-space */
	frm_handle->result = 0;

	/* -----------------------------------------------
	 * convert flex fields into legacy v1 structure
	 * -----------------------------------------------
	 */
	/* ==== set v1 basic field  ==== */
	/* [V2:KI:DECFRM:F:00|01] */
	V2_FLEXIP_GET_ADDR(fli, DECFRM_BS_PA, dec_input->m_BitstreamDataAddr[PA]);
	/* [V2:KI:DECFRM:F:02] */
	V2_FLEXIP_GET(fli, DECFRM_BS_SIZE, dec_input->m_iBitstreamDataSize);

	return (void *)frm_handle;
}

void v2jpgmgr_marshal_op_frmdata(void *args)
{
	struct v2hw_flex_io *flo;
	struct v2hw_flex_io *fli = NULL;
	struct vpu_decoder_data *vdata = NULL;
	union { void *ptr; uint64_t off; } uniaddr;
	size_t index;

	JPU_DECODE_t *frm_handle;
	jpu_dec_output_t *dec_output;
	const jpu_dec_output_info_t *info;

	VPU_CAST_PT(fli, args);
	if (fli == NULL) {
		(void)pr_err("%s: fli is nullified", __func__);
		return;
	}

	uniaddr.off = fli->v1_strt;
	VPU_CAST_PT(vdata, uniaddr.ptr);
	if (vdata == NULL) {
		(void)pr_err("%s: vdata is nullified !!", __func__);
		return;
	}

	/* assign flex array for 'frmdata' output process */
	index = V2D_FINDEX(V2D_OP_DEC_FRMDATA);
	V2_FLEXIO_ASSIGN(vdata->flx_list[index], V2D_FO_DECFRM_MAX);
	flo = vdata->flx_list[index];
	if (flo == NULL) {
		(void)pr_err("[%s] failed to kzalloc flo (size %u)", __func__, V2D_FO_DECFRM_MAX);
		return;
	}

	frm_handle = &vdata->gsJpuDecInOut_Info;
	dec_output = &frm_handle->gsJpuDecOutput;
	info = &dec_output->m_DecOutInfo;

	/* ==== convert v1 basic field  ==== */
	/* [V2:KO:DECFRM:F:22|23] Y PA of display output */
	V2_FLEXOP_SET_ADDR(flo, DECFRM_OUT_Y_PA, (uintptr_t)dec_output->m_pCurrOut[PA][Y]);
	/* [V2:KO:DECFRM:F:24|25] Y VA of display output */
	V2_FLEXOP_SET_ADDR(flo, DECFRM_OUT_Y_VA, (uintptr_t)dec_output->m_pCurrOut[VA][Y]);
	/* [V2:KO:DECFRM:F:26|27] U PA of display output */
	V2_FLEXOP_SET_ADDR(flo, DECFRM_OUT_U_PA, (uintptr_t)dec_output->m_pCurrOut[PA][U]);
	/* [V2:KO:DECFRM:F:28|29] U VA of display output */
	V2_FLEXOP_SET_ADDR(flo, DECFRM_OUT_U_VA, (uintptr_t)dec_output->m_pCurrOut[VA][U]);
	/* [V2:KO:DECFRM:F:30|31] V PA of display output */
	V2_FLEXOP_SET_ADDR(flo, DECFRM_OUT_V_PA, (uintptr_t)dec_output->m_pCurrOut[PA][V]);
	/* [V2:KO:DECFRM:F:32|33] V VA of display output */
	V2_FLEXOP_SET_ADDR(flo, DECFRM_OUT_V_VA, (uintptr_t)dec_output->m_pCurrOut[VA][V]);

	/* [V2:KO:DECFRM:F:00] */
	V2_FLEXOP_SET(flo, DECFRM_PICTY, V2_PICTYPE_I);
	/* [V2:KO:DECFRM:F:01] */
	V2_FLEXOP_SET(flo, DECFRM_OSLOT, info->m_iDispOutIdx);
	/* [V2:KO:DECFRM:F:02] */
	V2_FLEXOP_SET(flo, DECFRM_ISLOT, info->m_iDispOutIdx);
	/* [V2:KO:DECFRM:F:03] */
	V2_FLEXOP_SET(flo, DECFRM_OSTAT, info->m_iDecodingStatus);
	/* [V2:KO:DECFRM:F:04] */
	V2_FLEXOP_SET(flo, DECFRM_ISTAT, info->m_iDecodingStatus);
	/* [V2:KO:DECFRM:F:06] */
	V2_FLEXOP_SET(flo, DECFRM_ERRMB, info->m_iNumOfErrMBs);

	/* [V2:KO:DECFRM:F:11|12] frame width, height */
	V2_FLEXOP_SET(flo, DECFRM_FRM_W, info->m_iWidth);
	V2_FLEXOP_SET(flo, DECFRM_FRM_H, info->m_iHeight);

#if 0
	(void)printk(V2_FLXMETA_PRINTFMT(flo));
	for (index = 0; i < flo->maxfields; index++) {
		(void)pr_info("[%s][%zu] %#x", __func__, index, flo->fields[index]);
	}
#endif
}

#endif //#ifdef CONFIG_SUPPORT_TCC_JPU
