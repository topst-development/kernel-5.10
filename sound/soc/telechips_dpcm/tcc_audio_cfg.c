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
#include <linux/reset.h>

#include "tcc_audio_hw.h"
#include "tcc_audio_cfg.h"

#undef audio_cfg_dbg
#if 0
#define audio_cfg_dbg(a...) \
	(void) pr_info("[DEBUG][AUDIO_CFG] " a)
#else
#define audio_cfg_dbg(a...)
#endif
#define audio_cfg_err(a...) \
	(void) pr_err("[ERROR][AUDIO_CFG] " a)
#define MAX_ARRAY \
	(10)

struct tcc_audio_cfg_t {
	struct platform_device *pdev;
	void __iomem *reg;

	uint32_t dai_arr_sz;
	uint32_t dai_arr[MAX_ARRAY];

	uint32_t spdif_arr_sz;
	uint32_t spdif_arr[MAX_ARRAY];

	uint32_t dp_arr_sz;
	uint32_t dp_arr[MAX_ARRAY];

	uint32_t ar_prot_arr_sz;
	uint32_t ar_prot_arr[IDX_ARRAY_PROT_CACHE];
	uint32_t aw_prot_arr_sz;
	uint32_t aw_prot_arr[IDX_ARRAY_PROT_CACHE];
	uint32_t ar_cache_arr_sz;
	uint32_t ar_cache_arr[IDX_ARRAY_PROT_CACHE];
	uint32_t aw_cache_arr_sz;
	uint32_t aw_cache_arr[IDX_ARRAY_PROT_CACHE];

	uint32_t axi_ctrl_master;
	uint32_t axi_ctrl_slave;
	uint32_t axi_ctrl_slave_power;
	struct reset_control *rst;
};

static void tcc_audio_cfg_dai_chmux_setup(const struct tcc_audio_cfg_t *audio_cfg)
{
	uint32_t i;
	uint32_t port;

	for (i = 0; i < audio_cfg->dai_arr_sz; i++) {
		port = audio_cfg->dai_arr[i];

		audio_cfg_dbg("dai_chmux[%d] : %d\n", i, port);
#if defined(CONFIG_ARCH_TCC807X)
		tcc_audio_cfg_daif_port_sel(audio_cfg->reg, i, port);
#else
		iobuscfg_dai_chmux(audio_cfg->reg, i, port);
#endif
	}
}

static void tcc_audio_cfg_spdif_chmux_setup(const struct tcc_audio_cfg_t *audio_cfg)
{
	uint32_t port_idx;
	uint32_t maic_idx;

	for (port_idx = 0; port_idx < audio_cfg->spdif_arr_sz; port_idx++) {
		maic_idx = audio_cfg->spdif_arr[port_idx];

		audio_cfg_dbg("spdif_chmux[%d] : %d\n", port_idx, maic_idx);
#if defined(CONFIG_ARCH_TCC807X)
		tcc_audio_cfg_spdif_port_sel(audio_cfg->reg, port_idx, maic_idx);
#else
		iobuscfg_spdif_chmux(audio_cfg->reg, i, port);
#endif
	}
}


static void tcc_audio_cfg_dp_chmux_setup(const struct tcc_audio_cfg_t *audio_cfg)
{
	uint32_t port_idx;
	uint32_t maic_idx;

	for (port_idx = 0; port_idx < audio_cfg->dp_arr_sz; port_idx++) {
		maic_idx = audio_cfg->dp_arr[port_idx];

		audio_cfg_dbg("dp_chmux[%d] : %d\n", port_idx, maic_idx);
#if defined(CONFIG_ARCH_TCC807X)
		tcc_audio_cfg_dp_port_sel(audio_cfg->reg, port_idx, maic_idx);
#endif
	}
}

static void tcc_audio_cfg_chmux_setup(const struct tcc_audio_cfg_t *audio_cfg)
{
	tcc_audio_cfg_dai_chmux_setup(audio_cfg);
	tcc_audio_cfg_spdif_chmux_setup(audio_cfg);
	tcc_audio_cfg_dp_chmux_setup(audio_cfg);
}

static void tcc_audio_cfg_ar_prot_setup(const struct tcc_audio_cfg_t *audio_cfg)
{
	uint32_t i;
	uint32_t prot;

	for (i = 0; i < audio_cfg->ar_prot_arr_sz; i++) {
		prot = audio_cfg->ar_prot_arr[i];

		audio_cfg_dbg("ar_prot[%d] : 0x%x\n", i, prot);
#if defined(CONFIG_ARCH_TCC807X)
		tcc_audio_cfg_set_ar_prot(audio_cfg->reg, i, prot);
#endif
	}
}

static void tcc_audio_cfg_aw_prot_setup(const struct tcc_audio_cfg_t *audio_cfg)
{
	uint32_t i;
	uint32_t prot;

	for (i = 0; i < audio_cfg->ar_prot_arr_sz; i++) {
		prot = audio_cfg->ar_prot_arr[i];

		audio_cfg_dbg("aw_prot[%d] : %d\n", i, prot);
#if defined(CONFIG_ARCH_TCC807X)
		tcc_audio_cfg_set_aw_prot(audio_cfg->reg, i, prot);
#endif
	}
}

static void tcc_audio_cfg_ar_cache_setup(const struct tcc_audio_cfg_t *audio_cfg)
{
	uint32_t i;
	uint32_t cache;

	for (i = 0; i < audio_cfg->ar_prot_arr_sz; i++) {
		cache = audio_cfg->ar_prot_arr[i];

		audio_cfg_dbg("ar_cache[%d] : %d\n", i, cache);
#if defined(CONFIG_ARCH_TCC807X)
		tcc_audio_cfg_set_ar_cache(audio_cfg->reg, i, cache);
#endif
	}
}

