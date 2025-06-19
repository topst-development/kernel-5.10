// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/usb/phy.h>
#include <linux/usb/otg.h>
#include "../dwc3/core.h"
#include "../dwc3/io.h"
#include <linux/kthread.h>
#include <soc/telechips/chipinfo.h>

#include "phy-tcc-dwc3.h"

static int32_t tcc_dwc3_phy_create(struct device *dev,
		struct tcc_dwc3_phy *tcc_dwc3)
{
	int32_t ret = -ENODEV;

	if ((dev != NULL) && (tcc_dwc3 != NULL)) {
		tcc_dwc3->uphy.otg = devm_kzalloc(dev,
				sizeof(*tcc_dwc3->uphy.otg), GFP_KERNEL);
		if (tcc_dwc3->uphy.otg == NULL) {
			ret = -ENOMEM;
		} else {
			tcc_dwc3->dev = dev;

			tcc_dwc3->uphy.dev = tcc_dwc3->dev;
			tcc_dwc3->uphy.label = "tcc_dwc3_phy";
			tcc_dwc3->uphy.type = USB_PHY_TYPE_USB3;

			tcc_dwc3->uphy.init = tcc_dwc3_phy_init;
			tcc_dwc3->uphy.set_vbus = tcc_dwc3_phy_set_vbus;
			tcc_dwc3->uphy.set_suspend = tcc_dwc3_phy_set_suspend;

#if 0
#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
			tcc_dwc3->uphy.get_dc_level = tcc_dwc3_phy_get_dc_level;
			tcc_dwc3->uphy.set_dc_level = tcc_dwc3_phy_set_dc_level;
#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */

#if defined(CONFIG_ENABLE_BC_30_HOST)
			tcc_dwc3->uphy.set_chg_det = tcc_dwc3_phy_set_chg_det;
			tcc_dwc3->uphy.stop_chg_det = tcc_dwc3_phy_stop_chg_det;
#endif /* CONFIG_ENABLE_BC_30_HOST */
#endif

			tcc_dwc3->uphy.otg->usb_phy = &tcc_dwc3->uphy;

			ret = 0;
		}
	}

	return ret;
}

