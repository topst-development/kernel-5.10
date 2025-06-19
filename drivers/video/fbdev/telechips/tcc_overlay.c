// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/module.h>
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
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/videodev2.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include <linux/io.h>
#include <asm/div64.h>
#include <linux/poll.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#endif

#include <video/telechips/tcc_overlay_ioctl.h>
#include <video/telechips/tccfb_ioctrl.h>

#if defined(CONFIG_VIOC_MAP_DECOMP)
#include <video/telechips/tca_map_converter.h>
#endif

#include <video/telechips/tcc_types.h>

#include <video/telechips/vioc_outcfg.h>
#include <video/telechips/vioc_rdma.h>
#include <video/telechips/vioc_wdma.h>
#include <video/telechips/vioc_wmix.h>
#include <video/telechips/vioc_scaler.h>
#include <video/telechips/vioc_disp.h>
#include <video/telechips/vioc_global.h>
#include <video/telechips/vioc_config.h>

#if defined(CONFIG_VIOC_AFBCDEC)
#include <video/telechips/vioc_afbcdec.h>
#endif

#ifndef ON
#define ON 1
#endif //

#ifndef OFF
#define OFF 0
#endif //

#ifndef DEBUG
#define dprintk(msg...)
#else
#define dprintk(msg...) (void)pr_info("[DBG][OVERLAY] " msg);
#endif

#define DEFAULT_OVERLAY_N 3U
#define OVERLAY_LAYER_MAX (4U)

struct overlay_drv_vioc {
	void __iomem *reg;
	unsigned int id;
};
struct overlay_drv_type {
	struct miscdevice *misc;
	struct clk *pclk;

	unsigned int layer_n;
	unsigned int layer_nlast;
	struct overlay_drv_vioc rdma[4];
	struct overlay_drv_vioc wmix;
	struct overlay_drv_vioc sc;
#if defined(CONFIG_VIOC_MAP_DECOMP)
	struct overlay_drv_vioc mc;
#endif
#if defined(CONFIG_VIOC_AFBCDEC)
	struct overlay_drv_vioc afbc_dec;
#endif

	// extend infomation
	unsigned int id;
	unsigned int fb_dd_num;
	unsigned int open_cnt;

	// to back up image  infomation.
	overlay_video_buffer_t overBuffCfg;
};

#ifdef CONFIG_VIDEO_DISPLAY_SWAP_VPU_FRAME
extern void tcc_video_frame_backup(struct tcc_lcdc_image_update *Image);	// tcc_vsync.c
#endif

#if defined(CONFIG_VIOC_AFBCDEC)
static void tcc_overlay_configure_AFBCDEC(
	void __iomem *pAFBC_Dec, unsigned int afbc_dec_id,
	void __iomem *pRDMA, unsigned int rdmaPath,
	unsigned int bSet_Comp, unsigned int onthefly, unsigned int bFirst,
	unsigned int base_addr, unsigned int fmt, unsigned int bSplitMode,
	unsigned int bWideMode, unsigned int width, unsigned int height)
{
	if (!pAFBC_Dec)
		return;

	if (bSet_Comp) {
		if (bFirst) {
			VIOC_RDMA_SetImageDisable(pRDMA);
			VIOC_CONFIG_FBCDECPath(afbc_dec_id, rdmaPath, 1);
			VIOC_AFBCDec_SurfaceCfg(
				pAFBC_Dec, base_addr, fmt, width, height, 0,
				bSplitMode, bWideMode, VIOC_AFBCDEC_SURFACE_0,
				1);
			VIOC_AFBCDec_SetContiDecEnable(pAFBC_Dec, onthefly);
			VIOC_AFBCDec_SetSurfaceN(
				pAFBC_Dec, VIOC_AFBCDEC_SURFACE_0, 1);
			VIOC_AFBCDec_SetIrqMask(
				pAFBC_Dec, 0, AFBCDEC_IRQ_ALL); // disable all
		} else {
			VIOC_AFBCDec_SurfaceCfg(
				pAFBC_Dec, base_addr, fmt, width, height, 0,
				bSplitMode, bWideMode, VIOC_AFBCDEC_SURFACE_0,
				0);
		}

		if (onthefly) {
			if (bFirst)
				VIOC_AFBCDec_TurnOn(
					pAFBC_Dec, VIOC_AFBCDEC_SWAP_DIRECT);
			else
				VIOC_AFBCDec_TurnOn(
					pAFBC_Dec, VIOC_AFBCDEC_SWAP_PENDING);
		} else {
			VIOC_AFBCDec_TurnOn(
				pAFBC_Dec, VIOC_AFBCDEC_SWAP_DIRECT);
		}
	} else {
		VIOC_RDMA_SetImageDisable(pRDMA);
		VIOC_AFBCDec_TurnOFF(pAFBC_Dec);
		VIOC_CONFIG_FBCDECPath(afbc_dec_id, rdmaPath, 0);

		//VIOC_CONFIG_SWReset(VIOC_AFBCDEC + dec_num,
		//	VIOC_CONFIG_RESET);
		//VIOC_CONFIG_SWReset(VIOC_AFBCDEC + dec_num,
		//	VIOC_CONFIG_CLEAR);
	}
}
#endif

static int tcc_overlay_mmap(
	struct file *pfile,
	struct vm_area_struct *vma)
{
	int ret = 0;

	(void)pfile;

	if((vma->vm_end > (ULONG_MAX / 2UL)) || (vma->vm_start > (ULONG_MAX / 2UL))) {
		goto exit;
	}

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	if ((bool)remap_pfn_range(
		    vma, vma->vm_start, vma->vm_pgoff,
		    vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		ret = -EAGAIN;
	}

	vma->vm_ops = NULL;
	vma->vm_flags |= (unsigned long)VM_IO;
	vma->vm_flags |= (unsigned long)VM_DONTEXPAND | (unsigned long)VM_DONTDUMP;

exit:
	return ret;
}

static DECLARE_WAIT_QUEUE_HEAD(overlay_wait);

static unsigned int tcc_overlay_poll(struct file *pfile, struct poll_table_struct *wait)
{
	poll_wait(pfile, &(overlay_wait), wait);

	return POLLIN;
}

#if defined(CONFIG_TELECHIPS_SCREEN_SHARE)
static int tcc_overlay_display_shared_screen(
	overlay_shared_buffer_t buffer_cfg,
	struct overlay_drv_type *overlay_drv)
{
	unsigned int layer = 0;
	int ret = 0;

	dprintk("%s addr:0x%x  fmt : 0x%x position:%d %d  size: %d %d\n",
		__func__, buffer_cfg.src_addr, buffer_cfg.fmt, buffer_cfg.dst_x,
		buffer_cfg.dst_y, buffer_cfg.dst_w, buffer_cfg.dst_h);

	if (buffer_cfg.layer >= OVERLAY_LAYER_MAX) {
		(void)pr_err("[ERR][OVERLAY] %s: invalid layer:%d\n", __func__,
		       buffer_cfg.layer);
		ret = -1;
	}

	if(ret != -1) {
		dprintk("rgb_swap:%d\n", buffer_cfg.rgb_swap);
		dprintk("layer number :%d  last layer:%d  chage : %d\n",
		overlay_drv->layer_n, overlay_drv->layer_nlast,
		buffer_cfg.layer);
		overlay_drv->layer_n = buffer_cfg.layer;
		layer = overlay_drv->layer_n;

		//Check if it is a format supported by RDMA
		if (VIOC_RDMA_IsVRDMA(overlay_drv->rdma[layer].id) == 0) {
			if ((buffer_cfg.fmt == (unsigned int)TCC_LCDC_IMG_FMT_444SEP) ||
				(buffer_cfg.fmt == (unsigned int)TCC_LCDC_IMG_FMT_YUV420SP) ||
				(buffer_cfg.fmt == (unsigned int)TCC_LCDC_IMG_FMT_YUV422SP) ||
				((buffer_cfg.fmt >= (unsigned int)TCC_LCDC_IMG_FMT_YUV420ITL0)
				&& (buffer_cfg.fmt <= (unsigned int)TCC_LCDC_IMG_FMT_YUV422ITL1))) {
				(void)pr_err("[ERR][OVERLAY] This format %d is not supported by RDMA id %d.\n",
					buffer_cfg.fmt, get_vioc_index(overlay_drv->rdma[layer].id));
				ret = -1;
			}
		}
	}

