// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/spinlock.h>
#include <linux/iio/iio.h>
#include <linux/iio/driver.h>
#include <linux/iio/sysfs.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/io.h>

#include "tcc_adc.h"

static const char *tcc_adc_ch_name[TCC_ADC_MAX_CHANNEL] = {
	"tcc_adc_channel0",
	"tcc_adc_channel1",
	"tcc_adc_channel2",
	"tcc_adc_channel3",
	"tcc_adc_channel4",
	"tcc_adc_channel5",
	"tcc_adc_channel6",
	"tcc_adc_channel7",
	"tcc_adc_channel8",
	"tcc_adc_channel9",
	"tcc_adc_channel10",
	"tcc_adc_channel11",
	"tcc_adc_channel12",
	"tcc_adc_channel13",
	"tcc_adc_channel14",
	"tcc_adc_channel15",
};

static struct iio_chan_spec tcc_adc_iio_channels[TCC_ADC_MAX_CHANNEL];

/* Device Attribute for Debugging */
static ssize_t tccadc_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count);
IIO_DEVICE_ATTR_WO(tccadc, 0);

static struct attribute *tcc_adc_attributes[] = {
	&iio_dev_attr_tccadc.dev_attr.attr,
	NULL,
};

static const struct attribute_group tcc_adc_attribute_group = {
	.attrs = tcc_adc_attributes,
};

static void tcc_adc_channel_init(struct iio_chan_spec *channels, uint8_t last_ch)
{
	uint8_t ch;

	for (ch = 0U; ch < last_ch; ch++) {
		channels[ch].type = IIO_VOLTAGE;
		channels[ch].indexed = 1;
		channels[ch].channel = (int)ch;
		channels[ch].info_mask_separate = (long)1;
		channels[ch].datasheet_name = tcc_adc_ch_name[ch];
	}
}

static struct tcc_adc* tcc_adc_get_priv(const struct iio_dev *iiodev)
{
	return (struct tcc_adc*)iio_priv(iiodev);
}

static void tcc_adc_pmu_power_ctrl(const struct tcc_adc *adc, bool pwr_on)
{
	uint32_t reg_values;

	BUG_ON(adc == NULL);

	/* Enable ADC power */
	reg_values = readl(adc->pmu_regs);
	reg_values &= ~(((u32)1U << PMU_TSADC_PWREN_SHIFT) |
			((u32)1U << PMU_TSADC_STOP_SHIFT));

	if (pwr_on) {
		(void)clk_prepare_enable(adc->iso_clk);
		reg_values |= ((u32)1U << PMU_TSADC_PWREN_SHIFT);
		writel(reg_values, adc->pmu_regs);
	} else {
		reg_values |= ((u32)1U << PMU_TSADC_STOP_SHIFT);
		writel(reg_values, adc->pmu_regs);
		(void)clk_prepare_enable(adc->iso_clk);
	}
}

static void tcc_adc_power_down(const struct tcc_adc *adc)
{
	uint32_t reg_values;

	if (adc->soc_info->id == (u32)TCC897X) {
		if (adc->pclk != NULL) {
			clk_disable_unprepare(adc->pclk);
		}
		if (adc->hclk != NULL) {
			clk_disable_unprepare(adc->hclk);
		}
		tcc_adc_pmu_power_ctrl(adc, (bool)false);
	} else if ((adc->soc_info->id == (u32)TCC803X) ||
		   (adc->soc_info->id == (u32)TCC805X)) {
		/* disable clock for micom adc */
		reg_values = readl(adc->clk_regs);
		reg_values &= ~(ADC_CLK_OUT_EN | ADC_CLK_DIV_EN);
		writel(reg_values, adc->clk_regs);
	} else {
		(void)clk_disable_unprepare(adc->hclk);
	}
}

