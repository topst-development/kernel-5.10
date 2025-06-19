// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-buf.h>
//#include <linux/dma-mapping.h>

#include <uapi/misc/tccmisc_drv.h>

#ifdef CONFIG_ANDROID
#include <linux/rmem_heap.h>	// tcc_mem driver
#define USE_TCCMISC_GET_PHYS
#endif

int dbg;
#define kdbg(fmt, args...) \
	do { \
		if (dbg) \
			pr_err("\e[33m[%s:%d] \e[0m" fmt, \
				__func__, __LINE__, ## args); \
	} while (0)

struct device_attribute dev_attr_pmap;
struct device_attribute dev_attr_debug;

/* Driver private data */
struct tccmisc_data_t {
	struct miscdevice *misc;
	struct mutex io_mutex;

	struct tccmisc_user_t info;
	struct tccmisc_phys_t phys;
};

/*
 * for ioctl testing
 */
//#define TCCMISC_IOCTL_TEST
#ifdef TCCMISC_IOCTL_TEST
int tccmisc_drv_test(void)
{
	struct file *fp;
	struct tccmisc_user_t info;
	int ret;

	fp = filp_open("/dev/tccmisc", O_RDWR, 0666);
	if (IS_ERR(fp)) {
		pr_err("%s: tccmisc file open error\n", __func__);
		ret = -ENODEV;
		goto exit;
	}

	strcpy(info.name, "fb_video");
	info.base = 0;
	info.size = 0;

	ret = fp->f_op->unlocked_ioctl(fp, IOCTL_TCCMISC_PMAP_KERNEL,
		(unsigned long)&info);
	if (ret < 0) {
		pr_err("%s: tccmisc IOCTL_TCCMISC_PMAP_KERNEL error\n",
			__func__);
		goto exit_fp;
	}

	pr_info("%s: %s,0x%llx,0x%llx\n", __func__,
		info.name, info.base, info.size);

exit_fp:
	filp_close(fp, NULL);
exit:
	pr_info("%s: ret(%d)\n", __func__, ret);
	return ret;
}
#endif

/*
 * compatible = "telechips,pmap";
 * telechips,pmap-name = "overlay";
 */
int tccmisc_pmap(struct tccmisc_user_t *pmap)
{
	struct device_node *np, *rmem_node;
	struct reserved_mem *rmem;
	int ret = -1, len;

	np = of_find_node_by_name(NULL, "reserved-memory");
	if (np == NULL) {
		kdbg("can't find reserved_memory node\n");
		ret = -2;
		goto exit;
	}

	for_each_child_of_node(np, rmem_node) {
		const char *name = of_get_property(rmem_node, "telechips,pmap-name", &len);

		if (name == NULL) {
			kdbg("next:1\n");
			continue;
		}

		//if (of_property_read_string(rmem_node, "telechips,pmap-name", &name)) {
		//	kdbg("next:2\n");
		//	continue;
		//}

		if (strncmp(name, pmap->name, 32)) {
			kdbg("next:3\n");
			continue;
		} else {
			kdbg("find:%s\n", pmap->name);
			ret = 0;
			break;
		}
	}

	if (ret == 0) {
		rmem = of_reserved_mem_lookup(rmem_node);
		if (rmem == NULL) {
			kdbg("pmap %s allocation is failed\n", pmap->name);
			ret = -3;
			goto exit;
		}

		pmap->base = rmem->base;
		pmap->size = rmem->size;

		kdbg("name(%s) base(0x%llx) size(0x%llx)\n",
			pmap->name, pmap->base, pmap->size);
	} else {
		pr_info("%s: can't find pmap.%s\n", __func__, pmap->name);
	}

exit:
	kdbg("ret(%d)\n", ret);
	return ret;
}
EXPORT_SYMBOL(tccmisc_pmap);

/*
 * compatible = "pmap,overlay";
 * pmap-name = "overlay";
 */
int tccmisc_pmap2(struct tccmisc_user_t *pmap)
{
	struct device_node *np;
	struct reserved_mem *rmem;
	//const char *name;
	char buf[32];
	int ret = 0;

	sprintf(buf, "pmap,%s", pmap->name);
	kdbg("%s = %s\n", pmap->name, buf);

	np = of_find_compatible_node(NULL, NULL, buf);
	if (np == NULL) {
		kdbg("can't find %s\n", buf);
		ret = -1;
		goto exit;
	}

	rmem = of_reserved_mem_lookup(np);
	if (rmem == NULL) {
		kdbg("pmap,%s allocation is failed\n", pmap->name);
		ret = -2;
		goto exit;
	}

	//if (of_property_read_string(np, "pmap-name", &name)) {
	//	kdbg("can't get pmap-name\n");
	//} else {
	//	strcpy(pmap->name, name);
	//}

	pmap->base = rmem->base;
	pmap->size = rmem->size;

	kdbg("name(%s) base(0x%llx) size(0x%llx)\n",
		pmap->name, pmap->base, pmap->size);
exit:
	kdbg("ret(%d)\n", ret);
	return ret;
}
EXPORT_SYMBOL(tccmisc_pmap2);


#ifdef USE_TCCMISC_GET_PHYS

int tccmisc_phys(struct device *dev, struct tccmisc_phys_t *phys)
{
	struct dma_buf *dmabuf;
	struct rmem_heap_buffer *buffer;
	int ret = -1;

	pr_debug("%s: dmabuf_fd %d\n", __func__, phys->dmabuf_fd);
	dmabuf = dma_buf_get(phys->dmabuf_fd);
	if (dmabuf == NULL) {
		pr_err("%s failed to get dma_buf from fd(%d).\n",
			__func__, phys->dmabuf_fd);
		ret = -1;
		goto exit;
	}

	buffer = (struct rmem_heap_buffer *)dmabuf->priv;

	phys->addr = buffer->phys;
	phys->len = buffer->size;

	dma_buf_put(dmabuf);

	ret = 0;

exit:
    return ret;
}
EXPORT_SYMBOL(tccmisc_phys);

int tccmisc_phys_sg_dma_addr(struct device *dev, struct tccmisc_phys_t *phys)
{
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	int ret = -1;

	kdbg("%s: dmabuf_fd %d\n", __func__, phys->dmabuf_fd);
	dmabuf = dma_buf_get(phys->dmabuf_fd);
	if (dmabuf == NULL) {
		pr_err("%s failed to get dma_buf from fd(%d).\n",
			__func__, phys->dmabuf_fd);
		ret = -1;
		goto exit;
	}

	attach = dma_buf_attach(dmabuf, dev);
	if (attach == NULL) {
		pr_err("%s failed to get dma_buf_attach.\n", __func__);
		ret = -1;
		goto exit;
	}

	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (sgt == NULL) {
		pr_err("%s failed to get dma_buf_map_attachment.\n",
			__func__);
		ret = -1;
		goto exit;
	}

	phys->addr = sg_dma_address(sgt->sgl);
	phys->len = dmabuf->size;

	dma_buf_put(dmabuf);

	ret = 0;

exit:
    return ret;
}
EXPORT_SYMBOL(tccmisc_phys_sg_dma_addr);

int tccmisc_phys_videobuf2(struct device *dev, struct tccmisc_phys_t *phys)
{
	struct dma_buf *dmabuf;
	unsigned int offset = 0;
	int ret = -1;


	kdbg("%s: dmabuf_fd %d\n", __func__, phys->dmabuf_fd);
	dmabuf = dma_buf_get(phys->dmabuf_fd);
	if (dmabuf == NULL) {
		pr_err("%s failed to get dma_buf from fd(%d).\n",
			__func__, phys->dmabuf_fd);
		ret = -1;
		goto exit;
	}

	offset = sizeof(struct device *) + sizeof(void *) +
		sizeof(unsigned long) + sizeof(void *);
	/************************************************************
	 * The priv data of dmabuf is as follows.                   *
	 * This struct is defined in                                *
	 * drivers/media/common/videobuf2/videobuf2-dma-contig.c.   *
	 * This struct may change depending on the kernel version.  *
	 *                                                          *
	 * struct vb2_dc_buf {                                      *
	 * 	struct device  *dev;                                *
	 * 	void           *vaddr;                              *
	 * 	unsigned long  size;                                *
	 * 	void           *cookie;                             *
	 * 	dma_addr_t     dma_addr;                            *
	 * 	...                                                 *
	 * };                                                       *
	 ************************************************************/

	phys->addr = *(int *)((void *)(dmabuf->priv) + offset);
	phys->len = dmabuf->size;

	dma_buf_put(dmabuf);

	ret = 0;

exit:
    return ret;
}
EXPORT_SYMBOL(tccmisc_phys_videobuf2);

#if 0
#if 0
#define IOCTL_TCCMISC_PHYS_SG_DMA_ADDR _IOWR('T', 7, struct tccmisc_phys_t)
#define IOCTL_TCCMISC_PHYS_SG_DMA_ADDR_KERNEL _IOWR('T', 8, struct tccmisc_phys_t)
#define IOCTL_TCCMISC_DMA_ALLOC _IOWR('T', 9, struct tccmisc_dma_alloc_t)
#define IOCTL_TCCMISC_DMA_ALLOC_KERNEL _IOWR('T', 10, struct tccmisc_dma_alloc_t)
#define IOCTL_TCCMISC_DMA_FREE _IOWR('T', 11, struct tccmisc_dma_alloc_t)
#define IOCTL_TCCMISC_DMA_FREE_KERNEL _IOWR('T', 12, struct tccmisc_dma_alloc_t)

struct tccmisc_dma_alloc_t {
	int dmabuf_fd;
	__u64 size;
};

struct tccmisc_dma_free_t {
	int dmabuf_fd;
};

struct dma_buf_exporter_priv {
    struct dma_buf *buf;    /* dma-buf handle structure */
    __u64 phys;        /* physical address */
    __u64 size;            /* allocation size */
    void *virt;             /* virtual address */
};
#endif

static int tccmisc_dma_exporter_attach(struct dma_buf *buf, struct dma_buf_attachment *attach)
{
	/* reserved */
	return 0;
}

static void tccmisc_dma_exporter_detach(struct dma_buf *buf,
			  struct dma_buf_attachment *attach)
{
	/* reserved */
}

static struct sg_table *tccmisc_dma_exporter_map_dma_buf(struct dma_buf_attachment *attach,
					   enum dma_data_direction dir)
{
	struct dma_buf_exporter_priv *data = attach->dmabuf->priv;
	struct sg_table *sgt;

	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return NULL;

	if (sg_alloc_table(sgt, 1, GFP_KERNEL)) {
		kfree(sgt);
		return NULL;
	}

	sg_dma_address(sgt->sgl) = data->phys;
	sg_dma_len(sgt->sgl) = data->size;

	return sgt;
}

static void tccmisc_dma_exporter_unmap_dma_buf(struct dma_buf_attachment *attach,
				 struct sg_table *sgt,
				 enum dma_data_direction dir)
{
	sg_free_table(sgt);
	kfree(sgt);
}

static void tccmisc_dma_exporter_release(struct dma_buf *buf)
{
	struct dma_buf_exporter_priv *data;
	data = buf->priv;

	kfree(data);
}

static int tccmisc_dma_exporter_begin_cpu_access(struct dma_buf *buf,
					    enum dma_data_direction direction)
{
	/* reserved */
	return 0;
}

static int tccmisc_dma_exporter_end_cpu_access(struct dma_buf *buf,
					  enum dma_data_direction direction)
{
	/* reserved */
	return 0;
}

static void *tccmisc_dma_exporter_kmap(struct dma_buf *buf, unsigned long page)
{
	/* reserved */
	return NULL;
}

static void tccmisc_dma_exporter_kunmap(struct dma_buf *buf, unsigned long page, void *vaddr)
{
	/* reserved */
}

static void tccmisc_dma_exporter_vm_open(struct vm_area_struct *vma)
{
	/* reserved */
}

static void tccmisc_dma_exporter_vm_close(struct vm_area_struct *vma)
{
	/* reserved */
}

static unsigned int tccmisc_dma_exporter_vm_fault(struct vm_fault *vmf)
{
	/* reserved */
	return 0;
}

static const struct vm_operations_struct tccmisc_dma_exporter_vm_ops = {
	.open = tccmisc_dma_exporter_vm_open,
	.close = tccmisc_dma_exporter_vm_close,
	.fault = tccmisc_dma_exporter_vm_fault,
};

static int tccmisc_dma_exporter_mmap(struct dma_buf *buf, struct vm_area_struct *vma)
{
	pgprot_t prot = vm_get_page_prot(vma->vm_flags);
	struct dma_buf_exporter_priv *data = buf->priv;

	vma->vm_flags |= VM_IO | VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP;
	vma->vm_ops = &tccmisc_dma_exporter_vm_ops;
	vma->vm_private_data = data;
	vma->vm_page_prot = pgprot_writecombine(prot);

	return remap_pfn_range(vma, vma->vm_start, data->phys >> PAGE_SHIFT,
			       vma->vm_end - vma->vm_start, vma->vm_page_prot);
}

static const struct dma_buf_ops tccmisc_dma_exporter_ops = {
	.attach = tccmisc_dma_exporter_attach,
	.detach = tccmisc_dma_exporter_detach,
	.map_dma_buf = tccmisc_dma_exporter_map_dma_buf,
	.unmap_dma_buf = tccmisc_dma_exporter_unmap_dma_buf,
	.release = tccmisc_dma_exporter_release,
	.mmap = tccmisc_dma_exporter_mmap,
	.begin_cpu_access = tccmisc_dma_exporter_begin_cpu_access,
	.end_cpu_access = tccmisc_dma_exporter_end_cpu_access,
	.map = tccmisc_dma_exporter_kmap,
	.unmap = tccmisc_dma_exporter_kunmap,
	//.map_atomic = tccmisc_dma_exporter_kmap_atomic
};

static struct dma_buf_export_info export_info_fuc(struct dma_buf_exporter_priv *data)
{
	struct dma_buf_export_info export_info = {
        .exp_name = "tccmisc_dma_exporter",
        .owner = THIS_MODULE,
        .ops = &tccmisc_dma_exporter_ops,
        .size = data->size,
        .flags = O_CLOEXEC | O_RDWR,
        .priv = data,
    };
	return export_info;
}

int tccmisc_dma_free(struct device *dev, struct tccmisc_dma_free_t *dma_free)
{
	struct dma_buf *dmabuf;
	struct dma_buf_exporter_priv *buffer;
	int ret = -1;

	kdbg("%s: dmabuf_fd %d\n", __func__, dma_free->dmabuf_fd);
	dmabuf = dma_buf_get(dma_free->dmabuf_fd);
	if (dmabuf == NULL) {
		pr_err("%s failed to get dma_buf from fd(%d).\n",
			__func__, dma_free->dmabuf_fd);
		ret = -1;
		goto exit;
	}

	buffer = (struct dma_buf_exporter_priv *)dmabuf->priv;
	kdbg("%s: size:%llx, virt:%p, phys:%llx\n",__func__,buffer->size, buffer->virt, buffer->phys);

	dma_free_coherent(dev, buffer->size, buffer->virt, buffer->phys);
	ret = 0;
exit:
	return ret;

}
EXPORT_SYMBOL(tccmisc_dma_free);

int tccmisc_dma_alloc(struct device *dev, struct tccmisc_dma_alloc_t *dma_alloc)
{
	struct dma_buf_exporter_priv *data = NULL;
	struct dma_buf_export_info export_info;
	dma_addr_t phy_addr;
	int ret = 0;

    data = kzalloc(sizeof(struct dma_buf_exporter_priv), GFP_KERNEL);
    if (data == NULL) {
        pr_err("%s: couldn't alloc exporter structure", __func__);
        ret = -ENOMEM;
        goto free;
    }

	data->virt = dma_alloc_coherent(dev, dma_alloc->size, &phy_addr, GFP_KERNEL);
	if (data->virt == NULL) {
		pr_err("[ERR][TCCMISC][%s]can't alloc memeory\n", __func__);
		ret = -1;
		goto free_object;
	}

	data->phys = phy_addr;
	data->size = dma_alloc->size;

	export_info = export_info_fuc(data);

    data->buf = dma_buf_export(&export_info);
    if (!data->buf) {
        pr_err("%s: couldn't export dma_buf", __func__);
		ret = -1;
        goto free_object;
    }

    if (IS_ERR(data->buf)) {
        ret = PTR_ERR(data->buf);
        goto free_object;
    }

    /* get the dma-buf fd for pass to target device through userspace */
    dma_alloc->dmabuf_fd = dma_buf_fd(data->buf, O_CLOEXEC);
    if (dma_alloc->dmabuf_fd < 0) {
        pr_err("%s: couldn't get fd from dma_buf", __func__);
		ret = -1;
        goto no_fd;
    }

	kdbg("%s: dmabuf_fd:%d size:0x%llx, virt:0x%p, phys:0x%llx\n",
		__func__, dma_alloc->dmabuf_fd, data->size, data->virt, data->phys);

    return 0;
no_fd:
    dma_buf_put(data->buf);
free_object:
    kfree(data);
free:
    return ret;
}
EXPORT_SYMBOL(tccmisc_dma_alloc);
#endif

#endif	//USE_TCCMISC_GET_PHYS

static long tccmisc_drv_ioctl(
	struct file *filp,
	unsigned int cmd,
	unsigned long arg)
{
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct tccmisc_data_t *pdata = dev_get_drvdata(misc->parent);
	int ret = 0;

	kdbg("ioctl(%d)\n", cmd);

	mutex_lock(&(pdata->io_mutex));

	switch (cmd) {
	case IOCTL_TCCMISC_PMAP:
	case IOCTL_TCCMISC_PMAP_KERNEL:
		if (cmd == IOCTL_TCCMISC_PMAP_KERNEL) {
			memcpy(&pdata->info,
				(struct tccmisc_user_t *)arg,
				sizeof(struct tccmisc_user_t));
		} else {
			if (copy_from_user(&pdata->info,
				(struct tccmisc_user_t *)arg,
				sizeof(struct tccmisc_user_t))) {
				pr_err("%s: error IOCTL_TCCMISC_PMAP copy_from_user\n",
					__func__);
				ret = -EFAULT;
				goto exit;
			}
		}

		ret = tccmisc_pmap(&pdata->info);

		if (ret == 0) {
			if (cmd == IOCTL_TCCMISC_PMAP_KERNEL) {
				memcpy((struct tccmisc_user_t *)arg,
					&pdata->info,
					sizeof(struct tccmisc_user_t));
			} else {
				if (copy_to_user((struct tccmisc_user_t *)arg,
					&pdata->info,
					sizeof(struct tccmisc_user_t))) {
					pr_err("%s: error IOCTL_TCCMISC_PMAP copy_to_user\n",
						__func__);
					ret = -EFAULT;
					goto exit;
				}
			}

			kdbg("pmap %s (base:0x%llx, size:0x%llx)\n",
				pdata->info.name,
				pdata->info.base,
				pdata->info.size);
		} else {
			pr_err("%s: pmap %s doen't exist.\n",
				__func__,
				pdata->info.name);
		}
		break;

#ifdef USE_TCCMISC_GET_PHYS
	case IOCTL_TCCMISC_PHYS:
	case IOCTL_TCCMISC_PHYS_KERNEL:
		if (cmd == IOCTL_TCCMISC_PHYS_KERNEL) {
			memcpy(&pdata->phys,
				(struct tccmisc_phys_t *)arg,
				sizeof(struct tccmisc_phys_t));
		} else {
			if (copy_from_user(&pdata->phys,
				(struct tccmisc_phys_t *)arg,
				sizeof(struct tccmisc_phys_t))) {
				pr_err("%s: error IOCTL_TCCMISC_PHYS copy_from_user\n",
					__func__);
				ret = -EFAULT;
				goto exit;
			}
		}

		ret = tccmisc_phys(pdata->misc->parent, &pdata->phys);

		if (ret == 0) {
			if (cmd == IOCTL_TCCMISC_PHYS_KERNEL) {
				memcpy((struct tccmisc_phys_t *)arg,
					&pdata->phys,
					sizeof(struct tccmisc_phys_t));
			} else {
				if (copy_to_user((struct tccmisc_phys_t *)arg,
					&pdata->phys,
					sizeof(struct tccmisc_phys_t))) {
					pr_err("%s: error IOCTL_TCCMISC_PHYS copy_to_user\n",
						__func__);
					ret = -EFAULT;
					goto exit;
				}
			}
			kdbg("%s: dmabuf_fd:%d dma_addr:0x%llx size:0x%llx\n",
				__func__,
				pdata->phys.dmabuf_fd,
				(__u64)pdata->phys.addr,
				pdata->phys.len);

		} else {
			pr_err("%s: faild to get physical address from fd(%d)\n",
				__func__, pdata->phys.dmabuf_fd);
		}
		break;
	case IOCTL_TCCMISC_PHYS_VIDEOBUF2:
	case IOCTL_TCCMISC_PHYS_VIDEOBUF2_KERNEL:
		if (cmd == IOCTL_TCCMISC_PHYS_VIDEOBUF2_KERNEL) {
			memcpy(&pdata->phys,
				(struct tccmisc_phys_t *)arg,
				sizeof(struct tccmisc_phys_t));
		} else {
			if (copy_from_user(&pdata->phys,
				(struct tccmisc_phys_t *)arg,
				sizeof(struct tccmisc_phys_t))) {
				pr_err("%s: error IOCTL_TCCMISC_PHYS_VIDEOBUF2 copy_from_user\n",
					__func__);
				ret = -EFAULT;
				goto exit;
			}
		}

		ret = tccmisc_phys_videobuf2(pdata->misc->parent, &pdata->phys);

		if (ret == 0) {
			if (cmd == IOCTL_TCCMISC_PHYS_VIDEOBUF2_KERNEL) {
				memcpy((struct tccmisc_phys_t *)arg,
					&pdata->phys,
					sizeof(struct tccmisc_phys_t));
			} else {
				if (copy_to_user((struct tccmisc_phys_t *)arg,
					&pdata->phys,
					sizeof(struct tccmisc_phys_t))) {
					pr_err("%s: error IOCTL_TCCMISC_PHYS_VIDEOBUF2 copy_to_user\n",
						__func__);
					ret = -EFAULT;
					goto exit;
				}
			}
			kdbg("%s: dmabuf_fd:%d dma_addr:0x%llx size:0x%llx\n",
				__func__,
				pdata->phys.dmabuf_fd,
				(__u64)pdata->phys.addr,
				pdata->phys.len);

		} else {
			pr_err("%s: faild to get physical address from fd(%d)\n",
				__func__, pdata->phys.dmabuf_fd);
		}
		break;
#if 0
	case IOCTL_TCCMISC_PHYS_SG_DMA_ADDR:
	case IOCTL_TCCMISC_PHYS_SG_DMA_ADDR_KERNEL:
		if (cmd == IOCTL_TCCMISC_PHYS_SG_DMA_ADDR_KERNEL) {
			memcpy(&pdata->phys,
				(struct tccmisc_phys_t *)arg,
				sizeof(struct tccmisc_phys_t));
		} else {
			if (copy_from_user(&pdata->phys,
				(struct tccmisc_phys_t *)arg,
				sizeof(struct tccmisc_phys_t))) {
				pr_err("%s: error IOCTL_TCCMISC_PHYS_SG_DMA_ADDR copy_from_user\n",
					__func__);
				ret = -EFAULT;
				goto exit;
			}
		}

		ret = tccmisc_phys_sg_dma_addr(pdata->misc->parent, &pdata->phys);

		if (ret == 0) {
			if (cmd == IOCTL_TCCMISC_PHYS_SG_DMA_ADDR_KERNEL) {
				memcpy((struct tccmisc_phys_t *)arg,
					&pdata->phys,
					sizeof(struct tccmisc_phys_t));
			} else {
				if (copy_to_user((struct tccmisc_phys_t *)arg,
					&pdata->phys,
					sizeof(struct tccmisc_phys_t))) {
					pr_err("%s: error IOCTL_TCCMISC_PHYS_SG_DMA_ADDR copy_to_user\n",
						__func__);
					ret = -EFAULT;
					goto exit;
				}
			}
			kdbg("%s: dmabuf_fd:%d dma_addr:0x%llx size:%llx\n",
				__func__,
				pdata->phys.dmabuf_fd,
				(__u64)pdata->phys.addr,
				pdata->phys.len);

		} else {
			pr_err("%s: faild to get physical address from fd(%d)\n",
				__func__, pdata->phys.dmabuf_fd);
		}
		break;
	case IOCTL_TCCMISC_DMA_ALLOC:
	case IOCTL_TCCMISC_DMA_ALLOC_KERNEL:
		if (cmd == IOCTL_TCCMISC_DMA_ALLOC_KERNEL) {
			memcpy(&pdata->dma_alloc,
				(struct tccmisc_dma_alloc_t *)arg,
				sizeof(struct tccmisc_dma_alloc_t));
		} else {
			if (copy_from_user(&pdata->dma_alloc,
				(struct tccmisc_dma_alloc_t *)arg,
				sizeof(struct tccmisc_dma_alloc_t))) {
				pr_err("%s: error IOCTL_TCCMISC_DMA_ALLOC copy_from_user\n",
					__func__);
				ret = -EFAULT;
				goto exit;
			}
		}

		ret = tccmisc_dma_alloc(pdata->misc->parent, &pdata->dma_alloc);

		if (ret == 0) {
			if (cmd == IOCTL_TCCMISC_DMA_ALLOC) {
				memcpy((struct tccmisc_dma_alloc_t *)arg,
					&pdata->dma_alloc,
					sizeof(struct tccmisc_dma_alloc_t));
			} else {
				if (copy_to_user((struct tccmisc_dma_alloc_t *)arg,
					&pdata->dma_alloc,
					sizeof(struct tccmisc_dma_alloc_t))) {
					pr_err("%s: error IOCTL_TCCMISC_DMA_ALLOC copy_to_user\n",
						__func__);
					ret = -EFAULT;
					goto exit;
				}
			}
		} else {
			pr_err("%s: faild to alloc dma buf\n", __func__);
		}
		break;
	case IOCTL_TCCMISC_DMA_FREE:
	case IOCTL_TCCMISC_DMA_FREE_KERNEL:
		if (cmd == IOCTL_TCCMISC_DMA_FREE_KERNEL) {
			memcpy(&pdata->dma_free,
				(struct tccmisc_dma_free_t *)arg,
				sizeof(struct tccmisc_dma_free_t));
		} else {
			if (copy_from_user(&pdata->dma_free,
				(struct tccmisc_dma_free_t *)arg,
				sizeof(struct tccmisc_dma_free_t))) {
				pr_err("%s: error IOCTL_TCCMISC_DMA_FREE copy_from_user\n",
					__func__);
				ret = -EFAULT;
				goto exit;
			}
		}

		ret = tccmisc_dma_free(pdata->misc->parent, &pdata->dma_free);

		if (ret == 0) {
			if (cmd == IOCTL_TCCMISC_DMA_FREE_KERNEL) {
				memcpy((struct tccmisc_dma_free_t *)arg,
					&pdata->dma_free,
					sizeof(struct tccmisc_dma_free_t));
			} else {
				if (copy_to_user((struct tccmisc_dma_free_t *)arg,
					&pdata->dma_free,
					sizeof(struct tccmisc_dma_free_t))) {
					pr_err("%s: error IOCTL_TCCMISC_DMA_FREE copy_to_user\n",
						__func__);
					ret = -EFAULT;
					goto exit;
				}
			}
		} else {
			pr_err("%s: faild to free dma buf\n", __func__);
		}
		break;
#endif
#endif	//USE_TCCMISC_GET_PHYS

	default:
		pr_err("%s: error ioctl cmd(%d)\n", __func__, cmd);
	}

exit:
	mutex_unlock(&(pdata->io_mutex));

	return ret;
}

static ssize_t pmap_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct tccmisc_data_t *pdata;
	int ret;

	pdata = (struct tccmisc_data_t *)dev->platform_data;

	ret = tccmisc_pmap(&pdata->info);
	if (ret < 0) {
		kdbg("pmap %s doen't exist.\n", pdata->info.name);
		return sprintf(buf, "pmap,%s doen't exist.\n",
			pdata->info.name);
	} else {
		return sprintf(buf, "%s,0x%llx,0x%llx\n",
			pdata->info.name,
			pdata->info.base,
			pdata->info.size);
	}
}

