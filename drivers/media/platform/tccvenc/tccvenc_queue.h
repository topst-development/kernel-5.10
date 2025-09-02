#ifndef _TCCVENC_QUEUE_H_
#define _TCCVENC_QUEUE_H_

#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>

#include <linux/dma-direct.h>
#include <linux/platform_device.h>

#include "tccvenc.h"
#include "tccvenc_debug.h"

int tccvenc_queue_init(void *priv, struct vb2_queue *src_vq,
                                    struct vb2_queue *dst_vq);

extern const struct vb2_ops tccvenc_vb2_ops;
extern const struct v4l2_m2m_ops tccvenc_m2m_ops;

#endif
