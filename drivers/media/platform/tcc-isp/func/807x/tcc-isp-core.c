// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include "../../tcc-isp.h"
#include "../../tcc-isp-helper.h"
#include "tcc-isp-reg.h"

static uint32_t tcc_isp_core_pixel_order(const struct tcc_isp_state *state,
					 uint32_t code)
{
	uint32_t ret = 0;

	switch (code) {
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SBGGR14_1X14:
		ret = IMG_IN_ORDER_CTL_ORDER_B_FIRST;
		break;
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGBRG14_1X14:
		ret = IMG_IN_ORDER_CTL_ORDER_GB_FIRST;
		break;
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SGRBG14_1X14:
		ret = IMG_IN_ORDER_CTL_ORDER_GR_FIRST;
		break;
	case MEDIA_BUS_FMT_SRGGB8_1X8:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
	case MEDIA_BUS_FMT_SRGGB14_1X14:
		ret = IMG_IN_ORDER_CTL_ORDER_R_FIRST;
		break;
	default:
		loge(&(state->pdev->dev), "invalid mbus code(0x%x)\n", code);
		ret = IMG_IN_ORDER_CTL_ORDER_R_FIRST;
		break;
	}

	return ret;
}

static uint32_t
tcc_isp_core_get_rgbir_output_order(const struct tcc_isp_state *state,
				    uint32_t code)
{
	uint32_t ret = 0;

	switch (code) {
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SBGGR14_1X14:
		ret = ISP_RGBIR_CTL_ROBP_B_FIRST;
		break;
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGBRG14_1X14:
		ret = ISP_RGBIR_CTL_ROBP_GB_FIRST;
		break;
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SGRBG14_1X14:
		ret = ISP_RGBIR_CTL_ROBP_GR_FIRST;
		break;
	case MEDIA_BUS_FMT_SRGGB8_1X8:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
	case MEDIA_BUS_FMT_SRGGB14_1X14:
		ret = ISP_RGBIR_CTL_ROBP_R_FIRST;
		break;
	default:
		loge(&(state->pdev->dev), "invalid mbus code(0x%x)\n", code);
		ret = ISP_RGBIR_CTL_ROBP_R_FIRST;
		break;
	}

	return ret;
}

static void tcc_isp_core_set_rgbir(const struct tcc_isp_state *state)
{
	static const char *const ribp_str[] = { "GBIRG", "BGGIR", "IRGGB",
						"GIRBG", "GRIRG", "RGGIR",
						"IRGGR", "GIRRG" };
	static const char *const rgbir_out_str[] = { "GR_FIRST", "R_FIRST",
						     "B_FIRST", "GB_FIRST" };
	uint32_t pixel_order;
	uint32_t output_order;
	uint32_t src_w, src_h, dst_w, dst_h, dst_x, dst_y;
	uint32_t val = 0U;

	src_w = state->fmt[TCC_ISP_PAD_SINK].width;
	src_h = state->fmt[TCC_ISP_PAD_SINK].height;

	dst_x = state->out_win_crop.x;
	dst_y = state->out_win_crop.y;
	dst_w = state->fmt[TCC_ISP_PAD_SRC].width;
	dst_h = state->fmt[TCC_ISP_PAD_SRC].height;

	pixel_order = state->rgbir_order;
	output_order = tcc_isp_core_get_rgbir_output_order(
		state, state->fmt[TCC_ISP_PAD_SINK].code);

	logd(&(state->pdev->dev), "input size(%d x %d), rgb-ir order(%s)\n",
	     src_w, src_h, ribp_str[pixel_order]);

	logd(&(state->pdev->dev),
	     "ir output pos(%d, %d), ir output size(%d x %d), rgb order(%s)\n",
	     dst_x, dst_y, dst_w, dst_h, rgbir_out_str[output_order]);

	/* img size */
	val = 0U;
	val |= ((src_w << ISP_RGBIR_IMG_RIW_SHIFT) |
		(src_h << ISP_RGBIR_IMG_RIH_SHIFT));
	tcc_isp_writel(val, state->isp_base, REG_ISP_RGBIR_IMG);

	/* crop boundary for ir output */
	val = 0U;
	val |= ((dst_x << ISP_RGBIR_CROP_POS_RCXP_SHIFT) |
		(dst_y << ISP_RGBIR_CROP_POS_RCYP_SHIFT));
	tcc_isp_writel(val, state->isp_base, REG_ISP_RGBIR_CROP_POS);
	val = 0U;
	val |= ((dst_w << ISP_RGBIR_CROP_IMG_RCIW_SHIFT) |
		(dst_h << ISP_RGBIR_CROP_IMG_RCIH_SHIFT));
	tcc_isp_writel(val, state->isp_base, REG_ISP_RGBIR_CROP_IMG);

	/* input order, sub sample mode and enable */
	val = 0U;
	val |= ((state->rgbir_order << ISP_RGBIR_CTL_RIBP_SHIFT) |
		((uint32_t)1U << ISP_RGBIR_CTL_RSE_SHIFT) |
		((uint32_t)1U << ISP_RGBIR_CTL_RWE_SHIFT) |
		(output_order << ISP_RGBIR_CTL_ROBP_SHIFT) |
		((uint32_t)0U << ISP_RGBIR_CTL_RE_SHIFT) |
		((uint32_t)0U << ISP_RGBIR_CTL_RGE_SHIFT));
	tcc_isp_writel(val, state->isp_base, REG_ISP_RGBIR_CTL);
}

