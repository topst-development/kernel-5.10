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

#define WDT_ENABLE    1UL
#define WDT_DISABLE   0UL

#define WWDT_WR_PW  0x5AFEACE5

enum {
	CBUS_WWDT_EN,
	CBUS_WWDT_CLEAR,
	CBUS_WWDT_IRQCNT,
	CBUS_WWDT_RSTCNT,
	CBUS_WWDT_SM_MODE,
	CBUS_WWDT_WR_PW,
};

struct tcc_windowed_watchdog_device {
	struct watchdog_device	wdd;
	struct platform_device	*pdev;

	unsigned int	*wwdt_base;

	struct clk 	*wwdt_clk;

	unsigned int	is_reset_enable;
	unsigned int	reset_enable_watchdog;
};

static int tcc_cb_wwdt_ping(struct watchdog_device *wdd);
static int tcc_cb_wwdt_start(struct watchdog_device *wdd);
static int tcc_cb_wwdt_stop(struct watchdog_device *wdd);

static void tcc_cb_wwdt_enable_timer(struct watchdog_device *wdd)
{
	const struct tcc_windowed_watchdog_device *cb_wwdt = watchdog_get_drvdata(wdd);

    writel(WWDT_WR_PW, &cb_wwdt->wwdt_base[CBUS_WWDT_WR_PW]);
	writel(0x1, &cb_wwdt->wwdt_base[CBUS_WWDT_EN]);

	set_bit(WDOG_HW_RUNNING, &wdd->status);
}

static void tcc_cb_wwdt_disable_timer(struct watchdog_device *wdd)
{
	const struct tcc_windowed_watchdog_device *cb_wwdt = watchdog_get_drvdata(wdd);

    writel(WWDT_WR_PW, &cb_wwdt->wwdt_base[CBUS_WWDT_WR_PW]);
	writel(0x0, &cb_wwdt->wwdt_base[CBUS_WWDT_EN]);

	clear_bit(WDOG_HW_RUNNING, &wdd->status);
}

static void tcc_cb_wwdt_reset_ctrl(const struct tcc_windowed_watchdog_device *cb_wwdt,
		unsigned long wdt_en)
{
	struct arm_smccc_res res = {0};

	if (cb_wwdt->is_reset_enable != 0U) {
		pr_err("KSJ 3-4:%s:%d\n", __func__, __LINE__);
		arm_smccc_smc(SIP_WATCHDOG_RESET_CTRL,
				wdt_en,
				cb_wwdt->reset_enable_watchdog,
				0, 0, 0, 0, 0, &res);
	}
}

static void tcc_cb_wwdt_check_and_set_pretimeout(struct watchdog_device *wdd,
		unsigned int pretimeout)
{
	const struct tcc_windowed_watchdog_device *cb_wwdt = watchdog_get_drvdata(wdd);
	uint32_t kick_cnt = 0;
	unsigned long ul_clk_rate = clk_get_rate(cb_wwdt->wwdt_clk);
	unsigned int clk_rate;

	if ((ul_clk_rate > 0UL) && (ul_clk_rate <= UINT_MAX)) {
		clk_rate = (unsigned int)ul_clk_rate;
		/*
		 * Show warning about invalid pretimeout in watchdog probe.
		 * WDIOC_SETPRETIMEOUT ioctl can not reach hear if new pretimeout value
		 * is invalid.
		 */
		if (watchdog_pretimeout_invalid(wdd, pretimeout)) {

			dev_warn(&cb_wwdt->pdev->dev,
					"[%s] pretimeout [%u] is invalid.\n",
					__func__, pretimeout);
		}

		if (check_mul_overflow(pretimeout, clk_rate, &kick_cnt) != (bool)0) {

			dev_warn(&cb_wwdt->pdev->dev,
					"[%s] pretimeout [%u] is too big. Reduce pretimeout to maximum. [%u]\n",
					__func__, pretimeout, (UINT_MAX/clk_rate));

			pretimeout = UINT_MAX / clk_rate;
			kick_cnt = pretimeout * clk_rate;
		}

        writel(WWDT_WR_PW, &cb_wwdt->wwdt_base[CBUS_WWDT_WR_PW]);
		writel(kick_cnt, &cb_wwdt->wwdt_base[CBUS_WWDT_IRQCNT]);
		// Set WDT IRQ CNT

		wdd->pretimeout = pretimeout;

		if (wdd->pretimeout > wdd->timeout) {
			dev_warn(&cb_wwdt->pdev->dev,
					"[%s] pretimeout value is bigger than timeout\n",
					__func__);
		}
	}
}

