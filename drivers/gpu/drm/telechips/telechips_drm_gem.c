// SPDX-License-Identifier: GPL-2.0-or-later

/* telechips_drm_gem.c
 *
 * Copyright (C) 2016 Telechips Inc.
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Author: Inki Dae <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#if defined(CONFIG_REFCODE_PRE_K510)
#include <drm/drmP.h>
#include <linux/shmem_fs.h>
#endif
#include <linux/dma-buf.h>
#include <linux/pfn_t.h>
#include <linux/tcc_math.h>

#include <drm/drm.h>
#include <drm/drm_gem.h>
#include <drm/drm_prime.h>
#include <drm/drm_device.h>
#include <drm/drm_vma_manager.h>

#include <drm/drm_plane.h>
#include <drm/telechips_drm.h>

#include "telechips_drm_drv.h"
#include "telechips_drm_gem.h"


#if defined(CONFIG_REFCODE_PRE_K54)
static int tcc_drm_gem_prime_attach(struct dma_buf *in_buf,
				/* coverity[misra_c_2012_rule_8_13] */
				struct device *dev,
				struct dma_buf_attachment *attach)
#else
/* coverity[misra_c_2012_rule_8_13] */
static int tcc_drm_gem_prime_attach(struct dma_buf *in_buf,
				    /* coverity[misra_c_2012_rule_8_13] */
				    struct dma_buf_attachment *attach)
#endif
{
	/* coverity[misra_c_2012_rule_11_5] */
	const struct drm_gem_object *obj = (const struct drm_gem_object *)in_buf->priv;
	int ret = 0;

	/* Restrict access to Rogue */
	if ((obj->dev->dev->parent == NULL) ||
	    (obj->dev->dev->parent != attach->dev->parent)) {
		DRM_DEV_ERROR(NULL, "%s invalid input parameters\r\n", __func__);
		ret = -EPERM;
	}
	return ret;
}

static struct sg_table *
/* coverity[misra_c_2012_rule_8_13] */
tcc_drm_gem_prime_map_dma_buf(struct dma_buf_attachment *attach,
			      enum dma_data_direction dir)
{
	struct drm_gem_object *obj =
		/* coverity[misra_c_2012_rule_11_5] */
		(struct drm_gem_object *)attach->dmabuf->priv;
	const struct tcc_drm_gem *tcc_gem =
		/* coverity[cert_arr39_c] */
		/* coverity[cert_dcl37_c] */
		/* coverity[misra_c_2012_rule_8_5] */
		/* coverity[misra_c_2012_rule_8_6] */
		/* coverity[misra_c_2012_rule_8_13] */
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_11_5] */
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_15_6] */
		/* coverity[misra_c_2012_rule_18_4] */
		/* coverity[misra_c_2012_rule_20_7] */
		/* coverity[misra_c_2012_rule_21_2] */
		(const struct tcc_drm_gem *)to_tcc_gem(obj);
	bool need_free_sgt = (bool)false;
	bool internal_ok = (bool)true;
	struct sg_table *sgt = NULL;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter dir is not used in
	 * the function.
	 */
	(void)dir;

	/* coverity[misra_c_2012_rule_10_8] */
	/* coverity[misra_c_2012_rule_11_5] */
	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (sgt == NULL) {
		internal_ok = (bool)false;
	} else {
		need_free_sgt = (bool)true;
	}

	if (internal_ok) {
		/* coverity[misra_c_2012_rule_10_8] */
		/* coverity[misra_c_2012_rule_11_5] */
		if (sg_alloc_table(sgt, 1, GFP_KERNEL) < 0) {
			internal_ok = (bool)false;
		}
	}

	if (internal_ok) {
		if (tcc_math_ulong_gt_uintmax(obj->size)) {
			internal_ok = (bool)false;
		} else {
			sg_dma_address(sgt->sgl) = tcc_gem->dma_addr;
			sg_dma_len(sgt->sgl) = (unsigned int)obj->size;
		}
	}
	if (!internal_ok) {
		if (need_free_sgt) {
			kfree(sgt);
			sgt = NULL;
		}
	}
	return sgt;
}

/* coverity[misra_c_2012_rule_8_13] */
static void tcc_drm_gem_prime_unmap_dma_buf(struct dma_buf_attachment *attach,
					struct sg_table *sgt,
					enum dma_data_direction dir)
{
	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter attach is not used
	 * in the function
	 */
	(void)attach;
	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter dir is not used
	 * in the function
	 */
	(void)dir;
	sg_free_table(sgt);
	kfree(sgt);
}

#if defined(CONFIG_REFCODE_PRE_K510)
/* coverity[misra_c_2012_rule_8_13] */
static void *tcc_drm_gem_prime_kmap(struct dma_buf *in_buf,
				unsigned long page_num)
{
	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter in_buf is not used
	 * in the function
	 */
	(void)in_buf;
	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter page_num is not used
	 * in the function
	 */
	(void)page_num;
	return NULL;
}
#endif

static int tcc_drm_gem_mmap_buffer(const struct tcc_drm_gem *tcc_gem,
				      struct vm_area_struct *vma)
{
	const struct drm_device *drm_dev = tcc_gem->base.dev;
	unsigned long vm_size;
	bool internal_ok = (bool)true;
	int ret = 0;
	const unsigned long vm_flags = VM_PFNMAP;

	vma->vm_flags &= ~vm_flags;
	vma->vm_pgoff = 0;

	vm_size = vma->vm_end - vma->vm_start;

