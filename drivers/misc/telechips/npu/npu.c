// SPDX-License-Identifier: GPL-2.0-only
/*
 * npu.c - telechips npu driver
 *
 * Copyright (C) 2020 Telechips
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

#define DRIVER_DATE "20250114"
#define DRIVER_MAJOR 1
#define DRIVER_MINOR 0
#define DRIVER_PATCH 0

#define MAX_MLX_KERNEL_SIZE (32768U)
#define MAX_MLX_FILES_NUM   (8U)

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

#define NPU_OPCODE_WAIT     (0x0U)
#define NPU_OPCODE_RUN      (0x1U)
#define NPU_OPCODE_WR_REG   (0x2U)
#define NPU_OPCODE_TRAP     (0x3U)

#define IRQ_CBUF_ECC        (0x00300000U)
#define IRQ_GBUF_ECC        (0x000FF000U)
#define IRQ_MLX_ECC         (0x00000F00U)
#define IRQ_MLX             (0x000000F0U)
#define IRQ_TRAP            (0x00000004U)
#define IRQ_ECC             (0x003FFF00U)
#define IRQ_ALL             (0x003FFFF4U)

#define MASK_CORE0_BUSY     (0xF0000000U)
#define MASK_CORE1_BUSY     (0x0F000000U)
#define MASK_CORE2_BUSY     (0x00F00000U)
#define MASK_CORE3_BUSY     (0x000F0000U)
#define MASK_DMA_BUSY       (0x00000100U)

#define DRAM2CMD            (0x2U)
#define DRAM2MLX            (0x6U)

#define npu_align(x, g)     ((((x)+(g)-1U) & (~((g)-1U))))
#define npu_page_align(x)   npu_align(x, PAGE_SIZE)

/*
#define npu_drv_err(fmt, args...)  (void)pr_err("[EnDrv]%4d:%s:" fmt, __LINE__, __func__, ##args);
#define npu_drv_info(fmt, args...) (void)pr_info("[EnDrv]%4d:%s:" fmt, __LINE__, __func__, ##args);
#define npu_drv_dbg(fmt, args...)  (void)pr_info("[EnDrv Dbg]%4d:%s():" fmt, __LINE__, __func__, ##args);
*/

#define npu_drv_fin(val)  (void)pr_info("[EnDrv fin]%4d:%s():0x%x\n", __LINE__, __func__, val);
#define npu_drv_fout(val) (void)pr_info("[EnDrv fout]%4d:%s():0x%x\n", __LINE__, __func__, val);


MODULE_LICENSE("GPL v2");
MODULE_VERSION(__stringify(DRIVER_MAJOR) "."
               __stringify(DRIVER_MINOR) "."
               __stringify(DRIVER_PATCH));
MODULE_DESCRIPTION("ENLIGHT NPU driver");

static struct class npu_class = {
	.name = "npu",
};

static DEFINE_IDA(telechips_ida);

typedef struct oe_npu       oe_npu_t;
typedef struct oe_npu_buf   oe_npu_buf_t;
typedef struct oe_npu_net   oe_npu_net_t;
typedef struct oe_npu_cmd   oe_npu_cmd_t;
typedef struct oe_npu_sched oe_npu_sched_t;

struct oe_npu_sched {
	struct work_struct work;
	struct list_head queue;
	struct mutex lock;
};

struct oe_npu {
	int status;

	struct cdev cdev_inst;
	struct device *dev;

	unsigned int id;

	int irq;
	wait_queue_head_t irq_waitq;
	atomic_t irq_done;

	void __iomem *dev_iomap_base;
	oe_npu_buf_t *work;

	struct mutex lock;
	oe_npu_sched_t sched;
	
	npu_err_bits_t disable_fail;

	unsigned int mlx_bin_idx;

	unsigned int wdt_ext_cnt;
	unsigned int wdt_int_cnt;

	unsigned int ecc_test_ctrl;
	unsigned int mlx_err_inj_mask_data;
	unsigned int mlx_err_inj_mask_par;

	unsigned int dbg_print_reg;

	struct kref ref_cnt;

	atomic_t is_initialized;
};

struct oe_npu_buf {
	oe_npu_t *npu;
	void *buf;

	dma_addr_t phys_addr;

	unsigned long phys_start;
	unsigned long size;

	struct kref ref_cnt;
};

struct oe_npu_net {
	int status;
	unsigned int color_format;
	unsigned int elapsed_in_us;

	unsigned int err_stat;

	oe_npu_t *npu;
	oe_npu_buf_t *cmd_buf;
	oe_npu_buf_t *wei_buf;

	npu_err_bits_t* user_status;

	wait_queue_head_t poll_waitq;

	struct kref ref_cnt;
};


struct oe_npu_cmd {
	oe_npu_net_t *npu_net;

	oe_npu_buf_t *in;
	oe_npu_buf_t *out;

	npu_err_bits_t status;

	struct list_head list;
};


static oe_npu_t devs[NPU_MAX_MINORS];

static npu_err_rd_req_t npu_err_stat;


static oe_npu_buf_t *buf_alloc(oe_npu_t *npu, unsigned long size);
static void buf_free(const oe_npu_buf_t *npu_buf);

static void net_free(const oe_npu_net_t *npu_net);

static int npu_init_mlx_firmware(oe_npu_t *npu);

static void npu_free(struct kref *ref);
static void npu_net_free(struct kref *ref);
static void npu_buffer_free(struct kref *ref);

static unsigned int npu_read_ecc_wdt(oe_npu_t *npu);

static void npu_reset(oe_npu_t *npu, int hard);
static int npu_enable_wdt(oe_npu_t *npu);
static int npu_disable_wdt(oe_npu_t *npu);
static void npu_enable_ecc(oe_npu_t *npu);


static inline oe_npu_buf_t *npu_buffer_get(unsigned int npu_buf_fd)
{
	struct file *fp = fget(npu_buf_fd);
	oe_npu_buf_t *npu_buf = (oe_npu_buf_t *)fp->private_data;
	kref_get(&npu_buf->ref_cnt);
	fput(fp);

	return npu_buf;
}

static inline void npu_buffer_put(oe_npu_buf_t *npu_buf)
{
	(void)kref_put(&npu_buf->ref_cnt, npu_buffer_free);
}

static inline void NPU_WRITE_REG(const unsigned int addr, unsigned int data, const oe_npu_t *npu)
{
	void __iomem *reg_addr = (void __iomem *)((uintptr_t)npu->dev_iomap_base + addr);

	if (npu->dbg_print_reg != 0U) {
		(void)pr_info("[NPU-W]0x%08x<=0x%08x\n", addr, data);
	}

	writel_relaxed(data, reg_addr);
}

static inline unsigned int NPU_READ_REG(const unsigned int addr, const oe_npu_t *npu)
{
	unsigned int data;
	void __iomem *reg_addr = (void __iomem *)((uintptr_t)npu->dev_iomap_base + addr);

	data = readl_relaxed(reg_addr);

	if (npu->dbg_print_reg != 0U) {
		(void)pr_info("[NPU-R]0x%08x=>0x%08x\n", addr, data);
	}

	return data;
}

static int npu_stop(oe_npu_t *npu)
{
	int i;
	int ret;

	//npu_drv_fin(0);

	// stop fetching command
	NPU_WRITE_REG(ADDR_NPU_CONTROL, 0, npu);

	// wait DMA_IDLE
	ret = -ETIMEDOUT;
	for (i = 0; i < NPU_WAIT_DMA_BUSY_CNT; i++) {
		if ((NPU_READ_REG(ADDR_NPU_STATUS, npu) & MASK_DMA_BUSY) == 0U) {
			ret = 0;
			break;
		}

		mdelay(1);
	}

	(void)pr_err("NPU DMA waiting timeout\n");

	//npu_drv_fout(0);

	return ret;
}

static int npu_wait_interrupt(oe_npu_t *npu, u32 timeout)
{
	atomic_t *irq_done = &npu->irq_done;
	int ret;

	//npu_drv_fin(0);

	if (wait_event_timeout(
		npu->irq_waitq,
		atomic_add_unless(irq_done, -1, 0),
		msecs_to_jiffies(timeout)) > 0L) {
		ret = 0;
	}
	else {
		(void)npu_stop(npu);
		ret = -ETIMEDOUT;
	}

	//npu_drv_fout(ret);

	return ret;
}

/** @brief NPU interrupt handler
 *  NPU_IRQ_REASON
 *  [31:24] trap_id
 *  [23:22] reserved
 *  [   21] irq_cmd_buf_ecc_ce
 *  [   20] irq_cmd_buf_ecc_ue
 *  [19:16] irq_gbuf_ecc_ce_c3:c0
 *  [15:12] irq_gbuf_ecc_ue_c3:c0
 *  [11: 8] irq_mlx_ecc_c3:c0
 *  [ 7: 4] irq_mlx_c3:c0
 *  [    2] irq_trap
 *  [    0] irq_q_emtpy
 */
