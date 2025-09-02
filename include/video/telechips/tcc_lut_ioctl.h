/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_LUT_IOCTL_H
#define TCC_LUT_IOCTL_H

#define LUT_IOC_MAGIC		'l'

enum {
	LUT_DEV0 = 0,
	LUT_DEV1 = 1,
	LUT_DEV2 = 2,
	LUT_COMP0 = 3,
	LUT_COMP1 = 4,
	LUT_COMP2 = 5,
	LUT_COMP3 = 6,
	LUT_DEV3 = 7,
	LUT_DEV4 = 8,
};

enum {
	PLUGIN_INDEX_RDMA00 = 0,	/*0x00   VRDMA00 WMIX0 path */
	PLUGIN_INDEX_RDMA01,		/*0x00   VRDMA00 WMIX0 path */
	PLUGIN_INDEX_RDMA02,		/*0x02   VRDMA02 WMIX0 path */
	PLUGIN_INDEX_RDMA03,		/*0x03   VRDMA03 WMIX0 path */
	PLUGIN_INDEX_RDMA04,		/*0x04   GRDMA04 WMIX1 path */
	PLUGIN_INDEX_RDMA05,		/*0x05   GRDMA05 WMIX1 path */
	PLUGIN_INDEX_RDMA06,		/*0x06   VRDMA06 WMIX1 path */
	PLUGIN_INDEX_RDMA07,		/*0x07   VRDMA07 WMIX1 path */
	PLUGIN_INDEX_RDMA08,		/*0x08   GRDMA08 WMIX2 path */
	PLUGIN_INDEX_RDMA09,		/*0x09   GRDMA09 WMIX2 path */
	PLUGIN_INDEX_RDMA10 = 10,	/*0x0A   VRDMA10 WMIX2 path */
	PLUGIN_INDEX_RDMA11,		/*0x0B   VRDMA11 WMIX2 path */
	PLUGIN_INDEX_RDMA12,		/*0x0C   VRDMA12 WMIX3 path */
	PLUGIN_INDEX_RDMA13,		/*0x0D   GRDMA13 WMIX3 path */
	PLUGIN_INDEX_RDMA14,		/*0x0E   VRDMA14 WMIX4 path */
	PLUGIN_INDEX_RDMA15,		/*0x0F   GRDMA15 WMIX4 path */
	PLUGIN_INDEX_VIN00,			/*0x10   VIDEOIN0 WMIX5 path */
	PLUGIN_INDEX_RDMA16,		/*0x11   GRDMA16 WMIX5 path */
	PLUGIN_INDEX_VIN01,			/*0x12   VIDEOIN1 WMIX6 path */
	PLUGIN_INDEX_RDMA17,		/*0x13   GRDMA17 WMIX6 path */
	PLUGIN_INDEX_WDMA00 = 20,	/*0x14   WMIX0 VWDMA0 path */
	PLUGIN_INDEX_WDMA01,		/*0x15   WMIX1 VWDMA1 path */
	PLUGIN_INDEX_WDMA02,		/*0x16   WMIX2V WDMA2 path */
	PLUGIN_INDEX_WDMA03,		/*0x17   WMIX3 VWDMA3 path */
	PLUGIN_INDEX_WDMA04,		/*0x18   WMIX4 VWDMA4 path */
	PLUGIN_INDEX_WDMA05,		/*0x19   WMIX5 VWDMA5 path */
	PLUGIN_INDEX_WDMA06,		/*0x1A   WMIX5 VWDMA6 path */
	PLUGIN_INDEX_WDMA07,		/*0x1B   WMIX6 VWDMA7 path */
	PLUGIN_INDEX_WDMA08,		/*0x1C   WMIX6 VWDMA8 path */
	PLUGIN_INDEX_MAX,
};

struct VIOC_LUT_VALUE_SET {
	unsigned int Gamma[256];
	unsigned int lut_number;
};

/**
 * Structure that interfaces with the IOCTL of the tcc_lut driver.
 * This is an extension of VIOC_LUT_VALUE_SET, It has the additional
 * variable to provides information of lut_size
 */
struct VIOC_LUT_VALUE_SET_EX {
	/** lookup table size */
	unsigned int lut_size;
	/** lookup table id */
	unsigned int lut_number;
	/** lookup parameter bit[ :0] table 0: rgb-table, 1: y-table */
	unsigned int param;
	/** lookup table */
	unsigned int Gamma[1024];
};

struct VIOC_LUT_PLUG_IN_SET {
	unsigned int enable;
	unsigned int lut_number;	//enum VIOC_LUT_NUM
	unsigned int lut_plug_in_ch;	//ex :VIOC_LUT_RDMA_00
};

struct VIOC_LUT_ONOFF_SET {
	unsigned int lut_onoff;
	unsigned int lut_number;	//enum VIOC_LUT_NUM
};

#define TCC_LUT_SET 		_IOW(LUT_IOC_MAGIC, 2, struct VIOC_LUT_VALUE_SET)
#define TCC_LUT_PLUG_IN 	_IOW(LUT_IOC_MAGIC, 3, struct VIOC_LUT_PLUG_IN_SET)
#define TCC_LUT_ONOFF		_IOW(LUT_IOC_MAGIC, 4, struct VIOC_LUT_ONOFF_SET)

#define TCC_LUT_GET_DEPTH	_IOR(LUT_IOC_MAGIC, 20, unsigned int)
#define TCC_LUT_SET_EX		_IOW(LUT_IOC_MAGIC, 21, struct VIOC_LUT_VALUE_SET_EX)

#endif /* TCC_LUT_IOCTL_H */
