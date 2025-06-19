// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pwm.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/irqflags.h>

/* Debugging stuff */
#undef PWM_DEBUG
#ifdef PWM_DEBUG
#define dprintk(msg...) pr_err("pwm-tcc: " msg)
#else
#define dprintk(msg...)	do {} while (0)
#endif

#if defined(CONFIG_ARCH_TCC807X)
#define NUM_PWM 6
#else
#define NUM_PWM 4
#endif

#define PWMEN				(0x4U)
#define PWMMODE				(0x8UL)
#define PWMPSTN1(CH)		(0x10U + (0x10U * (CH)))
#define PWMPSTN2(CH)		(0x14U + (0x10U * (CH)))
#define PWMPSTN3(CH)		(0x18U + (0x10U * (CH)))
#define PWMPSTN4(CH)		(0x1CU + (0x10U * (CH)))

#define PWMOUT1(CH)			(0x50U + (0x10U * (CH)))
#define PWMOUT2(CH)			(0x54U + (0x10U * (CH)))
#define PWMOUT3(CH)			(0x58U + (0x10U * (CH)))
#define PWMOUT4(CH)			(0x5CU + (0x10U * (CH)))
#define PWMOUT5(CH)			(0x30U + (0x10U * (CH)))

#define PHASE_MODE			(1U)
#define REGISTER_OUT_MODE	(2U)

#define PWM_DIVID_MAX		(3U)	// clock divide max value 3(divide 16)
#define PWM_PERI_CLOCK		(400U * 1000U * 1000U)	// 400Mhz

#if defined(CONFIG_ARCH_TCC802X) || defined(CONFIG_ARCH_TCC803X)	\
	|| defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC807X) \
	|| defined(CONFIG_ARCH_TCN100X)
#define TCC_USE_GFB_PORT
#endif

#if defined(CONFIG_ARCH_TCN100X)
#define BIT_SHIFT 0
#else
#define BIT_SHIFT 8
#endif

#define pwm_writel		__raw_writel
#define pwm_readl		__raw_readl

struct tcc_chip {
	struct pwm_chip chip;
	struct platform_device *pdev;
	void __iomem *pwm_base;
	void __iomem *io_pwm_base;
	uint32_t pwm_num;
	uint32_t pwm_num_id;
	struct clk *pwm_pclk;
	struct clk *pwm_ioclk;	//io power control
#ifdef TCC_USE_GFB_PORT
	void __iomem *io_pwm_port_base;
	uint32_t gfb_port;
#endif
	uint32_t freq;
};

static int32_t tcc_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct pwm_chip *tmp = chip;
	struct tcc_chip *tcc;
	ulong flags;
	tcc = container_of(tmp, struct tcc_chip, chip);

	dprintk("%s npwn:%d hwpwm:%d  regs:0x%p : 0x%x\n",
		__func__, chip->npwm, tcc->pwm_num, tcc->pwm_base + PWMEN,
		pwm_readl(tcc->pwm_base + PWMEN));

	local_irq_save((flags));
	if (tcc->pwm_num < (unsigned int)NUM_PWM) {
		pwm_writel(pwm_readl(tcc->pwm_base + PWMEN) | 
				((uint32_t)(0x00010UL) << (tcc->pwm_num_id)),
				tcc->pwm_base + PWMEN);
		pwm_writel(pwm_readl(tcc->pwm_base + PWMEN) | 
				((uint32_t)(0x00011UL) << (tcc->pwm_num_id)),
				tcc->pwm_base + PWMEN);
		pwm_writel(pwm_readl(tcc->pwm_base + PWMEN) | 
				((uint32_t)(0x10011UL) << (tcc->pwm_num_id)),
				tcc->pwm_base + PWMEN);
	}
	pwm_writel((uint32_t)0x000, tcc->io_pwm_base);

	local_irq_restore(flags);

	return 0;
}

