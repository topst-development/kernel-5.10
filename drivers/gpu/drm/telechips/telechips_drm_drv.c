// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2022 Telechips Inc.
 * Authors:
 *	Jayden Kim
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/pm_runtime.h>
#include <linux/component.h>
#include <linux/tcc_math.h>
#if defined(CONFIG_REFCODE_PRE_K510)
#include <drm/drmP.h>
#endif
#include <linux/platform_device.h>

#include <linux/module.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_bridge.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>
#include <drm/telechips_drm.h>

#include <telechips_drm_types.h>
#include <telechips_drm_drv.h>
#include <telechips_drm_fbdev.h>
#include <telechips_drm_fb.h>
#include <telechips_drm_gem.h>
#include <telechips_drm_edid.h>
#include <telechips_drm_vioc.h>
#include <telechips_drm_screen_share.h>
#include <telechips_drm_dp.h>
#include <telechips_drm_dummy_dev.h>
#include <telechips_drm_lvds.h>
#include <telechips_drm_dsi.h>
#include <telechips_drm_atomic_helper.h>

#if defined(CONFIG_DRM_TELECHIPS_HDMI)
#include <telechips_drm_hdmi.h>
#endif

#define DRIVER_NAME	"tccdrm"
#define DRIVER_DESC	"Telechips SoC DRM"
#define DRIVER_DATE	"20240227"
#define DRIVER_MAJOR	2
#define DRIVER_MINOR	5
#define DRIVER_PATCH	2

struct tccdrm_driver_info {
	struct platform_driver *driver;
	unsigned int drv_flags;
};

#define DRM_COMPONENT_DRIVER	BIT(0)	/* supports component framework */
#define DRM_SUBSYSTEM_DEVICE	BIT(1)	/* create virtual platform device */

static struct platform_device *tccdrm_device = NULL;

#if defined(CONFIG_REFCODE_PRE_K510)
static void tccdrm_debug_set(void)
{
	drm_debug = 0U;
	#ifdef CONFIG_DRM_UT_CORE
	drm_debug |= (unsigned int)DRM_UT_CORE;
	#endif
	#ifdef CONFIG_DRM_UT_DRIVER
	drm_debug |= (unsigned int)DRM_UT_DRIVER;
	#endif
	#ifdef CONFIG_DRM_UT_KMS
	drm_debug |= (unsigned int)DRM_UT_KMS;
	#endif
	#ifdef CONFIG_DRM_UT_PRIME
	drm_debug |= (unsigned int)DRM_UT_PRIME;
	#endif
}
#endif

/* coverity[misra_c_2012_rule_8_13] */
static int tccdrm_open(struct drm_device *dev, struct drm_file *infile)
{
	struct drm_tcc_file_private *file_priv;
	int ret = 0;

	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter dev is not used in
	 * the function.
	 */
	(void)dev;

	/* coverity[misra_c_2012_rule_10_8] */ // K5.4
	/* coverity[misra_c_2012_rule_11_5] */
	file_priv = kzalloc(sizeof(*file_priv), GFP_KERNEL);
	if (file_priv != NULL) {
		infile->driver_priv = file_priv;
	} else {
		ret = -ENOMEM;
	}
	return ret;
}

/* coverity[misra_c_2012_rule_8_13] */
static void tccdrm_postclose(struct drm_device *dev, struct drm_file *infile)
{
	/*
	 * FIX
	 * misra_c_2012_rule_2_7_violation: The parameter dev is not used in
	 * the function.
	 */
	(void)dev;

	if (infile->driver_priv != NULL) {
		kfree(infile->driver_priv);
	}
	infile->driver_priv = NULL;
}

/* coverity[misra_c_2012_rule_8_13] */
static void tccdrm_lastclose(struct drm_device *dev)
{
	#if defined(CONFIG_REFCODE_PRE_K54)
	tcc_drm_fbdev_restore_mode(dev);
	#else
	#if defined(CONFIG_DRM_TELECHIPS_SKIP_RESTORE_FBDEV)
	DRM_DEV_INFO(
		dev->dev,
		"[INFO] TCCDRM skips drm_fb_helper_restore_fbdev_mode_unlocked\r\n");
	#else
	drm_fb_helper_lastclose(dev);
	#endif
	#endif
}

