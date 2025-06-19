// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_comm.h"

#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC

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
#include <soc/telechips/pmap.h>
#endif

#include "vpu_buffer.h"
#include "vpu_devices.h"
#include "hevc_mgr_sys.h"
#include "hevc_mgr.h"
#include "hevc_mgr_flexio.h"

#define dprintk_hevcd(msg...)  V_DBG(VPU_DBG_INFO, "TCC_HEVC_MGR: " msg)
#define detailk_hevcd(msg...)  V_DBG(VPU_DBG_INFO, "TCC_HEVC_MGR: " msg)
#define cmdk_hevcd(msg...)     V_DBG(VPU_DBG_INFO, "TCC_HEVC_MGR [Cmd]: " msg)
#define err_hevcd(msg...)      V_DBG(VPU_DBG_ERROR, "TCC_HEVC_MGR [Err]: " msg)

#define HEVC_REGISTER_DUMP
#define HEVC_DUMP_STATUS

#if 0 //For test purpose!!
#define FORCED_ERROR
#endif
#ifdef FORCED_ERROR
#define FORCED_ERR_CNT 300
static int forced_error_count = FORCED_ERR_CNT;
#endif

#ifdef HEVC_DUMP_STATUS
#define W4_REG_BASE                 0x0000
#define W4_BS_RD_PTR                0x0130
#define W4_BS_WR_PTR                0x0134
#define W4_BS_OPTION                0x012C
#define W4_BS_PARAM                 0x0128

#define W4_VCPU_PDBG_RDATA_REG      0x001C
#define W4_VCPU_FIO_CTRL            0x0020
#define W4_VCPU_FIO_DATA            0x0024
#endif


static struct VpuList hevc_mgr_vlist;
static char hevcd_fname_file[] = "file";

// Control only once!!
static struct mgr_data_t hmgr_data;
static struct task_struct *kidle_task_hevcd;	// = NULL;

#if defined(USE_ACCESS_POINT)
// SHARE_POINT_ORDER_XXX :
//    VPU = 0, JPU = 1, HEVC = 2,
//    4KD2 = 3, HEVC_ENC = 4, HEVC_ENC_2 = 5
#   define SHARE_POINT_ORDER_HEVC 2U

typedef int (*tccfp_hevc_dec_t)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
static tccfp_hevc_dec_t tcc_hevc_dec;

typedef struct st_hevc_func_t {
	unsigned int check_code1;
	int (*tccfp_hevc_dec)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	unsigned int check_code2;
	int (*tccfp_hevc_enc)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	unsigned int check_code3;
	int (*tccfp_hevc_dec_esc)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	int (*tccfp_hevc_dec_ext)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	unsigned int check_code4;
} st_hevc_func;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
static st_hevc_func stHEVCFuncBase = {0, NULL, 0, NULL, 0, NULL, NULL, 0};
#endif

static st_hevc_func *stHEVCFunc = INITIAL_NULL;