static int tcc_cb_wwdt_set_pretimeout(struct watchdog_device *wdd,
		unsigned int pretimeout)
{
	const struct tcc_windowed_watchdog_device *cb_wwdt = watchdog_get_drvdata(wdd);
	bool is_active = watchdog_hw_running(wdd);
	int ret = 0;

	if (is_active) {
		tcc_cb_wwdt_disable_timer(wdd);
	}

	if (pretimeout > 0U) {
		tcc_cb_wwdt_check_and_set_pretimeout(wdd, pretimeout);
	} else {
		ret = -EINVAL;
	}

	if (is_active) {
		tcc_cb_wwdt_enable_timer(wdd);
	}

	if (ret == 0) {
		/* If someone call SETPRETIMEOUT ioctl repeatly faster than
		 * pretimeout interval. Watchdog reset will occur accidently.
		 * (kick interrupt never occur..)
		 * To prevent this situation, call ping at here.
		 */
		(void)tcc_cb_wwdt_ping(wdd);
		dev_info(&cb_wwdt->pdev->dev, "[%s] pretimeout: %u sec\n",
				__func__, wdd->pretimeout);
	}
	return ret;
}

static void tcc_cb_wwdt_check_and_set_timeout(struct watchdog_device *wdd,
		unsigned int timeout)
{
	const struct tcc_windowed_watchdog_device *cb_wwdt = watchdog_get_drvdata(wdd);
	uint32_t reset_cnt = 0;
	unsigned long ul_clk_rate = clk_get_rate(cb_wwdt->wwdt_clk);
	unsigned int clk_rate = (ul_clk_rate > UINT_MAX) ?
		UINT_MAX : (unsigned int)ul_clk_rate;

	/*
	 * Show warning about invalid timeout in watchdog probe.
	 * WDIOC_SETTIMEOUT ioctl can not reach hear if new timeout value
	 * is invalid.
	 */
	if (watchdog_timeout_invalid(wdd, timeout)) {
		dev_warn(&cb_wwdt->pdev->dev,
				"[%s] timeout [%u] is invalid\n",
				__func__, timeout);
	}

	if (check_mul_overflow(timeout, clk_rate, &reset_cnt) != (bool)0) {
		dev_warn(&cb_wwdt->pdev->dev,
			"[%s] timeout [%u] is too big. Reduce timeout to maximum. [%u]\n",
			__func__, timeout, wdd->max_timeout);

		timeout = wdd->max_timeout;
		reset_cnt = timeout * clk_rate;
	}

    writel(WWDT_WR_PW, &cb_wwdt->wwdt_base[CBUS_WWDT_WR_PW]);
	writel(reset_cnt, &cb_wwdt->wwdt_base[CBUS_WWDT_RSTCNT]);
	// Set WDT RST CNT

	wdd->timeout = timeout;
}

static int tcc_cb_wwdt_set_timeout(struct watchdog_device *wdd,
		unsigned int timeout)
{
	const struct tcc_windowed_watchdog_device *cb_wwdt = watchdog_get_drvdata(wdd);
	bool is_active = watchdog_hw_running(wdd);
	int ret = 0;

	if (is_active) {
		tcc_cb_wwdt_disable_timer(wdd);
	}

	tcc_cb_wwdt_check_and_set_timeout(wdd, timeout);

	if (is_active) {
		tcc_cb_wwdt_enable_timer(wdd);
	}

	dev_info(&cb_wwdt->pdev->dev,
			"[%s] timeout: %u sec\n", __func__, wdd->timeout);

	return ret;
}

static int tcc_cb_wwdt_start(struct watchdog_device *wdd)
{
	const struct tcc_windowed_watchdog_device *cb_wwdt = watchdog_get_drvdata(wdd);

	(void)tcc_cb_wwdt_ping(wdd);

	tcc_cb_wwdt_enable_timer(wdd);

	tcc_cb_wwdt_reset_ctrl(cb_wwdt, WDT_ENABLE);

	return 0;
}

