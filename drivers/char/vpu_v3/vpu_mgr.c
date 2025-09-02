/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

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
#include "vpu_mgr_encode.h"
#include "vpu_mgr_decode.h"
#include "vpu_dllist.h"

#define dlog_vmgr(fmt, args...)		V_DBG(VPU_DBG_INFO, "[VPU_MGR][LOG]:" fmt, ## args)
#define detail_vmgr(fmt, args...)	V_DBG(VPU_DBG_DETAIL, "[VPU_MGR][DETAIL]:" fmt, ## args)
#define seq_vmgr(fmt, args...)		V_DBG(VPU_DBG_CMD, "[VPU_MGR][SEQ]:"  fmt, ## args)
#define err_vmgr(fmt, args...)		V_DBG(VPU_DBG_ERROR, "[VPU_MGR][ERR]:"  fmt, ## args)

#define dlog_info(fmt, args...)		V_DBG(VPU_DBG_INFO, "[VPU_MGR][LOG][id:%u]:" fmt, drv_info->drv_id, ## args)
#define detail_info(fmt, args...)	V_DBG(VPU_DBG_DETAIL, "[VPU_MGR][DETAIL][id:%u]:" fmt, drv_info->drv_id, ## args)
#define seq_info(fmt, args...)		V_DBG(VPU_DBG_CMD, "[VPU_MGR][SEQ][id:%u]:"  fmt, drv_info->drv_id, ## args)
#define err_info(fmt, args...)		V_DBG(VPU_DBG_ERROR, "[VPU_MGR][ERR][id:%u]:"  fmt, drv_info->drv_id, ## args)

#define DBG(fmt, args...)	do { pr_err("[DEC][%s:%d]" fmt "\n", __FUNCTION__, __LINE__, ## args); } while (0)

//#define VPU_REGISTER_DUMP
// for interlaced format with fileplay-mode or all format w/ ring-mode

//#define SHOW_POLL_UP_COUNT_LOG
//#define SHOW_CAPABILITY_LOG

#define INVALID_VPU_INDEX	(0x99)

