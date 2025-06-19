// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/uaccess.h>
#include <linux/pinctrl/consumer.h>
#include <linux/ktime.h>
#include <linux/reset.h>

/* I2C Configuration Reg. */
#define I2C_PRES	0x00U
#define I2C_CTRL	0x04U
#define I2C_TXR		0x08U
#define I2C_CMD		0x0CU
#define I2C_RXR		0x10U
#define I2C_SR		0x14U
#define I2C_TR0		0x18U
#define I2C_TR1		0x24U
#define I2C_ACR		0xFCU
#define I2C_ACR4	0x4FCU

/* I2C CTRL Reg */
#define CTRL_EN		((u32)BIT(7))
#define CTRL_IEN	((u32)BIT(6))

/* I2C CMD Reg */
#define CMD_START	((u32)BIT(7))
#define CMD_STOP	((u32)BIT(6))
#define CMD_RD		((u32)BIT(5))
#define CMD_WR		((u32)BIT(4))
#define CMD_NACK	((u32)BIT(3))
#define CMD_IACK	((u32)BIT(0))
#define CMD_MASK	((u32)(BIT(4) | BIT(5) | BIT(6) | BIT(7)))

/* I2C Command Register clear Timeout ms */
#define CMD_TIMEOUT	1U

/* I2C SR Reg */
#define SR_THST_SDA	((u32)BIT(11))
#define SR_THST_SCL	((u32)BIT(10))
#define SR_RX_ACK	((u32)BIT(7))
#define SR_BUSY		((u32)BIT(6))
#define SR_AL		((u32)BIT(5))
#define SR_TIP		((u32)BIT(1))
#define SR_IF		((u32)BIT(0))

/* I2C ACR Reg */
#define ACR_IDLE	0x00U
#define ACR_HAVE_ACC_PERM ((u32)BIT(1))
#define ACR_AP		((u32)BIT(4))
#define ACR_ISP0	((u32)BIT(5))
#define ACR_ISP1	((u32)BIT(6))
#define ACR_ISP2	((u32)BIT(7))
#define ACR_ARBS_MASK	0xF0U

/* I2C Port Configuration Reg. */
#define I2C_PORT_CFG0	0x00U
#define I2C_PORT_CFG1	0x04U
#define I2C_PORT_CFG2	0x08U
#define I2C_IRQ_STS	0x10U

/* Shared i2c core Arbitration Timeout ms */
#define SHARED_I2C_ARB_TIMEOUT 5U

#define IOBUS_I2C_PERI_CLK	4000000UL
#define DEDICATED_I2C_PERI_CLK	24000000UL
#define I2C_TR0_FC_MASK ((u32)0xFFFFFFE0U)
#define I2C_TR1_MAX	4096U

#define I2C_M_NO_STOP	0x0020

#define THST_SDA(val)		((val) & (SR_THST_SDA))
#define THST_SCL(val)		((val) & (SR_THST_SCL))
#define TCC_I2C_ERR_FLAG(val)	((val) & ((SR_AL) | (SR_BUSY)))

#define UTIME_DIFF(ns)	(div_u64(((ktime_get_mono_fast_ns()) - (ns)), (NSEC_PER_USEC)))

/*
 * We are generating clock pulses. ndelay() determines the duration of clk pulses.
 * We will generate clock with rate 100 KHz and so duration of both clock levels are
 * : period in ns = (10^6 / 100)
 */
#define RECOVERY_PERIOD_NSEC	10000UL
#define RECOVERY_CLK_CNT	9

/* I2C transfer state */
enum {
	STATE_IDLE,	/* Before Start */
	STATE_START,	/* Slave address write ended*/
	STATE_TRANSFER,	/* I2C data transfer in current msg*/
	STATE_FINISH,	/* Last msg send completed */
	STATE_COMPLETE,	/* Return to Driver */
};

#define TCC803X	((u32)BIT(0))
#define TCC805X	((u32)BIT(1))
#define TCC750X	((u32)BIT(2))
#define TCC807X	((u32)BIT(3))
#define TCN100X	((u32)BIT(4))

#define i2c_readl  __raw_readl
#define i2c_writel __raw_writel

struct i2c_msg_flags {
	bool is_rw_reverse;
	bool is_transfer_read;
	bool is_ack_ignore;
	bool is_force_stop;
	bool is_nostop;
};

struct tcc_i2c_struct {
	void                           __iomem *base;
	void                           __iomem *port_cfg;
	void                           __iomem *shared_base;
	void                           __iomem *virt_slv;

	bool                          is_last_msg;
	bool                          is_last_data;
	bool                          is_cur_msg_complete;
	bool                          rexfer;
	bool                          burst_mode;
	bool                          cam_i2c_use; /*dedicated I2C in camera */
	uint8_t                       core;
	uint16_t                      cur_data_index;
	int32_t                       last_msg_index;
	int32_t                       cur_msg_index;
	uint32_t                      port_mux;
	uint32_t                      i2c_clk_rate;
	uint32_t                      pwh;
	uint32_t                      pwl;
	int32_t	                      result;	/* transfer result */
	int32_t	                      state;
	uint32_t                      irq;
	uint32_t                      noise_filter;
	uint32_t                      xfer_timeout;
	uint32_t                      max_retry_count;
	uint32_t                      cur_retry_count;
	uint32_t                      cmd_clear_timeout;
	uint64_t                      cmd_set_time;

	struct clk                    *pclk; /* I2C Peri */
	struct clk                    *hclk; /* I2C IO config*/
	struct clk                    *fclk; /* FBUS_IO */
	struct pinctrl                *pinctl; /* Pin-control */
	struct i2c_msg                *msgs;
	struct reset_control	      *rst;  /* I2C SW reset */

	struct completion             msg_complete;

	struct device                 *dev;
	struct i2c_adapter            adap;
	struct i2c_bus_recovery_info  tcc_rinfo;
	struct i2c_msg_flags          msg_flags;
	const struct tcc_i2c_soc_info *soc_info;
};

struct tcc_i2c_soc_info {
	uint32_t id;
	uint8_t last_ch;
	uint8_t isp_i2c_ch;
	uint8_t cam_i2c_ch0;
	uint8_t cam_i2c_ch1;
};

static ssize_t tcc_i2c_show(struct device *dev,
		struct device_attribute *attr,
		char *buf);

static DEVICE_ATTR(tcci2c_v3,
		((u16)S_IRUGO),
		(tcc_i2c_show),
		(NULL));

static int32_t tcc_i2c_config(const struct tcc_i2c_struct *tcc_i2c);
static int32_t tcc_i2c_request_acc_perm(void __iomem *base, bool on,
					const struct tcc_i2c_struct *tcc_i2c);
static void tcc_i2c_stop(struct tcc_i2c_struct *tcc_i2c);

static void __iomem *shared_i2c_base;

int32_t tcc_i2c7_set_trfc(uint32_t fc, const struct tcc_i2c_struct *tcc_i2c)
{
	int32_t ret = 0;
	uint32_t reg_data;

	/* i2c7 permission request */
	ret = tcc_i2c_request_acc_perm(shared_i2c_base, (bool)1, tcc_i2c);
	if (ret == 0) {
		reg_data = i2c_readl(shared_i2c_base + I2C_TR0);
		reg_data &= I2C_TR0_FC_MASK;
		reg_data |= (fc & ~I2C_TR0_FC_MASK);
		i2c_writel(reg_data, shared_i2c_base + I2C_TR0);

		/* i2c7 permission release */
		(void)tcc_i2c_request_acc_perm(shared_i2c_base, (bool)0, tcc_i2c);
	}

	return ret;
}
EXPORT_SYMBOL(tcc_i2c7_set_trfc);

int32_t tcc_i2c7_get_trfc(uint32_t *fc, const struct tcc_i2c_struct *tcc_i2c)
{
	int32_t ret = 0;
	uint32_t reg_data;

	/* i2c7 permission request */
	ret = tcc_i2c_request_acc_perm(shared_i2c_base, (bool)1, tcc_i2c);
	if (ret == 0) {
		reg_data = i2c_readl(shared_i2c_base + I2C_TR0);
		reg_data &= ~I2C_TR0_FC_MASK;
		*fc = reg_data;

		/* i2c7 permission release */
		(void)tcc_i2c_request_acc_perm(shared_i2c_base, (bool)0, tcc_i2c);
	}

	return ret;
}
EXPORT_SYMBOL(tcc_i2c7_get_trfc);

static void tcc_i2c_clear_sr(const struct tcc_i2c_struct *tcc_i2c)
{
	int32_t ret = 0;
	uint32_t ctrl_reg = i2c_readl(tcc_i2c->base + I2C_CTRL) & (CTRL_EN | CTRL_IEN);
	uint32_t fc = 0;

	if ((tcc_i2c->soc_info->id == TCC805X) &&
	    (tcc_i2c->core == tcc_i2c->soc_info->isp_i2c_ch)) {
		ret = tcc_i2c7_get_trfc(&fc, tcc_i2c);
	}

	if (ret == 0) {
		if (tcc_i2c->soc_info->id != TCC750X) {
			clk_disable_unprepare(tcc_i2c->hclk);
			/* Clear I2C SR register SW reset */
			reset_control_assert(tcc_i2c->rst);
			reset_control_deassert(tcc_i2c->rst);

			ret = clk_prepare_enable(tcc_i2c->hclk);
			if (ret < 0) {
				dev_err(tcc_i2c->dev,
					"[ERROR][I2C] %s, failed to swreset release\n",
					__func__);
			}
		} else {
			/* TO DO. TCC750X SW RESET*/
		}
	}

	if (ret == 0) {
		if ((tcc_i2c->soc_info->id == TCC805X) &&
		    (tcc_i2c->core == tcc_i2c->soc_info->isp_i2c_ch)) {
			ret = tcc_i2c7_set_trfc(fc, tcc_i2c);
		}
	}

	if (ret == 0) {
		/* set clock & port_mux */
		ret = tcc_i2c_config(tcc_i2c);
		if (ret < 0) {
			dev_err(tcc_i2c->dev,
				"[ERROR][I2C] I2C config fail in %s\n", __func__);
		}
	}

	if (ret == 0) {
		if (tcc_i2c->core == tcc_i2c->soc_info->isp_i2c_ch) {
			ret = tcc_i2c_request_acc_perm(tcc_i2c->base, (bool)true, tcc_i2c);
		}
	}

	if (ret == 0) {
		i2c_writel(ctrl_reg, tcc_i2c->base + I2C_CTRL);
	}
}