static int check_hevc_access_addr_valid(void)
{
	int ret = -1;

	if (((CHECK_CODE_01 | stHEVCFunc->check_code1) == CHECK_CODE_01) &&
			((CHECK_CODE_02 | stHEVCFunc->check_code2) == CHECK_CODE_02) &&
			((CHECK_CODE_03 | stHEVCFunc->check_code3) == CHECK_CODE_03) &&
			((CHECK_CODE_04 | stHEVCFunc->check_code4) == CHECK_CODE_04)) {
		ret = 0;
	} else {
		V_DBG(VPU_DBG_ERROR, "HEVC CheckCode %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
				GET_FOURCC_1(stHEVCFunc->check_code1),
				GET_FOURCC_2(stHEVCFunc->check_code1),
				GET_FOURCC_3(stHEVCFunc->check_code1),
				GET_FOURCC_4(stHEVCFunc->check_code1),
				GET_FOURCC_1(stHEVCFunc->check_code2),
				GET_FOURCC_2(stHEVCFunc->check_code2),
				GET_FOURCC_3(stHEVCFunc->check_code2),
				GET_FOURCC_4(stHEVCFunc->check_code2),
				GET_FOURCC_1(stHEVCFunc->check_code3),
				GET_FOURCC_2(stHEVCFunc->check_code3),
				GET_FOURCC_3(stHEVCFunc->check_code3),
				GET_FOURCC_4(stHEVCFunc->check_code3),
				GET_FOURCC_1(stHEVCFunc->check_code4),
				GET_FOURCC_2(stHEVCFunc->check_code4),
				GET_FOURCC_3(stHEVCFunc->check_code4),
				GET_FOURCC_4(stHEVCFunc->check_code4)
			  );
	}
	return ret;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
static int get_hevc_access_addr_file(void)
{
	int ret = 0;
	struct file *filp = NULL;
	mm_segment_t oldfs;
	void *tTmpPtr = NULL;

#	if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
	oldfs = get_fs();
	set_fs(get_ds());
#	elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
	oldfs = get_fs();
	set_fs(KERNEL_DS);
#	else
	oldfs = force_uaccess_begin();
#	endif

	filp = filp_open("/proc/hevc", O_RDONLY, 0x1A4); //0644
	VPU_CAST_PT(tTmpPtr, filp);
	if (IS_ERR(tTmpPtr)) {
		V_DBG(VPU_DBG_ERROR, "/proc/hevc file open fail!!");
		ret = -1;
	} else {
		char data[20];
		unsigned long long res = 0;
		u32 idx = 0;

		idx = (u32)((u32)sizeof(void *)*2) + 2;

#	if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
		ret = vfs_read(filp, data, sizeof(data), &filp->f_pos);
#	else
		ret = filp->f_op->read(filp, data, sizeof(data), &filp->f_pos);
#	endif

		data[idx] = '\0';

		ret = kstrtoull(data, 16, &res);

		(void)memmove((void *)&stHEVCFunc, (void *)&res, sizeof(unsigned long));

		(void)filp_close(filp, NULL);
	}

#	if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
	set_fs(oldfs);
#	else
	force_uaccess_end(oldfs);
#	endif

	return ret;
}

#else

static int get_hevc_access_addr_mem(void)
{
	int ret = 0;
	void *va = NULL;

	va = vetc_ioremap(SHARE_POINT_ADDR + (SHARD_POINT_GAP * SHARE_POINT_ORDER_HEVC), SHARD_POINT_GAP);

	if (va == NULL) {
		V_DBG(VPU_DBG_ERROR, "ioremap failed");
		ret = -ENOMEM;
	} else {
		memcpy(&stHEVCFuncBase, va, sizeof(st_hevc_func));
		stHEVCFunc = &stHEVCFuncBase;

		V_DBG(VPU_DBG_INFO, "remap (PA : 0x%08x / VA : 0x%p) Dec ADDR : 0x%p",
				SHARE_POINT_ADDR, va,
				stHEVCFunc->tccfp_hevc_dec);

		iounmap(va);
	}

	return ret;
}
#endif

static int get_hevc_access_addr(void)
{
	int ret = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
	ret = get_hevc_access_addr_file();
#else
	ret = get_hevc_access_addr_mem();
#endif

	if (ret == 0) {
		ret = check_hevc_access_addr_valid();
		if (ret == 0) {
			tcc_hevc_dec = (tccfp_hevc_dec_t)stHEVCFunc->tccfp_hevc_dec;
		} else {
			ret = -1;
		}
	}

	return ret;
}

#else

extern int tcc_hevc_dec(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);

#endif

static int tcc_hevc_dec_l(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2) VPU_NO_SANITIZE_CFI
{
	return tcc_hevc_dec(Op, pHandle, pParam1, pParam2);
}

int hmgr_opened(void)
{
	int ret = 1;

	if (atomic_read(&hmgr_data.opened) == 0) {
		ret = 0;
	}
	return ret;
}
EXPORT_SYMBOL(hmgr_opened);

#ifdef HEVC_REGISTER_DUMP
#ifdef HEVC_DUMP_STATUS
static unsigned int hmgr_FIORead(unsigned int addr)
{
	unsigned int ctrl;
	unsigned int count = 0;
	unsigned int data = 0xffffffffU;

	ctrl = (addr & 0xffff);
	ctrl |= (unsigned int)(0x10000); //(1u << 16u);	read operation
	vetc_reg_write(hmgr_data.base_addr, W4_VCPU_FIO_CTRL, ctrl);
	count = 10000;
	while (count--) {
		ctrl = vetc_reg_read(hmgr_data.base_addr, W4_VCPU_FIO_CTRL);
		if (ctrl & 0x80000000U) {
			data =
				vetc_reg_read(hmgr_data.base_addr,
					  W4_VCPU_FIO_DATA);
			break;
		}
	}

	return data;
}

static int hmgr_FIOWrite(unsigned int addr, unsigned int data)
{
	unsigned int ctrl;

	vetc_reg_write(hmgr_data.base_addr, W4_VCPU_FIO_DATA, data);
	ctrl = (addr & 0xffff);
	ctrl |= (unsigned int)(0x10000); //(1u << 16u); write operation
	vetc_reg_write(hmgr_data.base_addr, W4_VCPU_FIO_CTRL, ctrl);

	return 1;
}

static unsigned int hmgr_ReadRegVCE(unsigned int vce_addr)
{
#define VCORE_DBG_ADDR              0x8300
#define VCORE_DBG_DATA              0x8304
#define VCORE_DBG_READY             0x8308

	int vcpu_reg_addr;
	unsigned int udata = 0xffffffffU;

	hmgr_FIOWrite(VCORE_DBG_READY, 0);

	vcpu_reg_addr = vce_addr >> 2;

	hmgr_FIOWrite(VCORE_DBG_ADDR, vcpu_reg_addr + 0x8000);

	if (hmgr_FIORead(VCORE_DBG_READY) == 1) {
		udata = hmgr_FIORead(VCORE_DBG_DATA);
	}

	return udata;
}

static void hmgr_dump_status(void)
{
	int rd, wr;
	unsigned int tq, ip, mc, lf;
	unsigned int tmp_avail_1, tmp_avail_2;
	unsigned int avail_cu, avail_tu, avail_tc, avail_lf, avail_ip;
	unsigned int ctu_fsm, nb_fsm, cabac_fsm, cu_info, mvp_fsm, tc_busy;
	unsigned int lf_fsm, bs_data, bbusy, fv;
	unsigned int reg_val;
	unsigned int index;
	unsigned int vcpu_reg[31] = { 0, };
	unsigned int bitstart, bitend, bitcommand;
	unsigned int ctu_x, ctu_y;
	unsigned int stat_tq, state_ip, state_mc, state_lf;
	unsigned int bwb1_res_cnt, bwb1_res_info;
	unsigned int bwb2_res_cnt, bwb2_res_info;
	unsigned int read_cnd_0, read_cnd_1, read_cnd_2;
	unsigned int write_cnt_0, write_cnt_1, write_cnt_2;
	unsigned int bs_opt, bs_param;
	int i = 0;

	V_DBG(VPU_DBG_REG_DUMP, "--------------------------------------");
	V_DBG(VPU_DBG_REG_DUMP, "--------- ---  VCPU_STATUS  ----------");
	V_DBG(VPU_DBG_REG_DUMP, "--------------------------------------");
	rd = vetc_reg_read(hmgr_data.base_addr, W4_BS_RD_PTR);
	wr = vetc_reg_read(hmgr_data.base_addr, W4_BS_WR_PTR);
	bs_opt = vetc_reg_read(hmgr_data.base_addr, W4_BS_OPTION);
	bs_param = vetc_reg_read(hmgr_data.base_addr, W4_BS_PARAM);
	V_DBG(VPU_DBG_REG_DUMP,
	  "RD_PTR:0x%08x WR_PTR:0x%08x BS_OPT:0x%08x BS_PARAM:0x%08x",
	  rd, wr, bs_opt, bs_param);

	// --------- VCPU register Dump
	V_DBG(VPU_DBG_REG_DUMP, "[+] VCPU REG Dump");
	for (index = 0; index < 25; index++) {
		vetc_reg_write(hmgr_data.base_addr, 0x14, (0x200) | (index & 0xff)); //(1 << 9)
		vcpu_reg[index] = vetc_reg_read(hmgr_data.base_addr,
						W4_VCPU_PDBG_RDATA_REG);

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
			default:
				V_DBG(VPU_DBG_REG_DUMP,
					"Unknown index");
				break;
			}
		}
	}
	V_DBG(VPU_DBG_REG_DUMP, "[-] VCPU REG Dump");
	// --------- BIT register Dump
	V_DBG(VPU_DBG_REG_DUMP, "[+] BPU REG Dump");
	V_DBG(VPU_DBG_REG_DUMP, "BITPC = 0x%08x",
		   hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x18)));
	for (i = 0; i < 10; i++) {
		unsigned int bitpc = hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x18));
		V_DBG(VPU_DBG_REG_DUMP, "BITPC = 0x%08x", bitpc);
	}

	bitstart = hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x11c));
	bitend = hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x120));
	bitcommand = hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x1FC));
	V_DBG(VPU_DBG_REG_DUMP, "BIT START=0x%08x, BIT END=0x%08x",
		   bitstart, bitend);
	V_DBG(VPU_DBG_REG_DUMP, "BIT COMMAND 0x%x", bitcommand);

	// --------- BIT HEVC Status Dump
	ctu_fsm = hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x48));
	nb_fsm = hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x4c));
	cabac_fsm = hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x50));
	cu_info = hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x54));
	mvp_fsm = hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x58));
	tc_busy = hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x5c));
	lf_fsm = hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x60));
	bs_data = hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x64));
	bbusy = hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x68));
	fv = hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x6C));

	ctu_x = hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x40));
	ctu_y = hmgr_FIORead((W4_REG_BASE + 0x8000 + 0x44));
	V_DBG(VPU_DBG_REG_DUMP,
	  "[DEBUG-BPUHEVC] CTU_X: %4d, CTU_Y: %4d",
	  ctu_x, ctu_y);

	V_DBG(VPU_DBG_REG_DUMP,
	  "[DEBUG-BPUHEVC] CTU_FSM>   Main: 0x%02x, FIFO: 0x%1x, NB: 0x%02x, DBK: 0x%1x",
	  ((ctu_fsm >> 24) & 0xff), ((ctu_fsm >> 16) & 0xff),
	  ((ctu_fsm >> 8) & 0xff), (ctu_fsm & 0xff));
	V_DBG(VPU_DBG_REG_DUMP,
	  "[DEBUG-BPUHEVC] NB_FSM: 0x%02x", nb_fsm & 0xff);
	V_DBG(VPU_DBG_REG_DUMP,
	  "[DEBUG-BPUHEVC] CABAC_FSM> SAO: 0x%02x, CU: 0x%02x, PU: 0x%02x, TU: 0x%02x, EOS: 0x%02x",
	  ((cabac_fsm >> 25) & 0x3f), ((cabac_fsm >> 19) & 0x3f),
	  ((cabac_fsm >> 13) & 0x3f), ((cabac_fsm >> 6) & 0x7f),
	  (cabac_fsm & 0x3f));
	V_DBG(VPU_DBG_REG_DUMP,
	  "[DEBUG-BPUHEVC] CU_INFO value = 0x%04x\n\t\t(l2cb: 0x%1x, cux: %1d, cuy; %1d, pred: %1d, pcm: %1d, wr_done: %1d, par_done: %1d, nbw_done: %1d, dec_run: %1d)",
	  cu_info, ((cu_info >> 16) & 0x3), ((cu_info >> 13) & 0x7),
	  ((cu_info >> 10) & 0x7), ((cu_info >> 9) & 0x3),
	  ((cu_info >> 8) & 0x1), ((cu_info >> 6) & 0x3),
	  ((cu_info >> 4) & 0x3), ((cu_info >> 2) & 0x3), (cu_info & 0x3));
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
	  ((bs_data >> 31) & 0x1),
	  ((bs_data >> 16) & 0xfff), (bs_data & 0xfff));
	V_DBG(VPU_DBG_REG_DUMP,
	  "[DEBUG-BPUHEVC] BUS_BUSY> mib_wreq_done: %1d, mib_busy: %1d, sdma_bus: %1d",
	  ((bbusy >> 2) & 0x1), ((bbusy >> 1) & 0x1), (bbusy & 0x1));
	V_DBG(VPU_DBG_REG_DUMP,
	  "[DEBUG-BPUHEVC] FIFO_VALID> cu: %1d, tu: %1d, iptu: %1d, lf: %1d, coff: %1d",
	  ((fv >> 4) & 0x1), ((fv >> 3) & 0x1),
	  ((fv >> 2) & 0x1), ((fv >> 1) & 0x1), (fv & 0x1));
	V_DBG(VPU_DBG_REG_DUMP, "[-] BPU REG Dump");

	// --------- VCE register Dump
	V_DBG(VPU_DBG_REG_DUMP, "[+] VCE REG Dump");
	tq = hmgr_ReadRegVCE(0xd0);
	ip = hmgr_ReadRegVCE(0xd4);
	mc = hmgr_ReadRegVCE(0xd8);
	lf = hmgr_ReadRegVCE(0xdc);

	tmp_avail_1 = hmgr_ReadRegVCE(0x11C);
	tmp_avail_2 = hmgr_ReadRegVCE(0x110);
	avail_cu = (tmp_avail_1>>16)
			- (tmp_avail_2>>16);

	avail_tu = (tmp_avail_1 & 0xFFFF)
			- (tmp_avail_2 & 0xFFFF);

	tmp_avail_1 = hmgr_ReadRegVCE(0x120);
	tmp_avail_2 = hmgr_ReadRegVCE(0x114);
	avail_tc = (tmp_avail_1>>16)
			- (tmp_avail_2>>16);

	avail_lf = (tmp_avail_1 & 0xFFFF)
			- (tmp_avail_2 & 0xFFFF);

	tmp_avail_1 = hmgr_ReadRegVCE(0x124);
	tmp_avail_2 = hmgr_ReadRegVCE(0x118);
	avail_ip = (tmp_avail_1>>16)
			- (tmp_avail_2>>16);
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
		hmgr_FIORead(0x88f4), /* GDI empty */
		avail_cu, avail_tu, avail_tc, avail_lf, avail_ip);
	/* CU/TU Queue count */
	reg_val = hmgr_ReadRegVCE(0x12C);
	V_DBG(VPU_DBG_REG_DUMP, "[DCIDEBUG] QUEUE COUNT: CU(%5d) TU(%5d) ",
		(reg_val>>16)&0xffff, reg_val&0xffff);
	reg_val = hmgr_ReadRegVCE(0x1A0);
	V_DBG(VPU_DBG_REG_DUMP, "TC(%5d) IP(%5d) ",
		(reg_val>>16)&0xffff, reg_val&0xffff);
	reg_val = hmgr_ReadRegVCE(0x1A4);
	V_DBG(VPU_DBG_REG_DUMP, "LF(%5d)", (reg_val>>16)&0xffff);
	V_DBG(VPU_DBG_REG_DUMP,
	  "VALID SIGNAL : CU0(%d)  CU1(%d)  CU2(%d) TU(%d) TC(%d) IP(%5d) LF(%5d)               DCI_FALSE_RUN(%d) VCE_RESET(%d) CORE_INIT(%d) SET_RUN_CTU(%d)",
		(reg_val>>6)&1, (reg_val>>5)&1,
		(reg_val>>4)&1, (reg_val>>3)&1,
		(reg_val>>2)&1, (reg_val>>1)&1,
		(reg_val>>0)&1,
		(reg_val>>10)&1, (reg_val>>9)&1,
		(reg_val>>8)&1, (reg_val>>7)&1);

	stat_tq = hmgr_ReadRegVCE(0xd0);
	state_ip = hmgr_ReadRegVCE(0xd4);
	state_mc = hmgr_ReadRegVCE(0xd8);
	state_lf = hmgr_ReadRegVCE(0xdc);
	V_DBG(VPU_DBG_REG_DUMP,
	  "State TQ: 0x%08x IP: 0x%08x MC: 0x%08x LF: 0x%08x",
		stat_tq, state_ip, state_mc, state_lf);

	bwb1_res_cnt = hmgr_ReadRegVCE(0x194);
	bwb1_res_info = hmgr_ReadRegVCE(0x198);
	V_DBG(VPU_DBG_REG_DUMP, "BWB[1]: RESPONSE_CNT(0x%08x) INFO(0x%08x)",
		bwb1_res_cnt, bwb1_res_info);

	bwb2_res_cnt = hmgr_ReadRegVCE(0x194);
	bwb2_res_info = hmgr_ReadRegVCE(0x198);
	V_DBG(VPU_DBG_REG_DUMP, "BWB[2]: RESPONSE_CNT(0x%08x) INFO(0x%08x)",
		bwb2_res_cnt, bwb2_res_info);

	V_DBG(VPU_DBG_REG_DUMP, "DCI INFO");

	read_cnd_0 = hmgr_ReadRegVCE(0x110);
	read_cnd_1 = hmgr_ReadRegVCE(0x114);
	read_cnd_2 = hmgr_ReadRegVCE(0x118);
	V_DBG(VPU_DBG_REG_DUMP,
	  "READ_CNT_0 : 0x%08x", read_cnd_0);
	V_DBG(VPU_DBG_REG_DUMP,
	  "READ_CNT_1 : 0x%08x", read_cnd_1);
	V_DBG(VPU_DBG_REG_DUMP,
	  "READ_CNT_2 : 0x%08x", read_cnd_2);

	write_cnt_0 = hmgr_ReadRegVCE(0x11c);
	write_cnt_1 = hmgr_ReadRegVCE(0x120);
	write_cnt_2 = hmgr_ReadRegVCE(0x124);
	V_DBG(VPU_DBG_REG_DUMP,
	  "WRITE_CNT_0: 0x%08x", write_cnt_0);
	V_DBG(VPU_DBG_REG_DUMP,
	  "WRITE_CNT_1: 0x%08x", write_cnt_1);
	V_DBG(VPU_DBG_REG_DUMP,
	  "WRITE_CNT_2: 0x%08x", write_cnt_2);
	reg_val = hmgr_ReadRegVCE(0x128);
	V_DBG(VPU_DBG_REG_DUMP, "LF_DEBUG_PT: 0x%08x", reg_val & 0xffffffff);

	V_DBG(VPU_DBG_REG_DUMP,
	  "cur_main_state %2d, r_lf_pic_deblock_disable %1d, r_lf_pic_sao_disable %1d",
			(reg_val >> 16) & 0x1f,
			(reg_val >> 15) & 0x1,
			(reg_val >> 14) & 0x1);
	V_DBG(VPU_DBG_REG_DUMP,
	  "para_load_done %1d, i_rdma_ack_wait %1d, i_sao_intl_col_done %1d, i_sao_outbuf_full %1d",
			(reg_val >> 13) & 0x1,
			(reg_val >> 12) & 0x1,
			(reg_val >> 11) & 0x1,
			(reg_val >> 10) & 0x1);
	V_DBG(VPU_DBG_REG_DUMP,
	  "lf_sub_done %1d, i_wdma_ack_wait %1d, lf_all_sub_done %1d, cur_ycbcr %1d, sub8x8_done %2d",
			(reg_val >> 9) & 0x1,
			(reg_val >> 8) & 0x1,
			(reg_val >> 6) & 0x1,
			(reg_val >> 4) & 0x1,
			reg_val & 0xf);

	V_DBG(VPU_DBG_REG_DUMP, "[-] VCE REG Dump");

	V_DBG(VPU_DBG_REG_DUMP, "----------------------------------------");
	V_DBG(VPU_DBG_REG_DUMP, "---------------------------------------");
}

