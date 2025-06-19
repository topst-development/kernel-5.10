// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/types.h>
#include <linux/wait_bit.h>
#include <linux/interrupt.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/mailbox/mailbox-tcc-sss.h>
#include <linux/kthread.h>
#include <linux/delay.h>

/* Samsung SSS mbox */
// clang-format off
#define SSS_MBOX_STATU						(0x0000U)
#define SSS_MBOX_CTR						(0x2000U)
#define SSS_MBOX_CMD(x)						(0x2010U + (x) * 4U)
#define SSS_MBOX_DATA(x)					(0x2030U + (x) * 4U)
#define SSS_MBOX_PEND 						(0x003Cu)

#define SSS_MBOX_SUCCESS 					(0xA1U)
#define SSS_MBOX_FAIL 						(0xF1U)

#define SSS_MBOX_IDLE 						(0x00U)
#define SSS_MBOX_BUSY 						(0x01U)

#define SSS_MAX_ATTEMPT_COUNT 				(100U)

#define TCCMBOX_CID_A53 					(0x53u)
#define TCCMBOX_BSID_KERNEL 				(0x46u)
#define MBOX_ID_MAX_LEN 					(6U)
// clang-format on

struct tcc_sss_mbox_device {
	struct mbox_controller mbox;
	void __iomem *mbox_base;
	int irq;

	spinlock_t lock;
	struct mutex sendLock;
	struct tcc_mbox_data *msg;
};

struct channel_id0 {
	s8 bsid[1];
	s8 cid[1];
	char idName[2];
};

struct channel_id1 {
	char idName[4];
};

union mbox_header0 {
	struct channel_id0 id;
	uint32_t cmd;
};

union mbox_header1 {
	struct channel_id1 id;
	uint32_t cmd;
};

struct tcc_channel {
	struct tcc_sss_mbox_device *mdev;
	uint32_t channel;

	struct tcc_mbox_data *msg;

	union mbox_header0 header0;
	union mbox_header1 header1;
};

static int tcc_sss_mbox_send_data(struct mbox_chan *chan, void *data)
{
	struct tcc_sss_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);
	struct tcc_mbox_data *msg = (struct tcc_mbox_data *)data;
	struct tcc_channel *chan_info = chan->con_priv;
	unsigned long flags;
	int32_t i;
	int32_t ret = 0;

	/* check mailbox busy */
	for (i = 0; i < SSS_MAX_ATTEMPT_COUNT; i++) {
		if (((readl_relaxed(mdev->mbox_base) &
		      (uint32_t)SSS_MBOX_BUSY) == (uint32_t)SSS_MBOX_IDLE)) {
			break;
		}
	}
	if (i >= SSS_MAX_ATTEMPT_COUNT) {
		ret = -EBUSY;
		return ret;
	}

	/* write cmd */
	spin_lock_irqsave(&mdev->lock, flags);
	writel_relaxed(chan_info->header0.cmd,
		       mdev->mbox_base + SSS_MBOX_CMD(0));
	writel_relaxed(msg->cmd[0], mdev->mbox_base + SSS_MBOX_CMD(1));
	writel_relaxed(msg->cmd[1], mdev->mbox_base + SSS_MBOX_CMD(2));
	writel_relaxed(msg->cmd[2], mdev->mbox_base + SSS_MBOX_CMD(3));
	writel_relaxed(msg->cmd[3], mdev->mbox_base + SSS_MBOX_CMD(4));
	writel_relaxed(chan_info->header1.cmd,
		       mdev->mbox_base + SSS_MBOX_CMD(5)); //HSM

	/* write data */
	for (i = 0; i < msg->data_len; i++) {
		writel_relaxed(msg->data[i],
			       mdev->mbox_base + SSS_MBOX_DATA(i));
	}

	/* enable transmit data output. */
	writel_relaxed(0x02, mdev->mbox_base + SSS_MBOX_CTR);

	/* SSS Busy Check */
	for (i = 0; i < SSS_MAX_ATTEMPT_COUNT; i++) {
		if (((readl_relaxed(mdev->mbox_base) &
		      (uint32_t)SSS_MBOX_BUSY) == (uint32_t)SSS_MBOX_IDLE)) {
			break;
		}
	}
	if (i >= SSS_MAX_ATTEMPT_COUNT) {
		spin_unlock_irqrestore(&mdev->lock, flags);
		return -EBUSY;
	}
	/* Check Recived Data */
	ret = readl_relaxed(mdev->mbox_base + SSS_MBOX_CTR);
	if (ret == SSS_MBOX_SUCCESS) {
		ret = 0;
	} else {
		ret = -EBUSY;
	}
	spin_unlock_irqrestore(&mdev->lock, flags);
	return ret;
}