static int32_t tcc_dwc3_phy_init(struct usb_phy *uphy)
{
	const struct tcc_dwc3_phy *tcc_dwc3 =
		container_of(uphy, struct tcc_dwc3_phy, uphy);
	uint32_t is_sram_init_done = 0;
	int32_t retry = 0;
	int32_t ret = 0;
	uint32_t val;
	uint32_t tmp_data = 0;

	if (is_suspended != 0)
	{
		// PHY Configuration Setting per Protocol
		val = readl(&tcc_dwc3->phy_regs->PCFG4);
		writel(val | PHY_EXT_CTRL_SEL, &tcc_dwc3->phy_regs->PCFG4);

		// Controller Software Reset(Reset Controller)
		val = readl(&tcc_dwc3->phy_regs->LCFG);
		writel(val & ~VCC_RESET_N, &tcc_dwc3->phy_regs->LCFG);

#if defined(CONFIG_ARCH_TCC807X)
		// Enable Double Bandwidth Mode
		// Set USB 3.0 High-speed PHY Reference Clock Frequency
		val = readl(&tcc_dwc3->phy_regs->FPCFG0);
		writel(val & ~FSEL_RESET, &tcc_dwc3->phy_regs->FPCFG0);
		val = readl(&tcc_dwc3->phy_regs->FPCFG0);
		writel(val | (PLLBTUNE | FSEL), &tcc_dwc3->phy_regs->FPCFG0);

		// Set USB 3.0 PHY Reference Clock Frequency
		val = readl(&tcc_dwc3->phy_regs->PCFG5);
		writel(val & ~EXT_MPLLA_BANDWIDTH_RST,
				&tcc_dwc3->phy_regs->PCFG5);
		val = readl(&tcc_dwc3->phy_regs->PCFG5);
		writel(val | EXT_MPLLA_BANDWIDTH, &tcc_dwc3->phy_regs->PCFG5);

		val = readl(&tcc_dwc3->phy_regs->PCFG6);
		writel(val | (EXT_MPLLA_SSC_FREQ_CNT_PEAK
					| EXT_MPLLA_SSC_FREQ_CNT_INIT),
				&tcc_dwc3->phy_regs->PCFG6);
#endif /* CONFIG_ARCH_TCC807X */

		// PHY Reset(Release Reset)
		val = readl(&tcc_dwc3->phy_regs->PCFG0);
		writel(val & ~PHY_RESET, &tcc_dwc3->phy_regs->PCFG0);

		// PHY Power-on Reset(Reset USB2.0 Host PHY)
		val = readl(&tcc_dwc3->phy_regs->FPCFG0);
		writel(val | PHY_POR, &tcc_dwc3->phy_regs->FPCFG0);

		// Power-on SuperSpeed Circuit
		val = readl(&tcc_dwc3->phy_regs->PCFG0);
		writel(val & ~PD_SS, &tcc_dwc3->phy_regs->PCFG0);
		usleep_range(1000, 2000);

		// PHY Reset(PHY Set to Reset)
		val = readl(&tcc_dwc3->phy_regs->PCFG0);
		writel(val | PHY_RESET, &tcc_dwc3->phy_regs->PCFG0);

		// Waiting for SRAM Initialization Done
		while (is_sram_init_done == 0U) {
			is_sram_init_done =
				tcc_dwc3->phy_regs->PCFG0 & SRAM_INIT_DONE;
		}

		// SRAM External Load Done
		val = readl(&tcc_dwc3->phy_regs->PCFG0);
		writel(val | SRAM_EXT_LD_DONE, &tcc_dwc3->phy_regs->PCFG0);

		// Set TXVRT to 0xC
		writel(0xE31C243C, &tcc_dwc3->phy_regs->FPCFG1);

		// Set External TX VBoost Level to 0x7
		writel(0x31C71457, &tcc_dwc3->phy_regs->PCFG13);

		// Set External TX IBoost Level to 0xA
		writel(0xA4C4302A, &tcc_dwc3->phy_regs->PCFG15);

		// SIDDQ is released
		val = readl(&tcc_dwc3->phy_regs->FPCFG0);
		writel(val & ~SIDDQ, &tcc_dwc3->phy_regs->FPCFG0);

		// PHY Power-on Reset(Normal Function)
		val = readl(&tcc_dwc3->phy_regs->FPCFG0);
		writel(val & ~PHY_POR, &tcc_dwc3->phy_regs->FPCFG0);

		// Controller Software Reset(Controller is in Normal Operation)
		val = readl(&tcc_dwc3->phy_regs->LCFG);
		writel(val | VCC_RESET_N, &tcc_dwc3->phy_regs->LCFG);

		// Waiting for USB 3.0 PHY to operate in SuperSpeed Mode
		while (retry < RETRY_CNT) {
			if ((tcc_dwc3->phy_regs->PCFG0 & PHY_STABLE) != 0U) {
				break;
			}

			retry++;
			udelay(5);
		}

		dev_info(tcc_dwc3->dev, "[INFO][USB] Checking PHY validity... %s\n",
				(retry >= RETRY_CNT) ? "FAIL!" : "SUCCESS");
		retry = 0;

		// Initialize LCFG
		val = readl(&tcc_dwc3->phy_regs->LCFG);
		writel(val | (HUB_PORT_PERM_ATTACH | (uint32_t)BIT(19) |
					(uint32_t)BIT(18) | FLADJ | PPC),
				&tcc_dwc3->phy_regs->LCFG);

		/* Set 2.0phy REXT */
		if (IS_ENABLED(CONFIG_ARCH_TCC803X) ||
				(get_chip_name() == 0x8059U)) {
			do {
				// Read calculated value
				writel((uint32_t)BIT(26) | (uint32_t)BIT(25),
						tcc_dwc3->ref_base);
				val = readl(tcc_dwc3->ref_base);
				dev_info(tcc_dwc3->dev, "[INFO][USB] 2.0H status bus = 0x%08x\n",
						val);
				tmp_data = 0x0000F000U & val;

				tmp_data = tmp_data << 4; // set TESTDATAIN

				//Read Status Bus
				writel(TAD, &tcc_dwc3->phy_regs->FPCFG3);

				//Read Override Bus
				val = readl(&tcc_dwc3->phy_regs->FPCFG3);
				writel(val | TDOSEL,
						&tcc_dwc3->phy_regs->FPCFG3);

				//Write Override Bus
				val = readl(&tcc_dwc3->phy_regs->FPCFG3);
				writel(val | (TDI | tmp_data),
						&tcc_dwc3->phy_regs->FPCFG3);
				udelay(1);

				val = readl(&tcc_dwc3->phy_regs->FPCFG3);
				writel(val | TCK, &tcc_dwc3->phy_regs->FPCFG3);
				udelay(1);

				val = readl(&tcc_dwc3->phy_regs->FPCFG3);
				writel(val & ~TCK, &tcc_dwc3->phy_regs->FPCFG3);
				udelay(1);

				//Read Status Bus
				writel(TAD, &tcc_dwc3->phy_regs->FPCFG3);

				//Read Override Bus
				val = readl(&tcc_dwc3->phy_regs->FPCFG3);
				writel(val | TDOSEL,
						&tcc_dwc3->phy_regs->FPCFG3);

				dev_info(tcc_dwc3->dev, "[INFO][USB] 2.0 REXT = 0x%08x\n",
						tcc_dwc3->phy_regs->FPCFG3 &
						TDO);

				retry++;
			} while (((tcc_dwc3->phy_regs->FPCFG3 & TDO) == 0U) &&
					(retry < 5));
		}

#if 0
#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
		(void)tcc_dwc3_phy_set_dc_level(uphy,
				CONFIG_USB_HS_DC_VOLTAGE_LEVEL);
#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */

#if defined(CONFIG_ENABLE_BC_30_HOST)
		// Clear PHY IRQ
		val = readl(&tcc_dwc3->phy_regs->FPCFG4);
		writel(val | IRQ_CLR, &tcc_dwc3->phy_regs->FPCFG4);

		// Disable IRQ for PHY Valid
		val = readl(&tcc_dwc3->phy_regs->FPCFG4);
		writel(val & ~IRQ_PHYVALIDEN, &tcc_dwc3->phy_regs->FPCFG4);

		// Clear PHY IRQ
		val = readl(&tcc_dwc3->phy_regs->FPCFG4);
		writel(val & ~IRQ_CLR, &tcc_dwc3->phy_regs->FPCFG4);
		udelay(1);

		/*
		 * Data Source Voltage(VDAT_SRC) is sourced onto DM and sunk
		 * from DP. And Data Detect Voltage(CHG_DET) is enabled.
		 */
		val = readl(&tcc_dwc3->phy_regs->FPCFG2);
		writel(val | (CHRGSEL | VDATDETENB),
				&tcc_dwc3->phy_regs->FPCFG2);
		udelay(1);

		// Event for PHY Valid is pending
		val = readl(&tcc_dwc3->phy_regs->FPCFG4);
		writel(val | IRQ_PHYVALID, &tcc_dwc3->phy_regs->FPCFG4);

		// Enable CHGDET Interrupt
		val = readl(&tcc_dwc3->phy_regs->PINT);
		writel(val & ~PINT_MSK_CHGDET, &tcc_dwc3->phy_regs->PINT);

		// Disable interrupts
		val = readl(&tcc_dwc3->phy_regs->PINT);
		writel(val | (PINT_EN | PINT_MSK_RESERVED | PINT_MSK_PHY |
					PINT_MSK_BC_CHIRP_ON),
				&tcc_dwc3->phy_regs->PINT);

		enable_irq((unsigned int)tcc_dwc3->charge_detect_irq);
#endif /* CONFIG_ENABLE_BC_30_HOST */
#endif

		// Controller Software Reset(Reset Controller)
		val = readl(&tcc_dwc3->phy_regs->LCFG);
		writel(val & ~VCC_RESET_N, &tcc_dwc3->phy_regs->LCFG);

		/*
		 * disable usb20mode -> removed in DWC_usb3 2.60a, but use as
		 * interrupt
		 */
		val = readl(&tcc_dwc3->phy_regs->LCFG);
		writel(val & ~(u32)BIT(28), &tcc_dwc3->phy_regs->LCFG);

		// Power-on SuperSpeed Circuit
		val = readl(&tcc_dwc3->phy_regs->PCFG0);
		writel(val & ~PD_SS, &tcc_dwc3->phy_regs->PCFG0);

		// PHY Reset(Release Reset)
		val = readl(&tcc_dwc3->phy_regs->PCFG0);
		writel(val & ~PHY_RESET, &tcc_dwc3->phy_regs->PCFG0);
		udelay(1000);

		// PHY Reset(PHY Set to Reset)
		val = readl(&tcc_dwc3->phy_regs->PCFG0);
		writel(val | PHY_RESET, &tcc_dwc3->phy_regs->PCFG0);

		// Controller Software Reset(Controller is in Normal Operation)
		val = readl(&tcc_dwc3->phy_regs->LCFG);
		writel(val | VCC_RESET_N, &tcc_dwc3->phy_regs->LCFG);
		udelay(2000);

		is_suspended = 0;
	}

	return ret;
}

