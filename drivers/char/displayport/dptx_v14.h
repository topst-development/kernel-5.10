/*
 * Copyright (c) 2016 Synopsys, Inc.
 *
 * Synopsys DP TX Linux Software Driver and documentation (hereinafter,
 * "Software") is an Unsupported proprietary work of Synopsys, Inc. unless
 * otherwise expressly agreed to in writing between Synopsys and you.
 *
 * The Software IS NOT an item of Licensed Software or Licensed Product under
 * any End User Software License Agreement or Agreement for Licensed Product
 * with Synopsys or any supplement thereto. You are permitted to use and
 * redistribute this Software in source and binary forms, with or without
 * modification, provided that redistributions of source code must retain this
 * notice. You may not view, use, disclose, copy or distribute this file or
 * any information contained herein except pursuant to this license grant from
 * Synopsys. If you do not agree with this notice, including the disclaimer
 * below, then you are not authorized to use the Software.
 *
 * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
*/

/*
* Modified by Telechips Inc.
*/

#ifndef DPTX_DRV_V14_HEADER
#define DPTX_DRV_V14_HEADER

#include <linux/compat.h>
#include <linux/irqreturn.h>
#include <linux/regmap.h>
#if defined(CONFIG_TOUCHSCREEN_INIT_SERDES)
#include <linux/input/tcc_tsc_serdes.h>
#endif

#define TCC_DPTX_DRV_MAJOR_VER			3
#define TCC_DPTX_DRV_MINOR_VER			2
#define TCC_DPTX_DRV_SUBTITLE_VER		2

#define TCC80XX_REVISION_ES			0x00
#define TCC80XX_REVISION_CS			0x01
#define TCC80XX_REVISION_BX			0x02

#define DP_DDIBUS_BASE_REG_ADDRESS		0x12400000u
#define DP_MICOM_BASE_REG_ADDRESS		0x1BD00000u
#define DP_HDCP_OFFSET				0x00040000u
#define DP_REGISTER_BANK_OFFSET			0x00080000u
#define DP_CKC_OFFSET				0x000C0000u
#define DP_PROTECT_OFFSET			0x000D0000u
#define DP_SEC_PHY_OFFSET			0x000E0000u
#define DP_MAX_OFFSET				0x01000000u

/* 1025 : 1024x600 : AV080WSM-NW0*/
#define DP_CUSTOM_1025_DTD_VIC			1025
/* 1026 : 5760x900@54p */
#define DP_CUSTOM_1026_DTD_VIC			1026
/* 1027 : 1920x720 : PVLBJT_020_01 */
#define DP_CUSTOM_1027_DTD_VIC			1027
/* 1028 : 1920x720 : BOE */
#define DP_CUSTOM_1028_DTD_VIC			1028
#define DP_CUSTOM_MAX_DTD_VIC			1030

#define DPTX_SINK_CAP_SIZE			0x100
#define DPTX_SDP_NUM				0x10
#define DPTX_SDP_LEN				0x9
#define DPTX_SDP_SIZE				(9 * 4)

#define DPTX_MAX_LINK_LANES			4
#define DPTX_MAX_LINK_SYMBOLS			64
#define DPTX_MAX_LINK_SLOTS			64

#define DP_LINK_STATUS_SIZE			6

#define EDID_I2C_OVER_AUX_ADDR			0x50
#define EDID_I2C_OVER_AUX_SEGMENT_ADDR		0x30
#define INVALID_MST_PORT_NUM			0xFF
#define DPTX_EDID_BUFLEN			512
#define DPTX_ONE_EDID_BLK_LEN			128

#define DPTX_KEEP_PAYLOAD			(bool)false
#define DPTX_CLEAR_PAYLOAD			(bool)true

#define DPTX_RETURN_NO_ERROR			0
#define DPTX_RETURN_EPERM			EPERM  /*Operation not permitted*/
#define DPTX_RETURN_ENOENT			ENOENT /*No such file or directory*/
#define DPTX_RETURN_ENOMEM			ENOMEM /*Out of memory*/
#define DPTX_RETURN_EACCES			EACCES /*Permission denied*/
#define DPTX_RETURN_EBUSY			EBUSY  /*Device or resource busy*/
#define DPTX_RETURN_ENODEV			ENODEV /*No such device*/
#define DPTX_RETURN_EINVAL			EINVAL /*Invalid argument*/
#define DPTX_RETURN_ENOSPC			ENOSPC /*No space left on device*/
#define DPTX_RETURN_ESPIPE			ESPIPE /*Illegal seek*/
#define DPTX_RETURN_I2C_OVER_AUX_NO_ACK 	1000 /* No ack from I2C Over Aux */
#define DPTX_RETURN_MST_ACT_TIMEOUT 		1001 /* MST Act timeout */

#define DPTX_RETURN_SUCCESS(ret)		((ret) == DPTX_RETURN_NO_ERROR)
#define DPTX_RETURN_ERROR(ret)			((ret) != DPTX_RETURN_NO_ERROR)

