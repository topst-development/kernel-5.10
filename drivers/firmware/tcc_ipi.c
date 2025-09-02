// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/list.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/sched/signal.h>
#include <linux/spinlock.h>
#include <linux/of_device.h>

#include <linux/mailbox_client.h>
#include <linux/mailbox_controller.h>

#include <linux/mailbox/mailbox-tcc.h>
#include <linux/firmware/tcc_ipi.h>

#include <linux/delay.h>

#if defined(CONFIG_ARCH_TCC807X)
#define TCC_SC_CID_MAIN		(0x01U)
#define TCC_SC_CID_SUB		(0x02U)
#else
#define TCC_SC_CID_MAIN		(0x72U)
#define TCC_SC_CID_SUB		(0x53U)
#endif

#define TCC803X_IPC_PROT		(1U)
#define TCC803X_MAIN_REBOOT		(2U) //reserve
#define TCC803X_SUB_REBOOT		(3U) //reserve
#define TCC803X_SNOR_UPDATE_PROT	(4U)
#define TCC803X_HSM_PROT		(6U)

#define CLIENT_MSG_INDEX(x)	((x) << (8u))
#define CHECK_MSG_INDEX(x)	((x) >> (8u))

struct tcc_ipi_prot_header {
	u8 bsid;
	u8 cid;
	char idName[2];
};

struct tcc_ipi_prot_tail {
	char idName[4];
};

struct tcc_ipi {
	struct device *dev;
	struct platform_device *pdev;
	struct mbox_chan *chan;
	struct mbox_client cl;
	u32 rx_timeout_ms;

	bool legacy_prot;

	spinlock_t rx_lock;
	spinlock_t tx_lock;
	spinlock_t lock;

	struct list_head xfers_list;
};

struct tcc_ipi_xfer {
	int index;

	struct tcc_ipi_prot_header 	header;
	struct tcc_ipi_prot_tail	tail;

	struct tcc_ipi_msg tx_msg;
	u32 tx_cmd_buf_len;
	u32 tx_data_buf_len;

	struct tcc_ipi_msg rx_msg;
	u32 rx_cmd_buf_len;
	u32 rx_data_buf_len;

	struct list_head node;
	spinlock_t lock;

	unsigned int legacy_prot_index;

	struct platform_device *pdev;

	int (*rx_callback)(struct tcc_ipi_msg *msg, struct platform_device* pdev);

	struct completion tx_complete;
};

static struct tcc_ipi_xfer * tcc_mbox_search_prot(struct tcc_ipi * tcc_ipi, int index)
{
	struct tcc_ipi_xfer * xfer = NULL;
	struct tcc_ipi_xfer * list = NULL;
	unsigned long flags;

	spin_lock_irqsave(&tcc_ipi->lock, flags);
	if (list_empty(&tcc_ipi->xfers_list)) {
		dev_err(tcc_ipi->dev,"[ERROR][%s][%d] xfers_list is empty\n", __func__, __LINE__);
	} else {
		list_for_each_entry(list, &tcc_ipi->xfers_list, node) {
			if(list->index == index) {
				xfer = list;
				break;
			}
		}

		if (xfer == NULL) {
			dev_err(tcc_ipi->dev,"[ERROR][%s][%d] no register in tcc_ipi driver\n", __func__, __LINE__);
		}
	}
	spin_unlock_irqrestore(&tcc_ipi->lock, flags);

	return xfer;
}

