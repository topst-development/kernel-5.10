// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#if 0
#define NDEBUG
#endif
#define TLOG_LEVEL (TLOG_WARNING)
#include "tcc_ipc_log.h"

#include <linux/mailbox/tcc_sec_ipc.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mailbox/mailbox-tcc.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/time.h>

#if defined(CONFIG_TELECHIPS_SSS_MAILBOX)
#include <linux/mailbox/mailbox-tcc-sss.h>
#endif

#if IS_ENABLED(CONFIG_TCC_IPI_PROTOCOL)
#include <linux/firmware/tcc_ipi.h>
#endif

/**
 * @addtogroup secdrv
 * @{
 * @file tcc_sec_ipc.c This file contains sec_ipc device driver,
 *	communicating with a53 <-> A7, R5, M4. (for TCC803x)
 *	communicating with a72 <-> A53, R5, M4. (for TCC805x)
 */

#define DEVICE_NAME ("sec-ipc")

/** Used when R2R/M2M data is transfered to SP. */
#define MBOX_DMA_SIZE (1U * 1024U * 1024U)

/** Time to wait for SP to respond. */
#define CMD_TIMEOUT_MSEC (3000U)

/** Returns a demux event from a mailbox command. The demux event can be
 * distinguished by cmd[15:12], i.e. magic number 0 for demux event.
 */
#define IS_DMX_EVENT(cmd)                                                      \
	((0U != (((cmd)&0xFFFF0000U) >> 16U)) &&                               \
	 (0U == (((cmd)&0xF000U) >> 12U)))

/** Returns an event from a mailbox command. The event can be
 * distinguished by cmd[15:12], magic number of THSM is 5
 */
#define IS_THSM_EVENT(cmd) (5U == (((cmd)&0xF000U) >> 12U))
#define HSM_EVENT_FLAG(cmd) (0x00000001U << (cmd))

//#define DEBUG_TIME_MEASUREMENT 1

static const struct of_device_id sec_ipc_dt_id[7] = {
	{ .compatible = "telechips,sec-ipc-m4" },
	{ .compatible = "telechips,sec-ipc-hsm" },
	{ .compatible = "telechips,sec-ipc-a7" },
	{ .compatible = "telechips,sec-ipc-a53" },
	{ .compatible = "telechips,sec-ipc-a72" },
	{ .compatible = "telechips,sec-ipc-r5" },
	{},
};

MODULE_DEVICE_TABLE(of, sec_ipc_dt_id);

struct tcc_sec_msg {
	uint32_t cmd;
	uint32_t msg_len;
	uint32_t trans_type;
	uintptr_t dma_addr;
	uint8_t message[TCC_MBOX_MAX_SIZE];
};
struct sec_device {
	struct tcc_sec_msg mbox_rmsg;
	struct device *hsm_device;
	struct cdev hsm_cdev;
	dev_t devnum;
	struct class *hsm_class;
	int32_t mbox_received;
	struct mbox_chan *mbox_ch;
	uint32_t hsm_recv_event;
	uint8_t *vaddr; // Holds a virtual address to DMA.
	dma_addr_t paddr; // Holds a physical address to DMA.
#if IS_ENABLED(CONFIG_TCC_IPI_PROTOCOL)
	struct device_node *mbox_client;
	int index;
#endif
};

static struct sec_device *sec_ipc_device[MBOX_DEV_MAX];

static int32_t (*dmx_callback)(int32_t cmd, void *rdata, int32_t size);
static DECLARE_WAIT_QUEUE_HEAD(waitq);
static DECLARE_WAIT_QUEUE_HEAD(event_waitq);

static DEFINE_MUTEX(sec_mutex);
static uint32_t recv_event;

/**
 * Used to upload SP firmware to memory.
 * @warning This variable is used only during SP firmware development. Once
 * SP firmware is included BL1, the variable is not used.
 */
static void __iomem *codebase;

/**
 * Mapped to CM4_RESET register.
 * @warning This variable is used only during SP firmware development. Once
 * SP firmware is included BL1, the variable is not used.
 */
static void __iomem *cfgbase;

static int32_t sec_set_device(uint32_t device_id, struct sec_device *sec_dev)
{
	int result = 0;

	if (device_id == MBOX_DEV_M4) {
		sec_ipc_device[MBOX_DEV_M4] = sec_dev;
	} else if (device_id == MBOX_DEV_A7) {
		sec_ipc_device[MBOX_DEV_A7] = sec_dev;
	} else if (device_id == MBOX_DEV_A53) {
		sec_ipc_device[MBOX_DEV_A53] = sec_dev;
	} else if (device_id == MBOX_DEV_A72) {
		sec_ipc_device[MBOX_DEV_A72] = sec_dev;
	} else if (device_id == MBOX_DEV_R5) {
		sec_ipc_device[MBOX_DEV_R5] = sec_dev;
	} else if (device_id == MBOX_DEV_HSM) {
		sec_ipc_device[MBOX_DEV_HSM] = sec_dev;
	} else {
		result = -EINVAL;
	}

	return result;
}