static int tcc_sss_mbox_startup(struct mbox_chan *chan)
{
	int32_t ret = 0;
	struct tcc_sss_mbox_device *mdev;
	struct tcc_channel *chan_info = NULL;
	const struct mbox_client *cl = NULL;
	const struct mbox_controller *mbox = NULL;
	char mbox_id[MBOX_ID_MAX_LEN] = { 'H', 'S', 'M' };
	unsigned long flags;

	if (chan != NULL) {
		mdev = dev_get_drvdata(chan->mbox->dev);
		chan_info = chan->con_priv;
		cl = chan->cl;
		mbox = chan->mbox;
	}

	if ((mdev != NULL) && (chan_info != NULL) && (cl != NULL)) {
		mutex_lock(&mdev->sendLock);

		chan_info->header0.id.idName[0] = mbox_id[4];
		chan_info->header0.id.idName[1] = mbox_id[5];
		chan_info->header0.id.cid[0] = TCCMBOX_CID_A53;
		chan_info->header0.id.bsid[0] = TCCMBOX_BSID_KERNEL;

		chan_info->header1.id.idName[0] = mbox_id[0];
		chan_info->header1.id.idName[1] = mbox_id[1];
		chan_info->header1.id.idName[2] = mbox_id[2];
		chan_info->header1.id.idName[3] = mbox_id[3];

		chan_info->msg =
			devm_kzalloc(chan->mbox->dev,
				     sizeof(struct tcc_mbox_data), GFP_KERNEL);
		mutex_unlock(&mdev->sendLock);
	} else {
		ret = -EINVAL;
	}
	/* Enable Interrupt */
	spin_lock_irqsave(&mdev->lock, flags);
	writel_relaxed(0x00, mdev->mbox_base + SSS_MBOX_PEND);
	spin_unlock_irqrestore(&mdev->lock, flags);
	return ret;
}

static void tcc_sss_mbox_shutdown(struct mbox_chan *chan)
{
	struct tcc_sss_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);
	unsigned long flags;

	/* Disable interrupt */
	spin_lock_irqsave(&mdev->lock, flags);
	writel_relaxed(0x01, mdev->mbox_base + SSS_MBOX_PEND);
	spin_unlock_irqrestore(&mdev->lock, flags);
}

static bool tcc_sss_mbox_last_tx_done(struct mbox_chan *chan)
{
	struct tcc_sss_mbox_device *mdev = dev_get_drvdata(chan->mbox->dev);

	if ((readl_relaxed(mdev->mbox_base + SSS_MBOX_CTR) != SSS_MBOX_SUCCESS))
		return false;

	if ((readl_relaxed(mdev->mbox_base + 0x0010) != 0x01))
		return false;

	return true;
}

static const struct mbox_chan_ops tcc_sss_mbox_chan_ops = {
	.send_data = tcc_sss_mbox_send_data,
	.startup = tcc_sss_mbox_startup,
	.shutdown = tcc_sss_mbox_shutdown,
	.last_tx_done = tcc_sss_mbox_last_tx_done,
};

static irqreturn_t tcc_sss_mbox_irq(int irq, void *dev_id)
{
	struct tcc_sss_mbox_device *mdev = (struct tcc_sss_mbox_device *)dev_id;
	unsigned long flags;
	if ((irq == mdev->irq)) {
		spin_lock_irqsave(&mdev->lock, flags);
		writel_relaxed(0x01, mdev->mbox_base + SSS_MBOX_PEND);
		spin_unlock_irqrestore(&mdev->lock, flags);
		return IRQ_WAKE_THREAD;
	}
	return IRQ_NONE;
}

