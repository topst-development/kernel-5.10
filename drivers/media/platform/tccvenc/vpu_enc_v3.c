/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "vpu_enc_v3.h"
#include "tccvenc_parser.h"

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/io.h>
#include <tcc_mem_ioctl.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#define MULTI_SLICES_AVC

#define ALIGNED_BUFF(buf, mul) (((unsigned long)buf + (mul - 1)) & ~(mul - 1))
#define ALIGN_LEN (4 * 1024)
#define STABILITY_GAP (512)

#define VPU_ENCODER_DEVICE "/dev/vpu_venc"

extern long vdec_poll_2(struct file *filp, int timeout_ms);

#ifdef MULTI_SLICES_AVC
#define AVC_AUD_SIZE	8
const unsigned char avcAudData[AVC_AUD_SIZE] = {0x00, 0x00, 0x00, 0x01, 0x09, 0x50, 0x00, 0x00};
//#define INSERT_AUD_HEVC
#ifdef INSERT_AUD_HEVC
#define HEVC_AUD_SIZE	7
const unsigned char hevcAudData[HEVC_AUD_SIZE] = {0x00, 0x00, 0x00, 0x01, 0x46, 0x01, 0x10};
#endif
#endif

static DEFINE_MUTEX(instance_mutex);
static unsigned int total_opened_encoder = 0;

const char* vpu_codec_string[] =
{
	"NONE",
	"AVC",
	"VC1",
	"MPEG2",
	"MPEG4",
	"H263",
	"DIVX",
	"AVS",
	"MJPG",
	"VP8",
	"MVC",
	"HEVC",
	"VP9"
};

typedef struct venc_t
{
	int venc_id;

	int codec_format;

	struct file *pfenc_fd;

	const struct file_operations *pfenc_fd_fop;

	unsigned int total_frm;

	venc_v3_init_t init_info;
	venc_v3_putheader_t putheader_info;
	venc_v3_encode_t encode_info;
	venc_v3_close_t close_info;

	unsigned int seq_header_count;

	unsigned int frame_index;
	int bitstream_buf_size;
	vpu_addr_t bitstream_buf_addr[3];

	int encoded_buf_size;
	vpu_addr_t encoded_buf_base_pos[3];
	vpu_addr_t encoded_buf_end_pos[3];
	vpu_addr_t encoded_buf_cur_pos[3];
	int keyInterval_cnt;

	int enc_aud_enable;

#ifdef MULTI_SLICES_AVC
	vpu_addr_t enc_slice_info_addr[3];
	unsigned int enc_slice_info_size;
#endif

	struct pollfd tcc_event[1];

	unsigned char *seq_backup;
	unsigned int seq_len;

	int enc_avc_idr_frame_enable;

	long long sum_of_time;
} venc_t;

static const char* codec_id_to_string(enum vpu_codec_id codec_id)
{
	const char* codec_string = NULL;
	int nb_codec = sizeof(vpu_codec_string) / sizeof(vpu_codec_string[0]);

	if((codec_id >= 0) && (codec_id < nb_codec))
	{
		codec_string = vpu_codec_string[codec_id];
	}

	return codec_string;
}

static int venc_memcpy(char *d, char *s, int size)
{
	int cnt = 0;
	//printf("In %s, [0x%08x] -> [0x%08x], size[%d]", __func__, s, d, size);
	while (size--)
	{
		*d++ = *s++;
		cnt++;
	}
	return cnt;
}
struct bs {
    unsigned char *buf;
    size_t size;
    size_t bitpos;
};


static enum vpu_return_code venc_cmd_process(venc_t *pInst, int cmd, unsigned long *args)
{
    enum vpu_return_code ret;
	int success = 0;
	int retry_cnt = 10;
	int all_retry_cnt = 3;
	struct pollfd tcc_event[1];

    if(pInst->pfenc_fd == NULL) {
		return VPU_RETCODE_FAILURE;
	}

    if((ret = pInst->pfenc_fd->f_op->unlocked_ioctl(pInst->pfenc_fd, cmd, (unsigned long)args)) < 0)
	{
		if (ret == -0x999)
		{
			tcvenc_err("VPU[%d] Invalid command(0x%x) ", pInst->venc_id, cmd);
			return VPU_RETCODE_INVALID_PARAM;
		}
		else
		{
			tcvenc_err("VPU[%d] ioctl err : cmd = 0x%x", pInst->venc_id, cmd);
		}
	}

Retry:
	while (retry_cnt > 0)
	{
        long ret_poll;
        memset(tcc_event, 0, sizeof(tcc_event));

        ret_poll = vdec_poll_2(pInst->pfenc_fd, 1000); // 1 sec

		if (ret_poll < 0)
		{
			tcvenc_err("vpu(0x%x)-retry(%d:cmd(%d)) poll error '%ld'", cmd, retry_cnt, cmd, ret_poll);
			retry_cnt--;
			continue;
		}
		else if (ret_poll == 0)
		{
			tcvenc_err("vpu(0x%x)-retry(%d:cmd(%d)) poll timeout", cmd, retry_cnt, cmd);
			retry_cnt--;
			continue;
		}
		else if (ret_poll == POLLIN) {
			success = 1;
			break;
        } 
        else if (ret_poll == POLLERR) {
			break;
		}
	}

