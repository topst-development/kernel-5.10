// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Telechips Inc.
 */
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#if defined(CONFIG_PINCTRL_TCC_SCFW)
#include <linux/soc/telechips/tcc_sc_protocol.h>

static const struct tcc_sc_fw_handle *sc_fw_handle;
#endif

#if defined(CONFIG_PINCTRL_TCC_SCFW)
static int32_t request_gpio_to_sc(ulong address,
	ulong bit_number, ulong width, ulong value)
{
	s32 ret;
	u32 u32_mask = 0xFFFFFFFFU;
	u32 addr_32 = (u32)(address & u32_mask);
	u32 bit_num_32 = (u32)(bit_number & u32_mask);
	u32 width_32 = (u32)(width & u32_mask);
	u32 value_32 = (u32)(value & u32_mask);

	if (sc_fw_handle != NULL) {
		ret = sc_fw_handle
			->ops.gpio_ops->request_gpio_no_res
			(sc_fw_handle, addr_32,
			bit_num_32, width_32, value_32);
	} else {
		(void)pr_err(
		    "[ERROR][PINCTRL] %s : sc_fw_handle is NULL"
		    , __func__);
		ret = -EINVAL;
	}

	return ret;
}
#endif

#if defined(CONFIG_ARCH_TCC750X)
#define GPIO_EINT_MUX	0x630U
#else
#define GPIO_EINT_MUX	0x280U
#endif

#define GPIO_BIT_SET	0x8
#define GPIO_BIT_CLEAR	0xC
#define GPIO_BIT(nr)	(1U << (nr))
#define GPIO_EINT_IN_USE 1U
#define GPIO_EINT_NOT_USE 0U

static const struct of_device_id telechips_gpio_dt_ids[] = {
	{.compatible = "telechips,tcc-gpio",	.data = NULL,},
};

struct tcc_gpio_port;

struct tcc_gpio_soc_data {
	int temp_data;
};

struct tcc_gpio_group {
	struct tcc_gpio_port *port;
	struct gpio_chip gc;
	struct irq_chip ic;
	u32 source_section;
	u32 *gpio_offset_num;
	u32 *source_offset_num;
	u32 *source_range;
	const char *name;
	u32 reg_offset;
};

struct tcc_gpio_eint {
	const char *group;
	unsigned int pin;
	unsigned int type;
	unsigned int used;
};

struct tcc_gpio_port {
	unsigned int gpio_num_gr;
	uintptr_t base;
#if defined(CONFIG_PINCTRL_TCC_SCFW)
	u32 raw_base;
#endif
	struct tcc_gpio_group *gpio_gr;
	const struct tcc_gpio_soc_data *sdata;
	unsigned int *irq;
	unsigned int irq_num;
	unsigned int **irq_port_map;
};

static void gpio_mul_wrap_ul(ulong ui_a, ulong ui_b, ulong *ui_mul)
{
	if ((ui_b == 0U) || ((ULONG_MAX / ui_b) < ui_a)) {
		BUG();
	} else {
		*ui_mul = ui_a * ui_b;
	}
}

static void gpio_add_wrap_ul(ulong ui_a, ulong ui_b, ulong *ui_sum)
{
	if ((ULONG_MAX - ui_a) < ui_b) {
		BUG();
	} else {
		*ui_sum = ui_a + ui_b;
	}
}

static void gpio_sub_wrap_ul(ulong ui_a, ulong ui_b, ulong *ui_sum)
{
	if (ui_a < ui_b) {
		BUG();
	} else {
		*ui_sum = ui_a - ui_b;
	}
}

static void tcc_gpio_set(struct gpio_chip *gc, unsigned int gpio_offset, int val);

static void tcc_gpio_set_irq_idx(struct irq_data *d, struct tcc_gpio_port *port, struct tcc_gpio_group *gpio_gr, int *flag, unsigned long *idx_sel_reg, unsigned long *idx_sel_plc)
{
	irq_hw_number_t hwirq;
#if defined(CONFIG_ARCH_TCC897X)
	unsigned int i = 0;
#endif

	pr_debug("%s: ############name : %s %s %s %s %s\n"
		, __func__, d->chip->name, gpio_gr->name, port->gpio_gr->ic.name,
		port->gpio_gr->gc.label, port->gpio_gr->name);

	*flag = (int)irqd_get_trigger_type(d);
	hwirq = irqd_to_hwirq(d)&0xFFFFFFFFU;
	gpio_sub_wrap_ul(hwirq, 32U, &hwirq);

#if defined(CONFIG_ARCH_TCC897X)
	for (i = 0; i < port->irq_num; i++) {
		pr_debug("[GPIO][DEBUG] %s: irq map 4 : %d hwirq : %ld\n",
			__func__, port->irq_port_map[i][4], hwirq);

		if (port->irq_port_map[i][4] == hwirq)
			break;
	}

	if (i > 15U) {
		i -= 16U;
		*idx_sel_reg = i / 4U;
		*idx_sel_plc = i % 4U;
	} else {
		*idx_sel_reg = i / 4U;
		*idx_sel_plc = i % 4U;
	}

#elif defined(CONFIG_TCC803X_CA7S)
	if (hwirq > 7U) {
		hwirq -= 8U;
		*idx_sel_reg = hwirq / 2U;
		*idx_sel_plc = hwirq % 2U;
	} else {
		*idx_sel_reg = hwirq / 2U;
		*idx_sel_plc = hwirq % 2U;
	}
#elif defined(CONFIG_ARCH_TCC750X)
	if (hwirq > 7U) {
		hwirq -= 8U;
		*idx_sel_reg = hwirq / 4U;
		*idx_sel_plc = hwirq % 4U;
	} else {
		*idx_sel_reg = hwirq / 4U;
		*idx_sel_plc = hwirq % 4U;
	}
#else
	if (hwirq > 15U) {
		hwirq -= 16U;
		*idx_sel_reg = hwirq / 4U;
		*idx_sel_plc = hwirq % 4U;
	} else {
		*idx_sel_reg = hwirq / 4U;
		*idx_sel_plc = hwirq % 4U;
	}
#endif
	pr_debug("[GPIO][DEBUG] %s: hwirq : %ld\n", __func__, hwirq);
	pr_debug("[GPIO][DEBUG] %s: idx_sel_reg : %ld idx_sel_plc : %ld\n",
		 __func__, *idx_sel_reg, *idx_sel_plc);
}