static void tcc_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct pwm_chip *tmp = chip;
	struct tcc_chip *tcc;
	ulong flags;

	tcc = container_of(tmp, struct tcc_chip, chip);
	dprintk("%s npwn:%d hwpwm:%d\n", __func__, chip->npwm, tcc->pwm_num);

	local_irq_save((flags));
	if (tcc->pwm_num < (unsigned int)NUM_PWM) {
		pwm_writel(pwm_readl(tcc->pwm_base + PWMEN) & 
			~((uint32_t)1 << (tcc->pwm_num_id)), tcc->pwm_base + PWMEN);
	}
	local_irq_restore(flags);

}

static void tcc_pwm_wait(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct pwm_chip *tmp = chip;
	struct tcc_chip *tcc;
	uint32_t busy;
	uint32_t delay_cnt = 0xFFFFFFF;

	tcc = container_of(tmp, struct tcc_chip, chip);

	while (delay_cnt != 0UL) {
		busy = pwm_readl(tcc->pwm_base);
		if (tcc->pwm_num < (unsigned int)NUM_PWM) {
			if ((busy & ((uint32_t)0x1 << tcc->pwm_num_id)) ==
				(uint32_t)0) {
				break;
			}
		}
		delay_cnt--;
	}
	dprintk("%s hwpwm:%d  delay_cnt:%d\n",
			__func__, tcc->pwm_num, delay_cnt);
}

static void tcc_pwm_register_mode_set(struct pwm_chip *chip,
		struct pwm_device *pwm, uint32_t regist_value)
{
	struct pwm_chip *tmp = chip;
	struct tcc_chip *tcc;
	uint32_t reg;
	uint32_t bit_shift;
	
	tcc = container_of(tmp, struct tcc_chip, chip);

	bit_shift = (uint32_t)4 * tcc->pwm_num_id;
	reg = pwm_readl(tcc->pwm_base + PWMMODE);
	if (bit_shift < (uint32_t)32) {
		if (((reg >> bit_shift) & (uint32_t)0xFu) !=
			(uint32_t)(REGISTER_OUT_MODE)) {
			tcc_pwm_disable(chip, pwm);
			tcc_pwm_wait(chip, pwm);
		}
		reg = ((reg & ~((uint32_t)0xF << bit_shift)) |
			 ((uint32_t)REGISTER_OUT_MODE << bit_shift));
		pwm_writel(reg, tcc->pwm_base + PWMMODE);	//phase mode
	}
	reg = pwm_readl(tcc->pwm_base + PWMMODE);
	if (tcc->pwm_num < (unsigned int)NUM_PWM){
		bit_shift = ((uint32_t)2 * (unsigned int)tcc->pwm_num_id) + (uint32_t)24;
	}
	/* divide by 2 : default value */
	if (bit_shift < (uint32_t)32) {
		reg = (reg & ~((uint32_t)0x3 << bit_shift));
	}
	pwm_writel(reg, tcc->pwm_base + PWMMODE);
	if (tcc->pwm_num > 3) {
		pwm_writel(regist_value, tcc->pwm_base + PWMOUT5(tcc->pwm_num_id));
	} else {
		pwm_writel(regist_value, tcc->pwm_base + PWMOUT1(tcc->pwm_num_id));
	}
}

static int32_t pwm_hi_out(struct pwm_chip *chip, struct pwm_state *state, struct pwm_device *pwm, int *flag, unsigned long flags)
{
	local_irq_save((flags));
	tcc_pwm_register_mode_set(chip, pwm, (uint32_t)0xFFFFFFFFu);
	pwm_get_state(pwm, state);
	if (state->enabled) {
		(void)tcc_pwm_enable(chip, pwm);
	}
	local_irq_restore(flags);
	*flag = 1;
	return 0;
}

static int32_t pwm_low_out(struct pwm_chip *chip,struct pwm_state *state, struct pwm_device *pwm, int *flag, unsigned long flags)
{
	local_irq_save((flags));
	tcc_pwm_register_mode_set(chip, pwm, (uint32_t)0x00000000u);
	pwm_get_state(pwm, state);
	if (state->enabled) {
		(void)tcc_pwm_enable(chip, pwm);
	}
	local_irq_restore(flags);
	*flag = 1;
	return 0;
}

