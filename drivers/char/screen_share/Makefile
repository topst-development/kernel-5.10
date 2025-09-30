ifneq ($(filter y m,$(CONFIG_TCC_MAILBOX_CLIENT) $(CONFIG_TCC_MAILBOX_CLIENT_MODULE)),)
ccflags-y += -I$(srctree)/drivers/char/screen_share/ipc/mbox_client
else 
ifneq ($(filter y m,$(CONFIG_TCC_IPI_PROTOCOL)),)
ccflags-y += -I$(srctree)/drivers/char/screen_share/ipc/ipi
else
ccflags-y += -I$(srctree)/drivers/char/screen_share/ipc/multi_mbox
endif
endif
ccflags-y += -I$(srctree)/drivers/char/screen_share
obj-$(CONFIG_TELECHIPS_SCREEN_SHARE_DRV) += tcc_screen_share.o
tcc_screen_share-$(CONFIG_TELECHIPS_SCREEN_SHARE_DRV) += tcc_scrshare.o tcc_scrshare_overlay.o