static irqreturn_t npu_interrupt_handler(int irq, void *handle)
{
	oe_npu_t *npu = (oe_npu_t *)handle;
	unsigned int reason;

	reason = NPU_READ_REG(ADDR_NPU_IRQ_REASON, npu);
	reason = reason & 0x00FFFFFFU;

	if ((reason & IRQ_TRAP) != 0U) {
		NPU_WRITE_REG(ADDR_NPU_IRQ_REASON, IRQ_TRAP, npu);
		//0x2U = SW_RESET
		NPU_WRITE_REG(ADDR_NPU_CONTROL, 0x2U, npu);
	}
	else {
		(void)pr_err("%08x: Unknown IRQ_REASON\n", reason);
		NPU_WRITE_REG(ADDR_NPU_IRQ_REASON, reason, npu);
	}

	NPU_WRITE_REG(ADDR_NPU_IRQ_CLEAR, 1, npu);

	atomic_set(&npu->irq_done, 1);

	wake_up(&npu->irq_waitq);

	//(void)pr_info("%08x: IRQ reason\n", reason);

	return IRQ_HANDLED;
}

/** @brief npu_set_color_format parameter description
 * NPU_COLOR_YUV
 *        R_CB   0/128      R_Y  128/128     R_CR 197/128
 * conv0  = (0x000U << 20) + (0x080U << 10) + (0x0c5U << 0);
 *        G_CB -23/128      G_Ya 128/128     G_CR -59/128
 * conv1  = (0x3e9U << 20) + (0x080U << 10) + (0x3c5U << 0);
 *        B_CB  232/128 B_Y   128/128 B_CR   0/128
 * conv2  = (0x0e8U << 20) + (0x080U << 10) + (0x000U << 0);
 *        R_BIAS - 128 : -325 G_BIAS - 128 :  -46 B_BIAS - 128 : -360
 * bias   = (0x2bbU << 20) + (0x3d2U << 10) + (0x298U << 0);
 *
 * NPU_COLOR_RGB
 *        R 128/128        G 0/128          B 0/128
 * conv0 = (0x080U << 20) + (0x000U << 10) + (0x000U << 0);
 *        R 0/128          G 128/128        B 0/128
 * conv1 = (0x000U << 20) + (0x080U << 10) + (0x000U << 0);
 *        R 0/128          G 0/128          B 128/128
 * conv2 = (0x000U << 20) + (0x000U << 10) + (0x080U << 0)
 *        R_BIAS - 128     G_BIAS - 128	 B_BIAS - 128
 * bias  = (0x380U << 20)  + (0x380U << 10)  + (0x380U << 0)
 */
static void inline npu_set_color_format(unsigned int color_format, oe_npu_t *npu)
{
	unsigned int conv0;
	unsigned int conv1;
	unsigned int conv2;
	unsigned int bias;

	if (color_format == NPU_COLOR_YUV) {
		conv0 = 0x000200C5U;
		conv1 = 0x3E9203C5U;
		conv2 = 0x0E820000U;
		bias  = 0x2BBF4A98U;
	}
	else {
		conv0 = 0x08000000U;
		conv1 = 0x00020000U;
		conv2 = 0x00000080U;
		bias  = 0x380E0380U;
	}

	NPU_WRITE_REG(ADDR_NPU_COLOR_CONV_0, conv0, npu);
	NPU_WRITE_REG(ADDR_NPU_COLOR_CONV_1, conv1, npu);
	NPU_WRITE_REG(ADDR_NPU_COLOR_CONV_2, conv2, npu);
	NPU_WRITE_REG(ADDR_NPU_COLOR_CONV_BIAS, bias, npu);
}

static int npu_run_pic(oe_npu_cmd_t *cmd)
{
	unsigned int data;
	int ret;

	oe_npu_net_t *npu_net = cmd->npu_net;
	oe_npu_t *npu = npu_net->npu;

	oe_npu_buf_t *in = cmd->in;
	oe_npu_buf_t *out = cmd->out;

	const int hard_reset = 0;
	unsigned int err_stat;

	unsigned int cmd_base = 0U;
	unsigned int wei_base = 0U;
	unsigned int work_base = 0U;
	unsigned int in_base = 0U;
	unsigned int out_base = 0U;

	//npu_drv_fin(0);

	npu_net->status = NET_BUSY;

	dma_sync_single_for_device(npu->dev,
		in->phys_addr,
		in->size,
		DMA_TO_DEVICE);

	mutex_lock(&npu->lock);

	// Reset
	npu_set_color_format(npu_net->color_format, npu);

	npu_reset(npu, hard_reset);

	// APB command for RUN_INFERENCE
	// load_description
	// [31: 28] :   2 => NPU_OPCODE_WR_REG
	// [23: 16] :   4 => count - 1
	// [15:  0] : 004 => iaddr
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, 0x20040004U, npu);

	// reg1
	// [31: 0] : addr offset
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, 0x00000000U, npu);

	// reg2
	// [10: 8] : 0 => base_buf_idx
	// [ 2: 0] : 2 => DRAM2CMD
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, 0x00000002U, npu);

	// reg3
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, 0x0, npu);

	// reg4
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, 0x0, npu);

	// reg5
	// [31:20] : 0 => num_chunk_m1
	// [12: 0] : F => sz_chunk
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, 0x0000000FU, npu);

	// reg0
	// [31:28]; 1 => op_code NPU_OPCODE_RUN
	// [   24]; 1 => dma_flag
	// [23:21]; 0 => dma_id
	// [20:16]; 1 => wdata
	// [12: 0]; 0 => iaddr
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, 0x11010000U, npu);

	if ((npu_net->cmd_buf->phys_addr >> 4) < 0xFFFFFFFFULL) {
		cmd_base = (unsigned int)(npu_net->cmd_buf->phys_addr >> 4);
	}
	if ((npu_net->wei_buf->phys_addr >> 4) < 0xFFFFFFFFULL) {
		wei_base = (unsigned int)(npu_net->wei_buf->phys_addr >> 4);
	}
	if ((npu->work->phys_addr >> 4) <= 0xFFFFFFFFULL) {
		work_base = (unsigned int)(npu->work->phys_addr >> 4);
	}
	if ((in->phys_addr >> 4) <= 0xFFFFFFFFULL) {
		in_base = (unsigned int)(in->phys_addr >> 4);
	}
	if ((out->phys_addr >> 4) <= 0xFFFFFFFFULL) {
		out_base = (unsigned int)(out->phys_addr >> 4);
	}

	NPU_WRITE_REG(ADDR_NPU_BASE_ADDR0, cmd_base, npu);
	NPU_WRITE_REG(ADDR_NPU_BASE_ADDR1, wei_base, npu);
	NPU_WRITE_REG(ADDR_NPU_BASE_ADDR2, work_base, npu);
	NPU_WRITE_REG(ADDR_NPU_BASE_ADDR3, in_base, npu);
	NPU_WRITE_REG(ADDR_NPU_BASE_ADDR4, out_base, npu);

	// 0x5U =  RUN, CMD_DMA
	NPU_WRITE_REG(ADDR_NPU_CONTROL, 0x5U, npu);

	ret = npu_wait_interrupt(npu, 1000);

	err_stat = npu_read_ecc_wdt(npu);

	npu_net->err_stat = err_stat;

	cmd->status.as_word = err_stat;

	if (ret == 0) {
		unsigned int mask;
		npu_net->status = NET_DONE;

		mask = ~npu->disable_fail.as_word;
		if ((cmd->status.as_word & mask) != 0U) {
			npu_net->status = NET_ERR;
		}
	}
	else {
		npu_net->status = NET_ERR;

		data = NPU_READ_REG(ADDR_NPU_CMD_CNT, npu);
		(void)pr_err("%08x: NPU_CMD_CNT\n", data);
		data = NPU_READ_REG(ADDR_NPU_STATUS, npu);
		(void)pr_err("%08x: NPU_STATUS\n", data);
	}

	mutex_unlock(&npu->lock);

	dma_sync_single_for_cpu(npu->dev,
			out->phys_addr,
			out->size,
			DMA_FROM_DEVICE);

	wake_up_interruptible(&npu_net->poll_waitq);

	npu_buffer_put(in);
	npu_buffer_put(out);

	//npu_drv_fout(0);

	return ret;
}

static unsigned int calc_inference_time(struct timespec64 *start, struct timespec64 *end)
{
	unsigned int ret = 0u;
	s64 time_start;
	s64 time_end;

	time_start = timespec64_to_ns(start);
	time_end = timespec64_to_ns(end);
	if ((time_start > 0 && time_end < LLONG_MIN + time_start) ||
		(time_start < 0 && time_end > LLONG_MAX + time_start)) {
		ret = 0;
	} else {
		ret = (time_end - time_start) / 1000L;
	}

	if (ret  < 0LL) {
		ret = 0;
	}
	else if (ret > 100000000LL) { //0 if more than 100sec
		ret = 0;
	}
	else {
		;
	}

	return ret;
}

