/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#include "vpu_mgr_encode.h"
#include "vpu_comm.h"
#include "vpu_internal_type.h"
#include "vpu_rm.h"
#include "vpu_devices.h"
#include "vpu_mgr_sys.h"
#include "vpu_mgr.h"
#include "vpu_list_manager.h"
#include "vpu_dbg_string.h"
#include "vpu_sys_common.h"
#include "vpu_mgr_context.h"
#include "vpu_mgr_common.h"

#define dlog_vmgr(fmt, args...)		V_DBG(VPU_DBG_INFO, "[VPU_MGR_ENC][LOG]:" fmt, ## args)
#define detail_vmgr(fmt, args...)	V_DBG(VPU_DBG_DETAIL, "[VPU_MGR_ENC][DETAIL]:" fmt, ## args)
#define seq_vmgr(fmt, args...)		V_DBG(VPU_DBG_CMD, "[VPU_MGR_ENC][SEQ]:"  fmt, ## args)
#define err_vmgr(fmt, args...)		V_DBG(VPU_DBG_ERROR, "[VPU_MGR_ENC][ERR]:"  fmt, ## args)

#define dlog_info(fmt, args...)		V_DBG(VPU_DBG_INFO, "[VPU_MGR_ENC][LOG][id:%u]:" fmt, drv_info->drv_id, ## args)
#define detail_info(fmt, args...)	V_DBG(VPU_DBG_DETAIL, "[VPU_MGR_ENC][DETAIL][id:%u]:" fmt, drv_info->drv_id, ## args)
#define seq_info(fmt, args...)		V_DBG(VPU_DBG_CMD, "[VPU_MGR_ENC][SEQ][id:%u]:"  fmt, drv_info->drv_id, ## args)
#define err_info(fmt, args...)		V_DBG(VPU_DBG_ERROR, "[VPU_MGR_ENC][ERR][id:%u]:"  fmt, drv_info->drv_id, ## args)

#define DBG(fmt, args...)	do { pr_err("[DEC][%s:%d]" fmt "\n", __func__, __LINE__, ## args); } while (0)

static int vmgr_encode_alloc_init_buffer(vpu_mgr_t *mgr_ctx, vpu_drv_info_t *drv_info, venc_v3_init_t *cmd_init)
{
	int ret = 0;
	int codec_type;
	unsigned buf_size;
	MEM_ALLOC_INFO_t bitstreambuf_info;
	MEM_ALLOC_INFO_t bitworkbuf_info;

	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;
	vpu_ip_module_t *each_ip = mgr_ctx->each_ip;

	codec_type = vmgr_get_bitstream_format(drv_info->codec_id, VPU_OP_TYPE_ENC);

#if defined(ENABLE_VPU_DRV_HEVCENC2)
	if (drv_info->ip_type == VPU_IP_HEVC_ENC2 && codec_type == STD_HEVC_ENC) {
		codec_type = STD_HEVC_ENC2;
		dlog_info("alloc STD_HEVC_ENC2 bitstream buffer");
	}
#endif

	//bitstream buffer
	if (cmd_init->input.user_bitstream_buf_size > 0) {
		buf_size = ALIGNED_BUFF(cmd_init->input.user_bitstream_buf_size, (1024u));
	} else {
		buf_size = each_ip->proc_get_buffer_size(each_ip->ip_private, VMGR_BUF_BITSTREAM, drv_info);
	}

	if (buf_size > 0) {
		bitstreambuf_info.buffer_type = BUFFER_ELSE;
		bitstreambuf_info.request_size = buf_size;

		dlog_info("try alloc bitstream_buf, codec_type:%d, buffer_type:%d, request_size:0x%x", codec_type, bitstreambuf_info.buffer_type, bitstreambuf_info.request_size);
		ret = vmem_proc_alloc_memory(codec_type, &bitstreambuf_info, (vputype)drv_info->pmap_type);
		if (ret == 0) {

			alloc_info->bitstream_buf.addr[VPU_PA] = (codec_addr_t)bitstreambuf_info.phy_addr;
			alloc_info->bitstream_buf.addr[VPU_KVA] = (codec_addr_t)bitstreambuf_info.kernel_remap_addr;
			alloc_info->bitstream_buf.size = bitstreambuf_info.request_size;
			dlog_info("alloc success bitstream_buf, PA:0x%llx, KVA:0x%llx, size:%d",
				alloc_info->bitstream_buf.addr[VPU_PA], alloc_info->bitstream_buf.addr[VPU_KVA], alloc_info->bitstream_buf.size);
		} else {
			ret = -1;
			err_info("alloc bitstream_buf fail");
		}
	}

	//bitwork buffer
	if (ret == 0) {
		buf_size = each_ip->proc_get_buffer_size(each_ip->ip_private, VMGR_BUF_BITWORK, drv_info);
		dlog_info("bitwork buffer size:%d", buf_size);
		if (buf_size > 0) {
			bitworkbuf_info.buffer_type = BUFFER_WORK;
			bitworkbuf_info.request_size = buf_size;

			dlog_info("try alloc bitworkbuf_info, codec_type:%d, buffer_type:%d, request_size:0x%x", codec_type, bitworkbuf_info.buffer_type, bitworkbuf_info.request_size);
			ret = vmem_proc_alloc_memory(codec_type, &bitworkbuf_info, (vputype)drv_info->pmap_type);
			if (ret == 0) {
				alloc_info->bitwork_buf.addr[VPU_PA] = (codec_addr_t)bitworkbuf_info.phy_addr;
				alloc_info->bitwork_buf.addr[VPU_KVA] = (codec_addr_t)bitworkbuf_info.kernel_remap_addr;
				alloc_info->bitwork_buf.size = bitworkbuf_info.request_size;
				dlog_info("alloc success bitwork_buf, PA:0x%llx, KVA:0x%llx, size:%d",
					alloc_info->bitwork_buf.addr[VPU_PA], alloc_info->bitwork_buf.addr[VPU_KVA], alloc_info->bitwork_buf.size);
			} else {
				ret = -1;
				err_info("alloc bitworkbuf_info fail");
			}
		}
	}

	return ret;
}

