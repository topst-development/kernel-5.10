/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_VOUT_H
#define TCC_VOUT_H

#include <linux/videodev2.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>

#include <linux/types.h>
#include <video/telechips/vioc_global.h>
#include <video/telechips/tccfb_ioctrl.h>
#include <video/telechips/vioc_rdma.h>
#include <video/telechips/vioc_wdma.h>
#include <video/telechips/vioc_scaler.h>
#include <video/telechips/vioc_wmix.h>
#include <video/telechips/vioc_viqe.h>
#include <video/telechips/vioc_config.h>
#include <video/telechips/vioc_disp.h>
#include <video/telechips/vioc_intr.h>
#include <video/telechips/vioc_deintls.h>
#include <video/telechips/tcc_wmixer_ioctrl.h>
#include <video/telechips/tcc_types.h>

#ifdef CONFIG_VIOC_MAP_DECOMP
#include <video/telechips/tcc_video_private.h>
#endif

#include <video/telechips/tcc_vout_v4l2.h>
#ifdef CONFIG_TELECHIPS_G2D
#include <linux/sched.h>
#endif

#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))

#define VOUT_NAME		"tcc-vout-video"
#define VOUT_VERSION	KERNEL_VERSION(1, 0, 0)
#define VOUT_CLK_SRC	"lcdc1"
#define VOUT_MEM_PATH_PMAP_NAME	"v4l2_vout0"
#define VOUT_DUAL_PATH_PMAP_NAME	"dual_display"
#define VOUT_DISP_PATH_PMAP_NAME	"video"		/* using vpu pmap */

/* to check error state */
#define MAX_ERROR_CNT		10

/* disp_path uses 2nd scaler */
//#define VOUT_DISP_PATH_SCALER

/* Rounds an integer value up to the next multiple of num */
#define ROUND_UP_2(num)		(((num)+1U)&~1U)
#define ROUND_UP_4(num)		(((num)+3U)&~3U)
#define ROUND_UP_8(num)		(((num)+7U)&~7U)
#define ROUND_UP_16(num)	(((num)+15U)&~15U)
#define ROUND_UP_32(num)	(((num)+31U)&~31U)
#define ROUND_UP_64(num)	(((num)+63U)&~63U)
/* Rounds an integer value down to the next multiple of num */
#define ROUND_DOWN_2(num)	((num)&(~1U))
#define ROUND_DOWN_4(num)	((num)&(~3U))
#define ROUND_DOWN_8(num)	((num)&(~7U))
#define ROUND_DOWN_16(num)	((num)&(~15U))
#define ROUND_DOWN_32(num)	((num)&(~31U))
#define ROUND_DOWN_64(num)	((num)&(~63U))

/* Still-image format */
#define STILL_IMGAGE			0xFF00U

/* rdma alpha */
#define RDMA_ALPHA_ASEL_GLOBAL	0
#define RDMA_ALPHA_ASEL_PIXEL	1

#ifdef CONFIG_TELECHIPS_VOUT_BOOTING_ANIMATION
	#define M2M_WMIX_WIDTH		3840
	#define M2M_WMIX_HEIGHT		720
#else
	#define M2M_WMIX_WIDTH		1920
	#define M2M_WMIX_HEIGHT		720
#endif

#ifdef CONFIG_DUAL_DISPLAY_HD
	#define DEINTL_WIDTH            (1280)
	#define DEINTL_HEIGHT           (720)
#elif defined(CONFIG_DUAL_DISPLAY_FHD)
	#define DEINTL_WIDTH            (1920)
	#define DEINTL_HEIGHT           (1088)
#elif defined(CONFIG_DUAL_DISPLAY_UHD)
	#define DEINTL_WIDTH            (3840)
	#define DEINTL_HEIGHT           (2160)
#endif

enum M2M_DUAL_DISP {
	M2M_DUAL_0,
	M2M_DUAL_1,
	M2M_DUAL_MAX,
};

/* mplane */
#define MPLANE_NUM	2UL
#define MPLANE_VID	0UL
#define MPLANE_SUB	1UL

#define MIN_DEINTLBUF_NUM      (3)

#define VOUT_MAIN 	0UL
#define VOUT_SUB 	1UL
#define VOUT_MAX_CH	2UL

#define VIDEO_INFO_NUM 80
#define SUBTITLE_INFO_NUM 8

enum v4l2_plane_reserved_list {
	VIDEODEV2_RESERVED_64,
	VIDEODEV2_RESERVED_11,
};