static void npu_work_func(struct work_struct *work)
{
	oe_npu_sched_t *sched = (oe_npu_sched_t *)container_of(work, oe_npu_sched_t, work);
	oe_npu_cmd_t *cmd;
	oe_npu_cmd_t *tmp;

	struct timespec64 start;
	struct timespec64 end;

	cmd = NULL;
	tmp = NULL;

	list_for_each_entry_safe(cmd, tmp, &sched->queue, list) {
		oe_npu_net_t *npu_net = cmd->npu_net;

		ktime_get_ts64(&start);
		(void)npu_run_pic(cmd);
		ktime_get_ts64(&end);

		npu_net->elapsed_in_us  = calc_inference_time(&start, &end);

		(void)kref_put(&npu_net->ref_cnt, npu_net_free);

		mutex_lock(&sched->lock);
		list_del(&cmd->list);
		mutex_unlock(&sched->lock);

		kfree(cmd);
	}
}

static int npu_release(struct inode *np, struct file *fp)
{
	oe_npu_t *npu = (oe_npu_t *)fp->private_data;

	mutex_lock(&npu->lock);

	(void)kref_put(&npu->ref_cnt, npu_free);

	mutex_unlock(&npu->lock);

	return 0;
}

static void npu_free(struct kref *ref)
{
	int ret;

	oe_npu_t *npu = (oe_npu_t *)container_of(ref, oe_npu_t, ref_cnt);

	//npu_drv_fin(0);

	ret = npu_disable_wdt(npu);

	// Enable CLOCK GATING
	NPU_WRITE_REG(ADDR_NPU_CG_CTRL, 0x0, npu);

	// ASSERT MLX RESET & DISABLE MLX CLOCK
	NPU_WRITE_REG(ADDR_NPU_INT_RST_CTRL, 0, npu);

	if (ret == 0) {
		(void)pr_info("Disable WDT\n");
	}
	else {
		(void)pr_err("npu_disable_wdt failed\n");
	}

	kref_init(&npu->ref_cnt);

	atomic_set(&npu->is_initialized, 0);

	module_put(THIS_MODULE);

	//npu_drv_fout(0);
}

static int npu_open(struct inode *np, struct file *fp)
{
	oe_npu_t *npu;
	int ret;
	const int hard_reset = 1;
	unsigned int err_stat;
	unsigned int minor_num;

	//npu_drv_fin(0);

	ret = 0;

	minor_num = iminor(np);

	npu = NULL;

	if (minor_num < NPU_MAX_MINORS) {
		fp->private_data = &devs[minor_num];

		npu = (oe_npu_t *)fp->private_data;
		npu->mlx_bin_idx = 0U;
		npu->ecc_test_ctrl = 0xF1U;
		npu->mlx_err_inj_mask_data = 0U;
		npu->mlx_err_inj_mask_par = 0U;
		npu->dbg_print_reg = 0U;
		
		mutex_lock(&npu->lock);

		if (atomic_add_unless(&npu->is_initialized, 1, 1)) {
			npu_reset(npu, hard_reset);

			npu_enable_ecc(npu);

			if (npu_init_mlx_firmware(npu) == 0) {

				if (npu_disable_wdt(npu) == 0) {
					(void)pr_info("Disable WDT\n");
				}
				else {
					ret = -EFAULT;
					(void)pr_err("npu_disable_wdt failed\n");
				}

				if (npu_enable_wdt(npu) == 0) {
					(void)pr_info("Enable WDT\n");
				}
				else {
					ret = -EFAULT;
					(void)pr_err("npu_enable_wdt failed\n");
				}
			}
			else {
				ret = -EFAULT;
				(void)pr_err("npu_init_mlx_firmware failed\n");
			}
		}
		else {
			kref_get(&npu->ref_cnt);
		}

		err_stat = npu_read_ecc_wdt(npu);
		err_stat = err_stat & (~npu->disable_fail.as_word);
	}
	else {
		ret = -EFAULT;
	}

	if ((ret == 0) && (err_stat == 0U)) {
		(void)try_module_get(THIS_MODULE);
		ret = 0;
	}
	else {
		if (npu != NULL) {
			atomic_set(&npu->is_initialized, 0);
			(void)kref_put(&npu->ref_cnt, npu_free);
		}
		(void)pr_err("npu_open failed\n");
		ret = -EFAULT;
	}

	mutex_unlock(&npu->lock);

	//npu_drv_fout(0);

	return ret;
}

static int npu_net_release(struct inode *np, struct file *fp)
{
	oe_npu_net_t *npu_net = (oe_npu_net_t *)fp->private_data;
	(void)kref_put(&npu_net->ref_cnt, npu_net_free);
	return 0;
}

static void net_free(const oe_npu_net_t *npu_net)
{
	if (npu_net != NULL) {
		buf_free(npu_net->cmd_buf);
		buf_free(npu_net->wei_buf);
		kfree(npu_net);
	}
	else {
		(void)pr_err("failed, net_free(NULL)\n");
	}
}

static void npu_net_free(struct kref *ref)
{
	oe_npu_net_t *npu_net = (oe_npu_net_t *)container_of(ref, oe_npu_net_t, ref_cnt);
	net_free(npu_net);
}

static unsigned int npu_poll(struct file *fp, poll_table *wait)
{
	oe_npu_net_t *npu_net = (oe_npu_net_t *)fp->private_data;
	unsigned int ret;

	//npu_drv_fin(0);

	if (npu_net->user_status != NULL) {
		npu_err_bits_t *addr = npu_net->user_status;
		if (copy_to_user((void *)addr, &(npu_net->err_stat), sizeof(unsigned int)) != 0UL) {
			(void)pr_err("copy_to_user() failed\n");
		}
	}

	poll_wait(fp, &npu_net->poll_waitq, wait);

	if (npu_net->status == NET_DONE) {
		npu_net->status = NET_IDLE;
		ret = EPOLLIN;
	} else if (npu_net->status == NET_ERR) {
		npu_net->status = NET_IDLE;
		ret = EPOLLERR;
	} else if (npu_net->status == NET_STOP) {
		npu_net->status = NET_IDLE;
		ret = EPOLLERR;
	} else if (npu_net->status == NET_DMA_BUSY) {
		npu_net->status = NET_IDLE;
		ret = EPOLLERR;
	} else {
		ret = 0;
	}

	//npu_drv_fout(0);

	return ret;
}

static long npu_net_run(oe_npu_net_t *npu_net, unsigned long arg)
{
	const void __user *udata = (void __user *)arg;
	net_run_req_t req;

	oe_npu_t *npu = npu_net->npu;
	oe_npu_cmd_t *cmd;
	long ret;

	//npu_drv_fin(0);

	ret = 0;
	if (copy_from_user(&req, udata, sizeof(net_run_req_t)) == 0UL) {

		cmd = (oe_npu_cmd_t *)kmalloc(sizeof(oe_npu_cmd_t), GFP_KERNEL);
		if (cmd != NULL) {
			kref_get(&npu_net->ref_cnt);

			cmd->npu_net = npu_net;

			cmd->in = npu_buffer_get(req.in_fd);
			cmd->out = npu_buffer_get(req.out_fd);

			npu_net->user_status = req.err_status;

			mutex_lock(&npu->sched.lock);

			list_add_tail(&cmd->list, &npu->sched.queue);
			(void)schedule_work(&npu->sched.work);

			mutex_unlock(&npu->sched.lock);
		}
		else {
			ret = -EFAULT;
		}
	}
	else {
		ret = -EFAULT;
	}

	//npu_drv_fout(0);

	return ret;
}

static long npu_net_profile(oe_npu_net_t *npu_net, unsigned long arg)
{
	void __user *udata = (void __user *)arg;
	net_profile_req_t req;

	oe_npu_cmd_t cmd = {0, };

	struct timespec64 start;
	struct timespec64 end;

	oe_npu_t *npu = npu_net->npu;

	long ret;

	//npu_drv_fin(0);

	ret = 0;
	if (copy_from_user(&req, udata, sizeof(net_profile_req_t)) == 0UL) {
		cmd.npu_net = npu_net;

		cmd.in = npu_buffer_get(req.in_fd);
		cmd.out = npu_buffer_get(req.out_fd);

		npu_net->user_status = req.err_status;

		NPU_WRITE_REG(ADDR_NPU_PERF_DMA, 0, npu);
		NPU_WRITE_REG(ADDR_NPU_PERF_COMP, 0, npu);
		NPU_WRITE_REG(ADDR_NPU_PERF_ALL, 0, npu);

		ktime_get_ts64(&start);
		(void) npu_run_pic(&cmd);
		ktime_get_ts64(&end);

		req.dma = NPU_READ_REG(ADDR_NPU_PERF_DMA, npu);
		req.comp = NPU_READ_REG(ADDR_NPU_PERF_COMP, npu);
		req.all = NPU_READ_REG(ADDR_NPU_PERF_ALL, npu);
		req.elapsed_in_us = calc_inference_time(&start, &end);

		if (npu_net->user_status != NULL) {
			npu_err_bits_t *addr = npu_net->user_status;
			if (copy_to_user((void *)addr, &(npu_net->err_stat), sizeof(unsigned int)) != 0UL) {
				(void)pr_err("copy_to_user() failed\n");
				ret = -EFAULT;
			}
		}

		if (copy_to_user((void __user *)udata, &req, sizeof(net_profile_req_t)) != 0UL) {
			(void)pr_err("copy_to_user() failed\n");
			ret = -EFAULT;
		}
	}
	else {
		ret = -EFAULT;
	}

	//npu_drv_fout(0);

	return ret;
}

