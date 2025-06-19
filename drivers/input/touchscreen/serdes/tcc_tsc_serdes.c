// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/input/tcc_tsc_serdes.h>
#include <linux/of_device.h>

#include "max96751.h"
#include "max96851.h"
#include "max96878.h"

#define MODULE_NAME "tsc_serdes"

static const struct regmap_config tsc_serdes_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
};

int tcc_tsc_serdes_update(
		struct i2c_client *client,
		struct regmap *i2c_regmap,
		unsigned int disp_num,
		unsigned int board_type)
{
	unsigned short addr = client->addr;
	const struct i2c_data *ser_reg = NULL;
	const struct i2c_data *des_reg = NULL;
	unsigned char buf[3] = {0,};
	int i, ret = 0;

	dev_info(&client->dev,
		"[INFO][TSC_SERDES] board type 0x%x, display num %d\n",
		board_type,
		disp_num);
	switch (board_type) {
	case TCC803X_EVB:
	case TCC803XPE_EVB:
		ser_reg = tcc803x_hdmi_ser_regs;
		des_reg = tcc803x_hdmi_des_regs;
		break;
	case TCC805X_EVB:
	case TCC8050_53_EVB:
	case TCC8059_EVB:
		ser_reg = tcc805x_dp_ser_regs;
		des_reg = tcc805x_dp_des_regs;
		break;
	case TCC807X_EVB:
	case TCC8070_EVB:
		ser_reg = tcc807x_dp_ser_regs;
		des_reg = tcc807x_dp_des_regs;
		break;
	default:
		dev_err(&client->dev,
			"[ERROR][TSC_SERDES] %s: Not supported TCC EVB(0x%x)\n",
			__func__, board_type);
		ret = -ENOMEM;
		break;
	}

	if (ret == 0) {
		/* update register of serializer */
		for (i = 0; ser_reg[i].addr != 0U; i++) {
			if (((board_type & ser_reg[i].board) == 0U) ||
				(disp_num <= ser_reg[i].disp_idx)) {
				continue;
			}

			client->addr = (ser_reg[i].addr >> 1U);
			if (i2c_regmap != NULL) {
				ret = regmap_write(i2c_regmap,
						ser_reg[i].reg,
						ser_reg[i].val);
			} else {
				buf[0] = (unsigned char)(ser_reg[i].reg >> 8);
				buf[1] = (unsigned char)(ser_reg[i].reg & 0xFFU);
				buf[2] = (unsigned char)(ser_reg[i].val & 0xFFU);
				ret = i2c_master_send(client, (const char*)buf, SER_DES_W_LEN);
				if (ret == SER_DES_W_LEN) {
					ret = 0;
				}
			}

			if (ret < 0) {
				dev_err(&client->dev,
					"[ERR][TSC_SERDES] SER, %s: failed to write i2c data[%d] (%d)\n",
					__func__, i, ret);
			}

			dev_dbg(&client->dev,
				"[DEBUG][TSC_SERDES][%d] A_0x%02x R_0x%02x V_0x%02x\n",
				i, ser_reg[i].addr,
				ser_reg[i].reg, ser_reg[i].val);
		}
	}

	if (ret == 0) {
		/* update register of deserializer */
		for (i = 0; des_reg[i].addr != 0U; i++) {
			if (((board_type & des_reg[i].board) == 0U) ||
				(disp_num <= des_reg[i].disp_idx)) {
				continue;
			}

			client->addr = (des_reg[i].addr >> 1U);
			if (i2c_regmap != NULL) {
				ret = regmap_write(i2c_regmap,
						des_reg[i].reg,
						des_reg[i].val);
			} else {
				buf[0] = (unsigned char)(des_reg[i].reg >> 8);
				buf[1] = (unsigned char)(des_reg[i].reg & 0xFFU);
				buf[2] = (unsigned char)(des_reg[i].val & 0xFFU);
				ret = i2c_master_send(client, (char*)buf, SER_DES_W_LEN);
				if (ret == SER_DES_W_LEN) {
					ret = 0;
				}
			}

			if (ret < 0) {
				dev_warn(&client->dev,
					"[WARN][TSC_SERDES] DES, %s: failed to write i2c data[%d] (%d)\n",
					__func__, i, ret);
			}

			dev_dbg(&client->dev,
				"[DEBUG][TSC_SERDES][%d] A_0x%02x R_0x%02x V_0x%02x\n",
				i, des_reg[i].addr,
				des_reg[i].reg, des_reg[i].val);
		}
	}

	client->addr = addr;

	return 0;
}