	/* check if user-requested size is valid. */
	if (vm_size > tcc_gem->size) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	}

	if (internal_ok) {
		/* coverity[misra_c_2012_rule_8_15] */
		/* coverity[misra_c_2012_rule_11_5] */
		ret = dma_mmap_attrs(drm_dev->dev, vma, tcc_gem->cookie,
				tcc_gem->dma_addr, tcc_gem->size,
				tcc_gem->dma_attrs);
		if (ret < 0) {
			DRM_ERROR("failed to mmap.\n");
		}
	}

	return ret;
}

/* coverity[misra_c_2012_rule_8_13] */
static int tcc_drm_gem_mmap_obj(struct drm_gem_object *obj,
				   struct vm_area_struct *vma)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	const struct tcc_drm_gem *tcc_gem = to_tcc_gem(obj);
	int ret;

	DRM_DEBUG_KMS("flags = 0x%x\n", tcc_gem->mem_flags);

	/* non-cachable as default. */
	if ((tcc_gem->mem_flags & TCC_BO_CACHABLE) != 0U) {
		vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
	} else if ((tcc_gem->mem_flags & TCC_BO_WC) != 0U) {
		vma->vm_page_prot =
			pgprot_writecombine(vm_get_page_prot(vma->vm_flags));
	} else {
		vma->vm_page_prot =
			pgprot_noncached(vm_get_page_prot(vma->vm_flags));
	}

	ret = tcc_drm_gem_mmap_buffer((const struct tcc_drm_gem *)tcc_gem, vma);
	if (ret < 0) {
		drm_gem_vm_close(vma);
	}

	return ret;
}

/* coverity[misra_c_2012_rule_8_13] */
static int tcc_drm_gem_prime_mmap(struct dma_buf *in_buf,
				  struct vm_area_struct *vma)
{
	/* coverity[misra_c_2012_rule_11_5] */
	struct drm_gem_object *obj = (struct drm_gem_object *)in_buf->priv;
	int ret;

	mutex_lock(&obj->dev->struct_mutex);
	ret = drm_gem_mmap_obj(obj, obj->size, vma);
	if (ret == 0) {
		ret = tcc_drm_gem_mmap_obj(obj, vma);
	}
	mutex_unlock(&obj->dev->struct_mutex);
	return ret;
}

static const struct dma_buf_ops tcc_drm_gem_prime_dmabuf_ops = {
	.attach = tcc_drm_gem_prime_attach,
	.map_dma_buf = tcc_drm_gem_prime_map_dma_buf,
	.unmap_dma_buf = tcc_drm_gem_prime_unmap_dma_buf,
	.release = drm_gem_dmabuf_release,
	#if defined(CONFIG_REFCODE_PRE_K54)
	.map_atomic = tcc_drm_gem_prime_kmap_atomic,
	#elif defined(CONFIG_REFCODE_PRE_K510)
	.map = tcc_drm_gem_prime_kmap,
	#endif
	.mmap = tcc_drm_gem_prime_mmap,
};

static int tcc_drm_gem_lookup_object(struct drm_file *file_priv, u32 in_handle,
			  struct drm_gem_object **objp)
{
	struct drm_gem_object *obj;
	bool internal_ok = (bool)true;
	int ret = 0;

	obj = drm_gem_object_lookup(file_priv, in_handle);
	if (obj == NULL) {
		internal_ok = (bool)false;
		ret = -ENOENT;
	}

	if (internal_ok &&
	    (obj->import_attach != NULL)) {
		/*
		 * The dmabuf associated with the object is not one of ours.
		 * Our own buffers are handled differently on import.
		 */
		#if defined(CONFIG_REFCODE_PRE_K54)
		drm_gem_object_unreference_unlocked(obj);
		#elif defined(CONFIG_REFCODE_PRE_K510)
		drm_gem_object_put_unlocked(obj);
		#else
		drm_gem_object_put(obj);
		#endif
		internal_ok = (bool)false;
		ret = -EINVAL;
	}

	if (internal_ok) {
		*objp = obj;
	}
	return ret;
}


#if defined(CONFIG_REFCODE_PRE_K54)
struct dma_buf *tcc_drm_gem_prime_export(
				     struct drm_device *dev,
				     struct drm_gem_object *obj,
				     int in_flags)
#else
struct dma_buf *tcc_drm_gem_prime_export(
				     struct drm_gem_object *obj,
				     int in_flags)
#endif
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	const struct tcc_drm_gem *tcc_gem = to_tcc_gem(obj);

	DEFINE_DMA_BUF_EXPORT_INFO(export_info);

	export_info.ops = &tcc_drm_gem_prime_dmabuf_ops;
	export_info.size = obj->size;
	export_info.flags = in_flags;
	export_info.resv = tcc_gem->resv;
	export_info.priv = obj;

	#if defined(CONFIG_REFCODE_PRE_K54)
	return drm_gem_dmabuf_export(dev, &export_info);
	#else
	return drm_gem_dmabuf_export(obj->dev, &export_info);
	#endif
}

struct drm_gem_object *tcc_drm_gem_prime_import(
		struct drm_device *dev, struct dma_buf *in_buf)
{
	/* coverity[misra_c_2012_rule_11_5] */
	struct drm_gem_object *obj = (struct drm_gem_object *)in_buf->priv;

	if (obj->dev == dev) {
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_15_6] */
		BUG_ON(in_buf->ops != &tcc_drm_gem_prime_dmabuf_ops);
		/*
		 * The dmabuf is one of ours, so return the associated
		 * TCC GEM object, rather than create a new one.
		 */
		drm_gem_object_get(obj);
	} else {
		/* coverity[misra_c_2012_rule_8_15] */
		/* coverity[misra_c_2012_rule_11_5] */
		obj = drm_gem_prime_import_dev(dev, in_buf, dev->dev);
	}
	return obj;
}

