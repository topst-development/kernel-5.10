/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#include "vpu_comm.h"

#if DEFINED_CONFIG_VDEC

#include "vpu_rm.h"
#include "vpu_dec.h"
#include "vpu_mgr_context.h"
#include "vpu_mgr_common.h"

#define dlog_vdec(msg...)		V_DBG(VPU_DBG_INFO, "[VPU_DEC][INFO]:" msg)
#define detail_vdec(msg...)		V_DBG(VPU_DBG_DETAIL, "[VPU_DEC][DETAIL]:" msg)
#define seq_vdec(msg...)		V_DBG(VPU_DBG_SEQUENCE, "[VPU_DEC][SEQ]: " msg)
#define err_vdec(msg...)		V_DBG(VPU_DBG_ERROR, "[VPU_DEC][ERR]:" msg)

#define DBG(fmt, args...)	do { pr_err_vdec("[DEC][%s:%d]" fmt "\n", __func__, __LINE__, ## args); } while (0)

static bool decoder_map[VPU_DRV_ID_MAX] = {false, };

//for debugging

static const char *vpu_dec_ioctl_invalid = "VPU_INVALID_COMMAND";
static const char *vpu_dec_com_ioctl_name[] = {
	"VPU_V3_COM_BASE",
	"VPU_V3_GET_DRV_VERSION_SYNC",
	"VPU_V3_GET_CAPABILITY_SYNC",
};

static const char *vpu_dec_ioctl_name[] = {
	"VDEC_V3_BASE", //invalid
	"VDEC_V3_INIT",
	"VDEC_V3_INIT_RESULT",
	"VDEC_V3_SEQ_HEADER",
	"VDEC_V3_SEQ_HEADER_RESULT",
	"VDEC_V3_DECODE",
	"VDEC_V3_DECODE_RESULT",
	"VDEC_V3_BUF_CLEAR",
	"VDEC_V3_BUF_CLEAR_RESULT",
	"VDEC_V3_DRAIN",
	"VDEC_V3_DRAIN_RESULT",
	"VDEC_V3_FLUSH",
	"VDEC_V3_FLUSH_RESULT",
	"VDEC_V3_CLOSE",
	"VDEC_V3_CLOSE_RESULT",
	"VDEC_V3_REG_FRAMEBUFFER",
	"VDEC_V3_REG_FRAMEBUFFER_RESULT",
	"VDEC_V3_GET_NEXT_RESULT_SYNC",
	"VDEC_V3_ALLOC_MEMORY_SYNC",
};

//vpu_internal_type.h
//enum vpu_cmd_type to ioctl
static const int vdec_opcode_to_ioctl_map[] = {
	VDEC_V3_INIT, //VPU_CMD_DEC_INIT
	VDEC_V3_SEQ_HEADER, //VPU_CMD_DEC_SEQ_HEADER
	-1, //VPU_CMD_DEC_GET_INFO
	VDEC_V3_REG_FRAMEBUFFER, //VPU_CMD_DEC_REG_FRAME_BUFFER
	-1, //VPU_CMD_DEC_GET_OUTPUT_INFO
	VDEC_V3_DECODE, //VPU_CMD_DEC_DECODE
	VDEC_V3_BUF_CLEAR, //VPU_CMD_DEC_BUF_FLAG_CLEAR
	VDEC_V3_FLUSH, //VPU_CMD_DEC_FLUSH
	VDEC_V3_DRAIN, //VPU_CMD_DEC_DRAIN
	-1, //VPU_CMD_DEC_RING_GET_INFO
	-1, //VPU_CMD_DEC_RING_SET_INFO
	VDEC_V3_CLOSE, //VPU_CMD_DEC_CLOSE
};

static const char *vdec_ioctl_name(const int cmd)
{
	const char *ret_name = NULL;

	if ((cmd > VPU_V3_COM_BASE) && (cmd <= VPU_V3_GET_CAPABILITY_SYNC)) {
		ret_name = vpu_dec_com_ioctl_name[cmd - VPU_V3_COM_BASE];
	} else if ((cmd > VDEC_V3_BASE) && (cmd <= VDEC_V3_ALLOC_MEMORY_SYNC)) {
		ret_name = vpu_dec_ioctl_name[cmd - VDEC_V3_BASE];
	} else {
		ret_name = vpu_dec_ioctl_invalid;
	}

	return ret_name;
}

static bool vdec_is_from_kernel(vpu_dec_drv_t *vdata)
{
	bool from_kernel = false;

	if (vdata->info.is_kernel_call == true) {
		from_kernel = true;
	}

	return from_kernel;
}

static int vdec_add_list(vpu_dec_drv_t *vdata, enum vpu_cmd_type cmd, void *args)
{
	int ret = 0;
	vpu_cmd_t *node = NULL;

	node = vmgr_list_manager_alloc(vdata->mgr_ctx);
	if (node != NULL) {
		node->op_type = vdata->info.op_type;
		node->drv_id = vdata->info.drv_id;
		node->pmap_type = (vputype)vdata->info.pmap_type;
		node->cmd_type = cmd;
		node->cmd_id = vdata->command_id;

		node->handle = vdata->info.handle;
		node->args = args;
		node->result = VPU_RETCODE_SUCCESS;
		node->poll_data = &vdata->drv_poll_data;
		node->drv_info = &vdata->info;

		vdata->command_id++;
		if (vdata->command_id > MAX_COMMAND_ID) {
			vdata->command_id = 0;
		}

		ret = vmgr_list_manager_add(vdata->mgr_ctx, node);
	} else {
		V_DBG(VPU_DBG_ERROR, "dec-id:%u, cmd node allocation failed!", vdata->info.drv_id);
		ret = (int)-ENOMEM;
	}

	return ret;
}

