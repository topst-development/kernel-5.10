// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#define NDEBUG
#define TLOG_LEVEL TLOG_DEBUG
#include "tcc_thsm_log.h"

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mailbox/mailbox-tcc.h>
#include <linux/mailbox/tcc_sec_ipc.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/wait.h>

#include <linux/io.h>
#include <linux/uaccess.h>
#include <asm/div64.h>

#include "tcc_thsm_cmd.h"

#define DMA_MAX_RSIZE (1 * 1024 * 1024)

int32_t tcc_thsm_cmd_init(uint32_t device_id)
{
	int32_t rdata = 0;
	int32_t rdata_size = 0;
	int32_t result = -1;

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_INIT, NULL, 0,
				      &rdata, sizeof(rdata));
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}
	result = rdata;
	if (result != 0) {
		ELOG("error: %d\n", result);
		return -EBADR;
	}

	return result;
}

int32_t tcc_thsm_cmd_finalize(uint32_t device_id)
{
	int32_t rdata = 0;
	int32_t rdata_size = 0;
	int32_t result = -1;

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_FINALIZE, NULL, 0,
				      &rdata, sizeof(rdata));
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}
	result = rdata;
	if (result != 0) {
		ELOG("error: %d\n", result);
		return -EBADR;
	}

	return result;
}

int32_t tcc_thsm_cmd_get_ta_version(uint32_t device_id, uint32_t *major,
				    uint32_t *minor)
{
	int32_t rdata[4] = { 0 };
	int32_t rdata_size = 0;
	int32_t result = 0;

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_GET_TA_VERSION,
				      NULL, 0, rdata, sizeof(rdata));
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result != 0) {
		ELOG("error: %d\n", result);
		return result;
	}

	rdata_size = rdata[1];
	if (rdata_size == (sizeof(uint32_t) * 2)) {
		*major = rdata[2];
		*minor = rdata[3];
	} else {
		ELOG("Wrong data size = %d\n", rdata_size);
		return -EBADR;
	}

	return result;
}

int32_t tcc_thsm_cmd_get_ca_version(uint32_t device_id, uint32_t *major,
				    uint32_t *minor)
{
	int32_t rdata[4] = { 0 };
	int32_t rdata_size = 0;
	int32_t result = 0;

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_GET_CA_VERSION,
				      NULL, 0, rdata, sizeof(rdata));
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result != 0) {
		ELOG("error: %d\n", result);
		return result;
	}

	rdata_size = rdata[1];
	if (rdata_size == (sizeof(uint32_t) * 2)) {
		*major = rdata[2];
		*minor = rdata[3];
	} else {
		ELOG("Wrong data size = %d\n", rdata_size);
		return -EBADR;
	}

	return result;
}

int32_t tcc_thsm_cmd_get_daemon_version(uint32_t device_id, uint32_t *major,
					uint32_t *minor)
{
	int32_t rdata[4] = { 0 };
	int32_t rdata_size = 0;
	int32_t result = 0;

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_GET_DAEMON_VERSION,
				      NULL, 0, rdata, sizeof(rdata));
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result != 0) {
		ELOG("error: %d\n", result);
		return result;
	}

	rdata_size = rdata[1];
	if (rdata_size == (sizeof(uint32_t) * 2)) {
		*major = rdata[2];
		*minor = rdata[3];
	} else {
		ELOG("Wrong data size = %d\n", rdata_size);
		return -EBADR;
	}

	return result;
}

int32_t tcc_thsm_cmd_set_mode(uint32_t device_id, uint32_t key_index,
			      uint32_t algorithm, uint32_t op_mode,
			      uint32_t key_size)
{
	uint32_t data[4] = { 0 };
	int32_t data_size = 0;
	int32_t rdata = 0, rdata_size = 0;
	int32_t result = 0;

	data[0] = key_index;
	data[1] = algorithm;
	data[2] = op_mode;
	data[3] = key_size;
	data_size = (sizeof(int32_t) * 4);

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_SET_MODE, data,
				      data_size, &rdata, sizeof(rdata));
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != 0) {
		ELOG("Error: %d\n", result);
		return result;
	}

	return result;
}

int32_t tcc_thsm_cmd_set_key(uint32_t device_id, uint32_t key_index,
			     uint32_t *key1, uint32_t key1_size, uint32_t *key2,
			     uint32_t key2_size, uint32_t *key3,
			     uint32_t key3_size)
{
	uint32_t data[128] = { 0 };
	int32_t rdata = 0;
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	data[0] = key_index;
	data[1] = key1_size;
	data[2] = key2_size;
	data[3] = key3_size;
	if (key1 != NULL && key1_size > 0) {
		memcpy(&data[4], key1, key1_size);
	}
	if (key2 != NULL && key2_size > 0) {
		memcpy(&data[4 + key1_size], key2, key2_size);
	}
	if (key3 != NULL && key3_size > 0) {
		memcpy(&data[4 + key1_size + key2_size], key3, key3_size);
	}
	data_size = (sizeof(uint32_t) * 4) + key1_size + key2_size + key3_size;

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_SET_KEY, data,
				      data_size, &rdata, sizeof(rdata));
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != 0) {
		ELOG("Error: %d\n", result);
		return result;
	}

	return result;
}