static int32_t tcc_dwc3_phy_set_vbus(struct usb_phy *uphy, int32_t on_off)
{
	struct tcc_dwc3_phy *tcc_dwc3 =
		container_of(uphy, struct tcc_dwc3_phy, uphy);
	const struct device *dev = tcc_dwc3->dev;
	int32_t ret = 0;

	if ((on_off == ON) && (tcc_dwc3->vbus_enabled == FALSE)) {
		ret = regulator_enable(tcc_dwc3->vbus_supply);
		if (ret == 0) {
			ret = regulator_set_voltage(tcc_dwc3->vbus_supply,
					ON_VOLTAGE, ON_VOLTAGE);
			if (ret == 0) {
				tcc_dwc3->vbus_enabled = TRUE;
				usleep_range(3000, 5000);
			} else {
				dev_err(dev, "[ERROR][USB] VBus on is FAILED!\n");
				(void)regulator_disable(tcc_dwc3->vbus_supply);
			}
		} else {
			dev_err(dev, "[ERROR][USB] Regulator enable is FAILED!\n");
		}
	} else if ((on_off == OFF) && (tcc_dwc3->vbus_enabled == TRUE)) {
		ret = regulator_set_voltage(tcc_dwc3->vbus_supply,
				OFF_VOLTAGE, OFF_VOLTAGE);
		if (ret == 0) {
			ret = regulator_disable(tcc_dwc3->vbus_supply);
			if (ret == 0) {
				tcc_dwc3->vbus_enabled = FALSE;
				usleep_range(3000, 5000);
			} else {
				dev_err(dev, "[ERROR][USB] Regulator disable is FAILED!\n");
			}
		} else {
			dev_err(dev, "[ERROR][USB] VBus off is FAILED!\n");
			(void)regulator_disable(tcc_dwc3->vbus_supply);
		}
	} else if ((on_off == ON) && (tcc_dwc3->vbus_enabled == TRUE)) {
		dev_info(dev, "[INFO][USB] Vbus is already ENABLED!\n");
	} else if ((on_off == OFF) && (tcc_dwc3->vbus_enabled == FALSE)) {
		dev_info(dev, "[INFO][USB] Vbus is already DISABLED!\n");
	} else {
		/* Nothing to do */
	}

	return ret;
}

