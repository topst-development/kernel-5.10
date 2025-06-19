// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <soc/telechips/chipinfo.h>

#include <video/telechips/tcc_types.h>
#include <video/telechips/vioc_global.h>
#include <video/telechips/vioc_config.h>
#include <video/telechips/vioc_scaler.h>
#include <video/telechips/vioc_ddicfg.h> // is_VIOC_REMAP
#include <video/telechips/vioc_viqe.h>

static int debug_config;
#define dprintk(msg...)						\
	do {							\
		if (debug_config == 1) {					\
			(void)pr_info("[DBG][VIOC_CONFIG] " msg);	\
		}						\
	} while ((bool)0)

static int vioc_config_sc_rdma_sel[] = {
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8,
	0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF,
};

#if 0
static int vioc_config_sc_vin_sel[] = {
	0x10,
	0x12,
	0x0C,
	0x0E,
};
#endif

static int vioc_config_sc_wdma_sel[] = {
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
};

static int vioc_config_sc_disp_sel[] = {
	0x17, 0x18, 0x19,
};

static int vioc_config_viqe_rdma_sel[] = {
	-1, -1, -1, 0x0, -1, -1, -1, 0x1,
	-1, 0x02, 0x03, 0x04, 0x05, 0x06,
	0x07, 0x08, -1, -1,
};

//static int vioc_config_lut_rdma_sel[] = {
//	0,    0x1,  0x2,  0x03, 0x04, 0x05, -1, 0x07, 0x08, // rdma 0 ~ 8
//	0x09, 0x0A, 0x0B, 0x0C, 0x0D, -1,   -1, 0x11, 0x13,
//};

//static int vioc_config_lut_vin_sel[] = {
//	0x10,
//	-1,
//};
//static int vioc_config_lut_wdma_sel[] = {
//	0x14, 0x15, 0x16, 0x17, -1, 0x19, 0x1A, 0x1B, 0x1C,
//};

#if defined(CONFIG_VIOC_AFBCDEC)
static int vioc_config_AFBCDec_rdma_sel[] = {
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,  0x8,
	0x9, 0xA, 0xB, 0xC, 0xD,
};
#endif

/* support bypass mode DMA */
static const unsigned int bypassDMA[] = {
	/* RDMA */
	VIOC_RDMA00, VIOC_RDMA03, VIOC_RDMA04,
	VIOC_RDMA07,
	VIOC_RDMA08, VIOC_RDMA10, VIOC_RDMA12,
	0x00 // just for final recognition
};

static void __iomem *pIREQ_reg;

/* HIS_CALLING */
static void __iomem *CalcAddressViocComponent(unsigned int component)
{
	void __iomem *reg;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		reg = NULL;
		/* If necessary, add code and use it */
		//switch (get_vioc_index(component)) {
		//default:
		//	reg = NULL;
		//	break;
		//}
		break;
	case get_vioc_type(VIOC_SCALER):
		switch (get_vioc_index(component)) {
		case 0:
			reg = (pIREQ_reg + CFG_PATH_SC0_OFFSET);
			break;
		case 1:
			reg = (pIREQ_reg + CFG_PATH_SC1_OFFSET);
			break;
		case 2:
			reg = (pIREQ_reg + CFG_PATH_SC2_OFFSET);
			break;
		case 3:
			reg = (pIREQ_reg + CFG_PATH_SC3_OFFSET);
			break;
		case 4:
			reg = (pIREQ_reg + CFG_PATH_SC4_OFFSET);
			break;
		case 5:
			reg = (pIREQ_reg + CFG_PATH_SC5_OFFSET);
			break;
		case 6:
			reg = (pIREQ_reg + CFG_PATH_SC6_OFFSET);
			break;
		case 7:
			reg = (pIREQ_reg + CFG_PATH_SC7_OFFSET);
			break;
		default:
			reg = NULL;
			break;
		}
		break;
	case get_vioc_type(VIOC_VIQE):
		switch (get_vioc_index(component)) {
		case 0:
			reg = (pIREQ_reg + CFG_PATH_VIQE0_OFFSET);
			break;
		default:
			reg = NULL;
			break;
		}
		break;
#ifdef CONFIG_VIOC_MAP_DECOMP
	case get_vioc_type(VIOC_MC):
		reg = (pIREQ_reg + CFG_PATH_MC_OFFSET);
		/* If necessary, add code and use it */
		//switch (get_vioc_index(component)) {
		//default:
		//	reg = NULL;
		//	break;
		//}
		break;
#endif
	default:
		(void)pr_err("[ERR][VIOC_CONFIG] %s-%d: wierd component(0x%x) type(0x%x) index(%d)\n",
		       __func__, __LINE__, component, get_vioc_type(component),
		       get_vioc_index(component));

		WARN_ON(1);
		reg = NULL;
		break;
	}

	return reg;
}

static int CheckScalerPathSelection(unsigned int component)
{
	int ret = 0;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		ret = vioc_config_sc_rdma_sel[get_vioc_index(component)];
		break;
#if 0
	case get_vioc_type(VIOC_VIN):
		ret = vioc_config_sc_vin_sel[get_vioc_index(component) / 2U];
		break;
#endif
	case get_vioc_type(VIOC_WDMA):
		ret = vioc_config_sc_wdma_sel[get_vioc_index(component)];
		if (get_chip_rev() == 0) {
			if (get_vioc_index(component) > get_vioc_index(VIOC_WDMA01)) {
				(void)pr_err("[ERR][VIOC_CONFIG] %s, Scaler path for WDMA is not available.\n",__func__);
				ret = -1;
				goto exit_func;
			}
		}
		break;
	case get_vioc_type(VIOC_DISP):
		ret = vioc_config_sc_disp_sel[get_vioc_index(component)];
		if (get_chip_rev() == 0) {
			(void)pr_err("[ERR][VIOC_CONFIG] %s, Scaler path for DISP is not available.\n",__func__);
			ret = -1;
			goto exit_func;
		}
		break;
	default:
		(void)pr_err("[ERR][VIOC_CONFIG] %s, wrong path parameter(0x%08x)\n",
		       __func__, component);
		ret = -1;
		break;
	}
exit_func:
	return ret;
}

static int CheckViqePathSelection(unsigned int component)
{
	int ret = 0;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		ret = vioc_config_viqe_rdma_sel[get_vioc_index(component)];
		break;
	default:
		(void)pr_err("[ERR][VIOC_CONFIG] %s, wrong path parameter(0x%08x)\n",
		       __func__, component);
		ret = -1;
		break;
	}
	return ret;
}

#ifdef CONFIG_VIOC_PIXEL_MAPPER
int CheckPixelMapPathSelection(unsigned int component)
{
	int ret = 0;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		ret = vioc_config_pixel_mapper_rdma_sel[get_vioc_index(
			component)];
		break;
#if 0
	case get_vioc_type(VIOC_VIN):
		ret = vioc_config_pixel_mapper_vin_sel
			[get_vioc_index(component) / 2U];
		break;
#endif
	default:
		(void)pr_err("[ERR][VIOC_CONFIG] %s, wrong path parameter(0x%08x)\n",
		       __func__, component);
		ret = -1;
		break;
	}
	return ret;
}
#endif //

#if defined(CONFIG_VIOC_AFBCDEC) || defined(CONFIG_VIOC_PVRIC_FBDC)
static int CheckFBCDecPathSelection(unsigned int component)
{
	int ret = 0;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		ret = vioc_config_AFBCDec_rdma_sel[get_vioc_index(component)];
		break;
	default:
		(void)pr_err("[ERR][VIOC_CONFIG] %s, wrong path parameter(0x%08x)\n",
		       __func__, component);
		ret = -1;
		break;
	}
	return ret;
}
#endif

