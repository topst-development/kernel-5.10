// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <soc/telechips/chipinfo.h>
#include "../../tcc-isp.h"
#include "../../tcc-isp-helper.h"
#include "tcc-isp-reg.h"
#include "tcc-isp-mcu.h"

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

static uint32_t tcc_isp_core_get_in_bit_sel(const struct tcc_isp_state *state,
					    uint32_t code)
{
	uint32_t ret = 0;

	switch (code) {
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		/*
		 * RAW8 will be padded 2 bits
		 * via ISP_FMT_CFG(MIPI_CFG_BASE + 0x214)
		 */
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		ret = DCPD_CTL_IN_BIT_SEL_10BITS;
		break;
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		ret = DCPD_CTL_IN_BIT_SEL_12BITS;
		break;
	case MEDIA_BUS_FMT_SBGGR14_1X14:
	case MEDIA_BUS_FMT_SGBRG14_1X14:
	case MEDIA_BUS_FMT_SGRBG14_1X14:
	case MEDIA_BUS_FMT_SRGGB14_1X14:
		ret = DCPD_CTL_IN_BIT_SEL_14BITS;
		break;
	default:
		loge(&(state->pdev->dev), "invalid mbus code(0x%x)\n", code);
		ret = DCPD_CTL_IN_BIT_SEL_10BITS;
		break;
	}

	return ret;
}

static void tcc_isp_core_set_apb(const struct tcc_isp_state *state)
{
	/* TCS: CV8050C-658 */
	tcc_isp_writel(0x16371637, state->cfg_base, ISP_APBADDR_CFG0);
	tcc_isp_writel(0x16371637, state->cfg_base, ISP_APBADDR_CFG1);
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
		mask = ISP_FMT_CFG_ISP0_FMT_MASK;
		shift = ISP_FMT_CFG_ISP0_FMT_SHIFT;
		break;
	case 1:
		mask = ISP_FMT_CFG_ISP1_FMT_MASK;
		shift = ISP_FMT_CFG_ISP1_FMT_SHIFT;
		break;
	case 2:
		mask = ISP_FMT_CFG_ISP2_FMT_MASK;
		shift = ISP_FMT_CFG_ISP2_FMT_SHIFT;
		break;
	case 3:
		mask = ISP_FMT_CFG_ISP3_FMT_MASK;
		shift = ISP_FMT_CFG_ISP3_FMT_SHIFT;
		break;
	default:
		loge(&(state->pdev->dev), "invalid isp id(%d)\n",
		     state->pdev->id);
		break;
	}

	val = (tcc_isp_readl(state->cfg_base, ISP_FMT_CFG) & (~mask));
	val |= (padding << shift);
	tcc_isp_writel(val, state->cfg_base, ISP_FMT_CFG);
}

/**
 * tcc_isp_core_req_asbr_lp() - Set async bridge to low-power mode
 *
 * @state: pointer to &struct tcc_isp_state
 */
static int32_t tcc_isp_core_req_asbr_lp(const struct tcc_isp_state *state)
{
	void __iomem *mem_base = state->cfg_base;
	uint32_t val = 0, cnt = 0;
	int32_t ret = 0;

	/* disable power down bypass(pwrdn_bypass) */
	val = tcc_isp_readl(mem_base, ISP_X2X_CFG);
	val &= ~ISP_X2X_CFG_PWRDN_BYPASS_MASK;
	tcc_isp_writel(val, mem_base, ISP_X2X_CFG);
	usleep_range(1000U, 2000U);

	/* request power down(pwrdnreqn) */
	val = tcc_isp_readl(mem_base, ISP_X2X_CFG);
	val &= ~ISP_X2X_CFG_PWRDNREQN_MASK;
	tcc_isp_writel(val, mem_base, ISP_X2X_CFG);
	usleep_range(1000U, 2000U);

	/* check power down request acknowledge(pwrdnackn) */
	while (cnt < 10U) {
		val = tcc_isp_readl(mem_base, ISP_X2X_CFG);
		if ((val & ISP_X2X_CFG_PWRDNACKN_MASK) == 0U) {
			/* wait ack */
			break;
		}

		cnt++;
		usleep_range(1000U, 2000U);
	}

	if (cnt >= 10U) {
		ret = -EBUSY;
		loge(&(state->pdev->dev), "NOT received ack\n");
	}

	return ret;
}

