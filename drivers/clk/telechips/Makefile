# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2023 Telechips Inc.

ifeq ($(CONFIG_ARCH_TCC803X),y)
	clk-tcc-y			:= clk-legacy.o
else
	clk-tcc-y			:= clk.o
	clk-tcc-y			+= pll.o
#	clk-tcc-y			+= divider.o
	clk-tcc-y			+= composite.o
	clk-tcc-y			+= gate.o
endif

clk-tcc-$(CONFIG_ARCH_TCC803X)		+= clk-tcc803x.o
clk-tcc-$(CONFIG_ARCH_TCC805X)		+= clk-tcc805x.o
clk-tcc-$(CONFIG_ARCH_TCC807X)		+= clk-tcc807x.o
obj-$(CONFIG_COMMON_CLK_TELECHIPS)	+= clk-tcc.o