static const struct vm_operations_struct tccdrm_gem_vm_ops = {
	.fault = tcc_drm_gem_fault,
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

/* coverity[misra_c_2012_rule_9_5] */
static const struct drm_ioctl_desc tcc_ioctls[] = {
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_12_1] */
	/* coverity[misra_c_2012_rule_20_7] */
	DRM_IOCTL_DEF_DRV(TCC_GEM_CREATE, tcc_drm_gem_create_ioctl,
			DRM_AUTH | DRM_RENDER_ALLOW),
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_12_1] */
	/* coverity[misra_c_2012_rule_20_7] */
	DRM_IOCTL_DEF_DRV(TCC_GEM_MAP, tcc_drm_gem_map_ioctl,
			DRM_AUTH | DRM_RENDER_ALLOW),
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_12_1] */
	/* coverity[misra_c_2012_rule_20_7] */
	DRM_IOCTL_DEF_DRV(TCC_GEM_GET, tcc_drm_gem_get_ioctl,
			DRM_RENDER_ALLOW),
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_12_1] */
	/* coverity[misra_c_2012_rule_20_7] */
	DRM_IOCTL_DEF_DRV(TCC_GEM_CPU_PREP, tcc_gem_cpu_prep_ioctl,
			DRM_AUTH | DRM_RENDER_ALLOW),
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_12_1] */
	/* coverity[misra_c_2012_rule_20_7] */
	DRM_IOCTL_DEF_DRV(TCC_GEM_CPU_FINI, tcc_gem_cpu_fini_ioctl,
			DRM_AUTH | DRM_RENDER_ALLOW),
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_12_1] */
	/* coverity[misra_c_2012_rule_20_7] */
	DRM_IOCTL_DEF_DRV(TCC_GET_EDID, tccdrm_parse_edid_from_crtc_id_ioctl,
			DRM_AUTH | DRM_RENDER_ALLOW),
};

static const struct file_operations tccdrm_driver_fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.mmap		= tcc_drm_gem_mmap,
	.poll		= drm_poll,
	.read		= drm_read,
	.unlocked_ioctl	= drm_ioctl,
	.compat_ioctl = drm_compat_ioctl,
	.release	= drm_release,
};

static struct drm_driver tccdrm_driver = {
	#if defined(CONFIG_REFCODE_PRE_K54)
	.driver_features	= DRIVER_MODESET | DRIVER_GEM | DRIVER_PRIME
	#else
	.driver_features	= DRIVER_MODESET | DRIVER_GEM
	#endif
				  | DRIVER_ATOMIC | DRIVER_RENDER,
	.open			= tccdrm_open,
	.lastclose		= tccdrm_lastclose,
	.postclose		= tccdrm_postclose,
	.gem_free_object_unlocked = tcc_drm_gem_free_object,
	.gem_vm_ops		= &tccdrm_gem_vm_ops,
	.dumb_create		= tcc_drm_gem_dumb_create,
	.prime_handle_to_fd	= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle	= drm_gem_prime_fd_to_handle,
	.gem_prime_export	= tcc_drm_gem_prime_export,
	.gem_prime_import	= tcc_drm_gem_prime_import,
	#if defined(CONFIG_REFCODE_PRE_K54)
	.gem_prime_res_obj	= tcc_gem_prime_res_obj,
	#endif
	.gem_prime_get_sg_table	= tcc_drm_gem_prime_get_sg_table,
	.gem_prime_import_sg_table	= tcc_drm_gem_prime_import_sg_table,
	.gem_prime_vmap		= tcc_drm_gem_prime_vmap,
	.gem_prime_vunmap	= tcc_drm_gem_prime_vunmap,
	.ioctls			= tcc_ioctls,
	/* coverity[misra_c_2012_rule_6_1] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_12_1] */
	.num_ioctls		= ARRAY_SIZE(tcc_ioctls),
	.fops			= &tccdrm_driver_fops,
	/* coverity[cert_str30_c] */
	.name	= DRIVER_NAME,
	/* coverity[cert_str30_c] */
	.desc	= DRIVER_DESC,
	/* coverity[cert_str30_c] */
	.date	= DRIVER_DATE,
	.major	= DRIVER_MAJOR,
	.minor	= DRIVER_MINOR,
	.patchlevel = DRIVER_PATCH,
};

