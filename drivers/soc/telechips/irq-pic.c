// SPDX-License-Identifier: (GPL-2.0-or-later OR MIT)
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/syscore_ops.h>
#include <soc/telechips/irq.h>

#define TCC_PIC_NAME		((const u8 *)"tcc_pic")

/* index of pic registers */
#define POL_0                   ((u32)0)
#define POL_1                   ((u32)1)
#define CMB_CM4                 ((u32)2)
#define STRGB_CM4               ((u32)3)
#define CA7_IRQO_EN             ((u32)4)
#define PIC_REG_MAX             ((u32)5)

struct tcc_pic_ops {
	s32 (*set_polarity)(const struct irq_data *d, u32 type);
	s32 (*mask_cm4)(u32 irq, u32 bus);
	s32 (*unmask_cm4)(u32 irq, u32 bus);
};

struct tcc_pic {
	void __iomem *reg[PIC_REG_MAX];
	u32 *reg_save[PIC_REG_MAX];
	u32 reg_size[PIC_REG_MAX];
	u32 demarcation;
	u32 max_irq;
	const struct tcc_pic_ops *ops;
	spinlock_t lock;
};

static struct tcc_pic *picinfo;

static s32 tcc_pic_set_polarity(const struct irq_data *irqd, u32 type)
{
	void __iomem *pol;
	u32 mask;
	u32 irq;
	s32 ret = -1;
	unsigned long flags;

	if (irqd == NULL) {
		(void)pr_err("[ERROR][%s] %s: Check for null ptr dereference\n",
			     TCC_PIC_NAME, __func__);
		ret = -EINVAL;
	} else if ((irqd->hwirq < (u32)32) || (picinfo->reg[POL_0] == NULL)
					   || (picinfo->reg[POL_1] == NULL)) {
		(void)pr_err("[ERROR][%s] %s: You have tried the wrong approach:%ld\n",
			     TCC_PIC_NAME, __func__, irqd->hwirq);
		ret = -EFAULT;
	} else {
		irq = (u32)irqd->hwirq - (u32)32;

		if (picinfo->max_irq < (u32)1) {
			(void)pr_err("[ERROR][%s] %s: picinfo->max_irq is wrong.\n",
				     TCC_PIC_NAME, __func__);
			ret = -EINVAL;
		} else {
			if (irq > (picinfo->max_irq - (u32)1)) {
				(void)pr_warn("[WARN][%s] %s: But, GIC will be set IRQ:%d\n",
					      TCC_PIC_NAME, __func__, irq);
				/* need to avoid accessing polarity registers */
				ret = 1;
			} else {
				ret = 0;
			}
		}
	}

	if (ret == 0) {
		if (irq < picinfo->demarcation) {
			pol = picinfo->reg[POL_0] + ((irq >> (u32)5) << (u32)2);
		} else {
			pol = picinfo->reg[POL_1]
			      + (((irq - picinfo->demarcation) >> (u32)5) << (u32)2);
		}
		mask = (u32)1 << (irq & (u32)0x1F);

		spin_lock_irqsave(&picinfo->lock, flags);

		if ((type == (u32)IRQ_TYPE_LEVEL_HIGH) ||
		    (type == (u32)IRQ_TYPE_EDGE_RISING)) {
			writel_relaxed(readl_relaxed(pol) & ~mask, pol);
		} else if ((type == (u32)IRQ_TYPE_LEVEL_LOW) ||
			   (type == (u32)IRQ_TYPE_EDGE_FALLING)) {
			writel_relaxed(readl_relaxed(pol) | mask, pol);
		} else {
			ret = -EINVAL;
		}

		spin_unlock_irqrestore(&picinfo->lock, flags);
	}

	if (ret > 0) {
		/* It does not mean errors */
		ret = 0;
	}
	return ret;
}

