// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *	Copyright (C) Telechips Inc.
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
#include <linux/overflow.h>
#include <linux/uaccess.h>
#include <linux/arm-smccc.h>
#include <soc/telechips/smc.h>
#include <soc/telechips/chipinfo.h>

#define wdt_readl	readl
#define wdt_writel	writel
#define WDTBIT(n)	((uint32_t)1U << (n))
#define WDTULLBIT(n)	((uint64_t)1U << (n))

#define KICK_CNT_MAX	0xFFFFU
#define WDT_CNT_MAX	UINT_MAX

#define TCKSEL_MAX		7U
extern unsigned int tcksel_factor[TCKSEL_MAX];
unsigned int tcksel_factor[TCKSEL_MAX] = {
	2, 4, 8, 16, 32, 1024, 4096
};

enum wdt_cfg_bit_e {
	TWDCFG_EN	= 0,
	TWDCFG_IEN	= 3,
	TWDCFG_TCKSEL	= 4,
};

enum wdt_en_bit_e {
	/* ONLY FOR TCC803X START */
	WDT_CPU_BUS_0	= 0,
	WDT_CPU_BUS_1	= 1,
	WDT_CPU_BUS_2	= 2,
	WDT_CPU_BUU_3	= 3,
	WDT_CPU_BUS_4	= 4,
	WDT_CM_BUS	= 5,
	WDT_PMU_RESET	= 6,
	/* ONLY FOR TCC803X END */
	WDT_PMU_EN	= 31
};

struct tcc_kick_timer {
	void __iomem	*tireq;
	void __iomem	*wdtcfg;
	void __iomem	*wdtcnt;
	struct clk	*kick_clk;
};

struct tcc_wdt_timer {
	/* wdtctrl is only for tcc897x, others will use SIP service */
	void __iomem	*wdtctrl;
	struct clk	*wdt_clk;
};

struct tcc_watchdog_device {
	struct watchdog_device		wdd;
	struct platform_device		*pdev;
	struct tcc_kick_timer		kick;
	struct tcc_wdt_timer		wdt;

	unsigned int	kick_cnt;

	unsigned int	have_rstcnt;
	unsigned int	wdt_irq_bit;
	unsigned int	pmu_clr_bit;
};

static void tcc_pmu_wdt_enable_timer(struct watchdog_device *wdd);
static void tcc_pmu_wdt_disable_timer(struct watchdog_device *wdd);
static int tcc_pmu_wdt_start(struct watchdog_device *wdd);
static int tcc_pmu_wdt_stop(struct watchdog_device *wdd);
static int tcc_pmu_wdt_ping(struct watchdog_device *wdd);
static unsigned int tcc_pmu_wdt_get_status(struct watchdog_device *wdd);

static void tcc_pmu_wdt_enable_timer(struct watchdog_device *wdd)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	const struct tcc_watchdog_device *pmu_wdt = watchdog_get_drvdata(wdd);
	unsigned long reg_val;

#if defined(CONFIG_HAVE_TELECHIPS_SIP_SERVICE)
	struct arm_smccc_res res = {0};

	arm_smccc_smc(SIP_WATCHDOG_START,
			0,
			WDTBIT(WDT_PMU_EN),
			0, 0, 0, 0, 0,
			&res);
#else
	/* [DR]
	 * Kernel API readl has defects it's inside.
	 */
	reg_val = wdt_readl(pmu_wdt->wdt.wdtctrl);
	reg_val |= WDTBIT(WDT_PMU_EN);
	wdt_writel(reg_val, pmu_wdt->wdt.wdtctrl);
#endif
	if (wdd->pretimeout > 0U) {
		/* [DR]
		 * Kernel API readl has defects it's inside.
		 */
		reg_val = wdt_readl(pmu_wdt->kick.wdtcfg);
		reg_val |= (WDTBIT(TWDCFG_EN) | WDTBIT(TWDCFG_IEN));

		wdt_writel(reg_val, pmu_wdt->kick.wdtcfg);
	}

	set_bit(WDOG_HW_RUNNING, &wdd->status);
}

static void tcc_pmu_wdt_disable_timer(struct watchdog_device *wdd)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	const struct tcc_watchdog_device *pmu_wdt = watchdog_get_drvdata(wdd);
	unsigned long reg_val;
