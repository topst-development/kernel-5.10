/*******************************************************************************

*   Copyright (c) Telechips Inc.
*   TCC Version 1.0

This source code contains confidential information of Telechips.

Any unauthorized use without a written permission of Telechips including not
limited to re-distribution in source or binary form is strictly prohibited.

This source code is provided "AS IS" and nothing contained in this source code
shall constitute any express or implied warranty of any kind, including without
limitation, any warranty of merchantability, fitness for a particular purpose
or non-infringement of any patent, copyright or other third party intellectual
property right.
No warranty is made, express or implied, regarding the information's accuracy,
completeness, or performance.

In no event shall Telechips be liable for any claim, damages or other
liability arising from, out of or in connection with this source code or
the use in the source code.

This source code is provided subject to the terms of a Mutual Non-Disclosure
Agreement between Telechips and Company.
*
*******************************************************************************/

#include "vpu_v3/tcc_vpu_v3_ioctl.h"
#include "vpu_v3/tcc_vpu_v3_decoder.h"
#include "vpu_dec_v3.h"
#include "tccvdec_debug.h"

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

#define VPU_DECODER_DEVICE "/dev/vpu_vdec"

#define VDEC_ALIGN32(_X)             ((_X+0x1f)&~0x1f)
#define VDEC_ALIGN64(_X)             ((_X+0x3f)&~0x3f)
#define ALIGN_LEN (4*1024)

extern long vdec_poll_2(struct file *filp, int timeout_ms);

static DEFINE_MUTEX(instance_mutex);
static uint32_t total_opened_decoder = 0;

/*
static char* picture_type_name[VPU_PICTURE_MAX] =
{
	"I",
	"P",
	"B",
	"B_PB",
	"BI",
	"IDR",
	"SKIP",
	"UNKNOWN",
};

static char* get_pic_type_name(int pic_type)
{
	char* name = NULL;

	if((pic_type >= 0) && (pic_type < VPU_PICTURE_MAX))
	{
		name = picture_type_name[pic_type];
	}
	else
	{
		name = picture_type_name[VPU_PICTURE_UNKNOWN];
	}

	return name;
}

*/

typedef struct vdec_t {
    int32_t vdec_id;
    uint32_t total_frm;

	struct file *pfdec_fd;
	const struct file_operations *pfdec_fd_fop;

    int32_t bitstream_buf_size;
    int32_t userdata_buf_size;
    int32_t max_bitstream_size;
    int32_t additional_frame_count;
    int enable_user_framebuffer;
    int register_user_framebuffer;

    vpu_addr_t bitstream_buf_addr[VPU_ADDR_MAX];
    vpu_addr_t userdata_buf_addr[VPU_ADDR_MAX];
	vpu_addr_t bitstream_buf_addr_end[VPU_ADDR_MAX];

	//next input bitstream buffer information
	vpu_addr_t bitstream_buf_next[VPU_ADDR_MAX];
	int32_t bitstream_buf_next_size;

	//second next input bitstream buffer information for ring buffer mode used
	vpu_addr_t bitstream_buf_next_2[VPU_ADDR_MAX];
	int32_t bitstream_buf_next_size_2;

    int32_t frame_buf_size;
    vpu_addr_t frame_buf_addr[VPU_ADDR_MAX];

	vpu_drv_version_t vpu_version;
	vpu_capability_t vpu_capability;

	vdec_v3_init_t init_info;
	vdec_v3_seqheader_t seqenceheader_info;
	vdec_v3_reg_framebuffer_t regframebuffer_info;
	vdec_v3_decode_t decode_info;
	vdec_v3_buf_clear_t bufclear_info;
	vdec_v3_drain_t drain_info;
	vdec_v3_flush_t flush_info;
	vdec_v3_close_t close_info;

	//default 0(=fileplay), 1(=ringbuffer)
	int use_ringbuffer_mode; 
	int use_dec_profile;
	unsigned long total_elapsed_time;

	dma_addr_t addr_compressed;
	dma_addr_t addr_mvcol;
	dma_addr_t addr_ext;

	void *va_compressed;
	void *va_mvcol;
	void *va_ext;

	int32_t size_compressed;
	int32_t size_mvcol;
	int32_t size_ext;
} vdec_t;


static enum vpu_return_code vdec_cmd_process(int32_t cmd, unsigned long* args, vdec_t *pVdec)
{
    enum vpu_return_code ret;
    int32_t success = 0;
    int32_t retry_cnt = 2;
    int32_t all_retry_cnt = 3;
	struct pollfd tcc_event[1];

    vdec_t * pInst = pVdec;

	if(pInst->pfdec_fd == NULL) {
		return VPU_RETCODE_FAILURE;
	}

