/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note OR MIT  */

/*
 * (C) COPYRIGHT 2020-2023 Arm Limited or its affiliates. All rights reserved.
 */

/*
 * Public interface for the handler from Partition Manager messages.
 */

#ifndef _MALI_GPU_RG_AW_MSG_BUFF_H_
#define _MALI_GPU_RG_AW_MSG_BUFF_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/iopoll.h>

/* Message specific defs of the Partition Manager HW */
#define PTM_MESSAGE_SIZE 0x0020
#define PTM_INCOMING_MESSAGE0 0x0000
#define PTM_INCOMING_MESSAGE1 0x0004
#define PTM_OUTGOING_MESSAGE_STATUS 0x0008
#define PTM_OUTGOING_MESSAGE0 0x000c
#define PTM_OUTGOING_MESSAGE1 0x0010
#define PTM_OUTGOING_MSG_STATUS_MASK 0x01

/* Buffer sizes defs for the message passing */
#define ARB_TO_VM_BUFF_SIZE 3
#define VM_TO_ARB_BUFF_SIZE 1

/* Timeout to poll MSG registers */
#define MSG_REG_POLL_SLEEP_US 100
#define PTM_SEND_RETRY_LIMIT 1000

#define PTM_MESSAGE_OFFSET(aw_id) ((aw_id)*PTM_MESSAGE_SIZE)

#define MAX_AW_NUM 16
#define RETRY_MASK_RESET 0xFFFFFFFF

/**
 * struct msg_buff - Fifo Buffer to store PTM messages
 * @buff_lock:		Spinlock to be hold while filling or reading a message
 * @msg:		Payload of the messages
 * @head:		Head of the fifo
 * @tail:		Tail of the fifo
 * @size:		Size of the fifo
 * @retry_count:	Number of retry attempts to send this message
 *
 * This is a simple implementation of a circular buffer where overwriting is
 * possible and the first element will be discarded if a new element needs to be
 * added while buffer is full.
 */
struct msg_buff {
	spinlock_t buff_lock;
	uint64_t *msg;
	int head;
	int tail;
	int size;
	int retry_count;
};

/**
 * struct ptm_msgs - Structure containing message buffers and associated info
 *
 * @msgs:			Pointer to an array of msg_buff containing the
 *				messages
 * @n_buffers:			The number of buffers in msgs
 * @mask:			Flags to indicate which buffers have msgs
 *				available to be read
 * @can_retry_mask:		Flags to indicate which buffers have yet not
 *				reached the maximum number of sending retries
 * @last_aw_id_processed:	The last member of msgs which was processed
 */
struct ptm_msgs {
	struct msg_buff *msgs;
	int n_buffers;
	unsigned long mask;
	unsigned long can_retry_mask;
	uint32_t last_aw_id_processed;
};

/**
 * struct ptm_msg_handler - Structure containing necessary data for storing and
 * retrieving Partition Manager messages
 *
 * @dev:		The device to which this handler belongs
 * @send_msgs:		Send message buffers
 * @recv_msgs:		Receive message buffers
 * @base_addr:		The base address of the PTM_MESSAGE registers
 * @ptm_send_wq:	Send work queue for delayed writes
 * @ptm_send_work:	Delayed work struct for outgoing messages
 * @msg_send_mutex:	Mutex to protect multiple threads trying to send
 *			messages from the same sending buffer
 *
 */
struct ptm_msg_handler {
	struct device *dev;
	struct ptm_msgs send_msgs;
	struct ptm_msgs recv_msgs;
	void __iomem *base_addr;
	struct workqueue_struct *ptm_send_wq;
	struct work_struct ptm_send_work;
	struct mutex msg_send_mutex;
};

/**
 * struct msg_worker_params - Parameters for a message worker
 * see (ptm_msg_process_msgs)
 *
 *  @data:	Opaque data for worker
 *  @aw_id:	AW ID being processed
 *
 */
struct msg_worker_params {
	void *data;
	int aw_id;
};

static inline int ptm_msg_buff_read(struct ptm_msgs *msgs, uint32_t buff_id, uint64_t *msg);
static inline bool ptm_msg_buff_retry(struct ptm_msgs *msgs, uint32_t buff_id);
static inline void ptm_msg_write(struct ptm_msg_handler *msg_handler, uint32_t msg_id,
				 uint64_t *message);
static inline void ptm_msg_process_msgs(struct ptm_msgs *msgs, void *data,
					void (*worker)(struct msg_worker_params *));

