/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#ifndef VPU_ENC_H
#define VPU_ENC_H

#include "vpu_comm.h"
#include "vpu_mgr.h"
#include "vpu_internal_type.h"

typedef struct vpu_enc_drv_t {
	vpu_drv_shared_t *enc_shared; //encoder shared data
	vpu_drv_poll_t drv_poll_data; //each decoder have this context to process command interrrupt
	vpu_mgr_t *mgr_ctx;
	vpu_drv_info_t info;

	//each decoder has a sequentially incremented command_id to check for drops or unusual sequences.
	//set to 0 if command_id is greater than MAX_COMMAND_ID.
	unsigned int command_id;
} vpu_enc_drv_t;

int venc_probe(struct platform_device *pdev);
VREMOVE_RET_TYPE venc_remove(struct platform_device *pdev);

#endif /*VPU_ENC_H*/