#if defined(USE_ACCESS_POINT)
// SHARE_POINT_ORDER_XXX :
// VPU = 0, JPU = 1, HEVC = 2,
// 4KD2 = 3, HEVC_ENC = 4, HEVC_ENC_2 = 5
#define SHARE_POINT_ORDER_VPU 0U

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
static vpu_accesspoint_t *vmgr_accesspoint_get_addr(const char *accesspoint_path)
{
	vpu_accesspoint_t *accesspoint_base = NULL;
	vpu_accesspoint_t *accesspoint = NULL;
	mm_segment_t oldfs;
	int ret = 0;

	union {
		struct file *filp;
		void *pv_data;
	} uidata;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
	oldfs = get_fs();
	set_fs(get_ds());
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
	oldfs = get_fs();
	set_fs(KERNEL_DS);
#else
	oldfs = force_uaccess_begin();
#endif

	uidata.filp = filp_open(accesspoint_path, O_RDONLY, 420); // 0644 to 420
	if (IS_ERR(uidata.pv_data)) {
		err_vmgr("%s file open fail!!\n", accesspoint_path);
		ret = -1;
	} else {
		char tmpdata[20];
		unsigned long long res = 0;
		u32 idx = 0;

		idx = (u32)((u32)sizeof(void *)*2U) + 2U;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
		ret = vfs_read(uidata.filp, tmpdata, sizeof(tmpdata), &uidata.filp->f_pos);
#else
		ret = uidata.filp->f_op->read(uidata.filp, tmpdata, sizeof(tmpdata), &uidata.filp->f_pos);
#endif

		tmpdata[idx] = '\0';

		//convert string to unsigned long long
		ret = kstrtoull(tmpdata, 16, &res);

		(void)memmove((void *)&accesspoint_base, (void *)&res, sizeof(unsigned long));

		(void)filp_close(uidata.filp, NULL);
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
	set_fs(oldfs);
#else
	force_uaccess_end(oldfs);
#endif

	if (ret == 0) {
		accesspoint = (vpu_accesspoint_t *)VPU_alloc(sizeof(vpu_accesspoint_t));
		if (accesspoint != NULL) {
			vetc_memcpy(accesspoint, accesspoint_base, sizeof(vpu_accesspoint_t), 0);

			detail_vmgr("VPU CheckCode %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
				  GET_FOURCC_1(accesspoint->check_code1),
				  GET_FOURCC_2(accesspoint->check_code1),
				  GET_FOURCC_3(accesspoint->check_code1),
				  GET_FOURCC_4(accesspoint->check_code1),
				  GET_FOURCC_1(accesspoint->check_code2),
				  GET_FOURCC_2(accesspoint->check_code2),
				  GET_FOURCC_3(accesspoint->check_code2),
				  GET_FOURCC_4(accesspoint->check_code2),
				  GET_FOURCC_1(accesspoint->check_code3),
				  GET_FOURCC_2(accesspoint->check_code3),
				  GET_FOURCC_3(accesspoint->check_code3),
				  GET_FOURCC_4(accesspoint->check_code3),
				  GET_FOURCC_1(accesspoint->check_code4),
				  GET_FOURCC_2(accesspoint->check_code4),
				  GET_FOURCC_3(accesspoint->check_code4),
				  GET_FOURCC_4(accesspoint->check_code4));

			if (((CHECK_CODE_01 | accesspoint->check_code1) == CHECK_CODE_01) &&
				((CHECK_CODE_02 | accesspoint->check_code2) == CHECK_CODE_02) &&
				((CHECK_CODE_03 | accesspoint->check_code3) == CHECK_CODE_03) &&
				((CHECK_CODE_04 | accesspoint->check_code4) == CHECK_CODE_04)) {
				ret = 0;
				detail_vmgr("%s get address succeed", accesspoint_path);
			} else {
				ret = -1;
				err_vmgr("%s get address failed", accesspoint_path);
			}
		}
	} else {
		err_vmgr("%s get address has some problem", accesspoint_path);
	}

	return accesspoint;
}

#else

static vpu_accesspoint_t *vmgr_accesspoint_get_addr_mem(enum vpu_ip_type ip_type)
{
	int ret = 0;
	void *va = NULL;
	unsigned int addr_offset = 0U;

	vpu_accesspoint_t *accesspoint = NULL;

	switch (ip_type) {
	case VPU_IP_C7: //VPU_IP_D6
		addr_offset = 0U;
	break;

	case VPU_IP_4KD2:
		addr_offset = 3U;
	break;

	case VPU_IP_HEVC_ENC:
		addr_offset = 4U;
	break;

	case VPU_IP_HEVC_ENC2:
		addr_offset = 5U;
	break;

	case VPU_IP_JPU_C6:
		addr_offset = 1U;
	break;

	case VPU_IP_HEVC_DEC:
		addr_offset = 2U;
	break;

	default:
		addr_offset = 0U;
	break;
	}

	accesspoint = (vpu_accesspoint_t *)VPU_alloc(sizeof(vpu_accesspoint_t));
	if (accesspoint != NULL) {
		va = vetc_ioremap(SHARE_POINT_ADDR + (SHARD_POINT_GAP * addr_offset), SHARD_POINT_GAP);

		if (va == NULL) {
			V_DBG(VPU_DBG_ERROR, "ioremap failed");
			ret = -ENOMEM;
		} else {
			memcpy(accesspoint, va, sizeof(vpu_accesspoint_t));

			V_DBG(VPU_DBG_INFO, "remap (PA : 0x%08x / VA : 0x%p) Dec ADDR : 0x%p",
					SHARE_POINT_ADDR, va,
					accesspoint->tccfp_vpu_dec);

			iounmap(va);
		}
	}

	return accesspoint;
}

#endif

int vmgr_accesspoint_check_addr_valid(vpu_accesspoint_t *accesspoint)
{
	int ret = -1;

	if (accesspoint != NULL) {
		if (((CHECK_CODE_01 | accesspoint->check_code1) == CHECK_CODE_01) &&
				((CHECK_CODE_02 | accesspoint->check_code2) == CHECK_CODE_02) &&
				((CHECK_CODE_03 | accesspoint->check_code3) == CHECK_CODE_03) &&
				((CHECK_CODE_04 | accesspoint->check_code4) == CHECK_CODE_04)) {
			ret = 0;
		} else {
			err_vmgr("VPU CheckCode %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
					GET_FOURCC_1(accesspoint->check_code1),
					GET_FOURCC_2(accesspoint->check_code1),
					GET_FOURCC_3(accesspoint->check_code1),
					GET_FOURCC_4(accesspoint->check_code1),
					GET_FOURCC_1(accesspoint->check_code2),
					GET_FOURCC_2(accesspoint->check_code2),
					GET_FOURCC_3(accesspoint->check_code2),
					GET_FOURCC_4(accesspoint->check_code2),
					GET_FOURCC_1(accesspoint->check_code3),
					GET_FOURCC_2(accesspoint->check_code3),
					GET_FOURCC_3(accesspoint->check_code3),
					GET_FOURCC_4(accesspoint->check_code3),
					GET_FOURCC_1(accesspoint->check_code4),
					GET_FOURCC_2(accesspoint->check_code4),
					GET_FOURCC_3(accesspoint->check_code4),
					GET_FOURCC_4(accesspoint->check_code4)
				  );
		}
	} else {
		err_vmgr("NULL Access Point");
	}

	return ret;
}
#endif // defined(USE_ACCESS_POINT)

static unsigned long vdec_gettime_us(void)
{
	unsigned long time_us;
	ktime_t now;

	//ktime_sub (Rettime, starttime)
	now = ktime_get();
	time_us = ktime_to_us(now);

	return time_us;
}

static int vmgr_set_drv_info(vpu_mgr_t *mgr_ctx, const enum vpu_op_type op_type, const unsigned int drv_id, vpu_drv_info_t *info)
{
	int ret = 0;

	if (((op_type == VPU_OP_TYPE_DEC) || (op_type == VPU_OP_TYPE_ENC)) && (drv_id < VPU_DRV_ID_MAX)) {
		mgr_ctx->drv_info[op_type][drv_id] = info;
	} else {
		err_vmgr("invalid parameter, op_type:%d, drv_id:%d", op_type, drv_id);
		ret = -1;
	}

	return ret;
}

static vpu_drv_info_t *vmgr_get_drv_info(vpu_mgr_t *mgr_ctx, const enum vpu_op_type op_type, const unsigned int drv_id)
{
	vpu_drv_info_t *info = NULL;

	if (((op_type == VPU_OP_TYPE_DEC) || (op_type == VPU_OP_TYPE_ENC)) && (drv_id < VPU_DRV_ID_MAX)) {
		//The access is always through unique id and op_type, hence, a mutex is not necessary
		info = mgr_ctx->drv_info[op_type][drv_id];
	}

	return info;
}

static int vmgr_set_last_cmd(vpu_mgr_t *mgr_ctx, const enum vpu_op_type op_type, const unsigned int drv_id, int cmd)
{
	vpu_drv_info_t *drvinfo = vmgr_get_drv_info(mgr_ctx, op_type, drv_id);

	if (drvinfo != NULL) {
		drvinfo->last_cmd = cmd;
		drvinfo->count_cmd++;
		drvinfo->cmd_start_time = vdec_gettime_us();
	}

	return 0;
}

static long vmgr_set_last_cmd_finish(vpu_mgr_t *mgr_ctx, const enum vpu_op_type op_type, const unsigned int drv_id, int cmd)
{
	long ret_time = 0L;
	unsigned long fin_time_us = 0UL;
	vpu_drv_info_t *drvinfo = vmgr_get_drv_info(mgr_ctx, op_type, drv_id);

	if (drvinfo != NULL) {
		fin_time_us = vdec_gettime_us();

		drvinfo->cmd_proc_time_us = fin_time_us - drvinfo->cmd_start_time;
		ret_time = drvinfo->cmd_proc_time_us;
	}

	return ret_time;
}

int vmgr_get_alive(vpu_mgr_t *mgr_ctx)
{
	return atomic_read(&mgr_ctx->dev_opened);
}

EXPORT_SYMBOL(vmgr_get_alive);


bool vmgr_get_opened(vpu_mgr_t *mgr_ctx, const enum vpu_op_type op_type, const unsigned int drv_id)
{
	bool opened = false;
	vpu_drv_info_t *drvinfo = NULL;

	drvinfo = vmgr_get_drv_info(mgr_ctx, op_type, drv_id);

	if (drvinfo != NULL) {
		opened = drvinfo->opened;
	}

	return opened;
}

EXPORT_SYMBOL(vmgr_get_opened);

int vmgr_set_opened(vpu_mgr_t *mgr_ctx, const enum vpu_op_type op_type, const unsigned int drv_id, bool isOpened)
{
	int ret = 0;
	vpu_drv_info_t *drvinfo = NULL;

	drvinfo = vmgr_get_drv_info(mgr_ctx, op_type, drv_id);

	if (drvinfo != NULL) {
		drvinfo->opened = isOpened;
	} else {
		ret = -1;
	}

	return ret;
}

EXPORT_SYMBOL(vmgr_set_opened);

long vmgr_get_handle(vpu_mgr_t *mgr_ctx, const enum vpu_op_type op_type, const unsigned int drv_id)
{
	long handle;

	vpu_drv_info_t *drvinfo = vmgr_get_drv_info(mgr_ctx, op_type, drv_id);

	if (drvinfo != NULL) {
		handle = drvinfo->handle;
	} else {
		handle = -1;
	}

	return handle;
}

EXPORT_SYMBOL(vmgr_get_handle);


int vmgr_set_handle(vpu_mgr_t *mgr_ctx, const enum vpu_op_type op_type, const unsigned int drv_id, long handle)
{
	int ret = 0;
	vpu_drv_info_t *drvinfo = vmgr_get_drv_info(mgr_ctx, op_type, drv_id);

	if (drvinfo != NULL) {
		drvinfo->handle = handle;
	} else {
		ret = -1;
	}

	return ret;
}

EXPORT_SYMBOL(vmgr_set_handle);

unsigned long vmgr_add_accumulated_pixelproduct(vpu_mgr_t *mgr_ctx, unsigned long pixel_product)
{
	unsigned long ret_value = 0UL;

	vetc_mutex_lock(&mgr_ctx->vmgr_mutex);
	if (pixel_product < ULONG_MAX - mgr_ctx->accumulated_pixel_product) {
		mgr_ctx->accumulated_pixel_product += pixel_product;
		dlog_vmgr("ip:%s, add accumulated_pixel_product:%lu, pixel_product:%lu", vmgr_get_ip_name(mgr_ctx->each_ip->ip_type), mgr_ctx->accumulated_pixel_product, pixel_product)
	}
	ret_value = mgr_ctx->accumulated_pixel_product;
	vetc_mutex_unlock(&mgr_ctx->vmgr_mutex);

	return ret_value;
}

EXPORT_SYMBOL(vmgr_add_accumulated_pixelproduct);

unsigned long vmgr_sub_accumulated_pixelproduct(vpu_mgr_t *mgr_ctx, unsigned long pixel_product)
{
	unsigned long ret_value = 0UL;

	vetc_mutex_lock(&mgr_ctx->vmgr_mutex);
	if (pixel_product > mgr_ctx->accumulated_pixel_product) {
		mgr_ctx->accumulated_pixel_product = 0;
    } else {
	    // Subtract the pixel product value
	    mgr_ctx->accumulated_pixel_product -= pixel_product;
	}

	ret_value = mgr_ctx->accumulated_pixel_product;
	dlog_vmgr("ip:%s, sub accumulated_pixel_product:%lu, pixel_product:%lu", vmgr_get_ip_name(mgr_ctx->each_ip->ip_type), mgr_ctx->accumulated_pixel_product, pixel_product)

	vetc_mutex_unlock(&mgr_ctx->vmgr_mutex);

   return ret_value;
}

EXPORT_SYMBOL(vmgr_sub_accumulated_pixelproduct);

unsigned long vmgr_get_accumulated_pixelproduct(vpu_mgr_t *mgr_ctx)
{
	unsigned long pixel_product = 0UL;

	vetc_mutex_lock(&mgr_ctx->vmgr_mutex);
	pixel_product = mgr_ctx->accumulated_pixel_product;
	vetc_mutex_unlock(&mgr_ctx->vmgr_mutex);

	return pixel_product;
}

EXPORT_SYMBOL(vmgr_get_accumulated_pixelproduct);

vpu_cmd_t *vmgr_list_manager_alloc(vpu_mgr_t *mgr_ctx)
{
	vpu_cmd_t *node = NULL;

	if (mgr_ctx != NULL) {
		node = vmgr_list_alloc(&mgr_ctx->comm_data);
	}

	return node;
}

EXPORT_SYMBOL(vmgr_list_manager_alloc);

int vmgr_list_manager_add(vpu_mgr_t *mgr_ctx, vpu_cmd_t *args)
{
	int ret = 0;

	if (mgr_ctx != NULL) {
		vmgr_list_add(&mgr_ctx->comm_data, args);
	} else {
		ret = -1;
	}

	return ret;
}

EXPORT_SYMBOL(vmgr_list_manager_add);

vpu_cmd_t *vmgr_list_manager_show_result(vpu_mgr_t *mgr_ctx, const enum vpu_op_type op_type, const unsigned int drv_id)
{
	vpu_cmd_t *cmd = NULL;

	if (mgr_ctx != NULL) {
		cmd = vmgr_list_show_result(&mgr_ctx->comm_data, op_type, drv_id);
	}

	return cmd;
}

EXPORT_SYMBOL(vmgr_list_manager_show_result);

vpu_cmd_t *vmgr_list_manager_get_result(vpu_mgr_t *mgr_ctx, const enum vpu_op_type op_type, const unsigned int drv_id)
{
	vpu_cmd_t *cmd = NULL;

	if (mgr_ctx != NULL) {
		cmd = vmgr_list_get_result(&mgr_ctx->comm_data, op_type, drv_id);
	}

	return cmd;
}

EXPORT_SYMBOL(vmgr_list_manager_get_result);

int vmgr_list_manager_result_remain(vpu_mgr_t *mgr_ctx, const enum vpu_op_type op_type, const unsigned int drv_id)
{
	int count = 0;

	if (mgr_ctx != NULL) {
		count = vmgr_list_get_result_count(&mgr_ctx->comm_data, op_type, drv_id);
	}

	return count;
}

EXPORT_SYMBOL(vmgr_list_manager_result_remain);

int vmgr_list_manager_add_pool(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd)
{
	int ret = 0;
	if (mgr_ctx != NULL) {
		ret = vmgr_list_add_pool(&mgr_ctx->comm_data, cmd);
	} else {
		ret = -1;
	}

	return ret;
}

EXPORT_SYMBOL(vmgr_list_manager_add_pool);

static void vmgr_close_all(vpu_mgr_t *mgr_ctx)
{
	int ii;

	for (ii = (int)VPU_DEC; ii < (int)VPU_ENC; ii++) {
		vmgr_set_opened(mgr_ctx, VPU_OP_TYPE_DEC, ii, false);
		vmem_proc_free_memory(ii);
	}

	for (ii = (int)VPU_ENC; ii < (int)VPU_MAX; ii++) {
		vmgr_set_opened(mgr_ctx, VPU_OP_TYPE_ENC, ii, false);
		vmem_proc_free_memory(ii);
	}
}

int vmgr_process_ex(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_list, const enum vpu_op_type op_type, const unsigned int drv_id, enum vpu_cmd_type cmd, int *result)
{
	//cmd_list is empty, so don't use the parameter of cmd_list
	cmd_list->cmd_id = 0;
	cmd_list->drv_id = drv_id;
	cmd_list->cmd_type = cmd;
	cmd_list->op_type = op_type;
	cmd_list->handle = vmgr_get_handle(mgr_ctx, op_type, drv_id);
	cmd_list->args = NULL;
	cmd_list->poll_data = NULL;
	cmd_list->result = VPU_RETCODE_SUCCESS;
	cmd_list->drv_info = vmgr_get_drv_info(mgr_ctx, op_type, drv_id);
	if (cmd_list->drv_info != NULL) {
		cmd_list->pmap_type = (vputype)cmd_list->drv_info->pmap_type;
	}

	vmgr_list_add(&mgr_ctx->comm_data, cmd_list);

	return 1;
}

EXPORT_SYMBOL(vmgr_process_ex);

int vmgr_external_all_close(vpu_mgr_t *mgr_ctx, int wait_ms)
{
	int type;
	int max_count;
	int ret;

	for (type = VPU_DEC; type < VPU_ENC; type++) {
		if ((vmgr_get_opened(mgr_ctx, VPU_OP_TYPE_DEC, type) == true) && (vmgr_get_handle(mgr_ctx, VPU_OP_TYPE_DEC, type) != 0x00)) {
			vpu_cmd_t *node = NULL;
			detail_vmgr("[%s] call vmgr_process_ex, type:%d", vmgr_get_ip_name(mgr_ctx->each_ip->ip_type), type);

			node = vmgr_list_alloc(&mgr_ctx->comm_data);
			if (vmgr_process_ex(mgr_ctx, node, VPU_OP_TYPE_DEC, type, VPU_DEC_CLOSE, &ret)) {
				max_count = wait_ms / 10;

				while (vmgr_get_opened(mgr_ctx, VPU_OP_TYPE_DEC, type) == true) {
					max_count--;
					usleep_range(10000, 11000); //msleep(10);
				}
			}
		}
	}

	for (type = VPU_ENC; type < VPU_MAX; type++) {
		if ((vmgr_get_opened(mgr_ctx, VPU_OP_TYPE_ENC, type) == true) && (vmgr_get_handle(mgr_ctx, VPU_OP_TYPE_ENC, type) != 0x00)) {
			vpu_cmd_t *node = NULL;
			detail_vmgr("[%s] call vmgr_process_ex, type:%d", vmgr_get_ip_name(mgr_ctx->each_ip->ip_type), type);

			node = vmgr_list_alloc(&mgr_ctx->comm_data);
			if (vmgr_process_ex(mgr_ctx, node, VPU_OP_TYPE_ENC, type, VPU_ENC_CLOSE, &ret)) {
				max_count = wait_ms / 10;

				while (vmgr_get_opened(mgr_ctx, VPU_OP_TYPE_ENC, type) == true) {
					max_count--;
					usleep_range(10000, 11000); //msleep(10);
				}
			}
		}
	}

	return 0;
}

//Since it is used only in vpu_mgr_encode and vpu_mgr_decode, a mutex is not necessary
int vmgr_alloc_ip_parameter(vpu_drv_info_t *drv_info, unsigned int param_size)
{
	int ret = 0;

	if (param_size == 0) {
		ret = -1;
		err_vmgr("[%s][id:%d] the value of param_size input is 0", vmgr_get_optype_name(drv_info->op_type), drv_info->drv_id);
	} else {
		if (drv_info->ip_param != NULL) {
			err_vmgr("[%s][id:%d] since ip_param(0x%x) is already allocated, it needs to be released before allocating again.", vmgr_get_optype_name(drv_info->op_type), drv_info->drv_id, drv_info->ip_param);
			VPU_free(drv_info->ip_param);
			drv_info->ip_param = NULL;
		}

		drv_info->ip_param = VPU_alloc(param_size);
		if (drv_info->ip_param != NULL) {
			vetc_memset(drv_info->ip_param, 0x00, param_size, 0);
		} else {
			err_vmgr("[%s][id:%d] failed to allocate ip_param", vmgr_get_optype_name(drv_info->op_type), drv_info->drv_id);
			ret = -1;
		}
	}

	return ret;
}

//Since it is used only in vpu_mgr_encode and vpu_mgr_decode, a mutex is not necessary
int vmgr_release_ip_parameter(vpu_drv_info_t *drv_info)
{
	int ret = 0;

	if (drv_info->ip_param != NULL) {
		VPU_free(drv_info->ip_param);
		drv_info->ip_param = NULL;
	} else {
		err_vmgr("[%s][id:%d] ip_param is not allocated, so it cannot be released", vmgr_get_optype_name(drv_info->op_type), drv_info->drv_id);
		ret = -1;
	}

	return ret;
}

static void vmgr_poll_wake_up(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd)
{
	atomic_inc(&cmd->poll_data->count);
	detail_vmgr("[id:%d] cmd:%s, cmd_queued(%d), poll wakeup count = %d", cmd->drv_id, vmgr_cmd_name(cmd->cmd_type), mgr_ctx->comm_data.cmd_queued, atomic_read(&cmd->poll_data->count));

#if defined(SHOW_POLL_UP_COUNT_LOG)
	if (atomic_read(&cmd->poll_data->count) != 1) {
		detail_vmgr("[id:%d] poll wakeup count = %d, cmd(0x%x)",
			cmd->drv_id,
			atomic_read(&cmd->poll_data->count),
			cmd->cmd_type);
	}
#endif //SHOW_POLL_UP_COUNT_LOG

	//enc/dec driver wake up in polling
	wake_up_interruptible(&cmd->poll_data->wq);
}

#if defined(ENABLE_INTERLACE_DELAY_PROCESS)
#define DEFAULT_TIMEOUT_MSEC 200
static int gs_timeout_msec = DEFAULT_TIMEOUT_MSEC;

static vpu_cmd_t *vmgr_get_pending_escapable(vpu_mgr_t *mgr_ctx, vpu_drv_info_t *drv_info)
{
	vpu_cmd_t *cmd = NULL;
	vmgr_comm_t *cmdQueue = &mgr_ctx->comm_data;
	unsigned int vmgr_timeout_msec = (unsigned int)gs_timeout_msec;
	unsigned int time_coefficient = 3U;
	unsigned int timeout_msec = vmgr_timeout_msec * time_coefficient;

	unsigned int time_diff = ((jiffies < mgr_ctx->field_processed_timestamp) || (mgr_ctx->field_processed_timestamp == 0)) ? 0u
							: jiffies - mgr_ctx->field_processed_timestamp;

	unsigned long jtimeout = msecs_to_jiffies(timeout_msec);
	bool timed_out = (time_diff > jtimeout);

	//when a FLUSH or CLOSE command is input, it quickly ends the wait(timed_off) and proceeds to handle the respective command.
	bool timed_off = (vmgr_list_show_dec_cmd_with_emergency(cmdQueue, drv_info->drv_id) != NULL);
	vpu_dllist_t *delay_queue = drv_info->delay_queue;

	//dlog_vmgr("time_diff:%lu, jtimeout:%lu, field_processed_timestamp:%lu", time_diff, jtimeout, mgr_ctx->field_processed_timestamp);

	if (timed_out || timed_off) {
		//Continued waiting will eventually lead to a timeout
		//forces retrieval of the data of the 2nd field from the queue to resolve the pending state
		dlog_vmgr("timed %s, force get next cmd", timed_out ? "out" : "off");

		cmd = (vpu_cmd_t *)vpu_dllist_remove_head_sync(delay_queue);
		if (cmd != NULL) {
			cmd->drv_info->avoid_pending = 1U;
			cmd->drv_info->initial_wakeup_poll = 0U;
		}
	} else {
		//if there is only one, wait (no action for other instances if pending).
		//
		if (jtimeout - time_diff < 5) {
			dlog_vmgr("pending elapsed: %ld ms <= %ld ms", msecs_to_jiffies(time_diff), msecs_to_jiffies(jtimeout));
		}

		dlog_vmgr("wait ... jtimeout:%u, time_diff:%lu", jtimeout, time_diff);
	}

	return cmd;
}


static void vmgr_wakeup_user_poll(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd)
{
	vpu_cmd_t *tmp_cmd = NULL;
	vdec_v3_decode_t *arg_decode = NULL;
	vmgr_comm_t *cmdQueue = &mgr_ctx->comm_data;

	//check condition
	// 1. when first frame delay, need to create cmd to wake up with fake
	if (cmd->drv_info->temp_decoding_result == NULL) {
		//create fake result to user, to insert next frame
		tmp_cmd = vmgr_list_manager_alloc(mgr_ctx);
		if (tmp_cmd != NULL) {
			arg_decode = (vdec_v3_decode_t *)VPU_alloc(sizeof(vdec_v3_decode_t));
			if (arg_decode != NULL) {
				//copy data
				vetc_memcpy(tmp_cmd, cmd, sizeof(vpu_cmd_t), 0);
				vetc_memcpy(arg_decode, cmd->args, sizeof(vdec_v3_decode_t), 0);

				arg_decode->result = VPU_RETCODE_FRAME_NOT_COMPLETE;
				arg_decode->output.out_info.display_status = VPU_DISP_STAT_FAIL;
				arg_decode->output.out_info.decoded_status = VPU_DEC_STAT_NONE;

				tmp_cmd->args = arg_decode;

				(void)vmgr_dec_decode_update_bitstream_addr(mgr_ctx, tmp_cmd, cmd->drv_info);

				dlog_vmgr("drv_id:%d, add fake result to result_queue", cmd->drv_id);
				vmgr_list_add_result(cmdQueue, tmp_cmd);
			} else {
				VPU_free(tmp_cmd);
				tmp_cmd = NULL;
			}
		}
	} else {
		dlog_vmgr("drv_id:%d, add temp_result to result_queue", cmd->drv_id, cmd->drv_info->temp_decoding_result);
		vmgr_list_add_result(cmdQueue, (vpu_cmd_t *)cmd->drv_info->temp_decoding_result);
		cmd->drv_info->temp_decoding_result = NULL;
	}

	dlog_vmgr("drv_id:%d, force to wake up", cmd->drv_id);
	vmgr_poll_wake_up(mgr_ctx, cmd);
}

static vpu_cmd_t *vmgr_cmd_delay_for_interlace(vpu_mgr_t *mgr_ctx)
{
	vpu_cmd_t *cmd = NULL;
	vmgr_comm_t *cmdQueue = &mgr_ctx->comm_data;

	int delay_count = 0;

	//If there is a pending DRV ID
	dlog_vmgr("pending drv_id:%d, cmd queue:%d", mgr_ctx->pending_drv_id, vmgr_list_get_cmd_count(cmdQueue));

	if (mgr_ctx->pending_drv_id != INVALID_DRV_ID) {
		vpu_drv_info_t *drv_info = NULL;

		drv_info = mgr_ctx->drv_info[VPU_OP_TYPE_DEC][mgr_ctx->pending_drv_id];
		if ((drv_info != NULL) && (drv_info->enable_avoid_pending == 1U)) {
			vpu_cmd_t *cmd_decode = NULL;

			//dlog_vmgr("drv_id:%d, queue count in cmdQueue:%d", mgr_ctx->pending_drv_id,
			//	vmgr_list_get_cmd_count_with_criteria(cmdQueue, mgr_ctx->pending_drv_id, VPU_OP_TYPE_DEC, VPU_CMD_DEC_DECODE));
			cmd_decode = vmgr_list_get_cmd_with_criteria(cmdQueue, mgr_ctx->pending_drv_id, VPU_OP_TYPE_DEC, VPU_CMD_DEC_DECODE);
			if (cmd_decode != NULL) {
				vpu_dllist_insert_tail(cmd_decode->drv_info->delay_queue, (vpu_dllist_node_t *)cmd_decode);
				dlog_vmgr("pending drv_id:%d, got from cmdQueue, delay_queue count:%d", mgr_ctx->pending_drv_id, vpu_dllist_get_count_sync(cmd_decode->drv_info->delay_queue));

				cmd_decode = NULL;
			}

			delay_count = vpu_dllist_get_count_sync(drv_info->delay_queue);
			dlog_vmgr("pending drv_id:%u, delay_count:%d", mgr_ctx->pending_drv_id, delay_count);

			if (delay_count >= 2) {
				cmd = (vpu_cmd_t *)vpu_dllist_remove_head_sync(drv_info->delay_queue);
				if (cmd != NULL) {
					dlog_vmgr("drv_id:%u, get cmd:%s", cmd->drv_id, vmgr_cmd_name(cmd->cmd_type));
				}
			} else {
				cmd = vmgr_get_pending_escapable(mgr_ctx, drv_info);
				//dlog_vmgr("pending drv_id:%d, wait cmd:0x%x", mgr_ctx->pending_drv_id, cmd);
			}

			if (cmd != NULL) {
				mgr_ctx->pending_drv_id = INVALID_DRV_ID;
			}
		} else {
			err_vmgr("invalid, drv_info:0x%x, pending_drv_id:%u", drv_info, mgr_ctx->pending_drv_id);
		}
	} else {
		cmd = vmgr_list_get_cmd(cmdQueue);
#if 0
		if (cmd != NULL) {
			dlog_vmgr("drv_id:%u, enable_avoid_pending:%d, cmd:%s", cmd->drv_id, cmd->drv_info->enable_avoid_pending, vmgr_cmd_name(cmd->cmd_type));
		} else {
			err_vmgr("no item in queue, %d", vmgr_list_get_cmd_count(cmdQueue));
		}
#endif

		if ((cmd != NULL) && (cmd->drv_info->enable_avoid_pending == 1U) && (cmd->cmd_type == VPU_CMD_DEC_DECODE)) {
			vpu_dllist_insert_tail(cmd->drv_info->delay_queue, (vpu_dllist_node_t *)cmd);

			delay_count = vpu_dllist_get_count_sync(cmd->drv_info->delay_queue);
			dlog_vmgr("drv_id:%u, add cmd to delay queue, delay_count:%d", cmd->drv_id, delay_count);

			if (delay_count >= 2) {
				//get delayed cmd from delay queue (replace cmd)
				cmd = (vpu_cmd_t *)vpu_dllist_remove_head_sync(cmd->drv_info->delay_queue);
				if (cmd != NULL) {
					dlog_vmgr("drv_id:%u, get cmd:%s", cmd->drv_id, vmgr_cmd_name(cmd->cmd_type));
				}
			} else {
				if (cmd->drv_info->initial_wakeup_poll == 0U) {
					vmgr_wakeup_user_poll(mgr_ctx, cmd);
					cmd->drv_info->initial_wakeup_poll = 1U;
				}

				cmd = NULL;
			}
		}
	}

	return cmd;
}


#endif //defined(ENABLE_INTERLACE_DELAY_PROCESS)

static int vmgr_operation(vpu_mgr_t *mgr_ctx)
{
	int ret = 0;
	bool cmd_finished;
	vpu_cmd_t *cmd = NULL;
	vmgr_clock_t *clock_ctrl = mgr_ctx->each_ip->clock_ctrl;
	vmgr_comm_t *cmdQueue = &mgr_ctx->comm_data;

	while (
#if defined(ENABLE_INTERLACE_DELAY_PROCESS)
					(mgr_ctx->pending_drv_id != INVALID_DRV_ID) ||
#endif
		(vmgr_list_is_empty(cmdQueue) == false)) {
		cmd_finished = true;

#if defined(ENABLE_INTERLACE_DELAY_PROCESS)
		//0. Processing occurs only when in a paused state (timeout).
		//1. Interlace processing is only possible with one instance.
		//2. Activation of 'enable_avoid_pending' must occur when detecting interlace, and immediately before the next input (second field).

		// When the user verifies the result of the first field and activates the 'enable_avoid_pending' upon entering the second field
		// (in fact, can be activated at any time, once activated, it remains until the end).
		if (mgr_ctx->enable_avoid_pending == 1U) {
			cmd = vmgr_cmd_delay_for_interlace(mgr_ctx);
		} else
#endif
		{
			//get command reference from queue
			cmd = vmgr_list_get_cmd(cmdQueue);
		}

		if (cmd != NULL) {
			//set last command for debugging
			vmgr_set_last_cmd(mgr_ctx, cmd->op_type, cmd->drv_id, cmd->cmd_type);

			detail_vmgr("[%d] :: cmd = 0x%x, op_type:%s, cmd_queued(%d)", cmd->drv_id, cmd->cmd_type, vmgr_get_optype_name(cmd->op_type), cmdQueue->cmd_queued);

			if (cmd->drv_id < VPU_MAX) {
				if (cmd->op_type == VPU_OP_TYPE_DEC) {
					ret = vmgr_decode_proc(mgr_ctx, cmd);
				} else {
					ret = vmgr_encode_proc(mgr_ctx, cmd);
				}

				cmd_finished = true;

				if (cmd->result != VPU_RETCODE_SUCCESS) {
					if (cmd->result == VPU_RETCODE_CODEC_EXIT) {
						err_vmgr("result is VPU_RETCODE_CODEC_EXIT!!");
						if (clock_ctrl != NULL) {
							clock_ctrl->restore_clock(clock_ctrl, 0, atomic_read(&mgr_ctx->dev_opened));
						}

						vmgr_close_all(mgr_ctx);
					}
				}
			} else {
				err_vmgr("missed info or unknown command => type = 0x%x, cmd = 0x%x", cmd->drv_id, cmd->cmd_type);
				cmd->result = VPU_RETCODE_FAILURE;
				cmd_finished = false;
			}

			vmgr_set_last_cmd_finish(mgr_ctx, cmd->op_type, cmd->drv_id, cmd->cmd_type);

			if (cmd_finished) {
				if ((cmd->poll_data != NULL) /*&& (atomic_read(&mgr_ctx->dev_opened) > 0)*/) {
#if defined(ENABLE_INTERLACE_DELAY_PROCESS)
					//add to result queue
					if (cmd->drv_info->avoid_pending == 0U) {
						dlog_vmgr("add to result, drv_id:%d, cmd_type:%s", cmd->drv_id, vmgr_cmd_name(cmd->cmd_type));
						vmgr_list_add_result(cmdQueue, cmd);
						vmgr_poll_wake_up(mgr_ctx, cmd);
					} else {
						if (cmd->drv_info->temp_decoding_result != NULL) {
							err_vmgr("drv_id:%d, already has temp_decoding_result:0x%x", cmd->drv_id, cmd->drv_info->temp_decoding_result);
						}

						//to replace the decoding results after resuming,
						//they need to be temporarily stored
						cmd->drv_info->temp_decoding_result = (void *)cmd;
						cmd->drv_info->avoid_pending = 0U;
						dlog_vmgr("store to result temporary, drv_id:%d, cmd_type:%s", cmd->drv_id, vmgr_cmd_name(cmd->cmd_type));
					}
#else
					dlog_vmgr("add to result, drv_id:%d, cmd_type:%s", cmd->drv_id, vmgr_cmd_name(cmd->cmd_type));
					vmgr_list_add_result(cmdQueue, cmd);
					vmgr_poll_wake_up(mgr_ctx, cmd);
#endif
				} else {
					//add to command pool to re-use
					vmgr_list_add_pool(cmdQueue, cmd);

					err_vmgr("abnormal exception or external command was processed!! 0x%p - %d", cmd->poll_data, atomic_read(&mgr_ctx->dev_opened));
				}
			} else {
				//add to command pool to re-use
				vmgr_list_add_pool(cmdQueue, cmd);

				err_vmgr("abnormal exception 2!! 0x%p - %d", cmd->poll_data, atomic_read(&mgr_ctx->dev_opened));
			}
		}

	}

	return 0;
}

static int vmgr_thread(void *kthread)
{
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)kthread;
	vpu_ip_module_t *each_ip = mgr_ctx->each_ip;
	vmgr_comm_t *cmdQueue = &mgr_ctx->comm_data;

	V_DBG(VPU_DBG_THREAD, "%s, mgr_ctx:%p vmgr thread started", vmgr_get_ip_name(each_ip->ip_type), mgr_ctx);

	do {
		if (
#if defined(ENABLE_INTERLACE_DELAY_PROCESS)
			(mgr_ctx->pending_drv_id == INVALID_DRV_ID) &&
#endif
			(vmgr_list_is_empty(cmdQueue) != false)) {
			(void) wait_event_interruptible_timeout(
				cmdQueue->thread_wq,
				cmdQueue->thread_intr > 0,
				msecs_to_jiffies(50));

			cmdQueue->thread_intr = 0;
		} else {
			if ((atomic_read(&mgr_ctx->dev_opened) > 0) || (mgr_ctx->external_proc == true)) {
				vmgr_operation(mgr_ctx);
			} else {
				//FIXME : is it necessary?
				vpu_cmd_t *cmd = NULL;
				cmd = vmgr_list_get_cmd(cmdQueue);
				vmgr_list_add_pool(cmdQueue, cmd);
				err_vmgr("DEL for empty");
			}
		}
	} while (!kthread_should_stop());

	V_DBG(VPU_DBG_THREAD, "%s, vmgr thread finished", vmgr_get_ip_name(each_ip->ip_type));
	return 0;
}