/**
 * send_msg_worker() - Send worker function for use with send_msg_worker
 *
 * @params:	Pointer to a msg_worker_params struct containing the data
 *		needed to process a message
 *
 * If the PTM_OUTGOING_MESSAGE_STATUS register is clear, write the message to
 * the send registers
 */
static inline void send_msg_worker(struct msg_worker_params *params)
{
	uint64_t message = 0;
	uint32_t message_status;
	struct ptm_msg_handler *msg_handler;
	int aw;
	int error = 0;
	bool exceeded_retries = false;

	if (WARN_ON(!params) || WARN_ON(!params->data))
		return;

	msg_handler = (struct ptm_msg_handler *)params->data;
	aw = params->aw_id;

	if (WARN_ON(aw >= MAX_AW_NUM))
		return;

	exceeded_retries = ptm_msg_buff_retry(&msg_handler->send_msgs, aw);
	if (exceeded_retries) {
		/* If the maximum number of retries is exceeded we need to clear
		 * the can_retry_mask so that this buffer is not tried anymore
		 * after the current retry cycle.
		 */
		clear_bit(aw, &msg_handler->send_msgs.can_retry_mask);
		dev_warn(msg_handler->dev, "Exceeded retries to send %llx to AW%d\n", message, aw);
	}

	mutex_lock(&msg_handler->msg_send_mutex);

	/* Check if the message to send is actually still in the send buffer */
	if (!test_bit(aw, &msg_handler->send_msgs.mask)) {
		dev_dbg(msg_handler->dev, "Send buffer %d empty\n", aw);
		goto cleanup_mutex;
	}

	message_status = ioread32(msg_handler->base_addr + PTM_MESSAGE_OFFSET(aw) +
				  PTM_OUTGOING_MESSAGE_STATUS) &
			 PTM_OUTGOING_MSG_STATUS_MASK;

	if (message_status == 0) {
		/* Read the outgoing message */
		error = ptm_msg_buff_read(&msg_handler->send_msgs, aw, &message);
		if (!error)
			ptm_msg_write(msg_handler, aw, &message);
		else
			dev_err(msg_handler->dev, "AW%d send buffer read failed\n", aw);
	} else
		dev_dbg(msg_handler->dev, "AW%d send buffer busy. Leaving in queue\n", aw);

cleanup_mutex:
	mutex_unlock(&msg_handler->msg_send_mutex);
}

/**
 * ptm_send_message_worker() - Worker thread for sending messages to
 *				a PTM_MESSAGE pipe
 *
 * @data:	Work contained within the device data.
 *
 * The PTM send register was busy so this work item was scheduled.
 * Check the AW send_msgs and send any valid messages
 */
static inline void ptm_send_message_worker(struct work_struct *data)
{
	struct ptm_msg_handler *msg_handler;

	if (WARN_ON(!data))
		return;

	msg_handler = container_of(data, struct ptm_msg_handler, ptm_send_work);

	while (msg_handler->send_msgs.can_retry_mask & msg_handler->send_msgs.mask) {
		/* Process pending outgoing messages */
		ptm_msg_process_msgs(&msg_handler->send_msgs, msg_handler, send_msg_worker);

		usleep_range(MSG_REG_POLL_SLEEP_US >> 1, MSG_REG_POLL_SLEEP_US);
	}

	/* Resetting the can_retry_mask so that next time this worker runs it at
	 * least try one more time the buffers that already reached the maximum
	 * number of retries.
	 */
	msg_handler->send_msgs.can_retry_mask = RETRY_MASK_RESET;
}

/**
 * ptm_msg_handler_init() - Initialise a ptm_message_handler structure
 *
 * @msg_handler:	Pointer to the message handler
 * @dev:		Device to which this handler belongs
 * @base_addr:		PTM_MESSAGE base address
 * @n_buffers:		Number of send/receive buffers
 * @send_buff_size:	Size of the buffers used for sending messages
 * @rcv_buff_size:	Size of the buffers used for receiving messages
 *
 * Populate the msg_handler structure, allocating appropriately sized buffers.
 * Also managed its own work queue.
 *
 * ptm_msg_handler_destroy() needs to be called in order to free resources
 * allocated in this funciton.
 *
 * Return: 0 if successfully or an error code
 */