#if defined(CONFIG_HAVE_TELECHIPS_SIP_SERVICE)
	struct arm_smccc_res res = {0};

	arm_smccc_smc(SIP_WATCHDOG_STOP,
			0,
			WDTBIT(WDT_PMU_EN),
			0, 0, 0, 0, 0,
			&res);
#else
	/* [DR]
	 * Kernel API readl has defects it's inside.
	 */
	reg_val = wdt_readl(pmu_wdt->wdt.wdtctrl);
	reg_val &= ~(WDTBIT(WDT_PMU_EN));
	wdt_writel(reg_val, pmu_wdt->wdt.wdtctrl);
#endif
	/* [DR]
	 * Kernel API readl has defects it's inside.
	 */
	reg_val = wdt_readl(pmu_wdt->kick.wdtcfg);
	reg_val &= ~(WDTBIT(TWDCFG_EN) | WDTBIT(TWDCFG_IEN));

	wdt_writel(reg_val, pmu_wdt->kick.wdtcfg);

	clear_bit(WDOG_HW_RUNNING, &wdd->status);

}

static void tcc_pmu_wdt_check_and_set_pretimeout(struct watchdog_device *wdd,
		unsigned int pretimeout)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	struct tcc_watchdog_device *pmu_wdt = watchdog_get_drvdata(wdd);
	unsigned long ul_clk_rate = clk_get_rate(pmu_wdt->kick.kick_clk);
	uint32_t clk_rate = (ul_clk_rate > UINT_MAX) ?
				UINT_MAX : (uint32_t)ul_clk_rate;
	uint32_t timer_tick = 0, max_pretimeout = 0;
	uint32_t kick_cnt = 0, tcksel = TCKSEL_MAX;

	/*
	 * Show warning about invalid pretimeout in watchdog probe.
	 * WDIOC_SETPRETIMEOUT ioctl can not reach hear if new pretimeout value
	 * is invalid.
	 */
	if (watchdog_pretimeout_invalid(wdd, pretimeout)) {

		dev_warn(&pmu_wdt->pdev->dev,
				"[%s] pretimeout [%u] is invalid.\n",
				__func__, pretimeout);
	}

	/* tcksel limited in range 4 ~ 6 */
	for (tcksel = 4; tcksel < TCKSEL_MAX; tcksel++) {
		timer_tick = clk_rate;
		timer_tick /= tcksel_factor[tcksel];
		max_pretimeout = KICK_CNT_MAX / timer_tick;
		if (max_pretimeout >= pretimeout) {
			break;
		}
	}

	if (tcksel >= TCKSEL_MAX) {
		dev_warn(&pmu_wdt->pdev->dev,
				"[%s] pretimeout [%u] is overflowed. Reduce pretimeout to maximum. [%u]\n",
				__func__, pretimeout, max_pretimeout);

		pretimeout = max_pretimeout;
	}

	if (check_mul_overflow(pretimeout, timer_tick, &kick_cnt) != (bool)0) {
		dev_warn(&pmu_wdt->pdev->dev,
			"[%s] pretimeout [%u] is too big. Reduce pretimeout to maximum. [%u]\n",
			__func__, pretimeout, max_pretimeout);
		pretimeout = max_pretimeout;
		kick_cnt = max_pretimeout * timer_tick;
	}

	kick_cnt = KICK_CNT_MAX - kick_cnt;
	wdt_writel(kick_cnt, pmu_wdt->kick.wdtcnt);
	wdt_writel((tcksel << TWDCFG_TCKSEL), pmu_wdt->kick.wdtcfg);

	pmu_wdt->kick_cnt = kick_cnt;
	pmu_wdt->wdd.pretimeout = pretimeout;

	if (wdd->pretimeout > wdd->timeout) {
		dev_warn(&pmu_wdt->pdev->dev,
				"[%s] pretimeout value is bigger than timeout\n",
				__func__);
	}
}

static int tcc_pmu_wdt_set_pretimeout(struct watchdog_device *wdd,
		unsigned int pretimeout)
{
	struct tcc_watchdog_device *pmu_wdt = watchdog_get_drvdata(wdd);
	bool is_active = watchdog_hw_running(wdd);
	int ret = 0;

	if (is_active) {
		tcc_pmu_wdt_disable_timer(wdd);
	}

	tcc_pmu_wdt_check_and_set_pretimeout(wdd, pretimeout);

	if (is_active) {
		tcc_pmu_wdt_enable_timer(wdd);
	}

	/* If someone call SETPRETIMEOUT ioctl repeatly faster than
	 * pretimeout interval. Watchdog reset will occur accidently.
	 * (kick interrupt never occur..)
	 * To prevent this situation, call ping at here.
	 */
	(void)tcc_pmu_wdt_ping(wdd);
	dev_info(&pmu_wdt->pdev->dev, "[%s] pretimeout: %u sec\n",
			__func__, pmu_wdt->wdd.pretimeout);

	return ret;
}