    if((ret = pInst->pfdec_fd->f_op->unlocked_ioctl(pInst->pfdec_fd, cmd, (unsigned long)args)) < 0)
    {
        if( ret == -0x999 )
        {
            tcvdec_err("[VDEC-%d] Invalid command(0x%x) ", pInst->vdec_id, cmd);
            return VPU_RETCODE_INVALID_PARAM;
        }
        else
        {
        	tcvdec_err("[VDEC-%d] pInst->pfdec_fd->f_op->unlocked_ioctl err[%d]: cmd = 0x%x(%d)", pInst->vdec_id, ret, cmd, cmd);
        }
    }

Retry:
    while (retry_cnt > 0) {
        memset(tcc_event, 0, sizeof(tcc_event));


        ret = vdec_poll_2(pInst->pfdec_fd, 1000); // 1 sec
        if (ret < 0) {
            tcvdec_err("[VDEC-%d] -retry(%d:cmd(%d)) poll error '%d'", pInst->vdec_id, retry_cnt, cmd, ret);
            retry_cnt--;
            continue;
        }else if (ret == 0) {
            tcvdec_err("[VDEC-%d] -retry(%d:cmd(%d)) poll timeout: %d'th frames", pInst->vdec_id, retry_cnt, cmd, pInst->total_frm);
            retry_cnt--;
            continue;
        }else if (ret == POLLIN) {
			success = 1;
			break;
        } else if (ret == POLLERR) {
			break;
		}
    }

    switch(cmd)
    {
        case VDEC_V3_INIT_KERNEL:
            {
                vdec_v3_init_t* init_info = (vdec_v3_init_t *)args;
                if(pInst->pfdec_fd->f_op->unlocked_ioctl(pInst->pfdec_fd, VDEC_V3_INIT_RESULT_KERNEL, (unsigned long)init_info) < 0){
                    tcvdec_err("[VDEC-%d] pInst->pfdec_fd->f_op->unlocked_ioctl(0x%x) error!!", pInst->vdec_id, VDEC_V3_INIT_RESULT_KERNEL);
                }
                ret = init_info->result;
            }
            break;

        case VDEC_V3_SEQ_HEADER_KERNEL:
            {
                vdec_v3_seqheader_t* seq_info = (vdec_v3_seqheader_t*)args;
                if(pInst->pfdec_fd->f_op->unlocked_ioctl(pInst->pfdec_fd, VDEC_V3_SEQ_HEADER_RESULT_KERNEL, (unsigned long)seq_info) < 0)
				{
                    tcvdec_err("[VDEC-%d] pInst->pfdec_fd->f_op->unlocked_ioctl(0x%x) error!!", pInst->vdec_id, VDEC_V3_SEQ_HEADER_RESULT_KERNEL);
                }
                ret = seq_info->result;
            }
            break;

		case VDEC_V3_DECODE_KERNEL:
            {
                vdec_v3_decode_t* dec_info = (vdec_v3_decode_t*)args;
                if(pInst->pfdec_fd->f_op->unlocked_ioctl(pInst->pfdec_fd, VDEC_V3_DECODE_RESULT_KERNEL, (unsigned long)dec_info) < 0)
				{
                    tcvdec_err("[VDEC-%d] pInst->pfdec_fd->f_op->unlocked_ioctl(0x%x) error!!", pInst->vdec_id, VDEC_V3_DECODE_RESULT_KERNEL);
                }
                ret = dec_info->result;
            }
            break;

		case VDEC_V3_BUF_CLEAR_KERNEL:
			{
				vdec_v3_buf_clear_t* bufclear_info = (vdec_v3_buf_clear_t*)args;
                if(pInst->pfdec_fd->f_op->unlocked_ioctl(pInst->pfdec_fd, VDEC_V3_BUF_CLEAR_RESULT_KERNEL, (unsigned long)bufclear_info) < 0)
				{
                    tcvdec_err("[VDEC-%d] pInst->pfdec_fd->f_op->unlocked_ioctl(0x%x) error!!", pInst->vdec_id, VDEC_V3_BUF_CLEAR_RESULT_KERNEL);
                }
                ret = bufclear_info->result;
			}
			break;

		case VDEC_V3_FLUSH_KERNEL:
			{
				vdec_v3_flush_t* flush_info = (vdec_v3_flush_t*)args;
                if(pInst->pfdec_fd->f_op->unlocked_ioctl(pInst->pfdec_fd, VDEC_V3_FLUSH_RESULT_KERNEL, (unsigned long)flush_info) < 0)
				{
                    tcvdec_err("[VDEC-%d] pInst->pfdec_fd->f_op->unlocked_ioctl(0x%x) error!!", pInst->vdec_id, VDEC_V3_FLUSH_RESULT_KERNEL);
                }
                ret = flush_info->result;
			}
		break;

		case VDEC_V3_DRAIN_KERNEL:
			{
				vdec_v3_drain_t* drain_info = (vdec_v3_drain_t*)args;
                if(pInst->pfdec_fd->f_op->unlocked_ioctl(pInst->pfdec_fd, VDEC_V3_DRAIN_RESULT_KERNEL, (unsigned long)drain_info) < 0)
				{
                    tcvdec_err("[VDEC-%d] pInst->pfdec_fd->f_op->unlocked_ioctl(0x%x) error!!", pInst->vdec_id, VDEC_V3_DRAIN_RESULT_KERNEL);
                }
                ret = drain_info->result;
			}
		break;

		case VDEC_V3_REG_FRAMEBUFFER_KERNEL:
		   {
			   vdec_v3_reg_framebuffer_t* regframebuffer_info = (vdec_v3_reg_framebuffer_t *)args;
			  if(pInst->pfdec_fd->f_op->unlocked_ioctl(pInst->pfdec_fd, VDEC_V3_REG_FRAMEBUFFER_RESULT_KERNEL, (unsigned long)regframebuffer_info) < 0)
			   {
				   tcvdec_err("[VDEC-%d] ioctl(0x%x) error!!", pInst->vdec_id, VDEC_V3_REG_FRAMEBUFFER_RESULT_KERNEL);
			   }
			   ret = regframebuffer_info->result;
		   }
		   break;

		case VDEC_V3_CLOSE_KERNEL:
			{
				vdec_v3_close_t* close_info = (vdec_v3_close_t*)args;
                if(pInst->pfdec_fd->f_op->unlocked_ioctl(pInst->pfdec_fd, VDEC_V3_CLOSE_RESULT_KERNEL, (unsigned long)close_info) < 0)
				{
                    tcvdec_err("[VDEC-%d] pInst->pfdec_fd->f_op->unlocked_ioctl(0x%x) error!!", pInst->vdec_id, VDEC_V3_CLOSE_RESULT_KERNEL);
                }
                ret = close_info->result;
			}
			break;

        default:
            tcvdec_err("[VDEC-%d] invalid cmd:%d, no result case", pInst->vdec_id, cmd);
            break;
    }

