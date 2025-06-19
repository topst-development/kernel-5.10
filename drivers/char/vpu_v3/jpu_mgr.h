// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef JPU_MGR_H
#define JPU_MGR_H

#include "vpu_comm.h"

int jmgr_probe(struct platform_device *pdev);
int jmgr_remove(struct platform_device *pdev);

#if defined(CONFIG_PM)
int jmgr_suspend(struct platform_device *pdev, pm_message_t state);
int jmgr_resume(struct platform_device *pdev);
#endif

#endif /*JPU_MGR_H*/