int32_t tcc_thsm_cmd_set_key_v2(uint32_t device_id, uint32_t key_index,
				struct TEETHSMKey *key)
{
	uint32_t data[128] = { 0 };
	int32_t rdata = 0;
	int32_t data_size = 0, rdata_size = 0;
	int32_t idx = 0;
	int32_t result = 0;

	/* Set data FIFO */
	data[idx] = key_index;
	idx++;
	data[idx] = key->keyType;
	idx++;

	/* Set data FIFO */
	switch (key->keyType) {
	case TCCTHSM_KEY_TYPE_SECRET:
		data[idx] = key->key.SECRET.key1Size;
		idx++;
		data[idx] = key->key.SECRET.key2Size;
		idx++;
		data[idx] = 0; // Key3 does not exist in KEY_TYPE_SECRET
		idx++;
		memcpy((void *)&data[idx], (const void *)key->key.SECRET.key1,
		       key->key.SECRET.key1Size);
		idx += ((key->key.SECRET.key1Size + 3u) / sizeof(uint32_t));
		if (key->key.SECRET.key2Size != 0u) {
			memcpy((void *)&data[idx],
			       (const void *)key->key.SECRET.key2,
			       key->key.SECRET.key2Size);
			idx += ((key->key.SECRET.key2Size + 3u) /
				sizeof(uint32_t));
		}
		break;

	case TCCTHSM_KEY_TYPE_RSA_PAIR:
		data[idx] = key->key.RSA.modulusSize;
		idx++;
		data[idx] = key->key.RSA.publicExpSize;
		idx++;
		data[idx] = key->key.RSA.privateExpSize;
		idx++;
		memcpy((void *)&data[idx], (const void *)key->key.RSA.modulus,
		       key->key.RSA.modulusSize);
		idx += ((key->key.RSA.modulusSize + 3u) / sizeof(uint32_t));
		memcpy((void *)&data[idx], (const void *)key->key.RSA.publicExp,
		       key->key.RSA.publicExpSize);
		idx += ((key->key.RSA.publicExpSize + 3u) / sizeof(uint32_t));
		memcpy((void *)&data[idx],
		       (const void *)key->key.RSA.privateExp,
		       key->key.RSA.privateExpSize);
		idx += ((key->key.RSA.privateExpSize + 3u) / sizeof(uint32_t));
		break;

	case TCCTHSM_KEY_TYPE_RSA_PUBLIC:
		data[idx] = key->key.RSA.modulusSize;
		idx++;
		data[idx] = key->key.RSA.publicExpSize;
		idx++;
		data[idx] =
			0; // Key3 does not exist in TEETHSM_KEY_TYPE_RSA_PUBLIC
		idx++;
		memcpy((void *)&data[idx], (const void *)key->key.RSA.modulus,
		       key->key.RSA.modulusSize);
		idx += ((key->key.RSA.modulusSize + 3u) / sizeof(uint32_t));
		memcpy((void *)&data[idx], (const void *)key->key.RSA.publicExp,
		       key->key.RSA.publicExpSize);
		idx += ((key->key.RSA.publicExpSize + 3u) / sizeof(uint32_t));
		break;

	case TCCTHSM_KEY_TYPE_ECDSA_PAIR:
		data[idx] = key->key.ECDSA.publicXSize;
		idx++;
		data[idx] = key->key.ECDSA.publicYSize;
		idx++;
		data[idx] = key->key.ECDSA.privateSize;
		idx++;
		memcpy((void *)&data[idx], (const void *)key->key.ECDSA.publicX,
		       key->key.ECDSA.publicXSize);
		idx += ((key->key.ECDSA.publicXSize + 3u) / sizeof(uint32_t));
		memcpy((void *)&data[idx], (const void *)key->key.ECDSA.publicY,
		       key->key.ECDSA.publicYSize);
		idx += ((key->key.ECDSA.publicYSize + 3u) / sizeof(uint32_t));
		memcpy((void *)&data[idx], (const void *)key->key.ECDSA.private,
		       key->key.ECDSA.privateSize);
		idx += ((key->key.ECDSA.privateSize + 3u) / sizeof(uint32_t));
		break;

	case TCCTHSM_KEY_TYPE_ECDSA_PUBLIC:
		data[idx] = key->key.ECDSA.publicXSize;
		idx++;
		data[idx] = key->key.ECDSA.publicYSize;
		idx++;
		data[idx] = 0; // Key3 does not exist in
			// TEETHSM_KEY_TYPE_ECDSA_PUBLIC
		idx++;
		memcpy((void *)&data[idx], (const void *)key->key.ECDSA.publicX,
		       key->key.ECDSA.publicXSize);
		idx += ((key->key.ECDSA.publicXSize + 3u) / sizeof(uint32_t));
		memcpy((void *)&data[idx], (const void *)key->key.ECDSA.publicY,
		       key->key.ECDSA.publicYSize);
		idx += ((key->key.ECDSA.publicYSize + 3u) / sizeof(uint32_t));
		break;

	default:
		ELOG("Key type Err(%d)\n", key->keyType);
		break;
	}

