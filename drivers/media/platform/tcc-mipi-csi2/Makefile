# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) Telechips Inc.
#

tcc-cap-mipi-csi2-rx-objs := tcc-mipi-csi2.o

ifeq ($(CONFIG_ARCH_TCC803X), y)
	TCC-CAP-MIPI-CSI2-RX-ARCH = 803x
	TCC-CAP-MIPI-CSI2-RX-CSI = v1.2
	TCC-CAP-MIPI-CSI2-RX-DPHY = v1.3
endif
ifeq ($(CONFIG_ARCH_TCC805X), y)
	TCC-CAP-MIPI-CSI2-RX-ARCH = 805x
	TCC-CAP-MIPI-CSI2-RX-CSI = v1.2
	TCC-CAP-MIPI-CSI2-RX-DPHY = v1.3
endif
ifeq ($(CONFIG_ARCH_TCC750X), y)
	TCC-CAP-MIPI-CSI2-RX-ARCH = 750x
	TCC-CAP-MIPI-CSI2-RX-CSI = v2.0
	TCC-CAP-MIPI-CSI2-RX-DPHY = v1.3
endif
ifeq ($(CONFIG_ARCH_TCC807X), y)
	TCC-CAP-MIPI-CSI2-RX-ARCH = 807x
	TCC-CAP-MIPI-CSI2-RX-CSI = v2.1
	TCC-CAP-MIPI-CSI2-RX-DPHY = v2.0
endif

tcc-cap-mipi-csi2-rx-objs += \
	$(TCC-CAP-MIPI-CSI2-RX-ARCH)/tcc-mipi-csi2-cfg.o \
	$(TCC-CAP-MIPI-CSI2-RX-ARCH)/tcc-mipi-csi2-ckc.o \
	csi2_s/$(TCC-CAP-MIPI-CSI2-RX-CSI)/tcc-mipi-csi2-csis.o \
	dphy_s/$(TCC-CAP-MIPI-CSI2-RX-DPHY)/tcc-mipi-csi2-dphys.o

obj-$(CONFIG_TCC_MIPI_CSI2) += tcc-cap-mipi-csi2-rx.o
