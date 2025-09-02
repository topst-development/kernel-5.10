// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef INCLUDED_SEC_DRV
#define INCLUDED_SEC_DRV

#include <linux/types.h>

/**
 * Syntatic structure used for commnication with A53 <-> M4, A7, R5.
 */
struct sec_segment {
	uint32_t cmd; /**< Multi ipc command */
	ulong data_addr; /**< Data to send */
	uint32_t size; /**< Size of data */
	ulong rdata_addr; /**< Data to receive */
	uint32_t rsize; /**< Size of rdata */
	uint32_t device_id; /**< Type A53, A7, R5, M4  */
};

#define HSM_MBOX_LOCATION_DATA (0x0400U)
#define HSM_MBOX_LOCATION_CMD (0x0000U)
#define HSM_MBOX_CMD_MAX_LEN (8U)
#define HSM_MBOX_ID_HSM (0x4D5348) /* 0x4D5348 = "HSM" */

#define HSM_MBOX_CID_A76 (0x7200U)
#define HSM_MBOX_CID_A55 (0x5300U)
#define HSM_MBOX_CID_SC (0xD300U)
#define HSM_MBOX_CID_HSM (0xA000U)
#define HSM_MBOX_CID_R5 (0xFF00U)

#define HSM_MBOX_BSID_BL0 (0x0042U)
#define HSM_MBOX_BSID_BL1 (0x0043U)
#define HSM_MBOX_BSID_BL3 (0x0045U)
#define HSM_MBOX_BSID_KERNEL (0x0046U)

#define HSM_MBOX_HSM_CMD0 (HSM_MBOX_CID_R5 | HSM_MBOX_BSID_KERNEL)

/* Mailbox driver instance. */
#define MBOX_DEV_M4 (0U)
#define MBOX_DEV_A7 (1U)
#define MBOX_DEV_R5 (2U)
#define MBOX_DEV_A53 (3U)
#define MBOX_DEV_A72 (4U)
#define MBOX_DEV_HSM (5U)
#define MBOX_DEV_MAX (6U)
#define MBOX_DEV_INVALID (7U)

#define TCC_MBOX_MAX_SIZE (512U)

#define MBOX_NONE_DMA (0U)
#define MBOX_DMA (1U)

/**
 * @defgroup spdrv Multi IPC Device Driver
 *  Channel, communicating with between SP, SP API, and demux.
 * @addtogroup spdrv
 * @{
 * @file tcc_sp_ipc.h This file contains Secure Process (SP) device driver
 *interface, called by demux driver.
 */

// clang-format off
#define SEC_IOCTL_MAGIC 'S'

/** Only used during SP firmware development. */
#define SEC_RESET				_IO(SEC_IOCTL_MAGIC, 0)

/** Sends and receives #sp_segment to SP. */
#define SEC_SEND_CMD    _IOWR(SEC_IOCTL_MAGIC, 1, struct sec_segment)

/** Gets an event when poll operation is awaken. */
#define SEC_GET_EVENTS    _IOR(SEC_IOCTL_MAGIC, 2, struct sec_segment)

/** Gets an event when poll operation is awaken. */
#define SEC_GET_EVT_INFO    _IOR(SEC_IOCTL_MAGIC, 5, struct sec_segment)
// clang-format on

/**
 * This macro makes a SP command to communicate with SP. The command range
 * must be defined as follows. Take a look at the following examples.
 * - HWDMX Magic number: 0. The range: 0x0000 ~ 0x0FFF
 *	- \#define HWDMX_START_CMD SP_CMD(MAGIC_NUM, 0x001)
 * - NAGRA Magic number: 1, e.g. 0x1000 ~ 0x1FFF
 *	- \#define NOCS_EXE_CIPHER_CMD SP_CMD(MAGIC_NUM, 0x001)
 * - Cisco Magic number: 2, e.g. 0x2000 ~ 0x2FFF
 * - VMX Magic number: 3, e.g. 0x3000 ~ 0x3FFF
 */
#define SEC_IPC_CMD(magic_num, cmd) (((magic_num & 0xF) << 12) | (cmd & 0xFFF))

/**
 * This macro makes an event that SP firmware notifies. The event comes
 * along with SP command, residing in 16 MSB of a 32bit SP command. These
 * are an example how an event can be defined.
 * - HWDMX_RESERVED1	\#define HWDMX_RESERVED1 SP_EVENT(magic_num, 1)
 * - HWDMX_RESERVED2	\#define HWDMX_RESERVED2 SP_EVENT(magic_num, 2)
 * - HWDMX_RESERVED16	\#define HWDMX_RESERVED16 SP_EVENT(magic_num, 16)
 * @attention Up to 16 of events can be assigned.
 */
#define SEC_IPC_EVENT(magic_num, event)                                        \
	((1 << (event + 15)) | ((magic_num & 0xF) << 12))

void sec_set_callback(int32_t (*dmx_cb)(int32_t cmd, void *rdata,
					int32_t size));
int32_t sec_sendrecv_cmd(uint32_t device_id, uint32_t cmd, void *data,
			 uint32_t size, void *rdata, uint32_t rsize);

/** @} */

#endif /* INCLUDED_SEC_DRV */
