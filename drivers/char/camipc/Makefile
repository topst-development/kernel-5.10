# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2023 Telechips Inc.
#
ifeq ($(CONFIG_TCC_IPI_PROTOCOL),y)
	TELECHIPS_CAMIPC_API = camipc_ipi_protocol/camipc_ipi_protocol
endif
ifeq ($(CONFIG_TCC_MAILBOX_CLIENT),y)
	TELECHIPS_CAMIPC_API = camipc_mailbox_client/camipc_mailbox_client
endif
ifeq ($(CONFIG_TCC_MULTI_MAILBOX),y)
	TELECHIPS_CAMIPC_API = camipc_multi_mbox/camipc_multi_mbox
endif

telechips_camipc-objs := \
	camipc.o \
	$(TELECHIPS_CAMIPC_API).o

obj-$(CONFIG_TELECHIPS_CAMIPC) += telechips_camipc.o