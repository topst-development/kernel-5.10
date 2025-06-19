/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */
/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TCC_AUDIO_CCU_H
#define TCC_AUDIO_CCU_H

#if 0
#define AUDIO_CKC_REG_INFO( _NAME, _ADDR, _ACCESS, _RESET, _WR_MASK) \
    AUDIO_CKC_ ## _NAME,
enum {
#include "audio_ckc_reg_info.h"
    AUDIO_CKC_REG_NUM
};
#endif

//----------------------------------------------------------------------------------------------------------------------------
// Define
//----------------------------------------------------------------------------------------------------------------------------
// Audio subsystem XIN frequency
#define AUDIO_XIN_FREQ_MHZ     (24U)
#define AUDIO_XIN_FREQ_KHZ     (24000U)
#define AUDIO_XIN_FREQ_HZ      (24000000U)
#define AUDIO_PLL_MIN_P        (1U)
#define AUDIO_PLL_MAX_P        (63U)

//----------------------------------------------------------------------------------------------------------------------------
// define
//----------------------------------------------------------------------------------------------------------------------------

#define AUDIO_PLL0		(0U)
#define AUDIO_PLL1		(1U)

//AudioCKCSource
#define BCLK_SRC_XIN			(0U)
#define BCLK_SRC_PLL0_FOUT		(1U)
#define BCLK_SRC_PLL1_FOUT		(2U)
#define BCLK_SRC_XIN_DIV		(3U)
#define BCLK_SRC_PLL0_FOUT_DIV	(4U)
#define BCLK_SRC_PLL1_FOUT_DIV	(5U)
#define BCLK_SRC_RESERVED0		(6U)
#define BCLK_SRC_RESERVED1		(7U)

//NewAudioPeriClock
#define PCLK_AIC0_DAI   	(0U)
#define PCLK_AIC0_FILTER	(1U)
#define PCLK_AIC0_SPDIF 	(2U)
#define PCLK_AIC1_DAI   	(3U)
#define PCLK_AIC1_FILTER	(4U)
#define PCLK_AIC1_SPDIF 	(5U)
#define PCLK_AIC2_DAI   	(6U)
#define PCLK_AIC2_FILTER	(7U)
#define PCLK_AIC2_SPDIF 	(8U)
#define PCLK_AIC3_DAI   	(9U)
#define PCLK_AIC3_FILTER	(10U)
#define PCLK_AIC3_SPDIF 	(11U)
#define PCLK_AIC4_DAI   	(12U)
#define PCLK_AIC4_FILTER	(13U)
#define PCLK_AIC4_SPDIF 	(14U)
#define PCLK_AIC5_DAI   	(15U)
#define PCLK_AIC5_FILTER	(16U)
#define PCLK_AIC5_SPDIF 	(17U)
#define PCLK_AIC6_DAI   	(18U)
#define PCLK_AIC6_FILTER	(19U)
#define PCLK_AIC6_SPDIF 	(20U)
#define PCLK_AIC7_DAI   	(21U)
#define PCLK_AIC7_FILTER	(22U)
#define PCLK_AIC7_SPDIF 	(23U)

//AudioPeriClock
#define PCLK_MAIC0_DAI           (0U)
#define PCLK_MAIC0_FILTER        (1U)
#define PCLK_MAIC0_SPDIF         (2U)
#define PCLK_MAIC1_DAI           (3U)
#define PCLK_MAIC1_FILTER        (4U)
#define PCLK_MAIC1_SPDIF         (5U)
#define PCLK_MAIC2_DAI           (6U)
#define PCLK_MAIC2_FILTER        (7U)
#define PCLK_MAIC2_SPDIF         (8U)
#define PCLK_MAIC3_DAI           (9U)
#define PCLK_MAIC3_FILTER        (10U)
#define PCLK_MAIC3_SPDIF         (11U)
#define PCLK_SAIC0_DAI           (12U)
#define PCLK_SAIC0_FILTER        (13U)
#define PCLK_SAIC0_SPDIF         (14U)
#define PCLK_SAIC1_DAI           (15U)
#define PCLK_SAIC1_FILTER        (16U)
#define PCLK_SAIC1_SPDIF         (17U)
#define PCLK_SAIC2_DAI           (18U)
#define PCLK_SAIC2_FILTER        (19U)
#define PCLK_SAIC2_SPDIF         (20U)
#define PCLK_SAIC3_DAI           (21U)
#define PCLK_SAIC3_FILTER        (22U)
#define PCLK_SAIC3_SPDIF         (23U)
#define PCLK_MASRC_AUX0          (24U)
#define PCLK_MASRC_AUX1          (25U)
#define PCLK_MASRC_AUX2          (26U)
#define PCLK_MASRC_AUX3          (27U)