/* coverity[misra_c_2012_rule_8_13] */
struct dma_resv *tcc_gem_prime_res_obj(struct drm_gem_object *obj)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	const struct tcc_drm_gem *tcc_gem = to_tcc_gem(obj);

	return tcc_gem->resv;
}

/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 2>
 */
static int tcc_drm_alloc_buf(struct tcc_drm_gem *tcc_gem)
{
	const struct drm_device *dev = tcc_gem->base.dev;
	unsigned long attr = 0UL;
	size_t nr_pages;
	struct sg_table sgt;
	bool internal_ok = (bool)true;
	bool need_free = (bool)false;
	bool need_dma_free = (bool)false;
	bool need_sgt_free = (bool)false;
	int ret = 0;

	if (tcc_gem->dma_addr != 0UL) {
		DRM_DEBUG_KMS("already allocated.\n");
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		tcc_gem->dma_attrs = DMA_ATTR_NO_KERNEL_MAPPING;

		/*
		 * if TCC_BO_CONTIG, fully physically contiguous memory
		 * region will be allocated else physically contiguous
		 * as possible.
		 */
		if ((tcc_gem->mem_flags & TCC_BO_NONCONTIG) == 0U) {
			tcc_gem->dma_attrs |= DMA_ATTR_FORCE_CONTIGUOUS;
		}

		/*
	 	 * if TCC_BO_WC or TCC_BO_NONCACHABLE, writecombine mapping
		 * else cachable mapping.
		 */
		if (((tcc_gem->mem_flags & TCC_BO_WC) != 0U) ||
		    ((tcc_gem->mem_flags & TCC_BO_CACHABLE) == 0U)) {
			attr = DMA_ATTR_WRITE_COMBINE;
		#if defined(CONFIG_REFCODE_PRE_K510)
		} else {
			attr = DMA_ATTR_NON_CONSISTENT;
		#endif
		}

		tcc_gem->dma_attrs |= attr;

		if(tcc_math_ulong_gt_uintmax(tcc_gem->size >> PAGE_SHIFT)) {
			internal_ok = (bool)false;
			ret = -EINVAL;
		} else {
			nr_pages = tcc_gem->size >> PAGE_SHIFT;
		}
	}

	if (internal_ok) {
		tcc_gem->pagelist =
			/* coverity[misra_c_2012_rule_11_5] */
			(struct page **)kvmalloc_array(nr_pages,
						      sizeof(struct page *),
			/* coverity[misra_c_2012_rule_10_8] */
			/* coverity[misra_c_2012_rule_11_5] */
							GFP_KERNEL | __GFP_ZERO);
		if (tcc_gem->pagelist == NULL) {
			internal_ok = (bool)false;
			ret = -ENOMEM;
		} else {
			need_free = (bool)true;
		}
	}
	if (internal_ok) {
		tcc_gem->cookie =
			/* coverity[misra_c_2012_rule_8_15] */
			/* coverity[misra_c_2012_rule_11_5] */
			dma_alloc_attrs(dev->dev, tcc_gem->size,
				/* coverity[misra_c_2012_rule_10_8] */
				/* coverity[misra_c_2012_rule_11_5] */
				&tcc_gem->dma_addr, GFP_KERNEL,
				tcc_gem->dma_attrs);
		if (tcc_gem->cookie == NULL) {
			DRM_DEV_ERROR(dev->dev,
				      "%s failed to allocate buffer.\n",
				      __func__);
			internal_ok = (bool)false;
			ret = -ENOMEM;
		} else {
			need_dma_free = (bool)true;
		}
	}
	if (internal_ok) {
		/* coverity[misra_c_2012_rule_8_15] */
		/* coverity[misra_c_2012_rule_11_5] */
		ret = dma_get_sgtable_attrs(dev->dev, &sgt, tcc_gem->cookie,
					tcc_gem->dma_addr, tcc_gem->size,
					tcc_gem->dma_attrs);
		if (ret < 0) {
			DRM_DEV_ERROR(dev->dev,
				      "%s failed to get sgtable.\n",
				      __func__);
			internal_ok = (bool)false;
			ret = -ENOMEM;
		} else {
			need_sgt_free = (bool)true;
		}
	}

	if (internal_ok) {
		if (tcc_math_ulong_gt_intmax(nr_pages)) {
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		if (drm_prime_sg_to_page_addr_arrays(&sgt,
						     tcc_gem->pagelist, NULL,
					     	     (int)nr_pages) < 0) {
		DRM_DEV_ERROR(dev->dev,
			      "%s invalid sgtable.\n", __func__);
			internal_ok = (bool)false;
			ret = -EINVAL;
		}
	}
	if (internal_ok) {
		sg_free_table(&sgt);

		DRM_DEBUG_KMS("dma_addr(0x%lx), size(0x%lx)\n",
				(unsigned long)tcc_gem->dma_addr, tcc_gem->size);
	} else {
		/* Error process */
		if (need_sgt_free) {
			sg_free_table(&sgt);
		}
		if (need_dma_free) {
			/* coverity[misra_c_2012_rule_8_15] */
			/* coverity[misra_c_2012_rule_11_5] */
			dma_free_attrs(dev->dev, tcc_gem->size, tcc_gem->cookie,
				tcc_gem->dma_addr, tcc_gem->dma_attrs);
		}
		if (need_free) {
			kvfree(tcc_gem->pagelist);
		}
	}
	return ret;
}

/* coverity[misra_c_2012_rule_8_13] */
static void tcc_drm_free_buf(struct tcc_drm_gem *tcc_gem)
{
	const struct drm_device *dev = tcc_gem->base.dev;

	if (tcc_gem->dma_addr == 0UL) {
		DRM_DEBUG_KMS("dma_addr is invalid.\n");
	} else {
		DRM_DEBUG_KMS("dma_addr(0x%lx), size(0x%lx)\n",
				(unsigned long)tcc_gem->dma_addr, tcc_gem->size);

		if (tcc_gem->cookie != NULL) {
			/* coverity[misra_c_2012_rule_8_15] */
			/* coverity[misra_c_2012_rule_11_5] */
			dma_free_attrs(dev->dev, tcc_gem->size,
				       tcc_gem->cookie,
				       (dma_addr_t)tcc_gem->dma_addr,
				       tcc_gem->dma_attrs);
		}
		tcc_gem->cookie = NULL;
	}
}

static int tcc_drm_gem_handle_create(struct drm_gem_object *obj,
					struct drm_file *file_priv,
					unsigned int *in_handle)
{
	int ret = 0;

	/*
	 * allocate a id of idr table where the obj is registered
	 * and gem_handle has the id what user can see.
	 */
	ret = drm_gem_handle_create(file_priv, obj, in_handle);
	if (ret == 0) {
		DRM_DEBUG_KMS("gem handle = 0x%x\n", *in_handle);

		/* drop reference from allocate - handle holds it now. */
		#if defined(CONFIG_REFCODE_PRE_K54)
		drm_gem_object_unreference_unlocked(obj);
		#elif defined(CONFIG_REFCODE_PRE_K510)
		drm_gem_object_put_unlocked(obj);
		#else
		drm_gem_object_put(obj);
		#endif
	}

	return ret;
}

void tcc_drm_gem_destroy(struct tcc_drm_gem *tcc_gem)
{
	struct drm_gem_object *obj;

	if (tcc_gem != NULL) {
		obj = &tcc_gem->base;

		DRM_DEBUG_KMS("handle count = %d\n", obj->handle_count);
		#if defined(CONFIG_REFCODE_PRE_K54)
		if (&tcc_gem->_resv == tcc_gem->resv) {
			dma_resv_fini(&tcc_gem->_resv);
		}
		#endif

		/*
		* do not release memory region from exporter.
		*
		* the region will be released by exporter
		* once dmabuf's refcount becomes 0.
		*/
		if (obj->import_attach != NULL) {
			drm_prime_gem_destroy(obj, tcc_gem->sgt);
		} else {
			tcc_drm_free_buf(tcc_gem);
		}

		/* release file pointer to gem object. */
		drm_gem_object_release(obj);

		if (tcc_gem->pagelist != NULL) {
			kvfree(tcc_gem->pagelist);
		}
		kfree(tcc_gem);
	}
}

/* coverity[misra_c_2012_rule_8_13] */
unsigned long tcc_drm_gem_get_size(struct drm_device *dev,
						unsigned int in_handle,
						struct drm_file *file_priv)
{
	const struct tcc_drm_gem *tcc_gem;
	struct drm_gem_object *obj;
	unsigned long size = 0UL;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter dev is not used
	 * in the function
	 */
	(void)dev;

	obj = drm_gem_object_lookup(file_priv, in_handle);
	if (obj != NULL) {
		/* coverity[cert_arr39_c] */
		/* coverity[cert_dcl37_c] */
		/* coverity[misra_c_2012_rule_8_5] */
		/* coverity[misra_c_2012_rule_8_6] */
		/* coverity[misra_c_2012_rule_8_13] */
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_11_5] */
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_15_6] */
		/* coverity[misra_c_2012_rule_18_4] */
		/* coverity[misra_c_2012_rule_20_7] */
		/* coverity[misra_c_2012_rule_21_2] */
		tcc_gem = to_tcc_gem(obj);

		#if defined(CONFIG_REFCODE_PRE_K54)
		drm_gem_object_unreference_unlocked(obj);
		#elif defined(CONFIG_REFCODE_PRE_K510)
		drm_gem_object_put_unlocked(obj);
		#else
		drm_gem_object_put(obj);
		#endif
		size = tcc_gem->size;
	} else {
		DRM_ERROR("failed to lookup gem object.\n");
	}
	return size;
}