static inline int ptm_msg_handler_init(struct ptm_msg_handler *msg_handler, struct device *dev,
				       void __iomem *base_addr, int n_buffers, int send_buff_size,
				       int rcv_buff_size)
{
	int ptm_msg_size = sizeof(struct msg_buff) * n_buffers;
	int i = 0;

	*msg_handler = (struct ptm_msg_handler) {
		.dev = dev,
		.send_msgs = {
			.msgs = devm_kzalloc(dev, ptm_msg_size, GFP_KERNEL),
			.n_buffers = n_buffers,
			.mask = 0,
			.can_retry_mask = RETRY_MASK_RESET,
			.last_aw_id_processed = 0
		},
		.recv_msgs = {
			.msgs = devm_kzalloc(dev, ptm_msg_size, GFP_KERNEL),
			.n_buffers = n_buffers,
			.mask = 0,
			.can_retry_mask = RETRY_MASK_RESET,
			.last_aw_id_processed = 0,
		},
		.base_addr = base_addr
	};

	for (i = 0; i < n_buffers; i++) {
		struct msg_buff *msg_buff = &(msg_handler->recv_msgs.msgs[i]);
		int msg_buffer_size = sizeof(uint64_t) * (rcv_buff_size + 1);

		spin_lock_init(&(msg_buff->buff_lock));
		msg_buff->msg = devm_kzalloc(dev, msg_buffer_size, GFP_KERNEL);
		msg_buff->size = rcv_buff_size + 1;

		msg_buff = &(msg_handler->send_msgs.msgs[i]);
		msg_buffer_size = sizeof(uint64_t) * (send_buff_size + 1);
		spin_lock_init(&(msg_buff->buff_lock));
		msg_buff->msg = devm_kzalloc(dev, msg_buffer_size, GFP_KERNEL);
		msg_buff->size = send_buff_size + 1;
	}

	/* Initialize the PTM send work queue and delayed work */
	msg_handler->ptm_send_wq = alloc_ordered_workqueue("ptm_send_wq", WQ_HIGHPRI);
	if (msg_handler->ptm_send_wq)
		INIT_WORK(&msg_handler->ptm_send_work, ptm_send_message_worker);
	else {
		dev_err(dev, "Failed to allocate the AW send work queue.\n");
		return -EINVAL;
	}

	mutex_init(&msg_handler->msg_send_mutex);

	return 0;
}

/**
 * ptm_msg_handler_destroy() - Destroy the contents of a ptm_message_handler
 *				structure
 *
 * @msg_handler:	Pointer to the message handler
 *
 * Destroy any dynamically generated members of the msg_handler structure and
 * destroy the send work queue.
 */
static inline void ptm_msg_handler_destroy(struct ptm_msg_handler *msg_handler)
{
	struct device *dev;
	int n_buffers;
	int i;

	if (!msg_handler)
		return;

	/* If dev is not initialised then it means ptm_msg_handler_init() has
	 * not been called on msg_handler.
	 */
	dev = msg_handler->dev;
	n_buffers = msg_handler->recv_msgs.n_buffers;
	if (dev) {
		destroy_workqueue(msg_handler->ptm_send_wq);

		for (i = 0; i < n_buffers; i++) {
			struct msg_buff *msg_buff = &(msg_handler->recv_msgs.msgs[i]);
			devm_kfree(dev, msg_buff->msg);

			msg_buff = &(msg_handler->send_msgs.msgs[i]);
			devm_kfree(dev, msg_buff->msg);
		}

		devm_kfree(dev, msg_handler->send_msgs.msgs);
		devm_kfree(dev, msg_handler->recv_msgs.msgs);
	}
}

/**
 * ptm_msg_buff_read() - Read and clear the specified message buffer
 *
 * @msgs:	Pointer to a ptm_msgs struct containing send or receive buffers
 * @buff_id:	Index from which to read the message
 * @msg:	Destination pointer for the retrieved message
 *
 * Read the buffer of the message specified by the index and clear the
 * corresponding bit in the mask to indicate that the buffer is empty.
 *
 * Return: 0 if successful or an error code
 */
static inline int ptm_msg_buff_read(struct ptm_msgs *msgs, uint32_t buff_id, uint64_t *msg)
{
	unsigned long flags;
	struct msg_buff *buffer;

	if (WARN_ON(!msgs) || WARN_ON(!msg) || buff_id >= (uint32_t)msgs->n_buffers)
		return -EINVAL;

	buffer = &msgs->msgs[buff_id];

	spin_lock_irqsave(&buffer->buff_lock, flags);
	/* If empty just return. */
	if (buffer->tail == buffer->head) {
		spin_unlock_irqrestore(&buffer->buff_lock, flags);
		return -EPERM;
	}
	*msg = buffer->msg[buffer->tail];
	buffer->msg[buffer->tail] = 0;
	buffer->tail = (buffer->tail + 1) % buffer->size;
	if (buffer->tail == buffer->head)
		clear_bit(buff_id, &msgs->mask);
	/* If we are taking a message from the buffer it means that it should be
	 * ready to send, so the retry can be reset.
	 */
	buffer->retry_count = 0;
	spin_unlock_irqrestore(&buffer->buff_lock, flags);

	return 0;
}