	if(ret != -1) {
		if ((overlay_drv->rdma[layer].reg != NULL) && (overlay_drv->wmix.reg != NULL)) {

		VIOC_RDMA_SetImageSize(
			overlay_drv->rdma[layer].reg, buffer_cfg.dst_w,
			buffer_cfg.dst_h);
		VIOC_RDMA_SetImageFormat(
			overlay_drv->rdma[layer].reg, buffer_cfg.fmt);
		VIOC_RDMA_SetImageOffset(
			overlay_drv->rdma[layer].reg, buffer_cfg.fmt,
			buffer_cfg.frm_w);
		VIOC_RDMA_SetImageRGBSwapMode(
			overlay_drv->rdma[layer].reg, buffer_cfg.rgb_swap);
		VIOC_RDMA_SetImageBase(
			overlay_drv->rdma[layer].reg, buffer_cfg.src_addr,
			buffer_cfg.src_addr, buffer_cfg.src_addr);

		VIOC_WMIX_SetPosition(
			overlay_drv->wmix.reg, layer, buffer_cfg.dst_x,
			buffer_cfg.dst_y);
		VIOC_WMIX_SetUpdate(overlay_drv->wmix.reg);

		VIOC_RDMA_SetImageEnable(overlay_drv->rdma[layer].reg);

		overlay_drv->layer_nlast = overlay_drv->layer_n;

		#ifdef CONFIG_DISPLAY_EXT_FRAME
		tcc_ctrl_ext_frame(0);
		#endif // CONFIG_DISPLAY_EXT_FRAME
		}
	}

	return ret;
}
#endif

static int tcc_overlay_display_video_buffer(overlay_video_buffer_t buffer_cfg, struct overlay_drv_type *overlay_drv)
{
	unsigned int layer = 0, base0, base1, base2;
	int ret = 0;
#if defined(CONFIG_VIOC_AFBCDEC)
	unsigned int afbc_changed = 0;
#endif

	dprintk("%s addr:0x%x  fmt : 0x%x position:%d %d  size: %d %d\n",
		__func__, buffer_cfg.addr, buffer_cfg.cfg.format,
		buffer_cfg.cfg.sx, buffer_cfg.cfg.sy, buffer_cfg.cfg.width,
		buffer_cfg.cfg.height);
	layer = overlay_drv->layer_n;

	//Check if it is a format supported by RDMA
	if (VIOC_RDMA_IsVRDMA(overlay_drv->rdma[layer].id) == 0) {
		if ((buffer_cfg.cfg.format == (unsigned int)TCC_LCDC_IMG_FMT_444SEP) ||
			(buffer_cfg.cfg.format == (unsigned int)TCC_LCDC_IMG_FMT_YUV420SP) ||
			(buffer_cfg.cfg.format == (unsigned int)TCC_LCDC_IMG_FMT_YUV422SP) ||
			((buffer_cfg.cfg.format >= (unsigned int)TCC_LCDC_IMG_FMT_YUV420ITL0) &&
			(buffer_cfg.cfg.format <= (unsigned int)TCC_LCDC_IMG_FMT_YUV422ITL1))) {
			(void)pr_err("[ERR][OVERLAY] This format %d is not supported by RDMA id %d.\n",
				buffer_cfg.cfg.format, get_vioc_index(overlay_drv->rdma[layer].id));
			ret = -1;
			goto overlay_exit;
		}
	}

	if ((overlay_drv->rdma[layer].reg != NULL) && (overlay_drv->wmix.reg != NULL)) {
		dprintk("%s SET[%d] : %p %p\n", __func__, layer,
			overlay_drv->rdma[layer].reg, overlay_drv->wmix.reg);

#if defined(CONFIG_VIOC_AFBCDEC)
		if ((overlay_drv->afbc_dec.reg != NULL)
		    && (buffer_cfg.afbc_dec_need == 0
			|| (get_vioc_index(overlay_drv->afbc_dec.id)
			    != buffer_cfg.afbc_dec_num)
			|| (layer != overlay_drv->layer_nlast))) {
			void __iomem *reg =
				overlay_drv->rdma[layer].reg;
			unsigned int id = overlay_drv->rdma[layer].id;

			if (layer != overlay_drv->layer_nlast) {
				reg = overlay_drv
					      ->rdma[overlay_drv->layer_nlast]
					      .reg;
				id = overlay_drv->rdma[overlay_drv->layer_nlast]
					     .id;
			}

			tcc_overlay_configure_AFBCDEC(
				overlay_drv->afbc_dec.reg,
				overlay_drv->afbc_dec.id, reg, id,
				0 /*for plug-out*/, 0, 0, buffer_cfg.addr,
				buffer_cfg.cfg.format, 1, 0,
				buffer_cfg.cfg.width, buffer_cfg.cfg.height);
			overlay_drv->afbc_dec.reg = NULL;
			afbc_changed = 1;
		}
#endif
		VIOC_RDMA_SetImageSize(
			overlay_drv->rdma[layer].reg, buffer_cfg.cfg.width,
			buffer_cfg.cfg.height);
		VIOC_RDMA_SetImageFormat(
			overlay_drv->rdma[layer].reg, buffer_cfg.cfg.format);
		VIOC_RDMA_SetImageOffset(
			overlay_drv->rdma[layer].reg, buffer_cfg.cfg.format,
			buffer_cfg.cfg.width);

		base0 = buffer_cfg.addr;
		base1 = buffer_cfg.addr1;
		base2 = buffer_cfg.addr2;

		if (base1 == 0U) {
			base1 = GET_ADDR_YUV42X_spU(
				buffer_cfg.addr, buffer_cfg.cfg.width,
				buffer_cfg.cfg.height);
		}

		if (base2 == 0U) {
			if (buffer_cfg.cfg.format == (unsigned int)TCC_LCDC_IMG_FMT_444SEP) {
				base2 = (unsigned int)GET_ADDR_YUV42X_spU(
					base1, buffer_cfg.cfg.width,
					buffer_cfg.cfg.height);
			} else if (
				buffer_cfg.cfg.format
				== (unsigned int)TCC_LCDC_IMG_FMT_YUV422SP) {
				base2 = (unsigned int)GET_ADDR_YUV422_spV(
					base1, buffer_cfg.cfg.width,
					buffer_cfg.cfg.height);
			} else {
				base2 = (unsigned int)GET_ADDR_YUV420_spV(
					base1, buffer_cfg.cfg.width,
					buffer_cfg.cfg.height);
			}
		}
		VIOC_RDMA_SetImageBase(
			overlay_drv->rdma[layer].reg, base0, base1, base2);
		VIOC_RDMA_SetImageRGBSwapMode(overlay_drv->rdma[layer].reg, 0);

		if (buffer_cfg.cfg.format >= VIOC_IMG_FMT_COMP) {
			VIOC_RDMA_SetImageY2RMode(
				overlay_drv->rdma[layer].reg, 2);
			VIOC_RDMA_SetImageY2REnable(
				overlay_drv->rdma[layer].reg, 1);
		} else {
			VIOC_RDMA_SetImageY2REnable(
				overlay_drv->rdma[layer].reg, 0);
		}

		VIOC_WMIX_SetPosition(
			overlay_drv->wmix.reg, layer, buffer_cfg.cfg.sx,
			buffer_cfg.cfg.sy);
		VIOC_WMIX_SetUpdate(overlay_drv->wmix.reg);

#if defined(CONFIG_VIOC_AFBCDEC)
		if (buffer_cfg.afbc_dec_need != 0) {
			if (overlay_drv->afbc_dec.reg == NULL)
				afbc_changed = 1;

			if (afbc_changed) {
				overlay_drv->afbc_dec.id =
					VIOC_FBCDEC0 + buffer_cfg.afbc_dec_num;
				overlay_drv->afbc_dec.reg =
					VIOC_AFBCDec_GetAddress(
						overlay_drv->afbc_dec.id);
			}

			tcc_overlay_configure_AFBCDEC(
				overlay_drv->afbc_dec.reg,
				overlay_drv->afbc_dec.id,
				overlay_drv->rdma[layer].reg,
				overlay_drv->rdma[layer].id,
				buffer_cfg.afbc_dec_need, 1, afbc_changed,
				buffer_cfg.addr, buffer_cfg.cfg.format, 1, 0,
				buffer_cfg.cfg.width, buffer_cfg.cfg.height);
		}

#if !defined(CONFIG_ARCH_TCC807X)
		if (overlay_drv->afbc_dec.reg != NULL)
			VIOC_RDMA_SetIssue(overlay_drv->rdma[layer].reg, 7, 16);
		else
#endif
#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) \
	|| defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
			VIOC_RDMA_SetIssue(
				overlay_drv->rdma[layer].reg, 15, 16);
#endif
#endif

		VIOC_RDMA_SetImageEnable(overlay_drv->rdma[layer].reg);

		overlay_drv->layer_nlast = overlay_drv->layer_n;

#ifdef CONFIG_DISPLAY_EXT_FRAME
		tcc_ctrl_ext_frame(0);
#endif // CONFIG_DISPLAY_EXT_FRAME
	}
overlay_exit:
	return ret;
}

static int tcc_overlay_buffer_scaling(
	overlay_video_buffer_scaling_t buffer_cfg,
	struct overlay_drv_type *overlay_drv)
{
	unsigned int layer = 0, base0, base1, base2;
	struct panel_size_t panel;
	unsigned int output_width, output_height;
	unsigned int disp_num = 0U;
	int ret = 0;
	const void __iomem *pDISPBase;

