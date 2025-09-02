// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>

#include "vin_wrap_wdma.h"
#if defined(CONFIG_ARCH_TCC750X)
#include "../750x/vin_wrap_cfg.h"
#endif
#if defined(CONFIG_ARCH_TCC807X)
#include "../807x/vin_wrap_cfg.h"
#endif

static struct device_node *VinWrapWdma_np;
static void __iomem *pWDMA_reg[VIN_WRAP_WDMA_MAX] = { 0 };

void VIN_WRAP_WDMA_SetImageEnable(void __iomem *reg, unsigned int nContinuous)
{
	u32 value;

	value = (__raw_readl(reg + WDMACTRL_OFFSET) &
		 ~(WDMACTRL_IEN_MASK | WDMACTRL_CONT_MASK | WDMACTRL_UPD_MASK));
	/*
	 * redundant update UPD has problem
	 * So if UPD is high, do not update UPD bit.
	 */
	value |= (((u32)0x1U << WDMACTRL_IEN_SHIFT) |
		  (nContinuous << WDMACTRL_CONT_SHIFT) |
		  ((u32)0x1U << WDMACTRL_UPD_SHIFT));

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

void VIN_WRAP_WDMA_GetImageEnable(const void __iomem *reg, unsigned int *enable)
{
	*enable = ((__raw_readl(reg + WDMACTRL_OFFSET) & WDMACTRL_IEN_MASK) >>
		   WDMACTRL_IEN_SHIFT);
}

void VIN_WRAP_WDMA_SetImageDisable(void __iomem *reg)
{
	u32 value;

	value = (__raw_readl(reg + WDMACTRL_OFFSET) &
		 ~(WDMACTRL_IEN_MASK | WDMACTRL_UPD_MASK));
	value |= (((u32)0x0U << WDMACTRL_IEN_SHIFT) |
		  ((u32)0x1U << WDMACTRL_UPD_SHIFT));
	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

void VIN_WRAP_WDMA_SetImageUpdate(void __iomem *reg)
{
	u32 value;

	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_UPD_MASK));
	value |= ((u32)0x1U << WDMACTRL_UPD_SHIFT);
	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

void VIN_WRAP_WDMA_SetContinuousMode(void __iomem *reg, unsigned int enable)
{
	u32 value;

	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_CONT_MASK));
	value |= (enable << WDMACTRL_CONT_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

void VIN_WRAP_WDMA_SetImageFormat(void __iomem *reg, unsigned int nFormat)
{
	u32 value;

	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_FMT_MASK));
	value |= (nFormat << WDMACTRL_FMT_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

void VIN_WRAP_WDMA_SetImageRGBSwapMode(void __iomem *reg, unsigned int rgb_mode)
{
	u32 value;

	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_SWAP_MASK));
	value |= (rgb_mode << WDMACTRL_SWAP_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

void VIN_WRAP_WDMA_SetImageInterlaced(void __iomem *reg, unsigned int intl)
{
	u32 value;

	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_INTL_MASK));
	value |= (intl << WDMACTRL_INTL_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

void VIN_WRAP_WDMA_SetImageR2YMode(void __iomem *reg, unsigned int r2y_mode)
{
	u32 value;

	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_R2YMD_MASK));
	value |= (r2y_mode << WDMACTRL_R2YMD_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

void VIN_WRAP_WDMA_SetImageR2YEnable(void __iomem *reg, unsigned int enable)
{
	u32 value;

	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_R2Y_MASK));
	value |= (enable << WDMACTRL_R2Y_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

void VIN_WRAP_WDMA_SetImageY2RMode(void __iomem *reg, unsigned int y2r_mode)
{
	u32 value;

	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_Y2RMD_MASK));
	value |= (y2r_mode << WDMACTRL_Y2RMD_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

void VIN_WRAP_WDMA_SetImageY2REnable(void __iomem *reg, unsigned int enable)
{
	u32 value;

	value = (__raw_readl(reg + WDMACTRL_OFFSET) & ~(WDMACTRL_Y2R_MASK));
	value |= (enable << WDMACTRL_Y2R_SHIFT);

	__raw_writel(value, reg + WDMACTRL_OFFSET);
}

void VIN_WRAP_WDMA_SetImageSize(void __iomem *reg, unsigned int sw,
				unsigned int sh)
{
	u32 value;

	value = ((sh << WDMASIZE_HEIGHT_SHIFT) | (sw << WDMASIZE_WIDTH_SHIFT));
	__raw_writel(value, reg + WDMASIZE_OFFSET);
}

void VIN_WRAP_WDMA_SetImageBase(void __iomem *reg, unsigned int nBase0,
				unsigned int nBase1, unsigned int nBase2)
{
	__raw_writel(nBase0 << WDMABASE0_BASE0_SHIFT, reg + WDMABASE0_OFFSET);
	__raw_writel(nBase1 << WDMABASE1_BASE1_SHIFT, reg + WDMABASE1_OFFSET);
	__raw_writel(nBase2 << WDMABASE2_BASE2_SHIFT, reg + WDMABASE2_OFFSET);
}

void VIN_WRAP_WDMA_SetImageOffset(void __iomem *reg, unsigned int imgFmt,
				  unsigned int imgWidth)
{
	unsigned int offset0 = 0;
	unsigned int offset1 = 0;
	u32 value = 0;

	if (imgWidth > 0x1FFFU) {
		(void)pr_err("[ERR][WMIX] %s imgWidth(%d) is wrong\n", __func__,
			     imgWidth);
	} else {
		switch (imgFmt) {
		case (unsigned int)VIE_WDMACTRL_FMT_1BPP:
			offset0 = (1U * imgWidth) / 8U;
			break;
		case (unsigned int)VIE_WDMACTRL_FMT_2BPP:
			offset0 = (1U * imgWidth) / 4U;
			break;
		case (unsigned int)VIE_WDMACTRL_FMT_4BPP:
			offset0 = (1U * imgWidth) / 2U;
			break;
		case (unsigned int)VIE_WDMACTRL_FMT_8BPP:
			offset0 = (1U * imgWidth);
			break;
		case (unsigned int)VIE_WDMACTRL_FMT_RGB332:
			offset0 = 1U * imgWidth;
			break;
		case (unsigned int)VIE_WDMACTRL_FMT_ARGB444:
		case (unsigned int)VIE_WDMACTRL_FMT_RGB565:
		case (unsigned int)VIE_WDMACTRL_FMT_ARGB1555:
			offset0 = 2U * imgWidth;
			break;
		case (unsigned int)VIE_WDMACTRL_FMT_ARGB8888:
		case (unsigned int)VIE_WDMACTRL_FMT_ARGB6666_4:
			offset0 = 4U * imgWidth;
			break;
		case (unsigned int)VIE_WDMACTRL_FMT_RGB888:
		case (unsigned int)VIE_WDMACTRL_FMT_ARGB6666_3:
			offset0 = 3U * imgWidth;
			break;
		case (unsigned int)VIN_WRAP_IMG_FMT_IR8:
			offset0 = imgWidth;
			break;
		case (unsigned int)VIE_WDMACTRL_FMT_444SEP:
			offset0 = imgWidth;
			offset1 = imgWidth;
			break;
		case (unsigned int)VIE_WDMACTRL_FMT_YUV420SP:
			offset0 = imgWidth;
			offset1 = imgWidth / 2U;
			break;
		case (unsigned int)VIE_WDMACTRL_FMT_YUV422SP:
			offset0 = imgWidth;
			offset1 = imgWidth / 2U;
			break;
		case (unsigned int)VIE_WDMACTRL_FMT_UYVY:
		case (unsigned int)VIE_WDMACTRL_FMT_VYUY:
		case (unsigned int)VIE_WDMACTRL_FMT_YUYV:
		case (unsigned int)VIE_WDMACTRL_FMT_YVYU:
			offset0 = 2U * imgWidth;
			break;
		case (unsigned int)VIE_WDMACTRL_FMT_YUV420ITL0:
		case (unsigned int)VIE_WDMACTRL_FMT_YUV420ITL1:
			offset0 = imgWidth;
			offset1 = imgWidth;
			break;
		case (unsigned int)VIE_WDMACTRL_FMT_YUV422ITL0:
		case (unsigned int)VIE_WDMACTRL_FMT_YUV422ITL1:
			offset0 = imgWidth;
			offset1 = imgWidth;
			break;
		default:
			offset0 = imgWidth;
			offset1 = imgWidth;
			break;
		}

		value = (__raw_readl(reg + WDMAOFFS_OFFSET) &
			 ~(WDMAOFFS_OFFSET1_MASK | WDMAOFFS_OFFSET0_MASK));
		value |= ((offset1 << WDMAOFFS_OFFSET1_SHIFT) |
			  (offset0 << WDMAOFFS_OFFSET0_SHIFT));
		__raw_writel(value, reg + WDMAOFFS_OFFSET);
	}
}

#ifdef L_STRIDE_ALIGN
void VIN_WRAP_WDMA_SetImageOffset_withYV12(void __iomem *reg,
					   unsigned int imgWidth)
{
	unsigned int stride, stride_c;
	u32 value;

	stride = ALIGNED_BUFF(imgWidth, L_STRIDE_ALIGN);
	stride_c = ALIGNED_BUFF((stride / 2U), C_STRIDE_ALIGN);

	value = (__raw_readl(reg + WDMAOFFS_OFFSET) &
		 ~(WDMAOFFS_OFFSET1_MASK | WDMAOFFS_OFFSET0_MASK));
	value |= ((stride_c << WDMAOFFS_OFFSET1_SHIFT) |
		  (stride << WDMAOFFS_OFFSET0_SHIFT));
	__raw_writel(value, reg + WDMAOFFS_OFFSET);
}
#endif

void VIN_WRAP_WDMA_SetIreqMask(void __iomem *reg, unsigned int mask,
			       unsigned int set)
{
	/*
	 * set 1 : IREQ Masked(interrupt disable),
	 * set 0 : IREQ UnMasked(interrput enable)
	 */
	u32 value;

	value = (__raw_readl(reg + WDMAIRQMSK_OFFSET) & ~(mask));

	if (set == 1U) { /* Interrupt Disable*/
		/* Prevent KCS warning */
		value |= mask;
	}

	__raw_writel(value, reg + WDMAIRQMSK_OFFSET);
}

void VIN_WRAP_WDMA_SetIreqStatus(void __iomem *reg, unsigned int mask)
{
	u32 value;

	value = (__raw_readl(reg + WDMAIRQSTS_OFFSET) & ~(mask));
	value |= mask;
	__raw_writel(value, reg + WDMAIRQSTS_OFFSET);
}

void VIN_WRAP_WDMA_ClearEOFR(void __iomem *reg)
{
	u32 value;

	value = (__raw_readl(reg + WDMAIRQSTS_OFFSET) &
		 ~(WDMAIRQSTS_EOFR_MASK));
	value |= ((u32)0x1U << WDMAIRQSTS_EOFR_SHIFT);
	__raw_writel(value, reg + WDMAIRQSTS_OFFSET);
}

void VIN_WRAP_WDMA_ClearEOFF(void __iomem *reg)
{
	u32 value;

	value = (__raw_readl(reg + WDMAIRQSTS_OFFSET) &
		 ~(WDMAIRQSTS_EOFF_MASK));
	value |= ((u32)0x1U << WDMAIRQSTS_EOFF_SHIFT);
	__raw_writel(value, reg + WDMAIRQSTS_OFFSET);
}

void VIN_WRAP_WDMA_GetStatus(const void __iomem *reg, unsigned int *status)
{
	*status = __raw_readl(reg + WDMAIRQSTS_OFFSET);
}

bool VIN_WRAP_WDMA_IsImageEnable(const void __iomem *reg)
{
	return ((((__raw_readl(reg + WDMACTRL_OFFSET) & WDMACTRL_IEN_MASK) >>
		  WDMACTRL_IEN_SHIFT) != 0U) ?
			true :
			false);
}

bool VIN_WRAP_WDMA_IsContinuousMode(const void __iomem *reg)
{
	return ((((__raw_readl(reg + WDMACTRL_OFFSET) & WDMACTRL_CONT_MASK) >>
		  WDMACTRL_CONT_SHIFT) != 0U) ?
			true :
			false);
}

unsigned int VIN_WRAP_WDMA_Get_CAddress(const void __iomem *reg)
{
	return (__raw_readl(reg + WDMACADDR_OFFSET));
}

void __iomem *VIN_WRAP_WDMA_GetAddress(unsigned int wdma_id)
{
	unsigned int Num = get_vin_wrap_index(wdma_id);
	void __iomem *ret = NULL;

	if ((Num >= VIN_WRAP_WDMA_MAX) || (pWDMA_reg[Num] == NULL)) {
		(void)pr_err("[ERR][WDMA] %s num:%d Max wdma num:%d\n",
			     __func__, Num, VIN_WRAP_WDMA_MAX);
		ret = NULL;
	} else {
		ret = pWDMA_reg[Num];
	}

	return ret;
}

void VIN_WRAP_WDMA_DUMP(const void __iomem *reg, unsigned int wdma_id)
{
	unsigned int cnt = 0;
	const void __iomem *pReg = reg;
	unsigned int Num = get_vin_wrap_index(wdma_id);

	if (Num >= VIN_WRAP_WDMA_MAX) {
		(void)pr_err("[ERR][WDMA] %s num:%d Max wdma num:%d\n",
			     __func__, Num, VIN_WRAP_WDMA_MAX);
	} else {
		if (pReg == NULL) {
			/* Prevent KCS warning */
			pReg = VIN_WRAP_WDMA_GetAddress(wdma_id);
		}
		if (pReg != NULL) {
			(void)pr_info("[DBG][WDMA] WDMA-%d ::\n", Num);
			while (cnt < 0x70U) {
				(void)pr_info(
					"WDMA-%d + 0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
					Num, cnt, __raw_readl(pReg + cnt),
					__raw_readl(pReg + cnt + 0x4U),
					__raw_readl(pReg + cnt + 0x8U),
					__raw_readl(pReg + cnt + 0xCU));
				cnt += 0x10U;
			}
		}
	}
}

void vin_wrap_wdma_init(void)
{
	unsigned int i = 0;

	VinWrapWdma_np =
		of_find_compatible_node(NULL, NULL, "telechips,vin_wrap_wdma");
	if (VinWrapWdma_np == NULL) {
		/* Prevent KCS warning */
		(void)pr_info("[INF][WDMA] vin-wrap-wdma: disabled\n");
	} else {
		for (i = 0; i < VIN_WRAP_WDMA_MAX; i++) {
			pWDMA_reg[i] = (void __iomem *)of_iomap(VinWrapWdma_np,
								(int)i);

			if (pWDMA_reg[i] != NULL) {
				/* Prevent KCS warning */
				(void)pr_info("[INF][WDMA] vin-wrap-wdma%d\n",
					      i);
			}
		}
	}
}
