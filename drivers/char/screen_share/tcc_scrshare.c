// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/platform_device.h>

#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>

#include <video/telechips/tcc_screen_share.h>
#include <video/telechips/vioc_config.h>
#include <video/telechips/tcc_types.h>

#include <tcc_scrshare_ipc.h>
#include <tcc_scrshare_overlay.h>
#include <tcc_shared_buffer.h>

#define release_date ("r240311")

struct tcc_scrshare_flag {
	struct mutex lock;
	atomic_t seq;
};

struct tcc_scrshare_device {
	struct platform_device *pdev;
	struct device *dev;
	struct cdev scdev;
	struct class *pclass;
	dev_t devt;
	const char *name;

	scrshare_ipc_t ipc;

	atomic_t status;

	wait_queue_head_t ipc_process_waitq;
	int ipc_process_done;

	struct tcc_scrshare_overlay overlay;
	unsigned int screen_mode;

	struct tcc_scrshare_flag tx;
	struct tcc_scrshare_flag rx;

	struct tcc_scrshare_info *info;
};

static struct tcc_scrshare_device *stcc_scrshare_device;

static void tcc_scrshare_on(
struct tcc_scrshare_device *dev) {

	struct tcc_scrshare_info *info = dev->info;

	info->share_enable = 1u;

	if((bool)dev->screen_mode) {
		tcc_scrshare_overlay_open(&dev->overlay);
	}
}

static void tcc_scrshare_off(
struct tcc_scrshare_device *dev) {

	struct tcc_scrshare_info *info = dev->info;

	info->share_enable = 0u;

	if((bool)dev->screen_mode) {
		tcc_scrshare_overlay_release(&dev->overlay);
	}
}

static void tcc_scrshare_get_dstinfo(
const struct tcc_scrshare_device *dev, scrshare_msg_data_t *msg_) {

	const struct tcc_scrshare_info *info = dev->info;

	msg_->data[0] = info->dstinfo->x;
	msg_->data[1] = info->dstinfo->y;
	msg_->data[2] = info->dstinfo->width;
	msg_->data[3] = info->dstinfo->height;
	msg_->data[4] = info->dstinfo->img_num;
	msg_->data_len = 5u;
	(void)pr_info("%s: x:%u, y:%u, w:%u, h:%u, img_num:%u\n",
		 __func__, msg_->data[0], msg_->data[1], msg_->data[2],
		 msg_->data[3], msg_->data[4]);
}

static void tcc_scrshare_parse_overlay_cfg(
shared_buffer_cfg_t *cfg, const struct tcc_scrshare_info *info,
const scrshare_msg_data_t *msg_) {

	cfg->src_addr = msg_->data[0];
	cfg->frm_w = msg_->data[1];
	cfg->frm_h = msg_->data[2];
	cfg->fmt = msg_->data[3];
	cfg->rgb_swap = msg_->data[4];
	cfg->dst_x = info->dstinfo->x;
	cfg->dst_y = info->dstinfo->y;
	cfg->dst_w = info->dstinfo->width;
	cfg->dst_h = info->dstinfo->height;
}

static void tcc_scrshare_display(
const struct tcc_scrshare_device *dev, const scrshare_msg_data_t *msg_) {

	const struct tcc_scrshare_info *info = dev->info;

	if ((dev->screen_mode == 1u) && (info->share_enable == 1u)) {

		shared_buffer_cfg_t cfg;

		tcc_scrshare_parse_overlay_cfg(&cfg, info, msg_);

		if(tcc_scrshare_overlay_display(&dev->overlay, cfg) < 0) {
			(void)pr_err("%s: failed to push buffer\n",__func__);
		}

	}
}

static void tcc_scrshare_send_message(
const struct tcc_scrshare_device *dev,
scrshare_msg_data_t *mssg) {

	if (dev != NULL) {
#if defined(USE_TCC_IPC_SYSTEM)
		tcc_ipc_msg_t *msg_ = (tcc_ipc_msg_t *)mssg;

		int ret = tcc_ipc_send_data(dev->ipc.np, msg_, dev->ipc.idx);
		if (ret < 0) {
			(void)pr_err("screen share mbox send error(%d)\n", ret);
		}
#else
		struct tcc_mbox_data *msg_ = (struct tcc_mbox_data *)mssg;

		int ret = mbox_send_message(dev->ipc.mbox_ch, msg_);
		mbox_client_txdone(dev->ipc.mbox_ch, ret);
#endif
	}
}

static int tcc_scrshare_store_seq_id(
atomic_t *seq, unsigned int data) {

	int ret = 0;

	if (data < (~0U>>1)) {
		atomic_set(seq, (int)data);
	} else {
		ret = -1;
	}

	return ret;
}