static void tcc_isp_core_set_demosaic_mode(const struct tcc_isp_state *state)
{
	static const char *const demosaic_mode_str[] = { "RGGB", "N/A", "N/A",
							 "RGB-IR" };

	logd(&(state->pdev->dev), "demosaic %s\n",
	     demosaic_mode_str[state->demosaic_mode]);

	switch (state->demosaic_mode) {
	case TCC_ISP_DEMOSAIC_MODE_RGGB:
		break;
	case TCC_ISP_DEMOSAIC_MODE_RGBIR:
		tcc_isp_core_set_rgbir(state);
		break;
	default:
		loge(&(state->pdev->dev), "NOT supported demosaic(%d)\n",
		     state->demosaic_mode);
	}
}

static void tcc_isp_core_set_deblank(const struct tcc_isp_state *state)
{
	uint32_t val = 0U;
	uint32_t ib_sel = 0U;

	if (state->hdr) {
		logd(&(state->pdev->dev), "input bit bypass\n");
		ib_sel = ISP_DEBLANK_IB_BYPASS;
	} else {
		switch (state->fmt[TCC_ISP_PAD_SINK].code) {
		case MEDIA_BUS_FMT_SBGGR8_1X8:
		case MEDIA_BUS_FMT_SGBRG8_1X8:
		case MEDIA_BUS_FMT_SGRBG8_1X8:
		case MEDIA_BUS_FMT_SRGGB8_1X8:
			logd(&(state->pdev->dev), "input bit 8bit\n");
			ib_sel = ISP_DEBLANK_IB_8BIT_INPUT;
			break;
		case MEDIA_BUS_FMT_SBGGR10_1X10:
		case MEDIA_BUS_FMT_SGBRG10_1X10:
		case MEDIA_BUS_FMT_SGRBG10_1X10:
		case MEDIA_BUS_FMT_SRGGB10_1X10:
			logd(&(state->pdev->dev), "input bit 10bit\n");
			ib_sel = ISP_DEBLANK_IB_10BIT_INPUT;
			break;
		case MEDIA_BUS_FMT_SBGGR12_1X12:
		case MEDIA_BUS_FMT_SGBRG12_1X12:
		case MEDIA_BUS_FMT_SGRBG12_1X12:
		case MEDIA_BUS_FMT_SRGGB12_1X12:
			logd(&(state->pdev->dev), "input bit 12bit\n");
			ib_sel = ISP_DEBLANK_IB_12BIT_INPUT;
			break;
		default:
			loge(&(state->pdev->dev), "input bit default bypass\n");
			ib_sel = ISP_DEBLANK_IB_BYPASS;
		}
	}

	val = ((ISP_DEBLANK_OFF << ISP_DEBLANK_ONOFF_SHIFT) |
	       (ISP_DEBLANK_VS_BYPASS << ISP_DEBLANK_VSEL_SHIFT) |
	       (5U << ISP_DEBLANK_FRONTPORCH_CONFIGURE_SHIFT) |
	       (3U << ISP_DEBLANK_VBLANK_CONFIGURE_SHIFT) |
	       (ib_sel << ISP_DEBLANK_IB_SEL_SHIFT));
	tcc_isp_writel(val, state->isp_base, REG_ISP_DEBLANK_CTL);
}

static void tcc_isp_core_set_apb(const struct tcc_isp_state *state)
{
	uint32_t val = 0U;

	val = tcc_isp_readl(state->cfg_base, ISP_CTL);
	val &= ~(ISP_CTL_IBA_MASK);
	val |= (ISP_CTL_IBA << ISP_CTL_IBA_SHIFT);
	tcc_isp_writel(val, state->cfg_base, ISP_CTL);
}

void tcc_isp_core_padding(const struct tcc_isp_state *state, uint32_t code)
{
	uint32_t val = 0U, padding = 0U, mask = 0U, shift = 0U;

	switch (code) {
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		padding = ISP_FMT_RAW_RGB_8_TO_10_USING_ZERO;
		logi(&(state->pdev->dev), "padding(8 to 10)\n");
		break;
	default:
		padding = ISP_FMT_RAW_RGB_10_OR_MORE;
		break;
	}

	switch (state->pdev->id) {
	case 0:
		mask = ISP_CTL_I0F_MASK;
		shift = ISP_CTL_I0F_SHIFT;
		break;
	case 1:
		mask = ISP_CTL_I1F_MASK;
		shift = ISP_CTL_I1F_SHIFT;
		break;
	case 2:
		mask = ISP_CTL_I2F_MASK;
		shift = ISP_CTL_I2F_SHIFT;
		break;
	case 3:
		mask = ISP_CTL_I3F_MASK;
		shift = ISP_CTL_I3F_SHIFT;
		break;
	default:
		loge(&(state->pdev->dev), "invalid isp id(%d)\n",
		     state->pdev->id);
		break;
	}

	val = tcc_isp_readl(state->cfg_base, ISP_CTL);
	val &= ~(mask);
	val |= (padding << shift);
	tcc_isp_writel(val, state->cfg_base, ISP_CTL);
}