enum PHY_DEVICE_MODEL {
	PHY_DEVICE_SNPS = 0,
	PHY_DEVICE_SEC = 1,
	PHY_DEVICE_UNKNOWN = 2
};

enum TCC80XX_EVB_TYPE {
	TCC8059_EVB_01			= 0,
	TCC8050_SV_01			= 1,
	TCC8050_SV_10			= 2,
	TCC8070_SV_01			= 3,
	TCC805X_EVB_UNKNOWN		= 0xFE
};

enum TCC_EVB_LCD_POW_TYPE {
	TCC_EVB_LCD_ONE_POW = 0,
	TCC_EVB_LCD_FOUR_POW = 1,
	TCC_EVB_LCD_POW_MAX = 2
};

enum DP_MUX_INDEX {
	DP_MUX_0	= 0,
	DP_MUX_1	= 1,
	DP_MUX_2	= 2,
	DP_MUX_3	= 3,
	DP_MUX_MAX	= 4
};


#define DIV_CFG_CLK_INVALID 0
#define DIV_CFG_CLK_400HMZ 0x81
#define DIV_CFG_CLK_200HMZ 0x83
#define DIV_CFG_CLK_160HMZ 0xB1
#define DIV_CFG_CLK_100HMZ 0x87
#define DIV_CFG_CLK_40HMZ 0x93

enum PHY_POWER_STATE {
	PHY_POWER_ON = 0,
	PHY_POWER_DOWN_SWITCHING_RATE		= 0x02,
	PHY_POWER_DOWN_PHY_CLOCK		= 0x03,
	PHY_POWER_DOWN_REF_CLOCK		= 0x0C
};

enum PHY_LINK_RATE {
	LINK_RATE_RBR = 0,
	LINK_RATE_HBR,
	LINK_RATE_HBR2,
	LINK_RATE_HBR3,
	LINK_RATE_MAX
};

enum PHY_PRE_EMPHASIS_LEVEL {
	PRE_EMPHASIS_LEVEL_0 = 0,
	PRE_EMPHASIS_LEVEL_1,
	PRE_EMPHASIS_LEVEL_2,
	PRE_EMPHASIS_LEVEL_3,
	PRE_EMPHASIS_LEVEL_MAX
};

enum PHY_VOLTAGE_SWING_LEVEL {
	VOLTAGE_SWING_LEVEL_0 = 0,
	VOLTAGE_SWING_LEVEL_1,
	VOLTAGE_SWING_LEVEL_2,
	VOLTAGE_SWING_LEVEL_3,
	VOLTAGE_SWING_LEVEL_MAX
};

enum PHY_INPUT_STREAM_INDEX {
	PHY_INPUT_STREAM_0	= 0u,
	PHY_INPUT_STREAM_1	= 1u,
	PHY_INPUT_STREAM_2	= 2u,
	PHY_INPUT_STREAM_3	= 3u,
	PHY_INPUT_STREAM_MAX	= 4u
};

enum PHY_LANE_INDEX {
	PHY_LANE_0		= 0,
	PHY_LANE_1		= 1,
	PHY_LANE_2		= 2,
	PHY_LANE_3		=  3,
	PHY_LANE_4		= 4,
	PHY_LANE_MAX	= 5
};

enum PHY_DATA_WIDTH {
	PHY_DATA_WIDTH_20BITS = 0,
	PHY_DATA_WIDTH_40BITS = 1,
	PHY_DATA_WIDTH_MAX = 2
};

enum MST_INPUT_PORT_TYPE {
	INPUT_PORT_TYPE_TX			= 0,
	INPUT_PORT_TYPE_RX			= 1,
	INPUT_PORT_TYPE_INVALID		= 2
};

enum MST_BRANCH_UNIT_INDEX {
	FIRST_MST_BRANCH_UNIT			= 0,
	SECOND_MST_BRANCH_UNIT			= 1,
	THIRRD_MST_BRANCH_UNIT			= 2,
	INVALID_MST_BRANCH_UNIT			= 3
};

enum MST_PEER_DEV_TYPE {
	PEER_NO_DEV_CONNECTED			= 0,
	PEER_SOURCE_DEV					= 1,
	PEER_BRANCHING_DEV				= 2,
	PEER_STREAM_SINK_DEV			= 3,
	PEER_DP_TO_LEGECY_CONV			= 4,
	PEER_DP_TO_WIRELESS_CONV		= 5,
	PEER_WIRELESS_TO_DP_CONV		= 6
};

enum SER_DES_INPUT_INDEX {
	SER_INPUT_INDEX_0		= 0,
	DES_INPUT_INDEX_0		= 1,
	DES_INPUT_INDEX_1		= 2,
	DES_INPUT_INDEX_2		= 3,
	DES_INPUT_INDEX_3		= 4,
	SER_DES_INPUT_INDEX_MAX		= 5
};

enum VIDEO_RGB_TYPE {
	RGB_STANDARD_LEGACY		= 0,
	RGB_STANDARD_S			= 1,
	RGB_STANDARD_MAX		= 2
};

