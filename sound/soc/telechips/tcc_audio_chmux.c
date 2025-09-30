// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/errno.h>

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/io.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>

#include <linux/string.h>

#include "tcc_audio_chmux.h"

#undef chmux_dbg
#if 0
#define chmux_dbg(a...) \
	(void) pr_info("[DEBUG][AUDIO_CHMUX] " a)
#else
#define chmux_dbg(a...)
#endif
#define chmux_err(a...) \
	(void) pr_err("[ERROR][AUDIO_CHMUX] " a)

#define MAX_ARRAY \
	(10)

struct tcc_audio_chmux_t {
	struct platform_device *pdev;
	void __iomem *reg;

	int dai_arr_sz;
	uint32_t dai_arr[MAX_ARRAY];

	int spdif_arr_sz;
	uint32_t spdif_arr[MAX_ARRAY];
};

static void tcc_audio_dai_chmux_setup(const struct tcc_audio_chmux_t *chmux)
{
	int32_t i;
	uint32_t port;

	for (i = 0; i < chmux->dai_arr_sz; i++) {
		port = chmux->dai_arr[i];

		chmux_dbg("dai[%d] : %d\n", i, port);

		iobuscfg_dai_chmux(chmux->reg, i, port);
	}

}

static void tcc_audio_spdif_chmux_setup(const struct tcc_audio_chmux_t *chmux)
{
	int32_t i;
	uint32_t port;

	for (i = 0; i < chmux->spdif_arr_sz; i++) {
		port = chmux->spdif_arr[i];

		chmux_dbg("spdif[%d] : %d\n", i, port);

		iobuscfg_spdif_chmux(chmux->reg, i, port);
	}
}

static void tcc_audio_chmux_setup(const struct tcc_audio_chmux_t *chmux)
{
	tcc_audio_dai_chmux_setup(chmux);
	tcc_audio_spdif_chmux_setup(chmux);
}

static ssize_t tcc_dai_chmux_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	const struct tcc_audio_chmux_t *chmux = dev_get_drvdata(dev);
	int len, i;

	unused(attr);

	len = 0;
	for (i = 0; i < chmux->dai_arr_sz; i++) {
		int ret = 0;
		ret = scnprintf(&buf[len], 19u,
					"dai_chmux[%d] : %u\n",
					i,
					chmux->dai_arr[i]);
		if ((ret >= 0) && (len <= (INT_MAX - ret))) {
			len += ret;
		}
	}

	return len;
}

static ssize_t tcc_dai_chmux_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	struct tcc_audio_chmux_t *chmux = dev_get_drvdata(dev);
	const char *sep = " ";
	const char *stok, *freeptr;
	char *str;
	int32_t idx, ret;
	uint32_t value;

	unused(attr);
	unused(count);

	str = kstrdup(buf, GFP_KERNEL);
	freeptr = str;

	idx = 0;
	stok = strsep(&str, sep);
	while((idx < chmux->dai_arr_sz) && (stok != NULL)) {
		chmux_dbg("[DAI][%d] %s\n", idx, stok);

		ret = kstrtouint(stok, 10, &value);
		if (ret == 0) {
			chmux->dai_arr[idx] = value;
		}

		idx++;
		stok = strsep(&str, sep);
	}

	kfree(freeptr);

	tcc_audio_dai_chmux_setup(chmux);

	return idx;
}

static ssize_t tcc_spdif_chmux_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	const struct tcc_audio_chmux_t *chmux = dev_get_drvdata(dev);
	int len, i;

	unused(attr);

	len = 0;
	for (i = 0; i < chmux->spdif_arr_sz; i++) {
		int ret = 0;
		ret = scnprintf(&buf[len], 19u,
					"spdif_chmux[%d] : %u\n",
					i,
					chmux->spdif_arr[i]);
		if ((ret >= 0) && (len <= (INT_MAX - ret))) {
			len += ret;
		}
	}

	return len;
}

static ssize_t tcc_spdif_chmux_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	struct tcc_audio_chmux_t *chmux = dev_get_drvdata(dev);
	const char *sep = " ";
	const char *stok, *freeptr;
	char *str;
	int32_t idx, ret;
	uint32_t value;

	unused(attr);
	unused(count);

	str = kstrdup(buf, GFP_KERNEL);
	freeptr = str;

	idx = 0;
	stok = strsep(&str, sep);
	while ((idx < chmux->spdif_arr_sz) && (stok != NULL)) {
		chmux_dbg("[SPDIF][%d] %s\n", idx, stok);

		ret = kstrtouint(stok, 10, &value);
		if (ret == 0) {
			chmux->spdif_arr[idx] = value;
		}
		idx++;
		stok = strsep(&str, sep);
	}

	kfree(freeptr);

	tcc_audio_spdif_chmux_setup(chmux);

	return idx;
}

#define TCC_RWUGO (S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IWGRP) //0664
static DEVICE_ATTR(
	tcc_dai_chmux,
	TCC_RWUGO,
	tcc_dai_chmux_show,
	tcc_dai_chmux_store);