#ifdef CONFIG_VIOC_MAP_DECOMP
static int CheckMCPathSelection(unsigned int component, unsigned int mc)
{
	int ret = 0;

	//Component Check
	if (get_vioc_type(component) != get_vioc_type(VIOC_RDMA)) {
		/* Prevent KCS warning */
		(void)pr_err("[ERR][VIOC_CONFIG] %s, ret:%d wrong path parameter(Path: 0x%08x MC: 0x%08x)\n",
		       __func__, ret, component, mc);
		ret = -EINVAL;
	}

	return ret;
}

static void CheckMCOverlapping(unsigned int mc)
{
	unsigned int val = 0;
	unsigned int mc0_val = 0, mc1_val = 0;
	unsigned int check_point = 0;
	void __iomem *reg = (pIREQ_reg + CFG_PATH_MC_OFFSET);

	mc0_val = ((__raw_readl(reg) & CFG_PATH_MC_MC0_SEL_MASK) >> CFG_PATH_MC_MC0_SEL_SHIFT);
	mc1_val = ((__raw_readl(reg) & CFG_PATH_MC_MC1_SEL_MASK) >> CFG_PATH_MC_MC1_SEL_SHIFT);

	if ((mc0_val == mc1_val) && (mc == VIOC_MC0)) {
		if (mc0_val < 8U) {
			val = (__raw_readl(reg));
			//clear
			val &= ~(CFG_PATH_MC_MC1_SEL_MASK);
			//set
			val |= ((u32)(8U) << (CFG_PATH_MC_MC1_SEL_SHIFT));
			__raw_writel(val, reg);
		} else {
			val = (__raw_readl(reg));
			//clear
			val &= ~(CFG_PATH_MC_MC1_SEL_MASK);
			//set
			__raw_writel(val, reg);
		}
	} else if ((mc0_val == mc1_val) && (mc == VIOC_MC1)) {
		if (mc1_val < 8U) {
			val = (__raw_readl(reg));
			//clear
			val &= ~(CFG_PATH_MC_MC0_SEL_MASK);
			//set
			val |= ((u32)(mc1_val + 1U) << (CFG_PATH_MC_MC0_SEL_SHIFT));
			__raw_writel(val, reg);
		} else {
			val = (__raw_readl(reg));
			//clear
			val &= ~(CFG_PATH_MC_MC0_SEL_MASK);
			//set
			__raw_writel(val, reg);
		}
	} else {
		check_point = 1;
	}

	if (check_point == 0) {
		(void)pr_info("[INF][VIOC_CONFIG]%s: identified overlapping in the MC Component and will change the path.\n",__func__);
	}
}
#endif

/* HIS_GOTO */
int VIOC_AUTOPWR_Enalbe(unsigned int component, unsigned int onoff)
{
	int shift_bit = -1;
	u32 value = 0U;
	int ret = -1;

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_RDMA):
		shift_bit = (int)PWR_AUTOPD_RDMA_SHIFT;
		ret = 0;
		break;
#ifdef CONFIG_VIOC_MAP_DECOMP
	case get_vioc_type(VIOC_MC):
		shift_bit = (int)PWR_AUTOPD_MC_SHIFT;
		ret = 0;
		break;
#endif //
	case get_vioc_type(VIOC_WMIX):
		shift_bit = (int)PWR_AUTOPD_MIX_SHIFT;
		ret = 0;
		break;
	case get_vioc_type(VIOC_WDMA):
		shift_bit = (int)PWR_AUTOPD_WDMA_SHIFT;
		ret = 0;
		break;
	case get_vioc_type(VIOC_SCALER):
		shift_bit = (int)PWR_AUTOPD_SC_SHIFT;
		ret = 0;
		break;
	case get_vioc_type(VIOC_VIQE):
		shift_bit = (int)PWR_AUTOPD_VIQE_SHIFT;
		ret = 0;
		break;
	default:
		(void)pr_err("[ERR][VIOC_CONFIG] %s, in error, Type :0x%x onoff:%d\n",
	       __func__, component, onoff);
		ret = -1;
		break;
	}

	if (ret < 0) {
		goto FUNC_EXIT;
	}

	/* shift_bit >= 0, Always */
	if (onoff != 0U) {
		value =
			(__raw_readl(pIREQ_reg + PWR_AUTOPD_OFFSET)
			 | (((u32)1U) << (unsigned int)shift_bit));
	} else {
		value =
			(__raw_readl(pIREQ_reg + PWR_AUTOPD_OFFSET)
			 & ~(((u32)1U) << (unsigned int)shift_bit));
	}

	__raw_writel(value, pIREQ_reg + PWR_AUTOPD_OFFSET);

	ret = 0;

FUNC_EXIT:
	return ret;
}
EXPORT_SYMBOL(VIOC_AUTOPWR_Enalbe);

/* HIS_GOTO */ /* HIS_CCM */ /* HIS_CALLS */
int VIOC_CONFIG_PlugIn(unsigned int component, unsigned int select)
{
	u32 value = 0;
	unsigned int loop = 0;
	void __iomem *reg;
	int plugin_path;
	int ret = (int)VIOC_PATH_DISCONNECTED;

	reg = CalcAddressViocComponent(component);
	if (reg == NULL) {
		/* Prevent KCS warning */
		ret = VIOC_DEVICE_INVALID;

		goto FUNC_EXIT;
	}

	/* Check selection has type value. If has, select value is invalid */
	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_SCALER):
		plugin_path = CheckScalerPathSelection(select);
		if (plugin_path < 0) {
			/* Prevent KCS warning */
			ret = VIOC_DEVICE_INVALID;
		} else {
			ret = (int)VIOC_PATH_DISCONNECTED;
		}
		break;
	case get_vioc_type(VIOC_VIQE):
		plugin_path = CheckViqePathSelection(select);
		if (plugin_path < 0) {
			/* Prevent KCS warning */
			ret = VIOC_DEVICE_INVALID;
		} else {
			ret = (int)VIOC_PATH_DISCONNECTED;
		}
		break;
	default:
		ret = VIOC_DEVICE_INVALID;
		break;
	}

	if (ret == VIOC_DEVICE_INVALID) {
		goto FUNC_EXIT;
	}

	value = ((__raw_readl(reg) & CFG_PATH_STS_MASK) >> CFG_PATH_STS_SHIFT);
	if (value == VIOC_PATH_CONNECTED) {
		value =
			((__raw_readl(reg) & CFG_PATH_SEL_MASK)
			 >> CFG_PATH_SEL_SHIFT);
		if (value != (unsigned int)plugin_path) {
			(void)pr_warn("[WAN][VIOC_CONFIG] %s, VIOC(T:%d I:%d) is plugged-out by force (from 0x%08x to %d)!!\n",
				__func__, get_vioc_type(component),
				get_vioc_index(component), value, plugin_path);
			(void)VIOC_CONFIG_PlugOut(component);
		}
	}

	value = (__raw_readl(reg) & ~(CFG_PATH_SEL_MASK | CFG_PATH_EN_MASK));
	value |= (((unsigned int)plugin_path << CFG_PATH_SEL_SHIFT) | ((u32)0x1U << CFG_PATH_EN_SHIFT));
	__raw_writel(value, reg);

	if ((__raw_readl(reg) & CFG_PATH_ERR_MASK) != 0U) {
		(void)pr_err("[ERR][VIOC_CONFIG] %s, path configuration error(ERR_MASK). device is busy. VIOC(T:%d I:%d) Path:%d\n",
		       __func__, get_vioc_type(component),
		       get_vioc_index(component), plugin_path);
		value = (__raw_readl(reg) & ~(CFG_PATH_EN_MASK));
		__raw_writel(value, reg);
	}

	loop = 50U;
	while ((bool)1) {
		unsigned int ext_flag = 0;

		mdelay(1);
		loop--;
		value =
			((__raw_readl(reg) & CFG_PATH_STS_MASK)
			 >> CFG_PATH_STS_SHIFT);
		if (value == VIOC_PATH_CONNECTED) {
			ret = (int)VIOC_PATH_CONNECTED;
			ext_flag = 1U;
		} else {
			if (loop < 1U) {
				(void)pr_err("[ERR][VIOC_CONFIG] %s, path configuration error(TIMEOUT). device is busy. VIOC(T:%d I:%d) Path:%d\n",
				       __func__, get_vioc_type(component),
				       get_vioc_index(component), plugin_path);
				ret = (int)VIOC_DEVICE_BUSY;
				ext_flag = 1U;
			}
		}
		if (ext_flag == 1U) {
			break;
		}
	}

