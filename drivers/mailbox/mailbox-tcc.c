// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/types.h>
#include <linux/wait_bit.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/mailbox/mailbox-tcc.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#define MBOX_CMD_TX_FIFO		(0x0U)

#define MBOX_CMD_RX_FIFO		(0x20U)

#define MBOX_CTRL			(0x40U)
 #define MBOX_CTRL_TEST			(31U)
 #define MBOX_CTRL_ICLR_WRITE		(21U)
 #define MBOX_CTRL_IEN_WRITE		(20U)
 #define MBOX_CTRL_DF_FLUSH		(7U)
 #define MBOX_CTRL_CF_FLUSH		(6U)
 #define MBOX_CTRL_OEN			(5U)
 #define MBOX_CTRL_IEN_READ		(4U)
 #define MBOX_CTRL_ILEVEL		(0U)
  #define MBOX_ILEVEL_NEMP		(0x0U)
  #define MBOX_ILEVEL_GT2		(0x1U)
  #define MBOX_ILEVEL_GT4		(0x2U)
  #define MBOX_ILEVEL_FULL		(0x3U)

#define MBOX_CMD_FIFO_STS		(0x44U)
 #define MBOX_CMD_RX_FIFO_COUNT			(20U)
  #define MBOX_CMD_RX_FIFO_COUNT_MASK		(0xFU)
 #define MBOX_CMD_RX_FIFO_FULL			(17U)
 #define MBOX_CMD_RX_FIFO_EMPTY			(16U)
 #define MBOX_CMD_TX_FIFO_COUNT			(4U)
  #define MBOX_CMD_TX_FIFO_COUNT_MASK		(0xFU)
 #define MBOX_CMD_TX_FIFO_FULL			(1U)
 #define MBOX_CMD_TX_FIFO_EMPTY			(0U)

#define MBOX_DAT_FIFO_TX_STS		(0x50U)
 #define MBOX_DAT_TX_FIFO_EMPTY			(31U)
 #define MBOX_DAT_TX_FIFO_FULL			(30U)
 #define MBOX_DAT_TX_FIFO_COUNT			(0U)
  #define MBOX_DAT_TX_FIFO_COUNT_MASK		(0xFFFFU)

#define MBOX_DAT_FIFO_RX_STS		(0x54U)
 #define MBOX_DAT_RX_FIFO_EMPTY			(31U)
 #define MBOX_DAT_RX_FIFO_FULL			(30U)
 #define MBOX_DAT_RX_FIFO_COUNT			(0U)
  #define MBOX_DAT_RX_FIFO_COUNT_MASK		(0xFFFFU)

#define MBOX_DAT_FIFO_TXD		(0x60U)

#define MBOX_DAT_FIFO_RXD		(0x70U)

#define MBOX_CTRL_SET			(0x74U)

#define MBOX_CTRL_CLR			(0x78U)

#define MBOX_OPPOSITE_STS		(0x7CU)
 #define OPP_TMN_STS			(16U)
 #define OWN_TMN_STS			(0U)

#define MBOX_MAX_CMD_LENGTH		(8U)
#define MBOX_MAX_DATA_LENGTH		(128U)

#define MBOX_DAT_TX_TIMEOUT_MS		(600000U) /* 10 minute timeout */

struct tcc_mbox_device *mbox_chan_to_tcc_mbox(
		const struct mbox_chan *chan)
{
	struct tcc_mbox_device *ret_dev = NULL;

	if ((chan != NULL) && (chan->con_priv != NULL)) {
		/* Get tcc_mbox_device struct from mbox_chan struct */
		ret_dev = (struct tcc_mbox_device *)chan->con_priv;
	}

	return ret_dev;
}
EXPORT_SYMBOL_GPL(mbox_chan_to_tcc_mbox);

static inline void tcc_mbox_writel(
		const struct tcc_mbox_device *mdev, u32 val, u32 reg)
{
	writel(val, mdev->mbox_base + reg);
}

static inline u32 tcc_mbox_readl(
		const struct tcc_mbox_device *mdev, u32 reg)
{
	return readl(mdev->mbox_base + reg);
}

static inline void tcc_mbox_set_ctrl(const struct tcc_mbox_device *mdev,
					u32 mask)
{
	if (mdev->soc_ops->version == 1) {
		u32 tmp;
		tmp = tcc_mbox_readl(mdev, MBOX_CTRL);
		tmp |= mask;
		tcc_mbox_writel(mdev, tmp, MBOX_CTRL);
	} else if (mdev->soc_ops->version == 2) {
		tcc_mbox_writel(mdev, mask, MBOX_CTRL_SET);
	}
}