enum VIDEO_COLORIMETRY_TYPE {
	COLORIMETRY_ITU601		= 1,
	COLORIMETRY_ITU709		= 2,
	COLORIMETRY_MAX			= 3
};

enum VIDEO_PIXEL_BIT_DEPTH {
	PIXEL_BPC_6_BITS		= 6,
	PIXEL_BPC_8_BITS		= 8,
	PIXEL_BPC_10_BITS		= 10,
	PIXEL_BPC_12_BITS		= 12,
	PIXEL_BPC_16_BITS		= 16,
	PIXEL_BPC_MAX			= 17
};

enum VIDEO_COLOR_DEPTH {
	PIXEL_16_BPP		= 16,
	PIXEL_18_BPP		= 18,
	PIXEL_24_BPP		= 24,
	PIXEL_30_BPP		= 30,
	PIXEL_36_BPP		= 36,
	PIXEL_48_BPP		= 48,
	PIXEL_INVALID_BPP	= 0xFF
};

enum VIDEO_COLOR_BYTE_DEPTH {
	VIDEO_LINK_BPP_YCbCr422			= 2,
	VIDEO_LINK_BPP_RGB_YCbCr444	= 3,
	VIDEO_LINK_BPP_INVALID			= 0xFF,
};

enum VIDEO_FORMAT_TYPE {
	VIDEO_FORMAT_CEA_861		= 0,
	VIDEO_FORMAT_VESA_CVT		= 1,
	VIDEO_FORMAT_VESA_DMT		= 2,
	VIDEO_FORMAT_MAX
};

enum VIDEO_DYNAMIC_RANGE_TYPE {
	VIDEO_DYNAMIC_RANGE_VESA     = 0,
	VIDEO_DYNAMIC_RANGE_CEA = 1,
	VIDEO_DYNAMIC_RANGE_MAX
};

enum VIDEO_REFRESH_RATE {
	VIDEO_REFRESH_RATE_30_00HZ = 30000,
	VIDEO_REFRESH_RATE_49_92HZ = 49920,
	VIDEO_REFRESH_RATE_50_00HZ = 50000,
	VIDEO_REFRESH_RATE_50_08HZ = 50080,
	VIDEO_REFRESH_RATE_59_94HZ = 59940,
	VIDEO_REFRESH_RATE_60_00HZ = 60000,
	VIDEO_REFRESH_RATE_60_54HZ = 60540,
	VIDEO_REFRESH_RATE_MAX			= 60541
};

enum VIDEO_PIXEL_ENCODING_TYPE {
	PIXEL_ENCODING_TYPE_RGB				= 0,
	PIXEL_ENCODING_TYPE_YCBCR422		= 2,
	PIXEL_ENCODING_TYPE_YCBCR444		= 4,
	PIXEL_ENCODING_TYPE_MAX
};

enum VIDEO_MULTI_PIXEL_TYPE {
	MULTI_PIXEL_TYPE_SINGLE	= 0,
	MULTI_PIXEL_TYPE_DUAL		= 1,
	MULTI_PIXEL_TYPE_QUAD		= 2,
	MULTI_PIXEL_TYPE_MAX
};

enum DPTX_VIDEO_PATTERN_MODE {
	VIDEO_PATTERN_NONE = 0,
	VIDEO_PATTERN_RAMP = 1,
	VIDEO_PATTERN_VERITCAL_LINES = 2,
	VIDEO_PATTERN_COLOR_SQUARE = 3,
	VIDEO_PATTERN_MAX
};

enum AUDIO_INPUT_MUTE {
	AUDIO_INPUT_CLEAR_MUTE_FLAG_VBID		= 0,
	AUDIO_INPUT_SET_MUTE_FLAG_VBID			= 1,
	AUDIO_INPUT_SET_MUTE_FLAG_MAX			= 2
};

enum AUDIO_INPUT_INTERFACE {
	AUDIO_INPUT_INTERFACE_I2S		= 0,
	AUDIO_INPUT_INTERFACE_INVALID
};

enum AUDIO_MAX_INPUT_DATA_WIDTH {
	MAX_INPUT_DATA_WIDTH_16BIT = 16,
	MAX_INPUT_DATA_WIDTH_17BIT = 17,
	MAX_INPUT_DATA_WIDTH_18BIT = 18,
	MAX_INPUT_DATA_WIDTH_19BIT = 19,
	MAX_INPUT_DATA_WIDTH_20BIT = 20,
	MAX_INPUT_DATA_WIDTH_21BIT = 21,
	MAX_INPUT_DATA_WIDTH_22BIT = 22,
	MAX_INPUT_DATA_WIDTH_23BIT = 23,
	MAX_INPUT_DATA_WIDTH_24BIT = 24,
	MAX_INPUT_DATA_WIDTH_INVALID = 25
};