static void *vdec_cmd_alloc_copy(vpu_dec_drv_t *vdata, void *arg, size_t copySize)
{
	int ret = 0;
	void *pArgs = NULL;

	if ((copySize > 0) && (arg != NULL)) {
		pArgs = VPU_alloc(copySize);
		if (pArgs != NULL) {
			if (vdec_is_from_kernel(vdata) == true) {
				vetc_memcpy(pArgs, arg, copySize, 0);
			} else {
				if (copy_from_user(pArgs, arg, copySize) != 0U) {
					ret = (int)-EFAULT;
				}
			}
		} else {
			ret = (int)-ENOMEM;
		}

		if (ret != 0) {
			if (pArgs != NULL) {
				VPU_free(pArgs);
				pArgs = NULL;
			}
		}
	}

	return pArgs;
}

static int vdec_cmd_put(vpu_dec_drv_t *vdata, enum vpu_cmd_type cmd, void *arg, size_t copySize)
{
	int ret = 0;
	void *pArgs = NULL;

	pArgs = vdec_cmd_alloc_copy(vdata, arg, copySize);
	if (pArgs != NULL) {
		ret = vdec_add_list(vdata, cmd, pArgs);
	} else {
		ret = -1;
	}

	return ret;
}

//FIXME : add copySize to cmd_result member variable
static vpu_cmd_t *vdec_cmd_get_result(vpu_dec_drv_t *vdata, size_t copySize, enum vpu_cmd_type match_cmd_type)
{
	vpu_cmd_t *cmd_result = NULL;

	cmd_result = vmgr_list_manager_get_result(vdata->mgr_ctx, vdata->info.op_type, vdata->info.drv_id);
	if (cmd_result != NULL) {
		//unmatched command
		if (cmd_result->cmd_type != match_cmd_type) {
			V_DBG(VPU_DBG_ERROR, "dec_id:%u, unmatched cmd, result_type:(%s)%d, match_type:(%s)%d", vdata->info.drv_id, vmgr_cmd_name(cmd_result->cmd_type), cmd_result->cmd_type, vmgr_cmd_name(match_cmd_type), match_cmd_type);
		} else {
			if (cmd_result->args != NULL) {
				//set result
				*((enum vpu_return_code *)cmd_result->args) = cmd_result->result;
				if (cmd_result->result != VPU_RETCODE_SUCCESS && cmd_result->result != RETCODE_FRAME_NOT_COMPLETE) {
					V_DBG(VPU_DBG_ERROR, "dec_id:%u,  cmd:%s, result:%s (%d)", vdata->info.drv_id, vmgr_cmd_name(cmd_result->cmd_type), vmgr_get_return_name(cmd_result->result), cmd_result->result);
				}
			}
		}
	} else {
		V_DBG(VPU_DBG_ERROR, "dec_id:%u,  error, no result for %s(%d)", vdata->info.drv_id, vmgr_cmd_name(match_cmd_type), match_cmd_type);
	}

	return cmd_result;
}

static int vdec_cmd_result_to_user(vpu_dec_drv_t *vdata, void *arg, vpu_cmd_t *cmd_result, size_t copySize)
{
	int ret = 0;

	if (vdec_is_from_kernel(vdata) == true) {
		vetc_memcpy(arg, cmd_result->args, copySize, 0);
	} else {
		if (copy_to_user(arg, cmd_result->args, copySize) != 0U) {
			ret = (int)-EFAULT;
			V_DBG(VPU_DBG_ERROR, "dec_id:%u, , error copy to user", vdata->info.drv_id);
		}
	}

	(void)vmgr_list_manager_add_pool(vdata->mgr_ctx, cmd_result);

	return ret;
}

static int vdec_cmd_get_user_result(vpu_dec_drv_t *vdata, void *arg, size_t copySize, enum vpu_cmd_type match_cmd_type)
{
	int ret = 0;
	vpu_cmd_t *cmd_result = NULL;

	cmd_result = vdec_cmd_get_result(vdata, copySize, match_cmd_type);
	if (cmd_result != NULL) {
		ret = vdec_cmd_result_to_user(vdata, arg, cmd_result, copySize);
	} else {
		ret = -EAGAIN;
	}

	return ret;
}

static void vdec_force_close(vpu_dec_drv_t *vdata)
{
	int ret = 0;

	dlog_vdec("dec_id:%u, vdec_force_close, opened:%d, vmgr_get_alive:%d", vdata->info.drv_id, vmgr_get_opened(vdata->mgr_ctx, vdata->info.op_type, vdata->info.drv_id), vmgr_get_alive(vdata->mgr_ctx));
	if ((vmgr_get_opened(vdata->mgr_ctx, vdata->info.op_type, vdata->info.drv_id) == true) && vmgr_get_alive(vdata->mgr_ctx) > 0) {
		int max_count = 100;
		vpu_cmd_t *node = NULL;

		dlog_vdec("dec_id:%u, call vmgr_process_ex", vdata->info.drv_id);

		node = vmgr_list_manager_alloc(vdata->mgr_ctx);

		vmgr_process_ex(vdata->mgr_ctx, node, vdata->info.op_type, vdata->info.drv_id, VPU_CMD_DEC_CLOSE, &ret);

		while (vmgr_get_opened(vdata->mgr_ctx, vdata->info.op_type, vdata->info.drv_id) == true) {
			max_count--;
			msleep(20);

			if (max_count <= 0) {
				break;
			}
		}
	}

}

