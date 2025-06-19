// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_comm.h"

#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
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
#include <soc/telechips/pmap.h>
#endif

#include "vpu_buffer.h"
#include "vpu_devices.h"
#include "vpu_4k_d2_mgr_sys.h"
#include "vpu_4k_d2_mgr.h"
#include "vpu_4k_d2_mgr_flexio.h"

#define dprintk_4kd2(msg...)  V_DBG(VPU_DBG_INFO, "TCC_4K_D2_VMGR: " msg)
#define detailk_4kd2(msg...)  V_DBG(VPU_DBG_INFO, "TCC_4K_D2_VMGR: " msg)
#define cmdk_4kd2(msg...)     V_DBG(VPU_DBG_INFO, "TCC_4K_D2_VMGR [Cmd]: " msg)
#define err_4kd2(msg...)      V_DBG(VPU_DBG_ERROR, "TCC_4K_D2_VMGR [Err]: " msg)

// Enable the featue for future use
//#define VPU_4K_D2_REGISTER_DUMP
//#define VPU_4K_D2_DUMP_STATUS

#define DEBUG_VPU_4K_D2_K //To debug vpu drv in usersapce side (e.g. omx)

#if 0 //For test purpose!!
#define FORCED_ERROR
#endif
#ifdef FORCED_ERROR
#define FORCED_ERR_CNT 300
static int forced_error_count = FORCED_ERR_CNT;
#endif

#ifdef VPU_4K_D2_DUMP_STATUS
#define W5_REG_BASE                 0x0000
#define W5_BS_RD_PTR                0x0118
#define W5_BS_WR_PTR                0x011C
#define W5_BS_OPTION                0x0120
#define W5_CMD_BS_PARAM             0x0124

#define W4_VCPU_PDBG_RDATA_REG      0x001C
#define W5_VPU_FIO_CTRL_ADDR        0x0020
#define W5_VPU_FIO_DATA             0x0024
#endif

static char vpu_4kd2_api_version[] = VPU_4KD2_API_VERSION;

//static int32_t op_frm_num = 0;
static unsigned int cntInt_4kd2;	// = 0;

static struct VpuList vpu_4kd2_mgr_vlist;
static char vpu_4k_fname_file[] = "file";

#ifdef DEBUG_VPU_4K_D2_K
struct debug_4k_d2_k_isr_t {
	int ret_code_vmgr_hdr;
	unsigned int vpu_k_isr_cnt_hit;
	unsigned int wakeup_interrupt_cnt;
};
static struct debug_4k_d2_k_isr_t vpu_4k_d2_isr_param_debug;
static unsigned int cntwk_4kd2;	// = 0;
#endif

// Control only once!!
static struct mgr_data_t vmgr_4k_d2_data;
static struct task_struct *kidle_task_4kd2;	// = NULL;

#if defined(USE_ACCESS_POINT)
// SHARE_POINT_ORDER_XXX :
//    VPU = 0, JPU = 1, HEVC = 2,
//    4KD2 = 3, HEVC_ENC = 4, HEVC_ENC_2 = 5
#   define SHARE_POINT_ORDER_4KD2 3U

//Decoder
typedef int (*tccfp_vpu_4k_d2_dec_t)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
static tccfp_vpu_4k_d2_dec_t tcc_vpu_4k_d2_dec;
typedef int (*tccfp_vpu_4k_d2_dec_esc_t)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
static tccfp_vpu_4k_d2_dec_esc_t tcc_vpu_4k_d2_dec_esc;
typedef int (*tccfp_vpu_4k_d2_dec_ext_t)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
static tccfp_vpu_4k_d2_dec_ext_t tcc_vpu_4k_d2_dec_ext;

typedef struct st_vpu_4k_d2_func_t {
	unsigned int check_code1;
	int (*tccfp_vpu_4k_d2_dec)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	unsigned int check_code2;
	int (*tccfp_vpu_4k_d2_enc)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	unsigned int check_code3;
	int (*tccfp_vpu_4k_d2_dec_esc)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	int (*tccfp_vpu_4k_d2_dec_ext)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	unsigned int check_code4;
} st_vpu_4k_d2_func;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,10,0)
static st_vpu_4k_d2_func st4KD2FuncBase = {0, NULL, 0, NULL, 0, NULL, NULL, 0};
#endif

static st_vpu_4k_d2_func *st4KD2Func = NULL;

