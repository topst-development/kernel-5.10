// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <video/telechips/vioc_global.h>
#include <video/telechips/vioc_ddicfg.h>	// is_VIOC_REMAP
#include <video/telechips/vioc_afbcdec.h>

#define AFBCDec_MAX_N 2U
#define AFBC_DEC_BUFFER_ALIGN 64U
#define AFBC_ALIGNED(w, mul) ((((unsigned int)w) + ((mul) - 1U)) & ~((mul) - 1U))

static struct device_node *pViocAFBCDec_np;
static void __iomem *pAFBCDec_reg[AFBCDec_MAX_N] = {0};

/******************************* AFBC_DEC Control
 * *******************************/

void VIOC_AFBCDec_GetBlockInfo(const void __iomem *reg,
			       unsigned int *productID, unsigned int *verMaj,
			       unsigned int *verMin, unsigned int *verStat)
{
	*productID = ((__raw_readl(reg + AFBCDEC_BLOCK_ID) &
		       AFBCDEC_PRODUCT_ID_MASK) >>
		      AFBCDEC_PRODUCT_ID_SHIFT);
	*verMaj = ((__raw_readl(reg + AFBCDEC_BLOCK_ID) &
		    AFBCDEC_VERSION_MAJOR_MASK) >>
		   AFBCDEC_VERSION_MAJOR_SHIFT);
	*verMin = ((__raw_readl(reg + AFBCDEC_BLOCK_ID) &
		    AFBCDEC_VERSION_MINOR_MASK) >>
		   AFBCDEC_VERSION_MINOR_SHIFT);
	*verStat = ((__raw_readl(reg + AFBCDEC_BLOCK_ID) &
		     AFBCDEC_VERSION_STATUS_MASK) >>
		    AFBCDEC_VERSION_STATUS_SHIFT);
}
EXPORT_SYMBOL(VIOC_AFBCDec_GetBlockInfo);

void VIOC_AFBCDec_SetContiDecEnable(void __iomem *reg,
				    unsigned int enable)
{
	u32 val;

	val = (__raw_readl(reg + AFBCDEC_SURFACE_CFG) &
	       ~(AFBCDEC_SURCFG_CONTI_MASK));
	val |= (enable << AFBCDEC_SURCFG_CONTI_SHIFT);
	__raw_writel(val, reg + AFBCDEC_SURFACE_CFG);
}
EXPORT_SYMBOL(VIOC_AFBCDec_SetContiDecEnable);

void VIOC_AFBCDec_SetSurfaceN(void __iomem *reg,
			      VIOC_AFBCDEC_SURFACE_NUM nSurface,
			      unsigned int enable)
{
	u32 val;

	val = (__raw_readl(reg + AFBCDEC_SURFACE_CFG)
		& ~((u32)0x1U << (unsigned int)nSurface));
	val |= (enable << (unsigned int)nSurface);
	__raw_writel(val, reg + AFBCDEC_SURFACE_CFG);
}
EXPORT_SYMBOL(VIOC_AFBCDec_SetSurfaceN);

void VIOC_AFBCDec_SetAXICacheCfg(void __iomem *reg, unsigned int cache)
{
	u32 val;

	val = (__raw_readl(reg + AFBCDEC_AXI_CFG) &
	       ~(AFBCDEC_AXICFG_CACHE_MASK));
	val |= ((cache & 0xFU) << AFBCDEC_AXICFG_CACHE_SHIFT);
	__raw_writel(val, reg + AFBCDEC_AXI_CFG);
}
EXPORT_SYMBOL(VIOC_AFBCDec_SetAXICacheCfg);

void VIOC_AFBCDec_SetAXIQoSCfg(void __iomem *reg, unsigned int qos)
{
	u32 val;

	val = (__raw_readl(reg + AFBCDEC_AXI_CFG) & ~(AFBCDEC_AXICFG_QOS_MASK));
	val |= ((qos & 0xFU) << AFBCDEC_AXICFG_QOS_SHIFT);
	__raw_writel(val, reg + AFBCDEC_AXI_CFG);
}
EXPORT_SYMBOL(VIOC_AFBCDec_SetAXIQoSCfg);

