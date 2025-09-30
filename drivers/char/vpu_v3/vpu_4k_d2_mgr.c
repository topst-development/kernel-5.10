/*
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc.
* Contact: jayhouse@telechips.com
*/

#include "vpu_comm.h"

#ifdef ENABLE_VPU_DRV_4K_D2

#include "vpu_mgr.h"
#include "vpu_mgr_common.h"
#include "vpu_mgr_context.h"
#include "vpu_rm.h"
#include "vpu_devices.h"
#include "vpu_memtrace.h"
#include "vpu_4k_d2_mgr_sys.h"
#include "vpu_4k_d2_mgr.h"

#include "vpu_dbg_info.h"

#define dlog_4kd2(msg...)  		V_DBG(VPU_DBG_INFO, "[4K_D2][INFO]: " msg)
#define detail_4kd2(msg...)  	V_DBG(VPU_DBG_DETAIL, "[4K_D2][DETAIL]: " msg)
#define seq_4kd2(msg...)     	V_DBG(VPU_DBG_SEQUENCE, "[4K_D2][SEQ]: " msg)
#define err_4kd2(msg...)      	V_DBG(VPU_DBG_ERROR, "[4K_D2][ERR]: " msg)

// Enable the featue for future use
//#define VPU_4K_D2_REGISTER_DUMP
//#define VPU_4K_D2_DUMP_STATUS

#define DEBUG_VPU_4K_D2_K //To debug vpu drv in usersapce side (e.g. omx)

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

#define VPU_4K_D2_MAX_SUPER_FRAME	(16)

#define VPU_4K_D2_ACCESSPOINT_PATH		"/proc/4kd2"

#if !defined(USE_ACCESS_POINT)
#if DEFINED_CONFIG_VDEC
extern int tcc_vpu_4k_d2_dec(int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2);
#endif
#endif //!defined(USE_ACCESS_POINT)

#define CQ2_DEPTH_MIN				(2U)
#define NUM_OF_INSTANCE_CQ2			(2U) //WAVE5_COMMAND_QUEUE_DEPTH / CQ2_DEPTH_MINs

static const int VPU_4KD2_NUM_OF_BITSTREAM_BUFFERS = NUM_OF_INSTANCE_CQ2; //This value must match the depth value of CQ for CQ to operate properly.

//to avoid potential issues caused by stack frames, parameters are stored in the heap instead of using local variables.
//This value is assigned to ip_param of vpu_drv_info_t.
typedef struct vpu_4kd2_papam_t {
	vpu_4K_D2_dec_ctrl_log_status_t dec_log;
#if defined(ENABLE_VPU_FW_LOADING)
	vpu_4K_D2_dec_set_fw_addr_t fw_info;
#endif
	vpu_4K_D2_dec_init_t dec_init;
	vpu_4K_D2_dec_set_options_t dec_opt;
	vpu_4K_D2_dec_initial_info_t dec_initialInfo;
	vpu_4K_D2_dec_input_t seq_input;
	vpu_4K_D2_dec_buffer_t dec_buffer;
	vpu_4K_D2_dec_buffer3_t dec_buffer3;
	vpu_4K_D2_dec_input_t dec_input;
	vpu_4K_D2_dec_output_t dec_output;
} vpu_4kd2_papam_t;


static vpu_mgr_t *vpu_4kd2_mgr_ctx = INITIAL_NULL;

static int tcc_vpu_4k_d2_dec_l(vpu_accesspoint_t *vpu_ap, int Op, codec_handle_t *pHandle, void *pParam1, void *pParam2) VPU_NO_SANITIZE_CFI
{
	return vpu_ap->tccfp_vpu_dec(Op, pHandle, pParam1, pParam2);
}

#if defined(ENABLE_CQ2)

#define W5_REG_BASE                     (0x00000000)
#define W5_CMD_REG_BASE                 (0x00000100)
#define W5_CMD_REG_END                  (0x00000200)

#define W5_VPU_INT_REASON			(0x004C)
#define W5_VPU_INT_REASON_CLEAR		(0x0034)
#define W5_VPU_VINT_CLEAR			(0x003C)
#define W5_RET_BS_EMPTY_INST		(0x01E4)
#define W5_RET_QUEUE_CMD_DONE_INST	(0x01E8)
#define W5_RET_DONE_INSTANCE_INFO	(0x01FC)

typedef enum {
    INT_WAVE5_INIT_VPU          = 0,
    INT_WAVE5_WAKEUP_VPU        = 1,
    INT_WAVE5_SLEEP_VPU         = 2,
    INT_WAVE5_CREATE_INSTANCE   = 3,
    INT_WAVE5_FLUSH_INSTANCE    = 4,
    INT_WAVE5_DESTORY_INSTANCE  = 5,
    INT_WAVE5_INIT_SEQ          = 6,
    INT_WAVE5_SET_FRAMEBUF      = 7,
    INT_WAVE5_DEC_PIC           = 8,
    INT_WAVE5_ENC_PIC           = 8,
    INT_WAVE5_ENC_SET_PARAM     = 9,
#ifdef SUPPORT_SOURCE_RELEASE_INTERRUPT
    INT_WAVE5_ENC_SRC_RELEASE   = 10,
#endif
    INT_WAVE5_ENC_LOW_LATENCY   = 13,
    INT_WAVE5_DEC_QUERY         = 14,
    INT_WAVE5_BSBUF_EMPTY       = 15,
    INT_WAVE5_BSBUF_FULL        = 15,
} Wave5InterruptBit;

static int vmgr_4k_d2_internal_handler(unsigned int id, unsigned int mask, unsigned int *flag)
{
	vpu_mgr_t *mgr_ctx = vpu_4kd2_mgr_ctx;
	int ret_code = RETCODE_SUCCESS; //@@@RETCODE_INTR_DETECTION_NOT_ENABLED;

	*flag = atomic_read(&mgr_ctx->handler_intr);

	//@@@@@@@@@@@@@@@@@@@@@@
	//*flag = (*flag | 0x100);
	//@@@@@@@@@@@@@@@@@@@@@@

	atomic_set(&mgr_ctx->handler_intr, 0);

	//printk("@@@ interrupt handler-2, id=%d, mask=0x%02X, flag=0x%04X\n", id, mask, *flag);

	return ret_code;
}

static unsigned int get_4k_d2_inst_idx(unsigned int reason, unsigned int empty_inst, unsigned int done_inst, unsigned int other_inst)
{
	unsigned int inst_val, inst_idx;

	if (reason & (1 << INT_WAVE5_BSBUF_EMPTY)) {
		inst_val = empty_inst & 0xffff;
	} else if (reason & (1 << INT_WAVE5_INIT_SEQ)) {
		inst_val = done_inst & 0xffff;
	} else if (reason & (1 << INT_WAVE5_DEC_PIC)) {
		inst_val = done_inst & 0xffff;
	} else {
		inst_val = other_inst & 0xffff;
	}

	for (inst_idx = 0; inst_idx < MAX_NUM_INSTANCE; inst_idx++) {
		if (((inst_val >> inst_idx) & 0x01) == 0x01) {
			break;
		}
	}

	//TODO : Check if lower than provisioned instances
	inst_idx = (inst_idx < MAX_NUM_INSTANCE) ? inst_idx : 0; //MAX_NUM_INSTANCE
	return inst_idx;
}

//invoked from vpu_mgr by callback
static unsigned int vmgr_4k_d2_get_id_with_reason(void *priv, unsigned int *reason)
{
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)priv;

	unsigned int intr_reason;
	unsigned int empty_inst;
	unsigned int done_inst;
	unsigned int other_inst;
	unsigned int intr_inst_index;

	intr_reason = 0x00;

	intr_reason = vetc_reg_read(mgr_ctx->base_addr, W5_VPU_INT_REASON);
	empty_inst = vetc_reg_read(mgr_ctx->base_addr, W5_RET_BS_EMPTY_INST);
	done_inst = vetc_reg_read(mgr_ctx->base_addr, W5_RET_QUEUE_CMD_DONE_INST);
	other_inst = vetc_reg_read(mgr_ctx->base_addr, W5_RET_DONE_INSTANCE_INFO);

	vetc_reg_write(mgr_ctx->base_addr, W5_VPU_INT_REASON_CLEAR, intr_reason);
	vetc_reg_write(mgr_ctx->base_addr, W5_VPU_VINT_CLEAR, 0x1);

	intr_inst_index = get_4k_d2_inst_idx(intr_reason, empty_inst, done_inst, other_inst);

	atomic_set(&mgr_ctx->handler_intr, intr_reason);

	if (reason) {
		*reason = intr_reason;
		//V_DBG(VPU_DBG_INFO, "reason:0x%x", *reason);
	}

	return intr_inst_index;
}

//invoked from vpu_mgr by callback
static unsigned int vmgr_4k_d2_interrupt_status(void *priv)
{
	unsigned int intr_status;
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)priv;

	intr_status = vetc_reg_read(mgr_ctx->base_addr, W5_REG_BASE + 0x0044); //W5_VPU_VPU_INT_STS

	return intr_status;
}

