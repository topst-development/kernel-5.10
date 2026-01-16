/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#include "vpu_comm.h"
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
unsigned int vpu_memtrace_enable = (unsigned int)INITIAL_ZERO;
unsigned int vpu_lib_dbg_param = (unsigned int)INITIAL_ZERO;

module_param_named(use_prtk, vpu_prt_mode, uint, 0644);
MODULE_PARM_DESC(use_prtk, "use always debug print mode (default: disabled(=0))");

module_param_named(debug_mask, vdbg_mask, uint, 0644);
MODULE_PARM_DESC(debug_mask, "debug output mask");

module_param_named(memtrace_enable, vpu_memtrace_enable, uint, 0644);
MODULE_PARM_DESC(memtrace_enable, "enable(1)/disable(0) memtrace (default 0)");

module_param_named(vdbg_lib, vpu_lib_dbg_param, uint, 0x1A4); //0644
MODULE_PARM_DESC(vdbg_lib, "debug vpu-core-library");

void vpu_dbg_get_info(int *type, int *mask)
{
	if (type != NULL) {
		*type = vpu_prt_mode;
	}

	if (mask != NULL) {
		*mask = vdbg_mask;
	}
}

EXPORT_SYMBOL(vpu_dbg_get_info);

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
			(void)pr_err("[%s:%d] %pV\n", fn, ln, &vaf);
		} else {
			if (vpu_have_debug_mask(dbg_mask) == (bool)true) {
				(void)pr_err("[%s:%d] %pV\n", fn, ln, &vaf);
			}
		}
	} else {
		if (((unsigned int)dbg_mask & (unsigned int)VPU_DBG_ERROR) == (unsigned int)VPU_DBG_ERROR) {
			(void)pr_info("[%s:%d] %pV\n", fn, ln, &vaf);
		} else {
			if (vpu_have_debug_mask(dbg_mask) == (bool)true) {
				(void)pr_info("[%s:%d] %pV\n", fn, ln, &vaf);;
			}
		}
	}
	//trace_vpu_log_dbg(dev, &vaf);
	va_end(args);
}

EXPORT_SYMBOL(V_DEBUG);

int get_memtrace_enable(void)
{
	return vpu_memtrace_enable;
}
EXPORT_SYMBOL(get_memtrace_enable);

void vpu_printk(const char *fmt, ...)
{
	va_list args;
    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
}
EXPORT_SYMBOL(vpu_printk);

unsigned int get_vpu_lib_dbg_param(void)
{
	return vpu_lib_dbg_param;
}
EXPORT_SYMBOL(get_vpu_lib_dbg_param);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
// Function to log messages into a log file (truncate if it exists)
void vpu_wprintk(const char *fmt, ...)
{
	struct file *file;
	loff_t pos = 0;
	char *log_buf;
	size_t buf_size = 512;
	va_list args;

	// Allocate memory for the log buffer
	log_buf = kmalloc(buf_size, GFP_KERNEL);
	if (!log_buf) {
		printk(KERN_ERR "Failed to allocate log buffer\n");
		return;
	}

	// Process the variable argument list
	va_start(args, fmt);
	vsnprintf(log_buf, buf_size, fmt, args);
	va_end(args);

	// Open the log file for writing, truncate if it exists
	file = filp_open("/tmp/log/vpu_debug.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (!IS_ERR(file)) {
		if (file->f_path.dentry->d_inode->i_size == 0) {
			printk(KERN_ERR "/tmp/log/vpu_debug.log created and opened for the first time\n");
		}

		// Write the log message to the file
		kernel_write(file, log_buf, strlen(log_buf), &pos);
		kernel_write(file, "\n", 1, &pos);

		// Close the log file
		filp_close(file, NULL);
	} else {
		printk(KERN_ERR "Failed to open /tmp/log/vpu_debug.log\n");
	}

	// Free the allocated memory for the log buffer
	kfree(log_buf);
}
EXPORT_SYMBOL(vpu_wprintk);
#endif
