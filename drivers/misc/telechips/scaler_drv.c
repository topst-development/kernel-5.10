// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/io.h>
#include <linux/compat.h>

#include <video/telechips/vioc_intr.h>
#include <video/telechips/tcc_types.h>
#include <video/telechips/tcc_scaler_ioctrl.h>
#include <video/telechips/vioc_config.h>
#include <video/telechips/vioc_scaler.h>
#include <video/telechips/vioc_rdma.h>
#include <video/telechips/vioc_wdma.h>
#include <video/telechips/vioc_wmix.h>
#include <video/telechips/vioc_global.h>
#include <video/telechips/vioc_lut.h>
#include <video/telechips/vioc_mc.h>
#include <video/telechips/vioc_config.h>

#ifdef CONFIG_VIOC_MAP_DECOMP
#include <video/telechips/tcc_video_private.h>
#include <video/telechips/tca_map_converter.h>
#endif

#define SCALER_DEBUG   0
#if SCALER_DEBUG
#define dprintk(msg...) (void)pr_info("[DBG][SCALER] " msg)
#else
#define dprintk(msg...)
#endif

#define TCC_PA_VIOC_CFGINT	(HwVIOC_BASE + 0xA000)

struct scaler_data {
	// wait for poll
	wait_queue_head_t	poll_wq;
	spinlock_t		poll_lock;
	unsigned int		poll_count;

	// wait for ioctl command
	wait_queue_head_t	cmd_wq;
	spinlock_t		cmd_lock;
	unsigned int		cmd_count;

	struct mutex		io_mutex;
	unsigned char		block_operating;
	unsigned char		block_waiting;
	unsigned char		irq_reged;
	unsigned int		dev_opened;
};

struct scaler_drv_vioc {
	void __iomem *reg;
	unsigned int id;
};

struct scaler_drv_type {
	struct vioc_intr_type	*vioc_intr;

	unsigned int		id;
	unsigned int		irq;

	struct miscdevice	*misc;

	struct scaler_drv_vioc rdma;
	struct scaler_drv_vioc wmix;
	struct scaler_drv_vioc sc;
	struct scaler_drv_vioc wdma;
#if defined(CONFIG_VIOC_MAP_DECOMP)
	struct scaler_drv_vioc mc;
#endif

	struct clk			*pclk;
	struct scaler_data	*data;
	struct SCALER_TYPE	*info;
	
	unsigned int		wmix_bypass;
};

static int scaler_drv_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret = 0;
	(void)filp;

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	if(vma->vm_end >= vma->vm_start) {
		if ((bool)remap_pfn_range(vma, vma->vm_start,
			vma->vm_pgoff, vma->vm_end - vma->vm_start,
			vma->vm_page_prot)) {
			ret = -EAGAIN;
			goto err_remap_pfn_range;
		}
	}

	vma->vm_ops = NULL;
	vma->vm_flags |= (unsigned long)VM_IO;
	vma->vm_flags |= (unsigned long)VM_DONTEXPAND | (unsigned long)VM_PFNMAP;

err_remap_pfn_range:
	return ret;
}