#ifdef CONFIG_VIOC_MAP_DECOMP
enum vioc_mapconv_list {
	VIOC_MC_00,
	VIOC_MC_01,
};

/* map_conv compressed information */
enum mplane_mapconv_component {
	VID_HEVC_FRAME_BASE_Y0 = 11,
	VID_HEVC_FRAME_BASE_Y1 = 12,
	VID_HEVC_FRAME_BASE_C0 = 13,
	VID_HEVC_FRAME_BASE_C1 = 14,
	VID_HEVC_OFFSET_BASE_Y0 = 15,
	VID_HEVC_OFFSET_BASE_Y1 = 16,
	VID_HEVC_OFFSET_BASE_C0 = 17,
	VID_HEVC_OFFSET_BASE_C1 = 18,
	VID_HEVC_LUMA_STRIDE = 19,
	VID_HEVC_CHROMA_STRIDE = 20,
	VID_HEVC_LUMA_BIT_DEPTH	= 21,
	VID_HEVC_CHROMA_BIT_DEPTH = 22,
	VID_HEVC_FRMAE_ENDIAN = 23,
	VID_HEVC_RESERVED0 = 24,
};
#endif

enum dec_vid_format {
	/* video pixelformat */
	DEC_FMT_420IL0 = 0,	// yuv420 interleaved type 0 foramt (0x1C)
	DEC_FMT_422SEP = 1,	// yuv422 seperate format (0x19)
	DEC_FMT_422V = 2,	// unknown format
	DEC_FMT_444SEP = 3,	// yuv444 seperate format (0x15)
	DEC_FMT_400 = 4,	// unknown formatt
	DEC_FMT_420SEP = 5,	// yuv420 seperate format (0x18)
};

enum mplane_vid_component {
	/* video information */
	VID_SRC = 0,		// MPLANE_VID 0x0
	VID_NUM = 1,		// num of mplanes
	VID_BASE1 = 2,		// base1 address of video src (U/Cb)
	VID_BASE2 = 3,		// base1 address of video src (V/Cr)
	VID_WIDTH = 4,		// width/height of video src
	VID_HEIGHT = 5,
	VID_CROP_LEFT = 6,	// crop-[left/top/width/height] of video src
	VID_CROP_TOP = 7,
	VID_CROP_WIDTH = 8,
	VID_CROP_HEIGHT = 9,
	VID_CONVERTER_EN = 10,

	#ifdef CONFIG_TELECHIPS_HDMI_DRIVER_V2_0
	VID_HDR_VERSION = 26,
	VID_HDR_STRUCT_SIZE = 27,
	VID_HDR_BIT_DEPTH = 28,
	VID_HDR_COLOR_PRIMARY = 29,
	VID_HDR_TC = 30,
	VID_HDR_MC = 31,
	VID_HDR_PRIMARY_X0 = 32,
	VID_HDR_PRIMARY_X1 = 33,
	VID_HDR_PRIMARY_X2 = 34,
	VID_HDR_PRIMARY_Y0 = 35,
	VID_HDR_PRIMARY_Y1 = 36,
	VID_HDR_PRIMARY_Y2 = 37,
	VID_HDR_WPOINT_X = 38,
	VID_HDR_WPOINT_Y = 39,
	VID_HDR_MAX_LUMINANCE = 40,
	VID_HDR_MIN_LUMINANCE = 41,
	VID_HDR_MAX_CONTENT = 42,
	VID_HDR_MAX_PIC_AVR	= 43,
	VID_HDR_EOTF		= 44,
	VID_HDR_DESCRIPTOR_ID = 45,
	#endif

	VID_MJPEG_FORMAT	= 46,
};

enum mplane_sub_component {
	/* subtitle information */
	SUB_SRC = 0,		// MPLANE_SUB 0x1
	SUB_ON = 1,		// subtitle on(1) or off(0)
	SUB_BUF_INDEX = 2,	// index of subtitle buf
	SUB_FOURCC = 3,		// forcc format
	SUB_WIDTH = 4,		// width
	SUB_HEIGHT = 5,		// height
	SUB_OFFSET_X = 6,	// x position
	SUB_OFFSET_Y = 7,	// y position
};

#define TCC_PMAP_NAME_LEN	(16U)
struct pmap {
	char name[TCC_PMAP_NAME_LEN];
	u64 base;
	u64 size;
	u32 groups;
	u32 rc;
	u32 flags;
};

struct tcc_v4l2_img {
	unsigned int base0;
	unsigned int base1;
	unsigned int base2;
};

struct vioc_disp {
	unsigned int id;
	void __iomem *addr;
	unsigned int irq;
	unsigned int irq_enable;	// avoid overlapping irq
	struct vioc_intr_type *vioc_intr;
};