static void tcc_gpio_set_irq_source(struct tcc_gpio_port *port, unsigned long idx_sel_reg, unsigned long idx_sel_plc, unsigned int *irq_source)
{
	ulong gpio_addr = 0;
	ulong gpio_idx = 0;

	gpio_mul_wrap_ul(idx_sel_reg, 4U, &gpio_addr);
	gpio_add_wrap_ul(gpio_addr, GPIO_EINT_MUX, &gpio_addr);
	gpio_add_wrap_ul(gpio_addr, port->base, &gpio_addr);

#if defined(CONFIG_TCC803X_CA7S)
	gpio_mul_wrap_ul(idx_sel_plc, 16U, &gpio_idx);
	*irq_source =
		0xffU & (readl((void __iomem *)gpio_addr) >>
			(gpio_idx));
#else
	gpio_mul_wrap_ul(idx_sel_plc, 8U, &gpio_idx);
	*irq_source =
		0xffU & (readl((void __iomem *)gpio_addr) >>
			(gpio_idx));
#endif
}

static void tcc_gpio_set_irq_port_map(struct tcc_gpio_port *port, unsigned int irq_source, unsigned int *pin)
{
	unsigned int i;
	pr_debug("[GPIO][DEBUG] %s: irq_source num : %d\n", __func__, irq_source);
	pr_debug("[GPIO][DEBUG] %s: sources map : %d %d\n", __func__, port->irq_port_map[0][1], port->irq_port_map[0][2]);

	for (i = 0U; i < (port->irq_num / 2U); i++) {
		if ( (port->irq_port_map[i][3] == GPIO_EINT_IN_USE) && (port->irq_port_map[i][2] == irq_source)) {
			*pin = port->irq_port_map[i][1];
			break;
		}
	}
}

static irqreturn_t tcc_gpio_irq_generic(struct irq_chip *chip, struct irq_desc *desc, struct tcc_gpio_group *gpio_gr, unsigned int pin, int flag)
{
	irqreturn_t ret = IRQ_HANDLED;
	struct gpio_irq_chip gpio_irq = gpio_gr->gc.irq;

	chained_irq_enter(chip, desc);

	if (flag == IRQ_TYPE_EDGE_RISING) {
		ret = generic_handle_irq(irq_find_mapping(gpio_irq.domain, pin));
	} else if (flag == IRQ_TYPE_LEVEL_HIGH) {
		handle_nested_irq(irq_find_mapping(gpio_irq.domain, pin));
	} else {
		chained_irq_exit(chip, desc);
		ret = IRQ_NONE;
	}

	if (ret == IRQ_HANDLED) {
		chained_irq_exit(chip, desc);
	}
	return ret;
}

static irqreturn_t tcc_gpio_irq_handler(int irq, void *data)
{
	struct tcc_gpio_group *gpio_gr = data;
	struct tcc_gpio_port *port = gpio_gr->port;
	unsigned int irq_source;
	unsigned int pin = 0;
	unsigned long idx_sel_reg, idx_sel_plc;
	int flag;
	irqreturn_t ret = IRQ_HANDLED;

	if (irq < 0) {
		ret = IRQ_NONE;
	}

	if (ret == IRQ_HANDLED) {
		struct irq_desc *desc = irq_to_desc((unsigned int)irq);
		struct irq_data *d = irq_get_irq_data((unsigned int)irq);
		if (d == NULL) {
			ret = IRQ_NONE;
		}
		if (ret == IRQ_HANDLED) {
			struct irq_chip *chip = d->chip;

			tcc_gpio_set_irq_idx(d, port, gpio_gr, &flag, &idx_sel_reg, &idx_sel_plc);

			tcc_gpio_set_irq_source(port, idx_sel_reg, idx_sel_plc, &irq_source);

			tcc_gpio_set_irq_port_map(port, irq_source, &pin);

#if !defined(CONFIG_ARCH_TCC897X)
			/*
			* Exception handing. '0' in irq_source means something went wrong because irq
			* source number starts with '1' and the '0' indicates 'disable'.
			*/
			if (irq_source == 0U) {
				ret = IRQ_HANDLED;
			} else {
				ret = tcc_gpio_irq_generic(chip, desc, gpio_gr, pin, flag);
			}
#else
			ret = tcc_gpio_irq_generic(chip, desc, gpio_gr, pin, flag);
#endif
		}
	}
	return ret;
}

