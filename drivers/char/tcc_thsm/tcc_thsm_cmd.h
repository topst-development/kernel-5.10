// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef _TCC_THSM_CMD_H_
#define _TCC_THSM_CMD_H_

#define THSM_EVENT(magic_num, evt) (((magic_num & 0xF) << 12) | (evt & 0xFFF))
/* tccTHSM command range: 0x5000 ~ 0x5FFF */

// clang-format off
#define MAGIC_NUM                         (0x0005UL)  // THSM magic number
#define TCCTHSM_EVT_INIT        	  	  THSM_EVENT(MAGIC_NUM, 0x011U) // Generic
#define TCCTHSM_EVT_FINALIZE       		  THSM_EVENT(MAGIC_NUM, 0x012U) // Generic
#define TCCTHSM_EVT_GET_TA_VERSION        THSM_EVENT(MAGIC_NUM, 0x013U) // Generic
#define TCCTHSM_EVT_SET_MODE              THSM_EVENT(MAGIC_NUM, 0x014U) // Generic
#define TCCTHSM_EVT_SET_KEY               THSM_EVENT(MAGIC_NUM, 0x015U) // Generic
#define TCCTHSM_EVT_SET_KEY_V2            THSM_EVENT(MAGIC_NUM, 0x016U) // Generic
#define TCCTHSM_EVT_SET_KEY_FROM_STORAGE  THSM_EVENT(MAGIC_NUM, 0x017U) // Generic
#define TCCTHSM_EVT_SET_KEY_FROM_OTP      THSM_EVENT(MAGIC_NUM, 0x018U) // Generic
#define TCCTHSM_EVT_FREE_MODE             THSM_EVENT(MAGIC_NUM, 0x019U) // Generic
#define TCCTHSM_EVT_GET_DAEMON_VERSION    THSM_EVENT(MAGIC_NUM, 0x01AU) // Generic
#define TCCTHSM_EVT_GET_CA_VERSION    	  THSM_EVENT(MAGIC_NUM, 0x01BU) // Generic

#define TCCTHSM_EVT_SET_IV_SYMMETRIC      THSM_EVENT(MAGIC_NUM, 0x021U) // Symmetric Cipher
#define TCCTHSM_EVT_RUN_CIPHER            THSM_EVENT(MAGIC_NUM, 0x022U) // Symmetric Cipher
#define TCCTHSM_EVT_RUN_DIGEST            THSM_EVENT(MAGIC_NUM, 0x031U) // Message Digest (SHA)
#define TCCTHSM_EVT_SET_IV_MAC            THSM_EVENT(MAGIC_NUM, 0x041U) // Message Authentication Code (MAC)
#define TCCTHSM_EVT_COMPUTE_MAC           THSM_EVENT(MAGIC_NUM, 0x042U) // Message Authentication Code (MAC)
#define TCCTHSM_EVT_COMPARE_MAC           THSM_EVENT(MAGIC_NUM, 0x043U) // Message Authentication Code (MAC)
#define TCCTHSM_EVT_GET_RAND              THSM_EVENT(MAGIC_NUM, 0x051U) // RNG
#define TCCTHSM_EVT_GEN_KEY_SS     	      THSM_EVENT(MAGIC_NUM, 0x061U) // Key Generate
#define TCCTHSM_EVT_DEL_KEY_SS            THSM_EVENT(MAGIC_NUM, 0x062U) // Key Generate
#define TCCTHSM_EVT_WRITE_KEY_SS          THSM_EVENT(MAGIC_NUM, 0x063U) // Key Generate
#define TCCTHSM_EVT_WRITE_KEY_SS_V2       THSM_EVENT(MAGIC_NUM, 0x064U) // Key Generate
#define TCCTHSM_EVT_KDF                   THSM_EVENT(MAGIC_NUM, 0x065U) // Key Generate
#define TCCTHSM_EVT_ECDH_COMPUTE_PUBKEY   THSM_EVENT(MAGIC_NUM, 0x066U) // Key Generate
#define TCCTHSM_EVT_HSM_ECDH_PHASE_I      THSM_EVENT(MAGIC_NUM, 0x067U) // Key Generate
#define TCCTHSM_EVT_HSM_ECDH_PHASE_II     THSM_EVENT(MAGIC_NUM, 0x068U) // Key Generate

