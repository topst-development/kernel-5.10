# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2023 Telechips Inc.

#
# Telechips DTs for EVB
#

dtb-y += tcc8030-lpd4321.dtb
dtb-y += tcc8031pe-lpd4321.dtb

#
# Telechips SOC DTs
#

dtbo-y += tcc803x.dtb

#
# Telechips DT Overlays for EVB
#

dtbo-y += overlay-tcc8030-lpd4321.dtb
dtbo-y += overlay-tcc8031pe-lpd4321.dtb

# Generate __symbols__ node for SOC DTs and DT overlays
$(foreach dtbo,$(dtbo-y),$(eval DTC_FLAGS_$(basename $(notdir $(dtbo))) += -@))
dtb-y += $(dtbo-y)
