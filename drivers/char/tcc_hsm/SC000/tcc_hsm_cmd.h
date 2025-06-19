// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */


#ifndef TCC_HSM_CMD_H
#define TCC_HSM_CMD_H

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

/* HSM Command */
#define REQ_HSM_RUN_AES                         (0x10010000U)
#define REQ_HSM_RUN_AES_BY_KT                   (0x10020000U)
#define REQ_HSM_RUN_SM4                         (0x10030000U)
#define REQ_HSM_RUN_SM4_BY_KT                   (0x10040000U)

#define REQ_HSM_GEN_CMAC						(0x10110000U)
#define REQ_HSM_GEN_CMAC_BY_KT					(0x10120000U)
#define REQ_HSM_VERIFY_CMAC						(0x10130000U)
#define REQ_HSM_VERIFY_CMAC_BY_KT				(0x10140000U)
#define REQ_HSM_GEN_HMAC						(0x10150000U)
#define REQ_HSM_GEN_HMAC_BY_KT					(0x10160000U)
#define REQ_HSM_GEN_SM3_HMAC					(0x10170000U)
#define REQ_HSM_GEN_SM3_HMAC_BY_KT				(0x10180000U)

#define REQ_HSM_GEN_SHA                         (0x10210000U)
#define REQ_HSM_GEN_SM3                         (0x10220000U)

#define REQ_HSM_RUN_ECDSA_SIGN                  (0x10310000U)
#define REQ_HSM_RUN_ECDSA_VERIFY                (0x10320000U)
#define REQ_HSM_RUN_ECDH_PUBKEY_COMPUTE		    (0x10330000u)
#define REQ_HSM_RUN_ECDH_PHASE_I			    (0x10340000u)
#define REQ_HSM_RUN_ECDH_PHASE_II			    (0x10350000u)
#define REQ_HSM_RUN_ECDSA_SIGN_BY_KT		    (0x10360000u)
#define REQ_HSM_RUN_ECDSA_VERIFY_BY_KT		    (0x10370000u)

#define REQ_HSM_RUN_RSAES_PKCS_ENC		        (0x10510000u)
#define REQ_HSM_RUN_RSAES_PKCS_DEC				(0x10520000u)
#define REQ_HSM_RUN_RSAES_OAEP_ENC				(0x10530000u)
#define REQ_HSM_RUN_RSAES_OAEP_DEC				(0x10540000u)
#define REQ_HSM_RUN_RSASSA_PKCS_SIGN            (0x10550000U)
#define REQ_HSM_RUN_RSASSA_PKCS_VERIFY          (0x10560000U)
#define REQ_HSM_RUN_RSASSA_PSS_SIGN             (0x10570000U)
#define REQ_HSM_RUN_RSASSA_PSS_VERIFY           (0x10580000U)
#define REQ_HSM_RUN_RSAES_PKCS_ENC_BY_KT        (0x10590000u)
#define REQ_HSM_RUN_RSAES_PKCS_DEC_BY_KT        (0x105A0000u)
#define REQ_HSM_RUN_RSAES_OAEP_ENC_BY_KT        (0x105B0000u)
#define REQ_HSM_RUN_RSAES_OAEP_DEC_BY_KT        (0x105C0000u)
#define REQ_HSM_RUN_RSASSA_PKCS_SIGN_BY_KT      (0x105D0000U)
#define REQ_HSM_RUN_RSASSA_PKCS_VERIFY_BY_KT    (0x105E0000U)
#define REQ_HSM_RUN_RSASSA_PSS_SIGN_BY_KT       (0x105F0000U)
#define REQ_HSM_RUN_RSASSA_PSS_VERIFY_BY_KT     (0x10600000U)

#define REQ_HSM_GET_RNG                         (0x10610000U)

#define REQ_HSM_WRITE_OTP                       (0x10710000U)
#define REQ_HSM_WRITE_SNOR                      (0x10720000U)

#define REQ_HSM_SET_KEY_FROM_OTP                (0x10810000U)
#define REQ_HSM_SET_KEY_FROM_SNOR               (0x10820000U)
#define REQ_HSM_SET_MODN_FROM_OTP               (0x10840000u)
#define REQ_HSM_SET_MODN_FROM_SNOR              (0x10850000u)


#define REQ_HSM_GET_VER                         (0x20010000U)

#define HSM_NONE_DMA                            (0U)
#define HSM_DMA                                 (1U)

#define TCC_HSM_AES_KEY_SIZE        (64U)
#define TCC_HSM_AES_IV_SIZE         (32U)
#define TCC_HSM_AES_TAG_SIZE        (32U)
#define TCC_HSM_AES_AAD_SIZE        (32U)

#define TCC_HSM_MAC_KEY_SIZE        (32U)
#define TCC_HSM_MAC_MSG_SIZE        (64U)

#define TCC_HSM_HASH_DIGEST_SIZE (64U)
#define TCC_HSM_ECDSA_MAX_KEY_SIZE (68U)
#define TCC_HSM_ECDSA_DIGEST_SIZE (64U)
#define TCC_HSM_ECDSA_SIGN_SIZE	(136U)

