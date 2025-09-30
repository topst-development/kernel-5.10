/*
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc.
* Contact: jayhouse@telechips.com
*/

#include "vpu_comm.h"

#if defined(ENABLE_VPU_DRV_VPU_C7) || defined(ENABLE_VPU_DRV_VPU_D6)

#include "vpu_mgr.h"
#include "vpu_mgr_context.h"
#include "vpu_mgr_common.h"
#include "vpu_rm.h"
#include "vpu_devices.h"
#include "vpu_memtrace.h"
#include "vpu_c7_mgr_sys.h"
#include "vpu_c7_mgr.h"

#define dlog_vpuc7(msg...)  	V_DBG(VPU_DBG_INFO, "[VPU_C7][INFO]:" msg)
#define detail_vpuc7(msg...)  	V_DBG(VPU_DBG_DETAIL, "[VPU_C7][DETAIL]:" msg)
#define seq_vpuc7(msg...)     	V_DBG(VPU_DBG_INFO, "[VPU_C7][SEQ]:" msg)
#define err_vpuc7(msg...)      	V_DBG(VPU_DBG_ERROR, "[VPU_C7][ERR]:" msg)

#define VPU_C7_ACCESSPOINT_PATH	 "/proc/vpu"
static const int VPU_C7_NUM_OF_BITSTREAM_BUFFERS = 1;
static const int VPU_C7_BITSTREAM_SAFE_AREA_SIZE = (1024 * 1024); //400K

//to avoid potential issues caused by stack frames, parameters are stored in the heap instead of using local variables.
//This value is assigned to ip_param of each driver's vpu_drv_info_t.
typedef struct vpu_c7_dec_papam_t {
	vpu_dec_ctrl_log_status_t dec_log;
#if defined(ENABLE_VPU_FW_LOADING)
	vpu_c7_set_fw_addr_t fw_info;
#endif
	dec_init_t dec_init;
	dec_initial_info_t dec_initialInfo;
	dec_input_t seq_input;
	dec_buffer_t dec_buffer;
	dec_buffer3_t dec_buffer3;
	dec_input_t dec_input;
	dec_output_t dec_output;
	dec_ring_buffer_status_out_t dec_ringbuffer_status;
} vpu_c7_dec_papam_t;

#if defined(ENABLE_VPU_DRV_VPU_C7)
typedef struct vpu_c7_enc_papam_t {
	vpu_dec_ctrl_log_status_t enc_log;
#if defined(ENABLE_VPU_FW_LOADING)
	vpu_c7_set_fw_addr_t fw_info;
#endif
	enc_init_t enc_init;
	enc_initial_info_t enc_initialInfo;
	enc_buffer_t enc_buffer;
	enc_header_t enc_header;
	enc_input_t enc_input;
	enc_output_t enc_output;
} vpu_c7_enc_papam_t;
#endif //defined(ENABLE_VPU_DRV_VPU_C7)

typedef struct vpu_c7_private_t {

} vpu_c7_private_t;

static vpu_mgr_t *vpu_c7_mgr_ctx = INITIAL_NULL;

#if !defined(USE_ACCESS_POINT)
#if DEFINED_CONFIG_VDEC
extern int tcc_vpu_dec(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2);
#endif

#if DEFINED_CONFIG_VENC
extern int tcc_vpu_enc(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2);
#endif
#endif //!defined(USE_ACCESS_POINT)


#define AVC_AUD_SIZE	8
const unsigned char avcAudData[AVC_AUD_SIZE] = {0x00, 0x00, 0x00, 0x01, 0x09, 0x50, 0x00, 0x00};

static int tcc_vpu_dec_l(vpu_accesspoint_t *vpu_ap, int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2) VPU_NO_SANITIZE_CFI
{
	return vpu_ap->tccfp_vpu_dec(Op, pHandle, pParam1, pParam2);
}

#if defined(ENABLE_VPU_DRV_VPU_C7)
static int tcc_vpu_enc_l(vpu_accesspoint_t *vpu_ap, int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2) VPU_NO_SANITIZE_CFI {
	return vpu_ap->tccfp_vpu_enc(Op, pHandle, pParam1, pParam2);
}
#endif //defined(ENABLE_VPU_DRV_VPU_C7)

static void vmgr_c7_status_clear(unsigned int *base_addr)
{
#if defined(VPU_C5)
	vetc_reg_write(base_addr, 0x174, 0x00);
	vetc_reg_write(base_addr, 0x00C, 0x01);
#endif
}

static int vmgr_c7_internal_handler(void)
{
	int ret, ret_code = RETCODE_INTR_DETECTION_NOT_ENABLED;
	vpu_mgr_t *mgr_ctx = vpu_c7_mgr_ctx;
	unsigned long jtimeout;

	if (mgr_ctx != NULL) {
		int timeout = mgr_ctx->each_ip->internal_timeout_ms;

		if (atomic_read(&mgr_ctx->oper_intr) > 0) {
			V_DBG(VPU_DBG_INTERRUPT, "Success 1: vpu operation!!");
			ret_code = RETCODE_SUCCESS;
		} else {
			jtimeout = msecs_to_jiffies(timeout);
			if (jtimeout >= ULONG_MAX) {
				jtimeout = ULONG_MAX;
			}

			ret = wait_event_interruptible_timeout(mgr_ctx->oper_wq, atomic_read(&mgr_ctx->oper_intr) > 0, (long)jtimeout);

			if (atomic_read(&mgr_ctx->oper_intr) > 0) {
				V_DBG(VPU_DBG_INTERRUPT, "Success 2: vpu operation!!");
				ret_code = RETCODE_SUCCESS;
			} else {
				ret_code = RETCODE_CODEC_EXIT;
			}
		}

		atomic_set(&mgr_ctx->oper_intr, 0);
		vmgr_c7_status_clear(mgr_ctx->base_addr);
	}

	return ret_code;
}

static int vmgr_c7_set_pictype(vpu_drv_info_t *drv_info, dec_output_t *dec_output)
{
	int ret_type = 0;
	int ret_top = 0;
	int ret_bottom = 0;
	int pictype, fieldtype1st, fieldtype2nd;

	pictype = (dec_output->m_DecOutInfo.m_iPicType & 0x3F);

	fieldtype1st = 0;
	if (dec_output->m_DecOutInfo.m_iInterlacedFrame > 0) {
		fieldtype1st = (dec_output->m_DecOutInfo.m_iPicType >> 3) & 0x7;
	}

	fieldtype2nd = (dec_output->m_DecOutInfo.m_iPicType & 0x7);

	detail_vpuc7("m_iPictureStructure:%d, m_iInterlacedFrame:%d, m_iPicType:0x%x, first:0x%x, second:0x%x",
		dec_output->m_DecOutInfo.m_iPictureStructure, dec_output->m_DecOutInfo.m_iInterlacedFrame, dec_output->m_DecOutInfo.m_iPicType,
		fieldtype1st, fieldtype2nd);

	switch (drv_info->codec_id) {
	case VCODEC_ID_MPEG2:
	{
		if (dec_output->m_DecOutInfo.m_iInterlacedFrame > 0) {
			// FIELD_INTERLACED
			if (fieldtype1st == PIC_TYPE_I) {
				ret_top = VPU_PICTURE_I;
			} else if (fieldtype1st == PIC_TYPE_P) {
				ret_top = VPU_PICTURE_P;
			} else if (fieldtype1st == PIC_TYPE_B) {
				ret_top = VPU_PICTURE_B;
			} else {
				ret_top = VPU_PICTURE_UNKNOWN; //TOP_FIELD = D_TYPE
			}

			if (fieldtype2nd == PIC_TYPE_I) {
				ret_bottom = VPU_PICTURE_I;   //BOTTOM_FIELD = I
			} else if (fieldtype2nd == PIC_TYPE_P) {
				ret_bottom = VPU_PICTURE_P;   //BOTTOM_FIELD = P
			} else if (fieldtype2nd == PIC_TYPE_B) {
				ret_bottom = VPU_PICTURE_BI;   //BOTTOM_FIELD = BI_TYPE
			} else {
				ret_bottom = VPU_PICTURE_UNKNOWN; //BOTTOM_FIELD = D_TYPE
			}

			ret_type = (ret_top << 3) | ret_bottom;
		} else {
			if (pictype == PIC_TYPE_I) {
				ret_type = VPU_PICTURE_I;
			} else if (pictype == PIC_TYPE_P) {
				ret_type = VPU_PICTURE_P;
			} else if (pictype == PIC_TYPE_B) {
				ret_type = VPU_PICTURE_B;
			} else {
				ret_type = VPU_PICTURE_UNKNOWN; //D_TYPE
			}
		}
	}
	break;

	case VCODEC_ID_MPEG4:
	{
		if (pictype == PIC_TYPE_I) {
			ret_type = VPU_PICTURE_I;
		} else if (pictype == PIC_TYPE_P) {
			ret_type = VPU_PICTURE_P;
		} else if (pictype == PIC_TYPE_B) {
			ret_type = VPU_PICTURE_B;
		}
		/*
		else if (pictype == PIC_TYPE_B_PB) //MPEG-4 Packed PB-frame {
			ret_type = VPU_PICTURE_B_PB;
		}
		*/
		else {
			ret_type = VPU_PICTURE_UNKNOWN; //S_TYPE
		}
	}
	break;

	case VCODEC_ID_VC1:
	{
		if (dec_output->m_DecOutInfo.m_iInterlacedFrame > 0) {
			// FIELD_INTERLACED
			if (fieldtype1st == PIC_TYPE_I) {
				ret_top = VPU_PICTURE_I;   //TOP_FIELD = I
			} else if (fieldtype1st == PIC_TYPE_P) {
				ret_top = VPU_PICTURE_P;   //TOP_FIELD = P
			} else if (fieldtype1st == 2) {
				ret_top = VPU_PICTURE_BI;   //TOP_FIELD = BI_TYPE
			} else if (fieldtype1st == 3) {
				ret_top = VPU_PICTURE_B;   //TOP_FIELD = B_TYPE
			} else if (fieldtype1st == 4) {
				ret_top = VPU_PICTURE_SKIP;   //TOP_FIELD = SKIP_TYPE
			} else {
				ret_top = VPU_PICTURE_UNKNOWN; //TOP_FIELD = FORBIDDEN
			}

			if (fieldtype2nd == PIC_TYPE_I) {
				ret_bottom = VPU_PICTURE_I;   //BOTTOM_FIELD = I
			} else if (fieldtype2nd == PIC_TYPE_P) {
				ret_bottom = VPU_PICTURE_P;   //BOTTOM_FIELD = P
			} else if (fieldtype2nd == 2) {
				ret_bottom = VPU_PICTURE_BI;   //BOTTOM_FIELD = BI_TYPE
			} else if (fieldtype2nd == 3) {
				ret_bottom = VPU_PICTURE_B;   //BOTTOM_FIELD = B_TYPE
			} else if (fieldtype2nd == 4) {
				ret_bottom = VPU_PICTURE_SKIP;   //BOTTOM_FIELD = SKIP_TYPE
			} else {
				ret_bottom = VPU_PICTURE_UNKNOWN; //BOTTOM_FIELD = FORBIDDEN
			}

			ret_type = (ret_top << 3) | ret_bottom;
		} else {
			if (pictype == PIC_TYPE_I) {
				ret_type = VPU_PICTURE_I;
			} else if (pictype == PIC_TYPE_P) {
				ret_type = VPU_PICTURE_P;
			} else if (pictype == 2) {
				ret_type = VPU_PICTURE_BI;
			} else if (pictype == 3) {
				ret_type = VPU_PICTURE_B;
			} else if (pictype == 4) {
				ret_type = VPU_PICTURE_SKIP;
			} else {
				ret_type = VPU_PICTURE_UNKNOWN; //FORBIDDEN
			}
		}
	}
	break;

	case VCODEC_ID_AVC:
	{
		if (dec_output->m_DecOutInfo.m_iInterlacedFrame > 0) {
			int missing_field = ((dec_output->m_DecOutInfo.m_iPicType >> 16) & 0x3);
			if (missing_field == 1) {
				dlog_vpuc7("bottom, top-field missing");
			} else if (missing_field == 2) {
				dlog_vpuc7("top, bottom-field missing");
			} else if (missing_field == 3) {
				dlog_vpuc7("none, top,bottom-field missing");
			}

			// FIELD_INTERLACED
			if (fieldtype1st == PIC_TYPE_I) {
				ret_top = VPU_PICTURE_I;  //TOP_FIELD = I
			} else if (fieldtype1st == PIC_TYPE_P) {
				ret_top = VPU_PICTURE_P;  //TOP_FIELD = P
			} else if (fieldtype1st == 2) {
				ret_top = VPU_PICTURE_B;   //TOP_FIELD = B_TYPE
			} else {
				ret_top = VPU_PICTURE_UNKNOWN; //TOP_FIELD = Unknown
			}

			if (fieldtype2nd == PIC_TYPE_I) {
				ret_bottom = VPU_PICTURE_I;  //BOTTOM_FIELD = I
			} else if (fieldtype2nd == PIC_TYPE_P) {
				ret_bottom = VPU_PICTURE_P;  //BOTTOM_FIELD = P
			} else if (fieldtype2nd == 2) {
				ret_bottom = VPU_PICTURE_B;   //BOTTOM_FIELD = B_TYPE
			} else {
				ret_bottom = VPU_PICTURE_UNKNOWN; //BOTTOM_FIELD = Unknown
			}

			ret_type = (ret_top << 3) | ret_bottom;
		} else {
			if (pictype == PIC_TYPE_IDR) {
				ret_type = VPU_PICTURE_IDR;
			} else if (pictype == PIC_TYPE_I) {
				ret_type = VPU_PICTURE_I;
			} else if (pictype == PIC_TYPE_P) {
				ret_type = VPU_PICTURE_P;
			} else if (pictype == 2) {
				ret_type = VPU_PICTURE_B;
			} else {
				ret_type = VPU_PICTURE_UNKNOWN; //Unknown
			}
		}
	}
	break;

	default:
	{
		if (pictype == PIC_TYPE_IDR) {
			ret_type = VPU_PICTURE_IDR;
		} else if (pictype == PIC_TYPE_I) {
			ret_type = VPU_PICTURE_I;
		} else if (pictype == PIC_TYPE_P) {
			ret_type = VPU_PICTURE_P;
		} else if (pictype == 2) {
			ret_type = VPU_PICTURE_B;
		} else {
			ret_type = VPU_PICTURE_UNKNOWN; //Unknown
		}
		break;
	}
	}

	detail_vpuc7("picture type:0x%x, top:0x%x, bottom:0x%x", ret_type, ret_top, ret_bottom);

	return ret_type;
}