#endif
#endif

int hmgr_get_close(vputype type)
{
	return hmgr_data.closed[type];
}

int hmgr_get_alive(void)
{
	return atomic_read(&hmgr_data.opened);
}

int hmgr_set_close(vputype type, int value, int bfreemem)
{
	int ret = 0;

	if (hmgr_get_close(type) == value) {
		dprintk_hevcd(" %d was already set into %d.", type, value);
		return -1;
	} else {
		hmgr_data.closed[type] = value;
		if (value == 1) {
			hmgr_data.handle[type] = 0x00;
			if (bfreemem != 0) {
				(void)vmem_proc_free_memory(type);
			}
		}
	}

	return ret;
}

static void hmgr_close_all(int bfreemem)
{
	(void)hmgr_set_close(VPU_DEC, 1, bfreemem);
	(void)hmgr_set_close(VPU_DEC_EXT, 1, bfreemem);
	(void)hmgr_set_close(VPU_DEC_EXT2, 1, bfreemem);
	(void)hmgr_set_close(VPU_DEC_EXT3, 1, bfreemem);
	(void)hmgr_set_close(VPU_DEC_EXT4, 1, bfreemem);
}

int hmgr_process_ex(struct VpuList *cmd_list, vputype type, int Op, int *result)
{
	if (atomic_read(&hmgr_data.opened) == 0) {
		return 0;
	}

	err_hevcd(" process_ex %d - 0x%x", type, Op);

	if (hmgr_get_close(type) == 0) {
		cmd_list->type = type;
		cmd_list->cmd_type = Op;
		cmd_list->handle = hmgr_data.handle[(unsigned int)type];
		cmd_list->args = NULL;
		cmd_list->comm_data = NULL;
		cmd_list->vpu_result = result;
		(void)hmgr_list_manager(cmd_list, (unsigned int)LIST_ADD);

		return 1;
	}

	return 1;
}

static int hmgr_internal_handler(void)
{
	int ret, ret_code = (int)RETCODE_INTR_DETECTION_NOT_ENABLED;
	int timeout = 200;
	unsigned long jtimeout;

	jtimeout = msecs_to_jiffies(timeout);
	if (jtimeout > LONG_MAX) {
		jtimeout = LONG_MAX;
	}

	if (hmgr_data.check_interrupt_detection) {
		if (atomic_read(&hmgr_data.oper_intr) > 0) {
			detailk_hevcd("Success 1: hevc operation!!");
			ret_code = RETCODE_SUCCESS;
		} else {
			ret = wait_event_interruptible_timeout(
				hmgr_data.oper_wq,
				atomic_read(&hmgr_data.oper_intr) > 0,
				(long)jtimeout);

			if (atomic_read(&hmgr_data.oper_intr) > 0) {
				detailk_hevcd("Success 2: hevc operation!!");
#if defined(FORCED_ERROR)
				if (forced_error_count-- <= 0) {
					ret_code = RETCODE_CODEC_EXIT;
					forced_error_count = FORCED_ERR_CNT;
					vetc_dump_reg_all(hmgr_data.base_addr,
						  "hmgr_internal_handler force-timed_out");
				} else
#endif
					ret_code = RETCODE_SUCCESS;
			} else {
				static unsigned char fname[] =
					  "hmgr_internal_handler timed_out";
				err_hevcd(
				"[CMD 0x%x][%d]: hevc timed_out(ref %d msec) => oper_intr[%d]!! [%d]th frame len %d",
					 hmgr_data.current_cmd, ret,
					 timeout,
					atomic_read(&hmgr_data.oper_intr),
					hmgr_data.nDecode_Cmd,
					hmgr_data.szFrame_Len);
				vetc_dump_reg_all(hmgr_data.base_addr, fname);
				hmgr_dump_status();
				ret_code = RETCODE_CODEC_EXIT;
			}
		}

		atomic_set(&hmgr_data.oper_intr, 0);
		hmgr_status_clear(hmgr_data.base_addr);
	}

	V_DBG(VPU_DBG_INTERRUPT, "out (Interrupt detection=%d, ret_code=%d)",
		hmgr_data.check_interrupt_detection,
		ret_code);

	return ret_code;
}

