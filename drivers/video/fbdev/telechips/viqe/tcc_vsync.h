/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_VSYNC_H__
#define TCC_VSYNC_H__

/*==============================================================================
 *
 * structures
 *
 *==============================================================================
 */
#include <video/telechips/vioc_intr.h>
#include <video/telechips/tcc_video_private.h>
#include <uapi/misc/tccmisc_drv.h>

#ifdef CONFIG_USE_SUB_MULTI_FRAME
enum VSYNC_CH_TYPE {
	VSYNC_MAIN = 0,
	VSYNC_SUB0,
	VSYNC_SUB1,
	VSYNC_SUB2,
	VSYNC_SUB3,
	VSYNC_MAX
};
#else
enum VSYNC_CH_TYPE {
	VSYNC_MAIN = 0,
	VSYNC_SUB0,
	VSYNC_MAX
};
#endif

enum HDMI_DRM_MODE {
	DRM_INIT = 0,
	DRM_ON,
	DRM_OFF
};

#if defined(CONFIG_VSYNC_DRV_ALWAYS_ACCEPT_START_VSYNC)
enum VSYNC_RUN_STATUS{
	EM_VSYNC_DISABLED = 0,
	EM_VSYNC_PREPARE,
	EM_VSYNC_RUNNING
};
#else
enum VSYNC_RUN_STATUS {
	EM_VSYNC_DISABLED = 0,
	EM_VSYNC_RUNNING
};
#endif

struct tcc_vsync_buffer_t {
	int readIdx;
	int writeIdx;
	int clearIdx;

	atomic_t valid_buff_count;
	atomic_t readable_buff_count;

	int max_buff_num;
	int last_cleared_buff_id;
	int available_buffer_id_on_vpu;
	struct tcc_lcdc_image_update stImage[VPU_BUFFER_MANAGE_COUNT];
	struct tcc_lcdc_image_update curr_displaying_imgInfo;
};

#define TIME_BUFFER_COUNT  30

struct tcc_lastframe_reason {
	int Resolution; // resolution changed..
	int Codec;      // codec changed..
};

struct tcc_video_lastframe {
	int support;

	struct tcc_lcdc_image_update CurrImage;
	struct tcc_lcdc_image_update LastImage;
	int enabled;
	struct tccmisc_user_t pmapBuff;

	void __iomem *pRDMA;

	struct tcc_lastframe_reason reason;
	unsigned int nCount;
};

struct tcc_video_disp {
	struct tcc_vsync_buffer_t vsync_buffer;
	int type;

	int isVsyncRunning;
	// for time sync
	unsigned int unVsyncCnt;
	int baseTime;
	unsigned int timeGapIdx;
	unsigned int timeGapBufferFullFlag;
	int timeGap[TIME_BUFFER_COUNT];
	int timeGapTotal;
	int updateGapTime;
	int vsync_interval;
	int perfect_vsync_flag;

	int skipFrameStatus;
	int overlayUsedFlag;
	int outputMode;
	int video_frame_rate;

	//for deinterlace mode
	int deinterlace_mode;
	int firstFrameFlag;
	int frameInfo_interlace;
	int m2m_mode;
	int output_toMemory;
	int nDeinterProcCount;
	int nTimeGapToNextField;
	int interlace_output;
	int interlace_bypass_lcdc;
	int mvcMode;
	int duplicateUseFlag;
	int vsync_started;
	int lastUdateTime;
	int time_gap_sign;

	int player_no_sync_time;
	int prev_time_stamp;
	int prev_buffer_unique_id;
	int player_seek;
	int wait_too_long_indicate;
	int prevImageWidth;
	int prevImageHeight;

	struct tcc_lcdc_image_update *pIntlNextImage;

	struct tcc_lcdc_image_update push_ext_infoframe;
	int push_ext_status_paused;
	int push_ext_count;

};

struct tcc_vsync_display_info_t {
	struct vioc_intr_type *vioc_intr;

	int lcdc_num;
	int irq_num;
	int irq_reged;
	void __iomem *virt_addr;
};

/* CONFIG_LCD_VIDEO_DISPLAY_BY_VSYNC_INT */
enum {
	LCD_START_VSYNC,
	HDMI_START_VSYNC
};

struct tcc_vioc_block {
	void __iomem *virt_addr; // virtual address
	unsigned int irq_num;
	unsigned int blk_num; //block number like dma number or mixer number
};