enum AUDIO_INPUT_MAX_NUM_OF_CH {
	INPUT_MAX_1_CH			= 0,
	INPUT_MAX_2_CH			= 1,
	INPUT_MAX_3_CH			= 2,
	INPUT_MAX_4_CH			= 3,
	INPUT_MAX_5_CH			= 4,
	INPUT_MAX_6_CH			= 5,
	INPUT_MAX_7_CH			= 6,
	INPUT_MAX_8_CH			= 7
};

enum AUDIO_EDID_MAX_SAMPLE_FREQ {
	SAMPLE_FREQ_32			= 0,
	SAMPLE_FREQ_44_1		= 1,
	SAMPLE_FREQ_48			= 2,
	SAMPLE_FREQ_88_2		= 3,
	SAMPLE_FREQ_96			= 4,
	SAMPLE_FREQ_176_4		= 5,
	SAMPLE_FREQ_192			= 6,
	SAMPLE_FREQ_INVALID		= 7
};

enum AUDIO_IEC60958_3_SAMPLE_FREQ {
	IEC60958_3_SAMPLE_FREQ_44_1			= 0,
	IEC60958_3_SAMPLE_FREQ_88_2			= 1,
	IEC60958_3_SAMPLE_FREQ_22_05		= 2,
	IEC60958_3_SAMPLE_FREQ_176_4		= 3,
	IEC60958_3_SAMPLE_FREQ_48			= 4,
	IEC60958_3_SAMPLE_FREQ_96			= 5,
	IEC60958_3_SAMPLE_FREQ_24			= 6,
	IEC60958_3_SAMPLE_FREQ_192			= 7,
	IEC60958_3_SAMPLE_FREQ_32			= 12,
	IEC60958_3_SAMPLE_FREQ_INVALID		= 13
};

enum AUDIO_IEC60958_3_ORIGINAL_SAMPLE_FREQ {
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_16			= 1,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_32			= 3,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_12			= 4,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_11_025		= 5,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_8			= 6,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_192			= 8,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_24			= 9,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_96			= 10,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_48			= 11,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_176_4		= 12,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_22_05		= 13,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_88_2			= 14,
	IEC60958_3_ORIGINAL_SAMPLE_FREQ_44_1			= 15
};

enum DMT_ESTABLISHED_TIMING {
	DMT_640x480_60hz,
	DMT_800x600_60hz,
	DMT_1024x768_60hz,
	DMT_NOT_SUPPORTED
};

enum HPD_Detection_Status {
	HPD_STATUS_UNPLUGGED = 0,
	HPD_STATUS_PLUGGED
};

enum HDCP_Detection_Status {
	HDCP_STATUS_NOT_DETECTED = 0,
	HDCP_STATUS_DETECTED
};

enum AUX_REPLY_Status {
	AUX_REPLY_NOT_RECEIVED = 0,
	AUX_REPLY_RECEIVED
};


typedef void (*Dptx_HPD_Intr_Callback)(u8 ucDP_Index, bool bHPD_State);
typedef int (*Dptx_Str_Resume_Callback)(void);
typedef int (*Dptx_Panel_Topology_Callback)(uint8_t *pucNumOfPorts);


struct Dptx_Link_Params {
	u8		aucTraining_Status[DP_LINK_STATUS_SIZE];
	u8		ucLinkRate;
	u8		ucNumOfLanes;
	u8		aucPreEmphasis_level[PRE_EMPHASIS_LEVEL_MAX];
	u8		aucVoltageSwing_level[VOLTAGE_SWING_LEVEL_MAX];
};

struct Dptx_Aux_Params {
	u32			uiAuxStatus;
	u32			auiReadData[4];
};

struct Dptx_Dtd_Params {
	u8		interlaced;
	u8		h_sync_polarity;
	u8		v_sync_polarity;
	u16		pixel_repetition_input;
	u16		h_active;
	u16		h_blanking;
	u16		h_image_size;
	u16		h_sync_offset;
	u16		h_sync_pulse_width;
	u16		v_active;
	u16		v_blanking;
	u16		v_image_size;
	u16		v_sync_offset;
	u16		v_sync_pulse_width;
	u32		uiPixel_Clock;
};

struct Dptx_Video_Params {
	u8		ucMultiPixel;
	u8		ucPixel_Encoding;
	u8		ucBitPerComponent;
	u8		ucBitPerPixel;
	u8		ucRGB_Standard;
	u8		ucAverage_BytesPerTu;
	u8		ucAver_BytesPer_Tu_Frac;
	u8		ucInit_Threshold;
	u8		ucVideo_Format;
	u8		ucColorimetry;
	u8		ucDynamicRange;
	u8		ucPatternMode;
	u32		uiPeri_Pixel_Clock[PHY_INPUT_STREAM_MAX];
	u32		auiVideo_Code[PHY_INPUT_STREAM_MAX];
	u32		uiRefresh_Rate;

	struct Dptx_Dtd_Params	stDtdParams[PHY_INPUT_STREAM_MAX];
};

struct Dptx_Params {
	struct mutex Mutex;
	struct device *pstParentDev;
	wait_queue_head_t WaitQ;