static int tcc_scaler_run(const struct scaler_drv_type *scaler)
{
	int ret = 0;
	unsigned int pSrcBase0 = 0, pSrcBase1 = 0, pSrcBase2 = 0;
	unsigned int y2r = 0U;
	unsigned long flags = 0U;

	void __iomem *pSC_RDMABase = scaler->rdma.reg;
	void __iomem *pSC_WMIXBase = scaler->wmix.reg;
	void __iomem *pSC_WDMABase = scaler->wdma.reg;
	void __iomem *pSC_SCALERBase = scaler->sc.reg;

	dprintk("%s():  IN.\n", __func__);

#if !defined(CONFIG_ARCH_TCC750X)
	if (VIOC_CONFIG_GetViqeDeintls_PluginToRDMA(get_vioc_index(scaler->rdma.id)) >= 0) {
			scaler->info->src_winBottom = ((scaler->info->src_winBottom >> 2) << 2);
	}
#endif

	spin_lock_irqsave(&(scaler->data->cmd_lock), flags);

	dprintk(
		"scaler %d src   add : 0x%x 0x%x 0x%x, fmt :0x%x IMG:(%d %d)%d %d %d %d\n",
		scaler->sc.id, scaler->info->src_Yaddr, scaler->info->src_Uaddr,
		scaler->info->src_Vaddr, scaler->info->src_fmt,
		scaler->info->src_ImgWidth, scaler->info->src_ImgHeight,
		scaler->info->src_winLeft, scaler->info->src_winTop,
		scaler->info->src_winRight, scaler->info->src_winBottom);
	dprintk(
		"scaler %d dest  add : 0x%x 0x%x 0x%x, fmt :0x%x IMG:(%d %d)%d %d %d %d\n",
		scaler->sc.id, scaler->info->dest_Yaddr,
		scaler->info->dest_Uaddr,
		scaler->info->dest_Vaddr,
		scaler->info->dest_fmt,
		scaler->info->dest_ImgWidth,
		scaler->info->dest_ImgHeight,
		scaler->info->dest_winLeft,
		scaler->info->dest_winTop,
		scaler->info->dest_winRight,
		scaler->info->dest_winBottom);
	dprintk(
		"scaler %d interlace:%d\n",
		scaler->sc.id,  scaler->info->interlaced);

#ifdef CONFIG_VIOC_MAP_DECOMP
	if (scaler->info->mapConv_info.m_CompressedY[0] != 0) {
		if (scaler->mc.id == VIOC_NO_COMPONENT) {
			(void)pr_err("[ERR][SCALER] %s(): scaler%d MC Component Empty.\n",__func__,get_vioc_index(scaler->sc.id));
			ret = -EINVAL;
		} else {
			dprintk(
				"scaler %d path-id(rdma:%d)\n",
				scaler->sc.id, scaler->rdma.id);
			dprintk(
				"scaler %d src  map converter: size: %d %d pos:%d %d\n",
				scaler->sc.id,
				scaler->info->src_winRight - scaler->info->src_winLeft,
				scaler->info->src_winBottom - scaler->info->src_winTop,
				scaler->info->src_winLeft, scaler->info->src_winTop);

			(void)VIOC_CONFIG_MCPath(scaler->rdma.id, scaler->mc.id);

			if (scaler->info->dest_fmt < clamp_t(u32,(SCLAER_COMPRESS_DATA), 0, INT_MAX)) {
				y2r = 1U;
			}

			// scaler limitation
			tca_map_convter_driver_set(
				scaler->mc.id,
				scaler->info->src_ImgWidth,
				scaler->info->src_ImgHeight,
				scaler->info->src_winLeft,
				scaler->info->src_winTop,
				scaler->info->src_winRight - scaler->info->src_winLeft,
				scaler->info->src_winBottom - scaler->info->src_winTop,
				y2r, &scaler->info->mapConv_info);

			tca_map_convter_onoff(scaler->mc.id, 1, 0);

		}
	} else
#endif
	{
		VIOC_RDMA_SetImageAlphaSelect(pSC_RDMABase, 1);
		VIOC_RDMA_SetImageAlphaEnable(pSC_RDMABase, 1);
		VIOC_RDMA_SetImageFormat(pSC_RDMABase, scaler->info->src_fmt);

		//interlaced frame process ex) MPEG2
		if (scaler->info->interlaced != 0U) {
			VIOC_RDMA_SetImageSize(
				pSC_RDMABase,
				(scaler->info->src_winRight -
				scaler->info->src_winLeft),
				(scaler->info->src_winBottom -
				scaler->info->src_winTop)/2U);
			VIOC_RDMA_SetImageOffset(
				pSC_RDMABase, scaler->info->src_fmt,
				scaler->info->src_ImgWidth*2U);
		} else {
			VIOC_RDMA_SetImageSize(
				pSC_RDMABase,
				(scaler->info->src_winRight -
				scaler->info->src_winLeft),
				(scaler->info->src_winBottom -
				scaler->info->src_winTop));

				VIOC_RDMA_SetImageOffset(
					pSC_RDMABase, scaler->info->src_fmt,
					scaler->info->src_ImgWidth);
		}

		pSrcBase0 = (unsigned int)scaler->info->src_Yaddr;
		pSrcBase1 = (unsigned int)scaler->info->src_Uaddr;
		pSrcBase2 = (unsigned int)scaler->info->src_Vaddr;

		scaler->info->src_winLeft =
			(scaler->info->src_winLeft>>3)<<3;
		scaler->info->src_winRight =
			scaler->info->src_winLeft +
			(scaler->info->src_winRight -
			scaler->info->src_winLeft);
		tcc_get_addr_yuv(
			scaler->info->src_fmt,
			(unsigned int)scaler->info->src_Yaddr,
			scaler->info->src_ImgWidth,
			scaler->info->src_ImgHeight,
			scaler->info->src_winLeft,
			scaler->info->src_winTop,
			&pSrcBase0, &pSrcBase1, &pSrcBase2);

		if ((scaler->info->src_fmt > VIOC_IMG_FMT_COMP)
			&& (scaler->info->dest_fmt < VIOC_IMG_FMT_COMP)) {
			y2r = 1U;
		}
		VIOC_RDMA_SetImageY2REnable(pSC_RDMABase, y2r);

		if (scaler->info->src_fmt < VIOC_IMG_FMT_COMP) {
			VIOC_RDMA_SetImageRGBSwapMode(
				pSC_RDMABase, scaler->info->src_rgb_swap);
		} else {
			VIOC_RDMA_SetImageRGBSwapMode(pSC_RDMABase, 0);
		}

		VIOC_RDMA_SetImageBase(
			pSC_RDMABase, (unsigned int)pSrcBase0,
			(unsigned int)pSrcBase1, (unsigned int)pSrcBase2);
		VIOC_RDMA_SetImageEnable(pSC_RDMABase); // SoC guide info.
	}

	if (ret == 0) {
		// look up table use
		if (scaler->info->lut.use_lut != 0U) {
			(void)tcc_set_lut_plugin(
				VIOC_LUT + scaler->info->lut.use_lut_number,
				scaler->rdma.id);
			tcc_set_lut_enable(
				VIOC_LUT + scaler->info->lut.use_lut_number,
				1U);
		}

		VIOC_SC_SetBypass(pSC_SCALERBase, 0);

#if defined(CONFIG_MC_WORKAROUND)
		if (!system_rev && scaler->info->mapConv_info.m_CompressedY[0] != 0) {
			unsigned int plus_height =
				VIOC_SC_GetPlusSize(
					(scaler->info->src_winBottom -
					scaler->info->src_winTop),
					(scaler->info->dest_winBottom -
					scaler->info->dest_winTop));

			VIOC_SC_SetDstSize(
				pSC_SCALERBase,
				(scaler->info->dest_winRight -
				scaler->info->dest_winLeft),
				(scaler->info->dest_winBottom -
				scaler->info->dest_winTop) + plus_height);
		} else 
#endif
		{
			VIOC_SC_SetDstSize(
				pSC_SCALERBase,
				(scaler->info->dest_winRight -
				scaler->info->dest_winLeft),
				(scaler->info->dest_winBottom -
				scaler->info->dest_winTop));
			VIOC_SC_SetOutSize(
				pSC_SCALERBase,
				(scaler->info->dest_winRight -
				scaler->info->dest_winLeft),
				(scaler->info->dest_winBottom -
				scaler->info->dest_winTop));
			VIOC_SC_SetOutPosition(pSC_SCALERBase, 0, 0);
			(void)VIOC_CONFIG_PlugIn(scaler->sc.id, scaler->rdma.id);
			VIOC_SC_SetUpdate(pSC_SCALERBase);

			if (scaler->wmix_bypass == 0U) {
				VIOC_WMIX_SetSize(
					pSC_WMIXBase,
					(scaler->info->dest_winRight - scaler->info->dest_winLeft),
					(scaler->info->dest_winBottom - scaler->info->dest_winTop));
				VIOC_WMIX_SetUpdate(pSC_WMIXBase);
			}
		}

		VIOC_WDMA_SetImageFormat(pSC_WDMABase, scaler->info->dest_fmt);

		if (scaler->info->dest_fmt < VIOC_IMG_FMT_COMP) {
			VIOC_WDMA_SetImageRGBSwapMode(pSC_WDMABase,scaler->info->dst_rgb_swap);
		} else {
			VIOC_WDMA_SetImageRGBSwapMode(pSC_WDMABase, 0);
		}

		VIOC_WDMA_SetImageSize(
			pSC_WDMABase,
			(scaler->info->dest_winRight - scaler->info->dest_winLeft),
			(scaler->info->dest_winBottom - scaler->info->dest_winTop));

		VIOC_WDMA_SetImageOffset(pSC_WDMABase,
			scaler->info->dest_fmt, scaler->info->dest_ImgWidth);

		VIOC_WDMA_SetImageBase(
			pSC_WDMABase, (unsigned int)scaler->info->dest_Yaddr,
			(unsigned int)scaler->info->dest_Uaddr,
			(unsigned int)scaler->info->dest_Vaddr);
		if ((scaler->info->src_fmt < VIOC_IMG_FMT_COMP) &&
			(scaler->info->dest_fmt > VIOC_IMG_FMT_COMP)) {
			VIOC_WDMA_SetImageR2YEnable(pSC_WDMABase, 1);
		} else {
			VIOC_WDMA_SetImageR2YEnable(pSC_WDMABase, 0);
		}

		VIOC_WDMA_SetImageEnable(pSC_WDMABase, 0);
		VIOC_WDMA_SetIreqStatus(pSC_WDMABase, VIOC_WDMA_IREQ_ALL_MASK);
	}

	spin_unlock_irqrestore(&(scaler->data->cmd_lock), flags);

	if(ret == 0) {
		if (scaler->info->responsetype  == SCALER_POLLING) {
			ret = wait_event_interruptible_timeout(scaler->data->poll_wq, scaler->data->block_operating == 0U, msecs_to_jiffies(500));
			if (ret <= 0) {
				scaler->data->block_operating = 0;
				(void)pr_err("[ERR][SCALER] %s():  time out(%d), line(%d).\n",__func__, ret, __LINE__);
				ret = -EINTR;
			}
			// look up table use
			if (scaler->info->lut.use_lut != 0U) {
				tcc_set_lut_enable(
					VIOC_LUT + scaler->info->lut.use_lut_number,
					0U);
				scaler->info->lut.use_lut = 0U;
			}
		} else if (scaler->info->responsetype == SCALER_NOWAIT) {
			if ((scaler->info->viqe_onthefly & 0x2U) != 0U) {
				scaler->data->block_operating = 0U;
			}
		}
	}

	return ret;
}