#if defined(ENABLE_CQ2)

static int vmgr_dispatch_encode(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd)
{
	err_vmgr("Not implemented function yet!");

	return 0;
}

static int vmgr_dispatch_decode(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd)
{
	enum vpu_op_type op_type = cmd->op_type;
	unsigned int drv_id = cmd->drv_id;

	//set last command for debugging
	vmgr_set_last_cmd(mgr_ctx, cmd->op_type, cmd->drv_id, cmd->cmd_type);

	switch (cmd->cmd_type) {
	case VPU_DEC_DECODE:
	{
		cmd->result = vmgr_decode_proc(mgr_ctx, cmd);
		if (cmd->result == RETCODE_SUCCESS) {
			//V_DBG(VPU_DBG_INFO, "ret success, cq remaining:%d", atomic_read(&mgr_ctx->cq_remaining));

			//CAUTION : Interrupt may occur between vmgr_decode_proc() and vmgr_list_add_result()
			vmgr_list_add_resultwait(&mgr_ctx->comm_data, cmd);
			mgr_ctx->procCount[op_type][drv_id]++;
		} else {
			atomic_inc(&cmd->poll_data->count);

#if defined(SHOW_POLL_UP_COUNT_LOG)
			if (atomic_read(&cmd->poll_data->count) != 1) {
				dprintk(
				"poll wakeup count = %d :: type(0x%x) cmd(0x%x)",
				atomic_read(&cmd->poll_data->count),
				cmd->drv_id,
				cmd->cmd_type);
			}
#endif
			//wake up polling in dec/enc by user
			wake_up_interruptible(&cmd->poll_data->wq);
			vmgr_list_add_pool(&mgr_ctx->comm_data, cmd);
		}
	}
	break;

	case VPU_DEC_FLUSH_OUTPUT:
	{
		//V_DBG(VPU_DBG_INFO, "@@@ FLUSH/1, drv_id=%d, procCount=%d", drv_id, mgr_ctx->procCount[drv_id]);

		if (mgr_ctx->procCount[op_type][drv_id] > 0) {
			//V_DBG(VPU_DBG_INFO, "@@@ FLUSH/2, drv_id=%d, procCount=%d", drv_id, mgr_ctx->procCount[drv_id]);
			vmgr_list_add_resultwait(&mgr_ctx->comm_data, cmd);
		} else if (atomic_read(&mgr_ctx->intrCount[op_type][drv_id]) > 0) {
			//V_DBG(VPU_DBG_INFO, "@@@ FLUSH/3, drv_id=%d, procCount=%d", drv_id, mgr_ctx->procCount[drv_id]);
			vmgr_list_add_resultwait(&mgr_ctx->comm_data, cmd);
		} else {
			cmd->result = vmgr_decode_proc(mgr_ctx, cmd);
			//Should put FLUSH state and put the cmd to wait list for interrupt...

			if (cmd->result == RETCODE_SUCCESS) {
				mgr_ctx->procCount[op_type][drv_id]++;

				vmgr_list_add_resultwait(&mgr_ctx->comm_data, cmd);
			} else {
				if ((cmd->poll_data != NULL) && (atomic_read(&mgr_ctx->dev_opened) > 0)) {
					atomic_inc(&cmd->poll_data->count);
#if defined(SHOW_POLL_UP_COUNT_LOG)
					if (atomic_read(&cmd->poll_data->count) != 1) {
						dprintk(
						"poll wakeup count = %d :: type(0x%x) cmd(0x%x)",
						atomic_read(&cmd->poll_data->count),
						cmd->drv_id,
						cmd->cmd_type);
					}
#endif //#if defined(SHOW_POLL_UP_COUNT_LOG)

					wake_up_interruptible(&cmd->poll_data->wq);
				}

				vmgr_list_add_pool(&mgr_ctx->comm_data, cmd);
			}

			//V_DBG(VPU_DBG_INFO, "@@@ FLUSH/5, drv_id=%d, result=%d\n", drv_id, cmd->result);
		}
	}
	break;

	default: {
		cmd->result = vmgr_decode_proc(mgr_ctx, cmd);

		//V_DBG(VPU_DBG_INFO, "@@@ result:%d - cmd:%s(0x%x)\n", cmd->result, vmgr_cmd_name(cmd->cmd_type), cmd->cmd_type);

		if (cmd->result == RETCODE_SUCCESS) {
			vmgr_list_add_result(&mgr_ctx->comm_data, cmd);
		} else {
			vmgr_list_add_pool(&mgr_ctx->comm_data, cmd);
			//V_DBG(VPU_DBG_INFO, "@@@ dispatcher / end2 - drv_id = %d, cmd=%d, result_q=%d\n", cmd->drv_id, cmd->cmd_type, vpu_dllist_get_count_sync(mgr_ctx->comm_data.result_q[VPU_OP_TYPE_DEC][cmd->drv_id]));
		}

		if ((cmd->poll_data != NULL) && (atomic_read(&mgr_ctx->dev_opened) > 0)) {
			atomic_inc(&cmd->poll_data->count);

#if defined(SHOW_POLL_UP_COUNT_LOG)
			if (atomic_read(&cmd->poll_data->count) != 1) {
				dprintk(
				"poll wakeup count = %d :: type(0x%x) cmd(0x%x)",
				atomic_read(&cmd->poll_data->count),
				cmd->drv_id,
				cmd->cmd_type);
			}
#endif //#if defined(SHOW_POLL_UP_COUNT_LOG)

			wake_up_interruptible(&cmd->poll_data->wq);
		}
	}
	break;
	//V_DBG(VPU_DBG_INFO, "@@@ decode/2, drv_id = %d, result_q=%d", drv_id, vpu_dllist_get_count_sync(mgr_ctx->comm_data.result_q[VPU_OP_TYPE_DEC][drv_id]));
	}

	return 0;
}

