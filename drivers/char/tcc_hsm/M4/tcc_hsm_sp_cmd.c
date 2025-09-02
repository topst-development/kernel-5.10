// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
//#define NDEBUG
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
#include <linux/tcc_hsm.h>
#include <linux/uaccess.h>
#include <asm/div64.h>

#include "tcc_hsm_sp_cmd.h"

uint32_t tcc_hsm_sp_cmd_get_version(uint32_t device_id,
				    struct tcc_hsm_ioctl_version_param *param)
{
	uint32_t mbox_data[2] = {
		0,
	};
	uint32_t mbox_result[3] = {
		0,
	};
	int32_t mbox_result_size = 0;
	uint32_t result = 0;

	mbox_result_size = sec_sendrecv_cmd(device_id, TCCHSM_CMD_GET_VERSION,
					    mbox_data, sizeof(mbox_data),
					    mbox_result, sizeof(mbox_result));
	if (mbox_result_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", mbox_result_size);
		result = TCCHSM_ERR;
		goto out;
	}

	result = mbox_result[0];
	if (result != 0) {
		ELOG("SP returned an error: %d\n", result);
		goto out;
	}

	param->major = mbox_result[1];
	param->minor = mbox_result[2];
out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_sp_cmd_get_version);

uint32_t
tcc_hsm_sp_cmd_set_mode(uint32_t device_id,
			const struct tcc_hsm_ioctl_set_mode_param *param)
{
	uint32_t mbox_data[5] = {
		0,
	};
	uint32_t mbox_result = 0;
	int32_t mbox_result_size = 0;
	uint32_t result = 0;

	mbox_data[0] = param->keyIndex;
	mbox_data[1] = param->algorithm;
	mbox_data[2] = param->opMode;
	mbox_data[3] = param->residual;
	mbox_data[4] = param->sMsg;

	mbox_result_size = sec_sendrecv_cmd(device_id, TCCHSM_CMD_SET_MODE,
					    mbox_data, sizeof(mbox_data),
					    &mbox_result, sizeof(mbox_result));
	if (mbox_result_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", mbox_result_size);
		result = TCCHSM_ERR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		ELOG("SP returned an error: %d\n", result);
		goto out;
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_sp_cmd_set_mode);

uint32_t tcc_hsm_sp_cmd_set_key(uint32_t device_id,
				const struct tcc_hsm_ioctl_set_key_param *param,
				uint8_t *user_key)
{
	uint32_t mbox_data[12] = {
		0,
	};
	uint32_t mbox_result = 0;
	int32_t mbox_result_size = 0;
	uint32_t result = 0;

	mbox_data[0] = param->keyIndex;
	mbox_data[1] = param->keyType;
	mbox_data[2] = param->keyMode;
	mbox_data[3] = param->keySize;

	if (user_key != NULL) {
		memcpy(&mbox_data[4], user_key, param->keySize);
	}

	mbox_result_size = sec_sendrecv_cmd(device_id, TCCHSM_CMD_SET_KEY,
					    mbox_data, sizeof(mbox_data),
					    &mbox_result, sizeof(mbox_result));
	if (mbox_result_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", mbox_result_size);
		result = TCCHSM_ERR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		ELOG("SP returned an error: %d\n", result);
		goto out;
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_sp_cmd_set_key);

uint32_t tcc_hsm_sp_cmd_set_key_from_otp(
	uint32_t device_id,
	const struct tcc_hsm_ioctl_set_key_from_otp_param *param)
{
	uint32_t mbox_data[5] = {
		0,
	};
	uint32_t mbox_result = 0;
	int32_t mbox_result_size = 0;
	uint32_t result = 0;

	mbox_data[0] = param->keyIndex;
	mbox_data[1] = param->keyType;
	mbox_data[2] = param->keyMode;
	mbox_data[3] = param->keySize;
	mbox_data[4] = param->otpAddr;

	mbox_result_size =
		sec_sendrecv_cmd(device_id, TCCHSM_CMD_SET_KEY_FROM_OTP,
				 mbox_data, sizeof(mbox_data), &mbox_result,
				 sizeof(mbox_result));
	if (mbox_result_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", mbox_result_size);
		result = TCCHSM_ERR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		ELOG("SP returned an error: %d\n", result);
		goto out;
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_sp_cmd_set_key_from_otp);

uint32_t tcc_hsm_sp_cmd_set_iv(uint32_t device_id,
			       const struct tcc_hsm_ioctl_set_iv_param *param,
			       uint8_t *iv)
{
	uint32_t mbox_data[11] = {
		0,
	};
	uint32_t mbox_result = 0;
	int32_t mbox_result_size = 0;
	uint32_t result = 0;

	mbox_data[0] = param->keyIndex;
	mbox_data[1] = 1;
	mbox_data[2] = param->ivSize;

	if (iv != NULL) {
		memcpy(&mbox_data[3], iv, param->ivSize);
	}

	mbox_result_size = sec_sendrecv_cmd(device_id, TCCHSM_CMD_SET_IV,
					    mbox_data, sizeof(mbox_data),
					    &mbox_result, sizeof(mbox_result));
	if (mbox_result_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", mbox_result_size);
		result = TCCHSM_ERR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		ELOG("SP returned an error: %d\n", result);
		goto out;
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_sp_cmd_set_iv);

uint32_t tcc_hsm_sp_cmd_set_kldata(uint32_t device_id, uint32_t keyIndex,
				   const struct tcc_hsm_kldata *klData,
				   uint32_t klDataSize)
{
	uint32_t mbox_data[50] = {
		0,
	};
	uint32_t mbox_result = 0;
	int32_t mbox_result_size = 0;
	uint32_t result = 0;

	mbox_data[0] = keyIndex;
	mbox_data[1] = klDataSize;

	if (klData != NULL) {
		memcpy(&mbox_data[2], klData, klDataSize);
	}

	mbox_result_size = sec_sendrecv_cmd(device_id, TCCHSM_CMD_SET_KLDATA,
					    mbox_data, sizeof(mbox_data),
					    &mbox_result, sizeof(mbox_result));
	if (mbox_result_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", mbox_result_size);
		result = TCCHSM_ERR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		ELOG("SP returned an error: %d\n", result);
		goto out;
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_sp_cmd_set_kldata);

uint32_t tcc_hsm_sp_cmd_run_cipher_by_dma(uint32_t device_id, uint32_t keyIndex,
					  uint32_t srcAddr, uint32_t dstAddr,
					  uint32_t srcSize, uint32_t enc,
					  uint32_t cwSel, uint32_t klIndex,
					  uint32_t keyMode)
{
	uint32_t mbox_data[12] = {
		0,
	};
	uint32_t mbox_result = 0;
	int32_t mbox_result_size = 0;
	uint32_t result = 0;

	mbox_data[0] = keyIndex;
	mbox_data[1] = srcAddr;
	mbox_data[2] = dstAddr;
	mbox_data[3] = srcSize;
	mbox_data[4] = 0;
	mbox_data[5] = 0;
	mbox_data[6] = enc;
	mbox_data[7] = cwSel;
	mbox_data[8] = klIndex;
	mbox_data[9] = keyMode;
	mbox_data[10] = 0;
	mbox_data[11] = 0;

	mbox_result_size =
		sec_sendrecv_cmd(device_id, TCCHSM_CMD_RUN_CIPHER_BY_DMA,
				 mbox_data, sizeof(mbox_data), &mbox_result,
				 sizeof(mbox_result));
	if (mbox_result_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", mbox_result_size);
		result = TCCHSM_ERR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		ELOG("SP returned an error: %d\n", result);
		goto out;
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_sp_cmd_run_cipher_by_dma);

uint32_t tcc_hsm_sp_cmd_run_cmac(uint32_t device_id, uint32_t keyIndex,
				 uint32_t flag, uint8_t *srcAddr,
				 uint32_t srcSize, uint8_t *macAddr,
				 uint32_t *macSize)
{
	uint32_t mbox_data[96] = { 0 };
	uint32_t mbox_result[32] = { 0 };
	uint32_t data_size = 0;
	uint32_t idx = 0;
	int32_t mbox_result_size = 0;
	uint32_t result = 0;

	mbox_data[idx] = keyIndex;
	idx++;
	mbox_data[idx] = flag;
	idx++;
	mbox_data[idx] = srcSize;
	idx++;

	memcpy(&mbox_data[3], srcAddr, srcSize);
	idx += ((srcSize + 3U) / (uint32_t)sizeof(uint32_t));
	data_size = (uint32_t)(sizeof(uint32_t) * idx);

	mbox_result_size =
		sec_sendrecv_cmd(device_id, TCCHSM_CMD_RUN_CMAC, mbox_data,
				 data_size, mbox_result, sizeof(mbox_result));
	if (mbox_result_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", mbox_result_size);
		result = TCCHSM_ERR;
		goto out;
	}

	result = mbox_result[0];
	if (result != 0) {
		ELOG("SP returned an error: %d\n", result);
		goto out;
	}

	*macSize = mbox_result[1];
	if (*macSize > TCCHSM_CIPHER_CMAC_SIZE) {
		ELOG("The macSize(0x%x) should not exceed 0x%x bytes\n",
		     *macSize, TCCHSM_CIPHER_CMAC_SIZE);
		goto out;
	}
	memcpy((void *)macAddr, (const void *)&mbox_result[2], *macSize);

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_sp_cmd_run_cmac);

uint32_t tcc_hsm_sp_cmd_run_sha256(uint32_t device_id, uint32_t flag,
				   uint8_t *src, uint32_t srcSize, uint8_t *dig,
				   uint32_t *digSize)
{
	uint32_t mbox_data[96] = { 0 };
	uint32_t mbox_result[32] = { 0 };
	int32_t mbox_result_size = 0;
	uint32_t data_size = 0;
	uint32_t idx = 0;
	uint32_t result = 0;

	mbox_data[idx] = flag;
	idx++;
	mbox_data[idx] = srcSize;
	idx++;

	memcpy(&mbox_data[idx], src, srcSize);
	idx += ((srcSize + 3U) / (uint32_t)sizeof(uint32_t));
	data_size = (uint32_t)(sizeof(uint32_t) * idx);

	mbox_result_size =
		sec_sendrecv_cmd(device_id, TCCHSM_CMD_RUN_SHA256, mbox_data,
				 data_size, mbox_result, sizeof(mbox_result));
	if (mbox_result_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", mbox_result_size);
		result = TCCHSM_ERR;
		goto out;
	}

	result = mbox_result[0];
	if (result != 0) {
		ELOG("SP returned an error: %d\n", result);
		goto out;
	}

	*digSize = mbox_result[1];
	if (*digSize > TCCHSM_CIPHER_SHA256_SIZE) {
		ELOG("The digSize(0x%x) should not exceed 0x%x bytes\n",
		     *digSize, TCCHSM_CIPHER_SHA256_SIZE);
		goto out;
	}
	memcpy((void *)dig, (const void *)&mbox_result[2], *digSize);

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_sp_cmd_run_sha256);

uint32_t tcc_hsm_sp_cmd_write_otp(uint32_t device_id, uint32_t otpAddr,
				  uint8_t *otpBuf, uint32_t otpSize)
{
	uint32_t mbox_data[32] = {
		0,
	};
	uint32_t mbox_result = 0;
	int32_t mbox_result_size = 0;
	uint32_t result = 0;

	mbox_data[0] = otpAddr;
	mbox_data[1] = otpSize;

	memcpy(&mbox_data[2], otpBuf, otpSize);

	mbox_result_size = sec_sendrecv_cmd(device_id, TCCHSM_CMD_WRITE_OTP,
					    mbox_data, sizeof(mbox_data),
					    &mbox_result, sizeof(mbox_result));
	if (mbox_result_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", mbox_result_size);
		result = TCCHSM_ERR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		ELOG("SP returned an error: %d\n", result);
		goto out;
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_sp_cmd_write_otp);

uint32_t tcc_hsm_sp_cmd_write_otp_image(uint32_t device_id, uint32_t imageAddr,
					uint32_t imageSize)
{
	uint32_t mbox_data[2] = {
		0,
	};
	uint32_t mbox_result = 0;
	int32_t mbox_result_size = 0;
	uint32_t result = 0;

	mbox_data[0] = imageAddr;
	mbox_data[1] = imageSize;

	mbox_result_size =
		sec_sendrecv_cmd(device_id, TCCHSM_CMD_WRITE_OTP_IMAGE,
				 mbox_data, sizeof(mbox_data), &mbox_result,
				 sizeof(mbox_result));
	if (mbox_result_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", mbox_result_size);
		result = TCCHSM_ERR;
		goto out;
	}

	result = mbox_result;
	if (result != 0) {
		ELOG("SP returned an error: %d\n", result);
		goto out;
	}

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_sp_cmd_write_otp_image);

uint32_t tcc_hsm_sp_cmd_get_rand(uint32_t device_id, uint8_t *rng,
				 uint32_t rngSize)
{
	uint32_t mbox_data = 0;
	uint32_t mbox_result[16] = {
		0,
	};
	int32_t mbox_result_size = 0;
	uint32_t result = 0;

	mbox_data = rngSize;

	mbox_result_size = sec_sendrecv_cmd(device_id, TCCHSM_CMD_GET_RNG,
					    &mbox_data, sizeof(mbox_data),
					    mbox_result, sizeof(mbox_result));
	if (mbox_result_size < 0) {
		ELOG("sec_sendrecv_cmd error(%d)\n", mbox_result_size);
		result = TCCHSM_ERR;
		goto out;
	}

	result = mbox_result[0];
	if (result != 0) {
		ELOG("SP returned an error: %d\n", result);
		goto out;
	}

	rngSize = mbox_result[1];
	memcpy(rng, &mbox_result[2], rngSize);

out:
	return result;
}
EXPORT_SYMBOL(tcc_hsm_sp_cmd_get_rand);

MODULE_AUTHOR("linux <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips TCC HSM driver");
MODULE_LICENSE("GPL");