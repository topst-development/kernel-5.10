/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <video/tcc-svdw.h>

#include "tcc-svdw.h"

static int tcc_svdw_probe(struct platform_device *p_pdev)
{
	const struct device *p_dev = NULL;
	struct tcc_svdw_device *p_svdw = NULL;
	int ret = 0;

	p_dev = &p_pdev->dev;

	p_svdw = kzalloc(sizeof(*p_svdw), GFP_KERNEL);
	if (p_svdw == NULL) {
		loge(p_dev, "Failed to allocate device instance.\n");
		ret = -ENOMEM;
	} else {
		platform_set_drvdata(p_pdev, p_svdw);
		tcc_svdw_init(p_svdw, p_pdev);
	}

	return ret;
}

static int tcc_svdw_remove(struct platform_device *pdev)
{
	struct tcc_svdw_device *p_svdw = NULL;
	p_svdw = (struct tcc_svdw_device *)platform_get_drvdata(pdev);

	tcc_svdw_unregister_video_device(p_svdw);
	tcc_svdw_free_irq(p_svdw);
	return 0;
}

const static struct of_device_id tcc_svdw_of_match[] = {
	{ .compatible = "telechips,tcc_svdw" },
	{}
};

MODULE_DEVICE_TABLE(of, tcc_svdw_of_match);

static struct platform_driver tcc_svdw_driver = {
	.probe		= tcc_svdw_probe,
	.remove		= tcc_svdw_remove,
	.driver		= {
		.name		= KBUILD_MODNAME,
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(tcc_svdw_of_match),
	},
};

module_platform_driver(tcc_svdw_driver);

MODULE_SOFTDEP("pre: tcc-dewarp-odw");
MODULE_AUTHOR("Telechips.Co.Ltd");
MODULE_DESCRIPTION("Telechips SVDW Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