static int vmgr_dispatch_proc(vpu_mgr_t *mgr_ctx)
{
	int cq_remaining;
	int cmd_count;
	vpu_cmd_t *cmd = NULL;

	cq_remaining = atomic_read(&mgr_ctx->cq_remaining);
	cmd_count = vpu_dllist_get_count_sync(mgr_ctx->comm_data.cmd_q);

	while (cmd_count > 0) {
		cmd = vmgr_list_get_cmd(&mgr_ctx->comm_data);
		if (cmd != NULL) {
			if (cmd->op_type == VPU_OP_TYPE_DEC) {
				vmgr_dispatch_decode(mgr_ctx, cmd);
			} else {
				vmgr_dispatch_encode(mgr_ctx, cmd);
			}

			cmd_count--;
		}
	}

	return 0;
}

static int vmgr_dispatch_thread(void *kthread)
{
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)kthread;

	V_DBG(VPU_DBG_THREAD, "%s, mgr_ctx:%p vmgr dispatch thread started", vmgr_get_ip_name(mgr_ctx->each_ip->ip_type), mgr_ctx);

	do {
		if (vmgr_list_is_empty(&mgr_ctx->comm_data) != false) {
			(void) wait_event_interruptible_timeout(
				mgr_ctx->comm_data.thread_wq, //from vmgr_list_add : command input
				mgr_ctx->comm_data.thread_intr > 0,
				msecs_to_jiffies(50));

			mgr_ctx->comm_data.thread_intr = 0;
		} else {
			if ((atomic_read(&mgr_ctx->dev_opened) > 0) || (mgr_ctx->external_proc == true)) {
				if ((atomic_read(&mgr_ctx->dev_opened) == 0) && (mgr_ctx->external_proc == true)) {
					dlog_vmgr("do external_proc, even though dev_opened is 0");
				}

				vmgr_dispatch_proc(mgr_ctx);
			} else {
				//FIXME : is it necessary?
				vpu_cmd_t *cmd = NULL;
				cmd = vmgr_list_get_cmd(&mgr_ctx->comm_data);
				vmgr_list_add_pool(&mgr_ctx->comm_data, cmd);
				err_vmgr("DEL for empty");
			}
		}
	} while (!kthread_should_stop());

	V_DBG(VPU_DBG_THREAD, "%s, vmgr dispatch thread finished", vmgr_get_ip_name(mgr_ctx->each_ip->ip_type));

	return 0;
}