static int tcc_scaler_data_copy_run(const struct scaler_drv_type *scaler, const struct SCALER_DATA_COPY_TYPE *copy_info)
{
	int ret = 0;
	unsigned long flags = 0U;
	void __iomem *pSC_RDMABase = scaler->rdma.reg;
	void __iomem *pSC_WMIXBase = scaler->wmix.reg;
	void __iomem *pSC_WDMABase = scaler->wdma.reg;
	void __iomem *pSC_SCALERBase = scaler->sc.reg;

	dprintk("%s():\n", __func__);
	dprintk(
		"Src  : addr:0x%x 0x%x 0x%x  fmt:%d\n",
		copy_info->src_y_addr, copy_info->src_u_addr,
		copy_info->src_v_addr, copy_info->src_fmt);
	dprintk(
		"Dest: addr:0x%x 0x%x 0x%x  fmt:%d\n",
		copy_info->dst_y_addr, copy_info->dst_u_addr,
		copy_info->dst_v_addr, copy_info->dst_fmt);
	dprintk(
		"Size : W:%d  H:%d\n", copy_info->img_width,
		copy_info->img_height);


	spin_lock_irqsave(&(scaler->data->cmd_lock), flags);

	VIOC_RDMA_SetImageFormat(pSC_RDMABase,
		copy_info->src_fmt);
	VIOC_RDMA_SetImageSize(pSC_RDMABase,
		copy_info->img_width, copy_info->img_height);
	VIOC_RDMA_SetImageOffset(pSC_RDMABase,
		copy_info->src_fmt, copy_info->img_width);
	VIOC_RDMA_SetImageBase(pSC_RDMABase,
		(unsigned int)copy_info->src_y_addr,
		(unsigned int)copy_info->src_u_addr,
		(unsigned int)copy_info->src_v_addr);

	VIOC_SC_SetBypass(pSC_SCALERBase, 0U);
	VIOC_SC_SetDstSize(pSC_SCALERBase,
		copy_info->img_width, copy_info->img_height);
	VIOC_SC_SetOutSize(pSC_SCALERBase,
		copy_info->img_width, copy_info->img_height);
	VIOC_SC_SetOutPosition(pSC_SCALERBase, 0, 0);
	(void)VIOC_CONFIG_PlugIn(scaler->sc.id, scaler->rdma.id);
	VIOC_SC_SetUpdate(pSC_SCALERBase);
	VIOC_RDMA_SetImageEnable(pSC_RDMABase); // SoC guide info.

	if (scaler->wmix_bypass == 0U) {
		VIOC_WMIX_SetSize(pSC_WMIXBase,
			copy_info->img_width, copy_info->img_height);
		VIOC_WMIX_SetUpdate(pSC_WMIXBase);
	}

	VIOC_WDMA_SetImageFormat(pSC_WDMABase,
		copy_info->dst_fmt);
	VIOC_WDMA_SetImageSize(pSC_WDMABase,
		copy_info->img_width, copy_info->img_height);
	VIOC_WDMA_SetImageOffset(pSC_WDMABase,
		copy_info->dst_fmt, copy_info->img_width);
	VIOC_WDMA_SetImageBase(pSC_WDMABase,
		(unsigned int)copy_info->dst_y_addr,
		(unsigned int)copy_info->dst_u_addr,
		(unsigned int)copy_info->dst_v_addr);
	VIOC_WDMA_SetImageEnable(pSC_WDMABase, 0/*OFF*/);
	VIOC_WDMA_SetIreqStatus(pSC_WDMABase, VIOC_WDMA_IREQ_ALL_MASK);
	// wdma status register all clear.

	spin_unlock_irqrestore(&(scaler->data->cmd_lock), flags);

	if (copy_info->rsp_type == (unsigned int)SCALER_POLLING) {
		ret = wait_event_interruptible_timeout(
			scaler->data->poll_wq,
			scaler->data->block_operating == 0,
			msecs_to_jiffies(500));
		if (ret <= 0) {
			scaler->data->block_operating = 0;
			(void)pr_warn(
				"[WAR][SCALER] wmixer time out: %d, Line: %d.\n",
				ret, __LINE__);
		}
	}

	return ret;
}