static int tcc_scrshare_increase_seq_id(
atomic_t *seq, unsigned int *data) {

	int ret = 0;
	int value = 0;

	atomic_inc(seq);

	value = atomic_read(seq);
	if(value < 0) {
		(void)pr_err("%s: atomic_read err(%d)\n",__func__, value);
		ret = -1;
	}
	else {
		*data = (unsigned int)value;
	}

	return ret;
}

static int tcc_scrshare_wait_ipc_process(struct tcc_scrshare_device *dev) {

	unsigned long timeout_ms = msecs_to_jiffies(100);
	long ret;

	if(timeout_ms > (~0UL>>1)) {
		timeout_ms = ~0UL>>1;
	}

	ret = wait_event_interruptible_timeout(dev->ipc_process_waitq,
			(dev->ipc_process_done == 1), (long)timeout_ms);

	return (ret < 0) ? -EPERM : (ret == 0) ? 0 : 1;
}

static int tcc_scrshare_send_msg_process(
struct tcc_scrshare_device *dev, scrshare_msg_data_t *data) {

	int ret = 0;

	ret = tcc_scrshare_increase_seq_id(&dev->tx.seq, &data->cmd[0]);
	if (ret < 0) {
		(void)pr_err("%s: fail to increase sequence id!!",__func__);
	}
	else {
		/* Initialize tx-wakup condition */
		dev->ipc_process_done = 0;

		tcc_scrshare_send_message(dev, data);
		ret = tcc_scrshare_wait_ipc_process(dev);
		if(ret <= 0) {
			(void)pr_err("%s: Timeout send_message(%d)(%d)\n",__func__,
				ret, dev->ipc_process_done);
			ret = -1;
		}
		dev->ipc_process_done = 0;
	}

	return ret;
}

static void tcc_scrshare_alloc_msg(
const struct tcc_scrshare_device *dev, scrshare_msg_data_t *data,
unsigned int data_len) {

	(void)memset(data, 0x0, sizeof(scrshare_msg_data_t));

#if defined(USE_TCC_IPC_SYSTEM)
	data->cmd_len = MBOX_MSG_CMD_MAX_LEN;
	data->cmd = devm_kzalloc(dev->dev, sizeof(u32)*data->cmd_len, GFP_KERNEL);
	if(data_len > 0u) {
		data->data_len = data_len;
		data->data = devm_kzalloc(dev->dev, sizeof(u32)*data->data_len, GFP_KERNEL);
	}
	else {
		data->data_len = 0u;
	}
#else
	(void)dev;
	data->data_len = data_len;
#endif
}

static void tcc_scrshare_free_msg(
const struct tcc_scrshare_device *dev, const scrshare_msg_data_t *data) {
#if defined(USE_TCC_IPC_SYSTEM)
	if (data->cmd != NULL) {
		devm_kfree(dev->dev, data->cmd);
	}
	if (data->data != NULL) {
		devm_kfree(dev->dev, data->data);
	}
#else
	(void)dev;
	(void)data;
#endif
}

#define parse_cmd(cmd) ((((unsigned int)(cmd)) & 0xffffu) << 16u)
static int tcc_scrshare_connect_check(
struct tcc_scrshare_device *dev) {

	int ret = 0;

	if (atomic_read(&dev->status) != SCRSHARE_STS_READY) {

		scrshare_msg_data_t data;

		mutex_lock(&dev->tx.lock);

		tcc_scrshare_alloc_msg(dev, &data, 0u);

		data.cmd[1] = parse_cmd(SCRSHARE_CMD_READY) | TCC_SCRSHARE_SEND;
		ret = tcc_scrshare_send_msg_process(dev, &data);
		if(ret < 0) {
			(void)pr_err("%s: fail to send message\n",__func__);
		}
		tcc_scrshare_free_msg(dev, &data);

		mutex_unlock(&dev->tx.lock);
	}

	return ret;
}

static int tcc_scrshare_check_seq_id(
const struct tcc_scrshare_device *dev, unsigned int seq_id) {

	int ret = 0;
	int value = atomic_read(&dev->rx.seq);

	if(value >= 0) {

		unsigned int value_u = (unsigned int)value;

		if(value_u > seq_id) {
			(void)pr_err("%s: stored seq id(%u), but requested seq id(%u)\n",
				__func__, value_u, seq_id);
			ret = -1;
		}
	}
	else {
		(void)pr_err("%s: atomic_read err(%d)\n",__func__, value);
		ret = -1;
	}

	return ret;
}

static int tcc_scrshare_rx_cmd_process(
struct tcc_scrshare_device *dev, unsigned int cmd, scrshare_msg_data_t *data) {

	int ret = 0;