static void tcc_pmu_wdt_check_and_set_timeout(struct watchdog_device *wdd,
		unsigned int timeout)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	const struct tcc_watchdog_device *pmu_wdt = watchdog_get_drvdata(wdd);
	unsigned long ul_clk_rate;
	uint32_t reset_cnt = 0, clk_rate = 0;
#if defined(CONFIG_HAVE_TELECHIPS_SIP_SERVICE)
	struct arm_smccc_res res = {0};
#endif

	/*
	 * Show warning about invalid timeout in watchdog probe.
	 * WDIOC_SETTIMEOUT ioctl can not reach hear if new timeout value
	 * is invalid.
	 */
	if (watchdog_timeout_invalid(wdd, timeout)) {
		dev_warn(&pmu_wdt->pdev->dev,
			"[%s] timeout [%u] is invalid\n",
			__func__, timeout);
	}

	ul_clk_rate = clk_get_rate(pmu_wdt->wdt.wdt_clk);
	clk_rate = (ul_clk_rate > UINT_MAX) ?
			UINT_MAX : (uint32_t)ul_clk_rate;

	if (check_mul_overflow(timeout, clk_rate, &reset_cnt) != (bool)0) {
		dev_warn(&pmu_wdt->pdev->dev,
			"[%s] timeout [%u] is too big. Reduce timeout to maximum. [%u]\n",
			__func__, timeout, wdd->max_timeout);

		timeout = wdd->max_timeout;
		reset_cnt = timeout * clk_rate;
	}

#if defined(CONFIG_HAVE_TELECHIPS_SIP_SERVICE)
	arm_smccc_smc(SIP_WATCHDOG_SETUP,
			0, 0, 0,
			reset_cnt,
			0, 0, 0,
			&res);
#else
	// Set WDT Reset CNT
	wdt_writel(reset_cnt, pmu_wdt->wdt.wdtctrl);
#endif

	wdd->timeout = timeout;
}

static int tcc_pmu_wdt_set_timeout(struct watchdog_device *wdd,
		unsigned int timeout)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	const struct tcc_watchdog_device *pmu_wdt = watchdog_get_drvdata(wdd);
	int ret = 0;
	bool is_active = watchdog_hw_running(wdd);

	if (is_active) {
		tcc_pmu_wdt_disable_timer(wdd);
	}

	tcc_pmu_wdt_check_and_set_timeout(wdd, timeout);

	if (is_active) {
		tcc_pmu_wdt_enable_timer(wdd);
	}

	dev_info(&pmu_wdt->pdev->dev,
			"[%s] timeout: %u sec\n", __func__, wdd->timeout);

	return ret;
}

static int tcc_pmu_wdt_start(struct watchdog_device *wdd)
{

	tcc_pmu_wdt_enable_timer(wdd);

	(void)tcc_pmu_wdt_ping(wdd);


	return 0;
}

static int tcc_pmu_wdt_stop(struct watchdog_device *wdd)
{

	(void)tcc_pmu_wdt_ping(wdd);

	tcc_pmu_wdt_disable_timer(wdd);

	return 0;
}

static int tcc_pmu_wdt_ping(struct watchdog_device *wdd)
{
	/* [DR]
	 * Function family of get_drvdata usually need to cast void* to others.
	 */
	const struct tcc_watchdog_device *pmu_wdt = watchdog_get_drvdata(wdd);
#if defined(CONFIG_HAVE_TELECHIPS_SIP_SERVICE)
	struct arm_smccc_res res = {0};
#else
	unsigned int reg_val;
#endif
	/* [DR]
	 * Kernel API dev_dbg has defects it's inside.
	 */
	dev_dbg(&pmu_wdt->pdev->dev, "%s\n", __func__);

	wdt_writel(pmu_wdt->kick_cnt, pmu_wdt->kick.wdtcnt);

	if (pmu_wdt->pmu_clr_bit < 32U) {
#if defined(CONFIG_HAVE_TELECHIPS_SIP_SERVICE)
		arm_smccc_smc(SIP_WATCHDOG_PING, 0,
				WDTULLBIT(pmu_wdt->pmu_clr_bit), 0,
				0, 0, 0, 0, &res);
#else
		reg_val = wdt_readl(pmu_wdt->wdt.wdtctrl);
		reg_val |= WDTBIT(pmu_wdt->pmu_clr_bit);
		wdt_writel(reg_val, pmu_wdt->wdt.wdtctrl);
#endif
	}

	return 0;
}