static void tcc_adc_power_on(const struct tcc_adc *adc)
{
	uint32_t reg_values, ps_val;
	ulong clk_rate;

	if (adc->soc_info->id == (u32)TCC897X) {
		/* enable ADC PMU power */
		tcc_adc_pmu_power_ctrl(adc, (bool)true);

		/* enable IO BUS, perhipheral clock */
		(void)clk_prepare_enable(adc->pclk);
		(void)clk_prepare_enable(adc->hclk);

		/* adc delay */
		reg_values = readl(adc->regs + ADCDLY_REG);
		reg_values &= ~ADCDLY_DELAY_MASK;
		reg_values |= ADCDLY_DELAY(adc->delay);
		writel(reg_values, adc->regs + ADCDLY_REG);

		/* calculate prescale */
		clk_rate = clk_get_rate(adc->pclk);
		ps_val = (((u32)clk_rate + (adc->ckin / 2U)) / adc->ckin) - 1U;
		BUG_ON((ps_val < 4U) || (ps_val > ADCCON_PS_VAL_MASK));

		/* enable and set prescale : stand-by mode */
		reg_values = ADCCON_PS_EN;
		reg_values |= ADCCON_PS_VAL(ps_val);
		reg_values |= ADCCON_STBY;

		/* 12bit resolution */
		if (adc->is_12bit_res) {
			reg_values |= ADCCON_12BIT_RES;
		}
		writel(reg_values, adc->regs + ADCCON_REG);
	} else if ((adc->soc_info->id == (u32)TCC803X) ||
		   (adc->soc_info->id == (u32)TCC805X)) {
		/* enable clock for micom adc */
		reg_values = readl(adc->clk_regs);
		reg_values |= ADC_CLK_OUT_EN;
		reg_values |= ADC_CLK_DIV_EN;
		writel(reg_values, adc->clk_regs);
	} else {
		(void)clk_prepare_enable(adc->hclk);
	}
}

static void tcc_adc_power_ctrl(const struct iio_dev *iiodev, bool pwr_on)
{
	const struct tcc_adc *adc = tcc_adc_get_priv(iiodev);

	if (pwr_on) {
		tcc_adc_power_on(adc);
	} else {
		tcc_adc_power_down(adc);
	}
}

static void tcc_adc_set_normal_mode(const struct tcc_adc *adc)
{
	uint32_t reg_values;
	bool use_touch = (bool)false;

#ifdef CONFIG_TOUCHSCREEN_TCCTS
	use_touch = (bool)true;
#endif
	if (!use_touch) {
		/* set normal conversion mode */
		reg_values = readl(adc->regs + ADCTSC_REG);
		reg_values &= ~ADCTSC_MASK;
		reg_values |= (ADCTSC_PUON | ADCTSC_XPEN | ADCTSC_YPEN);
		writel(reg_values, adc->regs + ADCTSC_REG);
	}
}

static int32_t tcc_adc_read_v1(const struct tcc_adc *adc, uint32_t ch)
{
	uint32_t data, reg_values;

	/* If touch screen ADC is not enabled, set normal conversion mode */
	tcc_adc_set_normal_mode(adc);

	/* change operation mode */
	reg_values = readl(adc->regs + ADCCON_REG);
	reg_values &= ~ADCCON_STBY;
	writel(reg_values, adc->regs + ADCCON_REG);

	/* clear input channel select */
	reg_values &= ~(ADCCON_ASEL(15U));
	writel(reg_values, adc->regs + ADCCON_REG);
	ndelay(adc->conv_time_ns);

	/* select input channel, enable conversion */
	reg_values |= (ADCCON_ASEL(ch) | ADCCON_EN_ST);
	writel(reg_values, adc->regs + ADCCON_REG);

	/* Wait for Start Bit Cleared */
	while ((readl(adc->regs + ADCCON_REG) & ADCCON_EN_ST) != 0U) {
		ndelay(5);
	}

	/* Wait for ADC Conversion Ended */
	while (((readl(adc->regs + ADCCON_REG) & ADCCON_E_FLG) == 0U)) {
		ndelay(5);
	}

	/* Read Measured data */
	if (adc->is_12bit_res) {
		data = readl(adc->regs + ADCDAT0_REG) & ADCDATA_MASK;
	} else {
		data = readl(adc->regs + ADCDAT0_REG) & 0x3FFU;
	}

	/* clear input channel select, disable conversion */
	reg_values &= ~(ADCCON_ASEL(15U) | ADCCON_EN_ST);
	writel(reg_values, adc->regs + ADCCON_REG);

	/* chagne stand-by mode */
	reg_values |= ADCCON_STBY;
	writel(reg_values, adc->regs + ADCCON_REG);

	return (int32_t)data;
}

