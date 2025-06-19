// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_DEC_H
#define VPU_DEC_H

#include "vpu_comm.h"
#include "vpu_mgr.h"
#include "vpu_internal_type.h"

typedef struct vpu_dec_drv_t
{
	vpu_drv_shared_t* dec_shared; //decoder shared data
	vpu_drv_poll_t drv_poll_data; //each decoder have this context to process command interrrupt
	vpu_mgr_t* mgr_ctx;
	vpu_drv_info_t info;

	//each decoder has a sequentially incremented command_id to check for drops or unusual sequences.
	//set to 0 if command_id is greater than MAX_COMMAND_ID.
	unsigned int command_id;

	//after sequence header init, flush, it will set to 1
	enum vpu_frame_skip_mode auto_frame_skipmode;
}vpu_dec_drv_t;

int vdec_probe(struct platform_device *pdev);
int vdec_remove(struct platform_device *pdev);

#endif /*VPU_DEC_H*/
