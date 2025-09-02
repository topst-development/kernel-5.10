// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <media/videobuf2-dma-contig.h>

#include "../tcc-idi2axi-helper.h"
#include "tcc-idi2axi-reg.h"
#include "tcc-idi2axi-dev.h"

static inline uint32_t tcc_idi2axi_dev_readl(struct device *dev,
					     const void __iomem *base,
					     uint32_t offset)
{
	uint32_t val;

	val = __raw_readl(base + offset);
	logd(dev, "read offset: 0x%x, val: 0x%x\n", offset, val);

	return val;
}

static inline void tcc_idi2axi_dev_writel(struct device *dev, uint32_t val,
					  void __iomem *base, uint32_t offset)
{
	logd(dev, "write offset: 0x%x, val: 0x%x\n", offset, val);
	__raw_writel(val, base + offset);
}

void tcc_idi2axi_dev_try_fmt(const struct tcc_idi2axi_state *state,
			     struct v4l2_format *f)
{
	f->fmt.pix.bytesperline = f->fmt.pix.width * 4U;
	f->fmt.pix.sizeimage =
		f->fmt.pix.bytesperline * f->fmt.pix.height * state->sets.sscnt;
}

static int tcc_idi2axi_dev_dt_res(struct tcc_idi2axi_state *state)
{
	const struct resource *res = NULL;
	int ret = 0;

	/* Get IDI2AXI base address */
	res = platform_get_resource_byname(state->pdev, IORESOURCE_MEM,
					   "idi2axi");
	state->idi2axi_base = devm_ioremap_resource(&state->pdev->dev, res);
	if (IS_ERR((const void *)state->idi2axi_base)) {
		/* error */
		ret = (int)PTR_ERR((const void *)state->idi2axi_base);
		loge(&(state->pdev->dev),
		     "Invalid IDI2AXI base addr(%d)\n", ret);
	}

	/* get interrupt number */
	if (ret == 0) {
		ret = platform_get_irq_byname(state->pdev, "idi2axi");
		if (ret <= 0) {
			/* error */
			loge(&(state->pdev->dev), "Invalid IRQ(%d)\n", ret);
		} else {
			/* okay */
			state->intr.num = (uint32_t)ret;
			ret = 0;
		}
	}

	return ret;
}

static int tcc_idi2axi_dev_dt_props(struct tcc_idi2axi_state *state)
{
	uint32_t idx = 0U;
	int ret = 0;

	const struct tcc_idi2axi_prop props[] = {
		{
			.name = "sscnt",
			.out = &(state->sets.sscnt),
			.is_def = (bool)true,
			.def = 1U,
		},
		{
			.name = "mode",
			.out = &(state->sets.i2xm),
			.is_def = (bool)true,
			.def = 0U,
		},
		{
		},
	};

	for (idx = 0; props[idx].name != NULL; idx++) {
		ret = of_property_read_u32(state->pdev->dev.of_node,
					   props[idx].name, props[idx].out);
		if (ret < 0) {
			if (props[idx].is_def) {
				/* default value */
				*(props[idx].out) = props[idx].def;
				ret = 0;
			} else {
				/* error */
				loge(&(state->pdev->dev),
				     "invalid %s property(%d)\n",
				     props[idx].name, ret);
				break;
			}
		}
	}

	return ret;
}

int tcc_idi2axi_dev_parse_dt(struct tcc_idi2axi_state *state)
{
	int ret = 0;

	/* Parsing used resource */
	ret = tcc_idi2axi_dev_dt_res(state);
	if (ret < 0) {
		/* error */
		loge(&(state->pdev->dev),
		     "tcc_idi2axi_dev_dt_res returned %d", ret);
	}

	if (ret == 0) {
		ret = tcc_idi2axi_dev_dt_props(state);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
			     "tcc_idi2axi_dev_dt_props returned %d", ret);
		}
	}

	return ret;
}

static inline struct tcc_idi2axi_buffer *
tcc_idi2axi_dev_get_queued_buffer(struct tcc_idi2axi_state *state)
{
	struct tcc_idi2axi_buffer *buf;

	buf = list_first_entry_or_null(&(state->buf_list),
				       struct tcc_idi2axi_buffer, list);

	if (buf == NULL) {
		/* error */
		logd(&(state->pdev->dev), "There isn't queued buffer\n");
	} else {
		/* dequeue buffer */
		list_del(&(buf->list));
	}

	return buf;
}