/* [DR]
 * tcc_pmu_wdt_get_status is call back of tcc_pmu_wdt_ops.status
 * watchdog core driver declare status function as
 * ‘unsigned int (*status)(struct watchdog_device *)’
 */
static unsigned int tcc_pmu_wdt_get_status(struct watchdog_device *wdd)
{
	unsigned int wdt_stat = 0;
#if defined(CONFIG_HAVE_TELECHIPS_SIP_SERVICE)
	struct arm_smccc_res res = {0};
	(void)wdd;

	arm_smccc_smc(SIP_WATCHDOG_GET_STATUS, 0, 0, 0, 0, 0, 0, 0, &res);
	wdt_stat = (res.a0 < UINT_MAX) ? (unsigned int)res.a0 : 0U;
#else
	const struct tcc_watchdog_device *pmu_wdt = watchdog_get_drvdata(wdd);

	/* [DR]
	 * Kernel API readl has defects it's inside.
	 */
	wdt_stat = wdt_readl(pmu_wdt->wdt.wdtctrl);
#endif

	wdt_stat &= WDTBIT(WDT_PMU_EN);
	wdt_stat >>= WDT_PMU_EN;

	return wdt_stat;
}

static const struct watchdog_info tcc_pmu_wdt_info = {
	.options = (	WDIOF_SETTIMEOUT |
			WDIOF_PRETIMEOUT |
			WDIOF_KEEPALIVEPING |
			WDIOF_MAGICCLOSE ),
	.firmware_version = 0,
	.identity = "tcc-pmu-wdt",
};

static const struct watchdog_info tcc_pmu_wdt_info_no_irq = {
	.options = (	WDIOF_SETTIMEOUT |
			WDIOF_KEEPALIVEPING |
			WDIOF_MAGICCLOSE ),
	.firmware_version = 0,
	.identity = "tcc-pmu-wdt",
};

static irqreturn_t tcc_pmu_wdt_timer_ping(int irq, void *dev_id)
{
	/* [DR]
	 * void *dev_id must covert to struct device *.
	 */
	struct tcc_watchdog_device* pmu_wdt =
			(struct tcc_watchdog_device *)dev_id;
	uint32_t reg_val;
	(void)irq;

	/* [DR]
	 * Kernel API readl has defects it's inside.
	 */
	reg_val = wdt_readl(pmu_wdt->kick.tireq);
	if ((reg_val & WDTBIT(6)) != 0U) {
		(void)tcc_pmu_wdt_ping(&pmu_wdt->wdd);
		reg_val = WDTBIT(6);
		wdt_writel(reg_val, pmu_wdt->kick.tireq);
	}

	return IRQ_HANDLED;
}

static const struct watchdog_ops tcc_pmu_wdt_ops = {
	.owner		= THIS_MODULE,
	.start		= tcc_pmu_wdt_start,
	.stop		= tcc_pmu_wdt_stop,
	.ping		= tcc_pmu_wdt_ping,
	.set_timeout	= tcc_pmu_wdt_set_timeout,
	.set_pretimeout	= tcc_pmu_wdt_set_pretimeout,
	.status		= tcc_pmu_wdt_get_status,
	/* Currently not support custom ioctl */
	.ioctl		= NULL,
};

static int tcc_pmu_wdt_ioremap(struct tcc_watchdog_device *pmu_wdt,
		const struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;

	/* [DR] */
	pmu_wdt->wdt.wdtctrl = of_iomap(np, 0);
	pmu_wdt->kick.wdtcfg = of_iomap(np, 1);
	pmu_wdt->kick.wdtcnt = of_iomap(np, 2);
	pmu_wdt->kick.tireq  = of_iomap(np, 3);

	if ((pmu_wdt->kick.wdtcfg == NULL) || (pmu_wdt->kick.tireq  == NULL) ||
	    (pmu_wdt->wdt.wdtctrl == NULL)) {
		ret = -EFAULT;
	}


	if (of_find_property(np, "have-rstcnt-reg", NULL) != NULL) {
		pmu_wdt->have_rstcnt = 1;
	}

	if (of_property_read_u32(np, "clear-bit", &pmu_wdt->pmu_clr_bit) != 0) {
		ret = -ENOENT;
	}

	return ret;
}

