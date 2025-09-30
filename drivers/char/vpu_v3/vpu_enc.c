/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#include "vpu_comm.h"

#if DEFINED_CONFIG_VENC

#include "vpu_rm.h"
#include "vpu_enc.h"
#include "vpu_mgr_context.h"
#include "vpu_mgr_common.h"

#define dlog_venc(msg...)		V_DBG(VPU_DBG_INFO, "[VPU_ENC][LOG]:" msg)
#define detail_venc(msg...)		V_DBG(VPU_DBG_DETAIL, "[VPU_ENC][DETAIL]:" msg)
#define seq_venc(msg...)		V_DBG(VPU_DBG_SEQUENCE, "[VPU_ENC][SEQ]: " msg)
#define err_venc(msg...)		V_DBG(VPU_DBG_ERROR, "[VPU_ENC][ERR]:" msg)

//#define DBG(fmt, args...)	do { pr_err_venc("[Devices][%s:%d]" fmt "\n", __FUNCTION__, __LINE__, ## args); } while (0)

static bool encoder_map[VPU_DRV_ID_MAX] = {false, };

//for debugging
static const char *vpu_enc_ioctl_invalid = "VPU_INVALID_COMMAND";
static const char *vpu_enc_com_ioctl_name[] = {
	"VPU_V3_COM_BASE",
	"VPU_V3_GET_DRV_VERSION_SYNC",
	"VPU_V3_GET_CAPABILITY_SYNC",
};

static const char *vpu_enc_ioctl_name[] = {
	"VENC_V3_BASE", //invalid
	"VENC_V3_INIT",
	"VENC_V3_INIT_RESULT",
	"VENC_V3_PUT_HEADER",
	"VENC_V3_PUT_HEADER_RESULT",
	"VENC_V3_ENCODE",
	"VENC_V3_ENCODE_RESULT",
	"VENC_V3_CLOSE",
	"VENC_V3_CLOSE_RESULT",
	"VENC_V3_ALLOC_MEMORY_SYNC",
};

#if defined(V_ENC_GET_NEXT_RESULT_CMD)

//ioctl in tcc_video_common.h
//opcode in TCCxxx_VPU_CODEC_COMMON.h
static const int venc_opcode_to_ioctl_map[] = {
	V_ENC_INIT, // VPU_ENC_INIT 0x00
	V_ENC_REG_FRAME_BUFFER, //VPU_ENC_REG_FRAME_BUFFER  0x01
	-1, //0x02
	-1,
	-1,
	-1,
	-1,
	-1, //0x07
	-1, //0x08
	-1, //0x09
	-1, //0x0a
	-1, //0x0b
	-1, //0x0c
	-1, //0x0d
	-1, //0x0e
	-1, //0x0f
	V_ENC_PUT_HEADER,  //VPU_ENC_PUT_HEADER 16(0x10)
	-1, //0x11
	V_ENC_ENCODE, //VPU_ENC_ENCODE 18(0x12)
	-1, //0x13
	-1, //0x14
	-1, //21
	-1, //22(0x16)
	-1, //23
	-1, //24
	-1, //25(0x19)
	-1, //26(0x1a)
	-1, //27
	-1, //28
	-1, //29
	-1, //30
	-1, //31 (0x1f)
	V_ENC_CLOSE, // VPU_ENC_CLOSE (0x20)
};
#endif

static const char *venc_ioctl_name(const int cmd)
{
	const char *ret_name = NULL;

	if ((cmd > VPU_V3_COM_BASE) && (cmd <= VPU_V3_GET_CAPABILITY_SYNC)) {
		ret_name = vpu_enc_com_ioctl_name[cmd - VPU_V3_COM_BASE];
	} else if ((cmd > VENC_V3_BASE) && (cmd <= VENC_V3_ALLOC_MEMORY_SYNC)) {
		ret_name = vpu_enc_ioctl_name[cmd - VENC_V3_BASE];
	} else {
		ret_name = vpu_enc_ioctl_invalid;
	}

	return ret_name;
}

static bool venc_is_from_kernel(vpu_enc_drv_t *vdata)
{
	bool from_kernel = false;

	if (vdata->info.is_kernel_call == true) {
		from_kernel = true;
	}

	return from_kernel;
}