static inline void tcc_idi2axi_dev_update(struct tcc_idi2axi_state *state)
{
	uint32_t val = 0U;

	val = TCC_IDI2AXI_CTL02_I2XUPD_MASK |
	      TCC_IDI2AXI_CTL02_I2XSU_MASK;
	tcc_idi2axi_dev_writel(&(state->pdev->dev), val, state->idi2axi_base,
			       TCC_IDI2AXI_CTL02);
}

static inline void tcc_idi2axi_dev_set_mode(struct tcc_idi2axi_state *state)
{
	const char *i2xm2str[] = { "Camera Mode", "LiDAR Mode" };
	uint32_t val = 0U;

	if (state->sets.i2xm > 1U) {
		/* error */
		loge(&(state->pdev->dev),
		     "Wrong i2xm(%d). Fixed to the camera mode\n",
		     state->sets.i2xm);
		state->sets.i2xm = 0U;
	}

	logi(&(state->pdev->dev), "SSYNC counter: %d, Mode: %s\n",
	     state->sets.sscnt, i2xm2str[state->sets.i2xm]);

	val = ((state->sets.sscnt << TCC_IDI2AXI_SPADCTL01_SSCNT_SHIFT) |
	       (state->sets.i2xm << TCC_IDI2AXI_SPADCTL01_I2XM_SHIFT));
	tcc_idi2axi_dev_writel(&(state->pdev->dev), val, state->idi2axi_base,
			       TCC_IDI2AXI_SPADCTL01);
}

static inline void tcc_idi2axi_dev_set_ctl(struct tcc_idi2axi_state *state,
					   uint32_t i2xfpol, uint32_t i2xvpol,
					   uint32_t i2xbm)
{
	const char *i2xfpol2str[] = { "Active high(sync to falling edge)",
				      "Active low(sync to rising edge)" };
	const char *i2xvpol2str[] = { "Active high(sync to falling edge)",
				      "Active low(sync to rising edge)" };
	const char *i2xbm2str[] = { "NONE",
				    "Single buffer mode",
				    "Dual buffer mode",
				    "Tripple buffer mode" };
	uint32_t val = 0U;

	if (i2xfpol > 1U) {
		loge(&(state->pdev->dev),
		     "Wrong i2xfpol(%d). Fixed to the %s\n",
		     i2xfpol, i2xfpol2str[0U]);
		i2xfpol = 0U;
	}

	if (i2xvpol > 1U) {
		loge(&(state->pdev->dev),
		     "Wrong i2xvpol(%d). Fixed to the %s\n",
		     i2xvpol, i2xvpol2str[1U]);
		i2xvpol = 1U;
	}

	if ((i2xbm == 0U) || (i2xbm > 3U)) {
		loge(&(state->pdev->dev),
		     "Wrong i2xbm(%d). Fixed to the %s\n",
		     i2xbm, i2xbm2str[1U]);
		i2xbm = 1U;
	}

	logi(&(state->pdev->dev),
	     "FSYNC: %s, VSYNC: %s, Buffer mode: %s\n",
	     i2xfpol2str[i2xfpol], i2xvpol2str[i2xvpol], i2xbm2str[i2xbm]);

	val = ((i2xfpol << TCC_IDI2AXI_CTL01_I2XFPOL_SHIFT) |
	       (i2xvpol << TCC_IDI2AXI_CTL01_I2XVPOL_SHIFT) |
	       (1U << TCC_IDI2AXI_CTL01_I2XECCEN_SHIFT) |
	       (i2xbm << TCC_IDI2AXI_CTL01_I2XBM_SHIFT));
	tcc_idi2axi_dev_writel(&(state->pdev->dev), val, state->idi2axi_base,
			       TCC_IDI2AXI_CTL01);
}