static struct sec_device *sec_get_device(uint32_t device_id)
{
	struct sec_device *sec_dev = NULL;

	if (device_id == MBOX_DEV_M4) {
		sec_dev = sec_ipc_device[MBOX_DEV_M4];
	} else if (device_id == MBOX_DEV_A7) {
		sec_dev = sec_ipc_device[MBOX_DEV_A7];
	} else if (device_id == MBOX_DEV_A53) {
		sec_dev = sec_ipc_device[MBOX_DEV_A53];
	} else if (device_id == MBOX_DEV_A72) {
		sec_dev = sec_ipc_device[MBOX_DEV_A72];
	} else if (device_id == MBOX_DEV_R5) {
		sec_dev = sec_ipc_device[MBOX_DEV_R5];
	} else if (device_id == MBOX_DEV_HSM) {
		sec_dev = sec_ipc_device[MBOX_DEV_HSM];
	} else {
		sec_dev = NULL;
	}

	return sec_dev;
}

static uint32_t sec_get_device_id(const char *mbox_dev_name)
{
	uint32_t dev_id = MBOX_DEV_INVALID;

	if (strcmp(mbox_dev_name, (const char *)"sec-ipc-m4") == 0) {
		dev_id = MBOX_DEV_M4;
	} else if (strcmp(mbox_dev_name, (const char *)"sec-ipc-a7") == 0) {
		dev_id = MBOX_DEV_A7;
	} else if (strcmp(mbox_dev_name, (const char *)"sec-ipc-a53") == 0) {
		dev_id = MBOX_DEV_A53;
	} else if (strcmp(mbox_dev_name, (const char *)"sec-ipc-a72") == 0) {
		dev_id = MBOX_DEV_A72;
	} else if (strcmp(mbox_dev_name, (const char *)"sec-ipc-r5") == 0) {
		dev_id = MBOX_DEV_R5;
	} else if (strcmp(mbox_dev_name, (const char *)"sec-ipc-hsm") == 0) {
		dev_id = MBOX_DEV_HSM;
	} else {
		dev_id = MBOX_DEV_INVALID;
	}

	return dev_id;
}

/* To reduce codesonar warning message */
static int32_t sec_copy_from_user(void *param, ulong arg, uint32_t size)
{
	int32_t result = 0;

	if (copy_from_user((void *)param, (const void *)arg, (ulong)size) !=
	    (ulong)0) {
		ELOG("copy_from_user failed\n");
		result = -ENOMEM;
	}

	return result;
}

/* To reduce codesonar warning message */
static int32_t sec_copy_to_user(ulong arg, void *param, uint32_t size)
{
	int32_t result = 0;

	if (copy_to_user((void *)arg, (const void *)param, (ulong)size) !=
	    (ulong)0) {
		ELOG("copy_to_user failed\n");
		result = -ENOMEM;
	}

	return result;
}

/**
 * This function communicates with Secure Processor (SP), sending and
 * receiving data along with SP command. It support thread-safe.
 * @param[in] cmd SP command, made by #SP_CMD macro.
 * @param[in] data A pointer to data in kernel space to send.
 * @param[in] size size of data. This must be less than #MBOX_DMA_SIZE.
 * @param[out] rdata A pointer to data to receive in kernel space. It can be
 *NULL if not necessary.
 * @param[in] rsize size of rdata. This must be less than #MBOX_DMA_SIZE.
 * @return On success, it returns received byte size and a errno, e.g.
 *-EXXX,otherwise.
 */
