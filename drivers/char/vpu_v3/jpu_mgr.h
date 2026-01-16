/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#ifndef JPU_MGR_H
#define JPU_MGR_H

#include "vpu_comm.h"

int jmgr_probe(struct platform_device *pdev);
VREMOVE_RET_TYPE jmgr_remove(struct platform_device *pdev);

#if defined(CONFIG_PM)
int jmgr_suspend(struct platform_device *pdev, pm_message_t state);
int jmgr_resume(struct platform_device *pdev);
#endif

#endif /*JPU_MGR_H*/
