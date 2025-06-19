// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  Copyright (C) Telechips Inc.
 */

#include <linux/bitops.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/watchdog.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/arm-smccc.h>
#include <soc/telechips/smc.h>
#include <soc/telechips/chipinfo.h>

#define WDT_ENABLE	1UL
#define WDT_DISABLE	0UL

enum {
	CBUS_WDT_EN,
	CBUS_WDT_CLEAR,
	CBUS_WDT_IRQCNT,
	CBUS_WDT_RSTCNT,
};

struct tcc_watchdog_device {
	struct watchdog_device	wdd;
	struct platform_device	*pdev;

	unsigned int	*wdt_base;
	unsigned int	*kick_wdt_base;

	struct clk 	*wdt_clk;

	unsigned int	is_reset_enable;
	unsigned int	reset_enable_watchdog;
};

static int tcc_cb_wdt_ping(struct watchdog_device *wdd);
static int tcc_cb_wdt_start(struct watchdog_device *wdd);
static int tcc_cb_wdt_stop(struct watchdog_device *wdd);


static void tcc_cb_wdt_enable_timer(struct watchdog_device *wdd)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	const struct tcc_watchdog_device *cb_wdt = watchdog_get_drvdata(wdd);

	if (wdd->pretimeout > 0U) {
		writel(0x1, &cb_wdt->kick_wdt_base[CBUS_WDT_EN]);
	}
	writel(0x1, &cb_wdt->wdt_base[CBUS_WDT_EN]);

	set_bit(WDOG_HW_RUNNING, &wdd->status);
}

static void tcc_cb_wdt_disable_timer(struct watchdog_device *wdd)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	const struct tcc_watchdog_device *cb_wdt = watchdog_get_drvdata(wdd);

	writel(0x0, &cb_wdt->kick_wdt_base[CBUS_WDT_EN]);
	writel(0x0, &cb_wdt->wdt_base[CBUS_WDT_EN]);

	clear_bit(WDOG_HW_RUNNING, &wdd->status);
}

static void tcc_cb_wdt_reset_ctrl(const struct tcc_watchdog_device *cb_wdt,
		unsigned long wdt_en)
{
	struct arm_smccc_res res = {0};

	if (cb_wdt->is_reset_enable != 0U) {
		arm_smccc_smc(SIP_WATCHDOG_RESET_CTRL,
				wdt_en,
				cb_wdt->reset_enable_watchdog,
				0, 0, 0, 0, 0, &res);
	}
}

static void tcc_cb_wdt_check_and_set_pretimeout(struct watchdog_device *wdd,
		unsigned int pretimeout)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	const struct tcc_watchdog_device *cb_wdt = watchdog_get_drvdata(wdd);
	uint32_t kick_cnt = 0;
	unsigned long ul_clk_rate = clk_get_rate(cb_wdt->wdt_clk);
	unsigned int clk_rate;

	if ((ul_clk_rate > 0UL) && (ul_clk_rate <= UINT_MAX)) {
		clk_rate = (unsigned int)ul_clk_rate;
		/*
		 * Show warning about invalid pretimeout in watchdog probe.
		 * WDIOC_SETPRETIMEOUT ioctl can not reach hear if new pretimeout value
		 * is invalid.
		 */
		if (watchdog_pretimeout_invalid(wdd, pretimeout)) {

			dev_warn(&cb_wdt->pdev->dev,
					"[%s] pretimeout [%u] is invalid.\n",
					__func__, pretimeout);
		}

		if (check_mul_overflow(pretimeout, clk_rate, &kick_cnt) != (bool)0) {

			dev_warn(&cb_wdt->pdev->dev,
					"[%s] pretimeout [%u] is too big. Reduce pretimeout to maximum. [%u]\n",
					__func__, pretimeout, (UINT_MAX/clk_rate));

			pretimeout = UINT_MAX / clk_rate;
			kick_cnt = pretimeout * clk_rate;
		}

		writel(kick_cnt, &cb_wdt->kick_wdt_base[CBUS_WDT_IRQCNT]);
		// Set WDT Reset IRQ CNT

		wdd->pretimeout = pretimeout;

		if (wdd->pretimeout > wdd->timeout) {
			dev_warn(&cb_wdt->pdev->dev,
					"[%s] pretimeout value is bigger than timeout\n",
					__func__);
		}
	}
}