static struct tcc_drm_gem *tcc_drm_gem_init(
		struct drm_device *dev, struct dma_resv *resv,
		unsigned long size)
{
	struct tcc_drm_gem *tcc_gem;
	struct drm_gem_object *obj;
	bool internal_ok = (bool)true;
	int ret;

	/* coverity[misra_c_2012_rule_10_8] */
	/* coverity[misra_c_2012_rule_11_5] */
	tcc_gem = (struct tcc_drm_gem *)kzalloc(sizeof(*tcc_gem), GFP_KERNEL);
	if (tcc_gem == NULL) {
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		tcc_gem->size = size;
		obj = &tcc_gem->base;

		#if defined(CONFIG_REFCODE_PRE_K54)
		if (resv == NULL) {
			dma_resv_init(&tcc_gem->d_resv);
			tcc_gem->resv = &tcc_gem->d_resv;
		} else {
			tcc_gem->resv = resv;
		}
		#else
		tcc_gem->base.resv = resv;
		#endif
		ret = drm_gem_object_init(dev, obj, size);
		if (ret < 0) {
			DRM_ERROR("failed to initialize gem object\n");
			kfree(tcc_gem);
			internal_ok = (bool)false;
			/* coverity[misra_c_2012_rule_11_5] */
			tcc_gem = ERR_PTR(ret);
		}
	}

	if (internal_ok) {
		#if defined(CONFIG_REFCODE_PRE_K54)
		#else
		tcc_gem->resv = tcc_gem->base.resv;
		#endif
		ret = drm_gem_create_mmap_offset(obj);
		if (ret < 0) {
			#if defined(CONFIG_REFCODE_PRE_K54)
			if (&tcc_gem->d_resv == tcc_gem->resv) {
				dma_resv_fini(&tcc_gem->d_resv);
			}
			#endif
			drm_gem_object_release(obj);
			kfree(tcc_gem);
			/* coverity[misra_c_2012_rule_11_5] */
			tcc_gem = ERR_PTR(ret);
		}
	}

	return tcc_gem;
}

