// SPDX-License-Iden`ifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/phy.h>
#include <linux/delay.h>

MODULE_DESCRIPTION("TCC Marvell PHY driver");
MODULE_AUTHOR("Telechips");
MODULE_LICENSE("GPL");

#define PHY_ID_MARVELL_Q2110	0x00000983
#define PHY_ID_MARVELL_Q1110	0x00000b21
#define PHY_ID_MARVELL_MASK		0xfff

#define PHY_NAME_MARVELL_Q2110	"Marvell Q2110"
#define PHY_NAME_MARVELL_Q1110	"Marvell Q1110"

#define	MMD_ACCESS_CTRL			(unsigned int)(13)
#define MMD_ACCESS_ADDR_DATA	(unsigned int)(14)

#define OPERATION_BIT_SHIFT		(unsigned int)(14)
#define OPERATION_ADDR			(unsigned int)(0x0)
#define OPERATION_RW			(unsigned int)(0x1)

#define SUPPORT_MRVL_88Q2220_PHY

#if defined(SUPPORT_MRVL_88Q2220_PHY)
#define MRVL_Q222X_PRODUCT_ID				0x0000U     /*!< Product series id, see detailed package id for more information */

#define MRVL_Q222X_PACKAGE_ID_Q2220M		0x0000U	    /*!< 1000/100 speed with RGMII and MACsec */
#define MRVL_Q222X_PACKAGE_ID_Q2221M		0x0001U	    /*!< 1000/100 speed with SGMII and MACsec */
#define MRVL_Q222X_PACKAGE_ID_Q1210M		0x0002U	    /*!< 100 speed with RGMII and MACsec */
#define MRVL_Q222X_PACKAGE_ID_Q1211M		0x0003U	    /*!< 100 speed with RGMII and MACsec */
#define MRVL_Q222X_PACKAGE_ID_Q2220			0x0004U	    /*!< 1000/100 speed with RGMII */
#define MRVL_Q222X_PACKAGE_ID_Q2221			0x0005U	    /*!< 1000/100 speed with SGMII and MACsec */
#define MRVL_Q222X_PACKAGE_ID_Q1210			0x0006U	    /*!< 100 speed with RGMII and MACsec */
#define MRVL_Q222X_PACKAGE_ID_Q1211			0x0007U	    /*!< 100 speed with SGMII and MACsec */
#define MRVL_Q222X_PACKAGE_ID_Q2233M_P0		0x0008U	    /*!< 1000/100 speed two-port PHY with SGMII and MACsec, port 0 */
#define MRVL_Q222X_PACKAGE_ID_Q2233M_P1		0x0009U	    /*!< 1000/100 speed two-port PHY with SGMII and MACsec, port 1 */
#define MRVL_Q222X_PACKAGE_ID_Q2233_P0		0x000CU	    /*!< 1000/100 speed two-port PHY with SGMII, port 0 */
#define MRVL_Q222X_PACKAGE_ID_Q2233_P1		0x000DU	    /*!< 1000/100 speed two-port PHY with SGMII, port 1 */

#define MRVL_Q222X_A0						0x0000U     /*!< A0 is not covered in this API package, A0 is not supported. */
#define MRVL_Q222X_B0						0x0001U     /*!< API supports B0 */
#define MRVL_Q222X_B1						0x0002U     /*!< API supports B1 */

/* Auto-Negotiation Controls - reg 7.0x0200 */
#define MRVL_Q222X_AN_DISABLE				(0x0000U)
#define MRVL_Q222X_AN_RESET					(0x8000U)
#define MRVL_Q222X_AN_ENABLE				(0x1000U)

/* Auto-Negotiation Status - reg 7.0x0201 */
#define MRVL_Q222X_AN_COMPLETE				(0x0020U)

/* Auto-Negotiation Flags - reg 7.0x0203 */
#define MRVL_Q222X_AN_FE_ABILITY			(0x0020U)
#define MRVL_Q222X_AN_GE_ABILITY			(0x0080U)

#define MRVL_Q222X_LOWER_BOUND				(0) 
#define MRVL_Q222X_UPPER_BOUND				(5)

#define MRVL_APHY_STATUS_OK					(0x0000U)		/*!< Status code - Success */
#define MRVL_APHY_STATUS_ERROR_GENERAL		(0x0001U)		/*!< Status code - Error */
#define MRVL_APHY_STATUS_ERROR_TIMEOUT		(0x0002U)		/*!< Status code - Timeout error */
#define MRVL_APHY_STATUS_ERROR_MDIO			(0x0004U)		/*!< Status code - Reg access error */
#define MRVL_APHY_STATUS_ERROR_NOTSUPPORT	(0x0010U)		/*!< Status code - Non-support error */
#define MRVL_APHY_STATUS_UNKNOW				(0x1000U)		/*!< Status code - Other unknow error */

#define MRVL_APHY_OP_SLAVE			(0)		/*!< OP mode: SLAVE */
#define MRVL_APHY_OP_MASTER			(1)		/*!< OP mode: MASTER */

#define MRVL_Q222X_FALSE			(0)
#define MRVL_Q222X_TRUE				(1)

#define MRVL_Q222X_SPEED_1000		(0)
#define MRVL_Q222X_SPEED_100		(1)

/* Operation Mode */
#define MRVL_Q222X_SLAVE			(0)
#define MRVL_Q222X_MASTER			(1)

/* General ID detection related registers */
#define MRVL_ID_DEVICE				(1)
#define MRVL_ORGANIZATION_ID_REG	(0x2)
#define MRVL_APHY_ID_REG			(0x3)
#define MRVL_APHY_ID				(0x0032)
#define MRVL_ORGANIZATION_ID		(0x2B)
#define MRVL_APHY_ID_EXT_DEVICE		(4)
#define MRVL_APHY_ID_EXT_REG		(0x80FF)

#define PHY_ID_MARVELL_Q2220		(0x00000032)

#define PHY_NAME_MARVELL_Q2220		"Marvell Q2220"

struct mrvl_88q222x {
	unsigned char phy_addr;
	unsigned char smi_port;
	unsigned short model_num;
	unsigned short package_num;
	unsigned char rev_num;
};
#endif