static int32_t tcc_dwc3_phy_set_suspend(struct usb_phy *uphy, int32_t suspend)
{
	const struct tcc_dwc3_phy *tcc_dwc3;
	int32_t ret = -ENODEV;

	if (uphy != NULL) {
		tcc_dwc3 = container_of(uphy, struct tcc_dwc3_phy, uphy);

		if (suspend == 0) {
			if (is_suspended == 1) {
				dev_info(tcc_dwc3->dev, "[INFO][USB] SuperSpeed circuit power on\n");

				BIT_CLR(tcc_dwc3->phy_regs->PCFG0, PD_SS);
				is_suspended = 0;
			}
		} else {
			if (is_suspended == 0) {
				dev_info(tcc_dwc3->dev, "[INFO][USB] SuperSpeed circuit power down in %s()\n",
						__func__);

				BIT_SET(tcc_dwc3->phy_regs->PCFG0, PD_SS);
				mdelay(10);

				is_suspended = 1;
			}
		}

		ret = 0;
	}

	return ret;
}

#if 0
#if defined(CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT)
static uint32_t tcc_dwc3_phy_get_dc_level(struct usb_phy *uphy)
{
	const struct tcc_dwc3_phy *tcc_dwc3 =
		container_of(uphy, struct tcc_dwc3_phy, uphy);
	uint32_t fpcfg1_val;

	fpcfg1_val = readl(&tcc_dwc3->phy_regs->FPCFG1);

	return BIT_MSK(fpcfg1_val, PCFG_TXVRT);
}