static int vmgr_encode_alloc_framebuffer(vpu_mgr_t *mgr_ctx, vpu_drv_info_t *drv_info, venc_v3_init_t *cmd_init)
{
	int ret = 0;
	int codec_type;
	unsigned buf_size;
	MEM_ALLOC_INFO_t framebuff_info;
	vpu_pmap_alloc_info_t *alloc_info = NULL;
	vpu_ip_module_t *each_ip = NULL;

	alloc_info = &drv_info->pmap_alloc_info;
	each_ip = mgr_ctx->each_ip;

	dlog_info("alloc framebuffer");

	codec_type = vmgr_get_bitstream_format(drv_info->codec_id, VPU_OP_TYPE_ENC);

#if defined(ENABLE_VPU_DRV_HEVCENC2)
	if (drv_info->ip_type == VPU_IP_HEVC_ENC2 && codec_type == STD_HEVC_ENC) {
		codec_type = STD_HEVC_ENC2;
	}
#endif

	//V_DBG(VPU_DBG_INFO, "[VPU_MGR][LOG]-[%s][%s][id:%u]: alloc framebuffer", vmgr_get_ip_name(drv_info->ip_type), vmgr_get_optype_name(drv_info->op_type), drv_info->drv_id);

	buf_size = each_ip->proc_get_buffer_size(mgr_ctx, VMGR_BUF_FRAMEBUF, drv_info);
	//V_DBG(VPU_DBG_INFO, "[VPU_MGR][LOG]-[%s][%s][id:%u]:buf_size:%d", vmgr_get_ip_name(drv_info->ip_type), vmgr_get_optype_name(drv_info->op_type), drv_info->drv_id, buf_size);
	if (buf_size > 0) {
		V_DBG(VPU_DBG_INFO, "[VPU_MGR][LOG]-[%s][%s][id:%u]: bufsize:%d", vmgr_get_ip_name(drv_info->ip_type), vmgr_get_optype_name(drv_info->op_type), drv_info->drv_id, buf_size);

		//framebuffer
		dlog_info("alloc framebuffer min count:%d, size:%d", cmd_init->output.min_frame_buffer_count, cmd_init->output.min_frame_buffer_size);
		alloc_info->framebuffer_count = cmd_init->output.min_frame_buffer_count;


		dlog_info("framebuffer alloc count:%d, alloc size:%d", cmd_init->output.min_frame_buffer_count, cmd_init->output.min_frame_buffer_size);

		buf_size = cmd_init->output.min_frame_buffer_count * cmd_init->output.min_frame_buffer_size;
		buf_size = ALIGNED_BUFF((unsigned)buf_size, (4096u));

		dlog_info("alloc framebuffer size", vmgr_get_optype_name(drv_info->op_type), drv_info->drv_id, buf_size);

		if (buf_size > 0) {
			framebuff_info.buffer_type = BUFFER_ELSE; //why encoder alloc buffer by BUFFER_ELSE althogh this is framebuffer.
			framebuff_info.request_size = buf_size;

			dlog_info("try alloc framebuff_info, codec_type:%d buffer_type:%d, request_size:0x%x", codec_type, framebuff_info.buffer_type, framebuff_info.request_size);
			ret = vmem_proc_alloc_memory(codec_type, &framebuff_info, (vputype)drv_info->pmap_type);
			if (ret == 0) {
				alloc_info->frame_buf.addr[VPU_PA] = (codec_addr_t)framebuff_info.phy_addr;
				alloc_info->frame_buf.addr[VPU_KVA] = (codec_addr_t)framebuff_info.kernel_remap_addr;
				alloc_info->frame_buf.size = framebuff_info.request_size;
				dlog_info("alloc success, frame_buf, PA:0x%llx, KVA:0x%llx, size:%d",
					alloc_info->frame_buf.addr[VPU_PA], alloc_info->frame_buf.addr[VPU_KVA], alloc_info->frame_buf.size);
			} else {
				ret = -1;
				err_info("alloc framebuff_info fail");
			}
		}
	} else {
		dlog_info("don't use framebuffer for encoding %s");
	}

	return ret;
}