static long npu_net_set_color_format(oe_npu_net_t *npu_net, unsigned long arg)
{
	long ret;

	//npu_drv_fin(0);

	if (arg >= NPU_COLOR_END) {
		ret = -EFAULT;
	} else {
		npu_net->color_format = (unsigned int)arg;
		ret = 0;
	}

	//npu_drv_fout(0);

	return ret;
}

//TODO: For next update
// static long npu_net_get_npu_status(oe_npu_net_t *npu_net, unsigned long arg)
// {
// 	void __user *udata = (void __user *)arg;
// 	const oe_npu_t *npu = npu_net->npu;
// 	net_current_state_req_t req;
// 	long ret = 0l;

// 	req.dma = NPU_READ_REG(ADDR_NPU_PERF_DMA, npu);
// 	req.comp = NPU_READ_REG(ADDR_NPU_PERF_COMP, npu);
// 	req.elapsed_in_us = npu_net->elapsed_in_us;

// 	if (copy_to_user(udata, &req, sizeof(net_current_state_req_t)) != 0UL) {
// 			ret = -EFAULT;
// 			pr_err("copy_to_user() failed\n");
// 	}

// 	return ret;
// }

static long npu_net_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	oe_npu_net_t *npu_net = (oe_npu_net_t *)fp->private_data;
	long ret;

	//npu_drv_fin(0);

	switch (cmd) {
	case NPU_NET_IOCTL_RUN:
		ret = npu_net_run(npu_net, arg);
		break;

	case NPU_NET_IOCTL_PROFILE:
		ret = npu_net_profile(npu_net, arg);
		break;

	case NPU_NET_IOCTL_SET_COLOR_FMT:
		ret = npu_net_set_color_format(npu_net, arg);
		break;

	//TODO: For next update
	// case NPU_NET_GET_LASTEST_STATUS:
	// 	ret = npu_net_get_npu_status(npu_net, arg);
	// 	break;

	default:
		ret = -EFAULT;
		break;
	}

	//npu_drv_fout(0);

	return ret;
}

static const struct file_operations npu_net_fops = {
	.release = &npu_net_release,
	.unlocked_ioctl = &npu_net_ioctl,
	.poll = &npu_poll,
};

static int npu_buffer_mmap(struct file *fp, struct vm_area_struct *vma)
{
	int ret;
	const oe_npu_buf_t *npu = (oe_npu_buf_t *)fp->private_data;

	if (npu->phys_addr != 0U) {
		unsigned long vm_len = vma->vm_end - vma->vm_start;
		unsigned long vm_addr = npu->phys_addr;

		ret = remap_pfn_range(vma,
				vma->vm_start,
				PFN_DOWN(vm_addr),
				vm_len,
				vma->vm_page_prot);
	}
	else {
		ret = -EFAULT;
	}
	return ret;
}

static int npu_buffer_release(struct inode *np, struct file *fp)
{
	oe_npu_buf_t *buf = (oe_npu_buf_t *)fp->private_data;
	(void)kref_put(&buf->ref_cnt, npu_buffer_free);
	return 0;
}

static oe_npu_buf_t *buf_alloc(oe_npu_t *npu, unsigned long size)
{
	oe_npu_buf_t *buf;

	buf = (oe_npu_buf_t *)kzalloc(sizeof(oe_npu_buf_t), GFP_KERNEL);
	if (buf != NULL) {
		buf->npu = npu;
		if (ULONG_MAX - size < PAGE_SIZE) {
			buf->size = 0;
		}
		else {
			buf->size = npu_page_align(size);
		}

		buf->buf = dma_alloc_coherent(npu->dev, buf->size, &buf->phys_addr, GFP_KERNEL);
		if (buf->buf == NULL || buf->size == 0) {
			(void)pr_err("dma_alloc_coherent(%ld) failed\n", buf->size);
			kfree(buf);
			buf = NULL;
		}
	}
	else {
		(void)pr_err("%p:kzalloc() failed\n", buf);
	}

	return buf;
}

static void buf_free(const oe_npu_buf_t *npu_buf)
{
	if (npu_buf != NULL) {
		int err = 0;

		if (npu_buf->buf == NULL) {
			(void)pr_err("NULL buf->buf\n");
			err = -EFAULT;
		}

		if (npu_buf->npu == NULL) {
			(void)pr_err("NULL buf->npu\n");
			err = -EFAULT;
		}

		if (err == 0) {
			dma_free_coherent(npu_buf->npu->dev, npu_buf->size,
				npu_buf->buf, npu_buf->phys_addr);
		}

		kfree(npu_buf);
	}
	else {
		(void)pr_err("NULL buf free\n");
	}
}

static void npu_buffer_free(struct kref *ref)
{
	const oe_npu_buf_t *buf = (oe_npu_buf_t *)container_of(ref, oe_npu_buf_t, ref_cnt);
	buf_free(buf);
}

static const struct file_operations npu_buffer_fops = {
	.release = &npu_buffer_release,
	.mmap = &npu_buffer_mmap,
};

static int mlx_validate(oe_npu_t *npu, uint32_t core_id)
{
	unsigned int base;
	unsigned int data;
	int cnt = 1000;
	int ret;

	if (core_id < 4U) {
		base = ADDR_NPU_MLX_C0_HCI_00 + (core_id * 0x40U);
		NPU_WRITE_REG(base + 0x4U, 0xED9EU, npu);
		NPU_WRITE_REG(base + 0x0U, 0x101U, npu);

		do {
			data = NPU_READ_REG(base + 0x0U, npu);

			mdelay(1);
			if (cnt-- == 0) {
				(void)pr_err("mlx validation timeout\n");
				break;
			}
		} while ((data & 0x10U) != 0U);

		data = NPU_READ_REG(base + 0x8U, npu);

		if (data != 0x107EU) {
			ret = -EFAULT;
		} else {
			ret = 0;
		}
	}
	else {
		ret = -EFAULT;
	}

	return ret;
}

static int mlx_load_kernel(oe_npu_t *npu, const oe_npu_buf_t *buf)
{
	unsigned int i;
	unsigned int data;
	int ret;
	unsigned int num_core;
	unsigned int buf_base = 0;

	// Prepare DMA operation

	data = NPU_READ_REG(ADDR_NPU_ID_CODE, npu);
	num_core = (data >> 24) & 0xfU;
	num_core += 1U;

	if ((buf->phys_addr >> 4) <= 0xFFFFFFFFULL) {
		buf_base = (unsigned int)(buf->phys_addr >> 4);
	}

	NPU_WRITE_REG(ADDR_NPU_BASE_ADDR7, buf_base, npu);

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
	switch (num_core) {
		case 1: data = 0x00100706U; break;
		case 2: data = 0x00300706U; break;
		case 3: data = 0x00700706U; break;
		default: data = 0x00F00706U; break;
	}
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, data, npu);

	// reg3
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, 0x0, npu);

	// reg4
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, data, npu);

	// reg5
	// [31: 20] :   0 => num_chunk_m1
	// [12:  0] :   size_chunk32_m1
	if ((((buf->size + 31U) >> 5U) - 1U) <= 0x1FFFUL) {
		data = (((unsigned int)buf->size + 31U) >> 5U) - 1U;
	}
	else {
		data = 0x1FFF; // 0x3FF for 32KB
	}
	NPU_WRITE_REG(ADDR_NPU_APB_COMMAND, data, npu);

	// RUN DMA to load MLX Kernel
	// [31: 28] :   2 => NPU_OPCODE_RUN
	// [    24] :   1 => dma_flag=STALL_WHEN_DMA_Q_FULL
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

	// 0x1U = RUN + CMD_APB
	NPU_WRITE_REG(ADDR_NPU_CONTROL, 0x1U, npu);

	// wait queued operation is done
	ret = npu_wait_interrupt(npu, 1000);

	if (ret == 0) {
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
		(void)pr_err("%08x: NPU_CMD_CNT\n", data);
		data = NPU_READ_REG(ADDR_NPU_STATUS, npu);
		(void)pr_err("%08x: NPU_STATUS\n", data);
	}

	(void)pr_info("All %d MLX core(s) is(are) validated.\r\n", num_core);

	return ret;
}

/*
   copy_from/to_user() is to copy block data between user and kernel.
   And user and kernel memory space are independent and
   implemented in separate address spaces.
*/
static long npu_read_reg(oe_npu_t *npu, unsigned long arg)
{
	const void __user *udata = (void __user *)arg;
	reg_access_req_t req;
	long ret;

	ret = 0L;
	if (copy_from_user(&req, udata, sizeof(reg_access_req_t)) == 0UL) {
		req.data = NPU_READ_REG(req.addr, npu);
		if (copy_to_user((void *)arg, &req, sizeof(reg_access_req_t)) != 0UL) {
			ret = -EFAULT;
		}
	}
	else {
		ret = -EFAULT;
	}

	return ret;
}