static int tcc_pmu_wdt_init_clock(struct tcc_watchdog_device *pmu_wdt,
		struct platform_device *pdev)
{
	int ret = 0;

	pmu_wdt->wdt.wdt_clk = devm_clk_get(&pdev->dev, "peri_tcx");
	pmu_wdt->kick.kick_clk = devm_clk_get(&pdev->dev, "peri_tct");
	/* [DR]
	 * Kernel API IS_ERR has defects
	 * it's inside.
	 */
	if (IS_ERR(pmu_wdt->wdt.wdt_clk) || IS_ERR(pmu_wdt->kick.kick_clk)) {
		dev_err(&pdev->dev, 	"Cannot find watchdog clock\n"
					"   wdt_clk : 0x%p   kick_clk: 0x%p\n",
				pmu_wdt->wdt.wdt_clk, pmu_wdt->kick.kick_clk);
		ret = -ENODEV;
	}

	if (ret == 0) {

		ret = clk_prepare_enable(pmu_wdt->wdt.wdt_clk);
		dev_info(&pdev->dev, "[%s] watchdog clock rate: %lu\n",
				__func__, clk_get_rate(pmu_wdt->wdt.wdt_clk));
	}

	return ret;
}

static int tcc_pmu_wdt_read_of_property_kick_timer(struct tcc_watchdog_device *pmu_wdt,
		const struct platform_device* pdev)
{
	const struct device_node* np = pdev->dev.of_node;
	struct watchdog_device *wdd = &pmu_wdt->wdd;
	int ret = 0;

	if (of_property_read_u32(np, "kick-interval", &wdd->pretimeout) != 0) {
		/* Set pretimeout to 0 */
		pmu_wdt->wdd.pretimeout = 0U;
		pmu_wdt->wdd.info = &tcc_pmu_wdt_info_no_irq;
		wdt_writel(0U, pmu_wdt->kick.wdtcnt);
		ret = -ENOENT;
	}

	return ret;
}

static int tcc_pmu_wdt_init_kick_timer(struct tcc_watchdog_device *pmu_wdt,
		struct platform_device *pdev)
{
	int ret = 0;
	unsigned int kick_irq;

	ret = tcc_pmu_wdt_read_of_property_kick_timer(pmu_wdt, pdev);