static int tcc_cb_wdt_set_pretimeout(struct watchdog_device *wdd,
		unsigned int pretimeout)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	const struct tcc_watchdog_device *cb_wdt = watchdog_get_drvdata(wdd);
	bool is_active = watchdog_hw_running(wdd);
	int ret = 0;

	if (is_active) {
		tcc_cb_wdt_disable_timer(wdd);
	}

	if (pretimeout > 0U) {
		tcc_cb_wdt_check_and_set_pretimeout(wdd, pretimeout);
	} else {
		ret = -EINVAL;
	}

	if (is_active) {
		tcc_cb_wdt_enable_timer(wdd);
	}

	if (ret == 0) {
		/* If someone call SETPRETIMEOUT ioctl repeatly faster than
		 * pretimeout interval. Watchdog reset will occur accidently.
		 * (kick interrupt never occur..)
		 * To prevent this situation, call ping at here.
		 */
		(void)tcc_cb_wdt_ping(wdd);
		dev_info(&cb_wdt->pdev->dev, "[%s] pretimeout: %u sec\n",
				__func__, wdd->pretimeout);
	}
	return ret;
}

static void tcc_cb_wdt_check_and_set_timeout(struct watchdog_device *wdd,
		unsigned int timeout)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	const struct tcc_watchdog_device *cb_wdt = watchdog_get_drvdata(wdd);
	uint32_t reset_cnt = 0;
	unsigned long ul_clk_rate = clk_get_rate(cb_wdt->wdt_clk);
	unsigned int clk_rate = (ul_clk_rate > UINT_MAX) ?
		UINT_MAX : (unsigned int)ul_clk_rate;

	/*
	 * Show warning about invalid timeout in watchdog probe.
	 * WDIOC_SETTIMEOUT ioctl can not reach hear if new timeout value
	 * is invalid.
	 */
	if (watchdog_timeout_invalid(wdd, timeout)) {
		dev_warn(&cb_wdt->pdev->dev,
				"[%s] timeout [%u] is invalid\n",
				__func__, timeout);
	}

	if (check_mul_overflow(timeout, clk_rate, &reset_cnt) != (bool)0) {
		dev_warn(&cb_wdt->pdev->dev,
			"[%s] timeout [%u] is too big. Reduce timeout to maximum. [%u]\n",
			__func__, timeout, wdd->max_timeout);

		timeout = wdd->max_timeout;
		/* [DR]
		 * reset_cnt will not warp.
		 * wdd->max_timeout was caculated by (UINT_MAX / clk_rate)
		 */
		reset_cnt = timeout * clk_rate;
	}

	// Set WDT Reset IRQ CNT
	writel(reset_cnt, &cb_wdt->wdt_base[CBUS_WDT_IRQCNT]);
	// If reset counter set 0,
	// Reset signal immediatly generated after watchdog enable.
	// Set RST CNT to 1.
	writel(0x1, &cb_wdt->wdt_base[CBUS_WDT_RSTCNT]);

	wdd->timeout = timeout;
}

static int tcc_cb_wdt_set_timeout(struct watchdog_device *wdd,
		unsigned int timeout)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	const struct tcc_watchdog_device *cb_wdt = watchdog_get_drvdata(wdd);
	bool is_active = watchdog_hw_running(wdd);
	int ret = 0;

	if (is_active) {
		tcc_cb_wdt_disable_timer(wdd);
	}

	tcc_cb_wdt_check_and_set_timeout(wdd, timeout);

	if (is_active) {
		tcc_cb_wdt_enable_timer(wdd);
	}

	dev_info(&cb_wdt->pdev->dev,
			"[%s] timeout: %u sec\n", __func__, wdd->timeout);

	return ret;
}