static inline void tcc_idi2axi_dev_set_vc(struct tcc_idi2axi_state *state,
					  uint32_t ch, uint32_t vc)
{
	const uint32_t shift[] = { TCC_IDI2AXI_VC01_CH00_VC_SHIFT,
				   TCC_IDI2AXI_VC01_CH01_VC_SHIFT,
				   TCC_IDI2AXI_VC01_CH02_VC_SHIFT,
				   TCC_IDI2AXI_VC01_CH03_VC_SHIFT };
	const uint32_t mask[] = { TCC_IDI2AXI_VC01_CH00_VC_MASK,
				  TCC_IDI2AXI_VC01_CH01_VC_MASK,
				  TCC_IDI2AXI_VC01_CH02_VC_MASK,
				  TCC_IDI2AXI_VC01_CH03_VC_MASK };
	uint32_t val = 0U;

	if (ch > 3U) {
		/* error */
		loge(&(state->pdev->dev),
		     "Wrong CH(%d). Fixed to 0\n", ch);
		ch = 0U;
	}

	if (vc > 3U) {
		/* error */
		loge(&(state->pdev->dev),
		     "Wrong VC(%d). Fixed to 0\n", vc);
		vc = 0U;
	}

	logi(&(state->pdev->dev), "CH%d: VC is %d\n", ch, vc);

	val = tcc_idi2axi_dev_readl(&(state->pdev->dev), state->idi2axi_base,
				    TCC_IDI2AXI_CTL02);
	val &= ~(mask[ch]);
	val |= (vc << shift[ch]);
	tcc_idi2axi_dev_writel(&(state->pdev->dev), val, state->idi2axi_base,
			       TCC_IDI2AXI_VC01);
}

static inline void tcc_idi2axi_dev_set_dt(struct tcc_idi2axi_state *state,
					  uint32_t ch, uint32_t dt_en,
					  const uint32_t dt[8U])
{
	const uint32_t dt_en_shift[] = { TCC_IDI2AXI_DTEN01_CH00_DT_EN_SHIFT,
					 TCC_IDI2AXI_DTEN01_CH01_DT_EN_SHIFT,
					 TCC_IDI2AXI_DTEN01_CH02_DT_EN_SHIFT,
					 TCC_IDI2AXI_DTEN01_CH03_DT_EN_SHIFT };
	const uint32_t dt_en_mask[] = { TCC_IDI2AXI_DTEN01_CH00_DT_EN_MASK,
					TCC_IDI2AXI_DTEN01_CH01_DT_EN_MASK,
					TCC_IDI2AXI_DTEN01_CH02_DT_EN_MASK,
					TCC_IDI2AXI_DTEN01_CH03_DT_EN_MASK };
	const uint32_t dtxx_offset[] = { TCC_IDI2AXI_DT01,
					 TCC_IDI2AXI_DT03,
					 TCC_IDI2AXI_DT05,
					 TCC_IDI2AXI_DT07 };
	uint32_t val = 0U, idx = 0U, dt_offset = 0U, dt_shift = 0U;

	if (ch > 3U) {
		/* error */
		loge(&(state->pdev->dev),
		     "Wrong CH(%d). Fixed to 0\n", ch);
		ch = 0U;
	}

	if (dt_en > 0xFFU) {
		/* error */
		loge(&(state->pdev->dev),
		     "Wrong DT_EN(%d). Fixed to 1\n", dt_en);
		dt_en = 1U;
	}

	logi(&(state->pdev->dev), "CH%d: DT_EN is 0x%x\n", ch, dt_en);

	val = tcc_idi2axi_dev_readl(&(state->pdev->dev), state->idi2axi_base,
				    TCC_IDI2AXI_DTEN01);
	val &= ~(dt_en_mask[ch]);
	val |= (dt_en << dt_en_shift[ch]);
	tcc_idi2axi_dev_writel(&(state->pdev->dev), val, state->idi2axi_base,
			       TCC_IDI2AXI_DTEN01);

	dt_offset = dtxx_offset[ch];
	/* DT 0 ~ 3 */
	for (idx = 0U; idx < 4U; idx++) {
		if ((dt_en & (1U << idx)) != 0U) {
			dt_shift = (8U * idx);

			logi(&(state->pdev->dev),
			     "DT%d is 0x%x\n", idx, dt[idx]);
			val = tcc_idi2axi_dev_readl(&(state->pdev->dev),
						    state->idi2axi_base,
						    dt_offset);
			val &= ~(TCC_IDI2AXI_DT01_CH00_DT_00_MASK << dt_shift);
			val |= (dt[idx] << dt_shift);
			tcc_idi2axi_dev_writel(&(state->pdev->dev), val,
					       state->idi2axi_base, dt_offset);
		}
	}

	dt_offset = dtxx_offset[ch] + (TCC_IDI2AXI_DT02 - TCC_IDI2AXI_DT01);
	/* DT 4 ~ 7 */
	for (idx = 4U; idx < 8U; idx++) {
		if ((dt_en & (1U << idx)) != 0U) {
			dt_shift = (8U * (idx - 4U));

			logi(&(state->pdev->dev),
			     "DT%d is 0x%x\n", idx, dt[idx]);
			val = tcc_idi2axi_dev_readl(&(state->pdev->dev),
						    state->idi2axi_base,
						    dt_offset);
			val &= ~(TCC_IDI2AXI_DT01_CH00_DT_00_MASK << dt_shift);
			val |= (dt[idx] << dt_shift);
			tcc_idi2axi_dev_writel(&(state->pdev->dev), val,
					       state->idi2axi_base, dt_offset);
		}
	}
}