	data_size = (idx * sizeof(uint32_t));

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_SET_KEY_V2, data,
				      data_size, &rdata, sizeof(rdata));

	if (rdata_size < 0) {
		DLOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != 0) {
		DLOG("Error: %d\n", result);
		return result;
	}

	return result;
}

int32_t tcc_thsm_cmd_set_key_from_storage(uint32_t device_id,
					  uint32_t key_index, uint8_t *obj_id,
					  uint32_t obj_id_len)
{
	uint32_t data[64] = { 0 };
	int32_t rdata = 0;
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	data[0] = key_index;
	data[1] = obj_id_len;
	if (obj_id != NULL && obj_id_len > 0) {
		memcpy((uint8_t *)&data[2], obj_id, obj_id_len);
	}
	data_size = (sizeof(uint32_t) * 2) + obj_id_len;

	rdata_size =
		sec_sendrecv_cmd(device_id, TCCTHSM_EVT_SET_KEY_FROM_STORAGE,
				 data, data_size, &rdata, sizeof(rdata));
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != 0) {
		ELOG("Error: %d\n", result);
		return result;
	}

	return result;
}

int32_t tcc_thsm_cmd_set_key_from_otp(uint32_t device_id, uint32_t key_index,
				      uint32_t otp_addr, uint32_t key_size)
{
	uint32_t data[64] = { 0 };
	int32_t rdata = 0;
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	data[0] = key_index;
	data[1] = otp_addr;
	data[2] = key_size;
	data_size = (sizeof(uint32_t) * 3);

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_SET_KEY_FROM_OTP,
				      data, data_size, &rdata, sizeof(rdata));
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != 0) {
		ELOG("Error: %d\n", result);
		return result;
	}

	return result;
}

int32_t tcc_thsm_cmd_free_mode(uint32_t device_id, uint32_t key_index)
{
	uint32_t data;
	int32_t data_size = 0;
	int32_t rdata = 0;
	int32_t rdata_size = 0;
	int32_t result = 0;

	data = key_index;
	data_size = sizeof(uint32_t);

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_FREE_MODE, &data,
				      data_size, &rdata, sizeof(rdata));
	if (rdata_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != 0) {
		ELOG("Error: %d\n", result);
		return result;
	}

	return result;
}

int32_t tcc_thsm_cmd_set_iv_symmetric(uint32_t device_id, uint32_t key_index,
				      uint32_t *iv, uint32_t iv_size)
{
	uint32_t data[32] = { 0 };
	int32_t rdata = 0;
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	data[0] = key_index;
	data[1] = iv_size;
	data_size = (sizeof(uint32_t) * 2) + iv_size;

	if (iv != NULL && iv_size > 0) {
		memcpy(&data[2], iv, iv_size);
	}

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_SET_IV_SYMMETRIC,
				      data, data_size, &rdata, sizeof(rdata));
	if (rdata_size < 0) {
		DLOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != 0) {
		DLOG("Error: %d\n", result);
		return result;
	}

	return result;
}

int32_t tcc_thsm_cmd_run_cipher_by_dma(uint32_t device_id, uint32_t key_index,
				       uint32_t src_addr, uint32_t src_size,
				       uint32_t dst_addr, uint32_t *dst_size,
				       uint32_t flag)
{
	uint32_t data[6] = { 0 };
	int32_t rdata[2] = { 0 };
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	data[0] = key_index;
	data[1] = flag;
	data[2] = src_addr;
	data[3] = src_size;
	data[4] = dst_addr;
	data_size = (sizeof(uint32_t) * 5);

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_RUN_CIPHER_BY_DMA,
				      data, data_size, rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		DLOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result != 0) {
		DLOG("Error: %d\n", result);
		return result;
	}
	*dst_size = rdata[1];

	return result;
}

int32_t tcc_thsm_cmd_run_digest_by_dma(uint32_t device_id, uint32_t key_index,
				       uint32_t chunk_addr, uint32_t chunk_len,
				       uint8_t *hash, uint32_t *hash_len,
				       uint32_t flag)
{
	uint32_t data[8] = { 0 };
	int32_t rdata[32] = { 0 };
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	data[0] = key_index;
	data[1] = flag;
	data[2] = chunk_addr;
	data[3] = chunk_len;
	data[4] = *hash_len;

	data_size = (sizeof(uint32_t) * 5);

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_RUN_DIGEST_BY_DMA,
				      data, data_size, rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		DLOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result != 0) {
		DLOG("Error: %d\n", result);
		return result;
	}
	*hash_len = rdata[1];
	if (*hash_len > 0) {
		memcpy(hash, (uint8_t *)&rdata[2], *hash_len);
	} else {
		DLOG("wrong hash_len(%d)\n", *hash_len);
		return -EBADR;
	}

	return result;
}

