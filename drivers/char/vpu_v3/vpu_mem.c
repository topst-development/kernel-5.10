/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#include "vpu_comm.h"
#include "vpu_devices.h"
#include "vpu_rm.h"
#include "vpu_mem.h"

struct tcc_dma_buf_priv {
	struct dma_buf *buf;    // dma-buf handle structure
	dma_addr_t phys;        // physical address
	size_t size;            // allocation size
	void *virt;             // virtual address
};

struct tcc_dma_buf_data {
	int dmabuf_fd;
};

//#if defined(CONFIG_TCC_MEM)
static int curr_displaying_idx[VPU_INST_MAX];
static int curr_displayed_buffer_id[VPU_INST_MAX];
static struct mutex buff_io_mutex;

int set_displaying_index(unsigned long arg)
{
	int ret = 0;
	vbuffer_manager vBuffSt;

	mutex_lock(&buff_io_mutex);

	if (copy_from_user(&vBuffSt, (int *)arg, sizeof(vbuffer_manager))) {
		ret = -EFAULT;
	} else {
		if (vBuffSt.istance_index >= VPU_INST_MAX) {
			ret = -1;
		} else {
			curr_displaying_idx[vBuffSt.istance_index] = vBuffSt.index;
		}
	}

	mutex_unlock(&buff_io_mutex);

	return ret;
}

int get_displaying_index(int nInst)
{
	int ret = 0;

	mutex_lock(&buff_io_mutex);

	if (nInst < 0 || nInst >= VPU_INST_MAX) {
		ret = -1;
	} else {
		ret = curr_displaying_idx[nInst];
	}

	mutex_unlock(&buff_io_mutex);

	return ret;
}

int set_buff_id(unsigned long arg)
{
	int ret = 0;
	vbuffer_manager vBuffSt;

	mutex_lock(&buff_io_mutex);

	if (copy_from_user(&vBuffSt, (int *)arg, sizeof(vbuffer_manager))) {
		ret = -EFAULT;
	} else {
		if (vBuffSt.istance_index >= VPU_INST_MAX) {
			ret = -1;
		} else {
			curr_displayed_buffer_id[vBuffSt.istance_index] = vBuffSt.index;
		}
	}

	mutex_unlock(&buff_io_mutex);

	return ret;
}

int get_buff_id(int nInst)
{
	int ret = 0;

	mutex_lock(&buff_io_mutex);

	if (nInst < 0 || nInst >= VPU_INST_MAX) {
		ret = -1;
	} else {
		ret = curr_displayed_buffer_id[nInst];
	}

	mutex_unlock(&buff_io_mutex);

	return ret;
}

#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME) && \
	defined(CONFIG_TCC_VIDEO_DISPLAY_BY_VSYNC_INT)
extern void tcc_vsync_set_deinterlace_mode(struct tcc_video_disp *p, int mode);

extern int tcc_video_last_frame(void *pVSyncDisp,
				struct stTcc_last_frame iLastFrame,
				struct tcc_lcdc_image_update *lastUpdated,
				unsigned int type);
#endif

#define vpu_dma_printk //printk
#define VPU_PRINT_LEVEL KERN_DEBUG

//static int tcc_mem_create_dma_buf(stVpuPhysInfo *pmap_info);
//static int tcc_mem_release_dma_buf(int ifd);

