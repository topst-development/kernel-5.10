// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Telechips Inc.
 */

#define pr_fmt(fmt) "reboot mode: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/reboot-mode.h>
#include <soc/telechips/smc.h>

static void __iomem *reboot_mode_reg;

/* For legacy platforms with no SiP service provided */
static inline int telechips_reboot_mode_of_iomap(const struct device * dev)
{
#if !defined(CONFIG_HAVE_TELECHIPS_SIP_SERVICE)
	reboot_mode_reg = of_iomap(dev->of_node, 0);

	return (reboot_mode_reg == NULL) ? -EINVAL : 0;
#else
	reboot_mode_reg = NULL;

	return 0;
#endif
}

/* For legacy platforms with no SiP service provided */
static inline void telechips_reboot_mode_of_iounmap(void)
{
#if !defined(CONFIG_HAVE_TELECHIPS_SIP_SERVICE)
	iounmap(reboot_mode_reg);
#endif
	reboot_mode_reg = NULL;
}

static int telechips_reboot_mode_write(struct reboot_mode_driver *reboot,
				       unsigned int magic)
{
#if defined(CONFIG_HAVE_TELECHIPS_SIP_SERVICE)
	struct arm_smccc_res res;

	arm_smccc_smc(SIP_SET_RESET_REASON, magic, 0, 0, 0, 0, 0, 0, &res);
#else
	writel(magic, reboot_mode_reg);
#endif
	(void)pr_info("Set magic 0x%x\n", magic);

	return 0;
}

static struct reboot_mode_driver reboot_mode = {
	.write = telechips_reboot_mode_write,
};

static u32 panic_magic;

static int telechips_reboot_mode_panic_notifier(struct notifier_block *nb,
						unsigned long action,
						void *data)
{
	s32 ret = telechips_reboot_mode_write(NULL, panic_magic);

	return (ret == 0) ? NOTIFY_OK : NOTIFY_DONE;
}

static struct notifier_block telechips_reboot_mode_panic_notifier_block = {
	.notifier_call = &telechips_reboot_mode_panic_notifier,
};

static int telechips_reboot_mode_init_panic_notifier(const struct device *dev)
{
	const struct device_node *np = dev->of_node;
	struct notifier_block *nb;
	s32 ret;

	ret = of_property_read_u32(np, "record-panic", &panic_magic);
	if (ret == -EINVAL) {
		(void)pr_info("No record-panic property given\n");
		ret = 0;
	} else if (ret != 0) {
		(void)pr_err("Failed to get record-panic (err: %d)\n", ret);
	} else {
		nb = &telechips_reboot_mode_panic_notifier_block;
		ret = atomic_notifier_chain_register(&panic_notifier_list, nb);
	}

	return ret;
}

static int telechips_reboot_mode_remove_panic_notifier(void)
{
	struct notifier_block *nb;

	nb = &telechips_reboot_mode_panic_notifier_block;

	return atomic_notifier_chain_unregister(&panic_notifier_list, nb);
}

static void telechips_reboot_mode_record_init(const struct device *dev)
{
	const struct device_node *np = dev->of_node;
	s32 ret;
	u32 init_magic;

	ret = of_property_read_u32(np, "record-init", &init_magic);
	if (ret == -EINVAL) {
		(void)pr_info("No record-init property given\n");
	} else if (ret != 0) {
		(void)pr_err("Failed to get record-init (err: %d)\n", ret);
	} else {
		(void)telechips_reboot_mode_write(&reboot_mode, init_magic);
	}
}

static int telechips_reboot_mode_init(struct device *dev)
{
	s32 ret;

	ret = telechips_reboot_mode_init_panic_notifier(dev);
	if (ret != 0) {
		(void)pr_err("Failed to register panic notifier (err: %d)\n",
				ret);
		devm_reboot_mode_unregister(dev, &reboot_mode);
		telechips_reboot_mode_of_iounmap();
	} else {
		telechips_reboot_mode_record_init(dev);
	}

	return ret;
}

static int telechips_reboot_mode_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	s32 ret;
	const char *fail = NULL;

	ret = telechips_reboot_mode_of_iomap(dev);
	if (ret != 0) {
		fail = "Failed to iomap reg";
	} else {
		reboot_mode.dev = dev;

		/* Register reboot mode driver */
		ret = devm_reboot_mode_register(dev, &reboot_mode);
		if (ret != 0) {
			fail = "Failed to register reboot mode";
			telechips_reboot_mode_of_iounmap();
		} else {
			/*
			 * Register reboot mode panic handler.
			 * If record-init property exists, set initial
			 * boot mode magic of record-init property.
			 */
			ret = telechips_reboot_mode_init(dev);
		}
	}

	if (fail != NULL) {
		(void)pr_err("%s (err: %d)\n", fail, ret);
	}

	return ret;
}

static int telechips_reboot_mode_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	s32 ret;

	ret = telechips_reboot_mode_remove_panic_notifier();
	devm_reboot_mode_unregister(dev, &reboot_mode);
	telechips_reboot_mode_of_iounmap();

	return ret;
}

static const struct of_device_id telechips_reboot_mode_match[2] = {
	{ .compatible = "telechips,reboot-mode" },
	{ .compatible = "" }
};

MODULE_DEVICE_TABLE(of, telechips_reboot_mode_match);

static struct platform_driver telechips_reboot_mode_driver = {
	.probe = telechips_reboot_mode_probe,
	.remove = telechips_reboot_mode_remove,
	.driver = {
		.name = "telechips-reboot-mode",
		.of_match_table = of_match_ptr(telechips_reboot_mode_match),
	},
};

module_platform_driver(telechips_reboot_mode_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jigi Kim <jigi.kim@telechips.com>");
MODULE_DESCRIPTION("Telechips reboot mode driver");
