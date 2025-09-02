/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */
#ifndef FB_PANEL_HELPER_H
#define FB_PANEL_HELPER_H

struct fb_panel;

struct fb_panel_funcs {
	int (*disable)(struct fb_panel *panel);
	int (*unprepare)(struct fb_panel *panel);
	int (*prepare)(struct fb_panel *panel);
	int (*enable)(struct fb_panel *panel);
	int (*get_videomode)(struct fb_panel *panel, struct videomode *vm);

	/*
	 * set_pclk - Set the pixel clock rate.
	 *  Incase of some display device such as Displayport requires actual
	 *  pixel clock information from the display controller to operate
	 *  properly.
	 *  In the case of a panel like this, you must set the actual pixel
	 *  clock by calling set_pclk callback before calling enable callback.
	 */
	int (*set_pclk)(struct fb_panel *panel, unsigned long pclk);
};

struct fb_panel {
	struct device *dev;
	const struct fb_panel_funcs *funcs;
	struct list_head list;
};

void fb_panel_init(struct fb_panel *panel);
int fb_panel_add(struct fb_panel *panel);
void fb_panel_remove(struct fb_panel *panel);
struct fb_panel *of_fb_find_panel(const struct device_node *np);

int fb_panel_prepare(struct fb_panel *panel);
int fb_panel_enable(struct fb_panel *panel);
int fb_panel_disable(struct fb_panel *panel);
int fb_panel_unprepare(struct fb_panel *panel);
int fb_panel_get_mode(struct fb_panel *panel, struct videomode *vm);
int fb_panel_set_pclk(struct fb_panel *panel, unsigned long pclk);

#endif /*FB_PANEL_HELPER_H*/

