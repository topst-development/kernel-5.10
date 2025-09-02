// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/poll.h>
#include <linux/cpufreq.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/uaccess.h>
#include <linux/types.h>

#include <linux/io.h>
#include <asm/div64.h>

#include "tcc_grp.h"
#include <video/telechips/tcc_gre2d.h>
#include <video/telechips/tcc_gre2d_api.h>
#include <video/telechips/tcc_gre2d_type.h>
#include <video/telechips/tcc_grp_ioctrl.h>

extern G2D_DITHERING_TYPE gG2D_Dithering_type;

#define DO_NOTHING /* Do nothing to comply for MISRA C:2012 Rule 16.4 */

//#define dprintk(msg...) pr_info("[DBG][GRP] " msg)
#define dprintk(msg...)

//#define GRP_DBG(msg...) pr_info(msg)
#define GRP_DBG(msg...)

struct g2d_data {
	uint32_t dev_opened;

	wait_queue_head_t poll_wq;
	wait_queue_head_t cmd_wq;

	spinlock_t g2d_spin_lock;
	struct mutex io_mutex;

	unsigned char block_waiting;
	unsigned char block_operating;
	unsigned char irq_reged;
};


struct g2d_drv_type {
	uint32_t irq;
	struct miscdevice *misc;
	void __iomem *reg;
	struct clk *pClk;
	struct g2d_data *data;
};


static void g2d_drv_response_check(
struct g2d_data *data, uint32_t res_type) {

	int32_t ret = 0;
	uint32_t type_intr = (uint32_t) G2D_INTERRUPT;
	uint32_t type_poll = (uint32_t) G2D_POLLING;

	if (type_intr == res_type) {
		dprintk("%s:[%d][%d]\n", __func__, res_type, data->block_waiting);
	} else if (type_poll == res_type) {
		dprintk("%s:[%d][%d]\n", __func__, res_type, data->block_waiting);

		ret = wait_event_interruptible_timeout(
			data->poll_wq,
			data->block_operating == 0u,
			msecs_to_jiffies(500u));
		if (ret <= 0) {
			(void)pr_warn("[WAR][GRP] G2D time out :%d Line :%d\n", __LINE__, ret);
		}
	} else {
		DO_NOTHING /* Do nothing to comply for MISRA C:2012 Rule 16.4 */
	}
}


static int32_t grp_common_ctrl(
struct g2d_data	*data, G2D_COMMON_TYPE *g2d_p) {

	uint32_t srcY=0u, srcU=0u, srcV=0u;
	uint32_t tgtY=0u, tgtU=0u, tgtV=0u;
	struct rot_src_params src_params;
	struct rot_dst_params dst_params;
	int32_t res = 0;

	if ((0u == g2d_p->src1) && (0u == g2d_p->src2)) {
		tcc_get_addr_yuv(g2d_p->srcfm.format, g2d_p->src0, g2d_p->src_imgx,
						g2d_p->src_imgy, 0, 0, &srcY, &srcU, &srcV);
	} else {
		srcY = g2d_p->src0;
		srcU = g2d_p->src1;
		srcV = g2d_p->src2;
	}

	if ((char)0 != g2d_p->DefaultBuffer) {
		res = -EFAULT;
		/* prevent KCS warning */
	} else {
		if ((g2d_p->tgt1 == 0u) && (g2d_p->tgt2 == 0u)) {
			tcc_get_addr_yuv(g2d_p->tgtfm.format, g2d_p->tgt0, g2d_p->dst_imgx,
							 g2d_p->dst_imgy, 0, 0, &tgtY, &tgtU, &tgtV);
			g2d_p->tgt1 = tgtU;
			g2d_p->tgt2 = tgtV;
			if (g2d_p->srcfm.format == (uint32_t)GE_YUV420_in) {
				g2d_p->tgt2 = 0u;
				tgtV = 0u;
			}
		} else {
			tgtY = g2d_p->tgt0;
			tgtU = g2d_p->tgt1;
			tgtV = g2d_p->tgt2;
		}
	}