	switch (cmd) {
	case SCRSHARE_CMD_GET_DSTINFO:
		tcc_scrshare_get_dstinfo(dev, data);
		break;
	case SCRSHARE_CMD_SET_SRCINFO:
		tcc_scrshare_display(dev, data);
		break;
	case SCRSHARE_CMD_ON:
		tcc_scrshare_on(dev);
		break;
	case SCRSHARE_CMD_OFF:
		tcc_scrshare_off(dev);
		break;
	default:
		(void)pr_warn("err in %s: Invalid command(%u)\n",__func__, cmd);
		ret = -1;
		break;
	}

	return ret;
}

static void tcc_scrshare_rx_cmd_handler(
struct tcc_scrshare_device *dev, unsigned int cmd, scrshare_msg_data_t *data) {

	int ret = tcc_scrshare_check_seq_id(dev, data->cmd[0]);

	if(ret < 0) {
		(void)pr_err("%s: can`t process the command(%u)",__func__, cmd);
	}
	else {
		ret = tcc_scrshare_rx_cmd_process(dev, cmd, data);
		if ((ret == 0) && (data->cmd[0] != 0u)) {
			/* Update rx-sequence ID */
			data->cmd[1] |= TCC_SCRSHARE_ACK;
			tcc_scrshare_send_message(dev, data);
			ret = tcc_scrshare_store_seq_id(&dev->rx.seq, data->cmd[0]);
			if(ret < 0) {
				(void)pr_err("%s: data->cmd[0] err(%u)\n",__func__, data->cmd[0]);
			}
		}
	}

	return;
}

static void tcc_scrshare_irq_wake_up_signal(
struct tcc_scrshare_device *dev) {
	dev->ipc_process_done = 1;
	wake_up_interruptible(&dev->ipc_process_waitq);
}

static void tcc_scrshare_rx_ack_process(
struct tcc_scrshare_device *dev, unsigned int cmd, const scrshare_msg_data_t *msg_) {

	struct tcc_scrshare_info *info = dev->info;

	switch(cmd) {
	case SCRSHARE_CMD_GET_DSTINFO:
		(void)pr_info("%s: x:%u, y:%u, w:%u, h:%u\n", __func__,
			msg_->data[0],msg_->data[1], msg_->data[2], msg_->data[3]);
		info->dstinfo->x = msg_->data[0];
		info->dstinfo->y = msg_->data[1];
		info->dstinfo->width = msg_->data[2];
		info->dstinfo->height = msg_->data[3];
		info->dstinfo->img_num = msg_->data[4];
		break;
	case SCRSHARE_CMD_ON:
		(void)pr_info("%s: SCRSHARE_CMD_ON ok\n",__func__);
		info->share_enable = 1u;
		break;
	case SCRSHARE_CMD_OFF:
		(void)pr_info("%s: SCRSHARE_CMD_OFF ok\n",__func__);
		info->share_enable = 0u;
		break;
	default:
		// misra_c_2012_rule_16_4_violation:
		// The switch statement does not have a non-empty default clause
		break;
	}
	tcc_scrshare_irq_wake_up_signal(dev);
}

static void tcc_scrshare_rx_cmd_ready(
struct tcc_scrshare_device *dev, scrshare_msg_data_t *msg_) {

	if(atomic_read(&dev->status) == SCRSHARE_STS_INIT) {
		atomic_set(&dev->status, SCRSHARE_STS_READY);
	}

	(void)tcc_scrshare_store_seq_id(&dev->rx.seq, msg_->cmd[0]);

	if((msg_->cmd[1] & TCC_SCRSHARE_ACK) == 0u) {
		msg_->cmd[1] |= TCC_SCRSHARE_ACK;
		tcc_scrshare_send_message(dev, msg_);
	}
	else {
		tcc_scrshare_irq_wake_up_signal(dev);
	}
}

static void tcc_scrshare_rx_msg_process(
struct tcc_scrshare_device *dev, unsigned int cmd, scrshare_msg_data_t *msg_) {

	if ((msg_->cmd[1] & TCC_SCRSHARE_ACK) != 0u) {
		tcc_scrshare_rx_ack_process(dev, cmd, msg_);
	}
	else {
		mutex_lock(&dev->rx.lock);
		tcc_scrshare_rx_cmd_handler(dev, cmd, msg_);
		mutex_unlock(&dev->rx.lock);
	}
}

static void tcc_scrshare_rx_msg_handler(
struct tcc_scrshare_device *dev, scrshare_msg_data_t *msg_) {

	unsigned int cmd = ((msg_->cmd[1] >> 16u) & 0xFFFFu);