static int check_vpu_4k_d2_access_addr_valid(void)
{
	int ret = -1;

	if(((CHECK_CODE_01 | st4KD2Func->check_code1) == CHECK_CODE_01) &&
			((CHECK_CODE_02 | st4KD2Func->check_code2) == CHECK_CODE_02) &&
			((CHECK_CODE_03 | st4KD2Func->check_code3) == CHECK_CODE_03) &&
			((CHECK_CODE_04 | st4KD2Func->check_code4) == CHECK_CODE_04)) {
		ret = 0;
	} else {
		V_DBG(VPU_DBG_INFO, "VPU_4KD2 CheckCode %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
				GET_FOURCC_1((st4KD2Func->check_code1)),
				GET_FOURCC_2((st4KD2Func->check_code1)),
				GET_FOURCC_3((st4KD2Func->check_code1)),
				GET_FOURCC_4((st4KD2Func->check_code1)),
				GET_FOURCC_1((st4KD2Func->check_code2)),
				GET_FOURCC_2((st4KD2Func->check_code2)),
				GET_FOURCC_3((st4KD2Func->check_code2)),
				GET_FOURCC_4((st4KD2Func->check_code2)),
				GET_FOURCC_1((st4KD2Func->check_code3)),
				GET_FOURCC_2((st4KD2Func->check_code3)),
				GET_FOURCC_3((st4KD2Func->check_code3)),
				GET_FOURCC_4((st4KD2Func->check_code3)),
				GET_FOURCC_1((st4KD2Func->check_code4)),
				GET_FOURCC_2((st4KD2Func->check_code4)),
				GET_FOURCC_3((st4KD2Func->check_code4)),
				GET_FOURCC_4((st4KD2Func->check_code4))
			);
	}
	return ret;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
static int get_vpu_4k_d2_access_addr_file(void)
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

	filp = filp_open("/proc/4kd2", O_RDONLY, 0x1A4); //0644
	VPU_CAST_PT(tTmpPtr, filp);
	if(IS_ERR(tTmpPtr)) {
		V_DBG(VPU_DBG_ERROR, "/proc/4kd2 file open fail!!");
		ret = -1;
	} else {
		char data[20];
		unsigned long long res = 0;
		int idx = 0;

		idx = (sizeof(void*)*2) + 2;
#	if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
		ret = vfs_read(filp, data, sizeof(data), &filp->f_pos);
#	else
		ret = filp->f_op->read(filp, data, sizeof(data), &filp->f_pos);
#	endif

		data[idx] = '\0';

		ret = kstrtoull(data, 16, &res);

		(void)memmove((void*)&st4KD2Func, (void*)&res, sizeof(unsigned long));

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

static int get_vpu_4k_d2_access_addr_mem(void)
{
	int ret = 0;
	void *va = NULL;

	va = ioremap(SHARE_POINT_ADDR + (SHARD_POINT_GAP * SHARE_POINT_ORDER_4KD2), SHARD_POINT_GAP);

	if (va == NULL) {
		V_DBG(VPU_DBG_ERROR, "ioremap failed");
		ret = -ENOMEM;
	} else {
		memcpy(&st4KD2FuncBase, va, sizeof(st_vpu_4k_d2_func));

		st4KD2Func = &st4KD2FuncBase;

		V_DBG(VPU_DBG_INFO, "remap (PA : 0x%08x / VA : 0x%p) Dec ADDR : 0x%p",
				(SHARE_POINT_ADDR + (SHARD_POINT_GAP * SHARE_POINT_ORDER_4KD2)), va,
				st4KD2FuncBase.tccfp_vpu_4k_d2_dec);

		iounmap(va);
	}
	return ret;
}
#endif

static int get_vpu_4k_d2_access_addr(void)
{
	int ret = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,10,0)
	ret = get_vpu_4k_d2_access_addr_file();
#else
	ret = get_vpu_4k_d2_access_addr_mem();
#endif

	if (ret == 0) {
		ret = check_vpu_4k_d2_access_addr_valid();

		if (ret == 0) {
			tcc_vpu_4k_d2_dec = (tccfp_vpu_4k_d2_dec_t)st4KD2Func->tccfp_vpu_4k_d2_dec;
			tcc_vpu_4k_d2_dec_esc = (tccfp_vpu_4k_d2_dec_esc_t)st4KD2Func->tccfp_vpu_4k_d2_dec_esc;
			tcc_vpu_4k_d2_dec_ext =	(tccfp_vpu_4k_d2_dec_ext_t)st4KD2Func->tccfp_vpu_4k_d2_dec_ext;
		} else {
			ret = -1;
		}
	}

	return ret;
}


#else

extern int tcc_vpu_4k_d2_dec(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
extern int tcc_vpu_4k_d2_dec_esc(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
extern int tcc_vpu_4k_d2_dec_ext(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);

#endif //#if defined(USE_ACCESS_POINT)

static int tcc_vpu_4k_d2_dec_l(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2) VPU_NO_SANITIZE_CFI
{
	return tcc_vpu_4k_d2_dec(Op, pHandle, pParam1, pParam2);
}

int vmgr_4k_d2_opened(void)
{
	int ret = 1;

	if (atomic_read(&vmgr_4k_d2_data.opened) == 0) {
		ret = 0;
	}
	return ret;
}

#ifdef VPU_4K_D2_DUMP_STATUS
static unsigned int vpu_4k_d2mgr_FIORead(unsigned int addr)
{
	unsigned int ctrl;
	unsigned int count = 0;
	unsigned int data = 0xffffffff;

	ctrl = (addr & 0xffff);
	ctrl |= (0 << 16);	/* read operation */
	vetc_reg_write(vmgr_4k_d2_data.base_addr, W5_VPU_FIO_CTRL_ADDR, ctrl);
	count = 10000;
	while (count--) {
		ctrl = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_VPU_FIO_CTRL_ADDR);
		if (ctrl & 0x80000000) {
			data = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_VPU_FIO_DATA);
			break;
		}
	}

	return data;
}

static int vpu_4k_d2mgr_FIOWrite(unsigned int addr, unsigned int data)
{
	unsigned int ctrl;

	vetc_reg_write(vmgr_4k_d2_data.base_addr, W5_VPU_FIO_DATA, data);
	ctrl = (addr & 0xffff);
	ctrl |= (1 << 16);	/* write operation */
	vetc_reg_write(vmgr_4k_d2_data.base_addr, W5_VPU_FIO_CTRL_ADDR, ctrl);

	return 1;
}

static unsigned int vpu_4k_d2mgr_ReadRegVCE(unsigned int vce_addr)
{
#define VCORE_DBG_ADDR              0x8300
#define VCORE_DBG_DATA              0x8304
#define VCORE_DBG_READY             0x8308

	int vcpu_reg_addr;
	unsigned int udata = 0xffffffff;

	vpu_4k_d2mgr_FIOWrite(VCORE_DBG_READY, 0);

	vcpu_reg_addr = vce_addr >> 2;

	vpu_4k_d2mgr_FIOWrite(VCORE_DBG_ADDR, vcpu_reg_addr + 0x8000);

	while (1) {
		if (vpu_4k_d2mgr_FIORead(VCORE_DBG_READY) == 1) {
			udata = vpu_4k_d2mgr_FIORead(VCORE_DBG_DATA);
			break;
		}
	}

	return udata;
}

static void vpu_4k_d2_dump_status(void)
{
	unsigned int rd, wr;
	unsigned int tq, ip, mc, lf;
	unsigned int avail_cu, avail_tu, avail_tc, avail_lf, avail_ip;
	unsigned int ctu_fsm, nb_fsm, cabac_fsm, cu_info, mvp_fsm, tc_busy;
	unsigned int lf_fsm, bs_data, bbusy, fv;
	unsigned int reg_val;
	unsigned int index;
	unsigned int vcpu_reg[31] = { 0, };

	V_DBG(VPU_DBG_REG_DUMP,
	"-------------------------------------------------------------------------------");
	V_DBG(VPU_DBG_REG_DUMP,
	"------                            VCPU STATUS                             -----");
	V_DBG(VPU_DBG_REG_DUMP,
	"-------------------------------------------------------------------------------");

	rd = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_BS_RD_PTR);
	wr = vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_BS_WR_PTR);
	V_DBG(VPU_DBG_REG_DUMP,
	"RD_PTR: 0x%08x WR_PTR: 0x%08x BS_OPT: 0x%08x BS_PARAM: 0x%08x",
		rd, wr, vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_BS_OPTION),
		vetc_reg_read(vmgr_4k_d2_data.base_addr, W5_CMD_BS_PARAM));

	// --------- VCPU register Dump
	V_DBG(VPU_DBG_REG_DUMP, "[+] VCPU REG Dump");
	for (index = 0; index < 25; index++) {
		vetc_reg_write(vmgr_4k_d2_data.base_addr, 0x14,  (1 << 9) | (index & 0xff));
		vcpu_reg[index] = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x1c);

		if (index < 16) {
			V_DBG(VPU_DBG_REG_DUMP, "0x%08x\t", vcpu_reg[index]);
			if ((index % 4) == 3)
				V_DBG(VPU_DBG_REG_DUMP, "");
		} else {
			switch (index) {
			case 16:
				V_DBG(VPU_DBG_REG_DUMP,
					"CR0: 0x%08x", vcpu_reg[index]);
				break;
			case 17:
				V_DBG(VPU_DBG_REG_DUMP,
					"CR1: 0x%08x", vcpu_reg[index]);
				break;
			case 18:
				V_DBG(VPU_DBG_REG_DUMP,
					"ML:  0x%08x", vcpu_reg[index]);
				break;
			case 19:
				V_DBG(VPU_DBG_REG_DUMP,
					"MH:  0x%08x", vcpu_reg[index]);
				break;
			case 21:
				V_DBG(VPU_DBG_REG_DUMP,
					"LR:  0x%08x", vcpu_reg[index]);
				break;
			case 22:
				V_DBG(VPU_DBG_REG_DUMP,
					"PC:  0x%08x", vcpu_reg[index]);
				break;
			case 23:
				V_DBG(VPU_DBG_REG_DUMP,
					"SR:  0x%08x", vcpu_reg[index]);
				break;
			case 24:
				V_DBG(VPU_DBG_REG_DUMP,
					"SSP: 0x%08x", vcpu_reg[index]);
				break;
			}
		}
	}
	V_DBG(VPU_DBG_REG_DUMP, "[-] VCPU REG Dump");
	// --------- BIT register Dump
	V_DBG(VPU_DBG_REG_DUMP, "[+] BPU REG Dump");
	V_DBG(VPU_DBG_REG_DUMP, "BITPC = 0x%08x",
		vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x18)));
	V_DBG(VPU_DBG_REG_DUMP, "BIT START=0x%08x, BIT END=0x%08x",
		vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x11c)),
		vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x120)));

	V_DBG(VPU_DBG_REG_DUMP, "CODE_BASE           %x",
		vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x7000 + 0x18)));
	V_DBG(VPU_DBG_REG_DUMP, "VCORE_REINIT_FLAG   %x",
		vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x7000 + 0x0C)));

	// --------- BIT HEVC Status Dump
	ctu_fsm = vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x48));
	nb_fsm = vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x4c));
	cabac_fsm = vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x50));
	cu_info = vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x54));
	mvp_fsm = vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x58));
	tc_busy = vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x5c));
	lf_fsm = vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x60));
	bs_data = vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x64));
	bbusy = vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x68));
	fv = vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x6C));

	V_DBG(VPU_DBG_REG_DUMP,
	"[DEBUG-BPUHEVC] CTU_X: %4d, CTU_Y: %4d",
		vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x40)),
		vpu_4k_d2mgr_FIORead((W5_REG_BASE + 0x8000 + 0x44)));
	V_DBG(VPU_DBG_REG_DUMP,
	"[DEBUG-BPUHEVC] CTU_FSM>   Main: 0x%02x, FIFO: 0x%1x, NB: 0x%02x, DBK: 0x%1x",
		((ctu_fsm >> 24) & 0xff), ((ctu_fsm >> 16) & 0xff),
		((ctu_fsm >> 8) & 0xff), (ctu_fsm & 0xff));
	V_DBG(VPU_DBG_REG_DUMP, "[DEBUG-BPUHEVC] NB_FSM: 0x%02x",
		nb_fsm & 0xff);
	V_DBG(VPU_DBG_REG_DUMP,
	"[DEBUG-BPUHEVC] CABAC_FSM> SAO: 0x%02x, CU: 0x%02x, PU: 0x%02x, TU: 0x%02x, EOS: 0x%02x",
		((cabac_fsm >> 25) & 0x3f), ((cabac_fsm >> 19) & 0x3f),
		((cabac_fsm >> 13) & 0x3f), ((cabac_fsm >> 6) & 0x7f),
		(cabac_fsm & 0x3f));
	V_DBG(VPU_DBG_REG_DUMP,
	"[DEBUG-BPUHEVC] CU_INFO value = 0x%04x\n\t\t(l2cb: 0x%1x, cux: %1d, cuy; %1d, pred: %1d, pcm: %1d, wr_done: %1d, par_done: %1d, nbw_done: %1d, dec_run: %1d)",
		cu_info, ((cu_info >> 16) & 0x3),
		((cu_info >> 13) & 0x7), ((cu_info >> 10) & 0x7),
		((cu_info >> 9) & 0x3), ((cu_info >> 8) & 0x1),
		((cu_info >> 6) & 0x3), ((cu_info >> 4) & 0x3),
		((cu_info >> 2) & 0x3),  (cu_info & 0x3));
	V_DBG(VPU_DBG_REG_DUMP,
	"[DEBUG-BPUHEVC] MVP_FSM> 0x%02x", mvp_fsm & 0xf);
	V_DBG(VPU_DBG_REG_DUMP,
	"[DEBUG-BPUHEVC] TC_BUSY> tc_dec_busy: %1d, tc_fifo_busy: 0x%02x",
		((tc_busy >> 3) & 0x1), (tc_busy & 0x7));
	V_DBG(VPU_DBG_REG_DUMP,
	"[DEBUG-BPUHEVC] LF_FSM>  SAO: 0x%1x, LF: 0x%1x",
		((lf_fsm >> 4) & 0xf), (lf_fsm  & 0xf));
	V_DBG(VPU_DBG_REG_DUMP,
	"[DEBUG-BPUHEVC] BS_DATA> ExpEnd=%1d, bs_valid: 0x%03x, bs_data: 0x%03x",
		((bs_data >> 31) & 0x1), ((bs_data >> 16) & 0xfff),
		(bs_data & 0xfff));
	V_DBG(VPU_DBG_REG_DUMP,
	"[DEBUG-BPUHEVC] BUS_BUSY> mib_wreq_done: %1d, mib_busy: %1d, sdma_bus: %1d",
		((bbusy >> 2) & 0x1), ((bbusy >> 1) & 0x1), (bbusy & 0x1));
	V_DBG(VPU_DBG_REG_DUMP,
	"[DEBUG-BPUHEVC] FIFO_VALID> cu: %1d, tu: %1d, iptu: %1d, lf: %1d, coff: %1d",
		((fv >> 4) & 0x1), ((fv >> 3) & 0x1), ((fv >> 2) & 0x1),
		((fv >> 1) & 0x1), (fv & 0x1));
	V_DBG(VPU_DBG_REG_DUMP, "[-] BPU REG Dump");

	// --------- VCE register Dump
	V_DBG(VPU_DBG_REG_DUMP, "[+] VCE REG Dump");
	tq = vpu_4k_d2mgr_ReadRegVCE(0xd0);
	ip = vpu_4k_d2mgr_ReadRegVCE(0xd4);
	mc = vpu_4k_d2mgr_ReadRegVCE(0xd8);
	lf = vpu_4k_d2mgr_ReadRegVCE(0xdc);
	avail_cu =
		(vpu_4k_d2mgr_ReadRegVCE(0x11C) >> 16) -
		(vpu_4k_d2mgr_ReadRegVCE(0x110) >> 16);
	avail_tu =
		(vpu_4k_d2mgr_ReadRegVCE(0x11C) & 0xFFFF) -
		(vpu_4k_d2mgr_ReadRegVCE(0x110) & 0xFFFF);
	avail_tc =
		(vpu_4k_d2mgr_ReadRegVCE(0x120) >> 16) -
		(vpu_4k_d2mgr_ReadRegVCE(0x114) >> 16);
	avail_lf =
		(vpu_4k_d2mgr_ReadRegVCE(0x120) & 0xFFFF) -
		(vpu_4k_d2mgr_ReadRegVCE(0x114) & 0xFFFF);
	avail_ip =
		(vpu_4k_d2mgr_ReadRegVCE(0x124) >> 16) -
		(vpu_4k_d2mgr_ReadRegVCE(0x118) >> 16);
	V_DBG(VPU_DBG_REG_DUMP,
	"       TQ            IP              MC             LF      GDI_EMPTY          ROOM");
	V_DBG(VPU_DBG_REG_DUMP,
	"------------------------------------------------------------------------------------------------------------");
	V_DBG(VPU_DBG_REG_DUMP,
	"| %d %04d %04d | %d %04d %04d |  %d %04d %04d | %d %04d %04d | 0x%08x | CU(%d) TU(%d) TC(%d) LF(%d) IP(%d)",
		(tq>>22)&0x07, (tq>>11)&0x3ff, tq&0x3ff,
		(ip>>22)&0x07, (ip>>11)&0x3ff, ip&0x3ff,
		(mc>>22)&0x07, (mc>>11)&0x3ff, mc&0x3ff,
		(lf>>22)&0x07, (lf>>11)&0x3ff, lf&0x3ff,
		vpu_4k_d2mgr_FIORead(0x88f4),
		/* GDI empty */
		avail_cu, avail_tu, avail_tc, avail_lf, avail_ip);

	/* CU/TU Queue count */
	reg_val = vpu_4k_d2mgr_ReadRegVCE(0x12C);
	V_DBG(VPU_DBG_REG_DUMP, "[DCIDEBUG] QUEUE COUNT: CU(%5d) TU(%5d) ",
		(reg_val>>16)&0xffff, reg_val&0xffff);
	reg_val = vpu_4k_d2mgr_ReadRegVCE(0x1A0);
	V_DBG(VPU_DBG_REG_DUMP,
		"TC(%5d) IP(%5d) ", (reg_val>>16)&0xffff, reg_val&0xffff);
	reg_val = vpu_4k_d2mgr_ReadRegVCE(0x1A4);
	V_DBG(VPU_DBG_REG_DUMP, "LF(%5d)", (reg_val>>16)&0xffff);
	V_DBG(VPU_DBG_REG_DUMP,
	"VALID SIGNAL : CU0(%d)  CU1(%d)  CU2(%d) TU(%d) TC(%d) IP(%5d) LF(%5d)\n"
	"               DCI_FALSE_RUN(%d) VCE_RESET(%d) CORE_INIT(%d) SET_RUN_CTU(%d)",
		(reg_val>>6)&1, (reg_val>>5)&1, (reg_val>>4)&1,
		(reg_val>>3)&1, (reg_val>>2)&1, (reg_val>>1)&1,
		(reg_val>>0)&1,
		(reg_val>>10)&1, (reg_val>>9)&1,
		(reg_val>>8)&1, (reg_val>>7)&1);

	V_DBG(VPU_DBG_REG_DUMP,
	"State TQ: 0x%08x IP: 0x%08x MC: 0x%08x LF: 0x%08x",
		vpu_4k_d2mgr_ReadRegVCE(0xd0), vpu_4k_d2mgr_ReadRegVCE(0xd4),
		vpu_4k_d2mgr_ReadRegVCE(0xd8), vpu_4k_d2mgr_ReadRegVCE(0xdc));
	V_DBG(VPU_DBG_REG_DUMP, "BWB[1]: RESPONSE_CNT(0x%08x) INFO(0x%08x)",
		vpu_4k_d2mgr_ReadRegVCE(0x194),
		vpu_4k_d2mgr_ReadRegVCE(0x198));
	V_DBG(VPU_DBG_REG_DUMP, "BWB[2]: RESPONSE_CNT(0x%08x) INFO(0x%08x)",
		vpu_4k_d2mgr_ReadRegVCE(0x194),
		vpu_4k_d2mgr_ReadRegVCE(0x198));
	V_DBG(VPU_DBG_REG_DUMP, "DCI INFO");
	V_DBG(VPU_DBG_REG_DUMP, "READ_CNT_0 : 0x%08x",
		vpu_4k_d2mgr_ReadRegVCE(0x110));
	V_DBG(VPU_DBG_REG_DUMP, "READ_CNT_1 : 0x%08x",
		vpu_4k_d2mgr_ReadRegVCE(0x114));
	V_DBG(VPU_DBG_REG_DUMP, "READ_CNT_2 : 0x%08x",
		vpu_4k_d2mgr_ReadRegVCE(0x118));
	V_DBG(VPU_DBG_REG_DUMP, "WRITE_CNT_0: 0x%08x",
		vpu_4k_d2mgr_ReadRegVCE(0x11c));
	V_DBG(VPU_DBG_REG_DUMP, "WRITE_CNT_1: 0x%08x",
		vpu_4k_d2mgr_ReadRegVCE(0x120));
	V_DBG(VPU_DBG_REG_DUMP, "WRITE_CNT_2: 0x%08x",
		vpu_4k_d2mgr_ReadRegVCE(0x124));
	reg_val = vpu_4k_d2mgr_ReadRegVCE(0x128);
	V_DBG(VPU_DBG_REG_DUMP, "LF_DEBUG_PT: 0x%08x", reg_val & 0xffffffff);
	V_DBG(VPU_DBG_REG_DUMP,
	"cur_main_state %2d, r_lf_pic_deblock_disable %1d, r_lf_pic_sao_disable %1d",
		(reg_val >> 16) & 0x1f, (reg_val >> 15) & 0x1,
		(reg_val >> 14) & 0x1);
	V_DBG(VPU_DBG_REG_DUMP,
	"para_load_done %1d, i_rdma_ack_wait %1d, i_sao_intl_col_done %1d, i_sao_outbuf_full %1d",
		(reg_val >> 13) & 0x1, (reg_val >> 12) & 0x1,
		(reg_val >> 11) & 0x1, (reg_val >> 10) & 0x1);
	V_DBG(VPU_DBG_REG_DUMP,
	"lf_sub_done %1d, i_wdma_ack_wait %1d, lf_all_sub_done %1d, cur_ycbcr %1d, sub8x8_done %2d",
		(reg_val >> 9) & 0x1, (reg_val >> 8) & 0x1,
		(reg_val >> 6) & 0x1, (reg_val >> 4) & 0x1,
		reg_val & 0xf);
	V_DBG(VPU_DBG_REG_DUMP, "[-] VCE REG Dump");
	V_DBG(VPU_DBG_REG_DUMP, "[-] VCE REG Dump");

	V_DBG(VPU_DBG_REG_DUMP,
	"-------------------------------------------------------------------------------");

	{
		unsigned int q_cmd_done_inst, stage0_inst_info,
			stage1_inst_info, stage2_inst_info, dec_seek_cycle,
			dec_parsing_cycle, dec_decoding_cycle;
		q_cmd_done_inst
		 = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x01E8);
		stage0_inst_info
		 = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x01EC);
		stage1_inst_info
		 = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x01F0);
		stage2_inst_info
		 = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x01F4);
		dec_seek_cycle
		 = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x01C0);
		dec_parsing_cycle
		 = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x01C4);
		dec_decoding_cycle
		 = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x01C8);
		V_DBG(VPU_DBG_REG_DUMP, "W5_RET_QUEUE_CMD_DONE_INST : 0x%08x",
			   q_cmd_done_inst);
		V_DBG(VPU_DBG_REG_DUMP, "W5_RET_STAGE0_INSTANCE_INFO : 0x%08x",
			   stage0_inst_info);
		V_DBG(VPU_DBG_REG_DUMP, "W5_RET_STAGE1_INSTANCE_INFO : 0x%08x",
			   stage1_inst_info);
		V_DBG(VPU_DBG_REG_DUMP, "W5_RET_STAGE2_INSTANCE_INFO : 0x%08x",
			   stage2_inst_info);
		V_DBG(VPU_DBG_REG_DUMP, "W5_RET_DEC_SEEK_CYCLE : 0x%08x",
			dec_seek_cycle);
		V_DBG(VPU_DBG_REG_DUMP, "W5_RET_DEC_PARSING_CYCLE : 0x%08x",
			   dec_parsing_cycle);
		V_DBG(VPU_DBG_REG_DUMP, "W5_RET_DEC_DECODING_CYCLE : 0x%08x",
			   dec_decoding_cycle);
	}

	V_DBG(VPU_DBG_REG_DUMP,
	"-------------------------------------------------------------------------------");

	/* SDMA & SHU INFO */
	// -----------------------------------------
	// SDMA registers
	// -----------------------------------------

	{
		//DECODER SDMA INFO
		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5000);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_LOAD_CMD    = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5004);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_AUTO_MOD  = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5008);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_START_ADDR  = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x500C);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_END_ADDR   = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5010);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_ENDIAN     = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5014);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_IRQ_CLEAR  = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5018);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_BUSY       = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x501C);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_LAST_ADDR  = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5020);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_SC_BASE_ADDR  = 0x%x", reg_val);

		// -------------------------------------------
		// SHU registers
		// -------------------------------------------

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5400);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_INIT = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5404);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SEEK_NXT_NAL = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5408);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_RD_NAL_ADDR = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x540c);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_STATUS = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5410);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_1 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5414);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_2 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5418);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_3 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x541c);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_4 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5420);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_5 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5424);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_6 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5428);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_7 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x542c);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_8 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5430);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SBYTE_LOW = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5434);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SBYTE_HIGH = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5438);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_ST_PAT_DIS = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5440);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF0_0 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5444);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF0_1 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5448);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF0_2 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x544c);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF0_3 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5450);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF1_0 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5454);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF1_1 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5458);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF1_2 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x545c);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF1_3 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5460);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF2_0 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5464);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF2_1 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5468);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF2_", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x546c);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF2_3 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5470);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_NBUF_RPTR = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5474);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_NBUF_WPTR = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5478);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_REMAIN_BYTE = 0x%x", reg_val);

		// -----------------------------------------
		// GBU registers
		// -----------------------------------------

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5800);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_INIT = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5804);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_STATUS = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5808);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_TCNT = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x580c);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_TCNT = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5c10);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF0_LOW = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5c14);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF0_HIGH = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5c18);

		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF1_LOW = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5c1c);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF1_HIGH = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5c20);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF2_LOW = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5c24);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF2_HIGH = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5c30);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF_RPTR = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5c34);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF_WPTR = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(vmgr_4k_d2_data.base_addr,
				  W5_REG_BASE + 0x5c28);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_REMAIN_BIT = 0x%x", reg_val);
	}
}
#endif

