// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/rtc.h>
#include <linux/bcd.h>
#include <linux/clk.h>
#include <linux/log2.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include "tca_alarm.h"

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>

#define DRV_NAME        ((const char *)"tcc-rtc")

/* This option will set alarm 0sec ~ 59sec. Check it is working correctly. */
#if 0
#define RTC_PMWKUP_TEST
#endif

#ifdef RTC_PMWKUP_TEST
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/io.h>

struct task_struct *alaram_test_thread;
struct rtctime pTime_test;
static atomic_t irq_flag = ATOMIC_INIT(0);
#endif

#define rtc_readl       (__raw_readl)
#define rtc_writel      (__raw_writel)
#define rtc_reg(x)      (*(volatile u32 *)(tcc_rtc->regs + (x)))

struct tcc_rtc_data {
	void __iomem *regs;
	struct clk *hclk;
	s32 irq;
	u32 rtc_timeout;
	struct rtc_device *rtc_dev;
	spinlock_t lock;
};

static s32 tcc_rtc_stime_is_valid(const struct rtc_time *rtc_tm)
{
	s32 ret = -1;

	if (rtc_tm == NULL) {
		ret = -1;
	} else {
	        if ((rtc_tm->tm_sec < 0) || (rtc_tm->tm_sec > 59)) {
			ret = -1;
		} else if ((rtc_tm->tm_min < 0) || (rtc_tm->tm_min > 59)) {
			ret = -1;
		} else if ((rtc_tm->tm_hour < 0) || (rtc_tm->tm_hour > 23)) {
			ret = -1;
		} else if ((rtc_tm->tm_mday < 1) || (rtc_tm->tm_mday > 31)) {
			ret = -1;
		} else if ((rtc_tm->tm_wday < 0) || (rtc_tm->tm_wday > 6)) {
			ret = -1;
		} else if ((rtc_tm->tm_mon < 0) || (rtc_tm->tm_mon > 11)) {
			/* from Jan to Dec, wMonth = tm_mon + 1 */
			ret = -1;
		} else if ((rtc_tm->tm_year < 70) || (rtc_tm->tm_year > 8099)) {
			/* from 1970 to 9999, wYear = tm_year + 1900 */
			ret = -1;
		} else {
			ret = 0;
		}
	}

	return ret;
}

static s32 tcc_rtc_utime_is_valid(const struct rtctime *pTime)
{
	s32 ret;

	if (pTime == NULL) {
		ret = -1;
	} else {
	        if (pTime->wSecond > (u32)59) {
			ret = -1;
		} else if (pTime->wMinute > (u32)59) {
			ret = -1;
		} else if (pTime->wHour > (u32)23) {
			ret = -1;
		} else if ((pTime->wDay < (u32)1) || (pTime->wDay > (u32)31)) {
			ret = -1;
		} else if (pTime->wDayOfWeek > (u32)6) {
			ret = -1;
		} else if ((pTime->wMonth < (u32)1) || (pTime->wMonth > (u32)12)) {
			ret = -1;
		} else if ((pTime->wYear < (u32)1970) || (pTime->wYear > (u32)9999)) {
			ret = -1;
		} else {
			ret = 0;
		}
	}

	return ret;
}

/* IRQ Handlers */
static irqreturn_t tcc_rtc_alarmirq(s32 irq, void *class_dev)
{

	const struct tcc_rtc_data *tcc_rtc = class_dev;
	const struct device *dev = &tcc_rtc->rtc_dev->dev;
	irqreturn_t ret = IRQ_NONE;

	if (tcc_rtc->regs == NULL) {
		(void)dev_err(dev,
			      "[ERR][%s][%s] irq:%u, has no rtc reg\n", DRV_NAME, __func__, irq);
	} else {
		(void)dev_info(dev, "[INFO][%s][%s] irq:%u\n", DRV_NAME, __func__, irq);

		tca_rtc_unlock_reg(tcc_rtc->regs, 0, 0);

		/* Clear Interrupt Setting values */
		rtc_writel((u32)0, tcc_rtc->regs + RTCIM);
		/* Change Operation Mode from PowerDown Mode to Normal Operation Mode */
		rtc_writel(rtc_readl(tcc_rtc->regs + RTCIM) | Hw2, tcc_rtc->regs + RTCIM);
		/* PEND bit Clear - Clear RTC Wake-Up pin */
		rtc_writel((u32)0, tcc_rtc->regs + RTCPEND);
		/* RTC Alarm, wake-up interrupt pending clear */
		rtc_writel(rtc_readl(tcc_rtc->regs + RTCSTR) | Hw6 | Hw7, tcc_rtc->regs + RTCSTR);
		(void)dev_info(dev,
			       "[INFO][%s] RTCIM[%#x] RTCPEND[%#x] RTCSTR[%#x]\n",
			       DRV_NAME,
			       rtc_readl(tcc_rtc->regs + RTCIM),
			       rtc_readl(tcc_rtc->regs + RTCPEND),
			       rtc_readl(tcc_rtc->regs + RTCSTR));

		tca_rtc_lock_reg(tcc_rtc->regs, 0, 0);

		rtc_update_irq(tcc_rtc->rtc_dev, 1, (u32)RTC_AF | (u32)RTC_IRQF);

#ifdef RTC_PMWKUP_TEST
		(void)dev_info(dev,
			       "[INFO][%s] RTC TEST : %s ___________\n", DRV_NAME, __func__);
		atomic_set(&irq_flag, 0);
#endif
		ret = IRQ_HANDLED;
	}

	return ret;
}