static int32_t tcc_adc_read_v2(const struct tcc_adc *adc, uint32_t ch)
{
	uint32_t data, reg_values;
	int32_t ret, retry, max_count = 100;

	if (ch <= adc->soc_info->last_ch) {
		reg_values = ADCCMD_SMP_CMD(ch);

		for (retry = 0; retry < 2; retry++) {
			/* sampling Command */
			writel(reg_values, adc->regs + ADCCMD);

			/* wait for sampling completion */
			while (((readl(adc->regs + ADCCMD) & ADCCMD_DONE) == 0U)
					&& (max_count > 0)) {
				udelay(1);
				max_count--;
			}
			if (max_count > 0) {
				/* read the converted value of adc */
				data = readl(adc->regs + ADCDATA0) & ADCDATA_MASK;
				dev_dbg(adc->dev, "[DEBUG][ADC] %s: data = %#X\n",
						__func__, data);
				ret = (int32_t)data;
			} else {
				/* time out - sampling completion */
				dev_warn(adc->dev, "[WARN][ADC] %s: ADC CH %u Sampling Command is not completed.\n",
						__func__, ch);
				ret = -EIO;
				break;
			}

			max_count = 100;
		}
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static int32_t tcc_adc_read(const struct tcc_adc *adc, uint32_t ch)
{
	int32_t ret = 0;

	dev_dbg(adc->dev, "[DEBUG][ADC] %s:\n", __func__);

	if (ch <= adc->soc_info->last_ch) {
		if (adc->soc_info->id == TCC897X) {
			ret = tcc_adc_read_v1(adc, ch);
		} else {
			ret = tcc_adc_read_v2(adc, ch);
		}
	} else {
		dev_err(adc->dev,
			"[ERROR][ADC] %s: ADC CH %u is not supported.\n",
			__func__,
			ch);
		ret = -EINVAL;
	}

	return ret;
}

static int32_t tcc_adc_read_raw(struct iio_dev *iiodev,
		struct iio_chan_spec const *chan,
		int32_t *val, int32_t *val2, long info)
{
	struct tcc_adc *adc = tcc_adc_get_priv(iiodev);
	int32_t ret = -EINVAL;

	(void)val2;

	switch (info) {
	case (long)IIO_CHAN_INFO_RAW:
		if ((chan->channel < 0) ||
		    (chan->channel > (int)adc->soc_info->last_ch)) {
			/* Read converted ADC data */
			spin_lock(&adc->lock);
			ret = tcc_adc_read(adc, (uint32_t)chan->channel);
			spin_unlock(&adc->lock);
			if (ret >= 0) {
				*val = ret;
				ret = IIO_VAL_INT;
			}
		} else {
			ret = -EINVAL;
		}
		break;
	default:
		dev_warn(adc->dev, "[WARN][ADC] %s: %ld is not supported\n",
				__func__, info);
		break;
	}

	return ret;
}

static int32_t tcc_adc_parse_dt(struct tcc_adc *adc)
{
	struct device_node *np = adc->dev->of_node;
	uint32_t clk_rate;
	int32_t ret = 0;

	adc->soc_info = (const struct tcc_adc_soc_info *)of_device_get_match_data(adc->dev);
	if (adc->soc_info == NULL) {
		ret = -EINVAL;
	}

	if (ret == 0) {
		/* get adc base address */
		adc->regs = of_iomap(np, 0);
		if (adc->regs == NULL) {
			dev_err(adc->dev, "[ERROR][ADC] failed to get adc registers\n");
			ret = -ENXIO;
		}
	}

	if (ret == 0) {
		if (adc->soc_info->id == (u32)TCC897X) {
			/* get adc  pmu address */
			adc->pmu_regs = of_iomap(np, 1);
			if (adc->pmu_regs == NULL) {
				dev_err(adc->dev, "[ERROR][ADC] failed to get pmu registers\n");
				ret = -ENXIO;
			}

			if (ret == 0) {
				/* get adc peri clock */
				ret = of_property_read_u32(np, "clock-frequency", &clk_rate);
				if (ret != 0) {
					dev_err(adc->dev, "[ERROR][ADC] Can not get clock frequency value\n");
					ret = -EINVAL;
				}
			}

			if (ret == 0) {
				/* get adc ckin */
				ret = of_property_read_u32(np, "ckin-frequency", &adc->ckin);
				if (ret != 0) {
					dev_err(adc->dev, "[ERROR][ADC]Can not get adc ckin value\n");
					ret = -EINVAL;
				}
			}

			if (ret == 0) {
				/* calculate conversion time */
				adc->conv_time_ns = 6U * (1000U * (1000000U / adc->ckin));

				/* get adc delay */
				ret = of_property_read_u32(np, "adc-delay", &adc->delay);
				if (ret != 0) {
					adc->delay = 0;
				}

				adc->is_12bit_res = of_property_read_bool(np, "adc-12bit-resolution");

				/* board version check */
				ret = of_property_read_u32(np, "adc-board-ver", &adc->board_ver_ch);

				adc->pclk = of_clk_get(np, 0);
				ret = clk_set_rate(adc->pclk, (ulong)clk_rate);
				if (ret != 0) {
					dev_err(adc->dev, "[ERROR][ADC] failed to set clock\n");
				}

				if (ret == 0) {
					adc->hclk = of_clk_get(np, 1);
					adc->iso_clk = of_clk_get(np, 2);
				}
			}
		} else if ((adc->soc_info->id == (u32)TCC803X) ||
			(adc->soc_info->id == (u32)TCC805X)) {

			/* get adc clock address */
			adc->clk_regs = of_iomap(np, 1);
			if (adc->clk_regs == NULL) {
				dev_err(adc->dev, "[ERROR][ADC] failed to get adc clk registers\n");
				ret = -ENXIO;
			}
		} else {
			adc->pclk = of_clk_get(np, 0);
			adc->hclk = of_clk_get(np, 1);
		}
	}

	return ret;
}

static const struct tcc_adc_soc_info tcc897x_soc_info = {
	.id = TCC897X,
	.last_ch = 15,
};

static const struct tcc_adc_soc_info tcc803x_soc_info = {
	.id = TCC803X,
	.last_ch = 11,
};

static const struct tcc_adc_soc_info tcc805x_soc_info = {
	.id = TCC805X,
	.last_ch = 11,
};

static const struct tcc_adc_soc_info tcc807x_soc_info = {
	.id = TCC807X,
	.last_ch = 11,
};


static const struct of_device_id tcc_adc_of_match[] = {
	{ .compatible = "telechips,adc",
			.data = &tcc897x_soc_info, },
	{ .compatible = "telechips,tcc803x-adc",
			.data = &tcc803x_soc_info, },
	{ .compatible = "telechips,tcc805x-adc",
			.data = &tcc805x_soc_info, },
	{ .compatible = "telechips,tcc807x-adc",
			.data = &tcc807x_soc_info, },
	{},
};

static const struct iio_info tcc_adc_info = {
	.read_raw = &tcc_adc_read_raw,
	.attrs = &tcc_adc_attribute_group,
};

MODULE_DEVICE_TABLE(of, tcc_adc_of_match);

static int32_t tcc_adc_probe(struct platform_device *pdev)
{
	struct iio_dev *iiodev;
	struct tcc_adc *adc;
	int32_t ret = 0;

	dev_dbg(&pdev->dev, "[DEBUG][ADC] %s:\n", __func__);

	/* allocate IIO device */
	iiodev = devm_iio_device_alloc(&pdev->dev, (int32_t)sizeof(*adc));
	if (iiodev == NULL) {
		ret = -ENOMEM;
	}

	if (ret == 0) {
		adc = tcc_adc_get_priv(iiodev);
		adc->dev = &pdev->dev;
		platform_set_drvdata(pdev, iiodev);

		/* parsing device tree */
		ret = tcc_adc_parse_dt(adc);
		if (ret < 0) {
			dev_err(&pdev->dev, "[ERROR][ADC] failed to parsing device tree\n");
		}
	}

	if (ret == 0) {
		spin_lock_init(&adc->lock);

		tcc_adc_channel_init(tcc_adc_iio_channels, adc->soc_info->last_ch);

		iiodev->name = dev_name(&pdev->dev);
		iiodev->dev.parent = &pdev->dev;
		iiodev->info = &tcc_adc_info;
		iiodev->modes = INDIO_DIRECT_MODE;
		iiodev->channels = tcc_adc_iio_channels;
		iiodev->num_channels = ((int)adc->soc_info->last_ch + 1);

		/* ADC power control */
		tcc_adc_power_ctrl(iiodev, (bool)true);

		ret = iio_device_register(iiodev);
		if (ret < 0) {
			dev_err(&pdev->dev, "[ERROR][ADC] failed to register iio device.\n");
			tcc_adc_power_ctrl(iiodev, (bool)false);
		} else {
			dev_info(&pdev->dev, "[INFO][ADC] attached iio driver.\n");
		}
	}

	if (ret < 0) {
		iio_device_free(iiodev);
	}

	return ret;
}

static int32_t tcc_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *iiodev = platform_get_drvdata(pdev);

	tcc_adc_power_ctrl(iiodev, (bool)false);
	iio_device_unregister(iiodev);

	return 0;
}

static int32_t tcc_adc_suspend(struct device *dev)
{
	const struct iio_dev *iiodev = dev_get_drvdata(dev);

	tcc_adc_power_ctrl(iiodev, (bool)false);

	return 0;
}

static int32_t tcc_adc_resume(struct device *dev)
{
	const struct iio_dev *iiodev = dev_get_drvdata(dev);

	tcc_adc_power_ctrl(iiodev, (bool)true);

	return 0;
}

static SIMPLE_DEV_PM_OPS(adc_pm_ops, tcc_adc_suspend, tcc_adc_resume);

static ssize_t tccadc_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	const struct iio_dev *iiodev = (const struct iio_dev *)dev_get_drvdata(dev);
	struct tcc_adc *adc = tcc_adc_get_priv(iiodev);
	int32_t ch, data;
	ssize_t ret;

	(void)attr;

	ret = kstrtoint(buf, 10U, &ch);
	if (ret == 0) {
		if ((ch < 0) || (ch > (int)adc->soc_info->last_ch)) {
			dev_err(dev, "[ERROR][ADC] failed to get channel.\n");
			ret = -EINVAL;
		} else {
			spin_lock(&adc->lock);
			data = tcc_adc_read(adc, (uint32_t)ch);
			spin_unlock(&adc->lock);

			if (data >= 0) {
				dev_info(dev, "[INFO][ADC] Get ADC %d : value = %#X\n",
						ch, (uint32_t)data);

				if (count < __INT_MAX__) {
					ret = (ssize_t)count;
				}
			} else {
				ret = -EIO;
			}
		}
	}

	return ret;
}

static struct platform_driver tcc_adc_driver = {
	.driver	= {
		.name	= "tcc-adc",
		.owner	= THIS_MODULE,
		.pm	= &adc_pm_ops,
		.of_match_table	= of_match_ptr(tcc_adc_of_match),
	},
	.probe	= tcc_adc_probe,
	.remove	= tcc_adc_remove,
};
module_platform_driver(tcc_adc_driver);

MODULE_AUTHOR("Telechips Corporation");
MODULE_DESCRIPTION("Telechips ADC Driver");
MODULE_LICENSE("GPL v2");