	/* It is used to changes link rate */
	struct completion hpd_plug_comp;
	volatile unsigned long wait_flags;

	atomic_t HPD_IRQ_State;
	atomic_t Sink_request;

	void __iomem *pioDPLink_BaseAddr;
	void __iomem *pioMIC_SubSystem_BaseAddr;
	void __iomem *pioPMU_BaseAddr;

	uint32_t uiHDCP22_RegAddr_Offset;
	uint32_t uiRegBank_RegAddr_Offset;
	uint32_t uiCKC_RegAddr_Offset;
	uint32_t uiProtect_RegAddr_Offset;
	uint32_t uiSEC_PHY_Reg_Offset;

	bool bSideBand_MSG_Supported;
	bool bPhy_Lane_Std;
	bool bSDM_Bypass;
	bool bTRVC_Bypass;
	bool bSpreadSpectrum_Clock;
	bool bMultStreamTransport;

	uint8_t ucNumOfPorts;
	uint8_t ucMax_Rate;
	uint8_t ucMax_Lanes;
	uint8_t aucVCP_Id[PHY_INPUT_STREAM_MAX];
	uint8_t aucMux_Idx[PHY_INPUT_STREAM_MAX];
	uint8_t aucDPCD_Caps[DPTX_SINK_CAP_SIZE];
	uint8_t aucNumOfSlots[PHY_INPUT_STREAM_MAX];
	uint8_t aucStreamSink_PortNumber[PHY_INPUT_STREAM_MAX];
	uint8_t aucRAD_PortNumber[PHY_INPUT_STREAM_MAX];
	uint8_t *pucEdidBuf;
	uint8_t *paucEdidBuf_Entry[PHY_INPUT_STREAM_MAX];
	uint16_t ausPayloadBandwidthNumber[PHY_INPUT_STREAM_MAX];

	int32_t iHPD_IRQ;
	uint32_t uiTCC80xx_Rev;

	enum PHY_DEVICE_MODEL ePhy_Dev;
	enum HPD_Detection_Status eLast_HPDStatus;
	enum DMT_ESTABLISHED_TIMING eEstablished_Timing;

	struct Dptx_Video_Params stVideoParams;
	struct Dptx_Aux_Params stAuxParams;
	struct Dptx_Link_Params	 stDptxLink;

	struct proc_dir_entry *pstDP_Proc_Dir;
	struct proc_dir_entry *pstDP_HPD_Dir;
	struct proc_dir_entry *pstDP_Topology_Dir;
	struct proc_dir_entry *pstDP_EDID_Dir;
	struct proc_dir_entry *pstDP_LinkT_Dir;
	struct proc_dir_entry *pstDP_Video_Dir;
	struct proc_dir_entry *pstDP_Auio_Dir;

	struct device_link *pstI2cLink;

	Dptx_HPD_Intr_Callback pvHPD_Intr_CallBack;
	Dptx_Str_Resume_Callback pvStr_Resume_CallBack;
	Dptx_Panel_Topology_Callback pvPanel_Topology_CallBack;
};


struct Dptx_Params *Dpv14_Tx_Get_Device_Handle(void);


int32_t Dptx_Cfg_Init_Params(struct Dptx_Params *pstDptx);
int32_t Dptx_Cfg_Init(struct Dptx_Params *pstDptx, uint8_t ucLinkRate);
int32_t Dptx_Cfg_Reset(struct Dptx_Params *pstDptx, uint8_t ucLinkRate);
int32_t Dptx_Cfg_SoftReset(struct Dptx_Params *pstDptx, uint32_t uiVal);
int32_t Dptx_Cfg_Get_SDM_Bypass(struct Dptx_Params *pstDptx, bool *pbSdm_Bypass);
int32_t Dptx_Cfg_Get_TRVC_Bypass(struct Dptx_Params *pstDptx, bool *pbTrvc_Bypass);
int32_t Dptx_Cfg_Get_MuxSelect(struct Dptx_Params *pstDptx, uint8_t ucDP_Idx, uint8_t *pucMux_Index);
int32_t Dptx_Cfg_Get_PHY_Standard_LaneCfg(struct Dptx_Params *pstDptx, bool *pbPhy_LaneCfg);
int32_t Dptx_Cfg_Set_PHY_Cfg(struct Dptx_Params *pstDptx, uint32_t uiCfg_Val);
int32_t Dptx_Cfg_Check_PHY_Pll(struct Dptx_Params *pstDptx, bool *pbPll_Ready);
int32_t Dptx_Cfg_Check_PHY_Ready(struct Dptx_Params *pstDptx, bool *pbPhy_Ready);
int32_t Dptx_Cfg_Check_Sec_PHY_SB_Ready(struct Dptx_Params *pstDptx, bool *pbSb_Ready);