static int vmgr_collect_proc(vpu_mgr_t *mgr_ctx)
{
	vpu_ip_module_t *each_ip = mgr_ctx->each_ip;

	unsigned int intr_status = 0;
	unsigned int intr_reason;
	vpu_cmd_t *cmd = NULL;

	enum vpu_op_type op_type = VPU_OP_TYPE_DEC;

	intr_status = 0;

	if (atomic_read(&mgr_ctx->dev_opened) > 0) {
		intr_status = each_ip->cq2_func->vpu_get_interrupt_status(mgr_ctx);
		//DBG("intr_status:0x%x", intr_status);
	}

	if (intr_status) {
		unsigned int intr_inst_index = INVALID_VPU_INDEX;

		if (each_ip->cq2_func != NULL) {
			intr_inst_index = each_ip->cq2_func->vpu_get_id_with_reason(mgr_ctx, &intr_reason);
		}

		if (((intr_reason & 0x100) == 0x100) && (intr_inst_index != INVALID_VPU_INDEX)) {
			atomic_inc(&mgr_ctx->intrCount[op_type][intr_inst_index]);
			//V_DBG(VPU_DBG_INFO, "ins_idx=%d, intrCount 0:%d, 1:%d", intr_inst_index, mgr_ctx->intrCount[op_type][0], mgr_ctx->intrCount[op_type][1]);
		} else {
			//DBG("@@@ OTHER intr_reason, %X", intr_reason);
		}
		//DBG("@@@ 2/ intr_status=%d, intr_reason=0x%X, intr_inst_index=%d, result_q=%d, intrCount=%d", intr_status, intr_reason, intr_inst_index, vpu_dllist_get_count_sync(mgr_ctx->comm_data.result_q[VPU_OP_TYPE_DEC][intr_inst_index]), mgr_ctx->intrCount[intr_inst_index]);
	}

	if (mgr_ctx->drv_list != NULL) {
		vpu_drv_info_t *drvInfo = NULL;
		drvInfo = (vpu_drv_info_t *)vpu_dllist_get_head_sync(mgr_ctx->drv_list);

		while (drvInfo != NULL) {
			enum vpu_op_type op_type = drvInfo->op_type;
			int drv_id = drvInfo->drv_id;

			//V_DBG(VPU_DBG_INFO, ">>> drv_id:%d, next:%p", drvInfo->drv_id, drvInfo->list.next);

			if (drvInfo->opened == false) {
				atomic_set(&mgr_ctx->intrCount[op_type][drv_id], 0);
			}

			if (atomic_read(&mgr_ctx->intrCount[op_type][drv_id]) > 0) {
				//get from wait_q to result_q
				cmd = vmgr_list_get_wait(&mgr_ctx->comm_data, op_type, drv_id);
				if (cmd != NULL) {
					int cmd_preserved;
					long elapsed_us;
					atomic_dec(&mgr_ctx->intrCount[op_type][drv_id]);
					mgr_ctx->procCount[op_type][drv_id]--;

					cmd_preserved = cmd->cmd_type;
					cmd->cmd_type = VPU_DEC_GET_OUTPUT_INFO;

					//intentional_fbfull = 0; //@@@

					cmd->result = vmgr_decode_proc(mgr_ctx, cmd);
					cmd->cmd_type = cmd_preserved;

					elapsed_us = vmgr_set_last_cmd_finish(mgr_ctx, cmd->op_type, cmd->drv_id, cmd->cmd_type);

					if ((cmd->poll_data != NULL) && (atomic_read(&mgr_ctx->dev_opened) > 0)) {
						atomic_inc(&cmd->poll_data->count);
#if defined(SHOW_POLL_UP_COUNT_LOG)
						if (atomic_read(&cmd->poll_data->count) != 1) {
							dprintk("poll wakeup count = %d :: type(0x%x) cmd(0x%x)",
								atomic_read(&cmd->poll_data->count),
								cmd->drv_id,
								cmd->cmd_type);
						}
#endif //#if defined(SHOW_POLL_UP_COUNT_LOG)
					} else {
						dlog_vmgr("drv_id:%d, comm_data:%p, dev_opened:%d", cmd->drv_id, cmd->poll_data, atomic_read(&mgr_ctx->dev_opened));
					}

					//decoder, encoder driver get from this result queue, and then push to command_pool
					vmgr_list_add_result(&mgr_ctx->comm_data, cmd);

					wake_up_interruptible(&cmd->poll_data->wq);
				} else {
					//DBG("@@@ on result data to get output, drv_id=%d", drv_id);
				}
			} //end of if (mgr_ctx->intrCount[op_type][drv_id] > 0)

			drvInfo = (vpu_drv_info_t *)vpu_dllist_get_next_sync(mgr_ctx->drv_list, (vpu_dllist_node_t *)drvInfo);
		} //end of while (drvInfo != NULL)
	} // end of if (mgr_ctx->drv_list != NULL)

	return 0;
}