static int tcc_gpio_irq_set_gr(struct tcc_gpio_group *gpio_gr, unsigned int pin, unsigned int *irq_source)
{
	unsigned int i = 0;
	int ret = 0;
	int pin_valid = 0;

	if (gpio_gr->source_section == 0xffU) {
		(void)pr_err
		    ("[GPIO][ERROR] %s : external interrupt is not supported\n",
		     __func__);
		ret = -EINVAL;
	}

	if (ret == 0) {
		for (i = 0U; i < gpio_gr->source_section; i++) {
			if ((pin >= gpio_gr->gpio_offset_num[i]) &&
			(pin < (gpio_gr->gpio_offset_num[i] +
				gpio_gr->source_range[i]))) {
				*irq_source = gpio_gr->source_offset_num[i] +
					(pin - gpio_gr->gpio_offset_num[i]);
				pin_valid = 1;	//true
				break;
			} else {
				pin_valid = 0;	//false
			}
		}

		if (pin_valid == 0) {
			(void)pr_err("[GPIO][ERROR] %s: %d is out of range of pin number\n"
				, __func__, pin);
			ret = -EINVAL;
		}
	}
	return ret;
}

static int tcc_gpio_irq_set_sel_reg(struct tcc_gpio_port *port, unsigned int irq_source, unsigned int cnt)
{
	unsigned int idx_sel_reg, idx_sel_plc;
	ulong gpio_addr = 0;
	ulong gpio_idx = 0;

#if defined(CONFIG_TCC803X_CA7S)
	idx_sel_reg = port->irq_port_map[cnt][0] / 2U;
	idx_sel_plc = port->irq_port_map[cnt][0] % 2U;
#else
	idx_sel_reg = port->irq_port_map[cnt][0] / 4U;
	idx_sel_plc = port->irq_port_map[cnt][0] % 4U;
#endif

#if defined(CONFIG_TCC803X_CA7S)
	irq_source = (irq_source | (irq_source << 8)) << (idx_sel_plc * 16);

	gpio_mul_wrap_ul(idx_sel_reg, 4U, &gpio_idx);
	gpio_add_wrap_ul(port->base, GPIO_EINT_MUX, &gpio_addr);
	gpio_add_wrap_ul(gpio_addr, gpio_idx, &gpio_addr);
	writel( (readl((void __iomem *)gpio_addr) | irq_source), (void __iomem *)gpio_addr);
#else
#if defined(CONFIG_PINCTRL_TCC_SCFW)
	gpio_mul_wrap_ul(idx_sel_reg, 4U, &gpio_idx);
	gpio_add_wrap_ul(port->raw_base, GPIO_EINT_MUX, &gpio_addr);
	gpio_add_wrap_ul(gpio_addr, gpio_idx, &gpio_addr);
	gpio_mul_wrap_ul(idx_sel_plc, 8U, &gpio_idx);
	(void)request_gpio_to_sc(gpio_addr, gpio_idx, 8U, irq_source);
#else
	gpio_mul_wrap_ul(idx_sel_reg, 4U, &gpio_idx);
	gpio_add_wrap_ul(port->base, GPIO_EINT_MUX, &gpio_addr);
	gpio_add_wrap_ul(gpio_addr, gpio_idx, &gpio_addr);
	gpio_mul_wrap_ul(idx_sel_plc, 8U, &gpio_idx);
	writel((readl((void __iomem *)gpio_addr) | (irq_source << gpio_idx)), (void __iomem *)gpio_addr);
#endif
#endif
	return 0;
}