int32_t sec_sendrecv_cmd(uint32_t device_id, uint32_t cmd, void *data,
			 uint32_t size, void *rdata, uint32_t rsize)
{
#if IS_ENABLED(CONFIG_TCC_IPI_PROTOCOL)
	struct tcc_ipi_msg mbox_data;
#else
	struct tcc_mbox_data mbox_data;
#endif

	int32_t result = 0;
	int32_t mbox_result = 0;
	struct sec_device *sec_dev = NULL;

	mutex_lock(&sec_mutex);

#ifdef DEBUG_TIME_MEASUREMENT
	struct timeval t1 = { 0 }, t2 = { 0 };
	int32_t time_gap_ms = 0;
#endif

	sec_dev = sec_get_device(device_id);
	if (sec_dev == NULL) {
		ELOG("Can't find device\n");
		result = -EINVAL;
		goto out;
	}

#if !IS_ENABLED(CONFIG_TCC_IPI_PROTOCOL)
	if (sec_dev->mbox_ch == NULL) {
		ELOG("Channel cannot do Tx\n");
		result = -EINVAL;
		goto out;
	}
#endif

#if IS_ENABLED(CONFIG_TCC_IPI_PROTOCOL)
	mbox_data.cmd =
		devm_kzalloc(sec_dev->hsm_device,
			     (sizeof(u32) * HSM_MBOX_CMD_MAX_LEN), GFP_KERNEL);
	if (mbox_data.cmd == NULL) {
		ELOG("devm_kzalloc fail\n");
		result = -EINVAL;
		goto out;
	}
#endif

	if ((size > MBOX_DMA_SIZE) || (rsize > MBOX_DMA_SIZE)) {
		ELOG("Err size=%d rsize=%d\n", size, rsize);
		result = -EINVAL;
		goto out;
	}

	if (sec_dev->paddr > UINT_MAX) {
		ELOG("sec_dev->paddr Err\n");
		result = -EINVAL;
		goto out;
	}

#if IS_ENABLED(CONFIG_TCC_IPI_PROTOCOL)
	mbox_data.cmd_len = 6;
#endif
	mbox_data.cmd[0] = cmd;
	mbox_data.cmd[1] = (uint32_t)sec_dev->paddr;
	mbox_data.cmd[3] = size;
	mbox_data.data_len = ((size + 3U) / (uint32_t)sizeof(uint32_t));

	// size 0 is included on purpose to send a command without data
	if (size <= TCC_MBOX_MAX_SIZE) {
#if IS_ENABLED(CONFIG_TCC_IPI_PROTOCOL)
		mbox_data.data_buf = data;
		mbox_data.cmd[2] = MBOX_NONE_DMA;
		DLOG("cmd %X, size %d\n", cmd, size);
#else
		memcpy(mbox_data.data, data, size);
		mbox_data.cmd[2] = MBOX_NONE_DMA;
		DLOG("cmd %X, size %d\n", cmd, size);
#endif
		// print_hex_dump_bytes("Sending message: ",
		// DUMP_PREFIX_ADDRESS, mbox_msg.message, size);
	} else {
		memcpy(sec_dev->vaddr, data, size);
		mbox_data.cmd[2] = MBOX_DMA;
	}

#if IS_ENABLED(CONFIG_TCC_IPI_PROTOCOL)
	mbox_result = tcc_ipi_send_data(sec_dev->mbox_client, &mbox_data,
					sec_dev->index);
#else
	// Init condition to wait
	mbox_result = mbox_send_message(sec_dev->mbox_ch, &(mbox_data));
#endif
#if !defined(CONFIG_TCC_IPI_PROTOCOL) && (defined(CONFIG_ARCH_TCC803X))
	mbox_client_txdone(sec_dev->mbox_ch, 0);
#endif
	if (mbox_result < 0) {
		ELOG("Failed to send message via mailbox\n");
		result = -EINVAL;
		goto out;
	}
		// Awaiting mbox_msg_received to be called.
#ifdef DEBUG_TIME_MEASUREMENT
	do_gettimeofday(&t1);
#endif

	if ((wait_event_timeout(waitq, sec_dev->mbox_received,
				CMD_TIMEOUT_MSEC) == (long)0) &&
	    (sec_dev->mbox_received != 1)) {
		ELOG("Cmd: %d Timeout\n", cmd);
		result = -EINVAL;
		goto out;
	}
#ifdef DEBUG_TIME_MEASUREMENT
	do_gettimeofday(&t2);
	time_gap_ms = ((t2.tv_sec - t1.tv_sec) * 1000) +
		      ((t2.tv_usec - t1.tv_usec) / 1000);
	DLOG("SendRecv gap time = %d ms\n", time_gap_ms)
#endif
	// mbox_rmsg.msg_len is set at this point by sec_msg_received
	// Nothing to read
	if ((rdata == NULL) || (rsize == 0U)) {
		result = 0;
		goto out;
	}
	if (sec_dev->mbox_rmsg.msg_len > (int32_t)rsize) {
		result = -EPERM;
		ELOG("received msg size(0x%x) is larger than rsize(0x%x)\n",
		     sec_dev->mbox_rmsg.msg_len, rsize);
		goto out;
	}
	if ((sec_dev->mbox_rmsg.trans_type == (int32_t)MBOX_NONE_DMA) &&
	    (sec_dev->mbox_rmsg.msg_len > TCC_MBOX_MAX_SIZE)) {
		result = -EPERM;
		ELOG("received msg size(0x%x) is larger than rsize(0x%x)\n",
		     sec_dev->mbox_rmsg.msg_len, TCC_MBOX_MAX_SIZE);
		goto out;
	}
	// Copy received data
	if (sec_dev->mbox_rmsg.trans_type == (int32_t)MBOX_NONE_DMA) {
		memcpy(rdata, (const void *)sec_dev->mbox_rmsg.message,
		       (uint32_t)sec_dev->mbox_rmsg.msg_len);
	} else {
		memcpy(rdata, (const void *)sec_dev->vaddr,
		       (uint32_t)sec_dev->mbox_rmsg.msg_len);
	}
	result = sec_dev->mbox_rmsg.msg_len;

out:
#if IS_ENABLED(CONFIG_TCC_IPI_PROTOCOL)
	if (mbox_data.cmd != NULL) {
		devm_kfree(sec_dev->hsm_device, mbox_data.cmd);
	}
#endif
	if (sec_dev != NULL) {
		sec_dev->mbox_received = 0;
	}
	DLOG("End result=%d\n", result);
	mutex_unlock(&sec_mutex);
	return result;
}
EXPORT_SYMBOL(sec_sendrecv_cmd);

