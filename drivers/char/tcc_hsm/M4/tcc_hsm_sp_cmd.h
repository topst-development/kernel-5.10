// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/tcc_hsm.h>

#ifndef _TCC_HSM_SP_CMD_H_
#define _TCC_HSM_SP_CMD_H_

// clang-format off
/* Error Code 0x000000XX */
#define TCCHSM_SUCCESS 					(0x00000000u)
#define TCCHSM_ERR 						(0x00000001u)
#define TCCHSM_ERR_INVALID_PARAM 		(0x00000002u)
#define TCCHSM_ERR_INVALID_STATE 		(0x00000003u)
#define TCCHSM_ERR_INVALID_MEMORY		(0x00000004u)
#define TCCHSM_ERR_UNSUPPORTED_FUNC 	(0x00000005u)
#define TCCHSM_ERR_OTP 					(0x00000006u)
#define TCCHSM_ERR_CRYPTO 				(0x00000007u)
#define TCCHSM_ERR_OCCUPIED_RESOURCE 	(0x00000008u)
#define TCCHSM_ERR_IMG_INTEGRITY 		(0x00000009u)
#define TCCHSM_ERR_RBID_MISMATCH 		(0x0000000Au)
#define TCCHSM_ERR_IMGID_MISMATCH 		(0x0000000Bu)
#define TCCHSM_ERR_ATTR_MISMATCH 		(0x0000000Cu)
#define TCCHSM_ERR_VERSION_MISMATCH		(0x0000000Du)

/* tccHSM command range: 0x4000 ~ 0x4FFF */
#define MAGIC_NUM                       (4)	// for HSM
#define TCCHSM_CMD_START                SP_CMD(MAGIC_NUM, 0x001)
#define TCCHSM_CMD_STOP                 SP_CMD(MAGIC_NUM, 0x002)
#define TCCHSM_CMD_GET_VERSION          SP_CMD(MAGIC_NUM, 0x003)
#define TCCHSM_CMD_SET_MODE             SP_CMD(MAGIC_NUM, 0x011)
#define TCCHSM_CMD_SET_KEY              SP_CMD(MAGIC_NUM, 0x012)
#define TCCHSM_CMD_SET_KEY_FROM_OTP     SP_CMD(MAGIC_NUM, 0x013)
#define TCCHSM_CMD_SET_IV               SP_CMD(MAGIC_NUM, 0x014)
#define TCCHSM_CMD_SET_KLDATA           SP_CMD(MAGIC_NUM, 0x017)
#define TCCHSM_CMD_RUN_CIPHER           SP_CMD(MAGIC_NUM, 0x018)
#define TCCHSM_CMD_RUN_CIPHER_BY_DMA    SP_CMD(MAGIC_NUM, 0x019)
#define TCCHSM_CMD_RUN_CMAC             SP_CMD(MAGIC_NUM, 0x021)
#define TCCHSM_CMD_RUN_SHA256           SP_CMD(MAGIC_NUM, 0x022)
#define TCCHSM_CMD_WRITE_OTP            SP_CMD(MAGIC_NUM, 0x032)
#define TCCHSM_CMD_GET_RNG              SP_CMD(MAGIC_NUM, 0x041)
#define TCCHSM_CMD_WRITE_OTP_IMAGE      SP_CMD(MAGIC_NUM, 0x051)
// clang-format on

#define TCCHSM_CIPHER_KEYSIZE_FOR_64 8
#define TCCHSM_CIPHER_KEYSIZE_FOR_128 16
#define TCCHSM_CIPHER_KEYSIZE_FOR_192 24
#define TCCHSM_CIPHER_KEYSIZE_FOR_256 32

#define TCCHSM_CIPHER_CMAC_SIZE 16u
#define TCCHSM_CIPHER_SHA256_SIZE 32u

uint32_t tcc_hsm_sp_cmd_start(void);
uint32_t tcc_hsm_sp_cmd_stop(void);
uint32_t tcc_hsm_sp_cmd_get_version(uint32_t device_id,
				    struct tcc_hsm_ioctl_version_param *param);
uint32_t
tcc_hsm_sp_cmd_set_mode(uint32_t device_id,
			const struct tcc_hsm_ioctl_set_mode_param *param);
uint32_t tcc_hsm_sp_cmd_set_key(uint32_t device_id,
				const struct tcc_hsm_ioctl_set_key_param *param,
				uint8_t *key);
uint32_t tcc_hsm_sp_cmd_set_key_from_otp(
	uint32_t device_id,
	const struct tcc_hsm_ioctl_set_key_from_otp_param *param);
uint32_t tcc_hsm_sp_cmd_set_iv(uint32_t device_id,
			       const struct tcc_hsm_ioctl_set_iv_param *param,
			       uint8_t *iv);
uint32_t tcc_hsm_sp_cmd_set_kldata(uint32_t device_id, uint32_t keyIndex,
				   const struct tcc_hsm_kldata *klData,
				   uint32_t klDataSize);
uint32_t tcc_hsm_sp_cmd_run_cipher_by_dma(uint32_t device_id, uint32_t keyIndex,
					  uint32_t srcAddr, uint32_t dstAddr,
					  uint32_t srcSize, uint32_t enc,
					  uint32_t swSel, uint32_t klIndex,
					  uint32_t keyMode);
uint32_t tcc_hsm_sp_cmd_run_cmac(uint32_t device_id, uint32_t keyIndex,
				 uint32_t flag, uint8_t *srcAddr,
				 uint32_t srcSize, uint8_t *macAddr,
				 uint32_t *macSize);
uint32_t tcc_hsm_sp_cmd_run_sha256(uint32_t device_id, uint32_t flag,
				   uint8_t *src, uint32_t srcSize, uint8_t *dig,
				   uint32_t *digSize);
uint32_t tcc_hsm_sp_cmd_write_otp(uint32_t device_id, uint32_t otpAddr,
				  uint8_t *otpBuf, uint32_t otpSize);
uint32_t tcc_hsm_sp_cmd_write_otp_image(uint32_t device_id, uint32_t imageAddr,
					uint32_t imageSize);
uint32_t tcc_hsm_sp_cmd_get_rand(uint32_t device_id, uint8_t *rng,
				 uint32_t rngSize);

#endif /*_TCC_HSM_SP_CMD_H_*/