static int vmgr_collect_thread(void *kthread)
{
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)kthread;

	V_DBG(VPU_DBG_THREAD, "%s, mgr_ctx:%p vmgr collect thread started", vmgr_get_ip_name(mgr_ctx->each_ip->ip_type), mgr_ctx);

	do {
		(void)wait_event_interruptible_timeout(
										mgr_ctx->oper_wq, //from ip's isr_handler
										atomic_read(&mgr_ctx->oper_intr) > 0,
										msecs_to_jiffies(50));

		if ((atomic_read(&mgr_ctx->oper_intr) > 0) && (atomic_read(&mgr_ctx->dev_opened) > 0)) {
			vmgr_collect_proc(mgr_ctx);
		}
	} while (!kthread_should_stop());

	V_DBG(VPU_DBG_THREAD, "%s, vmgr collect thread finished", vmgr_get_ip_name(mgr_ctx->each_ip->ip_type));

	return 0;
}

#endif

void vmgr_decode_pre_flush(vpu_mgr_t *mgr_ctx, vpu_drv_info_t *drv_info)
{
#if defined(ENABLE_INTERLACE_DELAY_PROCESS)
	//when receiving a flush, results are flushed preemptively as data in the result queue may be processed before flush completion.
	vmgr_list_flush_result(&mgr_ctx->comm_data, drv_info->drv_id, drv_info->op_type);
#endif
}
EXPORT_SYMBOL(vmgr_decode_pre_flush);

//called by the dec/enc driver at init
int vmgr_register(vpu_mgr_t *mgr_ctx, vpu_drv_info_t *drv_info)
{
	int ret = 0;

	if ((mgr_ctx != NULL) && (drv_info != NULL)) {
		vmgr_clock_t *clock_ctrl = mgr_ctx->each_ip->clock_ctrl;

		dlog_info("vmgr_register");

		vetc_mutex_lock(&mgr_ctx->vmgr_mutex);

		//create result queue
		if ((drv_info->op_type < VPU_OP_TYPE_MAX) && (drv_info->drv_id < VPU_DRV_ID_MAX) && (mgr_ctx->comm_data.result_q[drv_info->op_type][drv_info->drv_id] == NULL)) {
			V_DBG(VPU_DBG_INFO, "create result queue!! op_type:%s(%d), drv_id:%d", vmgr_get_optype_name(drv_info->op_type), drv_info->op_type, drv_info->drv_id);
			vmgr_list_create_result_q(&mgr_ctx->comm_data, drv_info->op_type, drv_info->drv_id);
		}

		//create wait queue
		if ((drv_info->op_type < VPU_OP_TYPE_MAX) && (drv_info->drv_id < VPU_DRV_ID_MAX) && (mgr_ctx->comm_data.wait_q[drv_info->op_type][drv_info->drv_id] == NULL)) {
			V_DBG(VPU_DBG_INFO, "create wait queue!! op_type:%s(%d), drv_id:%d", vmgr_get_optype_name(drv_info->op_type), drv_info->op_type, drv_info->drv_id);
			vmgr_list_create_wait_q(&mgr_ctx->comm_data, drv_info->op_type, drv_info->drv_id);
		}

		if (clock_ctrl != NULL) {
			clock_ctrl->enable_clock(clock_ctrl, 0);
		}

		dlog_info("dev_opened:%d", atomic_read(&mgr_ctx->dev_opened));
		if (atomic_read(&mgr_ctx->dev_opened) == 0) {
			if (clock_ctrl != NULL) {
				detail_info("invoke hw_reset:%s", vmgr_get_ip_name(mgr_ctx->each_ip->ip_type));
				clock_ctrl->hw_reset(clock_ctrl);
			}

			ret = vmem_init();
			if (ret < 0) {
				err_info("failed to allocate memory for VPU!! %d", ret);
			}

			//do something for init
			detail_info("initialized reset, irq, ret, vmem");
		}

		vmgr_set_drv_info(mgr_ctx, drv_info->op_type, drv_info->drv_id, drv_info);

#if defined(ENABLE_INTERLACE_DELAY_PROCESS)
		if (drv_info->op_type == VPU_OP_TYPE_DEC) {
			char qname[64];
			vetc_memset(qname, 0x00, sizeof(qname), 0);
			snprintf(qname, sizeof(qname), "delay_queue_%d", drv_info->drv_id);

			drv_info->detected_interlace = 0U;
			drv_info->enable_avoid_pending = 0U;
			drv_info->avoid_pending = 0U;
			drv_info->initial_wakeup_poll = 0U;
			drv_info->delay_queue = vpu_dllist_create(qname);
			if (drv_info->delay_queue != NULL) {
				dlog_vmgr("dec_id:%u, create delay queue:%s", drv_info->drv_id, vpu_dllist_name(drv_info->delay_queue));
			} else {
				err_vmgr("dec_id:%u, create delay queue failed", drv_info->drv_id);
			}

			drv_info->temp_decoding_result = NULL;
		}
#endif

#if defined(ENABLE_CQ2)
		vpu_dllist_insert_tail_sync(mgr_ctx->drv_list, (vpu_dllist_node_t *)drv_info);
		detail_info(">>> add to drv_list, type:%s, id:%d, list count:%d", vmgr_get_optype_name(drv_info->op_type), drv_info->drv_id, vpu_dllist_get_count_sync(mgr_ctx->drv_list));
#endif

		atomic_inc(&mgr_ctx->dev_opened);

		vetc_mutex_unlock(&mgr_ctx->vmgr_mutex);
	} else {
		err_info("null parameter, mgr_ctX:%p, drv_info:%p", mgr_ctx, drv_info);
		ret = -1;
	}

	return ret;
}

