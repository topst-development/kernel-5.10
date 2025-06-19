// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_comm.h"

#ifdef CONFIG_SUPPORT_TCC_WAVE420L_2ND_VPU_HEVC_ENC

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/compat.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 1, 0)
#include <soc/telechips/pmap.h>
#endif

#include "vpu_buffer.h"
#include "vpu_devices.h"
#include "vpu_hevc_enc2_mgr_sys.h"
#include "vpu_hevc_enc2_mgr.h"

static unsigned int cntInt_vpu_he;	// = 0;
static void __iomem *vidsys_conf_reg;

static struct VpuList hevc_enc_mgr_vlist;
static char str_file_hevc_enc[] = "file";

#if DEFINED_CONFIG_VENC_CNT_1to16
static unsigned char str_cmd_time_out[] = "hevc enc vmgr_internal_handler timed_out";
static unsigned char str_cmd_init[] = "hevc enc vmgr_internal_handler timed_out";
#endif
// Control only once!!
static struct mgr_data_t vmgr_hevc_enc2_data;
static struct task_struct *kidle_task_hevce;	// = NULL;

#if defined(USE_ACCESS_POINT)
// SHARE_POINT_ORDER_XXX :
//    VPU = 0, JPU = 1, HEVC = 2,
//    4KD2 = 3, HEVC_ENC = 4, HEVC_ENC_2 = 5
#   define SHARE_POINT_ORDER_HEVC_ENC2 5U

typedef int (*tccfp_vpu_hevc_enc_t)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
static tccfp_vpu_hevc_enc_t tcc_vpu_hevc_enc2;
typedef int (*tccfp_vpu_hevc_enc_esc_t)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
static tccfp_vpu_hevc_enc_esc_t tcc_vpu_hevc_enc2_esc;
typedef int (*tccfp_vpu_hevc_enc_ext_t)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
static tccfp_vpu_hevc_enc_ext_t tcc_vpu_hevc_enc2_ext;

typedef struct st_hevc_enc_func_t {
	unsigned int check_code1;
	int (*tccfp_vpu_hevc_dec)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	unsigned int check_code2;
	int (*tccfp_vpu_hevc_enc)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	unsigned int check_code3;
	int (*tccfp_vpu_hevc_enc_esc)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	int (*tccfp_vpu_hevc_enc_ext)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	unsigned int check_code4;
} st_hevc_enc_func;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,10,0)
static st_hevc_enc_func stHENC2FuncBase = {0, NULL, 0, NULL, 0, NULL, NULL, 0};
#endif

static st_hevc_enc_func *stHENC2Func = NULL;

