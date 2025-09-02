// SPDX-License-Identifier: GPL-2.0-or-late
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/debugfs.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/scatterlist.h>
#include <linux/mailbox/mailbox-tcc.h>
#include <linux/soc/telechips/tcc_sc_protocol.h>
#include <linux/proc_fs.h>

#define CREATE_TRACE_POINTS
#include <trace/events/tcc_sc_fw.h>

#if defined(CONFIG_ARCH_TCC807X)
#define TCC_SC_CID_MAIN		(0x01U)
#define TCC_SC_CID_SUB		(0x02U)
#define TCC_SC_CID_SC		(0xD0U)
#else /* CONFIG_ARCH_TCC805X */
#define TCC_SC_CID_MAIN		(0x72U)
#define TCC_SC_CID_SUB		(0x53U)
#define TCC_SC_CID_SC		(0xD3U)
#endif
#define TCC_SC_CID_HSM		(0xA0U)

#define TCC_SC_BSID_BL0		(0x42U)
#define TCC_SC_BSID_BL1		(0x43U)
#define TCC_SC_BSID_BL2		(0x44U)
#define TCC_SC_BSID_BL3		(0x45U)
#define TCC_SC_BSID_BL4		(0x46U)

#define TCC_SC_CMD_REQ_GET_VERSION	(0x00000000U)
#define TCC_SC_CMD_REQ_GET_PROT_INFO	(0x00000001U)
#define TCC_SC_PROT_ID_MMC		(0x00000002U)
#define TCC_SC_CMD_REQ_STOR_IO		(0x00000004U)
#define TCC_SC_CMD_REQ_STOR_IO_READ	(0x1U)
#define TCC_SC_CMD_REQ_STOR_IO_WRITE	(0x0U)
#define TCC_SC_CMD_REQ_MMC_REQ		(0x00000005U)
#define TCC_SC_CMD_REQ_UFS_REQ		(0x00000006U)
#define TCC_SC_CMD_REQ_GPIO_CONFIG	(0x00000010U)
#define TCC_SC_CMD_REQ_OTP_READ		(0x00000014U)

#define TCC_SC_MAX_CMD_LENGTH		(8U)
#define TCC_SC_MAX_DATA_LENGTH		(128U)

#define TCC_SC_TX_TIMEOUT_MS		(10000UL)
#define TCC_SC_RX_TIMEOUT_MS		(10000U)

#define MAX_TCC_SC_FW_XFERS		(5)

#define TCC_SC_CMD_MMC_PART_NUM_MASK	(0xFFU << 0U)
#define TCC_SC_CMD_MMC_REL_WR_MASK		((unsigned int)0x1 << 31U)


struct tcc_sc_fw_cmd {
	u8 bsid;
	u8 cid;
	u16 uid;
	u32 cmd;
	u32 args[6];
};

struct tcc_sc_fw_sync_ctx {
	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_cmd *response;
	struct completion done;
};

struct tcc_sc_fw_info {
	struct device *dev;

	u8 bsid;
	u8 cid;
	u16 uid;

	struct mbox_client cl;
	struct mbox_chan *chan;
	u32 max_rx_timeout_ms;

	struct list_head rx_pending;
	struct list_head xfers_list;
	struct tcc_sc_fw_xfer *xfers;

	spinlock_t lock;
	spinlock_t rx_lock;
	spinlock_t xfers_lock;

	struct tcc_sc_fw_version version;
	struct tcc_sc_fw_handle *handle;

	bool suspend;
};

#define DESC_LEN (55U)			/* 80 cols - 26 prefix + 1 null */

static char desc_scfw[DESC_LEN];	/* v%u.%u.%u-g%07x%s */

#define cl_to_tcc_sc_fw_info(c)	((struct tcc_sc_fw_info *) \
			container_of((c), const struct tcc_sc_fw_info, cl))
#define msg_to_tcc_sc_fw_xfer(c)	((struct tcc_sc_fw_xfer *) \
			container_of((c), struct tcc_sc_fw_xfer, tx_mssg))

static int tcc_sc_fw_check_suspend(const struct tcc_sc_fw_info *info)
{
	int ret = 0;
	if (info->suspend == true) {
		ret = -EBUSY;
	}

	return ret;
}

static void tcc_sc_fw_print_command(const struct tcc_sc_fw_info *info,
				    const struct tcc_sc_fw_cmd *cmd)
{
	if ((info != NULL) && (cmd != NULL)) {
		(void)dev_info(info->dev,
			"======== SC FW CMD DUMP ========\n");
		(void)dev_info(info->dev,
			"BSID 0x%08x | CID 0x%08x\n",
			 cmd->bsid, cmd->cid);
		(void)dev_info(info->dev,
			"UID 0x%08x | CMD 0x%08x\n",
			 cmd->uid, cmd->cmd);
		(void)dev_info(info->dev,
			"ARG0 0x%08x | ARG1 0x%08x\n",
			 cmd->args[0], cmd->args[1]);
		(void)dev_info(info->dev,
			"ARG2 0x%08x | ARG3 0x%08x\n",
			 cmd->args[2], cmd->args[3]);
		(void)dev_info(info->dev,
			"ARG4 0x%08x | ARG5 0x%08x\n",
			 cmd->args[4], cmd->args[5]);
	}
}

static struct tcc_sc_fw_info *handle_to_tcc_sc_fw_info
				(const struct tcc_sc_fw_handle *handle)
{
	struct tcc_sc_fw_info *ret;

	if (handle == NULL) {
		ret = NULL;
	} else {
		ret = (struct tcc_sc_fw_info *)handle->priv;
	}

	return ret;
}

static void tcc_sc_fw_set_xfer_status(struct tcc_sc_fw_xfer *xfer, u8 status)
{
	if (xfer != NULL) {
		xfer->status = status;
		trace_tcc_sc_fw_xfer_status(xfer);
	}
}

static u8 tcc_sc_fw_get_xfer_status(const struct tcc_sc_fw_xfer *xfer)
{
	u8 status = 0;

	if (xfer != NULL) {
		status = xfer->status;
	}

	return status;
}

static struct tcc_sc_fw_xfer *get_tcc_sc_fw_xfer(struct tcc_sc_fw_info *info)
{
	struct tcc_sc_fw_xfer *xfer;
	unsigned long flags;

