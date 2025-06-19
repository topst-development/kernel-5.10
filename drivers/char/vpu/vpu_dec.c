// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_comm.h"

#if (defined(CONFIG_VDEC_CNT_1) || defined(CONFIG_VDEC_CNT_2) || \
	defined(CONFIG_VDEC_CNT_3) || defined(CONFIG_VDEC_CNT_4) || \
	defined(CONFIG_VDEC_CNT_5))

#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/compat.h>

#include <linux/io.h>
#include <asm/div64.h>

#include "vpu_buffer.h"
#include "vpu_dec.h"
#include "vpu_rm.h"

#ifdef CONFIG_SUPPORT_TCC_VPU
#include "vpu_mgr.h"
#endif

#ifdef CONFIG_SUPPORT_TCC_JPU
#include "jpu_mgr.h"
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
#include "vpu_4k_d2_mgr.h"
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
#include "hevc_mgr.h"
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
#include "vp9_mgr.h"
#endif

#include "vpu_dec_flexio.h"

#define dprintk(msg...)  V_DBG(VPU_DBG_INFO, "TCC_VPU_MGR: " msg)
#define detailk(msg...)  V_DBG(VPU_DBG_INFO, "TCC_VPU_MGR: " msg)
#define err_dec(msg...)      V_DBG(VPU_DBG_ERROR, "TCC_VPU_MGR [Err]: " msg)

#undef VLOG_TAG
#define VLOG_TAG "[vdec]"

unsigned int vdec_poll(struct file *filp, poll_table *wait);
long vdec_poll_2(struct file *filp, int timeout_ms);
int vdec_mmap(struct file *filp, struct vm_area_struct *vma);

void vdec_inter_add_list(struct vpu_decoder_data *vdata, int cmd, void *args);
void vdec_init_list(struct vpu_decoder_data *vdata);
bool vdec_dev_pre_init(struct vpu_decoder_data *vdata);
void vdec_dev_post_init(struct vpu_decoder_data *vdata);

static struct mutex vdec_mutex;
static char fname_file_dec[] = "file";

static int isVPUDecSupportCodec(int decType)
{
	int ret = 0;

	if (decType == STD_AVC || decType == STD_VC1 ||
			decType == STD_MPEG2 || decType == STD_MPEG4 ||
			decType == STD_H263 || decType == STD_DIV3 ||
			decType == STD_EXT || decType == STD_AVS ||
			decType == STD_SH263 || decType == STD_VP8 ||
			decType == STD_THEORA || decType == STD_MVC) {
		ret = 1;
	}

	return ret;
}

void vdec_inter_add_list(struct vpu_decoder_data *vdata, int cmd, void *args)
{
	struct VpuList *oper_data;

	if ((vdata == NULL) || (vdata->gsDecType < 0)) {
		return;
	}

	oper_data = &vdata->vdec_list[vdata->list_idx];
	oper_data->type          = (unsigned int)vdata->gsDecType;
	oper_data->cmd_type      = cmd;

	oper_data->args          = args;
	oper_data->comm_data     = &vdata->vComm_data;
	oper_data->vpu_result    = &vdata->gsCommDecResult;
	*oper_data->vpu_result   = 0;

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		oper_data->handle = vdata->gsJpuDecInit_Info.gsJpuDecHandle;
		(void)jmgr_list_manager(oper_data, (unsigned int)LIST_ADD);
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		oper_data->handle = vdata->gsV4kd2DecInit_Info.gsV4kd2DecHandle;
		(void)vmgr_4k_d2_list_manager(oper_data, (unsigned int)LIST_ADD);
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		oper_data->handle = vdata->gsHevcDecInit_Info.gsHevcDecHandle;
		(void)hmgr_list_manager(oper_data, (unsigned int)LIST_ADD);
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		oper_data->handle = vdata->gsVp9DecInit_Info.gsVp9DecHandle;
		(void)vp9mgr_list_manager(oper_data, (unsigned int)LIST_ADD);
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		oper_data->handle = vdata->gsVpuDecInit_Info.gsVpuDecHandle;
		(void)vmgr_list_manager(oper_data, (unsigned int)LIST_ADD);
	}
#endif

	vdata->list_idx += 1;
	if (vdata->list_idx >= (unsigned)LIST_MAX ) {
		vdata->list_idx = 0;
	}
}

void vdec_init_list(struct vpu_decoder_data *vdata)
{
	int i;

	for (i = 0; i < (int)LIST_MAX; i++) {
		vdata->vdec_list[i].comm_data = NULL;
	}
	vdata->list_inited = true;
}