static int tcc_cb_wwdt_stop(struct watchdog_device *wdd)
{
	const struct tcc_windowed_watchdog_device *cb_wwdt = watchdog_get_drvdata(wdd);

	(void)tcc_cb_wwdt_ping(wdd);

	tcc_cb_wwdt_disable_timer(wdd);

	tcc_cb_wwdt_reset_ctrl(cb_wwdt, WDT_DISABLE);

	return 0;
}

static int tcc_cb_wwdt_ping(struct watchdog_device *wdd)
{
	const struct tcc_windowed_watchdog_device *cb_wwdt = watchdog_get_drvdata(wdd);

	dev_dbg(&cb_wwdt->pdev->dev, "%s\n", __func__);
    writel(WWDT_WR_PW, &cb_wwdt->wwdt_base[CBUS_WWDT_WR_PW]);
	writel(0x1, &cb_wwdt->wwdt_base[CBUS_WWDT_CLEAR]);

	return 0;
}

static unsigned int tcc_cb_wwdt_get_status(struct watchdog_device *wdd)
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

static const struct watchdog_info tcc_cb_wwdt_info = {
	.options = (	WDIOF_SETTIMEOUT |
			WDIOF_PRETIMEOUT |
			WDIOF_KEEPALIVEPING |
			WDIOF_MAGICCLOSE ),
	.identity = "tcc-cb-wwdt",
};

static const struct watchdog_info tcc_cb_wwdt_info_no_irq = {
	.options = (	WDIOF_SETTIMEOUT |
			WDIOF_KEEPALIVEPING |
			WDIOF_MAGICCLOSE ),
	.identity = "tcc-cb-wwdt",
};

static irqreturn_t tcc_cb_wwdt_kick(int irq, void *dev_id)
{
	struct watchdog_device* wdd = dev_id;

	(void)irq;

	(void)tcc_cb_wwdt_ping(wdd);

	return IRQ_HANDLED;
}

static const struct watchdog_ops tcc_cb_wwdt_ops = {
	.owner		= THIS_MODULE,
	.start		= tcc_cb_wwdt_start,
	.stop		= tcc_cb_wwdt_stop,
	.ping		= tcc_cb_wwdt_ping,
	.set_timeout	= tcc_cb_wwdt_set_timeout,
	.set_pretimeout	= tcc_cb_wwdt_set_pretimeout,
	.status		= tcc_cb_wwdt_get_status,
	.ioctl		= NULL,
};

static int tcc_cb_wwdt_ioremap(struct tcc_windowed_watchdog_device *cb_wwdt,
		const struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;

	cb_wwdt->wwdt_base = (unsigned int *)of_iomap(np, 0);

	if (cb_wwdt->wwdt_base == NULL) {
		ret = -EFAULT;
	} else {
		/* Get which cpu bus wathcdog reset enable */
		if (of_property_read_u32(np, "reset-enable-watchdog",
					&cb_wwdt->reset_enable_watchdog) == 0) {
			cb_wwdt->is_reset_enable = 1U;
		}
	}

	return ret;
}

static int tcc_cb_wwdt_init_clock(struct tcc_windowed_watchdog_device *cb_wwdt,
		struct platform_device *pdev)
{
	int ret = 0;

	cb_wwdt->wwdt_clk = devm_clk_get(&pdev->dev, "clk_wwdt");

	if (IS_ERR(cb_wwdt->wwdt_clk)) {
		ret = -ENODEV;
	}

	return ret;
}

static int tcc_cb_wwdt_read_of_property_kick_timer(struct tcc_windowed_watchdog_device *cb_wwdt,
		const struct platform_device* pdev)
{
	const struct device_node* np = pdev->dev.of_node;
	struct watchdog_device *wdd = &cb_wwdt->wdd;
	int ret = 0;

	if (of_property_read_u32(np, "kick-interval", &wdd->pretimeout) != 0) {
		/* Set pretimeout to 0 */
		cb_wwdt->wdd.pretimeout = 0U;
		cb_wwdt->wdd.info = &tcc_cb_wwdt_info_no_irq;
        writel(WWDT_WR_PW, &cb_wwdt->wwdt_base[CBUS_WWDT_WR_PW]);
		writel(0U, &cb_wwdt->wwdt_base[CBUS_WWDT_IRQCNT]);
		ret = -ENOENT;
	}

	return ret;
}

static int tcc_cb_wwdt_init_kick_timer(struct tcc_windowed_watchdog_device *cb_wwdt,
		struct platform_device *pdev)
{
	int ret = 0;
	unsigned int kick_irq;