	spin_lock_irqsave(&info->xfers_lock, flags);
	if (list_empty(&info->xfers_list)) {
		spin_unlock_irqrestore(&info->xfers_lock, flags);
		(void)dev_err(info->dev,
			"[ERROR][TCC_SC_FW] Failed to get xfer. All xfer is in use.\n");
		xfer = NULL;
	} else {
		xfer = list_first_entry(&info->xfers_list, struct tcc_sc_fw_xfer, node);
		list_del_init(&xfer->node);
		spin_unlock_irqrestore(&info->xfers_lock, flags);

		xfer->complete = NULL;
		xfer->args = NULL;
		xfer->tx_mssg.data_len = 0;
		xfer->tx_mssg.cmd_len = 0;
	}

	return xfer;
}

static void put_tcc_sc_fw_xfer(struct tcc_sc_fw_xfer *xfer,
					struct tcc_sc_fw_info *info)
{
	unsigned long flags;

	spin_lock_irqsave(&info->xfers_lock, flags);
	list_add_tail(&xfer->node, &info->xfers_list);
	spin_unlock_irqrestore(&info->xfers_lock, flags);
}

const struct tcc_sc_fw_handle *tcc_sc_fw_get_handle(struct device_node *np)
{
	const struct platform_device *pdev = of_find_device_by_node(np);
	const struct tcc_sc_fw_info *info = NULL;
	const struct tcc_sc_fw_handle *ret = NULL;

	if (pdev != NULL) {
		info = platform_get_drvdata(pdev);
	}

	if (info != NULL) {
		ret = info->handle;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(tcc_sc_fw_get_handle);

static void tcc_sc_fw_halt_xfer(struct tcc_sc_fw_info *info,
					struct tcc_sc_fw_xfer *xfer)
{
	unsigned long flags;
	struct tcc_sc_fw_xfer *list;
	u8 status;
	bool peek_data;
	int ret = 0;

	spin_lock_irqsave(&xfer->lock, flags);

	status = tcc_sc_fw_get_xfer_status(xfer);
	if (status == TCC_SC_FW_XFER_STAT_TX_PEND) {
		tcc_sc_fw_set_xfer_status(xfer, TCC_SC_FW_XFER_STAT_HALT);
		spin_unlock_irqrestore(&xfer->lock, flags);

		(void)dev_err(info->dev,
			"[ERROR][TCC_SC_FW] Halt tx pending xfer(%p)\n",
			xfer);
		ret = -1;
	} else if ((status == TCC_SC_FW_XFER_STAT_TX_START) ||
		(status == TCC_SC_FW_XFER_STAT_RX_PEND)) {
		tcc_sc_fw_set_xfer_status(xfer, TCC_SC_FW_XFER_STAT_HALT);
		spin_unlock_irqrestore(&xfer->lock, flags);
	} else {
		(void)dev_warn(info->dev,
			"[WARN][TCC_SC_FW] Wrong xfer(%p) status 0x%x (%s)\n",
			xfer, status, __func__);
		spin_unlock_irqrestore(&xfer->lock, flags);

		tcc_sc_fw_print_command(info,
			(struct tcc_sc_fw_cmd *)xfer->tx_mssg.cmd);

		ret = -1;
	}

	if (ret == 0) {
		spin_lock_irqsave(&info->rx_lock, flags);
		if (list_empty(&info->rx_pending)) {
			spin_unlock_irqrestore(&info->rx_lock, flags);
			ret = -1;
		}
	}

	if (ret == 0) {
		list_for_each_entry(list, &info->rx_pending, node) {
			if (((list->tx_mssg.cmd[0] & 0xFFFF0000UL) ==
				(xfer->tx_mssg.cmd[0] & 0xFFFF0000UL)) &&
				(list->tx_mssg.cmd[1] == xfer->tx_mssg.cmd[1])) {

				list_del_init(&list->node);
				peek_data = info->chan->mbox->ops->peek_data(info->chan);

				if (status == TCC_SC_FW_XFER_STAT_RX_PEND) {
					(void)dev_err(info->dev,
							"[ERROR][TCC_SC_FW] Halt xfer, remove xfer(%p) from rx_pending list, put to pool (rx_cmd_fifo: %s)\n",
							xfer, peek_data ? "not empty" : "empty");

					spin_lock_irqsave(&xfer->lock, flags);
					tcc_sc_fw_set_xfer_status(xfer, TCC_SC_FW_XFER_STAT_IDLE);
					spin_unlock_irqrestore(&xfer->lock, flags);

					put_tcc_sc_fw_xfer(xfer, info);
				} else { /* TCC_SC_FW_XFER_STAT_TX_START */
					(void)dev_err(info->dev,
							"[ERROR][TCC_SC_FW] Halt xfer, remove xfer(%p) from rx_pending list (rx_cmd_fifo: %s)\n",
							xfer, peek_data ? "not empty" : "empty");
				}
				break;
			}
		}
		spin_unlock_irqrestore(&info->rx_lock, flags);
	}
}

static s32 tcc_sc_fw_xfer_async(struct tcc_sc_fw_info *info,
					struct tcc_sc_fw_xfer *xfer)
{
	s32 ret;
	const struct device *dev = info->dev;
	unsigned long flags;
	u8 status;

	trace_tcc_sc_fw_start_xfer(xfer);

	spin_lock_irqsave(&info->lock, flags);
	if (info->uid >= 0xFFFFU) {
		info->uid = 0;
	} else {
		info->uid++;
	}
	spin_unlock_irqrestore(&info->lock, flags);

	xfer->tx_mssg.cmd[0] &= ~(0xFFFF0000U);
	xfer->tx_mssg.cmd[0] |= (((u32)info->uid & 0xFFFFU) << 16U);
	xfer->tx_mssg.flags = 0;

	spin_lock_irqsave(&xfer->lock, flags);

	status = tcc_sc_fw_get_xfer_status(xfer);
	if (status != TCC_SC_FW_XFER_STAT_IDLE) {
		(void)dev_warn(dev,
			"[WARN][TCC_SC_FW] Wrong xfer(%p) status 0x%x (%s)\n",
			xfer, status, __func__);
	}
	tcc_sc_fw_set_xfer_status(xfer, TCC_SC_FW_XFER_STAT_TX_PEND);

	spin_unlock_irqrestore(&xfer->lock, flags);

	ret = mbox_send_message(info->chan, &xfer->tx_mssg);
	if (ret < 0) {
		(void)dev_err(dev,
			"[ERROR][TCC_SC_FW] Failed to send command (%d)\n",
			ret);
	}

	return ret;
}

static void tcc_sc_fw_xfer_sync_complete(void *args, void * msg)
{
	struct tcc_sc_fw_sync_ctx *ctx = (struct tcc_sc_fw_sync_ctx *)args;
	const struct tcc_sc_fw_xfer *xfer;

	BUG_ON(ctx == NULL);

	xfer = ctx->xfer;
	BUG_ON(xfer == NULL);

	if (ctx->response != NULL) {
		(void)memcpy(ctx->response, xfer->rx_mssg.cmd,
		       sizeof(struct tcc_sc_fw_cmd));
	}

	complete(&ctx->done);
}

static s32 tcc_sc_fw_xfer_sync(struct tcc_sc_fw_info *info,
		struct tcc_sc_fw_xfer *xfer, struct tcc_sc_fw_cmd *res_cmd)
{
	s32 ret;
	unsigned long timeout;
	struct tcc_sc_fw_sync_ctx ctx;

	init_completion(&ctx.done);
	ctx.response = res_cmd;
	ctx.xfer = xfer;

	xfer->complete = tcc_sc_fw_xfer_sync_complete;
	xfer->args = &ctx;

	ret = tcc_sc_fw_xfer_async(info, xfer);
	if (ret < 0) {
		(void)dev_err(info->dev,
				"[ERROR][TCC_SC_FW] failed xfer_async xfer(%p)\n",
				xfer);

	} else {
		/* And we wait for the response. */
		timeout = msecs_to_jiffies(info->max_rx_timeout_ms);
		/* Wait for Buffer Read Ready interrupt */
		if (wait_for_completion_timeout(&ctx.done, timeout) == 0UL) {
			(void)dev_err(info->dev,
				"[ERROR][TCC_SC_FW] Sync xfer(%p) timeout occur\n",
				xfer);
			tcc_sc_fw_halt_xfer(info, xfer);

			ret = -ETIMEDOUT;
		} else {
			ret = 0;
		}
	}
	return ret;
}

static void tcc_sc_fw_tx_prepare(struct mbox_client *cl, void *msg)
{
	unsigned long flags;
	struct tcc_mbox_msg *tx_mbox_msg = msg;
	struct tcc_sc_fw_info *info = cl_to_tcc_sc_fw_info(cl);
	struct tcc_sc_fw_xfer *xfer = msg_to_tcc_sc_fw_xfer(tx_mbox_msg);
	u8 status;

	if (xfer == NULL) {
		(void)dev_err(info->dev,
			"[ERROR][TCC_SC_FW] %s %d is NULL\n", __func__, __LINE__);
	} else {
		spin_lock_irqsave(&xfer->lock, flags);
		status = tcc_sc_fw_get_xfer_status(xfer);
		if (status == TCC_SC_FW_XFER_STAT_HALT) {
			spin_unlock_irqrestore(&xfer->lock, flags);

			xfer->tx_mssg.flags = TCC_MBOX_FLAG_SKIP_XFER;
			(void)dev_warn(info->dev,
				"[WARN][TCC_SC_FW] xfer(%p) is halted. Skip transfer message\n",
				xfer);
		} else if (status == TCC_SC_FW_XFER_STAT_TX_PEND) {
			tcc_sc_fw_set_xfer_status(xfer, TCC_SC_FW_XFER_STAT_TX_START);
			spin_unlock_irqrestore(&xfer->lock, flags);

			spin_lock_irqsave(&info->rx_lock, flags);
			list_add_tail(&xfer->node, &info->rx_pending);
			spin_unlock_irqrestore(&info->rx_lock, flags);
		} else {
			spin_unlock_irqrestore(&xfer->lock, flags);
		}
	}
}

static void tcc_sc_fw_tx_done(struct mbox_client *cl, void *msg, int r)
{
	struct tcc_mbox_msg *tx_mbox_msg = msg;
	struct tcc_sc_fw_info *info = cl_to_tcc_sc_fw_info(cl);
	struct tcc_sc_fw_xfer *xfer = msg_to_tcc_sc_fw_xfer(tx_mbox_msg);
	unsigned long flags;
	u8 status;

	spin_lock_irqsave(&xfer->lock, flags);
	status = tcc_sc_fw_get_xfer_status(xfer);
	if (status == TCC_SC_FW_XFER_STAT_HALT) {
		(void)dev_warn(info->dev,
			"[WARN][TCC_SC_FW] put halted xfer(%p) to pool (%d)\n",
			xfer, r);

		tcc_sc_fw_set_xfer_status(xfer, TCC_SC_FW_XFER_STAT_IDLE);
		spin_unlock_irqrestore(&xfer->lock, flags);

		put_tcc_sc_fw_xfer(xfer, info);
	} else if (status == TCC_SC_FW_XFER_STAT_TX_START){
		tcc_sc_fw_set_xfer_status(xfer, TCC_SC_FW_XFER_STAT_RX_PEND);
		spin_unlock_irqrestore(&xfer->lock, flags);
	} else {
		spin_unlock_irqrestore(&xfer->lock, flags);
	}
}

static void tcc_sc_fw_rx_callback(struct mbox_client *cl, void *mssg)
{
	struct tcc_sc_fw_info *info;
	const struct device *dev;
	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_xfer *match = NULL;
	struct tcc_mbox_msg *mbox_msg = mssg;
	struct tcc_mbox_msg *rx_mbox_msg;
	unsigned long flags;

	if ((mssg != NULL) && (cl != NULL)) {
		info = cl_to_tcc_sc_fw_info(cl);
		dev = info->dev;

		spin_lock_irqsave(&info->rx_lock, flags);
		if (list_empty(&info->rx_pending) == 0) {
			list_for_each_entry(xfer, &info->rx_pending, node) {
				if (((xfer->tx_mssg.cmd[0] & 0xFFFF0000UL) ==
					(mbox_msg->cmd[0] & 0xFFFF0000UL)) &&
					(xfer->tx_mssg.cmd[1] == mbox_msg->cmd[1])) {
					list_del_init(&xfer->node);
					match = xfer;
					break;
				}
			}
		}
		spin_unlock_irqrestore(&info->rx_lock, flags);

		if (match != NULL) {
			rx_mbox_msg = &match->rx_mssg;
			rx_mbox_msg->cmd_len = 0U;
			rx_mbox_msg->data_len = 0U;

			if ((mbox_msg->cmd_len == match->rx_cmd_buf_len) &&
					(mbox_msg->data_len <= match->rx_data_buf_len)) {

				rx_mbox_msg->cmd_len = mbox_msg->cmd_len;
				(void)memcpy(rx_mbox_msg->cmd, mbox_msg->cmd,
				       (size_t) rx_mbox_msg->cmd_len * 4UL);

				rx_mbox_msg->data_len = mbox_msg->data_len;
				if (rx_mbox_msg->data_len > 0U) {
					(void)memcpy(rx_mbox_msg->data_buf,
							mbox_msg->data_buf,
					       (size_t) (rx_mbox_msg->data_len * 4UL));
				}

				if (match->complete != NULL) {
					match->complete(match->args, (void *)rx_mbox_msg->cmd);
				}

				spin_lock_irqsave(&xfer->lock, flags);
				tcc_sc_fw_set_xfer_status(xfer, TCC_SC_FW_XFER_STAT_IDLE);
				spin_unlock_irqrestore(&xfer->lock, flags);

				put_tcc_sc_fw_xfer(xfer, info);

				trace_tcc_sc_fw_done_xfer(xfer);

				dev_dbg(dev,
					"[DEBUG][TCC_SC_FW] Complete command rx\n");
			} else {
				(void)dev_err(dev,
					"[ERROR][TCC_SC_FW] Unable to handle cmd %d (expected %d) data %d (max %d)\n",
					mbox_msg->cmd_len, match->rx_cmd_buf_len,
					mbox_msg->data_len, match->rx_data_buf_len);

			}
		} else {
			trace_tcc_sc_fw_rx_invalid_message(mbox_msg);
			(void)dev_err(dev,
				"[ERROR][TCC_SC_FW] Invalid response\n");
		}
	}
}

static void *tcc_sc_fw_cmd_request_mmc_cmd(
				const struct tcc_sc_fw_handle *handle,
				struct tcc_sc_fw_mmc_cmd *cmd,
				struct tcc_sc_fw_mmc_data *data,
				void (*mmc_complete)(void *args, void *msg),
				void *args)
{
	struct tcc_sc_fw_info *info = NULL;
	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_cmd req_cmd = { 0, };
	struct scatterlist *sg;
	dma_addr_t addr;
	s32 ret = -EINVAL, i;
	u32 len;
	void *xfer_handle;

	if (handle != NULL) {
		info = handle_to_tcc_sc_fw_info(handle);
		if (info != NULL) {
			ret = tcc_sc_fw_check_suspend(info);
		}
	}

	if (ret == 0) {
		trace_tcc_sc_fw_start_mmc_req(cmd, data);
		xfer = get_tcc_sc_fw_xfer(info);
		if (xfer == NULL) {
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		(void)memset(xfer->rx_mssg.cmd, 0, xfer->rx_cmd_buf_len);

		req_cmd.bsid = info->bsid;
		req_cmd.cid = info->cid;
		req_cmd.cmd = TCC_SC_CMD_REQ_MMC_REQ;
		req_cmd.args[0] = cmd->opcode;
		req_cmd.args[1] = cmd->arg;
		req_cmd.args[2] = cmd->flags;
		req_cmd.args[3] =
			(cmd->part_num & TCC_SC_CMD_MMC_PART_NUM_MASK) |
			(cmd->rel_wr & TCC_SC_CMD_MMC_REL_WR_MASK);
		req_cmd.args[4] = 0;
		req_cmd.args[5] = 0;

		(void)memcpy(xfer->tx_mssg.cmd, &req_cmd, sizeof(struct tcc_sc_fw_cmd));
		xfer->tx_mssg.cmd_len =
		    (u32)(sizeof(struct tcc_sc_fw_cmd) / sizeof(u32));

		if (data != NULL) {
			xfer->tx_mssg.data_buf[0] = data->blksz;
			xfer->tx_mssg.data_buf[1] = data->blocks;
			xfer->tx_mssg.data_buf[2] = data->flags;
			if (0 <= data->sg_count) {
				xfer->tx_mssg.data_buf[3] = (u32)data->sg_count;
			}
			for_each_sg((data->sg), (sg), data->sg_count, i) {
				addr = sg_dma_address(sg);
				len = sg_dma_len(sg);
				if (addr <= UINT_MAX) {
					xfer->tx_mssg.data_buf[4U + ((u32) i * 2U)] =
						(u32)addr;
				}
				xfer->tx_mssg.data_buf[5U + ((u32) i * 2U)] = len;
			}
			xfer->tx_mssg.data_len = (u32)(4U + ((u32) i * 2U));
		} else {
			xfer->tx_mssg.data_len = 0;
		}

		xfer->complete = mmc_complete;
		xfer->args = args;
		ret = tcc_sc_fw_xfer_async(info, xfer);
	}

	if (ret < 0) {
		xfer_handle = NULL;
	} else {
		xfer_handle = (void *)xfer;
	}

	return xfer_handle;
}

static s32 tcc_sc_fw_cmd_get_mmc_prot_info(
			   const struct tcc_sc_fw_handle *handle,
			   struct tcc_sc_fw_prot_mmc *mmc_info)
{
	struct tcc_sc_fw_info *info;
	const struct device *dev;
	struct tcc_sc_fw_xfer *xfer = NULL;
	struct tcc_sc_fw_cmd req_cmd = { 0, }, res_cmd = {
	0,};
	s32 ret = -EINVAL;

	if ((handle != NULL) && (mmc_info != NULL)) {
		info = handle_to_tcc_sc_fw_info(handle);
		if (info != NULL) {
			ret = tcc_sc_fw_check_suspend(info);
		}
	}

	if (ret == 0) {
		dev = info->dev;
		xfer = get_tcc_sc_fw_xfer(info);
		if (xfer == NULL) {
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		req_cmd.bsid = info->bsid;
		req_cmd.cid = info->cid;
		req_cmd.cmd = TCC_SC_CMD_REQ_GET_PROT_INFO;
		req_cmd.args[0] = TCC_SC_PROT_ID_MMC;

		(void)memcpy(xfer->tx_mssg.cmd, &req_cmd, sizeof(struct tcc_sc_fw_cmd));
		xfer->tx_mssg.cmd_len = (u32)sizeof(struct tcc_sc_fw_cmd) /
		    (u32)sizeof(u32);

		xfer->tx_mssg.data_len = 0;

		(void)memset(xfer->rx_mssg.cmd, 0, xfer->rx_cmd_buf_len);

		ret = tcc_sc_fw_xfer_sync(info, xfer, &res_cmd);
		if (ret != 0) {
			(void)dev_err(dev,
				"[ERROR][TCC_SC_FW] Failed to send mbox (%d)\n",
				ret);
		} else {
			if ((res_cmd.bsid != info->bsid)
			    || (res_cmd.cid != (u8)TCC_SC_CID_SC)) {
				(void)dev_err(dev,
					"[ERROR][TCC_SC_FW] Receive NAK for CMD 0x%x (BSID 0x%x CID 0x%x)\n",
					req_cmd.cmd, res_cmd.bsid, res_cmd.cid);
				ret = -ENODEV;
			} else {
				(void)memcpy((void *)mmc_info, (void *)res_cmd.args,
				       sizeof(struct tcc_sc_fw_prot_mmc));
			}
		}
	}

	return ret;
}

static void *tcc_sc_fw_cmd_request_ufs_cmd(const struct tcc_sc_fw_handle *handle,
				struct tcc_sc_fw_ufs_cmd *sc_cmd,
				void (*ufs_complete)(void *args, void *msg), void *args)
{
	struct tcc_sc_fw_info *info = NULL;
	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_cmd req_cmd = { 0, }, res_cmd = { 0, };
	struct scatterlist *sg;
	void *xfer_handle;
	dma_addr_t addr;
	s32 ret = -EINVAL, i;
	u32 len;

	if (handle != NULL) {
		info = handle_to_tcc_sc_fw_info(handle);
		if (info != NULL) {
			ret = tcc_sc_fw_check_suspend(info);
		}
	}

	if (ret == 0) {
		xfer = get_tcc_sc_fw_xfer(info);
		if (xfer == NULL) {
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		(void)memset(xfer->rx_mssg.cmd, 0, xfer->rx_cmd_buf_len);

		req_cmd.bsid = info->bsid;
		req_cmd.cid = info->cid;
		req_cmd.cmd = TCC_SC_CMD_REQ_UFS_REQ;
		req_cmd.args[0] = sc_cmd->datsz;
		if (0 <= sc_cmd->sg_count) {
			req_cmd.args[1] = (u32)sc_cmd->sg_count;
		}
		req_cmd.args[2] = sc_cmd->lba;
		req_cmd.args[3] = sc_cmd->lun;
		req_cmd.args[4] = sc_cmd->tag;
		req_cmd.args[5] = sc_cmd->dir;

		(void)memcpy(xfer->tx_mssg.cmd, &req_cmd, sizeof(struct tcc_sc_fw_cmd));
		xfer->tx_mssg.cmd_len =
		    (u32)(sizeof(struct tcc_sc_fw_cmd) / sizeof(u32));

		xfer->tx_mssg.data_buf[0] = sc_cmd->cdb0;
		xfer->tx_mssg.data_buf[1] = sc_cmd->cdb1;
		xfer->tx_mssg.data_buf[2] = sc_cmd->cdb2;
		xfer->tx_mssg.data_buf[3] = sc_cmd->cdb3;

		if ((sc_cmd->dir == 0xeU) || (sc_cmd->dir == 0xfU))
		{
			sc_cmd->sg_count = 0;
		}

		for_each_sg((sc_cmd->sg), (sg), sc_cmd->sg_count, i) {
			addr = sg_dma_address(sg);
			len = sg_dma_len(sg);
			if (addr <= UINT_MAX) {
				xfer->tx_mssg.data_buf[4U + ((u32) i * 2U)] =
					(u32)addr;
			}
			xfer->tx_mssg.data_buf[5U + ((u32) i * 2U)] = len;
		}

		xfer->tx_mssg.data_len = (u32)(4U + ((u32) i * 2U));

		xfer->complete = ufs_complete;
		xfer->args = args;
		if ((sc_cmd->dir == 0xeU) || (sc_cmd->dir == 0xfU)) {
			dev_dbg(info->dev,
				"[INFO][TCC_SC_FW] ufs sync msg\n");
			ret = tcc_sc_fw_xfer_sync(info, xfer, &res_cmd);
			if (ret != 0) {
				(void)dev_err(info->dev,
					"[ERROR][TCC_SC_FW] Failed to send mbox (%d)\n",
						ret);
			} else if ((res_cmd.bsid != info->bsid)
					|| (res_cmd.cid != (u8)TCC_SC_CID_SC)) {
				(void)dev_err(info->dev,
						"[ERROR][TCC_SC_FW] Receive NAK for CMD 0x%x (BSID 0x%x CID 0x%x)\n",
						req_cmd.cmd, res_cmd.bsid, res_cmd.cid);
				ret = -ENODEV;
			} else {
				/*Do Nothing*/
			}
		} else {
			ret = tcc_sc_fw_xfer_async(info, xfer);
		}
	}

	if (ret < 0) {
		xfer_handle = NULL;
	} else {
		xfer_handle = (void *)xfer;
	}

	return xfer_handle;
}

static s32 tcc_sc_fw_cmd_get_revision(struct tcc_sc_fw_info *info)
{
	const struct device *dev = info->dev;
	struct tcc_sc_fw_xfer *xfer = get_tcc_sc_fw_xfer(info);
	struct tcc_sc_fw_cmd req_cmd = { 0, };
	struct tcc_sc_fw_cmd res_cmd = { 0, };
	s32 ret = -ENOMEM;

	if (xfer != NULL) {
		req_cmd.bsid = info->bsid;
		req_cmd.cid = info->cid;
		req_cmd.cmd = TCC_SC_CMD_REQ_GET_VERSION;

		(void)memcpy(xfer->tx_mssg.cmd, &req_cmd, sizeof(struct tcc_sc_fw_cmd));
		xfer->tx_mssg.cmd_len = (u32)sizeof(struct tcc_sc_fw_cmd)
				    / (u32)sizeof(u32);

		xfer->tx_mssg.data_len = 0;

		(void)memset(xfer->rx_mssg.cmd, 0, xfer->rx_cmd_buf_len);

		ret = tcc_sc_fw_xfer_sync(info, xfer, &res_cmd);
		if (ret != 0) {
			(void)dev_err(dev,
				"[ERROR][TCC_SC_FW] Failed to send mbox (%d)\n",
				ret);
		} else {
			if ((res_cmd.bsid != info->bsid)
			    || (res_cmd.cid != (u8)TCC_SC_CID_SC)) {
				(void)dev_err(dev,
					"[ERROR][TCC_SC_FW] Receive NAK for CMD 0x%x (BSID 0x%x CID 0x%x)\n",
					req_cmd.cmd, res_cmd.bsid, res_cmd.cid);
				ret = -ENODEV;
			} else {
				(void)memcpy((void *)&info->version, (void *)res_cmd.args,
				       sizeof(struct tcc_sc_fw_version));
			}
		}
	}

	return ret;
}

static s32 tcc_sc_fw_cmd_request_gpio_cmd(const struct tcc_sc_fw_handle *handle,
					  uint32_t address, uint32_t bit_number,
					  uint32_t width, uint32_t value)
{
	struct tcc_sc_fw_info *info;
	const struct device *dev;
	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_cmd req_cmd = { 0, };
	struct tcc_sc_fw_cmd res_cmd = { 0, };
	s32 ret = -EINVAL;

	if (handle != NULL) {
		info = handle_to_tcc_sc_fw_info(handle);
		if (info != NULL) {
			ret = tcc_sc_fw_check_suspend(info);
		}
	}

	if (ret == 0) {
		dev = info->dev;
		xfer = get_tcc_sc_fw_xfer(info);
		if (xfer == NULL) {
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		req_cmd.bsid = info->bsid;
		req_cmd.cid = info->cid;
		req_cmd.cmd = TCC_SC_CMD_REQ_GPIO_CONFIG;
		req_cmd.args[0] = address + 0x60000000U;
		req_cmd.args[1] = bit_number;
		req_cmd.args[2] = width;
		req_cmd.args[3] = value;
		req_cmd.args[4] = 0;
		req_cmd.args[5] = 0;

		(void)memcpy(xfer->tx_mssg.cmd, &req_cmd, sizeof(struct tcc_sc_fw_cmd));
		xfer->tx_mssg.cmd_len = (u32)sizeof(struct tcc_sc_fw_cmd)
		    / (u32)sizeof(u32);

		xfer->tx_mssg.data_len = 0;

		(void)memset(xfer->rx_mssg.cmd, 0, xfer->rx_cmd_buf_len);

		ret = tcc_sc_fw_xfer_sync(info, xfer, &res_cmd);
		if (ret != 0) {
			(void)dev_err(dev,
				"[ERROR][TCC_SC_FW] Failed to send mbox (GPIO Command) (%d)\n",
				ret);
		} else {
			if ((res_cmd.bsid != info->bsid)
			    || (res_cmd.cid != (u8)TCC_SC_CID_SC)) {
				(void)dev_err(dev,
					"[ERROR][TCC_SC_FW] Receive NAK for CMD 0x%x (BSID 0x%x CID 0x%x)\n",
					req_cmd.cmd, res_cmd.bsid, res_cmd.cid);
				ret = -ENODEV;
			}
		}
	}

	return ret;
}

static s32 tcc_sc_fw_cmd_request_gpio_no_res_cmd(
				const struct tcc_sc_fw_handle *handle,
				uint32_t address, uint32_t bit_number,
				uint32_t width, uint32_t value)
{
	struct tcc_sc_fw_info *info;
	const struct device *dev;
	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_cmd req_cmd = { 0, };
	s32 ret = -EINVAL;

	if (handle != NULL) {
		info = handle_to_tcc_sc_fw_info(handle);
		if (info != NULL) {
			ret = tcc_sc_fw_check_suspend(info);
		}
	}

	if (ret == 0) {
		dev = info->dev;
		xfer = get_tcc_sc_fw_xfer(info);
		if (xfer == NULL) {
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		req_cmd.bsid = info->bsid;
		req_cmd.cid = info->cid;
		req_cmd.cmd = TCC_SC_CMD_REQ_GPIO_CONFIG;
		req_cmd.args[0] = address + 0x60000000U;
		req_cmd.args[1] = bit_number;
		req_cmd.args[2] = width;
		req_cmd.args[3] = value;
		req_cmd.args[4] = 0;
		req_cmd.args[5] = 0;

		(void)memcpy(xfer->tx_mssg.cmd, &req_cmd, sizeof(struct tcc_sc_fw_cmd));
		xfer->tx_mssg.cmd_len = (u32)sizeof(struct tcc_sc_fw_cmd)
		    / (u32)sizeof(u32);

		xfer->tx_mssg.data_len = 0;

		(void)memset(xfer->rx_mssg.cmd, 0, xfer->rx_cmd_buf_len);

		ret = tcc_sc_fw_xfer_async(info, xfer);
		if (ret < 0) {
			(void)dev_err(dev,
				"[ERROR][TCC_SC_FW] Failed to send mbox (GPIO Command) (%d)\n",
				ret);
		}
	}

	return ret;
}

static s32 tcc_sc_fw_cmd_get_tfuse_cmd(const struct tcc_sc_fw_handle *handle,
				struct tcc_sc_fw_tfuse_cmd *cmd, uint32_t offset)
{
	struct tcc_sc_fw_info *info;
	const struct device *dev;
	struct tcc_sc_fw_xfer *xfer;
	struct tcc_sc_fw_cmd req_cmd = { 0, };
	struct tcc_sc_fw_cmd res_cmd = { 0, };
	s32 ret = -EINVAL;

	if (handle != NULL) {
		info = handle_to_tcc_sc_fw_info(handle);
		if (info != NULL) {
			ret = tcc_sc_fw_check_suspend(info);
		}
	}

	if (ret == 0) {
		dev = info->dev;
		xfer = get_tcc_sc_fw_xfer(info);
		if (xfer == NULL) {
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		req_cmd.bsid = info->bsid;
		req_cmd.cid = info->cid;
		req_cmd.uid = info->uid;
		req_cmd.cmd = TCC_SC_CMD_REQ_OTP_READ;
		req_cmd.args[0] = offset;

		(void)memcpy(xfer->tx_mssg.cmd, &req_cmd, sizeof(struct tcc_sc_fw_cmd));
		xfer->tx_mssg.cmd_len = (u32)sizeof(struct tcc_sc_fw_cmd)
		    / (u32)sizeof(u32);

		xfer->tx_mssg.data_len = 0;

		(void)memset(xfer->rx_mssg.cmd, 0, xfer->rx_cmd_buf_len);
		ret = tcc_sc_fw_xfer_sync(info, xfer, &res_cmd);
		if (ret != 0) {
			(void)dev_err(dev,
				"[ERROR][TCC_SC_FW] Failed to send mbox (%d)\n",
				ret);
		} else {
			if ((res_cmd.bsid != info->bsid)
			    || (res_cmd.cid != (u8)TCC_SC_CID_SC)) {
				(void)dev_err(dev,
					"[ERROR][TCC_SC_FW] Receive NAK for CMD 0x%x (BSID 0x%x CID 0x%x)\n",
					req_cmd.cmd, res_cmd.bsid, res_cmd.cid);
				ret = -ENODEV;
			} else {
				cmd->cmd = res_cmd.cmd;
				cmd->resp[0] = res_cmd.args[0];
				cmd->resp[1] = res_cmd.args[1];
				cmd->resp[2] = res_cmd.args[2];
			}
		}
	}

	return ret;
}

static void tcc_sc_fw_halt_cmd(const struct tcc_sc_fw_handle *handle,
			   void *xfer_handle)
{
	struct tcc_sc_fw_info *info;

	BUG_ON(handle == NULL);
	BUG_ON(xfer_handle == NULL);

	info = handle_to_tcc_sc_fw_info(handle);
	tcc_sc_fw_halt_xfer(info, (struct tcc_sc_fw_xfer *)xfer_handle);
}


static const struct of_device_id tcc_sc_fw_of_match[3] = {
	{.compatible = "telechips,tcc805x-sc-fw"},
	{.compatible = "telechips,tcc807x-sc-fw"},
	{},
};

MODULE_DEVICE_TABLE(of, tcc_sc_fw_of_match);

static struct tcc_sc_fw_mmc_ops sc_fw_mmc_ops = {
	.request_command = tcc_sc_fw_cmd_request_mmc_cmd,
	.halt_cmd = tcc_sc_fw_halt_cmd,
	.prot_info = tcc_sc_fw_cmd_get_mmc_prot_info,
};

static struct tcc_sc_fw_ufs_ops sc_fw_ufs_ops = {
	.request_command = tcc_sc_fw_cmd_request_ufs_cmd,
	.halt_cmd = tcc_sc_fw_halt_cmd,
};

static struct tcc_sc_fw_gpio_ops sc_fw_gpio_ops = {
	.request_gpio = tcc_sc_fw_cmd_request_gpio_cmd,
	.request_gpio_no_res = tcc_sc_fw_cmd_request_gpio_no_res_cmd,
};

static struct tcc_sc_fw_tfuse_ops sc_fw_tfuse_ops = {
	.get_tfuse = tcc_sc_fw_cmd_get_tfuse_cmd,
};

static s32 tcc_sc_fw_alloc_xfer_list(struct device *dev,
				struct tcc_sc_fw_info *info)
{
	s32 ret = -ENOMEM;
	s32 i;
	struct tcc_sc_fw_xfer *xfers;

	xfers = devm_kzalloc(dev,
		(unsigned int)MAX_TCC_SC_FW_XFERS * sizeof(*xfers), GFP_KERNEL);
	if (xfers != NULL) {
		info->xfers = xfers;
		for (i = 0; i < MAX_TCC_SC_FW_XFERS; i++) {
			/* for error handle */
			ret = 0;

			list_add_tail(&xfers->node, &info->xfers_list);

			xfers->status = TCC_SC_FW_XFER_STAT_IDLE;

			xfers->tx_cmd_buf_len = TCC_SC_MAX_CMD_LENGTH;
			xfers->tx_data_buf_len = TCC_SC_MAX_DATA_LENGTH;
			xfers->tx_mssg.cmd =
			    devm_kzalloc(dev, sizeof(u32) * xfers->tx_cmd_buf_len,
					 GFP_KERNEL);
			if (xfers->tx_mssg.cmd == NULL) {
				(void)dev_err(dev,
					"[ERROR][TCC_SC_FW] Failed to allocate memory for Tx cmd buffer\n");
				ret = -ENOMEM;
			}

			xfers->tx_mssg.data_buf =
			    devm_kzalloc(dev, sizeof(u32) * xfers->tx_data_buf_len,
					 GFP_KERNEL);
			if (xfers->tx_mssg.data_buf == NULL) {
				(void)dev_err(dev,
					"[ERROR][TCC_SC_FW] Failed to allocate memory for Tx data buffer\n");
				ret = -ENOMEM;
			}

			xfers->rx_cmd_buf_len = TCC_SC_MAX_CMD_LENGTH;
			xfers->rx_data_buf_len = TCC_SC_MAX_DATA_LENGTH;
			xfers->rx_mssg.cmd =
			    devm_kzalloc(dev, sizeof(u32) * xfers->rx_cmd_buf_len,
					 GFP_KERNEL);
			if (xfers->rx_mssg.cmd == NULL) {
				(void)dev_err(dev,
				"[ERROR][TCC_SC_FW] Failed to allocate memory for Rx cmd buffer\n");
				ret = -ENOMEM;
			}
			xfers->rx_mssg.data_buf =
			    devm_kzalloc(dev, sizeof(u32) * xfers->rx_data_buf_len,
					 GFP_KERNEL);
			if (xfers->rx_mssg.data_buf == NULL) {
				(void)dev_err(dev,
					"[ERROR][TCC_SC_FW] Failed to allocate memory for Rx data buffer\n");
				ret = -ENOMEM;
			}

			if (ret != 0) {
				break;
			}

			spin_lock_init(&xfers->lock);

			xfers++;
		}
	}

	return ret;
}

static int proc_scfwinfo_show(struct seq_file *m, void *v)
{
	if (desc_scfw[0] != '\0') {
		/* Print if description exists */
		seq_printf(m, "%-24s: %s\n", "StorageCore Firmware", desc_scfw);
	}

	return 0;
}

DEFINE_PROC_SHOW_ATTRIBUTE(proc_scfwinfo);

static s32 tcc_sc_fw_probe(struct platform_device *pdev)
{
	const struct proc_dir_entry *pent;
	struct device *dev = &pdev->dev;
	struct tcc_sc_fw_info *info = NULL;
	struct tcc_sc_fw_handle *handle = NULL;
	struct mbox_client *cl;
	s32 ret = -EINVAL;

	info = devm_kzalloc(dev, sizeof(struct tcc_sc_fw_info), GFP_KERNEL);
	if (info == NULL) {
		ret = -ENOMEM;
		goto err_free_mem;
	}

	handle = devm_kzalloc(dev, sizeof(struct tcc_sc_fw_handle), GFP_KERNEL);
	if (handle == NULL) {
		ret = -ENOMEM;
		goto err_free_mem;
	}

	info->dev = dev;
	info->bsid = TCC_SC_BSID_BL4;
#if (defined(CONFIG_TCC805X_CA53Q) || defined(CONFIG_TCC807X_CA55_SUB))
	info->cid = TCC_SC_CID_SUB;
#else
	info->cid = TCC_SC_CID_MAIN;
#endif
	info->uid = 0;
	info->max_rx_timeout_ms = TCC_SC_RX_TIMEOUT_MS;

	INIT_LIST_HEAD(&info->rx_pending);
	INIT_LIST_HEAD(&info->xfers_list);

	spin_lock_init(&info->rx_lock);
	spin_lock_init(&info->xfers_lock);
	spin_lock_init(&info->lock);

	dev_set_drvdata(dev, info);
	platform_set_drvdata(pdev, info);

	cl = &info->cl;
	cl->dev = dev;
	cl->tx_block = (bool)false;
	cl->tx_tout = TCC_SC_TX_TIMEOUT_MS;
	cl->rx_callback = tcc_sc_fw_rx_callback;
	cl->tx_prepare = tcc_sc_fw_tx_prepare;
	cl->tx_done = tcc_sc_fw_tx_done;
	cl->knows_txdone = (bool)false;

	info->chan = mbox_request_channel(cl, 0);
	if (IS_ERR(info->chan)) {
		ret = PTR_ERR_OR_ZERO(info->chan);
		(void)dev_err(&pdev->dev,
			"[ERROR][TCC_SC_FW] Failed to get mbox (%d)\n", ret);
		goto out;
	}

	ret = tcc_sc_fw_alloc_xfer_list(&pdev->dev, info);
	if (ret != 0) {
		(void)dev_err(&pdev->dev,
			"[ERROR][TCC_SC_FW] Failed to allocate memory for xfers\n");
		goto out;
	}

	handle->ops.mmc_ops = &sc_fw_mmc_ops;
	handle->ops.ufs_ops = &sc_fw_ufs_ops;
	handle->ops.gpio_ops = &sc_fw_gpio_ops;
	handle->ops.tfuse_ops = &sc_fw_tfuse_ops;
	handle->priv = info;
	info->handle = handle;

	ret = tcc_sc_fw_cmd_get_revision(info);
	if (ret != 0) {
		(void)dev_err(dev,
			"[ERROR][TCC_SC_FW] failed to get firmware version %d\n"
			, ret);
		goto out;
	}
	(void)memcpy(&info->handle->version, &info->version,
			sizeof(struct tcc_sc_fw_version));

	(void)dev_info(dev,
		"[INFO][TCC_SC_FW] firmware version %d.%d.%d (%s)\n",
		 info->version.major, info->version.minor, info->version.patch,
		 info->version.desc);

	(void)scnprintf(desc_scfw, DESC_LEN, "v%u.%u.%u (%s)",
			info->version.major, info->version.minor,
			info->version.patch, info->version.desc);

	pent = proc_create("scfwinfo", 0444, NULL, &proc_scfwinfo_proc_ops);
	if (pent == NULL) {
		(void)pr_err("Failed to create scfwinfo procfs entry\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = of_platform_populate(dev->of_node, NULL, NULL, dev);

out:
	if (ret != 0) {
		if (!IS_ERR(info->chan)) {
			mbox_free_channel(info->chan);
		}
	}

err_free_mem:

	return ret;
}

static s32 tcc_sc_fw_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	s32 ret = 0;

	of_platform_depopulate(dev);

	return ret;
}

static int tcc_sc_fw_suspend(struct device *dev)
{
	struct tcc_sc_fw_info *info =
			(struct tcc_sc_fw_info *)dev_get_drvdata(dev);
	struct tcc_sc_fw_xfer *xfer;
	unsigned long flags;
	int ret;

	if (info != NULL) {
		spin_lock_irqsave(&info->rx_lock, flags);

		if (list_empty(&info->rx_pending) == 0) {
			/* clear rx_pending message */
			list_for_each_entry(xfer, &info->rx_pending, node) {
				list_del_init(&xfer->node);
				put_tcc_sc_fw_xfer(xfer, info);
			}
		}
		spin_unlock_irqrestore(&info->rx_lock, flags);

		info->suspend = true;
		(void)dev_info(dev,"[%s][%d] enter suspend mode!!!\n", __func__, __LINE__);

		ret = 0;
	} else {
		dev_err(dev,"[%s][%d] Invalide drvdata\n", __func__,__LINE__);
		ret = -EINVAL;
	}

	return ret;
}

static int tcc_sc_fw_resume(struct device *dev)
{
	struct tcc_sc_fw_info *info =
			(struct tcc_sc_fw_info *)dev_get_drvdata(dev);
	int ret;

	if (info != NULL) {
		info->suspend = false;
		(void)dev_info(dev,"[%s][%d] enter resume mode!!!\n", __func__, __LINE__);

		ret = 0;
	} else {
		dev_err(dev,"[%s][%d] Invalide drvdata\n", __func__,__LINE__);
		ret = -EINVAL;
	}

	return ret;
}

static const struct dev_pm_ops tcc_sc_fw_pm = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS((tcc_sc_fw_suspend),
				     (tcc_sc_fw_resume))
};


static struct platform_driver tcc_sc_fw_driver = {
	.probe = tcc_sc_fw_probe,
	.remove = tcc_sc_fw_remove,
	.driver = {
		   .name = "tcc-sc-fw",
		   .pm = &tcc_sc_fw_pm,
		   .of_match_table = of_match_ptr(tcc_sc_fw_of_match),
		   },
};

static s32 __init tcc_sc_fw_init(void)
{
	return platform_driver_register(&tcc_sc_fw_driver);
}

core_initcall(tcc_sc_fw_init);

static void __exit tcc_sc_fw_exit(void)
{
	platform_driver_unregister(&tcc_sc_fw_driver);
}

module_exit(tcc_sc_fw_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Telechips Storage Core Protocol");
MODULE_AUTHOR("Telechips Inc.");