//AudioPeriClockSource
#define PCLK_SRC_PLL0_FOUT      (0U)
#define PCLK_SRC_PLL1_FOUT      (1U)
#define PCLK_SRC_XIN            (5U)
#define PCLK_SRC_PLL0_FOUT_DIV  (10U)
#define PCLK_SRC_PLL1_FOUT_DIV  (11U)
#define PCLK_SRC_DP_PLL_FOUT    (21U)
#define PCLK_SRC_XIN_DIV        (23U)
#define PCLK_SRC_EXT_CLK0       (25U)
#define PCLK_SRC_EXT_CLK1       (26U)

//----------------------------------------------------------------------------------------------------------------------------
// PLL
//----------------------------------------------------------------------------------------------------------------------------
// P[ 5:0] : Division value of the  6-bit programmable pre-divider
// - Unsigned integer
// - 6'b00_0001(1) <= P <=  6'b11_1111(63)
// - 6MHz <= FREF(FIN/P) <= 30MHz
//
// M[ 9:0] : Division value of the 10-bit programmable main-divider
// - Unsigned integer
// - 10'b00_0001_0000(16) <= M <= 10'b01_1111_1111(1023)
//
// S[ 2:0] : Division value of the  3-bit programmable scaler
// - Unsigned integer
// - 3'b000(0) <= S <= 3'b110(6)
//
// K[15:0] : Value of 16-bit DSM
// - Two's complement integer
// - 16'b1000_0000_0000_0000(-32678) <= K <= 16'b0111_1111_1111_1111(32767)
//
// FVCO = ((M + K/65536) x FIN) / P
// FOUT = ((M + K/65536) x FIN) / (P x 2^S)
// - Fout frequency                                        =   25MHz < Fout < 3200MHz
// - Fin  frequency                                        =    4MHz < Fin  <  300MHz
// - Fvco(Voltage Controlled Oscillator frequency)         = 1600MHz < Fvco < 3200MHz
// - Fref(Reference frequency)                     = Fin/P =    6MHz < Fref <   30MHz

#define MAX_FOUT    (3200U) /* MHz */
#define MIN_FOUT    (25U) /* MHz */

#define MAX_FIN     (30U) /* MHz */
#define MIN_FIN     (4U) /* MHz */

#define MAX_FVCO    (3200U) /* MHz */
#define MIN_FVCO    (1600U) /* MHz */

#define MAX_FREF    (30U) /* MHz */
#define MIN_FREF    (6U) /* MHz */

#define TCC_PERI_IDX_MAIC0_DAIF		(0)
#define TCC_PERI_IDX_MAIC0_FILTER	(1)
#define TCC_PERI_IDX_MAIC0_SPIDF	(2)
#define TCC_PERI_IDX_MAIC1_DAIF		(3)
#define TCC_PERI_IDX_MAIC1_FILTER	(4)
#define TCC_PERI_IDX_MAIC1_SPIDF	(5)
#define TCC_PERI_IDX_MAIC2_DAIF		(6)
#define TCC_PERI_IDX_MAIC2_FILTER	(7)
#define TCC_PERI_IDX_MAIC2_SPIDF	(8)
#define TCC_PERI_IDX_MAIC3_DAIF		(9)
#define TCC_PERI_IDX_MAIC3_FILTER	(10)
#define TCC_PERI_IDX_MAIC3_SPIDF	(11)
#define TCC_PERI_IDX_MAIC4_DAIF		(12)
#define TCC_PERI_IDX_MAIC4_FILTER	(13)
#define TCC_PERI_IDX_MAIC4_SPIDF	(14)
#define TCC_PERI_IDX_MAIC5_DAIF		(15)
#define TCC_PERI_IDX_MAIC5_FILTER	(16)
#define TCC_PERI_IDX_MAIC5_SPIDF	(17)
#define TCC_PERI_IDX_MAIC6_DAIF		(18)
#define TCC_PERI_IDX_MAIC6_FILTER	(19)
#define TCC_PERI_IDX_MAIC6_SPIDF	(20)
#define TCC_PERI_IDX_MAIC7_DAIF		(21)
#define TCC_PERI_IDX_MAIC7_FILTER	(22)
#define TCC_PERI_IDX_MAIC7_SPIDF	(23)
#define TCC_PERI_IDX_MASRC_AUX0		(24)
#define TCC_PERI_IDX_MASRC_AUX1		(25)
#define TCC_PERI_IDX_MASRC_AUX2		(26)
#define TCC_PERI_IDX_MASRC_AUX3		(27)

