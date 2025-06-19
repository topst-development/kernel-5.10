// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_HEVC_ENC2_MGR_H
#define VPU_HEVC_ENC2_MGR_H

#include "vpu_comm.h"

int vmgr_hevc_enc2_probe(struct platform_device *pdev);
int vmgr_hevc_enc2_remove(struct platform_device *pdev);

#if defined(CONFIG_PM)
int vmgr_hevc_enc2_suspend(struct platform_device *pdev, pm_message_t state);
int vmgr_hevc_enc2_resume(struct platform_device *pdev);
#endif

#endif /*VPU_HEVC_ENC2_MGR_H*/