static void tcc_audio_cfg_aw_cache_setup(const struct tcc_audio_cfg_t *audio_cfg)
{
	uint32_t i;
	uint32_t cache;

	for (i = 0; i < audio_cfg->ar_prot_arr_sz; i++) {
		cache = audio_cfg->ar_prot_arr[i];

		audio_cfg_dbg("aw_cache[%d] : %d\n", i, cache);
#if defined(CONFIG_ARCH_TCC807X)
		tcc_audio_cfg_set_aw_cache(audio_cfg->reg, i, cache);
#endif
	}
}

static void tcc_audio_cfg_prot_cache_setup(const struct tcc_audio_cfg_t *audio_cfg)
{
	tcc_audio_cfg_ar_prot_setup(audio_cfg);
	tcc_audio_cfg_aw_prot_setup(audio_cfg);
	tcc_audio_cfg_ar_cache_setup(audio_cfg);
	tcc_audio_cfg_aw_cache_setup(audio_cfg);
}

static ssize_t tcc_audio_cfg_dai_chmux_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	const struct tcc_audio_cfg_t *audio_cfg = dev_get_drvdata(dev);
	ssize_t len;
	unsigned int i;
	int ret = 0;

	unused(attr);

	len = 0;
	for (i = 0; i < audio_cfg->dai_arr_sz; i++) {
		ret = scnprintf(&buf[len], 19u,
				"dai_chmux[%d] : %u\n",
				ui_to_si(i),
				audio_cfg->dai_arr[i]);
		if (ret >= 0){
			len = sl_add(len, ret); 
		}
	}

	return len;	
}

static ssize_t tcc_audio_cfg_dai_chmux_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	struct tcc_audio_cfg_t *audio_cfg = dev_get_drvdata(dev);
	const char *sep = " ";
	const char *stok, *freeptr;
	char *str;
	uint32_t idx;
	int32_t err;
	uint32_t value;
	ssize_t ret;

	unused(attr);

	str = kstrdup(buf, GFP_KERNEL);
	freeptr = str;

	idx = 0u;
	stok = strsep(&str, sep);
	while((idx < audio_cfg->dai_arr_sz) && (stok != NULL)) {
		audio_cfg_dbg("[DAI][%d] %s\n", idx, stok);

		err = kstrtouint(stok, 10, &value);
		if (err == 0) {
			audio_cfg->dai_arr[idx] = value;
		}

		idx++;
		stok = strsep(&str, sep);
	}

	kfree(freeptr);

	tcc_audio_cfg_dai_chmux_setup(audio_cfg);

	ret=ul_to_sl(count);
	return ret;
}

static ssize_t tcc_audio_cfg_spdif_chmux_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	const struct tcc_audio_cfg_t *audio_cfg = dev_get_drvdata(dev);
	ssize_t len;
	unsigned int i;
	int ret = 0;

	unused(attr);

	len = 0;
	for (i = 0; i < ui_to_si(audio_cfg->spdif_arr_sz); i++) {
		ret = scnprintf(&buf[len], 19u,
					"spdif_chmux[%d] : %u\n",
					ui_to_si(i),
					audio_cfg->spdif_arr[i]);
		if (ret >= 0){
			len = sl_add(len, ret); 
		}
	}

	return len;
}

static ssize_t tcc_audio_cfg_spdif_chmux_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	struct tcc_audio_cfg_t *audio_cfg = dev_get_drvdata(dev);
	const char *sep = " ";
	const char *stok, *freeptr;
	char *str;
	int32_t err;
	uint32_t idx, value;
	ssize_t ret;

	unused(attr);

	str = kstrdup(buf, GFP_KERNEL);
	freeptr = str;

	idx = 0u;
	stok = strsep(&str, sep);
	while ((idx < audio_cfg->spdif_arr_sz) && (stok != NULL)) {
		audio_cfg_dbg("[SPDIF][%d] %s\n", idx, stok);

		err = kstrtouint(stok, 10, &value);
		if (err == 0) {
			audio_cfg->spdif_arr[idx] = value;
		}
		idx++;
		stok = strsep(&str, sep);
	}

	kfree(freeptr);

	tcc_audio_cfg_spdif_chmux_setup(audio_cfg);

	ret=ul_to_sl(count);
	return ret;
}

static ssize_t tcc_audio_cfg_dp_chmux_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	const struct tcc_audio_cfg_t *audio_cfg = dev_get_drvdata(dev);
	ssize_t len;
	unsigned int i;
	int ret = 0;

	unused(attr);

	audio_cfg_dbg("%s\n", __func__);

	len = 0;

	for (i = 0; i < audio_cfg->dp_arr_sz; i++) {
		ret = scnprintf(&buf[len], 19u,
				"dp_chmux[%d] : %u\n",
				ui_to_si(i),
				audio_cfg->dp_arr[i]);
		if (ret >= 0){
			len = sl_add(len, ret); 
		}
	}

	return len;
}

