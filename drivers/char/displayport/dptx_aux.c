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

#include <linux/delay.h>

#include "dptx_v14.h"
#include "dptx_reg.h"
#include "dptx_dbg.h"

#define AUX_RETRY_INTERVAL 400 /* us */

#define DPTX_AUX_MAX_RW_RETRY				200u

#define DPTX_AUX_MAX_REPLY_RETRY			50u

/*
 * DPTX_AUX_MAX_REPLY_RETRY
 * For some DP2HDMI genders, a timeout occurs when set
 * DPTX_AUX_MAX_REPLY_RETRY to 50.
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

	if ((dptx_data->buffer_len == 0u) || (dptx_data->buffer_len > 16u)) {
		dptx_err("AUX read/write len must be 1-15, len=%d ",
			  dptx_data->buffer_len);
	} else {
		aux_cmd = 0u;
		if (dptx_data->read) {
			aux_cmd |= (uint32_t)DPTX_AUX_CMD_TYPE_READ;
		}
		if (dptx_data->native_mode) {
			aux_cmd |= (uint32_t)DPTX_AUX_CMD_TYPE_NATIVE;
		} else {
			if (dptx_data->mot_mode) {
				aux_cmd |= (uint32_t)DPTX_AUX_CMD_TYPE_MOT;
			}
		}
		aux_cmd <<= DPTX_AUX_CMD_TYPE_SHIFT;
		aux_cmd |= (dptx_data->address << DPTX_AUX_CMD_ADDR_SHIFT);
		if (dptx_data->addr_mode) {
			aux_cmd |= (uint32_t)DPTX_AUX_CMD_I2C_ADDR_ONLY;
		}
		aux_cmd |= ((dptx_data->buffer_len - 1u) & 0xfu);

		for (retry_aux_rw = 0u; retry_aux_rw < DPTX_AUX_MAX_RW_RETRY;
		     retry_aux_rw++) {
			Dptx_Core_Clear_General_Interrupt(pstDptx, (uint32_t)DPTX_ISTS_AUX_REPLY);
			dptx_aux_clear_data(pstDptx);

			if (!dptx_data->read) {
				dptx_aux_write_data(pstDptx, dptx_data->buffer_ptr, dptx_data->buffer_len);
			}
			Dptx_Reg_Writel(pstDptx, DPTX_AUX_CMD, aux_cmd);

			/* Wait Maximum 5ms */
			/* Wait Maximum 5ms -> 600us */
			for (retry_respond = 0u; retry_respond < DPTX_AUX_MAX_REPLY_RETRY; retry_respond++) {
				reg_val = Dptx_Reg_Readl(pstDptx, DPTX_AUX_STS);
				if ((reg_val & (uint32_t)DPTX_AUX_STS_TIMEOUT) != 0u) {
					ret = -ETIMEDOUT;
					dptx_dbg("Aux timeout from Sink->try(%u).. 0x%x",
						 retry_aux_rw, reg_val);
				}
				if ((ret != DPTX_RETURN_NO_ERROR) ||
				    (reg_val & (uint32_t)DPTX_AUX_STS_REPLY_RECEIVED) == 0u) {
					break;
				}
				/* Coverity False positives - udelay
				 * [FP] misra_c_2012_rule_10_4_violation
				 * [FP] misra_c_2012_rule_12_1_violation
				 * [FP] misra_c_2012_rule_14_4_violation
				 * [FP] misra_c_2012_rule_15_6_violation
				 * [FP] cert_dcl37_c_violation
				 */
				udelay(10);
			};
			/* check timeout */
			if (retry_respond  >= DPTX_AUX_MAX_REPLY_RETRY) {
				ret = -ETIMEDOUT;
				dptx_dbg("Aux timeout from Sink->try(%u).. 0x%x",
					 retry_aux_rw, reg_val);
			}
			if (ret != DPTX_RETURN_NO_ERROR) {
				if (timeout_count < 100u) {
					timeout_count++;
				}
				/* Coverity False positives - udelay
				 * [FP] misra_c_2012_rule_10_4_violation
				 * [FP] misra_c_2012_rule_12_1_violation
				 * [FP] misra_c_2012_rule_14_4_violation
				 * [FP] misra_c_2012_rule_15_6_violation
				 * [FP] cert_dcl37_c_violation
				 */
				udelay(AUX_RETRY_INTERVAL);
				continue;
			}

			aux_status = ((reg_val & (uint32_t)DPTX_AUX_STS_STATUS_MASK) >> DPTX_AUX_STS_STATUS_SHIFT);
			aux_rd_bytes = ((reg_val & (uint32_t)DPTX_AUX_STS_BYTES_READ_MASK) >> DPTX_AUX_STS_BYTES_READ_SHIFT);

			/* Chack ACK Staus */
			if ((aux_status & 0xfu) == 0u) {
				if (aux_rd_bytes == 0u) {
					zero_count++;
					Dptx_Core_Soft_Reset(pstDptx, DPTX_SRST_CTRL_AUX);
				} else {
					if ((zero_count > 0u) || (defer_count > 0u) ||
					    (timeout_count > 0u)) {
						dptx_dbg("DP: ack with zero(%d), defer(%d), timeout(%d)\n",
							  zero_count, defer_count, timeout_count);
					}
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
			/* wait aux tetry interval */
			/* Coverity False positives - udelay
			 * [FP] misra_c_2012_rule_10_4_violation
			 * [FP] misra_c_2012_rule_12_1_violation
			 * [FP] misra_c_2012_rule_14_4_violation
			 * [FP] misra_c_2012_rule_15_6_violation
			 * [FP] cert_dcl37_c_violation
			 */
			udelay(AUX_RETRY_INTERVAL);
		}
		if (!aux_ack) {
			if (aux_nack) {
				if ((dptx_data->address == (uint32_t)EDID_I2C_OVER_AUX_ADDR) ||
				    (dptx_data->address == (uint32_t)EDID_I2C_OVER_AUX_SEGMENT_ADDR)) {
					dptx_info("I2C NACK from Sink. The connected RX may not support EDID.");
				} else {
					dptx_err("NACK from Sink reg<0x%x>", reg_val);
				}
				ret = -EREMOTEIO;
			} else {
				dptx_err("No response from Sink->try(%d) reg<0x%x>\n ERROR> zero(%d), defer(%d), timeout(%d)",
					 retry_aux_rw, reg_val, zero_count, defer_count, timeout_count);
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
                if (ret != 0 && ret != -ETIMEDOUT) {
			/* Coverity False positives - udelay
			 * [FP] misra_c_2012_rule_10_4_violation
			 * [FP] misra_c_2012_rule_12_1_violation
			 * [FP] misra_c_2012_rule_14_4_violation
			 * [FP] misra_c_2012_rule_15_6_violation
			 * [FP] cert_dcl37_c_violation
			 */
                        udelay(AUX_RETRY_INTERVAL);
                }
		ret = dptx_aux_read_write(pstDptx, (const struct dptx_aux_data *)&dptx_data);
		if (ret == DPTX_RETURN_NO_ERROR) {
			break;
		} else {
			dptx_err("addr=%04x", dptx_data.address);
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
		for (retry = 0u; retry < 1u; retry++) {
			if (ret != -ETIMEDOUT) {
				/* Coverity False positives - udelay
				 * [FP] misra_c_2012_rule_10_4_violation
				 * [FP] misra_c_2012_rule_12_1_violation
				 * [FP] misra_c_2012_rule_14_4_violation
				 * [FP] misra_c_2012_rule_15_6_violation
				 * [FP] cert_dcl37_c_violation
				 */
				udelay(AUX_RETRY_INTERVAL);
			}
			ret = dptx_aux_read_write(pstDptx, (const struct dptx_aux_data *)&dptx_data);
			if (ret == DPTX_RETURN_NO_ERROR) {
				if (retry > 0) {
					dptx_err("addr=%04x, retry = %u", dptx_data.address, retry);
				}
				break;
			} else {
				dptx_err("addr=%04x", dptx_data.address);
			}
		}

		if (ret !=  DPTX_RETURN_NO_ERROR) {
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
	for (retry = 0u; retry < 1u; retry++) {
		if (ret != -ETIMEDOUT) {
			/* Coverity False positives - udelay
			 * [FP] misra_c_2012_rule_10_4_violation
			 * [FP] misra_c_2012_rule_12_1_violation
			 * [FP] misra_c_2012_rule_14_4_violation
			 * [FP] misra_c_2012_rule_15_6_violation
			 * [FP] cert_dcl37_c_violation
			 */
			udelay(AUX_RETRY_INTERVAL);
		}
		ret = dptx_aux_read_write(pstDptx, (const struct dptx_aux_data *)&dptx_data);
		if (ret == DPTX_RETURN_NO_ERROR) {
			break;
		} else {
			dptx_err("addr=%04x", dptx_data.address);
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
		for (retry = 0u; retry < 1u; retry++) {
			if (ret != -ETIMEDOUT) {
				/* Coverity False positives - udelay
				 * [FP] misra_c_2012_rule_10_4_violation
				 * [FP] misra_c_2012_rule_12_1_violation
				 * [FP] misra_c_2012_rule_14_4_violation
				 * [FP] misra_c_2012_rule_15_6_violation
				 * [FP] cert_dcl37_c_violation
				 */
				udelay(AUX_RETRY_INTERVAL);
			}
			ret = dptx_aux_read_write(pstDptx, (const struct dptx_aux_data *)&dptx_data);
			if (ret == DPTX_RETURN_NO_ERROR) {
				break;
			} else {
				dptx_err("addr=%04x", dptx_data.address);
			}
		}

		if (ret !=  DPTX_RETURN_NO_ERROR) {
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
			if (ret != -ETIMEDOUT) {
				/* Coverity False positives - udelay
				 * [FP] misra_c_2012_rule_10_4_violation
				 * [FP] misra_c_2012_rule_12_1_violation
				 * [FP] misra_c_2012_rule_14_4_violation
				 * [FP] misra_c_2012_rule_15_6_violation
				 * [FP] cert_dcl37_c_violation
				 */
				udelay(AUX_RETRY_INTERVAL);
			}
			ret = dptx_aux_read_write(pstDptx, (const struct dptx_aux_data *)&dptx_data);
			if (ret == DPTX_RETURN_NO_ERROR) {
				break;
			} else {
				dptx_err("addr=%04x", dptx_data.address);
			}
		}

		if (ret !=  DPTX_RETURN_NO_ERROR) {
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
			if (ret != -ETIMEDOUT) {
				/* Coverity False positives - udelay
				 * [FP] misra_c_2012_rule_10_4_violation
				 * [FP] misra_c_2012_rule_12_1_violation
				 * [FP] misra_c_2012_rule_14_4_violation
				 * [FP] misra_c_2012_rule_15_6_violation
				 * [FP] cert_dcl37_c_violation
				 */
				udelay(AUX_RETRY_INTERVAL);
			}
			ret = dptx_aux_read_write(pstDptx, (const struct dptx_aux_data *)&dptx_data);
			if (ret == DPTX_RETURN_NO_ERROR) {
				break;
			}
		}

		if (ret !=  DPTX_RETURN_NO_ERROR) {
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
                if (ret != -ETIMEDOUT) {
			/* Coverity False positives - udelay
			 * [FP] misra_c_2012_rule_10_4_violation
			 * [FP] misra_c_2012_rule_12_1_violation
			 * [FP] misra_c_2012_rule_14_4_violation
			 * [FP] misra_c_2012_rule_15_6_violation
			 * [FP] cert_dcl37_c_violation
			 */
                        udelay(AUX_RETRY_INTERVAL);
                }
		ret = dptx_aux_read_write(pstDptx, (const struct dptx_aux_data *)&dptx_data);
		if (ret == DPTX_RETURN_NO_ERROR) {
			break;
		} else {
			dptx_err("addr=%04x", dptx_data.address);
		}
	}

	return ret;
}