int vmgr_4k_d2_get_close(vputype type)
{
	return vmgr_4k_d2_data.closed[type];
}

int vmgr_4k_d2_get_alive(void)
{
	return atomic_read(&vmgr_4k_d2_data.opened);
}

int vmgr_4k_d2_set_close(vputype type, int value, int bfreemem)
{
	int ret = 0;

	if (vmgr_4k_d2_get_close(type) == value) {
		dprintk_4kd2(" %d was already set into %d.", type, value);
		ret = -1;
	} else {
		vmgr_4k_d2_data.closed[type] = value;
		if (value == 1) {
			vmgr_4k_d2_data.handle[type] = 0x00;
			if (bfreemem != 0) {
				(void)vmem_proc_free_memory(type);
			}
		}
	}

	return ret;
}

static void vmgr_4k_d2_close_all(int bfreemem)
{
	(void)vmgr_4k_d2_set_close(VPU_DEC, 1, bfreemem);
	(void)vmgr_4k_d2_set_close(VPU_DEC_EXT, 1, bfreemem);
	(void)vmgr_4k_d2_set_close(VPU_DEC_EXT2, 1, bfreemem);
	(void)vmgr_4k_d2_set_close(VPU_DEC_EXT3, 1, bfreemem);
	(void)vmgr_4k_d2_set_close(VPU_DEC_EXT4, 1, bfreemem);
}

int vmgr_4k_d2_process_ex(struct VpuList *cmd_list, vputype type, int Op, int *result)
{
	int ret = 1;

	if (atomic_read(&vmgr_4k_d2_data.opened) != 0) {
		err_4kd2(" process_ex %d - 0x%x", type, Op);

		if ((type < (vputype)0) || (type >= (vputype)VPU_MAX)) {
			ret = 0;
		} else {
			if (vmgr_4k_d2_get_close(type) == 0) {
				cmd_list->type = (unsigned int)type;
				cmd_list->cmd_type = Op;
				cmd_list->handle = vmgr_4k_d2_data.handle[(unsigned int)type];
				cmd_list->args = NULL;
				cmd_list->comm_data = NULL;
				cmd_list->vpu_result = result;
				(void)vmgr_4k_d2_list_manager(cmd_list, (int)LIST_ADD);
				ret = 1;
			}
		}
	} else {
		ret = 0;
	}

	return ret;
}

static unsigned int oper_inst_reason_met(unsigned int reason, unsigned int vint_reason)
{
	unsigned int oper_inst = 0;
	oper_inst = atomic_read(&vmgr_4k_d2_data.oper_intr);
	oper_inst |= vint_reason;

	return (oper_inst & reason);
}

static int vmgr_4k_d2_internal_handler(unsigned int reason)
{
	long ret;
	int ret_code = RETCODE_INTR_DETECTION_NOT_ENABLED;
	unsigned int vint_reason = 0;
	unsigned int oper_inst = 0;
	int cnt = 0;
	int timeout = 500;
	unsigned long jtimeout;

	jtimeout = msecs_to_jiffies(5);
	if (jtimeout > (ULONG_MAX / 2UL)) {
		jtimeout = ((ULONG_MAX / 2UL) - 1UL);
	}

	if (vmgr_4k_d2_data.check_interrupt_detection) {
		oper_inst = atomic_read(&vmgr_4k_d2_data.oper_intr);
		if ((oper_inst & reason) > 0U) {
			detailk_4kd2("Success 1: vpu-4k-d2 vp9/hevc operation!!");
			ret_code = RETCODE_SUCCESS;
		} else {
			for (cnt = 0; cnt < timeout; cnt += 5) {
				ret = wait_event_interruptible_timeout(vmgr_4k_d2_data.oper_wq, oper_inst_reason_met(reason, vint_reason), (long)jtimeout);

				if (ret == 0 /*TIMEOUT*/)
				{
					vint_reason = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x004C);
				} else if (ret >= 0) {
					oper_inst = atomic_read(&vmgr_4k_d2_data.oper_intr);
				} else {
					//-ERESTARTSYS(Interrupted by a signal)
					err_4kd2("ERESTARTSYS");
					break;
				}

				if ((oper_inst & reason) > 0) {
					detailk_4kd2("Success 2: vpu-4k-d2 vp9/hevc operation!!");
#if defined(FORCED_ERROR)
					if (forced_error_count-- <= 0) {
						ret_code = RETCODE_CODEC_EXIT;
						forced_error_count
						= FORCED_ERR_CNT;
					} else {
#endif
						ret_code = RETCODE_SUCCESS;
						break;
#if defined(FORCED_ERROR)
					}
#endif
				}
			}

			if (cnt >= timeout) {
				err_4kd2(
				"[CMD 0x%x][%d]: vpu-4k-d2 vp9/hevc timed_out(ref %d msec) => oper_intr[%d]!! [%d]th frame len %d",
					vmgr_4k_d2_data.current_cmd, ret,
					timeout,
					vmgr_4k_d2_data.oper_intr,
					vmgr_4k_d2_data.nDecode_Cmd,
					vmgr_4k_d2_data.szFrame_Len);
				ret_code = RETCODE_CODEC_EXIT;
			}
		}
	}

	if (ret_code == RETCODE_SUCCESS) {
		atomic_andnot(reason, &vmgr_4k_d2_data.oper_intr);
	}

	#ifdef DEBUG_VPU_4K_D2_K
	vpu_4k_d2_isr_param_debug.ret_code_vmgr_hdr = ret_code;
	dprintk_4kd2(
	"[%s] : ret_code (%d), cntInt_4kd2 (%d), cntwk_4kd2 (%d), vmgr_4k_d2_data.oper_intr (%d)",
		__func__, ret_code, cntInt_4kd2, cntwk_4kd2,
		vmgr_4k_d2_data.oper_intr);
	#endif

	V_DBG(VPU_DBG_INTERRUPT, "out (Interrupt option=%d, isr cnt=%d, ev=%d)",
		vmgr_4k_d2_data.check_interrupt_detection,
		cntInt_4kd2,
		ret_code);

	return ret_code;
}

/**
 * Checks the API version of the VPU_4K_D2 library.
 *
 * @return 0 if the API version matches, a positive value if there's a version
 *         mismatch, or a negative value if an error occurs during the check.
 */