int32_t tcc_thsm_cmd_set_iv_mac(uint32_t device_id, uint32_t key_index,
				uint32_t *iv, uint32_t iv_size)
{
	uint32_t data[32] = { 0 };
	int32_t rdata = 0;
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	data[0] = key_index;
	data[1] = iv_size;
	if (iv != NULL && iv_size > 0) {
		memcpy(&data[2], iv, iv_size);
	}

	data_size = (sizeof(uint32_t) * 2) + iv_size;

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_SET_IV_MAC, data,
				      data_size, &rdata, sizeof(rdata));
	if (rdata_size < 0) {
		DLOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != 0) {
		DLOG("Error: %d\n", result);
		return result;
	}

	return result;
}

int32_t tcc_thsm_cmd_compute_mac_by_dma(uint32_t device_id, uint32_t key_index,
					uint32_t message, uint32_t message_len,
					uint8_t *mac, uint32_t *mac_len,
					uint32_t flag)
{
	uint32_t data[8] = { 0 };
	int32_t rdata[32] = { 0 };
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	data[0] = key_index;
	data[1] = flag;
	data[2] = message;
	data[3] = message_len;
	data[4] = *mac_len;

	data_size = (sizeof(uint32_t) * 5);

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_COMPUTE_MAC_BY_DMA,
				      data, data_size, rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		DLOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result != 0) {
		DLOG("Error: %d\n", result);
		return result;
	}

	*mac_len = rdata[1];
	if (*mac_len > 0) {
		memcpy(mac, (uint8_t *)&rdata[2], *mac_len);
	}
	return result;
}

int32_t tcc_thsm_cmd_compare_mac_by_dma(uint32_t device_id, uint32_t key_index,
					uint32_t message, uint32_t message_len,
					uint8_t *mac, uint32_t mac_len,
					uint32_t flag)
{
	uint32_t data[32] = { 0 };
	int32_t rdata = 0;
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	data[0] = key_index;
	data[1] = flag;
	data[2] = message;
	data[3] = message_len;
	data[4] = mac_len;
	memcpy((uint8_t *)&data[5], mac, mac_len);

	data_size = (sizeof(uint32_t) * 5) + mac_len;

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_COMPARE_MAC_BY_DMA,
				      data, data_size, &rdata, sizeof(rdata));
	if (rdata_size < 0) {
		DLOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;

	return result;
}

int32_t tcc_thsm_cmd_get_rand(uint32_t device_id, uint32_t *rng,
			      uint32_t rng_size)
{
	uint32_t data = 0;
	int32_t rdata[16] = { 0 };
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	data = rng_size;

	data_size = sizeof(uint32_t);

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_GET_RAND, &data,
				      data_size, &rdata, sizeof(rdata));
	if (rdata_size < 0) {
		DLOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result != 0) {
		DLOG("Error: %d\n", result);
		return result;
	}

	rng_size = rdata[1];
	if (rng_size > 0) {
		memcpy(rng, &rdata[2], rng_size);
	} else {
		ELOG("Wrong read data size = %d\n", rng_size);
		return -EBADR;
	}

	return result;
}

int32_t tcc_thsm_cmd_gen_key_ss(uint32_t device_id, int8_t *obj_id,
				uint32_t obj_len, uint32_t algorithm,
				uint32_t key_size)
{
	uint32_t data[64] = { 0 };
	int32_t rdata = 0;
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	data[0] = algorithm;
	data[1] = key_size;
	data[2] = obj_len;
	data_size = (sizeof(uint32_t) * 3) + obj_len;

	if (obj_id != NULL && obj_len > 0) {
		memcpy((void *)&data[3], (const void *)obj_id, obj_len);
	}

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_GEN_KEY_SS, data,
				      data_size, &rdata, sizeof(rdata));
	if (rdata_size < 0) {
		DLOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != 0) {
		DLOG("Error: %d\n", result);
		return result;
	}

	return result;
}

int32_t tcc_thsm_cmd_del_key_ss(uint32_t device_id, int8_t *obj_id,
				uint32_t obj_len)
{
	uint32_t data[64] = { 0 };
	int32_t rdata = 0;
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	data[0] = obj_len;
	if (obj_id != NULL && obj_len > 0) {
		memcpy((void *)&data[1], (const void *)obj_id, obj_len);
	}

	data_size = sizeof(uint32_t) + obj_len;

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_DEL_KEY_SS, data,
				      data_size, &rdata, sizeof(rdata));
	if (rdata_size < 0) {
		DLOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != 0) {
		DLOG("Error: %d\n", result);
		return result;
	}

	return result;
}

int32_t tcc_thsm_cmd_write_key_ss(uint32_t device_id, int8_t *obj_id,
				  uint32_t obj_len, uint8_t *buffer,
				  uint32_t buffer_size)
{
	uint32_t data[128] = { 0 };
	int32_t rdata = 0;
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0, offset = 0, obj_len_align = 0;

	obj_len_align = ((obj_len + 3) / sizeof(uint32_t)) * 4;
	data[0] = obj_len;
	data[1] = buffer_size;

	data_size = (sizeof(uint32_t) * 2) + obj_len_align + buffer_size;

	if (obj_id != NULL && obj_len > 0 && buffer_size > 0) {
		memcpy((void *)&data[2], (const void *)obj_id, obj_len);
		offset = ((obj_len + 3) / sizeof(uint32_t));
		memcpy((uint8_t *)&data[2 + offset], buffer, buffer_size);
	}

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_WRITE_KEY_SS, data,
				      data_size, &rdata, sizeof(rdata));
	if (rdata_size < 0) {
		DLOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != 0) {
		DLOG("Error: %d\n", result);
		return result;
	}

	return result;
}