static int vdec_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret = 0;
	vpu_dec_drv_t *vdata = (vpu_dec_drv_t *)filp->private_data;

#if defined(CONFIG_PMAP)
	if (vma->vm_end < vma->vm_start) {
		err_vdec("dec_id:%u, %s :: mmap :: vm_address_range failed : start_addr(%p), end_addr(%p)", vdata->info.drv_id, vdata->dec_shared->misc.name, vma->vm_start, vma->vm_end);
		ret = (int)-EAGAIN;
	} else {
		if (range_is_allowed(vma->vm_pgoff, vma->vm_end - vma->vm_start) < 0) {
			err_vdec("dec_id:%u, %s :: mmap: this address is not allowed", vdata->info.drv_id, vdata->dec_shared->misc.name);
			ret = (int)-EAGAIN;
		}
	}
#endif

	vma->vm_page_prot = vmem_get_pgprot(vma->vm_page_prot, vma->vm_pgoff);
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, (vma->vm_end - vma->vm_start), vma->vm_page_prot) != 0) {
		V_DBG(VPU_DBG_ERROR,
			"dec_id:%u, %s :: mmap :: remap_pfn_range failed", vdata->info.drv_id, vdata->dec_shared->misc.name);
		ret = (int)-EAGAIN;
	}

	vma->vm_ops = NULL;
	vetc_vm_flags_set(vma, (VM_IO | VM_DONTEXPAND | VM_PFNMAP));

	return ret;
}

static unsigned int vdec_poll(struct file *filp, poll_table *wait)
{
	unsigned int ret = 0U;
	vpu_dec_drv_t *vdata = (vpu_dec_drv_t *)filp->private_data;

	if (vdata == NULL) {
		ret = (unsigned int)POLLERR | (unsigned int)POLLNVAL;
	} else {
		if (atomic_read(&vdata->drv_poll_data.count) == 0) {
			poll_wait(filp, &(vdata->drv_poll_data.wq), wait);
		}

		if (atomic_read(&vdata->drv_poll_data.count) > 0) {
			atomic_dec(&vdata->drv_poll_data.count);
			ret = (unsigned int)POLLIN;
		}
	}

	return ret;
}

long vdec_poll_2(struct file *filp, int timeout_ms)
{
	long ret = 0;
	unsigned long jtimeout;
	vpu_dec_drv_t *vdata = (vpu_dec_drv_t *)filp->private_data;

	if (vdata == NULL) {
		ret = (long)((unsigned int)POLLERR | (unsigned int)POLLNVAL);
	} else {
		if (timeout_ms < 0) {
			timeout_ms = 0;
		}

		jtimeout = msecs_to_jiffies(timeout_ms);
		if (jtimeout > (unsigned long)LONG_MAX) {
			jtimeout = (unsigned long)LONG_MAX;
		}

		(void) wait_event_interruptible_timeout(vdata->drv_poll_data.wq,
					atomic_read(&vdata->drv_poll_data.count) > 0,
					(long)jtimeout);

		if (atomic_read(&vdata->drv_poll_data.count) > 0) {
			atomic_dec(&vdata->drv_poll_data.count);
			ret = (long)POLLIN;
		}
	}

	//V_DBG(VPU_DBG_ERROR, "## poll time out, drv_id:%d", vdata->info.drv_id);
	return ret;
}

EXPORT_SYMBOL(vdec_poll_2);

static int vdec_init(vpu_dec_drv_t *vdata)
{
	V_DBG(VPU_DBG_SEQUENCE, "dec_id:%u, %s :: _vdec_open(%d)!!", vdata->info.drv_id, vdata->dec_shared->misc.name);

	vetc_memset(&vdata->drv_poll_data, 0, sizeof(vpu_drv_poll_t), 0);

	init_waitqueue_head(&vdata->drv_poll_data.wq);
	atomic_set(&vdata->drv_poll_data.count, 0);

	vdata->info.op_type = VPU_OP_TYPE_DEC;
	vdata->info.pmap_type = VPU_PMAP_MAX;

	return 0;
}

static int vdec_deinit(vpu_dec_drv_t *vdata)
{
	dlog_vdec("dec_id:%u, clear instance, pmap type:%s(%d)", vdata->info.drv_id, vmgr_get_pmap_name(vdata->info.pmap_type), vdata->info.pmap_type);

	(void)vmem_proc_free_memory((vputype)vdata->info.pmap_type);

	vmem_clear_instance(vdata->info.pmap_type);

	if (vdata->mgr_ctx != NULL) {
		(void)vdec_force_close(vdata);

		(void)vmgr_deregister(vdata->mgr_ctx, &vdata->info);
	} else {
		err_vdec("dec_id:%u the decoder was not initialized!", vdata->info.drv_id);
	}

	return 0;
}

static enum vpu_pmap_type vdec_assign_pmap_type(vpu_dec_drv_t *vdata, vdec_v3_init_in_t *init_in)
{
	enum vpu_pmap_type pmap_type;