// Clause 22 to Clause 45 access method is from Marvell 88Q111x phy datasheet.

static int phy_read_c22_to_c45(struct phy_device *phydev, u16 dev_addr,
			       u32 reg_addr)
{
	phy_write(phydev, MMD_ACCESS_CTRL,
			(((OPERATION_ADDR) & 0x3)<<OPERATION_BIT_SHIFT) |
			(dev_addr & 0x1F));
	phy_write(phydev, MMD_ACCESS_ADDR_DATA, reg_addr);
	phy_write(phydev, MMD_ACCESS_CTRL,
			(((OPERATION_RW) & 0x3)<<OPERATION_BIT_SHIFT) |
			(dev_addr & 0x1F));

	return phy_read(phydev, (u32) MMD_ACCESS_ADDR_DATA);
}

static void phy_write_c22_to_c45(struct phy_device *phydev, u16 dev_addr,
				 u16 reg_addr, u16 data)
{
	phy_write(phydev, MMD_ACCESS_CTRL,
		  (((OPERATION_ADDR) & 0x3) << OPERATION_BIT_SHIFT) |
			  (dev_addr & 0x1F));
	phy_write(phydev, MMD_ACCESS_ADDR_DATA, reg_addr);
	phy_write(phydev, MMD_ACCESS_CTRL,
		  (((OPERATION_RW) & 0x3) << OPERATION_BIT_SHIFT) |
			  (dev_addr & 0x1F));
	phy_write(phydev, MMD_ACCESS_ADDR_DATA, data);
}

static void softReset(struct phy_device *phydev)
{
	u16 data = (unsigned short)phy_read_c22_to_c45(phydev, 1, 0x0000);

	data |= (unsigned short)(1 << 11);
	phy_write_c22_to_c45(phydev, 1, 0x0000, data);
	phy_write_c22_to_c45(phydev, 3, 0xFFE4, 0x000C);
	usleep_range(1000, 2000);
	//mdelay(1);
	phy_write_c22_to_c45(phydev, 3, 0xFFE4, 0x06B6);
	data &= (unsigned short)(~(1 << 11));
	phy_write_c22_to_c45(phydev, 1, 0x0000, data);
	usleep_range(1000, 2000);
	//mdelay(1);
	phy_write_c22_to_c45(phydev, 3, 0xFC47, 0x0030);
	phy_write_c22_to_c45(phydev, 3, 0xFC47, 0x0031);
	phy_write_c22_to_c45(phydev, 3, 0xFC47, 0x0030);
	phy_write_c22_to_c45(phydev, 3, 0xFC47, 0x0000);
	phy_write_c22_to_c45(phydev, 3, 0xFC47, 0x0001);
	phy_write_c22_to_c45(phydev, 3, 0xFC47, 0x0000);
	phy_write_c22_to_c45(phydev, 3, 0x0900, 0x8000);
	phy_write_c22_to_c45(phydev, 1, 0x0900, 0x0000);
	phy_write_c22_to_c45(phydev, 3, 0xFFE4, 0x000C);
}

#ifdef TCC_MARVELL_TEST_FEATATURE
static void set_master_slave(struct phy_device *phydev, bool forceMaster)
{
	u16 data = 0;

	data = (unsigned short)phy_read_c22_to_c45(phydev, 1, 0x0834);
	if (forceMaster) {
		data |= (unsigned short)0x4000;
	} else {
		data &= (unsigned short)0xBFFF;
	}

	phy_write_c22_to_c45(phydev, 1, 0x0834, data);

#ifdef MRVL_Q212X_LPSD_FEATURE_ENABLE
	if (forceMaster != 0) {
		phy_write_c22_to_c45(phydev, 7, 0x8032, 0x005A);
	} else {
		phy_write_c22_to_c45(phydev, 7, 0x8032, 0x0064);
	}
	phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0A01);
	phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0C01);
#else
	phy_write_c22_to_c45(phydev, 7, 0x8032, 0x0064);
	phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0A01);
	phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0C01);
#endif
}

static bool check_link(struct phy_device *phydev)
{
	u16 ret_data1, ret_data2 = 0;

	phy_read_c22_to_c45(phydev, 3, 0x0901);
	ret_data1 = (unsigned short)phy_read_c22_to_c45(phydev, 3, 0x0901);
	ret_data2 = (unsigned short)phy_read_c22_to_c45(phydev, 7, 0x8001);
	usleep_range(1000, 2000);
	//mdelay(1);

	return ((unsigned short)0x0 !=
		((unsigned short)ret_data1 & (unsigned short)0x0004)) &&
	       ((unsigned short)0x0 !=
		((unsigned short)ret_data2 & (unsigned short)0x3000));
}

static bool get_realtime_link_status(struct phy_device *phydev)
{
	u16 data = 0;

	phy_read_c22_to_c45(phydev, 3, 0x0901);
	data = (unsigned short)phy_read_c22_to_c45(phydev, 3, 0x0901);
	return ((unsigned short)0x0 !=
		((unsigned short)data & (unsigned short)0x0004));
}
#endif

static u16 getMasterSlave(struct phy_device *phydev)
{
	return (unsigned short)((unsigned int)((unsigned int)
				phy_read_c22_to_c45(phydev, 7, 0x8001) >>
				14) & 0x0001);
}

static bool getLatchedLinkStatus(struct phy_device *phydev)
{
	u16 data =
	    (unsigned short)phy_read_c22_to_c45(phydev, 7, 0x0201);

	return ((unsigned short)0x0 !=
		((unsigned short)data & (unsigned short)0x0004));
}