static unsigned int scaler_drv_poll(struct file *filp, poll_table *wait)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct scaler_drv_type *scaler = dev_get_drvdata(misc->parent);
	unsigned int ret = 0U;
	unsigned long flags = 0U;

	if (scaler->data == NULL) {
		goto err_poll;
	}

	poll_wait(filp, &(scaler->data->poll_wq), wait);
	spin_lock_irqsave(&(scaler->data->poll_lock), flags);
	if (scaler->data->block_operating == 0U) {
		ret = ((unsigned int)POLLIN|(unsigned int)POLLRDNORM);
	}
	spin_unlock_irqrestore(&(scaler->data->poll_lock), flags);

err_poll:
	return ret;
}

static irqreturn_t scaler_drv_handler(int irq, void *client_data)
{
	irqreturn_t ret;
	const struct scaler_drv_type *scaler =
		(struct scaler_drv_type *)client_data;

	(void)irq;

	if (is_vioc_intr_activatied(
		clamp_t(s32, (scaler->vioc_intr->id), 0 ,INT_MAX),
		scaler->vioc_intr->bits) == false) {
			ret = IRQ_NONE;
			goto err_handler;
	}
	(void)vioc_intr_clear(
		clamp_t(s32, (scaler->vioc_intr->id), 0 ,INT_MAX),
		scaler->vioc_intr->bits);

	dprintk(
		"%s(): block_operating(%d), block_waiting(%d), cmd_count(%d), poll_count(%d).\n",
		__func__, scaler->data->block_operating,
		scaler->data->block_waiting,
		scaler->data->cmd_count,
		scaler->data->poll_count);

	if (scaler->data->block_operating >= 1U) {
		scaler->data->block_operating = 0U;
	}

	wake_up_interruptible(&(scaler->data->poll_wq));

	if (scaler->data->block_waiting != 0U) {
		wake_up_interruptible(&scaler->data->cmd_wq);
	}

	// look up table use
	if (scaler->info->lut.use_lut != 0U)		{
		tcc_set_lut_enable(
			VIOC_LUT + scaler->info->lut.use_lut_number,
			0U);
		scaler->info->lut.use_lut = 0U;
	}

	#if defined(CONFIG_VIOC_MAP_DECOMP)
	if (scaler->mc.id != VIOC_NO_COMPONENT) {
			(void)VIOC_CONFIG_MCPath(scaler->rdma.id, scaler->rdma.id); // MC is not used.
	}
	#endif

	ret = IRQ_HANDLED;

err_handler:
	return ret;

}

