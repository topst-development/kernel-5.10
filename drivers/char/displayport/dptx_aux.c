// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 Synopsys, Inc.
 *
 * Synopsys DP TX Linux Software Driver and documentation (hereinafter,
 * "Software") is an Unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto. You are permitted to use and
 * redistribute this Software in source and binary forms, with or without
 * modification, provided that redistributions of source code must retain this
 * notice. You may not view, use, disclose, copy or distribute this file or
 * any information contained herein except pursuant to this license grant from
 * Synopsys. If you do not agree with this notice, including the disclaimer
 * below, then you are not authorized to use the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */
/*
 * Modified by Telechips Inc.
 */
#include <drm/drm_dp_helper.h>
#include <linux/delay.h>

#include "dptx_v14.h"
#include "dptx_reg.h"
#include "dptx_dbg.h"

#define AUX_RETRY_INTERVAL 400 /* us */

#define DPTX_AUX_MAX_RW_RETRY				200u

#define DPTX_AUX_MAX_WAIT_REPLY_RETRY			60u
/*
 * DPTX_AUX_MAX_WAIT_REPLY_RETRY
 * For some DP2HDMI genders, a timeout occurs when set
 * DPTX_AUX_MAX_WAIT_REPLY_RETRY to 50.
 * For some DPRX a timeout occurs when set
 * DPTX_AUX_MAX_WAIT_REPLY_RETRY to 60.
 */

static void dptx_aux_read_data(const uint32_t read_rawdata[4],
			       uint8_t *buffer_ptr, uint32_t buffer_len)
{
	uint32_t elements, buffer;

	/*
	 * Code for coverity
	 * - It is guaranteed that buffer_len will be less than 16.
	 */
	if (buffer_len > 16u) {
		/* For KCS */
		buffer_len = 16u;
	}

	for (elements = 0u; elements < buffer_len; elements++) {
		buffer = (read_rawdata[elements / 4u] >> ((elements % 4u) * 8u)) & 0xffu;
		buffer_ptr[elements] = (uint8_t)buffer;
	}
}

static void dptx_aux_clear_data(struct Dptx_Params *pstDptx)
{
	Dptx_Reg_Writel(pstDptx, DPTX_AUX_DATA0, 0);
	Dptx_Reg_Writel(pstDptx, DPTX_AUX_DATA1, 0);
	Dptx_Reg_Writel(pstDptx, DPTX_AUX_DATA2, 0);
	Dptx_Reg_Writel(pstDptx, DPTX_AUX_DATA3, 0);
}

static void dptx_aux_write_data(struct Dptx_Params *pstDptx, uint8_t const *pucBuffer, uint32_t uiLength)
{
	uint32_t auiWriteData[4];
	uint32_t elements, buffer_data;

	(void)memset(auiWriteData, 0, sizeof(uint32_t) * 4u);

	for (elements = 0u; elements < uiLength; elements++) {
		buffer_data = pucBuffer[elements];
		auiWriteData[elements / 4u] |= (buffer_data << ((elements % 4u) * 8u));
	}

	Dptx_Reg_Writel(pstDptx, DPTX_AUX_DATA0, auiWriteData[0]);
	Dptx_Reg_Writel(pstDptx, DPTX_AUX_DATA1, auiWriteData[1]);
	Dptx_Reg_Writel(pstDptx, DPTX_AUX_DATA2, auiWriteData[2]);
	Dptx_Reg_Writel(pstDptx, DPTX_AUX_DATA3, auiWriteData[3]);
}

struct dptx_aux_data {
	bool read;
	bool native_mode;
	bool mot_mode;
	bool addr_mode; /* I2C Address only mode */
	uint32_t address;
	uint32_t buffer_len;
	uint8_t *buffer_ptr;
};