static inline void tcc_mbox_clr_ctrl(const struct tcc_mbox_device *mdev,
					u32 mask)
{
	if (mdev->soc_ops->version == 1) {
		u32 tmp;
		tmp = tcc_mbox_readl(mdev, MBOX_CTRL);
		tmp &= ~(mask);
		tcc_mbox_writel(mdev, tmp, MBOX_CTRL);
	} else if (mdev->soc_ops->version == 2) {
		tcc_mbox_writel(mdev, ~(mask), MBOX_CTRL_CLR);
	}
}

static bool tcc_mbox_can_trans(const struct tcc_mbox_device *mdev)
{
	u32 opposite_status;

	if (mdev->soc_ops->version == 1 ) {
		/*
		 * In version 1, since there is no opposite_ready
		 * register, it always true.
		 */
		opposite_status = 1;
	} else {
		opposite_status = tcc_mbox_readl(mdev, MBOX_OPPOSITE_STS);
		opposite_status >>= OPP_TMN_STS;
	}
	return opposite_status ? true : false;
}

static bool tcc_mbox_peek_data(struct mbox_chan *chan)
{
	const struct tcc_mbox_device *mdev = mbox_chan_to_tcc_mbox(chan);
	unsigned int count;

	if (mdev == NULL) {
		(void)pr_err("[ERROR][%s] mdev is null\n",
				__func__);
		count = 0;
	} else {
		count = tcc_mbox_readl(mdev, MBOX_CMD_FIFO_STS);
		count >>= MBOX_CMD_RX_FIFO_COUNT;
		count &= MBOX_CMD_RX_FIFO_COUNT_MASK;
	}

	return ((count > 0U) ? true : false);
}

static void tcc_mbox_tasklet_finish(unsigned long param)
{
	const struct tcc_mbox_device *mdev =
		(struct tcc_mbox_device *)param;

	/* Notify completion */
	mbox_chan_txdone(&mdev->mbox.chans[0], -EINVAL);
}

