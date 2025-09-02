// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Telechips Inc.
 */

#define pr_fmt(fmt) "tcc-pmi: " fmt

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/firmware/tcc_ipi.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sched/signal.h>
#include <linux/spinlock.h>

#define MSGLEN ((size_t)MBOX_MSG_CMD_MAX_LEN << 2U)

struct power_msg {
	u8 cmd[MSGLEN];
	struct list_head list;
};

struct power_mbox {
	struct list_head msgs;
	spinlock_t lock;
	struct wait_queue_head wq;
};

struct pmi_data {
	struct platform_device *pdev;
	struct device *dev;
	struct cdev chrdev;
	struct power_mbox mbox;
	struct device_node *mbox_client;
	int index;
};

static void power_mbox_print_msg(const char *prefix, const u8 *cmd)
{
	char buf[MSGLEN * 3U];
	const char *shorten;
	size_t cmdlen;
	const size_t bufsz = sizeof(buf);

	for (cmdlen = MSGLEN; cmdlen > 0U; cmdlen--) {
		if (cmd[cmdlen - 1U] != 0U) {
			break;
		}
	}

	shorten = (cmdlen < (MSGLEN - 3U)) ? " 00 .. 00" : "";

	(void)hex_dump_to_buffer(cmd, cmdlen, 32, 1, buf, bufsz, (bool)false);
	(void)pr_info("%s: %s%s\n", prefix, buf, shorten);
}

static struct power_msg *power_mbox_get_msg(struct power_mbox *mbox)
{
	struct power_msg *msg;
	ulong flags;

	spin_lock_irqsave(&mbox->lock, flags);

	msg = list_first_entry_or_null(&mbox->msgs, struct power_msg, list);
	if (msg != NULL) {
		/* Delete the message from list */
		list_del(&msg->list);
	}

	spin_unlock_irqrestore(&mbox->lock, flags);

	return msg;
}

static struct power_msg *power_mbox_poll_msg(struct power_mbox *mbox)
{
	struct power_msg *msg;
	int sigpend;
	bool retry = (bool)true;

	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(&mbox->wq, &wait);

	/* Poll message until either catching a message or signal. */
	while (retry) {
		__set_current_state(TASK_INTERRUPTIBLE);

		msg = power_mbox_get_msg(mbox);
		sigpend = signal_pending(current);
		retry = (bool)false;

		if ((msg == NULL) && (sigpend == 0)) {
			schedule();
			retry = (bool)true;
		}
	}

	set_current_state(TASK_RUNNING);
	remove_wait_queue(&mbox->wq, &wait);

	return msg;
}

static void power_mbox_put_msg(struct power_mbox *mbox, struct power_msg *msg)
{
	ulong flags;
	int empty;

	spin_lock_irqsave(&mbox->lock, flags);

	empty = list_empty(&mbox->msgs);
	list_add_tail(&msg->list, &mbox->msgs);

	if (empty != 0) {
		/* Wake the one waiting for any messages coming */
		wake_up_interruptible(&mbox->wq);
	}

	spin_unlock_irqrestore(&mbox->lock, flags);
}

static inline void power_mbox_rm_msg(struct power_msg *msg, struct device *dev)
{
	list_del(&msg->list);
	devm_kfree(dev, msg);
}

static void power_mbox_purge_msg(struct power_mbox *mbox, struct device *dev)
{
	struct power_msg *msg;
	struct power_msg *next;
	ulong flags;

	spin_lock_irqsave(&mbox->lock, flags);

	list_for_each_entry_safe(msg, next, &mbox->msgs, list) {
		power_mbox_rm_msg(msg, dev);
	}

	spin_unlock_irqrestore(&mbox->lock, flags);
}

static int power_mbox_rx_callback(struct tcc_ipi_msg *m,
				  struct platform_device *pdev)
{
	struct pmi_data *data = platform_get_drvdata(pdev);
	struct power_msg *msg = devm_kmalloc(data->dev, MSGLEN, GFP_KERNEL);

	if (msg != NULL) {
		(void)memcpy(msg->cmd, m->cmd, MSGLEN);
		power_mbox_put_msg(&data->mbox, msg);
		power_mbox_print_msg("receive", msg->cmd);
	}

	return 0;
}