/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 2>
 */
struct tcc_drm_gem *tcc_drm_gem_create(struct drm_device *dev,
					     unsigned int mem_flags,
					     unsigned long size)
{
	/* coverity[misra_c_2012_rule_11_5] */
	struct tcc_drm_gem *tcc_gem = ERR_PTR(-EINVAL);
	bool internal_ok = (bool)true;
	int ret;

	if ((mem_flags & ~(TCC_BO_MASK)) != 0U) {
		DRM_ERROR("invalid GEM buffer flags: %u\n", mem_flags);
		internal_ok = (bool)false;
	}

	if (internal_ok && (size == 0UL)) {
		DRM_ERROR("invalid GEM buffer size: %lu\n", size);
		internal_ok = (bool)false;
	}

	if (internal_ok) {
		unsigned long const_page_size = (unsigned long)PAGE_SIZE;

		if (tcc_math_check_ulong_plus_ulong(size, const_page_size - 1UL)) {
			/* coverity[cert_dcl37_c] */
			/* coverity[misra_c_2012_rule_10_4] */
			/* coverity[misra_c_2012_rule_20_7] */
			size = roundup(size, const_page_size);
		} else {
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		tcc_gem = tcc_drm_gem_init(dev, NULL, size);
		if (IS_ERR(tcc_gem) || (tcc_gem == NULL)) {
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		if ((mem_flags & TCC_BO_NONCONTIG) != 0U) {
			/*
			 * when no IOMMU is available, all allocated buffers are
			 * contiguous anyway, so drop TCC_BO_NONCONTIG flag
			 */
			mem_flags &= ~TCC_BO_NONCONTIG;
			DRM_DEV_INFO(dev->dev,
				"Non-contiguous allocation is not supported without IOMMU, falling back to contiguous buffer\n");
		}

		/* set memory type and cache attribute from user side. */
		tcc_gem->mem_flags = mem_flags;

		ret = tcc_drm_alloc_buf(tcc_gem);
		if (ret < 0) {
			drm_gem_object_release(&tcc_gem->base);
			kfree(tcc_gem);
			/* coverity[misra_c_2012_rule_11_5] */
			tcc_gem = ERR_PTR(ret);
		}
	}
	return tcc_gem;
}

int tcc_drm_gem_create_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv)
{
	/* coverity[misra_c_2012_rule_11_5] */
	struct drm_tcc_gem_create *args = data;
	struct tcc_drm_gem *tcc_gem;
	bool internal_ok = (bool)true;
	int ret = 0;

	tcc_gem = tcc_drm_gem_create(dev, args->flags, args->size);
	if (IS_ERR(tcc_gem)) {
		internal_ok = (bool)false;
		ret = -ENOMEM;
	}

	if (internal_ok) {
		ret = tcc_drm_gem_handle_create(&tcc_gem->base, file_priv,
					&args->handle);
		if (ret != 0) {
			tcc_drm_gem_destroy(tcc_gem);
		}
	}

	return ret;
}

int tcc_drm_gem_map_ioctl(struct drm_device *dev, void *data,
			     struct drm_file *file_priv)
{
	/* coverity[misra_c_2012_rule_11_5] */
	struct drm_tcc_gem_map *args = (struct drm_tcc_gem_map *)data;

	return drm_gem_dumb_map_offset(file_priv, dev, args->handle,
				       &args->offset);
}


/* coverity[misra_c_2012_rule_8_13] */
int tcc_drm_gem_get_ioctl(struct drm_device *dev, void *data,
				      struct drm_file *file_priv)
{
	const struct tcc_drm_gem *tcc_gem;
	/* coverity[misra_c_2012_rule_11_5] */
	struct drm_tcc_gem_info *args = (struct drm_tcc_gem_info *)data;
	struct drm_gem_object *obj;
	int ret = 0;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter dev is not used in
	 * the function.
	 */
	(void)dev;

	obj = drm_gem_object_lookup(file_priv, args->handle);
	if (obj == NULL) {
		DRM_ERROR("failed to lookup gem object.\n");
		ret = -EINVAL;
	} else {
		/* coverity[cert_arr39_c] */
		/* coverity[cert_dcl37_c] */
		/* coverity[misra_c_2012_rule_8_5] */
		/* coverity[misra_c_2012_rule_8_6] */
		/* coverity[misra_c_2012_rule_8_13] */
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_11_5] */
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_15_6] */
		/* coverity[misra_c_2012_rule_18_4] */
		/* coverity[misra_c_2012_rule_20_7] */
		/* coverity[misra_c_2012_rule_21_2] */
		tcc_gem = (const struct tcc_drm_gem *)to_tcc_gem(obj);

		args->flags = tcc_gem->mem_flags;
		args->size = tcc_gem->size;

		#if defined(CONFIG_REFCODE_PRE_K54)
		drm_gem_object_unreference_unlocked(obj);
		#elif defined(CONFIG_REFCODE_PRE_K510)
		drm_gem_object_put_unlocked(obj);
		#else
		drm_gem_object_put(obj);
		#endif

	}

	return ret;
}

/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 2>
 */