int32_t tcc_thsm_cmd_write_key_ss_v2(uint32_t device_id, int8_t *obj_id,
				     uint32_t obj_len, struct TEETHSMKey *key)
{
	uint32_t data[128] = { 0 };
	int32_t rdata = 0;
	int32_t data_size = 0, rdata_size = 0;
	int32_t idx = 0;
	int32_t result = 0;

	data[idx] = obj_len;
	idx++;
	data[idx] = key->keyType;
	idx++;

	switch (key->keyType) {
	case TCCTHSM_KEY_TYPE_SECRET:
		data[idx] = key->key.SECRET.key1Size;
		idx++;
		data[idx] = key->key.SECRET.key2Size;
		idx++;
		data[idx] = 0; // Key3 does not exist in KEY_TYPE_SECRET
		idx++;
		memcpy((void *)&data[idx], (const void *)key->key.SECRET.key1,
		       key->key.SECRET.key1Size);
		idx += ((key->key.SECRET.key1Size + 3u) / sizeof(uint32_t));
		if (key->key.SECRET.key2Size != 0u) {
			memcpy((void *)&data[idx],
			       (const void *)key->key.SECRET.key2,
			       key->key.SECRET.key2Size);
			idx += ((key->key.SECRET.key2Size + 3u) /
				sizeof(uint32_t));
		}
		break;

	case TCCTHSM_KEY_TYPE_RSA_PAIR:
		data[idx] = key->key.RSA.modulusSize;
		idx++;
		data[idx] = key->key.RSA.publicExpSize;
		idx++;
		data[idx] = key->key.RSA.privateExpSize;
		idx++;
		memcpy((void *)&data[idx], (const void *)key->key.RSA.modulus,
		       key->key.RSA.modulusSize);
		idx += ((key->key.RSA.modulusSize + 3u) / sizeof(uint32_t));
		memcpy((void *)&data[idx], (const void *)key->key.RSA.publicExp,
		       key->key.RSA.publicExpSize);
		idx += ((key->key.RSA.publicExpSize + 3u) / sizeof(uint32_t));
		memcpy((void *)&data[idx],
		       (const void *)key->key.RSA.privateExp,
		       key->key.RSA.privateExpSize);
		idx += ((key->key.RSA.privateExpSize + 3u) / sizeof(uint32_t));
		break;

	case TCCTHSM_KEY_TYPE_RSA_PUBLIC:
		data[idx] = key->key.RSA.modulusSize;
		idx++;
		data[idx] = key->key.RSA.publicExpSize;
		idx++;
		data[idx] =
			0; // Key3 does not exist in TEETHSM_KEY_TYPE_RSA_PUBLIC
		idx++;
		memcpy((void *)&data[idx], (const void *)key->key.RSA.modulus,
		       key->key.RSA.modulusSize);
		idx += ((key->key.RSA.modulusSize + 3u) / sizeof(uint32_t));
		memcpy((void *)&data[idx], (const void *)key->key.RSA.publicExp,
		       key->key.RSA.publicExpSize);
		idx += ((key->key.RSA.publicExpSize + 3u) / sizeof(uint32_t));
		break;

	case TCCTHSM_KEY_TYPE_ECDSA_PAIR:
		data[idx] = key->key.ECDSA.publicXSize;
		idx++;
		data[idx] = key->key.ECDSA.publicYSize;
		idx++;
		data[idx] = key->key.ECDSA.privateSize;
		idx++;
		memcpy((void *)&data[idx], (const void *)key->key.ECDSA.publicX,
		       key->key.ECDSA.publicXSize);
		idx += ((key->key.ECDSA.publicXSize + 3u) / sizeof(uint32_t));
		memcpy((void *)&data[idx], (const void *)key->key.ECDSA.publicY,
		       key->key.ECDSA.publicYSize);
		idx += ((key->key.ECDSA.publicYSize + 3u) / sizeof(uint32_t));
		memcpy((void *)&data[idx], (const void *)key->key.ECDSA.private,
		       key->key.ECDSA.privateSize);
		idx += ((key->key.ECDSA.privateSize + 3u) / sizeof(uint32_t));
		break;

	case TCCTHSM_KEY_TYPE_ECDSA_PUBLIC:
		data[idx] = key->key.ECDSA.publicXSize;
		idx++;
		data[idx] = key->key.ECDSA.publicYSize;
		idx++;
		data[idx] = 0; // Key3 does not exist in
			// TEETHSM_KEY_TYPE_ECDSA_PUBLIC
		idx++;
		memcpy((void *)&data[idx], (const void *)key->key.ECDSA.publicX,
		       key->key.ECDSA.publicXSize);
		idx += ((key->key.ECDSA.publicXSize + 3u) / sizeof(uint32_t));
		memcpy((void *)&data[idx], (const void *)key->key.ECDSA.publicY,
		       key->key.ECDSA.publicYSize);
		idx += ((key->key.ECDSA.publicYSize + 3u) / sizeof(uint32_t));
		break;

	default:
		ELOG("key type Err(%d)\n", key->keyType);
		return -EBADR;
	}

	memcpy((void *)&data[idx], (const void *)obj_id, obj_len);
	idx += ((obj_len + 3u) / sizeof(uint32_t));

	data_size = (idx * sizeof(uint32_t));

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_WRITE_KEY_SS_V2,
				      data, data_size, &rdata, sizeof(rdata));

	if (rdata_size < 0) {
		DLOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != 0) {
		DLOG("Error: %d\n", result);
		return result;
	}

	return result;
}

