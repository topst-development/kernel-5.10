/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_AUDIO_ACFG_H
#define TCC_AUDIO_ACFG_H

#define MAIC0_IDX			(0U)
#define MAIC1_IDX			(MAIC0_IDX + 1U)
#define MAIC2_IDX			(MAIC1_IDX + 1U)
#define MAIC3_IDX			(MAIC2_IDX + 1U)
#define MAIC4_IDX			(MAIC3_IDX + 1U)
#define MAIC5_IDX			(MAIC4_IDX + 1U)
#define MAIC6_IDX			(MAIC5_IDX + 1U)
#define MAIC7_IDX			(MAIC6_IDX + 1U)
#define NOF_MAIC_IDX		(MAIC7_IDX + 1U)

#define SPDIF_PORT0			(0U)
#define SPDIF_PORT1			(SPDIF_PORT0 + 1U)
#define SPDIF_PORT2			(SPDIF_PORT1 + 1U)
#define SPDIF_PORT3			(SPDIF_PORT2 + 1U)
#define SPDIF_PORT4			(SPDIF_PORT3 + 1U)
#define SPDIF_PORT5			(SPDIF_PORT4 + 1U)
#define SPDIF_PORT6			(SPDIF_PORT5 + 1U)
#define SPDIF_PORT7			(SPDIF_PORT6 + 1U)
#define NOF_SPDIF_PORT		(SPDIF_PORT7 + 1U)

#define DP_PORT0			(0U)
#define DP_PORT1			(DP_PORT0 + 1U)
#define DP_PORT2			(DP_PORT1 + 1U)
#define DP_PORT3			(DP_PORT2 + 1U)
#define NOF_DP_PORT			(DP_PORT3 + 1U)

#define IDX_MAIC_PROT_CACHE0	(0x0U)
#define IDX_MAIC_PROT_CACHE1	(IDX_MAIC_PROT_CACHE0 + 0x1U)
#define IDX_MAIC_PROT_CACHE2	(IDX_MAIC_PROT_CACHE1 + 0x1U)
#define IDX_MAIC_PROT_CACHE3	(IDX_MAIC_PROT_CACHE2 + 0x1U)
#define IDX_MAIC_PROT_CACHE4	(IDX_MAIC_PROT_CACHE3 + 0x1U)
#define IDX_MAIC_PROT_CACHE5	(IDX_MAIC_PROT_CACHE4 + 0x1U)
#define IDX_MAIC_PROT_CACHE6	(IDX_MAIC_PROT_CACHE5 + 0x1U)
#define IDX_MAIC_PROT_CACHE7	(IDX_MAIC_PROT_CACHE6 + 0x1U)
#define IDX_MAVC_PROT_CACHE0	(IDX_MAIC_PROT_CACHE7 + 0x1U)
#define IDX_MAVC_PROT_CACHE1	(IDX_MAVC_PROT_CACHE0 + 0x1U)
#define IDX_MAVC_PROT_CACHE2	(IDX_MAVC_PROT_CACHE1 + 0x1U)
#define IDX_MAVC_PROT_CACHE3	(IDX_MAVC_PROT_CACHE2 + 0x1U)
#define IDX_MAVC_PROT_CACHE4	(IDX_MAVC_PROT_CACHE3 + 0x1U)
#define IDX_MAVC_PROT_CACHE5	(IDX_MAVC_PROT_CACHE4 + 0x1U)
#define IDX_MAVC_PROT_CACHE6	(IDX_MAVC_PROT_CACHE5 + 0x1U)
#define IDX_MAVC_PROT_CACHE7	(IDX_MAVC_PROT_CACHE6 + 0x1U)
#define IDX_MARS_PROT_CACHE0	(IDX_MAVC_PROT_CACHE7 + 0x1U)
#define IDX_MARS_PROT_CACHE1	(IDX_MARS_PROT_CACHE0 + 0x1U)
#define IDX_MARS_PROT_CACHE2	(IDX_MARS_PROT_CACHE1 + 0x1U)
#define IDX_MARS_PROT_CACHE3	(IDX_MARS_PROT_CACHE2 + 0x1U)
#define IDX_MARS_PROT_CACHE4	(IDX_MARS_PROT_CACHE3 + 0x1U)
#define IDX_MARS_PROT_CACHE5	(IDX_MARS_PROT_CACHE4 + 0x1U)
#define IDX_MASM_PROT_CACHE0	(IDX_MARS_PROT_CACHE5 + 0x1U)
#define IDX_MASM_PROT_CACHE1	(IDX_MASM_PROT_CACHE0 + 0x1U)
#define IDX_MASM_PROT_CACHE2	(IDX_MASM_PROT_CACHE1 + 0x1U)
#define IDX_MASM_PROT_CACHE3	(IDX_MASM_PROT_CACHE2 + 0x1U)
#define IDX_MASM_PROT_CACHE4	(IDX_MASM_PROT_CACHE3 + 0x1U)
#define IDX_MASRC_PROT_CACHE0	(IDX_MASM_PROT_CACHE4 + 0x1U)
#define IDX_MASRC_PROT_CACHE1	(IDX_MASRC_PROT_CACHE0 + 0x1U)
#define IDX_MASRC_PROT_CACHE2	(IDX_MASRC_PROT_CACHE1 + 0x1U)
#define IDX_MASRC_PROT_CACHE3	(IDX_MASRC_PROT_CACHE2 + 0x1U)
#define IDX_MASRC_PROT_CACHE4	(IDX_MASRC_PROT_CACHE3 + 0x1U)
#define IDX_MASRC_PROT_CACHE5	(IDX_MASRC_PROT_CACHE4 + 0x1U)
#define IDX_ACFG_PROT_CACHE0	(IDX_MASRC_PROT_CACHE5 + 0x1U)
#define IDX_ACFG_PROT_CACHE1	(IDX_ACFG_PROT_CACHE0 + 0x1U)
#define IDX_ACFG_PROT_CACHE2	(IDX_ACFG_PROT_CACHE1 + 0x1U)
#define IDX_ACFG_PROT_CACHE3	(IDX_ACFG_PROT_CACHE2 + 0x1U)
#define IDX_ARRAY_PROT_CACHE	(IDX_ACFG_PROT_CACHE3 + 0x1U)


