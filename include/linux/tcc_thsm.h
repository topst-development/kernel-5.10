// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef _TCC_THSM_H_
#define _TCC_THSM_H_

#define TCCTHSM_DEVICE_NAME "tcc_thsm"
#define TCCTHSM_RNG_MAX 16
#define TCCTHSM_OBJ_ID_MAX 64

// clang-format off
#define TCCTHSM_IOCTL_MAGIC								(0x48U)
#define TCCTHSM_IOCTL_INIT 								(_IOWR(TCCTHSM_IOCTL_MAGIC, 0U, uint32_t))  // Generic
#define TCCTHSM_IOCTL_FINALIZE 							(_IOWR(TCCTHSM_IOCTL_MAGIC, 1U, uint32_t))  // Generic
#define TCCTHSM_IOCTL_GET_TA_VERSION 					(_IOWR(TCCTHSM_IOCTL_MAGIC, 2U, uint32_t))  // Generic
#define TCCTHSM_IOCTL_SET_MODE 							(_IOWR(TCCTHSM_IOCTL_MAGIC, 3U, uint32_t))  // Generic
#define TCCTHSM_IOCTL_SET_KEY 							(_IOWR(TCCTHSM_IOCTL_MAGIC, 4U, uint32_t))  // Generic
#define TCCTHSM_IOCTL_SET_KEY_V2						(_IOWR(TCCTHSM_IOCTL_MAGIC, 5U, uint32_t))  // Generic
#define TCCTHSM_IOCTL_SET_KEY_FROM_STORAGE 				(_IOWR(TCCTHSM_IOCTL_MAGIC, 6U, uint32_t))  // Generic
#define TCCTHSM_IOCTL_SET_KEY_FROM_OTP 					(_IOWR(TCCTHSM_IOCTL_MAGIC, 7U, uint32_t))  // Generic
#define TCCTHSM_IOCTL_FREE_MODE 						(_IOWR(TCCTHSM_IOCTL_MAGIC, 8U, uint32_t))  // Generic
#define TCCTHSM_IOCTL_GET_DAEMON_VERSION 				(_IOWR(TCCTHSM_IOCTL_MAGIC, 9U, uint32_t))  // Generic
#define TCCTHSM_IOCTL_GET_CA_VERSION 					(_IOWR(TCCTHSM_IOCTL_MAGIC, 10U, uint32_t)) // Generic
#define TCCTHSM_IOCTL_GET_DRIVER_VERSION 				(_IOWR(TCCTHSM_IOCTL_MAGIC, 11U, uint32_t)) // Generic

#define TCCTHSM_IOCTL_SET_IV_SYMMETRIC 					(_IOWR(TCCTHSM_IOCTL_MAGIC, 12U, uint32_t)) // Symmetric Cipher
#define TCCTHSM_IOCTL_RUN_CIPHER 						(_IOWR(TCCTHSM_IOCTL_MAGIC, 13U, uint32_t)) // Symmetric Cipher
#define TCCTHSM_IOCTL_RUN_DIGEST 						(_IOWR(TCCTHSM_IOCTL_MAGIC, 14U, uint32_t)) // Message Digest (SHA)
#define TCCTHSM_IOCTL_SET_IV_MAC 						(_IOWR(TCCTHSM_IOCTL_MAGIC, 15U, uint32_t)) // Message Authentication Code (MAC)
#define TCCTHSM_IOCTL_COMPUTE_MAC 						(_IOWR(TCCTHSM_IOCTL_MAGIC, 16U, uint32_t)) // Message Authentication Code (MAC)
#define TCCTHSM_IOCTL_COMPARE_MAC 						(_IOWR(TCCTHSM_IOCTL_MAGIC, 17U, uint32_t)) // Message Authentication Code (MAC)
#define TCCTHSM_IOCTL_GET_RAND 							(_IOWR(TCCTHSM_IOCTL_MAGIC, 18U, uint32_t)) // RNG
#define TCCTHSM_IOCTL_GEN_KEY_SS 						(_IOWR(TCCTHSM_IOCTL_MAGIC, 19U, uint32_t)) // Key Generate
#define TCCTHSM_IOCTL_DEL_KEY_SS 						(_IOWR(TCCTHSM_IOCTL_MAGIC, 20U, uint32_t)) // Key Generate
#define TCCTHSM_IOCTL_WRITE_KEY_SS 						(_IOWR(TCCTHSM_IOCTL_MAGIC, 21U, uint32_t)) // Key Generate
#define TCCTHSM_IOCTL_WRITE_KEY_SS_V2					(_IOWR(TCCTHSM_IOCTL_MAGIC, 22U, uint32_t)) // Key Generate
#define TCCTHSM_IOCTL_KDF								(_IOWR(TCCTHSM_IOCTL_MAGIC, 23U, uint32_t)) // Key Generate
#define TCCTHSM_IOCTL_ECDH_COMPUTE_PUBKEY				(_IOWR(TCCTHSM_IOCTL_MAGIC, 24U, uint32_t)) // Key Generate
#define TCCTHSM_IOCTL_ECDH_PHASE_I						(_IOWR(TCCTHSM_IOCTL_MAGIC, 25U, uint32_t)) // Key Generate
#define TCCTHSM_IOCTL_ECDH_PHASE_II						(_IOWR(TCCTHSM_IOCTL_MAGIC, 26U, uint32_t)) // Key Generate