static long scaler_drv_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct scaler_drv_type	*scaler = dev_get_drvdata(misc->parent);

	struct SCALER_DATA_COPY_TYPE copy_info;
	int ret = 0;

	dprintk(
		"%s(): cmd(%d), block_operating(%d), block_waiting(%d), cmd_count(%d), poll_count(%d).\n",
		__func__, cmd, scaler->data->block_operating,
		scaler->data->block_waiting,
		scaler->data->cmd_count,
		scaler->data->poll_count);

	switch (cmd) {
	case TCC_SCALER_IOCTRL:
	case TCC_SCALER_IOCTRL_KERENL:
		mutex_lock(&scaler->data->io_mutex);
		if (scaler->data->block_operating != 0U) {
			scaler->data->block_waiting = 1U;
			ret = wait_event_interruptible_timeout(
				scaler->data->cmd_wq,
				scaler->data->block_operating == 0,
				msecs_to_jiffies(200));
			if (ret <= 0) {
				scaler->data->block_operating = 0;
				(void)pr_warn(
					"[WAR][SCALER] %s(%d): timed_out block_operation(%d), cmd_count(%d).\n",
					__func__, ret,
					scaler->data->block_waiting,
					scaler->data->cmd_count);
			}
			ret = 0;
		}

		if (cmd == TCC_SCALER_IOCTRL_KERENL) {
			(void)memcpy(scaler->info,(struct SCALER_TYPE *)arg, sizeof(struct SCALER_TYPE));
		} else {
			if ((bool)copy_from_user(scaler->info, (struct SCALER_TYPE *)arg, sizeof(struct SCALER_TYPE))) {
				(void)pr_err(
					"[ERR][SCALER] %s(): Not Supported copy_from_user(%d).\n",
					__func__, cmd);
				ret = -EFAULT;
			}
		}

		if (ret >= 0) {
			if (scaler->data->block_operating >= 1U) {
				(void)pr_info(
					"[INF][SCALER] %s(): block_operating(%d), block_waiting(%d), cmd_count(%d), poll_count(%d).\n",
					__func__, scaler->data->block_operating,
					scaler->data->block_waiting,
					scaler->data->cmd_count,
					scaler->data->poll_count);
			}

//				convert_image_format(scaler);

			scaler->data->block_waiting = 0U;
			scaler->data->block_operating = 1U;
			ret = tcc_scaler_run(scaler);
			if (ret < 0) {
				scaler->data->block_operating = 0U;
			}
		}
		mutex_unlock(&scaler->data->io_mutex);
		break;

	case TCC_SCALER_VIOC_DATA_COPY:
		mutex_lock(&scaler->data->io_mutex);
		if (scaler->data->block_operating != 0U) {
			scaler->data->block_waiting = 1U;
			ret = wait_event_interruptible_timeout(
				scaler->data->cmd_wq,
				scaler->data->block_operating == 0U,
				msecs_to_jiffies(200));
			if (ret <= 0) {
				scaler->data->block_operating = 0;
				(void)pr_warn(
					"[WAR][SCALER] %s(%d): wmixer 0 timed_out block_operation(%d), cmd_count(%d).\n",
					__func__, ret,
					scaler->data->block_waiting,
					scaler->data->cmd_count);
			}
			ret = 0;
		}

		if ((bool)copy_from_user(&copy_info, (struct SCALER_DATA_COPY_TYPE *)arg, sizeof(struct SCALER_DATA_COPY_TYPE))) {
			(void)pr_err(
				"[ERR][SCALER] %s(): Not Supported copy_from_user(%d)\n",
				__func__, cmd);
			ret = -EFAULT;
		}

		if (ret >= 0) {
			if (scaler->data->block_operating >= 1U) {
				(void)pr_info("[INF][SCALER] %s(): block_operating(%d), block_waiting(%d), cmd_count(%d), poll_count(%d).\n",
				__func__, scaler->data->block_operating,
				scaler->data->block_waiting,
				scaler->data->cmd_count,
				scaler->data->poll_count);
			}

			scaler->data->block_waiting = 0U;
			scaler->data->block_operating = 1U;
			ret = tcc_scaler_data_copy_run(scaler, &copy_info);
			if (ret < 0) {
				scaler->data->block_operating = 0U;
			}
		}
		mutex_unlock(&scaler->data->io_mutex);
		break;

	default:
		(void)pr_err(
			"[ERR][SCALER] %s(): Not Supported SCALER0_IOCTL(%d).\n",
			__func__, cmd);
		break;
	}

	return 0;
}

#ifdef CONFIG_COMPAT
static long scaler_drv_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return scaler_drv_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static int scaler_drv_release(struct inode *pinode, struct file *filp)
{
	int ret = 0;
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct scaler_drv_type	*scaler = dev_get_drvdata(misc->parent);

	(void)pinode;

	dprintk(
		"%s(): In -release(%d), block(%d), wait(%d), cmd(%d), irq(%d)\n",
		__func__, scaler->data->dev_opened,
		scaler->data->block_operating,
		scaler->data->block_waiting,
		scaler->data->cmd_count,
		scaler->data->irq_reged);

	if (scaler->data->dev_opened > 0U) {
		scaler->data->dev_opened--;
	}

	if (scaler->data->dev_opened == 0U) {
		if (scaler->data->block_operating != 0U) {
			ret = wait_event_interruptible_timeout(
				scaler->data->cmd_wq,
				scaler->data->block_operating == 0U,
				msecs_to_jiffies(200));
			if (ret <= 0) {
				(void)pr_warn(
					 "[WAR][SCALER] %s(%d): timed_out block_operation:%d, cmd_count:%d.\n",
					 __func__, ret,
					 scaler->data->block_waiting,
					 scaler->data->cmd_count);
			}
		}

		if ((bool)scaler->data->irq_reged) {
			(void)vioc_intr_clear(clamp_t(s32, (scaler->vioc_intr->id), 0, INT_MAX), scaler->vioc_intr->bits);
			(void)vioc_intr_disable(clamp_t(s32, (scaler->irq), 0, INT_MAX),
				clamp_t(s32, (scaler->vioc_intr->id), 0, INT_MAX),
				scaler->vioc_intr->bits);
			(void)irq_set_affinity_hint(scaler->irq, NULL);
			(void)free_irq(scaler->irq, scaler);
			scaler->data->irq_reged = 0;
		}

		(void)VIOC_CONFIG_PlugOut(scaler->sc.id);

		#if defined(CONFIG_VIOC_MAP_DECOMP)
		if (scaler->mc.id != VIOC_NO_COMPONENT) {
			(void)VIOC_CONFIG_MCPath(scaler->rdma.id, scaler->rdma.id); // MC is not used.
		}
		#endif

		VIOC_CONFIG_SWReset(scaler->wdma.id, VIOC_CONFIG_RESET);
		if (scaler->wmix_bypass == 0U) {
			VIOC_CONFIG_SWReset(scaler->wmix.id, VIOC_CONFIG_RESET);
		}
		VIOC_CONFIG_SWReset(scaler->sc.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->rdma.id, VIOC_CONFIG_RESET);

		VIOC_CONFIG_SWReset(scaler->rdma.id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->sc.id, VIOC_CONFIG_CLEAR);
		if (scaler->wmix_bypass == 0U) {
			VIOC_CONFIG_SWReset(scaler->wmix.id, VIOC_CONFIG_CLEAR);
		}
		VIOC_CONFIG_SWReset(scaler->wdma.id, VIOC_CONFIG_CLEAR);

		scaler->data->block_operating = 0U;
		scaler->data->block_waiting = 0U;
		scaler->data->poll_count = 0U;
		scaler->data->cmd_count = 0U;
	}

	if (scaler->pclk != NULL) {
		clk_disable_unprepare(scaler->pclk);
	}
	dprintk(
		"%s():  Out - release(%d).\n",
		__func__, scaler->data->dev_opened);

	return ret;
}