static bool tcc_i2c_time_after(ulong orig_jiffies, uint32_t timeout_ms)
{
	ulong timeout_jiffies;

	if (orig_jiffies < (ULONG_MAX - msecs_to_jiffies(timeout_ms))) {
		timeout_jiffies = orig_jiffies + msecs_to_jiffies(timeout_ms);
	} else {
		timeout_jiffies = orig_jiffies - ULONG_MAX + msecs_to_jiffies(timeout_ms);
	}

	return time_after(jiffies, timeout_jiffies);
}

static void tcc_i2c_clear_intr_flag(const struct tcc_i2c_struct *tcc_i2c)
{
	i2c_writel((i2c_readl(tcc_i2c->base + I2C_CMD) | CMD_IACK),
			tcc_i2c->base + I2C_CMD);
}

/* Wait for i2c command complete */
static void tcc_i2c_wait_for_cmd_complete(const struct tcc_i2c_struct *tcc_i2c)
{
	ulong orig_jiffies = 0;

	orig_jiffies = jiffies;
	while ((i2c_readl(tcc_i2c->base + I2C_CMD) & CMD_MASK) != 0U) {
		if (tcc_i2c_time_after(orig_jiffies, CMD_TIMEOUT)) {
			break;
		}
	}

	/* Clear the interrupt flag set by the completion of the i2c command. */
	tcc_i2c_clear_intr_flag(tcc_i2c);
}

static void tcc_i2c_enable_core(
		const struct tcc_i2c_struct *tcc_i2c,
		bool enable)
{
	uint32_t reg_val = i2c_readl(tcc_i2c->base + I2C_CTRL);

	if (enable == (bool)true) {
		/* Enable I2C Core */
		reg_val |= CTRL_EN;
	} else {
		/* Disable I2C Core */
		reg_val &= ~CTRL_EN;
	}

	i2c_writel(reg_val, tcc_i2c->base + I2C_CTRL);
}

static bool tcc_i2c_is_enable_intr(const struct tcc_i2c_struct *tcc_i2c)
{
	return ((i2c_readl(tcc_i2c->base + I2C_CTRL) & CTRL_IEN) != 0U);
}

static void tcc_i2c_enable_intr(const struct tcc_i2c_struct *tcc_i2c,
				bool enable)
{
	if (enable) {
		i2c_writel(i2c_readl(tcc_i2c->base + I2C_CTRL) | CTRL_IEN,
				tcc_i2c->base + I2C_CTRL);
	} else {
		i2c_writel(i2c_readl(tcc_i2c->base + I2C_CTRL) & ~CTRL_IEN,
				tcc_i2c->base + I2C_CTRL);
	}
}

static int tcc_i2c_sda_recovery(struct i2c_adapter *adap)
{
	const struct i2c_bus_recovery_info *rinfo = adap->bus_recovery_info;
	const struct tcc_i2c_struct *tcc_i2c = (struct tcc_i2c_struct *)adap->algo_data;
	ulong high_period = 2, low_period = 3;
	int32_t i = 0, scl = 1, ret = 0;

	if ((tcc_i2c->pwh <= I2C_TR1_MAX) &&
	    (tcc_i2c->pwl <= I2C_TR1_MAX)) {
		high_period = RECOVERY_PERIOD_NSEC * tcc_i2c->pwh /
				(tcc_i2c->pwh + tcc_i2c->pwl);
		low_period = RECOVERY_PERIOD_NSEC * tcc_i2c->pwl /
				(tcc_i2c->pwh + tcc_i2c->pwl);
	}

	if ((rinfo->prepare_recovery != NULL) &&
	    (rinfo->unprepare_recovery != NULL) &&
	    (rinfo->set_scl != NULL) &&
	    (rinfo->get_scl != NULL) &&
	    (rinfo->get_sda != NULL)) {
		rinfo->prepare_recovery(adap);

		/*
		 * By this time SCL is high, as we need to give 9 falling-rising edges
		 */
		while (i++ < (RECOVERY_CLK_CNT * 2)) {
			scl = (scl == 1) ? 0 : 1;
			rinfo->set_scl(adap, scl);

			if (scl != 0)  {
				ndelay(high_period);

				if ((rinfo->get_sda(adap) != 0) ||
				    (rinfo->get_scl(adap) == 0)) {
					/* SCL shouldn't be low here */
					if (rinfo->get_scl(adap) == 0) {
						dev_err(&adap->dev,
							"SCL is stuck low, exit recovery\n");
						ret = -EBUSY;
					}

					break;
				}
			} else {
				ndelay(low_period);
			}
		}

		rinfo->unprepare_recovery(adap);
	} else {
		ret = -ENOSYS;
	}

	return ret;
}

static int tcc_i2c_get_scl_value(struct i2c_adapter *adap)
{
	return gpiod_get_value(adap->bus_recovery_info->scl_gpiod);
}

static void tcc_i2c_set_scl_value(struct i2c_adapter *adap, int val)
{
	gpiod_set_value(adap->bus_recovery_info->scl_gpiod, val);
}

static int tcc_i2c_get_sda_value(struct i2c_adapter *adap)
{
	return gpiod_get_value(adap->bus_recovery_info->sda_gpiod);
}

static void tcc_i2c_prepare_recovery(struct i2c_adapter *adap)
{
	const struct tcc_i2c_struct *tcc_i2c = (struct tcc_i2c_struct *)adap->algo_data;
	struct pinctrl_state *sleep_pinctl;
	int32_t ret = 0;

	if (tcc_i2c->core == tcc_i2c->soc_info->isp_i2c_ch) {
		ret = tcc_i2c_request_acc_perm(tcc_i2c->base, (bool)true, tcc_i2c);
	}

	if (ret == 0) {
		/* Core disable */
		tcc_i2c_enable_core(tcc_i2c, (bool)false);

		if (tcc_i2c->core == tcc_i2c->soc_info->isp_i2c_ch) {
			(void)tcc_i2c_request_acc_perm(tcc_i2c->base, (bool)false, tcc_i2c);
		}

		sleep_pinctl = pinctrl_lookup_state(tcc_i2c->pinctl,
					PINCTRL_STATE_SLEEP);

		(void)pinctrl_select_state(tcc_i2c->pinctl, sleep_pinctl);
		(void)gpiod_direction_output(adap->bus_recovery_info->scl_gpiod, 1);
	}
}

static void tcc_i2c_unprepare_recovery(struct i2c_adapter *adap)
{
	const struct tcc_i2c_struct *tcc_i2c = (struct tcc_i2c_struct *)adap->algo_data;
	struct pinctrl_state *default_pinctl;
	int32_t ret = 0;

	default_pinctl = pinctrl_lookup_state(tcc_i2c->pinctl,
				PINCTRL_STATE_DEFAULT);

	(void)pinctrl_select_state(tcc_i2c->pinctl, default_pinctl);

	if (tcc_i2c->core == tcc_i2c->soc_info->isp_i2c_ch) {
		ret = tcc_i2c_request_acc_perm(tcc_i2c->base, (bool)true, tcc_i2c);
	}

	if (ret == 0) {
		/* Core enable*/
		tcc_i2c_enable_core(tcc_i2c, (bool)true);

		if (tcc_i2c->core == tcc_i2c->soc_info->isp_i2c_ch) {
			(void)tcc_i2c_request_acc_perm(tcc_i2c->base, (bool)false, tcc_i2c);
		}
	}
}

static int32_t tcc_i2c_recovery_init(struct tcc_i2c_struct *tcc_i2c)
{
	int ret = 0;
	struct i2c_bus_recovery_info *rinfo = &tcc_i2c->tcc_rinfo;
	struct pinctrl_state *sleep_pinctl, *default_pinctl;

	if (IS_ERR_OR_NULL((void *)tcc_i2c->pinctl)) {
		dev_dbg(tcc_i2c->dev,
			"[DEBUG][I2C] can't get pinctrl, bus recovery not supported\n");
		ret = -EINVAL;
	}

	if (ret == 0) {
		rinfo->scl_gpiod = devm_gpiod_get_index(
						tcc_i2c->dev,
						"recovery",
						0,
						GPIOD_OUT_HIGH);

		rinfo->sda_gpiod = devm_gpiod_get_index(
						tcc_i2c->dev,
						"recovery",
						1,
						GPIOD_IN);

		sleep_pinctl = pinctrl_lookup_state(tcc_i2c->pinctl,
				PINCTRL_STATE_SLEEP);
		default_pinctl = pinctrl_lookup_state(tcc_i2c->pinctl,
				PINCTRL_STATE_DEFAULT);

		if (IS_ERR_OR_NULL((void *)rinfo->sda_gpiod) ||
		    IS_ERR_OR_NULL((void *)rinfo->scl_gpiod) ||
		    IS_ERR_OR_NULL((void *)default_pinctl) ||
		    IS_ERR_OR_NULL((void *)sleep_pinctl)) {
			ret = -EINVAL;
		} else {
			rinfo->prepare_recovery		= tcc_i2c_prepare_recovery;
			rinfo->unprepare_recovery 	= tcc_i2c_unprepare_recovery;
			rinfo->recover_bus		= tcc_i2c_sda_recovery;
			rinfo->get_scl			= tcc_i2c_get_scl_value;
			rinfo->set_scl			= tcc_i2c_set_scl_value;
			rinfo->get_sda			= tcc_i2c_get_sda_value;
			tcc_i2c->adap.bus_recovery_info	= rinfo;
		}

		(void)pinctrl_select_state(tcc_i2c->pinctl, sleep_pinctl);
		(void)pinctrl_select_state(tcc_i2c->pinctl, default_pinctl);
	}

	return ret;
}