static int tcc_gpio_request_irq(struct tcc_gpio_port *port, const char *group, unsigned int pin, unsigned int type)
{
	unsigned int i = 0;
	int ret = 0;
	unsigned int irq_mux_num = 0;
	ulong irq_num_rev = 0;
	int irq_type;
	unsigned int irq_port = 0U;
	unsigned int irq_port_both;
	struct tcc_gpio_group *gpio_gr;
	unsigned int irq_source = 0U;
	int irq_valid = 0;

	gpio_gr = port->gpio_gr;

	for (i = 0; i < port->gpio_num_gr; i++) {
		if (!strcmp(group, gpio_gr->name)) {
			break;
		}
		gpio_gr++;
	}

	ret = tcc_gpio_irq_set_gr(gpio_gr, pin, &irq_source);

	if (ret == 0) {
		for (i = 0; i < port->irq_num / 2; i++) {
			if (port->irq_port_map[i][3] == GPIO_EINT_NOT_USE) {
				port->irq_port_map[i][1] = pin;
				port->irq_port_map[i][2] = irq_source;
				port->irq_port_map[i][3] = GPIO_EINT_IN_USE;
				irq_mux_num = i;
				irq_valid = 1;
				break;
			}
		}

		if (irq_valid == 0) {
			pr_err("[GPIO][ERROR] %s: no more external irq selection\n", __func__);
			ret = -1;
		}
	}

	if (ret == 0) {
		gpio_add_wrap_ul(irq_mux_num, (port->irq_num / 2UL), &irq_num_rev);

		switch (type) {
			case IRQ_TYPE_EDGE_RISING:
				irq_port = port->irq[irq_mux_num];
				irq_type = IRQ_TYPE_EDGE_RISING;
				break;
			case IRQ_TYPE_EDGE_FALLING:
				irq_port = port->irq[irq_num_rev];
				irq_type = IRQ_TYPE_EDGE_RISING;
				break;
			case IRQ_TYPE_EDGE_BOTH:
				irq_port = port->irq[irq_mux_num];
				irq_port_both = port->irq[irq_num_rev];
				irq_type = IRQ_TYPE_EDGE_RISING;
				break;
			case IRQ_TYPE_LEVEL_LOW:
				irq_port = port->irq[irq_num_rev];
				irq_type = IRQF_TRIGGER_HIGH|IRQF_ONESHOT;
				break;
			case IRQ_TYPE_LEVEL_HIGH:
				irq_port = port->irq[irq_mux_num];
				irq_type = IRQF_TRIGGER_HIGH|IRQF_ONESHOT;
				break;
			default:
				ret = -EINVAL;
				break;
		}

		if (ret == 0) {
			irq_set_status_flags(irq_port, IRQ_NOAUTOEN);

			if ((type & (IRQ_TYPE_EDGE_RISING|IRQ_TYPE_EDGE_FALLING)) != 0) {
				ret = devm_request_irq(gpio_gr->gc.parent, irq_port,
						tcc_gpio_irq_handler,
						irq_type, KBUILD_MODNAME,
						gpio_gr);
				if (type == IRQ_TYPE_EDGE_BOTH) {
					irq_set_status_flags(irq_port_both, IRQ_NOAUTOEN);
					ret = devm_request_irq(gpio_gr->gc.parent, irq_port_both,
							tcc_gpio_irq_handler,
							IRQ_TYPE_EDGE_RISING, KBUILD_MODNAME,
							gpio_gr);
				}
			} else if ((type & IRQ_TYPE_LEVEL_MASK) != 0) {
				ret = devm_request_threaded_irq(gpio_gr->gc.parent, irq_port,
						NULL, tcc_gpio_irq_handler,
						irq_type, KBUILD_MODNAME,
						gpio_gr);
			} else {
				;
			}
		}
	}
	return ret;
}

static int tcc_gpio_irq_set_data(struct irq_data *d, struct tcc_gpio_port *port, unsigned int type)
{
	unsigned int i;
	int ret = 0;
	unsigned int irq_source = 0;
	unsigned int pin = (unsigned int)(d->hwirq & 0xFFFFFFFFU);
	struct tcc_gpio_group *gpio_gr;
	int irq_valid = 0;
	unsigned int irq_mux_num = 0;
	ulong irq_num_rev = 0;

	gpio_gr = port->gpio_gr;

	for (i = 0; i < port->gpio_num_gr; i++) {
		if (!strcmp(d->chip->name, gpio_gr->name)) {
			break;
		}
		gpio_gr++;
	}

	ret = tcc_gpio_irq_set_gr(gpio_gr, pin, &irq_source);

	if (ret == 0) {
		for (i = 0; i < port->irq_num / 2; i++) {
			if ((port->irq_port_map[i][3] == GPIO_EINT_IN_USE) && (port->irq_port_map[i][1] == pin)
						&& (port->irq_port_map[i][2] == irq_source)) {
				irq_mux_num = i;
				irq_valid = 1;
				break;
			}
		}

		if (irq_valid == 0) {
			pr_err("[GPIO][ERROR] %s: Not matched EINT to IRQ Source, Please Check EINT device tree\n", __func__);
			ret = -1;
		} else {
			ret = tcc_gpio_irq_set_sel_reg(port, irq_source, i);
		}
	}

	if (ret==0) {
		gpio_add_wrap_ul(irq_mux_num, (port->irq_num / 2UL), &irq_num_rev);

		switch (type) {
			case IRQ_TYPE_LEVEL_HIGH:
			case IRQ_TYPE_EDGE_RISING:
				enable_irq(port->irq[irq_mux_num]);
				break;
			case IRQ_TYPE_LEVEL_LOW:
			case IRQ_TYPE_EDGE_FALLING:
				enable_irq(port->irq[irq_num_rev]);
				break;
			case IRQ_TYPE_EDGE_BOTH:
				enable_irq(port->irq[irq_num_rev]);
				enable_irq(port->irq[irq_mux_num]);
				break;
			default:
				ret = -EINVAL;
				break;
		}
	}

	return ret;
}

static int tcc_gpio_irq_set_type(struct irq_data *d, u32 type)
{
	int ret = 0;
	struct gpio_chip *chip = irq_data_get_irq_chip_data(d);
	struct tcc_gpio_port *port = gpiochip_get_data(chip);

	ret = tcc_gpio_irq_set_data(d, port, type);

	return ret;
}

static void tcc_gpio_irq_disable(struct irq_data *d)
{
}