static int32_t tcc_dwc3_phy_set_dc_level(struct usb_phy *uphy, uint32_t level)
{
	const struct tcc_dwc3_phy *tcc_dwc3 =
		container_of(uphy, struct tcc_dwc3_phy, uphy);
	uint32_t fpcfg1_val;

	fpcfg1_val = readl(&tcc_dwc3->phy_regs->FPCFG1);
	BIT_CLR_SET(fpcfg1_val, PCFG_TXVRT, level);
	writel(fpcfg1_val, &tcc_dwc3->phy_regs->FPCFG1);

	dev_info(tcc_dwc3->dev, "[INFO][USB] current DC voltage level: %d\n",
			BIT_MSK(fpcfg1_val, PCFG_TXVRT));

	return 0;
}
#endif /* CONFIG_DYNAMIC_DC_LEVEL_ADJUSTMENT */

#if defined(CONFIG_ENABLE_BC_30_HOST)
static irqreturn_t tcc_dwc3_phy_chg_det_irq(int32_t irq, void *data)
{
	struct tcc_dwc3_phy *tcc_dwc3 = (struct tcc_dwc3_phy *)data;
	uint32_t val;

	dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] %s : CHGDET\n", __func__);

	// clear irq
	val = readl(&tcc_dwc3->phy_regs->PINT);
	writel(val | PINT_STS, &tcc_dwc3->phy_regs->PINT);
	udelay(1);

	// clear irq
	val = readl(&tcc_dwc3->phy_regs->PINT);
	writel(val & ~PINT_STS, &tcc_dwc3->phy_regs->PINT);

	(void)schedule_work(&tcc_dwc3->dwc3_work);

	return IRQ_HANDLED;
}

static void tcc_dwc3_phy_chg_det_monitor(struct work_struct *data)
{
	struct tcc_dwc3_phy *tcc_dwc3 =
		container_of(data, struct tcc_dwc3_phy, dwc3_work);
	int32_t count = 3;
	int32_t timeout_count = 500;
	uint32_t val;

	tcc_dwc3->chg_ready = TRUE;

	while (count > 0) {
		val = readl(&tcc_dwc3->phy_regs->FPCFG2);
		if ((val & CHGDET) != 0U) {
			break;
		}

		usleep_range(1000, 1100);
		count--;
	}

	if (count == 0) {
		dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] Charge Detection FAIL!\n");
	} else {
		val = readl(&tcc_dwc3->phy_regs->FPCFG2);
		writel((val | VDATSRCENB), &tcc_dwc3->phy_regs->FPCFG2);

		while (timeout_count > 0) {
			val = readl(&tcc_dwc3->phy_regs->FPCFG2);
			if ((val & CHGDET) != 0U) { // Check VDP_SRC signal
				usleep_range(1000, 1100);
				timeout_count--;
			} else {
				break;
			}
		}

		if (timeout_count == 0) {
			dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] Timeout - VDM_SRC on\n");
		}

		// Data Source Voltage & Data Detect Voltage is disabled
		val = readl(&tcc_dwc3->phy_regs->FPCFG2);
		writel(val & ~(VDATSRCENB | VDATDETENB), &tcc_dwc3->phy_regs->FPCFG2);

		if (tcc_dwc3->chg_ready == TRUE) {
			dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] Enable chg det monitor!\n");

			if (tcc_dwc3->dwc3_chgdet_thread != NULL) {
				(void)kthread_stop(tcc_dwc3->dwc3_chgdet_thread);
				tcc_dwc3->dwc3_chgdet_thread = NULL;
			}

			dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] start chg det thread!\n");

			tcc_dwc3->dwc3_chgdet_thread =
				kthread_run(tcc_dwc3_phy_chg_det_thread,
						(void *)tcc_dwc3, "dwc3-chgdet");
			if (IS_ERR(tcc_dwc3->dwc3_chgdet_thread)) {
				dev_err(tcc_dwc3->dev, "[ERROR][USB] failed to run tcc_dwc3_phy_chg_det_thread\n");
			}
		} else {
			dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] No need to start chg det monitor!\n");
		}
	}
}