	if ((g2d_p->ch_mode == ROTATE_90) || (g2d_p->ch_mode == ROTATE_270)) {
		if (g2d_p->crop_imgy > (g2d_p->dst_imgx - g2d_p->dst_off_x)) {
			dprintk("rg2d_p->crop_imgx[%d][%d][%d]!!!\n",
				g2d_p->crop_imgx, g2d_p->dst_imgx, g2d_p->dst_off_x);
			g2d_p->crop_imgy = g2d_p->dst_imgx - g2d_p->dst_off_x;
		}

		if (g2d_p->crop_imgx > (g2d_p->dst_imgy - g2d_p->dst_off_y)) {
			dprintk("rg2d_p->crop_imgy[%d][%d][%d]!!!\n",
				g2d_p->crop_imgy, g2d_p->dst_imgy, g2d_p->dst_off_y);
			g2d_p->crop_imgx = g2d_p->dst_imgy - g2d_p->dst_off_y;
		}
	} else {
		if (g2d_p->crop_imgx > (g2d_p->dst_imgx - g2d_p->dst_off_x)) {
			dprintk("g2d_p->crop_imgx[%d][%d][%d]!!!\n",
				g2d_p->crop_imgx, g2d_p->dst_imgx, g2d_p->dst_off_x);
			g2d_p->crop_imgx = g2d_p->dst_imgx - g2d_p->dst_off_x;
		}

		if (g2d_p->crop_imgy > (g2d_p->dst_imgy - g2d_p->dst_off_y)) {
			dprintk("g2d_p->crop_imgy[%d][%d][%d]!!!\n",
				g2d_p->crop_imgy, g2d_p->dst_imgy, g2d_p->dst_off_y);
			g2d_p->crop_imgy = g2d_p->dst_imgy - g2d_p->dst_off_y;
		}
	}

	gre2d_rsp_interrupt(G2D_INTERRUPT_TYPE);

	if ((g2d_p->srcfm.format <= (uint32_t)GE_YUV422_sq) &&
		(g2d_p->tgtfm.format == (uint32_t)GE_RGB565)) {
		gG2D_Dithering_en = 1u;
		gG2D_Dithering_type = ADD_1_OP;
	} else {
		gG2D_Dithering_en = 0u;
	}

	spin_lock_irq(&(data->g2d_spin_lock));

	src_params.src0 = srcY;
	src_params.src1 = srcU;
	src_params.src2 = srcV;
	src_params.srcfm = g2d_p->srcfm;
	src_params.src_imgx = g2d_p->src_imgx;
	src_params.src_imgy = g2d_p->src_imgy;
	src_params.offset_x = g2d_p->crop_offx;
	src_params.offset_y = g2d_p->crop_offy;
	src_params.Rimg_x = g2d_p->crop_imgx;
	src_params.Rimg_y = g2d_p->crop_imgy;

	dst_params.tgt0 = tgtY;
	dst_params.tgt1 = tgtU;
	dst_params.tgt2 = tgtV;
	dst_params.tgtfm = g2d_p->tgtfm;
	dst_params.des_imgx = g2d_p->dst_imgx;
	dst_params.des_imgy = g2d_p->dst_imgy;
	dst_params.offset_x = g2d_p->dst_off_x;
	dst_params.offset_y = g2d_p->dst_off_y;

	(void) gre2d_ImgRotate_Ex(src_params, dst_params, g2d_p->ch_mode, g2d_p->parallel_ch_mode);

	(void) gre2d_interrupt_ctrl(0, G2D_INT_NONE, 0, 0);
	spin_unlock_irq(&(data->g2d_spin_lock));

	if (gG2D_Dithering_en != 0u) {
		gG2D_Dithering_en = 0u;
	}

	g2d_drv_response_check(data, g2d_p->responsetype);

	return res;
}


static void grp_rotate_ctrl(
struct g2d_data *data, const G2D_BITBLIT_TYPE *g2d_p) {