FUNC_EXIT:
	if (ret != (int)VIOC_PATH_CONNECTED) {
		(void)pr_err("[ERR][VIOC_CONFIG] %s, in error, Type :0x%x Select:0x%x cfg_reg:0x%x\n",
			__func__, component, select, __raw_readl(reg));
		/* If you want to find the caller when an error occurs, uncomment it. */
		//WARN_ON(1);
	}
	return ret;
}
EXPORT_SYMBOL(VIOC_CONFIG_PlugIn);

/* HIS_GOTO */
int VIOC_CONFIG_PlugOut(unsigned int component)
{
	u32 value = 0U;
	unsigned int loop = 0U;
	void __iomem *reg;
	int ret = (int)VIOC_PATH_CONNECTED;

	reg = CalcAddressViocComponent(component);
	if (reg == NULL) {
		ret = VIOC_DEVICE_INVALID;

		goto FUNC_EXIT;
	}

	value = ((__raw_readl(reg) & CFG_PATH_STS_MASK) >> CFG_PATH_STS_SHIFT);
	if (value == VIOC_PATH_DISCONNECTED) {
		__raw_writel(0x00000000, reg);
		(void)pr_warn("[WAN][VIOC_CONFIG] %s, VIOC(T:%d I:%d) was already plugged-out!!\n",
			__func__, get_vioc_type(component),
			get_vioc_index(component));
		ret = (int)VIOC_PATH_DISCONNECTED;

		goto FUNC_EXIT;
	}

	value = (__raw_readl(reg) & ~(CFG_PATH_EN_MASK));
	__raw_writel(value, reg);

	if ((__raw_readl(reg) & CFG_PATH_ERR_MASK) != 0U) {
		(void)pr_err("[ERR][VIOC_CONFIG] %s, path configuration error(ERR_MASK). device is busy. VIOC(T:%d I:%d)\n",
		       __func__, get_vioc_type(component),
		       get_vioc_index(component));
		value = (__raw_readl(reg) & ~(CFG_PATH_EN_MASK));
		__raw_writel(value, reg);
		ret = (int)VIOC_DEVICE_BUSY;

		goto FUNC_EXIT;
	}

	loop = 100U;
	while ((bool)1) {
		unsigned int ext_flag = 0;

		mdelay(1);
		loop--;
		value =
			((__raw_readl(reg) & CFG_PATH_STS_MASK)
			 >> CFG_PATH_STS_SHIFT);
		if (value == VIOC_PATH_DISCONNECTED) {
			__raw_writel(0x00000000, reg);
			ret = (int)VIOC_PATH_DISCONNECTED;
			ext_flag = 1U;
		} else {
			if (loop < 1U) {
				(void)pr_err("[ERR][VIOC_CONFIG] %s, path configuration error(TIMEOUT). device is busy. VIOC(T:%d I:%d)\n",
				       __func__, get_vioc_type(component),
				       get_vioc_index(component));
				ret = VIOC_DEVICE_BUSY;
				ext_flag = 1U;
			}
		}
		if (ext_flag == 1U) {
			break;
		}
	}

FUNC_EXIT:
	if (ret != (int)VIOC_PATH_DISCONNECTED) {
		(void)pr_err("[ERR][VIOC_CONFIG] %s, in error, Type :0x%x cfg_reg:0x%x\n",
			__func__, component, __raw_readl(reg));

		/* If you want to find the caller when an error occurs, uncomment it. */
		//WARN_ON(1);

	}
	return ret;
}
EXPORT_SYMBOL(VIOC_CONFIG_PlugOut);

/* HIS_CCM */
int VIOC_CONFIG_WMIXPath(unsigned int component_num, unsigned int wmixmode)
{
	/* mode - 0: BY-PSSS PATH, 1: WMIX PATH */
	u32 value;
	unsigned int i, shift_mix_path, support_bypass = 0;
	void __iomem *config_reg = pIREQ_reg;
	int ret = -1;

	for (i = 0; i < (sizeof(bypassDMA) / sizeof(unsigned int)); i++) {
		if (component_num == bypassDMA[i]) {
			support_bypass = 1U;
			break;
		}
	}

	if (support_bypass == 1U) {
		shift_mix_path = 0xFFU; // ignore value

		switch (get_vioc_type(component_num)) {
		case get_vioc_type(VIOC_RDMA):
			switch (get_vioc_index(component_num)) {
			case get_vioc_index(VIOC_RDMA00):
				shift_mix_path = CFG_MISC0_MIX00_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA03):
				shift_mix_path = CFG_MISC0_MIX03_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA04):
				shift_mix_path = CFG_MISC0_MIX10_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA07):
				shift_mix_path = CFG_MISC0_MIX13_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA08):
				shift_mix_path = CFG_MISC0_MIX20_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA10):
				shift_mix_path = CFG_MISC0_MIX30_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA12):
				shift_mix_path = CFG_MISC0_MIX40_SHIFT;
				break;
			default:
				(void)pr_err("[ERR][VIOC_CONFIG] %s: wrong component type(0x%x) index(%d)\n",
				__func__, get_vioc_type(component_num), get_vioc_index(component_num));
				break;
			}
			break;
		default:
			(void)pr_err("[ERR][VIOC_CONFIG] %s: wrong component type(0x%x) index(%d)\n",
		       __func__, get_vioc_type(component_num), get_vioc_index(component_num));
			break;
		}

		if (shift_mix_path != 0xFFU) {
			VIOC_CONFIG_WMIXPathReset(component_num, 1);
			value = __raw_readl(config_reg + CFG_MISC0_OFFSET)
				& ~((u32)1U << shift_mix_path);
			if (wmixmode != 0U) {
				/* Prevent KCS warning */
				value |= (u32)1U << shift_mix_path;
			}

			__raw_writel(value, config_reg + CFG_MISC0_OFFSET);
			VIOC_CONFIG_WMIXPathReset(component_num, 0);
		}

		ret = 0;
	} else {
		dprintk("%s: vioc(0x%x) doesn't support mixer bypass(%d) mode\n",
			__func__, component_num, support_bypass);

		ret = -1;
	}

	return ret;
}
EXPORT_SYMBOL(VIOC_CONFIG_WMIXPath);