static irqreturn_t tcc_sss_mbox_isr(int irq, void *dev_id)
{
	struct tcc_sss_mbox_device *mdev = (struct tcc_sss_mbox_device *)dev_id;
	int32_t idx;
	int32_t i;
	unsigned long flags;
	if (irq != mdev->irq) {
		return IRQ_NONE;
	}

	if (!mdev) {
		return IRQ_NONE;
	}

	if (!mdev->msg) {
		return IRQ_NONE;
	}
	/* SSS Busy Check */
	spin_lock_irqsave(&mdev->lock, flags);
	for (i = 0; i < SSS_MAX_ATTEMPT_COUNT; i++) {
		if (((readl_relaxed(mdev->mbox_base) &
		      (uint32_t)SSS_MBOX_BUSY) == (uint32_t)SSS_MBOX_IDLE)) {
			break;
		}
	}
	if (i >= SSS_MAX_ATTEMPT_COUNT) {
		spin_unlock_irqrestore(&mdev->lock, flags);
		return IRQ_NONE;
	}
	if (readl_relaxed(mdev->mbox_base + SSS_MBOX_CTR) != SSS_MBOX_SUCCESS) {
		spin_unlock_irqrestore(&mdev->lock, flags);
		return IRQ_NONE;
	}

	mdev->msg->cmd[0] = readl_relaxed(mdev->mbox_base + SSS_MBOX_CMD(1));
	mdev->msg->cmd[1] = readl_relaxed(mdev->mbox_base + SSS_MBOX_CMD(2));
	mdev->msg->cmd[2] = readl_relaxed(mdev->mbox_base + SSS_MBOX_CMD(3));
	mdev->msg->cmd[3] = readl_relaxed(mdev->mbox_base + SSS_MBOX_CMD(4));

	for (idx = 0; idx < (mdev->msg->cmd[3] + 3) / 4; idx++) {
		mdev->msg->data[idx] =
			readl_relaxed(mdev->mbox_base + SSS_MBOX_DATA(idx));
	}
	spin_unlock_irqrestore(&mdev->lock, flags);

	mbox_chan_received_data(&mdev->mbox.chans[0], (void *)mdev->msg);
	mdev->mbox.chans->msg_count = 0;
	/* Set mbox received interrupt bit */
	spin_lock_irqsave(&mdev->lock, flags);
	writel_relaxed(0x00, mdev->mbox_base + SSS_MBOX_PEND);
	spin_unlock_irqrestore(&mdev->lock, flags);
	return IRQ_HANDLED;
}

static struct mbox_chan *tcc_sss_mbox_xlate(struct mbox_controller *mbox,
					    const struct of_phandle_args *spec)
{
	struct tcc_sss_mbox_device *mdev = dev_get_drvdata(mbox->dev);
	struct mbox_chan *chan = NULL;
	struct tcc_channel *chan_info;
	if ((mbox != NULL) && (spec != NULL)) {
		uint32_t channel = spec->args[0];

		chan = &mbox->chans[channel];
		chan_info =
			devm_kzalloc(mbox->dev, sizeof(*chan_info), GFP_KERNEL);

		if (chan_info == NULL) {
			chan = ERR_PTR(-ENOMEM);
		} else {
			chan_info->mdev = mdev;
			chan_info->channel = (uint32_t)channel;

			chan->con_priv = chan_info;
		}
	}
	return chan;
}

static const struct of_device_id tcc_sss_mbox_of_match[] = {
	{ .compatible = "telechips,sss-mailbox" },
	{},
};
MODULE_DEVICE_TABLE(of, tcc_sss_mbox_of_match);

