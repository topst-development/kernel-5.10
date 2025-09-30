// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/of.h>

/* initcall functions of each VIOC low-api files */
extern int vioc_ddicfg_init(void);
extern int vioc_intr_init(void);
extern int vioc_config_init(void);
extern int vioc_rdma_init(void);
extern int vioc_wdma_init(void);
extern int vioc_wmixer_init(void);
extern int vioc_disp_init(void);
extern int vioc_sc_init(void);
extern int vioc_lut_init(void);
extern int vioc_fifo_init(void);
extern int vioc_timer_init(void);
extern int vioc_viqe_init(void);
extern int gre2d_init(void);
extern int vioc_mc_init(void);
extern int vioc_afbc_dec_init(void);
extern int vioc_lut_3d_init(void);

static const struct of_device_id vioc_api_of_match[] = {
	{ .compatible = "telechips,vioc_api" },
	{}
};
MODULE_DEVICE_TABLE(of, vioc_api_of_match);

static struct platform_driver vioc_api_driver = {
	.probe = NULL,
	.remove = NULL,
	.driver = {
		.name = "vioc_api",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(vioc_api_of_match),
	},
};

static void __exit vioc_exit(void)
{
	platform_driver_unregister(&vioc_api_driver);
}

static int __init vioc_init(void)
{
	vioc_ddicfg_init();	// 1st. postcall_initcall() for VIOC_REMAP
	vioc_config_init();	// 2nd.
	vioc_intr_init();	// 3rd.

	vioc_rdma_init();
	vioc_wdma_init();
	vioc_wmixer_init();
	vioc_disp_init();
	vioc_sc_init();
	vioc_lut_init();
	vioc_fifo_init();
	vioc_timer_init();

#ifdef CONFIG_VIOC_DEINTERLACER
	vioc_viqe_init();
#endif
#ifdef CONFIG_VIOC_G2D
	gre2d_init();
#endif
#ifdef CONFIG_VIOC_MAP_DECOMP
	vioc_mc_init();
#endif
#ifdef CONFIG_VIOC_AFBCDEC
	vioc_afbc_dec_init();
#endif
	vioc_lut_3d_init();

	pr_info("%s done\n", __func__);

	return platform_driver_register(&vioc_api_driver);
}

postcore_initcall(vioc_init);
module_exit(vioc_exit);
MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("TCC807x VIOC API driver");
MODULE_LICENSE("GPL");