static void init_q211x(struct phy_device *phydev)
{
	u16 data = 0;
	unsigned int get_mas_slave;

	usleep_range(2000, 4000);
	//mdelay(2);
	phy_write_c22_to_c45(phydev, 1, 0x0900, 0x4000);
	phy_write_c22_to_c45(phydev, 7, 0x0200, 0x0000);
	data = (unsigned short)phy_read_c22_to_c45(phydev, 1, 0x0834);
	data =
	(unsigned short)(((unsigned short)data & (unsigned short)0xFFF0)
			| (unsigned short)0x0001);

	get_mas_slave = getMasterSlave(phydev);
	if (get_mas_slave == 0x1) { // master
		data &=
		(unsigned
		short)(~(unsigned short)((unsigned short)1 <<
					(unsigned short)14));
		data |=
		(unsigned short)((unsigned short)1 << (unsigned short)14);
	} else { // slave
		data &=
		(unsigned
		short)(~(unsigned short)((unsigned short)1 <<
					(unsigned short)14));
	}

	phy_write_c22_to_c45(phydev, 1, 0x0834, data);
	phy_write_c22_to_c45(phydev, 3, 0xFFE4, 0x07B5);
	phy_write_c22_to_c45(phydev, 3, 0xFFE4, 0x06B6);
	usleep_range(5000, 7000);
	//mdelay(5);
	phy_write_c22_to_c45(phydev, 3, 0xFFDE, 0x402F);
	phy_write_c22_to_c45(phydev, 3, 0xFE2A, 0x3C3D);
	phy_write_c22_to_c45(phydev, 3, 0xFE34, 0x4040);
	phy_write_c22_to_c45(phydev, 3, 0xFE4B, 0x9337);
	phy_write_c22_to_c45(phydev, 3, 0xFE2A, 0x3C1D);
	phy_write_c22_to_c45(phydev, 3, 0xFE34, 0x0040);
	phy_write_c22_to_c45(phydev, 3, 0xFE0F, 0x0000);
	phy_write_c22_to_c45(phydev, 3, 0xFC00, 0x01C0);
	phy_write_c22_to_c45(phydev, 3, 0xFC17, 0x0425);
	phy_write_c22_to_c45(phydev, 3, 0xFC94, 0x5470);
	phy_write_c22_to_c45(phydev, 3, 0xFC95, 0x0055);
	phy_write_c22_to_c45(phydev, 3, 0xFC19, 0x08d8);
	phy_write_c22_to_c45(phydev, 3, 0xFC1A, 0x0110);
	phy_write_c22_to_c45(phydev, 3, 0xFC1B, 0x0a10);
	phy_write_c22_to_c45(phydev, 3, 0xFC3A, 0x2725);
	phy_write_c22_to_c45(phydev, 3, 0xFC61, 0x2627);
	phy_write_c22_to_c45(phydev, 3, 0xFC3B, 0x1612);
	phy_write_c22_to_c45(phydev, 3, 0xFC62, 0x1C12);
	phy_write_c22_to_c45(phydev, 3, 0xFC9D, 0x6367);
	phy_write_c22_to_c45(phydev, 3, 0xFC9E, 0x8060);
	phy_write_c22_to_c45(phydev, 3, 0xFC00, 0x01C8);
	phy_write_c22_to_c45(phydev, 3, 0x8000, 0x0000);
	phy_write_c22_to_c45(phydev, 3, 0x8016, 0x0011);
	phy_write_c22_to_c45(phydev, 3, 0xFDA3, 0x1800);
	phy_write_c22_to_c45(phydev, 3, 0xFE02, 0x00C0);
	phy_write_c22_to_c45(phydev, 3, 0xFFDB, 0x0010);
	phy_write_c22_to_c45(phydev, 3, 0xFFF3, 0x0020);
	phy_write_c22_to_c45(phydev, 3, 0xFE40, 0x00A6);
	phy_write_c22_to_c45(phydev, 3, 0xFE60, 0x0000);
	phy_write_c22_to_c45(phydev, 3, 0xFE2A, 0x3C3D);
	phy_write_c22_to_c45(phydev, 3, 0xFE4B, 0x9334);
	phy_write_c22_to_c45(phydev, 3, 0xFC10, 0xF600);
	phy_write_c22_to_c45(phydev, 3, 0xFC11, 0x073D);
	phy_write_c22_to_c45(phydev, 3, 0xFC12, 0x000D);
	phy_write_c22_to_c45(phydev, 3, 0xFC13, 0x0010);

#ifdef MRVL_Q212X_LPSD_FEATURE_ENABLE
	if (getMasterSlave(phydev)) {
		phy_write_c22_to_c45(phydev, 7, 0x8032, 0x005A);
	} else {
		phy_write_c22_to_c45(phydev, 7, 0x8032, 0x0064);
	}
	phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0A01);
	phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0C01);
	phy_write_c22_to_c45(phydev, 3, 0x800C, 0x0008);
	phy_write_c22_to_c45(phydev, 7, 0x8032, 0x0001);
	phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0A1B);
	phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0C1B);
	phy_write_c22_to_c45(phydev, 7, 0x8032, 0x000B);
	phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0A1C);
	phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0C1C);
	phy_write_c22_to_c45(phydev, 3, 0xFE04, 0x0016);
#else
	phy_write_c22_to_c45(phydev, 7, 0x8032, 0x0064);
	phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0A01);
	phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0C01);
	phy_write_c22_to_c45(phydev, 3, 0x800C, 0x0000);
	phy_write_c22_to_c45(phydev, 7, 0x8032, 0x0002);
	phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0A1B);
	phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0C1B);
	phy_write_c22_to_c45(phydev, 7, 0x8032, 0x0003);
	phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0A1C);
	phy_write_c22_to_c45(phydev, 7, 0x8031, 0x0C1C);
	phy_write_c22_to_c45(phydev, 3, 0xFE04, 0x0008);
#endif
	softReset(phydev);
}

static int q2110_config_init(struct phy_device *phydev)
{
	init_q211x(phydev);
#if defined(CONFIG_TCC_MARVELL_LOOPBACK)
	// Ethernet phy loopback mode
	pr_info("Set Loopback mode !!!!\n");
	phy_write_c22_to_c45(phydev, 1, 0x0000,
			     ((phy_read_c22_to_c45(phydev, 1, 0x0000) | 0x1)));
#endif
	pr_info("q2110 phy master(1)/slave(0) : %x\n", getMasterSlave(phydev));
	return 0;
}

static int q2110_read_status(struct phy_device *phydev)
{
	phydev->link = (int)getLatchedLinkStatus(phydev);
	phydev->speed = SPEED_1000;
	phydev->duplex = DUPLEX_FULL;
#if defined(CONFIG_TCC_MARVELL_LOOPBACK)
	// If loopback is enabled, force ethernet phy to be link up.
	phydev->link = 1; // link up
#endif

	return 0;
}