static int tcc_dwc3_phy_chg_det_thread(void *work)
{
	struct tcc_dwc3_phy *tcc_dwc3 = (struct tcc_dwc3_phy *) work;
	int32_t timeout = 500;
	uint32_t val;

	dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] Start to check CHGDET\n");

	while (!kthread_should_stop() && (timeout > 0)) {
		usleep_range(1000, 1100);
		timeout--;
	}

	if (timeout <= 0) {
		val = readl(&tcc_dwc3->phy_regs->FPCFG2);
		writel(val | VDATDETENB, &tcc_dwc3->phy_regs->FPCFG2);
		dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] Data Detect Voltage is enabled\n");
	}

	tcc_dwc3->dwc3_chgdet_thread = NULL;

	dev_info(tcc_dwc3->dev, "[INFO][USB] Monitoring is finished(%d)\n", timeout);

	return 0;
}

static void tcc_dwc3_phy_set_chg_det(struct usb_phy *uphy)
{
	const struct tcc_dwc3_phy *tcc_dwc3 =
		container_of(uphy, struct tcc_dwc3_phy, uphy);
	uint32_t val;

	dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] %s() call SUCCESS\n", __func__);

	// Data Detect Voltage(CHG_DET) is enabled
	val = readl(&tcc_dwc3->phy_regs->FPCFG2);
	writel(val | VDATDETENB, &tcc_dwc3->phy_regs->FPCFG2);
}

static void tcc_dwc3_phy_stop_chg_det(struct usb_phy *uphy)
{
	struct tcc_dwc3_phy *tcc_dwc3 =
		container_of(uphy, struct tcc_dwc3_phy, uphy);
	uint32_t fpcfg2, fpcfg4;

	dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] %s() call SUCCESS\n", __func__);

	tcc_dwc3->chg_ready = FALSE;

	if (tcc_dwc3->dwc3_chgdet_thread != NULL) {
		dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] Kill Charging Detection Thread\n");

		(void)kthread_stop(tcc_dwc3->dwc3_chgdet_thread);
		tcc_dwc3->dwc3_chgdet_thread = NULL;
	}

	fpcfg2 = readl(&tcc_dwc3->phy_regs->FPCFG2);
	fpcfg4 = readl(&tcc_dwc3->phy_regs->FPCFG4);
	dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] Before\nU30_FPCFG2: 0x%x / U30_FPCFG4: 0x%x\n",
			fpcfg2, fpcfg4);

	// Data Source Voltage & Data Detect Voltage is disabled
	fpcfg2 = readl(&tcc_dwc3->phy_regs->FPCFG2);
	writel(fpcfg2 & ~(VDATSRCENB | VDATDETENB), &tcc_dwc3->phy_regs->FPCFG2);

	fpcfg2 = readl(&tcc_dwc3->phy_regs->FPCFG2);
	fpcfg4 = readl(&tcc_dwc3->phy_regs->FPCFG4);
	dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] Charging Detection is disabled\n");
	dev_dbg(tcc_dwc3->dev, "[DEBUG][USB] After\nU30_FPCFG2: 0x%x / U30_FPCFG4: 0x%x\n",
			fpcfg2, fpcfg4);
}

static int32_t tcc_dwc3_phy_init_battery_charging(struct platform_device *pdev,
		struct tcc_dwc3_phy *tcc_dwc3)
{
	int32_t irq = platform_get_irq(pdev, 0);
	struct device *dev = &pdev->dev;
	int32_t ret = -ENODEV;

	if (irq <= 0) {
		dev_err(dev, "[ERROR][USB] Found HC with no IRQ. Check %s setup!\n",
				dev_name(dev));
	} else {
		dev_info(dev, "[INFO][USB] platform_get_irq() SUCCESS, irq: %d\n",
				irq);

		ret = devm_request_irq(dev, (unsigned int)irq,
				tcc_dwc3_phy_chg_det_irq,
				IRQF_SHARED, pdev->dev.kobj.name, tcc_dwc3);
		if (ret != 0) {
			dev_err(dev, "[ERROR][USB] devm_request_irq() FAIL!, ret: %d\n",
					ret);
		} else {
			disable_irq((unsigned int)irq);
			tcc_dwc3->charge_detect_irq = irq;
			INIT_WORK(&tcc_dwc3->dwc3_work,
					tcc_dwc3_phy_chg_det_monitor);
		}
	}

