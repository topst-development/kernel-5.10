// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __MAX96712_REG_H__
#define __MAX96712_REG_H__

/*
 * MAX96712 REG
 */
#define MAX96712_LINK_EN_A			(1 << 0)
#define MAX96712_LINK_EN_B			(1 << 1)
#define MAX96712_LINK_EN_C			(1 << 2)
#define MAX96712_LINK_EN_D			(1 << 3)

#define MAX96712_GMSL1_A			(0 << 4)
#define MAX96712_GMSL1_B			(0 << 5)
#define MAX96712_GMSL1_C			(0 << 6)
#define MAX96712_GMSL1_D			(0 << 7)

#define MAX96712_GMSL2_A			(1 << 4)
#define MAX96712_GMSL2_B			(1 << 5)
#define MAX96712_GMSL2_C			(1 << 6)
#define MAX96712_GMSL2_D			(1 << 7)

#define MAX96712_GMSL2_ABCD					\
		(MAX96712_GMSL2_A | MAX96712_GMSL2_B |		\
		 MAX96712_GMSL2_C | MAX96712_GMSL2_D)

#define MAX96712_GMSL1_4CH					\
		(MAX96712_GMSL1_A | MAX96712_GMSL1_B |		\
		 MAX96712_GMSL1_C | MAX96712_GMSL1_D |		\
		 MAX96712_LINK_EN_A | MAX96712_LINK_EN_B |	\
		 MAX96712_LINK_EN_C | MAX96712_LINK_EN_D)

#define MAX96712_GMSL1_2CH					\
		(MAX96712_GMSL1_A | MAX96712_GMSL1_B |		\
		 MAX96712_GMSL1_C | MAX96712_GMSL1_D |		\
		 MAX96712_LINK_EN_A | MAX96712_LINK_EN_B)

#define MAX96712_GMSL1_1CH					\
		(MAX96712_GMSL1_A | MAX96712_GMSL1_B |		\
		 MAX96712_GMSL1_C | MAX96712_GMSL1_D |		\
		 MAX96712_LINK_EN_A)

#define MAX96712_GMSL2_2CH					\
		(MAX96712_GMSL2_A | MAX96712_GMSL2_B |		\
		 MAX96712_GMSL2_C | MAX96712_GMSL2_D |		\
		 MAX96712_LINK_EN_A | MAX96712_LINK_EN_B)

#define MAX96712_REG_GMSL1_A_FWDCCEN		(0x0B04)
#define MAX96712_REG_GMSL1_B_FWDCCEN		(0x0C04)
#define MAX96712_REG_GMSL1_C_FWDCCEN		(0x0D04)
#define MAX96712_REG_GMSL1_D_FWDCCEN		(0x0E04)

#define MAX96712_GMSL1_FWDCC_ENABLE		(0x03)
#define MAX96712_GMSL1_FWDCC_DISABLE		(0x00)

#define MAX96712_REG_STATUS_A			(0x0BCB)
#define MAX96712_REG_STATUS_B			(0x0CCB)
#define MAX96712_REG_STATUS_C			(0x0DCB)
#define MAX96712_REG_STATUS_D			(0x0ECB)

#define MAX96712_VAL_STATUS			(0x01)