static int q2110_aneg_done(struct phy_device *phydev)
{
	return 0;
}

static int q2110_match_phy_device(struct phy_device *phydev)
{
	int phy_id = phy_read_c22_to_c45(phydev, 1, 0x0003);

	return (phy_id == PHY_ID_MARVELL_Q2110);
}

static int q1110_config_init(struct phy_device *phydev)
{
	u16 data = 0;

	data = phy_read_c22_to_c45(phydev, 4, 0x8000);
	pr_info("[INFO][GMAC] %s: default 4.8000.1:0: %08x\n", __func__, data);

#if defined(CONFIG_TCC_MARVELL_LOOPBACK)
	// Ethernet phy loopback mode
	data = phy_read_c22_to_c45(phydev, 3, 0x0000);
	data |= (0x1 << 14);
	phy_write_c22_to_c45(phydev, 3, 0x0000, data);
	data = phy_read_c22_to_c45(phydev, 3, 0x0000);
	pr_info("[INFO][GMAC] %s: Set loopback: %08x\n", d__func__ata);
#endif
	// Set PMOS/NMOS value
	data = phy_read_c22_to_c45(phydev, 4, 0x8001);
	pr_info("[INFO][GMAC] %s: Before set PMOS: %08x\n", __func__, data);
	data |= (0xF << 8) | (0xF << 4);
	phy_write_c22_to_c45(phydev, 4, 0x8001, data);

	data = phy_read_c22_to_c45(phydev, 4, 0x8001);
	pr_info("[INFO][GMAC] %s: After set PMOS: %08x\n", __func__, data);

	return 0;
}

static int q1110_read_status(struct phy_device *phydev)
{
	phydev->link = (int)getLatchedLinkStatus(phydev);
	phydev->speed = SPEED_100;
	phydev->duplex = DUPLEX_FULL;
#if defined(CONFIG_TCC_MARVELL_LOOPBACK)
	// If loopback is enabled, force ethernet phy to be link up.
	phydev->link = 1; // link up
#endif

	return 0;
}

static int q1110_config_aneg(struct phy_device *phydev)
{
	return 0;
}

static int q1110_aneg_done(struct phy_device *phydev)
{
	return 0;
}

static int q1110_match_phy_device(struct phy_device *phydev)
{
	int phy_id = phy_read_c22_to_c45(phydev, 1, 0x0003);

	return (phy_id == PHY_ID_MARVELL_Q1110);
}

#if defined(SUPPORT_MRVL_88Q2220_PHY)
static unsigned int q222x_check_reg_access(struct phy_device *phydev)
{
	unsigned short i;
	int mrvl_id;
	unsigned int status = MRVL_APHY_STATUS_ERROR_MDIO;

	/* check reg access by reading bacl MRVL ID*/
	for (i = MRVL_Q222X_LOWER_BOUND; i < MRVL_Q222X_UPPER_BOUND; i++) {
		mrvl_id = phy_read_c22_to_c45(phydev, MRVL_ID_DEVICE,
				MRVL_ORGANIZATION_ID_REG);
		if (mrvl_id == MRVL_ORGANIZATION_ID) {
			status = MRVL_APHY_STATUS_OK;
			break;
		}
		usleep_range(1000, 2000);
		//mdelay(1);
	}

	return status;
}

static unsigned int q222x_get_device_info(struct phy_device *phydev)
{
	int model_num;
	unsigned int status = MRVL_APHY_STATUS_OK;
	struct mrvl_88q222x *mrvl_dev = (struct mrvl_88q222x *)phydev->priv;

	/* check reg access */
	status = q222x_check_reg_access(phydev);
	if (status == MRVL_APHY_STATUS_OK) {
		model_num = phy_read_c22_to_c45(phydev, MRVL_ID_DEVICE, MRVL_APHY_ID_REG);
		mrvl_dev->rev_num = (unsigned char)(model_num & 0x000FU);
		model_num = (model_num & 0x03F0U) >> 4;

		//pr_info("[INFO][GMAC] %s: rev_num: 0x%08x", __func__, (unsigned int)mrvl_dev->rev_num);
		//pr_info("[INFO][GMAC] %s: PHY ID: 0x%08x", __func__, (unsigned int)model_num);

		if (model_num == MRVL_APHY_ID) {
			model_num = phy_read_c22_to_c45(phydev, MRVL_APHY_ID_EXT_DEVICE, MRVL_APHY_ID_EXT_REG);
			mrvl_dev->model_num = (model_num & 0xFFF0U) >> 5;
			mrvl_dev->package_num = (model_num & 0xFU);

			//pr_info("[INFO][GMAC] %s: model_num: 0x%08x", __func__, (unsigned int)mrvl_dev->model_num);
			//pr_info("[INFO][GMAC] %s: package_num: 0x%08x", __func__, (unsigned int)mrvl_dev->package_num);

			if (mrvl_dev->model_num != MRVL_Q222X_PRODUCT_ID) {
				status = MRVL_APHY_STATUS_ERROR_NOTSUPPORT;
				pr_err("[ERR][GMAC] %s: check PRODUCT_ID(0x08%x)\n",
						__func__, mrvl_dev->model_num);
			}
		} else {
			status = MRVL_APHY_STATUS_ERROR_NOTSUPPORT;
			pr_err("[ERR][GMAC] %s: check PHY_ID(0x08%x)\n", __func__, model_num);
		}
	} else {
		pr_err("[ERR][GMAC] %s: fail to access register!\n", __func__);
	}

	return status;
}

static unsigned int q222x_check_support_sgmii(struct phy_device *phydev,
		unsigned int *support_sgmii)
{
	struct mrvl_88q222x *mrvl_dev = (struct mrvl_88q222x *)phydev->priv;

	switch (mrvl_dev->package_num) {
		case MRVL_Q222X_PACKAGE_ID_Q2220M:
		case MRVL_Q222X_PACKAGE_ID_Q1210M:
		case MRVL_Q222X_PACKAGE_ID_Q2220:
		case MRVL_Q222X_PACKAGE_ID_Q1210:
			*support_sgmii = MRVL_Q222X_FALSE;
			break;
		default:
			*support_sgmii = MRVL_Q222X_FALSE;
			break;
	}

	return MRVL_APHY_STATUS_OK;
}

