// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/io.h>
#include "tca_rtc.h"

#define rtc_readl       (__raw_readl)
#define rtc_writel      (__raw_writel)

void tca_rtc_unlock_reg(void __iomem *pRTC, s32 halt, s32 prot_unlock)
{
	/* RTC Register Write Enable */
	rtc_writel(rtc_readl(pRTC + RTCCON) | Hw1, pRTC + RTCCON);

	if (halt != 0) {
		/* RTC start bit - Halt */
		rtc_writel(rtc_readl(pRTC + RTCCON) | Hw0, pRTC + RTCCON);
	}

	/* Interrupt Block Write Enable */
	rtc_writel(rtc_readl(pRTC + INTCON) | Hw0, pRTC + INTCON);

	if (prot_unlock != 0) {
		/* Interrupt Block Protection Disable */
		rtc_writel(rtc_readl(pRTC + INTCON) & ~Hw15, pRTC + INTCON);
	}
}

void tca_rtc_lock_reg(void __iomem *pRTC, s32 run, s32 prot_lock)
{
	if (prot_lock != 0) {
		/* Interrupt Block Protection Enable */
		rtc_writel(rtc_readl(pRTC + INTCON) | Hw15, pRTC + INTCON);
	}

	if (prot_lock != LEAVE_INTWREN) {
		/* Interrupt Block Write Disable */
		rtc_writel(rtc_readl(pRTC + INTCON) & ~Hw0, pRTC + INTCON);
	}

	if (run != 0) {
		/* RTC start bit - Run */
		rtc_writel(rtc_readl(pRTC + RTCCON) & ~Hw0, pRTC + RTCCON);
	}

	/* RTC Register write Disable */
	rtc_writel(rtc_readl(pRTC + RTCCON) & ~Hw1, pRTC + RTCCON);
}

void tca_rtc_gettime(void __iomem *pRTC, struct rtctime *pTime)
{
	u32 data;
	u32 seconds;

	if (pTime != NULL) {
		/* Enable RTC control */
		tca_rtc_unlock_reg(pRTC, 0, 0);
		rtc_writel(rtc_readl(pRTC + RTCCON) & ~Hw4, pRTC + RTCCON);

		do {
			data = rtc_readl(pRTC + BCDSEC) & (u32)0x7f;
			seconds = FROM_BCD(data);

			data = rtc_readl(pRTC + BCDYEAR) & (u32)0xFFFF;
			pTime->wYear = (FROM_BCD(data >> (u32)8) * (u32)100)
					+ FROM_BCD(data & (u32)0xFF);

			data = rtc_readl(pRTC + BCDMON) & (u32)0x1f;
			pTime->wMonth = FROM_BCD(data);

			data = rtc_readl(pRTC + BCDDATE) & (u32)0x3f;
			pTime->wDay = FROM_BCD(data);

			data = rtc_readl(pRTC + BCDDAY) & (u32)0x07;
			pTime->wDayOfWeek = FROM_BCD(data);

			data = rtc_readl(pRTC + BCDHOUR) & (u32)0x3f;
			pTime->wHour = FROM_BCD(data);

			data = rtc_readl(pRTC + BCDMIN) & (u32)0x7f;
			pTime->wMinute = FROM_BCD(data);

			data = rtc_readl(pRTC + BCDSEC) & (u32)0x7f;
			pTime->wSecond = FROM_BCD(data);

			pTime->wMilliseconds = 0;
		} while (pTime->wSecond != seconds);

		tca_rtc_lock_reg(pRTC, 0, 0);
	}
}

void tca_rtc_settime(void __iomem *pRTC, const struct rtctime *pTime)
{
	if (pTime != NULL) {
		tca_rtc_unlock_reg(pRTC, 1, 1);

		/* RTC Clock Count Reset */
		rtc_writel(rtc_readl(pRTC + RTCCON) | Hw4, pRTC + RTCCON);
		/* RTC Clock Count No Reset */
		rtc_writel(rtc_readl(pRTC + RTCCON) & ~Hw4, pRTC + RTCCON);
		/* RTC Clock Count Reset */
		rtc_writel(rtc_readl(pRTC + RTCCON) | Hw4, pRTC + RTCCON);

		rtc_writel(TO_BCD(pTime->wSecond), pRTC + BCDSEC);
		rtc_writel(TO_BCD(pTime->wMinute), pRTC + BCDMIN);
		rtc_writel(TO_BCD(pTime->wHour), pRTC + BCDHOUR);
		rtc_writel(TO_BCD(pTime->wDayOfWeek), pRTC + BCDDAY);
		rtc_writel(TO_BCD(pTime->wDay), pRTC + BCDDATE);
		rtc_writel(TO_BCD(pTime->wMonth), pRTC + BCDMON);
		rtc_writel((TO_BCD(pTime->wYear / (u32)100) << (u32)8)
			   | TO_BCD(pTime->wYear % (u32)100),
			   pRTC + BCDYEAR);	/* YearFix : hjbae */

		tca_rtc_lock_reg(pRTC, 1, 1);
	}
}

s32 tca_rtc_checkvalidtime(void __iomem *devbaseaddresss)
{
	s32 IsNeedMending;
	struct rtctime pTimeTest;

	tca_rtc_gettime(devbaseaddresss, &pTimeTest);

	/*
	 * Conversion to Dec and Check validity
	 */
	IsNeedMending = 0;

	/* Second */
	if (pTimeTest.wSecond > (u32)59) {
		IsNeedMending = 1;
	}

	/* Minute */
	if (pTimeTest.wMinute > (u32)59) {
		IsNeedMending = 1;
	}

	/* Hour */
	if (pTimeTest.wHour > (u32)23) {
		IsNeedMending = 1;
	}

	/* Date, 1~31 */
	if ((pTimeTest.wDay < (u32)1) || (pTimeTest.wDay > (u32)31)) {
		IsNeedMending = 1;
	}

	/* Day, Sunday ~ Saturday */
	if (pTimeTest.wDayOfWeek > (u32)6) {
		IsNeedMending = 1;
	}

	/* Month */
	if ((pTimeTest.wMonth < (u32)1) || (pTimeTest.wMonth > (u32)12)) {
		IsNeedMending = 1;
	}

	/* Year */
	if ((pTimeTest.wYear < (u32)1980) || (pTimeTest.wYear > (u32)9999)) {
		IsNeedMending = 1;
	}

	return IsNeedMending;
}