static ssize_t tcc_audio_cfg_dp_chmux_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	struct tcc_audio_cfg_t *audio_cfg = dev_get_drvdata(dev);
	const char *sep = " ";
	const char *stok, *freeptr;
	char *str;
	int32_t err;
	uint32_t idx, value;
	ssize_t ret;

	unused(attr);

	audio_cfg_dbg("%s\n", __func__);

	str = kstrdup(buf, GFP_KERNEL);
	freeptr = str;

	idx = 0u;
	stok = strsep(&str, sep);
	while ((idx < audio_cfg->dp_arr_sz) && (stok != NULL)) {
		audio_cfg_dbg("[DP][%d] %s\n", idx, stok);

		err = kstrtouint(stok, 10, &value);
		if (err == 0) {
			audio_cfg->dp_arr[idx] = value;
		}
		idx++;
		stok = strsep(&str, sep);
	}

	kfree(freeptr);

	tcc_audio_cfg_dp_chmux_setup(audio_cfg);

	ret=ul_to_sl(count);
	return ret;
}

static ssize_t tcc_audio_cfg_ar_prot_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	const struct tcc_audio_cfg_t *audio_cfg = dev_get_drvdata(dev);
	ssize_t len;
	unsigned int i;
	int ret = 0;

	unused(attr);

	audio_cfg_dbg("%s\n", __func__);

	len = 0;

	for (i = 0; i < audio_cfg->ar_prot_arr_sz; i++) {
		ret = scnprintf(&buf[len], 26u,
				"ar_prot_arr[%d] : 0x%x\n",
				ui_to_si(i),
				audio_cfg->ar_prot_arr[i]);
		if (ret >= 0){
			len = sl_add(len, ret); 
		}
	}
	return len;
}

static ssize_t tcc_audio_cfg_ar_prot_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	struct tcc_audio_cfg_t *audio_cfg = dev_get_drvdata(dev);
	const char *sep = " ";
	const char *stok, *freeptr;
	char *str;
	int32_t err;
	uint32_t idx, value;
	ssize_t ret;

	unused(attr);

	audio_cfg_dbg("%s\n", __func__);

	str = kstrdup(buf, GFP_KERNEL);
	freeptr = str;

	idx = 0u;
	stok = strsep(&str, sep);
	while ((idx < audio_cfg->ar_prot_arr_sz) && (stok != NULL)) {
		audio_cfg_dbg("[AR_PROT][%d] %s\n", idx, stok);

		err = kstrtouint(stok, 0, &value);
		if (err == 0) {
			audio_cfg->ar_prot_arr[idx] = value;
		}
		idx++;
		stok = strsep(&str, sep);
	}

	kfree(freeptr);

	tcc_audio_cfg_ar_prot_setup(audio_cfg);

	ret = ul_to_sl(count);
	return ret;
}

static ssize_t tcc_audio_cfg_aw_prot_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	const struct tcc_audio_cfg_t *audio_cfg = dev_get_drvdata(dev);
	ssize_t len;
	unsigned int i;
	int ret = 0;

	unused(attr);

	audio_cfg_dbg("%s\n", __func__);

	len = 0;

	for (i = 0; i < audio_cfg->aw_prot_arr_sz; i++) {
		ret = scnprintf(&buf[len], 26u,
				"aw_prot_arr[%d] : 0x%x\n",
				i,
				audio_cfg->aw_prot_arr[i]);
		if (ret >= 0) {
			len = sl_add(len, ret); 
		}
	}

	return len;
}

static ssize_t tcc_audio_cfg_aw_prot_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	struct tcc_audio_cfg_t *audio_cfg = dev_get_drvdata(dev);
	const char *sep = " ";
	const char *stok, *freeptr;
	char *str;
	uint32_t idx;
	int err;
	uint32_t value;
	ssize_t ret;

	unused(attr);

	audio_cfg_dbg("%s\n", __func__);

	str = kstrdup(buf, GFP_KERNEL);
	freeptr = str;

	idx = 0u;
	stok = strsep(&str, sep);
	while ((idx < audio_cfg->aw_prot_arr_sz) && (stok != NULL)) {
		audio_cfg_dbg("[AW_PROT][%d] %s\n", idx, stok);

		err = kstrtouint(stok, 0, &value);
		if (err == 0) {
			audio_cfg->aw_prot_arr[idx] = value;
		}
		idx++;
		stok = strsep(&str, sep);
	}

	kfree(freeptr);

	tcc_audio_cfg_aw_prot_setup(audio_cfg);

	ret = ul_to_sl(count);
	return ret;
}


static ssize_t tcc_audio_cfg_ar_cache_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	const struct tcc_audio_cfg_t *audio_cfg = dev_get_drvdata(dev);
	ssize_t len;
	unsigned int i;
	int ret = 0;

	unused(attr);

	audio_cfg_dbg("%s\n", __func__);

	len = 0;
	for (i = 0; i < audio_cfg->ar_cache_arr_sz; i++) {
		ret = scnprintf(&buf[len], 26u,
			"ar_prot_arr[%d] : 0x%x\n",
			i,
			audio_cfg->ar_cache_arr[i]);
		if (ret >= 0){
			len = sl_add(len, ret); 
		}
	}

	return len;
}

static ssize_t tcc_audio_cfg_ar_cache_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	struct tcc_audio_cfg_t *audio_cfg = dev_get_drvdata(dev);
	const char *sep = " ";
	const char *stok, *freeptr;
	char *str;
	uint32_t idx;
	int32_t err;
	uint32_t value;
	ssize_t ret;

	unused(attr);

	audio_cfg_dbg("%s\n", __func__);

	str = kstrdup(buf, GFP_KERNEL);
	freeptr = str;

	idx = 0;
	stok = strsep(&str, sep);
	while ((idx < audio_cfg->ar_cache_arr_sz) && (stok != NULL)) {
		audio_cfg_dbg("[AR_PROT][%d] %s\n", idx, stok);

		err = kstrtouint(stok, 0, &value);
		if (err == 0) {
			audio_cfg->ar_cache_arr[idx] = value;
		}
		idx++;
		stok = strsep(&str, sep);
	}

	kfree(freeptr);

	tcc_audio_cfg_ar_cache_setup(audio_cfg);

	ret = ul_to_sl(count);
	return ret;
}

