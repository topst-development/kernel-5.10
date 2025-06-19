// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/vmalloc.h>
#include <linux/utsname.h>
#include <linux/crc32.h>
#include <linux/firmware.h>
#include <linux/devcoredump.h>

#include "vpu_dbg.h"


/**
 * enum vpu_fw_dump_type - types of data in the dump file
 * @VPU_FW_DUMP_REGDUMP: Register dump in binary format
 * @VPU_FW_DUMP_DATA: Data dump in binary format
 */
enum vpu_fw_crash_dump_type {
	VPU_FW_DUMP_REGISTERS = 0,
	VPU_FW_DUMP_DATA = 1,
	VPU_FW_CRASH_DUMP_MAX,
};


unsigned int vpu_prt_mode = (unsigned int)VPU_PRT_SINCE_SET_DBG;
unsigned int vdbg_mask = (unsigned int)VPU_DBG_ERROR;
unsigned int module_param_vdbg_lib = 0U;
unsigned int module_param_vdbg_drv = 0U;


module_param_named(use_prtk, vpu_prt_mode, uint, 0x1A4); //0644
MODULE_PARM_DESC(use_prtk, "use always debug print mode (default: disabled(=0))");

module_param_named(debug_mask, vdbg_mask, uint, 0x1A4); //0644
MODULE_PARM_DESC(debug_mask, "debug output mask");

module_param_named(vdbg_lib, module_param_vdbg_lib, uint, 0x1A4); //0644
MODULE_PARM_DESC(vdbg_lib, "debug vpu-core-library");

module_param_named(vdbg_drv, module_param_vdbg_drv, uint, 0x1A4); //0644
MODULE_PARM_DESC(vdbg_drv, "debug vpu-driver");

bool vdbg_mode(void)
{
	unsigned int mode = vpu_prt_mode & 0xFFFF;
	return (mode == (unsigned int)VPU_ALWAYS_PRT_DBG) ? (bool)true : (bool)false;
}

static inline bool vpu_have_debug_mask(enum vdbg_mask_type dbg_mask)
{
	bool ret = (bool)false;

	if (((unsigned int)dbg_mask & vdbg_mask) == dbg_mask) {
		ret = (bool)true;
	}
	return ret;
}

void V_DEBUG(enum vdbg_mask_type dbg_mask, const char *fn, int ln, const char *fmt, ...)
{
	struct va_format vaf = {
		.fmt = fmt,
	};

	va_list args;
	va_start(args, fmt);
	vaf.va = &args;

	if (vpu_prt_mode == (unsigned int)VPU_ALWAYS_PRT_DBG) {
		if (((unsigned int)dbg_mask & (unsigned int)VPU_DBG_ERROR) == (unsigned int)VPU_DBG_ERROR) {
			(void)pr_err("[%s:%4d] %pV\n", fn, ln, &vaf);
		} else {
			if (vpu_have_debug_mask(dbg_mask) == (bool)true) {
				(void)pr_err("[%s:%4d] %pV\n", fn, ln, &vaf);
			}
		}
	} else {
		if (((unsigned int)dbg_mask & (unsigned int)VPU_DBG_ERROR) == (unsigned int)VPU_DBG_ERROR) {
			(void)pr_info("[%s:%4d] %pV\n", fn, ln, &vaf);
		} else {
			if (vpu_have_debug_mask(dbg_mask) == (bool)true) {
				(void)pr_info("[%s:%4d] %pV\n", fn, ln, &vaf);;
			}
		}
	}
	//trace_vpu_log_dbg(dev, &vaf);
	va_end(args);
}
EXPORT_SYMBOL(V_DEBUG);


///////////////////////////////////////////////////////////////////////////////////////
///
/// WRITE LOG
///
///////////////////////////////////////////////////////////////////////////////////////
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/time64.h>

#if !defined(CONFIG_ANDROID)
static char gs_szFilename[512];
static char *gs_pszFilename;
static int gs_iUseCustomLog = 0;
static int gs_iLogCount;

#define LOG_FILE_PATH "/tmp"
#endif