/**
 * This function sets a callback for demux driver. Demux driver can
 * get noticed by the callback.
 * @note This is a temporary solution to work with demux driver.
 *	When the demux driver is refactored, this function will be removed.
 * @param dmx_cb  a pointer to a callback function.
 */
void sec_set_callback(int32_t (*dmx_cb)(int32_t cmd, void *rdata, int32_t size))
{
	dmx_callback = dmx_cb;
}
EXPORT_SYMBOL(sec_set_callback);

static int32_t sec_open(struct inode *sec_inode, struct file *sec_filp)
{
	return 0;
}

static int32_t sec_release(struct inode *sec_inode, struct file *sec_filp)
{
	return 0;
}

static int32_t sec_send_cmd_ioctl(ulong arg)
{
#if IS_ENABLED(CONFIG_TCC_IPI_PROTOCOL)
	struct tcc_ipi_msg mbox_data = { 0 };
#else
	struct tcc_mbox_data mbox_data = { 0 };
#endif
	int32_t result = 0;
	int32_t mbox_result = 0;
	uint32_t data_size = 0;
	struct sec_device *sec_dev = NULL;
	struct sec_segment segment;

	// Copy data from user space to kernel space
	result = sec_copy_from_user((void *)&segment, arg,
				    (uint32_t)sizeof(struct sec_segment));
	if (result != 0) {
		ELOG("copy_from_user failed: %d\n", result);
		goto out;
	}

	if ((segment.size > MBOX_DMA_SIZE) || (segment.data_addr > ULONG_MAX)) {
		ELOG("size(0x%x) or addr(0x%lx) is invalid\n", segment.size,
		     segment.data_addr);
		result = -EINVAL;
		goto out;
	}

	sec_dev = sec_get_device(segment.device_id);
	if (sec_dev == NULL) {
		ELOG("Can't find device\n");
		result = -EINVAL;
		goto out;
	}
#if !IS_ENABLED(CONFIG_TCC_IPI_PROTOCOL)
	if (sec_dev->mbox_ch == NULL) {
		ELOG("Channel cannot do Tx\n");
		result = -EINVAL;
		goto out;
	}
#endif

#if IS_ENABLED(CONFIG_TCC_IPI_PROTOCOL)
	mbox_data.cmd =
		devm_kzalloc(sec_dev->hsm_device,
			     (sizeof(u32) * HSM_MBOX_CMD_MAX_LEN), GFP_KERNEL);
	mbox_data.data_buf =
		devm_kzalloc(sec_dev->hsm_device, segment.size, GFP_KERNEL);
	if ((mbox_data.cmd == NULL) || (mbox_data.data_buf == NULL)) {
		ELOG("devm_kzalloc fail\n");
		result = -EINVAL;
		goto out;
	}
#endif

#if IS_ENABLED(CONFIG_TCC_IPI_PROTOCOL)
	mbox_data.cmd_len = 6;
#endif
	data_size = segment.size;
	mbox_data.cmd[0] = 0; // To be handled by a normal command
	mbox_data.cmd[1] = (uint32_t)sec_dev->paddr;
	mbox_data.cmd[3] = data_size;
	mbox_data.data_len = ((data_size + 3U) / (uint32_t)sizeof(uint32_t));

	// size 0 is included on purpose to send a command without data
	if (data_size <= TCC_MBOX_MAX_SIZE) {
#if IS_ENABLED(CONFIG_TCC_IPI_PROTOCOL)
		if (copy_from_user((void *)mbox_data.data_buf,
				   (const void *)segment.data_addr,
				   data_size) != 0) {
			ELOG("copy_from_user failed\n");
			result = -EINVAL;
			goto out;
		}
		mbox_data.cmd[2] = MBOX_NONE_DMA;
		DLOG("cmd=0x%X, data size=0x%x\n", segment.cmd, data_size);
#else
		if (copy_from_user((void *)mbox_data.data,
				   (const void *)segment.data_addr,
				   data_size) != 0) {
			ELOG("copy_from_user failed\n");
			result = -EINVAL;
			goto out;
		}
		mbox_data.cmd[2] = MBOX_NONE_DMA;
		DLOG("cmd=0x%X, data size=0x%x\n", segment.cmd, data_size);
#endif
	} else {
		if (copy_from_user((void *)sec_dev->vaddr,
				   (const void *)segment.data_addr,
				   data_size) != 0) {
			ELOG("copy_from_user failed\n");
			result = -EINVAL;
			goto out;
		}
		mbox_data.cmd[2] = MBOX_DMA;
	}
	DLOG("SEND cmd[0]=0x%x dma addr=0x%x dma type=0x%x data_len=0x%x\n",
	     mbox_data.cmd[0], mbox_data.cmd[1], mbox_data.cmd[2],
	     mbox_data.data_len);
	// Init condition to wait
	sec_dev->mbox_received = 0;
#if IS_ENABLED(CONFIG_TCC_IPI_PROTOCOL)
	mbox_result = tcc_ipi_send_data(sec_dev->mbox_client, &mbox_data,
					sec_dev->index);
#else
	mbox_result = mbox_send_message(sec_dev->mbox_ch, &(mbox_data));
	mbox_client_txdone(sec_dev->mbox_ch, 0);
#endif

	if (mbox_result < 0) {
		ELOG("Failed to send message via mailbox\n");
		result = -EINVAL;
		goto out;
	}

out:
#if IS_ENABLED(CONFIG_TCC_IPI_PROTOCOL)
	if (mbox_data.cmd != NULL) {
		devm_kfree(sec_dev->hsm_device, mbox_data.cmd);
	}
	if (mbox_data.data_buf != NULL) {
		devm_kfree(sec_dev->hsm_device, mbox_data.data_buf);
	}
#endif
	return result;
}

