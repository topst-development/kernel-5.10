/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_ASRC_M2M_H
#define TCC_ASRC_M2M_H

#include "tcc_asrc_m2m_ioctl.h"

struct tcc_asrc_t;
struct volume_ramp_t;


extern int tcc_asrc_m2m_drvinit(struct platform_device *pdev);
extern int tcc_pl080_asrc_m2m_txisr_ch(struct tcc_asrc_t *asrc,	uint32_t asrc_pair);
extern int tcc_asrc_m2m_start(
	struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	enum tcc_asrc_drv_bitwidth_t src_bitwidth,
	enum tcc_asrc_drv_bitwidth_t dst_bitwidth,
	enum tcc_asrc_drv_ch_t channels,
	uint32_t ratio_shift22);
extern int tcc_asrc_m2m_stop(struct tcc_asrc_t *asrc, uint32_t asrc_pair);
extern int tcc_asrc_m2m_push_data(
	struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	const void *data,
	uint32_t size);
extern int tcc_asrc_m2m_pop_data(
	struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	void *data,
	uint32_t size);

extern int tcc_asrc_m2m_volume_gain(
	struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	uint32_t volume_gain);

extern int tcc_asrc_m2m_volume_ramp(
	struct tcc_asrc_t *asrc,
	uint32_t asrc_pair,
	struct volume_ramp_t *volume_ramp);

#endif /*TCC_ASRC_M2M_H*/