static long vmem_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	union {
		unsigned long ul_data;
		int *pi_data;
		void *pv_data;	//NULL
		const void *pcv_Data;
	} uarg;

	uarg.pv_data = NULL;
	uarg.ul_data = arg;

	switch (cmd) {
	case TCC_VIDEO_SET_DISPLAYING_IDX:
		{
			ret = set_displaying_index(arg);
		}
		break;

	case TCC_VIDEO_GET_DISPLAYING_IDX:
		{
			int nInstIdx = 0, nId = 0;

			if (copy_from_user(&nInstIdx, (int *)uarg.pi_data,
								sizeof(int)) != 0U) {
				ret = -EFAULT;
			} else {
				nId = get_displaying_index(nInstIdx);
				if (copy_to_user((int *)uarg.pi_data,
						 &nId, sizeof(int)) != 0U) {
					ret = -EFAULT;
				}
			}
		}
		break;

	case TCC_VIDEO_SET_DISPLAYED_BUFFER_ID:
		{
			ret = set_buff_id(uarg.ul_data);
		}
		break;

	case TCC_VIDEO_GET_DISPLAYED_BUFFER_ID:
		{
			int nInstIdx = 0, nId = 0;

			if (copy_from_user(&nInstIdx, (int *)uarg.pi_data,
								sizeof(int)) != 0U) {
				ret = -EFAULT;
			} else {
				nId = get_buff_id(nInstIdx);
				if (copy_to_user((int *)uarg.pi_data,
						 &nId, sizeof(int)) != 0U) {
					ret = -EFAULT;
				}
			}
		}
		break;

#if defined(CONFIG_HDMI_DISPLAY_LASTFRAME) && \
	defined(CONFIG_TCC_VIDEO_DISPLAY_BY_VSYNC_INT)
	case TCC_LCDC_VIDEO_KEEP_LASTFRAME:
		{
			struct stTcc_last_frame lastframe_info;

			if (copy_from_user((void *)&lastframe_info,
				(const void *)uarg.pcv_Data,
				sizeof(struct stTcc_last_frame)) != 0U) {
				ret = -EFAULT;
			} else {
				struct tcc_lcdc_image_update lastUpdated;

				// Only support VSYNC_MAIN
				ret = tcc_video_last_frame(NULL, lastframe_info,
						&lastUpdated, 0/*VSYNC_MAIN*/);
				if (ret > 0) {
					tcc_vsync_set_deinterlace_mode(NULL, 0);
				}
			}
		}
		break;
#endif
	case TCC_VIDEO_CREATE_DMA_BUF:
		{
			stVpuPhysInfo pmap_info;
			if (copy_from_user((void *)&pmap_info,
				(const void *)arg,
				sizeof(stVpuPhysInfo)) != 0U)	{
				ret = -EFAULT;
			} else {
				VPU_PhyMemInfo_t mem_info;
				mem_info.phys = pmap_info.phys;
				mem_info.size = pmap_info.size;
				mem_info.fd = pmap_info.fd;

				ret = tcc_mem_create_dma_buf(&mem_info);
				if (ret == 0) {
					pmap_info.phys = mem_info.phys;
					pmap_info.size = mem_info.size;
					pmap_info.fd = mem_info.fd;

					if (copy_to_user((stVpuPhysInfo *)arg,
						&pmap_info,
						sizeof(stVpuPhysInfo)) != 0U)	{
						(void)tcc_mem_release_dma_buf(pmap_info.fd);
						ret = -EFAULT;
					}
				}
			}
		}
		break;
	case TCC_VIDEO_RELEASE_DMA_BUF:
		{
			int ifd;
			if (copy_from_user((void *)&ifd,
				(void *)arg,
				sizeof(int)) != 0U) {
				ret = -EFAULT;
			} else {
				ret = tcc_mem_release_dma_buf(ifd);
			}
		}
		break;

	default:
		V_DBG(VPU_DBG_ERROR,
			"Unsupported cmd(0x%x) for vmem_ioctl_fn.", cmd);
		ret = -EFAULT;
		break;
	}

	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0)
static int tcc_dma_buf_attach(struct dma_buf *buf,
			struct dma_buf_attachment *attach)
{
	return 0;
}
#else
static int tcc_dma_buf_attach(struct dma_buf *buf,
			struct device *dev,
			struct dma_buf_attachment *attach)
{
	return 0;
}
#endif

static void tcc_dma_buf_detach(struct dma_buf *buf,
			struct dma_buf_attachment *attach)
{

}

static struct sg_table *tcc_dma_buf_map(struct dma_buf_attachment *attach,
				enum dma_data_direction dir)
{
	struct tcc_dma_buf_priv *dma_buf_priv = (struct tcc_dma_buf_priv *)attach->dmabuf->priv;
	struct sg_table *sgt = NULL;