static s32 tcc_pic_mask_control(u32 irq, u32 bus, u32 enable)
{
	void __iomem *reg = NULL;
	u32 mask;
	s32 ret = -1;
	unsigned long flags;

	if (picinfo->max_irq < (u32)1) {
		(void)pr_err("[ERROR][%s] %s: picinfo->max_irq is wrong.\n",
			     TCC_PIC_NAME, __func__);
		ret = -EINVAL;
	} else if (irq > (picinfo->max_irq - (u32)1)) {
		(void)pr_err("[ERROR][%s] %s: Wrong IRQ:%d\n",
			     TCC_PIC_NAME, __func__, irq);
		ret = -EFAULT;
	} else if (bus == PIC_GATE_CMB) {
		if (picinfo->reg[CMB_CM4] == NULL) {
			(void)pr_err(
				     "[ERROR][%s] %s: You have tried the wrong approach:%d\n",
				     TCC_PIC_NAME, __func__, irq);
			ret = -EFAULT;
		} else {
			reg = picinfo->reg[CMB_CM4] + ((irq >> (u32)5) << (u32)2);
			ret = 0;
		}
	} else if (bus == PIC_GATE_STRGB) {
		if (picinfo->reg[STRGB_CM4] == NULL) {
			(void)pr_err(
				     "[ERROR][%s] %s: You have tried the wrong approach:%d\n",
				     TCC_PIC_NAME, __func__, irq);
			ret = -EFAULT;
		} else {
			reg = picinfo->reg[STRGB_CM4] + ((irq >> (u32)5) << (u32)2);
			ret = 0;
		}
	} else if (bus == SMU_GATE_CA7) {
		if (picinfo->reg[CA7_IRQO_EN] == NULL) {
			(void)pr_err(
				     "[ERROR][%s] %s: You have tried the wrong approach.\n",
				     TCC_PIC_NAME, __func__);
			ret = -EFAULT;
		} else {
			reg = picinfo->reg[CA7_IRQO_EN] + ((irq >> (u32)5) << (u32)2);
			ret = 0;
		}
	} else {
		(void)pr_err("[ERROR][%s] %s: bus type(%d) is wrong.\n",
			     TCC_PIC_NAME, __func__, bus);
		ret = -EINVAL;
	}

	if (ret == 0) {
		mask = (u32)1 << (irq & (u32)0x1F);

		spin_lock_irqsave(&picinfo->lock, flags);

		if (enable == (u32)0) {
			/* mask */
			writel_relaxed(readl_relaxed(reg) & ~mask, reg);
		} else {
			/* unmask */
			writel_relaxed(readl_relaxed(reg) | mask, reg);
		}

		spin_unlock_irqrestore(&picinfo->lock, flags);
	}

	return ret;
}

static s32 tcc_pic_mask_cm4(u32 irq, u32 bus)
{
	return tcc_pic_mask_control(irq, bus, 0);
}

static s32 tcc_pic_unmask_cm4(u32 irq, u32 bus)
{
	return tcc_pic_mask_control(irq, bus, 1);
}

static void __maybe_unused tcc_pic_allmask_ca7(void)
{
	unsigned long flags;

	if (picinfo->reg[CA7_IRQO_EN] == NULL) {
		(void)pr_err("[ERROR][%s] %s: You have tried the wrong approach.\n",
			     TCC_PIC_NAME, __func__);
	} else {
		spin_lock_irqsave(&picinfo->lock, flags);
		writel_relaxed(0, picinfo->reg[CA7_IRQO_EN] + 0x00);
		writel_relaxed(0, picinfo->reg[CA7_IRQO_EN] + 0x04);
		writel_relaxed(0, picinfo->reg[CA7_IRQO_EN] + 0x08);
		writel_relaxed(0, picinfo->reg[CA7_IRQO_EN] + 0x0C);
		writel_relaxed(0, picinfo->reg[CA7_IRQO_EN] + 0x10);
		writel_relaxed(0, picinfo->reg[CA7_IRQO_EN] + 0x14);
		writel_relaxed(0, picinfo->reg[CA7_IRQO_EN] + 0x18);
		spin_unlock_irqrestore(&picinfo->lock, flags);
	}
}

s32 tcc_irq_set_polarity_ca7(u32 irq, u32 type)
{
	void __iomem *pol;
	u32 mask;
	s32 ret = -1;
	unsigned long flags;

	if ((picinfo->reg[POL_0] == NULL) || (picinfo->reg[POL_1] == NULL)) {
		(void)pr_err("[ERROR][%s] %s: You have tried the wrong approach\n",
			     TCC_PIC_NAME, __func__);
		ret = -EFAULT;
	} else if (picinfo->max_irq < (u32)1) {
		(void)pr_err("[ERROR][%s] %s: picinfo->max_irq is wrong.\n", TCC_PIC_NAME, __func__);
		ret = -EINVAL;
	} else if (irq > (picinfo->max_irq - (u32)1)) {
		(void)pr_err("[ERROR][%s] %s: Wrong IRQ:%d\n", TCC_PIC_NAME, __func__, irq);
		ret = -EFAULT;
	} else {
		if (irq < picinfo->demarcation) {
			pol = picinfo->reg[POL_0] + ((irq >> (u32)5) << (u32)2);
		} else {
			pol = picinfo->reg[POL_1]
			      + (((irq - picinfo->demarcation) >> (u32)5) << (u32)2);
		}
		mask = (u32)1 << (irq & (u32)0x1F);

		spin_lock_irqsave(&picinfo->lock, flags);

		if ((type == (u32)IRQ_TYPE_LEVEL_HIGH) ||
		    (type == (u32)IRQ_TYPE_EDGE_RISING)) {
			writel_relaxed(readl_relaxed(pol) & ~mask, pol);
		} else if ((type == (u32)IRQ_TYPE_LEVEL_LOW) ||
			   (type == (u32)IRQ_TYPE_EDGE_FALLING)) {
			writel_relaxed(readl_relaxed(pol) | mask, pol);
		} else {
			ret = -EINVAL;
		}

		spin_unlock_irqrestore(&picinfo->lock, flags);
		ret = 0;
	}

	return ret;
}
EXPORT_SYMBOL(tcc_irq_set_polarity_ca7);