	uint32_t src_addr_comp = 0u;
	uint32_t dest_addr_comp = 0u;
	uint32_t src_addr = g2d_p->src0;
	uint32_t dest_addr = g2d_p->tgt0;
	struct rot_src_params src_params;
	struct rot_dst_params dst_params;

	src_addr_comp = (src_addr % 8u) / 2u;
	dest_addr_comp = (dest_addr % 8u) / 2u;
	src_addr = (src_addr >> 3u) << 3u;
	dest_addr = (dest_addr >> 3u) << 3u;

	dprintk("%s  add:0x%x w:%u h:%u sx:%u sy:%u !!!\n",
		__func__, src_addr, g2d_p->src_imgx,
		g2d_p->src_imgy, g2d_p->crop_imgx, g2d_p->crop_imgy);

	dprintk("%s  0x%x %u %u	%u %u !! %u %u  !\n",
		__func__, dest_addr, g2d_p->dst_imgx,
		g2d_p->dst_imgy, g2d_p->crop_offx,
		g2d_p->crop_offy, g2d_p->dst_off_x, g2d_p->dst_off_y);

	spin_lock_irq(&(data->g2d_spin_lock));

	gre2d_rsp_interrupt(G2D_INTERRUPT_TYPE);

	src_params.src0 = src_addr;
	src_params.src1 = 0;
	src_params.src2 = 0;
	src_params.srcfm = g2d_p->srcfm;
	src_params.src_imgx = g2d_p->src_imgx;
	src_params.src_imgy = g2d_p->src_imgy;
	src_params.offset_x = g2d_p->crop_offx + src_addr_comp;
	src_params.offset_y = g2d_p->crop_offy;
	src_params.Rimg_x = g2d_p->crop_imgx;
	src_params.Rimg_y = g2d_p->crop_imgy;

	dst_params.tgt0 = dest_addr;
	dst_params.tgt1 = 0;
	dst_params.tgt2 = 0;
	dst_params.tgtfm = g2d_p->tgtfm;
	dst_params.des_imgx = g2d_p->dst_imgx;
	dst_params.des_imgy = g2d_p->dst_imgy;
	dst_params.offset_x = g2d_p->dst_off_x + dest_addr_comp;
	dst_params.offset_y = g2d_p->dst_off_y;

	(void) gre2d_ImgRotate_Ex(src_params, dst_params, g2d_p->ch_mode, NOOP);

	(void) gre2d_interrupt_ctrl(0, G2D_INT_NONE, 0, 0);
	spin_unlock_irq(&(data->g2d_spin_lock));

	g2d_drv_response_check(data, g2d_p->responsetype);
}


static int32_t g2d_drv_mmap(
struct file *filp, struct vm_area_struct *vma) {

	int32_t res = 0;
	unsigned long pfn = 0u;
	(void) filp;

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	if (vma->vm_end >= vma->vm_start) {
		pfn = vma->vm_end - vma->vm_start;
	} else {
		pfn = 0u;
	}
	
	res = remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, pfn, vma->vm_page_prot);

	if (0 != res) {
		res = -EAGAIN;
	} else {
		vma->vm_ops = NULL;
		vma->vm_flags |= (unsigned long)VM_IO;
		vma->vm_flags |= ((unsigned long)VM_DONTEXPAND | (unsigned long)VM_PFNMAP);
	}

	return res;
}


static uint32_t g2d_drv_poll(
struct file *filp, struct poll_table_struct *wait) {

	const struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	const struct g2d_drv_type *g2d = dev_get_drvdata(misc->parent);
	uint32_t ret = 0u;

	if (NULL != g2d->data) {
		dprintk("%s\n", __func__);

		poll_wait(filp, &(g2d->data->poll_wq), wait);
		spin_lock_irq(&(g2d->data->g2d_spin_lock));
		if (0u == g2d->data->block_operating) {
			ret = ((uint32_t)POLLIN | (uint32_t)POLLRDNORM);
		}

		spin_unlock_irq(&(g2d->data->g2d_spin_lock));
	}

