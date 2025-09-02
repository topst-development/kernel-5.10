// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *      tcc-dewarp-video.c  --  Telechips On-the-Fly ODW Path Driver
 *
 *      Copyright (C) 2005-2010
 *          Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 ******************************************************************************


 *   Modified by Telechips Inc.


 *   Modified date : 2020


 *   Description : Video handling


 *****************************************************************************/

#include <video/tcc-svdw.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <linux/clk.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_graph.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/version.h>

#include <media/v4l2-common.h>
#include <media/videobuf2-dma-contig.h>

#include <asm/unaligned.h>
#include "tcc-dewarp-video.h"
#ifdef CONFIG_ARCH_TCC807X
#include "807x/tcc-dewarp-odw.h"
#endif
#ifdef CONFIG_ARCH_TCC750X
#include "750x/tcc-dewarp-odw.h"
#endif

#if defined(CONFIG_ARCH_TCC807X) || defined(CONFIG_ARCH_TCC750X)
extern void parse_cfg_and_set_regs(struct tcc_dewarp_stream *vstream,
				   const struct firmware *fw);
#else
static inline void parse_cfg_and_set_regs(struct tcc_dewarp_stream *vstream,
					  const struct firmware *fw)
{
}
#endif

/* this spinlock is defined in the tccvin2 driver */
extern spinlock_t cam_mux_cfg_lock;

struct tcc_dewarp_mbus_format {
	u32 mbus_fmt;
	u32 data_format;
	u32 data_order;
};

struct tcc_dewarp_format {
	u32 pixelformat;
	u32 guid;
};

/* ------------------------------------------------------------------------
 * helper macro
 */

#define IS_SET(value, mask) (((u32)(value) & (u32)(mask)) != 0U)

/* ------------------------------------------------------------------------
 * DEFINITION
 */

static struct tcc_dewarp_mbus_format tcc_dewarp_mbus_format_list[] = {
	{ .mbus_fmt = MEDIA_BUS_FMT_UYVY8_2X8,
#ifdef CONFIG_ARCH_TCC750X
	  .data_format = FMT_UVY422_LSB_16BIT,
#else
	  .data_format = FMT_YUV422_LSB_16BIT,
#endif
	  .data_order = ORDER_RGB },
	{ .mbus_fmt = MEDIA_BUS_FMT_UYVY8_1X16,
#ifdef CONFIG_ARCH_TCC750X
	  .data_format = FMT_UVY422_LSB_16BIT,
#else
	  .data_format = FMT_YUV422_LSB_16BIT,
#endif
	  .data_order = ORDER_RGB },
	{ .mbus_fmt = MEDIA_BUS_FMT_Y8_1X8,
#ifdef CONFIG_ARCH_TCC750X
	  .data_format = FMT_UVY422_LSB_16BIT,
#else
	  .data_format = FMT_YUV422_LSB_16BIT,
#endif
	  .data_order = ORDER_RBG },
	{ .mbus_fmt = MEDIA_BUS_FMT_Y10_1X10,
#ifdef CONFIG_ARCH_TCC750X
	  .data_format = FMT_UVY422_LSB_16BIT,
#else
	  .data_format = FMT_YUV422_LSB_16BIT,
#endif
	  .data_order = ORDER_RBG },
	{ .mbus_fmt = MEDIA_BUS_FMT_Y12_1X12,
#ifdef CONFIG_ARCH_TCC750X
	  .data_format = FMT_UVY422_LSB_16BIT,
#else
	  .data_format = FMT_YUV422_LSB_16BIT,
#endif
	  .data_order = ORDER_RBG },
	{ .mbus_fmt = MEDIA_BUS_FMT_RGB888_1X24,
	  .data_format = FMT_RGB,
	  .data_order = ORDER_RBG },
	{ .mbus_fmt = MEDIA_BUS_FMT_YUV8_1X24,
	  .data_format = FMT_YUV444,
	  .data_order = ORDER_RBG },
};

/* supported color formats */
static struct tcc_dewarp_format tcc_dewarp_format_list[] = {
	{
		.pixelformat = V4L2_PIX_FMT_RGB32,
		.guid = DEWARP_COLOR_FMT_RGB,
	},
	/* sequential (YUV packed) */
	{
		.pixelformat = V4L2_PIX_FMT_UYVY,
		.guid = DEWARP_COLOR_FMT_UYVY,
	},
	{
		.pixelformat = V4L2_PIX_FMT_VYUY,
		.guid = DEWARP_COLOR_FMT_VYUY,
	},
	{
		.pixelformat = V4L2_PIX_FMT_YUYV,
		.guid = DEWARP_COLOR_FMT_YUYV,
	},
	{
		.pixelformat = V4L2_PIX_FMT_YVYU,
		.guid = DEWARP_COLOR_FMT_YVYU,
	},

	/* two non contiguous planes - one Y, one Cr + Cb interleaved  */
	{
		.pixelformat = V4L2_PIX_FMT_NV12M,
		.guid = DEWARP_COLOR_FMT_YUV420IL_ODD,
	},
	{
		.pixelformat = V4L2_PIX_FMT_NV21M,
		.guid = DEWARP_COLOR_FMT_YVU420IL_ODD,
	},
	{
		.pixelformat = V4L2_PIX_FMT_NV16M,
		.guid = DEWARP_COLOR_FMT_YUV422IL,
	},
	{
		.pixelformat = V4L2_PIX_FMT_NV61M,
		.guid = DEWARP_COLOR_FMT_YVU422IL,
	},
};

static void tcc_dewarp_get_vs_info_format_by_mbus_format(
	const struct tcc_dewarp_stream *vstream, u32 mbus_pixelcode,
	struct tcc_dewarp_vs_info *vs_fmt)
{
	const struct device *p_dev = NULL;
	const struct tcc_dewarp_mbus_format *format = NULL;
	u32 idxList = 0;
	u32 nList = 0;

	p_dev = stream_to_device(vstream);

	/* default format */
#ifdef CONFIG_ARCH_TCC750X
	vs_fmt->data_format = FMT_UVY422_LSB_16BIT;
#else
	vs_fmt->data_format = FMT_YUV422_LSB_16BIT;
#endif
	vs_fmt->data_order = ORDER_RGB;

	nList = ARRAY_SIZE(tcc_dewarp_mbus_format_list);
	for (idxList = 0; idxList < nList; idxList++) {
		format = &tcc_dewarp_mbus_format_list[idxList];
		if (mbus_pixelcode == format->mbus_fmt) {
			vs_fmt->data_format = format->data_format;
			vs_fmt->data_order = format->data_order;
		}
	}
}

static const struct tcc_dewarp_format *
tcc_odw_get_format_by_pixelformat(u32 pixelformat)
{
	u32 idxList = 0;
	u32 nList = 0;
	const struct tcc_dewarp_format *tformat = NULL;

	nList = ARRAY_SIZE(tcc_dewarp_format_list);
	for (idxList = 0; idxList < nList; idxList++) {
		tformat = &tcc_dewarp_format_list[idxList];
		if (pixelformat == tformat->pixelformat) {
			/* matched */
			break;
		}
	}

	if (idxList == nList) {
		/* When it failed to find a corresponding value from a list of
		supported format, return NULL  */
		tformat = NULL;
	}

	return tformat;
}

static bool tcc_dewarp_handover_flagged(u32 flags, u32 mask)
{
	return (IS_SET(flags, V4L2_CAP_CTRL_SKIP_ALL) || IS_SET(flags, mask));
}