	if(overlay_drv->fb_dd_num < 10U) {
		disp_num = VIOC_DISP + overlay_drv->fb_dd_num;
	} else {
		disp_num = VIOC_DISP;
	}

	pDISPBase = VIOC_DISP_GetAddress(disp_num);

	VIOC_DISP_GetSize(pDISPBase, &panel.xres, &panel.yres);

	dprintk("%s addr:0x%x  fmt : 0x%x position:%d %d  size: %d %d\n",
		__func__, buffer_cfg.addr, buffer_cfg.cfg.format,
		buffer_cfg.cfg.sx, buffer_cfg.cfg.sy, buffer_cfg.cfg.width,
		buffer_cfg.cfg.height);
	layer = overlay_drv->layer_n;

	//Check if it is a format supported by RDMA
	if (VIOC_RDMA_IsVRDMA(overlay_drv->rdma[layer].id) == 0) {
		if ((buffer_cfg.cfg.format == (unsigned int)TCC_LCDC_IMG_FMT_444SEP) ||
			(buffer_cfg.cfg.format == (unsigned int)TCC_LCDC_IMG_FMT_YUV420SP) ||
			(buffer_cfg.cfg.format == (unsigned int)TCC_LCDC_IMG_FMT_YUV422SP) ||
			((buffer_cfg.cfg.format >= (unsigned int)TCC_LCDC_IMG_FMT_YUV420ITL0) &&
			(buffer_cfg.cfg.format <= (unsigned int)TCC_LCDC_IMG_FMT_YUV422ITL1))) {
			(void)pr_err("[ERR][OVERLAY] This format %d is not supported by RDMA id %d.\n",
				buffer_cfg.cfg.format, get_vioc_index(overlay_drv->rdma[layer].id));

			ret = -1;
			goto overlay_exit;
		}
	}

