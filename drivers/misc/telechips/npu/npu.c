// SPDX-License-Identifier: GPL-2.0
/*
 * npu.c - openedges npu driver
 *
 * Copyright (C) 2020 Openedges
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fcntl.h>
#include <linux/unistd.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/kref.h>
#include <linux/io.h>
#include <linux/interrupt.h>

#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/delay.h>
#include <asm/cacheflush.h>
#include <linux/anon_inodes.h>
#include <linux/file.h>
#include <linux/mutex.h>
#include <linux/vmstat.h>
#include <linux/mmzone.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/clk.h>
#include <linux/mm.h>
#include <linux/regulator/driver.h>

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>

#include "npu.h"
#include "npu_def.h"
#include "npu_reg.h"

#define WAIT_EVENT_INT_TO_WORKAROUND //NDOLPHIN-271

#define MAX_MLX_KERNEL_SIZE (32*1024)
#define MAX_MLX_FILES_NUM   (8U)

static char *mlx_kernel_fnames[8] = { 
	"mlx_kernel.bin",
	"test_kernel_0.bin",
	"test_kernel_1.bin",
	"test_kernel_2.bin",
	"test_kernel_3.bin",
	"test_kernel_4.bin",
	"test_kernel_5.bin",
	"test_kernel_6.bin"
};

//#define ENLIGHT_DEBUG

enum {
	NPU_READY,
	NPU_BUSY,
	NPU_STOP
};

enum {
	NET_IDLE,
	NET_BUSY,
	NET_STOP,
	NET_DONE,
	NET_ERR,
	NET_DMA_BUSY
};

enum {
	NPU_CTRL_RUN        = 0x1U,
	NPU_CTRL_SW_RESET   = 0x2U,
	NPU_CTRL_CMD_DMA    = 0x4U,
};

enum {
	RST_CTRL_IBUS       = 0x10000U,
	RST_CTRL_MCLK_EN    = 0xF00U,
	RST_CTRL_MRST_N     = 0xFU
};

enum {
	NPU_ERR_FLAG_NONE   = 0x0U,
	NPU_ERR_FLAG_UE     = 0x1U,
	NPU_ERR_FLAG_CE     = 0x2U,
	NPU_ERR_FLAG_WDT    = 0x4U,
};


#define NPU_OPCODE_WAIT     (0x0U)
#define NPU_OPCODE_RUN      (0x1U)
#define NPU_OPCODE_WR_REG   (0x2U)
#define NPU_OPCODE_TRAP     (0x3U)

#define EN_ECC_GBUF_CBUF    (0xF1U)

#define IRQ_CBUF_ECC        (0x00300000U)
#define IRQ_GBUF_ECC        (0x000FF000U)
#define IRQ_MLX_ECC         (0x00000F00U)
#define IRQ_MLX             (0x000000F0U)
#define IRQ_TRAP            (0x00000004U)
#define IRQ_ECC             (0x003FFF00U)
#define IRQ_ALL             (0x003FFFF4U)

#define ECC_UE_DISABLE      (0x2)
#define ECC_CE_DISABLE      (0x1)
#define ECC_DISABLE         (0x3)


enum {
	MASK_CORE0_BUSY     = 0xF0000000U,
	MASK_CORE1_BUSY     = 0x0F000000U,
	MASK_CORE2_BUSY     = 0x00F00000U,
	MASK_CORE3_BUSY     = 0x000F0000U,
	MASK_DMA_BUSY       = 0x00000100U,
};

#define DRAM2CMD            0x2U
#define DRAM2MLX            0x6U

#define npu_align(x, g)     ((((x)+(g)-1) & (~((g)-1))))
#define npu_page_align(x)   npu_align(x, PAGE_SIZE)

//#define NPU_MUTE
#ifdef NPU_MUTE
#	define npu_drv_err(fmt, args...)          do {} while(0)
#	define npu_drv_info(fmt, args...)         do {} while(0)
#else
#	define npu_drv_err(fmt, args...) \
		do { \
			pr_err("[EnDrv]%4d:%s:" fmt, \
			__LINE__, __func__, ##args); \
		} while(0)

#	define npu_drv_info(fmt, args...) \
		do { \
			pr_info("[EnDrv]%4d:%s:" fmt, \
			__LINE__, __func__, ##args); \
		} while(0)
#endif

#ifdef ENLIGHT_DEBUG
#define npu_drv_fin(val) \
	do { \
		pr_info("[EnDrv fin]%4d:%s():0x%x\n", \
		__LINE__, __func__, val); \
	} while(0)
#define npu_drv_fout(val) \
	do { \
		pr_info("[EnDrv fout]%4d:%s():0x%x\n", \
		__LINE__, __func__, val); \
	} while(0)

#define npu_drv_dbg(fmt, args...) \
	do { \
		pr_info("[EnDrv Dbg]%4d:%s():" fmt, \
		__LINE__, __func__, ##args); \
	} while(0)
#else

#define npu_drv_fin(val)          do {} while(0)
#define npu_drv_fout(val)         do {} while(0)
#define npu_drv_dbg(fmt, args...) do {} while(0)

#endif


MODULE_AUTHOR("Hunt <hunt.hj.jeon@openedges.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("2.0");
MODULE_DESCRIPTION("enlight npu driver");


static struct class npu_class = {
	.name = "npu",
};

// cppcheck-suppress misra-c2012-8.2
static DEFINE_IDA(openedge_ida);

struct npu_sched {
	struct work_struct work;
	struct list_head queue;
	struct mutex lock;
};

struct npu {
	int status;

	struct cdev cdev;
	struct device *dev;

	int id;

	unsigned int irq;
	wait_queue_head_t irq_waitq;
	atomic_t irq_done;

	void __iomem *membase;
	struct npu_buf *work;

	struct mutex lock;
	struct npu_sched sched;

	struct clk *npu_aclk;
	struct clk *npu_pclk;
	struct clk *npu_cclk;
	struct clk *npu_cpuclk;

	ecc_wdt_access_req_t ecc_wdt_status;

	unsigned int disable_ue_fail;
	unsigned int disable_ce_fail;
	unsigned int disable_wdt;
	unsigned int mlx_bin_idx;

	unsigned int wdt_ext_cnt;
	unsigned int wdt_int_cnt;

	unsigned int ecc_test_ctrl;
	unsigned int mlx_err_inj_mask_data;
	unsigned int mlx_err_inj_mask_par;

	unsigned int dbg_print_reg;
};

struct npu_buf {
	struct npu *npu;
	void *buf;

	dma_addr_t phys_addr;

	unsigned long phys_start;
	unsigned long size;

	struct kref ref_cnt;
};

struct npu_net {
	int status;
	int color_format;

	struct npu *npu;
	struct npu_buf *cmd_buf;
	struct npu_buf *wei_buf;

	wait_queue_head_t poll_waitq;

	struct kref ref_cnt;
};

struct npu_cmd {
	struct npu_net *net;

	struct npu_buf *in;
	struct npu_buf *out;

	struct list_head list;
};


static struct npu devs[NPU_MAX_MINORS];

static long npu_init_mlx_firmware(struct npu *npu);
static void npu_net_free(struct kref *ref);
static void npu_buffer_free(struct kref *ref);

static void npu_init_ecc_status(struct npu *npu);
static void npu_read_ecc_regs(struct npu *npu);
static int npu_get_ecc_wdt_status(struct npu *npu);

static int npu_reset(struct npu *npu, int arg);
static long npu_init_npu(struct npu *npu, unsigned long arg);
static int npu_enable_wdt(struct npu *npu);
static int npu_disable_wdt(struct npu *npu);
static int npu_enable_ecc(struct npu *npu);


static inline struct npu_buf *npu_buffer_get(int fd)
{
	struct file *file = fget(fd);
	struct npu_buf *buf = (struct npu_buf *)file->private_data;

	kref_get(&buf->ref_cnt);
	fput(file);

	return buf;
}

static inline void npu_buffer_put(struct npu_buf *buf)
{
	kref_put(&buf->ref_cnt, npu_buffer_free);
}

static inline void NPU_WRITE_REG(int addr, int data, struct npu *npu)
{
	if (npu->dbg_print_reg != 0U) {
		pr_info("[NPU-W]0x%08x<=0x%08x\n", addr, data);
	}

	writel_relaxed(data, npu->membase + addr);
}

static inline unsigned int NPU_READ_REG(int addr, struct npu *npu)
{
	unsigned int data;	

	data = readl_relaxed(npu->membase + addr);

	if (npu->dbg_print_reg != 0U) {
		pr_info("[NPU-R]0x%08x=>0x%08x\n", addr, data);
	}

	return data;
}

static void npu_hard_reset(struct npu *npu)
{
	unsigned int data;

	// ASSERT MLX RESET w/ clock enabled
	data = RST_CTRL_IBUS | RST_CTRL_MCLK_EN;
	NPU_WRITE_REG(ADDR_NPU_INT_RST_CTRL, data, npu);

	// ASSERT MLX RESET & DISABLE MLX CLOCK
	NPU_WRITE_REG(ADDR_NPU_INT_RST_CTRL, 0, npu);

	// RESET GBUS/MBUS
	// Jake. double internal bus reset(Dec212022)
	data = RST_CTRL_IBUS;
	NPU_WRITE_REG(ADDR_NPU_INT_RST_CTRL, data, npu);
	NPU_WRITE_REG(ADDR_NPU_INT_RST_CTRL, data, npu);

	// Disable CLOCK GATING
	NPU_WRITE_REG(ADDR_NPU_CG_CTRL, 0x70, npu);

	// NPU SOFT_RESET
	data = NPU_CTRL_CMD_DMA | NPU_CTRL_SW_RESET;
	NPU_WRITE_REG(ADDR_NPU_CONTROL, data, npu);

	// NPU SOFT_RESET
	NPU_WRITE_REG(ADDR_NPU_CONTROL, NPU_CTRL_SW_RESET, npu);
	udelay(1);

	// Enable CLOCK GATING
	NPU_WRITE_REG(ADDR_NPU_CG_CTRL, 0x0, npu);

	// Reset clk_en status
	NPU_WRITE_REG(ADDR_NPU_CONTROL, NPU_CTRL_SW_RESET, npu);

	// Disable interrupt
	NPU_WRITE_REG(ADDR_NPU_IRQ_MASK, 0, npu);
	NPU_WRITE_REG(ADDR_NPU_IRQ_ENABLE, 0, npu);

	// Clear all reason
	NPU_WRITE_REG(ADDR_NPU_IRQ_REASON, 0x0FFFFFFF, npu);
	NPU_WRITE_REG(ADDR_NPU_IRQ_CLEAR, 1, npu);

	atomic_set(&npu->irq_done, 0);

	// Enable interrupt
	NPU_WRITE_REG(ADDR_NPU_IRQ_MASK, IRQ_TRAP, npu);
	NPU_WRITE_REG(ADDR_NPU_IRQ_ENABLE, IRQ_ALL, npu);
}

static void npu_soft_reset(struct npu *npu)
{
	int cg;

	cg = NPU_READ_REG(ADDR_NPU_CG_CTRL, npu);

	// Disable CLOCK GATING
	NPU_WRITE_REG(ADDR_NPU_CG_CTRL, 0x70, npu);

	NPU_WRITE_REG(ADDR_NPU_CONTROL, NPU_CTRL_CMD_DMA | NPU_CTRL_SW_RESET, npu);

	NPU_WRITE_REG(ADDR_NPU_CONTROL, NPU_CTRL_SW_RESET, npu);
	udelay(1);

	// Enable CLOCK GATING
	NPU_WRITE_REG(ADDR_NPU_CG_CTRL, cg, npu);

	atomic_set(&npu->irq_done, 0);
}

static int npu_stop(struct npu *npu)
{
	int i;
	int ret;

	npu_drv_fin(0);

	// stop fetching command
	NPU_WRITE_REG(ADDR_NPU_CONTROL, 0, npu);

	// wait DMA_IDLE
	ret = -ETIMEDOUT;
	for (i = 0; i < NPU_WAIT_DMA_BUSY_CNT; i++) {
		if (!(NPU_READ_REG(ADDR_NPU_STATUS, npu) & MASK_DMA_BUSY)) {
			ret = 0;
			break;
		}

		mdelay(1);
	}

	npu_drv_err("NPU DMA waiting timeout\n");

	npu_drv_fout(0);

	return ret;
}

static int npu_wait_interrupt(struct npu *npu, u32 timeout)
{
	atomic_t *irq_done = &npu->irq_done;
	int ret;

	npu_drv_fin(0);

#ifdef WAIT_EVENT_INT_TO_WORKAROUND
	ret = wait_event_timeout(npu->irq_waitq,
				atomic_add_unless(irq_done, -1, 0),
				msecs_to_jiffies(timeout));
#else
	ret = wait_event_interruptible_timeout(npu->irq_waitq,
				atomic_add_unless(irq_done, -1, 0),
				msecs_to_jiffies(timeout));
#endif

	if (ret <= 0) {
		(void)npu_stop(npu);
		ret = -ETIMEDOUT;
	}
	else {
		ret = 0;
	}

	npu_drv_fout(ret);

	return ret;
}

/** @brief NPU interrupt handler
 *      NPU_IRQ_REASON
 *          [31:24] trap_id
 *          [23:22] reserved
 *          [   21] irq_cmd_buf_ecc_ce
 *          [   20] irq_cmd_buf_ecc_ue
 *          [19:16] irq_gbuf_ecc_ce_c3:c0
 *          [15:12] irq_gbuf_ecc_ue_c3:c0
 *          [11: 8] irq_mlx_ecc_c3:c0
 *          [ 7: 4] irq_mlx_c3:c0
 *          [    2] irq_trap
 *          [    0] irq_q_emtpy
 */