static int check_vpu4kd2_lib_api_version(void)
{
	int ret = 0;

	// Prepare the input structure for the VPU_4KD2_GET_VERSION command
	vpu_4K_D2_dec_get_version_t getVersion = {0};
	getVersion.pszHeaderApiVersion = vpu_4kd2_api_version;

	// Issue the VPU_4KD2_GET_VERSION command to retrieve the VPU_4K_D2 library version
	ret = tcc_vpu_4k_d2_dec_l(VPU_4KD2_GET_VERSION,
							NULL,
							(void *)(&getVersion),
							(void *)NULL);

	// Check the return value of the VPU_4KD2_GET_VERSION command
	if (ret > 0) {
		if (ret == RETCODE_API_VERSION_MISMATCH) {
			// The API version doesn't match, print an error message
			V_DBG(VPU_DBG_ERROR, "API version (%s) mismatched. Please check your VPU_4K_D2 API header version.", VPU_4KD2_API_VERSION);
			return ret;
		}
	} else {
		// The VPU library version is retrieved successfully, print the version information
		V_DBG(VPU_DBG_INFO, "[VPU_DRV] VPU_4K_D2 Version   : %s", getVersion.szGetVersion);
		V_DBG(VPU_DBG_INFO, "[VPU_DRV] VPU_4K_D2 Build Date: %s", getVersion.szGetBuildDate);
	}

	return ret;
}
///////////////////////////////////////////////////////////////////////////////
static int vmgr_4k_d2_process(vputype type, int cmd, long pHandle, void *args)
{
	int ret = 0;

#ifdef CONFIG_VPU_TIME_MEASUREMENT
	long long startTime, endTime;
	long long time_gap_us = 0LL;
#endif

	vmgr_4k_d2_data.check_interrupt_detection = 0;
	vmgr_4k_d2_data.current_cmd = cmd;

	if (type < VPU_ENC) {
		if (cmd != VPU_DEC_INIT &&
			cmd != VPU_DEC_INIT_KERNEL &&
			cmd != V2D_IP_DRV_INI)
		{
			if (vmgr_4k_d2_get_close(type)
				|| (vmgr_4k_d2_data.handle[type] == 0x00)) {
				return RETCODE_MULTI_CODEC_EXIT_TIMEOUT;
			}
		}

		if (cmd != VPU_DEC_BUF_FLAG_CLEAR
			&& cmd != VPU_DEC_DECODE
			&& cmd != VPU_DEC_BUF_FLAG_CLEAR_KERNEL
			&& cmd != VPU_DEC_DECODE_KERNEL) {
			cmdk_4kd2("Decoder(%d), command: 0x%x", type, cmd);
		}

		switch (cmd) {
		case VPU_DEC_INIT:
		case VPU_DEC_INIT_KERNEL:
		case V2D_IP_DRV_INI:
		{
			VPU_4K_D2_INIT_t *arg = NULL;
			union_codec_handle_t codec_handle;
			bool isFlexible = (cmd == V2D_IP_DRV_INI);

#if !defined(CONFIG_ANDROID)
			init_vdbg_log();
#endif
			ret = check_vpu4kd2_lib_api_version();
			if (ret != 0) {
				V_DBG(VPU_DBG_ERROR, "[%s %4d][VPU_DEC_INIT] check_vpu4kd2_lib_api_version failed...", __FILE__, __LINE__);
				return ret;
			}

			if (isFlexible) {
				arg = (VPU_4K_D2_INIT_t *) v24kd2mgr_unmarshal_ip_inidata(args);
				cmd = VPU_DEC_INIT;
			} else {
				arg = (VPU_4K_D2_INIT_t *) args;
			}
			if (arg == NULL) {
				ret = RETCODE_FAILURE;
			}
			else
			{
				(void)pr_info("%s: cmd(%d), handle(%p)", __func__, cmd, arg);
				vmgr_4k_d2_data.handle[type] = 0x00;

				arg->gsV4kd2DecInit.m_RegBaseVirtualAddr
					= (codec_addr_t)vmgr_4k_d2_data.base_addr;
				arg->gsV4kd2DecInit.m_Memcpy
				 = (void *(*) (void *dest, const void* src,
					unsigned int count, unsigned int types))vetc_memcpy;
				arg->gsV4kd2DecInit.m_Memset
				 = (void (*) (void *ptr, int values,
					  unsigned int num, unsigned int types))vetc_memset;
				arg->gsV4kd2DecInit.m_Interrupt
				 = (int (*) (unsigned int reason))vmgr_4k_d2_internal_handler;
				arg->gsV4kd2DecInit.m_Ioremap
				 = (void *(*) (phys_addr_t phy_addr, unsigned int size))vetc_ioremap;
				arg->gsV4kd2DecInit.m_Iounmap
				 = (void (*) (void *virt_addr))vetc_iounmap;
				arg->gsV4kd2DecInit.m_reg_read
				= (unsigned int (*)(void *base_addr, unsigned int offset))vetc_reg_read;
				arg->gsV4kd2DecInit.m_reg_write
				 = (void (*)(void *base_addr, unsigned int offset,
					 unsigned int data))vetc_reg_write;
				arg->gsV4kd2DecInit.m_Usleep
				= (void (*)(unsigned int uimin, unsigned int uimax))vetc_usleep;

				vmgr_4k_d2_data.check_interrupt_detection = 1;

				vmgr_4k_d2_data.bDiminishInputCopy
				= (arg->gsV4kd2DecInit.m_uiDecOptFlags
					& (unsigned int)(0x4000000)) ? (bool)true : (bool)false; //(1 << 26)

				if ((vetc_get_chip_family() == 0x8050U)
						&& (vetc_get_chip_rev() == 0U) /*replaced from system_rev*/
						&& (arg->gsV4kd2DecInit.m_Reserved[10] == 10U)) {
					arg->gsV4kd2DecInit.m_uiDecOptFlags
						|= WAVE5_WTL_ENABLE; //disable map converter
					V_DBG(VPU_DBG_INFO,
					"Dec :: Init In => enable WTL (off the compressed output mode)");
					// to notify this refusal
					arg->gsV4kd2DecInit.m_Reserved[10] = 5;

					//[work-around] reduce total memory size
					// of min. frame buffers
					// Max Bitdepth
					arg->gsV4kd2DecInit.m_Reserved[5] = 8;
					arg->gsV4kd2DecInit.m_uiDecOptFlags
					|= (1 << 3);	// 10 to 8 bit shit
				}


				dprintk_4kd2(
				"Dec[%d] :: Init In => workbuff 0x%llx/0x%llx, Reg: 0x%p/0x%llx, format : %d, Stream(0x%llx/0x%llx, 0x%x)",
					type,
					arg->gsV4kd2DecInit.m_BitWorkAddr[PA],
					arg->gsV4kd2DecInit.m_BitWorkAddr[VA],
					vmgr_4k_d2_data.base_addr,
					arg->gsV4kd2DecInit.m_RegBaseVirtualAddr,
					arg->gsV4kd2DecInit.m_iBitstreamFormat,
					arg->gsV4kd2DecInit.m_BitstreamBufAddr[PA],
					arg->gsV4kd2DecInit.m_BitstreamBufAddr[VA],
					arg->gsV4kd2DecInit.m_iBitstreamBufSize);

				dprintk_4kd2(
				"Dec[%d] :: Init In => optFlag 0x%x, Userdata(%d), Inter: %d, PlayEn: %d",
					type,
					arg->gsV4kd2DecInit.m_uiDecOptFlags,
					arg->gsV4kd2DecInit.m_bEnableUserData,
					arg->gsV4kd2DecInit.m_bCbCrInterleaveMode,
					arg->gsV4kd2DecInit.m_iFilePlayEnable);

#if defined(USE_ACCESS_POINT)
				if (check_vpu_4k_d2_access_addr_valid() != 0) {
					err_4kd2(
						"Dec-%d Access address envalid!!(%d)",
						type, check_vpu_4k_d2_access_addr_valid());

					return RETCODE_FAILURE;
				}
#endif

				if (vmem_alloc_count((int)type) <= 0) {
					err_4kd2(
					"Dec-%d No Buffer allocation", type);
					return RETCODE_FAILURE;
				}

#ifdef VPU_LIB_WRITE_OUTPUT_TO_YUV
				if ((module_param_vdbg_lib & VLOG_MASK_LIB_WRITE_OUTPUT_TO_YUV)
						== VLOG_MASK_LIB_WRITE_OUTPUT_TO_YUV)
				{
					if ((arg->gsV4kd2DecInit.m_uiDecOptFlags & WAVE5_WTL_ENABLE) != WAVE5_WTL_ENABLE) {
						arg->gsV4kd2DecInit.m_uiDecOptFlags |= WAVE5_WTL_ENABLE;
						V_DBG(VPU_DBG_ERROR, "[%s %4d][vdbg] force enable wtl", __FILE__, __LINE__);
					}
					if ((arg->gsV4kd2DecInit.m_uiDecOptFlags & WAVE5_WTL_ENABLE) == WAVE5_WTL_ENABLE) {
						int bit_depth = 10;
						if ((arg->gsV4kd2DecInit.m_uiDecOptFlags & WAVE5_10BITS_DISABLE) == WAVE5_10BITS_DISABLE) {
							bit_depth = 8;
						}
						V_DBG(VPU_DBG_ERROR, "[%s %4d][vdbg] ready to write yuv to file (%d/%d/%d)", __FILE__, __LINE__,
							arg->gsV4kd2DecInit.m_iBitstreamFormat, arg->gsV4kd2DecInit.m_bCbCrInterleaveMode, bit_depth);
						init_vdbg_save_yuv(arg->gsV4kd2DecInit.m_iBitstreamFormat, arg->gsV4kd2DecInit.m_bCbCrInterleaveMode, bit_depth);
					}
				}
#endif
				// Temporary code
				if ((module_param_vdbg_lib & VLOG_MASK_LIB_MEASURE_PERF) == VLOG_MASK_LIB_MEASURE_PERF)
				{
					V_DBG(VPU_DBG_ERROR, "[%s %4d][VPU_DEC_INIT] enable flag - VLOG_MASK_LIB_MEASURE_PERF", __FILE__, __LINE__);
					arg->gsV4kd2DecInit.m_Reserved[2] = 0x1000;
				}

				codec_handle.pcodec_handle = &arg->gsV4kd2DecHandle;

				ret =
					tcc_vpu_4k_d2_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
					(vcodec_handle_t *)(codec_handle.pvcodec_handle),
					(void *)(&arg->gsV4kd2DecInit),
					(void *)NULL);
				if (ret != RETCODE_SUCCESS) {
					V_DBG(VPU_DBG_SEQUENCE,
					"Dec :: Init Done with ret(0x%x)", ret);
				} else {
					if (isFlexible) {
						v24kd2mgr_marshal_op_inidata(args);
					}
				}

				if (ret != RETCODE_CODEC_EXIT
					&& arg->gsV4kd2DecHandle != 0) {
					vmgr_4k_d2_data.handle[type] =
						arg->gsV4kd2DecHandle;
					(void)vmgr_4k_d2_set_close(type, 0, 0);
					cmdk_4kd2(
					"Dec :: vmgr_4k_d2_data.handle = 0x%x",
						arg->gsV4kd2DecHandle);
				} else {
					//To free memory!!
					(void)vmgr_4k_d2_set_close(type, 0, 0);
					(void)vmgr_4k_d2_set_close(type, 1, 1);
				}
				dprintk_4kd2("Dec :: Init Done Handle(0x%x)",
					arg->gsV4kd2DecHandle);

		#ifdef CONFIG_VPU_TIME_MEASUREMENT
				vmgr_4k_d2_data.iTime[type].print_out_index
				= vmgr_4k_d2_data.iTime[type].proc_base_cnt = 0;
				vmgr_4k_d2_data.iTime[type].accumulated_proc_time
				= vmgr_4k_d2_data.iTime[type].accumulated_frame_cnt
				= 0;
				vmgr_4k_d2_data.iTime[type].proc_time_30frames
				= 0;
		#endif
				// VPU_4KD2_SET_OPTIONS
				{
					int result = 0;
					// Do not set below code to 1 until fix issue that green screen appear
					arg->gsV4kd2DecSetOptions.iUseBitstreamOffset = 0; //Do not use - not tested yet
					arg->gsV4kd2DecSetOptions.iMeasureDecPerf = 0;
					arg->gsV4kd2DecSetOptions.pfPrintCb = NULL;

					if ((module_param_vdbg_lib & VLOG_MASK_LIB_MEASURE_PERF) == VLOG_MASK_LIB_MEASURE_PERF)
					{
						arg->gsV4kd2DecSetOptions.iMeasureDecPerf = 1;

						if ((module_param_vdbg_lib & VLOG_MASK_LIB_USE_CB_PRINTK)
								== VLOG_MASK_LIB_USE_CB_PRINTK)
						{
							V_DBG(VPU_DBG_ERROR, "[%s %4d][VPU_4KD2_SET_OPTIONS] set - printk", __FILE__, __LINE__);
							arg->gsV4kd2DecSetOptions.pfPrintCb = (void (*)(const char *fmt, ...))vdbg_log_printk;
						}
						else
						{
							V_DBG(VPU_DBG_ERROR, "[%s %4d][VPU_4KD2_SET_OPTIONS] set - vdbg_log", __FILE__, __LINE__);
							arg->gsV4kd2DecSetOptions.pfPrintCb = (void (*)(const char *fmt, ...))vdbg_log; // file output
						}
					}

					if (module_param_vdbg_lib != 0) // only for test
					{
						unsigned int reserved_id100 = ((module_param_vdbg_lib >> 16) & 0xFFFF);
						if (reserved_id100 > 0) {
							arg->gsV4kd2DecSetOptions.iReservedId100[0] = 100; //~65536
							arg->gsV4kd2DecSetOptions.iReservedId100[1] = ((reserved_id100 &   1) ==   1);
							arg->gsV4kd2DecSetOptions.iReservedId100[2] = ((reserved_id100 &   2) ==   2);
							arg->gsV4kd2DecSetOptions.iReservedId100[3] = ((reserved_id100 &   4) ==   4);
							arg->gsV4kd2DecSetOptions.iReservedId100[4] = ((reserved_id100 &   8) ==   8);
							arg->gsV4kd2DecSetOptions.iReservedId100[5] = ((reserved_id100 &  16) ==  16);
							arg->gsV4kd2DecSetOptions.iReservedId100[6] = ((reserved_id100 &  32) ==  32);
							arg->gsV4kd2DecSetOptions.iReservedId100[7] = ((reserved_id100 &  64) ==  64);
							arg->gsV4kd2DecSetOptions.iReservedId100[8] = ((reserved_id100 & 128) == 128);
						}
					}

					codec_handle.pcodec_handle = &arg->gsV4kd2DecHandle;
					result = tcc_vpu_4k_d2_dec_l(VPU_4KD2_SET_OPTIONS,
											(vcodec_handle_t *)(codec_handle.pvcodec_handle),
											(void *)(&arg->gsV4kd2DecSetOptions),
											(void *)NULL);

					if (result != RETCODE_SUCCESS) {
						arg->gsV4kd2DecSetOptions.iUseBitstreamOffset = 0; // not supported command
						V_DBG(VPU_DBG_SEQUENCE, "[%s %4d][VPU_4KD2_SET_OPTIONS] not supported command, result=%d", __FILE__, __LINE__, result);
					}
				}
			}
		}
		break;

		case VPU_DEC_SEQ_HEADER:
		case VPU_DEC_SEQ_HEADER_KERNEL:
		{
			void *arg = args;
			unsigned int uiSize, streamSize;
			int iSize;
			union {
				unsigned int ui_data;
				int *pi_data;	//NULL
				void *pv_data;
			} udata;
			vpu_4K_D2_dec_initial_info_t *gsV4kd2DecInitialInfo;

			udata.pi_data = NULL;

			 gsV4kd2DecInitialInfo = vmgr_4k_d2_data.bDiminishInputCopy
			   ? &((VPU_4K_D2_DECODE_t *)arg)->gsV4kd2DecInitialInfo
			   : &((VPU_4K_D2_SEQ_HEADER_t *)arg)
				   ->gsV4kd2DecInitialInfo;

			streamSize
			 = ((VPU_4K_D2_SEQ_HEADER_t *)arg)->stream_size;
			if (streamSize > INT_MAX) {
				streamSize = INT_MAX;
			}

			iSize = ((VPU_4K_D2_DECODE_t *)arg)->gsV4kd2DecInput
					.m_iBitstreamDataSize;
			if (iSize < 0) {
				iSize = 0;
			}

			if (vmgr_4k_d2_data.bDiminishInputCopy) {
				uiSize = (unsigned int)iSize;
			} else {
				uiSize = streamSize;
			}
			vmgr_4k_d2_data.szFrame_Len = uiSize;
			udata.ui_data = uiSize;
			vmgr_4k_d2_data.check_interrupt_detection = 1;
			vmgr_4k_d2_data.nDecode_Cmd = 0;

			dprintk_4kd2(
			"Dec :: VPU_4K_D2_DEC_SEQ_HEADER in :: size(%d)", uiSize);
			ret =
				tcc_vpu_4k_d2_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle,
				(vmgr_4k_d2_data.bDiminishInputCopy
				  ? (void *)(&((VPU_4K_D2_DECODE_t *)arg)
					->gsV4kd2DecInput) :
				  (void *)udata.pv_data),	//uiSize
				  (void *)gsV4kd2DecInitialInfo);

			dprintk_4kd2(
			"Dec :: VPU_4K_D2_DEC_SEQ_HEADER out 0x%x\n res info. %d - %d - %d, %d - %d - %d",
				ret,
				gsV4kd2DecInitialInfo->m_iPicWidth,
				gsV4kd2DecInitialInfo->m_PicCrop.m_iCropLeft,
				gsV4kd2DecInitialInfo->m_PicCrop.m_iCropRight,
				gsV4kd2DecInitialInfo->m_iPicHeight,
				gsV4kd2DecInitialInfo->m_PicCrop.m_iCropTop,
				gsV4kd2DecInitialInfo->m_PicCrop.m_iCropBottom);

			vmgr_4k_d2_change_clock(
				gsV4kd2DecInitialInfo->m_iPicWidth,
				gsV4kd2DecInitialInfo->m_iPicHeight);
		}
		break;

		case V2D_IP_DEC_SEQDATA:
		{
			VPU_4K_D2_SEQ_HEADER_t *arg =
				(VPU_4K_D2_SEQ_HEADER_t *)v24kd2mgr_unmarshal_ip_seqdata(args);

			if (arg == NULL)
			{
				ret = RETCODE_FAILURE;
			}
			else
			{
				vpu_4K_D2_dec_initial_info_t *initial_info = &arg->gsV4kd2DecInitialInfo;
				unsigned long iSize = arg->stream_size;

				vmgr_4k_d2_data.szFrame_Len = arg->stream_size;
				vmgr_4k_d2_data.check_interrupt_detection = 1;
				vmgr_4k_d2_data.nDecode_Cmd = 0;

				dprintk_4kd2("[%s][In] seqdata in: size %lu", __func__, iSize);

				ret = tcc_vpu_4k_d2_dec_l(VPU_DEC_SEQ_HEADER,
						(vcodec_handle_t *)&pHandle,
						(void *) iSize,
						(void *)initial_info);

				dprintk_4kd2("[%s][Out] seqdata out: ret = %#x\n "
						"res info. %d - %d - %d, %d - %d - %d",
						__func__, ret,
						initial_info->m_iPicWidth,
						initial_info->m_PicCrop.m_iCropLeft,
						initial_info->m_PicCrop.m_iCropRight,
						initial_info->m_iPicHeight,
						initial_info->m_PicCrop.m_iCropTop,
						initial_info->m_PicCrop.m_iCropBottom);

				vmgr_4k_d2_change_clock(initial_info->m_iPicWidth,
						initial_info->m_iPicHeight);

				if (ret == RETCODE_SUCCESS) {
					v24kd2mgr_marshal_op_seqdata(args);
					ret = v24kd2mgr_register_hwbuf_fb(args, pHandle, tcc_vpu_4k_d2_dec_l);
				}
			}
		}
		break;

		case VPU_DEC_REG_FRAME_BUFFER:
		case VPU_DEC_REG_FRAME_BUFFER_KERNEL:
		{
			VPU_4K_D2_SET_BUFFER_t *arg;
			arg = (VPU_4K_D2_SET_BUFFER_t *) args;

			dprintk_4kd2(
			"Dec :: VPU_4K_D2_DEC_REG_FRAME_BUFFER in :: 0x%x/0x%x",
				arg->gsV4kd2DecBuffer.m_FrameBufferStartAddr[0],
				arg->gsV4kd2DecBuffer.m_FrameBufferStartAddr[1]);

			ret =
				tcc_vpu_4k_d2_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle,
				(void *)(&arg->gsV4kd2DecBuffer),
				(void *)NULL);
			dprintk_4kd2
				("@@ Dec :: VPU_4K_D2_DEC_REG_FRAME_BUFFER out");
		}
		break;

		case VPU_DEC_DECODE:
		case VPU_DEC_DECODE_KERNEL:
		case V2D_IP_DEC_FRMDATA:
		{
			int rcnt = 0;
			VPU_4K_D2_DECODE_t *arg = NULL;
			int hiding_superframe;
			bool isFlexible = (cmd == V2D_IP_DEC_FRMDATA);

			if (isFlexible) {
				arg = (VPU_4K_D2_DECODE_t *) v24kd2mgr_unmarshal_ip_frmdata(args);
				cmd = VPU_DEC_DECODE;
			} else {
				arg = (VPU_4K_D2_DECODE_t *) args;
			}

			if (arg == NULL)
			{
				ret = RETCODE_FAILURE;
			}
			else
			{
				hiding_superframe = arg->gsV4kd2DecInput.m_Reserved[20];
#ifdef CONFIG_VPU_TIME_MEASUREMENT
				startTime = vetc_GetKtime();
#endif

			if (arg->gsV4kd2DecInput.m_iBitstreamDataSize < 0) {
					err_4kd2("[%d] The size of bitstream data is negative",
						arg->gsV4kd2DecInput.m_iBitstreamDataSize);
				break;
			}

			for(; rcnt < 16; rcnt++) {
				vmgr_4k_d2_data.szFrame_Len = (unsigned int)
					arg->gsV4kd2DecInput.m_iBitstreamDataSize;
				dprintk_4kd2(
				"Dec: Dec In => 0x%x - 0x%x, 0x%x, 0x%x - 0x%x, %d, flag: %d",
					arg->gsV4kd2DecInput.m_BitstreamDataAddr[PA],
					arg->gsV4kd2DecInput.m_BitstreamDataAddr[VA],
					arg->gsV4kd2DecInput.m_iBitstreamDataSize,
					arg->gsV4kd2DecInput.m_UserDataAddr[PA],
					arg->gsV4kd2DecInput.m_UserDataAddr[VA],
					arg->gsV4kd2DecInput.m_iUserDataBufferSize,
					arg->gsV4kd2DecInput.m_iSkipFrameMode);

				vmgr_4k_d2_data.check_interrupt_detection = 1;
				ret = tcc_vpu_4k_d2_dec_l(
					cmd & ~VPU_BASE_OP_KERNEL,
					(vcodec_handle_t *)&pHandle,
					(void *)&arg->gsV4kd2DecInput,
					(void *)&arg->gsV4kd2DecOutput);

				dprintk_4kd2("Dec: Dec Out => %d - %d - %d, %d - %d - %d",
				  arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDisplayWidth,
				  arg->gsV4kd2DecOutput.m_DecOutInfo
					.m_DisplayCropInfo.m_iCropLeft,
				  arg->gsV4kd2DecOutput.m_DecOutInfo
					.m_DisplayCropInfo.m_iCropRight,
				  arg->gsV4kd2DecOutput.m_DecOutInfo
					.m_iDisplayHeight,
				  arg->gsV4kd2DecOutput.m_DecOutInfo
					.m_DisplayCropInfo.m_iCropTop,
				  arg->gsV4kd2DecOutput.m_DecOutInfo
					.m_DisplayCropInfo.m_iCropBottom);

				dprintk_4kd2(
				"Dec[%d]: Dec Out => ret[%d] !! PicType[%d], OutIdx[%d/%d], "
				"OutStatus[%d/%d], POC[%d/%d]",
				  type, ret,
				  arg->gsV4kd2DecOutput.m_DecOutInfo.m_iPicType,
				  arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDispOutIdx,
				  arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodedIdx,
				  arg->gsV4kd2DecOutput.m_DecOutInfo.m_iOutputStatus,
				  arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodingStatus,
				  arg->gsV4kd2DecOutput.m_DecOutInfo.m_Reserved[5],
				  arg->gsV4kd2DecOutput.m_DecOutInfo.m_Reserved[6]);

#ifdef VPU_LIB_WRITE_OUTPUT_TO_YUV
					if ((module_param_vdbg_lib & VLOG_MASK_LIB_WRITE_OUTPUT_TO_YUV)
							== VLOG_MASK_LIB_WRITE_OUTPUT_TO_YUV) {
						if (arg->gsV4kd2DecOutput.m_DecOutInfo.m_iOutputStatus == 1) {
							if (arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDispOutIdx >= 0) {
								unsigned int p_src_y  = (unsigned int )((uintptr_t)arg->gsV4kd2DecOutput.m_pDispOut[PA][0]);
								unsigned int p_src_cb = (unsigned int )((uintptr_t)arg->gsV4kd2DecOutput.m_pDispOut[PA][1]);
								unsigned int p_src_cr = (unsigned int )((uintptr_t)arg->gsV4kd2DecOutput.m_pDispOut[PA][2]);
								request_thread_dump(
									arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDisplayWidth,
									arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDisplayHeight,
									arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDispOutIdx,
									p_src_y, p_src_cb, p_src_cr);
							}
						}
					}
#endif
					switch (arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodingStatus) {
					case VPU_DEC_BUF_FULL:
						err_4kd2("[%d] Buffer full", type);
						rcnt = 16;
						break;

					case VPU_DEC_VP9_SUPER_FRAME:
						dprintk_4kd2("superframe: sub-frame num: %u, %d/%d, is_sf=%d, index=%d, nFrames=%d, (%d/%d)",
									arg->gsV4kd2DecOutput.m_DecOutInfo.m_SuperFrameInfo.m_uiNframes,
									arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodedIdx, arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDispOutIdx,
									arg->gsV4kd2DecOutput.m_DecOutInfo.m_iIsSuperFrame,
									arg->gsV4kd2DecOutput.m_DecOutInfo.m_SuperFrameInfo.m_uiCurrentIdx,
									arg->gsV4kd2DecOutput.m_DecOutInfo.m_SuperFrameInfo.m_uiNframes,
									arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodedIdx,
									arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDispOutIdx);
						if (hiding_superframe == 20U && arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDispOutIdx < 0) {
							//repeat the decoding process for VP9 super-frame
							LOG_COVERITY("%d", arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodingStatus);
						} else {
							rcnt = 16;
						}
						break;

					default:
						rcnt = 16;
						break;
					}
				}

				if (isFlexible) {
					v24kd2mgr_marshal_op_frmdata(args, (ret == RETCODE_SUCCESS));
				}

				vmgr_4k_d2_data.nDecode_Cmd++;

#ifdef CONFIG_VPU_TIME_MEASUREMENT
			endTime = vetc_GetKtime();
//DBG_INFO
#endif

				vmgr_4k_d2_change_clock(
					arg->gsV4kd2DecOutput.m_DecOutInfo
						.m_iDecodedWidth,
					arg->gsV4kd2DecOutput.m_DecOutInfo
						.m_iDecodedHeight);
			}
		}
		break;

		case VPU_DEC_GET_OUTPUT_INFO:
		case VPU_DEC_GET_OUTPUT_INFO_KERNEL:
		{
			VPU_4K_D2_DECODE_t *arg =
				(VPU_4K_D2_DECODE_t *) args;

			dprintk_4kd2("Dec: VPU_DEC_GET_OUTPUT_INFO");
			ret =
			  tcc_vpu_4k_d2_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
			  (vcodec_handle_t *)&pHandle, (void *)arg,
			  (void *)&arg->gsV4kd2DecOutput);

			dprintk_4kd2("Dec: Dec Out => %d - %d - %d, %d - %d - %d",
				arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDisplayWidth,
				arg->gsV4kd2DecOutput.m_DecOutInfo.m_DisplayCropInfo.m_iCropLeft,
				arg->gsV4kd2DecOutput.m_DecOutInfo.m_DisplayCropInfo.m_iCropRight,
				arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDisplayHeight,
				arg->gsV4kd2DecOutput.m_DecOutInfo.m_DisplayCropInfo.m_iCropTop,
				arg->gsV4kd2DecOutput.m_DecOutInfo.m_DisplayCropInfo.m_iCropBottom);

			dprintk_4kd2(
			"Dec: Dec Out => ret[%d] !! PicType[%d], OutIdx[%d/%d], OutStatus[%d/%d]",
				ret,
				arg->gsV4kd2DecOutput.m_DecOutInfo.m_iPicType,
				arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDispOutIdx,
				arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodedIdx,
				arg->gsV4kd2DecOutput.m_DecOutInfo.m_iOutputStatus,
				arg->gsV4kd2DecOutput.m_DecOutInfo
				.m_iDecodingStatus);

			dprintk_4kd2(
			"Dec: Dec Out => dec_Idx(%d), 0x%x 0x%x 0x%x / 0x%x 0x%x 0x%x",
				arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDispOutIdx,
				arg->gsV4kd2DecOutput.m_pDispOut[PA][0],
				arg->gsV4kd2DecOutput.m_pDispOut[PA][1],
				arg->gsV4kd2DecOutput.m_pDispOut[PA][2],
				arg->gsV4kd2DecOutput.m_pDispOut[VA][0],
				arg->gsV4kd2DecOutput.m_pDispOut[VA][1],
				arg->gsV4kd2DecOutput.m_pDispOut[VA][2]);

			dprintk_4kd2(
			"Dec: Dec Out => disp_Idx(%d), 0x%x 0x%x 0x%x / 0x%x 0x%x 0x%x",
				arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodedIdx,
				arg->gsV4kd2DecOutput.m_pCurrOut[PA][0],
				arg->gsV4kd2DecOutput.m_pCurrOut[PA][1],
				arg->gsV4kd2DecOutput.m_pCurrOut[PA][2],
				arg->gsV4kd2DecOutput.m_pCurrOut[VA][0],
				arg->gsV4kd2DecOutput.m_pCurrOut[VA][1],
				arg->gsV4kd2DecOutput.m_pCurrOut[VA][2]);

			if (arg->gsV4kd2DecOutput.m_DecOutInfo.m_iDecodingStatus
				== VPU_DEC_BUF_FULL) {
				err_4kd2("%d: Buffer full", type);
			}
		}
		break;

		case VPU_DEC_BUF_FLAG_CLEAR:
		case VPU_DEC_BUF_FLAG_CLEAR_KERNEL:
		{
			int *arg = (int *)args;

			dprintk_4kd2("Dec[%d] :: DispIdx Clear %d", type, *arg);
			ret =
			  tcc_vpu_4k_d2_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle, (void *)(arg),
				(void *)NULL);
		}
		break;

		case V2D_IP_FRM_CLEAR:
		{
			int slot = -1;
			struct v2hw_flex_io *fli = (struct v2hw_flex_io *)args;
			V2_FLEXIP_GET(fli, CLEAR_FB_IDX, slot);

			ret = tcc_vpu_4k_d2_dec_l(VPU_DEC_BUF_FLAG_CLEAR,
					(vcodec_handle_t *)&pHandle,
					(void *)&slot,
					(void *)NULL);
			if (ret != RETCODE_SUCCESS) {
				(void)pr_err("failed to clear slot %d (ret: %d)", slot, ret);
			}
		}
		break;

		case V2D_IP_FRM_FLUSH:
		{
			int slot = 0, max_count = 0;
			struct v2hw_flex_io *fli = (struct v2hw_flex_io *)args;
			V2_FLEXIP_GET(fli, FLUSH_FB_MAX, max_count);

			for (; slot < max_count; slot++) {
				uint64_t op_key = (1LLU << (unsigned)slot);
				if (fli->op_keys & op_key) {
					ret = tcc_vpu_4k_d2_dec_l(VPU_DEC_BUF_FLAG_CLEAR,
							(vcodec_handle_t *)&pHandle,
							(void *)&slot,
							(void *)NULL);
					if (ret == RETCODE_SUCCESS) {
						fli->op_keys &= ~op_key;
					} else {
						(void)pr_err(
								"failed to clear slot %d (ret: %d)", slot, ret);
					}
				}
			}
		}
		break;

		case VPU_DEC_FLUSH_OUTPUT:
		case VPU_DEC_FLUSH_OUTPUT_KERNEL:
		case V2D_IP_FRM_DRAIN:
		{
			VPU_4K_D2_DECODE_t *arg;

			bool isFlexible = (cmd == V2D_IP_FRM_DRAIN);
			if (isFlexible) {
				arg = (VPU_4K_D2_DECODE_t *) v24kd2mgr_unmarshal_ip_drndata(args);
				cmd = VPU_DEC_FLUSH_OUTPUT;
			} else {
				arg = (VPU_4K_D2_DECODE_t *) args;
			}

			dprintk_4kd2("Dec[%d] :: VPU_4K_D2_DEC_FLUSH_OUTPUT !!", type);
			ret = tcc_vpu_4k_d2_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle,
				(void *)&arg->gsV4kd2DecInput,
				(void *)&arg->gsV4kd2DecOutput);

			if (isFlexible) {
				v24kd2mgr_marshal_op_frmdata(args, (ret == RETCODE_SUCCESS));
			}
		}
		break;

		case VPU_DEC_CLOSE:
		case VPU_DEC_CLOSE_KERNEL:
		case V2D_IP_DRV_RST:
		{
			bool isFlexible = (cmd == V2D_IP_DRV_RST);
			if (isFlexible) {
				cmd = VPU_DEC_CLOSE;
			}

			ret =
			  tcc_vpu_4k_d2_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle, (void *)NULL,
				(void *)NULL);
			V_DBG(VPU_DBG_SEQUENCE,
				"Dec :: VPU_4K_D2_DEC_CLOSED!!");
			(void)vmgr_4k_d2_set_close(type, 1, 1);
		}
		break;

		case GET_RING_BUFFER_STATUS:
		case GET_RING_BUFFER_STATUS_KERNEL:
		case V2D_IP_RNG_GETPOS:
		{
			VPU_4K_D2_RINGBUF_GETINFO_t *arg;

			bool isFlexible = (cmd == V2D_IP_RNG_GETPOS);
			if (isFlexible) {
				arg = (VPU_4K_D2_RINGBUF_GETINFO_t *)
						v24kd2mgr_unmarshal_ip_getpos(args);
				cmd = GET_RING_BUFFER_STATUS;
			} else {
				arg = (VPU_4K_D2_RINGBUF_GETINFO_t *)args;
			}

			ret = tcc_vpu_4k_d2_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle, (void *)NULL,
				(void *)&arg->gsV4kd2DecRingStatus);

			if (isFlexible && (ret == RETCODE_SUCCESS)) {
				v24kd2mgr_marshal_op_getpos(args);
			}
		}
		break;

		case FILL_RING_BUFFER_AUTO:
		case FILL_RING_BUFFER_AUTO_KERNEL:
		{
			VPU_4K_D2_RINGBUF_SETBUF_t *arg =
				(VPU_4K_D2_RINGBUF_SETBUF_t *)args;

			ret =
				tcc_vpu_4k_d2_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle,
				(void *)&arg->gsV4kd2DecInit,
				(void *)&arg->gsV4kd2DecRingFeed);
			dprintk_4kd2
			("Dec :: ReadPTR : 0x%08x, WritePTR : 0x%08x",
				vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x120),
				vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x124));
		}
		break;

		case VPU_UPDATE_WRITE_BUFFER_PTR:
		case VPU_UPDATE_WRITE_BUFFER_PTR_KERNEL:
		case V2D_IP_RNG_SETPOS:
		{
			VPU_4K_D2_RINGBUF_SETBUF_PTRONLY_t *arg = NULL;

			union {
				int i_data;
				int *pi_data;	//NULL
				void *pv_data;
			} ucopysize, flushbuf;

			bool isFlexible = (cmd == V2D_IP_RNG_SETPOS);
			if (isFlexible) {
				arg = (VPU_4K_D2_RINGBUF_SETBUF_PTRONLY_t *)v24kd2mgr_unmarshal_ip_setpos(args);
				cmd = VPU_UPDATE_WRITE_BUFFER_PTR;
			} else {
				arg = (VPU_4K_D2_RINGBUF_SETBUF_PTRONLY_t *)args;
			}

			if (arg == NULL)
			{
				ret = RETCODE_FAILURE;
			}
			else
			{
				ucopysize.pi_data = NULL;
				ucopysize.i_data = arg->iCopiedSize;
				flushbuf.pi_data = NULL;
				flushbuf.i_data = arg->iFlushBuf;

				vmgr_4k_d2_data.check_interrupt_detection = 1;
				ret = tcc_vpu_4k_d2_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
					(vcodec_handle_t *)&pHandle,
					(void *)ucopysize.pv_data,
					(void *)flushbuf.pv_data);
			}
		}
		break;

		case GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY:
		case GET_INITIAL_INFO_KERNEL_FOR_STREAMING_MODE_ONLY:
		{
			VPU_4K_D2_SEQ_HEADER_t *arg =
				(VPU_4K_D2_SEQ_HEADER_t *)args;

			ret =
				tcc_vpu_4k_d2_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				 (vcodec_handle_t *)&pHandle,
				 (void *)(&arg->gsV4kd2DecInitialInfo), NULL);
		}
		break;

		case VPU_CODEC_GET_VERSION:
		case VPU_CODEC_GET_VERSION_KERNEL:
		{
			VPU_4K_D2_GET_VERSION_t *arg =
				(VPU_4K_D2_GET_VERSION_t *)args;

			ret =
				tcc_vpu_4k_d2_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				  (vcodec_handle_t *)&pHandle,
				  arg->pszVersion, arg->pszBuildData);
			dprintk_4kd2("Dec: version : %s, build : %s",
				arg->pszVersion, arg->pszBuildData);
		}
		break;

		case VPU_DEC_SWRESET:
		case VPU_DEC_SWRESET_KERNEL:
			ret =
				tcc_vpu_4k_d2_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
					(vcodec_handle_t *)&pHandle, NULL, NULL);
			break;

		default:
			{
				err_4kd2("Dec: not supported command(0x%x)", cmd);
				ret = 0x999;
			}
			break;
		}
	}