#ifdef CONFIG_SYSFS
static ssize_t tcc_rtc_timeout_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	const struct tcc_rtc_data *tcc_rtc = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "rtc_timeout: %u seconds\n", tcc_rtc->rtc_timeout);
}

static DEFINE_MUTEX(rtc_timeout_mutex);


static ssize_t tcc_rtc_timeout_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct tcc_rtc_data *tcc_rtc = dev_get_drvdata(dev);
	u32 seconds;
	ssize_t ret = -1;

	if (count < (size_t)10) {
		ret = (ssize_t)kstrtouint(buf, 10, &seconds);
		if (ret == 0) {
			mutex_lock(&rtc_timeout_mutex);
			tcc_rtc->rtc_timeout = seconds;
			mutex_unlock(&rtc_timeout_mutex);
			ret = (ssize_t)count;
		}
	}

	return ret;
}
static DEVICE_ATTR_RW(tcc_rtc_timeout);
#endif

/* Update control registers */
static void tcc_rtc_setaie(const struct device *dev, u32 enable)
{
	const struct tcc_rtc_data *tcc_rtc = dev_get_drvdata(dev);
	u32 tmp;

	if ((tcc_rtc == NULL) || (tcc_rtc->regs == NULL)) {
		(void)dev_warn(dev, "[WARN][%s] Failed to get RTC structure\n", DRV_NAME);
	} else {
		tca_rtc_unlock_reg(tcc_rtc->regs, 0, 0);

		tmp = rtc_readl(tcc_rtc->regs + RTCALM) & ~Hw7;

		if (enable != (u32)0) {
			tmp |= Hw7;
		}

		rtc_writel(tmp, tcc_rtc->regs + RTCALM);

		tca_rtc_lock_reg(tcc_rtc->regs, 0, LEAVE_INTWREN);
	}

}

static s32 tcc_rtc_alarm_irq_enable(struct device *dev, u32 enabled)
{
	tcc_rtc_setaie(dev, enabled);

	return 0;
}

/* Time read/write */
static s32 tcc_rtc_gettime(struct device *dev, struct rtc_time *rtc_tm)
{
	struct tcc_rtc_data *tcc_rtc = dev_get_drvdata(dev);
	struct rtctime pTime;
	s32 ret = -1;
	unsigned long flags;

	if (rtc_tm == NULL) {
		(void)dev_err(dev,
			      "[ERR][%s][%s] Check for null ptr dereference.\n",
			      DRV_NAME, __func__);
		ret = -EINVAL;
	} else {
		spin_lock_irqsave(&tcc_rtc->lock, flags);

		tca_rtc_gettime(tcc_rtc->regs, &pTime);

		ret = tcc_rtc_utime_is_valid(&pTime);
		if (ret == 0) {
			rtc_tm->tm_sec = (s32)pTime.wSecond;
			rtc_tm->tm_min = (s32)pTime.wMinute;
			rtc_tm->tm_hour = (s32)pTime.wHour;
			rtc_tm->tm_mday = (s32)pTime.wDay;
			rtc_tm->tm_wday = (s32)pTime.wDayOfWeek;
			rtc_tm->tm_mon = (s32)pTime.wMonth - 1;
			rtc_tm->tm_year = (s32)pTime.wYear - 1900;
		} else {
			(void)memset(&pTime, 0, sizeof(struct rtctime));
		}

		spin_unlock_irqrestore(&tcc_rtc->lock, flags);

		(void)dev_info(dev,
			       "[INFO][%s] read time %04d/%02d/%02d %02d:%02d:%02d\n",
			       DRV_NAME,
			       pTime.wYear, pTime.wMonth, pTime.wDay,
			       pTime.wHour, pTime.wMinute, pTime.wSecond);
		ret = 0;
	}

	return ret;
}