static int scaler_drv_open(struct inode *pinode, struct file *filp)
{
	struct miscdevice	*misc = (struct miscdevice *)filp->private_data;
	struct scaler_drv_type	*scaler = dev_get_drvdata(misc->parent);

	int ret = 0;
	(void)pinode;

	dprintk(
		"%s():  In -open(%d), block(%d), wait(%d), cmd(%d), irq(%d :: %d/0x%x)\n",
		__func__, scaler->data->dev_opened,
		scaler->data->block_operating,
		scaler->data->block_waiting, scaler->data->cmd_count,
		scaler->data->irq_reged,
		scaler->vioc_intr->id, scaler->vioc_intr->bits);

	if (scaler->pclk != NULL) {
		(void)clk_prepare_enable(scaler->pclk);
	}

	if (!(bool)scaler->data->irq_reged) {
		VIOC_CONFIG_SWReset(scaler->wdma.id, VIOC_CONFIG_RESET);
		if (scaler->wmix_bypass == 0U) {
			VIOC_CONFIG_SWReset(scaler->wmix.id, VIOC_CONFIG_RESET);
		}
		VIOC_CONFIG_SWReset(scaler->sc.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->rdma.id, VIOC_CONFIG_RESET);
		#ifdef CONFIG_VIOC_MAP_DECOMP
		if (scaler->mc.id != VIOC_NO_COMPONENT) {
			tca_map_convter_swreset(scaler->mc.id);
		}
		#endif
		VIOC_CONFIG_SWReset(scaler->rdma.id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->sc.id, VIOC_CONFIG_CLEAR);
		if (scaler->wmix_bypass == 0U) {
			VIOC_CONFIG_SWReset(scaler->wmix.id, VIOC_CONFIG_CLEAR);
		}
		VIOC_CONFIG_SWReset(scaler->wdma.id, VIOC_CONFIG_CLEAR);

		synchronize_irq(scaler->irq);
		(void)vioc_intr_clear(clamp_t(s32, (scaler->vioc_intr->id), 0, INT_MAX), scaler->vioc_intr->bits);
		ret = request_irq(scaler->irq, scaler_drv_handler, IRQF_SHARED, scaler->misc->name, scaler);
		if ((bool)ret) {
			if (scaler->pclk != NULL) {
				clk_disable_unprepare(scaler->pclk);
			}
			(void)pr_err("[ERR][SCALER] failed(ret %d) to aquire %s request_irq(%d).\n", ret, scaler->misc->name, scaler->irq);
			ret = -EFAULT;
			goto err_scaler_drv_open;
		}
		dprintk("success(ret %d) to aquire %s request_irq(%d).\n", ret, scaler->misc->name, scaler->irq);
		(void)vioc_intr_enable(
			clamp_t(s32, (scaler->irq), 0, INT_MAX),
			clamp_t(s32, (scaler->vioc_intr->id), 0, INT_MAX), scaler->vioc_intr->bits);

#if defined(CONFIG_VIOC_MAP_DECOMP)
		if (scaler->mc.id != VIOC_NO_COMPONENT) {
			(void)VIOC_CONFIG_MCPath(scaler->rdma.id, scaler->rdma.id); // MC is not used.
		}
#endif
		scaler->data->irq_reged = 1;
	}

	scaler->data->dev_opened++;
	dprintk(
		"%s():  Out - open(%d).\n",
		__func__, scaler->data->dev_opened);

err_scaler_drv_open:
	return ret;
}

static const struct file_operations scaler_drv_fops = {
	.owner			= THIS_MODULE,
	.unlocked_ioctl		= scaler_drv_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= scaler_drv_compat_ioctl,
#endif
	.mmap			= scaler_drv_mmap,
	.open			= scaler_drv_open,
	.release		= scaler_drv_release,
	.poll			= scaler_drv_poll,
};