static long npu_write_reg(oe_npu_t *npu, unsigned long arg)
{
	const void __user *udata = (void __user *)arg;
	reg_access_req_t req;
	long ret;

	ret = 0L;
	if (copy_from_user(&req, udata, sizeof(reg_access_req_t)) == 0UL) {
		NPU_WRITE_REG(req.addr, req.data, npu);
	}
	else {
		ret = -EFAULT;
	}

	return ret;
}

static long npu_load_network(oe_npu_t *npu, unsigned long arg)
{
	const void __user *udata = (void __user *)arg;
	net_load_req_t req;
	oe_npu_net_t *npu_net = NULL;
	int npu_net_fd;
	int err;
	long ret;

	err = 0;
	if (copy_from_user(&req, udata, sizeof(net_load_req_t)) == 0UL) {

		npu_net = (oe_npu_net_t *)kzalloc(sizeof(oe_npu_net_t), GFP_KERNEL);
		if (npu_net != NULL) {
			oe_npu_buf_t *buf;

			buf = buf_alloc(npu, req.cmd_size);
			if (buf != NULL) {
				npu_net->cmd_buf = buf;

				if (req.cmd_size <= buf->size) {
					unsigned long cmd_size = req.cmd_size;
					if (copy_from_user(buf->buf, req.cmd_data, cmd_size) == 0UL) {
						dma_sync_single_for_device(npu->dev,
							buf->phys_addr, buf->size, DMA_TO_DEVICE);
					}
					else {
						err = -EFAULT;
						(void)pr_err("copy_from_user() failed\n");
					}
				}
				else {
					err = -EFAULT;
					(void)pr_err("wrong buf size %ld %ld\n", req.cmd_size, buf->size);
				}
			}
			else {
				err = -EFAULT;
				(void)pr_err("buf_alloc() failed\n");
			}

			buf = buf_alloc(npu, req.wei_size);
			if (buf != NULL) {
				npu_net->wei_buf = buf;

				if (req.wei_size <= buf->size) {
					unsigned long wei_size = req.wei_size;
					if (copy_from_user(buf->buf, req.wei_data, wei_size) == 0UL) {
						dma_sync_single_for_device(npu->dev,
							buf->phys_addr, buf->size, DMA_TO_DEVICE);
					}
					else {
						err = -EFAULT;
						(void)pr_err("copy_from_user() failed\n");
					}
				}
				else {
					err = -EFAULT;
					(void)pr_err("wrong buf size %ld %ld\n", req.wei_size, buf->size);
				}
			}
			else {
				err = -EFAULT;
				(void)pr_err("buf_alloc() failed\n");
			}
		}
		else {
			err = -EFAULT;
			(void)pr_err("%p:net kzalloc() failed\n", npu_net);
		}
	} else {
		err = -EFAULT;
		(void)pr_err("copy_from_user() failed\n");
	}

	if (err == 0) {
		int mask;
		npu_net->npu = npu;
		npu_net->color_format = NPU_COLOR_RGB;

		kref_init(&npu_net->ref_cnt);
		init_waitqueue_head(&npu_net->poll_waitq);

		mask = O_ACCMODE + O_CLOEXEC;
		npu_net_fd = anon_inode_getfd("npu_network", &npu_net_fops, npu_net, mask);
		if (npu_net_fd >= 0) {
			struct file *net_fp;
			net_fp = fget(npu_net_fd);
			net_fp->f_mode |= FMODE_LSEEK | FMODE_WRITE | FMODE_READ;
			fput(net_fp);
		}
		else {
			err = -EFAULT;
			(void)pr_err("anon_inode_getfd() failed\n");
		}
	}

	if (err == 0) {
		ret = npu_net_fd;
	}
	else {
		ret = err;
		if (npu_net != NULL) {
			net_free(npu_net);
		}
	}

	return ret;
}

static long npu_alloc_buffer(oe_npu_t *npu, unsigned long arg)
{
	const void __user *udata = (void __user *)arg;
	buf_alloc_req_t req;

	oe_npu_buf_t *buf = NULL;

	int npu_buf_fd;

	long ret;
	int err;

	err = 0;

	if (copy_from_user(&req, udata, sizeof(buf_alloc_req_t)) == 0UL) {
		buf = buf_alloc(npu, req.size);
		if (buf == NULL) {
			err = -EFAULT;
			(void)pr_err("buf_alloc() failed\n");
		}
		else {
			kref_init(&buf->ref_cnt);
		}
	}
	else {
		err = -EFAULT;
		(void)pr_err("copy_from_user() failed\n");
	}

	if (err == 0) {
		int mask = O_ACCMODE + O_CLOEXEC;
		npu_buf_fd = anon_inode_getfd("npu-buffer", &npu_buffer_fops, buf, mask);
		if (npu_buf_fd >= 0) {
			struct file *buf_fp;
			buf_fp = fget(npu_buf_fd);
			buf_fp->f_mode |= FMODE_LSEEK | FMODE_WRITE | FMODE_READ;
			fput(buf_fp);
		}
		else {
			err = -EFAULT;
			(void)pr_err("anon_inode_getfd() failed\n");
		}
	}

	// FIXME. why copy?
	if (err == 0) {
		req.addr = buf->phys_addr;
		if (copy_to_user((void *)arg, &req, sizeof(buf_alloc_req_t)) != 0UL) {
			err = -EFAULT;
			(void)pr_err("copy_to_user() failed\n");
		}
	}

	if (err == 0) {
		ret = npu_buf_fd;
	}
	else {
		buf_free(buf);
		ret = err;
	}

	return ret;
}

/*
   copy_from/to_user() is to copy block data between user and kernel.
   And user and kernel memory space are independent and
   implemented in separate address spaces.
*/
static long npu_read_err_status(const oe_npu_t *npu, unsigned long arg)
{
	long ret;
	const npu_err_rd_req_t *err_stat = &npu_err_stat;

	ret = 0L;

	if (npu != NULL) {
		if (copy_to_user((void *)arg, err_stat, 
			sizeof(npu_err_rd_req_t)) != 0UL) {
			ret = -EFAULT;
		}
	}
	else {
		ret = -EFAULT;
	}


	return ret;
}

static int npu_init_mlx_firmware(oe_npu_t *npu)
{
	int err;
	unsigned int fidx;
	oe_npu_buf_t *buf;

	err = 0;
	buf = NULL;

	fidx = npu->mlx_bin_idx;
	if (fidx >= MAX_MLX_FILES_NUM) {
		err = -EFAULT;
		(void)pr_err("%d mlx_bin_idx\n", fidx);
	}

	if (err == 0) {
		buf = buf_alloc(npu, MAX_MLX_KERNEL_SIZE);
		if (buf == NULL) {
			err = -EFAULT;
			(void)pr_err("buf_alloc() failed\n");
		}
	}

	if (err == 0) {
		const char *fn;
		int req_ret;
		const struct firmware *fp = NULL;
		const char bin_fn0[] = "mlx_kernel.bin";
		const char bin_fn1[] = "test_kernel_0.bin";
		const char bin_fn2[] = "test_kernel_1.bin";
		const char bin_fn3[] = "test_kernel_2.bin";
		const char bin_fn4[] = "test_kernel_3.bin";
		const char bin_fn5[] = "test_kernel_4.bin";
		const char bin_fn6[] = "test_kernel_5.bin";
		const char bin_fn7[] = "test_kernel_6.bin";

		switch (fidx) {
		case 0: fn = bin_fn0; break;
		case 1: fn = bin_fn1; break;
		case 2: fn = bin_fn2; break;
		case 3: fn = bin_fn3; break;
		case 4: fn = bin_fn4; break;
		case 5: fn = bin_fn5; break;
		case 6: fn = bin_fn6; break;
		default: fn = bin_fn7; break;
		}

		req_ret = request_firmware_into_buf(&fp, fn,
				npu->dev, buf->buf, buf->size);
		if (req_ret != 0) {
			err = -EFAULT;
			(void)pr_err("request_firmware_into_buf() failed\n");
		}
	}

	if (err == 0) {
		// flushing cache
		dma_sync_single_for_device(npu->dev,
					buf->phys_addr,
					buf->size,
					DMA_TO_DEVICE);

		if (mlx_load_kernel(npu, buf) < 0) {
			err = -EFAULT;
			(void)pr_err("mlx_load_kernel() failed\n");
		}
	}

	if (buf != NULL) {
		buf_free(buf);
	}

	return err;
}