static void wave5_inform(int id, void *v)
{
	vpu_mgr_t *mgr_ctx = vpu_4kd2_mgr_ctx;

	//printk("@@@ INFORM : id=%d, val=%d\n", id, ((v != NULL) ? *(int*)v : 0));

	if (id == 1/*totalQueueCount*/) {
		int cq_remaining = WAVE5_COMMAND_QUEUE_DEPTH - *(int *)v;
		atomic_set(&mgr_ctx->cq_remaining, cq_remaining);

		mgr_ctx->comm_data.thread_intr++;
		wake_up_interruptible(&mgr_ctx->comm_data.thread_wq);
	} else if (id == 2) {
		//intentional_fbfull = 1;
		//DBG("@@@ INFORM=%d, v=%d", id, *(int*)v);
	} else if (id == 3/*seqHeader - totalQueueCount*/) {
		int cq_remaining = WAVE5_COMMAND_QUEUE_DEPTH - *(int *)v;
		atomic_set(&mgr_ctx->cq_remaining, cq_remaining);
	} else if (id == 4/*decode - totalQueueCount*/) {
		int cq_remaining = WAVE5_COMMAND_QUEUE_DEPTH - *(int *)v;
		atomic_set(&mgr_ctx->cq_remaining, cq_remaining);
	}
}

#else //#if defined(ENABLE_CQ2)

static unsigned int oper_inst_reason_met(vpu_mgr_t *mgr_ctx, unsigned int reason, unsigned int vint_reason)
{
	unsigned int oper_inst = 0;
	oper_inst = atomic_read(&mgr_ctx->oper_intr);
	oper_inst |= vint_reason;

	return (oper_inst & reason);
}

static int vmgr_4k_d2_internal_handler(unsigned int reason)
{
	int ret;
	int ret_code = RETCODE_INTR_DETECTION_NOT_ENABLED;
	unsigned int vint_reason = 0;
	int oper_inst = 0;
	int cnt = 0;
	unsigned long jtimeout;
	vpu_mgr_t *mgr_ctx = vpu_4kd2_mgr_ctx;

	jtimeout = msecs_to_jiffies(5);
	if (jtimeout > (ULONG_MAX / 2UL)) {
		jtimeout = ((ULONG_MAX / 2UL) - 1UL);
	}

	if (mgr_ctx != NULL) {
		int timeout = mgr_ctx->each_ip->internal_timeout_ms;

		oper_inst = atomic_read(&mgr_ctx->oper_intr);
		if ((oper_inst & reason) > 0U) {
			detail_4kd2("Success 1: vpu-4k-d2 vp9/hevc operation!!");
			ret_code = RETCODE_SUCCESS;
		} else {
			for (cnt = 0; cnt < timeout; cnt += 5) {
				ret = wait_event_interruptible_timeout(mgr_ctx->oper_wq, oper_inst_reason_met(mgr_ctx, reason, vint_reason), (long)jtimeout);
				if (ret == 0 /*TIMEOUT*/) {
					vint_reason = vetc_reg_read(mgr_ctx->base_addr, 0x004C/*W5_VPU_VINT_REASON */);
				} else if (ret >= 0) {
					oper_inst = atomic_read(&mgr_ctx->oper_intr);
				} else {
					//-ERESTARTSYS(Interrupted by a signal)
					err_4kd2("ERESTARTSYS");
					break;
				}

				if ((oper_inst & reason) > 0) {
					detail_4kd2("Success 2: vpu-4k-d2 vp9/hevc operation!!");
					ret_code = RETCODE_SUCCESS;
					break;
				}
			}

			if (cnt >= timeout) {
				err_4kd2("vpu-4k-d2 vp9/hevc timed_out(ref %d msec) => oper_intr[%d]!!",
					timeout, atomic_read(&mgr_ctx->oper_intr));

				ret_code = RETCODE_CODEC_EXIT;
			}
		}

		if (ret_code == RETCODE_SUCCESS) {
			atomic_andnot(reason, &mgr_ctx->oper_intr);
		}
	}

	V_DBG(VPU_DBG_INTERRUPT, "out Interrupt isr ev=%d", ret_code);

	return ret_code;
}

#endif //#if defined(ENABLE_CQ2)

#ifdef VPU_4K_D2_DUMP_STATUS
static unsigned int vpu_4k_d2mgr_FIORead(vpu_mgr_t *mgr_ctx, unsigned int addr)
{
	unsigned int ctrl;
	unsigned int count = 0;
	unsigned int data = 0xffffffff;

	ctrl = (addr & 0xffff);
	ctrl |= (0 << 16);	/* read operation */
	vetc_reg_write(mgr_ctx->base_addr, W5_VPU_FIO_CTRL_ADDR, ctrl);
	count = 10000;
	while (count--) {
		ctrl = vetc_reg_read(mgr_ctx->base_addr, W5_VPU_FIO_CTRL_ADDR);
		if (ctrl & 0x80000000) {
			data = vetc_reg_read(mgr_ctx->base_addr, W5_VPU_FIO_DATA);
			break;
		}
	}

	return data;
}

static int vpu_4k_d2mgr_FIOWrite(vpu_mgr_t *mgr_ctx, unsigned int addr, unsigned int data)
{
	unsigned int ctrl;

	vetc_reg_write(mgr_ctx->base_addr, W5_VPU_FIO_DATA, data);
	ctrl = (addr & 0xffff);
	ctrl |= (1 << 16);	/* write operation */
	vetc_reg_write(mgr_ctx->base_addr, W5_VPU_FIO_CTRL_ADDR, ctrl);

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

static void vpu_4k_d2_dump_status(vpu_mgr_t *mgr_ctx)
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

	rd = vetc_reg_read(mgr_ctx->base_addr, W5_BS_RD_PTR);
	wr = vetc_reg_read(mgr_ctx->base_addr, W5_BS_WR_PTR);
	V_DBG(VPU_DBG_REG_DUMP,
	"RD_PTR: 0x%08x WR_PTR: 0x%08x BS_OPT: 0x%08x BS_PARAM: 0x%08x",
		rd, wr, vetc_reg_read(mgr_ctx->base_addr, W5_BS_OPTION),
		vetc_reg_read(mgr_ctx->base_addr, W5_CMD_BS_PARAM));

	// --------- VCPU register Dump
	V_DBG(VPU_DBG_REG_DUMP, "[+] VCPU REG Dump");
	for (index = 0; index < 25; index++) {
		vetc_reg_write(mgr_ctx->base_addr, 0x14,  (1 << 9) | (index & 0xff));
		vcpu_reg[index] = vetc_reg_read(mgr_ctx->base_addr, 0x1c);

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
		 = vetc_reg_read(mgr_ctx->base_addr, 0x01E8);
		stage0_inst_info
		 = vetc_reg_read(mgr_ctx->base_addr, 0x01EC);
		stage1_inst_info
		 = vetc_reg_read(mgr_ctx->base_addr, 0x01F0);
		stage2_inst_info
		 = vetc_reg_read(mgr_ctx->base_addr, 0x01F4);
		dec_seek_cycle
		 = vetc_reg_read(mgr_ctx->base_addr, 0x01C0);
		dec_parsing_cycle
		 = vetc_reg_read(mgr_ctx->base_addr, 0x01C4);
		dec_decoding_cycle
		 = vetc_reg_read(mgr_ctx->base_addr, 0x01C8);
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
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5000);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_LOAD_CMD    = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5004);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_AUTO_MOD  = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5008);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_START_ADDR  = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x500C);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_END_ADDR   = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5010);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_ENDIAN     = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5014);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_IRQ_CLEAR  = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5018);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_BUSY       = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x501C);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_LAST_ADDR  = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5020);
		V_DBG(VPU_DBG_REG_DUMP, "SDMA_SC_BASE_ADDR  = 0x%x", reg_val);

		// -------------------------------------------
		// SHU registers
		// -------------------------------------------

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5400);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_INIT = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5404);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SEEK_NXT_NAL = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5408);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_RD_NAL_ADDR = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x540c);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_STATUS = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5410);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_1 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5414);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_2 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5418);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_3 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x541c);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_4 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5420);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_5 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5424);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_6 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5428);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_7 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x542c);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_GBYTE_8 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5430);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SBYTE_LOW = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5434);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SBYTE_HIGH = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5438);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_ST_PAT_DIS = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5440);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF0_0 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5444);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF0_1 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5448);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF0_2 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x544c);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF0_3 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5450);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF1_0 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5454);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF1_1 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5458);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF1_2 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x545c);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF1_3 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5460);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF2_0 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5464);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF2_1 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5468);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF2_", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x546c);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_SHU_NBUF2_3 = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5470);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_NBUF_RPTR = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5474);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_NBUF_WPTR = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5478);
		V_DBG(VPU_DBG_REG_DUMP, "SHU_REMAIN_BYTE = 0x%x", reg_val);

		// -----------------------------------------
		// GBU registers
		// -----------------------------------------

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5800);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_INIT = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5804);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_STATUS = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5808);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_TCNT = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x580c);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_TCNT = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5c10);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF0_LOW = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5c14);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF0_HIGH = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5c18);

		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF1_LOW = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5c1c);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF1_HIGH = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5c20);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF2_LOW = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5c24);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF2_HIGH = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5c30);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF_RPTR = 0x%x", reg_val);
		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5c34);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_WBUF_WPTR = 0x%x", reg_val);

		reg_val =
			vetc_reg_read(mgr_ctx->base_addr,
				  W5_REG_BASE + 0x5c28);
		V_DBG(VPU_DBG_REG_DUMP, "GBU_REMAIN_BIT = 0x%x", reg_val);
	}
}
#endif

static void vmgr_4kd2_set_compressed_data(vdec_v3_mapconv_info_t *vdec_mapconv, vpu_4K_D2_dec_MapConv_info_t *vpu_mapconv)
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

	//4kd2 use m_Reserved 0, 3, .. 0 for bitstream format, 3 for bitdepth
	vetc_memcpy(vdec_mapconv->reserved, vpu_mapconv->m_Reserved, sizeof(vpu_mapconv->m_Reserved), 0);
}