    if((ret&0xf000) != 0x0000)
	{ //If there is an invalid return, we skip it because this return means that vpu didn't process current command yet.
        all_retry_cnt--;
        if( all_retry_cnt > 0)
        {
            retry_cnt = 2;
            goto Retry;
        }
        else
        {
            tcvdec_err("abnormal exception!!");
        }
    }

    if(!success
        || ((ret&0xf000) != 0x0000) /* vpu can not start or finish its processing with unknown reason!! */
    )
    {
        tcvdec_err("[VDEC-%d] command(0x%x) didn't work properly. maybe hangup(no return(0x%x))!!", pInst->vdec_id, cmd, ret);

        if(ret != VPU_RETCODE_CODEC_EXIT && ret != VPU_RETCODE_MULTI_CODEC_EXIT_TIMEOUT){
//          pInst->pfdec_fd->f_op->unlocked_ioctl(pInst->mgr_fd, VPU_HW_RESET, (void*)NULL);
        }

        return VPU_RETCODE_CODEC_EXIT;
    }

    return ret;
}


enum vpu_return_code vdec_close(vdec_handle_h handle, struct device *dev)
{
	enum vpu_return_code vpu_ret = VPU_RETCODE_SUCCESS;
	vdec_t *pInst = (vdec_t *)handle;

	memset(&pInst->close_info, 0x00, sizeof(vdec_v3_close_t));

	if(pInst->addr_compressed)
	{
		dma_free_coherent(dev, pInst->size_compressed, pInst->va_compressed, (dma_addr_t)pInst->addr_compressed);
		pInst->addr_compressed = 0;
	}

	if(pInst->addr_mvcol)
	{
		dma_free_coherent(dev, pInst->size_mvcol, pInst->va_mvcol, (dma_addr_t)pInst->addr_mvcol);
		pInst->addr_mvcol = 0;
	}

	if(pInst->addr_ext)
	{
		dma_free_coherent(dev, pInst->size_ext, pInst->va_ext, (dma_addr_t)pInst->addr_ext);
		pInst->addr_ext = 0;
	}
	
	vpu_ret = vdec_cmd_process(VDEC_V3_CLOSE_KERNEL, (long unsigned int *)&pInst->close_info, pInst);
	if( vpu_ret != VPU_RETCODE_SUCCESS )
	{
		tcvdec_err( "[VDEC-%d] VPU_DEC_CLOSE failed Error code is 0x%x ", pInst->vdec_id, vpu_ret );
	}

	return vpu_ret;
}

vdec_handle_h vdec_alloc_instance()
{
    vdec_t *pInst = NULL;
	int ret = 0;

    pInst = (vdec_t*)kmalloc(sizeof(vdec_t), GFP_KERNEL);
    if(pInst != NULL)
    {
        memset(pInst, 0x00, sizeof(vdec_t));
        pInst->pfdec_fd = NULL;

		mutex_lock(&instance_mutex);

        pInst->pfdec_fd = filp_open(VPU_DECODER_DEVICE, O_RDWR, 0);
        if (pInst->pfdec_fd < 0)
        {
            tcvdec_err("/dev/vpu_vdec open error");
            ret = -1;
        }

		pInst->pfdec_fd_fop = pInst->pfdec_fd->f_op;

		if(ret == 0)
        {
			memset(&pInst->vpu_version, 0x00, sizeof(vpu_drv_version_t));
			ret = pInst->pfdec_fd_fop->unlocked_ioctl(pInst->pfdec_fd, VPU_V3_GET_DRV_VERSION_SYNC_KERNEL,(unsigned long) &pInst->vpu_version);
			if(ret == 0)
			{
				tcvdec_info( "VPU Driver Ver %d.%d.%d", pInst->vpu_version.major, pInst->vpu_version.minor, pInst->vpu_version.revision);
			}
			else
			{
				tcvdec_err("VPU_V3_GET_DRV_VERSION_SYNC_KERNEL error, ret:%d", ret);
			}

			memset(&pInst->vpu_capability, 0x00, sizeof(vpu_capability_t));
			ret = pInst->pfdec_fd_fop->unlocked_ioctl(pInst->pfdec_fd, VPU_V3_GET_CAPABILITY_SYNC_KERNEL, (unsigned long) &pInst->vpu_capability);

			pInst->vdec_id = pInst->vpu_capability.drv_id;
			pInst->use_dec_profile = 0;

			total_opened_decoder++;
		}
		else
		{
			tcvdec_err("/dev/vpu_vdec open error");
			kfree(pInst);
			pInst = NULL;
		}

		mutex_unlock(&instance_mutex);
	}

	return (vdec_handle_h)pInst;
}