static int vmgr_c7_set_output(vpu_drv_info_t *drv_info, vdec_v3_decode_out_t *arg_decode_out, dec_output_t *dec_output, vpu_pmap_alloc_info_t *alloc_info)
{
	int ret = 0;

	if ((arg_decode_out != NULL) && (dec_output != NULL) && (alloc_info != NULL)) {
		arg_decode_out->display_out[VPU_PA][VPU_COMP_Y] = dec_output->m_pDispOut[VPU_PA][0];
		arg_decode_out->display_out[VPU_PA][VPU_COMP_U] = dec_output->m_pDispOut[VPU_PA][1];
		arg_decode_out->display_out[VPU_PA][VPU_COMP_V] = dec_output->m_pDispOut[VPU_PA][2];

		arg_decode_out->display_out[VPU_KVA][VPU_COMP_Y] = dec_output->m_pDispOut[VPU_KVA][0];
		arg_decode_out->display_out[VPU_KVA][VPU_COMP_U] = dec_output->m_pDispOut[VPU_KVA][1];
		arg_decode_out->display_out[VPU_KVA][VPU_COMP_V] = dec_output->m_pDispOut[VPU_KVA][2];

		arg_decode_out->decoded_out[VPU_PA][VPU_COMP_Y] = dec_output->m_pCurrOut[VPU_PA][0];
		arg_decode_out->decoded_out[VPU_PA][VPU_COMP_U] = dec_output->m_pCurrOut[VPU_PA][1];
		arg_decode_out->decoded_out[VPU_PA][VPU_COMP_V] = dec_output->m_pCurrOut[VPU_PA][2];

		arg_decode_out->decoded_out[VPU_KVA][VPU_COMP_Y] = dec_output->m_pCurrOut[VPU_KVA][0];
		arg_decode_out->decoded_out[VPU_KVA][VPU_COMP_U] = dec_output->m_pCurrOut[VPU_KVA][1];
		arg_decode_out->decoded_out[VPU_KVA][VPU_COMP_V] = dec_output->m_pCurrOut[VPU_KVA][2];

		arg_decode_out->out_info.pic_type = vmgr_c7_set_pictype(drv_info, dec_output);

		arg_decode_out->out_info.display_idx = dec_output->m_DecOutInfo.m_iDispOutIdx;
		arg_decode_out->out_info.decoded_idx = dec_output->m_DecOutInfo.m_iDecodedIdx;
		arg_decode_out->out_info.display_status = vmgr_convert_display_status(dec_output->m_DecOutInfo.m_iOutputStatus);
		arg_decode_out->out_info.decoded_status = vmgr_convert_decoding_status(dec_output->m_DecOutInfo.m_iDecodingStatus);

		if (dec_output->m_DecOutInfo.m_iInterlacedFrame == 1) {
			arg_decode_out->out_info.interlaced_frame = 1;
		} else if (drv_info->initial_info.interlace == 1) {
			if ((drv_info->codec_id == VCODEC_ID_MPEG2) && (dec_output->m_DecOutInfo.m_iPictureStructure == 3)) {
				arg_decode_out->out_info.interlaced_frame = 1;
			} else if ((drv_info->codec_id == VCODEC_ID_AVC) && (dec_output->m_DecOutInfo.m_iPictureStructure == 1)) {
				arg_decode_out->out_info.interlaced_frame = 1;
			}
		} else {
			arg_decode_out->out_info.interlaced_frame = 0;
		}

		arg_decode_out->out_info.num_of_err_mbs = dec_output->m_DecOutInfo.m_iNumOfErrMBs;

		arg_decode_out->out_info.decoded_width = dec_output->m_DecOutInfo.m_iWidth;
		arg_decode_out->out_info.decoded_height = dec_output->m_DecOutInfo.m_iHeight;
		arg_decode_out->out_info.display_width = dec_output->m_DecOutInfo.m_iWidth;
		arg_decode_out->out_info.display_height = dec_output->m_DecOutInfo.m_iHeight;

		arg_decode_out->out_info.decoded_crop.left = dec_output->m_DecOutInfo.m_CropInfo.m_iCropLeft;
		arg_decode_out->out_info.decoded_crop.right = dec_output->m_DecOutInfo.m_CropInfo.m_iCropRight;
		arg_decode_out->out_info.decoded_crop.top = dec_output->m_DecOutInfo.m_CropInfo.m_iCropTop;
		arg_decode_out->out_info.decoded_crop.bottom = dec_output->m_DecOutInfo.m_CropInfo.m_iCropBottom;

		arg_decode_out->out_info.display_crop.left = dec_output->m_DecOutInfo.m_CropInfo.m_iCropLeft;
		arg_decode_out->out_info.display_crop.right = dec_output->m_DecOutInfo.m_CropInfo.m_iCropRight;
		arg_decode_out->out_info.display_crop.top = dec_output->m_DecOutInfo.m_CropInfo.m_iCropTop;
		arg_decode_out->out_info.display_crop.bottom = dec_output->m_DecOutInfo.m_CropInfo.m_iCropBottom;

		arg_decode_out->out_info.dma_buf_align_width = ALIGNED_BUFF(arg_decode_out->out_info.decoded_width, 16U);
		arg_decode_out->out_info.dma_buf_align_height = ALIGNED_BUFF(arg_decode_out->out_info.decoded_height, 32U);

		arg_decode_out->out_info.userdata_buf_addr[VPU_PA] = dec_output->m_DecOutInfo.m_UserDataAddress[VPU_PA];
		arg_decode_out->out_info.userdata_buf_addr[VPU_KVA] = dec_output->m_DecOutInfo.m_UserDataAddress[VPU_KVA];
		arg_decode_out->out_info.userdata_buffer_size = alloc_info->userdata_buf.size; //no information from c7

		//vdec_v3_specific_info_t specific_info;
		arg_decode_out->out_info.specific_info.m2v_field_sequence = dec_output->m_DecOutInfo.m_iM2vFieldSequence;
		arg_decode_out->out_info.specific_info.m2v_framerate = dec_output->m_DecOutInfo.m_iM2vFrameRate;
		arg_decode_out->out_info.specific_info.picture_structure = dec_output->m_DecOutInfo.m_iPictureStructure;
		arg_decode_out->out_info.specific_info.top_field_first = dec_output->m_DecOutInfo.m_iTopFieldFirst;

		detail_vpuc7("[id:%u] Decode output disp:0x%x(%x), 0x%x(%x), 0x%x(%x), decod:0x%x(%x), 0x%x(%x), 0x%x(%x), PicType:%d, disp_idx:%d, dec_idx%d, disp_stat:%d, dec_stat:%d, w:%d, h:%d, interlace_frame:%d, crop:%d,%d - %d,%d",
							drv_info->drv_id,
							arg_decode_out->display_out[VPU_PA][VPU_COMP_Y],
							arg_decode_out->display_out[VPU_KVA][VPU_COMP_Y],
							arg_decode_out->display_out[VPU_PA][VPU_COMP_U],
							arg_decode_out->display_out[VPU_KVA][VPU_COMP_U],
							arg_decode_out->display_out[VPU_PA][VPU_COMP_V],
							arg_decode_out->display_out[VPU_KVA][VPU_COMP_V],
							arg_decode_out->decoded_out[VPU_PA][VPU_COMP_Y],
							arg_decode_out->decoded_out[VPU_KVA][VPU_COMP_Y],
							arg_decode_out->decoded_out[VPU_PA][VPU_COMP_U],
							arg_decode_out->decoded_out[VPU_KVA][VPU_COMP_U],
							arg_decode_out->decoded_out[VPU_PA][VPU_COMP_V],
							arg_decode_out->decoded_out[VPU_KVA][VPU_COMP_V],
							arg_decode_out->out_info.pic_type,
							arg_decode_out->out_info.display_idx,
							arg_decode_out->out_info.decoded_idx,
							arg_decode_out->out_info.display_status,
							arg_decode_out->out_info.decoded_status,
							arg_decode_out->out_info.display_width,
							arg_decode_out->out_info.display_height,
							arg_decode_out->out_info.interlaced_frame,
							arg_decode_out->out_info.display_crop.left,
							arg_decode_out->out_info.display_crop.top,
							arg_decode_out->out_info.display_crop.right,
							arg_decode_out->out_info.display_crop.bottom);


	} else {
		ret = -1;
	}

	return ret;
}

