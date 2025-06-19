/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_comm.h"

#ifdef CONFIG_SUPPORT_TCC_JPU

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
#include <linux/time.h>
#include <linux/compat.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 1, 0)
#	include <soc/telechips/pmap.h>
#endif

#include "vpu_buffer.h"
#include "vpu_devices.h"
#include "jpu_mgr_sys.h"
#include "jpu_mgr.h"
#include "jpu_mgr_flexio.h"

#define dprintk_jpu(msg...)  V_DBG(VPU_DBG_INFO, "TCC_JPU_MGR: " msg)
#define detailk_jpu(msg...)  V_DBG(VPU_DBG_INFO, "TCC_JPU_MGR: " msg)
#define cmdk_jpu(msg...)     V_DBG(VPU_DBG_INFO, "TCC_JPU_MGR [Cmd]: " msg)
#define err_jpu(msg...)      V_DBG(VPU_DBG_INFO, "TCC_JPU_MGR [Err]: " msg)

#define JPU_REGISTER_DUMP

#if 0 //For test purpose!!
#define FORCED_ERROR
#endif
#ifdef FORCED_ERROR
#define FORCED_ERR_CNT 300
static int forced_error_count = FORCED_ERR_CNT;
#endif

// Control only once!!
static struct mgr_data_t jmgr_data;
static struct task_struct *kidle_task_jpu;	// = NULL;

static struct VpuList jmgr_vlist;
static char jpu_fname_file[] = "file";

#if defined(USE_ACCESS_POINT)
// SHARE_POINT_ORDER_XXX :
//    VPU = 0, JPU = 1, HEVC = 2,
//    4KD2 = 3, HEVC_ENC = 4, HEVC_ENC_2 = 5
#   define SHARE_POINT_ORDER_JPU 1U

//Decoder
typedef int (*tccfp_jpu_dec_t)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
static tccfp_jpu_dec_t tcc_jpu_dec;

//Encoder
#	if DEFINED_CONFIG_VENC_CNT_1to16
	typedef int (*tccfp_jpu_enc_t)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	tccfp_jpu_enc_t tcc_jpu_enc;
#	endif

typedef struct st_jpu_func_t {
	unsigned int check_code1;
	int (*tccfp_jpu_dec)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	unsigned int check_code2;
	int (*tccfp_jpu_enc)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	unsigned int check_code3;
	int (*tccfp_jpu_dec_esc)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	int (*tccfp_jpu_dec_ext)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	unsigned int check_code4;
} st_jpu_func;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,10,0)
static st_jpu_func stJPUFuncBase = {0, NULL, 0, NULL, 0, NULL, NULL, 0};
#endif

static st_jpu_func *stJPUFunc = NULL;