#if 0//DEBUG
#define cfg_writel(v, c)                        \
        ({(void)pr_info("<ASoC> AUDIO_CFG_REG(%p) = 0x%08x\n", c, (unsigned int)v); \
        writel(v, c); })
#else
#define cfg_writel(v, c)                        writel(v, c)
#endif

static inline void tcc_audio_cfg_wr(void __iomem *base_addr, uint32_t offset, uint32_t data) {
   	cfg_writel(data, base_addr + offset);
}

static inline uint32_t tcc_audio_cfg_rd(const void __iomem *base_addr, uint32_t offset) {
    uint32_t    data = 0;

   	data = readl(base_addr + offset);
    return  data;
}

static inline void tcc_audio_cfg_daif_port_sel(void __iomem *base_addr, uint32_t maic_idx, uint port_sel) {
	uint32_t data = tcc_audio_cfg_rd(base_addr, TCC_CFG_DAIF_PORT_SEL);

    if(port_sel >= NOF_MAIC_IDX) {
	if(port_sel != 99U){
	        (void)pr_err("port sel err. daif maic[%d] port_sel[%d]is invalid", maic_idx, port_sel);
	}
    }else{
		port_sel &= ACFG_DAIF_PORT_SEL_MAIC_Msk;
		data &= ~(ui_lshift(ACFG_DAIF_PORT_SEL_MAIC_Msk, ui_to_ui_mul(maic_idx, 4U)));
		data |= ui_lshift(port_sel, ui_to_ui_mul(maic_idx, 4U));

		tcc_audio_cfg_wr(base_addr, TCC_CFG_DAIF_PORT_SEL, data);
    }
}

static inline void tcc_audio_cfg_spdif_port_sel(void __iomem *base_addr, uint32_t port_sel, uint maic_idx) {
	uint32_t data = tcc_audio_cfg_rd(base_addr, TCC_CFG_SPDIF_PORT_SEL);
    if(maic_idx >= NOF_MAIC_IDX) {
	if(maic_idx != 99U){
	        (void)pr_err("port sel err. spdif maic[%d] port_sel[%d] is invalid", maic_idx, port_sel);
	}
    }else{
		maic_idx &= ACFG_SPDIF_PORT_SEL_MAIC_Msk;
		data &= ~(ui_lshift(ACFG_SPDIF_PORT_SEL_MAIC_Msk, ui_to_ui_mul(port_sel, 4U)));
		data |= ui_lshift(maic_idx, ui_to_ui_mul(port_sel, 4U));

		tcc_audio_cfg_wr(base_addr, TCC_CFG_SPDIF_PORT_SEL, data);
    }
}