static void tcc_i2c_recovery(struct tcc_i2c_struct *tcc_i2c)
{
	uint32_t reg_val;

	if ((tcc_i2c->soc_info->id == TCC805X) ||
	    (tcc_i2c->soc_info->id == TCC807X) ||
	    (tcc_i2c->soc_info->id == TCN100X)) {
		reg_val = i2c_readl(tcc_i2c->base + I2C_SR);

		/* I2C SCL Line Recovery */
		if (THST_SCL(reg_val) == 0U) {
			tcc_i2c_stop(tcc_i2c);
			tcc_i2c_wait_for_cmd_complete(tcc_i2c);
		}

		if (tcc_i2c->adap.bus_recovery_info != NULL) {
			/* I2C SDA Line Recovery */
			if ((THST_SDA(reg_val) == 0U) && (THST_SCL(reg_val) != 0U)) {
				dev_dbg(&tcc_i2c->adap.dev,
					"[DEBUG][I2C] I2C BUS Recovery Start\n");

				/* Unable to set pinctrl inside interrupt handle
				   A similar signal is output when a Read command is written to the CMD register.
				 */
				if (in_atomic()) {
					/* generate 9 clocks */
					i2c_writel((CMD_RD | CMD_NACK), tcc_i2c->base + I2C_CMD);
					tcc_i2c_wait_for_cmd_complete(tcc_i2c);
				} else {
					if (i2c_recover_bus(&tcc_i2c->adap) < 0) {
						dev_err(&tcc_i2c->adap.dev,
							"[ERROR][I2C] I2C SDA Recovery Failed\n");
					}
				}
			}
		}

		/* Update SR register value after recovery operation */
		reg_val = i2c_readl(tcc_i2c->base + I2C_SR);

		/* Clear I2C error flag */
		if ((TCC_I2C_ERR_FLAG(reg_val) != 0U) &&
		    (THST_SDA(reg_val) != 0U) &&
		    (THST_SCL(reg_val) != 0U)) {
			tcc_i2c_clear_sr(tcc_i2c);
		}
	} else {
		tcc_i2c_clear_sr(tcc_i2c);
	}
}

/* Request for I2C Controller Access permission to share with ISP */
static int32_t tcc_i2c_request_acc_perm(void __iomem *base, bool on,
					const struct tcc_i2c_struct *tcc_i2c)
{
	ulong orig_jiffies = 0;
	int32_t ret = 0;

	if (!on) {
		/* clear permission */
		i2c_writel(0, base + I2C_ACR);
	} else if ((i2c_readl(base + I2C_ACR) & ACR_ARBS_MASK) == ACR_IDLE) {

		/* ACR_IDLE bit (ACR0 4bit) has two mean : IDLE / ISP3
		 * so check IDLE or ISP3
		 */
		if ((i2c_readl(base + I2C_ACR4) & 0xFU) ==  ACR_HAVE_ACC_PERM) {
			dev_dbg(tcc_i2c->dev, "[DEBUG][I2C] ISP3 has permission \n");
		/* if IDLE, AP get access permission  */
		} else {


			orig_jiffies = jiffies;


			/* Read status whether shared i2c is idle */
			while ((i2c_readl(base + I2C_ACR) & ACR_ARBS_MASK) != ACR_IDLE) {
				if (tcc_i2c_time_after(orig_jiffies, SHARED_I2C_ARB_TIMEOUT)) {
					dev_err(tcc_i2c->dev,
						"[ERROR][I2C] Shared I2C is busy - timeout 0\n");
					ret = -ETIMEDOUT;
					break;
				}
			}

			if (ret == 0) {
				/* request access perimission */
				i2c_writel(1, base + I2C_ACR);

				orig_jiffies = jiffies;

				/* Check status for permission */
				while ((i2c_readl(base + I2C_ACR) & ACR_AP) == 0U) {
					if (tcc_i2c_time_after(orig_jiffies, SHARED_I2C_ARB_TIMEOUT)) {
						/* clear permission */
						i2c_writel(0, base + I2C_ACR);
						dev_err(tcc_i2c->dev,
							"[ERROR][I2C] Shared I2C is busy - timeout 1\n");
						ret = -ETIMEDOUT;
						break;
					}
				}
				dev_dbg(tcc_i2c->dev, "[DEBUG][I2C] AP has permission \n");
			}
		}
	/* ISP has permission to use I2C */
	} else if (((i2c_readl(base + I2C_ACR) & ACR_ARBS_MASK) == ACR_ISP0) ||
		   ((i2c_readl(base + I2C_ACR) & ACR_ARBS_MASK) == ACR_ISP1) ||
		   ((i2c_readl(base + I2C_ACR) & ACR_ARBS_MASK) == ACR_ISP2)) {
		dev_dbg(tcc_i2c->dev, "[DEBUG][I2C] ISP 0~2 has permission \n");
	} else {
		dev_dbg(tcc_i2c->dev, "[DEBUG][I2C] i2c-7 permission has already been acquired\n");
	}

	return ret;
}

static void tcc_i2c_disable_clock(const struct tcc_i2c_struct *tcc_i2c)
{
	if (tcc_i2c->hclk != NULL) {
		clk_disable_unprepare(tcc_i2c->hclk);
	}
}

static void tcc_i2c_v1_set_noise_filter(const struct tcc_i2c_struct *tcc_i2c,
	uint32_t filter_cnt)
{
	uint32_t tr0_reg;

	tr0_reg = i2c_readl(tcc_i2c->base + I2C_TR0);
	tr0_reg |= ((filter_cnt & 0x0000001FU) << 0);

	i2c_writel(tr0_reg, tcc_i2c->base + I2C_TR0);
}

static void tcc_i2c_v2_set_noise_filter(const struct tcc_i2c_struct *tcc_i2c,
	uint32_t filter_cnt)
{
	uint32_t tr0_reg;

	tr0_reg = i2c_readl(tcc_i2c->base + I2C_TR0);
	tr0_reg |= ((filter_cnt & 0x000001FFU) << 20);

	i2c_writel(tr0_reg, tcc_i2c->base + I2C_TR0);
}


