/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#ifndef VPU_HEVC_DEC_MGR_H
#define VPU_HEVC_DEC_MGR_H

#include "vpu_comm.h"

int vmgr_hevc_dec_probe(struct platform_device *pdev);
VREMOVE_RET_TYPE vmgr_hevc_dec_remove(struct platform_device *pdev);
#if defined(CONFIG_PM)
int vmgr_hevc_dec_suspend(struct platform_device *pdev, pm_message_t state);
int vmgr_hevc_dec_resume(struct platform_device *pdev);
#endif

#endif /*VPU_HEVC_DEC_MGR_H*/