static int32_t clk_error(unsigned int clk_freq, int *flag)
{
	(void)pr_err("%s ERROR clk_freq:%u\n", __func__, clk_freq);
	*flag = 1;
	return -1;
}

static int32_t tcc_pwm_config(struct pwm_chip *chip, struct pwm_device *pwm,
			  int32_t duty_ns, int32_t period_ns_)
{
	struct pwm_state state;
	struct pwm_chip *tmp = chip;
	struct tcc_chip *tcc;
	uint32_t k = 0;
	uint32_t reg;
	uint32_t bit_shift;
	uint32_t clk_freq;
	uint64_t divide;
	uint32_t cal_duty;
	uint32_t cal_period = 1;
	ulong flags = 0;
	uint32_t hi_cnt = 0;
	uint32_t low_cnt = 0;
	uint64_t total_cnt = 0;
	uint64_t clk_period_ns;
	uint64_t period_ns = 0;
	uint64_t result;
	int flag= 0;
	int ret = 0;

#ifdef TCC_USE_GFB_PORT
	uint32_t gfb_port_value;
#endif
	if (period_ns_ > 0) {
		period_ns = (uint64_t) period_ns_;
	}
	tcc = container_of(tmp, struct tcc_chip, chip);
	
	clk_freq = (uint32_t)clk_get_rate(tcc->pwm_pclk);
	if (duty_ns > 0) {
		if ((clk_freq == (uint32_t)0) || ((uint64_t) duty_ns > period_ns)) {
			ret = clk_error(clk_freq, &flag);
		}
	}
	if(flag == 0) {
		clk_period_ns = div_u64((1000UL * 1000UL * 1000UL), clk_freq);

		dprintk(
				"%s clk_freq:%u  npwn:%d duty_ns:%d period_ns:%llu hwpwm:%d\n",
			 __func__, clk_freq, chip->npwm, duty_ns, period_ns, tcc->pwm_num);

#ifdef TCC_USE_GFB_PORT
		gfb_port_value = pwm_readl(tcc->io_pwm_port_base);
		if (tcc->pwm_num < (uint32_t)NUM_PWM) {
			gfb_port_value =
				((gfb_port_value &
				  ~((uint32_t)0xFF << ((uint32_t)BIT_SHIFT * tcc->pwm_num_id)))
				 | ((tcc->gfb_port & (uint32_t)0xFF) <<
				((uint32_t)BIT_SHIFT * tcc->pwm_num_id)));
		}
		pwm_writel(gfb_port_value, tcc->io_pwm_port_base);
#endif

		if (duty_ns == 0) {
			ret = pwm_low_out(chip, &state, pwm, &flag, flags);
		}
		else if ((uint64_t) duty_ns == period_ns) {
			ret = pwm_hi_out(chip, &state, pwm, &flag, flags);
		} 
		else {	
			while (true) {
				if (clk_period_ns <= (ULLONG_MAX / 2ULL)) {
					clk_period_ns = clk_period_ns * (2UL);
					if (clk_period_ns <= (uint64_t)0xFFFFFFFFU) {
						total_cnt = div_u64(period_ns, (uint32_t)clk_period_ns);
					}
				}

				if (total_cnt <= 1UL) {
					if ((uint64_t) duty_ns > (period_ns / 2UL)) {
						ret = pwm_hi_out(chip, &state, pwm, &flag, flags);
					} else {
						ret = pwm_low_out(chip, &state, pwm, &flag, flags);
					}
				}

				if ((flag == 1) || (k == PWM_DIVID_MAX) || (total_cnt <= 0xFFFFFFFFUL)) {
					break;
				}
				k++;
			}
			if (flag == 0) {
				//prevent over flow.
				for (divide = 1; divide < 0xFFFFFFFFu; divide++) {
					// 0xFFFFFFFF > total_cnt * duty / divide
					if ((div_u64(ULLONG_MAX, (uint32_t)duty_ns)) >
						(div_u64(total_cnt, (uint32_t)divide))) {
						break;
					}
				}

				cal_duty = ((uint32_t)duty_ns / (uint32_t)(divide & 0xFFFFFFFFu));
				result = div_u64(period_ns, (uint32_t)divide);
				if (result <= (uint64_t)0xFFFFFFFFU) {
					cal_period = (uint32_t)result;
				}
				if(total_cnt > (uint64_t)0) {
					if ((ULLONG_MAX / total_cnt) >= (uint64_t)cal_duty) {
						result = div_u64((total_cnt * (uint64_t)(cal_duty)), (cal_period));	
					}
				}
				if (result <= (uint64_t)0xFFFFFFFFU) {
					hi_cnt = (uint32_t)result;
					if(total_cnt <= (uint64_t)0xFFFFFFFFU) {
						low_cnt = (uint32_t)total_cnt - hi_cnt;
					}
				}

			//pwm_result:
				dprintk("k: %d clk_p: %llu cnt : total :%llu, hi:%d , low:%d\n",
						k, clk_period_ns, total_cnt, hi_cnt, low_cnt);

				local_irq_save((flags));

				reg = pwm_readl(tcc->pwm_base + PWMMODE);

				bit_shift = (uint32_t)4 * tcc->pwm_num_id;
				if (((reg >> bit_shift) & (uint32_t)0xF) != PHASE_MODE) {
					tcc_pwm_disable(chip, pwm);
					tcc_pwm_wait(chip, pwm);
				}

				reg = (reg & ~((uint32_t)0xF << bit_shift)) |
					((uint32_t)PHASE_MODE << bit_shift);//phase mode
				pwm_writel(reg, tcc->pwm_base + PWMMODE);

				bit_shift = ((uint32_t)2 * tcc->pwm_num_id) + (uint32_t)24;
				if(bit_shift < (uint32_t)32) {
					reg = (reg & ~((uint32_t)0x3 << bit_shift)) | (k << bit_shift);
				}
				pwm_writel(reg, tcc->pwm_base + PWMMODE);	//divide
				
				pwm_writel(low_cnt, tcc->pwm_base + PWMPSTN1(tcc->pwm_num_id));

				pwm_writel(hi_cnt, tcc->pwm_base + PWMPSTN2(tcc->pwm_num_id));

				reg = pwm_readl(tcc->pwm_base + PWMMODE);
				bit_shift = ((uint32_t)1 << tcc->pwm_num_id) + (uint32_t)16;
				if(bit_shift < (uint32_t)32) {
					reg = (reg & ~((uint32_t)0x1 << bit_shift)); //signal inverse clear
				}
				pwm_writel(reg, tcc->pwm_base + PWMMODE);

				pwm_get_state(pwm, &state);
				if (state.enabled) {
					(void)tcc_pwm_enable(chip, pwm);
				}

				local_irq_restore(flags);
			}
		}
	}
	return ret;
}