static void tcc_gpio_irq_enable(struct irq_data *d)
{
}

static int tcc_gpio_direction_input(struct gpio_chip *chip, unsigned int gpio_offset)
{
	int ret = 0;
	ulong gpio_addr = 0;

	if (chip->base < 0) {
		ret = -1;
	} else {
		gpio_add_wrap_ul((unsigned int)chip->base, gpio_offset, &gpio_addr);
		if (gpio_addr > UINT_MAX) {
			ret = -1;
		}
		if (ret == 0) {
			ret = pinctrl_gpio_direction_input((unsigned int)gpio_addr);
		}
	}
	return ret;
}

static int tcc_gpio_direction_output(struct gpio_chip *chip, unsigned int gpio_offset,
					    int value)
{
	int ret = 0;
	ulong gpio_addr = 0;
	tcc_gpio_set(chip, gpio_offset, value);
	if (chip->base < 0) {
		ret = -1;
	} else {
		gpio_add_wrap_ul((unsigned int)chip->base, gpio_offset, &gpio_addr);
		if (gpio_addr > UINT_MAX) {
			ret = -1;
		}
		if (ret == 0) {
			ret = pinctrl_gpio_direction_output((unsigned int)gpio_addr);
		}
	}
	return ret;
}

static int tcc_gpio_get(struct gpio_chip *gc, unsigned int gpio_offset)
{
	const struct tcc_gpio_port *port = gpiochip_get_data(gc);
	const struct tcc_gpio_group *gpio_gr = port->gpio_gr;
	unsigned int gpio_bit = gpio_offset & 0x0000001FU;
	unsigned int bit = 1U << gpio_bit;
	unsigned int i;
	uintptr_t reg;
	unsigned int gpio_data = 0;
	int ret = 0;

	for (i = 0U; i < port->gpio_num_gr; i++) {
		if (strcmp(gc->label, gpio_gr->name) == 0) {
			break;
		}
		gpio_gr++;
	}

	gpio_add_wrap_ul(port->base, gpio_gr->reg_offset, &reg);

	gpio_data = readl_relaxed((void __iomem *)reg) & bit;
	if (gpio_data > INT_MAX) {
		ret = -1;
	} else {
		ret = (int)gpio_data;
	}

	return ret;
}

static void tcc_gpio_set(struct gpio_chip *gc, unsigned int gpio_offset, int val)
{
	const struct tcc_gpio_port *port = gpiochip_get_data(gc);
	const struct tcc_gpio_group *gpio_gr = port->gpio_gr;
	unsigned int gpio_bit = gpio_offset & 0x0000001FU;
	unsigned int bit = 1U << gpio_bit;
	uintptr_t reg;
	unsigned int i;
	ulong gpio_addr = 0;

	for (i = 0U; i < port->gpio_num_gr; i++) {
		if (strcmp(gc->label, gpio_gr->name) == 0) {
			break;
		}
		gpio_gr++;
	}

	gpio_add_wrap_ul(port->base, gpio_gr->reg_offset, &reg);

	if (val != 0) {
		gpio_add_wrap_ul(reg, GPIO_BIT_SET, &gpio_addr);
		writel_relaxed(bit, (void __iomem *)gpio_addr);
	} else {
		gpio_add_wrap_ul(reg, GPIO_BIT_CLEAR, &gpio_addr);
		writel_relaxed(bit, (void __iomem *)gpio_addr);
	}

}

static int tcc_get_direction(struct gpio_chip *gc, unsigned int gpio_offset)
{
	const struct tcc_gpio_port *port = gpiochip_get_data(gc);
	const struct tcc_gpio_group *gpio_gr = port->gpio_gr;
	unsigned int gpio_bit = gpio_offset & 0x0000001FU;
	unsigned int bit = 1U << gpio_bit;
	unsigned int i;
	unsigned int output_enable_val;
	uintptr_t reg;
	int ret = 0;


	for (i = 0U; i < port->gpio_num_gr; i++) {
		if (strcmp(gc->label, gpio_gr->name) == 0) {
			break;
		}
		gpio_gr++;
	}

	gpio_add_wrap_ul(port->base, gpio_gr->reg_offset, &reg);
	gpio_add_wrap_ul(reg, 0x4U, &reg);

	output_enable_val = readl_relaxed((void __iomem *)reg);

	output_enable_val &= bit;

	if (output_enable_val == 0U) {
		ret = 1;
	}

	return ret;
}

static void tcc_irq_ack(struct irq_data *d)
{
}

#if defined(CONFIG_PINCTRL_TCC_SCFW)
static int tcc_gpio_probe_sc_get_handle(struct platform_device *pdev, struct device_node *sc_np)
{
	int ret = 0;

	sc_fw_handle = tcc_sc_fw_get_handle(sc_np);
	if (sc_fw_handle == NULL) {
		ret = -EPROBE_DEFER;
	} else if ((sc_fw_handle->version.major == 0U)
			&& (sc_fw_handle->version.minor == 0U)
			&& (sc_fw_handle->version.patch < 7U)) {
		dev_err(&(pdev->dev),
				"[ERROR][GPIO] %s : The version of SCFW is low. So, register cannot be set through SCFW.\n"
				, __func__);
		dev_err(&(pdev->dev),
				"[ERROR][GPIO] %s : SCFW Version : %d.%d.%d\n",
				__func__,
				sc_fw_handle->version.major,
				sc_fw_handle->version.minor,
				sc_fw_handle->version.patch);
		ret = -EINVAL;
	}
	return ret;
}
#endif