static ssize_t tcc_audio_cfg_aw_cache_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	const struct tcc_audio_cfg_t *audio_cfg = dev_get_drvdata(dev);
	int  len, ret = 0;
	unsigned int i;
	unused(attr);

	audio_cfg_dbg("%s\n", __func__);

	len = 0;

	for (i = 0; i < audio_cfg->aw_cache_arr_sz; i++) {
		ret = scnprintf(&buf[len], 26u,
				"aw_prot_arr[%d] : 0x%x\n",
				ui_to_si(i),
				audio_cfg->aw_cache_arr[i]);
		if (ret >= 0){
			len = si_add(len, ret);
		}
	}

	return len;
}

static ssize_t tcc_audio_cfg_aw_cache_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	struct tcc_audio_cfg_t *audio_cfg = dev_get_drvdata(dev);
	const char *sep = " ";
	const char *stok, *freeptr;
	char *str;
	uint32_t idx;
	int err;
	uint32_t value;
	ssize_t ret;

	unused(attr);

	audio_cfg_dbg("%s\n", __func__);

	str = kstrdup(buf, GFP_KERNEL);
	freeptr = str;

	idx = 0;
	stok = strsep(&str, sep);
	while ((idx < audio_cfg->aw_cache_arr_sz) && (stok != NULL)) {
		audio_cfg_dbg("[AW_CACHE][%d] %s\n", idx, stok);

		err = kstrtouint(stok, 0, &value);
		if (err == 0) {
			audio_cfg->aw_cache_arr[idx] = value;
		}
		idx++;
		stok = strsep(&str, sep);
	}

	kfree(freeptr);

	tcc_audio_cfg_aw_cache_setup(audio_cfg);
	ret = ul_to_sl(count);
	return ret;
}

static ssize_t tcc_audio_cfg_axi_ctrl_master_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	const struct tcc_audio_cfg_t *audio_cfg = dev_get_drvdata(dev);
	int ret = 0;
	unused(attr);
	audio_cfg_dbg("%s\n", __func__);

	ret = scnprintf(buf, 30u, "axi_ctr_master : 0x%x\n", audio_cfg->axi_ctrl_master);
	return ret;
}

static ssize_t tcc_audio_cfg_axi_ctrl_master_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	struct tcc_audio_cfg_t *audio_cfg = dev_get_drvdata(dev);
	const char *sep = " ";
	const char *stok, *freeptr;
	char *str;
	int32_t err;
	uint32_t value;
	ssize_t ret;

	unused(attr);

	audio_cfg_dbg("%s\n", __func__);

	str = kstrdup(buf, GFP_KERNEL);
	freeptr = str;

	stok = strsep(&str, sep);
	audio_cfg_dbg("[AXI_CTRL_MASTER]%s\n", stok);
	err = kstrtouint(stok, 0, &value);
	if (err == 0) {
		audio_cfg->axi_ctrl_master = value;

		//To do : tcc_audio_cfg_ctrl_master_setup(audio_cfg);
	}
	stok = strsep(&str, sep);
	kfree(freeptr);
	
	ret = ul_to_sl(count);
	return ret;
}

static ssize_t tcc_audio_cfg_tcc_axi_ctrl_slave_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	const struct tcc_audio_cfg_t *audio_cfg = dev_get_drvdata(dev);
	int ret = 0;
	unused(attr);
	audio_cfg_dbg("%s\n", __func__);

	ret = scnprintf(buf, 30u, "axi_ctr_slave : 0x%x\n", audio_cfg->axi_ctrl_master);
	return ret;
}

static ssize_t tcc_audio_cfg_tcc_axi_ctrl_slave_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	struct tcc_audio_cfg_t *audio_cfg = dev_get_drvdata(dev);
	const char *sep = " ";
	const char *stok, *freeptr;
	char *str;
	int32_t err;
	uint32_t value;
	ssize_t ret;

	unused(attr);

	audio_cfg_dbg("%s\n", __func__);

	str = kstrdup(buf, GFP_KERNEL);
	freeptr = str;

	stok = strsep(&str, sep);
	audio_cfg_dbg("[AXI_CTRL_MASTER]%s\n", stok);
	err = kstrtouint(stok, 0, &value);
	if (err == 0) {
		audio_cfg->axi_ctrl_slave = value;

		//To do : tcc_audio_cfg_ctrl_slave_setup(audio_cfg);
	}
	stok = strsep(&str, sep);
	kfree(freeptr);

	ret = ul_to_sl(count);
	return ret;
}

static ssize_t tcc_audio_cfg_tcc_axi_ctrl_slave_power_show(
	struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	const struct tcc_audio_cfg_t *audio_cfg = dev_get_drvdata(dev);
	int ret = 0;
	unused(attr);
	audio_cfg_dbg("%s\n", __func__);

	ret = scnprintf(buf, 36u, "axi_ctr_slave power : 0x%x\n", audio_cfg->axi_ctrl_master);
	return ret;
}