#define TCC_HSM_RSA_MODN_SIZE		(512U)
#define TCC_HSM_RSA_DIG_SIZE		(64U)
#define TCC_HSM_RSA_SIG_SIZE		(512U)

#define OID_OPMODE_INIT         (0x10000000u)  /* Operation Mode is "Init" */
#define OID_OPMODE_UPDATE       (0x20000000u)  /* Operation Mode is "Update". Used to calculate intermediate results. */
#define OID_OPMODE_FINAL        (0x40000000u)  /* Operation Mode is "Final". The calculations shall be finalized. */
#define OID_OPMODE_SINGLECALL   (0x00000000u)  /* Operation Mode is "Single Call". Mixture of "Init", "Update" and "Final". */

// clang-format on

struct tcc_hsm_ioctl_set_key_param {
	uint32_t addr;
	uint32_t data_size;
	uint32_t key_index;
};

struct tcc_hsm_ioctl_set_modn_param {
	uint32_t addr;
	uint32_t data_size;
	uint32_t key_index;
};

struct tcc_hsm_ioctl_aes_param {
	uint32_t obj_id;
	uint8_t aes_key[TCC_HSM_AES_KEY_SIZE];
	uint32_t key_size;
	uint8_t iv[TCC_HSM_AES_IV_SIZE];
	uint32_t iv_size;
	uint32_t counter_size;
	uint8_t tag[TCC_HSM_AES_TAG_SIZE];
	uint32_t tag_size;
	uint8_t aad[TCC_HSM_AES_AAD_SIZE];
	uint32_t aad_size;
	ulong src;
	uint32_t src_size;
	ulong dst;
	uint32_t dst_size;
	uint32_t dma;
};

struct tcc_hsm_ioctl_aes_by_kt_param {
	uint32_t obj_id;
	uint32_t key_index;
	uint8_t iv[TCC_HSM_AES_IV_SIZE];
	uint32_t iv_size;
	uint32_t counter_size;
	uint8_t tag[TCC_HSM_AES_TAG_SIZE];
	uint32_t tag_size;
	uint8_t aad[TCC_HSM_AES_AAD_SIZE];
	uint32_t aad_size;
	ulong src;
	uint32_t src_size;
	ulong dst;
	uint32_t dst_size;
	uint32_t dma;
};

struct tcc_hsm_ioctl_mac_param {
	uint32_t obj_id;
	uint8_t mac_key[TCC_HSM_MAC_KEY_SIZE];
	uint32_t key_size;
	ulong src;
	uint32_t src_size;
	uint8_t mac[TCC_HSM_MAC_MSG_SIZE];
	uint32_t mac_size;
	uint32_t dma;
};

struct tcc_hsm_ioctl_mac_by_kt_param {
	uint32_t obj_id;
	uint32_t key_index;
	ulong src;
	uint32_t src_size;
	uint8_t mac[TCC_HSM_MAC_MSG_SIZE];
	uint32_t mac_size;
	uint32_t dma;
};

struct tcc_hsm_ioctl_hash_param {
	uint32_t obj_id;
	ulong src;
	uint32_t src_size;
	uint8_t digest[TCC_HSM_HASH_DIGEST_SIZE];
	uint32_t digest_size;
	uint32_t dma;
};

struct tcc_hsm_ioctl_ecdsa_param {
	uint32_t obj_id;
	uint8_t ecdsa_key[TCC_HSM_ECDSA_MAX_KEY_SIZE * 2];
	uint32_t key_size;
	uint8_t digest[TCC_HSM_ECDSA_DIGEST_SIZE];
	uint32_t digest_size;
	uint8_t sig[TCC_HSM_ECDSA_SIGN_SIZE];
	uint32_t sig_size;
};

struct tcc_hsm_ioctl_ecdsa_by_kt_param {
	uint32_t obj_id;
	uint32_t key_index;
	uint8_t digest[TCC_HSM_ECDSA_DIGEST_SIZE];
	uint32_t digest_size;
	uint8_t sig[TCC_HSM_ECDSA_SIGN_SIZE];
	uint32_t sig_size;
};

struct tcc_hsm_ioctl_rsaes_param {
	uint32_t obj_id;
	uint8_t modN[TCC_HSM_RSA_MODN_SIZE];
	uint32_t modN_size;
	uint8_t rsa_key[TCC_HSM_RSA_MODN_SIZE];
	uint32_t key_size;
	uint8_t src[TCC_HSM_RSA_MODN_SIZE];
	uint32_t src_size;
	uint8_t dst[TCC_HSM_RSA_MODN_SIZE];
	uint32_t dst_size;
	uint8_t label[TCC_HSM_RSA_MODN_SIZE];
	uint32_t label_size;
};
struct tcc_hsm_ioctl_rsaes_by_kt_param {
	uint32_t obj_id;
	uint32_t key_index;
	uint8_t src[TCC_HSM_RSA_MODN_SIZE];
	uint32_t src_size;
	uint8_t dst[TCC_HSM_RSA_MODN_SIZE];
	uint32_t dst_size;
	uint8_t label[TCC_HSM_RSA_MODN_SIZE];
	uint32_t label_size;
};
struct tcc_hsm_ioctl_rsassa_param {
	uint32_t obj_id;
	uint8_t modN[TCC_HSM_RSA_MODN_SIZE];
	uint32_t modN_size;
	uint8_t rsa_key[TCC_HSM_RSA_MODN_SIZE];
	uint32_t key_size;
	uint8_t digest[TCC_HSM_RSA_DIG_SIZE];
	uint32_t digest_size;
	uint8_t sig[TCC_HSM_RSA_SIG_SIZE];
	uint32_t sig_size;
};