	if (init_in->use_forced_pmap_idx == 1U) {

		int inUse = 0;
		pmap_type = init_in->forced_pmap_idx;
		dlog_vdec("dec_id:%u, vdec get forced maptype:%s(%d)", vmgr_get_pmap_name(pmap_type), pmap_type);

		inUse = vmem_is_index_in_use(pmap_type);
		if (inUse == 1) {
			//already in-use index
			err_vdec("dec_id:%u, forced_pmap_idx:%d, already in-use index", vdata->info.drv_id, init_in->forced_pmap_idx);
			pmap_type = VPU_PMAP_MAX;
		} else if (inUse == 0) {
			pmap_type = vmem_set_instance(pmap_type);
			if ((pmap_type < 0)) {
				err_vdec("dec_id:%u, fail to set forced_pmap_idx:%d, ret:%d", vdata->info.drv_id, init_in->forced_pmap_idx, pmap_type);
				pmap_type = VPU_PMAP_MAX;
			} else {
				if (init_in->forced_pmap_idx != pmap_type) {
					err_vdec("dec_id:%u, fail to set forced_pmap_idx:%d, get pmap_type:%d", vdata->info.drv_id, init_in->forced_pmap_idx, pmap_type);
					pmap_type = VPU_PMAP_MAX;
				} else {
					vdata->info.pmap_type = pmap_type;
					dlog_vdec("dec_id:%u, succeed set forced_pmap_idx:%d, get pmap_type:%d", vdata->info.drv_id, init_in->forced_pmap_idx, pmap_type);
				}
			}
		}
	} else {
		int instance_id = (int)vdata->info.drv_id;
		vmem_get_instance(&instance_id);
		pmap_type = (enum vpu_pmap_type)instance_id;
	}

	return pmap_type;
}

static int vdec_register_info(vpu_dec_drv_t *vdata, int force_index)
{
	int ret = 0;

	vetc_mutex_lock(&vdata->dec_shared->shared_mutex);

	vdata->info.ip_type = vmgr_find_ip_type(vdata->info.drv_id, vdata->info.codec_id, vdata->info.op_type, force_index);
	dlog_vdec("dec_id:%u, codec_id:%d, ip_type:%s(%d), force_index:%d", vdata->info.drv_id, vdata->info.codec_id, vmgr_get_ip_name(vdata->info.ip_type), vdata->info.ip_type, force_index);

	if ((vdata->info.ip_type > VPU_IP_UNKNOWN) && (vdata->info.ip_type < VPU_IP_MAX)) {
		vdata->mgr_ctx = vmgr_get_context(vdata->info.ip_type);
		if (vdata->mgr_ctx != NULL) {
			dlog_vdec("dec_id:%u, mgr ctx:%p, ip type:%s", vdata->info.drv_id, vdata->mgr_ctx, vmgr_get_ip_name(vdata->mgr_ctx->each_ip->ip_type));

			if (vmem_get_free_memory((vputype)vdata->info.pmap_type) == 0) {
				ret = (int)-ENOMEM;
			} else {
				vmgr_register(vdata->mgr_ctx, &vdata->info);
			}
		} else {
			ret = (int)-EFAULT;
		}
	} else {
		ret = (int)-EFAULT;
		err_vdec("dec_id:%u, NULL mgr context for ip type:%s", vdata->info.drv_id, vmgr_get_ip_name(vdata->info.ip_type));
	}

	vetc_mutex_unlock(&vdata->dec_shared->shared_mutex);

	return ret;
}

static int vdec_v3_proc_get_drv_version(vpu_dec_drv_t *vdata, void *arg)
{
	int ret = 0;
	vpu_drv_version_t drv_version;
	VPU_DONOTHING(vdata);

	vetc_memset(&drv_version, 0x00, sizeof(vpu_drv_version_t), 0);
	drv_version.major = VPU_V3_VERSION_MAJOR;
	drv_version.minor = VPU_V3_VERSION_MINOR;
	drv_version.revision = VPU_V3_VERSION_REV;

	dlog_vdec("dec_id:%u, driver version v%d.%d.%02d", vdata->info.drv_id, drv_version.major, drv_version.minor, drv_version.revision);

	if (vdec_is_from_kernel(vdata) == true) {
		vetc_memcpy(arg, &drv_version, sizeof(vpu_drv_version_t), 0);
	} else {
		if (copy_to_user(arg, &drv_version, sizeof(vpu_drv_version_t)) != 0U) {
			ret = -EFAULT;
		}
	}

	return ret;
}

static void vdec_v3_get_instance_info(int *max_count, int *avail_count)
{
	int ii;
	int max_alloc_count = 0;
	int available_count = 0;

	for (ii = (int)VPU_PMAP_DEC; ii < (int)VPU_PMAP_ENC; ii++) {
		int avail = 0;

		avail = vrm_check_index_availability(ii);
		if (avail >= 0) {
			max_alloc_count++;

			if (avail > 0) {
				available_count++;
			}
		}
	}

	if (max_count != NULL) {
		*max_count = max_alloc_count;
		dlog_vdec("max alloc count:%d", max_alloc_count);
	}

	if (avail_count != NULL) {
		*avail_count = available_count;
		dlog_vdec("available count:%d", available_count);
	}
}