void VIOC_AFBCDec_SetSrcImgBase(void __iomem *reg, unsigned int base0,
				unsigned int base1)
{
	__raw_writel((base0 & 0xFFFFFFC0U), reg + AFBCDEC_S_HEADER_BUF_ADDR_LOW);
	__raw_writel((base1 & 0xFFFFU), reg + AFBCDEC_S_HEADER_BUF_ADDR_HIGH);
}
EXPORT_SYMBOL(VIOC_AFBCDec_SetSrcImgBase);

void VIOC_AFBCDec_SetWideModeEnable(void __iomem *reg,
				    unsigned int enable)
{
	u32 val;

	val = (__raw_readl(reg + AFBCDEC_S_FORMAT_SPECIFIER) &
	       ~(AFBCDEC_MODE_SUPERBLK_MASK));
	val |= (enable << AFBCDEC_MODE_SUPERBLK_SHIFT);
	__raw_writel(val, reg + AFBCDEC_S_FORMAT_SPECIFIER);
}
EXPORT_SYMBOL(VIOC_AFBCDec_SetWideModeEnable);

void VIOC_AFBCDec_SetSplitModeEnable(void __iomem *reg,
				     unsigned int enable)
{
	u32 val;

	val = (__raw_readl(reg + AFBCDEC_S_FORMAT_SPECIFIER) &
	       ~(AFBCDEC_MODE_SPLIT_MASK));
	val |= (enable << AFBCDEC_MODE_SPLIT_SHIFT);
	__raw_writel(val, reg + AFBCDEC_S_FORMAT_SPECIFIER);
}
EXPORT_SYMBOL(VIOC_AFBCDec_SetSplitModeEnable);

void VIOC_AFBCDec_SetYUVTransEnable(void __iomem *reg,
				    unsigned int enable)
{
	u32 val;

	val = (__raw_readl(reg + AFBCDEC_S_FORMAT_SPECIFIER) &
	       ~(AFBCDEC_MODE_YUV_TRANSF_MASK));
	val |= (enable << AFBCDEC_MODE_YUV_TRANSF_SHIFT);
	__raw_writel(val, reg + AFBCDEC_S_FORMAT_SPECIFIER);
}
EXPORT_SYMBOL(VIOC_AFBCDec_SetYUVTransEnable);

void VIOC_AFBCDec_SetPaylodeLimitEnable(void __iomem *reg,
				    unsigned int enable)
{
	u32 val;

	val = (__raw_readl(reg + AFBCDEC_S_FORMAT_SPECIFIER) &
	       ~(AFBCDEC_MODE_PAYLOAD_LIMIT_MASK));
	val |= (enable << AFBCDEC_MODE_PAYLOAD_LIMIT_SIHFT);
	__raw_writel(val, reg + AFBCDEC_S_FORMAT_SPECIFIER);
}
EXPORT_SYMBOL(VIOC_AFBCDec_SetPaylodeLimitEnable);

void VIOC_AFBCDec_SetTiledHeaderEnable(void __iomem *reg,
				    unsigned int enable)
{
	u32 val;

	val = (__raw_readl(reg + AFBCDEC_S_FORMAT_SPECIFIER) &
	       ~(AFBCDEC_MODE_TILED_HEADER_MASK));
	val |= (enable << AFBCDEC_MODE_TILED_HEADER_SHIFT);
	__raw_writel(val, reg + AFBCDEC_S_FORMAT_SPECIFIER);
}
EXPORT_SYMBOL(VIOC_AFBCDec_SetTiledHeaderEnable);

void VIOC_AFBCDec_SetImgFmt(void __iomem *reg, unsigned int fmt,
			    unsigned int enable_10bit)
{
	u32 val;

	val = (__raw_readl(reg + AFBCDEC_S_FORMAT_SPECIFIER) &
	       ~(AFBCDEC_MODE_PIXELFMT_MASK));

	switch (fmt) {
	case VIOC_IMG_FMT_RGB888:
		val |= AFBCDEC_FORMAT_RGB888;
		break;
	case VIOC_IMG_FMT_ARGB8888:
		val |= AFBCDEC_FORMAT_RGBA8888;
		break;
	case VIOC_IMG_FMT_YUYV:
		if (enable_10bit != 0U) {
			val |= AFBCDEC_FORMAT_10BIT_YUV422;
		} else {
			val |= AFBCDEC_FORMAT_8BIT_YUV422;
		}
		break;
	default:
		(void)pr_info("[INFO][AFBC] info %s fmt is %d\n",
				__func__, fmt);
		break;
	}

	__raw_writel(val, reg + AFBCDEC_S_FORMAT_SPECIFIER);
}
EXPORT_SYMBOL(VIOC_AFBCDec_SetImgFmt);

