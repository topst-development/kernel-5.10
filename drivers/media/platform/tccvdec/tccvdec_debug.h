/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef TCC_VDEC_DEBUG_H
#define TCC_VDEC_DEBUG_H

enum{
	VDEC_ERR = 0,
	VDEC_DEBUG = 1,
	VDEC_INFO = 2,
	VDEC_DATA = 3,
	VDEC_STEP = 4,
	VDEC_DBG_MAX = VDEC_DATA
};


#define dummy_log(...) do{ }while(0)

#define tcvdec_err(fmt, ...)  pr_err("[%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define tcvdec_dbg(fmt, ...)  pr_debug("[%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define tcvdec_info(fmt, ...) pr_info("[%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define tcvdec_data(fmt, ...) pr_debug("[%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define tcvdec_step(fmt, ...) pr_debug("[%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)

#define null_print(type,x) \
		if       (type == VDEC_ERR)	tcvdec_err("%s [%s] is null !\n",__func__,#x); \
		else if(type == VDEC_DEBUG) tcvdec_dbg("%s [%s] is null !\n",__func__,#x); \
		else if(type == VDEC_INFO) tcvdec_info("%s [%s] is null !\n",__func__,#x); \
		else if(type == VDEC_DATA) tcvdec_data("%s [%s] is null !\n",__func__,#x); \
		else if(type == VDEC_STEP) tcvdec_step("%s [%s] is null !\n",__func__,#x); \
		else tcvdec_err("%s [%s] is null !\n",__func__,#x);

#define if_null_print_return_value(x,v) \
	if(!x){ \
		null_print(VDEC_ERR,x); \
		return v; \
	}

#define if_null_print_return(x) \
	if(!x){ \
		null_print(VDEC_ERR,x); \
		return; \
	}

#define vdec_err_return_value(x,v,f,format,...) \
	if(x){ \
		f(format,__VA_ARGS__); \
		return v; \
	}
#define vdec_err_return(x,f,format,...) \
	if(x){ \
		f(format,__VA_ARGS__); \
		return; \
	}

#endif //TCC_VDEC_DEBUG_H

