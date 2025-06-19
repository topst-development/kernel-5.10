// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_comm.h"

#ifdef ENABLE_VPU_DRV_HEVCDEC

#include "vpu_rm.h"
#include "vpu_devices.h"
#include "vpu_mgr.h"
#include "vpu_mgr_common.h"
#include "vpu_mgr_context.h"
#include "vpu_hevc_dec_mgr_sys.h"
#include "vpu_hevc_dec_mgr.h"
#include "vpu_dbg_info.h"

#define dlog_hevcd(msg...) 	 	V_DBG(VPU_DBG_INFO, "[HEVC_DEC][INFO]:" msg)
#define detail_hevcd(msg...)  	V_DBG(VPU_DBG_DETAIL, "[HEVC_DEC][DETAIL]:" msg)
#define seq_hevcd(msg...)     	V_DBG(VPU_DBG_SEQUENCE, "[HEVC_DEC][SEQ]:" msg)
#define err_hevcd(msg...)       V_DBG(VPU_DBG_ERROR, "[HEVC_DEC][ERR]:" msg)

#define HEVC_REGISTER_DUMP

#if 0 //For test purpose!!
#define FORCED_ERROR
#endif
#ifdef FORCED_ERROR
#define FORCED_ERR_CNT 300
static int forced_error_count = FORCED_ERR_CNT;
#endif

#ifdef HEVC_REGISTER_DUMP
#define W4_REG_BASE                 0x0000
#define W4_BS_RD_PTR                0x0130
#define W4_BS_WR_PTR                0x0134
#define W4_BS_OPTION                0x012C
#define W4_BS_PARAM                 0x0128

#define W4_VCPU_PDBG_RDATA_REG      0x001C
#define W4_VCPU_FIO_CTRL            0x0020
#define W4_VCPU_FIO_DATA            0x0024
#endif

static const int VPU_HEVC_DEC_NUM_OF_BITSTREAM_BUFFERS = 2;

//to avoid potential issues caused by stack frames, parameters are stored in the heap instead of using local variables.
//This value is assigned to ip_param of vpu_drv_info_t.
typedef struct vpu_hevc_dec_papam_t
{
	hevc_dec_init_t dec_init;
	hevc_dec_initial_info_t dec_initialInfo;
	hevc_dec_input_t seq_input;
	hevc_dec_buffer_t dec_buffer;
	hevc_dec_input_t dec_input;
	hevc_dec_output_t dec_output;
	hevc_dec_ring_buffer_status_out_t dec_ringbuffer_status;
} vpu_hevc_dec_papam_t;

static vpu_mgr_t* vpu_hevc_dec_mgr_ctx = NULL;

#define HEVC_ACCESSPOINT_PATH	 "/proc/hevc"

#if !defined(USE_ACCESS_POINT)
#if DEFINED_CONFIG_VDEC
extern int tcc_hevc_dec(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2);
#endif
#endif

static int tcc_hevc_dec_l(vpu_accesspoint_t* vpu_ap, int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2) VPU_NO_SANITIZE_CFI
{
	return vpu_ap->tccfp_vpu_dec(Op, pHandle, pParam1, pParam2);
}

#ifdef HEVC_REGISTER_DUMP
static unsigned int hmgr_FIORead(void* base_addr, unsigned int addr)
{
	unsigned int ctrl;
	unsigned int count = 0;
	unsigned int data = 0xffffffff;

	ctrl = (addr & 0xffff);
	ctrl |= (0 << 16);	/* read operation */
	vetc_reg_write(base_addr, W4_VCPU_FIO_CTRL, ctrl);
	count = 10000;
	while (count--)
	{
		ctrl = vetc_reg_read(base_addr, W4_VCPU_FIO_CTRL);
		if (ctrl & 0x80000000)
		{
			data = vetc_reg_read(base_addr, W4_VCPU_FIO_DATA);
			break;
		}
	}

	return data;
}

static int hmgr_FIOWrite(void* base_addr, unsigned int addr, unsigned int data)
{
	unsigned int ctrl;

	vetc_reg_write(base_addr, W4_VCPU_FIO_DATA, data);
	ctrl = (addr & 0xffff);
	ctrl |= (1 << 16);	/* write operation */
	vetc_reg_write(base_addr, W4_VCPU_FIO_CTRL, ctrl);

	return 1;
}

static unsigned int hmgr_ReadRegVCE(void* base_addr, unsigned int vce_addr)
{
#define VCORE_DBG_ADDR              0x8300
#define VCORE_DBG_DATA              0x8304
#define VCORE_DBG_READY             0x8308

	int vcpu_reg_addr;
	unsigned int udata = 0xffffffff;

	hmgr_FIOWrite(base_addr, VCORE_DBG_READY, 0);

	vcpu_reg_addr = vce_addr >> 2;

	hmgr_FIOWrite(base_addr, VCORE_DBG_ADDR, vcpu_reg_addr + 0x8000);

	if (hmgr_FIORead(base_addr, VCORE_DBG_READY) == 1)
	{
		udata = hmgr_FIORead(base_addr, VCORE_DBG_DATA);
	}

	return udata;
}