static int tcc_mbox_send_data(struct mbox_chan *chan, void *data)
{
	struct tcc_mbox_device *mdev = mbox_chan_to_tcc_mbox(chan);
	const struct tcc_mbox_msg *msg = (struct tcc_mbox_msg *)data;

	unsigned long flags;
	u32 i;
	int ret = 0;
#define TCC_MBOX_SKIP (1)
	u32 status;

	if ((mdev == NULL) || (msg == NULL)) {
		(void)pr_err(
				"[ERROR][%s] argument is null",
				__func__);
		ret = -EINVAL;
	}

	/*
	 * Clint can halt pending mbox message before send data
	 * by setting this flag.
	 */
	if ((ret == 0) && ((msg->flags & TCC_MBOX_FLAG_SKIP_XFER) != 0U)) {
		dev_err(mdev->dev,
				"[ERROR][%s] Skip mbox message transfer\n", __func__);

		if (mdev->soc_ops->version == 2) {
			tasklet_schedule(&mdev->finish_tasklet);
		}
		ret = TCC_MBOX_SKIP;
	}

	/* check message */
	if ((ret == 0) && (msg->cmd_len > MBOX_MAX_CMD_LENGTH)) {
		dev_err(mdev->dev,
				"[ERROR][%s] Exceed command buffer (%d > %d)\n",
				__func__, msg->cmd_len, MBOX_MAX_CMD_LENGTH);
		ret = -EINVAL;
	}

	if ((ret == 0) && (msg->cmd_len == 0U)) {
		dev_err(mdev->dev,
				"[ERROR][%s] Command length is zero\n", __func__);
		ret = -EINVAL;
	}

	if ((ret == 0) && (msg->cmd == NULL)) {
		dev_err(mdev->dev,
				"[ERROR][%s] Command buffer is null\n", __func__);
		ret = -EINVAL;
	}

	if ((ret == 0) && (msg->data_len > MBOX_MAX_DATA_LENGTH)) {
		dev_err(mdev->dev,
				"[ERROR][%s] Exceed data buffer (%d > %d)\n",
				__func__,msg->data_len, MBOX_MAX_DATA_LENGTH);
		ret = -EINVAL;
	}

	if ((ret == 0) && (msg->data_len != 0U) && (msg->data_buf == NULL)) {
		dev_err(mdev->dev,
				"[ERROR][%s] Data buffer is null\n", __func__);
		ret = -EINVAL;
	}

	if (ret == 0) {
		spin_lock_irqsave((&mdev->lock), (flags));

		status = tcc_mbox_readl(mdev, MBOX_CMD_FIFO_STS);
		if ((status & ((u32)1 << MBOX_CMD_TX_FIFO_EMPTY)) == 0U) {
			dev_err(mdev->dev,
					"[ERROR][%s] Tx command FIFO is not empty\n", __func__);
			ret = -EBUSY;
		}

		if (ret == 0) {
			status = tcc_mbox_readl(mdev, MBOX_DAT_FIFO_TX_STS);
			status &= ((u32)1 << MBOX_DAT_TX_FIFO_EMPTY);
			if (status == 0U) {
				dev_err(mdev->dev,
						"[ERROR][%s] Tx data FIFO is not empty\n", __func__);
				ret = -EBUSY;
			}
		}

		if (ret == 0) {
			/* Write command to fifo */
			for (i = 0; i < msg->cmd_len; i++) {
				tcc_mbox_writel(mdev, msg->cmd[i],
					(MBOX_CMD_TX_FIFO + (i * 0x4U)));
			}

			/* Write data if exist */
			if (msg->data_len > 0U) {
				for (i = 0U; i < msg->data_len; i++) {
					tcc_mbox_writel(
							mdev, msg->data_buf[i],
							MBOX_DAT_FIFO_TXD);
				}
			}

			if (mdev->soc_ops->version == 2 ) {
				/* Clear and enable tx interrupt */
				tcc_mbox_set_ctrl(mdev,
						((u32)1 << MBOX_CTRL_ICLR_WRITE));
				tcc_mbox_set_ctrl(mdev,
						((u32)1 << MBOX_CTRL_IEN_WRITE));
			}
			/* Send message */
			tcc_mbox_set_ctrl(mdev,
					((u32)1 << MBOX_CTRL_OEN));
		}

		spin_unlock_irqrestore(&mdev->lock, flags);
	}

	return (ret < 0) ? ret : 0;
}

static int tcc_mbox_startup(struct mbox_chan *chan)
{
	struct tcc_mbox_device *mdev;
	unsigned long flags;
	int ret = 0;

	mdev = mbox_chan_to_tcc_mbox(chan);
	if (mdev == NULL) {
		(void)pr_err("[ERROR][%s] con_priv is null",
				__func__);
		ret = -EINVAL;
	} else {
		spin_lock_irqsave((&mdev->lock), (flags));

		/* Disable output */
		tcc_mbox_clr_ctrl(mdev, ((u32)1 << MBOX_CTRL_OEN));

		/* Flush command and data FIFO */
		tcc_mbox_set_ctrl(mdev,
				((u32)1 << MBOX_CTRL_CF_FLUSH) |
				((u32)1 << MBOX_CTRL_DF_FLUSH));

		/* Set rx interrupt */
		tcc_mbox_set_ctrl(mdev,
				((u32)1 << MBOX_CTRL_IEN_READ) |
				(MBOX_ILEVEL_NEMP << MBOX_CTRL_ILEVEL));

		/* Set terminal status register */
		if (mdev->soc_ops->version == 2) {
			tcc_mbox_writel(mdev, 1U, MBOX_OPPOSITE_STS);
		}
		spin_unlock_irqrestore(&mdev->lock, flags);
	}

	return ret;
}

static void tcc_mbox_shutdown(struct mbox_chan *chan)
{
	struct tcc_mbox_device *mdev;
	unsigned long flags;

	mdev = mbox_chan_to_tcc_mbox(chan);
	if (mdev == NULL) {
		(void)pr_err("[ERROR][%s] mdev is null",
				__func__);
	} else {
		spin_lock_irqsave((&mdev->lock), (flags));

		/* Disable output */
		tcc_mbox_clr_ctrl(mdev, ((u32)1 << MBOX_CTRL_OEN));

		/* Flush command and data FIFO */
		tcc_mbox_set_ctrl(mdev,
				((u32)1 << MBOX_CTRL_CF_FLUSH) |
				((u32)1 << MBOX_CTRL_DF_FLUSH));

		/* Disable rx interrupt */
		tcc_mbox_clr_ctrl(mdev, ((u32)1 << MBOX_CTRL_IEN_READ));

		/* Clear terminal status register */
		if (mdev->soc_ops->version == 2) {
			tcc_mbox_writel(mdev, 0U, MBOX_OPPOSITE_STS);
		}

		spin_unlock_irqrestore(&mdev->lock, flags);
	}
}