	switch (cmd)
	{
		case VENC_V3_INIT_KERNEL:
		{
			venc_v3_init_t *init_info = (venc_v3_init_t *)args;

            if(pInst->pfenc_fd->f_op->unlocked_ioctl(pInst->pfenc_fd, VENC_V3_INIT_RESULT_KERNEL, (unsigned long)init_info) < 0){
                tcvenc_err("[VDEC-%d] pInst->pfdec_fd->f_op->unlocked_ioctl(0x%x) error!!", pInst->venc_id, VENC_V3_INIT_RESULT_KERNEL);
            }
			ret = init_info->result;
		}
		break;

		case VENC_V3_PUT_HEADER_KERNEL:
		{
			venc_v3_putheader_t *putheader_info = (venc_v3_putheader_t *)args;

            if(pInst->pfenc_fd->f_op->unlocked_ioctl(pInst->pfenc_fd, VENC_V3_PUT_HEADER_RESULT_KERNEL, (unsigned long)putheader_info) < 0){
                tcvenc_err("[VDEC-%d] pInst->pfdec_fd->f_op->unlocked_ioctl(0x%x) error!!", pInst->venc_id, VENC_V3_PUT_HEADER_RESULT_KERNEL);
            }
			ret = putheader_info->result;
		}
		break;

		case VENC_V3_ENCODE_KERNEL:
		{
			venc_v3_encode_t *encode_info = (venc_v3_encode_t *)args;

            if(pInst->pfenc_fd->f_op->unlocked_ioctl(pInst->pfenc_fd, VENC_V3_ENCODE_RESULT_KERNEL, (unsigned long)encode_info) < 0){
                tcvenc_err("[VDEC-%d] pInst->pfdec_fd->f_op->unlocked_ioctl(0x%x) error!!", pInst->venc_id, VENC_V3_ENCODE_RESULT_KERNEL);
            }
			ret = encode_info->result;
		}
		break;

		case VENC_V3_CLOSE_KERNEL:
		{
			venc_v3_close_t *close_info = (venc_v3_close_t *)args;

            if(pInst->pfenc_fd->f_op->unlocked_ioctl(pInst->pfenc_fd, VENC_V3_CLOSE_RESULT_KERNEL, (unsigned long)close_info) < 0){
                tcvenc_err("[VDEC-%d] pInst->pfdec_fd->f_op->unlocked_ioctl(0x%x) error!!", pInst->venc_id, VENC_V3_CLOSE_RESULT_KERNEL);
            }
			ret = close_info->result;
		}
		break;

		default:
			tcvenc_err("Invalid ioctl:%d", cmd);
			ret = -1;
			break;
	}


	if ((ret & 0xf000) != 0x0000)
	{ //If there is an invalid return, we skip it because this return means that vpu didn't process current command yet.
		all_retry_cnt--;
		if (all_retry_cnt > 0)
		{
			retry_cnt = 10;
			goto Retry;
		}
		else
		{
			tcvenc_err("abnormal exception!!");
		}
	}

	/* todo */
	if (!success || ((ret & 0xf000) != 0x0000))
	{ /* vpu can not start or finish its processing with unknown reason!! */
		tcvenc_err("VENC command(0x%x) didn't work properly. maybe hangup(no return(0x%x))!!", cmd, ret);

		if (ret != VPU_RETCODE_CODEC_EXIT && ret != VPU_RETCODE_MULTI_CODEC_EXIT_TIMEOUT)
		{
			//ioctl(pInst->mgr_fd, VPU_HW_RESET, (unsigned long *)NULL);
		}

		return VPU_RETCODE_CODEC_EXIT;
	}

	return ret;
}