static int check_vpu_hevc_enc_access_addr_valid(void)
{
	int ret = -1;

	if(((CHECK_CODE_01 | stHENC2Func->check_code1) == CHECK_CODE_01) &&
			((CHECK_CODE_02 | stHENC2Func->check_code2) == CHECK_CODE_02) &&
			((CHECK_CODE_03 | stHENC2Func->check_code3) == CHECK_CODE_03) &&
			((CHECK_CODE_04 | stHENC2Func->check_code4) == CHECK_CODE_04)) {
		ret = 0;
	} else {
		V_DBG(VPU_DBG_ERROR, "HEVC Enc CheckCode %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
				GET_FOURCC_1(stHENC2Func->check_code1),
				GET_FOURCC_2(stHENC2Func->check_code1),
				GET_FOURCC_3(stHENC2Func->check_code1),
				GET_FOURCC_4(stHENC2Func->check_code1),
				GET_FOURCC_1(stHENC2Func->check_code2),
				GET_FOURCC_2(stHENC2Func->check_code2),
				GET_FOURCC_3(stHENC2Func->check_code2),
				GET_FOURCC_4(stHENC2Func->check_code2),
				GET_FOURCC_1(stHENC2Func->check_code3),
				GET_FOURCC_2(stHENC2Func->check_code3),
				GET_FOURCC_3(stHENC2Func->check_code3),
				GET_FOURCC_4(stHENC2Func->check_code3),
				GET_FOURCC_1(stHENC2Func->check_code4),
				GET_FOURCC_2(stHENC2Func->check_code4),
				GET_FOURCC_3(stHENC2Func->check_code4),
				GET_FOURCC_4(stHENC2Func->check_code4)
			  );
	}
	return ret;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
static int get_vpu_hevc_enc2_access_addr_file(void)
{
	int ret = 0;
	struct file *filp = NULL;
	mm_segment_t oldfs;
	void *tTmpPtr = NULL;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,0,0)
	oldfs = get_fs();
	set_fs( get_ds() );
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
	oldfs = get_fs();
	set_fs( KERNEL_DS );
#else
	oldfs = force_uaccess_begin();
#endif

	filp = filp_open("/proc/hevc_enc_2", O_RDONLY, 0x1A4); //0644
	VPU_CAST_PT(tTmpPtr, filp);
	if(IS_ERR(tTmpPtr)) {
		V_DBG(VPU_DBG_ERROR, "/proc/hevc_enc file open fail!!");
		ret = -1;
	} else {
		char data[20];
		unsigned long long res = 0;
		u32 idx = 0;

		idx = (u32)((u32)sizeof(void*)*2U) + 2U;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
		ret = vfs_read(filp, data, sizeof(data), &filp->f_pos);
#else
		ret = filp->f_op->read(filp, data, sizeof(data), &filp->f_pos);
#endif

		data[idx] = '\0';

		ret = kstrtoull(data, 16, &res);

		(void)memmove((void*)&stHENC2Func, (void*)&res, sizeof(unsigned long));

		(void)filp_close(filp, NULL);
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
	set_fs(oldfs);
#else
	force_uaccess_end(oldfs);
#endif
	return ret;
}

#else

static int get_vpu_hevc_enc2_access_addr_mem(void)
{
	int ret = 0;
	void *va = NULL;

	va = ioremap(SHARE_POINT_ADDR + (SHARD_POINT_GAP * SHARE_POINT_ORDER_HEVC_ENC2), SHARD_POINT_GAP);

	if (va == NULL) {
		V_DBG(VPU_DBG_ERROR, "ioremap failed");
		ret = -ENOMEM;
	} else {
		memcpy(&stHENC2FuncBase, va, sizeof(st_hevc_enc_func));
		stHENC2Func = &stHENC2FuncBase;

		V_DBG(VPU_DBG_INFO, "remap (PA : 0x%08x / VA : 0x%p) Dec ADDR : 0x%p",
				SHARE_POINT_ADDR, va,
				stHENC2Func->tccfp_vpu_hevc_enc);

		iounmap(va);
	}
	return ret;
}
#endif

static int get_vpu_hevc_enc2_access_addr(void)
{
	int ret = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
	ret = get_vpu_hevc_enc2_access_addr_file();
#else
	ret = get_vpu_hevc_enc2_access_addr_mem();
#endif

	if (ret == 0) {
		ret = check_vpu_hevc_enc_access_addr_valid();

		if (ret == 0) {
			tcc_vpu_hevc_enc2 = (tccfp_vpu_hevc_enc_t)stHENC2Func->tccfp_vpu_hevc_enc;
			tcc_vpu_hevc_enc2_esc = (tccfp_vpu_hevc_enc_esc_t)stHENC2Func->tccfp_vpu_hevc_enc_esc;
			tcc_vpu_hevc_enc2_ext = (tccfp_vpu_hevc_enc_ext_t)stHENC2Func->tccfp_vpu_hevc_enc_ext;
		} else {
			ret = -1;
		}
	}

	return ret;
}

#else

extern int tcc_vpu_hevc_enc2(int Op, vcodec_handle_t *pHandle,
					void *pParam1, void *pParam2);
extern int tcc_vpu_hevc_enc2_esc(int Op, vcodec_handle_t *pHandle,
					void *pParam1, void *pParam2);
extern int tcc_vpu_hevc_enc2_ext(int Op, vcodec_handle_t *pHandle,
					void *pParam1, void *pParam2);
#endif //#if defined(USE_ACCESS_POINT)


#if DEFINED_CONFIG_VENC_CNT_1to16
static int tcc_vpu_hevc_enc2_l(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2) VPU_NO_SANITIZE_CFI
{
	return tcc_vpu_hevc_enc2(Op, pHandle, pParam1, pParam2);
}
#endif

struct VpuList *vmgr_hevc_enc2_list_manager(struct VpuList *args,
					unsigned int cmd)
{
	struct VpuList *ret = NULL;
	struct VpuList *oper_data = (struct VpuList *) args;

	if (oper_data == NULL) {
		if ((cmd == (unsigned int)LIST_ADD) ||
			(cmd == (unsigned int)LIST_DEL)) {
			V_DBG(VPU_DBG_ERROR, "Data is null, cmd=%d", cmd);
			return NULL;
		}
	}

	if (cmd == (unsigned int)LIST_ADD) {
		*oper_data->vpu_result = RET0;
	}

	mutex_lock(&vmgr_hevc_enc2_data.comm_data.list_mutex);
	{
		switch (cmd) {
		case LIST_ADD:
			*oper_data->vpu_result |= RET1;
			list_add_tail(&oper_data->list,
				&vmgr_hevc_enc2_data.comm_data.main_list);
			if( vmgr_hevc_enc2_data.cmd_queued > INT_MAX_U) {
				V_DBG(VPU_DBG_ERROR, "Cmd queued is already FULL");
			} else {
				vmgr_hevc_enc2_data.cmd_queued++;
			}

			if(vmgr_hevc_enc2_data.comm_data.thread_intr > INT_MAX_U) {
				V_DBG(VPU_DBG_ERROR, "Comm data thread interrupt count is already NULL");
			} else {
				vmgr_hevc_enc2_data.comm_data.thread_intr++;
			}
			break;
		case LIST_DEL:
			list_del(&oper_data->list);
			if(vmgr_hevc_enc2_data.cmd_queued > 0U) {
				vmgr_hevc_enc2_data.cmd_queued--;
			}
			break;
		case LIST_IS_EMPTY:
			if (list_empty(&vmgr_hevc_enc2_data.comm_data.main_list) != 0) {
				ret = &hevc_enc_mgr_vlist;
			}
			break;
		case (unsigned int)LIST_GET_ENTRY:
			ret = list_first_entry(
					&vmgr_hevc_enc2_data.comm_data.main_list,
					struct VpuList, list);
			break;
		default:
			/* Nothing to do */
			break;
		}
	}
	mutex_unlock(&vmgr_hevc_enc2_data.comm_data.list_mutex);

	if (cmd == (unsigned int)LIST_ADD) {
		wake_up_interruptible(&vmgr_hevc_enc2_data.comm_data.thread_wq);
	}

	return ret;
}

#if 0 // Keep the code for future use
static void vmgr_hevc_enc2_wait_process(int wait_ms)
{
	int max_count = wait_ms / 20;

	//wait!! in case exceptional processing. ex). sdcard out!!
	while (vmgr_hevc_enc2_data.cmd_processing) {
		max_count--;
		msleep(20);

		if (max_count <= 0) {
			V_DBG(VPU_DBG_ERROR,
			"cmd_processing(cmd %d) didn't finish!!",
				vmgr_hevc_enc2_data.current_cmd);
			break;
		}
	}
}
#endif

static irqreturn_t vmgr_hevc_enc2_isr_handler(int irq, void *dev_id)
{
	if ((cntInt_vpu_he) < (unsigned int)(UINT_MAX - 1U)) {
		cntInt_vpu_he++;
	} else {
		LOG_COVERITY("%d,%p", irq, dev_id);
	}

	atomic_inc(&vmgr_hevc_enc2_data.oper_intr);

	wake_up_interruptible(&vmgr_hevc_enc2_data.oper_wq);

	return IRQ_HANDLED;
}

static int vmgr_hevc_enc2_cmd_open(char *str)
{
	int ret = 0;

	V_DBG(VPU_DBG_SEQUENCE, "======> _vmgr_hevc_enc2_%s_open enter!! %d'th",
		str, atomic_read(&vmgr_hevc_enc2_data.opened));

	vmgr_hevc_enc2_enable_clock(0);

	if (atomic_read(&vmgr_hevc_enc2_data.opened) == 0) {
	#ifdef FORCED_ERROR
		forced_error_count = FORCED_ERR_CNT;
	#endif
		vmgr_hevc_enc2_data.only_decmode = 0;
		vmgr_hevc_enc2_data.clk_limitation = 1;
		vmgr_hevc_enc2_data.cmd_processing = 0;

		vmgr_hevc_enc2_hw_reset();
		vmgr_hevc_enc2_enable_irq(vmgr_hevc_enc2_data.irq);
		ret = vmem_init();
		if (ret < 0) {
			V_DBG(VPU_DBG_ERROR,
			"failed to allocate memory for VPU_HEVC_ENC(WAVE420L)!! %d",
				ret);
		}
		cntInt_vpu_he = 0;
	}

	atomic_inc(&vmgr_hevc_enc2_data.opened);

	V_DBG(VPU_DBG_SEQUENCE, "======> _vmgr_hevc_enc2_%s_open out!! %d'th",
		str, atomic_read(&vmgr_hevc_enc2_data.opened));

	return ret;
}

int vmgr_hevc_enc2_get_alive(void)
{
	return atomic_read(&vmgr_hevc_enc2_data.opened);
}

int vmgr_hevc_enc2_get_close(vputype type)
{
	return vmgr_hevc_enc2_data.closed[type];
}

int vmgr_hevc_enc2_set_close(vputype type, int value, int bfreemem)
{
	int ret;

	if (vmgr_hevc_enc2_get_close(type) == value) {
		V_DBG(VPU_DBG_CLOSE, " %d was already set to %d.", type, value);
		ret = -1;
	} else {
		vmgr_hevc_enc2_data.closed[type] = value;
		if (value == 1) {
			vmgr_hevc_enc2_data.handle[type] = 0x00;
			if (bfreemem != 0) {
				(void)vmem_proc_free_memory(type);
			}
		}
		ret = 0;
	}
	return ret;
}

static void vmgr_hevc_enc2_close_all(int bfreemem)
{
#if DEFINED_CONFIG_VENC_CNT_1to16
	vmgr_hevc_enc2_set_close(VPU_ENC, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_2to16
	vmgr_hevc_enc2_set_close(VPU_ENC_EXT, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_3to16
	vmgr_hevc_enc2_set_close(VPU_ENC_EXT2, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_4to16
	vmgr_hevc_enc2_set_close(VPU_ENC_EXT3, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_5to16
	vmgr_hevc_enc2_set_close(VPU_ENC_EXT4, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_6to16
	vmgr_hevc_enc2_set_close(VPU_ENC_EXT5, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_7to16
	vmgr_hevc_enc2_set_close(VPU_ENC_EXT6, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_8to16
	vmgr_hevc_enc2_set_close(VPU_ENC_EXT7, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_9to16
	vmgr_hevc_enc2_set_close(VPU_ENC_EXT8, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_10to16
	vmgr_hevc_enc2_set_close(VPU_ENC_EXT9, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_11to16
	vmgr_hevc_enc2_set_close(VPU_ENC_EXT10, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_12to16
	vmgr_hevc_enc2_set_close(VPU_ENC_EXT11, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_13to16
	vmgr_hevc_enc2_set_close(VPU_ENC_EXT12, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_14to16
	vmgr_hevc_enc2_set_close(VPU_ENC_EXT13, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_15to16
	vmgr_hevc_enc2_set_close(VPU_ENC_EXT14, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_16
	vmgr_hevc_enc2_set_close(VPU_ENC_EXT15, 1, bfreemem);
#endif
	LOG_COVERITY("%d", bfreemem);
}

int vmgr_hevc_enc2_process_ex(struct VpuList *cmd_list, vputype type,
					int Op, int *result)
{
	int ret = 1;
	if (atomic_read(&vmgr_hevc_enc2_data.opened) == 0) {
		ret = 0;
	} else {

		(void)pr_info("\n process_ex %d - 0x%x\n\n", type, Op);

		if (vmgr_hevc_enc2_get_close(type) == 0) {
			cmd_list->type = (unsigned int)type;
			cmd_list->cmd_type = Op;
			cmd_list->handle
			= vmgr_hevc_enc2_data.handle[(unsigned int)type];
			cmd_list->args = NULL;
			cmd_list->comm_data = NULL;
			cmd_list->vpu_result = result;

			(void)vmgr_hevc_enc2_list_manager(cmd_list,
						(unsigned int)LIST_ADD);
		}
	}

	return ret;
}

static int vmgr_hevc_enc2_proc_exit_by_external(struct VpuList *list,
					int *result, unsigned int type)
{
	int ret = 0;
	if ((type < (unsigned int)VPU_MAX) &&
		(vmgr_hevc_enc2_get_close((vputype)type) == 0) &&
		(vmgr_hevc_enc2_data.handle[(unsigned int)type] != 0L)) {
		list->type = type;
		list->cmd_type = VPU_ENC_CLOSE;
		list->handle = vmgr_hevc_enc2_data.handle[type];
		list->args = NULL;
		list->comm_data = NULL;
		list->vpu_result = result;

		(void)pr_info("%s for %d!!\n", __func__, type);
		(void)vmgr_hevc_enc2_list_manager(list, (unsigned int)LIST_ADD);

		ret = 1;
	}

	return ret;
}

static int vmgr_hevc_enc2_external_all_close(int wait_ms)
{
	int type = 0;
	int max_count = 0;
	int ret;

	for (type = (int)VPU_ENC; type < (int)VPU_HEVC_ENC_MAX; type++) {
		if (vmgr_hevc_enc2_proc_exit_by_external(
				&vmgr_hevc_enc2_data.vList[type],
				&ret, (unsigned int)type) != 0) {
			max_count = wait_ms / 10;

			while ((type < (int)VPU_MAX)  &&
			  (vmgr_hevc_enc2_get_close((vputype)type) == 0)  ) {
				if(max_count > 0) {
					max_count--;
				}
				usleep_range(0, 1000);	//msleep(10);
			}
		}
	}

	return 0;
}

static int vmgr_hevc_enc2_cmd_release(char *str)
{
	V_DBG(VPU_DBG_CLOSE, "======> _vmgr_hevc_enc2_%s_release In!! %d'th",
		str, atomic_read(&vmgr_hevc_enc2_data.opened));

	if (atomic_read(&vmgr_hevc_enc2_data.opened) > 0) {
		atomic_dec(&vmgr_hevc_enc2_data.opened);
	}

	if (atomic_read(&vmgr_hevc_enc2_data.opened) == 0) {
		unsigned int type = 0;
		unsigned int alive_cnt = 0;

		//To close whole vpu-hevc-enc instance
		// when being killed process opened this.
		if (!vmgr_hevc_enc2_data.bVpu_already_proc_force_closed) {
			vmgr_hevc_enc2_data.external_proc = 1;
			(void)vmgr_hevc_enc2_external_all_close(200);
			vmgr_hevc_enc2_data.external_proc = 0;
		}
		vmgr_hevc_enc2_data.bVpu_already_proc_force_closed = (bool)false;

		for (type = (unsigned int)VPU_ENC; type < (unsigned int)VPU_HEVC_ENC_MAX; type++) {
			if (vmgr_hevc_enc2_data.closed[type] == 0) {
				if (alive_cnt < (unsigned int)(UINT_MAX - 1)) {
					alive_cnt++;
				}
			}
		}

		if (alive_cnt != 0U) {
			(void)pr_info("VPU-HEVC-ENC might be cleared by force.\n");
		}

		atomic_set(&vmgr_hevc_enc2_data.oper_intr, 0);
		vmgr_hevc_enc2_data.cmd_processing = 0U;

		vmgr_hevc_enc2_close_all(1);

		vmgr_hevc_enc2_disable_irq(vmgr_hevc_enc2_data.irq);
		(void)vmgr_hevc_enc2_BusPrioritySetting(BUS_FOR_NORMAL, 0);

		vmem_deinit();

		vmgr_hevc_enc2_hw_assert();

		udelay(1000); //1ms
	}

	vmgr_hevc_enc2_disable_clock(0);

	if (vmgr_hevc_enc2_data.nOpened_Count < (unsigned int)(UINT_MAX-1)) {
		vmgr_hevc_enc2_data.nOpened_Count++;
	}

	V_DBG(VPU_DBG_ERROR,
	"======> _vmgr_hevc_enc2_%s_release Out!! %d'th, total = %d  - ENC(%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d)",
		str,
		atomic_read(&vmgr_hevc_enc2_data.opened),
		vmgr_hevc_enc2_data.nOpened_Count,
		vmgr_hevc_enc2_get_close(VPU_ENC),
		vmgr_hevc_enc2_get_close(VPU_ENC_EXT),
		vmgr_hevc_enc2_get_close(VPU_ENC_EXT2),
		vmgr_hevc_enc2_get_close(VPU_ENC_EXT3),
		vmgr_hevc_enc2_get_close(VPU_ENC_EXT4),
		vmgr_hevc_enc2_get_close(VPU_ENC_EXT5),
		vmgr_hevc_enc2_get_close(VPU_ENC_EXT6),
		vmgr_hevc_enc2_get_close(VPU_ENC_EXT7),
		vmgr_hevc_enc2_get_close(VPU_ENC_EXT8),
		vmgr_hevc_enc2_get_close(VPU_ENC_EXT9),
		vmgr_hevc_enc2_get_close(VPU_ENC_EXT10),
		vmgr_hevc_enc2_get_close(VPU_ENC_EXT11),
		vmgr_hevc_enc2_get_close(VPU_ENC_EXT12),
		vmgr_hevc_enc2_get_close(VPU_ENC_EXT13),
		vmgr_hevc_enc2_get_close(VPU_ENC_EXT14),
		vmgr_hevc_enc2_get_close(VPU_ENC_EXT15));

	return 0;
}

static int vmgr_hevc_enc2_open(struct inode *pinode, struct file *filp)
{
	if (vmgr_hevc_enc2_data.irq_reged == 0U) {
		V_DBG(VPU_DBG_ERROR, "not registered vpu-hevc-enc-mgr-irq");
	}

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	V_DBG(VPU_DBG_SEQUENCE, "%s In!! %d'th",
		__func__,
		atomic_read(&vmgr_hevc_enc2_data.dev_file_opened));
	atomic_inc(&vmgr_hevc_enc2_data.dev_file_opened);
	V_DBG(VPU_DBG_SEQUENCE, "%s Out!! %d'th",
		__func__,
		atomic_read(&vmgr_hevc_enc2_data.dev_file_opened));
#else
	mutex_lock(&vmgr_hevc_enc2_data.comm_data.file_mutex);
	(void)vmgr_hevc_enc2_cmd_open(str_file_hevc_enc);
	mutex_unlock(&vmgr_hevc_enc2_data.comm_data.file_mutex);
#endif

	filp->private_data = &vmgr_hevc_enc2_data;
	LOG_COVERITY("%p", pinode);
	return 0;
}

static int vmgr_hevc_enc2_release(struct inode *pinode, struct file *filp)
{
#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	V_DBG(VPU_DBG_CLOSE,
		"enter!! %d'th",
		atomic_read(&vmgr_hevc_enc2_data.dev_file_opened));

	atomic_dec(&vmgr_hevc_enc2_data.dev_file_opened);
	vmgr_hevc_enc2_data.nOpened_Count++;

	V_DBG(VPU_DBG_CLOSE,
	"Out!! %d'th, total = %d  - DEC(%d/%d/%d/%d/%d)",
		atomic_read(&vmgr_hevc_enc2_data.dev_file_opened),
		vmgr_hevc_enc2_data.nOpened_Count,
		vmgr_hevc_enc2_get_close(VPU_DEC),
		vmgr_hevc_enc2_get_close(VPU_DEC_EXT),
		vmgr_hevc_enc2_get_close(VPU_DEC_EXT2),
		vmgr_hevc_enc2_get_close(VPU_DEC_EXT3),
		vmgr_hevc_enc2_get_close(VPU_DEC_EXT4));
#else
	V_DBG(VPU_DBG_CLOSE, "enter");

	mutex_lock(&vmgr_hevc_enc2_data.comm_data.file_mutex);
	(void)vmgr_hevc_enc2_cmd_release(str_file_hevc_enc);
	mutex_unlock(&vmgr_hevc_enc2_data.comm_data.file_mutex);

	V_DBG(VPU_DBG_CLOSE, "out");
#endif
	LOG_COVERITY("%p,%p", pinode, filp);
	return 0;
}

#if DEFINED_CONFIG_VENC_CNT_1to16
static int vmgr_hevc_enc2_internal_handler(void)
{
	int ret, ret_code = RETCODE_INTR_DETECTION_NOT_ENABLED;
	int timeout = 200;
	unsigned long jtimeout;

	jtimeout = msecs_to_jiffies(timeout);
	if (jtimeout > LONG_MAX) {
		jtimeout = LONG_MAX;
	}

	if (vmgr_hevc_enc2_data.check_interrupt_detection) {
		if (atomic_read(&vmgr_hevc_enc2_data.oper_intr) > 0) {
			V_DBG(VPU_DBG_INTERRUPT,
			"Success-1: vpu hevc enc operation!! (isr cnt:%d)",
				cntInt_vpu_he);
			ret_code = RETCODE_SUCCESS;
		} else {
			ret = wait_event_interruptible_timeout(
					vmgr_hevc_enc2_data.oper_wq,
					atomic_read(
					  &vmgr_hevc_enc2_data.oper_intr) > 0,
					  (long)jtimeout);

			if (atomic_read(&vmgr_hevc_enc2_data.oper_intr) > 0) {
				V_DBG(VPU_DBG_INTERRUPT,
				"Success-2: vpu hevc enc operation!! (isr cnt:%d)",
					cntInt_vpu_he);
#if defined(FORCED_ERROR)
				if (forced_error_count-- <= 0) {
					ret_code = RETCODE_CODEC_EXIT;
					forced_error_count = FORCED_ERR_CNT;
					vetc_dump_reg_all(
					  vmgr_hevc_enc2_data.base_addr,
					  "_vmgr_internal_handler force-timed_out");
				} else {
					ret_code = RETCODE_SUCCESS;
				}
#else
				ret_code = RETCODE_SUCCESS;
#endif
			} else {
				V_DBG(VPU_DBG_ERROR,
				"[CMD 0x%x][ret:%d]: vpu timed_out(ref %d msec) => oper_intr[%d], isr cnt:%d!! [%d]th frame len %d",
				  vmgr_hevc_enc2_data.current_cmd,
				  ret,
				  timeout,
				  atomic_read(&vmgr_hevc_enc2_data.oper_intr),
				  cntInt_vpu_he,
				  vmgr_hevc_enc2_data.nDecode_Cmd,
				  vmgr_hevc_enc2_data.szFrame_Len
				);
				vetc_dump_reg_all(
					(char *)vmgr_hevc_enc2_data.base_addr,
					str_cmd_time_out);
				ret_code = RETCODE_CODEC_EXIT;
			}
		}
		atomic_set(&vmgr_hevc_enc2_data.oper_intr, 0);
		vmgr_hevc_enc2_status_clear(
			(const unsigned int *)vmgr_hevc_enc2_data.base_addr);
	}

	V_DBG(VPU_DBG_INTERRUPT, "out (Interrupt option=%d, isr cnt=%d, ev=%d)",
		vmgr_hevc_enc2_data.check_interrupt_detection,
		cntInt_vpu_he,
		ret_code
		);

	return ret_code;
}
#endif

static int vmgr_hevc_enc2_process(vputype type, int cmd, long pHandle,
					 void *args)
{
	int ret = 0;
#ifdef CONFIG_VPU_TIME_MEASUREMENT
	long long startTime = 0LL, endTime = 0LL;
	long long time_gap_us = 0LL;
#endif

	LOG_COVERITY("%d,%ld,%p", (int)type, pHandle, args);


	vmgr_hevc_enc2_data.check_interrupt_detection = 0;
	vmgr_hevc_enc2_data.current_cmd = cmd;

#if DEFINED_CONFIG_VENC_CNT_1to16
	if (type <= VPU_HEVC_ENC_MAX) {
		if (cmd != VPU_ENC_INIT) {
			if (vmgr_hevc_enc2_get_close(type)
				 || vmgr_hevc_enc2_data.handle[type] == 0x00) {
				return RETCODE_MULTI_CODEC_EXIT_TIMEOUT;
			}
		}

		if (cmd != VPU_ENC_ENCODE) {
			V_DBG(VPU_DBG_SEQUENCE, "Encoder(%d), command: 0x%x",
				type, cmd);
		}

		switch (cmd) {
		case VPU_ENC_INIT:
		{
			VENC_HEVC_INIT_t *arg = (VENC_HEVC_INIT_t *) args;

			union_codec_handle_t codec_handle;

			int width, height;

			vmgr_hevc_enc2_data.check_interrupt_detection = 1;

			vmgr_hevc_enc2_data.handle[type] = 0x00;

			width = arg->encInit.m_iPicWidth;
			if ((width<16) || (width > VPU_LIMIT_PICWIDTH)) {
				V_DBG(VPU_DBG_ERROR,
				  "## Enc :: not supported pic. width(%d)",
				  width);
				return RETCODE_INVALID_STRIDE;
			}

			height = arg->encInit.m_iPicHeight;
			if ((height<16) || (height > VPU_LIMIT_PICHEIGHT)) {
				V_DBG(VPU_DBG_ERROR,
				  "## Enc :: not supported pic. height(%d)",
				  height);
				return RETCODE_INVALID_STRIDE;
			}

			vmgr_hevc_enc2_data.szFrame_Len
			 = (((unsigned int)width * (unsigned int)height * 3)
				 / 2);

			arg->encInit.m_RegBaseAddr[VA]
			= (codec_addr_t) vmgr_hevc_enc2_data.base_addr;
			arg->encInit.m_Memcpy = vetc_memcpy;
			arg->encInit.m_Memset
			= (void (*) (void *, int, unsigned int, unsigned int))
				vetc_memset;
			arg->encInit.m_Interrupt
			= (int (*) (void)) vmgr_hevc_enc2_internal_handler;
			arg->encInit.m_Ioremap
			= (void * (*) (phys_addr_t, unsigned int)) vetc_ioremap;
			arg->encInit.m_Iounmap
			= (void (*) (void *)) vetc_iounmap;
			arg->encInit.m_reg_read
			= (unsigned int (*)(void *, unsigned int))
				vetc_reg_read;
			arg->encInit.m_reg_write
			= (void (*)(void *, unsigned int, unsigned int))
				vetc_reg_write;
			arg->encInit.m_Usleep
			= (void (*)(unsigned int, unsigned int))vetc_usleep;

			V_DBG(VPU_DBG_SEQUENCE,
			"Enc :: Init In =>Memcpy(0x%px),Memset(0x%px),Interrupt(0x%px),remap(0x%px),unmap(0x%px),read(0x%px),write(0x%px),sleep(0x%px)||workbuff(0x%px/0x%px),Reg(0x%px/0x%px), format(%d),W:H(%d:%d),Fps(%d),Bps(%d),Keyi(%d),Stream(0x%px/0x%px, %d)",
				arg->encInit.m_Memcpy,
				arg->encInit.m_Memset,
				arg->encInit.m_Interrupt,
				arg->encInit.m_Ioremap,
				arg->encInit.m_Iounmap,
				arg->encInit.m_reg_read,
				arg->encInit.m_reg_write,
				arg->encInit.m_Usleep,
				arg->encInit.m_BitWorkAddr[PA],
				arg->encInit.m_BitWorkAddr[VA],
				vmgr_hevc_enc2_data.base_addr,
				arg->encInit.m_RegBaseAddr[VA],
				arg->encInit.m_iBitstreamFormat,
				arg->encInit.m_iPicWidth,
				arg->encInit.m_iPicHeight,
				arg->encInit.m_iFrameRate,
				arg->encInit.m_iTargetKbps,
				arg->encInit.m_iKeyInterval,
				arg->encInit.m_BitstreamBufferAddr[PA],
				arg->encInit.m_BitstreamBufferAddr[VA],
				arg->encInit.m_iBitstreamBufferSize);

#if defined(USE_ACCESS_POINT)
			if (check_vpu_hevc_enc_access_addr_valid() != 0) {
				 V_DBG(VPU_DBG_ERROR,
					"Dec-%d ######################## Access address envalid!!(%d)",
					type, check_vpu_hevc_enc_access_addr_valid());

				return RETCODE_FAILURE;
			}
#endif
			codec_handle.pcodec_handle = &arg->handle;

			ret = tcc_vpu_hevc_enc2_l(cmd,
				(vcodec_handle_t *)(codec_handle.pvcodec_handle),
				(void *)(&arg->encInit),
				(void *)(&arg->encInitialInfo));

			if (ret != RETCODE_SUCCESS) {
				V_DBG(VPU_DBG_ERROR,
				" :: Init failed with ret(0x%x)", ret);
				if (ret != RETCODE_CODEC_EXIT) {
					vetc_dump_reg_all(
					  (char *)vmgr_hevc_enc2_data.base_addr,
					  str_cmd_init);
				}
			}

			if (ret != RETCODE_CODEC_EXIT && arg->handle != 0) {
				vmgr_hevc_enc2_data.handle[type] = arg->handle;
				vmgr_hevc_enc2_set_close(type, 0, 0);
				V_DBG(VPU_DBG_SEQUENCE,
				"vmgr_hevc_enc2_data.handle = 0x%x",
					arg->handle);
			} else {
				//To free memory!!
				vmgr_hevc_enc2_set_close(type, 0, 0);
				vmgr_hevc_enc2_set_close(type, 1, 1);
			}
			V_DBG(VPU_DBG_SEQUENCE,
			" :: Init Done Handle(0x%x)", arg->handle);
			vmgr_hevc_enc2_data.nDecode_Cmd = 0;

#ifdef CONFIG_VPU_TIME_MEASUREMENT
			vmgr_hevc_enc2_data.iTime[type].print_out_index
			= vmgr_hevc_enc2_data.iTime[type].proc_base_cnt = 0;
			vmgr_hevc_enc2_data.iTime[type].accumulated_proc_time
			= vmgr_hevc_enc2_data.iTime[type].accumulated_frame_cnt
			= 0;
			vmgr_hevc_enc2_data.iTime[type].proc_time_30frames = 0;
#endif
		}
		break;

		case VPU_ENC_REG_FRAME_BUFFER:
		{
			VENC_HEVC_SET_BUFFER_t *arg
			= (VENC_HEVC_SET_BUFFER_t *) args;

			V_DBG(VPU_DBG_SEQUENCE,
			"HEnc-%d: Register a frame buffer w PA(0x%px)/VA(0x%px)",
				type,
				arg->encBuffer.m_FrameBufferStartAddr[0],
				arg->encBuffer.m_FrameBufferStartAddr[1]);

			ret = tcc_vpu_hevc_enc2_l(cmd,
					  (vcodec_handle_t *)&pHandle,
				(void *)(&arg->encBuffer), (void *)NULL);
		}
		break;

		case VPU_ENC_PUT_HEADER:
		{
			VENC_HEVC_PUT_HEADER_t *arg
			= (VENC_HEVC_PUT_HEADER_t *) args;

			vmgr_hevc_enc2_data.check_interrupt_detection = 1;

			V_DBG(VPU_DBG_SEQUENCE,
			"HEnc-%d: put an Enc header w type(%d),size(%d),PA(0x%px)/VA(0x%px)",
				type,
				arg->encHeader.m_iHeaderType,
				arg->encHeader.m_iHeaderSize,
				arg->encHeader.m_HeaderAddr[0],
				arg->encHeader.m_HeaderAddr[1]);
			ret = tcc_vpu_hevc_enc2_l(cmd,
					  (vcodec_handle_t *)&pHandle,
				(void *)(&arg->encHeader), (void *)NULL);
		}
		break;

		case VPU_ENC_ENCODE:
		{
			VENC_HEVC_ENCODE_t *arg = (VENC_HEVC_ENCODE_t *)args;

			V_DBG(VPU_DBG_SEQUENCE,
			" enter w/ Handle(0x%x) :: 0x%x-0x%x-0x%x, %d-%d-%d, %d-%d-%d, %d, 0x%x-%d",
				pHandle,
				arg->encInput.m_PicYAddr,
				arg->encInput.m_PicCbAddr,
				arg->encInput.m_PicCrAddr,
				arg->encInput.m_iForceIPicture,
				arg->encInput.m_iSkipPicture,
				arg->encInput.m_iQuantParam,
				arg->encInput.m_iChangeRcParamFlag,
				arg->encInput.m_iChangeTargetKbps,
				arg->encInput.m_iChangeFrameRate,
				arg->encInput.m_iChangeKeyInterval,
				arg->encInput.m_BitstreamBufferAddr,
				arg->encInput.m_iBitstreamBufferSize);

			vmgr_hevc_enc2_data.check_interrupt_detection = 1;

#ifdef CONFIG_VPU_TIME_MEASUREMENT
			startTime = vetc_GetKtime();
#endif

			ret = tcc_vpu_hevc_enc2_l(cmd,
					  (vcodec_handle_t *)&pHandle,
				(void *)(&arg->encInput),
				(void *)(&arg->encOutput));

#ifdef CONFIG_VPU_TIME_MEASUREMENT
			endTime = vetc_GetKtime();
#endif
			vmgr_hevc_enc2_data.nDecode_Cmd++;

			V_DBG(VPU_DBG_SEQUENCE,
			" out w/ [%d] !! PicType[%d], Encoded_size[%d]",
				ret,
				arg->encOutput.m_iPicType,
				arg->encOutput.m_iBitstreamOutSize);
		}
		break;

		case VPU_ENC_CLOSE:
			V_DBG(VPU_DBG_SEQUENCE, "VPU_ENC_CLOSE !!");
			vmgr_hevc_enc2_data.check_interrupt_detection = 1;
			ret = tcc_vpu_hevc_enc2_l(cmd,
					  (vcodec_handle_t *)&pHandle,
				(void *)NULL, (void *)NULL);
			vmgr_hevc_enc2_set_close(type, 1, 1);
			break;

		default:
			V_DBG(VPU_DBG_ERROR,
			":: not supported command(0x%x)", cmd);
			return 0x999;
		}
	}

#ifdef CONFIG_VPU_TIME_MEASUREMENT
	time_gap_us = vetc_GetTimediff_us(endTime, startTime);

	if (cmd == VPU_ENC_INIT) {
		V_DBG(VPU_DBG_PERF, "Elapsed time for VENC_INIT[dev-%u]: %d us",
				type, time_gap_us);
	} else if (cmd == VPU_ENC_ENCODE) {
		printMeasurementTime((void*)&vmgr_hevc_enc2_data, type, 0, time_gap_us);
	}
#endif
#endif

	return ret;
}

static int vmgr_hevc_enc2_operation(void)
{
	int oper_finished;
	struct VpuList *oper_data = NULL;

	while (vmgr_hevc_enc2_list_manager(NULL, (unsigned int)LIST_IS_EMPTY) == NULL) {
		V_DBG(VPU_DBG_THREAD, ":: not empty cmd_queued (%d)",
		 vmgr_hevc_enc2_data.cmd_queued);

		vmgr_hevc_enc2_data.cmd_processing = 1;
		oper_finished = 1;

		oper_data
		 = (struct VpuList *)vmgr_hevc_enc2_list_manager(NULL,
						(unsigned int)LIST_GET_ENTRY);
		if (oper_data == NULL) {
			V_DBG(VPU_DBG_ERROR, "data is null");
			vmgr_hevc_enc2_data.cmd_processing = 0;
			return 0;
		}

		*oper_data->vpu_result |= RET2;

		V_DBG(VPU_DBG_THREAD,
		"[%d] :: cmd = 0x%x, vmgr_hevc_enc2_data.cmd_queued (%d)",
			oper_data->type,
			oper_data->cmd_type,
			vmgr_hevc_enc2_data.cmd_queued);

		if ((oper_data->type >= (unsigned int)VPU_ENC)
			&& (oper_data->type < (unsigned int)VPU_HEVC_ENC_MAX) ) {
			*oper_data->vpu_result |= RET3;
			*oper_data->vpu_result
			= vmgr_hevc_enc2_process((vputype)oper_data->type,
				oper_data->cmd_type,
				oper_data->handle,
				oper_data->args);
			oper_finished = 1;
			if (*oper_data->vpu_result != RETCODE_SUCCESS) {
				if ((*oper_data->vpu_result
					!= RETCODE_INSUFFICIENT_BITSTREAM) &&
					(*oper_data->vpu_result
					!= RETCODE_INSUFFICIENT_BITSTREAM_BUF)) {

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0)
					V_DBG(VPU_DBG_ERROR,
						"- out[0x%px] :: type = %u, vmgr_hevc_enc2_data.handle = 0x%lx, cmd = %d, frame_len %u",
						(void *)(uintptr_t)*oper_data->vpu_result,
						oper_data->type,
						oper_data->handle,
						oper_data->cmd_type,
						vmgr_hevc_enc2_data.szFrame_Len);
#else
					V_DBG(VPU_DBG_ERROR,
						"- out[0x%px] :: type = %u, vmgr_hevc_enc2_data.handle = 0x%lx, cmd = %d, frame_len %u",
						*oper_data->vpu_result,
						oper_data->type,
						oper_data->handle,
						oper_data->cmd_type,
						vmgr_hevc_enc2_data.szFrame_Len);
#endif
				}

				if (*oper_data->vpu_result
					== RETCODE_CODEC_EXIT) {
					vmgr_hevc_enc2_restore_clock(0,
						atomic_read(
							&vmgr_hevc_enc2_data.opened));
					vmgr_hevc_enc2_close_all(1);
				}
			}
		} else {
			V_DBG(VPU_DBG_ERROR,
			" :: missed info or unknown command > type = 0x%x, cmd = 0x%x",
				oper_data->type,
				oper_data->cmd_type);

			*oper_data->vpu_result = RETCODE_FAILURE;
			oper_finished = 0;
		}

		if (oper_finished != 0) {
			int opened = atomic_read(&vmgr_hevc_enc2_data.opened);
			if ((oper_data->comm_data != NULL)
				&& (opened != 0)) {
				oper_data->comm_data->count++;
				if (oper_data->comm_data->count != 1U) {
					V_DBG(VPU_DBG_THREAD,
					"polling wakeup count = %d :: type(0x%x) cmd(0x%x)",
						oper_data->comm_data->count,
						oper_data->type,
						oper_data->cmd_type);
				}
				wake_up_interruptible(
					&oper_data->comm_data->wq);
			} else {
				V_DBG(VPU_DBG_ERROR,
				"Error: abnormal exception or external command was processed!! 0x%p - %d",
					oper_data->comm_data, opened);
			}
		} else {
			V_DBG(VPU_DBG_ERROR,
			"Error: abnormal exception 2!! 0x%p - %d",
				oper_data->comm_data,
				atomic_read(&vmgr_hevc_enc2_data.opened));
		}

		(void)vmgr_hevc_enc2_list_manager(oper_data, (unsigned int)LIST_DEL);

		vmgr_hevc_enc2_data.cmd_processing = 0;
	}

	return 0;
}

static int vmgr_hevc_enc2_thread(void *kthread)
{
	unsigned long jtimeout;

	V_DBG(VPU_DBG_THREAD, "enter");

	jtimeout = msecs_to_jiffies((const unsigned int)50);
	if (jtimeout > (unsigned long)LONG_MAX) {
		jtimeout = (unsigned long)LONG_MAX;
	}

	do {
		if (vmgr_hevc_enc2_list_manager(NULL, (unsigned int)LIST_IS_EMPTY) != NULL) {
			vmgr_hevc_enc2_data.cmd_processing = 0;

			(void)wait_event_interruptible_timeout(
				vmgr_hevc_enc2_data.comm_data.thread_wq,
				(vmgr_hevc_enc2_data.comm_data.thread_intr > 0U),
				(long)jtimeout);

			vmgr_hevc_enc2_data.comm_data.thread_intr = 0;
		} else {
			if ((atomic_read(&vmgr_hevc_enc2_data.opened) != 0)
				|| (vmgr_hevc_enc2_data.external_proc != 0U)) {
				(void)vmgr_hevc_enc2_operation();
			} else {
				struct VpuList *oper_data = NULL;

				V_DBG(VPU_DBG_THREAD, "DEL for empty");

				oper_data
				= vmgr_hevc_enc2_list_manager(NULL,
					(unsigned int)LIST_GET_ENTRY);
				if (oper_data != NULL) {
					(void)vmgr_hevc_enc2_list_manager(
						oper_data, (unsigned int)LIST_DEL);
				}
			}
		}
	} while (!kthread_should_stop());

	V_DBG(VPU_DBG_THREAD, "finish");
	LOG_COVERITY("%p", kthread);

	return 0;
}

static unsigned int hangup_rel_count_hevcenc;	// = 0;
static long vmgr_hevc_enc2_ioctl(struct file *filep,
			unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	CONTENTS_INFO info;
	OPENED_sINFO open_info;
	union {
		unsigned long ul_data;
		unsigned int *pui_data;
		int *pi_data;
		void *pv_data;
		const void *pcv_data;	//NULL
		CONTENTS_INFO *pci_data;
		OPENED_sINFO *posi_data;
	} uarg;

	uarg.pcv_data = NULL;
	uarg.ul_data = arg;

	mutex_lock(&vmgr_hevc_enc2_data.comm_data.io_mutex);

	switch (cmd) {
	case VPU_SET_CLK:
	case VPU_SET_CLK_KERNEL:
		if (cmd == (unsigned int)VPU_SET_CLK_KERNEL) {
			(void)memcpy(&info, (CONTENTS_INFO *)uarg.pci_data,
				sizeof(info));

			if ((info.type >= VPU_ENC) && (info.isSWCodec != 0U)) {
				vmgr_hevc_enc2_data.clk_limitation = 0;
				V_DBG(VPU_DBG_SEQUENCE,
				"The clock limitation for VPU HEVC ENC is released.");
			}
		} else {
			if (copy_from_user(&info,
					  (CONTENTS_INFO *)uarg.pci_data,
				sizeof(info)) != 0U) {
				ret = -EFAULT;
			} else {
				if ((info.type >= VPU_ENC) && (info.isSWCodec != 0U)) {
					vmgr_hevc_enc2_data.clk_limitation = 0;
					V_DBG(VPU_DBG_SEQUENCE,
					"The clock limitation for VPU HEVC ENC is released.");
				}
			}
		}
		break;

	case VPU_GET_FREEMEM_SIZE:
	case VPU_GET_FREEMEM_SIZE_KERNEL:
	{
		unsigned int type = 0;
		unsigned int freemem_sz;

		if (cmd == (unsigned int)VPU_GET_FREEMEM_SIZE_KERNEL) {
			(void)memcpy(&type, (unsigned int *)uarg.pui_data,
			 sizeof(unsigned int));
			if (type > (unsigned int)VPU_HEVC_ENC_MAX) {
				type = (unsigned int)VPU_DEC;
			}
			freemem_sz = vmem_get_freemem_size((vputype)type);
			(void)memcpy((unsigned int *)uarg.pui_data, &freemem_sz,
			 sizeof(unsigned int));
		} else {
			if (copy_from_user(&type,
					   (unsigned int *)uarg.pui_data,
				 sizeof(unsigned int)) != 0U) {
				ret = -EFAULT;
			} else {
				if (type > (unsigned int)VPU_HEVC_ENC_MAX) {
					type = (unsigned int)VPU_DEC;
				}
				freemem_sz
				= vmem_get_freemem_size((vputype)type);
				if (copy_to_user((unsigned int *)uarg.pui_data,
					&freemem_sz,
					sizeof(unsigned int)) != 0U) {
					ret = -EFAULT;
				}
			}
		}
	}
	break;

	case VPU_HW_RESET:
		vmgr_hevc_enc2_hw_reset();
		break;

	case VPU_SET_MEM_ALLOC_MODE:
	case VPU_SET_MEM_ALLOC_MODE_KERNEL:
		if (cmd == (unsigned int)VPU_SET_MEM_ALLOC_MODE_KERNEL) {
			(void)memcpy(&open_info,
					(OPENED_sINFO *)uarg.posi_data,
			sizeof(OPENED_sINFO));

			if (open_info.opened_cnt != 0U) {
				vmem_set_only_decode_mode((int)open_info.type);
			}
			ret = 0;
		} else {
			if (copy_from_user(&open_info,
					  (OPENED_sINFO *)uarg.posi_data,
				 sizeof(OPENED_sINFO)) != 0U) {
				ret = -EFAULT;
			} else {
				if (open_info.opened_cnt != 0U) {
					vmem_set_only_decode_mode(
					  (int)open_info.type);
				}
				ret = 0;
			}
		}
		break;

	case VPU_CHECK_CODEC_STATUS:
	case VPU_CHECK_CODEC_STATUS_KERNEL:
		if (cmd == (unsigned int)VPU_CHECK_CODEC_STATUS_KERNEL) {
			(void)memcpy((int *)uarg.pi_data,
			vmgr_hevc_enc2_data.closed,
			sizeof(vmgr_hevc_enc2_data.closed));
		} else {
			if (copy_to_user((int *)uarg.pi_data,
				vmgr_hevc_enc2_data.closed,
				sizeof(vmgr_hevc_enc2_data.closed)) != 0U) {
				ret = -EFAULT;
			} else {
				ret = 0;
			}
		}
		break;

	case VPU_CHECK_INSTANCE_AVAILABLE:
	case VPU_CHECK_INSTANCE_AVAILABLE_KERNEL:
	{
		unsigned int nAvailable_Instance = 0;
		unsigned int type = (unsigned int)VPU_DEC;

		ret = 0;

		if (cmd == (unsigned int)VPU_CHECK_INSTANCE_AVAILABLE_KERNEL) {
			if (memcpy(&type, (int *)uarg.pi_data,
				sizeof(unsigned int)) == NULL) {
				ret = -EFAULT;
			} else {
				if (copy_from_user(&type, (int *)uarg.pi_data,
						sizeof(unsigned int)) != 0U) {
					ret = -EFAULT;
				}
			}
		}

		if (ret == 0) {
			vdec_check_instance_available(&nAvailable_Instance);

			if (cmd == (unsigned int)VPU_CHECK_INSTANCE_AVAILABLE_KERNEL) {
				(void)memcpy((unsigned int *)uarg.pui_data,
					&nAvailable_Instance,
					sizeof(unsigned int));
			} else {
				if (copy_to_user((unsigned int *)uarg.pui_data,
					&nAvailable_Instance,
					sizeof(unsigned int)) != 0U) {
					ret = -EFAULT;
				}
			}
		}
	}
	break;

	case VPU_GET_INSTANCE_IDX:
	case VPU_GET_INSTANCE_IDX_KERNEL:
		{
			INSTANCE_INFO iInst;

			if (cmd == (unsigned int)VPU_GET_INSTANCE_IDX_KERNEL) {
				(void)memcpy(&iInst, (int *)uarg.pi_data,
					sizeof(INSTANCE_INFO));
			} else {
				if (copy_from_user(&iInst, (int *)uarg.pi_data,
					sizeof(INSTANCE_INFO)) != 0U) {
					ret = -EFAULT;
				}
			}

			if (ret == 0) {
				venc_get_instance(&iInst.nInstance);

				if (cmd == (unsigned int)(VPU_GET_INSTANCE_IDX_KERNEL)) {
					(void)memcpy(
						(int *)uarg.pi_data, &iInst,
						sizeof(INSTANCE_INFO));
				} else {
					if (copy_to_user(
						(int *)uarg.pi_data, &iInst,
						sizeof(INSTANCE_INFO)) != 0U) {
						ret = -EFAULT;
					}
				}
			}
		}
		break;

	case VPU_CLEAR_INSTANCE_IDX:
	case VPU_CLEAR_INSTANCE_IDX_KERNEL:
		{
			INSTANCE_INFO iInst;

			if (cmd == (unsigned int)VPU_CLEAR_INSTANCE_IDX_KERNEL) {
				(void)memcpy(&iInst, (int *)uarg.pi_data,
				sizeof(INSTANCE_INFO));
			} else {
				if (copy_from_user(&iInst, (int *)uarg.pi_data,
					sizeof(INSTANCE_INFO)) != 0U) {
					ret = -EFAULT;
				}
			}
			if (ret == 0) {
				venc_clear_instance(iInst.nInstance);
			}
		}
		break;

	case VPU_SET_RENDERED_FRAMEBUFFER:
	case VPU_SET_RENDERED_FRAMEBUFFER_KERNEL:
		V_DBG(VPU_DBG_ERROR, "VPU_SET_RENDERED_FRAMEBUFFER is not support ioctl in encoder");
		break;

	case VPU_GET_RENDERED_FRAMEBUFFER:
	case VPU_GET_RENDERED_FRAMEBUFFER_KERNEL:
		V_DBG(VPU_DBG_ERROR, "VPU_GET_RENDERED_FRAMEBUFFER is not support ioctl in encoder");
	break;

	case VPU_TRY_FORCE_CLOSE:
	case VPU_TRY_FORCE_CLOSE_KERNEL:
		if (!vmgr_hevc_enc2_data.bVpu_already_proc_force_closed) {
			vmgr_hevc_enc2_data.external_proc = 1;
			(void)vmgr_hevc_enc2_external_all_close(200);
			vmgr_hevc_enc2_data.external_proc = 0;
			vmgr_hevc_enc2_data
			.bVpu_already_proc_force_closed = (bool)true;
		}
		break;

	case VPU_TRY_CLK_RESTORE:
	case VPU_TRY_CLK_RESTORE_KERNEL:
		vmgr_hevc_enc2_restore_clock(0,
			atomic_read(&vmgr_hevc_enc2_data.opened));
		break;

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	case VPU_TRY_OPEN_DEV:
	case VPU_TRY_OPEN_DEV_KERNEL:
		(void)vmgr_hevc_enc2_cmd_open("cmd");
		break;

	case VPU_TRY_CLOSE_DEV:
	case VPU_TRY_CLOSE_DEV_KERNEL:
		(void)vmgr_hevc_enc2_cmd_release("cmd");
		break;
#endif

	case VPU_TRY_HANGUP_RELEASE:
		if (hangup_rel_count_hevcenc <
			(unsigned int)(UINT_MAX - 1U)) {
			hangup_rel_count_hevcenc++;
		}
		V_DBG(VPU_DBG_CMD,
			" vpu ===> VPU_TRY_HANGUP_RELEASE %d'th",
			hangup_rel_count_hevcenc);
		break;

	default:
		V_DBG(VPU_DBG_ERROR, "Unsupported ioctl[%d]!!!", cmd);
		LOG_COVERITY("%p", filep);
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&vmgr_hevc_enc2_data.comm_data.io_mutex);

	return ret;
}

#ifdef CONFIG_COMPAT
static long vmgr_hevc_enc2_compat_ioctl(struct file *filep,
					unsigned int cmd, unsigned long arg)
{
	unsigned int iarg;

	if (arg < (unsigned long)INT_MAX) {
		iarg = (unsigned int)arg;
	} else {
		iarg = (unsigned int)INT_MAX;
	}

	return vmgr_hevc_enc2_ioctl(filep, cmd, (unsigned long)compat_ptr(iarg));

}
#endif

static int vmgr_hevc_enc2_mmap(struct file *filep, struct vm_area_struct *vma)
{
	int ret;
	unsigned long current_vm_range = (vma->vm_end >= vma->vm_start) ?
		(vma->vm_end - vma->vm_start) : 0U;

#if defined(CONFIG_TCC_MEM)
	if (range_is_allowed(vma->vm_pgoff, current_vm_range) < 0) {
		V_DBG(VPU_DBG_ERROR, "_vmgr_mmap: this address is not allowed");
		return -EAGAIN;
	}
#endif

	vma->vm_page_prot = vmem_get_pgprot(vma->vm_page_prot, vma->vm_pgoff);
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, current_vm_range, vma->vm_page_prot) != 0) {
		V_DBG(VPU_DBG_ERROR, "_vmgr_mmap :: remap_pfn_range failed");
		LOG_COVERITY("%p", filep);
		ret = -EAGAIN;
	} else {
		vma->vm_ops	= NULL;
		vetc_vm_flags_set(vma, VM_IO | VM_DONTEXPAND | VM_PFNMAP);
		ret = 0;
	}
	return ret;
}

static const struct file_operations vmgr_hevc_enc2_fops = {
	.open			= vmgr_hevc_enc2_open,
	.release		= vmgr_hevc_enc2_release,
	.mmap			= vmgr_hevc_enc2_mmap,
	.unlocked_ioctl		= vmgr_hevc_enc2_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl		= vmgr_hevc_enc2_compat_ioctl,
#endif
};

static struct miscdevice vmgr_hevc_enc2_misc_device = {
	MISC_DYNAMIC_MINOR,
	VPU_HEVC_ENC2_MGR_NAME,
	&vmgr_hevc_enc2_fops,
};

int vmgr_hevc_enc2_opened(void)
{
	int ret = 1;
	if (atomic_read(&vmgr_hevc_enc2_data.opened) == 0) {
		ret = 0;
	}
	return ret;
}
EXPORT_SYMBOL(vmgr_hevc_enc2_opened);

int vmgr_hevc_enc2_probe(struct platform_device *pdev)
{
	int ret = 0;
	int type = 0;
	long irqv = 0;
	unsigned long int_flags;
	struct resource *presource = NULL;
	void *tTmpPtr = NULL;

	ret = vetc_check_ip_enabled((int)vip_wave420l);

	if ((ret < 0)
			|| (pdev->dev.of_node == NULL)) {
		return -ENODEV;
	}

	V_DBG(VPU_DBG_PROBE, "enter");

	(void)memset(&vmgr_hevc_enc2_data, 0, sizeof(struct mgr_data_t));
	for (type = (int)VPU_ENC; type < (int)VPU_HEVC_ENC_MAX; type++) {
		vmgr_hevc_enc2_data.closed[type] = 1;
	}

	vmgr_hevc_enc2_init_variable();

	atomic_set(&vmgr_hevc_enc2_data.oper_intr, 0);
#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	atomic_set(&vmgr_hevc_enc2_data.dev_file_opened, 0);
#endif

	//Fetching IRQ number for device
	{
		int irq = platform_get_irq(pdev, 0);

		if (irq < 0) {
			dev_err(&pdev->dev, "could not get IRQ");
			return irq;
		}
		vmgr_hevc_enc2_data.irq = (unsigned int)irq;
	}

	vmgr_hevc_enc2_data.nOpened_Count = 0U;
	presource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (presource == NULL) {
		V_DBG(VPU_DBG_ERROR, "failed to get phy memory resourcea");
		return -1;
	}
	presource->end += 1U;

	vmgr_hevc_enc2_data.base_addr = devm_ioremap(&pdev->dev,
					presource->start,
					(presource->end - presource->start));

	irqv = (long)vmgr_hevc_enc2_data.irq;
	if (irqv < 32) {
		irqv = 0;
	} else {
		irqv -= 32;
	}
	V_DBG(VPU_DBG_PROBE,
		"VPU-HEVC-ENC base address [0x%x -> 0x%px], resource size [%d], irq num [%d]",
		presource->start,
		vmgr_hevc_enc2_data.base_addr,
		(presource->end - presource->start),
		irqv);

	vidsys_conf_reg = (void __iomem *)of_iomap(pdev->dev.of_node, 1);
	if (vidsys_conf_reg == NULL) {
		V_DBG(VPU_DBG_ERROR, "vidsys_conf_reg: NULL");
	} else {
		unsigned int chipfamilyname = vetc_get_chip_family();

		V_DBG(VPU_DBG_PROBE, "Chip famil name is TCC%xx", chipfamilyname);
		if (chipfamilyname == 0x8050U) {
			vetc_reg_write((void *)vidsys_conf_reg, 0x84, 0x9c9a3000U);	//MISRA C-2012 Literals and Constants (MISRA C-2012 Rule 7.2) 1. misra_c_2012_rule_7_2_violation: Numeric literal 2627350528U is unsigned but does not use a u or U suffix.
			V_DBG(VPU_DBG_PROBE,
				"Video sub-system cfg reg (0x%px) : Sec. AXI (0x%x)",
				vidsys_conf_reg,
				vetc_reg_read((void *) vidsys_conf_reg, 0x84));
		}
	}

	vmgr_hevc_enc2_get_clock(pdev->dev.of_node);
	vmgr_hevc_enc2_get_reset(pdev->dev.of_node);

	init_waitqueue_head(&vmgr_hevc_enc2_data.comm_data.thread_wq);
	init_waitqueue_head(&vmgr_hevc_enc2_data.oper_wq);

	mutex_init(&vmgr_hevc_enc2_data.comm_data.list_mutex);
	mutex_init(&vmgr_hevc_enc2_data.comm_data.io_mutex);
	mutex_init(&vmgr_hevc_enc2_data.comm_data.file_mutex);

	INIT_LIST_HEAD(&vmgr_hevc_enc2_data.comm_data.main_list);
	INIT_LIST_HEAD(&vmgr_hevc_enc2_data.comm_data.wait_list);

	ret = vmem_config();
	if (ret < 0) {
		V_DBG(VPU_DBG_ERROR,
		"unable to configure memory for VPU HEVC ENC!! %d",
			ret);
		return -ENOMEM;
	}

#if defined(USE_ACCESS_POINT)
	if (stHENC2Func == NULL) {
		ret = get_vpu_hevc_enc2_access_addr();
		if (ret != 0) {
			V_DBG(VPU_DBG_ERROR, "Getting for library access point failed!!");
			return RETCODE_FAILURE;
		}
	}
#endif

	vmgr_hevc_enc2_init_interrupt();
	int_flags = vmgr_hevc_enc2_get_int_flags();
	ret = vmgr_hevc_enc2_request_irq(vmgr_hevc_enc2_data.irq,
				vmgr_hevc_enc2_isr_handler,
				int_flags,
				VPU_HEVC_ENC_MGR_NAME,
				&vmgr_hevc_enc2_data);
	if (ret != 0) {
		V_DBG(VPU_DBG_ERROR, "to aquire vpu-hevc-enc-irq");
	}

	vmgr_hevc_enc2_data.irq_reged = 1;
	vmgr_hevc_enc2_disable_irq(vmgr_hevc_enc2_data.irq);

	kidle_task_hevce = kthread_run(vmgr_hevc_enc2_thread, NULL, "vHEVC_ENC_th");
	VPU_CAST_PT(tTmpPtr, kidle_task_hevce);
	if (IS_ERR(tTmpPtr)) {
		V_DBG(VPU_DBG_ERROR, "unable to create thread!!");
		kidle_task_hevce = NULL;
		return -1;
	}

	V_DBG(VPU_DBG_PROBE, "success: thread created!!");

	vmgr_hevc_enc2_close_all(1);

	ret = 0;
	if (misc_register(&vmgr_hevc_enc2_misc_device) != 0) {
		V_DBG(VPU_DBG_ERROR,
			"VPU HEVC ENC Manager: Couldn't register device");
		ret = -EBUSY;
	}

	return ret;

}
EXPORT_SYMBOL(vmgr_hevc_enc2_probe);

int vmgr_hevc_enc2_remove(struct platform_device *pdev)
{
	V_DBG(VPU_DBG_CLOSE, "enter");

	misc_deregister(&vmgr_hevc_enc2_misc_device);

	if (kidle_task_hevce != NULL) {
		(void)kthread_stop(kidle_task_hevce);
		kidle_task_hevce = NULL;
	}

	devm_iounmap(&pdev->dev,
		(void __iomem *)vmgr_hevc_enc2_data.base_addr);
	if (vmgr_hevc_enc2_data.irq_reged != 0U) {
		vmgr_hevc_enc2_free_irq(vmgr_hevc_enc2_data.irq,
			&vmgr_hevc_enc2_data);
		vmgr_hevc_enc2_data.irq_reged = 0;
	}

	vmgr_hevc_enc2_put_clock();
	vmgr_hevc_enc2_put_reset();
	vmem_deinit();

	V_DBG(VPU_DBG_CLOSE, "out :: thread stopped!!");

	return 0;
}
EXPORT_SYMBOL(vmgr_hevc_enc2_remove);

#if defined(CONFIG_PM)
int vmgr_hevc_enc2_suspend(struct platform_device *pdev, pm_message_t state)
{
	int i, open_count = 0;

	if (atomic_read(&vmgr_hevc_enc2_data.opened) != 0) {
		(void)pr_info("\n vpu hevc enc: suspend enter for ENC(%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d)\n",
			vmgr_hevc_enc2_get_close(VPU_ENC),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT2),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT3),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT4),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT5),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT6),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT7),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT8),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT9),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT10),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT11),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT12),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT13),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT14),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT15));

		(void)vmgr_hevc_enc2_external_all_close(200);

		open_count = atomic_read(&vmgr_hevc_enc2_data.opened);

		vmgr_hevc_enc2_hw_assert();

		for (i = 0; i < open_count; i++) {
			vmgr_hevc_enc2_disable_clock(0);
		}

		(void)pr_info("vpu hevc enc: suspend out for ENC(%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d)\n",
			vmgr_hevc_enc2_get_close(VPU_ENC),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT2),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT3),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT4),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT5),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT6),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT7),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT8),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT9),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT10),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT11),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT12),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT13),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT14),
			vmgr_hevc_enc2_get_close(VPU_ENC_EXT15));
	} else {
		LOG_COVERITY("%p, %d", pdev, state.event);
	}

	return 0;

}
EXPORT_SYMBOL(vmgr_hevc_enc2_suspend);

int vmgr_hevc_enc2_resume(struct platform_device *pdev)
{
	int i, open_count = 0;

	if (atomic_read(&vmgr_hevc_enc2_data.opened) != 0) {
		open_count = atomic_read(&vmgr_hevc_enc2_data.opened);

		for (i = 0; i < open_count; i++) {
			vmgr_hevc_enc2_enable_clock(0);
		}

		vmgr_hevc_enc2_hw_deassert();

		(void)pr_info("\n vpu: resume\n\n");
	} else {
		LOG_COVERITY("%p", pdev);
	}

	return 0;
}

EXPORT_SYMBOL(vmgr_hevc_enc2_resume);
#endif	//CONFIG_PM

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
MODULE_SOFTDEP("pre: vpu_lib jpu_lib hevc_lib vpu_4k_d2_lib vpu_hevc_enc_lib vpu");

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vpu hevc enc manager");
MODULE_LICENSE("GPL");

#endif /*CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC*/