static inline void tcc_idi2axi_dev_set_dpm(struct tcc_idi2axi_state *state,
					   uint32_t ch, uint32_t dt_en,
					   uint32_t dpm[8U])
{
	const uint32_t dpm_shift[] = { TCC_IDI2AXI_DPM01_CH00_DT_00_DPM_SHIFT,
				       TCC_IDI2AXI_DPM01_CH00_DT_01_DPM_SHIFT,
				       TCC_IDI2AXI_DPM01_CH00_DT_02_DPM_SHIFT,
				       TCC_IDI2AXI_DPM01_CH00_DT_03_DPM_SHIFT,
				       TCC_IDI2AXI_DPM01_CH00_DT_04_DPM_SHIFT,
				       TCC_IDI2AXI_DPM01_CH00_DT_05_DPM_SHIFT,
				       TCC_IDI2AXI_DPM01_CH00_DT_06_DPM_SHIFT,
				       TCC_IDI2AXI_DPM01_CH00_DT_07_DPM_SHIFT };
	const uint32_t dpm_mask[] = { TCC_IDI2AXI_DPM01_CH00_DT_00_DPM_MASK,
				      TCC_IDI2AXI_DPM01_CH00_DT_01_DPM_MASK,
				      TCC_IDI2AXI_DPM01_CH00_DT_02_DPM_MASK,
				      TCC_IDI2AXI_DPM01_CH00_DT_03_DPM_MASK,
				      TCC_IDI2AXI_DPM01_CH00_DT_04_DPM_MASK,
				      TCC_IDI2AXI_DPM01_CH00_DT_05_DPM_MASK,
				      TCC_IDI2AXI_DPM01_CH00_DT_06_DPM_MASK,
				      TCC_IDI2AXI_DPM01_CH00_DT_07_DPM_MASK };
	const uint32_t dpmxx_offset[] = { TCC_IDI2AXI_DPM01,
					  TCC_IDI2AXI_DPM02,
					  TCC_IDI2AXI_DPM03,
					  TCC_IDI2AXI_DPM04 };
	uint32_t val = 0U, idx = 0U;

	if (ch > 3U) {
		/* error */
		loge(&(state->pdev->dev),
		     "Wrong CH(%d). Fixed to 0\n", ch);
		ch = 0U;
	}

	if (dt_en > 0xFFU) {
		/* error */
		loge(&(state->pdev->dev),
		     "Wrong DT_EN(%d). Fixed to 1\n", dt_en);
		dt_en = 1U;
	}

	for (idx = 0U; idx < 8; idx++) {
		if ((dt_en & (1U << idx)) != 0U) {
			logi(&(state->pdev->dev),
			     "CH%d DT%d DPM is 0x%x\n", ch, idx, dpm[idx]);

			val = tcc_idi2axi_dev_readl(&(state->pdev->dev),
						    state->idi2axi_base,
						    dpmxx_offset[ch]);
			val &= ~(dpm_mask[idx]);
			val |= (dpm[idx] << dpm_shift[idx]);
			tcc_idi2axi_dev_writel(&(state->pdev->dev), val,
					       state->idi2axi_base,
					       dpmxx_offset[ch]);
		}
	}
}

