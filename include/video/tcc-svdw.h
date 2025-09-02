#ifndef TCC_SVDW_H
#define TCC_SVDW_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/videodev2.h>

#define TCC_SVDW_DEWARP_MAX 4

extern struct list_head tcc_svdw_dewarp_list;
extern atomic_t tcc_svdw_dewarp_cnt;

struct tcc_svdw_ops {
	int (*start_streaming)(void *p_data);
	int (*stop_streaming)(void *p_data);
	int (*g_status)(void *p_data);
	int (*g_phys_addr)(void *p_data, phys_addr_t *p_addr);
	int (*s_fmt)(void *p_data, struct v4l2_pix_format_mplane *pix_mp);
};

struct tcc_svdw_dewarp_unit {
	struct list_head anchor;
	struct device *dev;
	struct video_device *vdev;
	const struct tcc_svdw_ops *ops;

	bool svdw_mode;
};

#endif // TCC_SVDW_H