static int scaler_drv_probe(struct platform_device *pdev)
{
	struct scaler_drv_type *scaler;
	struct device_node *dev_np;
	unsigned int index;
	int ret = -ENODEV;
	int itemp;

	scaler = kzalloc(sizeof(struct scaler_drv_type), GFP_KERNEL);
	if (scaler == NULL) {
		goto err_misc_alloc;
	}

	scaler->pclk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(scaler->pclk)) {
		scaler->pclk = NULL;
	}

	scaler->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (scaler->misc == NULL) {
		goto err_misc_alloc;
	}

	scaler->info = kzalloc(sizeof(struct SCALER_TYPE), GFP_KERNEL);
	if (scaler->info == NULL) {
		goto err_info_alloc;
	}

	scaler->data = kzalloc(sizeof(struct scaler_data), GFP_KERNEL);
	if (scaler->data == NULL) {
		goto err_data_alloc;
	}

	scaler->vioc_intr = kzalloc(sizeof(struct vioc_intr_type), GFP_KERNEL);
	if (scaler->vioc_intr == NULL) {
		goto err_vioc_intr_alloc;
	}

	itemp = of_alias_get_id(pdev->dev.of_node, "scaler-drv");
	if(itemp >= 0) {
		scaler->id = (unsigned int)itemp;
	} else {
		scaler->id = 0;
	}

	/* register scaler discdevice */
	scaler->misc->minor = MISC_DYNAMIC_MINOR;
	scaler->misc->fops = &scaler_drv_fops;
	scaler->misc->name =
		kasprintf(GFP_KERNEL, "scaler%d", scaler->id);
	scaler->misc->parent = &pdev->dev;

	ret = misc_register(scaler->misc);
	if ((bool)ret) {
		goto err_misc_register;
	}

	dev_np = of_parse_phandle(
		pdev->dev.of_node, "rdmas", 0);
	if (dev_np != NULL) {
		(void)of_property_read_u32_index(
			pdev->dev.of_node, "rdmas", 1, &index);
		scaler->rdma.reg = VIOC_RDMA_GetAddress(index);
		scaler->rdma.id = index;
	} else {
		(void)pr_warn(
			"[WAR][SCALER] could not find rdma node of %s driver.\n",
			scaler->misc->name);
		scaler->rdma.reg = NULL;
	}

	dev_np = of_parse_phandle(
		pdev->dev.of_node, "scalers", 0);
	if (dev_np != NULL) {
		(void)of_property_read_u32_index(
			pdev->dev.of_node, "scalers", 1, &index);
		scaler->sc.reg = VIOC_SC_GetAddress(index);
		scaler->sc.id = index;
	} else {
		(void)pr_warn(
			"[WAR][SCALER] could not find scaler node of %s driver.\n",
			scaler->misc->name);
		scaler->sc.reg = NULL;
	}

	dev_np = of_parse_phandle(
		pdev->dev.of_node, "wmixs", 0);
	if (dev_np != NULL) {
		(void)of_property_read_u32_index(
			pdev->dev.of_node, "wmixs", 1, &index);
		scaler->wmix.reg = VIOC_WMIX_GetAddress(index);
		scaler->wmix.id = index;
	} else {
		(void)pr_warn(
			"[WAR][SCALER] could not find wmix node of %s driver.\n",
			scaler->misc->name);
		scaler->wmix.reg = NULL;
	}

	dev_np = of_parse_phandle(pdev->dev.of_node, "wdmas", 0);
	if (dev_np != NULL) {
		unsigned int uitemp;
		(void)of_property_read_u32_index(pdev->dev.of_node, "wdmas", 1, &index);
		scaler->wdma.reg = VIOC_WDMA_GetAddress(index);
		scaler->wdma.id = index;

		/* get irq no. */
		uitemp = get_vioc_index(index);
		itemp = clamp_t(s32,(uitemp), 0, INT_MAX);
		scaler->irq = irq_of_parse_and_map(dev_np, itemp);

		/* get vioc_intr no. */
		#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)
			switch (scaler->wdma.id) {
			case VIOC_WDMA00:
			case VIOC_WDMA01:
			case VIOC_WDMA02:
			case VIOC_WDMA03:
			case VIOC_WDMA04:
			case VIOC_WDMA05:
			case VIOC_WDMA06:
			case VIOC_WDMA07:
			case VIOC_WDMA08:
				uitemp = clamp_t(u32, (VIOC_INTR_WD0), 0, INT_MAX) + get_vioc_index(scaler->wdma.id);
				break;

			case VIOC_WDMA09:
			case VIOC_WDMA10:
			case VIOC_WDMA11:
			#if defined(CONFIG_ARCH_TCC805X)
			case VIOC_WDMA12:
			#endif
				uitemp = clamp_t(u32, (VIOC_INTR_WD9), 0, INT_MAX) + get_vioc_index((scaler->wdma.id - VIOC_WDMA09));
				break;

			#if defined(CONFIG_ARCH_TCC805X)
			case VIOC_WDMA13:
				uitemp = clamp_t(u32, (VIOC_INTR_WD13), 0, INT_MAX) + get_vioc_index((scaler->wdma.id - VIOC_WDMA13));
				break;
			#endif

			default:
				pr_err("[ERR][SCALER] %s: scaler%d: invalid wdma id(0x%x)\n",
					__func__, scaler->id, scaler->wdma.id);
				break;
			}
		#else
			/* TCC750x, TCC807x */
			uitemp = clamp_t(u32, (VIOC_INTR_WD0), 0, INT_MAX) + get_vioc_index(scaler->wdma.id);
		#endif

		scaler->vioc_intr->id = uitemp;
		scaler->vioc_intr->bits = VIOC_WDMA_IREQ_EOFR_MASK;
	} else {
		(void)pr_warn("[WAR][SCALER] could not find wdma node of %s driver.\n", scaler->misc->name);
		scaler->wdma.reg = NULL;
	}

	#if defined(CONFIG_VIOC_MAP_DECOMP)
	dev_np = of_parse_phandle(pdev->dev.of_node, "mc", 0);
	if (dev_np != NULL) {
		(void)of_property_read_u32_index(pdev->dev.of_node, "mc", 1, &index);
		scaler->mc.id = index;
		(void)pr_info("[INF][SCALER] sclaer%d: MC(0x%x)\n", scaler->id, scaler->mc.id);
	} else {
		scaler->mc.id = VIOC_NO_COMPONENT;
		(void)pr_info("[INF][SCALER] sclaer%d: MC Component not used\n", scaler->id);
	}
	#endif

	dev_np = of_parse_phandle(pdev->dev.of_node, "wmix_bypass", 0);
	if (dev_np != NULL) {
		if (of_property_read_u32_index(pdev->dev.of_node, "wmix_bypass", 0, &index) == 0) {
			scaler->wmix_bypass = index;

			if (scaler->wmix_bypass > 0U) {
				//sacler wmix bypass
				(void)VIOC_CONFIG_WMIXPath(scaler->rdma.id, 0);
			}
		}
	} else {
		scaler->wmix_bypass = 0U;
	}
	(void)pr_info("[INF][SCALER]%d: wmix(0x%x) %s mode\n", scaler->id, scaler->wmix.id, (scaler->wmix_bypass != 0U) ? "bypass" : "mixing");

	spin_lock_init(&(scaler->data->poll_lock));
	spin_lock_init(&(scaler->data->cmd_lock));

	mutex_init(&(scaler->data->io_mutex));

	init_waitqueue_head(&(scaler->data->poll_wq));
	init_waitqueue_head(&(scaler->data->cmd_wq));

	platform_set_drvdata(pdev, scaler);

	(void)pr_info("[INF][SCALER] scaler%d: RDMA%d-SC%d-WMIX%d-WDMA%d\n",
			scaler->id,
			get_vioc_index(scaler->rdma.id),
			get_vioc_index(scaler->sc.id),
			get_vioc_index(scaler->wmix.id),
			get_vioc_index(scaler->wdma.id));

	ret = 0;
	goto probe_return;