void vdec_release_instance(vdec_handle_h handle)
{
	vdec_t *pInst = (vdec_t *)handle;

    if(pInst)
    {
		mutex_lock(&instance_mutex);

        if(pInst->pfdec_fd)
        {
            if(filp_close(pInst->pfdec_fd, NULL) < 0)
            {
                tcvdec_err("/dev/vpu_vdec close error");
            }
            pInst->pfdec_fd = NULL;
        }

		kfree(pInst);
        pInst = NULL;

        if (total_opened_decoder > 0)
        {
            total_opened_decoder--;
        }

		mutex_unlock(&instance_mutex);
    }
}

enum vpu_return_code vdec_init(vdec_handle_h handle, vdec_v3_init_t* p_init_param)
{
	enum vpu_return_code vpu_ret = VPU_RETCODE_SUCCESS;
	vdec_t *pInst = (vdec_t *)handle;

	memset(&pInst->init_info, 0x00, sizeof(vdec_v3_init_t));

	memcpy(&pInst->init_info, p_init_param, sizeof(vdec_v3_init_t));

/*
	tcvdec_info( "[VDEC-%d] codec id:%d, max w:%d, h:%d, output:%d, addtional frame count:%d, enable ringbuffer mode:%d, forced_pmap:%d, userdata:%d, enable dmabuff:%d",
		pInst->vdec_id, pInst->init_info.input.codec_id, pInst->init_info.input.max_support_width, pInst->init_info.input.max_support_height,
		pInst->init_info.input.output_format, pInst->init_info.input.additional_frame_count, pInst->init_info.input.enable_ringbuffer_mode,
		pInst->init_info.input.use_forced_pmap_idx, pInst->init_info.input.enable_user_data, pInst->init_info.input.enable_dma_buf_id);
*/
	vpu_ret = vdec_cmd_process(VDEC_V3_INIT_KERNEL, (long unsigned int *)&pInst->init_info, pInst);
	if( vpu_ret != VPU_RETCODE_SUCCESS )
	{
		tcvdec_err( "[VDEC-%d] VDEC_V3_INIT failed Error code is 0x%x ", pInst->vdec_id, vpu_ret );
	}
	else
	{
		pInst->bitstream_buf_addr[VPU_PA] = pInst->init_info.output.next_bitstream_buf_addr[VPU_PA];
		pInst->bitstream_buf_addr[VPU_KVA] = pInst->init_info.output.next_bitstream_buf_addr[VPU_KVA];
		pInst->bitstream_buf_size = pInst->init_info.output.next_bitstream_buf_size;
		
		pInst->bitstream_buf_addr_end[VPU_PA] = pInst->bitstream_buf_addr[VPU_PA] + pInst->bitstream_buf_size;
		pInst->bitstream_buf_addr_end[VPU_KVA] = pInst->bitstream_buf_addr[VPU_KVA] + pInst->bitstream_buf_size;

		pInst->bitstream_buf_next[VPU_PA] = pInst->bitstream_buf_addr[VPU_PA];
		pInst->bitstream_buf_next[VPU_KVA] = pInst->bitstream_buf_addr[VPU_KVA];
		pInst->bitstream_buf_next_size = pInst->bitstream_buf_size;

		pInst->max_bitstream_size = pInst->bitstream_buf_size;
		
		if(pInst->init_info.output.is_ringbuffer_mode == 1)
		{
			pInst->use_ringbuffer_mode = 1;
		}
		else
		{
			pInst->use_ringbuffer_mode = 0;
		}
	}

	return vpu_ret;
}

enum vpu_return_code vdec_seq_header(vdec_handle_h handle, struct tcc_codec_bs_t *bs, struct tcc_codec_header_t *hdr)
{
	enum vpu_return_code vpu_ret = VPU_RETCODE_SUCCESS;

	vdec_t *pInst = (vdec_t *)handle;

	memset(&pInst->seqenceheader_info, 0x00, sizeof(vdec_v3_seqheader_t));

	pInst->seqenceheader_info.input.bitstream_addr[VPU_PA] = (vpu_addr_t)bs->pa;
	pInst->seqenceheader_info.input.bitstream_addr[VPU_KVA] = (vpu_addr_t)bs->va;
	pInst->seqenceheader_info.input.bitstream_size = bs->size;

	pInst->seqenceheader_info.input.enable_user_register_framebuffer = 1U;
	pInst->enable_user_framebuffer = pInst->seqenceheader_info.input.enable_user_register_framebuffer;