static void npu_reset(oe_npu_t *npu, int hard)
{
	unsigned int data;

	if (hard == 0) {
		//soft reset

		data = NPU_READ_REG(ADDR_NPU_CG_CTRL, npu);

		// Disable CLOCK GATING
		NPU_WRITE_REG(ADDR_NPU_CG_CTRL, 0x70, npu);

		// 0x6U = CMD_DMA | SW_RESET;
		NPU_WRITE_REG(ADDR_NPU_CONTROL, 0x6U, npu);

		// 0x2U = SW_RESET
		NPU_WRITE_REG(ADDR_NPU_CONTROL, 0x2U, npu);
		udelay(1);

		// Enable CLOCK GATING
		NPU_WRITE_REG(ADDR_NPU_CG_CTRL, data, npu);

		atomic_set(&npu->irq_done, 0);

	} else {
		//hard reset

		// ASSERT MLX RESET w/ clock enabled
		//0x10F00U = RST_CTRL_IBUS | RST_CTRL_MCLK_EN
		data = 0x10F00U; 
		NPU_WRITE_REG(ADDR_NPU_INT_RST_CTRL, data, npu);

		// ASSERT MLX RESET & DISABLE MLX CLOCK
		NPU_WRITE_REG(ADDR_NPU_INT_RST_CTRL, 0, npu);

		// RESET GBUS/MBUS
		// Jake. double internal bus reset(Dec212022)
		//0x10000U = RST_CTRL_IBUS
		NPU_WRITE_REG(ADDR_NPU_INT_RST_CTRL, 0x10000U, npu);
		NPU_WRITE_REG(ADDR_NPU_INT_RST_CTRL, 0x10000U, npu);

		// Disable CLOCK GATING
		NPU_WRITE_REG(ADDR_NPU_CG_CTRL, 0x70, npu);

		// NPU SOFT_RESET
		// 0x6U = CMD_DMA | SW_RESET;
		NPU_WRITE_REG(ADDR_NPU_CONTROL, 0x6U, npu);

		// NPU SOFT_RESET
		// 0x2U = SW_RESET
		NPU_WRITE_REG(ADDR_NPU_CONTROL, 0x2U, npu);
		udelay(1);

		// Enable CLOCK GATING
		NPU_WRITE_REG(ADDR_NPU_CG_CTRL, 0x0, npu);

		// Reset clk_en status
		// 0x2U = SW_RESET
		NPU_WRITE_REG(ADDR_NPU_CONTROL, 0x2U, npu);

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
}

static long npu_write_test_cfg(oe_npu_t *npu, unsigned long arg)
{
	const void __user *udata = (void __user *)arg;
	test_cfg_wr_req_t req;
	long ret;

	ret = 0;

	if (copy_from_user(&req, udata, sizeof(test_cfg_wr_req_t)) == 0UL) {

		if ((req.wdt_ext_cnt != 0U) || (req.wdt_int_cnt != 0U)) {
			npu->wdt_ext_cnt = req.wdt_ext_cnt;
			npu->wdt_int_cnt = req.wdt_int_cnt / 100U;
		}

		if (req.ecc_test_ctrl != 0U) {
			npu->ecc_test_ctrl = req.ecc_test_ctrl;
		}

		npu->mlx_err_inj_mask_data = req.mlx_err_inj_mask_data;
		npu->mlx_err_inj_mask_par  = req.mlx_err_inj_mask_par;

		if (req.mlx_bin_idx < MAX_MLX_FILES_NUM) {
			npu->mlx_bin_idx = req.mlx_bin_idx;
		}
		else {
			ret = -EFAULT;
			(void)pr_err("%d mlx_bin_idx\n", req.mlx_bin_idx);
		}
	}
	else {
		ret = -EFAULT;
		(void)pr_err("copy_from_user() failed\n");
	}

	return ret;
}

static long npu_init_npu(oe_npu_t *npu, unsigned long arg)
{
	const void __user *udata = (void __user *)arg;
	npu_init_req_t req;
	long ret = 0L;
	unsigned int err_stat = 0;

	mutex_lock(&npu->lock);

	if (copy_from_user(&req, udata, sizeof(npu_init_req_t)) == 0UL) {
		unsigned int data;

		data = req.disable_ue_fail;
		npu->disable_fail.as_field.ue_sram = (data >> 0) & 0x1U;
		npu->disable_fail.as_field.ue_gbuf = (data >> 1) & 0x1U;
		npu->disable_fail.as_field.ue_cbuf = (data >> 2) & 0x1U;

		data = req.disable_ce_fail;
		npu->disable_fail.as_field.ce_sram = (data >> 0) & 0x1U;
		npu->disable_fail.as_field.ce_gbuf = (data >> 1) & 0x1U;
		npu->disable_fail.as_field.ce_cbuf = (data >> 2) & 0x1U;

		data = req.disable_wdt;
		npu->disable_fail.as_field.wdt_to = data & 0x1U;

		npu->dbg_print_reg = req.soft_reset & 0x2U;

		if ((req.soft_reset & 0x1U) == 0U) {
			npu_reset(npu, 1);
		}
		else {
			npu_reset(npu, 0);
		}

		if ((req.soft_reset & 0x1U) != 0U) {
			(void)pr_info("soft reset called\n");
			(void)pr_info("Do not ECC set and load fireware\n");
		}
		else {
			(void)pr_info("hard reset called\n");
			(void)pr_info("Do ECC set and load fireware\n");

			npu_enable_ecc(npu);

			if (npu_init_mlx_firmware(npu) == 0) {
				(void)pr_info("npu_init_mlx_firmware succeeded\n");

				ret = npu_disable_wdt(npu);
				
				if (ret == 0L) {
					(void)pr_info("Disable WDT\n");
				}
				else {
					(void)pr_err("npu_disable_wdt failed\n");
				}
				
				ret = npu_enable_wdt(npu);
				
				if (ret == 0L) {
					(void)pr_info("Enable WDT\n");
				}
				else {
					(void)pr_err("npu_enable_wdt failed\n");
				}
			}
			else {
				ret = -EFAULT;
				(void)pr_err("npu_init_mlx_firmware failed\n");
			}
		}
	}
	else {
		(void)pr_err("copy_from_user failed\n");
	}

	err_stat = npu_read_ecc_wdt(npu);
	err_stat = err_stat & (~npu->disable_fail.as_word);

	if ((ret == 0L) && (err_stat == 0U)) {
		ret = 0L;
	}
	else {
		ret = -EFAULT;
	}

	mutex_unlock(&npu->lock);

	return ret;
}

static long npu_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	oe_npu_t *npu = (oe_npu_t *)fp->private_data;
	long ret;

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

	case NPU_IOCTL_WRITE_TEST_CFG:
		ret = npu_write_test_cfg(npu, arg);
		break;

	case NPU_IOCTL_READ_NPU_ERR:
		ret = npu_read_err_status(npu, arg);
		break;

	default:
		ret = -EFAULT;
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
static int ndolphin_set_clk(const oe_npu_t *npu);
#endif

static dev_t stDev;
static int npu_device_probe(struct platform_device *pdev)
{
	struct device *dev = NULL;
	oe_npu_t *npu = NULL;
	struct device_node *npu_node;

	int err = 0;
	int ret;
	unsigned int data;

	if (pdev != NULL) {
		int val;
		val = ida_simple_get(&telechips_ida, 0U, NPU_MAX_MINORS, GFP_KERNEL);
		if (val >= 0) {
			npu = &devs[val];
			npu->id = val;

			dev = &pdev->dev;
			npu_node = dev->of_node;

			npu->dev = dev;
			npu->dev_iomap_base = devm_of_iomap(&pdev->dev, npu_node, 0, NULL);
			if (npu->dev_iomap_base != NULL) {
				kref_init(&npu->ref_cnt);
				atomic_set(&npu->is_initialized, 0);
			}
			else {
				err = -EFAULT;
				dev_err(dev, "%p:devm_of_iomap fail\n", npu->dev_iomap_base);
			}

			data = NPU_READ_REG(ADDR_NPU_ID_CODE, npu);
			if ((data & 0xFFFFU) != 0xED9EU) {
				dev_err(dev, "NPU not found\n");
				err = -EFAULT;
			}
		}
		else {
			err = -EFAULT;
			(void)pr_err("%d: ida_simple_get fail\n", val);
		}
	}
	else {
		err = -EFAULT;
		(void)pr_err("%p:pdev\n", pdev);
	}

	if (err == 0) {
		npu->irq = platform_get_irq(pdev, 0U);

		if (npu->irq >= 0) {
			ret = request_irq((unsigned int)npu->irq, npu_interrupt_handler, IRQF_TRIGGER_RISING, "npu", npu);
			if (ret == 0) {
				unsigned int work_buf_size;

				init_waitqueue_head(&npu->irq_waitq);

				if (of_property_read_u32(npu_node, "work-buffer-size", &work_buf_size) == 0) {

					oe_npu_buf_t *buf;

					if (of_reserved_mem_device_init(dev) == 0) {
						(void)pr_info("using reserved memory\n");
					}
					else {
						(void)pr_info("using cma-default\n");
					}

					(void)dma_set_mask(dev, DMA_BIT_MASK(64));

					buf = buf_alloc(npu, work_buf_size);
					if (buf != NULL) {
						npu->work = buf;
					}
					else {
						err = -EFAULT;
						dev_err(dev, "buf_alloc() failed\n");
					}

				}
				else {
					err = -EFAULT;
					dev_err(dev, "work-buffer-size not found\n");
				}
			}
			else {
				err = -EFAULT;
				dev_err(dev, "%d:request_irq failed\n", ret);
			}
		}
		else { 
			err = -EFAULT;
			dev_err(dev, "%d:platform_get_irq fail\n", npu->irq);
		}
	}

	if (err == 0) {
		npu->status = NPU_READY;

		stDev = MKDEV(NPU_MAJOR, npu->id);
		dev_set_drvdata(dev, npu);

		if (npu->id < NPU_MAX_MINORS) {
			cdev_init(&devs[npu->id].cdev_inst, &fops);

			if (cdev_add(&devs[npu->id].cdev_inst, stDev, 1) < 0) {
				err = -EFAULT;
				dev_err(dev, "cdev_add fail\n");
			}
		}
		else {
			err = -EFAULT;
		}
	}

	if (err == 0) {
		struct device *dev_ret;

		dev_ret = device_create(&npu_class, NULL, stDev, NULL, "npu%d", npu->id);
		if (IS_ERR(dev_ret)) {
			dev_err(dev, "device_create failed\n");
			err = -EFAULT;
		}
		else {

			mutex_init(&npu->lock);

			INIT_WORK(&npu->sched.work, npu_work_func);
			INIT_LIST_HEAD(&npu->sched.queue);
			mutex_init(&npu->sched.lock);
		}
	}

#ifdef NDOLPHIN_ENV
	if (err == 0) {
		static int clk_init;
		if (clk_init == 0) {
			clk_init = 1;
			err = ndolphin_set_clk(npu);
		}
	}
#endif
	if (err == 0) {
		// read from tcc750x-npu.dtsi
		npu->disable_fail.as_word = 0;

		ret = of_property_read_u32(npu_node, "disable-ue-fail", &data);
		if (ret != 0) {
			data = 0U;
			dev_warn(dev, "disable_ue_fail set to default (0x%08x)\n", data);
		}

		npu->disable_fail.as_field.ue_sram = (data >> 0) & 0x1U;
		npu->disable_fail.as_field.ue_gbuf = (data >> 1) & 0x1U;
		npu->disable_fail.as_field.ue_cbuf = (data >> 2) & 0x1U;

		ret = of_property_read_u32(npu_node, "disable-ce-fail", &data);
		if (ret != 0) {
			data = 0U;
			dev_warn(dev, "disable_ce_fail set to default (0x%08x)\n", data);
		}
		npu->disable_fail.as_field.ce_sram = (data >> 0) & 0x1U;
		npu->disable_fail.as_field.ce_gbuf = (data >> 1) & 0x1U;
		npu->disable_fail.as_field.ce_cbuf = (data >> 2) & 0x1U;

		ret = of_property_read_u32(npu_node, "wdt-timeout-count", &data);
		if (ret != 0) {
			data = 800000000U;   // 800000000U = 1000(ms) * 800(MHz) * 1000
			dev_warn(dev, "wdt_ext_cnt set to default (0x%08x)\n", data);
		}
		npu->wdt_ext_cnt = data;

		ret = of_property_read_u32(npu_node, "wdt-rearm-count", &data);
		if (ret != 0) {
			data = npu->wdt_ext_cnt / 2U;
			dev_warn(dev, "wdt_int_cnt set to default (0x%08x)\n", data);
		}
		npu->wdt_int_cnt = data / 100U; // 1 tick for 100 cycle

		ret = of_property_read_u32(npu_node, "disable-wdt", &data);
		if (ret != 0) {
			data = 0U;
			dev_warn(dev, "disable_wdt set to default (0x%08x)\n", data);
		}

		npu->disable_fail.as_field.wdt_to = data & 0x1U;
	}

	if (err == 0) {
		(void)pr_info("NPU DRIVER VERSION %d.%d.%d\n", DRIVER_MAJOR, DRIVER_MINOR, DRIVER_PATCH);
		(void)pr_info("NPU initialized..\n");
	}
	else {
		if ((npu != NULL) && (npu->irq >= 0)) {
			(void)free_irq((unsigned int)npu->irq, npu);
		}
		(void)pr_info("NPU initialize failed\n");
	}

	return err;
}

#ifdef NDOLPHIN_ENV
static int ndolphin_set_clk(const oe_npu_t *npu)
{
	unsigned int clk_rate = 0;
	unsigned long clk_rate_u64 = 0;
	int err;
	int ret;
	struct device_node *npu_node;

	struct clk *npu_aclk;
	struct clk *npu_pclk;
	struct clk *npu_cclk;
	struct clk *npu_cpuclk;

	const struct device *dev;

	dev = npu->dev;
	npu_node = dev->of_node;

	err = 0;
	npu_aclk = of_clk_get_by_name(npu_node, "npubus_aclk");
	if (IS_ERR((void *)npu_aclk)) {
		err = -EFAULT;
	}

	ret = of_property_read_u32(npu_node, "aclk-freq", &clk_rate);
	if (ret != 0) {
		clk_rate = 800000000;
		dev_warn(dev, "[WARN][NPU] Set aclk to default(%d Hz)\n", clk_rate);
	}

	(void)clk_prepare_enable(npu_aclk);
	(void)clk_set_rate(npu_aclk, clk_rate);
	clk_rate_u64 = clk_get_rate(npu_aclk);
	dev_dbg(dev, "[DEBUG][NPU] Set aclk to %ld Hz\n", clk_rate_u64);

	npu_pclk = of_clk_get_by_name(npu_node, "npubus_pclk");
	if (IS_ERR((void *)npu_pclk)) {
		err = -EFAULT;
	}

	ret = of_property_read_u32(npu_node, "pclk-freq", &clk_rate);
	if (ret != 0) {
		clk_rate = 200000000;
		dev_warn(dev, "[WARN][NPU] Set aclk to default(%d Hz)\n",
			clk_rate);
	}
	(void)clk_prepare_enable(npu_pclk);
	(void)clk_set_rate(npu_pclk, clk_rate);
	clk_rate_u64 = clk_get_rate(npu_pclk);
	dev_dbg(dev, "[DEBUG][NPU] Set pclk to %ld Hz\n", clk_rate_u64);

	npu_cclk = of_clk_get_by_name(npu_node, "npubus_cclk");
	if (IS_ERR((void *)npu_cclk)) {
		err = -EFAULT;
	}

	ret = of_property_read_u32(npu_node, "cclk-freq", &clk_rate);
	if (ret != 0) {
		clk_rate = 1200000000;
		dev_warn(dev, "[WARN][NPU] Set cclk to default(%d Hz)\n", clk_rate);
	}
	(void)clk_prepare_enable(npu_cclk);
	(void)clk_set_rate(npu_cclk, clk_rate);
	clk_rate_u64 = clk_get_rate(npu_cclk);
	dev_dbg(dev, "[DEBUG][NPU] Set cclk to %ld Hz\n", clk_rate_u64);

	npu_cpuclk = of_clk_get_by_name(npu_node, "npubus_cpu");
	if (IS_ERR((void *)npu_cpuclk)) {
		err = -EFAULT;
	}

	ret = of_property_read_u32(npu_node, "cpuclk-freq", &clk_rate);
	if (ret != 0) {
		clk_rate = 800000000;
		dev_warn(dev, "[WARN][NPU] Set cpuclk to default(%d Hz)\n", clk_rate);
	}
	(void)clk_prepare_enable(npu_cpuclk);
	(void)clk_set_rate(npu_cpuclk, clk_rate);
	clk_rate_u64 = clk_get_rate(npu_cpuclk);
	dev_dbg(dev, "[DEBUG][NPU] Set cpuclk to %ld Hz\n", clk_rate_u64);

	return err;
}
#endif

static int npu_device_remove(struct platform_device *pdev)
{
	struct device *dev;
	oe_npu_t *npu;
	const oe_npu_buf_t *work_buf;

	BUG_ON(pdev == NULL);

	if (pdev != NULL) {
		dev = &pdev->dev;
		npu = (oe_npu_t *)dev_get_drvdata(dev);
		work_buf = npu->work;

		iounmap(npu->dev_iomap_base);
		if (npu->irq >= 0) {
			(void)free_irq((unsigned int)npu->irq, npu);
		}

		dma_free_coherent(dev,
				work_buf->size,
				work_buf->buf,
				work_buf->phys_addr);

		kfree(work_buf);

		NPU_WRITE_REG(ADDR_NPU_IRQ_MASK, 0, npu);
		NPU_WRITE_REG(ADDR_NPU_IRQ_ENABLE, 0, npu);
	}

	(void)pr_info("NPU de-initialized\n");

	return 0;
}


#ifdef CONFIG_OF
static const struct of_device_id npu_match[] = {
	{
		.compatible = "telechips,npu",
	}
};
MODULE_DEVICE_TABLE(of, npu_match);
#endif

static struct platform_driver npu_driver = {
	.probe = npu_device_probe,
	.remove = npu_device_remove,
	.driver = {
		.name  = "telechips-npu",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(npu_match),
	},
};

static int __init npu_init(void)
{
	int ret;
	int cls_reg;
	int plf_reg;

	cls_reg = class_register(&npu_class);
	plf_reg = platform_driver_register(&npu_driver);

	if ((cls_reg == 0) && (plf_reg == 0)) {
		ret = 0;
	}
	else {
		(void)pr_err("%d:class_register\n", cls_reg);
		(void)pr_err("%d:platform_driver_register\n", plf_reg);
		ret = -EFAULT;
	}

	return ret;
}

static void npu_exit(void)
{
	platform_driver_unregister(&npu_driver);
	device_destroy(&npu_class, devs[0].cdev_inst.dev);
	cdev_del(&devs[0].cdev_inst);
	class_unregister(&npu_class);
};

module_init(npu_init);
module_exit(npu_exit);

/** @brief return ECC, WDT status
 *  @param[in] npu   npu handler
 *  @return     non-zero if error detected
 */
static unsigned int npu_read_ecc_wdt(oe_npu_t *npu)
{
	unsigned int i;
	unsigned int data;
	unsigned int base;
	unsigned int num_core;
	unsigned int ue_flag;
	unsigned int ce_flag;

	npu_err_bits_t ecc_wdt;

	npu_err_rd_req_t* err_stat;
	npu_ecc_cbuf_t* ecc_cbuf;
	npu_ecc_gbuf_t* ecc_gbuf;
	npu_ecc_sram_t* ecc_sram;

	data = NPU_READ_REG(ADDR_NPU_ID_CODE, npu);
	num_core = (data >> 24) & 0xfU;
	num_core += 1U;

	err_stat = &npu_err_stat;
	err_stat->irq_reason = NPU_READ_REG(ADDR_NPU_IRQ_REASON, npu);

	//CBuf
	ecc_cbuf = &err_stat->cbuf;
	data = NPU_READ_REG(ADDR_NPU_ECC_CBUF_ECC_CNT, npu);
	ecc_cbuf->ue_cnt = (data >> 0) & 0xFFU;
	ecc_cbuf->ce_cnt = (data >> 8) & 0xFFU;

	//GBuf
	for (i = 0; i < num_core; i++) {
		ecc_gbuf = &err_stat->gbuf[i];
		base = ADDR_NPU_ECC_GBUF_UE_CNT_C0 + (i * 0x4U);
		data = NPU_READ_REG(base , npu);
		ecc_gbuf->ue_cnt = data;

		base = ADDR_NPU_ECC_GBUF_CE_CNT_C0 + (i * 0x4U);
		data = NPU_READ_REG(base , npu);
		ecc_gbuf->ce_cnt = data;
	}

	//MLX SRAM
	for (i = 0; i < num_core; i++) {
		ecc_sram = &err_stat->sram[i];

		base = ADDR_NPU_MLX_C0_ECC_CTRL + (i * 0x40U);
		data = NPU_READ_REG(base, npu);
		ecc_sram->ue_status = (data >> 9) & 0x1U;
		ecc_sram->ce_status = (data >> 8) & 0x1U;

		base = ADDR_NPU_MLX_C0_ECC_CNT + (i * 0x40U);
		data = NPU_READ_REG(base, npu);
		ecc_sram->ue_cnt = (data >> 16) & 0xFFU;
		ecc_sram->ce_cnt = (data >>  0) & 0xFFU;

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

	//WDT
	err_stat->wdt_to = 0U;
	for (i = 0; i < num_core; i++) {
		base = ADDR_NPU_MLX_C0_HCI_00 + (i * 0x40U);
		data = NPU_READ_REG(base, npu);

		if ((data & 0x02000000U) != 0U) {
			err_stat->wdt_to = 1U;
		}
	}

	// Clear ECC count
	NPU_WRITE_REG(ADDR_NPU_IRQ_REASON, IRQ_ECC, npu);
	NPU_WRITE_REG(ADDR_NPU_IRQ_CLEAR, 1, npu);

	ecc_wdt.as_field.wdt_to = (err_stat->wdt_to > 0) ? 1U : 0U;

	ue_flag = (err_stat->cbuf.ue_cnt > 0U) ? 1U : 0U;
	ce_flag = (err_stat->cbuf.ce_cnt > 0U) ? 1U : 0U;

	ecc_wdt.as_field.ue_cbuf = ue_flag;
	ecc_wdt.as_field.ce_cbuf = ce_flag;

	ue_flag = 0U;
	ce_flag = 0U;

	for (i = 0; i < num_core; i++) {
		if (err_stat->gbuf[i].ue_cnt > 0U) {
			ue_flag = 1U;
		}
		if (err_stat->gbuf[i].ce_cnt > 0U) {
			ce_flag = 1U;
		}
	}

	ecc_wdt.as_field.ce_gbuf = ce_flag;
	ecc_wdt.as_field.ue_gbuf = ue_flag;

	ue_flag = 0U;
	ce_flag = 0U;

	for (i = 0; i < num_core; i++) {
		if (err_stat->sram[i].ue_cnt > 0U) {
			ue_flag = 1U;
		}

		if (err_stat->sram[i].ce_cnt > 0U) {
			ce_flag = 1U;
		}
	}

	ecc_wdt.as_field.ce_sram = ce_flag;
	ecc_wdt.as_field.ue_sram = ue_flag;

	return ecc_wdt.as_word;
}

static void npu_enable_ecc(oe_npu_t *npu)
{
	unsigned int i;
	unsigned int base;
	unsigned int data;
	unsigned int num_core;

	data = NPU_READ_REG(ADDR_NPU_ID_CODE, npu);
	num_core = (data >> 24) & 0xfU;
	num_core += 1U;

	NPU_WRITE_REG(ADDR_NPU_ECC_CTRL, 0xF1U, npu); // 0xF1: GBUF, CBUF ECC enable

	data = npu->ecc_test_ctrl;
	NPU_WRITE_REG(ADDR_NPU_ECC_TEST_CTRL, data, npu);

	for (i = 0; i < num_core; i++) {
		base = ADDR_NPU_MLX_C0_ECC_CTRL + (i * 0x40U);
		NPU_WRITE_REG(base, 0x30001U, npu);

		base = ADDR_NPU_MLX_C0_ECC_CNT + (i * 0x40U);
		NPU_WRITE_REG(base, 0x0U, npu);

		base = ADDR_NPU_MLX_C0_ECC_MASK_DAT + (i * 0x40U);
		data = npu->mlx_err_inj_mask_data;
		NPU_WRITE_REG(base, data, npu);
		base = ADDR_NPU_MLX_C0_ECC_MASK_PAR + (i * 0x40U);
		data = npu->mlx_err_inj_mask_par;
		NPU_WRITE_REG(base, data, npu);
	}
}

static int npu_enable_wdt(oe_npu_t *npu)
{
	unsigned int i;

	unsigned int data;
	unsigned int base;
	unsigned int ext_cnt;
	unsigned int int_cnt;
	unsigned int wdt_dis;
	int ret = 0;
	unsigned int num_core;

	data = NPU_READ_REG(ADDR_NPU_ID_CODE, npu);
	num_core = (data >> 24) & 0xfU;
	num_core += 1U;

	wdt_dis = npu->disable_fail.as_field.wdt_to;
	if (wdt_dis == 0U) {
		ext_cnt = npu->wdt_ext_cnt;
		int_cnt = npu->wdt_int_cnt;
	}
	else {
		ext_cnt = 0xFFFFFFFFU;
		int_cnt = 0x0U;
	}

	for (i = 0; i < num_core; i++){
		int timeout_cnt = 1000;

		base = ADDR_NPU_MLX_C0_HCI_00 + (i * 0x40U);
		NPU_WRITE_REG(base + 0x4U, 0xCD09, npu);
		NPU_WRITE_REG(base + 0x8U, ext_cnt, npu);
		NPU_WRITE_REG(base + 0xCU, int_cnt, npu);

		// [24] WDTEN, [8] INTDIS, [0] run
		if (wdt_dis == 0U) {
			NPU_WRITE_REG(base, 0x01000101, npu);
		}
		else {
			NPU_WRITE_REG(base, 0x00000101, npu);
		}

		ret = -ETIMEDOUT;

		do {
			data = NPU_READ_REG(base, npu);
			if ((data & 0x10U) == 0U) {
				ret = 0;
				break;
			}

			mdelay(1);
		} while(--timeout_cnt > 0);

		if (ret != 0) {
			(void)pr_err("WDT[%d] activate timeout!\n", i);
		}
	}

	return ret;
}

static int npu_disable_wdt(oe_npu_t *npu)
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

			if ((data & 0x10U) == 0U) {
				ret = 0;
				break;
			}

			mdelay(1);

		} while(--timeout_cnt > 0);

		if (ret != 0) {
			(void)pr_err("WDT[%d] disable timeout!\n", i);
		}
	}

	return ret;
}
