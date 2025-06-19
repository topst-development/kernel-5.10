/* SPDX-License-Identifier: (GPL-2.0-or-later OR MIT) */
/*
 * Copyright (C) 2023 Telechips Inc.
 */

#include <linux/hw_random.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/mailbox/tcc_sec_ipc.h>

#if defined(CONFIG_ARCH_TCC803X)
#include "../tcc_hsm/M4/tcc_hsm_sp_cmd.h"
#define MAX_ENR_CNT (4)
#define trng_run(priv, count) \
	tcc_hsm_sp_cmd_get_rand(MBOX_DEV_M4, (char*)(priv)->rnd_word, count)

#else
#include "../tcc_hsm/SC000/tcc_hsm_cmd.h"
#define MAX_ENR_CNT (8)
#define trng_run(priv, count) \
	tcc_hsm_cmd_get_rand( \
		MBOX_DEV_HSM, REQ_HSM_GET_RNG, (priv)->phy_rnd_word, count)
#endif

struct tcc_rng_priv {
	dma_addr_t phy_rnd_word;
	u32 *rnd_word;
	uint32_t rnd_word_cnt;
	uint32_t rnd_byte_cnt;
	u8 rnd_byte[4];
};

static inline u32 rng_read_word(struct tcc_rng_priv *priv)
{
	if (priv->rnd_word_cnt == 0) {
		trng_run(priv, MAX_ENR_CNT << 2);
		priv->rnd_word_cnt = MAX_ENR_CNT;
	}
	priv->rnd_word_cnt--;
	return priv->rnd_word[priv->rnd_word_cnt];
}

static inline char rng_read_byte(struct tcc_rng_priv *priv)
{
	u32 rand;

	if (priv->rnd_byte_cnt == 0) {
		rand = rng_read_word(priv);
		memcpy(priv->rnd_byte, &rand, 4);
		priv->rnd_byte_cnt = 4;
	}
	priv->rnd_byte_cnt--;

	return priv->rnd_byte[priv->rnd_byte_cnt];
}

static int tcc_rng_data_read(struct hwrng *rng, u32 *data)
{
	struct tcc_rng_priv *priv = (struct tcc_rng_priv *)rng->priv;

	*data = rng_read_word(priv);

	return sizeof(*data);
}

static int tcc_rng_read(struct hwrng *rng, void *data, size_t max, bool wait)
{
	struct tcc_rng_priv *priv = (struct tcc_rng_priv *)rng->priv;
	int i;

	if (!wait)
		return 0;

	for (i = 0; i < (max >> 2); i++)
		*((u32 *) data + i) = rng_read_word(priv);

	if (unlikely(max & 0x3))
		for (i = i << 2; i < max; i++)
			*((char *) data + i) = rng_read_byte(priv);

	return max;
}

static struct hwrng tcc_rng_ops = {
	.name = "tcc",
	.data_read = tcc_rng_data_read,
	.read = tcc_rng_read,
	.quality = 1000
};

static int tcc_rng_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tcc_rng_priv *priv;
	int err = -ENOMEM;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (priv != NULL) {
		priv->phy_rnd_word = 0;
		priv->rnd_word_cnt = 0;
		priv->rnd_byte_cnt = 0;
		priv->rnd_word = dma_alloc_coherent(
			dev, MAX_ENR_CNT << 2, &priv->phy_rnd_word, GFP_KERNEL);

		tcc_rng_ops.priv = (unsigned long)priv;

		err = hwrng_register(&tcc_rng_ops);
		if (err) {
			dev_err(dev, "hwrng registration failed\n");
			dma_free_coherent(
				dev, MAX_ENR_CNT << 2, priv->rnd_word,
				priv->phy_rnd_word);
		} else {
			dev_info(dev, "hwrng registered\n");
		}
	}

	return err;
}

static int tcc_rng_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tcc_rng_priv *priv = (struct tcc_rng_priv *)tcc_rng_ops.priv;

	hwrng_unregister(&tcc_rng_ops);

	dma_free_coherent(
		dev, MAX_ENR_CNT << 2, priv->rnd_word, priv->phy_rnd_word);

	kfree(priv);

	return 0;
}

static const struct of_device_id tcc_rng_of_match[] = {
	{
		.compatible = "telechips,tcc-rng",
	},
	{},
};

static struct platform_driver tcc_rng_driver = {
	.driver = {
		.name = "tcc-rng",
		.of_match_table = tcc_rng_of_match,
	},
	.probe = tcc_rng_probe,
	.remove = tcc_rng_remove,
};

module_platform_driver(tcc_rng_driver);

MODULE_AUTHOR("linux <linux@telechips.com>");
MODULE_DESCRIPTION("Telechips RNG driver");
MODULE_LICENSE("GPL");
