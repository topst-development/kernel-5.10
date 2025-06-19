// SPDX-License-Identifier: GPL-2.0-or-later
/*******************************************************************************
 *
 * Copyright (C) 2023 Telechips Inc.
 *
 ******************************************************************************/

#include "camipc_ipi_protocol.h"

static struct camipc_device *camipc_dev;
static struct class *camipc_cls;

extern wait_queue_head_t receive_waitq;
extern int receive_flag;

extern struct file_operations camipc_fops;

static int camipc_check_rx_seqence(struct camipc_device *p_camipc, packet *msg)
{
	int ret = 0;
	uint32_t tx_seq;
	uint32_t rx_seq;

	if (msg == NULL) {
		ret = -1;
		loge("message is NULL(%d)\n", ret);
	} else {
		tx_seq = (uint32_t)(msg->cmd[0]);
		rx_seq = (uint32_t)(atomic_read(&p_camipc->rx.seq));

		if (rx_seq <= tx_seq) {
			atomic_set(&p_camipc->rx.seq, tx_seq);
		} else {
			/* ignore message */
			ret = 1;
		}
	}

	return ret;
}

static void camipc_cmd_ovp_handle(struct camipc_device *p_camipc, packet *msg)
{
	/* received ACK */
	if ((msg->cmd[1] & CAMIPC_ACK) != 0) {
		receive_flag = 1;
		wake_up_interruptible(&receive_waitq);
	} else {
		mutex_lock(&p_camipc->rx.lock);
		camipc_set_ovp(msg);
		mutex_unlock(&p_camipc->rx.lock);
		camipc_generate_meessage(p_camipc, msg, CAMIPC_CMD_OVP,
					 CAMIPC_ACK, NULL, 0);
		camipc_send_message(p_camipc, msg);
	}
}

static void camipc_cmd_ready_handle(struct camipc_device *p_camipc, packet *msg)
{
	atomic_set(&p_camipc->status, CAMIPC_STS_READY);

	/* send ACK */
	if ((msg->cmd[1] & CAMIPC_ACK) == 0) {
		camipc_generate_meessage(p_camipc, msg, CAMIPC_CMD_READY,
					 CAMIPC_ACK, NULL, 0);
		camipc_send_message(p_camipc, msg);
	}
}

static void camipc_cmd_status_handle(struct camipc_device *p_camipc, packet *msg)
{
	return;
}

int camipc_receive_message(struct tcc_ipi_msg *p_msg,
			   struct platform_device *p_pdev)
{
	struct camipc_device *p_camipc = platform_get_drvdata(p_pdev);
	uint32_t command = (uint32_t)((p_msg->cmd[1] >> 16) & 0xFFFFU);
	int ret = 0;

	ret = camipc_check_rx_seqence(p_camipc, p_msg);

	if (ret == 0) {
		switch (command) {
		case CAMIPC_CMD_OVP:
			camipc_cmd_ovp_handle(p_camipc, p_msg);
			break;
		case CAMIPC_CMD_POS:
			break;
		case CAMIPC_CMD_RESET:
			break;
		case CAMIPC_CMD_READY:
			camipc_cmd_ready_handle(p_camipc, p_msg);
			break;
		case CAMIPC_CMD_STATUS:
			camipc_cmd_status_handle(p_camipc, p_msg);
			break;
		default:
			logi("Invalid command(%d)\n", command);
			break;
		}
	}
	return 0;
}
EXPORT_SYMBOL(camipc_receive_message);

int camipc_send_message(struct camipc_device *p_camipc, packet *msg)
{
	int ret = 0;

	if (msg == NULL) {
		ret = -1;
		loge("message is NULL(%d)\n", ret);
	} else {
		if (p_camipc != NULL) {
			ret = tcc_ipi_send_data(p_camipc->ipi_protocol_client,
						msg,
						p_camipc->ipi_protocol_index);
			atomic_inc(&p_camipc->tx.seq);
		} else {
			ret = -1;
			loge("camipc device is NULL(%d)\n", ret);
		}
	}

	return ret;
}
EXPORT_SYMBOL(camipc_send_message);

int camipc_generate_meessage(struct camipc_device *p_camipc, packet *msg,
			     int ctrl_cmd, int cmd_sts, int *args, int num_args)
{
	int ret = 0;
	int arg_idx;

	if (msg == NULL) {
		ret = -1;
		loge("Fail camipc_generate_meessage(%d)\n", ret)
	} else {
		msg->cmd_len = MBOX_MSG_CMD_MAX_LEN;
		msg->cmd[0] = (uint32_t)atomic_read(&p_camipc->tx.seq);
		msg->cmd[1] = (((uint32_t)ctrl_cmd & 0xFFFFU) << 16U) |
			      (uint32_t)cmd_sts;
		if (args == NULL) {
			msg->data_len = 0;
		} else {
			for (arg_idx = 0; arg_idx < num_args; arg_idx++) {
				msg->data_buf[arg_idx] = args[arg_idx];
			}
			msg->data_len = num_args;
		}
	}

	return ret;
}
EXPORT_SYMBOL(camipc_generate_meessage);

