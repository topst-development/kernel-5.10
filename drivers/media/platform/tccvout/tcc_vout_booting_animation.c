/*
 * Copyright (C) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include "tcc_vout.h"
#include "tcc_vout_core.h"
#include "tcc_vout_dbg.h"
#include "tcc_vout_booting_animation.h"

#define TO_KERNEL_SPACE		1
#define TO_USER_SPACE		0

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#endif

extern const struct v4l2_fmtdesc tcc_fmtdesc[];
extern int tcc_vout_get_status(unsigned int type);

static int tc_v4l2_buffer_set_bootani(struct tcc_vout_device *vout, struct v4l2_buffer *buf)
{
	struct tcc_v4l2_buffer *qbuf = vout->qbufs + buf->index;
	unsigned long user_addr;
	unsigned int *p_user_addr;
	unsigned int *vid_buf;
	unsigned int size, high_addr, low_addr;
	//unsigned int i;
	int ret = 0;

	/*
		* MPLANE_VID
		*/
	size = vout->qbufs[vout->pushIdx].buf.m.planes[MPLANE_VID].reserved[0];
	low_addr = vout->qbufs[vout->pushIdx].buf.m.planes[MPLANE_VID].reserved[1];
	high_addr = vout->qbufs[vout->pushIdx].buf.m.planes[MPLANE_VID].reserved[2];
	dprintk("[AK] 0x%08x, 0x%08x, 0x%08x\n", size, low_addr, high_addr);

	if (high_addr == 0U) {
		user_addr = low_addr;
		p_user_addr = (unsigned __user *)user_addr;
		dprintk("[AK] 32bit addr [0x%lx] = 0x%px\n", user_addr, p_user_addr);
	}
	#if defined(CONFIG_ARM64)
	else {
		user_addr = high_addr;
		user_addr <<= 32;
		user_addr |= low_addr;
		p_user_addr = (unsigned __user *)user_addr;
		dprintk("[AK] 64bit addr [0x%lx] = 0x%px\n", user_addr, p_user_addr);
	}
	#endif

	vid_buf = memdup_user(p_user_addr, size * sizeof(unsigned int));
	if (IS_ERR_OR_NULL(vid_buf)) {
		dprintk("[AK] memdup_user failed (%d)\n", (int)PTR_ERR(vid_buf));
		ret = -ENOMEM;
		goto buffer_set_bootani_exit;
	}

	(void)memcpy(&qbuf->tc_vbuf.vid_reserved, vid_buf, sizeof(qbuf->tc_vbuf.vid_reserved));

	#if 0
	//verify test data
	for (i = 0; i < VIDEO_INFO_NUM; i++) {
		pr_err("[%2d] %4x", i, qbuf->tc_vbuf.vid_reserved[i]);
	}
	#endif

buffer_set_bootani_exit:
	if(p_user_addr != NULL) {
		kfree(vid_buf);
	}

	return ret;
}

void vout_video_overlay_bootani(struct tcc_vout_device *vout)
{
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int sw, sh;	// src image size in rdma
	unsigned int dw, dh;	// destination size in scaler
	int bypass = 0;

	sw = vioc->m2m_rdma.width;
	sh = vioc->m2m_rdma.height;
	dw = BOOTANIMATION_WIDTH;
	dh = BOOTANIMATION_HEIGHT;

	if ((dw == sw) && (dh == sh)) {
		bypass = 1;
	}
	VIOC_SC_SetBypass(vioc->sc.addr, bypass);

	VIOC_SC_SetDstSize(vioc->sc.addr,
		vout->disp_rect.left < 0 ? (vout->disp_rect.width + abs(vout->disp_rect.left)) : dw,
		vout->disp_rect.top < 0 ? (vout->disp_rect.height + abs(vout->disp_rect.top)) : dh);	// set destination size in scaler
	VIOC_SC_SetOutPosition(vioc->sc.addr,
		vout->disp_rect.left < 0 ? abs(vout->disp_rect.left) : 0,
		vout->disp_rect.top < 0 ? abs(vout->disp_rect.top) : 0);
	VIOC_SC_SetOutSize(vioc->sc.addr, dw, dh);	// set output size in scaler
	VIOC_SC_SetUpdate(vioc->sc.addr);

	// wdma
	vioc->m2m_wdma.width = dw;
	vioc->m2m_wdma.height = dh;
	VIOC_WDMA_SetImageSize(vioc->m2m_wdma.addr, vioc->m2m_wdma.width, vioc->m2m_wdma.height);
	VIOC_WDMA_SetImageOffset(vioc->m2m_wdma.addr, vioc->m2m_wdma.fmt, vioc->m2m_wdma.width);
	VIOC_WDMA_SetImageUpdate(vioc->m2m_wdma.addr);

	// rdma
	vioc->rdma.width = vout->disp_rect.width;
	vioc->rdma.height = vout->disp_rect.height;

	// wmix
	vioc->wmix.left = vout->disp_rect.left < 0 ? 0 : vout->disp_rect.left;
	vioc->wmix.top = vout->disp_rect.top < 0 ? 0 : vout->disp_rect.top;
}

int vidioc_streamon_bootani(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct tcc_vout_device *vout = fh;
	struct tcc_vout_vioc *vioc = vout->vioc;
	int ret = 0;

	mutex_lock(&vout->lock);

	if (i != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		pr_err("[ERR][VOUT] invalid v4l2_buf_type(%d)\n", i);
		ret = -EINVAL;
		goto streamon_bootani_exit;
	}

	/* If status is TCC_VOUT_STOP then we have to enable */
	if (vout->status == TCC_VOUT_STOP) {
		vout_disp_ctrl_bootani(vout, 1);	// enable disp1_path
		vout_m2m_ctrl(vioc, 1);		// enable deintl_path
	}

	vout->status = TCC_VOUT_RUNNING;

	dprintk("\n");

streamon_bootani_exit:
	mutex_unlock(&vout->lock);
	return ret;
}

int vidioc_streamoff_bootani(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct tcc_vout_device *vout = fh;
	struct tcc_vout_vioc *vioc = vout->vioc;

	mutex_lock(&vout->lock);

	if (i != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		pr_err("[ERR][VOUT] invalid v4l2_buf_type(%d)\n", i);
		return -EINVAL;
	}

	vout->status = TCC_VOUT_STOP;

	vout_disp_ctrl_bootani(vout, 0);	// disable disp1_path
	if (!vout->onthefly_mode) {
		vout_m2m_ctrl(vioc, 0);		// disable deintl_path
	}

	if(vout->id == VOUT_MAIN) {
		m2m_path_reset(vioc);
	}

	mutex_unlock(&vout->lock);
	dprintk("\n");
	return 0;
}