static int check_jpu_access_addr_valid(void)
{
	int ret = -1;
	unsigned int fourcc_1 = GET_FOURCC_1(stJPUFunc->check_code1);
	unsigned int fourcc_2 = GET_FOURCC_2(stJPUFunc->check_code1);
	unsigned int fourcc_3 = GET_FOURCC_3(stJPUFunc->check_code1);
	unsigned int fourcc_4 = GET_FOURCC_4(stJPUFunc->check_code1);
	unsigned int fourcc_5 = GET_FOURCC_1(stJPUFunc->check_code2);
	unsigned int fourcc_6 = GET_FOURCC_2(stJPUFunc->check_code2);
	unsigned int fourcc_7 = GET_FOURCC_3(stJPUFunc->check_code2);
	unsigned int fourcc_8 = GET_FOURCC_4(stJPUFunc->check_code2);
	unsigned int fourcc_9 = GET_FOURCC_1(stJPUFunc->check_code3);
	unsigned int fourcc_10 = GET_FOURCC_2(stJPUFunc->check_code3);
	unsigned int fourcc_11 = GET_FOURCC_3(stJPUFunc->check_code3);
	unsigned int fourcc_12 = GET_FOURCC_4(stJPUFunc->check_code3);
	unsigned int fourcc_13 = GET_FOURCC_1(stJPUFunc->check_code4);
	unsigned int fourcc_14 = GET_FOURCC_2(stJPUFunc->check_code4);
	unsigned int fourcc_15 = GET_FOURCC_3(stJPUFunc->check_code4);
	unsigned int fourcc_16 = GET_FOURCC_4(stJPUFunc->check_code4);

	if(((CHECK_CODE_01 | stJPUFunc->check_code1) == CHECK_CODE_01) &&
			((CHECK_CODE_02 | stJPUFunc->check_code2) == CHECK_CODE_02) &&
			((CHECK_CODE_03 | stJPUFunc->check_code3) == CHECK_CODE_03) &&
			((CHECK_CODE_04 | stJPUFunc->check_code4) == CHECK_CODE_04)) {
		ret = 0;
	} else {
		V_DBG(VPU_DBG_INFO, "JPU CheckCode %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
				fourcc_1, fourcc_2, fourcc_3, fourcc_4,
				fourcc_5, fourcc_6, fourcc_7, fourcc_8,
				fourcc_9, fourcc_10,fourcc_11,fourcc_12,
				fourcc_13,fourcc_14,fourcc_15,fourcc_16
			);
	}
	return ret;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
static int get_jpu_access_addr_file(void)
{
	int ret = 0;
	struct file *filp = NULL;
	mm_segment_t oldfs;
	void *tTmpPtr = NULL;

#	if LINUX_VERSION_CODE < KERNEL_VERSION(5,0,0)
	oldfs = get_fs();
	set_fs( get_ds() );
#	elif LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
	oldfs = get_fs();
	set_fs( KERNEL_DS );
#	else
	oldfs = force_uaccess_begin();
#	endif

	filp = filp_open("/proc/jpu", O_RDONLY, 0x1A4);

	VPU_CAST_PT(tTmpPtr, filp);
	if(IS_ERR(tTmpPtr)) {
		V_DBG(VPU_DBG_ERROR, "/proc/jpu file open fail!!");
		ret = -1;
	} else {
		char data[20];
		unsigned long long res = 0;
		int idx = 0;

		idx = (sizeof(void*)*2UL) + 2UL;

#	if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
		ret = vfs_read(filp, data, sizeof(data), &filp->f_pos);
#	else
		ret = filp->f_op->read(filp, data, sizeof(data), &filp->f_pos);
#	endif

		data[idx] = '\0';

		ret = kstrtoull(data, 16, &res);

		(void)memmove((void*)&stJPUFunc, (void*)&res, sizeof(unsigned long));

		(void)filp_close(filp, NULL);
	}

#	if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
	set_fs(oldfs);
#	else
	force_uaccess_end(oldfs);
#	endif
	return ret;
}

#else

static int get_jpu_access_addr_mem(void)
{
	int ret = 0;
	void *va = NULL;

	va = ioremap(SHARE_POINT_ADDR + (SHARD_POINT_GAP * SHARE_POINT_ORDER_JPU), SHARD_POINT_GAP);

	if (va == NULL) {
		V_DBG(VPU_DBG_ERROR, "ioremap failed");
		ret = -ENOMEM;
	} else {
		memcpy(&stJPUFuncBase, va, sizeof(st_jpu_func));
		stJPUFunc = &stJPUFuncBase;

		V_DBG(VPU_DBG_INFO, "remap (PA : 0x%08x / VA : 0x%p) Dec ADDR : 0x%p",
				SHARE_POINT_ADDR, va,
				stJPUFunc->tccfp_jpu_dec);

		iounmap(va);
	}
	return ret;
}
#endif

static int get_jpu_access_addr(void)
{
	int ret = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
	ret = get_jpu_access_addr_file();
#else
	ret = get_jpu_access_addr_mem();
#endif

	if (ret == 0) {
		ret = check_jpu_access_addr_valid();
		if (ret == 0) {
			tcc_jpu_dec = (tccfp_jpu_dec_t)stJPUFunc->tccfp_jpu_dec;
			//tcc_jpu_dec_esc = (tccfp_jpu_dec_esc_t)stJPUFunc->tccfp_jpu_dec_esc;
			//tcc_jpu_dec_ext = (tccfp_jpu_dec_ext_t)stJPUFunc->tccfp_jpu_dec_ext;
			#if DEFINED_CONFIG_VENC_CNT_1to16
			tcc_jpu_enc = (tccfp_jpu_enc_t)stJPUFunc->tccfp_jpu_enc;
			#endif
		} else {
			ret = -1;
		}
	}

	return ret;
}

#else // defined(USE_ACCESS_POINT)

extern int tcc_jpu_dec(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
# 	if DEFINED_CONFIG_VENC_CNT_1to16
extern int tcc_jpu_enc(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2);
# 	endif
#endif //#if defined(USE_ACCESS_POINT)

static int tcc_jpu_dec_l(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2) VPU_NO_SANITIZE_CFI
{
	return tcc_jpu_dec(Op, pHandle, pParam1, pParam2);
}

#if DEFINED_CONFIG_VENC_CNT_1to16
static int tcc_jpu_enc_l(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2) VPU_NO_SANITIZE_CFI
{
	return tcc_jpu_enc(Op, pHandle, pParam1, pParam2);
}
#endif

static int (*gs_fpTccJpuDec)(int Op, vcodec_handle_t *pHandle, void *pParam1,
				  void *pParam2);

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)

#if 0
#define DEBUG_TRACE_TA
#endif
#ifdef DEBUG_TRACE_TA
#define tracetee(msg...) V_DBG(VPU_DBG_INFO, "TCC_JPU_MGR: " msg)
#else
#define tracetee(msg...)
#endif

// kernel client application area
#include <linux/slab.h>		// kmalloc
struct JpuInstKernel {
	uint32_t pRegBaseAddr;	// physical addr
	uint32_t u32RegBaseAddrSize;	// physical addr size
	vcodec_handle_t hJpuDecHandle;
	jpu_dec_init_t stJpuDecInit;
	jpu_dec_initial_info_t stJpuDecInitialInfo;
	jpu_dec_buffer_t stJpuDecBuffer;
	jpu_dec_input_t stJpuDecInput;
	jpu_dec_output_t stJpuDecOutput;
	int iSeqHeaderSize;
	char szVersion[64];
	char szBuildData[32];
};

#define JPU_REG_BASE_ADDR (0x15180000)

int tcc_jpu_dec_internal(int Op, vcodec_handle_t *pHandle, void *pParam1,
			void *pParam2)
{

	int ret = 0;
	struct JpuInstKernel *pstInst = NULL;

	tracetee(
	"[KERNEL(%s)] OpCode = %d, %x, pHandle = %x, %x\n",
		__func__, Op, Op, pHandle, *pHandle);

	if (pHandle != NULL) {
		pstInst = (struct JpuInstKernel *)((codec_handle_t)*pHandle);
	}

	switch (Op) {
	case JPU_DEC_INIT:
	{
		tracetee("[KERNEL(JPU_DEC_INIT) %s] pHandle = %p, %x\n",
		 __func__, pHandle, (uint32_t)(*pHandle));

		pstInst = kmalloc(sizeof(struct JpuInstKernel),
		  GFP_KERNEL);
		if (pstInst == NULL) {
			tracetee("[KERNEL(JPU_DEC_INIT)] pstInst is NULL\n");
		}

		(void)memset(pstInst, 0, sizeof(struct JpuInstKernel));
		(void)memcpy(&pstInst->stJpuDecInit,
			   (jpu_dec_init_t *) pParam1,
			   sizeof(jpu_dec_init_t));

		pstInst->pRegBaseAddr = JPU_REG_BASE_ADDR;
		pstInst->u32RegBaseAddrSize = 16 * 1024;

		// you need re-mapping into TA
		pstInst->stJpuDecInit.m_Memset = NULL;
		pstInst->stJpuDecInit.m_Memcpy = NULL;
		pstInst->stJpuDecInit.m_reg_read = NULL;
		pstInst->stJpuDecInit.m_reg_write = NULL;

		pstInst->stJpuDecInit.m_Interrupt = NULL;
		pstInst->stJpuDecInit.m_Ioremap = NULL;
		pstInst->stJpuDecInit.m_Iounmap = NULL;

		if (pParam2 != NULL) {
			tracetee("[KERNEL(JPU_DEC_INIT)] trace\n");
			(void)memcpy(&pstInst->stJpuDecInitialInfo,
				   (jpu_dec_initial_info_t *) pParam2,
				   sizeof(jpu_dec_initial_info_t));
		}

#ifdef DEBUG_TRACE_TA
		{
			jpu_dec_init_t *pinit =
				(jpu_dec_init_t *)pParam1;
			unsigned char *ptr =
				pinit->m_BitstreamBufAddr[VA];

			tracetee(
			"[KERNEL(JPU_DEC_INIT):%s] BITSTRAM_DATA %x %x =========================",
			 __func__,
				pstInst->stJpuDecInit.m_BitstreamBufAddr[PA],
				pstInst->stJpuDecInit.m_BitstreamBufAddr[VA]);
			tracetee("%02x %02x %02x %02x %02x %02x %02x %02x\n",
				ptr[0], ptr[1], ptr[2], ptr[3],
				ptr[4], ptr[5], ptr[6], ptr[7]);
		}
#endif

		// New instance for TA
		ret = jpu_optee_command(Op, (void *)pstInst,
		 sizeof(struct JpuInstKernel));

		tracetee(
			"[KERNEL(JPU_DEC_INIT):%s] Instance Handle address = %p %p, pstInst = %p\n",
			 __func__, pHandle, *pHandle, pstInst);
		*pHandle = (vcodec_handle_t)pstInst;
	}
	break;

#if defined(JPU_C6)
	case JPU_DEC_SEQ_HEADER:
	{
		bool bDiminishInputCopy = (pstInst->stJpuDecInit.m_uiDecOptFlags
			 & (1UL << 26U)) ? (bool)true : (bool)false;

		tracetee(
			"[KERNEL(JPU_DEC_SEQ_HEADER) %s] 002 pHandle = %p, %x\n",
			__func__, pHandle, (uint32_t) (*pHandle));

		if (bDiminishInputCopy) {
			(void)memcpy(&pstInst->stJpuDecInput,
				   (jpu_dec_input_t *) pParam1,
				   sizeof(jpu_dec_input_t));
			pstInst->iSeqHeaderSize =
			  (int)pstInst->stJpuDecInput.m_iBitstreamDataSize;
			tracetee(
				"[KERNEL(JPU_DEC_SEQ_HEADER) %s] DiminishInputCopy[O] - seq.header size = %d, %x\n",
				 __func__, pstInst->iSeqHeaderSize,
				 pstInst->iSeqHeaderSize);
		} else {
			pstInst->iSeqHeaderSize = (int)pParam1;
			tracetee(
				"[KERNEL(JPU_DEC_SEQ_HEADER) %s] DiminishInputCopy[X] - seq.header size = %d, %x\n",
				 __func__, pstInst->iSeqHeaderSize,
				 pstInst->iSeqHeaderSize);
		}

		ret = jpu_optee_command(Op, (void *)pstInst,
			 sizeof(struct JpuInstKernel));

		(void)memcpy((jpu_dec_initial_info_t *)pParam2,
			 &pstInst->stJpuDecInitialInfo,
			 sizeof(jpu_dec_initial_info_t));
		tracetee(
			"[KERNEL(JPU_DEC_SEQ_HEADER) %s] [INITIAL_INFO] W x H (%d x %d)",
			__func__, pstInst->stJpuDecInitialInfo.m_iPicWidth,
			pstInst->stJpuDecInitialInfo.m_iPicHeight);
		}
		break;

	case JPU_DEC_GET_ROI_INFO:
		break;
	#endif

	case JPU_DEC_REG_FRAME_BUFFER:
	{
		tracetee(
		"[KERNEL(JPU_DEC_REG_FRAME_BUFFER) %s] pHandle = %p, %x\n",
			__func__, pHandle, (uint32_t)(*pHandle));

		(void)memcpy(&pstInst->stJpuDecBuffer,
			 (jpu_dec_buffer_t *) pParam1,
			 sizeof(jpu_dec_buffer_t));

		ret =
			jpu_optee_command(Op, (void *)pstInst,
					sizeof(struct JpuInstKernel));
	}
	break;

	case JPU_DEC_DECODE:
	{
		tracetee(
		"[KERNEL(JPU_DEC_DECODE) xxxxxxxxx %s] pHandle = %p, %x\n",
			__func__, pHandle, (uint32_t)(*pHandle));

		(void)memcpy(&pstInst->stJpuDecInput,
		 (jpu_dec_input_t *)pParam1, sizeof(jpu_dec_input_t));
		(void)memcpy(&pstInst->stJpuDecOutput,
		 (jpu_dec_output_t *)pParam2, sizeof(jpu_dec_output_t));

		#ifdef DEBUG_TRACE_TA
		tracetee(
		"[KERNEL(JPU_DEC_DECODE) BITSTRAM_DATA %x, Size = %d=========================",
			pstInst->stJpuDecInput.m_BitstreamDataAddr[VA],
			pstInst->stJpuDecInput.m_iBitstreamDataSize);

		{
			unsigned char *ptr =
			  pstInst->stJpuDecInput.m_BitstreamDataAddr[VA];

			tracetee(
			"%02x %02x %02x %02x %02x %02x %02x %02x\n",
			 ptr[0], ptr[1], ptr[2], ptr[3], ptr[4],
			  ptr[5], ptr[6], ptr[7]); ptr += 8;
			tracetee(
			"%02x %02x %02x %02x %02x %02x %02x %02x\n",
			 ptr[0], ptr[1], ptr[2], ptr[3], ptr[4],
			  ptr[5], ptr[6], ptr[7]); ptr += 8;
			tracetee(
			"%02x %02x %02x %02x %02x %02x %02x %02x\n",
			 ptr[0], ptr[1], ptr[2], ptr[3], ptr[4],
			  ptr[5], ptr[6], ptr[7]); ptr += 8;
			tracetee(
			"%02x %02x %02x %02x %02x %02x %02x %02x\n",
			 ptr[0], ptr[1], ptr[2], ptr[3], ptr[4],
			  ptr[5], ptr[6], ptr[7]); ptr += 8;
		}
#endif
		ret =
			jpu_optee_command(Op, (void *)pstInst,
				sizeof(struct JpuInstKernel));

		(void)memcpy((jpu_dec_output_t *) pParam2,
			   &pstInst->stJpuDecOutput,
			   sizeof(jpu_dec_output_t));
		tracetee(
		  "[[KERNEL(JPU_DEC_DECODE)OUT][W:%d][H:%d][DecStatus:%d][ConsumedBytes:%d][ErrMBs:%d][DispOutIdx:%d]",
		  pstInst->stJpuDecOutput.m_DecOutInfo.m_iWidth,
		  pstInst->stJpuDecOutput.m_DecOutInfo.m_iHeight,
		  pstInst->stJpuDecOutput.m_DecOutInfo.m_iDecodingStatus,
		  pstInst->stJpuDecOutput.m_DecOutInfo.m_iConsumedBytes,
		  pstInst->stJpuDecOutput.m_DecOutInfo.m_iNumOfErrMBs,
		  pstInst->stJpuDecOutput.m_DecOutInfo.m_iDispOutIdx);

	}
	break;

	case JPU_DEC_CLOSE:
		{
			tracetee(
				"[KERNEL(JPU_DEC_CLOSE) %s] pHandle = %p, %x\n",
				 __func__, pHandle, (uint32_t) (*pHandle));

			ret =
				jpu_optee_command(Op, (void *)pstInst,
						  sizeof(struct JpuInstKernel));

			kfree(pstInst);
			pstInst = NULL;
			pHandle = NULL;
			tracetee("free jpu instance !!");
		}
		break;

	case JPU_CODEC_GET_VERSION:
		{
			tracetee(
				"[KERNEL(JPU_CODEC_GET_VERSION) %s] pHandle = %p, %x\n",
				 __func__, pHandle, (uint32_t) (*pHandle));

			ret =
				jpu_optee_command(Op, (void *)pstInst,
						  sizeof(struct JpuInstKernel));

			if (pParam1 == NULL && pParam2 == NULL) {
				pParam1 = pstInst->szVersion;
				pParam2 = pstInst->szBuildData;
			} else {
				(void)memcpy(pParam1, pstInst->szVersion,
					   sizeof(pstInst->szVersion));
				(void)memcpy(pParam2, pstInst->szBuildData,
					   sizeof(pstInst->szBuildData));
			}

			tracetee(
				"[KERNEL(JPU_CODEC_GET_VERSION) %s] Version = %s, %s\n",
				 __func__, pParam1, pstInst->szVersion);
			tracetee(
				"[KERNEL(JPU_CODEC_GET_VERSION) %s] BuildData = %s, %s\n",
				 __func__, pParam2, pstInst->szBuildData);
		}
		break;

	default:
			err_jpu("Invalid Operation = %d(0x%x)", __func__, Op, Op);
		break;
	}

	return ret;
}
#endif /*CONFIG_ARCH_TCC899X || CONFIG_ARCH_TCC901X*/

int jmgr_opened(void)
{
	int ret = 1;

	if (atomic_read(&jmgr_data.opened) == 0) {
		ret = 0;
	}

	return ret;
}
EXPORT_SYMBOL(jmgr_opened);

int jmgr_get_close(vputype type)
{
	return jmgr_data.closed[type];
}

int jmgr_get_alive(void)
{
	return atomic_read(&jmgr_data.opened);
}

int jmgr_set_close(vputype type, int value, int bfreemem)
{
	int ret = 0;

	if (jmgr_get_close(type) == value) {
		dprintk_jpu(" %d was already set into %d.", type, value);
		ret = -1;
	} else {
		jmgr_data.closed[type] = value;
		if (value == 1) {
			jmgr_data.handle[type] = 0x00;

			if (bfreemem != 0) {
				(void)vmem_proc_free_memory(type);
			}
		}
	}

	return ret;
}

static void jmgr_close_all(int bfreemem)
{
	(void)jmgr_set_close(VPU_DEC, 1, bfreemem);
	(void)jmgr_set_close(VPU_DEC_EXT, 1, bfreemem);
	(void)jmgr_set_close(VPU_DEC_EXT2, 1, bfreemem);
	(void)jmgr_set_close(VPU_DEC_EXT3, 1, bfreemem);
	(void)jmgr_set_close(VPU_DEC_EXT4, 1, bfreemem);
}

int jmgr_process_ex(struct VpuList *cmd_list, vputype type, int Op, int *result)
{
	int ret = 0;

	if (atomic_read(&jmgr_data.opened) != 0) {
		err_jpu("\n process_ex %d - 0x%x\n", type, Op);

		if ((type < (vputype)0) || (type >= (vputype)VPU_MAX)) {
			err_jpu("range \n");
		} else {
			ret = 1;
			if (jmgr_get_close(type) == 0) {
				cmd_list->type = (unsigned int)type;
				cmd_list->cmd_type = Op;
				cmd_list->handle = jmgr_data.handle[(unsigned int)type];
				cmd_list->args = NULL;
				cmd_list->comm_data = NULL;
				cmd_list->vpu_result = result;
				(void)jmgr_list_manager(cmd_list, (unsigned int)LIST_ADD);
				//ret = 1;
			}
		}
	}

	return ret;
}

static int jmgr_internal_handler(void)
{
	long ret = 0L;
	int ret_code = RETCODE_INTR_DETECTION_NOT_ENABLED;
	unsigned int timeout = 200UL;
	unsigned long jtimeout;
	long long_max = LONG_MAX;

	if (jmgr_data.current_resolution > (unsigned int)(1920UL * 1080UL)) {
		timeout = 5000UL;
	}

	jtimeout = msecs_to_jiffies(timeout);
	if (jtimeout > (unsigned long)long_max) {
		jtimeout = (unsigned long)long_max;
	}

	if (jmgr_data.check_interrupt_detection != 0) {
		unsigned int *tmpPtr = NULL;

		if (atomic_read(&jmgr_data.oper_intr) > 0) {
			detailk_jpu("Success 1: jpu operation!!");
			ret_code = RETCODE_SUCCESS;
		} else {
			ret =
				wait_event_interruptible_timeout(
					jmgr_data.oper_wq,
					atomic_read(&jmgr_data.oper_intr) > 0,
								(long)jtimeout);

			if (atomic_read(&jmgr_data.oper_intr) > 0) {
				detailk_jpu("Success 2: jpu operation!!");

#if defined(FORCED_ERROR)
				if (forced_error_count-- <= 0) {
					static unsigned char fname[] =
					  "jmgr_internal_handler force-timed_out";

					ret_code = RETCODE_CODEC_EXIT;
					forced_error_count = FORCED_ERR_CNT;
					vetc_dump_reg_all(
					  (char *)jmgr_data.base_addr, fname);
				} else {
					ret_code = RETCODE_SUCCESS;
				}
#else
				ret_code = RETCODE_SUCCESS;
#endif
			} else {
				char *tmpVoidPtr = NULL;
				static unsigned char fname[] =
				  "jmgr_internal_handler timed_out";

				err_jpu(
				"[CMD 0x%x][%ld]: jpu timed_out(ref %d msec) => oper_intr[%d]!! [%d]th frame len %d\n",
				jmgr_data.current_cmd, ret, timeout,
				atomic_read(&jmgr_data.oper_intr),
				jmgr_data.nDecode_Cmd,
				jmgr_data.szFrame_Len);
				VPU_CAST_PT(tmpVoidPtr, jmgr_data.base_addr);

				vetc_dump_reg_all(tmpVoidPtr, fname);
				ret_code = RETCODE_CODEC_EXIT;
			}
		}

		atomic_set(&jmgr_data.oper_intr, 0);
		VPU_CAST_PT(tmpPtr, jmgr_data.base_addr);
		jmgr_status_clear(tmpPtr);
	}

	V_DBG(VPU_DBG_INTERRUPT, "out (Interrupt option=%d, ev=%d)",
		jmgr_data.check_interrupt_detection,
		ret_code);

	return ret_code;
}

static int jmgr_convert_returnType(int err)
{
	int ret;

	if ((err >= JPG_RET_INVALID_HANDLE) && (err <= JPG_RET_NOT_INITIALIZED)) {
		ret = (err - 2);
	} else {
		switch (err) {
		case JPG_RET_BIT_EMPTY:
			ret = 50;
			break;
		case JPG_RET_EOS:
			ret = 51;
			break;
		case JPG_RET_INSUFFICIENT_BITSTREAM_BUF:
			ret = RETCODE_INSUFFICIENT_BITSTREAM_BUF;
			break;
		case JPG_RET_CODEC_FINISH:
			ret = RETCODE_CODEC_FINISH;
			break;
		default:
			ret = err;
			break;
		}
	}

	return ret;
}

static int jmgr_process(vputype type, int cmd, long pHandle, void *args)
{
	int ret = 0;
	void *temp_ptr = NULL;
	char *char_addr = NULL;
	vcodec_handle_t *dec_handle = NULL;
#ifdef CONFIG_VPU_TIME_MEASUREMENT
	long long startTime, endTime;
	long long time_gap_us = 0LL;
#endif

	jmgr_data.check_interrupt_detection = 0;
	jmgr_data.current_cmd = cmd;

	if (type < VPU_ENC) {
		if ((cmd != VPU_DEC_INIT) &&
			(cmd != VPU_DEC_INIT_KERNEL) &&
			(cmd != V2D_IP_DRV_INI)) {
			if ((jmgr_get_close(type) != 0)
				|| (jmgr_data.handle[type] == 0x00)) {
				return RETCODE_MULTI_CODEC_EXIT_TIMEOUT;
			}
		}

		if ((cmd != (int)VPU_DEC_BUF_FLAG_CLEAR) && (cmd != (int)VPU_DEC_DECODE)
			&& (cmd != (int)VPU_DEC_BUF_FLAG_CLEAR_KERNEL)
			&& (cmd != (int)VPU_DEC_DECODE_KERNEL)) {
			cmdk_jpu("Decoder(%d), command: %#x\n", type, cmd);
		}

		switch (cmd) {
		case VPU_DEC_INIT:
		case VPU_DEC_INIT_KERNEL:
		case V2D_IP_DRV_INI:
		{
			int retDec = 0;
			JDEC_INIT_t *arg = NULL;
			bool isFlexible = (cmd == V2D_IP_DRV_INI);
			if (isFlexible) {
				temp_ptr = v2jpgmgr_unmarshal_ip_inidata(args);
				VPU_CAST_PT(arg, temp_ptr);
				//cmd = VPU_DEC_INIT;
			} else {
				VPU_CAST_PT(arg, args);
			}

			jmgr_data.handle[type] = 0x00;

			if (arg == NULL)
			{
				err_jpu("Dec :: null pointer error(%s %d)", __func__, __LINE__);
				return RETCODE_FAILURE;
			}
			else
			{
				VPU_CAST_PT(arg->gsJpuDecInit.m_RegBaseVirtualAddr, jmgr_data.base_addr);
				arg->gsJpuDecInit.m_Memcpy = vetc_memcpy;
				arg->gsJpuDecInit.m_Memset
				= (void  (*) (void *tar, int value, unsigned int size, unsigned int count))
					vetc_memset;
				arg->gsJpuDecInit.m_Interrupt
				= (int  (*) (void))jmgr_internal_handler;
				arg->gsJpuDecInit.m_reg_read
				= (unsigned int (*)(void *base_addr, unsigned int offset))
					vetc_reg_read;
				arg->gsJpuDecInit.m_reg_write
				= (void (*)(void *base_addr, unsigned int offset, unsigned int data))
					vetc_reg_write;

				jmgr_data.check_interrupt_detection = 1;
				jmgr_data.bDiminishInputCopy = (arg->gsJpuDecInit.m_uiDecOptFlags & (1UL<<26UL)) ? (bool)true : (bool)false;

			#if defined(USE_ACCESS_POINT)
				if (check_jpu_access_addr_valid() != 0) {
					err_jpu(
						"Dec-%d Access address envalid!!(%d)",
						type, check_jpu_access_addr_valid());
					return RETCODE_FAILURE;
				}
			#endif

				gs_fpTccJpuDec = tcc_jpu_dec_l;
				dprintk_jpu("Dec :: loading JPU ...");

			#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
				if (arg->gsJpuDecInit.m_uiDecOptFlags & (1 << 30)) {
					dprintk_jpu("Dec :: USE OPTEE_JPU");
					gs_fpTccJpuDec = tcc_jpu_dec_internal;
				}
			#endif

				if (vmem_alloc_count((int)type) <= 0) {
					dprintk_jpu(
					"Dec-%d No Buffer allocation\n",
						type);
					return RETCODE_FAILURE;
				}

			#if defined(JPU_C5)
				dprintk_jpu(
				"Dec :: Init In => Reg(0x%x/0x%x), Stream(%#x/%#x, %#x)",
					jmgr_data.base_addr,
					arg->gsJpuDecInit.m_RegBaseVirtualAddr,
					arg->gsJpuDecInit.m_BitstreamBufAddr[PA],
					arg->gsJpuDecInit.m_BitstreamBufAddr[VA],
					arg->gsJpuDecInit.m_iBitstreamBufSize);

				dprintk_jpu(
				"Dec :: Init In => rotation(%d/%d), mirror(%d/%d), Interleave: %d\n",
					arg->gsJpuDecInit.m_iRot_angle,
					arg->gsJpuDecInit.m_iRot_enalbe,
					arg->gsJpuDecInit.m_iMirrordir,
					arg->gsJpuDecInit.m_iMirror_enable,
					arg->gsJpuDecInit.m_bCbCrInterleaveMode);

				retDec = gs_fpTccJpuDec(JPU_DEC_INIT,
						(void *)(&arg->gsJpuDecHandle),
						(void *)(&arg->gsJpuDecInit),
						(void *)(&arg->gsJpuDecInitialInfo));
				jmgr_data.current_resolution
				= arg->gsJpuDecInitialInfo.m_iPicWidth
				* arg->gsJpuDecInitialInfo.m_iPicHeight;
			#else
				dprintk_jpu(
				"Dec :: Init In => Handle(%#x) Reg(%#x/%#x), Stream(%#x/%#x, %#x) Interleave: %d\n",
					arg->gsJpuDecHandle,
					jmgr_data.base_addr,
					arg->gsJpuDecInit.m_RegBaseVirtualAddr,
					arg->gsJpuDecInit.m_BitstreamBufAddr[PA],
					arg->gsJpuDecInit.m_BitstreamBufAddr[VA],
					arg->gsJpuDecInit.m_iBitstreamBufSize,
					arg->gsJpuDecInit.m_iCbCrInterleaveMode);
				temp_ptr = &arg->gsJpuDecHandle;
				VPU_CAST_PT(dec_handle, temp_ptr);
				retDec = gs_fpTccJpuDec(JPU_DEC_INIT,
						dec_handle,
						(void *)(&arg->gsJpuDecInit),
						(void *)NULL);
			#endif

				if (retDec != RETCODE_SUCCESS) {
					dprintk_jpu("Dec :: Init Done with ret(0x%x)",
						retDec);
					if (retDec != JPG_RET_CODEC_EXIT) {
						static unsigned char fname[] =
						"init failure";
						VPU_CAST_PT(char_addr, jmgr_data.base_addr);
						vetc_dump_reg_all(
						char_addr, fname);
					}
				} else {
					if (isFlexible) {
						v2jpgmgr_marshal_op_inidata(args);
					}
				}

				ret = jmgr_convert_returnType(retDec);
				if (ret != RETCODE_CODEC_EXIT
					&& arg->gsJpuDecHandle != 0) {
					jmgr_data.handle[type]
						= arg->gsJpuDecHandle;
					(void)jmgr_set_close(type, 0, 0);
					cmdk_jpu("Dec :: jmgr_data.handle = 0x%x\n",
						arg->gsJpuDecHandle);
				} else {
					(void)jmgr_set_close(type, 0, 0);
					(void)jmgr_set_close(type, 1, 1);
				}
				dprintk_jpu("Dec :: Init Done Handle(0x%x)",
					arg->gsJpuDecHandle);

			#ifdef CONFIG_VPU_TIME_MEASUREMENT
				jmgr_data.iTime[type].print_out_index
				= jmgr_data.iTime[type].proc_base_cnt = 0;
				jmgr_data.iTime[type].accumulated_proc_time
				= jmgr_data.iTime[type].accumulated_frame_cnt = 0;
				jmgr_data.iTime[type].proc_time_30frames = 0;
			#endif
			}
		}
		break;

		case VPU_DEC_SEQ_HEADER:
		case VPU_DEC_SEQ_HEADER_KERNEL:
	#if defined(JPU_C6)
		{
			int iSize;
			int iBitstreamDataSize;
			void *arg = args;
			int retDec, width, height;
			jpu_dec_initial_info_t *gsJpuDecInitialInfo;
			union {
				int i_data;
				int *pi_data;	//NULL
				void *pv_data;
			} udata;
			JPU_DECODE_t *jpu_dec_arg = NULL;
			JDEC_SEQ_HEADER_t *jpu_seq_header_arg = NULL;

			udata.pi_data = NULL;

			VPU_CAST_PT(jpu_dec_arg, arg);
			VPU_CAST_PT(jpu_seq_header_arg, arg);
			gsJpuDecInitialInfo
			 = jmgr_data.bDiminishInputCopy
			 ? &(jpu_dec_arg->gsJpuDecInitialInfo)
			 : &(jpu_seq_header_arg->gsJpuDecInitialInfo);

			if ((int)jpu_dec_arg->gsJpuDecInput.m_iBitstreamDataSize > 0) {
				iBitstreamDataSize =
					jpu_dec_arg->gsJpuDecInput.m_iBitstreamDataSize;
			} else {
				iBitstreamDataSize = 0;
			}

			if (jmgr_data.bDiminishInputCopy) {
				iSize = iBitstreamDataSize;
			} else {
				unsigned int uisize = jpu_seq_header_arg->stream_size;

				if(uisize < (unsigned int)INT_MAX) {
					iSize = (int)uisize;
				} else {
					iSize = INT_MAX;
				}
			}
			udata.i_data =  iSize;
			jmgr_data.szFrame_Len = (unsigned int)iSize;

			jmgr_data.check_interrupt_detection = 1;
			jmgr_data.nDecode_Cmd = 0;
			dprintk_jpu(
			"Dec :: JPU_DEC_SEQ_HEADER in :: Handle(0x%x) size(%d)",
				pHandle, iSize);
			retDec = gs_fpTccJpuDec(JPU_DEC_SEQ_HEADER,
					(vcodec_handle_t *)&pHandle,
					(jmgr_data.bDiminishInputCopy ?
						(void *)(&(jpu_dec_arg
							->gsJpuDecInput))
							: (void *)udata.pv_data),
					(void *)gsJpuDecInitialInfo);

			ret = jmgr_convert_returnType(retDec);
			dprintk_jpu(
			"Dec :: JPU_DEC_SEQ_HEADER out 0x%x :: res info(%d x %d), src_format(%d), Error_reason(%d), minFB(%d)",
			  ret, gsJpuDecInitialInfo->m_iPicWidth,
			  gsJpuDecInitialInfo->m_iPicHeight,
			  gsJpuDecInitialInfo->m_iSourceFormat,
			  gsJpuDecInitialInfo->m_iErrorReason,
			  gsJpuDecInitialInfo->m_iMinFrameBufferCount);

			width = gsJpuDecInitialInfo->m_iPicWidth;
			if ((width<16) || (width > VPU_LIMIT_PICWIDTH)) {
				V_DBG(VPU_DBG_ERROR,"## Dec :: not supported pic. width(%d)", width);
				return RETCODE_INVALID_STRIDE;
			}

			height = gsJpuDecInitialInfo->m_iPicHeight;
			if ((height<16) || (height > VPU_LIMIT_PICHEIGHT)) {
				V_DBG(VPU_DBG_ERROR,"## Dec :: not supported pic. height(%d)", height);
				return RETCODE_INVALID_STRIDE;
			}

			jmgr_data.szFrame_Len = (((unsigned int)width * (unsigned int)height * 3U) / 2U);
			jmgr_data.current_resolution
			 = ((unsigned int)width * (unsigned int)height);
		}
	#else
		{
			err_jpu("Dec :: not supported command(0x%x)", cmd);
			return 0x999;
		}
	#endif
		break;
		case V2D_IP_DEC_SEQDATA:
		{
		#if defined(JPU_C6)
			JDEC_SEQ_HEADER_t *arg = NULL;
			jpu_dec_initial_info_t *initial_info = NULL;
			unsigned long iSize = 0UL;

			temp_ptr = v2jpgmgr_unmarshal_ip_seqdata(args);
			if (temp_ptr != NULL) {
				VPU_CAST_PT(arg, temp_ptr);
				initial_info = &arg->gsJpuDecInitialInfo;
				iSize = (unsigned long)arg->stream_size;
				jmgr_data.szFrame_Len = iSize;
				jmgr_data.check_interrupt_detection = 1;
				jmgr_data.nDecode_Cmd = 0;

				(void)pr_info("[%s][IP] V2D_IP_DEC_SEQDATA: size %lu", __func__, iSize);

				ret = gs_fpTccJpuDec(JPU_DEC_SEQ_HEADER,
									(vcodec_handle_t *)&pHandle,
									(void *)iSize,
									(void *)initial_info);
				ret = jmgr_convert_returnType(ret);

				(void)pr_info("[%s][OP] V2D_IP_DEC_SEQDATA: ret = %x, %d x %d, "
							"yuv(%d), reason(%d), min.fb(%d)",
						__func__, ret, initial_info->m_iPicWidth, initial_info->m_iPicHeight,
						initial_info->m_iSourceFormat,
						initial_info->m_iErrorReason,
						initial_info->m_iMinFrameBufferCount);

				jmgr_data.current_resolution = (unsigned)initial_info->m_iPicWidth
					* (unsigned)initial_info->m_iPicHeight;
				jmgr_data.szFrame_Len = (jmgr_data.current_resolution * 3U) / 2U;

				if (ret == RETCODE_SUCCESS) {
					v2jpgmgr_marshal_op_seqdata(args);

					ret = v2jpgmgr_register_hwbuf(args, pHandle, gs_fpTccJpuDec);
					ret = jmgr_convert_returnType(ret);
					(void)pr_info("[%s] V2D_IP_DEC_SEQDATA result = %d", __func__, ret);
				}
			} else {
				err_jpu("%s: V2D_IP_DEC_SEQDATA: v2jpgmgr_unmarshal_ip_seqdata failed ..", __func__);
				ret = 0x999;
			}
		#else
			err_jpu("%s: V2D_IP_DEC_SEQDATA: Not supported", __func__);
			ret = 0x999;
		#endif
		}
		break;

		case VPU_DEC_REG_FRAME_BUFFER:
		case VPU_DEC_REG_FRAME_BUFFER_KERNEL:
		{
			JPU_SET_BUFFER_t *arg = (JPU_SET_BUFFER_t *)args;
			int retDec;

#if defined(JPU_C5)
			dprintk_jpu(
			"Dec :: JPU_DEC_REG_FRAME_BUFFER in :: scale[%d], addr[0x%x/0x%x]",
			  arg->gsJpuDecBuffer.m_iJPGScaleRatio,
			  arg->gsJpuDecBuffer.m_FrameBufferStartAddr[PA],
			  arg->gsJpuDecBuffer.m_FrameBufferStartAddr[VA]);
#else
			dprintk_jpu(
			"Dec :: JPU_DEC_REG_FRAME_BUFFER in :: count[%d], scale[%d], addr[0x%x/0x%x], Reserved[8] = 0x%x\n",
			  arg->gsJpuDecBuffer.m_iFrameBufferCount,
			  arg->gsJpuDecBuffer.m_iJPGScaleRatio,
			  arg->gsJpuDecBuffer.m_FrameBufferStartAddr[PA],
			  arg->gsJpuDecBuffer.m_FrameBufferStartAddr[VA],
			  arg->gsJpuDecBuffer.m_Reserved[8]);
#endif

			retDec = gs_fpTccJpuDec(
				JPU_DEC_REG_FRAME_BUFFER,
				(vcodec_handle_t *)&pHandle,
				(void *)(&arg->gsJpuDecBuffer),
				(void *)NULL);
			ret = jmgr_convert_returnType(retDec);
			dprintk_jpu(
			"Dec :: JPU_DEC_REG_FRAME_BUFFER out\n");
		}
		break;

		case VPU_DEC_DECODE:
		case VPU_DEC_DECODE_KERNEL:
		case V2D_IP_DEC_FRMDATA:
		{
			JPU_DECODE_t *arg = NULL;
			int retDec, iSize;

			bool isFlexible = (cmd == V2D_IP_DEC_FRMDATA);

			if (isFlexible) {
				arg = (JPU_DECODE_t *) v2jpgmgr_unmarshal_ip_frmdata(args);
			} else {
				arg = (JPU_DECODE_t *) args;
			}

			if(arg == NULL)
			{
				ret = RETCODE_FAILURE;
			}
			else
			{
#ifdef CONFIG_VPU_TIME_MEASUREMENT
				startTime = vetc_GetKtime();
#endif
				iSize = arg->gsJpuDecInput.m_iBitstreamDataSize;
				if (iSize < 0) {
					iSize = 0;
				}
				jmgr_data.szFrame_Len
				= (unsigned int)iSize;
#if defined(JPU_C6)
				dprintk_jpu(
				"Dec :: Dec In => 0x%x - 0x%x, 0x%x\n",
				arg->gsJpuDecInput.m_BitstreamDataAddr[PA],
				arg->gsJpuDecInput.m_BitstreamDataAddr[VA],
				arg->gsJpuDecInput.m_iBitstreamDataSize);
#else
				dprintk_jpu(
				"Dec :: Dec In => 0x%x - 0x%x, 0x%x, 0x%x - 0x%x, toggle: %d\n",
				arg->gsJpuDecInput.m_BitstreamDataAddr[PA],
				arg->gsJpuDecInput.m_BitstreamDataAddr[VA],
				arg->gsJpuDecInput.m_iBitstreamDataSize,
				arg->gsJpuDecInput.m_FrameBufferStartAddr[PA],
				arg->gsJpuDecInput.m_FrameBufferStartAddr[VA],
						arg->gsJpuDecInput.m_iLooptogle);
#endif
				jmgr_data.check_interrupt_detection = 1;
				retDec
				= gs_fpTccJpuDec(JPU_DEC_DECODE,
					(vcodec_handle_t *)&pHandle,
					(void *)(&arg->gsJpuDecInput),
					(void *)(&arg->gsJpuDecOutput));
				ret = jmgr_convert_returnType(retDec);

				dprintk_jpu(
				"Dec :: Dec Out => %d x %d, status(%d), Consumed(%d), Err(%d)",
				arg->gsJpuDecOutput.m_DecOutInfo.m_iWidth,
				arg->gsJpuDecOutput.m_DecOutInfo.m_iHeight,
				arg->gsJpuDecOutput.m_DecOutInfo.m_iDecodingStatus,
				arg->gsJpuDecOutput.m_DecOutInfo.m_iConsumedBytes,
				arg->gsJpuDecOutput.m_DecOutInfo.m_iNumOfErrMBs);

				dprintk_jpu(
				"Dec :: Dec Out => 0x%x 0x%x 0x%x / 0x%x 0x%x 0x%x\n",
				(unsigned long)arg->gsJpuDecOutput.m_pCurrOut[PA][0],
				(unsigned long)arg->gsJpuDecOutput.m_pCurrOut[PA][1],
				(unsigned long)arg->gsJpuDecOutput.m_pCurrOut[PA][2],
				(unsigned long)arg->gsJpuDecOutput.m_pCurrOut[VA][0],
				(unsigned long)arg->gsJpuDecOutput.m_pCurrOut[VA][1],
				(unsigned long)arg->gsJpuDecOutput.m_pCurrOut[VA][2]);

				if (arg->gsJpuDecOutput.m_DecOutInfo.m_iDecodingStatus
				== VPU_DEC_BUF_FULL) {
					err_jpu("Buffer full\n");
				}

				if (isFlexible && (ret == RETCODE_SUCCESS)) {
					v2jpgmgr_marshal_op_frmdata(args);
				}
				jmgr_data.nDecode_Cmd++;

#ifdef CONFIG_VPU_TIME_MEASUREMENT
				endTime = vetc_GetKtime();
#endif
			}
		}
		break;

		case VPU_DEC_CLOSE:
		case VPU_DEC_CLOSE_KERNEL:
		case V2D_IP_DRV_RST:
		{
			int retDec;
			jmgr_data.check_interrupt_detection = 1;
			retDec =
				gs_fpTccJpuDec(JPU_DEC_CLOSE,
					   (vcodec_handle_t *)&pHandle,
					   (void *)NULL,
					   (void *)NULL);
			ret = jmgr_convert_returnType(retDec);
			dprintk_jpu("Dec :: JPU_DEC_CLOSED !!");

			(void)jmgr_set_close(type, 1, 1);
		}
		break;

		case VPU_CODEC_GET_VERSION:
		case VPU_CODEC_GET_VERSION_KERNEL:
		{
			JPU_GET_VERSION_t *arg = (JPU_GET_VERSION_t *)args;
			int retDec;

			jmgr_data.check_interrupt_detection = 1;

			retDec = gs_fpTccJpuDec(JPU_CODEC_GET_VERSION,
			 (vcodec_handle_t *)&pHandle,
			 arg->pszVersion,
			 arg->pszBuildData);
			ret = jmgr_convert_returnType(retDec);
			dprintk_jpu("Dec :: version : %s, build : %s\n",
				arg->pszVersion, arg->pszBuildData);
		}
		break;

		default:
			err_jpu("Dec :: not supported command(0x%x)", cmd);
			ret = 0x999;
			break;
		}
	}
#if DEFINED_CONFIG_VENC_CNT_1to16
	else {
		switch (cmd) {
		case VPU_ENC_INIT:
		{
			JENC_INIT_t *arg = (JENC_INIT_t *)args;
			union_codec_handle_t codec_handle;
			int retEnc;
			int width, height;

			jmgr_data.handle[type] = 0x00;

			arg->gsJpuEncInit.m_RegBaseVirtualAddr
			 = (codec_addr_t)jmgr_data.base_addr;
			arg->gsJpuEncInit.m_Memcpy
			 = vetc_memcpy;
			arg->gsJpuEncInit.m_Memset
			 = (void (*) (void *, int, unsigned int, unsigned int))
				 vetc_memset;
			arg->gsJpuEncInit.m_Interrupt
			 = (int (*) (void))jmgr_internal_handler;
			arg->gsJpuEncInit.m_reg_read
			 = (unsigned int (*)(void *, unsigned int))
				 vetc_reg_read;
			arg->gsJpuEncInit.m_reg_write
			 = (void (*)(void*, unsigned int, unsigned int))
				vetc_reg_write;

			jmgr_data.check_interrupt_detection = 1;
#if defined(JPU_C6)
			dprintk_jpu(
			  "Enc :: Init In => Reg(0x%x/0x%x), Src(%d x %d, %d), Q(%d), Stream(0x%x/0x%x, 0x%x), Inter(%d), Option(0x%x)",
			  jmgr_data.base_addr,
			  arg->gsJpuEncInit.m_RegBaseVirtualAddr,
			  arg->gsJpuEncInit.m_iPicWidth,
			  arg->gsJpuEncInit.m_iPicHeight,
			  arg->gsJpuEncInit.m_iSourceFormat,
			  arg->gsJpuEncInit.m_iEncQuality,
			  arg->gsJpuEncInit.m_BitstreamBufferAddr[PA],
			  arg->gsJpuEncInit.m_BitstreamBufferAddr[VA],
			  arg->gsJpuEncInit.m_iBitstreamBufferSize,
			  arg->gsJpuEncInit.m_iCbCrInterleaveMode,
			  arg->gsJpuEncInit.m_uiEncOptFlags);
#else
			dprintk_jpu(
			  "Enc :: Init In => Reg(0x%x/0x%x), Src(%d x %d, %d), Stream(0x%x/0x%x, 0x%x), rotation(%d), Inter(%d), Option(0x%x)",
			  jmgr_data.base_addr,
			  arg->gsJpuEncInit.m_RegBaseVirtualAddr,
			  arg->gsJpuEncInit.m_iPicWidth,
			  arg->gsJpuEncInit.m_iPicHeight,
			  arg->gsJpuEncInit.m_iMjpg_sourceFormat,
			  arg->gsJpuEncInit.m_iEncQuality,
			  arg->gsJpuEncInit.m_BitstreamBufferAddr[PA],
			  arg->gsJpuEncInit.m_BitstreamBufferAddr[VA],
			  arg->gsJpuEncInit.m_iBitstreamBufferSize,
			  arg->gsJpuEncInit.m_iRotMode,
			  arg->gsJpuEncInit.m_bCbCrInterleaveMode,
			  arg->gsJpuEncInit.m_uiEncOptFlags);
#endif
			width = arg->gsJpuEncInit.m_iPicWidth;
			if ((width<16) || (width > VPU_LIMIT_PICWIDTH)) {
				V_DBG(VPU_DBG_ERROR,"## Enc :: not supported pic. width(%d)", width);
				return RETCODE_INVALID_STRIDE;
			}

			height = arg->gsJpuEncInit.m_iPicHeight;
			if ((height<16) || (height > VPU_LIMIT_PICHEIGHT)) {
				V_DBG(VPU_DBG_ERROR,"## Enc :: not supported pic. height(%d)", height);
				return RETCODE_INVALID_STRIDE;
			}

			jmgr_data.szFrame_Len = (((unsigned int)width * (unsigned int)height * 3) / 2);
			jmgr_data.current_resolution = ((unsigned int)width * (unsigned int)height);

			codec_handle.pcodec_handle = &arg->gsJpuEncHandle;
			retEnc
			 = tcc_jpu_enc_l(JPU_ENC_INIT,
				(vcodec_handle_t *)(codec_handle.pvcodec_handle),
				(void *)(&arg->gsJpuEncInit), (void *)NULL);
			if (retEnc != RETCODE_SUCCESS) {
				dprintk_jpu(
					"## Enc :: Init Done with ret(0x%x)",
					retEnc);
				if (retEnc != RETCODE_CODEC_EXIT) {
					static unsigned char fname[] = "init failure";

					vetc_dump_reg_all(
					  (char *)jmgr_data.base_addr, fname);
				}
			}

			ret = jmgr_convert_returnType(retEnc);
			if ((ret != RETCODE_CODEC_EXIT)
				&& (arg->gsJpuEncHandle != 0)) {
				jmgr_data.handle[type]
					= arg->gsJpuEncHandle;
				(void)jmgr_set_close(type, 0, 0);
				cmdk_jpu("Enc :: jmgr_data.handle = 0x%x\n",
					 arg->gsJpuEncHandle);
			}
			dprintk_jpu("Enc :: Init Done Handle(0x%x)",
				  arg->gsJpuEncHandle);
			jmgr_data.nDecode_Cmd = 0;
#ifdef CONFIG_VPU_TIME_MEASUREMENT
			jmgr_data.iTime[type].print_out_index
			 = jmgr_data.iTime[type].proc_base_cnt = 0;
			jmgr_data.iTime[type].accumulated_proc_time
			 = jmgr_data.iTime[type].accumulated_frame_cnt
			 = 0;
			jmgr_data.iTime[type].proc_time_30frames = 0;
#endif
		}
		break;

		case VPU_ENC_ENCODE:
			{
				JPU_ENCODE_t *arg = (JPU_ENCODE_t *)args;
				int retEnc;

#ifdef CONFIG_VPU_TIME_MEASUREMENT
				startTime = vetc_GetKtime();
#endif

				dprintk_jpu(
				  "Enc :: Enc In => Handle(0x%x), YUV(0x%x - 0x%x - 0x%x) -> BitStream(0x%x - 0x%x / 0x%x)",
				  pHandle,
				  arg->gsJpuEncInput.m_PicYAddr,
				  arg->gsJpuEncInput.m_PicCbAddr,
				  arg->gsJpuEncInput.m_PicCrAddr,
				  arg->gsJpuEncInput.m_BitstreamBufferAddr[PA],
				  arg->gsJpuEncInput.m_BitstreamBufferAddr[VA],
				  arg->gsJpuEncInput.m_iBitstreamBufferSize);

				jmgr_data.check_interrupt_detection = 1;
				retEnc =
				 tcc_jpu_enc_l(JPU_ENC_ENCODE,
				  (vcodec_handle_t *)&pHandle,
				   (void *)(&arg->gsJpuEncInput),
					(void *)(&arg->gsJpuEncOutput));
				ret = jmgr_convert_returnType(retEnc);

#if defined(JPU_C5)
				dprintk_jpu(
				"Enc :: Enc Out => (0x%x/0x%x), Size(%d/%d)",
					 arg->gsJpuEncOutput.m_BitstreamOut[0],
					 arg->gsJpuEncOutput.m_BitstreamOut[1],
					 arg->gsJpuEncOutput.m_iHeaderOutSize,
					 arg->gsJpuEncOutput.m_iBitstreamOutSize);
#else
				dprintk_jpu(
				"Enc :: Enc Out => ret(%d) (0x%x/0x%x), Size(%d/%d)",
					 ret, arg->gsJpuEncOutput.m_BitstreamOut[0],
					 arg->gsJpuEncOutput.m_BitstreamOut[1],
					 arg->gsJpuEncOutput.m_iBitstreamHeaderSize,
					 arg->gsJpuEncOutput.m_iBitstreamOutSize);
#endif
				if(UINT_MAX - jmgr_data.nDecode_Cmd > 1U) {
					jmgr_data.nDecode_Cmd++;
				}
#ifdef CONFIG_VPU_TIME_MEASUREMENT
				endTime = vetc_GetKtime();
#endif
			}
			break;

		case VPU_ENC_CLOSE:
		{
			int retEnc;

			jmgr_data.check_interrupt_detection = 1;
			retEnc
			 = tcc_jpu_enc_l(JPU_ENC_CLOSE,
				(vcodec_handle_t *)&pHandle,
				(void *)NULL, (void *)NULL);
			ret = jmgr_convert_returnType(retEnc);
			dprintk_jpu("Enc :: JPU_ENC_CLOSED!!");

			(void)jmgr_set_close(type, 1, 1);
		}
		break;

		case VPU_CODEC_GET_VERSION:
		{
			JPU_GET_VERSION_t *arg = (JPU_GET_VERSION_t *)args;
			int retEnc;

			jmgr_data.check_interrupt_detection = 1;
			retEnc = tcc_jpu_enc_l(JPU_CODEC_GET_VERSION,
					(vcodec_handle_t *)&pHandle,
					arg->pszVersion,
					arg->pszBuildData);
			ret = jmgr_convert_returnType(retEnc);
			dprintk_jpu("Enc :: version : %s, build : %s\n",
				arg->pszVersion, arg->pszBuildData);
		}
		break;

		default:
			err_jpu("Enc :: not supported command(0x%x)", cmd);
			ret = 0x999;
			break;
		}
	}
#endif

#ifdef CONFIG_VPU_TIME_MEASUREMENT
	time_gap_us = vetc_GetTimediff_us(endTime, startTime);

	if (cmd == VPU_DEC_DECODE || cmd == VPU_ENC_ENCODE) {
		printMeasurementTime((void*)&jmgr_data, type, ((cmd == VPU_DEC_DECODE) ? 1 : 0), time_gap_us);
	}
#endif

	return ret;
}

static int jmgr_proc_exit_by_external(struct VpuList *list, int *result, unsigned int type)
{
	int ret = 1;
	vputype vtype;

	if (type >= (unsigned int)VPU_MAX) {
		ret = 0;
	} else {
		vtype = (vputype)type;
		ret = 0;

		if (!jmgr_get_close(vtype) && (jmgr_data.handle[type] != 0x00)) {
			list->type = type;

			if (type >= (u32)VPU_ENC) {
				list->cmd_type = VPU_ENC_CLOSE;
			} else {
				list->cmd_type = VPU_DEC_CLOSE;
			}

			list->handle = jmgr_data.handle[type];
			list->args = NULL;
			list->comm_data = NULL;
			list->vpu_result = result;

			dprintk_jpu("%s for %d!!", __func__, type);
			(void)jmgr_list_manager(list, (unsigned int)LIST_ADD);

			ret = 1;
		}
	}

	return ret;
}

#if 0 // Keep the code for future use
static void jmgr_wait_process(int wait_ms)
{
	int max_count = wait_ms / 20;

	// In case of exceptional processing. ex). sdcard out!!
	while (jmgr_data.cmd_processing) {
		max_count--;
		msleep(20);

		if (max_count <= 0) {
			err_jpu("cmd_processing(cmd %d) didn't finish!!",
				jmgr_data.current_cmd);
			break;
		}
	}
}
#endif

static int jmgr_external_all_close(int wait_ms)
{
	unsigned int type;
	int max_count;
	int ret;

	for (type = 0; type < (unsigned int)JPU_MAX; type++) {
		if (jmgr_proc_exit_by_external(&jmgr_data.vList[type], &ret, type) != 0) {
			max_count = wait_ms / 10;

			while (!jmgr_get_close((vputype)type)) {
				if (max_count < 0) {
					break;
				}
				max_count--;
				usleep_range(0, 1000);	//msleep(10);
			}
		}
	}

	return 0;
}

static int jmgr_cmd_open(char *str)
{
	int ret = 0;

	dprintk_jpu("jmgr_%s_open In!! %d'th\n", str, atomic_read(&jmgr_data.opened));

	jmgr_enable_clock();	//jmgr_enable_clock(0, 0);

	if (atomic_read(&jmgr_data.opened) == 0) {
		char *tmpPtr = NULL;
#ifdef FORCED_ERROR
		forced_error_count = FORCED_ERR_CNT;
#endif
#if DEFINED_CONFIG_VENC_CNT_1to16
		jmgr_data.only_decmode = 0;
#else
		jmgr_data.only_decmode = 1;
#endif
		jmgr_data.clk_limitation = 1;
		jmgr_data.cmd_processing = 0;

		jmgr_hw_reset();
		jmgr_enable_irq(jmgr_data.irq);
		VPU_CAST_PT(tmpPtr, jmgr_data.base_addr);
		ret = vmem_init();
		if (ret < 0) {
			err_jpu("failed to allocate memory for JPU!! %d\n", ret);
		}
	}

	atomic_inc(&jmgr_data.opened);

	dprintk_jpu("jmgr_%s_open Out!! %d'th\n", str, atomic_read(&jmgr_data.opened));

	return 0;
}

static int jmgr_cmd_release(char *str)
{
	dprintk_jpu("jmgr_%s_release In!! %d'th\n", str,
		atomic_read(&jmgr_data.opened));

	if (atomic_read(&jmgr_data.opened) > 0) {
		atomic_dec(&jmgr_data.opened);
	}

	if (atomic_read(&jmgr_data.opened) == 0) {
		int type = 0;
		int alive_cnt = 0;

// To close whole jpu instance when being killed process opened this.
#if 1
		if (!jmgr_data.bVpu_already_proc_force_closed) {
			jmgr_data.external_proc = 1;
			(void)jmgr_external_all_close(200);
			jmgr_data.external_proc = 0;
		}
		jmgr_data.bVpu_already_proc_force_closed = (bool)false;
#endif

		for (type = 0; type < JPU_MAX; type++) {
			if (jmgr_data.closed[type] == 0) {
				if (alive_cnt < INT_MAX) {
					alive_cnt++;
				}
			}
		}

		if (alive_cnt != 0) {
			dprintk_jpu("JPU might be cleared by force.");
		}

		atomic_set(&jmgr_data.oper_intr, 0);
		jmgr_data.cmd_processing = 0;

		jmgr_close_all(1);

		jmgr_disable_irq(jmgr_data.irq);
		(void)jmgr_BusPrioritySetting(BUS_FOR_NORMAL, 0);
		vmem_deinit();
		jmgr_hw_assert();

		udelay(1000);
	}

	jmgr_disable_clock();	//jmgr_disable_clock(0, 0);

	if (jmgr_data.nOpened_Count > INT_MAX) {
		V_DBG(VPU_DBG_ERROR, "jmgr_data.nOpened_Count is already MAX count, can't increase.");
		jmgr_data.nOpened_Count = 0;
	} else {
		jmgr_data.nOpened_Count++;
	}

	dprintk_jpu("jmgr_%s_release Out!! %d'th, total = %d  - DEC(%d/%d/%d/%d)",
	  str,
	  atomic_read(&jmgr_data.opened),
	  jmgr_data.nOpened_Count,
	  jmgr_get_close(VPU_DEC),
	  jmgr_get_close(VPU_DEC_EXT),
	  jmgr_get_close(VPU_DEC_EXT2),
	  jmgr_get_close(VPU_DEC_EXT3),
	  jmgr_get_close(VPU_DEC_EXT4));

	return 0;
}

static long jmgr_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	CONTENTS_INFO info;
	OPENED_sINFO open_info;
	union {
		unsigned long ul_data;
		unsigned int *pui_data;
		int *pi_data;
		void *pv_data;	//NULL
		CONTENTS_INFO *pci_data;
		OPENED_sINFO *posi_data;
	} uarg;

	uarg.pv_data = NULL;
	uarg.ul_data = arg;

	mutex_lock(&jmgr_data.comm_data.io_mutex);

	switch (cmd) {
	case VPU_SET_CLK:
	case VPU_SET_CLK_KERNEL:
		if (cmd == (unsigned int)VPU_SET_CLK_KERNEL) {
			(void)memcpy(&info, (CONTENTS_INFO *)uarg.pci_data,
				sizeof(info));
		} else {
			if (copy_from_user(&info, (CONTENTS_INFO *)uarg.pci_data,
				sizeof(info)) != 0U) {
				ret = -EFAULT;
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
			} else {
				if (copy_from_user(&type, (unsigned int *)uarg.pui_data,
					sizeof(unsigned int)) != 0U) {
					ret = -EFAULT;
				}
			}

			if (ret == 0) {
				if (type > (unsigned int)VPU_MAX) {
					type = (unsigned int)VPU_DEC;
				}
				freemem_sz = vmem_get_freemem_size((vputype)type);

				if (cmd == (unsigned int)VPU_GET_FREEMEM_SIZE_KERNEL) {
					(void)memcpy((unsigned int *)uarg.pui_data,
						&freemem_sz, sizeof(unsigned int));
				} else {
					if (copy_to_user((unsigned int *)uarg.pui_data,
						&freemem_sz, sizeof(unsigned int)) != 0U) {
						ret = -EFAULT;
					}
				}
			}
		}
		break;

	case VPU_SET_MEM_ALLOC_MODE:
	case VPU_SET_MEM_ALLOC_MODE_KERNEL:
		if (cmd == (unsigned int)VPU_SET_MEM_ALLOC_MODE_KERNEL) {
			(void)memcpy(&open_info,
				(OPENED_sINFO *)uarg.posi_data,
				sizeof(OPENED_sINFO));
		} else {
			if (copy_from_user(&open_info, (OPENED_sINFO *)uarg.posi_data, sizeof(OPENED_sINFO)) != 0U) {
				ret = -EFAULT;
			}
		}

		if (ret == 0) {
			if (open_info.opened_cnt != 0U) {
				vmem_set_only_decode_mode((int)open_info.type);
			}
			ret = 0;
		}
		break;

	case VPU_CHECK_CODEC_STATUS:
	case VPU_CHECK_CODEC_STATUS_KERNEL:
		if (cmd == (unsigned int)VPU_CHECK_CODEC_STATUS_KERNEL) {
			(void)memcpy((int *)uarg.pi_data, jmgr_data.closed, sizeof(jmgr_data.closed));
		} else {
			if (copy_to_user((int *)uarg.pi_data, jmgr_data.closed, sizeof(jmgr_data.closed)) != 0U) {
				ret = -EFAULT;
			}
		}
		break;

	case VPU_GET_INSTANCE_IDX:
	case VPU_GET_INSTANCE_IDX_KERNEL:
		{
			INSTANCE_INFO iInst;

			if (cmd == (unsigned int)VPU_GET_INSTANCE_IDX_KERNEL) {
				(void)memcpy(&iInst, (int *)uarg.pi_data, sizeof(INSTANCE_INFO));
			} else {
				if (copy_from_user(&iInst, (int *)uarg.pi_data, sizeof(INSTANCE_INFO)) != 0U) {
					ret = -EFAULT;
				}
			}

			if (ret == 0) {
				if (iInst.type == VPU_ENC) {
					venc_get_instance(&iInst.nInstance);
				} else {
					vdec_get_instance(&iInst.nInstance);
				}

				if (cmd == (unsigned int)VPU_GET_INSTANCE_IDX_KERNEL) {

					(void)memcpy((int *)uarg.pi_data, &iInst, sizeof(INSTANCE_INFO));
				} else {
					if (copy_to_user((int *)uarg.pi_data, &iInst, sizeof(INSTANCE_INFO)) != 0U) {
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
				(void)memcpy(&iInst, (int *)uarg.pi_data, sizeof(INSTANCE_INFO));
			} else {
				if (copy_from_user(&iInst, (int *)uarg.pi_data, sizeof(INSTANCE_INFO)) != 0U) {
					ret = -EFAULT;
				}
			}

			if (ret == 0) {
				if (iInst.type == (int)VPU_ENC) {
					venc_clear_instance(iInst.nInstance);
				} else {
					vdec_clear_instance(iInst.nInstance);
				}
			}
		}
		break;

	case VPU_HW_RESET:
		jmgr_hw_reset();
		break;

	case VPU_TRY_FORCE_CLOSE:
	case VPU_TRY_FORCE_CLOSE_KERNEL:
		if (!jmgr_data.bVpu_already_proc_force_closed) {
			jmgr_data.external_proc = 1;
			(void)jmgr_external_all_close(200);
			jmgr_data.external_proc = 0;
			jmgr_data.bVpu_already_proc_force_closed = (bool)true;
		}
		break;

	case VPU_TRY_CLK_RESTORE:
	case VPU_TRY_CLK_RESTORE_KERNEL:
		jmgr_restore_clock(0, atomic_read(&jmgr_data.opened));
		break;

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	case VPU_TRY_OPEN_DEV:
	case VPU_TRY_OPEN_DEV_KERNEL:
		(void)jmgr_cmd_open("cmd");
		break;

	case VPU_TRY_CLOSE_DEV:
	case VPU_TRY_CLOSE_DEV_KERNEL:
		(void)jmgr_cmd_release("cmd");
		break;
#endif

	default:
		err_jpu("Unsupported ioctl[%d]!!!", cmd);
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&jmgr_data.comm_data.io_mutex);
	LOG_COVERITY("%p", filep);

	return ret;
}

#ifdef CONFIG_COMPAT
static long jmgr_compat_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	return jmgr_ioctl(filep, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static irqreturn_t jmgr_isr_handler(int irq, void *dev_id)
{

	detailk_jpu("%s\n", __func__);
	atomic_inc(&jmgr_data.oper_intr);
	wake_up_interruptible(&jmgr_data.oper_wq);
	LOG_COVERITY("%d%p", irq, dev_id);

	return IRQ_HANDLED;
}

static int jmgr_open(struct inode *pinode, struct file *filp)
{
	if (jmgr_data.irq_reged == 0U) {
		err_jpu("not registered jpu-mgr-irq\n");
	}

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	dprintk_jpu("%s In!! %d'th\n", __func__,
		atomic_read(&jmgr_data.dev_file_opened));
	atomic_inc(&jmgr_data.dev_file_opened);
	dprintk_jpu("%s Out!! %d'th\n", __func__,
		atomic_read(&jmgr_data.dev_file_opened));
#else
	mutex_lock(&jmgr_data.comm_data.file_mutex);
	(void)jmgr_cmd_open(jpu_fname_file);
	mutex_unlock(&jmgr_data.comm_data.file_mutex);
#endif

	filp->private_data = &jmgr_data;
	LOG_COVERITY("%p", pinode);

	return 0;
}

static int jmgr_release(struct inode *pinode, struct file *filp)
{
#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	dprintk_jpu("%s In!! %d'th\n", __func__, atomic_read(&jmgr_data.dev_file_opened));
	atomic_dec(&jmgr_data.dev_file_opened);
	jmgr_data.nOpened_Count++;

	(void)pr_info("vmgr_release Out!! %d'th, total = %d  - DEC(%d/%d/%d/%d/%d)",
		atomic_read(&jmgr_data.dev_file_opened),
		jmgr_data.nOpened_Count,
		jmgr_get_close(VPU_DEC),
		jmgr_get_close(VPU_DEC_EXT),
		jmgr_get_close(VPU_DEC_EXT2),
		jmgr_get_close(VPU_DEC_EXT3),
		jmgr_get_close(VPU_DEC_EXT4));
#else
	mutex_lock(&jmgr_data.comm_data.file_mutex);
	(void)jmgr_cmd_release(jpu_fname_file);
	mutex_unlock(&jmgr_data.comm_data.file_mutex);
#endif
	LOG_COVERITY("%p%p", pinode, filp);

	return 0;
}

struct VpuList *jmgr_list_manager(struct VpuList *args, unsigned int cmd)
{
	struct VpuList *ret = NULL;
	struct VpuList *oper_data = (struct VpuList *) args;

	if (oper_data == NULL) {
		if (cmd == (unsigned int)LIST_ADD || cmd == (unsigned int)LIST_DEL) {
			V_DBG(VPU_DBG_ERROR, "Data is null, cmd=%d", cmd);
			return NULL;
		}
	}

	if (cmd == (unsigned int)LIST_ADD) {
		*oper_data->vpu_result = RET0;
	}

	mutex_lock(&jmgr_data.comm_data.list_mutex);
	switch (cmd) {
	case LIST_ADD:
		*oper_data->vpu_result |= RET1;
		list_add_tail(&oper_data->list,
			&jmgr_data.comm_data.main_list);
		if(UINT_MAX - jmgr_data.cmd_queued > 1U) {
			jmgr_data.cmd_queued++;
		}
		if(UINT_MAX - jmgr_data.comm_data.thread_intr > 1U) {
			jmgr_data.comm_data.thread_intr++;
		}
		break;

	case LIST_DEL:
		list_del(&oper_data->list);
		if (jmgr_data.cmd_queued > 0) {
			jmgr_data.cmd_queued--;
		}
		break;

	case LIST_IS_EMPTY:
		if (list_empty(&jmgr_data.comm_data.main_list) != 0) {
			ret = &jmgr_vlist;
		}
		break;

	case LIST_GET_ENTRY:
		ret =
			list_first_entry(&jmgr_data.comm_data.main_list,
					 struct VpuList, list);
		break;
	default:
		/* Nothing to do */
		break;
	}
	mutex_unlock(&jmgr_data.comm_data.list_mutex);

	if (cmd == (unsigned int)LIST_ADD) {
		wake_up_interruptible(&jmgr_data.comm_data.thread_wq);
	}

	return ret;
}

static int jmgr_operation(void)
{
	int oper_finished;
	struct VpuList *oper_data = NULL;

	while (jmgr_list_manager(NULL, (unsigned int)LIST_IS_EMPTY) == NULL) {
		jmgr_data.cmd_processing = 1;
		oper_finished = 1;
		dprintk_jpu("%s :: not empty jmgr_data.cmd_queued(%d)",
			__func__, jmgr_data.cmd_queued);

		oper_data = jmgr_list_manager(NULL, (unsigned int)LIST_GET_ENTRY);
		if (oper_data == NULL) {
			err_jpu("data is null\n");
			jmgr_data.cmd_processing = 0;
			return 0;
		}
		*oper_data->vpu_result |= RET2;

		dprintk_jpu("%s [%d] :: cmd = 0x%x, cmd_queued(%d)",
			__func__,  oper_data->type,
			oper_data->cmd_type, jmgr_data.cmd_queued);

		if (oper_data->type < JPU_MAX) {
			*oper_data->vpu_result |= RET3;

			*oper_data->vpu_result
			 = jmgr_process((vputype)oper_data->type,
					oper_data->cmd_type,
					oper_data->handle,
					oper_data->args);
			oper_finished = 1;
			if (*oper_data->vpu_result != RETCODE_SUCCESS) {
				if ((*oper_data->vpu_result !=
					  RETCODE_INSUFFICIENT_BITSTREAM) &&
					(*oper_data->vpu_result !=
					  RETCODE_INSUFFICIENT_BITSTREAM_BUF)) {
					err_jpu(
					"jmgr_out[0x%x] :: type = %d, handle = 0x%x, cmd = 0x%x, frame_len %d\n",
					  *oper_data->vpu_result,
					  oper_data->type,
					  oper_data->handle,
					  oper_data->cmd_type,
					  jmgr_data.szFrame_Len);
				}

				if (*oper_data->vpu_result
					 == RETCODE_CODEC_EXIT) {
					jmgr_restore_clock(0,
						atomic_read(&jmgr_data.opened));
					jmgr_close_all(1);
				}
			}
		} else {
			dprintk_jpu(
			"%s :: missed info or unknown command => type = 0x%x, cmd = 0x%x,",
			  __func__, oper_data->type, oper_data->cmd_type);
			*oper_data->vpu_result = RETCODE_FAILURE;
			oper_finished = 0;
		}

		if (oper_finished != 0) {
			int opened = atomic_read(&jmgr_data.opened);
			if ((oper_data->comm_data != NULL)
				&& (opened != 0)) {
				oper_data->comm_data->count++;
				if (oper_data->comm_data->count != 1) {
					dprintk_jpu(
					  "poll wakeup count = %d :: type(0x%x) cmd(0x%x)",
					  oper_data->comm_data->count,
					  oper_data->type, oper_data->cmd_type);
				}

				wake_up_interruptible(
					&oper_data->comm_data->wq);
			} else {
				err_jpu(
				"Error: abnormal exception or external command was processed!! 0x%p - %d\n",
					oper_data->comm_data, opened);
			}
		} else {
			err_jpu(
			"Error: abnormal exception 2!! 0x%p - %d\n",
				oper_data->comm_data,
				atomic_read(&jmgr_data.opened));
		}

		(void)jmgr_list_manager((void *)oper_data, (unsigned int)LIST_DEL);

		jmgr_data.cmd_processing = 0;
	}

	return 0;
}

static int jmgr_thread(void *kthread)
{
	unsigned long jtimeout;

	dprintk_jpu("enter %s\n", __func__);

	jtimeout = msecs_to_jiffies(50);
	if (jtimeout > (unsigned long)LONG_MAX) {
		jtimeout = (unsigned long)LONG_MAX;
	}

	do {
		if (jmgr_list_manager(NULL, (unsigned int)LIST_IS_EMPTY) != NULL) {
			jmgr_data.cmd_processing = 0;

			(void)wait_event_interruptible_timeout(
				jmgr_data.comm_data.thread_wq,
				jmgr_data.comm_data.thread_intr > 0,
				(long)jtimeout);

			jmgr_data.comm_data.thread_intr = 0;
		} else {
			if ((atomic_read(&jmgr_data.opened) != 0)
				|| (jmgr_data.external_proc != 0U)) {
				(void)jmgr_operation();
			} else {
				struct VpuList *oper_data = NULL;

				err_jpu("DEL for empty\n");
				oper_data =
					jmgr_list_manager(NULL, (unsigned int)LIST_GET_ENTRY);
				if (oper_data != NULL) {
					(void)jmgr_list_manager(oper_data, (unsigned int)LIST_DEL);
				} else {
					LOG_COVERITY("%p", kthread);
				}
			}
		}
	} while (!kthread_should_stop());

	dprintk_jpu("finish %s\n", __func__);

	return 0;
}

static int jmgr_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret = 0;
	unsigned long current_vm_range = (vma->vm_end >= vma->vm_start) ?
		(vma->vm_end - vma->vm_start) : 0U;

#if defined(CONFIG_TCC_MEM)
	if (vma->vm_end < vma->vm_start ) {
		err_jpu("this address is not allowed");
		return -EAGAIN;
	}

	if (range_is_allowed(vma->vm_pgoff, current_vm_range) < 0) {
		err_jpu(KERN_ERR "%s: this address is not allowed\n", __func__);
		return -EAGAIN;
	}
#endif

	vma->vm_page_prot = vmem_get_pgprot(vma->vm_page_prot, vma->vm_pgoff);
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, current_vm_range, vma->vm_page_prot) != 0) {
		err_jpu("%s :: remap_pfn_range failed\n", __func__);
		ret = -EAGAIN;
		LOG_COVERITY("%p", &filp);
	} else {
		vma->vm_ops = NULL;
		vetc_vm_flags_set(vma, VM_IO | VM_DONTEXPAND | VM_PFNMAP);
	}

	return ret;
}