static int vmgr_4kd2_set_output(vpu_drv_info_t *drv_info, vdec_v3_decode_out_t *arg_decode_out, vpu_4K_D2_dec_output_t *dec_output, vpu_pmap_alloc_info_t *alloc_info)
{
	int ret = 0;

	if ((arg_decode_out != NULL) && (dec_output != NULL) && (alloc_info != NULL)) {
		vdec_v3_mapconv_info_t *mapconv = &arg_decode_out->out_info.disp_map_conv_info;

		arg_decode_out->display_out[VPU_PA][VPU_COMP_Y] = dec_output->m_pDispOut[VPU_PA][0];
		arg_decode_out->display_out[VPU_PA][VPU_COMP_U] = dec_output->m_pDispOut[VPU_PA][1];
		arg_decode_out->display_out[VPU_PA][VPU_COMP_V] = dec_output->m_pDispOut[VPU_PA][2];

		arg_decode_out->display_out[VPU_KVA][VPU_COMP_Y] = dec_output->m_pDispOut[VPU_KVA][0];
		arg_decode_out->display_out[VPU_KVA][VPU_COMP_U] = dec_output->m_pDispOut[VPU_KVA][1];
		arg_decode_out->display_out[VPU_KVA][VPU_COMP_V] = dec_output->m_pDispOut[VPU_KVA][2];

		arg_decode_out->decoded_out[VPU_PA][VPU_COMP_Y] = dec_output->m_pCurrOut[VPU_PA][0];
		arg_decode_out->decoded_out[VPU_PA][VPU_COMP_U] = dec_output->m_pCurrOut[VPU_PA][1];
		arg_decode_out->decoded_out[VPU_PA][VPU_COMP_V] = dec_output->m_pCurrOut[VPU_PA][2];

		arg_decode_out->decoded_out[VPU_KVA][VPU_COMP_Y] = dec_output->m_pCurrOut[VPU_KVA][0];
		arg_decode_out->decoded_out[VPU_KVA][VPU_COMP_U] = dec_output->m_pCurrOut[VPU_KVA][1];
		arg_decode_out->decoded_out[VPU_KVA][VPU_COMP_V] = dec_output->m_pCurrOut[VPU_KVA][2];

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

		vmgr_4kd2_set_compressed_data(mapconv, &dec_output->m_DecOutInfo.m_DispMapConvInfo);

		detail_4kd2("[id:%u] Decode output disp:0x%x(%x), 0x%x(%x), 0x%x(%x), decod:0x%x(%x), 0x%x(%x), 0x%x(%x), PicType:%d, disp_idx:%d, dec_idx%d, disp_stat:%d, dec_stat:%d, w:%d, h:%d, interlace_frame:%d, crop:%d,%d - %d,%d",
							drv_info->drv_id,
							arg_decode_out->display_out[VPU_PA][VPU_COMP_Y],
							arg_decode_out->display_out[VPU_KVA][VPU_COMP_Y],
							arg_decode_out->display_out[VPU_PA][VPU_COMP_U],
							arg_decode_out->display_out[VPU_KVA][VPU_COMP_U],
							arg_decode_out->display_out[VPU_PA][VPU_COMP_V],
							arg_decode_out->display_out[VPU_KVA][VPU_COMP_V],
							arg_decode_out->decoded_out[VPU_PA][VPU_COMP_Y],
							arg_decode_out->decoded_out[VPU_KVA][VPU_COMP_Y],
							arg_decode_out->decoded_out[VPU_PA][VPU_COMP_U],
							arg_decode_out->decoded_out[VPU_KVA][VPU_COMP_U],
							arg_decode_out->decoded_out[VPU_PA][VPU_COMP_V],
							arg_decode_out->decoded_out[VPU_KVA][VPU_COMP_V],
							arg_decode_out->out_info.pic_type,
							arg_decode_out->out_info.display_idx,
							arg_decode_out->out_info.decoded_idx,
							arg_decode_out->out_info.display_status,
							arg_decode_out->out_info.decoded_status,
							arg_decode_out->out_info.display_width,
							arg_decode_out->out_info.display_height,
							arg_decode_out->out_info.interlaced_frame,
							arg_decode_out->out_info.display_crop.left,
							arg_decode_out->out_info.display_crop.top,
							arg_decode_out->out_info.display_crop.right,
							arg_decode_out->out_info.display_crop.bottom);

		detail_4kd2("[id:%u] compressed Y:0x%x(0x%x), Cb:0x%x(0x%x), fbc_y:0x%x(0x%x), fbc_c:0x%x(0x%x), t_size:%d, %d, stride:%d, %d, depth:%d, %d, endian:%d",
							drv_info->drv_id,
							mapconv->compressed_y[VPU_PA],
							mapconv->compressed_y[VPU_KVA],
							mapconv->compressed_cb[VPU_PA],
							mapconv->compressed_cb[VPU_KVA],
							mapconv->fbc_y_offset_addr[VPU_PA],
							mapconv->fbc_y_offset_addr[VPU_KVA],
							mapconv->fbc_c_offset_addr[VPU_PA],
							mapconv->fbc_c_offset_addr[VPU_KVA],
							mapconv->compression_table_luma_size,
							mapconv->compression_table_chroma_size,
							mapconv->luma_stride,
							mapconv->chroma_stride,
							mapconv->luma_bit_depth,
							mapconv->chroma_bit_depth,
							mapconv->frame_endian);
	} else {
		ret = -1;
	}

	return ret;
}

//static char vpu_4kd2_api_version[] = VPU_4KD2_API_VERSION;
static void init_dec_options(vpu_4K_D2_dec_set_options_t *opt)
{
    //*opt = (vpu_4K_D2_dec_set_options_t){0};
	int i;
	opt->iUseBitstreamOffset = 0;
	opt->iMeasureDecPerf = 0;
	opt->pfPrintCb = NULL;
	for (i = 0; i < 16; i++) {
		opt->iReserved[i] = 0;
	}
}