#define TCCTHSM_EVT_WRITE_OTP             THSM_EVENT(MAGIC_NUM, 0x071U) // OTP
#define TCCTHSM_EVT_WRITE_OTP_IMAGE       THSM_EVENT(MAGIC_NUM, 0x072U) // OTP
#define TCCTHSM_EVT_ASYMMETRIC_ENC        THSM_EVENT(MAGIC_NUM, 0x081U) // Asymmetric
#define TCCTHSM_EVT_ASYMMETRIC_DEC        THSM_EVENT(MAGIC_NUM, 0x082U) // Asymmetric
#define TCCTHSM_EVT_ASYMMETRIC_SIGN       THSM_EVENT(MAGIC_NUM, 0x083U) // Asymmetric
#define TCCTHSM_EVT_ASYMMETRIC_VERIFY     THSM_EVENT(MAGIC_NUM, 0x084U) // Asymmetric

#define TCCTHSM_EVT_RUN_CIPHER_BY_DMA     THSM_EVENT(MAGIC_NUM, 0x091U) // Symmetric Cipher
#define TCCTHSM_EVT_RUN_DIGEST_BY_DMA     THSM_EVENT(MAGIC_NUM, 0x092U) // Message Digest (SHA)
#define TCCTHSM_EVT_COMPUTE_MAC_BY_DMA    THSM_EVENT(MAGIC_NUM, 0x093U) // Message Authentication Code (MAC)
#define TCCTHSM_EVT_COMPARE_MAC_BY_DMA    THSM_EVENT(MAGIC_NUM, 0x094U) // Message Authentication Code (MAC)
#define TCCTHSM_EVT_ASYMMETRIC_ENC_BY_DMA THSM_EVENT(MAGIC_NUM, 0x095U) // Asymmetric
#define TCCTHSM_EVT_ASYMMETRIC_DEC_BY_DMA THSM_EVENT(MAGIC_NUM, 0x096U) // Asymmetric
// clang-format on

#define TCCTHSM_CIPHER_KEYSIZE_FOR_64 8
#define TCCTHSM_CIPHER_KEYSIZE_FOR_128 16
#define TCCTHSM_CIPHER_KEYSIZE_FOR_192 24
#define TCCTHSM_CIPHER_KEYSIZE_FOR_256 32

#define TCCTHSM_KEY_TYPE_SECRET 0x1U
#define TCCTHSM_KEY_TYPE_RSA_PAIR 0x2U
#define TCCTHSM_KEY_TYPE_RSA_PUBLIC 0x3U
#define TCCTHSM_KEY_TYPE_ECDSA_PAIR 0x4U
#define TCCTHSM_KEY_TYPE_ECDSA_PUBLIC 0x5U

#define TCCTHSM_SECRET_KEY_MAX_SIZE 128
#define TCCTHSM_RSA_KEY_MAX_SIZE 128
#define TCCTHSM_ECDSA_KEY_MAX_SIZE 68

#define TCCTHSM_MAX_KEYSIZE 256U

enum tcc_thsm_cmd_cipher_enc {
	DECRYPTION = 0,
	ENCRYPTION = 1,
};

struct TEETHSMKey {
	uint32_t keyType;
	union {
		struct {
			uint8_t key1[TCCTHSM_SECRET_KEY_MAX_SIZE];
			uint32_t key1Size;
			uint8_t key2[TCCTHSM_SECRET_KEY_MAX_SIZE];
			uint32_t key2Size;
		} SECRET;
		struct {
			uint8_t modulus[TCCTHSM_RSA_KEY_MAX_SIZE];
			uint32_t modulusSize;
			uint8_t publicExp[TCCTHSM_RSA_KEY_MAX_SIZE];
			uint32_t publicExpSize;
			uint8_t privateExp[TCCTHSM_RSA_KEY_MAX_SIZE];
			uint32_t privateExpSize;
		} RSA;
		struct {
			uint8_t private[TCCTHSM_ECDSA_KEY_MAX_SIZE];
			uint32_t privateSize;
			uint8_t publicX[TCCTHSM_ECDSA_KEY_MAX_SIZE];
			uint32_t publicXSize;
			uint8_t publicY[TCCTHSM_ECDSA_KEY_MAX_SIZE];
			uint32_t publicYSize;
		} ECDSA;
	} key;
};
struct tcc_thsm_ioctl_version_param {
	uint32_t major;
	uint32_t minor;
};