static const struct file_operations jmgr_fops = {
	.open = jmgr_open,
	.release = jmgr_release,
	.mmap = jmgr_mmap,
	.unlocked_ioctl = jmgr_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = jmgr_compat_ioctl,
#endif
};

static struct miscdevice jmgr_misc_device = {
	MISC_DYNAMIC_MINOR,
	JMGR_NAME,
	&jmgr_fops,
};

int jmgr_probe(struct platform_device *pdev)
{
	int ret;
	unsigned int type;
	unsigned long int_flags;
	struct resource *res = NULL;
	void *tTmpPtr = NULL;

	if (pdev->dev.of_node == NULL) {
		return -ENODEV;
	}

	dprintk_jpu("jmgr initializing!!");
	(void)memset(&jmgr_data, 0, sizeof(struct mgr_data_t));
	for (type = 0; type < (unsigned int)JPU_MAX; type++) {
		jmgr_data.closed[type] = 1;
	}

	jmgr_init_variable();
	atomic_set(&jmgr_data.oper_intr, 0);
#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	atomic_set(&jmgr_data.dev_file_opened, 0);
#endif

	ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		err_jpu("could not get IRQ");
		return -1;
	} else {
		jmgr_data.irq = (unsigned int)ret;
	}

	jmgr_data.nOpened_Count = 0;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "missing phy memory resource\n");
		return -1;
	}
	res->end += 1;

	jmgr_data.base_addr =
		devm_ioremap(&pdev->dev, res->start,
		  (res->end - res->start));
	dprintk_jpu(
		"============> JPU base address [0x%x -> 0x%p], irq num [%d]",
		res->start, jmgr_data.base_addr, ((long)jmgr_data.irq - 32));

	jmgr_get_clock(pdev->dev.of_node);
	jmgr_get_reset(pdev->dev.of_node);

	init_waitqueue_head(&jmgr_data.comm_data.thread_wq);
	init_waitqueue_head(&jmgr_data.oper_wq);

	mutex_init(&(jmgr_data.comm_data.list_mutex));
	mutex_init(&(jmgr_data.comm_data.io_mutex));
	mutex_init(&(jmgr_data.comm_data.file_mutex));

	INIT_LIST_HEAD(&jmgr_data.comm_data.main_list);
	INIT_LIST_HEAD(&jmgr_data.comm_data.wait_list);

	ret = vmem_config();
	if (ret < 0) {
		err_jpu("unable to configure memory for VPU!! %d\n", ret);
		return -ENOMEM;
	}