	if (WARN_ON(((int)dir == (int)DMA_NONE) | (dma_buf_priv == NULL)) != 0) {
		V_DBG(VPU_DBG_ERROR,
			"%s %d dir(%d) dma_buf_priv=%d dmabuf=%p\n",
			__func__, __LINE__, dir, dma_buf_priv, attach->dmabuf);
		sgt = ERR_PTR(-EINVAL);
	} else {
		sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
		if (sgt != NULL) {
			if (sg_alloc_table(sgt, 1, GFP_KERNEL) != 0)	{
				V_DBG(VPU_DBG_ERROR,
					"%s %d sg_alloc_table() failed \n", __func__, __LINE__);
				kfree(sgt);
				sgt = NULL;
			} else {
				sg_dma_address(sgt->sgl) = dma_buf_priv->phys;
				if (dma_buf_priv->size <= (unsigned long)UINT_MAX) {
					sg_dma_len(sgt->sgl) = (unsigned int)dma_buf_priv->size;
				} else {
					sg_dma_len(sgt->sgl) = (unsigned int)UINT_MAX;
				}
			}
		} else {
			V_DBG(VPU_DBG_ERROR, "%s %d \n", __func__, __LINE__);
		}
	}
	return sgt;
}

static void tcc_dma_buf_unmap(struct dma_buf_attachment *attach,
						struct sg_table *sgt,
						enum dma_data_direction dir)
{
	if (sgt != NULL)	{
		sg_free_table(sgt);
		kfree(sgt);
	}
}

static void tcc_dma_buf_release(struct dma_buf *buf)
{
	struct tcc_dma_buf_priv *dma_buf_priv;

	if (buf != NULL) {
		dma_buf_priv = buf->priv;
		#if 1
		if (dma_buf_priv != NULL) {
			kfree(dma_buf_priv);
		}
		#endif
		buf->priv = NULL;
	} else {
		V_DBG(VPU_DBG_ERROR, "%s %d buf is null\n", __func__, __LINE__);
	}
}

static int tcc_dma_buf_begin_cpu_access(struct dma_buf *buf,
					enum dma_data_direction direction)
{
	return 0;
}

static int tcc_dma_buf_end_cpu_access(struct dma_buf *buf,
					enum dma_data_direction direction)
{

	return 0;
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 1, 0)
static void *tcc_dma_buf_kmap_atomic(struct dma_buf *buf,
				unsigned long page_num)
{

	return NULL;
}
#endif

static void tcc_dma_buf_vm_open(struct vm_area_struct *vma)
{
}

static void tcc_dma_buf_vm_close(struct vm_area_struct *vma)
{
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0)
static vm_fault_t tcc_dma_buf_vm_fault(struct vm_fault *vmf)
#else
static int tcc_dma_buf_vm_fault(struct vm_fault *vmf)
#endif
{
	return 0;
}

static const struct vm_operations_struct tcc_dma_buf_vm_ops = {
	.open = tcc_dma_buf_vm_open,
	.close = tcc_dma_buf_vm_close,
	.fault = tcc_dma_buf_vm_fault,
};

static int tcc_dma_buf_mmap(struct dma_buf *buf,
						struct vm_area_struct *vma)
{
	int ret = -EINVAL;
	if ((vma != NULL) && (buf != NULL)) {
		struct tcc_dma_buf_priv *dma_buf_priv = buf->priv;

		if (dma_buf_priv != NULL) {
			pgprot_t prot = vm_get_page_prot(vma->vm_flags);
			vetc_vm_flags_set(vma, (VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP));
			vma->vm_ops = &tcc_dma_buf_vm_ops;
			vma->vm_private_data = dma_buf_priv;
			vma->vm_page_prot = pgprot_writecombine(prot);

			ret = remap_pfn_range(vma, vma->vm_start,
								dma_buf_priv->phys >> PAGE_SHIFT,
								vma->vm_end - vma->vm_start,
								vma->vm_page_prot);
		} else {
			V_DBG(VPU_DBG_ERROR,
				"%s %d buf %p , vma %p.. dma_buf_priv is null\n",
				__func__, __LINE__, buf, vma);
		}
	} else {
		V_DBG(VPU_DBG_ERROR,
			"%s %d Invalid argument buf %p , vma %p\n"
			, __func__, __LINE__, buf, vma);
	}
	return ret;
}