static struct pwm_ops tcc_pwm_ops = {
	.enable = tcc_pwm_enable,
	.disable = tcc_pwm_disable,
	.config = tcc_pwm_config,
	.owner = THIS_MODULE,
};

static int32_t tcc_pwm_probe(struct platform_device *pdev)
{
	struct tcc_chip *tcc;
	uint32_t freq;
	int32_t ret;
	int result = 0;

	dprintk(KERN_INFO " %s\n", __func__);

	tcc = devm_kzalloc(&pdev->dev, sizeof(*tcc), GFP_KERNEL);
	
	if (tcc == NULL) {
		result = -ENOMEM;
	}
	else {
		tcc->pwm_base = of_iomap(pdev->dev.of_node, 0);
		if (IS_ERR(tcc->pwm_base)) {
			result = (int32_t)PTR_ERR(tcc->pwm_base);
		}
		else {
			tcc->io_pwm_base = of_iomap(pdev->dev.of_node, 1);
			if (IS_ERR(tcc->io_pwm_base)) {
				result = (int32_t)PTR_ERR(tcc->io_pwm_base);
			}
			else {
				tcc->pwm_pclk = of_clk_get(pdev->dev.of_node, 0);
				if (IS_ERR((void *)tcc->pwm_pclk)) {
					dev_err(&pdev->dev, "Unable to get pwm peri clock.\n");
					result = -ENODEV;
				}
				else {
					ret = of_property_read_u32(pdev->dev.of_node, "clock-frequency", &freq);
					if (ret < 0) {
						freq = (uint32_t)PWM_PERI_CLOCK;
						dev_info(&pdev->dev, "pwm default clock :%u init",
							 PWM_PERI_CLOCK);
					} else {
						dev_info(&pdev->dev, "pwm default clock :%u init", freq);
					}

					tcc->freq = freq;
					tcc->pwm_ioclk = of_clk_get(pdev->dev.of_node, 1);
					if (IS_ERR((void *)tcc->pwm_ioclk)) {
						dev_err(&pdev->dev, "Unable to get pwm io clock.\n");
						result = -ENODEV;
					}
					else {
						tcc->chip.dev = &pdev->dev;
						tcc->chip.ops = &tcc_pwm_ops;
						tcc->chip.base = -1;

						ret =
							of_property_read_u32(pdev->dev.of_node, "tcc,pwm-number",
									 &tcc->chip.npwm);
						if (ret < 0) {
							dev_err(&pdev->dev, "failed  ret : %d  get pwm number: %d\n",
								ret, tcc->chip.npwm);
							result = ret;
						}
						else {
							if (tcc->chip.npwm > (uint32_t)1) {
								dev_err(&pdev->dev, "npwm exceeded the maximum value(1).: %d\n",
									tcc->chip.npwm);
								result = -EINVAL;
							}
							else {
								dev_info(&pdev->dev, "get pwm number: %d\n", tcc->chip.npwm);
								ret =
									of_property_read_u32(pdev->dev.of_node, "pwm-num",
											&tcc->pwm_num);
								if (ret < 0) {
									dev_err(&pdev->dev, "failed  ret : %d  get pwm-num: %u\n",
											ret, tcc->pwm_num);
									result = ret;
								} else {
									dev_info(&pdev->dev, "get pwm-num: %d\n", tcc->pwm_num);
#ifdef TCC_USE_GFB_PORT
									ret =
										of_property_read_u32(pdev->dev.of_node, "gfb-port",&tcc->gfb_port);
									if (ret < 0) {
										dev_err(&pdev->dev, "failed  ret : %d  get gfb-port: %u\n",
												ret, tcc->gfb_port);
										result = ret;
									}
									dev_info(&pdev->dev, "gfb-port : %u\n"
											,tcc->gfb_port);
									tcc->io_pwm_port_base = of_iomap(pdev->dev.of_node, 2);
									if (IS_ERR(tcc->io_pwm_port_base)) {
										result = (int32_t)PTR_ERR(tcc->io_pwm_port_base);
									}
									else {
#endif
										tcc->chip.base = tcc->pwm_num;
										tcc->pwm_num_id = tcc->pwm_num % 4;
										ret = pwmchip_add(&tcc->chip);
										if (ret < 0) {
											dev_err(&pdev->dev, "pwmchip_add() failed: %d\n", ret);
											result = ret;
										}
										else {
										/*	tcc_pwm_disable(&tcc->chip, tcc->chip.pwms); */
											(void)clk_prepare_enable(tcc->pwm_ioclk);
											(void)clk_prepare_enable(tcc->pwm_pclk);
											(void)clk_set_rate(tcc->pwm_pclk, (unsigned long)freq);
											dprintk("pwm peri clock set :%u return :%lu\n",
													freq, clk_get_rate(tcc->pwm_pclk));
											platform_set_drvdata(pdev, tcc);
											//Make to disable
											if ((pwm_readl(tcc->pwm_base) & 0xFUL) == 0UL) {
												pwm_writel((uint32_t)0, tcc->pwm_base + PWMEN);
											}
											(void)pinctrl_pm_select_default_state(&pdev->dev);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return result;
}

static int32_t tcc_pwm_remove(struct platform_device *pdev)
{
	struct tcc_chip *tcc;

	dprintk("%s\n", __func__);

	tcc = (struct tcc_chip *)platform_get_drvdata(pdev);

	(void)clk_disable_unprepare(tcc->pwm_pclk);
	clk_put(tcc->pwm_pclk);
	clk_put(tcc->pwm_ioclk);

	return pwmchip_remove(&tcc->chip);
}

#ifdef CONFIG_PM_SLEEP
static int32_t tcc_pwm_suspend(struct device *dev)
{
/* struct tcc_chip *tcc = platform_get_drvdata(dev); */

	(void)pinctrl_pm_select_sleep_state(dev);

	dprintk("%s\n", __func__);
	return 0;
}

static int32_t tcc_pwm_resume(struct device *dev)
{
	struct tcc_chip *tcc;
	struct pwm_device *pwm;
	struct pwm_state state;
	uint32_t i;

	dprintk("### [%s] %d ###\n", __func__, __LINE__);

	tcc = (struct tcc_chip *)dev_get_drvdata(dev);
	(void)pinctrl_pm_select_default_state(dev);

	(void)clk_prepare_enable(tcc->pwm_ioclk);
	(void)clk_prepare_enable(tcc->pwm_pclk);

	(void)clk_set_rate(tcc->pwm_pclk, tcc->freq);

	dprintk("pwm peri clock set :%d return :%lu\n",
			tcc->freq, clk_get_rate(tcc->pwm_pclk));

	//Make to disable
	if ((pwm_readl(tcc->pwm_base) & 0xFUL) == 0UL) {
		pwm_writel(0, tcc->pwm_base + PWMEN);
	}

	for (i = 0; i < tcc->chip.npwm; i++) {
		pwm = &tcc->chip.pwms[i];
		dprintk("pwms[%d]->hwpwm=%d,pwm=%d,flags=%lu,label=%s\n",
			i, tcc->pwm_num, pwm->pwm, pwm->flags, pwm->label);

		pwm_get_state(pwm, &state);
		dprintk(" state: %s", state.enabled ? "enabled" : "disabled");
		dprintk(" period: %llu ns", state.period);
		dprintk(" duty: %llu ns", state.duty_cycle);
		dprintk(" polarity: %s", state.polarity ? "inverse" : "normal");

		if (state.enabled) {
			if ((state.duty_cycle <= 0xFFFFFFFFU) && (state.period <= 0xFFFFFFFFU)) {
				(void)tcc_pwm_config(&tcc->chip, pwm,
						(int32_t)state.duty_cycle,
						(int32_t)state.period);
				(void)tcc_pwm_enable(&tcc->chip, pwm);
			}
		}
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS((tcc_pwm_pm_ops), (tcc_pwm_suspend), (tcc_pwm_resume));
#endif

static const struct of_device_id tcc_pwm_of_match[2] = {
	{.compatible = "telechips,pwm"},
	{.compatible = ""}
};

MODULE_DEVICE_TABLE(of, tcc_pwm_of_match);

static struct platform_driver tcc_pwm_driver = {
	.driver = {
		   .name = "tcc-pwm",
#ifdef CONFIG_PM_SLEEP
		   .pm = &tcc_pwm_pm_ops,
#endif
		   .of_match_table = tcc_pwm_of_match,
		   },
	.probe = tcc_pwm_probe,
	.remove = tcc_pwm_remove,
/*
 * .suspend        = tcc_pwm_suspend,
 * .resume         = tcc_pwm_resume,
 */

};

module_platform_driver(tcc_pwm_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Telechips Corporation");
MODULE_ALIAS("platform:tcc-pwm");
