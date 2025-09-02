// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Telechips Inc.
 */

#include <linux/clk/tcc_clk.h>
#include <linux/slab.h>

#include <linux/arm-smccc.h>
#include <soc/telechips/smc.h>

#define TCC803X_CKC_DRIVER
#include "clk-tcc803x.h"


#include <linux/proc_fs.h>
#include <linux/seq_file.h>

/* [DR]
 * tcc_clk_show is call back of single_open()
 * single_open declare call back function as
 * ‘int (*show)(struct seq_file *, void *)’
 */
static int tcc_clk_show(struct seq_file *s, void *v)
{
	struct arm_smccc_res res;
	unsigned long rate, enabled;
	char dis_str[] = "(disabled)";
	unsigned int i;
	struct clk_list {
		unsigned long id;
		char *output_fmt;
	};
	const struct clk_list bus_list[] = {
		{FBUS_CPU0,	  "         CPU(CA53) : %15lu Hz %s\n"},
		{FBUS_CPU1,	  "         CPU(A7SP) : %15lu Hz %s\n"},
		{FBUS_CMBUS,	  "            CM BUS : %15lu Hz %s\n"},
		{FBUS_CBUS,	  "           CPU BUS : %15lu Hz %s\n"},
		{FBUS_MEM,	  "       MEMBUS Core : %15lu Hz %s\n"},
		{FBUS_MEM_PHY,	  "           MEM PHY : %15lu Hz %s\n"},
		{FBUS_SMU,	  "           SMU BUS : %15lu Hz %s\n"},
		{FBUS_IO,	  "            IO BUS : %15lu Hz %s\n"},
		{FBUS_HSIO,	  "          HSIO BUS : %15lu Hz %s\n"},
		{FBUS_DDI,	  "DISPLAY BUS(DDIBUS): %15lu Hz %s\n"},
		{FBUS_GPU,	  "    Graphic 3D BUS : %15lu Hz %s\n"},
		{FBUS_G2D,	  "    Graphic 2D BUS : %15lu Hz %s\n"},
		{FBUS_VBUS,	  "         Video BUS : %15lu Hz %s\n"},
		{FBUS_CODA,	  "        CODA Clock : %15lu Hz %s\n"},
		{FBUS_CHEVC,	  "        HEVC(CCLK) : %15lu Hz %s\n"},
		{FBUS_BHEVC,	  "        HEVC(BCLK) : %15lu Hz %s\n"},
	};

	(void)v;

	/* [DR]
	 * Kernel API ARRAY_SIZE has defects
	 * it's inside.
	 */
	for (i = 0; i < ARRAY_SIZE(bus_list); i++) {
		arm_smccc_smc(SIP_CLK_WDPR_GET_CLKCTRL,
			      bus_list[i].id, 0, 0, 0, 0, 0, 0, &res);
		rate = res.a0;
		arm_smccc_smc(SIP_CLK_WDPR_IS_CLKCTRL,
			      bus_list[i].id, 0, 0, 0, 0, 0, 0, &res);
		enabled = res.a0;
		seq_printf(s, bus_list[i].output_fmt, rate,
			   (enabled == 1UL)?"":dis_str);
	}

	return 0;

}

void tcc_clk_proc_init(void)
{
	umode_t mode = 0x124; // (S_IRUSR | S_IRGRP | S_IROTH)
	const struct proc_dir_entry *proc_entry =
		proc_create_single_data("clocks", mode, NULL, tcc_clk_show, NULL);

	if (proc_entry == NULL) {
		(void)pr_err("[ERR][tcc_clk] Create /proc/clocks is failed\n");
	}
}
