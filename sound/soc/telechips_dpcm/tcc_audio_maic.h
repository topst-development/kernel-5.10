/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_AUDIO_MAIC_H
#define TCC_AUDIO_MAIC_H

#if 0//DEBUG
#define maic_writel(v, c)                        \
        ({(void)pr_info("<ASoC> MAIC_REG(0x%px) = 0x%08x\n", c, (unsigned int)v); \
        writel(v, c); })
#else
#define maic_writel(v, c)                        writel(v, c)
#endif

#define DATA_FORMAT_1CH  0
#define DATA_FORMAT_2CH  1
#define DATA_FORMAT_3CH  2
#define DATA_FORMAT_4CH  3
#define DATA_FORMAT_5CH  4
#define DATA_FORMAT_6CH  5
#define DATA_FORMAT_7CH  6
#define DATA_FORMAT_8CH  7
#define DATA_FORMAT_9CH  8
#define DATA_FORMAT_10CH  9
#define DATA_FORMAT_11CH  10
#define DATA_FORMAT_12CH  11
#define DATA_FORMAT_13CH  12
#define DATA_FORMAT_14CH  13
#define DATA_FORMAT_15CH  14
#define DATA_FORMAT_16CH  15

#define TCC_INTERFACE_ADMA	(0x0U)
#define TCC_INTERFACE_TAS	(0x1U)

#define TCC_TAS_TX			(0x1U)
#define TCC_TAS_RX			(0x2U)

static inline void tcc_audio_maic_daif_wr(void __iomem *base_addr, uint32_t offset, uint32_t data) {
	maic_writel(data, base_addr + offset);
}

static inline uint32_t tcc_audio_maic_daif_rd(const void __iomem *base_addr, uint32_t offset) {
    uint32_t    data = 0;
    data = readl(base_addr + offset);
    return  data;
}

static inline void tcc_audio_maic_select_interface(void __iomem *base_addr, uint32_t interface){

	uint32_t rdata = tcc_audio_maic_daif_rd(base_addr, TCC_MAIC_DAIF_DAMR1);

	rdata = rdata & ~(TCC_MAIC_DAMR1_TAS_SEL_MsK);
	if(interface == TCC_INTERFACE_ADMA){
		rdata |= TCC_MAIC_DAMR1_TAS_SEL_ADMA;
	}else{
		rdata |= TCC_MAIC_DAMR1_TAS_SEL_TAS;
	}

	tcc_audio_maic_daif_wr(base_addr, TCC_MAIC_DAIF_DAMR1, rdata);
}

static inline void tcc_audio_maic_set_tas_enable(void __iomem *base_addr, bool enable){

	uint32_t rdata = tcc_audio_maic_daif_rd(base_addr, TCC_MAIC_DAIF_DAMR1);

	rdata = rdata & ~(TCC_MAIC_DAMR1_TAS_EN_MsK);
	if(enable){
		rdata |= TCC_MAIC_DAMR1_TAS_EN_ENABLE;
	}else{
		rdata |= TCC_MAIC_DAMR1_TAS_EN_DISABLE;
	}

	tcc_audio_maic_daif_wr(base_addr, TCC_MAIC_DAIF_DAMR1, rdata);
}

static inline void tcc_audio_maic_set_tas_loopback(void __iomem *base_addr, bool enable){

	uint32_t rdata = tcc_audio_maic_daif_rd(base_addr, TCC_MAIC_DAIF_DAMR1);

	rdata = rdata & ~(TCC_MAIC_DAMR1_TAS_LOOP_MsK);
	if(enable){
		rdata |= TCC_MAIC_DAMR1_TAS_LOOP_ENABLE;
	}else{
		rdata |= TCC_MAIC_DAMR1_TAS_LOOP_DISABLE;
	}

	tcc_audio_maic_daif_wr(base_addr, TCC_MAIC_DAIF_DAMR1, rdata);
}

static inline bool tcc_audio_maic_get_tas_loopback(const void __iomem *base_addr){
	uint32_t rdata = tcc_audio_maic_daif_rd(base_addr, TCC_MAIC_DAIF_DAMR1);
	bool ret = TRUE;

	rdata = rdata & TCC_MAIC_DAMR1_TAS_LOOP_MsK;

	if(rdata == TCC_MAIC_DAMR1_TAS_LOOP_ENABLE){
		ret = TRUE;
	}else{
		ret = FALSE;
	}
	return ret;
}


