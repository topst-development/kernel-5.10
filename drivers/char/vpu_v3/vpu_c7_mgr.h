/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#ifndef VPU_MGR_C7_H
#define VPU_MGR_C7_H

#include "vpu_comm.h"

int vmgr_c7_probe(struct platform_device *pdev);
VREMOVE_RET_TYPE vmgr_c7_remove(struct platform_device *pdev);

#if defined(CONFIG_PM)
int vmgr_c7_suspend(struct platform_device *pdev, pm_message_t state);
int vmgr_c7_resume(struct platform_device *pdev);
#endif

#endif //VPU_MGR_C7_H