#if DEFINED_CONFIG_VENC_CNT_1to16
	else {
		err_4kd2(
		"Enc[%d]: Encoder for VPU-4K-D2 VP9/HEVC do not support. command(0x%x)",
			type, cmd);
		ret = 0x999;
	}
#endif

#ifdef CONFIG_VPU_TIME_MEASUREMENT
	time_gap_us = vetc_GetTimediff_us(endTime, startTime);

	if (cmd == VPU_DEC_DECODE) {
		printMeasurementTime((void*)&vmgr_4k_d2_data, type, 1, time_gap_us);
	}
#endif

	return ret;
}

static int vmgr_4k_d2_proc_exit_by_external(struct VpuList *list, int *result,
						 unsigned int type)
{
	long lhandle;

	if (type >= VPU_MAX) {
		err_4kd2("%s for %d!!", __func__, type);
		V_DBG(VPU_DBG_ERROR,"vputype_range failed (%d)", type);
		return 1;
	}

	lhandle = vmgr_4k_d2_data.handle[type];
	if ((vmgr_4k_d2_get_close((vputype)type) == 0) && (lhandle != 0x00)) {
		list->type = type;

		if (type >= (unsigned int)VPU_ENC) {
			list->cmd_type = VPU_ENC_CLOSE;
		} else {
			list->cmd_type = VPU_DEC_CLOSE;
		}

		list->handle = lhandle;
		list->args = NULL;
		list->comm_data = NULL;
		list->vpu_result = result;

		err_4kd2("%s for %d!!", __func__, type);

		(void)vmgr_4k_d2_list_manager(list, (int)LIST_ADD);

		return 1;
	}

	return 0;
}