static int vdec_v3_proc_capability(vpu_dec_drv_t *vdata, void *arg)
{
	int ret = 0;
	vpu_capability_t *vpu_capability = NULL;

	vpu_capability = VPU_alloc(sizeof(vpu_capability_t));
	if (vpu_capability != NULL) {
		int max_count = 0;
		int avail_count = 0;

		vetc_memset(vpu_capability, 0x00, sizeof(vpu_capability_t), 0);
		(void)vmgr_get_capability(VPU_OP_TYPE_DEC, vpu_capability);

		vdec_v3_get_instance_info(&max_count, &avail_count);

		vpu_capability->max_supported_instance = max_count;
		vpu_capability->available_instance = avail_count;

		vpu_capability->drv_id = vdata->info.drv_id;

		if (vdec_is_from_kernel(vdata) == true) {
			vetc_memcpy(arg, vpu_capability, sizeof(vpu_capability_t), 0);
		} else {
			if (copy_to_user(arg, vpu_capability, sizeof(vpu_capability_t)) != 0U) {
				ret = -EFAULT;
			}
		}

		VPU_free(vpu_capability);
		vpu_capability = NULL;
	} else {
		ret = -EFAULT;
	}

	return ret;
}

static int vdec_v3_proc_init(vpu_dec_drv_t *vdata, void *arg)
{
	int ret = 0;
	size_t copySize = 0;
	void *pArg = NULL;

	copySize = sizeof(vdec_v3_init_t);
	pArg = vdec_cmd_alloc_copy(vdata, arg, copySize);

	if (pArg != NULL) {
		vdec_v3_init_t *dec_init = (vdec_v3_init_t *)pArg;
		vdec_v3_init_in_t *init_in = &dec_init->input;

		vdata->info.codec_id = init_in->codec_id;
		vdata->info.pmap_type = vdec_assign_pmap_type(vdata, init_in);
		dlog_vdec("dec_id:%u, map_type:%s", vdata->info.drv_id, vmgr_get_pmap_name((const int)vdata->info.pmap_type));

		if (vdata->info.pmap_type < VPU_PMAP_MAX) {
			int force_index = -1;
			if ((init_in->enable_force_vpu_ip == 1) && (init_in->force_vpu_ip_index >= 0)) {
				force_index = init_in->force_vpu_ip_index;
			}

			ret = vdec_register_info(vdata, force_index);
		}

		if (ret == 0) {
			//save init info
			vetc_memcpy(&vdata->info.dec_init_info, init_in, sizeof(vdec_v3_init_in_t), 0);

			ret = vdec_add_list(vdata, VPU_CMD_DEC_INIT, pArg);
		} else {
			VPU_free(pArg);
			pArg = NULL;
		}
	}

	return ret;
}

static int vdec_v3_proc_init_result(vpu_dec_drv_t *vdata, void *arg)
{
	int ret = 0;

	ret = vdec_cmd_get_user_result(vdata, arg, sizeof(vdec_v3_init_t), VPU_CMD_DEC_INIT);

	return ret;
}

static int vdec_v3_proc_seqheader(vpu_dec_drv_t *vdata, void *arg)
{
	int ret = 0;
	size_t copySize = 0;

	copySize = sizeof(vdec_v3_seqheader_t);

	ret = vdec_cmd_put(vdata, VPU_CMD_DEC_SEQ_HEADER, arg, copySize);

	return ret;
}

static int vdec_v3_proc_seqheader_result(vpu_dec_drv_t *vdata, void *arg)
{
	int ret = 0;

	ret = vdec_cmd_get_user_result(vdata, arg, sizeof(vdec_v3_seqheader_t), VPU_CMD_DEC_SEQ_HEADER);

	return ret;
}

static int vdec_v3_proc_decode(vpu_dec_drv_t *vdata, void *arg)
{
	int ret = 0;
	size_t copySize = 0;

	copySize = sizeof(vdec_v3_decode_t);

	ret = vdec_cmd_put(vdata, VPU_CMD_DEC_DECODE, arg, copySize);

	return ret;
}

static int vdec_v3_proc_decode_result(vpu_dec_drv_t *vdata, void *arg)
{
	int ret = 0;

	ret = vdec_cmd_get_user_result(vdata, arg, sizeof(vdec_v3_decode_t), VPU_CMD_DEC_DECODE);

	return ret;
}

static int vdec_v3_proc_buf_clear(vpu_dec_drv_t *vdata, void *arg)
{
	int ret = 0;
	size_t copySize = 0;

	copySize = sizeof(vdec_v3_buf_clear_t);

	ret = vdec_cmd_put(vdata, VPU_CMD_DEC_BUF_FLAG_CLEAR, arg, copySize);

	return ret;
}

static int vdec_v3_proc_buf_clear_result(vpu_dec_drv_t *vdata, void *arg)
{
	int ret = 0;

	ret = vdec_cmd_get_user_result(vdata, arg, sizeof(vdec_v3_buf_clear_t), VPU_CMD_DEC_BUF_FLAG_CLEAR);

	return ret;
}

static int vdec_v3_proc_drain(vpu_dec_drv_t *vdata, void *arg)
{
	int ret = 0;
	size_t copySize = 0;

	copySize = sizeof(vdec_v3_drain_t);

	ret = vdec_cmd_put(vdata, VPU_CMD_DEC_DRAIN, arg, copySize);

	return ret;
}

static int vdec_v3_proc_drain_result(vpu_dec_drv_t *vdata, void *arg)
{
	int ret = 0;

	ret = vdec_cmd_get_user_result(vdata, arg, sizeof(vdec_v3_drain_t), VPU_CMD_DEC_DRAIN);

	return ret;
}