	vpu_ret = vdec_cmd_process(VDEC_V3_SEQ_HEADER_KERNEL, (unsigned long*)&pInst->seqenceheader_info, pInst);
	if( vpu_ret != VPU_RETCODE_SUCCESS )
	{
		tcvdec_err( "[VDEC-%d] VPU_DEC_SEQ_HEADER failed Error code is 0x%x. ErrorReason is %d", pInst->vdec_id, vpu_ret, pInst->seqenceheader_info.output.initial_info.report_error_reason);
		if(vpu_ret == VPU_RETCODE_CODEC_SPECOUT)
			tcvdec_err("[VDEC-%d] NOT SUPPORTED CODEC. VPU SPEC OUT!!", pInst->vdec_id);
	}
	else
	{
		pInst->bitstream_buf_next[VPU_PA] = pInst->seqenceheader_info.output.next_bitstream_buf_addr[VPU_PA];
		pInst->bitstream_buf_next[VPU_KVA] = pInst->seqenceheader_info.output.next_bitstream_buf_addr[VPU_KVA];
		pInst->bitstream_buf_next_size = pInst->bitstream_buf_size;

		hdr->width = pInst->seqenceheader_info.output.initial_info.pic_width;
		hdr->height = pInst->seqenceheader_info.output.initial_info.pic_height;
		hdr->min_framebuffer_cnt = pInst->seqenceheader_info.output.initial_info.min_frame_buffer_count;
		hdr->profile = pInst->seqenceheader_info.output.initial_info.profile;
		hdr->level = pInst->seqenceheader_info.output.initial_info.level;
	}

    return vpu_ret;
}

int vdec_decode(vdec_handle_h handle, struct tcc_codec_bs_t* bs, struct tcc_codec_decode_output_t* output)
{
	enum vpu_return_code vpu_ret = VPU_RETCODE_SUCCESS;
	vdec_t *pInst = (vdec_t *)handle;

	memset(&pInst->decode_info, 0x00, sizeof(vdec_v3_decode_t));

	if(bs->size > pInst->bitstream_buf_next_size)
	{
		tcvdec_err("[VDEC-%d] write overflow, input_size:%d, buf_next_size:%d", pInst->vdec_id, pInst->decode_info.input.bitstream_size, pInst->bitstream_buf_next_size);
		pInst->decode_info.input.bitstream_size = 0;
	}
	else
	{
		pInst->decode_info.input.bitstream_addr[VPU_PA] = pInst->bitstream_buf_next[VPU_PA];
		pInst->decode_info.input.bitstream_addr[VPU_KVA] = pInst->bitstream_buf_next[VPU_KVA];

		memcpy( (unsigned long*)pInst->bitstream_buf_next[VPU_KVA], (unsigned long*)bs->va, bs->size);
		pInst->decode_info.input.bitstream_size = bs->size;
	}
	
	pInst->decode_info.input.skip_mode = VPU_FRAMESKIP_AUTO;

	vpu_ret = vdec_cmd_process(VDEC_V3_DECODE_KERNEL, (unsigned long*)&pInst->decode_info, pInst);

	pInst->total_frm++;

	if(vpu_ret != VPU_RETCODE_INFO_INSUFFICIENT_DATA)
	{
		if( (pInst->decode_info.output.out_info.decoded_status == VPU_DEC_STAT_BUF_FULL)
		|| ( (pInst->decode_info.output.out_info.decoded_status != VPU_DEC_STAT_SUCCESS_FIELD_PICTURE)
			&& (pInst->decode_info.output.out_info.display_width <= 64 || pInst->decode_info.output.out_info.display_height <= 64))
		||  (vpu_ret == VPU_RETCODE_CODEC_EXIT))
		{
			if(pInst->decode_info.output.out_info.decoded_status == VPU_DEC_STAT_BUF_FULL)
			{
				tcvdec_dbg("[VDEC-%d] Buffer full", pInst->vdec_id);
				output->status |= TCC_VIDEO_CODEC_STATUS_BUF_FULL;
			}
			else if (vpu_ret == VPU_RETCODE_CODEC_EXIT)
			{
				tcvdec_dbg("[VDEC-%d] Codec Exit", pInst->vdec_id);
			}
			else
			{
				tcvdec_dbg("[VDEC-%d] Strange resolution", pInst->vdec_id);
			}

			/*
			tcvdec_info("[VDEC-%d] Dec In 0x%llx, %d", pInst->vdec_id,
								pInst->decode_info.input.bitstream_addr[VPU_PA], pInst->decode_info.input.bitstream_size);

			tcvdec_info("[VDEC-%d] %d - %d - %d, %d - %d - %d", pInst->vdec_id, pInst->decode_info.output.out_info.display_width,
								pInst->decode_info.output.out_info.display_crop.left, pInst->decode_info.output.out_info.display_crop.right,
								pInst->decode_info.output.out_info.display_height,
								pInst->decode_info.output.out_info.display_crop.top, pInst->decode_info.output.out_info.display_crop.bottom);
			*/
		}
	}

	pInst->bitstream_buf_next[VPU_PA] = pInst->decode_info.output.next_bitstream_buf_addr[VPU_PA];
	pInst->bitstream_buf_next[VPU_KVA] = pInst->decode_info.output.next_bitstream_buf_addr[VPU_KVA];
	pInst->bitstream_buf_next_size = pInst->decode_info.output.next_bitstream_buf_size;
	tcvdec_dbg("[VDEC-%d] next bitstream addr:0x%llx, size:%d", pInst->vdec_id, pInst->bitstream_buf_next[VPU_PA], pInst->bitstream_buf_next_size);

	output->width =  pInst->decode_info.output.out_info.display_width;
	output->height =  pInst->decode_info.output.out_info.display_height;
	
	output->fb.pa[VPU_FRAMEBUFFER_Y] = pInst->decode_info.output.display_out[VPU_PA][VPU_COMP_Y];
	output->fb.pa[VPU_FRAMEBUFFER_CB] = pInst->decode_info.output.display_out[VPU_PA][VPU_COMP_U];
	output->fb.pa[VPU_FRAMEBUFFER_CR] = pInst->decode_info.output.display_out[VPU_PA][VPU_COMP_V];

	output->displayIndex = pInst->decode_info.output.out_info.display_idx;		
	output->decodedIndex= pInst->decode_info.output.out_info.decoded_idx;		


	if( vpu_ret == VPU_RETCODE_INFO_INSUFFICIENT_DATA )
	{
		tcvdec_err( "[VDEC-%d] VPU_DEC_DECODE failed 0x%x, Need more data", pInst->vdec_id, vpu_ret);
	}
	else if( vpu_ret == VPU_RETCODE_FRAME_NOT_COMPLETE )
	{
		tcvdec_err( "[VDEC-%d] VPU_DEC_DECODE is not completed, push next frame", pInst->vdec_id);
	}
	else if( vpu_ret == VPU_RETCODE_CODEC_FINISH )
	{
		tcvdec_err( "[VDEC-%d] VPU_DEC_DECODE failed Error code is 0x%x ", pInst->vdec_id, vpu_ret );
	}
	else if( vpu_ret != VPU_RETCODE_SUCCESS )
	{
		tcvdec_err( "[VDEC-%d] VPU_DEC_DECODE failed Error code is 0x%x ", pInst->vdec_id, vpu_ret );
	}
	else 
	{
		if(pInst->decode_info.output.out_info.display_status == VPU_DISP_STAT_SUCCESS) 
		{
			output->status = TCC_VIDEO_CODEC_STATUS_DISPLAYABLE;
		}
		if(pInst->decode_info.output.out_info.decoded_status == VPU_DEC_STAT_BUF_FULL) 
		{
			tcvdec_err("[VDEC-%d] buffer full", pInst->vdec_id);
			output->status |= TCC_VIDEO_CODEC_STATUS_BUF_FULL;
		}
		else if (pInst->decode_info.output.out_info.decoded_status == VPU_DEC_STAT_SUCCESS || 
			pInst->decode_info.output.out_info.decoded_status == VPU_DEC_STAT_SUCCESS_FIELD_PICTURE) 
		{
			output->status |= TCC_VIDEO_CODEC_STATUS_DECODED;
		}

			/*
			if(pInst->decode_info.output.out_info.specific_info.picture_structure == 1)
			{
				int top_field_type = 0;
				int bottom_field_type = 0;

				top_field_type = ((unsigned int)pInst->decode_info.output.out_info.pic_type >> 3U) & 0x07U;
				bottom_field_type = (unsigned int)pInst->decode_info.output.out_info.pic_type & 0x07U;
				tcvdec_err("[VDEC-%d] Dec ret:%d Interlace PicType[%s(%d),%s(%d)], OutIdx[%d/%d], OutStatus[%d/%d] \n", pInst->vdec_id, ret,
									get_pic_type_name(top_field_type), top_field_type, get_pic_type_name(bottom_field_type), bottom_field_type,
									pInst->decode_info.output.out_info.display_idx,pInst->decode_info.output.out_info.decoded_idx,
									pInst->decode_info.output.out_info.display_status, pInst->decode_info.output.out_info.decoded_status);
			}
			else
			{
				tcvdec_err("[VDEC-%d] Dec ret:%d Progressive PicType[%s(%d)], OutIdx[%d/%d], OutStatus[%d/%d] \n", pInst->vdec_id, ret,
									get_pic_type_name(pInst->decode_info.output.out_info.pic_type), pInst->decode_info.output.out_info.pic_type,
									pInst->decode_info.output.out_info.display_idx, pInst->decode_info.output.out_info.decoded_idx,
									pInst->decode_info.output.out_info.display_status, pInst->decode_info.output.out_info.decoded_status);
			}
			*/
	}

	
	return (int) vpu_ret;
}

