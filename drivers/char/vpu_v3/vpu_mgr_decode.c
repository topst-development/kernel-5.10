// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_mgr_decode.h"
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
#include "vpu_mem.h"

#define dlog_vmgr(fmt, args...)  	V_DBG(VPU_DBG_INFO, "[VPU_MGR_DEC][LOG]:" fmt, ## args)
#define detail_vmgr(fmt, args...)  	V_DBG(VPU_DBG_DETAIL, "[VPU_MGR_DEC][DETAIL]:" fmt, ## args)
#define seq_vmgr(fmt, args...)     	V_DBG(VPU_DBG_CMD, "[VPU_MGR_DEC][SEQ]:"  fmt, ## args)
#define err_vmgr(fmt, args...)      V_DBG(VPU_DBG_ERROR, "[VPU_MGR_DEC][ERR]:"  fmt, ## args)

#define dlog_info(fmt, args...)  	V_DBG(VPU_DBG_INFO, "[VPU_MGR_DEC][LOG][id:%u]:" fmt, drv_info->drv_id, ## args)
#define detail_info(fmt, args...)  	V_DBG(VPU_DBG_DETAIL, "[VPU_MGR_DEC][DETAIL][id:%u]:" fmt, drv_info->drv_id, ## args)
#define seq_info(fmt, args...)     	V_DBG(VPU_DBG_CMD, "[VPU_MGR_DEC][SEQ][id:%u]:"  fmt, drv_info->drv_id, ## args)
#define err_info(fmt, args...)      V_DBG(VPU_DBG_ERROR, "[VPU_MGR_DEC][ERR][id:%u]:"  fmt, drv_info->drv_id, ## args)

#define DBG(fmt, args...)	do { pr_err("[DEC][%s:%d]" fmt "\n", __FUNCTION__, __LINE__, ## args); } while (0)

#define MAX_FRAMEBUFFER_COUNT	(31)

static int vmgr_decode_get_bitstream_offset_update_index(vpu_pmap_alloc_info_t *alloc_info)
{
	int offset = 0;

	offset = alloc_info->bitstream_buffer_index * alloc_info->size_of_bitstream_buffer;
	alloc_info->bitstream_buffer_index = (alloc_info->bitstream_buffer_index + 1) % alloc_info->num_of_bitstream_buffers;

	return offset;
}

