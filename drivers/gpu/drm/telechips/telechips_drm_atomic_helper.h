/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * tcc_drm_crtc_plane_helper.h
 *
 * Copyright (C) 2022 Telechips Inc.
 * Authors:
 *	Jayden Kim
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#ifndef TCCDRM_ATOMIC_HELPER_H
#define TCCDRM_ATOMIC_HELPER_H

void tccdrm_commit_tail(struct drm_atomic_state *old_state);

#endif