/* coverity[misra_c_2012_rule_8_13] */
static int tccdrm_suspend(struct device *dev)
{
	/* coverity[misra_c_2012_rule_11_5] */
	struct drm_device *drm_dev =
		(struct drm_device *)dev_get_drvdata(dev);
	#if defined(CONFIG_REFCODE_PRE_K54)
	struct tccdrm_private *private;
	bool internal_ok = (bool)true;
	int ret = 0;

	if (drm_dev == NULL) {
		DRM_DEV_INFO(NULL, "[WARN] drm_dev is NULL\r\n");
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		/* coverity[misra_c_2012_rule_11_5] */
		private = (struct tccdrm_private *)drm_dev->dev_private;

		drm_kms_helper_poll_disable(drm_dev);
		tcc_drm_fbdev_suspend(drm_dev);
		private->suspend_state = drm_atomic_helper_suspend(drm_dev);
		/* coverity[misra_c_2012_rule_11_2] */
		if (IS_ERR(private->suspend_state)) {
			tcc_drm_fbdev_resume(drm_dev);
			drm_kms_helper_poll_enable(drm_dev);
			/* coverity[misra_c_2012_rule_10_3] */
			ret = PTR_ERR(private->suspend_state);
		}
	}

	return ret;
	#else
	return drm_mode_config_helper_suspend(drm_dev);
	#endif
}

/* coverity[misra_c_2012_rule_8_13] */
static int tccdrm_resume(struct device *dev)
{
	/* coverity[misra_c_2012_rule_11_5] */
	struct drm_device *drm_dev =
		(struct drm_device *)dev_get_drvdata(dev);
	#if defined(CONFIG_REFCODE_PRE_K54)
	const struct tccdrm_private *private;
	bool internal_ok = (bool)true;

	if (drm_dev == NULL) {
		DRM_DEV_INFO(NULL, "[WARN] drm_dev is NULL\r\n");
		internal_ok = (bool)false;
	}
	if (internal_ok) {
		/* coverity[misra_c_2012_rule_11_5] */
		private = (const struct tccdrm_private *)drm_dev->dev_private;
		if (drm_atomic_helper_resume(drm_dev, private->suspend_state) != 0) {
			DRM_DEV_INFO(dev,
				     "drm_atomic_helper_resume is failed\r\n");
		}
		tcc_drm_fbdev_resume(drm_dev);
		drm_kms_helper_poll_enable(drm_dev);
	}
	return 0;
	#else
	return drm_mode_config_helper_resume(drm_dev);
	#endif
}
static const struct dev_pm_ops tccdrm_pm_ops = {
	/* coverity[misra_c_2012_rule_20_7] */
	SET_SYSTEM_SLEEP_PM_OPS(tccdrm_suspend, tccdrm_resume)
};

/* forward declaration */
static struct platform_driver tccdrm_platform_driver;

/*
 * Connector drivers should not be placed before associated crtc drivers,
 * because connector requires pipe number of its crtc during initialization.
 */

