// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/err.h>

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kdev_t.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/debugfs.h>

#include <linux/cdev.h>
#include <linux/atomic.h>

#include <linux/firmware/tcc_ipi.h>
#include <linux/tcc_snor_updater_dev.h>

#define SNOR_UPDATER_DEV_NAME        ("tcc_snor_updater")
#define SNOR_UPDATER_DEV_MINOR       (0)

#define MAX_MSG_DATA_SIZE		(256U)
#define MAX_MSG_CMD_SIZE		(MBOX_MSG_CMD_MAX_LEN << 2)

#define SNOR_UPDATER_ACK_TIMEOUT_MS	(5000U) /* msec */
#define SNOR_UPDATER_ERASE_TIMEOUT_MS	(60000U) /* msec */

#define SNOR_UPDATER_SUCCESS					(0)
#define SNOR_UPDATER_ERR_COMMON					(-1)
#define SNOR_UPDATER_ERR_ARGUMENT				(-2)
#define SNOR_UPDATER_ERR_NOTREADY				(-3)
#define SNOR_UPDATER_ERR_TIMEOUT				(-4)
#define SNOR_UPDATER_ERR_UNKNOWN_CMD			(-5)
#define SNOR_UPDATER_ERR_NACK					(-6)
#define SNOR_UPDATER_ERR_SNOR_FIRMWARE_FAIL		(-7)
#define SNOR_UPDATER_ERR_SNOR_INIT_FAIL			(-8)
#define SNOR_UPDATER_ERR_SNOR_ACCESS_FAIL		(-9)
#define SNOR_UPDATER_ERR_CRC_ERROR				(-10)

#define SNOR_UPDATE_ACK			(1)
#define SNOR_UPDATE_NACK		(2)

#define UPDATE_START	(0x01UL)
#define UPDATE_READY	(0x02UL)
#define UPDATE_FW_START	(0x03UL)
#define UPDATE_FW_READY	(0x04UL)
#define UPDATE_FW_SEND	(0x05UL)
#define UPDATE_FW_SEND_ACK	(0x06UL)
#define UPDATE_FW_DONE	(0x07UL)
#define UPDATE_FW_COMPLETE	(0x08UL)
#define UPDATE_DONE	(0x09UL)
#define UPDATE_COMPLETE	(0x0AUL)

struct snor_updater_device {
	struct platform_device *pdev;
	struct device *dev;
	struct cdev chrdev;
	struct class *updater_class;
	dev_t devnum;

	int is_opened;
	struct mutex dev_mutex;

	struct device_node *mbox_client;
	struct tcc_ipi_msg rx_data;
	struct tcc_ipi_msg tx_data;
	struct completion cmpl;
	int index;
};

/*****************************************************************/
/*                                                               */
/* CRC LOOKUP TABLE                                              */
/* ================                                              */
/* The following CRC lookup table was generated automagically    */
/* by the Rocksoft^tm Model CRC Algorithm Table Generation       */
/* Program V1.0 using the following model parameters:            */
/*                                                               */
/*    Width   : 4 bytes.                                         */
/*    Poly    : 0x8001801BL                                      */
/*    Reverse : TRUE.                                            */
/*                                                               */
/* For more information on the Rocksoft^tm Model CRC Algorithm,  */
/* see the document titled "A Painless Guide to CRC Error        */
/* Detection Algorithms" by Ross Williams                        */
/* (ross@guest.adelaide.edu.au.). This document is likely to be  */
/* in the FTP archive "ftp.adelaide.edu.au/pub/rocksoft".        */
/*                                                               */
/*****************************************************************/