/* Dptx Core */
int32_t Dptx_Core_Init_Params(struct Dptx_Params *pstDptx);
int32_t Dptx_Core_Init(struct Dptx_Params *pstDptx);
int32_t Dptx_Core_Deinit(struct Dptx_Params *pstDptx);
int32_t Dptx_Core_Clear_General_Interrupt(struct Dptx_Params *pstDptx, uint32_t uiClear_Bits);
int32_t Dptx_Core_Get_PHY_BUSY_Status(struct Dptx_Params *dptx, uint8_t ucNumOfLanes);
int32_t Dptx_Core_Get_PHY_NumOfLanes(struct Dptx_Params *dptx, uint8_t *pucNumOfLanes);
int32_t Dptx_Core_Get_PHY_Rate(struct Dptx_Params *dptx, uint8_t *pucPHY_Rate);
int32_t Dptx_Core_Get_Stream_Mode(struct Dptx_Params *pstDptx, bool *pbMST_Mode);
int32_t Dptx_Core_Set_PHY_PowerState(struct Dptx_Params *pstDptx, enum PHY_POWER_STATE ePowerState);
int32_t Dptx_Core_Set_PHY_NumOfLanes(struct Dptx_Params *pstDptx, uint8_t ucNumOfLanes);
int32_t Dptx_Core_Get_Sink_SSC_Capability(struct Dptx_Params *pstDptx, bool *pbSSC_Profiled);
int32_t Dptx_Core_Set_PHY_SSC(struct Dptx_Params *dptx, bool bSink_Supports_SSC);
int32_t Dptx_Core_Get_PHY_SSC(struct Dptx_Params *pstDptx, bool *pbSSC_Enabled);
int32_t Dptx_Core_Set_PHY_Rate(struct Dptx_Params *pstDptx, enum PHY_LINK_RATE eRate);
int32_t Dptx_Core_Set_PHY_PreEmphasis(struct Dptx_Params *pstDptx, uint32_t uiLane_Index, enum PHY_PRE_EMPHASIS_LEVEL ePreEmphasisLevel);
int32_t Dptx_Core_Set_PHY_VSW(struct Dptx_Params *pstDptx, uint32_t uiLane_Index, enum PHY_VOLTAGE_SWING_LEVEL eVoltageSwingLevel);
int32_t Dptx_Core_Set_PHY_Pattern(struct Dptx_Params *pstDptx, uint32_t uiPattern);
int32_t Dptx_Core_Enable_PHY_XMIT(struct Dptx_Params *pstDptx, uint32_t iNumOfLanes);
int32_t Dptx_Core_Disable_PHY_XMIT(struct Dptx_Params *pstDptx, uint32_t iNumOfLanes);
void Dptx_Core_Soft_Reset(struct Dptx_Params *pstDptx, uint32_t uiReset_Bits);
void Dptx_Core_Init_PHY(struct Dptx_Params *pstDptx, uint8_t ucData_Width);
void Dptx_Core_Enable_Global_Intr(struct Dptx_Params *pstDptx, uint32_t uiEnable_Bits);
void Dptx_Core_Disable_Global_Intr(struct Dptx_Params *pstDptx);

int32_t Dptx_Protect_Set_PW(struct Dptx_Params *pstDptx);
int32_t Dptx_Protect_Set_CfgAccess(struct Dptx_Params *pstDptx, bool bAccessible);
int32_t Dptx_Protect_Set_CfgLock(struct Dptx_Params *pstDptx, bool bLocked);

void Dptx_Clk_Reset_PLL(struct Dptx_Params *pstDptx);
void Dptx_Clk_Set_PLL_Divisor(struct Dptx_Params *pstDptx);
void Dptx_Clk_Set_PLL_ClkSrc(struct Dptx_Params *pstDptx, uint8_t ucClockSource);
void Dptx_Clk_Get_PLLLock_Status(struct Dptx_Params *pstDptx, uint8_t *pucPll_Locked);


int32_t Dptx_Sec_PHY_Init(struct Dptx_Params *pstDptx, uint8_t ucLinkRate, uint8_t ucLanes);
int32_t Dptx_Sec_Set_NumOfLanes(struct Dptx_Params *pstDptx, uint8_t ucLanes);
int32_t Dptx_Sec_Set_EQ(struct Dptx_Params *pstDptx, uint8_t lane_idx,
			uint8_t link_rate, uint8_t ucVsw, uint8_t ucPreemp);

/* Dptx Video Input */
int32_t Dptx_VidIn_Init_Params(struct Dptx_Params *pstDptx, uint32_t uiPeri_Pixel_Clock[PHY_INPUT_STREAM_MAX]);
int32_t Dptx_VidIn_Set_Stream_Enable(struct Dptx_Params *pstDptx, bool bEnable_Stream, uint8_t ucStream_Index);
int32_t Dptx_VidIn_Get_Stream_Enable(struct Dptx_Params *pstDptx, bool *pbEnable_Stream, uint8_t ucStream_Index);
int32_t Dptx_VidIn_Set_Detailed_Timing(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, struct Dptx_Dtd_Params *pstDtd_Params);