#define TCC_PCLK_SEL_PLL0_FOUT			((uint32_t)0U)
#define TCC_PCLK_SEL_PLL1_FOUT			((uint32_t)1U)
#define TCC_PCLK_SEL_XIN				((uint32_t)5U)
#define TCC_PCLK_SEL_PLL0_FOUT_DIV		((uint32_t)10U)
#define TCC_PCLK_SEL_PLL1_FOUT_DIV		((uint32_t)11U)
#define TCC_PCLK_SEL_DP_PLL_OUT			((uint32_t)21U)
#define TCC_PCLK_SEL_XIN_DIV			((uint32_t)23U)
#define TCC_PCLK_SEL_EXT_CLK0			((uint32_t)25U)
#define TCC_PCLK_SEL_EXT_CLK1			((uint32_t)26U)

#define TCC_CPU_CLK_SEL_XIN				((uint32_t)0x0U)
#define TCC_CPU_CLK_SEL_PLL0_FOUT		((uint32_t)0x1U)
#define TCC_CPU_CLK_SEL_PLL1_FOUT		((uint32_t)0x2U)
#define TCC_CPU_CLK_SEL_XIN_DIV			((uint32_t)0x3U)
#define TCC_CPU_CLK_SEL_PLL0_FOUT_DIV	((uint32_t)0x4U)
#define TCC_CPU_CLK_SEL_PLL1_FOUT_DIV	((uint32_t)0x5U)

#define MAX_CNT_CLK_STABLE	(100000U)
#if 0//DEBUG
#define ccu_writel(v, c)                        \
        ({(void)pr_info("<ASoC> CCU_REG(%p) = 0x%08x\n", c, (unsigned int)v); \
        writel(v, c); })
#else
#define ccu_writel(v, c)                        writel(v, c)
#endif

