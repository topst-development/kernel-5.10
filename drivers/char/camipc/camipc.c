// SPDX-License-Identifier: GPL-2.0-or-later
/*******************************************************************************
 *
 * Copyright (C) 2023 Telechips Inc.
 *
 ******************************************************************************/

#if IS_ENABLED(CONFIG_TCC_IPI_PROTOCOL)
#include "camipc_ipi_protocol/camipc_ipi_protocol.h"
#endif
#if IS_ENABLED(CONFIG_TCC_MAILBOX_CLIENT)
#include "camipc_mailbox_client/camipc_mailbox_client.h"
#endif
#if IS_ENABLED(CONFIG_TCC_MULTI_MAILBOX)
#include "camipc_multi_mbox/camipc_multi_mbox.h"
#endif

wait_queue_head_t receive_waitq;
int receive_flag;

__weak int camipc_send_message(struct camipc_device *p_camipc, packet *msg)
{
	loge("Please check Kconfig: failed to override weak symbol\n");
	return -1;
}
EXPORT_SYMBOL(camipc_send_message);

__weak int camipc_generate_meessage(struct camipc_device *p_camipc, packet *msg,
				    int ctrl_cmd, int cmd_sts, int *args,
				    int num_args)
{
	loge("Please check Kconfig: failed to override weak symbol\n");
	return -1;
}
EXPORT_SYMBOL(camipc_generate_meessage);

__weak int camipc_request_channel(struct camipc_device *p_camipc,
				  const char *name)
{
	loge("Please check Kconfig: failed to override weak symbol\n");
	return -1;
}
EXPORT_SYMBOL(camipc_request_channel);

__weak int camipc_probe(struct platform_device *p_pdev)
{
	loge("Please check Kconfig: failed to override weak symbol\n");
	return -1;
}
EXPORT_SYMBOL(camipc_probe);

__weak void camipc_set_ovp(const packet *msg)
{
	loge("Please check Kconfig: failed to override weak symbol\n");
}
EXPORT_SYMBOL(camipc_set_ovp);

__weak void camipc_set_pos(const packet *msg)
{
	loge("Please check Kconfig: failed to override weak symbol\n");
}
EXPORT_SYMBOL(camipc_set_pos);

__weak void camipc_set_reset(const packet *msg)
{
	loge("Please check Kconfig: failed to override weak symbol\n");
}
EXPORT_SYMBOL(camipc_set_reset);

__weak void camipc_parsing_set_ovp(struct camipc_device *p_camipc, packet *msg,
				   unsigned long arg, size_t data_size)
{
	loge("Please check Kconfig: failed to override weak symbol\n");
}
EXPORT_SYMBOL(camipc_parsing_set_ovp);

__weak int camipc_ioctl_handle(struct camipc_device *p_camipc, int cmd,
			       unsigned long arg)
{
	loge("Please check Kconfig: failed to override weak symbol\n");
	return -1;
}
EXPORT_SYMBOL(camipc_ioctl_handle);

void camipc_send_and_check_timeout(struct camipc_device *p_camipc, packet *msg)
{
	int ret;

	camipc_send_message(p_camipc, msg);
	ret = wait_event_interruptible_timeout(
		receive_waitq, (receive_flag == 1), msecs_to_jiffies(100));

	if (ret <= 0) {
		loge("Timeout camipc_send_message(%d)(%d)\n", ret,
		     receive_flag);
	}
	receive_flag = 0;
}
EXPORT_SYMBOL(camipc_send_and_check_timeout);

static int camipc_is_initialized(const struct camipc_device *p_camipc)
{
	uint32_t status = 0;
	int ret = 0;

	status = (uint32_t)atomic_read(&p_camipc->status);

	if (status >= 0) {
		if (status != (uint32_t)CAMIPC_STS_READY) {
			loge("Not ready to send message\n");
			ret = -100;
		}
	}

	return ret;
}

static long camipc_ioctl(struct file *filp, uint32_t cmd, unsigned long arg)
{
	struct camipc_device *p_camipc = filp->private_data;
	int ret;

	if (p_camipc == NULL) {
		ret = -ENODEV;
	} else {
		if (camipc_is_initialized(p_camipc) == 0) {
			ret = camipc_ioctl_handle(p_camipc, cmd, arg);
		} else {
			ret = -EINVAL;
		}
	}

	return (long)ret;
}

static int camipc_open(struct inode *pnode, struct file *filp)
{
	struct camipc_device *p_camipc = NULL;
	int ret = 0;

	p_camipc = container_of(pnode->i_cdev, struct camipc_device, m_cdev);
	if (filp != NULL) {
		filp->private_data = p_camipc;
	} else {
		loge("An object of filp is NULL\n");
		ret = -ENODEV;
	}

	return ret;
}

const struct file_operations camipc_fops = {
	.owner = THIS_MODULE,
	.open = camipc_open,
	.unlocked_ioctl = camipc_ioctl,
};

static int camipc_remove(struct platform_device *p_pdev)
{
	struct camipc_device *p_camipc = NULL;
	int ret = 0;

	if (p_pdev == NULL) {
		loge("pdev is NULL");
	} else {
		p_camipc = platform_get_drvdata(p_pdev);
		if (p_camipc != NULL) {
			device_destroy(p_camipc->m_class, p_camipc->m_devt);
			class_destroy(p_camipc->m_class);
			cdev_del(&p_camipc->m_cdev);
			unregister_chrdev_region(p_camipc->m_devt, 1);
		} else {
			loge("camipc struct is NULL");
			ret = -1;
		}
	}

	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id camipc_of_match[] = {
	{
		.compatible = "telechips,camipc",
	},
	{},
};
MODULE_DEVICE_TABLE(of, camipc_of_match);
#endif

static struct platform_driver camipc = {
	.probe = camipc_probe,
	.remove = camipc_remove,
	.driver =
		{
			.name = "camipc",
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = camipc_of_match,
#endif
		},
};

static int __init camipc_init(void)
{
	return platform_driver_register(&camipc);
}

static void __exit camipc_exit(void)
{
	platform_driver_unregister(&camipc);
}

module_init(camipc_init);

module_exit(camipc_exit);

MODULE_AUTHOR("Telechips Inc.");

MODULE_DESCRIPTION("CAMIPC Manager Driver");

MODULE_LICENSE("GPL");