#if !defined(CONFIG_ANDROID)
void init_vdbg_log(void)
{
	(void)memset(gs_szFilename, 0, sizeof(gs_szFilename));
	gs_pszFilename = NULL;
	gs_iUseCustomLog = 1;
	gs_iLogCount = 0;
}
#endif

void vdbg_log(const char *fmt, ...)
{
#if !defined(CONFIG_ANDROID)
	if (gs_iUseCustomLog == 1)
	{
		struct file *filp;
		char buf[512] = {0};
		va_list args;
		int len;
		int written;
		struct timespec64 ts;
		struct tm tm_info;
		int tm_info_month;
		long tm_info_year;

		va_start(args, fmt);

		// Save current time and log message in buf
		ktime_get_real_ts64(&ts);
		time64_to_tm(ts.tv_sec, 0, &tm_info);
		//len = sprintf(buf, "[%04d/%02d/%02d %02d:%02d:%02d.%06ld] ", tm.tm_year + 1900,
		//			tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec / 1000);
		tm_info_month = (tm_info.tm_mon >= 0 && tm_info.tm_mon < 12) ? tm_info.tm_mon + 1 : 1;
		tm_info_year = tm_info.tm_year + 1900 + 1;

		len = snprintf(buf, sizeof(buf), "[%04ld/%02d/%02d-%02d:%02d:%02d] ", tm_info_year,
					tm_info_month, tm_info.tm_mday, tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);

		if (len < 0)
		{
			printk(KERN_ERR "[vdbg_log] tm_info buffuer error. Cannot print dbg log.");
			return;
		}

		// Open log file
		if (gs_pszFilename == NULL)
		{
			(void)snprintf(gs_szFilename, sizeof(gs_szFilename), "%s/vdbg_log-%04ld-%02d-%02d-%02d-%02d-%02d.txt", LOG_FILE_PATH,
					tm_info_year, tm_info_month, tm_info.tm_mday, tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);
			//sprintf(gs_szFilename, "%s/profile.txt", LOG_FILE_PATH);
			gs_pszFilename = gs_szFilename;

			filp = filp_open(gs_pszFilename, O_WRONLY | O_CREAT | O_TRUNC, 0x1B6); //0666
			if (IS_ERR(filp)) {
				printk(KERN_ERR "[vdbg_log] Failed to open file: %s\n", gs_pszFilename);
				gs_iUseCustomLog = 0;
				va_end(args);
				return;
			}
			printk(KERN_ERR "[vdbg_log] open file: %s\n", gs_pszFilename);
			filp_close(filp, NULL);
		}

		if ((sizeof(buf) > len) && ((sizeof(buf) - len) > 0)) {
			written = vsnprintf(buf + len, sizeof(buf) - len, fmt, args);
			if (written >= 0) {
				if (INT_MAX - len >= written) {
					len += written;
				} else {
					V_DBG(VPU_DBG_ERROR,
					  "In the calculation of len += written, overflow occurred over UINT_MAX");
				}
			} else {
				V_DBG(VPU_DBG_ERROR,
				  "[vpu_dbg][%s][vsnprintf Error] buf:%s, sizeof(buf):%s, len:%d, written:%d",
				  __func__, buf, sizeof(buf), len, written);
			}
		}
		//buf[len++] = '\n';

		filp = filp_open(gs_pszFilename, O_WRONLY | O_CREAT | O_APPEND, 0x1B6); //0666

		// Write message to log file
		if (IS_ERR(filp)) {
			printk(KERN_ERR "[vdbg_log] Failed to write file: %s\n", gs_pszFilename);
			gs_iUseCustomLog = 0;
		} else {
			//printk(KERN_ERR "[vdbg_log] [%4d]: %s, %d\n", gs_iLogCount, buf, len);
			if (gs_iLogCount < (int)INT_MAX) {
				gs_iLogCount++;
			}
			filp->f_op->write(filp, buf, len, &filp->f_pos);
			filp_close(filp, NULL);
		}

		va_end(args);
	}
#else
	va_list args;
	va_start(args, fmt);
	(void)printk(fmt, args);
	va_end(args);
#endif
}

