/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef TCC_VENC_DEBUG_H
#define TCC_VENC_DEBUG_H

enum{
	VENC_ERR = 0,
	VENC_DEBUG = 1,
	VENC_INFO = 2,
	VENC_DATA = 3,
	VENC_STEP = 4,
	VENC_DBG_MAX = VENC_DATA
};


#define dummy_log(...) do{ }while(0)

#define tcvenc_err(fmt, ...)  pr_err("[%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define tcvenc_dbg(fmt, ...)  pr_debug("[%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define tcvenc_info(fmt, ...) pr_info("[%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define tcvenc_data(fmt, ...) pr_debug("[%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)
#define tcvenc_step(fmt, ...) pr_debug("[%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__)

#define null_print(type,x) \
		if       (type == VENC_ERR)	tcvenc_err("%s [%s] is null !\n",__func__,#x); \
		else if(type == VENC_DEBUG) tcvenc_dbg("%s [%s] is null !\n",__func__,#x); \
		else if(type == VENC_INFO) tcvenc_info("%s [%s] is null !\n",__func__,#x); \
		else if(type == VENC_DATA) tcvenc_data("%s [%s] is null !\n",__func__,#x); \
		else if(type == VENC_STEP) tcvenc_step("%s [%s] is null !\n",__func__,#x); \
		else tcvenc_err("%s [%s] is null !\n",__func__,#x);

#define if_null_print_return_value(x,v) \
	if(!x){ \
		null_print(VENC_ERR,x); \
		return v; \
	}

#define if_null_print_return(x) \
	if(!x){ \
		null_print(VENC_ERR,x); \
		return; \
	}

#define venc_err_return_value(x,v,f,format,...) \
	if(x){ \
		f(format,__VA_ARGS__); \
		return v; \
	}
#define venc_err_return(x,f,format,...) \
	if(x){ \
		f(format,__VA_ARGS__); \
		return; \
	}

#endif //TCC_VENC_DEBUG_H

