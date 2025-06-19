// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#if 0
#define NDEBUG
#endif
#define TLOG_LEVEL (TLOG_WARNING)
#include "tcc_hsm_log.h"

#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/mailbox/tcc_sec_ipc.h>
#ifdef CONFIG_PM
#include <linux/pm.h>
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include <linux/timer.h>
#include <linux/delay.h>

#include <linux/io.h>
#include <asm/div64.h>

#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include "tcc_hsm.h"
#include "tcc_hsm_cmd.h"

/****************************************************************************
 * DEFINITiON
 ****************************************************************************/
#define TCC_HSM_DESC_BUF_SIZE (512U)
#define TCC_HSM_DMA_BUF_SIZE (4096U)

#define HSM_REQUIRED_FW_VER_X (1u)
#define HSM_REQUIRED_FW_VER_Y (0u)
#define HSM_REQUIRED_FW_VER_Z (0u)

#define HSM_DRIVER_VER_X (1u)
#define HSM_DRIVER_VER_Y (0u)
#define HSM_DRIVER_VER_Z (4u)

/****************************************************************************
 * DEFINITION OF LOCAL VARIABLES
 ****************************************************************************/
static DEFINE_MUTEX(tcc_hsm_mutex);

static struct tcc_hsm_dma_buf {
	struct device *dev;
	dma_addr_t descPhy;
	uint32_t *descVir;
	dma_addr_t srcPhy;
	uint8_t *srcVir;
	dma_addr_t dstPhy;
	uint8_t *dstVir;
} * hsm_dma_buf;

static struct tcc_hsm_ioctl_version_param gVerParam;
/****************************************************************************
 * DEFINITION OF LOCAL FUNCTIONS
 ****************************************************************************/