s32 tcc_irq_mask_ca7(u32 irq)
{
	return tcc_pic_mask_control(irq, SMU_GATE_CA7, 0);
}
EXPORT_SYMBOL(tcc_irq_mask_ca7);

s32 tcc_irq_unmask_ca7(u32 irq)
{
	return tcc_pic_mask_control(irq, SMU_GATE_CA7, 1);
}
EXPORT_SYMBOL(tcc_irq_unmask_ca7);

s32 tcc_irq_set_polarity(u32 irq, u32 type)
{
	s32 ret = 0;
	struct irq_desc *desc;

	desc = irq_to_desc(irq);
	if (desc == NULL) {
		ret = -EINVAL;
	}

	if ((picinfo != NULL) && (ret == 0)) {
		if (picinfo->ops->set_polarity != NULL) {
			ret = picinfo->ops->set_polarity(&desc->irq_data, type);
		}
	}

	return ret;
}
EXPORT_SYMBOL(tcc_irq_set_polarity);

s32 tcc_irq_mask_cm4(u32 irq, u32 bus)
{
	s32 ret = 0;

	if (picinfo != NULL) {
		if (picinfo->ops->mask_cm4 != NULL) {
			ret = picinfo->ops->mask_cm4(irq, bus);
		}
	}

	return ret;
}
EXPORT_SYMBOL(tcc_irq_mask_cm4);

s32 tcc_irq_unmask_cm4(u32 irq, u32 bus)
{
	s32 ret = 0;

	if (picinfo != NULL) {
		if (picinfo->ops->unmask_cm4 != NULL) {
			ret = picinfo->ops->unmask_cm4(irq, bus);
		}
	}

	return ret;
}
EXPORT_SYMBOL(tcc_irq_unmask_cm4);

#if defined(CONFIG_PM_SLEEP)
static s32 tcc_pic_suspend(void)
{
	u32 i, j;
	s32 ret = -1;

	/* store all registers of pic */
	for (i = 0;  i < PIC_REG_MAX; i++) {
		if (picinfo->reg_size[i] == (u32)0) {
			continue;
		}

		picinfo->reg_save[i] =
		    kzalloc(picinfo->reg_size[i], GFP_KERNEL);
		if (picinfo->reg_save[i] != NULL) {
			for (j = 0; (j << 2) < picinfo->reg_size[i]; j++) {
				*(picinfo->reg_save[i] + j) =
				    readl_relaxed(picinfo->reg[i] + (j << 2));
			}
			ret = 0;
		} else {
			(void)pr_err("[ERROR][%s] %s: can't allocate memory.\n",
				     TCC_PIC_NAME, __func__);
			ret = -ENOMEM;
			break;
		}
	}

	return ret;
}

static void tcc_pic_resume(void)
{
	u32 i, j;

	/* restore all registers of pic */
	for (i = 0; i < PIC_REG_MAX; i++) {
		if (picinfo->reg_size[i] == (u32)0) {
			continue;
		}

		if (picinfo->reg_save[i] != NULL) {
			for (j = 0; (j << 2) < picinfo->reg_size[i]; j++) {
				writel_relaxed(*(picinfo->reg_save[i] + j),
					       picinfo->reg[i] + (j << (u32)2));
			}
			kfree(picinfo->reg_save[i]);
		} else {
			(void)pr_err("[ERROR][%s] %s: can't find memory.\n",
				     TCC_PIC_NAME, __func__);
			break;
		}
	}
}
#else
static s32 tcc_pic_suspend(void)
{
	return 0;
}

static void tcc_pic_resume(void)
{
}
#endif

static struct syscore_ops tcc_pic_syscore_ops = {
	.suspend = tcc_pic_suspend,
	.resume = tcc_pic_resume,
};

static struct tcc_pic_ops tcc807x_pic_ops = {
	.set_polarity = tcc_pic_set_polarity,
	.mask_cm4 = NULL,
	.unmask_cm4 = NULL,
};

static struct tcc_pic_ops tcc805x_pic_ops = {
	.set_polarity = tcc_pic_set_polarity,
	.mask_cm4 = tcc_pic_mask_cm4,
	.unmask_cm4 = tcc_pic_unmask_cm4,
};

