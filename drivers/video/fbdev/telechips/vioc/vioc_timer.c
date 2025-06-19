// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <video/telechips/vioc_timer.h>
#include <video/telechips/vioc_ddicfg.h>	// is_VIOC_REMAP

struct vioc_timer_context {
	void __iomem *reg;
	struct clk *pclk;
	unsigned int unit_us;
	unsigned int suspend;
};
static struct vioc_timer_context ctx;

static unsigned int clk_parsed = 0; // Variables for clock parsing status

static int vioc_timer_check_parse_clk(void)
{
	int ret = -1;

	if (clk_parsed == 0U) {
		struct device_node *np;

		np = of_find_compatible_node(NULL, NULL, "telechips,vioc_timer");

		if (np == NULL) {
			(void)pr_info("[INFO][TIMER] disabled\n");

			ret = -EINVAL;
			goto out;
		}

		ctx.pclk  = of_clk_get_by_name(np, "timer-clk");

		if (IS_ERR(ctx.pclk)) {
			/* Prevent KCS warning */
			ret = -ENODEV;
			(void)pr_err("[ERROR][TIMER] Please check timer-clk on DT\r\n");
			goto out;
		}

		clk_parsed = 1; // Clock is parsed.
		ret = 0;
	} else {
		ret = 0;
	}

out:
	return ret;
}

/* HIS_GOTO */
static int vioc_timer_set_usec_enable(int enable, unsigned int unit_us)
{
	unsigned int xin_mhz;
	unsigned int reg_val;
	int ret;
	int temp = 0; /* avoid MISRA C-2012 Rule 10.8 */

	if (vioc_timer_check_parse_clk() < 0) {
		ret = -ENODEV;
		goto out;
	}

	temp = DIV_ROUND_UP(XIN_CLK_RATE, 1000000UL);
	xin_mhz = (unsigned int)temp;
	xin_mhz = (xin_mhz - 1U) << USEC_USECDIV_SHIFT;

	if (ctx.reg == NULL) {
		ret = -EINVAL;

		goto out;
	}

	if (IS_ERR(ctx.pclk)) {
		ret = -ENODEV;

		goto out;
	}
	if (unit_us == 0U) {
		ret = -EINVAL;

		goto out;
	}
	unit_us = (unit_us - 1U) << USEC_UINTDIV_SHIFT;

	if (enable == 1) {
		(void)clk_set_rate(ctx.pclk, (unsigned long)XIN_CLK_RATE);
		(void)clk_prepare_enable(ctx.pclk);

		reg_val = ((u32)1U << USEC_EN_SHIFT) |
			(unit_us & USEC_UINTDIV_MASK) |
			(xin_mhz & USEC_USECDIV_MASK);
	} else {
		reg_val = __raw_readl(ctx.reg + USEC);
		reg_val &= ~USEC_EN_MASK;

		clk_disable_unprepare(ctx.pclk);
	}

	__raw_writel(reg_val, ctx.reg + USEC);
	ret = 0;
out:
	return ret;
}

/* HIS_GOTO */
int vioc_timer_get_usec_enable(void)
{
	int enable = 0;
	unsigned int reg_val;

	if (ctx.reg == NULL) {

		goto out;
	}

	reg_val = __raw_readl(ctx.reg + USEC);
	if ((reg_val & USEC_EN_MASK) != 0U) {
		/* Prevent KCS warning */
		enable = 1;
	}
out:
	return enable;
}
EXPORT_SYMBOL_GPL(vioc_timer_get_usec_enable);

/* HIS_GOTO */
unsigned int vioc_timer_get_unit_us(void)
{
	unsigned int reg_val;
	unsigned int unit_us = 0;

	if (ctx.reg == NULL) {
		goto out;
	}

	if (vioc_timer_check_parse_clk() < 0) {
		goto out;
	}

	if (IS_ERR(ctx.pclk)) {
		goto out;
	}

	reg_val = __raw_readl(ctx.reg + USEC);

	unit_us = 1U + ((reg_val & USEC_UINTDIV_MASK) >> USEC_UINTDIV_SHIFT);

out:
	return unit_us;
}
EXPORT_SYMBOL_GPL(vioc_timer_get_unit_us);

/* HIS_GOTO */
unsigned int vioc_timer_get_curtime(void)
{
	unsigned int curtime = 0;

	if (ctx.reg == NULL) {

		goto out;
	}

	if (vioc_timer_check_parse_clk() < 0) {
		goto out;
	}

	curtime = __raw_readl(ctx.reg + CURTIME);
out:
	return curtime;
}
EXPORT_SYMBOL_GPL(vioc_timer_get_curtime);