static irqreturn_t tcc_mbox_rx_irq_handler(s32 irq, void *data)
{
	const struct tcc_mbox_device *mdev =
		mbox_chan_to_tcc_mbox((struct mbox_chan *)data);
	u32 status;
	irqreturn_t ret = IRQ_NONE;

	if (mdev == NULL) {
		(void)pr_err("[ERROR][%s] argument is null",
				__func__);
	} else if (irq != mdev->rx_irq) {
		dev_err(mdev->dev,
				"[ERROR][%s] Wrong RX_IRQ # (%d)\n",
				__func__,irq);
	} else {
		status = tcc_mbox_readl(mdev, MBOX_CMD_FIFO_STS);
		if ((status & ((u32)1 << MBOX_CMD_RX_FIFO_EMPTY)) == 0U) {
			/* Disable Rx interrupt */
			tcc_mbox_clr_ctrl(mdev,
					((u32)1 << MBOX_CTRL_IEN_READ));

			ret = IRQ_WAKE_THREAD;
		} else {
			dev_err(mdev->dev,
					"[ERROR][%s] RX command FIFO is empty at irq\n", __func__);
		}
	}

	return ret;
}

static irqreturn_t tcc_mbox_rx_isr_handler(s32 irq, void *data)
{
	struct tcc_mbox_device *mdev =
		mbox_chan_to_tcc_mbox((struct mbox_chan *)data);
	struct tcc_mbox_msg *msg;
	unsigned long flags;
	u32 status_cmd, status_dat;
	u32 count;
	u32 i;
	u32 rx_buf_len;
	irqreturn_t ret = IRQ_NONE;

	if (mdev == NULL) {
		(void)pr_err("[ERROR][%s] Device is null\n",
				__func__);
	} else if (irq != mdev->rx_irq) {
		dev_err(mdev->dev,
				"[ERROR][%s] Wrong RX_IRQ # (%d)\n",
				__func__,irq);
	} else {
		msg = &mdev->rx_msg;

		spin_lock_irqsave((&mdev->lock), (flags));

		status_dat = tcc_mbox_readl(mdev, MBOX_DAT_FIFO_RX_STS);
		status_dat &= ((u32)1 << MBOX_DAT_RX_FIFO_EMPTY);

		status_cmd = tcc_mbox_readl(mdev, MBOX_CMD_FIFO_STS);
		status_cmd &= ((u32)1 << MBOX_CMD_RX_FIFO_EMPTY);

		if ((status_dat == 0U) && (status_cmd == 0U)) {
			/* Read Data */
			count = tcc_mbox_readl(mdev,
					MBOX_DAT_FIFO_RX_STS);
			count >>= MBOX_DAT_RX_FIFO_COUNT;
			count &= MBOX_DAT_RX_FIFO_COUNT_MASK;
			rx_buf_len = mdev->rx_data_buf_len;
			for (i = 0; i < count; i++) {
				/*
				 * if buf len is smaller than fifo cnt,
				 * do dummy read
				 */
				if ((msg->data_buf == NULL) ||
						(rx_buf_len < (i + 0x1U))) {
					(void)tcc_mbox_readl(mdev,
							MBOX_DAT_FIFO_RXD);
				} else {
					msg->data_buf[i] =
						tcc_mbox_readl(mdev,
							MBOX_DAT_FIFO_RXD);
				}
			}

			msg->data_len = count;
		}

		if (status_cmd == 0U) {
			/* Read command */
			count = tcc_mbox_readl(mdev, MBOX_CMD_FIFO_STS);
			count >>= MBOX_CMD_RX_FIFO_COUNT;
			count &= MBOX_CMD_RX_FIFO_COUNT_MASK;
			rx_buf_len = mdev->rx_cmd_buf_len;
			for (i = 0U; i < count; i++) {
				if (rx_buf_len < (i + 0x1U)) {
					(void)tcc_mbox_readl(mdev,
						(MBOX_CMD_RX_FIFO + (i * 4U)));
				} else {
					msg->cmd[i] = tcc_mbox_readl(mdev,
						(MBOX_CMD_RX_FIFO + (i * 4U)));
				}
			}
			msg->cmd_len = count;

			if (status_dat != 0) {
				msg->data_len = 0;
			}
		} else {
			/* Enable Rx interrupt */
			tcc_mbox_set_ctrl(mdev,
					((u32)1 << MBOX_CTRL_IEN_READ));
		}

		spin_unlock_irqrestore(&mdev->lock, flags);

		if (status_cmd == 0U) {
			dev_dbg(mdev->dev,
					"[DEBUG][%s] Receive cmd(%d) and data(%d)\n",
					__func__, msg->cmd_len, msg->data_len);

			mbox_chan_received_data(&mdev->mbox.chans[0], msg);

			spin_lock_irqsave((&mdev->lock), (flags));
			/* Enable Rx interrupt */
			tcc_mbox_set_ctrl(mdev,
					((u32)1 << MBOX_CTRL_IEN_READ));
			spin_unlock_irqrestore(&mdev->lock, flags);

			ret = IRQ_HANDLED;
		} else {
			dev_err(mdev->dev,
					"[ERROR][%s] RX command FIFO is empty at isr, Re-enable Rx Interrupt.\n", __func__);
		}
	}

	return ret;
}