static int32_t dptx_aux_read_write(struct Dptx_Params *pstDptx,
				   const struct dptx_aux_data *dptx_data)
{
	int32_t ret = DPTX_RETURN_NO_ERROR;
	bool aux_nack = (bool)false;
	bool aux_ack = (bool)false;

	uint32_t aux_status, aux_rd_bytes, aux_cmd;
	uint32_t retry_aux_rw, retry_respond;
	uint32_t reg_val;

	uint32_t timeout_count = 0u;
	uint32_t defer_count = 0u;
	uint32_t zero_count = 0u;

	if (dptx_intr_get_hotplug_status(pstDptx) == HPD_STATUS_UNPLUGGED) {
		dptx_err("DisplayPort HPD is unplugged\n");
	} else if ((dptx_data->buffer_len == 0u) || (dptx_data->buffer_len > 16u)) {
		dptx_err("AUX read/write len must be 1-15, len=%d ",
			  dptx_data->buffer_len);
	} else {
		aux_cmd = 0u;
		if (dptx_data->read) {
			/* For KCS */
			aux_cmd |= (uint32_t)DPTX_AUX_CMD_TYPE_READ;
		}
		if (dptx_data->native_mode) {
			aux_cmd |= (uint32_t)DPTX_AUX_CMD_TYPE_NATIVE;
		} else {
			if (dptx_data->mot_mode) {
				/* For KCS */
				aux_cmd |= (uint32_t)DPTX_AUX_CMD_TYPE_MOT;
			}
		}
		aux_cmd <<= DPTX_AUX_CMD_TYPE_SHIFT;
		aux_cmd |= (dptx_data->address << DPTX_AUX_CMD_ADDR_SHIFT);
		if (dptx_data->addr_mode) {
			/* For KCS */
			aux_cmd |= (uint32_t)DPTX_AUX_CMD_I2C_ADDR_ONLY;
		}
		aux_cmd |= ((dptx_data->buffer_len - 1u) & 0xfu);

		for (retry_aux_rw = 0u; retry_aux_rw < DPTX_AUX_MAX_RW_RETRY;
		     retry_aux_rw++) {
			if (dptx_intr_get_hotplug_status(pstDptx) == HPD_STATUS_UNPLUGGED) {
				dptx_err("DisplayPort HPD is unplugged\n");
				break;
			}
			(void)Dptx_Core_Clear_General_Interrupt(pstDptx, (uint32_t)DPTX_ISTS_AUX_REPLY);
			dptx_aux_clear_data(pstDptx);

			if (!dptx_data->read) {
				/* For KCS */
				dptx_aux_write_data(pstDptx, dptx_data->buffer_ptr, dptx_data->buffer_len);
			}
			Dptx_Reg_Writel(pstDptx, DPTX_AUX_CMD, aux_cmd);

			/* Wait Maximum 600us */
			for (retry_respond = 0u; retry_respond < DPTX_AUX_MAX_WAIT_REPLY_RETRY; retry_respond++) {
				if (dptx_intr_get_hotplug_status(pstDptx) == HPD_STATUS_UNPLUGGED) {
					dptx_err("DisplayPort HPD is unplugged\n");
					ret = -DPTX_RETURN_ENODEV;
				} else {
					reg_val = Dptx_Reg_Readl(pstDptx, DPTX_AUX_STS);
					if ((reg_val & (uint32_t)DPTX_AUX_STS_REPLY_ERR) != 0u) {
						#if defined(CONFIG_ARCH_TCC805X)
						if (((reg_val >> 25) & 7u) != 4u) {
						#endif
							dptx_err("REPLY_ERR 0b04h(0x%x), 0204h(0x%x)\n",
								  reg_val, Dptx_Reg_Readl(pstDptx, DPTX_SRST_CTRL));
							ret = -DPTX_RETURN_EPERM;
						#if defined(CONFIG_ARCH_TCC805X)
						}
						#endif
					}
					if ((reg_val & (uint32_t)DPTX_AUX_STS_TIMEOUT) != 0u) {
						ret = -ETIMEDOUT;
						dptx_dbg("Aux timeout from Sink->try(%u).. 0x%x",
							retry_aux_rw, reg_val);
					}
				}
				if (DPTX_RETURN_ERROR(ret) ||
				    (reg_val & (uint32_t)DPTX_AUX_STS_REPLY_RECEIVED) == 0u) {
					break;
				}
				udelay(10);
			};
			/* check timeout */
			if (retry_respond  >= DPTX_AUX_MAX_WAIT_REPLY_RETRY) {
				ret = -ETIMEDOUT;
				dptx_dbg("Aux timeout from Sink->try(%u).. 0x%x",
					 retry_aux_rw, reg_val);
			}
			if (DPTX_RETURN_ERROR(ret)) {
				if (timeout_count < DPTX_AUX_MAX_RW_RETRY) {
					/* For KCS */
					timeout_count++;
				}
				Dptx_Core_Soft_Reset(pstDptx, DPTX_SRST_CTRL_AUX);
				udelay(AUX_RETRY_INTERVAL);
				continue;
			}

			aux_status = ((reg_val & (uint32_t)DPTX_AUX_STS_STATUS_MASK) >> DPTX_AUX_STS_STATUS_SHIFT);
			aux_rd_bytes = ((reg_val & (uint32_t)DPTX_AUX_STS_BYTES_READ_MASK) >> DPTX_AUX_STS_BYTES_READ_SHIFT);

			/* Chack ACK Staus */
			if ((aux_status & 0xfu) == 0u) {
				if (aux_rd_bytes == 0u) {
					/* For KCS */
					zero_count++;
				} else {
					aux_ack = (bool)true;
				}
			}
			if (((aux_status & DPTX_AUX_STS_STATUS_NACK) != 0u) ||
			   ((aux_status & DPTX_AUX_STS_STATUS_I2C_NACK) != 0u)) {
				/*
				 * This NACK message means that a protocol not
				 * supported by the connected RX and does not
				 * repeat unnecessary communication.
				 */
				aux_nack = (bool)true;
			}
			if (((aux_status & DPTX_AUX_STS_STATUS_I2C_DEFER) != 0u) ||
			   ((aux_status & DPTX_AUX_STS_STATUS_DEFER) != 0u)) {
				/* SINK is not ready to read/write data */
				defer_count++;
				dptx_dbg("AUX Defer status(%u).. try %u", aux_status,
					 retry_aux_rw);
			}
			if (aux_ack || aux_nack) {
				/*
				 * The NACK message means that a protocol not
				 * supported by the connected RX and does not
				 * repeat unnecessary communication.
				 */
				if (!aux_nack) {
					if (dptx_data->read) {
						uint32_t read_rawdata[4];

						read_rawdata[0] = Dptx_Reg_Readl(pstDptx, DPTX_AUX_DATA0);
						read_rawdata[1] = Dptx_Reg_Readl(pstDptx, DPTX_AUX_DATA1);
						read_rawdata[2] = Dptx_Reg_Readl(pstDptx, DPTX_AUX_DATA2);
						read_rawdata[3] = Dptx_Reg_Readl(pstDptx, DPTX_AUX_DATA3);

						dptx_aux_read_data((const uint32_t *)read_rawdata,
								dptx_data->buffer_ptr,
								dptx_data->buffer_len);

						if (dptx_data->buffer_len == (aux_rd_bytes - 1)) {
							/* For KCS */
							ret = DPTX_RETURN_NO_ERROR;
						} else {
							dptx_err("msg length(%u) is not match with (%u)",
								dptx_data->buffer_len, aux_rd_bytes - 1u);
							ret = -EPROTO;
						}
					}
				}
				break;
			}
			Dptx_Core_Soft_Reset(pstDptx, DPTX_SRST_CTRL_AUX);
			/* wait aux tetry interval */
			udelay(AUX_RETRY_INTERVAL);
		}
		if (!aux_ack) {
			if (aux_nack) {
				if ((dptx_data->address == (uint32_t)EDID_I2C_OVER_AUX_ADDR) ||
				    (dptx_data->address == (uint32_t)EDID_I2C_OVER_AUX_SEGMENT_ADDR)) {
					dptx_info("I2C NACK from Sink. The connected RX may not support EDID.");
				} else {
					dptx_info("NACK from Sink %s %05xh-%u",
						  dptx_data->read ? "R" : "W",
						  dptx_data->address,
						  dptx_data->buffer_len);
				}
				ret = -EREMOTEIO;
			} else {
				dptx_info(" No response - %s %05xh-%u Z(%u)/D(%u)/T(%u)\n",
					  dptx_data->read ? "R" : "W",
					  dptx_data->address, dptx_data->buffer_len,
					  zero_count, defer_count, timeout_count);
				ret = -EPROTO;
			}
		}
	}
	return ret;
}