EXPORT_SYMBOL(vmgr_register);

int vmgr_deregister(vpu_mgr_t *mgr_ctx, vpu_drv_info_t *drv_info)
{
	int ret = 0;

	if ((mgr_ctx != NULL) && (drv_info != NULL)) {
		vmgr_clock_t *clock_ctrl = mgr_ctx->each_ip->clock_ctrl;

		vetc_mutex_lock(&mgr_ctx->vmgr_mutex);

		if (atomic_read(&mgr_ctx->dev_opened) > 0) {
			atomic_dec(&mgr_ctx->dev_opened);
		}

		//when all devices are closed
		dlog_info("number of open devices:%d", atomic_read(&mgr_ctx->dev_opened));
		if (atomic_read(&mgr_ctx->dev_opened) == 0) {
			detail_info("starting the shutdown of all devices");
			// To close whole vpu instance when being killed process opened this.
			mgr_ctx->external_proc = true;
			(void)vmgr_external_all_close(mgr_ctx, 200);
			mgr_ctx->external_proc = false;

			atomic_set(&mgr_ctx->oper_intr, 0);

			vmgr_close_all(mgr_ctx);
			vmem_deinit();

			(void)udelay(1000);	//1ms
		}

		if (clock_ctrl != NULL) {
			clock_ctrl->disable_clock(clock_ctrl, 0);
		}

#if defined(ENABLE_INTERLACE_DELAY_PROCESS)
		if (drv_info->op_type == VPU_OP_TYPE_DEC) {
			drv_info->enable_avoid_pending = 0U;
			mgr_ctx->enable_avoid_pending = 0U;

			if (drv_info->delay_queue != NULL) {
				dlog_vmgr("dec_id:%u, delete delay queue:%s", drv_info->drv_id, vpu_dllist_name(drv_info->delay_queue));
				vpu_dllist_destroy(drv_info->delay_queue);
			}

			if (drv_info->temp_decoding_result != NULL) {
				vpu_cmd_t *temp_cmd = (vpu_cmd_t *)drv_info->temp_decoding_result;
				VPU_free(temp_cmd->args);
				VPU_free(temp_cmd);
				drv_info->temp_decoding_result = NULL;
				dlog_info("delte temp_decoding_result");
			}

			dlog_info("set disable avoid pending, dev_opened:%d", atomic_read(&mgr_ctx->dev_opened));
		}
#endif

		//destroy result queue
		if ((drv_info->op_type < VPU_OP_TYPE_MAX) && (drv_info->drv_id < VPU_DRV_ID_MAX)) {
			vmgr_list_reset_with_id(&mgr_ctx->comm_data, drv_info->op_type, drv_info->drv_id);
		}

		(void)vmgr_set_drv_info(mgr_ctx, drv_info->op_type, drv_info->drv_id, NULL);
#if defined(ENABLE_CQ2)
		(void)vpu_dllist_remove_sync(mgr_ctx->drv_list, (vpu_dllist_node_t *)drv_info);
		detail_info(">>> remove from drv_list, type:%s, id:%d, list count:%d", vmgr_get_optype_name(drv_info->op_type), drv_info->drv_id, vpu_dllist_get_count_sync(mgr_ctx->drv_list));
#endif
		vetc_mutex_unlock(&mgr_ctx->vmgr_mutex);
	} else {
		if (mgr_ctx == NULL) {
			err_info("mgr_ctx is NULL");
		}

		if (drv_info == NULL) {
			err_info("drv_info is NULL");
		}

		ret = -1;
	}

	return ret;
}

EXPORT_SYMBOL(vmgr_deregister);

#define VMGR_THREAD_NAME_LEN	64
static int vmgr_init(vpu_mgr_t *mgr_ctx, const char *mgr_name)
{
	int ret = 0;
	char thread_name[VMGR_THREAD_NAME_LEN];

	dlog_vmgr("vmgr initialize for %s", vmgr_get_ip_name(mgr_ctx->each_ip->ip_type));

	init_waitqueue_head(&mgr_ctx->oper_wq);
	vmgr_list_init(&mgr_ctx->comm_data);

	ret = vmem_config();
	if (ret < 0) {
		err_vmgr("unable to configure memory for VPU!! %d", ret);
		ret = (int)-ENOMEM;
	} else {
		unsigned long init_flag = vmgr_get_int_flags();

		detail_vmgr("request irq");
		ret = vmgr_request_irq(mgr_ctx->irq, mgr_ctx->each_ip->isr_handler, init_flag, mgr_name, mgr_ctx);
		if (ret) {
			err_vmgr("to aquire vpu-dec-irq");
			ret = -1;
		}

#if defined(ENABLE_VPU_FW_LOADING)
		ret = vetc_prepare_firmware(mgr_ctx->pdev, mgr_ctx->each_ip->ip_type, &mgr_ctx->fw_addr);

		if (ret != 0) {
			V_DBG(VPU_DBG_ERROR,"Prepare Firmware Functions retured error!!");
		}
#endif

		if (ret == 0) {
			mgr_ctx->irq_reged = 1;

			memset(thread_name, 0x00, VMGR_THREAD_NAME_LEN);

			if (mgr_ctx->each_ip->cq_type == VPU_CQ_LEGACY) {
				sprintf(thread_name, "%s_thread", mgr_name);

				detail_vmgr("create %s thread ", thread_name);
				mgr_ctx->kidle_task = kthread_run(vmgr_thread, mgr_ctx, thread_name);
				if (IS_ERR(mgr_ctx->kidle_task)) {
					err_vmgr("unable to create kidle_task thread!!");
					mgr_ctx->kidle_task = NULL;
					ret = -1;
				}
			}
#if defined(ENABLE_CQ2)
			else {
				sprintf(thread_name, "%s_dispatch_thread", mgr_name);

				//V_DBG(VPU_DBG_INFO, "create %s thread ", thread_name);
				mgr_ctx->kidle_task = kthread_run(vmgr_dispatch_thread, mgr_ctx, thread_name);
				if (IS_ERR(mgr_ctx->kidle_task)) {
					err_vmgr("unable to create kidle_task thread!!");
					mgr_ctx->kidle_task = NULL;
					ret = -1;
				} else {
					memset(thread_name, 0x00, VMGR_THREAD_NAME_LEN);
					sprintf(thread_name, "%s_collect_thread", mgr_name);

					//V_DBG(VPU_DBG_INFO, "create %s thread for cq2", thread_name);
					mgr_ctx->collect_task = kthread_run(vmgr_collect_thread, mgr_ctx, thread_name);
					if (IS_ERR(mgr_ctx->collect_task)) {
						err_vmgr("unable to create collect_task thread!!");
						mgr_ctx->collect_task = NULL;
						ret = -1;
					}
				}
			}
#endif
		} //if (ret == 0)
	} //ret = vmem_config();

	return ret;
}

static int vmgr_deinit(vpu_mgr_t *mgr_ctx)
{
	int ret = 0;
	vmgr_clock_t *clock_ctrl = mgr_ctx->each_ip->clock_ctrl;

	if (mgr_ctx->kidle_task) {
		//V_DBG(VPU_DBG_INFO, "stop vmgr thread");
		kthread_stop(mgr_ctx->kidle_task);
		mgr_ctx->kidle_task = NULL;
	}

#if defined(ENABLE_CQ2)
	if (mgr_ctx->collect_task) {
		//V_DBG(VPU_DBG_INFO, "stop cq2 collect thread");
		kthread_stop(mgr_ctx->collect_task);
		mgr_ctx->collect_task = NULL;
	}
#endif

	if (mgr_ctx->irq_reged) {
		vmgr_free_irq(mgr_ctx->irq, mgr_ctx);
		mgr_ctx->irq_reged = 0;
	}

	if (clock_ctrl != NULL) {
		clock_ctrl->hw_assert(clock_ctrl);
	}

	return ret;
}