static inline void tcc_audio_ccu_set_pll_auto(void __iomem *base_addr, uint32_t pll_id, uint32_t fout_freq) {
    uint32_t pll_p;
    uint32_t pll_m;
    uint32_t pll_s;
	uint32_t pll_k = 0;
    uint32_t pll_fvco;
	uint32_t max_cnt_clk_stable = MAX_CNT_CLK_STABLE;
    //uint32_t pll_fref;
    int32_t err_set_pll = 0;
    uint32_t value_pll_pms;
    uint32_t value_pll_con;
    uint32_t value_pll_mon;
    uint32_t offset_pll_pms;
    uint32_t offset_pll_con;
    uint32_t offset_pll_mon;

    if(pll_id == AUDIO_PLL0) {
        offset_pll_pms = TCC_CCU_PLL0_PMS_OFFSET;
        offset_pll_con = TCC_CCU_PLL0_CON_OFFSET;
        offset_pll_mon = TCC_CCU_PLL0_MON_OFFSET;
    } else if (pll_id == AUDIO_PLL1) {
        offset_pll_pms = TCC_CCU_PLL1_PMS_OFFSET;
        offset_pll_con = TCC_CCU_PLL1_CON_OFFSET;
        offset_pll_mon = TCC_CCU_PLL1_MON_OFFSET;
    } else {
        (void)pr_err("PLL ID setting error, Unsupported ID is used.");
		err_set_pll = -1;
    }

    if ((AUDIO_XIN_FREQ_MHZ > MAX_FIN) || (AUDIO_XIN_FREQ_MHZ <  MIN_FIN)) {
        (void)pr_err("PLL FIN is out of range(%dMHz < Fout < %dMHz).", MIN_FIN, MAX_FIN);
		err_set_pll = -1;
    }

    if ((fout_freq > MAX_FOUT) || (fout_freq < MIN_FOUT)) {
        (void)pr_err("PLL FOUT is out of range(%dMHz < Fout < %dMHz).", MIN_FOUT, MAX_FOUT);
        err_set_pll = -1;
    }

	if(err_set_pll == 0){
	    // Find minimum value of S
	    pll_s = 0U;
	    while ((fout_freq << pll_s) < MIN_FVCO) {
	        pll_s++;
	    }

	    do{
	        if (pll_s > 6U) {
	            (void)pr_err("PLL S is out of range(0 <= S <= 6).");
	            err_set_pll = -1;
	        }else{
				pll_p = AUDIO_PLL_MIN_P;
		        do{              // P value is integer between 2 and 6 because xin_freq is 24MHz
		            pll_fvco = fout_freq << pll_s;

		            if ((pll_fvco > MIN_FVCO) && (pll_fvco < MAX_FVCO)) {
		                if ((ui_to_ui_mul(pll_fvco, pll_p) % AUDIO_XIN_FREQ_MHZ) == 0U) {                   // if M is integer
		                    pll_m = ui_to_ui_mul(pll_fvco, pll_p) / AUDIO_XIN_FREQ_MHZ;

		                    if ( (pll_m < 64U) || (pll_m > 1023U) ) {
		                        (void)pr_err("PLL M is out of range(64 <= M <= 2047).");
		                        err_set_pll = -1;
		                    }else{
			                    err_set_pll = 1;
		                    }
		                }
		            }
		        }while((++pll_p<= AUDIO_PLL_MAX_P) && (err_set_pll == 0));\
	        }
	    }while(((fout_freq << ++pll_s) <= MAX_FVCO) && (err_set_pll == 0));

	    if (err_set_pll != 1) {
	        (void)pr_err("Error : PLL PMS didn't search.");
	    }else{
			value_pll_pms = readl(base_addr + offset_pll_pms);
			value_pll_con = readl(base_addr + offset_pll_con);
			value_pll_mon = readl(base_addr + offset_pll_mon);

			value_pll_pms &= ~CCU_PMS_RESETB_Msk;
			value_pll_pms |= CCU_PMS_RESETB_DISABLE_PLL;
			ccu_writel(value_pll_pms, base_addr + offset_pll_pms);
			udelay(1);

			value_pll_pms &= ~(CCU_PMS_P_Msk|CCU_PMS_M_Msk|CCU_PMS_S_Msk);
			value_pll_pms |= (pll_p << CCU_PMS_P_Pos);
			value_pll_pms |= (pll_m << CCU_PMS_M_Pos);
			value_pll_pms |= (pll_s << CCU_PMS_S_Pos);

			value_pll_con &= ~(CCU_CON_K_Msk);
			value_pll_con |= (pll_k << CCU_CON_K_Pos);

			value_pll_mon &= ~(CCU_MON_LOCKCNT_Msk|CCU_MON_LOCK_EN_Msk);
			value_pll_mon |= ui_lshift(500U, CCU_MON_LOCKCNT_Pos);
			value_pll_mon |= CCU_MON_LOCK_EN_ENABLE;

			ccu_writel(value_pll_mon, base_addr + offset_pll_mon);
			ccu_writel(value_pll_con, base_addr + offset_pll_con);
			ccu_writel(value_pll_pms, base_addr + offset_pll_pms);

			value_pll_pms &= ~CCU_PMS_RESETB_Msk;
			value_pll_pms |= CCU_PMS_RESETB_ENABLE_PLL;
			ccu_writel(value_pll_pms, base_addr + offset_pll_pms);

			while(max_cnt_clk_stable != 0U) {
				value_pll_mon = readl(base_addr + offset_pll_mon);
				if((value_pll_mon & CCU_MON_LOCK_Msk) == CCU_MON_LOCK_LOCKED_PLL){
					break;
				}
				max_cnt_clk_stable--;
			}

			if(max_cnt_clk_stable == 0U){
				(void)pr_err("pll lock is failed");;
			}else{
				(void)pr_info("pll%d(P : %x, M : %x, S : %x, K : %x) locked", pll_id, pll_p, pll_m, pll_s, pll_k);
				(void)pr_info("[Info] PLL-%d(Fout : %dMHz) locked", pll_id, fout_freq);
			}
	    }
	}
}