int32_t Dptx_Aux_Read_DPCD(struct Dptx_Params *pstDptx, uint32_t uiAddr, uint8_t *pucBuffer)
{
	struct dptx_aux_data dptx_data = {
		.read = (bool)true,
		.native_mode = (bool)true,
		.mot_mode = (bool)true,
		.addr_mode = (bool)false,
	};

	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint32_t retry;

	dptx_data.address = uiAddr;
	dptx_data.buffer_ptr = pucBuffer;
	dptx_data.buffer_len = 1u;
	/*
	 * The specification doesn't give any recommendation on how often to
	 * retry native transactions. We used to retry 7 times like for
	 * aux i2c transactions but real world devices this wasn't
	 * sufficient, bump to 32 which makes Dell 4k monitors happier.
	 */
	for (retry = 0u; retry < 2u; retry++) {
		ret = dptx_aux_read_write(pstDptx, (const struct dptx_aux_data *)&dptx_data);
		if (DPTX_RETURN_SUCCESS(ret)) {
			if (retry > 0u) {
				/* For KCS */
				dptx_info("%05xh %u times", dptx_data.address, retry);
			}
			break;
		}
		if (ret != -ETIMEDOUT) {
			/* For KCS */
			udelay(AUX_RETRY_INTERVAL);
		}
	}

	return ret;
}