static int vmgr_c7_dec_init(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;
	vpu_c7_dec_papam_t *ip_param = (vpu_c7_dec_papam_t *)drv_info->ip_param;

	union {
		unsigned long ul_data;
		int *pi_data;	//NULL
		unsigned char *uc_data;
	} uarg;

	codec_handle_t decHandle;
	dec_init_t *pDecInit = &ip_param->dec_init;

	vdec_v3_init_t *arg_init = (vdec_v3_init_t *)cmd_info->args;
	vdec_v3_init_in_t *arg_init_in = &arg_init->input;

	//debug settings for vpu_lib: echo 0xPXABBB > /sys/module/vpu/parameters/vdbg_lib
	unsigned int vpulib_dbg_param = get_vpu_lib_dbg_param();

	dlog_vpuc7("[id:%u] VPU_DEC_INIT start", drv_id);

	pDecInit->m_RegBaseVirtualAddr = (codec_addr_t) mgr_ctx->base_addr;
	pDecInit->m_Memcpy = (void* (*)(void *dest, const void *src, unsigned int count,	unsigned int type)) vetc_memcpy;
	pDecInit->m_Memset = (void (*)(void *ptr, int value, unsigned int num, unsigned int type)) vetc_memset;
	pDecInit->m_Interrupt = (int (*)(void))mgr_ctx->each_ip->internal_handler;
	pDecInit->m_Ioremap = (void *(*)(phys_addr_t phy_addr, unsigned int size)) vetc_ioremap;
	pDecInit->m_Iounmap = (void (*)(void *virt_addr))vetc_iounmap;
	pDecInit->m_reg_read = (unsigned int (*)(void *base_addr, unsigned int offset)) vetc_reg_read;
	pDecInit->m_reg_write = (void (*)(void *base_addr, unsigned int offset, unsigned int data)) vetc_reg_write;
	pDecInit->m_Usleep = (void (*)(unsigned int uimin, unsigned int uimax)) vetc_usleep;

	pDecInit->m_iBitstreamFormat = vmgr_get_bitstream_format(arg_init_in->codec_id, VPU_OP_TYPE_DEC); //get from vcodec_id
	pDecInit->m_bEnableUserData = arg_init_in->enable_user_data;

	dlog_vpuc7("[id:%u] bitstream format:%d, userdata:%d, output_format:%d", drv_id,
		pDecInit->m_iBitstreamFormat, arg_init_in->enable_user_data, arg_init_in->output_format);

	if (arg_init_in->output_format == VPU_OUTPUT_LINEAR_NV12) {
		pDecInit->m_bCbCrInterleaveMode = 1U;
	}

	pDecInit->m_iBitstreamBufSize = LARGE_STREAM_BUF_SIZE;
	pDecInit->m_uiDecOptFlags = 0U;
	if (arg_init_in->dec_opt_flags > 0) {
		if ((arg_init_in->dec_opt_flags & VDEC_V3_M4V_GMC_FILE_SKIP) != 0) {
			pDecInit->m_uiDecOptFlags |= M4V_GMC_FILE_SKIP;
			dlog_vpuc7("[id:%u] set M4V_GMC_FILE_SKIP", drv_id);
		}

		if ((arg_init_in->dec_opt_flags & VDEC_V3_M4V_GMC_FRAME_SKIP) != 0) {
			pDecInit->m_uiDecOptFlags |= M4V_GMC_FRAME_SKIP;
			dlog_vpuc7("[id:%u] set M4V_GMC_FRAME_SKIP", drv_id);
		}

		if ((arg_init_in->dec_opt_flags & VDEC_V3_AVC_FIELD_DISPLAY) != 0) {
			pDecInit->m_uiDecOptFlags |= AVC_FIELD_DISPLAY;
			dlog_vpuc7("[id:%u] set AVC_FIELD_DISPLAY", drv_id);
		}

		if ((arg_init_in->dec_opt_flags & VDEC_V3_MVC_DEC_ENABLE) != 0) {
			pDecInit->m_uiDecOptFlags |= MVC_DEC_ENABLE;
			dlog_vpuc7("[id:%u] set MVC_DEC_ENABLE", drv_id);
		}

		if ((arg_init_in->dec_opt_flags & VDEC_V3_USE_MAX_FRAMEBUFFER) != 0) {
			pDecInit->m_uiDecOptFlags |= (1U << 16U);
			dlog_vpuc7("[id:%u] set VDEC_USE_MAX_FRAMEBUFFER, (%d x %d)", drv_id, arg_init_in->max_support_width, arg_init_in->max_support_height);
		}

		if ((arg_init_in->dec_opt_flags & VDEC_V3_NO_BUFFER_DELAY) != 0) {
			pDecInit->m_uiDecOptFlags |= (1U << 2U);
			dlog_vpuc7("[id:%u] set VDEC_NO_BUFFER_DELAY", drv_id);
		}
	}

#if defined(ENABLE_SEQHEADER_BUFFER_CHANGE)
	//seqheader use vpu_decode_t instead of seqheader_t
	pDecInit->m_uiDecOptFlags |= (1 << 26);
#endif

#if defined(ENABLE_VPU_FW_LOADING)
	pDecInit->m_uiDecOptFlags |= (1U << 7U);
#endif

	if ((arg_init_in->enable_ringbuffer_mode == 1U) && ((mgr_ctx->each_ip->buffer_mode & VPU_BS_MODE_RINGBUFFER) != 0)) {
		vdec_v3_init_out_t *arg_init_out = &arg_init->output;
		arg_init_out->is_ringbuffer_mode = 1U; //to inform the user that the system is operating in ring buffer mode, it is set to 1U.
		pDecInit->m_iFilePlayEnable = 0;

		alloc_info->bitstream_safearea_size = VPU_C7_BITSTREAM_SAFE_AREA_SIZE; //400k
	} else {
		pDecInit->m_iFilePlayEnable = 1;
		alloc_info->bitstream_safearea_size = 0;
	}

	pDecInit->m_iPicWidth = arg_init_in->max_support_width;
	pDecInit->m_iPicHeight = arg_init_in->max_support_height;

	pDecInit->m_BitWorkAddr[PA] = alloc_info->bitwork_buf.addr[VPU_PA];
	pDecInit->m_BitWorkAddr[VA] = alloc_info->bitwork_buf.addr[VPU_KVA];

	pDecInit->m_BitstreamBufAddr[PA] = alloc_info->bitstream_buf.addr[VPU_PA];
	pDecInit->m_BitstreamBufAddr[VA] = alloc_info->bitstream_buf.addr[VPU_KVA];
	pDecInit->m_iBitstreamBufSize = alloc_info->bitstream_buf.size - alloc_info->bitstream_safearea_size;

	uarg.pi_data = NULL;
	uarg.ul_data = alloc_info->spspps_buf.addr[VPU_PA];

	pDecInit->m_pSpsPpsSaveBuffer = uarg.uc_data;
	pDecInit->m_iSpsPpsSaveBufferSize = alloc_info->spspps_buf.size;

	dlog_vpuc7("[id:%u] Init In => workbuff %#x/%#x, Reg: %#x, format : %d, "
			"Stream(%#x/%#x, %d, safearea:%d, Res: %d x %d Dec-%d: "
			"Init In => optFlag %#x, avcBuff: %#x- %d, Userdata(%d), "
			"Intereave: %d, fpmode: %d, MaxRes: %d",
			drv_id,
			pDecInit->m_BitWorkAddr[PA],
			pDecInit->m_BitWorkAddr[VA],
			pDecInit->m_RegBaseVirtualAddr,
			pDecInit->m_iBitstreamFormat,
			pDecInit->m_BitstreamBufAddr[PA],
			pDecInit->m_BitstreamBufAddr[VA],
			pDecInit->m_iBitstreamBufSize,
			alloc_info->bitstream_safearea_size,
			pDecInit->m_iPicWidth,
			pDecInit->m_iPicHeight,
			drv_id,
			pDecInit->m_uiDecOptFlags,
			pDecInit->m_pSpsPpsSaveBuffer,
			pDecInit->m_iSpsPpsSaveBufferSize,
			pDecInit->m_bEnableUserData,
			pDecInit->m_bCbCrInterleaveMode,
			pDecInit->m_iFilePlayEnable,
			pDecInit->m_iMaxResolution);

	//debug settings for vpu_lib: echo 0xPXABBB > /sys/module/vpu/parameters/vdbg_lib
	// Check if debugging is enabled using VPU_DBG_LIB_USE_CB_PRINTK (P part)
	if ((vpulib_dbg_param & VPU_DBG_LIB_USE_CB_PRINTK) == VPU_DBG_LIB_USE_CB_PRINTK) {
		unsigned int codec_ip;

		// Extract codec_ip (A part), please refer to enum vpu_ip_type
		// VPU_IP_C7(D6) = 1, VPU_IP_4KD2 = 2, VPU_IP_HEVC_ENC = 3, VPU_IP_HEVC_ENC2 = 4, VPU_IP_JPU_C6 = 5, VPU_IP_HEVC_DEC
		codec_ip = (vpulib_dbg_param & 0x00F000U) >> 12;
		V_DBG(VPU_DBG_ERROR, "[VPU_C7] codec_ip: %d", codec_ip);

		// Check if codec_ip matches desired value
		if (codec_ip == VPU_IP_C7) {
			// Extract log_mask (BBB part)
			unsigned int log_mask = (vpulib_dbg_param & 0x000FFFU);

			V_DBG(VPU_DBG_ERROR, "[VPU_C7] log_mask: %d (%x)", log_mask, log_mask);
			ip_param->dec_log.pfLogPrintCb = (void (*)(const char *, ...))vpu_printk;
			ip_param->dec_log.stLogLevel.bVerbose = (log_mask & 1U) ? 1 : 0;
			ip_param->dec_log.stLogLevel.bDebug   = (log_mask & 2U) ? 1 : 0;
			ip_param->dec_log.stLogLevel.bInfo    = (log_mask & 4U) ? 1 : 0;
			ip_param->dec_log.stLogLevel.bWarn    = (log_mask & 8U) ? 1 : 0;
			ip_param->dec_log.stLogLevel.bError   = (log_mask & 16U) ? 1 : 0;
			ip_param->dec_log.stLogLevel.bAssert  = (log_mask & 32U) ? 1 : 0;
			ip_param->dec_log.stLogLevel.bFunc    = (log_mask & 64U) ? 1 : 0;
			ip_param->dec_log.stLogLevel.bTrace   = (log_mask & 128U) ? 1 : 0;
			ret = tcc_vpu_dec_l(vpu_ap, VPU_CTRL_LOG_STATUS, NULL, (void *)(&ip_param->dec_log), (void *)NULL);
		}
	}

#if defined(ENABLE_VPU_FW_LOADING)
	if (mgr_ctx->fw_addr != 0) {
		vetc_memset(&ip_param->fw_info, 0x00, sizeof(vpu_c7_set_fw_addr_t), 0);
		ip_param->fw_info.m_FWBaseAddr = mgr_ctx->fw_addr;

		dlog_vpuc7("[id:%u] VPU_C7_SET_FW_ADDRESS addr 0x%x", drv_id, mgr_ctx->fw_addr);
		ret = tcc_vpu_dec_l(vpu_ap, VPU_C7_SET_FW_ADDRESS,
				NULL, (void *)(&ip_param->fw_info), (void *)NULL);
	}
#endif

	ret = tcc_vpu_dec_l(vpu_ap, VPU_DEC_INIT, (codec_handle_t *)&decHandle, (void *)pDecInit, (void *)NULL);
	if (ret != RETCODE_CODEC_EXIT && decHandle != 0) {
		drv_info->handle = decHandle;
	}

	dlog_vpuc7("[id:%u] VPU_DEC_INIT, ret:%d", drv_id, ret);
	return ret;
}

