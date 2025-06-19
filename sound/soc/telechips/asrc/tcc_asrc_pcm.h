/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_ASRC_PCM_H
#define TCC_ASRC_PCM_H

#include <linux/platform_device.h>
#include "tcc_asrc_drv.h"

extern const struct snd_pcm_ops tcc_asrc_pcm_ops;
extern int tcc_asrc_pcm_open(struct snd_soc_component *component,
		struct snd_pcm_substream *substream);
extern int tcc_asrc_pcm_close(struct snd_soc_component *component,
		struct snd_pcm_substream *substream);
extern int tcc_asrc_pcm_ioctl(struct snd_soc_component *component,
		struct snd_pcm_substream *substream,
		unsigned int cmd, void *arg);
extern int tcc_asrc_pcm_mmap(
		struct snd_soc_component *component,
		struct snd_pcm_substream *substream,
		struct vm_area_struct *vma);
extern int tcc_asrc_pcm_hw_params(
		struct snd_soc_component *component,
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params);
extern int tcc_asrc_pcm_hw_free( struct snd_soc_component *component,
		struct snd_pcm_substream *substream);
extern int tcc_asrc_pcm_prepare(struct snd_soc_component *component,
		struct snd_pcm_substream *substream);
extern int tcc_asrc_pcm_trigger(struct snd_soc_component *component,
		struct snd_pcm_substream *substream, int cmd);
extern snd_pcm_uframes_t tcc_asrc_pcm_pointer(
		struct snd_soc_component *component,
		struct snd_pcm_substream *substream);
extern int tcc_asrc_pcm_new(struct snd_soc_component *component,
		struct snd_soc_pcm_runtime *rtd);
extern void tcc_asrc_pcm_free_dma_buffers(struct snd_soc_component *component,
		struct snd_pcm *pcm);
extern int tcc_pl080_asrc_pcm_isr_ch(const struct tcc_asrc_t *asrc,
		uint32_t asrc_pair);

#endif //TCC_ASRC_PCM_H