static int vmgr_encode_pre_proc(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd, vpu_drv_info_t *drv_info)
{
	int ret = 0;

	dlog_info("cmd:%s, drv_info:%p", vmgr_cmd_name(cmd->cmd_type), drv_info);

	switch (cmd->cmd_type) {
	case VPU_CMD_ENC_INIT:
	{
		venc_v3_init_t *cmd_init = (venc_v3_init_t *)cmd->args;

		drv_info->handle = 0x00;

		if ((cmd_init->input.pic_width < 16) || (cmd_init->input.pic_width > VPU_LIMIT_PICWIDTH) ||
			(cmd_init->input.pic_height < 16) || (cmd_init->input.pic_height > VPU_LIMIT_PICHEIGHT)) {
			err_info("not supported pic resolution %d x %d", cmd_init->input.pic_width, cmd_init->input.pic_height);
			ret = RETCODE_INVALID_STRIDE;
		} else {

#if defined(USE_ACCESS_POINT)
			dlog_info("vmgr_accesspoint_check --- cmd:%s", vmgr_cmd_name(cmd->cmd_type));
			if (vmgr_accesspoint_check_addr_valid(mgr_ctx->access_point) != 0) {
				err_info("Invalid Access address!!");
				ret = -1;
			}
			dlog_info("vmgr_accesspoint_check OK +++, cmd:%s", vmgr_cmd_name(cmd->cmd_type));
#endif

			if (ret == 0) {
				ret = vmgr_encode_alloc_init_buffer(mgr_ctx, drv_info, cmd_init);
				if (ret != 0) {
					cmd->result = VPU_RETCODE_INSUFFICIENT_MEMORY;
					err_info("failed to allocate vmgr_encode_alloc_init_buffer()");
				}
			}

			if (ret == 0) {
				ret = vmgr_alloc_ip_parameter(drv_info, mgr_ctx->each_ip->enc_param_size);
				if (ret != 0) {
					cmd->result = VPU_RETCODE_INSUFFICIENT_MEMORY;
					err_info("failed to allocate vmgr_alloc_ip_parameter() for encoder");
				}
			}
		}
	}
	break;

	default:
	{
		ret = 0;
	}
	break;
	}

	return ret;
}