#if 0 // Keep the code for future use
static void vmgr_4k_d2_wait_process(int wait_ms)
{
	int max_count = wait_ms / 20;

	while (vmgr_4k_d2_data.cmd_processing) {
		max_count--;
		msleep(20);

		if (max_count <= 0) {
			err_4kd2("cmd_processing(cmd %d) didn't finish!!", vmgr_4k_d2_data.current_cmd);
			break;
		}
	}
}
#endif

static int vmgr_4k_d2_external_all_close(int wait_ms)
{
	vputype type;
	int max_count;
	int ret = 0;

	for (type = 0; type < VPU_4K_D2_MAX; type++) {
		if (vmgr_4k_d2_proc_exit_by_external(&vmgr_4k_d2_data.vList[type], &ret, (unsigned int)type)) {
			max_count = wait_ms / 10;

			while (vmgr_4k_d2_get_close(type) == 0) {
				max_count--;
				if (max_count < 0) {
					break;
				}
				usleep_range(0, 1000);	//msleep(10);
			}
		}
	}

	return ret;
}

static int vmgr_4k_d2_cmd_open(char *str)
{
	int ret = 0;

	dprintk_4kd2("======> vmgr_4k_d2_%s_open In!! %d'th", str, atomic_read(&vmgr_4k_d2_data.opened));

	vmgr_4k_d2_enable_clock(0, 0);

	if (atomic_read(&vmgr_4k_d2_data.opened) == 0) {
#ifdef FORCED_ERROR
		forced_error_count = FORCED_ERR_CNT;
#endif
#if DEFINED_CONFIG_VENC_CNT_1to16
		vmgr_4k_d2_data.only_decmode = 0;
#else
		vmgr_4k_d2_data.only_decmode = 1;
#endif
		vmgr_4k_d2_data.clk_limitation = 1;
		vmgr_4k_d2_data.cmd_processing = 0;

		vmgr_4k_d2_hw_reset();
		vmgr_4k_d2_enable_irq(vmgr_4k_d2_data.irq);
		ret = vmem_init();
		if (ret < 0) {
			err_4kd2("failed to allocate memory for VPU_4K_D2!! %d", ret);
		}
		cntInt_4kd2 = 0;
		#ifdef DEBUG_VPU_4K_D2_K
		cntwk_4kd2 = 0;
		#endif
	}
	atomic_inc(&vmgr_4k_d2_data.opened);

	dprintk_4kd2("======> vmgr_4k_d2_%s_open Out!! %d'th", str, atomic_read(&vmgr_4k_d2_data.opened));

	return ret;
}

static int vmgr_4k_d2_cmd_release(char *str)
{
	V_DBG(VPU_DBG_CLOSE, "======> vmgr_4k_d2_%s_release In!! %d'th", str,
		&vmgr_4k_d2_data.opened);

	if (atomic_read(&vmgr_4k_d2_data.opened) > 0) {
		atomic_dec(&vmgr_4k_d2_data.opened);
	}

	if (atomic_read(&vmgr_4k_d2_data.opened) == 0) {
		unsigned int type = 0;
		int alive_cnt = 0;

#if 1 // To close whole vpu-4k-d2 vp9/hevc instance
		//when being killed process opened this.
		if (!vmgr_4k_d2_data.bVpu_already_proc_force_closed) {
			vmgr_4k_d2_data.external_proc = 1;
			(void)vmgr_4k_d2_external_all_close(200);
			vmgr_4k_d2_data.external_proc = 0;
		}
		vmgr_4k_d2_data.bVpu_already_proc_force_closed = (bool)false;
#endif

		for (type = 0; type < (unsigned int)VPU_4K_D2_MAX; type++) {
			if (vmgr_4k_d2_data.closed[type] == 0) {
				if (alive_cnt < (int)INT_MAX) {
					alive_cnt++;
				}
			}
		}

		if (alive_cnt != 0) {
			V_DBG(VPU_DBG_CLOSE, "VPU-4K-D2 VP9/HEVC might be cleared by force.");
		}

		atomic_set(&vmgr_4k_d2_data.oper_intr, 0);
		vmgr_4k_d2_data.cmd_processing = 0;

		vmgr_4k_d2_close_all(1);

		vmgr_4k_d2_disable_irq(vmgr_4k_d2_data.irq);
		(void)vmgr_4k_d2_BusPrioritySetting(BUS_FOR_NORMAL, 0);
		vmem_deinit();
		vmgr_4k_d2_hw_assert();

		udelay(1000); //1ms
	}

	vmgr_4k_d2_disable_clock(0, 0);

	if (vmgr_4k_d2_data.nOpened_Count >= UINT_MAX) {
		return 0;
	} else {
		vmgr_4k_d2_data.nOpened_Count++;
	}

	V_DBG(VPU_DBG_CLOSE,
	"======> vmgr_4k_d2_%s_release Out!! %d'th, total = %d  - DEC(%d/%d/%d/%d/%d)",
		str, atomic_read(&vmgr_4k_d2_data.opened),
		vmgr_4k_d2_data.nOpened_Count,
		vmgr_4k_d2_get_close(VPU_DEC),
		vmgr_4k_d2_get_close(VPU_DEC_EXT),
		vmgr_4k_d2_get_close(VPU_DEC_EXT2),
		vmgr_4k_d2_get_close(VPU_DEC_EXT3),
		vmgr_4k_d2_get_close(VPU_DEC_EXT4));

	return 0;
}