static inline void tcc_audio_cfg_dp_port_sel(void __iomem *base_addr, uint32_t port_sel, uint maic_idx) {
	uint32_t data = tcc_audio_cfg_rd(base_addr, TCC_CFG_DP_PORT_SEL);

    if(maic_idx >= NOF_MAIC_IDX) {
	if(maic_idx != 99U){
        	(void)pr_err("port sel err. dp maic[%d] port_sel[%d] is invalid", maic_idx, port_sel);
	}
    }else{
		maic_idx &= ACFG_DP_PORT_SEL_MAIC_Msk;
		data &= ~(ui_lshift(ACFG_DP_PORT_SEL_MAIC_Msk, ui_to_ui_mul(port_sel, 4U)));
		data |= ui_lshift(maic_idx, ui_to_ui_mul(port_sel, 4U));

		tcc_audio_cfg_wr(base_addr, TCC_CFG_DP_PORT_SEL, data);
    }
}

static inline void tcc_audio_cfg_set_ar_prot(void __iomem *base_addr, uint32_t prot_cache_idx, uint32_t prot){
	uint32_t data = tcc_audio_cfg_rd(base_addr, ui_add(TCC_CFG_MAIC_PROT_CACHE0, ui_to_ui_mul(prot_cache_idx, 4)));

	data = data & ~(ACFG_PROT_CACHE_AR_PROT_SEL_Msk | ACFG_PROT_CACHE_AR_PROT_CFG_Msk);
	data = data | ui_lshift(prot, ACFG_PROT_CACHE_AR_PROT_CFG_Pos);
	tcc_audio_cfg_wr(base_addr, ui_add(TCC_CFG_MAIC_PROT_CACHE0, ui_to_ui_mul(prot_cache_idx, 4)), data);
}

static inline void tcc_audio_cfg_set_aw_prot(void __iomem *base_addr, uint32_t prot_cache_idx, uint32_t prot){
	uint32_t data = tcc_audio_cfg_rd(base_addr, ui_add(TCC_CFG_MAIC_PROT_CACHE0, ui_to_ui_mul(prot_cache_idx,4)));

	data = data & ~(ACFG_PROT_CACHE_AW_PROT_SEL_Msk | ACFG_PROT_CACHE_AW_PROT_CFG_Msk);
	data = data | ui_lshift(prot, ACFG_PROT_CACHE_AW_PROT_CFG_Pos);
	tcc_audio_cfg_wr(base_addr, ui_add(TCC_CFG_MAIC_PROT_CACHE0, ui_to_ui_mul(prot_cache_idx, 4)), data);
}

static inline void tcc_audio_cfg_set_ar_cache(void __iomem *base_addr, uint32_t prot_cache_idx, uint32_t cache){
	uint32_t data = tcc_audio_cfg_rd(base_addr, ui_add(TCC_CFG_MAIC_PROT_CACHE0, ui_to_ui_mul(prot_cache_idx, 4)));

	data = data & ~(ACFG_PROT_CACHE_AR_CACHE_SEL_Msk | ACFG_PROT_CACHE_AR_CACHE_CFG_Msk);
	data = data | ui_lshift(cache, ACFG_PROT_CACHE_AR_CACHE_CFG_Pos);
	tcc_audio_cfg_wr(base_addr, ui_add(TCC_CFG_MAIC_PROT_CACHE0,  ui_to_ui_mul(prot_cache_idx, 4)), data);
}

static inline void tcc_audio_cfg_set_aw_cache(void __iomem *base_addr, uint32_t prot_cache_idx, uint32_t cache){
	uint32_t data = tcc_audio_cfg_rd(base_addr, ui_add(TCC_CFG_MAIC_PROT_CACHE0, ui_to_ui_mul(prot_cache_idx, 4)));

	data = data & ~(ACFG_PROT_CACHE_AW_CACHE_SEL_Msk | ACFG_PROT_CACHE_AW_CACHE_CFG_Msk);
	data = data | ui_lshift(cache, ACFG_PROT_CACHE_AW_CACHE_CFG_Pos);
	tcc_audio_cfg_wr(base_addr, ui_add(TCC_CFG_MAIC_PROT_CACHE0,  ui_to_ui_mul(prot_cache_idx, 4)), data);
}

