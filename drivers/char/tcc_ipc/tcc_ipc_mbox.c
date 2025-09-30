// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/cdev.h>
#include <linux/of_device.h>

#include <linux/firmware/tcc_ipi.h>
#include <linux/mailbox_client.h>
#include <linux/tcc_ipc.h>
#include "tcc_ipc_typedef.h"
#include "tcc_ipc_os.h"
#include "tcc_ipc_mbox.h"

IPC_INT32 ipc_mailbox_send(
			struct ipc_device *ipc_dev,
			struct tcc_ipc_data *ipc_msg)
{
	IPC_INT32 ret;

	if ((ipc_dev != NULL) && (ipc_msg != NULL)) {
		struct IpcHandler *ipc_handle = &ipc_dev->ipc_handler;
		struct tcc_ipi_msg mdata;

		mdata.cmd_len = IPC_CMD_SIZE;
		mdata.data_len = ipc_msg->data_len;
		mdata.cmd = kzalloc(sizeof(u32)* MBOX_MSG_CMD_MAX_LEN, GFP_KERNEL);
		if(mdata.cmd != NULL)
		{	
			mdata.data_buf = kzalloc(sizeof(u32)* MBOX_MSG_DAT_MAX_LEN, GFP_KERNEL);
			if(mdata.data_buf != NULL)
			{
				(void)memcpy((void *)mdata.cmd,
					(const void *)ipc_msg->cmd,
					(size_t)IPC_CMD_SIZE * sizeof(unsigned int));
		
				(void)memcpy((void *)mdata.data_buf,
					(const void *)ipc_msg->data,
					(size_t)ipc_msg->data_len * sizeof(unsigned int));	
			

				d2printk((ipc_dev), ipc_dev->dev,
					"data size(%d)\n", mdata.data_len);

				mutex_lock(&ipc_handle->mboxMutex);

				ret = tcc_ipi_send_data(ipc_dev->mbox_client, &mdata, ipc_dev->mbox_index);
				if (ret < 0) {
					d2printk((ipc_dev), ipc_dev->dev,
						"mbox send error(%d)\n", ret);
				} else {
					ret = IPC_SUCCESS;
				}
				kfree(mdata.cmd);
				kfree(mdata.data_buf);
				mutex_unlock(&ipc_handle->mboxMutex);
			} else {
				kfree(mdata.cmd);
				(void)pr_err(
				"[ERROR][%s]%s: Mailbox kalloc faile\n",
				(const IPC_CHAR *)LOG_TAG, __func__);
				ret = IPC_ERR_ARGUMENT;
			}
		} else {
			(void)pr_err(
				"[ERROR][%s]%s: Mailbox kalloc faile\n",
				(const IPC_CHAR *)LOG_TAG, __func__);
			ret = IPC_ERR_ARGUMENT;
		}
	} else {
		(void)pr_err("[ERROR][%s]%s: Invalid Arguements\n",
			(const IPC_CHAR *)LOG_TAG,
			__func__);
		ret = IPC_ERR_ARGUMENT;
	}

	return ret;
}

int mbox_register(struct platform_device *pdev,
					ipc_mbox_receive handler)
{
	int ret;

	if ((pdev != NULL)&&(handler != NULL)) {
		
		struct ipc_device *ipc_dev = platform_get_drvdata(pdev);
		struct tcc_ipi_register_dat mbox_prot_dat = {0,};
		const char ipc_id[TCC_IPI_PROTOCOL_ID_MAX_LEN] = "IPC";
		(void)memcpy(&(mbox_prot_dat.id[0]), &ipc_id, TCC_IPI_PROTOCOL_ID_MAX_LEN); 
		mbox_prot_dat.rx_callback = handler;
		mbox_prot_dat.pdev = pdev;
		ipc_dev->mbox_client = of_parse_phandle(pdev->dev.of_node, "tcc_ipi", 0);
		ipc_dev->mbox_index = tcc_ipi_register(ipc_dev->mbox_client, &mbox_prot_dat);

		if (ipc_dev->mbox_index < 0) {
			(void)pr_err("[ERROR][%s]%s: mbox register error\n",
			(const IPC_CHAR *)LOG_TAG,
			__func__);
			ret = -EPROBE_DEFER;
		} else {
			ret = 0;
		}
		
	}
	else {
		ret = -ENOMEM;
	}
	return ret;
}

