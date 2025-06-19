# SPDX-License-Identifier: GPL-2.0 OR MIT
#
# (C) COPYRIGHT 2021-2023 ARM Limited or its affiliates. All rights reserved.
#
#

KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build
KDIR ?= $(KERNEL_SRC)
M ?= $(shell pwd)

ifeq ($(KDIR),)
    $(error Must specify KDIR to point to the kernel to target))
endif

all:
	$(MAKE) -C $(KDIR) M=$(M) modules

modules_install:
	$(MAKE) -C $(KDIR) M=$(M) $(MAKE_ARGS) modules_install

clean:
	$(MAKE) -C $(KDIR) M=$(M) $(MAKE_ARGS) clean