static int32_t sec_get_evt_ioctl(ulong arg)
{
	int32_t result = -1;

	result = sec_copy_to_user(arg, (void *)&recv_event,
				  (uint32_t)sizeof(uint32_t));
	if (result != 0) {
		ELOG("copy_to_user failed: %d\n", result);
	}
	DLOG("recv_event: %d\n", recv_event);
	return result;
}

static int32_t sec_get_evt_info_ioctl(ulong arg)
{
	int32_t result = 0;
	struct sec_segment segment_user;
	struct sec_device *sec_dev = NULL;

	result = sec_copy_from_user((void *)&segment_user, arg,
				    (uint32_t)sizeof(struct sec_segment));
	if (result != 0) {
		ELOG("copy_from_user failed: %d\n", result);
		goto out;
	}

	sec_dev = sec_get_device(segment_user.device_id);
	if (sec_dev == NULL) {
		ELOG("Can't find device\n");
		result = -EINVAL;
		goto out;
	}

	/* Send received data */
	if (sec_dev->mbox_rmsg.trans_type == (int32_t)MBOX_NONE_DMA) {
		result = sec_copy_to_user(segment_user.rdata_addr,
					  (void *)&sec_dev->mbox_rmsg.message,
					  (uint32_t)sec_dev->mbox_rmsg.msg_len);
	} else {
		result = sec_copy_to_user(segment_user.rdata_addr,
					  (void *)&sec_dev->vaddr,
					  (uint32_t)sec_dev->mbox_rmsg.msg_len);
	}
	if (result != 0) {
		ELOG("copy_to_user failed: %d\n", result);
		goto out;
	}

	/* Send cmd and rsize */
	segment_user.cmd = (uint32_t)sec_dev->mbox_rmsg.cmd;
	segment_user.rsize = (uint32_t)sec_dev->mbox_rmsg.msg_len;
	result = sec_copy_to_user(arg, (void *)&segment_user,
				  (uint32_t)sizeof(struct sec_segment));
	if (result != 0) {
		ELOG("copy_to_user failed: %d\n", result);
		goto out;
	}
	recv_event &= (uint32_t) ~(HSM_EVENT_FLAG(segment_user.device_id));
	memset(sec_dev->mbox_rmsg.message, 0,
	       (uint32_t)sec_dev->mbox_rmsg.msg_len);
	sec_dev->mbox_rmsg.msg_len = 0;

out:
	return result;
}

static long sec_ioctl(struct file *filp, uint32_t cmd, ulong arg)
{
	int32_t result = 0;

	switch (cmd) {
	case SEC_SEND_CMD:
		result = sec_send_cmd_ioctl(arg);
		break;

	case SEC_GET_EVENTS:
		result = sec_get_evt_ioctl(arg);
		break;

	case SEC_GET_EVT_INFO:
		result = sec_get_evt_info_ioctl(arg);
		break;

	default:
		ELOG("ioctl failed: %d\n", cmd);
		result = -EINVAL;
		break;
	}

	return result;
}

static uint32_t sec_poll(struct file *filp, poll_table *wait)
{
	uint32_t result = 0;

	poll_wait(filp, &event_waitq, wait);

	if (recv_event != 0U) {
		result = POLLPRI;
		goto out;
	} else {
		result = 0;
		goto out;
	}

out:
	return result;
}