static int vmgr_4kd2_dec_init(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;

	vpu_ip_module_t *each_ip = mgr_ctx->each_ip;
	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;
	vpu_4kd2_papam_t *ip_param = (vpu_4kd2_papam_t *)drv_info->ip_param;

	codec_handle_t decHandle;
	vpu_4K_D2_dec_init_t *pDecInit = &ip_param->dec_init;

	vdec_v3_init_t *arg_init = (vdec_v3_init_t *)cmd_info->args;
	vdec_v3_init_in_t *arg_init_in = &arg_init->input;

	vpu_4K_D2_dec_set_options_t *pst_dec_setopt = &ip_param->dec_opt;

	//debug settings for vpu_lib: echo 0xPXABBB > /sys/module/vpu/parameters/vdbg_lib
	unsigned int vpulib_dbg_param = get_vpu_lib_dbg_param();

	dlog_4kd2("[id:%u] VPU_DEC_INIT start", drv_id);

	pDecInit->m_RegBaseVirtualAddr = (codec_addr_t)mgr_ctx->base_addr;
	pDecInit->m_Memcpy = (void *(*) (void *, const void*, unsigned int, unsigned int))vetc_memcpy;
	pDecInit->m_Memset = (void (*) (void *, int, unsigned int, unsigned int))vetc_memset;
#if defined(ENABLE_CQ2)
	pDecInit->m_Interrupt = (int (*) (unsigned int, unsigned int, unsigned int *))vmgr_4k_d2_internal_handler;
#else
	pDecInit->m_Interrupt = (int (*) (unsigned int))vmgr_4k_d2_internal_handler;
#endif
	pDecInit->m_Ioremap = (void *(*) (phys_addr_t, unsigned int))vetc_ioremap;
	pDecInit->m_Iounmap = (void (*) (void *))vetc_iounmap;
	pDecInit->m_reg_read = (unsigned int (*)(void *, unsigned int))vetc_reg_read;
	pDecInit->m_reg_write = (void (*)(void *, unsigned int, unsigned int))vetc_reg_write;
	pDecInit->m_Usleep = (void (*)(unsigned int, unsigned int))vetc_usleep;
#if defined(ENABLE_CQ2)
	pDecInit->m_inform = (void (*)(int, void*))wave5_inform;
#endif

	pDecInit->m_iBitstreamFormat = vmgr_get_bitstream_format(arg_init_in->codec_id, VPU_OP_TYPE_DEC); //get from vcodec_id
	pDecInit->m_bEnableUserData = arg_init_in->enable_user_data;
	pDecInit->m_iCQCount = each_ip->cq_depth;

	switch (arg_init_in->output_format) {
	case VPU_OUTPUT_LINEAR_YUV420:
	{
		dlog_4kd2("[id:%u] ouput set VPU_OUTPUT_LINEAR_YUV420", drv_id);
		pDecInit->m_bCbCrInterleaveMode = 0U;
		pDecInit->m_uiDecOptFlags |= WAVE5_WTL_ENABLE;
	}
	break;

	case VPU_OUTPUT_LINEAR_NV12:
	{
		dlog_4kd2("[id:%u] ouput set VPU_OUTPUT_LINEAR_NV12", drv_id);
		pDecInit->m_bCbCrInterleaveMode = 1U;
		pDecInit->m_uiDecOptFlags |= WAVE5_WTL_ENABLE;
	}
	break;

	case VPU_OUTPUT_LINEAR_10_TO_8_BIT_YUV420:
	{
		dlog_4kd2("[id:%u] ouput set VPU_OUTPUT_LINEAR_10_TO_8_BIT_YUV420", drv_id);
		pDecInit->m_bCbCrInterleaveMode = 0U;
		pDecInit->m_uiDecOptFlags |= WAVE5_WTL_ENABLE;
		pDecInit->m_uiDecOptFlags |= WAVE5_10BITS_DISABLE;
	}
	break;

	case VPU_OUTPUT_LINEAR_10_TO_8_BIT_NV12:
	{
		dlog_4kd2("[id:%u] ouput set VPU_OUTPUT_LINEAR_10_TO_8_BIT_NV12", drv_id);
		pDecInit->m_bCbCrInterleaveMode = 1U;
		pDecInit->m_uiDecOptFlags |= WAVE5_WTL_ENABLE;
		pDecInit->m_uiDecOptFlags |= WAVE5_10BITS_DISABLE;
	}
	break;

	case VPU_OUTPUT_COMPRESSED_MAPCONV:
	{
		pDecInit->m_bCbCrInterleaveMode = 1U;
		dlog_4kd2("[id:%u] ouput set VPU_OUTPUT_COMPRESSED_MAPCONV", drv_id);
	}
	break;

	case VPU_OUTPUT_COMPRESSED_AFBC:
	{
		dlog_4kd2("[id:%u] ouput set VPU_OUTPUT_COMPRESSED_AFBC", drv_id);
		pDecInit->m_uiDecOptFlags |= WAVE5_AFBC_ENABLE;
	}
	break;

	default:
		err_4kd2("[id:%u] unknown output format:%d, Set as the default value", drv_id, arg_init_in->output_format);
	break;
	}

	//debug settings for vpu_lib: echo 0xPXABBB > /sys/module/vpu/parameters/vdbg_lib
	//set pre-condition for debugging
	if ((vpulib_dbg_param & VPU_DBG_LIB_CQ_COUNT) == VPU_DBG_LIB_CQ_COUNT) {
		unsigned int cq_count;
		init_dec_options(pst_dec_setopt);

		// Extract CQ count from debug parameter (X part)
		// CQ count can be 0, 1, or 2
		cq_count = (vpulib_dbg_param & 0x0F0000U) >> 16;

		// Check if CQ count is 0 or already set to the same value
		if ((cq_count == 0) || (cq_count == pDecInit->m_iCQCount)) {
			V_DBG(VPU_DBG_ERROR, "[4K_D2] CQ count unchanged or invalid: %d", cq_count);
		} else {
			V_DBG(VPU_DBG_ERROR, "[4K_D2] Changing CQ count from %d to %d", pDecInit->m_iCQCount, cq_count);

			// Set new CQ count
			pst_dec_setopt->iReserved[0] = 101;
			pst_dec_setopt->iReserved[1] = cq_count;  // New CQ count
			(void)tcc_vpu_4k_d2_dec_l(vpu_ap, VPU_4KD2_SET_OPTIONS, NULL, (void *)pst_dec_setopt, (void *)NULL);
		}
	}

	//debug settings for vpu_lib: echo 0xPXABBB > /sys/module/vpu/parameters/vdbg_lib
	// Check if debugging is enabled using VPU_DBG_LIB_USE_CB_PRINTK (P part)
	if ((vpulib_dbg_param & VPU_DBG_LIB_USE_CB_PRINTK) == VPU_DBG_LIB_USE_CB_PRINTK) {
		unsigned int codec_ip;

		// Extract codec_ip (A part), please refer to enum vpu_ip_type
		// VPU_IP_C7 = 1, VPU_IP_4KD2 = 2, VPU_IP_HEVC_ENC = 3, VPU_IP_HEVC_ENC2 = 4, VPU_IP_JPU_C6 = 5, VPU_IP_HEVC_DEC
		codec_ip = (vpulib_dbg_param & 0x00F000U) >> 12;
		V_DBG(VPU_DBG_ERROR, "[4K_D2] codec_ip: %d", codec_ip);

		// Check if codec_ip matches desired value
		if (codec_ip == VPU_IP_4KD2) {
			// Extract log_mask (BBB part)
			unsigned int log_mask = (vpulib_dbg_param & 0x000FFFU);

			V_DBG(VPU_DBG_ERROR, "[4K_D2] log_mask: %d (%x)", log_mask, log_mask);
			ip_param->dec_log.pfLogPrintCb = (void (*)(const char *, ...))vpu_printk;
			ip_param->dec_log.stLogLevel.bVerbose = (log_mask & 1U) ? 1 : 0;
			ip_param->dec_log.stLogLevel.bDebug   = (log_mask & 2U) ? 1 : 0;
			ip_param->dec_log.stLogLevel.bInfo    = (log_mask & 4U) ? 1 : 0;
			ip_param->dec_log.stLogLevel.bWarn    = (log_mask & 8U) ? 1 : 0;
			ip_param->dec_log.stLogLevel.bError   = (log_mask & 16U) ? 1 : 0; // 0x10
			ip_param->dec_log.stLogLevel.bAssert  = (log_mask & 32U) ? 1 : 0; // 0x20
			ip_param->dec_log.stLogLevel.bFunc    = (log_mask & 64U) ? 1 : 0; // 0x40
			ip_param->dec_log.stLogLevel.bTrace   = (log_mask & 128U) ? 1 : 0; // 0x80
			ret = tcc_vpu_4k_d2_dec_l(vpu_ap, VPU_4KD2_CTRL_LOG_STATUS, NULL, (void *)(&ip_param->dec_log), (void *)NULL);
		}
	}

	// Temporary - measure decoder performance
	if ((vpulib_dbg_param & VPU_DBG_LIB_MEASURE_DEC_PERF) == VPU_DBG_LIB_MEASURE_DEC_PERF) {
		V_DBG(VPU_DBG_ERROR, "[4K_D2] MEASURE_DEC_PERF");
		pDecInit->m_Reserved[2] = 0x1000; //Temporary
	}

	if (arg_init_in->dec_opt_flags > 0) {
		if ((arg_init_in->dec_opt_flags & VDEC_V3_USE_MAX_FRAMEBUFFER) != 0) {
			pDecInit->m_uiDecOptFlags |= (1 << 16);
			pDecInit->m_Reserved[3] = arg_init_in->max_support_width;
			pDecInit->m_Reserved[4] = arg_init_in->max_support_height;
			dlog_4kd2("[id:%u] set VDEC_USE_MAX_FRAMEBUFFER, (%d x %d)", drv_id, arg_init_in->max_support_width, arg_init_in->max_support_height);
		}

		if ((arg_init_in->dec_opt_flags & VDEC_V3_NO_BUFFER_DELAY) != 0) {
			pDecInit->m_uiDecOptFlags |= (1U << 2U);
			dlog_4kd2("[id:%u] set VDEC_NO_BUFFER_DELAY", drv_id);
		}
	}

#if defined(ENABLE_SEQHEADER_BUFFER_CHANGE)
	//seqheader use vpu_decode_t instead of seqheader_t
	pDecInit->m_uiDecOptFlags |= (1 << 26);
#endif

#if defined(ENABLE_VPU_FW_LOADING)
	pDecInit->m_uiDecOptFlags |= (1U << 7U);
#endif

	if (each_ip->buffer_mode == VPU_BS_MODE_RINGBUFFER) {
		pDecInit->m_iFilePlayEnable = 0;
	} else {
		pDecInit->m_iFilePlayEnable = 1;
	}

	dlog_4kd2("[id:%u] set %s mode", drv_id, (pDecInit->m_iFilePlayEnable == 1) ? "FilePlay" : "StreamingPlay");

	if (get_chip_rev() == 0 /*MPW1*/ && pDecInit->m_Reserved[10] == 10) {
		dlog_4kd2("[id:%u] Init In => enable WTL (off the compressed output mode)", drv_id);
		// to notify this refusal
		pDecInit->m_Reserved[10] = 5;

		//[work-around] reduce total memory size
		// of min. frame buffers
		// Max Bitdepth
		pDecInit->m_Reserved[5] = 8;
		pDecInit->m_uiDecOptFlags |= WAVE5_WTL_ENABLE; //disable map converter
		pDecInit->m_uiDecOptFlags |= WAVE5_10BITS_DISABLE;	// 10 to 8 bit shit
	}

	pDecInit->m_BitWorkAddr[PA] = alloc_info->bitwork_buf.addr[VPU_PA];
	pDecInit->m_BitWorkAddr[VA] = alloc_info->bitwork_buf.addr[VPU_KVA];

	pDecInit->m_BitstreamBufAddr[PA] = alloc_info->bitstream_buf.addr[VPU_PA];
	pDecInit->m_BitstreamBufAddr[VA] = alloc_info->bitstream_buf.addr[VPU_KVA];
	pDecInit->m_iBitstreamBufSize = alloc_info->bitstream_buf.size;

	if (pDecInit->m_bEnableUserData == 1U) {
		pDecInit->m_UserDataAddr[VPU_PA] = alloc_info->userdata_buf.addr[VPU_PA];
		pDecInit->m_UserDataAddr[VPU_KVA] = alloc_info->userdata_buf.addr[VPU_KVA];
		pDecInit->m_iUserDataBufferSize = alloc_info->userdata_buf.size;
	}

	dlog_4kd2(
	"[id:%u] Init In => workbuff 0x%x/0x%x, Reg: 0x%p/0x%x, format : %d, Stream(0x%x/0x%x, 0x%x)",
		drv_id,
		pDecInit->m_BitWorkAddr[PA],
		pDecInit->m_BitWorkAddr[VA],
		mgr_ctx->base_addr,
		pDecInit->m_RegBaseVirtualAddr,
		pDecInit->m_iBitstreamFormat,
		pDecInit->m_BitstreamBufAddr[PA],
		pDecInit->m_BitstreamBufAddr[VA],
		pDecInit->m_iBitstreamBufSize);

	dlog_4kd2(
	"[id:%u] Init In => optFlag 0x%x, Userdata(%d), userdata 0x%x/0x%x size:%d, Inter: %d, PlayEn: %d",
		drv_id,
		pDecInit->m_uiDecOptFlags,
		pDecInit->m_bEnableUserData,
		pDecInit->m_UserDataAddr[VPU_PA],
		pDecInit->m_UserDataAddr[VPU_KVA],
		pDecInit->m_iUserDataBufferSize,
		pDecInit->m_bCbCrInterleaveMode,
		pDecInit->m_iFilePlayEnable);

#if defined(ENABLE_VPU_FW_LOADING)
	if (mgr_ctx->fw_addr != 0) {
		vetc_memset(&ip_param->fw_info, 0x00, sizeof(vpu_4K_D2_dec_set_fw_addr_t), 0);
		ip_param->fw_info.m_FWBaseAddr = mgr_ctx->fw_addr;

		dlog_4kd2("[id:%u] VPU_4KD2_SET_FW_ADDRESS addr 0x%x", drv_id, mgr_ctx->fw_addr);
		ret = tcc_vpu_4k_d2_dec_l(vpu_ap, VPU_4KD2_SET_FW_ADDRESS,
				NULL, (void *)(&ip_param->fw_info), (void *)NULL);
	}
#endif

	ret = tcc_vpu_4k_d2_dec_l(vpu_ap, VPU_DEC_INIT, (codec_handle_t *)&decHandle, (void *)pDecInit, (void *)NULL);
	if (ret != RETCODE_CODEC_EXIT && decHandle != 0) {
		dlog_4kd2("[id:%u] Init Done with ret(0x%x)", drv_id, ret);
		drv_info->handle = decHandle;

		// measure decoder performance
		if ((vpulib_dbg_param & VPU_DBG_LIB_MEASURE_DEC_PERF) == VPU_DBG_LIB_MEASURE_DEC_PERF) {
			if (pDecInit->m_Reserved[2] == 0x1000) { //Temporary
				init_dec_options(pst_dec_setopt);

				V_DBG(VPU_DBG_ERROR, "[4K_D2] measure decoder performance");

				pst_dec_setopt->iMeasureDecPerf = 1;
				pst_dec_setopt->pfPrintCb = (void (*)(const char *, ...))vpu_wprintk;
				(void)tcc_vpu_4k_d2_dec_l(vpu_ap, VPU_4KD2_SET_OPTIONS, (codec_handle_t *)&decHandle, (void *)pst_dec_setopt, (void *)NULL);
			}
		}
	} else {
		err_4kd2("[id:%u] Error, ret:0x%x, handle:%p", drv_id, ret, decHandle);
	}

	return ret;
}