static irqreturn_t tcc_mbox_tx_irq_handler(s32 irq, void *data)
{
	const struct tcc_mbox_device *mdev =
		mbox_chan_to_tcc_mbox((struct mbox_chan *)data);
	irqreturn_t ret = IRQ_NONE;
	u32 status;

	if (mdev == NULL) {
		(void)pr_err("[ERROR][%s] Device is null\n",
				__func__);
	} else if (irq != mdev->tx_irq) {
		dev_err(mdev->dev,
				"[ERROR][%s] Wrong TX_IRQ # (%d)\n",
				__func__, irq);
	} else {
		/* check transmmit cmd fifo */
		status = tcc_mbox_readl(mdev, MBOX_CMD_FIFO_STS);
		status &= ((u32)1 << MBOX_CMD_TX_FIFO_EMPTY);
		if (status != 0U) {
			/* Clear interrupt and disable output */
			tcc_mbox_set_ctrl(mdev,
					((u32)1 << MBOX_CTRL_ICLR_WRITE));
			tcc_mbox_clr_ctrl(mdev,
					((u32)1 << MBOX_CTRL_OEN));

			ret = IRQ_WAKE_THREAD;
		} else {
			dev_err(mdev->dev,
					"[ERROR][%s] TX CMD FIFO is not empty\n", __func__);
		}
	}

	return ret;
}

static irqreturn_t tcc_mbox_tx_isr_handler(s32 irq, void *data)
{
	struct tcc_mbox_device *mdev =
		mbox_chan_to_tcc_mbox((struct mbox_chan *) data);
	unsigned long flags;
	struct mbox_chan *chan;
	irqreturn_t ret = IRQ_NONE;
	unsigned long timeout =
		jiffies + msecs_to_jiffies(MBOX_DAT_TX_TIMEOUT_MS);
	int expired = 0;
	u32 status;

	if (mdev == NULL) {
		(void)pr_err("[ERROR][%s] Device is null\n",
				__func__);
	} else if (irq != mdev->tx_irq) {
		dev_err(mdev->dev, "[ERROR][%s] Wrong TX_IRQ # (%d)\n",
				__func__, irq);
	} else {
		while (expired == 0) {
			spin_lock_irqsave((&mdev->lock), (flags));
			status = tcc_mbox_readl(mdev, MBOX_DAT_FIFO_TX_STS);
			spin_unlock_irqrestore(&mdev->lock, flags);
			status &= ((u32)1 << MBOX_DAT_TX_FIFO_EMPTY);
			if (status != 0U) {
				/* Notify completion */
				chan = &mdev->mbox.chans[0];
				mbox_chan_txdone(chan, 0);

				ret = IRQ_HANDLED;

				break;
			}

			udelay(1UL);

			if (time_after(jiffies, timeout)) {
				/* Timeout trigger */
				expired = 1;
			}
		}

		if (ret != IRQ_HANDLED) {
			dev_err(mdev->dev,
					"[ERROR][%s] TX DATA FIFO is not empty\n", __func__);
		} else {
			dev_dbg(mdev->dev, "[DEBUG][%s] Tx done interrupt occurs\n", __func__);
		}
	}

	return ret;
}