static const uint32_t crc32_table[256] = {
	0x00000000U, 0x90910101U, 0x91210201U, 0x01B00300U,
	0x92410401U, 0x02D00500U, 0x03600600U, 0x93F10701U,
	0x94810801U, 0x04100900U, 0x05A00A00U, 0x95310B01U,
	0x06C00C00U, 0x96510D01U, 0x97E10E01U, 0x07700F00U,
	0x99011001U, 0x09901100U, 0x08201200U, 0x98B11301U,
	0x0B401400U, 0x9BD11501U, 0x9A611601U, 0x0AF01700U,
	0x0D801800U, 0x9D111901U, 0x9CA11A01U, 0x0C301B00U,
	0x9FC11C01U, 0x0F501D00U, 0x0EE01E00U, 0x9E711F01U,
	0x82012001U, 0x12902100U, 0x13202200U, 0x83B12301U,
	0x10402400U, 0x80D12501U, 0x81612601U, 0x11F02700U,
	0x16802800U, 0x86112901U, 0x87A12A01U, 0x17302B00U,
	0x84C12C01U, 0x14502D00U, 0x15E02E00U, 0x85712F01U,
	0x1B003000U, 0x8B913101U, 0x8A213201U, 0x1AB03300U,
	0x89413401U, 0x19D03500U, 0x18603600U, 0x88F13701U,
	0x8F813801U, 0x1F103900U, 0x1EA03A00U, 0x8E313B01U,
	0x1DC03C00U, 0x8D513D01U, 0x8CE13E01U, 0x1C703F00U,
	0xB4014001U, 0x24904100U, 0x25204200U, 0xB5B14301U,
	0x26404400U, 0xB6D14501U, 0xB7614601U, 0x27F04700U,
	0x20804800U, 0xB0114901U, 0xB1A14A01U, 0x21304B00U,
	0xB2C14C01U, 0x22504D00U, 0x23E04E00U, 0xB3714F01U,
	0x2D005000U, 0xBD915101U, 0xBC215201U, 0x2CB05300U,
	0xBF415401U, 0x2FD05500U, 0x2E605600U, 0xBEF15701U,
	0xB9815801U, 0x29105900U, 0x28A05A00U, 0xB8315B01U,
	0x2BC05C00U, 0xBB515D01U, 0xBAE15E01U, 0x2A705F00U,
	0x36006000U, 0xA6916101U, 0xA7216201U, 0x37B06300U,
	0xA4416401U, 0x34D06500U, 0x35606600U, 0xA5F16701U,
	0xA2816801U, 0x32106900U, 0x33A06A00U, 0xA3316B01U,
	0x30C06C00U, 0xA0516D01U, 0xA1E16E01U, 0x31706F00U,
	0xAF017001U, 0x3F907100U, 0x3E207200U, 0xAEB17301U,
	0x3D407400U, 0xADD17501U, 0xAC617601U, 0x3CF07700U,
	0x3B807800U, 0xAB117901U, 0xAAA17A01U, 0x3A307B00U,
	0xA9C17C01U, 0x39507D00U, 0x38E07E00U, 0xA8717F01U,
	0xD8018001U, 0x48908100U, 0x49208200U, 0xD9B18301U,
	0x4A408400U, 0xDAD18501U, 0xDB618601U, 0x4BF08700U,
	0x4C808800U, 0xDC118901U, 0xDDA18A01U, 0x4D308B00U,
	0xDEC18C01U, 0x4E508D00U, 0x4FE08E00U, 0xDF718F01U,
	0x41009000U, 0xD1919101U, 0xD0219201U, 0x40B09300U,
	0xD3419401U, 0x43D09500U, 0x42609600U, 0xD2F19701U,
	0xD5819801U, 0x45109900U, 0x44A09A00U, 0xD4319B01U,
	0x47C09C00U, 0xD7519D01U, 0xD6E19E01U, 0x46709F00U,
	0x5A00A000U, 0xCA91A101U, 0xCB21A201U, 0x5BB0A300U,
	0xC841A401U, 0x58D0A500U, 0x5960A600U, 0xC9F1A701U,
	0xCE81A801U, 0x5E10A900U, 0x5FA0AA00U, 0xCF31AB01U,
	0x5CC0AC00U, 0xCC51AD01U, 0xCDE1AE01U, 0x5D70AF00U,
	0xC301B001U, 0x5390B100U, 0x5220B200U, 0xC2B1B301U,
	0x5140B400U, 0xC1D1B501U, 0xC061B601U, 0x50F0B700U,
	0x5780B800U, 0xC711B901U, 0xC6A1BA01U, 0x5630BB00U,
	0xC5C1BC01U, 0x5550BD00U, 0x54E0BE00U, 0xC471BF01U,
	0x6C00C000U, 0xFC91C101U, 0xFD21C201U, 0x6DB0C300U,
	0xFE41C401U, 0x6ED0C500U, 0x6F60C600U, 0xFFF1C701U,
	0xF881C801U, 0x6810C900U, 0x69A0CA00U, 0xF931CB01U,
	0x6AC0CC00U, 0xFA51CD01U, 0xFBE1CE01U, 0x6B70CF00U,
	0xF501D001U, 0x6590D100U, 0x6420D200U, 0xF4B1D301U,
	0x6740D400U, 0xF7D1D501U, 0xF661D601U, 0x66F0D700U,
	0x6180D800U, 0xF111D901U, 0xF0A1DA01U, 0x6030DB00U,
	0xF3C1DC01U, 0x6350DD00U, 0x62E0DE00U, 0xF271DF01U,
	0xEE01E001U, 0x7E90E100U, 0x7F20E200U, 0xEFB1E301U,
	0x7C40E400U, 0xECD1E501U, 0xED61E601U, 0x7DF0E700U,
	0x7A80E800U, 0xEA11E901U, 0xEBA1EA01U, 0x7B30EB00U,
	0xE8C1EC01U, 0x7850ED00U, 0x79E0EE00U, 0xE971EF01U,
	0x7700F000U, 0xE791F101U, 0xE621F201U, 0x76B0F300U,
	0xE541F401U, 0x75D0F500U, 0x7460F600U, 0xE4F1F701U,
	0xE381F801U, 0x7310F900U, 0x72A0FA00U, 0xE231FB01U,
	0x71C0FC00U, 0xE151FD01U, 0xE0E1FE01U, 0x7070FF00U
};