#if defined(USE_ACCESS_POINT)
	if (stJPUFunc == NULL) {
		ret = get_jpu_access_addr();
		if (ret != 0) {
			V_DBG(VPU_DBG_ERROR, "Getting for library access point failed!!");
			return RETCODE_FAILURE;
		}
	}
#endif

	jmgr_init_interrupt();
	int_flags = jmgr_get_int_flags();
	ret = jmgr_request_irq(jmgr_data.irq, jmgr_isr_handler, int_flags, JMGR_NAME, &jmgr_data);
	if (ret != 0) {
		err_jpu("to aquire jpu-dec-irq\n");
	}

	jmgr_data.irq_reged = 1U;
	jmgr_disable_irq(jmgr_data.irq);

	kidle_task_jpu = (struct task_struct *)kthread_run(jmgr_thread, NULL, "vJPU_th");
	VPU_CAST_PT(tTmpPtr, kidle_task_jpu);
	if (IS_ERR(tTmpPtr)) {
		err_jpu("unable to create thread!!");
		kidle_task_jpu = NULL;
		return -1;
	}
	dprintk_jpu("success :: thread created!!");

	jmgr_close_all(1);

	if (misc_register(&jmgr_misc_device) != 0) {
		err_jpu("JPU Manager: Couldn't register device.");
		return -EBUSY;
	}

	return 0;
}
EXPORT_SYMBOL(jmgr_probe);