enum vpu_return_code vdec_buf_clear(vdec_handle_h handle, u32 clear_idx)
{
	enum vpu_return_code vpu_ret = VPU_RETCODE_SUCCESS;
	vdec_t *pInst = (vdec_t *)handle;

	if(pInst->init_info.input.codec_id != VCODEC_ID_MJPG)
	{
		memset(&pInst->bufclear_info, 0x00, sizeof(vdec_v3_buf_clear_t));

		pInst->bufclear_info.index = clear_idx;
		pInst->bufclear_info.dma_buf_id = -1;

		vpu_ret = vdec_cmd_process(VDEC_V3_BUF_CLEAR_KERNEL, (unsigned long*)&pInst->bufclear_info, pInst);
		if( vpu_ret != VPU_RETCODE_SUCCESS )
		{
			tcvdec_err( "[VDEC-%d] VDEC_V3_BUF_CLEAR failed Error code is 0x%x ", pInst->vdec_id, vpu_ret);
		}
	}

	return vpu_ret;
}

enum vpu_return_code vdec_drain(vdec_handle_h handle, struct tcc_codec_decode_output_t* output)
{
	enum vpu_return_code vpu_ret = VPU_RETCODE_SUCCESS;
	vdec_t *pInst = (vdec_t *)handle;
	vdec_v3_reg_framebuffer_in_t* input = &pInst->regframebuffer_info.input;

	memset(&pInst->drain_info, 0x00, sizeof(vdec_v3_drain_t));
	vpu_ret = vdec_cmd_process(VDEC_V3_DRAIN_KERNEL, (unsigned long*)&pInst->drain_info, pInst);