static ssize_t tcc_audio_cfg_tcc_axi_ctrl_slave_power_store(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t count)
{
	struct tcc_audio_cfg_t *audio_cfg = dev_get_drvdata(dev);
	const char *sep = " ";
	const char *stok, *freeptr;
	char *str;
	int32_t err;
	uint32_t value;
	ssize_t ret;

	unused(attr);

	audio_cfg_dbg("%s\n", __func__);

	str = kstrdup(buf, GFP_KERNEL);
	freeptr = str;

	stok = strsep(&str, sep);
	audio_cfg_dbg("[AXI_CTRL_MASTER]%s\n", stok);
	err = kstrtouint(stok, 10, &value);
	if (err == 0) {
		audio_cfg->axi_ctrl_slave_power = value;

		//To do : tcc_audio_cfg_ctrl_slave_power_setup(audio_cfg);
	}
	stok = strsep(&str, sep);
	kfree(freeptr);

	ret = ul_to_sl(count);
	return ret;
}


#define TCC_RWUGO (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH) //0664

static DEVICE_ATTR(
	tcc_dai_chmux,
	TCC_RWUGO,
	tcc_audio_cfg_dai_chmux_show,
	tcc_audio_cfg_dai_chmux_store);
static DEVICE_ATTR(
	tcc_spdif_chmux,
	TCC_RWUGO,
	tcc_audio_cfg_spdif_chmux_show,
	tcc_audio_cfg_spdif_chmux_store);
static DEVICE_ATTR(
	tcc_dp_chmux,
	TCC_RWUGO,
	tcc_audio_cfg_dp_chmux_show,
	tcc_audio_cfg_dp_chmux_store);

static DEVICE_ATTR(
	tcc_ar_prot,
	TCC_RWUGO,
	tcc_audio_cfg_ar_prot_show,
	tcc_audio_cfg_ar_prot_store);

static DEVICE_ATTR(
	tcc_ar_cache,
	TCC_RWUGO,
	tcc_audio_cfg_ar_cache_show,
	tcc_audio_cfg_ar_cache_store);

static DEVICE_ATTR(
	tcc_aw_prot,
	TCC_RWUGO,
	tcc_audio_cfg_aw_prot_show,
	tcc_audio_cfg_aw_prot_store);

static DEVICE_ATTR(
	tcc_aw_cache,
	TCC_RWUGO,
	tcc_audio_cfg_aw_cache_show,
	tcc_audio_cfg_aw_cache_store);

static DEVICE_ATTR(
	tcc_axi_ctrl_master,
	TCC_RWUGO,
	tcc_audio_cfg_axi_ctrl_master_show,
	tcc_audio_cfg_axi_ctrl_master_store);

static DEVICE_ATTR(
	tcc_axi_ctrl_slave,
	TCC_RWUGO,
	tcc_audio_cfg_tcc_axi_ctrl_slave_show,
	tcc_audio_cfg_tcc_axi_ctrl_slave_store);

static DEVICE_ATTR(
	tcc_axi_ctrl_slave_power,
	TCC_RWUGO,
	tcc_audio_cfg_tcc_axi_ctrl_slave_power_show,
	tcc_audio_cfg_tcc_axi_ctrl_slave_power_store);


static const struct attribute *tcc_audio_cfg_attributes[] = {
	&dev_attr_tcc_dai_chmux.attr,
	&dev_attr_tcc_spdif_chmux.attr,
	&dev_attr_tcc_dp_chmux.attr,
	&dev_attr_tcc_ar_prot.attr,
	&dev_attr_tcc_ar_cache.attr,
	&dev_attr_tcc_aw_prot.attr,
	&dev_attr_tcc_aw_cache.attr,
	&dev_attr_tcc_axi_ctrl_master.attr,
	&dev_attr_tcc_axi_ctrl_slave.attr,
	&dev_attr_tcc_axi_ctrl_slave_power.attr,
	NULL,
};

#if defined(CONFIG_ARCH_TCC807X)
#if 0//for the future
static int tcc_audio_cfg_assert_pmu_reset(struct tcc_audio_cfg_t *audio_cfg)
{
	int err = -EINVAL;

	if (audio_cfg != NULL) {
		if (audio_cfg->rst != NULL) {
			err = reset_control_assert(audio_cfg->rst);
			if (err != (int)0) {
				goto err_assert_pmu_reset;
			}

			mdelay(10);
			err = reset_control_deassert(audio_cfg->rst);
			if (err != (int)0) {
				goto err_assert_pmu_reset;
			}
		}
	}
	err_assert_pmu_reset:
	return err;
}
#endif