void VIOC_AFBCDec_SetImgSize(void __iomem *reg, unsigned int width,
			     unsigned int height)
{
	u32 val_width, val_height;

	val_width = (width & 0x3FFFU) << AFBCDEC_SIZE_WIDTH_SHIFT;
	__raw_writel(val_width, reg + AFBCDEC_S_BUFFER_WIDTH);

	val_height = (height & 0x3FFFU) << AFBCDEC_SIZE_HEIGHT_SHIFT;
	__raw_writel(val_height, reg + AFBCDEC_S_BUFFER_HEIGHT);
}
EXPORT_SYMBOL(VIOC_AFBCDec_SetImgSize);

void VIOC_AFBCDec_GetImgSize(const void __iomem *reg, unsigned int *width,
			     unsigned int *height)
{
	*width = ((__raw_readl(reg + AFBCDEC_SIZE_WIDTH_SHIFT) &
		   AFBCDEC_SIZE_WIDTH_MASK) >>
		  AFBCDEC_SIZE_WIDTH_SHIFT);
	*height = ((__raw_readl(reg + AFBCDEC_SIZE_HEIGHT_SHIFT) &
		    AFBCDEC_SIZE_HEIGHT_MASK) >>
		   AFBCDEC_SIZE_HEIGHT_SHIFT);
}
EXPORT_SYMBOL(VIOC_AFBCDec_GetImgSize);

void VIOC_AFBCDec_SetBoundingBox(void __iomem *reg, unsigned int startX,
				 unsigned int endX, unsigned int startY,
				 unsigned int endY)
{
	u32 val_startX, val_endX, val_startY, val_endY;

	val_startX = ((startX & 0x1FFFU) << AFBCDEC_BOUNDING_BOX_X_START_SHIFT);
	__raw_writel(val_startX, reg + AFBCDEC_S_BOUNDING_BOX_X_START);

	val_endX = ((endX & 0x1FFFU) << AFBCDEC_BOUNDING_BOX_X_END_SHIFT);
	__raw_writel(val_endX, reg + AFBCDEC_S_BOUNDING_BOX_X_END);

	val_startY = ((startY & 0x1FFFU) << AFBCDEC_BOUNDING_BOX_Y_START_SHIFT);
	__raw_writel(val_startY, reg + AFBCDEC_S_BOUNDING_BOX_Y_START);

	val_endY = ((endY & 0x1FFFU) << AFBCDEC_BOUNDING_BOX_Y_END_SHIFT);
	__raw_writel(val_endX, reg + AFBCDEC_S_BOUNDING_BOX_Y_END);
}
EXPORT_SYMBOL(VIOC_AFBCDec_SetBoundingBox);

void VIOC_AFBCDec_GetBoundingBox(const void __iomem *reg,
				 unsigned int *startX, unsigned int *endX,
				 unsigned int *startY, unsigned int *endY)
{
	*startX = ((__raw_readl(reg + AFBCDEC_S_BOUNDING_BOX_X_START) &
		  AFBCDEC_BOUNDING_BOX_X_START_MASK) >>
		 AFBCDEC_BOUNDING_BOX_X_START_SHIFT);
	*endX = ((__raw_readl(reg + AFBCDEC_S_BOUNDING_BOX_X_END) &
		  AFBCDEC_BOUNDING_BOX_X_END_MASK) >>
		 AFBCDEC_BOUNDING_BOX_X_END_SHIFT);

	*startY = ((__raw_readl(reg + AFBCDEC_S_BOUNDING_BOX_Y_START) &
		  AFBCDEC_BOUNDING_BOX_Y_START_MASK) >>
		 AFBCDEC_BOUNDING_BOX_Y_START_SHIFT);
	*endY = ((__raw_readl(reg + AFBCDEC_S_BOUNDING_BOX_Y_END) &
		  AFBCDEC_BOUNDING_BOX_Y_END_MASK) >>
		 AFBCDEC_BOUNDING_BOX_Y_END_SHIFT);
}
EXPORT_SYMBOL(VIOC_AFBCDec_GetBoundingBox);

