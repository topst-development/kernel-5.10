// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/clk.h>
#include <linux/clocksource.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/syscore_ops.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <soc/telechips/timer_reg.h>
#include <soc/telechips/timer_api.h>

#define TCC_TIMER_NAME          ((const s8 *)"tcc_timer")
#define timer_readl             (__raw_readl)
#define timer_writel            (__raw_writel)

#define TCC_TIMER_TC0           (0)
#define TCC_TIMER_TC1           (1)
#define TCC_TIMER_TC2           (2)
#define TCC_TIMER_TC3           (3)
#define TCC_TIMER_TC4           (4)
#define TCC_TIMER_TC5           (5)
#define TCC_TIMER_MAX           ((u32)6)
#define MAX_TCKSEL              (6)

#define TREF_LPO_REF            ((u32)0x5B)  /* based on using 12MHz TCLK */

static void __iomem *timer_base;
static unsigned long clk_rate;
static struct tcc_timer timer_res[TCC_TIMER_MAX];
static u32 tco_id;
static DEFINE_SPINLOCK(tcc_timer_lock);

#ifdef CONFIG_TCC_MICOM
extern s32 is_micom_timer(u32 ch);
#endif

static irqreturn_t tcc_timer_handler(s32 irq, void *data)
{
	struct tcc_timer *timer = (struct tcc_timer *)data;
	irqreturn_t ret = IRQ_NONE;

	if (timer == NULL) {
		(void)pr_err("[ERR][%s] %s: has no timer structure\n",
			     TCC_TIMER_NAME, __func__);
	} else if (timer->id >= TCC_TIMER_MAX) {
		(void)pr_err("[ERR][%s] %s: wrong timer id:%d\n",
			     TCC_TIMER_NAME, __func__, timer->id);
	} else {
		if ((timer_readl(timer_base + TCC_TIREQ) & ((u32)1 << timer->id)) != (u32)0) {
			if (timer->irqcnt < (UINT_MAX - (u32)1)) {
				timer->irqcnt++;
			} else {
				timer->irqcnt = 0;
			}
			timer_writel(((u32)1 << ((u32)8 + timer->id)) | ((u32)1 << timer->id),
				     timer_base + TCC_TIREQ);
			if (timer->handler != NULL) {
				ret = timer->handler(irq, data);
			} else {
				ret = IRQ_HANDLED;
			}
		}
	}

	return ret;
}

static void tcc_timer_tco_enable(u32 id)
{
	if (id >= TCC_TIMER_MAX) {
		(void)pr_err("[ERROR][%s] %s: wrong timer id:%d\n",
			     TCC_TIMER_NAME, __func__, id);
	} else {
		/* Set TREF to generate 32.768KHz approximately */
		timer_writel(TREF_LPO_REF, timer_base + (id * TIMER_OFFSET) + TIMER_TREF);
		/* Enable Timer with TCKSEL=0 */
		timer_writel(1, timer_base + (id * TIMER_OFFSET) + TIMER_TCFG);
	}
}

s32 tcc_timer_enable(struct tcc_timer *timer)
{
	void __iomem *reg;
	s32 ret = -1;

	if (timer == NULL) {
		ret = -ENODEV;
	} else if (timer->id >= TCC_TIMER_MAX) {
		(void)pr_err("[ERROR][%s] %s: wrong timer id:%d\n",
			     TCC_TIMER_NAME, __func__, timer->id);
		ret = -EINVAL;
	} else {
		reg = timer_base + (timer->id * (u32)0x10);
		timer->irqcnt = 0;
		timer_writel(0x0, reg + TIMER_TCNT);
		timer_writel(timer_readl(reg + TIMER_TCFG) | (TCFG_IEN | TCFG_EN),
			     reg + TIMER_TCFG);
		ret = 0;
	}

	return ret;
}
EXPORT_SYMBOL(tcc_timer_enable);

s32 tcc_timer_disable(struct tcc_timer *timer)
{
	void __iomem *reg;
	s32 ret = -1;

	if (timer == NULL) {
		ret = -ENODEV;
	} else if (timer->id >= TCC_TIMER_MAX) {
		(void)pr_err("[ERROR][%s] %s: wrong timer id:%d\n",
			     TCC_TIMER_NAME, __func__, timer->id);
		ret = -EINVAL;
	} else {
		reg = timer_base + (timer->id * (u32)0x10);

		timer_writel(timer_readl(reg + TIMER_TCFG) & ~(TCFG_IEN | TCFG_EN),
			     reg + TIMER_TCFG);
		timer_writel(0x0, reg + TIMER_TCNT);
		timer->irqcnt = 0;
		ret = 0;
	}

	return ret;
}
EXPORT_SYMBOL(tcc_timer_disable);