static struct tccdrm_driver_info tccdrm_drivers[] = {
	/* crtcs and planes */
	#if defined(CONFIG_DRM_TELECHIPS_VIOC) || defined(CONFIG_DRM_TELECHIPS_VIOC_MODULE)
	{
		&tccdrm_vioc_driver, (unsigned int)DRM_COMPONENT_DRIVER
	},
	#endif
	#if defined(CONFIG_DRM_TELECHIPS_SCREEN_SHARE) || defined(CONFIG_DRM_TELECHIPS_SCREEN_SHARE_MODULE)
	{
		&tccdrm_ss_driver, (unsigned int)DRM_COMPONENT_DRIVER
	},
	#endif
	#if defined(CONFIG_DRM_TELECHIPS_DP) || defined(CONFIG_DRM_TELECHIPS_DP_MODULE)
	/* encoders and connectors */
	{
		&tccdrm_dp_driver, (unsigned int)DRM_COMPONENT_DRIVER
	},
	#endif
	#if defined(CONFIG_DRM_TELECHIPS_DUMMY_DEV) || defined(CONFIG_DRM_TELECHIPS_DUMMY_DEV_MODULE)
	{
		&tccdrm_dummy_driver, (unsigned int)DRM_COMPONENT_DRIVER
	},
	#endif
	#if defined(CONFIG_DRM_TELECHIPS_HDMI) || defined(CONFIG_DRM_TELECHIPS_HDMI_MODULE)
	{
		&tccdrm_hdmi_driver, (unsigned int)DRM_COMPONENT_DRIVER
	},
	#endif
	#if defined(CONFIG_DRM_TELECHIPS_LVDS) || defined(CONFIG_DRM_TELECHIPS_LVDS_MODULE)
	{
		&tccdrm_lvds_driver, (unsigned int)DRM_COMPONENT_DRIVER
	},
	#endif
	#if defined(CONFIG_DRM_TELECHIPS_DSI) || defined(CONFIG_DRM_TELECHIPS_DSI_MODULE)
	{
		&tccdrm_dsi_driver, (unsigned int)DRM_COMPONENT_DRIVER
	},
	#endif
	{
		&tccdrm_platform_driver, (unsigned int)DRM_SUBSYSTEM_DEVICE
	}
};

/* coverity[misra_c_2012_rule_8_13] */
static int compare_dev(struct device *dev, void *data)
{
	/* coverity[misra_c_2012_rule_11_5] */
	return (dev == (struct device *)data) ? 1 : 0;
}
#if defined(CONFIG_REFCODE_PRE_K54)
static int drm_platform_match(struct device *dev, const void *drv)
{
	/* coverity[cert_exp40_c] */
	/* coverity[misra_c_2012_rule_11_5] */
	/* coverity[misra_c_2012_rule_11_8] */
	return platform_bus_type.match(dev, (struct device_driver *)drv);
}
#endif

static struct component_match *tccdrm_match_add(struct device *dev)
{
	const struct tccdrm_driver_info *info;
	struct component_match *match = NULL;
	bool internal_ok = (bool)true;
	struct device *p, *d;
	unsigned long num_drvs;
	int i, inum_drvs;

	/* coverity[misra_c_2012_rule_6_1] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_12_1] */
	num_drvs = ARRAY_SIZE(tccdrm_drivers);

	if (tcc_math_ulong_gt_intmax(num_drvs)) {
		DRM_DEV_ERROR(dev, "num_drvs is out of range\r\n");
		internal_ok = (bool)false;
	} else {
		inum_drvs = (int)num_drvs;
	}
	if (internal_ok) {
		for (i = 0; i < inum_drvs; ++i) {
			info = (const struct tccdrm_driver_info *)&tccdrm_drivers[i];

			if ((info->driver == NULL) ||
				((info->drv_flags & DRM_COMPONENT_DRIVER) == 0U)) {
				continue;
			}
			p = NULL;
			#if defined(CONFIG_REFCODE_PRE_K54)
			do {
				d = bus_find_device(&platform_bus_type, p,
						&info->driver->driver,
						drm_platform_match);
				put_device(p);
				p = d;

				if (d == NULL) {
					break;
				}
				component_match_add(dev, &match, compare_dev, d);
			} while ((bool)true);
			#else
			do {
				d = platform_find_device_by_driver(p,
								   &info->driver->driver);
				put_device(p);
				p = d;

				if (d == NULL) {
					break;
				}
				component_match_add(dev, &match, compare_dev, d);
			} while ((bool)true);
			#endif
		}
	}

	/* coverity[misra_c_2012_rule_11_2] */
	return (match != NULL) ? match : ERR_PTR(-ENODEV);
}

