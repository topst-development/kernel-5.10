/*
 * Copyright (C) Telechips, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef TCC_VOUT_BOOTING_ANIMATION_H
#define TCC_VOUT_BOOTING_ANIMATION_H

#define BOOTANIMATION_WIDTH		3840
#define BOOTANIMATION_HEIGHT	720
#define PANEL_WIDTH				1920
#define PANEL_HEIGHT			720

enum VOUT_BOOTANI_TYPE {
	VOUT_BOOTANI_OFF,
	VOUT_BOOTANI_ON,
	VOUT_BOOTANI_MAX,
};

extern int vout_set_vout_bootani_path(struct tcc_vout_device *vout);
extern int vidioc_s_fmt_vid_out_bootani(struct file *file, void *fh,
	struct v4l2_format *f);
extern int vidioc_qbuf_bootani(struct file *file, void *fh,
	struct v4l2_buffer *buf);
extern int vidioc_streamon_bootani(struct file *file, void *fh,
	enum v4l2_buf_type i);
extern int vidioc_streamoff_bootani(struct file *file, void *fh,
	enum v4l2_buf_type i);
extern int vidioc_reqbufs_bootani(struct file *file, void *fh,
	struct v4l2_requestbuffers *req);
extern int tcc_v4l2_buffer_set_bootani(struct tcc_vout_device *vout,
	struct v4l2_requestbuffers *req);

extern void vout_m2m_display_update_bootani(struct tcc_vout_device *vout,
	struct v4l2_buffer *buf);
extern void vout_m2m_display_update_bootani_reserved11(struct tcc_vout_device *vout,
	struct v4l2_buffer *buf);

extern int vidioc_s_fmt_vid_out_overlay_bootani(struct file *file, void *fh,
	struct v4l2_format *f);
extern void vout_deinit_bootani(struct tcc_vout_device *vout);
extern void vout_disp_ctrl_bootani(struct tcc_vout_device *vout, int enable);
extern int tcc_deintl_buffer_set_bootani(struct tcc_vout_device *vout);

#endif //__TCC_VOUT_BOOTING_ANIMATION_H__
