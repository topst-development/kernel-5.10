/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_ASRC_DAI_H
#define TCC_ASRC_DAI_H

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/errno.h>

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/io.h>

#include <sound/soc.h>

int tcc_asrc_dai_drvinit(struct platform_device *pdev);

#define TCC_ASRC_CLKID_PERI_DAI_RATE\
	(0x0)
#define TCC_ASRC_CLKID_PERI_DAI_FORMAT\
	(0x1)
#define TCC_ASRC_CLKID_PERI_DAI\
	(0x2)

int mcaudio0_mux_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int mcaudio0_mux_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int mcaudio1_mux_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int mcaudio1_mux_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int mcaudio2_mux_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int mcaudio2_mux_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int mcaudio3_mux_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int mcaudio3_mux_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);

#if defined(CONFIG_ARCH_TCC805X) || defined(CONFIG_ARCH_TCC806X)
int pair0_fifo_in_size_set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int pair0_fifo_in_size_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int pair1_fifo_in_size_set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int pair1_fifo_in_size_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int pair2_fifo_in_size_set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int pair2_fifo_in_size_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int pair3_fifo_in_size_set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int pair3_fifo_in_size_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
#endif

int pair0_fade_in_time_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int pair0_fade_in_time_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int pair1_fade_in_time_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int pair1_fade_in_time_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int pair2_fade_in_time_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int pair2_fade_in_time_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int pair3_fade_in_time_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int pair3_fade_in_time_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);

int pair0_fade_out_time_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int pair0_fade_out_time_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int pair1_fade_out_time_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int pair1_fade_out_time_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int pair2_fade_out_time_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int pair2_fade_out_time_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int pair3_fade_out_time_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
int pair3_fade_out_time_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
#endif //TCC_ASRC_DAI_H