static int32_t tcc_sss_mbox_probe(struct platform_device *pdev)
{
	struct tcc_sss_mbox_device *mdev = NULL;
	int32_t ret = 0;

	if (pdev != NULL) {
		if (pdev->dev.of_node == NULL) {
			ret = -ENODEV;
		}
	} else {
		ret = -EINVAL;
	}

	mdev = devm_kzalloc(&pdev->dev, sizeof(struct tcc_sss_mbox_device),
			    GFP_KERNEL);
	if (!mdev)
		return -ENOMEM;

	mdev->msg = devm_kzalloc(&pdev->dev, sizeof(struct tcc_mbox_data),
				 GFP_KERNEL);
	if (!mdev->msg)
		return -ENOMEM;

	platform_set_drvdata(pdev, mdev);

	mdev->mbox.dev = &pdev->dev;
	mdev->mbox.num_chans = 1;
	mdev->mbox.ops = &tcc_sss_mbox_chan_ops;
	mdev->mbox.txdone_irq = (bool)false;
	mdev->mbox.txdone_poll = (bool)true;
	mdev->mbox.txpoll_period = 1; /* 1ms */
	mdev->mbox.of_xlate = &tcc_sss_mbox_xlate;
	mdev->mbox_base = of_iomap(pdev->dev.of_node, 0);
	/* Allocated one channel */
	mdev->mbox.chans =
		devm_kzalloc(&pdev->dev, sizeof(struct mbox_chan), GFP_KERNEL);
	if (!mdev->mbox.chans)
		return -ENOMEM;

	if (IS_ERR(mdev->mbox_base)) {
		return PTR_ERR(mdev->mbox_base);
	}

	spin_lock_init(&mdev->lock);

	mdev->irq = platform_get_irq(pdev, 0);
	if (mdev->irq < 0) {
		return mdev->irq;
	}

	writel_relaxed(0x01, mdev->mbox_base + SSS_MBOX_PEND);

	ret = devm_request_threaded_irq(&pdev->dev, mdev->irq, tcc_sss_mbox_irq,
					tcc_sss_mbox_isr, IRQF_ONESHOT,
					dev_name(&pdev->dev), mdev);

	if (ret < 0)
		return ret;

	ret = mbox_controller_register(&mdev->mbox);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register mailbox: %d\n", ret);
	}

	return ret;
}

static int32_t tcc_sss_mbox_remove(struct platform_device *pdev)
{
	struct tcc_sss_mbox_device *mdev = platform_get_drvdata(pdev);

	if (!mdev)
		return -EINVAL;

	mbox_controller_unregister(&mdev->mbox);
	return 0;
}

int32_t tcc_sss_mbox_suspend(struct device *dev)
{
	struct tcc_sss_mbox_device *mdev = dev_get_drvdata(dev);

	/* disable received interrupt */
	writel_relaxed(0x01, mdev->mbox_base + SSS_MBOX_PEND);
	return 0;
}

int32_t tcc_sss_mbox_resume(struct device *dev)
{
	struct tcc_sss_mbox_device *mdev = dev_get_drvdata(dev);

	/* enable received interrupt */
	writel_relaxed(0x00, mdev->mbox_base + SSS_MBOX_PEND);

	return 0;
}

static const struct dev_pm_ops tcc_sss_mbox_pm = { SET_LATE_SYSTEM_SLEEP_PM_OPS(
	tcc_sss_mbox_suspend, tcc_sss_mbox_resume) };

static struct platform_driver tcc_sss_mbox_driver = {
	.probe = tcc_sss_mbox_probe,
	.remove = tcc_sss_mbox_remove,
	.driver =
		{
			.name = "tcc-sss-mailbox",
			.pm = &tcc_sss_mbox_pm,
			.of_match_table = of_match_ptr(tcc_sss_mbox_of_match),
		},
};

static int32_t __init tcc_sss_mbox_init(void)
{
	return platform_driver_register(&tcc_sss_mbox_driver);
}
subsys_initcall(tcc_sss_mbox_init);

static void __exit tcc_sss_mbox_exit(void)
{
	platform_driver_unregister(&tcc_sss_mbox_driver);
}
module_exit(tcc_sss_mbox_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Telechips SSS Mailbox Driver");
MODULE_AUTHOR("hj.jeon@telechips.com");