struct tcc_thsm_ioctl_set_mode_param {
	uint32_t key_index;
	uint32_t algorithm;
	uint32_t op_mode;
	uint32_t key_size;
};

struct tcc_thsm_ioctl_set_key_param {
	uint32_t key_index;
	uint32_t *key1;
	uint32_t key1_size;
	uint32_t *key2;
	uint32_t key2_size;
	uint32_t *key3;
	uint32_t key3_size;
};

struct tcc_thsm_ioctl_set_key_v2_param {
	uint32_t key_index;
	struct TEETHSMKey *buffer;
};

struct tcc_thsm_ioctl_set_key_storage_param {
	uint32_t key_index;
	uint8_t *obj_id;
	uint32_t obj_id_len;
};

struct tcc_thsm_ioctl_set_key_otp_param {
	uint32_t key_index;
	uint32_t otp_addr;
	uint32_t key_size;
};

struct tcc_thsm_ioctl_set_iv_param {
	uint32_t key_index;
	uint32_t *iv;
	uint32_t iv_size;
};

struct tcc_thsm_ioctl_cipher_param {
	uint32_t key_index;
	uint8_t *src_addr;
	uint32_t src_size;
	uint8_t *dst_addr;
	uint32_t *dst_size;
	uint32_t flag;
};

struct tcc_thsm_ioctl_cipher_dma_param {
	uint32_t key_index;
	uint32_t src_addr;
	uint32_t src_size;
	uint32_t dst_addr;
	uint32_t *dst_size;
	uint32_t flag;
};

struct tcc_thsm_ioctl_run_digest_param {
	uint32_t key_index;
	uint8_t *chunk;
	uint32_t chunk_len;
	uint8_t *hash;
	uint32_t *hash_len;
	uint32_t flag;
};

struct tcc_thsm_ioctl_run_digest_dma_param {
	uint32_t key_index;
	uint32_t chunk_addr;
	uint32_t chunk_len;
	uint8_t *hash;
	uint32_t *hash_len;
	uint32_t flag;
};

struct tcc_thsm_ioctl_compute_mac_param {
	uint32_t key_index;
	uint8_t *message;
	uint32_t message_len;
	uint8_t *mac;
	uint32_t *mac_len;
	uint32_t flag;
};

struct tcc_thsm_ioctl_compute_mac_dma_param {
	uint32_t key_index;
	uint32_t message_addr;
	uint32_t message_len;
	uint8_t *mac;
	uint32_t *mac_len;
	uint32_t flag;
};

struct tcc_thsm_ioctl_compare_mac_param {
	uint32_t key_index;
	uint8_t *message;
	uint32_t message_len;
	uint8_t *mac;
	uint32_t mac_len;
	uint32_t flag;
};

struct tcc_thsm_ioctl_compare_mac_dma_param {
	uint32_t key_index;
	uint32_t message_addr;
	uint32_t message_len;
	uint8_t *mac;
	uint32_t mac_len;
	uint32_t flag;
};

struct tcc_thsm_ioctl_rng_param {
	uint32_t *rng;
	uint32_t size;
};

struct tcc_thsm_ioctl_gen_key_param {
	int8_t *obj_id;
	uint32_t obj_len;
	uint32_t algorithm;
	uint32_t key_size;
};

struct tcc_thsm_ioctl_del_key_param {
	int8_t *obj_id;
	uint32_t obj_len;
};

struct tcc_thsm_ioctl_write_key_param {
	int8_t *obj_id;
	uint32_t obj_len;
	uint8_t *buffer;
	uint32_t size;
};