static int vmgr_4kd2_dec_seqheader(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_ip_module_t *each_ip = mgr_ctx->each_ip;
	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_4kd2_papam_t *ip_param = (vpu_4kd2_papam_t *)drv_info->ip_param;

	vpu_4K_D2_dec_input_t *pDecInput = &ip_param->seq_input;
	vpu_4K_D2_dec_initial_info_t *pDecInitialInfo = &ip_param->dec_initialInfo;

	vdec_v3_seqheader_t *arg_seqheader = (vdec_v3_seqheader_t *)cmd_info->args;
	vdec_v3_seqheader_in_t *arg_seqheader_in = &arg_seqheader->input;

	dlog_4kd2("[id:%u] seqheader use vpu_4K_D2_dec_input_t, input size:%d", drv_id, arg_seqheader_in->bitstream_size);

#if defined(ENABLE_SEQHEADER_BUFFER_CHANGE)
	pDecInput->m_BitstreamDataAddr[VPU_PA] = arg_seqheader_in->bitstream_addr[VPU_PA];
	pDecInput->m_BitstreamDataAddr[VPU_KVA] = arg_seqheader_in->bitstream_addr[VPU_KVA];
	pDecInput->m_iBitstreamDataSize = arg_seqheader_in->bitstream_size;

	ret = tcc_vpu_4k_d2_dec_l(vpu_ap, VPU_DEC_SEQ_HEADER, (codec_handle_t *)&pHandle, (void *)pDecInput,  (void *)pDecInitialInfo);
#else
	{
		union {
			unsigned int ui_data;
			unsigned int *pi_data;
			void *pv_data;
		} udata;

		udata.pi_data = NULL;
		udata.ui_data = arg_seqheader_in->bitstream_size;

		ret = tcc_vpu_4k_d2_dec_l(vpu_ap, VPU_DEC_SEQ_HEADER, (codec_handle_t *)&pHandle, (void *)udata.pv_data, (void *)pDecInitialInfo);
	}
#endif

	if (ret == RETCODE_SUCCESS) {
		vdec_v3_initial_info_t *init_info = &arg_seqheader->output.initial_info;

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
		init_info->interlace = pDecInitialInfo->m_iInterlace;
		init_info->aspectratio = pDecInitialInfo->m_iAspectRateInfo;
		init_info->report_error_reason = pDecInitialInfo->m_iReportErrorReason;
		init_info->bitdepth = pDecInitialInfo->m_iBitDepth;

		dlog_4kd2("[id:%u] [2] sizeLinearLuma:%d, [3] sizeLinearChroma:%d, [4] sizeCompLuma:%d, [5] sizeCompChroma:%d, [6] mvColSize:%d, [7] fbcYTblSize:%d, [8] fbcCTblSize:%d",
			drv_id, pDecInitialInfo->m_uiBufSizeLinearLuma, pDecInitialInfo->m_uiBufSizeLinearChroma,
			pDecInitialInfo->m_uiBufSizeCompressedLuma, pDecInitialInfo->m_uiBufSizeCompressedChroma,
			pDecInitialInfo->m_uiBufSizeMVCol, pDecInitialInfo->m_uiBufSizeFBCYTable, pDecInitialInfo->m_uiBufSizeFBCCTable);

		init_info->metadata = pDecInitialInfo->m_uiUserData;
		if (sizeof(vpu_metadata_info_t) == sizeof(vpu_4K_D2_dec_UserData_info_t)) {
			vpu_metadata_info_t *meta = &init_info->metadata_info;
			vetc_memcpy(meta, &pDecInitialInfo->m_UserDataInfo, sizeof(vpu_metadata_info_t), 0);
			dlog_4kd2("[id:%u] metadata info, colour_primaries:%u, transfer_characteristics:%u, matrix_coefficients:%u",
				drv_id,
				meta->vui_param.colour_primaries,
				meta->vui_param.transfer_characteristics,
				meta->vui_param.matrix_coefficients);
		} else {
			dlog_4kd2("[id:%u] different size of parameter, target:%lu, src:%lu",  drv_id, sizeof(vpu_metadata_info_t), sizeof(vpu_4K_D2_dec_UserData_info_t));
		}

		vetc_memcpy(&drv_info->initial_info, init_info, sizeof(vdec_v3_initial_info_t), 0);

		dlog_4kd2("[id:%u] VPU_DEC_SEQ_HEADER out ret:%d, min framebuffer cont:%d, size:%d, res info:%d - %d - %d, %d - %d - %d",
			drv_id, ret,
			init_info->min_frame_buffer_count,
			init_info->min_frame_buffer_size,
			init_info->pic_width,
			init_info->pic_crop.left,
			init_info->pic_crop.right,
			init_info->pic_height,
			init_info->pic_crop.top,
			init_info->pic_crop.bottom);

		each_ip->clock_ctrl->change_clock(each_ip->clock_ctrl, pDecInitialInfo->m_iPicWidth, pDecInitialInfo->m_iPicHeight);

	}

	return ret;
}

static int vmgr_4kd2_dec_register_framebuffer(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;
	vpu_4kd2_papam_t *ip_param = (vpu_4kd2_papam_t *)drv_info->ip_param;

	vpu_4K_D2_dec_buffer_t *pDecBuffer = &ip_param->dec_buffer;

	pDecBuffer->m_FrameBufferStartAddr[VPU_PA] = alloc_info->frame_buf.addr[VPU_PA];
	pDecBuffer->m_FrameBufferStartAddr[VPU_KVA] = alloc_info->frame_buf.addr[VPU_KVA];
	pDecBuffer->m_iFrameBufferCount = alloc_info->framebuffer_count;

	dlog_4kd2("[id:%u] VPU_DEC_REG_FRAME_BUFFER in :: addr:0x%x/0x%x, frame buffer count:%d",
		drv_id, pDecBuffer->m_FrameBufferStartAddr[VPU_PA], pDecBuffer->m_FrameBufferStartAddr[VPU_KVA], pDecBuffer->m_iFrameBufferCount);

	ret = tcc_vpu_4k_d2_dec_l(vpu_ap, VPU_DEC_REG_FRAME_BUFFER, (codec_handle_t *)&pHandle, (void *)pDecBuffer, (void *)NULL);

	return ret;
}