static inline void tcc_idi2axi_dev_set_bpm(struct tcc_idi2axi_state *state,
					   uint32_t ch, uint32_t dt_en,
					   uint32_t bpm[8U])
{
	const uint32_t bpm_shift[] = { TCC_IDI2AXI_BPM01_CH00_DT_00_BPM_SHIFT,
				       TCC_IDI2AXI_BPM01_CH00_DT_01_BPM_SHIFT,
				       TCC_IDI2AXI_BPM01_CH00_DT_02_BPM_SHIFT,
				       TCC_IDI2AXI_BPM01_CH00_DT_03_BPM_SHIFT,
				       TCC_IDI2AXI_BPM01_CH00_DT_04_BPM_SHIFT,
				       TCC_IDI2AXI_BPM01_CH00_DT_05_BPM_SHIFT,
				       TCC_IDI2AXI_BPM01_CH00_DT_06_BPM_SHIFT,
				       TCC_IDI2AXI_BPM01_CH00_DT_07_BPM_SHIFT };
	const uint32_t bpm_mask[] = { TCC_IDI2AXI_BPM01_CH00_DT_00_BPM_MASK,
				      TCC_IDI2AXI_BPM01_CH00_DT_01_BPM_MASK,
				      TCC_IDI2AXI_BPM01_CH00_DT_02_BPM_MASK,
				      TCC_IDI2AXI_BPM01_CH00_DT_03_BPM_MASK,
				      TCC_IDI2AXI_BPM01_CH00_DT_04_BPM_MASK,
				      TCC_IDI2AXI_BPM01_CH00_DT_05_BPM_MASK,
				      TCC_IDI2AXI_BPM01_CH00_DT_06_BPM_MASK,
				      TCC_IDI2AXI_BPM01_CH00_DT_07_BPM_MASK };
	const uint32_t bpmxx_offset[] = { TCC_IDI2AXI_BPM01,
					  TCC_IDI2AXI_BPM02,
					  TCC_IDI2AXI_BPM03,
					  TCC_IDI2AXI_BPM04 };
	uint32_t val = 0U, idx = 0U;

	if (ch > 3U) {
		/* error */
		loge(&(state->pdev->dev),
		     "Wrong CH(%d). Fixed to 0\n", ch);
		ch = 0U;
	}

	if (dt_en > 0xFFU) {
		/* error */
		loge(&(state->pdev->dev),
		     "Wrong DT_EN(%d). Fixed to 1\n", dt_en);
		dt_en = 1U;
	}

	for (idx = 0U; idx < 8; idx++) {
		if ((dt_en & (1U << idx)) != 0U) {
			logi(&(state->pdev->dev),
			     "CH%d DT%d BPM is 0x%x\n", ch, idx, bpm[idx]);

			val = tcc_idi2axi_dev_readl(&(state->pdev->dev),
						    state->idi2axi_base,
						    bpmxx_offset[ch]);
			val &= ~(bpm_mask[idx]);
			val |= (bpm[idx] << bpm_shift[idx]);
			tcc_idi2axi_dev_writel(&(state->pdev->dev), val,
					       state->idi2axi_base,
					       bpmxx_offset[ch]);
		}
	}
}