static void
tcc_dewarp_print_handover_flags(const struct tcc_dewarp_stream *vstream,
				u32 flags)
{
	const struct device *p_dev = NULL;

	p_dev = stream_to_device(vstream);

	switch (flags) {
	case (u32)V4L2_CAP_CTRL_SKIP_NONE:
		logi(p_dev, "V4L2_CAP_CTRL_SKIP_NONE\n");
		break;
	case (u32)V4L2_CAP_CTRL_SKIP_ALL:
		logi(p_dev, "V4L2_CAP_CTRL_SKIP_ALL\n");
		break;
	case (u32)V4L2_CAP_CTRL_SKIP_SUBDEV:
		logi(p_dev, "V4L2_CAP_CTRL_SKIP_SUBDEV\n");
		break;
	case (u32)V4L2_CAP_CTRL_SKIP_DEV:
		logi(p_dev, "V4L2_CAP_CTRL_SKIP_DEV\n");
		break;
	default:
		loge(p_dev, "flags(0x%08x) is wrong\n", flags);
		break;
	}
}

static int tcc_dewarp_get_clock(struct tcc_dewarp_stream *vstream,
				struct device_node *dev_node)
{
	const struct device *p_dev = NULL;
	int ret = 0;

	p_dev = stream_to_device(vstream);

	/* get the axi clock */
	vstream->dewarp_wrap.cam_clk_axi = of_clk_get(dev_node, 0);
	if (vstream->dewarp_wrap.cam_clk_axi == NULL) {
		loge(p_dev, "of_clk_get_axi\n");
		ret = -ENODEV;
	}