static inline void tcc_audio_maic_set_tx_rx_valid_channel(void __iomem *base_addr, uint32_t direction, uint32_t channel){

	uint32_t rdata = tcc_audio_maic_daif_rd(base_addr, TCC_MAIC_DAIF_DAMR1);

	if((direction & TCC_TAS_TX) != 0u){
		rdata = rdata & ~(TCC_MAIC_DAMR1_TAS_TCHVLD_MsK);
		switch(channel){
			case 2U:
				rdata |= TCC_MAIC_DAMR1_TAS_TCHVLD_TAS_2CH;
				break;
			case 4U:
				rdata |= TCC_MAIC_DAMR1_TAS_TCHVLD_TAS_4CH;
				break;
			case 8U:
				rdata |= TCC_MAIC_DAMR1_TAS_TCHVLD_TAS_8CH;
				break;
			case 16U:
				rdata |= TCC_MAIC_DAMR1_TAS_TCHVLD_TAS_16CH;
				break;
			default:
				rdata |= TCC_MAIC_DAMR1_TAS_TCHVLD_NA;
				break;
		}
	}

	if((direction & TCC_TAS_RX) != 0u){
		rdata = rdata & ~(TCC_MAIC_DAMR1_TAS_RCHVLD_MsK);
		switch(channel){
			case 2U:
				rdata |= TCC_MAIC_DAMR1_TAS_RCHVLD_TAS_2CH;
				break;
			case 4U:
				rdata |= TCC_MAIC_DAMR1_TAS_RCHVLD_TAS_4CH;
				break;
			case 8U:
				rdata |= TCC_MAIC_DAMR1_TAS_RCHVLD_TAS_8CH;
				break;
			case 16U:
				rdata |= TCC_MAIC_DAMR1_TAS_RCHVLD_TAS_16CH;
				break;
			default:
				rdata |= TCC_MAIC_DAMR1_TAS_RCHVLD_NA;
				break;
		}
	}

	tcc_audio_maic_daif_wr(base_addr, TCC_MAIC_DAIF_DAMR1, rdata);
}

static inline void tcc_audio_maic_set_tx_rx_clear(void __iomem *base_addr, uint32_t direction, bool enable){

	uint32_t rdata = tcc_audio_maic_daif_rd(base_addr, TCC_MAIC_DAIF_DAMR1);

	if((direction & TCC_TAS_TX) != 0u){
		rdata = rdata & ~(TCC_MAIC_DAMR1_TAS_TX_CLR_MsK);
		if(enable){
			rdata |= TCC_MAIC_DAMR1_TAS_TX_CLR_ENABLE;
		}else{
			rdata |= TCC_MAIC_DAMR1_TAS_TX_CLR_DISABLE;
		}
	}

	if((direction & TCC_TAS_RX) != 0u){
		rdata = rdata & ~(TCC_MAIC_DAMR1_TAS_RX_CLR_MsK);
		if(enable){
			rdata |= TCC_MAIC_DAMR1_TAS_RX_CLR_ENABLE;
		}else{
			rdata |= TCC_MAIC_DAMR1_TAS_RX_CLR_DISABLE;
		}
	}

	tcc_audio_maic_daif_wr(base_addr, TCC_MAIC_DAIF_DAMR1, rdata);
}

static inline void tcc_audio_maic_set_32bit_mode(void __iomem *base_addr, bool enable){

	uint32_t rdata = tcc_audio_maic_daif_rd(base_addr, TCC_MAIC_DAIF_DAMR1);

	rdata = rdata & ~(TCC_MAIC_DAMR1_32MOD_MsK);
	if(enable){
		rdata |= TCC_MAIC_DAMR1_32MOD_ENABLE;
	}else{
		rdata |= TCC_MAIC_DAMR1_32MOD_DISABLE;
	}

	tcc_audio_maic_daif_wr(base_addr, TCC_MAIC_DAIF_DAMR1, rdata);
}

static inline void tcc_audio_maic_rdma0_update(void __iomem *base_addr, uint64_t addr,
	uint32_t data_signed, uint32_t width, uint32_t format){

    uint32_t wdata;
    uint32_t addr_lsb = ul_to_ui(addr & 0xFFFFFFFFu);
    uint32_t addr_msb = ul_to_ui(addr >> 32);

    wdata = ui_add(ui_lshift(data_signed, 16), ui_add(ui_lshift(width, 8), format));

    tcc_audio_maic_daif_wr(base_addr, TCC_MAIC_RDMA0_ADDR_LSB, addr_lsb);
    tcc_audio_maic_daif_wr(base_addr, TCC_MAIC_RDMA0_ADDR_MSB, addr_msb);

    tcc_audio_maic_daif_wr(base_addr, TCC_MAIC_RDMA0_DATA_CFG, wdata);
}