static void tcc_gpio_probe_find_gr(struct device_node *gpio_node, struct tcc_gpio_port *port)
{
	struct device_node *np;
	const struct property *prop;

	for_each_child_of_node(gpio_node, np) {
		prop = of_find_property(np, "gpio-controller", NULL);
		if (prop != NULL) {
			++port->gpio_num_gr;
		}
	}
}

static int tcc_gpio_probe_irq_port(struct platform_device *pdev, struct device_node *gpio_node, struct tcc_gpio_port *port, unsigned int *irq_num)
{
	int irq_num_tmp;
	int ret = 0;
	unsigned int i = 0;

	irq_num_tmp = platform_irq_count(pdev);
	if (irq_num_tmp >= 0) {
		port->irq_num = (unsigned int)irq_num_tmp;

		pr_debug("[GPIO][DEBUG] %s: irq num : %d\n", __func__, port->irq_num);
		*irq_num = port->irq_num;
	} else {
		ret = irq_num_tmp;
	}

	if (ret == 0) {
#if defined(CONFIG_ARCH_TCC897X)
		port->irq_port_map =
			devm_kcalloc(&pdev->dev, *irq_num, sizeof(int *), GFP_KERNEL);

		if (port->irq_port_map == 0) {
			ret = -ENOMEM;
		} else {
			for (i = 0U; i < *irq_num; i++) {
				port->irq_port_map[i] = devm_kcalloc(&pdev->dev, 5, sizeof(int),
				GFP_KERNEL);
			}

			for (i = 0U; i < *irq_num; i++) {
				port->irq_port_map[i][0] = i;
				ret = of_property_read_u32_index(gpio_node, "interrupts", 1 + (i * 3),
					&port->irq_port_map[i][4]);
				pr_debug("[GPIO][DEBUG] %s: interrupt port mux num : %d\n",
					__func__, port->irq_port_map[i][4]);
			}
		}
#else
		port->irq_port_map =
			devm_kcalloc(&pdev->dev, ((unsigned long)*irq_num / 2UL), sizeof(int *), GFP_KERNEL);

		if (port->irq_port_map == (void *)0) {
			ret = -ENOMEM;
		} else {
			for (i = 0U; i < (*irq_num / 2U); i++) {
				port->irq_port_map[i] = devm_kcalloc(&pdev->dev, 4, sizeof(int),
				GFP_KERNEL);
			}

			for (i = 0U; i < (*irq_num / 2U); i++) {
				ret = of_property_read_u32_index(gpio_node, "interrupts", 1U + (i * 3U),
					&port->irq_port_map[i][0]);
				pr_debug("[GPIO][DEBUG] %s: interrupt port mux num : %d\n",
					__func__, port->irq_port_map[i][0]);
			}
		}
#endif
	}
	return ret;
}

static int tcc_gpio_probe_get_irq(struct platform_device *pdev, struct device_node *gpio_node, struct tcc_gpio_port *port, unsigned int irq_num)
{
	int irq_tmp;
	int ret = 0;
	unsigned int i = 0U;

	port->irq = devm_kcalloc(&pdev->dev, irq_num, sizeof(int), GFP_KERNEL);

	if (port->irq == (void *)0) {
	    ret = -ENOMEM;
	}

	for (i = 0U; i < irq_num; i++) {
		irq_tmp = of_irq_get(gpio_node, (int)i);
		if (irq_tmp >= 0) {
			port->irq[i] = (unsigned int)irq_tmp;
		} else {
			ret = irq_tmp;
		}
	}
	return ret;
}

static int tcc_gpio_probe_set_gc_ic(struct device *dev, struct device_node *np, struct tcc_gpio_port *port, struct tcc_gpio_group *gpio_gr)
{
	int ret = 0;
	unsigned int gpio_num;
	struct gpio_irq_chip *gic = NULL;
	struct irq_chip *ic;
	struct gpio_chip *gc;

	ret = of_property_read_u32_index(np, "reg-offset", 0U,
		&gpio_gr->reg_offset);

	if (ret == 0) {
		ret = of_property_read_u32_index(np, "gpio-ranges", 3U, &gpio_num);

		if (ret == 0 && (of_device_is_available(np) == true)) {

			gc = &gpio_gr->gc;
			gc->of_node = np;
			gc->parent = dev;
			gc->label = gpio_gr->name;
			gc->ngpio = (unsigned short)gpio_num;
			gc->base = of_alias_get_id(np, "gpio");
			gc->request = gpiochip_generic_request;
			gc->free = gpiochip_generic_free;
			gc->direction_input = tcc_gpio_direction_input;
			gc->direction_output = tcc_gpio_direction_output;
			gc->get_direction = tcc_get_direction;
			gc->get = tcc_gpio_get;
			gc->set = tcc_gpio_set;

			ic = &gpio_gr->ic;
			ic->name = gpio_gr->name;
			ic->irq_ack = tcc_irq_ack;
			ic->irq_set_type = tcc_gpio_irq_set_type;
			ic->irq_disable = tcc_gpio_irq_disable;
			ic->irq_enable = tcc_gpio_irq_enable;

			gic = &gpio_gr->gc.irq;
			gic->chip = ic;
			gic->handler = handle_simple_irq;
			gic->default_type = IRQ_TYPE_NONE;

			ret = gpiochip_add_data(gc, port);
			if (ret != 0) {
				dev_err(dev, "failed to add gpiochip\n");
				gpiochip_remove(gc);
			}
		}
	}
	return ret;
}

