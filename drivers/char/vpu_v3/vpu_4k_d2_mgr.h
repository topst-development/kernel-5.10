/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#ifndef VPU_4K_D2_MGR_H
#define VPU_4K_D2_MGR_H

#include "vpu_comm.h"

int vmgr_4k_d2_probe(struct platform_device *pdev);
VREMOVE_RET_TYPE vmgr_4k_d2_remove(struct platform_device *pdev);

#if defined(CONFIG_PM)
int vmgr_4k_d2_suspend(struct platform_device *pdev, pm_message_t state);
int vmgr_4k_d2_resume(struct platform_device *pdev);
#endif

#endif /*VPU_4K_D2_MGR_H*/
