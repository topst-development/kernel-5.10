// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2023 Telechips Inc.
 */

#include <linux/of_platform.h>
#include <asm/mach/arch.h>

/* Wrapper of 'OF_DEV_AUXDATA' to parenthesize arguments */
#define OF_DEV_AUXDATA_P(_compat, _phys, _name) \
	OF_DEV_AUXDATA((_compat), (_phys), (_name), NULL)

static struct of_dev_auxdata tcc803x_auxdata_lookup[] __initdata = {
	/* sentinel */
	{},
};

static void __init tcc803x_dt_init(void)
{
	of_platform_default_populate(NULL, tcc803x_auxdata_lookup, NULL);
	platform_device_register_simple("tcc-cpufreq", -1, NULL, 0);
}

static char const *tcc803x_dt_compat[] __initconst = {
	"telechips,tcc803x",
	NULL
};

DT_MACHINE_START(tcc803x_dt, "Telechips TCC803x (Flattened Device Tree)")
	.init_machine = tcc803x_dt_init,
	.dt_compat = tcc803x_dt_compat,
MACHINE_END