static inline void tcc_idi2axi_dev_set_height(struct tcc_idi2axi_state *state,
					      uint32_t ch, uint32_t dt_en,
					      uint32_t height[8U])
{
	const uint32_t dt_hxx_offset[] = { TCC_IDI2AXI_DT_H01,
					   TCC_IDI2AXI_DT_H05,
					   TCC_IDI2AXI_DT_H09,
					   TCC_IDI2AXI_DT_H13 };
	uint32_t val = 0U, idx = 0U, dt_h_offset = 0U;

	if (ch > 3U) {
		/* error */
		loge(&(state->pdev->dev),
		     "Wrong CH(%d). Fixed to 0\n", ch);
		ch = 0U;
	}

	if (dt_en > 0xFFU) {
		/* error */
		loge(&(state->pdev->dev),
		     "Wrong DT_EN(%d). Fixed to 1\n", dt_en);
		dt_en = 1U;
	}

	for (idx = 0U; idx < 8; idx++) {
		if ((dt_en & (1U << idx)) != 0U) {
			logi(&(state->pdev->dev),
			     "CH%d DT%d HEIGHT is %d\n", ch, idx, height[idx]);

			dt_h_offset = dt_hxx_offset[ch] + (0x4U * (idx / 2));
			val = tcc_idi2axi_dev_readl(&(state->pdev->dev),
						    state->idi2axi_base,
						    dt_h_offset);
			val &= ~(TCC_IDI2AXI_DT_H01_CH00_DT_00_HEIGHT_MASK <<
				 (16U * (idx % 2U)));
			val |= (height[idx] << (16U * (idx % 2U)));
			tcc_idi2axi_dev_writel(&(state->pdev->dev), val,
					       state->idi2axi_base,
					       dt_h_offset);
		}
	}
}

static inline void tcc_idi2axi_dev_clear_eof(struct tcc_idi2axi_state *state)
{
	uint32_t val = 0U;

	val = tcc_idi2axi_dev_readl(&(state->pdev->dev), state->idi2axi_base,
				    TCC_IDI2AXI_CTL02);
	val |= TCC_IDI2AXI_CTL02_I2XEC_MASK;
	tcc_idi2axi_dev_writel(&(state->pdev->dev), val, state->idi2axi_base,
			       TCC_IDI2AXI_CTL02);

	val = tcc_idi2axi_dev_readl(&(state->pdev->dev),
				    state->idi2axi_base, TCC_IDI2AXI_CTL02);
	val &= ~TCC_IDI2AXI_CTL02_I2XEC_MASK;
	tcc_idi2axi_dev_writel(&(state->pdev->dev),
			   val, state->idi2axi_base, TCC_IDI2AXI_CTL02);
}

static inline void tcc_idi2axi_dev_set_buf_addr(struct tcc_idi2axi_state *state,
						uint32_t ch, uint32_t addr)
{
	uint32_t bf01_xx[] = { TCC_IDI2AXI_DT_BF01_A01,
			       TCC_IDI2AXI_DT_BF01_A09,
			       TCC_IDI2AXI_DT_BF01_A17,
			       TCC_IDI2AXI_DT_BF01_A25 };

	if (ch > 3U) {
		/* error */
		loge(&(state->pdev->dev),
		     "Wrong CH(%d). Fixed to 0\n", ch);
		ch = 0U;
	}

	tcc_idi2axi_dev_writel(&(state->pdev->dev), addr, state->idi2axi_base,
			       bf01_xx[ch]);
}

static inline void tcc_idi2axi_dev_set_stride(struct tcc_idi2axi_state *state,
					      uint32_t ch, uint32_t stride)
{
	uint32_t bf01_xx[] = { TCC_IDI2AXI_DT_BF01_S01,
			       TCC_IDI2AXI_DT_BF01_S09,
			       TCC_IDI2AXI_DT_BF01_S17,
			       TCC_IDI2AXI_DT_BF01_S25 };

	if (ch > 3U) {
		/* error */
		loge(&(state->pdev->dev),
		     "Wrong CH(%d). Fixed to 0\n", ch);
		ch = 0U;
	}

	logi(&(state->pdev->dev), "CH%d stride is %d bytes\n", ch, stride);

	tcc_idi2axi_dev_writel(&(state->pdev->dev), stride, state->idi2axi_base,
			       bf01_xx[ch]);
}

static inline void tcc_idi2axi_dev_set_wdmactl(struct tcc_idi2axi_state *state,
					       uint32_t mo, uint32_t mb)
{
	uint32_t val = 0U;

	logi(&(state->pdev->dev), "MO: 0x%x, MB: 0x%x\n", mo, mb);

	val = ((mo << TCC_IDI2AXI_WDMACTL01_WMO_SHIFT) |
	       (mb << TCC_IDI2AXI_WDMACTL01_WMB_SHIFT));
	tcc_idi2axi_dev_writel(&(state->pdev->dev), val, state->idi2axi_base,
			       TCC_IDI2AXI_WDMACTL01);
}