void VIOC_AFBCDec_SetOutBufBase(void __iomem *reg, unsigned int base0,
				unsigned int base1, unsigned int fmt,
				unsigned int width)
{
	unsigned int afbc_stride = 0U;

	__raw_writel((base0 & 0xFFFFFF80U), reg + AFBCDEC_S_OUTPUT_BUF_ADDR_LOW);
	__raw_writel((base1 & 0xFFFFU), reg + AFBCDEC_S_OUTPUT_BUF_ADDR_HIGH);

	/* avoid CERT-C Integers Rule INT30-C */
	if (width < UINT_MAX) {
		switch (fmt) {
		case VIOC_IMG_FMT_RGB888:
		case VIOC_IMG_FMT_ARGB8888:
			afbc_stride = width * 4U;
			break;
		case VIOC_IMG_FMT_YUYV:
			afbc_stride = width * 2U;
			break;
		default:
			(void)pr_info("[INFO][AFBC] info %s fmt is %d\n",
				__func__, fmt);
			break;
		}
		__raw_writel(afbc_stride, reg + AFBCDEC_S_OUTPUT_BUF_STRIDE);
	}
}
EXPORT_SYMBOL(VIOC_AFBCDec_SetOutBufBase);

void VIOC_AFBCDec_SetBufferFlipX(void __iomem *reg,
				 unsigned int enable)
{
	u32 val;

	val = (__raw_readl(reg + AFBCDEC_S_PREFETCH_CFG) &
	       ~(AFBCDEC_PREFETCH_X_MASK));
	val |= (enable << AFBCDEC_PREFETCH_X_SHIFT);
	__raw_writel(val, reg + AFBCDEC_S_PREFETCH_CFG);
}
EXPORT_SYMBOL(VIOC_AFBCDec_SetBufferFlipX);

void VIOC_AFBCDec_SetBufferFlipY(void __iomem *reg,
				 unsigned int enable)
{
	u32 val;

	val = (__raw_readl(reg + AFBCDEC_S_PREFETCH_CFG) &
	       ~(AFBCDEC_PREFETCH_Y_MASK));
	val |= (enable << AFBCDEC_PREFETCH_Y_SHIFT);
	__raw_writel(val, reg + AFBCDEC_S_PREFETCH_CFG);
}
EXPORT_SYMBOL(VIOC_AFBCDec_SetBufferFlipY);

void VIOC_AFBCDec_SetPaylode(void __iomem *reg, unsigned int minLow,
				 unsigned int minHigh, unsigned int maxLow,
				 unsigned int maxHigh)
{
	u32 val_minLow, val_minHigh, val_maxLow, val_maxHigh;

	val_minLow = ((minLow & 0xFFFFFFFFU) << AFBCDEC_PAYLOAD_MIN_LOW_SHIFT);
	__raw_writel(val_minLow, reg + AFBCDEC_S_PAYLOAD_MIN_LOW);

	val_minHigh = ((minHigh & 0xFFFFU) << AFBCDEC_PAYLOAD_MIN_HIGH_SHIFT);
	__raw_writel(val_minHigh, reg + AFBCDEC_S_PAYLOAD_MIN_HIGH);

	val_maxLow = ((maxLow & 0xFFFFFFFFU) << AFBCDEC_PAYLOAD_MAX_LOW_SHIFT);
	__raw_writel(val_maxLow, reg + AFBCDEC_S_PAYLOAD_MAX_LOW);

	val_maxHigh = ((maxHigh & 0x1FFFU) << AFBCDEC_PAYLOAD_MAX_HIGH_SHIFT);
	__raw_writel(val_maxHigh, reg + AFBCDEC_S_PAYLOAD_MAX_HIGH);
}
EXPORT_SYMBOL(VIOC_AFBCDec_SetPaylode);

