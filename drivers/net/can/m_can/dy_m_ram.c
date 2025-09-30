// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <asm/io.h>

#ifdef CONFIG_PHYS_ADDR_T_64BIT
#define CAN_ADDR_SHIFT 16UL
#else
#define CAN_ADDR_SHIFT 16U
#endif

union reg_addr {
	void __iomem *addr;
	u32 addr_uc;
};

static s32 dy_m_writel(void __iomem *addr, u32 offset, u32 val)
{
	union reg_addr cal_addr;
	s32 ret = 0;

	cal_addr.addr = addr;

	if ((UINT_MAX - cal_addr.addr_uc) > offset) {
		cal_addr.addr_uc = cal_addr.addr_uc + offset;
		writel_relaxed(val, cal_addr.addr);
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static int dy_m_config_message_ram(struct device *dev)
{
	struct device_node *np = dev_of_node(dev), *np_child;
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	void __iomem *m_ram_base = devm_ioremap_resource(dev, res);
	s32 ret = 0;

	for_each_child_of_node(np, np_child)
	{
		struct resource can_res;
		struct device_node *can_node;
		u64 tmp_addr_val;
		u32 offset, addr_val;
		s32 l_ret;

		can_node = of_parse_phandle(np_child, "can", 0);

		if (!of_device_is_available(can_node))
		{
			continue;
		}

		l_ret = of_property_read_u32(np_child, "reg_offset", &offset);

		if (l_ret != 0) {
			pr_err("failed(%s): invalid reg_offset (%d, %s)\n"
				, __func__, l_ret, of_node_full_name(np_child));
			ret = l_ret;
			continue;
		}

		l_ret = of_address_to_resource(can_node, 1, &can_res);

		if (l_ret != 0) {
			pr_err("failed(%s): invalid message ram address (%d, %s)\n"
				, __func__, l_ret, of_node_full_name(np_child));
			ret = l_ret;
			continue;
		}

		tmp_addr_val = can_res.start >> CAN_ADDR_SHIFT;
		if (tmp_addr_val <= UINT_MAX) {
			addr_val = tmp_addr_val;
		} else {
			pr_err("failed(%s): invalid message ram address (%d, %s)\n"
				, __func__, l_ret, of_node_full_name(np_child));
			ret = -EINVAL;
			continue;
		}

		l_ret = dy_m_writel(m_ram_base, offset, addr_val);

		if (l_ret != 0) {
			pr_err("failed(%s): failed to write address (%d, %s)\n"
			       , __func__, l_ret, of_node_full_name(np_child));
			ret = l_ret;
		}
	}

	return ret;
}

int dy_m_ram_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	return dy_m_config_message_ram(dev);
}

int dy_m_ram_remove(struct platform_device *pdev)
{
	return 0;
}

int dy_m_ram_suspend(struct device *dev)
{
	return 0;
}

int dy_m_ram_resume(struct device *dev)
{
	return dy_m_config_message_ram(dev);
}

static SIMPLE_DEV_PM_OPS(dy_m_ram_pmops, dy_m_ram_suspend, dy_m_ram_resume);

static const struct of_device_id dy_m_ram_of_table[] = {
	{ .compatible = "dy_m_can", .data = NULL },
	{},
};
MODULE_DEVICE_TABLE(of, dy_m_ram_of_table);

static struct platform_driver dy_m_ram_driver = {
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = dy_m_ram_of_table,
		.pm     = &dy_m_ram_pmops,
	},
	.probe = dy_m_ram_probe,
	.remove = dy_m_ram_remove,
};

module_platform_driver(dy_m_ram_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jaeyoung Park <dwayne.park@telechips.com>");
MODULE_DESCRIPTION("Dynamic message ram driver");


