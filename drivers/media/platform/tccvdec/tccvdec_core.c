// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *      tccvdec_core.c  --  Telechips Video Decoder Driver
 *
 ******************************************************************************


 *   Modified by Telechips Inc.


 *   Modified date : 2023


 *   Description : video decoder


 *****************************************************************************/
#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <media/videobuf2-dma-sg.h>
#include <media/v4l2-mem2mem.h>
#include <asm/div64.h>
#include <linux/dma-mapping.h>

#include "tccvdec_core.h"
#include "tccvdec_debug.h"