static unsigned int q222x_get_aneg_enabled(struct phy_device *phydev,
		unsigned int *is_enable)
{
	int reg_val;

	reg_val = phy_read_c22_to_c45(phydev, 7, 0x0200);

	*is_enable = ((reg_val & MRVL_Q222X_AN_ENABLE) != 0x0) ?
		MRVL_Q222X_TRUE : MRVL_Q222X_FALSE;

	//pr_info("[INFO][GMAC] %s: is_enable = %d\n", __func__, *is_enable);

	return MRVL_APHY_STATUS_OK;
}

static unsigned int q222x_check_is_master(struct phy_device *phydev,
		unsigned int *is_master)
{
	int reg_val;
	unsigned int status = MRVL_APHY_STATUS_OK;
	unsigned int ang_enable;

	status = q222x_get_aneg_enabled(phydev, &ang_enable);
	if (ang_enable == MRVL_Q222X_TRUE) {
		/* read 7.0x8001 in auto-neg mode */
		reg_val = phy_read_c22_to_c45(phydev, 7, 0x8001);
		/* check bit 14 */
		reg_val = (reg_val >> 14) & 0x0001U;
	} else {
		/* read 1.0x0834 in forced speed mode */
		reg_val = phy_read_c22_to_c45(phydev, 1, 0x0834);
		/* check bit 14 */
		reg_val = (reg_val >> 14) & 0x0001U;
	}

	if (reg_val == 1U) {
		*is_master = MRVL_Q222X_TRUE;
	} else {
		*is_master = MRVL_Q222X_FALSE;
	}

	pr_info("[INFO][GMAC] %s: is_master = %d\n", __func__, *is_master);

	return status;
}

static int q222x_soft_reset(struct phy_device *phydev)
{
	/* enable RESET of DCL */
	phy_write_c22_to_c45(phydev, 3, 0xFE1B, 0x48);

	/* soft reset */
	phy_write_c22_to_c45(phydev, 3, 0x0900, 0x8000);
	phy_write_c22_to_c45(phydev, 3, 0xFFE4, 0x000C);

	/* disable RESET of DCL */
	phy_write_c22_to_c45(phydev, 3, 0xFE1B, 0x58);

	//pr_info("[INFO][GMAC] %s: out\n", __func__);

	return 0;
}

static unsigned int q222x_get_speed(struct phy_device *phydev,
		int *speed)
{
	int reg_val;
	unsigned int is_enable;
	unsigned int status;

	status = q222x_get_aneg_enabled(phydev, &is_enable);
	if (is_enable == MRVL_Q222X_TRUE) {
		reg_val = phy_read_c22_to_c45(phydev, 7, 0x801A);
		reg_val = (reg_val & 0x4000U) >> 14;
	} else {
		reg_val = phy_read_c22_to_c45(phydev, 1, 0x0834);
		reg_val &= 0x0001U;
	}

	/* only two speeds available */
	if (reg_val == 0) {
		*speed = MRVL_Q222X_SPEED_100;
	} else {
		*speed = MRVL_Q222X_SPEED_1000;
	}

	return status;
}

static unsigned int q222x_get_latched_link_status(struct phy_device *phydev,
		int *is_linkup)
{
	int reg_val;

	reg_val = phy_read_c22_to_c45(phydev, 1, 0x0901);

	*is_linkup = ((reg_val & 0x0001U) != 0x0U) ?
		MRVL_Q222X_TRUE : MRVL_Q222X_FALSE;

	return MRVL_APHY_STATUS_OK;
}

static unsigned int q222x_get_real_time_link_status(struct phy_device *phydev,
		int *is_linkup)
{
	int reg_val;

	/* 1000 speed real time link is a latched link */
	reg_val = phy_read_c22_to_c45(phydev, 3, 0x0901);
	reg_val = phy_read_c22_to_c45(phydev, 3, 0x0901);    /* Read twice: register latches low */

	*is_linkup = ((reg_val & 0x0004U) != 0x0) ? MRVL_Q222X_TRUE : MRVL_Q222X_FALSE;

	return MRVL_APHY_STATUS_OK;
}

static unsigned int q222x_check_link_ge(struct phy_device *phydev)
{
	int reg_val1, reg_val2, reg_val3;
	unsigned int is_linkup;

	reg_val1 = phy_read_c22_to_c45(phydev, 3, 0x0901);
	reg_val1 = phy_read_c22_to_c45(phydev, 3, 0x0901);	/* Read twice: link latches low status */
	reg_val2 = phy_read_c22_to_c45(phydev, 7, 0x8001);	/* local and remote receiver status */
	reg_val3 = phy_read_c22_to_c45(phydev, 3, 0xFD9D);	/* local receiver status 2 */
	is_linkup = ((0x0004U == (reg_val1 & 0x0004U)) 
			&& (0x3000U == (reg_val2 & 0x3000U)) 
			&& (0x0010U == (reg_val3 & 0x0010U))) ? MRVL_Q222X_TRUE : MRVL_Q222X_FALSE;

	return is_linkup;
}

static void q222x_check_link(struct phy_device *phydev, int *is_linkup)
{
	*is_linkup = q222x_check_link_ge(phydev);
}

static int q222x_read_status(struct phy_device *phydev)
{
	int is_linkup, speed;

	q222x_get_latched_link_status(phydev, &is_linkup);
	phydev->link = is_linkup;

	q222x_get_speed(phydev, &speed);
	if (speed == MRVL_Q222X_SPEED_100) {
		phydev->speed = SPEED_100;
	} else {
		phydev->speed = SPEED_1000;
	} 

	phydev->duplex = DUPLEX_FULL;

	return 0;
}