	if ((overlay_drv->rdma[layer].reg != NULL) && (overlay_drv->wmix.reg != NULL)) {
		dprintk("%s SET[%d] : %p %p\n", __func__, layer,
			overlay_drv->rdma[layer].reg, overlay_drv->wmix.reg);

		VIOC_RDMA_SetImageSize(
			overlay_drv->rdma[layer].reg, buffer_cfg.cfg.width,
			buffer_cfg.cfg.height);
		VIOC_RDMA_SetImageFormat(
			overlay_drv->rdma[layer].reg, buffer_cfg.cfg.format);
		VIOC_RDMA_SetImageOffset(
			overlay_drv->rdma[layer].reg, buffer_cfg.cfg.format,
			buffer_cfg.cfg.width);

		base0 = buffer_cfg.addr;
		base1 = buffer_cfg.addr1;
		base2 = buffer_cfg.addr2;

		if (base1 == 0U) {
			base1 = GET_ADDR_YUV42X_spU(
				buffer_cfg.addr, buffer_cfg.cfg.width,
				buffer_cfg.cfg.height);
		}

		if (base2 == 0U) {
			if (buffer_cfg.cfg.format == (unsigned int)TCC_LCDC_IMG_FMT_444SEP) {
				base2 = (unsigned int)GET_ADDR_YUV42X_spU(
					base1, buffer_cfg.cfg.width,
					buffer_cfg.cfg.height);
			} else if (buffer_cfg.cfg.format == (unsigned int)TCC_LCDC_IMG_FMT_YUV422SP) {
				base2 = (unsigned int)GET_ADDR_YUV422_spV(base1, buffer_cfg.cfg.width, buffer_cfg.cfg.height);
			} else {
				base2 = (unsigned int)GET_ADDR_YUV420_spV(
					base1, buffer_cfg.cfg.width,
					buffer_cfg.cfg.height);
			}
		}
		VIOC_RDMA_SetImageBase(
			overlay_drv->rdma[layer].reg, base0, base1, base2);
		VIOC_RDMA_SetImageRGBSwapMode(overlay_drv->rdma[layer].reg, 0);

		if (buffer_cfg.cfg.format >= VIOC_IMG_FMT_COMP) {
			VIOC_RDMA_SetImageY2RMode(
				overlay_drv->rdma[layer].reg, 2);
			VIOC_RDMA_SetImageY2REnable(
				overlay_drv->rdma[layer].reg, 1);
		} else {
			VIOC_RDMA_SetImageY2REnable(
				overlay_drv->rdma[layer].reg, 0);
		}

		if (overlay_drv->sc.reg != NULL) {
			if ((buffer_cfg.cfg.dstwidth <= 0U) || (buffer_cfg.cfg.dstheight <= 0U)) {
				(void)pr_err("dstination width/height out of range !! ");
				ret = -1;
				goto overlay_exit;
			}

			VIOC_SC_SetBypass(overlay_drv->sc.reg, 0);

			if(panel.xres > buffer_cfg.cfg.sx) {
				output_width =
					((panel.xres - buffer_cfg.cfg.sx) < buffer_cfg.cfg.dstwidth) ?
					(panel.xres - buffer_cfg.cfg.sx) : buffer_cfg.cfg.dstwidth;
			} else {
				output_width = buffer_cfg.cfg.dstwidth;
			}

			if(panel.yres > buffer_cfg.cfg.sy) {
				output_height =
					((panel.yres - buffer_cfg.cfg.sy) < buffer_cfg.cfg.dstheight) ?
					(panel.yres - buffer_cfg.cfg.sy) : buffer_cfg.cfg.dstheight;
			} else {
				output_height = buffer_cfg.cfg.dstheight;
			}

			VIOC_SC_SetDstSize(overlay_drv->sc.reg, output_width, output_height);
			VIOC_SC_SetOutSize(overlay_drv->sc.reg, output_width, output_height);
			VIOC_SC_SetOutPosition(overlay_drv->sc.reg, 0, 0);
			(void)VIOC_CONFIG_PlugIn(overlay_drv->sc.id, overlay_drv->rdma[layer].id);
			VIOC_SC_SetUpdate(overlay_drv->sc.reg);
		}

		VIOC_WMIX_SetPosition(
			overlay_drv->wmix.reg, layer, buffer_cfg.cfg.sx,
			buffer_cfg.cfg.sy);
		VIOC_WMIX_SetUpdate(overlay_drv->wmix.reg);

#if defined(CONFIG_ARCH_TCC898X) || defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC901X)
			VIOC_RDMA_SetIssue(overlay_drv->rdma[layer].reg, 15, 16);
#endif

		VIOC_RDMA_SetImageEnable(overlay_drv->rdma[layer].reg);

		overlay_drv->layer_nlast = overlay_drv->layer_n;

#ifdef CONFIG_DISPLAY_EXT_FRAME
		tcc_ctrl_ext_frame(0);
#endif // CONFIG_DISPLAY_EXT_FRAME
	}
overlay_exit:
	return ret;
}

#define RDMA_UVI_MAX_WIDTH 3072U

#if 0
static void tcc_video_post_process(struct tcc_lcdc_image_update *ImageInfo)
{
	if (ImageInfo == NULL) {
		(void)pr_err("[ERR][VIOC_I] %s: ImageInfo is null\n", __func__);
		goto overlay_exit;
	}

	#ifdef CONFIG_HDMI_DISPLAY_LASTFRAME
	tcc_video_clear_last_frame(ImageInfo->Lcdc_layer, false);
	#endif
	#ifdef CONFIG_VIDEO_DISPLAY_SWAP_VPU_FRAME
	if (ImageInfo)
		tcc_video_frame_backup(ImageInfo);
	#endif
	#ifdef CONFIG_DISPLAY_EXT_FRAME
	#if defined(CONFIG_LINUX) || defined(CONFIG_LINUX_ANDROID)
	if (!restricted_ExtFrame)
	#endif
		tcc_ctrl_ext_frame(0);
	#endif

overlay_exit:
	return;
}
#endif

static void tccxxx_GetAddress(unsigned int format, unsigned int base_Yaddr,
		       unsigned int src_imgx, unsigned int src_imgy,
		       unsigned int start_x, unsigned int start_y,
		       unsigned int *Y, unsigned int *U, unsigned int *V)
{
	unsigned int Uaddr, Vaddr, Yoffset, UVoffset, start_yPos, start_xPos;

	start_yPos = (start_y >> 1) << 1;
	start_xPos = (start_x >> 1) << 1;

	check_wrap(src_imgx);
	check_wrap(start_yPos);
	check_wrap(start_xPos);

	Yoffset = (src_imgx * start_yPos) + start_xPos;

	// RGB format
	if ((format >= (unsigned int)TCC_LCDC_IMG_FMT_RGB332) &&
	    (format <= (unsigned int)TCC_LCDC_IMG_FMT_ARGB6666_3)) {
		int Bpp;

		if (format == (unsigned int)TCC_LCDC_IMG_FMT_RGB332) {
			Bpp = 1;
		} else if (format <= (unsigned int)TCC_LCDC_IMG_FMT_RGB555) {
			Bpp = 2;
		} else {
			//format >= TCC_LCDC_IMG_FMT_RGB888) && (format <= TCC_LCDC_IMG_FMT_ARGB6666_3)
			Bpp = 4;
		}

		check_wrap(Yoffset);
		check_wrap(base_Yaddr);
		check_wrap((unsigned int)Bpp);
		*Y = base_Yaddr + (Yoffset * (unsigned int)Bpp);

		goto getaddress_exit;
	}

	if ((format == (unsigned int)TCC_LCDC_IMG_FMT_UYVY) ||
	    (format == (unsigned int)TCC_LCDC_IMG_FMT_VYUY) ||
	    (format == (unsigned int)TCC_LCDC_IMG_FMT_YUYV) ||
	    (format == (unsigned int)TCC_LCDC_IMG_FMT_YVYU)) {
		check_wrap(Yoffset);
		Yoffset = 2U * Yoffset;
	}

	check_wrap(Yoffset);
	check_wrap(base_Yaddr);
	*Y = base_Yaddr + Yoffset;

	if ((*U == 0U) && (*V == 0U)) {
		Uaddr = GET_ADDR_YUV42X_spU(base_Yaddr, src_imgx, src_imgy);
		if (format == (unsigned int)TCC_LCDC_IMG_FMT_YUV420SP) {
			Vaddr = GET_ADDR_YUV420_spV(Uaddr, src_imgx, src_imgy);
		} else {
			Vaddr = GET_ADDR_YUV422_spV(Uaddr, src_imgx, src_imgy);
		}
	} else {
		Uaddr = *U;
		Vaddr = *V;
	}

	if ((format == (unsigned int)TCC_LCDC_IMG_FMT_YUV420SP) ||
	    (format == (unsigned int)TCC_LCDC_IMG_FMT_YUV420ITL0) ||
	    (format == (unsigned int)TCC_LCDC_IMG_FMT_YUV420ITL1)) {
		if (format == (unsigned int)TCC_LCDC_IMG_FMT_YUV420SP) {
			UVoffset =
				(((src_imgx * start_yPos) / 4U) + (start_xPos / 2U));
		} else {
			UVoffset = (((src_imgx * start_yPos) / 2U) + start_xPos);
		}
	} else {
		if (format == (unsigned int)TCC_LCDC_IMG_FMT_YUV422ITL1) {
			UVoffset = ((src_imgx * start_yPos) + start_xPos);
		} else {
			UVoffset =
				(((src_imgx * start_yPos) / 2U) + (start_xPos / 2U));
		}
	}

	check_wrap(Uaddr);
	check_wrap(Vaddr);
	check_wrap(UVoffset);

	*U = Uaddr + UVoffset;
	*V = Vaddr + UVoffset;

getaddress_exit:
	return;
}

static unsigned int tca_get_scaler_num(enum TCC_OUTPUT_TYPE Output, unsigned int Layer)
{
	#if defined(CONFIG_ARCH_TCC750X)
	unsigned int iSCNum = VIOC_SCALER1;
	#else
	unsigned int iSCNum = VIOC_SCALER3;
	#endif

#ifdef CONFIG_ANDROID
	if (Output == TCC_OUTPUT_LCD) {
		// lvds
		#if defined(CONFIG_ARCH_TCC750X)
		iSCNum = VIOC_SCALER1;
		#else
		iSCNum = VIOC_SCALER3;
		#endif
	} else if (Output == TCC_OUTPUT_HDMI
		|| Output == TCC_OUTPUT_COMPOSITE
		|| Output == TCC_OUTPUT_DP
		|| Output == TCC_OUTPUT_COMPONENT) {

		if (Layer == RDMA_FB) {
			// FB layer
			iSCNum = VIOC_SCALER2;
		} else if (Layer == RDMA_VIDEO)
		{
			#if defined(CONFIG_ARCH_TCC750X)
			iSCNum = VIOC_SCALER1;
			#else
			iSCNum = VIOC_SCALER3;
			#endif
		} else {
			// RDMA_VIDEO_SUB
			#if defined(CONFIG_ARCH_TCC899X) \
				|| defined(CONFIG_ARCH_TCC901X)
			iSCNum = VIOC_SCALER4;
			#elif defined(CONFIG_ARCH_TCC750X)
			iSCNum = VIOC_SCALER1;
			#else
			iSCNum = VIOC_SCALER3;
			#endif
		}
	}
#else
	#ifdef CONFIG_PLATFORM_STB
	if (Layer == RDMA_VIDEO) {
		// video
		iSCNum = VIOC_SCALER1;
	} else {
		#if defined(CONFIG_VIOC_DOLBY_VISION_EDR)
			#if defined(CONFIG_ARCH_TCC750X)
			iSCNum = VIOC_SCALER1;
			#else
			iSCNum = VIOC_SCALER3;
			#endif
		#else
		iSCNum = VIOC_SCALER0;
		#endif
	}
	#else
	iSCNum = VIOC_SCALER1;
	#endif
#endif

	return iSCNum;
}

static void overlay_onthefly_display_update(const struct tcc_lcdc_image_update *ImageInfo, const struct overlay_drv_type *overlay_drv)
{
	void __iomem *pSC;
	int iSCType;
	unsigned int uiSCType;
	unsigned int layer = overlay_drv->layer_n;
	unsigned int lcd_width = 0, lcd_height = 0;
	void __iomem *pDISP;

	check_wrap(overlay_drv->fb_dd_num);
	pDISP = VIOC_DISP_GetAddress(VIOC_DISP0 + overlay_drv->fb_dd_num);

	if (ImageInfo == NULL) {
		goto overlay_exit;
	}

	if ((ImageInfo->Lcdc_layer >= (unsigned int)RDMA_MAX_NUM)
		|| (ImageInfo->fmt > (unsigned int)TCC_LCDC_IMG_FMT_MAX)) {
		(void)pr_err("[ERR][VIOC_I] LCD :: enable:%d, layer:%d, addr:0x%x, \
			fmt:%d, Fw:%d, Fh:%d, Iw:%d, Ih:%d, fmt:%d onthefly:%d\n",
		       ImageInfo->enable, ImageInfo->Lcdc_layer,
		       ImageInfo->addr0, ImageInfo->fmt,
		       ImageInfo->Frame_width, ImageInfo->Frame_height,
		       ImageInfo->Image_width, ImageInfo->Image_height,
		       ImageInfo->fmt, ImageInfo->on_the_fly);

		goto overlay_exit;
	}

	if(layer != (ImageInfo->Lcdc_layer)) {
		(void)pr_err("[ERR] ImageInfo->Lcdc_layer != overlay_drv->layer_n\n");
		goto overlay_exit;
	}

	if (ImageInfo->enable == 0U)	{
		#if !defined(CONFIG_VIOC_DOLBY_VISION_EDR) && defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST_UI)
		VIOC_RDMA_PreventEnable_for_UI(0, 0);
		#endif

		//map converter
		#if defined(CONFIG_VIOC_MAP_DECOMP)
		if (overlay_drv->mc.id != VIOC_NO_COMPONENT) {
			(void)VIOC_CONFIG_MCPath(overlay_drv->rdma[layer].id, overlay_drv->rdma[layer].id);
		}
		#endif

		VIOC_RDMA_SetImageDisable(overlay_drv->rdma[layer].reg);

		#ifdef CONFIG_PLATFORM_AVN
		/* TODO: Will be deprecated.
		 * If CLS Platform disable below code is normal,
		 * But problem occurs in ALS
		 */
		VIOC_CONFIG_SWReset(
			overlay_drv->rdma[layer].id,
			VIOC_CONFIG_RESET);
		VIOC_CONFIG_SWReset(
			overlay_drv->rdma[layer].id,
			VIOC_CONFIG_CLEAR);
		#endif

		if (ImageInfo->MVCframeView != 1) {
			// scaler plug in status check
			if (VIOC_CONFIG_GetScaler_PluginToRDMA(overlay_drv->rdma[layer].id) != -1) {
				iSCType = VIOC_CONFIG_GetScaler_PluginToRDMA(overlay_drv->rdma[layer].id);
				uiSCType = clamp_t(u32, (iSCType), 0, (INT_MAX));
				(void)VIOC_CONFIG_PlugOut(uiSCType);
				VIOC_CONFIG_SWReset(uiSCType, VIOC_CONFIG_RESET);
				VIOC_CONFIG_SWReset(uiSCType, VIOC_CONFIG_CLEAR);
			}
		}
		goto overlay_exit;
	}

	uiSCType = tca_get_scaler_num(TCC_OUTPUT_DP, ImageInfo->Lcdc_layer);

	pSC = VIOC_SC_GetAddress(uiSCType);

	VIOC_DISP_GetSize(pDISP,&lcd_width, &lcd_height);

	if ((lcd_width == 0U) || (lcd_height == 0U)) {
		(void)pr_err("[ERR][VIOC_I] %s: lcd_width %d, lcd_height %d\n",
			__func__, lcd_width, lcd_height);
		goto overlay_exit;
	}

	if ((ImageInfo->MVCframeView != 1) && (bool)ImageInfo->on_the_fly) {
		#ifdef TEST_SC_NO_PLUGIN_IN_BYPASS_CASE
		unsigned int bSCBypass = 0;
		#endif

		#if defined(CONFIG_MC_WORKAROUND)
		if (!system_rev && ImageInfo->private_data.optional_info[
			VID_OPT_HAVE_MC_INFO] != 0) {
			unsigned int plus_height = VIOC_SC_GetPlusSize(
				(ImageInfo->crop_bottom - ImageInfo->crop_top),
				ImageInfo->Image_height);

			// set destination size in scaler
			VIOC_SC_SetDstSize(pSC, ImageInfo->Image_width,
				ImageInfo->Image_height+plus_height);
		} else
		#endif
		{
			// set destination size in scaler
			VIOC_SC_SetDstSize(pSC, ImageInfo->Image_width,
				ImageInfo->Image_height);
		}

		// set output size in scaler
		VIOC_SC_SetOutSize(pSC, ImageInfo->Image_width,
			ImageInfo->Image_height);

		if (!(bool)(__raw_readl(pDISP+DCTRL) & DCTRL_NI_MASK)) {
			// interlace output
			VIOC_SC_SetBypass(pSC, OFF);
			dprintk(" %s  scaler %d is plug in SetBypass OFF\n",
				__func__, get_vioc_index(uiSCType));
		} else {
			if ((!(bool)ImageInfo->one_field_only_interlace &&
				(((ImageInfo->crop_right - ImageInfo->crop_left)
					== ImageInfo->Image_width) &&
				((ImageInfo->crop_bottom - ImageInfo->crop_top)
					== ImageInfo->Image_height)))
				|| ((bool)ImageInfo->one_field_only_interlace &&
				(((ImageInfo->crop_right - ImageInfo->crop_left)
					== ImageInfo->Image_width) &&
				(((ImageInfo->crop_bottom - ImageInfo->crop_top) / 2U)
					== ImageInfo->Image_height)))
			) {
				#if defined(CONFIG_MC_WORKAROUND)
				if (!system_rev &&
					ImageInfo->private_data.optional_info[
					VID_OPT_HAVE_MC_INFO] != 0) {
					VIOC_SC_SetBypass(pSC, OFF);
				} else
				#endif
				{
					VIOC_SC_SetBypass(pSC, ON);
				}
				#ifdef TEST_SC_NO_PLUGIN_IN_BYPASS_CASE
				bSCBypass = 1;
				#endif
				dprintk("%s: sc%d is plug in SetBypass ON\n",__func__, get_vioc_index(uiSCType));
			} else {
				VIOC_SC_SetBypass(pSC, OFF);
				dprintk("%s: sc%d is plug in SetBypass OFF\n",__func__, get_vioc_index(uiSCType));
			}
		}

		#ifdef TEST_SC_NO_PLUGIN_IN_BYPASS_CASE
		if (!bSCBypass)
		#endif
		{
			//scaler plug in status check
			if (VIOC_CONFIG_GetScaler_PluginToRDMA(
				overlay_drv->rdma[layer].id) == -1) {

				VIOC_RDMA_SetImageDisable(
					overlay_drv->rdma[layer].reg);

				dprintk("%s: scaler 1 is plug in RDMA %d\n",
					__func__,
					get_vioc_index(overlay_drv->rdma[layer].id));

				(void)VIOC_CONFIG_PlugIn(uiSCType,
					overlay_drv->rdma[layer].id);
			}
		}
		#ifdef TEST_SC_NO_PLUGIN_IN_BYPASS_CASE
		else {
			//scaler plug in status check
			if (VIOC_CONFIG_GetScaler_PluginToRDMA(
				pdp_data->rdma_info[
				ImageInfo->Lcdc_layer].blk_num) != -1) {
				(void)VIOC_CONFIG_PlugOut(uiSCType);
			}
		}
		#endif
	} else {
		if (VIOC_CONFIG_GetScaler_PluginToRDMA(
			overlay_drv->rdma[layer].id) != -1) {
			dprintk("%s: scaler 1 is plug out\n", __func__);
			VIOC_RDMA_SetImageDisable(
			overlay_drv->rdma[layer].reg);
			(void)VIOC_CONFIG_PlugOut(uiSCType);
		}
	}

	if (VIOC_CONFIG_GetScaler_PluginToRDMA(
		overlay_drv->rdma[layer].id) != -1) {
		VIOC_SC_SetUpdate(pSC);
	}

	/*----------------------------------------------------------------------
	 * POSITION
	 */
	VIOC_WMIX_SetPosition(overlay_drv->wmix.reg,
		ImageInfo->Lcdc_layer, ImageInfo->offset_x, ImageInfo->offset_y);
	VIOC_WMIX_SetUpdate(overlay_drv->wmix.reg);

	/*
	 * MapConverter
	 */
	#ifdef CONFIG_VIOC_MAP_DECOMP
	if (ImageInfo->private_data.optional_info[VID_OPT_HAVE_MC_INFO] != 0U) {

		void __iomem *HwVIOC_MC = VIOC_MC_GetAddress(overlay_drv->mc.id);
		// size_t vsync_type;

		if (HwVIOC_MC != NULL) {
			(void)VIOC_CONFIG_MCPath(overlay_drv->rdma[layer].id, overlay_drv->mc.id);

			#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
			tca_map_convter_set(overlay_drv->mc.id,ImageInfo, 0);
			#else
			tca_map_convter_set(overlay_drv->mc.id,ImageInfo, 1);
			#endif

			#if !defined(CONFIG_VIOC_DOLBY_VISION_EDR) && defined(CONFIG_VIOC_DOLBY_VISION_CERTIFICATION_TEST_UI)
			VIOC_RDMA_PreventEnable_for_UI(1, 1);
			#endif

			VIOC_MC_Start_OnOff(HwVIOC_MC, 1);
			goto next;
		} else {
			(void)pr_err("[ERR][OVERLAY] overlay%d MC Component Unknown 0x%x\n",overlay_drv->id, overlay_drv->mc.id);
			if (overlay_drv->mc.id != VIOC_NO_COMPONENT) {
				(void)VIOC_CONFIG_MCPath(overlay_drv->rdma[layer].id, overlay_drv->rdma[layer].id);
			}

			VIOC_RDMA_SetImageDisable(overlay_drv->rdma[layer].reg);
			VIOC_CONFIG_SWReset(overlay_drv->rdma[layer].id,VIOC_CONFIG_RESET);
			VIOC_CONFIG_SWReset(overlay_drv->rdma[layer].id,VIOC_CONFIG_CLEAR);

			if (ImageInfo->MVCframeView != 1) {
				// scaler plug in status check
				if (VIOC_CONFIG_GetScaler_PluginToRDMA(overlay_drv->rdma[layer].id) != -1) {
					iSCType = VIOC_CONFIG_GetScaler_PluginToRDMA(overlay_drv->rdma[layer].id);
					uiSCType = clamp_t(u32, (iSCType), 0, (INT_MAX));
					(void)VIOC_CONFIG_PlugOut(uiSCType);
					VIOC_CONFIG_SWReset(uiSCType, VIOC_CONFIG_RESET);
					VIOC_CONFIG_SWReset(uiSCType, VIOC_CONFIG_CLEAR);
				}
			}

			goto overlay_exit;
		}
	} else
	#endif
	{
		/*
		* RDMA
		*/
		if ((ImageInfo->fmt >= (unsigned int)TCC_LCDC_IMG_FMT_444SEP) &&
			(ImageInfo->fmt <= (unsigned int)TCC_LCDC_IMG_FMT_YUV422ITL1)) {

			#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
			VIOC_RDMA_SetImageY2REnable(
				overlay_drv->rdma[layer].reg,
				false);
			#else
			VIOC_RDMA_SetImageY2REnable(
				overlay_drv->rdma[layer].reg, 1U);
			#endif

			/* Y2RMode Default 0 (Studio Color) */
			VIOC_RDMA_SetImageY2RMode(
				overlay_drv->rdma[layer].reg,
				0x2);

			if (ImageInfo->Frame_width <= RDMA_UVI_MAX_WIDTH) {
				VIOC_RDMA_SetImageUVIEnable(
				overlay_drv->rdma[layer].reg, 1U);
			} else {
				VIOC_RDMA_SetImageUVIEnable(
				overlay_drv->rdma[layer].reg, 0U);
			}
		} else {
			#if defined(CONFIG_TCC_VIOC_DISP_PATH_INTERNAL_CS_YUV)
			VIOC_RDMA_SetImageR2YEnable(
				overlay_drv->rdma[layer].reg, 1U);
			VIOC_RDMA_SetImageR2YMode(
				overlay_drv->rdma[layer].reg,
				1U); /* Y2RMode Default 0 (Studio Color) */
			#endif
		}

		if ((bool)ImageInfo->one_field_only_interlace
		#ifdef CONFIG_VIOC_10BIT
		|| (ImageInfo->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10)
		#endif
		) {
			VIOC_RDMA_SetImageOffset(
				overlay_drv->rdma[layer].reg,
				ImageInfo->fmt, ImageInfo->Frame_width * 2U);
		}
		#ifdef CONFIG_VIOC_10BIT
		else if (ImageInfo->private_data.optional_info[VID_OPT_BIT_DEPTH]
				== 11) {
			VIOC_RDMA_SetImageOffset(
				overlay_drv->rdma[layer].reg,
				ImageInfo->fmt, ImageInfo->Frame_width * 125 / 100);
		}
		#endif
		else {
			VIOC_RDMA_SetImageOffset(
				overlay_drv->rdma[layer].reg,
				ImageInfo->fmt, ImageInfo->Frame_width);
		}

		#ifdef CONFIG_VIOC_10BIT
		if (ImageInfo->private_data.optional_info[VID_OPT_BIT_DEPTH] == 10) {
			/* YUV 10bit(16bit) support */
			VIOC_RDMA_SetDataFormat(
				overlay_drv->rdma[layer].reg,
				0x1, 0x1);
		} else if (ImageInfo->private_data.optional_info[VID_OPT_BIT_DEPTH]
			== 11) {
			/* YUV real 10bit support */
			VIOC_RDMA_SetDataFormat(
				overlay_drv->rdma[layer].reg,
				0x3, 0x0);
		} else {
			/* YUV 8bit support */
			VIOC_RDMA_SetDataFormat(
				overlay_drv->rdma[layer].reg,
				0x0, 0x0);
		}
		#endif

		VIOC_RDMA_SetImageFormat(
			overlay_drv->rdma[layer].reg,
			ImageInfo->fmt);

		VIOC_RDMA_SetImageSize(
			overlay_drv->rdma[layer].reg,
			ImageInfo->crop_right - ImageInfo->crop_left,
			ImageInfo->crop_bottom - ImageInfo->crop_top);

		if (!(((ImageInfo->crop_left == 0U) &&
			(ImageInfo->crop_right == ImageInfo->Frame_width)) &&
			((ImageInfo->crop_top == 0U) &&
			(ImageInfo->crop_bottom == ImageInfo->Frame_height)))
		) {
			unsigned int addr_Y = ImageInfo->addr0;
			unsigned int addr_U = ImageInfo->addr1;
			unsigned int addr_V = ImageInfo->addr2;

			dprintk("Crop left:%d right:%d top:%d bot:%d\n",
				ImageInfo->crop_left, ImageInfo->crop_right,
				ImageInfo->crop_top, ImageInfo->crop_bottom);

			tccxxx_GetAddress(ImageInfo->fmt,
				(unsigned int)ImageInfo->addr0,
				ImageInfo->Frame_width, ImageInfo->Frame_height,
				ImageInfo->crop_left, ImageInfo->crop_top,
				&addr_Y, &addr_U, &addr_V);

			if ((bool)ImageInfo->one_field_only_interlace) {
				VIOC_RDMA_SetImageSize(
					overlay_drv->rdma[layer].reg,
					ImageInfo->crop_right
						- ImageInfo->crop_left,
					(ImageInfo->crop_bottom
						- ImageInfo->crop_top) / 2U);
			} else {
				VIOC_RDMA_SetImageSize(
					overlay_drv->rdma[layer].reg,
					ImageInfo->crop_right
						- ImageInfo->crop_left,
					ImageInfo->crop_bottom
						- ImageInfo->crop_top);
			}

			VIOC_RDMA_SetImageBase(
				overlay_drv->rdma[layer].reg,
				addr_Y, addr_U, addr_V);
		} else {
			dprintk("Don't crop left:%d right:%d top:%d bot:%d\n",
				ImageInfo->crop_left, ImageInfo->crop_right,
				ImageInfo->crop_top, ImageInfo->crop_bottom);

			if ((bool)ImageInfo->one_field_only_interlace) {
				VIOC_RDMA_SetImageSize(
					overlay_drv->rdma[layer].reg,
					ImageInfo->Frame_width,
					ImageInfo->Frame_height / 2U);
			} else {
				VIOC_RDMA_SetImageSize(
					overlay_drv->rdma[layer].reg,
					ImageInfo->Frame_width,
					ImageInfo->Frame_height);
			}

			VIOC_RDMA_SetImageBase(
				overlay_drv->rdma[layer].reg,
				ImageInfo->addr0, ImageInfo->addr1,
				ImageInfo->addr2);
		}

		VIOC_RDMA_SetImageIntl(overlay_drv->rdma[layer].reg, 0U);
		VIOC_RDMA_SetImageEnable(overlay_drv->rdma[layer].reg);
	}

#ifdef CONFIG_VIOC_MAP_DECOMP
next:
#endif

	if (VIOC_CONFIG_GetScaler_PluginToRDMA(overlay_drv->rdma[layer].id) != -1) {
		VIOC_SC_SetUpdate(pSC);
	}

overlay_exit:
	return;
}

static long tcc_overlay_ioctl(struct file *pfile, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	const struct miscdevice *misc = (struct miscdevice *)pfile->private_data;
	struct overlay_drv_type *overlay_drv = dev_get_drvdata(misc->parent);

	switch (cmd) {
#if defined(CONFIG_TELECHIPS_SCREEN_SHARE)
	case OVERLAY_PUSH_SHARED_BUFFER:
		{
			overlay_shared_buffer_t overBuffCfg;

			(void)memcpy(&overBuffCfg, (overlay_shared_buffer_t *)arg,sizeof(overlay_shared_buffer_t));
			ret = tcc_overlay_display_shared_screen(overBuffCfg, overlay_drv);
		}
		break;
#endif
	case OVERLAY_PUSH_VIDEO_BUFFER:
		{
			overlay_video_buffer_t overBuffCfg;

			if ((bool)copy_from_user(
					&overBuffCfg, (overlay_video_buffer_t *)arg,
					sizeof(overlay_video_buffer_t))) {
				ret = -EFAULT;
			} else {
				overlay_drv->overBuffCfg = overBuffCfg;
			}

			if(ret != -EFAULT) {
				ret = tcc_overlay_display_video_buffer(overBuffCfg, overlay_drv);
			}
		}
		break;
	case OVERLAY_PUSH_VIDEO_BUFFER_SCALING:
		{
			overlay_video_buffer_scaling_t overBuffCfg;

			if ((bool)copy_from_user(
					&overBuffCfg, (overlay_video_buffer_scaling_t *)arg,
					sizeof(overlay_video_buffer_scaling_t))) {
				ret = -EFAULT;
			}

			if(ret != -EFAULT) {
				ret = tcc_overlay_buffer_scaling(overBuffCfg, overlay_drv);
			}
		}
		break;
	case OVERLAY_SET_CONFIGURE:
		{
			overlay_config_t overCfg;

			if ((bool)copy_from_user(
					&overCfg, (overlay_config_t *)arg,
					sizeof(overlay_config_t))) {
				ret = -EFAULT;
			}
		}
		break;
	case OVERLAY_SET_LAYER:
		{
			unsigned int overlay_layer;

			if ((bool)copy_from_user(&overlay_layer, (unsigned int *)arg, sizeof(unsigned int))) {
				ret = -EFAULT;
			}

			if(ret != -EFAULT) {
				if (overlay_drv->wmix.reg != NULL) {
					VIOC_WMIX_SetOverlayPriority(overlay_drv->wmix.reg, 0xC);
				}
				if (overlay_layer < 4U) {
					(void)pr_info("[INF][OVERLAY] layer number :%d  last layer:%d  chage : %d\n",
						overlay_drv->layer_n, overlay_drv->layer_nlast,
						overlay_layer);
					overlay_drv->layer_n = overlay_layer;
				} else {
					(void)pr_err("[ERR][OVERLAY] wrong layer number :%d  cur layer:%d\n",
						overlay_layer, overlay_drv->layer_nlast);
				}
			}
		}
		break;
	case OVERLAY_DISALBE_LAYER:
		{
			if(overlay_drv->rdma[overlay_drv->layer_n].reg != NULL) {
				VIOC_RDMA_SetImageDisable(overlay_drv->rdma[overlay_drv->layer_n].reg);
			} else {
				(void)pr_err("[ERR][OVERLAY] rdma address of layer%d is NULL\n", overlay_drv->layer_n);
				ret = -EFAULT;
			}
		}
		break;
	case OVERLAY_GET_LAYER:
		{
			if ((bool)copy_to_user(
					(unsigned int *)arg, &overlay_drv->layer_n,
					sizeof(unsigned int))) {
				ret = -EFAULT;
			}
		}
		break;
	case OVERLAY_SET_OVP:
	case OVERLAY_SET_OVP_KERNEL:
		{
			unsigned int ovp = 5U;	//default value is 5

			if (cmd == OVERLAY_SET_OVP_KERNEL) {
				ovp = clamp_t(u32, (arg), 0, (UINT_MAX));
			} else {
				if ((bool)copy_from_user(
						&ovp, (unsigned int *)arg,
						sizeof(unsigned int))) {
					(void)pr_err("[ERR][OVERLAY] OVERLAY_SET_OVP copy_from_user failed\n");
					ret = -EFAULT;;
				}
			}

			if (ovp > 29U) {
				(void)pr_err("[ERR][OVERLAY] wrong ovp number: %d\n", ovp);
				ret = -EINVAL;
			}
			if((ret != -EINVAL) && (ret != -EFAULT)) {
				if (overlay_drv->wmix.reg != NULL) {
					VIOC_WMIX_SetOverlayPriority(overlay_drv->wmix.reg, ovp);
					VIOC_WMIX_SetUpdate(overlay_drv->wmix.reg);
				} else {
					(void)pr_err("[ERR][OVERLAY] wmix address is NULL\n");
					ret = -EFAULT;
				}
			}
		}
		break;
	case OVERLAY_GET_OVP:
	case OVERLAY_GET_OVP_KERNEL:
		{
			unsigned int ovp;
			
			if (overlay_drv->wmix.reg != NULL) {
				VIOC_WMIX_GetOverlayPriority(overlay_drv->wmix.reg, &ovp);
			} else {
				(void)pr_err("[ERR][OVERLAY] wmix address is NULL\n");
				ret = -EFAULT;
			}

			if (ret != -EFAULT) {
				if (cmd == OVERLAY_GET_OVP_KERNEL) {
					*(unsigned long *)arg = ovp;
				} else {
					if ((bool)copy_to_user((unsigned int *)arg, &ovp, sizeof(unsigned int))) {
						(void)pr_err("[ERR][OVERLAY] OVERLAY_SET_OVP copy_to_user failed\n");
						ret = -EFAULT;
					}
				}
			}
		}
		break;
	case OVERLAY_GET_PANEL_SIZE:
		{
			struct panel_size_t panel;
			unsigned int disp_num = 0U;
			const void __iomem *pDISPBase;

			if(overlay_drv->fb_dd_num < 10U) {
				disp_num = VIOC_DISP + overlay_drv->fb_dd_num;
			} else {
				disp_num = VIOC_DISP;
			}

			pDISPBase = VIOC_DISP_GetAddress(disp_num);

			VIOC_DISP_GetSize(pDISPBase, &panel.xres, &panel.yres);

			if ((bool)copy_to_user((struct panel_size_t *)arg,
				&panel,
				sizeof(struct panel_size_t))) {
				(void)pr_err("[ERR][OVERLAY] OVERLAY_GET_PANEL_SIZE copy_to_user failed\n");
				ret = -EFAULT;
			}
		}
		break;

	case OVERLAY_OTF_PUSH_FRAME:
		{
			struct tcc_lcdc_image_update ImageInfo;
			dprintk("[%d] OVERLAY_OTF_PUSH_FRAME\n", __LINE__);

			if ((bool)copy_from_user(
					&ImageInfo, (struct tcc_lcdc_image_update *)arg,
					sizeof(struct tcc_lcdc_image_update))) {
				ret = -EFAULT;
				break;
			}

			dprintk("addr:0x%x fmt:0x%x offset:%d %d frame:%d %d image:%d %d\n",
			ImageInfo.addr0, ImageInfo.fmt, ImageInfo.offset_x, ImageInfo.offset_y,
			ImageInfo.Frame_width, ImageInfo.Frame_height,
			ImageInfo.Image_width, ImageInfo.Image_height);

			#if defined(CONFIG_TELECHIPS_VIDEO_DISPLAY_BY_VSYNC_INT) || defined(CONFIG_TELECHIPS_VIDEO_DISPLAY_BY_VSYNC_INT_MODULE)
			dprintk("deinterlace_mode:%x\n", ImageInfo.deinterlace_mode);
			#endif

			overlay_onthefly_display_update((struct tcc_lcdc_image_update *)&ImageInfo, overlay_drv);

			ret = 0;
		}
		break;

	default:
		(void)pr_err(" Unsupported IOCTL(%d)!!!\n", cmd);
		break;
	}

	return ret;
}

static int tcc_overlay_release(struct inode *pinode, struct file *pfile)
{
	unsigned int layer = 0;
	const struct miscdevice *misc = (struct miscdevice *)pfile->private_data;
	struct overlay_drv_type *overlay_drv = dev_get_drvdata(misc->parent);

	(void)pinode;

	if(overlay_drv->open_cnt > 0U) {
		overlay_drv->open_cnt--;
	}

	dprintk("%s num:%d\n", __func__, overlay_drv->open_cnt);

	if (overlay_drv->open_cnt == 0U) {

		layer = overlay_drv->layer_n;

		if (overlay_drv->rdma[layer].reg != NULL) {
			VIOC_RDMA_SetImageDisable(overlay_drv->rdma[layer].reg);
		}

#if defined(CONFIG_VIOC_AFBCDEC)
		if (overlay_drv->afbc_dec.reg != NULL) {
			tcc_overlay_configure_AFBCDEC(
				overlay_drv->afbc_dec.reg,
				overlay_drv->afbc_dec.id,
				overlay_drv->rdma[layer].reg,
				overlay_drv->rdma[layer].id, 0, 0, 0, 0, 0, 0,
				0, 0, 0);
			overlay_drv->afbc_dec.reg = NULL;
		}
#endif

		if (overlay_drv->wmix.reg != NULL) {
			VIOC_WMIX_SetPosition(
				overlay_drv->wmix.reg, layer, 0, 0);
			VIOC_WMIX_SetUpdate(overlay_drv->wmix.reg);
		}

		clk_disable_unprepare(overlay_drv->pclk);

#ifdef CONFIG_DISPLAY_EXT_FRAME
		tcc_ctrl_ext_frame(0);
#endif
	}

	return 0;
}

static int tcc_overlay_open(struct inode *pinode, struct file *pfile)
{
	int ret;
	const struct miscdevice *misc = (struct miscdevice *)pfile->private_data;
	struct overlay_drv_type *overlay_drv = dev_get_drvdata(misc->parent);

	(void)pinode;
	if(overlay_drv->open_cnt < (UINT_MAX / 2U)) {
		overlay_drv->open_cnt++;
	}
	ret = clk_prepare_enable(overlay_drv->pclk);

	dprintk("%s num:%d\n", __func__, overlay_drv->open_cnt);

	return 0;
}

static const struct file_operations tcc_overlay_fops = {
	.owner = THIS_MODULE,
	.poll = tcc_overlay_poll,
	.unlocked_ioctl = tcc_overlay_ioctl,
	.mmap = tcc_overlay_mmap,
	.open = tcc_overlay_open,
	.release = tcc_overlay_release,
};

static int tcc_overlay_probe(struct platform_device *pdev)
{
	struct overlay_drv_type *overlay_drv;
	struct device_node *vioc_node, *tmp_node;
	unsigned int index;
	unsigned int i = 0U;
	int ret = 0;
	int itemp;

	overlay_drv = kzalloc(sizeof(struct overlay_drv_type), GFP_KERNEL);

	if (overlay_drv == NULL) {
		ret = -ENOMEM;
		goto err_overlay_drv_misc;
	}

	overlay_drv->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);