static s32 tcc_rtc_settime(struct device *dev, struct rtc_time *rtc_tm)
{
	struct tcc_rtc_data *tcc_rtc = dev_get_drvdata(dev);
	struct rtctime pTime;
	s32 ret = -1;
	unsigned long flags;

	if (rtc_tm == NULL) {
		(void)dev_err(dev,
			      "[ERR][%s][%s] Check for null ptr dereference.\n",
			      DRV_NAME, __func__);
		ret = -EINVAL;
	} else {
		spin_lock_irqsave(&tcc_rtc->lock, flags);

		ret = tcc_rtc_stime_is_valid(rtc_tm);
		if (ret == 0) {
			pTime.wSecond = (u32)rtc_tm->tm_sec;
			pTime.wMinute = (u32)rtc_tm->tm_min;
			pTime.wHour = (u32)rtc_tm->tm_hour;
			pTime.wDay = (u32)rtc_tm->tm_mday;
			pTime.wDayOfWeek = (u32)rtc_tm->tm_wday;
			pTime.wMonth = (u32)(rtc_tm->tm_mon + 1);
			pTime.wYear = (u32)(rtc_tm->tm_year + 1900);
		} else {
			(void)memset(&pTime, 0, sizeof(struct rtctime));
		}

		tca_rtc_settime(tcc_rtc->regs, &pTime);

		spin_unlock_irqrestore(&tcc_rtc->lock, flags);

		(void)dev_info(dev,
			       "[INFO][%s] set time %02d/%02d/%02d %02d:%02d:%02d\n",
			       DRV_NAME,
			       pTime.wYear, pTime.wMonth, pTime.wDay,
			       pTime.wHour, pTime.wMinute, pTime.wSecond);
		ret = 0;
	}

	return ret;
}

static s32 tcc_rtc_getalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct tcc_rtc_data *tcc_rtc = dev_get_drvdata(dev);
	struct rtc_time *alm_tm;
	u32 alm_en;
	u32 alm_pnd;
	s32 ret = -1;
	struct rtctime pTime;
	unsigned long flags;

	if ((alrm == NULL) || (tcc_rtc == NULL) || (tcc_rtc->regs == NULL)) {
		(void)dev_err(dev,
			      "[ERR][%s][%s] Check for null ptr dereference.\n",
			      DRV_NAME, __func__);
		ret = -EINVAL;
	} else {
		alm_tm = &alrm->time;

		spin_lock_irqsave(&tcc_rtc->lock, flags);

		tca_rtc_unlock_reg(tcc_rtc->regs, 0, 0);

		alm_en = rtc_readl(tcc_rtc->regs + RTCALM);
		alm_pnd = rtc_readl(tcc_rtc->regs + RTCPEND);

		alrm->enabled = ((alm_en & Hw7) != (u32)0) ? (u8)1 : (u8)0;
		alrm->pending = ((alm_pnd & Hw0) != (u32)0) ? (u8)1 : (u8)0;

		(void)dev_info(dev,
			       "[INFO][%s] alrm->enabled = %d, alm_en = %d\n",
			       DRV_NAME, alrm->enabled, alm_en);

		tca_rtc_lock_reg(tcc_rtc->regs, 0, 0);

		tca_alarm_gettime(tcc_rtc->regs, &pTime);

		ret = tcc_rtc_utime_is_valid(&pTime);
		if ((ret == 0) && (alm_tm != NULL)) {
			alm_tm->tm_sec = (s32)pTime.wSecond;
			alm_tm->tm_min = (s32)pTime.wMinute;
			alm_tm->tm_hour = (s32)pTime.wHour;
			alm_tm->tm_mday = (s32)pTime.wDay;
			alm_tm->tm_mon = (s32)pTime.wMonth - 1;
			alm_tm->tm_year = (s32)pTime.wYear - 1900;
		} else {
			(void)memset(&pTime, 0, sizeof(struct rtctime));
		}

		spin_unlock_irqrestore(&tcc_rtc->lock, flags);

		(void)dev_info(dev,
			       "[INFO][%s] read alarm %02x %02x/%02x/%02x %02x:%02x:%02x\n",
			       DRV_NAME, alm_en,
			       pTime.wYear, pTime.wMonth, pTime.wDay,
			       pTime.wHour, pTime.wMinute, pTime.wSecond);
		ret = 0;
	}

	return ret;
}

#ifdef RTC_PMWKUP_TEST
static struct rtc_time rtctime_to_rtc_time(struct rtctime pTime)
{
	struct rtc_time rtc_tm;
	s32 ret = -1;

	ret = tcc_rtc_utime_is_valid(&pTime);
	if (ret == 0) {
		rtc_tm.tm_sec = (s32)pTime.wSecond;
		rtc_tm.tm_min = (s32)pTime.wMinute;
		rtc_tm.tm_hour = (s32)pTime.wHour;
		rtc_tm.tm_mday = (s32)pTime.wDay;
		rtc_tm.tm_wday = (s32)pTime.wDayOfWeek;
		rtc_tm.tm_mon = (s32)pTime.wMonth - 1;
		rtc_tm.tm_year = (s32)pTime.wYear - 1900;
	}