int jmgr_remove(struct platform_device *pdev)
{
	misc_deregister(&jmgr_misc_device);

	if (kidle_task_jpu != NULL) {
		(void)kthread_stop(kidle_task_jpu);
		kidle_task_jpu = NULL;
	}

	devm_iounmap(&pdev->dev, jmgr_data.base_addr);
	if (jmgr_data.irq_reged > 0U) {
		jmgr_free_irq(jmgr_data.irq, &jmgr_data);
		jmgr_data.irq_reged = 0;
	}

	jmgr_put_clock();
	jmgr_put_reset();
	vmem_deinit();

	//(void)pr_info("success :: jmgr thread stopped!!\n");

	return 0;
}
EXPORT_SYMBOL(jmgr_remove);

#if defined(CONFIG_PM)
int jmgr_suspend(struct platform_device *pdev, pm_message_t state)
{
	int i, open_count = 0;

	if (atomic_read(&jmgr_data.opened) != 0) {
		(void)pr_info(
			"\n jpu: suspend In DEC(%d/%d/%d/%d/%d), ENC(%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d)\n",
			jmgr_get_close(VPU_DEC), jmgr_get_close(VPU_DEC_EXT),
			jmgr_get_close(VPU_DEC_EXT2), jmgr_get_close(VPU_DEC_EXT3),
			jmgr_get_close(VPU_DEC_EXT4),
			jmgr_get_close(VPU_ENC), jmgr_get_close(VPU_ENC_EXT),
			jmgr_get_close(VPU_ENC_EXT2), jmgr_get_close(VPU_ENC_EXT3),
			jmgr_get_close(VPU_ENC_EXT4), jmgr_get_close(VPU_ENC_EXT5),
			jmgr_get_close(VPU_ENC_EXT6), jmgr_get_close(VPU_ENC_EXT7),
			jmgr_get_close(VPU_ENC_EXT8), jmgr_get_close(VPU_ENC_EXT9),
			jmgr_get_close(VPU_ENC_EXT10), jmgr_get_close(VPU_ENC_EXT11),
			jmgr_get_close(VPU_ENC_EXT12), jmgr_get_close(VPU_ENC_EXT13),
			jmgr_get_close(VPU_ENC_EXT14), jmgr_get_close(VPU_ENC_EXT15)
		);

		(void)jmgr_external_all_close(200);

		open_count = atomic_read(&jmgr_data.opened);

		for (i = 0; i < open_count; i++) {
			jmgr_disable_clock();	//jmgr_disable_clock(0, 0);
		}
		(void)pr_info(
			"jpu: suspend Out DEC(%d/%d/%d/%d/%d), ENC(%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d)\n\n",
			jmgr_get_close(VPU_DEC), jmgr_get_close(VPU_DEC_EXT),
			jmgr_get_close(VPU_DEC_EXT2), jmgr_get_close(VPU_DEC_EXT3),
			jmgr_get_close(VPU_DEC_EXT4),
			jmgr_get_close(VPU_ENC), jmgr_get_close(VPU_ENC_EXT),
			jmgr_get_close(VPU_ENC_EXT2), jmgr_get_close(VPU_ENC_EXT3),
			jmgr_get_close(VPU_ENC_EXT4), jmgr_get_close(VPU_ENC_EXT5),
			jmgr_get_close(VPU_ENC_EXT6), jmgr_get_close(VPU_ENC_EXT7),
			jmgr_get_close(VPU_ENC_EXT8), jmgr_get_close(VPU_ENC_EXT9),
			jmgr_get_close(VPU_ENC_EXT10), jmgr_get_close(VPU_ENC_EXT11),
			jmgr_get_close(VPU_ENC_EXT12), jmgr_get_close(VPU_ENC_EXT13),
			jmgr_get_close(VPU_ENC_EXT14), jmgr_get_close(VPU_ENC_EXT15)
		);
	} else {
		LOG_COVERITY("%p%p", pdev, &state);
	}

	return 0;
}
EXPORT_SYMBOL(jmgr_suspend);

int jmgr_resume(struct platform_device *pdev)
{
	int i, open_count = 0;

	if (atomic_read(&jmgr_data.opened) != 0) {
		open_count = atomic_read(&jmgr_data.opened);

		for (i = 0; i < open_count; i++) {
			jmgr_enable_clock();	//jmgr_enable_clock(0, 0);
		}

		(void)pr_info("\n jpu: resume\n\n");
	} else {
		LOG_COVERITY("%p", pdev);
	}

	return 0;
}

EXPORT_SYMBOL(jmgr_resume);
#endif

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
MODULE_SOFTDEP("pre: vpu_lib jpu_lib hevc_lib vpu_4k_d2_lib vpu_hevc_enc_lib vpu");

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC jpu manager");
MODULE_LICENSE("GPL");

#endif