	/* get the pix clock */
	if (ret == 0) {
		vstream->dewarp_wrap.cam_clk_pix = of_clk_get(dev_node, 1);
		if (vstream->dewarp_wrap.cam_clk_pix == NULL) {
			loge(p_dev, "of_clk_get_pix\n");
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		/* get the apb clock */
		vstream->dewarp_wrap.cam_clk_apb = of_clk_get(dev_node, 2);
		if (vstream->dewarp_wrap.cam_clk_apb == NULL) {
			loge(p_dev, "of_clk_get_apb\n");
			ret = -ENODEV;
		}
	}

	return ret;
}

static void tcc_dewarp_put_clock(const struct tcc_dewarp_stream *vstream)
{
	clk_put(vstream->dewarp_wrap.cam_clk_axi);
	clk_put(vstream->dewarp_wrap.cam_clk_pix);
	clk_put(vstream->dewarp_wrap.cam_clk_apb);
}

static int tcc_dewarp_enable_clock(const struct tcc_dewarp_stream *vstream)
{
	const struct device *p_dev = NULL;
	int ret = 0;

	p_dev = stream_to_device(vstream);

	ret = clk_prepare_enable(vstream->dewarp_wrap.cam_clk_axi);
	if (ret != 0) {
		/* failure of clk_prepare_enable */
		loge(p_dev, "clk_prepare_enable axi, ret: %d\n", ret);
	}

	if (ret == 0) {
		ret = clk_prepare_enable(vstream->dewarp_wrap.cam_clk_pix);
		if (ret != 0) {
			/* failure of clk_prepare_enable */
			loge(p_dev, "clk_prepare_enable pix, ret: %d\n", ret);
		}
	}

	if (ret == 0) {
		ret = clk_prepare_enable(vstream->dewarp_wrap.cam_clk_apb);
		if (ret != 0) {
			/* failure of clk_prepare_enable */
			loge(p_dev, "clk_prepare_enable apb, ret: %d\n", ret);
		}
	}

	return ret;
}

static void tcc_dewarp_disable_clock(const struct tcc_dewarp_stream *vstream)
{
	clk_disable_unprepare(vstream->dewarp_wrap.cam_clk_axi);
	clk_disable_unprepare(vstream->dewarp_wrap.cam_clk_pix);
	clk_disable_unprepare(vstream->dewarp_wrap.cam_clk_apb);
}

/*
 * tcc_dewarp_parse_dt
 *
 * - DESCRIPTION:
 *	Parse video-input path's device tree
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 *	-ENODEV:	a certain device node is not found.
 */

static int tcc_dewarp_parse_dt(struct tcc_dewarp_stream *vstream,
			       const struct device_node *dev_node)
{
	const struct device *p_dev = NULL;
	struct device_node *odw_node = NULL;
	struct tcc_dewarp_wrap *dewarp_wrap = NULL;
	struct dewarp_odw_params *odw_params = NULL;
	const char *name = NULL;
	int ret = 0;

	const char *const name_list[ODW_DEV_MAX] = {
		[ODW_DEV_ID] = "odw-id",
		[ODW_DEV_BYPASS] = "odw-bypass",
		[ODW_DEV_IRQ] = "odw-irq",
	};

	p_dev = stream_to_device(vstream);
	dewarp_wrap = &vstream->dewarp_wrap;
	odw_params = &dewarp_wrap->odw_params;

	/* cam mux */
	odw_node = of_parse_phandle(dev_node, "cam-mux", 0);
	if (!IS_ERR_OR_NULL(odw_node)) {
		(void)of_property_read_u32_index(dev_node, "cam-mux", 1,
						 &dewarp_wrap->cam_mux);
		dewarp_wrap->cam_mux_addr = of_iomap(odw_node, 0);
		logd(p_dev, "%10s: %u\n", "CAM-MUX", dewarp_wrap->cam_mux);
	} else {
		loge(p_dev, "\"cam-mux\" node is not found.\n");
		ret = -ENODEV;
	}

	odw_node = of_get_parent(dev_node);
	if (!IS_ERR_OR_NULL(odw_node)) {
		(void)of_property_read_u32(dev_node, "cam-ch",
					   &dewarp_wrap->cam_ch);
		(void)of_property_read_u32(dev_node, name_list[ODW_DEV_ID],
					   &odw_params->id);
		(void)of_property_read_u32(dev_node, name_list[ODW_DEV_BYPASS],
					   &odw_params->in.is_bypass);
		if (odw_params->in.is_bypass == 1U) {
			odw_params->in.is_dewarp = 0U;
		} else {
			odw_params->in.is_dewarp = 1U;
		}
		(void)of_property_read_u32(dev_node, "interrupt-delay",
					   &odw_params->eof_interrupt_delay);

		logd(p_dev, "%s : %d, %s : %d, odw-dewarp : %d\n",
		     name_list[ODW_DEV_ID], name_list[ODW_DEV_BYPASS],
		     odw_params->id, odw_params->in.is_bypass,
		     odw_params->in.is_dewarp);
	} else {
		loge(p_dev, "\"%s\" node is not found.\n",
		     name_list[ODW_DEV_ID]);
		ret = -ENODEV;
	}

	ret = platform_get_irq_byname(vstream->tdev->pdev,
				      name_list[ODW_DEV_IRQ]);
	if (ret <= 0) {
		/* error */
		loge(p_dev, "Invalid IRQ(%d)\n", ret);
	} else {
		/* okay */
		name = "odw";
		dewarp_wrap->intr.num = clamp_t(s32, ret, 0, INT_MAX);
		logd(p_dev, "%s : %u\n", name_list[ODW_DEV_IRQ],
		     dewarp_wrap->intr.num);
		ret = 0;
	}

	return ret;
}

static int tcc_dewarp_parse_params(struct tcc_dewarp_stream *vstream)
{
	const struct device *p_dev = NULL;
	struct tcc_dewarp_wrap *dewarp_wrap = NULL;
	struct dewarp_odw_params *odw_params = NULL;
	struct dewarp_odw_input_params *in = NULL;
	int ret = 0;

	p_dev = stream_to_device(vstream);
	dewarp_wrap = &vstream->dewarp_wrap;
	odw_params = &dewarp_wrap->odw_params;
	in = &odw_params->in;

	/* TODO: max_ros, max_rburst are not used in 750x */
	odw_params->max_ros = 0x7;
	odw_params->max_rburst = 0x7;
	odw_params->max_wos = 0x7;
	odw_params->max_wburst = 0x7;

	/*  Temporarily register setting to check the dewarp function */
	if (odw_params->in.is_dewarp == 1) {
		odw_params->frame_time_cycle = 0x0989680;
		odw_params->frame_timeout_thres = 0x0989680;
		odw_params->frame_toggle_thres = 0x200;
		odw_params->line_toggle_thres = 0x400;
		odw_params->max_toggle_thres = 0x4;
		odw_params->offset_x = 0x0;
		odw_params->offset_y = 0x0;
		odw_params->interval_x = 0x4;
		odw_params->interval_y = 0x2;
		odw_params->firs[0] = 0x0;
		odw_params->firs[1] = 0xC;
		odw_params->firs[2] = 0x28;
		odw_params->firs[3] = 0xC;
		odw_params->firs[4] = 0x0;
		odw_params->iirs[0] = 0x28;
		odw_params->iirs[1] = 0x20;
		odw_params->iirs[2] = 0xF8;
		odw_params->cam_mat[0] = 0x4801D8;
		odw_params->cam_mat[1] = 0x197C35;
		odw_params->cam_mat[2] = 0x77E353;
		odw_params->cam_mat[3] = 0x47E529;
		odw_params->cam_mat[4] = 0x435415;
		odw_params->dist_coeff_fwds[0] = 0xF7D737;
		odw_params->dist_coeff_fwds[1] = 0x1EDEB;
		odw_params->dist_coeff_fwds[2] = 0x0AAB;
		odw_params->dist_coeff_fwds[3] = 0x0DC9;
		odw_params->dist_coeff_bwds[0] = 0x7E80B;
		odw_params->dist_coeff_bwds[1] = 0x0C73A;
		odw_params->dist_coeff_bwds[2] = 0xFDE1FC;
		odw_params->dist_coeff_bwds[3] = 0x5365;
		odw_params->homography_fwd[0] = 0x122E8B;
		odw_params->homography_fwd[1] = 0x66F;
		odw_params->homography_fwd[2] = 0x1DF8D4;
		odw_params->homography_fwd[3] = 0x0;
		odw_params->homography_fwd[4] = 0x12274D;
		odw_params->homography_fwd[5] = 0x10D505;
		/* TODO: 807x has 2 more homography_fwd register */
		odw_params->homography_bwd[0] = 0x1C28F;
		odw_params->homography_bwd[1] = 0xFF6049;
		odw_params->homography_bwd[2] = 0xF2D534;
		odw_params->homography_bwd[3] = 0x0;
		odw_params->homography_bwd[4] = 0x1C343;
		odw_params->homography_bwd[5] = 0xF89513;
		/* TODO: 807x has 2 more homography_bwd register */
		odw_params->scan_xy_swap = 0;
		odw_params->scan_margin_x = 0;
		odw_params->scan_margin_y = 0;
		odw_params->round_thres = 0xE;
		in->is_fisheye = 1;
	} else {
		logi(p_dev, "skip dewarp setting(bypass mode)\n");
	}

	return ret;
}

static void tcc_dewarp_load_fw_cfg(struct tcc_dewarp_stream *vstream)
{
	int ret = 0;
	const struct firmware *fw = NULL;
	struct device *p_dev = NULL;
	const char *fname = "dewarp";

	p_dev = stream_to_device(vstream);

	ret = request_firmware(&fw, fname, p_dev);
	if (ret < 0) {
		dev_err(p_dev,
			"request_firmware(%s) returned an error: %d. Skip to load a fw\n",
			fname, ret);
	} else {
		dev_info(p_dev, "load %s(size: %ld Byte)\n", fname, fw->size);
		parse_cfg_and_set_regs(vstream, fw);
		release_firmware(fw);
	}
}

static int tcc_dewarp_parse_reserved_memory(struct tcc_dewarp_stream *vstream,
					    const struct device_node *dev_node)
{
	const struct device *p_dev = NULL;
	struct device_node *mem_node = NULL;
	/* the ordor of reserved memorys depends on enum reserved_memory */
	const char *const name_list[RESERVED_MEM_MAX] = {
		[RESERVED_MEM_PGL] = "pmap_pgl",
		[RESERVED_MEM_PREV] = "pmap_prev",
	};
	const char *name = NULL;
	u32 idxMem = 0;
	u32 base = 0;
	u32 size = 0;
	int ret = 0;

	p_dev = stream_to_device(vstream);

	for (idxMem = 0; idxMem < (u32)RESERVED_MEM_MAX; idxMem++) {
		mem_node = of_parse_phandle(dev_node, "memory-region",
					    clamp_t(s32, idxMem, 0, INT_MAX));
		name = name_list[idxMem];
		if (!IS_ERR_OR_NULL(mem_node)) {
			vstream->dewarp_wrap.rsvd_mem[idxMem] =
				of_reserved_mem_lookup(mem_node);
			if (!IS_ERR_OR_NULL(
				    vstream->dewarp_wrap.rsvd_mem[idxMem])) {
#if defined(CONFIG_ARM64)
				base = clamp_t(u32,
					       vstream->dewarp_wrap
						       .rsvd_mem[idxMem]
						       ->base,
					       0, UINT_MAX);
				size = clamp_t(u32,
					       vstream->dewarp_wrap
						       .rsvd_mem[idxMem]
						       ->size,
					       0, UINT_MAX);
#else
				base = vstream->dewarp_wrap.rsvd_mem[idxMem]
					       ->base;
				size = vstream->dewarp_wrap.rsvd_mem[idxMem]
					       ->size;
#endif //defined(CONFIG_ARM64)
				logd(p_dev, "%20s: 0x%08x ~ 0x%08x (0x%08x)\n",
				     name, base, base + size, size);
			}
		} else {
			logd(p_dev, "\"%s\" node is not found.\n", name);
			if (idxMem == (u32)RESERVED_MEM_PREV) {
				/* error in case of RESERVED_MEM_PREV */
				ret = -1;
			}
		}
		of_node_put(mem_node);
	}

	return ret;
}

static int tcc_dewarp_parse_fwnode(struct tcc_dewarp_stream *vstream,
				   const struct device_node *dev_node)
{
	const struct device *p_dev = NULL;
	struct device_node *loc_ep = NULL;
	struct fwnode_handle *fwnode = NULL;
	struct v4l2_fwnode_endpoint fw_ep = {
		0,
	};
	int ret_call = 0;
	int ret = 0;

	p_dev = stream_to_device(vstream);

	/* get phandle of subdev */
	loc_ep = of_graph_get_endpoint_by_regs(dev_node, -1, -1);
	if (loc_ep == NULL) {
		loge(p_dev, "No subdev is found\n");
		ret = -EINVAL;
	} else {
		fwnode = of_fwnode_handle(loc_ep);

		fw_ep.bus_type = V4L2_MBUS_UNKNOWN;
		ret_call = v4l2_fwnode_endpoint_parse(fwnode, &fw_ep);
		if (ret_call != 0) {
			loge(p_dev, "v4l2_fwnode_endpoint_parse, ret: %d\n",
			     ret_call);
			ret = -1;
		}
	}

	return ret;
}

static bool
tcc_dewarp_video_is_framesize_supported(const struct tcc_dewarp_stream *vstream,
					u32 width, u32 height)
{
	const struct device *p_dev = NULL;

	bool ret = true;

	p_dev = stream_to_device(vstream);

	logd(p_dev, "frmaesize(%u * %u)\n", width, height);

	if ((width == 0U) || (height == 0U)) {
		loge(p_dev, "width or height is 0\n");
		ret = false;
	}

	if ((width * height) >= (MAX_FRAMEWIDTH * MAX_FRAMEHEIGHT)) {
		loge(p_dev,
		     "frmaesize(%u * %u) exceeds the maximum size(%u * %u)\n",
		     width, height, MAX_FRAMEWIDTH, MAX_FRAMEHEIGHT);
		ret = false;
	}

	return ret;
}

/*
 * tcc_dewarp_map_cam_mux
 *
 * - DESCRIPTION:
 *	Map cam mux to receive video data
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */
static int tcc_dewarp_map_cam_mux(struct tcc_dewarp_stream *vstream)
{
	const struct device *p_dev = NULL;
	void __iomem *addr = NULL;
	u32 mask = 0x7;
	u32 odw_index = 0;
	u32 value = 0;

	p_dev = stream_to_device(vstream);
	addr = vstream->dewarp_wrap.cam_mux_addr;

	odw_index = vstream->dewarp_wrap.odw_params.id;

	spin_lock_irq(&cam_mux_cfg_lock);
	value = ((__raw_readl(addr) & ~(mask << (odw_index * 3U))) |
		 (vstream->dewarp_wrap.cam_ch << (odw_index * 3U)));

	__raw_writel(value, addr);

	value = __raw_readl(addr);
	spin_unlock_irq(&cam_mux_cfg_lock);

	logd(p_dev,
	     "CAM Mux: %d, CAM CH: %d, ODW Index: %d, Register Value: 0x%08x\n",
	     vstream->dewarp_wrap.cam_mux, vstream->dewarp_wrap.cam_ch,
	     odw_index, value);

	return 0;
}

/*
 * tcc_dewarp_set_path
 *
 * - DESCRIPTION:
 *	Set dewarp component to receive video data via mipi-csi
 *
 * - PARAMETERS:
 *	@vstream:	video-input path device's data
 *
 * - RETURNS:
 *	0:		Success
 */

static int tcc_dewarp_set_path(struct tcc_dewarp_stream *vstream)
{
	unsigned long flags = 0;
	const struct device *p_dev = NULL;
	struct tcc_dewarp_queue *p_queue = NULL;
	struct tcc_dewarp_wrap *dewarp_wrap = NULL;
	struct tcc_dewarp_buffer *p_buf = NULL;
	const struct tcc_dewarp_vs_info *vs_info = NULL;
	struct vb2_buffer *vb = NULL;
	const struct v4l2_pix_format_mplane *pix_mp = NULL;
	const struct tcc_dewarp_format *pformat = NULL;
	u32 dma_addrs[MAX_PLANES];

	struct dewarp_odw_params *odw_params = NULL;
	struct dewarp_odw_input_params *in = NULL;
	struct dewarp_odw_output_params *out = NULL;

	u32 idxpln = 0;

	p_dev = stream_to_device(vstream);
	p_queue = &vstream->queue;
	dewarp_wrap = &vstream->dewarp_wrap;
	vs_info = &vstream->vs_info;
	pix_mp = &vstream->format.fmt.pix_mp;
	odw_params = &dewarp_wrap->odw_params;

	in = &odw_params->in;
	out = &odw_params->out;

	in->width = vs_info->width;
	in->height = vs_info->height;
	in->format = vs_info->data_format;
	// hhj check
	in->ir_enable = 0;

	out->width = pix_mp->width;
	out->height = pix_mp->height;
	pformat = tcc_odw_get_format_by_pixelformat(pix_mp->pixelformat);
	if (pformat == NULL) {
		loge(p_dev, "Failed to set ODW: unsupported format\n");

		return -EINVAL;
	}
	out->format = pformat->guid;

	spin_lock_irqsave(&p_queue->slock, flags);

	if (!list_empty(&p_queue->buf_list)) {
		p_buf = list_first_entry(&p_queue->buf_list,
					 struct tcc_dewarp_buffer, entry);
	}
	spin_unlock_irqrestore(&p_queue->slock, flags);

	if (p_buf != NULL) {
		vb = &p_buf->buf.vb2_buf;
		(void)memset(dma_addrs, 0, sizeof(dma_addrs));
		tcc_dewarp_get_dma_addrs(vstream, vb, dma_addrs);
		tcc_dewarp_print_dma_addrs(vstream, vb, dma_addrs);

		switch (out->format) {
		case (unsigned int)DEWARP_COLOR_FMT_RGB:
			out->address[0] = dma_addrs[0];
			out->strides[0] = out->width * 4;
			break;
		case (unsigned int)DEWARP_COLOR_FMT_VYUY:
		case (unsigned int)DEWARP_COLOR_FMT_UYVY:
		case (unsigned int)DEWARP_COLOR_FMT_YVYU:
		case (unsigned int)DEWARP_COLOR_FMT_YUYV:
			out->address[0] = dma_addrs[0];
			out->strides[0] = out->width * 2;
			break;
		case (unsigned int)DEWARP_COLOR_FMT_YVU422IL:
		case (unsigned int)DEWARP_COLOR_FMT_YUV422IL:
		case (unsigned int)DEWARP_COLOR_FMT_YUV420IL_ODD:
		case (unsigned int)DEWARP_COLOR_FMT_YVU420IL_ODD:
		case (unsigned int)DEWARP_COLOR_FMT_YUV420IL_EVEN:
		case (unsigned int)DEWARP_COLOR_FMT_YVU420IL_EVEN:
			for (idxpln = 0; idxpln < vb->num_planes; idxpln++) {
				out->address[idxpln] = dma_addrs[idxpln];
				out->strides[idxpln] = out->width;
			}
			break;
		default:
			break;
		}

		odw_params->stream_enable = 1;

		logd(p_dev, "id(%d), in(%d x %d, %d), out(%d x %d, %d)\n",
		     odw_params->id, in->width, in->height, in->format,
		     out->width, out->height, out->format);
		logd(p_dev,
		     "id(%d), out->address[0] : 0x%08x, out->address[1] : 0x%08x out->address[2] : 0x%08x\n",
		     odw_params->id, out->address[0], out->address[1],
		     out->address[2]);
		logd(p_dev,
		     "id(%d), out->strides[0] : %d, out->strides[1] : %d, out->strides[2] : %d\n",
		     odw_params->id, out->strides[0], out->strides[1],
		     out->strides[2]);
		logd(p_dev,
		     "id(%d), ir_enable(%d), is_dewarp(%d), stream_enable(%d), is_bypass(%d)\n",
		     odw_params->id, in->ir_enable, in->is_dewarp,
		     odw_params->stream_enable, in->is_bypass);

		dewarp_odw_set_input_size(odw_params->id, in->width,
					  in->height);
		dewarp_odw_set_input_format(odw_params->id, in->format,
					    in->ir_enable);

		dewarp_odw_set_output_size(odw_params->id, out->width,
					   out->height);
		dewarp_odw_set_output_format(odw_params->id, out->format);
		dewarp_odw_set_output_address(odw_params->id, out->address,
					      in->ir_enable);
		dewarp_odw_set_output_stride(odw_params->id, out->strides);

		if (odw_params->in.is_dewarp == 1) {
			dewarp_odw_set_input_fisheye(odw_params->id,
						     in->is_fisheye);
			dewarp_odw_set_frame_time_cycle(
				odw_params->id, odw_params->frame_time_cycle);
			dewarp_odw_set_frame_timeout_threshold(
				odw_params->id,
				odw_params->frame_timeout_thres);
			dewarp_odw_set_frame_toggle_threshold(
				odw_params->id, odw_params->frame_toggle_thres);
			dewarp_odw_set_line_toggle_threshold(
				odw_params->id, odw_params->line_toggle_thres);
			dewarp_odw_set_max_toggle_threshold(
				odw_params->id, odw_params->max_toggle_thres);
			dewarp_odw_set_block_config(odw_params->id,
						    odw_params->offset_x,
						    odw_params->offset_y,
						    odw_params->interval_x,
						    odw_params->interval_y);
			dewarp_odw_set_filter(odw_params->id, odw_params->firs,
					      odw_params->iirs);
			dewarp_odw_set_backward_scan_config(
				odw_params->id, odw_params->scan_xy_swap,
				odw_params->scan_margin_x,
				odw_params->scan_margin_y,
				odw_params->round_thres);
		}

		/* It can be changed depending on the results of the SOC review */
		dewarp_odw_set_axi_config(odw_params->id, odw_params->max_ros,
					  odw_params->max_wos,
					  odw_params->max_rburst,
					  odw_params->max_wburst);
		dewarp_odw_set_dewarp_fifo_timeout_cnt(odw_params->id, 0);
		dewarp_odw_set_eof_interrupt_delay(
			odw_params->id, odw_params->eof_interrupt_delay);

		tcc_dewarp_load_fw_cfg(vstream);

		dewarp_odw_enable(odw_params->id, in->is_dewarp,
				  odw_params->stream_enable, in->is_bypass);
		dewarp_odw_update(odw_params->id);
	}
	return 0;
}

static void
tcc_dewarp_get_and_update_time(const struct tcc_dewarp_stream *vstream,
			       struct timespec64 *ts_prev,
			       struct timespec64 *ts_next, u32 debug)
{
	const struct device *p_dev = NULL;
	struct timespec64 ts_diff = {
		0,
	};

