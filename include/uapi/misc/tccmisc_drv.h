/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef __TCCMISC_DRV_H__
#define __TCCMISC_DRV_H__

#include <linux/types.h>
#include <linux/ioctl.h>

/* Ioctl commands */
#define IOCTL_TCCMISC_PMAP _IOWR('T', 1, struct tccmisc_user_t)
#define IOCTL_TCCMISC_PMAP_KERNEL _IOWR('T', 2, struct tccmisc_user_t)
#define IOCTL_TCCMISC_PHYS _IOWR('T', 3, struct tccmisc_phys_t)
#define IOCTL_TCCMISC_PHYS_KERNEL _IOWR('T', 4, struct tccmisc_phys_t)
#define IOCTL_TCCMISC_PHYS_VIDEOBUF2 _IOWR('T', 5, struct tccmisc_phys_t)
#define IOCTL_TCCMISC_PHYS_VIDEOBUF2_KERNEL _IOWR('T', 6, struct tccmisc_phys_t)
#define IOCTL_TCCMISC_OVP _IOWR('T', 7, struct tccmisc_ovp_t)

/* Ioctl data */
struct tccmisc_user_t {
	char name[32];	/* input: pmap name */
	__u64 base;	/* output: pmap base address */
	__u64 size;	/* output: pmap size */
};

struct tccmisc_phys_t {
	int dmabuf_fd;
	__u64 addr;
	__u64 len;
};

/*
 * Get & Set Overlay Priority (OVP) in WMIX
 * ========================================
 * HOWTO GET OVP
 * -------------
 * struct tccmisc_ovp_t ovp_t;
 * ovp_t.dir = 0;		// Get OVP
 * ovp_t.nr_wmix = 1;	// VIOC WMIX1
 * if (ioctl(fd, IOCTL_TCCMISC_OVP, &ovp_t) == 0) {
 *     printf("Current OVP is %d\n", ovp_t.ovp);
 * } else {
 *     printf("Error\n");
 * }
 *
 * HOWTO SET OVP
 * -------------
 * struct tccmisc_ovp_t ovp_t;
 * ovp_t.dir = 1;		// Set OVP
 * ovp_t.nr_wmix = 0;	// VIOC WMIX0
 * ovp_t.ovp = 21;		// Chage OVP to 21
 * if (ioctl(fd, IOCTL_TCCMISC_OVP, &ovp_t) == 0) {
 *     printf("Changed OVP is %d\n", ovp_t.ovp);
 * } else {
 *     printf("Error\n");
 * }
 */
struct tccmisc_ovp_t {
	unsigned int dir;		/* [Direction] 0: get OVP value, 1: set OVP value*/
	unsigned int nr_wmix;	/* VIOC WMIX index */
	unsigned int ovp;		/* OVP (Overlay priority) of nr_wmix */
};

/* Only kernel function */
int tccmisc_pmap(struct tccmisc_user_t *pmap);
int tccmisc_phys(struct device *dev, struct tccmisc_phys_t *phys);
#endif