// cppcheck-suppress misra-c2012-2.7
static irqreturn_t npu_interrupt_handler(int irq, void *handle)
{
	// cppcheck-suppress misra-c2012-11.5
	struct npu *npu = (struct npu *)handle;
	unsigned int reason;

	reason = NPU_READ_REG(ADDR_NPU_IRQ_REASON, npu);
	reason = reason & 0x00FFFFFFU;

	if ((reason & IRQ_TRAP) != 0U) {
		NPU_WRITE_REG(ADDR_NPU_IRQ_REASON, IRQ_TRAP, npu);

		// NPU_CONTROL
		//  [2]:0  0: APB(interactive mode) 1: DMA
		//  [1]:1  SW_RESET
		//  [0]:0  0: STOP, 1:RUN
		NPU_WRITE_REG(ADDR_NPU_CONTROL, NPU_CTRL_SW_RESET, npu);
	}
	else {
		npu_drv_err("%08x: Unknown IRQ_REASON\n", reason);
		NPU_WRITE_REG(ADDR_NPU_IRQ_REASON, reason, npu);
	}

	NPU_WRITE_REG(ADDR_NPU_IRQ_CLEAR, 1, npu);

	atomic_set(&npu->irq_done, 1);
#ifdef WAIT_EVENT_INT_TO_WORKAROUND
	wake_up(&npu->irq_waitq);
#else
	wake_up_interruptible(&npu->irq_waitq);
#endif

	npu_drv_dbg("%08x: IRQ reason\n", reason);

	return IRQ_HANDLED;
}

static void npu_set_color_format(int color_format, struct npu *npu)
{
	unsigned int data;

	if (color_format == NPU_COLOR_YUV) {
		// 
		//          R_CB   0/128      R_Y  128/128     R_CR 197/128
		// conv0  = (0x000U << 20) + (0x080U << 10) + (0x0c5U << 0);
		//          G_CB -23/128      G_Ya 128/128     G_CR -59/128
		// conv1  = (0x3e9U << 20) + (0x080U << 10) + (0x3c5U << 0);
		//          B_CB  232/128 B_Y   128/128 B_CR   0/128
		// conv2  = (0x0e8U << 20) + (0x080U << 10) + (0x000U << 0);
		//          R_BIAS - 128 : -325 G_BIAS - 128 :  -46 B_BIAS - 128 : -360
		// bias   = (0x2bbU << 20) + (0x3d2U << 10) + (0x298U << 0);
		// 
		data = 0x000200C5U;
		NPU_WRITE_REG(ADDR_NPU_COLOR_CONV_0, data, npu);
		data = 0x3E9203C5U;
		NPU_WRITE_REG(ADDR_NPU_COLOR_CONV_1, data, npu);
		data = 0x0E820000U;
		NPU_WRITE_REG(ADDR_NPU_COLOR_CONV_2, data, npu);
		data = 0x2BBF4A98U;
		NPU_WRITE_REG(ADDR_NPU_COLOR_CONV_BIAS, data, npu);
	} else {
		// NPU_COLOR_RGB
		//          R 128/128        G 0/128          B 0/128
		// conv0 = (0x080U << 20) + (0x000U << 10) + (0x000U << 0);
		//          R 0/128          G 128/128        B 0/128
		// conv1 = (0x000U << 20) + (0x080U << 10) + (0x000U << 0);
		//          R 0/128          G 0/128          B 128/128
		// conv2 = (0x000U << 20) + (0x000U << 10) + (0x080U << 0)
		//          R_BIAS - 128     G_BIAS - 128     B_BIAS - 128
		// bias  = (0x380U << 20)  + (0x380U << 10)  + (0x380U << 0)
		// 
		data = 0x08000000U;
		NPU_WRITE_REG(ADDR_NPU_COLOR_CONV_0, data, npu);
		data = 0x00020000U;
		NPU_WRITE_REG(ADDR_NPU_COLOR_CONV_1, data, npu);
		data = 0x00000080U;
		NPU_WRITE_REG(ADDR_NPU_COLOR_CONV_2, data, npu);
		data = 0x380E0380U;
		NPU_WRITE_REG(ADDR_NPU_COLOR_CONV_BIAS, data, npu);
	}
}