int venc_init(venc_handle_h handle, venc_init_t* p_init)
{
	venc_t* pInst = (venc_t*)handle;
	enum vpu_return_code vpu_ret = VPU_RETCODE_SUCCESS;
	int ret = 0;
	venc_init_t *p_init_param = (venc_init_t *)p_init;

	(void)memset(&pInst->init_info, 0x00, sizeof(venc_v3_init_t));

	pInst->init_info.input.codec_id     = p_init_param->codec;
	pInst->init_info.input.pic_width    = p_init_param->pic_width;
	pInst->init_info.input.pic_height   = p_init_param->pic_height;
	pInst->init_info.input.frame_rate   = p_init_param->framerate;
	pInst->init_info.input.target_kbps  = p_init_param->bitrateKbps;
	pInst->init_info.input.key_interval = p_init_param->key_interval;
	pInst->init_info.input.enc_quality  = p_init->encoding_quality;

	switch(p_init->source_format)
	{
		case VENC_SOURCE_YUV420P:
			pInst->init_info.input.yuv_format = VPU_SOURCE_YUV420;
			pInst->init_info.input.cbcr_interleave_mode = 0;
		break;

		case VENC_SOURCE_NV12:
			pInst->init_info.input.yuv_format = VPU_SOURCE_YUV420;
			pInst->init_info.input.cbcr_interleave_mode = 1;
		break;

		case VENC_SOURCE_YUV422:
			pInst->init_info.input.yuv_format = VPU_SOURCE_YUV422;
			pInst->init_info.input.cbcr_interleave_mode = 0;
		break;

		case VENC_SOURCE_YUV444:
			pInst->init_info.input.yuv_format = VPU_SOURCE_YUV444;
			pInst->init_info.input.cbcr_interleave_mode = 0;
		break;

		case VENC_SOURCE_YUV400:
			pInst->init_info.input.yuv_format = VPU_SOURCE_YUV400;
			pInst->init_info.input.cbcr_interleave_mode = 0;
		break;

		default:
			pInst->init_info.input.yuv_format = VPU_SOURCE_YUV420;
			pInst->init_info.input.cbcr_interleave_mode = 0;
			tcvenc_dbg("set default src format YUV420");
		break;
	}

	tcvenc_dbg("[VENC-%d] set source foramt:%d, interleaved:%d", pInst->venc_id, pInst->init_info.input.yuv_format, pInst->init_info.input.cbcr_interleave_mode);


	pInst->init_info.input.use_specific_rc_option =  1;
	pInst->init_info.input.rc.avc_fast_encoding = p_init_param->avc_fast_encoding;
	pInst->init_info.input.rc.pic_qp_y = (p_init_param->initial_qp > 0) ? p_init_param->initial_qp : -1;
	pInst->init_info.input.rc.intra_mb_refresh = 0;
	pInst->init_info.input.rc.deblk_disable = p_init_param->deblk_disable;
	pInst->init_info.input.rc.deblk_alpha = 0;
	pInst->init_info.input.rc.deblk_beta = 0;
	pInst->init_info.input.rc.deblk_ch_qp_offset = 0;
	pInst->init_info.input.rc.constrained_intra = 0;
	pInst->init_info.input.rc.vbv_buffer_size = p_init_param->vbv_buffer_size;
	pInst->init_info.input.rc.search_range = 2;
	pInst->init_info.input.rc.pvm_disable = 0;
	pInst->init_info.input.rc.weight_intra_cost = 0;
	pInst->init_info.input.rc.rc_interval_mode = 1;
	pInst->init_info.input.rc.rc_interval_mbnum = 0;

	tcvenc_dbg("[VENC-%d] Init Encoding, codec:%s", pInst->venc_id, codec_id_to_string(pInst->init_info.input.codec_id));

	if(p_init_param->max_p_qp > 0)
	{
		pInst->init_info.input.rc.enc_quality_level = (p_init_param->max_p_qp << 16U);
	}
	else
	{
		pInst->init_info.input.rc.enc_quality_level = 11;
	}

	pInst->init_info.input.enc_opt_flags = 0;

#ifdef MULTI_SLICES_AVC
	if (pInst->init_info.input.codec_id == VCODEC_ID_AVC)
	{
		pInst->init_info.input.rc.slice_mode = p_init_param->slice_mode;
		pInst->init_info.input.rc.slice_size_mode = p_init_param->slice_size_mode;
		pInst->init_info.input.rc.slice_size = p_init_param->slice_size;

		if (pInst->init_info.input.rc.slice_mode == 1)
			pInst->enc_aud_enable = 1;
		else
			pInst->enc_aud_enable = 0;
	}
	else
#endif
	{
		pInst->init_info.input.rc.slice_mode = 0;
		pInst->init_info.input.rc.slice_size_mode = 0;
		pInst->init_info.input.rc.slice_size = 0;
	}
	tcvenc_dbg("SliceMode[%d] - SizeMode[%d] - %d", pInst->init_info.input.rc.slice_mode, pInst->init_info.input.rc.slice_size_mode, pInst->init_info.input.rc.slice_size);

	pInst->keyInterval_cnt = 0;

	if (pInst->init_info.input.frame_rate < 1)
	{
		tcvenc_dbg("[VENC-%d] Set framerate into minimum value (1). The encoded bitrate value will not match the target value", pInst->venc_id);
		pInst->init_info.input.frame_rate = 1;
	}

	tcvenc_dbg("[VENC-%d] %d x %d, %d fps, %d key-interval, %d target kbps(%s)",
		pInst->venc_id,
		pInst->init_info.input.pic_width, pInst->init_info.input.pic_height,
		pInst->init_info.input.frame_rate,
		pInst->init_info.input.key_interval,
		pInst->init_info.input.target_kbps,
		pInst->init_info.input.target_kbps == 0 ? "VBR mode" : "CBR mode");

	if (pInst->init_info.input.use_specific_rc_option == 1)
	{
		tcvenc_dbg("[VENC-%d] fast encoding %s, deblk %s, %s mode",
		pInst->venc_id,
		pInst->init_info.input.rc.avc_fast_encoding == 1 ? "enable" : "disable",
		pInst->init_info.input.rc.deblk_disable == 1 ? "enable" : "disable",
		pInst->init_info.input.rc.slice_mode == 0 ? "frame" : "slice");

		if (pInst->init_info.input.rc.pic_qp_y > 0)
		{
			tcvenc_dbg("[VENC-%d] initial qp: %d", pInst->venc_id, pInst->init_info.input.rc.pic_qp_y);
		}
	}

	if (p_init_param->enable_force_vpu_ip == 1)
	{
		pInst->init_info.input.enable_force_vpu_ip = 1;
		pInst->init_info.input.force_vpu_ip_index = p_init_param->force_vpu_ip_index;
		tcvenc_dbg("[VENC-%d] force to use of the VPU IP at the specific index:%d ", pInst->venc_id, pInst->init_info.input.force_vpu_ip_index);
	}

	vpu_ret = venc_cmd_process(pInst, VENC_V3_INIT_KERNEL, (unsigned long *)&pInst->init_info);
	if (vpu_ret != VPU_RETCODE_SUCCESS)
	{
		tcvenc_err("[VENC-%d,Err:0x%x] venc_vpu VPU_ENC_INIT failed", pInst->venc_id, ret);
		return -1;
	}
	tcvenc_dbg("[VENC-%d] venc_vpu VPU_ENC_INIT (Success)", pInst->venc_id);

	pInst->bitstream_buf_addr[VPU_PA] = pInst->init_info.output.bitstream_out[VPU_PA];
	pInst->bitstream_buf_addr[VPU_KVA] = pInst->init_info.output.bitstream_out[VPU_KVA];
	pInst->bitstream_buf_size = pInst->init_info.output.bitstream_outsize;
	pInst->encoded_buf_size = pInst->init_info.output.bitstream_outsize;

	tcvenc_dbg("[VENC-%d] bitstream_buf_addr[PA] = 0x%llx, 0x%x ", pInst->venc_id, (vpu_addr_t)pInst->bitstream_buf_addr[VPU_PA], pInst->encoded_buf_size);

	pInst->encoded_buf_base_pos[VPU_PA] = pInst->bitstream_buf_addr[VPU_PA];
	pInst->encoded_buf_base_pos[VPU_KVA] = pInst->bitstream_buf_addr[VPU_KVA];
	pInst->encoded_buf_cur_pos[VPU_PA] = pInst->encoded_buf_base_pos[VPU_PA];
	pInst->encoded_buf_cur_pos[VPU_KVA] = pInst->encoded_buf_base_pos[VPU_KVA];
	pInst->encoded_buf_end_pos[VPU_PA] = pInst->encoded_buf_base_pos[VPU_PA] + pInst->encoded_buf_size;
	pInst->encoded_buf_end_pos[VPU_KVA] = pInst->encoded_buf_base_pos[VPU_KVA] + pInst->encoded_buf_size;

	tcvenc_dbg("[VENC-%d] PA = 0x%llx, size = 0x%x!!", pInst->venc_id, pInst->encoded_buf_base_pos[VPU_PA], pInst->encoded_buf_size);
	tcvenc_dbg("VENC_V3_INIT ok ret:%d", ret);
	return ret;
}