/*
 * This is &drm_mode_config_helper_funcs.atomic_commit_tail hook,
 * for drivers that support runtime_pm or need the CRTC to be enabled to
 * perform a commit. Otherwise, one should use the default implementation
 * drm_atomic_helper_commit_tail().
 */
static struct drm_mode_config_helper_funcs tccdrm_mode_config_helpers = {
	.atomic_commit_tail = tccdrm_commit_tail,
};

static const struct drm_mode_config_funcs tccdrm_mode_config_funcs = {
	.fb_create = tccdrm_user_fb_create,
	.output_poll_changed = tccdrm_output_poll_changed,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static void tccdrm_mode_config_init(struct drm_device *dev)
{
	(void)drm_mode_config_init(dev);

	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;

	/*
	 * set max width and height as default value(4096x4096).
	 * this value would be used to check framebuffer size limitation
	 * at drm_mode_addfb().
	 */
	dev->mode_config.max_width = 4096;
	dev->mode_config.max_height = 4096;

	dev->mode_config.funcs = &tccdrm_mode_config_funcs;
	dev->mode_config.helper_private = &tccdrm_mode_config_helpers;

	dev->mode_config.allow_fb_modifiers = (bool)false;

	dev->mode_config.fb_base = 0;
	dev->mode_config.async_page_flip = (bool)true;
}


/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 2>
 * HIS metric violation (HIS_CCM)
 *  DR <case 3>
 */
static int tccdrm_bind(struct device *dev)
{
	struct tccdrm_private *private;
	struct drm_device *drm;
	bool need_mode_config_cleanup = (bool)false;
	bool need_cleanup_fbdev = (bool)false;
	bool need_free_drm_dev = (bool)false;
	bool need_free_private = (bool)false;
	bool need_cleanup_poll = (bool)false;
	bool need_unbind_all = (bool)false;
	bool internal_ok = (bool)true;
	int ret = 0;

	drm = drm_dev_alloc(&tccdrm_driver, dev);
	/* coverity[misra_c_2012_rule_11_2] */
	if (IS_ERR(drm)) {
		DRM_DEV_ERROR(dev, "drm_dev_alloc is failed\r\n");
		internal_ok = (bool)false;
		/* coverity[misra_c_2012_rule_10_3] */
		ret = PTR_ERR(drm);
	} else {
		need_free_drm_dev = (bool)true;
	}
	if (internal_ok) {
		/* coverity[misra_c_2012_rule_10_8] */ // K5.4
		/* coverity[misra_c_2012_rule_11_5] */
		private = kzalloc(sizeof(struct tccdrm_private), GFP_KERNEL);
		if (private == NULL) {
			internal_ok = (bool)false;
			ret = -ENOMEM;
		} else {
			need_free_private = (bool)true;
		}
	}
	if (internal_ok) {
		/* coverity[cert_dcl37_c] */
		/* coverity[misra_c_2012_rule_14_4] */
		init_waitqueue_head(&private->wait);
		/* coverity[cert_dcl37_c] */
		/* coverity[misra_c_2012_rule_10_3] */
		/* coverity[misra_c_2012_rule_14_4] */
		/* coverity[misra_c_2012_rule_17_7] */
		spin_lock_init(&private->lock);

		dev_set_drvdata(dev, drm);
		drm->dev_private = (void *)private;
	}
	if (internal_ok) {
		if (dev->dma_parms == NULL) {
			dev->dma_parms = &private->dma_parms;
		}

		/* coverity[misra_c_2012_rule_10_4] */
		/* coverity[misra_c_2012_rule_14_3] */
		(void)dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
		DRM_DEV_INFO(dev,
			     "TCC DRM: using %s device for DMA mapping operations\n",
			     dev_name(dev));
		tccdrm_mode_config_init(drm);
		need_mode_config_cleanup = (bool)true;
	}
	if (internal_ok) {
		/* Try to bind all sub drivers. */
		ret = component_bind_all(drm->dev, drm);
		if (ret != 0) {
			internal_ok = (bool)false;
		} else {
			need_unbind_all = (bool)true;
		}
	}
	if (internal_ok) {
		if (drm->mode_config.num_crtc < 0) {
			internal_ok = (bool)false;
			ret = -ENODEV;
		}
	}
	if (internal_ok) {
		ret = drm_vblank_init(drm, (unsigned int)drm->mode_config.num_crtc);
		if (ret != 0) {
			internal_ok = (bool)false;
		}
	}
	if (internal_ok) {
		drm_mode_config_reset(drm);

		/*
		* enable drm irq mode.
		* - with irq_enabled = (bool)true, we can use the vblank feature.
		*
		* P.S. note that we wouldn't use drm irq handler but
		*	just specific driver own one instead because
		*	drm framework supports only one irq handler.
		*/
		drm->irq_enabled = (bool)true;

		/* init kms poll for handling hpd */
		drm_kms_helper_poll_init(drm);
		need_cleanup_poll = (bool)true;
	}
	if (internal_ok) {
		ret = tccdrm_fbdev_init(drm);
		if (ret != 0) {
			internal_ok = (bool)false;
		} else {
			need_cleanup_fbdev = (bool)true;
		}
	}
	if (internal_ok) {
		/* register the DRM device */
		ret = drm_dev_register(drm, 0);
		if (ret < 0) {
			internal_ok = (bool)false;
		}
	}
	if (!internal_ok) {
		if (need_cleanup_fbdev) {
			tccdrm_fbdev_fini(drm);
		}
		if (need_cleanup_poll) {
			drm_kms_helper_poll_fini(drm);
		}
		if (need_unbind_all) {
			component_unbind_all(drm->dev, drm);
		}
		if (need_mode_config_cleanup) {
			drm_mode_config_cleanup(drm);
		}
		if (need_free_private) {
			kfree(private);
		}
		if (need_free_drm_dev) {
			#if defined(CONFIG_REFCODE_PRE_K54)
			drm_dev_unref(drm);
			#else
			drm_dev_put(drm);
			#endif
		}
	}
	return ret;
}

/*
 * HIS metric violation (HIS_CALLS)
 *  DR <case 2>
 */
static void tccdrm_unbind(struct device *dev)
{
	/* coverity[misra_c_2012_rule_11_5] */ //
	struct drm_device *drm = dev_get_drvdata(dev);

	drm_dev_unregister(drm);

	tccdrm_fbdev_fini(drm);
	drm_kms_helper_poll_fini(drm);

	component_unbind_all(drm->dev, drm);
	drm_mode_config_cleanup(drm);

	if (drm->dev_private != NULL) {
		kfree(drm->dev_private);
		drm->dev_private = NULL;
	}
	dev_set_drvdata(dev, NULL);

	#if defined(CONFIG_REFCODE_PRE_K54)
	drm_dev_unref(drm);
	#else
	drm_dev_put(drm);
	#endif
}

static const struct component_master_ops tcc_drm_ops = {
	.bind		= tccdrm_bind,
	.unbind		= tccdrm_unbind,
};

static int tccdrm_platform_probe(struct platform_device *plat_dev)
{
	struct component_match *match =
			tccdrm_match_add(&plat_dev->dev);
	int ret = 0;

	/* coverity[misra_c_2012_rule_11_2] */
	if (IS_ERR(match)) {
		/* coverity[misra_c_2012_rule_10_3] */
		/* coverity[misra_c_2012_rule_11_2] */
		ret = PTR_ERR(match);
	} else {
		ret = component_master_add_with_match(&plat_dev->dev,
						      &tcc_drm_ops,
						      match);
	}
	return ret;
}

static int tccdrm_platform_remove(struct platform_device *plat_dev)
{
	component_master_del(&plat_dev->dev, &tcc_drm_ops);
	return 0;
}

static struct platform_driver tccdrm_platform_driver = {
	.probe	= tccdrm_platform_probe,
	.remove	= tccdrm_platform_remove,
	.driver	= {
		/* coverity[cert_str30_c] */
		.name	= DRIVER_NAME,
		.pm	= &tccdrm_pm_ops,
	},
};

static void tccdrm_unregister_drivers(void)
{
	const struct tccdrm_driver_info *info;
	bool internal_ok = (bool)true;
	unsigned long num_drvs;
	int i, inum_drvs;

	/* coverity[misra_c_2012_rule_6_1] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_12_1] */
	num_drvs = ARRAY_SIZE(tccdrm_drivers);

	if (tcc_math_ulong_gt_intmax(num_drvs)) {
		internal_ok = (bool)false;
	} else {
		inum_drvs = (int)num_drvs;
	}
	if (internal_ok) {
		for (i = inum_drvs - 1; i >= 0; --i) {
			info = (const struct tccdrm_driver_info *)&tccdrm_drivers[i];
			if (info->driver == NULL) {
				continue;
			}
			platform_driver_unregister(info->driver);
		}
	}
}

static int tccdrm_register_drivers(void)
{
	const struct tccdrm_driver_info *info;
	bool internal_ok = (bool)true;
	int i, inum_drvs, ret = 0;
	unsigned long num_drvs;

	/* coverity[misra_c_2012_rule_6_1] */
	/* coverity[misra_c_2012_rule_10_3] */
	/* coverity[misra_c_2012_rule_10_4] */
	/* coverity[misra_c_2012_rule_12_1] */
	num_drvs = ARRAY_SIZE(tccdrm_drivers);

	if (tcc_math_ulong_gt_intmax(num_drvs)) {
		internal_ok = (bool)false;
		ret = -EINVAL;
	} else {
		inum_drvs = (int)num_drvs;
	}
	if (internal_ok) {

		for (i = 0; i < inum_drvs; ++i) {
			info = (const struct tccdrm_driver_info *)&tccdrm_drivers[i];

			if (info->driver == NULL) {
				continue;
			}
			DRM_DEV_DEBUG(NULL,
				"driver register for %s \r\n",
				info->driver->driver.name);
			ret = platform_driver_register(info->driver);
			if (ret != 0) {
				DRM_DEV_ERROR(NULL,
					"failed to register %s \r\n",
					info->driver->driver.name);
				tccdrm_unregister_drivers();
				break;
			}
		}
	}
	return ret;
}

//telechips,drm-subsystem
static int tccdrm_init(void)
{
	struct platform_device *plat_dev;
	int ret = 0;

	plat_dev = platform_device_register_simple(tccdrm_platform_driver.driver.name,
						   PLATFORM_DEVID_AUTO, NULL, 0);

	/* coverity[misra_c_2012_rule_11_2] */
	if (IS_ERR(plat_dev)) {
		DRM_DEV_ERROR(NULL,
			      "failed to register %s \r\n",
			      tccdrm_platform_driver.driver.name);
		/* coverity[misra_c_2012_rule_10_3] */
		ret = PTR_ERR(plat_dev);
	} else {
		tccdrm_device = plat_dev;

		#if defined(CONFIG_REFCODE_PRE_K510)
		tccdrm_debug_set();
		#endif

		ret = tccdrm_register_drivers();
	}
	return ret;
}

static void tccdrm_exit(void)
{
	tccdrm_unregister_drivers();
	if (tccdrm_device != NULL) {
		platform_device_unregister(tccdrm_device);
	}
}
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
module_init(tccdrm_init);
/* coverity[misra_c_2012_rule_20_7] */
/* coverity[misra_c_2012_rule_21_2] */
/* coverity[cert_dcl37_c] */
module_exit(tccdrm_exit);

/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_5_9] */
/* coverity[misra_c_2012_rule_21_2] */ // K5.4
MODULE_DESCRIPTION("Telechips SoC DRM Driver");
/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_5_9] */
/* coverity[misra_c_2012_rule_21_2] */ // K5.4
MODULE_LICENSE("GPL");

/* coverity[cert_dcl37_c] */
/* coverity[misra_c_2012_rule_5_9] */ //K5.4
/* coverity[misra_c_2012_rule_21_2] */ //K5.4
MODULE_VERSION(__stringify(DRIVER_MAJOR) "."
               __stringify(DRIVER_MINOR) "."
               __stringify(DRIVER_PATCH));