static int tcc_cb_wdt_start(struct watchdog_device *wdd)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	const struct tcc_watchdog_device *cb_wdt = watchdog_get_drvdata(wdd);

	(void)tcc_cb_wdt_ping(wdd);

	tcc_cb_wdt_enable_timer(wdd);

	tcc_cb_wdt_reset_ctrl(cb_wdt, WDT_ENABLE);

	return 0;
}

static int tcc_cb_wdt_stop(struct watchdog_device *wdd)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	const struct tcc_watchdog_device *cb_wdt = watchdog_get_drvdata(wdd);

	(void)tcc_cb_wdt_ping(wdd);

	tcc_cb_wdt_disable_timer(wdd);

	tcc_cb_wdt_reset_ctrl(cb_wdt, WDT_DISABLE);

	return 0;
}

static int tcc_cb_wdt_ping(struct watchdog_device *wdd)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	const struct tcc_watchdog_device *cb_wdt = watchdog_get_drvdata(wdd);

	/* [DR]
	 * Kernel API dev_dbg has defects it's inside.
	 */
	dev_dbg(&cb_wdt->pdev->dev, "%s\n", __func__);
	writel(0x1, &cb_wdt->wdt_base[CBUS_WDT_CLEAR]);
	writel(0x1, &cb_wdt->kick_wdt_base[CBUS_WDT_CLEAR]);

	return 0;
}

/* [DR]
 * tcc_cb_wdt_get_status is call back of tcc_cb_wdt_ops.status
 * watchdog core driver declare the status function as
 * ‘unsigned int (*status)(struct watchdog_device *);’
 */
static unsigned int tcc_cb_wdt_get_status(struct watchdog_device *wdd)
{
	const unsigned long status = wdd->status;
	unsigned int ret;

	if (test_bit(WDOG_HW_RUNNING, &status)) {
		ret = 1U;
	} else {
		ret = 0U;
	}

	return ret;
}

static const struct watchdog_info tcc_cb_wdt_info = {
	.options = (	WDIOF_SETTIMEOUT |
			WDIOF_PRETIMEOUT |
			WDIOF_KEEPALIVEPING |
			WDIOF_MAGICCLOSE ),
	.identity = "tcc-cb-wdt",
};

static const struct watchdog_info tcc_cb_wdt_info_no_irq = {
	.options = (	WDIOF_SETTIMEOUT |
			WDIOF_KEEPALIVEPING |
			WDIOF_MAGICCLOSE ),
	.identity = "tcc-cb-wdt",
};

static irqreturn_t tcc_cb_wdt_kick(int irq, void *dev_id)
{
	/* [DR]
	 * void *dev_id must covert to struct device *.
	 */
	struct watchdog_device* wdd = dev_id;

	(void)irq;

	(void)tcc_cb_wdt_ping(wdd);


	return IRQ_HANDLED;
}

static const struct watchdog_ops tcc_cb_wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= tcc_cb_wdt_start,
	.stop		= tcc_cb_wdt_stop,
	.ping		= tcc_cb_wdt_ping,
	.set_timeout	= tcc_cb_wdt_set_timeout,
	.set_pretimeout	= tcc_cb_wdt_set_pretimeout,
	.status		= tcc_cb_wdt_get_status,
	.ioctl		= NULL,

};

static int tcc_cb_wdt_ioremap(struct tcc_watchdog_device *cb_wdt,
		const struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;


	/* [DR] */
	cb_wdt->kick_wdt_base = (unsigned int *)of_iomap(np, 0);
	cb_wdt->wdt_base = (unsigned int *)of_iomap(np, 1);

	if ((cb_wdt->kick_wdt_base == NULL) || (cb_wdt->wdt_base == NULL)) {
		ret = -EFAULT;
	} else {
		/* Get which cpu bus wathcdog reset enable */
		if (of_property_read_u32(np, "reset-enable-watchdog",
					&cb_wdt->reset_enable_watchdog) == 0) {
			cb_wdt->is_reset_enable = 1U;
		}
	}

	return ret;
}