#if 0//for the future
unsigned int tcc_cfg_axi_reset_activation_clock_stop(struct tcc_audio_cfg_t *audio_cfg)
{

	/** NOTE **/
	/* SMU_PMU_Configuration registers are described in Part2 SMU_PMU. */
	/* Audio_Subsystem_Configuration registers are described in Chapter 11.3. */
	/* Memory_Subsystem_Configuration registers are described in Part4 Memory sub-system. */


	/*** Asynchronous Bridge Low-Power Request ***/
	/* Before reset activation/clock stop to Audio sub-system, asynchronous bridge must be stop.
	Because remaining bus traffic can causes interconnector corruption.*/

	// Change Q-channel controller to manual mode for AXI asynchronous bridge in Audio sub-system.
	audio_cfg->axi_ctrl_slave_power = tcc_audio_cfg_rd(audio_cfg->reg, TCC_CFG_X2X_SLV_PWR_CFG);
	audio_cfg->axi_ctrl_slave_power &= ~(ACFG_SLV_PWR_CFG_CC_MODE_REQ_Msk);
	audio_cfg->axi_ctrl_slave_power |= (ACFG_SLV_PWR_CFG_CC_MODE_REQ_MANUAL);
	tcc_audio_cfg_wr(audio_cfg->reg, TCC_CFG_X2X_SLV_PWR_CFG, audio_cfg->axi_ctrl_slave_power);

	do{
		audio_cfg->axi_ctrl_slave_power = tcc_audio_cfg_rd(audio_cfg->reg, TCC_CFG_X2X_SLV_PWR_CFG);
	}while((audio_cfg->axi_ctrl_slave_power & ACFG_SLV_PWR_CFG_CC_MODE_ACK_Msk) != (0x1U << ACFG_SLV_PWR_CFG_CC_MODE_ACK_Pos));

	// Low-power request for AXI asynchronous bridge in Audio sub-system.
	audio_cfg->axi_ctrl_slave_power &= ~(ACFG_SLV_PWR_CFG_M_QREQN_Msk);
	tcc_audio_cfg_wr(audio_cfg->reg, TCC_CFG_X2X_SLV_PWR_CFG, audio_cfg->axi_ctrl_slave_power);


	// Wait for the low-power acknowledge for AXI asynchronous bridge in Audio sub-system.
	do{
		audio_cfg->axi_ctrl_slave_power = tcc_audio_cfg_rd(audio_cfg->reg, TCC_CFG_X2X_SLV_PWR_CFG);
	}while(((audio_cfg->axi_ctrl_slave_power & ACFG_SLV_PWR_CFG_M_QACCEPTN_Msk) != 0u) || \
			((audio_cfg->axi_ctrl_slave_power & ACFG_SLV_PWR_CFG_M_QDENY_Msk) != 0u));


	// Change Q-channel controller to manual mode for AXI asynchronous bridge in Memory sub-system
	audio_cfg->axi_ctrl_slave_power &= ~(ACFG_SLV_PWR_CFG_CC_MODE_REQ_Msk);
	audio_cfg->axi_ctrl_slave_power |= (ACFG_SLV_PWR_CFG_CC_MODE_REQ_MANUAL);
	tcc_audio_cfg_wr(audio_cfg->reg, TCC_CFG_X2X_SLV_PWR_CFG, audio_cfg->axi_ctrl_slave_power);

	// Low-power request for AXI asynchronous bridge in Memory sub-system
	audio_cfg->axi_ctrl_slave_power &= ~(ACFG_SLV_PWR_CFG_M_QREQN_Msk);
	tcc_audio_cfg_wr(audio_cfg->reg, TCC_CFG_X2X_SLV_PWR_CFG, audio_cfg->axi_ctrl_slave_power);

	// Wait for the low-power acknowledge for AXI asynchronous bridge in Memory sub-system
	do{
		audio_cfg->axi_ctrl_slave_power = tcc_audio_cfg_rd(audio_cfg->reg, TCC_CFG_X2X_SLV_PWR_CFG);
	}while(((audio_cfg->axi_ctrl_slave_power & ACFG_SLV_PWR_CFG_M_QACCEPTN_Msk) != 0u) ||
		((audio_cfg->axi_ctrl_slave_power & ACFG_SLV_PWR_CFG_M_QDENY_Msk) != 0u));

	/*** Sub-system Reset Assert & Clock Halt ***/
#if 0
	SMU_PMU_Configuration.CKC.SWRSTDIS[25] = 0;  	// software reset control enable for Audio sub-system
	SMU_PMU_Configuration.CKC.SWRESET[25] = 0;		// software reset assert for Audio sub-system
	SMU_PMU_Configuration.PMU.GCLKOEN_CFG[20] = 0;	// halt the clock for Audio sub-system
#else
	if (audio_cfg->rst != NULL){
		reset_control_assert(audio_cfg->rst);
	}
#endif
	return 0;
}
#endif 