int32_t Dptx_VidIn_Set_Reset(struct Dptx_Params *pstDptx, bool ucReset, uint8_t ucStream_Index);
int32_t Dptx_VidIn_Set_Test_Pattern_Timing(struct Dptx_Params *pstDptx, uint8_t ucStream_Index);
int32_t Dptx_VidIn_Set_BPC(struct Dptx_Params *pstDptx, uint8_t ucStream_Index);
int32_t Dptx_VidIn_Set_Ts_Change(struct Dptx_Params *pstDptx, uint8_t ucStream_Index);

int32_t Dptx_VidIn_Set_Timing(struct Dptx_Params *pstDptx, uint8_t ucStream_Index);
int32_t Dptx_VidIn_Get_Pixel_Mode(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t *pucPixelMode);
int32_t Dptx_VidIn_Get_NumOfStreams(struct Dptx_Params *pstDptx, uint8_t *pucEnabled_Streams);
int32_t Dptx_VidIn_Get_PixelEncoding_Type(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint8_t *pucPixel_Encoding);
int32_t Dptx_VidIn_Get_Configured_Timing(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, struct Dptx_Dtd_Params *pstDtd);
int32_t Dptx_VidIn_Calculate_Average_TU_Symbols(struct Dptx_Params *pstDptx, int iNumOfLane, int iLinkRate, int iBpc, int iEncodingType, int iPixel_Clock, uint8_t ucStream_Index);
int32_t Dptx_VidIn_Get_VIC_From_Dtd(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, uint32_t *puiVideo_Code);
int32_t Dptx_VidIn_Fill_Dtd(struct Dptx_Dtd_Params *pstDtd, uint32_t uiVideo_Code, uint32_t uiRefreshRate, uint8_t ucVideoFormat);


/* Dptx Video Generator */
int32_t Dptx_VidGen_Gen_Pattern(struct Dptx_Params *pstDptx, uint8_t ucStream_Index);
int32_t Dptx_VidGen_Get_Timing(struct Dptx_Params *pstDptx, uint8_t ucStream_Index, int32_t *piHactive, int32_t *piVactive);


#if defined(DP_LINK_VG_REGISTER_SET)
int32_t Dptx_VidGen_Set_Ycc_Mapping(struct Dptx_Params *pstDptx, uint8_t ucStream_Index);
int32_t Dptx_VidGen_Set_Gen_Pattern(struct Dptx_Params *pstDptx, uint8_t ucStream_Index);
int32_t Dptx_VidGen_Set_Pattern_Change(struct Dptx_Params *pstDptx, uint8_t ucStream_Index);
#endif

/* Dptx Audio + Hdmi codec */
int32_t Dptx_AudIn_Register(struct Dptx_Params *pstDptx);


/* Dptx Link */
int32_t dptx_reset_link_rate_and_lanes(struct Dptx_Params *pstDptx,
				       uint8_t link_rate, uint8_t link_lanes);
int32_t Dptx_Link_Perform_Training(struct Dptx_Params *pstDptx, uint8_t ucRate, uint8_t ucNumOfLanes);
int32_t Dptx_Link_Perform_BringUp(struct Dptx_Params *pstDptx);
bool dptx_link_get_linktraining_status(struct Dptx_Params *pstDptx);
int32_t Dptx_Link_PHY_Rate_To_Bandwidth(struct Dptx_Params *pstDptx, uint8_t ucRate, uint8_t *pucBandWidth);
int32_t Dptx_Link_Bandwidth_To_PHY_Rate(struct Dptx_Params *pstDptx, uint8_t ucBandWidth, uint8_t *pucRate);


/* Dptx Interrupt */
int32_t Dptx_Intr_Init_Params(struct Dptx_Params *pstDptx);
irqreturn_t Dptx_Intr_IRQ(int irq, void *dev);
irqreturn_t Dptx_Intr_Threaded_IRQ(int irq, void *dev);
int32_t Dptx_Intr_Get_Port_Composition(struct Dptx_Params *pstDptx, bool bClear_PayloadID);
int32_t Dptx_Intr_Register_HPD_Callback(struct Dptx_Params *pstDptx, Dptx_HPD_Intr_Callback HPD_Intr_Callback);
int32_t Dptx_Intr_Register_Panel_Callback(struct Dptx_Params *pstDptx, Dptx_Panel_Topology_Callback Panel_Topology_CallBack);
int32_t Dptx_Intr_Handle_HotUnplug(struct Dptx_Params *pstDptx);
bool dptx_intr_get_hotplug_status(struct Dptx_Params *pstDptx);


/* Dptx EDID */
int32_t Dptx_Edid_Verify_EDID(uint8_t *pucEDID_Buf);
int32_t Dptx_Edid_Read_EDID_I2C_Over_Aux(struct Dptx_Params *pstDptx);
int32_t Dptx_Edid_Read_EDID_Over_Sideband_Msg(struct Dptx_Params *pstDptx, uint8_t ucStream_Index);