static int hmgr_process(vputype type, int cmd, long pHandle, void *args)
{
	int ret = 0;

#ifdef CONFIG_VPU_TIME_MEASUREMENT
	long long startTime, endTime;
	long long time_gap_us = 0LL;
#endif

	hmgr_data.check_interrupt_detection = 0;
	hmgr_data.current_cmd = cmd;

	if (type < VPU_ENC) {
		if ((cmd != (int)VPU_DEC_INIT) &&
			(cmd != (int)VPU_DEC_INIT_KERNEL) &&
			(cmd != (int)V2D_IP_DRV_INI)) {
			if ((hmgr_get_close(type) != 0)
				|| (hmgr_data.handle[type] == 0x00)) {
				return RETCODE_MULTI_CODEC_EXIT_TIMEOUT;
			}
		}

		if (cmd != VPU_DEC_BUF_FLAG_CLEAR
			&& cmd != VPU_DEC_DECODE
			&& cmd != VPU_DEC_BUF_FLAG_CLEAR_KERNEL
			&& cmd != VPU_DEC_DECODE_KERNEL) {
			cmdk_hevcd("Decoder(%d), command: 0x%x", type, cmd);
		}

		switch (cmd) {
		case VPU_DEC_INIT:
		case VPU_DEC_INIT_KERNEL:
		case V2D_IP_DRV_INI:
		{
			HEVC_INIT_t *arg = NULL;
			unsigned int vpu_lib_dbg = get_vpu_lib_dbg_param();
			bool isFlexible = (cmd == V2D_IP_DRV_INI);

			if (isFlexible) {
				arg = (HEVC_INIT_t *) v2hevcmgr_unmarshal_ip_inidata(args);
				cmd = VPU_DEC_INIT;
			} else {
				arg = (HEVC_INIT_t *) args;
			}

			if (arg != NULL) {
				hmgr_data.handle[type] = 0x00;

				arg->gsHevcDecInit.m_RegBaseVirtualAddr =
					(codec_addr_t)hmgr_data.base_addr;
				arg->gsHevcDecInit.m_Memcpy = vetc_memcpy;
				arg->gsHevcDecInit.m_Memset =
					(void (*) (void*, int, unsigned int, unsigned int))vetc_memset;
				arg->gsHevcDecInit.m_Interrupt =
					(int (*) (void))hmgr_internal_handler;
				arg->gsHevcDecInit.m_Ioremap =
					(void *(*) (phys_addr_t, unsigned int))vetc_ioremap;
				arg->gsHevcDecInit.m_Iounmap =
					(void  (*) (void *))vetc_iounmap;
				arg->gsHevcDecInit.m_reg_read =
					(unsigned int (*)(void *, unsigned int))vetc_reg_read;
				arg->gsHevcDecInit.m_reg_write =
					(void (*)(void *, unsigned int, unsigned int))vetc_reg_write;

				hmgr_data.check_interrupt_detection = 1;
				dprintk_hevcd(
					"Dec :: Init In => workbuff 0x%x/0x%x, Reg: 0x%p/0x%x, format : %d, Stream(0x%x/0x%x, 0x%x)",
					arg->gsHevcDecInit.m_BitWorkAddr[PA],
					arg->gsHevcDecInit.m_BitWorkAddr[VA],
					hmgr_data.base_addr,
					arg->gsHevcDecInit.m_RegBaseVirtualAddr,
					arg->gsHevcDecInit.m_iBitstreamFormat,
					arg->gsHevcDecInit.m_BitstreamBufAddr[PA],
					arg->gsHevcDecInit.m_BitstreamBufAddr[VA],
					arg->gsHevcDecInit.m_iBitstreamBufSize);
				dprintk_hevcd(
					"Dec :: Init In => optFlag 0x%x, Userdata(%d), Inter: %d, PlayEn: %d",
					arg->gsHevcDecInit.m_uiDecOptFlags,
					arg->gsHevcDecInit.m_bEnableUserData,
					arg->gsHevcDecInit.m_bCbCrInterleaveMode,
					arg->gsHevcDecInit.m_iFilePlayEnable);

#if defined(USE_ACCESS_POINT)
				if (check_hevc_access_addr_valid() != 0) {
					err_hevcd(
						"Dec-%d ######################## Access address envalid!!(%d)",
						type, check_hevc_access_addr_valid());

					return RETCODE_FAILURE;
				}
#endif
				if (vmem_alloc_count(type) <= 0) {
					err_hevcd(
					"@@ Dec-%d #################### No Buffer allocation",
						type);
					return RETCODE_FAILURE;
				}

				if ((vpu_lib_dbg & VPU_DBG_LIB_USE_CB_PRINTK) == VPU_DBG_LIB_USE_CB_PRINTK) {
					unsigned int codec_ip;
					hevc_dec_ctrl_log_status_t dec_log;

					// Extract codec_ip (A part), please refer to enum vpu_ip_type
					// VPU_IP_C7 = 1, VPU_IP_4KD2 = 2, VPU_IP_HEVC_ENC = 3, VPU_IP_HEVC_ENC2 = 4, VPU_IP_JPU_C6 = 5, VPU_IP_HEVC_DEC = 6
					codec_ip = (vpu_lib_dbg & 0x00F000U) >> 12;
					V_DBG(VPU_DBG_ERROR, "[HEVC_DEC] codec_ip: %d", codec_ip);

					// Check if codec_ip matches desired value
					if (codec_ip == 6) {
						// Check if codec_ip matches desired value
						// Extract log_mask (BBB part)
						unsigned int log_mask = (vpu_lib_dbg & 0x000FFFU);

						V_DBG(VPU_DBG_ERROR, "[HEVC_DEC] log_mask: %d (%x)", log_mask, log_mask);
						dec_log.pfLogPrintCb = (void (*)(const char *, ...))vpu_printk;
						dec_log.stLogLevel.bVerbose = (log_mask & 1U) ? 1 : 0;
						dec_log.stLogLevel.bDebug   = (log_mask & 2U) ? 1 : 0;
						dec_log.stLogLevel.bInfo	  = (log_mask & 4U) ? 1 : 0;
						dec_log.stLogLevel.bWarn	  = (log_mask & 8U) ? 1 : 0;
						dec_log.stLogLevel.bError   = (log_mask & 16U) ? 1 : 0; // 0x10
						dec_log.stLogLevel.bAssert  = (log_mask & 32U) ? 1 : 0; // 0x20
						dec_log.stLogLevel.bFunc	  = (log_mask & 64U) ? 1 : 0; // 0x40
						dec_log.stLogLevel.bTrace   = (log_mask & 128U) ? 1 : 0; // 0x80
						ret = tcc_hevc_dec_l(HEVCDEC_CTRL_LOG_STATUS, NULL, (void *)(&dec_log), (void *)NULL);
					}
				}

			ret = tcc_hevc_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(void *)(&arg->gsHevcDecHandle),
				(void *)(&arg->gsHevcDecInit),
				(void *)NULL);

			if (ret != RETCODE_SUCCESS) {
				static unsigned char fname[] =
					"init failure";
				err_hevcd("Dec :: Init Done with ret(0x%x)", ret);
				if (ret != RETCODE_CODEC_EXIT) {
					vetc_dump_reg_all(hmgr_data.base_addr,
							fname);
				}
			} else {
				if (isFlexible) {
					v2hevcmgr_marshal_op_inidata(args);
				}
			}

			if (ret != RETCODE_CODEC_EXIT
					&& arg->gsHevcDecHandle != 0) {
				hmgr_data.handle[type]
					= arg->gsHevcDecHandle;
				hmgr_set_close(type, 0, 0);
				cmdk_hevcd("Dec :: hmgr_data.handle = 0x%x",
						arg->gsHevcDecHandle);
			} else {
				//To free memory!!
				hmgr_set_close(type, 0, 0);
				hmgr_set_close(type, 1, 1);
			}
			dprintk_hevcd("Dec :: Init Done Handle(0x%x)",
					arg->gsHevcDecHandle);

#ifdef CONFIG_VPU_TIME_MEASUREMENT
				hmgr_data.iTime[type].print_out_index =
					hmgr_data.iTime[type].proc_base_cnt = 0;
				hmgr_data.iTime[type].accumulated_proc_time
				= hmgr_data.iTime[type].accumulated_frame_cnt = 0;
				hmgr_data.iTime[type].proc_time_30frames = 0;
#endif
			} else {
				err_hevcd("Dec :: VPU_DEC_INIT :: Undefined args. (cmd=%d)", cmd);
				ret = RETCODE_FAILURE;
			}
		}
		break;

		case VPU_DEC_SEQ_HEADER:
		case VPU_DEC_SEQ_HEADER_KERNEL:
		{
			HEVC_SEQ_HEADER_t *arg = NULL;
			int iSize;
			union {
				int i_data;
				int *pi_data;	//NULL
				void *pv_data;
			} udata;

			udata.pi_data = NULL;

			arg = (HEVC_SEQ_HEADER_t *)args;

			if (arg != NULL) {
				hmgr_data.szFrame_Len = iSize
					= (int)arg->stream_size;
				udata.i_data = iSize;
				hmgr_data.check_interrupt_detection = 1;
				hmgr_data.nDecode_Cmd = 0;
				dprintk_hevcd(
					"Dec :: HEVC_DEC_SEQ_HEADER in :: size(%d)",
					arg->stream_size);
				ret = tcc_hevc_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
					(vcodec_handle_t *)&pHandle,
					(void *)udata.pv_data,
					(void *)(&arg->gsHevcDecInitialInfo));
				dprintk_hevcd(
					"Dec :: HEVC_DEC_SEQ_HEADER out 0x%x\n res info. %d - %d - %d, %d - %d - %d",
					ret,
					arg->gsHevcDecInitialInfo.m_iPicWidth,
					arg->gsHevcDecInitialInfo.m_PicCrop.m_iCropLeft,
					arg->gsHevcDecInitialInfo.m_PicCrop.m_iCropRight,
					arg->gsHevcDecInitialInfo.m_iPicHeight,
					arg->gsHevcDecInitialInfo.m_PicCrop.m_iCropTop,
					arg->gsHevcDecInitialInfo.m_PicCrop.m_iCropBottom);
			} else {
				err_hevcd("Dec :: VPU_DEC_SEQ_HEADER :: Undefined args. (cmd=%d)", cmd);
			}
		}
		break;

		case V2D_IP_DEC_SEQDATA:
		{
			HEVC_SEQ_HEADER_t *arg =
				(HEVC_SEQ_HEADER_t *)v2hevcmgr_unmarshal_ip_seqdata(args);

			if (arg != NULL) {
				hevc_dec_initial_info_t *initial_info = &arg->gsHevcDecInitialInfo;
				unsigned long iSize = arg->stream_size;

				hmgr_data.szFrame_Len = arg->stream_size;
				hmgr_data.check_interrupt_detection = 1;
				hmgr_data.nDecode_Cmd = 0;

				dprintk_hevcd("[%s][In] V2D_IP_DEC_SEQDATA: size %lu", __func__, iSize);

				ret = tcc_hevc_dec_l(VPU_DEC_SEQ_HEADER,
						(vcodec_handle_t *)&pHandle,
						(void *) iSize,
						(void *)initial_info);

				dprintk_hevcd("[%s][Out] V2D_IP_DEC_SEQDATA: ret = %#x\n "
						"res info. %d - %d - %d, %d - %d - %d",
						ret,
						initial_info->m_iPicWidth,
						initial_info->m_PicCrop.m_iCropLeft,
						initial_info->m_PicCrop.m_iCropRight,
						initial_info->m_iPicHeight,
						initial_info->m_PicCrop.m_iCropTop,
						initial_info->m_PicCrop.m_iCropBottom);

				if (ret == RETCODE_SUCCESS) {
					v2hevcmgr_marshal_op_seqdata(args);
					ret = v2hevcmgr_register_hwbuf_fb(args, pHandle, tcc_hevc_dec_l);
				}
			} else {
				err_hevcd("Dec :: V2D_IP_DEC_SEQDATA :: Undefined args. (cmd=%d)", cmd);
				ret = RETCODE_FAILURE;
			}
		}
		break;

		case VPU_DEC_REG_FRAME_BUFFER:
		case VPU_DEC_REG_FRAME_BUFFER_KERNEL:
		{
			HEVC_SET_BUFFER_t *arg = NULL;

			arg = (HEVC_SET_BUFFER_t *)args;
			dprintk_hevcd(
				"Dec :: HEVC_DEC_REG_FRAME_BUFFER in :: 0x%x/0x%x",
				arg->gsHevcDecBuffer.m_FrameBufferStartAddr[0],
				arg->gsHevcDecBuffer.m_FrameBufferStartAddr[1]);

			ret = tcc_hevc_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle,
				(void *)(&arg->gsHevcDecBuffer),
				(void *)NULL);
			dprintk_hevcd
				("@@ Dec :: HEVC_DEC_REG_FRAME_BUFFER out");
		}
		break;

		case VPU_DEC_DECODE:
		case VPU_DEC_DECODE_KERNEL:
		case V2D_IP_DEC_FRMDATA:
		{
			HEVC_DECODE_t *arg = NULL;
			bool isFlexible = (cmd == V2D_IP_DEC_FRMDATA);

#ifdef CONFIG_VPU_TIME_MEASUREMENT
			startTime = vetc_GetKtime();
#endif

			if (isFlexible) {
				arg = (HEVC_DECODE_t *) v2hevcmgr_unmarshal_ip_frmdata(args);
				cmd = VPU_DEC_DECODE;
			} else {
				arg = (HEVC_DECODE_t *) args;
			}

			if (arg != NULL) {
				hmgr_data.szFrame_Len =
					arg->gsHevcDecInput.m_iBitstreamDataSize;
				dprintk_hevcd(
					"Dec :: Dec In => 0x%x - 0x%x, 0x%x, 0x%x - 0x%x, %d, flag: %d",
					arg->gsHevcDecInput.m_BitstreamDataAddr[PA],
					arg->gsHevcDecInput.m_BitstreamDataAddr[VA],
					arg->gsHevcDecInput.m_iBitstreamDataSize,
					arg->gsHevcDecInput.m_UserDataAddr[PA],
					arg->gsHevcDecInput.m_UserDataAddr[VA],
					arg->gsHevcDecInput.m_iUserDataBufferSize,
					arg->gsHevcDecInput.m_iSkipFrameMode);

				#ifdef DATA_PRINT_ON
				{
					unsigned char *ptr
						= (unsigned char *)arg->gsHevcDecInput
							.m_BitstreamDataAddr[VA];
					int i, datasize = 32;

					V_DBG(VPU_DBG_IO_FB_INFO, "=== data = %d",
						arg->gsHevcDecInput.m_iBitstreamDataSize);
					if (arg->gsHevcDecInput.m_iBitstreamDataSize < 32) {
						datasize = arg->gsHevcDecInput.m_iBitstreamDataSize;
					}

					for (i = 0; i < datasize; i++) {
						V_DBG(VPU_DBG_IO_FB_INFO, "0x%02x ", ptr[i]);
					}

					V_DBG(VPU_DBG_IO_FB_INFO, "==========");
				}
				#endif

				hmgr_data.check_interrupt_detection = 1;
				ret = tcc_hevc_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
						(vcodec_handle_t *)&pHandle,
						(void *)(&arg->gsHevcDecInput),
						(void *)(&arg->gsHevcDecOutput));

				dprintk_hevcd(
					"Dec :: Dec Out => %d - %d - %d, %d - %d - %d",
					arg->gsHevcDecOutput.m_DecOutInfo.m_iDisplayWidth,
					arg->gsHevcDecOutput.m_DecOutInfo.m_DisplayCropInfo.m_iCropLeft,
					arg->gsHevcDecOutput.m_DecOutInfo.m_DisplayCropInfo.m_iCropRight,
					arg->gsHevcDecOutput.m_DecOutInfo.m_iDisplayHeight,
					arg->gsHevcDecOutput.m_DecOutInfo.m_DisplayCropInfo.m_iCropTop,
					arg->gsHevcDecOutput.m_DecOutInfo.m_DisplayCropInfo.m_iCropBottom);

				dprintk_hevcd(
					"Dec :: Dec Out => ret[%d] !! PicType[%d], OutIdx[%d/%d], OutStatus[%d/%d]",
					ret, arg->gsHevcDecOutput.m_DecOutInfo.m_iPicType,
					arg->gsHevcDecOutput.m_DecOutInfo.m_iDispOutIdx,
					arg->gsHevcDecOutput.m_DecOutInfo.m_iDecodedIdx,
					arg->gsHevcDecOutput.m_DecOutInfo.m_iOutputStatus,
					arg->gsHevcDecOutput.m_DecOutInfo.m_iDecodingStatus);
				dprintk_hevcd(
					"Dec :: Dec Out => dec_Idx(%d), %#x %#x %#x / %#x %#x %#x",
					arg->gsHevcDecOutput.m_DecOutInfo.m_iDispOutIdx,
					(unsigned int)arg->gsHevcDecOutput.m_pDispOut[PA][0],
					(unsigned int)arg->gsHevcDecOutput.m_pDispOut[PA][1],
					(unsigned int)arg->gsHevcDecOutput.m_pDispOut[PA][2],
					(unsigned int)arg->gsHevcDecOutput.m_pDispOut[VA][0],
					(unsigned int)arg->gsHevcDecOutput.m_pDispOut[VA][1],
					(unsigned int)arg->gsHevcDecOutput.m_pDispOut[VA][2]);
				dprintk_hevcd(
					"Dec :: Dec Out => disp_Idx(%d), %#x %#x %#x / %#x %#x %#x",
					arg->gsHevcDecOutput.m_DecOutInfo.m_iDecodedIdx,
					(unsigned int)arg->gsHevcDecOutput.m_pCurrOut[PA][0],
					(unsigned int)arg->gsHevcDecOutput.m_pCurrOut[PA][1],
					(unsigned int)arg->gsHevcDecOutput.m_pCurrOut[PA][2],
					(unsigned int)arg->gsHevcDecOutput.m_pCurrOut[VA][0],
					(unsigned int)arg->gsHevcDecOutput.m_pCurrOut[VA][1],
					(unsigned int)arg->gsHevcDecOutput.m_pCurrOut[VA][2]);

				if (arg->gsHevcDecOutput.m_DecOutInfo.m_iDecodingStatus
					== VPU_DEC_BUF_FULL) {
					err_hevcd("Buffer full");
				}

				if (isFlexible) {
					v2hevcmgr_marshal_op_frmdata(args, (ret == RETCODE_SUCCESS));
				}

				hmgr_data.nDecode_Cmd++;
#ifdef CONFIG_VPU_TIME_MEASUREMENT
				endTime = vetc_GetKtime();
#endif
			} else {
				err_hevcd("Dec :: VPU_DEC_DECODE :: Undefined args. (cmd=%d)", cmd);
				ret = RETCODE_FAILURE;
			}
		}
		break;

		case VPU_DEC_BUF_FLAG_CLEAR:
		case VPU_DEC_BUF_FLAG_CLEAR_KERNEL:
		{
			int *arg = (int *)args;

			dprintk_hevcd("Dec :: DispIdx Clear %d", *arg);
			ret =
			tcc_hevc_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle, (void *)(arg),
				(void *)NULL);
		}
		break;

		case V2D_IP_FRM_CLEAR:
		{
			int slot = -1;
			struct v2hw_flex_io *fli = (struct v2hw_flex_io *)args;
			V2_FLEXIP_GET(fli, CLEAR_FB_IDX, slot);

			ret = tcc_hevc_dec_l(VPU_DEC_BUF_FLAG_CLEAR,
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
					ret = tcc_hevc_dec_l(VPU_DEC_BUF_FLAG_CLEAR,
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
			HEVC_DECODE_t *arg = NULL;
			bool isFlexible = (cmd == V2D_IP_FRM_DRAIN);
			if (isFlexible) {
				arg = (HEVC_DECODE_t *) v2hevcmgr_unmarshal_ip_drndata(args);
				cmd = VPU_DEC_FLUSH_OUTPUT;
			} else {
				arg = (HEVC_DECODE_t *) args;
			}

			if (arg != NULL) {
				dprintk_hevcd("Dec :: HEVC_DEC_FLUSH_OUTPUT !!");
				ret = tcc_hevc_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
					(vcodec_handle_t *)&pHandle,
					(void *)(&arg->gsHevcDecInput),
					(void *)(&arg->gsHevcDecOutput));

				if (isFlexible) {
					v2hevcmgr_marshal_op_frmdata(args, (ret == RETCODE_SUCCESS));
				}
			} else {
				err_hevcd("Dec :: VPU_DEC_FLUSH_OUTPUT :: Undefined args. (cmd=%d)", cmd);
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

			hmgr_data.check_interrupt_detection = 1;
			ret =
			tcc_hevc_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle, (void *)NULL,
				(void *)NULL);
			dprintk_hevcd("Dec :: HEVC_DEC_CLOSED!!");
			(void)hmgr_set_close(type, 1, 1);
		}
		break;

		case GET_RING_BUFFER_STATUS:
		case GET_RING_BUFFER_STATUS_KERNEL:
		case V2D_IP_RNG_GETPOS:
		{
			HEVC_RINGBUF_GETINFO_t *arg;

			bool isFlexible = (cmd == V2D_IP_RNG_GETPOS);
			if (isFlexible) {
				arg = (HEVC_RINGBUF_GETINFO_t *)v2hevcmgr_unmarshal_ip_getpos(args);
				cmd = GET_RING_BUFFER_STATUS;
			} else {
				arg = (HEVC_RINGBUF_GETINFO_t *)args;
			}

			if (arg != NULL) {
				hmgr_data.check_interrupt_detection = 1;

				ret = tcc_hevc_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
					(vcodec_handle_t *)&pHandle, (void *)NULL,
					(void *)(&arg->gsHevcDecRingStatus));

				if (isFlexible && (ret == RETCODE_SUCCESS)) {
					v2hevcmgr_marshal_op_getpos(args);
				}
			} else {
				err_hevcd("Dec :: GET_RING_BUFFER_STATUS :: Undefined args. (cmd=%d)", cmd);
			}
		}
		break;

		case FILL_RING_BUFFER_AUTO:
		case FILL_RING_BUFFER_AUTO_KERNEL:
		{
			HEVC_RINGBUF_SETBUF_t *arg =
				(HEVC_RINGBUF_SETBUF_t *)args;

			uint32_t read_ptr = vetc_reg_read(hmgr_data.base_addr, 0x120);
			uint32_t write_ptr = vetc_reg_read(hmgr_data.base_addr, 0x124);

			if (arg != NULL) {
				hmgr_data.check_interrupt_detection = 1;
				ret = tcc_hevc_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
					(vcodec_handle_t *)&pHandle,
					(void *)(&arg->gsHevcDecInit),
					(void *)(&arg->gsHevcDecRingFeed));
				dprintk_hevcd(
				"Dec :: ReadPTR : 0x%08x, WritePTR : 0x%08x",
					read_ptr, write_ptr);
			} else {
				err_hevcd("Dec :: FILL_RING_BUFFER_AUTO :: Undefined args. (cmd=%d)", cmd);
			}
		}
		break;

		case VPU_UPDATE_WRITE_BUFFER_PTR:
		case VPU_UPDATE_WRITE_BUFFER_PTR_KERNEL:
		case V2D_IP_RNG_SETPOS:
		{
			HEVC_RINGBUF_SETBUF_PTRONLY_t *arg =
				(HEVC_RINGBUF_SETBUF_PTRONLY_t *)args;

			union {
				int i_data;
				int *pi_data;	//NULL
				void *pv_data;
			} ucopysize, flushbuf;

			bool isFlexible = (cmd == V2D_IP_RNG_SETPOS);
			if (isFlexible) {
				arg = (HEVC_RINGBUF_SETBUF_PTRONLY_t *)v2hevcmgr_unmarshal_ip_setpos(args);
				cmd = VPU_UPDATE_WRITE_BUFFER_PTR;
			} else {
				arg = (HEVC_RINGBUF_SETBUF_PTRONLY_t *)args;
			}

			if (arg != NULL) {
				ucopysize.pi_data = NULL;
				ucopysize.i_data = arg->iCopiedSize;
				flushbuf.pi_data = NULL;
				flushbuf.i_data = arg->iFlushBuf;

				hmgr_data.check_interrupt_detection = 1;
				ret = tcc_hevc_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
					(vcodec_handle_t *)&pHandle,
					(void *)ucopysize.pv_data,
					(void *)flushbuf.pv_data);
			} else {
				err_hevcd("Dec :: VPU_UPDATE_WRITE_BUFFER_PTR :: Undefined args. (cmd=%d)", cmd);
				ret = RETCODE_FAILURE;
			}
		}
		break;

		case GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY:
		case GET_INITIAL_INFO_KERNEL_FOR_STREAMING_MODE_ONLY:
		{
			HEVC_SEQ_HEADER_t *arg =
				(HEVC_SEQ_HEADER_t *)args;

			if (arg != NULL) {
				hmgr_data.check_interrupt_detection = 1;
				ret = tcc_hevc_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
					(vcodec_handle_t *)&pHandle,
					(void *)(&arg->gsHevcDecInitialInfo), NULL);
			} else {
				err_hevcd("Dec :: GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY :: Undefined args. (cmd=%d)", cmd);
			}
		}
		break;

		case VPU_CODEC_GET_VERSION:
		case VPU_CODEC_GET_VERSION_KERNEL:
		{
			HEVC_GET_VERSION_t *arg =
				(HEVC_GET_VERSION_t *)args;

			if (arg != NULL) {
				hmgr_data.check_interrupt_detection = 1;
				ret = tcc_hevc_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
						(vcodec_handle_t *)&pHandle,
						arg->pszVersion, arg->pszBuildData);
				dprintk_hevcd("Dec :: version : %s, build : %s",
						arg->pszVersion, arg->pszBuildData);
			} else {
				err_hevcd("Dec :: VPU_CODEC_GET_VERSION :: Undefined args. (cmd=%d)", cmd);
			}
		}
		break;

		case VPU_DEC_SWRESET:
		case VPU_DEC_SWRESET_KERNEL:
		{
			ret = tcc_hevc_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle, NULL, NULL);
		}
		break;

		default:
		{
			err_hevcd("Dec :: not supported command(0x%x)", cmd);
			ret = 0x999;
		}
		break;
		}
	}