	return rtc_tm;
}

static struct rtctime rtc_time_to_rtctime(struct rtc_time rtc_tm)
{
	struct rtctime pTime;
	s32 ret = -1;

	ret = tcc_rtc_stime_is_valid(&rtc_tm);
	if (ret == 0) {
		pTime.wSecond = (u32)rtc_tm.tm_sec;
		pTime.wMinute = (u32)rtc_tm.tm_min;
		pTime.wHour = (u32)rtc_tm.tm_hour;
		pTime.wDay = (u32)rtc_tm.tm_mday;
		pTime.wDayOfWeek = (u32)rtc_tm.tm_wday;
		pTime.wMonth = (u32)rtc_tm.tm_mon + 1;
		pTime.wYear = (u32)rtc_tm.tm_year + 1900;
	}

	return pTime;
}

static s32 check_time(struct rtctime now_time, struct rtctime new_time)
{
	struct rtc_time now_tm, new_tm;
	ktime_t now_kt, new_kt;
	s32 ret = -1;

	now_tm = rtctime_to_rtc_time(now_time);
	new_tm = rtctime_to_rtc_time(new_time);

	now_kt = rtc_tm_to_ktime(now_tm);
	new_kt = rtc_tm_to_ktime(new_tm);

	if (now_kt == new_kt)
		ret = 0;	/* now_time and new_time is same. */
#if 1
	/* if 1sec is different, but this function regard it same. */
	if ((now_kt + NSEC_PER_SEC) == new_kt)
		ret = 0;	/* now_time + 1sec and new_time is same. */

	if ((now_kt - NSEC_PER_SEC) == new_kt)
		ret = 0;	/* now_time - 1sec and new_time is same. */
#endif

	return ret;
}
#endif

static s32 tcc_rtc_setalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct tcc_rtc_data *tcc_rtc = dev_get_drvdata(dev);
	struct rtctime pTime;
	struct rtc_time *rtc_tm;
	s32 ret = -1;
	unsigned long flags;

	if ((alrm == NULL) || (tcc_rtc == NULL) || (tcc_rtc->regs == NULL)) {
		(void)dev_err(dev,
			      "[ERR][%s][%s] Check for null ptr dereference.\n",
			      DRV_NAME, __func__);
		ret = -EINVAL;
	} else {
		rtc_tm = &alrm->time;

		alrm->enabled = 1;

		ret = tcc_rtc_stime_is_valid(rtc_tm);
		if (ret == 0) {
			pTime.wSecond = (u32)rtc_tm->tm_sec;
			pTime.wMinute = (u32)rtc_tm->tm_min;
			pTime.wHour = (u32)rtc_tm->tm_hour;
			pTime.wDay = (u32)rtc_tm->tm_mday;
			pTime.wDayOfWeek = (u32)rtc_tm->tm_wday;
			pTime.wMonth = (u32)(rtc_tm->tm_mon + 1);
			pTime.wYear = (u32)(rtc_tm->tm_year + 1900);
		} else {
			(void)memset(&pTime, 0, sizeof(struct rtctime));
		}

		(void)dev_info(dev,
			       "[INFO][%s] set alarm %02d/%02d/%02d %02d:%02d:%02d\n",
			       DRV_NAME,
			       pTime.wYear, pTime.wMonth, pTime.wDay,
			       pTime.wHour, pTime.wMinute, pTime.wSecond);

		spin_lock_irqsave(&tcc_rtc->lock, flags);

		tcc_rtc_setaie(dev, 0);

		tca_alarm_settime(tcc_rtc->regs, &pTime);

		tca_rtc_unlock_reg(tcc_rtc->regs, 0, 0);

		if (tcc_rtc->irq > 0) {
			ret = enable_irq_wake((u32)tcc_rtc->irq);
			if (ret < 0) {
				(void)dev_warn(dev, "[WARN][%s] Failed set irq wake\n", DRV_NAME);
			}

		}

		tca_rtc_lock_reg(tcc_rtc->regs, 0, 0);

		tcc_rtc_setaie(dev, 1);

		spin_unlock_irqrestore(&tcc_rtc->lock, flags);

		ret = 0;
	}

	return ret;
}

static s32 tcc_rtc_proc(struct device *dev, struct seq_file *seq)
{
	return 0;
}