static int vmgr_4kd2_dec_user_framebuffer(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;
	int ii;
	int linear_start_idx = -1;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_4kd2_papam_t *ip_param = (vpu_4kd2_papam_t *)drv_info->ip_param;

	vdec_v3_reg_framebuffer_t *arg_register = (vdec_v3_reg_framebuffer_t *)cmd_info->args;
	vdec_v3_reg_framebuffer_in_t *input = &arg_register->input;
	vpu_4K_D2_dec_buffer3_t *pDecBuffer = &ip_param->dec_buffer3;
	vpu_4K_D2_dec_init_t *pDecInit = &ip_param->dec_init;

	if ((pDecInit->m_uiDecOptFlags & WAVE5_WTL_ENABLE) == WAVE5_WTL_ENABLE) {
		linear_start_idx = input->frame_buffer_count / 2;
		detail_4kd2("[id:%u] VPU_DEC_REG_FRAME_BUFFER3, linear start index:%d", drv_id, linear_start_idx);
	}

	pDecBuffer->m_iFrameBufferCount = input->frame_buffer_count;
	for (ii = 0; ii < input->frame_buffer_count; ii++) {
		/*
		//[0]linear/compressed luma, [1]linear/compressed chroma cb, [2]linear cr, [3]fbcY, [4]fccC, [5]mvcol
		*/
		//compressed output
		if ((linear_start_idx != -1) && (ii >= linear_start_idx)) {
			//linear buffer
			pDecBuffer->m_addrFrameBuffer[PA][ii][0] = input->frameBuffer[ii][VPU_FRAMEBUFFER_Y].framebuffer[VPU_PA];
			pDecBuffer->m_addrFrameBuffer[VA][ii][0] = input->frameBuffer[ii][VPU_FRAMEBUFFER_Y].framebuffer[VPU_KVA];

			pDecBuffer->m_addrFrameBuffer[PA][ii][1] = input->frameBuffer[ii][VPU_FRAMEBUFFER_CB].framebuffer[VPU_PA];
			pDecBuffer->m_addrFrameBuffer[VA][ii][1] = input->frameBuffer[ii][VPU_FRAMEBUFFER_CB].framebuffer[VPU_KVA];

			pDecBuffer->m_addrFrameBuffer[PA][ii][2] = input->frameBuffer[ii][VPU_FRAMEBUFFER_CR].framebuffer[VPU_PA];
			pDecBuffer->m_addrFrameBuffer[VA][ii][2] = input->frameBuffer[ii][VPU_FRAMEBUFFER_CR].framebuffer[VPU_KVA];
			detail_4kd2("[id:%u] linear index: %d, y:0x%x, cb:0x%x, cr:0x%x", drv_id, ii,
				pDecBuffer->m_addrFrameBuffer[PA][ii][0], pDecBuffer->m_addrFrameBuffer[PA][ii][1], pDecBuffer->m_addrFrameBuffer[PA][ii][2]);
		} else {
			//compressed buffer
			pDecBuffer->m_addrFrameBuffer[PA][ii][0] = input->frameBuffer[ii][VPU_FRAMEBUFFER_COMP_Y].framebuffer[VPU_PA];
			pDecBuffer->m_addrFrameBuffer[VA][ii][0] = input->frameBuffer[ii][VPU_FRAMEBUFFER_COMP_Y].framebuffer[VPU_KVA];

			pDecBuffer->m_addrFrameBuffer[PA][ii][1] = input->frameBuffer[ii][VPU_FRAMEBUFFER_COMP_C].framebuffer[VPU_PA];
			pDecBuffer->m_addrFrameBuffer[VA][ii][1] = input->frameBuffer[ii][VPU_FRAMEBUFFER_COMP_C].framebuffer[VPU_KVA];

			pDecBuffer->m_addrFrameBuffer[PA][ii][2] = 0;
			pDecBuffer->m_addrFrameBuffer[VA][ii][2] = 0;

			pDecBuffer->m_addrFrameBuffer[PA][ii][3] = input->frameBuffer[ii][VPU_FRAMEBUFFER_FBCY].framebuffer[VPU_PA];
			pDecBuffer->m_addrFrameBuffer[VA][ii][3] = input->frameBuffer[ii][VPU_FRAMEBUFFER_FBCY].framebuffer[VPU_KVA];

			pDecBuffer->m_addrFrameBuffer[PA][ii][4] = input->frameBuffer[ii][VPU_FRAMEBUFFER_FBCC].framebuffer[VPU_PA];
			pDecBuffer->m_addrFrameBuffer[VA][ii][4] = input->frameBuffer[ii][VPU_FRAMEBUFFER_FBCC].framebuffer[VPU_KVA];

			pDecBuffer->m_addrFrameBuffer[PA][ii][5] = input->frameBuffer[ii][VPU_FRAMEBUFFER_MVCOL].framebuffer[VPU_PA];
			pDecBuffer->m_addrFrameBuffer[VA][ii][5] = input->frameBuffer[ii][VPU_FRAMEBUFFER_MVCOL].framebuffer[VPU_KVA];

			detail_4kd2("[id:%u] compressed index: %d, comp y:0x%x, c:0x%x, fbcy:0x%x, fbcc:0x%x, mvcol:0x%x", drv_id, ii,
				 pDecBuffer->m_addrFrameBuffer[PA][ii][0], pDecBuffer->m_addrFrameBuffer[PA][ii][1],
				 pDecBuffer->m_addrFrameBuffer[PA][ii][3], pDecBuffer->m_addrFrameBuffer[PA][ii][4], pDecBuffer->m_addrFrameBuffer[PA][ii][5]);
		}
	}

	detail_4kd2("[id:%u] VPU_DEC_REG_FRAME_BUFFER3 in, frame_buffer_count:%d", drv_id, pDecBuffer->m_iFrameBufferCount);
	ret = tcc_vpu_4k_d2_dec_l(vpu_ap, VPU_DEC_REG_FRAME_BUFFER3, (codec_handle_t *) &pHandle, (void *)pDecBuffer, (void *)NULL);
	detail_4kd2("[id:%u] VPU_DEC_REG_FRAME_BUFFER3, ret:%d", drv_id, ret);

	return ret;
}

static int vmgr_4kd2_dec_decode(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;
	int rcnt;
	unsigned int hiding_suferframe;

	vpu_ip_module_t *each_ip = mgr_ctx->each_ip;
	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;
	vpu_4kd2_papam_t *ip_param = (vpu_4kd2_papam_t *)drv_info->ip_param;

	vpu_4K_D2_dec_input_t *pDecInput = &ip_param->dec_input;
	vpu_4K_D2_dec_output_t *pDecOutput = &ip_param->dec_output;

	vdec_v3_decode_t *arg_decode = (vdec_v3_decode_t *)cmd_info->args;
	vdec_v3_decode_in_t *arg_decode_in = &arg_decode->input;
	vdec_v3_decode_out_t *arg_decode_out = &arg_decode->output;

	pDecInput->m_BitstreamDataAddr[VPU_PA] = arg_decode_in->bitstream_addr[VPU_PA];
	pDecInput->m_BitstreamDataAddr[VPU_KVA] = arg_decode_in->bitstream_addr[VPU_KVA];
	pDecInput->m_iBitstreamDataSize = arg_decode_in->bitstream_size;

	if (drv_info->dec_init_info.enable_user_data == 1U) {
		pDecInput->m_UserDataAddr[VPU_PA] = alloc_info->userdata_buf.addr[VPU_PA];
		pDecInput->m_UserDataAddr[VPU_KVA] = alloc_info->userdata_buf.addr[VPU_KVA];
		pDecInput->m_iUserDataBufferSize = alloc_info->userdata_buf.size;
	}

	//the control of frame skip-related behavior is handled by vpu_dec
	if (arg_decode_in->skip_mode == (int)VPU_FRAMESKIP_DISABLED) {
		pDecInput->m_iSkipFrameMode = 0;
	} else if (arg_decode_in->skip_mode == (int)VPU_FRAMESKIP_NON_I) {
		pDecInput->m_iSkipFrameMode = 1;
		detail_4kd2("[id:%u] set I-frame search", drv_id);
	} else {
		detail_4kd2("[id:%u] invalid skip mode", drv_id);
		pDecInput->m_iSkipFrameMode = 0;
	}

	hiding_suferframe = 20;

	for (rcnt = 0; rcnt < VPU_4K_D2_MAX_SUPER_FRAME; rcnt++) {
		detail_4kd2("[id:%u]  Dec In => 0x%x - 0x%x, 0x%x, 0x%x - 0x%x, %d, flag: %d",
			drv_id,
			pDecInput->m_BitstreamDataAddr[PA],
			pDecInput->m_BitstreamDataAddr[VA],
			pDecInput->m_iBitstreamDataSize,
			pDecInput->m_UserDataAddr[PA],
			pDecInput->m_UserDataAddr[VA],
			pDecInput->m_iUserDataBufferSize,
			pDecInput->m_iSkipFrameMode);

		ret = tcc_vpu_4k_d2_dec_l(vpu_ap, VPU_DEC_DECODE, (codec_handle_t *)&pHandle, (void *)pDecInput, (void *)pDecOutput);

		detail_4kd2("[id:%u] Dec Out => %d - %d - %d, %d - %d - %d",
			drv_id,
			pDecOutput->m_DecOutInfo.m_iDisplayWidth,
			pDecOutput->m_DecOutInfo.m_DisplayCropInfo.m_iCropLeft,
			pDecOutput->m_DecOutInfo.m_DisplayCropInfo.m_iCropRight,
			pDecOutput->m_DecOutInfo.m_iDisplayHeight,
			pDecOutput->m_DecOutInfo.m_DisplayCropInfo.m_iCropTop,
			pDecOutput->m_DecOutInfo.m_DisplayCropInfo.m_iCropBottom);

		detail_4kd2("[id:%u] Dec Out => ret[%d] !! PicType[%d], OutIdx[%d/%d], OutStatus[%d/%d], POC[%d/%d]", drv_id, ret,
			pDecOutput->m_DecOutInfo.m_iPicType,
			pDecOutput->m_DecOutInfo.m_iDispOutIdx,
			pDecOutput->m_DecOutInfo.m_iDecodedIdx,
			pDecOutput->m_DecOutInfo.m_iOutputStatus,
			pDecOutput->m_DecOutInfo.m_iDecodingStatus,
			pDecOutput->m_DecOutInfo.m_Reserved[5],
			pDecOutput->m_DecOutInfo.m_Reserved[6]);

		if (pDecOutput->m_DecOutInfo.m_iDecodingStatus == VPU_DEC_BUF_FULL) {
			err_4kd2("[id:%u] Buffer full", drv_id);
			break;
		} else if (pDecOutput->m_DecOutInfo.m_iDecodingStatus == VPU_DEC_VP9_SUPER_FRAME) {
				detail_4kd2("[id:%u] superframe: sub-frame num: %u", drv_id, pDecOutput->m_DecOutInfo.m_SuperFrameInfo.m_uiNframes);
				if (hiding_suferframe == 20U && pDecOutput->m_DecOutInfo.m_iDispOutIdx < 0) {
					//repeat the decoding process for VP9 super-frame
				} else {
					break;
				}
		} else {
			break;
		}
	}

	if (ret == RETCODE_SUCCESS) {
		(void)vmgr_4kd2_set_output(drv_info, arg_decode_out, pDecOutput, alloc_info);

		each_ip->clock_ctrl->change_clock(each_ip->clock_ctrl, pDecOutput->m_DecOutInfo.m_iDecodedWidth, pDecOutput->m_DecOutInfo.m_iDecodedHeight);
	}

	return ret;
}