err_misc_register:
	kfree(scaler->vioc_intr);
err_vioc_intr_alloc:
	kfree(scaler->data);
err_data_alloc:
	kfree(scaler->info);
err_info_alloc:
	kfree(scaler->misc);
err_misc_alloc:
	kfree(scaler);

	(void)pr_err(
		"[ERR][SCALER] %s: %s: err ret:%d\n",
		__func__, pdev->name, ret);
probe_return:
	return ret;
}

static int scaler_drv_remove(struct platform_device *pdev)
{
	struct scaler_drv_type *scaler = (struct scaler_drv_type *)platform_get_drvdata(pdev);

	misc_deregister(scaler->misc);
	kfree(scaler->vioc_intr);
	kfree(scaler->data);
	kfree(scaler->info);
	kfree(scaler->misc);
	kfree(scaler);
	return 0;
}

static int scaler_drv_suspend(struct platform_device *pdev, pm_message_t state)
{
	(void)pdev;
	(void)state;
	(void)pr_info("[INF][SCALER] %s\n", __func__);
	return 0;
}

static int scaler_drv_resume(struct platform_device *pdev)
{
	struct scaler_drv_type *scaler = (struct scaler_drv_type *)platform_get_drvdata(pdev);

	if (scaler->data->dev_opened > 0U) {
		VIOC_CONFIG_SWReset(scaler->wdma.id, VIOC_CONFIG_RESET);
		if (scaler->wmix_bypass == 0U) {
			VIOC_CONFIG_SWReset(scaler->wmix.id, VIOC_CONFIG_RESET);
		}
		VIOC_CONFIG_SWReset(scaler->sc.id, VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(scaler->rdma.id, VIOC_CONFIG_RESET);

		VIOC_CONFIG_SWReset(scaler->rdma.id, VIOC_CONFIG_CLEAR);
		VIOC_CONFIG_SWReset(scaler->sc.id, VIOC_CONFIG_CLEAR);
		if (scaler->wmix_bypass == 0U) {
			VIOC_CONFIG_SWReset(scaler->wmix.id, VIOC_CONFIG_CLEAR);
		}
		VIOC_CONFIG_SWReset(scaler->wdma.id, VIOC_CONFIG_CLEAR);
	}

	return 0;
}

static const struct of_device_id scaler_of_match[] = {
	{ .compatible = "telechips,scaler_drv" },
	{}
};
MODULE_DEVICE_TABLE(of, scaler_of_match);

static struct platform_driver scaler_driver = {
	.probe = scaler_drv_probe,
	.remove = scaler_drv_remove,
	.suspend = scaler_drv_suspend,
	.resume = scaler_drv_resume,
	.driver = {
		.name = "scaler_pdev",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table	= of_match_ptr(scaler_of_match),
#endif
	},
};

static int __init scaler_drv_init(void)
{
	return platform_driver_register(&scaler_driver);
}

static void __exit scaler_drv_exit(void)
{
	platform_driver_unregister(&scaler_driver);
}

module_init(scaler_drv_init);
module_exit(scaler_drv_exit);

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("Telechips Scaler Driver");
MODULE_LICENSE("GPL");