/*
 * return the timer irq count.
 */
u32 tcc_get_timer_count(const struct tcc_timer *timer)
{
	u32 cnt = 0;
	unsigned long flags;

	if (timer != NULL) {
		spin_lock_irqsave(&tcc_timer_lock, flags);
		cnt = timer->irqcnt;
		spin_unlock_irqrestore(&tcc_timer_lock, flags);
	}

	return cnt;
}
EXPORT_SYMBOL(tcc_get_timer_count);

/*
 * dev: device pointer
 * usec: timer tick vaule.
 *       if usec value is 1000 then timer counter tick is 1 msec.
 * handler
 */
#define REF_HZ  ((u32)50000000)
struct tcc_timer *tcc_register_timer(struct device *dev, u32 usec, irq_handler_t handler)
{
	s32 ret = -1;
	s32 i;
	s32 k;
	u32 max_ref;
	u32 srch_k;
	u32 ref[MAX_TCKSEL + 1];
	struct tcc_timer *ptimer;
	u32 scl;
	u32 tcksel;

	if (usec == (u32)0) {
		(void)pr_err("[ERROR][%s] %s: usec is zero.\n", TCC_TIMER_NAME, __func__);
		ret = -EDOM;
	} else {
		ret = 0;
	}

	if (ret == 0) {
		/* try to use 20-bit counter. when usec is lower then 1ms */
		if (usec < (u32)1000) {
			for (i = TCC_TIMER_TC5; i >= TCC_TIMER_TC0; i--) {
				if ((timer_res[i].used == 0) && (timer_res[i].reserved == 0)) {
					break;
				}
			}
		} else {
			for (i = TCC_TIMER_TC0 ; i <= TCC_TIMER_TC5 ; i++) {
				if ((timer_res[i].used == 0) && (timer_res[i].reserved == 0)) {
					break;
				}
			}
		}

		if ((i > TCC_TIMER_TC5) || (i < (s32)TCC_TIMER_TC0)) {
			(void)pr_err("[ERROR][%s] %s: There is no available timer.\n",
				     TCC_TIMER_NAME, __func__);
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		if (i < 4) {
			max_ref = 0xFFFF;       /* 16bit counter */
		} else {
			max_ref = 0xFFFFF;      /* 20bit counter */
		}

		if (clk_rate > (ULONG_MAX/(u32)50)) {
			(void)pr_err("[ERROR][%s] %s: wrong timer clock has set.\n",
				     TCC_TIMER_NAME, __func__);
			ret = -EDOM;
		}
	}

	if (ret == 0) {
		/* find divide factor */
		scl  = clk_rate * usec / 1000000;
		srch_k = 0;

		for (k = 0; k <= MAX_TCKSEL; k++) {
			s32 max_cnt;

			max_cnt = (k < 5) ? (k + 1) : (k * 2);
			tcksel = (u32)1 << (u32)max_cnt;
			ref[k] = scl / tcksel;

			if (ref[k] <= max_ref)
			{
				srch_k = k;

				break;
			}
		}
	}

	if (ret == 0) {
		/* cannot found divide factor */
		if (k > MAX_TCKSEL) {
			k = MAX_TCKSEL;
			srch_k = (u32)k;
			ref[srch_k] = max_ref;
			(void)pr_warn("[WARN][%s] %s: cannot get the correct timer.\n",
				      TCC_TIMER_NAME, __func__);
			/* TODO: supplementary setting */
		}

		timer_res[i].used = 1;
		timer_res[i].dev = dev;
		timer_res[i].irqcnt = 0;
		timer_res[i].ref = ref[srch_k];
		timer_res[i].mref = 0;
		timer_res[i].handler = handler;
		timer_res[i].div_f = tcksel;
		
		timer_writel(TCFG_TCKSEL(srch_k),
			     timer_base + ((u32)i * TIMER_OFFSET) + TIMER_TCFG);
		timer_writel(0x0, timer_base + ((u32)i * TIMER_OFFSET) + TIMER_TCNT);
		timer_writel(timer_res[i].mref,
			     timer_base + ((u32)i * TIMER_OFFSET) + TIMER_TMREF);
		timer_writel(timer_res[i].ref,
			     timer_base + ((u32)i * TIMER_OFFSET) + TIMER_TREF);

		(void)scnprintf(timer_res[i].name, sizeof(timer_res[i].name), "timer%d", i);
		ret = request_irq(timer_res[i].virq, &tcc_timer_handler, IRQF_SHARED,
				  (const s8 *)timer_res[i].name, &timer_res[i]);
		if (ret != 0) {
			(void)pr_err("[ERROR][%s] %s: cannot request irq\n",
				     TCC_TIMER_NAME, __func__);
		}
	}

	if (ret == 0) {
		ptimer = &(timer_res[i]);
	} else {
		ptimer = ERR_PTR(ret);
	}

	return ptimer;
}
EXPORT_SYMBOL(tcc_register_timer);

void tcc_unregister_timer(const struct tcc_timer *timer)
{
	void __iomem *reg;

	if (timer == NULL) {
		(void)pr_err("[ERROR][%s] %s: wrong timer\n",
			     TCC_TIMER_NAME, __func__);
	} else if (timer->id >= TCC_TIMER_MAX) {
		(void)pr_err("[ERROR][%s] %s: wrong timer id:%d\n",
			     TCC_TIMER_NAME, __func__, timer->id);
	} else if (timer_res[timer->id].used == 0) {
		(void)pr_warn("[WARN][%s] %s: id:%d is not registered index\n",
			      TCC_TIMER_NAME, __func__, timer->id);
	} else {
		reg = timer_base + (timer->id * (u32)0x10);
		(void)free_irq(timer_res[timer->id].virq, &(timer_res[timer->id]));
		timer_writel(0x0, reg + TIMER_TCFG);
		timer_writel(0x0, reg + TIMER_TCNT);

		timer_res[timer->id].dev = NULL;
		timer_res[timer->id].used = 0;
		timer_res[timer->id].handler = NULL;
	}
}
EXPORT_SYMBOL(tcc_unregister_timer);

#if defined(CONFIG_PM_SLEEP)
#define TIMER_REG_SIZE	((u32)0xA0)
static u32 *timer_backup;

void tcc_timer_save(void)
{
	u32 i;

	if (timer_backup != NULL) {
		(void)pr_err("[ERROR][%s] %s: timer_backup is allocated already\n",
			     TCC_TIMER_NAME, __func__);
	} else {
		timer_backup = kzalloc(TIMER_REG_SIZE, GFP_KERNEL);
		if (timer_backup != NULL) {
			for (i = 0; i < (TIMER_REG_SIZE / (u32)4); i++) {
				timer_backup[i] = timer_readl(timer_base + (i * (u32)4));
			}
		}

	}
}
EXPORT_SYMBOL(tcc_timer_save);

void tcc_timer_restore(void)
{
	u32 i;

	if (timer_backup == NULL) {
		(void)pr_err("[ERROR][%s] %s: cannot find timer_backup\n",
			     TCC_TIMER_NAME, __func__);
	} else {
		for (i = 0; i < (TIMER_REG_SIZE / (u32)4); i++) {
			timer_writel(timer_backup[i], timer_base + (i * (u32)4));
		}

		kfree(timer_backup);
		timer_backup = NULL;
	}
}
EXPORT_SYMBOL(tcc_timer_restore);

#if defined(CONFIG_ARCH_TCC805X)
static s32 tcc_timer_suspend(void)
{
	/* do nothing */
	return 0;
}

static void tcc_timer_resume(void)
{
	/* enable TCO for LPO clock source */
	tcc_timer_tco_enable(tco_id);
}

static struct syscore_ops tcc_timer_syscore_ops = {
	.suspend = tcc_timer_suspend,
	.resume = tcc_timer_resume,
};
#endif
#endif  /* CONFIG_PM_SLEEP */

static s32 tcc_timer_parse_dt(struct device_node *np)
{
	struct property *prop;
	const __be32 *p;
	u32 res;
	s32 i;
	s32 ret = 0;

	for (i = 0; i < TCC_TIMER_MAX; i++) {
		int irq = of_irq_to_resource(np, i, NULL);
		if (irq < 0) {
			(void)pr_err("[ERROR][%s] %s: unable to get irq.\n",
				     TCC_TIMER_NAME, __func__);
			ret = -EINVAL;
			break;
		} else {
			timer_res[i].virq = (u32)irq;
		}
	}

	if (ret == 0) {
		timer_base = of_iomap(np, 0);
		if (timer_base == NULL) {
			(void)pr_err("[ERROR][%s] %s: unable to get timer register.\n",
				     TCC_TIMER_NAME, __func__);
			ret = -EINVAL;
		}
	}

	if (ret == 0) {
		/* check the reserved timer */
		of_property_for_each_u32(np, "tcc-timer-reserved-for-lpo", (prop), (p), (res)) {
			if (res < TCC_TIMER_MAX) {
				(void)pr_info("[INFO][%s]: reserved channel-%d for LPO clock\n",
					      TCC_TIMER_NAME, res);
				timer_res[res].reserved = 1;
				tcc_timer_tco_enable(res);
				tco_id = res;
			}
		}

		of_property_for_each_u32(np, "tcc-timer-reserved-for-wdt", (prop), (p), (res)) {
			if (res < TCC_TIMER_MAX) {
				(void)pr_info(
					      "[INFO][%s]: reserved channel-%d for watchdog reset timer\n",
					      TCC_TIMER_NAME, res);
				timer_res[res].reserved = 1;
			}
		}
	}

	return ret;
}

static s32 tcc_timer_set_clk(struct device_node *np)
{
	struct clk *timer_clk;
	u32 rate;
	s32 ret = -1;

	ret = of_property_read_u32(np, "clock-frequency", &rate);
	if (ret != 0) {
		(void)pr_warn("[WARN][%s] %s: can't read clock-frequency\n",
			      TCC_TIMER_NAME, __func__);
		rate = 12000000;
	}
	if (rate < 2000000) {
		(void)pr_warn("[WARN][%s] %s: too low clock-frequency\n",
			      TCC_TIMER_NAME, __func__);
		rate = 12000000;
	}

	timer_clk = of_clk_get(np, 0);
	if (IS_ERR(timer_clk)) {
		(void)pr_err("[ERROR][%s] %s: unable to get timer clock.\n",
			     TCC_TIMER_NAME, __func__);
		ret = -EINVAL;
	}

	if (ret == 0) {
#ifdef CONFIG_TCC_MICOM
		s32 i;
		for (i = 0; i < TCC_TIMER_MAX; i++) {
			if (is_micom_timer(i))
				break;
		}
		if (i >= TCC_TIMER_MAX)
#endif
		{
			ret = clk_set_rate(timer_clk, rate);
			if (ret != 0) {
				(void)pr_err("%s: failed to set clk rate %u.\n", __func__, rate);
				ret = -EINVAL;
			}
		}
	}

	if (ret == 0) {
		clk_rate = clk_get_rate(timer_clk);
		(void)pr_info("[INFO][%s] %s: clk_rate: %lu\n",
			      TCC_TIMER_NAME, __func__, clk_rate);

		ret = clk_prepare_enable(timer_clk);
		if (ret != 0) {
			(void)pr_err("[ERROR][%s] %s: failed to prepare and enable clk.\n",
				     TCC_TIMER_NAME, __func__);
			ret = -EINVAL;
		}
	}

	return ret;
}

/* This function only set the clock of timer-t. */
static s32 tcc_init_timer(struct device_node *np)
{
	s32 i;
	s32 ret = -1;

	(void)memset(&timer_res[0], 0, sizeof(struct tcc_timer) * TCC_TIMER_MAX);

	ret = tcc_timer_parse_dt(np);
	if (ret == 0) {
		ret = tcc_timer_set_clk(np);
	}
	if (ret == 0) {
		for (i = 1; i < (s32)TCC_TIMER_MAX; i++) {
#ifdef CONFIG_TCC_MICOM
			if (is_micom_timer(i))
				timer_res[i].used = 1;
#endif
			timer_res[i].id = (u32)i;
			if (timer_res[i].virq == (u32)0) {
				/* if it has only one irq source for the timer */
				timer_res[i].virq = timer_res[i - 1].virq;
			}
		}

#if defined(CONFIG_ARCH_TCC805X)
		register_syscore_ops(&tcc_timer_syscore_ops);
#endif
	}

	return ret;
}

static s32 tcc_timer_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;

	return tcc_init_timer(np);
}

static const struct of_device_id tcc_timer_match_table[] = {
	{ .compatible = "telechips,timer" },
	{ }
};

static struct platform_driver tcc_timer_driver = {
	.probe          = tcc_timer_probe,
	.driver         = {
		.name   = TCC_TIMER_NAME,
		.of_match_table = tcc_timer_match_table,
	},
};

static int __init tcc_timer_driver_init(void)
{
	return platform_driver_register(&tcc_timer_driver);
}
arch_initcall(tcc_timer_driver_init);

MODULE_DEVICE_TABLE(of, tcc_timer_match_table);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Telechips.com, Inc.");
MODULE_DESCRIPTION("Timer driver");