int32_t Dptx_Aux_Read_Bytes_From_DPCD(struct Dptx_Params *pstDptx, uint32_t uiAddr, uint8_t *pucBuffer, uint32_t uiLength)
{
	struct dptx_aux_data dptx_data = {
		.read = (bool)true,
		.native_mode = (bool)true,
		.mot_mode = (bool)true,
		.addr_mode = (bool)false,
	};

	uint32_t uiActive_Length = 0;
	uint32_t uiElements;

	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint32_t retry;

	for (uiElements = 0; uiElements < uiLength; uiElements += uiActive_Length) {
		uiActive_Length = min_t(unsigned int, uiLength - uiElements, 16);

		dptx_data.address = uiAddr + uiElements;
		dptx_data.buffer_ptr = &pucBuffer[uiElements];
		dptx_data.buffer_len = uiActive_Length;

		/*
		 * The specification doesn't give any recommendation on how often to
		 * retry native transactions. We used to retry 7 times like for
		 * aux i2c transactions but real world devices this wasn't
		 * sufficient, bump to 32 which makes Dell 4k monitors happier.
		 */
		for (retry = 0u; retry < 4u; retry++) {
			ret = dptx_aux_read_write(pstDptx, (const struct dptx_aux_data *)&dptx_data);
			if (DPTX_RETURN_SUCCESS(ret)) {
				if (retry > 0u) {
					/* For KCS */
					dptx_info("%05xh %u times", dptx_data.address, retry);
				}
				break;
			}
			if (ret != -ETIMEDOUT) {
				/* For KCS */
				udelay(AUX_RETRY_INTERVAL);
			}
		}

		if (ret !=  DPTX_RETURN_NO_ERROR) {
			/* For KCS */
			break;
		}
	}

	return ret;
}