static inline void tcc_audio_maic_wdma0_update(void __iomem *base_addr,
        uint64_t addr, uint32_t bitconvert_mode,
        uint32_t data_signed, uint32_t width, uint32_t format){

    uint32_t wdata;
    uint32_t addr_lsb = ul_to_ui(addr & 0xFFFFFFFFu);
    uint32_t addr_msb = ul_to_ui(addr >> 32);

    wdata = ui_add(ui_add(ui_lshift(bitconvert_mode, 20), ui_lshift(data_signed, 16))
		, ui_add(ui_lshift(width, 8), format));

    tcc_audio_maic_daif_wr(base_addr, TCC_MAIC_WDMA0_ADDR_LSB, addr_lsb);
    tcc_audio_maic_daif_wr(base_addr, TCC_MAIC_WDMA0_ADDR_MSB, addr_msb);

    tcc_audio_maic_daif_wr(base_addr, TCC_MAIC_WDMA0_DATA_CFG, wdata);
}

static inline void tcc_audio_maic_axi_master_setting(void __iomem *base_addr){
    tcc_audio_maic_daif_wr(base_addr, TCC_MAIC_AXI_TASDMA_CFG, 0x10101010);
}

static inline void tcc_audio_maic_dma_start(void __iomem *base_addr){
    tcc_audio_maic_daif_wr(base_addr, TCC_MAIC_DMA0_CTRL, 1);
}

static inline void tcc_audio_maic_dma_stop(void __iomem *base_addr){
    tcc_audio_maic_daif_wr(base_addr, TCC_MAIC_DMA0_CTRL, 0);
}

static inline void tcc_audio_maic_register_update(const void __iomem *base_addr){
    uint32_t rdata = 0;
    int count = 0;
    do{
        if ((rdata & 0x10000u)==0x10000u){
            break;
        }else{
            rdata = tcc_audio_maic_daif_rd(base_addr, TCC_MAIC_DMA0_CTRL);
            count ++;
        }
    }while(count != 0x1000);

	if (count == 0x1000){
		//std::cout << "ERR : Check if IP is running or is stalled" << std::endl;
		(void)pr_err("%s: ERR : Check if IP is running or is stalled", __func__);
	}
}

static inline void tcc_audio_maic_set_tas_bit_convert(void __iomem *base_addr, uint32_t wdata){
	tcc_audio_maic_daif_wr(base_addr, TCC_MAIC_TAS_BIT_CONVERT, wdata);
}

static inline void tcc_audio_maic_enable_fifo_pointer_switching
	(void __iomem *base_addr, bool enable)
{
	uint32_t rdata = tcc_audio_maic_daif_rd(base_addr, TCC_MAIC_TAS_FIFO_CFG);

	rdata = rdata & ~(FIFO_CFG_FIFO_EN_Msk);
	if(enable){
		rdata |= FIFO_CFG_FIFO_EN_ENABLE;
	}else{
		rdata |= FIFO_CFG_FIFO_EN_DISABLE;
	}

	tcc_audio_maic_daif_wr(base_addr, TCC_MAIC_TAS_FIFO_CFG, rdata);
}

static inline void tcc_audio_maic_set_tx_start_pointer_value
	(void __iomem *base_addr, uint32_t start_pointer)
{
	uint32_t rdata = tcc_audio_maic_daif_rd(base_addr, TCC_MAIC_TAS_FIFO_CFG);

	rdata = rdata & ~(FIFO_CFG_INIT_TX_FIFO_Msk);
	rdata |= ui_lshift(start_pointer, FIFO_CFG_INIT_TX_FIFO_Pos);

	tcc_audio_maic_daif_wr(base_addr, TCC_MAIC_TAS_FIFO_CFG, rdata);
}

static inline void tcc_audio_maic_set_rx_start_pointer_value
	(void __iomem *base_addr, uint32_t start_pointer)
{
	uint32_t rdata = tcc_audio_maic_daif_rd(base_addr, TCC_MAIC_TAS_FIFO_CFG);

	rdata = rdata & ~(FIFO_CFG_INIT_RX_FIFO_Msk);
	rdata |= ui_lshift(start_pointer, FIFO_CFG_INIT_RX_FIFO_Pos);

	tcc_audio_maic_daif_wr(base_addr, TCC_MAIC_TAS_FIFO_CFG, rdata);
}


#endif /* TCC_AUDIO_MAIC_H */
