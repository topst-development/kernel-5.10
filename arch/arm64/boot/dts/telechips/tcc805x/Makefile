# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2023 Telechips Inc.

#
# Telechips DTs for EVB
#

dtb-y += tcc8050-lpd4x322-sv0.1.dtb
dtb-y += tcc8050-lpd4x322-sv1.0.dtb
dtb-y += tcc8053-lpd4x322-sv0.1.dtb
dtb-y += tcc8053-lpd4x322-sv1.0.dtb
dtb-y += tcc8059-lpd4x321.dtb
dtb-y += tcc8050-topst-d3-g.dtb

dtb-y += tcc8050-subcore-lpd4x322-sv0.1.dtb
dtb-y += tcc8050-subcore-lpd4x322-sv1.0.dtb
dtb-y += tcc8053-subcore-lpd4x322-sv0.1.dtb
dtb-y += tcc8053-subcore-lpd4x322-sv1.0.dtb
dtb-y += tcc8059-subcore-lpd4x321.dtb
dtb-y += tcc8050-subcore-topst-d3-g.dtb

#
# Telechips SOC DTs
#

dtbo-y += tcc805x.dtb
dtbo-y += tcc805x-subcore.dtb

#
# Telechips DT Overlays for EVB
#

dtbo-y += overlay-tcc8050-lpd4x322-sv0.1.dtb
dtbo-y += overlay-tcc8050-lpd4x322-sv1.0.dtb
dtbo-y += overlay-tcc8053-lpd4x322-sv0.1.dtb
dtbo-y += overlay-tcc8053-lpd4x322-sv1.0.dtb
dtbo-y += overlay-tcc8059-lpd4x321.dtb
dtbo-y += overlay-tcc8050-topst-d3-g.dtb

dtbo-y += overlay-tcc8050-subcore-lpd4x322-sv0.1.dtb
dtbo-y += overlay-tcc8050-subcore-lpd4x322-sv1.0.dtb
dtbo-y += overlay-tcc8053-subcore-lpd4x322-sv0.1.dtb
dtbo-y += overlay-tcc8053-subcore-lpd4x322-sv1.0.dtb
dtbo-y += overlay-tcc8059-subcore-lpd4x321.dtb
dtbo-y += overlay-tcc8050-subcore-topst-d3-g.dtb

# Generate __symbols__ node for SOC DTs and DT overlays
$(foreach dtbo,$(dtbo-y),$(eval DTC_FLAGS_$(basename $(notdir $(dtbo))) += -@))
dtb-y += $(dtbo-y)