static uint32_t tcc_hsm_ioctl_set_key(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_set_key_param param;
	uint32_t req = 0;
	uint32_t ret = TCCHSM_ERR;

	if (cmd == HSM_SET_KEY_FROM_OTP_CMD) {
		req = REQ_HSM_SET_KEY_FROM_OTP;
	} else if (cmd == HSM_SET_KEY_FROM_SNOR_CMD) {
		req = REQ_HSM_SET_KEY_FROM_SNOR;
	} else {
		ELOG("Invalid CMD\n");
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	if (copy_from_user((void *)&param, (ulong *)arg, (uint32_t)sizeof(param)) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	ret = tcc_hsm_cmd_set_key(MBOX_DEV_HSM, req, &param);
	if (ret != TCCHSM_SUCCESS) {
		ELOG("tcc_hsm_cmd_set_key failed\n");
		goto out;
	}

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_set_modn(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_set_modn_param param;
	uint32_t req = 0;
	uint32_t ret = TCCHSM_ERR;

	if (cmd == HSM_SET_MODN_FROM_OTP_CMD) {
		req = REQ_HSM_SET_MODN_FROM_OTP;
	} else if (cmd == HSM_SET_MODN_FROM_SNOR_CMD) {
		req = REQ_HSM_SET_MODN_FROM_SNOR;
	} else {
		ELOG("Invalid CMD\n");
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	if (copy_from_user((void *)&param, (ulong *)arg, (uint32_t)sizeof(param)) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	ret = tcc_hsm_cmd_set_modn(MBOX_DEV_HSM, req, &param);
	if (ret != TCCHSM_SUCCESS) {
		ELOG("tcc_hsm_cmd_set_modn failed\n");
		goto out;
	}

out:
	return ret;
}
static uint32_t tcc_hsm_run_aes(uint32_t req, struct tcc_hsm_ioctl_aes_param *param)
{
	ulong user_dst;
	uint32_t obj_id = 0;
	uint32_t op_mode = 0;
	uint32_t idx = 0;
	uint32_t descPhy_size = 0;
	uint8_t *aad = NULL;
	uint32_t ret = TCCHSM_ERR;

	if ((param->aad_size > TCC_HSM_AES_AAD_SIZE) ||
	    (param->key_size > TCC_HSM_AES_KEY_SIZE) ||
	    (param->iv_size > TCC_HSM_AES_IV_SIZE) ||
	    (param->tag_size > TCC_HSM_AES_TAG_SIZE)) {
		ELOG("Wrong AAD(%d) Key(%d) Iv(%d) tag(%d)size\n",
		     param->aad_size, param->key_size, param->iv_size,
		     param->tag_size);
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	/* To meet HSM AAD standards*/
	if (param->aad_size > 0u) {
		memset(hsm_dma_buf->descVir, 0, (TCC_HSM_DESC_BUF_SIZE / 4u));
		aad = (uint8_t *)hsm_dma_buf->descVir;
		obj_id = (param->obj_id & 0x00FFFFFFu);
		if ((obj_id == OID_AES_CCM_128) ||
		    (obj_id == OID_AES_CCM_192) ||
		    (obj_id == OID_AES_CCM_256)) {
			aad[idx] = 0u;
			idx++;
			aad[idx] = (uint8_t)param->aad_size;
			idx++;
		}
		memcpy(&aad[idx], param->aad, param->aad_size);
		idx += param->aad_size;

		descPhy_size = (((idx + 15u) / 16u) * 16u);
	}

	if (param->dma == HSM_DMA) {
		ret = tcc_hsm_cmd_run_aes(MBOX_DEV_HSM, req, param,
					  hsm_dma_buf->descPhy,
					  descPhy_size);
		if (ret != TCCHSM_SUCCESS) {
			ELOG("tcc_hsm_cmd_run_aes fail\n");
			goto out;
		}

	} else {
		if ((param->src_size > TCC_HSM_DMA_BUF_SIZE) ||
		    (param->dst_size > TCC_HSM_DMA_BUF_SIZE)) {
			ELOG("Size(src=0x%x,dst=0x%x) should not exceed 0x%x\n",
			     param->src_size, param->dst_size,
			     TCC_HSM_DMA_BUF_SIZE);
			ret = TCCHSM_ERR_INVALID_PARAM;
			goto out;
		}
		if (copy_from_user((void *)hsm_dma_buf->srcVir,
				       (ulong *)param->src,
				       param->src_size) != TCCHSM_SUCCESS) {
			ELOG("copy_from_user failed\n");
			ret = TCCHSM_ERR_INVALID_MEMORY;
			goto out;
		}

		user_dst = param->dst;
		param->src = (ulong)hsm_dma_buf->srcPhy;
		param->dst = (ulong)hsm_dma_buf->dstPhy;
		ret = tcc_hsm_cmd_run_aes(MBOX_DEV_HSM, req, param,
					  hsm_dma_buf->descPhy,
					  descPhy_size);
		if (ret != TCCHSM_SUCCESS) {
			ELOG("tcc_hsm_cmd_run_aes fail(%d)\n", ret);
			goto out;
		}

		dma_sync_single_for_cpu(hsm_dma_buf->dev, hsm_dma_buf->dstPhy,
					param->dst_size, DMA_FROM_DEVICE);

		op_mode = (param->obj_id & 0x10000000U);
		if (((op_mode | OID_OPMODE_UPDATE) == OID_OPMODE_UPDATE) ||
		    ((op_mode | OID_OPMODE_FINAL) == OID_OPMODE_FINAL)) {
			if (copy_to_user(
				    (ulong *)user_dst, (void *)hsm_dma_buf->dstVir,
				    param->dst_size) != TCCHSM_SUCCESS) {
				ELOG("copy_to_user failed\n");
				ret = TCCHSM_ERR_INVALID_MEMORY;
				goto out;
			}
		}
	}
out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_run_aes(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_aes_param param;
	uint32_t req = 0;
	uint32_t ret = TCCHSM_ERR;

	if (cmd == HSM_RUN_AES_CMD) {
		req = REQ_HSM_RUN_AES;
	} else if (cmd == HSM_RUN_SM4_CMD) {
		req = REQ_HSM_RUN_SM4;
	} else {
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	if (copy_from_user((void *)&param, (ulong *)arg, (uint32_t)sizeof(param)) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	ret = tcc_hsm_run_aes(req, &param);

	if(ret != TCCHSM_SUCCESS){
		goto out;
	}

	if (copy_to_user((ulong *)arg, (void *)&param,
		     (uint32_t)sizeof(param)) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

out:
	return ret;
}

static uint32_t tcc_hsm_run_aes_by_kt(uint32_t req, struct tcc_hsm_ioctl_aes_by_kt_param *param)
{
	ulong user_dst;
	uint32_t op_mode = 0;
	uint32_t obj_id = 0;
	uint32_t idx = 0;
	uint32_t descPhy_size = 0;
	uint8_t *aad = NULL;
	uint32_t ret = TCCHSM_ERR;

	if ((param->aad_size > TCC_HSM_AES_AAD_SIZE) ||
	    (param->iv_size > TCC_HSM_AES_IV_SIZE) ||
	    (param->tag_size > TCC_HSM_AES_TAG_SIZE)) {
		ELOG("Wrong AAD(%d) Iv(%d) tag(%d)size\n", param->aad_size,
		     param->iv_size, param->tag_size);
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	/* To meet HSM AAD standards*/
	if (param->aad_size > 0u) {
		memset(hsm_dma_buf->descVir, 0, (TCC_HSM_DESC_BUF_SIZE / 4u));
		aad = (uint8_t *)hsm_dma_buf->descVir;
		obj_id = (param->obj_id & 0x00FFFFFFu);
		if ((obj_id == OID_AES_CCM_128) ||
		    (obj_id == OID_AES_CCM_192) ||
		    (obj_id == OID_AES_CCM_256)) {
			aad[idx] = 0u;
			idx++;
			aad[idx] = (uint8_t)param->aad_size;
			idx++;
		}
		memcpy(&aad[idx], param->aad, param->aad_size);
		idx += param->aad_size;

		descPhy_size = (((idx + 15u) / 16u) * 16u);
	}

	if (param->dma == HSM_DMA) {
		ret = tcc_hsm_cmd_run_aes_by_kt(MBOX_DEV_HSM, req, param,
						hsm_dma_buf->descPhy,
						descPhy_size);
		if (ret != TCCHSM_SUCCESS) {
			ELOG("tcc_hsm_cmd_run_aes_by_kt fail(%d)\n", ret);
			goto out;
		}

	} else {
		if ((param->src_size > TCC_HSM_DMA_BUF_SIZE) ||
		    (param->dst_size > TCC_HSM_DMA_BUF_SIZE)) {
			ELOG("Size(src=0x%x,dst=0x%x) should not exceed 0x%x\n",
			     param->src_size, param->dst_size,
			     TCC_HSM_DMA_BUF_SIZE);
			ret = TCCHSM_ERR_INVALID_PARAM;
			goto out;
		}
		if (copy_from_user((void *)hsm_dma_buf->srcVir, (ulong *)param->src,
				       param->src_size) != TCCHSM_SUCCESS) {
			ELOG("copy_from_user failed\n");
			ret = TCCHSM_ERR_INVALID_MEMORY;
			goto out;
		}

		user_dst = param->dst;
		param->src = (ulong)hsm_dma_buf->srcPhy;
		param->dst = (ulong)hsm_dma_buf->dstPhy;
		ret = tcc_hsm_cmd_run_aes_by_kt(MBOX_DEV_HSM, req, param,
						hsm_dma_buf->descPhy,
						descPhy_size);
		if (ret != TCCHSM_SUCCESS) {
			ELOG("tcc_hsm_cmd_run_aes_by_kt fail(%d)\n", ret);
			goto out;
		}

		dma_sync_single_for_cpu(hsm_dma_buf->dev, hsm_dma_buf->dstPhy,
					param->dst_size, DMA_FROM_DEVICE);


		op_mode = (param->obj_id & 0x10000000U);
		if (((op_mode | OID_OPMODE_UPDATE) == OID_OPMODE_UPDATE) ||
		    ((op_mode | OID_OPMODE_FINAL) == OID_OPMODE_FINAL)) {
			if (copy_to_user(
				    (ulong *)user_dst, (void *)hsm_dma_buf->dstVir,
				    param->dst_size) != TCCHSM_SUCCESS) {
				ELOG("copy_to_user failed\n");
				ret = TCCHSM_ERR_INVALID_MEMORY;
				goto out;
			}
		}
	}
out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_run_aes_by_kt(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_aes_by_kt_param param = {0};
	uint32_t req = 0;
	uint32_t ret = TCCHSM_ERR;

	if (cmd == HSM_RUN_AES_BY_KT_CMD) {
		req = REQ_HSM_RUN_AES_BY_KT;
	} else if (cmd == HSM_RUN_SM4_BY_KT_CMD) {
		req = REQ_HSM_RUN_SM4_BY_KT;
	} else {
		ELOG("Invalid CMD\n");
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	if (copy_from_user((void *)&param, (ulong *)arg, (uint32_t)sizeof(param)) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	ret = tcc_hsm_run_aes_by_kt(req, &param);

	if(ret != TCCHSM_SUCCESS) {
		goto out;
	}

	if (copy_to_user((ulong *)arg, (void *)&param,
			     (uint32_t)sizeof(param)) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

out:
	return ret;
}

static uint32_t tcc_hsm_run_gen_mac(uint32_t req, struct tcc_hsm_ioctl_mac_param *param)
{
	uint32_t op_mode = 0;
	uint32_t ret = TCCHSM_ERR;

	if (param->dma == HSM_DMA) {
		ret = tcc_hsm_cmd_gen_mac(MBOX_DEV_HSM, req, param);
	} else {
		op_mode = (param->obj_id & 0x10000000U);
		if ((op_mode | OID_OPMODE_UPDATE) == OID_OPMODE_UPDATE) {
			if (param->src_size > TCC_HSM_DMA_BUF_SIZE) {
				ELOG("The srcSize(0x%x) should not exceed 0x%x bytes\n",
				     param->src_size, TCC_HSM_DMA_BUF_SIZE);
				ret = TCCHSM_ERR_INVALID_PARAM;
				goto out;
			}

			if (copy_from_user((void *)hsm_dma_buf->srcVir,
					       (ulong *)param->src, param->src_size) !=
			    TCCHSM_SUCCESS) {
				ELOG("copy_from_user failed\n");
				ret = TCCHSM_ERR_INVALID_MEMORY;
				goto out;
			}
		}
		param->src = (ulong)hsm_dma_buf->srcPhy;
		ret = tcc_hsm_cmd_gen_mac(MBOX_DEV_HSM, req, param);
	}

	if (ret != TCCHSM_SUCCESS) {
		ELOG("tcc_hsm_cmd_gen_mac fail(%d)\n", ret);
		goto out;
	}

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_gen_mac(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_mac_param param;
	uint32_t req = 0;
	uint32_t ret = TCCHSM_ERR;

	if (cmd == HSM_VERIFY_CMAC_CMD) {
		req = REQ_HSM_VERIFY_CMAC;
	} else if (cmd == HSM_GEN_CMAC_CMD) {
		req = REQ_HSM_GEN_CMAC;
	} else if (cmd == HSM_GEN_HMAC_CMD) {
		req = REQ_HSM_GEN_HMAC;
	} else if (cmd == HSM_GEN_SM3_HMAC_CMD) {
		req = REQ_HSM_GEN_SM3_HMAC;
	} else {
		ELOG("Invalid CMD\n");
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	if (copy_from_user((void *)&param, (ulong *)arg, (uint32_t)sizeof(param)) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	if ((param.mac_size > TCC_HSM_MAC_MSG_SIZE) ||
	    (param.key_size > TCC_HSM_MAC_KEY_SIZE)) {
		ELOG("The macSize(0x%x) or keySize(0x%x) should not exceed 0x%x bytes\n",
		     param.mac_size, param.key_size, TCC_HSM_MAC_MSG_SIZE);
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

			/* To run on previous version */
	if (param.obj_id == 0u) {
		if((cmd == HSM_VERIFY_CMAC_CMD) || (cmd == HSM_GEN_CMAC_CMD)) {
			param.obj_id = (OID_AES_CMAC_128 | OID_AES_ENCRYPT | OID_OPMODE_SINGLECALL);
		}
	}

	ret = tcc_hsm_run_gen_mac(req, &param);
	if(ret != TCCHSM_SUCCESS) {
		goto out;
	}

	if (copy_to_user((ulong *)arg, (void *)&param, (uint32_t)sizeof(param)) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

out:
	return ret;
}

static uint32_t tcc_hsm_run_gen_mac_by_kt(uint32_t req, struct tcc_hsm_ioctl_mac_by_kt_param *param)
{
	uint32_t op_mode = 0;
	uint32_t ret = TCCHSM_ERR;

	if (param->dma == HSM_DMA) {
		ret = tcc_hsm_cmd_gen_mac_by_kt(MBOX_DEV_HSM, req, param);
	} else {
		op_mode = (param->obj_id & 0x10000000U);
		if ((op_mode | OID_OPMODE_UPDATE) == OID_OPMODE_UPDATE) {
			if (param->src_size > TCC_HSM_DMA_BUF_SIZE) {
				ELOG("The srcSize(0x%x) should not exceed 0x%x bytes\n",
				     param->src_size, TCC_HSM_DMA_BUF_SIZE);
				ret = TCCHSM_ERR_INVALID_PARAM;
				goto out;
			}
			if (copy_from_user((void *)hsm_dma_buf->srcVir,
					       (ulong *)param->src, param->src_size) !=
			    TCCHSM_SUCCESS) {
				ELOG("copy_from_user failed\n");
				ret = TCCHSM_ERR_INVALID_MEMORY;
				goto out;
			}
		}
		param->src = (ulong)hsm_dma_buf->srcPhy;
		ret = tcc_hsm_cmd_gen_mac_by_kt(MBOX_DEV_HSM, req, param);
	}

	if (ret != TCCHSM_SUCCESS) {
		ELOG("tcc_hsm_cmd_gen_mac_by_kt fail(%d)\n", ret);
		goto out;
	}

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_gen_mac_by_kt(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_mac_by_kt_param param;
	uint32_t req = 0;
	uint32_t ret = TCCHSM_ERR;

	if (cmd == HSM_VERIFY_CMAC_BY_KT_CMD) {
		req = REQ_HSM_VERIFY_CMAC_BY_KT;
	} else if (cmd == HSM_GEN_CMAC_BY_KT_CMD) {
		req = REQ_HSM_GEN_CMAC_BY_KT;
	} else if (cmd == HSM_GEN_HMAC_BY_KT_CMD) {
		req = REQ_HSM_GEN_HMAC_BY_KT;
	} else if (cmd == HSM_GEN_SM3_HMAC_BY_KT_CMD) {
		req = REQ_HSM_GEN_SM3_HMAC_BY_KT;
	} else {
		ELOG("Invalid CMD\n");
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	if (copy_from_user((void *)&param, (ulong *)arg, (uint32_t)sizeof(param)) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	if (param.mac_size > TCC_HSM_MAC_MSG_SIZE) {
		ELOG("Wrong mac(%d) size\n", param.mac_size);
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	/* To run on previous version */
	if (param.obj_id == 0u) {
		if((cmd == HSM_VERIFY_CMAC_BY_KT_CMD) || (cmd == HSM_GEN_CMAC_BY_KT_CMD)) {
			param.obj_id = (OID_AES_CMAC_128 | OID_AES_ENCRYPT | OID_OPMODE_SINGLECALL);
		}
	}

	ret = tcc_hsm_run_gen_mac_by_kt(req, &param);
	if(ret != TCCHSM_SUCCESS){
		goto out;
	}

	if (copy_to_user((ulong *)arg, (void *)&param, (uint32_t)sizeof(param)) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_gen_hash(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_hash_param param = { 0 };
	uint32_t req = 0;
	uint32_t op_mode = 0;
	uint32_t ret = TCCHSM_ERR;

	if (cmd == HSM_GEN_SHA_CMD) {
		req = REQ_HSM_GEN_SHA;
	} else if (cmd == HSM_GEN_SM3_CMD) {
		req = REQ_HSM_GEN_SM3;
	} else {
		ELOG("Invalid CMD\n");
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	if (copy_from_user((void *)&param, (ulong *)arg, (uint32_t)sizeof(param)) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	if (param.dma == HSM_DMA) {
		ret = tcc_hsm_cmd_gen_hash(MBOX_DEV_HSM, req, &param);
	} else {
		op_mode = (param.obj_id & 0x10000000U);
		if ((op_mode | OID_OPMODE_UPDATE) == OID_OPMODE_UPDATE) {
			if (param.src_size > TCC_HSM_DMA_BUF_SIZE) {
				ELOG("The srcSize(0x%x) should not exceed 0x%x bytes\n",
				     param.src_size, TCC_HSM_DMA_BUF_SIZE);
				ret = TCCHSM_ERR_INVALID_PARAM;
				goto out;
			}
			if (copy_from_user((void *)hsm_dma_buf->srcVir,
					       (ulong *)param.src, param.src_size) !=
			    TCCHSM_SUCCESS) {
				ELOG("copy_from_user failed\n");
				ret = TCCHSM_ERR_INVALID_MEMORY;
				goto out;
			}
		}
		param.src = (ulong)hsm_dma_buf->srcPhy;
		ret = tcc_hsm_cmd_gen_hash(MBOX_DEV_HSM, req, &param);
	}

	if (ret != TCCHSM_SUCCESS) {
		ELOG("tcc_hsm_ioctl_gen_hash fail(%d)\n", ret);
		goto out;
	}

	if (copy_to_user((ulong *)arg, (void *)&param, (uint32_t)sizeof(param)) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_run_ecdsa(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_ecdsa_param param;
	uint32_t req = 0;
	uint32_t ret = TCCHSM_ERR;

	if (copy_from_user((void *)&param, (ulong *)arg, (uint32_t)sizeof(param)) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	if ((param.key_size > (TCC_HSM_ECDSA_MAX_KEY_SIZE * 2u)) ||
	    (param.digest_size > TCC_HSM_ECDSA_DIGEST_SIZE) ||
	    (param.sig_size > TCC_HSM_ECDSA_SIGN_SIZE)) {
		ELOG("Wrong Key(%d) dig(%d) sig(%d) size\n", param.key_size,
		     param.digest_size, param.sig_size);
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	switch (cmd) {
	case HSM_RUN_ECDSA_SIGN_CMD:
		req = REQ_HSM_RUN_ECDSA_SIGN;
		ret = tcc_hsm_cmd_run_ecdsa(MBOX_DEV_HSM, req, &param);
		if (ret != TCCHSM_SUCCESS) {
			ELOG("tcc_hsm_cmd_run_ecdsa fail(%d)\n", ret);
			break;
		}
		if (copy_to_user((ulong *)arg, (void *)&param,
				     (uint32_t)sizeof(param)) !=
		    TCCHSM_SUCCESS) {
			ELOG("copy_to_user failed\n");
			ret = TCCHSM_ERR_INVALID_MEMORY;
			break;
		}
		break;

	case HSM_RUN_ECDSA_VERIFY_CMD:
		req = REQ_HSM_RUN_ECDSA_VERIFY;
		ret = tcc_hsm_cmd_run_ecdsa(MBOX_DEV_HSM, req, &param);
		if (ret != TCCHSM_SUCCESS) {
			ELOG("tcc_hsm_cmd_run_ecdsa Err(0x%x)\n", ret);
			goto out;
		}
		break;

	default:
		ELOG("Invalid CMD\n");
		ret = TCCHSM_ERR_INVALID_PARAM;
		break;
	}

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_run_ecdsa_by_kt(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_ecdsa_by_kt_param param;
	uint32_t req = 0;
	uint32_t ret = TCCHSM_ERR;

	if (copy_from_user((void *)&param, (ulong *)arg, (uint32_t)sizeof(param)) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	if ((param.digest_size > TCC_HSM_ECDSA_DIGEST_SIZE) ||
	    (param.sig_size > TCC_HSM_ECDSA_SIGN_SIZE)) {
		ELOG("Wrong dig(%d) sig(%d) size\n", param.digest_size,
		     param.sig_size);
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	switch (cmd) {
	case HSM_RUN_ECDSA_SIGN_BY_KT_CMD:
		req = REQ_HSM_RUN_ECDSA_SIGN_BY_KT;
		ret = tcc_hsm_cmd_run_ecdsa_by_kt(MBOX_DEV_HSM, req, &param);
		if (ret != TCCHSM_SUCCESS) {
			ELOG("tcc_hsm_cmd_run_ecdsa_by_kt fail(%d)\n", ret);
			break;
		}
		if (copy_to_user((ulong *)arg, (void *)&param,
				     (uint32_t)sizeof(param)) !=
		    TCCHSM_SUCCESS) {
			ELOG("copy_to_user failed\n");
			ret = TCCHSM_ERR_INVALID_MEMORY;
			break;
		}
		break;

	case HSM_RUN_ECDSA_VERIFY_BY_KT_CMD:
		req = REQ_HSM_RUN_ECDSA_VERIFY_BY_KT;
		ret = tcc_hsm_cmd_run_ecdsa_by_kt(MBOX_DEV_HSM, req, &param);
		if (ret != TCCHSM_SUCCESS) {
			ELOG("tcc_hsm_cmd_run_ecdsa_by_kt Err(0x%x)\n", ret);
			goto out;
		}
		break;

	default:
		ELOG("Invalid CMD\n");
		break;
	}

out:
	return ret;
}


static uint32_t tcc_hsm_run_rsaes(uint32_t req, ulong arg,
						struct tcc_hsm_ioctl_rsaes_param *param, uint32_t param_size)
{
	uint32_t ret = TCCHSM_ERR;

	if (copy_from_user((void *)param, (ulong *)arg, param_size) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	if ((param->modN_size > TCC_HSM_RSA_MODN_SIZE) ||
	    (param->key_size > TCC_HSM_RSA_MODN_SIZE) ||
	    (param->src_size > TCC_HSM_RSA_MODN_SIZE) ||
	    (param->dst_size > TCC_HSM_RSA_MODN_SIZE) ||
	    (param->label_size > TCC_HSM_RSA_MODN_SIZE)) {
		ELOG("Invalid Param\n");
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	/* To handle RSA 4096 */
	memcpy((void *)hsm_dma_buf->srcVir, (const void *)param->modN,
	       param->modN_size);
	memcpy((void *)&hsm_dma_buf->srcVir[TCC_HSM_RSA_MODN_SIZE],
	       (const void *)param->rsa_key, param->key_size);
	memcpy((void *)&hsm_dma_buf->srcVir[TCC_HSM_RSA_MODN_SIZE * 2u],
	       (const void *)param->src, param->src_size);
	memcpy((void *)&hsm_dma_buf->srcVir[TCC_HSM_RSA_MODN_SIZE * 3u],
	       (const void *)param->label, param->label_size);

	ret = tcc_hsm_cmd_run_rsaes(
		MBOX_DEV_HSM, req, param, hsm_dma_buf->srcPhy,
		hsm_dma_buf->dstPhy);
	if (ret != TCCHSM_SUCCESS) {
		ELOG("tcc_hsm_cmd_run_rsaes fail(%d)\n", ret);
		goto out;
	}

	dma_sync_single_for_cpu(hsm_dma_buf->dev, hsm_dma_buf->dstPhy,
				param->dst_size, DMA_FROM_DEVICE);

	memcpy((void *)param->dst, (const void *)hsm_dma_buf->dstVir,
	       param->dst_size);

	if (copy_to_user((ulong *)arg, (void *)param, param_size) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_run_rsaes(uint32_t cmd, ulong arg)
{
	uint32_t req = 0;
	uint32_t ret = TCCHSM_ERR;
	uint32_t param_size = (uint32_t)sizeof(struct tcc_hsm_ioctl_rsaes_param);
	struct tcc_hsm_ioctl_rsaes_param *param = kmalloc(param_size, GFP_KERNEL);

	if (param == NULL) {
		ELOG("kmalloc failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	if (cmd == HSM_RUN_RSAES_PKCS_ENC_CMD) {
		req = REQ_HSM_RUN_RSAES_PKCS_ENC;
	} else if (cmd == HSM_RUN_RSAES_PKCS_DEC_CMD) {
		req = REQ_HSM_RUN_RSAES_PKCS_DEC;
	} else if (cmd == HSM_RUN_RSAES_OAEP_ENC_CMD) {
		req = REQ_HSM_RUN_RSAES_OAEP_ENC;
	} else if (cmd == HSM_RUN_RSAES_OAEP_DEC_CMD) {
		req = REQ_HSM_RUN_RSAES_OAEP_DEC;
	} else {
		ELOG("Invalid CMD\n");
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	ret = tcc_hsm_run_rsaes(req, arg, param, param_size);

out:
	if(param != NULL){
		kfree(param);
	}
	return ret;
}

static uint32_t tcc_hsm_ioctl_run_rsaes_by_kt(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_rsaes_by_kt_param param = {0};
	uint32_t param_size = 0;
	uint32_t req = 0;
	uint32_t ret = TCCHSM_ERR;

	if (cmd == HSM_RUN_RSAES_PKCS_ENC_BY_KT_CMD) {
		req = REQ_HSM_RUN_RSAES_PKCS_ENC_BY_KT;
	} else if (cmd == HSM_RUN_RSAES_PKCS_DEC_BY_KT_CMD) {
		req = REQ_HSM_RUN_RSAES_PKCS_DEC_BY_KT;
	} else if (cmd == HSM_RUN_RSAES_OAEP_ENC_BY_KT_CMD) {
		req = REQ_HSM_RUN_RSAES_OAEP_ENC_BY_KT;
	} else if (cmd == HSM_RUN_RSAES_OAEP_DEC_BY_KT_CMD) {
		req = REQ_HSM_RUN_RSAES_OAEP_DEC_BY_KT;
	} else {
		ELOG("Invalid CMD\n");
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	param_size = (uint32_t)sizeof(struct tcc_hsm_ioctl_rsaes_by_kt_param);

	if (copy_from_user((void *)&param, (ulong *)arg, param_size) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	if ((param.src_size > TCC_HSM_RSA_MODN_SIZE) ||
	    (param.dst_size > TCC_HSM_RSA_MODN_SIZE) ||
	    (param.label_size > TCC_HSM_RSA_MODN_SIZE)) {
		ELOG("Invalid Param\n");
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	/* To handle RSA 4096 */
	memcpy((void *)hsm_dma_buf->srcVir, (const void *)param.src,
	       param.src_size);
	memcpy((void *)&hsm_dma_buf->srcVir[TCC_HSM_RSA_MODN_SIZE],
	       (const void *)param.label, param.label_size);

	ret = tcc_hsm_cmd_run_rsaes_by_kt(
		MBOX_DEV_HSM, req, &param, hsm_dma_buf->srcPhy,
		hsm_dma_buf->dstPhy);
	if (ret != TCCHSM_SUCCESS) {
		ELOG("tcc_hsm_cmd_run_rsaes_by_kt fail(%d)\n", ret);
		goto out;
	}

	dma_sync_single_for_cpu(hsm_dma_buf->dev, hsm_dma_buf->dstPhy,
				param.dst_size, DMA_FROM_DEVICE);

	memcpy((void *)param.dst, (const void *)hsm_dma_buf->dstVir,
	       param.dst_size);

	if (copy_to_user((ulong *)arg, (void *)&param, param_size) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_run_rsassa(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_rsassa_param param = {0};
	uint32_t param_size = 0;
	uint32_t req = 0;
	uint32_t ret = TCCHSM_ERR;

	if (cmd == HSM_RUN_RSASSA_PKCS_SIGN_CMD) {
		req = REQ_HSM_RUN_RSASSA_PKCS_SIGN;
	} else if (cmd == HSM_RUN_RSASSA_PKCS_VERIFY_CMD) {
		req = REQ_HSM_RUN_RSASSA_PKCS_VERIFY;
	} else if (cmd == HSM_RUN_RSASSA_PSS_SIGN_CMD) {
		req = REQ_HSM_RUN_RSASSA_PSS_SIGN;
	} else if (cmd == HSM_RUN_RSASSA_PSS_VERIFY_CMD) {
		req = REQ_HSM_RUN_RSASSA_PSS_VERIFY;
	} else {
		ELOG("Invalid CMD\n");
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	param_size = (uint32_t)sizeof(struct tcc_hsm_ioctl_rsassa_param);

	if (copy_from_user((void *)&param, (ulong *)arg, param_size) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	if ((param.modN_size > TCC_HSM_RSA_MODN_SIZE) ||
	    (param.key_size > TCC_HSM_RSA_MODN_SIZE) ||
	    (param.digest_size > TCC_HSM_RSA_DIG_SIZE) ||
	    (param.sig_size > TCC_HSM_RSA_SIG_SIZE)) {
		ELOG("Invalid Param(%d)\n", param.digest_size);
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	/* To handle RSA 4096 */
	memcpy((void *)hsm_dma_buf->srcVir, (const void *)param.modN,
	       param.modN_size);
	memcpy((void *)&hsm_dma_buf->srcVir[TCC_HSM_RSA_MODN_SIZE],
	       (const void *)param.rsa_key, param.key_size);
	memcpy((void *)&hsm_dma_buf->srcVir[TCC_HSM_RSA_MODN_SIZE * 2u],
	       (const void *)param.digest, param.digest_size);

	if ((req == REQ_HSM_RUN_RSASSA_PKCS_VERIFY) ||
	    (req == REQ_HSM_RUN_RSASSA_PSS_VERIFY)) {
		memcpy((void *)hsm_dma_buf->dstVir, (const void *)param.sig,
		       param.sig_size);
	}

	ret = tcc_hsm_cmd_run_rsassa(
		MBOX_DEV_HSM, req, &param,
		hsm_dma_buf->srcPhy,
		hsm_dma_buf->dstPhy);
	if (ret != TCCHSM_SUCCESS) {
		ELOG("tcc_hsm_cmd_run_rsassa fail(%d)\n", ret);
		goto out;
	}

	if ((req == REQ_HSM_RUN_RSASSA_PKCS_SIGN) ||
	    (req == REQ_HSM_RUN_RSASSA_PSS_SIGN)) {
		dma_sync_single_for_cpu(hsm_dma_buf->dev, hsm_dma_buf->dstPhy,
					param.sig_size, DMA_FROM_DEVICE);

		memcpy((void *)param.sig, (const void *)hsm_dma_buf->dstVir,
		       param.sig_size);
	}

	if (copy_to_user((ulong *)arg, (void *)&param, param_size) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_run_rsassa_by_kt(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_rsassa_by_kt_param param = {0};
	uint32_t param_size = 0;
	uint32_t req = 0;
	uint32_t ret = TCCHSM_ERR;

	if (cmd == HSM_RUN_RSASSA_PKCS_SIGN_BY_KT_CMD) {
		req = REQ_HSM_RUN_RSASSA_PKCS_SIGN_BY_KT;
	} else if (cmd == HSM_RUN_RSASSA_PKCS_VERIFY_BY_KT_CMD) {
		req = REQ_HSM_RUN_RSASSA_PKCS_VERIFY_BY_KT;
	} else if (cmd == HSM_RUN_RSASSA_PSS_SIGN_BY_KT_CMD) {
		req = REQ_HSM_RUN_RSASSA_PSS_SIGN_BY_KT;
	} else if (cmd == HSM_RUN_RSASSA_PSS_VERIFY_BY_KT_CMD) {
		req = REQ_HSM_RUN_RSASSA_PSS_VERIFY_BY_KT;
	} else {
		ELOG("Invalid CMD\n");
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	param_size = (uint32_t)sizeof(struct tcc_hsm_ioctl_rsassa_by_kt_param);

	if (copy_from_user((void *)&param, (ulong *)arg, param_size) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	if ((param.digest_size > TCC_HSM_RSA_DIG_SIZE) ||
	    (param.sig_size > TCC_HSM_RSA_SIG_SIZE)) {
		ELOG("Invalid Param\n");
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	/* To handle RSA 4096 */
	memcpy((void *)hsm_dma_buf->srcVir, (const void *)param.digest,
	       param.digest_size);

	if ((req == REQ_HSM_RUN_RSASSA_PKCS_VERIFY_BY_KT) ||
	    (req == REQ_HSM_RUN_RSASSA_PSS_VERIFY_BY_KT)) {
		memcpy((void *)hsm_dma_buf->dstVir, (const void *)param.sig,
		       param.sig_size);
	}

	ret = tcc_hsm_cmd_run_rsassa_by_kt(MBOX_DEV_HSM, req, &param,
					   hsm_dma_buf->srcPhy,
					   hsm_dma_buf->dstPhy);
	if (ret != TCCHSM_SUCCESS) {
		ELOG("tcc_hsm_cmd_run_rsassa_by_kt fail(%d)\n", ret);
		goto out;
	}

	if ((req == REQ_HSM_RUN_RSASSA_PKCS_SIGN_BY_KT) ||
	    (req == REQ_HSM_RUN_RSASSA_PSS_SIGN_BY_KT)) {
		dma_sync_single_for_cpu(hsm_dma_buf->dev, hsm_dma_buf->dstPhy,
					param.sig_size, DMA_FROM_DEVICE);

		memcpy((void *)param.sig, (const void *)hsm_dma_buf->dstVir,
		       param.sig_size);
	}

	if (copy_to_user((ulong *)arg, (void *)&param, param_size) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_write(uint32_t cmd, ulong arg)
{
	struct tcc_hsm_ioctl_write_param param;
	uint32_t req = REQ_HSM_WRITE_OTP;
	uint32_t ret = TCCHSM_ERR;

	if (cmd == HSM_WRITE_OTP_CMD) {
		req = REQ_HSM_WRITE_OTP;
	} else if (cmd == HSM_WRITE_SNOR_CMD) {
		req = REQ_HSM_WRITE_SNOR;
	} else {
		ELOG("Invalid CMD\n");
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	if (copy_from_user((void *)&param, (ulong *)arg, (uint32_t)sizeof(param)) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	if (param.data_size > TCC_HSM_DMA_BUF_SIZE) {
		ELOG("The data_size(0x%x) should not exceed 0x%x bytes\n",
		     param.data_size, TCC_HSM_DMA_BUF_SIZE);
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	if (param.dma == HSM_DMA) {
		if(copy_from_user((void *)hsm_dma_buf->srcVir, (ulong *)param.data,
				   param.data_size) != TCCHSM_SUCCESS){
			ELOG("copy_from_user failed\n");
			ret = TCCHSM_ERR_INVALID_MEMORY;
			goto out;
		}
		param.data = (ulong)hsm_dma_buf->srcPhy;

		ret = tcc_hsm_cmd_write(MBOX_DEV_HSM, req, &param);
		dma_sync_single_for_cpu(hsm_dma_buf->dev, hsm_dma_buf->srcPhy,
					param.data_size, DMA_FROM_DEVICE);
	} else {
		if (copy_from_user((void *)hsm_dma_buf->srcVir, (ulong *)param.data,
				       param.data_size) != TCCHSM_SUCCESS) {
			ELOG("copy_from_user failed\n");
			ret = TCCHSM_ERR_INVALID_MEMORY;
			goto out;
		}

		param.data = (ulong)hsm_dma_buf->srcVir;
		ret = tcc_hsm_cmd_write(MBOX_DEV_HSM, req, &param);
	}

	if (ret != TCCHSM_SUCCESS) {
		ELOG("tcc_hsm_cmd_write failed\n");
		goto out;
	}

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_get_fw_version(ulong arg)
{
	struct tcc_hsm_ioctl_version_param param;
	uint32_t req = REQ_HSM_GET_VER;
	uint32_t ret = TCCHSM_ERR;

	ret = tcc_hsm_cmd_get_version(MBOX_DEV_HSM, req, &param);
	if (ret != TCCHSM_SUCCESS) {
		ELOG("failed to get version\n");
		goto out;
	}

	if (copy_to_user((ulong *)arg, (void *)&param, (uint32_t)sizeof(param)) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_get_driver_version(ulong arg)
{
	struct tcc_hsm_ioctl_version_param param;
	uint32_t ret = TCCHSM_ERR;

	param.x = HSM_DRIVER_VER_X;
	param.y = HSM_DRIVER_VER_Y;
	param.z = HSM_DRIVER_VER_Z;
	if (copy_to_user((ulong *)arg, (void *)&param, (uint32_t)sizeof(param)) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	} else {
		ret = TCCHSM_SUCCESS;
	}

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_run_ecdh_phaseI(ulong arg)
{
	struct tcc_hsm_ioctl_ecdh_key_param param;
	uint32_t req = REQ_HSM_RUN_ECDH_PHASE_I;
	uint32_t ret = TCCHSM_ERR;

	if (copy_from_user((void *)&param, (ulong *)arg, (uint32_t)sizeof(param)) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	ret = tcc_hsm_cmd_run_ecdh_phaseI(MBOX_DEV_HSM, req, &param);
	if (ret != TCCHSM_SUCCESS) {
		ELOG("failed to run ecdh phase I\n");
		goto out;
	}

	if (copy_to_user((ulong *)arg, (void *)&param, (uint32_t)sizeof(param)) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_get_rng(ulong arg)
{
	struct tcc_hsm_ioctl_rng_param param;
	uint32_t req = REQ_HSM_GET_RNG;
	uint32_t ret = TCCHSM_ERR;

	if (copy_from_user((void *)&param, (ulong *)arg, (uint32_t)sizeof(param)) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	if (param.rng_size > TCC_HSM_DMA_BUF_SIZE) {
		ELOG("The rngSize(0x%x) should not exceed 0x%x bytes\n",
		     param.rng_size, TCC_HSM_DMA_BUF_SIZE);
		ret = TCCHSM_ERR_INVALID_PARAM;
		goto out;
	}

	ret = tcc_hsm_cmd_get_rand(MBOX_DEV_HSM, req,
				   hsm_dma_buf->dstPhy,
				   param.rng_size);

	dma_sync_single_for_cpu(hsm_dma_buf->dev, hsm_dma_buf->dstPhy,
				param.rng_size, DMA_FROM_DEVICE);

	if (ret != TCCHSM_SUCCESS) {
		ELOG("failed to get random number\n");
		goto out;
	}
	if (copy_to_user((ulong *)param.rng, (void *)hsm_dma_buf->dstVir,
			     param.rng_size) != TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

out:
	return ret;
}

/* Set and Write */
static uint32_t tcc_hsm_set_write_cmd(uint32_t cmd, ulong arg){
	uint32_t ret = TCCHSM_ERR;

	switch (cmd) {
	case HSM_SET_KEY_FROM_OTP_CMD:
	case HSM_SET_KEY_FROM_SNOR_CMD:
		ret = tcc_hsm_ioctl_set_key(cmd, arg);
		break;

	case HSM_SET_MODN_FROM_OTP_CMD:
	case HSM_SET_MODN_FROM_SNOR_CMD:
		ret = tcc_hsm_ioctl_set_modn(cmd, arg);
		break;

	case HSM_WRITE_OTP_CMD:
	case HSM_WRITE_SNOR_CMD:
		ret = tcc_hsm_ioctl_write(cmd, arg);
		break;
	default:
		ELOG("unknown command(0x%x)\n", cmd);
		ret = TCCHSM_ERR_INVALID_PARAM;
		break;
	}

	return ret;
}
/*symmentric*/
static uint32_t tcc_hsm_symmetric_cmd(uint32_t cmd, ulong arg){
	uint32_t ret = TCCHSM_ERR;

	switch (cmd) {
	case HSM_RUN_AES_CMD:
	case HSM_RUN_SM4_CMD:
		ret = tcc_hsm_ioctl_run_aes(cmd, arg);
		break;

	case HSM_RUN_AES_BY_KT_CMD:
	case HSM_RUN_SM4_BY_KT_CMD:
		ret = tcc_hsm_ioctl_run_aes_by_kt(cmd, arg);
		break;

	default:
		ELOG("unknown command(0x%x)\n", cmd);
		ret = TCCHSM_ERR_INVALID_PARAM;
		break;
	}
	return ret;
}
/* mac hash */
static uint32_t tcc_hsm_mac_hash_cmd(uint32_t cmd, ulong arg){
	uint32_t ret = TCCHSM_ERR;

	switch (cmd) {
	case HSM_VERIFY_CMAC_CMD:
	case HSM_GEN_CMAC_CMD:
	case HSM_GEN_HMAC_CMD:
	case HSM_GEN_SM3_HMAC_CMD:
		ret = tcc_hsm_ioctl_gen_mac(cmd, arg);
		break;

	case HSM_VERIFY_CMAC_BY_KT_CMD:
	case HSM_GEN_CMAC_BY_KT_CMD:
	case HSM_GEN_HMAC_BY_KT_CMD:
	case HSM_GEN_SM3_HMAC_BY_KT_CMD:
		ret = tcc_hsm_ioctl_gen_mac_by_kt(cmd, arg);
		break;

	case HSM_GEN_SHA_CMD:
	case HSM_GEN_SM3_CMD:
		ret = tcc_hsm_ioctl_gen_hash(cmd, arg);
		break;

	default:
		ELOG("unknown command(0x%x)\n", cmd);
		ret = TCCHSM_ERR_INVALID_PARAM;
		break;
	}
	return ret;
}

/* Asymmetric */
static uint32_t tcc_hsm_ecc_cmd(uint32_t cmd, ulong arg){
	uint32_t ret = TCCHSM_ERR;

	switch (cmd) {
	case HSM_RUN_ECDSA_SIGN_CMD:
	case HSM_RUN_ECDSA_VERIFY_CMD:
		ret = tcc_hsm_ioctl_run_ecdsa(cmd, arg);
		break;

	case HSM_RUN_ECDSA_SIGN_BY_KT_CMD:
	case HSM_RUN_ECDSA_VERIFY_BY_KT_CMD:
		ret = tcc_hsm_ioctl_run_ecdsa_by_kt(cmd, arg);
		break;

	case HSM_RUN_ECDH_PHASE_I_CMD:
		ret = tcc_hsm_ioctl_run_ecdh_phaseI(arg);
		break;

	default:
		ELOG("unknown command(0x%x)\n", cmd);
		ret = TCCHSM_ERR_INVALID_PARAM;
		break;
	}

	return ret;
}

static uint32_t tcc_hsm_rsa_cmd(uint32_t cmd, ulong arg)
{
	uint32_t ret = TCCHSM_ERR;

	switch (cmd) {
	case HSM_RUN_RSAES_PKCS_ENC_CMD:
	case HSM_RUN_RSAES_PKCS_DEC_CMD:
	case HSM_RUN_RSAES_OAEP_ENC_CMD:
	case HSM_RUN_RSAES_OAEP_DEC_CMD:
		ret = tcc_hsm_ioctl_run_rsaes(cmd, arg);
		break;
	case HSM_RUN_RSAES_PKCS_ENC_BY_KT_CMD:
	case HSM_RUN_RSAES_PKCS_DEC_BY_KT_CMD:
	case HSM_RUN_RSAES_OAEP_ENC_BY_KT_CMD:
	case HSM_RUN_RSAES_OAEP_DEC_BY_KT_CMD:
		ret = tcc_hsm_ioctl_run_rsaes_by_kt(cmd, arg);
		break;
	case HSM_RUN_RSASSA_PKCS_SIGN_CMD:
	case HSM_RUN_RSASSA_PKCS_VERIFY_CMD:
	case HSM_RUN_RSASSA_PSS_SIGN_CMD:
	case HSM_RUN_RSASSA_PSS_VERIFY_CMD:
		ret = tcc_hsm_ioctl_run_rsassa(cmd, arg);
		break;

	case HSM_RUN_RSASSA_PKCS_SIGN_BY_KT_CMD:
	case HSM_RUN_RSASSA_PKCS_VERIFY_BY_KT_CMD:
	case HSM_RUN_RSASSA_PSS_SIGN_BY_KT_CMD:
	case HSM_RUN_RSASSA_PSS_VERIFY_BY_KT_CMD:
		ret = tcc_hsm_ioctl_run_rsassa_by_kt(cmd, arg);
		break;

	default:
		ELOG("unknown command(0x%x)\n", cmd);
		ret = TCCHSM_ERR_INVALID_PARAM;
		break;
	}

	return ret;
}

/* Get Random number and version number */
static uint32_t tcc_hsm_number_cmd(uint32_t cmd, ulong arg){
	uint32_t ret = TCCHSM_ERR;

	switch (cmd) {
	case HSM_GET_FW_VER_CMD:
		ret = tcc_hsm_ioctl_get_fw_version(arg);
		break;

	case HSM_GET_DRIVER_VER_CMD:
		ret = tcc_hsm_ioctl_get_driver_version(arg);
		break;

	case HSM_GET_RNG_CMD:
		ret = tcc_hsm_ioctl_get_rng(arg);
		break;
	default:
		ELOG("unknown command(0x%x)\n", cmd);
		ret = TCCHSM_ERR_INVALID_PARAM;
		break;
	}
	return ret;
}

static uint32_t tcc_hsm_cmd_parse(uint32_t cmd, ulong arg) {
	uint32_t ret = TCCHSM_ERR;

	switch (cmd) {
	// set and write key and modn
	case HSM_SET_KEY_FROM_OTP_CMD:
	case HSM_SET_KEY_FROM_SNOR_CMD:
	case HSM_SET_MODN_FROM_OTP_CMD:
	case HSM_SET_MODN_FROM_SNOR_CMD:
	case HSM_WRITE_OTP_CMD:
	case HSM_WRITE_SNOR_CMD:
		ret = tcc_hsm_set_write_cmd(cmd, arg);
		break;
	// Symmetric
	case HSM_RUN_AES_CMD:
	case HSM_RUN_SM4_CMD:
	case HSM_RUN_AES_BY_KT_CMD:
	case HSM_RUN_SM4_BY_KT_CMD:
		ret = tcc_hsm_symmetric_cmd(cmd, arg);
		break;
	//mac hash
	case HSM_VERIFY_CMAC_CMD:
	case HSM_GEN_CMAC_CMD:
	case HSM_GEN_HMAC_CMD:
	case HSM_GEN_SM3_HMAC_CMD:
	case HSM_VERIFY_CMAC_BY_KT_CMD:
	case HSM_GEN_CMAC_BY_KT_CMD:
	case HSM_GEN_HMAC_BY_KT_CMD:
	case HSM_GEN_SM3_HMAC_BY_KT_CMD:
	case HSM_GEN_SHA_CMD:
	case HSM_GEN_SM3_CMD:
		ret = tcc_hsm_mac_hash_cmd(cmd, arg);
		break;

	//ECC
	case HSM_RUN_ECDSA_SIGN_CMD:
	case HSM_RUN_ECDSA_VERIFY_CMD:
	case HSM_RUN_ECDSA_SIGN_BY_KT_CMD:
	case HSM_RUN_ECDSA_VERIFY_BY_KT_CMD:
	case HSM_RUN_ECDH_PHASE_I_CMD:
		ret = tcc_hsm_ecc_cmd(cmd, arg);
		break;
	//RSA
	case HSM_RUN_RSAES_PKCS_ENC_CMD:
	case HSM_RUN_RSAES_PKCS_DEC_CMD:
	case HSM_RUN_RSAES_OAEP_ENC_CMD:
	case HSM_RUN_RSAES_OAEP_DEC_CMD:
	case HSM_RUN_RSAES_PKCS_ENC_BY_KT_CMD:
	case HSM_RUN_RSAES_PKCS_DEC_BY_KT_CMD:
	case HSM_RUN_RSAES_OAEP_ENC_BY_KT_CMD:
	case HSM_RUN_RSAES_OAEP_DEC_BY_KT_CMD:
	case HSM_RUN_RSASSA_PKCS_SIGN_CMD:
	case HSM_RUN_RSASSA_PKCS_VERIFY_CMD:
	case HSM_RUN_RSASSA_PSS_SIGN_CMD:
	case HSM_RUN_RSASSA_PSS_VERIFY_CMD:
	case HSM_RUN_RSASSA_PKCS_SIGN_BY_KT_CMD:
	case HSM_RUN_RSASSA_PKCS_VERIFY_BY_KT_CMD:
	case HSM_RUN_RSASSA_PSS_SIGN_BY_KT_CMD:
	case HSM_RUN_RSASSA_PSS_VERIFY_BY_KT_CMD:
		ret = tcc_hsm_rsa_cmd(cmd, arg);
		break;
	//version and rng
	case HSM_GET_FW_VER_CMD:
	case HSM_GET_DRIVER_VER_CMD:
	case HSM_GET_RNG_CMD:
		ret = tcc_hsm_number_cmd(cmd, arg);
		break;

	case HSM_RUN_ECDH_PHASE_II_CMD:
	case HSM_RUN_ECDH_PUBKEY_COMPUTE_CMD:
		ELOG("Not yet supported!\n");
		ret = TCCHSM_ERR_INVALID_PARAM;
		break;

	default:
		ELOG("unknown command(0x%x)\n", cmd);
		ret = TCCHSM_ERR_INVALID_PARAM;
		break;
	}

	return ret;
}

static long tcc_hsm_ioctl(struct file *hsm_file, uint32_t cmd, ulong arg)
{
	uint32_t ret = TCCHSM_ERR;

	/* gVerParam is initialized in tcc_hsm_open
	 * Do not run ioctl cmd If HSM F/W version is lower than the required
	 * version */
	if ((gVerParam.x != HSM_REQUIRED_FW_VER_X) ||
	    (gVerParam.y < HSM_REQUIRED_FW_VER_Y)) {
		ELOG("HSM FW verison(%d.%d.%d) must be higher than equal to %d.%d.%d\n",
		     gVerParam.x, gVerParam.y, gVerParam.z,
		     HSM_REQUIRED_FW_VER_X, HSM_REQUIRED_FW_VER_Y,
		     HSM_REQUIRED_FW_VER_Z);
		ret = TCCHSM_ERR_VERSION_MISMATCH;
		goto out;
	}

	mutex_lock(&tcc_hsm_mutex);
	ret =  tcc_hsm_cmd_parse(cmd, arg);
	mutex_unlock(&tcc_hsm_mutex);

out:
	return (long)ret;
}

static int32_t tcc_hsm_open(struct inode *hsm_inode, struct file *hsm_filp)
{
	int32_t ret = 0;

	/*Get HSM F/W version to check version when running ioctl  */
	if (tcc_hsm_cmd_get_version(MBOX_DEV_HSM, REQ_HSM_GET_VER,
				    &gVerParam) != TCCHSM_SUCCESS) {
		ELOG("failed to get version\n");
		ret = -1;
		goto out;
	}

out:
	return ret;
}

static int32_t tcc_hsm_release(struct inode *hsm_inode, struct file *hsm_file)
{
	return 0;
}

static const struct file_operations tcc_hsm_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = tcc_hsm_ioctl,
	.compat_ioctl = tcc_hsm_ioctl,
	.open = tcc_hsm_open,
	.release = tcc_hsm_release,
};

static struct miscdevice tcc_hsm_miscdevice = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = TCCHSM_DEVICE_NAME,
	.fops = &tcc_hsm_fops,
};
#if (0)
static int proc_hsminfo_show(struct seq_file *m, void *v)
{
	/* Print if description exists */

	/* Get HSM F/W version */
	if (tcc_hsm_cmd_get_version(MBOX_DEV_HSM, REQ_HSM_GET_VER,
				    &gVerParam) == TCCHSM_SUCCESS) {
		seq_printf(m, "%-20s: v%d.%d.%d\n", "HSM F/W", gVerParam.x,
			   gVerParam.y, gVerParam.z);
	}

	seq_printf(m, "%-20s: v%d.%d.%d\n", "HSM Device Driver",
		   HSM_DRIVER_VER_X, HSM_DRIVER_VER_Y, HSM_DRIVER_VER_Z);

	seq_printf(m, "%-20s: v%d.%d.%d or later\n", "Compatible HSM F/W",
		   HSM_REQUIRED_FW_VER_X, HSM_REQUIRED_FW_VER_Y,
		   HSM_REQUIRED_FW_VER_Z);

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(proc_hsminfo);
#endif

static int32_t tcc_hsm_probe(struct platform_device *pdev)
{
#if (0)
	const struct proc_dir_entry *pent;
#endif
	int32_t ret = 0;

	hsm_dma_buf = devm_kzalloc(&pdev->dev, sizeof(struct tcc_hsm_dma_buf),
				   GFP_KERNEL);
	if (hsm_dma_buf == NULL) {
		ELOG("failed to allocate dma_buf\n");
		ret = -ENOMEM;
		goto out;
	}

	hsm_dma_buf->dev = &pdev->dev;

	hsm_dma_buf->descVir =
		dma_alloc_coherent(&pdev->dev, TCC_HSM_DESC_BUF_SIZE,
				   &hsm_dma_buf->descPhy, GFP_KERNEL);
	if (hsm_dma_buf->descVir == NULL) {
		ELOG("failed to allocate hsm_dma_buf->descVir\n");
		devm_kfree(&pdev->dev, hsm_dma_buf);
		ret = -ENOMEM;
		goto out;
	}

	hsm_dma_buf->srcVir =
		dma_alloc_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE,
				   &hsm_dma_buf->srcPhy, GFP_KERNEL);
	if (hsm_dma_buf->srcVir == NULL) {
		ELOG("failed to allocate hsm_dma_buf->srcVir\n");
		dma_free_coherent(&pdev->dev, TCC_HSM_DESC_BUF_SIZE,
				  hsm_dma_buf->descVir, hsm_dma_buf->descPhy);
		devm_kfree(&pdev->dev, hsm_dma_buf);
		ret = -ENOMEM;
		goto out;
	}

	hsm_dma_buf->dstVir =
		dma_alloc_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE,
				   &hsm_dma_buf->dstPhy, GFP_KERNEL);
	if (hsm_dma_buf->dstVir == NULL) {
		ELOG("failed to allocate hsm_dma_buf->dstVir\n");
		dma_free_coherent(&pdev->dev, TCC_HSM_DESC_BUF_SIZE,
				  hsm_dma_buf->descVir, hsm_dma_buf->descPhy);
		dma_free_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE,
				  hsm_dma_buf->srcVir, hsm_dma_buf->srcPhy);
		devm_kfree(&pdev->dev, hsm_dma_buf);
		ret = -ENOMEM;
		goto out;
	}

	if (misc_register(&tcc_hsm_miscdevice) != 0) {
		ELOG("register device err\n");
		dma_free_coherent(&pdev->dev, TCC_HSM_DESC_BUF_SIZE,
				  hsm_dma_buf->descVir, hsm_dma_buf->descPhy);
		dma_free_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE,
				  hsm_dma_buf->srcVir, hsm_dma_buf->srcPhy);
		dma_free_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE,
				  hsm_dma_buf->dstVir, hsm_dma_buf->dstPhy);
		devm_kfree(&pdev->dev, hsm_dma_buf);
		ret = -EBUSY;
		goto out;
	}

#if (0)
	pent = proc_create("hsminfo", 0444, NULL, &proc_hsminfo_fops);
	if (pent == NULL) {
		(void)pr_err("Failed to create hsminfo procfs entry\n");
		ret = -ENOMEM;
		goto out;
	}
#endif
out:
	return ret;
}

static int32_t tcc_hsm_remove(struct platform_device *pdev)
{
	misc_deregister(&tcc_hsm_miscdevice);

	dma_free_coherent(&pdev->dev, TCC_HSM_DESC_BUF_SIZE,
			  hsm_dma_buf->descVir, hsm_dma_buf->descPhy);
	hsm_dma_buf->descVir = NULL;

	dma_free_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE, hsm_dma_buf->srcVir,
			  hsm_dma_buf->srcPhy);
	hsm_dma_buf->srcVir = NULL;

	dma_free_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE, hsm_dma_buf->dstVir,
			  hsm_dma_buf->dstPhy);
	hsm_dma_buf->dstVir = NULL;

	devm_kfree(&pdev->dev, hsm_dma_buf);

	return 0;
}

#ifdef CONFIG_PM
static int32_t tcc_hsm_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int32_t tcc_hsm_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define tcc_hsm_suspend (NULL)
#define tcc_hsm_resume (NULL)
#endif

#ifdef CONFIG_OF
static const struct of_device_id hsm_of_match[2] = {
	{ .compatible = "telechips,tcc-hsm" },
	{ "", "", "", NULL },
};

MODULE_DEVICE_TABLE(of, hsm_of_match);
#endif

static struct platform_driver tcc_hsm_driver = {
	.driver = { .name = "tcc_hsm",
		    .owner = THIS_MODULE,
#ifdef CONFIG_OF
		    .of_match_table = of_match_ptr(hsm_of_match)
#endif
	},
	.probe = tcc_hsm_probe,
	.remove = tcc_hsm_remove,
#ifdef CONFIG_PM
	.suspend = tcc_hsm_suspend,
	.resume = tcc_hsm_resume,
#endif
};

static int32_t __init tcc_hsm_init(void)
{
	int32_t ret = 0;

	ret = platform_driver_register(&tcc_hsm_driver);
	if (ret != 0) {
		ELOG("platform_driver_register err(%d)\n", ret);
		goto out;
	}

out:
	return ret;
}

static void __exit tcc_hsm_exit(void)
{
	platform_driver_unregister(&tcc_hsm_driver);
}

module_init(tcc_hsm_init);
module_exit(tcc_hsm_exit);

MODULE_AUTHOR("linux <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips TCC HSM driver");
MODULE_LICENSE("GPL");