#if 0 // Test code
static int32_t sec_send_cmd(
		int32_t cmd, void *data, int32_t size, int32_t device_id)
{
	struct tcc_mbox_data mbox_data = {
		0,
	};
	int32_t result = 0, mbox_result = 0;
	uint32_t data_size = 0;
	struct sec_device *sec_dev = NULL;

	sec_dev = sec_get_device(device_id);
	if (sec_dev == NULL) {
		ELOG("Can't find device\n");
		return -EINVAL;
	}
	if (size < 0 || MBOX_DMA_SIZE < size) {
		ELOG("size is %d\n", size);
		return -EINVAL;
	}
	if (!sec_dev->mbox_ch) {
		ELOG("Channel cannot do Tx\n");
		return -EINVAL;
	}

	mbox_data.cmd[0] = cmd;
	mbox_data.cmd[1] = (unsigned int)sec_dev->paddr;
	mbox_data.cmd[3] = size;
	mbox_data.data_len = ((size + 3) / (uint32_t)sizeof(uint32_t));
	data_size = size;
	// size 0 is included on purpose to send a command without data
	if (size <= TCC_MBOX_MAX_SIZE) {
		memcpy(mbox_data.data, data, size);
		mbox_data.cmd[2] = MBOX_NONE_DMA;
	} else if (size > TCC_MBOX_MAX_SIZE) {
		memcpy(sec_dev->vaddr, data, size);
		mbox_data.cmd[2] = MBOX_DMA;
	}
	DLOG("SEND cmd[0]=0x%x cmd[1]=0x%x cmd[2]=0x%x len=0x%x\n",
	     mbox_data.cmd[0], mbox_data.cmd[1], mbox_data.cmd[2],
	     mbox_data.data_len);
	// Init condition to wait
	sec_dev->mbox_received = 0;
	mbox_result = mbox_send_message(sec_dev->mbox_ch, &(mbox_data));
	mbox_client_txdone(sec_dev->mbox_ch, 0);
	if (mbox_result < 0) {
		ELOG("Failed to send message via mailbox\n");
		result = -EINVAL;
		goto out;
	}

out:
	return result;
}

static void test_send_mbox(int32_t cmd)
{
	int32_t device_id = MBOX_DEV_R5;
	uint8_t buffer[40] = {
		0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x28, 0x21,
		0x0f, 0xde, 0x1f, 0x51, 0xc0, 0x0b, 0x59, 0x68, 0x33, 0x6a,
		0x4a, 0x87, 0x83, 0x12, 0x7a, 0x33, 0x56, 0xca, 0xfc, 0xfd,
		0xcf, 0x31, 0x1f, 0x7a, 0xc8, 0x99, 0xe5, 0x55, 0xf6, 0x4b
	};
	cmd = cmd & 0xFFFF;
	sec_send_cmd(cmd, buffer, sizeof(buffer), device_id);
}

#endif

	/**
 * This function is atomic.
 */

#if IS_ENABLED(CONFIG_TCC_IPI_PROTOCOL)
static int sec_msg_received_callback(struct tcc_ipi_msg *msg,
				     struct platform_device *pdev)
{
	struct sec_device *sec_dev = NULL;
	uint32_t msg_len = 0;
	uint32_t cmd = 0;
	uint32_t trans_type = 0;
	uint32_t dma_addr = 0;
	uint32_t device_id = 0;

	device_id = sec_get_device_id(pdev->name);

	if (device_id >= MBOX_DEV_MAX) {
		ELOG("Invalid dev_id(%d)\n", device_id);
		goto out;
	}
	sec_dev = sec_get_device(device_id);
	if (sec_dev == NULL) {
		ELOG("Can't find device\n");
		goto out;
	}

	cmd = msg->cmd[0];
	dma_addr = msg->cmd[1];
	trans_type = msg->cmd[2];
	msg_len = msg->cmd[3];