int venc_put_seqheader(venc_handle_h handle, venc_seq_header_t* p_seqhead)
{
	venc_t* pInst = (venc_t*)handle;
	enum vpu_return_code vpu_ret = VPU_RETCODE_SUCCESS;
	int ret = 0;
	venc_seq_header_t *p_seq_param = (venc_seq_header_t *)p_seqhead;
	unsigned char *p_dest = NULL;
	int aud_size = 0;

	tcvenc_dbg("[VENC-%d] Put Header, enc_aud_enable:%d", pInst->venc_id, pInst->enc_aud_enable);

	(void)memset(&pInst->putheader_info, 0x00, sizeof(venc_v3_putheader_t));

	if (pInst->seq_backup == NULL)
	{
		pInst->seq_backup = (unsigned char *)kmalloc(ALIGNED_BUFF(100 * 1024, ALIGN_LEN), GFP_KERNEL);
		pInst->seq_len = 0;
		tcvenc_dbg("[VENC-%d] alloc seq_backup:%p", pInst->venc_id, pInst->seq_backup);
	}

	//When it is the Nth sequence header instead of the 0th, the next sequence data is appended after the preceding sequence data
	p_dest = (unsigned char *)pInst->seq_backup + pInst->seq_len;

	tcvenc_dbg("[VENC-%d] Put Header, codec:%s", pInst->venc_id, codec_id_to_string(pInst->init_info.input.codec_id));
	if (pInst->init_info.input.codec_id == VCODEC_ID_AVC)
	{
		pInst->putheader_info.header_type = VPU_HEADER_AVC_SPS | VPU_HEADER_AVC_PPS;
		pInst->putheader_info.bitstream_buffer_addr[VPU_PA] = pInst->bitstream_buf_addr[VPU_PA];
		pInst->putheader_info.bitstream_buffer_addr[VPU_KVA] = pInst->bitstream_buf_addr[VPU_KVA];
		pInst->putheader_info.bitstream_buffer_size = pInst->bitstream_buf_size;
	
		tcvenc_dbg("[VENC-%d] VENC_V3_PUT_HEADER for H.264 SPS/PPS, dest:%p, prev seq_len:%d", pInst->venc_id, p_dest, pInst->seq_len);
		vpu_ret = venc_cmd_process(pInst, VENC_V3_PUT_HEADER_KERNEL, (unsigned long *)&pInst->putheader_info);
		if (vpu_ret != VPU_RETCODE_SUCCESS)
		{
			tcvenc_err("[VENC-%d:Err:0x%x] venc_vpu VENC_V3_PUT_HEADER H.264 SPS/PPS failed", pInst->venc_id, ret);
			return -1;
		}

		{
			u8 *kva = (u8 *)pInst->bitstream_buf_addr[VPU_KVA];
			int sz  = pInst->putheader_info.bitstream_buffer_size;
			int rc;
			rc = tccvenc_h264_strip_vui_in_place_annexb(kva, &sz);
			if (rc == 0) {
				pInst->putheader_info.bitstream_buffer_size = sz;
			} else {
				tcvenc_err("[VENC-%d] strip VUI failed (rc=%d), keep original", pInst->venc_id, rc);
			}
		}

		venc_memcpy((char *)p_dest, (char *)pInst->bitstream_buf_addr[VPU_KVA], pInst->putheader_info.bitstream_buffer_size);
		pInst->seq_len += pInst->putheader_info.bitstream_buffer_size;
	}
	else if (pInst->init_info.input.codec_id == VCODEC_ID_HEVC)
	{
		pInst->putheader_info.header_type = VPU_HEADER_HEVC_VPS | VPU_HEADER_HEVC_SPS | VPU_HEADER_HEVC_PPS;
		pInst->putheader_info.bitstream_buffer_addr[VPU_PA] = pInst->bitstream_buf_addr[VPU_PA];
		pInst->putheader_info.bitstream_buffer_addr[VPU_KVA] = pInst->bitstream_buf_addr[VPU_KVA];
		pInst->putheader_info.bitstream_buffer_size = pInst->bitstream_buf_size;

		tcvenc_dbg("[VENC-%d] VENC_V3_PUT_HEADER for HEVC VPS/SPS/PPS, , dest:%p, prev seq_len:%d", pInst->venc_id, p_dest, pInst->seq_len);
		vpu_ret = venc_cmd_process(pInst, VENC_V3_PUT_HEADER_KERNEL, (unsigned long *)&pInst->putheader_info);
		if (vpu_ret != VPU_RETCODE_SUCCESS)
		{
			tcvenc_err("[VENC-%d:Err:0x%x] venc_vpu VENC_V3_PUT_HEADER HEVC VPS/SPS/PPS failed", pInst->venc_id, ret);
			return -1;
		}
		venc_memcpy((char *)p_dest, (char *)pInst->bitstream_buf_addr[VPU_KVA], pInst->putheader_info.bitstream_buffer_size + aud_size);
		pInst->seq_len += pInst->putheader_info.bitstream_buffer_size + aud_size;
	}

    p_seq_param->seq_header_out      = pInst->seq_backup;
    p_seq_param->seq_header_out_size = pInst->seq_len;

	pInst->seq_header_count++;

	tcvenc_dbg("[VENC-%d] VENC_ENC_PUT_HEADER - OK, ret:%d", pInst->venc_id, ret);

	return ret;
}