static int tcc_gpio_probe_set_src(struct platform_device *pdev, struct tcc_gpio_group *gpio_gr, struct device_node *np)
{
	unsigned int i = 0U;
	int ret = 0;

	for (i = 0U; i < gpio_gr->source_section; i++) {
		ret = of_property_read_u32_index(np,
			"source-num", ((i * 3U) + 1U),
			&gpio_gr->gpio_offset_num[i]);

		if (ret > 0) {
			dev_err(&(pdev->dev),
		"[ERROR][PINCTRL] failed to get source offset base\n"
			);
			ret = -EINVAL;
		} else {
			ret = of_property_read_u32_index(np,
				"source-num", ((i * 3U) + 2U),
				&gpio_gr->source_offset_num[i]);

			if (ret > 0) {
				dev_err(&(pdev->dev),
			"[ERROR][PINCTRL] failed to get source base\n"
				);
				ret = -EINVAL;
			} else {
				ret = of_property_read_u32_index(np,
					"source-num", ((i * 3U) + 3U),
					&gpio_gr->source_range[i]);

				if (ret > 0) {
					dev_err(&(pdev->dev),
				"[ERROR][PINCTRL] failed to get source range\n"
					);
					ret = -EINVAL;
				}
			}
		}
	}
	return ret;
}

static int tcc_gpio_probe_source(struct platform_device *pdev, struct tcc_gpio_group *gpio_gr, struct device_node *np)
{
	int ret = 0;
	u32 source_section;

	ret = of_property_read_u32_index(np, "source-num", 0,
		&source_section);

	if (ret == 0) {
		gpio_gr->source_section = source_section;

		if (gpio_gr->source_section != 0xffU) {
			gpio_gr->gpio_offset_num =
				kcalloc(source_section, sizeof(u32), GFP_KERNEL);
			if (gpio_gr->gpio_offset_num == NULL) {
				ret =  -EINVAL;
			} else {
				gpio_gr->source_offset_num = kcalloc(source_section,
					sizeof(u32), GFP_KERNEL);
				if (gpio_gr->source_offset_num == NULL) {
					ret = -EINVAL;
				} else {
					gpio_gr->source_range =
						kcalloc(source_section, sizeof(u32),
							GFP_KERNEL);
					if (gpio_gr->source_range == NULL) {
						ret = -EINVAL;
					} else {
						ret = tcc_gpio_probe_set_src(pdev, gpio_gr, np);
					}
				}
			}
		}
	}
	return ret;
}

static int tcc_gpio_probe_set_node(struct platform_device *pdev, struct device *dev, struct tcc_gpio_port *port, struct device_node *gpio_node)
{
	int ret = 0;
	struct device_node *np;
	struct tcc_gpio_group *gpio_gr;

	port->gpio_gr = kcalloc(port->gpio_num_gr,
		sizeof(struct tcc_gpio_group), GFP_KERNEL);

	if (port->gpio_gr == (void *)0) {
	    ret = -ENOMEM;
	}

	gpio_gr = port->gpio_gr;

	for_each_child_of_node(gpio_node, np) {

		ret = of_property_read_string(np, "label", &gpio_gr->name);

		if (ret == 0) {
			ret = tcc_gpio_probe_set_gc_ic(dev, np, port, gpio_gr);
		}

		if (ret == 0) {
			ret = tcc_gpio_probe_source(pdev, gpio_gr, np);
		}

		if (ret == 0) {
			gpio_gr->port = port;
			gpio_gr++;
		}
	}
	return ret;
}

static int tcc_gpio_probe_set(struct platform_device *pdev, struct device *dev, struct device_node *gpio_node, struct tcc_gpio_port *port, unsigned int irq_num)
{
	int ret = 0;

	tcc_gpio_probe_find_gr(gpio_node, port);

	ret = tcc_gpio_probe_irq_port(pdev, gpio_node, port, &irq_num);


	if (ret == 0) {
		ret = tcc_gpio_probe_get_irq(pdev, gpio_node, port, irq_num);
	}

	if (ret == 0) {
		ret = tcc_gpio_probe_set_node(pdev, dev, port, gpio_node);
	}

	if (ret == 0) {
		platform_set_drvdata(pdev, port);
	}
	return ret;
}
static int telechips_gpio_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id =
	    of_match_device(telechips_gpio_dt_ids, &pdev->dev);
	struct device *dev = &pdev->dev;
	struct device_node *gpio_node = dev->of_node;
	struct tcc_gpio_port *port;
	const struct resource *res;
	unsigned int irq_num = 0U;
	int ret = 0;
	unsigned int i = 0U;