#if DEFINED_CONFIG_VENC_CNT_1to16
	else {
		err_hevcd(
		"Enc :: Encoder for HEVC do not support. command(0x%x)",
			cmd);
		ret = 0x999;
	}
#endif

#ifdef CONFIG_VPU_TIME_MEASUREMENT
	time_gap_us = vetc_GetTimediff_us(endTime, startTime);

	if (cmd == VPU_DEC_DECODE) {
		printMeasurementTime((void *)&hmgr_data, type, 1, time_gap_us);
	}
#endif

	return ret;
}

static int hmgr_proc_exit_by_external(struct VpuList *list, int *result, unsigned int type)
{
	if ((hmgr_get_close(type) == 0) && (hmgr_data.handle[type] != 0x00)) {
		list->type = type;
		if (type >= (unsigned int)VPU_ENC) {
			list->cmd_type = VPU_ENC_CLOSE;
		} else {
			list->cmd_type = VPU_DEC_CLOSE;
		}
		list->handle = hmgr_data.handle[type];
		list->args = NULL;
		list->comm_data = NULL;
		list->vpu_result = result;

		err_hevcd("%s for %d!!", __func__, type);
		(void)hmgr_list_manager(list, (int)LIST_ADD);

		return 1;
	}

	return 0;
}

#if 0 // Keep the code for future use
static void hmgr_wait_process(int wait_ms)
{
	int max_count = wait_ms/20;

	//wait!! in case exceptional processing. ex). sdcard out!!
	while (hmgr_data.cmd_processing) {
		max_count--;
		msleep(20);

		if (max_count <= 0) {
			err_hevcd("cmd_processing(cmd %d) didn't finish!!", hmgr_data.current_cmd);
			break;
		}
	}
}
#endif