/* Extern API*/
int tcc_ipi_send_data(struct device_node *np, struct tcc_ipi_msg *data,int index)
{
	const struct platform_device *pdev = NULL;
	struct tcc_ipi *tcc_ipi = NULL;
	struct device *dev = NULL;
	struct mbox_chan *chan = NULL;

	struct tcc_mbox_device *mdev = NULL;
	struct tcc_ipi_xfer *match = NULL;

	int i;
	unsigned long wait;
	bool opposite_ready = false;
	s32 ret = -1;

	if (data == NULL || np == NULL) {
		pr_err("[ERROR][%s]dev or device_node is null\n", __func__);
		return -1;
	}

	pdev = of_find_device_by_node(np);
	if (pdev == NULL) {
		pr_err("[ERROR][%s] device_node is null\n", __func__);
		return -1;
	}

	tcc_ipi = platform_get_drvdata(pdev);
	if (tcc_ipi == NULL) {
		pr_err("[ERROR][%s] platform data is null\n", __func__);
		return -1;
	}

	dev = tcc_ipi->dev;
	chan = tcc_ipi->chan;
	if ((dev == NULL) || (chan == NULL)) {
		pr_err("[ERROR][%s] channel or device is null\n", __func__);
		return -1;
	}

	mdev = (struct tcc_mbox_device *)mbox_chan_to_tcc_mbox(chan);
	if (mdev == NULL) {
		dev_err(dev,"[ERROR][%s][%d] mbox is not channeling\n", __func__, __LINE__);
		return -1;
	}

	match = tcc_mbox_search_prot(tcc_ipi, index);
	if(match == NULL) {
		dev_err(dev,"[ERROR][%s][%d] match is empty\n", __func__, __LINE__);

		return -1;
	}

	if (mdev->soc_ops->tcc_mbox_can_trans == NULL) {
		dev_err(dev,"[ERROR][%s][%d] tcc_mbox_can_trans is NULL\n", __func__, __LINE__);
		return -1;
	}
	opposite_ready = mdev->soc_ops->tcc_mbox_can_trans(mdev);
	if (opposite_ready == false) {
		return -EAGAIN;
	}

	if (match != NULL) {
		if (data->cmd_len != 6) {
			dev_err(dev,"[ERROR][%s][%d] cmd_len must be 6\n", __func__, __LINE__);
			return -1;
		}

		match->tx_msg.cmd_len = data->cmd_len + 2;

		if (tcc_ipi->legacy_prot == false) {
			match->tx_msg.cmd[0] = match->header.bsid;
			match->tx_msg.cmd[0] |= match->header.cid << 8;
			match->tx_msg.cmd[0] |= match->header.idName[0] << 16;
			match->tx_msg.cmd[0] |= match->header.idName[1] << 24;

			match->tx_msg.cmd[7] = match->tail.idName[0];
			match->tx_msg.cmd[7] |= match->tail.idName[1] << 8;
			match->tx_msg.cmd[7] |= match->tail.idName[2] << 16;
			match->tx_msg.cmd[7] |= match->tail.idName[3] << 24;
		} else {
			match->tx_msg.cmd[0] = match->legacy_prot_index;
			match->tx_msg.cmd[7] = 0;
		}

		for (i = 0 ; i < data->cmd_len ; ++i) {
			match->tx_msg.cmd[i + 1] = data->cmd[i];
		}

		match->tx_msg.data_len = data->data_len;
		if (data->data_len != 0) {

			for (i = 0 ; i < match->tx_msg.data_len ; ++i) {
				match->tx_msg.data_buf[i] = data->data_buf[i];
			}
		}

		match->tx_msg.flags = CLIENT_MSG_INDEX(match->index);

		/* send data */
		ret = mbox_send_message(chan, &match->tx_msg);
		if (ret < 0) {
			dev_err(dev,"[ERROR] failed mbox_send_message (%d)\n", ret);
		} else {
			if (tcc_ipi->cl.tx_done != NULL) {
				wait = msecs_to_jiffies(50);

				ret = wait_for_completion_timeout(&match->tx_complete, wait);
				if (ret == 0) {
					dev_err(dev,"[ERROR][%s] tx_done timeout\n", __func__);
					ret = -EBUSY;
				}
			}
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(tcc_ipi_send_data);

int tcc_ipi_register(struct device_node *np, struct tcc_ipi_register_dat *rgst_dat)
{
	const struct platform_device *pdev = NULL;
	struct tcc_ipi *tcc_ipi = NULL;
	struct device *dev = NULL;

	struct tcc_ipi_xfer *xfer = NULL;
	struct tcc_ipi_xfer *tail = NULL;
	int ret = 0;
	unsigned long flags;

	if ((np == NULL) || (rgst_dat == NULL)) {
		pr_err("[%s] device_node(np 0x%p) or rgst_dat(0x%p) is null \n",
				__func__, np, rgst_dat);
		return -EINVAL;
	}

	pdev = of_find_device_by_node(np);
	if (pdev == NULL) {
		pr_err("[%s] platform device is null \n", __func__);
		return -EINVAL;
	}

	tcc_ipi = platform_get_drvdata(pdev);
	if (tcc_ipi == NULL) {
		pr_err("[%s] tcc_ipi is null \n", __func__);
		return -EINVAL;
	}
	dev = tcc_ipi->dev;
	xfer = devm_kzalloc(dev, sizeof(struct tcc_ipi_xfer), GFP_KERNEL);

	if (xfer != NULL) {

		spin_lock_irqsave(&tcc_ipi->lock, flags);
		if (list_empty(&tcc_ipi->xfers_list)) {
			xfer->index = 1;
		} else {
			list_for_each_entry(tail, &tcc_ipi->xfers_list,node) {
				if (list_is_last(&tail->node, &tcc_ipi->xfers_list))
					xfer->index = tail->index + 1;
			}
		}

		list_add_tail(&xfer->node, &tcc_ipi->xfers_list);
		spin_unlock_irqrestore(&tcc_ipi->lock, flags);

		xfer->tail.idName[0] = rgst_dat->id[0];
		xfer->tail.idName[1] = rgst_dat->id[1];
		xfer->tail.idName[2] = rgst_dat->id[2];
		xfer->tail.idName[3] = rgst_dat->id[3];

		xfer->header.idName[0] = rgst_dat->id[4];
		xfer->header.idName[1] = rgst_dat->id[5];

		if (tcc_ipi->legacy_prot == true) {
			if ( 0 == strncmp(rgst_dat->id,"HSM",3)) {
				xfer->legacy_prot_index = TCC803X_HSM_PROT;
			} else if( 0 == strncmp(rgst_dat->id,"FWUG",4)) {
				xfer->legacy_prot_index = TCC803X_SNOR_UPDATE_PROT;
			} else if( 0 == strncmp(rgst_dat->id, "IPC",3)) {
				xfer->legacy_prot_index = TCC803X_IPC_PROT;
			} else {
				xfer->legacy_prot_index = 0xffffffff;
			}
		} else {
			xfer->legacy_prot_index = 0;
		}

		if (rgst_dat->pdev == NULL) {
			dev_err(dev, "[ERROR] %s: platform_device is NULL\n", __func__);
			return -1;
		}
		xfer->pdev = rgst_dat->pdev;

		if (rgst_dat->rx_callback == NULL ) {
			dev_err(dev, "[ERROR] %s: callback func is null index = %d\n", __func__,xfer->index);
			return -1;
		} else {
			xfer->rx_callback = rgst_dat->rx_callback;
		}

#if (defined(CONFIG_TCC805X_CA53Q) || defined(CONFIG_TCC807X_CA55_SUB))
		xfer->header.cid = TCC_SC_CID_SUB;
#else
		xfer->header.cid = TCC_SC_CID_MAIN;
#endif
		xfer->header.bsid = 0x46;

		/* Config TX */
		xfer->tx_cmd_buf_len = 8;
		xfer->tx_data_buf_len = 128;
		xfer->tx_msg.cmd =  devm_kzalloc(dev, (sizeof(u32) * xfer->tx_cmd_buf_len), GFP_KERNEL);
		if (xfer->tx_msg.cmd == NULL) {
			(void)dev_err(dev,
			"[ERROR][%s] Failed to allocate memory for Tx cmd buffer\n", __func__);
			ret = -ENOMEM;
			devm_kfree(dev,xfer);
			return ret;
		}

		xfer->tx_msg.data_buf = devm_kzalloc(dev, (sizeof(u32) * xfer->tx_data_buf_len) ,GFP_KERNEL);
		if (xfer->tx_msg.data_buf == NULL) {
			(void)dev_err(dev,
			"[ERROR][%s] Failed to allocate memory for Tx data buffer\n", __func__);
			ret = -ENOMEM;
			devm_kfree(dev,xfer->tx_msg.cmd);
			devm_kfree(dev,xfer);
			return ret;
		}

		/* Config RX */
		xfer->rx_cmd_buf_len = 8;
		xfer->rx_data_buf_len = 128;
		xfer->rx_msg.cmd = devm_kzalloc(dev, sizeof(u32) * xfer->rx_cmd_buf_len, GFP_KERNEL);
		if (xfer->tx_msg.cmd == NULL) {
			(void)dev_err(dev,
			"[ERROR][%s] Failed to allocate memory for Tx cmd buffer\n", __func__);
			ret = -ENOMEM;
			devm_kfree(dev,xfer->tx_msg.data_buf);
			devm_kfree(dev,xfer->tx_msg.cmd);
			devm_kfree(dev,xfer);
			return ret;
		}

		xfer->rx_msg.data_buf = devm_kzalloc(dev, sizeof(u32) * xfer->rx_data_buf_len, GFP_KERNEL);
		if (xfer->tx_msg.data_buf == NULL) {
			(void)dev_err(dev,
			"[ERROR][%s] Failed to allocate memory for Tx data buffer\n", __func__);
			ret = -ENOMEM;
			devm_kfree(dev,xfer->rx_msg.cmd);
			devm_kfree(dev,xfer->tx_msg.data_buf);
			devm_kfree(dev,xfer->tx_msg.cmd);
			devm_kfree(dev,xfer);
			return ret;
		}

		spin_lock_init(&xfer->lock);
		init_completion(&xfer->tx_complete);

		if (rgst_dat->rx_callback == NULL ) {
			dev_err(dev, "[ERROR][%s]callback func is null\n", __func__);
			devm_kfree(dev,xfer->rx_msg.data_buf);
			devm_kfree(dev,xfer->rx_msg.cmd);
			devm_kfree(dev,xfer->tx_msg.data_buf);
			devm_kfree(dev,xfer->tx_msg.cmd);
			devm_kfree(dev,xfer);
			return -1;
		} else {
			xfer->rx_callback = rgst_dat->rx_callback;
		}

		if (ret == 0) {
			ret = xfer->index;
		}

		dev_info(dev, "[%s]register ipi protocol driver\n", __func__);

	} else {
		dev_err(dev, "[ERROR][%s]memory alloc failed\n", __func__);
		ret = -ENOMEM;
		devm_kfree(dev,xfer);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(tcc_ipi_register);

static void tcc_ipi_rx_callback(struct mbox_client *cl, void *mssg)
{
	struct tcc_mbox_msg *msg = (struct tcc_mbox_msg *)mssg;
	struct tcc_ipi *tcc_ipi = NULL;

	struct tcc_ipi_xfer *list = NULL;
	struct tcc_ipi_xfer *match = NULL;
	unsigned long flags;
	int i;
	u32 tail, header, checker;

	if ((msg == NULL) || (cl == NULL)) {
		pr_err("[ERROR][%s][%d] msg or cl is NULL\n", __func__, __LINE__);
		return ;
	}

	tcc_ipi = ((struct tcc_ipi *)container_of((cl), const struct tcc_ipi, cl));
	if (tcc_ipi == NULL) {
		pr_err("[ERROR][%s][%d] tcc_ipi is NULL\n", __func__, __LINE__);
		return ;
	} else {

		spin_lock_irqsave(&tcc_ipi->rx_lock, flags);
		if (list_empty(&tcc_ipi->xfers_list)) {
			spin_unlock_irqrestore(&tcc_ipi->rx_lock, flags);
			dev_warn(tcc_ipi->dev,"[WRAN][%s][%d] There is no driver registered in mailbox.\n", __func__, __LINE__);
			return ;
		}
		spin_unlock_irqrestore(&tcc_ipi->rx_lock, flags);


		if (tcc_ipi->legacy_prot == false) {

			checker = msg->cmd[0] >> 16;

			/* find xfer */
			spin_lock_irqsave(&tcc_ipi->rx_lock, flags);
			list_for_each_entry(list, &tcc_ipi->xfers_list, node) {
				tail = list->tail.idName[0];
				tail |= list->tail.idName[1] << 8;
				tail |= list->tail.idName[2] << 16;
				tail |= list->tail.idName[3] << 24;

				header = list->header.idName[0];
				header |= list->header.idName[1] << 8;

				if (msg->cmd[7] == tail
					&& checker == header) {
					match = list;
					break;
				}
			}
			spin_unlock_irqrestore(&tcc_ipi->rx_lock, flags);
		} else {
			checker = msg->cmd[0];

			/* find xfer */
			spin_lock_irqsave(&tcc_ipi->rx_lock, flags);
			list_for_each_entry(list, &tcc_ipi->xfers_list, node) {
				if (list->legacy_prot_index == checker){
					match = list;
					break;
				}
			}
			spin_unlock_irqrestore(&tcc_ipi->rx_lock, flags);
		}

		if (match == NULL) {
			dev_warn(tcc_ipi->dev, "[WARN][%s][%d] There is no matching driver among the registered drivers\n", __func__, __LINE__);
			return ;
		}

		/*input data*/
		match->rx_msg.cmd_len = 6;
		for (i = 0 ; i < match->rx_msg.cmd_len ; ++i) {
			match->rx_msg.cmd[i] = msg->cmd[i+1];
		}

		if (msg->data_len != 0) {
			match->rx_msg.data_len = msg->data_len;

			for (i = 0 ; i < match->rx_msg.data_len ; ++i) {
				match->rx_msg.data_buf[i] = msg->data_buf[i];
			}
		}

		match->rx_callback(&match->rx_msg, match->pdev);
	}
}

#if defined(CONFIG_ARCH_TCC803X)
static void tcc_mbox_tx_done(struct mbox_client *cl, void *msg, int r)
{
	struct tcc_ipi *tcc_ipi = NULL;
	struct tcc_ipi_xfer *match = NULL;
	struct tcc_mbox_msg *mssg = (struct tcc_mbox_msg *)msg;
	int index;

	tcc_ipi = ((struct tcc_ipi *)container_of((cl), const struct tcc_ipi, cl));
	if (tcc_ipi == NULL) {
		pr_err("[ERROR][%s]%d: tx_ipi is null \n", __func__, __LINE__);
	} else {
		index = CHECK_MSG_INDEX(mssg->flags);
		match = tcc_mbox_search_prot(tcc_ipi, index);
		if (match == NULL) {
			dev_err(tcc_ipi->dev, "[ERROR][%s][%d] no match protocol in tx_done\n", __func__, __LINE__);
		} else {
			complete(&match->tx_complete);
		}
	}
}
#endif

static s32 tcc_ipi_probe(struct platform_device *pdev)
{
	struct device *dev;
	struct tcc_ipi *data = NULL;
	s32 ret = 0;

	if (pdev == NULL) {
		pr_err("[ERROR][%s]%d: platform_device is null\n", __func__, __LINE__);
		return -EPROBE_DEFER;
	}

	dev = &pdev->dev;
	if (dev == NULL) {
		pr_err("[ERROR][%s]%d: device node is null\n", __func__, __LINE__);
		return -EPROBE_DEFER;
	}

	data = devm_kzalloc(dev, sizeof(struct tcc_ipi), GFP_KERNEL);
	if (data == NULL) {
		dev_err(dev,"[ERROR][%s][%d] failed to devm_alloc\n", __func__, __LINE__);
		return -EPROBE_DEFER;
	}

	/*create client*/
	data->dev = dev;

	INIT_LIST_HEAD(&data->xfers_list);

	spin_lock_init(&data->rx_lock);
	spin_lock_init(&data->tx_lock);
	spin_lock_init(&data->lock);

	data->cl.dev = dev;
	data->cl.rx_callback = tcc_ipi_rx_callback;
	data->cl.knows_txdone = false;
#if defined(CONFIG_ARCH_TCC803X)
	data->cl.tx_block = false;
	data->cl.tx_tout = 0; /*timeout value*/
	data->cl.tx_done = tcc_mbox_tx_done;
#else
	data->cl.tx_block = true;
	data->cl.tx_tout = 50; /*timeout value*/
#endif

	data->legacy_prot = of_property_read_bool(pdev->dev.of_node, "legacy_prot");
	if (data->legacy_prot == true) {
		dev_info(dev,"[INFO][%s][%d] tcc803x protocol enable!!!!!\n", __func__, __LINE__);
	} else {
		dev_info(dev,"[INFO][%s][%d] new protocol\n", __func__, __LINE__);
	}

	data->chan = mbox_request_channel(&(data->cl), 0);
	if (IS_ERR(data->chan)) {
		dev_err(dev,"[ERROR][%s][%d] failed to mbox request channel\n", __func__, __LINE__);
		return -EPROBE_DEFER;
	}

	dev_set_drvdata(dev, data);
	platform_set_drvdata(pdev, data);
	data->pdev = pdev;

	ret = of_platform_populate(dev->of_node, NULL, NULL, dev);

	dev_info(dev,"[INFO] ipi driver probing!!!!\n");

	return ret;
}

static const struct of_device_id tcc_ipi_match[2] = {
	{ .compatible = "telechips,mailbox-ipi-protocol" },
	{ .compatible = "" }
};

MODULE_DEVICE_TABLE(of, tcc_ipi_match);

static struct platform_driver tcc_ipi_driver = {
	.probe = tcc_ipi_probe,
	.driver = {
		.name = "tcc-mailbox-ipi",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tcc_ipi_match),
	},
};

static int32_t __init tcc_ipi_init(void)
{
	return platform_driver_register(&tcc_ipi_driver);
}
subsys_initcall(tcc_ipi_init);

static void __exit tcc_ipi_exit(void)
{
	platform_driver_unregister(&tcc_ipi_driver);
}
module_exit(tcc_ipi_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Peter Choi <peter.choi@telechips.com>");
MODULE_DESCRIPTION("Telechips Inter Processor Interface Driver");