	p_dev = stream_to_device(vstream);

	ktime_get_raw_ts64(ts_next);

	if (debug == 1U) {
		ts_diff.tv_sec = ts_next->tv_sec - ts_prev->tv_sec;
		ts_diff.tv_nsec = ts_next->tv_nsec - ts_prev->tv_nsec;
		if (ts_diff.tv_nsec < 0) {
			ts_diff.tv_sec -= 1;
			ts_diff.tv_nsec += 1000000000;
		}

		logi(p_dev, "timestamp curr: %9lld.%09ld, diff: %9lld.%09ld\n",
		     ts_next->tv_sec, ts_next->tv_nsec, ts_diff.tv_sec,
		     ts_diff.tv_nsec);
	}

	*ts_prev = *ts_next;
}

static u32 list_get_entry_count(struct tcc_dewarp_stream *vstream,
				struct list_head *head)
{
	struct list_head *list = NULL;
	struct tcc_dewarp_buffer *p_buf = NULL;
	const struct device *p_dev = NULL;
	u32 count = 0;

	p_dev = stream_to_device(vstream);
	list_for_each (list, head) {
		p_buf = list_entry(list, struct tcc_dewarp_buffer, entry);
		count++;
	}

	logd(p_dev, "list_get_entry_count: %d\n", count);