static int tsc_serdes_get_info(struct serdes_info *info)
{
	struct i2c_client *client = info->client;
	struct i2c_data *ser_reg = NULL;
	struct i2c_data *des_reg = NULL;
	unsigned short addr = client->addr;
	int ret = 0;
	unsigned int i = 0;

	info->act_disp = 0;

	switch (info->board_type) {
	case TCC803X_EVB:
	case TCC803XPE_EVB:
		ser_reg = tcc803x_hdmi_ser_rev;
		des_reg = tcc803x_hdmi_des_rev;
		break;
	case TCC805X_EVB:
	case TCC8050_53_EVB:
	case TCC8059_EVB:
		ser_reg = tcc805x_dp_ser_rev;
		des_reg = tcc805x_dp_des_rev;
		break;
	case TCC807X_EVB:
	case TCC8070_EVB:
		ser_reg = tcc807x_dp_ser_rev;
		des_reg = tcc807x_dp_des_rev;
		break;
	default:
		dev_err(&client->dev,
			"[ERROR][TSC_SERDES] %s: Not supported TCC EVB(0x%x)\n",
			__func__, info->board_type);
		ret = -ENOMEM;
		break;
	}

	if (ret == 0) {
		/* get serializer revision */
		client->addr = (ser_reg[i].addr >> 1U);
		ret = regmap_read(info->tsc_regmap, ser_reg[i].reg, (unsigned int*)&ser_reg[i].val);
		if (ret < 0) {
			dev_err(&client->dev,
				"[ERROR][TSC_SERDES] %s: failed to get serializer device revision\n",
				__func__);
		} else {
			dev_info(&client->dev,
				"[INFO][TSC_SERDES] serializer revision: v%d\n",
				ser_reg[i].val);
		}
	}

	if (ret == 0) {
		for (i = 0U; i<info->max_disp; i++) {
			/* get active deserializer count */
			client->addr = (des_reg[i].addr >> 1U);
			ret = regmap_read(info->tsc_regmap, des_reg[i].reg, (unsigned int*)&des_reg[i].val);
			if (ret == 0) {
				dev_info(&client->dev,
					"[INFO][TSC_SERDES] deserializer[%d] revision: v%d\n",
					info->act_disp,
					des_reg[i].val);
				info->act_disp++;
			}
		}

		if (info->act_disp == 0U) {
			ret = -ENODEV;
		} else {
			ret = 0;
		}
	}

	client->addr = addr;

	return ret;
}

static const struct of_device_id tsc_serdes_of_match[] = {
	{	.compatible = "telechips,tcc803x-tsc-serdes",
		.data = (void *)TCC803X_EVB, },
	{	.compatible = "telechips,tcc805x-tsc-serdes",
		.data = (void *)TCC805X_EVB, },
	{	.compatible = "telechips,tcc807x-tsc-serdes",
		.data = (void *)TCC807X_EVB, },
	{},
};
MODULE_DEVICE_TABLE(of, tsc_serdes_of_match);

int tcc_tsc_device_match_of_node(struct device *dev, const void *np)
{
	return dev->of_node == np;
}

static int tcc_tsc_serdes_touch_ic_addr(struct device *dev, struct serdes_info *info)
{
	struct i2c_data *des_reg = NULL;
	int i, ret = 0;

	switch (info->board_type) {
	case TCC805X_EVB:
	case TCC8050_53_EVB:
	case TCC8059_EVB:
		des_reg = tcc805x_dp_des_regs;
		break;
	case TCC807X_EVB:
	case TCC8070_EVB:
		des_reg = tcc807x_dp_des_regs;
		break;
	default:
		dev_err(dev,
			"[ERROR][TSC_SERDES] %s: Not supported TCC EVB(0x%x)\n",
			__func__, info->board_type);
		ret = -ENOMEM;
		break;
	}

	/* slave addr change */
	for (i = 0; des_reg[i].addr != 0U; i++) {
		/* re assignment touch ic address serdes -> touch ic */
		if (des_reg[i].reg == 0x253 || des_reg[i].reg == 0x257) {
			des_reg[i].val = info->lcd_touch_addr;

			if (info->lcd_touch_addr == 0x94) {
				dev_info(dev, "[INFO][TSC_SERDES] BOE touch IC\n");
			} else if (info->lcd_touch_addr == 0xBA) {
				dev_info(dev, "[INFO][TSC_SERDES] AUO touch IC\n");
			} else {
				dev_err(dev, "[ERROR][TSC_SERDES] wrong touch IC address \n");
				ret = -EINVAL;
				break;
			}
		}
	}

	return ret;
}