vpu_mgr_t *vmgr_alloc(vpu_ip_module_t *ip_param)
{
	vpu_mgr_t *mgr_ctx = NULL;

	mgr_ctx = VPU_alloc(sizeof(vpu_mgr_t));
	if (mgr_ctx != NULL) {
		atomic_set(&mgr_ctx->dev_opened, 0);
		mgr_ctx->each_ip = ip_param;

		dlog_vmgr("register ip type:%d(%s), cq_type:%d, cq_depth:%d, buffer mode:0x%x, internal timeout:%d",
				ip_param->ip_type,
				vmgr_get_ip_name(ip_param->ip_type),
				ip_param->cq_type,
				ip_param->cq_depth,
				ip_param->buffer_mode,
				ip_param->internal_timeout_ms);

		dlog_vmgr("param size, enc:%d, dec:%d, assess point path:%s",
			ip_param->enc_param_size, ip_param->dec_param_size, ip_param->access_point_path);

		dlog_vmgr("fp dec:%p, fp enc:%p, fp getbuffer:%p, clock_ctrl:%p, cq_func:%p, ip_private:%p",
				ip_param->proc_decode,
				ip_param->proc_encode,
				ip_param->proc_get_buffer_size,
				ip_param->clock_ctrl,
				ip_param->cq_func,
				ip_param->ip_private);

#if defined(SHOW_CAPABILITY_LOG)
		{
			int ii;
			V_DBG(VPU_DBG_INFO, "support decoder codec list:");
			for (ii = 0; ii < MAX_SUPPORT_CODEC; ii++) {
				vpu_ip_cap_t *pcap = &mgr_ctx->each_ip->dec_capa[ii];

				if (pcap->support_codec == NULL) {
					break;
				}

				dlog_vmgr("\t%s %s %s %ux%u@%u", pcap->support_codec,
						(pcap->support_profile != NULL) ? pcap->support_profile : " ",
						(pcap->support_level != NULL) ? pcap->support_level : " ",
						pcap->max_width, pcap->max_height, pcap->max_fps);
			}

			V_DBG(VPU_DBG_INFO, "support encoder codec list:");
			for (ii = 0; ii < MAX_SUPPORT_CODEC; ii++) {
				vpu_ip_cap_t *pcap = &mgr_ctx->each_ip->enc_capa[ii];

				if (pcap->support_codec == NULL) {
					break;
				}

				dlog_vmgr("\t%s %s %s %ux%u@%u", pcap->support_codec,
						(pcap->support_profile != NULL) ? pcap->support_profile : " ",
						(pcap->support_level != NULL) ? pcap->support_level : " ",
						pcap->max_width, pcap->max_height, pcap->max_fps);
			}
		}
#endif //SHOW_CAPABILITY_LOG

		(void)vmgr_set_context(mgr_ctx->each_ip->ip_type, mgr_ctx);

#if defined(ENABLE_CQ2)
		mgr_ctx->drv_list = vpu_dllist_create("drv_list");
#endif

		mutex_init(&mgr_ctx->vmgr_mutex);

#if defined(ENABLE_INTERLACE_DELAY_PROCESS)
		mgr_ctx->enable_avoid_pending = 0U;
		mgr_ctx->pending_drv_id = INVALID_DRV_ID;
#endif

#if defined(ENABLE_CQ2)
		atomic_set(&mgr_ctx->cq_remaining, mgr_ctx->each_ip->cq_depth);
#endif
	} else {
		err_vmgr("vpu_mgr context alloc failed");
	}

	return mgr_ctx;
}

EXPORT_SYMBOL(vmgr_alloc);

void vmgr_free(vpu_mgr_t *mgr_ctx)
{
	//V_DBG(VPU_DBG_INFO, "vmgr free");
	if (mgr_ctx != NULL) {
		dlog_vmgr("%p, vpu ip:%s", mgr_ctx, vmgr_get_ip_name(mgr_ctx->each_ip->ip_type));

#if defined(ENABLE_CQ2)
		vpu_dllist_destroy(mgr_ctx->drv_list);
#endif

		(void)vmgr_set_context(mgr_ctx->each_ip->ip_type, NULL);
		VPU_free(mgr_ctx);
	}
}

EXPORT_SYMBOL(vmgr_free);


int vmgr_probe(vpu_mgr_t *mgr_ctx, struct platform_device *pdev, const char *mgr_name)
{
	int ret = 0;
	vmgr_clock_t *clock_ctrl = NULL;
	struct resource *resource = NULL;

	if (pdev->dev.of_node == NULL) {
		ret = (int)-ENODEV;
	} else {
#if defined(USE_ACCESS_POINT)
		if (mgr_ctx->access_point == NULL) {
			detail_vmgr("getting for library access point");

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
			mgr_ctx->access_point = vmgr_accesspoint_get_addr(mgr_ctx->each_ip->access_point_path);
#else
			mgr_ctx->access_point = vmgr_accesspoint_get_addr_mem(mgr_ctx->each_ip->ip_type);
#endif

			if (mgr_ctx->access_point != NULL) {
				detail_vmgr("access point info:%p, dec:%p, enc:%p", mgr_ctx->access_point, mgr_ctx->access_point->tccfp_vpu_dec, mgr_ctx->access_point->tccfp_vpu_enc);
			} else {
				err_vmgr("failed to get library access point!!");
				ret = -1;
			}
		} else {
			err_vmgr("access point already allocated 0x%p", mgr_ctx->access_point);
			ret = -1;
		}
#endif

		if (ret == 0) {
			dlog_vmgr("%s probe", vmgr_get_ip_name(mgr_ctx->each_ip->ip_type));
			clock_ctrl = mgr_ctx->each_ip->clock_ctrl;

			atomic_set(&mgr_ctx->oper_intr, 0);

			//set platform device structure
			mgr_ctx->pdev = pdev;

			mgr_ctx->irq = platform_get_irq(pdev, 0);
			resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
			if (!resource) {
				dev_err(&pdev->dev, "missing phy memory resource");
				ret = -1;
			} else {
				resource->end += 1;

				mgr_ctx->base_addr = devm_ioremap(&pdev->dev, resource->start, (resource->end - resource->start));
				detail_vmgr("VPU base address [0x%x -> 0x%p], irq num [%d]", resource->start, mgr_ctx->base_addr, (mgr_ctx->irq - 32));

				if (clock_ctrl != NULL) {
					detail_vmgr("get clock, reset");
					clock_ctrl->get_clock(clock_ctrl, pdev->dev.of_node);
					clock_ctrl->get_reset(clock_ctrl, pdev->dev.of_node);
				}

				ret = vmgr_init(mgr_ctx, mgr_name);

				dlog_vmgr("%s probe success!!", vmgr_get_ip_name(mgr_ctx->each_ip->ip_type));
			} //if (!resource)
		} //if (ret == 0) access point
	} //pdev->dev.of_node == NULL

	return ret;
}

EXPORT_SYMBOL(vmgr_probe);


int vmgr_remove(vpu_mgr_t *mgr_ctx, struct platform_device *pdev)
{
	vmgr_clock_t *clock_ctrl = mgr_ctx->each_ip->clock_ctrl;

	(void)vmgr_deinit(mgr_ctx);

	devm_iounmap(&pdev->dev, (void __iomem *)mgr_ctx->base_addr);

	if (clock_ctrl != NULL) {
		clock_ctrl->put_clock(clock_ctrl);
		clock_ctrl->put_reset(clock_ctrl);
	}

	vmem_deinit();

	vmgr_list_deinit(&mgr_ctx->comm_data);

	if (mgr_ctx->access_point != NULL) {
		VPU_free(mgr_ctx->access_point);
	}

	dlog_vmgr("%s remove success!!\n", vmgr_get_ip_name(mgr_ctx->each_ip->ip_type));

	return 0;
}

EXPORT_SYMBOL(vmgr_remove);


#if defined(CONFIG_PM)
int vmgr_suspend(vpu_mgr_t *mgr_ctx, struct platform_device *pdev, pm_message_t state)
{
	int i;
	int open_count = 0;
	vmgr_clock_t *clock_ctrl = mgr_ctx->each_ip->clock_ctrl;

	if (atomic_read(&mgr_ctx->dev_opened) > 0) {
		vmgr_external_all_close(mgr_ctx, 200);

		open_count = atomic_read(&mgr_ctx->dev_opened);

		if (clock_ctrl != NULL) {
			for (i = 0; i < open_count; i++) {
				clock_ctrl->disable_clock(clock_ctrl, 0);
			}
		}
	}

	return 0;
}

EXPORT_SYMBOL(vmgr_suspend);

int vmgr_resume(vpu_mgr_t *mgr_ctx, struct platform_device *pdev)
{
	int i;
	int open_count = 0;
	vmgr_clock_t *clock_ctrl = mgr_ctx->each_ip->clock_ctrl;

	if (atomic_read(&mgr_ctx->dev_opened) > 0) {
		open_count = atomic_read(&mgr_ctx->dev_opened);

		if (clock_ctrl != NULL) {
			for (i = 0; i < open_count; i++) {
				clock_ctrl->enable_clock(clock_ctrl, 0);
			}
		}
	}

	return 0;
}

EXPORT_SYMBOL(vmgr_resume);

#endif

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
MODULE_SOFTDEP("pre: vpu_lib jpu_lib hevc_lib vpu_4k_d2_lib vpu_hevc_enc_lib");

MODULE_VERSION(VPU_V3_DRIVER_VERSION);
MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC VPU Manager");
MODULE_LICENSE("Dual BSD/GPL"); 