static int npu_run_pic(struct npu_cmd *cmd)
{
	unsigned int data;
	int ret;

	struct npu_net *net = cmd->net;
	struct npu *npu = net->npu;

	struct npu_buf *in = cmd->in;
	struct npu_buf *out = cmd->out;
	int err_status;
	int wdt_act_status;
	int wdt_deact_status;

	net->status = NET_BUSY;

	npu_drv_fin(0);

	dma_sync_single_for_device(npu->dev,
		in->phys_addr,
		in->size,
		DMA_TO_DEVICE);

	mutex_lock(&npu->lock);

	npu_init_ecc_status(npu);

	// Reset
	npu_set_color_format(net->color_format, npu);

	npu_soft_reset(npu);

	wdt_act_status = npu_enable_wdt(npu);

	// APB command for RUN_INFERENCE
	// load_description
	// [31: 28] :   2 => NPU_OPCODE_WR_REG
	// [23: 16] :   4 => count - 1
	// [15:  0] : 004 => iaddr
	data = 0x20040004U;
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, data, npu);

	// reg1
	// [31: 0] : ext mem base
	data = 0x00000000U;
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, data, npu);

	// reg2
	// [10: 8] : 0 => base_buf_idx
	// [ 2: 0] : 2 => DRAM2CMD
	data = 0x00000002U;
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, data, npu);

	// reg3
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, 0x0, npu);

	// reg4
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, 0x0, npu);

	// reg5
	// [31:20] : 0 => num_chunk_m1
	// [12: 0] : F => sz_chunk
	data = 0x0000000FU;
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, data, npu);

	// reg0
	// [31:28]; 1 => op_code NPU_OPCODE_RUN
	// [   24]; 1 => dma_flag
	// [23:21]; 0 => dma_id
	// [20:16]; 1 => wdata
	// [12: 0]; 0 => iaddr
	data = 0x11010000U;
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, data, npu);

	NPU_WRITE_REG(ADDR_NPU_BASE_ADDR0, net->cmd_buf->phys_addr >> 4, npu);
	NPU_WRITE_REG(ADDR_NPU_BASE_ADDR1, net->wei_buf->phys_addr >> 4, npu);
	NPU_WRITE_REG(ADDR_NPU_BASE_ADDR2, npu->work->phys_addr >> 4, npu);
	NPU_WRITE_REG(ADDR_NPU_BASE_ADDR3, (in->phys_addr >> 4), npu);
	NPU_WRITE_REG(ADDR_NPU_BASE_ADDR4, (out->phys_addr >> 4), npu);

	NPU_WRITE_REG(ADDR_NPU_CONTROL, NPU_CTRL_RUN | NPU_CTRL_CMD_DMA, npu);

	ret = npu_wait_interrupt(npu, 1000);

	// Read ECC status regs and then clear ECC status regs
	npu_read_ecc_regs(npu);

	NPU_WRITE_REG(ADDR_NPU_IRQ_REASON, IRQ_ECC, npu);
	NPU_WRITE_REG(ADDR_NPU_IRQ_CLEAR, 1, npu);

	err_status = npu_get_ecc_wdt_status(npu);

	wdt_deact_status = npu_disable_wdt(npu);

	if (!ret) {
		net->status = NET_DONE;

		if ((wdt_act_status != 0) || (wdt_deact_status != 0)) {
			net->status = NET_ERR;
			npu_drv_err("WDT activate or disable error\n");
		}
		if (err_status != 0) {
			net->status = NET_ERR;
			npu_drv_err("%08x: err status\n", err_status);
		}

	} else {
		net->status = NET_ERR;

		data = NPU_READ_REG(ADDR_NPU_CMD_CNT, npu);
		npu_drv_err("%08x: NPU_CMD_CNT\n", data);
		data = NPU_READ_REG(ADDR_NPU_STATUS, npu);
		npu_drv_err("%08x: NPU_STATUS\n", data);
	}

	mutex_unlock(&npu->lock);

	dma_sync_single_for_cpu(npu->dev,
			out->phys_addr,
			out->size,
			DMA_FROM_DEVICE);

	wake_up_interruptible(&net->poll_waitq);

	npu_buffer_put(in);
	npu_buffer_put(out);

	npu_drv_fout(0);

	return ret;
}

static void npu_work_func(struct work_struct *work)
{
	struct npu_sched *sched = container_of(work, struct npu_sched, work);
	struct npu_cmd *cmd;
	struct npu_cmd *tmp;

	cmd = NULL;
	tmp = NULL;

	list_for_each_entry_safe(cmd, tmp, &sched->queue, list) {
		struct npu_net *net = cmd->net;

		(void)npu_run_pic(cmd);

		kref_put(&net->ref_cnt, npu_net_free);

		mutex_lock(&sched->lock);
		list_del(&cmd->list);
		mutex_unlock(&sched->lock);

		kfree(cmd);
	}
}

// cppcheck-suppress misra-c2012-2.7
static int npu_release(struct inode *inode_p, struct file *const file)
{
	struct npu *npu = file->private_data;

	// Enable CLOCK GATING
	NPU_WRITE_REG(ADDR_NPU_CG_CTRL, 0x0, npu);

	// ASSERT MLX RESET & DISABLE MLX CLOCK
	NPU_WRITE_REG(ADDR_NPU_INT_RST_CTRL, 0, npu);

	module_put(THIS_MODULE);

	return 0;
}

static int npu_open(struct inode *inode, struct file *file)
{
	struct npu *npu;
	int ret;
	unsigned int data;

	file->private_data = &devs[iminor(inode)];
	npu = file->private_data;

	npu->disable_ue_fail = 0U;
	npu->disable_ce_fail = 0U;
	npu->wdt_ext_cnt = 800000000U;   // 800MHz 1000ms
	npu->wdt_int_cnt = 4000000U;     // 1 tick for 100 cycle
	npu->mlx_bin_idx = 0U;
	npu->disable_wdt = 0U;
	npu->ecc_test_ctrl = 0xF1U;
	npu->mlx_err_inj_mask_data = 0U;
	npu->mlx_err_inj_mask_par  = 0U;
	npu->dbg_print_reg = 0U;

	data = NPU_READ_REG(ADDR_NPU_ID_CODE, npu);
	if ((data & 0xFFFFU) != 0xED9EU) {
		npu_drv_err("%X Wrong ID_CODE\n", data);
		ret = -EINVAL;
	}
	else {
		ret = 0;
	}

	if (!ret) {
		npu_init_ecc_status(npu);

		ret = npu_reset(npu, 1);
		if (ret != 0) {
			npu_drv_err("npu hard reset failed\n");
		}
	}

	if (!ret) {
		ret = npu_enable_ecc(npu);
		if (ret != 0) {
			npu_drv_err("npu_enable_ecc failed\n");
		}
	}

	if (!ret) {
		ret = npu_init_mlx_firmware(npu);
		if (ret != 0) {
			npu_drv_err("npu_init_mlx_firmware failed\n");
		}
	}

	if (!ret) {
		ret = npu_disable_wdt(npu);
		if (ret != 0) {
			npu_drv_err("npu_disable_wdt failed\n");
		}
	}

	if (!ret) {
		try_module_get(THIS_MODULE);
	}
	else {
		npu_drv_err("npu_open failed\n");
	}

	// Read ECC status regs and then clear ECC status regs
	npu_read_ecc_regs(npu);

	return ret;
}

// cppcheck-suppress misra-c2012-2.7
static int npu_net_release(struct inode *const inode, struct file *const file)
{
	struct npu_net *net = file->private_data;

	npu_drv_fin(0);

	kref_put(&net->ref_cnt, npu_net_free);

	npu_drv_fout(0);

	return 0;
}

static void npu_net_free(struct kref *ref)
{
	struct npu_net *net = container_of(ref, struct npu_net, ref_cnt);
	struct npu_buf *buf;

	struct npu *npu = net->npu;

	npu_drv_fin(0);

	if (net->cmd_buf != NULL) {
		buf = net->cmd_buf;

		dma_free_coherent(npu->dev,
				buf->size,
				buf->buf,
				buf->phys_addr);

		kfree(buf);
	}

	if (net->wei_buf != NULL) {
		buf = net->wei_buf;

		dma_free_coherent(npu->dev,
				buf->size,
				buf->buf,
				buf->phys_addr);

		kfree(buf);
	}

	kfree(net);

	npu_drv_fout(0);
}

static unsigned int npu_poll(struct file *file, poll_table *wait)
{
	struct npu_net *net = file->private_data;
	int ret = 0;

	poll_wait(file, &net->poll_waitq, wait);

	if (net->status == NET_DONE) {
		net->status = NET_IDLE;
		ret = EPOLLIN;

	} else if (net->status == NET_ERR) {
		net->status = NET_IDLE;
		ret = EPOLLERR;

	} else if (net->status == NET_STOP) {
		net->status = NET_IDLE;
		ret = EPOLLERR;

	} else if (net->status == NET_DMA_BUSY) {
		net->status = NET_IDLE;
		ret = EPOLLERR;
	} else {
		ret = 0;
	}

	return ret;
}