/* HIS_GOTO */
int vioc_timer_set_timer(enum vioc_timer_id id, int enable, unsigned int timer_hz)
{
	int ret = -1;
	unsigned int timer_offset;
	unsigned int reg_val;

	if (ctx.reg == NULL) {
		ret = -ENODEV;

		goto out;
	}

	if (vioc_timer_check_parse_clk() < 0) {
		ret = -ENODEV;
		goto out;
	}

	switch (id) {
	case VIOC_TIMER_TIMER0:
		timer_offset = TIMER0;
		break;
	case VIOC_TIMER_TIMER1:
		timer_offset = TIMER1;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if(ret == -EINVAL) {
		goto out;
	}

	if (enable == 1) {
		reg_val =
			(DIV_ROUND_UP(XIN_CLK_RATE, timer_hz) - 1U) &
			TIMER_COUNTER_MASK;
		reg_val |= ((u32)1U << TIMER_EN_SHIFT);
	} else {
		reg_val = __raw_readl(ctx.reg + timer_offset);
		reg_val &= ~((u32)1U << TIMER_EN_SHIFT);
	}

	__raw_writel(reg_val, ctx.reg + timer_offset);
	ret = 0;
out:
	return ret;
}
EXPORT_SYMBOL_GPL(vioc_timer_set_timer);

/* HIS_GOTO */
int vioc_timer_set_timer_req(
	enum vioc_timer_id id, int enable, unsigned int units)
{
	unsigned int tireq_offset;
	unsigned int reg_val;
	int ret = -1;

	if (ctx.reg == NULL) {
		ret = -ENODEV;

		goto out;
	}

	if (vioc_timer_check_parse_clk() < 0) {
		ret = -ENODEV;
		goto out;
	}

	switch (id) {
	case VIOC_TIMER_TIREQ0:
		tireq_offset = TIREQ0;
		break;
	case VIOC_TIMER_TIREQ1:
		tireq_offset = TIREQ1;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if(ret == -EINVAL) {
		goto out;
	}

	if (enable == 1) {
		reg_val = units & TIREQ_TIME_MASK;
		reg_val |= ((u32)1U << TIREQ_EN_SHIFT);
	} else {
		reg_val = __raw_readl(ctx.reg + tireq_offset);
		reg_val &= ~((u32)1U << TIREQ_EN_SHIFT);
	}

	__raw_writel(reg_val, ctx.reg + tireq_offset);
	ret = 0;
out:
	return ret;
}
EXPORT_SYMBOL_GPL(vioc_timer_set_timer_req);

/* HIS_GOTO */
int vioc_timer_set_irq_mask(enum vioc_timer_id id)
{
	unsigned int reg_val;
	int ret = -1;

	if (ctx.reg == NULL) {
		ret = -ENODEV;

		goto out;
	}

	if (id > VIOC_TIMER_TIREQ1) {
		ret = -EINVAL;

		goto out;
	}

	reg_val = __raw_readl(ctx.reg + IRQMASK);
	reg_val |= ((u32)1U << (unsigned int)id);

	__raw_writel(reg_val, ctx.reg + IRQMASK);
	ret = 0;
out:
	return ret;
}
EXPORT_SYMBOL_GPL(vioc_timer_set_irq_mask);

/* HIS_GOTO */
int vioc_timer_clear_irq_mask(enum vioc_timer_id id)
{
	unsigned int reg_val;
	int ret = -1;

	if (ctx.reg == NULL) {
		ret = -ENODEV;

		goto out;
	}

	if (id > VIOC_TIMER_TIREQ1) {
		ret = -EINVAL;

		goto out;
	}

	reg_val = __raw_readl(ctx.reg + IRQMASK);
	reg_val &= ~((u32)1U << (unsigned int)id);

	__raw_writel(reg_val, ctx.reg + IRQMASK);
	ret = 0;
out:
	return ret;
}
EXPORT_SYMBOL_GPL(vioc_timer_clear_irq_mask);

/* HIS_GOTO */
unsigned int vioc_timer_get_irq_status(void)
{
	unsigned int reg_val;
	unsigned int ret = 0U;

	if (ctx.reg == NULL) {
		ret = 0U;

		goto out;
		/* prevent KCS warning */
	}

	reg_val = __raw_readl(ctx.reg + IRQSTAT);
	ret = reg_val;
out:
	return ret;
}
EXPORT_SYMBOL_GPL(vioc_timer_get_irq_status);

/* HIS_GOTO */
int vioc_timer_is_interrupted(enum vioc_timer_id id)
{
	unsigned int reg_val;
	int ret = 0;

	if (ctx.reg == NULL) {
		ret = 0;

		goto out;
	}

	if (id > VIOC_TIMER_TIREQ1) {
		ret = 0;

		goto out;
	}

	reg_val = __raw_readl(ctx.reg + IRQSTAT);
	ret = (((reg_val & ((u32)1U << (unsigned int)id)) != 0U) ? 1 : 0);
out:
	return ret;
}
EXPORT_SYMBOL_GPL(vioc_timer_is_interrupted);

/* HIS_GOTO */
int vioc_timer_clear_irq_status(enum vioc_timer_id id)
{
	int ret = -1;

	if (ctx.reg == NULL) {
		ret = -ENODEV;

		goto out;
	}

	if (id > VIOC_TIMER_TIREQ1) {
		ret = -EINVAL;

		goto out;
	}

	__raw_writel((u32)1U << (unsigned int)id, ctx.reg + IRQSTAT);

	ret = 0;
out:
	return ret;
}
EXPORT_SYMBOL_GPL(vioc_timer_clear_irq_status);

int vioc_timer_suspend(void)
{
	if (ctx.suspend == 0U) {
		ctx.suspend = 1U;
			if (vioc_timer_check_parse_clk() < 0) {
			goto out;
		}
		(void)vioc_timer_set_usec_enable(0, 0);
	}
out:
	return 0;
}
EXPORT_SYMBOL_GPL(vioc_timer_suspend);

int vioc_timer_resume(void)
{
	if (ctx.suspend == 1U) {
		ctx.suspend = 0U;
		if (vioc_timer_check_parse_clk() < 0) {
			goto out;
		}
		(void)vioc_timer_set_usec_enable(1, 100);
	}
out:
	return 0;
}
EXPORT_SYMBOL_GPL(vioc_timer_resume);

/* avoid HIS metric violation (HIS_CALLS) */
static void vioc_timer_set_timer_context(struct device_node *np)
{
	(void)memset(&ctx, 0, sizeof(ctx));

	ctx.reg =
		(void __iomem *)of_iomap(np, (is_VIOC_REMAP != 0U) ? 1 : 0);
	if (ctx.reg != NULL) {
		/* Prevent KCS warning */
		(void)pr_info(
			"[INFO][TIMER] vioc-timer\n");
	}

#if 0 // When vioc_timer is actually used, clk is parsed.
	ctx.pclk  = of_clk_get_by_name(np, "timer-clk");
	if (IS_ERR(ctx.pclk)) {
		/* Prevent KCS warning */
		(void)pr_err("[ERROR][TIMER] Please check timer-clk on DT\r\n");
	}
#endif
}

/* avoid HIS metric violation (HIS_CALLS) */
static void vioc_timer_set_init(void)
{
	/* Set interrupt mask for all interrupt source */
	(void)vioc_timer_set_irq_mask(VIOC_TIMER_TIMER0);
	(void)vioc_timer_set_irq_mask(VIOC_TIMER_TIMER1);
	(void)vioc_timer_set_irq_mask(VIOC_TIMER_TIREQ0);
	(void)vioc_timer_set_irq_mask(VIOC_TIMER_TIREQ1);

#if 0 // When vioc_timer is actually used, clk is parsed.
	if (vioc_timer_get_usec_enable() == 0) {
		(void)vioc_timer_set_usec_enable(1, 100U);
	} else {
		unsigned int unit_us = vioc_timer_get_unit_us();

		if (unit_us != 100U) {
			/* disable vioc timer */
			(void)vioc_timer_set_usec_enable(0, 0);

			/* reset vioc timer */
			(void)vioc_timer_set_usec_enable(1, 100U);
		}
	}
#endif
}

/* HIS_GOTO */
int vioc_timer_init(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "telechips,vioc_timer");

	if (np == NULL) {
		(void)pr_info("[INFO][TIMER] disabled\n");

		goto out;
	}

	vioc_timer_set_timer_context(np);
	/* avoid HIS metric violation (HIS_CALLS) */
	vioc_timer_set_init();
out:
	return 0;
}
EXPORT_SYMBOL_GPL(vioc_timer_init);
/* EOF */