static void tcc_isp_up_reg(const struct tcc_isp_state *state)
{
	void __iomem *isp_base = state->isp_base;

	tcc_isp_writel(UP_CTL_UP_ALL, isp_base, REG_ISP_UP_CTL);
}

static int32_t tcc_isp_core_is_cmd(const struct tcc_isp_state *state, char *cmd)
{
	size_t len = 0U;
	int32_t ret = 0;

	/* remove leading and trailing space */
	cmd = strim(cmd);
	len = strlen(cmd);
	if (len == 0U) {
		/* error */
		ret = -EINVAL;
	}

	if (ret >= 0) {
		if ((len >= 2U) && (((cmd[0] == '/') && (cmd[1] == '/')) ||
				    ((cmd[0] == '/') && (cmd[1] == '*')))) {
			logd(&(state->pdev->dev), "%s\n", cmd);
			/* this cmd is comment */
			ret = -EINVAL;
		}
	}

	if (ret >= 0) {
		if (cmd[0] == '\r') {
			/* this cmd is comment */
			ret = -EINVAL;
		}
	}

	/* remove trailing comment */
	if (ret >= 0) {
		if ((cmd[0] == 'I' && cmd[1] == 'D')) {
			/* case for ID command */
			cmd[5] = '\0';
		} else {
			/* case except for ID */
			cmd = strsep(&cmd, " \t\r\n");
			len = strlen(cmd);
			if (len == 0U) {
				/* error */
				ret = -EINVAL;
			}
		}
	}

	return ret;
}

static bool tcc_isp_core_cmd_id_isp_core_skip(struct tcc_isp_state *state,
					      uint32_t reg, uint32_t *val)
{
	bool ret = true;
	uint32_t mask;

	switch (reg) {
	case REG_ISP_CTL:
		/* skip ISP reset related setting */
		ret = true;
		logd(&(state->pdev->dev),
		     "ISP_CTL(0x%x) of btset will be ignored\n", reg);
		break;
	case REG_ISP_HDR_CTL:
		ret = false;
		mask = ISP_HDR_CTL_HE_MASK;
		if ((*val & mask) != 0U) {
			/*
			 * HDR is enabled by btset.
			 * But HDR will be not enabled at the init callback.
			 * HDR will be enabled at s_stream callback.
			 */
			logd(&(state->pdev->dev), "HDR is enabled by btset\n");
			state->hdr = true;
			*val &= ~mask;
		} else {
			/* HDR disabled */
			state->hdr = false;
		}
		break;
	case REG_ISP_RGBIR_CTL:
		ret = false;
		mask = (ISP_RGBIR_CTL_RE_MASK | ISP_RGBIR_CTL_RGE_MASK);
		if ((*val & mask) != 0U) {
			logd(&(state->pdev->dev),
			     "RGBIR is enabled by btset\n");
			*val &= ~mask;
		}
		break;
	case REG_ISP_DEBLANK_CTL:
		ret = true;
		logd(&(state->pdev->dev),
		     "DEBLANK_CTL(0x%x) of btest will be ignored\n", reg);
		break;
	default:
		ret = false;
	}

	return ret;
}

static int32_t tcc_isp_core_cmd_id_isp_core(struct tcc_isp_state *state,
					    uint32_t reg, uint32_t val)
{
	bool skip_cmd = false;
	uint32_t cmd_cnt = state->tune_core.isp_setting_cnt;
	int32_t ret = 0;

	if (cmd_cnt >= TCC_ISP_SETTING_CMD_MAX) {
		loge(&(state->pdev->dev),
		     "the number of setting is larger than max(%d)\n",
		     TCC_ISP_SETTING_CMD_MAX);
		ret = -ENOMEM;
	}

	if (ret >= 0) {
		skip_cmd = tcc_isp_core_cmd_id_isp_core_skip(state, reg, &val);

		if (skip_cmd == false) {
			/* These btset values will be set at init callback. */
			state->tune_core.setting[cmd_cnt].reg = reg;
			state->tune_core.setting[cmd_cnt].val = val;

			logd(&(state->pdev->dev), "{0x%x, 0x%x},\n",
			     state->tune_core.setting[cmd_cnt].reg,
			     state->tune_core.setting[cmd_cnt].val);

			state->tune_core.isp_setting_cnt++;
		} else {
			logi(&(state->pdev->dev),
			     "skip isp tune_core.setting value{0x%x, 0x%x}\n",
			     reg, val);
		}
	}

	return ret;
}

