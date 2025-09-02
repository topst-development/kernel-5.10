// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#if 0
#define NDEBUG
#endif
#define TLOG_LEVEL (TLOG_WARNING)
#include "tcc_hsm_log.h"

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mailbox/mailbox-tcc.h>
#include <linux/mailbox/tcc_sec_ipc.h>
#include <linux/mailbox/tcc_sp_ioctl.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/wait.h>

#include <linux/io.h>
#include <linux/uaccess.h>
#include <asm/div64.h>

#include "tcc_hsm.h"
#include "tcc_hsm_cmd.h"

/****************************************************************************
 * DEFINITiON
 ****************************************************************************/
#define DMA_MAX_RSIZE (1U * 1024U * 1024U)
#define MBOX_LOCATION_DATA (0x0400U)
#define MBOX_LOCATION_CMD (0x0000U)

/****************************************************************************
 * DEFINITION OF LOCAL FUNCTIONS
 ****************************************************************************/
uint32_t tcc_hsm_cmd_set_key(uint32_t device_id, uint32_t req,
			     const struct tcc_hsm_ioctl_set_key_param *param)
{
	uint32_t data[128] = { 0 };
	uint32_t idx = 0;
	uint32_t rdata = 0;
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	uint32_t result = TCCHSM_ERR;

	if (param == NULL) {
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	data[idx] = param->addr;
	idx++;
	data[idx] = param->data_size;
	idx++;
	data[idx] = param->key_index;
	idx++;

	data_size = (uint32_t)(sizeof(uint32_t) * idx);

	rdata_size =
		sec_sendrecv_cmd(device_id, (req | MBOX_LOCATION_DATA), data,
				 data_size, (void *)&rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		result = TCCHSM_ERR_INVALID_STATE;
		goto out;
	}

	result = rdata;
	if (result != TCCHSM_SUCCESS) {
		ELOG("Error: 0x%x\n", result);
		goto out;
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_cmd_set_key);

uint32_t tcc_hsm_cmd_set_modn(uint32_t device_id, uint32_t req,
			      const struct tcc_hsm_ioctl_set_modn_param *param)
{
	uint32_t data[128] = { 0 };
	uint32_t idx = 0;
	uint32_t rdata = 0;
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	uint32_t result = TCCHSM_ERR;

	if (param == NULL) {
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	data[idx] = param->addr;
	idx++;
	data[idx] = param->data_size;
	idx++;
	data[idx] = param->key_index;
	idx++;

	data_size = (uint32_t)(sizeof(uint32_t) * idx);

	rdata_size =
		sec_sendrecv_cmd(device_id, (req | MBOX_LOCATION_DATA), data,
				 data_size, (void *)&rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		result = TCCHSM_ERR_INVALID_STATE;
		goto out;
	}

	result = rdata;
	if (result != TCCHSM_SUCCESS) {
		ELOG("Error: 0x%x\n", result);
		goto out;
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_cmd_set_modn);

static uint32_t tcc_hsm_cmd_aes_fill_data(uint32_t req, uint32_t *data, uint32_t *data_size,
								const struct tcc_hsm_ioctl_aes_param *param,
								ulong desc, uint32_t descPhy_size)
{
	uint32_t op_mode = 0;
	uint32_t encType = 0;
	uint32_t result = TCCHSM_SUCCESS;
	uint32_t idx = 0;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	if ((param->src > UINT_MAX) || (param->dst > UINT_MAX)) {
		ELOG("src(0x%lx) or dst(0x%lx) addr err\n", param->src,
		     param->dst);
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	encType = (param->obj_id & 0x01000000U);
	op_mode = (param->obj_id & 0xF0000000U);

	data[idx] = op_mode;
	idx++;
	data[idx] = (param->obj_id & 0x0FFFFFFFU);
	idx++;
	data[idx] = HSM_DMA;
	idx++;

	if ((op_mode | OID_OPMODE_INIT) == OID_OPMODE_INIT) {
		data[idx] = param->key_size;
		idx++;
		if (param->key_size > 0U) {
			memcpy((void *)&data[idx], (const void *)param->aes_key,
			       param->key_size);
			idx += ((param->key_size + 3U) /
				(uint32_t)sizeof(uint32_t));
		}

		data[idx] = param->iv_size;
		idx++;
		if (param->iv_size > 0U) {
			data[idx] = param->counter_size;
			idx++;

			memcpy((void *)&data[idx], (const void *)param->iv,
			       param->iv_size);
			idx += ((param->iv_size + 3U) /
				(uint32_t)sizeof(uint32_t));
		}

		if (req == REQ_HSM_RUN_AES) {
			/* AAD */
			data[idx] = descPhy_size;
			idx++;
			if ((descPhy_size > 0U) && (desc <= UINT_MAX)) {
				data[idx] = (uint32_t)desc;
				idx++;
			}
		}
	}

	if ((((op_mode | OID_OPMODE_INIT) == OID_OPMODE_INIT) ||
	     ((op_mode | OID_OPMODE_FINAL) == OID_OPMODE_FINAL)) &&
	    (req == REQ_HSM_RUN_AES)) {
		data[idx] = param->tag_size;
		idx++;
	}

	if (((op_mode | OID_OPMODE_FINAL) == OID_OPMODE_FINAL) &&
	    (req == REQ_HSM_RUN_AES)) {
		if ((param->tag_size > 0U) && (encType == OID_AES_DECRYPT)) {
			memcpy((void *)&data[idx], (const void *)param->tag,
			       param->tag_size);
			idx += ((param->tag_size + 3U) /
				(uint32_t)sizeof(uint32_t));
		}
	}

	data[idx] = param->src_size;
	idx++;
	data[idx] = (uint32_t)param->src;
	idx++;
	data[idx] = param->dst_size;
	idx++;
	data[idx] = (uint32_t)param->dst;
	idx++;

	if (idx > 128U) {
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}
	*data_size = (uint32_t)(sizeof(uint32_t) * idx);

out:
	return result;
	
}

uint32_t tcc_hsm_cmd_run_aes(uint32_t device_id, uint32_t req,
			     struct tcc_hsm_ioctl_aes_param *param,
			     ulong desc, uint32_t descPhy_size)
{
	uint32_t data[128] = { 0 };
	uint32_t op_mode = 0;
	uint32_t rdata[12] = { 0 };
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	uint32_t encType = 0;
	uint32_t result = TCCHSM_ERR;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	encType = (param->obj_id & 0x01000000U);
	op_mode = (param->obj_id & 0xF0000000U);

	result = tcc_hsm_cmd_aes_fill_data(req, &data[0], &data_size, param, desc, descPhy_size);
	if (result != TCCHSM_SUCCESS){
		goto out;
	}

	rdata_size =
		sec_sendrecv_cmd(device_id, (req | MBOX_LOCATION_DATA), data,
				 data_size, (void *)rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		result = TCCHSM_ERR_INVALID_STATE;
		goto out;
	}

	result = rdata[0];
	if (result != TCCHSM_SUCCESS) {
		ELOG("Error: 0x%x\n", result);
		goto out;
	}

	if ((req == REQ_HSM_RUN_AES) && (param->tag_size > 0U) &&
	    (encType == OID_AES_ENCRYPT) &&
	    ((op_mode | OID_OPMODE_FINAL) == OID_OPMODE_FINAL)) {
		if (rdata[1] == param->tag_size) {
			memcpy((void *)param->tag, (const void *)&rdata[2],
			       param->tag_size);
		} else {
			ELOG("wrong tag_size(%d)\n", rdata[1]);
			result = TCCHSM_ERR_INVALID_STATE;
			goto out;
		}
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_cmd_run_aes);

static uint32_t tcc_hsm_cmd_aes_by_kt_fill_data(uint32_t req, uint32_t *data, uint32_t *data_size,
						const struct tcc_hsm_ioctl_aes_by_kt_param *param,
						ulong desc, uint32_t descPhy_size)
{
	uint32_t op_mode = 0;
	uint32_t encType = 0;
	uint32_t result = TCCHSM_SUCCESS;
	uint32_t idx = 0;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	if ((param->src > UINT_MAX) || (param->dst > UINT_MAX)) {
		ELOG("src(0x%lx) or dst(0x%lx) addr err\n", param->src,
		     param->dst);
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	encType = (param->obj_id & 0x01000000U);
	op_mode = (param->obj_id & 0xF0000000U);

	data[idx] = op_mode;
	idx++;
	data[idx] = (param->obj_id & 0x0FFFFFFFU);
	idx++;
	data[idx] = HSM_DMA;
	idx++;

	if ((op_mode | OID_OPMODE_INIT) == OID_OPMODE_INIT) {
		data[idx] = param->key_index;
		idx++;

		data[idx] = param->iv_size;
		idx++;
		if (param->iv_size > 0U) {
			data[idx] = param->counter_size;
			idx++;

			memcpy((void *)&data[idx], (const void *)param->iv,
			       param->iv_size);
			idx += ((param->iv_size + 3U) /
				(uint32_t)sizeof(uint32_t));
		}
		if (req == REQ_HSM_RUN_AES_BY_KT) {
			/* AAD */
			data[idx] = descPhy_size;
			idx++;
			if ((descPhy_size > 0U) && (desc <= UINT_MAX)) {
				data[idx] = (uint32_t)desc;
				idx++;
			}
		}
	}

	if ((((op_mode | OID_OPMODE_INIT) == OID_OPMODE_INIT) ||
	     ((op_mode | OID_OPMODE_FINAL) == OID_OPMODE_FINAL)) &&
	    (req == REQ_HSM_RUN_AES_BY_KT)) {
		data[idx] = param->tag_size;
		idx++;
	}

	if (((op_mode | OID_OPMODE_FINAL) == OID_OPMODE_FINAL) &&
	    (req == REQ_HSM_RUN_AES_BY_KT)) {
		if ((param->tag_size > 0U) && (encType == OID_AES_DECRYPT)) {
			memcpy((void *)&data[idx], (const void *)param->tag,
			       param->tag_size);
			idx += ((param->tag_size + 3U) /
				(uint32_t)sizeof(uint32_t));
		}
	}

	data[idx] = param->src_size;
	idx++;
	data[idx] = (uint32_t)param->src;
	idx++;
	data[idx] = param->dst_size;
	idx++;
	data[idx] = (uint32_t)param->dst;
	idx++;

	if (idx > 128U) {
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}
	
	*data_size = ((uint32_t)sizeof(uint32_t) * idx);

out:
	return result;
}

uint32_t tcc_hsm_cmd_run_aes_by_kt(uint32_t device_id, uint32_t req,
				   struct tcc_hsm_ioctl_aes_by_kt_param *param,
				   ulong desc, uint32_t descPhy_size)
{
	uint32_t data[128] = { 0 };
	uint32_t encType = 0;
	uint32_t op_mode = 0;
	uint32_t rdata[12] = { 0 };
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	uint32_t result = TCCHSM_ERR;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	encType = (param->obj_id & 0x01000000U);
	op_mode = (param->obj_id & 0xF0000000U);

	result = tcc_hsm_cmd_aes_by_kt_fill_data(req, &data[0], &data_size, param, desc, descPhy_size);
	if(result != TCCHSM_SUCCESS) {
		goto out;
	}

	rdata_size =
		sec_sendrecv_cmd(device_id, (req | MBOX_LOCATION_DATA), data,
				 data_size, (void *)rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		result = TCCHSM_ERR_INVALID_STATE;
		goto out;
	}

	result = rdata[0];
	if (result != TCCHSM_SUCCESS) {
		ELOG("Error: 0x%x\n", result);
		goto out;
	}
	if ((req == REQ_HSM_RUN_AES_BY_KT) && (param->tag_size > 0U) &&
	    (encType == OID_AES_ENCRYPT) &&
	    ((op_mode | OID_OPMODE_FINAL) == OID_OPMODE_FINAL)) {
		if (rdata[1] == param->tag_size) {
			memcpy((void *)param->tag, (const void *)&rdata[2],
			       param->tag_size);
		} else {
			ELOG("wrong tag_size(%d)\n", rdata[1]);
			result = TCCHSM_ERR_INVALID_STATE;
			goto out;
		}
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_cmd_run_aes_by_kt);

uint32_t tcc_hsm_cmd_gen_mac(uint32_t device_id, uint32_t req,
			     struct tcc_hsm_ioctl_mac_param *param)
{
	uint32_t data[128] = { 0 };
	uint32_t idx = 0;
	uint32_t op_mode = 0;
	uint32_t rdata[128] = { 0 };
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	uint32_t result = TCCHSM_ERR;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}
	
	if (param->src > UINT_MAX) {
		ELOG("src(0x%lx) addr err\n", param->src);
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	op_mode = (param->obj_id & 0xF0000000U);

	data[idx] = op_mode;
	idx++;
	data[idx] = (param->obj_id & 0x0FFFFFFFU);
	idx++;
	data[idx] = HSM_DMA;
	idx++;

	if ((op_mode | OID_OPMODE_INIT) == OID_OPMODE_INIT) {
		data[idx] = param->key_size;
		idx++;
		if (param->key_size > 0U) {
			memcpy((void *)&data[idx], (const void *)param->mac_key,
			       param->key_size);
		}
		idx += ((param->key_size + 3U) / (uint32_t)sizeof(uint32_t));
	}


	data[idx] = param->src_size;
	idx++;
	data[idx] = (uint32_t)param->src;
	idx++;
	data[idx] = param->mac_size;
	idx++;

	if ((req == REQ_HSM_VERIFY_CMAC) &&
	    ((op_mode | OID_OPMODE_FINAL) == OID_OPMODE_FINAL)) {
		memcpy((void *)&data[idx], (const void *)param->mac,
		       param->mac_size);
		idx += ((param->mac_size + 3U) / (uint32_t)sizeof(uint32_t));
	}

	if (idx > (UINT_MAX / (uint32_t)sizeof(uint32_t))) {
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}
	data_size = ((uint32_t)sizeof(uint32_t) * idx);
	rdata_size =
		sec_sendrecv_cmd(device_id, (req | MBOX_LOCATION_DATA), data,
				 data_size, (void *)rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		result = TCCHSM_ERR_INVALID_STATE;
		goto out;
	}

	result = rdata[0];
	if (result != TCCHSM_SUCCESS) {
		ELOG("Error: 0x%x\n", result);
		goto out;
	}

	if ((req != REQ_HSM_VERIFY_CMAC) &&
	    ((op_mode | OID_OPMODE_FINAL) == OID_OPMODE_FINAL)) {
		if (rdata[1] == param->mac_size) {
			memcpy((void *)param->mac, (const void *)&rdata[2],
			       param->mac_size);
		} else {
			ELOG("wrong mac_size(%d)\n", rdata[1]);
			result = TCCHSM_ERR_INVALID_STATE;
			goto out;
		}
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_cmd_gen_mac);

uint32_t tcc_hsm_cmd_gen_mac_by_kt(uint32_t device_id, uint32_t req,
				   struct tcc_hsm_ioctl_mac_by_kt_param *param)
{
	uint32_t data[128] = { 0 };
	uint32_t idx = 0;
	uint32_t op_mode = 0;
	uint32_t rdata[128] = { 0 };
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	uint32_t result = TCCHSM_ERR;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	if (param->src > UINT_MAX) {
		ELOG("src addr err(0x%lx)\n", param->src);
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	op_mode = (param->obj_id & 0xF0000000U);
	data[idx] = op_mode;
	idx++;
	data[idx] = (param->obj_id & 0x0FFFFFFFU);
	idx++;
	data[idx] = HSM_DMA;
	idx++;
	if ((op_mode | OID_OPMODE_INIT) == OID_OPMODE_INIT) {
		data[idx] = param->key_index;
	}
	idx++;
	data[idx] = param->src_size;
	idx++;
	data[idx] = (uint32_t)param->src;
	idx++;
	data[idx] = param->mac_size;
	idx++;

	if ((req == REQ_HSM_VERIFY_CMAC_BY_KT) &&
	    ((op_mode | OID_OPMODE_FINAL) == OID_OPMODE_FINAL)) {
		memcpy((void *)&data[idx], (const void *)param->mac,
		       param->mac_size);
		idx += ((param->mac_size + 3U) / (uint32_t)sizeof(uint32_t));
	}

	if (idx > (UINT_MAX / (uint32_t)sizeof(uint32_t))) {
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}
	data_size = ((uint32_t)sizeof(uint32_t) * idx);
	rdata_size =
		sec_sendrecv_cmd(device_id, (req | MBOX_LOCATION_DATA), data,
				 data_size, (void *)rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		result = TCCHSM_ERR_INVALID_STATE;
		goto out;
	}

	result = rdata[0];
	if (result != TCCHSM_SUCCESS) {
		ELOG("Error: 0x%x\n", result);
		goto out;
	}

	if ((req != REQ_HSM_VERIFY_CMAC_BY_KT) &&
	    ((op_mode | OID_OPMODE_FINAL) == OID_OPMODE_FINAL)) {
		if (rdata[1] == param->mac_size) {
			memcpy((void *)param->mac, (const void *)&rdata[2],
			       param->mac_size);
		} else {
			ELOG("wrong mac_size(%d)\n", rdata[1]);
			result = TCCHSM_ERR_INVALID_STATE;
			goto out;
		}
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_cmd_gen_mac_by_kt);

uint32_t tcc_hsm_cmd_gen_hash(uint32_t device_id, uint32_t req,
			      struct tcc_hsm_ioctl_hash_param *param)
{
	uint32_t data[128] = { 0 };
	uint32_t idx = 0;
	uint32_t op_mode = 0;
	uint32_t rdata[128] = { 0 };
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	uint32_t result = TCCHSM_ERR;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	if (param->src > UINT_MAX) {
		ELOG("src size err(0x%lx)\n", param->src);
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	op_mode = (param->obj_id & 0xF0000000U);

	data[idx] = op_mode;
	idx++;
	data[idx] = (param->obj_id & 0x0FFFFFFFU);
	idx++;
	data[idx] = HSM_DMA;
	idx++;
	data[idx] = param->src_size;
	idx++;
	data[idx] = (uint32_t)param->src;
	idx++;
	data[idx] = param->digest_size;
	idx++;

	data_size = ((uint32_t)sizeof(uint32_t) * idx);
	rdata_size =
		sec_sendrecv_cmd(device_id, (req | MBOX_LOCATION_DATA), data,
				 data_size, (void *)rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		result = TCCHSM_ERR_INVALID_STATE;
		goto out;
	}

	result = rdata[0];
	if (result != TCCHSM_SUCCESS) {
		ELOG("Error: 0x%x\n", result);
		goto out;
	}

	if ((op_mode | OID_OPMODE_FINAL) == OID_OPMODE_FINAL) {
		if (rdata[1] == param->digest_size) {
			memcpy((void *)param->digest, (const void *)&rdata[2],
			       param->digest_size);
		} else {
			ELOG("wrong sig_size(%d)\n", rdata[1]);
			result = TCCHSM_ERR_INVALID_STATE;
			goto out;
		}
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_cmd_gen_hash);

uint32_t tcc_hsm_cmd_run_ecdh_phaseI(uint32_t device_id, uint32_t req,
				     struct tcc_hsm_ioctl_ecdh_key_param *param)
{
	uint32_t data[4] = { 0 };
	uint32_t idx = 0;
	uint32_t rdata[64] = { 0 };
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	uint32_t result = 0;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	data[idx] = param->key_type;
	idx++;
	data[idx] = param->obj_id;
	idx++;
	data[idx] = param->prikey_size;
	idx++;
	data[idx] = param->pubkey_size;
	idx++;
	data_size = ((uint32_t)sizeof(uint32_t) * idx);

	rdata_size =
		sec_sendrecv_cmd(device_id, (req | MBOX_LOCATION_DATA), data,
				 data_size, (void *)rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		result = TCCHSM_ERR_INVALID_STATE;
		goto out;
	}

	idx = 0;

	result = rdata[idx];
	idx++;
	if (result != TCCHSM_SUCCESS) {
		ELOG("Error: 0x%x\n", result);
		goto out;
	}

	if (rdata[idx] == (param->prikey_size + param->pubkey_size)) {
		idx++;
		if((param->prikey_size > TCC_HSM_ECDSA_MAX_KEY_SIZE) ||
		    ( param->pubkey_size > (TCC_HSM_ECDSA_MAX_KEY_SIZE *2u))){
			ELOG("Error Invalid Param\n");
			result = TCCHSM_ERR_INVALID_PARAM;
		} else {
			memcpy((void *)param->prikey, (const void *)&rdata[idx],
		       param->prikey_size);
			idx += ((param->prikey_size + 3U) / (uint32_t)sizeof(uint32_t));

			memcpy((void *)param->pubkey, (const void *)&rdata[idx],
			       param->pubkey_size);
		}
	} else {
		ELOG("wrong prikey_size(%d)\n", rdata[idx]);
		result = TCCHSM_ERR_INVALID_STATE;
		goto out;
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_cmd_run_ecdh_phaseI);

uint32_t tcc_hsm_cmd_run_ecdsa(uint32_t device_id, uint32_t req,
			       const struct tcc_hsm_ioctl_ecdsa_param *param)
{
	uint32_t data[128] = { 0 };
	uint32_t idx = 0;
	uint32_t rdata[128] = { 0 };
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	uint32_t result = TCCHSM_ERR;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	data[idx] = param->obj_id;
	idx++;
	data[idx] = param->key_size;
	idx++;

	if (param->key_size > 0U) {
		memcpy((void *)&data[idx], (const void *)param->ecdsa_key,
		       param->key_size);
		idx += ((param->key_size + 3U) / (uint32_t)sizeof(uint32_t));
	}
	data[idx] = param->digest_size;
	idx++;
	memcpy((void *)&data[idx], (const void *)param->digest,
	       param->digest_size);
	idx += ((param->digest_size + 3U) / (uint32_t)sizeof(uint32_t));

	data[idx] = param->sig_size;
	idx++;
	if (req == REQ_HSM_RUN_ECDSA_VERIFY) {
		memcpy(&data[idx], param->sig, param->sig_size);
		idx += ((param->sig_size + 3U) / (uint32_t)sizeof(uint32_t));
	}

	if (idx > (UINT_MAX / (uint32_t)sizeof(uint32_t))) {
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}
	data_size = ((uint32_t)sizeof(uint32_t) * idx);

	rdata_size =
		sec_sendrecv_cmd(device_id, (req | MBOX_LOCATION_DATA), data,
				 data_size, (void *)rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		result = TCCHSM_ERR_INVALID_STATE;
		goto out;
	}

	result = rdata[0];
	if (result != TCCHSM_SUCCESS) {
		ELOG("Error: 0x%x\n", result);
		goto out;
	}

	if (req == REQ_HSM_RUN_ECDSA_SIGN) {
		if (rdata[1] == param->sig_size) {
			memcpy((void *)param->sig, (const void *)&rdata[2],
			       param->sig_size);
		} else {
			ELOG("wrong sig_size(%d)\n", rdata[1]);
			result = TCCHSM_ERR_INVALID_STATE;
			goto out;
		}
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_cmd_run_ecdsa);

uint32_t
tcc_hsm_cmd_run_ecdsa_by_kt(uint32_t device_id, uint32_t req,
			    const struct tcc_hsm_ioctl_ecdsa_by_kt_param *param)
{
	uint32_t data[128] = { 0 };
	uint32_t idx = 0;
	uint32_t rdata[128] = { 0 };
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	uint32_t result = TCCHSM_ERR;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	data[idx] = param->obj_id;
	idx++;
	data[idx] = param->key_index;
	idx++;

	data[idx] = param->digest_size;
	idx++;
	memcpy((void *)&data[idx], (const void *)param->digest,
	       param->digest_size);
	idx += ((param->digest_size + 3U) / (uint32_t)sizeof(uint32_t));

	data[idx] = param->sig_size;
	idx++;
	if (req == REQ_HSM_RUN_ECDSA_VERIFY_BY_KT) {
		memcpy(&data[idx], param->sig, param->sig_size);
		idx += ((param->sig_size + 3U) / (uint32_t)sizeof(uint32_t));
	}

	if (idx > (UINT_MAX / (uint32_t)sizeof(uint32_t))) {
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}
	data_size = ((uint32_t)sizeof(uint32_t) * idx);

	rdata_size =
		sec_sendrecv_cmd(device_id, (req | MBOX_LOCATION_DATA), data,
				 data_size, (void *)rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		result = TCCHSM_ERR_INVALID_STATE;
		goto out;
	}

	result = rdata[0];
	if (result != TCCHSM_SUCCESS) {
		ELOG("Error: 0x%x\n", result);
		goto out;
	}

	if (req == REQ_HSM_RUN_ECDSA_SIGN_BY_KT) {
		if (rdata[1] == param->sig_size) {
			memcpy((void *)param->sig, (const void *)&rdata[2],
			       param->sig_size);
		} else {
			ELOG("wrong sig_size(%d)\n", rdata[1]);
			result = TCCHSM_ERR_INVALID_STATE;
			goto out;
		}
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_cmd_run_ecdsa_by_kt);

uint32_t tcc_hsm_cmd_run_rsaes(uint32_t device_id, uint32_t req,
			       struct tcc_hsm_ioctl_rsaes_param *param,
			       ulong src, ulong dst)
{
	uint32_t data[128] = { 0 };
	uint32_t idx = 0;
	uint32_t rdata[128] = { 0 };
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	uint32_t result = TCCHSM_ERR;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	if ((src > (UINT_MAX - (TCC_HSM_RSA_MODN_SIZE * 3U))) || (dst > UINT_MAX)) {
		ELOG("src(0x%lx) or dst(0x%lx) addr err\n", src,
		     dst);
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	data[idx] = param->obj_id;
	idx++;
	data[idx] = HSM_DMA;
	idx++;
	data[idx] = param->modN_size;
	idx++;
	data[idx] = (uint32_t)src; //modN
	idx++;
	data[idx] = param->key_size;
	idx++;
	data[idx] = (uint32_t)(src + (TCC_HSM_RSA_MODN_SIZE)); //rsa_key
	idx++;
	data[idx] = param->src_size;
	idx++;
	data[idx] = (uint32_t)(src + (TCC_HSM_RSA_MODN_SIZE * 2u)); //src
	idx++;
	data[idx] = param->dst_size;
	idx++;
	data[idx] = (uint32_t)dst;
	idx++;

	if ((req == REQ_HSM_RUN_RSAES_OAEP_ENC) ||
	    (req == REQ_HSM_RUN_RSAES_OAEP_DEC)) {
		data[idx] = param->label_size;
		idx++;
		data[idx] = (uint32_t)(src + (TCC_HSM_RSA_MODN_SIZE * 3u)); //label
		idx++;
	}

	data_size = ((uint32_t)sizeof(uint32_t) * idx);

	rdata_size =
		sec_sendrecv_cmd(device_id, (req | MBOX_LOCATION_DATA), data,
				 data_size, (void *)rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		result = TCCHSM_ERR_INVALID_STATE;
		goto out;
	}

	result = rdata[0];
	if (result != TCCHSM_SUCCESS) {
		ELOG("Error: 0x%x\n", result);
		goto out;
	}

	if (rdata[1] == sizeof(uint32_t)) {
		param->dst_size = rdata[2];
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_cmd_run_rsaes);

uint32_t
tcc_hsm_cmd_run_rsaes_by_kt(uint32_t device_id, uint32_t req,
			    struct tcc_hsm_ioctl_rsaes_by_kt_param *param,
			    ulong src, ulong dst)
{
	uint32_t data[128] = { 0 };
	uint32_t idx = 0;
	uint32_t rdata[128] = { 0 };
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	uint32_t result = TCCHSM_ERR;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	if ((src> (UINT_MAX - TCC_HSM_RSA_MODN_SIZE)) || (dst > UINT_MAX)) {
		ELOG("src(0x%lx) or dst(0x%lx) addr err\n", src,
		     dst);
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	data[idx] = param->obj_id;
	idx++;
	data[idx] = HSM_DMA;
	idx++;
	data[idx] = param->key_index;
	idx++;
	data[idx] = param->src_size;
	idx++;
	data[idx] = (uint32_t)src;
	idx++;
	data[idx] = param->dst_size;
	idx++;
	data[idx] = (uint32_t)dst;
	idx++;

	if ((req == REQ_HSM_RUN_RSAES_OAEP_ENC_BY_KT) ||
	    (req == REQ_HSM_RUN_RSAES_OAEP_DEC_BY_KT)) {
		data[idx] = param->label_size;
		idx++;
		data[idx] = (uint32_t)(src + TCC_HSM_RSA_MODN_SIZE); //label
		idx++;
	}

	data_size = ((uint32_t)sizeof(uint32_t) * idx);

	rdata_size =
		sec_sendrecv_cmd(device_id, (req | MBOX_LOCATION_DATA), data,
				 data_size, (void *)rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		result = TCCHSM_ERR_INVALID_STATE;
		goto out;
	}

	result = rdata[0];
	if (result != TCCHSM_SUCCESS) {
		ELOG("Error: 0x%x\n", result);
		goto out;
	}

	if (rdata[1] == sizeof(uint32_t)) {
		param->dst_size = rdata[2];
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_cmd_run_rsaes_by_kt);

uint32_t tcc_hsm_cmd_run_rsassa(uint32_t device_id, uint32_t req,
				const struct tcc_hsm_ioctl_rsassa_param *param,
				ulong src_addr, ulong sig)
{
	uint32_t data[128] = { 0 };
	uint32_t idx = 0;
	uint32_t rdata[128] = { 0 };
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	uint32_t result = TCCHSM_ERR;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	if ((src_addr> (UINT_MAX - TCC_HSM_RSA_MODN_SIZE * 2u)) || (sig > UINT_MAX)) {
		ELOG("src_addr(0x%lx) or dst(0x%lx) addr err\n", src_addr,
		     sig);
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	data[idx] = param->obj_id;
	idx++;
	data[idx] = HSM_DMA;
	idx++;
	data[idx] = param->modN_size;
	idx++;
	data[idx] = (uint32_t)src_addr; //modN
	idx++;
	data[idx] = param->key_size;
	idx++;
	data[idx] = (uint32_t)(src_addr + TCC_HSM_RSA_MODN_SIZE); //rsa_key
	idx++;
	data[idx] = param->digest_size;
	idx++;
	data[idx] = (uint32_t)(src_addr + (TCC_HSM_RSA_MODN_SIZE * 2u));//digest
	idx++;
	data[idx] = param->sig_size;
	idx++;
	data[idx] = (uint32_t)sig;
	idx++;

	data_size = ((uint32_t)sizeof(uint32_t) * idx);

	rdata_size =
		sec_sendrecv_cmd(device_id, (req | MBOX_LOCATION_DATA), data,
				 data_size, (void *)rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		result = TCCHSM_ERR_INVALID_STATE;
		goto out;
	}

	result = rdata[0];
	if (result != TCCHSM_SUCCESS) {
		ELOG("Error: 0x%x\n", result);
		goto out;
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_cmd_run_rsassa);

uint32_t
tcc_hsm_cmd_run_rsassa_by_kt(uint32_t device_id, uint32_t req,
			     const struct tcc_hsm_ioctl_rsassa_by_kt_param *param,
			     ulong digest, ulong sig)
{
	uint32_t data[128] = { 0 };
	uint32_t idx = 0;
	uint32_t rdata[128] = { 0 };
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	uint32_t result = TCCHSM_ERR;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	if ((digest > (UINT_MAX)) || (sig > UINT_MAX)) {
		ELOG("digest(0x%lx) or sig(0x%lx) addr err\n", digest,
		     sig);
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	data[idx] = param->obj_id;
	idx++;
	data[idx] = HSM_DMA;
	idx++;
	data[idx] = param->key_index;
	idx++;
	data[idx] = param->digest_size;
	idx++;
	data[idx] = (uint32_t)digest;
	idx++;
	data[idx] = param->sig_size;
	idx++;
	data[idx] = (uint32_t)sig;
	idx++;

	data_size = ((uint32_t)sizeof(uint32_t) * idx);

	rdata_size =
		sec_sendrecv_cmd(device_id, (req | MBOX_LOCATION_DATA), data,
				 data_size, (void *)rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		result = TCCHSM_ERR_INVALID_STATE;
		goto out;
	}

	result = rdata[0];
	if (result != TCCHSM_SUCCESS) {
		ELOG("Error: 0x%x\n", result);
		goto out;
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_cmd_run_rsassa_by_kt);

uint32_t tcc_hsm_cmd_write(uint32_t device_id, uint32_t req,
			   const struct tcc_hsm_ioctl_write_param *param)
{
	uint32_t data[128] = { 0 };
	uint32_t idx = 0;
	uint32_t rdata = 0;
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	uint32_t result = TCCHSM_ERR;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	data[idx] = param->addr;
	idx++;
	data[idx] = param->dma;
	idx++;
	data[idx] = param->data_size;
	idx++;

	if (param->dma == HSM_DMA) {
		if (param->data > UINT_MAX) {
			ELOG("data(0x%lx) addr err\n", param->data);
			result = TCCHSM_ERR_INVALID_PARAM;
			goto out;
		}
		data[idx] = (uint32_t)param->data;
		idx++;
	} else {
		memcpy((void *)&data[idx], (const void *)param->data,
		       param->data_size);
		idx += ((param->data_size + 3U) / (uint32_t)sizeof(uint32_t));
	}

	if (idx > (UINT_MAX / (uint32_t)sizeof(uint32_t))) {
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}
	data_size = (uint32_t)(sizeof(uint32_t) * idx);

	rdata_size =
		sec_sendrecv_cmd(device_id, (req | MBOX_LOCATION_DATA), data,
				 data_size, (void *)&rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		result = TCCHSM_ERR_INVALID_STATE;
		goto out;
	}

	result = rdata;
	if (result != TCCHSM_SUCCESS) {
		ELOG("Error: 0x%x\n", result);
		goto out;
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_cmd_write);

uint32_t tcc_hsm_cmd_get_version(uint32_t device_id, uint32_t req,
				 struct tcc_hsm_ioctl_version_param *param)
{
	uint32_t data[128] = { 0 };
	uint32_t idx = 0;
	uint32_t rdata[128] = { 0 };
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	uint32_t result = TCCHSM_ERR;

	if (param == NULL) {
		ELOG("Invalid parameter!\n");
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	data_size = ((uint32_t)sizeof(uint32_t) * idx);

	rdata_size =
		sec_sendrecv_cmd(device_id, (req | MBOX_LOCATION_DATA), data,
				 data_size, (void *)rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		result = TCCHSM_ERR_INVALID_STATE;
		goto out;
	}

	result = rdata[0];
	if (result != TCCHSM_SUCCESS) {
		ELOG("Error: 0x%x\n", result);
		goto out;
	}

	if ((uint32_t)rdata[1] == ((uint32_t)sizeof(uint32_t) * 3U)) {
		param->x = rdata[2];
		param->y = rdata[3];
		param->z = rdata[4];
	} else {
		ELOG("wrong sig_size(%d)\n", rdata[1]);
		result = TCCHSM_ERR_INVALID_STATE;
		goto out;
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_cmd_get_version);

uint32_t tcc_hsm_cmd_get_rand(uint32_t device_id, uint32_t req, ulong rng,
			      uint32_t rng_size)
{
	uint32_t data[128] = { 0 };
	uint32_t idx = 0;
	uint32_t rdata = 0;
	uint32_t data_size = 0;
	int32_t rdata_size = 0;
	uint32_t result = 0;

	if (rng > UINT_MAX) {
		ELOG("rng(0x%lx) addr err\n", rng);
		result = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	data[idx] = HSM_DMA;
	idx++;
	data[idx] = rng_size;
	idx++;
	data[idx] = (uint32_t)rng;
	idx++;

	data_size = ((uint32_t)sizeof(uint32_t) * idx);

	rdata_size =
		sec_sendrecv_cmd(device_id, (req | MBOX_LOCATION_DATA), data,
				 data_size, (void *)&rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		result = TCCHSM_ERR_INVALID_STATE;
		goto out;
	}

	result = rdata;
	if (result != TCCHSM_SUCCESS) {
		ELOG("Error: 0x%x\n", result);
		goto out;
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_cmd_get_rand);

MODULE_AUTHOR("linux <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips TCC HSM driver");
MODULE_LICENSE("GPL");