static const struct dma_buf_ops vpu_dma_buf_ops = {
	.attach = tcc_dma_buf_attach,
	.detach = tcc_dma_buf_detach,
	.map_dma_buf = tcc_dma_buf_map,
	.unmap_dma_buf = tcc_dma_buf_unmap,
	.release = tcc_dma_buf_release,
	.mmap = tcc_dma_buf_mmap,
	.begin_cpu_access = tcc_dma_buf_begin_cpu_access,
	.end_cpu_access = tcc_dma_buf_end_cpu_access,
#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 1, 0)
	.map_atomic = tcc_dma_buf_kmap_atomic
#endif
};

int tcc_mem_create_dma_buf(VPU_PhyMemInfo_t *pmap_info)
{
	int ret = 0;
	struct tcc_dma_buf_priv *dma_buf_priv = NULL;
	struct dma_buf_export_info export_info;
	int free_obj = 0;
	void *tTmpPtr = NULL;

	dma_buf_priv = kzalloc(sizeof(struct tcc_dma_buf_priv), GFP_KERNEL);
	if (dma_buf_priv != NULL) {

		/* Telechips specific code for get the reserved memory */
		dma_buf_priv->phys = (dma_addr_t)pmap_info->phys;
		dma_buf_priv->size = (size_t)pmap_info->size;

		/* This allocates virtual address */
		dma_buf_priv->virt = vetc_ioremap(dma_buf_priv->phys, dma_buf_priv->size);
		if (dma_buf_priv->virt == NULL) {
			V_DBG(VPU_DBG_ERROR,
				"%s %d phys:0x%lx size:0x%lx.. ioremap failed \n",
				__func__, __LINE__, pmap_info->phys, pmap_info->size);
			ret = -ENOMEM;
			free_obj = 1;
		} else {
			export_info.exp_name = "vpu_dma_exp";
			export_info.owner = THIS_MODULE;
			export_info.ops = &vpu_dma_buf_ops;
			export_info.size = dma_buf_priv->size;
			export_info.flags = (int)((unsigned int)O_CLOEXEC | (unsigned int)O_RDWR);
			export_info.resv = NULL;
			export_info.priv = dma_buf_priv;

			dma_buf_priv->buf = dma_buf_export(&export_info);

			if (dma_buf_priv->buf != NULL) {
				VPU_CAST_PT(tTmpPtr, dma_buf_priv->buf);
				if (IS_ERR(tTmpPtr)) {
					long lret;

					V_DBG(VPU_DBG_ERROR,
						"%s %d phys:0x%lx size:0x%lx.. IS_ERR(buf)\n",
						__func__, __LINE__, pmap_info->phys, pmap_info->size);
					lret = PTR_ERR(dma_buf_priv->buf);
					if (lret > (long)INT_MAX) {
						ret = (int)INT_MAX;
					} else if (lret < (long)INT_MIN) {
						ret = (int)INT_MIN;
					} else {
						ret = (int)lret;
					}
					free_obj = 1;
				} else {
					/* get the dma-buf fd for pass to target device through userspace */
					pmap_info->fd = dma_buf_fd(dma_buf_priv->buf, O_CLOEXEC);
					if (pmap_info->fd < 0) {
						V_DBG(VPU_DBG_ERROR,
							"%s %d phys:0x%lx size:0x%lx.. dma_buf_fd(buf)\n",
							__func__, __LINE__, pmap_info->phys, pmap_info->size);
						dma_buf_put(dma_buf_priv->buf);
						free_obj = 1;
					} else {
						ret = 0;
					}
				}
			} else {
				V_DBG(VPU_DBG_ERROR,
					"%s %d phys:0x%lx size:0x%lx.. dma_buf_export() failed\n",
					__func__, __LINE__, pmap_info->phys, pmap_info->size);
				free_obj = 1;
			}
		}
	} else {
		V_DBG(VPU_DBG_ERROR,
			"%s %d phys:0x%lx size:0x%lx.. kzalloc() failed\n",
			__func__, __LINE__, pmap_info->phys, pmap_info->size);
	}

	if (free_obj == 1) {
		dma_buf_priv->virt = NULL;
		kfree(dma_buf_priv);
		dma_buf_priv = NULL;
	}

	return ret;
}
EXPORT_SYMBOL(tcc_mem_create_dma_buf);