static ssize_t pmap_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct tccmisc_data_t *pdata;
	int n;

	pdata = (struct tccmisc_data_t *)dev->platform_data;

	//strcpy(pdata->info.name, buf);
	n = sprintf(pdata->info.name, "%s", buf);
	if (pdata->info.name[n - 1] == '\n')
		pdata->info.name[n - 1] = '\0';

	kdbg("%s, %s\n", buf, pdata->info.name);

	return count;
}
DEVICE_ATTR_RW(pmap);

static ssize_t debug_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct tccmisc_data_t *pdata;

#ifdef TCCMISC_IOCTL_TEST
	tccmisc_drv_test();
#endif

	pdata = (struct tccmisc_data_t *)dev->platform_data;
	return sprintf(buf, "%d\n", dbg);
}

static ssize_t debug_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct tccmisc_data_t *pdata;

	pdata = (struct tccmisc_data_t *)dev->platform_data;
	if (kstrtoul(buf, 10, (unsigned long *)&dbg))
		pr_err("%s\n", __func__);
	return count;
}
DEVICE_ATTR_RW(debug);

static int tccmisc_drv_release(struct inode *inode, struct file *filp)
{
	//struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	//struct tccmisc_data_t *pdata = dev_get_drvdata(misc->parent);
	int ret = 0;

	kdbg("\n");
	return ret;
}