	return ret;
}


static irqreturn_t
g2d_drv_handler(int irq, void *client_data) {

	const struct g2d_drv_type *g2d = (struct g2d_drv_type *)client_data;
	uint32_t int_ctrl = (uint32_t) G2D_INT_NONE;
	uint32_t r_flg = (uint32_t) G2D_INT_R_FLG;
	(void) irq;

	/* GE IRQ */
	int_ctrl = (uint32_t) gre2d_interrupt_ctrl(0, G2D_INT_NONE, 0, 0);

	dprintk("%s irq[%u] %u block[%u]\n", __func__, int_ctrl, g2d->data->block_waiting, g2d->data->block_operating);

	if ((int_ctrl & r_flg) == r_flg) {
		int_ctrl = (uint32_t) gre2d_interrupt_ctrl(1, G2D_INT_ALL, 0, 1);
		gre2d_set_dma_interrupt(SET_G2D_DMA_INT_DISABLE);

		wake_up_interruptible(&g2d->data->poll_wq);

		if (g2d->data->block_operating >= 1u) {
			g2d->data->block_operating = 0u;
		}

		if (g2d->data->block_waiting != 0u) {
			wake_up_interruptible(&g2d->data->cmd_wq);
		}
	}

	return IRQ_HANDLED;
}


long g2d_drv_ioctl(
struct file *filp, unsigned int cmd, unsigned long arg) {

	const struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	const struct g2d_drv_type *g2d = dev_get_drvdata(misc->parent);
	int32_t ret = 0;

	dprintk("%s IOCTL [%d] [%d]!!!\n", __func__, cmd, g2d->data->block_operating);

	mutex_lock(&g2d->data->io_mutex);

	switch (cmd) {
		case TCC_GRP_COMMON_IOCTRL:
		case TCC_GRP_COMMON_IOCTRL_KERNEL:
		{
			G2D_COMMON_TYPE	*g2d_p = kmalloc(sizeof(G2D_COMMON_TYPE), GFP_KERNEL);
			if (NULL == g2d_p) {
				ret = ENOMEM;
				break;
			}

			if (cmd == (uint32_t)TCC_GRP_COMMON_IOCTRL) {
				if (0u != copy_from_user((void *)(g2d_p), (void __user *)arg, sizeof(G2D_COMMON_TYPE))) {
					kfree((const void *)g2d_p);
					ret = -EFAULT;
					break;
				}
			} else {
				(void) memcpy(g2d_p, (G2D_COMMON_TYPE *)arg, sizeof(G2D_COMMON_TYPE));
			}

			if (g2d->data->block_operating >= 1u)	 {
				g2d->data->block_waiting = 1u;
				ret = wait_event_interruptible_timeout(
					g2d->data->cmd_wq,
					g2d->data->block_operating == 0u,
					msecs_to_jiffies(200u));
				if (ret <= 0) {
					(void)pr_warn("[WAR][GRP] G2D time out :%d Line ret:%d block:%d\n",
									__LINE__, ret, g2d->data->block_operating);
					g2d->data->block_operating = 0u;
				}
			}

			spin_lock_irq(&(g2d->data->g2d_spin_lock));
			g2d->data->block_waiting = 0u;
			g2d->data->block_operating++;
			spin_unlock_irq(&(g2d->data->g2d_spin_lock));

			(void) grp_common_ctrl(g2d->data, (G2D_COMMON_TYPE *)g2d_p);

			if (cmd == (uint32_t)TCC_GRP_COMMON_IOCTRL) {
				if (0u != copy_to_user((void __user *)arg, (void *)(g2d_p), sizeof(G2D_COMMON_TYPE))) {
					kfree((const void *)g2d_p);
					ret = -EFAULT;
					break;
				}
			} else {
				(void)memcpy((G2D_COMMON_TYPE *)arg, g2d_p, sizeof(G2D_COMMON_TYPE));
			}

			kfree((const void *)g2d_p);
		}
		break;

		case TCC_GRP_ROTATE_IOCTRL:
		case TCC_GRP_ROTATE_IOCTRL_KERNEL:
		{
			G2D_BITBLIT_TYPE g2d_p1;

			if (cmd == (uint32_t) TCC_GRP_ROTATE_IOCTRL) {
				if (0u != copy_from_user((void *)(&g2d_p1), (void __user *)arg, sizeof(g2d_p1))) {
					ret = -EFAULT;
					break;
				}
			} else {
				(void)memcpy(&g2d_p1, (G2D_BITBLIT_TYPE *)arg, sizeof(g2d_p1));
			}

			if (g2d->data->block_operating >= 1u)	{
				g2d->data->block_waiting = 1u;
				ret = wait_event_interruptible_timeout(g2d->data->cmd_wq, g2d->data->block_operating == 0u, msecs_to_jiffies(200u));
				if (ret <= 0) {
					(void)pr_warn("[WAR][GRP] G2D time out :%d Line :%d block:%d\n",
								__LINE__, ret, g2d->data->block_operating);
					g2d->data->block_operating = 0u;
				}
			}

			spin_lock_irq(&(g2d->data->g2d_spin_lock));
			g2d->data->block_waiting = 0;
			g2d->data->block_operating++;
			spin_unlock_irq(&(g2d->data->g2d_spin_lock));

			grp_rotate_ctrl(g2d->data, &g2d_p1);
		}
		break;

		default:
			dprintk("Commands that G2D does not support\n");
			ret = -EFAULT;
			break;
	}

