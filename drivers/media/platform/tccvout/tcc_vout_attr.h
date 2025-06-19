/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_VOUT_ATTR_H
#define TCC_VOUT_ATTR_H

extern void tcc_vout_attr_create(struct platform_device *pdev);
extern void tcc_vout_attr_remove(struct platform_device *pdev);

/**
 * This file contains the interface to the Linux device attributes.
 */
extern struct device_attribute dev_attr_vioc_path;
extern struct device_attribute dev_attr_vioc_rdma;
extern struct device_attribute dev_attr_vioc_sc;
extern struct device_attribute dev_attr_vioc_wmix_ovp;
extern struct device_attribute dev_attr_force_v4l2_memory_userptr;
extern struct device_attribute dev_attr_vout_pmap;
/* deinterlace */
extern struct device_attribute dev_attr_deinterlace;
extern struct device_attribute dev_attr_deinterlace_path;
extern struct device_attribute dev_attr_deinterlace_rdma;
extern struct device_attribute dev_attr_deinterlace_pmap;
extern struct device_attribute dev_attr_deinterlace_bufs;
extern struct device_attribute dev_attr_deinterlace_bfield;
extern struct device_attribute dev_attr_deinterlace_sc;
extern struct device_attribute dev_attr_deinterlace_force;
/* on-the-fly */
extern struct device_attribute dev_attr_otf_mode;

#endif //TCC_VOUT_ATTR_H