int tcc_deintl_buffer_set_bootani(struct tcc_vout_device *vout)
{
	struct tcc_v4l2_buffer *b;
	unsigned int total_offset, y_offset = 0;
	unsigned int i;
	unsigned int actual_bufs;
	unsigned int width, height;

	/* calculate buf count
	 * Size of display image can't be bigger than panel size
	 */
	if (vout->deintl_buf_size) {
		actual_bufs = ((unsigned int)vout->deintl_pmap.size)
			/ vout->deintl_buf_size;
	} else {
		actual_bufs = MIN_DEINTLBUF_NUM;
	}

	if (actual_bufs < 2) {
		pr_err("[ERR][VOUT] deintl_nr_bufs is not enough (%d).\n",
			actual_bufs);
		return -ENOMEM;
	}
	dprintk("request deintl_nr_bufs(%d), actual buf(%d)\n",
		vout->deintl_nr_bufs, actual_bufs);
	if ((vout->deintl_nr_bufs > actual_bufs) || (vout->deintl_nr_bufs <= 0)) {
		vout->deintl_nr_bufs = actual_bufs;
	}

	vout->deintl_bufs = kcalloc(vout->deintl_nr_bufs,
		sizeof(struct tcc_v4l2_buffer), GFP_KERNEL);
	if (!vout->deintl_bufs)
		return -ENOMEM;
	b = vout->deintl_bufs;

	/* The deintl_buf_size is calculated by the panel size.
	 *  - 20190809 alanK
	 */
	if (vout->id == VOUT_MAIN) {
		vout_wmix_getsize(vout, &width, &height);
	} else {
		width = vout->disp_rect.width;
		height = vout->disp_rect.height;
	}

	//for booting animation
	y_offset = width * height * 2;

	if (vout->vioc->m2m_wdma.fmt == VIOC_IMG_FMT_YUV420IL0) {
		total_offset = PAGE_ALIGN(y_offset * 3 / 2);
	} else {
		total_offset = PAGE_ALIGN(y_offset * 2);
	}

	/* set base address each buffers
	 */
	dprintk("  alloc_buf    img_base0   img_base1   img_base2\n");
	for (i = 0; i < vout->deintl_nr_bufs; i++) {
		b[i].index = i;
		b[i].img_base0 = vout->deintl_pmap.base + (total_offset * i);
		if (vout->vioc->m2m_wdma.fmt == VIOC_IMG_FMT_YUV420IL0) {
			b[i].img_base1 = b[i].img_base0 + y_offset;
		} else {
			b[i].img_base1 = 0;
		}
		b[i].img_base2 = 0;
		dprintk("     [%02d]     0x%08x  0x%08x  0x%08x\n",
			i, b[i].img_base0, b[i].img_base1, b[i].img_base2);
	}
	dprintk("-----------------------------------------------------\n");

	print_v4l2_reqbufs_format(vout->pfmt, vout->src_pix.pixelformat,
		"tcc_v4l2_buffer_set");
	return 0;
}

int vidioc_s_fmt_vid_out_overlay_bootani(struct file *file, void *fh,
	struct v4l2_format *f)
{
	struct tcc_vout_device *vout = fh;
	unsigned int width, height;
	int ret = 0;

	mutex_lock(&vout->lock);

	/* 2-pixel alignment */
	f->fmt.win.w.left = ROUND_DOWN_2(f->fmt.win.w.left);
	f->fmt.win.w.top = ROUND_DOWN_2(f->fmt.win.w.top);
	f->fmt.win.w.width = ROUND_DOWN_2(f->fmt.win.w.width);
	f->fmt.win.w.height = ROUND_DOWN_2(f->fmt.win.w.height);

	if (vout->disp_rect.left == f->fmt.win.w.left &&
		vout->disp_rect.top == f->fmt.win.w.top &&
		vout->disp_rect.width == f->fmt.win.w.width &&
		vout->disp_rect.height == f->fmt.win.w.height) {
		dprintk("Nothing to do because setting the same size\n");
		goto overlay_exit;
	}

	memcpy(&vout->disp_rect, &f->fmt.win.w, sizeof(struct v4l2_rect));

	vout_wmix_getsize(vout, &width, &height);
	if ((width == 0) || (height == 0)) {
		pr_err("[ERR][VOUT] output device size (%dx%d)\n",
			width, height);
		ret = -EBUSY;
		goto overlay_exit;
	}

	/* The image fits in the panel by cropping.
	 */
	if (vout->disp_rect.left < 0) {
		vout->disp_rect.width =
			vout->disp_rect.width + vout->disp_rect.left;
		if (width < vout->disp_rect.width) {
			vout->disp_rect.left = 0;
			vout->disp_rect.width = width;
		}
	} else {
		if (width < vout->disp_rect.left + vout->disp_rect.width)
			vout->disp_rect.width = width - vout->disp_rect.left;
	}

	if (vout->disp_rect.top < 0) {
		vout->disp_rect.height =
			vout->disp_rect.height + vout->disp_rect.top;
		if (height < vout->disp_rect.height) {
			vout->disp_rect.top = 0;
			vout->disp_rect.height = height;
		}
	} else {
		if (height < vout->disp_rect.top + vout->disp_rect.height)
			vout->disp_rect.height = height - vout->disp_rect.top;
	}

	vout->disp_rect.width = ROUND_DOWN_2(vout->disp_rect.width);
	vout->disp_rect.height = ROUND_DOWN_2(vout->disp_rect.height);

	if (vout->disp_rect.width <= 0 || vout->disp_rect.height <= 0) {
		vout->status = TCC_VOUT_STOP;
		goto overlay_exit;
	} else {
		vout->status = TCC_VOUT_RUNNING;
		/* prevent KCS warning */
	}
	vout_video_overlay_bootani(vout);

overlay_exit:
	mutex_unlock(&vout->lock);
	return ret;
}

void vout_deinit_bootani(struct tcc_vout_device *vout)
{
	struct tcc_vout_vioc *vioc = vout->vioc;

	VIOC_RDMA_SetImageDisable(vioc->rdma_bootani.addr);
}

int vout_set_vout_bootani_path(struct tcc_vout_device *vout)
{
	struct device_node *dev_np;
	struct tcc_vout_vioc *vioc = vout->vioc;

	// set display rdma
	dev_np = of_parse_phandle(vout->v4l2_np, "rdma_bootani", 0);
	if (dev_np) {
		/* swreser bit */
		of_property_read_u32_index(vout->v4l2_np, "rdma_bootani", 1, &vioc->rdma_bootani.id);
		vioc->rdma_bootani.addr = VIOC_RDMA_GetAddress(get_vioc_index(vioc->rdma_bootani.id));
		dprintk("[DISP] RDMA < vir_addr = 0x%p , id = %d \n", vioc->rdma_bootani.addr, get_vioc_index(vioc->rdma_bootani.id));
	} else {
		dprintk("[DISP] could not find rdma_bootani node of vout driver. \n");
	}

	// set display wmix
	dev_np = of_parse_phandle(vout->v4l2_np, "wmix_bootani", 0);
	if (dev_np) {
		/* swreser bit */
		of_property_read_u32_index(vout->v4l2_np, "wmix_bootani", 1, &vioc->wmix_bootani.id);
		vioc->wmix_bootani.addr = VIOC_WMIX_GetAddress(get_vioc_index(vioc->wmix_bootani.id));
		dprintk("[DISP] WMIX < vir_addr = 0x%p , id = %d \n", vioc->wmix_bootani.addr, get_vioc_index(vioc->wmix_bootani.id));
	} else {
		dprintk("[DISP] could not find wmix_bootani node of vout driver. \n");
	}

	switch (vioc->rdma_bootani.id) {
	case VIOC_RDMA07:
		if(get_vioc_index(vioc->wmix_bootani.id)) {
			vioc->wmix.pos = get_vioc_index(vioc->rdma_bootani.id) - (0x4 * get_vioc_index(vioc->wmix_bootani.id));
		} else {
			vioc->wmix.pos = get_vioc_index(vioc->rdma_bootani.id);
		}
		break;
	case VIOC_RDMA09:
		if(get_vioc_index(vioc->wmix_bootani.id)) {
			vioc->wmix.pos = get_vioc_index(vioc->rdma_bootani.id) - (0x4 * get_vioc_index(vioc->wmix_bootani.id));
		} else {
			vioc->wmix.pos = get_vioc_index(vioc->rdma_bootani.id);
		}
		break;
	default:
		pr_err("[ERR][VOUT] invalid rdma(%d) index\n", get_vioc_index(vioc->rdma_bootani.id));
		return -1;
	}

	dev_np = of_parse_phandle(vout->v4l2_np, "disp_bootani", 0);
	if (dev_np) {
		of_property_read_u32_index(vout->v4l2_np, "disp_bootani", 1, &vioc->disp_bootani.id);
		vioc->disp_bootani.addr = VIOC_DISP_GetAddress(get_vioc_index(vioc->disp_bootani.id));
		dprintk("[DISP] DISP < vir_addr = 0x%p , id = %d\n", vioc->disp_bootani.addr, get_vioc_index(vioc->disp_bootani.id));
	} else {
		dprintk("[DISP] could not find disp_bootani node of vout driver.\n");
	}

	dprintk("RDMA%d - WMIX%d - DISP%d\n", get_vioc_index(vioc->rdma.id), get_vioc_index(vioc->wmix.id), get_vioc_index(vioc->disp.id));
	return 0;
}