void vdbg_log_printk(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	(void)printk(fmt, args);
	va_end(args);
}
#ifdef VPU_LIB_WRITE_OUTPUT_TO_YUV
///////////////////////////////////////////////////////////////////////////////////////
///
/// WRITE YUV
///
///////////////////////////////////////////////////////////////////////////////////////
#include <linux/kthread.h>
#include <linux/wait.h>

static DECLARE_WAIT_QUEUE_HEAD(gs_yuvThreadDumpQueue);

static int gs_iYuvDumpCount = 0;
static int gs_iYuvDumpRequestedCount = 0;
static spinlock_t gs_yuvThreadInfoLock;
static bool gs_bThreadDumpRequested = false;
struct task_struct *gs_pYuvThreadTask = NULL;
typedef struct yuv_thread_info_t
{
	int iDispW;
	int iDispH;
	int iDispIdx;

	unsigned int virtAddrY;
	size_t sizeY;
	unsigned int virtAddrU;
	size_t sizeU;
	unsigned int virtAddrV;
	size_t sizeV;
} yuv_thread_info_t;
yuv_thread_info_t gs_stYuvThreadInfo;

void request_thread_dump(int w, int h, int idx, unsigned int y, unsigned int u, unsigned int v)
{
	spin_lock(&gs_yuvThreadInfoLock);

	if (gs_bThreadDumpRequested) {
		printk(KERN_ERR "Thread dump request already in progress, ignore idx:%d (dump:%d/all:%d)\n", idx, gs_iYuvDumpCount, gs_iYuvDumpRequestedCount);
		gs_iYuvDumpRequestedCount++;
		spin_unlock(&gs_yuvThreadInfoLock);
		return;
	}

	gs_stYuvThreadInfo.iDispW = w;
	gs_stYuvThreadInfo.iDispH = h;
	gs_stYuvThreadInfo.iDispIdx = idx;

	gs_stYuvThreadInfo.virtAddrY = y;
	gs_stYuvThreadInfo.virtAddrU = u;
	gs_stYuvThreadInfo.virtAddrV = v;

	gs_iYuvDumpCount++;
	gs_bThreadDumpRequested = true;

	spin_unlock(&gs_yuvThreadInfoLock);
	wake_up_interruptible(&gs_yuvThreadDumpQueue);
}