static long npu_net_run(struct npu_net *net, unsigned long arg)
{
	// cppcheck-suppress misra-c2012-11.6
	const void __user *udata = (void __user *)arg;
	struct net_run_req req;

	struct npu *npu = net->npu;
	struct npu_cmd *cmd;
	int ret;

	ret = 0;
	if (copy_from_user(&req, udata, sizeof(struct net_run_req)) != 0) {
		ret = -EINVAL;
	}

	if (!ret) {
		cmd = kmalloc(sizeof(struct npu_cmd), GFP_KERNEL);
		if (!cmd) {
			ret = -EINVAL;
		}
	}

	if (!ret) {
		kref_get(&net->ref_cnt);

		cmd->net = net;

		cmd->in = npu_buffer_get(req.in_fd);
		cmd->out = npu_buffer_get(req.out_fd);

		mutex_lock(&npu->sched.lock);

		list_add_tail(&cmd->list, &npu->sched.queue);
		schedule_work(&npu->sched.work);

		mutex_unlock(&npu->sched.lock);
	}

	return ret;
}

static long npu_net_profile(struct npu_net *net, unsigned long arg)
{
	// cppcheck-suppress misra-c2012-11.6
	const void __user *udata = (void __user *)arg;
	struct net_profile_req req;

	struct npu_cmd cmd = {0, };

	struct timespec64 start;
	struct timespec64 end;

	struct npu *npu = net->npu;

	u64 elapsed_in_ns;
	int err;

	err = 0;
	if (copy_from_user(&req, udata, sizeof(struct net_profile_req)) != 0) {
		err = -EINVAL;
	}

	if (!err) {

		cmd.net = net;

		cmd.in = npu_buffer_get(req.in_fd);
		cmd.out = npu_buffer_get(req.out_fd);

		NPU_WRITE_REG(ADDR_NPU_PERF_DMA, 0, npu);
		NPU_WRITE_REG(ADDR_NPU_PERF_COMP, 0, npu);
		NPU_WRITE_REG(ADDR_NPU_PERF_ALL, 0, npu);

		ktime_get_ts64(&start);
		(void) npu_run_pic(&cmd);
		ktime_get_ts64(&end);

		req.dma = NPU_READ_REG(ADDR_NPU_PERF_DMA, npu);
		req.comp = NPU_READ_REG(ADDR_NPU_PERF_COMP, npu);
		req.all = NPU_READ_REG(ADDR_NPU_PERF_ALL, npu);

		elapsed_in_ns = timespec64_to_ns(&end) - timespec64_to_ns(&start);

		req.elapsed_in_us = elapsed_in_ns / 1000L;

		if (copy_to_user((void __user *)udata, &req, sizeof(struct net_profile_req)) != 0) {
			err = -EINVAL;
		}
	}

	return err;
}

static long npu_net_set_color_format(struct npu_net *net, unsigned long arg)
{
	int data = (int)arg;
	int ret;

	if (data >= NPU_COLOR_END) {
		ret = -EINVAL;
	} else {
		ret = 0;
	}

	net->color_format = data;

	return ret;
}

static long npu_net_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	struct npu_net *net = fp->private_data;
	int ret;

	npu_drv_fin(0);

	switch (cmd) {
	case NPU_NET_IOCTL_RUN:
		ret = npu_net_run(net, arg);
		break;

	case NPU_NET_IOCTL_PROFILE:
		ret = npu_net_profile(net, arg);
		break;

	case NPU_NET_IOCTL_SET_COLOR_FMT:
		ret = npu_net_set_color_format(net, arg);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	npu_drv_fout(0);

	return ret;
}

static const struct file_operations npu_net_fops = {
	.release = &npu_net_release,
	.unlocked_ioctl = &npu_net_ioctl,
	.poll = &npu_poll,
};

static int npu_buffer_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ret;
	struct npu_buf *npu = file->private_data;
	ssize_t memlen = vma->vm_end - vma->vm_start;
	u64 memaddr = 0;

	memaddr = npu->phys_addr;
	if (!memaddr) {
		ret = -EFAULT;
	}

	ret = remap_pfn_range(vma,
			vma->vm_start,
			PFN_DOWN(memaddr),
			memlen,
			vma->vm_page_prot);
	return ret;
}

// cppcheck-suppress misra-c2012-2.7
static int npu_buffer_release(struct inode *const inode,
			struct file *const file)
{
	struct npu_buf *buf = file->private_data;

	npu_drv_fin(0);

	kref_put(&buf->ref_cnt, npu_buffer_free);

	npu_drv_fout(0);

	return 0;
}

static void npu_buffer_free(struct kref *ref)
{
	struct npu_buf *buf = container_of(ref, struct npu_buf, ref_cnt);

	npu_drv_fin(0);

	dma_free_coherent(buf->npu->dev,
			buf->size,
			buf->buf,
			buf->phys_addr);

	kfree(buf);

	npu_drv_fout(0);
}

static const struct file_operations npu_buffer_fops = {
	.release = &npu_buffer_release,
	.mmap = &npu_buffer_mmap,
};

static int mlx_validate(struct npu *npu, uint32_t core_id)
{
	unsigned int base;
	unsigned int data;
	int cnt = 1000;
	int ret;

	base = ADDR_NPU_MLX_C0_HCI_00 + (core_id * 0x40U);
	NPU_WRITE_REG(base + 0x4U, 0xED9EU, npu);
	NPU_WRITE_REG(base + 0x0U, 0x101U, npu);

	do {
		data = NPU_READ_REG(base + 0x0U, npu);

		mdelay(1);
		if (cnt-- == 0) {
			npu_drv_err("mlx validation timeout\n");
			break;
		}
	} while ((data & 0x10U) != 0U);

	data = NPU_READ_REG(base + 0x8U, npu);

	if (data != 0x107EU) {
		ret = -1;
	} else {
		ret = 0;
	}

	return ret;
}

static int mlx_load_kernel(struct npu *npu, struct npu_buf *buf)
{
	unsigned int i;
	unsigned int core_flags;
	unsigned int data;
	unsigned int tmp;
	int ret;
	unsigned int num_core;

	mutex_lock(&npu->lock);

	// Prepare DMA operation

	data = NPU_READ_REG(ADDR_NPU_ID_CODE, npu);
	num_core = (data >> 24) & 0xfU;
	num_core += 1U;

	NPU_WRITE_REG(ADDR_NPU_BASE_ADDR7, buf->phys_addr >> 4, npu);

	// [31: 28] :   2 => NPU_OPCODE_WR_REG
	// [23: 16] :   4 => count - 1
	// [15:  0] : 004 => iaddr
	data = 0x20040004U;
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, data, npu);

	// reg1
	// [31: 0] : ext mem base
	data = 0x00000000U;
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, data, npu);

	// reg2
	// [23:20] : dest_core_wr
	// [10: 8] : 7 => base_buf_idx
	// [ 2: 0] : 6 => DRAM2MLX
	core_flags = (1U << num_core) - 1U;
	tmp = core_flags << 20;
	data = 0x706U;
	data += tmp;
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, data, npu);

	// reg3
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, 0x0, npu);

	// reg4
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, data, npu);

	// reg5
	// [31: 20] :   0 => num_chunk_m1
	// [23: 16] :   size_chunk32_m1
	data = ((buf->size + 31U) >> 5U) - 1U;
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, data, npu);

	// RUN DMA to load MLX Kernel
	// [31: 28] :   2 => NPU_OPCODE_RUN
	// [	24] :   1 => dma_flag=STALL_WHEN_DMA_Q_FULL
	// [23: 21] :   2 => dma_id
	// [20: 16] :   1 => wdata
	// [12:  0] :   0 => iaddr
	data = 0x11410000;
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, data, npu);

	// WAIT until DMA operation is done
	// [31: 28] :   0 => NPU_OPCODE_WAIT
	// [23: 21] :   2 => dma_id
	data = 0x00400000;
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, data, npu);

	// TRAP
	// [31: 28] :   3 => NPU_OPCODE_TRAP
	// [23: 16] :   1 => option=CLEAR_RUN_STATE
	// [ 7:  0] :   0 => trap_id
	data = 0x30010000;
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, data, npu);

	NPU_WRITE_REG(ADDR_NPU_CONTROL, NPU_CTRL_RUN, npu);

	// wait queued operation is done
	ret = npu_wait_interrupt(npu, 1000);

	if (!ret) {
		// Run MLX Cores
		NPU_WRITE_REG(ADDR_NPU_INT_RST_CTRL, 0xf00U, npu);
		mdelay(1);
		NPU_WRITE_REG(ADDR_NPU_INT_RST_CTRL, 0xf0fU, npu);
		mdelay(1);

		// Validate Cores
		for (i = 0; i < num_core; i++) {
			if (mlx_validate(npu, i) != 0) {
				ret = -1;
				break;
			}
		}
	}
	else {
		data = NPU_READ_REG(ADDR_NPU_CMD_CNT, npu);
		npu_drv_err("%08x: NPU_CMD_CNT\n", data);
		data = NPU_READ_REG(ADDR_NPU_STATUS, npu);
		npu_drv_err("%08x: NPU_STATUS\n", data);
	}

	mutex_unlock(&npu->lock);

	npu_drv_dbg("All %d MLX core(s) is(are) validated.\r\n", num_core);

	return ret;
}