static s32 tcc_rtc_ioctl(struct device *dev,
			 u32 cmd, unsigned long arg)
{
	s32 ret = -ENOIOCTLCMD;

	switch (cmd) {
	case RTC_AIE_OFF:
		tcc_rtc_setaie(dev, 0);
		ret = 0;
		break;
	case RTC_AIE_ON:
		tcc_rtc_setaie(dev, 1);
		ret = 0;
		break;
	case RTC_PIE_OFF:
		break;
	case RTC_PIE_ON:
		break;
	case RTC_IRQP_READ:
		break;
	case RTC_IRQP_SET:
		break;
	case RTC_UIE_ON:
		break;
	case RTC_UIE_OFF:
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct rtc_class_ops tcc_rtcops = {
	.read_time = &tcc_rtc_gettime,
	.set_time = &tcc_rtc_settime,
	.read_alarm = &tcc_rtc_getalarm,
	.set_alarm = &tcc_rtc_setalarm,
	.alarm_irq_enable = &tcc_rtc_alarm_irq_enable,
	.proc = &tcc_rtc_proc,
	.ioctl = &tcc_rtc_ioctl,
};

static s32 tcc_rtc_remove(struct platform_device *pdev)
{
	struct tcc_rtc_data *tcc_rtc = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	tcc_rtc_setaie(&pdev->dev, 0);
	if (tcc_rtc->irq > 0) {
		(void)devm_free_irq(&pdev->dev, (u32)tcc_rtc->irq, tcc_rtc);
	}

	if (tcc_rtc->hclk != NULL) {
		clk_disable_unprepare(tcc_rtc->hclk);
		clk_put(tcc_rtc->hclk);
		tcc_rtc->hclk = NULL;
	}

	return 0;
}

#ifdef RTC_PMWKUP_TEST
static s32 cmp_rtc_gettime(struct device *dev)
{
	struct rtctime pTime;
	struct rtc_time now_time;

	(void)tcc_rtc_gettime(dev, &now_time);

	pTime = rtc_time_to_rtctime(now_time);

	(void)dev_info(dev,
		       "[INFO][%s] RTC TEST: now irq time %04d/%02d/%02d %02d:%02d:%02d\n",
		       DRV_NAME,
		       pTime.wYear, pTime.wMonth, pTime.wDay,
		       pTime.wHour, pTime.wMinute, pTime.wSecond);

	if (check_time(pTime, pTime_test)) {
		(void)dev_info(dev,
			       "[INFO][%s] RTC TEST: __________ alarm un-matched time.\n",
			       DRV_NAME);
	}

	return 0;
}

static s32 get_next_sec(s32 set_sec)
{
	s32 out;
#if 0
	out = set_sec + 1;
#else
	s32 interval = 10;

	out = set_sec + interval;
	if (out == 60) {
		out = 1;
	} else if (out > 60) {
		out = out % 10;
		out++;

		if (out >= 10)
			out = 60;
	}
#endif

	return out;
}

static s32 tcc_alarm_test(void *arg)
{
	struct device *dev = (struct device *)arg;

	struct rtc_time rtm;
	struct rtc_time *rtc_tm = &rtm;
	struct rtctime pTime;

	s32 ret = -1;
	s32 set_sec = 0;

	(void)dev_info(dev, "[INFO][%s] RTC TEST :  __________ %s START\n", DRV_NAME, __func__);
	msleep(30000);

	do {
		struct rtc_wkalrm rtc_al;

		if (set_sec >= 60)
			break;

		(void)dev_info(dev, "[INFO][%s] RTC TEST : Set Alarm - sec[%d]\n",
			       DRV_NAME, set_sec);
		(void)tcc_rtc_gettime(dev, rtc_tm);

		/* -2 is delay time to set alarm. */
		if (rtc_tm->tm_sec >= (set_sec - 2)) {
			rtc_tm->tm_min += 1;
		}
		rtc_tm->tm_sec = set_sec;

		if (rtc_tm->tm_min >= 60) {
			rtc_tm->tm_min %= 60;
			rtc_tm->tm_hour++;
		}

		if (rtc_tm->tm_hour >= 24) {
			rtc_tm->tm_hour %= 24;
			rtc_tm->tm_mday++;
		}

		rtc_al.time = *rtc_tm;
		tcc_rtc_setalarm(dev, &rtc_al);
		atomic_set(&irq_flag, 1);

		ret = tcc_rtc_stime_is_valid(rtc_tm);
		if (ret == 0) {
			pTime.wSecond = (u32)rtc_tm->tm_sec;
			pTime.wMinute = (u32)rtc_tm->tm_min;
			pTime.wHour = (u32)rtc_tm->tm_hour;
			pTime.wDay = (u32)rtc_tm->tm_mday;
			pTime.wMonth = (u32)(rtc_tm->tm_mon + 1);
			pTime.wYear = (u32)(rtc_tm->tm_year + 1900);
		} else {
			(void)memset(&pTime, 0, sizeof(struct rtctime));
		}

		(void)dev_info(dev,
			       "[INFO][%s] RTC TEST : set alarm %02d/%02d/%02d %02d:%02d:%02d\n",
			       DRV_NAME,
			       pTime.wYear, pTime.wMonth, pTime.wDay,
			       pTime.wHour, pTime.wMinute, pTime.wSecond);

		memcpy(&pTime_test, &pTime, sizeof(struct rtctime));

		set_sec = get_next_sec(set_sec);

		while (atomic_read(&irq_flag)) {
			msleep(400);
		};	/* Wait until interrupt is occured. */

		cmp_rtc_gettime(dev);
	} while (1);

	(void)dev_info(dev, "[INFO][%s] RTC TEST : Alarm Test is finished.\n", DRV_NAME);

	return 0;
}
#endif /* RTC_PMWKUP_TEST */

static void tcc_rtc_setclock(const struct tcc_rtc_data *tcc_rtc,
			     u32 rtc_clock)
{
	tca_rtc_unlock_reg(tcc_rtc->regs, 1, 1);

	/* INTCON[13:12] XDRV, set clock source for rtc */
	rtc_writel(rtc_readl(tcc_rtc->regs + INTCON) | ((rtc_clock & (u32)0x3) << (u32)12),
		   tcc_rtc->regs + INTCON);

	tca_rtc_lock_reg(tcc_rtc->regs, 1, WITH_PROT);
}

static s32 tcc_rtc_probe(struct platform_device *pdev)
{
	struct tcc_rtc_data *tcc_rtc;
	s32 ret = -1;
	u32 rtc_clock;
	bool err;

	if (pdev == NULL) {
		ret = -EINVAL;
		goto out;
	}
	tcc_rtc = devm_kzalloc(&pdev->dev, sizeof(struct tcc_rtc_data), GFP_KERNEL);

	if (tcc_rtc == NULL) {
		(void)dev_err(&pdev->dev,
			      "[ERROR][%s] failed to allocate memory\n",
			      DRV_NAME);
		ret = -ENOMEM;
		goto out;
	}
	platform_set_drvdata(pdev, tcc_rtc);

	spin_lock_init(&tcc_rtc->lock);

	tcc_rtc->regs = of_iomap(pdev->dev.of_node, 0);
	if (tcc_rtc->regs == NULL) {
		(void)dev_err(&pdev->dev,
			      "[ERROR][%s] failed RTC of_iomap()\n", DRV_NAME);
		ret = -ENOMEM;
		goto err_get_dt_property;
	}

	tcc_rtc->irq = platform_get_irq(pdev, 0);
	if (tcc_rtc->irq <= 0) {
		(void)dev_err(&pdev->dev,
			      "[ERROR][%s] no irq for alarm\n", DRV_NAME);
		ret = -ENOENT;
		goto err_get_dt_property;
	}

	tcc_rtc->hclk = of_clk_get(pdev->dev.of_node, 0);
	err = IS_ERR(tcc_rtc->hclk);
	if (err) {
		(void)dev_err(&pdev->dev,
			      "[ERROR][%s] failed to get hclk\n", DRV_NAME);
		ret = -ENXIO;
		goto err_get_hclk;
	}

	ret = clk_prepare_enable(tcc_rtc->hclk);
	if (ret != 0) {
		(void)dev_err(&pdev->dev,
			      "[ERROR][%s] clk_enable failed\n", DRV_NAME);
		ret = -ENXIO;
		goto err_get_hclk;
	}

	tca_rtc_unlock_reg(tcc_rtc->regs, 1, WITH_PROT);

	/* 32.768kHz XTAL 1.8V - XTAL I/O Driver Strength Select */
	rtc_writel(rtc_readl(tcc_rtc->regs + INTCON) & ~(Hw13 | Hw12), tcc_rtc->regs + INTCON);
	/* 32.768kHz XTAL - Divider Output Select - FSEL */
	rtc_writel(rtc_readl(tcc_rtc->regs + INTCON) & ~(Hw10 | Hw9 | Hw8), tcc_rtc->regs + INTCON);

	/*
	 * Start : Disable the RTC Alarm - 120125, hjbae
	 * Disable Wake Up Interrupt Output(Hw7) and
	 * Alarm Interrupt Output(Hw6)
	 */
	/* Disable RTC Wake-Up, Alarm Interrupt */
	rtc_writel(rtc_readl(tcc_rtc->regs + RTCCON) & ~(Hw7 | Hw6), tcc_rtc->regs + RTCCON);

	/* Disable - Alarm Control */
	/* Clear RTCALM reg - Disable Alarm */
	rtc_writel((u32)0, tcc_rtc->regs + RTCALM);

	/* Power down mode, Active HIGH, Disable alarm interrupt */
	/* ActiveHigh */
	rtc_writel(rtc_readl(tcc_rtc->regs + RTCIM) | Hw2, tcc_rtc->regs + RTCIM);
	/* PowerDown Mode, Disable Interrupt */
	rtc_writel(rtc_readl(tcc_rtc->regs + RTCIM) & ~(Hw3 | Hw1 | Hw0), tcc_rtc->regs + RTCIM);

	tca_rtc_lock_reg(tcc_rtc->regs, 1, WITH_PROT);

	ret = of_property_read_u32(pdev->dev.of_node, "rtc_clock", &rtc_clock);
	if (ret == 0) {
		tcc_rtc_setclock(tcc_rtc, rtc_clock);
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				   "rtc_timeout", &tcc_rtc->rtc_timeout);
	if (ret != 0) {
		(void)dev_warn(&pdev->dev,
			       "[WARN][%s] Failted to get rtc_timeout.\n",
			       DRV_NAME);
		tcc_rtc->rtc_timeout = (u32)0;
	}

	/* Check Valid Time */
	ret = tca_rtc_checkvalidtime(tcc_rtc->regs);
	if (ret != 0) {
		/* Invalied Time */
		struct rtctime pTime;

		/* temp init */
		pTime.wDay = 27;
		pTime.wMonth = 3;
		pTime.wDayOfWeek = 5;
		pTime.wHour = 9;
		pTime.wMinute = 30;
		pTime.wSecond = 0;
		pTime.wYear = 2014;

		tca_rtc_settime(tcc_rtc->regs, &pTime);

		(void)dev_info(&pdev->dev,
			       "[INFO][%s] RTC Invalied Time, Set Time %04d/%02d/%02d %02d:%02d:%02d\n",
			       DRV_NAME,
			       pTime.wYear, pTime.wMonth, pTime.wDay,
			       pTime.wHour, pTime.wMinute, pTime.wSecond);
	}

	(void)dev_info(&pdev->dev, "[INFO][%s] tcc_rtc: alarm irq %d\n", DRV_NAME, tcc_rtc->irq);

	/*
	 *  From kernel 3.4 version,
	 *  the following function has to be used to wake device
	 *  from suspend mode.
	 */
	ret = device_init_wakeup(&pdev->dev, (bool)true);
	if (ret != 0) {
		(void)dev_warn(&pdev->dev,
			      "[WARNING][%s] device_init_wakeup has failed.\n", DRV_NAME);
	}

	/* register RTC and exit */
	tcc_rtc->rtc_dev = devm_rtc_allocate_device(&pdev->dev);
	err = IS_ERR(tcc_rtc->rtc_dev);
	if (err) {
		(void)dev_err(&pdev->dev,
			      "[ERROR][%s] cannot attach rtc\n", DRV_NAME);
		ret = -ENXIO;
		goto err_nortc;
	}
	tcc_rtc->rtc_dev->ops = &tcc_rtcops;

	ret = rtc_register_device(tcc_rtc->rtc_dev);
	if (ret != 0) {
		goto err_nortc;
	}

#ifdef CONFIG_SYSFS
	ret = device_create_file(&pdev->dev, &dev_attr_tcc_rtc_timeout);
	if (ret != 0) {
		(void)dev_warn(&pdev->dev,
			       "[ERROR][%s] cannot create sysfs\n", DRV_NAME);
	}
#endif

	/*
	 * Use threaded IRQ, remove IRQ enable in interrupt handler
	 *   - 120126, hjbae
	 */
	ret = devm_request_irq(&pdev->dev, (u32)tcc_rtc->irq, &tcc_rtc_alarmirq,
			       0, DRV_NAME, tcc_rtc);
	if (ret != 0) {
		(void)dev_err(&pdev->dev,
			      "[ERROR][%s] %s: RTC timer interrupt IRQ%d already claimed\n",
			      DRV_NAME, pdev->name, tcc_rtc->irq);
		goto err_nortc;
	}
#ifdef RTC_PMWKUP_TEST
	alaram_test_thread = (struct task_struct *)kthread_run(tcc_alarm_test,
							       (void *)&pdev->dev,
							       "alarm_test");
#endif
	ret = 0;
	goto out;

err_nortc:
	clk_disable_unprepare(tcc_rtc->hclk);
	clk_put(tcc_rtc->hclk);

err_get_hclk:

err_get_dt_property:
	devm_kfree(&pdev->dev, tcc_rtc);

out:
	return ret;
}

static void tcc_rtc_set_timeout(struct device *dev)
{
	const struct tcc_rtc_data *tcc_rtc = dev_get_drvdata(dev);
	struct rtc_wkalrm alrm;
	time64_t now;
	time64_t timeout;
	s32 res;

	(void)tcc_rtc_gettime(dev, &alrm.time);
	now = rtc_tm_to_time64(&alrm.time);

	/* set alarm time based on now */
	if ((LLONG_MAX - now) < tcc_rtc->rtc_timeout) {
		(void)dev_warn(dev,
			       "[WARN][%s] Failted to set timeout.\n", DRV_NAME);
		/* TODO: Check ULONG_MAX */
	} else {
		timeout = now + tcc_rtc->rtc_timeout;
		rtc_time64_to_tm(timeout, &alrm.time);
		res = tcc_rtc_setalarm(dev, &alrm);
		if (res != 0) {
			(void)dev_warn(dev,
				       "[WARN][%s] Failted to setalarm.\n",
				       DRV_NAME);
		}
	}
}

#ifdef CONFIG_PM

/* RTC Power management control */
static s32 tcc_rtc_suspend(struct device *dev)
{
	const struct tcc_rtc_data *tcc_rtc = dev_get_drvdata(dev);

	if (tcc_rtc->rtc_timeout > (u32)0) {
		tcc_rtc_set_timeout(dev);
	}

	tca_rtc_unlock_reg(tcc_rtc->regs, 0, 0);

	/* set PWDN - Power Down Mode */
	rtc_writel(rtc_readl(tcc_rtc->regs + RTCIM) | Hw3, tcc_rtc->regs + RTCIM);

	tca_rtc_lock_reg(tcc_rtc->regs, 0, 0);

	return 0;
}

static s32 tcc_rtc_resume(struct device *dev)
{
	const struct tcc_rtc_data *tcc_rtc = dev_get_drvdata(dev);

	tca_rtc_unlock_reg(tcc_rtc->regs, 0, 0);

	/* set PWDN - Normal Operation */
	rtc_writel(rtc_readl(tcc_rtc->regs + RTCIM) & ~Hw3, tcc_rtc->regs + RTCIM);

	tca_rtc_lock_reg(tcc_rtc->regs, 0, 0);

	return 0;
}

static s32 tcc_rtc_restore(struct device *dev)
{
	s32 ret = -1;
	const struct tcc_rtc_data *tcc_rtc = dev_get_drvdata(dev);

	if ((tcc_rtc == NULL) || (tcc_rtc->regs == NULL)) {
		(void)dev_err(dev,
			      "[ERR][%s][%s] Check for null ptr dereference.\n",
			      DRV_NAME, __func__);
		ret = -EINVAL;
	} else {
		/* Check Valid Time */
		ret = tca_rtc_checkvalidtime(tcc_rtc->regs);
		if (ret != 0) {
			/* Invalied Time */
			struct rtctime pTime;

			/* temp init */
			pTime.wDay = 27;
			pTime.wMonth = 3;
			pTime.wDayOfWeek = 5;
			pTime.wHour = 9;
			pTime.wMinute = 30;
			pTime.wSecond = 0;
			pTime.wYear = 2014;

			tca_rtc_settime(tcc_rtc->regs, &pTime);

			(void)dev_info(dev,
				       "[INFO][%s] RTC Invalied Time, Set Time %04d/%02d/%02d %02d:%02d:%02d\n",
				       DRV_NAME,
				       pTime.wYear, pTime.wMonth, pTime.wDay,
				       pTime.wHour, pTime.wMinute, pTime.wSecond);
		}

		tca_rtc_unlock_reg(tcc_rtc->regs, 0, 0);

		/* set PWDN - Normal Operation */
		rtc_writel(rtc_readl(tcc_rtc->regs + RTCIM) & ~Hw3, tcc_rtc->regs + RTCIM);

		tca_rtc_lock_reg(tcc_rtc->regs, 0, 0);

		ret = 0;
	}

	return ret;
}
#else
#define tcc_rtc_suspend	(NULL)
#define tcc_rtc_resume  (NULL)
#endif

#ifdef CONFIG_PM
static const struct dev_pm_ops tcc_rtc_pm_ops = {
	.suspend = &tcc_rtc_suspend,
	.resume = &tcc_rtc_resume,
	.freeze = &tcc_rtc_suspend,
	.thaw = &tcc_rtc_resume,
	.restore = &tcc_rtc_restore,
};
#endif

static const struct of_device_id tcc_rtc_of_match[] = {
	{.compatible = "telechips,rtc",},
	{}
};

static struct platform_driver tcc_rtc_driver = {
	.probe = &tcc_rtc_probe,
	.remove = &tcc_rtc_remove,
#ifndef CONFIG_PM
	.suspend = &tcc_rtc_suspend,
	.resume = &tcc_rtc_resume,
#endif
	.driver = {
		   .name = DRV_NAME,
		   .owner = THIS_MODULE,
		   .pm = &tcc_rtc_pm_ops,
		   .of_match_table = of_match_ptr(tcc_rtc_of_match),
		   },
};

module_platform_driver(tcc_rtc_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Telchips.com, Inc.");
MODULE_DESCRIPTION("RTC driver");