/**
 * tcc_isp_core_rel_asbr_lp() - Release low-power mode of async bridge
 *
 * @state: pointer to &struct tcc_isp_state
 */
static int32_t tcc_isp_core_rel_asbr_lp(const struct tcc_isp_state *state)
{
	void __iomem *mem_base = state->cfg_base;
	int32_t ret = 0;
	uint32_t val = 0U, cnt = 0U;

	/* request normal operation(pwrdnreqn) */
	val = tcc_isp_readl(mem_base, ISP_X2X_CFG);
	val |= ISP_X2X_CFG_PWRDNREQN_MASK;
	tcc_isp_writel(val, mem_base, ISP_X2X_CFG);
	usleep_range(1000U, 2000U);

	/* check normal operation request acknowledge(pwrdnackn) */
	while (cnt < 10U) {
		val = tcc_isp_readl(mem_base, ISP_X2X_CFG);
		if ((val & ISP_X2X_CFG_PWRDNACKN_MASK) != 0U) {
			/* get ack */
			break;
		}

		cnt++;
		usleep_range(1000U, 2000U);
	}

	if (cnt >= 10U) {
		ret = -1;
		loge(&(state->pdev->dev), "NOT recevied ack\n");
	}

	return ret;
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

static int32_t tcc_isp_core_cmd_id_isp_core(struct tcc_isp_state *state,
					    uint32_t reg, uint32_t val)
{
	uint32_t cmd_cnt = state->tune_core.isp_setting_cnt;
	int32_t ret = 0;

	if (cmd_cnt >= TCC_ISP_SETTING_CMD_MAX) {
		loge(&(state->pdev->dev),
		     "the number of setting is larger than max(%d)\n",
		     TCC_ISP_SETTING_CMD_MAX);
		ret = -ENOMEM;
	}

	if (ret >= 0) {
		state->tune_core.setting[cmd_cnt].reg = reg;
		state->tune_core.setting[cmd_cnt].val = val;

		if ((state->tune_core.setting[cmd_cnt].reg ==
		     REG_ISP_SOFT_RESET) ||
		    (state->tune_core.setting[cmd_cnt].reg ==
		     REG_ISP_MCU_CTL)) {
			logi(&(state->pdev->dev),
			     "skip isp tune_core.setting value{0x%x, 0x%x}\n",
			     state->tune_core.setting[cmd_cnt].reg,
			     state->tune_core.setting[cmd_cnt].val);
		} else {
			logd(&(state->pdev->dev), "{0x%x, 0x%x},\n",
			     state->tune_core.setting[cmd_cnt].reg,
			     state->tune_core.setting[cmd_cnt].val);

			state->tune_core.isp_setting_cnt++;
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
	case TCC_ISP_CORE_CMD_ID_ISP_CORE:
		logd(&(state->pdev->dev),
		     "data id: 0x%x, reg: 0x%x, val: 0x%x\n", data_id, reg,
		     val);
		ret = tcc_isp_core_cmd_id_isp_core(state, reg, val);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
			     "tcc_isp_cmd_id_isp_core returned %d\n", ret);
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

	if (onOff == 1U) {
		tcc_isp_writel((MEM_SHARE_MEM_SHARE_EN_ENABLE
				<< MEM_SHARE_MEM_SHARE_EN_SHIFT),
			       isp_base, REG_ISP_MEM_SHARE);
	} else {
		tcc_isp_writel((MEM_SHARE_MEM_SHARE_EN_DISABLE
				<< MEM_SHARE_MEM_SHARE_EN_SHIFT),
			       isp_base, REG_ISP_MEM_SHARE);
	}
}

static void tcc_isp_core_set_up_mode(const struct tcc_isp_state *state)
{
	void __iomem *isp_base = state->isp_base;
	uint32_t up_sel1, up_sel2, up_mode1, up_mode2, user_cnt1, user_cnt2;
	static const char *const str_update_sel[] = {
		"USER SPECIFIED TIMING", "VSYNC FALLING EDGE TIMING",
		"VSYNC RISING EDGE TIMING", "WRITING TIMING"
	};
	static const char *const str_up_mode1[] = { "SYNC MODE",
						    "ALWAYS MODE" };
	static const char *const str_up_mode2[] = { "INDIVIDUAL SYNC",
						    "GROUP SYNC" };

	up_sel1 = UP_SEL1_ALL_SYNC_ON_USER_SPECIFIED_TIMING;
	up_sel2 = UP_SEL2_ALL_SYNC_ON_USER_SPECIFIED_TIMING;

	up_mode1 = UP_MODE1_ALL_SYNC_MODE;
	up_mode2 = UP_MODE2_ALL_INDIV_SYNC_MODE;

	user_cnt1 = (((uint32_t)0U) << USR_CNT1_SHIFT);
	user_cnt2 = (((uint32_t)0xFU) << USR_CNT2_SHIFT);

	/* USR_CNT1(MSB) + USR_CNT2(LSB) is used in USER SPECIFIED TIMING */
	tcc_isp_writel(user_cnt1, isp_base, REG_ISP_USR_CNT1);
	tcc_isp_writel(user_cnt2, isp_base, REG_ISP_USR_CNT2);

	/*
	 * 0: sync on user specified timing
	 * 1: sync on vsync falling edge timing
	 * 2: sync on vsync rising edge timing
	 * 3: sync on writing timing
	 */
	logi(&(state->pdev->dev), "up_sel1, 2(0x%x, 0x%x) is %s\n", up_sel1,
	     up_sel2,
	     str_update_sel[(up_sel1 >> UP_SEL1_TP_SEL_CTL_SHIFT) &
			    (UP_SEL1_TP_SEL_CTL_MASK)]);

	tcc_isp_writel(up_sel1, isp_base, REG_ISP_UP_SEL1);
	tcc_isp_writel(up_sel2, isp_base, REG_ISP_UP_SEL2);

	/*
	 * 0: sync mode
	 * 1: async mode
	 */
	logi(&(state->pdev->dev), "up_mode1(0x%x) is %s\n", up_mode1,
	     str_up_mode1[up_mode1 & (UP_MODE1_TP_MASK << UP_MODE1_TP_SHIFT)]);

	tcc_isp_writel(up_mode1, isp_base, REG_ISP_UP_MODE1);

	/*
	 * 0: individual sync
	 * 1: group sync
	 */
	logi(&(state->pdev->dev), "up_mode2(0x%x) is %s\n", up_mode2,
	     str_up_mode2[up_mode2 & (UP_MODE2_TP_MASK << UP_MODE2_TP_SHIFT)]);

	tcc_isp_writel(up_mode2, isp_base, REG_ISP_UP_MODE2);
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

static void tcc_isp_core_set_input(const struct tcc_isp_state *state)
{
	void __iomem *isp_base = state->isp_base;
	static const char *const str[] = { "Blue First", "Gb first", "Gr first",
					   "Red first" };
	uint32_t val, w, h, pixel_order, in_bit_sel;

	w = state->fmt[TCC_ISP_PAD_SINK].width;
	/* set -2 to get the margin of vertical front porch */
	h = state->fmt[TCC_ISP_PAD_SINK].height - 2;
	pixel_order = tcc_isp_core_pixel_order(
		state, state->fmt[TCC_ISP_PAD_SINK].code);

	logi(&(state->pdev->dev), "input size(%d x %d) rgb order(%s)\n", w, h,
	     str[pixel_order]);

	/* size */
	tcc_isp_writel((w << IMG_WIDTH_IN_IMG_WIDTH_SHIFT), isp_base,
		       REG_ISP_IMG_WIDTH);
	tcc_isp_writel((h << IMG_HEIGHT_IN_IMG_HEIGHT_SHIFT), isp_base,
		       REG_ISP_IMG_HEIGHT);

	/* bayer rgb order */
	tcc_isp_writel((pixel_order << IMG_IN_ORDER_CTL_ORDER_SHIFT), isp_base,
		       REG_ISP_IMG_IN_ORDER_CTL);

	/* bpp */
	in_bit_sel = tcc_isp_core_get_in_bit_sel(
		state, state->fmt[TCC_ISP_PAD_SINK].code);
	val = tcc_isp_readl(isp_base, REG_ISP_DCPD_CTL);
	val &= ~(DCPD_CTL_IN_BIT_SEL_MASK << DCPD_CTL_IN_BIT_SEL_SHIFT);
	val |= (in_bit_sel << DCPD_CTL_IN_BIT_SEL_SHIFT);
	tcc_isp_writel(val, isp_base, REG_ISP_DCPD_CTL);
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
		logi(&(state->pdev->dev), "enable crop window\n");
		win_ctl =
			(IMG_WIN_CTL_WIN_EN_ENABLE << IMG_WIN_CTL_WIN_EN_SHIFT);
	} else {
		logi(&(state->pdev->dev), "disable crop window\n");
	}

	win_ctl |=
		(IMG_WIN_CTL_DEBLANK_EN_ENABLE << IMG_WIN_CTL_DEBLANK_EN_SHIFT);

	tcc_isp_writel(win_ctl, isp_base, REG_ISP_IMG_WIN_CTL);

	/* format */
	switch (state->fmt[TCC_ISP_PAD_SRC].code) {
	case MEDIA_BUS_FMT_YUV8_1X24:
		fmt = (IMG_WIN_FORMAT_FORMAT_YUV444
		       << IMG_WIN_FORMAT_FORMAT_SHIFT);
		break;
	case MEDIA_BUS_FMT_BGR888_1X24:
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
		logi(&(state->pdev->dev), "output crop(%d, %d / %d x %d)\n",
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
	void __iomem *cfg_base = state->cfg_base;
	void __iomem *isp_base = state->isp_base;
	uint32_t val = 0;
	uint32_t isp_pinrst =
		(ISP_SWRST_PX2X_SWRST_MASK | ISP_SWRST_PX1X_SWRST_MASK |
		 ISP_SWRST_APB_SWRST_MASK);
	int32_t ret = 0;

	if (reset == ON) {
		/* reset state */
		if (get_chip_rev() != 0U) {
			/* CV8050C-595, CV8050C-689 */
			ret = tcc_isp_core_req_asbr_lp(state);
			if (ret < 0) {
				loge(&(state->pdev->dev),
				     "tcc_isp_core_req_asbr_lp returned %d\n",
				     ret);
			}
		}

		if (ret >= 0) {
			/* ISP pin reset (IM032B-35) */
			val = tcc_isp_readl(cfg_base, ISP_SWRST);
			val |= isp_pinrst;
			tcc_isp_writel(val, cfg_base, ISP_SWRST);
		}
	} else {
		/* reset release state */
		if (get_chip_rev() != 0U) {
			/* CV8050C-595, CV8050C-689 */
			ret = tcc_isp_core_rel_asbr_lp(state);
			if (ret < 0) {
				loge(&(state->pdev->dev),
				     "tcc_isp_core_rel_asbr_lp returned %d\n",
				     ret);
			}
		}

		if (ret >= 0) {
			/* ISP pin reset release */
			val = tcc_isp_readl(cfg_base, ISP_SWRST);
			val &= ~(isp_pinrst);
			tcc_isp_writel(val, cfg_base, ISP_SWRST);

			/* Wakeup ISP */
			tcc_isp_writel((SLEEP_MODE_SLEEP_MODE_DISABLE
					<< SLEEP_MODE_SLEEP_MODE_SHIFT),
				       isp_base, REG_ISP_SLEEP_MODE);
		}
	}

	return ret;
}

static void tcc_isp_core_set_io(const struct tcc_isp_state *state)
{
	/* set register update control */
	tcc_isp_core_set_up_mode(state);

	/* disable wdma(IM896A-22) */
	tcc_isp_core_set_wdma(state, OFF);

	if (get_chip_rev() != 0U) {
		/* memory sharing */
		tcc_isp_core_mem_share(state, state->mem_share);
	}

	/*
	 * ZELCOVA setting
	 */
	/* input */
	tcc_isp_core_set_input(state);
	/* output */
	tcc_isp_core_set_output(state);

	/* register update */
	tcc_isp_up_reg(state);
}

static void tcc_isp_core_set_tune(const struct tcc_isp_state *state)
{
	uint32_t i = 0U;

	memset_io(state->scene_data_base + ISP_MEM_OFFSET_DATA, 0,
		  ISP_MEM_OFFSET_CODE - ISP_MEM_OFFSET_DATA);

	for (i = 0U; i < state->tune_scene.isp_setting_cnt; i++) {
		/* set tune_scene value of ISP */
		tcc_isp_writel(state->tune_scene.setting[i].val,
			       state->scene_data_base,
			       state->tune_scene.setting[i].reg);
	}

	for (i = 0U; i < state->tune_core.isp_setting_cnt; i++) {
		/* set tune value of ISP */
		tcc_isp_writel(state->tune_core.setting[i].val, state->isp_base,
			       state->tune_core.setting[i].reg);
	}
	tcc_isp_up_reg(state);

	logi(&(state->pdev->dev), "complete %s\n", __func__);
}

void tcc_isp_core_set_default(struct tcc_isp_state *state)
{
	tcc_isp_core_padding(state, state->fmt[TCC_ISP_PAD_SINK].code);

	if (get_chip_rev() != 0U) {
		/*
		 * CV8050C-658
		 * set upper 16bit addr
		 */
		tcc_isp_core_set_apb(state);
	}

	/*
	 * set axi bus output disable
	 */
	/* set register update control */
	tcc_isp_core_set_up_mode(state);

	/* disable wdma(IM896A-22) */
	tcc_isp_core_set_wdma(state, OFF);

	/* register update */
	tcc_isp_up_reg(state);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
int32_t tcc_isp_core_get_reg(const struct tcc_isp_state *state,
			     const uint64_t reg, uint64_t *const val)
{
	const void __iomem *base = NULL;
	int32_t ret = 0;

	if ((reg & 0x10000U) == 0x10000U) {
		/* SW 3A Memory */
		base = state->scene_data_base;
	} else {
		/* HW ISP */
		base = state->isp_base;
	}

	/* host must get permission to access isp (TMPTG-257) */
	ret = tcc_isp_mcu_get_access_perm(state);
	if (ret < 0) {
		loge(&(state->pdev->dev),
		     "tcc_isp_mcu_get_access_perm returned %d\n", ret);
	}

	/* access isp */
	if (ret >= 0) {
		*val = tcc_isp_readl(base, (uint32_t)(reg & 0xFFFFU));

		/* release permission (TMPTG-257) */
		ret = tcc_isp_mcu_rel_access_perm(state);
		if (ret < 0) {
			/* error */
			loge(&(state->pdev->dev),
			     "tcc_isp_mcu_rel_access_perm returned %d\n", ret);
		} else {
			/* okay */
			ret = 4;
		}
	}

	return ret;
}

int32_t tcc_isp_core_set_reg(const struct tcc_isp_state *state,
			     const uint64_t reg, const uint64_t val)
{
	void __iomem *base = NULL;
	int32_t ret = 0;

	if ((reg & 0x10000U) == 0x10000U) {
		/* SW 3A Memory */
		base = state->scene_data_base;
	} else {
		/* HW ISP */
		base = state->isp_base;
	}

	/* host must get permission to access isp (TMPTG-257) */
	ret = tcc_isp_mcu_get_access_perm(state);
	if (ret < 0) {
		loge(&(state->pdev->dev),
		     "tcc_isp_mcu_get_access_perm returned %d\n", ret);
	}

	if (ret >= 0) {
		tcc_isp_writel((uint32_t)(val & 0xFFFFFFFFU), base,
			       (uint32_t)(reg & 0xFFFFU));

		/* release permission (TMPTG-257) */
		ret = tcc_isp_mcu_rel_access_perm(state);
		if (ret < 0) {
			loge(&(state->pdev->dev),
			     "tcc_isp_mcu_rel_access_perm returned %d\n", ret);
		} else {
			/* okay */
			ret = 4;
		}
	}

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
		if (tcc_isp_mcu_is_all_isp_stopped() == TCC_ISP_STOPPED) {
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
					     "tcc_isp_core_reset returned %d\n",
					     ret);
				}
			}
		} else {
			/* already reset has been done */
			logi(&(state->pdev->dev), "skip isp reset\n");
		}
	}

	return ret;
}