static int32_t tcc_isp_core_cmd_id_scene_data(struct tcc_isp_state *state,
					      uint32_t reg, uint32_t val)
{
	uint32_t cmd_cnt = state->tune_scene.isp_setting_cnt;
	int32_t ret = 0;

	if (cmd_cnt >= TCC_ISP_SETTING_CMD_MAX) {
		loge(&(state->pdev->dev),
		     "the number of setting is larger than max(%d)\n",
		     TCC_ISP_SETTING_CMD_MAX);
		ret = -ENOMEM;
	}

	if (ret >= 0) {
		state->tune_scene.setting[cmd_cnt].reg = reg;
		state->tune_scene.setting[cmd_cnt].val = val;

		logd(&(state->pdev->dev), "{0x%x, 0x%x},\n",
		     state->tune_scene.setting[cmd_cnt].reg,
		     state->tune_scene.setting[cmd_cnt].val);

		state->tune_scene.isp_setting_cnt++;
	}

	return ret;
}

static int32_t tcc_isp_core_save_cmd(struct tcc_isp_state *state,
				     uint32_t data_id, uint32_t reg,
				     uint32_t val)
{
	int32_t ret = 0;

	switch (data_id) {
	case TCC_ISP_CORE_CMD_ID_ISP0_CORE:
	case TCC_ISP_CORE_CMD_ID_ISP1_CORE:
	case TCC_ISP_CORE_CMD_ID_ISP2_CORE:
	case TCC_ISP_CORE_CMD_ID_ISP3_CORE:
		logd(&(state->pdev->dev),
		     "data id: 0x%x, reg: 0x%x, val: 0x%x\n", data_id, reg,
		     val);
		ret = tcc_isp_core_cmd_id_isp_core(state, reg, val);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
			     "tcc_isp_core_cmd_id_isp_core returned %d\n", ret);
		}
		break;
	case TCC_ISP_CORE_CMD_ID_SCENE_DATA:
		ret = tcc_isp_core_cmd_id_scene_data(state, reg, val);
		logd(&(state->pdev->dev),
		     "data id: 0x%x, reg: 0x%x, val: 0x%x\n", data_id, reg,
		     val);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
			     "tcc_isp_core_cmd_id_scene_data returned %d\n",
			     ret);
		}
		break;
	default:
		/* error */
		loge(&(state->pdev->dev), "wrong data id(%d)\n", data_id);
		ret = -EINVAL;
	}

	return ret;
}

static int32_t tcc_isp_core_parse_cmd(struct tcc_isp_state *state, char *cmd,
				      struct btset_element *parsed_cmd)
{
	uint32_t val = 0U;
	int32_t ret = 0;

	logd(&(state->pdev->dev), "parsing %s\n", cmd);

	if (strncmp(cmd, "btp", 3) == 0) {
		ret = kstrtouint(&(cmd[3]), 0, &(parsed_cmd->pg));
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev), "kstrtouint returned %d\n",
			     ret);
		} else {
			/* page */
			parsed_cmd->pg &= 0x0000F;
			parsed_cmd->pg <<= 12U;
		}
	} else if (strncmp(cmd, "bta", 3) == 0) {
		ret = kstrtouint(&(cmd[3]), 0, &(parsed_cmd->addr));
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev), "kstrtouint returned %d\n",
			     ret);
		} else {
			/* addr */
			parsed_cmd->addr &= 0x0FFFF;
			parsed_cmd->addr |= parsed_cmd->pg;
		}
	} else if (strncmp(cmd, "btw", 3) == 0) {
		ret = kstrtouint(&(cmd[3]), 0, &val);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev), "kstrtouint returned %d\n",
			     ret);
		} else {
			/* okay */
			ret = tcc_isp_core_save_cmd(state, parsed_cmd->data_id,
						    parsed_cmd->addr, val);
			if (ret < 0) {
				/* error */
				loge(&(state->pdev->dev),
				     "tcc_isp_core_save_cmd returned %d\n",
				     ret);
			} else {
				/* okay */

				if ((U32_MAX - 4U) < parsed_cmd->addr) {
					/* error */
					ret = -EINVAL;
					loge(&(state->pdev->dev),
					     "addr overflow\n");
				} else {
					/* okay */
					parsed_cmd->addr += 4U;
				}
			}
		}
	} else if (strncmp(cmd, "ID", 2) == 0) {
		ret = kstrtouint(&(cmd[3]), 16, &parsed_cmd->data_id);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev), "kstrtouint returned %d\n",
			     ret);
		}
	} else {
		/* error */
		logw(&(state->pdev->dev), "unknown cmd(%s)\n", cmd);
	}

	return ret;
}