	if (msg_len > TCC_MBOX_MAX_SIZE) {
		ELOG("Invalid msg_len(0x%x)\n", msg_len);
		goto out;
	}
	if (IS_DMX_EVENT(cmd) != (bool)0) {
		if (dmx_callback != NULL) {
			dmx_callback(cmd, msg->data_buf, (int32_t)msg_len);
		}
	} else if (IS_THSM_EVENT(cmd) != (bool)0) {
		if (trans_type == MBOX_NONE_DMA) {
			memcpy(sec_dev->mbox_rmsg.message, msg->data_buf,
			       msg_len);
		} else {
			sec_dev->mbox_rmsg.dma_addr = dma_addr;
		}

		sec_dev->mbox_rmsg.trans_type = (int32_t)trans_type;
		sec_dev->mbox_rmsg.msg_len = (int32_t)msg_len;
		sec_dev->mbox_rmsg.cmd = _IOC_NR(cmd);

		recv_event |= (uint32_t)HSM_EVENT_FLAG(device_id);
		wake_up(&event_waitq);
		// test_send_mbox(cmd);
	} else { /* For normal SP commands */
		if (trans_type == MBOX_NONE_DMA) {
			memcpy(sec_dev->mbox_rmsg.message, msg->data_buf,
			       msg_len);
		} else {
			sec_dev->mbox_rmsg.dma_addr = dma_addr;
		}
		sec_dev->mbox_rmsg.trans_type = (int32_t)trans_type;
		sec_dev->mbox_rmsg.msg_len = (int32_t)msg_len;
		sec_dev->mbox_received = 1;
		wake_up(&waitq);
	}

out:
	return 0;
}
#else
static void sec_msg_received(struct mbox_client *client, void *message)
{
	struct tcc_mbox_data *mbox_data = NULL;
	struct sec_device *sec_dev = NULL;
	uint32_t msg_len = 0;
	uint32_t cmd = 0;
	uint32_t trans_type = 0;
	uint32_t dma_addr = 0;
	uint32_t device_id = 0;

	mbox_data = (struct tcc_mbox_data *)message;
	device_id = sec_get_device_id(client->dev->init_name);
	if (device_id >= MBOX_DEV_MAX) {
		ELOG("Invalid dev_id(%d)\n", device_id);
		goto out;
	}
	sec_dev = sec_get_device(device_id);
	if (sec_dev == NULL) {
		ELOG("Can't find device\n");
		goto out;
	}

	/* Get cmd fifo */
	cmd = mbox_data->cmd[0];
	dma_addr = mbox_data->cmd[1];
	trans_type = mbox_data->cmd[2];
	msg_len = mbox_data->cmd[3];

	if (msg_len > TCC_MBOX_MAX_SIZE) {
		ELOG("Invalid msg_len(0x%x)\n", msg_len);
		goto out;
	}

	if (IS_DMX_EVENT(cmd) != (bool)0) { /* Demux event */
		if (dmx_callback != NULL) {
			dmx_callback(cmd, mbox_data->data, (int32_t)msg_len);
		}
	} else if (IS_THSM_EVENT(cmd) != (bool)0) {
		if (trans_type == MBOX_NONE_DMA)
			memcpy(sec_dev->mbox_rmsg.message, mbox_data->data,
			       msg_len);
		else {
			sec_dev->mbox_rmsg.dma_addr = dma_addr;
		}

		sec_dev->mbox_rmsg.trans_type = (int32_t)trans_type;
		sec_dev->mbox_rmsg.msg_len = (int32_t)msg_len;
		sec_dev->mbox_rmsg.cmd = _IOC_NR(cmd);

		recv_event |= (uint32_t)HSM_EVENT_FLAG(device_id);
		wake_up(&event_waitq);
		// test_send_mbox(cmd);
	} else { /* For normal SP commands */
		if (trans_type == MBOX_NONE_DMA)
			memcpy(sec_dev->mbox_rmsg.message, mbox_data->data,
			       msg_len);
		else {
			sec_dev->mbox_rmsg.dma_addr = dma_addr;
		}
		sec_dev->mbox_rmsg.trans_type = (int32_t)trans_type;
		sec_dev->mbox_rmsg.msg_len = (int32_t)msg_len;
		sec_dev->mbox_received = 1;
		wake_up(&waitq);
	}

out:
	return;
}
#endif

#if !defined(CONFIG_TCC_IPI_PROTOCOL) &&                                       \
	(defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC750X))
static void sec_msg_sent(struct mbox_client *client, void *message, int32_t r)
{
	if (r) {
		ELOG("Message could not be sent: %d\n", r);
	}
}
#endif

#if !IS_ENABLED(CONFIG_TCC_IPI_PROTOCOL)
static struct mbox_chan *sec_request_channel(struct platform_device *pdev,
					     const char *name)
{
	struct mbox_client *client = NULL;
	struct mbox_chan *channel = NULL;

	client = devm_kzalloc(&pdev->dev, sizeof(*client), GFP_KERNEL);
	if (client == NULL) {
		channel = NULL;
		goto out;
	}

	client->dev = &pdev->dev;
	client->rx_callback = sec_msg_received;
	client->knows_txdone = (bool)false;
	client->dev->init_name = name;
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC750X)
	client->tx_done = sec_msg_sent;
	client->tx_block = false;
	client->tx_tout = CLIENT_MBOX_TX_TIMEOUT;
#else
	client->tx_done = NULL;
	client->tx_block = (bool)true;
	client->tx_tout = CLIENT_MBOX_TX_TIMEOUT;
#endif

	channel = mbox_request_channel_byname(client, name);
	if (IS_ERR(channel)) {
		ELOG("Failed to request %s channel\n", name);
		channel = NULL;
		goto out;
	}

out:
	return channel;
}
#endif

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = sec_open,
	.release = sec_release,
	.unlocked_ioctl = sec_ioctl,
	.compat_ioctl = sec_ioctl,
	.poll = sec_poll,
	.llseek = generic_file_llseek,
};

