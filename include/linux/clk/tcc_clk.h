/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2023 Telechips Inc.
 */

#ifndef TCC_CLK_H
#define TCC_CLK_H

#include <linux/types.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>

struct tcc_clk {
	struct clk_hw hw;
	const struct clk_ops *ops;
	struct list_head list;
	uint32_t id;
	uint32_t *clk_source_idx;
	uint8_t clk_src_length;
};

extern struct list_head pll_clk_list;
extern struct list_head bus_clk_list;
extern struct list_head peri_clk_list;
extern struct list_head gate_clk_list;

struct tcc_clk_data {
	const char *name;
	const char *parent_name;
	unsigned int idx;
	unsigned int flags;
};

struct tcc_clks_type {
	const char *parent_name;
	unsigned int clks_num;
	struct tcc_clk_data *data;
	unsigned int data_num;
	unsigned int common_flags;
};

struct tcc_ckc_ops {
	/* pmu pwdn */
	int (*ckc_pmu_pwdn)(int id, bool pwdn);
	int (*ckc_is_pmu_pwdn)(int id);

	/* software reset */
	int (*ckc_swreset)(int id, bool reset);

	/* isoip top */
	int (*ckc_isoip_top_pwdn)(int id, bool pwdn);
	int (*ckc_is_isoip_top_pwdn)(int id);

	/* isoip ddi */
	int (*ckc_isoip_ddi_pwdn)(int id, bool pwdn);
	int (*ckc_is_isoip_ddi_pwdn)(int id);

	/* isoip gpu */
	int (*ckc_isoip_gpu_pwdn)(int id, bool pwdn);
	int (*ckc_is_isoip_gpu_pwdn)(int id);

	/* pll */
	int (*ckc_pll_set_rate)(int id, unsigned long rate);
	unsigned long (*ckc_pll_get_rate)(int id);
	int (*ckc_is_pll_enabled)(int id);

	/* clkctrl */
	int (*ckc_clkctrl_enable)(int id);
	int (*ckc_clkctrl_disable)(int id);
	int (*ckc_clkctrl_set_rate)(int id, unsigned long rate);
	unsigned long (*ckc_clkctrl_get_rate)(int id);
	int (*ckc_is_clkctrl_enabled)(int id);

	/* peripheral */
	int (*ckc_peri_enable)(int id);
	int (*ckc_peri_disable)(int id);
#if defined(CONFIG_ARCH_TCC897X)
	int (*ckc_peri_set_rate)(int id, unsigned long rate);
#else
	int (*ckc_peri_set_rate)(int id, unsigned long rate, ulong flags);
#endif
	unsigned long (*ckc_peri_get_rate)(int id);
	int (*ckc_is_peri_enabled)(int id);

	/* display bus */
	int (*ckc_ddibus_pwdn)(int id, bool pwdn);
	int (*ckc_is_ddibus_pwdn)(int id);
	int (*ckc_ddibus_swreset)(int id, bool reset);

	/* graphic bus */
	int (*ckc_gpubus_pwdn)(int id, bool pwdn);
	int (*ckc_is_gpubus_pwdn)(int id);
	int (*ckc_gpubus_swreset)(int id, bool reset);

	/* io bus */
	int (*ckc_iobus_pwdn)(int id, bool pwdn);
	int (*ckc_is_iobus_pwdn)(int id);
	int (*ckc_iobus_swreset)(int id, bool reset);

	/* video bus */
	int (*ckc_vpubus_pwdn)(int id, bool pwdn);
	int (*ckc_is_vpubus_pwdn)(int id);
	int (*ckc_vpubus_swreset)(int id, bool reset);

	/* hsio bus */
	int (*ckc_hsiobus_pwdn)(int id, bool pwdn);
	int (*ckc_is_hsiobus_pwdn)(int id);
	int (*ckc_hsiobus_swreset)(int id, bool reset);

	/* g2d bus */
	int (*ckc_g2dbus_pwdn)(int id, bool pwdn);
	int (*ckc_is_g2dbus_pwdn)(int id);
	int (*ckc_g2dbus_swreset)(int id, bool reset);

	/* cortex-m bus */
	int (*ckc_cmbus_pwdn)(int id, bool pwdn);
	int (*ckc_is_cmbus_pwdn)(int id);
	int (*ckc_cmbus_swreset)(int id, bool reset);
};

void tcc_clk_proc_init(void);
int tcc_clk_suspend(void);
void tcc_clk_resume(void);

static inline struct tcc_clk *to_tcc_clk(void *data)
{
	return (struct tcc_clk *)container_of((data), struct tcc_clk, hw);
}

#endif
