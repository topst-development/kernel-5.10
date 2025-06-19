/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_IPC_TYPE_DEF_H
#define TCC_IPC_TYPE_DEF_H

#define LOG_TAG    ("TCC_IPC")

#define IPC_RXBUFFER_SIZE	(524288U) //(512U * 1024U)
#define IPC_TXBUFFER_SIZE	(512U)
#define IPC_MAX_WRITE_SIZE	(512U)

#define Hw1                     (0x00000002U)
#define Hw0                     (0x00000001U)

/* Extension feautre Bit*/
#define USE_NACK	(0x00000001U)


/* NACK Reason Bit */
#define NACK_BUF_FULL	(Hw0)
#define NACK_BUF_ERR	(Hw1)

#define IPC_CMD_SIZE	6
#define IPC_DATA_SIZE	128

typedef	char IPC_CHAR;
typedef	unsigned char IPC_UCHAR;
typedef	unsigned int IPC_UINT32;
typedef	int IPC_INT32;
typedef	unsigned long long IPC_UINT64;
typedef	unsigned long IPC_ULONG;
typedef	long IPC_LONG;

#ifndef	BITSET
#define BITSET(X, MASK)	\
	((X) |= (IPC_UINT32)(MASK))
#endif

#ifndef	BITSCLR
#define BITSCLR(X, SMASK, CMASK) \
	((X) = ((((IPC_UINT32)(X)) | ((IPC_UINT32)(SMASK))) & \
		~((IPC_UINT32)(CMASK))))
#endif

#ifndef	BITCSET
#define BITCSET(X, CMASK, SMASK) \
	((X) = ((((IPC_UINT32)(X)) & ~((IPC_UINT32)(CMASK))) | \
		((IPC_UINT32)(SMASK))))
#endif

#ifndef	BITCLR
#define	BITCLR(X, MASK)	\
	((X) &= ~((IPC_UINT32)(MASK)))
#endif

#define IPC_SUCCESS					(0)
#define IPC_ERR_COMMON				(-1)
#define IPC_ERR_BUSY				(-2)
#define IPC_ERR_NOTREADY			(-3)
#define IPC_ERR_TIMEOUT				(-4)
#define IPC_ERR_WRITE				(-5)
#define IPC_ERR_READ				(-6)
#define IPC_ERR_BUFFER				(-7)
#define IPC_ERR_ARGUMENT			(-8)
#define IPC_ERR_RECEIVER_NOT_SET	(-9)
#define IPC_ERR_RECEIVER_DOWN		(-10)
#define IPC_ERR_NACK_BUF_FULL		(-11)


typedef enum {
	CTL_CMD = 0x0000U,
	WRITE_CMD,
	MAX_CMD_TYPE
} IpcCmdType;

typedef enum {
	/* control command */
	IPC_OPEN = 0x0001U,
	IPC_CLOSE,
	IPC_SEND_PING,
	IPC_WRITE,
	IPC_ACK,
	IPC_NACK,
	MAC_CMD_ID
} IpcCmdID;

typedef enum {
	IPC_NULL = 0U,
	IPC_INIT,
	IPC_READY,
	IPC_MAX_STATUS
} IpcStatus;

typedef enum {
	IPC_BUF_NULL = 0U,
	IPC_BUF_READY,
	IPC_BUF_BUSY,
	IPC_BUF_MAX_STATUS
} IpcBufferStatus;

struct tcc_ipc_data {
	unsigned int    cmd[IPC_CMD_SIZE];
	unsigned int    data[IPC_DATA_SIZE];
	unsigned int    data_len;
};

typedef void (*ipc_receive_queue_t)
				(void *device_info,
				const struct tcc_ipc_data  *pMsg);

struct IPC_RINGBUF {
	IPC_UINT32	head; //read position
	IPC_UINT32	tail; //write position
	IPC_UINT32	maxBufferSize;
	IPC_UCHAR	*pBuffer;
};

struct ipc_wait_queue {
	wait_queue_head_t cmdQueue;
	IPC_UINT32 seqID;
	IPC_UINT32 condition;
	IPC_UINT32 result;
};