/* prepare to write init code */
static void apply_b0_preinit(struct phy_device *phydev)
{
	int i;
	int reg_val;

	/* enable txdac */
	phy_write_c22_to_c45(phydev, 3, 0x8033, 0x6801);

	/* disable ANEG */
	phy_write_c22_to_c45(phydev, 7, 0x0200, MRVL_Q222X_AN_DISABLE);

	/* set IEEE power down */
	phy_write_c22_to_c45(phydev, 1, 0x0, 0x840);

	/* exit standby state(internal state) */
	phy_write_c22_to_c45(phydev, 3, 0xFE1B, 0x48);

	/* set power management state breakpoint (internal state) */
	phy_write_c22_to_c45(phydev, 3, 0xFFE4, 0x6B6);

	/* exit IEEE power down */
	phy_write_c22_to_c45(phydev, 1, 0x0000, 0x0000);
	phy_write_c22_to_c45(phydev, 3, 0x0000, 0x0000);

	/* wait upto MRVL_Q222X_ACCESS_ATTEMPT_UPPER_BOUND to enter to power management state */
	/* if not meet the target value, is still ok to proceed */
	for (i = MRVL_Q222X_LOWER_BOUND; i <= MRVL_Q222X_UPPER_BOUND; i++) {
		reg_val = phy_read_c22_to_c45(phydev, 3, 0xFFE4);
		if (reg_val == 0x06BA) {
			break;
		}
		usleep_range(1000, 2000);
		//mdelay(1);
	}
}

static void apply_b1_init(struct phy_device *phydev)
{
	/* set power management state breakpoint (internal state) */
	phy_write_c22_to_c45(phydev, 3, 0xFFE4, 0x0007);

	/* disable ANEG */ 
	phy_write_c22_to_c45(phydev, 7, 0x0200, MRVL_Q222X_AN_DISABLE);

	/* prepare to write init code */
	/* enable TXDAC setting overwrite bit */
	phy_write_c22_to_c45(phydev, 3, 0xFFE3, 0x7000);

	/* set IEEE power down */
	phy_write_c22_to_c45(phydev, 1, 0x0, 0x0840);

	/* wait 3 ms to get to ON_TBGB State and send out WUP if needed */
	usleep_range(3000, 5000);
	//mdelay(3);

	/* exit standby state(internal state) */
	phy_write_c22_to_c45(phydev, 3, 0xFE1B, 0x0048);

	/* exit IEEE power down */
	phy_write_c22_to_c45(phydev, 1, 0x0000, 0x0000);
	phy_write_c22_to_c45(phydev, 3, 0x0000, 0x0000);

	/* configure TC10 loc_act_detect for rev B1 */
	phy_write_c22_to_c45(phydev, 3, 0xFCAD, 0x030C);
	phy_write_c22_to_c45(phydev, 3, 0x8032, 0x6001);
	phy_write_c22_to_c45(phydev, 3, 0xFDFF, 0x05A5);
	phy_write_c22_to_c45(phydev, 3, 0xFDEC, 0xDBAF);
	phy_write_c22_to_c45(phydev, 3, 0xFCAB, 0x1054);
	phy_write_c22_to_c45(phydev, 3, 0xFCAC, 0x1483);

	/* set DISABLE_WAIT_COMM to 0 for WUR */
	phy_write_c22_to_c45(phydev, 3, 0x8033, 0xC801);

	/* send_s detection threshold, slave and master */
	phy_write_c22_to_c45(phydev, 7, 0x8032, 0x2020);
	phy_write_c22_to_c45(phydev, 7, 0x8031, 0xA28);
	phy_write_c22_to_c45(phydev, 7, 0x8031, 0xC28);

	/* disable DCL calibratin during tear down */
	phy_write_c22_to_c45(phydev, 3, 0xFFDB, 0xFC10);

	/* disable RESET of DCL*/
	phy_write_c22_to_c45(phydev, 3, 0xFE1B, 0x58);

	/* update Initial FFE Coefficients */
	phy_write_c22_to_c45(phydev, 3, 0xFBBA, 0x0CB2);
	phy_write_c22_to_c45(phydev, 3, 0xFBBB, 0x0C4A);

	/* Toggle Squelch override */
	phy_write_c22_to_c45(phydev, 4, 0x8178, 0x7540);
	phy_write_c22_to_c45(phydev, 4, 0x8178, 0x5540);

	/* aneg set pga gain and amp */
	phy_write_c22_to_c45(phydev, 3, 0xFE5F, 0xE8);
	phy_write_c22_to_c45(phydev, 3, 0xFE05, 0x755C);
}

static void apply_ge_init(struct phy_device *phydev, unsigned int need_prepare)
{
	struct mrvl_88q222x *mrvl_dev = (struct mrvl_88q222x *)phydev->priv;

	if (mrvl_dev->rev_num != MRVL_Q222X_B1) {
		pr_info("[INFO][GMAC] %s: revision is not B1\n", __func__);

		/* call rev B0's per mode init sequence */
		if (need_prepare == MRVL_Q222X_TRUE) {
			/* prepare to write init code */
			apply_b0_preinit(phydev);
		}

		/* send_s detection threshold, slave and master */
		phy_write_c22_to_c45(phydev, 7, 0x8032, 0x2020);
		phy_write_c22_to_c45(phydev, 7, 0x8031, 0xA28);
		phy_write_c22_to_c45(phydev, 7, 0x8031, 0xC28);

		/* disable DCL calibration during tear down */
		phy_write_c22_to_c45(phydev, 7, 0xFFDB, 0xFC10);

		/* disable RESET of DCL */
		phy_write_c22_to_c45(phydev, 7, 0xFE1B, 0x58);
	} else {
		pr_info("[INFO][GMAC] %s: revision is B1\n", __func__);

		/* call rev B1's general init sequence */
		apply_b1_init(phydev);
	}
}

static void apply_sgmii_init(struct phy_device *phydev)
{
	pr_info("[INFO][GMAC] %s: in\n", __func__);
}

static void q222x_init(struct phy_device *phydev)
{
	int reg_val;
	unsigned int op_mode = MRVL_APHY_OP_MASTER;
	unsigned int support_sgmii;
	unsigned int status = MRVL_APHY_STATUS_OK;

	q222x_check_reg_access(phydev);

	/* if reg access is ok, start to initialize device */
	if (status == MRVL_APHY_STATUS_OK) {
		q222x_check_support_sgmii(phydev, &support_sgmii);

		apply_ge_init(phydev, MRVL_Q222X_TRUE);

		if (support_sgmii == MRVL_Q222X_TRUE) {
			apply_sgmii_init(phydev);
		}

		/* write speed and mode */
		reg_val = phy_read_c22_to_c45(phydev, 1, 0x0834);
		reg_val = (reg_val & 0xFFF0U) | 0x0001U;
		if (op_mode == MRVL_APHY_OP_MASTER) {
			reg_val |= 0x4000U;
		} else {
			reg_val &= 0xBFFFU;
		}
		phy_write_c22_to_c45(phydev, 1, 0x0834, reg_val);

		/* Add software Reset to finish */
		q222x_soft_reset(phydev);
	} else {
		pr_err("[ERR][GMAC] %s: fail to access register!\n", __func__);
	}
}