static unsigned int tcc_cfg_axi_reset_deactivation_clock_restore(struct tcc_audio_cfg_t *audio_cfg)
{
	/** NOTE **/
	/* SMU_PMU_Configuration registers are described in Part2 SMU_PMU. */
	/* Audio_Subsystem_Configuration registers are described in Chapter 11.3. */
	/* Memory_Subsystem_Configuration registers are described in Part4 Memory sub-system. */
#if 0
	/*** Sub-system Reset De-Assert & Clock Restore ***/
	SMU_PMU_Configuration.PMU.GCLKOEN_CFG[20] = 1;	// restore the clock for Audio sub-system
	SMU_PMU_Configuration.CKC.SWRSTDIS[25] = 0;  	// software reset control enable for Audio sub-system
	SMU_PMU_Configuration.CKC.SWRESET[25] = 1;		// software reset de-assert for Audio sub-system
#else
	if (audio_cfg->rst != NULL){
		if(reset_control_deassert(audio_cfg->rst) != 0){
			audio_cfg_err("%s rest_contro_deasser error!\n", __func__);
		}
	}
#endif
	/*** Asynchronous Bridge Low-Power Release ***/
	// Change Q-channel controller to manual mode for AXI asynchronous bridge in Memory sub-system.
	audio_cfg->axi_ctrl_slave_power = tcc_audio_cfg_rd(audio_cfg->reg, TCC_CFG_X2X_SLV_PWR_CFG);
	audio_cfg->axi_ctrl_slave_power &= ~(ACFG_SLV_PWR_CFG_CC_MODE_REQ_Msk);
	audio_cfg->axi_ctrl_slave_power |= (ACFG_SLV_PWR_CFG_CC_MODE_REQ_MANUAL);
	tcc_audio_cfg_wr(audio_cfg->reg, TCC_CFG_X2X_SLV_PWR_CFG, audio_cfg->axi_ctrl_slave_power);

	do{
		audio_cfg->axi_ctrl_slave_power = tcc_audio_cfg_rd(audio_cfg->reg, TCC_CFG_X2X_SLV_PWR_CFG);
	}while((audio_cfg->axi_ctrl_slave_power & ACFG_SLV_PWR_CFG_CC_MODE_ACK_Msk) != ui_lshift(0x1U, ACFG_SLV_PWR_CFG_CC_MODE_ACK_Pos));

	// Low-power release for AXI asynchronous bridge in Memory sub-system.
	audio_cfg->axi_ctrl_slave_power &= ~(ACFG_SLV_PWR_CFG_M_QREQN_Msk);
	audio_cfg->axi_ctrl_slave_power |= ui_lshift(0x1U, ACFG_SLV_PWR_CFG_M_QREQN_Pos);
	tcc_audio_cfg_wr(audio_cfg->reg, TCC_CFG_X2X_SLV_PWR_CFG, audio_cfg->axi_ctrl_slave_power);

	// Wait for the low-power release acknowledge for AXI asynchronous bridge in Memory sub-system.
	do{
		audio_cfg->axi_ctrl_slave_power = tcc_audio_cfg_rd(audio_cfg->reg, TCC_CFG_X2X_SLV_PWR_CFG);
	}while(((audio_cfg->axi_ctrl_slave_power & ACFG_SLV_PWR_CFG_M_QACCEPTN_Msk) != ui_lshift(0x1U, ACFG_SLV_PWR_CFG_M_QACCEPTN_Pos)) ||
			((audio_cfg->axi_ctrl_slave_power & ACFG_SLV_PWR_CFG_M_QDENY_Msk) != 0u));

	// Change Q-channel controller to manual mode for AXI asynchronous bridge in Audio sub-system.
	audio_cfg->axi_ctrl_slave_power &= ~(ACFG_SLV_PWR_CFG_CC_MODE_REQ_Msk);
	audio_cfg->axi_ctrl_slave_power |= (ACFG_SLV_PWR_CFG_CC_MODE_REQ_MANUAL);
	tcc_audio_cfg_wr(audio_cfg->reg, TCC_CFG_X2X_SLV_PWR_CFG, audio_cfg->axi_ctrl_slave_power);

	// Low-power release for AXI asynchronous bridge in Audio sub-system.
	audio_cfg->axi_ctrl_slave_power &= ~(ACFG_SLV_PWR_CFG_M_QREQN_Msk);
	audio_cfg->axi_ctrl_slave_power |= ui_lshift(0x1U, ACFG_SLV_PWR_CFG_M_QREQN_Pos);
	tcc_audio_cfg_wr(audio_cfg->reg, TCC_CFG_X2X_SLV_PWR_CFG, audio_cfg->axi_ctrl_slave_power);

	// Wait for the low-power release acknowledge for AXI asynchronous bridge in Audio sub-system.
	do{
		audio_cfg->axi_ctrl_slave_power = tcc_audio_cfg_rd(audio_cfg->reg, TCC_CFG_X2X_SLV_PWR_CFG);
	}while(((audio_cfg->axi_ctrl_slave_power & ACFG_SLV_PWR_CFG_M_QACCEPTN_Msk) != ui_lshift(0x1U, ACFG_SLV_PWR_CFG_M_QACCEPTN_Pos)) ||
			((audio_cfg->axi_ctrl_slave_power & ACFG_SLV_PWR_CFG_M_QDENY_Msk) != 0u));
	return 0;
}
#endif


static int parse_audio_cfg_dt(
	struct platform_device *pdev,
	struct tcc_audio_cfg_t *audio_cfg)
{
	struct device_node *np = pdev->dev.of_node;
	int ret = 0, size = 0;

	audio_cfg->pdev = pdev;
	audio_cfg->reg = of_iomap(np, 0);
	if (IS_ERR((void *)audio_cfg->reg)) {
		audio_cfg->reg = NULL;
		audio_cfg_err("[ERROR][AUDIO_CFG] reg is NULL\n");
		ret = -EINVAL;
	} else {
		audio_cfg_dbg("reg=%p\n", audio_cfg->reg);

		size = of_property_count_elems_of_size(np, "dai_chmux", (int)sizeof(uint32_t));
		if (size < 0) {
			audio_cfg->dai_arr_sz = 0u;
		}else{
			audio_cfg->dai_arr_sz = si_to_ui(size);
		}

		(void)of_property_read_u32_array(np, "dai_chmux", audio_cfg->dai_arr,
				(size_t) audio_cfg->dai_arr_sz);

		size = of_property_count_elems_of_size(np, "spdif_chmux", (int)sizeof(uint32_t));
		if (size  < 0) {
			audio_cfg->spdif_arr_sz = 0u;
		}else{
			audio_cfg->spdif_arr_sz = si_to_ui(size);
		}
			

		(void)of_property_read_u32_array(
				np,
				"spdif_chmux",
				audio_cfg->spdif_arr,
				(size_t) audio_cfg->spdif_arr_sz);

		size = of_property_count_elems_of_size(np, "dp_chmux", (int)sizeof(uint32_t));
		if (size < 0) {
			audio_cfg->dp_arr_sz = 0u;
		}else{
			audio_cfg->dp_arr_sz = si_to_ui(size);
		}

		(void)of_property_read_u32_array(
				np,
				"dp_chmux",
				audio_cfg->dp_arr,
				(size_t) audio_cfg->dp_arr_sz);

		size = of_property_count_elems_of_size(np, "ar_prot", (int)sizeof(uint32_t));
		if (size < 0) {
			audio_cfg->ar_prot_arr_sz = 0u;
		}else{
			audio_cfg->ar_prot_arr_sz = si_to_ui(size);
		}

		(void)of_property_read_u32_array(
				np,
				"ar_prot",
				audio_cfg->ar_prot_arr,
				(size_t) audio_cfg->ar_prot_arr_sz);

		size = of_property_count_elems_of_size(np, "aw_prot", (int)sizeof(uint32_t));
		if (size < 0) {
			audio_cfg->aw_prot_arr_sz = 0u;
		}else{
			audio_cfg->aw_prot_arr_sz = si_to_ui(size);
		}

		(void)of_property_read_u32_array(
				np,
				"aw_prot",
				audio_cfg->aw_prot_arr,
				(size_t) audio_cfg->aw_prot_arr_sz);

		size = of_property_count_elems_of_size(np, "ar_cache", (int)sizeof(uint32_t));
		if (size < 0) {
			audio_cfg->ar_cache_arr_sz = 0u;
		}else{
			audio_cfg->ar_cache_arr_sz = si_to_ui(size);
		}

		(void)of_property_read_u32_array(
				np,
				"ar_cache",
				audio_cfg->ar_cache_arr,
				(size_t) audio_cfg->ar_cache_arr_sz);

		size = of_property_count_elems_of_size(np, "aw_cache", (int)sizeof(uint32_t));
		if (size < 0) {
			audio_cfg->aw_cache_arr_sz = 0u;
		}else{
			audio_cfg->aw_cache_arr_sz = si_to_ui(size);
		}

		(void)of_property_read_u32_array(
				np,
				"aw_cache",
				audio_cfg->aw_cache_arr,
				(size_t) audio_cfg->aw_cache_arr_sz);

#if defined(CONFIG_ARCH_TCC807X)
		audio_cfg->rst = devm_reset_control_get(&pdev->dev, NULL);
		if (IS_ERR((void*)audio_cfg->rst)) {
				audio_cfg->rst = NULL;
				audio_cfg_err("%s audio_cfg->rst err!!!\n",__func__);
		}
#endif
	}
	return ret;
}