#define TCCTHSM_IOCTL_WRITE_OTP 						(_IOWR(TCCTHSM_IOCTL_MAGIC, 27U, uint32_t)) // OTP
#define TCCTHSM_IOCTL_WRITE_OTP_IMAGE 					(_IOWR(TCCTHSM_IOCTL_MAGIC, 28U, uint32_t)) // OTP
#define TCCTHSM_IOCTL_ASYMMETRIC_ENC_DEC				(_IOWR(TCCTHSM_IOCTL_MAGIC, 29U, uint32_t)) // Asymmetric
#define TCCTHSM_IOCTL_ASYMMETRIC_SIGN 					(_IOWR(TCCTHSM_IOCTL_MAGIC, 30U, uint32_t)) // Asymmetric
#define TCCTHSM_IOCTL_ASYMMETRIC_VERIFY 				(_IOWR(TCCTHSM_IOCTL_MAGIC, 31U, uint32_t)) // Asymmetric

#define TCCTHSM_IOCTL_RUN_CIPHER_BY_DMA 				(_IOWR(TCCTHSM_IOCTL_MAGIC, 32U, uint32_t)) // Symmetric Cipher
#define TCCTHSM_IOCTL_RUN_DIGEST_BY_DMA 				(_IOWR(TCCTHSM_IOCTL_MAGIC, 33U, uint32_t)) // Message Digest (SHA)
#define TCCTHSM_IOCTL_COMPUTE_MAC_BY_DMA 				(_IOWR(TCCTHSM_IOCTL_MAGIC, 34U, uint32_t)) // Message Authentication Code(MAC)
#define TCCTHSM_IOCTL_COMPARE_MAC_BY_DMA 				(_IOWR(TCCTHSM_IOCTL_MAGIC, 35U, uint32_t)) // Message Authentication Code(MAC)
#define TCCTHSM_IOCTL_ASYMMETRIC_ENC_DEC_BY_DMA			(_IOWR(TCCTHSM_IOCTL_MAGIC, 36U, uint32_t)) // Asymmetric
// clang-format on

enum tcc_thsm_ioctl_cipher_cw_sel {
	TCKL = 0,
	CPU_Key = 1,
};

enum tcc_thsm_ioctl_cipher_algo {
	NONE = 0,
	DVB_CSA2 = 1,
	DVB_CSA3 = 2,
	AES_128 = 3,
	DES = 4,
	TDES_128 = 5,
	Multi2 = 6,
};

enum tcc_thsm_ioctl_cipher_op_mode {
	ECB = 0,
	CBC = 1,
	CTR_128 = 4,
	CTR_64 = 5,
};

enum tcc_thsm_ioctl_cipher_key_type {
	CORE_Key = 0,
	Multi2_System_Key = 1,
	CMAC_Key = 2,
};

enum tcc_thsm_ioctl_cipher_key_mode {
	TS_NOT_SCRAMBLED = 0,
	TS_RESERVED,
	TS_SCRAMBLED_WITH_EVENKEY,
	TS_SCRAMBLED_WITH_ODDKEY
};

#endif /*_TCC_THSM_H_*/