static int32_t sec_probe(struct platform_device *pdev)
{
	int32_t result = 0;
	struct sec_device *sec_dev = NULL;

#if IS_ENABLED(CONFIG_TCC_IPI_PROTOCOL)
	struct tcc_ipi_register_dat mbox_prot_dat = {
		0,
	};
	char prot_id[3] = { 'H', 'S', 'M' };
	memcpy(&(mbox_prot_dat.id[0]), prot_id, sizeof(prot_id));
	mbox_prot_dat.rx_callback = sec_msg_received_callback;
	mbox_prot_dat.pdev = pdev;
#endif

	sec_dev =
		devm_kzalloc(&pdev->dev, sizeof(struct sec_device), GFP_KERNEL);
	if (sec_dev == NULL) {
		ELOG("Cannot alloc sec device..\n");
		result = -ENOMEM;
		goto out;
	}
	result = alloc_chrdev_region(&sec_dev->devnum, 0, 1, DEVICE_NAME);
	if (result != 0) {
		ELOG("alloc_chrdev_region error %d\n", result);
		goto out;
	}

	cdev_init(&sec_dev->hsm_cdev, &fops);
	sec_dev->hsm_cdev.owner = THIS_MODULE;
	result = cdev_add(&sec_dev->hsm_cdev, sec_dev->devnum, 1);
	if (result != 0) {
		ELOG("cdev_add error %d\n", result);
		goto cdev_add_error;
	}

	sec_dev->hsm_class = class_create(THIS_MODULE, pdev->name);
	if (IS_ERR(sec_dev->hsm_class)) {
		result = -EPROBE_DEFER;
		ELOG("class_create error %d\n", result);
		goto class_create_error;
	}

	sec_dev->hsm_device = device_create(sec_dev->hsm_class, &pdev->dev,
					    sec_dev->devnum, NULL, DEVICE_NAME);
	if (IS_ERR(sec_dev->hsm_device)) {
		result = -EPROBE_DEFER;
		ELOG("device_create error %d\n", result);
		goto device_create_error;
	}
#if IS_ENABLED(CONFIG_TCC_IPI_PROTOCOL)
	sec_dev->mbox_client =
		of_parse_phandle(pdev->dev.of_node, "tcc_ipi", 0);
	sec_dev->index = tcc_ipi_register(sec_dev->mbox_client, &mbox_prot_dat);
	if (sec_dev->index < 0) {
		result = -EPROBE_DEFER;
		ELOG("tcc_ipi_register error: %d\n", result);
		goto mbox_request_channel_error;
	}
#else
	sec_dev->mbox_ch = sec_request_channel(pdev, pdev->name);
	if (sec_dev->mbox_ch == NULL) {
		result = -EPROBE_DEFER;
		ELOG("sec_request_channel error: %d\n", result);
		goto mbox_request_channel_error;
	}
#endif

	codebase = of_iomap(pdev->dev.of_node, 0);
	cfgbase = of_iomap(pdev->dev.of_node, 1);
	DLOG("code(%p) cfg(%p)\n", codebase, cfgbase);

	sec_dev->vaddr = dma_alloc_coherent(&pdev->dev, MBOX_DMA_SIZE,
					    &sec_dev->paddr, GFP_KERNEL);
	if (sec_dev->vaddr == NULL) {
		ELOG("DMA alloc fail: %d\n", result);
		result = -ENOMEM;
		goto out;
	}
	result = sec_set_device(sec_get_device_id(pdev->name), sec_dev);
	if (result != 0) {
		ELOG("Can't find device name %s %d\n", pdev->name, result);
	}
	DLOG("Successfully probe registered %s\n", pdev->name);
	goto out;
#if (0)
dma_alloc_error:
	mbox_free_channel(sec_dev->mbox_ch);
#endif
mbox_request_channel_error:
	device_destroy(sec_dev->hsm_class, sec_dev->devnum);

device_create_error:
	class_destroy(sec_dev->hsm_class);

class_create_error:
	cdev_del(&sec_dev->hsm_cdev);

cdev_add_error:
	unregister_chrdev_region(sec_dev->devnum, 1);

out:
	return result;
}

static int32_t sec_remove(struct platform_device *pdev)
{
	struct sec_device *sec_dev = NULL;

	sec_dev = sec_get_device(sec_get_device_id(pdev->name));
	if (sec_dev == NULL) {
		ELOG("sec_get_device fail\n");
		goto out;
	}

	dma_free_coherent(&pdev->dev, MBOX_DMA_SIZE, sec_dev->vaddr,
			  sec_dev->paddr);
#if (0)
	mbox_free_channel(sec_dev->mbox_ch);
#endif
	device_destroy(sec_dev->hsm_class, sec_dev->devnum);
	class_destroy(sec_dev->hsm_class);
	cdev_del(&sec_dev->hsm_cdev);
	unregister_chrdev_region(sec_dev->devnum, 1);

out:
	return 0;
}

// clang-format off
static struct platform_driver secdriver = {
	.probe = sec_probe,
	.remove = sec_remove,
	.driver = {
		   .name = "tcc_sec_ipc",
		   .of_match_table = sec_ipc_dt_id,
		   },
};

// clang-format on

static int32_t __init sec_init(void)
{
	return platform_driver_register(&secdriver);
}

fs_initcall(sec_init)

	static void __exit sec_exit(void)
{
	platform_driver_unregister(&secdriver);
}

module_exit(sec_exit);

MODULE_DESCRIPTION("Telechips SEC IPC interface");
MODULE_AUTHOR("Telechips co.");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