	if(vpu_ret != VPU_RETCODE_SUCCESS) {
		tcvdec_dbg("[%s:%d] Failed to Drain(). ret=%d", __func__, __LINE__, vpu_ret);
		return -EFAULT;
	} else {
		if(pInst->drain_info.output.out_info.display_status == VPU_DISP_STAT_SUCCESS) {
			output->status = TCC_VIDEO_CODEC_STATUS_DISPLAYABLE;
			output->width =  pInst->drain_info.output.out_info.display_width;
			output->height =  pInst->drain_info.output.out_info.display_height;
			
			output->fb.pa[VPU_FRAMEBUFFER_Y] = (void *)input->frameBuffer[pInst->drain_info.output.out_info.display_idx][VPU_FRAMEBUFFER_Y].framebuffer[VPU_PA];
			output->fb.pa[VPU_FRAMEBUFFER_CB] = (void *)input->frameBuffer[pInst->drain_info.output.out_info.display_idx][VPU_FRAMEBUFFER_CB].framebuffer[VPU_PA];
			output->fb.pa[VPU_FRAMEBUFFER_CR] = (void *)input->frameBuffer[pInst->drain_info.output.out_info.display_idx][VPU_FRAMEBUFFER_CR].framebuffer[VPU_PA];
			
			output->displayIndex = pInst->drain_info.output.out_info.display_idx;
		}
	}

	return vpu_ret;
}

enum vpu_return_code vdec_flush(vdec_handle_h handle)
{
	enum vpu_return_code vpu_ret = VPU_RETCODE_SUCCESS;
	vdec_t *pInst = (vdec_t *)handle;

	if(!pInst) {
		tcvdec_err( "[VDEC-%d][%s:%d] vdec_handle is NULL", pInst->vdec_id, __func__, __LINE__);
		return VPU_RETCODE_FAILURE;
	}

	memset(&pInst->flush_info, 0x00, sizeof(vdec_v3_flush_t));
	vpu_ret = vdec_cmd_process(VDEC_V3_FLUSH_KERNEL, (unsigned long*)&pInst->flush_info, pInst);
	if( (vpu_ret != VPU_RETCODE_SUCCESS) && (vpu_ret != VPU_RETCODE_CODEC_FINISH) )
	{
		tcvdec_err( "[VDEC-%d] VDEC_V3_FLUSH failed Error code is 0x%x ", pInst->vdec_id, vpu_ret);
		vpu_ret = VPU_RETCODE_FAILURE;
	}

	return vpu_ret;
}


enum vpu_return_code vdec_register_framebuffer(vdec_handle_h handle, struct device *dev, struct tcc_codec_fb_t *fb_array, u32 number)
{
	enum vpu_return_code vpu_ret = VPU_RETCODE_SUCCESS;
	int buffer_idx;
	int buffer_type_idx = 0;
	int size_compressed = 0;

	vdec_t *pInst = (vdec_t *)handle;

	vdec_v3_reg_framebuffer_in_t* input = &pInst->regframebuffer_info.input;

	memset(&pInst->regframebuffer_info, 0x00, sizeof(vdec_v3_reg_framebuffer_t));