	switch (cmd) {
	case SCRSHARE_CMD_GET_DSTINFO:
	case SCRSHARE_CMD_SET_SRCINFO:
	case SCRSHARE_CMD_ON:
	case SCRSHARE_CMD_OFF:
		if (atomic_read(&dev->status) == SCRSHARE_STS_READY) {
			tcc_scrshare_rx_msg_process(dev,cmd,msg_);
		}
		break;
	case SCRSHARE_CMD_READY:
		tcc_scrshare_rx_cmd_ready(dev, msg_);
		break;
	case SCRSHARE_CMD_NULL:
		dev->info->share_enable = 0u;
		atomic_set(&dev->status, SCRSHARE_STS_INIT);
		break;
	case SCRSHARE_CMD_MAX:
	default:
		(void)pr_warn("%s: Invalid command(%u)\n", __func__, cmd);
		break;
	}

	return;
};

static void tcc_scrshare_rx_msg_to_handler(
struct tcc_scrshare_device *dev, scrshare_msg_data_t *msg_) {

	if (msg_ == NULL) {
		(void)pr_err("%s: screen share message ptr is null\n", __func__);
	}
	else if (dev == NULL) {
		(void)pr_err("%s: screen share device ptr is null\n", __func__);
	}
	else {
		tcc_scrshare_rx_msg_handler(dev, msg_);
	}
}

#if defined(USE_TCC_IPC_SYSTEM)
static int tcc_scrshare_receive_message(
tcc_ipc_msg_t *client, struct platform_device *pddev) {

	scrshare_msg_data_t *msg_ = (scrshare_msg_data_t *)client;
	struct tcc_scrshare_device *dev = platform_get_drvdata(pddev);

	tcc_scrshare_rx_msg_to_handler(dev, msg_);

	return 0;
}
#else
static void tcc_scrshare_receive_message(
struct mbox_client *client, void *mssg) {

	scrshare_msg_data_t *msg_ = (scrshare_msg_data_t *)mssg;
	struct tcc_scrshare_device *dev =
		container_of(client, struct tcc_scrshare_device, ipc.cl);

	tcc_scrshare_rx_msg_to_handler(dev, msg_);
}
#endif

enum {
	ioctl_set_destination_info = IOCTL_TCC_SCRSHARE_SET_DSTINFO,
	ioctl_get_destination_info = IOCTL_TCC_SCRSHARE_GET_DSTINFO,
	ioctl_set_source_info = IOCTL_TCC_SCRSHARE_SET_SRCINFO,
	ioctl_screen_on = IOCTL_TCC_SCRSHARE_ON,
	ioctl_screen_off = IOCTL_TCC_SCRSHARE_OFF,
};

#define check_cmd_user_info(cmd) \
	(((cmd) == (unsigned)ioctl_set_destination_info) || \
	((cmd) == (unsigned)ioctl_set_source_info))
static int tcc_scrshare_set_user_info(
struct tcc_scrshare_device *dev, unsigned int cmd, const void __user *arg) {

	const struct tcc_scrshare_info *info = dev->info;
	int ret = 0;

	mutex_lock(&dev->tx.lock);

	if(cmd == (unsigned)ioctl_set_destination_info) {
		if((bool)copy_from_user(info->dstinfo, arg,
			(unsigned long)sizeof(struct tcc_scrshare_dstinfo))) {
			(void)pr_err("%s: unable to copy the dst info\n", __func__);
			ret = -EINVAL;
		}
		else {
			(void)pr_info("%s: SET_DSTINFO x:%u, y:%u, w:%u, h:%u, img_num:%u\n",__func__,
				info->dstinfo->x, info->dstinfo->y, info->dstinfo->width,
				info->dstinfo->height, info->dstinfo->img_num);
		}
	}
	else if(cmd == (unsigned)ioctl_set_source_info) {
		if((bool)copy_from_user(info->srcinfo, arg,
			(unsigned long)sizeof(struct tcc_scrshare_srcinfo))) {
			(void)pr_err("%s: unable to copy the src info\n", __func__);
			ret = -EINVAL;
		}
		else {
			(void)pr_info("%s: SET_SRCINFO x:%u, y:%u, w:%u, h:%u\n",__func__,
				info->srcinfo->x, info->srcinfo->y, info->srcinfo->width,
				info->srcinfo->height);
		}
	}
	else {
		// misra_c_2012_rule_15_7_violation:
		// No non-empty terminating "else" statement
	}

	mutex_unlock(&dev->tx.lock);

	return ret;
}

static int tcc_scrshare_send_ioctl_command(
struct tcc_scrshare_device *dev, unsigned int cmd) {

	int ret = 0;
	scrshare_msg_data_t data;

	tcc_scrshare_alloc_msg(dev, &data, 0u);