static bool tcc_last_tx_done(struct mbox_chan *chan)
{
	bool ret = false;
	u32 status;
	const struct tcc_mbox_device *mdev;
	if (chan != NULL) {
		mdev = mbox_chan_to_tcc_mbox(chan);
		status = tcc_mbox_readl(mdev, MBOX_CMD_FIFO_STS);
		if ((status & ((u32)1 << MBOX_CMD_TX_FIFO_EMPTY)) == 0U) {
			/* do notning */
		} else {
			/* TX Done */
			/* Output Disable */
			tcc_mbox_clr_ctrl(mdev,
					((u32)1 << MBOX_CTRL_OEN));
			ret = true;
		}

	} else {
		pr_err("[ERROR]%s chan is empty\n", __func__);
	}


	return ret;
}

static const struct mbox_chan_ops tcc_mbox_chan_ops_v1 = {
	.send_data = tcc_mbox_send_data,
	.startup = tcc_mbox_startup,
	.shutdown = tcc_mbox_shutdown,
	.peek_data = tcc_mbox_peek_data,
	.last_tx_done = tcc_last_tx_done,
};

static const struct mbox_chan_ops tcc_mbox_chan_ops_v2 = {
	.send_data = tcc_mbox_send_data,
	.startup = tcc_mbox_startup,
	.shutdown = tcc_mbox_shutdown,
	.peek_data = tcc_mbox_peek_data,
	.last_tx_done = NULL,
};

static const struct mbox_soc_ops soc_data_v1 = {
	.version = 1,
	.tcc_mbox_can_trans = tcc_mbox_can_trans,
	.tcc_mbox_tx_irq_handler = NULL,
};

static const struct mbox_soc_ops soc_data_v2 = {
	.version = 2,
	.tcc_mbox_can_trans = tcc_mbox_can_trans,
	.tcc_mbox_tx_irq_handler = tcc_mbox_tx_irq_handler,
};

static const struct of_device_id tcc_mbox_of_match[3] = {
	{.compatible = "telechips,mailbox-controller-v1", .data = &soc_data_v1},
	{.compatible = "telechips,mailbox-controller-v2", .data = &soc_data_v2},
	{}
};

MODULE_DEVICE_TABLE(of, tcc_mbox_of_match);