static int venc_add_list(vpu_enc_drv_t *vdata, enum vpu_cmd_type cmd, void *args)
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
		V_DBG(VPU_DBG_ERROR, "enc_id:%u, cmd node allocation failed!", vdata->info.drv_id);
		ret = -ENOMEM;
	}

	return ret;
}

static void *venc_cmd_alloc_copy(vpu_enc_drv_t *vdata, void *arg, size_t copySize)
{
	int ret = 0;
	void *pArgs = NULL;

	if ((copySize > 0) && (arg != NULL)) {
		pArgs = VPU_alloc(copySize);
		if (pArgs != NULL) {
			if (venc_is_from_kernel(vdata) == true) {
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

static int venc_cmd_put(vpu_enc_drv_t *vdata, enum vpu_cmd_type cmd, void *arg, size_t copySize)
{
	int ret = 0;
	void *pArgs = NULL;

	pArgs = venc_cmd_alloc_copy(vdata, arg, copySize);
	if (pArgs != NULL) {
		ret = venc_add_list(vdata, cmd, pArgs);
	} else {
		ret = -1;
	}

	return ret;
}

static vpu_cmd_t *venc_cmd_get_result(vpu_enc_drv_t *vdata, size_t copySize, enum vpu_cmd_type match_cmd_type)
{
	vpu_cmd_t *cmd_result = NULL;

	cmd_result = vmgr_list_manager_get_result(vdata->mgr_ctx, vdata->info.op_type, vdata->info.drv_id);
	if (cmd_result != NULL) {
		//unmatched command
		if (cmd_result->cmd_type != match_cmd_type) {
			err_venc("enc_id:%u,unmatched cmd, result_type:(%s)%d, match_type:(%s)%d", vdata->info.drv_id, vmgr_cmd_name(cmd_result->cmd_type), cmd_result->cmd_type, vmgr_cmd_name(match_cmd_type), match_cmd_type);
		} else {
			if (cmd_result->args != NULL) {
				//set result
				*((enum vpu_return_code *)cmd_result->args) = cmd_result->result;
				if (cmd_result->result != VPU_RETCODE_SUCCESS) {
					err_venc("enc_id:%u, cmd:%s, result:%s (%d)", vdata->info.drv_id, vmgr_cmd_name(cmd_result->cmd_type), vmgr_get_return_name(cmd_result->result), cmd_result->result);
				}
			}
		}
	} else {
		err_venc("enc_id:%u, error, no result for %s(%d)", vdata->info.drv_id, vmgr_cmd_name(match_cmd_type), match_cmd_type);
	}

	return cmd_result;
}

static int venc_cmd_result_to_user(vpu_enc_drv_t *vdata, void *arg, vpu_cmd_t *cmd_result, size_t copySize)
{
	int ret = 0;

	if (venc_is_from_kernel(vdata) == true) {
		vetc_memcpy(arg, cmd_result->args, copySize, 0);
	} else {
		if (copy_to_user(arg, cmd_result->args, copySize) != 0U) {
			ret = (int)-EFAULT;
			err_venc("enc_id:%u, error copy to user", vdata->info.drv_id);
		}
	}

	(void)vmgr_list_manager_add_pool(vdata->mgr_ctx, cmd_result);

	return ret;
}

static int venc_cmd_get_user_result(vpu_enc_drv_t *vdata, void *arg, size_t copySize, enum vpu_cmd_type match_cmd_type)
{
	int ret = 0;
	vpu_cmd_t *cmd_result = NULL;

	cmd_result = venc_cmd_get_result(vdata, copySize, match_cmd_type);
	if (cmd_result != NULL) {
		ret = venc_cmd_result_to_user(vdata, arg, cmd_result, copySize);
	} else {
		ret = -EAGAIN;
	}

	return ret;
}

#if defined(V_ENC_GET_NEXT_RESULT_CMD) //new ioctl
static int venc_get_next_result_cmd(vpu_enc_drv_t *vdata, void *arg)
{
	int ret = 0;
	vpu_cmd_t *cmd_result = NULL;
	VPU_GET_NEXT_CMD_t next;

	cmd_result = vmgr_list_manager_show_result(vdata->mgr_ctx, vdata->info.op_type, vdata->info.drv_id);
	if (cmd_result != NULL) {
		next.result = VPU_RETCODE_SUCCESS;
		if ((cmd_result->cmd_type >= 0) && (cmd_result->cmd_type <= VPU_ENC_CLOSE)) {
			next.next_cmd = venc_opcode_to_ioctl_map[cmd_result->cmd_type];
		} else {
			next.next_cmd = -1;
		}

		if (next.next_cmd < 0) {
			V_DBG(VPU_DBG_ERROR, "enc_id:%u, mapping failed, result cmd:%s(%d)", vdata->info.drv_id, vmgr_cmd_name(cmd_result->cmd_type), cmd_result->cmd_type);
		}

		next.remain_cmd = vmgr_list_manager_result_remain(vdata->mgr_ctx, vdata->info.op_type, vdata->info.drv_id);
	} else {
		next.result = VPU_RETCODE_SUCCESS;
		next.next_cmd = -1;
		next.remain_cmd = 0;
	}

	//dlog_venc("enc_id:%u, result cmd:%s(%d)", vdata->info.drv_id, vmgr_cmd_name(cmd_result->cmd_type), cmd_result->cmd_type);
	if (venc_is_from_kernel(vdata) == true) {
		vetc_memcpy(arg, &next, sizeof(VPU_GET_NEXT_CMD_t), 0);
	} else {
		if (copy_to_user(arg, &next, sizeof(VPU_GET_NEXT_CMD_t))) {
			ret = -EFAULT;
		}
	}

	return ret;
}

#endif //#if defined(V_ENC_GET_NEXT_RESULT_CMD)

static void venc_force_close(vpu_enc_drv_t *vdata)
{
	int ret = 0;

	if ((vmgr_get_opened(vdata->mgr_ctx, vdata->info.op_type, vdata->info.drv_id) == true) && vmgr_get_alive(vdata->mgr_ctx) > 0) {
		vpu_cmd_t *node = NULL;
		int max_count = 100;

		dlog_venc("enc_id:%u, op_type:%s(%d), call vmgr_process_ex", vdata->info.drv_id, vmgr_get_optype_name(vdata->info.op_type), vdata->info.op_type);

		node = vmgr_list_manager_alloc(vdata->mgr_ctx);
		vmgr_process_ex(vdata->mgr_ctx, node, vdata->info.op_type, vdata->info.drv_id, VPU_CMD_ENC_CLOSE, &ret);

		while (vmgr_get_opened(vdata->mgr_ctx, vdata->info.op_type, vdata->info.drv_id) == true) {
			max_count--;
			msleep(20);

			if (max_count <= 0)
				break;
		}
	}
}

static int venc_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret = 0;
	vpu_enc_drv_t *vdata = (vpu_enc_drv_t *)filp->private_data;

#if defined(CONFIG_PMAP)
	if (vma->vm_end < vma->vm_start) {
		err_venc("enc_id:%u, %s :: mmap :: vm_address_range failed : start_addr(%p), end_addr(%p)", vdata->info.drv_id, vdata->enc_shared->misc.name, vma->vm_start, vma->vm_end);
		ret = (int)-EAGAIN;
	} else {
		if (range_is_allowed(vma->vm_pgoff, (vma->vm_end - vma->vm_start)) < 0) {
			err_venc("enc_id:%u, %s :: mmap : this address is not allowed", vdata->info.drv_id, vdata->enc_shared->misc.name);
			ret = (int)-EAGAIN;
		}
	}
#endif

	if (ret == 0) {
		vma->vm_page_prot = vmem_get_pgprot(vma->vm_page_prot, vma->vm_pgoff);
		if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, vma->vm_end - vma->vm_start, vma->vm_page_prot) != 0) {
			err_venc("enc_id:%u, %s :: mmap :: remap_pfn_range failed", vdata->info.drv_id, vdata->enc_shared->misc.name);
			ret = (int)-EAGAIN;
		} else {
			vma->vm_ops = NULL;
			vetc_vm_flags_set(vma, (VM_IO | VM_DONTEXPAND | VM_PFNMAP));
		}
	}

	return ret;
}

static unsigned int venc_poll(struct file *filp, poll_table *wait)
{
	unsigned int ret = 0U;
	vpu_enc_drv_t *vdata = (vpu_enc_drv_t *)filp->private_data;

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

static int venc_init(vpu_enc_drv_t *vdata)
{
	vetc_memset(&vdata->drv_poll_data, 0, sizeof(vpu_drv_poll_t), 0);

	init_waitqueue_head(&vdata->drv_poll_data.wq);
	atomic_set(&vdata->drv_poll_data.count, 0);

	vdata->info.op_type = VPU_OP_TYPE_ENC;
	vdata->info.pmap_type = VPU_PMAP_MAX;

	return 0;
}

static int venc_deinit(vpu_enc_drv_t *vdata)
{
	dlog_venc("enc_id:%u, clear instance, pmap type:%s(%d)", vdata->info.drv_id, vmgr_get_pmap_name(vdata->info.pmap_type), vdata->info.pmap_type);

	(void)vmem_proc_free_memory((vputype)vdata->info.pmap_type);

	vmem_clear_instance(vdata->info.pmap_type);

	if (vdata->mgr_ctx != NULL) {
		unsigned long accu_pixproduct;
		venc_force_close(vdata);

		(void)vmgr_deregister(vdata->mgr_ctx, &vdata->info);

		accu_pixproduct = vmgr_sub_accumulated_pixelproduct(vdata->mgr_ctx, vdata->info.pixel_product);
		dlog_venc("enc_id:%u, accumulated_pixelproduct:%lu", vdata->info.drv_id, accu_pixproduct);
	} else {
		err_venc("enc_id:%u, the encoder was not initialized!", vdata->info.drv_id);
	}

	return 0;
}

static enum vpu_pmap_type venc_assign_pmap_type(vpu_enc_drv_t *vdata, venc_v3_init_in_t *init_in)
{
	enum vpu_pmap_type pmap_type = VPU_PMAP_MAX;

	if (init_in->use_forced_pmap_idx == 1U) {
		int inUse = 0;
		pmap_type = init_in->forced_pmap_idx;
		dlog_venc("enc_id:%u, venc get forced maptype:%s(%d)", vdata->info.drv_id, vmgr_get_pmap_name(pmap_type), pmap_type);

		inUse = vmem_is_index_in_use(pmap_type);
		if (inUse == 1) {
			//already in-use index
			err_venc("enc_id:%u, forced_pmap_idx:%d, already in-use index", vdata->info.drv_id, init_in->forced_pmap_idx);
			pmap_type = VPU_PMAP_MAX;
		} else if (inUse == 0) {
			pmap_type = vmem_set_instance(pmap_type);
			if ((pmap_type < 0)) {
				err_venc("enc_id:%u, fail to set forced_pmap_idx:%d, ret:%d", vdata->info.drv_id, init_in->forced_pmap_idx, pmap_type);
				pmap_type = VPU_PMAP_MAX;
			} else {
				if (init_in->forced_pmap_idx != pmap_type) {
					err_venc("enc_id:%u, fail to set forced_pmap_idx:%d, get pmap_type:%d", vdata->info.drv_id, init_in->forced_pmap_idx, pmap_type);
					pmap_type = VPU_PMAP_MAX;
				} else {
					vdata->info.pmap_type = pmap_type;
					dlog_venc("succeed set forced_pmap_idx:%d, get pmap_type:%d", init_in->forced_pmap_idx, pmap_type);
				}
			}
		}
	} else {
		int instance_id = (int)vdata->info.drv_id + VPU_ENC;
		dlog_venc("try to get vmem, instance id:%d", instance_id);
		vmem_get_instance(&instance_id);
		if (instance_id >= 0) {
			pmap_type = (enum vpu_pmap_type)instance_id;
		} else {
			err_venc("enc_id:%u, no pmap for instance id:%d, error:%d, return type:%d", ((int)vdata->info.drv_id + VPU_ENC), instance_id, pmap_type);
		}
	}

	return pmap_type;
}

static int venc_register_info(vpu_enc_drv_t *vdata, int force_index)
{
	int ret = 0;

	vetc_mutex_lock(&vdata->enc_shared->shared_mutex);

	vdata->info.ip_type = vmgr_find_ip_type(vdata->info.drv_id, vdata->info.codec_id, vdata->info.op_type, force_index);
	dlog_venc("enc_id:%u,codec_id:%d, ip_type:%s(%d), force_index:%d", vdata->info.drv_id, vdata->info.codec_id, vmgr_get_ip_name(vdata->info.ip_type), vdata->info.ip_type, force_index);

	if ((vdata->info.ip_type > VPU_IP_UNKNOWN) && (vdata->info.ip_type < VPU_IP_MAX)) {
		vdata->mgr_ctx = vmgr_get_context(vdata->info.ip_type);
		if (vdata->mgr_ctx != NULL) {
			dlog_venc("enc_id:%u, mgr ctx:%p, ip type:%s", vdata->info.drv_id, vdata->mgr_ctx, vmgr_get_ip_name(vdata->mgr_ctx->each_ip->ip_type));

			(void)vmgr_add_accumulated_pixelproduct(vdata->mgr_ctx, vdata->info.pixel_product);

			if (vmem_get_free_memory((vputype)vdata->info.pmap_type) == 0) {
				err_venc("enc_id:%u, no free memory for pmap_type:%s", vdata->info.drv_id, vmgr_get_pmap_name(vdata->info.pmap_type));
				ret = (int)-ENOMEM;
			} else {
				vmgr_register(vdata->mgr_ctx, &vdata->info);
			}
		} else {
			ret = (int)-EFAULT;
		}
	} else {
		ret = (int)-EFAULT;
		err_venc("enc_id:%u, NULL mgr context for ip type:%s", vdata->info.drv_id, vmgr_get_ip_name(vdata->info.ip_type));
	}

	vetc_mutex_unlock(&vdata->enc_shared->shared_mutex);

	return ret;
}

static int venc_v3_proc_get_drv_version(vpu_enc_drv_t *vdata, void *arg)
{
	int ret = 0;
	vpu_drv_version_t drv_version;
	VPU_DONOTHING(vdata);

	vetc_memset(&drv_version, 0x00, sizeof(vpu_drv_version_t), 0);

	drv_version.major = VPU_V3_VERSION_MAJOR;
	drv_version.minor = VPU_V3_VERSION_MINOR;
	drv_version.revision = VPU_V3_VERSION_REV;

	dlog_venc("enc_id:%u, driver version v%d.%d.%02d", vdata->info.drv_id, drv_version.major, drv_version.minor, drv_version.revision);

	if (venc_is_from_kernel(vdata) == true) {
		vetc_memcpy(arg, &drv_version, sizeof(vpu_drv_version_t), 0);
	} else {
		if (copy_to_user(arg, &drv_version, sizeof(vpu_drv_version_t)) != 0U) {
			ret = -EFAULT;
		}
	}

	return ret;
}

static void venc_v3_get_instance_info(int *max_count, int *avail_count)
{
	int ii;
	int max_alloc_count = 0;
	int available_count = 0;

	for (ii = (int)VPU_PMAP_ENC; ii < (int)VPU_PMAP_MAX; ii++) {
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
		dlog_venc("max alloc count:%d", max_alloc_count);
	}

	if (avail_count != NULL) {
		*avail_count = available_count;
		dlog_venc("available count:%d", available_count);
	}
}

static int venc_v3_proc_capability(vpu_enc_drv_t *vdata, void *arg)
{
	int ret = 0;
	vpu_capability_t *vpu_capability = NULL;

	vpu_capability = VPU_alloc(sizeof(vpu_capability_t));
	if (vpu_capability != NULL) {
		int max_count = 0;
		int avail_count = 0;

		vetc_memset(vpu_capability, 0x00, sizeof(vpu_capability_t), 0);
		(void)vmgr_get_capability(VPU_OP_TYPE_ENC, vpu_capability);

		venc_v3_get_instance_info(&max_count, &avail_count);

		vpu_capability->max_supported_instance = max_count;
		vpu_capability->available_instance = avail_count;

		vpu_capability->drv_id = vdata->info.drv_id;

		if (venc_is_from_kernel(vdata) == true) {
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

static int venc_v3_proc_init(vpu_enc_drv_t *vdata, void *arg)
{
	int ret = 0;
	size_t copySize = 0;
	void *pArg = NULL;

	copySize = sizeof(venc_v3_init_t);
	pArg = venc_cmd_alloc_copy(vdata, arg, copySize);

	if (pArg != NULL) {
		venc_v3_init_t *enc_init = (venc_v3_init_t *)pArg;
		venc_v3_init_in_t *init_in = &enc_init->input;

		vdata->info.codec_id = init_in->codec_id;
		vdata->info.pmap_type = venc_assign_pmap_type(vdata, init_in);
		vdata->info.pixel_product = (unsigned long)(init_in->pic_width * init_in->pic_height * init_in->frame_rate);

		dlog_venc("enc_id:%u, map_type:%s(%d)", vdata->info.drv_id, vmgr_get_pmap_name((const int)vdata->info.pmap_type), vdata->info.pmap_type);

		if (vdata->info.pmap_type < VPU_PMAP_MAX) {
			int force_index = -1;
			if ((init_in->enable_force_vpu_ip == 1) && (init_in->force_vpu_ip_index >= 0)) {
				force_index = init_in->force_vpu_ip_index;
			}

			ret = venc_register_info(vdata, force_index);
		}

		if (ret == 0) {
			//save init info
			vetc_memcpy(&vdata->info.enc_init_info, init_in, sizeof(venc_v3_init_in_t), 0);

			ret = venc_add_list(vdata, VPU_CMD_ENC_INIT, pArg);
		} else {
			VPU_free(pArg);
			pArg = NULL;
		}
	}

	return ret;
}

static int venc_v3_proc_init_result(vpu_enc_drv_t *vdata, void *arg)
{
	int ret = 0;

	ret = venc_cmd_get_user_result(vdata, arg, sizeof(venc_v3_init_t), VPU_CMD_ENC_INIT);

	return ret;
}

static int venc_v3_proc_putheader(vpu_enc_drv_t *vdata, void *arg)
{
	int ret = 0;
	size_t copySize = 0;

	copySize = sizeof(venc_v3_putheader_t);

	ret = venc_cmd_put(vdata, VPU_CMD_ENC_PUT_HEADER, arg, copySize);

	return ret;
}

static int venc_v3_proc_putheader_result(vpu_enc_drv_t *vdata, void *arg)
{
	int ret = 0;

	ret = venc_cmd_get_user_result(vdata, arg, sizeof(venc_v3_putheader_t), VPU_CMD_ENC_PUT_HEADER);

	return ret;
}

static int venc_v3_proc_encode(vpu_enc_drv_t *vdata, void *arg)
{
	int ret = 0;
	size_t copySize = 0;

	copySize = sizeof(venc_v3_encode_t);

	ret = venc_cmd_put(vdata, VPU_CMD_ENC_ENCODE, arg, copySize);

	return ret;
}

static int venc_v3_proc_encode_result(vpu_enc_drv_t *vdata, void *arg)
{
	int ret = 0;

	ret = venc_cmd_get_user_result(vdata, arg, sizeof(venc_v3_encode_t), VPU_CMD_ENC_ENCODE);

	return ret;
}


static int venc_v3_proc_close(vpu_enc_drv_t *vdata, void *arg)
{
	int ret = 0;
	size_t copySize = 0;

	copySize = sizeof(venc_v3_close_t);

	ret = venc_cmd_put(vdata, VPU_CMD_ENC_CLOSE, arg, copySize);

	return ret;
}

static int venc_v3_proc_close_result(vpu_enc_drv_t *vdata, void *arg)
{
	int ret = 0;

	ret = venc_cmd_get_user_result(vdata, arg, sizeof(venc_v3_close_t), VPU_CMD_ENC_CLOSE);

	return ret;
}

static int venc_v3_alloc_memory(vpu_enc_drv_t *vdata, void *arg)
{
	int ret = 0;
	vpu_mem_alloc_t user_alloc_info;
	MEM_ALLOC_INFO_t alloc_info;
	int instance_id;
	int format_type;

	if (venc_is_from_kernel(vdata) == true) {
		vetc_memcpy(&user_alloc_info, (vpu_mem_alloc_t *)arg, sizeof(vpu_mem_alloc_t), 0);
	} else {
		if (copy_from_user(&user_alloc_info, (vpu_mem_alloc_t *)arg, sizeof(vpu_mem_alloc_t)) != 0) {
			ret = -EFAULT;
		}
	}

	vdata->info.codec_id = user_alloc_info.codec;
	format_type = vmgr_get_bitstream_format(vdata->info.codec_id, VPU_OP_TYPE_ENC);

	//If memory allocation is being attempted when the VPU encoder is not initialized
	if (vdata->info.pmap_type == VPU_PMAP_MAX) {
		instance_id = (int)vdata->info.drv_id + VPU_ENC;
		vmem_get_instance(&instance_id);

		vdata->info.pmap_type = (enum vpu_pmap_type)instance_id;
		dlog_venc("enc_id:%u, map_type:%s, instance_id:%d, format_type:%d, codec_id:%d", vdata->info.drv_id, vmgr_get_pmap_name((const int)vdata->info.pmap_type), instance_id, format_type, vdata->info.codec_id);
	}

	vetc_memset(&alloc_info, 0x00, sizeof(MEM_ALLOC_INFO_t), 0);

	alloc_info.buffer_type = vmgr_convert_buffer_type(user_alloc_info.buffer_type);
	alloc_info.request_size = user_alloc_info.request_size;

	dlog_venc("buffer type:%d, req size:0x%x", alloc_info.buffer_type, alloc_info.request_size);
	ret = vmem_proc_alloc_memory(format_type, &alloc_info, (vputype)vdata->info.pmap_type);
	if (ret == 0) {
		user_alloc_info.phy_addr = alloc_info.phy_addr;
		user_alloc_info.kernel_remap_addr = alloc_info.kernel_remap_addr;
		dlog_venc("buffer phy_addr:0x%x, kernel_remap_addr:0x%x, size:%d", alloc_info.phy_addr, alloc_info.kernel_remap_addr, alloc_info.request_size);
		if (venc_is_from_kernel(vdata) == true) {
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

static long venc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	union {
		unsigned long ul_data;
		int *pi_data;	//NULL
		void *pv_data;
	} uarg;

	int ret = 0;
	vpu_enc_drv_t *vdata = (vpu_enc_drv_t *)filp->private_data;

	if (cmd >= KERNEL_OFFSET) {
		cmd -= KERNEL_OFFSET;

		if (vdata->info.is_kernel_call != true) {
			vdata->info.is_kernel_call = true;
			dlog_venc("enc_id:%u, type:%d, ioctl:%s(%d), is_kernel_call:%d", vdata->info.drv_id, vdata->info.pmap_type, venc_ioctl_name(cmd), cmd, vdata->info.is_kernel_call);
		}
	}

	uarg.pi_data = NULL;
	uarg.ul_data = arg;

	dlog_venc("--- enc_id:%u, type:%d, ioctl:%s(%d), k:%d", vdata->info.drv_id, vdata->info.pmap_type, venc_ioctl_name(cmd), cmd, vdata->info.is_kernel_call);

	switch (cmd) {
	case VPU_V3_GET_DRV_VERSION_SYNC:
	{
		ret = venc_v3_proc_get_drv_version(vdata, uarg.pv_data);
	}
	break;

	case VPU_V3_GET_CAPABILITY_SYNC:
	{
		ret = venc_v3_proc_capability(vdata, uarg.pv_data);
	}
	break;

	case VENC_V3_INIT:
	{
		ret = venc_v3_proc_init(vdata, uarg.pv_data);
	}
	break;

	case VENC_V3_INIT_RESULT:
	{
		ret = venc_v3_proc_init_result(vdata, uarg.pv_data);
	}
	break;

	case VENC_V3_PUT_HEADER:
	{
		ret = venc_v3_proc_putheader(vdata, uarg.pv_data);
	}
	break;

	case VENC_V3_PUT_HEADER_RESULT:
	{
		ret = venc_v3_proc_putheader_result(vdata, uarg.pv_data);
	}
	break;

	case VENC_V3_ENCODE:
	{
		ret = venc_v3_proc_encode(vdata, uarg.pv_data);
	}
	break;

	case VENC_V3_ENCODE_RESULT:
	{
		ret = venc_v3_proc_encode_result(vdata, uarg.pv_data);
	}
	break;

	case VENC_V3_CLOSE:
	{
		ret = venc_v3_proc_close(vdata, uarg.pv_data);
	}
	break;

	case VENC_V3_CLOSE_RESULT:
	{
		ret = venc_v3_proc_close_result(vdata, uarg.pv_data);
	}
	break;

	case VENC_V3_ALLOC_MEMORY_SYNC:
	{
		ret = venc_v3_alloc_memory(vdata, uarg.pv_data);
	}
	break;

	default:
	{
		ret = (int)-EFAULT;
	}
	break;
	}

	if (ret < 0) {
		err_venc("enc_id:%u, type:%d, ioctl:%s(%d), ret:%d", vdata->info.drv_id, vdata->info.pmap_type, venc_ioctl_name(cmd), cmd, ret);
	}

	return ret;
}

static int venc_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	vpu_drv_shared_t *shared = dev_get_drvdata(misc->parent);	//return dev->p->driver_data
	vpu_enc_drv_t *vdata = NULL;
	unsigned int encoder_idx;

	vdata = VPU_alloc(sizeof(vpu_enc_drv_t));
	if (vdata != NULL) {
		//set each device private
		vdata->enc_shared = (vpu_drv_shared_t *)shared;
		filp->private_data = vdata;

		vetc_mutex_lock(&shared->shared_mutex);

		vdata->info.drv_id = VPU_DRV_ID_MAX;
		for (encoder_idx = 0U; encoder_idx < VPU_DRV_ID_MAX; encoder_idx++) {
			if (encoder_map[encoder_idx] == false) {
				//set used
				encoder_map[encoder_idx] = true;
				vdata->info.drv_id = encoder_idx;
				dlog_venc("encoder id:%d set used", vdata->info.drv_id);
				break;
			}
		}

		atomic_inc(&shared->reference_count);
		vetc_mutex_unlock(&shared->shared_mutex);

		if (vdata->info.drv_id < VPU_DRV_ID_MAX) {
			(void)venc_init(vdata);
		} else {
			ret = -ENODEV;
		}
	} else {
		ret = -ENOMEM;
	}

	if (ret != 0) {
		if (vdata != NULL) {
			VPU_free(vdata);
			vdata = NULL;
		}
	}

	//dlog_venc("venc open, enc_id:%u, vdata:%p, ref_count:%d", vdata->info.drv_id, vdata, atomic_read(&shared->reference_count));
	return ret;
}

static int venc_release(struct inode *inode, struct file *filp)
{
	vpu_enc_drv_t *vdata = (vpu_enc_drv_t *)filp->private_data;
	vpu_drv_shared_t *shared = vdata->enc_shared;

	atomic_dec(&shared->reference_count);
	mutex_lock(&shared->shared_mutex);

	(void)venc_deinit(vdata);
	encoder_map[vdata->info.drv_id] = false;
	dlog_venc("encoder id:%d, set unused\n", vdata->info.drv_id);

	mutex_unlock(&shared->shared_mutex);

	VPU_free(vdata);

	//dlog_venc("venc release, ref_count:%d", atomic_read(&shared->reference_count));
	return 0;
}

static const struct file_operations vdev_enc_fops = {
	.owner = THIS_MODULE,
	.open = venc_open,
	.release = venc_release,
	.mmap = venc_mmap,
	.unlocked_ioctl = venc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = venc_ioctl,
#endif
	.poll = venc_poll,
};

int venc_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct miscdevice *misc = NULL;
	vpu_drv_shared_t *enc_shared = NULL;

	dlog_venc("VPU %s Driver(id:%d) probe", pdev->name, pdev->id);

	enc_shared = (vpu_drv_shared_t *)VPU_alloc(sizeof(vpu_drv_shared_t));
	if (enc_shared == NULL) {
		ret = -ENOMEM;
		V_DBG(VPU_DBG_ERROR, "enc_shared alloc failed");
	} else {
		misc = &enc_shared->misc;
		mutex_init(&enc_shared->shared_mutex);

		misc->minor = MISC_DYNAMIC_MINOR;
		misc->fops = &vdev_enc_fops;
		misc->name = pdev->name;
		misc->parent = &pdev->dev; //to get shared ptr from vdec_open()

		if (misc_register(misc) != 0) {
			V_DBG(VPU_DBG_ERROR, "VPU %s: Couldn't register device.", pdev->name);
			ret = -EBUSY;
		} else {
			platform_set_drvdata(pdev, enc_shared);
		}
	}

	if (ret != 0) {
		if (enc_shared != NULL) {
			VPU_free(enc_shared);
		}

		(void)pr_info("VPU %s Driver(id:%d) Initialize Failed", pdev->name, pdev->id);
	} else {
		(void)pr_info("VPU %s Driver(id:%d) Initialized.", pdev->name, pdev->id);
	}

	return ret;
}
EXPORT_SYMBOL(venc_probe);

VREMOVE_RET_TYPE venc_remove(struct platform_device *pdev)
{
	vpu_drv_shared_t *enc_shared = (vpu_drv_shared_t *)platform_get_drvdata(pdev);

	misc_deregister(&enc_shared->misc);

	if (enc_shared != NULL) {
		VPU_free(enc_shared);
	}

	//dlog_venc("venc removed");
	VREMOVE_RETURN();
}
EXPORT_SYMBOL(venc_remove);

#endif //#if DEFINED_CONFIG_VENC