static int q222x_config_init(struct phy_device *phydev)
{
	int is_linkup;
	unsigned int is_master;
	unsigned int speed;

	q222x_init(phydev);

	/* wait for linkup, 100ms */
	//usleep_range(100000, 200000);
	msleep(100);

	q222x_check_link(phydev, &is_linkup);
	q222x_check_is_master(phydev, &is_master);
	q222x_get_speed(phydev, &speed);
	//pr_info("[INFO][GMAC] %s: Link Status: %s\n", __func__,
	//		is_linkup == MRVL_Q222X_TRUE ? "Up" : "Down");

	q222x_get_latched_link_status(phydev, &is_linkup);
	//pr_info("[INFO][GMAC] %s: Latched Link Status 1: %s\n", __func__,
	//		is_linkup == MRVL_Q222X_TRUE ? "Up" : "Down");

	q222x_get_real_time_link_status(phydev, &is_linkup);
	//pr_info("[INFO][GMAC] %s: Real time Link Status : %s\n", __func__,
	//		is_linkup == MRVL_Q222X_TRUE ? "Up" : "Down");
	
	q222x_get_latched_link_status(phydev, &is_linkup);
	//pr_info("[INFO][GMAC] %s: Latched Link Status 2: %s\n",
	//		__func__, is_linkup == MRVL_Q222X_TRUE ? "Up" : "Down");

	//pr_info("[INFO][GMAC] %s: Master/Slave: %s\n", __func__,
	//		is_master == MRVL_Q222X_MASTER ? "Master" : "Slave");
	//pr_info("[INFO][GMAC] %s: Speed: %s\n", __func__,
	//		speed == MRVL_Q222X_SPEED_1000 ? "GE/1000" : "Fe/100");

	return 0;
}
#if 0	// for aneg
static int apply_aneg_init(struct phy_device *phydev, unsigned int flags)
{
	unsigned int status = MRVL_APHY_STATUS_OK;
	unsigned int an_ability = (flags & (MRVL_Q222X_AN_GE_ABILITY | MRVL_Q222X_AN_FE_ABILITY));
	struct mrvl_88q222x *mrvl_dev = (struct mrvl_88q222x *)phydev->priv;

	pr_info("[INFO][GMAC] %s: an_ability = %d\n", __func__, an_ability);

	/* validate aneg ability */
	if (an_ability == 0x0) {
		/* disable AN if data rates are invalid */
		phy_write_c22_to_c45(phydev, 7, 0x0203, MRVL_Q222X_AN_DISABLE);
		phy_write_c22_to_c45(phydev, 7, 0x0200, MRVL_Q222X_AN_RESET);
		status = MRVL_APHY_STATUS_ERROR_NOTSUPPORT;
	} else {
		if (mrvl_dev->rev_num != MRVL_Q222X_B1) {
			/* call rev B0's per mode init sequence */
			pr_info("[INFO][GMAC] %s: B0's init sequence\n", __func__);

			/* prepare to write init code */
			apply_b0_preinit(phydev);
			/* apply Fe/speed 100 init code */
			//applyFeInit(device, MRVL_APHY_FALSE);
			/* apply Ge/speed 1000 init code */
			apply_ge_init(phydev, MRVL_Q222X_FALSE);
			/* aneg set pga gain and amp */
			phy_write_c22_to_c45(phydev, 3, 0xFE5F, 0xE8);
			phy_write_c22_to_c45(phydev, 3, 0xFE05, 0x755C);
		} else {
			/* call rev B1's general init sequence */
			pr_info("[INFO][GMAC] %s: B1's init sequence\n", __func__);

			apply_b1_init(phydev);
		}
		/* set Ability registers */
		phy_write_c22_to_c45(phydev, 7, 0x0203, an_ability);
	}

	return status;
}

static int q222x_set_aneg(struct phy_device *phydev)
{
	int reg_val;
	unsigned int support_sgmii;
	unsigned int op_mode = MRVL_APHY_OP_MASTER;
	unsigned int status = MRVL_APHY_STATUS_OK;

	status = q222x_check_reg_access(phydev);
	if (status == MRVL_APHY_STATUS_OK) {
		/* validate speed selection and write init code */
		status = apply_aneg_init(phydev, MRVL_Q222X_AN_GE_ABILITY);
	} else {
		pr_err("[ERR][GMAC] %s: fail to access register\n", __func__);
	}

	if (status == MRVL_APHY_STATUS_OK) {
		q222x_check_support_sgmii(phydev, &support_sgmii);
		if (support_sgmii == MRVL_Q222X_TRUE) {
			pr_info("[INFO][GMAC] %s: support SGMII\n", __func__);

			apply_sgmii_init(phydev);
		}

		/* prefer master if needed */
		reg_val = phy_read_c22_to_c45(phydev, 7, 0x0203);

		if (op_mode == MRVL_APHY_OP_MASTER) {
			reg_val |= 0x10U;
		} else {
			reg_val &= 0xFFEFU;
		}
		phy_write_c22_to_c45(phydev, 7, 0x0203, reg_val);

		/* enable auto-neg */
		phy_write_c22_to_c45(phydev, 7, 0x0200, MRVL_Q222X_AN_ENABLE);

		/* add software reset to finish */
		q222x_soft_reset(phydev);
	}

	return status;
}

static int q222x_get_aneg_done(struct phy_device *phydev)
{
	int reg_val, is_done;

	reg_val = phy_read_c22_to_c45(phydev, 7, 0x0201);
	is_done = (0x0 != (reg_val & MRVL_Q222X_AN_COMPLETE)) ? MRVL_Q222X_TRUE : MRVL_Q222X_FALSE;

	return is_done;
}