int32_t tcc_thsm_cmd_kdf(uint32_t device_id, uint32_t algorithm, uint8_t *key,
			 uint32_t key_size, uint8_t *salt, uint32_t salt_size,
			 uint8_t *info, uint32_t info_size, uint8_t *sessionKey,
			 uint32_t session_size, uint32_t type)
{
	uint32_t data[128] = { 0 };
	uint32_t rdata[128] = { 0 };
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;
	uint32_t idx = 0;

	if ((key == NULL) || (key_size > TCCTHSM_MAX_KEYSIZE)) {
		ELOG("Error Key parameter\n");
		return -EBADR;
	}
	if (salt_size >
	    (TCC_MBOX_MAX_MSG - ((sizeof(uint32_t) * 4u) + key_size))) {
		ELOG("Error Key parameter\n");
		return -EBADR;
	}
	if (session_size > (TCC_MBOX_MAX_MSG -
			    ((sizeof(uint32_t) * 4u) + key_size + salt_size))) {
		ELOG("Error Key parameter\n");
		return -EBADR;
	}

	data_size =
		(key_size + salt_size + info_size + (sizeof(uint32_t) * 6u));

	/* Set data FIFO */
	data[idx] = algorithm;
	idx++;
	data[idx] = key_size;
	idx++;
	data[idx] = salt_size;
	idx++;
	data[idx] = info_size;
	idx++;
	data[idx] = session_size;
	idx++;
	data[idx] = type;
	idx++;

	if (copy_from_user((void *)&data[idx], (const void *)key, key_size) !=
	    0) {
		ELOG("Error copy_from_user\n");
		return -EBADR;
	}
	idx += ((key_size + 3U) / sizeof(uint32_t));

	if (copy_from_user((void *)&data[idx], (const void *)salt, salt_size) !=
	    0) {
		ELOG("Error copy_from_user\n");
		return -EBADR;
	}
	idx += ((salt_size + 3U) / sizeof(uint32_t));

	if ((info_size != 0u) && (info != NULL)) {
		if (copy_from_user((void *)&data[idx], (const void *)info,
				   info_size) != 0) {
			ELOG("Error copy_from_user\n");
			return -EBADR;
		}
		idx += ((info_size + 3U) / sizeof(uint32_t));
	}

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_KDF, data,
				      data_size, &rdata, DMA_MAX_RSIZE);

	if (rdata_size < 0) {
		DLOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	idx = 0;
	result = rdata[idx];
	idx++;
	if (result == 0) {
		session_size = rdata[idx];
		idx++;
		if ((session_size > 0U)) {
			if (copy_to_user((void *)sessionKey,
					 (const void *)&rdata[idx],
					 session_size) != 0) {
				ELOG("Error copy_to_user\n");
				return -EBADR;
			}
		} else {
			ELOG("Wrong dstSize(%d)\n", session_size);
			return -EBADR;
		}
	}

	return result;
}