static void tcc_idi2axi_dev_enable(struct tcc_idi2axi_state *state)
{
	uint32_t val = 0U;

	tcc_idi2axi_dev_update(state);

	val = tcc_idi2axi_dev_readl(&(state->pdev->dev), state->idi2axi_base,
				    TCC_IDI2AXI_SWRST);
	val &= (~TCC_IDI2AXI_SWRST_I2XID_MASK & ~TCC_IDI2AXI_SWRST_I2XSR_MASK);
	tcc_idi2axi_dev_writel(&(state->pdev->dev), val, state->idi2axi_base,
			       TCC_IDI2AXI_SWRST);
}

void tcc_idi2axi_dev_disable(struct tcc_idi2axi_state *state)
{
	uint32_t val = 0U;
	uint32_t count = 0U;

	/* flush AXI commands (MO) */
	val = TCC_IDI2AXI_SWRST_I2XID_MASK;
	tcc_idi2axi_dev_writel(&(state->pdev->dev), val, state->idi2axi_base,
			       TCC_IDI2AXI_SWRST);

	val = tcc_idi2axi_dev_readl(&(state->pdev->dev), state->idi2axi_base,
				    TCC_IDI2AXI_SWRST);
	while ((val & TCC_IDI2AXI_SWRST_I2XIDS_MASK) == 0U) {
		if (count > 100U) {
			loge(&(state->pdev->dev),
			     "AXI commands(MO) are not processed.\n");
			break;
		}
		usleep_range(1U, 2U);
		count++;
	}

	/* interrupt mask */
	val = (1U << TCC_IDI2AXI_DTEN01_CH00_DT_EN_SHIFT);
	tcc_idi2axi_dev_writel(&(state->pdev->dev), val, state->idi2axi_base,
			       TCC_IDI2AXI_DTEN01);
	val = (0x3F3F3F3FU << TCC_IDI2AXI_DT01_CH00_DT_00_SHIFT);
	tcc_idi2axi_dev_writel(&(state->pdev->dev), val, state->idi2axi_base,
			       TCC_IDI2AXI_DT01);
	tcc_idi2axi_dev_writel(&(state->pdev->dev), 1U, state->idi2axi_base,
			       TCC_IDI2AXI_DT_H01);

	tcc_idi2axi_dev_update(state);

	/* IP disable and SW reset*/
	val = (TCC_IDI2AXI_SWRST_I2XID_MASK | TCC_IDI2AXI_SWRST_I2XSR_MASK);
	tcc_idi2axi_dev_writel(&(state->pdev->dev), val, state->idi2axi_base,
			       TCC_IDI2AXI_SWRST);
}

static inline void
tcc_idi2axi_dev_handle_done_buf(struct tcc_idi2axi_state *state)
{
	state->buf_set_dma->vb2.vb2_buf.timestamp = ktime_get_ns();
	vb2_set_plane_payload(&(state->buf_set_dma->vb2.vb2_buf), 0U,
			      state->format.sizeimage);
	vb2_buffer_done(&(state->buf_set_dma->vb2.vb2_buf), VB2_BUF_STATE_DONE);
}

static inline int tcc_idi2axi_dev_change_buf(struct tcc_idi2axi_state *state,
					     struct tcc_idi2axi_buffer *buf)
{
	uint32_t addr = 0U;
	int ret = 0U;

	state->buf_set_dma = buf;
	addr = vb2_dma_contig_plane_dma_addr(&(state->buf_set_dma->vb2.vb2_buf),
					     0U);
	if (addr == 0U) {
		/* error */
		loge(&(state->pdev->dev),
		     "vb2_dma_contig_plane_dma_addr retruned null\n");
		ret = -EFAULT;
	} else {
		logd(&(state->pdev->dev), "change buffer to 0x%x\n", addr);
		tcc_idi2axi_dev_set_buf_addr(state, 0U, addr);
		tcc_idi2axi_dev_update(state);
	}

	return ret;
}