static inline void tcc_audio_ccu_set_pll_manual(void __iomem *base_addr, uint32_t pll_id, uint32_t pll_p, uint32_t pll_m, uint32_t pll_s, uint32_t pll_k) {
	uint32_t max_cnt_clk_stable = MAX_CNT_CLK_STABLE;
    uint32_t pll_fref;
    int32_t err_set_pll = 0;
    uint32_t value_pll_pms;
    uint32_t value_pll_con;
    uint32_t value_pll_mon;
    uint32_t offset_pll_pms;
    uint32_t offset_pll_con;
    uint32_t offset_pll_mon;

    if(pll_id == AUDIO_PLL0) {
        offset_pll_pms = TCC_CCU_PLL0_PMS_OFFSET;
        offset_pll_con = TCC_CCU_PLL0_CON_OFFSET;
        offset_pll_mon = TCC_CCU_PLL0_MON_OFFSET;
    } else if (pll_id == AUDIO_PLL1) {
        offset_pll_pms = TCC_CCU_PLL1_PMS_OFFSET;
        offset_pll_con = TCC_CCU_PLL1_CON_OFFSET;
        offset_pll_mon = TCC_CCU_PLL1_MON_OFFSET;
    } else {
        (void)pr_err("PLL ID setting error, Unsupported ID is used.");
		err_set_pll = -1;
    }

    if ((AUDIO_XIN_FREQ_MHZ > MAX_FIN) || (AUDIO_XIN_FREQ_MHZ < MIN_FIN)) {
        (void)pr_err("PLL fin is out of range(4 < Fin < 300).");
        err_set_pll = -1;
    }

    // Check PLL-P/M/S
    if ((pll_p <  1U) || (pll_p > 63U)) {
        (void)pr_err("PLL P is out of range(1 <= P < 63).");
        err_set_pll = -1;
    }

    if ((pll_m < 64U) || (pll_m > 2047U)) {
        (void)pr_err("PLL M is out of range(64 <= M <= 2047).");
        err_set_pll = -1;
    }

    if ( /* (pll_s <  0U) || */ (pll_s > 6U)) {
        (void)pr_err("PLL S is out of range(0 <= S <= 6).");
        err_set_pll = -1;
    }

    if (pll_k >= 65536U) {
        (void)pr_err("PLL S is out of range(K < 65536).");
        err_set_pll = -1;
    }

	if(err_set_pll != -1){
	    pll_fref = (uint32_t)(AUDIO_XIN_FREQ_MHZ/pll_p);
	    if ((pll_fref <  MIN_FREF) || (pll_fref > MAX_FREF)) {
	        (void)pr_err("PLL Fref is out of range(6 < Fref < 12).");
	        err_set_pll = -1;
	    }
	}

	if(err_set_pll == 0){
	    value_pll_pms = readl(base_addr + offset_pll_pms);
	    value_pll_con = readl(base_addr + offset_pll_con);
	    value_pll_mon = readl(base_addr + offset_pll_mon);

		value_pll_pms &= ~CCU_PMS_RESETB_Msk;
		value_pll_pms |= CCU_PMS_RESETB_DISABLE_PLL;
		ccu_writel(value_pll_pms, base_addr + offset_pll_pms);
	    udelay(1);

		value_pll_pms &= ~(CCU_PMS_P_Msk|CCU_PMS_M_Msk|CCU_PMS_S_Msk);
		value_pll_pms |= (pll_p << CCU_PMS_P_Pos);
		value_pll_pms |= (pll_m << CCU_PMS_M_Pos);
		value_pll_pms |= (pll_s << CCU_PMS_S_Pos);

		value_pll_con &= ~(CCU_CON_K_Msk);
		value_pll_con |= (pll_k << CCU_CON_K_Pos);

		value_pll_mon &= ~(CCU_MON_LOCKCNT_Msk|CCU_MON_LOCK_EN_Msk);
		value_pll_mon |= ui_lshift(500U, CCU_MON_LOCKCNT_Pos);
		value_pll_mon |= CCU_MON_LOCK_EN_ENABLE;

		ccu_writel(value_pll_mon, base_addr + offset_pll_mon);
		ccu_writel(value_pll_con, base_addr + offset_pll_con);
		ccu_writel(value_pll_pms, base_addr + offset_pll_pms);

		value_pll_pms &= ~CCU_PMS_RESETB_Msk;
		value_pll_pms |= CCU_PMS_RESETB_ENABLE_PLL;
		ccu_writel(value_pll_pms, base_addr + offset_pll_pms);

	    while(max_cnt_clk_stable != 0U) {
	        value_pll_mon = readl(base_addr + offset_pll_mon);
			if((value_pll_mon & CCU_MON_LOCK_Msk) == CCU_MON_LOCK_LOCKED_PLL){
				break;
			}
			--max_cnt_clk_stable;
	    }

		if(max_cnt_clk_stable == 0U){
			(void)pr_err("pll lock is failed");;
		}else{
            (void)pr_info("pll%d(P : %x, M : %x, S : %x, K : %x) locked", pll_id, pll_p, pll_m, pll_s, pll_k);
		}
	}
}