	switch (cmd) {
	case ioctl_get_destination_info:
		data.cmd[1] = parse_cmd(SCRSHARE_CMD_GET_DSTINFO);
		break;
	case ioctl_screen_on:
		data.cmd[1] = parse_cmd(SCRSHARE_CMD_ON);
		break;
	case ioctl_screen_off:
		data.cmd[1] = parse_cmd(SCRSHARE_CMD_OFF);
		break;
	default:
		// misra_c_2012_rule_16_4_violation:
		// The switch statement does not have a non-empty default clause
		break;
	}

	ret = tcc_scrshare_send_msg_process(dev, &data);
	if(ret < 0) {
		(void)pr_err("%s: fail to send message\n",__func__);
	}

	tcc_scrshare_free_msg(dev, &data);

	return ret;
}

static long tcc_scrshare_ioctl(
struct file *filp, unsigned int cmd, unsigned long arg) {

	struct tcc_scrshare_device *dev = filp->private_data;
	void __user *user_arg = (void __user *)arg;
	long ret = 0;

	if(dev == NULL) {
		(void)pr_err("%s: screen share device ptr is null\n",__func__);
		ret = -ENODEV;
	}
	else if(check_cmd_user_info(cmd)) {
		ret = tcc_scrshare_set_user_info(dev, cmd, user_arg);
	}
	else if(tcc_scrshare_connect_check(dev) < 0) {
		(void)pr_err("%s: opposite is not ready. Cannot use screen sharing.\n", __func__);
		ret = -EINVAL;
	}
	else {
		const struct tcc_scrshare_info *info = dev->info;

		mutex_lock(&dev->tx.lock);

		switch (cmd) {
		case ioctl_get_destination_info:
		case ioctl_screen_on:
		case ioctl_screen_off:
			ret = tcc_scrshare_send_ioctl_command(dev, cmd);
			if ((ret == 0) && (cmd == (unsigned)ioctl_get_destination_info)) {
				if((bool)copy_to_user(user_arg, info->dstinfo,
					(long)sizeof(struct tcc_scrshare_dstinfo))) {
					(void)pr_err("%s: copy to user fail\n",__func__);
					ret = -EINVAL;
				}
			}
			break;
		default:
			(void)pr_warn("%s: Invalid command (%d)\n", __func__, cmd);
			ret = -EINVAL;
			break;
		}

		mutex_unlock(&dev->tx.lock);
	}

	return ret;
}

static void tcc_scrshare_parse_src_info(
struct tcc_scrshare_info *info, unsigned int addr,
unsigned int frameWidth, unsigned int frameHeight, unsigned int fmt) {

	unsigned int base0 = 0u, base1 = 0u, base2 =0u;
/*
	(void)pr_info("%s: addr:0x%08x, w:%u, h:%u, fmt:0x%x\n",
		__func__, addr, frameWidth, frameHeight, fmt);
*/
	tcc_get_addr_yuv(fmt, addr, frameWidth, frameHeight,
		info->srcinfo->x, info->srcinfo->y, &base0, &base1, &base2);

	info->src_addr = base0;
	info->frm_w = frameWidth;
	info->frm_h = frameHeight;
	info->fmt = fmt;
}

static int tcc_scrshare_send_buffer(
struct tcc_scrshare_device *dev, unsigned int rgb_swap) {

	const struct tcc_scrshare_info *info = dev->info;
	scrshare_msg_data_t data;
	int ret = 0;

	mutex_lock(&dev->tx.lock);

	tcc_scrshare_alloc_msg(dev, &data, 5u);

	data.cmd[1] = parse_cmd(SCRSHARE_CMD_SET_SRCINFO);
	data.data[0] = info->src_addr;
	data.data[1] = info->frm_w;
	data.data[2] = info->frm_h;
	data.data[3] = info->fmt;
	data.data[4] = rgb_swap;

	ret = tcc_scrshare_send_msg_process(dev, &data);
	if(ret < 0) {
		(void)pr_err("%s: fail to send message\n", __func__);
	}

	tcc_scrshare_free_msg(dev, &data);

	mutex_unlock(&dev->tx.lock);

	return ret;
}

void tcc_scrshare_set_sharedBuffer(
unsigned int addr, unsigned int frameWidth, unsigned int frameHeight,
unsigned int fmt, unsigned int rgb_swap) {

	struct tcc_scrshare_device *dev = stcc_scrshare_device;

	if (dev == NULL) {
		(void)pr_err("%s: screen share device ptr is null\n",__func__);
	}
	else if(dev->info == NULL) {
		(void)pr_err("%s: screen share info ptr is null\n",__func__);
	}
	else if((addr == 0u) || (frameWidth == 0u) || (frameHeight == 0u)) {
		(void)pr_err("%s: Invalid args addr:0x%08x, w:%u, h:%u, fmt:0x%x, "
			"rgb_swap:0x%x\n", __func__, addr, frameWidth, frameHeight,
			fmt, rgb_swap);
	}
	else if(dev->info->share_enable == 1u) {
		tcc_scrshare_parse_src_info(dev->info, addr, frameWidth, frameHeight, fmt);
		if(tcc_scrshare_send_buffer(dev, rgb_swap) < 0) {
			(void)pr_err("%s: sub-core seems to have some problem\n", __func__);
			tcc_scrshare_off(dev);
		}
	}
	else {
		// misra_c_2012_rule_15_7_violation:
		// No non-empty terminating "else" statement
	}
}
EXPORT_SYMBOL(tcc_scrshare_set_sharedBuffer);