/**
 * ptm_msg_buff_write() - Write the message to the specified message buffer
 *
 * @msgs:	Pointer to a ptm_msgs struct containing send or receive buffers
 * @buff_id:	Index from which to write the message
 * @payload:	The message to be stored
 *
 * Store the the message in the buffer of the message specified by the index and
 * set the corresponding bit in the message bitmask to indicate that the message
 * is valid
 *
 * Return:
 * * 1    - if write was successful but overwrite happened
 * * 0    - if write was successful and no overwrite happened
 * * < 0  - if error
 */
static inline int ptm_msg_buff_write(struct ptm_msgs *msgs, uint32_t buff_id, uint64_t payload)
{
	unsigned long flags;
	int overwrite = 0;
	struct msg_buff *buffer;

	if (WARN_ON(!msgs) || buff_id >= (uint32_t)msgs->n_buffers)
		return -EINVAL;

	buffer = &msgs->msgs[buff_id];

	spin_lock_irqsave(&buffer->buff_lock, flags);
	/* Overwrite the first message if the buffer is full */
	if (((buffer->head + 1) % buffer->size) == buffer->tail) {
		buffer->tail = (buffer->tail + 1) % buffer->size;
		overwrite = 1;
	}
	buffer->msg[buffer->head] = payload;
	buffer->head = (buffer->head + 1) % buffer->size;
	set_bit(buff_id, &msgs->mask);
	spin_unlock_irqrestore(&buffer->buff_lock, flags);

	return overwrite;
}

/**
 * ptm_msg_buff_retry() - Increment and check the buffer retry counter
 *
 * @msgs:	Pointer to a ptm_msgs struct containing send or receive buffers
 * @buff_id:	Index from which to increment and check the retry counter
 *
 * Return: TRUE if reached the maximum number of retries or FALSE
 */
static inline bool ptm_msg_buff_retry(struct ptm_msgs *msgs, uint32_t buff_id)
{
	unsigned long flags;

	if (WARN_ON(!msgs) || buff_id >= (uint32_t)msgs->n_buffers)
		return -EINVAL;

	if (msgs->msgs[buff_id].retry_count >= PTM_SEND_RETRY_LIMIT)
		return true;

	spin_lock_irqsave(&msgs->msgs[buff_id].buff_lock, flags);
	msgs->msgs[buff_id].retry_count++;
	spin_unlock_irqrestore(&msgs->msgs[buff_id].buff_lock, flags);

	return false;
}

/**
 * ptm_msg_write() - Write a PTM_MESSAGE
 *
 * @msg_handler:	Pointer to the message handler
 * @msg_id:		Message index to target. It is equal to the AW_ID if
 *			targeting AW and zero if targeting RG.
 * @message:		64-bit message to send
 *
 * Send a message to the specified register related to the msg_id index. This is
 * simply a raw write and assumes that the status register has already been
 * checked.
 */
static inline void ptm_msg_write(struct ptm_msg_handler *msg_handler, uint32_t msg_id,
				 uint64_t *message)
{
	uint32_t message_lo = *message & U32_MAX;
	uint32_t message_hi = *message >> 32;

	iowrite32(message_lo,
		  msg_handler->base_addr + PTM_MESSAGE_OFFSET(msg_id) + PTM_OUTGOING_MESSAGE0);

	/* The PTM_OUTGOING_MESSAGE1 write must be last because it triggers
	 * the message copy and raises an interrupt on the recipient.
	 */
	iowrite32(message_hi,
		  msg_handler->base_addr + PTM_MESSAGE_OFFSET(msg_id) + PTM_OUTGOING_MESSAGE1);
}

/**
 * ptm_msg_read() - Reads the last message received
 *
 * @msg_handler:	Pointer to the message handler
 * @msg_id:		Message index to target. It is equal to the AW_ID if
 *			targeting AW and zero if targeting RG.
 * @message:		Destination pointer for the 64-bit message
 *
 * Receive a message from the specified register related to the msg_id index.
 * This is simply a raw read and assumes that the caller should clear any IRQ
 * related to this message if necessary.
 */