static unsigned int hangup_rel_count_4kd2;	// = 0;
static long vmgr_4k_d2_ioctl(struct file *filp, unsigned int cmd,
					unsigned long arg)
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

	VPU_UNUSED_PARAMETER(filp);

	uarg.pcv_data = NULL;
	uarg.ul_data = arg;

	mutex_lock(&vmgr_4k_d2_data.comm_data.io_mutex);

	switch (cmd) {
	case VPU_SET_CLK:
	case VPU_SET_CLK_KERNEL:
	{
		if (cmd == (unsigned int)VPU_SET_CLK_KERNEL) {
			(void)memcpy(&info, (CONTENTS_INFO *)uarg.pci_data, sizeof(info));
		} else {
			if (copy_from_user(
				&info, (CONTENTS_INFO *)uarg.pci_data, sizeof(info)) != 0U) {
				ret = -EFAULT;
			}
		}
	}
	break;

	case VPU_GET_FREEMEM_SIZE:
	case VPU_GET_FREEMEM_SIZE_KERNEL:
	{
		unsigned int uitype = 0;
		vputype type;
		unsigned int freemem_sz;

		if (cmd == (unsigned int)VPU_GET_FREEMEM_SIZE_KERNEL) {
			(void)memcpy(&uitype, uarg.pui_data, sizeof(unsigned int));
		} else {
			if (copy_from_user(&uitype, uarg.pui_data, sizeof(unsigned int)) != 0U) {
				ret = -EFAULT;
			}
		}

		if (uitype >= (unsigned int)VPU_MAX) {
			type = VPU_DEC;	//default
		} else {
			type = (vputype)uitype;
		}

		if (ret == 0) {
			freemem_sz = vmem_get_freemem_size(type);

			if (cmd == (unsigned int)VPU_GET_FREEMEM_SIZE_KERNEL) {
				(void)memcpy(uarg.pui_data, &freemem_sz, sizeof(unsigned int));
			} else {
				if (copy_to_user(uarg.pui_data, &freemem_sz, sizeof(unsigned int)) != 0U) {
					ret = -EFAULT;
				}
			}
		}
	}
	break;

	case VPU_HW_RESET:
		vmgr_4k_d2_hw_reset();
	break;

	case VPU_SET_MEM_ALLOC_MODE:
	case VPU_SET_MEM_ALLOC_MODE_KERNEL:
	{
		if (cmd == (unsigned int)VPU_SET_MEM_ALLOC_MODE_KERNEL) {
			(void)memcpy(&open_info, (OPENED_sINFO *)uarg.posi_data, sizeof(OPENED_sINFO));
		} else {
			if (copy_from_user(&open_info, uarg.posi_data, sizeof(OPENED_sINFO)) != 0U) {
				ret = -EFAULT;
			}
		}

		if (ret == 0) {
			if (open_info.opened_cnt != 0U) {
				vmem_set_only_decode_mode((int)open_info.type);
			}
		}
	}
	break;

	case VPU_CHECK_CODEC_STATUS:
	case VPU_CHECK_CODEC_STATUS_KERNEL:
	{
		if (cmd == (unsigned int)VPU_CHECK_CODEC_STATUS_KERNEL) {
			(void)memcpy(uarg.pi_data, vmgr_4k_d2_data.closed, sizeof(vmgr_4k_d2_data.closed));
		} else {
			if (copy_to_user(uarg.pi_data, vmgr_4k_d2_data.closed, sizeof(vmgr_4k_d2_data.closed)) != 0U) {
				ret = -EFAULT;
			}
		}
	}
	break;

	case VPU_GET_INSTANCE_IDX:
	case VPU_GET_INSTANCE_IDX_KERNEL:
	{
		INSTANCE_INFO iInst;

		if (cmd == (unsigned int)VPU_GET_INSTANCE_IDX_KERNEL) {
			(void)memcpy(&iInst, uarg.pi_data, sizeof(INSTANCE_INFO));
		} else {
			if (copy_from_user(&iInst, uarg.pi_data, sizeof(INSTANCE_INFO)) != 0U) {
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
				(void)memcpy(uarg.pi_data, &iInst, sizeof(INSTANCE_INFO));
			} else {
				if (copy_to_user(uarg.pi_data, &iInst, sizeof(INSTANCE_INFO)) != 0U) {
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
		} else  {
			if (copy_from_user(&iInst, (int *)uarg.pi_data, sizeof(INSTANCE_INFO)) != 0U) {
				ret = -EFAULT;
			}
		}

		if (ret == 0) {
			if (iInst.type == VPU_ENC) {
				venc_clear_instance(iInst.nInstance);
			} else {
				vdec_clear_instance(iInst.nInstance);
			}
		}
	}
	break;

	case VPU_TRY_FORCE_CLOSE:
	case VPU_TRY_FORCE_CLOSE_KERNEL:
	{
		if (vmgr_4k_d2_data.bVpu_already_proc_force_closed == (bool)false) {
			vmgr_4k_d2_data.external_proc = 1;
			(void)vmgr_4k_d2_external_all_close(200);
			vmgr_4k_d2_data.external_proc = 0;
			vmgr_4k_d2_data.bVpu_already_proc_force_closed = (bool)true;
		}
	}
	break;

	case VPU_TRY_CLK_RESTORE:
	case VPU_TRY_CLK_RESTORE_KERNEL:
	{
		vmgr_4k_d2_restore_clock(0, atomic_read(&vmgr_4k_d2_data.opened));
	}
	break;

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	case VPU_TRY_OPEN_DEV:
	case VPU_TRY_OPEN_DEV_KERNEL:
		(void)vmgr_4k_d2_cmd_open("cmd");
		break;

	case VPU_TRY_CLOSE_DEV:
	case VPU_TRY_CLOSE_DEV_KERNEL:
		(void)vmgr_4k_d2_cmd_release("cmd");
		break;
#endif

	case VPU_TRY_HANGUP_RELEASE:
		if (hangup_rel_count_4kd2 < (UINT_MAX / 2U)) {
			hangup_rel_count_4kd2++;
		}
		V_DBG(VPU_DBG_SEQUENCE,
			" vpu_4k_d2 ===> VPU_TRY_HANGUP_RELEASE %d'th",
			hangup_rel_count_4kd2);
		break;

#ifdef DEBUG_VPU_4K_D2_K
	case VPU_DEBUG_ISR:
		vpu_4k_d2_isr_param_debug.vpu_k_isr_cnt_hit = cntInt_4kd2;
		vpu_4k_d2_isr_param_debug.wakeup_interrupt_cnt = cntwk_4kd2;
		if (copy_to_user((void *)uarg.pv_data,
			&vpu_4k_d2_isr_param_debug,
			sizeof(struct debug_4k_d2_k_isr_t)) != 0U) {
			ret = -EFAULT;
		}
		break;
#endif
	default:
		err_4kd2("Unsupported ioctl[%d]!!!", cmd);
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&vmgr_4k_d2_data.comm_data.io_mutex);

	return ret;
}

#ifdef CONFIG_COMPAT
static long vmgr_4k_d2_compat_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	unsigned int tmpArg = 0U;

	if (arg < UINT_MAX) {
		tmpArg = (unsigned int)arg;
	}
	return vmgr_4k_d2_ioctl(filep, cmd, (unsigned long)compat_ptr(tmpArg));
}
#endif

static irqreturn_t vmgr_4k_d2_isr_handler(int irq, void *dev_id)
{
	unsigned int reason;

	VPU_UNUSED_PARAMETER(irq);
	VPU_UNUSED_PARAMETER(dev_id);

	if (cntInt_4kd2 < (UINT_MAX - 1U)) {
		cntInt_4kd2++;
	}

	if ((cntInt_4kd2 % 30U) == 0U) {
		detailk_4kd2("Interrupt cnt: %d", cntInt_4kd2);
	}


	reason = vetc_reg_read(vmgr_4k_d2_data.base_addr, 0x004C/*W5_VPU_VINT_REASON*/);
	reason &= 0x7FFFFFFFU; //bit [31] is reserved.
	atomic_or((int)reason, &vmgr_4k_d2_data.oper_intr);
#ifdef DEBUG_VPU_4K_D2_K
	if (cntwk_4kd2 < (UINT_MAX - 1UL)) {
		cntwk_4kd2++;
	}
#endif
	wake_up_interruptible(&vmgr_4k_d2_data.oper_wq);

	return IRQ_HANDLED;
}

static int vmgr_4k_d2_open(struct inode *pinode, struct file *filp)
{
	if (vmgr_4k_d2_data.irq_reged == 0U) {
		err_4kd2("not registered vpu-4k-d2 vp9/hevc-mgr-irq");
	}

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	dprintk_4kd2("enter!! %d'th",
		atomic_read(&vmgr_4k_d2_data.dev_file_opened));
	atomic_inc(&vmgr_4k_d2_data.dev_file_opened);
	dprintk_4kd2("Out!! %d'th",
		atomic_read(&vmgr_4k_d2_data.dev_file_opened));
#else
	mutex_lock(&vmgr_4k_d2_data.comm_data.file_mutex);
	(void)vmgr_4k_d2_cmd_open(vpu_4k_fname_file);
	mutex_unlock(&vmgr_4k_d2_data.comm_data.file_mutex);
#endif

	filp->private_data = &vmgr_4k_d2_data;
	LOG_COVERITY("%p", pinode);

	return 0;
}

static int vmgr_4k_d2_release(struct inode *pinode, struct file *filp)
{
#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	dprintk_4kd2("enter!! %d'th", atomic_read(&vmgr_4k_d2_data.dev_file_opened));
	atomic_dec(&vmgr_4k_d2_data.dev_file_opened);
	vmgr_4k_d2_data.nOpened_Count++;

	V_DBG(VPU_DBG_CLOSE, "Out!! %d'th, total = %d  - DEC(%d/%d/%d/%d/%d)",
		atomic_read(&vmgr_4k_d2_data.dev_file_opened),
		vmgr_4k_d2_data.nOpened_Count,
		vmgr_4k_d2_get_close(VPU_DEC),
		vmgr_4k_d2_get_close(VPU_DEC_EXT),
		vmgr_4k_d2_get_close(VPU_DEC_EXT2),
		vmgr_4k_d2_get_close(VPU_DEC_EXT3),
		vmgr_4k_d2_get_close(VPU_DEC_EXT4));
#else
	mutex_lock(&vmgr_4k_d2_data.comm_data.file_mutex);
	(void)vmgr_4k_d2_cmd_release(vpu_4k_fname_file);
	mutex_unlock(&vmgr_4k_d2_data.comm_data.file_mutex);
#endif
	LOG_COVERITY("%p,%p", pinode, filp);
	return 0;
}

/**
 * @brief Function to check if there is a duplicate element in the list.
 *
 * @param list The list to check for duplicates.
 * @param element The element to compare for duplicates.
 * @return true if a duplicate element is found, false otherwise.
 */
static bool vmgr_4k_d2_is_duplicate_element(struct list_head *list, struct VpuList *element)
{
	struct VpuList *entry;

	// Iterate through each element in the list
	list_for_each_entry(entry, list, list) {
		// Check if the current element is the same as the given element
		if (entry == element) {
			return true; // The duplicate element already exists in the list
		}
	}

	return false; // No duplicate element found
}

/**
 * @brief Function to add an element to the list.
 *
 * @param element The element to add to the list.
 * @param list The list to add the element to.
 * @return true if the element is added successfully, false if a duplicate element is found.
 */
static bool vmgr_4k_d2_add_element_to_list(struct VpuList *element, struct list_head *list)
{
	if (vmgr_4k_d2_is_duplicate_element(list, element)) {
		V_DBG(VPU_DBG_SEQUENCE, "Duplicate element detected, cannot add to the list");
		return false; // Return false to handle the error if a duplicate element is found
	}

	list_add_tail(&element->list, list);
	return true; // Element added successfully
}

/**
 * @brief Function to remove an element from the list.
 *
 * @param element The element to remove from the list.
 * @param list The list to remove the element from.
 * @return true if the element is removed successfully, false if the element does not exist in the list.
 */
static bool vmgr_4k_d2_remove_element_from_list(struct VpuList *element, struct list_head *list)
{
	if (!vmgr_4k_d2_is_duplicate_element(list, element)) {
		V_DBG(VPU_DBG_SEQUENCE, "Element does not exist in the list");
		return false; // Return false to indicate that the element does not exist in the list (for error handling)
	}

	list_del(&element->list);
	return true; // Return true to indicate successful removal of the element
}

struct VpuList *vmgr_4k_d2_list_manager(struct VpuList *args, unsigned int cmd)
{
	struct VpuList *ret = NULL;
	struct VpuList *oper_data = (struct VpuList *) args;
	bool should_wake_up = false; // Variable indicating whether wake_up_interruptible should be called

	if (oper_data == NULL) {
		if (cmd == (unsigned int)LIST_ADD || cmd == (unsigned int)LIST_DEL) {
			V_DBG(VPU_DBG_SEQUENCE, "Data is null, cmd=%d", cmd);
			return NULL;
		}
	}

	if (cmd == (unsigned int)LIST_ADD) {
		*oper_data->vpu_result = RET0;
	}

	mutex_lock(&vmgr_4k_d2_data.comm_data.list_mutex);

	switch (cmd) {
	case LIST_ADD:
		if (vmgr_4k_d2_add_element_to_list(oper_data, &vmgr_4k_d2_data.comm_data.main_list)) {
			*oper_data->vpu_result |= RET1;
			if (vmgr_4k_d2_data.cmd_queued > INT_MAX) {
				V_DBG(VPU_DBG_SEQUENCE, "Cmd queued is already FULL");
			} else {
				vmgr_4k_d2_data.cmd_queued++;
			}

			if (vmgr_4k_d2_data.comm_data.thread_intr > INT_MAX) {
				V_DBG(VPU_DBG_SEQUENCE, "Comm data thread interrupt count is already NULL");
			} else {
				vmgr_4k_d2_data.comm_data.thread_intr++;
			}
			should_wake_up = true; // Set should_wake_up to true to indicate that wake_up_interruptible needs to be called
		}
		break;
	case LIST_DEL:
		if (vmgr_4k_d2_data.cmd_queued == 0) {
			V_DBG(VPU_DBG_SEQUENCE, "No commands queued for deletion");
			break;
		}
		if (vmgr_4k_d2_remove_element_from_list(oper_data, &vmgr_4k_d2_data.comm_data.main_list)) {
			vmgr_4k_d2_data.cmd_queued--;
		}
		break;
	case LIST_IS_EMPTY:
		if (list_empty(&vmgr_4k_d2_data.comm_data.main_list) != 0) {
			ret = &vpu_4kd2_mgr_vlist;
		}
		break;
	case LIST_GET_ENTRY:
		ret = list_first_entry(
				&vmgr_4k_d2_data.comm_data.main_list,
				struct VpuList, list);
		break;
	default:
		/* Nothing to do */
		break;
	}

	mutex_unlock(&vmgr_4k_d2_data.comm_data.list_mutex);

	if (should_wake_up && (cmd == (unsigned int)LIST_ADD)) {
		wake_up_interruptible(&vmgr_4k_d2_data.comm_data.thread_wq);
	}

	return ret;
}