static int vdec_v3_proc_flush(vpu_dec_drv_t *vdata, void *arg)
{
	int ret = 0;
	size_t copySize = 0;

	copySize = sizeof(vdec_v3_flush_t);

	if (vdata->mgr_ctx != NULL) {
		vmgr_decode_pre_flush(vdata->mgr_ctx, &vdata->info);
	}

	ret = vdec_cmd_put(vdata, VPU_CMD_DEC_FLUSH, arg, copySize);

	return ret;
}

static int vdec_v3_proc_flush_result(vpu_dec_drv_t *vdata, void *arg)
{
	int ret = 0;

	ret = vdec_cmd_get_user_result(vdata, arg, sizeof(vdec_v3_flush_t), VPU_CMD_DEC_FLUSH);

	return ret;
}

static int vdec_v3_proc_close(vpu_dec_drv_t *vdata, void *arg)
{
	int ret = 0;
	size_t copySize = 0;

	copySize = sizeof(vdec_v3_close_t);

	ret = vdec_cmd_put(vdata, VPU_CMD_DEC_CLOSE, arg, copySize);

	return ret;
}

static int vdec_v3_proc_close_result(vpu_dec_drv_t *vdata, void *arg)
{
	int ret = 0;

	ret = vdec_cmd_get_user_result(vdata, arg, sizeof(vdec_v3_close_t), VPU_CMD_DEC_CLOSE);

	return ret;
}

static int vdec_v3_proc_reg_framebuffer(vpu_dec_drv_t *vdata, void *arg)
{
	int ret = 0;
	size_t copySize = 0;

	copySize = sizeof(vdec_v3_reg_framebuffer_t);

	ret = vdec_cmd_put(vdata, VPU_CMD_DEC_REG_USER_FRAME_BUFFER, arg, copySize);

	return ret;
}

static int vdec_v3_proc_reg_framebuffer_result(vpu_dec_drv_t *vdata, void *arg)
{
	int ret = 0;

	ret = vdec_cmd_get_user_result(vdata, arg, sizeof(vdec_v3_reg_framebuffer_t), VPU_CMD_DEC_REG_USER_FRAME_BUFFER);

	return ret;
}


static int vdec_v3_get_next_result(vpu_dec_drv_t *vdata, void *arg)
{
	int ret = 0;
	vpu_cmd_t *cmd_result = NULL;
	vdec_v3_get_next_result_t next;

	cmd_result = vmgr_list_manager_show_result(vdata->mgr_ctx, vdata->info.op_type, vdata->info.drv_id);
	if (cmd_result != NULL) {
		next.result = VPU_RETCODE_SUCCESS;
		if ((cmd_result->cmd_type >= 0) && (cmd_result->cmd_type <= VPU_CMD_DEC_CLOSE)) {
			next.next_result_cmd = vdec_opcode_to_ioctl_map[cmd_result->cmd_type];
		} else {
			next.next_result_cmd = -1;
		}

		if (next.next_result_cmd < 0) {
			V_DBG(VPU_DBG_ERROR, "dec_id:%d, mapping failed, result cmd:%s(%d)", vdata->info.drv_id, vmgr_cmd_name(cmd_result->cmd_type), cmd_result->cmd_type);
		}

		next.remain_result = vmgr_list_manager_result_remain(vdata->mgr_ctx, vdata->info.op_type, vdata->info.drv_id);
	} else {
		next.result = VPU_RETCODE_SUCCESS;
		next.next_result_cmd = -1;
		next.remain_result = 0;
	}

	//dlog_vdec("dec_id:%d, result cmd:%s(%d)", vdata->info.drv_id, vmgr_cmd_name(cmd_result->cmd_type), cmd_result->cmd_type);
	if (vdec_is_from_kernel(vdata) == true) {
		vetc_memcpy(arg, &next, sizeof(vdec_v3_get_next_result_t), 0);
	} else {
		if (copy_to_user(arg, &next, sizeof(vdec_v3_get_next_result_t)) != 0U) {
			ret = -EFAULT;
		}
	}

	return ret;
}

static int vdec_v3_alloc_memory(vpu_dec_drv_t *vdata, void *arg)
{
	int ret = 0;
	vpu_mem_alloc_t user_alloc_info;
	MEM_ALLOC_INFO_t alloc_info;
	int instance_id;
	int format_type;

	if (vdec_is_from_kernel(vdata) == true) {
		vetc_memcpy(&user_alloc_info, (vpu_mem_alloc_t *)arg, sizeof(vpu_mem_alloc_t), 0);
	} else {
		if (copy_from_user(&user_alloc_info, (vpu_mem_alloc_t *)arg, sizeof(vpu_mem_alloc_t)) != 0) {
			ret = -EFAULT;
		}
	}

	vdata->info.codec_id = user_alloc_info.codec;
	format_type = vmgr_get_bitstream_format(vdata->info.codec_id, VPU_OP_TYPE_DEC);

	//If memory allocation is being attempted when the VPU decoder is not initialized
	if (vdata->info.pmap_type == VPU_PMAP_MAX) {
		instance_id = (int)vdata->info.drv_id;
		vmem_get_instance(&instance_id);

		vdata->info.pmap_type = (enum vpu_pmap_type)instance_id;
		dlog_vdec("dec_id:%u, map_type:%s, instance_id:%d, format_type:%d, codec_id:%d", vdata->info.drv_id, vmgr_get_pmap_name((const int)vdata->info.pmap_type), instance_id, format_type, vdata->info.codec_id);
	}

	vetc_memset(&alloc_info, 0x00, sizeof(MEM_ALLOC_INFO_t), 0);

	alloc_info.buffer_type = vmgr_convert_buffer_type(user_alloc_info.buffer_type);
	alloc_info.request_size = user_alloc_info.request_size;

	dlog_vdec("buffer type:%d, req size:0x%x", alloc_info.buffer_type, alloc_info.request_size);
	ret = vmem_proc_alloc_memory(format_type, &alloc_info, (vputype)vdata->info.pmap_type);
	if (ret == 0) {
		user_alloc_info.phy_addr = alloc_info.phy_addr;
		user_alloc_info.kernel_remap_addr = alloc_info.kernel_remap_addr;
		dlog_vdec("buffer phy_addr:0x%x, kernel_remap_addr:0x%x, size:%d", alloc_info.phy_addr, alloc_info.kernel_remap_addr, alloc_info.request_size);

		if (vdec_is_from_kernel(vdata) == true) {
			vetc_memcpy(arg, &user_alloc_info, sizeof(vpu_mem_alloc_t), 0);
		} else {
			if (copy_to_user(arg, &user_alloc_info, sizeof(vpu_mem_alloc_t)) != 0U) {
				ret = -EFAULT;
			}
		}
	} else {
		ret = -ENOMEM;
	}

	return ret;
}