static int vmgr_4kd2_dec_buf_clear(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;
	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;

	vdec_v3_buf_clear_t *arg_bufclear = (vdec_v3_buf_clear_t *)cmd_info->args;

	int *arg = (int *)&arg_bufclear->index;

	detail_4kd2("[id:%u] VPU_CMD_DEC_BUF_FLAG_CLEAR, index:%d", drv_id, *arg);
	ret = tcc_vpu_4k_d2_dec_l(vpu_ap, VPU_DEC_BUF_FLAG_CLEAR, (codec_handle_t *) &pHandle, (void *)(arg), (void *)NULL);
	return ret;
}

static int vmgr_4kd2_dec_flush(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;
	vpu_4kd2_papam_t *ip_param = (vpu_4kd2_papam_t *)drv_info->ip_param;

	int flush_frame = 0;
	vpu_4K_D2_dec_input_t *pDecInput = &ip_param->dec_input;
	vpu_4K_D2_dec_output_t *pDecOutput = &ip_param->dec_output;

	dlog_4kd2("[id:%u] VPU_CMD_DEC_FLUSH, in", drv_id);

	while (flush_frame < 32) {
		pDecInput->m_BitstreamDataAddr[VPU_PA] = alloc_info->bitstream_buf.addr[VPU_PA];
		pDecInput->m_BitstreamDataAddr[VPU_KVA] = alloc_info->bitstream_buf.addr[VPU_KVA];
		pDecInput->m_iBitstreamDataSize = 0;
		pDecInput->m_iSkipFrameMode = 0; //VDEC_SKIP_FRAME_DISABLE

		ret = tcc_vpu_4k_d2_dec_l(vpu_ap, VPU_DEC_FLUSH_OUTPUT, (codec_handle_t *) &pHandle, (void *)pDecInput, (void *)pDecOutput);
		if (ret == RETCODE_SUCCESS) {
			if (pDecOutput->m_DecOutInfo.m_iOutputStatus == VPU_DEC_OUTPUT_SUCCESS) {
				int *arg = (int *)&pDecOutput->m_DecOutInfo.m_iDispOutIdx;

				dlog_4kd2("[id:%u] VPU_DEC_BUF_FLAG_CLEAR %d", drv_id, pDecOutput->m_DecOutInfo.m_iDispOutIdx);
				ret = tcc_vpu_4k_d2_dec_l(vpu_ap, VPU_DEC_BUF_FLAG_CLEAR, (codec_handle_t *)&pHandle, (void *)(arg), (void *)NULL);
			}
		} else if (ret == RETCODE_CODEC_FINISH) {
			dlog_4kd2("[id:%u] flush done!, flush_frame:%d, ret:%d", drv_id, flush_frame, ret);
			break;
		}

		flush_frame++;
	}

	return ret;
}

static int vmgr_4kd2_dec_drain(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;
	vpu_pmap_alloc_info_t *alloc_info = &drv_info->pmap_alloc_info;
	vpu_4kd2_papam_t *ip_param = (vpu_4kd2_papam_t *)drv_info->ip_param;

	int flush_frame = 0;
	vpu_4K_D2_dec_input_t *pDecInput = &ip_param->dec_input;
	vpu_4K_D2_dec_output_t *pDecOutput = &ip_param->dec_output;

	vdec_v3_drain_t *arg_drain = (vdec_v3_drain_t *)cmd_info->args;
	vdec_v3_decode_out_t *arg_decode_out = &arg_drain->output;

	dlog_4kd2("[id:%u] VPU_CMD_DEC_DRAIN, in", drv_id);

	pDecInput->m_BitstreamDataAddr[VPU_PA] = alloc_info->bitstream_buf.addr[VPU_PA];
	pDecInput->m_BitstreamDataAddr[VPU_KVA] = alloc_info->bitstream_buf.addr[VPU_KVA];
	pDecInput->m_iBitstreamDataSize = 0;
	pDecInput->m_iSkipFrameMode = 0; //VDEC_SKIP_FRAME_DISABLE

	ret = tcc_vpu_4k_d2_dec_l(vpu_ap, VPU_DEC_FLUSH_OUTPUT, (codec_handle_t *) &pHandle, (void *)pDecInput, (void *)pDecOutput);
	if (ret == RETCODE_SUCCESS) {
		(void)vmgr_4kd2_set_output(drv_info, arg_decode_out, pDecOutput, alloc_info);
	} else if (ret == RETCODE_CODEC_FINISH) {
		dlog_4kd2("[id:%u] drain done!, flush_frame:%d, ret:%d", drv_id, flush_frame, ret);
	}

	return ret;
}

static int vmgr_4kd2_dec_close(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	long pHandle = drv_info->handle;

	vpu_accesspoint_t *vpu_ap = mgr_ctx->access_point;

	dlog_4kd2("[id:%u] VPU_4K_D2_DEC_CLOSED", drv_id);
	ret = tcc_vpu_4k_d2_dec_l(vpu_ap, VPU_DEC_CLOSE, (codec_handle_t *)&pHandle, (void *)NULL, (void *)NULL);

	return ret;
}

