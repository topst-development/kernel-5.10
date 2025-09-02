/****************************************************************************
 *
 * Copyright (C) 2018 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef TCC_SDR_H
#define TCC_SDR_H

#include "tcc_sdr_hw.h"
#include "tcc_sdr_dai.h"
#include "tcc_sdr_adma.h"

//This is for PCM mode
#define DEFAULT_MCLK_DIV \
	(4u) //4, 6, 8, 16, 24, 32, 48, 64
#define DEFAULT_BCLK_RATIO \
	(32u) //32fs,48fs,64fs

//This is for I/Q mode
#define IQ_MODE_DEFAULT_CHANNEL \
	2u
#define IQ_MODE_DEFAULT_BITMODE \
	TCC_IQ_BITMODE_16
#define IQ_MODE_DEFAULT_PERIOD_DIV \
	16u
#define DIGITAL_IQ_FIFO_THRESHOLD \
	(128u)	//64, 128, 256

//This is for PCM mode
#define PCM_MODE_DEFAULT_CHANNEL \
	2u
#define PCM_MODE_DEFAULT_BITMODE \
	16u
#define PCM_MODE_DEFAULT_PERIOD_DIV \
	128u

#define TCC_SDR_BUFFER_SZ_MAX \
	2097152u //(2*1024*1024) Bytes
#define TCC_SDR_BUFFER_SZ_MIN \
	(1024u) //Bytes

#define TCC_SDR_PERIOD_SZ_MAX \
	262144u //(8192*32) Bytes
#define TCC_SDR_PERIOD_SZ_IQ_MIN \
	(512u) //Bytes
#define TCC_SDR_PERIOD_SZ_PCM_MIN \
	(256u) //Bytes

#define	IOCTL_SDR_MAGIC \
	('S')
#define	SDR_SET_PARAMS \
	_IO(IOCTL_SDR_MAGIC, 0u)
#if 0
#define	SDR_TX_START \
	_IO(IOCTL_SDR_MAGIC, 1u)
#define	SDR_TX_STOP \
	_IO(IOCTL_SDR_MAGIC, 2u)
#endif
#define	SDR_RX_START \
	_IO(IOCTL_SDR_MAGIC, 3u)
#define	SDR_RX_STOP \
	_IO(IOCTL_SDR_MAGIC, 4u)
#define	SDR_IQ_MODE_RX_DAI \
	_IO(IOCTL_SDR_MAGIC, 5u)
#define	SDR_PCM_MODE_RX_DAI \
	_IO(IOCTL_SDR_MAGIC, 6u)
#define SDR_GET_VALID_BYTES \
	_IO(IOCTL_SDR_MAGIC, 7u)

enum {
	SDR_BIT_POLARITY_POSITIVE_EDGE = 0u,
	SDR_BIT_POLARITY_NEGATIVE_EDGE = 1u,
};

struct sdr_param {
	unsigned int sdr_sample_rate;
	unsigned int sdr_iq_mode;
	unsigned int sdr_bit_mode;
	unsigned int sdr_bit_polarity;
	unsigned int sdr_bufferbytes;	//It should be pow of 2.
	unsigned int sdr_channel;
	unsigned int sdr_periodbytes;	//It should be multiple of 32.
	unsigned int reserved3[3];
};

struct sdr_rx_buf_param {
	char *sdr_buf;
	uint32_t read_count;
	unsigned int sdr_port_index;
};

#endif /*_TCC_SDR_H_*/