/*
   copy_from/to_user() is to copy block data between user and kernel.
   And user and kernel memory space are independent and
   implemented in separate address spaces.
*/
static long npu_read_reg(struct npu *npu, unsigned long arg)
{
	// cppcheck-suppress misra-c2012-11.6
	const void __user *udata = (void __user *)arg;
	struct reg_access_req req;
	int err;

	err = 0;

	if (copy_from_user(&req, udata, sizeof(struct reg_access_req)) != 0) {
		err = -EINVAL;
	}

	if (!err) {
		req.data = NPU_READ_REG(req.addr, npu);
		// cppcheck-suppress misra-c2012-11.6
		if (copy_to_user((void *)arg, &req, sizeof(struct reg_access_req)) != 0) {
			err = -EINVAL;
		}
	}

	return err;
}

static long npu_write_reg(struct npu *npu, unsigned long arg)
{
	// cppcheck-suppress misra-c2012-11.6
	const void __user *udata = (void __user *)arg;
	struct reg_access_req req;
	int err;

	err = 0;

	if (copy_from_user(&req, udata, sizeof(struct reg_access_req)) != 0) {
		err = -EINVAL;
	}

	if (!err) {
		NPU_WRITE_REG(req.addr, req.data, npu);
	}

	return err;
}

static long npu_load_network(struct npu *npu, unsigned long arg)
{
	// cppcheck-suppress misra-c2012-11.6
	const void __user *udata = (void __user *)arg;
	struct net_load_req req;

	struct npu_net *net = NULL;
	struct npu_buf *buf;

	struct file *file;

	int mask;
	int fd;

	int ret;
	int err;

	ret = 0;
	err = 0;

	if (copy_from_user(&req, udata, sizeof(struct net_load_req)) != 0) {
		err = -EINVAL;
		npu_drv_err("copy_from_user() failed\n");
	}

	if (!err) {
		net = kzalloc(sizeof(struct npu_net), GFP_KERNEL);
		if (!net) {
			err = -EINVAL;
			npu_drv_err("kzalloc(%ld) failed\n", sizeof(struct npu_net));
		}
	}

	if (!err) {
		// alloc & copy command
		net->cmd_buf = kzalloc(sizeof(struct npu_buf), GFP_KERNEL);
		if (!net->cmd_buf) {
			err = -EINVAL;
			npu_drv_err("kzalloc(%ld) failed\n", sizeof(struct npu_buf));
		}
	}

	if (!err) {
		buf = net->cmd_buf;
		buf->size = npu_page_align(req.cmd_size);
		buf->buf = dma_alloc_coherent(npu->dev,
						buf->size,
						&buf->phys_addr,
						GFP_KERNEL);
		if (!buf->buf) {
			err = -EINVAL;
			npu_drv_err("dma_alloc_coherent(%ld) failed\n", buf->size);
		}
	}

	if (!err) {
		if (copy_from_user(buf->buf, req.cmd_data, req.cmd_size) != 0) {
			err = -EINVAL;
			npu_drv_err("copy_from_user() failed\n");
		}
	}

	if (!err) {
		// flushing cache
		dma_sync_single_for_device(npu->dev,
					buf->phys_addr,
					buf->size,
					DMA_TO_DEVICE);

		// alloc & copy weight
		net->wei_buf = kzalloc(sizeof(struct npu_buf), GFP_KERNEL);
		if (!net->wei_buf) {
			err = -EINVAL;
			npu_drv_err("kzalloc(%ld) failed\n", sizeof(struct npu_buf));
		}
	}

	if (!err) {
		buf = net->wei_buf;
		buf->size = npu_page_align(req.wei_size);
		buf->buf = dma_alloc_coherent(npu->dev,
					buf->size,
					&buf->phys_addr,
					GFP_KERNEL);

		if (!buf->buf) {
			err = -EINVAL;
			npu_drv_err("dma_alloc_coherent() failed\n");
		}
	}

	if (!err) {
		if (copy_from_user(buf->buf, req.wei_data, req.wei_size) != 0) {
			err = -EINVAL;
			npu_drv_err("copy_from_user() failed\n");
		}
	}

	if (!err) {
		// flushing cache
		dma_sync_single_for_device(npu->dev,
					buf->phys_addr,
					buf->size,
					DMA_TO_DEVICE);

		net->npu = npu;
		net->color_format = NPU_COLOR_RGB;

		kref_init(&net->ref_cnt);
		init_waitqueue_head(&net->poll_waitq);

		mask = O_ACCMODE | O_CLOEXEC;
		fd = anon_inode_getfd("npu_network", &npu_net_fops, net, mask);
		if (fd < 0) {
			err = -EINVAL;
			npu_drv_err("anon_inode_getfd() failed\n");
		}
	}

	if (!err) {
		file = fget(fd);
		file->f_mode |= FMODE_LSEEK | FMODE_WRITE | FMODE_READ;
		fput(file);

		ret = fd;
	} else {
		if (net != NULL) {
			if (net->cmd_buf != NULL) {
				buf = net->cmd_buf;

				if (buf->buf != NULL) {
					dma_free_coherent(npu->dev,
							buf->size,
							buf->buf,
							buf->phys_addr);
				}

				kfree(buf);
			}

			if (net->wei_buf != NULL) {
				buf = net->wei_buf;

				if (buf->buf != NULL) {
					dma_free_coherent(npu->dev,
							buf->size,
							buf->buf,
							buf->phys_addr);
				}

				kfree(buf);
			}

			kfree(net);
		}

		ret = err;
	}

	return ret;
}

static long npu_alloc_buffer(struct npu *npu, unsigned long arg)
{
	// cppcheck-suppress misra-c2012-11.6
	const void __user *udata = (void __user *)arg;
	struct buf_alloc_req req;

	struct npu_buf *buf = NULL;
	struct file *file;

	int mask = O_ACCMODE | O_CLOEXEC;
	int fd;

	int ret;
	int err;

	err = 0;
	if (copy_from_user(&req, udata, sizeof(struct buf_alloc_req)) != 0) {
		err = -EINVAL;
		npu_drv_err("copy_from_user() failed\n");
	}

	if (!err) {
		buf = kzalloc(sizeof(struct npu_buf), GFP_KERNEL);
		if (!buf) {
			err = -EINVAL;
			npu_drv_err("kzalloc() failed\n");
		}
	}

	if (!err) {
		kref_init(&buf->ref_cnt);

		buf->size = npu_page_align(req.size);
		buf->buf = dma_alloc_coherent(npu->dev,
						buf->size,
						&buf->phys_addr,
						GFP_KERNEL);
		buf->npu = npu;

		fd = anon_inode_getfd("npu-buffer", &npu_buffer_fops, buf, mask);
		if (fd < 0) {
			err = -EINVAL;
			npu_drv_err("anon_inode_getfd() failed\n");
		}
	}

	if (!err) {
		file = fget(fd);
		file->f_mode |= FMODE_LSEEK | FMODE_WRITE | FMODE_READ;
		fput(file);

		req.addr = buf->phys_addr;
		// cppcheck-suppress misra-c2012-11.6
		if (copy_to_user((void *)arg, &req, sizeof(struct buf_alloc_req)) != 0) {
			err = -EINVAL;
			npu_drv_err("copy_to_user() failed\n");
		}
	}

	if (!err) {
		ret = fd;
	} else {
		if (buf != NULL) {
			if (buf->buf != NULL) {
				dma_free_coherent(npu->dev,
						buf->size,
						buf->buf,
						buf->phys_addr);
			}

			kfree(buf);
		}
		ret = err;
	}

	return ret;
}

/*
   copy_from/to_user() is to copy block data between user and kernel.
   And user and kernel memory space are independent and
   implemented in separate address spaces.
*/
static long npu_read_ecc(struct npu *npu, unsigned long arg)
{
	// cppcheck-suppress misra-c2012-11.6
	int err;

	ecc_wdt_access_req_t *ecc_npu = &npu->ecc_wdt_status;

	// cppcheck-suppress misra-c2012-11.6
	if (copy_to_user((void *)arg, ecc_npu, sizeof(ecc_wdt_access_req_t)) != 0) {
		err = -EINVAL;
	}
	else {
		err = 0;
	}

	return err;
}

