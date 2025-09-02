// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
//#define NDEBUG
#define TLOG_LEVEL (TLOG_WARNING)
#include "tcc_hsm_log.h"

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

#include <linux/tcc_hsm.h>
#include <linux/uaccess.h>
#include "tcc_hsm_sp_cmd.h"

/****************************************************************************
 * DEFINITiON
 ****************************************************************************/
#define TCC_HSM_DMA_BUF_SIZE 4096

/****************************************************************************
 * DEFINITION OF LOCAL VARIABLES
 ****************************************************************************/
static DEFINE_MUTEX(tcc_hsm_mutex);

static struct tcc_hsm_dma_buf {
	struct device *dev;
	dma_addr_t srcPhy;
	uint8_t *srcVir;
	dma_addr_t dstPhy;
	uint8_t *dstVir;
} * hsm_dma_buf;

/****************************************************************************
 * DEFINITION OF LOCAL FUNCTIONS
 ****************************************************************************/
static uint32_t tcc_hsm_ioctl_get_version(ulong arg)
{
	struct tcc_hsm_ioctl_version_param param;
	uint32_t ret = TCCHSM_ERR;

	ret = tcc_hsm_sp_cmd_get_version(MBOX_DEV_M4, &param);
	if (ret != TCCHSM_SUCCESS) {
		ELOG("failed to get version from SP\n");
		goto out;
	}

	if (copy_to_user((void *)arg, (void *)&param, sizeof(param))) {
		ELOG("copy_to_user failed\n");
		ret = TCCHSM_ERR;
		goto out;
	}

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_set_mode(ulong arg)
{
	struct tcc_hsm_ioctl_set_mode_param param;
	uint32_t ret = TCCHSM_ERR;

	if (copy_from_user(&param,
			   (const struct tcc_hsm_ioctl_set_mdoe_param *)arg,
			   sizeof(param))) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	ret = tcc_hsm_sp_cmd_set_mode(MBOX_DEV_M4, &param);

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_set_key(ulong arg)
{
	struct tcc_hsm_ioctl_set_key_param param;
	uint8_t user_key[32] = {
		0,
	};
	uint32_t ret = TCCHSM_ERR;

	if (copy_from_user(&param,
			   (const struct tcc_hsm_ioctl_set_key_param *)arg,
			   sizeof(param))) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	if ((param.key == NULL) || (param.keySize > sizeof(user_key))) {
		ELOG("Invalid key data(size=0x%x)\n", param.keySize);
		goto out;
	}

	if (copy_from_user(user_key, (const uint8_t *)param.key,
			   param.keySize) != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed(%d)\n", param.keySize);
		goto out;
	}

	ret = tcc_hsm_sp_cmd_set_key(MBOX_DEV_M4, &param, user_key);

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_set_key_from_otp(ulong arg)
{
	struct tcc_hsm_ioctl_set_key_from_otp_param param;
	uint32_t ret = TCCHSM_ERR;

	if (copy_from_user(
		    &param,
		    (const struct tcc_hsm_ioctl_set_key_from_otp_param *)arg,
		    sizeof(param))) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	ret = tcc_hsm_sp_cmd_set_key_from_otp(MBOX_DEV_M4, &param);

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_set_iv(ulong arg)
{
	struct tcc_hsm_ioctl_set_iv_param param;
	uint8_t iv[32] = {
		0,
	};
	uint32_t ret = TCCHSM_ERR;

	if (copy_from_user(&param,
			   (const struct tcc_hsm_ioctl_set_iv_param *)arg,
			   sizeof(param))) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	if ((param.iv == NULL) || (param.ivSize > sizeof(iv))) {
		ELOG("Invalid iv data(size=0x%x)\n", param.ivSize);
		goto out;
	}

	if (copy_from_user(iv, (const uint8_t *)param.iv, param.ivSize) !=
	    TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed(%d)\n", param.ivSize);
		goto out;
	}

	ret = tcc_hsm_sp_cmd_set_iv(MBOX_DEV_M4, &param, iv);

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_set_kldata(ulong arg)
{
	struct tcc_hsm_ioctl_set_kldata_param param;
	struct tcc_hsm_kldata klData;
	uint32_t ret = TCCHSM_ERR;

	if (copy_from_user(&param,
			   (const struct tcc_hsm_ioctl_set_kldata_param *)arg,
			   sizeof(param))) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	if (param.klData == NULL) {
		ELOG("invalid klData\n");
		goto out;
	}

	if (copy_from_user(&klData, (const struct tcc_hsm_kldata *)param.klData,
			   sizeof(klData)) != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		goto out;
	}

	ret = tcc_hsm_sp_cmd_set_kldata(MBOX_DEV_M4, param.keyIndex, &klData,
					sizeof(klData));

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_run_cipher(ulong arg)
{
	struct tcc_hsm_ioctl_run_cipher_param param;
	uint32_t ret = TCCHSM_ERR;

	if (copy_from_user(&param,
			   (const struct tcc_hsm_iotcl_run_cipher_param *)arg,
			   sizeof(param))) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	if (param.srcSize > TCC_HSM_DMA_BUF_SIZE) {
		ELOG("The srcSize(0x%x) should not exceed 0x%x bytes\n",
		     param.srcSize, TCC_HSM_DMA_BUF_SIZE);
		goto out;
	}

	if (copy_from_user(hsm_dma_buf->srcVir, (const uint8_t *)param.srcAddr,
			   param.srcSize) != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		goto out;
	}

	/* HSM Firmware doesn't support 8byte Address */
	if ((hsm_dma_buf->srcPhy > UINT_MAX)) {
		ELOG("srcPhy = 0x%x\n", (uint32_t)hsm_dma_buf->srcPhy);
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}
	if ((hsm_dma_buf->dstPhy > UINT_MAX)) {
		ELOG("dstPhy = 0x%x\n", (uint32_t)hsm_dma_buf->dstPhy);
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	ret = tcc_hsm_sp_cmd_run_cipher_by_dma(
		MBOX_DEV_M4, param.keyIndex, (uint32_t)hsm_dma_buf->srcPhy,
		(uint32_t)hsm_dma_buf->dstPhy, param.srcSize, param.enc,
		param.cwSel, param.klIndex, param.keyMode);

	dma_sync_single_for_cpu(hsm_dma_buf->dev, hsm_dma_buf->dstPhy,
				param.srcSize, DMA_FROM_DEVICE);

	if (copy_to_user(param.dstAddr, (const uint8_t *)hsm_dma_buf->dstVir,
			 param.srcSize) != TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		goto out;
	}

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_run_cipher_by_dma(ulong arg)
{
	struct tcc_hsm_ioctl_run_cipher_param param;
	uint32_t ret = TCCHSM_ERR;

	if (copy_from_user(&param,
			   (const struct tcc_hsm_ioctl_run_cipher_param *)arg,
			   sizeof(param))) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	/* HSM Firmware doesn't support 8byte Address */
	if ((ULONG_MAX != UINT_MAX) && ((ulong)param.srcAddr > UINT_MAX)) {
		ELOG("srcPhy = 0x%lx\n", (ulong)param.srcAddr);
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}
	if ((ULONG_MAX != UINT_MAX) && ((ulong)param.dstAddr > UINT_MAX)) {
		ELOG("dstPhy = 0x%lx\n", (ulong)param.dstAddr);
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	ret = tcc_hsm_sp_cmd_run_cipher_by_dma(
		MBOX_DEV_M4, param.keyIndex, (ulong)param.srcAddr,
		(ulong)param.dstAddr, param.srcSize, param.enc, param.cwSel,
		param.klIndex, param.keyMode);

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_run_cmac(ulong arg)
{
	struct tcc_hsm_ioctl_run_cmac_param param;
	uint32_t ret = TCCHSM_ERR;

	if (copy_from_user(&param,
			   (const struct tcc_hsm_ioctl_run_cmac_param *)arg,
			   sizeof(param))) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	if (param.srcSize > TCC_HSM_DMA_BUF_SIZE) {
		ELOG("The srcSize(0x%x) should not exceed 0x%x bytes\n",
		     param.srcSize, TCC_HSM_DMA_BUF_SIZE);
		goto out;
	}

	if (copy_from_user(hsm_dma_buf->srcVir, (const uint8_t *)param.srcAddr,
			   param.srcSize) != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		goto out;
	}

	ret = tcc_hsm_sp_cmd_run_cmac(MBOX_DEV_M4, param.keyIndex, param.flag,
				      hsm_dma_buf->srcVir, param.srcSize,
				      hsm_dma_buf->dstVir, &param.mac_size);

	if (param.mac_size > TCC_HSM_DMA_BUF_SIZE) {
		ELOG("The macSize(0x%x) should not exceed 0x%x bytes\n",
		     param.mac_size, TCC_HSM_DMA_BUF_SIZE);
		goto out;
	}

	if (copy_to_user(param.macAddr, (const uint8_t *)hsm_dma_buf->dstVir,
			 param.mac_size) != TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		goto out;
	}

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_run_sha256(ulong arg)
{
	struct tcc_hsm_ioctl_run_sha_param param;
	uint32_t ret = TCCHSM_ERR;

	if (copy_from_user(&param,
			   (const struct tcc_hsm_ioctl_run_sha_param *)arg,
			   sizeof(param))) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	if (param.srcSize > TCC_HSM_DMA_BUF_SIZE) {
		ELOG("The srcSize(0x%x) should not exceed 0x%x bytes\n",
		     param.srcSize, TCC_HSM_DMA_BUF_SIZE);
		goto out;
	}

	if (copy_from_user(hsm_dma_buf->srcVir, (const uint8_t *)param.srcAddr,
			   param.srcSize) != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		goto out;
	}

	ret = tcc_hsm_sp_cmd_run_sha256(MBOX_DEV_M4, param.flag,
					hsm_dma_buf->srcVir, param.srcSize,
					hsm_dma_buf->dstVir, &param.digSize);

	if (param.digSize > TCC_HSM_DMA_BUF_SIZE) {
		ELOG("The digSize(0x%x) should not exceed 0x%x bytes\n",
		     param.digSize, TCC_HSM_DMA_BUF_SIZE);
		goto out;
	}

	if (copy_to_user(param.dig, (const uint8_t *)hsm_dma_buf->dstVir,
			 param.digSize) != TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		goto out;
	}

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_write_otp(ulong arg)
{
	struct tcc_hsm_ioctl_otp_param param;
	uint32_t ret = TCCHSM_ERR;

	if (copy_from_user(&param, (const struct tcc_hsm_ioctl_otp_param *)arg,
			   sizeof(param))) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	if (param.size > TCC_HSM_DMA_BUF_SIZE) {
		ELOG("The size(0x%x) should not exceed 0x%x bytes\n",
		     param.size, TCC_HSM_DMA_BUF_SIZE);
		goto out;
	}

	if (copy_from_user(hsm_dma_buf->srcVir, (const uint8_t *)param.buf,
			   param.size) != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		goto out;
	}

	ret = tcc_hsm_sp_cmd_write_otp(MBOX_DEV_M4, param.addr,
				       hsm_dma_buf->srcVir, param.size);

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_write_otp_image(ulong arg)
{
	struct tcc_hsm_ioctl_otp_image_param param;
	uint32_t ret = TCCHSM_ERR;

	if (copy_from_user(&param,
			   (const struct tcc_hsm_ioctl_otp_image_param *)arg,
			   sizeof(param))) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	if (param.size > TCC_HSM_DMA_BUF_SIZE) {
		ELOG("The size(0x%x) should not exceed 0x%x bytes\n",
		     param.size, TCC_HSM_DMA_BUF_SIZE);
		goto out;
	}

	if (copy_from_user(hsm_dma_buf->srcVir, (const uint8_t *)param.buf,
			   param.size) != TCCHSM_SUCCESS) {
		ELOG("copy_from_user failed\n");
		goto out;
	}
	/* HSM Firmware doesn't support 8byte Address */
	if ((hsm_dma_buf->srcPhy > UINT_MAX)) {
		ELOG("srcPhy = 0x%x\n", (uint32_t)hsm_dma_buf->srcPhy);
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}
	ret = tcc_hsm_sp_cmd_write_otp_image(
		MBOX_DEV_M4, (uint32_t)hsm_dma_buf->srcPhy, param.size);

out:
	return ret;
}

static uint32_t tcc_hsm_ioctl_get_rng(ulong arg)
{
	struct tcc_hsm_ioctl_rng_param param;
	uint32_t ret = TCCHSM_ERR;

	if (copy_from_user(&param, (const struct tcc_hsm_ioctl_rng_param *)arg,
			   sizeof(param))) {
		ELOG("copy_from_user failed\n");
		ret = TCCHSM_ERR_INVALID_MEMORY;
		goto out;
	}

	if ((param.rng == NULL) || (param.rngSize > TCCHSM_RNG_MAX)) {
		ELOG(" invalid param(%p, %d)\n", param.rng, param.rngSize);
		goto out;
	}

	ret = tcc_hsm_sp_cmd_get_rand(MBOX_DEV_M4, hsm_dma_buf->dstVir,
				      param.rngSize);

	if (copy_to_user(param.rng, (const uint8_t *)hsm_dma_buf->dstVir,
			 param.rngSize) != TCCHSM_SUCCESS) {
		ELOG("copy_to_user failed\n");
		goto out;
	}

out:
	return ret;
}

static long tcc_hsm_ioctl(struct file *hsm_file, unsigned int cmd, ulong arg)
{
	uint32_t ret = TCCHSM_ERR;
	// DLOG("cmd=%d\n", cmd);
	mutex_lock(&tcc_hsm_mutex);

	switch (cmd) {
	case TCCHSM_IOCTL_GET_VERSION:
		ret = tcc_hsm_ioctl_get_version(arg);
		break;

	case TCCHSM_IOCTL_SET_MODE:
		ret = tcc_hsm_ioctl_set_mode(arg);
		break;

	case TCCHSM_IOCTL_SET_KEY:
		ret = tcc_hsm_ioctl_set_key(arg);
		break;

	case TCCHSM_IOCTL_SET_KEY_FROM_OTP:
		ret = tcc_hsm_ioctl_set_key_from_otp(arg);
		break;

	case TCCHSM_IOCTL_SET_IV:
		ret = tcc_hsm_ioctl_set_iv(arg);
		break;

	case TCCHSM_IOCTL_SET_KLDATA:
		ret = tcc_hsm_ioctl_set_kldata(arg);
		break;

	case TCCHSM_IOCTL_RUN_CIPHER:
		ret = tcc_hsm_ioctl_run_cipher(arg);
		break;

	case TCCHSM_IOCTL_RUN_CIPHER_BY_DMA:
		ret = tcc_hsm_ioctl_run_cipher_by_dma(arg);
		break;

	case TCCHSM_IOCTL_RUN_CMAC:
		ret = tcc_hsm_ioctl_run_cmac(arg);
		break;

	case TCCHSM_IOCTL_RUN_SHA256:
		ret = tcc_hsm_ioctl_run_sha256(arg);
		break;

	case TCCHSM_IOCTL_WRITE_OTP:
		ret = tcc_hsm_ioctl_write_otp(arg);
		break;

	case TCCHSM_IOCTL_WRITE_OTP_IMAGE:
		ret = tcc_hsm_ioctl_write_otp_image(arg);
		break;

	case TCCHSM_IOCTL_GET_RNG:
		ret = tcc_hsm_ioctl_get_rng(arg);
		break;

	default:
		ELOG("unknown command(%d)\n", cmd);
		break;
	}

	mutex_unlock(&tcc_hsm_mutex);

	if ((ULONG_MAX == UINT_MAX) && (ret > LONG_MAX)) {
		ret = TCCHSM_ERR_INVALID_STATE;
	}

	return (long)ret;
}

static int tcc_hsm_open(struct inode *hsm_inode, struct file *hsm_filp)
{
	DLOG("\n");

	return 0;
}

static int tcc_hsm_release(struct inode *hsm_inode, struct file *hsm_file)
{
	DLOG("\n");

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

static int tcc_hsm_probe(struct platform_device *pdev)
{
	int ret = 0;

	hsm_dma_buf = devm_kzalloc(&pdev->dev, sizeof(struct tcc_hsm_dma_buf),
				   GFP_KERNEL);
	if (hsm_dma_buf == NULL) {
		ELOG("failed to allocate dma_buf\n");
		ret = -ENOMEM;
		goto out;
	}

	hsm_dma_buf->dev = &pdev->dev;

	hsm_dma_buf->srcVir =
		dma_alloc_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE,
				   &hsm_dma_buf->srcPhy, GFP_KERNEL);
	if (hsm_dma_buf->srcVir == NULL) {
		ELOG("failed to allocate dma_buf->srcVir\n");
		devm_kfree(&pdev->dev, hsm_dma_buf);
		ret = -ENOMEM;
		goto out;
	}

	hsm_dma_buf->dstVir =
		dma_alloc_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE,
				   &hsm_dma_buf->dstPhy, GFP_KERNEL);
	if (hsm_dma_buf->dstVir == NULL) {
		ELOG("failed to allocate dma_buf->dstVir\n");
		dma_free_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE,
				  hsm_dma_buf->srcVir, hsm_dma_buf->srcPhy);
		devm_kfree(&pdev->dev, hsm_dma_buf);
		ret = -ENOMEM;
		goto out;
	}

	if (misc_register(&tcc_hsm_miscdevice) != 0) {
		ELOG("register device err\n");
		dma_free_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE,
				  hsm_dma_buf->srcVir, hsm_dma_buf->srcPhy);
		dma_free_coherent(&pdev->dev, TCC_HSM_DMA_BUF_SIZE,
				  hsm_dma_buf->dstVir, hsm_dma_buf->dstPhy);
		devm_kfree(&pdev->dev, hsm_dma_buf);
		ret = -EBUSY;
		goto out;
	}

out:
	return ret;
}

static int tcc_hsm_remove(struct platform_device *pdev)
{
	DLOG("\n");

	misc_deregister(&tcc_hsm_miscdevice);

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
static int tcc_hsm_suspend(struct platform_device *pdev, pm_message_t state)
{
	DLOG("\n");
	return 0;
}

static int tcc_hsm_resume(struct platform_device *pdev)
{
	DLOG("\n");
	return 0;
}
#else
#define tcc_hsm_suspend NULL
#define tcc_hsm_resume NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id hsm_of_match[] = {
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

static int __init tcc_hsm_init(void)
{
	int ret = 0;

	DLOG("\n");

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
	DLOG("\n");

	platform_driver_unregister(&tcc_hsm_driver);
}

module_init(tcc_hsm_init);
module_exit(tcc_hsm_exit);

MODULE_AUTHOR("linux <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips TCC HSM driver");
MODULE_LICENSE("GPL");