	if (overlay_drv->misc == NULL) {
		goto err_overlay_drv_misc;
	}

	overlay_drv->fb_dd_num = UINT_MAX;
	vioc_node = of_parse_phandle(pdev->dev.of_node, "fbdisplay-overlay", 0);
	if (vioc_node != NULL) {
		(void)of_property_read_u32(
			vioc_node, "telechips,fbdisplay_num",
			&overlay_drv->fb_dd_num);
	} else {
		(void)of_property_read_u32_index(
			pdev->dev.of_node, "display_num", 0U,
			&overlay_drv->fb_dd_num);
		(void)pr_info("[INF][OVERLAY] display_num = %d\n",
			overlay_drv->fb_dd_num);
	}

	if (overlay_drv->fb_dd_num > 2U) {
		goto err_overlay_drv_init;
	}

	overlay_drv->pclk = of_clk_get(pdev->dev.of_node, 0);
	if (IS_ERR(overlay_drv->pclk)) {
		overlay_drv->pclk = NULL;
	}

	itemp = of_alias_get_id(pdev->dev.of_node, "tcc-overlay-drv");
	if(itemp >= 0) {
		overlay_drv->id = (unsigned int)itemp;
	} else {
		overlay_drv->id = 0;
	}
	overlay_drv->misc->minor = MISC_DYNAMIC_MINOR;
	overlay_drv->misc->fops = &tcc_overlay_fops;
	if (overlay_drv->id == 0U) {
		overlay_drv->misc->name = kasprintf(GFP_KERNEL, "overlay");
	} else {
		overlay_drv->misc->name =
			kasprintf(GFP_KERNEL, "overlay%d", overlay_drv->id);
	}
	overlay_drv->misc->parent = &pdev->dev;
	ret = misc_register(overlay_drv->misc);
	if (ret != 0) {
		goto err_overlay_drv_init;
	}