static int tcc_cb_wdt_init_clock(struct tcc_watchdog_device *cb_wdt,
		struct platform_device *pdev)
{
	int ret = 0;

	cb_wdt->wdt_clk = devm_clk_get(&pdev->dev, "clk_wdt");

	if (IS_ERR(cb_wdt->wdt_clk)) {
		ret = -ENODEV;
	}

	if (ret == 0) {
		ret = clk_prepare_enable(cb_wdt->wdt_clk);
		dev_info(&pdev->dev, "[%s] watchdog clock rate: %lu\n",
				__func__, clk_get_rate(cb_wdt->wdt_clk));
	}

	return ret;
}

static int tcc_cb_wdt_read_of_property_kick_timer(struct tcc_watchdog_device *cb_wdt,
		const struct platform_device* pdev)
{
	const struct device_node* np = pdev->dev.of_node;
	struct watchdog_device *wdd = &cb_wdt->wdd;
	int ret = 0;

	if (of_property_read_u32(np, "kick-interval", &wdd->pretimeout) != 0) {
		/* Set pretimeout to 0 */
		cb_wdt->wdd.pretimeout = 0U;
		cb_wdt->wdd.info = &tcc_cb_wdt_info_no_irq;
		writel(0U, &cb_wdt->kick_wdt_base[CBUS_WDT_IRQCNT]);
		ret = -ENOENT;
	}

	return ret;
}

static int tcc_cb_wdt_init_kick_timer(struct tcc_watchdog_device *cb_wdt,
		struct platform_device *pdev)
{
	int ret = 0;
	unsigned int kick_irq;

	ret = tcc_cb_wdt_read_of_property_kick_timer(cb_wdt, pdev);

	if (ret == 0) {
		ret = platform_get_irq(pdev, 0);
		if (ret >= 0) {
			kick_irq = (unsigned int)ret;

			ret = request_irq(kick_irq, tcc_cb_wdt_kick, IRQF_SHARED,
					"tcc_cb_wdt_kick", &cb_wdt->wdd);
			if (ret != 0) {
				dev_err(&pdev->dev, "[%s] failed to request kick_irq\n",
						__func__);
			}
		}
		if (ret == 0) {
			ret = tcc_cb_wdt_set_pretimeout(&cb_wdt->wdd,
					cb_wdt->wdd.pretimeout);
		}
	} else {
		/* kick-interval property can be erased if user wants.
		 * Do not handle this as error. */
		dev_info(&pdev->dev,
			"[%s] Cannot find kick-interval property in device tree, watchdog kick irq handler will not registered.\n",
			__func__);
		ret = 0;
	}

	return ret;
}

static int tcc_cb_wdt_init_wdt_timer(struct tcc_watchdog_device *cb_wdt,
		struct platform_device *pdev)
{
	int ret = 0;
	unsigned long clk_rate = clk_get_rate(cb_wdt->wdt_clk);