static int vmgr_encode_post_proc(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;

	dlog_info("CMD:%s(%d), result:%d", vmgr_cmd_name(cmd->cmd_type), cmd->cmd_type, cmd->result);
	switch (cmd->cmd_type) {
	case VPU_CMD_ENC_INIT:
	{
		vpu_ip_module_t *each_ip = mgr_ctx->each_ip;

		dlog_info("try alloc framebuffer, each_ip:%p", each_ip);
		if (cmd->result != VPU_RETCODE_SUCCESS) {
			if (cmd->result != VPU_RETCODE_CODEC_EXIT) {
				vetc_dump_reg_all(mgr_ctx->base_addr, "enc init failure");
			}
		}

		if (cmd->result == VPU_RETCODE_SUCCESS) {
			venc_v3_init_t *cmd_init = (venc_v3_init_t *)cmd->args;

			dlog_info("try alloc framebuffer");
			ret = vmgr_encode_alloc_framebuffer(mgr_ctx, drv_info, cmd_init);
			if (ret == 0) {
				//it is performed only if using the framebuffer.
				if (alloc_info->frame_buf.size > 0) {
					//framebuffer register
					dlog_info("call VPU_ENC_REG_FRAME_BUFFER");
					cmd->result = each_ip->proc_encode(mgr_ctx, VPU_CMD_ENC_REG_FRAME_BUFFER, cmd, drv_info);
					dlog_info("VPU_ENC_REG_FRAME_BUFFER, result:%d", cmd->result);
					if (cmd->result != VPU_RETCODE_SUCCESS) {
						err_info("VPU_DEC_REG_FRAME_BUFFER failed, result:%d", cmd->result);
					}
				}
			} else {
				cmd->result = VPU_RETCODE_INSUFFICIENT_MEMORY;
			}

			if (cmd->result == VPU_RETCODE_SUCCESS) {
				//FIXME : need to calculate to next buffers
				cmd_init->output.bitstream_out[VPU_PA] = alloc_info->bitstream_buf.addr[VPU_PA];
				cmd_init->output.bitstream_out[VPU_KVA] = alloc_info->bitstream_buf.addr[VPU_KVA];
				cmd_init->output.bitstream_outsize = alloc_info->bitstream_buf.size;
			}

			drv_info->opened = true;

				dlog_info("Init Done with ret(0x%x), bitstream pa:0x%llx, kva:0x%llx, size:%d",
						cmd->result, cmd_init->output.bitstream_out[VPU_PA], cmd_init->output.bitstream_out[VPU_KVA], cmd_init->output.bitstream_outsize);

		} else {
			(void)vmem_proc_free_memory(cmd->pmap_type);
		}
	}
	break;

	case VPU_CMD_ENC_CLOSE:
	{
		drv_info->handle = 0x00;
		drv_info->opened = false;
		(void)vmem_proc_free_memory(cmd->pmap_type);
		(void)vmgr_release_ip_parameter(drv_info);
	}
	break;

	default:
		ret = 0;
	break;
	}

	return ret;
}

int vmgr_encode_proc(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd)
{
	int ret = 0;
	vpu_ip_module_t *each_ip = mgr_ctx->each_ip;
	vpu_drv_info_t *drv_info = cmd->drv_info;

	if (cmd->cmd_type != VPU_CMD_ENC_INIT) {
		if ((drv_info->opened == false)  || (drv_info->handle == 0x00)) {
			cmd->result = VPU_RETCODE_MULTI_CODEC_EXIT_TIMEOUT;
			ret = -1;
		}
	}

	if ((each_ip->proc_encode != NULL) && (cmd->result != VPU_RETCODE_MULTI_CODEC_EXIT_TIMEOUT)) {
		//pre-processing
		ret = vmgr_encode_pre_proc(mgr_ctx, cmd, drv_info);
		if (ret == 0) {
			dlog_info("- cmd:%s, drv_info:%p", vmgr_cmd_name(cmd->cmd_type), drv_info);

			//each ip's encoding process
			cmd->result = each_ip->proc_encode(mgr_ctx, cmd->cmd_type, cmd, drv_info);

			dlog_info("+ cmd:%s, ret:%d", vmgr_cmd_name(cmd->cmd_type), cmd->result);

			//post-processing
			ret = vmgr_encode_post_proc(mgr_ctx, cmd, drv_info);
		}
	}

	return ret;
}