static inline void tcc_audio_ccu_set_xin_fout_div(void __iomem *base_addr, uint32_t clk_div) {
	uint32_t max_cnt_clk_stable = MAX_CNT_CLK_STABLE;
    uint32_t value = readl(base_addr + TCC_CCU_SRC_CLK_DIV_OFFSET);

    if(((clk_div << CCU_SRC_CLK_DIV_XIN_DIV_Pos) == (value & CCU_SRC_CLK_DIV_XIN_DIV_Msk)) \
		&& ((value & CCU_SRC_CLK_DIV_XIN_DIV_EN_Msk) == CCU_SRC_CLK_DIV_XIN_DIV_EN_ENABLE)){
			(void)pr_info("XIN div is already set");
	}else{
			if(clk_div > 0x3FU){
					(void)pr_err("XIN div(%d) is out of range(0x00 ~ 0x3f).", clk_div);
			}else{
				value &= ~(CCU_SRC_CLK_DIV_XIN_DIV_EN_Msk | CCU_SRC_CLK_DIV_XIN_DIV_Msk);
				value |= CCU_SRC_CLK_DIV_XIN_DIV_EN_ENABLE;
				value |= (clk_div << CCU_SRC_CLK_DIV_XIN_DIV_Pos);
				ccu_writel(value, base_addr + TCC_CCU_BUS_CLK_CTRL_OFFSET);

				while(max_cnt_clk_stable != 0U) {
					value = readl(base_addr + TCC_CCU_SRC_CLK_DIV_OFFSET);
					if((value & CCU_SRC_CLK_DIV_XIN_DIV_STS_Msk) == CCU_SRC_CLK_DIV_XIN_DIV_STS_SET){
						break;
					}
					--max_cnt_clk_stable;
				}

				if(max_cnt_clk_stable == 0U){
					(void)pr_err("XIN div set error.");;
				}
			}
    }
}

static inline void tcc_audio_ccu_set_pll0_fout_div(void __iomem *base_addr, uint32_t clk_div) {
	uint32_t max_cnt_clk_stable = MAX_CNT_CLK_STABLE;
    uint32_t value = readl(base_addr + TCC_CCU_SRC_CLK_DIV_OFFSET);

    if(((clk_div << CCU_SRC_CLK_DIV_PLL0_DIV_Pos) == (value & CCU_SRC_CLK_DIV_PLL0_DIV_Msk))
		&& ((value & CCU_SRC_CLK_DIV_PLL0_DIV_EN_Msk) == CCU_SRC_CLK_DIV_PLL0_DIV_EN_ENABLE)){
			(void)pr_info("pll0 div is already set");;
	}else{
			if(clk_div > 0x3FU){
					(void)pr_err("pll0 div(%d) is out of range(0x00 ~ 0x3f).", clk_div);
			}else{
				value &= ~(CCU_SRC_CLK_DIV_PLL0_DIV_EN_Msk | CCU_SRC_CLK_DIV_PLL0_DIV_Msk);
				value |= CCU_SRC_CLK_DIV_PLL0_DIV_EN_ENABLE;
				value |= (clk_div << CCU_SRC_CLK_DIV_PLL0_DIV_Pos);
				ccu_writel(value, base_addr + TCC_CCU_BUS_CLK_CTRL_OFFSET);

				while(max_cnt_clk_stable != 0U) {
					value = readl(base_addr + TCC_CCU_SRC_CLK_DIV_OFFSET);
					if((value & CCU_SRC_CLK_DIV_PLL0_DIV_STS_Msk) == CCU_SRC_CLK_DIV_PLL0_DIV_STS_SET){
						break;
					}
					--max_cnt_clk_stable;
				}

				if(max_cnt_clk_stable == 0U){
					(void)pr_err("pll0 div set error.");;
				}
			}
    }
}

static inline void tcc_audio_ccu_set_pll1_fout_div(void __iomem *base_addr, uint32_t clk_div) {
	uint32_t max_cnt_clk_stable = MAX_CNT_CLK_STABLE;
    uint32_t value = readl(base_addr + TCC_CCU_SRC_CLK_DIV_OFFSET);

    if(((clk_div << CCU_SRC_CLK_DIV_PLL1_DIV_Pos) == (value & CCU_SRC_CLK_DIV_PLL1_DIV_Msk)) \
		&& ((value & CCU_SRC_CLK_DIV_PLL1_DIV_EN_Msk) == CCU_SRC_CLK_DIV_PLL1_DIV_EN_ENABLE)){
			(void)pr_info("pll1 div is already set");
	}else{
			if(clk_div > 0x3FU){
					(void)pr_err("pll1 div(%d) is out of range(0x00 ~ 0x3f).", clk_div);
			}else{
				value &= ~(CCU_SRC_CLK_DIV_PLL1_DIV_EN_Msk | CCU_SRC_CLK_DIV_PLL1_DIV_Msk);
				value |= CCU_SRC_CLK_DIV_PLL1_DIV_EN_ENABLE;
				value |= (clk_div << CCU_SRC_CLK_DIV_PLL1_DIV_Pos);
				ccu_writel(value, base_addr + TCC_CCU_BUS_CLK_CTRL_OFFSET);

				while(max_cnt_clk_stable != 0U) {
					value = readl(base_addr + TCC_CCU_SRC_CLK_DIV_OFFSET);
					if((value & CCU_SRC_CLK_DIV_PLL1_DIV_STS_Msk) == CCU_SRC_CLK_DIV_PLL1_DIV_STS_SET){
						break;
					}
					--max_cnt_clk_stable;
				}

				if(max_cnt_clk_stable == 0U){
					(void)pr_err("pll1 div set error.");;
				}
			}
    }
}