static int32_t tcc_i2c_set_noise_filter(const struct tcc_i2c_struct *tcc_i2c)
{
	uint32_t fclk_Mhz, filter_cnt = 0;
	uint64_t temp_64 = 0;
	int32_t ret = 0;

	/* Calculate noise filter counter load value */
	temp_64 = clk_get_rate(tcc_i2c->fclk);

	if (temp_64 <= UINT_MAX) {
		fclk_Mhz = (u32)temp_64 / (u32)1000000U;

		if ((UINT_MAX / fclk_Mhz) < tcc_i2c->noise_filter) {
			dev_err(tcc_i2c->dev,
				"[ERROR][I2C] %s: noise filter %dns is too much\n",
				__func__,
				tcc_i2c->noise_filter);
			ret = -EINVAL;
		} else {
			filter_cnt = fclk_Mhz * tcc_i2c->noise_filter;
			filter_cnt = (filter_cnt / 1000U) + 2U;

			switch (tcc_i2c->soc_info->id) {
			case TCC803X:
				if (tcc_i2c->core < 4U) {
					tcc_i2c_v1_set_noise_filter(
							tcc_i2c,
							filter_cnt);
				} else {
					tcc_i2c_v2_set_noise_filter(
							tcc_i2c,
							filter_cnt);
				}
				break;
			case TCC805X:
			case TCC750X:
			case TCC807X:
			case TCN100X:
				tcc_i2c_v2_set_noise_filter(
						tcc_i2c,
						filter_cnt);
				break;
			default:
				dev_err(tcc_i2c->dev,
					"[ERROR][I2C] Invalid SoC id\n");
				ret = -ENXIO;
				break;
			}

			dev_dbg(tcc_i2c->dev,
				"[DEBUG][I2C] %s: fbus_io clk: %uMHz, noise filter load value: %d\n",
				__func__,
				fclk_Mhz,
				filter_cnt);
		}
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static int32_t tcc_i2c_v1_set_prescale(const struct tcc_i2c_struct *tcc_i2c)
{
	uint32_t prescale = 0;
	uint64_t peri_clk = clk_get_rate(tcc_i2c->pclk);
	int32_t ret = 0;

	if (peri_clk <= UINT_MAX) {
		prescale = 5U * tcc_i2c->i2c_clk_rate;
		prescale = ((u32)peri_clk / prescale);
	} else {
		ret = -EINVAL;
	}

	if (ret == 0) {
		if (prescale >= 1U) {
			prescale -= 1U;
			i2c_writel(prescale, tcc_i2c->base + I2C_PRES);
		} else {
			ret = -EINVAL;
		}
	}

	return ret;
}

static int32_t tcc_i2c_v2_set_prescale(const struct tcc_i2c_struct *tcc_i2c)
{
	uint32_t prescale = 0, temp_32 = 0;
	uint64_t peri_clk = clk_get_rate(tcc_i2c->pclk);
	uint64_t temp_64 = 0;
	int32_t ret = 0;
	/*
	 * TR0.CKSEL = 0
	 * TR0.STD = 0
	 */
	i2c_writel(i2c_readl(tcc_i2c->base + I2C_TR0) & (~0x000000E0U),
			tcc_i2c->base + I2C_TR0);

	if ((tcc_i2c->pwh <= I2C_TR1_MAX) &&
		(tcc_i2c->pwl <= I2C_TR1_MAX)) {
		temp_32 = tcc_i2c->pwh + tcc_i2c->pwl;
		temp_64 = (u64)tcc_i2c->i2c_clk_rate * temp_32;
	} else {
		ret = -EINVAL;
	}

	if (ret == 0) {
		if ((peri_clk <= UINT_MAX) &&
			(temp_64 <= UINT_MAX)) {
			prescale = ((u32)peri_clk / (u32)temp_64);
		} else {
			ret = -EINVAL;
		}
	}

	if (ret == 0) {
		if (prescale >= 1U) {
			prescale -= 1U;
			i2c_writel(prescale, tcc_i2c->base + I2C_PRES);
			i2c_writel(((tcc_i2c->pwh << 16) | (tcc_i2c->pwl)),
				tcc_i2c->base + I2C_TR1);
		} else {
			ret = -EINVAL;
		}
	}

	return ret;
}

static int32_t tcc_i2c_set_prescale(const struct tcc_i2c_struct *tcc_i2c)
{
	int32_t ret = 0;
	ulong peri_clk, fbus_ioclk;

	peri_clk = clk_get_rate(tcc_i2c->pclk);
	fbus_ioclk = clk_get_rate(tcc_i2c->fclk);

	/* bus clock must be four times faster than the peri clk
	 * master frequency for the internal synchronization
	 */
	if (fbus_ioclk < (peri_clk << (u64)2)) {
		dev_err(tcc_i2c->dev,
			"[ERROR][I2C] fbus io clk(%ldHz) must be four times faster than the peri clk(%ldHz)\n",
			fbus_ioclk,
			peri_clk);

		ret = -EIO;
	} else {
		switch (tcc_i2c->soc_info->id) {
		case TCC803X:
			if (tcc_i2c->core < 4U) {
				ret = tcc_i2c_v1_set_prescale(tcc_i2c);
			} else {
				ret = tcc_i2c_v2_set_prescale(tcc_i2c);
			}
			break;
		case TCC805X:
		case TCC750X:
		case TCC807X:
		case TCN100X:
			ret = tcc_i2c_v2_set_prescale(tcc_i2c);
			break;
		default:
			dev_err(tcc_i2c->dev, "[ERROR][I2C] Invalid SoC id\n");
			ret = -ENXIO;
			break;
		}

		if (ret != 0) {
			dev_err(tcc_i2c->dev, "[ERROR][I2C] Invalid SCL frequency\n");
		}
	}

	return ret;
}


static void tcc_i2c_start(struct tcc_i2c_struct *tcc_i2c)
{
	uint16_t addr;
	const struct i2c_msg *msg = &tcc_i2c->msgs[tcc_i2c->cur_msg_index];

	addr = (msg->addr & (u16)0x7F) << (u16)1;
	if (tcc_i2c->msg_flags.is_transfer_read) {
		addr |= (u16)0x01;
	}
	if (tcc_i2c->msg_flags.is_rw_reverse) {
		addr ^= (u16)0x01;
	}

	dev_dbg(tcc_i2c->dev,
		"[DEBUG][I2C] slave address: 0x%02x, current message num: %d, message data length: %d\n",
		msg->addr,
		tcc_i2c->cur_msg_index,
		msg->len);

	i2c_writel((u32)addr, tcc_i2c->base + I2C_TXR);
	i2c_writel((CMD_START | CMD_WR), tcc_i2c->base + I2C_CMD);

	if (tcc_i2c->cmd_clear_timeout > 0U) {
		tcc_i2c->cmd_set_time = ktime_get_mono_fast_ns();
	}
}

static void tcc_i2c_read(struct tcc_i2c_struct *tcc_i2c)
{
	if (tcc_i2c->is_last_data) {
		i2c_writel((CMD_RD | CMD_NACK),
			tcc_i2c->base + I2C_CMD);
	} else {
		i2c_writel(CMD_RD, tcc_i2c->base + I2C_CMD);
	}

	if (tcc_i2c->cmd_clear_timeout > 0U) {
		tcc_i2c->cmd_set_time = ktime_get_mono_fast_ns();
	}
}

static void tcc_i2c_read_to_buffer(const struct tcc_i2c_struct *tcc_i2c)
{
	const struct i2c_msg *msg = &tcc_i2c->msgs[tcc_i2c->cur_msg_index];
	uint32_t rxr;

	if (tcc_i2c->state == STATE_TRANSFER) {
		rxr = (i2c_readl(tcc_i2c->base + I2C_RXR) & 0xFFU);
		msg->buf[tcc_i2c->cur_data_index] = (u8)rxr;
		dev_dbg(tcc_i2c->dev,
			"[DEBUG][I2C] READ Data: 0x%02x\n",
			rxr);
	}
}

static void tcc_i2c_write(struct tcc_i2c_struct *tcc_i2c)
{
	const struct i2c_msg *msg = &tcc_i2c->msgs[tcc_i2c->cur_msg_index];

	i2c_writel(msg->buf[tcc_i2c->cur_data_index], tcc_i2c->base + I2C_TXR);
	i2c_writel(CMD_WR, tcc_i2c->base + I2C_CMD);

	if (tcc_i2c->cmd_clear_timeout > 0U) {
		tcc_i2c->cmd_set_time = ktime_get_mono_fast_ns();
	}
}

static bool tcc_i2c_check_ack(const struct tcc_i2c_struct *tcc_i2c)
{
	bool ret = (bool)true;
	uint32_t sr;

	if ((tcc_i2c->state == STATE_START) ||
	   ((tcc_i2c->state == STATE_TRANSFER) &&
	    !tcc_i2c->msg_flags.is_transfer_read)) {
		sr = i2c_readl(tcc_i2c->base + I2C_SR);
		ret = ((sr & SR_RX_ACK) != 0U) ? (bool)false : (bool)true;
	}

	return ret;
}

static void tcc_i2c_stop(struct tcc_i2c_struct *tcc_i2c)
{
	if (tcc_i2c->msg_flags.is_nostop) {
		complete(&tcc_i2c->msg_complete);
	} else {
		i2c_writel(CMD_STOP, tcc_i2c->base + I2C_CMD);
	}
}

static void tcc_i2c_force_stop(const struct tcc_i2c_struct *tcc_i2c)
{
	if (!tcc_i2c->is_last_msg && tcc_i2c->is_last_data) {
		i2c_writel((i2c_readl(tcc_i2c->base + I2C_CMD) | CMD_STOP),
				tcc_i2c->base + I2C_CMD);
	}
}

/* I2C bus arbitration check */
static int32_t tcc_i2c_check_arbitration(const struct tcc_i2c_struct *tcc_i2c)
{
	int32_t ret = 0;

	if ((i2c_readl(tcc_i2c->base + I2C_SR) & SR_AL) != 0U) {
		ret = -EIO;
	}

	return ret;
}

/* i2c flag parsing in current msg */
static int32_t tcc_i2c_parse_flag(struct tcc_i2c_struct *tcc_i2c)
{
	int32_t ret = 0;
	const struct i2c_msg *msg = &tcc_i2c->msgs[tcc_i2c->cur_msg_index];
	uint16_t flags = msg->flags;

	if ((flags & (u16)I2C_M_REV_DIR_ADDR) != 0U) {
		dev_dbg(tcc_i2c->dev, "[DEBUG][I2C] I2C_M_REV_DIR_ADDR flag set\n");
		tcc_i2c->msg_flags.is_rw_reverse = (bool)true;
	} else {
		tcc_i2c->msg_flags.is_rw_reverse = (bool)false;
	}

	if ((flags & (u16)I2C_M_RD) != 0U) {
		dev_dbg(tcc_i2c->dev, "[DEBUG][I2C] I2C_M_RD flag set\n");
		tcc_i2c->msg_flags.is_transfer_read = (bool)true;
	} else {
		tcc_i2c->msg_flags.is_transfer_read = (bool)false;
	}

	if ((flags & (u16)I2C_M_IGNORE_NAK) != 0U) {
		dev_dbg(tcc_i2c->dev, "[DEBUG][I2C] I2C_M_IGNORE_NAK flag set\n");
		tcc_i2c->msg_flags.is_ack_ignore = (bool)true;
	} else {
		tcc_i2c->msg_flags.is_ack_ignore = (bool)false;
	}

	if ((flags & (u16)I2C_M_STOP) != 0U) {
		dev_dbg(tcc_i2c->dev, "[DEBUG][I2C] I2C_M_STOP flag set\n");
		tcc_i2c->msg_flags.is_force_stop = (bool)true;
	} else {
		tcc_i2c->msg_flags.is_force_stop = (bool)false;
	}

	if ((flags & (u16)I2C_M_TEN) != 0U) {
		/* Invalid request */
		dev_dbg(tcc_i2c->dev, "[INFO][I2C] I2C_M_TEN flag not support\n");
		ret = -EBADRQC;
	}

	if ((flags & (u16)I2C_M_NOSTART) != 0U) {
		/* Invalid request */
		dev_dbg(tcc_i2c->dev, "[INFO][I2C] I2C_M_NOSTART flag not support\n");
		ret = -EBADRQC;
	}

	if ((flags & (u16)I2C_M_NO_RD_ACK) != 0U) {
		/* Invalid request */
		dev_dbg(tcc_i2c->dev, "[INFO][I2C] I2C_M_NO_RD_ACK flag not support\n");
		ret = -EBADRQC;
	}

	if ((flags & (u16)I2C_M_RECV_LEN) != 0U) {
		/* Invalid request */
		dev_dbg(tcc_i2c->dev, "[INFO][I2C] I2C_M_RECV_LEN flag not support\n");
		ret = -EBADRQC;
	}

	if ((flags & (u16)I2C_M_NO_STOP) != 0U) {
		dev_dbg(tcc_i2c->dev,
			"[INFO][I2C] I2C_M_NO_STOP flag set (Telechips Defined Flag)\n");
		tcc_i2c->msg_flags.is_nostop  = (bool)true;
	} else {
		tcc_i2c->msg_flags.is_nostop  = (bool)false;
	}

	return ret;
}

static void tcc_i2c_set_state(struct tcc_i2c_struct *tcc_i2c)
{
	dev_dbg(tcc_i2c->dev,
		"[DEBUG][I2C] current state: %d\n",
		tcc_i2c->state);

	switch (tcc_i2c->state) {
	case STATE_IDLE:
		tcc_i2c->state = STATE_START;
		break;
	case STATE_START:
	case STATE_TRANSFER:
		if ((tcc_i2c->is_last_msg &&
			tcc_i2c->is_cur_msg_complete) ||
			(tcc_i2c->result < 0)) {
			/* transfer ended or transfer failure */
			tcc_i2c->state = STATE_FINISH;
		} else if (tcc_i2c->is_cur_msg_complete) {
		/* complete sending current msg, msgs remains. */
			if (tcc_i2c->burst_mode == (bool)true) {
				tcc_i2c->state = STATE_START;
			} else {
				tcc_i2c->state = STATE_COMPLETE;
			}
		} else {
		/* data left to send in the current msg */
			tcc_i2c->state = STATE_TRANSFER;
		}
		break;
	case STATE_FINISH:
		tcc_i2c->state = STATE_COMPLETE;
		break;
	case STATE_COMPLETE:
		tcc_i2c->state = STATE_IDLE;
		break;
	default:
		dev_err(tcc_i2c->dev,
			"[ERROR][I2C] Invalid operation state: %d\n",
			tcc_i2c->state);
		break;
	}

	dev_dbg(tcc_i2c->dev,
		"[DEBUG][I2C] next state: %d\n",
		tcc_i2c->state);
}

static void tcc_i2c_sm_operation(struct tcc_i2c_struct *tcc_i2c)
{
	switch (tcc_i2c->state) {
	case STATE_START:
		if (tcc_i2c_parse_flag(tcc_i2c) < 0) {
			tcc_i2c->result = -EBADRQC;
		} else {
			tcc_i2c_start(tcc_i2c);
		}
		break;
	case STATE_TRANSFER:
		if (tcc_i2c->msg_flags.is_transfer_read) {
			tcc_i2c_read(tcc_i2c);
		} else {
			tcc_i2c_write(tcc_i2c);
		}

		if (tcc_i2c->msg_flags.is_force_stop) {
			tcc_i2c_force_stop(tcc_i2c);
		}
		break;
	case STATE_FINISH:
		if (tcc_i2c->result == 0) {
			/* Convert the index to message length*/
			tcc_i2c->result = tcc_i2c->cur_msg_index + 1;
		}

		tcc_i2c_stop(tcc_i2c);
		break;
	case STATE_COMPLETE:
		complete(&tcc_i2c->msg_complete);
		break;
	default:
		dev_err(tcc_i2c->dev,
			"[ERROR][I2C] Invalid state: %d\n",
			tcc_i2c->state);
		tcc_i2c->result = -EINVAL;
		break;
	}
}

static void tcc_i2c_get_next_msg(struct tcc_i2c_struct *tcc_i2c)
{
	const struct i2c_msg* msg;

	if (tcc_i2c->state == STATE_TRANSFER) {
		tcc_i2c->cur_data_index++;
	}

	if ((tcc_i2c->state == STATE_START) ||
	    (tcc_i2c->state == STATE_TRANSFER)) {
		msg = &tcc_i2c->msgs[tcc_i2c->cur_msg_index];

		/* Check if the msg to send is the last msg */
		if (tcc_i2c->cur_msg_index == tcc_i2c->last_msg_index) {
			tcc_i2c->is_last_msg = (bool)true;
		} else {
			tcc_i2c->is_last_msg = (bool)false;
		}

		/* all data in the current message has been transferd */
		/* I2C_SMBUS_QUICK protocol has a data length of 0 */
		if ((tcc_i2c->is_last_data) || (msg->len == 0U)) {
			tcc_i2c->is_cur_msg_complete = (bool)true;
			tcc_i2c->is_last_data = (bool)false;
			tcc_i2c->cur_data_index = 0;

			if (!tcc_i2c->is_last_msg) {
				/* Get next message */
				tcc_i2c->cur_msg_index++;
			}
		} else {
			tcc_i2c->is_cur_msg_complete = (bool)false;

			/* Convert the data length to index, count starts at 1, but index starts at 0 */
			/* Check if the data to send is the last data */
			if (tcc_i2c->cur_data_index == (msg->len - 1U)) {
				tcc_i2c->is_last_data = (bool)true;
			} else {
				tcc_i2c->is_last_data = (bool)false;
			}
		}
	}
}

static uint32_t get_time_diff_us(u64 start_time) {
	uint32_t ret;
	u64 time_diff, end_time = ktime_get_mono_fast_ns();

	if (end_time >= start_time) {
		time_diff = ktime_get_mono_fast_ns() - start_time;
	} else {
		time_diff = ULONG_MAX - start_time + end_time;
	}

	time_diff = div_u64(time_diff, NSEC_PER_USEC);
	if (time_diff > UINT_MAX) {
		ret = UINT_MAX;
	} else {
		ret = (uint32_t)time_diff;
	}

	return ret;
}

static int32_t tcc_i2c_doxfer(struct tcc_i2c_struct *tcc_i2c)
{
	if (tcc_i2c->msgs == NULL) {
		dev_err(tcc_i2c->dev,
			"[ERROR][I2C] i2c message is not set, state: %d\n",
			tcc_i2c->state);
		tcc_i2c->result = -EINVAL;
		goto out;
	}

	if ((tcc_i2c->state != STATE_IDLE) &&
	    (tcc_i2c->cmd_clear_timeout > 0U)) {
		if ((get_time_diff_us(tcc_i2c->cmd_set_time) >= tcc_i2c->cmd_clear_timeout) &&
		    (tcc_i2c->cur_retry_count > 0U)) {
			tcc_i2c->cur_retry_count--;
			tcc_i2c->result = -ENOLINK;
			goto out;
		}
	}

	if (!tcc_i2c->msg_flags.is_ack_ignore) {
		if (!tcc_i2c_check_ack(tcc_i2c)) {
			if (tcc_i2c->cur_retry_count > 0U) {
				/* check ACK repeatedly for recovery */
				tcc_i2c->cur_retry_count--;
				tcc_i2c->result = -ENOLINK;
			} else {
				dev_dbg(tcc_i2c->dev,
					"[DEBUG][I2C] No ACK from slave device\n");

				tcc_i2c->result = -EAGAIN;
			}
			goto out;
		}
	}

	/* Save the read data resulting from the previous read command */
	if (tcc_i2c->msg_flags.is_transfer_read) {
		tcc_i2c_read_to_buffer(tcc_i2c);
	}

	tcc_i2c_get_next_msg(tcc_i2c);
	tcc_i2c_set_state(tcc_i2c);
	tcc_i2c_sm_operation(tcc_i2c);

out:
	/* I2C bus arbitration lost check */
	if (tcc_i2c_check_arbitration(tcc_i2c) < 0) {
		dev_err(tcc_i2c->dev,
			"[ERROR][I2C] Arbitration lost, state: %d\n",
			tcc_i2c->state);
		tcc_i2c->result = -EBUSY;
	}

	return tcc_i2c->result;
}

static irqreturn_t tcc_i2c_isr(int irq, void *dev_id)
{
	struct tcc_i2c_struct *tcc_i2c = (struct tcc_i2c_struct *)dev_id;
	uint32_t reg_data = 0;
	irqreturn_t ret = IRQ_HANDLED;

	if (tcc_i2c->state != STATE_IDLE) {
		reg_data = i2c_readl(tcc_i2c->base + I2C_SR);
		/* check status register - interrupt flag */
		if (((reg_data & SR_IF) == 0U) ||
			(!tcc_i2c_is_enable_intr(tcc_i2c))) {
			ret = IRQ_NONE;
		} else {
			/* clear interrupt flag */
			tcc_i2c_clear_intr_flag(tcc_i2c);

			/* if i2c command register complete */
			reg_data = i2c_readl(tcc_i2c->base + I2C_SR);
			if ((reg_data & SR_TIP) == 0U) {
				if (tcc_i2c_doxfer(tcc_i2c) < 0) {
					complete(&tcc_i2c->msg_complete);
				}
			} else {
				dev_err(tcc_i2c->dev,
					"[ERROR][I2C] i2c-%d command not complete [SR: 0x%08x] [CMD: 0x%08x]\n",
					tcc_i2c->core,
					reg_data,
					i2c_readl(tcc_i2c->base + I2C_CMD));
				tcc_i2c->result = -EIO;
			}
		}
	}

	return ret;
}

/* tcc_i2c_xfer
 *
 * first port of call from the i2c bus code when an message needs
 * transferring across the i2c bus.
 */
static int32_t tcc_i2c_xfer(
		struct i2c_adapter *adap,
		struct i2c_msg *msgs,
		int32_t num)
{
	int32_t ret = 0;
	ulong orig_jiffies, time_left;

	struct tcc_i2c_struct *tcc_i2c =
		(struct tcc_i2c_struct *)adap->algo_data;
	dev_dbg(tcc_i2c->dev, "[DEBUG][I2C] i2c xfer call, msg cnt: %d\n", num);


	if ((tcc_i2c->core == tcc_i2c->soc_info->isp_i2c_ch) ||
	    (tcc_i2c->cam_i2c_use)) {
		ret = tcc_i2c_request_acc_perm(tcc_i2c->base, (bool)true, tcc_i2c);
		if (ret < 0) {
			tcc_i2c->result = ret;
			goto out;
		}
	}

	orig_jiffies = jiffies;

	/* I2C multi matser arbitration lost */
	while ((i2c_readl(tcc_i2c->base + I2C_SR) & SR_BUSY) != 0U) {
		/* Perform i2c line recovery in the situations below
		   - Occupy i2c line for more than xfer_timeout (ms) from another i2c master
		   - i2c line not recovering within xfer_timeout (ms) in i2c single master environment
		   - i2c line is recovered, but the i2c controller internal flag is not updated
		 */
		if (tcc_i2c_time_after(orig_jiffies, tcc_i2c->xfer_timeout)) {
			dev_err(tcc_i2c->dev,
				"[ERROR][I2C] failed to get i2c-%d, state: %d\n",
				tcc_i2c->core, tcc_i2c->state);
			tcc_i2c_recovery(tcc_i2c);
			tcc_i2c->result = -EBUSY;
			goto out;
		}
		schedule();
	}

	/* Convert the number of messages to index, count starts at 1, but index starts at 0 */
	if (num > 0) {
		tcc_i2c->last_msg_index = num - 1;
	} else {
		goto out;
	}

	tcc_i2c->result = 0;
	tcc_i2c->msgs = msgs;
	tcc_i2c->cur_msg_index = 0;
	tcc_i2c->cur_data_index = 0;
	tcc_i2c->cur_retry_count = tcc_i2c->max_retry_count;

	do {
		tcc_i2c->state = STATE_IDLE;
		tcc_i2c->rexfer = (bool)false;
		tcc_i2c->is_last_data = (bool)false;
		tcc_i2c->is_cur_msg_complete = (bool)false;

		/* Restart due to transfer failure */
		if (tcc_i2c->result < 0) {
			tcc_i2c->result = 0;
			tcc_i2c->cur_msg_index = 0;
			tcc_i2c->cur_data_index = 0;
		}

		tcc_i2c_clear_intr_flag(tcc_i2c);

		reinit_completion(&tcc_i2c->msg_complete);
		if (tcc_i2c_doxfer(tcc_i2c) >= 0) {
			/* Interrupt enable */
			tcc_i2c_enable_intr(tcc_i2c, (bool)true);

			/* wait for i2c transfer end */
			time_left = wait_for_completion_timeout(&tcc_i2c->msg_complete,
					msecs_to_jiffies(tcc_i2c->xfer_timeout));

			if (time_left == 0U) {
				dev_err(tcc_i2c->dev,
					"[ERROR][I2C] i2c transfer timeout, addr: %x, result: %d\n",
					tcc_i2c->msgs[tcc_i2c->cur_msg_index].addr,
					tcc_i2c->result);
				tcc_i2c->result = -ETIMEDOUT;
			}

			/* Interrupt disable */
			tcc_i2c_enable_intr(tcc_i2c, (bool)false);
		}

		if (tcc_i2c->result < 0) {
			/* Perform recovery operation only i2c single master */
			if (tcc_i2c->result != -EBUSY) {
				tcc_i2c_stop(tcc_i2c);
				tcc_i2c_wait_for_cmd_complete(tcc_i2c);
				tcc_i2c_recovery(tcc_i2c);
			}

			if (tcc_i2c->result == -ENOLINK) {
				tcc_i2c->rexfer = (bool)true;
			} else {
				tcc_i2c->rexfer = (bool)false;
			}
		} else {
			if ((!tcc_i2c->burst_mode) && (!tcc_i2c->is_last_msg)) {
				tcc_i2c->rexfer = (bool)true;
				schedule();
			}
		}
	} while (tcc_i2c->rexfer);

	dev_dbg(tcc_i2c->dev,
		"[DEBUG][I2C] i2c xfer finish, result (msgs cnt): %d\n",
		tcc_i2c->result);

out:
	if ((tcc_i2c->core == tcc_i2c->soc_info->isp_i2c_ch) ||		
	    (tcc_i2c->cam_i2c_use)) {
		(void)tcc_i2c_request_acc_perm(tcc_i2c->base, (bool)false, tcc_i2c);
	}

	tcc_i2c->msgs = NULL;
	tcc_i2c->state = STATE_IDLE;

	return tcc_i2c->result;
}

static uint32_t tcc_i2c_func(struct i2c_adapter *adap)
{
	return (u32)I2C_FUNC_I2C |
		(u32)I2C_FUNC_PROTOCOL_MANGLING |
		(u32)I2C_FUNC_SMBUS_EMUL;
}

/* i2c bus registration info */
static const struct i2c_algorithm tcc_i2c_algo = {
	.master_xfer	= tcc_i2c_xfer,
	.functionality	= tcc_i2c_func,
};

static uint32_t tcc_i2c_get_pcfg(const struct tcc_i2c_struct *tcc_i2c,
				 uint8_t core)
{
	uint32_t val, offset;
	uint8_t shift;
	const void __iomem *port_reg;

	/* AXON port_reg is placed in GPIO register map */
	if (tcc_i2c->soc_info->id == TCN100X) {
		offset = (u32)core * 4U;
		port_reg = tcc_i2c->port_cfg + offset;
		val = readl(port_reg);
	/* other Chip is placed in I2C controller register map */
	} else {
		if ((core < (u8)4) || (tcc_i2c->cam_i2c_use)) {
			port_reg = tcc_i2c->port_cfg + I2C_PORT_CFG0;
		} else if (core <= tcc_i2c->soc_info->last_ch) {
			port_reg = tcc_i2c->port_cfg + I2C_PORT_CFG2;
		} else {
			port_reg = tcc_i2c->port_cfg + I2C_PORT_CFG1;
		}
		core %= (u8)4;
		val = readl(port_reg);
		shift = core << (u8)3;
		val >>= shift;
	}

	return val & 0xFFU;
}

static void tcc_i2c_set_pcfg(const struct tcc_i2c_struct *tcc_i2c,
			     uint8_t core,
			     uint32_t port)
{
	uint32_t val, offset;
	uint8_t shift;
	void  __iomem *port_reg;

	/* AXON port_reg is placed in GPIO register map */
	if (tcc_i2c->soc_info->id == TCN100X) {
		offset = (u32)core * 4U;
		port_reg = tcc_i2c->port_cfg + offset;
		port &= 0x3FU;
		writel(port, port_reg);
	/* other Chip is placed in I2C controller register map */
	} else {
		if ((core < (u8)4) || (tcc_i2c->cam_i2c_use)) {
			port_reg = tcc_i2c->port_cfg + I2C_PORT_CFG0;
		} else if (core <= tcc_i2c->soc_info->last_ch) {
			port_reg = tcc_i2c->port_cfg + I2C_PORT_CFG2;
		} else {
			port_reg = tcc_i2c->port_cfg + I2C_PORT_CFG1;
		}

		core %= (u8)4;
		val = readl(port_reg);
		shift = core << (u8)3;

		if (shift < (u8)(sizeof(val) * (u8)BITS_PER_BYTE)) {
			val &= (~((u32)0xFF << shift));
			val |= (port << shift);
			writel(val, port_reg);
		}
	}
}

/* tcc_i2c_set_port
 * set the port of i2c
 */
static int32_t tcc_i2c_set_port(const struct tcc_i2c_struct *tcc_i2c)
{
	uint32_t port, conflict;
	uint8_t i, start_ch, end_ch;
	int32_t ret = 0;

	port = tcc_i2c->port_mux;

	/* sub system type have different I2C controller count
	 * IO sub system : 0~11, Camera sub system : 0~1(only tcc807x)
	 * TCN100X - 0~4: master
	 */
	if (tcc_i2c->cam_i2c_use) {
		start_ch = (u8)12U;
		end_ch = (u8)13U;
	} else if ((tcc_i2c->soc_info->id) == (u32)TCN100X) {
		start_ch = (u8)0U;
		end_ch = (u8)4U;
	} else {
		start_ch = (u8)0U;
		end_ch = (u8)11U;
	}

	if (port != 0xFFU) {
		tcc_i2c_set_pcfg(tcc_i2c, tcc_i2c->core, port);

		/* clear conflict for each i2c master/slave core,
		 * 0~7: master, 8~11: slave
		 */

		for (i = start_ch; i <= end_ch; i++) {
			if (i == tcc_i2c->core) {
				continue;
			}
			conflict = tcc_i2c_get_pcfg(tcc_i2c, i);
			if (port == conflict) {
				dev_dbg(tcc_i2c->dev,
					"[DEBUG][I2C] i2c port[%d] conflict\n",
					port);
				tcc_i2c_set_pcfg(tcc_i2c, i, 0xFFU);
			}
		}
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static int32_t tcc_i2c_enable_clock(const struct tcc_i2c_struct *tcc_i2c)
{
	int32_t ret = 0;
	unsigned long clk_rate;

	if (tcc_i2c->soc_info->id != TCC750X) {
		ret = clk_prepare_enable(tcc_i2c->hclk);
		if (ret < 0) {
			dev_err(tcc_i2c->dev,
				"[ERROR][I2C] Failed to release i2c swreset\n");
		}
	} else {
		/* TO DO. TCC750X SW RESET */
	}

	if (ret == 0) {
		ret = clk_prepare_enable(tcc_i2c->pclk);
		if (ret < 0) {
			dev_err(tcc_i2c->dev,
				"[ERROR][I2C] Failed to enable i2c peri clock\n");
		}
	}

	if (ret == 0) {
		if ((tcc_i2c->soc_info->id == (u32)TCC750X) &&
	            (tcc_i2c->core == tcc_i2c->soc_info->isp_i2c_ch)) {
			clk_rate = DEDICATED_I2C_PERI_CLK;
		} else {
			clk_rate = IOBUS_I2C_PERI_CLK;
		}

		ret = clk_set_rate(tcc_i2c->pclk, clk_rate);
		if (ret < 0) {
			dev_err(tcc_i2c->dev,
				"[ERROR][I2C] Failed to set i2c peri clock\n");
		}
	}

	if (ret < 0) {
		clk_disable_unprepare(tcc_i2c->hclk);
		clk_disable_unprepare(tcc_i2c->pclk);
	}

	return ret;
}

/* tcc_i2c_config
 *
 * Configuration the I2C controller, set the IO lines and frequency
 */
static int32_t tcc_i2c_config(const struct tcc_i2c_struct *tcc_i2c)
{
	int32_t ret = 0;

	if ((tcc_i2c->core == tcc_i2c->soc_info->isp_i2c_ch) ||
	    (tcc_i2c->cam_i2c_use)) {
		ret = tcc_i2c_request_acc_perm(tcc_i2c->base, (bool)true, tcc_i2c);
	}

	if (ret == 0) {
		ret = tcc_i2c_set_prescale(tcc_i2c);
		if (ret < 0) {
			dev_err(tcc_i2c->dev, "[ERROR][I2C] Failed to set prescale\n");
		}
	}

	if (ret == 0) {
		ret = tcc_i2c_set_noise_filter(tcc_i2c);
		if (ret < 0) {
			dev_err(tcc_i2c->dev,
				"[ERROR][I2C] %s: failed to set i2c noise filter.\n",
				__func__);
		}
	}

	if (ret == 0) {
		if ((tcc_i2c->soc_info->id == TCC803X) ||
			(tcc_i2c->soc_info->id == TCC805X) ||
			(tcc_i2c->soc_info->id == TCC807X) ||
			(tcc_i2c->soc_info->id == TCN100X)) {
			/* set port mux */
			ret = tcc_i2c_set_port(tcc_i2c);
			if (ret < 0) {
				dev_err(tcc_i2c->dev,
					"[ERROR][I2C] %s: failed to set port configuration\n",
					__func__);
			}
		}
		tcc_i2c_enable_core(tcc_i2c, (bool)true);
	}

	if ((tcc_i2c->core == tcc_i2c->soc_info->isp_i2c_ch) ||
	    (tcc_i2c->cam_i2c_use)) {
		(void)tcc_i2c_request_acc_perm(tcc_i2c->base, (bool)false, tcc_i2c);
	}

	return ret;
}

static int32_t tcc_i2c_parse_dt(struct platform_device *pdev,
		struct tcc_i2c_struct *tcc_i2c)
{
	struct device_node *np = pdev->dev.of_node;
	const struct resource *res;
	int32_t ret = 0, ret1 = 0, ret2 = 0;

	tcc_i2c->soc_info =
		(const struct tcc_i2c_soc_info *)of_device_get_match_data(&pdev->dev);
	if (tcc_i2c->soc_info == NULL) {
		ret = -EINVAL;
	}

	if (ret == 0) {
		/* get base register */
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		tcc_i2c->base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR((void*)tcc_i2c->base)) {
			ret = -EFAULT;
		}

		/* get i2c peri clock */
		tcc_i2c->pclk = of_clk_get(np, 0);
		if (IS_ERR((void*)tcc_i2c->pclk)) {
			ret = -EFAULT;
		}
	}

	if (ret == 0) {
		if ((tcc_i2c->soc_info->id == TCC803X) ||
		    (tcc_i2c->soc_info->id == TCC805X) ||
		    (tcc_i2c->soc_info->id == TCC807X) ||
		    (tcc_i2c->soc_info->id == TCN100X)) {
			/* set port mux */
			/* get i2c port config register */
			tcc_i2c->port_cfg = of_iomap(np, 1);

			if (IS_ERR(tcc_i2c->port_cfg)) {
				ret = -EFAULT;;
			} else {
				/* get i2c clock shared core register */
				tcc_i2c->shared_base = of_iomap(np, 2);

				if (IS_ERR(tcc_i2c->shared_base)) {
					ret = -EFAULT;
				}
			}

			if (ret == 0) {
				tcc_i2c->hclk = of_clk_get(np, 1);
				if (IS_ERR((void*)tcc_i2c->hclk)) {
					ret = -EFAULT;
				}
			}

			if (ret == 0) {
				tcc_i2c->fclk = of_clk_get(np, 2);
				if (IS_ERR((void*)tcc_i2c->fclk)) {
					ret = -EFAULT;
				}
			}

		} else if (tcc_i2c->soc_info->id == TCC750X) {
			tcc_i2c->virt_slv = of_iomap(np, 1);

			if (IS_ERR(tcc_i2c->virt_slv)) {
				ret = -EFAULT;
			}

			if (ret == 0) {
				tcc_i2c->hclk = NULL;
				tcc_i2c->fclk = of_clk_get(np, 1);
				if (IS_ERR_OR_NULL((void*)tcc_i2c->fclk)) {
					ret = -EFAULT;
				}
			}
		} else {
			ret = -EINVAL;
		}
	}

	if (ret == 0) {
		/* Get SCL Speed */
		ret = of_property_read_u32(np,
					"clock-frequency",
					&tcc_i2c->i2c_clk_rate);
		if (ret < 0) {
			tcc_i2c->i2c_clk_rate = 100000;
		}
		if (tcc_i2c->i2c_clk_rate > 400000U) {
			dev_warn(&pdev->dev, "[WARN][I2C] SCL %dHz is not supported\n",
					tcc_i2c->i2c_clk_rate);
			tcc_i2c->i2c_clk_rate = 400000;
		}

		if ((tcc_i2c->soc_info->id == TCC803X) ||
			(tcc_i2c->soc_info->id == TCC805X) ||
			(tcc_i2c->soc_info->id == TCC807X) ||
			(tcc_i2c->soc_info->id == TCN100X)) {
			/* Get Port mux */
			ret = of_property_read_u32(np, "port-mux", &tcc_i2c->port_mux);
			if (ret < 0) {
				dev_err(&pdev->dev,
					"[ERROR][I2C] failed to get port-mux\n");
			}
		}
	}

	if (ret == 0) {
		/* Get Pulse Width of I2C */
		ret1 = of_property_read_u32(np,
				"pulse-width-high",
				&tcc_i2c->pwh);
		ret2 = of_property_read_u32(np,
				"pulse-width-low",
				&tcc_i2c->pwl);
		if ((ret1 != 0) || (ret2 != 0) ||
			(tcc_i2c->pwh > I2C_TR1_MAX) ||
			(tcc_i2c->pwl > I2C_TR1_MAX)) {
			tcc_i2c->pwh = 2;
			tcc_i2c->pwl = 3;
		}

		/* Get Noise Filtering Time */
		ret = of_property_read_u32(np,
				"noise_filter",
				&tcc_i2c->noise_filter);
		if (ret != 0) {
			tcc_i2c->noise_filter = 50;
		}
		if (tcc_i2c->noise_filter < 50U) {
			dev_warn(&pdev->dev, "[WARN][I2C] I2C must suppress noise of less than 50ns.\n");
			tcc_i2c->noise_filter = 50;
		}

		/* Transfer wait timeout (ms) */
		ret = of_property_read_u32(np,
				"xfer-timeout",
				&tcc_i2c->xfer_timeout);
		if (ret != 0) {
			tcc_i2c->xfer_timeout = 500;
		}

		/* ack timeout max 1000 times */
		ret = of_property_read_u32(np,
				"retry-count",
				&tcc_i2c->max_retry_count);
		if (ret != 0) {
			tcc_i2c->max_retry_count = 0;
		}
		if (tcc_i2c->max_retry_count > 1000U) {
			tcc_i2c->max_retry_count = 1000U;
		}

		ret = of_property_read_u32(np,
				"cmd-timeout",
				&tcc_i2c->cmd_clear_timeout);
		if (ret != 0) {
			tcc_i2c->cmd_clear_timeout = 0;
		}

		/*i2c burst mode */
		tcc_i2c->burst_mode = of_property_read_bool(np, "burst-mode");
		ret = 0;
	}

	if (ret == 0) {
		/* SW reset */
		if ((tcc_i2c->soc_info->id == TCC805X) ||
			(tcc_i2c->soc_info->id == TCC807X)) {
			tcc_i2c->rst = devm_reset_control_get_shared_by_index(tcc_i2c->dev, 0);
			if (!IS_ERR_OR_NULL(tcc_i2c->rst)) {
				reset_control_deassert(tcc_i2c->rst);
			}
		}
	}

	return ret;
}

static int32_t tcc_i2c_probe(struct platform_device *pdev)
{
	struct tcc_i2c_struct *tcc_i2c;
	int32_t ret = 0, id = 0;

	if (pdev->dev.of_node == NULL) {
		ret = -EINVAL;
	}

	if (ret == 0) {
		tcc_i2c = devm_kzalloc(&pdev->dev,
					sizeof(struct tcc_i2c_struct),
					(u32)GFP_KERNEL);
		if (tcc_i2c == NULL) {
			ret = -ENOMEM;
		} else {
			tcc_i2c->adap.owner = THIS_MODULE;
			tcc_i2c->adap.algo = &tcc_i2c_algo;
			tcc_i2c->dev = &(pdev->dev);
		}
	}

	if (ret == 0) {
		ret = tcc_i2c_parse_dt(pdev, tcc_i2c);
		if (ret < 0) {
			dev_err(tcc_i2c->dev, "[ERROR][I2C] failed to device tree parsing\n");
		}
	}

	if (ret == 0) {
		ret = platform_get_irq(pdev, 0);
		if (ret < 0) {
			dev_err(tcc_i2c->dev, "[ERROR][I2C] no irq resource\n");
		} else {
			tcc_i2c->irq = (u32)ret;
			ret = 0;
		}
	}

	if (ret == 0) {
		/* Interrupt disable */
		tcc_i2c_enable_intr(tcc_i2c, (bool)false);

		(void)strlcpy(tcc_i2c->adap.name,
				pdev->name,
				sizeof(tcc_i2c->adap.name));
		ret = devm_request_irq(tcc_i2c->dev,
				  tcc_i2c->irq,
				  tcc_i2c_isr,
				  IRQF_SHARED,
				  tcc_i2c->adap.name,
				  tcc_i2c);

		if (ret < 0) {
			dev_err(tcc_i2c->dev,
				"[ERROR][I2C] Failed to request irq %d\n",
				tcc_i2c->irq);
		}
	}

	if (ret == 0) {
		id = of_alias_get_id(pdev->dev.of_node, "i2c");
		if ((id >= 0) && (id <= (s32)tcc_i2c->soc_info->last_ch)) {
			pdev->id = id;
			tcc_i2c->core = (u8)id;
		/* dedicated i2c in camera sub system */
		} else if ((tcc_i2c->soc_info->id == TCC807X) &&
				((id == tcc_i2c->soc_info->cam_i2c_ch0) ||
				(id == tcc_i2c->soc_info->cam_i2c_ch1))) {
			pdev->id = id;
			tcc_i2c->core = (u8)id;
			tcc_i2c->cam_i2c_use = (bool)true;

		} else {
			dev_err(tcc_i2c->dev,
				"[ERROR][I2C] Invalid I2C id: %d\n",
				id);
			ret = -EINVAL;
		}
	}

	if (ret == 0) {
		/* get pinctrl */
		tcc_i2c->pinctl = devm_pinctrl_get_select(tcc_i2c->dev, "default");
		if (IS_ERR_OR_NULL((void *)tcc_i2c->pinctl)) {
			dev_err(tcc_i2c->dev,
				"[ERROR][I2C] Failed to get pinctrl\n");
			ret = -ENODEV;
		}
	}

	if (ret == 0) {
		tcc_i2c->adap.algo_data = tcc_i2c;
		tcc_i2c->adap.dev.parent = &pdev->dev;
		tcc_i2c->adap.class = (u32)(I2C_CLASS_HWMON | I2C_CLASS_SPD);
		tcc_i2c->adap.nr = pdev->id;
		tcc_i2c->adap.dev.of_node = pdev->dev.of_node;

		ret = tcc_i2c_recovery_init(tcc_i2c);
		if (ret < 0) {
			dev_dbg(tcc_i2c->dev, "[DEBUG][I2C] Not Using I2C BUS Recovery\n");
		} else {
			dev_info(tcc_i2c->dev, "[INFO][I2C] Using I2C BUS Recovery\n");
		}

		init_completion(&tcc_i2c->msg_complete);
		platform_set_drvdata(pdev, tcc_i2c);

		ret = tcc_i2c_enable_clock(tcc_i2c);
		if (ret < 0) {
			dev_err(tcc_i2c->dev, "[ERROR][I2C] Failed to enable clock\n");
		}
	}

	if (ret == 0) {
		/* set clock & port_mux */
		ret = tcc_i2c_config(tcc_i2c);
		if (ret < 0) {
			dev_err(tcc_i2c->dev, "[ERROR][I2C] I2C config fail\n");
		}
	}

	if (ret == 0) {
		ret = i2c_add_numbered_adapter(&(tcc_i2c->adap));
		if (ret < 0) {
			dev_err(tcc_i2c->dev,
				"[ERROR][I2C] %s: failed to add bus\n",
				tcc_i2c->adap.name);
			i2c_del_adapter(&tcc_i2c->adap);
			/* I2C clock disable */
			tcc_i2c_disable_clock(tcc_i2c);
		}
	}

	if (ret == 0) {
		ret = device_create_file(tcc_i2c->dev, &dev_attr_tcci2c_v3);
		if (ret < 0) {
			dev_warn(tcc_i2c->dev, "[WARN][I2C] I2C device file not creating\n");
			ret = 0;
		}
	}

	if (ret == 0) {
		dev_info(tcc_i2c->dev,
			"[INFO][I2C] sclk: %dkHz retry: %d, noise filter: %dns, xfer_timeout: %dms, retry_count: %d, burst_mode: %d, cmd_timeout: %d us\n",
			(tcc_i2c->i2c_clk_rate/1000U),
			tcc_i2c->adap.retries,
			tcc_i2c->noise_filter,
			tcc_i2c->xfer_timeout,
			tcc_i2c->max_retry_count,
			tcc_i2c->burst_mode,
			tcc_i2c->cmd_clear_timeout);
		dev_info(tcc_i2c->dev,
			"[INFO][I2C] port %d, pulse-width-high: %d, pulse-width-low: %d\n",
			tcc_i2c->port_mux, tcc_i2c->pwh, tcc_i2c->pwl);
	}

	if (ret == 0) {
		if ((tcc_i2c->soc_info->id == TCC805X) &&
		    (tcc_i2c->core == tcc_i2c->soc_info->isp_i2c_ch)) {
			shared_i2c_base = tcc_i2c->base;
		}
	}

	return ret;
}

static int32_t tcc_i2c_remove(struct platform_device *pdev)
{
	struct tcc_i2c_struct *tcc_i2c = (struct tcc_i2c_struct *)platform_get_drvdata(pdev);

	i2c_del_adapter(&tcc_i2c->adap);

	/* Core disable */
	tcc_i2c_enable_core(tcc_i2c, (bool)false);

	/* I2C clock disable */
	tcc_i2c_disable_clock(tcc_i2c);

	platform_set_drvdata(pdev, NULL);
	device_remove_file(&pdev->dev, &dev_attr_tcci2c_v3);

	dev_info(tcc_i2c->dev,
		"[INFO][I2C] i2c controller[%d] is removed\n",
		tcc_i2c->core);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int32_t tcc_i2c_suspend(struct device *dev)
{
	struct tcc_i2c_struct *tcc_i2c = 
		(struct tcc_i2c_struct *)dev_get_drvdata(dev);

	i2c_mark_adapter_suspended(&tcc_i2c->adap);

	if (pinctrl_pm_select_sleep_state(dev) < 0) {
		dev_err(tcc_i2c->dev,
			"[ERROR][I2C] i2c-%d Failed to set pinctrl\n", tcc_i2c->core);
	}

	return 0;
}

static int32_t tcc_i2c_resume(struct device *dev)
{
	struct tcc_i2c_struct *tcc_i2c =
		(struct tcc_i2c_struct *)dev_get_drvdata(dev);
	int32_t ret;

	ret = pinctrl_pm_select_default_state(dev);
	if (ret == 0) {
		ret = tcc_i2c_config(tcc_i2c);
	} else {
		dev_err(tcc_i2c->dev,
			"[ERROR][I2C] i2c-%d Failed to set pinctrl\n", tcc_i2c->core);
	}

	if (ret == 0) {
		i2c_mark_adapter_resumed(&tcc_i2c->adap);

		dev_info(tcc_i2c->dev, "[INFO][I2C] i2c-%d configuration success\n", tcc_i2c->core);
	} else {
		dev_err(tcc_i2c->dev, "[ERROR][I2C] i2c-%d configuration failed\n", tcc_i2c->core);
	}

	return ret;
}

static const struct dev_pm_ops tcc_i2c_pm = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS((tcc_i2c_suspend), (tcc_i2c_resume))
};
#define TCC_I2C_PM (&tcc_i2c_pm)
#else
#define TCC_I2C_PM NULL
#endif

static ssize_t tcc_i2c_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	const struct tcc_i2c_struct *tcc_i2c =
		(struct tcc_i2c_struct *)dev_get_drvdata(dev);
	uint32_t port_value;
	bool is_busy;
	int32_t ret;

	(void)attr;
	port_value = tcc_i2c_get_pcfg(tcc_i2c, tcc_i2c->core);
	is_busy = ((readl(tcc_i2c->base + I2C_SR) & SR_BUSY) !=
			(u32)0U) ? true : false;

	ret = scnprintf(buf,
			100,
			"[DEBUG][I2C] channel %u, speed %u kHz, PORT: %u, STATE: %d, BUSY: %d\n",
			tcc_i2c->core,
			tcc_i2c->i2c_clk_rate / (u32)1000,
			port_value,
			tcc_i2c->state,
			is_busy);

	if (ret <= 0) {
		ret = 0;
	}

	return ret;
}

static const struct tcc_i2c_soc_info tcc805x_soc_info = {
	.id = TCC805X,
	.last_ch = 7U,
	.isp_i2c_ch = 7U,
};

static const struct tcc_i2c_soc_info tcc803x_soc_info = {
	.id = TCC803X,
	.last_ch = 7U,
	.isp_i2c_ch = 0xFFU,
};

static const struct tcc_i2c_soc_info tcc750x_soc_info = {
	.id = TCC750X,
	.last_ch = 3U,
	.isp_i2c_ch = 3U,
};

static const struct tcc_i2c_soc_info tcc807x_soc_info = {
	.id = TCC807X,
	.last_ch = 7U,
	.isp_i2c_ch = 7U,
	.cam_i2c_ch0 = 12U,
	.cam_i2c_ch1 = 13U,
};

static const struct tcc_i2c_soc_info tcn100x_soc_info = {
	.id = TCN100X,
	.last_ch = 4U,
	.isp_i2c_ch = 0xFFU,
};

static const struct of_device_id tcc_i2c_of_match[] = {
	{ .compatible = "telechips,tcc803x-i2c-v3",
			.data = &tcc803x_soc_info, },
	{ .compatible = "telechips,tcc805x-i2c-v3",
			.data = &tcc805x_soc_info, },
	{ .compatible = "telechips,tcc750x-i2c-v3",
			.data = &tcc750x_soc_info, },
	{ .compatible = "telechips,tcc807x-i2c-v3",
			.data = &tcc807x_soc_info, },
	{ .compatible = "telechips,tcn100x-i2c-v3",
			.data = &tcn100x_soc_info, },
	{},
};

MODULE_DEVICE_TABLE(of, tcc_i2c_of_match);

static struct platform_driver tcc_i2c_driver = {
	.probe                  = tcc_i2c_probe,
	.remove                 = tcc_i2c_remove,
	.driver                 = {
		.name           = "tcc-i2c-v3",
		.pm             = TCC_I2C_PM,
		.of_match_table = of_match_ptr(tcc_i2c_of_match),
	},
};

static int __init i2c_adap_tcc_init(void)
{
	return platform_driver_register(&tcc_i2c_driver);
}

static void __exit i2c_adap_tcc_exit(void)
{
	platform_driver_unregister(&tcc_i2c_driver);
}

subsys_initcall(i2c_adap_tcc_init);
module_exit(i2c_adap_tcc_exit);

MODULE_AUTHOR("Telechips Inc.");
MODULE_DESCRIPTION("Telechips H/W I2C driver");
MODULE_LICENSE("GPL");
