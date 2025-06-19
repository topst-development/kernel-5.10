// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/memblock.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <asm/setup.h>
#include <linux/input.h>
#include <linux/debugfs.h>

#include "dwmac-tcc-v2.h"

static struct tcc_dwmac *gmac_debugfs;

static void tcc_create_debugfs(
	const char *name, struct dentry *fs_parent,
	const struct file_operations *file_ops)
{
	(void)debugfs_create_file(
		name, ((uint16_t)0x180), fs_parent, NULL, file_ops);
}

static int
dwmac_tcc_debugfs_enable_set(void *data, u64 val)
{
	(void)data;
	if (val > 0u) {
		tcc_dwmac_tuning_timing(gmac_debugfs);
	}
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(
	dwmac_tcc_debugfs_enable_fops,
	NULL,
	dwmac_tcc_debugfs_enable_set, "%llu\n");

static int txclk_i_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->txclk_i_dly;
	return 0;
}

static int txclk_i_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->txclk_i_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(
	txclk_i_dly_fops, txclk_i_dly_get, txclk_i_dly_set, "%llu\n");

static int txclk_i_inv_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->txclk_i_inv;
	return 0;
}

static int txclk_i_inv_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->txclk_i_inv = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(
	txclk_i_inv_fops, txclk_i_inv_get, txclk_i_inv_set, "%llu\n");

static int txclk_o_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->txclk_o_dly;
	return 0;
}

static int txclk_o_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->txclk_o_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(
	txclk_o_dly_fops, txclk_o_dly_get, txclk_o_dly_set, "%llu\n");

static int txclk_o_inv_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->txclk_o_inv;
	return 0;
}

static int txclk_o_inv_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->txclk_o_inv = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(
	txclk_o_inv_fops, txclk_o_inv_get, txclk_o_inv_set, "%llu\n");

static int txen_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->txen_dly;
	return 0;
}

static int txen_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->txen_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(txen_dly_fops, txen_dly_get, txen_dly_set, "%llu\n");

static int txer_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->txer_dly;
	return 0;
}

static int txer_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->txer_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(txer_dly_fops, txer_dly_get, txer_dly_set, "%llu\n");

static int txd0_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->txd0_dly;
	return 0;
}

static int txd0_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->txd0_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(txd0_dly_fops, txd0_dly_get, txd0_dly_set, "%llu\n");

static int txd1_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->txd1_dly;
	return 0;
}

static int txd1_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->txd1_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(txd1_dly_fops, txd1_dly_get, txd1_dly_set, "%llu\n");

static int txd2_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->txd2_dly;
	return 0;
}

static int txd2_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->txd2_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(txd2_dly_fops, txd2_dly_get, txd2_dly_set, "%llu\n");

static int txd3_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->txd3_dly;
	return 0;
}

static int txd3_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->txd3_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(txd3_dly_fops, txd3_dly_get, txd3_dly_set, "%llu\n");

static int txd4_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->txd4_dly;
	return 0;
}

static int txd4_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->txd4_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(txd4_dly_fops, txd4_dly_get, txd4_dly_set, "%llu\n");

static int txd5_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->txd5_dly;
	return 0;
}

static int txd5_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->txd5_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(txd5_dly_fops, txd5_dly_get, txd5_dly_set, "%llu\n");

static int txd6_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->txd6_dly;
	return 0;
}

static int txd6_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->txd6_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(txd6_dly_fops, txd6_dly_get, txd6_dly_set, "%llu\n");

static int txd7_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->txd7_dly;
	return 0;
}

static int txd7_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->txd7_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(txd7_dly_fops, txd7_dly_get, txd7_dly_set, "%llu\n");

static int rxclk_i_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->rxclk_i_dly;
	return 0;
}

static int rxclk_i_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->rxclk_i_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(
	rxclk_i_dly_fops, rxclk_i_dly_get, rxclk_i_dly_set, "%llu\n");

static int rxclk_i_inv_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->rxclk_i_inv;
	return 0;
}

static int rxclk_i_inv_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->rxclk_i_inv = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(
	rxclk_i_inv_fops, rxclk_i_inv_get, rxclk_i_inv_set, "%llu\n");

static int rxdv_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->rxdv_dly;
	return 0;
}

static int rxdv_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->rxdv_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(rxdv_dly_fops, rxdv_dly_get, rxdv_dly_set, "%llu\n");

static int rxer_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->rxer_dly;
	return 0;
}

static int rxer_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->rxer_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(rxer_dly_fops, rxer_dly_get, rxer_dly_set, "%llu\n");

static int rxd0_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->rxd0_dly;
	return 0;
}

static int rxd0_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->rxd0_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(rxd0_dly_fops, rxd0_dly_get, rxd0_dly_set, "%llu\n");

static int rxd1_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->rxd1_dly;
	return 0;
}

static int rxd1_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->rxd1_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(rxd1_dly_fops, rxd1_dly_get, rxd1_dly_set, "%llu\n");

static int rxd2_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->rxd2_dly;
	return 0;
}

static int rxd2_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->rxd2_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(rxd2_dly_fops, rxd2_dly_get, rxd2_dly_set, "%llu\n");

static int rxd3_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->rxd3_dly;
	return 0;
}

static int rxd3_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->rxd3_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(rxd3_dly_fops, rxd3_dly_get, rxd3_dly_set, "%llu\n");

static int rxd4_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->rxd4_dly;
	return 0;
}

static int rxd4_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->rxd4_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(rxd4_dly_fops, rxd4_dly_get, rxd4_dly_set, "%llu\n");

static int rxd5_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->rxd5_dly;
	return 0;
}