struct vioc_rdma {
	unsigned int id;
	void __iomem *addr;
	struct tcc_v4l2_img img;
	unsigned int width;
	unsigned int height;
	unsigned int fmt;
	unsigned int bf;		// BFIELD indication. 0: top, 1: bottom
	unsigned int y2r;
	unsigned int y2rmd;
	unsigned int intl;
	unsigned int rgbswap;		// RGB Swap Register
	unsigned int y_stride;	// Y-stride
};

struct vioc_wdma {
	int unsigned id;
	void __iomem *addr;
	struct tcc_v4l2_img img;
	unsigned int width;
	unsigned int height;
	unsigned int fmt;
	unsigned int r2y;
	unsigned int r2ymd;
	unsigned int cont;
	unsigned int irq;
	unsigned int irq_enable;	// avoid overlapping irq
	struct vioc_intr_type	*vioc_intr;
};

struct vioc_wmix {
	unsigned int id;
	void __iomem *addr;
	unsigned int width;
	unsigned int height;
	unsigned int pos;		// wmix image position
	unsigned int ovp;		// wmix overlay priority
	unsigned int left;
	unsigned int top;
};

struct vioc_alpha {
	unsigned int src_type;		// v4l2_buffer.m.planes[1].reserved[0]
	unsigned int on;		// v4l2_buffer.m.planes[1].reserved[1]
	unsigned int buf_index;		// v4l2_buffer.m.planes[1].reserved[2]
	unsigned int fourcc;		// v4l2_buffer.m.planes[1].reserved[3]
	unsigned int width;		// v4l2_buffer.m.planes[1].reserved[4]
	unsigned int height;		// v4l2_buffer.m.planes[1].reserved[5]
	unsigned int offset_x;		// v4l2_buffer.m.planes[1].reserved[6]
	unsigned int offset_y;		// v4l2_buffer.m.planes[1].reserved[7]

	unsigned int reserved8;
	unsigned int reserved9;
	unsigned int reserved10;
};

struct vioc_sc {
	unsigned int id;
	void __iomem *addr;
};

struct vioc_viqe {
	unsigned int id;
	void __iomem *addr;
	unsigned int y2r;
	unsigned int y2rmd;
};

struct vioc_deintls {
	unsigned int id;
};

#ifdef CONFIG_VIOC_MAP_DECOMP
struct vioc_mc {
	unsigned int id;
	hevc_MapConv_info_t mapConv_info;
};
#endif

enum tcc_pix_fmt {
	TCC_PFMT_YUV420,
	TCC_PFMT_YUV422,
	TCC_PFMT_RGB,
};

enum deintl_type {
	VOUT_DEINTL_NONE,
	VOUT_DEINTL_VIQE_BYPASS,
	VOUT_DEINTL_VIQE_2D,
	VOUT_DEINTL_VIQE_3D,
	VOUT_DEINTL_S,
};

struct tc_v4l2_buffer{
	unsigned int vid_reserved[VIDEO_INFO_NUM];
	unsigned int sub_reserved[SUBTITLE_INFO_NUM];
};

struct tcc_v4l2_buffer {
	int index;

	struct v4l2_buffer buf;
	unsigned int img_base0;		// RDMABASE0
	unsigned int img_base1;		// RDMABASE1
	unsigned int img_base2;		// RDMABASE2
	struct tc_v4l2_buffer tc_vbuf;
};

struct tcc_vout_vioc {
	struct clk *vout_clk;

	/* display path */
	struct vioc_disp disp;
	struct vioc_rdma rdma;
	struct vioc_wmix wmix;
	struct vioc_sc sc;

	/* deinterlace path */
	struct vioc_rdma m2m_rdma;
	struct vioc_wmix m2m_wmix;
	struct vioc_wdma m2m_wdma;
	struct vioc_viqe viqe;
	struct vioc_deintls deintl_s;

#ifdef CONFIG_TELECHIPS_VOUT_BOOTING_ANIMATION
	struct vioc_disp disp_bootani;
	struct vioc_rdma rdma_bootani;
	struct vioc_wmix wmix_bootani;
#endif

#ifdef CONFIG_TELECHIPS_DUAL_DISPLAY
	/* display dual path */
	struct vioc_disp disp_dual;
	struct vioc_rdma rdma_dual;
	struct vioc_wmix wmix_dual;
	struct vioc_sc sc_dual;

