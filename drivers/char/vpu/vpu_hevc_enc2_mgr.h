// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef VPU_HEVC_ENC2_MGR_H
#define VPU_HEVC_ENC2_MGR_H

#include "vpu_comm.h"

int vmgr_hevc_enc2_opened(void);
int vmgr_hevc_enc2_get_close(vputype type);
int vmgr_hevc_enc2_set_close(vputype type, int value, int bfreemem);
int vmgr_hevc_enc2_get_alive(void);

int vmgr_hevc_enc2_process_ex(struct VpuList *cmd_list, vputype type,
		int Op, int *result);
struct VpuList *vmgr_hevc_enc2_list_manager(struct VpuList *args,
		unsigned int cmd);

int vmgr_hevc_enc2_probe(struct platform_device *pdev);
VREMOVE_RET_TYPE vmgr_hevc_enc2_remove(struct platform_device *pdev);

#if defined(CONFIG_PM)
int vmgr_hevc_enc2_suspend(struct platform_device *pdev,
			pm_message_t state);
int vmgr_hevc_enc2_resume(struct platform_device *pdev);
#endif

#endif /*VPU_HEVC_ENC2_MGR_H*/