int venc_encode(venc_handle_h handle, venc_input_t* p_input, venc_output_t* p_output)
{
	venc_t* pInst = (venc_t*)handle;
	enum vpu_return_code vpu_ret = VPU_RETCODE_SUCCESS;
	int ret = 0;
	venc_input_t* p_input_param = (venc_input_t *)p_input;
	venc_output_t* p_output_param = (venc_output_t *)p_output;

	tcvenc_dbg("[VENC-%d] VENC_V3_ENCODE --", pInst->venc_id);

	(void)memset(&pInst->encode_info, 0x00, sizeof(venc_v3_encode_t));

	//pInst->encode_info.input.change_rc_param_flag = p_input_param->change_rc_param_flag;
	pInst->encode_info.input.change_target_kbps = p_input_param->change_target_bitrate_kbps;
	if(pInst->encode_info.input.change_target_kbps != 0)
	{
		pInst->encode_info.input.change_rc_param_flag |= VENC_V3_RC_FLAG_ENABLE;
	}

	pInst->encode_info.input.change_framerate = p_input_param->change_framerate;
	if(pInst->encode_info.input.change_framerate != 0)
	{
		pInst->encode_info.input.change_rc_param_flag |= VENC_V3_RC_FLAG_FRAMERATE;
	}

	if(pInst->encode_info.input.change_rc_param_flag != 0)
	{
		pInst->encode_info.input.change_rc_param_flag |= VENC_V3_RC_FLAG_ENABLE;
	}

	if (p_input_param->request_IntraFrame == 1)
	{
		pInst->encode_info.input.force_i_picture = 1; //set 1 For IDR-Type I-Frame without P-Frame!!
	}
	else
	{
		pInst->encode_info.input.force_i_picture= 0;
	}

	pInst->encode_info.input.skip_picture = 0;
	if (pInst->init_info.input.target_kbps == 0) // no rate control
	{
		pInst->encode_info.input.quant_param = 23;
	}
	else
	{
		if (p_input_param->quant_param > 0)
			pInst->encode_info.input.quant_param = p_input_param->quant_param;
		else
			pInst->encode_info.input.quant_param = 10;
	}

	if (pInst->encoded_buf_cur_pos[VPU_PA] + pInst->encoded_buf_size / 2 > pInst->encoded_buf_end_pos[VPU_PA])
	{
		pInst->encoded_buf_cur_pos[VPU_PA] = pInst->encoded_buf_base_pos[VPU_PA];
		pInst->encoded_buf_cur_pos[VPU_KVA] = pInst->encoded_buf_base_pos[VPU_KVA];
	}

	pInst->encode_info.input.bitstream_buffer_addr[VPU_PA] = (vpu_addr_t)pInst->encoded_buf_cur_pos[VPU_PA];
	pInst->encode_info.input.bitstream_buffer_addr[VPU_KVA] = (vpu_addr_t)pInst->encoded_buf_cur_pos[VPU_KVA];
	pInst->encode_info.input.bitstream_buffer_size = pInst->bitstream_buf_size;

	tcvenc_dbg("[VENC-%d] bitstream addr:0x%llx/0x%llx, size:%d", pInst->venc_id,
			pInst->encode_info.input.bitstream_buffer_addr[VPU_PA], pInst->encode_info.input.bitstream_buffer_addr[VPU_KVA], pInst->encode_info.input.bitstream_buffer_size);

	pInst->encode_info.input.pic_y_addr = (vpu_addr_t)p_input_param->input_y;
	if (pInst->init_info.input.cbcr_interleave_mode == 0)
	{
		pInst->encode_info.input.pic_cb_addr = (vpu_addr_t)p_input_param->input_crcb[0];
		pInst->encode_info.input.pic_cr_addr = (vpu_addr_t)p_input_param->input_crcb[1];
	}
	else
	{
		pInst->encode_info.input.pic_cb_addr = (vpu_addr_t)p_input_param->input_crcb[0];
	}

#ifdef MULTI_SLICES_AVC
	//H.264 AUD RBSP
	if (pInst->enc_aud_enable == 1 && pInst->frame_index > 0)
	{
		venc_memcpy((char *)pInst->encoded_buf_cur_pos[VPU_KVA], (char *)avcAudData, 8);
		pInst->encode_info.input.bitstream_buffer_addr[VPU_PA] += 8;
		pInst->encode_info.input.bitstream_buffer_size -= 8;

		tcvenc_dbg("[VENC-%d] Insert H.264 AUD, bitstream PA addr:0x%llx, size:%d", pInst->venc_id, pInst->encode_info.input.bitstream_buffer_addr[VPU_PA], pInst->encode_info.input.bitstream_buffer_size);
	}
#endif

	tcvenc_dbg("[VENC-%d] ioctl VENC_V3_ENCODE --", pInst->venc_id);
	vpu_ret = venc_cmd_process(pInst, VENC_V3_ENCODE_KERNEL, (unsigned long *)&pInst->encode_info);
	tcvenc_dbg("[VENC-%d] ioctl VENC_V3_ENCODE ++,. ret:%d", pInst->venc_id, vpu_ret);
	pInst->total_frm++;

	if (vpu_ret != VPU_RETCODE_SUCCESS)
	{
		tcvenc_err("[VENC:Err:0x%x] %d'th VPU_ENC_ENCODE failed", ret, pInst->total_frm);
		return -1;
	}

#ifdef MULTI_SLICES_AVC
	if (pInst->init_info.input.rc.slice_mode == 1)
	{
		if (pInst->enc_aud_enable == 1 && pInst->frame_index > 0)
		{
			tcvenc_dbg("[VENC-%d] enc_aud_enabled, offset -8, size +8", pInst->venc_id);
			pInst->encode_info.output.encoded_stream_addr[VPU_PA] -= 8;
			pInst->encode_info.output.encoded_stream_addr[VPU_KVA] -= 8;
			pInst->encode_info.output.encoded_stream_size += 8;
		}
	}
#endif
	// output
	if (pInst->init_info.input.codec_id == VCODEC_ID_AVC)
	{
		p_output_param->bitstream_out = (unsigned char *)pInst->encoded_buf_cur_pos[VPU_KVA];
		p_output_param->bitstream_out_size = pInst->encode_info.output.encoded_stream_size;
	}
	else
	{
		p_output_param->bitstream_out = (unsigned char *)pInst->encoded_buf_cur_pos[VPU_KVA];
		p_output_param->bitstream_out_size = pInst->encode_info.output.encoded_stream_size;
	}
	pInst->encoded_buf_cur_pos[VPU_PA] += ALIGNED_BUFF(p_output_param->bitstream_out_size + (STABILITY_GAP * 2), ALIGN_LEN);
	pInst->encoded_buf_cur_pos[VPU_KVA] += ALIGNED_BUFF(p_output_param->bitstream_out_size + (STABILITY_GAP * 2), ALIGN_LEN);

    p_output_param->pic_type = pInst->encode_info.output.pic_type;

	pInst->frame_index++;

	tcvenc_dbg("[VPU_ENC-%d] size : %d ", pInst->venc_id, p_output_param->bitstream_out_size);
	if (1)
	{
		unsigned char *ps = (unsigned char *)p_output_param->bitstream_out;
		tcvenc_dbg("[VPU_ENC-%d] size : %d "
				"0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x "
				"0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x "
				"0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x "
				"0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x "
				"0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x ",
				pInst->venc_id, p_output_param->bitstream_out_size,
				ps[0], ps[1], ps[2], ps[3], ps[4], ps[5], ps[6], ps[7], ps[8], ps[9], ps[10], ps[11], ps[12], ps[13], ps[14], ps[15],
				ps[16], ps[17], ps[18], ps[19], ps[20], ps[21], ps[22], ps[23], ps[24], ps[25], ps[26], ps[27], ps[28], ps[29], ps[30], ps[31],
				ps[32], ps[33], ps[34], ps[35], ps[36], ps[37], ps[38], ps[39], ps[40], ps[41], ps[42], ps[43], ps[44], ps[45], ps[46], ps[47],
				ps[48], ps[49], ps[50], ps[51], ps[52], ps[53], ps[54], ps[55], ps[56], ps[57], ps[58], ps[59], ps[60], ps[61], ps[62], ps[63],
				ps[64], ps[65], ps[66], ps[67], ps[68], ps[69], ps[70], ps[71], ps[72], ps[73], ps[74], ps[75], ps[76], ps[77], ps[78], ps[79]);
	}

	tcvenc_dbg("[VENC-%d] VENC_V3_ENCODE +++, ret:%d", pInst->venc_id, ret);
	return ret;
}