	if(pInst->init_info.input.codec_id == VCODEC_ID_HEVC)
	{
		size_compressed = pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_COMP_Y];
		size_compressed += pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_COMP_C];
		size_compressed += pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_FBCY];
		size_compressed += pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_FBCC];
		size_compressed += pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_MVCOL];
		pInst->size_compressed = size_compressed * number;
		
		pInst->va_compressed = dma_alloc_coherent(dev, pInst->size_compressed, &pInst->addr_compressed, GFP_KERNEL);

		tcvdec_dbg("[VDEC-%d] DMA Allocated compressed buffer:0x%llx (size : 0x%x)", pInst->vdec_id, pInst->addr_compressed, pInst->size_compressed);

		for(buffer_idx=0; buffer_idx < number; buffer_idx++)
		{
			vpu_addr_t addr_offset = pInst->addr_compressed + (buffer_idx * size_compressed);

			if (pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_COMP_Y] > 0) 
			{
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_COMP_Y].framebuffer[VPU_PA] = addr_offset;
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_COMP_Y].size = pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_COMP_Y];
				addr_offset += pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_COMP_Y];
			}
			if (pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_COMP_C] > 0) 
			{
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_COMP_C].framebuffer[VPU_PA] = addr_offset;
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_COMP_C].size = pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_COMP_C];
				addr_offset += pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_COMP_C];
			}
			if (pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_FBCY] > 0) 
			{
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_FBCY].framebuffer[VPU_PA] = addr_offset;
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_FBCY].size = pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_FBCY];
				addr_offset += pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_FBCY];
			}
			if (pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_FBCC] > 0) 
			{
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_FBCC].framebuffer[VPU_PA] = addr_offset;
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_FBCC].size = pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_FBCC];
				addr_offset += pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_FBCC];
			}
			if (pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_MVCOL] > 0) 
			{
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_MVCOL].framebuffer[VPU_PA] = addr_offset;
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_MVCOL].size = pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_MVCOL];
				addr_offset += pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_MVCOL];
			}

			tcvdec_dbg("[VDEC-%d] HEVC buffer[%d] Y:0x%llx, C:0x%llx, FBCY:0x%llx, FBCC:0x%llx, MVCOL:0x%llx", pInst->vdec_id,
				buffer_idx,
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_COMP_Y].framebuffer[VPU_PA],
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_COMP_C].framebuffer[VPU_PA],
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_FBCY].framebuffer[VPU_PA],
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_FBCC].framebuffer[VPU_PA],
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_MVCOL].framebuffer[VPU_PA]);
		}

		for (buffer_idx = number; buffer_idx < number * 2; buffer_idx++)
		{
			for (buffer_type_idx=VPU_FRAMEBUFFER_Y; buffer_type_idx <= VPU_FRAMEBUFFER_CR; buffer_type_idx++)
			{
				int fb_idx = buffer_idx - number;
				
				if (fb_array[fb_idx].pa[buffer_type_idx] != NULL)
				{
					input->frameBuffer[buffer_idx][buffer_type_idx].framebuffer[VPU_PA] = (vpu_addr_t) fb_array[fb_idx].pa[buffer_type_idx];
					input->frameBuffer[buffer_idx][buffer_type_idx].size = pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[buffer_type_idx];
				}
			}
			tcvdec_dbg("[VDEC-%d] buffer[%d] Y:0x%llx, CB:0x%llx, CR:0x%llx", pInst->vdec_id,
				buffer_idx,
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_Y].framebuffer[VPU_PA],
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_CB].framebuffer[VPU_PA],
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_CR].framebuffer[VPU_PA]);
	
		}
	} 
	else
	{
		if (pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_MVCOL] > 0 && pInst->init_info.input.codec_id != VCODEC_ID_HEVC) 
		{
			pInst->size_mvcol = pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_MVCOL] * number;
			pInst->va_mvcol = dma_alloc_coherent(dev, pInst->size_mvcol, &pInst->addr_mvcol, GFP_KERNEL);
	
			tcvdec_dbg("[VDEC-%d] DMA Allocated MVCOL buffer:0x%llx (size : 0x%x)", pInst->vdec_id, pInst->addr_mvcol, pInst->size_mvcol);
		}
	
		for (buffer_idx = 0; buffer_idx < number; buffer_idx++)
		{
			for (buffer_type_idx=0; buffer_type_idx<3; buffer_type_idx++)
			{
				if (fb_array[buffer_idx].pa[buffer_type_idx] != NULL)
				{
					input->frameBuffer[buffer_idx][buffer_type_idx].framebuffer[VPU_PA] = (vpu_addr_t) fb_array[buffer_idx].pa[buffer_type_idx];
					input->frameBuffer[buffer_idx][buffer_type_idx].size = pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[buffer_type_idx];
				}
			}
			tcvdec_dbg("[VDEC-%d] buffer[%d] Y:0x%llx, CB:0x%llx, CR:0x%llx", pInst->vdec_id,
				buffer_idx,
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_Y].framebuffer[VPU_PA],
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_CB].framebuffer[VPU_PA],
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_CR].framebuffer[VPU_PA]);
	
	
			if (pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_MVCOL] > 0 && pInst->init_info.input.codec_id != VCODEC_ID_HEVC) 
			{		
				vpu_addr_t addr_offset = pInst->addr_mvcol + (buffer_idx * pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_MVCOL]);
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_MVCOL].framebuffer[VPU_PA] = addr_offset;
				input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_MVCOL].size = pInst->seqenceheader_info.output.buffer_size_info.framebuffer_size[VPU_FRAMEBUFFER_MVCOL];
				tcvdec_dbg("[VDEC-%d] MVCOL buffer[%d]:0x%llx", pInst->vdec_id, buffer_idx, input->frameBuffer[buffer_idx][VPU_FRAMEBUFFER_MVCOL].framebuffer[VPU_PA]);
			}
		}
	
		if (pInst->init_info.input.codec_id == VCODEC_ID_AVC)
		{
			pInst->size_ext = pInst->seqenceheader_info.output.buffer_size_info.framebuffer_ext_size[VPU_FRAMEBUFFER_EXT_AVC_SLICE] * number;
			pInst->va_ext = dma_alloc_coherent(dev, pInst->size_ext, &pInst->addr_ext, GFP_KERNEL);

			tcvdec_dbg("[VDEC-%d] DMA Allocated AVC Extra buffer:0x%llx (size : 0x%x)", pInst->vdec_id, pInst->addr_ext, pInst->size_ext);

			input->framebuffer_ext[VPU_FRAMEBUFFER_EXT_AVC_SLICE].framebuffer[VPU_PA] = pInst->addr_ext;
			input->framebuffer_ext[VPU_FRAMEBUFFER_EXT_AVC_SLICE].size = pInst->seqenceheader_info.output.buffer_size_info.framebuffer_ext_size[VPU_FRAMEBUFFER_EXT_AVC_SLICE];
		}
	}

	input->frame_buffer_count = pInst->init_info.input.codec_id == VCODEC_ID_HEVC ? number * 2 : number;

	vpu_ret = vdec_cmd_process(VDEC_V3_REG_FRAMEBUFFER_KERNEL, (long unsigned int *)&pInst->regframebuffer_info, pInst);

	if (vpu_ret != VPU_RETCODE_SUCCESS)
	{
		tcvdec_err( "[VDEC-%d] VDEC_V3_REG_FRAMEBUFFER failed Error code is 0x%x", pInst->vdec_id, (int)vpu_ret);
		vpu_ret = VPU_RETCODE_FAILURE;
	}
	else
	{
		vpu_ret = VPU_RETCODE_SUCCESS;
	}

	return vpu_ret;
}