void scaler_setup_bootani(struct tcc_vout_device *vout)
{
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int sw, sh;	// src image size in rdma
	unsigned int dw, dh;	// destination size in scaler
	int bypass = 0;

	sw = vout->src_pix.width == vout->crop_src.width ? vout->src_pix.width : vout->crop_src.width;
	sh = vout->src_pix.height == vout->crop_src.height ? vout->src_pix.height : vout->crop_src.height;
	dw = BOOTANIMATION_WIDTH;
	dh = BOOTANIMATION_HEIGHT;

	if ((dw == sw) && (dh == sh)) {
		dprintk("Bypass scaler (same size)\n");
		bypass = 1;
	}

	VIOC_SC_SetBypass(vioc->sc.addr, bypass);
	VIOC_SC_SetDstSize(vioc->sc.addr,
		vout->disp_rect.left < 0 ? dw + abs(vout->disp_rect.left) : dw,
		vout->disp_rect.top < 0 ? dh + abs(vout->disp_rect.top) : dh);		// set destination size
	VIOC_SC_SetOutPosition(vioc->sc.addr,
		vout->disp_rect.left < 0 ? abs(vout->disp_rect.left) : 0,
		vout->disp_rect.top < 0 ? abs(vout->disp_rect.top) : 0);			// set output position
	VIOC_SC_SetOutSize(vioc->sc.addr, dw, dh);								// set output size in scaler

	#if defined(CONFIG_ARCH_TCC807X)
	VIOC_CONFIG_PlugIn(vioc->sc.id, vioc->m2m_rdma.id);	// plugin position in scaler
	#else
	VIOC_CONFIG_PlugIn(vioc->sc.id, vioc->m2m_wdma.id);	// plugin position in scaler
	#endif

	dprintk("%dx%d->[scaler]->%dx%d\n", sw, sh, dw, dh);
}


int vidioc_reqbufs_bootani(struct file *file, void *fh,
	struct v4l2_requestbuffers *req)
{
	struct tcc_vout_device *vout = fh;
	unsigned int max_frame = VIDEO_MAX_FRAME - vout->nr_qbufs;
	int i, ret = 0;

	dprintk("\n");

	/* free all buffers  */
	if (vout->qbufs && req->count == 0) {
		dprintk("free all buffers(%d) -> 0\n", vout->nr_qbufs);
		if (vout->nr_qbufs) {
			for (i = 0; i < vout->nr_qbufs; i++)
				kfree(vout->qbufs[i].buf.m.planes);
			kfree(vout->qbufs);
		}
		vout->nr_qbufs = 0;

		/* update buffer status */
		vout->mapped = OFF;
		goto reqbufs_exit;
	}

	if (!vout->mapped) {
		if (unlikely(req->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)) {
			pr_err(VOUT_NAME
				": [error] Invalid buf type(%d)\n", req->type);
			return -EINVAL;
		}

		mutex_lock(&vout->lock);

		vout->memory =
			vout->force_userptr ? V4L2_MEMORY_USERPTR : req->memory;

		if (vout->memory == V4L2_MEMORY_MMAP) {
			unsigned int actual_bufs, pmap_size;

			/* get pmap */
			ret = vout_get_pmap(&vout->pmap);
			if (ret) {
				pr_err("[ERR][VOUT] vout_get_pmap(%s)\n",
					vout->pmap.name);
				ret = -ENOMEM;
				goto reqbufs_exit_err;
			} else {
				if (tcc_vout_get_status(VOUT_SUB) >= 0) {
					pmap_size = vout->pmap.size/2;
					vout->pmap.base =
						(vout->id == VOUT_MAIN)
						? vout->pmap.base
						: (vout->pmap.base + pmap_size);
				} else {
					pmap_size = vout->pmap.size;
				}
			}

			/* calculate buf count */
			actual_bufs = pmap_size / vout->src_pix.sizeimage;
			dprintk(
				"V4L2_MEMORY_MMAP: request buf(%d), actual buf(%d)\n",
				req->count, actual_bufs);
			if ((req->count > actual_bufs)
				|| ((__s32)req->count < 0)) {
				req->count = actual_bufs;
			}
		}

		if (max_frame < req->count) {
			req->count = max_frame;
			/* prevent KCS warning */
		}

		vout->qbufs = kcalloc(req->count,
			sizeof(struct tcc_v4l2_buffer), GFP_KERNEL);
		if (!vout->qbufs) {
			return -ENOMEM;
			/* prevent KCS warning */
		}

		ret = tcc_v4l2_buffer_set_bootani(vout, req);
		if (ret < 0) {
			pr_err("[ERR][VOUT] tcc_v4l2_buffer_set_bootani(%d)\n", ret);
			goto reqbufs_exit;
		}

		vout->deintl_nr_bufs_count = 0;
		ret = tcc_deintl_buffer_set_bootani(vout);
		if (ret < 0) {
			pr_err("[ERR][VOUT] tcc_deintl_buffer_set_bootani(%d)\n", ret);
			goto reqbufs_deintl_err;
		}

		mutex_unlock(&vout->lock);

		/* update buffer status */
		vout->mapped = ON;

		print_v4l2_pix_format(&vout->src_pix,
			"_vidioc_reqbufs src info");
		print_v4l2_buf_type(req->type, "vidioc_reqbufs");
		print_v4l2_memory(req->memory, "vidioc_reqbufs");
		return ret;

reqbufs_deintl_err:
		/* free tcc_v4l2_buffer_set */
		for (i = 0; i < vout->nr_qbufs; i++)
			kfree(vout->qbufs[i].buf.m.planes);
		kfree(vout->qbufs);
reqbufs_exit_err:
		mutex_unlock(&vout->lock);
	} else {
		pr_warn("[WAR][VOUT] any buffers are still mapped(%d)\n",
			vout->nr_qbufs);
		req->count = vout->nr_qbufs;
	}
reqbufs_exit:
	return ret;
}

static irqreturn_t wdma_irq_handler_bootani(int irq, void *client_data)
{
	struct tcc_vout_device *vout = (struct tcc_vout_device *)client_data;
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int status;

	unsigned int base0, base1, base2;

	base0 = vioc->m2m_wdma.img.base0;
	base1 = vioc->m2m_wdma.img.base1;
	base2 = vioc->m2m_wdma.img.base2;

	if (!is_vioc_intr_activatied(vioc->m2m_wdma.vioc_intr->id, vioc->m2m_wdma.vioc_intr->bits)) {
		return IRQ_NONE;
	}

	VIOC_WDMA_GetStatus(vioc->m2m_wdma.addr, &status);
	if (status & VIOC_WDMA_IREQ_EOFR_MASK) {
		/* clear wdma interrupt status */
		vioc_intr_clear(vioc->m2m_wdma.vioc_intr->id, vioc->m2m_wdma.vioc_intr->bits);

		if(VIOC_DISP_Get_TurnOnOff(vioc->disp_bootani.addr)) {
			tcc_get_base_address(vioc->m2m_rdma.fmt, base0,
			BOOTANIMATION_WIDTH, vioc->m2m_rdma.height, 0,
			vout->crop_src.top, &base0, &base1, &base2);

			VIOC_WMIX_SetPosition(vioc->wmix_bootani.addr, vioc->wmix.pos, vioc->wmix.left, vioc->wmix.top);
			VIOC_WMIX_SetUpdate(vioc->wmix_bootani.addr);

			VIOC_RDMA_SetImageSize(vioc->rdma_bootani.addr, vioc->rdma.width, vioc->rdma.height);
			VIOC_RDMA_SetImageOffset(vioc->rdma_bootani.addr, vioc->rdma.fmt, BOOTANIMATION_WIDTH);
			VIOC_RDMA_SetImageBase(vioc->rdma_bootani.addr, base0, base1, base2);
			if (vout->clearFrameMode == OFF) {
				VIOC_RDMA_SetImageEnable(vioc->rdma_bootani.addr);
			}
		}

		if (VIOC_DISP_Get_TurnOnOff(vioc->disp.addr)) {
			tcc_get_base_address(vioc->m2m_rdma.fmt, base0,
				BOOTANIMATION_WIDTH, vioc->m2m_rdma.height, BOOTANIMATION_WIDTH / 2,
				vout->crop_src.top, &base0, &base1, &base2);

			VIOC_WMIX_SetPosition(vioc->wmix.addr, vioc->wmix.pos, vioc->wmix.left, vioc->wmix.top);
			VIOC_WMIX_SetUpdate(vioc->wmix.addr);

			VIOC_RDMA_SetImageSize(vioc->rdma.addr, vioc->rdma.width, vioc->rdma.height);
			VIOC_RDMA_SetImageOffset(vioc->rdma.addr, vioc->rdma.fmt, BOOTANIMATION_WIDTH);
			VIOC_RDMA_SetImageBase(vioc->rdma.addr, base0, base1, base2);
			if (vout->clearFrameMode == OFF)
				VIOC_RDMA_SetImageEnable(vioc->rdma.addr);
		}

		atomic_inc(&vout->displayed_buff_count);
		vout->wakeup_int = 1;
		wake_up_interruptible(&vout->frame_wait);
	}
	return IRQ_HANDLED;
}

