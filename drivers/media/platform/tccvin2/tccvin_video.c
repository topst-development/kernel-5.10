// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *      tccvin_video.c  --  Telechips Video-Input Path Driver
 *
 *      Copyright (C) 2005-2010
 *          Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 ******************************************************************************


 *   Modified by Telechips Inc.


 *   Modified date : 2020


 *   Description : Video handling


 *****************************************************************************/

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
#include <linux/version.h>

#include <media/v4l2-common.h>
#include <media/videobuf2-dma-contig.h>

#include <asm/unaligned.h>
#include "tccvin_common.h"
#include "tccvin_video.h"

/* ------------------------------------------------------------------------
 *  * helper macro
 *   */

#define IS_SET(value, mask) (((u32)(value) & (u32)(mask)) != 0U)

static u32 tccvin_get_data_enable_pol_by_mbus_conf(u32 p_conf)
{
	return IS_SET(p_conf, V4L2_MBUS_DATA_ACTIVE_HIGH) ?
		       (u32)DE_ACTIVE_HIGH :
		       (u32)DE_ACTIVE_LOW;
}

static u32 tccvin_get_vsync_pol_by_mbus_conf(u32 p_conf)
{
	return IS_SET(p_conf, V4L2_MBUS_VSYNC_ACTIVE_HIGH) ?
		       (u32)VS_ACTIVE_HIGH :
		       (u32)VS_ACTIVE_LOW;
}

static u32 tccvin_get_hsync_pol_by_mbus_conf(u32 p_conf)
{
	return IS_SET(p_conf, V4L2_MBUS_HSYNC_ACTIVE_HIGH) ?
		       (u32)HS_ACTIVE_HIGH :
		       (u32)HS_ACTIVE_LOW;
}

static u32 tccvin_get_pclk_pol_by_mbus_conf(u32 p_conf)
{
	return IS_SET(p_conf, V4L2_MBUS_PCLK_SAMPLE_RISING) ?
		       (u32)PCLK_ACTIVE_HIGH :
		       (u32)PCLK_ACTIVE_LOW;
}

u32 tccvin_get_embedded_sync_by_mbus_conf(enum v4l2_mbus_type p_conf)
{
	return IS_SET(p_conf, V4L2_MBUS_BT656) ? 1U : 0U;
}

bool tccvin_handover_flagged(u32 flags, u32 mask)
{
	return (IS_SET(flags, V4L2_CAP_CTRL_SKIP_ALL) || IS_SET(flags, mask));
}

void tccvin_print_handover_flags(const struct tccvin_stream *vstream, u32 flags)
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

static struct tccvin_mbus_vin_format tccvin_mbus_vin_format_list[] = {
	{ .mbus_fmt = MEDIA_BUS_FMT_UYVY8_2X8,
	  .data_format = FMT_YUV422_8BIT,
#ifdef CONFIG_ARCH_TCC750X
	  .data_order = ORDER_RBG
#else
	  .data_order = ORDER_RGB
#endif
	},
	{ .mbus_fmt = MEDIA_BUS_FMT_UYVY8_1X16,
	  .data_format = FMT_YUV422_16BIT,
#ifdef CONFIG_ARCH_TCC750X
	  .data_order = ORDER_RBG
#else
	  .data_order = ORDER_RGB
#endif
	},
	{ .mbus_fmt = MEDIA_BUS_FMT_Y8_1X8,
	  .data_format = FMT_YUV422_16BIT,
	  .data_order = ORDER_RBG },
	{ .mbus_fmt = MEDIA_BUS_FMT_Y10_1X10,
	  .data_format = FMT_YUV422_16BIT,
	  .data_order = ORDER_RBG },
	{ .mbus_fmt = MEDIA_BUS_FMT_Y12_1X12,
	  .data_format = FMT_YUV422_16BIT,
	  .data_order = ORDER_RBG },
	{ .mbus_fmt = MEDIA_BUS_FMT_RGB888_1X24,
	  .data_format = FMT_RGB444_24BIT,
	  .data_order = ORDER_RGB },
	{ .mbus_fmt = MEDIA_BUS_FMT_YUV8_1X24,
	  .data_format = FMT_RGB444_24BIT,
	  .data_order = ORDER_RGB },
};