static uint32_t tcc_snor_calc_crc8(const u8 *base, uint32_t length)
{
	uint32_t crcout = 0;
	uint32_t cnt;
	u8 code;
	uint32_t tmp;

	for (cnt = 0; cnt < length; cnt++) {
		code = base[cnt];
		tmp = code^crcout;
		crcout = (crcout >> (uint32_t)8) ^ crc32_table[tmp & (u8)0xFF];
	}

	return crcout;
}

static void snor_updater_print_msg(const char *prefix, const u8 *cmd)
{
	char buf[MAX_MSG_CMD_SIZE * 3U];
	const char *shorten;
	size_t cmdlen;
	const size_t bufsz = sizeof(buf);

	for (cmdlen = MAX_MSG_CMD_SIZE; cmdlen > 0U; cmdlen--) {
		if (cmd[cmdlen - 1U] != 0U) {
			break;
		}
	}

	shorten = (cmdlen < (MAX_MSG_CMD_SIZE - 3U)) ? " 00 .. 00" : "";

	(void)hex_dump_to_buffer(cmd, cmdlen, 32, 1, buf, bufsz, (bool)false);
	pr_debug("%s: %s%s\n", prefix, buf, shorten);
}

static int snor_updater_rx_callback(struct tcc_ipi_msg *m,
				  struct platform_device *pdev)
{
	struct snor_updater_device *updater_dev =
		platform_get_drvdata(pdev);
	struct tcc_ipi_msg *rx;

	rx = &updater_dev->rx_data;
	(void)memcpy(rx->cmd, m->cmd, MAX_MSG_CMD_SIZE);
	rx->cmd_len = MAX_MSG_CMD_SIZE;

	complete(&updater_dev->cmpl);

	return 0;
}

static int snor_updater_tx(struct snor_updater_device *updater_dev,
			   struct tcc_ipi_msg *mdata,
			   uint32_t expected_resp_cmd,
			   uint32_t timeout)
{
	int ret = -ENODEV;
	int id;
	struct device_node *cl;
	struct tcc_ipi_msg *rx_msg;
	struct device *dev;

	if(updater_dev == NULL) {
		(void)pr_err("%s: snor_updater_deivce is null\n", __func__);
		return -EINVAL;
	}

	dev = &updater_dev->pdev->dev;
	cl = updater_dev->mbox_client;
	id = updater_dev->index;
	rx_msg = &updater_dev->rx_data;

	if((cl != NULL) && (mdata != NULL)) {
		snor_updater_print_msg("TX", (u8 *)mdata->cmd);
		ret = tcc_ipi_send_data(cl, mdata, id);
		if (ret >= 0) {
			/* Wait response */
			ret = wait_for_completion_interruptible_timeout(&updater_dev->cmpl, timeout);
			if(ret > 0) {
				snor_updater_print_msg("RX", (u8 *)updater_dev->rx_data.cmd);
				if(rx_msg->cmd[0] == expected_resp_cmd) {
					if(rx_msg->cmd[1] != SNOR_UPDATE_ACK) {
						(void)dev_err(dev, "Error! Recive NAK (err=0x%x)\n",
							      rx_msg->cmd[1]);
						ret = -EIO;
					} else {
						ret = 0;
					}
				} else {
					(void)dev_err(dev, "Error! Recieve unexpected response (expt: 0x%x / rcv: 0x%x))\n",
						      expected_resp_cmd,
						      rx_msg->cmd[0]);
				}
			} else {
				(void)dev_err(dev, "Error. Response timeout occurs\n");
				ret = -ETIME;
			}
		}
	}

	return ret;
}