static int tcc_audio_cfg_probe(struct platform_device *pdev)
{
	struct tcc_audio_cfg_t *audio_cfg;
	int ret = 0;

	audio_cfg_dbg("%s\n", __func__);

	audio_cfg = (struct tcc_audio_cfg_t *)devm_kzalloc(
		&pdev->dev,
		sizeof(struct tcc_audio_cfg_t),
		GFP_KERNEL);
	if (audio_cfg == NULL) {
		ret = -ENOMEM;
	} else {
		audio_cfg_dbg("%s - audio cfg : %p\n", __func__, audio_cfg);

		ret = parse_audio_cfg_dt(pdev, audio_cfg);
		if (ret < 0) {
			audio_cfg_err("%s : Fail to parse audio cfg dt\n", __func__);
			kfree(audio_cfg);
		} else {
			platform_set_drvdata(pdev, audio_cfg);

#if defined(CONFIG_ARCH_TCC807X) //To be deleted
			(void)tcc_cfg_axi_reset_deactivation_clock_restore(audio_cfg);
			(void)tcc_audio_cfg_cache_and_power_init(audio_cfg->reg);
#endif
			ret = sysfs_create_files(&pdev->dev.kobj, tcc_audio_cfg_attributes);
			if (ret != 0) {
				audio_cfg_err("[ERROR][AUDIO_CFG] failed create sysfs\r\n");
				kfree(audio_cfg);
			} else {
				audio_cfg_dbg("[AUDIO_CFG] success create sysfs\r\n");
				tcc_audio_cfg_chmux_setup(audio_cfg);
				tcc_audio_cfg_prot_cache_setup(audio_cfg);
			}
		}
	}
	return ret;
}

static int tcc_audio_cfg_remove(struct platform_device *pdev)
{
	const struct tcc_audio_cfg_t *audio_cfg =
	    (struct tcc_audio_cfg_t *)platform_get_drvdata(pdev);

	audio_cfg_dbg("%s\n", __func__);

	devm_kfree(&pdev->dev, audio_cfg);

	return 0;
}

static int tcc_audio_cfg_suspend(
	struct platform_device *pdev,
	pm_message_t state)
{
#if 0
	struct tcc_audio_cfg_t *audio_cfg =
			(struct tcc_audio_cfg_t *)platform_get_drvdata(pdev);

	tcc_cfg_axi_reset_activation_clock_stop(audio_cfg);
#else
	unused(pdev);
#endif
	unused(state);

	return 0;
}

static int tcc_audio_cfg_resume(struct platform_device *pdev)
{
	const struct tcc_audio_cfg_t *audio_cfg =
	    (struct tcc_audio_cfg_t *)platform_get_drvdata(pdev);

	audio_cfg_dbg("%s\n", __func__);
#if 0
	tcc_cfg_axi_reset_deactivation_clock_restore(audio_cfg);
#endif
	tcc_audio_cfg_chmux_setup(audio_cfg);
	tcc_audio_cfg_prot_cache_setup(audio_cfg);

	return 0;
}

static const struct of_device_id tcc_audio_cfg_of_match[] = {
	{.compatible = "telechips,audio-cfg-807x"},
	{.compatible = ""}
};

MODULE_DEVICE_TABLE(of, tcc_audio_cfg_of_match);

static struct platform_driver tcc_audio_cfg_driver = {
	.probe = tcc_audio_cfg_probe,
	.remove = tcc_audio_cfg_remove,
	.suspend = tcc_audio_cfg_suspend,
	.resume = tcc_audio_cfg_resume,
	.driver = {
	.name = "tcc_audio_cfg_drv",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(tcc_audio_cfg_of_match),
#endif
	},
};

module_platform_driver(tcc_audio_cfg_driver);

MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips Audio Subsystem Configuration Driver");
MODULE_LICENSE("GPL");