int tcc_v4l2_buffer_set_bootani(struct tcc_vout_device *vout,
	struct v4l2_requestbuffers *req)
{
	struct tcc_v4l2_buffer *b = vout->qbufs;
	unsigned int index, y_offset = 0, uv_offset = 0, total_offset = 0;

	/* for sub_plane */
	for (index = vout->nr_qbufs;
		index < (vout->nr_qbufs + req->count); index++) {

		// WARNING: Prefer kcalloc over kzalloc with multiply
		//b[index].buf.m.planes = kzalloc(
		//	sizeof(struct v4l2_plane) * MPLANE_NUM, GFP_KERNEL);
		b[index].buf.m.planes = kcalloc(
			MPLANE_NUM, sizeof(struct v4l2_plane), GFP_KERNEL);
	}

	if (req->memory == V4L2_MEMORY_USERPTR) {
		for (index = vout->nr_qbufs;
			index < (vout->nr_qbufs + req->count); index++) {
			b[index].index = index;
		}

		/* update current number of created buffer */
		vout->nr_qbufs += req->count;

		/*
		 * We don't have to set v4l2 bufs.
		 */
		return 0;
	}

	switch (vout->pfmt) {
	case TCC_PFMT_YUV422:
		y_offset = vout->src_pix.width * vout->src_pix.height;
		uv_offset = y_offset / 2;
		break;
	case TCC_PFMT_YUV420:
		y_offset = vout->src_pix.width * vout->src_pix.height;
		uv_offset = y_offset / 4;
		break;
	case TCC_PFMT_RGB:
	default:
		break;
	}
	total_offset = PAGE_ALIGN(y_offset + uv_offset * 2);

	/* set base address each buffers
	 */
	dprintk("  alloc_buf    img_base0   img_base1   img_base2\n");
	for (index = vout->nr_qbufs;
		index < (vout->nr_qbufs + req->count); index++) {
		b[index].index = index;

		switch (vout->src_pix.pixelformat) {
		case V4L2_PIX_FMT_YUV420:
		case V4L2_PIX_FMT_YVU420:
		case V4L2_PIX_FMT_YUV422P:
			b[index].img_base0 =
				vout->pmap.base + (total_offset * index);
			b[index].img_base1 = b[index].img_base0 + y_offset;
			b[index].img_base2 = b[index].img_base1 + uv_offset;
			break;

		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
		case V4L2_PIX_FMT_NV16:
		case V4L2_PIX_FMT_NV61:
			b[index].img_base0 =
				vout->pmap.base + (total_offset * index);
			b[index].img_base1 = b[index].img_base0 + y_offset;
			b[index].img_base2 = 0;
			break;

		case V4L2_PIX_FMT_UYVY:
		case V4L2_PIX_FMT_VYUY:
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_YVYU:
			b[index].img_base0 =
				vout->pmap.base + (total_offset * index);
			b[index].img_base1 = 0;
			b[index].img_base2 = 0;
			break;

		/* RGB and etc... */
		default:
			b[index].img_base0 = vout->pmap.base +
				(vout->src_pix.sizeimage * index);
			b[index].img_base1 = 0;
			b[index].img_base2 = 0;
		}
	}
	dprintk("-----------------------------------------------------\n");

	// update current number of created buffer
	vout->nr_qbufs += req->count;

	print_v4l2_reqbufs_format(vout->pfmt, vout->src_pix.pixelformat,
		"tcc_v4l2_buffer_set");
	return 0;
}

void vout_disp_ctrl_bootani(struct tcc_vout_device *vout, int enable)
{
	struct tcc_vout_vioc *vioc = vout->vioc;

	if (enable) {
		VIOC_RDMA_SetImageEnable(vioc->rdma.addr);
	} else {
		VIOC_RDMA_SetImageDisable(vioc->rdma.addr);
	}
	dprintk("%d\n", enable);

	if (enable) {
		VIOC_RDMA_SetImageEnable(vioc->rdma_bootani.addr);
	} else {
		udelay(1);
		VIOC_RDMA_SetImageDisable(vioc->rdma_bootani.addr);
	}
}


int vout_m2m_init_bootani(struct tcc_vout_device *vout)
{
	struct tcc_vout_vioc *vioc = vout->vioc;
	struct vioc_rdma *rdma = &vioc->m2m_rdma;
	struct vioc_wdma *wdma = &vioc->m2m_wdma;
	int ret = 0;

	vout->wakeup_int = 0;
	vout->frame_count = 0;

	/* 0. reset deintl_path */
	m2m_path_reset(vioc);

	/* 1. rdma */
	rdma->intl = vout->deinterlace > VOUT_DEINTL_VIQE_BYPASS ? 1 : 0;
	rdma->img.base0 = 0;
	rdma->img.base1 = 0;
	rdma->img.base2 = 0;
	rdma->width = vout->src_pix.width;

	/*
	 * The VIQE need 4-line align
	 */
	if ((vout->src_pix.height % 4) &&
		(vout->deinterlace == VOUT_DEINTL_VIQE_3D || vout->deinterlace == VOUT_DEINTL_VIQE_2D)) {
		rdma->height = ROUND_UP_4(vout->src_pix.height);
		dprintk("viqe 4-line align: %d -> %d\n", vout->src_pix.height, rdma->height);
	} else {
		rdma->height = vout->src_pix.height;
	}

	rdma->y_stride = vout->src_pix.bytesperline & 0x0000ffff;
	dprintk("Y-stride(%d) UV-stride(%d)\n", rdma->y_stride,
			(vout->src_pix.bytesperline & 0xffff0000) >> 16);

	m2m_rdma_setup(rdma);

	/* 2. deinterlacer */
	switch (vout->deinterlace) {
	case VOUT_DEINTL_VIQE_3D:
	case VOUT_DEINTL_VIQE_2D:
	case VOUT_DEINTL_VIQE_BYPASS:
		ret = deintl_viqe_setup(vout, vout->deinterlace, 1);
		break;
	case VOUT_DEINTL_S:
		ret = deintl_s_setup(vout);
		break;
	case VOUT_DEINTL_NONE:
		/* only use rdma-wmix-wdma */
		break;
	default:
		return -EINVAL;
	}

	/* 3. wmixer */
	vioc->m2m_wmix.left = 0;		// default: to assume sub-plane size same video size
	vioc->m2m_wmix.top = 0;
	vioc->m2m_wmix.width = vout->src_pix.width;
	vioc->m2m_wmix.height = vout->src_pix.height;
	VIOC_CONFIG_WMIXPath(vioc->m2m_rdma.id, 1);
	m2m_wmix_setup(&vioc->m2m_wmix);

	/* 4. scaler */
	scaler_setup_bootani(vout);

	/* 5. wdma */
	wdma->width = BOOTANIMATION_WIDTH;
	wdma->height = BOOTANIMATION_HEIGHT;

	wdma->cont = 0;							// 0: frame-by-frame mode
	m2m_wdma_setup(wdma);

	/* wdma interrupt */
	VIOC_CONFIG_StopRequest(0);
	if (vioc->m2m_wdma.irq_enable == 0) {
		vioc->m2m_wdma.irq_enable++;		// set interrupt flag
		synchronize_irq(vioc->m2m_wdma.irq);
		vioc_intr_clear(vioc->m2m_wdma.vioc_intr->id, vioc->m2m_wdma.vioc_intr->bits);
		ret = request_irq(vioc->m2m_wdma.irq, wdma_irq_handler_bootani,
			IRQF_SHARED, vout->vdev->name, vout);
		if (ret)
			pr_err("[ERR][VOUT] wdma_irq_handler failed(%d)\n", ret);
		vioc_intr_enable(vioc->m2m_wdma.irq, vioc->m2m_wdma.vioc_intr->id, vioc->m2m_wdma.vioc_intr->bits);
	}

	print_vioc_deintl_path(vout, "vout_m2m_init");
	return ret;
}