static int q222x_config_aneg(struct phy_device *phydev)
{
	int aneg_done;
	int is_linkup;
	unsigned int is_master;
	unsigned int speed;

	pr_info("[INFO][GMAC] %s: in\n", __func__);

	/* 1) Call Sample_Code_Assign_Device_PTR if device has not been checked */

	/* 2) enable aneg */
	q222x_set_aneg(phydev);

	/* 3) wait for linkup, 100ms */
	msleep(100);
	//mdelay(100);

	/* 4) check status */
	aneg_done = q222x_get_aneg_done(phydev);

	q222x_check_link(phydev, &is_linkup);
	q222x_check_is_master(phydev, &is_master);
	q222x_get_speed(phydev, &speed);
	pr_info("[INFO][GMAC] %s: Aneg Finished: %s\n", __func__,
			aneg_done == MRVL_Q222X_TRUE ? "Yes" : "No");
	pr_info("[INFO][GMAC] %s: Link Status: %s\n", __func__,
			is_linkup == MRVL_Q222X_TRUE ? "Up" : "Down");
	q222x_get_latched_link_status(phydev, &is_linkup);
	pr_info("[INFO][GMAC] %s: Latched Link Status 1: %s\n", __func__,
			is_linkup == MRVL_Q222X_TRUE ? "Up" : "Down");
	q222x_get_real_time_link_status(phydev, &is_linkup);
	pr_info("[INFO][GMAC] %s: Real Time Link Status: %s\n", __func__,
			is_linkup == MRVL_Q222X_TRUE ? "Up" : "Down");
	q222x_get_latched_link_status(phydev, &is_linkup);
	pr_info("[INFO][GMAC] %s: Latched Link Status 2: %s\n", __func__,
			is_linkup == MRVL_Q222X_TRUE ? "Up" : "Down");

	pr_info("[INFO][GMAC] %s: Master/Slave: %s\n", __func__,
			is_master == MRVL_Q222X_MASTER ? "Master" : "Slave");
	pr_info("[INFO][GMAC] %s: Speed: %s\n", __func__,
			speed == MRVL_Q222X_SPEED_1000 ? "GE/1000" : "Fe/100");

	return 0;
}

static int q222x_aneg_done(struct phy_device *phydev)
{
	pr_info("[INFO][GMAC] %s: in\n", __func__);

	return 0;
}
#endif

static int q222x_match_phy_device(struct phy_device *phydev)
{
	int phy_id = phy_read_c22_to_c45(phydev, MRVL_ID_DEVICE, MRVL_APHY_ID_REG);

	//pr_info("[INFO][GMAC] %s: phy_id(with rev_num) = 0x%08x\n",
	//		__func__, (unsigned int)phy_id);

	phy_id = (phy_id & 0x03F0U) >> 4;

	//pr_info("[INFO][GMAC] %s: phy_id = 0x%08x\n",
	//		__func__, (unsigned int)phy_id);

	return (phy_id == PHY_ID_MARVELL_Q2220);
}

static int q222x_probe(struct phy_device *phydev)
{
	int ret = 0;
	unsigned int status = MRVL_APHY_STATUS_OK;
	struct mrvl_88q222x *mrvl_dev = NULL;

	mrvl_dev = devm_kzalloc(&phydev->mdio.dev, sizeof(mrvl_dev), GFP_KERNEL);
	if (mrvl_dev == NULL) {
		ret = -ENOMEM;
	} else {
		phydev->priv = mrvl_dev;
		status = q222x_get_device_info(phydev);
		if (status != MRVL_APHY_STATUS_OK) {
			pr_err("[ERR][GMAC] %s: fail to get device info.\n", __func__);
		} else {
			pr_info("[INFO][GMAC] %s: success to get device info.\n",
					__func__);
		}
	}

	return ret;
}
#endif

static struct phy_driver tcc_marvell_driver[] = {
	{
		.phy_id = PHY_ID_MARVELL_Q2110,
		.phy_id_mask = PHY_ID_MARVELL_MASK,
		.name = PHY_NAME_MARVELL_Q2110,
		.features = PHY_GBIT_FIBRE_FEATURES,
		.config_init = &q2110_config_init,
		.read_status = &q2110_read_status,
		.config_aneg = &q2110_aneg_done,
		.aneg_done = &q2110_aneg_done,
		.match_phy_device = &q2110_match_phy_device,
	},
	{
		.phy_id = PHY_ID_MARVELL_Q1110,
		.phy_id_mask = PHY_ID_MARVELL_MASK,
		.name = PHY_NAME_MARVELL_Q1110,
		.features = PHY_BASIC_FEATURES,
		.config_init = &q1110_config_init,
		.read_status = &q1110_read_status,
		.config_aneg = &q1110_config_aneg,
		.aneg_done = &q1110_aneg_done,
		.match_phy_device = &q1110_match_phy_device,
	},
#if defined(SUPPORT_MRVL_88Q2220_PHY)
	{
		.phy_id = PHY_ID_MARVELL_Q2220,
		.phy_id_mask = PHY_ID_MARVELL_MASK,
		.name = PHY_NAME_MARVELL_Q2220,
		.features = PHY_GBIT_FIBRE_FEATURES,
		.probe = &q222x_probe,
		.soft_reset = q222x_soft_reset,
		.config_init = &q222x_config_init,
		.read_status = &q222x_read_status,
		//.config_aneg = &q222x_config_aneg,
		//.aneg_done = &q222x_aneg_done,
		.match_phy_device = &q222x_match_phy_device,
	},
#endif
};

module_phy_driver(tcc_marvell_driver);

static struct mdio_device_id __maybe_unused tcc_marvell_tbl[] = {
	{ PHY_ID_MARVELL_Q2110, PHY_ID_MARVELL_MASK},
	{ PHY_ID_MARVELL_Q1110, PHY_ID_MARVELL_MASK},
#if defined(SUPPORT_MRVL_88Q2220_PHY)
	{ PHY_ID_MARVELL_Q2220, PHY_ID_MARVELL_MASK},
#endif
	{}
};

MODULE_DEVICE_TABLE(mdio, tcc_marvell_tbl);
