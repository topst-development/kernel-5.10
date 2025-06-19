// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_comm.h"

#ifdef CONFIG_SUPPORT_TCC_JPU

#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "jpu_mgr_sys.h"

#define dprintk_jpus(msg...)  V_DBG(VPU_DBG_INFO, "TCC_JPU_MGR_SYS:" msg)
#define detailk_jpus(msg...)  V_DBG(VPU_DBG_INFO, "TCC_JPU_MGR_SYS:" msg)
#define err_jpus(msg...)      V_DBG(VPU_DBG_INFO, "TCC_JPU_MGR_SYS[Err]:" msg)
#define info_jpus(msg...)     V_DBG(VPU_DBG_INFO, "TCC_JPU_MGR_SYS[Info]:" msg)

static struct clk *vbus_jpeg_clk;	// = NULL;

#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
#include <linux/reset.h>
static struct reset_control *vbus_jpeg_reset;	// = NULL;
#endif

//extern int tccxxx_sync_player(int sync);
static int cache_droped_jpu;	// = 0;

#define JPU_UDELAY(x)	udelay((unsigned long)(x))


void jmgr_enable_clock(void)	//void jmgr_enable_clock(int vbus_no_ctrl, int only_clk_ctrl)
{
	if (vbus_jpeg_clk != NULL) {
		(void)clk_prepare_enable(vbus_jpeg_clk);
	}

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	if (!only_clk_ctrl) {
		int ret = jpu_optee_open();

		if (ret != 0) {
			err_jpus("jpu_optee_open: failed !! - ret [%d]", ret);
		} else {
			dprintk_jpus("jpu_optee_open: success !!");
		}
	}
#endif
}

void jmgr_disable_clock(void)	//void jmgr_disable_clock(int vbus_no_ctrl, int only_clk_ctrl)
{
	if (vbus_jpeg_clk != NULL) {
		clk_disable_unprepare(vbus_jpeg_clk);
	}

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	if (!only_clk_ctrl) {
		int ret = jpu_optee_close();

		if (ret != 0) {
			err_jpus("jpu_optee_close: failed !! - ret [%d]", ret);
		} else {
			dprintk_jpus("jpu_optee_close: success !!");
		}
	}
#endif
}

void jmgr_get_clock(struct device_node *pnode)
{
	if (pnode == NULL) {
		err_jpus("device node is null");
	}

	vbus_jpeg_clk = of_clk_get(pnode, 1);
	VPU_BUG_ON(vbus_jpeg_clk);
}

void jmgr_put_clock(void)
{
	if (vbus_jpeg_clk != NULL) {
		clk_put(vbus_jpeg_clk);
		vbus_jpeg_clk = NULL;
	}
}

void jmgr_get_reset(struct device_node *pnode)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	if (pnode == NULL) {
		(void)pr_info("device node is null\n");
	}

	vbus_jpeg_reset = of_reset_control_get_by_index(pnode, 0);
	VPU_BUG_ON(vbus_jpeg_reset);
#else
	VPU_UNUSED_PARAMETER(pnode);
#endif
}

void jmgr_put_reset(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	if (vbus_jpeg_reset != NULL) {
		reset_control_put(vbus_jpeg_reset);
		vbus_jpeg_reset = NULL;
	}
#else
	V_DBG(VPU_DBG_RSTCLK, "not support");
#endif
}

int jmgr_get_reset_register(void)
{
	return 0;
}

void jmgr_hw_assert(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	V_DBG(VPU_DBG_RSTCLK, "enter");
	if (vbus_jpeg_reset != NULL) {
		(void)reset_control_assert(vbus_jpeg_reset);
	}

	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)", jmgr_get_reset_register());
#else
	V_DBG(VPU_DBG_RSTCLK, "not support");
#endif
}

void jmgr_hw_deassert(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	V_DBG(VPU_DBG_RSTCLK, "enter");
	if (vbus_jpeg_reset != NULL) {
		(void)reset_control_deassert(vbus_jpeg_reset);
	}

	V_DBG(VPU_DBG_RSTCLK, "out!! (rsr:0x%x)", jmgr_get_reset_register());
#else
	V_DBG(VPU_DBG_RSTCLK, "not support");
#endif
}

void jmgr_hw_reset(void)
{
#if defined(VIDEO_IP_DIRECT_RESET_CTRL)
	JPU_UDELAY(1000);

	jmgr_hw_assert();

	JPU_UDELAY(1000);

	jmgr_hw_deassert();

	JPU_UDELAY(1000);
#else
	V_DBG(VPU_DBG_RSTCLK, "not support");
#endif
}

void jmgr_restore_clock(int vbus_no_ctrl, int opened_cnt)
{
#if 1
	int opened_count = opened_cnt;

	jmgr_hw_assert();

	LOG_COVERITY("%d%d", vbus_no_ctrl, opened_cnt);

	while (opened_count > 0) {
		jmgr_disable_clock();	//jmgr_disable_clock(vbus_no_ctrl, 0);
		opened_count--;
	}

	JPU_UDELAY(1000);

	opened_count = opened_cnt;
	while (opened_count > 0) {
		jmgr_enable_clock();	//jmgr_enable_clock(vbus_no_ctrl, 0);
		opened_count--;
	}

	jmgr_hw_deassert();
#else
	jmgr_hw_reset();
#endif
}

void jmgr_enable_irq(unsigned int irq)
{
	enable_irq(irq);
}

void jmgr_disable_irq(unsigned int irq)
{
	disable_irq(irq);
}

void jmgr_free_irq(unsigned int irq, void *dev_id)
{
	(void)free_irq(irq, dev_id);
}

int jmgr_request_irq(unsigned int irq,
			 irqreturn_t (*handler)(int irqh, void *dev_idh),
			 unsigned long frags, const char *pdevice, void *dev_id)
{
	return request_irq(irq, handler, frags, pdevice, dev_id);
}

unsigned long jmgr_get_int_flags(void)
{
	return (unsigned long)IRQ_INT_TYPE;
}

void jmgr_init_interrupt(void)
{
	LOG_COVERITY("%d", cache_droped_jpu);
}

int jmgr_BusPrioritySetting(int mode, int type)
{
	LOG_COVERITY("%d%d", mode, type);
	return 0;
}

void jmgr_status_clear(const unsigned int *base_addr)
{
	LOG_COVERITY("%p", base_addr);
}

void jmgr_init_variable(void)
{
	cache_droped_jpu = 0;
}
#endif /*CONFIG_SUPPORT_TCC_JPU*/