static int vmgr_decode_alloc_init_buffer(vpu_mgr_t *mgr_ctx, vpu_drv_info_t *drv_info, vdec_v3_init_t *cmd_init)
{
	int ret = 0;
	int codec_type;
	unsigned buf_size;
	MEM_ALLOC_INFO_t bitstreambuf_info;
	MEM_ALLOC_INFO_t spsppssavebuf_info;
	MEM_ALLOC_INFO_t userdatabuf_info;
	MEM_ALLOC_INFO_t bitworkbuf_info;

	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;
	vpu_ip_module_t *each_ip = mgr_ctx->each_ip;

	codec_type = vmgr_get_bitstream_format(drv_info->codec_id, VPU_OP_TYPE_DEC);

	//bitstream buffer
	if (cmd_init->input.user_bitstream_buf_size > 0) {
		buf_size = ALIGNED_BUFF(cmd_init->input.user_bitstream_buf_size, (1024u));
	} else {
		buf_size = each_ip->proc_get_buffer_size(mgr_ctx, VMGR_BUF_BITSTREAM, drv_info);
	}

	if (buf_size > 0) {
		bitstreambuf_info.buffer_type = BUFFER_STREAM;
		bitstreambuf_info.request_size = buf_size;

		dlog_info("try alloc bitstream_buf, codec_type:%d, buffer_type:%d, request_size:0x%x", codec_type, bitstreambuf_info.buffer_type, bitstreambuf_info.request_size);
		ret = vmem_proc_alloc_memory(codec_type, &bitstreambuf_info, (vputype)drv_info->pmap_type);
		if (ret == 0) {
			alloc_info->bitstream_buf.addr[VPU_PA] = (codec_addr_t)bitstreambuf_info.phy_addr;
			alloc_info->bitstream_buf.addr[VPU_KVA] = (codec_addr_t)bitstreambuf_info.kernel_remap_addr;
			alloc_info->bitstream_buf.size = bitstreambuf_info.request_size;

			alloc_info->num_of_bitstream_buffers = each_ip->proc_get_buffer_size(mgr_ctx, VMGR_BUF_NUM_OF_BITSTREAM, drv_info);
			dlog_info("number of bitstream buffers:%d", alloc_info->num_of_bitstream_buffers);
			if (alloc_info->num_of_bitstream_buffers > 0) {
				alloc_info->size_of_bitstream_buffer = alloc_info->bitstream_buf.size / alloc_info->num_of_bitstream_buffers;
				alloc_info->size_of_bitstream_buffer = ALIGNED_BUFF(alloc_info->size_of_bitstream_buffer, 4096u);
			} else {
				alloc_info->size_of_bitstream_buffer = alloc_info->bitstream_buf.size;
			}

			alloc_info->bitstream_buffer_index = 0;

			dlog_info("alloc success bitstream_buf, PA:0x%llx, KVA:0x%llx, size:%d, num_of_buffers:%d, size_of_buffers:%d",
				alloc_info->bitstream_buf.addr[VPU_PA], alloc_info->bitstream_buf.addr[VPU_KVA], alloc_info->bitstream_buf.size,
				alloc_info->num_of_bitstream_buffers, alloc_info->size_of_bitstream_buffer);
		} else {
			ret = -1;
			err_info("alloc bitstream_buf fail");
		}
	}

	//sps pps save buffer
	if ((ret == 0) && ((drv_info->codec_id == VCODEC_ID_AVC) || (drv_info->codec_id == VCODEC_ID_MVC))) {
		buf_size = each_ip->proc_get_buffer_size(mgr_ctx, VMGR_BUF_SPSPPS, drv_info);
		if (buf_size > 0) {
			spsppssavebuf_info.buffer_type = BUFFER_PS;
			spsppssavebuf_info.request_size = buf_size;

			dlog_info("try alloc spsppssavebuf_info, codec_type:%d, buffer_type:%d, request_size:0x%x", codec_type, spsppssavebuf_info.buffer_type, spsppssavebuf_info.request_size);
			ret = vmem_proc_alloc_memory(codec_type, &spsppssavebuf_info, (vputype)drv_info->pmap_type);
			if (ret == 0) {
				alloc_info->spspps_buf.addr[VPU_PA] = (codec_addr_t)spsppssavebuf_info.phy_addr;
				alloc_info->spspps_buf.size = spsppssavebuf_info.request_size;
				dlog_info("alloc success, spsppssavebuf, PA:0x%x, size:%d", alloc_info->spspps_buf.addr[VPU_PA], alloc_info->spspps_buf.size);
			} else {
				ret = -1;
				err_info("alloc spsppssavebuf_info fail");
			}
		}
	}

	//user data buffer
	if ((ret == 0) && (cmd_init->input.enable_user_data == 1U)) {
		if (cmd_init->input.user_userdata_buf_size > 0) {
			buf_size = ALIGNED_BUFF((unsigned)cmd_init->input.user_userdata_buf_size, (4096u));
		} else {
			buf_size = each_ip->proc_get_buffer_size(mgr_ctx, VMGR_BUF_USERDATA, drv_info);
		}

		if (buf_size > 0) {
			userdatabuf_info.buffer_type = BUFFER_USERDATA;
			userdatabuf_info.request_size = buf_size;

			dlog_info("try alloc userdatabuf_info, codec_type:%d, buffer_type:%d, request_size:0x%x", codec_type, userdatabuf_info.buffer_type, userdatabuf_info.request_size);
			ret = vmem_proc_alloc_memory(codec_type, &userdatabuf_info, (vputype)drv_info->pmap_type);
			if (ret == 0) {
				alloc_info->userdata_buf.addr[VPU_PA] = (codec_addr_t)userdatabuf_info.phy_addr;
				alloc_info->userdata_buf.addr[VPU_KVA] = (codec_addr_t)userdatabuf_info.kernel_remap_addr;
				alloc_info->userdata_buf.size = userdatabuf_info.request_size;
				dlog_info("alloc success userdata_buf, PA:0x%llx, KVA:0x%llx, size:%d",
					alloc_info->userdata_buf.addr[VPU_PA], alloc_info->userdata_buf.addr[VPU_KVA], alloc_info->userdata_buf.size);
			} else {
				ret = -1;
				err_info("alloc userdatabuf_info fail");
			}
		}
	}

	//bitwork buffer
	if (ret == 0) {
		buf_size = each_ip->proc_get_buffer_size(mgr_ctx, VMGR_BUF_BITWORK, drv_info);
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


	//set additional frame buffer count to get size of frame buffer
	alloc_info->additional_frame_buffer_count = cmd_init->input.additional_frame_count;
	dlog_info("set additional_frame_buffer_count:%d", alloc_info->additional_frame_buffer_count);

	return ret;
}

static int vmgr_decode_alloc_framebuffer(vpu_mgr_t *mgr_ctx, vpu_drv_info_t *drv_info, vdec_v3_seqheader_t *cmd_seqheader)
{
	int ret = 0;
	int codec_type;
	unsigned buf_size;
	MEM_ALLOC_INFO_t avcslicebuf_info;
	MEM_ALLOC_INFO_t mbdatabuf_info;
	MEM_ALLOC_INFO_t framebuff_info;

	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;
	vpu_ip_module_t *each_ip = mgr_ctx->each_ip;

	codec_type = vmgr_get_bitstream_format(drv_info->codec_id, VPU_OP_TYPE_DEC);

	//avc slice buffer
	if (drv_info->codec_id == VCODEC_ID_AVC) {
		buf_size = each_ip->proc_get_buffer_size(mgr_ctx, VMGR_BUF_SLICE, drv_info);

		if (buf_size > 0) {
			avcslicebuf_info.buffer_type = BUFFER_SLICE;
			avcslicebuf_info.request_size = buf_size;

			dlog_info("try alloc avcslicebuf_info, buffer_type:%d, request_size:0x%x", avcslicebuf_info.buffer_type, avcslicebuf_info.request_size);
			ret = vmem_proc_alloc_memory(codec_type, &avcslicebuf_info, (vputype)drv_info->pmap_type);
			if (ret == 0) {
				alloc_info->slice_buf.addr[VPU_PA] = (codec_addr_t)avcslicebuf_info.phy_addr;
				alloc_info->slice_buf.addr[VPU_KVA] = (codec_addr_t)avcslicebuf_info.kernel_remap_addr;
				alloc_info->slice_buf.size = avcslicebuf_info.request_size;
				dlog_info("alloc success slice_buf, PA:0x%llx, KVA:0x%llx, size:%d",
					alloc_info->slice_buf.addr[VPU_PA], alloc_info->slice_buf.addr[VPU_KVA], alloc_info->slice_buf.size);
			} else {
				ret = -1;
				err_info("alloc avcslicebuf_info fail");
			}
		}
	}

	//vp8 mb buffer
	if (drv_info->codec_id == VCODEC_ID_VP8) {
		buf_size = each_ip->proc_get_buffer_size(mgr_ctx, VMGR_BUF_MBDATA, drv_info);
		if (buf_size > 0) {
			mbdatabuf_info.buffer_type = BUFFER_ELSE;
			mbdatabuf_info.request_size = buf_size;

			dlog_info("try alloc vp8 mbdatabuf_info, buffer_type:%d, request_size:0x%x", mbdatabuf_info.buffer_type, mbdatabuf_info.request_size);
			ret = vmem_proc_alloc_memory(codec_type, &mbdatabuf_info, (vputype)drv_info->pmap_type);
			if (ret == 0) {
				alloc_info->mbdata_buf.addr[VPU_PA] = (codec_addr_t)mbdatabuf_info.phy_addr;
				alloc_info->mbdata_buf.addr[VPU_KVA] = (codec_addr_t)mbdatabuf_info.kernel_remap_addr;
				alloc_info->mbdata_buf.size = mbdatabuf_info.request_size;
				dlog_info("alloc success vp8 mbdata_buf, PA:0x%llx, KVA:0x%llx, size:%d",
					alloc_info->mbdata_buf.addr[VPU_PA], alloc_info->mbdata_buf.addr[VPU_KVA], alloc_info->mbdata_buf.size);
			} else {
				ret = -1;
				err_info("alloc mbdatabuf_info fail");
			}
		}
	}

	//framebuffer
	dlog_info("ret:%d, to alloc framebuffer", ret);
	if (ret == 0) {
		dlog_info("framebuffer min count:%d, add count:%d", cmd_seqheader->output.initial_info.min_frame_buffer_count, alloc_info->additional_frame_buffer_count);
		alloc_info->framebuffer_count = cmd_seqheader->output.initial_info.min_frame_buffer_count + alloc_info->additional_frame_buffer_count;
		if (alloc_info->framebuffer_count > MAX_FRAMEBUFFER_COUNT) {
			alloc_info->framebuffer_count = MAX_FRAMEBUFFER_COUNT;
		}

		dlog_info("framebuffer alloc count:%d, alloc size:%d", alloc_info->framebuffer_count, cmd_seqheader->output.initial_info.min_frame_buffer_size);

		buf_size = alloc_info->framebuffer_count * cmd_seqheader->output.initial_info.min_frame_buffer_size;
		buf_size = ALIGNED_BUFF((unsigned)buf_size, (4096u));

		dlog_info("alloc framebuffer size", buf_size);

		if (buf_size > 0) {
			framebuff_info.buffer_type = BUFFER_FRAMEBUFFER;
			framebuff_info.request_size = buf_size;

			dlog_info("try alloc framebuff_info, buffer_type:%d, request_size:0x%x", framebuff_info.buffer_type, framebuff_info.request_size);
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
	}

	return ret;
}

#if defined(ENABLE_RINGBUFFER_MODE)
static int vmgr_ringbuffer_getinfo(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd, vpu_drv_info_t *drv_info, vdec_v3_ringbuff_get_info_t *get_info)
{
	int ret = 0;
	vpu_cmd_t tmp_cmd;
	vpu_ip_module_t *each_ip = mgr_ctx->each_ip;
	vetc_memcpy(&tmp_cmd, cmd, sizeof(vpu_cmd_t), 0);

	//get buffer info
	tmp_cmd.cmd_type = VPU_CMD_DEC_RING_GET_INFO;
	tmp_cmd.args = get_info;
	ret = each_ip->proc_decode(mgr_ctx, tmp_cmd.cmd_type, &tmp_cmd, drv_info);

	return ret;
}

static int vmgr_ringbuffer_setinfo(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd, vpu_drv_info_t *drv_info, vdec_v3_ringbuff_set_info_t *set_info)
{
	int ret = 0;
	vpu_cmd_t tmp_cmd;
	vpu_ip_module_t *each_ip = mgr_ctx->each_ip;

	vetc_memcpy(&tmp_cmd, cmd, sizeof(vpu_cmd_t), 0);

	//set buffer info
	tmp_cmd.cmd_type = VPU_CMD_DEC_RING_SET_INFO;
	tmp_cmd.args = set_info;
	ret = each_ip->proc_decode(mgr_ctx, tmp_cmd.cmd_type, &tmp_cmd, drv_info);

	return ret;
}
#endif //defined(ENABLE_RINGBUFFER_MODE)

static int vmgr_dec_init_preproc(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	vpu_ip_module_t *each_ip = mgr_ctx->each_ip;
	vdec_v3_init_t *cmd_init = (vdec_v3_init_t *)cmd->args;

	drv_info->handle = 0x00;

#if defined(USE_ACCESS_POINT)
	if (vmgr_accesspoint_check_addr_valid(mgr_ctx->access_point) != 0) {
		err_info("Invalid Access address!!");
		ret = -1;
	}
#endif

	if (ret != -1) {
		if (cmd_init->input.enable_interlace_processing == 1U) {
			drv_info->enable_interlace_delay_proc = 1U;
			dlog_info("enable interlace delay processing");
		}

		ret = vmgr_decode_alloc_init_buffer(mgr_ctx, drv_info, cmd_init);
		if (ret != 0) {
			cmd->result = VPU_RETCODE_INSUFFICIENT_MEMORY;
			err_info("failed to allocate vmgr_decode_alloc_init_buffer()");
		}

		ret = vmgr_alloc_ip_parameter(drv_info, each_ip->dec_param_size);
		if (ret != 0) {
			cmd->result = VPU_RETCODE_INSUFFICIENT_MEMORY;
			err_info("failed to allocate vmgr_alloc_ip_parameter() for decoder");
		}
	}

	return ret;
}

static int vmgr_dec_seqhead_preproc(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	vdec_v3_seqheader_t *cmd_seqheader = (vdec_v3_seqheader_t *)cmd->args;

	if (cmd_seqheader->input.enable_user_register_framebuffer == 1U) {
		drv_info->enable_user_register_framebuffer = 1U;
		dlog_info("enable_user_register_framebuffer");
	}

	if (drv_info->enabled_ringbuffer_mode == 1U) {
#if defined(ENABLE_RINGBUFFER_MODE)
		vdec_v3_ringbuff_set_info_t setinfo;

		setinfo.written_byte = cmd_seqheader->input.bitstream_size;
		setinfo.is_flush = 0;
		//dlog_info("SEQ_HEADER written : %d, flush:%d", setinfo.written_byte, setinfo.is_flush);
		ret = vmgr_ringbuffer_setinfo(mgr_ctx, cmd, drv_info, &setinfo);
		if (ret != VPU_RETCODE_SUCCESS) {
			err_info("failed to setinfo in ringbuffer mode, ret:%d", ret);
		}
#endif //defined(ENABLE_RINGBUFFER_MODE)
	}

	return ret;
}

static int vmgr_dec_reg_framebuffer_preproc(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd, vpu_drv_info_t *drv_info)
{
	int ret = 0;

	if (drv_info->enable_user_register_framebuffer == 1U) {

	} else {
		err_info("User framebuffer registration is not enabled, but attempting to register framebuffer from user");
		ret = -1;
	}

	return ret;
}


static int vmgr_dec_decode_preproc(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd, vpu_drv_info_t *drv_info)
{
	int ret = 0;

	vdec_v3_decode_t *cmd_decode = (vdec_v3_decode_t *)cmd->args;

#if defined(ENABLE_AUTO_FRAMESKIP)
	//frame skip control
	if (cmd_decode->input.skip_mode == VPU_FRAMESKIP_AUTO) {
		//auto frameskip
		detail_info("internal_skip_mode:%d", drv_info->internal_skip_mode);

		if (drv_info->internal_skip_mode == VPU_FRAMESKIP_DISABLED) {
			cmd_decode->input.skip_mode = (int)VPU_FRAMESKIP_DISABLED;
		} else if (drv_info->internal_skip_mode == VPU_FRAMESKIP_NON_I) {
			cmd_decode->input.skip_mode = (int)VPU_FRAMESKIP_NON_I;
		} else if (drv_info->internal_skip_mode == VPU_FRAMESKIP_B) {
			cmd_decode->input.skip_mode = (int)VPU_FRAMESKIP_B;
		} else {
			cmd_decode->input.skip_mode = (int)VPU_FRAMESKIP_DISABLED;
		}

		dlog_info("input.skip_mode:%d", cmd_decode->input.skip_mode);
	} else {
		//user manually control
		drv_info->internal_skip_mode = VPU_FRAMESKIP_DISABLED;
		dlog_info("internal_skip_mode:%d", drv_info->internal_skip_mode);
	}
#endif //defined(ENABLE_AUTO_FRAMESKIP)

	if (drv_info->enabled_ringbuffer_mode == 1U) {
#if defined(ENABLE_RINGBUFFER_MODE)
		vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;
		vdec_v3_ringbuff_set_info_t setinfo;
		int vpu_bitstream_size;
		vpu_addr_t bistream_end_addr = 0;

		setinfo.written_byte = cmd_decode->input.bitstream_size;
		setinfo.is_flush = 0;

		dlog_info("decode input size:%d", setinfo.written_byte);

		if (setinfo.written_byte > 0) {
			//dlog_info("DECODE written : %d(%d + %d), flush:%d", setinfo.written_byte, cmd_decode->input.bitstream_size, cmd_decode->input.bitstream_size_2, setinfo.is_flush);
			ret = vmgr_ringbuffer_setinfo(mgr_ctx, cmd, drv_info, &setinfo);
			if (ret != VPU_RETCODE_SUCCESS) {
				detail_info("failed to setinfo in ringbuffer mode, ret:%d", ret);
				detail_info("DECODE written : %d, flush:%d", setinfo.written_byte, setinfo.is_flush);
			}

			vpu_bitstream_size = alloc_info->bitstream_buf.size - alloc_info->bitstream_safearea_size; //vpu known bitstream buffer size
			bistream_end_addr = alloc_info->bitstream_buf.addr[VPU_PA] + vpu_bitstream_size; //vpu kneown end address
			dlog_info("vpu known, bitstream_size:%d, end addr:0x%llx", (alloc_info->bitstream_buf.size - alloc_info->bitstream_safearea_size), bistream_end_addr);

			//if bitstream buffer in save area, copy to begins of bitstream buffer with that size
			if ((cmd_decode->input.bitstream_addr[VPU_PA] + cmd_decode->input.bitstream_size) > bistream_end_addr) {
				int overflow_size;
				int offset;

				overflow_size = (cmd_decode->input.bitstream_addr[VPU_PA] + cmd_decode->input.bitstream_size) - bistream_end_addr;
				offset = cmd_decode->input.bitstream_size - overflow_size;
				detail_info("overflow_size:%d, offset:%d, src addr[KVA]:0x%llx", overflow_size, offset, (cmd_decode->input.bitstream_addr[VPU_KVA] + offset));

				//Copy to the front of the buffer
				memcpy((void *)(uintptr_t)alloc_info->bitstream_buf.addr[VPU_KVA], (void *)(uintptr_t)(cmd_decode->input.bitstream_addr[VPU_KVA] + offset), overflow_size);
				detail_info("overflow copy done");
			}
		} else {
			dlog_info("input size 0");
		}
#endif //defined(ENABLE_RINGBUFFER_MODE)
	}

	return ret;
}

static int vmgr_dec_flush_preproc(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd, vpu_drv_info_t *drv_info)
{
	int ret = 0;

	vmgr_list_flush_result(&mgr_ctx->comm_data, cmd->drv_id, cmd->op_type);

	return ret;
}

static int vmgr_decode_pre_proc(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd, vpu_drv_info_t *drv_info)
{
	int ret = 0;

	switch (cmd->cmd_type) {
	case VPU_CMD_DEC_INIT:
	{
		vmgr_dec_init_preproc(mgr_ctx, cmd, drv_info);
	}
	break;

	case VPU_CMD_DEC_SEQ_HEADER:
	{
		vmgr_dec_seqhead_preproc(mgr_ctx, cmd, drv_info);
	}
	break;

	case VPU_CMD_DEC_REG_USER_FRAME_BUFFER:
	{
		vmgr_dec_reg_framebuffer_preproc(mgr_ctx, cmd, drv_info);
	}
	break;

	case VPU_CMD_DEC_DECODE:
	{
		vmgr_dec_decode_preproc(mgr_ctx, cmd, drv_info);
	}
	break;

	case VPU_CMD_DEC_FLUSH:
	{
		vmgr_dec_flush_preproc(mgr_ctx, cmd, drv_info);
	}
	break;

	default:
		ret = 0;
	break;
	}

	return ret;
}

static int vmgr_dec_init_postproc(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;

	if (cmd->result != VPU_RETCODE_SUCCESS) {
		if (cmd->result != VPU_RETCODE_CODEC_EXIT) {
			vetc_dump_reg_all(mgr_ctx->base_addr, "dec init failure");
		}
	}

	if (cmd->result != VPU_RETCODE_CODEC_EXIT) {
		vdec_v3_init_t *cmd_init = (vdec_v3_init_t *)cmd->args;

		drv_info->opened = true;

#if defined(ENABLE_RINGBUFFER_MODE)
		if (cmd_init->output.is_ringbuffer_mode == 1U) {
			drv_info->enabled_ringbuffer_mode = 1U;
		}
#endif

		if (drv_info->enabled_ringbuffer_mode == 1U) {
#if defined(ENABLE_RINGBUFFER_MODE)
			vdec_v3_ringbuff_get_info_t getinfo;

			ret = vmgr_ringbuffer_getinfo(mgr_ctx, cmd, drv_info, &getinfo);
			if (ret == 0) {
				int writable_size;
				int offset;

				dlog_info("ringbuffer get, avail:%lu, read pa:0x%x, write pa:0x%x", getinfo.available_space, getinfo.read_physical_addr, getinfo.write_physical_addr);

				//calculating write address
				offset = getinfo.write_physical_addr - alloc_info->bitstream_buf.addr[VPU_PA]; //bitstream_buf_addr : start of bitstream buffer address

				//calculating empty size
				writable_size = alloc_info->bitstream_buf.size - offset;

				dlog_info("ringbuffer set next, offset:%d, writable_size:%d", offset, writable_size);

				cmd_init->output.next_bitstream_buf_addr[VPU_PA] = getinfo.write_physical_addr;
				cmd_init->output.next_bitstream_buf_addr[VPU_KVA] = alloc_info->bitstream_buf.addr[VPU_KVA] + offset; //including safe area size
				cmd_init->output.next_bitstream_buf_size = writable_size;

				dlog_info("addr:0x%x, size:%d", cmd_init->output.next_bitstream_buf_addr[VPU_PA], cmd_init->output.next_bitstream_buf_size);
			}
#endif //defined(ENABLE_RINGBUFFER_MODE)
		} else {
			cmd_init->output.next_bitstream_buf_addr[VPU_PA] = alloc_info->bitstream_buf.addr[VPU_PA];
			cmd_init->output.next_bitstream_buf_addr[VPU_KVA] = alloc_info->bitstream_buf.addr[VPU_KVA];
			cmd_init->output.next_bitstream_buf_size = alloc_info->bitstream_buf.size;
		}

		cmd_init->output.userdata_buf_addr[VPU_PA] = alloc_info->userdata_buf.addr[VPU_PA];
		cmd_init->output.userdata_buf_addr[VPU_KVA] = alloc_info->userdata_buf.addr[VPU_KVA];
		cmd_init->output.userdata_buf_size = alloc_info->userdata_buf.size;

		dlog_info("Init Done with ret(0x%x), bitstream pa:0x%llx, kva:0x%llx, size:%d, buffer index:%d, userdata pa:0x%llx, kva:0x%llx, size:%d, ringbuffer mode:%u",
				cmd->result,
				cmd_init->output.next_bitstream_buf_addr[VPU_PA], cmd_init->output.next_bitstream_buf_addr[VPU_KVA],
				alloc_info->size_of_bitstream_buffer, alloc_info->bitstream_buffer_index,
				cmd_init->output.userdata_buf_addr[VPU_PA], cmd_init->output.userdata_buf_addr[VPU_KVA], cmd_init->output.userdata_buf_size,
				cmd_init->output.is_ringbuffer_mode);
	} else {
		vmem_proc_free_memory(cmd->pmap_type);
	}

	return ret;
}

static int vmgr_dec_update_bitstream_buffer(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd, vpu_drv_info_t *drv_info)
{
	int ret = 0;

	vdec_v3_seqheader_t *cmd_seqheader = (vdec_v3_seqheader_t *)cmd->args;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;

	if (drv_info->enabled_ringbuffer_mode == 1U) {
#if defined(ENABLE_RINGBUFFER_MODE)
		vdec_v3_ringbuff_get_info_t getinfo;

		ret = vmgr_ringbuffer_getinfo(mgr_ctx, cmd, drv_info, &getinfo);
		if (ret == 0) {
			int writable_size;
			//int writable_size_2; //from the front address
			int offset;
			int empty;

			// bitstream addr	write py addr
			// |----------------------------|-------| bitstream buffer
			//		  offset
			offset = getinfo.write_physical_addr - alloc_info->bitstream_buf.addr[VPU_PA];
			empty = alloc_info->bitstream_buf.size - offset;

			dlog_info("ringbuffer get, empty:%d, avail:%lu, read pa:0x%x, write pa:0x%x",
					empty, getinfo.available_space, getinfo.read_physical_addr, getinfo.write_physical_addr);


			if (getinfo.write_physical_addr >= getinfo.read_physical_addr) {
				//	   vpu read    write	 safe
				// |----------------------|-------| bitstream buffer
				//					 |------------| writable size
				writable_size = alloc_info->bitstream_buf.size - offset; //including safe area size
				dlog_info("write > read, writable_size:%d, write - read:%d", writable_size,  getinfo.write_physical_addr - getinfo.read_physical_addr);
			} else {
				//	  write 	 vpu read	 safe
				// |----------------------|-------|  bitstream buffer
				//		 |-----------| writable size
				writable_size = getinfo.available_space;

				//check is equal -> getinfo.available_space == (getinfo.read_physical_addr - getinfo.write_physical_addr)
				dlog_info("write < read, writable_size:%d, read - write:%d", writable_size, (getinfo.read_physical_addr - getinfo.write_physical_addr));
			}

			//if write ptr is behind, SAFE_AREA_SIZE must be added to output bitstream buffer size

			cmd_seqheader->output.next_bitstream_buf_addr[VPU_PA] = getinfo.write_physical_addr;
			cmd_seqheader->output.next_bitstream_buf_addr[VPU_KVA] = alloc_info->bitstream_buf.addr[VPU_KVA] + offset;
			cmd_seqheader->output.next_bitstream_buf_size = writable_size;

			dlog_info("seq header output, next bitstream addr:0x%x, size:%d", cmd_seqheader->output.next_bitstream_buf_addr[VPU_PA], cmd_seqheader->output.next_bitstream_buf_size);
		}
#endif //#if defined(ENABLE_RINGBUFFER_MODE)
	} else {
		int offset = 0;
		offset = vmgr_decode_get_bitstream_offset_update_index(alloc_info);

		cmd_seqheader->output.next_bitstream_buf_addr[VPU_PA] = alloc_info->bitstream_buf.addr[VPU_PA] + offset;
		cmd_seqheader->output.next_bitstream_buf_addr[VPU_KVA] = alloc_info->bitstream_buf.addr[VPU_KVA] + offset;
		cmd_seqheader->output.next_bitstream_buf_size = alloc_info->size_of_bitstream_buffer;

		dlog_info("sequence header init done with ret(0x%x), bitstream pa:0x%llx, kva:0x%llx, size:%d, buffer index:%d",
			cmd->result,
			cmd_seqheader->output.next_bitstream_buf_addr[VPU_PA], cmd_seqheader->output.next_bitstream_buf_addr[VPU_KVA],
			alloc_info->size_of_bitstream_buffer, alloc_info->bitstream_buffer_index);
	}

	return ret;
}

static int vmgr_dec_seqheader_update_buffer_size(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	vdec_v3_seqheader_t *cmd_seqheader = (vdec_v3_seqheader_t *)cmd->args;
	vdec_v3_buffer_size_info_t *size_info = &cmd_seqheader->output.buffer_size_info;
	vpu_ip_module_t *each_ip = mgr_ctx->each_ip;

	size_info->framebuffer_size[VPU_FRAMEBUFFER_Y] = each_ip->proc_get_buffer_size(mgr_ctx, VMGR_BUF_Y, drv_info);
	size_info->framebuffer_size[VPU_FRAMEBUFFER_CB] = each_ip->proc_get_buffer_size(mgr_ctx, VMGR_BUF_CB, drv_info);
	size_info->framebuffer_size[VPU_FRAMEBUFFER_CR] = each_ip->proc_get_buffer_size(mgr_ctx, VMGR_BUF_CR, drv_info);
	size_info->framebuffer_size[VPU_FRAMEBUFFER_MVCOL] = each_ip->proc_get_buffer_size(mgr_ctx, VMGR_BUF_MVCOL, drv_info);
	size_info->framebuffer_size[VPU_FRAMEBUFFER_FBCY] = each_ip->proc_get_buffer_size(mgr_ctx, VMGR_BUF_FBCY, drv_info);
	size_info->framebuffer_size[VPU_FRAMEBUFFER_FBCC] = each_ip->proc_get_buffer_size(mgr_ctx, VMGR_BUF_FBCC, drv_info);
	size_info->framebuffer_size[VPU_FRAMEBUFFER_COMP_Y] = each_ip->proc_get_buffer_size(mgr_ctx, VMGR_BUF_COMP_Y, drv_info);
	size_info->framebuffer_size[VPU_FRAMEBUFFER_COMP_C] = each_ip->proc_get_buffer_size(mgr_ctx, VMGR_BUF_COMP_C, drv_info);

	size_info->framebuffer_ext_size[VPU_FRAMEBUFFER_EXT_AVC_SLICE] = each_ip->proc_get_buffer_size(mgr_ctx, VMGR_BUF_SLICE, drv_info);
	size_info->framebuffer_ext_size[VPU_FRAMEBUFFER_EXT_VP8_MBDATA] =  each_ip->proc_get_buffer_size(mgr_ctx, VMGR_BUF_MBDATA, drv_info);

	detail_info("vmgr_dec_seqheader buffer size Y:%d, CB:%d, CR:%d, MVCOL:%d, FBCY:%d, FBCC:%d, COMP_Y:%d, COMP_C:%d, AVC Slice:%d, VP8 MBData:%d",
				size_info->framebuffer_size[VPU_FRAMEBUFFER_Y],
				size_info->framebuffer_size[VPU_FRAMEBUFFER_CB],
				size_info->framebuffer_size[VPU_FRAMEBUFFER_CR],
				size_info->framebuffer_size[VPU_FRAMEBUFFER_MVCOL],
				size_info->framebuffer_size[VPU_FRAMEBUFFER_FBCY],
				size_info->framebuffer_size[VPU_FRAMEBUFFER_FBCC],
				size_info->framebuffer_size[VPU_FRAMEBUFFER_COMP_Y],
				size_info->framebuffer_size[VPU_FRAMEBUFFER_COMP_C],
				size_info->framebuffer_ext_size[VPU_FRAMEBUFFER_EXT_AVC_SLICE],
				size_info->framebuffer_ext_size[VPU_FRAMEBUFFER_EXT_VP8_MBDATA]);

	return ret;
}

static int vmgr_dec_seqhead_postproc(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	vdec_v3_seqheader_t *cmd_seqheader = (vdec_v3_seqheader_t *)cmd->args;
	vdec_v3_initial_info_t *init_info = &cmd_seqheader->output.initial_info;

	dlog_info("VPU_DEC_SEQ_HEADER post proc");

	//sequence header info
	detail_vmgr("[id:%u] pic %d x %d, frame_rate %u/%u, buffer count min:%d, size:%d, format:%d",
		drv_info->drv_id, init_info->pic_width, init_info->pic_height, init_info->frame_rate_res, init_info->frame_rate_div, init_info->min_frame_buffer_count, init_info->min_frame_buffer_size, init_info->frame_buffer_format);
	detail_vmgr("[id:%u] crop l:%d, r:%d, t:%d, b:%d, delay:%d, profile:%d, level:%d, interlace:%d, aspectratio:%d, bitdepth:%d",
		drv_info->drv_id, init_info->pic_crop.left, init_info->pic_crop.right, init_info->pic_crop.top, init_info->pic_crop.bottom, init_info->frame_buf_delay, init_info->profile, init_info->level, init_info->interlace, init_info->aspectratio, init_info->bitdepth);

	if (drv_info->enable_user_register_framebuffer == 0U) {
		vpu_ip_module_t *each_ip = mgr_ctx->each_ip;
		vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;

		detail_info("vmgr_decode_alloc_framebuffer ---");
		ret = vmgr_decode_alloc_framebuffer(mgr_ctx, drv_info, cmd_seqheader);
		detail_info("vmgr_decode_alloc_framebuffer +++ ret:%d", ret);
		if (ret == 0) {
			//framebuffer register
			detail_info("call VPU_DEC_REG_FRAME_BUFFER");
			cmd->result = each_ip->proc_decode(mgr_ctx, VPU_CMD_DEC_REG_FRAME_BUFFER, cmd, drv_info);
			detail_info("call VPU_DEC_REG_FRAME_BUFFER, result:%d", cmd->result);
			if (cmd->result == VPU_RETCODE_SUCCESS) {
				cmd_seqheader->output.framebuf_addr[VPU_PA] = alloc_info->frame_buf.addr[VPU_PA];
				cmd_seqheader->output.framebuf_addr[VPU_KVA] = alloc_info->frame_buf.addr[VPU_KVA];
				cmd_seqheader->output.framebuf_size = alloc_info->frame_buf.size;
			}
		} else {
			err_info("VPU_DEC_REG_FRAME_BUFFER failed, result:%d", cmd->result);
			cmd->result = VPU_RETCODE_INSUFFICIENT_MEMORY;
		}
	}

	vmgr_dec_update_bitstream_buffer(mgr_ctx, cmd, drv_info);
	(void)vmgr_dec_seqheader_update_buffer_size(mgr_ctx, cmd, drv_info);

#if defined(ENABLE_AUTO_FRAMESKIP)
	//frame skip control for auto skip mode
	drv_info->internal_skip_mode = VPU_FRAMESKIP_NON_I;
#endif //defined(ENABLE_AUTO_FRAMESKIP)

	return ret;
}

static int vmgr_dec_reg_framebuffer_postproc(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	//dlog_info("VPU_CMD_DEC_REG_USER_FRAME_BUFFER post proc");


	return ret;
}

static int vmgr_dec_decode_set_dma_buf_id(vpu_drv_info_t *drv_info, vdec_v3_decode_out_t *dec_output)
{
	int ret = 0;

	VPU_PhyMemInfo_t mem_info;

	mem_info.phys = (unsigned long)dec_output->display_out[VPU_PA][VPU_COMP_Y];
	mem_info.size = 0UL;

	switch (drv_info->dec_init_info.output_format) {
	case VPU_OUTPUT_LINEAR_YUV420:
		if (drv_info->initial_info.bitdepth == 8) {
			mem_info.size = (unsigned long)(dec_output->out_info.dma_buf_align_width * dec_output->out_info.dma_buf_align_height * 3) / 2;
		} else {
			ret = -1;
		}
	break;

	case VPU_OUTPUT_LINEAR_NV12:
		if (drv_info->initial_info.bitdepth == 8) {
			mem_info.size = (unsigned long)(dec_output->out_info.dma_buf_align_width * dec_output->out_info.dma_buf_align_height) +
				((dec_output->out_info.dma_buf_align_width/2) * (dec_output->out_info.dma_buf_align_height/2));
		} else {
			ret = -1;
		}
	break;

	case VPU_OUTPUT_LINEAR_10_TO_8_BIT_YUV420:
		mem_info.size = (unsigned long)(dec_output->out_info.dma_buf_align_width * dec_output->out_info.dma_buf_align_height * 3) / 2;
	break;

	case VPU_OUTPUT_LINEAR_10_TO_8_BIT_NV12:
		mem_info.size = (unsigned long)(dec_output->out_info.dma_buf_align_width * dec_output->out_info.dma_buf_align_height) +
				((dec_output->out_info.dma_buf_align_width/2) * (dec_output->out_info.dma_buf_align_height/2));
	break;

	case VPU_OUTPUT_COMPRESSED_MAPCONV:
		ret = -1;
	break;

	case VPU_OUTPUT_COMPRESSED_AFBC:
		ret = -1;
	break;

	default:
		ret = -1;
	break;
	}

	mem_info.fd = -1;
	if (mem_info.size != 0) {
		ret = tcc_mem_create_dma_buf(&mem_info);
		if (ret == 0) {
			dec_output->dma_buf_id = mem_info.fd;
		}
	}

	return ret;
}

int vmgr_dec_decode_update_bitstream_addr(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	vdec_v3_decode_t *cmd_decode = (vdec_v3_decode_t *)cmd->args;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;

	if (drv_info->enabled_ringbuffer_mode == 1U) {
#if defined(ENABLE_RINGBUFFER_MODE)
		vdec_v3_ringbuff_get_info_t getinfo;

		ret = vmgr_ringbuffer_getinfo(mgr_ctx, cmd, drv_info, &getinfo);
		if (ret == 0) {
			int writable_size;
			//int writable_size_2; //from the front address
			int offset;
			int empty;

			// bitstream addr	write py addr
			// |----------------------------|-------| bitstream buffer
			//		  offset
			offset = getinfo.write_physical_addr - alloc_info->bitstream_buf.addr[VPU_PA];
			empty = alloc_info->bitstream_buf.size - offset;

			dlog_info("ringbuffer get, empty:%d, avail:%lu, read pa:0x%x, write pa:0x%x",
					empty, getinfo.available_space, getinfo.read_physical_addr, getinfo.write_physical_addr);


			if (getinfo.write_physical_addr >= getinfo.read_physical_addr) {
				//	   vpu read    write	 safe
				// |----------------------|-------| bitstream buffer
				//					 |------------| writable size
				writable_size = alloc_info->bitstream_buf.size - offset; //including safe area size
				dlog_info("write > read, writable_size:%d, write - read(buffered):%d", writable_size,  getinfo.write_physical_addr - getinfo.read_physical_addr);
			} else {
				//	  write 	 vpu read	 safe
				// |----------------------|-------|  bitstream buffer
				//		 |-----------| writable size
				writable_size = getinfo.available_space;

				//check is equal -> getinfo.available_space == (getinfo.read_physical_addr - getinfo.write_physical_addr)
				dlog_info("write < read, writable_size:%d, read - write:%d", writable_size, (getinfo.read_physical_addr - getinfo.write_physical_addr));
			}

			cmd_decode->output.next_bitstream_buf_addr[VPU_PA] = getinfo.write_physical_addr;
			cmd_decode->output.next_bitstream_buf_addr[VPU_KVA] = alloc_info->bitstream_buf.addr[VPU_KVA] + offset;
			cmd_decode->output.next_bitstream_buf_size = writable_size;

			dlog_info("decode output, next bitstream addr:0x%x, size:%d", cmd_decode->output.next_bitstream_buf_addr[VPU_PA], cmd_decode->output.next_bitstream_buf_size);
		} else {
			err_info("vmgr_ringbuffer_getinfo error, ret:%d, handle:0x%x", ret, drv_info->handle);
		}
#endif //defined(ENABLE_RINGBUFFER_MODE)
	} else {
		int offset = 0;

		offset = vmgr_decode_get_bitstream_offset_update_index(alloc_info);

		cmd_decode->output.next_bitstream_buf_addr[VPU_PA] = alloc_info->bitstream_buf.addr[VPU_PA] + offset;
		cmd_decode->output.next_bitstream_buf_addr[VPU_KVA] = alloc_info->bitstream_buf.addr[VPU_KVA] + offset;
		cmd_decode->output.next_bitstream_buf_size = alloc_info->size_of_bitstream_buffer;

		dlog_info("linear set next to beins of buffer:0x%x, end:0x%x, size:%d, index:%d, nb buffer:%d",
			cmd_decode->output.next_bitstream_buf_addr[VPU_PA],
			alloc_info->bitstream_buf.addr[VPU_PA] + alloc_info->bitstream_buf.size,
			cmd_decode->output.next_bitstream_buf_size,
			alloc_info->bitstream_buffer_index,
			alloc_info->num_of_bitstream_buffers
			);
	}

	return ret;
}

static int vmgr_dec_decode_postproc(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	vdec_v3_decode_t *cmd_decode = (vdec_v3_decode_t *)cmd->args;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;

#if defined(ENABLE_INTERLACE_DELAY_PROCESS)
	if (drv_info->enable_interlace_delay_proc == 1U) {
		bool detecting_field;
		detecting_field = drv_info->enabled_ringbuffer_mode == 1U
							? (cmd_decode->result == VPU_RETCODE_INFO_INSUFFICIENT_DATA)
							: ((cmd_decode->result == VPU_RETCODE_SUCCESS) && (cmd_decode->output.out_info.decoded_status == VPU_DEC_STAT_SUCCESS_FIELD_PICTURE));


		if (detecting_field == true) {
			if (drv_info->detected_interlace == 0U) {
				//enable_avoid_pending starts only after the VPU_DEC_STAT_SUCCESS, set only once
				drv_info->detected_interlace = 1U;

				alloc_info->num_of_bitstream_buffers = 3; //number of buffers used for interlace processing
				dlog_info("change number of bitstream buffers:%d for interlace processing", alloc_info->num_of_bitstream_buffers);

				alloc_info->size_of_bitstream_buffer = alloc_info->bitstream_buf.size / alloc_info->num_of_bitstream_buffers;
				alloc_info->size_of_bitstream_buffer = ALIGNED_BUFF(alloc_info->size_of_bitstream_buffer, 4096u);
			}

			if (mgr_ctx->enable_avoid_pending == 1U) {
				mgr_ctx->pending_drv_id = drv_info->drv_id;
				mgr_ctx->field_processed_timestamp = jiffies;
				dlog_info("set interlace pending, id:%d, timestamp:%lu", mgr_ctx->pending_drv_id, mgr_ctx->field_processed_timestamp);

				if (cmd->drv_info->temp_decoding_result != NULL) {
					//replace output
					vpu_cmd_t *temp_cmd = (vpu_cmd_t *)cmd->drv_info->temp_decoding_result;
					vdec_v3_decode_t *tmp_result = (vdec_v3_decode_t *)temp_cmd->args;

					dlog_info("tmp result id:%d, replace output", temp_cmd->drv_id);

					vetc_memcpy(&cmd_decode->output, &tmp_result->output, sizeof(vdec_v3_decode_out_t), 0);

					//delete cmd
					VPU_free(temp_cmd->args);
					VPU_free(temp_cmd);
					cmd->drv_info->temp_decoding_result = NULL;
				}
			}
		} else {
			if (drv_info->detected_interlace == 1U) {
				if (mgr_ctx->enable_avoid_pending == 0U) {
					drv_info->enable_avoid_pending = 1U;
					mgr_ctx->enable_avoid_pending = 1U;
					dlog_info("drv_id:%d set avoid pending enable", cmd->drv_id);
				}
			}

			//mgr_ctx->pending_drv_id = INVALID_DRV_ID;
			dlog_info("frame decoding done, id:%d", cmd->drv_id);
		}
	}
#endif

	ret = vmgr_dec_decode_update_bitstream_addr(mgr_ctx, cmd, drv_info);

	cmd_decode->output.dma_buf_id = -1;

	if (drv_info->dec_init_info.enable_dma_buf_id == 1U) {
		(void)vmgr_dec_decode_set_dma_buf_id(drv_info, &cmd_decode->output);
	}

	dlog_info("decode done with ret(0x%x), next bit_addr pa:0x%x/0x%x, buf_size:%d, buf_idx:%d, dma_id:%d, idx:%d/%d, status:%d/%d, resoultion:%d x %d, crop:%d,%d - %d,%d, interlace:%d",
				cmd->result,
				cmd_decode->output.next_bitstream_buf_addr[VPU_PA], cmd_decode->output.next_bitstream_buf_addr[VPU_KVA],
				alloc_info->size_of_bitstream_buffer, alloc_info->bitstream_buffer_index, cmd_decode->output.dma_buf_id,
				cmd_decode->output.out_info.display_idx, cmd_decode->output.out_info.decoded_idx,
				cmd_decode->output.out_info.display_status, cmd_decode->output.out_info.decoded_status,
				cmd_decode->output.out_info.display_width, cmd_decode->output.out_info.display_height,
				cmd_decode->output.out_info.display_crop.left, cmd_decode->output.out_info.display_crop.top,
				cmd_decode->output.out_info.display_crop.right, cmd_decode->output.out_info.display_crop.bottom,
				cmd_decode->output.out_info.interlaced_frame);

	if ((drv_info->display_width != 0) && (drv_info->display_height != 0) &&
		(cmd_decode->output.out_info.display_width != 0) && (cmd_decode->output.out_info.display_height != 0) &&
		((drv_info->display_width != cmd_decode->output.out_info.display_width) ||
			(drv_info->display_height != cmd_decode->output.out_info.display_height))) {
		dlog_info("------------------------------------------------");
		dlog_info("resolution changed %d x %d  ->  %d x %d",
				drv_info->display_width, drv_info->display_height,
				cmd_decode->output.out_info.display_width, cmd_decode->output.out_info.display_height);
		dlog_info("------------------------------------------------");
	}

	drv_info->display_width = cmd_decode->output.out_info.display_width;
	drv_info->display_height = cmd_decode->output.out_info.display_height;

#if defined(ENABLE_AUTO_FRAMESKIP)
	//frameskip control
	if ((cmd_decode->output.out_info.decoded_idx >= 0) &&
		(cmd_decode->output.out_info.decoded_status == VPU_DEC_STAT_SUCCESS ||
		cmd_decode->output.out_info.decoded_status == VPU_DEC_STAT_SUCCESS_FIELD_PICTURE)) {
		if (drv_info->internal_skip_mode == VPU_FRAMESKIP_NON_I) {
			drv_info->internal_skip_mode = VPU_FRAMESKIP_B;
			detail_info("set next auto skip mode:%d", drv_info->internal_skip_mode);
		} else if (drv_info->internal_skip_mode == VPU_FRAMESKIP_B) {
			drv_info->internal_skip_mode = VPU_FRAMESKIP_DISABLED;
			detail_info("set next auto skip mode:%d", drv_info->internal_skip_mode);
		}
	}
#endif //defined(ENABLE_AUTO_FRAMESKIP)

	if (ret != 0) {
		err_info("ret:%d", ret);
	}
	//dlog_info("resolution %d x %d",	cmd_decode->output.out_info.display_width, cmd_decode->output.out_info.display_height);
	return ret;
}

static int vmgr_dec_buf_clear_postproc(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	vdec_v3_buf_clear_t *cmd_buf_clear = (vdec_v3_buf_clear_t *)cmd->args;

	if (drv_info->dec_init_info.enable_dma_buf_id == 1U) {
		if (cmd_buf_clear->dma_buf_id >= 0) {
			(void)tcc_mem_release_dma_buf(cmd_buf_clear->dma_buf_id);
		} else {
			dlog_info("invalid dma_buf_id:%d, clear index:%d", cmd_buf_clear->dma_buf_id, cmd_buf_clear->index);
		}
	}

	return ret;
}

static int vmgr_dec_flush_postproc(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd, vpu_drv_info_t *drv_info)
{
	int ret = 0;

#if defined(ENABLE_AUTO_FRAMESKIP)
	//frame skip control for auto skip mode
	drv_info->internal_skip_mode = VPU_FRAMESKIP_NON_I;
#endif //defined(ENABLE_AUTO_FRAMESKIP)

#if defined(ENABLE_INTERLACE_DELAY_PROCESS)
	cmd->drv_info->initial_wakeup_poll = 0U;

	//flush delay queue, if enabled
	vmgr_list_flush_sync(cmd->drv_info->delay_queue);
#endif

	return ret;
}

static int vmgr_decode_post_proc(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd, vpu_drv_info_t *drv_info)
{
	int ret = 0;

	switch (cmd->cmd_type) {
	case VPU_CMD_DEC_INIT:
	{
		ret = vmgr_dec_init_postproc(mgr_ctx, cmd, drv_info);
	}
	break;

	case VPU_CMD_DEC_SEQ_HEADER:
	{
		ret = vmgr_dec_seqhead_postproc(mgr_ctx, cmd, drv_info);
	}
	break;

	case VPU_CMD_DEC_REG_USER_FRAME_BUFFER:
	{
		ret = vmgr_dec_reg_framebuffer_postproc(mgr_ctx, cmd, drv_info);
	}
	break;

	case VPU_CMD_DEC_DECODE:
	{
		ret = vmgr_dec_decode_postproc(mgr_ctx, cmd, drv_info);
	}
	break;

	case VPU_CMD_DEC_BUF_FLAG_CLEAR:
	{
		ret = vmgr_dec_buf_clear_postproc(mgr_ctx, cmd, drv_info);
	}
	break;

	case VPU_CMD_DEC_FLUSH:
	{
		ret = vmgr_dec_flush_postproc(mgr_ctx, cmd, drv_info);
	}
	break;

	case VPU_CMD_DEC_CLOSE:
	{
		drv_info->handle = 0x00;
		drv_info->opened = false;
		(void)vmem_proc_free_memory(cmd->drv_id);
		(void)vmgr_release_ip_parameter(drv_info);
	}
	break;

	default:
	break;
	}

	return ret;
}

int vmgr_decode_proc(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd)
{
	int ret = 0;
	vpu_ip_module_t *each_ip = mgr_ctx->each_ip;
	vpu_drv_info_t *drv_info = cmd->drv_info;

	dlog_info("decode proc - cmd type:%s(%d)", vmgr_cmd_name(cmd->cmd_type), cmd->cmd_type);

	if (cmd->cmd_type != VPU_CMD_DEC_INIT) {
		if ((drv_info->opened == false)  || (drv_info->handle == 0x00)) {
			cmd->result = VPU_RETCODE_MULTI_CODEC_EXIT_TIMEOUT;
			ret = -1;
		}
	}

	detail_info("decode proc - proc_decode:%p, result:%d, pmap_type:%d", each_ip->proc_decode, cmd->result, cmd->pmap_type);

	if ((each_ip->proc_decode != NULL) && (cmd->result != VPU_RETCODE_MULTI_CODEC_EXIT_TIMEOUT)) {
		//pre-processing
		ret = vmgr_decode_pre_proc(mgr_ctx, cmd, drv_info);
		detail_info("vmgr_decode_pre_proc, ret:%d", ret);

		if (ret == 0) {
			detail_info("- cmd:%s", vmgr_cmd_name(cmd->cmd_type));

			cmd->result = each_ip->proc_decode(mgr_ctx, cmd->cmd_type, cmd, drv_info);

			detail_info("+ cmd:%s, ret:%d", vmgr_cmd_name(cmd->cmd_type), cmd->result);

			//post-processing
			ret = vmgr_decode_post_proc(mgr_ctx, cmd, drv_info);
		}
	}

	if (ret != 0) {
		cmd->result = VPU_RETCODE_FAILURE;
	}

	dlog_info("decode proc - cmd type:%s(%d), ret:%d", vmgr_cmd_name(cmd->cmd_type), cmd->cmd_type, ret);

	return ret;
}