struct tcc_dp_device {
	enum TCC_OUTPUT_TYPE DispDeviceType;
	//unsigned int DispOrder;    //DD_MAIN , DD_SUB
	unsigned int DispNum;      //0 or 1
	unsigned int FbPowerState; //true or false
	//unsigned int FbUpdateType; //like FB_RDMA_UDPATE or FB_SC_RDMA_UPDATE

	/*  if FB_SC_RDMA_UPDATE type plug in scaler number
	 * sc_num0 : scaler number for normal UI,
	 * sc_num1 : scaler number for 3D UI
	 */
	//unsigned int sc_num0, sc_num1;
	//unsigned int FbBaseAddr; //base address of frame buffer

	struct clk *vioc_clock;              //vioc blcok clock
	struct clk *ddc_clock;               //display blcok clock
	struct tcc_vioc_block ddc_info;      // display controller address
	struct tcc_vioc_block wmixer_info;   // wmixer address
	struct tcc_vioc_block wdma_info;     // wdma address
	struct tcc_vioc_block rdma_info[RDMA_MAX_NUM]; // rdma address
};

struct tccfb_platform_data {
	// main display device infomation
	unsigned int lcdc_number;
	unsigned int FbPowerState;

	// main display device infomation
	struct tcc_dp_device Mdp_data;

	// sub display device infomation
	struct tcc_dp_device Sdp_data;
};

#if defined(CONFIG_VIDEO_DISPLAY_BY_VSYNC_INT) || defined(CONFIG_VIDEO_DISPLAY_BY_VSYNC_INT_MODULE)
/*==============================================================================
 *
 * functions
 *
 *==============================================================================
 */
void tca_vsync_video_display_enable(void);
void tca_vsync_video_display_disable(void);
void tcc_vsync_set_firstFrameFlag_all(int firstFrameFlag);
int tcc_video_get_displayed(struct tcc_video_disp *p, enum VSYNC_CH_TYPE type);
void tcc_video_clear_frame(struct tcc_video_disp *p, int idx, int who_call);
void tcc_video_set_framerate(struct tcc_video_disp *p, int fps);
int tcc_video_check_framerate(struct tcc_video_disp *p, int fps);
void tcc_video_skip_frame_start(struct tcc_video_disp *p, enum VSYNC_CH_TYPE type);
void tcc_video_skip_frame_end(struct tcc_video_disp *p, enum VSYNC_CH_TYPE type);
void tcc_video_skip_one_frame(struct tcc_video_disp *p, int frame_id);
int tcc_video_get_readable_count(struct tcc_video_disp *p);
int tcc_video_get_valid_count(struct tcc_video_disp *p);
void tcc_vsync_set_output_mode(struct tcc_video_disp *p, int mode);
void tcc_vsync_reset_all(void);
void tcc_vsync_if_pop_all_buffer(struct tcc_video_disp *p);
void tcc_vsync_set_output_mode_all(int mode);
int tcc_vsync_isVsyncRunning(enum VSYNC_CH_TYPE type);
int tcc_vsync_get_isVsyncRunning(enum VSYNC_CH_TYPE type);
int tcc_vsync_get_output_toMemory(struct tcc_video_disp *p);
int tcc_vsync_get_interlace_bypass_lcdc(struct tcc_video_disp *p);
int tcc_vsync_get_deinterlace_mode(struct tcc_video_disp *p);
int is_deinterlace_enabled(enum VSYNC_CH_TYPE type);
void tcc_vsync_hdmi_start(struct tcc_dp_device *pdp_data, int *lcd_video_started);
void tcc_vsync_hdmi_end(struct tcc_dp_device *pdp_data);

void tcc_video_clear_last_frame(unsigned int lcdc_layer, bool reset);
enum VSYNC_CH_TYPE tcc_vsync_get_video_ch_type(unsigned int lcdc_layer);
int tcc_video_check_last_frame(struct tcc_lcdc_image_update *ImageInfo);

int tcc_ctrl_ext_frame(char enable);

struct tcc_dp_device *tca_fb_get_displayType(enum TCC_OUTPUT_TYPE check_type);
void tca_scale_display_update(struct tcc_dp_device *pdp_data, struct tcc_lcdc_image_update *ImageInfo);

#endif

#endif /*TCC_VSYNC_H__*/