int VIOC_CONFIG_GetWMIXPath(unsigned int component_num)
{
	/* mode - 0: BY-PSSS PATH, 1: WMIX PATH */
	int ret = -1;
	unsigned int i, shift_mix_path, mask_mix_path, support_bypass = 0U;
	const void __iomem *config_reg = pIREQ_reg;

	for (i = 0; i < (sizeof(bypassDMA) / sizeof(unsigned int)); i++) {
		if (component_num == bypassDMA[i]) {
			support_bypass = 1U;
			break;
		}
	}

	if (support_bypass == 1U) {
		shift_mix_path = 0xFFU; // ignore value
		mask_mix_path = 0U;

		switch (get_vioc_type(component_num)) {
		case get_vioc_type(VIOC_RDMA):
			switch (get_vioc_index(component_num)) {
			case get_vioc_index(VIOC_RDMA00):
				shift_mix_path = CFG_MISC0_MIX00_SHIFT;
				mask_mix_path = CFG_MISC0_MIX00_MASK;
				break;
			case get_vioc_index(VIOC_RDMA03):
				shift_mix_path = CFG_MISC0_MIX03_SHIFT;
				mask_mix_path = CFG_MISC0_MIX03_MASK;
				break;
			case get_vioc_index(VIOC_RDMA04):
				shift_mix_path = CFG_MISC0_MIX10_SHIFT;
				mask_mix_path = CFG_MISC0_MIX10_MASK;
				break;

			case get_vioc_index(VIOC_RDMA07):
				shift_mix_path = CFG_MISC0_MIX13_SHIFT;
				mask_mix_path = CFG_MISC0_MIX13_MASK;
				break;
			case get_vioc_index(VIOC_RDMA08):
				shift_mix_path = CFG_MISC0_MIX20_SHIFT;
				mask_mix_path = CFG_MISC0_MIX20_MASK;
				break;
			case get_vioc_index(VIOC_RDMA10):
				shift_mix_path = CFG_MISC0_MIX30_SHIFT;
				mask_mix_path = CFG_MISC0_MIX30_MASK;
				break;
			case get_vioc_index(VIOC_RDMA12):
				shift_mix_path = CFG_MISC0_MIX40_SHIFT;
				mask_mix_path = CFG_MISC0_MIX40_MASK;
				break;
			default:
				(void)pr_err("[ERR][VIOC_CONFIG] %s: wrong component type(0x%x) index(%d)\n",
				__func__, get_vioc_type(component_num), get_vioc_index(component_num));
				ret = -1;
				break;
			}
			break;
		default:
			(void)pr_err("[ERR][VIOC_CONFIG] %s: wrong component type(0x%x) index(%d)\n",
		       __func__, get_vioc_type(component_num), get_vioc_index(component_num));
			ret = -1;
			break;
		}

		if (shift_mix_path != 0xFFU) {
			unsigned int temp = 0U; /* avoid MISRA C-2012 Rule 10.8 */

			temp = (__raw_readl(config_reg + CFG_MISC0_OFFSET)
					& mask_mix_path) >> shift_mix_path;
			ret = (int)temp;
		}
	} else {
		dprintk("%s: vioc(0x%x) doesn't support mixer bypass(%d) mode\n",
			__func__, component_num, support_bypass);
		ret = -1;
	}

	return ret;
}
EXPORT_SYMBOL(VIOC_CONFIG_GetWMIXPath);

/*
 * The WMIX is divided into the mix and the bypass path.
 * VIOC_CONFIG_SWReset() is only used to reset the mix path of WMIX.
 * VIOC_CONFIG_WMIXPathReset() is only used to reset the bypass path of WMIX.
 *
 * VIOC_CONFIG_SWReset(component, resetmode)
 *    - Reset the mix path and other VIOC components
 *    - PWR_BLK_SWR1 register
 *    - component: WMIX VIOC ID (ex. VIOC_WMIX0, VIOC_WMIX1 etc...)
 *    - resetmode: 1 is reset mode, 0 is normal mode
 *
 * VIOC_CONFIG_WMIXPathReset(component, resetmode)
 *    - Reset the bypass path
 *    - WMIX_PATH_SWR register
 *    - component: Input Layer VIOC ID of WMIX (ex. VIOC_RDMA03, VIOC_VIN10 etc...)
 *    - resetmode: 1 is reset mode, 0 is normal mode
 */
void VIOC_CONFIG_WMIXPathReset(unsigned int component_num, unsigned int reset)
{
	/* reset - 0 :Normal, 1:Mixing PATH reset */
	u32 value;
	unsigned int i, shift_mix_path, support_bypass = 0U;
	//unsigned int shift_vin_rdma_path;
	void __iomem *config_reg = pIREQ_reg;

	for (i = 0; i < (sizeof(bypassDMA) / sizeof(unsigned int)); i++) {
		if (component_num == bypassDMA[i]) {
			support_bypass = 1;
			break;
		}
	}

	if (support_bypass == 1U) {
		shift_mix_path = 0xFFU; // ignore value
		//shift_vin_rdma_path = 0xFF;

		switch (get_vioc_type(component_num)) {
		case get_vioc_type(VIOC_RDMA):
			switch (get_vioc_index(component_num)) {
			case get_vioc_index(VIOC_RDMA00):
				shift_mix_path = WMIX_PATH_SWR_MIX00_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA03):
				shift_mix_path = WMIX_PATH_SWR_MIX03_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA04):
				shift_mix_path = WMIX_PATH_SWR_MIX10_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA07):
				shift_mix_path = WMIX_PATH_SWR_MIX13_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA08):
				shift_mix_path = WMIX_PATH_SWR_MIX20_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA10):
				shift_mix_path = WMIX_PATH_SWR_MIX30_SHIFT;
				break;
			case get_vioc_index(VIOC_RDMA12):
				shift_mix_path = WMIX_PATH_SWR_MIX40_SHIFT;
				break;
			default:
				(void)pr_err("[ERR][VIOC_CONFIG] %s: wrong component type(0x%x) index(%d)\n",
				__func__, get_vioc_type(component_num), get_vioc_index(component_num));
				break;
			}
			break;
		default:
			(void)pr_err("[ERR][VIOC_CONFIG] %s: wrong component type(0x%x) index(%d)\n",
				__func__, get_vioc_type(component_num), get_vioc_index(component_num));
			break;
		}

		if (shift_mix_path != 0xFFU) {
			value = __raw_readl(
					config_reg + CFG_WMIX_PATH_SWR_OFFSET)
					& ~((u32)1U << shift_mix_path);
			if (reset == 1U) {
				/* Prevent KCS warning */
				value |= (u32)1U << shift_mix_path;
			}
			__raw_writel(
				value, config_reg + CFG_WMIX_PATH_SWR_OFFSET);
		}
	}
}
EXPORT_SYMBOL(VIOC_CONFIG_WMIXPathReset);

#if defined(CONFIG_VIOC_AFBCDEC) || defined(CONFIG_VIOC_PVRIC_FBDC)
/* HIS_GOTO */
int VIOC_CONFIG_FBCDECPath(
	unsigned int FBCDecPath, unsigned int rdmaPath, unsigned int on)
{
	u32 value = 0;
	int select = 0;

	void __iomem *reg = (pIREQ_reg + CFG_FBC_DEC_SEL_OFFSET);
	int ret = (int)VIOC_PATH_DISCONNECTED;

	/* Check selection has type value. If has, select value is invalid */
	select = CheckFBCDecPathSelection(rdmaPath);
	if (select < 0) {
		ret = VIOC_DEVICE_INVALID;

		goto FUNC_EXIT;
	}

	/*Check RDMA and AFBC_DEC operations.*/
	if (on == 1U) {
		value =
			(__raw_readl(reg)
			 & ~(((u32)0x1U << (unsigned int)select)
			     << CFG_FBC_DEC_SEL_AXSM_SEL_SHIFT));
	} else {
		value =
			(__raw_readl(reg)
			 | (((u32)0x1U << (unsigned int)select)
			    << CFG_FBC_DEC_SEL_AXSM_SEL_SHIFT));
	}

	switch (FBCDecPath) {
	case VIOC_FBCDEC0:
		if (on == 1U) {
			value &= ~CFG_FBC_DEC_SEL_AD_PATH_SEL00_MASK;
			value |=
				((unsigned int)select
				 << CFG_FBC_DEC_SEL_AD_PATH_SEL00_SHIFT);
		} else {
			/* Prevent KCS warning */
			value |= ((u32)0x1F << CFG_FBC_DEC_SEL_AD_PATH_SEL00_SHIFT);
		}
		break;
	case VIOC_FBCDEC1:
		if (on == 1U) {
			value &= ~CFG_FBC_DEC_SEL_AD_PATH_SEL01_MASK;
			value |=
				((unsigned int)select
				 << CFG_FBC_DEC_SEL_AD_PATH_SEL01_SHIFT);
		} else {
			/* Prevent KCS warning */
			value |= ((u32)0x1FU << CFG_FBC_DEC_SEL_AD_PATH_SEL01_SHIFT);
		}
		break;
	default:
		ret = VIOC_DEVICE_INVALID;
		break;
	};

	if (ret == VIOC_DEVICE_INVALID) {
		goto FUNC_EXIT;
	}
	// pr_debug("[DBG][VIOC_CONFIG] %s RDMA(%d) with FBC(%d) :: On(%d)-
	// 0x%08lx \n", __func__, get_vioc_index(rdmaPath),
	// get_vioc_index(FBCDecPath), on, value);
	__raw_writel(value, reg);
	ret = (int)VIOC_PATH_CONNECTED;

FUNC_EXIT:
	if (ret == VIOC_DEVICE_INVALID) {
		(void)pr_err("[ERR][VIOC_CONFIG] %s, in error, FBCDecPath(%d) :0x%x rdmaPath:0x%x cfg_reg:0x%x\n",
			__func__, on, get_vioc_index(FBCDecPath), rdmaPath,
			__raw_readl(reg));

		WARN_ON(1);
	}
	return ret;
}
EXPORT_SYMBOL(VIOC_CONFIG_FBCDECPath);