int dump_yuv_thread(void *data)
{
	yuv_thread_info_t *p_yuvdata = (yuv_thread_info_t*)data;

	while (!kthread_should_stop())
	{
		if (gs_bThreadDumpRequested)
		{
			int w = p_yuvdata->iDispW;
			int h = p_yuvdata->iDispH;
			int stride = ((w + 15) >> 4) << 4;
			int bitstream_format;
			int yuv_interleaved;
			int byte_depth;
			int index = p_yuvdata->iDispIdx;

			vdbg_get_codec_info(&bitstream_format, &yuv_interleaved, &byte_depth);

			if (bitstream_format == 15) {
				stride = VPU_ALIGN32(w);
			} else if (bitstream_format == 16) {
				stride = VPU_ALIGN64(w);
			}

			{
				size_t save_size;
				unsigned int p_src_y = p_yuvdata->virtAddrY;
				unsigned int p_src_cb = p_yuvdata->virtAddrU;
				unsigned int p_src_cr = p_yuvdata->virtAddrV;

				if (p_src_y == 0 || p_src_cb == 0 || p_src_cr == 0) {
					V_DBG(VPU_DBG_ERROR, "[YUV][%2d] Error Virtual Address", index);
				} else {
					// Y
					{
						void *p_src_y_virt;
						save_size = stride * h * byte_depth;
						p_src_y_virt = ioremap(p_src_y, save_size);
						if (p_src_y_virt == NULL) {
							V_DBG(VPU_DBG_ERROR, "[Y][%2d] Error Virtual Address is NULL", index);
						} else {
							V_DBG(VPU_DBG_ERROR, "[Y:%x][%4d][%2d][Size:%d][s:%d][wxh:%dx%d]", p_src_y_virt, gs_iYuvDumpCount, index, save_size, stride, w, h);
							vdbg_save_yuv_frame(p_src_y_virt, save_size);
							iounmap(p_src_y_virt);
						}
					}
					if (yuv_interleaved) { //NV12
						// U / V
						void *p_src_cbcr_virt;
						save_size = ((stride * h) / 2) * byte_depth;
						p_src_cbcr_virt = ioremap(p_src_cb, save_size);
						if (p_src_cbcr_virt == NULL) {
							V_DBG(VPU_DBG_ERROR, "[C][%2d] Error Virtual Address is NULL", index);
						} else {
							V_DBG(VPU_DBG_ERROR, "[C:%x][%4d][%2d][Size:%d][s:%d][wxh:%dx%d]", p_src_cbcr_virt, gs_iYuvDumpCount, index, save_size, stride, w, h);
							vdbg_save_yuv_frame(p_src_cbcr_virt, save_size);
							iounmap(p_src_cbcr_virt);
						}
					} else { //YUV420P
						save_size = ((stride * h) / 4) * byte_depth;
						// U
						{
							void *p_src_cb_virt = ioremap(p_src_cb, save_size);
							if (p_src_cb_virt == NULL) {
								V_DBG(VPU_DBG_ERROR, "[U][%2d] Error Virtual Address is NULL", index);
							} else {
								V_DBG(VPU_DBG_ERROR, "[U:%x][%4d][%2d][Size:%d][s:%d][wxh:%dx%d]", p_src_cb_virt, gs_iYuvDumpCount, index, save_size, stride, w, h);
								vdbg_save_yuv_frame(p_src_cb_virt, save_size);
								iounmap(p_src_cb_virt);
							}
						}
						// V
						{
							void *p_src_cr_virt = ioremap(p_src_cb, save_size);
							if (p_src_cr_virt == NULL) {
								V_DBG(VPU_DBG_ERROR, "[V][%2d] Error Virtual Address is NULL", index);
							} else {
								V_DBG(VPU_DBG_ERROR, "[V:%x][%4d][%2d][Size:%d][s:%d][wxh:%dx%d]", p_src_cr_virt, gs_iYuvDumpCount, index, save_size, stride, w, h);
								vdbg_save_yuv_frame(p_src_cr_virt, save_size);
								iounmap(p_src_cr_virt);
							}
						}
					}
				}
			}

			gs_bThreadDumpRequested = false;
		}
	}

	return 0;
}

static char gs_szFilenameYuv[512];
static char *gs_pszFilenameYuv;
static int gs_iUseCustomLogYuv = 0;
static int gs_iLogCountYuv;
static int gs_iCodec;
static int gs_iYuvInterleaved;
static int gs_iByteDepth;

#ifdef CONFIG_ANDROID
#define LOG_FILE_PATH_YUV "/data/vendor/yuv_dump"
#else
#define LOG_FILE_PATH_YUV "/tmp/yuv_dump"
#endif

void init_vdbg_save_yuv(int iCodec, unsigned int iYuvInterleaved, int iBitDepth)
{
	(void)memset(gs_szFilenameYuv, 0, sizeof(gs_szFilenameYuv));
	gs_pszFilenameYuv = NULL;
	gs_iUseCustomLogYuv = 1;
	gs_iLogCountYuv = 0;
	gs_iCodec = iCodec;//15:hevc, 16:vp9
	gs_iYuvInterleaved = iYuvInterleaved;
	if (iBitDepth == 10)
	{
		gs_iByteDepth = 2;
	}
	else
	{
		gs_iByteDepth = 1;
	}

	if (gs_pYuvThreadTask != NULL)
	{
		kthread_stop(gs_pYuvThreadTask);
		gs_pYuvThreadTask = NULL;
	}
	gs_iYuvDumpCount = 0;
	gs_iYuvDumpRequestedCount = 0;
	gs_bThreadDumpRequested = false;

	(void)memset(&gs_stYuvThreadInfo, 0, sizeof(gs_stYuvThreadInfo));
	gs_pYuvThreadTask = kthread_run(dump_yuv_thread, &gs_stYuvThreadInfo, "dump_yuv_thread");
	if (IS_ERR(gs_pYuvThreadTask)) {
		V_DBG(VPU_DBG_ERROR, "[%s %4d][vdbg] kthread create failed", __FILE__, __LINE__);
	} else {
		V_DBG(VPU_DBG_ERROR, "[%s %4d][vdbg] kthread create success", __FILE__, __LINE__);
	}
}