void vout_rdma_setup_bootani(struct tcc_vout_device *vout)
{
	struct tcc_vout_vioc *vioc = vout->vioc;
	struct vioc_rdma *disp_rdma = &vioc->rdma;

	disp_rdma->width = PANEL_WIDTH;
	disp_rdma->height = PANEL_HEIGHT;
	VIOC_RDMA_SetImageSize(disp_rdma->addr, disp_rdma->width, disp_rdma->height);
	VIOC_RDMA_SetImageFormat(disp_rdma->addr, disp_rdma->fmt);
	VIOC_RDMA_SetImageOffset(disp_rdma->addr, disp_rdma->fmt, disp_rdma->width);
	VIOC_RDMA_SetImageY2REnable(disp_rdma->addr, disp_rdma->y2r);
	VIOC_RDMA_SetImageY2RMode(disp_rdma->addr, disp_rdma->y2rmd);
	VIOC_RDMA_SetImageDisable(disp_rdma->addr);

	//for booting animation
	VIOC_RDMA_SetImageBfield(vioc->rdma_bootani.addr, disp_rdma->bf);
	VIOC_RDMA_SetImageIntl(vioc->rdma_bootani.addr, disp_rdma->intl);
	VIOC_RDMA_SetImageFormat(vioc->rdma_bootani.addr, disp_rdma->fmt);
	VIOC_RDMA_SetImageY2REnable(vioc->rdma_bootani.addr, disp_rdma->y2r);
	VIOC_RDMA_SetImageDisable(vioc->rdma_bootani.addr);
}