static void tcc_scrshare_send_termination(
struct tcc_scrshare_device *dev) {

	scrshare_msg_data_t data;

	mutex_lock(&dev->tx.lock);

	tcc_scrshare_alloc_msg(dev, &data, 0u);

	if((bool)dev->screen_mode) {
		data.cmd[1] = parse_cmd(SCRSHARE_CMD_NULL);
	}
	else {
		data.cmd[1] = parse_cmd(SCRSHARE_CMD_OFF);
	}

	tcc_scrshare_off(dev);

	(void)tcc_scrshare_increase_seq_id(&dev->tx.seq, &data.cmd[0]);

	tcc_scrshare_send_message(dev, &data);

	tcc_scrshare_free_msg(dev, &data);

	mutex_unlock(&dev->tx.lock);
}

static int tcc_scrshare_release(
struct inode *inode_release, struct file *filp) {

	struct tcc_scrshare_device *dev = filp->private_data;

	if(dev == NULL) {
		(void)pr_err("%s: screen share device ptr is null\n",__func__);
	}
	else {
		tcc_scrshare_send_termination(dev);
	}

	(void)inode_release;

	return 0;
}

static int tcc_scrshare_open(
struct inode *inode_open, struct file *filp) {

	struct tcc_scrshare_device *dev =
		container_of(inode_open->i_cdev, struct tcc_scrshare_device, scdev);

	if(dev == NULL) {
		(void)pr_err("%s: screen share device ptr is null\n",__func__);
	}
	else {
		filp->private_data = dev;
	}

	return 0;
}

static const struct file_operations tcc_scrshare_fops = {
	.owner = THIS_MODULE,
	.open = tcc_scrshare_open,
	.release = tcc_scrshare_release,
	.unlocked_ioctl = tcc_scrshare_ioctl,
};

#if defined(USE_TCC_IPC_SYSTEM)
static int tcc_scrshare_register_ipc(
struct tcc_scrshare_device *dev, struct platform_device *pdev) {

	int ret = 0;
	tcc_ipc_register_data_t ipc_prot_dat = {0,};
	const char prot_id[] = "SCRSHARE";

	(void)memcpy(&(ipc_prot_dat.id[0]), &prot_id, TCC_IPC_PROTOCOL_ID_MAX_LEN);
	ipc_prot_dat.rx_callback = &tcc_scrshare_receive_message;
	ipc_prot_dat.pdev = pdev;

	dev->ipc.np = of_parse_phandle(pdev->dev.of_node, IPC_NODE_NAME, 0);
	dev->ipc.idx = tcc_ipc_register(dev->ipc.np, &ipc_prot_dat);
	if (dev->ipc.idx < 0 ) {
		ret = -EPROBE_DEFER;
	}

	return ret;
}
#else
static struct mbox_chan* tcc_scrshare_request_channel(
struct tcc_scrshare_device *dev, const char *name) {

	struct mbox_chan *channel;

	dev->ipc.cl.dev = &dev->pdev->dev;
	dev->ipc.cl.rx_callback = tcc_scrshare_receive_message;
	dev->ipc.cl.tx_done = NULL;
#if defined(CONFIG_ARCH_TCC805X)
	dev->ipc.cl.tx_block = (bool)true;
	dev->ipc.cl.tx_tout = CLIENT_MBOX_TX_TIMEOUT;
#else
	dev->ipc.cl.tx_block = (bool)false;
	dev->ipc.cl.tx_tout = 0; /*  doesn't matter here */
#endif
	dev->ipc.cl.knows_txdone = (bool)false;
	channel = mbox_request_channel_byname(&dev->ipc.cl, name);
	if (IS_ERR(channel)) {
		(void)pr_err("%s: Fail mbox_request_channel_byname(%s)\n",
			 __func__, name);
		channel = NULL;
	}

	return channel;
}
#endif

static int tcc_scrshare_rx_init(
struct tcc_scrshare_device *dev) {

	mutex_init(&dev->rx.lock);

	return tcc_scrshare_store_seq_id(&dev->rx.seq, 0u);
}