	ret = tcc_cb_wwdt_read_of_property_kick_timer(cb_wwdt, pdev);

	if (ret == 0) {
		ret = platform_get_irq(pdev, 0);
		if (ret >= 0) {
			kick_irq = (unsigned int)ret;

			ret = request_irq(kick_irq, tcc_cb_wwdt_kick, IRQF_SHARED,
					"tcc_cb_wwdt_kick", &cb_wwdt->wdd);
			if (ret != 0) {
				dev_err(&pdev->dev, "[%s] failed to request kick_irq\n",
						__func__);
			}
		}
		if (ret == 0) {
			ret = tcc_cb_wwdt_set_pretimeout(&cb_wwdt->wdd,
					cb_wwdt->wdd.pretimeout);
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

static int tcc_cb_wwdt_init_wdt_timer(struct tcc_windowed_watchdog_device *cb_wwdt,
		struct platform_device *pdev)
{
	int ret = 0;
	unsigned long clk_rate = clk_get_rate(cb_wwdt->wwdt_clk);

	if ((clk_rate > 0UL) && (clk_rate <= UINT_MAX)) {
		cb_wwdt->wdd.min_timeout = 1;
		cb_wwdt->wdd.max_timeout = UINT_MAX / (unsigned int)clk_rate;
		cb_wwdt->wdd.timeout = cb_wwdt->wdd.max_timeout;
		/* If timeout-sec is exist in device tree,
		 * wdd.timeout will be overwrite by watchdog_init_timeout(). */
		(void)watchdog_init_timeout(&cb_wwdt->wdd, 0, &pdev->dev);

		if (cb_wwdt->wdd.timeout > 0U) {
			ret = tcc_cb_wwdt_set_timeout(&cb_wwdt->wdd,
					cb_wwdt->wdd.timeout);
		}
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static int tcc_cb_wwdt_init_timer(struct tcc_windowed_watchdog_device* cb_wwdt,
		struct platform_device *pdev)
{
	int ret = 0;

	cb_wwdt->wdd.info = &tcc_cb_wwdt_info;
	cb_wwdt->wdd.ops = &tcc_cb_wwdt_ops;

	ret = tcc_cb_wwdt_init_wdt_timer(cb_wwdt, pdev);

	if (ret == 0) {
		ret = tcc_cb_wwdt_init_kick_timer(cb_wwdt, pdev);
	}

	return ret;
}

static int tcc_cb_wwdt_init(struct tcc_windowed_watchdog_device *cb_wwdt,
		struct platform_device *pdev)
{
	int ret;

	ret = tcc_cb_wwdt_ioremap(cb_wwdt, pdev);

	if (ret == 0) {
		ret = tcc_cb_wwdt_init_clock(cb_wwdt, pdev);
	}

	if (ret == 0) {
		ret = tcc_cb_wwdt_init_timer(cb_wwdt, pdev);
	}

	return ret;
}

static int tcc_cb_wwdt_register(struct tcc_windowed_watchdog_device *cb_wwdt,
		struct platform_device *pdev)
{
	int ret;
	struct watchdog_device *wdd;

	wdd = &cb_wwdt->wdd;

	ret = devm_watchdog_register_device(&pdev->dev, wdd);
	if (ret != 0) {
		dev_err(&pdev->dev,
				"[%s] failed to register wdt device\n", __func__);
	}

	watchdog_stop_on_reboot(wdd);
	watchdog_stop_on_unregister(wdd);

	return ret;
}

#define TFA_V_MAJOR	0
#define TFA_V_MINOR	0
#define TFA_V_PATCH	0

static int tcc_cb_wwdt_check_tf_a_version(const struct platform_device *pdev)
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

static int tcc_cb_wwdt_probe(struct platform_device *pdev)
{
	struct tcc_windowed_watchdog_device *cb_wwdt;
	int ret = 0;

	/* Check TF-A version before probe start. */
	ret = tcc_cb_wwdt_check_tf_a_version(pdev);

	if (ret == 0) {
		cb_wwdt = devm_kzalloc(&pdev->dev,
				sizeof(struct tcc_windowed_watchdog_device), GFP_KERNEL);

		if (cb_wwdt == NULL) {
			ret = -ENOMEM;
		} else {
			cb_wwdt->pdev = pdev;
			platform_set_drvdata(pdev, cb_wwdt);
			watchdog_set_drvdata(&cb_wwdt->wdd, cb_wwdt);
		}

		if (ret == 0) {
			ret = tcc_cb_wwdt_init(cb_wwdt, pdev);
		}

		if (ret == 0) {
			ret = tcc_cb_wwdt_register(cb_wwdt, pdev);
		}

		if (ret == 0) {
		    ret = tcc_cb_wwdt_start(&cb_wwdt->wdd);
		}
	}

	return ret;
}

static int tcc_cb_wwdt_remove(struct platform_device *pdev)
{
	struct tcc_windowed_watchdog_device *cb_wwdt = platform_get_drvdata(pdev);
	int ret = 0;

	if (cb_wwdt == NULL) {
		dev_err(&pdev->dev, "%s: cb_wwdt is NULL", __func__);
		ret = -ENODEV;
	} else {
		if (watchdog_hw_running(&cb_wwdt->wdd)) {
			/* if device is active,*/
			(void)tcc_cb_wwdt_stop(&cb_wwdt->wdd);
		}

		dev_info(&pdev->dev, "cbus wathcdog driver remove.");
		watchdog_unregister_device(&cb_wwdt->wdd);
	}

	return ret;
}

static void tcc_cb_wwdt_shutdown(struct platform_device *pdev)
{
	struct tcc_windowed_watchdog_device *cb_wwdt = platform_get_drvdata(pdev);

	if (cb_wwdt == NULL) {
		dev_err(&pdev->dev, "[%s]Cannot find watchdog device.", __func__);
	} else {
		if (watchdog_hw_running(&cb_wwdt->wdd)) {
			/* if device is active,*/
			(void)tcc_cb_wwdt_stop(&cb_wwdt->wdd);
		}
		dev_info(&pdev->dev, "cbus watchdog driver shutdown.");
	}
}

#ifdef CONFIG_PM
static int tcc_cb_wwdt_pm_suspend(struct device *dev)
{
	struct tcc_windowed_watchdog_device *cb_wwdt = dev_get_drvdata(dev);
	int ret = 0;

	if (cb_wwdt == NULL) {
		dev_err(dev, "%s: cb_wwdt is NULL", __func__);
		ret = -ENODEV;
	} else {
		(void)tcc_cb_wwdt_stop(&cb_wwdt->wdd);
		dev_info(dev, "%s\n", __func__);
	}

	return ret;
}

static int tcc_cb_wwdt_pm_resume(struct device *dev)
{
	struct tcc_windowed_watchdog_device *cb_wwdt = dev_get_drvdata(dev);
	int ret = 0;

	if (cb_wwdt == NULL) {
		dev_err(dev, "%s: cb_wwdt is NULL", __func__);
		ret = -ENODEV;
	} else {
		(void)tcc_cb_wwdt_set_timeout(
				&cb_wwdt->wdd, cb_wwdt->wdd.timeout);
		(void)tcc_cb_wwdt_set_pretimeout(
				&cb_wwdt->wdd, cb_wwdt->wdd.pretimeout);
		(void)tcc_cb_wwdt_start(&cb_wwdt->wdd);
		dev_info(dev, "%s\n", __func__);
	}

	return ret;
}
#else
#define tcc_cb_wwdt_pm_suspend  NULL
#define tcc_cb_wwdt_pm_resume   NULL
#endif

static const struct of_device_id tcc_cb_wwdt_of_match[] = {
	{.compatible = "telechips,tcc-cb-wwdt",},
	{},
};

MODULE_DEVICE_TABLE(of, tcc_cb_wwdt_of_match);

static const struct dev_pm_ops tcc_cb_wwdt_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(tcc_cb_wwdt_pm_suspend, tcc_cb_wwdt_pm_resume)
};

static struct platform_driver tcc_cb_wwdt_driver = {
	.probe		= tcc_cb_wwdt_probe,
	.remove		= tcc_cb_wwdt_remove,
	.shutdown	= tcc_cb_wwdt_shutdown,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "tcc-cb-wwdt",
		.pm	= &tcc_cb_wwdt_pm_ops,
		.of_match_table = tcc_cb_wwdt_of_match,
	},
};

module_platform_driver(tcc_cb_wwdt_driver);

MODULE_AUTHOR("Telechips Corporation");
MODULE_DESCRIPTION("Telechips CBUS Windowed Watchdog Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:tcc-cbus-wwdt");