int VIOC_CONFIG_GetFBDCPath(unsigned int fbcdec_id)
{
	int ret = -1;
	unsigned int mask;
	unsigned int shift;
	const void __iomem *reg = (pIREQ_reg + CFG_FBC_DEC_SEL_OFFSET);

	switch (fbcdec_id) {
	case VIOC_FBCDEC0:
		mask = CFG_FBC_DEC_SEL_AD_PATH_SEL00_MASK;
		shift = CFG_FBC_DEC_SEL_AD_PATH_SEL00_SHIFT;
		ret = 0;
		break;
	case VIOC_FBCDEC1:
		mask = CFG_FBC_DEC_SEL_AD_PATH_SEL01_MASK;
		shift = CFG_FBC_DEC_SEL_AD_PATH_SEL01_SHIFT;
		ret = 0;
		break;
	default:
		(void)pr_err("[ERR][VIOC_CONFIG] %s, in error, FBCDEC_ID :0x%x \n",
			__func__, fbcdec_id);
		ret = -1;
		break;
	}

	if (ret == 0) {
		unsigned int temp = 0U; /* avoid MISRA C-2012 Rule 10.8 */
		temp = ((__raw_readl(reg) & mask) >> shift) + VIOC_RDMA00;
		ret = (int)temp;
	}
	return (int)ret;
}
EXPORT_SYMBOL(VIOC_CONFIG_GetFBDCPath);
#endif

#ifdef CONFIG_VIOC_MAP_DECOMP
/*
 * VIOC_CONFIG_MCPath
 * Connect Mapconverter or RDMA block on component path
 * component : VIOC_RDMA03, VIOC_RDMA07, VIOC_RDMA09, VIOC_RDMA10, VIOC_RDMA11, VIOC_RDMA12, VIOC_RDMA13, VIOC_RDMA14, VIOC_RDMA15
 * mc : VIOC_MC0 VIOC_MC1
 */
/* HIS_GOTO */ /* HIS_CCM */
int VIOC_CONFIG_MCPath(unsigned int component, unsigned int mc)
{
	int ret = 0;
	unsigned int value = 0;
	unsigned int mc_sel_mask = 0;
	unsigned int mc_sel_shift = 0;
	void __iomem *reg = (pIREQ_reg + CFG_PATH_MC_OFFSET);

	if (CheckMCPathSelection(component, mc) < 0) {
		ret = -EINVAL;

		goto FUNC_EXIT;
	}

	/* MC Mask/Shift Value Selection */
	switch (mc) {
		case VIOC_MC0:
			mc_sel_mask = CFG_PATH_MC_MC0_SEL_MASK;
			mc_sel_shift = CFG_PATH_MC_MC0_SEL_SHIFT;
			break;
		case VIOC_MC1:
			mc_sel_mask = CFG_PATH_MC_MC1_SEL_MASK;
			mc_sel_shift = CFG_PATH_MC_MC1_SEL_SHIFT;
			break;
		default:
			break;
	}

	switch (component) {
	case VIOC_RDMA03:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD03_MASK) != 0U) {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg));
				//clear
				value &= ~((CFG_PATH_MC_RD03_MASK) | (mc_sel_mask));
				//set
				value |= (((u32)0x1U << CFG_PATH_MC_RD03_SHIFT) | ((u32)0x0U << mc_sel_shift));
				__raw_writel(value, reg);
			}
		} else {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD03_MASK) == 0U) {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD03_MASK));
				__raw_writel(value, reg);
			}
		}
		break;
	case VIOC_RDMA07:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD07_MASK) != 0U) {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg));
				//clear
				value &= ~((CFG_PATH_MC_RD07_MASK) | (mc_sel_mask));
				//set
				value |= (((u32)0x1U << CFG_PATH_MC_RD07_SHIFT) | ((u32)0x1U << mc_sel_shift));
				__raw_writel(value, reg);
			}
		} else {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD07_MASK) == 0U)  {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD07_MASK));
				__raw_writel(value, reg);
			}
		}
		break;
	case VIOC_RDMA09:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD09_MASK) != 0U) {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg));
				//clear
				value &= ~((CFG_PATH_MC_RD09_MASK) | (mc_sel_mask));
				//set
				value |= (((u32)0x1U << CFG_PATH_MC_RD09_SHIFT) | ((u32)0x2U << mc_sel_shift));
				__raw_writel(value, reg);
			}
		} else {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD09_MASK) == 0U) {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD09_MASK));
				__raw_writel(value, reg);
			}
		}
		break;
	case VIOC_RDMA10:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD10_MASK) != 0U)  {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg));
				//clear
				value &= ~((CFG_PATH_MC_RD10_MASK) | (mc_sel_mask));
				//set
				value |= (((u32)0x1U << CFG_PATH_MC_RD10_SHIFT) | ((u32)0x3U << mc_sel_shift));
				__raw_writel(value, reg);
			}
		} else {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD10_MASK) == 0U) {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD10_MASK));
				__raw_writel(value, reg);
			}
		}
		break;
	case VIOC_RDMA11:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD11_MASK) != 0U)  {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg));
				//clear
				value &= ~((CFG_PATH_MC_RD11_MASK) | (mc_sel_mask));
				//set
				value |= (((u32)0x1U << CFG_PATH_MC_RD11_SHIFT) | ((u32)0x4U << mc_sel_shift));
				__raw_writel(value, reg);
			}
		} else {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD11_MASK) == 0U) {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD11_MASK));
				__raw_writel(value, reg);
			}
		}
		break;
	case VIOC_RDMA12:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD12_MASK) != 0U)  {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg));
				//clear
				value &= ~((CFG_PATH_MC_RD12_MASK) | (mc_sel_mask));
				//set
				value |= (((u32)0x1U << CFG_PATH_MC_RD12_SHIFT) | ((u32)0x5U << mc_sel_shift));
				__raw_writel(value, reg);
			}
		} else {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD12_MASK) == 0U) {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD12_MASK));
				__raw_writel(value, reg);
			}
		}
		break;
	case VIOC_RDMA13:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD13_MASK) != 0U)  {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg));
				//clear
				value &= ~((CFG_PATH_MC_RD13_MASK) | (mc_sel_mask));
				//set
				value |= (((u32)0x1U << CFG_PATH_MC_RD13_SHIFT) | ((u32)0x6U << mc_sel_shift));
				__raw_writel(value, reg);
			}
		} else {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD13_MASK) == 0U) {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD13_MASK));
				__raw_writel(value, reg);
			}

		}
		break;
	case VIOC_RDMA14:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD14_MASK) != 0U)  {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg));
				//clear
				value &= ~((CFG_PATH_MC_RD14_MASK) | (mc_sel_mask));
				//set
				value |= (((u32)0x1U << CFG_PATH_MC_RD14_SHIFT) | ((u32)0x7U << mc_sel_shift));
				__raw_writel(value, reg);
			}
		} else {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD14_MASK) == 0U) {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD14_MASK));
				__raw_writel(value, reg);
			}

		}
		break;
	case VIOC_RDMA15:
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD15_MASK) != 0U)  {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg));
				//clear
				value &= ~((CFG_PATH_MC_RD15_MASK) | (mc_sel_mask));
				//set
				value |= (((u32)0x1U << CFG_PATH_MC_RD15_SHIFT) | ((u32)0x8U << mc_sel_shift));
				__raw_writel(value, reg);
			}
		} else {
			if ((__raw_readl(reg) & CFG_PATH_MC_RD15_MASK) == 0U) {
				/* Prevent KCS warning */
			} else {
				value = (__raw_readl(reg) & ~(CFG_PATH_MC_RD15_MASK));
				__raw_writel(value, reg);
			}

		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (ret == 0) {
		if (get_vioc_type(mc) == get_vioc_type(VIOC_MC)) {
			CheckMCOverlapping(mc);
		}
	}

FUNC_EXIT:
	if (ret < 0) {
		(void)pr_err("[ERR][VIOC_CONFIG] %s, in error, Path: 0x%x MC: %x cfg_reg:0x%x\n",
		       __func__, component, mc, __raw_readl(reg));
	}
	return ret;
}
EXPORT_SYMBOL(VIOC_CONFIG_MCPath);
#endif