static ssize_t power_mbox_read(struct file *filp, char __user *buf,
			       size_t len, loff_t *ppos)
{
	const struct power_msg *msg;
	ssize_t cnt;
	const char *fail = NULL;
	struct pmi_data *data = filp->private_data;

	if (len < MSGLEN) {
		fail = "Buffer too short";
		msg = NULL;
		cnt = -EINVAL;
	} else if ((filp->f_flags & (u32)O_NONBLOCK) != 0U) {
		msg = power_mbox_get_msg(&data->mbox);
		cnt = -EAGAIN;
	} else {
		msg = power_mbox_poll_msg(&data->mbox);
		cnt = -ERESTARTSYS;
	}

	if (msg != NULL) {
		cnt = simple_read_from_buffer(buf, MSGLEN, ppos, msg->cmd, len);
		if (cnt <= 0) {
			fail = "Failed to copy message";
		} else {
			power_mbox_print_msg("read", msg->cmd);
		}

		*ppos = 0;
		devm_kfree(data->dev, msg);
	}

	if (fail != NULL) {
		(void)pr_err("read: %s (err: %zd)\n", fail, cnt);
	}

	return cnt;
}

static ssize_t power_mbox_write(struct file *filp, const char __user *buf,
				size_t len, loff_t *ppos)
{
	struct tcc_ipi_msg mdata;
	ssize_t cnt;
	const struct pmi_data *data = filp->private_data;
	struct device_node *cl = data->mbox_client;
	int id = data->index;
	const char *fail = NULL;

	if (len > MSGLEN) {
		fail = "Buffer too long";
		cnt = -EINVAL;
	} else {
		u32 cmd[MBOX_MSG_CMD_MAX_LEN];

		(void)memset(&cmd, 0, sizeof(cmd));

		mdata.cmd = cmd;
		mdata.cmd_len = MBOX_MSG_CMD_MAX_LEN;
		mdata.data_len = 0;

		cnt = simple_write_to_buffer(mdata.cmd, MSGLEN, ppos, buf, len);
		if (cnt <= 0) {
			fail = "Failed to copy message";
		} else {
			int ret = tcc_ipi_send_data(cl, &mdata, id);
			if (ret < 0) {
				fail = "Failed to send message";
				cnt = (ssize_t)ret;
			} else {
				power_mbox_print_msg("write", (u8 *)mdata.cmd);
			}
		}

		*ppos = 0;
	}

	if (fail != NULL) {
		(void)pr_err("write: %s (err: %zd)\n", fail, cnt);
	}

	return cnt;
}

static int power_mbox_open(struct inode *in, struct file *filp)
{
	filp->private_data = NULL;

	if (in->i_cdev != NULL) {
		filp->private_data =
			container_of(in->i_cdev, struct pmi_data, chrdev);
	}

	return (filp->private_data == NULL) ? -ENODEV : 0;
}

static const struct file_operations power_mbox_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.read = power_mbox_read,
	.write = power_mbox_write,
	.open = power_mbox_open,
};

static void tcc_pmi_power_mbox_struct_init(struct pmi_data *data)
{
	struct power_mbox *mbox = &data->mbox;

	INIT_LIST_HEAD(&mbox->msgs);
	init_waitqueue_head(&mbox->wq);
	spin_lock_init(&mbox->lock);
}

static int tcc_pmi_power_mbox_client_init(struct pmi_data *data)
{
	struct device_node *cl;
	struct platform_device *pdev = data->pdev;
	struct tcc_ipi_register_dat mbox_prot_dat = {0,};
	int ret = -ENODEV;

	memcpy(mbox_prot_dat.id, "POWER0", TCC_IPI_PROTOCOL_ID_MAX_LEN);

	mbox_prot_dat.rx_callback = &power_mbox_rx_callback;
	mbox_prot_dat.pdev = pdev;

	cl = of_parse_phandle(pdev->dev.of_node, "tcc_ipi", 0);
	if (cl != NULL) {
		ret = tcc_ipi_register(cl, &mbox_prot_dat);
		if (ret < 0) {
			cl = NULL;
			ret = -EPROBE_DEFER;
		}
	}

	data->mbox_client = cl;

	return ret;
}

