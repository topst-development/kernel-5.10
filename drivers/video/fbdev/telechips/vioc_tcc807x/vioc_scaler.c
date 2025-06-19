// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of_address.h>
#include <soc/telechips/chipinfo.h>

#include <video/telechips/tcc_types.h>
#include <video/telechips/vioc_global.h>
#include <video/telechips/vioc_scaler.h>
#include <video/telechips/vioc_config.h>
#include <video/telechips/vioc_ddicfg.h> // is_VIOC_REMAP

static struct device_node *ViocSC_np;
static void __iomem *pScale[VIOC_SCALER_MAX];

/* HIS_CALLING */
void VIOC_SC_SetBypass(void __iomem *reg, unsigned int nOnOff)
{
	u32 val;

	val = (__raw_readl(reg + SCCTRL) & ~(SCCTRL_BP_MASK));
	val |= (nOnOff << SCCTRL_BP_SHIFT);
	__raw_writel(val, reg + SCCTRL);
}
EXPORT_SYMBOL(VIOC_SC_SetBypass);

/* HIS_CALLING */
void VIOC_SC_SetUpdate(void __iomem *reg)
{
	u32 val;

	val = (__raw_readl(reg + SCCTRL)
	       & ~(SCCTRL_UPD_MASK /*| SCCTRL_FFC_MASK*/));
	val |= ((u32)0x1U << SCCTRL_UPD_SHIFT);
	__raw_writel(val, reg + SCCTRL);
}
EXPORT_SYMBOL(VIOC_SC_SetUpdate);

void VIOC_SC_SetSrcSize(
	void __iomem *reg, unsigned int nWidth, unsigned int nHeight)
{
	u32 val;

	val = ((nHeight << SCSSIZE_HEIGHT_SHIFT)
	       | (nWidth << SCSSIZE_WIDTH_SHIFT));

	__raw_writel(val, reg + SCSSIZE);
}
EXPORT_SYMBOL(VIOC_SC_SetSrcSize);

/* HIS_CALLING */
void VIOC_SC_SetDstSize(
	void __iomem *reg, unsigned int nWidth, unsigned int nHeight)
{
	u32 val;

	val = ((nHeight << SCDSIZE_HEIGHT_SHIFT)
	       | (nWidth << SCDSIZE_WIDTH_SHIFT));

	__raw_writel(val, reg + SCDSIZE);
}
EXPORT_SYMBOL(VIOC_SC_SetDstSize);

/* HIS_CALLING */
void VIOC_SC_SetOutSize(
	void __iomem *reg, unsigned int nWidth, unsigned int nHeight)
{
	u32 val;

	val = ((nHeight << SCOSIZE_HEIGHT_SHIFT)
	       | (nWidth << SCOSIZE_WIDTH_SHIFT));

	__raw_writel(val, reg + SCOSIZE);
}
EXPORT_SYMBOL(VIOC_SC_SetOutSize);

void VIOC_SC_SetOutPosition(
	void __iomem *reg, unsigned int nXpos, unsigned int nYpos)
{
	u32 val;

	val = ((nYpos << SCOPOS_YPOS_SHIFT) | (nXpos << SCOPOS_XPOS_SHIFT));

	__raw_writel(val, reg + SCOPOS);
}
EXPORT_SYMBOL(VIOC_SC_SetOutPosition);

void __iomem *VIOC_SC_GetAddress(unsigned int vioc_id)
{
	void __iomem *pScaler = NULL;

	unsigned int Num = get_vioc_index(vioc_id);

	if ((Num >= VIOC_SCALER_MAX) || (ViocSC_np == NULL)) {
		(void)pr_err("[ERR][SC] %s Num:%d max num:%d\n", __func__, Num,
			VIOC_SCALER_MAX);
		pScaler = NULL;
	} else {
		if (pScale[Num] == NULL) {
			/* Prevent KCS warning */
			(void)pr_err("[ERR][SC] scaler number over range %d : function:%s\n  ",
					Num, __func__);
			pScaler = NULL;
		} else {
			/* Prevent KCS warning */
			pScaler = pScale[Num];
		}
	}
	return pScaler;
}
EXPORT_SYMBOL(VIOC_SC_GetAddress);

void VIOC_SCALER_DUMP(const void __iomem *reg, unsigned int vioc_id)
{
	unsigned int cnt = 0;
	const void __iomem *pReg = reg;
	unsigned int Num = get_vioc_index(vioc_id);

	if (Num >= VIOC_SCALER_MAX) {
		/* Prevent KCS warning */
		(void)pr_err("[ERR][SC] %s Num:%d max num:%d\n", __func__, Num,
			VIOC_SCALER_MAX);
	} else {
		if (pReg == NULL) {
			/* Prevent KCS warning */
			pReg = VIOC_SC_GetAddress(vioc_id);
		}
		if (pReg != NULL) {
			(void)pr_info("[DBG][SC] SCALER-%d ::\n", Num);
			while (cnt < 0x20U) {
				(void)pr_info(
					"SCALER-%d + 0x%x: 0x%08x 0x%08x 0x%08x 0x%08x\n", Num, cnt,
					__raw_readl(pReg + cnt), __raw_readl(pReg + cnt + 0x4U),
					__raw_readl(pReg + cnt + 0x8U),
					__raw_readl(pReg + cnt + 0xCU));
				cnt += 0x10U;
			}
		}
	}
}
EXPORT_SYMBOL(VIOC_SCALER_DUMP);

int vioc_sc_init(void)
{
	unsigned int i;

	ViocSC_np = of_find_compatible_node(NULL, NULL, "telechips,scaler");

	if (ViocSC_np == NULL) {
		(void)pr_err("[ERR][SC] cann't find vioc_scaler\n");
	} else {
		for (i = 0; i < VIOC_SCALER_MAX; i++) {
			/* Prevent KCS warning */
			pScale[i] = (void __iomem *)of_iomap(
				ViocSC_np, (int)i);
		}
	}
	return 0;
}
EXPORT_SYMBOL(vioc_sc_init);