static long npu_init_mlx_firmware(struct npu *npu)
{
	int err;
	const struct firmware *fp = NULL;
	struct npu_buf *buf = NULL;
	const int kernel_size = MAX_MLX_KERNEL_SIZE;
	const char *name = mlx_kernel_fnames[npu->mlx_bin_idx];	

	npu_drv_dbg("%s loaded\n", name);

	err = 0;

	if (!err) {
		buf = kzalloc(sizeof(struct npu_buf), GFP_KERNEL);
		if (!buf) {
			err = -EINVAL;
			npu_drv_err("kzalloc(%ld) failed\n", sizeof(struct npu_buf));
		}
	}

	if (!err) {
		buf->size = kernel_size;
		buf->buf = dma_alloc_coherent(npu->dev,
						buf->size,
						&buf->phys_addr,
						GFP_DMA32);

		if (!buf->buf) {
			err = -EINVAL;
			npu_drv_err("dma_alloc_coherent(%ld) failed\n", buf->size);
		}
	}

	if (!err) {
		err = request_firmware_into_buf(&fp, name, npu->dev, buf->buf, buf->size);
	}

	if (!err) {
		// flushing cache
		dma_sync_single_for_device(npu->dev,
					buf->phys_addr,
					buf->size,
					DMA_TO_DEVICE);

		if (mlx_load_kernel(npu, buf) < 0) {
			err = -EINVAL;
			npu_drv_err("mlx_load_kernel() failed\n");
		}
	}

	if (buf != NULL) {
		if (buf->buf != NULL) {
			dma_free_coherent(npu->dev,
					buf->size,
					buf->buf,
					buf->phys_addr);
		}
		kfree(buf);
	}

	return err;
}

static int npu_reset(struct npu *npu, int hard)
{
	if (!hard) {
		npu_soft_reset(npu);
	} else {
		npu_hard_reset(npu);
	}

	return 0;
}

static long npu_init_npu(struct npu *npu, unsigned long arg)
{
	// cppcheck-suppress misra-c2012-11.6
	const void __user *udata = (void __user *)arg;
	struct npu_init_req req;
	int ret = 0;

	if (copy_from_user(&req, udata, sizeof(struct npu_init_req)) != 0) {
		ret = -EINVAL;
		npu_drv_err("copy_from_user() failed\n");
	}

	npu->disable_ue_fail = req.disable_ue_fail;
	npu->disable_ce_fail = req.disable_ce_fail;
	npu->disable_wdt    = req.disable_wdt;

	if ((req.dev_wdt_ext_cnt > 0U) && (req.dev_wdt_int_cnt > 0U)) {
		npu->wdt_ext_cnt = req.dev_wdt_ext_cnt;
		npu->wdt_int_cnt = req.dev_wdt_int_cnt;
	}

	if (!req.dev_ecc_test_ctrl) {
		npu->ecc_test_ctrl = 0xF1U;
	}
	else {
		npu->ecc_test_ctrl = req.dev_ecc_test_ctrl;
	}
	npu->mlx_err_inj_mask_data = req.dev_mlx_err_inj_mask_data;
	npu->mlx_err_inj_mask_par  = req.dev_mlx_err_inj_mask_par;

	npu->dbg_print_reg = req.soft_reset & 0x2U;

	if (req.dev_mlx_bin_idx < MAX_MLX_FILES_NUM) {
		npu->mlx_bin_idx = req.dev_mlx_bin_idx;
	}
	else {
		ret = -EINVAL;
		npu_drv_err("%d dev_mlx_bin_idx err\n", req.dev_mlx_bin_idx);
	}

	npu_init_ecc_status(npu);

	if (!ret) {
		ret = npu_reset(npu, !(req.soft_reset& 0x1U));
		if (ret != 0) {
			npu_drv_err("npu_reset failed\n");
		}
	}

	if (!ret) {
		ret = npu_enable_ecc(npu);
		if (ret != 0) {
			npu_drv_err("npu_enable_ecc failed\n");
		}
	}

	if (!ret) {
		ret = npu_init_mlx_firmware(npu);
		if (ret != 0) {
			npu_drv_err("npu_init_mlx_firmware failed\n");
		}
	}

	if (!ret) {
		ret = npu_disable_wdt(npu);
		if (ret != 0) {
			npu_drv_err("npu_disable_wdt failed\n");
		}
	}

	// Read ECC status regs and then clear ECC status regs
	npu_read_ecc_regs(npu);

	return ret;
}

static long npu_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct npu *npu = file->private_data;
	int ret;

	switch (cmd) {
	case NPU_IOCTL_ALLOC_BUFFER:
		ret = npu_alloc_buffer(npu, arg);
		break;

	case NPU_IOCTL_LOAD_NETWORK:
		ret = npu_load_network(npu, arg);
		break;

	case NPU_IOCTL_READ_REG:
		ret = npu_read_reg(npu, arg);
		break;

	case NPU_IOCTL_WRITE_REG:
		ret = npu_write_reg(npu, arg);
		break;

	case NPU_IOCTL_RESET_NPU:
		ret = npu_init_npu(npu, arg);
		break;

	case NPU_IOCTL_READ_ECC:
		ret = npu_read_ecc(npu, arg);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = npu_open,
	.unlocked_ioctl = npu_ioctl,
	.release = npu_release,
};

#ifdef NDOLPHIN_ENV
//static int ndolphin_set_voltage(struct device *dev);
static int ndolphin_set_clk(struct npu *npu, struct device *dev);
#endif

static dev_t stDev;
static int npu_device_probe(struct platform_device *pdev)
{
	struct device *dev;
	struct npu *npu;
	struct device_node *node;

	struct npu_buf *work_buf;
	int work_buf_size;

	int ret = 0;
	int iomap_err = 0;
	int size = 0;
	int ida = 0;

	ida = ida_simple_get(&openedge_ida, 0, 2, GFP_KERNEL);

	npu = &devs[ida];
	dev = &pdev->dev;
	node = dev->of_node;

	if (!npu) {
		dev_err(dev, "fail in devm_kzalloc\n");
		iomap_err = -ENOMEM;
		ret = iomap_err;
	}

	if (!iomap_err) {
		npu->dev = dev;
		npu->membase = devm_of_iomap(&pdev->dev, node, 0, NULL);

		if (!npu->membase) {
			iomap_err = -EINVAL;
		}

		ret = iomap_err;
	}

	if (!iomap_err) {
		int err;

		npu->id = ida;
		npu->irq = platform_get_irq(pdev, 0);

		err = request_irq(npu->irq,
					npu_interrupt_handler,
					IRQF_TRIGGER_RISING,
					"npu",
					npu);

		if (err != 0) {
			dev_err(dev, "request irq failed: %d\n", err);
		}

		if (!err) {
			init_waitqueue_head(&npu->irq_waitq);

			if (!of_property_read_u32(node, "work-buffer-size", &size)) {
				if (!of_reserved_mem_device_init(dev)) {
					npu_drv_dbg("using reserved memory\n");
				} else {
					npu_drv_dbg("using cma-default\n");
				}

				dma_set_mask(dev, DMA_BIT_MASK(64));

				work_buf = kzalloc(sizeof(struct npu_buf), GFP_KERNEL);
				if (!work_buf) {
					err = -EINVAL;
					npu_drv_err("kzalloc(%ld) failed\n", sizeof(struct npu_buf));
				}
				work_buf_size = npu_page_align(size);

				work_buf->buf = dma_alloc_coherent(dev,
								work_buf_size,
								&work_buf->phys_addr,
								GFP_KERNEL);

				if (!work_buf->buf) {
					err = -EINVAL;
					npu_drv_err("dma_alloc_coherent(%d) failed\n", work_buf_size);
				}

				work_buf->size = work_buf_size;

				npu->work = work_buf;

			} else {
				dev_err(dev, "[enlight] work-buffer-size not found\n");
				err = -EINVAL;
			}
		}

#if 0
		if (!err) {
			err = ndolphin_set_voltage(dev);
		}
#endif

		if (!err) {
			npu->status = NPU_READY;

			stDev = MKDEV(NPU_MAJOR, ida);
			dev_set_drvdata(dev, npu);
			cdev_init(&devs[ida].cdev, &fops);

			err = cdev_add(&devs[ida].cdev, stDev, 1);
			if (err < 0) {
				dev_err(dev, "[enlight] failed to add npu%d\n", ida);
			}
		}

		if (!err) {
			device_create(&npu_class, NULL, stDev, NULL, "npu%d", ida);

			mutex_init(&npu->lock);

			INIT_WORK(&npu->sched.work, npu_work_func);
			INIT_LIST_HEAD(&npu->sched.queue);
			mutex_init(&npu->sched.lock);
		}

#ifdef NDOLPHIN_ENV
		if (!err) {
			static int clk_init;
			if (clk_init == 0) {
				clk_init = 1;

				err = ndolphin_set_clk(npu, dev);
			}
		}
#endif
		if (err != 0) {
			free_irq(npu->irq, npu);
		}

		ret = err;
	}

	if (!ret) {
		npu_drv_info("DRIVER VERSION v2.2X D\n");
		npu_drv_info("NPU initialized..\n");
	}

	return ret;
}