static int snor_updater_update_fw(struct snor_updater_device *updater_dev,
		tcc_snor_update_param *params)
{
	int ret;
	long cpy_cnt;
	unsigned long data_size, remain_size, written_size;
	struct tcc_ipi_msg *tx_msg;
	const u8 *user_buffer = NULL;
	uint32_t crc;
	uint32_t total_count, cur_count;
	struct device *dev;

	if((updater_dev == NULL) || (params == NULL)) {
		return -EINVAL;
	}

	user_buffer = (const u8 *)params->image;

	tx_msg = &updater_dev->tx_data;
	dev = &updater_dev->pdev->dev;

	memset(tx_msg->cmd, 0x0, (tx_msg->cmd_len << 2));
	tx_msg->cmd[0] = UPDATE_FW_START;
	tx_msg->cmd[1] = params->start_address;
	tx_msg->cmd[2] = params->partition_size;
	tx_msg->cmd[3] = params->image_size;
	tx_msg->data_len = 0x0;

	ret = snor_updater_tx(updater_dev, tx_msg, UPDATE_FW_READY,
			      msecs_to_jiffies(SNOR_UPDATER_ERASE_TIMEOUT_MS));
	if(ret != 0) {
		(void)dev_err(dev, "Failed to send UPDATE_FW_START err=%d\n", ret);
	} else {
		remain_size = params->image_size;
		total_count = (remain_size + MAX_MSG_DATA_SIZE - 1) /
			MAX_MSG_DATA_SIZE;
		written_size = 0;
		cur_count = 0;

		while(remain_size != 0) {
			dev_dbg(dev, "total_size=0x%x remain_size=0x%lx writeen_size=0x%lx\n",
				params->image_size, remain_size, written_size);
			if(remain_size > MAX_MSG_DATA_SIZE) {
				data_size = MAX_MSG_DATA_SIZE;
			} else {
				data_size = remain_size;
			}

			/* initialize buffer for remainder */
			memset(tx_msg->data_buf, 0xFF, tx_msg->data_len);
			/* copy actual data to buffer */
			cpy_cnt = copy_from_user((void *)tx_msg->data_buf,
					     (const void *)user_buffer,
					     data_size);
			if(cpy_cnt != 0) {
				(void)dev_err(dev, "Failed to copy data from user buffer to kernel buffer (cpy_cnt=0x%lx)\n",
					cpy_cnt);
				ret = -ENOMEM;
				break;
			}

			crc = tcc_snor_calc_crc8((const u8 *)tx_msg->data_buf, MAX_MSG_DATA_SIZE);

			/* send the data as MAX_MSG_DATA_SIZE always */
			memset(tx_msg->cmd, 0x0, (tx_msg->cmd_len << 2));
			tx_msg->cmd[0] = UPDATE_FW_SEND;
			tx_msg->cmd[1] = params->start_address + written_size;
			tx_msg->cmd[2] = cur_count + 1;
			tx_msg->cmd[3] = total_count;
			tx_msg->cmd[4] = MAX_MSG_DATA_SIZE;
			tx_msg->cmd[5] = crc;
			tx_msg->data_len = (MAX_MSG_DATA_SIZE >> 2);
			dev_dbg(dev, "addr=0x%x chunk_index=0x%x(total=0x%x) size=0x%x crc=0x%08x\n",
				tx_msg->cmd[1],
				tx_msg->cmd[2],
				tx_msg->cmd[3],
				tx_msg->cmd[4],
				tx_msg->cmd[5]);
			ret = snor_updater_tx(updater_dev, tx_msg, UPDATE_FW_SEND_ACK,
					      msecs_to_jiffies(SNOR_UPDATER_ACK_TIMEOUT_MS));
			if(ret != 0) {
				(void)dev_err(dev, "Failed to writing image data err=%d\n", ret);
				break;
			}

			remain_size = remain_size - data_size;
			written_size = written_size + data_size;
			user_buffer = user_buffer + data_size;
			cur_count = cur_count + 1;
		}
	}

	if(ret == 0) {
		memset(tx_msg->cmd, 0x0, (tx_msg->cmd_len << 2));
		tx_msg->cmd[0] = UPDATE_FW_DONE;
		tx_msg->data_len = 0x0;

		ret = snor_updater_tx(updater_dev, tx_msg, UPDATE_FW_COMPLETE,
				      msecs_to_jiffies(SNOR_UPDATER_ACK_TIMEOUT_MS));
		if(ret != 0) {
			(void)dev_err(dev, "Failed to send UPDATE_FW_DONE err=%d\n", ret);
		}
	}

	return ret;
}