int32_t tcc_isp_core_load_setting(struct tcc_isp_state *state,
				  const struct firmware *fw)
{
	char *btset = NULL, *btset_origin = NULL, *cmd = NULL;
	struct btset_element parsed_cmd = {
		0,
	};
	int32_t ret = 0;

	/* alloc buffer */
	btset_origin =
		(char *)devm_kzalloc(&(state->pdev->dev), fw->size, GFP_KERNEL);
	if (btset_origin == NULL) {
		loge(&(state->pdev->dev), "devm_kzalloc returned NULL\n");
		ret = -ENOMEM;
	}

	/* copy btset */
	if (ret >= 0) {
		if (btset_origin != memcpy((void *)btset_origin,
					   (const void *)fw->data, fw->size)) {
			loge(&(state->pdev->dev), "Fail - memcpy\n");
			ret = -EINVAL;
		}
	}

	/* parse btset */
	if (ret >= 0) {
		btset = btset_origin;

		while (true) {
			/* get line */
			cmd = strsep(&btset, "\n");
			if ((cmd == NULL) || (ret < 0)) {
				/* parsing end */
				break;
			}

			/* check line */
			if (tcc_isp_core_is_cmd(state, cmd) < 0) {
				/* skip cmd */
				continue;
			}

			/* parse command */
			ret = tcc_isp_core_parse_cmd(state, cmd, &parsed_cmd);
			if (ret < 0) {
				/* exit while loop */
				loge(&(state->pdev->dev),
				     "tcc_isp_core_parse_cmd returned %d\n",
				     ret);
			}
		}

		if (ret >= 0) {
			/* success loading setting */
			state->setting_load = 1;
		}
	}

	if (btset_origin != NULL) {
		/* free buffer */
		devm_kfree(&(state->pdev->dev), btset_origin);
	}

	return ret;
}

static void tcc_isp_core_mem_share(const struct tcc_isp_state *state,
				   uint32_t onOff)
{
	void __iomem *isp_base = state->isp_base;
	uint32_t val = 0U;

	switch (state->pdev->id) {
	case 0:
		val = (CTL_MEM_SHARE_EN0_MASK << CTL_MEM_SHARE_EN0_SHIFT);
		break;
	case 2:
		val = (CTL_MEM_SHARE_EN1_MASK << CTL_MEM_SHARE_EN1_SHIFT);
		break;
	default:
		loge(&(state->pdev->dev), "invalid isp id(%d)\n",
		     state->pdev->id);
		break;
	}

	if (onOff == 1U) {
		/* mem share enable */
		val = (tcc_isp_readl(isp_base, REG_ISP_CTL) | val);
		logd(&(state->pdev->dev), "memory share is enabled\n");
	} else {
		/* mem share disable */
		val = (tcc_isp_readl(isp_base, REG_ISP_CTL) & ~val);
		logd(&(state->pdev->dev), "memory share is disabled\n");
	}

	tcc_isp_writel(val, isp_base, REG_ISP_CTL);
}

static void tcc_isp_core_set_up_mode(const struct tcc_isp_state *state)
{
	void __iomem *isp_base = state->isp_base;
	uint32_t up_sel0, up_sel1, up_mode0, up_mode1, user_cnt;
	static const char *const str_update_sel[] = {
		"USER SPECIFIED TIMING", "VSYNC FALLING EDGE TIMING",
		"VSYNC RISING EDGE TIMING", "WRITING TIMING"
	};
	static const char *const str_up_mode0[] = { "INDIVIDUAL SYNC",
						    "GROUP SYNC" };
	static const char *const str_up_mode1[] = { "RISING EDGE OF VSYNC",
						    "FALLING EDGE OF VSYNC" };

	up_sel0 = UP_SEL0_ALL_SYNC_ON_USER_SPECIFIED_TIMING;
	up_sel1 = UP_SEL1_ALL_SYNC_ON_USER_SPECIFIED_TIMING;

	up_mode0 = UP_MODE0_ALL_INDIV_SYNC_MODE;
	up_mode1 = UP_MODE1_ALL_RISING_EDGE_MODE;

	/* TODO:
	 * check D3 and D5 value
	 */
	user_cnt = ((uint32_t)0x00000200U);

	/* USR_CNT is used in USER SPECIFIED TIMING */
	tcc_isp_writel(user_cnt, isp_base, REG_ISP_USR_CNT);

	/*
	 * 0: sync on user specified timing
	 * 1: sync on vsync falling edge timing
	 * 2: sync on vsync rising edge timing
	 * 3: sync on writing timing
	 */
	logd(&(state->pdev->dev), "up_sel0, 2(0x%x, 0x%x) is %s\n", up_sel0,
	     up_sel1,
	     str_update_sel[(up_sel0 >> UP_SEL0_TP_SEL_CTL_SHIFT) &
			    (UP_SEL0_TP_SEL_CTL_MASK)]);

	tcc_isp_writel(up_sel0, isp_base, REG_ISP_UP_SEL0);
	tcc_isp_writel(up_sel1, isp_base, REG_ISP_UP_SEL1);

	/*
	 * 0: rising edge of vsync
	 * 1: falling edge of vsync
	 */
	logd(&(state->pdev->dev), "up_mode0(0x%x) is %s\n", up_mode0,
	     str_up_mode0[up_mode0 & (UP_MODE0_TP_UP_MODE_MASK
				      << UP_MODE0_TP_UP_MODE_SHIFT)]);

	tcc_isp_writel(up_mode0, isp_base, REG_ISP_UP_MODE0);

	/*
	 * 0: individual sync
	 * 1: group sync
	 */
	logd(&(state->pdev->dev), "up_mode1(0x%x) is %s\n", up_mode1,
	     str_up_mode1[up_mode1 & (UP_MODE1_TP_VSYNC_SEL_MASK
				      << UP_MODE1_TP_VSYNC_SEL_SHIFT)]);

	tcc_isp_writel(up_mode1, isp_base, REG_ISP_UP_MODE1);
}