	return ret;
}
#endif
#endif

static int32_t tcc_dwc3_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct tcc_dwc3_phy *tcc_dwc3;
	int32_t ret;

	dev_info(dev, "[INFO][USB] %s() call SUCCESS\n", __func__);

	tcc_dwc3 = devm_kzalloc(dev, sizeof(*tcc_dwc3), GFP_KERNEL);

	ret = tcc_dwc3_phy_create(dev, tcc_dwc3);
	if (ret != 0) {
		dev_err(dev, "[ERROR][USB] tcc_dwc3_phy_create() FAIL!\n");
	} else {
		if (request_mem_region(pdev->resource[0].start,
					resource_size(&pdev->resource[0]),
					"dwc3_phy") == NULL) {
			dev_dbg(dev, "[DEBUG][USB] error reserving mapped memory\n");
			ret = -EFAULT;
		} else {
			tcc_dwc3->phy_regs = ioremap((resource_size_t)
					pdev->resource[0].start,
					resource_size(&pdev->resource[0]));

			tcc_dwc3->ref_base = ioremap((resource_size_t)
					pdev->resource[1].start,
					resource_size(&pdev->resource[1]));

			platform_set_drvdata(pdev, tcc_dwc3);

			ret = usb_add_phy_dev(&tcc_dwc3->uphy);
			if (ret != 0) {
				dev_err(dev, "[ERROR][USB] usb_add_phy_dev() FAIL!, ret: %d\n",
						ret);
#if 0
			} else {
#if defined(CONFIG_ENABLE_BC_30_HOST)
				ret = tcc_dwc3_phy_init_battery_charging(pdev,
						tcc_dwc3);
#endif /* CONFIG_ENABLE_BC_30_HOST */
				/* Do nothing, if BC 1.2 is not configured */
#endif
			}
		}
	}

	return ret;
}

static int32_t tcc_dwc3_phy_remove(struct platform_device *pdev)
{
	struct tcc_dwc3_phy *tcc_dwc3;
	int32_t ret = -ENODEV;

	if (pdev != NULL) {
		tcc_dwc3 = platform_get_drvdata(pdev);

		usb_remove_phy(&tcc_dwc3->uphy);

		release_mem_region(pdev->resource[0].start, // dwc3 base
				resource_size(&pdev->resource[0]));
		release_mem_region(pdev->resource[1].start, // dwc3 phy base
				resource_size(&pdev->resource[1]));

		ret = 0;
	}

	return ret;
}

static const struct of_device_id tcc_dwc3_phy_match[] = {
	{ .compatible = "telechips,tcc_dwc3_phy" },
	{},
};
MODULE_DEVICE_TABLE(of, tcc_dwc3_phy_match);

static struct platform_driver tcc_dwc3_phy_driver = {
	.probe			= tcc_dwc3_phy_probe,
	.remove			= tcc_dwc3_phy_remove,
	.driver = {
		.name		= "dwc3_phy",
		.owner		= THIS_MODULE,
		.of_match_table = of_match_ptr(tcc_dwc3_phy_match),
	},
};

static int32_t __init tcc_dwc3_phy_drv_init(void)
{
	int32_t ret = 0;

	ret = platform_driver_register(&tcc_dwc3_phy_driver);
	if (ret < 0) {
		pr_err("[ERROR][USB] %s() FAIL! ret: %d\n", __func__, ret);
	}

	return ret;
}
subsys_initcall_sync(tcc_dwc3_phy_drv_init);

static void __exit tcc_dwc3_phy_drv_cleanup(void)
{
	platform_driver_unregister(&tcc_dwc3_phy_driver);
}
module_exit(tcc_dwc3_phy_drv_cleanup);

MODULE_DESCRIPTION("Telechips DWC3 USB transceiver driver");
MODULE_LICENSE("GPL v2");