struct ipc_read_queue {
	wait_queue_head_t cmdQueue;
	IPC_UINT32 condition;
};

struct IpcBufferInfo {
	IPC_UCHAR *startAddr;
	IPC_UINT32 bufferSize;
	IpcBufferStatus	status;
};

struct ipc_receive_list {
	struct ipc_device *ipc_dev;
	struct tcc_ipc_data data;
	struct list_head	queue;
};

struct ipc_receiveQueue {
	struct kthread_worker  kworker;
	struct task_struct     *kworker_task;
	struct kthread_work    pump_messages;
	spinlock_t             rx_queue_lock;
	struct list_head       rx_queue;

	ipc_receive_queue_t  handler;
	void                   *handler_pdata;
};

struct IpcHandler {
	IpcStatus		ipcStatus;

	struct IpcBufferInfo	readBuffer;

	struct IPC_RINGBUF  readRingBuffer;

	IPC_UINT32	vTime;
	IPC_UINT32	vMin;

	tcc_ipc_ctl_param	setParam;

	struct ipc_receiveQueue receiveQueue[MAX_CMD_TYPE];
	struct ipc_wait_queue ipcWaitQueue[MAX_CMD_TYPE];
	struct ipc_read_queue ipcReadQueue;

	IPC_UINT32 seqID;
	IPC_UINT32 openSeqID;
	IPC_UINT64 requestConnectTime;
	IPC_UCHAR *tempWbuf;

	IPC_UINT32 sendNACK;

	spinlock_t spinLock; /* commont spinlock */
	struct mutex rMutex; /* read mutex*/
	struct mutex wMutex; /* write mutex */
	struct mutex mboxMutex; /* mbox mutex */
	struct mutex rbufMutex; /* read buffer mutex */
};

struct ipc_device {
	struct platform_device	*pdev;
	struct device *dev;
	struct cdev ipc_cdev;
	struct class *ipc_class;
	dev_t devnum;

	const IPC_CHAR *name;
	struct device_node *mbox_client;
	int	mbox_index;
	struct IpcHandler ipc_handler;
	atomic_t ipc_available;
	IPC_INT32       debug_level;
};

extern IPC_INT32 ipc_verbose_mode;

#define eprintk(dev, msg, ...)	\
	((void)dev_err(dev, \
		"[ERROR][%s]%s: " pr_fmt(msg), \
		(const IPC_CHAR *)LOG_TAG, \
		__func__, \
		##__VA_ARGS__))

#define wprintk(dev, msg, ...)	\
	((void)dev_warn(dev, \
		"[WARN][%s]%s: " pr_fmt(msg), \
		(const IPC_CHAR *)LOG_TAG, \
		__func__, \
		##__VA_ARGS__))

#define iprintk(dev, msg, ...)	\
	((void)dev_info(dev, \
		"[INFO][%s]%s: " pr_fmt(msg), \
		(const IPC_CHAR *)LOG_TAG, \
		__func__, \
		##__VA_ARGS__))

#define dprintk(dev, msg, ...)	\
	{ if (ipc_verbose_mode == (IPC_INT32)1) \
		{ (void)dev_info(dev, \
			"[INFO][%s]%s: " pr_fmt(msg), \
			(const IPC_CHAR *)LOG_TAG, \
			__func__, \
			##__VA_ARGS__); } }

#define d1printk(ipc_dev, dev, msg, ...) \
	{ if (ipc_dev->debug_level > (IPC_INT32)0) \
		{ (void)dev_info(dev, \
			"[DEBUG][%s]%s: " pr_fmt(msg), \
			(const IPC_CHAR *)LOG_TAG, \
			__func__, \
			##__VA_ARGS__); } }

#define d2printk(ipc_dev, dev, msg, ...) \
	{ if (ipc_dev->debug_level > (IPC_INT32)1) \
		{ (void)dev_info(dev, \
			"[DEBUG][%s]%s: " pr_fmt(msg), \
			(const IPC_CHAR *)LOG_TAG,	\
			__func__, \
			##__VA_ARGS__); } }

#endif /* __TCC_IPC_TYPE_DEF_H__ */