#if 0
static int ndolphin_set_voltage(struct device *dev)
{
	int err;
	struct regulator *regulator;
	int req_vol;

	struct device_node *node;

	node = dev->of_node;

	regulator = devm_regulator_get_optional(dev, "npu");
	if (IS_ERR(regulator) != 0U) {
		dev_err(dev, "Fail to get regulator");
		err = PTR_ERR(regulator);
	} else {
		int ret;
		ret = of_property_read_u32(node, "voltage", &req_vol);
		if (ret < 0) {
			dev_warn(dev, "Not set voltage\n");
		} else {
			ret = regulator_set_voltage(regulator, req_vol, req_vol);
			if (ret != 0) {
				dev_warn(dev, "Can't set voltage\n");
			}
		}

		err = 0;
	}

	return err;
}
#endif

#ifdef NDOLPHIN_ENV
static int ndolphin_set_clk(struct npu *npu, struct device *dev)
{
	unsigned int clk_rate = 0;
	int err;
	int ret;
	struct device_node *node;

	node = dev->of_node;

	err = 0;
	npu->npu_aclk = of_clk_get_by_name(node, "npubus_aclk");

	ret = IS_ERR(npu->npu_aclk);
	if (ret != 0) {
		err = -EINVAL;
	}

	if (!err) {
		ret = of_property_read_u32(node, "aclk-freq", &clk_rate);

		if (ret != 0) {
			clk_rate = 800000000;
			dev_warn(dev, "[WARN][NPU] Set aclk to default(%d Hz)\n",
				clk_rate);
		}

		(void)clk_prepare_enable(npu->npu_aclk);
		(void)clk_set_rate(npu->npu_aclk, clk_rate);
		clk_rate = clk_get_rate(npu->npu_aclk);
		dev_dbg(dev, "[DEBUG][NPU] Set aclk to %d Hz\n", clk_rate);
	}

	if (!err) {
		npu->npu_pclk = of_clk_get_by_name(node, "npubus_pclk");
		ret = IS_ERR(npu->npu_pclk);
		if (ret != 0) {
			err = -EINVAL;
		}
	}

	if (!err) {
		ret = of_property_read_u32(node, "pclk-freq", &clk_rate);
		if (ret != 0) {
			clk_rate = 200000000;
			dev_warn(dev, "[WARN][NPU] Set aclk to default(%d Hz)\n",
				clk_rate);
		}
		(void)clk_prepare_enable(npu->npu_pclk);
		(void)clk_set_rate(npu->npu_pclk, clk_rate);
		clk_rate = clk_get_rate(npu->npu_pclk);
		dev_dbg(dev, "[DEBUG][NPU] Set pclk to %d Hz\n", clk_rate);
	}

	if (!err) {
		npu->npu_cclk = of_clk_get_by_name(node, "npubus_cclk");
		ret = IS_ERR(npu->npu_cclk);
		if (ret != 0) {
			err = -EINVAL;
		}
	}

	if (!err) {
		ret = of_property_read_u32(node, "cclk-freq", &clk_rate);
		if (ret != 0) {
			clk_rate = 1200000000;
			dev_warn(dev, "[WARN][NPU] Set cclk to default(%d Hz)\n",
					clk_rate);
		}
		(void)clk_prepare_enable(npu->npu_cclk);
		(void)clk_set_rate(npu->npu_cclk, clk_rate);
		clk_rate = clk_get_rate(npu->npu_cclk);
		dev_dbg(dev, "[DEBUG][NPU] Set cclk to %d Hz\n", clk_rate);
	}

	if (!err) {
		npu->npu_cpuclk = of_clk_get_by_name(node, "npubus_cpu");
		ret = IS_ERR(npu->npu_cpuclk);
		if (ret != 0) {
			err = -EINVAL;
		}
	}

	if (!err) {
		ret = of_property_read_u32(node, "cpuclk-freq", &clk_rate);
		if (ret != 0) {
			clk_rate = 800000000;
			dev_warn(dev, "[WARN][NPU] Set cpuclk to default(%d Hz)\n",
				clk_rate);
		}
		(void)clk_prepare_enable(npu->npu_cpuclk);
		(void)clk_set_rate(npu->npu_cpuclk, clk_rate);
		clk_rate = clk_get_rate(npu->npu_cpuclk);
		dev_dbg(dev, "[DEBUG][NPU] Set cpuclk to %d Hz\n", clk_rate);
	}

	return err;
}
#endif

static int npu_device_remove(struct platform_device *pdev)
{
	struct device *dev;
	struct npu *npu;
	struct npu_buf *work_buf;

	BUG_ON(!pdev);

	dev = &pdev->dev;
	npu = dev_get_drvdata(dev);
	work_buf = npu->work;

	iounmap(npu->membase);
	free_irq(npu->irq, npu);

	dma_free_coherent(dev,
			work_buf->size,
			work_buf->buf,
			work_buf->phys_addr);

	kfree(work_buf);

	NPU_WRITE_REG(ADDR_NPU_IRQ_MASK, 0, npu);
	NPU_WRITE_REG(ADDR_NPU_IRQ_ENABLE, 0, npu);

	npu_drv_info("NPU de-initialized\n");

	return 0;
}


#ifdef CONFIG_OF
static const struct of_device_id npu_match[] = {
	{
		.compatible = "openedge,npu",
	}
};
MODULE_DEVICE_TABLE(of, npu_match);
#endif

static struct platform_driver npu_driver = {
	.probe = npu_device_probe,
	.remove = npu_device_remove,
	.driver = {
		.name  = "openedges-npu",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(npu_match),
	},
};

static int __init npu_init(void)
{
	int ret;

	ret = class_register(&npu_class);

	if (ret != 0) {
		npu_drv_err("error(%d) class register\n", ret);
	} else {
		ret = platform_driver_register(&npu_driver);
		if (ret != 0) {
			npu_drv_err("error(%d) in platform_driver_register\n", ret);
		}
	}

	if (ret != 0) {
		ret = -EINVAL;
	}

	return ret;
}

static void npu_exit(void)
{
	platform_driver_unregister(&npu_driver);
	device_destroy(&npu_class, devs[0].cdev.dev);
	cdev_del(&devs[0].cdev);
	class_unregister(&npu_class);
};

module_init(npu_init);
module_exit(npu_exit);

static void npu_read_ecc_regs(struct npu *npu)
{
	unsigned int i;
	unsigned int data;
	unsigned int base;
	unsigned int num_core;

	ecc_wdt_access_req_t* ecc_npu;
	ecc_cbuf_t* ecc_cbuf;
	ecc_gbuf_t* ecc_gbuf;
	ecc_sram_t* ecc_sram;

	data = NPU_READ_REG(ADDR_NPU_ID_CODE, npu);
	num_core = (data >> 24) & 0xfU;
	num_core += 1U;

	ecc_npu = &npu->ecc_wdt_status;
	ecc_npu->irq_reason = NPU_READ_REG(ADDR_NPU_IRQ_REASON, npu);

	//CBuf
	ecc_cbuf = &ecc_npu->cbuf;
	data = NPU_READ_REG(ADDR_NPU_ECC_CBUF_ECC_CNT, npu);
	ecc_cbuf->ue_cnt += (data >> 0) & 0xFFU;
	ecc_cbuf->ce_cnt += (data >> 8) & 0xFFU;

	//GBuf
	for (i = 0; i < num_core; i++) {
		ecc_gbuf = &ecc_npu->gbuf[i];
		base = ADDR_NPU_ECC_GBUF_UE_CNT_C0 + (i * 0x4U);
		data = NPU_READ_REG(base , npu);
		ecc_gbuf->ue_cnt += data;

		base = ADDR_NPU_ECC_GBUF_CE_CNT_C0 + (i * 0x4U);
		data = NPU_READ_REG(base , npu);
		ecc_gbuf->ce_cnt += data;
	}

	//MLX SRAM
	for (i = 0; i < num_core; i++) {
		ecc_sram = &ecc_npu->sram[i];

		base = ADDR_NPU_MLX_C0_ECC_CTRL + (i * 0x40U);
		data = NPU_READ_REG(base, npu);
		ecc_sram->ue_status = (data >> 9) & 0x1U;
		ecc_sram->ce_status = (data >> 8) & 0x1U;

		base = ADDR_NPU_MLX_C0_ECC_CNT + (i * 0x40U);
		data = NPU_READ_REG(base, npu);
		ecc_sram->ue_cnt += (data >> 16) & 0xFFU;
		ecc_sram->ce_cnt += (data >>  0) & 0xFFU;

		base = ADDR_NPU_MLX_CO_ECC_CE_ADDR + (i * 0x40U);
		ecc_sram->ce_addr = NPU_READ_REG(base, npu);

		base = ADDR_NPU_MLX_CO_ECC_CE_DATA + (i * 0x40U);
		ecc_sram->ce_data = NPU_READ_REG(base, npu);

		base = ADDR_NPU_MLX_CO_ECC_UE_ADDR + (i * 0x40U);
		ecc_sram->ue_addr = NPU_READ_REG(base, npu);

		base = ADDR_NPU_MLX_CO_ECC_UE_DATA + (i * 0x40U);
		ecc_sram->ue_data = NPU_READ_REG(base, npu);

		//clear cnt
		base = ADDR_NPU_MLX_C0_ECC_CNT + (i * 0x40U);
		NPU_WRITE_REG(base, 0, npu);
	}
}