	tmp_node = of_parse_phandle(pdev->dev.of_node, "rdmas", 0);
	if (tmp_node != NULL) {
		for (i = 0; i < OVERLAY_LAYER_MAX; i++) {
			ret = of_property_read_u32_index(pdev->dev.of_node, "rdmas",
			(overlay_drv->fb_dd_num * OVERLAY_LAYER_MAX) + i + 1U,
			&index);
			
			if(ret < 0) {
				overlay_drv->rdma[i].reg = NULL;
			} else {
				overlay_drv->rdma[i].reg = VIOC_RDMA_GetAddress(index);
				overlay_drv->rdma[i].id = index;
				#if defined(CONFIG_TCC_SCREEN_SHARE)
				if ((overlay_drv->fb_dd_num == 1U) && (i == 1)) {
					VIOC_RDMA_SetImageDisable(overlay_drv->rdma[i].reg);
				}
				#endif
			}

			dprintk("%s-%d :: fd_num(%d) => rdma[%d]: %d %d\n", __func__,
			__LINE__, overlay_drv->fb_dd_num, i, index, overlay_drv->rdma[i].id);

			if (IS_ERR(overlay_drv->rdma[i].reg)) {
				(void)pr_info("[INF][OVERLAY] rdma node of layer%d is n/a\n", i);
				overlay_drv->rdma[i].reg = NULL;
			}
		}
	} else {
		(void)pr_err("[ERR][OVERLAY] rdmas node is N/A\n");
		goto err_overlay_drv_init;
	}