static void tcc_scrshare_tx_init(
struct tcc_scrshare_device *dev) {

	mutex_init(&dev->tx.lock);
	atomic_set(&dev->tx.seq, 0);

	dev->ipc_process_done = 0;
	init_waitqueue_head(&dev->ipc_process_waitq);
}

static void tcc_scrshare_parse_node(
struct tcc_scrshare_device *dev, const struct platform_device *pdev) {

	(void)of_property_read_string(pdev->dev.of_node,"device-name", &dev->name);

	dev->screen_mode = tcc_scrshare_parse_overlay_node(&dev->overlay, pdev);
}

static int tcc_scrshare_alloc_chrdev_region(struct tcc_scrshare_device *dev) {

	int ret = 0;

	ret = alloc_chrdev_region(&dev->devt, 0, 1, dev->name);
	if (ret != 0) {
		(void)pr_err("%s: Fail alloc_chrdev_region(%d)\n", __func__, ret);
	}

	return ret;
}

static int tcc_scrshare_cdev_init(
struct tcc_scrshare_device *dev) {

	int ret = 0;

	cdev_init(&dev->scdev, &tcc_scrshare_fops);
	dev->scdev.owner = THIS_MODULE;
	ret = cdev_add(&dev->scdev, dev->devt, 1);
	if (ret != 0) {
		(void)pr_err("%s: Fail cdev_add(%d)\n", __func__, ret);
	}

	return ret;
}

static int tcc_scrshare_create_class(
struct tcc_scrshare_device *dev) {

	int ret = 0;

	dev->pclass = class_create(THIS_MODULE, dev->name);
	if (IS_ERR(dev->pclass)) {
		ret = (int)PTR_ERR(dev->pclass);
		(void)pr_err("%s: Fail class_create(%d)\n", __func__, ret);
	}

	return ret;
}

static int tcc_scrshare_create_dev(
struct tcc_scrshare_device *dev, struct platform_device *pdev) {

	int ret = 0;

	dev->dev = device_create(dev->pclass,
		&pdev->dev,  dev->devt, NULL, dev->name);
	if (IS_ERR(dev->dev)) {
		ret = (int)PTR_ERR(dev->dev);
		(void)pr_err(" %s: Fail device_create(%d)\n", __func__, ret);
	}
	else {
		dev->pdev = pdev;
	}

	return ret;
}

static int tcc_scrshare_ipc_init(
struct tcc_scrshare_device *dev, struct platform_device *pdev) {

	int ret = 0;

#if defined(USE_TCC_IPC_SYSTEM)
	ret = tcc_scrshare_register_ipc(dev, pdev);
#else
	(void)of_property_read_string(pdev->dev.of_node,
		"mbox-names", &dev->ipc.mbox_name);
	dev->ipc.mbox_ch = tcc_scrshare_request_channel(dev, dev->ipc.mbox_name);
	if (IS_ERR(dev->ipc.mbox_ch)) {
		ret = (int)PTR_ERR(dev->ipc.mbox_ch);
		(void)pr_err("%s: Fail tcc_scrshare_request_channel(%d)\n",
			 __func__, ret);
	}
#endif
	return ret;
}

static int tcc_scrshare_dev_init(
struct tcc_scrshare_device *dev, struct platform_device *pdev) {

	int ret = 0;

	platform_set_drvdata(pdev, dev);

	tcc_scrshare_parse_node(dev, pdev);

	ret = tcc_scrshare_ipc_init(dev, pdev);
	if(ret == 0) {
		ret = tcc_scrshare_alloc_chrdev_region(dev);
	}

	if(ret == 0) {
		ret = tcc_scrshare_cdev_init(dev);
	}

	if(ret == 0) {
		ret = tcc_scrshare_create_class(dev);
	}

	if(ret == 0) {
		ret = tcc_scrshare_create_dev(dev, pdev);
	}

	return ret;
}

static int tcc_scrshare_info_init(
struct tcc_scrshare_device *dev) {

	int ret = 0;
	struct tcc_scrshare_info *info;

	info = kzalloc(sizeof(struct tcc_scrshare_info), GFP_KERNEL);
	if (info == NULL) {
		ret = -ENOMEM;
	}

	if (ret == 0) {
		info->srcinfo = kzalloc(sizeof(struct tcc_scrshare_srcinfo), GFP_KERNEL);
		if (info->srcinfo == NULL) {
			kfree(info);
			(void)pr_err("%s: err_scrshare_mem.\n",__func__);
			ret = -ENOMEM;
		}
	}