/* Dptx Extension */
int32_t Dptx_Ext_Clear_VCP_Tables(struct Dptx_Params *pstDptx);
int32_t Dptx_Ext_Initiate_MST_Act(struct Dptx_Params *pstDptx);
int32_t Dptx_Ext_Set_Stream_Mode(struct Dptx_Params *pstDptx, uint8_t num_ports);
int32_t Dptx_Ext_Get_Stream_Mode(struct Dptx_Params *pstDptx, bool *pbMST_Supported, uint8_t *pucNumOfPorts);
int32_t Dptx_Ext_Set_Stream_Capability(struct Dptx_Params *pstDptx);
int32_t Dptx_Ext_Get_Link_PayloadBandwidthNumber(struct Dptx_Params *pstDptx, uint8_t ucStream_Index);
int32_t Dptx_Ext_Set_Link_VCP_Tables(struct Dptx_Params *pstDptx, uint8_t ucStream_Index);
int32_t Dptx_Ext_Set_Sink_VCP_Table_Slots(struct Dptx_Params *pstDptx, uint8_t ucStream_Index);
int32_t Dptx_Ext_Get_TopologyState(struct Dptx_Params *pstDptx, bool bClear_PayloadId, uint8_t *pucNumOfHotpluggedPorts);
int32_t Dptx_Ext_Set_Topology_Configuration(struct Dptx_Params *pstDptx);
int32_t Dptx_Ext_Remote_I2C_Read(struct Dptx_Params *pstDptx, uint8_t ucStream_Index);
int32_t Dptx_Ext_Clear_SidebandMsg_PayloadID_Table(struct Dptx_Params *pstDptx);
int32_t Dptx_Ext_Proc_Interface_Init(struct Dptx_Params *pstDptx);



/* Dptx Register */
uint32_t  Dptx_Reg_Readl(struct Dptx_Params *pstDptx, uint32_t uiOffset);
void Dptx_Reg_Writel(struct Dptx_Params *pstDptx, uint32_t uiOffset, uint32_t uiData);
uint32_t Dptx_MCU_DP_Reg_Read(struct Dptx_Params *pstDptx, uint32_t uiOffset);
void Dptx_MCU_DP_Reg_Write(struct Dptx_Params *pstDptx, uint32_t uiOffset, uint32_t uiData);
uint32_t Dptx_PMU_Reg_Read(struct Dptx_Params *pstDptx, uint32_t uiOffset);
void Dptx_PMU_Reg_Write(struct Dptx_Params *pstDptx, uint32_t uiOffset, uint32_t uiData);
uint32_t Dptx_Reg_Direct_Read(void __iomem *Reg);
void Dptx_Reg_Direct_Write(void __iomem *Reg, uint32_t uiData);

uint32_t dptx_phy_read(struct Dptx_Params *pstDptx, uint32_t address);
int32_t dptx_phy_write(struct Dptx_Params *pstDptx, uint32_t address,
		       uint32_t phy_data);

/* Dptx Aux */
int32_t Dptx_Aux_Read_DPCD(struct Dptx_Params *pstDptx, uint32_t uiAddr, uint8_t *pucBuffer);
int32_t Dptx_Aux_Read_Bytes_From_DPCD(struct Dptx_Params *pstDptx, uint32_t uiAddr, uint8_t *pucBuffer, uint32_t len);
int32_t Dptx_Aux_Write_DPCD(struct Dptx_Params *pstDptx, uint32_t uiAddr, uint8_t ucBuffer);
int32_t Dptx_Aux_Write_Bytes_To_DPCD(struct Dptx_Params *pstDptx, uint32_t uiAddr, uint8_t *pucBuffer, uint32_t uiLength);
int32_t  Dptx_Aux_Read_Bytes_From_I2C(struct Dptx_Params *pstDptx, uint32_t uiDevice_Addr, uint8_t *pucBuffer, uint32_t uiLength);
int32_t  Dptx_Aux_Write_Bytes_To_I2C(struct Dptx_Params *pstDptx, uint32_t uiDevice_Addr, uint8_t *pucBuffer, uint32_t uiLength);
int32_t  Dptx_Aux_Write_AddressOnly_To_I2C(struct Dptx_Params *pstDptx, uint32_t uiDevice_Addr);


int32_t Dpv14_Tx_Suspend_T(struct Dptx_Params *pstDptx);
int32_t Dpv14_Tx_Resume_T(struct Dptx_Params *pstDptx);


/* Dptx SerDes */
int32_t Dptx_Max968XX_Reset(const struct Dptx_Params *pstDptx);
int32_t Dptx_Max968XX_Get_TopologyState(u8 *pucNumOfPluggedPorts);


/* Dptx API */
int32_t Dptx_Api_Init_Params(void);
int32_t Dptx_Api_Register_Aud(void);
void Hpd_Intr_CallBabck(u8 ucDP_Index, bool bHPD_State);
int Str_Resume_CallBabck(void);
int Panel_Topology_CallBabck(uint8_t *pucNumOfPorts);
#endif /* __DPTX_API_H__  */