	if (ret == 0) {
		ret = platform_get_irq(pdev, 0);
		if (ret >= 0) {
			kick_irq = (unsigned int)ret;

			ret = request_irq(kick_irq, tcc_pmu_wdt_timer_ping,
					IRQF_SHARED, "tcc_pmu_wdt_kick", pmu_wdt);
			if (ret != 0) {
				dev_err(&pdev->dev, "[%s] failed to request kick_irq\n",
						__func__);
			}
		}

		if (ret == 0) {
			ret = tcc_pmu_wdt_set_pretimeout(&pmu_wdt->wdd,
					pmu_wdt->wdd.pretimeout);
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

static int tcc_pmu_wdt_init_wdt_timer(struct tcc_watchdog_device *pmu_wdt,
		struct platform_device *pdev)
{
	unsigned long ul_clk_rate = clk_get_rate(pmu_wdt->wdt.wdt_clk);
	uint32_t clk_rate = (ul_clk_rate > UINT_MAX) ?
				UINT_MAX : (uint32_t)ul_clk_rate;
	int ret = 0;

	pmu_wdt->wdd.min_timeout = 1;
	pmu_wdt->wdd.max_timeout = (clk_rate != 0U) ?
		(WDT_CNT_MAX / clk_rate) :
		(WDT_CNT_MAX);
	pmu_wdt->wdd.timeout = pmu_wdt->wdd.max_timeout; // Set default value

	// If timeout-sec is exist in device tree,
	// wdd.timeout will be overwrite by watchdog_init_timeout().
	(void)watchdog_init_timeout(&pmu_wdt->wdd, 0, &pdev->dev);
	if (pmu_wdt->wdd.timeout > 0U) {
		ret = tcc_pmu_wdt_set_timeout(&pmu_wdt->wdd, pmu_wdt->wdd.timeout);
	}

	return ret;
}

static int tcc_pmu_wdt_init_timer(struct tcc_watchdog_device* pmu_wdt,
		struct platform_device *pdev)
{
	int ret = 0;

	pmu_wdt->wdd.info = &tcc_pmu_wdt_info;
	pmu_wdt->wdd.ops = &tcc_pmu_wdt_ops;
	pmu_wdt->wdd.parent = &pdev->dev;

	ret = tcc_pmu_wdt_init_wdt_timer(pmu_wdt, pdev);

	if (ret == 0) {
		ret  = tcc_pmu_wdt_init_kick_timer(pmu_wdt, pdev);
	}

	return ret;
}

static int tcc_pmu_wdt_init(struct tcc_watchdog_device *pmu_wdt,
		struct platform_device *pdev)
{
	int ret;

	ret = tcc_pmu_wdt_ioremap(pmu_wdt, pdev);

	if (ret == 0) {
		ret = tcc_pmu_wdt_init_clock(pmu_wdt, pdev);
	}

	if (ret == 0) {
		ret = tcc_pmu_wdt_init_timer(pmu_wdt, pdev);
	}

	return ret;
}

static int tcc_pmu_wdt_register(struct tcc_watchdog_device *pmu_wdt,
		struct platform_device *pdev)
{
	int ret;
	struct watchdog_device *wdd;

	wdd = &pmu_wdt->wdd;

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

static int tcc_pmu_wdt_check_tf_a_version(const struct platform_device *pdev)
{
	int ret = 0;

#if defined(CONFIG_HAVE_TELECHIPS_SIP_SERVICE)
	struct version_info v_info;

	get_tf_version(&v_info);

	if (!version_compat(&v_info, TFA_V_MAJOR, TFA_V_MINOR, TFA_V_PATCH)) {
		dev_err(&pdev->dev,
			"Too old TF-A ROM. (required: v%d.%d.%d | current v%d.%d.%d)\n",
			TFA_V_MAJOR, TFA_V_MINOR, TFA_V_PATCH,
			v_info.major, v_info.minor, v_info.patch);
		ret = -EINVAL;
	}
#endif

	return ret;
}

static int tcc_pmu_wdt_probe(struct platform_device *pdev)
{
	struct tcc_watchdog_device *pmu_wdt;
	int ret = 0;

	/* Check TF-A version before probe start. */
	ret = tcc_pmu_wdt_check_tf_a_version(pdev);
	if (ret == 0) {
		/* [DR]
		 * Function family of kmalloc usually need to cast void* to others.
		 */
		pmu_wdt = devm_kzalloc(&pdev->dev,
				sizeof(struct tcc_watchdog_device), GFP_KERNEL);

		if (pmu_wdt == NULL) {
			ret = -ENOMEM;
		} else {
			pmu_wdt->pdev = pdev;
			platform_set_drvdata(pdev, pmu_wdt);
			watchdog_set_drvdata(&pmu_wdt->wdd, pmu_wdt);
		}

		if (ret == 0) {
			ret = tcc_pmu_wdt_init(pmu_wdt, pdev);
		}

		if (ret == 0) {
			ret = tcc_pmu_wdt_register(pmu_wdt, pdev);
		}

		if (ret == 0) {
			ret = tcc_pmu_wdt_start(&pmu_wdt->wdd);
		}
	}

	return ret;
}

/* [DR]
 * tcc_pmu_wdt_remove is call back of tcc_pmu_wdt_driver.remove
 * platform driver declare remove function as
 * ‘int (*)(struct platform_device *)’
 */
static int tcc_pmu_wdt_remove(struct platform_device *pdev)
{
	/* [DR]
	 * Function xxx_get_drvdata usually need to cast void* to others.
	 */
	struct tcc_watchdog_device *pmu_wdt = platform_get_drvdata(pdev);
	int ret = 0;


	if (pmu_wdt == NULL) {
		dev_err(&pdev->dev, "%s: pmu_wdt is NULL", __func__);
		ret = -ENODEV;
	} else {
		if (watchdog_hw_running(&pmu_wdt->wdd)) {
			/* if device is active,*/
			(void)tcc_pmu_wdt_stop(&pmu_wdt->wdd);
		}

		dev_info(&pdev->dev, "cbus wathcdog driver remove.");
		watchdog_unregister_device(&pmu_wdt->wdd);
	}

	return ret;
}

/* [DR]
 * tcc_pmu_wdt_shutdown is call back of tcc_pmu_wdt_driver.shutdown
 * platform driver declare shutdown function as
 * ‘void (*)(struct platform_device *)’
 */
static void tcc_pmu_wdt_shutdown(struct platform_device *pdev)
{
	/* [DR]
	 * Function xxx_get_drvdata usually need to cast void* to others.
	 */
	struct tcc_watchdog_device *pmu_wdt = platform_get_drvdata(pdev);

	if (pmu_wdt == NULL) {
		dev_err(&pdev->dev, "[%s]Cannot find watchdog device.", __func__);
	} else {
		if (watchdog_hw_running(&pmu_wdt->wdd)) {
			/* if device is active,*/
			(void)tcc_pmu_wdt_stop(&pmu_wdt->wdd);
		}
		dev_info(&pdev->dev, "cbus watchdog driver shutdown.");
	}
}

#ifdef CONFIG_PM
/* [DR]
 * tcc_pmu_wdt_pm_suspend is call back of tcc_pmu_wdt_pm_ops.suspend
 * pm driver declare suspend function as
 * ‘int (*)(struct device *)’
 */
static int tcc_pmu_wdt_pm_suspend(struct device *dev)
{
	/* [DR]
	 * Function xxx_get_drvdata usually need to cast void* to others.
	 */
	struct tcc_watchdog_device *pmu_wdt = dev_get_drvdata(dev);
	int ret = 0;

	if (pmu_wdt == NULL) {
		dev_err(dev, "%s: pmu_wdt is NULL", __func__);
		ret = -ENODEV;
	} else {
		(void)tcc_pmu_wdt_stop(&pmu_wdt->wdd);
		dev_info(dev, "%s\n", __func__);
	}

	return ret;
}

/* [DR]
 * tcc_pmu_wdt_pm_resume is call back of tcc_pmu_wdt_pm_ops.resume
 * pm driver declare resume function as
 * ‘int (*)(struct platform_device *, pm_message_t)’
 */
static int tcc_pmu_wdt_pm_resume(struct device *dev)
{
	/* [DR]
	 * Function xxx_get_drvdata usually need to cast void* to others.
	 */
	struct tcc_watchdog_device *pmu_wdt = dev_get_drvdata(dev);
	int ret = 0;

	if (pmu_wdt == NULL) {
		dev_err(dev, "%s: pmu_wdt is NULL", __func__);
		ret = -ENODEV;
	} else {
		(void)tcc_pmu_wdt_set_pretimeout(
				&pmu_wdt->wdd, pmu_wdt->wdd.pretimeout);
		(void)tcc_pmu_wdt_set_timeout(
				&pmu_wdt->wdd, pmu_wdt->wdd.timeout);
		(void)tcc_pmu_wdt_start(&pmu_wdt->wdd);
		dev_info(dev, "%s\n", __func__);
	}

	return ret;
}
#else
#define tcc_wdt_pm_suspend	NULL
#define tcc_wdt_pm_resume	NULL
#endif

static const struct of_device_id tcc_pmu_wdt_of_match[] = {
	{.compatible = "telechips,tcc-pmu-wdt",	},
	{ },
};

MODULE_DEVICE_TABLE(of, tcc_pmu_wdt_of_match);

static const struct dev_pm_ops tcc_pmu_wdt_pm_ops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(tcc_pmu_wdt_pm_suspend, tcc_pmu_wdt_pm_resume)
};

static struct platform_driver tcc_pmu_wdt_driver = {
	.probe		= tcc_pmu_wdt_probe,
	.remove		= tcc_pmu_wdt_remove,
	.shutdown	= tcc_pmu_wdt_shutdown,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "tcc-pmu-wdt",
		.pm	= &tcc_pmu_wdt_pm_ops,
		.of_match_table	= tcc_pmu_wdt_of_match,
	},
};

/* [DR]
 * Kernel API module_platform_driver has defects it's inside.
 */
module_platform_driver(tcc_pmu_wdt_driver);

/* [DR]
 * Kernel API MODULE_xxxx  has defects it's inside.
 */
MODULE_AUTHOR("Telechips Corporation");
MODULE_DESCRIPTION("Telechips PMU Watchdog Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:tcc-wdt");
