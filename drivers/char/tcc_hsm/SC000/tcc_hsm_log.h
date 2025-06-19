// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef T_LOG_H
#define T_LOG_H

#ifndef NDEBUG
#define DEBUG
#endif

#include <linux/printk.h>

#define TLOG_VBS (5)
#define TLOG_DEBUG (4)
#define TLOG_INFO (3)
#define TLOG_WARNING (2)
#define TLOG_ERROR (1)

#ifndef TLOG_LEVEL
#define TLOG_LEVEL TLOG_INFO
#endif

#ifndef TLOG_TAG
#define TLOG_TAG (__func__)
#endif

//#define COLOR_TAG	(1)

/* clang-format off */
#ifdef COLOR_TAG
#define GRAY_COLOR             "\033[1;30m"
#define NORMAL_COLOR           "\033[0m"
#define RED_COLOR              "\033[1;31m"
#define GREEN_COLOR            "\033[1;32m"
#define MAGENTA_COLOR          "\033[1;35m"
#define YELLOW_COLOR           "\033[1;33m"
#define BLUE_COLOR             "\033[1;34m"
#define DBLUE_COLOR            "\033[0;34m"
#define WHITE_COLOR            "\033[1;37m"
#define COLORERR_COLOR         "\033[1;37;41m"
#define COLORWRN_COLOR         "\033[0;31m"
#define BROWN_COLOR            "\033[0;40;33m"
#define CYAN_COLOR             "\033[0;40;36m"
#define LIGHTGRAY_COLOR        "\033[1;40;37m"
#define BRIGHTRED_COLOR        "\033[0;40;31m"
#define BRIGHTBLUE_COLOR       "\033[0;40;34m"
#define BRIGHTMAGENTA_COLOR    "\033[0;40;35m"
#define BRIGHTCYAN_COLOR       "\033[0;40;36m"
#else
#define GRAY_COLOR             ""
#define NORMAL_COLOR           ""
#define RED_COLOR              ""
#define GREEN_COLOR            ""
#define MAGENTA_COLOR          ""
#define YELLOW_COLOR           ""
#define BLUE_COLOR             ""
#define DBLUE_COLOR            ""
#define WHITE_COLOR            ""
#define COLORERR_COLOR         ""
#define COLORWRN_COLOR         ""
#define BROWN_COLOR            ""
#define CYAN_COLOR             ""
#define LIGHTGRAY_COLOR        ""
#define BRIGHTRED_COLOR        ""
#define BRIGHTBLUE_COLOR       ""
#define BRIGHTMAGENTA_COLOR    ""
#define BRIGHTCYAN_COLOR       ""
#endif


/* no logging */
#define TLOG_NDMSG(...)                       \
	do {                                  \
		if (0) {                      \
			pr_info(__VA_ARGS__); \
		}                             \
	} while (0)

/* Verbose logging */
#if (TLOG_LEVEL >= TLOG_VBS)
#define TLOG_VERBOSE_MSG(...) pr_notice(__VA_ARGS__)
#else
#define TLOG_VERBOSE_MSG(...) TLOG_NDMSG(__VA_ARGS__)
#endif

/* Debug logging */
#if (TLOG_LEVEL >= TLOG_DEBUG)
#define TLOG_DEBUG_MSG(...) pr_notice(__VA_ARGS__)
#else
#define TLOG_DEBUG_MSG(...) TLOG_NDMSG(__VA_ARGS__)
#endif

/* Informational logging */
#if (TLOG_LEVEL >= TLOG_INFO)
#define TLOG_INFO_MSG(...) pr_info(__VA_ARGS__)
#else
#define TLOG_INFO_MSG(...) TLOG_NDMSG(__VA_ARGS__)
#endif

/* Warning logging */
#if (TLOG_LEVEL >= TLOG_WARNING)
#define TLOG_WARN_MSG(...) pr_warn(__VA_ARGS__)
#else
#define TLOG_WARN_MSG(...) TLOG_NDMSG(__VA_ARGS__)
#endif

/* Error logging */
#if (TLOG_LEVEL >= TLOG_ERROR)
#define TLOG_ERROR_MSG(...) pr_err(__VA_ARGS__)
#else
#define TLOG_ERROR_MSG(...) TLOG_NDMSG(__VA_ARGS__)
#endif

/* Color tagging */
#define TRACE \
	TLOG_DEBUG_MSG(NORMAL_COLOR "[TRACE][%s][%d]\n", TLOG_TAG, __LINE__)

#define TLOG_VBSTAG(fmt, ...)                                               \
	TLOG_VERBOSE_MSG(                                                   \
		NORMAL_COLOR "[VERBOSE][%s][%d]" NORMAL_COLOR " " fmt "%s", \
		TLOG_TAG, __LINE__, __VA_ARGS__)

#define TLOG_DBGTAG(fmt, ...)                                            \
	TLOG_DEBUG_MSG(                                                  \
		GREEN_COLOR "[DEBUG][%s][%d]" NORMAL_COLOR " " fmt "%s", \
		TLOG_TAG, __LINE__, __VA_ARGS__)

#define TLOG_INFTAG(fmt, ...)                                            \
	TLOG_INFO_MSG(                                                   \
		YELLOW_COLOR "[INFO][%s][%d]" NORMAL_COLOR " " fmt "%s", \
		TLOG_TAG, __LINE__, __VA_ARGS__)

#define TLOG_WRNTAG(fmt, ...)                                              \
	TLOG_WARN_MSG(                                                     \
		COLORWRN_COLOR "[WARN][%s][%d]" NORMAL_COLOR " " fmt "%s", \
		TLOG_TAG, __LINE__, __VA_ARGS__)

#define TLOG_ERRTAG(fmt, ...)                                               \
	TLOG_ERROR_MSG(                                                     \
		COLORERR_COLOR "[ERROR][%s][%d]" NORMAL_COLOR " " fmt "%s", \
		TLOG_TAG, __LINE__, __VA_ARGS__)

#define VLOG(...) TLOG_VBSTAG(__VA_ARGS__, "")
#define DLOG(...) TLOG_DBGTAG(__VA_ARGS__, "")
#define ILOG(...) TLOG_INFTAG(__VA_ARGS__, "")
#define WLOG(...) TLOG_WRNTAG(__VA_ARGS__, "")
#define ELOG(...) TLOG_ERRTAG(__VA_ARGS__, "")
/* clang-format on */

#endif // T_LOG_H