static long snor_updater_ioctl(struct file *filp,
			uint cmd, ulong arg)
{
	long ret = -EINVAL;
	struct snor_updater_device *updater_dev = NULL;
	struct tcc_ipi_msg *tx_msg;
	tcc_snor_update_param params;
	struct device *dev;

	if (filp != NULL) {
		updater_dev = (struct snor_updater_device *)filp->private_data;
	}

	if(updater_dev != NULL) {
		tx_msg = &updater_dev->tx_data;
		dev = &updater_dev->pdev->dev;

		switch (cmd) {
		case IOCTL_UPDATE_START:
			{
				memset(tx_msg->cmd, 0x0, (tx_msg->cmd_len << 2));
				tx_msg->cmd[0] = UPDATE_START;
				tx_msg->data_len = 0x0;
				ret = snor_updater_tx(updater_dev, tx_msg, UPDATE_READY,
						      msecs_to_jiffies(SNOR_UPDATER_ACK_TIMEOUT_MS));
				if(ret != 0) {
					(void)dev_err(dev, "Failed to send UPDATE_START err=%ld\n", ret);
				}
			}
			break;
		case IOCTL_UPDATE_DONE:
			{
				memset(tx_msg->cmd, 0x0, (tx_msg->cmd_len << 2));
				tx_msg->cmd[0] = UPDATE_DONE;
				tx_msg->data_len = 0x0;
				ret = snor_updater_tx(updater_dev, tx_msg, UPDATE_COMPLETE,
					msecs_to_jiffies(SNOR_UPDATER_ACK_TIMEOUT_MS));
				if(ret != 0) {
					(void)dev_err(dev, "Failed to send UPDATE_DONE err=%ld\n", ret);
				}
			}
			break;
		case IOCTL_FW_UPDATE:
			{
				ret = copy_from_user((void *)&params,
						     (const void *)arg,
						     sizeof(tcc_snor_update_param));
				if(ret == 0L) {
					ret = snor_updater_update_fw(updater_dev,
						&params);
				} else {
					(void)dev_err(dev, "Failed to copy tcc_snor_updater_param\n");
					ret = -EINVAL;
				}
			}
			break;
		default:
			break;
		}
	}
	return ret;
}

static int snor_updater_open(struct inode *in, struct file *filp)
{
	int ret = -ENODEV;
	struct snor_updater_device *updater_dev = NULL;

	filp->private_data = NULL;

	if (in->i_cdev != NULL) {
		filp->private_data =
			container_of(in->i_cdev, struct snor_updater_device, chrdev);
	}

	if(filp->private_data != NULL) {
		ret = 0;
		updater_dev = (struct snor_updater_device *)filp->private_data;
		mutex_lock(&updater_dev->dev_mutex);
		if (updater_dev->is_opened == 0) {
			updater_dev->is_opened = 1;
		} else {
			ret = -EBUSY;
		}
		mutex_unlock(&updater_dev->dev_mutex);
	}

	return ret;
}

static int snor_updater_release(struct inode *in, struct file *filp)
{
	struct snor_updater_device *updater_dev = NULL;

	filp->private_data = NULL;

	if (in->i_cdev != NULL) {
		filp->private_data =
			container_of(in->i_cdev, struct snor_updater_device, chrdev);
	}

	if(filp->private_data != NULL) {
		updater_dev = (struct snor_updater_device *)filp->private_data;
		mutex_lock(&updater_dev->dev_mutex);
		if (updater_dev->is_opened != 0) {
			updater_dev->is_opened = 0;
		}
		mutex_unlock(&updater_dev->dev_mutex);
	}

	return 0;
}