#if defined(CONFIG_PINCTRL_TCC_SCFW)
	struct device_node *sc_np;
#endif
	struct tcc_gpio_eint *eint;
	struct device_node *gpio_np;

	port = devm_kzalloc(&pdev->dev, sizeof(*port), GFP_KERNEL);
	if (port == 0) {
		ret = -ENOMEM;
	}
	if (ret == 0) {

		if (of_id == NULL) {
			ret = -EINVAL;
		}
		if (ret == 0) {
			port->sdata = of_id->data;
			res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
			port->base = (uintptr_t)devm_ioremap_resource(dev, res);

#if defined(CONFIG_PINCTRL_TCC_SCFW)
			port->raw_base = (unsigned int)(res->start & 0xFFFFFFFFU);
			sc_np = of_parse_phandle(gpio_node, "sc-firmware", 0);
			if (sc_np == NULL) {
				ret =  -EINVAL;
			}

			if (ret == 0) {
				ret = tcc_gpio_probe_sc_get_handle(pdev, sc_np);
			}
#endif

			if (ret == 0) {
				ret = tcc_gpio_probe_set(pdev, dev, gpio_node, port, irq_num);
			}

			if (ret == 0) {

				eint = devm_kzalloc(&pdev->dev, sizeof(*eint) * (port->irq_num / 2U), GFP_KERNEL);
				for (i = 0; i < (port->irq_num / 2U); i++) {
					gpio_np = of_parse_phandle(gpio_node, "ext-int", (i * 3U));

					if (gpio_np != NULL) {
						of_property_read_string(gpio_np, "label", &eint[i].group);
						of_property_read_u32_index(gpio_node, "ext-int", 1U + (i * 3U),
							&eint[i].pin);
						of_property_read_u32_index(gpio_node, "ext-int", 2U + (i * 3U),
							&eint[i].type);
						if (eint[i].type != IRQ_TYPE_NONE) {
							eint[i].used = 1;
						} else {
								pr_err("[GPIO][ERROR] %s: EINT %s-%d irq type is invalid\n", __func__, eint[i].group, eint[i].pin);
						}
					}
				}
				for (i = 0;  i < (port->irq_num / 2U); i++) {
					if (eint[i].used == 1) {
						tcc_gpio_request_irq(port, eint[i].group, eint[i].pin, eint[i].type);
					}
				}
			}
		}
	}
	return ret;
}

static s32 __maybe_unused tcc_gpio_suspend(struct device *dev)
{
	return 0;
}

static s32 __maybe_unused tcc_gpio_resume(struct device *dev)
{
	const struct tcc_gpio_port *port = dev_get_drvdata(dev);
	unsigned int idx_sel_reg, idx_sel_plc;
	s32 ret = 0;
	unsigned int i;
	ulong gpio_addr = 0, gpio_idx = 0;

	for (i = 0U; i < (port->irq_num / 2U); i++) {
		if (port->irq_port_map[i][3] == GPIO_EINT_IN_USE) {
			idx_sel_reg = port->irq_port_map[i][0] / 4U;
			idx_sel_plc = port->irq_port_map[i][0] % 4U;
#if defined(CONFIG_PINCTRL_TCC_SCFW)
			gpio_mul_wrap_ul(idx_sel_reg, 4U, &gpio_idx);
			gpio_add_wrap_ul(port->raw_base, GPIO_EINT_MUX, &gpio_addr);
			gpio_add_wrap_ul(gpio_addr, gpio_idx, &gpio_addr);
			gpio_mul_wrap_ul(idx_sel_plc, 8U, &gpio_idx);
			(void)request_gpio_to_sc(gpio_addr,
			gpio_idx, 8U, port->irq_port_map[i][2]);
#else
			gpio_mul_wrap_ul(idx_sel_reg, 4U, &gpio_idx);
			gpio_add_wrap_ul(port->base, GPIO_EINT_MUX, &gpio_addr);
			gpio_add_wrap_ul(gpio_addr, gpio_idx, &gpio_addr);
			gpio_mul_wrap_ul(idx_sel_plc, 8U, &gpio_idx);
			writel((readl((void __iomem *)gpio_addr) | (port->irq_port_map[i][2] << gpio_idx)), (void __iomem *)gpio_addr);
#endif
		}
	}

	return ret;
}

static SIMPLE_DEV_PM_OPS(tcc_gpio_pm_ops, tcc_gpio_suspend, tcc_gpio_resume);

static struct platform_driver telechips_gpio_driver = {
	.driver = {
		   .name = "tcc-gpio",
		   .of_match_table = telechips_gpio_dt_ids,
		   .pm = &tcc_gpio_pm_ops,
		   },
	.probe = telechips_gpio_probe,
};

static int __init tcc_gpio_drv_register(void)
{
	return platform_driver_register(&telechips_gpio_driver);
}
postcore_initcall(tcc_gpio_drv_register);


static void __exit tcc_gpio_drv_unregister(void)
{
	platform_driver_unregister(&telechips_gpio_driver);
}
module_exit(tcc_gpio_drv_unregister);

MODULE_DESCRIPTION("Telechips GPIO driver");
MODULE_LICENSE("GPL");