void vout_m2m_display_update_bootani(struct tcc_vout_device *vout, struct v4l2_buffer *buf)
{
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int base0 = 0, base1 = 0, base2 = 0;
	unsigned int index = 0;
	unsigned int res_change = OFF;

	/* This is the code for only m2m path without vsync */
	if (vout->deintl_force == 0) {
		vout->src_pix.field = buf->field;
	}

	if (vout->firstFieldFlag == 0) {		// first field
		if ((vout->src_pix.field == (u32)V4L2_FIELD_INTERLACED_TB)
			|| (vout->src_pix.field == (u32)V4L2_FIELD_INTERLACED_BT)
			|| (vout->src_pix.field == (u32)V4L2_FIELD_INTERLACED)) {
			vout->firstFieldFlag++;
		}
	}

	/* get base address */
	if (vout->memory  == V4L2_MEMORY_USERPTR) {
		if (buf->timecode.type != STILL_IMGAGE) {
			base0 = clamp_t(u32, (buf->m.planes[MPLANE_VID].m.userptr), 0, (UINT_MAX));
			base1 = clamp_t(u32, (buf->m.planes[MPLANE_VID].reserved[VID_BASE1]), 0, (UINT_MAX));
			base2 = clamp_t(u32, (buf->m.planes[MPLANE_VID].reserved[VID_BASE2]), 0, (UINT_MAX));
		} else {
			base0 = clamp_t(u32, (buf->m.planes[MPLANE_VID].m.userptr), 0, (UINT_MAX));
			base1 = 0;
			base2 = 0;

			/*
			 * If the input is YUV format.
			 */
			if (vout->pfmt != (int)TCC_PFMT_RGB) {
				/*
				 * Re-store src_pix.height from "The VIQE need 4-line align"
				 */
				if (vioc->m2m_rdma.height != vout->src_pix.height) {
					vioc->m2m_rdma.height = vout->src_pix.height;
					m2m_rdma_setup(&vioc->m2m_rdma);
				}

				(void)tcc_get_base_address_of_image(
					vout->src_pix.pixelformat, base0,
					vout->src_pix.width, vout->src_pix.height,
					&base0, &base1, &base2);
			}
		}
	}

	if (vout->src_pix.field == (u32)V4L2_FIELD_INTERLACED_BT) {
		vioc->m2m_rdma.bf = 1;
	} else {
		vioc->m2m_rdma.bf = 0;
	}

	/* To prevent frequent switching of the scan type */
	if (vout->first_frame != 0) {
		vout->first_field = (enum v4l2_field)vout->src_pix.field;
		vout->first_frame = 0;
		dprintk("first_field(%s)\n", v4l2_field_names[vout->first_field]);
	}

	index = (vout->deintl_nr_bufs_count++ % vout->deintl_nr_bufs);
	if (vout->deintl_nr_bufs_count == vout->deintl_nr_bufs) {
		vout->deintl_nr_bufs_count = 0;
	}

	if ((vout->src_pix.width != vout->crop_src.width) || (vout->src_pix.height != vout->crop_src.height)) {
		(void)tcc_get_base_address(vioc->m2m_rdma.fmt, base0,
			(vioc->m2m_rdma.y_stride != 0U) ? vioc->m2m_rdma.y_stride : vioc->m2m_rdma.width,
			vioc->m2m_rdma.height,
			vout->crop_src.left, vout->crop_src.top,
			&base0, &base1, &base2);
	}

	/* rdma stride */
	if ((buf->m.planes[MPLANE_VID].bytesused != 0U) && (vioc->m2m_rdma.y_stride != buf->m.planes[MPLANE_VID].bytesused)) {
		dprintk("update rdma stride(%d -> %d)\n", vioc->m2m_rdma.y_stride, buf->m.planes[MPLANE_VID].bytesused);

		/*  change rdma stride */
		vioc->m2m_rdma.y_stride = buf->m.planes[MPLANE_VID].bytesused;
		vout->src_pix.bytesperline = buf->m.planes[MPLANE_VID].bytesused;
		VIOC_RDMA_SetImageOffset(vioc->m2m_rdma.addr, vioc->m2m_rdma.fmt, vioc->m2m_rdma.y_stride);
	}

	/* dec output format */
	if (vout->memory == V4L2_MEMORY_USERPTR) {
		vout_check_format(&vioc->m2m_rdma, buf->m.planes[MPLANE_VID].reserved[VID_MJPEG_FORMAT]);
	}

	/* rdma base address */
	vioc->m2m_rdma.img.base0 = base0;
	vioc->m2m_rdma.img.base1 = base1;
	vioc->m2m_rdma.img.base2 = base2;
	/* wdma base address */
	vioc->m2m_wdma.img.base0 = vout->deintl_bufs[index].img_base0;
	vioc->m2m_wdma.img.base1 = vout->deintl_bufs[index].img_base1;
	vioc->m2m_wdma.img.base2 = vout->deintl_bufs[index].img_base2;

	{
		if ((res_change != 0) || (vout->previous_field != vout->src_pix.field)) {
			vout->previous_field = vout->src_pix.field;
			if (vout->src_pix.field == V4L2_FIELD_INTERLACED_TB
				|| vout->src_pix.field == V4L2_FIELD_INTERLACED_BT
				|| vout->src_pix.field == V4L2_FIELD_INTERLACED) {
				switch (vout->deinterlace) {
				case VOUT_DEINTL_VIQE_3D:
				case VOUT_DEINTL_VIQE_2D:
					if (res_change != OFF) {
						(void)deintl_viqe_setup(vout, vout->deinterlace, 0);
					} else {
						VIOC_VIQE_SetDeintlMode(vioc->viqe.addr, VIOC_VIQE_DEINTL_MODE_2D);
						VIOC_VIQE_SetControlMode(vioc->viqe.addr, OFF, OFF, OFF, OFF, ON);
					}

					if (vout->src_pix.colorspace == (u32)V4L2_COLORSPACE_JPEG) {
						VIOC_RDMA_SetImageY2REnable(vioc->m2m_rdma.addr, 0U);	// force disable y2r
					}
					break;
				case VOUT_DEINTL_S:
					(void)deintl_s_setup(vout);
					break;
				default:
					break;
				}
				VIOC_RDMA_SetImageIntl(vioc->m2m_rdma.addr, 1);
				dprintk("enable deintl(%d)\n", vout->deinterlace);
			} else {
				switch (vout->deinterlace) {
				case VOUT_DEINTL_VIQE_3D:
				case VOUT_DEINTL_VIQE_2D:
					{
						VIOC_VIQE_SetDeintlMode(vioc->viqe.addr, VIOC_VIQE_DEINTL_MODE_BYPASS);
						VIOC_VIQE_SetControlMode(vioc->viqe.addr, OFF, OFF, OFF, OFF, OFF);
						vout->frame_count = 0;

						if (vout->src_pix.colorspace == (u32)V4L2_COLORSPACE_JPEG) {
							VIOC_RDMA_SetImageY2REnable(vioc->m2m_rdma.addr, 1); // force enable y2r
						}
					}
					break;
				case VOUT_DEINTL_S:
					(void)VIOC_CONFIG_PlugOut(vioc->deintl_s.id);
					break;
				default:
					break;
				}
				VIOC_RDMA_SetImageIntl(vioc->m2m_rdma.addr, 0);
				dprintk("disable deintl(%d)\n", vout->deinterlace);
			}
		}

		/* update rdma (bfield and src addr.).
		 */
		if (vioc->m2m_rdma.width <= 3072) {
			VIOC_RDMA_SetImageUVIEnable(vioc->m2m_rdma.addr, true);
		} else {
			VIOC_RDMA_SetImageUVIEnable(vioc->m2m_rdma.addr, false);
		}
		VIOC_RDMA_SetImageBfield(vioc->m2m_rdma.addr, vioc->m2m_rdma.bf);
		VIOC_RDMA_SetImageBase(vioc->m2m_rdma.addr,
			vioc->m2m_rdma.img.base0, vioc->m2m_rdma.img.base1, vioc->m2m_rdma.img.base2);

		VIOC_RDMA_SetImageEnable(vioc->m2m_rdma.addr);

		/* update wdma (dst addr.)
		 * the wdma was enabled only by vout_m2m_ctrl
		 */
		VIOC_WDMA_SetImageBase(vioc->m2m_wdma.addr,
			vioc->m2m_wdma.img.base0, vioc->m2m_wdma.img.base1, vioc->m2m_wdma.img.base2);
		vout_m2m_ctrl(vioc, 1);
	}

	vout->last_cleared_buffer = buf;

	if ((vout->src_pix.field == (u32)V4L2_FIELD_INTERLACED_TB)
				|| (vout->src_pix.field == (u32)V4L2_FIELD_INTERLACED_BT)
				|| (vout->src_pix.field == (u32)V4L2_FIELD_INTERLACED)) {
next_field:
		if (wait_event_interruptible_timeout(vout->frame_wait, vout->wakeup_int == 1, msecs_to_jiffies(200U)) <= 0) {
			(void)pr_err("[ERR][VOUT] [interlace] handler timeout\n");
			if (vout->firstFieldFlag == 0) {
				atomic_inc(&vout->displayed_buff_count);
			}
		}
		vout->wakeup_int = 0;

		if (vout->firstFieldFlag != 0) {
			unsigned int index = (vout->deintl_nr_bufs_count++ % vout->deintl_nr_bufs);

			if (vout->deintl_nr_bufs_count == vout->deintl_nr_bufs) {
				vout->deintl_nr_bufs_count = 0;
			}

			/* wdma base address */
			vioc->m2m_wdma.img.base0 = vout->deintl_bufs[index].img_base0;
			vioc->m2m_wdma.img.base1 = vout->deintl_bufs[index].img_base1;
			vioc->m2m_wdma.img.base2 = vout->deintl_bufs[index].img_base2;
			VIOC_WDMA_SetImageBase(vioc->m2m_wdma.addr, vioc->m2m_wdma.img.base0, vioc->m2m_wdma.img.base1, vioc->m2m_wdma.img.base2);

			/* change rdma bfield */
			VIOC_RDMA_SetImageBfield(vioc->m2m_rdma.addr, (vioc->m2m_rdma.bf > 0U) ? 0U : 1U);
			VIOC_RDMA_SetImageUpdate(vioc->m2m_rdma.addr);

			vout->firstFieldFlag = 0;

			/* enable wdma */
			vout_m2m_ctrl(vioc, 1);
			goto next_field;
		}
	} else {
		if (wait_event_interruptible_timeout(vout->frame_wait, vout->wakeup_int == 1, msecs_to_jiffies(200)) <= 0) {
			(void)pr_err("[ERR][VOUT] [progressive] handler timeout\n");
			atomic_inc(&vout->displayed_buff_count);
		}
		vout->wakeup_int = 0;
	}
}