	mutex_unlock(&g2d->data->io_mutex);

	return ret;
}
EXPORT_SYMBOL(g2d_drv_ioctl);


int32_t g2d_drv_release(
struct inode *p_inode, struct file *filp) {

	const struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct g2d_drv_type	*g2d = dev_get_drvdata(misc->parent);
	int32_t ret = 0;

	(void) p_inode;

	mutex_lock(&g2d->data->io_mutex);

	if (g2d->data->dev_opened > 0u) {
		g2d->data->dev_opened--;
	}

	if (g2d->data->dev_opened == 0u) {
		if (g2d->data->irq_reged != 0u) {
			(void) irq_set_affinity_hint(g2d->irq, NULL);
			(void) free_irq(g2d->irq, g2d);
			g2d->data->irq_reged = 0u;
		}
	}

	if (NULL != g2d->pClk) {
		clk_disable_unprepare(g2d->pClk);
	}

	mutex_unlock(&g2d->data->io_mutex);

	dprintk("%s open_cnt :%d\n", __func__, g2d->data->dev_opened);

	return ret;
}
EXPORT_SYMBOL(g2d_drv_release);


int32_t g2d_drv_open(
struct inode *p_inode, struct file *filp) {

	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct g2d_drv_type *g2d = dev_get_drvdata(misc->parent);
	int32_t ret = 0;
	(void) p_inode;

	mutex_lock(&g2d->data->io_mutex);

	if (NULL != g2d->pClk) {
		(void) clk_prepare_enable(g2d->pClk);
	}

	if (0u == g2d->data->irq_reged) {
		ret = request_irq(g2d->irq, g2d_drv_handler, IRQF_SHARED, g2d->misc->name, g2d);
		if (ret != 0) {
			if (NULL != g2d->pClk) {
				clk_disable_unprepare(g2d->pClk);
			}
			(void)pr_err("[ERR][GRP] aquire %s request_irq.\n", g2d->misc->name);
			ret = -EFAULT;
		} else {
			g2d->data->irq_reged = 1u;
			g2d->data->dev_opened++;
			dprintk("%s open_cnt :%d\n", __func__, g2d->data->dev_opened);
		}
	} else {	
		g2d->data->dev_opened++;
		dprintk("%s open_cnt :%d\n", __func__, g2d->data->dev_opened);
	}

	mutex_unlock(&g2d->data->io_mutex);

	return ret;
}
EXPORT_SYMBOL(g2d_drv_open);


