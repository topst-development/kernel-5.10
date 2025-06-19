/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it under
 *the terms of the GNU General Public License as published by the Free Software
 *Foundation; either version 2 of the License, or (at your option) any later
 *version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 *ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 *Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <media/v4l2-async.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <linux/kdev_t.h>
#include <linux/of_gpio.h>
#include <video/videomode.h>

#include "dsih_serdes.h"
//#include <media/v4l2-ctrls.h>
//#include <media/v4l2-dev.h>
//#include <media/v4l2-subdev.h>
//#include <video/tcc/vioc_vin.h>

#define LOG_TAG			"MAX9XXXX"
#define loge(fmt, ...)		\
		(void)pr_err("[ERROR][%s] %s - "\
			fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logw(fmt, ...)		\
		(void)pr_warn("[WARN][%s] %s - "\
			fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logd(fmt, ...)		\
		(void)pr_debug("[DEBUG][%s] %s - "\
			fmt, LOG_TAG, __func__, ##__VA_ARGS__)
#define logi(fmt, ...)		\
		(void)pr_info("[INFO][%s] %s - "\
			fmt, LOG_TAG, __func__, ##__VA_ARGS__)

struct max9XXXX {
	/* Regmaps */
	//struct regmap			*regmap;
	struct i2c_client		*pi2cClient;
	struct mutex lock;

};

#define SER_DES_DEALY	0xFFU

#define MAX96789_SER_ADDR   0x80U    // 0x80 >> 1    0x40
#define MAX96878_DES_ADDR   0xD4U    // 0xD4 >> 1    0x6A

#define MAX96789_SER_DEV_NUM	0
#define MAX96878_DES_DEV_NUM	1

#define SER_DES_INDEX_MAX 2U

static struct max9XXXX stMax9xxxx_dev[SER_DES_INDEX_MAX];

struct max9xxxx_reg_data {
	unsigned int uiDev_addr;
	unsigned int uiReg_addr;
	unsigned int uiReg_Val;
	unsigned int uiReg_len;
};

static struct max9xxxx_reg_data stSerMAX96789_MAX96851[] = {
	/* Device ADDR, Reg Addr, Value */

	{MAX96789_SER_ADDR, 0x010, 0x80, 2},
	{MAX96789_SER_ADDR, 0x010, 0x11, 2},
	{SER_DES_DEALY, 0xFFFF, 0xFF, 0xF}, // Delay

	{MAX96789_SER_ADDR, 0x330, 0x06, 2}, // Set phy_config
	{MAX96789_SER_ADDR, 0x331, 0x11, 2}, // Number of Lanes default 2lanes

	// Enable A link -> X transmit X channal
	{MAX96789_SER_ADDR, 0x010, 0x23, 2}, {MAX96789_SER_ADDR, 0x002, 0x53, 2},

	// HSYNC_WIDTH_L / VSYNC_WIDTH_L
	{MAX96789_SER_ADDR, 0x385, 0x08, 2}, {MAX96789_SER_ADDR, 0x386, 0x01, 2},
	{MAX96789_SER_ADDR, 0x387, 0x00, 2}, // HSYNC_WIDTH_H/VSYNC_WIDTH_H
	// VFP_L / VBP_H
	{MAX96789_SER_ADDR, 0x3A5, 0x0A, 2}, {MAX96789_SER_ADDR, 0x3A7, 0x00, 2},
	{MAX96789_SER_ADDR, 0x3A6, 0xA0, 2}, // VFP_H/VBP_L
	// VRES_L / VRES_H
	{MAX96789_SER_ADDR, 0x3A8, 0xD0, 2}, {MAX96789_SER_ADDR, 0x3A9, 0x02, 2},
	// HFP_L / HBP_H
	{MAX96789_SER_ADDR, 0x3AA, 0x1C, 2}, {MAX96789_SER_ADDR, 0x3AC, 0x01, 2},
	{MAX96789_SER_ADDR, 0x3AB, 0xC0, 2}, // HFP_H/HBP_L
	// HRES_L / HRES_H
	{MAX96789_SER_ADDR, 0x3AD, 0x80, 2}, {MAX96789_SER_ADDR, 0x3AE, 0x07, 2},
	{MAX96789_SER_ADDR, 0x3A4, 0xC1, 2}, // FIFO/DESKEW_EN
	
	{MAX96878_DES_ADDR, 0x0005, 0xB0, 2}, // GMSL2 mode w/ sink mode
	
	{MAX96878_DES_ADDR, 0x0005, 0xB0, 2}, // GMSL2 mode w/ sink mode
	{MAX96878_DES_ADDR, 0x01CE, 0x4E, 2}, // DES oLDI setting		// GPIO / I2C Setting
	{MAX96789_SER_ADDR, 0x001, 0x8, 2}, // I2C pass-through

	{MAX96789_SER_ADDR, 0x380, 0x0F, 2}, // Pol
	{MAX96789_SER_ADDR, 0x390, 0x0F, 2}, // Pol
	

	// MFP2 (GPIO02) - MFP18 (GPIO18) // LCD_ON
	{MAX96789_SER_ADDR, 0x2C4, 0x83, 2}, // GPIO2 Ser setting
	{MAX96789_SER_ADDR, 0x2C5, 0xAA, 2}, // SER GPIO_TX_ID  : 0x2 (to GPIO18)
	{MAX96878_DES_ADDR, 0x236, 0x84, 2}, // GPIO18 Des setting
	{MAX96878_DES_ADDR, 0x238, 0x6A, 2}, // DES GPIO_RX_ID	: 0x1 (from GPIO2)

	// MFP3 (GPIO03) - MFP17 (GPIO17) // RESET
	{MAX96789_SER_ADDR, 0x2C7, 0x83, 2}, // GPIO3 Ser setting
	{MAX96789_SER_ADDR, 0x2C8, 0xAB, 2}, // SER GPIO_TX_ID  : 0x2 (to GPIO17)
	{MAX96878_DES_ADDR, 0x233, 0x84, 2}, // GPIO18 Des setting
	{MAX96878_DES_ADDR, 0x235, 0x6B, 2}, // DES GPIO_RX_ID	: 0x1 (from GPIO2)

	// MFP7 (GPIO07) - MFP2 (GPIO2) // BL_EN
	{MAX96789_SER_ADDR, 0x2D3, 0x83, 2}, // GPIO3 Ser setting
	{MAX96789_SER_ADDR, 0x2D4, 0xAC, 2}, // SER GPIO_TX_ID  : 0x2 (to GPIO17)
	{MAX96878_DES_ADDR, 0x206, 0x84, 2}, // GPIO18 Des setting
	{MAX96878_DES_ADDR, 0x208, 0x6C, 2}, // DES GPIO_RX_ID	: 0x1 (from GPIO2)
	//{MAX96789_SER_ADDR, 0x3a4, 0x1, 2}, // skwe
    {0,0,0,2}
};

static int max9xxxx_i2c_write(const struct i2c_client *client, unsigned int reg, unsigned int val, unsigned int len)
{
	int cnt;
	int ret;
	if(len == 1U) {
		unsigned char buffer[2];
		cnt = 2;
		(void)memset(&buffer, 0x00, sizeof(buffer));
		buffer[0] = (unsigned char)(reg & 0xFFU);
		buffer[1] = (unsigned char)(val & 0xFFU);

		ret = i2c_master_send((const struct i2c_client *)client, (const char *)buffer, cnt);
	} else {
		unsigned char buffer[3];
		cnt = 3;
		(void)memset(&buffer, 0x00, sizeof(buffer));
		buffer[0] = (unsigned char)((reg >> 8) & 0xFFU);
		buffer[1] = (unsigned char)(reg & 0xFFU);
		buffer[2] = (unsigned char)(val & 0xFFU);

		ret = i2c_master_send((const struct i2c_client *)client, (const char *)buffer, cnt);
	}

	if(ret != cnt) {
		ret = -EIO;
	} else {
		(void)pr_info("Write I2C Dev 0x%x: Reg address 0x%x -> 0x%x", ( client->addr << 1 ), reg, val);
	}
	return ret;
}

static unsigned int max9xxxx_get_dev_index(unsigned int devaddr)
{
	unsigned int ret;
	const struct max9XXXX *pstMax9xxx_dev;

	for(ret = 0U ; ret < SER_DES_INDEX_MAX ; ret++) {
		pstMax9xxx_dev = &stMax9xxxx_dev[ret];
		if(pstMax9xxx_dev->pi2cClient != NULL) {
			if(pstMax9xxx_dev->pi2cClient->addr == (devaddr >> 1) ) {
				break;
			}
		}
	}
	return ret;
}

int max9xxxx_reg_set(unsigned int lane, struct videomode video_mode)
{
	unsigned int index;
	int retry;
	unsigned int serdesindex;
	int ret = 0;

	const struct max9XXXX *pstMax9xxx_dev;
	struct max9xxxx_reg_data *pstSERDES_Reg_info;

	pstSERDES_Reg_info = stSerMAX96789_MAX96851;

	for(index = 0U; !((pstSERDES_Reg_info[index].uiDev_addr == 0U) && (pstSERDES_Reg_info[index].uiReg_addr == 0U)); index++) {
		// check delay
		if(pstSERDES_Reg_info[index].uiDev_addr == SER_DES_DEALY) {
			mdelay(pstSERDES_Reg_info[index].uiReg_Val);
			continue;
		}
		if(ret == 0) {
	        serdesindex = max9xxxx_get_dev_index(pstSERDES_Reg_info[index].uiDev_addr);
			if(serdesindex < SER_DES_INDEX_MAX) {
			    pstMax9xxx_dev = &stMax9xxxx_dev[serdesindex];
			} else {
				ret = -EINVAL;
				break;
			}

	        if(pstSERDES_Reg_info[index].uiReg_addr == 0x331U) {
	            pstSERDES_Reg_info[index].uiReg_Val = lane - 1U;
	        }
			if(pstSERDES_Reg_info[index].uiReg_addr == 0x0385U) {
				pstSERDES_Reg_info[index].uiReg_Val = video_mode.hsync_len & 0xffu;
			}
			if(pstSERDES_Reg_info[index].uiReg_addr == 0x0386U) {
				pstSERDES_Reg_info[index].uiReg_Val = video_mode.vsync_len & 0xffu;
			}
			if(pstSERDES_Reg_info[index].uiReg_addr == 0x0387U) {
				pstSERDES_Reg_info[index].uiReg_Val = (((video_mode.hsync_len >> 8) & 0xfu) << 4) | ((video_mode.hsync_len >> 8) & 0xfu);
			}
			if(pstSERDES_Reg_info[index].uiReg_addr == 0x03A5U) {
				pstSERDES_Reg_info[index].uiReg_Val = video_mode.vfront_porch & 0xffu;
			}
			if(pstSERDES_Reg_info[index].uiReg_addr == 0x03A7U) {
				pstSERDES_Reg_info[index].uiReg_Val = (video_mode.vback_porch >> 4) & 0xffu;
			}
			if(pstSERDES_Reg_info[index].uiReg_addr == 0x03A6U) {
				pstSERDES_Reg_info[index].uiReg_Val = ((video_mode.vback_porch & 0xfu) << 4) | ((video_mode.vfront_porch >> 8) & 0xfu);
			}
			if(pstSERDES_Reg_info[index].uiReg_addr == 0x03A8U) {
				pstSERDES_Reg_info[index].uiReg_Val = video_mode.vactive & 0xffu;
			}
			if(pstSERDES_Reg_info[index].uiReg_addr == 0x03A9U) {
				pstSERDES_Reg_info[index].uiReg_Val = ((video_mode.vactive >> 8) & 0xfu);
			}
			if(pstSERDES_Reg_info[index].uiReg_addr == 0x03AAU) {
				pstSERDES_Reg_info[index].uiReg_Val = video_mode.hfront_porch & 0xffu;
			}
			if(pstSERDES_Reg_info[index].uiReg_addr == 0x03ACU) {
				pstSERDES_Reg_info[index].uiReg_Val = (video_mode.hback_porch >> 4) & 0xffu;
			}
			if(pstSERDES_Reg_info[index].uiReg_addr == 0x03ABU) {
				pstSERDES_Reg_info[index].uiReg_Val = ((video_mode.hback_porch & 0xfu) << 4) | ((video_mode.hfront_porch >> 8) & 0xfu);
			}
			if(pstSERDES_Reg_info[index].uiReg_addr == 0x03ADU) {
				pstSERDES_Reg_info[index].uiReg_Val = video_mode.hactive & 0xffu;
			}
			if(pstSERDES_Reg_info[index].uiReg_addr == 0x03AEU) {
				pstSERDES_Reg_info[index].uiReg_Val = (video_mode.hactive >> 8) & 0xffu;
			}

			for(retry = 100; retry >= 0 ; retry --) {
#if 1
				ret = max9xxxx_i2c_write(pstMax9xxx_dev->pi2cClient, pstSERDES_Reg_info[index].uiReg_addr, pstSERDES_Reg_info[index].uiReg_Val, pstSERDES_Reg_info[index].uiReg_len);
#else
				ret = max9xxxx_i2c_write(pstMax9xxx_dev->pi2cClient,
										pstSERDES_Reg_info[index].uiReg_addr,
										pstSERDES_Reg_info[index].uiReg_Val,
										pstSERDES_Reg_info[index].uiReg_len);
#endif
				if(ret > 0) {
					if(retry != 100) {
						(void)pr_err("[%s] success on retry %d\n", __func__, 100-retry);
					}
					ret = 0;
					break;
				} else {
					if(retry != 0) {
						mdelay(10);
						continue;
					} else {
						(void)pr_err("[%s] fail to write I2C dsi serdes\n", __func__);
						ret = -EINVAL;
					}
				}
			}
		}
	}
	return ret;
}

static struct max9XXXX max9XXXX_data = {
};

static const struct i2c_device_id max9XXXX_id[] = {
	{ "max9XXXX", 0, },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max9XXXX_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id max9XXXX_of_match[] = {
	{
		.compatible	= "maxim,max9XXXX",
		.data		= &max9XXXX_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, max9XXXX_of_match);
#endif


static int max9XXXX_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct max9XXXX *dev;
	int ret = 0;

	(void)id;
	
	if(client->addr == (MAX96789_SER_ADDR >> 1U)) { // LVDS0 SER 0x22
		dev = &stMax9xxxx_dev[MAX96789_SER_DEV_NUM];
		(void)pr_info("MAX96789_SER_DEV Probe\n");
	} else if (client->addr == (MAX96878_DES_ADDR>>1U)) { // LVDS0 DES 0x48
		dev = &stMax9xxxx_dev[MAX96878_DES_DEV_NUM];
		(void)pr_err("MAX96878_DES_DEV Probe\n");
	} else {
		(void)pr_err("Invalid dev addr check device tree\n");
		ret = -1;
	}

    if(ret == 0) {
	    dev->pi2cClient = client;
        i2c_set_clientdata(client, dev);

        if(dev->pi2cClient == NULL) {
            loge("i2c client store err\n");
        }

        loge("flag: %x,  chnum: %d \n", (client->flags)<<1, i2c_adapter_id(client->adapter));
        loge("name: %s, addr: 0x%x, client: 0x%p\n",
            dev->pi2cClient->name, (dev->pi2cClient->addr)<<1, dev->pi2cClient);

    }

	return ret;
}


static struct i2c_driver max9XXXX_driver = {
	.probe		= max9XXXX_probe,
	//.remove		= max9XXXX_remove,
	.driver		= {
		.name		= "max9XXXX",
		.of_match_table	= of_match_ptr(max9XXXX_of_match),
	},
	.id_table	= max9XXXX_id,
};

module_i2c_driver(max9XXXX_driver);