void vdbg_get_codec_info(int* piCodec, int* piYuvInterleaved, int* piByteDepth)
{
	*piCodec = gs_iCodec;
	*piYuvInterleaved = gs_iYuvInterleaved;
	*piByteDepth = gs_iByteDepth;
}

void vdbg_save_yuv_frame(const unsigned char* yuv_data, size_t yuv_size)
{
	if ((gs_iUseCustomLogYuv == 1) && (yuv_size > 0))
	{
		struct file *file;
		mm_segment_t old_fs;
		loff_t pos;

		// Open log file
		if (gs_pszFilenameYuv == NULL)
		{
			struct timespec64 ts;
			struct tm tm;
			int tm_month;
			long tm_year;

			ktime_get_real_ts64(&ts);
			time64_to_tm(ts.tv_sec, 0, &tm);

			tm_month = (tm.tm_mon >= 0 && tm.tm_mon < 12) ? tm.tm_mon + 1 : 1;
			tm_year = tm.tm_year + 1900 + 1;

			snprintf(gs_szFilenameYuv, sizeof(gs_szFilename), "%s/vdbg_yuv-%04ld-%02d-%02d-%02d-%02d-%02d.yuv", LOG_FILE_PATH_YUV,
					tm_year, tm_month, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

			gs_pszFilenameYuv = gs_szFilenameYuv;

			old_fs = get_fs();
			set_fs(KERNEL_DS);
			file = filp_open(gs_pszFilenameYuv, O_WRONLY | O_CREAT, 0x1A4); //0644
			if (IS_ERR(file)) {
				printk(KERN_ERR "[vdbg_save_yuv_frame] Failed to open file: %s\n", gs_pszFilenameYuv);
				gs_iUseCustomLogYuv = 0;
				return;
			}
			set_fs(old_fs);

			printk(KERN_ERR "[vdbg_save_yuv_frame] Opened file: %s\n", gs_pszFilenameYuv);
			filp_close(file, NULL);
		}

		old_fs = get_fs();
		set_fs(KERNEL_DS);

		// Open file
		file = filp_open(gs_pszFilenameYuv, O_WRONLY | O_APPEND, 0x1A4); //0644
		if (IS_ERR(file)) {
			printk(KERN_ERR "[vdbg_save_yuv_frame] Failed to open file: %s\n", gs_pszFilenameYuv);
			gs_iUseCustomLogYuv = 0;
			return;
		}

		// move to the end of file
		pos = file->f_op->llseek(file, 0, SEEK_END);
		if (pos < 0) {
			printk(KERN_ERR "[vdbg_save_yuv_frame] Failed to seek file: %s\n", gs_pszFilenameYuv);
			filp_close(file, NULL);
			return;
		}

		// write yuv data to file
		if (pos != file->f_pos) {
			printk(KERN_ERR "[vdbg_save_yuv_frame] write file: pos=%lld, file->f_pos=%lld\n", pos, file->f_pos);
		} else {
#if defined(CONFIG_ANDROID)
			if (file->f_op->write(file, yuv_data, yuv_size, &file->f_pos) < 0) {
				printk(KERN_ERR "[vdbg_save_yuv_frame] Failed to write file at %lld(%ld): %s\n", file->f_pos, yuv_size, gs_pszFilenameYuv);
			}
#else
			if (kernel_write(file, yuv_data, yuv_size, &file->f_pos) < 0) {
				printk(KERN_ERR "[vdbg_save_yuv_frame] Failed to write file at %lld(%ld): %s\n", file->f_pos, yuv_size, gs_pszFilenameYuv);
			}
#endif
		}

		set_fs(old_fs);
		filp_close(file, NULL);
	}
}
#endif