/* coverity[misra_c_2012_rule_8_13] */
int tcc_gem_cpu_prep_ioctl(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	const struct drm_tcc_gem_cpu_prep *args =
		/* coverity[misra_c_2012_rule_11_5] */
		(struct drm_tcc_gem_cpu_prep *)data;
	struct drm_gem_object *obj;
	struct tcc_drm_gem *tcc_gem;
	bool internal_ok = (bool)true;
	bool need_unlock = (bool)false;
	bool need_unref = (bool)false;
	bool arg_write, arg_wait;
	int err = 0;

	arg_write = ((args->flags & TCC_GEM_CPU_PREP_WRITE) != 0U) ? (bool)true : (bool)false;
	arg_wait = ((args->flags & TCC_GEM_CPU_PREP_NOWAIT) == 0U) ? (bool)true : (bool)false;

	if ((args->flags & ~TCC_GEM_CPU_PREP_FLAGS)  != 0U) {
		DRM_ERROR("invalid flags: %#08x\n", args->flags);
		internal_ok = (bool)false;
		err = -EINVAL;
	}

	if (internal_ok) {
		mutex_lock(&dev->struct_mutex);
		need_unlock = (bool)true;
		err = tcc_drm_gem_lookup_object(file_priv, args->handle, &obj);
		if (err < 0) {
			internal_ok = (bool)false;
		} else {
			need_unref = (bool)true;
		}
	}

	if (internal_ok) {

		/* coverity[cert_arr39_c] */
		/* coverity[cert_dcl37_c] */
		/* coverity[misra_c_2012_rule_8_5] */
		/* coverity[misra_c_2012_rule_8_6] */
		/* coverity[misra_c_2012_rule_8_13] */
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_11_5] */
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_15_6] */
		/* coverity[misra_c_2012_rule_18_4] */
		/* coverity[misra_c_2012_rule_20_7] */
		/* coverity[misra_c_2012_rule_21_2] */
		tcc_gem = to_tcc_gem(obj);
		if (tcc_gem->cpu_prep) {
			internal_ok = (bool)false;
			err = -EBUSY;
		}
	}

	if (internal_ok) {
		#if defined(CONFIG_REFCODE_PRE_K54)
		#else
		if (tcc_gem->resv == NULL) {
			dev_err(dev->dev, "[ERROR][GEM] tcc_gem->resv is NULL\r\n");
			internal_ok = (bool)false;
			err = -EINVAL;
		}
		#endif
	}

	if (internal_ok) {
		if (arg_wait) {
			long lerr;

			lerr = dma_resv_wait_timeout_rcu(tcc_gem->resv,
							arg_write,
							(bool)true,
							30 * HZ);
			if (lerr <= 0L) {
				internal_ok = (bool)false;
				err = -EBUSY;
			}
		} else {
			if (!dma_resv_test_signaled_rcu(tcc_gem->resv,
							arg_write)) {
				internal_ok = (bool)false;
				err = -EBUSY;
			}
		}
	}

	if (internal_ok) {
		tcc_gem->cpu_prep = (bool)true;
	}
	if (need_unref) {
		#if defined(CONFIG_REFCODE_PRE_K54)
		drm_gem_object_unreference_unlocked(obj);
		#elif defined(CONFIG_REFCODE_PRE_K510)
		drm_gem_object_put_unlocked(obj);
		#else
		drm_gem_object_put(obj);
		#endif

	}
	if (need_unlock) {
		mutex_unlock(&dev->struct_mutex);
	}
	return err;
}

/* coverity[misra_c_2012_rule_8_13] */
int tcc_gem_cpu_fini_ioctl(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	const struct drm_tcc_gem_cpu_prep *args =
		/* coverity[misra_c_2012_rule_11_5] */
		(const struct drm_tcc_gem_cpu_prep *)data;
	bool internal_ok = (bool)true;
	bool need_unref = (bool)false;
	struct drm_gem_object *obj;
	struct tcc_drm_gem *tcc_gem;
	int err = 0;

	mutex_lock(&dev->struct_mutex);
	err = tcc_drm_gem_lookup_object(file_priv, args->handle, &obj);
	if (err < 0) {
		internal_ok = (bool)false;
	} else {
		need_unref = (bool)true;
	}
	if (internal_ok) {
		/* coverity[cert_arr39_c] */
		/* coverity[cert_dcl37_c] */
		/* coverity[misra_c_2012_rule_8_5] */
		/* coverity[misra_c_2012_rule_8_6] */
		/* coverity[misra_c_2012_rule_8_13] */
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_11_5] */
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_15_6] */
		/* coverity[misra_c_2012_rule_18_4] */
		/* coverity[misra_c_2012_rule_20_7] */
		/* coverity[misra_c_2012_rule_21_2] */
		tcc_gem = to_tcc_gem(obj);
		if (!tcc_gem->cpu_prep) {
			internal_ok = (bool)false;
			err = -EINVAL;
		}
	}
	if (internal_ok) {
		tcc_gem->cpu_prep = (bool)false;
	}

	if (need_unref) {
		#if defined(CONFIG_REFCODE_PRE_K54)
		drm_gem_object_unreference_unlocked(obj);
		#elif defined(CONFIG_REFCODE_PRE_K510)
		drm_gem_object_put_unlocked(obj);
		#else
		drm_gem_object_put(obj);
		#endif

	}
	mutex_unlock(&dev->struct_mutex);

	return err;
}

/* coverity[misra_c_2012_rule_8_13] */
void tcc_drm_gem_free_object(struct drm_gem_object *obj)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	tcc_drm_gem_destroy(to_tcc_gem(obj));
}

int tcc_drm_gem_dumb_create(struct drm_file *file_priv,
			       struct drm_device *dev,
			       struct drm_mode_create_dumb *args)
{
	struct tcc_drm_gem *tcc_gem;
	unsigned long pitch, height;
	unsigned int mem_flags;
	bool internal_ok = (bool)true;
	int ret = 0;

	/*
	 * allocate memory to be used for framebuffer.
	 * - this callback would be called by user application
	 *	with DRM_IOCTL_MODE_CREATE_DUMB command.
	 */
	/* coverity[cert_int30_c] */
	/* coverity[misra_c_2012_rule_10_4] */
	args->pitch = ALIGN(args->width * DIV_ROUND_UP(args->bpp, 8U), 8U);