static int vmgr_4k_d2_operation(void)
{
	int oper_finished;
	struct VpuList *oper_data = NULL;
	vputype type;

	while (vmgr_4k_d2_list_manager(NULL, (int)LIST_IS_EMPTY) == NULL) {
		vmgr_4k_d2_data.cmd_processing = 1;
		oper_finished = 1;

		dprintk_4kd2("%s :: not empty cmd_queued(%d)", __func__, vmgr_4k_d2_data.cmd_queued);

		oper_data = (struct VpuList *)vmgr_4k_d2_list_manager(NULL, (int)LIST_GET_ENTRY);
		if (oper_data == NULL) {
			err_4kd2("data is null");
			vmgr_4k_d2_data.cmd_processing = 0;
			return 0;
		}

		*oper_data->vpu_result |= RET2;

		dprintk_4kd2(
		"%s [%d] :: cmd = 0x%x, vmgr_4k_d2_data.cmd_queued(%d)",
		__func__, oper_data->type, oper_data->cmd_type,
		vmgr_4k_d2_data.cmd_queued);

		type = VPU_DEC;
		if (oper_data->type < VPU_4K_D2_MAX ) {
			type = (vputype)oper_data->type;
		}

		if ((type>= 0) && (type < VPU_4K_D2_MAX)) {
			*oper_data->vpu_result |= RET3;

			*oper_data->vpu_result =
				vmgr_4k_d2_process(type,
					oper_data->cmd_type,
					oper_data->handle,
					oper_data->args);
			oper_finished = 1;
			if (*oper_data->vpu_result != RETCODE_SUCCESS) {
				if ((*oper_data->vpu_result
					!= RETCODE_INSUFFICIENT_BITSTREAM)
					&& (*oper_data->vpu_result !=
					RETCODE_INSUFFICIENT_BITSTREAM_BUF)) {
					err_4kd2(
					"vmgr_4k_d2_out[0x%x] :: type = %d, vmgr_4k_d2_data.handle = 0x%x, cmd = 0x%x, frame_len %d",
						*oper_data->vpu_result,
						oper_data->type,
						oper_data->handle,
						oper_data->cmd_type,
						vmgr_4k_d2_data.szFrame_Len);
				}

				if (*oper_data->vpu_result
					== RETCODE_CODEC_EXIT) {
					vmgr_4k_d2_restore_clock(0,
					 atomic_read(
					 &vmgr_4k_d2_data.opened));
					vmgr_4k_d2_close_all(1);
				}
			}
		} else {
			err_4kd2(
			"missed info or unknown command => type = 0x%x, cmd = 0x%x",
			 oper_data->type, oper_data->cmd_type);

			*oper_data->vpu_result = RETCODE_FAILURE;
			oper_finished = 0;
		}

		if (oper_finished != 0) {
			int opened = atomic_read(&vmgr_4k_d2_data.opened);
			if ((oper_data->comm_data != NULL)
				&& (opened != 0)) {
				oper_data->comm_data->count++;
				if (oper_data->comm_data->count != 1U) {
					dprintk_4kd2(
					"poll wakeup count = %d :: type(0x%x) cmd(0x%x)",
						oper_data->comm_data->count,
						oper_data->type,
						oper_data->cmd_type);
				}

				wake_up_interruptible(&oper_data->comm_data->wq);
			} else {
				err_4kd2(
				"Error: abnormal exception or external command was processed!! 0x%p - %d",
					oper_data->comm_data, opened);
			}
		} else {
			err_4kd2("Error: abnormal exception 2!! 0x%p - %d",
					oper_data->comm_data,
					atomic_read(&vmgr_4k_d2_data.opened));
		}

		(void)vmgr_4k_d2_list_manager(oper_data, (int)LIST_DEL);

		vmgr_4k_d2_data.cmd_processing = 0;
	}

	return 0;
}

static int vmgr_4k_d2_thread(void *kthread)
{
	unsigned long jtimeout;

	VPU_UNUSED_PARAMETER(kthread);

	V_DBG(VPU_DBG_THREAD, "enter");

	jtimeout = msecs_to_jiffies(50);
	if (jtimeout > (ULONG_MAX / 2UL)) {
		jtimeout = ((ULONG_MAX / 2UL) - 1UL);
	}

	do {
		if (vmgr_4k_d2_list_manager(NULL, (int)LIST_IS_EMPTY) != NULL) {
			vmgr_4k_d2_data.cmd_processing = 0;
			(void)wait_event_interruptible_timeout(
				vmgr_4k_d2_data.comm_data.thread_wq,
				(vmgr_4k_d2_data.comm_data.thread_intr > 0),
				(long)jtimeout);
			vmgr_4k_d2_data.comm_data.thread_intr = 0;
		} else {
			if ((atomic_read(&vmgr_4k_d2_data.opened) != 0)
				|| (vmgr_4k_d2_data.external_proc != 0U)) {
				(void)vmgr_4k_d2_operation();
			} else {
				struct VpuList *oper_data = NULL;

				err_4kd2("DEL for empty");

				oper_data = vmgr_4k_d2_list_manager(NULL, (int)LIST_GET_ENTRY);
				if (oper_data != NULL) {
					(void)vmgr_4k_d2_list_manager(oper_data, (int)LIST_DEL);
				}
			}
		}
	} while (!kthread_should_stop());

	V_DBG(VPU_DBG_THREAD, "out");

	return 0;
}

static int vmgr_4k_d2_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret = 0;
	unsigned long current_vm_range = (vma->vm_end >= vma->vm_start) ?
		(vma->vm_end - vma->vm_start) : 0U;

	VPU_UNUSED_PARAMETER(filp);

#if defined(CONFIG_TCC_MEM)
	if (vma->vm_end < vma->vm_start ) {
		err_4kd2("this address is not allowed");
		return -EAGAIN;
	}

	if (range_is_allowed(vma->vm_pgoff, current_vm_range) < 0) {
		err_4kd2("this address is not allowed");
		return -EAGAIN;
	}
#endif

	vma->vm_page_prot = vmem_get_pgprot(vma->vm_page_prot, vma->vm_pgoff);
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, current_vm_range, vma->vm_page_prot) != 0) {
		err_4kd2("remap_pfn_range failed");
		return -EAGAIN;
	}

	vma->vm_ops = NULL;
	vetc_vm_flags_set(vma, VM_IO | VM_DONTEXPAND | VM_PFNMAP);

	return ret;
}

static const struct file_operations vmgr_4k_d2_fops = {
	.open = vmgr_4k_d2_open,
	.release = vmgr_4k_d2_release,
	.mmap = vmgr_4k_d2_mmap,
	.unlocked_ioctl = vmgr_4k_d2_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vmgr_4k_d2_compat_ioctl,
#endif
};

static struct miscdevice vmgr_4k_d2_misc_device = {
	MISC_DYNAMIC_MINOR,
	VPU_4K_D2_MGR_NAME,
	&vmgr_4k_d2_fops,
};

int vmgr_4k_d2_probe(struct platform_device *pdev)
{
	int ret;
	int type;
	unsigned long int_flags;
	struct resource *res = NULL;
	void *tTmpPtr = NULL;

	if (pdev->dev.of_node == NULL) {
		return -ENODEV;
	}

	dprintk_4kd2("hmgr initializing!!");
	(void)memset(&vmgr_4k_d2_data, 0, sizeof(struct mgr_data_t));
	for (type = 0; type < VPU_4K_D2_MAX; type++) {
		vmgr_4k_d2_data.closed[type] = 1;
	}

	vmgr_4k_d2_init_variable();
	atomic_set(&vmgr_4k_d2_data.oper_intr, 0);
#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	atomic_set(&vmgr_4k_d2_data.dev_file_opened, 0);
#endif

	ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		err_4kd2("could not get IRQ");
		return -1;
	} else {
		vmgr_4k_d2_data.irq = (unsigned int)ret;
	}

	vmgr_4k_d2_data.nOpened_Count = 0;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "missing phy memory resource");
		return -1;
	}
	res->end += 1;

	vmgr_4k_d2_data.base_addr
	= devm_ioremap(&pdev->dev, res->start,
		res->end - res->start);
	dprintk_4kd2(
	"============> VPU-4K-D2 VP9/HEVC base address [0x%x -> 0x%p], irq num [%d]",
		res->start,
		vmgr_4k_d2_data.base_addr,
		(int)vmgr_4k_d2_data.irq - 32);

	vmgr_4k_d2_get_clock(pdev->dev.of_node);
	vmgr_4k_d2_get_reset(pdev->dev.of_node);

	init_waitqueue_head(&vmgr_4k_d2_data.comm_data.thread_wq);
	init_waitqueue_head(&vmgr_4k_d2_data.oper_wq);

	mutex_init(&vmgr_4k_d2_data.comm_data.list_mutex);
	mutex_init(&(vmgr_4k_d2_data.comm_data.io_mutex));
	mutex_init(&(vmgr_4k_d2_data.comm_data.file_mutex));

	INIT_LIST_HEAD(&vmgr_4k_d2_data.comm_data.main_list);
	INIT_LIST_HEAD(&vmgr_4k_d2_data.comm_data.wait_list);

	ret = vmem_config();
	if (ret < 0) {
		err_4kd2("unable to configure memory for VPU!! %d", ret);
		return -ENOMEM;
	}

#if defined(USE_ACCESS_POINT)
	if (st4KD2Func == NULL) {
		ret = get_vpu_4k_d2_access_addr();
		if (ret != 0) {
			V_DBG(VPU_DBG_ERROR, "Getting for library access point failed!!");
			return RETCODE_FAILURE;
		}
	}
#endif

	vmgr_4k_d2_init_interrupt();
	int_flags = vmgr_4k_d2_get_int_flags();
	ret = vmgr_4k_d2_request_irq(
			vmgr_4k_d2_data.irq, vmgr_4k_d2_isr_handler,
			int_flags, VPU_4K_D2_MGR_NAME, &vmgr_4k_d2_data);
	if (ret) {
		err_4kd2("to aquire vpu-4k-d2-vp9/hevc-dec-irq");
	}

	vmgr_4k_d2_data.irq_reged = 1;
	vmgr_4k_d2_disable_irq(vmgr_4k_d2_data.irq);

	kidle_task_4kd2 = kthread_run(vmgr_4k_d2_thread, NULL, "v4K-D2_th");
	VPU_CAST_PT(tTmpPtr, kidle_task_4kd2);
	if (IS_ERR(tTmpPtr)) {
		err_4kd2("unable to create thread!!");
		kidle_task_4kd2 = NULL;
		return -1;
	}
	dprintk_4kd2("success :: thread created!!");

	vmgr_4k_d2_close_all(1);

	if (misc_register(&vmgr_4k_d2_misc_device)) {
		(void)pr_info(
			   "VPU-4K-D2 VP9/HEVC Manager: Couldn't register device.");
		return -EBUSY;
	}

	return 0;
}
EXPORT_SYMBOL(vmgr_4k_d2_probe);

int vmgr_4k_d2_remove(struct platform_device *pdev)
{
	misc_deregister(&vmgr_4k_d2_misc_device);

	if (kidle_task_4kd2 != NULL) {
		(void)kthread_stop(kidle_task_4kd2);
		kidle_task_4kd2 = NULL;
	}

	devm_iounmap(&pdev->dev, vmgr_4k_d2_data.base_addr);
	if (vmgr_4k_d2_data.irq_reged != 0U) {
		vmgr_4k_d2_free_irq(vmgr_4k_d2_data.irq, &vmgr_4k_d2_data);
		vmgr_4k_d2_data.irq_reged = 0;
	}

	vmgr_4k_d2_put_clock();
	vmgr_4k_d2_put_reset();
	vmem_deinit();

	// comment: to avoid HIS metric violation(HIS_CALLS)
	//(void)pr_info("success :: vpu_4k_d2thread stopped!!");

	return 0;
}
EXPORT_SYMBOL(vmgr_4k_d2_remove);

#if defined(CONFIG_PM)
int vmgr_4k_d2_suspend(struct platform_device *pdev, pm_message_t state)
{
	int i, open_count = 0;

	VPU_UNUSED_PARAMETER(pdev);
	VPU_UNUSED_PARAMETER(state);

	if (atomic_read(&vmgr_4k_d2_data.opened) != 0) {
		(void)pr_info("\n vpu_4k_d2: suspend In DEC(%d/%d/%d/%d/%d)\n",
			vmgr_4k_d2_get_close(VPU_DEC),
			vmgr_4k_d2_get_close(VPU_DEC_EXT),
			vmgr_4k_d2_get_close(VPU_DEC_EXT2),
			vmgr_4k_d2_get_close(VPU_DEC_EXT3),
			vmgr_4k_d2_get_close(VPU_DEC_EXT4));

		(void)vmgr_4k_d2_external_all_close(200);

		open_count = atomic_read(&vmgr_4k_d2_data.opened);

		for (i = 0; i < open_count; i++) {
			vmgr_4k_d2_disable_clock(0, 0);
		}

		(void)pr_info("vpu_4k_d2: suspend Out DEC(%d/%d/%d/%d/%d)\n",
			vmgr_4k_d2_get_close(VPU_DEC),
			vmgr_4k_d2_get_close(VPU_DEC_EXT),
			vmgr_4k_d2_get_close(VPU_DEC_EXT2),
			vmgr_4k_d2_get_close(VPU_DEC_EXT3),
			vmgr_4k_d2_get_close(VPU_DEC_EXT4));
	}

	return 0;
}
EXPORT_SYMBOL(vmgr_4k_d2_suspend);

int vmgr_4k_d2_resume(struct platform_device *pdev)
{
	int i, open_count = 0;

	VPU_UNUSED_PARAMETER(pdev);

	if (atomic_read(&vmgr_4k_d2_data.opened) != 0) {
		open_count = atomic_read(&vmgr_4k_d2_data.opened);

		for (i = 0; i < open_count; i++) {
			vmgr_4k_d2_enable_clock(0, 0);
		}

		(void)pr_info("\n vpu_4k_d2: resume\n\n");
	}

	return 0;
}

EXPORT_SYMBOL(vmgr_4k_d2_resume);
#endif

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
MODULE_SOFTDEP("pre: vpu_lib jpu_lib hevc_lib vpu_4k_d2_lib vpu_hevc_enc_lib vpu");

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vpu_4k_d2 vp9/hevc manager");
MODULE_LICENSE("GPL");
#endif
