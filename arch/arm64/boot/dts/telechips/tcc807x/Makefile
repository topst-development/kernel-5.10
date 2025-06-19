# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2023 Telechips Inc.

#
# Telechips DTs for EVB
#

dtb-y += tcc8070-lpd4x322.dtb

dtb-y += tcc8070-subcore-lpd4x322.dtb

#
# Telechips SOC DTs
#

dtbo-y += tcc807x.dtb
dtbo-y += tcc807x-subcore.dtb

#
# Telechips DT Overlays for EVB
#

dtbo-y += overlay-tcc8070-lpd4x322.dtb
dtbo-y += overlay-tcc8070-subcore-lpd4x322.dtb

# Generate __symbols__ node for SOC DTs and DT overlays
$(foreach dtbo,$(dtbo-y),$(eval DTC_FLAGS_$(basename $(notdir $(dtbo))) += -@))
dtb-y += $(dtbo-y)