static inline uint32_t tcc_audio_cfg_get_ar_prot(const void __iomem *base_addr, uint32_t prot_cache_idx){
	uint32_t prot;
	uint32_t data = tcc_audio_cfg_rd(base_addr, ui_add(TCC_CFG_MAIC_PROT_CACHE0, ui_to_ui_mul(prot_cache_idx, 4)));

	data = data & (ACFG_PROT_CACHE_AR_PROT_SEL_Msk | ACFG_PROT_CACHE_AR_PROT_CFG_Msk);
	prot = ui_rshift(data, ACFG_PROT_CACHE_AR_PROT_CFG_Pos);
	return prot;
}

static inline uint32_t tcc_audio_cfg_get_aw_prot(const void __iomem *base_addr, uint32_t prot_cache_idx){
	uint32_t prot;
	uint32_t data = tcc_audio_cfg_rd(base_addr, ui_add(TCC_CFG_MAIC_PROT_CACHE0, ui_to_ui_mul(prot_cache_idx, 4)));

	data = data & (ACFG_PROT_CACHE_AW_PROT_SEL_Msk | ACFG_PROT_CACHE_AW_PROT_CFG_Msk);
	prot = ui_rshift(data, ACFG_PROT_CACHE_AW_PROT_CFG_Pos);
	return prot;
}

static inline uint32_t tcc_audio_cfg_get_ar_cache(const void __iomem *base_addr, uint32_t prot_cache_idx){
	uint32_t cache;
	uint32_t data = tcc_audio_cfg_rd(base_addr, ui_add(TCC_CFG_MAIC_PROT_CACHE0, ui_to_ui_mul(prot_cache_idx, 4)));

	data = data & (ACFG_PROT_CACHE_AR_CACHE_SEL_Msk | ACFG_PROT_CACHE_AR_CACHE_CFG_Msk);
	cache = ui_rshift(data, ACFG_PROT_CACHE_AR_CACHE_CFG_Pos);
	return cache;
}

static inline uint32_t tcc_audio_cfg_get_aw_cache(const void __iomem *base_addr, uint32_t prot_cache_idx){
	uint32_t cache;
	uint32_t data = tcc_audio_cfg_rd(base_addr, ui_add(TCC_CFG_MAIC_PROT_CACHE0, ui_to_ui_mul(prot_cache_idx, 4)));

	data = data & (ACFG_PROT_CACHE_AW_CACHE_SEL_Msk | ACFG_PROT_CACHE_AW_CACHE_CFG_Msk);
	cache = ui_rshift(data, ACFG_PROT_CACHE_AW_CACHE_CFG_Pos);
	return cache;
}

static inline void tcc_audio_cfg_cache_and_power_init(void __iomem *base_addr){
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MAIC_PROT_CACHE0, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MAIC_PROT_CACHE1, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MAIC_PROT_CACHE2, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MAIC_PROT_CACHE3, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MAIC_PROT_CACHE4, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MAIC_PROT_CACHE5, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MAIC_PROT_CACHE6, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MAIC_PROT_CACHE7, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MAVC_PROT_CACHE0, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MAVC_PROT_CACHE1, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MAVC_PROT_CACHE2, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MAVC_PROT_CACHE3, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MAVC_PROT_CACHE4, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MAVC_PROT_CACHE5, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MAVC_PROT_CACHE6, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MAVC_PROT_CACHE7, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MARS_PROT_CACHE0, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MARS_PROT_CACHE1, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MARS_PROT_CACHE2, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MARS_PROT_CACHE3, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MARS_PROT_CACHE4, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MARS_PROT_CACHE5, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MASM_PROT_CACHE0, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MASM_PROT_CACHE1, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MASM_PROT_CACHE2, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MASM_PROT_CACHE3, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MASM_PROT_CACHE4, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MASRC_PROT_CACHE0, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MASRC_PROT_CACHE1, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MASRC_PROT_CACHE2, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MASRC_PROT_CACHE3, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MASRC_PROT_CACHE4, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_MASRC_PROT_CACHE5, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_ACFG_PROT_CACHE0, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_ACFG_PROT_CACHE1, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_ACFG_PROT_CACHE2, 0x00220022);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_ACFG_PROT_CACHE3, 0x00220022);

	tcc_audio_cfg_wr(base_addr, TCC_CFG_X2X_MST_CLK_CFG, 0x00002511);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_X2X_SLV_CLK_CFG, 0x00002511);
	tcc_audio_cfg_wr(base_addr, TCC_CFG_X2X_SLV_PWR_CFG, 0x00002511);
}
#endif /* TCC_AUDIO_ACFG_H */