int32_t Dptx_Aux_Write_DPCD(struct Dptx_Params *pstDptx, uint32_t uiAddr, uint8_t ucBuffer)
{
	struct dptx_aux_data dptx_data = {
		.read = (bool)false,
		.native_mode = (bool)true,
		.mot_mode = (bool)true,
		.addr_mode = (bool)false,
	};

	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint32_t retry;

	dptx_data.address = uiAddr;
	dptx_data.buffer_ptr = &ucBuffer;
	dptx_data.buffer_len = 1;

	/*
	 * The specification doesn't give any recommendation on how often to
	 * retry native transactions. We used to retry 7 times like for
	 * aux i2c transactions but real world devices this wasn't
	 * sufficient, bump to 32 which makes Dell 4k monitors happier.
	 */
	for (retry = 0u; retry < 2u; retry++) {
		ret = dptx_aux_read_write(pstDptx, (const struct dptx_aux_data *)&dptx_data);
		if (DPTX_RETURN_SUCCESS(ret)) {
			if (retry > 0u) {
				/* For KCS */
				dptx_info("%05xh %u times", dptx_data.address, retry);
			}
			break;
		}
		if (ret != -ETIMEDOUT) {
			/* For KCS */
			udelay(AUX_RETRY_INTERVAL);
		}
	}

	return ret;
}

int32_t Dptx_Aux_Write_Bytes_To_DPCD(struct Dptx_Params *pstDptx, uint32_t uiAddr, uint8_t *pucBuffer, uint32_t uiLength)
{
	struct dptx_aux_data dptx_data = {
		.read = (bool)false,
		.native_mode = (bool)true,
		.mot_mode = (bool)true,
		.addr_mode = (bool)false,
	};

	uint32_t uiActive_Length = 0u;
	uint32_t uiElements;

	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint32_t retry;

	for (uiElements = 0u; uiElements < uiLength; uiElements += uiActive_Length) {
		uiActive_Length = min_t(unsigned int, uiLength - uiElements, 16);

		dptx_data.address = uiAddr + uiElements;
		dptx_data.buffer_ptr = &pucBuffer[uiElements];
		dptx_data.buffer_len = uiActive_Length;

		/*
		 * The specification doesn't give any recommendation on how often to
		 * retry native transactions. We used to retry 7 times like for
		 * aux i2c transactions but real world devices this wasn't
		 * sufficient, bump to 32 which makes Dell 4k monitors happier.
		 */
		for (retry = 0u; retry < 4u; retry++) {
			ret = dptx_aux_read_write(pstDptx, (const struct dptx_aux_data *)&dptx_data);
			if (DPTX_RETURN_SUCCESS(ret)) {
				if (retry > 0u) {
					/* For KCS */
					dptx_info("%05xh %u times", dptx_data.address, retry);
				}
				break;
			}
			if (ret != -ETIMEDOUT) {
				/* For KCS */
				udelay(AUX_RETRY_INTERVAL);
			}
		}

		if (ret !=  DPTX_RETURN_NO_ERROR) {
			/* For KCS */
			break;
		}
	}

	return ret;
}

int32_t Dptx_Aux_Read_Bytes_From_I2C(struct Dptx_Params *pstDptx, uint32_t uiDevice_Addr, uint8_t *pucBuffer, uint32_t uiLength)
{
	struct dptx_aux_data dptx_data = {
		.read = (bool)true,
		.native_mode = (bool)false,
		.mot_mode = (bool)true,
		.addr_mode = (bool)false,
	};

	uint32_t uiActive_Length = 0u;
	uint32_t uiElements;

	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint32_t retry;

	for (uiElements = 0u; uiElements < uiLength; uiElements += uiActive_Length) {
		uiActive_Length = min_t(unsigned int, uiLength - uiElements, 16);

		dptx_data.address = uiDevice_Addr;
		dptx_data.buffer_ptr = &pucBuffer[uiElements];
		dptx_data.buffer_len = uiActive_Length;

		/*
		 * The specification doesn't give any recommendation on how often to
		 * retry native transactions. We used to retry 7 times like for
		 * aux i2c transactions but real world devices this wasn't
		 * sufficient, bump to 32 which makes Dell 4k monitors happier.
		 */
		for (retry = 0u; retry < 1u; retry++) {
			ret = dptx_aux_read_write(pstDptx, (const struct dptx_aux_data *)&dptx_data);
			if (DPTX_RETURN_SUCCESS(ret)) {
				/* For KCS */
				break;
			}
			if (ret != -ETIMEDOUT) {
				/* For KCS */
				udelay(AUX_RETRY_INTERVAL);
			}
		}

		if (ret !=  DPTX_RETURN_NO_ERROR) {
			/* For KCS */
			break;
		}
	}

	return ret;
}