static int rxd5_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->rxd5_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(rxd5_dly_fops, rxd5_dly_get, rxd5_dly_set, "%llu\n");

static int rxd6_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->rxd6_dly;
	return 0;
}

static int rxd6_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->rxd6_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(rxd6_dly_fops, rxd6_dly_get, rxd6_dly_set, "%llu\n");

static int rxd7_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->rxd7_dly;
	return 0;
}

static int rxd7_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->rxd7_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(rxd7_dly_fops, rxd7_dly_get, rxd7_dly_set, "%llu\n");

static int crs_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->crs_dly;
	return 0;
}

static int crs_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->crs_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(crs_dly_fops, crs_dly_get, crs_dly_set, "%llu\n");

static int col_dly_get(void *data, u64 *val)
{
	(void)data;
	*val = gmac_debugfs->col_dly;
	return 0;
}

static int col_dly_set(void *data, u64 val)
{
	(void)data;
	gmac_debugfs->col_dly = (uint32_t)(val & 0xffffffffu);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(col_dly_fops, col_dly_get, col_dly_set, "%llu\n");

#ifdef GMAC_ECC_TEST_FEATURE

static int dummy_ecc_event_get(void *data, u64 *val)
{
	(void)data;
	*val = tcc_get_ecc_status(gmac_debugfs);
	return 0;
}

static int dumy_ecc_event_set(void *data, u64 val)
{
	(void)data;

	if (val == 0) {
		/* Set Dummy ECC Event */
		tcc_set_dummy_ecc_err(gmac_debugfs);
	} else {
		/* Enable ECC */
		tcc_enable_ecc_event(gmac_debugfs, val);
	}

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(
	dumy_ecc_event_fops, dummy_ecc_event_get, dumy_ecc_event_set, "%llu\n");
#endif

void tcc_dwmac_debugfs_init(struct tcc_dwmac *gmac)
{
	static struct dentry *dbgfs_dir;

	gmac_debugfs = gmac;
	dbgfs_dir = debugfs_create_dir("dwmac_tcc_debugfs", NULL);
	gmac->dbgfs_dir = dbgfs_dir;
	if (dbgfs_dir != NULL) {
		tcc_create_debugfs("txclk_i_dly", dbgfs_dir, &txclk_i_dly_fops);
		tcc_create_debugfs("txclk_i_inv", dbgfs_dir, &txclk_i_inv_fops);
		tcc_create_debugfs("txclk_o_dly", dbgfs_dir, &txclk_o_dly_fops);
		tcc_create_debugfs("txclk_o_inv", dbgfs_dir, &txclk_o_inv_fops);
		tcc_create_debugfs("txen_dly", dbgfs_dir, &txen_dly_fops);
		tcc_create_debugfs("txer_dly", dbgfs_dir, &txer_dly_fops);
		tcc_create_debugfs("txd0_dly", dbgfs_dir, &txd0_dly_fops);
		tcc_create_debugfs("txd1_dly", dbgfs_dir, &txd1_dly_fops);
		tcc_create_debugfs("txd2_dly", dbgfs_dir, &txd2_dly_fops);
		tcc_create_debugfs("txd3_dly", dbgfs_dir, &txd3_dly_fops);
		tcc_create_debugfs("txd4_dly", dbgfs_dir, &txd4_dly_fops);
		tcc_create_debugfs("txd5_dly", dbgfs_dir, &txd5_dly_fops);
		tcc_create_debugfs("txd6_dly", dbgfs_dir, &txd6_dly_fops);
		tcc_create_debugfs("txd7_dly", dbgfs_dir, &txd7_dly_fops);
		tcc_create_debugfs("rxclk_i_dly", dbgfs_dir, &rxclk_i_dly_fops);
		tcc_create_debugfs("rxclk_i_inv", dbgfs_dir, &rxclk_i_inv_fops);
		tcc_create_debugfs("rxdv_dly", dbgfs_dir, &rxdv_dly_fops);
		tcc_create_debugfs("rxer_dly", dbgfs_dir, &rxer_dly_fops);
		tcc_create_debugfs("rxd0_dly", dbgfs_dir, &rxd0_dly_fops);
		tcc_create_debugfs("rxd1_dly", dbgfs_dir, &rxd1_dly_fops);
		tcc_create_debugfs("rxd2_dly", dbgfs_dir, &rxd2_dly_fops);
		tcc_create_debugfs("rxd3_dly", dbgfs_dir, &rxd3_dly_fops);
		tcc_create_debugfs("rxd4_dly", dbgfs_dir, &rxd4_dly_fops);
		tcc_create_debugfs("rxd5_dly", dbgfs_dir, &rxd5_dly_fops);
		tcc_create_debugfs("rxd6_dly", dbgfs_dir, &rxd6_dly_fops);
		tcc_create_debugfs("rxd7_dly", dbgfs_dir, &rxd7_dly_fops);
		tcc_create_debugfs("crs_dly", dbgfs_dir, &crs_dly_fops);
		tcc_create_debugfs("col_dly", dbgfs_dir, &col_dly_fops);
		tcc_create_debugfs(
			"enable", dbgfs_dir, &dwmac_tcc_debugfs_enable_fops);
#ifdef GMAC_ECC_TEST_FEATURE
		tcc_create_debugfs("ecc_test", dbgfs_dir,
				   &dumy_ecc_event_fops);
#endif
	}
}

void tcc_dwmac_debugfs_exit(struct tcc_dwmac *gmac)
{
	if (gmac->dbgfs_dir != NULL) {
		debugfs_remove_recursive(gmac->dbgfs_dir);
	}
}