static struct tcc_pic_ops tcc803x_pic_ops = {
	.set_polarity = tcc_pic_set_polarity,
	.mask_cm4 = NULL,
	.unmask_cm4 = NULL,
};

static struct tcc_pic_ops tcc901x_pic_ops = {
	.set_polarity = tcc_pic_set_polarity,
	.mask_cm4 = NULL,
	.unmask_cm4 = NULL,
};

static struct tcc_pic_ops tcc899x_pic_ops = {
	.set_polarity = tcc_pic_set_polarity,
	.mask_cm4 = NULL,
	.unmask_cm4 = NULL,
};

static struct tcc_pic_ops tccsub_ca7_pic_ops = {
	.set_polarity = NULL,
	.mask_cm4 = NULL,
	.unmask_cm4 = NULL,
};

static struct tcc_pic_ops tcc897x_pic_ops = {
	.set_polarity = tcc_pic_set_polarity,
	.mask_cm4 = NULL,
	.unmask_cm4 = NULL,
};

#ifdef CONFIG_OF
static const struct of_device_id tcc_pic_of_match[] = {
	{.compatible = "telechips,tcc807x-pic", .data = &tcc807x_pic_ops},
	{.compatible = "telechips,tcc805x-pic", .data = &tcc805x_pic_ops},
	{.compatible = "telechips,tcc803x-pic", .data = &tcc803x_pic_ops},
	{.compatible = "telechips,tccsub-ca7-pic", .data = &tccsub_ca7_pic_ops},
	{.compatible = "telechips,tcc901x-pic", .data = &tcc901x_pic_ops},
	{.compatible = "telechips,tcc899x-pic", .data = &tcc899x_pic_ops},
	{.compatible = "telechips,tcc897x-pic", .data = &tcc897x_pic_ops},
	{}
};

static s32 tcc_pic_probe(struct platform_device *pdev)
{
	struct device *dev;
	struct device_node *np;
	const struct of_device_id *match;
	struct tcc_pic *pic;
	struct resource res;
	u32 i;
	s32 ret = -1;

	if (pdev == NULL) {
		ret = -ENODEV;
	} else {
		dev = &pdev->dev;
		np = pdev->dev.of_node;

		pic = devm_kzalloc(dev, sizeof(*pic), GFP_KERNEL);
		if (pic != NULL) {
			(void)memset(pic, 0, sizeof(*pic));
			picinfo = pic;
			ret = 0;
		} else {
			(void)pr_err("[ERROR][%s] %s: can't alloccate memory.\n",
				     TCC_PIC_NAME, __func__);
			ret = -ENOMEM;
		}
	}

	if (ret == 0) {
		match = of_match_node(tcc_pic_of_match, np);
		if (match != NULL) {
			picinfo->ops = (const struct tcc_pic_ops *)match->data;
		} else {
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		/* It'll be set as NULL if the size of reg node is zero. */
		for (i = 0; i < PIC_REG_MAX; i++) {
			picinfo->reg[i] = of_iomap(np, (s32)i);
			ret = of_address_to_resource(np, (s32)i, &res);
			if (ret != 0) {
				picinfo->reg_size[i] = 0;
			} else {
				picinfo->reg_size[i] = (u32)(resource_size(&res) & UINT_MAX);
			}
		}

		ret = of_property_read_u32(np,
					   "demarcation",
					   (u32 *)&picinfo->demarcation);
		if (ret != 0) {
			(void)pr_warn("[WARN][%s] %s: can't get the demarcation.\n",
				      TCC_PIC_NAME, __func__);
		}

		ret = of_property_read_u32(np, "max_irq", (u32 *)&picinfo->max_irq);
		if ((ret != 0) || (picinfo->max_irq < (u32)1)) {
			(void)pr_warn("[WARN][%s] %s: max_irq is wrong.\n",
				      TCC_PIC_NAME, __func__);
		}

		ret = of_device_is_compatible(np, "telechips,tccsub-ca7-pic");
		if (ret != 0) {
			tcc_pic_allmask_ca7();
		} else {
			/* no operation */
		}

		spin_lock_init(&picinfo->lock);
		platform_set_drvdata(pdev, picinfo);

		if (of_property_read_bool(np, "have-syscore-ops")) {
			register_syscore_ops(&tcc_pic_syscore_ops);
		}
	}

	return ret;
}

MODULE_DEVICE_TABLE(of, tcc_pic_of_match);
#endif

static struct platform_driver tcc_pic_driver = {
	.probe = &tcc_pic_probe,
	.driver = {
		   .name = TCC_PIC_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(tcc_pic_of_match),
#endif
		   },
};
builtin_platform_driver(tcc_pic_driver)


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Telechips, Inc");
MODULE_DESCRIPTION("Programmable interrupt controller driver");