static const struct file_operations g2d_drv_fops = {
	.owner = THIS_MODULE,
	.poll = g2d_drv_poll,
	.unlocked_ioctl = g2d_drv_ioctl,
	.mmap = g2d_drv_mmap,
	.open = g2d_drv_open,
	.release = g2d_drv_release,
};


static int32_t
g2d_drv_remove(struct platform_device *pdev) {

	const struct g2d_drv_type *g2d;

	if (NULL != pdev) {
		g2d = (struct g2d_drv_type *)platform_get_drvdata(pdev);
		misc_deregister(g2d->misc);
		kfree(g2d->data);
		kfree(g2d->misc);
		kfree(g2d);
	}

	return 0;
}


static int32_t
g2d_drv_probe(struct platform_device *pdev) {

	struct g2d_drv_type *g2d;
	int32_t ret = -ENODEV;

	g2d = kzalloc(sizeof(struct g2d_drv_type), GFP_KERNEL);
	if (NULL == g2d) {
		ret = -ENOMEM;
	} else {
		g2d->pClk = of_clk_get(pdev->dev.of_node, 0);
		if (true == IS_ERR((void *)g2d->pClk)) {
			g2d->pClk = NULL;
		}

		g2d->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
		if (NULL == g2d->misc) {
			ret = -ENOMEM;
			kfree(g2d);
		} else {
			g2d->data = kzalloc(sizeof(struct g2d_data), GFP_KERNEL);
			if (NULL == g2d->data) {
				ret = -ENOMEM;
				kfree(g2d->misc);
				kfree(g2d);
			} else {
				g2d->misc->minor = MISC_DYNAMIC_MINOR;
				g2d->misc->fops = &g2d_drv_fops;
				g2d->misc->name = "g2d";
				g2d->misc->parent = &pdev->dev;
				ret = misc_register(g2d->misc);
				if (0 == ret) {
					g2d->irq = platform_get_irq(pdev, 0u);

					dprintk("%s: irq: %d\n", g2d->misc->name, g2d->irq);

					spin_lock_init(&(g2d->data->g2d_spin_lock));
					mutex_init(&g2d->data->io_mutex);

					init_waitqueue_head(&g2d->data->poll_wq);
					init_waitqueue_head(&g2d->data->cmd_wq);

					platform_set_drvdata(pdev, g2d);

					g2d->data->block_waiting = 0u;
					g2d->data->block_operating = 0u;

					/* register g2d discdevice */
					dprintk("%s: G2D Driver Initialized\n", __func__);
				} else {
					kfree(g2d->data);
					kfree(g2d->misc);
					kfree(g2d);
				}
			}
		}
	}

	if (0 != ret) {
		(void)pr_err("[ERR][GRP] %s: %s: err ret:%d\n", __func__, pdev->name, ret);
	}

	return ret;
}


static int32_t g2d_drv_suspend(
struct platform_device *pdev, pm_message_t state) {

	(void) pdev;
	(void) state;
	return 0;
}


static int32_t
g2d_drv_resume(struct platform_device *pdev) {

	(void) pdev;
	return 0;
}


static const struct of_device_id g2d_of_match[] = {
	{ .compatible = "telechips,graphic.2d" },
	{}
};
MODULE_DEVICE_TABLE(of, g2d_of_match);


static struct platform_driver g2d_driver = {
	.probe = g2d_drv_probe,
	.remove = g2d_drv_remove,
	.suspend = g2d_drv_suspend,
	.resume = g2d_drv_resume,
	.driver = {
		.name = "g2d",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(g2d_of_match),
#endif
	},
};


static int32_t __init g2d_drv_init(void) {
	return platform_driver_register(&g2d_driver);
}


static void __exit g2d_drv_exit(void) {
	platform_driver_unregister(&g2d_driver);
}


module_init(g2d_drv_init);
module_exit(g2d_drv_exit);


MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC Video for Linux g2d driver");
MODULE_LICENSE("GPL");