	return count;
}

static irqreturn_t tcc_dewarp_odw_isr(int irq, void *data)
{
	struct tcc_dewarp_stream *vstream = NULL;
	const struct device *p_dev = NULL;
	struct tcc_dewarp_queue *p_queue = NULL;
	struct tcc_dewarp_wrap *dewarp_wrap = NULL;
	const struct cam_intr *intr = NULL;
	unsigned long flags = 0;
	struct vb2_buffer *vb = NULL;
	u32 dma_addrs[MAX_PLANES];

	struct dewarp_odw_params *odw_params = NULL;
	struct dewarp_odw_input_params *in = NULL;
	struct dewarp_odw_output_params *out = NULL;

	u32 idxpln = 0;

	int ret_call = 0;
	irqreturn_t ret = IRQ_NONE;

	vstream = (struct tcc_dewarp_stream *)data;
	p_dev = stream_to_device(vstream);
	p_queue = &vstream->queue;

	dewarp_wrap = &vstream->dewarp_wrap;
	intr = &dewarp_wrap->intr;

	odw_params = &dewarp_wrap->odw_params;
	in = &odw_params->in;
	out = &odw_params->out;

	dewarp_odw_clear_interrupt(odw_params->id);

	spin_lock_irqsave(&p_queue->slock, flags);

	vstream->prev_buf = NULL;
	vstream->curr_buf = NULL;
	vstream->next_buf = NULL;

	/* check if frameskip is needed */
	if (vstream->skip_frame_cnt > 0) {
		logd(p_dev, "skip frame count: 0x%08x\n",
		     vstream->skip_frame_cnt);
		vstream->skip_frame_cnt--;

		goto odw_update;
	}

	/* check if the incoming buffer list is empty */

	if (list_empty(&vstream->queue.buf_list)) {
		loge(p_dev, "The incoming buffer list is empty\n");

		goto odw_update;
	}

	vstream->prev_buf = list_first_entry(&p_queue->buf_list,
					     struct tcc_dewarp_buffer, entry);

	ret_call = ((p_queue->flags & (u32)TCC_DEWARP_QUEUE_DROP_CORRUPTED) !=
		    0U) ?
			   1 :
			   0;
	if (ret_call != 0) {
		loge(p_dev, "The buffer is corrupted\n");
		vstream->prev_buf = NULL;

		goto odw_update;
	}

	/* check if the incoming buffer list has only one entry */

	if (list_is_last(&vstream->prev_buf->entry, &p_queue->buf_list)) {
		logd(p_dev, "driver has only one buffer\n");
		vstream->prev_buf = NULL;

		goto odw_update;
	}

	if (list_get_entry_count(vstream, &vstream->queue.buf_list) < 3) {
		vstream->curr_buf = list_next_entry(vstream->prev_buf, entry);
		if (IS_ERR_OR_NULL(vstream->prev_buf) ||
		    IS_ERR_OR_NULL(vstream->curr_buf)) {
			loge(p_dev, "prev(0x%p) or curr(0x%p) is wrong\n",
			     vstream->prev_buf, vstream->curr_buf);

			goto odw_update;
		}
		vb = &vstream->curr_buf->buf.vb2_buf;

		(void)memset(dma_addrs, 0, sizeof(dma_addrs));
		tcc_dewarp_get_dma_addrs(vstream, vb, dma_addrs);
		tcc_dewarp_print_dma_addrs(vstream, vb, dma_addrs);
		for (idxpln = 0; idxpln < vb->num_planes; idxpln++) {
			out->address[idxpln] = dma_addrs[idxpln];
		}

		vstream->prev_buf = NULL;
		vstream->curr_buf = NULL;

		dewarp_odw_set_output_address(odw_params->id, out->address,
					      in->ir_enable);

		goto odw_update;
	}

	/* The incoming buffer list has two or more entries. */

	vstream->curr_buf = list_next_entry(vstream->prev_buf, entry);
	vstream->next_buf = list_next_entry(vstream->curr_buf, entry);

	if (IS_ERR_OR_NULL(vstream->prev_buf) ||
	    IS_ERR_OR_NULL(vstream->curr_buf) ||
	    IS_ERR_OR_NULL(vstream->next_buf)) {
		loge(p_dev, "prev(0x%p) or curr(0x%p) or next(0x%p) is wrong\n",
		     vstream->prev_buf, vstream->curr_buf, vstream->next_buf);

		goto odw_update;
	}

	logd(p_dev, "bufidx: %d, type: 0x%08x, memory: 0x%08x\n",
	     vstream->next_buf->buf.vb2_buf.index,
	     vstream->next_buf->buf.vb2_buf.type,
	     vstream->next_buf->buf.vb2_buf.memory);

	vb = &vstream->next_buf->buf.vb2_buf;
	(void)memset(dma_addrs, 0, sizeof(dma_addrs));
	tcc_dewarp_get_dma_addrs(vstream, vb, dma_addrs);
	tcc_dewarp_print_dma_addrs(vstream, vb, dma_addrs);
	for (idxpln = 0; idxpln < vb->num_planes; idxpln++) {
		out->address[idxpln] = dma_addrs[idxpln];
	}

	tcc_dewarp_get_and_update_time(vstream, &vstream->ts_prev,
				       &vstream->ts_next, vstream->timestamp);

	if (!IS_ERR_OR_NULL(vstream->prev_buf) &&
	    !IS_ERR_OR_NULL(vstream->curr_buf) &&
	    !IS_ERR_OR_NULL(vstream->next_buf)) {
		/* fill the buffer info */
		vstream->prev_buf->buf.vb2_buf.timestamp = clamp_t(
			u64, timespec64_to_ns(&vstream->ts_next), 0UL, S64_MAX);
		vstream->prev_buf->buf.field = (u32)V4L2_FIELD_NONE;
		vstream->sequence = clamp(vstream->sequence + 1U, 0U, UINT_MAX);
		vstream->prev_buf->buf.sequence = vstream->sequence;

		/* dequeue prev_buf from the incoming buffer list */
		list_del(&vstream->prev_buf->entry);

		/* queue the prev_buf to the outgoing buffer list */
		vb2_buffer_done(&vstream->prev_buf->buf.vb2_buf,
				VB2_BUF_STATE_DONE);

		vstream->prev_buf = NULL;
		vstream->next_buf = NULL;
	}

	dewarp_odw_set_output_address(odw_params->id, out->address,
				      in->ir_enable);

	ret = IRQ_HANDLED;

odw_update:
	spin_unlock_irqrestore(&p_queue->slock, flags);
	dewarp_odw_update(odw_params->id);

	return ret;
}

static int tcc_dewarp_start_stream(struct tcc_dewarp_stream *vstream)
{
	const struct device *p_dev = NULL;
	const struct v4l2_pix_format_mplane *pix_mp = NULL;
	int ret_call = 0;
	int ret = 0;

	p_dev = stream_to_device(vstream);
	pix_mp = &vstream->format.fmt.pix_mp;

	/* size info */
	logd(p_dev, "preview size: %d * %d\n", pix_mp->width, pix_mp->height);

	/* map cif-port */
	ret_call = tcc_dewarp_map_cam_mux(vstream);
	if (ret_call < 0) {
		loge(p_dev, "tcc_dewarp_map_cam_mux, ret: %d\n", ret_call);
		ret = -1;
	}

	/* set odw */
	ret_call = tcc_dewarp_set_path(vstream);
	if (ret_call < 0) {
		loge(p_dev, "tcc_dewarp_set_path, ret: %d\n", ret_call);
		ret = -1;
	}

	return ret;
}

static int tcc_dewarp_stop_stream(struct tcc_dewarp_stream *vstream)
{
	const struct device *p_dev = NULL;
	const struct tcc_dewarp_vs_info *vs_info = NULL;
	struct tcc_dewarp_wrap *dewarp_wrap = NULL;
	struct dewarp_odw_params *odw_params = NULL;
	struct dewarp_odw_input_params *in = NULL;

	p_dev = stream_to_device(vstream);
	dewarp_wrap = &vstream->dewarp_wrap;
	vs_info = &vstream->vs_info;

	odw_params = &dewarp_wrap->odw_params;
	in = &odw_params->in;

	// dewarp_odw_enable(1, 0, 1, 0);
	odw_params->stream_enable = 0;
	dewarp_odw_enable(odw_params->id, in->is_dewarp,
			  odw_params->stream_enable, in->is_bypass);
	dewarp_odw_update(odw_params->id);
	return 0;
}

static int tcc_dewarp_request_irq(struct tcc_dewarp_stream *vstream)
{
	const struct device *p_dev = NULL;
	struct tcc_dewarp_wrap *dewarp_wrap = NULL;
	struct cam_intr *intr = NULL;
	struct dewarp_odw_params *odw_params = NULL;
	int ret_call = 0;
	int ret = 0;

	p_dev = stream_to_device(vstream);

	/* vin interrupt */
	dewarp_wrap = &vstream->dewarp_wrap;
	intr = &dewarp_wrap->intr;
	odw_params = &dewarp_wrap->odw_params;

	if (intr->reg == 0U) {
		dewarp_set_ireq_mask(p_dev, odw_params->id, 1);

		ret_call = request_irq(clamp_t(u32, intr->num, 0, INT_MAX),
				       tcc_dewarp_odw_isr, IRQF_SHARED,
				       vstream->tdev->vdev.name, vstream);
		if (ret_call < 0) {
			loge(p_dev, "dewarp(odw) - request_irq, ret: %d\n",
			     ret_call);
			ret = -1;
		}

		intr->reg = 1;
	} else {
		loge(p_dev, "The irq(%d) is already registered.\n", intr->num);
		ret = -1;
	}

	return ret;
}

static int tcc_dewarp_free_irq(struct tcc_dewarp_stream *vstream)
{
	const struct device *p_dev = NULL;
	struct tcc_dewarp_wrap *dewarp_wrap = NULL;
	struct dewarp_odw_params *odw_params = NULL;
	struct cam_intr *intr = NULL;
	int ret = 0;

	p_dev = stream_to_device(vstream);

	dewarp_wrap = &vstream->dewarp_wrap;
	intr = &dewarp_wrap->intr;
	odw_params = &dewarp_wrap->odw_params;

	if (intr->reg == 1U) {
		(void)free_irq(clamp_t(u32, intr->num, 0, INT_MAX), vstream);
		dewarp_set_ireq_mask(p_dev, odw_params->id, 0);
		intr->reg = 0;
	}

	return ret;
}

static int
tcc_dewarp_video_check_subdev_status(const struct tcc_dewarp_stream *vstream)
{
	const struct device *p_dev = NULL;
	const struct tcc_dewarp_device *tdev = NULL;
	struct v4l2_subdev *subdev = NULL;
	u32 delay = 0;
	u32 idxTry = 0;
	u32 nTry = 3;
	u32 status = 0;
	int ret = 0;

	p_dev = stream_to_device(vstream);
	tdev = vstream->tdev;
	subdev = tdev->tsubdev.sd;

	/* wait for additional stabilization time */
	delay = vstream->vs_stabilization;
	if (delay != 0U) {
		logi(p_dev, "additional stabization time is %d ms\n", delay);
		msleep(delay);
	}

	/* check v4l2 subdev's status */
	idxTry = 0;
	status = (u32)V4L2_IN_ST_NO_SIGNAL;
	do {
		logd(p_dev, "g_input_status (%d / %d)\n", idxTry, nTry);

		ret = tcc_dewarp_subdev_video_g_input_status(subdev, &status);
		if (ret != 0) {
			logw(p_dev, "video.g_input_status is not supported\n");
			status = 0;
			ret = 0;
		} else {
			if (IS_SET(status, V4L2_IN_ST_NO_SIGNAL)) {
				logd(p_dev, "subdev is not stable\n");

				/* 20msec is minimum in msleep() */
				msleep(20);

				idxTry = idxTry + 1U;
			} else {
				logd(p_dev, "subdev is stable\n");
				status = 0;
			}
		}
	} while ((status != 0U) && (idxTry < nTry));

	return ret;
}

int tcc_dewarp_start_subdevs(struct tcc_dewarp_stream *vstream)
{
	const struct device *p_dev = NULL;
	struct v4l2_subdev *subdev = NULL;
	struct v4l2_subdev_format *fmt = NULL;
	struct tcc_dewarp_vs_info *vs_info = NULL;
	struct v4l2_dv_timings timings = {
		0,
	};
	const struct media_link *flink = NULL;
	int ret_call = 0;
	int ret = 0;

	p_dev = stream_to_device(vstream);
	fmt = &vstream->tdev->tsubdev.fmt;
	vs_info = &vstream->vs_info;

	/* find sub-device linked to sink pad */
	ret_call = tcc_dewarp_subdev_get_src_sd(
		vstream->tdev, &vstream->tdev->pads[0], &subdev);
	if (ret_call < 0) {
		/* error */
		loge(p_dev, "tcc_dewarp_subdev_get_src_sd, ret: %d\n",
		     ret_call);
		ret = ret_call;
	} else {
		flink = container_of(vstream->tdev->vdev.entity.links.next,
				     struct media_link, list);

		fmt = &vstream->tdev->tsubdev.fmt;
		fmt->pad = flink->source->index;
		fmt->which = (u32)V4L2_SUBDEV_FORMAT_ACTIVE;
		ret_call = tcc_dewarp_subdev_pad_get_fmt(subdev, fmt);
		if (ret_call != 0) {
			/* error: tcc_dewarp_subdev_pad_get_fmt */
			logd(p_dev, "tcc_dewarp_subdev_pad_get_fmt, ret: %d\n",
			     ret_call);
		} else {
			tcc_dewarp_get_vs_info_format_by_mbus_format(
				vstream, fmt->format.code, &vstream->vs_info);
			logd(p_dev, "data_format: %u, data_order: %u\n",
			     vs_info->data_format, vs_info->data_order);
		}

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
		ret_call = tcc_dewarp_subdev_video_g_mbus_config(
			subdev, flink->source->index, &vstream->mbus_config);
#else
		ret_call = tcc_dewarp_subdev_video_g_mbus_config(
			subdev, &vstream->mbus_config);
#endif
		if (ret_call != 0) {
			/* error: tcc_dewarp_subdev_video_g_mbus_config */
			logd(p_dev,
			     "tcc_dewarp_subdev_video_g_mbus_config, ret: %d\n",
			     ret_call);
		}

		ret_call =
			tcc_dewarp_subdev_video_g_dv_timings(subdev, &timings);
		if (ret_call != 0) {
			/* error: tcc_dewarp_subdev_video_g_dv_timings */
			logd(p_dev,
			     "tcc_dewarp_subdev_video_g_dv_timings, ret: %d\n",
			     ret_call);
		} else {
			/* size */
			vs_info->height = timings.bt.height;
			vs_info->width = timings.bt.width;
			logd(p_dev, "width: %d, height: %d\n", vs_info->height,
			     vs_info->width);
		}

		ret_call = tcc_dewarp_subdev_core_s_power(subdev, 1);
		if (ret_call != 0) {
			/* error: tcc_dewarp_subdev_video_s_power */
			logd(p_dev,
			     "tcc_dewarp_subdev_video_s_power, ret: %d\n",
			     ret_call);
		}
		ret_call = tcc_dewarp_subdev_core_init(subdev, 1);
		if (ret_call != 0) {
			/* error: tcc_dewarp_subdev_video_init */
			logd(p_dev, "tcc_dewarp_subdev_video_init, ret: %d\n",
			     ret_call);
		}
		ret_call = tcc_dewarp_subdev_video_s_stream(subdev, 1);
		if (ret_call != 0) {
			/* error: tcc_dewarp_subdev_video_s_stream */
			logd(p_dev,
			     "tcc_dewarp_subdev_video_s_stream, ret: %d\n",
			     ret_call);
		}

		ret_call = tcc_dewarp_video_check_subdev_status(vstream);
		if (ret_call != 0) {
			/* error: tcc_dewarp_vioc_check_subdev_status */
			logd(p_dev,
			     "tcc_dewarp_vioc_check_subdev_status, ret: %d\n",
			     ret_call);
		}
	}

	return ret;
}

EXPORT_SYMBOL_GPL(tcc_dewarp_start_subdevs);

int tcc_dewarp_stop_subdevs(const struct tcc_dewarp_stream *vstream)
{
	const struct device *p_dev = NULL;
	struct v4l2_subdev *subdev = NULL;
	int ret = 0;

	p_dev = stream_to_device(vstream);
	/* find sub-device linked to sink pad */
	ret = tcc_dewarp_subdev_get_src_sd(vstream->tdev,
					   &vstream->tdev->pads[0], &subdev);
	if (ret < 0) {
		/* error */
		loge(p_dev, "tcc_dewarp_subdev_get_src_sd, ret: %d\n", ret);
	} else {
		(void)tcc_dewarp_subdev_video_s_stream(subdev, 0);
		(void)tcc_dewarp_subdev_core_init(subdev, 0);
		(void)tcc_dewarp_subdev_core_s_power(subdev, 0);
	}

	return ret;
}

EXPORT_SYMBOL_GPL(tcc_dewarp_stop_subdevs);

int tcc_dewarp_video_init(struct tcc_dewarp_stream *vstream,
			  struct device_node *dev_node)
{
	const struct device *p_dev = NULL;
	int ret_call = 0;
	int ret = 0;