static void tcc_isp_core_set_wdma(const struct tcc_isp_state *state, int onOff)
{
	void __iomem *isp_base = state->isp_base;

	if (onOff == ON) {
		tcc_isp_writel((WDMA_CTL0_WDMA_ENABLE
				<< WDMA_CTL0_WDMA_ENABLE_SHIFT),
			       isp_base, REG_ISP_WDMA_CTL0);
	} else {
		tcc_isp_writel((WDMA_CTL0_WDMA_DISABLE
				<< WDMA_CTL0_WDMA_ENABLE_SHIFT),
			       isp_base, REG_ISP_WDMA_CTL0);
		/* CV8050C-810 */
		/* tcc_isp_writel(0x0U, isp_base, REG_ISP_WDMA_CFG0); */
	}
}

static void tcc_isp_core_set_rgbir_sync(const struct tcc_isp_state *state)
{
	void __iomem *isp_base = state->rgbir_sync_base;
	uint32_t w, h;
	uint32_t val = 0U;

	w = state->fmt[TCC_ISP_PAD_SRC].width;
	h = state->fmt[TCC_ISP_PAD_SRC].height;

	logd(&(state->pdev->dev), "input size(%d x %d)\n", w, h);

	/* img size */
	val = 0U;
	val |= ((w << RGBIR_SYNC_RESOLUTION_WIDTH_SHIFT) |
		(h << RGBIR_SYNC_RESOLUTION_HEIGHT_SHIFT));
	tcc_isp_writel(val, isp_base, REG_RGBIR_SYNC_RESOLUTION);

	/* interpolation mode */
	val = 0U;
	if (state->demosaic_mode == TCC_ISP_DEMOSAIC_MODE_RGBIR) {
		val = RGBIR_SYNC_INTERPOLATION_MODE2;
	} else {
		val = (RGBIR_SYNC_INTERPOLATION_MODE_RGB_BYPASS_MASK |
		       RGBIR_SYNC_INTERPOLATION_MODE2);
	}
	tcc_isp_writel(val, state->rgbir_sync_base,
		       REG_RGBIR_SYNC_INTERPOLATION_MODE);
}

static void tcc_isp_core_set_input(const struct tcc_isp_state *state)
{
	void __iomem *isp_base = state->isp_base;
	static const char *const str[] = { "Blue First", "Gb first", "Gr first",
					   "Red first" };
	uint32_t w, h, pixel_order;

	w = state->fmt[TCC_ISP_PAD_SINK].width;
	h = state->fmt[TCC_ISP_PAD_SINK].height;
	pixel_order = tcc_isp_core_pixel_order(
		state, state->fmt[TCC_ISP_PAD_SINK].code);

	logd(&(state->pdev->dev), "input size(%d x %d) rgb order(%s)\n", w, h,
	     str[pixel_order]);

	/* size */
	tcc_isp_writel((w << IMG_WIDTH_IN_IMG_WIDTH_SHIFT), isp_base,
		       REG_ISP_IMG_WIDTH);
	tcc_isp_writel((h << IMG_HEIGHT_IN_IMG_HEIGHT_SHIFT), isp_base,
		       REG_ISP_IMG_HEIGHT);

	/* bayer rgb order */
	tcc_isp_writel((pixel_order << IMG_IN_ORDER_CTL_ORDER_SHIFT), isp_base,
		       REG_ISP_IMG_IN_ORDER_CTL);
}