static int vmgr_c7_dec_seqheader(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_c7_dec_papam_t *ip_param = (vpu_c7_dec_papam_t *)drv_info->ip_param;

	dec_initial_info_t *pDecInitialInfo = &ip_param->dec_initialInfo;
	dec_input_t *pDecInput = &ip_param->seq_input;

	vdec_v3_seqheader_t *arg_seqheader = (vdec_v3_seqheader_t *)cmd_info->args;
	vdec_v3_seqheader_in_t *arg_seqheader_in = &arg_seqheader->input;
	//char *seqdata = (char *)arg_seqheader_in->bitstream_addr[VPU_KVA];

	dlog_vpuc7("[id:%u] VPU_DEC_SEQ_HEADER start, input size:%d", drv_id, arg_seqheader_in->bitstream_size);

#if defined(ENABLE_SEQHEADER_BUFFER_CHANGE)
	pDecInput->m_BitstreamDataAddr[VPU_PA] = arg_seqheader_in->bitstream_addr[VPU_PA];
	pDecInput->m_BitstreamDataAddr[VPU_KVA] = arg_seqheader_in->bitstream_addr[VPU_KVA];
	pDecInput->m_iBitstreamDataSize = arg_seqheader_in->bitstream_size;

	ret = tcc_vpu_dec_l(vpu_ap, VPU_DEC_SEQ_HEADER, (codec_handle_t *)&pHandle, (void *)pDecInput, (void *)pDecInitialInfo);
#else
	{
		union {
			unsigned int ul_data;
			int *pi_data;	//NULL
			void *pv_data;
		} uarg;

		uarg.pi_data = NULL;
		uarg.ul_data = arg_seqheader_in->bitstream_size;

		ret = tcc_vpu_dec_l(vpu_ap, VPU_DEC_SEQ_HEADER, (codec_handle_t *)&pHandle, (void *)(uarg.pv_data), (void *)pDecInitialInfo);
	}
#endif

	dlog_vpuc7("[id:%u] VPU_DEC_SEQ_HEADER ret:%d", drv_id, ret);
	if (ret == RETCODE_SUCCESS) {
		vdec_v3_initial_info_t *init_info = &arg_seqheader->output.initial_info;

		vetc_memset(init_info, 0x00, sizeof(vdec_v3_initial_info_t), 0);

		init_info->pic_width = pDecInitialInfo->m_iPicWidth;
		init_info->pic_height = pDecInitialInfo->m_iPicHeight;
		init_info->frame_rate_res = pDecInitialInfo->m_uiFrameRateRes;
		init_info->frame_rate_div = pDecInitialInfo->m_uiFrameRateDiv;
		init_info->min_frame_buffer_count = pDecInitialInfo->m_iMinFrameBufferCount;
		init_info->min_frame_buffer_size = pDecInitialInfo->m_iMinFrameBufferSize;
		init_info->frame_buffer_format = 0;

		init_info->pic_crop.left = pDecInitialInfo->m_iAvcPicCrop.m_iCropLeft;
		init_info->pic_crop.right = pDecInitialInfo->m_iAvcPicCrop.m_iCropRight;
		init_info->pic_crop.top = pDecInitialInfo->m_iAvcPicCrop.m_iCropTop;
		init_info->pic_crop.bottom = pDecInitialInfo->m_iAvcPicCrop.m_iCropBottom;

		init_info->frame_buf_delay = pDecInitialInfo->m_iFrameBufDelay;

		init_info->profile = pDecInitialInfo->m_iProfile;
		init_info->level = pDecInitialInfo->m_iLevel;
		init_info->interlace = pDecInitialInfo->m_iInterlace;
		init_info->aspectratio = pDecInitialInfo->m_iAspectRateInfo;
		init_info->report_error_reason = pDecInitialInfo->m_iReportErrorReason;
		init_info->bitdepth = 8; // vpu c7 only support 8bit output

		init_info->avc_vui_info.avc_vui_video_full_range_flag = pDecInitialInfo->m_AvcVuiInfo.m_iAvcVuiVideoFullRangeFlag;
		init_info->avc_vui_info.avc_vui_colour_primaries = pDecInitialInfo->m_AvcVuiInfo.m_iAvcVuiColourPrimaries;
		init_info->avc_vui_info.avc_vui_transfer_characteristics = pDecInitialInfo->m_AvcVuiInfo.m_iAvcVuiTransferCharacteristics;
		init_info->avc_vui_info.avc_vui_matrix_coefficients = pDecInitialInfo->m_AvcVuiInfo.m_iAvcVuiMatrixCoefficients;
		init_info->avc_vui_info.avc_vui_video_format = pDecInitialInfo->m_AvcVuiInfo.m_iAvcVuiVideoFormat;
		init_info->avc_vui_info.avc_vui_video_signal_present_flags = pDecInitialInfo->m_AvcVuiInfo.m_iAvcVuiVideoSignalPresentFlags;

		init_info->mpeg2_disp_info.mp2_color_primaries = pDecInitialInfo->m_Mp2SeqDisplayExt.m_iMp2ColorPrimaries;
		init_info->mpeg2_disp_info.mp2_transfer_characteristics = pDecInitialInfo->m_Mp2SeqDisplayExt.m_iMp2TransferCharacteristics;
		init_info->mpeg2_disp_info.mp2_matrix_coefficients = pDecInitialInfo->m_Mp2SeqDisplayExt.m_iMp2MatrixCoefficients;

#if defined(ENABLE_VPU_DRV_VPU_C7)
		init_info->mjpg_spec_info.mjpg_source_format = pDecInitialInfo->m_iMjpg_sourceFormat;
		init_info->mjpg_spec_info.mjpg_thumbnail_enable = pDecInitialInfo->m_iMjpg_ThumbnailEnable;
		init_info->mjpg_spec_info.mjpg_min_frameBufferSize[0] = pDecInitialInfo->m_iMjpg_MinFrameBufferSize[0];
		init_info->mjpg_spec_info.mjpg_min_frameBufferSize[1] = pDecInitialInfo->m_iMjpg_MinFrameBufferSize[1];
		init_info->mjpg_spec_info.mjpg_min_frameBufferSize[2] = pDecInitialInfo->m_iMjpg_MinFrameBufferSize[2];
		init_info->mjpg_spec_info.mjpg_min_frameBufferSize[3] = pDecInitialInfo->m_iMjpg_MinFrameBufferSize[3];
#endif

		vetc_memcpy(&drv_info->initial_info, init_info, sizeof(vdec_v3_initial_info_t), 0);

		dlog_vpuc7("[id:%u] VPU_DEC_SEQ_HEADER out %#x, min framebuffer cont:%d, size:%d, res info:%d - %d - %d, %d - %d - %d",
			drv_id, ret,
			init_info->min_frame_buffer_count,
			init_info->min_frame_buffer_size,
			init_info->pic_width,
			init_info->pic_crop.left,
			init_info->pic_crop.right,
			init_info->pic_height,
			init_info->pic_crop.top,
			init_info->pic_crop.bottom);
	}

	dlog_vpuc7("[id:%u] VPU_C7 VPU_DEC_SEQ_HEADER done, ret:%d", drv_id, ret);

	return ret;
}

static int vmgr_c7_dec_framebuffer(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;
	vpu_c7_dec_papam_t *ip_param = (vpu_c7_dec_papam_t *)drv_info->ip_param;

	dec_buffer_t *pDecBuffer = &ip_param->dec_buffer;

	pDecBuffer->m_FrameBufferStartAddr[VPU_PA] = alloc_info->frame_buf.addr[VPU_PA];
	pDecBuffer->m_FrameBufferStartAddr[VPU_KVA] = alloc_info->frame_buf.addr[VPU_KVA];
	pDecBuffer->m_iFrameBufferCount = alloc_info->framebuffer_count;

	pDecBuffer->m_AvcSliceSaveBufferAddr = alloc_info->slice_buf.addr[VPU_PA];
	pDecBuffer->m_iAvcSliceSaveBufferSize = alloc_info->slice_buf.size;

	pDecBuffer->m_Vp8MbDataSaveBufferAddr = alloc_info->mbdata_buf.addr[VPU_PA];
	pDecBuffer->m_iVp8MbDataSaveBufferSize = alloc_info->mbdata_buf.size;

	detail_vpuc7("[id:%u] VPU_DEC_REG_FRAME_BUFFER in :: addr:0x%x/0x%x, min frame count:%d",
		drv_id, pDecBuffer->m_FrameBufferStartAddr[VPU_PA], pDecBuffer->m_FrameBufferStartAddr[VPU_KVA], pDecBuffer->m_iFrameBufferCount);

	ret = tcc_vpu_dec_l(vpu_ap, VPU_DEC_REG_FRAME_BUFFER, (codec_handle_t *) &pHandle, (void *)pDecBuffer, (void *)NULL);
	detail_vpuc7("[id:%u] VPU_DEC_REG_FRAME_BUFFER, ret:%d", drv_id, ret);

	return ret;
}

#if defined(ENABLE_VPU_DRV_VPU_C7)
static int vmgr_c7_dec_user_framebuffer(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;
	int ii;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_c7_dec_papam_t *ip_param = (vpu_c7_dec_papam_t *)drv_info->ip_param;

	vdec_v3_reg_framebuffer_t *arg_register = (vdec_v3_reg_framebuffer_t *)cmd_info->args;
	vdec_v3_reg_framebuffer_in_t *input = &arg_register->input;
	dec_buffer3_t *pDecBuffer = &ip_param->dec_buffer3;

	pDecBuffer->m_ulFrameBufferCount = input->frame_buffer_count;
	for (ii = 0;  ii < input->frame_buffer_count; ii++) {
		pDecBuffer->m_addrFrameBuffer[PA][ii][COMP_Y] = input->frameBuffer[ii][VPU_FRAMEBUFFER_Y].framebuffer[VPU_PA];
		pDecBuffer->m_addrFrameBuffer[VA][ii][COMP_Y] = input->frameBuffer[ii][VPU_FRAMEBUFFER_Y].framebuffer[VPU_KVA];

		pDecBuffer->m_addrFrameBuffer[PA][ii][COMP_U] = input->frameBuffer[ii][VPU_FRAMEBUFFER_CB].framebuffer[VPU_PA];
		pDecBuffer->m_addrFrameBuffer[VA][ii][COMP_U] = input->frameBuffer[ii][VPU_FRAMEBUFFER_CB].framebuffer[VPU_KVA];

		pDecBuffer->m_addrFrameBuffer[PA][ii][COMP_V] = input->frameBuffer[ii][VPU_FRAMEBUFFER_CR].framebuffer[VPU_PA];
		pDecBuffer->m_addrFrameBuffer[VA][ii][COMP_V] = input->frameBuffer[ii][VPU_FRAMEBUFFER_CR].framebuffer[VPU_KVA];

		pDecBuffer->m_addrFrameBuffer[PA][ii][3] = input->frameBuffer[ii][VPU_FRAMEBUFFER_MVCOL].framebuffer[VPU_PA];
		pDecBuffer->m_addrFrameBuffer[VA][ii][3] = input->frameBuffer[ii][VPU_FRAMEBUFFER_MVCOL].framebuffer[VPU_KVA];
	}

	pDecBuffer->m_AvcSliceSaveBufferAddr = input->framebuffer_ext[VPU_FRAMEBUFFER_EXT_AVC_SLICE].framebuffer[VPU_PA];
	pDecBuffer->m_iAvcSliceSaveBufferSize = input->framebuffer_ext[VPU_FRAMEBUFFER_EXT_AVC_SLICE].size;

	pDecBuffer->m_Vp8MbDataSaveBufferAddr = input->framebuffer_ext[VPU_FRAMEBUFFER_EXT_VP8_MBDATA].framebuffer[VPU_PA];
	pDecBuffer->m_iVp8MbDataSaveBufferSize = input->framebuffer_ext[VPU_FRAMEBUFFER_EXT_VP8_MBDATA].size;

	detail_vpuc7("[id:%u] VPU_DEC_REG_FRAME_BUFFER3 in", drv_id);
	ret = tcc_vpu_dec_l(vpu_ap, VPU_DEC_REG_FRAME_BUFFER3, (codec_handle_t *) &pHandle, (void *)pDecBuffer, (void *)NULL);
	detail_vpuc7("[id:%u] VPU_DEC_REG_FRAME_BUFFER3, ret:%d", drv_id, ret);

	return ret;
}
#endif //#if defined(ENABLE_VPU_DRV_VPU_C7)

static int vmgr_c7_dec_decode(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;
	vpu_c7_dec_papam_t *ip_param = (vpu_c7_dec_papam_t *)drv_info->ip_param;

	dec_input_t *pDecInput = &ip_param->dec_input;
	dec_output_t *pDecOutput = &ip_param->dec_output;

	vdec_v3_decode_t *arg_decode = (vdec_v3_decode_t *)cmd_info->args;
	vdec_v3_decode_in_t *arg_decode_in = &arg_decode->input;
	vdec_v3_decode_out_t *arg_decode_out = &arg_decode->output;

	pDecInput->m_BitstreamDataAddr[VPU_PA] = arg_decode_in->bitstream_addr[VPU_PA];
	pDecInput->m_BitstreamDataAddr[VPU_KVA] = arg_decode_in->bitstream_addr[VPU_KVA];
	pDecInput->m_iBitstreamDataSize = arg_decode_in->bitstream_size;

	if ((drv_info->enabled_ringbuffer_mode == 1U) || ((pDecInput->m_iBitstreamDataSize > 0) && (pDecInput->m_BitstreamDataAddr[VPU_PA] != 0))) {
		if (drv_info->dec_init_info.enable_user_data == 1U) {
			pDecInput->m_UserDataAddr[VPU_PA] = alloc_info->userdata_buf.addr[VPU_PA];
			pDecInput->m_UserDataAddr[VPU_KVA] = alloc_info->userdata_buf.addr[VPU_KVA];
			pDecInput->m_iUserDataBufferSize = alloc_info->userdata_buf.size;
		}

		if (arg_decode_in->skip_mode == (int)VPU_FRAMESKIP_DISABLED) {
			pDecInput->m_iSkipFrameNum = 0;
			pDecInput->m_iFrameSearchEnable = 0;
			pDecInput->m_iSkipFrameMode = 0;
		} else if (arg_decode_in->skip_mode == (int)VPU_FRAMESKIP_NON_I) {
			pDecInput->m_iSkipFrameNum = 1;
			pDecInput->m_iFrameSearchEnable = 0x201;
			pDecInput->m_iSkipFrameMode = 0; //VDEC_SKIP_FRAME_DISABLE
			dlog_vpuc7("[id:%u] set I-frame search", drv_id);
		} else if (arg_decode_in->skip_mode == (int)VPU_FRAMESKIP_B) {
			pDecInput->m_iSkipFrameNum = 1;
			pDecInput->m_iFrameSearchEnable = 0;
			pDecInput->m_iSkipFrameMode = 2; //VDEC_SKIP_FRAME_ONLY_B
			dlog_vpuc7("[id:%u] set B-frame skip", drv_id);
		} else {
			detail_vpuc7("[id:%u] invalid skip mode", drv_id);
		}

		detail_vpuc7("[id:%u] VPU_DEC_DECODE, bitstream pos:0x%x, size:%d", drv_id, pDecInput->m_BitstreamDataAddr[0], pDecInput->m_iBitstreamDataSize);

		ret = tcc_vpu_dec_l(vpu_ap, VPU_DEC_DECODE, (codec_handle_t *)&pHandle, (void *)pDecInput, (void *)pDecOutput);
		if (ret == RETCODE_SUCCESS) {
			(void)vmgr_c7_set_output(drv_info, arg_decode_out, pDecOutput, alloc_info);
		}
	} else {
		detail_vpuc7("[id:%u] invalid stream info, addr PA:0x%x, size:%d", drv_id, pDecInput->m_BitstreamDataAddr[VPU_PA], pDecInput->m_iBitstreamDataSize);
		ret = RETCODE_INVALID_PARAM;
}

	detail_vpuc7("[id:%u] VPU_DEC_DECODE, ret:%d", drv_id, ret);
	return ret;
}