	/* m2m dual path */
	struct vioc_rdma m2m_dual_rdma[M2M_DUAL_MAX];
	struct vioc_wmix m2m_dual_wmix[M2M_DUAL_MAX];
	struct vioc_wdma m2m_dual_wdma[M2M_DUAL_MAX];
	struct vioc_sc m2m_dual_sc[M2M_DUAL_MAX];
#endif

	/* subtitle path */
	struct vioc_rdma m2m_subplane_rdma;
	struct vioc_wmix m2m_subplane_wmix;
	struct vioc_rdma subplane_rdma;
	struct vioc_wmix subplane_wmix;
	struct vioc_alpha subplane_alpha;
	int subplane_init;
	int subplane_buf_index;
	int subplane_enable;
#ifdef CONFIG_VIOC_MAP_DECOMP
	struct vioc_mc mc;
#endif
};

struct tcc_vout_device {
	unsigned int id;
	int opened;
	struct device_node *v4l2_np;
	struct v4l2_device v4l2_dev;
	struct video_device *vdev;
	void __iomem *base;
	struct mutex lock;	// lock to protect the shared data in ioctl

	struct tcc_vout_vioc *vioc;
	enum tcc_vout_status status;

	int fmt_idx;
	int bpp;
	int pfmt;

	struct v4l2_pix_format src_pix;	// src image format
	struct v4l2_rect disp_rect;	// display output area

#ifdef CONFIG_TELECHIPS_DUAL_DISPLAY
	int disp_mode;
	struct v4l2_rect dual_disp_rect; // dual-display output area
#endif

	struct v4l2_rect crop_src; // to crop source video (deintl_rdma crop)
#ifdef CONFIG_TELECHIPS_G2D
	/* Rotation Structure */
	struct v4l2_rect rotation_rect;			// rotation area
	unsigned int rotation_flag;
#endif

	/* vout */
	unsigned int force_userptr;	// force V4L2_MEMORY_USERPTR
	struct pmap pmap;		// for only V4L2_MEMORY_MMAP

	/* reqbuf */
	struct tcc_v4l2_buffer *qbufs;
	enum v4l2_memory memory;
	int nr_qbufs;
	int mapped;
	int clearFrameMode;

	/* deinterlce */
	enum v4l2_field previous_field;	// previous field for deinterlace
	enum deintl_type deinterlace;	// deinterlacer type
	struct pmap deintl_pmap;
	unsigned int deintl_nr_bufs, deintl_nr_bufs_count;
	// full size of deintl_buf, it is depended on panel size.
	unsigned int deintl_buf_size;
	struct tcc_v4l2_buffer *deintl_bufs;
	wait_queue_head_t frame_wait;
	int wakeup_int;
#ifdef CONFIG_TELECHIPS_G2D
	struct work_struct wdma_work;
#endif

#ifdef CONFIG_TELECHIPS_VOUT_BOOTING_ANIMATION
	int bootani;
#endif

#ifdef CONFIG_TELECHIPS_DUAL_DISPLAY
	int ext_wakeup_int;
	int hdmi_wakeup_int;

	wait_queue_head_t ext_frame_wait;
	wait_queue_head_t hdmi_frame_wait;

	struct pmap m2m_dual_pmap;
	struct tcc_v4l2_buffer *m2m_dual_bufs;
	struct tcc_v4l2_buffer *m2m_dual_bufs_hdmi;
	// full size of deintl_buf, it is depended on panel size.
	unsigned int m2m_dual_nr_bufs;
	unsigned int m2m_dual_buf_size;
#endif

	/*
	 * Identify the size of the reserved array of the struct v4l2_plane structure
	 * reserved[64] : The value of videodev2flag is 0
	 * reserved[11] : The value of videodev2flag is 1
	 */
	int videodev2flag;

	int frame_count;
	int deintl_force; // 0: depend on stream info 1: depend on 'deinterlace'
	int first_frame;
	enum v4l2_field first_field;
	int firstFieldFlag;

	int baseTime;
	unsigned int update_gap_time;

	//onthefly
	int onthefly_mode;
	int display_done;
	int last_displayed_buf_idx;
	int force_sync;

	//buffer management
	int popIdx;
	int pushIdx;
	int clearIdx;
	atomic_t displayed_buff_count;
	atomic_t readable_buff_count;
	struct v4l2_buffer *last_cleared_buffer;
};

int tcc_vout_try_fmt(unsigned int pixelformat);
int tcc_vout_try_bpp(unsigned int pixelformat,
	enum v4l2_colorspace *colorspace);
int tcc_vout_try_pix(unsigned int pixelformat);

#endif //TCC_VOUT_H