static int hmgr_external_all_close(int wait_ms)
{
	unsigned int type = 0;
	int max_count = 0;
	int ret = 0;

	for (type = 0; type < (unsigned int)HEVC_MAX; type++) {
		if (hmgr_proc_exit_by_external(
			&hmgr_data.vList[type], &ret, type)) {
			max_count = wait_ms / 10;

			while (!hmgr_get_close(type)) {
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

static int hmgr_cmd_open(char *str)
{
	int ret = 0;

	dprintk_hevcd("======> hmgr_%s_open In!! %d'th", str, atomic_read(&hmgr_data.opened));

	hmgr_enable_clock(0);

	if (atomic_read(&hmgr_data.opened) == 0) {
#ifdef FORCED_ERROR
		forced_error_count = FORCED_ERR_CNT;
#endif
#if DEFINED_CONFIG_VENC_CNT_1to16
		hmgr_data.only_decmode = 0;
#else
		hmgr_data.only_decmode = 1;
#endif
		hmgr_data.clk_limitation = 1;
		hmgr_data.cmd_processing = 0;

		hmgr_hw_reset();
		hmgr_enable_irq(hmgr_data.irq);
		ret = vmem_init();
		if (ret < 0) {
			err_hevcd("failed to allocate memory for VPU!! %d", ret);
		}
	}
	atomic_inc(&hmgr_data.opened);

	dprintk_hevcd("======> hmgr_%s_open Out!! %d'th", str, atomic_read(&hmgr_data.opened));

	return ret;
}

static int hmgr_cmd_release(char *str)
{
	dprintk_hevcd("======> hmgr_%s_release In!! %d'th", str,
		atomic_read(&hmgr_data.opened));

	if (atomic_read(&hmgr_data.opened) > 0)
		atomic_dec(&hmgr_data.opened);

	if (atomic_read(&hmgr_data.opened) == 0) {
		unsigned int type = 0;
		int alive_cnt = 0;

#if 1	//To close whole hevc instance
		//when being killed process opened this.
		if (!hmgr_data.bVpu_already_proc_force_closed) {
			hmgr_data.external_proc = 1;
			hmgr_external_all_close(200);
			hmgr_data.external_proc = 0;
		}
		hmgr_data.bVpu_already_proc_force_closed = (bool)false;
#endif

		for (type = 0; type < (unsigned int)HEVC_MAX; type++) {
			if (hmgr_data.closed[type] == 0) {
				if (alive_cnt < (int)INT_MAX) {
					alive_cnt++;
				}
			}
		}

		if (alive_cnt != 0) {
			V_DBG(VPU_DBG_CLOSE, "HEVC might be cleared by force.");
		}

		atomic_set(&hmgr_data.oper_intr, 0);
		hmgr_data.cmd_processing = 0;

		hmgr_close_all(1);

		hmgr_disable_irq(hmgr_data.irq);
		hmgr_BusPrioritySetting(BUS_FOR_NORMAL, 0);

		vmem_deinit();
		hmgr_hw_assert();

		udelay(1000); //1ms
	}

	hmgr_disable_clock(0);

	hmgr_data.nOpened_Count++;

	V_DBG(VPU_DBG_CLOSE,
	"======> hmgr_%s_release Out!! %d'th, total = %d  - DEC(%d/%d/%d/%d/%d)",
		str, atomic_read(&hmgr_data.opened),
		hmgr_data.nOpened_Count,
		hmgr_get_close(VPU_DEC),
		hmgr_get_close(VPU_DEC_EXT),
		hmgr_get_close(VPU_DEC_EXT2),
		hmgr_get_close(VPU_DEC_EXT3),
		hmgr_get_close(VPU_DEC_EXT4));

	return 0;
}

static long hmgr_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	CONTENTS_INFO info;
	OPENED_sINFO open_info;
	union {
		unsigned long ul_data;
		unsigned int *pui_data;
		int *pi_data;
		void *pv_data;
		CONTENTS_INFO *pci_data;
		OPENED_sINFO *posi_data;
	} uarg;

	VPU_UNUSED_PARAMETER(filp);

	uarg.pv_data = NULL;
	uarg.ul_data = arg;

	mutex_lock(&hmgr_data.comm_data.io_mutex);

	switch (cmd) {
	case VPU_SET_CLK:
	case VPU_SET_CLK_KERNEL:
	{
		if (cmd == (unsigned int)VPU_SET_CLK_KERNEL) {
			(void)memcpy(&info, uarg.pci_data, sizeof(info));
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
		unsigned int type = 0;
		unsigned int freemem_sz;

		if (cmd == (unsigned int)VPU_GET_FREEMEM_SIZE_KERNEL) {
			(void)memcpy(&type, uarg.pui_data, sizeof(unsigned int));
		} else {
			if (copy_from_user(&type, uarg.pui_data, sizeof(unsigned int)) != 0U) {
				ret = -EFAULT;
			}
		}

		if (ret == 0) {
			if (type > (unsigned int)VPU_MAX) {
				type = (unsigned int)VPU_DEC;
			}
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
		hmgr_hw_reset();
	break;

	case VPU_SET_MEM_ALLOC_MODE:
	case VPU_SET_MEM_ALLOC_MODE_KERNEL:
	{
		if (cmd == (unsigned int)VPU_SET_MEM_ALLOC_MODE_KERNEL) {
			(void)memcpy(&open_info, uarg.posi_data, sizeof(OPENED_sINFO));
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
			(void)memcpy(uarg.pi_data, hmgr_data.closed, sizeof(hmgr_data.closed));
		} else {
			if (copy_to_user(uarg.pi_data, hmgr_data.closed, sizeof(hmgr_data.closed)) != 0U) {
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
			(void)memcpy(&iInst, (int *)arg,
				sizeof(INSTANCE_INFO));
		} else {
			if (copy_from_user(&iInst, (int *)arg,
				sizeof(INSTANCE_INFO)) != 0U) {
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
		if (hmgr_data.bVpu_already_proc_force_closed == (bool)false) {
			hmgr_data.external_proc = 1;
			hmgr_external_all_close(200);
			hmgr_data.external_proc = 0;
			hmgr_data.bVpu_already_proc_force_closed = (bool)true;
		}
	}
	break;

	case VPU_TRY_CLK_RESTORE:
	case VPU_TRY_CLK_RESTORE_KERNEL:
	{
		hmgr_restore_clock(0,
			atomic_read(&hmgr_data.opened));
	}
	break;

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	case VPU_TRY_OPEN_DEV:
	case VPU_TRY_OPEN_DEV_KERNEL:
		(void)hmgr_cmd_open("cmd");
		break;

	case VPU_TRY_CLOSE_DEV:
	case VPU_TRY_CLOSE_DEV_KERNEL:
		(void)hmgr_cmd_release("cmd");
		break;
#endif

	default:
		err_hevcd("Unsupported ioctl[%d]!!!", cmd);
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&hmgr_data.comm_data.io_mutex);

	return ret;
}

#ifdef CONFIG_COMPAT
static long hmgr_compat_ioctl(struct file *filep, unsigned int cmd,
				   unsigned long arg)
{
	unsigned int tmpArg = 0U;

	if (arg < UINT_MAX) {
		tmpArg = (unsigned int)arg;
	}
	return hmgr_ioctl(filep, cmd, (unsigned long)compat_ptr(tmpArg));
}
#endif

static irqreturn_t hmgr_isr_handler(int irq, void *dev_id)
{
	VPU_UNUSED_PARAMETER(irq);
	VPU_UNUSED_PARAMETER(dev_id);

	atomic_inc(&hmgr_data.oper_intr);

	wake_up_interruptible(&hmgr_data.oper_wq);

	return (irqreturn_t)IRQ_HANDLED;
}

static int hmgr_open(struct inode *pinode, struct file *filp)
{
	if (hmgr_data.irq_reged == 0U) {
		err_hevcd("not registered hevc-mgr-irq");
	}

#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	dprintk_hevcd("enter!! %d'th", atomic_read(&hmgr_data.dev_file_opened));
	atomic_inc(&hmgr_data.dev_file_opened);
	dprintk_hevcd("%s Out!! %d'th", atomic_read(&hmgr_data.dev_file_opened));
#else
	mutex_lock(&hmgr_data.comm_data.file_mutex);
	hmgr_cmd_open(hevcd_fname_file);
	mutex_unlock(&hmgr_data.comm_data.file_mutex);
#endif

	filp->private_data = &hmgr_data;
	LOG_COVERITY("%p", pinode);

	return 0;
}

static int hmgr_release(struct inode *pinode, struct file *filp)
{
#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	V_DBG(VPU_DBG_CLOSE, "enter!! %d'th", atomic_read(&hmgr_data.dev_file_opened));
	atomic_dec(&hmgr_data.dev_file_opened);
	hmgr_data.nOpened_Count++;

	V_DBG(VPU_DBG_CLOSE,
		"Out!! %d'th, total = %d  - DEC(%d/%d/%d/%d/%d)",
		atomic_read(&hmgr_data.dev_file_opened),
		hmgr_data.nOpened_Count,
		hmgr_get_close(VPU_DEC),
		hmgr_get_close(VPU_DEC_EXT),
		hmgr_get_close(VPU_DEC_EXT2),
		hmgr_get_close(VPU_DEC_EXT3),
		hmgr_get_close(VPU_DEC_EXT4));
#else
	mutex_lock(&hmgr_data.comm_data.file_mutex);
	hmgr_cmd_release(hevcd_fname_file);
	mutex_unlock(&hmgr_data.comm_data.file_mutex);
#endif
	LOG_COVERITY("%p,%p", pinode, filp);
	return 0;
}

struct VpuList *hmgr_list_manager(struct VpuList *args, unsigned int cmd)
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

	mutex_lock(&hmgr_data.comm_data.list_mutex);

	switch (cmd) {
	case LIST_ADD:
		*oper_data->vpu_result |= RET1;
		list_add_tail(&oper_data->list,
			&hmgr_data.comm_data.main_list);
		if (hmgr_data.cmd_queued > INT_MAX) {
			V_DBG(VPU_DBG_ERROR, "Cmd queued is already FULL");
		} else {
			hmgr_data.cmd_queued++;
		}

		if (hmgr_data.comm_data.thread_intr > INT_MAX) {
			V_DBG(VPU_DBG_ERROR, "Comm data thread interrupt count is already NULL");
		} else {
			hmgr_data.comm_data.thread_intr++;
		}
		break;
	case LIST_DEL:
		list_del(&oper_data->list);
		if (hmgr_data.cmd_queued > 0) {
			hmgr_data.cmd_queued--;
		}
		break;
	case LIST_IS_EMPTY:
		if (list_empty(&hmgr_data.comm_data.main_list) != 0) {
			ret = &hevc_mgr_vlist;
		}
		break;
	case LIST_GET_ENTRY:
		ret = list_first_entry(
				&hmgr_data.comm_data.main_list,
				struct VpuList, list);
		break;
	default:
		/* Nothing to do */
		break;
	}

	mutex_unlock(&hmgr_data.comm_data.list_mutex);

	if (cmd == (unsigned int)LIST_ADD) {
		wake_up_interruptible(&hmgr_data.comm_data.thread_wq);
	}

	return ret;
}

static int hmgr_operation(void)
{
	int oper_finished;
	struct VpuList *oper_data = NULL;

	while (hmgr_list_manager(NULL, (int)LIST_IS_EMPTY) == NULL) {
		hmgr_data.cmd_processing = 1;

		oper_finished = 1;
		dprintk_hevcd("%s :: not empty cmd_queued(%d)", __func__, hmgr_data.cmd_queued);

		oper_data = (struct VpuList *)hmgr_list_manager(NULL, (int)LIST_GET_ENTRY);

		if (oper_data == NULL) {
			err_hevcd("data is null");
			hmgr_data.cmd_processing = 0;
			return 0;
		}
		*oper_data->vpu_result |= RET2;

		dprintk_hevcd("%s [%d] :: cmd =",
			__func__, oper_data->type);
		dprintk_hevcd("0x%x, hmgr_data.cmd_queued(%d)",
			 oper_data->cmd_type,
			 hmgr_data.cmd_queued);

		if (oper_data->type < HEVC_MAX
			&& oper_data !=
			NULL /*&& oper_data->comm_data != NULL*/) {
			*oper_data->vpu_result |= RET3;

			*oper_data->vpu_result =
				hmgr_process(oper_data->type,
					oper_data->cmd_type,
					oper_data->handle,
					oper_data->args);
			oper_finished = 1;

			if (*oper_data->vpu_result != RETCODE_SUCCESS) {
				if ((*oper_data->vpu_result
					!= RETCODE_INSUFFICIENT_BITSTREAM)
					&& (*oper_data->vpu_result !=
					RETCODE_INSUFFICIENT_BITSTREAM_BUF)) {
					err_hevcd(
					"hmgr_out[0x%x] :: type = %d, hmgr_data.handle = 0x%x, cmd = 0x%x, frame_len=%d",
						*oper_data->vpu_result,
						oper_data->type,
						oper_data->handle,
						oper_data->cmd_type,
						hmgr_data.szFrame_Len);
				}

				if (*oper_data->vpu_result
					== RETCODE_CODEC_EXIT) {
					hmgr_restore_clock(0,
						atomic_read(&hmgr_data.opened));
					hmgr_close_all(1);
				}
			}
		} else {
			err_hevcd(
			"hmgr_operation_fn :: missed info or unknown command => type = 0x%x, cmd = 0x%x",
			 oper_data->type, oper_data->cmd_type);

			*oper_data->vpu_result = RETCODE_FAILURE;
			oper_finished = 0;
		}

		if (oper_finished != 0) {
			int opened = atomic_read(&hmgr_data.opened);
			if ((oper_data->comm_data != NULL)
				 && (opened != 0)) {
				oper_data->comm_data->count++;
				if (oper_data->comm_data->count != 1U) {
					dprintk_hevcd(
					"poll wakeup count = %d :: type(0x%x) cmd(0x%x)",
						oper_data->comm_data->count,
						oper_data->type,
						oper_data->cmd_type);
				}

				wake_up_interruptible(
					&oper_data->comm_data->wq);
			} else {
				err_hevcd(
				"Error: abnormal exception or external command was processed!! 0x%p -%d",
				  oper_data->comm_data, atomic_read(&hmgr_data.opened));
			}
		} else {
			err_hevcd("Error: abnormal exception 2!! 0x%p - %d",
				oper_data->comm_data, atomic_read(&hmgr_data.opened));
		}

		(void)hmgr_list_manager(oper_data, (int)LIST_DEL);

		hmgr_data.cmd_processing = 0;
	}

	return 0;
}

static int hmgr_thread(void *kthread)
{
	unsigned long jtimeout;

	VPU_UNUSED_PARAMETER(kthread);
	V_DBG(VPU_DBG_THREAD, "enter");

	jtimeout = msecs_to_jiffies(50);
	if (jtimeout > (ULONG_MAX / 2UL)) {
		jtimeout = ((ULONG_MAX / 2UL) - 1UL);
	}

	do {
		if (hmgr_list_manager(NULL, (int)LIST_IS_EMPTY) != NULL) {
			hmgr_data.cmd_processing = 0;
			(void)wait_event_interruptible_timeout(
				hmgr_data.comm_data.thread_wq,
				hmgr_data.comm_data.thread_intr > 0,
				(long)jtimeout);
			hmgr_data.comm_data.thread_intr = 0;
		} else {
			if ((atomic_read(&hmgr_data.opened) != 0)
				|| (hmgr_data.external_proc != 0U)) {
				(void)hmgr_operation();
			} else {
				struct VpuList *oper_data = NULL;

				err_hevcd("DEL for empty");

				oper_data = hmgr_list_manager(NULL, (int)LIST_GET_ENTRY);
				if (oper_data != NULL) {
					(void)hmgr_list_manager(oper_data, (int)LIST_DEL);
				}
			}
		}
	} while (!kthread_should_stop());

	V_DBG(VPU_DBG_THREAD, "out");

	return 0;
}

static int hmgr_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret = 0;
	unsigned long current_vm_range = (vma->vm_end >= vma->vm_start) ?
		(vma->vm_end - vma->vm_start) : 0U;

	VPU_UNUSED_PARAMETER(filp);

#if defined(CONFIG_TCC_MEM)
	if (vma->vm_end < vma->vm_start) {
		err_hevcd("this address is not allowed");
		return -EAGAIN;
	}

	if (range_is_allowed(vma->vm_pgoff, current_vm_range) < 0) {
		err_hevcd("this address is not allowed");
		return -EAGAIN;
	}
#endif

	vma->vm_page_prot = vmem_get_pgprot(vma->vm_page_prot, vma->vm_pgoff);
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, current_vm_range, vma->vm_page_prot) != 0) {
		err_hevcd("remap_pfn_range failed");
		return -EAGAIN;
	}

	vma->vm_ops = NULL;
	vetc_vm_flags_set(vma, VM_IO | VM_DONTEXPAND | VM_PFNMAP);

	return ret;
}

static const struct file_operations hmgr_fops = {
	.open = hmgr_open,
	.release = hmgr_release,
	.mmap = hmgr_mmap,
	.unlocked_ioctl = hmgr_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = hmgr_compat_ioctl,
#endif
};

static struct miscdevice hmgr_misc_device = {
	MISC_DYNAMIC_MINOR,
	HMGR_NAME,
	&hmgr_fops,
};

int hmgr_probe(struct platform_device *pdev)
{
	int ret;
	int type = 0;
	unsigned long int_flags;
	struct resource *resource = NULL;
	void *tTmpPtr = NULL;

	if (pdev->dev.of_node == NULL) {
		return -ENODEV;
	}

	dprintk_hevcd("hmgr initializing!!");
	(void)memset(&hmgr_data, 0, sizeof(struct mgr_data_t));
	for (type = 0; type < HEVC_MAX; type++) {
		hmgr_data.closed[type] = 1;
	}

	hmgr_init_variable();
	atomic_set(&hmgr_data.oper_intr, 0);
#ifdef USE_DEV_OPEN_CLOSE_IOCTL
	atomic_set(&hmgr_data.dev_file_opened, 0);
#endif

	ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		err_hevcd("could not get IRQ");
		return -1;
	} else {
		hmgr_data.irq = (unsigned int)ret;
	}

	hmgr_data.nOpened_Count = 0;
	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (resource == NULL) {
		dev_err(&pdev->dev, "missing phy memory resource");
		return -1;
	}
	resource->end += 1;

	hmgr_data.base_addr = devm_ioremap(&pdev->dev, resource->start,
			(resource->end - resource->start));

	if (hmgr_data.irq > 32) {
		dprintk_hevcd("========> HEVC base address [0x%x -> 0x%p], irq num [%d]",
			resource->start, hmgr_data.base_addr, (hmgr_data.irq - 32));
	} else {
		err_hevcd("error:: hmgr_data.irq is under 32. (hmgr_data.irq: %d)", hmgr_data.irq);
	}

	hmgr_get_clock(pdev->dev.of_node);
	hmgr_get_reset(pdev->dev.of_node);

	init_waitqueue_head(&hmgr_data.comm_data.thread_wq);
	init_waitqueue_head(&hmgr_data.oper_wq);

	mutex_init(&hmgr_data.comm_data.list_mutex);
	mutex_init(&(hmgr_data.comm_data.io_mutex));
	mutex_init(&(hmgr_data.comm_data.file_mutex));

	INIT_LIST_HEAD(&hmgr_data.comm_data.main_list);
	INIT_LIST_HEAD(&hmgr_data.comm_data.wait_list);

	ret = vmem_config();
	if (ret < 0) {
		err_hevcd("unable to configure memory for VPU!! %d", ret);
		return -ENOMEM;
	}

#if defined(USE_ACCESS_POINT)
	if (stHEVCFunc == NULL) {
		ret = get_hevc_access_addr();
		if (ret != 0) {
			V_DBG(VPU_DBG_ERROR, "Getting for library access point failed!!");
			return RETCODE_FAILURE;
		}
	}
#endif

	hmgr_init_interrupt();
	int_flags = hmgr_get_int_flags();
	ret = hmgr_request_irq(hmgr_data.irq, hmgr_isr_handler,
			 int_flags, HMGR_NAME, &hmgr_data);
	if (ret) {
		err_hevcd("to aquire hevc-dec-irq");
	}

	hmgr_data.irq_reged = 1;
	hmgr_disable_irq(hmgr_data.irq);

	kidle_task_hevcd = kthread_run(hmgr_thread, NULL, "vHEVC_th");
	VPU_CAST_PT(tTmpPtr, kidle_task_hevcd);
	if (IS_ERR(tTmpPtr)) {
		err_hevcd("unable to create thread!!");
		kidle_task_hevcd = NULL;
		return -1;
	}
	dprintk_hevcd("success :: thread created!!");

	hmgr_close_all(1);

	if (misc_register(&hmgr_misc_device)) {
		(void)pr_info("HEVC Manager: Couldn't register device.");
		return -EBUSY;
	}

	return 0;
}
EXPORT_SYMBOL(hmgr_probe);

VREMOVE_RET_TYPE hmgr_remove(struct platform_device *pdev)
{
	misc_deregister(&hmgr_misc_device);

	if (kidle_task_hevcd != NULL) {
		(void)kthread_stop(kidle_task_hevcd);
		kidle_task_hevcd = NULL;
	}

	devm_iounmap(&pdev->dev, hmgr_data.base_addr);
	if (hmgr_data.irq_reged != 0U) {
		(void)hmgr_free_irq(hmgr_data.irq, &hmgr_data);
		hmgr_data.irq_reged = 0;
	}

	hmgr_put_clock();
	hmgr_put_reset();
	vmem_deinit();

	// comment: to avoid HIS metric violation(HIS_CALLS)
	//(void)pr_info("success :: hmgr thread stopped\n!!");

	VREMOVE_RETURN();
}
EXPORT_SYMBOL(hmgr_remove);

#if defined(CONFIG_PM)
int hmgr_suspend(struct platform_device *pdev, pm_message_t state)
{
	int i, open_count = 0;

	VPU_UNUSED_PARAMETER(pdev);
	VPU_UNUSED_PARAMETER(state);

	if (atomic_read(&hmgr_data.opened) != 0) {
		(void)pr_info("hevc: suspend In DEC(%d/%d/%d/%d/%d)\n",
			hmgr_get_close(VPU_DEC),
			hmgr_get_close(VPU_DEC_EXT),
			hmgr_get_close(VPU_DEC_EXT2),
			hmgr_get_close(VPU_DEC_EXT3),
			hmgr_get_close(VPU_DEC_EXT4));

		(void)hmgr_external_all_close(200);

		open_count = atomic_read(&hmgr_data.opened);

		for (i = 0; i < open_count; i++) {
			hmgr_disable_clock(0);
		}

		(void)pr_info("hevc: suspend Out DEC(%d/%d/%d/%d/%d)\n",
			hmgr_get_close(VPU_DEC),
			hmgr_get_close(VPU_DEC_EXT),
			hmgr_get_close(VPU_DEC_EXT2),
			hmgr_get_close(VPU_DEC_EXT3),
			hmgr_get_close(VPU_DEC_EXT4));
	}

	return 0;
}
EXPORT_SYMBOL(hmgr_suspend);

int hmgr_resume(struct platform_device *pdev)
{
	int i, open_count = 0;

	VPU_UNUSED_PARAMETER(pdev);

	if (atomic_read(&hmgr_data.opened) != 0) {

		open_count = atomic_read(&hmgr_data.opened);

		for (i = 0; i < open_count; i++) {
			hmgr_enable_clock(0);
		}

		(void)pr_info("\n hevc: resume\n\n");
	}

	return 0;
}

EXPORT_SYMBOL(hmgr_resume);
#endif

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
MODULE_SOFTDEP("pre: vpu_lib jpu_lib hevc_lib vpu_4k_d2_lib vpu_hevc_enc_lib vpu");

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC hevc manager");
MODULE_LICENSE("GPL");
#endif