static inline void tcc_audio_ccu_xin_div_enable(void __iomem *base_addr, bool enable){
	uint32_t value = readl(base_addr + TCC_CCU_SRC_CLK_DIV_OFFSET);

	value &= ~CCU_SRC_CLK_DIV_XIN_DIV_EN_Msk;
	if(enable){
		value |= CCU_SRC_CLK_DIV_XIN_DIV_EN_ENABLE;
	}else{
		value |= CCU_SRC_CLK_DIV_XIN_DIV_EN_DISABLE;
	}
    ccu_writel(value, base_addr + TCC_CCU_BUS_CLK_CTRL_OFFSET);
}

static inline void tcc_audio_ccu_pll0_div_enable(void __iomem *base_addr, bool enable){
	uint32_t value = readl(base_addr + TCC_CCU_SRC_CLK_DIV_OFFSET);

	value &= ~CCU_SRC_CLK_DIV_PLL0_DIV_EN_Msk;
	if(enable){
		value |= CCU_SRC_CLK_DIV_PLL0_DIV_EN_ENABLE;
	}else{
		value |= CCU_SRC_CLK_DIV_PLL0_DIV_EN_DISABLE;
	}
    ccu_writel(value, base_addr + TCC_CCU_BUS_CLK_CTRL_OFFSET);
}

static inline void tcc_audio_ccu_pll1_div_enable(void __iomem *base_addr, bool enable){
	uint32_t value = readl(base_addr + TCC_CCU_SRC_CLK_DIV_OFFSET);

	value &= ~CCU_SRC_CLK_DIV_PLL1_DIV_EN_Msk;
	if(enable){
		value |= CCU_SRC_CLK_DIV_PLL1_DIV_EN_ENABLE;
	}else{
		value |= CCU_SRC_CLK_DIV_PLL1_DIV_EN_DISABLE;
	}
    ccu_writel(value, base_addr + TCC_CCU_BUS_CLK_CTRL_OFFSET);
}

static inline void tcc_audio_ccu_change_bus_clk(void __iomem *base_addr, uint32_t src_clk_sel) {
	uint32_t max_cnt_clk_stable = MAX_CNT_CLK_STABLE;
	uint32_t value = readl(base_addr + TCC_CCU_BUS_CLK_CTRL_OFFSET);

	if((src_clk_sel != TCC_CPU_CLK_SEL_XIN				) &&
		(src_clk_sel != TCC_CPU_CLK_SEL_PLL0_FOUT		) &&
		(src_clk_sel != TCC_CPU_CLK_SEL_PLL1_FOUT		) &&
		(src_clk_sel != TCC_CPU_CLK_SEL_PLL0_FOUT_DIV	) &&
		(src_clk_sel != TCC_CPU_CLK_SEL_PLL1_FOUT_DIV	))	{
       	 	(void)pr_err("Invalid cpu bus clock source(%d) selected.", src_clk_sel);
	}else{
		if((value & CCU_CPU_CLK_CTRL_SEL_Msk) == (src_clk_sel << CCU_CPU_CLK_CTRL_SEL_Pos)){
	        value &= ~CCU_CPU_CLK_CTRL_SEL_Msk;
			value |= (src_clk_sel << CCU_CPU_CLK_CTRL_SEL_Pos);
	        ccu_writel(value, base_addr + TCC_CCU_BUS_CLK_CTRL_OFFSET);

			while(max_cnt_clk_stable != 0U){
				value = readl(base_addr + TCC_CCU_BUS_CLK_CTRL_OFFSET);
				if((value & CCU_CPU_CLK_CTRL_CLKCHS_STS_Msk) == CCU_CPU_CLK_CTRL_CLKCHS_STS_CHANGED){
					break;
				}
				--max_cnt_clk_stable;
			}
	    }
	}
}