static const struct file_operations snor_updater_ctrl_fops = {
	.owner          = THIS_MODULE,
	.open           = snor_updater_open,
	.release        = snor_updater_release,
	.unlocked_ioctl = snor_updater_ioctl,
};

static int snor_upater_mbox_init(struct snor_updater_device *updater_dev)
{
	struct device_node *cl;
	struct platform_device *pdev = updater_dev->pdev;
	struct device *dev = &updater_dev->pdev->dev;
	struct tcc_ipi_register_dat mbox_prot_dat = {0,};
	int ret = -ENODEV;

	memcpy(mbox_prot_dat.id, "FWUG", 4);
	mbox_prot_dat.rx_callback = snor_updater_rx_callback;
	mbox_prot_dat.pdev = pdev;

	cl = of_parse_phandle(pdev->dev.of_node, "tcc_ipi", 0);
	if (cl != NULL) {
		ret = tcc_ipi_register(cl, &mbox_prot_dat);
		if (ret < 0) {
			(void)dev_err(dev, "Failed to register ipi protocol\n");
			cl = NULL;
			ret = -EPROBE_DEFER;
		}
	} else {
		(void)dev_err(dev, "Failed to get tcc_ipi phandle\n");
	}

	updater_dev->mbox_client = cl;

	return ret;
}

static int snor_updater_cdev_init(struct snor_updater_device *updater_dev)
{
	dev_t devt;
	int ret;
	struct cdev *chrdev = &updater_dev->chrdev;

	ret = alloc_chrdev_region(&devt, SNOR_UPDATER_DEV_MINOR, 1,
				  SNOR_UPDATER_DEV_NAME);
	if (ret == 0) {
		cdev_init(chrdev, &snor_updater_ctrl_fops);
		chrdev->owner = THIS_MODULE;

		ret = cdev_add(chrdev, devt, 1);
		if (ret != 0) {
			unregister_chrdev_region(devt, 1);
		}
	}

	return ret;
}

static void snor_updater_cdev_free(struct snor_updater_device *updater_dev)
{
	struct cdev *chrdev = &updater_dev->chrdev;

	cdev_del(chrdev);
	unregister_chrdev_region(chrdev->dev, 1);
}

static int snor_updater_dev_init(struct snor_updater_device *updater_dev)
{
	struct class *cls;
	struct device *dev;
	int ret;
	const dev_t devt = updater_dev->chrdev.dev;

	cls = class_create(THIS_MODULE, SNOR_UPDATER_DEV_NAME);
	ret = PTR_ERR_OR_ZERO(cls);
	if (ret == 0) {
		dev = device_create(cls, NULL, devt,
				    NULL, SNOR_UPDATER_DEV_NAME);
		ret = PTR_ERR_OR_ZERO(dev);
		if (ret < 0) {
			class_destroy(cls);
		} else {
			updater_dev->dev = dev;
		}
	}

	return ret;
}

static void snor_updater_dev_free(struct snor_updater_device *updater_dev)
{
	const struct device *dev = updater_dev->dev;
	const dev_t devt = updater_dev->chrdev.dev;
	struct class *cls = dev->class;

	device_destroy(cls, devt);
	class_destroy(cls);
}

static int init_snor_updater_device(struct snor_updater_device *updater_dev)
{
	int ret = -ENODEV;
	struct device *dev = &updater_dev->pdev->dev;

	ret = snor_upater_mbox_init(updater_dev);
	if(ret < 0) {
		(void)dev_err(dev, "Failed to init mbox");
	} else {
		updater_dev->index = ret;

		ret = snor_updater_cdev_init(updater_dev);
		if(ret != 0) {
			(void)dev_err(dev, "Failed to init cdev");
		} else {
			ret = snor_updater_dev_init(updater_dev);
			if(ret != 0) {
				(void)dev_err(dev, "Failed to init device");
				snor_updater_cdev_free(updater_dev);
			}
		}
	}

	return ret;
}