static int tcc_mbox_probe(struct platform_device *pdev)
{
	struct tcc_mbox_device *mdev = NULL;
	const struct of_device_id *match;
	const struct resource *regs;
	int ret = 0;

	if (pdev == NULL) {
		/* Please check kernel probe process */
		ret = -EINVAL;
	}

	if ((ret == 0) && (pdev->dev.of_node == NULL)) {
		/* Please check kernel probe process */
		ret = -ENODEV;
	}

	if (ret == 0) {
		mdev = devm_kzalloc(&pdev->dev,
				sizeof(struct tcc_mbox_device), GFP_KERNEL);
		if (mdev == NULL) {
			/* Please check memory size */
			ret = -ENOMEM;
		} else {
			match = of_match_node(tcc_mbox_of_match, pdev->dev.of_node);
			if (match != NULL) {
				mdev->soc_ops = match->data;
			} else {
				ret = -ENODEV;
			}
		}
	}

	if (ret == 0) {
		mdev->rx_cmd_buf_len = MBOX_MAX_CMD_LENGTH;
		mdev->rx_msg.cmd = devm_kzalloc(&pdev->dev,
				sizeof(u32) * mdev->rx_cmd_buf_len, GFP_KERNEL);
		if (mdev->rx_msg.cmd == NULL) {
			/* Please check memory size */
			ret = -ENOMEM;
		}
	}

	if (ret == 0) {
		mdev->rx_data_buf_len = MBOX_MAX_DATA_LENGTH;
		mdev->rx_msg.data_buf = devm_kzalloc(&pdev->dev,
				sizeof(u32) * mdev->rx_data_buf_len,
				GFP_KERNEL);
		if (mdev->rx_msg.data_buf == NULL) {
			/* Please check memory size */
			ret = -ENOMEM;
		}
	}

	if (ret == 0) {
		regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);

		mdev->mbox_base = devm_ioremap_resource(&pdev->dev, regs);
		if (IS_ERR(mdev->mbox_base)) {
			dev_err(&pdev->dev,
				"[ERROR][%s] Failed to get resource\n", __func__);
			ret = PTR_ERR_OR_ZERO(mdev->mbox_base);
		}
	}

	if (ret == 0) {
		mdev->rx_irq = platform_get_irq(pdev, 0);
		if (mdev->rx_irq < 0) {
			dev_err(&pdev->dev,
					"[ERROR][%s] Failed to get RX_IRQ. ret: %d\n",
					__func__, mdev->rx_irq);
			ret = mdev->rx_irq;
		}
	}

	if (ret == 0) {
		mdev->mbox.chans = devm_kzalloc(&pdev->dev,
				sizeof(struct mbox_chan), GFP_KERNEL);
		if (mdev->mbox.chans == NULL) {
			/* Please check memory size */
			ret = -ENOMEM;
		}
	}

	if (ret == 0) {
		/* Disable output */
		tcc_mbox_clr_ctrl(mdev, ((u32)1 << MBOX_CTRL_OEN));

		/* Disable rx interrupt */
		tcc_mbox_clr_ctrl(mdev,
				((u32)1 << MBOX_CTRL_IEN_READ) |
				(MBOX_ILEVEL_NEMP << MBOX_CTRL_ILEVEL));

		/* Register interrupt handler */
		ret = devm_request_threaded_irq(&pdev->dev, (u32)mdev->rx_irq,
				tcc_mbox_rx_irq_handler,
				tcc_mbox_rx_isr_handler,
				IRQF_ONESHOT, dev_name(&pdev->dev),
				mdev->mbox.chans);
		if (ret < 0) {
			dev_err(&pdev->dev,
					"[ERROR][%s] Failed to request rx_irq\n", __func__);
		}
	}

	if (ret == 0) {
		if (mdev->soc_ops->version == 2) {
			/* Clear and disable tx interrupt */
			tcc_mbox_set_ctrl(mdev,
					((u32)1 << MBOX_CTRL_ICLR_WRITE));
			tcc_mbox_clr_ctrl(mdev,
					((u32)1 << MBOX_CTRL_IEN_WRITE));

			mdev->tx_irq = platform_get_irq(pdev, 1);
			if (mdev->tx_irq < 0) {
				dev_err(&pdev->dev,
						"[ERROR][%s] Failed to get TX_IRQ. ret: %d\n",
						__func__, mdev->tx_irq);
				ret = mdev->tx_irq;
			}

			if (ret == 0) {
				ret = devm_request_threaded_irq(&pdev->dev, (u32)mdev->tx_irq,
						tcc_mbox_tx_irq_handler,
						tcc_mbox_tx_isr_handler,
						IRQF_ONESHOT, dev_name(&pdev->dev),
						mdev->mbox.chans);
				if (ret < 0) {
					dev_err(&pdev->dev,
							"[ERROR][%s] Failed to request tx_irq\n", __func__);
				}
			}
		}
	}

	if (ret == 0) {
		mdev->mbox.chans->con_priv = mdev;

		platform_set_drvdata(pdev, mdev);

		spin_lock_init(&mdev->lock);
		/* Initialize mbox controller */
		mdev->dev = &pdev->dev;
		mdev->mbox.dev = &pdev->dev;
		mdev->mbox.num_chans = 1;
		if (mdev->soc_ops->version == 2) {
			mdev->mbox.ops = &tcc_mbox_chan_ops_v2;
			mdev->mbox.txdone_irq = (bool)true;
			mdev->mbox.txdone_poll = (bool)false;
			mdev->mbox.txpoll_period = 0;
			tasklet_init(&mdev->finish_tasklet,
				tcc_mbox_tasklet_finish,(unsigned long)mdev);
		} else if (mdev->soc_ops->version == 1) {
			mdev->mbox.ops = &tcc_mbox_chan_ops_v1;
			mdev->mbox.txdone_irq = (bool)false;
			mdev->mbox.txdone_poll = (bool)true;
			/* millisecond */
			mdev->mbox.txpoll_period = 30;
		}

		ret = mbox_controller_register(&mdev->mbox);
		if (ret < 0) {
			dev_err(&pdev->dev,
					"[ERROR][%s] Failed to register mailbox: %d\n",
					__func__,ret);
		}

	}

	if (ret == 0) {
		dev_info(&pdev->dev,
				"[INFO][%s] register mbox-tcc-controller\n", __func__);
	}

	return ret;
}