static int tcc_tsc_serdes_parse_dt(struct device *dev, struct serdes_info *info)
{
	const struct of_device_id *match;
	const struct device_node *np = dev->of_node;
	unsigned int board_type;
	unsigned int lcd_touch_addr;
	struct device_node *dp_np;
	struct device *dp_dev;
	int ret = 0;

	info->dp_link = NULL;

	match = of_match_node(tsc_serdes_of_match, np);
	if (match == NULL) {
		ret = -EINVAL;
	}
	if (ret == 0) {
		ret = of_property_read_u32(np, "board-type", &board_type);
		if (ret < 0) {
			dev_err(dev,
				"[ERROR][TSC_SERDES] Can't get board type\n");
		} else {
			info->board_type = board_type;

			ret = of_property_read_u32(np, "max-disp-num", &info->max_disp);
			if (ret < 0) {
				dev_warn(dev,
					"[WARN][TSC_SERDES] Can't get number of display, set to default\n");
				info->max_disp = 4;
				ret = 0;
			}
		}
	}

	/* AUO and BOE select */
	if (ret == 0) {
		ret = of_property_read_u32(np, "lcd-touch-addr", &lcd_touch_addr);

		if (ret < 0) {
			dev_err(dev,
				"[ERROR][TSC_SERDES] Can't get lcd touch addrress\n");
		} else {
			info->lcd_touch_addr = lcd_touch_addr;

			ret = tcc_tsc_serdes_touch_ic_addr(dev, info);
		}
	}

	if (ret == 0) {
		dp_np = of_parse_phandle(np, "dp-handle", 0);
		if (dp_np == NULL) {
			ret = -ENODEV;
		}
		if (ret == 0) {
			dp_dev = bus_find_device(&platform_bus_type, NULL, dp_np, tcc_tsc_device_match_of_node);
			if (dp_dev == NULL) {
				ret = -ENODEV;
			}
		}

		if (ret == 0) {
			info->dp_link = device_link_add(dev, dp_dev, DL_FLAG_STATELESS);
			if (info->dp_link == NULL) {
				ret = -ENODEV;
			}
		}

		if (ret == 0) {
			dev_info(dev,
				"[INFO][TSC_SERDES] Added device link with DP driver\n");
		}

		of_node_put(dp_np);
		ret = 0;
	}

	return ret;
}

static int tsc_serdes_probe(
		struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct serdes_info *info;
	int ret = 0;

	info = (struct serdes_info*)devm_kzalloc(&client->dev,
			sizeof(struct serdes_info),
			GFP_KERNEL);
	if (info == NULL) {
		ret = -ENOMEM;
	}

	if (ret == 0) {
		ret = tcc_tsc_serdes_parse_dt(&client->dev, info);
	}

	if (ret == 0) {
		info->client = client;
		info->tsc_regmap = devm_regmap_init_i2c(client, &tsc_serdes_regmap_config);
		if (IS_ERR((void*)info->tsc_regmap)) {
			ret = PTR_ERR((void*)info->tsc_regmap);
			dev_err(&client->dev,
				"[ERROR][TSC_SERDES] failed to initialize regmap\n");
		}
	}

	if (ret == 0) {
		/* get ser information of device */
		ret = tsc_serdes_get_info(info);
		if (ret < 0) {
			dev_err(&client->dev,
				"[ERROR][TSC_SERDES] failed to get device information\n");
		}
	}

	if (ret == 0) {
		i2c_set_clientdata(client, info);

		/* update serdes regs */
		ret = tcc_tsc_serdes_update(
			client,
			info->tsc_regmap,
			info->act_disp,
			info->board_type);
		if (ret < 0) {
			dev_err(&client->dev,
				"[ERROR][TSC_SERDES] failed to update Ser/Des register\n");
		} else {
			dev_info(&client->dev,
				"[INFO][TSC_SERDES] Success to update Ser/Des register for Touch (board:0x%x)\n",
				info->board_type);
		}
	}

	if (ret != 0) {
		if (info->dp_link != NULL) {
			device_link_del(info->dp_link);
		}
	}

	return ret;
}

static int tsc_serdes_remove(struct i2c_client *client)
{
	const struct serdes_info *info = (struct serdes_info *)i2c_get_clientdata(client);
	if (info->dp_link != NULL) {
		device_link_del(info->dp_link);
	}

	i2c_set_clientdata(client, NULL);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tsc_serdes_suspend(struct device *dev)
{
	return 0;
}

static int tsc_serdes_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client((dev));
	const struct serdes_info *info = (struct serdes_info *)i2c_get_clientdata(client);
	int error = 0;

	/* update serdes regs */
	error = tcc_tsc_serdes_update(
		client,
		info->tsc_regmap,
		info->act_disp,
		info->board_type);
	if (error < 0) {
		dev_err(&client->dev, "[ERROR][TSC_SERDES]failed to update serdes reg\n");
	}

	return error;
}
static SIMPLE_DEV_PM_OPS((tsc_serdes_pm), (tsc_serdes_suspend), (tsc_serdes_resume));
#endif

static const struct i2c_device_id serdes_id_table[] = {
	{MODULE_NAME, 0},
	{},
};

static struct i2c_driver tsc_serdes_driver = {
	.probe = tsc_serdes_probe,
	.remove = tsc_serdes_remove,
	.driver = {
		.name   = MODULE_NAME,
		.of_match_table = of_match_ptr(tsc_serdes_of_match),
		.pm = &tsc_serdes_pm,
	},
	.id_table = serdes_id_table,
};

static int __init tsc_serdes_init(void)
{
	return i2c_add_driver(&tsc_serdes_driver);
}
module_init(tsc_serdes_init);

static void __exit tsc_serdes_exit(void)
{
	i2c_del_driver(&tsc_serdes_driver);
}
module_exit(tsc_serdes_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_LICENSE("GPL");