int32_t Dptx_Aux_Write_Bytes_To_I2C(struct Dptx_Params *pstDptx, uint32_t uiDevice_Addr, uint8_t *pucBuffer, uint32_t uiLength)
{
	struct dptx_aux_data dptx_data = {
		.read = (bool)false,
		.native_mode = (bool)false,
		.mot_mode = (bool)true,
		.addr_mode = (bool)false,
	};

	uint32_t uiActive_Length = 0u;
	uint32_t uiElements;

	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint32_t retry;

	for (uiElements = 0u; uiElements < uiLength; uiElements += uiActive_Length) {
		uiActive_Length = min_t(unsigned int, uiLength - uiElements, 16);

		dptx_data.address = uiDevice_Addr;
		dptx_data.buffer_ptr = &pucBuffer[uiElements];
		dptx_data.buffer_len = uiActive_Length;

		/*
		 * The specification doesn't give any recommendation on how often to
		 * retry native transactions. We used to retry 7 times like for
		 * aux i2c transactions but real world devices this wasn't
		 * sufficient, bump to 32 which makes Dell 4k monitors happier.
		 */
		for (retry = 0u; retry < 1u; retry++) {
			ret = dptx_aux_read_write(pstDptx, (const struct dptx_aux_data *)&dptx_data);
			if (DPTX_RETURN_SUCCESS(ret)) {
				/* For KCS */
				break;
			}
			if (ret != -ETIMEDOUT) {
				/* For KCS */
				udelay(AUX_RETRY_INTERVAL);
			}
		}

		if (ret !=  DPTX_RETURN_NO_ERROR) {
			/* For KCS */
			break;
		}
	}

	return ret;
}

int32_t Dptx_Aux_Write_AddressOnly_To_I2C(struct Dptx_Params *pstDptx, uint32_t uiDevice_Addr)
{
	struct dptx_aux_data dptx_data = {
		.read = (bool)false,
		.native_mode = (bool)false,
		.mot_mode = (bool)false,
		.addr_mode = (bool)true,
	};

	int32_t ret = DPTX_RETURN_NO_ERROR;
	uint32_t retry;
	uint8_t buffer;

	dptx_data.address = uiDevice_Addr;
	dptx_data.buffer_ptr = &buffer;
	dptx_data.buffer_len = 1u;
	/*
	 * The specification doesn't give any recommendation on how often to
	 * retry native transactions. We used to retry 7 times like for
	 * aux i2c transactions but real world devices this wasn't
	 * sufficient, bump to 32 which makes Dell 4k monitors happier.
	 */
	for (retry = 0u; retry < 1u; retry++) {
		ret = dptx_aux_read_write(pstDptx, (const struct dptx_aux_data *)&dptx_data);
		if (DPTX_RETURN_SUCCESS(ret)) {
			/* For KCS */
			break;
		}
		if (ret != -ETIMEDOUT) {
			/* For KCS */
			udelay(AUX_RETRY_INTERVAL);
		}
	}

	return ret;
}

/**
 * @brief Set AUX hysteresis.
 *
 * This function sets the AUX hysteresis setting of the source device.
 *
 * @param[in] pstDptx DisplayPort driver context pointer containing general
 *                    information for DisplayPort control.
 * @param[in] hysteresis Hysteresis setting value of DisplayPort AUX
 *                       0 to 3
 * @return 0 if the function executes successfully; otherwise, a negative value is returned.
 *
 */