static inline void tcc_audio_ccu_enable_peri_clk(void __iomem *base_addr, uint32_t peri_id, bool enable) {
	uint32_t offset = 0;
	uint32_t value = 0;
	uint32_t max_cnt_clk_stable = MAX_CNT_CLK_STABLE;
	offset = ui_add(TCC_CCU_PCLK_CFG_MAIC0_DAIF_OFFSET, ui_to_ui_mul(peri_id, 4U));

    if (peri_id > PCLK_MASRC_AUX3) {
        (void)pr_err("Peripheral clock configuration register's ID is out of range.");
    }else{
		value = readl(base_addr + offset);
		value &= ~CCU_PCLK_CFG_OUT_EN_Msk;

		if(enable){
			value |= CCU_PCLK_CFG_OUT_EN_ENABLE;
			ccu_writel(value, base_addr + offset);

			while(max_cnt_clk_stable != 0U) {
	        	value = readl(base_addr + offset);
	        	if((value & CCU_PCLK_CFG_OUT_STS_Msk) == CCU_PCLK_CFG_OUT_STS_OUTPUT){
					break;
				}
				--max_cnt_clk_stable;
	    	}
		} else {
			value |= CCU_PCLK_CFG_OUT_EN_DISABLE;
			ccu_writel(value, base_addr + offset);

			while(max_cnt_clk_stable != 0U) {
	        	value = readl(base_addr + offset);
	        	if((value & CCU_PCLK_CFG_OUT_STS_Msk) == CCU_PCLK_CFG_OUT_STS_NOT_OUTPUT){
					break;
				}
				--max_cnt_clk_stable;
	    	}
		}

		if(max_cnt_clk_stable == 0U){
			(void)pr_err("Peripheral clock output error.");;
		}
    }
}

static inline void tcc_audio_ccu_set_peri_clk(void __iomem *base_addr, uint32_t peri_id, uint32_t src_clk_sel, uint32_t clk_div) {
	uint32_t offset = ui_add(TCC_CCU_PCLK_CFG_MAIC0_DAIF_OFFSET, ui_to_ui_mul(peri_id, 4U));
	uint32_t value;

    if (peri_id > PCLK_MASRC_AUX3) {
        (void)pr_err("Peripheral clock configuration register's ID is out of range.");
    }else{
    	if ((src_clk_sel != TCC_PCLK_SEL_PLL0_FOUT      ) &&
        	(src_clk_sel != TCC_PCLK_SEL_PLL1_FOUT      ) &&
        	(src_clk_sel != TCC_PCLK_SEL_XIN            ) &&
        	(src_clk_sel != TCC_PCLK_SEL_PLL0_FOUT_DIV  ) &&
        	(src_clk_sel != TCC_PCLK_SEL_PLL1_FOUT_DIV  ) &&
        	(src_clk_sel != TCC_PCLK_SEL_DP_PLL_OUT	    ) &&
        	(src_clk_sel != TCC_PCLK_SEL_XIN_DIV        ) &&
        	(src_clk_sel != TCC_PCLK_SEL_EXT_CLK0       ) &&
        	(src_clk_sel != TCC_PCLK_SEL_EXT_CLK1       )) {
       	 	(void)pr_err("Invalid peripheral clock source(%d) selected.", src_clk_sel);
    	}else{
		    if (clk_div > 0x1000U) {
		        (void)pr_err("Peripheral clock divisor(%d) is out of range(0x000 ~ 0xfff).", clk_div);
		    }else{
			    uint32_t max_cnt_clk_stable = MAX_CNT_CLK_STABLE;

			    value = readl(base_addr + offset);
				value &= ~(CCU_PCLK_CFG_OUT_EN_Msk | CCU_PCLK_CFG_DIV_EN_Msk);
				value |= (CCU_PCLK_CFG_OUT_EN_DISABLE | CCU_PCLK_CFG_DIV_EN_DISABLE);
				ccu_writel(value, base_addr + offset);

				value &= ~(CCU_PCLK_CFG_CLK_SEL_Msk | CCU_PCLK_CFG_CLK_DIV_Msk);
				value |= ((src_clk_sel << CCU_PCLK_CFG_CLK_SEL_Pos) | (clk_div << CCU_PCLK_CFG_CLK_DIV_Pos));
				ccu_writel(value, base_addr + offset);

				value |= CCU_PCLK_CFG_DIV_EN_ENABLE;
			    ccu_writel(value, base_addr + offset);

			    udelay(1);	//wait 1us
				value |= CCU_PCLK_CFG_OUT_EN_ENABLE;
			    ccu_writel(value, base_addr + offset);

			    while(max_cnt_clk_stable != 0U) {
					value = readl(base_addr + offset);
			        if((value & CCU_PCLK_CFG_OUT_STS_Msk) == CCU_PCLK_CFG_OUT_STS_OUTPUT){
						break;
					}
					max_cnt_clk_stable--;
			    }

				if(max_cnt_clk_stable == 0U){
					(void)pr_err("Set peripheral clock output error.");
				}
		    }
	    }
    }
}

#endif /* TCC_AUDIO_CCU_H */