struct tcc_thsm_ioctl_write_key_v2_param {
	int8_t *obj_id;
	uint32_t obj_len;
	struct TEETHSMKey *buffer;
};

struct tcc_thsm_ioctl_ecdh_param {
	uint32_t algorithm;
	uint8_t *pubkey;
	uint32_t pubkey_size;
	uint8_t *prikey;
	uint32_t prikey_size;
};

struct tcc_thsm_ioctl_kdf_param {
	uint32_t algorithm;
	uint8_t *key; // IKM(Input Keying Material)
	uint32_t key_size;
	uint8_t *salt;
	uint32_t salt_size;
	uint8_t *info;
	uint32_t info_size;
	uint8_t *sessionkey;
	uint32_t session_size;
	uint32_t type;
};

struct tcc_thsm_ioctl_ecdh_phaseII_param {
	uint32_t algorithm;
	uint8_t *pubkey;
	uint32_t pubkey_size;
	uint8_t *prikey;
	uint32_t prikey_size;
	uint8_t *seckey;
	uint32_t seckey_size;
};

struct tcc_thsm_ioctl_otp_param {
	uint32_t otp_addr;
	uint8_t *buf;
	uint32_t size;
};

struct tcc_thsm_ioctl_otpimage_param {
	uint32_t otp_addr;
	uint32_t size;
};

struct tcc_thsm_ioctl_asym_enc_dec_param {
	uint32_t key_index;
	uint8_t *src_addr;
	uint32_t src_size;
	uint8_t *dst_addr;
	uint32_t *dst_size;
	uint32_t enc;
};

struct tcc_thsm_ioctl_asym_enc_dec_dma_param {
	uint32_t key_index;
	uint32_t src_addr;
	uint32_t src_size;
	uint32_t dst_addr;
	uint32_t *dst_size;
	uint32_t enc;
};

struct tcc_thsm_ioctl_asym_sign_digest_param {
	uint32_t key_index;
	uint8_t *dig;
	uint32_t dig_size;
	uint8_t *sig;
	uint32_t *sig_size;
};

struct tcc_thsm_ioctl_asym_verify_digest_param {
	uint32_t key_index;
	uint8_t *dig;
	uint32_t dig_size;
	uint8_t *sig;
	uint32_t sig_size;
};
int32_t tcc_thsm_cmd_init(uint32_t device_id);
int32_t tcc_thsm_cmd_finalize(uint32_t device_id);
int32_t tcc_thsm_cmd_get_ta_version(uint32_t device_id, uint32_t *major,
				    uint32_t *minor);
int32_t tcc_thsm_cmd_get_ca_version(uint32_t device_id, uint32_t *major,
				    uint32_t *minor);
int32_t tcc_thsm_cmd_get_daemon_version(uint32_t device_id, uint32_t *major,
					uint32_t *minor);
int32_t tcc_thsm_cmd_set_mode(uint32_t device_id, uint32_t key_index,
			      uint32_t algorithm, uint32_t op_mode,
			      uint32_t key_size);
int32_t tcc_thsm_cmd_set_key(uint32_t device_id, uint32_t key_index,
			     uint32_t *key1, uint32_t key1_size, uint32_t *key2,
			     uint32_t key2_size, uint32_t *key3,
			     uint32_t key3_size);
int32_t tcc_thsm_cmd_set_key_v2(uint32_t device_id, uint32_t key_index,
				struct TEETHSMKey *key);
int32_t tcc_thsm_cmd_set_key_from_storage(uint32_t device_id,
					  uint32_t key_index, uint8_t *obj_id,
					  uint32_t obj_id_len);
int32_t tcc_thsm_cmd_set_key_from_otp(uint32_t device_id, uint32_t key_index,
				      uint32_t otp_addr, uint32_t key_size);
int32_t tcc_thsm_cmd_free_mode(uint32_t device_id, uint32_t key_index);
int32_t tcc_thsm_cmd_set_iv_symmetric(uint32_t device_id, uint32_t key_index,
				      uint32_t *iv, uint32_t iv_size);