static int vmgr_4kd2_decode_process(void *vpu_private, enum vpu_cmd_type cmd, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info)
{
	int ret = 0;
	unsigned int drv_id = drv_info->drv_id;
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)vpu_private;

	detail_4kd2("[id:%u] %s(%d)/start mgr_ctx:%p, drv_id:%d", drv_id, vmgr_cmd_name(cmd), cmd, mgr_ctx, drv_id);

	switch (cmd) {
	case VPU_CMD_DEC_INIT:
	{
		ret = vmgr_4kd2_dec_init(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_DEC_SEQ_HEADER:
	{
		ret = vmgr_4kd2_dec_seqheader(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_DEC_REG_FRAME_BUFFER:
	{
		ret = vmgr_4kd2_dec_register_framebuffer(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_DEC_REG_USER_FRAME_BUFFER:
	{
		ret = vmgr_4kd2_dec_user_framebuffer(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_DEC_DECODE:
	{
		ret = vmgr_4kd2_dec_decode(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_DEC_BUF_FLAG_CLEAR:
	{
		ret = vmgr_4kd2_dec_buf_clear(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_DEC_FLUSH:
	{
		ret = vmgr_4kd2_dec_flush(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_DEC_DRAIN:
	{
		ret = vmgr_4kd2_dec_drain(mgr_ctx, cmd_info, drv_info);
	}
	break;

	case VPU_CMD_DEC_CLOSE:
	{
		ret = vmgr_4kd2_dec_close(mgr_ctx, cmd_info, drv_info);
	}
	break;

#if 0
	case GET_RING_BUFFER_STATUS:
	{
		VPU_4K_D2_RINGBUF_GETINFO_t *arg = (VPU_4K_D2_RINGBUF_GETINFO_t *)cmd_info->args;

		ret = vpu_ap->tccfp_vpu_dec(cmd, (codec_handle_t *)&pHandle, (void *)NULL, (void *)&arg->gsV4kd2DecRingStatus);
	}
	break;

	case VPU_UPDATE_WRITE_BUFFER_PTR:
	{
		union {
			int i_data;
			int *pi_data;	//NULL
			void *pv_data;
		} udata, flushbuf;

		VPU_4K_D2_RINGBUF_SETBUF_PTRONLY_t *arg = (VPU_4K_D2_RINGBUF_SETBUF_PTRONLY_t *)cmd_info->args;

		udata.pi_data = NULL;
		udata.i_data = arg->iCopiedSize;

		flushbuf.pi_data = NULL;
		flushbuf.i_data = arg->iFlushBuf;
		ret = vpu_ap->tccfp_vpu_dec(cmd, (codec_handle_t *)&pHandle, (void *)(udata.pv_data), (void *)(flushbuf.pv_data));
	}
	break;
#endif

	default:
	{
		err_4kd2("[id:%u] not supported command(0x%x)", drv_id, cmd);
		ret = 0x999;
	}
	}

	//V_DBG(VPU_DBG_INFO, "%s(%d)/finish mgr_ctx:%p, drv_id:%d", vmgr_dec_cmd_name(cmd), cmd, mgr_ctx, drv_id);
	ret = vmgr_convert_retcode(ret);
	return ret;
}

static int vmgr_4kd2_get_buffer_size(void *vpu_private, enum vmgr_buffer_type buf_type, vpu_drv_info_t *drv_info)
{
	int size = 0;
	vpu_4kd2_papam_t *ip_param = (vpu_4kd2_papam_t *)drv_info->ip_param;
	vpu_4K_D2_dec_init_t *pDecInit = (vpu_4K_D2_dec_init_t *)&ip_param->dec_init;
	vpu_4K_D2_dec_initial_info_t *pDecInitInfo = &ip_param->dec_initialInfo;

	//for unused buffers, they must be set to 0.
	switch (buf_type) {
	case VMGR_BUF_BITSTREAM:
		size = ALIGNED_BUFF(WAVE5_STREAM_BUF_SIZE, 4096u); //20Mb
	break;

	case VMGR_BUF_NUM_OF_BITSTREAM:
		size = VPU_4KD2_NUM_OF_BITSTREAM_BUFFERS;
	break;

	case VMGR_BUF_BITWORK:
		size = ALIGNED_BUFF(WAVE5_WORK_CODE_BUF_SIZE, 4096u);
	break;

	case VMGR_BUF_FRAMEBUF:
		size = 1; //use framebuffer, calculating from vpu_mgr.c using min framebuffer count, size
	break;

	case VMGR_BUF_SPSPPS:
		size = 0;
	break;

	case VMGR_BUF_USERDATA:
		size = ALIGNED_BUFF((512U * 1024U), 4096u);
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

	//from here
	//when using the user framebuffer register,
	case VMGR_BUF_Y:
	{
		size = pDecInitInfo->m_uiBufSizeLinearLuma;
	}
	break;

	case VMGR_BUF_CB:
	{
		if (pDecInit->m_bCbCrInterleaveMode == 1U) {
			size = pDecInitInfo->m_uiBufSizeLinearChroma * 2;
		} else {
			size = pDecInitInfo->m_uiBufSizeLinearChroma; //nv12 to yuv420
		}
	}
	break;

	case VMGR_BUF_CR:
	{
		if (pDecInit->m_bCbCrInterleaveMode == 1U) {
			size = 0; //nv12 has no cr
		} else {
			size = pDecInitInfo->m_uiBufSizeLinearChroma; //nv12 to yuv420
		}
	}
	break;

	case VMGR_BUF_MVCOL:
	{
		size = pDecInitInfo->m_uiBufSizeMVCol;
	}
	break;

	case VMGR_BUF_FBCY:
	{
		size = pDecInitInfo->m_uiBufSizeFBCYTable;
	}
	break;

	case VMGR_BUF_FBCC:
	{
		size = pDecInitInfo->m_uiBufSizeFBCCTable;
	}
	break;

	case VMGR_BUF_COMP_Y:
	{
		size = pDecInitInfo->m_uiBufSizeCompressedLuma;
	}
	break;

	case VMGR_BUF_COMP_C:
	{
		size = pDecInitInfo->m_uiBufSizeCompressedChroma * 2;
	}
	break;

	default:
		size = 0;
	break;
	}

	detail_4kd2("[%s][%s][id:%u]: buf_type:%d, size:%d", vmgr_get_ip_name(drv_info->ip_type), vmgr_get_optype_name(drv_info->op_type), drv_info->drv_id, buf_type, size);
	return size;
}

static irqreturn_t vmgr_4k_d2_isr_handler(int irq, void *vpu_private)
{
	unsigned int reason;
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)vpu_private;

	reason = vetc_reg_read(mgr_ctx->base_addr, 0x004C/*W5_VPU_VINT_REASON*/);
	reason &= 0x7FFFFFFFU;

	atomic_or(reason, &mgr_ctx->oper_intr);

	wake_up_interruptible(&mgr_ctx->oper_wq);

	return IRQ_HANDLED;
}

extern vmgr_clock_t vpu_4kd2_clock; //from vpu_4k_d2_mgr_sys.c

#if defined(ENABLE_CQ2)
static cq_func_t vpu_4kd2_cq2 = {
	.vpu_get_id_with_reason = vmgr_4k_d2_get_id_with_reason,
	.vpu_get_interrupt_status = vmgr_4k_d2_interrupt_status
};
#endif

static vpu_ip_module_t vpu_4kd2_module = {
	.ip_type = VPU_IP_4KD2,
#if defined(ENABLE_CQ2)
	.cq_type = VPU_CQ_MULTI,
	.cq_depth = WAVE5_COMMAND_QUEUE_DEPTH,
#else
	.cq_type = VPU_CQ_LEGACY,
	.cq_depth = WAVE5_COMMAND_QUEUE_DEPTH,
#endif
	.buffer_mode = VPU_BS_MODE_LINEARBUFFR,
	.internal_timeout_ms = 200,
	.enc_param_size = 0,
	.dec_param_size = sizeof(vpu_4kd2_papam_t),
	.internal_handler = NULL, //since the 4kd2 internal handler uses different parameters, you should directly invoke the internal handler inside process()
	//codec id, codec name(string), profile(string), level(string), width(unsigned int), height(unsigned int), fps(unsigned int)
	.dec_capa = {{VCODEC_ID_HEVC, CODEC_NAME_HEVC, "main/main10", "5.1 high tier", 3840U, 2160U, 60U},
				{VCODEC_ID_NONE, NULL, NULL, NULL, 0U, 0U, 0U}},
	.enc_capa = {{VCODEC_ID_NONE, NULL, NULL, NULL, 0U, 0U, 0U}},
	.clock_ctrl = &vpu_4kd2_clock,
	.proc_encode = NULL,
	.proc_decode = vmgr_4kd2_decode_process,
	.proc_get_buffer_size = vmgr_4kd2_get_buffer_size,
	.isr_handler = vmgr_4k_d2_isr_handler,

#if defined(ENABLE_CQ2)
	.cq_func = &vpu_4kd2_cq2,
#else
	.cq_func = NULL,
#endif
	.ip_private = NULL,
	.access_point_path = VPU_4K_D2_ACCESSPOINT_PATH,
};

int vmgr_4k_d2_probe(struct platform_device *pdev)
{
	int ret = 0;

	vpu_mgr_t *mgr_ctx = NULL;

	mgr_ctx = vmgr_alloc(&vpu_4kd2_module);
	if (mgr_ctx != NULL) {
#if !defined(USE_ACCESS_POINT)
		mgr_ctx->access_point->tccfp_vpu_dec = tcc_vpu_4k_d2_dec;
		mgr_ctx->access_point->tccfp_vpu_enc = NULL;
#else
		mgr_ctx->access_point = NULL;
#endif

		ret = vmgr_probe(mgr_ctx, pdev, VPU_4K_D2_MGR_NAME);
		if (ret == 0) {
			//assigning VPU manager context to avoid mutex race condition in interrupt handler
			vpu_4kd2_mgr_ctx = (vpu_mgr_t *)vmgr_get_context(VPU_IP_4KD2);
		}
	}

	platform_set_drvdata(pdev, mgr_ctx);
	return ret;
}

EXPORT_SYMBOL(vmgr_4k_d2_probe);

VREMOVE_RET_TYPE vmgr_4k_d2_remove(struct platform_device *pdev)
{
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)platform_get_drvdata(pdev);

	if (mgr_ctx->each_ip->ip_private != NULL) {
		VPU_free(mgr_ctx->each_ip->ip_private);
		mgr_ctx->each_ip->ip_private = NULL;
	}

	vmgr_remove(mgr_ctx, pdev);
	vmgr_free(mgr_ctx);
	vpu_4kd2_mgr_ctx = NULL;

	VREMOVE_RETURN();
}

EXPORT_SYMBOL(vmgr_4k_d2_remove);

#if defined(CONFIG_PM)
int vmgr_4k_d2_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)platform_get_drvdata(pdev);

	ret = vmgr_suspend(mgr_ctx, pdev, state);
	return ret;
}

EXPORT_SYMBOL(vmgr_4k_d2_suspend);

int vmgr_4k_d2_resume(struct platform_device *pdev)
{
	vpu_mgr_t *mgr_ctx = (vpu_mgr_t *)platform_get_drvdata(pdev);

	vmgr_resume(mgr_ctx, pdev);
	return 0;
}

EXPORT_SYMBOL(vmgr_4k_d2_resume);
#endif

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
MODULE_SOFTDEP("pre: vpu_lib jpu_lib hevc_lib vpu_4k_d2_lib vpu_hevc_enc_lib vpu");

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC vpu_4k_d2 vp9/hevc manager");
MODULE_LICENSE("Dual BSD/GPL");

#endif //ENABLE_VPU_DRV_4K_D2