static long vdec_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	union {
		unsigned long ul_data;
		int *pi_data;	//NULL
		void *pv_data;
	} uarg;

	long ret = 0;
	vpu_dec_drv_t *vdata = (vpu_dec_drv_t *)filp->private_data;

	if (cmd >= KERNEL_OFFSET) {
		cmd -= KERNEL_OFFSET;

		if (vdata->info.is_kernel_call != true) {
			vdata->info.is_kernel_call = true;
			dlog_vdec("dec_id:%u, type:%d, ioctl:%s(%d), is_kernel_call:%d", vdata->info.drv_id, vdata->info.pmap_type, vdec_ioctl_name(cmd), cmd, vdata->info.is_kernel_call);
		}
	}

	uarg.pi_data = NULL;
	uarg.ul_data = arg;

	dlog_vdec("--- dec_id:%u, type:%d, ioctl:%s(%d), k:%d", vdata->info.drv_id, vdata->info.pmap_type, vdec_ioctl_name(cmd), cmd, vdata->info.is_kernel_call);

	if (ret == 0) {
		switch (cmd) {
		case VPU_V3_GET_DRV_VERSION_SYNC:
		{
			ret = vdec_v3_proc_get_drv_version(vdata, uarg.pv_data);
		}
		break;

		case VPU_V3_GET_CAPABILITY_SYNC:
		{
			ret = vdec_v3_proc_capability(vdata, uarg.pv_data);
		}
		break;

		case VDEC_V3_INIT:
		{
			ret = vdec_v3_proc_init(vdata, uarg.pv_data);
		}
		break;

		case VDEC_V3_INIT_RESULT:
		{
			ret = vdec_v3_proc_init_result(vdata, uarg.pv_data);
		}
		break;

		case VDEC_V3_SEQ_HEADER:
		{
			ret = vdec_v3_proc_seqheader(vdata, uarg.pv_data);
		}
		break;

		case VDEC_V3_SEQ_HEADER_RESULT:
		{
			ret = vdec_v3_proc_seqheader_result(vdata, uarg.pv_data);
		}
		break;

		case VDEC_V3_DECODE:
		{
			ret = vdec_v3_proc_decode(vdata, uarg.pv_data);
		}
		break;

		case VDEC_V3_DECODE_RESULT:
		{
			ret = vdec_v3_proc_decode_result(vdata, uarg.pv_data);
		}
		break;

		case VDEC_V3_BUF_CLEAR:
		{
			ret = vdec_v3_proc_buf_clear(vdata, uarg.pv_data);
		}
		break;

		case VDEC_V3_BUF_CLEAR_RESULT:
		{
			ret = vdec_v3_proc_buf_clear_result(vdata, uarg.pv_data);
		}
		break;

		case VDEC_V3_DRAIN:
		{
			ret = vdec_v3_proc_drain(vdata, uarg.pv_data);
		}
		break;

		case VDEC_V3_DRAIN_RESULT:
		{
			ret = vdec_v3_proc_drain_result(vdata, uarg.pv_data);
		}
		break;

		case VDEC_V3_FLUSH:
		{
			ret = vdec_v3_proc_flush(vdata, uarg.pv_data);
		}
		break;

		case VDEC_V3_FLUSH_RESULT:
		{
			ret = vdec_v3_proc_flush_result(vdata, uarg.pv_data);
		}
		break;

		case VDEC_V3_CLOSE:
		{
			ret = vdec_v3_proc_close(vdata, uarg.pv_data);
		}
		break;

		case VDEC_V3_CLOSE_RESULT:
		{
			ret = vdec_v3_proc_close_result(vdata, uarg.pv_data);
		}
		break;

		case VDEC_V3_REG_FRAMEBUFFER:
		{
			ret = vdec_v3_proc_reg_framebuffer(vdata, uarg.pv_data);
		}
		break;

		case VDEC_V3_REG_FRAMEBUFFER_RESULT:
		{
			ret = vdec_v3_proc_reg_framebuffer_result(vdata, uarg.pv_data);
		}
		break;

		case VDEC_V3_GET_NEXT_RESULT_SYNC:
		{
			ret = vdec_v3_get_next_result(vdata, uarg.pv_data);
		}
		break;

		case VDEC_V3_ALLOC_MEMORY_SYNC:
		{
			ret = vdec_v3_alloc_memory(vdata, uarg.pv_data);
		}
		break;

		default:
		{
			ret = -EFAULT;
		}
		break;
		} //switch
	} //if (ret == 0)

	if (ret < 0) {
		err_vdec("dec_id:%u, type:%d, ioctl:%s(%d), ret:%d", vdata->info.drv_id, vdata->info.pmap_type, vdec_ioctl_name(cmd), cmd, ret);
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long vdec_compat_ioctl(struct file *file, unsigned int cmd,
			unsigned long arg)
{
	return vdec_ioctl(file, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static int vdec_open(struct inode *pinode, struct file *filp)
{
	int ret = 0;
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	vpu_drv_shared_t *shared = dev_get_drvdata(misc->parent);
	vpu_dec_drv_t *vdata = NULL;
	unsigned int decoder_idx;

	vdata = VPU_alloc(sizeof(vpu_dec_drv_t));
	if (vdata != NULL) {
		//set each device private
		vdata->dec_shared = (vpu_drv_shared_t *)shared;
		filp->private_data = vdata;

		vetc_mutex_lock(&shared->shared_mutex);

		vdata->info.drv_id = VPU_DRV_ID_MAX;
		for (decoder_idx = 0U; decoder_idx < VPU_DRV_ID_MAX; decoder_idx++) {
			if (decoder_map[decoder_idx] == false) {
				//set used
				decoder_map[decoder_idx] = true;
				vdata->info.drv_id = decoder_idx;
				dlog_vdec("dec_id:%u, set used", vdata->info.drv_id);
				break;
			}
		}

		atomic_inc(&shared->reference_count);
		vetc_mutex_unlock(&shared->shared_mutex);

		if (vdata->info.drv_id < VPU_DRV_ID_MAX) {
			(void)vdec_init(vdata);
		} else {
			ret = -ENODEV;
			dlog_vdec("dec_id:%u, Failed to allocate device ID", vdata->info.drv_id);
		}
	} else {
		ret = -ENOMEM;
		dlog_vdec("Failed to allocate memory");
	}

	if (ret != 0) {
		if (vdata != NULL) {
			VPU_free(vdata);
			vdata = NULL;
		}
	}

	return ret;
}

static int vdec_release(struct inode *pinode, struct file *filp)
{
	int dec_id = 0;
	vpu_dec_drv_t *vdata = (vpu_dec_drv_t *)filp->private_data;
	vpu_drv_shared_t *shared = vdata->dec_shared;

	dec_id = vdata->info.drv_id;

	atomic_dec(&shared->reference_count);
	mutex_lock(&shared->shared_mutex);

	(void)vdec_deinit(vdata);
	decoder_map[vdata->info.drv_id] = false;
	dlog_vdec("dec_id:%u, set unused\n", vdata->info.drv_id);

	mutex_unlock(&shared->shared_mutex);

	VPU_free(vdata);

	dlog_vdec("dec_id:%u, release", dec_id);
	return 0;
}

static const struct file_operations vdev_dec_fops = {
	.owner              = THIS_MODULE,
	.open               = vdec_open,
	.release            = vdec_release,
	.mmap               = vdec_mmap,
	.unlocked_ioctl     = vdec_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl       = vdec_compat_ioctl,
#endif
	.poll               = vdec_poll,
};

int vdec_probe(struct platform_device *pdev)
{
	struct miscdevice *misc = NULL;
	vpu_drv_shared_t *dec_shared = NULL;
	int ret = 0;

	dlog_vdec("VPU %s Driver(id:%d) probe", pdev->name, pdev->id);

	dec_shared = (vpu_drv_shared_t *)VPU_alloc(sizeof(vpu_drv_shared_t));
	if (dec_shared == NULL) {
		ret = -ENOMEM;
		V_DBG(VPU_DBG_ERROR, "dec_shared alloc failed");
	} else {
		misc = &dec_shared->misc;

		mutex_init(&dec_shared->shared_mutex);
		misc->minor = MISC_DYNAMIC_MINOR;
		misc->fops = &vdev_dec_fops;
		misc->name = pdev->name;
		misc->parent = &pdev->dev;

		if (misc_register(misc) != 0) {
			V_DBG(VPU_DBG_ERROR, "VPU %s: Couldn't register device.", pdev->name);
			ret = -EBUSY;
		} else {
			platform_set_drvdata(pdev, dec_shared);
		}
	}

	if (ret != 0) {
		if (dec_shared != NULL) {
			VPU_free(dec_shared);
		}

		(void)pr_info("VPU %s Driver(id:%d) Initialize Failed", pdev->name, pdev->id);
	} else {
		(void)pr_info("VPU %s Driver(id:%d) Initialized.", pdev->name, pdev->id);
	}

	return ret;
}

EXPORT_SYMBOL(vdec_probe);

VREMOVE_RET_TYPE vdec_remove(struct platform_device *pdev)
{
	vpu_drv_shared_t *dec_shared = (vpu_drv_shared_t *)platform_get_drvdata(pdev);

	misc_deregister(&dec_shared->misc);

	if (dec_shared != NULL) {
		VPU_free(dec_shared);
	}

	VREMOVE_RETURN();
}

EXPORT_SYMBOL(vdec_remove);

#endif /*CONFIG_VDEC_CNT_X*/