static int vmgr_c7_dec_buf_clear(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;

	vdec_v3_buf_clear_t *arg_bufclear = (vdec_v3_buf_clear_t *)cmd_info->args;

	int *arg = (int *)&arg_bufclear->index;

	detail_vpuc7("[id:%u] VPU_CMD_DEC_BUF_FLAG_CLEAR, index:%d", drv_id, *arg);
	ret = tcc_vpu_dec_l(vpu_ap, VPU_DEC_BUF_FLAG_CLEAR, (codec_handle_t *) &pHandle, (void *)(arg), (void *)NULL);
	detail_vpuc7("[id:%u] VPU_DEC_BUF_FLAG_CLEAR:%d", drv_id, ret);
	return ret;
}

static int vmgr_c7_dec_buf_flush(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;
	vpu_c7_dec_papam_t *ip_param = (vpu_c7_dec_papam_t *)drv_info->ip_param;

	int flush_frame = 0;
	dec_input_t *pDecInput = &ip_param->dec_input;
	dec_output_t *pDecOutput = &ip_param->dec_output;

	//vdec_v3_flush_t *arg_flush = (vdec_v3_flush_t *)cmd_info->args;
	dlog_vpuc7("[id:%u] VPU_CMD_DEC_FLUSH, in", drv_id);

	while (flush_frame < 32) {
		pDecInput->m_BitstreamDataAddr[VPU_PA] = alloc_info->bitstream_buf.addr[VPU_PA];
		pDecInput->m_BitstreamDataAddr[VPU_KVA] = alloc_info->bitstream_buf.addr[VPU_KVA];
		pDecInput->m_iBitstreamDataSize = 0;
		pDecInput->m_iSkipFrameMode = 0; //VDEC_SKIP_FRAME_DISABLE
		pDecInput->m_iFrameSearchEnable = 0;
		pDecInput->m_iSkipFrameNum = 0;

		ret = tcc_vpu_dec_l(vpu_ap, VPU_DEC_FLUSH_OUTPUT, (codec_handle_t *) &pHandle, (void *)pDecInput, (void *)pDecOutput);
		if (ret == RETCODE_SUCCESS) {
			if (pDecOutput->m_DecOutInfo.m_iOutputStatus == VPU_DEC_OUTPUT_SUCCESS) {
				int *arg = (int *)&pDecOutput->m_DecOutInfo.m_iDispOutIdx;

				dlog_vpuc7("[id:%u] VPU_DEC_BUF_FLAG_CLEAR %d", drv_id, pDecOutput->m_DecOutInfo.m_iDispOutIdx);
				ret = tcc_vpu_dec_l(vpu_ap, VPU_DEC_BUF_FLAG_CLEAR, (codec_handle_t *)&pHandle, (void *)(arg), (void *)NULL);
			}
		} else if (ret == RETCODE_CODEC_FINISH) {
			detail_vpuc7("[id:%u] flush done!, flush_frame:%d, ret:%d", drv_id, flush_frame, ret);
			break;
		}

		flush_frame++;
	}

	dlog_vpuc7("[id:%u] VPU_CMD_DEC_FLUSH, out, ret:%d", drv_id, ret);
	return ret;
}

static int vmgr_c7_dec_buf_drain(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;
	vpu_c7_dec_papam_t *ip_param = (vpu_c7_dec_papam_t *)drv_info->ip_param;

	int flush_frame = 0;
	dec_input_t *pDecInput = &ip_param->dec_input;
	dec_output_t *pDecOutput = &ip_param->dec_output;

	vdec_v3_drain_t *arg_drain = (vdec_v3_drain_t *)cmd_info->args;
	vdec_v3_decode_out_t *arg_decode_out = &arg_drain->output;

	dlog_vpuc7("[id:%u] VPU_CMD_DEC_DRAIN, in", drv_id);

	pDecInput->m_BitstreamDataAddr[VPU_PA] = alloc_info->bitstream_buf.addr[VPU_PA];
	pDecInput->m_BitstreamDataAddr[VPU_KVA] = alloc_info->bitstream_buf.addr[VPU_KVA];
	pDecInput->m_iBitstreamDataSize = 0;
	pDecInput->m_iSkipFrameMode = 0; //VDEC_SKIP_FRAME_DISABLE
	pDecInput->m_iFrameSearchEnable = 0;
	pDecInput->m_iSkipFrameNum = 0;

	ret = tcc_vpu_dec_l(vpu_ap, VPU_DEC_FLUSH_OUTPUT, (codec_handle_t *) &pHandle, (void *)pDecInput, (void *)pDecOutput);
	if (ret == RETCODE_SUCCESS) {
		(void)vmgr_c7_set_output(drv_info, arg_decode_out, pDecOutput, alloc_info);
	} else if (ret == RETCODE_CODEC_FINISH) {
		detail_vpuc7("[id:%u] drain done!, flush_frame:%d, ret:%d", drv_id, flush_frame, ret);
	} else {
		err_vpuc7("[id:%u] VPU_CMD_DEC_DRAIN, out, ret:%d", drv_id, ret);
	}

	dlog_vpuc7("[id:%u] VPU_CMD_DEC_DRAIN, out, ret:%d", drv_id, ret);
	return ret;
}

static int vmgr_c7_dec_buf_close(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;

	//vdec_v3_close_t *arg_close = (vdec_v3_close_t *)cmd_info->args;
	dlog_vpuc7("[id:%u] VPU_DEC_CLOSE", drv_id);
	ret = tcc_vpu_dec_l(vpu_ap, VPU_DEC_CLOSE, (codec_handle_t *) &pHandle, (void *)NULL, (void *)NULL);
	dlog_vpuc7("[id:%u] VPU_DEC_CLOSE:%d", drv_id, ret);
	return ret;
}

static int vmgr_c7_dec_ringbuffer_getinfo(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_c7_dec_papam_t *ip_param = (vpu_c7_dec_papam_t *)drv_info->ip_param;
	dec_ring_buffer_status_out_t *pDecRingbufferStatus = &ip_param->dec_ringbuffer_status;
	vdec_v3_ringbuff_get_info_t *arg_ringbuff_get = (vdec_v3_ringbuff_get_info_t *)cmd_info->args;

	detail_vpuc7("[id:%u] VPU_CMD_DEC_RING_GET_INFO, in handle:0x%x", drv_id, pHandle);

	ret = tcc_vpu_dec_l(vpu_ap, VPU_GET_RING_BUFFER_STATUS, (codec_handle_t *)&pHandle, (void *)NULL, (void *)pDecRingbufferStatus);
	if (ret == RETCODE_SUCCESS) {
		arg_ringbuff_get->available_space = pDecRingbufferStatus->m_ulAvailableSpaceInRingBuffer;
		arg_ringbuff_get->read_physical_addr = pDecRingbufferStatus->m_ptrReadAddr_PA;
		arg_ringbuff_get->write_physical_addr = pDecRingbufferStatus->m_ptrWriteAddr_PA;

		detail_vpuc7("[id:%u] VPU_CMD_DEC_RING_GET_INFO, succeed, space:%d, read_pa:0x%x, write_pa:0x%x",
			drv_id, arg_ringbuff_get->available_space, arg_ringbuff_get->read_physical_addr, arg_ringbuff_get->write_physical_addr);
	}

	return ret;
}

static int vmgr_c7_dec_ringbuffer_setinfo(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;

	union {
		int i_data;
		int *pi_data;	//NULL
		void *pv_data;
	} ucopysize, flushbuf;

	vdec_v3_ringbuff_set_info_t *arg_ringbuff_set = (vdec_v3_ringbuff_set_info_t *)cmd_info->args;

	detail_vpuc7("[id:%u] VPU_CMD_DEC_RING_SET_INFO, in, handle:0x%x, written:%d, flush:%d", drv_id, pHandle, arg_ringbuff_set->written_byte, arg_ringbuff_set->is_flush);

	ucopysize.pi_data = NULL;
	ucopysize.i_data = arg_ringbuff_set->written_byte;
	flushbuf.pi_data = NULL;
	flushbuf.i_data = arg_ringbuff_set->is_flush;

	ret = tcc_vpu_dec_l(vpu_ap, VPU_UPDATE_WRITE_BUFFER_PTR, (codec_handle_t *) &pHandle, ucopysize.pv_data, flushbuf.pv_data);
	return ret;
}