static int tccmisc_drv_open(struct inode *inode, struct file *filp)
{
	//struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	//struct tccmisc_data_t *pdata = dev_get_drvdata(misc->parent);
	int ret = 0;

	kdbg("\n");
	return ret;
}

static const struct file_operations tccmisc_drv_fops = {
	.owner = THIS_MODULE,
	.open = tccmisc_drv_open,
	.release = tccmisc_drv_release,
	.unlocked_ioctl = tccmisc_drv_ioctl,
};

static int tccmisc_drv_probe(struct platform_device *pdev)
{
	struct tccmisc_data_t *pdata;
	struct device_node *np;
	int ret = -ENOMEM;

	pdata = kzalloc(sizeof(struct tccmisc_data_t), GFP_KERNEL);
	if (!pdata)
		goto exit;

	pdata->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (!pdata->misc)
		goto err;

	pdata->misc->minor = MISC_DYNAMIC_MINOR;
	pdata->misc->fops = &tccmisc_drv_fops;
	pdata->misc->name = pdev->name;
	pdata->misc->parent = &pdev->dev;
	ret = misc_register(pdata->misc);
	if (ret)
		goto err_misc;

	platform_set_drvdata(pdev, pdata); // @platform_device
	pdev->dev.platform_data = pdata;   // @device

	np = of_find_compatible_node(NULL, NULL, "telechips,tccmisc");
	if (np == NULL) {
		pr_err("%s: tccmisc dt is not exist\n", __func__);
		ret = -ENODEV;
		goto err_dt;
	}

	mutex_init(&(pdata->io_mutex));

	device_create_file(&pdev->dev, &dev_attr_pmap);
	device_create_file(&pdev->dev, &dev_attr_debug);

	pr_info("%s\n", __func__);
	return 0;

err_dt:
	misc_deregister(pdata->misc);
err_misc:
	kfree(pdata->misc);
err:
	kfree(pdata);
exit:
	pr_err("%s: err(%d)\n", __func__, ret);
	return ret;
}

static int tccmisc_drv_remove(struct platform_device *pdev)
{
	struct tccmisc_data_t *pdata;

	pdata = (struct tccmisc_data_t *)platform_get_drvdata(pdev);

	device_remove_file(&pdev->dev, &dev_attr_pmap);
	device_remove_file(&pdev->dev, &dev_attr_debug);

	misc_deregister(pdata->misc);
	kfree(pdata->misc);
	kfree(pdata);

	return 0;
}

static const struct of_device_id tccmisc_of_match[] = {
	{ .compatible = "telechips,tccmisc" },
	{}
};
MODULE_DEVICE_TABLE(of, tccmisc_of_match);

static struct platform_driver tccmisc_driver = {
	.probe = tccmisc_drv_probe,
	.remove = tccmisc_drv_remove,
	.driver = {
		.name = "tccmisc",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tccmisc_of_match),
	},
};

static int __init tccmisc_drv_init(void)
{
	return platform_driver_register(&tccmisc_driver);
}

static void __exit tccmisc_drv_exit(void)
{
	platform_driver_unregister(&tccmisc_driver);
}

module_init(tccmisc_drv_init);
module_exit(tccmisc_drv_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("tccmisc driver");
MODULE_LICENSE("GPL");
