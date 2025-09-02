/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef TCC_ISP_H
#define TCC_ISP_H

#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <linux/of_reserved_mem.h>

#ifndef ON
#define ON		1
#endif

#ifndef OFF
#define OFF		0
#endif

#define TCC_ISP_DRIVER_NAME	"tcc-isp"
#define TCC_ISP_SUBDEV_NAME	TCC_ISP_DRIVER_NAME
#define TCC_ISP_FIRMWARE_NAME	"tcc-isp-fw"
#define TCC_ISP_SETTING_NAME	"tcc-isp-setting"

#define TCC_ISP_SETTING_CMD_MAX 2048U

/* Brightness*/
#define TCC_ISP_BRI_MIN		-128
#define TCC_ISP_BRI_DEF		0
#define TCC_ISP_BRI_MAX		127

#ifdef CONFIG_VIDEO_ADV_DEBUG
#define MCU_DISABLE_REQ		0x10U
#define MCU_DISABLE_CLR		0x00U
#define MCU_DISABLE_ACK		0x0FU

extern int32_t tcc_i2c7_set_trfc(uint32_t fc);
extern int32_t tcc_i2c7_get_trfc(uint32_t *fc);
#endif

#define TCC_ISP_PAD_SINK	0U
#define TCC_ISP_PAD_SRC		1U
#define TCC_ISP_PAD_NUM		2U
struct i2c_slave_control {
	/* 7bit */
	uint32_t i2c_slv_id;
	/* I2C data length */
	uint32_t i2c_slv_mode;
};

struct reg_setting {
	uint32_t reg;
	uint32_t val;
};

struct isp_tune {
	struct i2c_slave_control i2c_ctrl;
	struct reg_setting setting[TCC_ISP_SETTING_CMD_MAX];
	uint32_t isp_setting_cnt;
};

struct btset_element {
	uint32_t data_id;
	uint32_t pg;
	uint32_t addr;
};

struct isp_out_win_crop {
	uint32_t x;
	uint32_t y;
};

struct tcc_isp_state {
	struct platform_device *pdev;
	struct v4l2_subdev sd;

	struct reserved_mem *rsvd_mem_3dnr;

	struct v4l2_dv_timings dv_timings;
	struct v4l2_ctrl_handler ctrl_hdl;

	struct v4l2_async_notifier nf;

	char isp_fw_name[20];
	int fw_load;

	/* register base addr */
	void __iomem *isp_base;
	void __iomem *mem_base;
	void __iomem *cfg_base;
	void __iomem *rgbir_sync_base;
	void __iomem *scene_data_base;

	/* uart pinctrl */
	struct pinctrl *uart_pctl;

	struct media_pad pads[TCC_ISP_PAD_NUM];
	struct v4l2_mbus_framefmt fmt[TCC_ISP_PAD_NUM];

	/* demosaic(rccb, rccc, rgbir) */
	uint32_t demosaic_mode;

	uint32_t rgbir_order;

	/* output window crop pos */
	struct isp_out_win_crop out_win_crop;

	/* tune setting */
	char isp_setting_name[20];
	struct isp_tune tune_core;
	struct isp_tune tune_scene;
	int setting_load;

	uint32_t mem_share;
	int isp_bypass;

	int irq;

	uint64_t mdelay_to_out;

	struct task_struct *ati_thread;
};

#endif