	overlay_drv->layer_nlast = DEFAULT_OVERLAY_N;
	overlay_drv->layer_n = DEFAULT_OVERLAY_N;
	(void)of_property_read_u32(
		pdev->dev.of_node, "rdma_init_layer", &overlay_drv->layer_n);
	if (overlay_drv->layer_n > 3U) {
		overlay_drv->layer_nlast = DEFAULT_OVERLAY_N;
		overlay_drv->layer_n = DEFAULT_OVERLAY_N;
	}

	(void)pr_info("[INF][OVERLAY] overlay driver init layer :%d\n",
		overlay_drv->layer_n);

#if defined(CONFIG_VIOC_AFBCDEC)
	overlay_drv->afbc_dec.reg = NULL;
	overlay_drv->afbc_dec.id = 0;
#endif

	tmp_node = of_parse_phandle(pdev->dev.of_node, "wmixs", 0);
	(void)of_property_read_u32_index(
		pdev->dev.of_node, "wmixs",
		overlay_drv->fb_dd_num + 1U, &index);

	if (tmp_node != NULL) {
		overlay_drv->wmix.reg = VIOC_WMIX_GetAddress(index);
		overlay_drv->wmix.id = index;
		dprintk("%s-%d :: fd_num(%d) => wmix[%d]: %d %d/%p\n", __func__,
			__LINE__, overlay_drv->fb_dd_num, i, index,
			overlay_drv->wmix.id, overlay_drv->wmix.reg);
	} else {
		overlay_drv->wmix.reg = VIOC_WMIX_GetAddress(index);
	}

