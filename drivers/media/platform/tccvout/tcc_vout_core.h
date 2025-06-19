/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef TCC_VOUT_CORE_H__
#define TCC_VOUT_CORE_H__

extern int tcc_get_base_address(
	unsigned int viocfmt, unsigned int base0_addr,
	unsigned int width, unsigned int height,
	unsigned int pos_x, unsigned int pos_y,
	unsigned int *base0, unsigned int *base1, unsigned int *base2);
extern int tcc_get_base_address_of_image(
	unsigned int pixelformat, unsigned int base0_addr,
	unsigned int width, unsigned int height,
	unsigned int *base0, unsigned int *base1, unsigned int *base2);
extern int deintl_viqe_setup(struct tcc_vout_device *vout,
	enum deintl_type deinterlace, int plugin);
extern int deintl_s_setup(struct tcc_vout_device *vout);
extern void m2m_wmix_setup(struct vioc_wmix *wmix);
extern void m2m_wdma_setup(struct vioc_wdma *wdma);
extern void vout_check_format(struct vioc_rdma *rdma, unsigned int fmt);
extern  int tcc_vout_buffer_copy(struct v4l2_buffer *dst,
	struct v4l2_buffer *src, int mode);

extern int vout_get_pmap(struct pmap *pmap);
extern int vout_set_vout_path(struct tcc_vout_device *vout);
extern int vout_set_m2m_path(int deintl_default, struct tcc_vout_device *vout);
extern int vout_vioc_set_default(struct tcc_vout_device *vout);
extern int vout_vioc_init(struct tcc_vout_device *vout);
extern void vout_deinit(struct tcc_vout_device *vout);
extern void vout_disp_ctrl(struct tcc_vout_vioc *vioc, int enable);
extern void vout_rdma_setup(struct tcc_vout_device *vout);
extern void vout_wmix_setup(struct tcc_vout_device *vout);
extern void vout_wmix_getsize(struct tcc_vout_device *vout,
	unsigned int *w, unsigned int *h);
extern void vout_path_reset(struct tcc_vout_vioc *vioc);
/* de-interlace */
extern void m2m_path_reset(struct tcc_vout_vioc *vioc);
extern void m2m_rdma_setup(struct vioc_rdma *rdma);
extern int vout_otf_init(struct tcc_vout_device *vout);
extern void vout_otf_deinit(struct tcc_vout_device *vout);
extern int vout_m2m_init(struct tcc_vout_device *vout);
extern void vout_m2m_ctrl(struct tcc_vout_vioc *vioc, int enable);
#if defined(CONFIG_TELECHIPS_DUAL_DISPLAY)
extern int vout_set_vout_dual_path(struct tcc_vout_device *vout);
extern int vout_m2m_dual_init(struct tcc_vout_device *vout);
extern void vout_m2m_dual_deinit(struct tcc_vout_device *vout);
extern void vout_m2m_dual_ctrl(struct tcc_vout_vioc *vioc, int enable,
	int m2m_dual_index);
#endif
extern void vout_m2m_deinit(struct tcc_vout_device *vout);
/* overlay  */
extern void vout_video_overlay(struct tcc_vout_device *vout);
/* sub-plane */
extern void vout_subplane_deinit(struct tcc_vout_device *vout);
extern void vout_subplane_ctrl(struct tcc_vout_device *vout, int enable);
extern void vout_subplane_m2m_init(struct tcc_vout_device *vout);
extern int vout_subplane_m2m_qbuf(struct tcc_vout_device *vout,
	struct vioc_alpha *alpha);
extern void vout_subplane_onthefly_init(struct tcc_vout_device *vout);
extern int vout_subplane_onthefly_qbuf(struct tcc_vout_device *vout);

/* buffer control */
extern void vout_pop_all_buffer(struct tcc_vout_device *vout);

/* streaming */
extern void vout_onthefly_display_update(
	struct tcc_vout_device *vout, struct v4l2_buffer *buf);
extern void vout_m2m_display_update(struct tcc_vout_device *vout,
		struct v4l2_buffer *buf);
extern void vout_m2m_display_update_reserved11(struct tcc_vout_device *vout,
		struct v4l2_buffer *buf);

#ifdef CONFIG_TELECHIPS_HDMI_DRIVER_V2_0
#include "../../../char/hdmi_v2_0/include/hdmi_ioctls.h"
extern void hdmi_set_drm(DRM_Packet_t *drmparm);
extern void hdmi_clear_drm(void);
extern unsigned int hdmi_get_refreshrate(void);
#endif

extern void vout_intr_onoff(char on, struct tcc_vout_device *vout);
#ifdef CONFIG_TELECHIPS_G2D
extern void wdma_work_thread(struct work_struct * data);
#endif

#endif //TCC_VOUT_CORE_H__