void vout_m2m_display_update_bootani_reserved11(struct tcc_vout_device *vout, struct v4l2_buffer *buf)
{
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int base0 = 0, base1 = 0, base2 = 0;
	unsigned int index = 0;
	unsigned int res_change = OFF;

	/* This is the code for only m2m path without vsync */
	if (vout->deintl_force == 0) {
		vout->src_pix.field = buf->field;
	}

	if (vout->firstFieldFlag == 0) {		// first field
		if ((vout->src_pix.field == (u32)V4L2_FIELD_INTERLACED_TB)
			|| (vout->src_pix.field == (u32)V4L2_FIELD_INTERLACED_BT)
			|| (vout->src_pix.field == (u32)V4L2_FIELD_INTERLACED)) {
			vout->firstFieldFlag++;
		}
	}

	/* get base address */
	if (vout->memory  == V4L2_MEMORY_USERPTR) {
		if (buf->timecode.type != STILL_IMGAGE) {
			base0 = clamp_t(u32, (buf->m.planes[MPLANE_VID].m.userptr), 0, (UINT_MAX));
			base1 = clamp_t(u32, (vout->qbufs[buf->index].tc_vbuf.vid_reserved[VID_BASE1]), 0, (UINT_MAX));
			base2 = clamp_t(u32, (vout->qbufs[buf->index].tc_vbuf.vid_reserved[VID_BASE2]), 0, (UINT_MAX));
		} else {
			base0 = clamp_t(u32, (buf->m.planes[MPLANE_VID].m.userptr), 0, (UINT_MAX));
			base1 = 0;
			base2 = 0;

			/*
			 * If the input is YUV format.
			 */
			if (vout->pfmt != (int)TCC_PFMT_RGB) {
				/*
				 * Re-store src_pix.height from "The VIQE need 4-line align"
				 */
				if (vioc->m2m_rdma.height != vout->src_pix.height) {
					vioc->m2m_rdma.height = vout->src_pix.height;
					m2m_rdma_setup(&vioc->m2m_rdma);
				}

				(void)tcc_get_base_address_of_image(
					vout->src_pix.pixelformat, base0,
					vout->src_pix.width, vout->src_pix.height,
					&base0, &base1, &base2);
			}
		}
	} else if (vout->memory == V4L2_MEMORY_MMAP) {
		base0 = vout->qbufs[buf->index].img_base0;
		base1 = vout->qbufs[buf->index].img_base1;
		base2 = vout->qbufs[buf->index].img_base2;
	} else {
		(void)pr_err("[ERR][VOUT] invalid qbuf v4l2_memory\n");
	}

	if (vout->src_pix.field == (__u32)V4L2_FIELD_INTERLACED_BT) {
		vioc->m2m_rdma.bf = 1;
	} else {
		vioc->m2m_rdma.bf = 0;
	}

	/* To prevent frequent switching of the scan type */
	if (vout->first_frame != 0) {
		vout->first_field = (enum v4l2_field)vout->src_pix.field;
		vout->first_frame = 0;
		dprintk("first_field(%s)\n", v4l2_field_names[vout->first_field]);
	}

	index = (vout->deintl_nr_bufs_count++ % vout->deintl_nr_bufs);
	if (vout->deintl_nr_bufs_count == vout->deintl_nr_bufs)
		vout->deintl_nr_bufs_count = 0;

	if ((vout->src_pix.width != vout->crop_src.width) || (vout->src_pix.height != vout->crop_src.height)) {
		tcc_get_base_address(vioc->m2m_rdma.fmt, base0,
			vioc->m2m_rdma.y_stride ? vioc->m2m_rdma.y_stride : vioc->m2m_rdma.width,
			vioc->m2m_rdma.height,
			vout->crop_src.left, vout->crop_src.top,
			&base0, &base1, &base2);
	}

	/* rdma stride */
	if ((buf->m.planes[MPLANE_VID].bytesused != 0U) && (vioc->m2m_rdma.y_stride != buf->m.planes[MPLANE_VID].bytesused)) {
		dprintk("update rdma stride(%d -> %d)\n", vioc->m2m_rdma.y_stride, buf->m.planes[MPLANE_VID].bytesused);

		/*  change rdma stride */
		vioc->m2m_rdma.y_stride = buf->m.planes[MPLANE_VID].bytesused;
		vout->src_pix.bytesperline = buf->m.planes[MPLANE_VID].bytesused;
		VIOC_RDMA_SetImageOffset(vioc->m2m_rdma.addr, vioc->m2m_rdma.fmt, vioc->m2m_rdma.y_stride);
	}

	/* dec output format */
	if (vout->memory == V4L2_MEMORY_USERPTR) {
		vout_check_format(&vioc->m2m_rdma, vout->qbufs[buf->index].tc_vbuf.vid_reserved[VID_MJPEG_FORMAT]);
	}

	/* rdma base address */
	vioc->m2m_rdma.img.base0 = base0;
	vioc->m2m_rdma.img.base1 = base1;
	vioc->m2m_rdma.img.base2 = base2;
	/* wdma base address */
	vioc->m2m_wdma.img.base0 = vout->deintl_bufs[index].img_base0;
	vioc->m2m_wdma.img.base1 = vout->deintl_bufs[index].img_base1;
	vioc->m2m_wdma.img.base2 = vout->deintl_bufs[index].img_base2;

	{
		if ((res_change != 0U) || (vout->previous_field != (enum v4l2_field)vout->src_pix.field))
		{
			vout->previous_field = (enum v4l2_field)vout->src_pix.field;
			if ((vout->src_pix.field == (u32)V4L2_FIELD_INTERLACED_TB)
				|| (vout->src_pix.field == (u32)V4L2_FIELD_INTERLACED_BT)
				|| (vout->src_pix.field == (u32)V4L2_FIELD_INTERLACED)) {
				switch (vout->deinterlace) {
				case VOUT_DEINTL_VIQE_3D:
				case VOUT_DEINTL_VIQE_2D:
					if (res_change != OFF) {
						(void)deintl_viqe_setup(vout, vout->deinterlace, 0);
					} else {
						VIOC_VIQE_SetDeintlMode(vioc->viqe.addr, VIOC_VIQE_DEINTL_MODE_2D);
						VIOC_VIQE_SetControlMode(vioc->viqe.addr, OFF, OFF, OFF, OFF, ON);
					}

					if (vout->src_pix.colorspace == (u32)V4L2_COLORSPACE_JPEG) {
						VIOC_RDMA_SetImageY2REnable(vioc->m2m_rdma.addr, 0U);	// force disable y2r
					}
					break;
				case VOUT_DEINTL_S:
					(void)deintl_s_setup(vout);
					break;
				default:
					/* prevent KCS */
					break;
				}
				VIOC_RDMA_SetImageIntl(vioc->m2m_rdma.addr, 1);
				dprintk("enable deintl(%d)\n", vout->deinterlace);
			} else {
				switch (vout->deinterlace) {
				case VOUT_DEINTL_VIQE_3D:
				case VOUT_DEINTL_VIQE_2D:
					VIOC_VIQE_SetDeintlMode(vioc->viqe.addr, VIOC_VIQE_DEINTL_MODE_BYPASS);
					VIOC_VIQE_SetControlMode(vioc->viqe.addr, OFF, OFF, OFF, OFF, OFF);
					vout->frame_count = 0;

					if (vout->src_pix.colorspace == (__u32)V4L2_COLORSPACE_JPEG) {
						VIOC_RDMA_SetImageY2REnable(vioc->m2m_rdma.addr, 1); // force enable y2r
					}
					break;
				case VOUT_DEINTL_S:
					(void)VIOC_CONFIG_PlugOut(vioc->deintl_s.id);
					break;
				default:
					/* prevent KCS */
					break;
				}
				VIOC_RDMA_SetImageIntl(vioc->m2m_rdma.addr, 0U);
				dprintk("disable deintl(%d)\n", vout->deinterlace);
			}
		}

		/* update rdma (bfield and src addr.).
		 */
		if (vioc->m2m_rdma.width <= 3072U) {
			VIOC_RDMA_SetImageUVIEnable(vioc->m2m_rdma.addr, 1U);	//true
		} else {
			VIOC_RDMA_SetImageUVIEnable(vioc->m2m_rdma.addr, 0U);	//false
		}
		VIOC_RDMA_SetImageBfield(vioc->m2m_rdma.addr, vioc->m2m_rdma.bf);
		VIOC_RDMA_SetImageBase(vioc->m2m_rdma.addr,
			vioc->m2m_rdma.img.base0, vioc->m2m_rdma.img.base1, vioc->m2m_rdma.img.base2);

		VIOC_RDMA_SetImageEnable(vioc->m2m_rdma.addr);

		/* update wdma (dst addr.)
		 * the wdma was enabled only by vout_m2m_ctrl
		 */
		VIOC_WDMA_SetImageBase(vioc->m2m_wdma.addr,
			vioc->m2m_wdma.img.base0, vioc->m2m_wdma.img.base1, vioc->m2m_wdma.img.base2);
		vout_m2m_ctrl(vioc, 1);
	}

	vout->last_cleared_buffer = buf;

	if ((vout->src_pix.field == (__u32)V4L2_FIELD_INTERLACED_TB)
				|| (vout->src_pix.field == (__u32)V4L2_FIELD_INTERLACED_BT)
				|| (vout->src_pix.field == (__u32)V4L2_FIELD_INTERLACED)) {
next_field:
		if (wait_event_interruptible_timeout(vout->frame_wait, vout->wakeup_int == 1, msecs_to_jiffies(200U)) <= 0) {
			(void)pr_err("[ERR][VOUT] [interlace] handler timeout\n");
			if (vout->firstFieldFlag == 0) {
				atomic_inc(&vout->displayed_buff_count);
			}
		}
		vout->wakeup_int = 0;

		if (vout->firstFieldFlag != 0) {
			unsigned int idx = vout->deintl_nr_bufs_count++ % vout->deintl_nr_bufs;

			if (vout->deintl_nr_bufs_count == vout->deintl_nr_bufs) {
				vout->deintl_nr_bufs_count = 0;
			}

			/* wdma base address */
			vioc->m2m_wdma.img.base0 = vout->deintl_bufs[idx].img_base0;
			vioc->m2m_wdma.img.base1 = vout->deintl_bufs[idx].img_base1;
			vioc->m2m_wdma.img.base2 = vout->deintl_bufs[idx].img_base2;
			VIOC_WDMA_SetImageBase(vioc->m2m_wdma.addr, vioc->m2m_wdma.img.base0, vioc->m2m_wdma.img.base1, vioc->m2m_wdma.img.base2);

			/* change rdma bfield */
			VIOC_RDMA_SetImageBfield(vioc->m2m_rdma.addr, (vioc->m2m_rdma.bf > 0U) ? 0U : 1U);
			VIOC_RDMA_SetImageUpdate(vioc->m2m_rdma.addr);

			vout->firstFieldFlag = 0;

			/* enable wdma */
			vout_m2m_ctrl(vioc, 1);
			goto next_field;
		}
	} else {
		if (wait_event_interruptible_timeout(vout->frame_wait, vout->wakeup_int == 1, msecs_to_jiffies(200U)) <= 0) {
			(void)pr_err("[ERR][VOUT] [progressive] handler timeout\n");
			atomic_inc(&vout->displayed_buff_count);
		}
		vout->wakeup_int = 0;
	}
}

