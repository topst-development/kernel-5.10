/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef VIN_WRAP_CONFIG_H
#define	VIN_WRAP_CONFIG_H

#define get_vin_wrap_type(x)			((x) >> 8U)
#define get_vin_wrap_index(x)			((x) & 0xFFU)

/*
 * register offset
 */
#define VIN_CTL_OFFSET		(0x000U)
#define VIN_SWREST_OFFSET	(0x004U)
#define VIN_PWRDN_OFFSET	(0x008U)
#define VIN_APWRDN_OFFSET	(0x00CU)
#define VIN_IRQ_STS_OFFSET	(0x208U)
#define VIN_IRQ_SEL0_OFFSET	(0x400U)
#define VIN_IRQ_MSK0_OFFSET	(0x410U)
#define VIN_IRQ_CLR0_OFFSET	(0x420U)
#define VIN_IRQ_SEL1_OFFSET	(0x440U)
#define VIN_IRQ_MSK1_OFFSET	(0x450U)
#define VIN_IRQ_CLR1_OFFSET	(0x460U)



/*
 * Soft Reset Registers
 * @Description: 0 - Release, 1 - Reset
 */
#define VIN0_SW_SHIFT		(0U)
#define VIN1_SW_SHIFT		(1U)
#define WDMA0_SW_SHIFT		(4U)
#define WDMA1_SW_SHIFT		(5U)

#define VIN0_SW_MASK		((u32)0x1U << VIN0_SW_SHIFT)
#define VIN1_SW_MASK		((u32)0x1U << VIN1_SW_SHIFT)
#define WDMA0_SW_MASK		((u32)0x1U << WDMA0_SW_SHIFT)
#define WDMA1_SW_MASK		((u32)0x1U << WDMA1_SW_SHIFT)

/*
 * Power Down Registers
 * @Description: 0 - On, 1 - Off
 */
#define VIN0_PWRDN_SHIFT	(0U)
#define VIN1_PWRDN_SHIFT	(1U)
#define WDMA0_PWRDN_SHIFT	(4U)
#define WDMA1_PWRDN_SHIFT	(4U)


#define VIN0_PWRDN_MASK		((u32)0x1U << VIN0_PWRDN_SHIFT)
#define VIN1_PWRDN_MASK		((u32)0x1U << VIN1_PWRDN_SHIFT)
#define WDMA0_PWRDN_MASK	((u32)0x1U << WDMA0_PWRDN_SHIFT)
#define WDMA1_PWRDN_MASK	((u32)0x1U << WDMA1_PWRDN_SHIFT)

/*
 * Auto Power Down Registers
 * @Description: 0 - Normal, 1 - AUto
 */
#define VIN_PWRDN_SHIFT		(0U)
#define WDMA_PWRDN_SHIFT	(4U)

#define VIN_PWRDN_MASK		((u32)0x1U << VIN_PWRDN_SHIFT)
#define WDMA_PWRDN_MASK		((u32)0x1U << WDMA_PWRDN_SHIFT)


/*
 * Interrupt Status Registers
 * @Description: 0 - Normal, 1 - Interrupt
 */
#define VIN_IRQ_STS_VIN0_SHIFT	(0U)
#define VIN_IRQ_STS_VIN1_SHIFT	(1U)
#define VIN_IRQ_STS_WDMA0_SHIFT	(16U)
#define VIN_IRQ_STS_WDMA1_SHIFT	(17U)

#define VIN_IRQ_STS_VIN0_MASK	((u32)0x1U << VIN_IRQ_STS_VIN0_SHIFT)
#define VIN_IRQ_STS_VIN1_MASK	((u32)0x1U << VIN_IRQ_STS_VIN1_SHIFT)
#define VIN_IRQ_STS_WMDA0_MASK	((u32)0x1U << VIN_IRQ_STS_WDNA0_SHIFT)
#define VIN_IRQ_STS_WDMA1_MASK	((u32)0x1U << VIN_IRQ_STS_WDNA1_SHIFT)


/*
 * Interrupt Select Registers
 * @Description: 0 - Async Interrupt, 1 - Sync Interrupt
 */
#define VIN_IRQ_SEL_VIN0_SHIFT	(0U)
#define VIN_IRQ_SEL_VIN1_SHIFT	(1U)
#define VIN_IRQ_SEL_WDMA0_SHIFT	(16U)
#define VIN_IRQ_SEL_WDMA1_SHIFT	(17U)

#define VIN_IRQ_SEL_VIN0_MASK	((u32)0x1U << VIN_IRQ_SEL_VIN0_SHIFT)
#define VIN_IRQ_SEL_VIN1_MASK	((u32)0x1U << VIN_IRQ_SEL_VIN1_SHIFT)
#define VIN_IRQ_SEL_WMDA0_MASK	((u32)0x1U << VIN_IRQ_SEL_WDNA0_SHIFT)
#define VIN_IRQ_SEL_WDMA1_MASK	((u32)0x1U << VIN_IRQ_SEL_WDNA1_SHIFT)


/*
 * Interrupt Mask 0 Registers
 * @Description: 0 - Enable, 1 - Mask
 */
#define VIN_IRQ_MSK_VIN0_SHIFT	(0U)
#define VIN_IRQ_MSK_VIN1_SHIFT	(1U)
#define VIN_IRQ_MSK_WDMA0_SHIFT	(16U)
#define VIN_IRQ_MSK_WDMA1_SHIFT	(17U)

#define VIN_IRQ_MSK_VIN0_MASK	((u32)0x1U << VIN_IRQ_MSK_VIN0_SHIFT)
#define VIN_IRQ_MSK_VIN1_MASK	((u32)0x1U << VIN_IRQ_MSK_VIN1_SHIFT)
#define VIN_IRQ_MSK_WMDA0_MASK	((u32)0x1U << VIN_IRQ_MSK_WDNA0_SHIFT)
#define VIN_IRQ_MSK_WDMA1_MASK	((u32)0x1U << VIN_IRQ_MSK_WDNA1_SHIFT)


/* define for only config & interrupt register */
#define VIN_WRAP_CONFIG_RESET 0x1U
#define VIN_WRAP_CONFIG_CLEAR 0x0U


typedef enum {
	VIN_WRAP_CONFIG_VIN,
	VIN_WRAP_CONFIG_WDMA
} VIN_WRAP_SWRESET_Component;

// /* Interface APIs */
void VIN_WRAP_CONFIG_SWReset(unsigned int component, unsigned int resetmode);
void __iomem *VIN_WRAP_IREQConfig_GetAddress(void);
void vin_wrap_config_init(void);
#endif