	/* -- */
	/* args->size = args->pitch * args->height; */
	pitch = (unsigned long)args->pitch;
	height = (unsigned long)args->height;
	if (!tcc_math_check_ulong_mul_ulong(pitch, height)) {
		internal_ok = (bool)false;
		ret = -ENOMEM;
	} else {
		args->size = pitch * height;
	}
	/* -- */

	if (internal_ok) {
		/*
		 * Default mem_flags are contiguous and write combine
		 * for preformance.
		 */
		mem_flags = TCC_BO_CONTIG | TCC_BO_WC;

		tcc_gem = tcc_drm_gem_create(dev, mem_flags, args->size);
		if (IS_ERR(tcc_gem)) {
			dev_warn(dev->dev, "FB allocation failed.\n");
			internal_ok = (bool)false;
			/* coverity[misra_c_2012_rule_10_3] */
			ret = PTR_ERR(tcc_gem);
		}
	}

	if (internal_ok) {
		ret = tcc_drm_gem_handle_create(&tcc_gem->base, file_priv,
						&args->handle);
		if (ret != 0) {
			tcc_drm_gem_destroy(tcc_gem);
		}
	}

	return ret;
}

#if defined(CONFIG_REFCODE_PRE_K54)
/* coverity[misra_c_2012_rule_8_13] */
int tcc_drm_gem_fault(struct vm_fault *vmf)
#else
/* coverity[misra_c_2012_rule_8_13] */
vm_fault_t tcc_drm_gem_fault(struct vm_fault *vmf)
#endif
{
	struct vm_area_struct *vma = vmf->vma;
	/* coverity[misra_c_2012_rule_11_5] */
	struct drm_gem_object *obj = vma->vm_private_data;
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	const struct tcc_drm_gem *tcc_gem = to_tcc_gem(obj);
	bool internal_ok = (bool)true;
	unsigned long pfn;
	pgoff_t page_offsets;
	#if defined(CONFIG_REFCODE_PRE_K54)
	int ret;
	#else
	vm_fault_t ret;
	#endif

	if (vmf->address < vma->vm_start) {
		internal_ok = (bool)false;
		ret = (vm_fault_t)VM_FAULT_SIGBUS;
	}

	if (internal_ok) {
		page_offsets = (vmf->address - vma->vm_start) >> PAGE_SHIFT;

		if (page_offsets >= (tcc_gem->size >> PAGE_SHIFT)) {
			DRM_ERROR("invalid page offset\n");
			internal_ok = (bool)false;
			#if defined(CONFIG_REFCODE_PRE_K54)
			ret = -EINVAL;
			#else
			ret = (vm_fault_t)VM_FAULT_SIGBUS;
			#endif
		}
	}

	if (internal_ok) {
		/* coverity[cert_int31_c] */
		/* coverity[cert_int36_c] */
		/* coverity[cert_int02_c] */
		/* coverity[misra_c_2012_rule_10_1] */
		/* coverity[misra_c_2012_rule_10_3] */ // K5.4
		/* coverity[misra_c_2012_rule_10_4] */
		/* coverity[misra_c_2012_rule_10_8] */
		/* coverity[misra_c_2012_rule_12_1] */
		/* coverity[misra_c_2012_rule_14_3] */
		/* coverity[misra_c_2012_rule_18_4] */
		pfn = page_to_pfn(tcc_gem->pagelist[page_offsets]);
		#if defined(CONFIG_REFCODE_PRE_K54)
		ret = vm_insert_mixed(
			vma, vmf->address, __pfn_to_pfn_t(pfn, PFN_DEV));
		#else
		ret = vmf_insert_mixed(
			vma, vmf->address, __pfn_to_pfn_t(pfn, PFN_DEV));
		#endif
	}

	#if defined(CONFIG_REFCODE_PRE_K54)
	switch (ret) {
	case 0:
	case -ERESTARTSYS:
	case -EINTR:
		ret = VM_FAULT_NOPAGE;
		break;
	case -ENOMEM:
		ret = VM_FAULT_OOM;
		break;
	default:
		ret = VM_FAULT_SIGBUS;
		break;
	}
	#endif
	return ret;
}

int tcc_drm_gem_mmap(struct file *filp,
			struct vm_area_struct *vma)
{
	struct drm_gem_object *obj;
	int ret;

	/* set vm_area_struct. */
	ret = drm_gem_mmap(filp, vma);
	if (ret < 0) {
		DRM_ERROR("failed to mmap.\n");
	} else {
		/* DP no.15 */
		/* coverity[misra_c_2012_rule_11_5] */
		obj = (struct drm_gem_object *)vma->vm_private_data;

		if (obj->import_attach != NULL) {
			ret = dma_buf_mmap(obj->dma_buf, vma, 0);
		} else {
			ret = tcc_drm_gem_mmap_obj(obj, vma);
		}
	}
	return ret;
}