static void tcc_isp_core_set_output(const struct tcc_isp_state *state)
{
	void __iomem *isp_base = state->isp_base;
	// static const char * const str[] = {
	// "YUV420", "YUV422", "YUV444", "RGB888"};
	uint32_t src_w, src_h, dst_w, dst_h, dst_x, dst_y;
	uint32_t fmt, win_ctl;

	win_ctl = 0U;

	src_w = state->fmt[TCC_ISP_PAD_SINK].width;
	src_h = state->fmt[TCC_ISP_PAD_SINK].height;

	dst_x = state->out_win_crop.x;
	dst_y = state->out_win_crop.y;
	dst_w = state->fmt[TCC_ISP_PAD_SRC].width;
	dst_h = state->fmt[TCC_ISP_PAD_SRC].height;

	if ((src_w != dst_w) || (src_h != dst_h)) {
		logd(&(state->pdev->dev), "enable crop window\n");
		win_ctl =
			(IMG_WIN_CTL_WIN_EN_ENABLE << IMG_WIN_CTL_WIN_EN_SHIFT);
	} else {
		logd(&(state->pdev->dev), "disable crop window\n");
	}

	tcc_isp_writel(win_ctl, isp_base, REG_ISP_IMG_WIN_CTL);

	/* format */
	switch (state->fmt[TCC_ISP_PAD_SRC].code) {
	case MEDIA_BUS_FMT_YUV8_1X24:
		fmt = (IMG_WIN_FORMAT_FORMAT_YUV444
		       << IMG_WIN_FORMAT_FORMAT_SHIFT);
		break;
	case MEDIA_BUS_FMT_RGB888_1X24:
		fmt = (IMG_WIN_FORMAT_FORMAT_RGB888
		       << IMG_WIN_FORMAT_FORMAT_SHIFT);
		break;
	case MEDIA_BUS_FMT_UYVY8_1X16:
	default:
		fmt = (IMG_WIN_FORMAT_FORMAT_YUV422
		       << IMG_WIN_FORMAT_FORMAT_SHIFT);
		fmt &= ~(IMG_WIN_FORMAT_DATA_ORDER_MASK
			 << IMG_WIN_FORMAT_DATA_ORDER_SHIFT);
		fmt |= (IMG_WIN_FORMAT_DATA_ORDER_P0P2P1
			<< IMG_WIN_FORMAT_DATA_ORDER_SHIFT);
		break;
	}
	tcc_isp_writel(fmt, isp_base, REG_ISP_IMG_WIN_FORMAT);

	/* crop */
	if ((src_w != dst_w) || (src_h != dst_h)) {
		logd(&(state->pdev->dev), "output crop(%d, %d / %d x %d)\n",
		     dst_x, dst_y, dst_w, dst_h);

		tcc_isp_writel((dst_x << IMG_WIN_X_START_SHIFT), isp_base,
			       REG_ISP_IMG_WIN_X_START);
		tcc_isp_writel((dst_y << IMG_WIN_Y_START_SHIFT), isp_base,
			       REG_ISP_IMG_WIN_Y_START);
		tcc_isp_writel((dst_w << IMG_WIN_WIDTH_SHIFT), isp_base,
			       REG_ISP_IMG_WIN_WIDTH);
		tcc_isp_writel((dst_h << IMG_WIN_HEIGHT_SHIFT), isp_base,
			       REG_ISP_IMG_WIN_HEIGHT);
	}
}

static int32_t tcc_isp_core_reset(const struct tcc_isp_state *state, int reset)
{
	void __iomem *isp_base = state->isp_base;
	uint32_t val = 0U;
	uint32_t swrst = (((CTL_ISPX_SOFT_RESET_MASK << state->pdev->id)
			   << CTL_ISP_SOFT_RESET_SHIFT) |
			  ((CTL_ISPX_APB_SOFT_RESET_MASK << state->pdev->id)
			   << CTL_ISP_APB_SOFT_RESET_SHIFT));
	int32_t ret = 0;

	if (reset == ON) {
		/* reset state */
		val = tcc_isp_readl(isp_base, REG_ISP_CTL);
		val |= swrst;
		tcc_isp_writel(val, isp_base, REG_ISP_CTL);
	} else {
		/* reset release state */
		val = tcc_isp_readl(isp_base, REG_ISP_CTL);
		val &= ~(swrst);
		tcc_isp_writel(val, isp_base, REG_ISP_CTL);
	}

	return ret;
}

static void tcc_isp_core_set_io(const struct tcc_isp_state *state)
{
	/* set register update control */
	tcc_isp_core_set_up_mode(state);

	/* disable wdma(IM896A-22) */
	tcc_isp_core_set_wdma(state, OFF);

	/* memory sharing */
	if ((state->pdev->id == 0) || (state->pdev->id == 2)) {
		/*
		 * ISP0 and ISP2 can process images up to 2560x1440@60fps
		 * using the memory of ISP1 and ISP3.
		 * When memory share is enabled, ISP1 and ISP3 cannot be used
		 */
		tcc_isp_core_mem_share(state, state->mem_share);
	}

	/* deblank */
	tcc_isp_core_set_deblank(state);

	/* demosaic */
	tcc_isp_core_set_demosaic_mode(state);

	/* zelcova(isp core) input */
	tcc_isp_core_set_input(state);

	/* zelcova(isp core) output */
	tcc_isp_core_set_output(state);

	if (state->rgbir_sync_base != NULL) {
		/* rgbir_sync */
		tcc_isp_core_set_rgbir_sync(state);
	}

	/* register update */
	tcc_isp_up_reg(state);
}