const struct reg_sequence max96712_reg_init_hd_ar0147[] = {
	/*
	 * Step 1 - GMSL Link Initialization
	 *	Link Enable (register 0x06)
	 *	Tx/Rx Rate Selection (register 0x10/0x11)
	 */

	{0x0006, 0x00, 0},		/* Select GMSL1 all links,
					 * All link disable
					 */

	{0x0010, 0x11, 0},		/* Link A/B 3Gbps mode */
	{0x0011, 0x11, 0},		/* Link C/D 3Gbps mode */

					/* Set mode of each link(A - D) */
	{0x0B06, 0xEF, 0},		/* HIM = 1 */
	{0x0C06, 0xEF, 0},		/* HIM = 1 */
	{0x0D06, 0xEF, 0},		/* HIM = 1 */
	{0x0E06, 0xEF, 0},		/* HIM = 1 */

	{0x0B07, 0xA4, 0},		/* DBL BWS and HVEN = 1 */
	{0x0C07, 0xA4, 0},		/* DBL BWS and HVEN = 1 */
	{0x0D07, 0xA4, 0},		/* DBL BWS and HVEN = 1 */
	{0x0E07, 0xA4, 0},		/* DBL BWS and HVEN = 1 */

	{0x0B0F, 0x01, 0},		/* DE_EN = 0 */
	{0x0C0F, 0x01, 0},		/* DE_EN = 0 */
	{0x0D0F, 0x01, 0},		/* DE_EN = 0 */
	{0x0E0F, 0x01, 0},		/* DE_EN = 0 */

	//0x04,0x90,0x14,0xC4,0x04,  //REV FAST GMSL1(disable)

	{0x0B0D, 0x80, 0},		/* Local ACK for pipe 0 EN */
	{0x0C0D, 0x80, 0},		/* Local ACK for pipe 1 EN */
	{0x0D0D, 0x80, 0},		/* Local ACK for pipe 2 EN */
	{0x0E0D, 0x80, 0},		/* Local ACK for pipe 3 EN */

	{0x0018, 0x0F, 100 * 1000},	/* Oneshot reset */


	/*
	 * Step 2 – Video Pipe Selection
	 *	Video pipes are the carriers and need to be selected properly
	 *	to match the video streams coming from serializers.
	 */
	{0x00F0, 0x62, 0},		/* pipe Z in link A to video pipe 0 */
					/* pipe Z in link B to video pipe 1 */
	{0x00F1, 0xEA, 0},		/* pipe Z in link C to video pipe 2 */
					/* pipe Z in link D to video pipe 3 */
	{0x00F4, 0x0F, 0},		/* Turn on pipe 0 - 3 */

	/*
	 * Step 3 – Software override
	 *	Parallel data does not contain datatypes.
	 *	Manual software override is required when
	 *	converting data from parallel to MIPI CSI-2.
	 *	Three overrides are needed for each stream:
	 *		Datatype, Virtual channel, and Bits-per-pixe
	 */
	{0x040B, 0x60, 0},		/* CSI Out Disable and */
	{0x0411, 0x6C, 0},		/* set BPP as 0x0C(DT 0x2C) for */
	{0x0412, 0x30, 0},		/* pipe 0 - 3 */
					/* Initial VC */
	{0x040C, 0x00, 0},		/* VC=00 for pipe0 & pipe1 */
	{0x040D, 0x00, 0},		/* VC=00 for pipe2 & pipe3 */

	{0x040E, 0xAC, 0},		/* Set Data Type as RAW12(0x2C) for */
	{0x040F, 0xBC, 0},		/* pipe 0 - 3 */
	{0x0410, 0xB0, 0},

	//e YU_10_MUX mode for pipe 0-3. It is not needed.
	//0x041A 0xF0


	/*
	 * Step 4 – Video Pipe to MIPI Controller Mapping
	 * (video pipe - mapping block - routing block - mipi ctrls / PHYs)
	 *	Map enable bit for each Source and Destination.
	 *	Map Destination MIPI Controller.
	 *	Map Source virtual channel and data type.
	 *	Map Destination virtual channel and data type.
	 */
					/* video pipe 0 */
	{0x090B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x092D, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x090D, 0x2C, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x090E, 0x2C, 0},		/* mapping block 0's DST
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x090F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0910, 0x00, 0},		/* mapping block 1's DST
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0911, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0912, 0x01, 0},		/* mapping block 2's DST
					 * VC: 0 / DT: 0x01 (FS)
					 */

					/* video pipe 1 */
	{0x094B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x096D, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x094D, 0x2C, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x094E, 0x6C, 0},		/* mapping block 0's DST
					 * VC: 1 / DT: 0x2C (RAW12)
					 */
	{0x094F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0950, 0x40, 0},		/* mapping block 1's DST
					 * VC: 1 / DT: 0x00 (FS)
					 */
	{0x0951, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0952, 0x41, 0},		/* mapping block 2's DST
					 * VC: 1 / DT: 0x01 (FS)
					 */

					/* video pipe 2 */
	{0x098B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x09AD, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x098D, 0x2C, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x098E, 0xAC, 0},		/* mapping block 0's DST
					 * VC: 2 / DT: 0x2C (RAW12)
					 */
	{0x098F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0990, 0x80, 0},		/* mapping block 1's DST
					 * VC: 2 / DT: 0x00 (FS)
					 */
	{0x0991, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0992, 0x81, 0},		/* mapping block 2's DST
					 * VC: 2 / DT: 0x01 (FS)
					 */

					/* video pipe 3 */
	{0x09CB, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x09ED, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x09CD, 0x2C, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x09CE, 0xEC, 0},		/* mapping block 0's DST
					 * VC: 3 / DT: 0x2C (RAW12)
					 */
	{0x09CF, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x09D0, 0xC0, 0},		/* mapping block 1's DST
					 * VC: 3 / DT: 0x00 (FS)
					 */
	{0x09D1, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x09D2, 0xC1, 0},		/* mapping block 2's DST
					 * VC: 3 / DT: 0x01 (FS)
					 */


	/*
	 * Step 5 – MIPI PHY Setting
	 *	5 different modes for D-PHY and C-PHY. Set one of the bits in
	 *	register 0x8A0 [4:0] exclusively to select the mode.
	 *		"(ports) x (data lanes) mode"
	 *		4x2 mode(bit0): four ports with 2-lanes each
	 *		2x4 mode(bit2): two ports with 4-lanes each
	 *		1x4a + 2x2 mode(bit3): one port with 4-lanes(PHY0, 1),
	 *					two ports with 2-lanes each
	 *		1x4b + 2x2 mode(bit4): one port with 4-lanes(PHY2, 3),
	 *					two ports with 2-lanes each
	 *
	 *	In x2 mode, 4 MIPI PHYs work independently. Each PHY has its
	 *	own clock pins.
	 *
	 *	In x4 mode, 2 of the 4 MIPI PHYs are glued together as 4-lane.
	 *	PHY1 and PHY2 are the master PHYs providing MIPI clock for
	 *	port A and B, respectively.
	 */
	{0x08A0, 0x04, 0},		/* Set Des in 2x4 mode */
					/* Set Lane Mapping for 4-lane */
	{0x08A3, 0xE4, 0},		/*
					 * PHY0
					 * data lane 0 - data lane 0
					 * data lane 1 - data lane 1
					 *
					 * PHY1
					 * data lane 0 - data lane 2
					 * data lane 1 - data lane 3
					 */
	{0x08A4, 0xE4, 0},		/*
					 * PHY2
					 * data lane 0 - data lane 0
					 * data lane 1 - data lane 1
					 *
					 * PHY3
					 * data lane 0 - data lane 2
					 * data lane 1 - data lane 3
					 */

					/* Set 4 lane D-PHY (default value) */
	{0x090A, 0xC0, 0},		/* 4 lanes, DPHY mode, 2 bit VC */
	{0x094A, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */
	{0x098A, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */
	{0x09CA, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */

					/* Turn on all MIPI PHYs */
	{0x08A2, 0xF4, 0},		/* enable PHY0 - 3 */
					/* Set data rate to be 900Mbps/lane
					 * Set software override
					 */
	{0x0415, 0xE9, 0},
	{0x0418, 0xE9, 0},

	{0x0006, 0x0F, 0},		/* GMSL1 mode for all Links and
					 * All LINK enabled.
					 */

	{0x0018, 0x0F, 100 * 1000},	/* reset oneshot */
};

const struct reg_sequence max96712_reg_init_fhd[] = {
	/*
	 * CSI DESKEW Function
	 *
	 * Quad deserializers also support optional periodic de-skew as
	 * described by the MIPI Alliance.
	 * The D-PHY de-skew mechanism is only relevant to lane speeds
	 * greater than 1.5Gbps per lane.
	 * All settings must be configured before video streams are received.
	 * After video lock, the MIPI Tx clock lane initiates automatically and
	 * an automatic initial de-skew pattern is generated before
	 * the first HS data transmission.
	 *
	 * For system flexibility, an additional initial deskew pattern
	 * can be inserted by changing DE-SKEW_INIT bit 5 any time after
	 * the clock lane is enabled.
	 *
	 * The periodic de-skew is initiated by setting bit 7 of
	 * DE-SKEW_PER[7:0]
	 * The period interval has a programmable range from every 1 to 128
	 * frames.
	 *
	 * SOC should enable DE_SKEW also
	 */
	{0x0943, 0x80, 0},		/* DE_SKEW_INIT
					 * Enable auto initial de-skew packets
					 * with the minimum width 32K UI
					 */
	{0x0944, 0x91, 0},		/* DE_SKEW_PER
					 * Enable periodic de-skew packets
					 * with width 2K UI every 4 frames
					 */


	/*
	 * Step 1 - GMSL Link Initialization
	 *	Link Enable (register 0x06)
	 *	Tx/Rx Rate Selection (register 0x10/0x11)
	 */
#if 0	/* this causes i2c fail of cxd5700 */
	{0x0006, 0x00, 0},		/* Select GMSL1 all links,
					 * All link disable
					 */
#endif

	{0x0010, 0x11, 0},		/* Link A/B 3Gbps mode */
	{0x0011, 0x11, 0},		/* Link C/D 3Gbps mode */

	/* BWS=0, HIBW=1, DRS=0 */
	{0x0B05, 0x79, 0},
	{0x0C05, 0x79, 0},
	{0x0D05, 0x79, 0},
	{0x0E05, 0x79, 0},

	/*
	 * choose HVD source
	 */
	{0x0B06, 0xE8, 0},
	{0x0C06, 0xE8, 0},
	{0x0D06, 0xE8, 0},
	{0x0E06, 0xE8, 0},
	// {0x0330,0x04, 0}, Default = 0x04

	/*
	 * HIBW=1
	 */
	{0x0B07, 0x08, 0},
	{0x0C07, 0x08, 0},
	{0x0D07, 0x08, 0},
	{0x0E07, 0x08, 0},

	/*
	 * enable processing HS and DE signals
	 */
	{0x0B0F, 0x09, 0},
	{0x0C0F, 0x09, 0},
	{0x0D0F, 0x09, 0},
	{0x0E0F, 0x09, 0},

	/*
	 * set local ack
	 */
	{0x0B0D, 0x80, 0},
	{0x0C0D, 0x80, 0},
	{0x0D0D, 0x80, 0},
	{0x0E0D, 0x80, 0},


	/*
	 * Step 2 – Video Pipe Selection
	 *	Video pipes are the carriers and need to be selected properly
	 *	to match the video streams coming from serializers.
	 */
	{0x00F0, 0x62, 0},		/* pipe Z in link A to video pipe 0 */
					/* pipe Z in link B to video pipe 1 */
	{0x00F1, 0xEA, 0},		/* pipe Z in link C to video pipe 2 */
					/* pipe Z in link D to video pipe 3 */
	{0x00F4, 0x0F, 0},		/* Turn on pipe 0 - 3 */

	/*
	 * Step 3 – Software override
	 *	Parallel data does not contain datatypes.
	 *	Manual software override is required when
	 *	converting data from parallel to MIPI CSI-2.
	 *	Three overrides are needed for each stream:
	 *		Datatype, Virtual channel, and Bits-per-pixe
	 */
	{0x040B, 0x80, 0},		/* CSI Out Disable and */
	{0x0411, 0x90, 0},		/* set BPP as 0x10(DT 0x1E) for */
	{0x0412, 0x40, 0},		/* pipe 0 - 3 */
					/* Initial VC */
	{0x040C, 0x00, 0},		/* VC=00 for pipe0 & pipe1 */
	{0x040D, 0x00, 0},		/* VC=00 for pipe2 & pipe3 */

	{0x040E, 0x5E, 0},		/* Set Data Type as YUV422 8-bit(0x1E)
					 *  for pipe 0 - 3
					 */
	{0x040F, 0x7E, 0},
	{0x0410, 0x7A, 0},


	/*
	 * Step 4 – Video Pipe to MIPI Controller Mapping
	 * (video pipe - mapping block - routing block - mipi ctrls / PHYs)
	 *	Map enable bit for each Source and Destination.
	 *	Map Destination MIPI Controller.
	 *	Map Source virtual channel and data type.
	 *	Map Destination virtual channel and data type.
	 */
					/* video pipe 0 */
	{0x090B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x092D, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x090D, 0x1E, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x1E (YUV422)
					 */
	{0x090E, 0x1E, 0},		/* mapping block 0's DST
					 * VC: 0 / DT: 0x1E (YUV422)
					 */
	{0x090F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0910, 0x00, 0},		/* mapping block 1's DST
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0911, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0912, 0x01, 0},		/* mapping block 2's DST
					 * VC: 0 / DT: 0x01 (FS)
					 */

					/* video pipe 1 */
	{0x094B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x096D, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x094D, 0x1E, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x1E (YUV422)
					 */
	{0x094E, 0x5E, 0},		/* mapping block 0's DST
					 * VC: 1 / DT: 0x1E (YUV422)
					 */
	{0x094F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0950, 0x40, 0},		/* mapping block 1's DST
					 * VC: 1 / DT: 0x00 (FS)
					 */
	{0x0951, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0952, 0x41, 0},		/* mapping block 2's DST
					 * VC: 1 / DT: 0x01 (FS)
					 */

					/* video pipe 2 */
	{0x098B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x09AD, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x098D, 0x1E, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x1E (YUV422)
					 */
	{0x098E, 0x9E, 0},		/* mapping block 0's DST
					 * VC: 2 / DT: 0x1E (YUV422)
					 */
	{0x098F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0990, 0x80, 0},		/* mapping block 1's DST
					 * VC: 2 / DT: 0x00 (FS)
					 */
	{0x0991, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0992, 0x81, 0},		/* mapping block 2's DST
					 * VC: 2 / DT: 0x01 (FS)
					 */

					/* video pipe 3 */
	{0x09CB, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x09ED, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x09CD, 0x1E, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x1E (YUV422)
					 */
	{0x09CE, 0xDE, 0},		/* mapping block 0's DST
					 * VC: 3 / DT: 0x1E (YUV422)
					 */
	{0x09CF, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x09D0, 0xC0, 0},		/* mapping block 1's DST
					 * VC: 3 / DT: 0x00 (FS)
					 */
	{0x09D1, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x09D2, 0xC1, 0},		/* mapping block 2's DST
					 * VC: 3 / DT: 0x01 (FS)
					 */


	/*
	 * Step 5 – MIPI PHY Setting
	 *	5 different modes for D-PHY and C-PHY. Set one of the bits in
	 *	register 0x8A0 [4:0] exclusively to select the mode.
	 *		"(ports) x (data lanes) mode"
	 *		4x2 mode(bit0): four ports with 2-lanes each
	 *		2x4 mode(bit2): two ports with 4-lanes each
	 *		1x4a + 2x2 mode(bit3): one port with 4-lanes(PHY0, 1),
	 *					two ports with 2-lanes each
	 *		1x4b + 2x2 mode(bit4): one port with 4-lanes(PHY2, 3),
	 *					two ports with 2-lanes each
	 *
	 *	In x2 mode, 4 MIPI PHYs work independently. Each PHY has its
	 *	own clock pins.
	 *
	 *	In x4 mode, 2 of the 4 MIPI PHYs are glued together as 4-lane.
	 *	PHY1 and PHY2 are the master PHYs providing MIPI clock for
	 *	port A and B, respectively.
	 */
	{0x08A0, 0x04, 0},		/* Set Des in 2x4 mode */
					/* Set Lane Mapping for 4-lane */
	{0x08A3, 0xE4, 0},		/*
					 * PHY0
					 * data lane 0 - data lane 0
					 * data lane 1 - data lane 1
					 *
					 * PHY1
					 * data lane 0 - data lane 2
					 * data lane 1 - data lane 3
					 */
	{0x08A4, 0xE4, 0},		/*
					 * PHY2
					 * data lane 0 - data lane 0
					 * data lane 1 - data lane 1
					 *
					 * PHY3
					 * data lane 0 - data lane 2
					 * data lane 1 - data lane 3
					 */

					/* Set 4 lane D-PHY (default value) */
	//{0x090A, 0x00, 0},
	{0x094A, 0xC0, 0},
	{0x098A, 0xC0, 0},
	//{0x04CA, 0x00, 0},

					/* Turn on all MIPI PHYs */
	{0x08A2, 0xF4, 0},		/* enable PHY0 - 3 */
					/* Set data rate to be 1500Mbps/lane
					 * Set software override
					 */
	{0x0415, 0xEF, 0}, // Override Enable
	{0x0418, 0xF1, 0}, // overide
	{0x041B, 0x2A, 0},


	/*
	 * etc
	 */
	{0x01DA, 0x18, 0},
	{0x01FA, 0x18, 0},
	{0x021A, 0x18, 0},
	{0x023A, 0x18, 0},

	/*
	 * Enable GMSL1 to GMSL2 color mapping (D1) and
	 * set mapping type (D[7:3]) stored below
	 */
	{0x0B96, 0x9B, 0}, //1001 1011
	{0x0C96, 0x9B, 0},
	{0x0D96, 0x9B, 0},
	{0x0E96, 0x9B, 0},

	/*
	 * Shift GMSL1 HVD
	 */
	{0x0BA7, 0x45, 0},
	{0x0CA7, 0x45, 0},
	{0x0DA7, 0x45, 0},
	{0x0EA7, 0x45, 0},

	{0x0006, 0x0F, 0},		/* GMSL1 mode for all Links and
					 * All LINK enabled.
					 */

	{0x0018, 0x0F, 100 * 1000},	/* reset oneshot */
};

const struct reg_sequence max96712_reg_init_fhd_ar0231[] = {
	/*
	 * Step 1 - GMSL Link Initialization
	 *	Link Enable (register 0x06)
	 *	Tx/Rx Rate Selection (register 0x10/0x11)
	 */

	{0x0006, 0x00, 0},		/* Select GMSL1 all links,
					 * All link disable
					 */

	{0x0010, 0x11, 0},		/* Link A/B 3Gbps mode */
	{0x0011, 0x11, 0},		/* Link C/D 3Gbps mode */

					/* Set mode of each link(A - D) */
	{0x0B06, 0xEF, 0},		/* HIM = 1 */
	{0x0C06, 0xEF, 0},		/* HIM = 1 */
	{0x0D06, 0xEF, 0},		/* HIM = 1 */
	{0x0E06, 0xEF, 0},		/* HIM = 1 */

	{0x0B07, 0x88, 0},		/* HIBW, DBL and HVEN = 1 */
	{0x0C07, 0x88, 0},		/* HIBW, DBL and HVEN = 1 */
	{0x0D07, 0x88, 0},		/* HIBW, DBL and HVEN = 1 */
	{0x0E07, 0x88, 0},		/* HIBW, DBL and HVEN = 1 */

	{0x0B0F, 0x01, 0},		/* DE_EN = 0 */
	{0x0C0F, 0x01, 0},		/* DE_EN = 0 */
	{0x0D0F, 0x01, 0},		/* DE_EN = 0 */
	{0x0E0F, 0x01, 0},		/* DE_EN = 0 */

	//0x04,0x90,0x14,0xC4,0x04,  //REV FAST GMSL1(disable)

	{0x0B0D, 0x80, 0},		/* Local ACK for pipe 0 EN */
	{0x0C0D, 0x80, 0},		/* Local ACK for pipe 1 EN */
	{0x0D0D, 0x80, 0},		/* Local ACK for pipe 2 EN */
	{0x0E0D, 0x80, 0},		/* Local ACK for pipe 3 EN */

	{0x0018, 0x0F, 100 * 1000},	/* Oneshot reset */


	/*
	 * Step 2 – Video Pipe Selection
	 *	Video pipes are the carriers and need to be selected properly
	 *	to match the video streams coming from serializers.
	 */

	/*
	 * Step 3 – Software override
	 *	Parallel data does not contain datatypes.
	 *	Manual software override is required when
	 *	converting data from parallel to MIPI CSI-2.
	 *	Three overrides are needed for each stream:
	 *		Datatype, Virtual channel, and Bits-per-pixe
	 */
	{0x040B, 0x60, 0},		/* CSI Out Disable and */
	{0x0411, 0x6C, 0},		/* set BPP as 0x0C(DT 0x2C) for */
	{0x0412, 0x30, 0},		/* pipe 0 - 3 */
					/* Initial VC */
	{0x040C, 0x00, 0},		/* VC=00 for pipe0 & pipe1 */
	{0x040D, 0x00, 0},		/* VC=00 for pipe2 & pipe3 */

	{0x040E, 0xAC, 0},		/* Set Data Type as RAW12(0x2C) for */
	{0x040F, 0xBC, 0},		/* pipe 0 - 3 */
	{0x0410, 0xB0, 0},

	//e YU_10_MUX mode for pipe 0-3. It is not needed.
	//0x041A 0xF0


	/*
	 * Step 4 – Video Pipe to MIPI Controller Mapping
	 * (video pipe - mapping block - routing block - mipi ctrls / PHYs)
	 *	Map enable bit for each Source and Destination.
	 *	Map Destination MIPI Controller.
	 *	Map Source virtual channel and data type.
	 *	Map Destination virtual channel and data type.
	 */
					/* video pipe 0 */
	{0x090B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x092D, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x090D, 0x2C, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x090E, 0x2C, 0},		/* mapping block 0's DST
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x090F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0910, 0x00, 0},		/* mapping block 1's DST
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0911, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0912, 0x01, 0},		/* mapping block 2's DST
					 * VC: 0 / DT: 0x01 (FS)
					 */

					/* video pipe 1 */
	{0x094B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x096D, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x094D, 0x2C, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x094E, 0x6C, 0},		/* mapping block 0's DST
					 * VC: 1 / DT: 0x2C (RAW12)
					 */
	{0x094F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0950, 0x40, 0},		/* mapping block 1's DST
					 * VC: 1 / DT: 0x00 (FS)
					 */
	{0x0951, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0952, 0x41, 0},		/* mapping block 2's DST
					 * VC: 1 / DT: 0x01 (FS)
					 */

					/* video pipe 2 */
	{0x098B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x09AD, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x098D, 0x2C, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x098E, 0xAC, 0},		/* mapping block 0's DST
					 * VC: 2 / DT: 0x2C (RAW12)
					 */
	{0x098F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0990, 0x80, 0},		/* mapping block 1's DST
					 * VC: 2 / DT: 0x00 (FS)
					 */
	{0x0991, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0992, 0x81, 0},		/* mapping block 2's DST
					 * VC: 2 / DT: 0x01 (FS)
					 */

					/* video pipe 3 */
	{0x09CB, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x09ED, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x09CD, 0x2C, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x09CE, 0xEC, 0},		/* mapping block 0's DST
					 * VC: 3 / DT: 0x2C (RAW12)
					 */
	{0x09CF, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x09D0, 0xC0, 0},		/* mapping block 1's DST
					 * VC: 3 / DT: 0x00 (FS)
					 */
	{0x09D1, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x09D2, 0xC1, 0},		/* mapping block 2's DST
					 * VC: 3 / DT: 0x01 (FS)
					 */


	/*
	 * Step 5 – MIPI PHY Setting
	 *	5 different modes for D-PHY and C-PHY. Set one of the bits in
	 *	register 0x8A0 [4:0] exclusively to select the mode.
	 *		"(ports) x (data lanes) mode"
	 *		4x2 mode(bit0): four ports with 2-lanes each
	 *		2x4 mode(bit2): two ports with 4-lanes each
	 *		1x4a + 2x2 mode(bit3): one port with 4-lanes(PHY0, 1),
	 *					two ports with 2-lanes each
	 *		1x4b + 2x2 mode(bit4): one port with 4-lanes(PHY2, 3),
	 *					two ports with 2-lanes each
	 *
	 *	In x2 mode, 4 MIPI PHYs work independently. Each PHY has its
	 *	own clock pins.
	 *
	 *	In x4 mode, 2 of the 4 MIPI PHYs are glued together as 4-lane.
	 *	PHY1 and PHY2 are the master PHYs providing MIPI clock for
	 *	port A and B, respectively.
	 */
	{0x08A0, 0x04, 0},		/* Set Des in 2x4 mode */
					/* Set Lane Mapping for 4-lane */
	{0x08A3, 0xE4, 0},		/*
					 * PHY0
					 * data lane 0 - data lane 0
					 * data lane 1 - data lane 1
					 *
					 * PHY1
					 * data lane 0 - data lane 2
					 * data lane 1 - data lane 3
					 */
	{0x08A4, 0xE4, 0},		/*
					 * PHY2
					 * data lane 0 - data lane 0
					 * data lane 1 - data lane 1
					 *
					 * PHY3
					 * data lane 0 - data lane 2
					 * data lane 1 - data lane 3
					 */

					/* Set 4 lane D-PHY (default value) */
	{0x090A, 0xC0, 0},		/* 4 lanes, DPHY mode, 2 bit VC */
	{0x094A, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */
	{0x098A, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */
	{0x09CA, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */

					/* Turn on all MIPI PHYs */
	{0x08A2, 0xF4, 0},		/* enable PHY0 - 3 */
					/* Set data rate to be 1500Mbps/lane
					 * Set software override
					 */
	{0x0415, 0xEF, 0},
	{0x0418, 0xEF, 0},

	{0x0006, 0x0F, 0},		/* GMSL1 mode for all Links and
					 * All LINK enabled.
					 */

	{0x0018, 0x0F, 100 * 1000},	/* reset oneshot */
};

const struct reg_sequence max96712_reg_init_qhd_imx424[] = {
	{0x0013, 0x75, 50 * 1000},	/* Device Reset */
	/*
	 * CSI DESKEW Function
	 *
	 * Quad deserializers also support optional periodic de-skew as
	 * described by the MIPI Alliance.
	 * The D-PHY de-skew mechanism is only relevant to lane speeds
	 * greater than 1.5Gbps per lane.
	 * All settings must be configured before video streams are received.
	 * After video lock, the MIPI Tx clock lane initiates automatically and
	 * an automatic initial de-skew pattern is generated before
	 * the first HS data transmission.
	 *
	 * For system flexibility, an additional initial deskew pattern
	 * can be inserted by changing DE-SKEW_INIT bit 5 any time after
	 * the clock lane is enabled.
	 *
	 * The periodic de-skew is initiated by setting bit 7 of
	 * DE-SKEW_PER[7:0]
	 * The period interval has a programmable range from every 1 to 128
	 * frames.
	 *
	 * SOC should enable DE_SKEW also
	 */
	{0x0943, 0x80, 0},		/* DE_SKEW_INIT
					 * Enable auto initial de-skew packets
					 * with the minimum width 32K UI
					 */
	{0x0944, 0x91, 0},		/* DE_SKEW_PER
					 * Enable periodic de-skew packets
					 * with width 2K UI every 4 frames
					 */

	/*
	 * Step 1 - GMSL Link Initialization
	 *	Link Enable (register 0x06)
	 *	Tx/Rx Rate Selection (register 0x10/0x11)
	 *	TODO: GMSL2 link lock sanity check
	 */
	{0x0006, 0xF3, 0},		/* Select GMSL2 all links,
					 * link A/B enable
					 */
	{0x0010, 0x22, 0},		/* Link A/B 6Gbps mode */
	{0x0011, 0x22, 0},		/* Link C/D 6Gbps mode */
	{0x0018, 0x0F, 100 * 1000},	/* Oneshot reset */
	{0x040B, 0x00, 0},		/* CSI output disabled */


	/*
	 * Step 2 – Video Pipe Selection
	 *	Video pipes are the carriers and need to be selected properly
	 *	to match the video streams coming from serializers.
	 */
	{0x00F0, 0x50, 0},		/* pipe X in link A to video pipe 0 */
					/* pipe Y in link B to video pipe 1 */
	{0x00F4, 0x03, 0},		/* Turn on pipe 0 - 1 */

#if 0
	/* TODO: Does it necessary in GMSL2 mode? */
	{0x040B, 0x60, 0},/* CSI Out Disable , BPP as 0x0C for link A*/
	{0x040E, 0xAC, 0},/* Set Data Type as RAW12(0x2C) for Link A,B */
	{0x040F, 0x0C, 0},// Set Data Type as RAW12(0x2C) for B
	{0x0411, 0x0C, 0},// Set BPP as 0xC Link B
#endif


	/*
	 * Step 3 – Video Pipe to MIPI Controller Mapping
	 * (video pipe - mapping block - routing block - mipi ctrls / PHYs)
	 *	Map enable bit for each Source and Destination.
	 *	Map Destination MIPI Controller.
	 *	Map Source virtual channel and data type.
	 *	Map Destination virtual channel and data type.
	 */
					/* video pipe 0 */
	{0x090B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x092D, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x090D, 0x2C, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x090E, 0x2C, 0},		/* mapping block 0's DST
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x090F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0910, 0x00, 0},		/* mapping block 1's DST
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0911, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0912, 0x01, 0},		/* mapping block 2's DST
					 * VC: 0 / DT: 0x01 (FS)
					 */

					/* video pipe 1 */
	{0x094B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x096D, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x094D, 0x2C, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x094E, 0x6C, 0},		/* mapping block 0's DST
					 * VC: 1 / DT: 0x2C (RAW12)
					 */
	{0x094F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0950, 0x40, 0},		/* mapping block 1's DST
					 * VC: 1 / DT: 0x00 (FS)
					 */
	{0x0951, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0952, 0x41, 0},		/* mapping block 2's DST
					 * VC: 1 / DT: 0x01 (FS)
					 */


	/*
	 * Step 4 – MIPI PHY Setting
	 *	5 different modes for D-PHY and C-PHY. Set one of the bits in
	 *	register 0x8A0 [4:0] exclusively to select the mode.
	 *		"(ports) x (data lanes) mode"
	 *		4x2 mode(bit0): four ports with 2-lanes each
	 *		2x4 mode(bit2): two ports with 4-lanes each
	 *		1x4a + 2x2 mode(bit3): one port with 4-lanes(PHY0, 1),
	 *					two ports with 2-lanes each
	 *		1x4b + 2x2 mode(bit4): one port with 4-lanes(PHY2, 3),
	 *					two ports with 2-lanes each
	 *
	 *	In x2 mode, 4 MIPI PHYs work independently. Each PHY has its
	 *	own clock pins.
	 *
	 *	In x4 mode, 2 of the 4 MIPI PHYs are glued together as 4-lane.
	 *	PHY1 and PHY2 are the master PHYs providing MIPI clock for
	 *	port A and B, respectively.
	 */
	{0x08A0, 0x04, 0},		/* Set Des in 2x4 mode */
					/* Set Lane Mapping for 4-lane */
	{0x08A3, 0xE4, 0},		/*
					 * PHY0
					 * data lane 0 - data lane 0
					 * data lane 1 - data lane 1
					 *
					 * PHY1
					 * data lane 0 - data lane 2
					 * data lane 1 - data lane 3
					 */
	{0x08A4, 0xE4, 0},		/*
					 * PHY2
					 * data lane 0 - data lane 0
					 * data lane 1 - data lane 1
					 *
					 * PHY3
					 * data lane 0 - data lane 2
					 * data lane 1 - data lane 3
					 */

					/* Set 4 lane D-PHY (default value) */
	{0x090A, 0xC0, 0},		/* 4 lanes, DPHY mode, 2 bit VC */
	{0x094A, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */
	{0x098A, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */
	{0x09CA, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */

					/* Turn on all MIPI PHYs */
	{0x08A2, 0xF4, 0},		/* enable PHY0 - 3 */
					/* Set data rate to be 1800Mbps/lane */
	//{0x0415, 0xEC, 0},		/* TODO: software override */
	{0x0415, 0x32, 0},
	{0x0418, 0x32, 0},
	{0x041B, 0x32, 0},
	{0x041E, 0x32, 0},


	/*
	 * etc
	 */
					/* GPIO settings (IMX424 reset gpio) */
	{0x0306, 0x83, 0},		/* GPIO A Tx enable */
	{0x0308, 0x40, 0},

	{0x040B, 0x00, 0},		/* CSI out diable */
	{0x0018, 0x0F, 100 * 1000},	/* reset oneshot */
	//MAX96712 force CSI out enable
	//{0x08A0, 0x84, 0},
};

const struct reg_sequence max96712_reg_init_qhd_ar0820[] = {
	{0x0013, 0x75, 50 * 1000},	/* Device Reset */
	/*
	 * CSI DESKEW Function
	 *
	 * Quad deserializers also support optional periodic de-skew as
	 * described by the MIPI Alliance.
	 * The D-PHY de-skew mechanism is only relevant to lane speeds
	 * greater than 1.5Gbps per lane.
	 * All settings must be configured before video streams are received.
	 * After video lock, the MIPI Tx clock lane initiates automatically and
	 * an automatic initial de-skew pattern is generated before
	 * the first HS data transmission.
	 *
	 * For system flexibility, an additional initial deskew pattern
	 * can be inserted by changing DE-SKEW_INIT bit 5 any time after
	 * the clock lane is enabled.
	 *
	 * The periodic de-skew is initiated by setting bit 7 of
	 * DE-SKEW_PER[7:0]
	 * The period interval has a programmable range from every 1 to 128
	 * frames.
	 *
	 * SOC should enable DE_SKEW also
	 */
	{0x0943, 0x80, 0},		/* DE_SKEW_INIT
					 * Enable auto initial de-skew packets
					 * with the minimum width 32K UI
					 */
	{0x0944, 0x91, 0},		/* DE_SKEW_PER
					 * Enable periodic de-skew packets
					 * with width 2K UI every 4 frames
					 */

	/*
	 * Step 1 - GMSL Link Initialization
	 *	Link Enable (register 0x06)
	 *	Tx/Rx Rate Selection (register 0x10/0x11)
	 *	TODO: GMSL2 link lock sanity check
	 */
	{0x0006, 0xF3, 0},		/* Select GMSL2 all links,
					 * link A/B enable
					 */
	{0x0010, 0x22, 0},		/* Link A/B 6Gbps mode */
	{0x0011, 0x22, 0},		/* Link C/D 6Gbps mode */
	{0x0018, 0x0F, 100 * 1000},	/* Oneshot reset */
	{0x040B, 0x00, 0},		/* CSI output disabled */


	/*
	 * Step 2 – Video Pipe Selection
	 *	Video pipes are the carriers and need to be selected properly
	 *	to match the video streams coming from serializers.
	 */
	{0x00F0, 0x40, 0},		/* pipe X in link A to video pipe 0 */
					/* pipe X in link B to video pipe 1 */
	{0x00F4, 0x03, 0},		/* Turn on pipe 0 - 1 */

#if 0
	/* TODO: Does it necessary in GMSL2 mode? */
	{0x040B, 0x60, 0},/* CSI Out Disable , BPP as 0x0C for link A*/
	{0x040E, 0xAC, 0},/* Set Data Type as RAW12(0x2C) for Link A,B */
	{0x040F, 0x0C, 0},// Set Data Type as RAW12(0x2C) for B
	{0x0411, 0x0C, 0},// Set BPP as 0xC Link B
#endif


	/*
	 * Step 3 – Video Pipe to MIPI Controller Mapping
	 * (video pipe - mapping block - routing block - mipi ctrls / PHYs)
	 *	Map enable bit for each Source and Destination.
	 *	Map Destination MIPI Controller.
	 *	Map Source virtual channel and data type.
	 *	Map Destination virtual channel and data type.
	 */
					/* video pipe 0 */
	{0x090B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x092D, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x090D, 0x2C, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x090E, 0x2C, 0},		/* mapping block 0's DST
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x090F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0910, 0x00, 0},		/* mapping block 1's DST
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0911, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0912, 0x01, 0},		/* mapping block 2's DST
					 * VC: 0 / DT: 0x01 (FS)
					 */

					/* video pipe 1 */
	{0x094B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x096D, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x094D, 0x2C, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x094E, 0x6C, 0},		/* mapping block 0's DST
					 * VC: 1 / DT: 0x2C (RAW12)
					 */
	{0x094F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0950, 0x40, 0},		/* mapping block 1's DST
					 * VC: 1 / DT: 0x00 (FS)
					 */
	{0x0951, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0952, 0x41, 0},		/* mapping block 2's DST
					 * VC: 1 / DT: 0x01 (FS)
					 */


	/*
	 * Step 4 – MIPI PHY Setting
	 *	5 different modes for D-PHY and C-PHY. Set one of the bits in
	 *	register 0x8A0 [4:0] exclusively to select the mode.
	 *		"(ports) x (data lanes) mode"
	 *		4x2 mode(bit0): four ports with 2-lanes each
	 *		2x4 mode(bit2): two ports with 4-lanes each
	 *		1x4a + 2x2 mode(bit3): one port with 4-lanes(PHY0, 1),
	 *					two ports with 2-lanes each
	 *		1x4b + 2x2 mode(bit4): one port with 4-lanes(PHY2, 3),
	 *					two ports with 2-lanes each
	 *
	 *	In x2 mode, 4 MIPI PHYs work independently. Each PHY has its
	 *	own clock pins.
	 *
	 *	In x4 mode, 2 of the 4 MIPI PHYs are glued together as 4-lane.
	 *	PHY1 and PHY2 are the master PHYs providing MIPI clock for
	 *	port A and B, respectively.
	 */
	{0x08A0, 0x04, 0},		/* Set Des in 2x4 mode */
					/* Set Lane Mapping for 4-lane */
	{0x08A3, 0xE4, 0},		/*
					 * PHY0
					 * data lane 0 - data lane 0
					 * data lane 1 - data lane 1
					 *
					 * PHY1
					 * data lane 0 - data lane 2
					 * data lane 1 - data lane 3
					 */
	{0x08A4, 0xE4, 0},		/*
					 * PHY2
					 * data lane 0 - data lane 0
					 * data lane 1 - data lane 1
					 *
					 * PHY3
					 * data lane 0 - data lane 2
					 * data lane 1 - data lane 3
					 */

					/* Set 4 lane D-PHY (default value) */
	{0x090A, 0xC0, 0},		/* 4 lanes, DPHY mode, 2 bit VC */
	{0x094A, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */
	{0x098A, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */
	{0x09CA, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */

					/* Turn on all MIPI PHYs */
	{0x08A2, 0xF4, 0},		/* enable PHY0 - 3 */
					/* Set data rate to be 1800Mbps/lane */
	//{0x0415, 0xEC, 0},		/* TODO: software override */
	{0x0415, 0x32, 0},
	{0x0418, 0x32, 0},
	{0x041B, 0x32, 0},
	{0x041E, 0x32, 0},

	{0x040B, 0x00, 0},		/* CSI out diable */
	{0x0018, 0x0F, 100 * 1000},	/* reset oneshot */
	//MAX96712 force CSI out enable
	//{0x08A0, 0x84, 0},
};

const struct reg_sequence max96712_reg_init_4k_ar0820[] = {
	{0x0013, 0x75, 50 * 1000},	/* Device Reset */
	/*
	 * CSI DESKEW Function
	 *
	 * Quad deserializers also support optional periodic de-skew as
	 * described by the MIPI Alliance.
	 * The D-PHY de-skew mechanism is only relevant to lane speeds
	 * greater than 1.5Gbps per lane.
	 * All settings must be configured before video streams are received.
	 * After video lock, the MIPI Tx clock lane initiates automatically and
	 * an automatic initial de-skew pattern is generated before
	 * the first HS data transmission.
	 *
	 * For system flexibility, an additional initial deskew pattern
	 * can be inserted by changing DE-SKEW_INIT bit 5 any time after
	 * the clock lane is enabled.
	 *
	 * The periodic de-skew is initiated by setting bit 7 of
	 * DE-SKEW_PER[7:0]
	 * The period interval has a programmable range from every 1 to 128
	 * frames.
	 *
	 * SOC should enable DE_SKEW also
	 */
	{0x0943, 0x80, 0},		/* DE_SKEW_INIT
					 * Enable auto initial de-skew packets
					 * with the minimum width 32K UI
					 */
	{0x0944, 0x91, 0},		/* DE_SKEW_PER
					 * Enable periodic de-skew packets
					 * with width 2K UI every 4 frames
					 */

	/*
	 * Step 1 - GMSL Link Initialization
	 *	Link Enable (register 0x06)
	 *	Tx/Rx Rate Selection (register 0x10/0x11)
	 *	TODO: GMSL2 link lock sanity check
	 */
	{0x0006, 0xF3, 0},		/* Select GMSL2 all links,
					 * link A/B enable
					 */
	{0x0010, 0x22, 0},		/* Link A/B 6Gbps mode */
	{0x0011, 0x22, 0},		/* Link C/D 6Gbps mode */
	{0x0018, 0x0F, 100 * 1000},	/* Oneshot reset */
	{0x040B, 0x00, 0},		/* CSI output disabled */

	{0x0933, 0x01, 0},  // Enable ALT MEM12 mode
	{0x0973, 0x01, 0},  // Enable ALT MEM12 mode
	{0x09B3, 0x01, 0},  // Enable ALT MEM12 mode
	{0x09F3, 0x01, 0},  // Enable ALT MEM12 mode

	/*
	 * Step 2 – Video Pipe Selection
	 *	Video pipes are the carriers and need to be selected properly
	 *	to match the video streams coming from serializers.
	 */
	{0x00F0, 0x40, 0},		/* pipe X in link A to video pipe 0 */
					/* pipe X in link B to video pipe 1 */
	{0x00F4, 0x03, 0},		/* Turn on pipe 0 - 1 */

#if 0
	/* TODO: Does it necessary in GMSL2 mode? */
	{0x040B, 0x60, 0},/* CSI Out Disable , BPP as 0x0C for link A*/
	{0x040E, 0xAC, 0},/* Set Data Type as RAW12(0x2C) for Link A,B */
	{0x040F, 0x0C, 0},// Set Data Type as RAW12(0x2C) for B
	{0x0411, 0x0C, 0},// Set BPP as 0xC Link B
#endif


	/*
	 * Step 3 – Video Pipe to MIPI Controller Mapping
	 * (video pipe - mapping block - routing block - mipi ctrls / PHYs)
	 *	Map enable bit for each Source and Destination.
	 *	Map Destination MIPI Controller.
	 *	Map Source virtual channel and data type.
	 *	Map Destination virtual channel and data type.
	 */
					/* video pipe 0 */
	{0x090B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x092D, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x090D, 0x2C, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x090E, 0x2C, 0},		/* mapping block 0's DST
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x090F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0910, 0x00, 0},		/* mapping block 1's DST
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0911, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0912, 0x01, 0},		/* mapping block 2's DST
					 * VC: 0 / DT: 0x01 (FS)
					 */

					/* video pipe 1 */
	{0x094B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x096D, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x094D, 0x2C, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x094E, 0x6C, 0},		/* mapping block 0's DST
					 * VC: 1 / DT: 0x2C (RAW12)
					 */
	{0x094F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0950, 0x40, 0},		/* mapping block 1's DST
					 * VC: 1 / DT: 0x00 (FS)
					 */
	{0x0951, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0952, 0x41, 0},		/* mapping block 2's DST
					 * VC: 1 / DT: 0x01 (FS)
					 */


	/*
	 * Step 4 – MIPI PHY Setting
	 *	5 different modes for D-PHY and C-PHY. Set one of the bits in
	 *	register 0x8A0 [4:0] exclusively to select the mode.
	 *		"(ports) x (data lanes) mode"
	 *		4x2 mode(bit0): four ports with 2-lanes each
	 *		2x4 mode(bit2): two ports with 4-lanes each
	 *		1x4a + 2x2 mode(bit3): one port with 4-lanes(PHY0, 1),
	 *					two ports with 2-lanes each
	 *		1x4b + 2x2 mode(bit4): one port with 4-lanes(PHY2, 3),
	 *					two ports with 2-lanes each
	 *
	 *	In x2 mode, 4 MIPI PHYs work independently. Each PHY has its
	 *	own clock pins.
	 *
	 *	In x4 mode, 2 of the 4 MIPI PHYs are glued together as 4-lane.
	 *	PHY1 and PHY2 are the master PHYs providing MIPI clock for
	 *	port A and B, respectively.
	 */
	{0x08A0, 0x04, 0},		/* Set Des in 2x4 mode */
					/* Set Lane Mapping for 4-lane */
	{0x08A3, 0xE4, 0},		/*
					 * PHY0
					 * data lane 0 - data lane 0
					 * data lane 1 - data lane 1
					 *
					 * PHY1
					 * data lane 0 - data lane 2
					 * data lane 1 - data lane 3
					 */
	{0x08A4, 0xE4, 0},		/*
					 * PHY2
					 * data lane 0 - data lane 0
					 * data lane 1 - data lane 1
					 *
					 * PHY3
					 * data lane 0 - data lane 2
					 * data lane 1 - data lane 3
					 */

					/* Set 4 lane D-PHY (default value) */
	{0x090A, 0xC0, 0},		/* 4 lanes, DPHY mode, 2 bit VC */
	{0x094A, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */
	{0x098A, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */
	{0x09CA, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */

					/* Turn on all MIPI PHYs */
	{0x08A2, 0xF4, 0},		/* enable PHY0 - 3 */
					/* Set data rate to be 1800Mbps/lane */
	//{0x0415, 0xEC, 0},		/* TODO: software override */
	{0x0415, 0x32, 0},
	{0x0418, 0x32, 0},
	{0x041B, 0x32, 0},
	{0x041E, 0x32, 0},

	{0x040B, 0x00, 0},		/* CSI out diable */
	{0x0018, 0x0F, 100 * 1000},	/* reset oneshot */
	//MAX96712 force CSI out enable
	//{0x08A0, 0x84, 0},
};

const struct reg_sequence max96712_reg_init_fhd_ar0239[] = {
	{0x0013, 0x75, 50 * 1000},	/* Device Reset */
	/*
	 * CSI DESKEW Function
	 *
	 * Quad deserializers also support optional periodic de-skew as
	 * described by the MIPI Alliance.
	 * The D-PHY de-skew mechanism is only relevant to lane speeds
	 * greater than 1.5Gbps per lane.
	 * All settings must be configured before video streams are received.
	 * After video lock, the MIPI Tx clock lane initiates automatically and
	 * an automatic initial de-skew pattern is generated before
	 * the first HS data transmission.
	 *
	 * For system flexibility, an additional initial deskew pattern
	 * can be inserted by changing DE-SKEW_INIT bit 5 any time after
	 * the clock lane is enabled.
	 *
	 * The periodic de-skew is initiated by setting bit 7 of
	 * DE-SKEW_PER[7:0]
	 * The period interval has a programmable range from every 1 to 128
	 * frames.
	 *
	 * SOC should enable DE_SKEW also
	 */
	{0x0943, 0x80, 0},		/* DE_SKEW_INIT
					 * Enable auto initial de-skew packets
					 * with the minimum width 32K UI
					 */
	{0x0944, 0x91, 0},		/* DE_SKEW_PER
					 * Enable periodic de-skew packets
					 * with width 2K UI every 4 frames
					 */

	/*
	 * Step 1 - GMSL Link Initialization
	 *	Link Enable (register 0x06)
	 *	Tx/Rx Rate Selection (register 0x10/0x11)
	 *	TODO: GMSL2 link lock sanity check
	 */
	{0x0006, 0xF3, 0},		/* Select GMSL2 all links,
					 * link A/B enable
					 */
	{0x0010, 0x22, 0},		/* Link A/B 6Gbps mode */
	{0x0011, 0x22, 0},		/* Link C/D 6Gbps mode */
	{0x0018, 0x0F, 100 * 1000},	/* Oneshot reset */
	{0x040B, 0x00, 0},		/* CSI output disabled */


	/*
	 * Step 2 – Video Pipe Selection
	 *	Video pipes are the carriers and need to be selected properly
	 *	to match the video streams coming from serializers.
	 */
	{0x00F0, 0x40, 0},		/* pipe X in link A to video pipe 0 */
					/* pipe X in link B to video pipe 1 */
	{0x00F4, 0x03, 0},		/* Turn on pipe 0 - 1 */

#if 0
	/* TODO: Does it necessary in GMSL2 mode? */
	{0x040B, 0x60, 0},/* CSI Out Disable , BPP as 0x0C for link A*/
	{0x040E, 0xAC, 0},/* Set Data Type as RAW12(0x2C) for Link A,B */
	{0x040F, 0x0C, 0},// Set Data Type as RAW12(0x2C) for B
	{0x0411, 0x0C, 0},// Set BPP as 0xC Link B
#endif


	/*
	 * Step 3 – Video Pipe to MIPI Controller Mapping
	 * (video pipe - mapping block - routing block - mipi ctrls / PHYs)
	 *	Map enable bit for each Source and Destination.
	 *	Map Destination MIPI Controller.
	 *	Map Source virtual channel and data type.
	 *	Map Destination virtual channel and data type.
	 */
					/* video pipe 0 */
	{0x090B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x092D, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x090D, 0x2C, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x090E, 0x2C, 0},		/* mapping block 0's DST
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x090F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0910, 0x00, 0},		/* mapping block 1's DST
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0911, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0912, 0x01, 0},		/* mapping block 2's DST
					 * VC: 0 / DT: 0x01 (FS)
					 */

					/* video pipe 1 */
	{0x094B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x096D, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x094D, 0x2C, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x094E, 0x6C, 0},		/* mapping block 0's DST
					 * VC: 1 / DT: 0x2C (RAW12)
					 */
	{0x094F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0950, 0x40, 0},		/* mapping block 1's DST
					 * VC: 1 / DT: 0x00 (FS)
					 */
	{0x0951, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0952, 0x41, 0},		/* mapping block 2's DST
					 * VC: 1 / DT: 0x01 (FS)
					 */


	/*
	 * Step 4 – MIPI PHY Setting
	 *	5 different modes for D-PHY and C-PHY. Set one of the bits in
	 *	register 0x8A0 [4:0] exclusively to select the mode.
	 *		"(ports) x (data lanes) mode"
	 *		4x2 mode(bit0): four ports with 2-lanes each
	 *		2x4 mode(bit2): two ports with 4-lanes each
	 *		1x4a + 2x2 mode(bit3): one port with 4-lanes(PHY0, 1),
	 *					two ports with 2-lanes each
	 *		1x4b + 2x2 mode(bit4): one port with 4-lanes(PHY2, 3),
	 *					two ports with 2-lanes each
	 *
	 *	In x2 mode, 4 MIPI PHYs work independently. Each PHY has its
	 *	own clock pins.
	 *
	 *	In x4 mode, 2 of the 4 MIPI PHYs are glued together as 4-lane.
	 *	PHY1 and PHY2 are the master PHYs providing MIPI clock for
	 *	port A and B, respectively.
	 */
	{0x08A0, 0x04, 0},		/* Set Des in 2x4 mode */
					/* Set Lane Mapping for 4-lane */
	{0x08A3, 0xE4, 0},		/*
					 * PHY0
					 * data lane 0 - data lane 0
					 * data lane 1 - data lane 1
					 *
					 * PHY1
					 * data lane 0 - data lane 2
					 * data lane 1 - data lane 3
					 */
	{0x08A4, 0xE4, 0},		/*
					 * PHY2
					 * data lane 0 - data lane 0
					 * data lane 1 - data lane 1
					 *
					 * PHY3
					 * data lane 0 - data lane 2
					 * data lane 1 - data lane 3
					 */

					/* Set 4 lane D-PHY (default value) */
	{0x090A, 0xC0, 0},		/* 4 lanes, DPHY mode, 2 bit VC */
	{0x094A, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */
	{0x098A, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */
	{0x09CA, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */

					/* Turn on all MIPI PHYs */
	{0x08A2, 0xF4, 0},		/* enable PHY0 - 3 */
					/* Set data rate to be 1800Mbps/lane */
	//{0x0415, 0xEC, 0},		/* TODO: software override */
	{0x0415, 0x32, 0},
	{0x0418, 0x32, 0},
	{0x041B, 0x32, 0},
	{0x041E, 0x32, 0},

	{0x040B, 0x00, 0},		/* CSI out diable */
	{0x0018, 0x0F, 100 * 1000},	/* reset oneshot */
	//MAX96712 force CSI out enable
	//{0x08A0, 0x84, 0},
};

const struct reg_sequence max96712_reg_init_fhd_ar0239_2ch[] = {
	{0x0013, 0x75, 50 * 1000},	/* Device Reset */

	/*
	 * Step 1 - GMSL Link Initialization
	 *	Link Enable (register 0x06)
	 *	Tx/Rx Rate Selection (register 0x10/0x11)
	 *	TODO: GMSL2 link lock sanity check
	 */
	{0x0006, 0xF3, 0},		/* Select GMSL2 all links,
					 * link A/B enable
					 */
	{0x0010, 0x22, 0},		/* Link A/B 6Gbps mode */
	{0x0011, 0x22, 0},		/* Link C/D 6Gbps mode */
	{0x0018, 0x0F, 100 * 1000},	/* Oneshot reset */
	{0x040B, 0x00, 0},		/* CSI output disabled */


	/*
	 * Step 2 – Video Pipe Selection
	 *	Video pipes are the carriers and need to be selected properly
	 *	to match the video streams coming from serializers.
	 */
	{0x00F0, 0x40, 0},		/* pipe X in link A to video pipe 0 */
					/* pipe X in link B to video pipe 1 */
	{0x00F4, 0x03, 0},		/* Turn on pipe 0 - 1 */

	/*
	 * Step 3 – Video Pipe to MIPI Controller Mapping
	 * (video pipe - mapping block - routing block - mipi ctrls / PHYs)
	 *	Map enable bit for each Source and Destination.
	 *	Map Destination MIPI Controller.
	 *	Map Source virtual channel and data type.
	 *	Map Destination virtual channel and data type.
	 */
					/* video pipe 0 */
	{0x090B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x092D, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x090D, 0x2C, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x090E, 0x2C, 0},		/* mapping block 0's DST
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x090F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0910, 0x00, 0},		/* mapping block 1's DST
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0911, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0912, 0x01, 0},		/* mapping block 2's DST
					 * VC: 0 / DT: 0x01 (FS)
					 */

					/* video pipe 1 */
	{0x094B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x096D, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x094D, 0x2C, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x094E, 0x6C, 0},		/* mapping block 0's DST
					 * VC: 1 / DT: 0x2C (RAW12)
					 */
	{0x094F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0950, 0x40, 0},		/* mapping block 1's DST
					 * VC: 1 / DT: 0x00 (FS)
					 */
	{0x0951, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0952, 0x41, 0},		/* mapping block 2's DST
					 * VC: 1 / DT: 0x01 (FS)
					 */


	/*
	 * Step 4 – MIPI PHY Setting
	 *	5 different modes for D-PHY and C-PHY. Set one of the bits in
	 *	register 0x8A0 [4:0] exclusively to select the mode.
	 *		"(ports) x (data lanes) mode"
	 *		4x2 mode(bit0): four ports with 2-lanes each
	 *		2x4 mode(bit2): two ports with 4-lanes each
	 *		1x4a + 2x2 mode(bit3): one port with 4-lanes(PHY0, 1),
	 *					two ports with 2-lanes each
	 *		1x4b + 2x2 mode(bit4): one port with 4-lanes(PHY2, 3),
	 *					two ports with 2-lanes each
	 *
	 *	In x2 mode, 4 MIPI PHYs work independently. Each PHY has its
	 *	own clock pins.
	 *
	 *	In x4 mode, 2 of the 4 MIPI PHYs are glued together as 4-lane.
	 *	PHY1 and PHY2 are the master PHYs providing MIPI clock for
	 *	port A and B, respectively.
	 */
	{0x08A0, 0x04, 0},		/* Set Des in 2x4 mode */
					/* Set Lane Mapping for 4-lane */
	{0x08A3, 0xE4, 0},		/*
					 * PHY0
					 * data lane 0 - data lane 0
					 * data lane 1 - data lane 1
					 *
					 * PHY1
					 * data lane 0 - data lane 2
					 * data lane 1 - data lane 3
					 */
	{0x08A4, 0xE4, 0},		/*
					 * PHY2
					 * data lane 0 - data lane 0
					 * data lane 1 - data lane 1
					 *
					 * PHY3
					 * data lane 0 - data lane 2
					 * data lane 1 - data lane 3
					 */

					/* Set 4 lane D-PHY (default value) */
	{0x090A, 0xC0, 0},		/* 4 lanes, DPHY mode, 2 bit VC */
	{0x094A, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */
	{0x098A, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */
	{0x09CA, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */

					/* Turn on all MIPI PHYs */
	{0x08A2, 0xF4, 0},		/* enable PHY0 - 3 */
	{0x0418, 0x2E, 0},		/* Set data rate to be 1400Mbps/lane */
	{0x040B, 0x00, 0},		/* CSI out diable */
	{0x0018, 0x0F, 100 * 1000},	/* reset oneshot */
};

const struct reg_sequence max96712_reg_init_fhd_3ch[] = {
	/*
	 * Step 1 - GMSL Link Initialization
	 *	Link Enable (register 0x06)
	 *	Tx/Rx Rate Selection (register 0x10/0x11)
	 */
	{0x0010, 0x11, 0},		/* Link A/B 3Gbps mode */
	{0x0011, 0x11, 0},		/* Link C/D 3Gbps mode */

	/* BWS=0, HIBW=1, DRS=0 */
	{0x0B05, 0x79, 0},
	{0x0C05, 0x79, 0},
	{0x0D05, 0x79, 0},
	{0x0E05, 0x79, 0},

	/*
	 * choose HVD source
	 */
	{0x0B06, 0xE8, 0},
	{0x0C06, 0xE8, 0},
	{0x0D06, 0xE8, 0},
	{0x0E06, 0xE8, 0},
	// {0x0330,0x04, 0}, Default = 0x04

	/*
	 * HIBW=1
	 */
	{0x0B07, 0x08, 0},
	{0x0C07, 0x08, 0},
	{0x0D07, 0x08, 0},
	{0x0E07, 0x08, 0},

	/*
	 * enable processing HS and DE signals
	 */
	{0x0B0F, 0x09, 0},
	{0x0C0F, 0x09, 0},
	{0x0D0F, 0x09, 0},
	{0x0E0F, 0x09, 0},

	/*
	 * set local ack
	 */
	{0x0B0D, 0x80, 0},
	{0x0C0D, 0x80, 0},
	{0x0D0D, 0x80, 0},
	{0x0E0D, 0x80, 0},


	/*
	 * Step 2 – Video Pipe Selection
	 *	Video pipes are the carriers and need to be selected properly
	 *	to match the video streams coming from serializers.
	 */
	{0x00F0, 0x62, 0},		/* pipe Z in link A to video pipe 0 */
					/* pipe Z in link B to video pipe 1 */
	{0x00F1, 0xEA, 0},		/* pipe Z in link C to video pipe 2 */
					/* pipe Z in link D to video pipe 3 */
	{0x00F4, 0x0F, 0},		/* Turn on pipe 0 - 3 */

	/*
	 * Step 3 – Software override
	 *	Parallel data does not contain datatypes.
	 *	Manual software override is required when
	 *	converting data from parallel to MIPI CSI-2.
	 *	Three overrides are needed for each stream:
	 *		Datatype, Virtual channel, and Bits-per-pixe
	 */
	{0x040B, 0x80, 0},		/* CSI Out Disable and */
	{0x0411, 0x90, 0},		/* set BPP as 0x10(DT 0x1E) for */
	{0x0412, 0x40, 0},		/* pipe 0 - 3 */
					/* Initial VC */
	{0x040C, 0x00, 0},		/* VC=00 for pipe0 & pipe1 */
	{0x040D, 0x00, 0},		/* VC=00 for pipe2 & pipe3 */

	{0x040E, 0x5E, 0},		/* Set Data Type as YUV422 8-bit(0x1E)
					 *  for pipe 0 - 3
					 */
	{0x040F, 0x7E, 0},
	{0x0410, 0x7A, 0},


	/*
	 * Step 4 – Video Pipe to MIPI Controller Mapping
	 * (video pipe - mapping block - routing block - mipi ctrls / PHYs)
	 *	Map enable bit for each Source and Destination.
	 *	Map Destination MIPI Controller.
	 *	Map Source virtual channel and data type.
	 *	Map Destination virtual channel and data type.
	 */
					/* video pipe 0 */
	{0x090B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x092D, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x090D, 0x1E, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x1E (YUV422)
					 */
	{0x090E, 0x1E, 0},		/* mapping block 0's DST
					 * VC: 0 / DT: 0x1E (YUV422)
					 */
	{0x090F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0910, 0x00, 0},		/* mapping block 1's DST
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0911, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0912, 0x01, 0},		/* mapping block 2's DST
					 * VC: 0 / DT: 0x01 (FS)
					 */

					/* video pipe 1 */
	{0x094B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x096D, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x094D, 0x1E, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x1E (YUV422)
					 */
	{0x094E, 0x5E, 0},		/* mapping block 0's DST
					 * VC: 1 / DT: 0x1E (YUV422)
					 */
	{0x094F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0950, 0x40, 0},		/* mapping block 1's DST
					 * VC: 1 / DT: 0x00 (FS)
					 */
	{0x0951, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0952, 0x41, 0},		/* mapping block 2's DST
					 * VC: 1 / DT: 0x01 (FS)
					 */

					/* video pipe 2 */
	{0x098B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x09AD, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x098D, 0x1E, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x1E (YUV422)
					 */
	{0x098E, 0x9E, 0},		/* mapping block 0's DST
					 * VC: 2 / DT: 0x1E (YUV422)
					 */
	{0x098F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0990, 0x80, 0},		/* mapping block 1's DST
					 * VC: 2 / DT: 0x00 (FS)
					 */
	{0x0991, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0992, 0x81, 0},		/* mapping block 2's DST
					 * VC: 2 / DT: 0x01 (FS)
					 */

					/* video pipe 3 */
	{0x09CB, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x09ED, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x09CD, 0x1E, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x1E (YUV422)
					 */
	{0x09CE, 0xDE, 0},		/* mapping block 0's DST
					 * VC: 3 / DT: 0x1E (YUV422)
					 */
	{0x09CF, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x09D0, 0xC0, 0},		/* mapping block 1's DST
					 * VC: 3 / DT: 0x00 (FS)
					 */
	{0x09D1, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x09D2, 0xC1, 0},		/* mapping block 2's DST
					 * VC: 3 / DT: 0x01 (FS)
					 */


	/*
	 * Step 5 – MIPI PHY Setting
	 *	5 different modes for D-PHY and C-PHY. Set one of the bits in
	 *	register 0x8A0 [4:0] exclusively to select the mode.
	 *		"(ports) x (data lanes) mode"
	 *		4x2 mode(bit0): four ports with 2-lanes each
	 *		2x4 mode(bit2): two ports with 4-lanes each
	 *		1x4a + 2x2 mode(bit3): one port with 4-lanes(PHY0, 1),
	 *					two ports with 2-lanes each
	 *		1x4b + 2x2 mode(bit4): one port with 4-lanes(PHY2, 3),
	 *					two ports with 2-lanes each
	 *
	 *	In x2 mode, 4 MIPI PHYs work independently. Each PHY has its
	 *	own clock pins.
	 *
	 *	In x4 mode, 2 of the 4 MIPI PHYs are glued together as 4-lane.
	 *	PHY1 and PHY2 are the master PHYs providing MIPI clock for
	 *	port A and B, respectively.
	 */
	{0x08A0, 0x04, 0},		/* Set Des in 2x4 mode */
					/* Set Lane Mapping for 4-lane */
	{0x08A3, 0xE4, 0},		/*
					 * PHY0
					 * data lane 0 - data lane 0
					 * data lane 1 - data lane 1
					 *
					 * PHY1
					 * data lane 0 - data lane 2
					 * data lane 1 - data lane 3
					 */
	{0x08A4, 0xE4, 0},		/*
					 * PHY2
					 * data lane 0 - data lane 0
					 * data lane 1 - data lane 1
					 *
					 * PHY3
					 * data lane 0 - data lane 2
					 * data lane 1 - data lane 3
					 */

					/* Set 4 lane D-PHY (default value) */
	{0x094A, 0xC0, 0},
	{0x098A, 0xC0, 0},

					/* Turn on all MIPI PHYs */
	{0x08A2, 0xF4, 0},		/* enable PHY0 - 3 */
					/* Set data rate to be 1500Mbps/lane
					 * Set software override
					 */
	{0x0415, 0xEE, 0},		/* Override Enable & Set 1400Mbps/lane*/
	{0x0418, 0xFE, 0},


	/*
	 * etc
	 */
	{0x01DA, 0x18, 0},
	{0x01FA, 0x18, 0},
	{0x021A, 0x18, 0},
	{0x023A, 0x18, 0},

	/*
	 * Enable GMSL1 to GMSL2 color mapping (D1) and
	 * set mapping type (D[7:3]) stored below
	 */
	{0x0B96, 0x9B, 0}, //1001 1011
	{0x0C96, 0x9B, 0},
	{0x0D96, 0x9B, 0},
	{0x0E96, 0x9B, 0},

	/*
	 * Shift GMSL1 HVD
	 */
	{0x0BA7, 0x45, 0},
	{0x0CA7, 0x45, 0},
	{0x0DA7, 0x45, 0},
	{0x0EA7, 0x45, 0},

	{0x0006, 0x07, 0},		/* GMSL1 mode for all Links and
					 * LINK A/B/C enabled.
					 */

	{0x0018, 0x0F, 100 * 1000},	/* reset oneshot */
};

const struct reg_sequence max96712_reg_init_qhd_ar0820_1ch[] = {
	{0x0013, 0x75, 50 * 1000},	/* Device Reset */

	/*
	 * Step 1 - GMSL Link Initialization
	 *	Link Enable (register 0x06)
	 *	Tx/Rx Rate Selection (register 0x10/0x11)
	 *	TODO: GMSL2 link lock sanity check
	 */
	{0x0006, 0xF1, 0},		/* Select GMSL2 all links,
					 * link A enable
					 */
	{0x0010, 0x22, 0},		/* Link A/B 6Gbps mode */
	{0x0011, 0x22, 0},		/* Link C/D 6Gbps mode */
	{0x0018, 0x0F, 100 * 1000},	/* Oneshot reset */
	{0x040B, 0x00, 0},		/* CSI output disabled */


	/*
	 * Step 2 – Video Pipe Selection
	 *	Video pipes are the carriers and need to be selected properly
	 *	to match the video streams coming from serializers.
	 */
	{0x00F0, 0x40, 0},		/* pipe X in link A to video pipe 0 */
					/* pipe X in link B to video pipe 1 */
	{0x00F4, 0x03, 0},		/* Turn on pipe 0 - 1 */

	/*
	 * Step 3 – Video Pipe to MIPI Controller Mapping
	 * (video pipe - mapping block - routing block - mipi ctrls / PHYs)
	 *	Map enable bit for each Source and Destination.
	 *	Map Destination MIPI Controller.
	 *	Map Source virtual channel and data type.
	 *	Map Destination virtual channel and data type.
	 */
					/* video pipe 0 */
	{0x090B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x092D, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x090D, 0x2C, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x090E, 0x2C, 0},		/* mapping block 0's DST
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x090F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0910, 0x00, 0},		/* mapping block 1's DST
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0911, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0912, 0x01, 0},		/* mapping block 2's DST
					 * VC: 0 / DT: 0x01 (FS)
					 */

					/* video pipe 1 */
	{0x094B, 0x07, 0},		/* enable mapping block 0 - 3 */
	{0x096D, 0x15, 0},		/* link mapping block 0 - 3 to
					 * MIPI Controller 1
					 */
	{0x094D, 0x2C, 0},		/* mapping block 0's SRC
					 * VC: 0 / DT: 0x2C (RAW12)
					 */
	{0x094E, 0x6C, 0},		/* mapping block 0's DST
					 * VC: 1 / DT: 0x2C (RAW12)
					 */
	{0x094F, 0x00, 0},		/* mapping block 1's SRC
					 * VC: 0 / DT: 0x00 (FS)
					 */
	{0x0950, 0x40, 0},		/* mapping block 1's DST
					 * VC: 1 / DT: 0x00 (FS)
					 */
	{0x0951, 0x01, 0},		/* mapping block 2's SRC
					 * VC: 0 / DT: 0x01 (FE)
					 */
	{0x0952, 0x41, 0},		/* mapping block 2's DST
					 * VC: 1 / DT: 0x01 (FS)
					 */


	/*
	 * Step 4 – MIPI PHY Setting
	 *	5 different modes for D-PHY and C-PHY. Set one of the bits in
	 *	register 0x8A0 [4:0] exclusively to select the mode.
	 *		"(ports) x (data lanes) mode"
	 *		4x2 mode(bit0): four ports with 2-lanes each
	 *		2x4 mode(bit2): two ports with 4-lanes each
	 *		1x4a + 2x2 mode(bit3): one port with 4-lanes(PHY0, 1),
	 *					two ports with 2-lanes each
	 *		1x4b + 2x2 mode(bit4): one port with 4-lanes(PHY2, 3),
	 *					two ports with 2-lanes each
	 *
	 *	In x2 mode, 4 MIPI PHYs work independently. Each PHY has its
	 *	own clock pins.
	 *
	 *	In x4 mode, 2 of the 4 MIPI PHYs are glued together as 4-lane.
	 *	PHY1 and PHY2 are the master PHYs providing MIPI clock for
	 *	port A and B, respectively.
	 */
	{0x08A0, 0x04, 0},		/* Set Des in 2x4 mode */
					/* Set Lane Mapping for 4-lane */
	{0x08A3, 0xE4, 0},		/*
					 * PHY0
					 * data lane 0 - data lane 0
					 * data lane 1 - data lane 1
					 *
					 * PHY1
					 * data lane 0 - data lane 2
					 * data lane 1 - data lane 3
					 */
	{0x08A4, 0xE4, 0},		/*
					 * PHY2
					 * data lane 0 - data lane 0
					 * data lane 1 - data lane 1
					 *
					 * PHY3
					 * data lane 0 - data lane 2
					 * data lane 1 - data lane 3
					 */

					/* Set 4 lane D-PHY (default value) */
	{0x090A, 0xC0, 0},		/* 4 lanes, DPHY mode, 2 bit VC */
	{0x094A, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */
	{0x098A, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */
	{0x09CA, 0xC0, 0},		/* 4 data lanes, DPHY mode, 2 bit VC */

					/* Turn on all MIPI PHYs */
	{0x08A2, 0xF4, 0},		/* enable PHY0 - 3 */
	{0x0418, 0x2E, 0},		/* Set data rate to be 1400Mbps/lane */

	{0x040B, 0x00, 0},		/* CSI out diable */
	{0x0018, 0x0F, 100 * 1000},	/* reset oneshot */
};




const struct reg_sequence max96712_vtg_reg_init[] = {
//Use VC0, VC1
	{0x1051, 0x10, 0},
//Set VS_DLY = 0
	{0x1052, 0x00, 0},
	{0x1053, 0x00, 0},
	{0x1054, 0x00, 0},
// Set VS High
	{0x1055, 0x00, 0},
	{0x1056, 0x1F, 0},
	{0x1057, 0x94, 0},
// Set Vs Low
	{0x1058, 0x26, 0},
	{0x1059, 0x05, 0},
	{0x105A, 0xF5, 0},
// Set HS Delay
	{0x105B, 0x00, 0},
	{0x105C, 0xEC, 0},
	{0x105D, 0x94, 0},
// Set HS_HIGH
	{0x105E, 0x07, 0},
	{0x105F, 0xDB, 0},
// Set HS_LOW
	{0x1060, 0x00, 0},
	{0x1061, 0x0A, 0},
// Set HS_CNT
	{0x1062, 0x04, 0},
	{0x1063, 0xD5, 0},
// Set V2D
	{0x1064, 0x00, 0},
	{0x1065, 0xEC, 0},
	{0x1066, 0xD6, 0},
// Set DE_HIGH
	{0x1067, 0x05, 0},
	{0x1068, 0x00, 0},
// Set DE_LOW
	{0x1069, 0x02, 0},
	{0x106A, 0xE5, 0},
// Set DE_CNT
	{0x106B, 0x02, 0},
	{0x106C, 0xD0, 0},
// Set Grad_INCR
	{0x106D, 0x06, 0},
// CHECKBOARD SETUP
// Set CHKR_COLOR_A_L_0
	{0x106E, 0xFF, 0},
// Set CHKR_COLOR_A_M_0
	{0x106F, 0xFF, 0},
// Set CHKR_COLOR_A_H_0
	{0x1070, 0x00, 0},
// Set CHKR_COLOR_B_L_0
	{0x1071, 0x00, 0},
// Set CHKR_COLOR_B_M_0
	{0x1072, 0xFF, 0},
// Set CHKR_COLOR_B_H_0
	{0x1073, 0xFF, 0},
// Set CHKR_RPT_A_0
	{0x1074, 0x80, 0},
// Set CHKR_RPT_B_0
	{0x1075, 0x80, 0},
// Set CHKR_ALT_0
	{0x1076, 0x80, 0},

// Set VTG mode, VRX_Patgen
	{0x1050, 0xF3, 0},

// Deserializer Setup
// Disable GMSL link
	{0x0006, 0x00, 0},
// Turn on pattern gen on pip 1 and 6
	{0x00F4, 0x42, 0},
// Set 1200MHz DPLL frequency (1.2Gbps/lane)
	{0x0415, 0x27, 0},
	{0x0418, 0x27, 0},
	{0x041B, 0x27, 0},
	{0x041E, 0x27, 0},
// Set Lane Count - 4 lane
	{0x090A, 0xC0, 0},
	{0x094A, 0xC0, 0},
	{0x098A, 0xC0, 0},
	{0x09CA, 0xC0, 0},
// Set Phy lane Map for all MIPI Phys
	{0x08A3, 0xE4, 0},
	{0x08A4, 0xE4, 0},
// Video pipeline 1 map to controller 1 for port A in x4 mode
	{0x094B, 0x07, 0},
	{0x096D, 0x15, 0},
	{0x094D, 0x24, 0},
	{0x094E, 0x24, 0},
	{0x094F, 0x00, 0},
	{0x0950, 0x00, 0},
	{0x0951, 0x01, 0},
	{0x0952, 0x01, 0},
//VC1
	{0x1081, 0x10, 0},
// Set VS_DLY = 0
	{0x1082, 0x00, 0},
	{0x1083, 0x00, 0},
	{0x1084, 0x00, 0},
// Set VS High
	{0x1085, 0x00, 0},
	{0x1086, 0x1F, 0},
	{0x1087, 0x94, 0},
// Set VS Low
	{0x1088, 0x26, 0},
	{0x1089, 0x05, 0},
	{0x108a, 0xF5, 0},
// Set HS Delay
	{0x108b, 0x00, 0},
	{0x108c, 0xEC, 0},
	{0x108d, 0x94, 0},
// Set HS High
	{0x108e, 0x07, 0},
	{0x108f, 0xDB, 0},
// Set HS Low
	{0x1090, 0x00, 0},
	{0x1091, 0x0A, 0},
// Set HS Cnt
	{0x1092, 0x04, 0},
	{0x1093, 0xD5, 0},
// Set V2D
	{0x1094, 0x00, 0},
	{0x1095, 0xEC, 0},
	{0x1096, 0xD6, 0},
// Set De HIGH
	{0x1097, 0x05, 0},
	{0x1098, 0x00, 0},
// Set De Low
	{0x1099, 0x02, 0},
	{0x109a, 0xE5, 0},
// Set De Cnt
	{0x109b, 0x02, 0},
	{0x109c, 0xD0, 0},
// Set Grad INCT_0_0
	{0x109d, 0x06, 0},
// CHECKERBOARD SETUP - PATGEN MODE = 1,,
// Set CHKR_COLOR_A_L_0
	{0x109e, 0xFF, 0},
	{0x109f, 0xFF, 0},
	{0x10a0, 0x00, 0},
	{0x10a1, 0x00, 0},
	{0x10a2, 0xFF, 0},
	{0x10a3, 0xFF, 0},
	{0x10a4, 0x80, 0},
	{0x10a5, 0x80, 0},
	{0x10a6, 0x80, 0},
// video pipeline 5, map to unused controller 0 to avoid conflicts with pipe 0
	{0x0A4B, 0x07, 0},
	{0x0A6D, 0x15, 0},
	{0x0A4D, 0x24, 0},
	{0x0A4E, 0x64, 0},
	{0x0A4F, 0x00, 0},
	{0x0A50, 0x40, 0},
	{0x0A51, 0x01, 0},
	{0x0A52, 0x41, 0},
};

const struct reg_sequence max96712_vtg_reg_s_stream[] = {
// Set to PCLK = 75MHz
	{0x0009, 0x01, 0},
// Enable CSI clock
	{0x08A0, 0x84, 0},
// Turn on PHYs
	{0x08A2, 0xF4, 0},
// Set VTG mode VRX_Patgen 0, Generate VS, HS, DE", Invert the VS
	{0x1050, 0xF3, 0},
// Set VTG mode VRX_Patgen 1, Generate VS, HS, DE", Invert the VS
	{0x1080, 0xF3, 0},
};


const struct reg_sequence max96712_reg_s_stream[] = {
	//MAX96712 force CSI out enable
	{0x08A0, 0x84, 0},
	/* Local ACK for pipe 0~3 DISEN */
	{0x0B0D, 0x00, 0},
	{0x0C0D, 0x00, 0},
	{0x0D0D, 0x00, 0},
	{0x0E0D, 0x00, 50*1000},
};

#endif