static inline void ptm_msg_read(struct ptm_msg_handler *msg_handler, uint32_t msg_id,
				uint64_t *message)
{
	if (!msg_handler || !message)
		return;

	*message = ioread32(msg_handler->base_addr + PTM_MESSAGE_OFFSET(msg_id) +
			    PTM_INCOMING_MESSAGE1);
	*message <<= 32;
	*message |= ioread32(msg_handler->base_addr + PTM_MESSAGE_OFFSET(msg_id) +
			     PTM_INCOMING_MESSAGE0);
}

/**
 * ptm_msg_flush_send_buffers() - Schedule a worker to flush the send buffers.
 * @msg_handler:	Pointer to the message handler
 *
 * Schedule a message send worker to flush the message sending buffers.
 */
static inline void ptm_msg_flush_send_buffers(struct ptm_msg_handler *msg_handler)
{
	if (!msg_handler)
		return;

	queue_work(msg_handler->ptm_send_wq, &msg_handler->ptm_send_work);
}

/**
 * ptm_msg_send() - Send message to the other end (AW or RG).
 *
 * @msg_handler:	Pointer to the message handler
 * @buff_id:		Message index to target. It is equal to the AW_ID if
 *			targeting AW and zero if targeting RG.
 *
 * Send a message payload to  the specified register related to the buff_id
 * index It could be AW or RG depending on the msg_handler passed.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static inline int ptm_msg_send(struct ptm_msg_handler *msg_handler, uint32_t buff_id)
{
	uint32_t msg_status;
	uint64_t payload;
	int error = 0;

	if (!msg_handler)
		return -EINVAL;

	mutex_lock(&msg_handler->msg_send_mutex);

	/* Check if the message to send is actually still in the send buffer */
	if (!test_bit(buff_id, &msg_handler->send_msgs.mask)) {
		dev_dbg(msg_handler->dev, "Send buffer %d empty\n", buff_id);
		goto cleanup_mutex;
	}

	msg_status = ioread32(msg_handler->base_addr + PTM_MESSAGE_OFFSET(buff_id) +
			      PTM_OUTGOING_MESSAGE_STATUS) &
		     PTM_OUTGOING_MSG_STATUS_MASK;

	if (msg_status == 0) {
		error = ptm_msg_buff_read(&msg_handler->send_msgs, buff_id, &payload);
		if (!error)
			ptm_msg_write(msg_handler, buff_id, &payload);
		else
			dev_err(msg_handler->dev, "Buffer%d send buffer read failed\n", buff_id);

		if (test_bit(buff_id, &msg_handler->send_msgs.mask))
			queue_work(msg_handler->ptm_send_wq, &msg_handler->ptm_send_work);
	} else {
		queue_work(msg_handler->ptm_send_wq, &msg_handler->ptm_send_work);
	}

cleanup_mutex:
	mutex_unlock(&msg_handler->msg_send_mutex);
	return error;
}

/**
 * ptm_msg_process_msgs() - Process valid messages in the specified buffer
 *
 * @msgs:	Pointer to a ptm_msgs struct containing send or receive buffers
 * @data:	An opaque pointer passed to the worker
 * @worker:	A worker function which is called for every valid message
 *
 * For every message in the buffer which is flagged as being valid, the worker
 * function is called. The prm_msgs structure records the last message
 * processed so that successive calls to this function result in fair
 * processing of messages. It is the worker function's responsibility
 * to retrieve the message with ptm_msg_buff_read() which will clear that
 * message's valid flag
 */
static inline void ptm_msg_process_msgs(struct ptm_msgs *msgs, void *data,
					void (*worker)(struct msg_worker_params *))
{
	uint32_t first_aw_id_to_process = msgs->last_aw_id_processed;
	uint32_t mask = 1 << first_aw_id_to_process;

	struct msg_worker_params params = { .data = data, .aw_id = first_aw_id_to_process };

	do {
		if (mask & msgs->mask & msgs->can_retry_mask) {
			(*worker)(&params);

			msgs->last_aw_id_processed = params.aw_id;
		}

		if (++params.aw_id >= MAX_AW_NUM) {
			mask = 1;
			params.aw_id = 0;
		} else
			mask <<= 1;
	} while ((uint32_t)params.aw_id != first_aw_id_to_process);
}

#endif