static int tcc_mbox_remove(struct platform_device *pdev)
{
	struct tcc_mbox_device *mdev = platform_get_drvdata(pdev);
	int ret = 0;

	if (mdev == NULL) {
		ret = -EINVAL;
	} else {
		mbox_controller_unregister(&mdev->mbox);

		/* Disable output */
		tcc_mbox_clr_ctrl(mdev, ((u32)1 << MBOX_CTRL_OEN));

		/* Disable rx interrupt */
		tcc_mbox_clr_ctrl(mdev,
				((u32)1 << MBOX_CTRL_IEN_READ) |
				(MBOX_ILEVEL_NEMP << MBOX_CTRL_ILEVEL));

		if (mdev->soc_ops->version == 2) {
			/* Clear and disable tx interrupt */
			tcc_mbox_set_ctrl(mdev, ((u32)1 << MBOX_CTRL_ICLR_WRITE));
			tcc_mbox_clr_ctrl(mdev, ((u32)1 << MBOX_CTRL_IEN_WRITE));
			/* Set terminal status register */
			tcc_mbox_writel(mdev, 0U, MBOX_OPPOSITE_STS);
		}
	}

	return ret;
}

static int tcc_mbox_suspend(struct device *dev)
{
	const struct tcc_mbox_device *mdev = dev_get_drvdata(dev);
	int ret = 0;

	if (mdev == NULL) {
		ret = -EINVAL;
	} else {
		/* Disable output */
		tcc_mbox_clr_ctrl(mdev, ((u32)1 << MBOX_CTRL_OEN));

		/* Disable rx interrupt */
		tcc_mbox_clr_ctrl(mdev,
			     ((u32)1 << MBOX_CTRL_IEN_READ) |
			     (MBOX_ILEVEL_NEMP << MBOX_CTRL_ILEVEL));

		if (mdev->soc_ops->version == 2) {
			/* Clear and disable tx interrupt */
			tcc_mbox_set_ctrl(mdev, ((u32)1 << MBOX_CTRL_ICLR_WRITE));
			tcc_mbox_clr_ctrl(mdev, ((u32)1 << MBOX_CTRL_IEN_WRITE));

			/* Set terminal status register */
			tcc_mbox_writel(mdev, 0U, MBOX_OPPOSITE_STS);
		}
	}

	return ret;
}

static int tcc_mbox_resume(struct device *dev)
{
	const struct tcc_mbox_device *mdev = dev_get_drvdata(dev);
	int ret = 0;

	if (mdev == NULL) {
		(void)pr_err(
				"[ERROR][%s] Device is null\n",
				__func__);
		ret = -EINVAL;
	} else {
		/* Disable output */
		tcc_mbox_clr_ctrl(mdev, ((u32)1 << MBOX_CTRL_OEN));

		/* Flush command and data FIFO */
		tcc_mbox_set_ctrl(mdev,
				((u32)1 << MBOX_CTRL_CF_FLUSH) |
				((u32)1 << MBOX_CTRL_DF_FLUSH));

		/* Set rx interrupt */
		tcc_mbox_set_ctrl(mdev,
				((u32)1 << MBOX_CTRL_IEN_READ) |
				(MBOX_ILEVEL_NEMP << MBOX_CTRL_ILEVEL));

		if (mdev->soc_ops->version == 2) {
			/* Set terminal status register */
			tcc_mbox_writel(mdev, 1U, MBOX_OPPOSITE_STS);
		}
	}

	return ret;
}

static const struct dev_pm_ops tcc_mbox_pm = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS((tcc_mbox_suspend),
				     (tcc_mbox_resume))
};

static struct platform_driver tcc_mbox_driver = {
	.probe = tcc_mbox_probe,
	.remove = tcc_mbox_remove,
	.driver = {
		   .name = "tcc-mailbox",
		   .pm = &tcc_mbox_pm,
		   .of_match_table = of_match_ptr(tcc_mbox_of_match),
		   },
};

static int __init tcc_mbox_init(void)
{
	return platform_driver_register(&tcc_mbox_driver);
}

core_initcall(tcc_mbox_init);

static void __exit tcc_mbox_exit(void)
{
	platform_driver_unregister(&tcc_mbox_driver);
}

module_exit(tcc_mbox_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Telechips Mailbox Driver");
MODULE_AUTHOR("peter.choi@telechips.com");