static int vmgr_c7_decode_process(void *vpu_private, enum vpu_cmd_type cmd, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)vpu_private;

	detail_vpuc7("[id:%u] %s(%d)/start mgr_ctx:%p, drv_id:%d", drv_id, vmgr_cmd_name(cmd), cmd, mgr_ctx, drv_id);

	switch (cmd) {
	case VPU_CMD_DEC_INIT:
	{
		ret = vmgr_c7_dec_init(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_DEC_SEQ_HEADER:
	{
		ret = vmgr_c7_dec_seqheader(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_DEC_REG_FRAME_BUFFER:
	{
		ret = vmgr_c7_dec_framebuffer(mgr_ctx, cmd_info, drv_info);
	}
	break;

#if defined(ENABLE_VPU_DRV_VPU_C7)
	case VPU_CMD_DEC_REG_USER_FRAME_BUFFER:
	{
		ret = vmgr_c7_dec_user_framebuffer(mgr_ctx, cmd_info, drv_info);
	}
	break;
#endif //#if defined(ENABLE_VPU_DRV_VPU_C7)

	case VPU_CMD_DEC_DECODE:
	{
		ret = vmgr_c7_dec_decode(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_DEC_BUF_FLAG_CLEAR:
	{
		ret = vmgr_c7_dec_buf_clear(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_DEC_FLUSH:
	{
		ret = vmgr_c7_dec_buf_flush(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_DEC_DRAIN:
	{
		ret = vmgr_c7_dec_buf_drain(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_DEC_CLOSE:
	{
		ret = vmgr_c7_dec_buf_close(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_DEC_RING_GET_INFO:
	{
		ret = vmgr_c7_dec_ringbuffer_getinfo(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_DEC_RING_SET_INFO:
	{
		ret = vmgr_c7_dec_ringbuffer_setinfo(mgr_ctx, cmd_info, drv_info);
	}
	break;

	default:
	{
		err_vpuc7("[id:%u] not supported command(0x%x)", drv_id, cmd);
		ret = 0x999;
	}
	} //switch (cmd)

	detail_vpuc7("[id:%u] command %s out, ret:%d", drv_id, vmgr_cmd_name(cmd), ret);
	ret = vmgr_convert_retcode(ret);
	return ret;
}

#if defined(ENABLE_VPU_DRV_VPU_C7)
static int vmgr_c7_enc_init(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;
	vpu_c7_enc_papam_t *ip_param = (vpu_c7_enc_papam_t *)drv_info->ip_param;

	codec_handle_t encHandle;
	enc_init_t *pEncInit = &ip_param->enc_init;
	enc_initial_info_t *pEncInitialInfo = &ip_param->enc_initialInfo;

	venc_v3_init_t *arg_init = (venc_v3_init_t *)cmd_info->args;
	venc_v3_init_in_t *arg_init_in = &arg_init->input;
	venc_v3_init_out_t *arg_init_out = &arg_init->output;

	//debug settings for vpu_lib: echo 0xPXABBB > /sys/module/vpu/parameters/vdbg_lib
	unsigned int vpulib_dbg_param = get_vpu_lib_dbg_param();

	dlog_vpuc7("[id:%u] VPU_ENC_INIT start", drv_id);

	pEncInit->m_RegBaseVirtualAddr = (codec_addr_t) mgr_ctx->base_addr;
	pEncInit->m_Memcpy = (void* (*)(void *dest, const void *src, unsigned int count, unsigned int type)) vetc_memcpy;
	pEncInit->m_Memset = (void (*)(void *ptr, int value, unsigned int num, unsigned int type)) vetc_memset;
	pEncInit->m_Interrupt = (int (*)(void))mgr_ctx->each_ip->internal_handler;
	pEncInit->m_Ioremap = (void *(*)(phys_addr_t phy_addr, unsigned int size)) vetc_ioremap;
	pEncInit->m_Iounmap = (void (*)(void *virt_addr))vetc_iounmap;
	pEncInit->m_reg_read = (unsigned int (*)(void *base_addr, unsigned int offset)) vetc_reg_read;
	pEncInit->m_reg_write = (void (*)(void *base_addr, unsigned int offset, unsigned int data)) vetc_reg_write;

	pEncInit->m_iBitstreamFormat = vmgr_get_bitstream_format(arg_init_in->codec_id, VPU_OP_TYPE_ENC);
	pEncInit->m_iPicWidth = arg_init_in->pic_width;
	pEncInit->m_iPicHeight = arg_init_in->pic_height;
	pEncInit->m_iFrameRate = arg_init_in->frame_rate;
	pEncInit->m_iTargetKbps = arg_init_in->target_kbps;
	pEncInit->m_iKeyInterval = arg_init_in->key_interval;
	pEncInit->m_bCbCrInterleaveMode =  arg_init_in->cbcr_interleave_mode;
	pEncInit->m_uiEncOptFlags = 0U;
	pEncInit->m_bEnableVideoCache = 0U;

#if defined(ENABLE_VPU_FW_LOADING)
	pEncInit->m_uiEncOptFlags |= (1 << 7);
#endif

	pEncInit->m_iUseSpecificRcOption = arg_init_in->use_specific_rc_option;

	if (pEncInit->m_iUseSpecificRcOption == 1) {
		pEncInit->m_stRcInit.m_iAvcFastEncoding = arg_init_in->rc.avc_fast_encoding;
		pEncInit->m_stRcInit.m_iPicQpY = arg_init_in->rc.pic_qp_y;
		pEncInit->m_stRcInit.m_iIntraMBRefresh = arg_init_in->rc.intra_mb_refresh;
		pEncInit->m_stRcInit.m_iDeblkDisable = arg_init_in->rc.deblk_disable;
		pEncInit->m_stRcInit.m_iDeblkAlpha = arg_init_in->rc.deblk_alpha;
		pEncInit->m_stRcInit.m_iDeblkBeta = arg_init_in->rc.deblk_beta;
		pEncInit->m_stRcInit.m_iDeblkChQpOffset = arg_init_in->rc.deblk_ch_qp_offset;
		pEncInit->m_stRcInit.m_iConstrainedIntra = arg_init_in->rc.constrained_intra;
		pEncInit->m_stRcInit.m_iVbvBufferSize = arg_init_in->rc.vbv_buffer_size;
		pEncInit->m_stRcInit.m_iSearchRange = arg_init_in->rc.search_range;
		pEncInit->m_stRcInit.m_iPVMDisable = arg_init_in->rc.pvm_disable;
		pEncInit->m_stRcInit.m_iWeightIntraCost = arg_init_in->rc.weight_intra_cost;
		pEncInit->m_stRcInit.m_iRCIntervalMode = arg_init_in->rc.rc_interval_mode;
		pEncInit->m_stRcInit.m_iRCIntervalMBNum = arg_init_in->rc.rc_interval_mbnum;
		pEncInit->m_stRcInit.m_iSliceMode = arg_init_in->rc.slice_mode;
		pEncInit->m_stRcInit.m_iSliceSizeMode = arg_init_in->rc.slice_size_mode;
		pEncInit->m_stRcInit.m_iSliceSize = arg_init_in->rc.slice_size;
		pEncInit->m_stRcInit.m_iEncQualityLevel = arg_init_in->rc.enc_quality_level;
		pEncInit->m_stRcInit.m_iOverrideProfileLevel = arg_init_in->rc.enc_profile_level;
	}

	pEncInit->m_BitWorkAddr[PA] = alloc_info->bitwork_buf.addr[VPU_PA];
	pEncInit->m_BitWorkAddr[VA] = alloc_info->bitwork_buf.addr[VPU_KVA];

	pEncInit->m_BitstreamBufferAddr = alloc_info->bitstream_buf.addr[VPU_PA];
	pEncInit->m_BitstreamBufferAddr_VA = alloc_info->bitstream_buf.addr[VPU_KVA];
	pEncInit->m_iBitstreamBufferSize = alloc_info->bitstream_buf.size;

	dlog_vpuc7("[id:%u] Init In => RegBase:%x, format:%d, res:%d x %d, rate:%d, %d kbps, interval:%d, crcb interleave:%d, option:0x%x",
			drv_id,
			pEncInit->m_RegBaseVirtualAddr,
			pEncInit->m_iBitstreamFormat,
			pEncInit->m_iPicWidth,
			pEncInit->m_iPicHeight,
			pEncInit->m_iFrameRate,
			pEncInit->m_iTargetKbps,
			pEncInit->m_iKeyInterval,
			pEncInit->m_bCbCrInterleaveMode,
			pEncInit->m_uiEncOptFlags);

	dlog_vpuc7("[id:%u] use rc option:%d, bitwork:%x/%x, bitstream:%x/%x size:%d",
			drv_id,
			pEncInit->m_iUseSpecificRcOption,
			pEncInit->m_BitWorkAddr[PA],
			pEncInit->m_BitWorkAddr[VA],
			pEncInit->m_BitstreamBufferAddr,
			pEncInit->m_BitstreamBufferAddr_VA,
			pEncInit->m_iBitstreamBufferSize);


	if (pEncInit->m_iUseSpecificRcOption == 1) {
		dlog_vpuc7("[id:%u] m_iAvcFastEncoding:%d", drv_id, pEncInit->m_stRcInit.m_iAvcFastEncoding);
		dlog_vpuc7("[id:%u] m_iPicQpY:%d", drv_id, pEncInit->m_stRcInit.m_iPicQpY);
		dlog_vpuc7("[id:%u] m_iIntraMBRefresh:%d", drv_id, pEncInit->m_stRcInit.m_iIntraMBRefresh);
		dlog_vpuc7("[id:%u] m_iDeblkDisable:%d", drv_id, pEncInit->m_stRcInit.m_iDeblkDisable);
		dlog_vpuc7("[id:%u] m_iDeblkAlpha:%d", drv_id, pEncInit->m_stRcInit.m_iDeblkAlpha);
		dlog_vpuc7("[id:%u] m_iDeblkBeta:%d", drv_id, pEncInit->m_stRcInit.m_iDeblkBeta);
		dlog_vpuc7("[id:%u] m_iDeblkChQpOffset:%d", drv_id, pEncInit->m_stRcInit.m_iDeblkChQpOffset);
		dlog_vpuc7("[id:%u] m_iConstrainedIntra:%d", drv_id, pEncInit->m_stRcInit.m_iConstrainedIntra);
		dlog_vpuc7("[id:%u] m_iVbvBufferSize:%d", drv_id, pEncInit->m_stRcInit.m_iVbvBufferSize);
		dlog_vpuc7("[id:%u] m_iSearchRange:%d", drv_id, pEncInit->m_stRcInit.m_iSearchRange);
		dlog_vpuc7("[id:%u] m_iPVMDisable:%d", drv_id, pEncInit->m_stRcInit.m_iPVMDisable);
		dlog_vpuc7("[id:%u] m_iWeightIntraCost:%d", drv_id, pEncInit->m_stRcInit.m_iWeightIntraCost);
		dlog_vpuc7("[id:%u] m_iRCIntervalMode:%d", drv_id, pEncInit->m_stRcInit.m_iRCIntervalMode);
		dlog_vpuc7("[id:%u] m_iRCIntervalMBNum:%d", drv_id, pEncInit->m_stRcInit.m_iRCIntervalMBNum);
		dlog_vpuc7("[id:%u] m_iSliceMode:%d", drv_id, pEncInit->m_stRcInit.m_iSliceMode);
		dlog_vpuc7("[id:%u] m_iSliceSizeMode:%d", drv_id, pEncInit->m_stRcInit.m_iSliceSizeMode);
		dlog_vpuc7("[id:%u] m_iSliceSize:%d", drv_id, pEncInit->m_stRcInit.m_iSliceSize);
		dlog_vpuc7("[id:%u] m_iEncQualityLevel:%d", drv_id, pEncInit->m_stRcInit.m_iEncQualityLevel);
	}

	//debug settings for vpu_lib: echo 0xPXABBB > /sys/module/vpu/parameters/vdbg_lib
	// Check if debugging is enabled using VPU_DBG_LIB_USE_CB_PRINTK (P part)
	if ((vpulib_dbg_param & VPU_DBG_LIB_USE_CB_PRINTK) == VPU_DBG_LIB_USE_CB_PRINTK) {
		unsigned int codec_ip;

		// Extract codec_ip (A part), please refer to enum vpu_ip_type
		// VPU_IP_C7(D6) = 1, VPU_IP_4KD2 = 2, VPU_IP_HEVC_ENC = 3, VPU_IP_HEVC_ENC2 = 4, VPU_IP_JPU_C6 = 5, VPU_IP_HEVC_DEC
		codec_ip = (vpulib_dbg_param & 0x00F000U) >> 12;
		V_DBG(VPU_DBG_ERROR, "[VPU_C7] codec_ip: %d", codec_ip);

		// Check if codec_ip matches desired value
		if (codec_ip == VPU_IP_C7) {
			// Extract log_mask (BBB part)
			unsigned int log_mask = (vpulib_dbg_param & 0x000FFFU);

			V_DBG(VPU_DBG_ERROR, "[VPU_C7] log_mask: %d (%x)", log_mask, log_mask);
			ip_param->enc_log.pfLogPrintCb = (void (*)(const char *, ...))vpu_printk;
			ip_param->enc_log.stLogLevel.bVerbose = (log_mask & 1U) ? 1 : 0;
			ip_param->enc_log.stLogLevel.bDebug   = (log_mask & 2U) ? 1 : 0;
			ip_param->enc_log.stLogLevel.bInfo    = (log_mask & 4U) ? 1 : 0;
			ip_param->enc_log.stLogLevel.bWarn    = (log_mask & 8U) ? 1 : 0;
			ip_param->enc_log.stLogLevel.bError   = (log_mask & 16U) ? 1 : 0;
			ip_param->enc_log.stLogLevel.bAssert  = (log_mask & 32U) ? 1 : 0;
			ip_param->enc_log.stLogLevel.bFunc    = (log_mask & 64U) ? 1 : 0;
			ip_param->enc_log.stLogLevel.bTrace   = (log_mask & 128U) ? 1 : 0;
			ret = tcc_vpu_enc_l(vpu_ap, VPU_CTRL_LOG_STATUS, NULL, (void *)(&ip_param->enc_log), (void *)NULL);
		}
	}

#if defined(ENABLE_VPU_FW_LOADING)
	if (mgr_ctx->fw_addr != 0) {
		vetc_memset(&ip_param->fw_info, 0x00, sizeof(vpu_c7_set_fw_addr_t), 0);
		ip_param->fw_info.m_FWBaseAddr = mgr_ctx->fw_addr;

		dlog_vpuc7("[id:%u] VPU_C7_SET_FW_ADDRESS addr 0x%x", drv_id, mgr_ctx->fw_addr);
		ret = tcc_vpu_dec_l(vpu_ap, VPU_C7_SET_FW_ADDRESS,
				NULL, (void *)(&ip_param->fw_info), (void *)NULL);
	}
#endif

	ret = tcc_vpu_enc_l(vpu_ap, VPU_ENC_INIT, (codec_handle_t *)&encHandle, (void *)pEncInit, (void *)pEncInitialInfo);
	if (ret != RETCODE_CODEC_EXIT && encHandle != 0) {
		drv_info->handle = encHandle;
		arg_init_out->min_frame_buffer_count = pEncInitialInfo->m_iMinFrameBufferCount;
		arg_init_out->min_frame_buffer_size = pEncInitialInfo->m_iMinFrameBufferSize;

		dlog_vpuc7("[id:%u] VPU_ENC_INIT ok, handle:0x%x, min framebuffer count:%d, size:%d",
			drv_id, drv_info->handle, arg_init_out->min_frame_buffer_count, arg_init_out->min_frame_buffer_size);
	} else {
		err_vpuc7("[id:%u] VPU_ENC_INIT error, ret:%d", drv_id, ret);
	}

	return ret;
}

static int vmgr_c7_enc_register_framebuffer(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;
	vpu_c7_enc_papam_t *ip_param = (vpu_c7_enc_papam_t *)drv_info->ip_param;

	enc_buffer_t *pEncBuffer = &ip_param->enc_buffer;

	pEncBuffer->m_FrameBufferStartAddr[PA] = alloc_info->frame_buf.addr[VPU_PA];
	pEncBuffer->m_FrameBufferStartAddr[VA] = alloc_info->frame_buf.addr[VPU_KVA];

	ret = tcc_vpu_enc_l(vpu_ap, VPU_ENC_REG_FRAME_BUFFER, (codec_handle_t *) &pHandle, (void *)pEncBuffer, (void *)NULL);

	return ret;
}

static int vmgr_c7_enc_put_header(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	int ii;
	int repeat_cnt = 0;
	unsigned int header_type = 0U;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_c7_enc_papam_t *ip_param = (vpu_c7_enc_papam_t *)drv_info->ip_param;

	venc_v3_putheader_t *arg_putheader = (venc_v3_putheader_t *)cmd_info->args;
	enc_header_t *pEncHeader = &ip_param->enc_header;
	unsigned char *tmpHeaderBuffer = NULL;
	int tmpHeaderSize = 0;

	dlog_vpuc7("[id:%u] VPU_ENC_PUT_HEADER In, codec id:%d, bitstream addr:%x/%x, size:%d, header type:%d",
				drv_id,
				drv_info->codec_id,
				arg_putheader->bitstream_buffer_addr[VPU_PA],
				arg_putheader->bitstream_buffer_addr[VPU_KVA],
				arg_putheader->bitstream_buffer_size,
				arg_putheader->header_type);

	tmpHeaderBuffer = VPU_alloc(512); //512 byte temp
	if (tmpHeaderBuffer != NULL) {
		header_type = arg_putheader->header_type;
		if (drv_info->codec_id == VCODEC_ID_AVC) {
			repeat_cnt = 2; // sps, pps
		} else if (drv_info->codec_id == VCODEC_ID_MPEG4) {
			repeat_cnt = 3; // vol, vos, vis
		} else {
			repeat_cnt = 0; //unknown or etc is not supported
		}

		dlog_vpuc7("[id:%u] %s header, repeat:%d", drv_id, vmgr_get_codec_name(drv_info->codec_id), repeat_cnt);
		for (ii = 0; ii < repeat_cnt; ii++) {
			dlog_vpuc7("[id:%u] [%d] %s header type : 0x%x", drv_id, ii, vmgr_get_codec_name(drv_info->codec_id), header_type);

			if (drv_info->codec_id == VCODEC_ID_AVC) {
				if ((header_type & VPU_HEADER_AVC_SPS) != 0) {
					pEncHeader->m_iHeaderType = AVC_SPS_RBSP;
					header_type &= (~VPU_HEADER_AVC_SPS);
				} else if ((header_type & VPU_HEADER_AVC_PPS) != 0) {
					pEncHeader->m_iHeaderType = AVC_PPS_RBSP;
					header_type &= (~VPU_HEADER_AVC_PPS);
				} else {
					dlog_vpuc7("[id:%u] no more header type for AVC:%d", drv_id, header_type);
					break;
				}
			} else if (drv_info->codec_id == VCODEC_ID_MPEG4) {
				if ((header_type & VPU_HEADER_MPEG4_VOL) != 0) {
					pEncHeader->m_iHeaderType = MPEG4_VOL_HEADER;
					header_type &= (~VPU_HEADER_MPEG4_VOL);
				} else if ((header_type & VPU_HEADER_MPEG4_VOS) != 0) {
					pEncHeader->m_iHeaderType = MPEG4_VOS_HEADER;
					header_type &= (~VPU_HEADER_MPEG4_VOS);
				} else if ((header_type & VPU_HEADER_MPEG4_VIS) != 0) {
					pEncHeader->m_iHeaderType = MPEG4_VIS_HEADER;
					header_type &= (~VPU_HEADER_MPEG4_VIS);
				} else {
					dlog_vpuc7("[id:%u] no more header type for MPEG4:%d", drv_id, header_type);
					break;
				}
			}

			pEncHeader->m_HeaderAddr = arg_putheader->bitstream_buffer_addr[VPU_PA];
			pEncHeader->m_HeaderAddr_VA = arg_putheader->bitstream_buffer_addr[VPU_KVA];
			pEncHeader->m_iHeaderSize = arg_putheader->bitstream_buffer_size - tmpHeaderSize;

			dlog_vpuc7("[id:%u] [%d] %s header type : 0x%x, addr:0x%x/0x%x, size:%d",
				drv_id, ii, vmgr_get_codec_name(drv_info->codec_id), pEncHeader->m_iHeaderType,
				pEncHeader->m_HeaderAddr, pEncHeader->m_HeaderAddr_VA, pEncHeader->m_iHeaderSize);

			ret = tcc_vpu_enc_l(vpu_ap, VPU_ENC_PUT_HEADER, (codec_handle_t *)&pHandle, (void *)pEncHeader, (void *)NULL);
			if (ret == RETCODE_SUCCESS) {
				vetc_memcpy(tmpHeaderBuffer + tmpHeaderSize, (void *)(pEncHeader->m_HeaderAddr_VA), pEncHeader->m_iHeaderSize, 0);
				tmpHeaderSize += pEncHeader->m_iHeaderSize;

				dlog_vpuc7("[id:%u] VPU_ENC_PUT_HEADER out, bitstream addr:%x/%x, size:%d, tmpHeaderSize:%d",
					drv_id,
					pEncHeader->m_HeaderAddr,
					pEncHeader->m_HeaderAddr_VA,
					pEncHeader->m_iHeaderSize, tmpHeaderSize);
			} else {
				err_vpuc7("[id:%u] VPU_ENC_PUT_HEADER out, failed, ret:%d", drv_id, ret);
				break;
			}
		}

		if (ret == RETCODE_SUCCESS) {
			//copy tmpHeaderbuffer to output buffer
			vetc_memcpy((void *)(arg_putheader->bitstream_buffer_addr[VPU_KVA]), tmpHeaderBuffer, tmpHeaderSize, 0);
			arg_putheader->bitstream_buffer_size = tmpHeaderSize;

			dlog_vpuc7("[id:%u] VPU_ENC_PUT_HEADER out, bitstream addr:%x/%x, size:%d",
				drv_id,
				arg_putheader->bitstream_buffer_addr[VPU_PA],
				arg_putheader->bitstream_buffer_addr[VPU_KVA],
				arg_putheader->bitstream_buffer_size);
		}

		if (tmpHeaderBuffer != NULL) {
			VPU_free(tmpHeaderBuffer);
			tmpHeaderBuffer = NULL;
		}
	} else {
		ret = -1;
	}

	dlog_vpuc7("[id:%u] VPU_ENC_PUT_HEADER done, ret:%d", drv_id, ret);
	return ret;
}

static int vmgr_c7_enc_encode(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_c7_enc_papam_t *ip_param = (vpu_c7_enc_papam_t *)drv_info->ip_param;

	venc_v3_encode_t *arg_encode = (venc_v3_encode_t *)cmd_info->args;
	venc_v3_encode_in_t *arg_init_in = &arg_encode->input;
	venc_v3_encode_out_t *arg_init_out = &arg_encode->output;

	enc_input_t *pEncInput = &ip_param->enc_input;
	enc_output_t *pEncOutput = &ip_param->enc_output;

	dlog_vpuc7("[id:%u] VPU_ENC_ENCODE in", drv_id, ret);

	dlog_vpuc7("[id:%u] encode input, pic_y_addr:0x%x, pic_cb_addr:0x%x, pic_cr_addr:0x%x", drv_id, arg_init_in->pic_y_addr, arg_init_in->pic_cb_addr, arg_init_in->pic_cr_addr);
	dlog_vpuc7("[id:%u] force_i_picture:%d, skip_picture:%d, quant_param:%d", drv_id, arg_init_in->force_i_picture, arg_init_in->skip_picture, arg_init_in->quant_param);
	dlog_vpuc7("[id:%u] bitstream_buffer_addr:0x%x/0x%x, bitstream_buffer_size:%d", drv_id, arg_init_in->bitstream_buffer_addr[VPU_PA], arg_init_in->bitstream_buffer_addr[VPU_KVA], arg_init_in->bitstream_buffer_size);
	dlog_vpuc7("[id:%u] change_rc_param_flag:%d, change_target_kbps:%d, change_framerate:%d, change_key_interval:%d", drv_id, arg_init_in->change_rc_param_flag, arg_init_in->change_target_kbps, arg_init_in->change_framerate, arg_init_in->change_key_interval);

	pEncInput->m_PicYAddr = arg_init_in->pic_y_addr;
	pEncInput->m_PicCbAddr = arg_init_in->pic_cb_addr;
	pEncInput->m_PicCrAddr = arg_init_in->pic_cr_addr;

	pEncInput->m_iForceIPicture = arg_init_in->force_i_picture;
	pEncInput->m_iSkipPicture = arg_init_in->skip_picture;
	pEncInput->m_iQuantParam = arg_init_in->quant_param;

	pEncInput->m_BitstreamBufferAddr = arg_init_in->bitstream_buffer_addr[VPU_PA];
	pEncInput->m_BitstreamBufferAddr_VA = arg_init_in->bitstream_buffer_addr[VPU_KVA];
	pEncInput->m_iBitstreamBufferSize = arg_init_in->bitstream_buffer_size;

	if ((arg_init_in->change_rc_param_flag & VENC_V3_RC_FLAG_ENABLE) != 0) {
		pEncInput->m_iChangeRcParamFlag |= 0x01;
	}

	if ((arg_init_in->change_rc_param_flag & VENC_V3_RC_FLAG_BITRATE) != 0) {
		pEncInput->m_iChangeRcParamFlag |= (0x01 << 1);
	}

	if ((arg_init_in->change_rc_param_flag & VENC_V3_RC_FLAG_FRAMERATE) != 0) {
		pEncInput->m_iChangeRcParamFlag |= (0x01 << 2);
	}

	if ((arg_init_in->change_rc_param_flag & VENC_V3_RC_FLAG_KEY_INTERVAL) != 0) {
		pEncInput->m_iChangeRcParamFlag |= (0x01 << 3);
	}

	pEncInput->m_iChangeTargetKbps = arg_init_in->change_target_kbps;
	pEncInput->m_iChangeFrameRate = arg_init_in->change_framerate;
	pEncInput->m_iChangeKeyInterval = arg_init_in->change_key_interval;

	ret = tcc_vpu_enc_l(vpu_ap, VPU_ENC_ENCODE, (codec_handle_t *)&pHandle, (void *)pEncInput, (void *)pEncOutput);
	if (ret == RETCODE_SUCCESS) {
		arg_init_out->encoded_stream_addr[VPU_PA] = pEncOutput->m_BitstreamOut[VPU_PA];
		arg_init_out->encoded_stream_addr[VPU_KVA] = pEncOutput->m_BitstreamOut[VPU_KVA];
		arg_init_out->encoded_stream_size = pEncOutput->m_iBitstreamOutSize;

		switch (pEncOutput->m_iPicType) {
		case PIC_TYPE_I:
			arg_init_out->pic_type = VPU_PICTURE_I;
		break;

		case PIC_TYPE_P:
			arg_init_out->pic_type = VPU_PICTURE_P;
		break;

		case PIC_TYPE_B:
			arg_init_out->pic_type = VPU_PICTURE_B;
		break;

		case PIC_TYPE_IDR:
			arg_init_out->pic_type = VPU_PICTURE_IDR;
		break;

		case PIC_TYPE_B_PB:
			arg_init_out->pic_type = VPU_PICTURE_B_PB;
		break;

		default:
			arg_init_out->pic_type = VPU_PICTURE_MAX;
		break;
		}

		dlog_vpuc7("[id:%u] VPU_ENC_ENCODE success, pic type:%d, addr:%x/%x, size:%d",
			drv_id, pEncOutput->m_iPicType, pEncOutput->m_BitstreamOut[VPU_PA], pEncOutput->m_BitstreamOut[VPU_KVA], pEncOutput->m_iBitstreamOutSize);
	}

	dlog_vpuc7("[id:%u] VPU_ENC_ENCODE done, ret:%d", drv_id, ret);
	return ret;
}

static int vmgr_c7_enc_close(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;
	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;

	dlog_vpuc7("[id:%u] VPU_ENC_CLOSE in", drv_id, ret);
	ret = tcc_vpu_enc_l(vpu_ap, VPU_ENC_CLOSE, (codec_handle_t *)&pHandle, (void *)NULL, (void *)NULL);

	dlog_vpuc7("[id:%u] VPU_ENC_CLOSE done, ret:%d", drv_id, ret);
	return ret;
}

static int vmgr_c7_encode_process(void *vpu_private, enum vpu_cmd_type cmd, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)vpu_private;

	detail_vpuc7("[id:%u] %s(%d)/start mgr_ctx:%p, drv_id:%d", drv_id, vmgr_cmd_name(cmd), cmd, mgr_ctx, drv_id);

	switch (cmd) {
	case VPU_CMD_ENC_INIT:
	{
		ret = vmgr_c7_enc_init(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_ENC_REG_FRAME_BUFFER:
	{
		ret = vmgr_c7_enc_register_framebuffer(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_ENC_PUT_HEADER:
	{
		ret = vmgr_c7_enc_put_header(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_ENC_ENCODE:
	{
		ret = vmgr_c7_enc_encode(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_ENC_CLOSE:
	{
		ret = vmgr_c7_enc_close(mgr_ctx, cmd_info, drv_info);
	}
	break;

	default:
	{
		err_vpuc7("[id:%u] unknown command(%d)", drv_id, cmd);
		ret = -1;
	}
	} //switch (cmd)

	ret = vmgr_convert_retcode(ret);
	return ret;
}
#endif //#if defined(ENABLE_VPU_DRV_VPU_C7)

static int vmgr_c7_get_buffer_size(void *vpu_private, enum vmgr_buffer_type buf_type, vpu_drv_info_t *drv_info)
{
	int size = 0;

	//for unused buffers, they must be set to 0.
	switch (buf_type) {
	case VMGR_BUF_BITSTREAM:
		size = ALIGNED_BUFF(LARGE_STREAM_BUF_SIZE, 4096u); //2Mb
	break;

	case VMGR_BUF_NUM_OF_BITSTREAM:
		size = VPU_C7_NUM_OF_BITSTREAM_BUFFERS;
	break;

	case VMGR_BUF_BITWORK:
		size = ALIGNED_BUFF(WORK_CODE_PARA_BUF_SIZE, 4096u);
	break;

	case VMGR_BUF_FRAMEBUF:
		size = 1; //size > 0 means, use framebuffer for enc/dec, calculating from vpu_mgr.c using min framebuffer count, size
	break;

	case VMGR_BUF_SPSPPS:
		size = ALIGNED_BUFF(PS_SAVE_SIZE, 1024u);
	break;

	case VMGR_BUF_USERDATA:
		size = ALIGNED_BUFF((50U * 1024U), 4096u);
	break;

	case VMGR_BUF_SLICE:
		size = ALIGNED_BUFF(SLICE_SAVE_SIZE, 4096u);
	break;

	case VMGR_BUF_MBDATA:
	{
		size = ALIGNED_BUFF(VP8_MB_SAVE_SIZE, 4096u);
	}
	break;

	case VMGR_BUF_MESEARCH:
	{
		size = ((drv_info->initial_info.pic_width + 15) & ~15) * 36 + 2048;
		size = ALIGNED_BUFF(size, 4096u);
	}
	break;

	case VMGR_BUF_SLICEINFO:
	{
		int mbwidth = (drv_info->initial_info.pic_width + 15) >> 4;
		int mbheight = (drv_info->initial_info.pic_height + 15) >> 4;

		size = mbwidth * mbheight * 8 + 48;
		size = ALIGNED_BUFF(size, 4096u);
	}
	break;

	//from here
	//when using the user framebuffer register,
	//it is called after the seq header initialization is complete and the result is passed to the output of the seq header.
	case VMGR_BUF_Y:
		size = ALIGNED_BUFF(drv_info->initial_info.pic_width, 16) * ALIGNED_BUFF(drv_info->initial_info.pic_height, 32);
	break;

	case VMGR_BUF_CB:
	{
		vpu_c7_dec_papam_t *ip_param = (vpu_c7_dec_papam_t *)drv_info->ip_param;
		dec_init_t *pDecInit = &ip_param->dec_init;

		if (pDecInit->m_bCbCrInterleaveMode == 1U) {
			//In the case of NV12,  CB returns the entire size of the chroma.
			size = ALIGNED_BUFF(drv_info->initial_info.pic_width, 16) * ALIGNED_BUFF(drv_info->initial_info.pic_height, 32) / 2;
		} else {
			//yuuv420
			size = ALIGNED_BUFF(drv_info->initial_info.pic_width, 16) * ALIGNED_BUFF(drv_info->initial_info.pic_height, 32) / 4;
		}
	}
	break;

	case VMGR_BUF_CR:
	{
		vpu_c7_dec_papam_t *ip_param = (vpu_c7_dec_papam_t *)drv_info->ip_param;
		dec_init_t *pDecInit = &ip_param->dec_init;

		if (pDecInit->m_bCbCrInterleaveMode == 1U) {
			//nv12
			size = 0;
		} else {
			//yuuv420
			size = ALIGNED_BUFF(drv_info->initial_info.pic_width, 16) * ALIGNED_BUFF(drv_info->initial_info.pic_height, 32) / 4;
		}
	}
	break;

	case VMGR_BUF_MVCOL:
	{
		size = ALIGNED_BUFF(drv_info->initial_info.pic_width, 16) * ALIGNED_BUFF(drv_info->initial_info.pic_height, 32) * 3 / 2;
		size = (size + 4) / 5;
		size = ALIGNED_BUFF(size, 16u) + 1024;
	}
	break;

	default:
		size = 0;
	break;
	}

	detail_vpuc7("[%s][%s][id:%u]: buf_type:%d, size:%d", vmgr_get_ip_name(drv_info->ip_type), vmgr_get_optype_name(drv_info->op_type), drv_info->drv_id, buf_type, size);
	return size;
}

static irqreturn_t vmgr_c7_isr_handler(int irq, void *vpu_private)
{
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)vpu_private;

	atomic_inc(&mgr_ctx->oper_intr);

	wake_up_interruptible(&mgr_ctx->oper_wq);
	return IRQ_HANDLED;
}

extern vmgr_clock_t vpu_c7_clock;

static vpu_ip_module_t vpu_c7_module = {
	.ip_type = VPU_IP_C7, //VPU_D6, vpu_d6 is supported as the decoder of vpu_c7
	.cq_type = VPU_CQ_LEGACY,
	.cq_depth = 1,
	.buffer_mode = (VPU_BS_MODE_RINGBUFFER | VPU_BS_MODE_LINEARBUFFR),
	.internal_timeout_ms = 200,
#if defined(ENABLE_VPU_DRV_VPU_C7)
	.enc_param_size = sizeof(vpu_c7_enc_papam_t),
#else
	.enc_param_size = 0,
#endif
	.dec_param_size = sizeof(vpu_c7_dec_papam_t),
	.internal_handler = vmgr_c7_internal_handler,
	//codec id, codec name(string), profile(string), level(string), width(unsigned int), height(unsigned int), fps(unsigned int)
	.dec_capa = {{VCODEC_ID_AVC, CODEC_NAME_AVC, "high", "4.2", 1920, 1080, 60},
				{VCODEC_ID_MPEG2, CODEC_NAME_MPEG2, "main", "high", 1920, 1080, 60},
				{VCODEC_ID_MPEG4, CODEC_NAME_MPEG4, "advanced simple", "5", 1920, 1080, 60},
				{VCODEC_ID_VC1, CODEC_NAME_VC1, "advanced", "3.0", 1920, 1080, 60},
				{VCODEC_ID_VP8, CODEC_NAME_VP8, NULL, NULL, 1920, 1080, 60},
				{VCODEC_ID_H263, CODEC_NAME_H263, "Profile3", "70", 1920, 1080, 30},
				{VCODEC_ID_MVC, CODEC_NAME_MVC, "stereo high", NULL, 1920, 1080, 60},
#if defined(ENABLE_VPU_DRV_VPU_C7)
				{VCODEC_ID_AVS, CODEC_NAME_AVS, "Jizhun", "6.2", 1920, 1080, 60},
#endif
				{VCODEC_ID_NONE, NULL, NULL, NULL, 0, 0, 0}},
#if defined(ENABLE_VPU_DRV_VPU_C7)
	.enc_capa = {{VCODEC_ID_AVC, CODEC_NAME_AVC, "baseline", "4.0", 1920, 1080, 60},
				{VCODEC_ID_H263, CODEC_NAME_H263, "Profile 3", "70", 1920, 1080, 60},
				{VCODEC_ID_MPEG4, CODEC_NAME_MPEG4, "simple", "5/6", 1920, 1080, 60},
				{VCODEC_ID_NONE, NULL, NULL, NULL, 0, 0, 0}},
#else
	//VPU_D6 (encoder is not supported)
	.enc_capa = {{VCODEC_ID_NONE, NULL, NULL, NULL, 0U, 0U, 0U}},
#endif
	.clock_ctrl = &vpu_c7_clock,
#if defined(ENABLE_VPU_DRV_VPU_C7)
	.proc_encode = vmgr_c7_encode_process,
#else
	.proc_encode = NULL,
#endif
	.proc_decode = vmgr_c7_decode_process,
	.proc_get_buffer_size = vmgr_c7_get_buffer_size,
	.isr_handler = vmgr_c7_isr_handler,
	.cq_func = NULL,
	.ip_private = NULL,
	.access_point_path = VPU_C7_ACCESSPOINT_PATH,
};

int vmgr_c7_probe(struct platform_device *pdev)
{
	int ret = 0;

	vpu_c7_private_t *ip_priv = NULL;
	vpu_mgr_t *mgr_ctx = NULL;

	ip_priv = VPU_alloc(sizeof(vpu_c7_private_t));
	if (ip_priv != NULL) {
		vpu_c7_module.ip_private = ip_priv;
	}

	mgr_ctx = vmgr_alloc(&vpu_c7_module);
	if (mgr_ctx != NULL) {
#if !defined(USE_ACCESS_POINT)
		mgr_ctx->access_point->tccfp_vpu_dec = tcc_vpu_dec;
		mgr_ctx->access_point->tccfp_vpu_enc = tcc_vpu_enc;
#else
		mgr_ctx->access_point = NULL;
#endif
		ret = vmgr_probe(mgr_ctx, pdev, MGR_NAME);
		if (ret == 0) {
			//assigning VPU manager context to avoid mutex race condition in interrupt handler
			vpu_c7_mgr_ctx = (vpu_mgr_t *)vmgr_get_context(VPU_IP_C7);
		}
	}

	platform_set_drvdata(pdev, mgr_ctx);
	return ret;
}

EXPORT_SYMBOL(vmgr_c7_probe);

VREMOVE_RET_TYPE vmgr_c7_remove(struct platform_device *pdev)
{
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)platform_get_drvdata(pdev);

	if (mgr_ctx->each_ip->ip_private != NULL) {
		VPU_free(mgr_ctx->each_ip->ip_private);
		mgr_ctx->each_ip->ip_private = NULL;
	}

	vmgr_remove(mgr_ctx, pdev);
	vmgr_free(mgr_ctx);
	vpu_c7_mgr_ctx = NULL;

	VPU_mem_usage();
	VREMOVE_RETURN();
}

EXPORT_SYMBOL(vmgr_c7_remove);

#if defined(CONFIG_PM)
int vmgr_c7_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)platform_get_drvdata(pdev);

	ret = vmgr_suspend(mgr_ctx, pdev, state);
	return ret;
}

EXPORT_SYMBOL(vmgr_c7_suspend);

int vmgr_c7_resume(struct platform_device *pdev)
{
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)platform_get_drvdata(pdev);

	vmgr_resume(mgr_ctx, pdev);
	return 0;
}

EXPORT_SYMBOL(vmgr_c7_resume);
#endif

MODULE_VERSION(VPU_V3_DRIVER_VERSION);
MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vpu c7 manager");
MODULE_LICENSE("Dual BSD/GPL");

#endif //ENABLE_VPU_DRV_VPU_C7