int camipc_request_channel(struct camipc_device *p_camipc, const char *name)
{
	int ret = 0;
	int32_t ipi_protocol_index;
	struct tcc_ipi_register_dat ipi_protocol_prot_dat = {
		0,
	};

	memcpy(&(ipi_protocol_prot_dat.id[0]), name, strlen(name));
	ipi_protocol_prot_dat.rx_callback = camipc_receive_message;
	ipi_protocol_prot_dat.pdev = p_camipc->m_pdev;
	ipi_protocol_index = tcc_ipi_register(p_camipc->ipi_protocol_client,
					      &ipi_protocol_prot_dat);
	if (ipi_protocol_index < 0) {
		ret = -1;
	} else {
		p_camipc->ipi_protocol_index = ipi_protocol_index;
	}

	return ret;
}
EXPORT_SYMBOL(camipc_request_channel);

static int camipc_probe_f_init(struct platform_device *p_pdev)
{
	struct camipc_device *p_camipc = NULL;
	int ret = 0;

	if (p_pdev == NULL) {
		loge("pdev is NULL");
		ret = -ENODEV;
	} else {
		p_camipc = devm_kzalloc(
			&p_pdev->dev, sizeof(struct camipc_device), GFP_KERNEL);

		if (p_camipc == NULL) {
			ret = -ENOMEM;
		} else {
			platform_set_drvdata(p_pdev, p_camipc);
		}
	}

	return ret;
}

static int camipc_probe_f_parse_dt(struct platform_device *p_pdev)
{
	struct camipc_device *p_camipc = platform_get_drvdata(p_pdev);

	of_property_read_string(p_pdev->dev.of_node, "device-name",
				&p_camipc->device_name);
	of_property_read_string(p_pdev->dev.of_node, "protocol",
				&p_camipc->protocol);
	p_camipc->ipi_protocol_client =
		of_parse_phandle(p_pdev->dev.of_node, "tcc_ipi", 0); //todo

	return 0;
}

static int camipc_probe_f_init_cdev(struct platform_device *p_pdev)
{
	struct camipc_device *p_camipc = platform_get_drvdata(p_pdev);
	int ret;

	ret = alloc_chrdev_region(&p_camipc->m_devt, CAMIPC_DEV_MINOR, 1,
				  p_camipc->device_name);

	if (ret > 0) {
		loge("Fail alloc_chrdev_region(%d)\n", ret);
	} else {
		cdev_init(&p_camipc->m_cdev, &camipc_fops);
		p_camipc->m_cdev.owner = THIS_MODULE;
		ret = cdev_add(&p_camipc->m_cdev, p_camipc->m_devt, 1);
		if (ret > 0) {
			loge("Fail cdev_add(%d)\n", ret);
		}
	}

	return ret;
}

static int camipc_probe_f_init_class(struct platform_device *p_pdev)
{
	struct camipc_device *p_camipc = platform_get_drvdata(p_pdev);

	int ret = 0;

	if (camipc_cls == NULL) {
		camipc_cls = class_create(THIS_MODULE, "camipc");
		if (IS_ERR(camipc_cls)) {
			ret = (int)PTR_ERR(camipc_cls);
			loge("Fail class_create(%d)\n", ret);
		}
	}
	p_camipc->m_class = camipc_cls;

	return ret;
}

static int camipc_probe_f_init_device(struct platform_device *p_pdev)
{
	struct camipc_device *p_camipc = platform_get_drvdata(p_pdev);

	int ret = 0;

	p_camipc->m_dev =
		device_create(p_camipc->m_class, &p_pdev->dev, p_camipc->m_devt,
			      NULL, p_camipc->device_name);
	if (IS_ERR(p_camipc->m_dev)) {
		ret = (int)PTR_ERR(p_camipc->m_dev);
		loge("Fail device_create(%d)\n", ret);
	} else {
		p_camipc->m_pdev = p_pdev;
	}

	return ret;
}

static int camipc_probe_f_init_txrx(struct platform_device *p_pdev)
{
	struct camipc_device *p_camipc = platform_get_drvdata(p_pdev);
	int ret;

	atomic_set(&p_camipc->status, CAMIPC_STS_INIT);

	mutex_init(&p_camipc->rx.lock);
	atomic_set(&p_camipc->rx.seq, 0);

	mutex_init(&p_camipc->tx.lock);
	atomic_set(&p_camipc->tx.seq, 0);

	receive_flag = 0;
	init_waitqueue_head(&receive_waitq);

	ret = camipc_request_channel(p_camipc, p_camipc->protocol);

	if (ret < 0) {
		loge("Fail camipc_rx_init\n");
		ret = -EFAULT;
	} else {
		camipc_dev = p_camipc;
		ret = 0;
	}

	return ret;
}

static int (*camipc_probe_func[CAMIPC_PROBE_MAX])(
	struct platform_device *p_pdev) = { camipc_probe_f_init,
					    camipc_probe_f_parse_dt,
					    camipc_probe_f_init_cdev,
					    camipc_probe_f_init_class,
					    camipc_probe_f_init_device,
					    camipc_probe_f_init_txrx };