	p_dev = stream_to_device(vstream);

	ret_call = tcc_dewarp_get_clock(vstream, dev_node);
	if (ret_call != 0) {
		loge(p_dev, "tcc_dewarp_get_clock, ret: %d\n", ret_call);
		ret = -ENODEV;
	}

	ret_call = tcc_dewarp_parse_dt(vstream, dev_node);
	if (ret_call != 0) {
		loge(p_dev, "tcc_dewarp_parse_dt, ret: %d\n", ret_call);
		ret = -ENODEV;
	}

	ret_call = tcc_dewarp_parse_params(vstream);
	if (ret_call != 0) {
		loge(p_dev, "tcc_dewarp_parse_params, ret: %d\n", ret_call);
		ret = -ENODEV;
	}

	ret_call = tcc_dewarp_parse_reserved_memory(vstream, dev_node);
	if (ret_call != 0) {
		loge(p_dev, "tcc_dewarp_parse_reserved_memory, ret: %d\n",
		     ret_call);
		ret = -ENODEV;
	}

	ret_call = tcc_dewarp_parse_fwnode(vstream, dev_node);
	if (ret_call != 0) {
		logd(p_dev, "tcc_dewarp_parse_fwnode, ret: %d\n", ret_call);
		ret = -ENODEV;
	}