struct tcc_hsm_ioctl_rsassa_by_kt_param {
	uint32_t obj_id;
	uint32_t key_index;
	uint8_t digest[TCC_HSM_RSA_DIG_SIZE];
	uint32_t digest_size;
	uint8_t sig[TCC_HSM_RSA_SIG_SIZE];
	uint32_t sig_size;
};

struct tcc_hsm_ioctl_write_param {
	uint32_t addr;
	ulong data;
	uint32_t data_size;
	uint32_t dma;
};

struct tcc_hsm_ioctl_rng_param {
	ulong rng;
	uint32_t rng_size;
};

struct tcc_hsm_ioctl_version_param {
	uint32_t x;
	uint32_t y;
	uint32_t z;
};

struct tcc_hsm_ioctl_ecdh_key_param {
	uint32_t key_type;
	uint32_t obj_id;
	uint8_t prikey[TCC_HSM_ECDSA_MAX_KEY_SIZE];
	uint32_t prikey_size;
	uint8_t pubkey[TCC_HSM_ECDSA_MAX_KEY_SIZE * 2];
	uint32_t pubkey_size;
};

uint32_t tcc_hsm_cmd_set_key(uint32_t device_id, uint32_t req,
			     const struct tcc_hsm_ioctl_set_key_param *param);
uint32_t tcc_hsm_cmd_set_modn(uint32_t device_id, uint32_t req,
			      const struct tcc_hsm_ioctl_set_modn_param *param);
uint32_t tcc_hsm_cmd_run_aes(uint32_t device_id, uint32_t req,
			     struct tcc_hsm_ioctl_aes_param *param,
			     ulong desc, uint32_t descPhy_size);
uint32_t tcc_hsm_cmd_run_aes_by_kt(uint32_t device_id, uint32_t req,
				   struct tcc_hsm_ioctl_aes_by_kt_param *param,
				   ulong desc, uint32_t descPhy_size);
uint32_t tcc_hsm_cmd_gen_mac(uint32_t device_id, uint32_t req,
			     struct tcc_hsm_ioctl_mac_param *param);
uint32_t tcc_hsm_cmd_gen_mac_by_kt(uint32_t device_id, uint32_t req,
				   struct tcc_hsm_ioctl_mac_by_kt_param *param);
uint32_t tcc_hsm_cmd_gen_hash(uint32_t device_id, uint32_t req,
			      struct tcc_hsm_ioctl_hash_param *param);
uint32_t
tcc_hsm_cmd_run_ecdh_phaseI(uint32_t device_id, uint32_t req,
			    struct tcc_hsm_ioctl_ecdh_key_param *param);
uint32_t tcc_hsm_cmd_run_ecdsa(uint32_t device_id, uint32_t req,
			       const struct tcc_hsm_ioctl_ecdsa_param *aram);
uint32_t
tcc_hsm_cmd_run_ecdsa_by_kt(uint32_t device_id, uint32_t req,
			    const struct tcc_hsm_ioctl_ecdsa_by_kt_param *aram);
uint32_t tcc_hsm_cmd_run_rsaes(uint32_t device_id, uint32_t req,
			       struct tcc_hsm_ioctl_rsaes_param *param,
			       ulong src, ulong dst);
uint32_t
tcc_hsm_cmd_run_rsaes_by_kt(uint32_t device_id, uint32_t req,
			    struct tcc_hsm_ioctl_rsaes_by_kt_param *param,
			    ulong src, ulong dst);
uint32_t tcc_hsm_cmd_run_rsassa(uint32_t device_id, uint32_t req,
				const struct tcc_hsm_ioctl_rsassa_param *param,
				ulong src_addr, ulong sig);
uint32_t
tcc_hsm_cmd_run_rsassa_by_kt(uint32_t device_id, uint32_t req,
			     const struct tcc_hsm_ioctl_rsassa_by_kt_param *param,
			     ulong digest, ulong sig);
uint32_t tcc_hsm_cmd_write(uint32_t device_id, uint32_t req,
			   const struct tcc_hsm_ioctl_write_param *param);
uint32_t tcc_hsm_cmd_get_version(uint32_t device_id, uint32_t req,
				 struct tcc_hsm_ioctl_version_param *param);
uint32_t tcc_hsm_cmd_get_rand(uint32_t device_id, uint32_t req, ulong rng,
			      uint32_t rng_size);
#endif /* TCC_HSM_CMD_H */
