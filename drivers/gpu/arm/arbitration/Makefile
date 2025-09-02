# SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
#
# (C) COPYRIGHT 2022-2023 ARM Limited. All rights reserved.
#
# This program is free software and is provided to you under the terms of the
# GNU General Public License version 2 as published by the Free Software
# Foundation, and any use by you of this program is subject to the terms
# of such GNU license.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, you can access it online at
# http://www.gnu.org/licenses/gpl-2.0.html.
#
#

#
# Default configuration values
#
# Dependency resolution is done through statements as Kconfig
# is not supported for out-of-tree builds.
#

CONFIG_MALI_MIDGARD ?= m
ifeq ($(CONFIG_MALI_MIDGARD),m)
    CONFIG_MALI_PLATFORM_NAME ?= "devicetree"
    CONFIG_MALI_ARBITRATION ?= n
    CONFIG_MALI_PARTITION_MANAGER ?= n

    ifeq ($(origin CONFIG_MALI_ABITER_MODULES), undefined)
        CONFIG_MALI_ARBITER_MODULES := $(CONFIG_MALI_ARBITRATION)
    endif

    ifeq ($(origin CONFIG_MALI_GPU_POWER_MODULES), undefined)
        CONFIG_MALI_GPU_POWER_MODULES := $(CONFIG_MALI_ARBITRATION)
    endif

    ifeq ($(CONFIG_XEN),y)
        ifneq ($(CONFIG_MALI_ARBITER_SUPPORT), n)
            CONFIG_MALI_XEN ?= m
        endif
    endif

    #
    # Expert/Debug/Test released configurations
    #
    ifneq ($(CONFIG_MALI_EXPERT), y)
        # Prevent misuse when CONFIG_MALI_EXPERT=n
        CONFIG_MALI_DEBUG = n
    endif

    ifeq ($(CONFIG_MALI_DEBUG), y)
        CONFIG_MALI_KUTF ?= y
    else
        # Prevent misuse when CONFIG_MALI_DEBUG=n
        CONFIG_MALI_KUTF = n
    endif
else
    # Prevent misuse when CONFIG_MALI_MIDGARD=n
    CONFIG_MALI_ARBITRATION = n
    CONFIG_MALI_ARBITER_MODULES = n
    CONFIG_MALI_GPU_POWER_MODULES = n
    CONFIG_MALI_KUTF = n
endif

# All Mali CONFIG should be listed here
CONFIGS += \
    CONFIG_MALI_ARBITER_SUPPORT \
    CONFIG_MALI_ARBITRATION \
    CONFIG_MALI_ARBITER_MODULES \
    CONFIG_MALI_GPU_POWER_MODULES \
    CONFIG_MALI_PARTITION_MANAGER \
    CONFIG_MALI_XEN

CONFIGS += \
    CONFIG_MALI_MIDGARD \
    CONFIG_MALI_EXPERT \
    CONFIG_MALI_DEBUG \
    CONFIG_MALI_KUTF



ifeq ($(CONFIG_GCOV_KERNEL),y)
    CFLAGS_MODULE += $(call cc-option, -ftest-coverage)
    CFLAGS_MODULE += $(call cc-option, -fprofile-arcs)
    EXTRA_CFLAGS += -DGCOV_PROFILE=1
endif

ifeq ($(CONFIG_MALI_KCOV),y)
    CFLAGS_MODULE += $(call cc-option, -fsanitize-coverage=trace-cmp)
    EXTRA_CFLAGS += -DKCOV=1
    EXTRA_CFLAGS += -DKCOV_ENABLE_COMPARISONS=1
endif