irqreturn_t tcc_idi2axi_dev_isr(int irq, void *client_data) {
	struct tcc_idi2axi_state *state =
		(struct tcc_idi2axi_state *)client_data;
	struct tcc_idi2axi_buffer *buf;
	irqreturn_t ret = IRQ_NONE;

	tcc_idi2axi_dev_clear_eof(state);

	if (vb2_is_busy(&(state->queue))) {
		spin_lock(&(state->qlock));

		buf = tcc_idi2axi_dev_get_queued_buffer(state);
		if (buf == NULL) {
			/* There is not buffer to change */
			logd(&(state->pdev->dev),
			     "tcc_idi2axi_dev_get_queued_buffer retruned null\n");
		}

		spin_unlock(&(state->qlock));

		if (buf != NULL) {
			tcc_idi2axi_dev_handle_done_buf(state);

			ret = tcc_idi2axi_dev_change_buf(state, buf);
			if (ret < 0) {
				/* error */
				loge(&(state->pdev->dev),
				     "tcc_idi2axi_chang_buf returned %d\n",
				     ret);
			}
		}
	}

	ret = IRQ_HANDLED;

	return ret;
}

int tcc_idi2axi_dev_streamon(struct tcc_idi2axi_state *state)
{
	struct tcc_idi2axi_buffer *buf;

	uint32_t i2xfpol = 1U;
	uint32_t i2xvpol = 1U;
	uint32_t i2xbm = 1U;

	uint32_t ch00_vc = 0U;
	uint32_t ch00_dt_en = 1U;
	uint32_t ch00_dt[8U] =  { 0x2CU, };
	uint32_t ch00_dpm[8U] = { 0x3U, };
	uint32_t ch00_bpm[8U] = { 0x3U, };
	uint32_t ch00_height[8U] = { state->format.height * state->sets.sscnt };
	uint32_t ch00_dt_00_bf01_addr = 0U;
	uint32_t ch00_dt_00_bf01_strd = state->format.bytesperline;

	int ret = 0;

	/* IDI2AXI mode */
	tcc_idi2axi_dev_set_mode(state);

	/* IDI2AXI Control */
	tcc_idi2axi_dev_set_ctl(state, i2xfpol, i2xvpol, i2xbm);

	/* VC filter */
	tcc_idi2axi_dev_set_vc(state, 0U, ch00_vc);

	/* Data type filter */
	tcc_idi2axi_dev_set_dt(state, 0U, ch00_dt_en, ch00_dt);

	/* Data Packet Mode */
	tcc_idi2axi_dev_set_dpm(state, 0U, ch00_dt_en, ch00_dpm);

	/* Bus Packet Mode */
	tcc_idi2axi_dev_set_bpm(state, 0U, ch00_dt_en, ch00_bpm);

	/* Height of Data type */
	tcc_idi2axi_dev_set_height(state, 0U, ch00_dt_en, ch00_height);

	/* Base buffer address */
	buf = tcc_idi2axi_dev_get_queued_buffer(state);
	if (buf == NULL) {
		/* There is not buffer to change */
		loge(&(state->pdev->dev),
		     "tcc_idi2axi_dev_get_queued_buffer retruned null\n");
	} else {
		ch00_dt_00_bf01_addr =
			vb2_dma_contig_plane_dma_addr(&(buf->vb2.vb2_buf), 0U);
		logd(&(state->pdev->dev),
		     "set buffer to 0x%x\n", ch00_dt_00_bf01_addr);
		tcc_idi2axi_dev_set_buf_addr(state, 0U, ch00_dt_00_bf01_addr);
		state->buf_set_dma = buf;
	}

	/* Stride */
	tcc_idi2axi_dev_set_stride(state, 0U, ch00_dt_00_bf01_strd);

	/* WDMACTL*/
	tcc_idi2axi_dev_set_wdmactl(state, 0x1FU, 0xFU);

	/* Update and IP enable */
	tcc_idi2axi_dev_enable(state);

	return ret;
}

void tcc_idi2axi_dev_streamoff(struct tcc_idi2axi_state *state)
{
	/* TODO:
	 * set IDI2AXI registers
	 */
	logw(&(state->pdev->dev), " NOT IMPLEMENTATION\n");
	tcc_idi2axi_dev_disable(state);
}