/* low-level interface prime helpers */
/* coverity[misra_c_2012_rule_8_13] */
struct sg_table *tcc_drm_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
	/* coverity[cert_arr39_c] */
	/* coverity[cert_dcl37_c] */
	/* coverity[misra_c_2012_rule_8_5] */
	/* coverity[misra_c_2012_rule_8_6] */
	/* coverity[misra_c_2012_rule_8_13] */
	/* coverity[misra_c_2012_rule_10_1] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_14_4] */
	/* coverity[misra_c_2012_rule_15_6] */
	/* coverity[misra_c_2012_rule_18_4] */
	/* coverity[misra_c_2012_rule_20_7] */
	/* coverity[misra_c_2012_rule_21_2] */
	const struct tcc_drm_gem *tcc_gem = to_tcc_gem(obj);
	/* coverity[misra_c_2012_rule_11_5] */
	struct sg_table *prime_sg_table = (struct sg_table *)ERR_PTR(-EINVAL);
	unsigned long ulpages;
	unsigned int npages;

	ulpages = tcc_gem->size >> (size_t)PAGE_SHIFT;

	/* npages = tcc_gem->size >> PAGE_SHIFT; */
	if (!tcc_math_ulong_gt_uintmax(ulpages)) {
		npages = (unsigned int)ulpages;
		#if defined(CONFIG_REFCODE_PRE_K510)
		prime_sg_table =
			drm_prime_pages_to_sg(tcc_gem->pagelist, npages);
		#else
		prime_sg_table =
			drm_prime_pages_to_sg(obj->dev, tcc_gem->pagelist, npages);
		#endif
	}
	/* -- */

	return prime_sg_table;
}

/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 2>
 */
struct drm_gem_object *
tcc_drm_gem_prime_import_sg_table(struct drm_device *dev,
				     /* coverity[misra_c_2012_rule_8_13] */
				     struct dma_buf_attachment *attach,
				     struct sg_table *sgt)
{
	struct tcc_drm_gem *tcc_gem;
	/* coverity[misra_c_2012_rule_11_5] */
	struct drm_gem_object *obj = (struct drm_gem_object *)ERR_PTR(-EINVAL);
	unsigned int unpages;
	size_t npages;
	int inpages;
	int ret;

	bool need_free_large = (bool)false;
	bool need_release = (bool)false;
	bool internal_ok = (bool)true;

	tcc_gem = tcc_drm_gem_init(
			dev, attach->dmabuf->resv, attach->dmabuf->size);
	if (IS_ERR(tcc_gem) || (tcc_gem == NULL)) {
		internal_ok = (bool)false;
	//} else {
	//	need_release = (bool)true;
	}

	if (internal_ok) {
		/*
		 * tcc_drm_gem_init is success
		 *  - It will be release tcc_gem->base using
		 *    drm_gem_object_release when this function is failed.
		 */
		need_release = (bool)true;

		tcc_gem->sgt = sgt;
		if (tcc_gem->sgt->nents != 1U) {
			internal_ok = (bool)false;
			/* coverity[misra_c_2012_rule_17_7] */
			pr_err("[ERR][DRMCRTC] %s nents is %d - Not continuous memory!!\r\n",
				__func__, tcc_gem->sgt->nents);
		}
	}
	if (internal_ok) {
		tcc_gem->dma_addr = sg_dma_address(sgt->sgl);

		npages = tcc_gem->size >> (size_t)PAGE_SHIFT;
		tcc_gem->pagelist =
			/* DP no.13 */
			/* coverity[misra_c_2012_rule_10_8] */
			/* coverity[misra_c_2012_rule_11_5] */
			kvmalloc_array(npages, sizeof(struct page *), GFP_KERNEL);
		if (tcc_gem->pagelist == NULL) {
			internal_ok = (bool)false;
			/* coverity[misra_c_2012_rule_11_5] */
			obj = (struct drm_gem_object *)ERR_PTR(-ENOMEM);
		//} else  {
		//	need_free_large = (bool)true;
		}
	}

	if (internal_ok) {
		/*
		 * kvmalloc_array is success - tcc_gem->pagelist will be free
		 * from memory*/
		need_free_large = (bool)true;

		if (tcc_math_ulong_gt_uintmax(npages)) {
			/* coverity[misra_c_2012_rule_11_5] */
			obj = (struct drm_gem_object *)ERR_PTR(-EINVAL);
			internal_ok = (bool)false;
		//} else {
		//	unpages = (unsigned int)npages;
		}
	}
	if (internal_ok) {
		/*
		 * npages can convert to uint because it is not great then
		 * UINT_MAX
		 */
		unpages = (unsigned int)npages;

		if (tcc_math_uint_gt_intmax(unpages)) {
			/* coverity[misra_c_2012_rule_11_5] */
			obj = (struct drm_gem_object *)ERR_PTR(-EINVAL);
			internal_ok = (bool)false;
		} else {
			inpages = (int)unpages;
		}
	}
	if (internal_ok) {
		ret = drm_prime_sg_to_page_addr_arrays(sgt, tcc_gem->pagelist, NULL,
						inpages);
		if (ret < 0) {
			/* coverity[misra_c_2012_rule_11_5] */
			obj = (struct drm_gem_object *)ERR_PTR(ret);
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		tcc_gem->sgt = sgt;

		if (sgt->nents != 1U) {
			/*
			* this case could be CONTIG or NONCONTIG type but for now
			* sets NONCONTIG.
			* TODO. we have to find a way that exporter can notify
			* the type of its own buffer to importer.
			*/
			tcc_gem->mem_flags |= TCC_BO_NONCONTIG;
		}
		obj = &tcc_gem->base;
	}
	if (!internal_ok) {
		if (need_free_large) {
			kvfree(tcc_gem->pagelist);
		}
		if (need_release) {
			drm_gem_object_release(&tcc_gem->base);
			kfree(tcc_gem);
		}
	}
	return obj;
}

/* coverity[misra_c_2012_rule_8_13] */
void *tcc_drm_gem_prime_vmap(struct drm_gem_object *obj)
{
	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter obj is not used
	 * in the function
	 */
	(void)obj;

	return NULL;
}

/* coverity[misra_c_2012_rule_8_13] */
void tcc_drm_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr)
{
	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter obj is not used
	 * in the function
	 */
	(void)obj;
	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter vaddr is not used
	 * in the function
	 */
	(void)vaddr;
}