static int snor_updater_ipi_msg_init(struct snor_updater_device *updater_dev)
{
	int ret = 0;
	struct device *dev = &updater_dev->pdev->dev;

	updater_dev->rx_data.cmd = devm_kzalloc(dev, MAX_MSG_CMD_SIZE,
						GFP_KERNEL);
	updater_dev->rx_data.cmd_len = (MAX_MSG_CMD_SIZE >> 2);
	updater_dev->tx_data.cmd = devm_kzalloc(dev, MAX_MSG_CMD_SIZE,
						GFP_KERNEL);
	updater_dev->tx_data.cmd_len = (MAX_MSG_CMD_SIZE >> 2);
	updater_dev->tx_data.data_buf = devm_kzalloc(dev, MAX_MSG_DATA_SIZE,
						 GFP_KERNEL);
	updater_dev->tx_data.data_len = (MAX_MSG_DATA_SIZE >> 2);

	if((updater_dev->rx_data.cmd == NULL) ||
	   (updater_dev->tx_data.cmd == NULL) ||
	   (updater_dev->tx_data.data_buf == NULL)) {
		ret = -ENOMEM;
	}

	return ret;
}

static void snor_updater_ipi_msg_free(struct snor_updater_device *updater_dev)
{
	struct device *dev = &updater_dev->pdev->dev;

	if(updater_dev->rx_data.cmd != NULL) {
		devm_kfree(dev, updater_dev->rx_data.cmd);
	}

	if(updater_dev->tx_data.cmd != NULL) {
		devm_kfree(dev, updater_dev->tx_data.cmd);
	}

	if(updater_dev->tx_data.data_buf != NULL) {
		devm_kfree(dev, updater_dev->tx_data.data_buf);
	}
}

static int snor_updater_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct snor_updater_device *updater_dev = NULL;
	struct device *dev = &pdev->dev;

	updater_dev = devm_kzalloc(dev, sizeof(struct snor_updater_device),
				   GFP_KERNEL);
	if (updater_dev == NULL) {
		ret = -ENOMEM;
		(void)dev_err(dev, "Failed to allocate driver data\n");
	} else {
		platform_set_drvdata(pdev, updater_dev);
		updater_dev->pdev = pdev;
		updater_dev->is_opened = 0;
		mutex_init(&updater_dev->dev_mutex);
		init_completion(&updater_dev->cmpl);

		ret = snor_updater_ipi_msg_init(updater_dev);
		if(ret == 0) {
			ret = init_snor_updater_device(updater_dev);
			if(ret < 0) {
				(void)dev_err(dev, "Failed to register ipi protocol\n");
			}
		} else {
			(void)dev_err(dev, "Failed to allocate message buffer\n");
		}
	}

	return ret;
}

static int snor_updater_remove(struct platform_device *pdev)
{
	struct snor_updater_device *updater_dev = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	snor_updater_dev_free(updater_dev);
	snor_updater_cdev_free(updater_dev);
	snor_updater_ipi_msg_free(updater_dev);
	devm_kfree(dev, updater_dev);

	return 0;
}


#if defined(CONFIG_PM)
static int snor_updater_suspend(struct platform_device *pdev,
			pm_message_t state)
{
	(void)state;

	return 0;
}

static int snor_updater_resume(struct platform_device *pdev)
{
	return 0;
}

#endif

#ifdef CONFIG_OF
static const struct of_device_id snor_updater_ctrl_of_match[] = {
	{.compatible = "telechips,snor-updater", },
	{ },
};

MODULE_DEVICE_TABLE(of, snor_updater_ctrl_of_match);
#endif

static struct platform_driver snor_updater_ctrl = {
	.probe	= snor_updater_probe,
	.remove	= snor_updater_remove,
#if defined(CONFIG_PM)
	.suspend = snor_updater_suspend,
	.resume = snor_updater_resume,
#endif
	.driver	= {
		.name	= SNOR_UPDATER_DEV_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = snor_updater_ctrl_of_match,
#endif
	},
};

static int __init snor_updater_init(void)
{
	return platform_driver_register(&snor_updater_ctrl);
}

static void __exit snor_updater_exit(void)
{
	platform_driver_unregister(&snor_updater_ctrl);
}

module_init(snor_updater_init);
module_exit(snor_updater_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ted Jeong <ted.jeong@telechips.com>");
MODULE_DESCRIPTION("Telechips SNOR update driver");