int tcc_mem_release_dma_buf(int ifd)
{
	int ret = 0;
	struct dma_buf *dmabuf = NULL;
	struct tcc_dma_buf_priv *dma_buf_priv = NULL;

	dmabuf = dma_buf_get(ifd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		V_DBG(VPU_DBG_ERROR,
			"%s %d fd %d, IS_ERR_OR_NULL(dmabuf) \n",
			__func__, __LINE__, ifd);
		ret = -EINVAL;
	} else {
		dma_buf_priv = (struct tcc_dma_buf_priv *)dmabuf->priv;
		if (dma_buf_priv != NULL) {
			/* Unmap virtual address */
			if (dma_buf_priv->virt != NULL)	{
				iounmap(dma_buf_priv->virt);
			}
			dma_buf_put(dma_buf_priv->buf);
		} else {
			V_DBG(VPU_DBG_ERROR,
				"%s %d fd %d, dma_buf_priv is null\n",
				__func__, __LINE__, ifd);
		}
	}

	return ret;
}
EXPORT_SYMBOL(tcc_mem_release_dma_buf);

#ifdef CONFIG_COMPAT
static long vmem_compat_ioctl(struct file *filep, unsigned int cmd,
				  unsigned long arg)
{
	return vmem_ioctl(filep, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static unsigned int vmem_poll(struct file *filp, poll_table *wait)
{
	unsigned int ret = 0U;
/*
	vpu_dec_drv_t *vdata = (vpu_dec_drv_t *)filp->private_data;

	if (vpu_data == NULL) {
		ret = (unsigned int)POLLERR | (unsigned int)POLLNVAL;
	} else {
		if (vdata->drv_poll_data.count == 0U) {
			poll_wait(filp, &(vpu_data->wq), wait);
		}

		if (vdata->drv_poll_data.count > 0U) {
			atomic_dec(&vdata->drv_poll_data.count);
			ret = (unsigned int)POLLIN;
		}
	}
*/
	return ret;
}

static int vmem_open(struct inode *pinode, struct file *filp)
{
	return 0;
}

static int vmem_release(struct inode *pinode, struct file *filp)
{
	V_DBG(VPU_DBG_CLOSE, "Out!!");
	return 0;
}

static int vmem_mmap(struct file *filep, struct vm_area_struct *vma)
{
	int ret = 0;

#if defined(CONFIG_TCC_MEM)
	if (range_is_allowed(vma->vm_pgoff, (vma->vm_end >= vma->vm_start) ? (vma->vm_end - vma->vm_start) : 0U) < 0) {
		V_DBG(VPU_DBG_ERROR,
			"mem_mmap: this address is not allowed");
		ret = -EAGAIN;
	}
#endif
	if (ret == 0) {
		vma->vm_page_prot = vmem_get_pgprot(vma->vm_page_prot, vma->vm_pgoff);
		if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, (vma->vm_end - vma->vm_start), vma->vm_page_prot) != 0) {
			V_DBG(VPU_DBG_ERROR, "mem_mmap :: remap_pfn_range failed");
			ret = -EAGAIN;
		} else {
			vma->vm_ops = NULL;
			vetc_vm_flags_set(vma, (VM_IO | VM_DONTEXPAND | VM_PFNMAP));
		}
	}

	return ret;
}

static const struct file_operations vdev_mem_fops = {
	.owner = THIS_MODULE,
	.open = vmem_open,
	.release = vmem_release,
	.mmap = vmem_mmap,
	.unlocked_ioctl = vmem_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vmem_compat_ioctl,
#endif
	.poll = vmem_poll,
};

static struct miscdevice vmem_misc_device = {
	MISC_DYNAMIC_MINOR,
	MEM_NAME,
	&vdev_mem_fops,
};

int vmem_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (misc_register(&vmem_misc_device) != 0) {
		(void)pr_info("VPU mem: Couldn't register device.\n");
		ret = -EBUSY;
	} else {
		mutex_init(&buff_io_mutex);
	}

	return ret;
}

VREMOVE_RET_TYPE vmem_remove(struct platform_device *pdev)
{
	misc_deregister(&vmem_misc_device);

	VREMOVE_RETURN();
}