void VIOC_AFBCDec_GetPaylode(const void __iomem *reg,
				 unsigned int *minLow, unsigned int *minHigh,
				 unsigned int *maxLow, unsigned int *maxHigh)
{
	*minLow = ((__raw_readl(reg + AFBCDEC_S_PAYLOAD_MIN_LOW) &
		  AFBCDEC_PAYLOAD_MIN_LOW_MASK) >>
		 AFBCDEC_PAYLOAD_MIN_LOW_SHIFT);
	*minHigh = ((__raw_readl(reg + AFBCDEC_S_PAYLOAD_MIN_HIGH) &
		  AFBCDEC_PAYLOAD_MIN_HIGH_MASK) >>
		 AFBCDEC_PAYLOAD_MIN_HIGH_SHIFT);

	*maxLow = ((__raw_readl(reg + AFBCDEC_S_PAYLOAD_MAX_LOW) &
		  AFBCDEC_PAYLOAD_MAX_LOW_MASK) >>
		 AFBCDEC_PAYLOAD_MAX_LOW_SHIFT);
	*maxHigh = ((__raw_readl(reg + AFBCDEC_S_PAYLOAD_MAX_HIGH) &
		  AFBCDEC_PAYLOAD_MAX_HIGH_MASK) >>
		 AFBCDEC_PAYLOAD_MAX_HIGH_SHIFT);
}
EXPORT_SYMBOL(VIOC_AFBCDec_GetPaylode);

void VIOC_AFBCDec_SetIrqMask(void __iomem *reg, unsigned int enable,
				unsigned int mask)
{
	if (enable == 1U) { /* Interrupt Enable*/
		__raw_writel(mask, reg + AFBCDEC_IRQ_MASK);
	} else{ /* Interrupt Diable*/
		__raw_writel(~mask, reg + AFBCDEC_IRQ_MASK);
	}
}
EXPORT_SYMBOL(VIOC_AFBCDec_SetIrqMask);

unsigned int VIOC_AFBCDec_GetStatus(const void __iomem *reg)
{
	return __raw_readl(reg + AFBCDEC_IRQ_STATUS);
}
EXPORT_SYMBOL(VIOC_AFBCDec_GetStatus);

void VIOC_AFBCDec_ClearIrq(void __iomem *reg, unsigned int mask)
{
	__raw_writel(mask, reg + AFBCDEC_IRQ_CLEAR);
}
EXPORT_SYMBOL(VIOC_AFBCDec_ClearIrq);

void VIOC_AFBCDec_TurnOn(void __iomem *reg,
			      VIOC_AFBCDEC_SWAP swapmode)
{
	u32 val;

	val = (__raw_readl(reg + AFBCDEC_COMMAND) &
		~(AFBCDEC_CMD_DIRECT_SWAP_MASK |
		AFBCDEC_CMD_PENDING_SWAP_MASK));
	val |= ((u32)0x1U << (unsigned int)swapmode);
	__raw_writel(val, reg + AFBCDEC_COMMAND);
}
EXPORT_SYMBOL(VIOC_AFBCDec_TurnOn);

void VIOC_AFBCDec_TurnOFF(void __iomem *reg)
{
	u32 val;

	val = (__raw_readl(reg + AFBCDEC_COMMAND) &
		~(AFBCDEC_CMD_DIRECT_SWAP_MASK |
		AFBCDEC_CMD_PENDING_SWAP_MASK));
	__raw_writel(val, reg + AFBCDEC_COMMAND);
}
EXPORT_SYMBOL(VIOC_AFBCDec_TurnOFF);