int32_t tcc_thsm_cmd_ecdh_compute_pubkey(uint32_t device_id, uint32_t algorithm,
					 uint8_t *pubKey, uint32_t pub_size,
					 uint8_t *priKey, uint32_t pri_size)
{
	uint32_t data[128] = { 0 };
	uint32_t rdata[128] = { 0 };
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;
	uint32_t idx = 0;

	if ((pubKey == NULL) || (priKey == NULL) ||
	    (pub_size > TCCTHSM_MAX_KEYSIZE) ||
	    (pri_size > TCCTHSM_MAX_KEYSIZE)) {
		ELOG("Error Key parameter\n");
		return -EBADR;
	}
	data_size = (pub_size + pri_size + (sizeof(uint32_t) * 3u));
	/* Set data FIFO */
	data[idx] = algorithm;
	idx++;
	data[idx] = pub_size;
	idx++;
	data[idx] = pri_size;
	idx++;
	if (copy_from_user((void *)&data[idx], (const void *)priKey,
			   pri_size) != 0) {
		ELOG("Error copy_from_user\n");
		return -EBADR;
	}

	rdata_size =
		sec_sendrecv_cmd(device_id, TCCTHSM_EVT_ECDH_COMPUTE_PUBKEY,
				 data, data_size, &rdata, DMA_MAX_RSIZE);

	if (rdata_size < 0) {
		DLOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	idx = 0;
	result = rdata[idx];
	idx++;
	if (result == 0) {
		pub_size = rdata[idx];
		idx++;
		if ((pub_size > 0U)) {
			if (copy_to_user((void *)pubKey,
					 (const void *)&rdata[idx],
					 pub_size) != 0) {
				ELOG("Error copy_to_user\n");
				return -EBADR;
			}
		} else {
			ELOG("Wrong dstSize(%d)\n", pub_size);
			return -EBADR;
		}
	}
	return result;
}

int32_t tcc_thsm_cmd_ecdh_phaseI(uint32_t device_id, uint32_t algorithm,
				 uint8_t *pubKey, uint32_t pub_size,
				 uint8_t *priKey, uint32_t pri_size)
{
	uint32_t data[128] = { 0 };
	uint32_t rdata[128] = { 0 };
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;
	uint32_t idx = 0;

	if ((pubKey == NULL) || (priKey == NULL) ||
	    (pub_size > TCCTHSM_MAX_KEYSIZE) ||
	    (pri_size > TCCTHSM_MAX_KEYSIZE)) {
		ELOG("Error Key parameter\n");
		return -EBADR;
	}

	/* Set data FIFO */
	data[idx] = algorithm;
	idx++;
	data[idx] = pub_size;
	idx++;
	data[idx] = pri_size;
	idx++;

	data_size = (sizeof(uint32_t) * idx);

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_HSM_ECDH_PHASE_I,
				      data, data_size, &rdata, DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		DLOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	idx = 0;
	result = rdata[idx];
	idx++;
	if (result == 0) {
		pub_size = rdata[idx];
		idx++;
		pri_size = rdata[idx];
		idx++;
		if ((pub_size > 0U) && (pri_size > 0U)) {
			if (copy_to_user((void *)pubKey,
					 (const void *)&rdata[idx],
					 pub_size) != 0) {
				ELOG("Error copy_to_user\n");
				return -EBADR;
			}
			idx += ((pub_size + 3u) / sizeof(uint32_t));
			if (copy_to_user((void *)priKey,
					 (const void *)&rdata[idx],
					 pri_size) != 0) {
				ELOG("Error copy_to_user\n");
				return -EBADR;
			}
		} else {
			ELOG("Wrong dstSize(%d)\n", pub_size);
			return -EBADR;
		}
	}
	return result;
}

int32_t tcc_thsm_cmd_ecdh_phaseII(uint32_t device_id, uint32_t algorithm,
				  uint8_t *pubKey, uint32_t pub_size,
				  uint8_t *priKey, uint32_t pri_size,
				  uint8_t *secKey, uint32_t sec_size)
{
	uint32_t data[128] = { 0 };
	uint32_t rdata[128] = { 0 };
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;
	uint32_t idx = 0;

	if ((pubKey == NULL) || (pub_size > TCCTHSM_MAX_KEYSIZE)) {
		ELOG("Error pubKey parameter\n");
		return -EBADR;
	}
	if (pri_size >
	    (TCC_MBOX_MAX_MSG - ((sizeof(uint32_t) * 4u) + pub_size))) {
		ELOG("Error priKey parameter\n");
		return -EBADR;
	}
	if (sec_size > (TCC_MBOX_MAX_MSG -
			((sizeof(uint32_t) * 4u) + pri_size + pub_size))) {
		ELOG("Error secKey parameter\n");
		return -EBADR;
	}

	data_size = (pub_size + pri_size + (sizeof(uint32_t) * 4u));
	/* Set data FIFO */
	data[idx] = algorithm;
	idx++;
	data[idx] = pub_size;
	idx++;
	data[idx] = pri_size;
	idx++;
	data[idx] = sec_size;
	idx++;

	if (copy_from_user((void *)&data[idx], (const void *)pubKey,
			   pub_size) != 0) {
		ELOG("Error copy_from_user\n");
		return -EBADR;
	}
	idx += ((pub_size + 3U) / sizeof(uint32_t));
	if (copy_from_user((void *)&data[idx], (const void *)priKey,
			   pri_size) != 0) {
		ELOG("Error copy_from_user\n");
		return -EBADR;
	}

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_HSM_ECDH_PHASE_II,
				      data, data_size, &rdata, DMA_MAX_RSIZE);

	if (rdata_size < 0) {
		DLOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	idx = 0;
	result = rdata[idx];
	idx++;
	if (result == 0) {
		sec_size = rdata[idx];
		idx++;
		if ((sec_size > 0U)) {
			if (copy_to_user((void *)secKey,
					 (const void *)&rdata[idx],
					 sec_size) != 0) {
				ELOG("Error copy_to_user\n");
				return -EBADR;
			};
		} else {
			ELOG("Wrong dstSize(%d)\n", sec_size);
			return -EBADR;
		}
	}
	return result;
}

int32_t tcc_thsm_cmd_write_otp(uint32_t device_id, uint32_t otp_addr,
			       uint8_t *otp_buf, uint32_t otp_size)
{
	uint32_t data[32] = { 0 };
	int32_t rdata = 0;
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	data[0] = otp_addr;
	data[1] = otp_size;
	data_size = (sizeof(uint32_t) * 2) + otp_size;

	if (otp_buf != NULL && otp_size > 0) {
		memcpy((uint8_t *)&data[2], otp_buf, otp_size);
	}

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_WRITE_OTP, data,
				      data_size, &rdata, sizeof(rdata));
	if (rdata_size < 0) {
		DLOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != 0) {
		DLOG("Error: %d\n", result);
		return result;
	}

	return result;
}

