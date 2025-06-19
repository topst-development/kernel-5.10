# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2023 Telechips Inc.

#
# Telechips DTs for EVB
#

dtb-y += tcc7500-lpd4x321.dtb
dtb-y += tcc7501-lpd4x321.dtb
dtb-y += tcc7502-lpd4x321.dtb
dtb-y += tcc7505-lpd4x321.dtb

#
# Telechips SOC DTs
#

dtbo-y += tcc750x.dtb

#
# Telechips DT Overlays for EVB
#

dtbo-y += overlay-tcc7500-lpd4x321.dtb
dtbo-y += overlay-tcc7501-lpd4x321.dtb
dtbo-y += overlay-tcc7502-lpd4x321.dtb
dtbo-y += overlay-tcc7505-lpd4x321.dtb

# Generate __symbols__ node for SOC DTs and DT overlays
$(foreach dtbo,$(dtbo-y),$(eval DTC_FLAGS_$(basename $(notdir $(dtbo))) += -@))
dtb-y += $(dtbo-y)