	if (ret == 0) {
		info->dstinfo = kzalloc(sizeof(struct tcc_scrshare_dstinfo), GFP_KERNEL);
		if (info->dstinfo == NULL) {
			kfree(info->srcinfo);
			kfree(info);
			(void)pr_err("%s: err_scrshare_mem.\n",__func__);
			ret = -ENOMEM;
		}
	}

	if (ret == 0) {
		info->dstinfo->x = 640;
		info->dstinfo->y = 0;
		info->dstinfo->width = 640;
		info->dstinfo->height = 720;
		info->dstinfo->img_num = 1;

		dev->info = info;
	}

	return ret;

}

static int tcc_scrshare_probe(
struct platform_device *pdev) {

	int ret = 0;
	struct tcc_scrshare_device *dev;

	dev = devm_kzalloc(&pdev->dev, sizeof(struct tcc_scrshare_device), GFP_KERNEL);
	if (dev == NULL) {
		ret = -ENOMEM;
	}
	else {
		ret = tcc_scrshare_dev_init(dev, pdev);
		if(ret == 0) {
			atomic_set(&dev->status, SCRSHARE_STS_INIT);

			tcc_scrshare_tx_init(dev);
			if (tcc_scrshare_rx_init(dev) != 0) {
				(void)pr_warn("%s: Fail tcc_scrshare_rx_init\n", __func__);
				ret = -EFAULT;
			}
			else {
				ret = tcc_scrshare_info_init(dev);
				if(ret == 0) {
					(void)pr_info("%s: %s(%s) Driver Initialized(%s mode)\n",__func__,
						dev->name, release_date, (dev->screen_mode == 0u)?"bridge":"screen");
					stcc_scrshare_device = dev;
				}
			}
		}
	}

	return ret;
}

static int tcc_scrshare_remove(
struct platform_device *pdev) {

	struct tcc_scrshare_device *dev = platform_get_drvdata(pdev);

	if(dev != NULL) {
		const struct tcc_scrshare_info *info = dev->info;

		if(info != NULL) {
			if(info->dstinfo != NULL) {
				kfree(info->dstinfo);
			}
			if(info->srcinfo != NULL) {
				kfree(info->srcinfo);
			}
			kfree(info);
		}

		device_destroy(dev->pclass, dev->devt);
		class_destroy(dev->pclass);
		cdev_del(&dev->scdev);
		unregister_chrdev_region(dev->devt, 1);

		devm_kfree(&pdev->dev, dev);
	}

	return 0;
}

#ifdef CONFIG_PM
static int tcc_scrshare_suspend(
struct platform_device *pdev, pm_message_t pm_state) {

#if defined(USE_TCC_IPC_SYSTEM)
	/* unregister mailbox client */
	/* To be */
	(void)pdev;
#else
	struct tcc_scrshare_device *dev = platform_get_drvdata(pdev);

	/* unregister mailbox client */
	if (dev->ipc.mbox_ch != NULL) {
		mbox_free_channel(dev->ipc.mbox_ch);
		dev->ipc.mbox_ch = NULL;
	}
#endif
    (void)pm_state;

	return 0;
}

static int tcc_scrshare_resume(
struct platform_device *pdev) {

	int ret = 0;
#if defined(USE_TCC_IPC_SYSTEM)
	/* To be */
	(void)pdev;
#else
	struct tcc_scrshare_device *dev = platform_get_drvdata(pdev);

	/* register mailbox client */
	dev->ipc.mbox_ch = tcc_scrshare_request_channel(dev, dev->ipc.mbox_name);
	if (IS_ERR(dev->ipc.mbox_ch)) {
		ret = (int)PTR_ERR(dev->ipc.mbox_ch);
		(void)pr_err("%s: Fail request_channel (%d)\n", __func__, ret);
	}
#endif

	return ret;
}
#endif

#ifdef CONFIG_OF
static const struct of_device_id tcc_scrshare_of_match[] = {
	{.compatible = "telechips,tcc_scrshare",},
	{},
};

MODULE_DEVICE_TABLE(of, tcc_scrshare_of_match);
#endif

static struct platform_driver tcc_scrshare = {
	.probe = tcc_scrshare_probe,
	.remove = tcc_scrshare_remove,
	.driver = {
		   .name = "tcc_scrshare",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = tcc_scrshare_of_match,
#endif
		   },
#ifdef CONFIG_PM
	.suspend = tcc_scrshare_suspend,
	.resume = tcc_scrshare_resume,
#endif
};

static int __init tcc_scrshare_init(void)
{
	return platform_driver_register(&tcc_scrshare);
}

static void __exit tcc_scrshare_exit(void)
{
	platform_driver_unregister(&tcc_scrshare);
}

module_init(tcc_scrshare_init);
module_exit(tcc_scrshare_exit);
MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips SCREEN_SHARE Driver");
MODULE_LICENSE("GPL");
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
/* end of file */