void tccvin_get_vs_info_format_by_mbus_format(
	const struct tccvin_stream *vstream, u32 mbus_pixelcode,
	struct tccvin_vs_info *vs_fmt)
{
	const struct device *p_dev = NULL;
	const struct tccvin_mbus_vin_format *format = NULL;
	u32 idxList = 0;
	u32 nList = 0;

	p_dev = stream_to_device(vstream);

	/* default format */
	vs_fmt->data_format = FMT_YUV422_8BIT;
	vs_fmt->data_order = ORDER_RGB;

	nList = ARRAY_SIZE(tccvin_mbus_vin_format_list);
	for (idxList = 0; idxList < nList; idxList++) {
		format = &tccvin_mbus_vin_format_list[idxList];
		if (mbus_pixelcode == format->mbus_fmt) {
			vs_fmt->data_format = format->data_format;
			vs_fmt->data_order = format->data_order;
		}
	}
}

static int tccvin_parse_reserved_memory(struct tccvin_stream *vstream,
					const struct device_node *dev_node)
{
	const struct device *p_dev = NULL;
	struct device_node *mem_node = NULL;
	/* the ordor of reserved memorys depends on enum reserved_memory */
	const char *const name_list[RESERVED_MEM_MAX] = {
		[RESERVED_MEM_PGL] = "pmap_pgl",
		[RESERVED_MEM_VIQE] = "pmap_viqe",
		[RESERVED_MEM_PREV] = "pmap_prev",
		[RESERVED_MEM_LFRAME] = "pmap_lframe",
	};
	const char *name = NULL;
	u32 idxMem = 0;
	u32 base = 0;
	u32 size = 0;
	int ret = 0;

	p_dev = stream_to_device(vstream);

	for (idxMem = 0; idxMem < (u32)RESERVED_MEM_MAX; idxMem++) {
		mem_node = of_parse_phandle(dev_node, "memory-region",
					    clamp_t(s32, idxMem, 0, 255));
		name = name_list[idxMem];
		if (!IS_ERR_OR_NULL(mem_node)) {
			vstream->cif.rsvd_mem[idxMem] =
				of_reserved_mem_lookup(mem_node);
			if (!IS_ERR_OR_NULL(vstream->cif.rsvd_mem[idxMem])) {
#if defined(CONFIG_ARM64)
				base = clamp_t(
					u32,
					vstream->cif.rsvd_mem[idxMem]->base, 0,
					UINT_MAX);
				size = clamp_t(
					u32,
					vstream->cif.rsvd_mem[idxMem]->size, 0,
					UINT_MAX);
#else
				base = vstream->cif.rsvd_mem[idxMem]->base;
				size = vstream->cif.rsvd_mem[idxMem]->size;
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

static int tccvin_parse_fwnode_expand(struct tccvin_stream *vstream,
				      const struct fwnode_handle *fwnode)
{
	const struct device *p_dev = NULL;
	struct tccvin_vs_info *vs_info = NULL;
	int ret = 0;

	p_dev = stream_to_device(vstream);
	vs_info = &vstream->vs_info;

	if (vs_info != NULL) {
		struct tccvin_fwnode_property_map {
			char *name;
			u32 *val;
		};
		const struct tccvin_fwnode_property_map prop_list[] = {
			{ "data-order", &vs_info->data_order },
			{ "data-format", &vs_info->data_format },
			{ "stream-enable", &vs_info->stream_enable },
			{ "gen-field-en", &vs_info->gen_field_en },
			{ "vs-mask", &vs_info->vs_mask },
			{ "hsde-connect-en", &vs_info->hsde_connect_en },
			{ "intpl-en", &vs_info->intpl_en },
			{ "flush-vsync", &vs_info->flush_vsync },
		};
		u32 idxList = 0;
		u32 nList = 0;
		u32 v;

		nList = ARRAY_SIZE(prop_list);
		for (idxList = 0; idxList < nList; idxList++) {
			ret = fwnode_property_read_u32(
				fwnode, prop_list[idxList].name, &v);
			if (ret == 0) {
				/* update value */
				*prop_list[idxList].val = v;
			}
		}
	}

	return ret;
}

static int tccvin_parse_fwnode(struct tccvin_stream *vstream,
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

		ret_call = tccvin_parse_fwnode_expand(vstream, fwnode);
		if (ret_call != 0) {
			loge(p_dev, "tccvin_parse_fwnode_expand, ret: %d\n",
			     ret_call);
			ret = -1;
		}
	}

	return ret;
}

static bool
tccvin_video_is_framesize_supported(const struct tccvin_stream *vstream,
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

void tccvin_get_and_update_time(const struct tccvin_stream *vstream,
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

static int tccvin_video_check_subdev_status(const struct tccvin_stream *vstream)
{
	const struct device *p_dev = NULL;
	const struct tccvin_device *tdev = NULL;
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

		ret = tccvin_subdev_video_g_input_status(subdev, &status);
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

int tccvin_start_subdevs(struct tccvin_stream *vstream)
{
	const struct device *p_dev = NULL;
	struct v4l2_subdev *subdev = NULL;
	struct v4l2_subdev_format *fmt = NULL;
	struct tccvin_vs_info *vs_info = NULL;
	struct v4l2_dv_timings timings = {
		0,
	};
	const struct media_link *flink = NULL;
	u32 mbus_flags = 0;
	int ret_call = 0;
	int ret = 0;

	p_dev = stream_to_device(vstream);
	fmt = &vstream->tdev->tsubdev.fmt;
	vs_info = &vstream->vs_info;

	/* find sub-device linked to sink pad */
	ret_call = tccvin_subdev_get_src_sd(vstream->tdev, &vstream->tdev->pad,
					    &subdev);
	if (ret_call < 0) {
		/* error */
		loge(p_dev, "tccvin_subdev_get_src_sd, ret: %d\n", ret_call);
		ret = ret_call;
	} else {
		flink = container_of(vstream->tdev->vdev.entity.links.next,
				     struct media_link, list);

		fmt = &vstream->tdev->tsubdev.fmt;
		fmt->pad = flink->source->index;
		fmt->which = (u32)V4L2_SUBDEV_FORMAT_ACTIVE;
		ret_call = tccvin_subdev_pad_get_fmt(subdev, fmt);
		if (ret_call != 0) {
			/* error: tccvin_subdev_pad_get_fmt */
			logd(p_dev, "tccvin_subdev_pad_get_fmt, ret: %d\n",
			     ret_call);
		} else {
			tccvin_get_vs_info_format_by_mbus_format(
				vstream, fmt->format.code, &vstream->vs_info);
			logd(p_dev, "data_format: %u, data_order: %u\n",
			     vs_info->data_format, vs_info->data_order);
		}

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
		ret_call = tccvin_subdev_video_g_mbus_config(
			subdev, flink->source->index, &vstream->mbus_config);
#else
		ret_call = tccvin_subdev_video_g_mbus_config(
			subdev, &vstream->mbus_config);
#endif
		if (ret_call != 0) {
			/* error: tccvin_subdev_video_g_mbus_config */
			logd(p_dev,
			     "tccvin_subdev_video_g_mbus_config, ret: %d\n",
			     ret_call);
		} else {
			mbus_flags = vstream->mbus_config.flags;
			vs_info->de_low =
				tccvin_get_data_enable_pol_by_mbus_conf(
					mbus_flags);
			vs_info->vs_low =
				tccvin_get_vsync_pol_by_mbus_conf(mbus_flags);
			vs_info->hs_low =
				tccvin_get_hsync_pol_by_mbus_conf(mbus_flags);
			vs_info->pclk_polarity =
				tccvin_get_pclk_pol_by_mbus_conf(mbus_flags);
			vs_info->conv_en =
				tccvin_get_embedded_sync_by_mbus_conf(
					vstream->mbus_config.type);
			logd(p_dev,
			     "de: %u, vs: %u, hs: %u, pclk: %u, conv: %u\n",
			     vs_info->de_low, vs_info->vs_low, vs_info->hs_low,
			     vs_info->pclk_polarity, vs_info->conv_en);
		}

		ret_call = tccvin_subdev_video_g_dv_timings(subdev, &timings);
		if (ret_call != 0) {
			/* error: tccvin_subdev_video_g_dv_timings */
			logd(p_dev,
			     "tccvin_subdev_video_g_dv_timings, ret: %d\n",
			     ret_call);
		} else {
			/* size */
			vs_info->height = timings.bt.height;
			vs_info->width = timings.bt.width;
			logd(p_dev, "width: %d, height: %d\n", vs_info->height,
			     vs_info->width);

			/* interlaced */
			vs_info->interlaced = (timings.bt.interlaced ==
					       (u32)V4L2_DV_INTERLACED) ?
						      1U :
						      0U;
			logd(p_dev, "interalced: %u\n", vs_info->interlaced);
		}

		ret_call = tccvin_subdev_core_s_power(subdev, 1);
		if (ret_call != 0) {
			/* error: tccvin_subdev_video_s_power */
			logd(p_dev, "tccvin_subdev_video_s_power, ret: %d\n",
			     ret_call);
		}
		ret_call = tccvin_subdev_core_init(subdev, 1);
		if (ret_call != 0) {
			/* error: tccvin_subdev_video_init */
			logd(p_dev, "tccvin_subdev_video_init, ret: %d\n",
			     ret_call);
		}
		ret_call = tccvin_subdev_video_s_stream(subdev, 1);
		if (ret_call != 0) {
			/* error: tccvin_subdev_video_s_stream */
			logd(p_dev, "tccvin_subdev_video_s_stream, ret: %d\n",
			     ret_call);
		}

		ret_call = tccvin_video_check_subdev_status(vstream);
		if (ret_call != 0) {
			/* error: tccvin_vioc_check_subdev_status */
			logd(p_dev,
			     "tccvin_vioc_check_subdev_status, ret: %d\n",
			     ret_call);
		}
	}

	return ret;
}

int tccvin_stop_subdevs(const struct tccvin_stream *vstream)
{
	const struct device *p_dev = NULL;
	struct v4l2_subdev *subdev = NULL;
	int ret = 0;

	p_dev = stream_to_device(vstream);
	/* find sub-device linked to sink pad */
	ret = tccvin_subdev_get_src_sd(vstream->tdev, &vstream->tdev->pad,
				       &subdev);
	if (ret < 0) {
		/* error */
		loge(p_dev, "tccvin_subdev_get_src_sd, ret: %d\n", ret);
	} else {
		(void)tccvin_subdev_video_s_stream(subdev, 0);
		(void)tccvin_subdev_core_init(subdev, 0);
		(void)tccvin_subdev_core_s_power(subdev, 0);
	}

	return ret;
}

int tccvin_video_init(struct tccvin_stream *vstream,
		      struct device_node *dev_node)
{
	const struct device *p_dev = NULL;
	int ret_call = 0;
	int ret = 0;

	p_dev = stream_to_device(vstream);

	ret_call = tccvin_get_clock(vstream, dev_node);
	if (ret_call != 0) {
		loge(p_dev, "tccvin_get_clock, ret: %d\n", ret_call);
		ret = -ENODEV;
	}

	ret_call = tccvin_parse_ddibus(vstream, dev_node);
	if (ret_call != 0) {
		loge(p_dev, "tccvin_parse_ddibus, ret: %d\n", ret_call);
		ret = -ENODEV;
	}

	ret_call = tccvin_parse_reserved_memory(vstream, dev_node);
	if (ret_call != 0) {
		loge(p_dev, "tccvin_parse_reserved_memory, ret: %d\n",
		     ret_call);
		ret = -ENODEV;
	}

	ret_call = tccvin_parse_fwnode(vstream, dev_node);
	if (ret_call != 0) {
		logd(p_dev, "tccvin_parse_fwnode, ret: %d\n", ret_call);
		ret = -ENODEV;
	}

	return ret;
}

int tccvin_video_deinit(const struct tccvin_stream *vstream)
{
	const struct device *p_dev = NULL;
	int ret = 0;

	p_dev = stream_to_device(vstream);

	// hj_test check
	tccvin_put_clock(vstream);

	return ret;
}

bool tccvin_video_is_pixelformat_supported(const struct tccvin_stream *vstream,
					   u32 pixelformat)
{
	const struct device *p_dev = NULL;
	const struct tccvin_format *tformat = NULL;

	bool ret = true;

	p_dev = stream_to_device(vstream);

	tformat = tccvin_get_tccvin_format_by_pixelformat(pixelformat);
	if (tformat == NULL) {
		loge(p_dev, "pixelformat(0x%08x) is not supported\n",
		     pixelformat);
		ret = false;
	}

	return ret;
}

bool tccvin_video_is_format_supported(const struct tccvin_stream *vstream,
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

	ret_bool = tccvin_video_is_framesize_supported(vstream, pix_mp->width,
						       pix_mp->height);
	if (!ret_bool) {
		loge(p_dev, "tccvin_video_is_framesize_supported\n");
		ret = false;
	}

	ret_bool = tccvin_video_is_pixelformat_supported(vstream,
							 pix_mp->pixelformat);
	if (!ret_bool) {
		loge(p_dev, "tccvin_video_is_pixelformat_supported\n");
		ret = false;
	}

	return ret;
}

int tccvin_video_declare_coherent_dma_memory(const struct tccvin_stream *vstream)
{
	struct device *p_dev = NULL;
	int ret = 0;

	p_dev = stream_to_device(vstream);

	ret = of_reserved_mem_device_init_by_idx(p_dev, p_dev->of_node, 2);
	if (ret < 0) {
		loge(p_dev, "Failed to init per-device memory: %d\n", ret);
	}

	return ret;
}

int tccvin_video_get_lastframe_addrs(const struct tccvin_stream *vstream,
				     u32 *addrs)
{
	const struct device *p_dev = NULL;
	int ret = 0;

	p_dev = stream_to_device(vstream);

#if defined(CONFIG_ARM64)
	*addrs = clamp_t(u32, vstream->cif.rsvd_mem[RESERVED_MEM_LFRAME]->base,
			 0, UINT_MAX);
#else
	*addrs = vstream->cif.rsvd_mem[RESERVED_MEM_LFRAME]->base;
#endif //defined(CONFIG_ARM64)
	logi(p_dev, "addrs of lastframe is 0x%08x\n", *addrs);

	return ret;
}

int tccvin_video_s_handover(struct tccvin_stream *vstream, const u32 *flag)
{
	int ret = 0;

	vstream->handover_flags = *flag;

	tccvin_print_handover_flags(vstream, vstream->handover_flags);

	return ret;
}

int tccvin_video_s_lut(struct tccvin_stream *vstream,
		       const struct vin_lut *plut)
{
	const struct device *p_dev = NULL;
	int ret = 0;

	p_dev = stream_to_device(vstream);

	if (!IS_ERR_OR_NULL(plut)) {
		/* update look-up table */
		(void)memcpy(&vstream->cif.vin_internal_lut, plut,
			     sizeof(*plut));
	} else {
		loge(p_dev, "look-up table is null\n");
		ret = -1;
	}

	return ret;
}

void tccvin_get_dma_addrs(const struct tccvin_stream *vstream,
			  struct vb2_buffer *vb, u32 addrs[])
{
	const struct device *p_dev = NULL;
	dma_addr_t dma_addrs[MAX_PLANES];
	u32 idxpln = 0;

	p_dev = stream_to_device(vstream);

	switch (vb->memory) {
	case (u32)VB2_MEMORY_MMAP:
		for (idxpln = 0; idxpln < vb->num_planes; idxpln++) {
			dma_addrs[idxpln] =
				vb2_dma_contig_plane_dma_addr(vb, idxpln);
			if (IS_ENABLED(CONFIG_ARM64)) {
				addrs[idxpln] = clamp_t(u32, dma_addrs[idxpln],
							0, UINT_MAX);
			} else {
				addrs[idxpln] = (u32)dma_addrs[idxpln];
			}
		}
		break;

	default:
		loge(p_dev, "memory(0x%08x) is not supported\n", vb->memory);
		break;
	}
}

void tccvin_print_dma_addrs(const struct tccvin_stream *vstream,
			    const struct vb2_buffer *vb, const u32 addrs[])
{
	const struct device *p_dev = NULL;
	u32 idxpln = 0;

	p_dev = stream_to_device(vstream);

	/* misra_c_2012_rule_2_7_violation */
	(void)addrs;

	switch (vb->memory) {
	case (u32)VB2_MEMORY_MMAP:
		for (idxpln = 0; idxpln < vb->num_planes; idxpln++) {
			/* dma addr */
			logd(p_dev, "planes[%u]: 0x%08x\n", idxpln,
			     addrs[idxpln]);
		}
		break;

	default:
		loge(p_dev, "memory(0x%08x) is not supported\n", vb->memory);
		break;
	}
}