int32_t tcc_thsm_cmd_write_otpimage(uint32_t device_id, uint32_t otp_addr,
				    uint32_t otp_size)
{
	uint32_t data[2] = { 0 };
	int32_t rdata = 0;
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	data[0] = otp_addr;
	data[1] = otp_size;
	data_size = (sizeof(uint32_t) * 2);

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_WRITE_OTP_IMAGE,
				      data, data_size, &rdata, sizeof(rdata));
	if (rdata_size < 0) {
		DLOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != 0) {
		DLOG("Error: %d\n", result);
		return result;
	}

	return result;
}

int32_t tcc_thsm_cmd_asym_enc_dec_by_dma(uint32_t device_id, uint32_t key_index,
					 uint32_t src_addr, uint32_t src_size,
					 uint32_t dst_addr, uint32_t *dst_size,
					 uint32_t enc)
{
	uint32_t data[8] = { 0 };
	int32_t rdata[2] = { 0 };
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0, cmd = 0;

	if (enc == ENCRYPTION) {
		cmd = TCCTHSM_EVT_ASYMMETRIC_ENC_BY_DMA;
	} else {
		cmd = TCCTHSM_EVT_ASYMMETRIC_DEC_BY_DMA;
	}
	data[0] = key_index;
	data[1] = src_addr;
	data[2] = src_size;
	data[3] = dst_addr;
	data[4] = *dst_size;
	data_size = (sizeof(uint32_t) * 5);

	rdata_size = sec_sendrecv_cmd(device_id, cmd, data, data_size, rdata,
				      DMA_MAX_RSIZE);
	if (rdata_size < 0) {
		DLOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result != 0) {
		DLOG("Error: %d\n", result);
		return result;
	}

	*dst_size = rdata[1];
	return result;
}

int32_t tcc_thsm_cmd_asym_sign_digest(uint32_t device_id, uint32_t key_index,
				      uint8_t *dig, uint32_t dig_size,
				      uint8_t *sig, uint32_t *sig_size)
{
	uint32_t data[128] = { 0 };
	int32_t rdata[128] = { 0 };
	int32_t data_size = 0, rdata_size = 0;
	int32_t result = 0;

	data[0] = key_index;
	data[1] = dig_size;
	data[2] = *sig_size;
	if (dig != NULL && dig_size > 0) {
		memcpy((uint8_t *)&data[3], dig, dig_size);
	}

	data_size = (sizeof(uint32_t) * 3) + dig_size;

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_ASYMMETRIC_SIGN,
				      data, data_size, rdata, sizeof(rdata));
	if (rdata_size < 0) {
		DLOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata[0];
	if (result != 0) {
		DLOG("Error: %d\n", result);
		return result;
	}
	*sig_size = rdata[1];
	if (*sig_size > 0) {
		memcpy(sig, (uint8_t *)&rdata[2], *sig_size);
	} else {
		DLOG("wrong sig_size(%d)\n", *sig_size);
		return -EBADR;
	}

	return result;
}

int32_t tcc_thsm_cmd_asym_verify_digest(uint32_t device_id, uint32_t key_index,
					uint8_t *dig, uint32_t dig_size,
					uint8_t *sig, uint32_t sig_size)
{
	uint32_t data[128] = { 0 };
	int32_t rdata = 0, offset = 0;
	int32_t data_size = 0, rdata_size = 0, dig_size_align = 0;
	int32_t result = 0;

	dig_size_align = ((dig_size + 3) / sizeof(uint32_t)) * 4;
	data[0] = key_index;
	data[1] = dig_size;
	data[2] = sig_size;
	memcpy((uint8_t *)&data[3], dig, dig_size);
	offset = ((dig_size + 3) / sizeof(uint32_t));
	memcpy((uint8_t *)&data[3 + offset], sig, sig_size);

	data_size = (sizeof(uint32_t) * 3) + dig_size_align + sig_size;

	rdata_size = sec_sendrecv_cmd(device_id, TCCTHSM_EVT_ASYMMETRIC_VERIFY,
				      data, data_size, &rdata, sizeof(rdata));
	if (rdata_size < 0) {
		DLOG("sec_sendrecv_cmd error(%d)\n", rdata_size);
		return -EBADR;
	}

	result = rdata;
	if (result != 0) {
		DLOG("Error: %d\n", result);
		return result;
	}

	return result;
}