/*
 * The WMIX is divided into the mix and the bypass path.
 * VIOC_CONFIG_SWReset() is only used to reset the mix path of WMIX.
 * VIOC_CONFIG_WMIXPathReset() is only used to reset the bypass path of WMIX.
 *
 * VIOC_CONFIG_SWReset(component, resetmode)
 *    - Reset the mix path and other VIOC components
 *    - PWR_BLK_SWR1 register
 *    - component: WMIX VIOC ID (ex. VIOC_WMIX0, VIOC_WMIX1 etc...)
 *    - resetmode: 1 is reset mode, 0 is normal mode
 *
 * VIOC_CONFIG_WMIXPathReset(component, resetmode)
 *    - Reset the bypass path
 *    - WMIX_PATH_SWR register
 *    - component: Input Layer VIOC ID of WMIX (ex. VIOC_RDMA03, VIOC_VIN10 etc...)
 *    - resetmode: 1 is reset mode, 0 is normal mode
 */
/*HIS_GOTO */ /*HIS_CCM */
void VIOC_CONFIG_SWReset(unsigned int component, unsigned int resetmode)
{
	u32 value;
	void __iomem *reg = pIREQ_reg;

	if ((resetmode != VIOC_CONFIG_RESET) && (resetmode != VIOC_CONFIG_CLEAR)) {
		(void)pr_err("[ERR][VIOC_CONFIG] %s, in error, invalid mode:%d\n",
		       __func__, resetmode);

		goto FUNC_EXIT;
	}

	switch (get_vioc_type(component)) {
	case get_vioc_type(VIOC_DISP):
		value =
			(__raw_readl(reg + PWR_BLK_SWR1_OFFSET)
			 & ~(PWR_BLK_SWR1_DEV_MASK));
		value |=
			(resetmode
			 << (PWR_BLK_SWR1_DEV_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
		break;

	case get_vioc_type(VIOC_WMIX):
		value = (__raw_readl(reg + PWR_BLK_SWR1_OFFSET) & ~(PWR_BLK_SWR1_WMIX_MASK));
		value |= (resetmode << (PWR_BLK_SWR1_WMIX_SHIFT + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
		break;

	case get_vioc_type(VIOC_WDMA):
		value =
			(__raw_readl(reg + PWR_BLK_SWR1_OFFSET)
			 & ~(PWR_BLK_SWR1_WDMA_MASK));
		value |=
			(resetmode
			 << (PWR_BLK_SWR1_WDMA_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
		break;

	case get_vioc_type(VIOC_FIFO):
		value =
			(__raw_readl(reg + PWR_BLK_SWR1_OFFSET)
			 & ~(PWR_BLK_SWR1_FIFO_MASK));
		value |=
			(resetmode
			 << (PWR_BLK_SWR1_FIFO_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
		break;

	case get_vioc_type(VIOC_RDMA):
		value =
			(__raw_readl(reg + PWR_BLK_SWR0_OFFSET)
			 & ~(PWR_BLK_SWR0_RDMA_MASK));
		value |=
			(resetmode
			 << (PWR_BLK_SWR0_RDMA_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR0_OFFSET));
		break;

	case get_vioc_type(VIOC_SCALER):
		value =
			(__raw_readl(reg + PWR_BLK_SWR0_OFFSET)
			 & ~(PWR_BLK_SWR0_SC_MASK));
		value |=
			(resetmode
			 << (PWR_BLK_SWR0_SC_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR0_OFFSET));
		break;

	case get_vioc_type(VIOC_VIQE):
		if (get_vioc_index(component) == 0U) {
			value =
				(__raw_readl(reg + PWR_BLK_SWR1_OFFSET)
				 & ~(PWR_BLK_SWR1_VIQE0_MASK));
			value |= (resetmode << PWR_BLK_SWR1_VIQE0_SHIFT);
			__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
		} else {
			/* Prevent KCS warning */
		}
		break;
#if 0
	case get_vioc_type(VIOC_VIN):
#if defined(CONFIG_ARCH_TCC897X)

		value =
			(__raw_readl(reg + PWR_BLK_SWR0_OFFSET)
			 & ~(PWR_BLK_SWR0_VIN_MASK));
		value |=
			(resetmode
			 << (PWR_BLK_SWR0_VIN_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR0_OFFSET));
#endif
#if defined(CONFIG_ARCH_TCC803X) || defined(CONFIG_ARCH_TCC805X)

		value =
			(__raw_readl(
				 reg
				 + ((component > VIOC_VIN30) ?
					    PWR_BLK_SWR4_OFFSET :
					    PWR_BLK_SWR0_OFFSET))
			 & ~((component > VIOC_VIN30) ? PWR_BLK_SWR4_VIN_MASK :
						      PWR_BLK_SWR0_VIN_MASK));
		value |=
			(resetmode
			 << ((component > VIOC_VIN30) ? (PWR_BLK_SWR4_VIN_SHIFT
					     + (get_vioc_index(
							component - VIOC_VIN40)
						/ 2U)) :
						     (PWR_BLK_SWR0_VIN_SHIFT
					     + (get_vioc_index(component)
						/ 2U))));
		__raw_writel(
			value,
			reg
				+ ((component > VIOC_VIN30) ?
					   PWR_BLK_SWR4_OFFSET :
					   PWR_BLK_SWR0_OFFSET));
#endif
		break;
#endif
#ifdef CONFIG_VIOC_MAP_DECOMP
	case get_vioc_type(VIOC_MC):
		value =
			(__raw_readl(reg + PWR_BLK_SWR1_OFFSET)
			 & ~(PWR_BLK_SWR1_MC_MASK));
		value |=
			(resetmode
			 << (PWR_BLK_SWR1_MC_SHIFT
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + PWR_BLK_SWR1_OFFSET));
		break;
#endif
#if defined(CONFIG_VIOC_AFBCDEC) || defined(CONFIG_VIOC_PVRIC_FBDC)
	case get_vioc_type(VIOC_FBCDEC):
		value =
			(__raw_readl(reg + CFG_WMIX_PATH_SWR_OFFSET)
			 & ~(WMIX_PATH_SWR_AD_MASK));
		value |=
			(resetmode
			 << (WMIX_PATH_SWR_AD_SHITF
			     + get_vioc_index(component)));
		__raw_writel(value, (reg + CFG_WMIX_PATH_SWR_OFFSET));
		break;
#endif
	default:
		if (component == VIOC_NO_COMPONENT) {
			dprintk("%s: vioc_id(0x%x)\n", __func__, component);
		} else {
			(void)pr_err("[ERR][VIOC_CONFIG] %s, wrong component(0x%08x)\n",
				__func__, component);

			WARN_ON(1);
		}
		break;
	}

FUNC_EXIT:
	return;
}
EXPORT_SYMBOL(VIOC_CONFIG_SWReset);

/*
 * VIOC_CONFIG_Device_PlugState
 * Check PlugInOut status of VIOC SCALER, VIQE, DEINTLS.
 * component : VIOC_SC0, VIOC_SC1, VIOC_SC2, VIOC_VIQE, VIOC_DEINTLS
 * pDstatus : Pointer of status value.
 * return value : Device name of Plug in.
 */
int VIOC_CONFIG_Device_PlugState(
	unsigned int component, VIOC_PlugInOutCheck *VIOC_PlugIn)
{
	//	u32 value;
	const void __iomem *reg = NULL;
	int ret = -1;

	reg = CalcAddressViocComponent(component);
	if (reg == NULL) {
		VIOC_PlugIn->enable = 0;
		ret = VIOC_DEVICE_INVALID;
	} else {
		VIOC_PlugIn->enable =
			(__raw_readl(reg) & CFG_PATH_EN_MASK) >> CFG_PATH_EN_SHIFT;
		VIOC_PlugIn->connect_device =
			(__raw_readl(reg) & CFG_PATH_SEL_MASK) >> CFG_PATH_SEL_SHIFT;
		VIOC_PlugIn->connect_statue =
			(__raw_readl(reg) & CFG_PATH_STS_MASK) >> CFG_PATH_STS_SHIFT;
		ret = VIOC_DEVICE_CONNECTED;
	}
	return ret;
}
EXPORT_SYMBOL(VIOC_CONFIG_Device_PlugState);

static unsigned int CalcPathSelectionInScaler(unsigned int RdmaNum)
{
	unsigned int ret;

	ret = get_vioc_index(RdmaNum);
	return ret;
}

static unsigned int CalcPathSelectionInViqeDeinter(unsigned int RdmaNum)
{
	unsigned int ret;

	ret = get_vioc_index(RdmaNum);
	return ret;
}

/* HIS_GOTO */
int VIOC_CONFIG_GetScaler_PluginToRDMA(unsigned int RdmaNum)
{
	unsigned int i;
	unsigned int rdma_idx;
	VIOC_PlugInOutCheck VIOC_PlugIn;
	int ret = -1;

	rdma_idx = CalcPathSelectionInScaler(RdmaNum);

	for (i = get_vioc_index(VIOC_SCALER0); i <= VIOC_SCALER_MAX; i++) {
		if (VIOC_CONFIG_Device_PlugState(
			    (VIOC_SCALER0 + i), &VIOC_PlugIn)
		    == VIOC_DEVICE_INVALID) {
			/* prevent KCS warning */
			continue;
		}

		if ((VIOC_PlugIn.enable == 1U)
		    && (VIOC_PlugIn.connect_device == rdma_idx))  {
		    ret = ((int)VIOC_SCALER0 + (int)i);

			goto FUNC_EXIT;
		}
	}
	ret = -1;

FUNC_EXIT:
	return ret;
}
EXPORT_SYMBOL(VIOC_CONFIG_GetScaler_PluginToRDMA);

int VIOC_CONFIG_GetScaler_PluginToWDMA(unsigned int WdmaNum)
{
	unsigned int i;
	VIOC_PlugInOutCheck VIOC_PlugIn;
	int ret = -1;
	unsigned int break_flag = 0U; /* avoid MISRA C-2012 Rule 15.4 */

	for (i = get_vioc_index(VIOC_SCALER0); i < VIOC_SCALER_MAX; i++) {
		if (VIOC_CONFIG_Device_PlugState(
				(VIOC_SCALER0 + i), &VIOC_PlugIn)
				== VIOC_DEVICE_INVALID) {
				/* prevent KCS warning */
			continue;
		}

		if ((VIOC_PlugIn.enable == 0U) || (VIOC_PlugIn.connect_device < 0xEU)) {
			//disabled or connected device is not WDMA
			continue;
		}

		if ((VIOC_PlugIn.connect_device - 0xEU) == get_vioc_index(WdmaNum)) {
			ret = ((int)VIOC_SCALER0 + (int)i);
			break_flag = 1U;
		}

		if(break_flag == 1U) {
			break;
		}
	}
	return ret;
}
EXPORT_SYMBOL(VIOC_CONFIG_GetScaler_PluginToWDMA);

/* HIS_GOTO */
int VIOC_CONFIG_GetViqeDeintls_PluginToRDMA(unsigned int RdmaNum)
{
	unsigned int i;
	unsigned int rdma_idx;
	VIOC_PlugInOutCheck VIOC_PlugIn;
	int ret = -1;

	rdma_idx = CalcPathSelectionInViqeDeinter(RdmaNum);

	for (i = get_vioc_index(VIOC_VIQE0); i < VIOC_VIQE_MAX; i++) {
		if (VIOC_CONFIG_Device_PlugState((VIOC_VIQE0 + i), &VIOC_PlugIn)
		    == VIOC_DEVICE_INVALID) {
			/* prevent KCS warning */
			continue;
		}

		if ((VIOC_PlugIn.enable == 1U)
		    && (VIOC_PlugIn.connect_device == rdma_idx)) {
			ret = ((int)VIOC_VIQE0 + (int)i);

			goto FUNC_EXIT;
		}
	}

	ret = -1;

FUNC_EXIT:
	return ret;
}
EXPORT_SYMBOL(VIOC_CONFIG_GetViqeDeintls_PluginToRDMA);

int VIOC_CONFIG_GetRdma_PluginToComponent(
	unsigned int ComponentNum /*Viqe, Mc, Dtrc*/)
{
	VIOC_PlugInOutCheck VIOC_PlugIn;
	int ret = -1;

	if (VIOC_CONFIG_Device_PlugState(ComponentNum, &VIOC_PlugIn)
	    == VIOC_DEVICE_INVALID) {
		/* prevent KCS warning */
		ret = -1;
	} else {
		if (VIOC_PlugIn.enable != 0U) {
			/* prevent KCS warning */
			if (VIOC_PlugIn.connect_device < (UINT_MAX / 2U)) {/* avoid CERT-C Integers Rule INT31-C */
				ret = ((int)VIOC_RDMA + (int)VIOC_PlugIn.connect_device);
			}
		} else {
			/* prevent KCS warning */
			ret = -1;
		}
	}

	return ret;
}
EXPORT_SYMBOL(VIOC_CONFIG_GetRdma_PluginToComponent);

void VIOC_CONFIG_StopRequest(unsigned int en)
{
	u32 value;
	void __iomem *reg = pIREQ_reg;

	value = (__raw_readl(reg + CFG_MISC1_OFFSET) & ~(CFG_MISC1_S_REQ_MASK));

	if (en != 0U) {
		/* prevent KCS warning */
		value |= ((u32)0x0U << CFG_MISC1_S_REQ_SHIFT);
	} else {
		/* prevent KCS warning */
		value |= ((u32)0x1U << CFG_MISC1_S_REQ_SHIFT);
	}

	__raw_writel(value, reg + CFG_MISC1_OFFSET);
}
EXPORT_SYMBOL(VIOC_CONFIG_StopRequest);

/* HIS_GOTO */
int VIOC_CONFIG_LCDPath_Select(unsigned int lcdx_sel, unsigned int lcdx_if)
{
	int ret = 0;
	unsigned int i, cnt;
	unsigned int lcdx_bak;
	uint32_t val_misc1, val_misc2;
	unsigned int sel_lcd[5];
	cnt = 4;

	if ((lcdx_sel > 4U) || (lcdx_if > 4U)) {
		ret = -EINVAL;

		goto FUNC_EXIT;
	}

	val_misc1 = __raw_readl(pIREQ_reg + CFG_MISC1_OFFSET);
	sel_lcd[0] =
		(val_misc1 & CFG_MISC1_LCD0_SEL_MASK) >> CFG_MISC1_LCD0_SEL_SHIFT;
	sel_lcd[1] =
		(val_misc1 & CFG_MISC1_LCD1_SEL_MASK) >> CFG_MISC1_LCD1_SEL_SHIFT;

	val_misc2 = __raw_readl(pIREQ_reg + CFG_MISC2_OFFSET);
	sel_lcd[2] =
		(val_misc2 & CFG_MISC2_LCD2_SEL_MASK) >> CFG_MISC2_LCD2_SEL_SHIFT;
	sel_lcd[3] =
		(val_misc2 & CFG_MISC2_LCD3_SEL_MASK) >> CFG_MISC2_LCD3_SEL_SHIFT;
	sel_lcd[4] =
		(val_misc2 & CFG_MISC2_LCD3_SEL_MASK) >> CFG_MISC2_LCD3_SEL_SHIFT;

	if (sel_lcd[lcdx_if] != lcdx_sel) {
		lcdx_bak = sel_lcd[lcdx_if];
		sel_lcd[lcdx_if] = lcdx_sel;
	} else {
		goto FUNC_EXIT;
	}

	for (i = 0; i <= cnt; i++) {
		if ((i != lcdx_if) && (sel_lcd[i] == lcdx_sel)) {
			sel_lcd[i] = lcdx_bak;
			break;
		}
	}

	val_misc1 &=
		~(CFG_MISC1_LCD1_SEL_MASK | CFG_MISC1_LCD0_SEL_MASK);
	val_misc1 |= ((sel_lcd[1] << CFG_MISC1_LCD1_SEL_SHIFT)
		| (sel_lcd[0] << CFG_MISC1_LCD0_SEL_SHIFT));
	__raw_writel(val_misc1, pIREQ_reg + CFG_MISC1_OFFSET);

	val_misc2 &=
		~(CFG_MISC2_LCD4_SEL_MASK | CFG_MISC2_LCD3_SEL_MASK
		  | CFG_MISC2_LCD2_SEL_MASK);
	val_misc2 |= ((sel_lcd[4] << CFG_MISC2_LCD4_SEL_SHIFT)
		| (sel_lcd[3] << CFG_MISC2_LCD3_SEL_SHIFT)
		| (sel_lcd[2] << CFG_MISC2_LCD2_SEL_SHIFT));
	__raw_writel(val_misc2, pIREQ_reg + CFG_MISC2_OFFSET);

	dprintk("%s, CFG_MISC1: 0x%08x\n", __func__,
		__raw_readl(pIREQ_reg + CFG_MISC1_OFFSET));
	dprintk("%s, CFG_MISC2: 0x%08x\n", __func__,
		__raw_readl(pIREQ_reg + CFG_MISC2_OFFSET));

FUNC_EXIT:
	return ret;
}
EXPORT_SYMBOL(VIOC_CONFIG_LCDPath_Select);

/**
 *
 * @brief This API provides the function to find the ID of the display device
 *	  controller connected to the display mux through the mux ID.
 * @param uint32_t lcdx_sel Mux ID to find the ID of the connected
 *                          display device controller
 * @param uint32_t *ddc_id Pointer to stores ID of the display device
 *                         controller connected to the mux
 */
int VIOC_CONFIG_get_ddc_id_from_mux(uint32_t lcdx_sel, uint32_t *ddc_id)
{
	unsigned int sel_lcd[VIOC_DISP_MAX];
	uint32_t reg_val;
	int ret = 0;

	if (lcdx_sel >= VIOC_DISP_MAX) {
		ret = -EINVAL;
	} else {
		reg_val = __raw_readl(pIREQ_reg + CFG_MISC1_OFFSET);
		sel_lcd[0] =
			(reg_val & CFG_MISC1_LCD0_SEL_MASK) >> CFG_MISC1_LCD0_SEL_SHIFT;
		sel_lcd[1] =
			(reg_val & CFG_MISC1_LCD1_SEL_MASK) >> CFG_MISC1_LCD1_SEL_SHIFT;

		reg_val = __raw_readl(pIREQ_reg + CFG_MISC2_OFFSET);
		sel_lcd[2] =
			(reg_val & CFG_MISC2_LCD2_SEL_MASK) >> CFG_MISC2_LCD2_SEL_SHIFT;
		sel_lcd[3] =
			(reg_val & CFG_MISC2_LCD3_SEL_MASK) >> CFG_MISC2_LCD3_SEL_SHIFT;
		sel_lcd[4] =
			(reg_val & CFG_MISC2_LCD3_SEL_MASK) >> CFG_MISC2_LCD3_SEL_SHIFT;
		if (ddc_id != NULL) {
			*ddc_id = sel_lcd[lcdx_sel];
		}
	}

	return ret;
}
EXPORT_SYMBOL(VIOC_CONFIG_get_ddc_id_from_mux);

void __iomem *VIOC_IREQConfig_GetAddress(void)
{
	if (pIREQ_reg == NULL) {
		/* Prevent KCS warning */
		(void)pr_err("[ERR][VIOC_CONFIG] %s VIOC_IREQConfig\n", __func__);
	}

	return pIREQ_reg;
}
EXPORT_SYMBOL(VIOC_IREQConfig_GetAddress);

void VIOC_IREQConfig_DUMP(unsigned int offset, unsigned int size)
{
	unsigned int cnt = 0;

	if (pIREQ_reg == NULL) {
		/* Prevent KCS warning */
		(void)pr_err("[ERR][VIOC_CONFIG] %s, VIOC_IREQConfig\n", __func__);
	} else {
		dprintk(
			"[DBG][VIOC_CONFIG] VIOC_IREQConfig ::\n");
		while (cnt < size) {
			dprintk(
				"[DBG][VIOC_CONFIG] VIOC_IREQConfig + offset(%x) + 0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				offset, cnt,
				__raw_readl(pIREQ_reg + offset + cnt),
				__raw_readl(pIREQ_reg + offset + cnt + 0x4U),
				__raw_readl(pIREQ_reg + offset + cnt + 0x8U),
				__raw_readl(pIREQ_reg + offset + cnt + 0xCU));
			cnt += 0x10U;
		}
	}
}
EXPORT_SYMBOL(VIOC_IREQConfig_DUMP);

int vioc_config_init(void)
{
	struct device_node *ViocConfig_np;

	ViocConfig_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_config");

	if (ViocConfig_np == NULL) {
		(void)pr_info("[INF][VIOC_CONFIG] disabled [this is mandatory for vioc display]\n");
	} else {
		pIREQ_reg = of_iomap(ViocConfig_np, 0);

		if (pIREQ_reg != NULL) {
			(void)pr_info("[INF][VIOC_CONFIG] CONFIG\n");
		}
	}
	return 0;
}
EXPORT_SYMBOL(vioc_config_init);