static void tcc_isp_core_set_tune(const struct tcc_isp_state *state)
{
	uint32_t i = 0U;

	memset_io(state->scene_data_base, 0, ISP_MEM_SIZE_ADT);

	for (i = 0U; i < state->tune_scene.isp_setting_cnt; i++) {
		/* set tune_scene value of ISP */
		tcc_isp_writel(state->tune_scene.setting[i].val,
			       state->scene_data_base,
			       state->tune_scene.setting[i].reg);
	}

	for (i = 0U; i < state->tune_core.isp_setting_cnt; i++) {
		/* set tune_core value of ISP */
		tcc_isp_writel(state->tune_core.setting[i].val, state->isp_base,
			       state->tune_core.setting[i].reg);
	}
	tcc_isp_up_reg(state);

	logd(&(state->pdev->dev), "complete applying btset\n");
}

void tcc_isp_core_set_deblank_on(const struct tcc_isp_state *state)
{
	uint32_t val = 0U;

	val = tcc_isp_readl(state->isp_base, REG_ISP_DEBLANK_CTL);
	val &= ~(ISP_DEBLANK_ONOFF_MASK);

	val |= (ISP_DEBLANK_ON << ISP_DEBLANK_ONOFF_SHIFT);

	tcc_isp_writel(val, state->isp_base, REG_ISP_DEBLANK_CTL);

	/* register update */
	tcc_isp_up_reg(state);
}

void tcc_isp_core_set_rgbir_on(const struct tcc_isp_state *state)
{
	uint32_t val = 0U;

	val = tcc_isp_readl(state->isp_base, REG_ISP_RGBIR_CTL);

	/* enable */
	val |= (((uint32_t)1U << ISP_RGBIR_CTL_RE_SHIFT) |
		((uint32_t)1U << ISP_RGBIR_CTL_RGE_SHIFT));
	tcc_isp_writel(val, state->isp_base, REG_ISP_RGBIR_CTL);

	/* register update */
	tcc_isp_up_reg(state);
}

void tcc_isp_core_set_hdr_on(const struct tcc_isp_state *state)
{
	uint32_t val = 0U;

	val = tcc_isp_readl(state->isp_base, REG_ISP_HDR_CTL);
	val |= ISP_HDR_CTL_HE_MASK;
	tcc_isp_writel(val, state->isp_base, REG_ISP_HDR_CTL);

	/* register update */
	tcc_isp_up_reg(state);
}

void tcc_isp_core_set_default(struct tcc_isp_state *state)
{
	tcc_isp_core_padding(state, state->fmt[TCC_ISP_PAD_SINK].code);

	tcc_isp_core_set_apb(state);

	/* set register update control */
	tcc_isp_core_set_up_mode(state);

	/* register update */
	tcc_isp_up_reg(state);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
int32_t tcc_isp_core_get_reg(const struct tcc_isp_state *state,
			     const uint64_t reg, uint64_t *const val)
{
	const void __iomem *base = NULL;
	int32_t ret = 0;

	if ((reg & 0xD0000U) == 0xD0000U) {
		/* SW 3A Memory */
		base = state->scene_data_base;
	} else {
		/* HW ISP */
		base = state->isp_base;
	}

	*val = tcc_isp_readl(base, (uint32_t)(reg & 0xFFFFU));

	/* okay */
	ret = 4;

	return ret;
}

int32_t tcc_isp_core_set_reg(const struct tcc_isp_state *state,
			     const uint64_t reg, const uint64_t val)
{
	void __iomem *base = NULL;
	int32_t ret = 0;

	if ((reg & 0xD0000U) == 0xD0000U) {
		/* SW 3A Memory */
		base = state->scene_data_base;
	} else {
		/* HW ISP */
		base = state->isp_base;
	}

	tcc_isp_writel((uint32_t)(val & 0xFFFFFFFFU), base,
		       (uint32_t)(reg & 0xFFFFU));

	/* okay */
	ret = 4;

	return ret;
}
#endif

int32_t tcc_isp_core_init(const struct tcc_isp_state *state, uint32_t enable)
{
	int32_t ret = 0;

	if (enable != 0U) {
		/* enable isp */
		tcc_isp_core_set_tune(state);
		tcc_isp_core_set_io(state);
	} else {
		/* disable isp */
		/* reset all ISP */
		ret = tcc_isp_core_reset(state, ON);
		if (ret < 0) {
			loge(&(state->pdev->dev),
			     "tcc_isp_core_reset returned %d\n", ret);
		}
		if (ret >= 0) {
			/* reset release of all ISP */
			ret = tcc_isp_core_reset(state, OFF);
			if (ret < 0) {
				loge(&(state->pdev->dev),
				     "tcc_isp_core_reset returned %d\n", ret);
			}
		}
	}

	return ret;
}

void tcc_isp_core_s_stream(const struct tcc_isp_state *state)
{
	tcc_isp_core_set_deblank_on(state);
	if (state->demosaic_mode == TCC_ISP_DEMOSAIC_MODE_RGBIR) {
		tcc_isp_core_set_rgbir_on(state);
	}
	if (state->hdr == true) {
		tcc_isp_core_set_hdr_on(state);
	}
}