int venc_close(venc_handle_h handle)
{
	venc_t* pInst = (venc_t*)handle;
	enum vpu_return_code vpu_ret = VPU_RETCODE_SUCCESS;
	int ret = 0;

	tcvenc_dbg("[VENC-%d] VENC_V3_CLOSE ---", pInst->venc_id);

	(void)memset(&pInst->close_info, 0x00, sizeof(venc_v3_close_t));

	vpu_ret = venc_cmd_process(pInst, VENC_V3_CLOSE_KERNEL, (unsigned long *)&pInst->close_info);
	if (vpu_ret != VPU_RETCODE_SUCCESS)
	{
		tcvenc_err("[VENC] VPU_ENC_CLOSE failed Error code is 0x%x ", ret);
		ret = -1;
	}

	if (pInst->seq_backup != NULL)
	{
		kfree(pInst->seq_backup);
		pInst->seq_backup = NULL;
		pInst->seq_len = 0;
	}

	pInst->frame_index = 0;

	tcvenc_dbg("[VENC-%d] VENC_V3_CLOSE done, ret:%d", pInst->venc_id, ret);
	return ret;
}

venc_handle_h venc_alloc_instance(void)
{
	venc_t *pInst = NULL;
	int ret = 0;

	pInst = (venc_t *)kmalloc(sizeof(venc_t), GFP_KERNEL);
	if (pInst != NULL)
	{
		memset(pInst, 0x00, sizeof(venc_t));
        pInst->pfenc_fd = NULL;

		mutex_lock(&instance_mutex);

		tcvenc_dbg("open: %s \n", VPU_ENCODER_DEVICE);
        pInst->pfenc_fd = filp_open(VPU_ENCODER_DEVICE, O_RDWR, 0);
		if (IS_ERR(pInst->pfenc_fd))
		{
			tcvenc_err("%s open error", VPU_ENCODER_DEVICE);
			ret = -1;
		}
		else
		{
			pInst->pfenc_fd_fop = pInst->pfenc_fd->f_op;
		}

		if (ret == 0)
		{
			vpu_drv_version_t *vpu_version = kmalloc(sizeof(vpu_drv_version_t), GFP_KERNEL);
			vpu_capability_t *vpu_capability = kmalloc(sizeof(vpu_capability_t), GFP_KERNEL);

			if (!vpu_version || !vpu_capability) {
				tcvenc_err("Failed to allocate memory for vpu_version or vpu_capability");
				ret = -1;
			} else {
				memset(vpu_version, 0x00, sizeof(vpu_drv_version_t));
				ret = pInst->pfenc_fd_fop->unlocked_ioctl(pInst->pfenc_fd, VPU_V3_GET_DRV_VERSION_SYNC_KERNEL, (unsigned long)vpu_version);
				if (ret == 0)
				{
					tcvenc_dbg("VPU Driver Ver %d.%d.%d - %s", vpu_version->major, vpu_version->minor, vpu_version->revision, (char*)vpu_version->reserved);
				}
				else
				{
					tcvenc_err("VPU_V3_GET_DRV_VERSION_SYNC error, ret:%d", ret);
				}

				memset(vpu_capability, 0x00, sizeof(vpu_capability_t));
				ret = pInst->pfenc_fd_fop->unlocked_ioctl(pInst->pfenc_fd, VPU_V3_GET_CAPABILITY_SYNC_KERNEL, (unsigned long)vpu_capability);
				if (ret == 0)
				{
					pInst->venc_id = vpu_capability->drv_id;

					tcvenc_dbg("alloc Instance[%d] = %s\n", pInst->venc_id, VPU_ENCODER_DEVICE);
					total_opened_encoder++;
					tcvenc_dbg("[%d] venc_alloc_instance, opend:%d", pInst->venc_id, total_opened_encoder);
				}
			}

			if (vpu_version)
				kfree(vpu_version);
			if (vpu_capability)
				kfree(vpu_capability);
		}

		if (ret != 0)
		{
			tcvenc_err("%s open error", VPU_ENCODER_DEVICE);
			if (!IS_ERR(pInst->pfenc_fd))
				filp_close(pInst->pfenc_fd, NULL);
			kfree(pInst);
			pInst = NULL;
		}

		mutex_unlock(&instance_mutex);
	}

	return (venc_handle_h)pInst;
}

void venc_release_instance(venc_handle_h handle)
{
	venc_t* pInst = (venc_t*)handle;

	if (pInst)
	{
		mutex_lock(&instance_mutex);

		if (pInst->pfenc_fd)
		{
			if (filp_close(pInst->pfenc_fd, NULL) < 0)
			{
			   tcvenc_err("%s close error", VPU_ENCODER_DEVICE);
			}
			pInst->pfenc_fd = NULL;
		}

		kfree(pInst);
		pInst = NULL;

		if (total_opened_encoder > 0)
			total_opened_encoder--;

		tcvenc_dbg("venc_release_instance, total opended encoder:%d", total_opened_encoder);
		mutex_unlock(&instance_mutex);
	}
}