int32_t tcc_thsm_cmd_run_cipher_by_dma(uint32_t device_id, uint32_t key_index,
				       uint32_t src_addr, uint32_t src_size,
				       uint32_t dst_addr, uint32_t *dst_size,
				       uint32_t flag);
int32_t tcc_thsm_cmd_run_digest_by_dma(uint32_t device_id, uint32_t key_index,
				       uint32_t chunk_addr, uint32_t chunk_len,
				       uint8_t *hash, uint32_t *hash_len,
				       uint32_t flag);
int32_t tcc_thsm_cmd_set_iv_mac(uint32_t device_id, uint32_t key_index,
				uint32_t *iv, uint32_t iv_size);
int32_t tcc_thsm_cmd_compute_mac_by_dma(uint32_t device_id, uint32_t key_index,
					uint32_t message, uint32_t message_len,
					uint8_t *mac, uint32_t *mac_len,
					uint32_t flag);
int32_t tcc_thsm_cmd_compare_mac_by_dma(uint32_t device_id, uint32_t key_index,
					uint32_t message, uint32_t message_len,
					uint8_t *mac, uint32_t mac_len,
					uint32_t flag);
int32_t tcc_thsm_cmd_get_rand(uint32_t device_id, uint32_t *rng,
			      uint32_t rng_size);
int32_t tcc_thsm_cmd_gen_key_ss(uint32_t device_id, int8_t *obj_id,
				uint32_t obj_len, uint32_t algorithm,
				uint32_t key_size);
int32_t tcc_thsm_cmd_del_key_ss(uint32_t device_id, int8_t *obj_id,
				uint32_t obj_len);
int32_t tcc_thsm_cmd_write_key_ss(uint32_t device_id, int8_t *obj_id,
				  uint32_t obj_len, uint8_t *buffer,
				  uint32_t buffer_size);
int32_t tcc_thsm_cmd_write_key_ss_v2(uint32_t device_id, int8_t *obj_id,
				     uint32_t obj_len,
				     struct TEETHSMKey *buffer);
int32_t tcc_thsm_cmd_kdf(uint32_t device_id, uint32_t algorithm,
			 uint8_t *secKey, uint32_t sec_size, uint8_t *salt,
			 uint32_t salt_size, uint8_t *info, uint32_t info_size,
			 uint8_t *sessionKey, uint32_t session_size,
			 uint32_t type);
int32_t tcc_thsm_cmd_ecdh_compute_pubkey(uint32_t device_id, uint32_t algorithm,
					 uint8_t *pubKey, uint32_t pub_size,
					 uint8_t *priKey, uint32_t pri_size);
int32_t tcc_thsm_cmd_ecdh_phaseI(uint32_t device_id, uint32_t algorithm,
				 uint8_t *pubKey, uint32_t pub_size,
				 uint8_t *priKey, uint32_t pri_size);
int32_t tcc_thsm_cmd_ecdh_phaseII(uint32_t device_id, uint32_t algorithm,
				  uint8_t *pubKey, uint32_t pub_size,
				  uint8_t *priKey, uint32_t pri_size,
				  uint8_t *secKey, uint32_t sec_size);
int32_t tcc_thsm_cmd_write_otp(uint32_t device_id, uint32_t otp_addr,
			       uint8_t *otp_buf, uint32_t otp_size);
int32_t tcc_thsm_cmd_write_otpimage(uint32_t device_id, uint32_t otp_addr,
				    uint32_t otp_size);
int32_t tcc_thsm_cmd_asym_enc_dec_by_dma(uint32_t device_id, uint32_t key_index,
					 uint32_t src_addr, uint32_t src_size,
					 uint32_t dst_addr, uint32_t *dst_size,
					 uint32_t enc);
int32_t tcc_thsm_cmd_asym_sign_digest(uint32_t device_id, uint32_t key_index,
				      uint8_t *dig, uint32_t dig_size,
				      uint8_t *sig, uint32_t *sig_size);
int32_t tcc_thsm_cmd_asym_verify_digest(uint32_t device_id, uint32_t key_index,
					uint8_t *dig, uint32_t dig_size,
					uint8_t *sig, uint32_t sig_size);

#endif /*_TCC_THSM_CMD_H_*/