int camipc_probe(struct platform_device *p_pdev)
{
	struct camipc_device *p_camipc;
	packet msg = {
		0,
	};
	uint32_t msg_cmd[6] = {
		0,
	};
	int ret = 0;
	int msg_ret = 0;
	int step;

	msg.cmd = msg_cmd;
	msg.data_buf = NULL;

	for (step = CAMIPC_PROBE_INIT_OBJ; step < CAMIPC_PROBE_MAX; step++) {
		ret = camipc_probe_func[step](p_pdev);
		if (ret < 0) {
			break;
		}
	}

	if (ret == 0) {
		p_camipc = platform_get_drvdata(p_pdev);
		camipc_generate_meessage(p_camipc, &msg, CAMIPC_CMD_READY,
					 CAMIPC_SEND, NULL, 0);
		msg_ret = camipc_send_message(p_camipc, &msg);
		if (msg_ret < 0) {
			loge("Fail camipc_send_message\n");
		} else {
			logi("CAMIPC Driver Initialized\n");
		}
	} else {
		loge("Fail initialize camipc driver in %d\n", step);
	}

	return ret;
}

void camipc_parsing_set_ovp(struct camipc_device *p_camipc, packet *msg,
			    unsigned long arg, size_t data_size)

{
	struct camipc_ovp_data ovp_data;
	int args[2] = {
		0,
	};
	unsigned long ret = 0;

	ret = copy_from_user((void *)(&ovp_data), (void *)arg, data_size);

	if (ret > 0UL) {
		loge("Failed to copy_from_user");
	} else {
		args[0] = ovp_data.wmix_ch;
		args[1] = ovp_data.ovp;
		camipc_generate_meessage(p_camipc, msg, CAMIPC_CMD_OVP,
					 CAMIPC_SEND, args, 2);
	}
}

int camipc_ioctl_handle(struct camipc_device *p_camipc, int cmd,
			unsigned long arg)
{
	int ret = 0;
	packet msg = {
		0,
	};
	uint32_t msg_cmd[6] = {
		0,
	};
	uint32_t msg_data[4] = {
		0,
	};

	msg.cmd = msg_cmd;
	msg.data_buf = msg_data;

	mutex_lock(&p_camipc->tx.lock);

	switch (cmd) {
	case (unsigned int)IOCTL_CAMIPC_SET_OVP:
		camipc_parsing_set_ovp(p_camipc, &msg, arg,
				       sizeof(struct camipc_ovp_data));
		break;
	case (unsigned int)IOCTL_CAMIPC_SET_POS:
		break;
	case (unsigned int)IOCTL_CAMIPC_SET_RESET:
		break;
	default:
		loge("Invalid command (%d)\n", cmd);
		ret = -EINVAL;
		break;
	}
	camipc_send_and_check_timeout(p_camipc, &msg);

	mutex_unlock(&p_camipc->tx.lock);

	return ret;
}

/* Function: camipc_set_ovp
 * Description: Set the layer-order of WMIXx block
 * data[0]: WMIX Block Number
 * data[1]: ovp (Please refer to the full spec)
 */
void camipc_set_ovp(const packet *msg)
{
	uint32_t wmix_blk_num = 0;
	uint32_t ovp = 0;

	if (msg != NULL) {
		wmix_blk_num = msg->data_buf[0];
		ovp = msg->data_buf[1];
		VIOC_WMIX_SetOverlayPriority(VIOC_WMIX_GetAddress(wmix_blk_num),
					     ovp);
		VIOC_WMIX_SetUpdate(VIOC_WMIX_GetAddress(wmix_blk_num));
	} else {
		loge("mailbox message is NULL\n");
	}
}

/* Function : camipc_set_pos
 * Description : Set the position of WMIXx block
 * data[0] : WMIX block number
 * data[1] : the input channel
 * data[2] : x-position
 * data[3] : y-position
 */
void camipc_set_pos(const packet *msg)
{
	uint32_t wmix_blk_num = 0;
	uint32_t input_channel = 0;
	uint32_t x_pos = 0;
	uint32_t y_pos = 0;

	if (msg != NULL) {
		wmix_blk_num = msg->data_buf[0];
		input_channel = msg->data_buf[1];
		x_pos = msg->data_buf[2];
		y_pos = msg->data_buf[3];
		VIOC_WMIX_SetPosition(VIOC_WMIX_GetAddress(wmix_blk_num),
				      input_channel, x_pos, y_pos);
		VIOC_WMIX_SetUpdate(VIOC_WMIX_GetAddress(wmix_blk_num));

	} else {
		loge("mailbox message is NULL\n");
	}
}

/* Function : camipc_set_reset
 * Description : VIOC Block SWReset
 * data[0] : VIOC Block Number
 * data[1] : mode (0: clear, 1: reset)
 */
void camipc_set_reset(const packet *msg)
{
	uint32_t wmix_blk_num = 0;
	uint32_t mode = 0;

	if (msg != NULL) {
		wmix_blk_num = msg->data_buf[0];
		mode = msg->data_buf[1];
		VIOC_CONFIG_SWReset(wmix_blk_num, mode);
	} else {
		loge("mailbox message is NULL\n");
	}
}