void VIOC_AFBCDec_SurfaceCfg(void __iomem *reg, unsigned int base,
			 unsigned int fmt, unsigned int width,
			 unsigned int height, unsigned int b10bit,
			 unsigned int split_mode, unsigned int wide_mode,
			 unsigned int nSurface, unsigned int bSetOutputBase)
{
	void __iomem *pSurface_Dec = NULL;

	if ((width < 1U) || (height < 1U)) {
		(void)pr_err("[ERR][AFBC] err %s parameter is wrong\n",
			__func__);

		goto FUNC_EXIT;
	}
	//(void)pr_info("%s- Start\n", __func__);

	switch (nSurface) {
	case (unsigned int)VIOC_AFBCDEC_SURFACE_1:
		pSurface_Dec = reg + AFBCDEC_S1_BASE;
	break;
	case (unsigned int)VIOC_AFBCDEC_SURFACE_2:
		pSurface_Dec = reg + AFBCDEC_S2_BASE;
	break;
	case (unsigned int)VIOC_AFBCDEC_SURFACE_3:
		pSurface_Dec = reg + AFBCDEC_S3_BASE;
	break;
	default:
		pSurface_Dec = reg + AFBCDEC_S0_BASE;
	break;
	}

	VIOC_AFBCDec_SetSrcImgBase(pSurface_Dec, base, 0U);
	if (bSetOutputBase != 0U) {
		VIOC_AFBCDec_SetWideModeEnable(pSurface_Dec, wide_mode);
		VIOC_AFBCDec_SetSplitModeEnable(pSurface_Dec, split_mode);

		switch (fmt) {
		case VIOC_IMG_FMT_RGB888:
		case VIOC_IMG_FMT_ARGB8888:
			VIOC_AFBCDec_SetYUVTransEnable(pSurface_Dec, 1U);
		break;
		case VIOC_IMG_FMT_YUYV:
			VIOC_AFBCDec_SetYUVTransEnable(pSurface_Dec, 0U);
		break;
		default:
			(void)pr_info("[INFO][AFBC] info %s fmt is %d\n",
				__func__, fmt);
		break;
		}

		VIOC_AFBCDec_SetImgFmt(pSurface_Dec, fmt, b10bit);
		VIOC_AFBCDec_SetImgSize(pSurface_Dec, width, height);
		VIOC_AFBCDec_SetBoundingBox(pSurface_Dec, 0U, width - 1U,
								0U, height - 1U);
		VIOC_AFBCDec_SetOutBufBase(pSurface_Dec, base, 0U, fmt, width);
	}

	//(void)pr_info("%s - End\n", __func__);
FUNC_EXIT:
	return;
}
EXPORT_SYMBOL(VIOC_AFBCDec_SurfaceCfg);

void VIOC_AFBCDec_DUMP(const void __iomem *reg, unsigned int vioc_id)
{
	unsigned int cnt = 0;
	const void __iomem *pReg = reg;
	unsigned int Num = get_vioc_index(vioc_id);

	if (Num >= AFBCDec_MAX_N) {
		(void)pr_err("[ERR][AFBC] err %s Num:%d , max :%d\n",
		__func__, Num, AFBCDec_MAX_N);
	} else {
		if (pReg == NULL) {
			/* Prevent KCS warning */
			pReg = VIOC_AFBCDec_GetAddress(vioc_id);
		}
		if (pReg != NULL) {
			(void)pr_info("[DBG][AFBC] AFBC_DEC-%d ::\n", Num);
			while (cnt < 0x100U) {
				(void)pr_info(
					"[DBG][AFBC] AFBC_DEC-%d + 0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
					Num, cnt,
					__raw_readl(pReg + cnt), __raw_readl(pReg + cnt + 0x4U),
					__raw_readl(pReg + cnt + 0x8U),
					__raw_readl(pReg + cnt + 0xCU));
				cnt += 0x10U;
			}
		}
	}
}
EXPORT_SYMBOL(VIOC_AFBCDec_DUMP);

void __iomem *VIOC_AFBCDec_GetAddress(unsigned int vioc_id)
{
	unsigned int Num = get_vioc_index(vioc_id);
	void __iomem *ret = NULL;

	if ((Num >= AFBCDec_MAX_N) || (pAFBCDec_reg[Num] == NULL)) {
		(void)pr_err("[ERR][AFBC] err %s Num:%d , max :%d\n",
		__func__, Num, AFBCDec_MAX_N);
		ret = NULL;
	} else {
		ret = pAFBCDec_reg[Num];
	}

	return ret;
}
EXPORT_SYMBOL(VIOC_AFBCDec_GetAddress);

int vioc_afbc_dec_init(void)
{
	int i;

	pViocAFBCDec_np =
		of_find_compatible_node(NULL, NULL, "telechips,vioc_afbc_dec");

	if (pViocAFBCDec_np == NULL) {
		/* Prevent KCS warning */
		(void)pr_info("[INF][AFBC] vioc-afbc_dec: disabled\n");
	} else {
		for (i = 0; i < (int)AFBCDec_MAX_N; i++) {
			pAFBCDec_reg[i] = (void __iomem *)of_iomap(
					pViocAFBCDec_np,
					(is_VIOC_REMAP != 0U) ?
					(i + (int)AFBCDec_MAX_N) : i);

			if (pAFBCDec_reg[i] != NULL) {
				(void)pr_info("[INF][AFBC] vioc-afbc_dec%d\n", i);
			}
		}
	}
	return 0;
}
EXPORT_SYMBOL(vioc_afbc_dec_init);