static int tcc_pmi_cdev_init(struct pmi_data *data)
{
	dev_t devt;
	int ret;
	struct cdev *chrdev = &data->chrdev;

	ret = alloc_chrdev_region(&devt, 0, 1, "tcc-pmi");
	if (ret == 0) {
		cdev_init(chrdev, &power_mbox_fops);
		chrdev->owner = THIS_MODULE;

		ret = cdev_add(chrdev, devt, 1);
		if (ret != 0) {
			unregister_chrdev_region(devt, 1);
		}
	}

	return ret;
}

static void tcc_pmi_cdev_free(struct pmi_data *data)
{
	struct cdev *chrdev = &data->chrdev;

	cdev_del(chrdev);
	unregister_chrdev_region(chrdev->dev, 1);
}

static int tcc_pmi_power_mbox_dev_init(struct pmi_data *data)
{
	struct class *cls;
	struct device *dev;
	int ret;
	const dev_t devt = data->chrdev.dev;

	cls = class_create(THIS_MODULE, "mbox");
	ret = PTR_ERR_OR_ZERO(cls);
	if (ret == 0) {
		dev = device_create(cls, NULL, devt, NULL, "mbox_power");
		ret = PTR_ERR_OR_ZERO(dev);
		if (ret < 0) {
			class_destroy(cls);
		} else {
			data->dev = dev;
		}
	}

	return ret;
}

static void tcc_pmi_power_mbox_dev_free(struct pmi_data *data)
{
	const struct device *dev = data->dev;
	const dev_t devt = data->chrdev.dev;
	struct class *cls = dev->class;

	device_destroy(cls, devt);
	class_destroy(cls);
}

static int tcc_pmi_data_init(struct pmi_data *data)
{
	int ret;
	const char *fail = NULL;

	/* Initialize power mailbox struct */
	tcc_pmi_power_mbox_struct_init(data);

	/* Initialize power mailbox channel */
	ret = tcc_pmi_power_mbox_client_init(data);
	if (ret < 0) {
		fail = "Failed to init power mbox channel";
	} else {
		data->index = ret;

		/* Initialize character device for power mailbox */
		ret = tcc_pmi_cdev_init(data);
		if (ret != 0) {
			fail = "Failed to init character device";
		} else {
			/* Initialize device for power mailbox */
			ret = tcc_pmi_power_mbox_dev_init(data);
			if (ret != 0) {
				fail = "Failed to init device";
				tcc_pmi_cdev_free(data);
			}
		}
	}

	if (fail != NULL) {
		(void)pr_err("%s (err: %d)\n", fail, ret);
	}

	return ret;
}

static int tcc_pmi_probe(struct platform_device *pdev)
{
	int ret;
	struct pmi_data *data;
	struct device *dev = &pdev->dev;

	/* Allocate memory for driver data */
	data = devm_kzalloc(dev, sizeof(struct pmi_data), GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		(void)pr_err("Failed to allocate driver data (err: %d)\n", ret);
	} else {
		platform_set_drvdata(pdev, data);
		data->pdev = pdev;

		/* Initialize PMI driver data */
		ret = tcc_pmi_data_init(data);
		if (ret != 0) {
			devm_kfree(dev, data);
		}
	}

	return ret;
}

static int tcc_pmi_remove(struct platform_device *pdev)
{
	struct pmi_data *data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	tcc_pmi_power_mbox_dev_free(data);
	tcc_pmi_cdev_free(data);
	devm_kfree(dev, data);

	return 0;
}

static int tcc_pmi_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct pmi_data *data = platform_get_drvdata(pdev);

	power_mbox_purge_msg(&data->mbox, data->dev);

	return 0;
}

static const struct of_device_id tcc_pmi_match[2] = {
	{ .compatible = "telechips,pmi" },
	{ .compatible = "" }
};

MODULE_DEVICE_TABLE(of, tcc_pmi_match);

static struct platform_driver tcc_pmi_driver = {
	.probe = tcc_pmi_probe,
	.remove = tcc_pmi_remove,
	.suspend = tcc_pmi_suspend,
	.driver = {
		.name = "tcc-pmi",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tcc_pmi_match),
	},
};

module_platform_driver(tcc_pmi_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jigi Kim <jigi.kim@telechips.com>");
MODULE_DESCRIPTION("Telechips power management interface driver");