int vidioc_qbuf_bootani(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	struct tcc_vout_device *vout = fh;
	int backupIdx;
	int ret = 0;

	if (atomic_read(&vout->readable_buff_count) == vout->nr_qbufs) {
		pr_err("[ERR][VOUT] buffer full\n");
		return -EIO;
	}

	backupIdx = vout->pushIdx;
	if (++vout->pushIdx >= vout->nr_qbufs)
		vout->pushIdx = 0;

	if (tcc_vout_buffer_copy(&vout->qbufs[vout->pushIdx].buf, buf,
		TO_KERNEL_SPACE) < 0) {
		pr_err("[ERR][VOUT] memory copy fail\n");
		vout->pushIdx = backupIdx;
		return -EINVAL;
	}

	if (vout->videodev2flag == VIDEODEV2_RESERVED_11 && vout->qbufs[vout->pushIdx].buf.m.planes[MPLANE_VID].reserved[1] > VOUT_MAX_CH) {
		ret = tc_v4l2_buffer_set_bootani(vout, buf);

		if (ret < 0) {
			(void)pr_err("[ERR][VOUT] tc_v4l2_buffer_set_bootani\n");
			goto out_exit;
		}
	}

	mutex_lock(&vout->lock);
	atomic_inc(&vout->readable_buff_count);

	/* buffer handling */
	buf->flags |= V4L2_BUF_FLAG_QUEUED;
	buf->flags &= ~V4L2_BUF_FLAG_DONE;

	if (vout->videodev2flag == VIDEODEV2_RESERVED_64)
		vout_m2m_display_update_bootani(vout, &vout->qbufs[vout->popIdx].buf);
	else if(vout->videodev2flag == VIDEODEV2_RESERVED_11)
		vout_m2m_display_update_bootani_reserved11(vout, &vout->qbufs[vout->popIdx].buf);
	if (++vout->popIdx >= vout->nr_qbufs) {
		vout->popIdx = 0;
	}
	atomic_dec(&vout->readable_buff_count);
	if (atomic_read(&vout->readable_buff_count) < 0)
		atomic_set(&vout->readable_buff_count, 0);

	mutex_unlock(&vout->lock);
out_exit:
	return 0;
}

int vidioc_s_fmt_vid_out_bootani(struct file *file, void *fh,
	struct v4l2_format *f)
{
	struct tcc_vout_device *vout = fh;
	struct tcc_vout_vioc *vioc = vout->vioc;
	unsigned int panel_width, panel_height;
	int ret = 0;

	if (f->fmt.pix.width == 0 || f->fmt.pix.height == 0) {
		pr_err(VOUT_NAME ": [warrning] retry %s(%dx%d)\n",
			__func__, f->fmt.pix.width, f->fmt.pix.height);
		return -EINVAL;
	}

	if (f->fmt.pix.width > 4000 || f->fmt.pix.height > 4000) {
		pr_err(VOUT_NAME
			": [error] Image resolution not supported (%dx%d)\n",
			f->fmt.pix.width, f->fmt.pix.height);
		return -EINVAL;
	}

	mutex_lock(&vout->lock);

	/* Set src image infomations */
	memcpy(&vout->src_pix, &f->fmt.pix, sizeof(struct v4l2_pix_format));

	/* init crop_src = src_pix */
	vout->crop_src.left = 0;
	vout->crop_src.top = 0;
	vout->crop_src.width = vout->src_pix.width;
	vout->crop_src.height = vout->src_pix.height;

	vout->fmt_idx = tcc_vout_try_fmt(vout->src_pix.pixelformat);
	vout->bpp = tcc_vout_try_bpp(vout->src_pix.pixelformat,
		&vout->src_pix.colorspace);
	vout->pfmt = tcc_vout_try_pix(f->fmt.pix.pixelformat);
	vout->previous_field = vout->src_pix.field;

	switch (vout->pfmt) {
	case TCC_PFMT_YUV422:
		vout->src_pix.sizeimage =
			vout->src_pix.width * vout->src_pix.height * 2;
		break;
	case TCC_PFMT_YUV420:
		vout->src_pix.sizeimage =
			vout->src_pix.width * vout->src_pix.height * 3 / 2;
		break;
	case TCC_PFMT_RGB:
	default:
		vout->src_pix.sizeimage = PAGE_ALIGN(vout->src_pix.width *
			vout->src_pix.height * vout->bpp);
		break;
	}

	/* Gst1.4 needs sizeimage */
	f->fmt.pix.sizeimage = vout->src_pix.sizeimage;

	vioc->m2m_rdma.fmt = tcc_fmtdesc[vout->fmt_idx].reserved[0];
	vioc->m2m_rdma.rgbswap = 0;	// R-G-B
	switch (vout->src_pix.pixelformat) {
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUV422P:
		vout->src_pix.width =
			ROUND_UP_4(vout->src_pix.width);
		vout->src_pix.height =
			ROUND_UP_2(vout->src_pix.height);
		break;
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_RGB32:
		vioc->m2m_rdma.rgbswap = 5; // B-G-R in the rdma
		break;
	default:
		vioc->m2m_rdma.rgbswap = 0; // R-G-B
		break;
	}

	vioc->m2m_rdma.y2r = 0;
	vioc->m2m_rdma.y2rmd = 2; // 2 = Studio Color
	vioc->viqe.y2r = 1;
	vioc->viqe.y2rmd = 2;     // 2 = Studio Color
	vioc->m2m_wdma.r2y = 1;
	vioc->m2m_wdma.r2ymd = 2; // 2 = Studio Color
	vioc->rdma.y2r = 1;
	vioc->rdma.y2rmd = 2;     // 2 = Studio Color

	if ((vout->deinterlace == VOUT_DEINTL_S)
		|| (vout->deinterlace == VOUT_DEINTL_NONE)) {
		vioc->m2m_rdma.y2r = 1;
	}

	/* de-interlace path setting */
	if (vout->id == VOUT_MAIN) {
		vout_wmix_getsize(vout, &panel_width, &panel_height);

		vout->deintl_buf_size =
			PAGE_ALIGN(panel_width * panel_height * 3 / 2);
		vioc->m2m_wdma.fmt = VIOC_IMG_FMT_YUV420IL0;
	} else {
		vout->deintl_buf_size =
			PAGE_ALIGN(vout->disp_rect.width *
				vout->disp_rect.height * 2);
		// for GRDMA support
		vioc->m2m_wdma.fmt = VIOC_IMG_FMT_YUYV;
	}

	vout->first_frame = 1;
	ret = vout_m2m_init_bootani(vout);

	if (ret) {
		pr_err("[ERR][VOUT] deinterlace setup\n");
		goto out_exit;
	}

	/* vout path setting */
	vioc->rdma.fmt = vioc->m2m_wdma.fmt;
	vout_rdma_setup_bootani(vout);
	vout_wmix_setup(vout);

out_exit:
	mutex_unlock(&vout->lock);
	return ret;
}