static int32_t dptx_set_aux_hysteresis(struct Dptx_Params *pstDptx, uint32_t hysteresis)
{
	uint32_t reg_hysteresis, reg_hysteresis_val;

	if (pstDptx->ePhy_Dev == PHY_DEVICE_SNPS) {
		reg_hysteresis = pstDptx->uiRegBank_RegAddr_Offset + DP_REGISTER_BANK_REG_20;

		reg_hysteresis_val = Dptx_Reg_Readl(pstDptx, reg_hysteresis);
		//dptx_err("D3 current hysteresis reg = 0x%x\n", reg_hysteresis_val);

		reg_hysteresis_val &= ~((uint32_t)3u << 12);
		reg_hysteresis_val |= ((hysteresis & 0x3u) << 12);
		Dptx_Reg_Writel(pstDptx, reg_hysteresis, reg_hysteresis_val);

		//dptx_err("D3 New hysteresis reg = 0x%x\n", Dptx_Reg_Readl(pstDptx, reg_hysteresis));
	}
	if (pstDptx->ePhy_Dev == PHY_DEVICE_SEC) {
		reg_hysteresis = pstDptx->uiSEC_PHY_Reg_Offset + SEC_PHY_REG0108;

		reg_hysteresis_val = Dptx_Reg_Readl(pstDptx, reg_hysteresis);
		//dptx_err("D5 current hysteresis reg = 0x%x\n", reg_hysteresis_val);

		reg_hysteresis_val &= ~((uint32_t)3u << 4);
		reg_hysteresis_val |= ((hysteresis & 0x3u) << 4);
		Dptx_Reg_Writel(pstDptx, reg_hysteresis, reg_hysteresis_val);

		//dptx_err("D5 New hysteresis reg = 0x%x\n", Dptx_Reg_Readl(pstDptx, reg_hysteresis));
	}
	return 0;
}

/**
 * @brief Read AUX hysteresis from device tree
 *
 * This function reads the AUX hysteresis setting value from the device tree and
 * applies it to the aux_hysteresis variable of the DisplayPort device context.
 * This value is used to set the hysteresis of AUX when DisplayPort is
 * initialized.
 *
 * @param[in] pstDptx DisplayPort driver context pointer containing general
 *                    information for DisplayPort control.
 * @return None
 *
 */
void dptx_update_aux_hysteresis_from_dtb(struct Dptx_Params *dev_param)
{
	uint32_t hysteresis = 1u;
	struct device_node *np;

	#if defined(CONFIG_ARCH_TCC805X)
	hysteresis = 2u;
	#endif

	np = of_get_child_by_name(dev_param->dev->of_node, "hw-config");
	if (np != NULL) {
		(void)of_property_read_u32(np, "aux-hysteresis", &hysteresis);
		of_node_put(np);
	}

	/* Update  hysteresi from local hysteresi */
	if (hysteresis < 4u) {
		/* For KCS */
		dev_param->hw_config.aux_hysteresis = hysteresis;
	}
}

/**
 * @brief Check and adjust DisplayPort AUX hysteresis.
 *
 * This function reads the DisplayPort sink capabilities and checks whether
 * DisplayPort AUX communication operates normally. If an error occurs during
 * this process, it adjusts the AUX hysteresis settings of the source device.
 *
 * @param[in] pstDptx DisplayPort driver context pointer containing general
 *                    information for DisplayPort control.
 * @return 0 if the function executes successfully; otherwise, a negative value is returned.
 *
 */
int32_t dptx_check_aux_communication(struct Dptx_Params *dev_param)
{
	uint32_t hysteresis;
	int32_t ret = 0;

	uint8_t sink_cap;

	hysteresis = dev_param->hw_config.aux_hysteresis;

	dptx_set_aux_hysteresis(dev_param, hysteresis);

	while (hysteresis < 4u) {
		ret = Dptx_Aux_Read_DPCD(dev_param, DP_DPCD_REV, &sink_cap);
		if (DPTX_RETURN_SUCCESS(ret)) {
			/* Update  hysteresi from local hysteresi */
			dev_param->hw_config.aux_hysteresis = hysteresis;
			break;
		}
		hysteresis++;
		if (hysteresis < 4u) {
			dptx_set_aux_hysteresis(dev_param, hysteresis);
			dptx_info("New hysteresis level = %u\n", hysteresis);
		}
	}
	if (hysteresis > 3u) {
		/* For KCS */
		dptx_err("Faild to find valid hysteresis level");
	}

	return ret;
}