/** @brief return ECC status
 *  @param[in]	npu       npu handler
 *  @param[in]	disable_ecc disable to return status
 *  @return     non-zero if error detected
 */
static int npu_get_ecc_wdt_status(struct npu *npu)
{
	unsigned int i;
	unsigned int ue_cnt;
	unsigned int ce_cnt;
	unsigned int wdt_err;
	unsigned int base;
	unsigned int data;
	int ret;
	unsigned int num_core;

	ecc_wdt_access_req_t *ecc_npu;
	ecc_sram_t *ecc_sram;
	ecc_gbuf_t *ecc_gbuf;
	ecc_cbuf_t *ecc_cbuf;

	npu_drv_fin(0);

	data = NPU_READ_REG(ADDR_NPU_ID_CODE, npu);
	num_core = (data >> 24) & 0xfU;
	num_core += 1U;

	//ECC
	ue_cnt = 0;
	ce_cnt = 0;
	wdt_err = 0;

	ecc_npu = &npu->ecc_wdt_status;

	ecc_cbuf = &ecc_npu->cbuf;
	if (!(npu->disable_ue_fail & 0x4U)) {
		ue_cnt += ecc_cbuf->ue_cnt;
	}
	if (!(npu->disable_ce_fail & 0x4U)) {
		ce_cnt += ecc_cbuf->ce_cnt;
	}

	for (i = 0; i < num_core; i++) {
		ecc_gbuf = &ecc_npu->gbuf[i];
		if (!(npu->disable_ue_fail & 0x2U)) {
			ue_cnt += ecc_gbuf->ue_cnt;
		}
		if (!(npu->disable_ce_fail & 0x2U)) {
			ce_cnt += ecc_gbuf->ce_cnt;
		}
	}

	for (i = 0; i < num_core; i++) {
		ecc_sram = &ecc_npu->sram[i];
		if (!(npu->disable_ue_fail & 0x1U)) {
			ue_cnt += ecc_sram->ue_cnt;
		}
		if (!(npu->disable_ce_fail & 0x1U)) {
			ce_cnt += ecc_sram->ce_cnt;
		}
	}

	ret = NPU_ERR_FLAG_NONE;

	if (ue_cnt > 0U) {
		ret |= NPU_ERR_FLAG_UE;
	}

	if (ce_cnt > 0U) {
		ret |= NPU_ERR_FLAG_CE;
	}

	ecc_npu->wdt_to = 0U;

	//WDT
	if (!npu->disable_wdt) {
		for (i = 0; i < num_core; i++) {
			base = ADDR_NPU_MLX_C0_HCI_00 + (i * 0x40U);
			data = NPU_READ_REG(base, npu);

			if ((data & 0x02000000U) != 0U) {
				ret |= NPU_ERR_FLAG_WDT;
				ecc_npu->wdt_to = 1U;
			}
		}
	}

	npu_drv_fout(0);

	return ret;
}

/** @brief initialize ECC status
 *  @param[in]	npu  npu handler
 */
static void npu_init_ecc_status(struct npu *npu)
{
	unsigned int i;

	ecc_sram_t *sram;
	ecc_gbuf_t *gbuf;
	ecc_cbuf_t *cbuf;

	ecc_wdt_access_req_t *ecc_npu = &npu->ecc_wdt_status;

	npu_drv_fin(0);

	ecc_npu->wdt_to = 0;

	for (i = 0; i < MAX_NUM_NPU_CORE; i++) {
		sram = &ecc_npu->sram[i];
		sram->ue_irq_flag = 0;
		sram->ce_irq_flag = 0;
		sram->ue_cnt = 0;
		sram->ce_cnt = 0;
		sram->ce_addr = 0;
		sram->ce_data = 0;
		sram->ue_addr = 0;
		sram->ue_data = 0;
	}

	for (i = 0; i < MAX_NUM_NPU_CORE; i++) {
		gbuf = &ecc_npu->gbuf[i];
		gbuf->ue_irq_flag = 0;
		gbuf->ce_irq_flag = 0;
		gbuf->ue_cnt = 0;
		gbuf->ce_cnt = 0;
	}

	cbuf = &ecc_npu->cbuf;
	cbuf->ue_irq_flag = 0;
	cbuf->ce_irq_flag = 0;
	cbuf->ue_cnt = 0;
	cbuf->ce_cnt = 0;

	npu_drv_fout(0);
}

static int npu_enable_ecc(struct npu *npu)
{
	unsigned int i;
	unsigned int base;
	unsigned int data;
	unsigned int num_core;

	data = NPU_READ_REG(ADDR_NPU_ID_CODE, npu);
	num_core = (data >> 24) & 0xfU;
	num_core += 1U;

	NPU_WRITE_REG(ADDR_NPU_ECC_CTRL, EN_ECC_GBUF_CBUF, npu);

	data = npu->ecc_test_ctrl;
	NPU_WRITE_REG(ADDR_NPU_ECC_TEST_CTRL, data, npu);

	for (i = 0; i < num_core; i++) {
		base = ADDR_NPU_MLX_C0_ECC_CTRL + (i * 0x40U);
		NPU_WRITE_REG(base, 0x30001U, npu);
	
		base = ADDR_NPU_MLX_C0_ECC_CNT + (i * 0x40U);
		NPU_WRITE_REG(base, 0x0U, npu);
	
		base = ADDR_NPU_MLX_C0_ECC_ERR_INJ_MASK_DAT + (i * 0x40U);
		data = npu->mlx_err_inj_mask_data;
		NPU_WRITE_REG(base, data, npu);
		base = ADDR_NPU_MLX_C0_ECC_ERR_INJ_MASK_PAR + (i * 0x40U);
		data = npu->mlx_err_inj_mask_par;
		NPU_WRITE_REG(base, data, npu);
	}

	return 0;
}

static int npu_enable_wdt(struct npu *npu)
{
	unsigned int i;

	unsigned int data;
	unsigned int ext_cnt;
	unsigned int int_cnt;
	unsigned int wdt_dis;
	int ret = 0;
	unsigned int num_core;

	data = NPU_READ_REG(ADDR_NPU_ID_CODE, npu);
	num_core = (data >> 24) & 0xfU;
	num_core += 1U;

	wdt_dis = npu->disable_wdt;
	if (!wdt_dis) {
		ext_cnt = npu->wdt_ext_cnt;
		int_cnt = npu->wdt_int_cnt;
	}
	else {
		ext_cnt = 0xFFFFFFFFU;
		int_cnt = 0x0U;
	}

	for (i = 0; i < num_core; i++){
		int timeout_cnt = 1000;
		unsigned int base;
		unsigned int data;

		base = ADDR_NPU_MLX_C0_HCI_00 + (i * 0x40U);
		NPU_WRITE_REG(base + 0x4U, 0xCD09, npu);
		NPU_WRITE_REG(base + 0x8U, ext_cnt, npu);
		NPU_WRITE_REG(base + 0xCU, int_cnt, npu);

		// [24] WDTEN, [8] INTDIS, [0] run
		if (!wdt_dis) {
			NPU_WRITE_REG(base, 0x00000101, npu);
		}
		else {
			NPU_WRITE_REG(base, 0x01000101, npu);
		}

		ret = -ETIMEDOUT;

		do {
			data = NPU_READ_REG(base, npu);
			if (!(data & 0x10U)) {
				ret = 0;
				break;
			}
			
			mdelay(1);
		} while(--timeout_cnt > 0);

		if (ret != 0) {
			npu_drv_err("WDT[%d] activate timeout!\n", i);
		}
	}

	return ret;
}

static int npu_disable_wdt(struct npu *npu)
{
	unsigned int i;
	unsigned int base;
	unsigned int data;
	unsigned int num_core;
	int timeout_cnt = 1000;
	int ret = 0;

	data = NPU_READ_REG(ADDR_NPU_ID_CODE, npu);
	num_core = (data >> 24) & 0xfU;
	num_core += 1U;

	for (i = 0; i < num_core; i++){

		base = ADDR_NPU_MLX_C0_HCI_00 + (i * 0x40U);
	
		NPU_WRITE_REG(base + 0x4U, 0xCD09U, npu);
		NPU_WRITE_REG(base + 0x8U, 0xFFFFFFFFU, npu);
		NPU_WRITE_REG(base + 0xCU, 0x0U, npu);

		// [24] WDTEN, [8] INTDIS, [0] run
		data = 0x00000101;
		NPU_WRITE_REG(base, data, npu);

		ret = -ETIMEDOUT;
		do {
			data = NPU_READ_REG(base, npu);
			
			if (!(data & 0x10U)) {
				ret = 0;
				break;
			}
			
			mdelay(1);
			
		} while(--timeout_cnt > 0);

		if (ret != 0) {
			npu_drv_err("WDT[%d] disable timeout!\n", i);
		}
	}

	return ret;
}