static int vdec_proc_init(struct vpu_decoder_data *vdata,
					void *arg, bool fromKernel)
{
	void *pArgs = NULL;

	V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
			"%s: enter !!", vdata->misc->name);

	if (vdata->list_inited == false) {
		vdec_init_list(vdata);
	}

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsJpuDecInit_Info, arg,
				sizeof(JDEC_INIT_t));
		} else {
			if (copy_from_user(&vdata->gsJpuDecInit_Info, arg,
				sizeof(JDEC_INIT_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsJpuDecInit_Info;
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsV4kd2DecInit_Info, arg,
				sizeof(VPU_4K_D2_INIT_t));
		} else {
			if (copy_from_user(&vdata->gsV4kd2DecInit_Info, arg,
				sizeof(VPU_4K_D2_INIT_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsV4kd2DecInit_Info;
		vdata->gsIsDiminishedCopy
			= (vdata->gsV4kd2DecInit_Info.gsV4kd2DecInit
				.m_uiDecOptFlags & (0x4000000)) ? 1 : 0; //(1<<26)
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsHevcDecInit_Info, arg,
				sizeof(HEVC_INIT_t));
		} else {
			if (copy_from_user(&vdata->gsHevcDecInit_Info, arg,
				sizeof(HEVC_INIT_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsHevcDecInit_Info;
		vdata->gsIsDiminishedCopy
			= (vdata->gsHevcDecInit_Info.gsHevcDecInit
			.m_uiDecOptFlags & (0x4000000)) ? 1 : 0; //(1<<26)
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsVp9DecInit_Info, arg,
				sizeof(VP9_INIT_t));
		} else {
			if (copy_from_user(&vdata->gsVp9DecInit_Info, arg,
				sizeof(VP9_INIT_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsVp9DecInit_Info;
		vdata->gsIsDiminishedCopy
			= (vdata->gsVp9DecInit_Info.gsVp9DecInit
				.m_uiDecOptFlags & (0x4000000)) ? 1 : 0; //(1<<26)
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsVpuDecInit_Info, arg,
				sizeof(VDEC_INIT_t));
		} else {
			if (copy_from_user(&vdata->gsVpuDecInit_Info, arg,
				sizeof(VDEC_INIT_t)) != 0U) {
				return -EFAULT;
			}
		}

		pArgs = (void *)&vdata->gsVpuDecInit_Info;
		vdata->gsIsDiminishedCopy
			= (vdata->gsVpuDecInit_Info.gsVpuDecInit
				.m_uiDecOptFlags & (0x4000000)) ? 1 : 0; //(1<<26)
	}
#endif

	if (fromKernel) {
		vdec_inter_add_list(vdata, VPU_DEC_INIT_KERNEL, pArgs);
	} else {
		vdec_inter_add_list(vdata, VPU_DEC_INIT, pArgs);
	}

	return 0;
}

static void vdec_force_close(struct vpu_decoder_data *vdata)
{
	vputype vtype;
	int ret;
	struct VpuList *cmd_list;

#ifdef CONFIG_SUPPORT_TCC_VPU
	int vmgr_alive = vmgr_get_alive();
#endif
#ifdef CONFIG_SUPPORT_TCC_JPU
	int jmgr_alive = jmgr_get_alive();
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	int vmgr_4kd2_alive = vmgr_4k_d2_get_alive();
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	int hmgr_alive = hmgr_get_alive();
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	int vp9mgr_alive = vp9mgr_get_alive();
#endif

	vdata->list_idx += 1;
	if (vdata->list_idx >= (unsigned)LIST_MAX ) {
		vdata->list_idx = 0;
	}

	cmd_list = &vdata->vdec_list[vdata->list_idx];

	if ((vdata->gsDecType < 0) || (vdata->gsDecType >= (int)VPU_MAX)) {
		return;
	} else {
		vtype = (vputype)vdata->gsDecType;
	}

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		if ((jmgr_get_close(vtype) == 0)
			&& (jmgr_alive != 0)) {
			int max_count = 100;
			(void)jmgr_process_ex(cmd_list,
				vtype,
				VPU_DEC_CLOSE,
				&ret);
			while (jmgr_get_close(
				vtype) == 0) {
				max_count--;
				msleep(20);
				if (max_count <= 0) {
					break;
				}
			}
		}
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		if ((vmgr_4k_d2_get_close(vtype) == 0)
			&& (vmgr_4kd2_alive != 0)) {
			int max_count = 100;

			(void)vmgr_4k_d2_process_ex(cmd_list,
				vtype,
				VPU_DEC_CLOSE,
				&ret);
			while (vmgr_4k_d2_get_close(
					vtype) == 0) {
				max_count--;
				msleep(20);
				if (max_count <= 0) {
					break;
				}
			}
		}
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		if ((hmgr_get_close(vtype) == 0)
			&& (hmgr_alive != 0)) {
			int max_count = 100;

			(void)hmgr_process_ex(cmd_list,
				vtype,
				VPU_DEC_CLOSE,
				&ret);
			while (hmgr_get_close(
					vtype) == 0) {
				max_count--;
				msleep(20);
				if (max_count <= 0) {
					break;
				}
			}
		}
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		if ((vp9mgr_get_close(vtype) == 0)
			&& (vp9mgr_alive != 0)) {
			int max_count = 100;

			vp9mgr_process_ex(cmd_list, vtype,
				VPU_DEC_CLOSE, &ret);
			while (!vp9mgr_get_close(vtype)) {
				max_count--;
				msleep(20);
				if (max_count <= 0) {
					break;
				}
			}
		}
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		if ( (vmgr_get_close(vtype) == 0)
			&& (vmgr_alive != 0)) {
			int max_count = 100;

			(void)vmgr_process_ex(cmd_list,
				vtype,
				VPU_DEC_CLOSE, &ret);
			while (vmgr_get_close(vtype) == 0) {
				max_count--;
				msleep(20);
				if (max_count <= 0) {
					break;
				}
			}
		}
	}
#endif
}

bool vdec_dev_pre_init(struct vpu_decoder_data *vdata)
{
	if (vdata->vComm_data.dev_opened > 1U) {
		vputype vtype = (vputype)vdata->gsDecType;

		V_DBG(VPU_DBG_SEQUENCE, "TYPE:%d already opened!!", vtype);

#ifdef CONFIG_SUPPORT_TCC_JPU
		if (vdata->gsCodecType == STD_MJPG) {
			err_dec(
					"Jpu(%s) has been already opened. Maybe there is exceptional stop!! Mgr(%d)/Dec(%d)",
					vdata->misc->name,
					jmgr_get_alive(),
					jmgr_get_close(vtype));
		}
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
		if (vdata->gsCodecType == STD_HEVC
				|| vdata->gsCodecType == STD_VP9) {
			err_dec(
					"VPU-4K-D2(%s) has been already opened. Maybe there is exceptional stop!! Mgr(%d)/Dec(%d)",
					vdata->misc->name,
					vmgr_4k_d2_get_alive(),
					vmgr_4k_d2_get_close(vtype));
		}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
		if (vdata->gsCodecType == STD_HEVC) {
			err_dec(
					"Hevc(%s) has been already opened. Maybe there is exceptional stop!! Mgr(%d)/Dec(%d)",
					vdata->misc->name,
					hmgr_get_alive(),
					hmgr_get_close(vtype));
		}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
		if (vdata->gsCodecType == STD_VP9) {
			err_dec(
					"Vp9(%s) has been already opened. Maybe there is exceptional stop!! Mgr(%d)/Dec(%d)",
					vdata->misc->name,
					vp9mgr_get_alive(),
					vp9mgr_get_close(vtype));
		}
#endif

#ifdef CONFIG_SUPPORT_TCC_VPU
		if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
			err_dec(
					"Vpu_#%d(%s) device has been already opened(%d). Maybe there is exceptional stop!! Mgr(%d)/Dec(%d)",
					vtype,
					vdata->misc->name,
					vdata->vComm_data.dev_opened,
					vmgr_get_alive(),
					vmgr_get_close(vtype));
		}
#endif

		vdec_force_close(vdata);

#ifdef CONFIG_SUPPORT_TCC_JPU
		if (vdata->gsCodecType == STD_MJPG) {
			(void)jmgr_set_close(vtype, 1, 1);
		}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
		if (vdata->gsCodecType == STD_HEVC
				|| vdata->gsCodecType == STD_VP9) {
			(void)vmgr_4k_d2_set_close(vtype, 1, 1);
		}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
		if (vdata->gsCodecType == STD_HEVC) {
			(void)hmgr_set_close(vtype, 1, 1);
		}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
		if (vdata->gsCodecType == STD_VP9) {
			(void)vp9mgr_set_close(vtype, 1, 1);
		}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
		if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
			(void)vmgr_set_close(vtype, 1, 1);
		}
#endif
		vdata->vComm_data.dev_opened--;
	}

	vdata->gsCodecType = -1;
	vdata->list_inited = false;

	return true;
}

void vdec_dev_post_init(struct vpu_decoder_data *vdata)
{
#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		(void)memset(&vdata->gsJpuDecInit_Info, 0x00,
			sizeof(JDEC_INIT_t));
		(void)memset(&vdata->gsJpuDecBuffer_Info, 0x00,
			sizeof(JPU_SET_BUFFER_t));
		(void)memset(&vdata->gsJpuDecInOut_Info, 0x00,
			sizeof(JPU_DECODE_t));
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		(void)memset(&vdata->gsV4kd2DecInit_Info, 0x00,
			sizeof(VPU_4K_D2_INIT_t));
		(void)memset(&vdata->gsV4kd2DecSeqHeader_Info, 0x00,
			sizeof(VPU_4K_D2_SEQ_HEADER_t));
		(void)memset(&vdata->gsV4kd2DecBuffer_Info, 0x00,
			sizeof(VPU_4K_D2_SET_BUFFER_t));
		(void)memset(&vdata->gsV4kd2DecInOut_Info, 0x00,
			sizeof(VPU_4K_D2_DECODE_t));
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		(void)memset(&vdata->gsHevcDecInit_Info, 0x00,
			sizeof(HEVC_INIT_t));
		(void)memset(&vdata->gsHevcDecSeqHeader_Info, 0x00,
			sizeof(HEVC_SEQ_HEADER_t));
		(void)memset(&vdata->gsHevcDecBuffer_Info, 0x00,
			sizeof(HEVC_SET_BUFFER_t));
		(void)memset(&vdata->gsHevcDecInOut_Info, 0x00,
			sizeof(HEVC_DECODE_t));
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		(void)memset(&vdata->gsVp9DecInit_Info, 0x00,
			sizeof(VP9_INIT_t));
		(void)memset(&vdata->gsVp9DecSeqHeader_Info, 0x00,
			sizeof(VP9_SEQ_HEADER_t));
		(void)memset(&vdata->gsVp9DecBuffer_Info, 0x00,
			sizeof(VP9_SET_BUFFER_t));
		(void)memset(&vdata->gsVp9DecInOut_Info, 0x00,
			sizeof(VP9_DECODE_t));
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		(void)memset(&vdata->gsVpuDecInit_Info, 0x00,
			sizeof(VDEC_INIT_t));
		(void)memset(&vdata->gsVpuDecSeqHeader_Info, 0x00,
			sizeof(VDEC_SEQ_HEADER_t));
		(void)memset(&vdata->gsVpuDecBuffer_Info, 0x00,
			sizeof(VDEC_SET_BUFFER_t));
		(void)memset(&vdata->gsVpuDecInOut_Info, 0x00,
			sizeof(VDEC_DECODE_t));
	}
#endif
}

static int vdec_proc_exit(struct vpu_decoder_data *vdata,
					void *arg, bool fromKernel)
{
	void *pArgs = NULL;

	V_DBG(VPU_DBG_CLOSE,
		"%s :: vdec process exit!!", vdata->misc->name);

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsJpuDecInOut_Info, arg,
				sizeof(JPU_DECODE_t));
		} else {
			if (copy_from_user(&vdata->gsJpuDecInOut_Info, arg,
				sizeof(JPU_DECODE_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsJpuDecInOut_Info;
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsV4kd2DecInOut_Info, arg,
				sizeof(VPU_4K_D2_DECODE_t));
		} else {
			if (copy_from_user(&vdata->gsV4kd2DecInOut_Info, arg,
				sizeof(VPU_4K_D2_DECODE_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsV4kd2DecInOut_Info;
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsHevcDecInOut_Info, arg,
				sizeof(HEVC_DECODE_t));
		} else {
			if (copy_from_user(&vdata->gsHevcDecInOut_Info, arg,
				sizeof(HEVC_DECODE_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsHevcDecInOut_Info;
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsVp9DecInOut_Info, arg,
				sizeof(VP9_DECODE_t));
		} else {
			if (copy_from_user(&vdata->gsVp9DecInOut_Info, arg,
				sizeof(VP9_DECODE_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsVp9DecInOut_Info;
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsVpuDecInOut_Info, arg,
				sizeof(VDEC_DECODE_t));
		} else {
			if (copy_from_user(&vdata->gsVpuDecInOut_Info, arg,
				sizeof(VDEC_DECODE_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsVpuDecInOut_Info;
	}
#endif

	vdec_inter_add_list(vdata, VPU_DEC_CLOSE, pArgs);

	return 0;
}

static int vdec_proc_seq_header(struct vpu_decoder_data *vdata, void *arg,
				bool fromKernel)
{
	int ret = 0;
	void *pArgs = NULL;
	size_t copySize = 0;

	V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
		"%s :: enter!!", vdata->misc->name);

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
#if defined(JPU_C6)
		pArgs = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsJpuDecInOut_Info :
			(void *)&vdata->gsJpuDecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(JPU_DECODE_t) : sizeof(JDEC_SEQ_HEADER_t);
#else
		err_dec("%s :: jpu not support this !! ", vdata->misc->name);
		ret = -0x999;
#endif
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		pArgs = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsV4kd2DecInOut_Info :
			(void *)&vdata->gsV4kd2DecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VPU_4K_D2_DECODE_t) :
			sizeof(VPU_4K_D2_SEQ_HEADER_t);
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		pArgs = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsHevcDecInOut_Info :
			(void *)&vdata->gsHevcDecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(HEVC_DECODE_t) : sizeof(HEVC_SEQ_HEADER_t);
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		{
		pArgs = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsVp9DecInOut_Info :
			(void *)&vdata->gsVp9DecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VP9_DECODE_t) : sizeof(VP9_SEQ_HEADER_t);
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		pArgs = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsVpuDecInOut_Info :
			(void *)&vdata->gsVpuDecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VDEC_DECODE_t) : sizeof(VDEC_SEQ_HEADER_t);
		if (vdata->gsIsDiminishedCopy) {
			V_DBG(VPU_DBG_IO_FB_INFO,
				"[DiminishedCopy] %d x %d",
				vdata->gsVpuDecInOut_Info.gsVpuDecInitialInfo
					.m_iPicWidth,
				vdata->gsVpuDecInOut_Info.gsVpuDecInitialInfo
					.m_iPicHeight);
		}
	}
#endif

	if((pArgs != NULL) && (copySize > 0))
	{
		if (fromKernel) {
			(void)memcpy(pArgs, arg, copySize);
		} else {
			if (copy_from_user(pArgs, arg, copySize) != 0U) {
				return -EFAULT;
			}
		}
	}
	else
	{
		ret = (int)-EFAULT;
	}

	if(ret == 0)
	{
		vdec_inter_add_list(vdata, VPU_DEC_SEQ_HEADER, pArgs);
	}

	return ret;
}

static int vdec_proc_reg_framebuffer(struct vpu_decoder_data *vdata,
					void *arg, bool fromKernel)
{
	void *pArgs = NULL;

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsJpuDecBuffer_Info, arg,
				sizeof(JPU_SET_BUFFER_t));
		} else {
			if (copy_from_user(&vdata->gsJpuDecBuffer_Info, arg,
				sizeof(JPU_SET_BUFFER_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsJpuDecBuffer_Info;
		V_DBG(VPU_DBG_BUF_STATUS,
			"%s :: jpu_proc_reg_framebuffer :: phy = 0x%x, virt = 0x%x, cnt = 0x%x !!",
			vdata->misc->name,
			vdata->gsJpuDecBuffer_Info.gsJpuDecBuffer
				.m_FrameBufferStartAddr[0],
			vdata->gsJpuDecBuffer_Info.gsJpuDecBuffer
				.m_FrameBufferStartAddr[1],
			vdata->gsJpuDecBuffer_Info.gsJpuDecBuffer
				.m_iFrameBufferCount);
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsV4kd2DecBuffer_Info, arg,
				sizeof(VPU_4K_D2_SET_BUFFER_t));
		} else {
			if (copy_from_user(&vdata->gsV4kd2DecBuffer_Info, arg,
				sizeof(VPU_4K_D2_SET_BUFFER_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsV4kd2DecBuffer_Info;
		V_DBG(VPU_DBG_BUF_STATUS,
			"%s :: vpu-4k-d2_proc_reg_framebuffer :: phy = 0x%x, virt = 0x%x, cnt = 0x%x !!",
			vdata->misc->name,
			vdata->gsV4kd2DecBuffer_Info.gsV4kd2DecBuffer
				.m_FrameBufferStartAddr[0],
			vdata->gsV4kd2DecBuffer_Info.gsV4kd2DecBuffer
				.m_FrameBufferStartAddr[1],
			vdata->gsV4kd2DecBuffer_Info.gsV4kd2DecBuffer
				.m_iFrameBufferCount);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsHevcDecBuffer_Info, arg,
				sizeof(HEVC_SET_BUFFER_t));
		} else {
			if (copy_from_user(&vdata->gsHevcDecBuffer_Info, arg,
				sizeof(HEVC_SET_BUFFER_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsHevcDecBuffer_Info;
		V_DBG(VPU_DBG_BUF_STATUS,
			"%s :: hevc_proc_reg_framebuffer :: phy = 0x%x, virt = 0x%x, cnt = 0x%x !!",
			vdata->misc->name,
			vdata->gsHevcDecBuffer_Info.gsHevcDecBuffer
				.m_FrameBufferStartAddr[0],
			vdata->gsHevcDecBuffer_Info.gsHevcDecBuffer
				.m_FrameBufferStartAddr[1],
			vdata->gsHevcDecBuffer_Info.gsHevcDecBuffer
				.m_iFrameBufferCount);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsVp9DecBuffer_Info, arg,
				sizeof(VP9_SET_BUFFER_t));
		} else {
			if (copy_from_user(&vdata->gsVp9DecBuffer_Info, arg,
				sizeof(VP9_SET_BUFFER_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsVp9DecBuffer_Info;
		V_DBG(VPU_DBG_BUF_STATUS,
			"%s :: vp9_proc_reg_framebuffer :: phy = 0x%x, virt = 0x%x, cnt = 0x%x !!",
			vdata->misc->name,
			vdata->gsVp9DecBuffer_Info.gsVp9DecBuffer
				.m_FrameBufferStartAddr[0],
			vdata->gsVp9DecBuffer_Info.gsVp9DecBuffer
				.m_FrameBufferStartAddr[1],
			vdata->gsVp9DecBuffer_Info.gsVp9DecBuffer
				.m_iFrameBufferCount);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsVpuDecBuffer_Info, arg,
				sizeof(VDEC_SET_BUFFER_t));
		} else {
			if (copy_from_user(&vdata->gsVpuDecBuffer_Info, arg,
				sizeof(VDEC_SET_BUFFER_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsVpuDecBuffer_Info;
		V_DBG(VPU_DBG_BUF_STATUS,
			"%s :: _vdec proc reg framebuffer :: phy = 0x%x, virt = 0x%x, cnt = 0x%x!!",
			vdata->misc->name,
			vdata->gsVpuDecBuffer_Info.gsVpuDecBuffer
				.m_FrameBufferStartAddr[0],
			vdata->gsVpuDecBuffer_Info.gsVpuDecBuffer
				.m_FrameBufferStartAddr[1],
			vdata->gsVpuDecBuffer_Info.gsVpuDecBuffer
				.m_iFrameBufferCount);
	}
#endif

	if (pArgs != NULL) {
		vdec_inter_add_list(vdata, VPU_DEC_REG_FRAME_BUFFER, pArgs);
	}

	return 0;
}

static int vdec_proc_decode(struct vpu_decoder_data *vdata, void *arg,
			bool fromKernel)
{
	void *pArgs = NULL;

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsJpuDecInOut_Info, arg,
				sizeof(JPU_DECODE_t));
		} else {
			if (copy_from_user(&vdata->gsJpuDecInOut_Info, arg,
				sizeof(JPU_DECODE_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsJpuDecInOut_Info;
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s ::_jpu_proc_decode In !! handle = 0x%x, in_stream_size = 0x%x",
			vdata->misc->name,
			vdata->gsJpuDecInit_Info.gsJpuDecHandle,
			vdata->gsJpuDecInOut_Info.gsJpuDecInput
				.m_iBitstreamDataSize);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsV4kd2DecInOut_Info, arg,
				sizeof(VPU_4K_D2_DECODE_t));
		} else {
			if (copy_from_user(&vdata->gsV4kd2DecInOut_Info, arg,
				sizeof(VPU_4K_D2_DECODE_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsV4kd2DecInOut_Info;
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s:_vpu-4k-d2_proc_decode In !! handle = 0x%x, in_stream_size = 0x%x",
			vdata->misc->name,
			vdata->gsV4kd2DecInit_Info.gsV4kd2DecHandle,
			vdata->gsV4kd2DecInOut_Info.gsV4kd2DecInput
				.m_iBitstreamDataSize);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsHevcDecInOut_Info, arg,
				sizeof(HEVC_DECODE_t));
		} else {
			if (copy_from_user(&vdata->gsHevcDecInOut_Info, arg,
				sizeof(HEVC_DECODE_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsHevcDecInOut_Info;
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s: _hevc_proc_decode In !! handle = 0x%x, in_stream_size = 0x%x",
			vdata->misc->name,
			vdata->gsHevcDecInit_Info.gsHevcDecHandle,
			vdata->gsHevcDecInOut_Info.gsHevcDecInput
				.m_iBitstreamDataSize);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsVp9DecInOut_Info, arg,
				sizeof(VP9_DECODE_t));
		} else {
			if (copy_from_user(&vdata->gsVp9DecInOut_Info, arg,
				sizeof(VP9_DECODE_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsVp9DecInOut_Info;
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s ::_vp9_proc_decode In !! handle = 0x%x, in_stream_size = 0x%x",
			vdata->misc->name,
			vdata->gsVp9DecInit_Info.gsVp9DecHandle,
			vdata->gsVp9DecInOut_Info.gsVp9DecInput
				.m_iBitstreamDataSize);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsVpuDecInOut_Info, arg,
				sizeof(VDEC_DECODE_t));
		} else {
			if (copy_from_user(&vdata->gsVpuDecInOut_Info, arg,
				sizeof(VDEC_DECODE_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsVpuDecInOut_Info;
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s :: enter !! handle = 0x%x, in_stream_size = 0x%x",
			vdata->misc->name,
			vdata->gsVpuDecInit_Info.gsVpuDecHandle,
			vdata->gsVpuDecInOut_Info.gsVpuDecInput
				.m_iBitstreamDataSize);
	}
#endif

	if (pArgs != NULL) {
		vdec_inter_add_list(vdata, VPU_DEC_DECODE, pArgs);
	}

	return 0;
}

static int vdec_get_decode_output(struct vpu_decoder_data *vdata, void *arg,
				bool fromKernel)
{
	void *pArgs = NULL;
	int ret = 0;

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsV4kd2DecInOut_Info, arg,
				sizeof(VPU_4K_D2_DECODE_t));
		} else {
			if (copy_from_user(&vdata->gsV4kd2DecInOut_Info, arg,
				sizeof(VPU_4K_D2_DECODE_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsV4kd2DecInOut_Info;
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s ::_vpu-4k-d2_get_decode_output In !! handle = 0x%x, in_stream_size = 0x%x ",
			vdata->misc->name,
			vdata->gsV4kd2DecInit_Info.gsV4kd2DecHandle,
			vdata->gsV4kd2DecInOut_Info.gsV4kd2DecInput
				.m_iBitstreamDataSize);
	} else
#else
	VPU_UNUSED_PARAMETER(arg);
	VPU_UNUSED_PARAMETER(pArgs);
	VPU_UNUSED_PARAMETER(fromKernel);
#endif
	{
		V_DBG(VPU_DBG_ERROR,
			"%s :: vdec get decode output In !! error command",
			vdata->misc->name);
		ret = (int)-EFAULT;
	}

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (ret == 0) {
		vdec_inter_add_list(vdata, VPU_DEC_GET_OUTPUT_INFO, pArgs);
	}
#endif

	return ret;
}

static int vdec_proc_clear_bufferflag(struct vpu_decoder_data *vdata,
				void *arg, bool fromKernel)
{
	void *pArgs = NULL;

	if (fromKernel) {
		(void)memcpy(&vdata->gsDecClearBuffer_index, arg,
			sizeof(unsigned int));
	} else {
		if (copy_from_user(&vdata->gsDecClearBuffer_index, arg,
			sizeof(unsigned int)) != 0U) {
			return -EFAULT;
		}
	}
	pArgs = (void *)&vdata->gsDecClearBuffer_index;

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		V_DBG(VPU_DBG_CMD, "%s ::_jpu_proc_clear_bufferflag : %d !!",
			vdata->misc->name, vdata->gsDecClearBuffer_index);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		V_DBG(VPU_DBG_CMD,
			"%s ::_vpu-4k-d2_proc_clear_bufferflag : %d !!",
			vdata->misc->name, vdata->gsDecClearBuffer_index);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		V_DBG(VPU_DBG_CMD, "%s ::_hevc_proc_clear_bufferflag : %d !!",
			vdata->misc->name, vdata->gsDecClearBuffer_index);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		V_DBG(VPU_DBG_CMD, "%s ::_vp9_proc_clear_bufferflag : %d !!",
			vdata->misc->name, vdata->gsDecClearBuffer_index);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		V_DBG(VPU_DBG_CMD, "%s ::_vdec proc clear bufferflag : %d !!",
			vdata->misc->name, vdata->gsDecClearBuffer_index);
	}
#endif

	vdec_inter_add_list(vdata, VPU_DEC_BUF_FLAG_CLEAR, pArgs);

	return 0;
}

static int vdec_proc_flush(struct vpu_decoder_data *vdata,
					void *arg, bool fromKernel)
{
	void *pArgs = NULL;

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsJpuDecInOut_Info, arg,
			sizeof(JPU_DECODE_t));
		} else {
			if (copy_from_user(&vdata->gsJpuDecInOut_Info, arg,
				sizeof(JPU_DECODE_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsJpuDecInOut_Info;
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s ::jpu_proc_flush In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsJpuDecInit_Info.gsJpuDecHandle);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsV4kd2DecInOut_Info, arg,
				sizeof(VPU_4K_D2_DECODE_t));
		} else {
			if (copy_from_user(&vdata->gsV4kd2DecInOut_Info, arg,
				sizeof(VPU_4K_D2_DECODE_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsV4kd2DecInOut_Info;
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s ::vpu-4k-d2_proc_flush In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsV4kd2DecInit_Info.gsV4kd2DecHandle);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsHevcDecInOut_Info, arg,
				sizeof(HEVC_DECODE_t));
		} else {
			if (copy_from_user(&vdata->gsHevcDecInOut_Info, arg,
				sizeof(HEVC_DECODE_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsHevcDecInOut_Info;
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s ::hevc_proc_flush In !! handle = 0x%x ",
			vdata->misc->name,
			vdata->gsHevcDecInit_Info.gsHevcDecHandle);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsVp9DecInOut_Info, arg,
				sizeof(VP9_DECODE_t));
		} else {
			if (copy_from_user(&vdata->gsVp9DecInOut_Info, arg,
				sizeof(VP9_DECODE_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsVp9DecInOut_Info;
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s ::vp9_proc_flush In !! handle = 0x%x ",
			vdata->misc->name,
			vdata->gsVp9DecInit_Info.gsVp9DecHandle);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsVpuDecInOut_Info, arg,
				sizeof(VDEC_DECODE_t));
		} else {
			if (copy_from_user(&vdata->gsVpuDecInOut_Info, arg,
				sizeof(VDEC_DECODE_t)) != 0U) {
				return -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsVpuDecInOut_Info;
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s :: enter !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsVpuDecInit_Info.gsVpuDecHandle);
	}
#endif

	vdec_inter_add_list(vdata, VPU_DEC_FLUSH_OUTPUT, pArgs);

	return 0;
}

static int vdec_result_general(struct vpu_decoder_data *vdata, void *arg,
			bool fromKernel)
{
	int ret = 0;

	if (fromKernel) {
		(void)memcpy(arg, &vdata->gsCommDecResult, sizeof(int));
	} else {
		if (copy_to_user(arg, &vdata->gsCommDecResult, sizeof(int)) != 0U) {
			ret = (int)-EFAULT;
		}
	}
	return ret;
}

static int vdec_result_init(struct vpu_decoder_data *vdata, void *arg,
			bool fromKernel)
{
	int ret = 0;

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		vdata->gsJpuDecInit_Info.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsJpuDecInit_Info,
				sizeof(JDEC_INIT_t));
		} else {
			if (copy_to_user(arg, &vdata->gsJpuDecInit_Info,
				sizeof(JDEC_INIT_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		vdata->gsV4kd2DecInit_Info.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsV4kd2DecInit_Info,
				sizeof(VPU_4K_D2_INIT_t));
		} else {
			if (copy_to_user(arg, &vdata->gsV4kd2DecInit_Info,
				sizeof(VPU_4K_D2_INIT_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		vdata->gsHevcDecInit_Info.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsHevcDecInit_Info,
				sizeof(HEVC_INIT_t));
		} else {
			if (copy_to_user(arg, &vdata->gsHevcDecInit_Info,
				sizeof(HEVC_INIT_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		vdata->gsVp9DecInit_Info.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsVp9DecInit_Info,
				sizeof(VP9_INIT_t));
		} else {
			if (copy_to_user(arg, &vdata->gsVp9DecInit_Info,
				sizeof(VP9_INIT_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		vdata->gsVpuDecInit_Info.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsVpuDecInit_Info,
				sizeof(VDEC_INIT_t));
		} else {
			if (copy_to_user(arg, &vdata->gsVpuDecInit_Info,
				sizeof(VDEC_INIT_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif

	return ret;
}

static int vdec_result_seq_header(struct vpu_decoder_data *vdata, void *arg,
				bool fromKernel)
{
	int ret = 0;
	void *src = NULL;
	size_t copySize = 0;

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
#if defined(JPU_C6)
		src = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsJpuDecInOut_Info :
			(void *)&vdata->gsJpuDecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(JPU_DECODE_t) : sizeof(JDEC_SEQ_HEADER_t);

		if (vdata->gsIsDiminishedCopy) {
			vdata->gsJpuDecInOut_Info.result
				= vdata->gsCommDecResult;
		} else {
			vdata->gsJpuDecSeqHeader_Info.result
				= vdata->gsCommDecResult;
		}
#else
		err_dec("%s ::jpu not support this !!", vdata->misc->name);
		ret = -0x999;
#endif
	}
#endif

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		src = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsV4kd2DecInOut_Info :
			(void *)&vdata->gsV4kd2DecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VPU_4K_D2_DECODE_t) :
			sizeof(VPU_4K_D2_SEQ_HEADER_t);

		if (vdata->gsIsDiminishedCopy) {
			vdata->gsV4kd2DecInOut_Info.result
				= vdata->gsCommDecResult;
		} else {
			vdata->gsV4kd2DecSeqHeader_Info.result
				= vdata->gsCommDecResult;
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		src = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsHevcDecInOut_Info :
			(void *)&vdata->gsHevcDecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(HEVC_DECODE_t) :
			sizeof(HEVC_SEQ_HEADER_t);

		if (vdata->gsIsDiminishedCopy) {
			vdata->gsHevcDecInOut_Info.result
				= vdata->gsCommDecResult;
		} else {
			vdata->gsHevcDecSeqHeader_Info.result
				= vdata->gsCommDecResult;
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		src = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsVp9DecInOut_Info :
			(void *)&vdata->gsVp9DecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VP9_DECODE_t) :
			sizeof(VP9_SEQ_HEADER_t);

		if (vdata->gsIsDiminishedCopy) {
			vdata->gsVp9DecInOut_Info.result
					= vdata->gsCommDecResult;
		} else {
			vdata->gsVp9DecSeqHeader_Info.result
					= vdata->gsCommDecResult;
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		src = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsVpuDecInOut_Info :
			(void *)&vdata->gsVpuDecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VDEC_DECODE_t) :
			sizeof(VDEC_SEQ_HEADER_t);

		if (vdata->gsIsDiminishedCopy) {
			vdata->gsVpuDecInOut_Info.result
				= vdata->gsCommDecResult;
		} else {
			vdata->gsVpuDecSeqHeader_Info.result
				= vdata->gsCommDecResult;
		}

		V_DBG(VPU_DBG_IO_FB_INFO,
			"vdata->gsIsDiminishedCopy %d",
			vdata->gsIsDiminishedCopy);

		if (vdata->gsIsDiminishedCopy) {
			V_DBG(VPU_DBG_IO_FB_INFO,
				"%d x %d",
				vdata->gsVpuDecInOut_Info
					.gsVpuDecInitialInfo.m_iPicWidth,
				vdata->gsVpuDecInOut_Info
					.gsVpuDecInitialInfo.m_iPicHeight);
		} else {
			V_DBG(VPU_DBG_IO_FB_INFO,
				"%d x %d",
				vdata->gsVpuDecSeqHeader_Info
					.gsVpuDecInitialInfo.m_iPicWidth,
				vdata->gsVpuDecSeqHeader_Info
					.gsVpuDecInitialInfo.m_iPicHeight);
		}
	}
#endif

	if((src != NULL) && (copySize > 0))
	{
		if (fromKernel) {
			(void)memcpy(arg, src, copySize);
		} else {
			if (copy_to_user(arg, src, copySize) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
	else
	{
		ret = (int)-EFAULT;
	}

	return ret;
}

static int vdec_result_decode(struct vpu_decoder_data *vdata, void *arg,
			bool fromKernel)
{
	int ret = 0;

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		vdata->gsJpuDecInOut_Info.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsJpuDecInOut_Info,
				sizeof(JPU_DECODE_t));
		} else {

			if (copy_to_user(arg, &vdata->gsJpuDecInOut_Info,
				sizeof(JPU_DECODE_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		vdata->gsV4kd2DecInOut_Info.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsV4kd2DecInOut_Info,
				sizeof(VPU_4K_D2_DECODE_t));
		} else {
			if (copy_to_user(arg, &vdata->gsV4kd2DecInOut_Info,
				sizeof(VPU_4K_D2_DECODE_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		vdata->gsHevcDecInOut_Info.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsHevcDecInOut_Info,
				sizeof(HEVC_DECODE_t));
		} else {
			if (copy_to_user(arg, &vdata->gsHevcDecInOut_Info,
				sizeof(HEVC_DECODE_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		vdata->gsVp9DecInOut_Info.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsVp9DecInOut_Info,
				sizeof(VP9_DECODE_t));
		} else {
			if (copy_to_user(arg, &vdata->gsVp9DecInOut_Info,
				sizeof(VP9_DECODE_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		vdata->gsVpuDecInOut_Info.result = vdata->gsCommDecResult;
		if (vdata->gsCommDecResult == RETCODE_CODEC_EXIT) {
			V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
				" [%s][Codec:%d][V_DEC_DECODE_RESULT] RETCODE_CODEC_EXIT",
				vpu_vputype_to_string((vputype)vdata->gsDecType), vdata->gsCodecType);
		}

		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsVpuDecInOut_Info,
				sizeof(VDEC_DECODE_t));
		} else {
			if (copy_to_user(arg, &vdata->gsVpuDecInOut_Info,
				sizeof(VDEC_DECODE_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif

	return ret;
}

static int vdec_result_flush(struct vpu_decoder_data *vdata, void *arg,
			bool fromKernel)
{
	int ret = 0;

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		vdata->gsJpuDecInOut_Info.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsJpuDecInOut_Info,
				sizeof(JPU_DECODE_t));
		} else {
			if (copy_to_user(arg, &vdata->gsJpuDecInOut_Info,
				sizeof(JPU_DECODE_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		vdata->gsV4kd2DecInOut_Info.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsV4kd2DecInOut_Info,
				sizeof(VPU_4K_D2_DECODE_t));
		} else {
			if (copy_to_user(arg, &vdata->gsV4kd2DecInOut_Info,
				sizeof(VPU_4K_D2_DECODE_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		vdata->gsHevcDecInOut_Info.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsHevcDecInOut_Info,
				sizeof(HEVC_DECODE_t));
		} else {
			if (copy_to_user(arg, &vdata->gsHevcDecInOut_Info,
				sizeof(HEVC_DECODE_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		vdata->gsVp9DecInOut_Info.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsVp9DecInOut_Info,
				sizeof(VP9_DECODE_t));
		} else {
			if (copy_to_user(arg, &vdata->gsVp9DecInOut_Info,
				sizeof(VP9_DECODE_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		vdata->gsVpuDecInOut_Info.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsVpuDecInOut_Info,
				sizeof(VDEC_DECODE_t));
		} else {
			if (copy_to_user(arg, &vdata->gsVpuDecInOut_Info,
				sizeof(VDEC_DECODE_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif

	return ret;
}

static int vdec_proc_swreset(struct vpu_decoder_data *vdata, bool fromKernel)
{
	VPU_UNUSED_PARAMETER(fromKernel);

	vdec_inter_add_list(vdata, VPU_DEC_SWRESET, (void *)NULL);

	return 0;
}

static int vdec_proc_buf_status(struct vpu_decoder_data *vdata, void *arg,
			bool fromKernel)
{
	int ret = 0;
	void *pArgs = NULL;

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		err_dec("%s ::jpu not support this !!", vdata->misc->name);
		ret = -0x999;
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsV4kd2DecBufStatus, arg,
				sizeof(VPU_4K_D2_RINGBUF_GETINFO_t));
		} else {
			if (copy_from_user(&vdata->gsV4kd2DecBufStatus, arg,
				sizeof(VPU_4K_D2_RINGBUF_GETINFO_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsV4kd2DecBufStatus;
		V_DBG(VPU_DBG_BUF_STATUS,
			"%s ::vpu-4k-d2_proc_buf_status In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsV4kd2DecInit_Info.gsV4kd2DecHandle);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsHevcDecBufStatus, arg,
				sizeof(HEVC_RINGBUF_GETINFO_t));
		} else {
			if (copy_from_user(&vdata->gsHevcDecBufStatus, arg,
				sizeof(HEVC_RINGBUF_GETINFO_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsHevcDecBufStatus;
		V_DBG(VPU_DBG_BUF_STATUS,
			"%s ::hevc_proc_buf_status In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsHevcDecInit_Info.gsHevcDecHandle);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsVp9DecBufStatus, arg,
				sizeof(VP9_RINGBUF_GETINFO_t));
		} else {
			if (copy_from_user(&vdata->gsVp9DecBufStatus, arg,
				sizeof(VP9_RINGBUF_GETINFO_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsVp9DecBufStatus;
		V_DBG(VPU_DBG_BUF_STATUS,
			"%s: vp9_proc_buf_status In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsVp9DecInit_Info.gsVp9DecHandle);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsVpuDecBufStatus, arg,
				sizeof(VDEC_RINGBUF_GETINFO_t));
		} else {
			if (copy_from_user(&vdata->gsVpuDecBufStatus, arg,
				sizeof(VDEC_RINGBUF_GETINFO_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsVpuDecBufStatus;
		V_DBG(VPU_DBG_BUF_STATUS,
			"%s :: vdec_proc_buf_status In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsVpuDecInit_Info.gsVpuDecHandle);
	}
#endif

	if (ret == 0) {
		vdec_inter_add_list(vdata, GET_RING_BUFFER_STATUS, pArgs);
	}

	return ret;
}

static int vdec_proc_buf_fill(struct vpu_decoder_data *vdata, void *arg,
			bool fromKernel)
{
	int ret = 0;
	void *pArgs = NULL;

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		err_dec("%s: Not supported by JPU !!", vdata->misc->name);
		ret = -0x999;
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsV4kd2DecBufFill, arg,
				sizeof(VPU_4K_D2_RINGBUF_SETBUF_t));
		} else {
			if (copy_from_user(&vdata->gsV4kd2DecBufFill, arg,
				sizeof(VPU_4K_D2_RINGBUF_SETBUF_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsV4kd2DecBufFill;
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s: vpu-4k-d2_proc_buf_fill In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsV4kd2DecInit_Info.gsV4kd2DecHandle);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsHevcDecBufFill, arg,
				sizeof(HEVC_RINGBUF_SETBUF_t));
		} else {
			if (copy_from_user(&vdata->gsHevcDecBufFill, arg,
				sizeof(HEVC_RINGBUF_SETBUF_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsHevcDecBufFill;
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s: hevc_proc_buf_fill In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsHevcDecInit_Info.gsHevcDecHandle);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		err_dec("%s: Not supported by G2V2 VP9 !!", vdata->misc->name);
		return -0x999;
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsVpuDecBufFill, arg,
				sizeof(VDEC_RINGBUF_SETBUF_t));
		} else {
			if (copy_from_user(&vdata->gsVpuDecBufFill, arg,
				sizeof(VDEC_RINGBUF_SETBUF_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsVpuDecBufFill;
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s: enter !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsVpuDecInit_Info.gsVpuDecHandle);
	}
#endif

	if (ret == 0) {
		vdec_inter_add_list(vdata, FILL_RING_BUFFER_AUTO, pArgs);
	}

	return ret;
}

static int vdec_proc_update_wp(struct vpu_decoder_data *vdata, void *arg,
				bool fromKernel)
{
	int ret = 0;
	void *pArgs = NULL;

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		err_dec("%s ::jpu not support this !!", vdata->misc->name);
		ret = -0x999;
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsV4kd2DecUpdateWP, arg,
				sizeof(VPU_4K_D2_RINGBUF_SETBUF_PTRONLY_t));
		} else {
			if (copy_from_user(&vdata->gsV4kd2DecUpdateWP, arg,
				sizeof(VPU_4K_D2_RINGBUF_SETBUF_PTRONLY_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsV4kd2DecUpdateWP;
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s ::vpu-4k-d2_proc_buf_fill In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsV4kd2DecInit_Info.gsV4kd2DecHandle);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsHevcDecUpdateWP, arg,
				sizeof(HEVC_RINGBUF_SETBUF_PTRONLY_t));
		} else {
			if (copy_from_user(&vdata->gsHevcDecUpdateWP, arg,
				sizeof(HEVC_RINGBUF_SETBUF_PTRONLY_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsHevcDecUpdateWP;
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s ::hevc_proc_buf_fill In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsHevcDecInit_Info.gsHevcDecHandle);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsVp9DecUpdateWP, arg,
				sizeof(VP9_RINGBUF_SETBUF_PTRONLY_t));
		} else {
			if (copy_from_user(&vdata->gsVp9DecUpdateWP, arg,
				sizeof(VP9_RINGBUF_SETBUF_PTRONLY_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsVp9DecUpdateWP;
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s ::vp9_proc_buf_fill In !! handle = 0x%x",
			vdata->misc->name,
			vdata->gsVp9DecInit_Info.gsVp9DecHandle);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsVpuDecUpdateWP, arg,
				sizeof(VDEC_RINGBUF_SETBUF_PTRONLY_t));
		} else {
			if (copy_from_user(&vdata->gsVpuDecUpdateWP, arg,
				sizeof(VDEC_RINGBUF_SETBUF_PTRONLY_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsVpuDecUpdateWP;
		V_DBG(VPU_DBG_IO_FB_INFO,
			"%s: vdec_proc_buf_fill In !! handle = 0x%x",
			vdata->gsVpuDecInit_Info.gsVpuDecHandle);
	}
#endif

	if (ret == 0) {
		vdec_inter_add_list(vdata, VPU_UPDATE_WRITE_BUFFER_PTR, pArgs);
	}

	return ret;
}

static int vdec_proc_seq_header_ring(struct vpu_decoder_data *vdata,
					void *arg, bool fromKernel)
{
	int ret = 0;
	void *pArgs = NULL;
	size_t copySize = 0;

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		err_dec("%s ::jpu not support this !!", vdata->misc->name);
		ret = -0x999;
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		pArgs = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsV4kd2DecInOut_Info :
			(void *)&vdata->gsV4kd2DecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VPU_4K_D2_DECODE_t) :
			sizeof(VPU_4K_D2_SEQ_HEADER_t);
		V_DBG(VPU_DBG_IO_FB_INFO,
				"%s: vpu-4k-d2_proc_buf_fill In !! handle = 0x%x",
				vdata->misc->name,
				vdata->gsV4kd2DecInit_Info.gsV4kd2DecHandle);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		pArgs = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsHevcDecInOut_Info :
			(void *)&vdata->gsHevcDecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(HEVC_DECODE_t) : sizeof(HEVC_SEQ_HEADER_t);
		V_DBG(VPU_DBG_IO_FB_INFO,
				"%s: hevc_proc_buf_fill In !! handle = 0x%x",
				vdata->misc->name,
				vdata->gsHevcDecInit_Info.gsHevcDecHandle);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		pArgs = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsVp9DecInOut_Info :
			(void *)&vdata->gsVp9DecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VP9_DECODE_t) : sizeof(VP9_SEQ_HEADER_t);
		V_DBG(VPU_DBG_IO_FB_INFO,
				"%s ::vp9_proc_buf_fill In !! handle = 0x%x",
				vdata->misc->name,
				vdata->gsVp9DecInit_Info.gsVp9DecHandle);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		pArgs = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsVpuDecInOut_Info :
			(void *)&vdata->gsVpuDecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VDEC_DECODE_t) : sizeof(VDEC_SEQ_HEADER_t);
		V_DBG(VPU_DBG_IO_FB_INFO,
				"%s: vdec_proc_buf_fill In !! handle = 0x%x",
				vdata->misc->name,
				vdata->gsVpuDecInit_Info.gsVpuDecHandle);
	}
#endif

	if((pArgs != NULL) && (copySize > 0))
	{
		if (fromKernel) {
			(void)memcpy(pArgs, arg, copySize);
		} else {
			if (copy_from_user(pArgs, arg, copySize) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
	else
	{
		ret = (int)-EFAULT;
	}

	if (ret == 0) {
		vdec_inter_add_list(vdata,
			GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY,
			pArgs);
	}

	return ret;
}

static int vdec_proc_get_version(struct vpu_decoder_data *vdata, void *arg,
				bool fromKernel)
{
	int ret = 0;
	void *pArgs = NULL;

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsJpuDecVersion, arg,
				sizeof(JPU_GET_VERSION_t));
		} else {
			if (copy_from_user(&vdata->gsJpuDecVersion, arg,
				sizeof(JPU_GET_VERSION_t)) != 0U) {
				ret =  -EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsJpuDecVersion;
		V_DBG(VPU_DBG_INFO,
			"%s: jpu_proc_get_version (handle: 0x%x)",
			vdata->misc->name,
			vdata->gsJpuDecInit_Info.gsJpuDecHandle);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsV4kd2DecVersion, arg,
				sizeof(VPU_4K_D2_GET_VERSION_t));
		} else {
			if (copy_from_user(&vdata->gsV4kd2DecVersion,
				arg, sizeof(VPU_4K_D2_GET_VERSION_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsV4kd2DecVersion;
		V_DBG(VPU_DBG_INFO,
			"%s: vpu-4k-d2_proc_get_version (handle: 0x%x)",
			vdata->misc->name,
			vdata->gsV4kd2DecInit_Info.gsV4kd2DecHandle);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsHevcDecVersion, arg,
				sizeof(HEVC_GET_VERSION_t));
		} else {
			if (copy_from_user(&vdata->gsHevcDecVersion, arg,
				sizeof(HEVC_GET_VERSION_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsHevcDecVersion;
		V_DBG(VPU_DBG_INFO,
			"%s: hevc_proc_get_version(handle: 0x%x)",
			vdata->misc->name,
			vdata->gsHevcDecInit_Info.gsHevcDecHandle);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsVp9DecVersion, arg,
				sizeof(VP9_GET_VERSION_t));
		} else {
			if (copy_from_user(&vdata->gsVp9DecVersion, arg,
				sizeof(VP9_GET_VERSION_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsVp9DecVersion;
		V_DBG(VPU_DBG_INFO,
			"%s: vp9_proc_get_version (handle: 0x%x)",
			vdata->misc->name,
			vdata->gsVp9DecInit_Info.gsVp9DecHandle);
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		if (fromKernel) {
			(void)memcpy(&vdata->gsVpuDecVersion, arg,
				sizeof(VDEC_GET_VERSION_t));
		} else {
			if (copy_from_user(&vdata->gsVpuDecVersion,
				arg, sizeof(VDEC_GET_VERSION_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
		pArgs = (void *)&vdata->gsVpuDecVersion;
		V_DBG(VPU_DBG_INFO,
			"%s: handle: 0x%x",
			vdata->misc->name,
			vdata->gsVpuDecInit_Info.gsVpuDecHandle);
	}
#endif

	if (ret == 0) {
		vdec_inter_add_list(vdata, VPU_CODEC_GET_VERSION, pArgs);
	}

	return ret;
}

static int vdec_result_buf_status(struct vpu_decoder_data *vdata, void *arg,
				bool fromKernel)
{
	int ret = 0;

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		err_dec("%s ::jpu not support this !!", vdata->misc->name);
		ret = -0x999;
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		vdata->gsV4kd2DecBufStatus.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsV4kd2DecBufStatus,
				sizeof(VPU_4K_D2_RINGBUF_GETINFO_t));
		} else {
			if (copy_to_user(arg, &vdata->gsV4kd2DecBufStatus,
				sizeof(VPU_4K_D2_RINGBUF_GETINFO_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		vdata->gsHevcDecBufStatus.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsHevcDecBufStatus,
				sizeof(HEVC_RINGBUF_GETINFO_t));
		} else {
			if (copy_to_user(arg, &vdata->gsHevcDecBufStatus,
				sizeof(HEVC_RINGBUF_GETINFO_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		vdata->gsVp9DecBufStatus.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsVp9DecBufStatus,
				sizeof(VP9_RINGBUF_GETINFO_t));
		} else {
			if (copy_to_user(arg, &vdata->gsVp9DecBufStatus,
				sizeof(VP9_RINGBUF_GETINFO_t)) != 0U) {
				return -EFAULT;
			}
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		vdata->gsVpuDecBufStatus.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsVpuDecBufStatus,
				sizeof(VDEC_RINGBUF_GETINFO_t));
		} else {
			if (copy_to_user(arg, &vdata->gsVpuDecBufStatus,
				sizeof(VDEC_RINGBUF_GETINFO_t)) != 0U) {
				ret =  -EFAULT;
			}
		}
	}
#endif

	return ret;
}

static int vdec_result_buf_fill(struct vpu_decoder_data *vdata, void *arg,
			bool fromKernel)
{
	int ret = 0;

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		err_dec("%s ::jpu not support this !!", vdata->misc->name);
		ret = -0x999;
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		vdata->gsV4kd2DecBufFill.result = vdata->gsCommDecResult;

		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsV4kd2DecBufFill,
				sizeof(VPU_4K_D2_RINGBUF_SETBUF_t));
		} else {
			if (copy_to_user(arg, &vdata->gsV4kd2DecBufFill,
				sizeof(VPU_4K_D2_RINGBUF_SETBUF_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		vdata->gsHevcDecBufFill.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsHevcDecBufFill,
				sizeof(HEVC_RINGBUF_SETBUF_t));
		} else {
			if (copy_to_user(arg, &vdata->gsHevcDecBufFill,
				sizeof(HEVC_RINGBUF_SETBUF_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		err_dec("%s: Not supported by G2V2 VP9 !!", vdata->misc->name);
		ret = -0x999;
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		vdata->gsVpuDecBufFill.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsVpuDecBufFill,
				sizeof(VDEC_RINGBUF_SETBUF_t));
		} else {
			if (copy_to_user(arg, &vdata->gsVpuDecBufFill,
				sizeof(VDEC_RINGBUF_SETBUF_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif

	return ret;
}

static int vdec_result_update_wp(struct vpu_decoder_data *vdata, void *arg,
				bool fromKernel)
{
	int ret = 0;

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		err_dec("%s ::jpu not support this !! ", vdata->misc->name);
		ret = -0x999;
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		vdata->gsV4kd2DecUpdateWP.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsV4kd2DecUpdateWP,
				sizeof(VPU_4K_D2_RINGBUF_SETBUF_PTRONLY_t));
		} else {
			if (copy_to_user(arg, &vdata->gsV4kd2DecUpdateWP,
				sizeof(VPU_4K_D2_RINGBUF_SETBUF_PTRONLY_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		vdata->gsHevcDecUpdateWP.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsHevcDecUpdateWP,
				sizeof(HEVC_RINGBUF_SETBUF_PTRONLY_t));
		} else {
			if (copy_to_user(arg, &vdata->gsHevcDecUpdateWP,
				sizeof(HEVC_RINGBUF_SETBUF_PTRONLY_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		vdata->gsVp9DecUpdateWP.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsVp9DecUpdateWP,
				sizeof(VP9_RINGBUF_SETBUF_PTRONLY_t));
		} else {
			if (copy_to_user(arg, &vdata->gsVp9DecUpdateWP,
				sizeof(VP9_RINGBUF_SETBUF_PTRONLY_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		vdata->gsVpuDecUpdateWP.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsVpuDecUpdateWP,
				sizeof(VDEC_RINGBUF_SETBUF_PTRONLY_t));
		} else {
			if (copy_to_user(arg, &vdata->gsVpuDecUpdateWP,
				sizeof(VDEC_RINGBUF_SETBUF_PTRONLY_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	}
#endif

	return ret;
}

static int vdec_result_seq_header_ring(struct vpu_decoder_data *vdata,
					void *arg, bool fromKernel)
{
	int ret = 0;
	void *src = NULL;
	size_t copySize = 0;

#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		err_dec("%s: Not supported by JPU!!", vdata->misc->name);
		ret = -0x999;
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		src = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsV4kd2DecInOut_Info :
			(void *)&vdata->gsV4kd2DecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VPU_4K_D2_DECODE_t) :
			sizeof(VPU_4K_D2_SEQ_HEADER_t);

		if (vdata->gsIsDiminishedCopy) {
			vdata->gsV4kd2DecInOut_Info.result
				= vdata->gsCommDecResult;
		} else {
			vdata->gsV4kd2DecSeqHeader_Info.result
				= vdata->gsCommDecResult;
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		src = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsHevcDecInOut_Info :
			(void *)&vdata->gsHevcDecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(HEVC_DECODE_t) :
			sizeof(HEVC_SEQ_HEADER_t);

		if (vdata->gsIsDiminishedCopy) {
			vdata->gsHevcDecInOut_Info.result
					= vdata->gsCommDecResult;
		} else {
			vdata->gsHevcDecSeqHeader_Info.result
					= vdata->gsCommDecResult;
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		err_dec("%s: Not supported by G2V2 VP9 !!", vdata->misc->name);
		ret = -0x999;
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		src = vdata->gsIsDiminishedCopy ?
			(void *)&vdata->gsVpuDecInOut_Info :
			(void *)&vdata->gsVpuDecSeqHeader_Info;
		copySize = vdata->gsIsDiminishedCopy ?
			sizeof(VDEC_DECODE_t) :
			sizeof(VDEC_SEQ_HEADER_t);

		if (vdata->gsIsDiminishedCopy) {
			vdata->gsVpuDecInOut_Info.result
					= vdata->gsCommDecResult;
		} else {
			vdata->gsVpuDecSeqHeader_Info.result
					= vdata->gsCommDecResult;
		}
	}
#endif

	if((src != NULL) && (copySize > 0)) {
		if (fromKernel) {
			(void)memcpy(arg, src, copySize);
		} else {
			if (copy_to_user(arg, src, copySize) != 0U) {
				ret = (int)-EFAULT;
			}
		}
	} else {
		ret = (int)-EFAULT;
	}

	return ret;
}

static int vdec_result_get_version(struct vpu_decoder_data *vdata, void *arg,
				bool fromKernel)
{
#ifdef CONFIG_SUPPORT_TCC_JPU
	if (vdata->gsCodecType == STD_MJPG) {
		vdata->gsJpuDecVersion.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsJpuDecVersion,
				sizeof(JPU_GET_VERSION_t));
		} else {
			if (copy_to_user(arg, &vdata->gsJpuDecVersion,
				sizeof(JPU_GET_VERSION_t)) != 0U) {
				return -EFAULT;
			}
		}

		if ((*(vdata->gsJpuDecVersion.pszVersion) == 0xFFU) ||
			(*(vdata->gsJpuDecVersion.pszBuildData) == 0xFFU)) {
			vdata->gsJpuDecVersion.result =
				RETCODE_INVALID_COMMAND;
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	if (vdata->gsCodecType == STD_HEVC
			|| vdata->gsCodecType == STD_VP9) {
		vdata->gsV4kd2DecVersion.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsV4kd2DecVersion,
				sizeof(VPU_4K_D2_GET_VERSION_t));
		} else {
			if (copy_to_user(arg, &vdata->gsV4kd2DecVersion,
				sizeof(VPU_4K_D2_GET_VERSION_t)) != 0U) {
				return -EFAULT;
			}
		}

		if ((*(vdata->gsV4kd2DecVersion.pszVersion) == -1) ||
			(*(vdata->gsV4kd2DecVersion.pszBuildData) == -1)) {
			vdata->gsV4kd2DecVersion.result =
				RETCODE_INVALID_COMMAND;
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	if (vdata->gsCodecType == STD_HEVC) {
		vdata->gsHevcDecVersion.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsHevcDecVersion,
				sizeof(HEVC_GET_VERSION_t));
		} else {
			if (copy_to_user(arg, &vdata->gsHevcDecVersion,
				sizeof(HEVC_GET_VERSION_t)) != 0U) {
				return -EFAULT;
			}
		}

		if ((*(vdata->gsHevcDecVersion.pszVersion) == -1) ||
			(*(vdata->gsHevcDecVersion.pszBuildData) == -1)) {
			vdata->gsHevcDecVersion.result =
				RETCODE_INVALID_COMMAND;
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	if (vdata->gsCodecType == STD_VP9) {
		vdata->gsVp9DecVersion.result = vdata->gsCommDecResult;

		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsVp9DecVersion,
				sizeof(VP9_GET_VERSION_t));
		} else {
			if (copy_to_user(arg, &vdata->gsVp9DecVersion,
				sizeof(VP9_GET_VERSION_t)) != 0U) {
				return -EFAULT;
			}
		}

		if ((*(vdata->gsVp9DecVersion.pszVersion) == -1) ||
			(*(vdata->gsVp9DecVersion.pszBuildData) == -1)) {
			vdata->gsVp9DecVersion.result =
				RETCODE_INVALID_COMMAND;
		}
	}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
	if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
		vdata->gsVpuDecVersion.result = vdata->gsCommDecResult;
		if (fromKernel) {
			(void)memcpy(arg, &vdata->gsVpuDecVersion,
				sizeof(VDEC_GET_VERSION_t));
		} else {
			if (copy_to_user(arg, &vdata->gsVpuDecVersion,
				sizeof(VDEC_GET_VERSION_t)) != 0U) {
				return -EFAULT;
			}
		}

		if ((*(vdata->gsVpuDecVersion.pszVersion) == -1) ||
			(*(vdata->gsVpuDecVersion.pszBuildData) == -1))  {
			vdata->gsVpuDecVersion.result =
				RETCODE_INVALID_COMMAND;
		}
	}
#endif
	return 0;
}

static int vdec_dev_init(struct vpu_decoder_data *vdata,
					void *arg, bool fromKernel)
{
	vdec_dev_pre_init(vdata);

	if (fromKernel) {
		(void)memcpy(&vdata->gsCodecType, arg, sizeof(int));
	} else {
		if (copy_from_user(&vdata->gsCodecType, arg, sizeof(int)) != 0U) {
			return -EFAULT;
		}
	}

	vdec_dev_post_init(vdata);

	return 0;
}

int vdec_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct vpu_decoder_data *vdata = dev_get_drvdata(misc->parent);

	unsigned long vma_range_size;

#if defined(CONFIG_PMAP)
	if(vma->vm_end < vma->vm_start) {
		V_DBG(VPU_DBG_ERROR,
		  "%s :: mmap :: vm_address_range failed : start_addr(0x%u), end_addr(0x%u)",
		  vdata->misc->name, vma->vm_start, vma->vm_end);
		return -EAGAIN;
	}

	if (range_is_allowed(vma->vm_pgoff,
		vma->vm_end - vma->vm_start) < 0) {
		V_DBG(VPU_DBG_ERROR,
			"%s :: mmap: this address is not allowed",
			vdata->misc->name);
		return -EAGAIN;
	}
#endif

	if (vma->vm_end > vma->vm_start) {
		vma_range_size = vma->vm_end - vma->vm_start;
	} else {
		V_DBG(VPU_DBG_ERROR,
		  "%s :: mmap :: vm_address_range failed : start_addr(0x%u), end_addr(0x%u)",
		  vdata->misc->name, vma->vm_start, vma->vm_end);
		return -EAGAIN;
	}

	vma->vm_page_prot = vmem_get_pgprot(vma->vm_page_prot, vma->vm_pgoff);
	if (remap_pfn_range(vma,
		vma->vm_start,
		vma->vm_pgoff,
		vma_range_size,
		vma->vm_page_prot) != 0) {
		V_DBG(VPU_DBG_ERROR,
			"%s :: mmap :: remap_pfn_range failed",
			vdata->misc->name);
		return -EAGAIN;
	}

	vma->vm_ops     = NULL;
	vetc_vm_flags_set(vma, (VM_IO | VM_DONTEXPAND | VM_PFNMAP));

	return 0;
}

unsigned int vdec_poll(struct file *filp, poll_table *wait)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct vpu_decoder_data *vdata = dev_get_drvdata(misc->parent);

	if (vdata == NULL) {
		return (unsigned int)POLLERR | (unsigned int)POLLNVAL;
	}

	if (vdata->vComm_data.count == 0U) {
		poll_wait(filp, &(vdata->vComm_data.wq), wait);
	}

	if (vdata->vComm_data.count > 0U) {
		vdata->vComm_data.count--;
		return POLLIN;
	}

	return 0;
}

long vdec_poll_2(struct file *filp, int timeout_ms)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct vpu_decoder_data *vdata = dev_get_drvdata(misc->parent);
	unsigned long jtimeout;

	if (vdata == NULL) {
		return (long)((unsigned int)POLLERR | (unsigned int)POLLNVAL);
	}

	if (timeout_ms < 0) {
		timeout_ms = 0;
	}

	jtimeout = msecs_to_jiffies(timeout_ms);
	if (jtimeout > (unsigned long)LONG_MAX) {
		jtimeout = (unsigned long)LONG_MAX;
	}

	(void) wait_event_interruptible_timeout(vdata->vComm_data.wq,
					vdata->vComm_data.count > 0,
					(long)jtimeout);

	if (vdata->vComm_data.count > 0) {
		vdata->vComm_data.count--;
		return (long)POLLIN;
	}

	return 0;
}
EXPORT_SYMBOL(vdec_poll_2);

static int vdec_cmd_open(struct vpu_decoder_data *vdata, char *str)
{
#ifdef CONFIG_SUPPORT_TCC_VPU
	V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
		"[%s] %s Begin alive(decoder:%d|vmgr:%d)",
		vdata->misc->name, str, vdata->vComm_data.dev_opened, vmgr_get_alive());
#endif

	if (vmem_get_free_memory((vputype)vdata->gsDecType) == 0U) {
		V_DBG(VPU_DBG_ERROR,
			"VPU %s: Couldn't open device because of no-reserved memory.",
			vdata->misc->name);
		return -ENOMEM;
	}

	if (vdata->vComm_data.dev_opened == 0U) {
		vdata->vComm_data.count = 0U;
	}
	vdata->vComm_data.dev_opened++;

#ifdef CONFIG_SUPPORT_TCC_VPU
	V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
		"[%s] %s End.. alive(decoder:%d|vmgr:%d)",
		vdata->misc->name, str, vdata->vComm_data.dev_opened, vmgr_get_alive());
#endif
	return 0;
}

static int vdec_cmd_release(struct vpu_decoder_data *vdata, char *str)
{
#ifdef CONFIG_SUPPORT_TCC_VPU
	V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
		"[%s] %s Begin alive(decoder:%d|vmgr:%d)",
		vdata->misc->name, str, vdata->vComm_data.dev_opened, vmgr_get_alive());
#endif

	if (vdata->vComm_data.dev_opened > 0U) {
		vdata->vComm_data.dev_opened--;
	}

	if (vdata->vComm_data.dev_opened == 0U) {

		V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
			"[%s] %s force close => %d",
			vdata->misc->name, str, vdata->gsDecType);

		vdec_force_close(vdata);

#ifdef CONFIG_SUPPORT_TCC_JPU
		if (vdata->gsCodecType == STD_MJPG) {
			vdec_clear_instance(vdata->gsDecType-(int)VPU_DEC);
			(void)jmgr_set_close((vputype)vdata->gsDecType, 1, 1);
		}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
		if (vdata->gsCodecType == STD_HEVC
				|| vdata->gsCodecType == STD_VP9) {
			vdec_clear_instance(vdata->gsDecType-(int)VPU_DEC);
			(void)vmgr_4k_d2_set_close((vputype)vdata->gsDecType, 1, 1);
		}
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
		if (vdata->gsCodecType == STD_HEVC) {
			vdec_clear_instance(vdata->gsDecType-(int)VPU_DEC);
			(void)hmgr_set_close((vputype)vdata->gsDecType, 1, 1);
		}
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
		if (vdata->gsCodecType == STD_VP9) {
			vdec_clear_instance(vdata->gsDecType-(int)VPU_DEC);
			(void)vp9mgr_set_close((vputype)vdata->gsDecType, 1, 1);
		}
#endif
#ifdef CONFIG_SUPPORT_TCC_VPU
		if (isVPUDecSupportCodec(vdata->gsCodecType) == 1) {
			(void)vmgr_set_close((vputype)vdata->gsDecType, 1, 1);
		}
#endif
		/* detach all flex array handles from list */
		if (!vdbg_mode()) {
			(void)pr_info("%s: detach flexio array", __func__);
		}
		V2_FLEXIO_DETACH(vdata->flx_list, V2D_IOP_MAX);
		V2_FLXDIO_DETACH(vdata->flx_io_delay);
	}

#ifdef CONFIG_SUPPORT_TCC_VPU
	V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
		"[%s] %s End.. alive(decoder:%d|vmgr:%d)",
		vdata->misc->name, str, vdata->vComm_data.dev_opened, vmgr_get_alive());
#endif
	return 0;
}

static long vdec_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long lret = 0;
	struct miscdevice *misc;
	struct vpu_decoder_data *vdata;
	union {
		unsigned long ul_data;
		unsigned int *pui_data;
		int *pi_data;	//NULL
		void *pv_data;
		const void *pcv_data;
		MEM_ALLOC_INFO_t *pmai_data;
		const MEM_ALLOC_INFO_t *pcmai_data;
		MEM_ALLOC_INFO_EX_t *pmie_data;
	} uarg;
#ifdef CONFIG_SUPPORT_TCC_VPU
	int vmgr_alive = vmgr_get_alive();
#endif
#ifdef CONFIG_SUPPORT_TCC_JPU
	int jmgr_alive = jmgr_get_alive();
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
	int vmgr_4kd2_alive = vmgr_4k_d2_get_alive();
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
	int hmgr_alive = hmgr_get_alive();
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
	int vp9mgr_alive = vp9mgr_get_alive();
#endif

	misc = (struct miscdevice *)filp->private_data;
	if (misc == NULL) {
		printk(KERN_ERR VLOG_TAG"[ioctl cmd: 0x%x] invalid private_data (NULL)", cmd);
		return -EINVAL;
	}
	vdata = dev_get_drvdata(misc->parent);
	if (vdata == NULL) {
		printk(KERN_ERR VLOG_TAG"[ioctl cmd: 0x%x] invalid drvdata (NULL)", cmd);
		return -EINVAL;
	}

	uarg.pi_data = NULL;
	uarg.ul_data = arg;

	if ((cmd != (unsigned int)DEVICE_INITIALIZE)
		&& (cmd != (unsigned int)DEVICE_INITIALIZE_KERNEL)
		&& (cmd != (unsigned int)V_DEC_GENERAL_RESULT)
		&& (cmd != (unsigned int)V_DEC_GENERAL_RESULT_KERNEL)
#ifdef CONFIG_SUPPORT_TCC_VPU
		&& (vmgr_alive == 0)
#endif
#ifdef CONFIG_SUPPORT_TCC_JPU
		&& (jmgr_alive == 0)
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
		&& (vmgr_4kd2_alive == 0)
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
		&& (hmgr_alive == 0)
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
		&& (vp9mgr_alive == 0)
#endif
	) {
#ifdef CONFIG_SUPPORT_TCC_VPU
		V_DBG(VPU_DBG_ERROR,
			"vdec ioctl(name:%s, cmd: 0x%x) Vpu Manager aliveness error = %d",
			vdata->misc->name, cmd, vmgr_get_alive());
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
		V_DBG(VPU_DBG_ERROR, "Vpu 4k_d2 Manager aliveness error = %d",
			vmgr_4kd2_alive);
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
		V_DBG(VPU_DBG_ERROR, "Vpu hevc Manager aliveness error = %d",
			hmgr_alive);
#endif
#ifdef CONFIG_SUPPORT_TCC_JPU
		V_DBG(VPU_DBG_ERROR, "Vpu jpu Manager aliveness error = %d",
			jmgr_alive);
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
		V_DBG(VPU_DBG_ERROR, "Vpu vp9 Manager aliveness error = %d",
			vp9mgr_alive);
#endif

		return -EPERM;
	}

	switch (cmd) {
	case V2D_IP_DRV_INI:
	case V2D_OP_DRV_INI:
	case V2D_IP_DEC_SEQDATA:
	case V2D_OP_DEC_SEQDATA:
	case V2D_IP_FRM_DRAIN:
	case V2D_IP_DEC_FRMDATA:
	case V2D_OP_DEC_FRMDATA:
	case V2D_IP_FRM_CLEAR:
	case V2D_OP_FRM_CLEAR:
	case V2D_IP_FRM_FLUSH:
	case V2D_OP_FRM_FLUSH:
	case V2D_IP_RNG_SETPOS:
	case V2D_OP_RNG_SETPOS:
	case V2D_IP_RNG_GETPOS:
	case V2D_OP_RNG_GETPOS:
	case V2D_IP_DRV_RST:
	case V2D_OP_DRV_RST:
	case V2D_IP_HWBUF_ASGN:
		lret = vdec_ioctl_flexio(filp, cmd, arg);
		break;

	case DEVICE_INITIALIZE:
	case DEVICE_INITIALIZE_KERNEL:
		lret = (long) vdec_dev_init(vdata, uarg.pv_data,
			cmd == (unsigned int)DEVICE_INITIALIZE_KERNEL);
		break;
	case V_DEC_INIT:
	case V_DEC_INIT_KERNEL:
		lret = (long)vdec_proc_init(vdata, uarg.pv_data,
			cmd == (unsigned int)V_DEC_INIT_KERNEL);
		break;
	case V_DEC_SEQ_HEADER:
	case V_DEC_SEQ_HEADER_KERNEL:
		lret = (long)vdec_proc_seq_header(vdata, uarg.pv_data,
				cmd == (unsigned int)V_DEC_SEQ_HEADER_KERNEL);
		break;
	case V_DEC_REG_FRAME_BUFFER:
	case V_DEC_REG_FRAME_BUFFER_KERNEL:
		lret = (long)vdec_proc_reg_framebuffer(vdata, uarg.pv_data,
				cmd == (unsigned int)V_DEC_REG_FRAME_BUFFER_KERNEL);
		break;
	case V_DEC_DECODE:
	case V_DEC_DECODE_KERNEL:
		lret = (long)vdec_proc_decode(vdata, uarg.pv_data,
				cmd == (unsigned int)V_DEC_DECODE_KERNEL);
		break;
	case V_DEC_GET_OUTPUT_INFO:
	case V_DEC_GET_OUTPUT_INFO_KERNEL:
		lret = (long)vdec_get_decode_output(vdata, uarg.pv_data,
				cmd == (unsigned int)V_DEC_GET_OUTPUT_INFO_KERNEL);
		break;
	case V_DEC_BUF_FLAG_CLEAR:
	case V_DEC_BUF_FLAG_CLEAR_KERNEL:
		lret = (long)vdec_proc_clear_bufferflag(vdata, uarg.pv_data,
				cmd == (unsigned int)V_DEC_BUF_FLAG_CLEAR_KERNEL);
		break;
	case V_DEC_CLOSE:
	case V_DEC_CLOSE_KERNEL:
		lret = (long)vdec_proc_exit(vdata, uarg.pv_data,
				cmd == (unsigned int)V_DEC_CLOSE_KERNEL);
		break;
	case V_DEC_FLUSH_OUTPUT:
	case V_DEC_FLUSH_OUTPUT_KERNEL:
		lret = (long)vdec_proc_flush(vdata, uarg.pv_data,
				cmd == (unsigned int)V_DEC_FLUSH_OUTPUT_KERNEL);
		break;
	case V_DEC_SWRESET:
	case V_DEC_SWRESET_KERNEL:
		lret = (long)vdec_proc_swreset(vdata,
				cmd == (unsigned int)V_DEC_SWRESET_KERNEL);
		break;
	case V_GET_RING_BUFFER_STATUS:
	case V_GET_RING_BUFFER_STATUS_KERNEL:
		lret = (long)vdec_proc_buf_status(vdata, uarg.pv_data,
				cmd == (unsigned int)V_GET_RING_BUFFER_STATUS_KERNEL);
		break;
	case V_FILL_RING_BUFFER_AUTO:
	case V_FILL_RING_BUFFER_AUTO_KERNEL:
		lret = (long)vdec_proc_buf_fill(vdata, uarg.pv_data,
				cmd == (unsigned int)V_FILL_RING_BUFFER_AUTO_KERNEL);
		break;
	case V_DEC_UPDATE_RINGBUF_WP:
	case V_DEC_UPDATE_RINGBUF_WP_KERNEL:
		lret = (long)vdec_proc_update_wp(vdata, uarg.pv_data,
				cmd == (unsigned int)V_DEC_UPDATE_RINGBUF_WP_KERNEL);
		break;
	case V_GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY:
	case V_GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY_KERNEL:
		lret = (long)vdec_proc_seq_header_ring(vdata, uarg.pv_data, cmd ==
		V_GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY_KERNEL);
		break;
	case V_GET_VPU_VERSION:
	case V_GET_VPU_VERSION_KERNEL:
		lret = (long)vdec_proc_get_version(vdata, uarg.pv_data,
				cmd == (unsigned int)V_GET_VPU_VERSION_KERNEL);
		break;
	case V_GET_RING_BUFFER_STATUS_RESULT:
	case V_GET_RING_BUFFER_STATUS_RESULT_KERNEL:
		lret = (long)vdec_result_buf_status(vdata, uarg.pv_data,
			cmd == (unsigned int)V_GET_RING_BUFFER_STATUS_RESULT_KERNEL);
		break;
	case V_FILL_RING_BUFFER_AUTO_RESULT:
	case V_FILL_RING_BUFFER_AUTO_RESULT_KERNEL:
		lret = (long)vdec_result_buf_fill(vdata, uarg.pv_data,
			cmd == (unsigned int)V_FILL_RING_BUFFER_AUTO_RESULT_KERNEL);
		break;
	case V_DEC_UPDATE_RINGBUF_WP_RESULT:
	case V_DEC_UPDATE_RINGBUF_WP_RESULT_KERNEL:
		lret = (long)vdec_result_update_wp(vdata, uarg.pv_data,
			cmd == (unsigned int)V_DEC_UPDATE_RINGBUF_WP_RESULT_KERNEL);
		break;
	case V_GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY_RESULT:
	case V_GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY_RESULT_KERNEL:
		lret = (long)vdec_result_seq_header_ring(vdata, uarg.pv_data,
			cmd ==
			(unsigned int)V_GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY_RESULT_KERNEL);
		break;
	case V_GET_VPU_VERSION_RESULT:
	case V_GET_VPU_VERSION_RESULT_KERNEL:
		lret = (long)vdec_result_get_version(vdata, uarg.pv_data,
			cmd == (unsigned int)V_GET_VPU_VERSION_RESULT_KERNEL);
		break;
	case V_DEC_GET_INFO:
	case V_DEC_GET_INFO_KERNEL:
	case V_DEC_REG_FRAME_BUFFER2:
	case V_DEC_REG_FRAME_BUFFER2_KERNEL:
		V_DBG(VPU_DBG_ERROR, "%s: ioctl %d: TBD", vdata->misc->name, cmd);
		break;
	case V_DEC_ALLOC_MEMORY:
	case V_DEC_ALLOC_MEMORY_KERNEL:
	{
		vputype type;
		int ret;

		MEM_ALLOC_INFO_t alloc_info;

		if (cmd == (unsigned int)V_DEC_ALLOC_MEMORY_KERNEL) {
			(void)memcpy(&alloc_info, uarg.pcv_data,
				sizeof(MEM_ALLOC_INFO_t));
		} else {
			if (copy_from_user(&alloc_info,
				(const MEM_ALLOC_INFO_t *)uarg.pcmai_data,
				sizeof(MEM_ALLOC_INFO_t)) != 0U) {
				lret = (long)-EFAULT;
				break;
			}
		}

		if (vdata->gsDecType < 0) {
			return -EFAULT;
		} else {
			type = (vputype)vdata->gsDecType;
		}

		ret = vmem_proc_alloc_memory(vdata->gsCodecType,
				&alloc_info, type);

		if (cmd == (unsigned int)V_DEC_ALLOC_MEMORY_KERNEL) {
			(void)memcpy(uarg.pv_data,
				&alloc_info, sizeof(MEM_ALLOC_INFO_t));
		} else {
			if (copy_to_user((MEM_ALLOC_INFO_t *)uarg.pmai_data,
				&alloc_info, sizeof(MEM_ALLOC_INFO_t)) != 0U) {
				ret = (int)-EFAULT;
			}
		}

		lret = (long)ret;
	}
	break;

	case V_DEC_ALLOC_MEMORY_EX: // 32bit user space, 64bit kernel space
	{
		int ret;
		vputype type;
		MEM_ALLOC_INFO_t alloc_info;
		MEM_ALLOC_INFO_EX_t alloc_info_ex;

		if (copy_from_user(&alloc_info_ex,
				(MEM_ALLOC_INFO_EX_t *)uarg.pmie_data,
				sizeof(MEM_ALLOC_INFO_EX_t)) != 0U) {
			lret = (long)-EFAULT;
			break;
		}

		(void)memcpy(&alloc_info, &alloc_info_ex, sizeof(MEM_ALLOC_INFO_t));

		type = (vputype)vdata->gsDecType;
		ret = vmem_proc_alloc_memory(vdata->gsCodecType,
			&alloc_info, type);

		(void)memcpy(&alloc_info_ex, &alloc_info,
			sizeof(MEM_ALLOC_INFO_EX_t));
		alloc_info_ex.kernel_remap_addr =
			alloc_info.kernel_remap_addr;

		/* Always returned zero value in case of framebuffer request */
		if (alloc_info.buffer_type != BUFFER_FRAMEBUFFER) {
			dprintk("[buffer_type: %d] kernel_remap_addr (%p)",
				(int) alloc_info.buffer_type,
				alloc_info.kernel_remap_addr);
		}

		if (copy_to_user((MEM_ALLOC_INFO_EX_t *)uarg.pmie_data,
			&alloc_info_ex, sizeof(MEM_ALLOC_INFO_EX_t)) != 0U) {
			lret = (long)-EFAULT;
		} else {
			lret = (long)ret;
		}
	}
	break;

	case V_DEC_FREE_MEMORY:
	case V_DEC_FREE_MEMORY_KERNEL:
	case V2D_IP_HWBUF_FREE:
		if (vdata->gsDecType >= 0) {
			vputype type;

			type = (vputype)vdata->gsDecType;
			lret = vmem_proc_free_memory(type);
		} else {
			lret = (long)-EFAULT;
		}
		break;

	case VPU_GET_FREEMEM_SIZE:
	case VPU_GET_FREEMEM_SIZE_KERNEL:
	{
		vputype type = VPU_DEC;	//0
		unsigned int szFreeMem = 0;

		if (vdata->gsDecType < 0) {
			lret = (long)-EFAULT;
		} else {
			type = (vputype)vdata->gsDecType;
		}

		szFreeMem = vmem_get_free_memory(type);

		if (cmd == (unsigned int)VPU_GET_FREEMEM_SIZE_KERNEL) {
			(void)memcpy(uarg.pv_data,
				&szFreeMem,
				sizeof(szFreeMem));
		} else {
			if (copy_to_user((unsigned int *)uarg.pui_data,
				&szFreeMem, sizeof(szFreeMem)) != 0U) {
				lret = (long)-EFAULT;
			}
		}
	}
	break;

	case V_DEC_GENERAL_RESULT:
	case V_DEC_GENERAL_RESULT_KERNEL:
		lret = vdec_result_general(vdata, uarg.pv_data,
			cmd == (unsigned int)V_DEC_GENERAL_RESULT_KERNEL);
		break;
	case V_DEC_INIT_RESULT:
	case V_DEC_INIT_RESULT_KERNEL:
		lret = vdec_result_init(vdata, uarg.pv_data,
			cmd == (unsigned int)V_DEC_INIT_RESULT_KERNEL);
		break;
	case V_DEC_SEQ_HEADER_RESULT:
	case V_DEC_SEQ_HEADER_RESULT_KERNEL:
		lret = vdec_result_seq_header(vdata, uarg.pv_data,
			cmd == (unsigned int)V_DEC_SEQ_HEADER_RESULT_KERNEL);
		break;
	case V_DEC_DECODE_RESULT:
	case V_DEC_DECODE_RESULT_KERNEL:
		lret = vdec_result_decode(vdata, uarg.pv_data,
			cmd == (unsigned int)V_DEC_DECODE_RESULT_KERNEL);
		break;
	case V_DEC_GET_OUTPUT_INFO_RESULT:
	case V_DEC_GET_OUTPUT_INFO_RESULT_KERNEL:
		lret = vdec_result_decode(vdata, uarg.pv_data,
			cmd == (unsigned int)V_DEC_GET_OUTPUT_INFO_RESULT_KERNEL);
		break;
	case V_DEC_FLUSH_OUTPUT_RESULT:
	case V_DEC_FLUSH_OUTPUT_RESULT_KERNEL:
		lret = vdec_result_flush(vdata, uarg.pv_data,
			cmd == (unsigned int)V_DEC_FLUSH_OUTPUT_RESULT_KERNEL);
		break;
#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	case V_DEC_TRY_OPEN_DEV:
	case V_DEC_TRY_OPEN_DEV_KERNEL:
		(void)vdec_cmd_open(vdata, "cmd");
		break;
	case V_DEC_TRY_CLOSE_DEV:
	case V_DEC_TRY_CLOSE_DEV_KERNEL:
		(void)vdec_cmd_release(vdata, "cmd");
		break;
#endif

	default:
		err_dec("[%s] Unsupported ioctl[%d]!!!",
			vdata->misc->name, cmd);
		break;
	}

	return lret;
}

#ifdef CONFIG_COMPAT
static long vdec_compat_ioctl(struct file *filep, unsigned int cmd,
			unsigned long arg)
{
	return vdec_ioctl(filep, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static int vdec_open(struct inode *pinode, struct file *filp)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct vpu_decoder_data *vdata = dev_get_drvdata(misc->parent);

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	vdata->vComm_data.dev_file_opened++;

	V_DBG(VPU_DBG_SEQUENCE, "%s :: open Out(%d)!!",
		vdata->misc->name, vdata->vComm_data.dev_file_opened);
#else
	(void)mutex_lock(&vdec_mutex);
	(void)vdec_cmd_open(vdata, fname_file_dec);
	(void)mutex_unlock(&vdec_mutex);
#endif
	LOG_COVERITY("%p", pinode);
	return 0;
}

static int vdec_release(struct inode *pinode, struct file *filp)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct vpu_decoder_data *vdata = dev_get_drvdata(misc->parent);

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	vdata->vComm_data.dev_file_opened--;

	V_DBG(VPU_DBG_CLOSE, "%s :: release Out(%d)!!",
		vdata->misc->name, vdata->vComm_data.dev_file_opened);
#else
	(void)mutex_lock(&vdec_mutex);
	(void)vdec_cmd_release(vdata, fname_file_dec);
	(void)mutex_unlock(&vdec_mutex);
#endif
	LOG_COVERITY("%p", pinode);
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
	struct vpu_decoder_data *vdata;
	int ret = -ENODEV;

	vdata = kzalloc(sizeof(struct vpu_decoder_data), GFP_KERNEL);
	if (vdata == NULL) {
		ret = -ENOMEM;
		return ret;
	}

	vdata->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (vdata->misc == NULL) {
		ret = -ENOMEM;
		kfree(vdata);
		return ret;
	}

	vdata->misc->minor = MISC_DYNAMIC_MINOR;
	vdata->misc->fops = &vdev_dec_fops;
	vdata->misc->name = pdev->name;
	vdata->misc->parent = &pdev->dev;

	vdata->gsDecType = pdev->id;
	(void)memset(&vdata->vComm_data, 0, sizeof(struct vpu_dec_data_t));
	spin_lock_init(&vdata->vComm_data.lock);
	init_waitqueue_head(&vdata->vComm_data.wq);

	if (misc_register(vdata->misc) != 0) {
		(void)pr_info("VPU %s: Couldn't register device.", pdev->name);
		ret = -EBUSY;
		kfree(vdata->misc);
		kfree(vdata);
		return ret;
	}

	mutex_init(&vdec_mutex);

	V2_FLEXIO_RESET(vdata->flx_list, V2D_IOP_MAX);
	V2_FLXDIO_RESET(vdata->flx_io_delay);

	platform_set_drvdata(pdev, vdata);
	(void)pr_info("VPU %s Driver(id:%d) Initialized.", pdev->name, pdev->id);

	return 0;
}
EXPORT_SYMBOL(vdec_probe);

int vdec_remove(struct platform_device *pdev)
{
	struct vpu_decoder_data *vdata =
			(struct vpu_decoder_data *)platform_get_drvdata(pdev);

	/* detach all flex array handles from list */
	(void)pr_info("%s: detach flexio array", __func__);
	V2_FLEXIO_DETACH(vdata->flx_list, V2D_IOP_MAX);
	V2_FLXDIO_DETACH(vdata->flx_io_delay);

	misc_deregister(vdata->misc);

	mutex_destroy(&vdec_mutex);
	kfree(vdata->misc);
	kfree(vdata);

	return 0;
}
EXPORT_SYMBOL(vdec_remove);

#endif /*CONFIG_VDEC_CNT_X*/