	return ret;
}

EXPORT_SYMBOL_GPL(tcc_dewarp_video_init);

int tcc_dewarp_video_deinit(const struct tcc_dewarp_stream *vstream)
{
	const struct device *p_dev = NULL;
	int ret = 0;

	p_dev = stream_to_device(vstream);

	tcc_dewarp_put_clock(vstream);

	return ret;
}

EXPORT_SYMBOL_GPL(tcc_dewarp_video_deinit);

bool tcc_dewarp_video_is_pixelformat_supported(
	const struct tcc_dewarp_stream *vstream, u32 pixelformat)
{
	const struct device *p_dev = NULL;
	const struct tcc_dewarp_format *tformat = NULL;

	bool ret = true;

	p_dev = stream_to_device(vstream);

	tformat = tcc_odw_get_format_by_pixelformat(pixelformat);
	if (tformat == NULL) {
		loge(p_dev, "pixelformat(0x%08x) is not supported\n",
		     pixelformat);
		ret = false;
	}

	return ret;
}

EXPORT_SYMBOL_GPL(tcc_dewarp_video_is_pixelformat_supported);

u32 tcc_dewarp_video_get_pixelformat_by_index(
	const struct tcc_dewarp_stream *vstream, u32 index)
{
	const struct device *p_dev = NULL;
	u32 nList = 0;
	u32 pixelformat = 0;

	p_dev = stream_to_device(vstream);

	nList = ARRAY_SIZE(tcc_dewarp_format_list);
	if (index < nList) {
		/* get pixelformat */
		pixelformat = tcc_dewarp_format_list[index].pixelformat;
	}

	return pixelformat;
}

EXPORT_SYMBOL_GPL(tcc_dewarp_video_get_pixelformat_by_index);

bool tcc_dewarp_video_is_format_supported(
	const struct tcc_dewarp_stream *vstream,
	const struct v4l2_format *format)
{
	const struct device *p_dev = NULL;
	const struct v4l2_pix_format_mplane *pix_mp = NULL;

	bool ret_bool = true;

	bool ret = true;

	p_dev = stream_to_device(vstream);
	pix_mp = &format->fmt.pix_mp;

	if (format->type != (u32)V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		/* the format is supported */
		ret = false;
	}

	ret_bool = tcc_dewarp_video_is_framesize_supported(
		vstream, pix_mp->width, pix_mp->height);
	if (!ret_bool) {
		loge(p_dev, "tcc_dewarp_video_is_framesize_supported\n");
		ret = false;
	}

	ret_bool = tcc_dewarp_video_is_pixelformat_supported(
		vstream, pix_mp->pixelformat);
	if (!ret_bool) {
		loge(p_dev, "tcc_dewarp_video_is_pixelformat_supported\n");
		ret = false;
	}

	return ret;
}

EXPORT_SYMBOL_GPL(tcc_dewarp_video_is_format_supported);

int tcc_dewarp_video_declare_coherent_dma_memory(
	const struct tcc_dewarp_stream *vstream)
{
	struct device *p_dev = NULL;
	int ret = 0;

	p_dev = stream_to_device(vstream);

	ret = of_reserved_mem_device_init_by_idx(p_dev, p_dev->of_node, 1);
	if (ret < 0) {
		loge(p_dev, "Failed to init per-device memory: %d\n", ret);
	}

	return ret;
}

EXPORT_SYMBOL_GPL(tcc_dewarp_video_declare_coherent_dma_memory);

static inline bool tcc_svdw_is_running(struct tcc_dewarp_device *dev)
{
	return dev->svdw_dwp_unit.svdw_mode;
}

int tcc_dewarp_video_streamon(struct tcc_dewarp_stream *vstream)
{
	const struct device *p_dev = NULL;
	struct tcc_dewarp_device *dwrp_dev = NULL;
	u32 flags = 0;
	int ret_call = 0;
	int ret = 0;

	p_dev = stream_to_device(vstream);
	flags = vstream->handover_flags;
	dwrp_dev = container_of(vstream, struct tcc_dewarp_device, vstream);

	/* print handover flags */
	tcc_dewarp_print_handover_flags(vstream, flags);

	/* skip to handle video source */
	if (!tcc_dewarp_handover_flagged(flags, V4L2_CAP_CTRL_SKIP_SUBDEV)) {
		/* start v4l2-subdev */
		(void)tcc_dewarp_start_subdevs(vstream);
	}

	if (!tcc_svdw_is_running(dwrp_dev)) {
		ret_call = tcc_dewarp_enable_clock(vstream);
		if (ret_call < 0) {
			loge(p_dev, "tcc_dewarp_enable_clock, ret: %d\n",
			     ret_call);
			ret = -1;
		}

		/* IMPORTANT: VIOC Interrupt MUST BE Requested after VIOC RESET Sequence */
		ret_call = tcc_dewarp_request_irq(vstream);
		if (ret_call < 0) {
			loge(p_dev, "tcc_dewarp_request_irq, ret: %d\n",
			     ret_call);
			ret = -1;
		}
	}

	/* skip to handle video-capture */
	if (!tcc_dewarp_handover_flagged(flags, V4L2_CAP_CTRL_SKIP_DEV)) {
		ret_call = tcc_dewarp_start_stream(vstream);
		if (ret_call < 0) {
			loge(p_dev, "tcc_dewarp_start_stream, ret: %d\n",
			     ret_call);
			ret = -1;
		}
	}

	return ret;
}

EXPORT_SYMBOL_GPL(tcc_dewarp_video_streamon);

int tcc_dewarp_video_streamoff(struct tcc_dewarp_stream *vstream)
{
	struct tcc_dewarp_device *dwrp_dev = NULL;
	const struct device *p_dev = NULL;
	u32 flags = 0;
	int ret_call = 0;
	int ret = 0;

	p_dev = stream_to_device(vstream);
	flags = vstream->handover_flags;
	dwrp_dev = container_of(vstream, struct tcc_dewarp_device, vstream);

	/* print handover flags */
	tcc_dewarp_print_handover_flags(vstream, flags);

	/* skip to handle video-capture */
	if (!tcc_dewarp_handover_flagged(flags, V4L2_CAP_CTRL_SKIP_DEV)) {
		ret_call = tcc_dewarp_stop_stream(vstream);
		if (ret_call < 0) {
			loge(p_dev, "tcc_dewarp_stop_stream, ret: %d\n",
			     ret_call);
			ret = -1;
		}
	}

	if (!tcc_svdw_is_running(dwrp_dev)) {
		ret_call = tcc_dewarp_free_irq(vstream);
		if (ret_call < 0) {
			loge(p_dev, "tcc_dewarp_free_irq, ret: %d\n", ret_call);
			ret = -1;
		}
		tcc_dewarp_disable_clock(vstream);
	}

	/* skip to handle video source */
	if (!tcc_dewarp_handover_flagged(flags, V4L2_CAP_CTRL_SKIP_SUBDEV)) {
		/* stop v4l2-subdev */
		(void)tcc_dewarp_stop_subdevs(vstream);
	}

	return ret;
}

EXPORT_SYMBOL_GPL(tcc_dewarp_video_streamoff);

void tcc_dewarp_video_check_path_status(const struct tcc_dewarp_stream *vstream,
					u32 *status)
{
	//	const struct device			*dev		= NULL;
	//	const struct dewarp_comp		*dewarp_path	= NULL;
	//	u32				prev_addr	= 0;
	//	u32				curr_addr	= 0;
	//	u32				nCheck		= 0;
	//	u32				idxCheck	= 0;
	//	u32				delay		= 20;

	//	dev		= stream_to_device(vstream);
	//	vin_path	= vstream->cif.vin_path;
	// #if defined(CONFIG_ARCH_TCC750X)
	//	pWDMA		= VIOC_WDMA_VIN_GetAddress(vin_path[VIN_COMP_WDMA].index);
	// #else
	//	pWDMA		= VIOC_WDMA_GetAddress(vin_path[VIN_COMP_WDMA].index);
	// #endif

	//	curr_addr	= VIOC_WDMA_Get_CAddress(pWDMA);
	//	msleep(delay);

	//	nCheck		= 4;
	//	for (idxCheck = 0; idxCheck < nCheck; idxCheck++) {
	//		prev_addr = curr_addr;
	//		msleep(delay);
	//		curr_addr = VIOC_WDMA_Get_CAddress(pWDMA);

	//		if (prev_addr != curr_addr) {
	//			/* path status is okay */
	//			*status = V4L2_CAP_PATH_WORKING;
	//		} else {
	//			*status = V4L2_CAP_PATH_NOT_WORKING;
	//			logd(dev, "[%d] prev_addr: 0x%08x, curr_addr: 0x%08x\n",
	//				idxCheck, prev_addr, curr_addr);
	//		}
	//	}

	// *status = V4L2_CAP_PATH_WORKING;

	*status = V4L2_CAP_PATH_NOT_WORKING;
}

EXPORT_SYMBOL_GPL(tcc_dewarp_video_check_path_status);

int tcc_dewarp_video_s_handover(struct tcc_dewarp_stream *vstream,
				const u32 *flag)
{
	int ret = 0;

	vstream->handover_flags = *flag;

	tcc_dewarp_print_handover_flags(vstream, vstream->handover_flags);

	return ret;
}

EXPORT_SYMBOL_GPL(tcc_dewarp_video_s_handover);
MODULE_AUTHOR("Telechips");
MODULE_DESCRIPTION("Telechips Video-Input Path(V4L2-Capture) Driver - V4L2");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