	if ((clk_rate > 0UL) && (clk_rate <= UINT_MAX)) {
		cb_wdt->wdd.min_timeout = 1;
		cb_wdt->wdd.max_timeout = UINT_MAX / (unsigned int)clk_rate;
		cb_wdt->wdd.timeout = cb_wdt->wdd.max_timeout;
		/* If timeout-sec is exist in device tree,
		 * wdd.timeout will be overwrite by watchdog_init_timeout(). */
		(void)watchdog_init_timeout(&cb_wdt->wdd, 0, &pdev->dev);

		if (cb_wdt->wdd.timeout > 0U) {
			ret = tcc_cb_wdt_set_timeout(&cb_wdt->wdd,
					cb_wdt->wdd.timeout);
		}
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static int tcc_cb_wdt_init_timer(struct tcc_watchdog_device* cb_wdt,
		struct platform_device *pdev)
{
	int ret = 0;

	cb_wdt->wdd.info = &tcc_cb_wdt_info;
	cb_wdt->wdd.ops = &tcc_cb_wdt_ops;

	ret = tcc_cb_wdt_init_wdt_timer(cb_wdt, pdev);

	if (ret == 0) {
		ret = tcc_cb_wdt_init_kick_timer(cb_wdt, pdev);
	}

	return ret;
}

static int tcc_cb_wdt_init(struct tcc_watchdog_device *cb_wdt,
		struct platform_device *pdev)
{
	int ret;

	ret = tcc_cb_wdt_ioremap(cb_wdt, pdev);

	if (ret == 0) {
		ret = tcc_cb_wdt_init_clock(cb_wdt, pdev);
	}

	if (ret == 0) {
		ret = tcc_cb_wdt_init_timer(cb_wdt, pdev);
	}

	return ret;
}

static int tcc_cb_wdt_register(struct tcc_watchdog_device *cb_wdt,
		struct platform_device *pdev)
{
	int ret;
	struct watchdog_device *wdd;

	wdd = &cb_wdt->wdd;

	ret = devm_watchdog_register_device(&pdev->dev, wdd);
	if (ret != 0) {
		dev_err(&pdev->dev,
				"[%s] failed to register wdt device\n", __func__);
	}

	watchdog_stop_on_reboot(wdd);
	watchdog_stop_on_unregister(wdd);

	return ret;
}

#if defined (CONFIG_ARCH_TCC805X)
#define TFA_V_MAJOR	0
#define TFA_V_MINOR	1
#define TFA_V_PATCH	64
#else
#define TFA_V_MAJOR	0
#define TFA_V_MINOR	0
#define TFA_V_PATCH	0
#endif

static int tcc_cb_wdt_check_tf_a_version(const struct platform_device *pdev)
{
	struct version_info v_info;
	int ret = 0;

	get_tf_version(&v_info);

	if (!version_compat(&v_info, TFA_V_MAJOR, TFA_V_MINOR, TFA_V_PATCH)) {
		dev_err(&pdev->dev,
			"Too old TF-A ROM. (required: v%d.%d.%d | current v%d.%d.%d)\n",
			TFA_V_MAJOR, TFA_V_MINOR, TFA_V_PATCH,
			v_info.major, v_info.minor, v_info.patch);
		ret = -EINVAL;
	}

	return ret;
}

static int tcc_cb_wdt_probe(struct platform_device *pdev)
{
	struct tcc_watchdog_device *cb_wdt;
	int ret = 0;

	/* Check TF-A version before probe start. */
	ret = tcc_cb_wdt_check_tf_a_version(pdev);

	if (ret == 0) {
		/* [DR]
		 * Function family of kmalloc usually need to cast void* to others.
		 */
		cb_wdt = devm_kzalloc(&pdev->dev,
				sizeof(struct tcc_watchdog_device), GFP_KERNEL);

		if (cb_wdt == NULL) {
			ret = -ENOMEM;
		} else {
			cb_wdt->pdev = pdev;
			platform_set_drvdata(pdev, cb_wdt);
			watchdog_set_drvdata(&cb_wdt->wdd, cb_wdt);
		}

		if (ret == 0) {
			ret = tcc_cb_wdt_init(cb_wdt, pdev);
		}

		if (ret == 0) {
			ret = tcc_cb_wdt_register(cb_wdt, pdev);
		}

		if (ret == 0) {
			ret = tcc_cb_wdt_start(&cb_wdt->wdd);
		}
	}

	return ret;
}

/* [DR]
 * tcc_cb_wdt_remove is call back of tcc_cb_wdt_driver.remove
 * platform driver declare remove function as
 * ‘int (*)(struct platform_device *)’
 */
static int tcc_cb_wdt_remove(struct platform_device *pdev)
{
	struct tcc_watchdog_device *cb_wdt = platform_get_drvdata(pdev);
	int ret = 0;


	if (cb_wdt == NULL) {
		dev_err(&pdev->dev, "%s: cb_wdt is NULL", __func__);
		ret = -ENODEV;
	} else {
		if (watchdog_hw_running(&cb_wdt->wdd)) {
			/* if device is active,*/
			(void)tcc_cb_wdt_stop(&cb_wdt->wdd);
		}

		dev_info(&pdev->dev, "cbus wathcdog driver remove.");
		watchdog_unregister_device(&cb_wdt->wdd);
	}

	return ret;
}

/* [DR]
 * tcc_cb_wdt_shutdown is call back of tcc_cb_wdt_driver.shutdown
 * platform driver declare shutdown function as
 * ‘void (*)(struct platform_device *)’
 */
static void tcc_cb_wdt_shutdown(struct platform_device *pdev)
{
	struct tcc_watchdog_device *cb_wdt = platform_get_drvdata(pdev);

	if (cb_wdt == NULL) {
		dev_err(&pdev->dev, "[%s]Cannot find watchdog device.", __func__);
	} else {
		if (watchdog_hw_running(&cb_wdt->wdd)) {
			/* if device is active,*/
			(void)tcc_cb_wdt_stop(&cb_wdt->wdd);
		}
		dev_info(&pdev->dev, "cbus watchdog driver shutdown.");
	}
}

#ifdef CONFIG_PM
/* [DR]
 * tcc_cb_wdt_pm_suspend is call back of tcc_cb_wdt_pm_ops.suspend
 * pm driver declare suspend function as
 * ‘int (*)(struct device *)’
 */
static int tcc_cb_wdt_pm_suspend(struct device *dev)
{
	struct tcc_watchdog_device *cb_wdt = dev_get_drvdata(dev);
	int ret = 0;

	if (cb_wdt == NULL) {
		dev_err(dev, "%s: cb_wdt is NULL", __func__);
		ret = -ENODEV;
	} else {
		(void)tcc_cb_wdt_stop(&cb_wdt->wdd);
		dev_info(dev, "%s\n", __func__);
	}

	return ret;
}

/* [DR]
 * tcc_cb_wdt_pm_resume is call back of tcc_cb_wdt_pm_ops.resume
 * pm driver declare resume function as
 * ‘int (*)(struct platform_device *, pm_message_t)’
 */
static int tcc_cb_wdt_pm_resume(struct device *dev)
{
	struct tcc_watchdog_device *cb_wdt = dev_get_drvdata(dev);
	int ret = 0;

	if (cb_wdt == NULL) {
		dev_err(dev, "%s: cb_wdt is NULL", __func__);
		ret = -ENODEV;
	} else {
		(void)tcc_cb_wdt_set_timeout(
				&cb_wdt->wdd, cb_wdt->wdd.timeout);
		(void)tcc_cb_wdt_set_pretimeout(
				&cb_wdt->wdd, cb_wdt->wdd.pretimeout);
		(void)tcc_cb_wdt_start(&cb_wdt->wdd);
		dev_info(dev, "%s\n", __func__);
	}

	return ret;
}
#else
#define tcc_cb_wdt_pm_suspend  NULL
#define tcc_cb_wdt_pm_resume   NULL
#endif

static const struct of_device_id tcc_cb_wdt_of_match[] = {
	{.compatible = "telechips,tcc-cb-wdt",},
	{},
};

MODULE_DEVICE_TABLE(of, tcc_cb_wdt_of_match);

static const struct dev_pm_ops tcc_cb_wdt_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(tcc_cb_wdt_pm_suspend, tcc_cb_wdt_pm_resume)
};

static struct platform_driver tcc_cb_wdt_driver = {
	.probe		= tcc_cb_wdt_probe,
	.remove		= tcc_cb_wdt_remove,
	.shutdown	= tcc_cb_wdt_shutdown,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "tcc-cb-wdt",
		.pm	= &tcc_cb_wdt_pm_ops,
		.of_match_table = tcc_cb_wdt_of_match,
	},
};

/* [DR]
 * Kernel API module_platform_driver has defects it's inside.
 */
module_platform_driver(tcc_cb_wdt_driver);

/* [DR]
 * Kernel API MODULE_xxxx  has defects it's inside.
 */
MODULE_AUTHOR("Telechips Corporation");
MODULE_DESCRIPTION("Telechips CBUS Watchdog Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:tcc-cbus-wdt");