static DEVICE_ATTR(
	tcc_spdif_chmux,
	TCC_RWUGO,
	tcc_spdif_chmux_show,
	tcc_spdif_chmux_store);

static const struct attribute *tcc_audio_chmux_attributes[] = {
	&dev_attr_tcc_dai_chmux.attr,
	&dev_attr_tcc_spdif_chmux.attr,
	NULL
};

static int parse_audio_chmux_dt(
	struct platform_device *pdev,
	struct tcc_audio_chmux_t *chmux)
{
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;

	chmux->pdev = pdev;
	chmux->reg = of_iomap(np, 0);
	if (IS_ERR((void *)chmux->reg)) {
		chmux->reg = NULL;
		(void)pr_err("[ERROR][AUDIO_CHMUX] reg is NULL\n");
		ret = -EINVAL;
	} else {
		chmux_dbg("reg=%p\n", chmux->reg);

		chmux->dai_arr_sz =
			of_property_count_elems_of_size(np, "dai", (int)sizeof(uint32_t));
		if (chmux->dai_arr_sz < 0) {
			chmux->dai_arr_sz = 0;
		}

		(void)of_property_read_u32_array(np, "dai", chmux->dai_arr,
				(size_t) chmux->dai_arr_sz);

		chmux->spdif_arr_sz =
			of_property_count_elems_of_size(np, "spdif", (int)sizeof(uint32_t));
		if (chmux->spdif_arr_sz < 0) {
			chmux->spdif_arr_sz = 0;
		}

		(void)of_property_read_u32_array(
				np,
				"spdif",
				chmux->spdif_arr,
				(size_t) chmux->spdif_arr_sz);
	}
	return ret;
}

static int tcc_audio_chmux_probe(struct platform_device *pdev)
{
	struct tcc_audio_chmux_t *chmux;
	int ret = 0;

	chmux_dbg("%s\n", __func__);

	chmux = (struct tcc_audio_chmux_t *)devm_kzalloc(
		&pdev->dev,
		sizeof(struct tcc_audio_chmux_t),
		GFP_KERNEL);
	if (chmux == NULL) {
		ret = -ENOMEM;
	} else {
		chmux_dbg("%s - chmux : %p\n", __func__, chmux);

		ret = parse_audio_chmux_dt(pdev, chmux);
		if (ret < 0) {
			chmux_err("%s : Fail to parse chmux dt\n", __func__);
			kfree(chmux);
		} else {
			platform_set_drvdata(pdev, chmux);

			ret = sysfs_create_files(&pdev->dev.kobj, tcc_audio_chmux_attributes);
			if (ret != 0) {
				chmux_err("[ERROR][AUDIO_CHMUX] failed create sysfs\r\n");
				kfree(chmux);
			} else {
				chmux_dbg("[AUDIO_CHMUX] success create sysfs\r\n");
				tcc_audio_chmux_setup(chmux);
			}
		}
	}
	return ret;
}

static int tcc_audio_chmux_remove(struct platform_device *pdev)
{
	const struct tcc_audio_chmux_t *chmux =
	    (struct tcc_audio_chmux_t *)platform_get_drvdata(pdev);

	chmux_dbg("%s\n", __func__);

	devm_kfree(&pdev->dev, chmux);

	return 0;
}

static int tcc_audio_chmux_suspend(
	struct platform_device *pdev,
	pm_message_t state)
{
	unused(pdev);
	unused(state);

	return 0;
}

static int tcc_audio_chmux_resume(struct platform_device *pdev)
{
	const struct tcc_audio_chmux_t *chmux =
	    (struct tcc_audio_chmux_t *)platform_get_drvdata(pdev);

	chmux_dbg("%s\n", __func__);

	tcc_audio_chmux_setup(chmux);

	return 0;
}

static const struct of_device_id tcc_audio_chmux_of_match[] = {
	{.compatible = "telechips,audio-chmux-898x"},
	{.compatible = "telechips,audio-chmux-899x"},
	{.compatible = "telechips,audio-chmux-803x"},
	{.compatible = "telechips,audio-chmux-805x"},
	{.compatible = "telechips,audio-chmux-806x"},
	{.compatible = "telechips,audio-chmux-901x"},
	{.compatible = ""}
};

MODULE_DEVICE_TABLE(of, tcc_audio_chmux_of_match);

static struct platform_driver tcc_audio_chmux_driver = {
	.probe = tcc_audio_chmux_probe,
	.remove = tcc_audio_chmux_remove,
	.suspend = tcc_audio_chmux_suspend,
	.resume = tcc_audio_chmux_resume,
	.driver = {
	.name = "tcc_audio_chmux_drv",
	.owner = THIS_MODULE,
#ifdef CONFIG_OF
	.of_match_table = of_match_ptr(tcc_audio_chmux_of_match),
#endif
},
};

module_platform_driver(tcc_audio_chmux_driver);

MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips Audio Channel Mux Driver");
MODULE_LICENSE("GPL");