	if (IS_ERR(overlay_drv->wmix.reg)) {
		(void)pr_err("[ERR][OVERLAY] could not find wmix node of %s driver.\n",
		       overlay_drv->misc->name);
		overlay_drv->wmix.reg = NULL;
	}

	tmp_node = of_parse_phandle(pdev->dev.of_node, "scalers", 0);
	(void)of_property_read_u32_index(pdev->dev.of_node, "scalers", 1, &index);
	if ((tmp_node != NULL) && (index != VIOC_NO_COMPONENT)) {
		overlay_drv->sc.reg = VIOC_SC_GetAddress(index);
		overlay_drv->sc.id = index;
		dprintk("%s-%d :: fd_num(%d) => scaler[%d]: %d %d/%p\n", __func__, __LINE__, overlay_drv->fb_dd_num, i, index, overlay_drv->sc.id, overlay_drv->sc.reg);
	} else {
		overlay_drv->sc.reg = NULL;
	}

	#if defined(CONFIG_VIOC_MAP_DECOMP)
	tmp_node = of_parse_phandle(pdev->dev.of_node, "mc", 0);
	(void)of_property_read_u32_index(pdev->dev.of_node, "mc", 1, &index);
	if ((tmp_node != NULL) && (index != VIOC_NO_COMPONENT)) {
		overlay_drv->mc.id = index;
		dprintk("%s-%d :: overlay%d: MC(0x%x)\n", overlay_drv->id, overlay_drv->mc.id);
	} else {
		overlay_drv->mc.id = VIOC_NO_COMPONENT;
		dprintk("%s-%d :: overlay%d: MC Component not used\n", overlay_drv->id);
	}
	#endif

	if (IS_ERR(overlay_drv->sc.reg)) {
		dprintk("could not find scaler node of %s driver.\n", overlay_drv->misc->name);
		overlay_drv->sc.reg = NULL;
	}

	overlay_drv->open_cnt = 0;

	platform_set_drvdata(pdev, overlay_drv);
	goto overlay_exit;
err_overlay_drv_init:
	kfree(overlay_drv->misc);
	(void)pr_err("[ERR][OVERLAY] err_overlay_drv_init.\n");

err_overlay_drv_misc:
	kfree(overlay_drv);
	(void)pr_err("[ERR][OVERLAY] err_overlay_drv_misc.\n");

overlay_exit:
	return ret;
}

static int tcc_overlay_remove(struct platform_device *pdev)
{
	struct overlay_drv_type *overlay_drv =
		(struct overlay_drv_type *)platform_get_drvdata(pdev);

	misc_deregister(overlay_drv->misc);

	kfree(overlay_drv->misc);
	kfree(overlay_drv);
	return 0;
}

#ifdef CONFIG_PM
static int tcc_overlay_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct overlay_drv_type *overlay_drv = (struct overlay_drv_type *)platform_get_drvdata(pdev);

	(void)state;

	if (overlay_drv->open_cnt != 0U) {
		(void)pr_info("[INF][OVERLAY] %s %d opened\n", __func__,
			overlay_drv->open_cnt);
		clk_disable_unprepare(overlay_drv->pclk);
	}
	return 0;
}

static int tcc_overlay_resume(struct platform_device *pdev)
{
	int ret;

	struct overlay_drv_type *overlay_drv = (struct overlay_drv_type *)platform_get_drvdata(pdev);

	if (overlay_drv->open_cnt != 0U) {
		(void)pr_info("[INF][OVERLAY] %s %d opened\n", __func__,
			overlay_drv->open_cnt);
		ret = clk_prepare_enable(overlay_drv->pclk);
	}
	return 0;
}
#else // CONFIG_PM
#define tcc_overlay_suspend NULL
#define tcc_overlay_resume NULL
#endif // CONFIG_PM

#ifdef CONFIG_OF
static const struct of_device_id tcc_overlay_of_match[] = {
	{.compatible = "telechips,tcc_overlay"},
	{}
};
MODULE_DEVICE_TABLE(of, tcc_overlay_of_match);
#endif

static struct platform_driver tcc_overlay_driver = {
	.probe = tcc_overlay_probe,
	.remove = tcc_overlay_remove,
	.suspend = tcc_overlay_suspend,
	.resume = tcc_overlay_resume,
	.driver = {
		.name = "tcc_overlay",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(tcc_overlay_of_match),
#endif
	},
};

static void __exit tcc_overlay_cleanup(void)
{
	platform_driver_unregister(&tcc_overlay_driver);
}

static int __init tcc_overlay_init(void)
{
	(void)pr_info("%s\n", __func__);
	(void)platform_driver_register(&tcc_overlay_driver);
	return 0;
}

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC Video for Linux overlay driver");
MODULE_LICENSE("GPL");

module_init(tcc_overlay_init);
module_exit(tcc_overlay_cleanup);