static void hmgr_dump_status(vpu_mgr_t* mgr_ctx)
{
	int rd, wr;
	unsigned int tq, ip, mc, lf;
	unsigned int avail_cu, avail_tu, avail_tc, avail_lf, avail_ip;
	unsigned int ctu_fsm, nb_fsm, cabac_fsm, cu_info, mvp_fsm, tc_busy;
	unsigned int lf_fsm, bs_data, bbusy, fv;
	unsigned int reg_val;
	unsigned int index;
	unsigned int vcpu_reg[31] = { 0, };
	int i = 0;

	V_DBG(VPU_DBG_REG_DUMP, "--------------------------------------");
	V_DBG(VPU_DBG_REG_DUMP, "--------- ---  VCPU_STATUS  ----------");
	V_DBG(VPU_DBG_REG_DUMP, "--------------------------------------");
	rd = vetc_reg_read(mgr_ctx->base_addr, W4_BS_RD_PTR);
	wr = vetc_reg_read(mgr_ctx->base_addr, W4_BS_WR_PTR);
	V_DBG(VPU_DBG_REG_DUMP, "RD_PTR:0x%08x WR_PTR:0x%08x BS_OPT:0x%08x BS_PARAM:0x%08x",
			rd, wr, vetc_reg_read(mgr_ctx->base_addr, W4_BS_OPTION),
	  		vetc_reg_read(mgr_ctx->base_addr, W4_BS_PARAM));

	// --------- VCPU register Dump
	V_DBG(VPU_DBG_REG_DUMP, "[+] VCPU REG Dump");
	for (index = 0; index < 25; index++)
	{
		vetc_reg_write(mgr_ctx->base_addr, 0x14, (1 << 9) | (index & 0xff));
		vcpu_reg[index] = vetc_reg_read(mgr_ctx->base_addr, W4_VCPU_PDBG_RDATA_REG);

		if (index < 16)
		{
			V_DBG(VPU_DBG_REG_DUMP, "0x%08x\t", vcpu_reg[index]);
			if ((index % 4) == 3)
			{
				V_DBG(VPU_DBG_REG_DUMP, "");
			}
		}
		else
		{
			switch (index)
			{
			case 16:
				V_DBG(VPU_DBG_REG_DUMP, "CR0: 0x%08x", vcpu_reg[index]);
				break;
			case 17:
				V_DBG(VPU_DBG_REG_DUMP, "CR1: 0x%08x", vcpu_reg[index]);
				break;
			case 18:
				V_DBG(VPU_DBG_REG_DUMP, "ML:  0x%08x", vcpu_reg[index]);
				break;
			case 19:
				V_DBG(VPU_DBG_REG_DUMP, "MH:  0x%08x", vcpu_reg[index]);
				break;
			case 21:
				V_DBG(VPU_DBG_REG_DUMP, "LR:  0x%08x", vcpu_reg[index]);
				break;
			case 22:
				V_DBG(VPU_DBG_REG_DUMP, "PC:  0x%08x", vcpu_reg[index]);
				break;
			case 23:
				V_DBG(VPU_DBG_REG_DUMP, "SR:  0x%08x", vcpu_reg[index]);
				break;
			case 24:
				V_DBG(VPU_DBG_REG_DUMP, "SSP: 0x%08x", vcpu_reg[index]);
				break;
			}
		}
	}

	V_DBG(VPU_DBG_REG_DUMP, "[-] VCPU REG Dump");
	// --------- BIT register Dump
	V_DBG(VPU_DBG_REG_DUMP, "[+] BPU REG Dump");
	V_DBG(VPU_DBG_REG_DUMP, "BITPC = 0x%08x", hmgr_FIORead(mgr_ctx->base_addr, (W4_REG_BASE + 0x8000 + 0x18)));

	for (i = 0; i < 10; i++)
	{
		V_DBG(VPU_DBG_REG_DUMP, "BITPC = 0x%08x", hmgr_FIORead(mgr_ctx->base_addr, (W4_REG_BASE + 0x8000 + 0x18)));
	}

	V_DBG(VPU_DBG_REG_DUMP, "BIT START=0x%08x, BIT END=0x%08x",
							hmgr_FIORead(mgr_ctx->base_addr, (W4_REG_BASE + 0x8000 + 0x11c)),
							hmgr_FIORead(mgr_ctx->base_addr, (W4_REG_BASE + 0x8000 + 0x120)));
	V_DBG(VPU_DBG_REG_DUMP, "BIT COMMAND 0x%x", hmgr_FIORead(mgr_ctx->base_addr, (W4_REG_BASE + 0x8000 + 0x1FC)));

	// --------- BIT HEVC Status Dump
	ctu_fsm = hmgr_FIORead(mgr_ctx->base_addr, (W4_REG_BASE + 0x8000 + 0x48));
	nb_fsm = hmgr_FIORead(mgr_ctx->base_addr, (W4_REG_BASE + 0x8000 + 0x4c));
	cabac_fsm = hmgr_FIORead(mgr_ctx->base_addr, (W4_REG_BASE + 0x8000 + 0x50));
	cu_info = hmgr_FIORead(mgr_ctx->base_addr, (W4_REG_BASE + 0x8000 + 0x54));
	mvp_fsm = hmgr_FIORead(mgr_ctx->base_addr, (W4_REG_BASE + 0x8000 + 0x58));
	tc_busy = hmgr_FIORead(mgr_ctx->base_addr, (W4_REG_BASE + 0x8000 + 0x5c));
	lf_fsm = hmgr_FIORead(mgr_ctx->base_addr, (W4_REG_BASE + 0x8000 + 0x60));
	bs_data = hmgr_FIORead(mgr_ctx->base_addr, (W4_REG_BASE + 0x8000 + 0x64));
	bbusy = hmgr_FIORead(mgr_ctx->base_addr, (W4_REG_BASE + 0x8000 + 0x68));
	fv = hmgr_FIORead(mgr_ctx->base_addr, (W4_REG_BASE + 0x8000 + 0x6C));

	V_DBG(VPU_DBG_REG_DUMP, "[DEBUG-BPUHEVC] CTU_X: %4d, CTU_Y: %4d",
							  hmgr_FIORead(mgr_ctx->base_addr, (W4_REG_BASE + 0x8000 + 0x40)),
							  hmgr_FIORead(mgr_ctx->base_addr, (W4_REG_BASE + 0x8000 + 0x44)));

	V_DBG(VPU_DBG_REG_DUMP, "[DEBUG-BPUHEVC] CTU_FSM>   Main: 0x%02x, FIFO: 0x%1x, NB: 0x%02x, DBK: 0x%1x",
							  ((ctu_fsm >> 24) & 0xff), ((ctu_fsm >> 16) & 0xff),
							  ((ctu_fsm >> 8) & 0xff), (ctu_fsm & 0xff));

	V_DBG(VPU_DBG_REG_DUMP, "[DEBUG-BPUHEVC] NB_FSM: 0x%02x", nb_fsm & 0xff);

	V_DBG(VPU_DBG_REG_DUMP, "[DEBUG-BPUHEVC] CABAC_FSM> SAO: 0x%02x, CU: 0x%02x, PU: 0x%02x, TU: 0x%02x, EOS: 0x%02x",
							  ((cabac_fsm >> 25) & 0x3f), ((cabac_fsm >> 19) & 0x3f),
							  ((cabac_fsm >> 13) & 0x3f), ((cabac_fsm >> 6) & 0x7f),
							  (cabac_fsm & 0x3f));

	V_DBG(VPU_DBG_REG_DUMP, "[DEBUG-BPUHEVC] CU_INFO value = 0x%04x\n\t\t(l2cb: 0x%1x, cux: %1d, cuy; %1d, pred: %1d, pcm: %1d, wr_done: %1d, par_done: %1d, nbw_done: %1d, dec_run: %1d)",
							  cu_info, ((cu_info >> 16) & 0x3), ((cu_info >> 13) & 0x7),
							  ((cu_info >> 10) & 0x7), ((cu_info >> 9) & 0x3),
							  ((cu_info >> 8) & 0x1), ((cu_info >> 6) & 0x3),
							  ((cu_info >> 4) & 0x3), ((cu_info >> 2) & 0x3), (cu_info & 0x3));

	V_DBG(VPU_DBG_REG_DUMP, "[DEBUG-BPUHEVC] MVP_FSM> 0x%02x", mvp_fsm & 0xf);
	V_DBG(VPU_DBG_REG_DUMP, "[DEBUG-BPUHEVC] TC_BUSY> tc_dec_busy: %1d, tc_fifo_busy: 0x%02x", ((tc_busy >> 3) & 0x1), (tc_busy & 0x7));
	V_DBG(VPU_DBG_REG_DUMP, "[DEBUG-BPUHEVC] LF_FSM>  SAO: 0x%1x, LF: 0x%1x", ((lf_fsm >> 4) & 0xf), (lf_fsm  & 0xf));
	V_DBG(VPU_DBG_REG_DUMP, "[DEBUG-BPUHEVC] BS_DATA> ExpEnd=%1d, bs_valid: 0x%03x, bs_data: 0x%03x", ((bs_data >> 31) & 0x1), ((bs_data >> 16) & 0xfff), (bs_data & 0xfff));

	V_DBG(VPU_DBG_REG_DUMP, "[DEBUG-BPUHEVC] BUS_BUSY> mib_wreq_done: %1d, mib_busy: %1d, sdma_bus: %1d",
	 						((bbusy >> 2) & 0x1), ((bbusy >> 1) & 0x1), (bbusy & 0x1));

	V_DBG(VPU_DBG_REG_DUMP, "[DEBUG-BPUHEVC] FIFO_VALID> cu: %1d, tu: %1d, iptu: %1d, lf: %1d, coff: %1d",
	 						 ((fv >> 4) & 0x1), ((fv >> 3) & 0x1), ((fv >> 2) & 0x1), ((fv >> 1) & 0x1), (fv & 0x1));
	V_DBG(VPU_DBG_REG_DUMP, "[-] BPU REG Dump");

	// --------- VCE register Dump
	V_DBG(VPU_DBG_REG_DUMP, "[+] VCE REG Dump");
	tq = hmgr_ReadRegVCE(mgr_ctx->base_addr, 0xd0);
	ip = hmgr_ReadRegVCE(mgr_ctx->base_addr, 0xd4);
	mc = hmgr_ReadRegVCE(mgr_ctx->base_addr, 0xd8);
	lf = hmgr_ReadRegVCE(mgr_ctx->base_addr, 0xdc);
	avail_cu = (hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x11C)>>16)
			- (hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x110)>>16);
	avail_tu = (hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x11C)&0xFFFF)
			- (hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x110)&0xFFFF);
	avail_tc = (hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x120)>>16)
			- (hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x114)>>16);
	avail_lf = (hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x120)&0xFFFF)
			- (hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x114)&0xFFFF);
	avail_ip = (hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x124)>>16)
			- (hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x118)>>16);
	V_DBG(VPU_DBG_REG_DUMP, "       TQ            IP              MC             LF      GDI_EMPTY          ROOM");
	V_DBG(VPU_DBG_REG_DUMP, "------------------------------------------------------------------------------------------------------------");
	V_DBG(VPU_DBG_REG_DUMP, "| %d %04d %04d | %d %04d %04d |  %d %04d %04d | %d %04d %04d | 0x%08x | CU(%d) TU(%d) TC(%d) LF(%d) IP(%d)",
							(tq>>22)&0x07, (tq>>11)&0x3ff, tq&0x3ff,
							(ip>>22)&0x07, (ip>>11)&0x3ff, ip&0x3ff,
							(mc>>22)&0x07, (mc>>11)&0x3ff, mc&0x3ff,
							(lf>>22)&0x07, (lf>>11)&0x3ff, lf&0x3ff,
							hmgr_FIORead(mgr_ctx->base_addr, 0x88f4), /* GDI empty */
							avail_cu, avail_tu, avail_tc, avail_lf, avail_ip);

	/* CU/TU Queue count */
	reg_val = hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x12C);
	V_DBG(VPU_DBG_REG_DUMP, "[DCIDEBUG] QUEUE COUNT: CU(%5d) TU(%5d) ", (reg_val>>16)&0xffff, reg_val&0xffff);

	reg_val = hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x1A0);
	V_DBG(VPU_DBG_REG_DUMP, "TC(%5d) IP(%5d) ", (reg_val>>16)&0xffff, reg_val&0xffff);

	reg_val = hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x1A4);
	V_DBG(VPU_DBG_REG_DUMP, "LF(%5d)", (reg_val>>16)&0xffff);
	V_DBG(VPU_DBG_REG_DUMP, "VALID SIGNAL : CU0(%d)  CU1(%d)  CU2(%d) TU(%d) TC(%d) IP(%5d) LF(%5d)               DCI_FALSE_RUN(%d) VCE_RESET(%d) CORE_INIT(%d) SET_RUN_CTU(%d)",
							(reg_val>>6)&1, (reg_val>>5)&1,
							(reg_val>>4)&1, (reg_val>>3)&1,
							(reg_val>>2)&1, (reg_val>>1)&1,
							(reg_val>>0)&1,
							(reg_val>>10)&1, (reg_val>>9)&1,
							(reg_val>>8)&1, (reg_val>>7)&1);

	V_DBG(VPU_DBG_REG_DUMP, "State TQ: 0x%08x IP: 0x%08x MC: 0x%08x LF: 0x%08x",
							hmgr_ReadRegVCE(mgr_ctx->base_addr, 0xd0), hmgr_ReadRegVCE(mgr_ctx->base_addr, 0xd4),
							hmgr_ReadRegVCE(mgr_ctx->base_addr, 0xd8), hmgr_ReadRegVCE(mgr_ctx->base_addr, 0xdc));
	V_DBG(VPU_DBG_REG_DUMP, "BWB[1]: RESPONSE_CNT(0x%08x) INFO(0x%08x)", hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x194), hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x198));
	V_DBG(VPU_DBG_REG_DUMP, "BWB[2]: RESPONSE_CNT(0x%08x) INFO(0x%08x)", hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x194), hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x198));
	V_DBG(VPU_DBG_REG_DUMP, "DCI INFO");
	V_DBG(VPU_DBG_REG_DUMP, "READ_CNT_0 : 0x%08x", hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x110));
	V_DBG(VPU_DBG_REG_DUMP, "READ_CNT_1 : 0x%08x", hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x114));
	V_DBG(VPU_DBG_REG_DUMP, "READ_CNT_2 : 0x%08x", hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x118));
	V_DBG(VPU_DBG_REG_DUMP, "WRITE_CNT_0: 0x%08x", hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x11c));
	V_DBG(VPU_DBG_REG_DUMP, "WRITE_CNT_1: 0x%08x", hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x120));
	V_DBG(VPU_DBG_REG_DUMP, "WRITE_CNT_2: 0x%08x", hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x124));

	reg_val = hmgr_ReadRegVCE(mgr_ctx->base_addr, 0x128);
	V_DBG(VPU_DBG_REG_DUMP, "LF_DEBUG_PT: 0x%08x", reg_val & 0xffffffff);

	V_DBG(VPU_DBG_REG_DUMP, "cur_main_state %2d, r_lf_pic_deblock_disable %1d, r_lf_pic_sao_disable %1d",
							(reg_val >> 16) & 0x1f,
							(reg_val >> 15) & 0x1,
							(reg_val >> 14) & 0x1);
	V_DBG(VPU_DBG_REG_DUMP, "para_load_done %1d, i_rdma_ack_wait %1d, i_sao_intl_col_done %1d, i_sao_outbuf_full %1d",
							(reg_val >> 13) & 0x1,
							(reg_val >> 12) & 0x1,
							(reg_val >> 11) & 0x1,
							(reg_val >> 10) & 0x1);
	V_DBG(VPU_DBG_REG_DUMP, "lf_sub_done %1d, i_wdma_ack_wait %1d, lf_all_sub_done %1d, cur_ycbcr %1d, sub8x8_done %2d",
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

static int vmgr_hevc_dec_internal_handler(void)
{
	int ret;
	int ret_code = RETCODE_INTR_DETECTION_NOT_ENABLED;
	vpu_mgr_t* mgr_ctx = vpu_hevc_dec_mgr_ctx;
	unsigned long jtimeout;

	long long_max = LONG_MAX;

	if(mgr_ctx != NULL)
	{
		int timeout = mgr_ctx->each_ip->internal_timeout_ms;

		if (atomic_read(&mgr_ctx->oper_intr) > 0)
		{
			detail_hevcd("Success 1: hevc operation!!");
			ret_code = RETCODE_SUCCESS;
		}
		else
		{
			jtimeout = msecs_to_jiffies(timeout);
			if (jtimeout > (unsigned long)long_max)
			{
				jtimeout = (unsigned long)long_max;
			}

			ret = wait_event_interruptible_timeout(mgr_ctx->oper_wq, atomic_read(&mgr_ctx->oper_intr) > 0, (long)jtimeout);
			if (atomic_read(&mgr_ctx->oper_intr) > 0)
			{
				detail_hevcd("Success 2: hevc operation!!");
				ret_code = RETCODE_SUCCESS;
			}
			else
			{
				err_hevcd(
				"[%d]: hevc timed_out(ref %d msec) => oper_intr[%d]!!", ret, timeout, atomic_read(&mgr_ctx->oper_intr));
				vetc_dump_reg_all(mgr_ctx->base_addr, "hmgr_internal_handler timed_out");
				hmgr_dump_status(mgr_ctx);
				ret_code = RETCODE_CODEC_EXIT;
			}
		}

		atomic_set(&mgr_ctx->oper_intr, 0);
	}

	V_DBG(VPU_DBG_INTERRUPT, "out ret=%d", ret_code);

	return ret_code;
}

static void vmgr_hevc_dec_set_compressed_data(vdec_v3_mapconv_info_t* vdec_mapconv, hevc_dec_MapConv_info_t* vpu_mapconv)
{
	vdec_mapconv->compressed_y[VPU_PA] = vpu_mapconv->m_CompressedY[0];
	vdec_mapconv->compressed_y[VPU_KVA] = vpu_mapconv->m_CompressedY[1];

	vdec_mapconv->compressed_cb[VPU_PA] = vpu_mapconv->m_CompressedCb[0];
	vdec_mapconv->compressed_cb[VPU_KVA] = vpu_mapconv->m_CompressedCb[1];

	vdec_mapconv->fbc_y_offset_addr[VPU_PA] = vpu_mapconv->m_FbcYOffsetAddr[0];
	vdec_mapconv->fbc_y_offset_addr[VPU_KVA] = vpu_mapconv->m_FbcYOffsetAddr[1];

	vdec_mapconv->fbc_c_offset_addr[VPU_PA] = vpu_mapconv->m_FbcCOffsetAddr[0];
	vdec_mapconv->fbc_c_offset_addr[VPU_KVA] = vpu_mapconv->m_FbcCOffsetAddr[1];

	vdec_mapconv->compression_table_luma_size = vpu_mapconv->m_uiCompressionTableLumaSize;
	vdec_mapconv->compression_table_chroma_size = vpu_mapconv->m_uiCompressionTableChromaSize;

	vdec_mapconv->luma_stride = vpu_mapconv->m_uiLumaStride;
	vdec_mapconv->chroma_stride = vpu_mapconv->m_uiChromaStride;

	vdec_mapconv->luma_bit_depth = vpu_mapconv->m_uiLumaBitDepth;
	vdec_mapconv->chroma_bit_depth = vpu_mapconv->m_uiChromaBitDepth;

	vdec_mapconv->frame_endian = vpu_mapconv->m_uiFrameEndian;
}

static int vmgr_hevc_dec_set_output(vdec_v3_decode_out_t* arg_decode_out, hevc_dec_output_t* dec_output, vpu_pmap_alloc_info_t* alloc_info)
{
	int ret = 0;

	if((arg_decode_out != NULL) && (dec_output != NULL) && (alloc_info != NULL))
	{
		arg_decode_out->display_out[VPU_PA][VPU_COMP_Y] = (unsigned char*)dec_output->m_pDispOut[VPU_PA][0];
		arg_decode_out->display_out[VPU_PA][VPU_COMP_U] = (unsigned char*)dec_output->m_pDispOut[VPU_PA][1];
		arg_decode_out->display_out[VPU_PA][VPU_COMP_V] = (unsigned char*)dec_output->m_pDispOut[VPU_PA][2];

		arg_decode_out->display_out[VPU_KVA][VPU_COMP_Y] = (unsigned char*)dec_output->m_pDispOut[VPU_KVA][0];
		arg_decode_out->display_out[VPU_KVA][VPU_COMP_U] = (unsigned char*)dec_output->m_pDispOut[VPU_KVA][1];
		arg_decode_out->display_out[VPU_KVA][VPU_COMP_V] = (unsigned char*)dec_output->m_pDispOut[VPU_KVA][2];

		arg_decode_out->decoded_out[VPU_PA][VPU_COMP_Y] = (unsigned char*)dec_output->m_pCurrOut[VPU_PA][0];
		arg_decode_out->decoded_out[VPU_PA][VPU_COMP_U] = (unsigned char*)dec_output->m_pCurrOut[VPU_PA][1];
		arg_decode_out->decoded_out[VPU_PA][VPU_COMP_V] = (unsigned char*)dec_output->m_pCurrOut[VPU_PA][2];

		arg_decode_out->decoded_out[VPU_KVA][VPU_COMP_Y] = (unsigned char*)dec_output->m_pCurrOut[VPU_KVA][0];
		arg_decode_out->decoded_out[VPU_KVA][VPU_COMP_U] = (unsigned char*)dec_output->m_pCurrOut[VPU_KVA][1];
		arg_decode_out->decoded_out[VPU_KVA][VPU_COMP_V] = (unsigned char*)dec_output->m_pCurrOut[VPU_KVA][2];

		arg_decode_out->out_info.pic_type = dec_output->m_DecOutInfo.m_iPicType;
		arg_decode_out->out_info.display_idx = dec_output->m_DecOutInfo.m_iDispOutIdx;
		arg_decode_out->out_info.decoded_idx = dec_output->m_DecOutInfo.m_iDecodedIdx;
		arg_decode_out->out_info.display_status = vmgr_convert_display_status(dec_output->m_DecOutInfo.m_iOutputStatus);
		arg_decode_out->out_info.decoded_status = vmgr_convert_decoding_status(dec_output->m_DecOutInfo.m_iDecodingStatus);

		arg_decode_out->out_info.num_of_err_mbs = dec_output->m_DecOutInfo.m_iNumOfErrMBs;

		arg_decode_out->out_info.decoded_width = dec_output->m_DecOutInfo.m_iDecodedWidth;
		arg_decode_out->out_info.decoded_height = dec_output->m_DecOutInfo.m_iDecodedHeight;
		arg_decode_out->out_info.display_width = dec_output->m_DecOutInfo.m_iDisplayWidth;
		arg_decode_out->out_info.display_height = dec_output->m_DecOutInfo.m_iDisplayHeight;

		arg_decode_out->out_info.decoded_crop.left = dec_output->m_DecOutInfo.m_DecodedCropInfo.m_iCropLeft;
		arg_decode_out->out_info.decoded_crop.right = dec_output->m_DecOutInfo.m_DecodedCropInfo.m_iCropRight;
		arg_decode_out->out_info.decoded_crop.top = dec_output->m_DecOutInfo.m_DecodedCropInfo.m_iCropTop;
		arg_decode_out->out_info.decoded_crop.bottom = dec_output->m_DecOutInfo.m_DecodedCropInfo.m_iCropBottom;

		arg_decode_out->out_info.display_crop.left = dec_output->m_DecOutInfo.m_DisplayCropInfo.m_iCropLeft;
		arg_decode_out->out_info.display_crop.right = dec_output->m_DecOutInfo.m_DisplayCropInfo.m_iCropRight;
		arg_decode_out->out_info.display_crop.top = dec_output->m_DecOutInfo.m_DisplayCropInfo.m_iCropTop;
		arg_decode_out->out_info.display_crop.bottom = dec_output->m_DecOutInfo.m_DisplayCropInfo.m_iCropBottom;

		arg_decode_out->out_info.dma_buf_align_width = ALIGNED_BUFF(arg_decode_out->out_info.decoded_width, 4096U);
		arg_decode_out->out_info.dma_buf_align_height = ALIGNED_BUFF(arg_decode_out->out_info.decoded_height, 4096U);

		arg_decode_out->out_info.userdata_buf_addr[VPU_PA] = dec_output->m_DecOutInfo.m_UserDataAddress[VPU_PA];
		arg_decode_out->out_info.userdata_buf_addr[VPU_KVA] = dec_output->m_DecOutInfo.m_UserDataAddress[VPU_KVA];
		arg_decode_out->out_info.userdata_buffer_size = alloc_info->userdata_buf.size;

		vmgr_hevc_dec_set_compressed_data(&arg_decode_out->out_info.disp_map_conv_info, &dec_output->m_DecOutInfo.m_DispMapConvInfo);
	}
	else
	{
		ret = -1;
	}

	return ret;
}

static int vmgr_hevc_dec_init(vpu_mgr_t* mgr_ctx, vpu_cmd_t* cmd_info, vpu_drv_info_t* drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;

	vpu_accesspoint_t* vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t* alloc_info = &drv_info->pmap_alloc_info;
	vpu_hevc_dec_papam_t* ip_param = (vpu_hevc_dec_papam_t*)drv_info->ip_param;

	codec_handle_t decHandle;
	hevc_dec_init_t* pDecInit = &ip_param->dec_init;

	vdec_v3_init_t* arg_init = (vdec_v3_init_t *)cmd_info->args;
	vdec_v3_init_in_t* arg_init_in = &arg_init->input;

	dlog_hevcd("[id:%u] VPU_DEC_INIT start", drv_id);

	pDecInit->m_RegBaseVirtualAddr = (codec_addr_t)mgr_ctx->base_addr;
	pDecInit->m_Memcpy = (void *(*) (void *, const void*, unsigned int, unsigned int))vetc_memcpy;
	pDecInit->m_Memset = (void (*) (void *, int, unsigned int, unsigned int))vetc_memset;
	pDecInit->m_Interrupt = (int (*) (void))mgr_ctx->each_ip->internal_handler;
	pDecInit->m_Ioremap = (void *(*) (phys_addr_t, unsigned int))vetc_ioremap;
	pDecInit->m_Iounmap = (void (*) (void *))vetc_iounmap;
	pDecInit->m_reg_read = (unsigned int (*)(void *, unsigned int))vetc_reg_read;
	pDecInit->m_reg_write = (void (*)(void *, unsigned int, unsigned int))vetc_reg_write;

	pDecInit->m_iBitstreamFormat = vmgr_get_bitstream_format(arg_init_in->codec_id, VPU_OP_TYPE_DEC); //get from vcodec_id
	pDecInit->m_bEnableUserData = arg_init_in->enable_user_data;

	dlog_hevcd("[id:%u] bitstream format:%d, userdata:%d, output_format:%d", drv_id,
		pDecInit->m_iBitstreamFormat, arg_init_in->enable_user_data, arg_init_in->output_format);

	switch(arg_init_in->output_format)
	{
		case VPU_OUTPUT_LINEAR_YUV420:
		{
			dlog_hevcd("[id:%u] ouput set VPU_OUTPUT_LINEAR_YUV420", drv_id);
			pDecInit->m_bCbCrInterleaveMode = 0U;
			pDecInit->m_uiDecOptFlags |= WAVE4_WTL_ENABLE;
		}
		break;

		case VPU_OUTPUT_LINEAR_NV12:
		{
			dlog_hevcd("[id:%u] ouput set VPU_OUTPUT_LINEAR_NV12", drv_id);
			pDecInit->m_bCbCrInterleaveMode = 1U;
			pDecInit->m_uiDecOptFlags |= WAVE4_WTL_ENABLE;
		}
		break;

		case VPU_OUTPUT_LINEAR_10_TO_8_BIT_YUV420:
		{
			dlog_hevcd("[id:%u] ouput set VPU_OUTPUT_LINEAR_10_TO_8_BIT_YUV420", drv_id);
			pDecInit->m_bCbCrInterleaveMode = 0U;
			pDecInit->m_uiDecOptFlags |= WAVE4_WTL_ENABLE;
			pDecInit->m_uiDecOptFlags |= WAVE4_10BITS_DISABLE;
		}
		break;

		case VPU_OUTPUT_LINEAR_10_TO_8_BIT_NV12:
		{
			dlog_hevcd("[id:%u] ouput set VPU_OUTPUT_LINEAR_10_TO_8_BIT_NV12", drv_id);
			pDecInit->m_bCbCrInterleaveMode = 1U;
			pDecInit->m_uiDecOptFlags |= WAVE4_WTL_ENABLE;
			pDecInit->m_uiDecOptFlags |= WAVE4_10BITS_DISABLE;
		}
		break;

		case VPU_OUTPUT_COMPRESSED_MAPCONV:
		{
			dlog_hevcd("[id:%u] ouput set VPU_OUTPUT_COMPRESSED_MAPCONV", drv_id);
		}
		break;

		case VPU_OUTPUT_COMPRESSED_AFBC:
		{
			dlog_hevcd("[id:%u] ouput set VPU_OUTPUT_COMPRESSED_AFBC", drv_id);
		}
		break;

		default:
			err_hevcd("[id:%u] unknown output format:%d, Set as the default value", drv_id, arg_init_in->output_format);
		break;
	}

	if(arg_init_in->dec_opt_flags > 0)
	{
		if((arg_init_in->dec_opt_flags & VDEC_V3_USE_MAX_FRAMEBUFFER) != 0)
		{
			pDecInit->m_uiDecOptFlags |= (1 << 16);
			pDecInit->m_Reserved[3] = arg_init_in->max_support_width;
			pDecInit->m_Reserved[4] = arg_init_in->max_support_height;
			pDecInit->m_Reserved[5] = 10;
			dlog_hevcd("[id:%u] set VDEC_USE_MAX_FRAMEBUFFER, (%d x %d)", drv_id, arg_init_in->max_support_width, arg_init_in->max_support_height);
		}

		if((arg_init_in->dec_opt_flags & VDEC_V3_NO_BUFFER_DELAY) != 0)
		{
			pDecInit->m_uiDecOptFlags |= (1U << 2U);
			dlog_hevcd("[id:%u] set VDEC_NO_BUFFER_DELAY", drv_id);
		}
	}

	if((arg_init_in->enable_ringbuffer_mode == 1U) && ((mgr_ctx->each_ip->buffer_mode & VPU_BS_MODE_RINGBUFFER) != 0))
	{
		vdec_v3_init_out_t* arg_init_out = &arg_init->output;
		arg_init_out->is_ringbuffer_mode = 1U; //to inform the user that the system is operating in ring buffer mode, it is set to 1U.
		pDecInit->m_iFilePlayEnable = 0;
	}
	else
	{
		pDecInit->m_iFilePlayEnable = 1;
	}

	pDecInit->m_BitWorkAddr[PA] = alloc_info->bitwork_buf.addr[VPU_PA];
	pDecInit->m_BitWorkAddr[VA] = alloc_info->bitwork_buf.addr[VPU_KVA];

	pDecInit->m_BitstreamBufAddr[PA] = alloc_info->bitstream_buf.addr[VPU_PA];
	pDecInit->m_BitstreamBufAddr[VA] = alloc_info->bitstream_buf.addr[VPU_KVA];
	pDecInit->m_iBitstreamBufSize = alloc_info->bitstream_buf.size;

	if(pDecInit->m_bEnableUserData == 1U)
	{
		pDecInit->m_UserDataAddr[VPU_PA] = alloc_info->userdata_buf.addr[VPU_PA];
		pDecInit->m_UserDataAddr[VPU_KVA] = alloc_info->userdata_buf.addr[VPU_KVA];
		pDecInit->m_iUserDataBufferSize = alloc_info->userdata_buf.size;
	}

	dlog_hevcd(
		"Dec :: Init In => workbuff 0x%x/0x%x, Reg: 0x%p/0x%x, format : %d, Stream(0x%x/0x%x, 0x%x)",
		pDecInit->m_BitWorkAddr[PA],
		pDecInit->m_BitWorkAddr[VA],
		mgr_ctx->base_addr,
		pDecInit->m_RegBaseVirtualAddr,
		pDecInit->m_iBitstreamFormat,
		pDecInit->m_BitstreamBufAddr[PA],
		pDecInit->m_BitstreamBufAddr[VA],
		pDecInit->m_iBitstreamBufSize);

	dlog_hevcd(
		"Dec :: Init In => optFlag 0x%x, Userdata(%d), userdata 0x%x/0x%x size:%d, Inter: %d, PlayEn: %d",
		pDecInit->m_uiDecOptFlags,
		pDecInit->m_bEnableUserData,
		pDecInit->m_UserDataAddr[VPU_PA],
		pDecInit->m_UserDataAddr[VPU_KVA],
		pDecInit->m_iUserDataBufferSize,
		pDecInit->m_bCbCrInterleaveMode,
		pDecInit->m_iFilePlayEnable);

	ret = tcc_hevc_dec_l(vpu_ap, VPU_DEC_INIT, (codec_handle_t *)&decHandle, (void *)pDecInit, (void *)NULL);
	if (ret != RETCODE_SUCCESS)
	{
		dlog_hevcd("[id:%u] Init Done with ret(0x%x)", drv_id, ret);
		if (ret != RETCODE_CODEC_EXIT)
		{
			vetc_dump_reg_all(mgr_ctx->base_addr, "init failure");
		}
	}

	if (ret != RETCODE_CODEC_EXIT && decHandle != 0)
	{
		drv_info->handle = decHandle;
	}

	dlog_hevcd("[id:%u] VPU_DEC_INIT, ret:%d", drv_id, ret);
	return ret;
}

static int vmgr_hevc_dec_seqheader(vpu_mgr_t* mgr_ctx, vpu_cmd_t* cmd_info, vpu_drv_info_t* drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t* vpu_ap = mgr_ctx->access_point;
	vpu_hevc_dec_papam_t* ip_param = (vpu_hevc_dec_papam_t*)drv_info->ip_param;

	hevc_dec_input_t* pDecInput = &ip_param->seq_input;
	hevc_dec_initial_info_t* pDecInitialInfo = &ip_param->dec_initialInfo;

	vdec_v3_seqheader_t *arg_seqheader = (vdec_v3_seqheader_t *)cmd_info->args;
	vdec_v3_seqheader_in_t *arg_seqheader_in = &arg_seqheader->input;

	dlog_hevcd("[id:%u] seqheader use vpu_hevc_dec_input_t, input size:%d", drv_id, arg_seqheader_in->bitstream_size);

#if defined(ENABLE_SEQHEADER_BUFFER_CHANGE)
	pDecInput->m_BitstreamDataAddr[VPU_PA] = arg_seqheader_in->bitstream_addr[VPU_PA];
	pDecInput->m_BitstreamDataAddr[VPU_KVA] = arg_seqheader_in->bitstream_addr[VPU_KVA];
	pDecInput->m_iBitstreamDataSize = arg_seqheader_in->bitstream_size;

	ret = tcc_hevc_dec_l(vpu_ap, VPU_DEC_SEQ_HEADER, (codec_handle_t *)&pHandle, (void *)pDecInput,  (void *)pDecInitialInfo);
#else
	{
		union {
			unsigned int ui_data;
			unsigned int* pi_data;
			void* pv_data;
		} udata;

		udata.pi_data = NULL;
		udata.ui_data = arg_seqheader_in->bitstream_size;

		ret = tcc_hevc_dec_l(vpu_ap, VPU_DEC_SEQ_HEADER, (codec_handle_t *)&pHandle, (void *)udata.pv_data, (void *)pDecInitialInfo);
	}
#endif

	if(ret == RETCODE_SUCCESS)
	{
		vdec_v3_initial_info_t* init_info = &arg_seqheader->output.initial_info;

		vetc_memset(init_info, 0x00, sizeof(vdec_v3_initial_info_t), 0);

		init_info->pic_width = pDecInitialInfo->m_iPicWidth;
		init_info->pic_height = pDecInitialInfo->m_iPicHeight;
		init_info->frame_rate_res = pDecInitialInfo->m_uiFrameRateRes;
		init_info->frame_rate_div = pDecInitialInfo->m_uiFrameRateDiv;
		init_info->min_frame_buffer_count = pDecInitialInfo->m_iMinFrameBufferCount;
		init_info->min_frame_buffer_size = pDecInitialInfo->m_iMinFrameBufferSize;
		init_info->frame_buffer_format = pDecInitialInfo->m_iFrameBufferFormat;

		init_info->pic_crop.left = pDecInitialInfo->m_PicCrop.m_iCropLeft;
		init_info->pic_crop.right = pDecInitialInfo->m_PicCrop.m_iCropRight;
		init_info->pic_crop.top = pDecInitialInfo->m_PicCrop.m_iCropTop;
		init_info->pic_crop.bottom = pDecInitialInfo->m_PicCrop.m_iCropBottom;

		init_info->frame_buf_delay = pDecInitialInfo->m_iFrameBufDelay;

		init_info->profile = pDecInitialInfo->m_iProfile;
		init_info->level = pDecInitialInfo->m_iLevel;
		init_info->interlace = 0;
		init_info->aspectratio = pDecInitialInfo->m_iAspectRateInfo;
		init_info->report_error_reason = pDecInitialInfo->m_iReportErrorReason;
		init_info->bitdepth = 0;

		//init_info->eotf ??
		init_info->metadata = pDecInitialInfo->m_uiUserData;
		if(sizeof(vpu_metadata_info_t) == sizeof(hevc_dec_UserData_info_t))
		{
			vpu_metadata_info_t* meta = &init_info->metadata_info;
			vetc_memcpy(meta, &pDecInitialInfo->m_UserDataInfo, sizeof(vpu_metadata_info_t), 0);
			dlog_hevcd("[id:%u] metadata info, colour_primaries:%u, transfer_characteristics:%u, matrix_coefficients:%u",
				drv_id,
				meta->vui_param.colour_primaries,
				meta->vui_param.transfer_characteristics,
				meta->vui_param.matrix_coefficients);
		}
		else
		{
			dlog_hevcd("[id:%u] different size of parameter, target:%lu, src:%lu",  drv_id, sizeof(vpu_metadata_info_t), sizeof(hevc_dec_UserData_info_t));
		}

		vetc_memcpy(&drv_info->initial_info, init_info, sizeof(vdec_v3_initial_info_t), 0);

		dlog_hevcd("[id:%u] VPU_DEC_SEQ_HEADER out ret:%d, min framebuffer cont:%d, size:%d, res info:%d - %d - %d, %d - %d - %d",
			drv_id, ret,
			init_info->min_frame_buffer_count,
			init_info->min_frame_buffer_size,
			init_info->pic_width,
			init_info->pic_crop.left,
			init_info->pic_crop.right,
			init_info->pic_height,
			init_info->pic_crop.top,
			init_info->pic_crop.bottom);
	}

	return ret;
}

static int vmgr_hevc_dec_register_framebuffer(vpu_mgr_t* mgr_ctx, vpu_cmd_t* cmd_info, vpu_drv_info_t* drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t* vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t* alloc_info = &drv_info->pmap_alloc_info;
	vpu_hevc_dec_papam_t* ip_param = (vpu_hevc_dec_papam_t*)drv_info->ip_param;

	hevc_dec_buffer_t* pDecBuffer = &ip_param->dec_buffer;

	pDecBuffer->m_FrameBufferStartAddr[VPU_PA] = alloc_info->frame_buf.addr[VPU_PA];
	pDecBuffer->m_FrameBufferStartAddr[VPU_KVA] = alloc_info->frame_buf.addr[VPU_KVA];
	pDecBuffer->m_iFrameBufferCount = alloc_info->framebuffer_count;

	dlog_hevcd("[id:%u] VPU_DEC_REG_FRAME_BUFFER in :: addr:0x%x/0x%x, frame buffer count:%d",
		drv_id, pDecBuffer->m_FrameBufferStartAddr[VPU_PA], pDecBuffer->m_FrameBufferStartAddr[VPU_KVA], pDecBuffer->m_iFrameBufferCount);

	ret = tcc_hevc_dec_l(vpu_ap, VPU_DEC_REG_FRAME_BUFFER, (codec_handle_t *)&pHandle, (void *)pDecBuffer, (void *)NULL);

	return ret;
}

static int vmgr_hevc_dec_decode(vpu_mgr_t* mgr_ctx, vpu_cmd_t* cmd_info, vpu_drv_info_t* drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t* vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t* alloc_info = &drv_info->pmap_alloc_info;
	vpu_hevc_dec_papam_t* ip_param = (vpu_hevc_dec_papam_t*)drv_info->ip_param;

	hevc_dec_input_t* pDecInput = &ip_param->dec_input;
	hevc_dec_output_t* pDecOutput = &ip_param->dec_output;

	vdec_v3_decode_t* arg_decode = (vdec_v3_decode_t*)cmd_info->args;
	vdec_v3_decode_in_t* arg_decode_in = &arg_decode->input;
	vdec_v3_decode_out_t* arg_decode_out = &arg_decode->output;

	pDecInput->m_BitstreamDataAddr[VPU_PA] = arg_decode_in->bitstream_addr[VPU_PA];
	pDecInput->m_BitstreamDataAddr[VPU_KVA] = arg_decode_in->bitstream_addr[VPU_KVA];
	pDecInput->m_iBitstreamDataSize = arg_decode_in->bitstream_size;

	if(drv_info->dec_init_info.enable_user_data == 1U)
	{
		pDecInput->m_UserDataAddr[VPU_PA] = alloc_info->userdata_buf.addr[VPU_PA];
		pDecInput->m_UserDataAddr[VPU_KVA] = alloc_info->userdata_buf.addr[VPU_KVA];
		pDecInput->m_iUserDataBufferSize = alloc_info->userdata_buf.size;
	}

	//the control of frame skip-related behavior is handled by vpu_dec
	if(arg_decode_in->skip_mode == (int)VPU_FRAMESKIP_DISABLED)
	{
		pDecInput->m_iSkipFrameMode = 0;
	}
	else if(arg_decode_in->skip_mode == (int)VPU_FRAMESKIP_NON_I)
	{
		pDecInput->m_iSkipFrameMode = 1;
		detail_hevcd("[id:%u] set I-frame search", drv_id);
	}
	else
	{
		detail_hevcd("[id:%u] invalid skip mode", drv_id);
		pDecInput->m_iSkipFrameMode = 0;
	}

	detail_hevcd("[id:%u]  Dec In => 0x%x - 0x%x, 0x%x, 0x%x - 0x%x, %d, flag: %d",
		drv_id,
		pDecInput->m_BitstreamDataAddr[PA],
		pDecInput->m_BitstreamDataAddr[VA],
		pDecInput->m_iBitstreamDataSize,
		pDecInput->m_UserDataAddr[PA],
		pDecInput->m_UserDataAddr[VA],
		pDecInput->m_iUserDataBufferSize,
		pDecInput->m_iSkipFrameMode);

	ret = tcc_hevc_dec_l(vpu_ap, VPU_DEC_DECODE, (codec_handle_t *)&pHandle, (void *)pDecInput, (void *)pDecOutput);

	detail_hevcd("[id:%u] Dec Out => %d - %d - %d, %d - %d - %d",
		drv_id,
		pDecOutput->m_DecOutInfo.m_iDisplayWidth,
		pDecOutput->m_DecOutInfo.m_DisplayCropInfo.m_iCropLeft,
		pDecOutput->m_DecOutInfo.m_DisplayCropInfo.m_iCropRight,
		pDecOutput->m_DecOutInfo.m_iDisplayHeight,
		pDecOutput->m_DecOutInfo.m_DisplayCropInfo.m_iCropTop,
		pDecOutput->m_DecOutInfo.m_DisplayCropInfo.m_iCropBottom);

	detail_hevcd("[id:%u] Dec Out => ret[%d] !! PicType[%d], OutIdx[%d/%d], OutStatus[%d/%d], POC[%d/%d]", drv_id, ret,
		pDecOutput->m_DecOutInfo.m_iPicType,
		pDecOutput->m_DecOutInfo.m_iDispOutIdx,
		pDecOutput->m_DecOutInfo.m_iDecodedIdx,
		pDecOutput->m_DecOutInfo.m_iOutputStatus,
		pDecOutput->m_DecOutInfo.m_iDecodingStatus,
		pDecOutput->m_DecOutInfo.m_Reserved[5],
		pDecOutput->m_DecOutInfo.m_Reserved[6]);

	if(ret == RETCODE_SUCCESS)
	{
		(void)vmgr_hevc_dec_set_output(arg_decode_out, pDecOutput, alloc_info);
	}

	detail_hevcd("[id:%u] VPU_DEC_DECODE:%d", drv_id, ret);
	return ret;
}

static int vmgr_hevc_dec_buf_clear(vpu_mgr_t* mgr_ctx, vpu_cmd_t* cmd_info, vpu_drv_info_t* drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;
	vpu_accesspoint_t* vpu_ap = mgr_ctx->access_point;

	vdec_v3_buf_clear_t* arg_bufclear = (vdec_v3_buf_clear_t*)cmd_info->args;

	int *arg = (int *)&arg_bufclear->index;

	detail_hevcd("[id:%u] VPU_CMD_DEC_BUF_FLAG_CLEAR, index:%d", drv_id, *arg);
	ret = tcc_hevc_dec_l(vpu_ap, VPU_DEC_BUF_FLAG_CLEAR, (codec_handle_t *) &pHandle, (void *)(arg), (void *)NULL);
	return ret;
}

static int vmgr_hevc_dec_flush(vpu_mgr_t* mgr_ctx, vpu_cmd_t* cmd_info, vpu_drv_info_t* drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t* vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t* alloc_info = &drv_info->pmap_alloc_info;
	vpu_hevc_dec_papam_t* ip_param = (vpu_hevc_dec_papam_t*)drv_info->ip_param;

	int flush_frame = 0;
	hevc_dec_input_t* pDecInput = &ip_param->dec_input;
	hevc_dec_output_t* pDecOutput = &ip_param->dec_output;

	dlog_hevcd("[id:%u] VPU_CMD_DEC_FLUSH, in", drv_id);

	while(flush_frame < 32)
	{
		pDecInput->m_BitstreamDataAddr[VPU_PA] = alloc_info->bitstream_buf.addr[VPU_PA];
		pDecInput->m_BitstreamDataAddr[VPU_KVA] = alloc_info->bitstream_buf.addr[VPU_KVA];
		pDecInput->m_iBitstreamDataSize = 0;
		pDecInput->m_iSkipFrameMode = 0; //VDEC_SKIP_FRAME_DISABLE

		ret = tcc_hevc_dec_l(vpu_ap, VPU_DEC_FLUSH_OUTPUT, (codec_handle_t *) &pHandle, (void *)pDecInput, (void *)pDecOutput);
		if(ret == RETCODE_SUCCESS)
		{
			if(pDecOutput->m_DecOutInfo.m_iOutputStatus == VPU_DEC_OUTPUT_SUCCESS)
			{
				int *arg = (int *)&pDecOutput->m_DecOutInfo.m_iDispOutIdx;

				dlog_hevcd("[id:%u] VPU_DEC_BUF_FLAG_CLEAR %d", drv_id, pDecOutput->m_DecOutInfo.m_iDispOutIdx);
				ret = tcc_hevc_dec_l(vpu_ap, VPU_DEC_BUF_FLAG_CLEAR, (codec_handle_t *)&pHandle, (void *)(arg), (void *)NULL);
			}
		}
		else if(ret == RETCODE_CODEC_FINISH)
		{
			dlog_hevcd("[id:%u] flush done!, flush_frame:%d, ret:%d", drv_id, flush_frame, ret);
			break;
		}

		flush_frame++;
	}

	return ret;
}

static int vmgr_hevc_dec_drain(vpu_mgr_t* mgr_ctx, vpu_cmd_t* cmd_info, vpu_drv_info_t* drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t* vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t* alloc_info = &drv_info->pmap_alloc_info;
	vpu_hevc_dec_papam_t* ip_param = (vpu_hevc_dec_papam_t*)drv_info->ip_param;

	int flush_frame = 0;
	hevc_dec_input_t* pDecInput = &ip_param->dec_input;
	hevc_dec_output_t* pDecOutput = &ip_param->dec_output;

	vdec_v3_drain_t* arg_drain = (vdec_v3_drain_t*)cmd_info->args;
	vdec_v3_decode_out_t* arg_decode_out = &arg_drain->output;

	dlog_hevcd("[id:%u] VPU_CMD_DEC_DRAIN, in", drv_id);

	pDecInput->m_BitstreamDataAddr[VPU_PA] = alloc_info->bitstream_buf.addr[VPU_PA];
	pDecInput->m_BitstreamDataAddr[VPU_KVA] = alloc_info->bitstream_buf.addr[VPU_KVA];
	pDecInput->m_iBitstreamDataSize = 0;
	pDecInput->m_iSkipFrameMode = 0; //VDEC_SKIP_FRAME_DISABLE

	ret = tcc_hevc_dec_l(vpu_ap, VPU_DEC_FLUSH_OUTPUT, (codec_handle_t *) &pHandle, (void *)pDecInput, (void *)pDecOutput);
	if(ret == RETCODE_SUCCESS)
	{
		(void)vmgr_hevc_dec_set_output(arg_decode_out, pDecOutput, alloc_info);
	}
	else if(ret == RETCODE_CODEC_FINISH)
	{
		dlog_hevcd("[id:%u] drain done!, flush_frame:%d, ret:%d", drv_id, flush_frame, ret);
	}

	return ret;
}

static int vmgr_hevc_dec_close(vpu_mgr_t* mgr_ctx, vpu_cmd_t* cmd_info, vpu_drv_info_t* drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t* vpu_ap = mgr_ctx->access_point;

	dlog_hevcd("[id:%u] VPU_4K_D2_DEC_CLOSED", drv_id);
	ret = tcc_hevc_dec_l(vpu_ap, VPU_DEC_CLOSE, (codec_handle_t *)&pHandle, (void *)NULL, (void *)NULL);

	return ret;
}

static int vmgr_hevc_dec_ringbuffer_getinfo(vpu_mgr_t* mgr_ctx, vpu_cmd_t* cmd_info, vpu_drv_info_t* drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t* vpu_ap = mgr_ctx->access_point;
	vpu_hevc_dec_papam_t* ip_param = (vpu_hevc_dec_papam_t*)drv_info->ip_param;

	hevc_dec_ring_buffer_status_out_t* pDecRingbufferStatus = &ip_param->dec_ringbuffer_status;

	vdec_v3_ringbuff_get_info_t* arg_ringbuff_get = (vdec_v3_ringbuff_get_info_t*)cmd_info->args;

	detail_hevcd("[id:%u] VPU_CMD_DEC_RING_GET_INFO, in", drv_id);

	ret = tcc_hevc_dec_l(vpu_ap, VPU_GET_RING_BUFFER_STATUS, (codec_handle_t *) &pHandle, (void *)NULL, (void *)pDecRingbufferStatus);
	if(ret == RETCODE_SUCCESS)
	{
		arg_ringbuff_get->available_space = pDecRingbufferStatus->m_ulAvailableSpaceInRingBuffer;
		arg_ringbuff_get->read_physical_addr = pDecRingbufferStatus->m_ptrReadAddr_PA;
		arg_ringbuff_get->write_physical_addr = pDecRingbufferStatus->m_ptrWriteAddr_PA;
		detail_hevcd("[id:%u] VPU_CMD_DEC_RING_GET_INFO, succeed, space:%d, read_pa:0x%x, write_pa:0x%x",
			drv_id, arg_ringbuff_get->available_space, arg_ringbuff_get->read_physical_addr, arg_ringbuff_get->write_physical_addr );
	}

	return ret;
}

static int vmgr_hevc_dec_ringbuffer_setinfo(vpu_mgr_t* mgr_ctx, vpu_cmd_t* cmd_info, vpu_drv_info_t* drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t* vpu_ap = mgr_ctx->access_point;

	union {
		int i_data;
		int *pi_data;	//NULL
		void *pv_data;
	} ucopysize, flushbuf;

	vdec_v3_ringbuff_set_info_t* arg_ringbuff_set = (vdec_v3_ringbuff_set_info_t*)cmd_info->args;

	detail_hevcd("[id:%u] VPU_CMD_DEC_RING_SET_INFO, in, written:%d, flush:%d", drv_id, arg_ringbuff_set->written_byte, arg_ringbuff_set->is_flush);

	ucopysize.pi_data = NULL;
	ucopysize.i_data = arg_ringbuff_set->written_byte;
	flushbuf.pi_data = NULL;
	flushbuf.i_data = arg_ringbuff_set->is_flush;

	ret = tcc_hevc_dec_l(vpu_ap, VPU_UPDATE_WRITE_BUFFER_PTR, (codec_handle_t *) &pHandle, ucopysize.pv_data, flushbuf.pv_data);
	return ret;
}

static int vmgr_hevc_dec_decode_process(void* vpu_private, enum vpu_cmd_type cmd, vpu_cmd_t* cmd_info, vpu_drv_info_t* drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	vpu_mgr_t* mgr_ctx = (vpu_mgr_t*)vpu_private;

	detail_hevcd("[id:%u] %s(%d)/start mgr_ctx:%p, drv_id:%d", drv_id, vmgr_cmd_name(cmd), cmd, mgr_ctx, drv_id);

	switch (cmd)
	{
		case VPU_CMD_DEC_INIT:
		{
			ret = vmgr_hevc_dec_init(mgr_ctx, cmd_info, drv_info);
		}
		break;

		case VPU_CMD_DEC_SEQ_HEADER:
		{
			ret = vmgr_hevc_dec_seqheader(mgr_ctx, cmd_info, drv_info);
		}
		break;

		case VPU_CMD_DEC_REG_FRAME_BUFFER:
		{
			ret = vmgr_hevc_dec_register_framebuffer(mgr_ctx, cmd_info, drv_info);
		}
		break;

		case VPU_CMD_DEC_DECODE:
		{
			ret = vmgr_hevc_dec_decode(mgr_ctx, cmd_info, drv_info);
		}
		break;

		case VPU_CMD_DEC_BUF_FLAG_CLEAR:
		{
			ret = vmgr_hevc_dec_buf_clear(mgr_ctx, cmd_info, drv_info);
		}
		break;

		case VPU_CMD_DEC_FLUSH:
		{
			ret = vmgr_hevc_dec_flush(mgr_ctx, cmd_info, drv_info);
		}
		break;

		case VPU_CMD_DEC_DRAIN:
		{
			ret = vmgr_hevc_dec_drain(mgr_ctx, cmd_info, drv_info);
		}
		break;

		case VPU_CMD_DEC_CLOSE:
		{
			ret = vmgr_hevc_dec_close(mgr_ctx, cmd_info, drv_info);
		}
		break;

		case VPU_CMD_DEC_RING_GET_INFO:
		{
			ret = vmgr_hevc_dec_ringbuffer_getinfo(mgr_ctx, cmd_info, drv_info);
		}
		break;

		case VPU_CMD_DEC_RING_SET_INFO:
		{
			ret = vmgr_hevc_dec_ringbuffer_setinfo(mgr_ctx, cmd_info, drv_info);
		}
		break;

		default:
		{
			err_hevcd("[id:%u] not supported command(0x%x)", drv_id, cmd);
			ret = 0x999;
		}
	}

	//V_DBG(VPU_DBG_INFO, "%s(%d)/finish mgr_ctx:%p, drv_id:%d", vmgr_dec_cmd_name(cmd), cmd, mgr_ctx, drv_id);
	ret = vmgr_convert_retcode(ret);
	return ret;
}

static int vmgr_hevc_dec_get_buffer_size(void* vpu_private, enum vmgr_buffer_type buf_type, vpu_drv_info_t* drv_info)
{
	int size = 0;

	//for unused buffers, they must be set to 0.
	switch(buf_type)
	{
		case VMGR_BUF_BITSTREAM:
			size = ALIGNED_BUFF(WAVE4_STREAM_BUF_SIZE, 4096u); //20Mb
		break;

		case VMGR_BUF_NUM_OF_BITSTREAM:
			size = VPU_HEVC_DEC_NUM_OF_BITSTREAM_BUFFERS;
		break;

		case VMGR_BUF_BITWORK:
			size = ALIGNED_BUFF(WAVE4_WORK_CODE_BUF_SIZE, 4096u);
		break;

		case VMGR_BUF_FRAMEBUF:
			size = 1; //use framebuffer, calculating from vpu_mgr.c using min framebuffer count, size
		break;

		case VMGR_BUF_SPSPPS:
			size = 0;
		break;

		case VMGR_BUF_USERDATA:
			size = ALIGNED_BUFF(WAVE4_USERDATA_BUF_SIZE, 4096u);
		break;

		case VMGR_BUF_SLICE:
			size = 0;
		break;

		case VMGR_BUF_MBDATA:
			size = 0;
		break;

		case VMGR_BUF_MESEARCH:
			size = 0;
		break;

		case VMGR_BUF_SLICEINFO:
			size = 0;
		break;

		case VMGR_BUF_Y:
		break;

		case VMGR_BUF_CB:
		break;

		case VMGR_BUF_CR:
		break;

		case VMGR_BUF_MVCOL:
		break;

		case VMGR_BUF_FBCY:
		break;

		case VMGR_BUF_FBCC:
		break;

		default:
			size = 0;
		break;
	}

	return size;
}

static irqreturn_t  vmgr_hevc_dec_isr_handler(int irq, void *vpu_private)
{
	vpu_mgr_t* mgr_ctx = (vpu_mgr_t*)vpu_private;

	atomic_inc(&mgr_ctx->oper_intr);
	wake_up_interruptible(&mgr_ctx->oper_wq);
	return IRQ_HANDLED;
}

extern vmgr_clock_t vpu_hevc_dec_clock;

static vpu_ip_module_t vpu_hevc_dec_module = {
	.ip_type = VPU_IP_HEVC_DEC,
	.cq_type = VPU_CQ_LEGACY,
	.cq_depth = 1,
	.buffer_mode = VPU_BS_MODE_LINEARBUFFR,  // | VPU_BS_MODE_RINGBUFFER
	.internal_timeout_ms = 200,
	.enc_param_size = 0,
	.dec_param_size = sizeof(vpu_hevc_dec_papam_t),
	.internal_handler = vmgr_hevc_dec_internal_handler,
	//codec name(string), profile(string), level(string), width(unsigned int), height(unsigned int), fps(unsigned int)
	.dec_capa = {{VCODEC_ID_HEVC, CODEC_NAME_HEVC, "main/main10", "5.0 high tier", 3840U, 2160U, 30U},
				{VCODEC_ID_NONE, NULL, NULL, NULL, 0U, 0U, 0U}},
	.enc_capa = {{VCODEC_ID_NONE, NULL, NULL, NULL, 0U, 0U, 0U}},
	.clock_ctrl = &vpu_hevc_dec_clock,
	.proc_encode = NULL,
	.proc_decode = vmgr_hevc_dec_decode_process,
	.proc_get_buffer_size = vmgr_hevc_dec_get_buffer_size,
	.isr_handler = vmgr_hevc_dec_isr_handler,
	.cq_func = NULL,
	.ip_private = NULL,
	.access_point_path = HEVC_ACCESSPOINT_PATH,
};

int vmgr_hevc_dec_probe(struct platform_device *pdev)
{
	int ret = 0;
	vpu_mgr_t* mgr_ctx = NULL;

	mgr_ctx = vmgr_alloc(&vpu_hevc_dec_module);
	if(mgr_ctx != NULL)
	{
#if !defined(USE_ACCESS_POINT)
		mgr_ctx->access_point->tccfp_vpu_dec = tcc_hevc_dec;
		mgr_ctx->access_point->tccfp_vpu_enc = NULL;
#else
		mgr_ctx->access_point = NULL;
#endif

		ret = vmgr_probe(mgr_ctx, pdev, HMGR_NAME);
		if(ret == 0)
		{
			//assigning VPU manager context to avoid mutex race condition in interrupt handler
			vpu_hevc_dec_mgr_ctx = (vpu_mgr_t*)vmgr_get_context(VPU_IP_HEVC_DEC);

			dlog_hevcd("vetc_reg_init for %s", vmgr_get_ip_name(VPU_IP_HEVC_DEC));
			vetc_reg_init(mgr_ctx->base_addr);
		}
	}

	platform_set_drvdata(pdev, mgr_ctx);
	return ret;
}

EXPORT_SYMBOL(vmgr_hevc_dec_probe);

int vmgr_hevc_dec_remove(struct platform_device *pdev)
{
	vpu_mgr_t* mgr_ctx = (vpu_mgr_t *)platform_get_drvdata(pdev);

	if(mgr_ctx->each_ip->ip_private != NULL)
	{
		VPU_free(mgr_ctx->each_ip->ip_private);
		mgr_ctx->each_ip->ip_private = NULL;
	}

	vmgr_remove(mgr_ctx, pdev);
	vmgr_free(mgr_ctx);
	vpu_hevc_dec_mgr_ctx = NULL;

	return 0;
}

EXPORT_SYMBOL(vmgr_hevc_dec_remove);

#if defined(CONFIG_PM)
int vmgr_hevc_dec_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	vpu_mgr_t* mgr_ctx = (vpu_mgr_t *)platform_get_drvdata(pdev);

	ret = vmgr_suspend(mgr_ctx, pdev, state);
	return ret;
}

EXPORT_SYMBOL(vmgr_hevc_dec_suspend);

int vmgr_hevc_dec_resume(struct platform_device *pdev)
{
	vpu_mgr_t* mgr_ctx = (vpu_mgr_t *)platform_get_drvdata(pdev);

	vmgr_resume(mgr_ctx, pdev);
	return 0;
}

EXPORT_SYMBOL(vmgr_hevc_dec_resume);
#endif

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
MODULE_SOFTDEP("pre: vpu_lib jpu_lib hevc_lib vpu_4k_d2_lib vpu_hevc_enc_lib vpu");

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC hevc dec manager");
MODULE_LICENSE("GPL");

#endif //ENABLE_VPU_DRV_HEVCDEC
